camerad watchdog system
========================

Three layers of supervision for the standalone camerad daemon:

  Layer 1  systemd Restart=always       -> catches any process exit
  Layer 2  camerad-watchdog ping probe  -> catches a frozen command loop
  Layer 3  systemd WatchdogSec=         -> catches a wedged watchdog

Target platform: Ubuntu 22.04.4 LTS (Jammy) -- systemd 249, polkit 0.105.

Failure-mode coverage
---------------------
  Process exits (crash/OOM/assert) .......... Layer 1 (Restart=always)
  Alive but command loop frozen ............. Layer 2 (ping/pong probe)
  Watchdog itself wedges .................... Layer 3 (WatchdogSec=)
  Commanded stop ("systemctl stop") ......... intentionally NOT restarted
  One thread hung, others alive ............. NOT covered (black-box liveness)

Build
-----
  cd build && cmake .. && make
    -> produces  bin/camerad  and  bin/camerad-watchdog

Install (scripted -- the easy path)
-----------------------------------
  After building, from this watchdog_system/ directory:

    sudo ./install-watchdog          # installs units + polkit + cameractl wrapper,
                                     # enables, starts, and wires the boot chain
    sudo ./uninstall-watchdog        # undo: stop, disable, and remove everything

  install-watchdog assumes the deploy tree at /home/user/Software/camera-interface;
  override with WATCHDOG_DIR=... BIN_DIR=... if yours differs. It first checks that
  the invoking user has sudo privileges (it installs into /etc), and refuses to run
  until both binaries are built. The manual equivalent of every step follows below
  for reference / debugging.

Install (manual; run as root; confirm the deployment values first)
------------------------------------------------------------------
  # 1. service account + operator group ("user" already exists on this host;
  #    just make sure operators are members of the "user" group)
  #    add human operators:  sudo usermod -aG user <operator>

  # 2. units
  sudo cp systemd/camerad.service systemd/camerad-watchdog.service \
          systemd/camerad.target  /etc/systemd/system/

  # 3. polkit grant (Jammy = polkit 0.105 -> .pkla)
  sudo cp polkit/49-camerad.pkla /etc/polkit-1/localauthority/50-local.d/

  # 4. cameractl operator wrapper
  sudo install -m 0755 bin/cameractl /usr/local/bin/cameractl

  # 5. enable + start
  #    NOTE: camerad.target MUST be enabled too, or nothing starts at boot.
  #    The services are WantedBy=camerad.target, which is WantedBy=multi-user.target,
  #    so enabling only the services links them into camerad.target.wants but leaves
  #    camerad.target itself out of the boot chain (services show "enabled" yet stay
  #    "inactive (dead)" after a reboot). Enable all three to complete the chain.
  sudo systemctl daemon-reload
  sudo systemctl enable --now camerad.target camerad camerad-watchdog

  # 6. confirm the boot chain is complete
  systemctl is-enabled camerad.target                                # must report: enabled
  ls -l /etc/systemd/system/multi-user.target.wants/camerad.target   # must exist

Verify
------
  L1:  sudo kill -9 $(pgrep -x camerad)         # back within ~2 s (RestartSec)
       systemctl status camerad                 # shows a fresh PID
  L2:  kill -STOP <camerad main pid>            # ~3 missed probes -> restart
       journalctl -u camerad-watchdog -f        # watch the decision
  L3:  kill -STOP <camerad-watchdog pid>        # systemd kills+restarts it
       journalctl -u camerad-watchdog -f
  probe: printf 'ping\n' | nc 127.0.0.1 <NBPORT>         # -> pong
  stop:  systemctl stop camerad                 # stays down, no restart fight
  boot:  sudo reboot ; then  systemctl status camerad.target camerad camerad-watchdog
         # all active (running) after reboot
         (if dead after reboot, camerad.target was not enabled -- see install step 5)

Starting and stopping
---------------------
cameractl is installed in PATH by install-watchdog and is just a systemctl wrapper

    cameractl restart              # restart the whole camera group
    cameractl {start|stop|status}

Underneath, cameractl is exactly the systemctl control surface; use it directly
when you want finer scope. Members of group "user" run either form without a
password (the polkit grant of install step 3); that same grant is what lets the
headless watchdog restart camerad.

    All units      systemctl {start|stop|restart} camerad.target
    Just camerad   systemctl {start|stop|restart} camerad
    Status         systemctl status camerad.target camerad camerad-watchdog
    Restart logs   journalctl -u camerad -f      (or -u camerad-watchdog)

A commanded stop is the intended off-switch: it stays stopped and is NOT auto-
restarted (that is also how you halt a daemon crash-looping on bad config).

Deployment values (already set in the unit files)
-------------------------------------------------
  * Service user/group: user:user.
  * Software root: /home/user/Software/camera-interface
    (binaries in .../bin/camerad and .../bin/camerad-watchdog).
  * Config file: /home/user/Software/camera-interface/Config/cryoscope.cfg
    The watchdog reads NBPORT and LOGPATH from this same file.
  * The "user" account must be able to read+exec both binaries and to read the
    config file and write the LOGPATH directory.

Tunables (compile-time, top of camerad_watchdog.cpp)
----------------------------------------------------
  PROBE_INTERVAL_SEC=5  PROBE_TIMEOUT_MS=2000  FAIL_THRESHOLD=3  COOLDOWN_SEC=30
  STARTUP_GRACE_SEC=30  (delay before the first probe, so a slow start isn't a hang)
  Keep WatchdogSec (camerad-watchdog.service) > FAIL_THRESHOLD*PROBE_INTERVAL_SEC.

sudoers alternative to polkit
------------------------------------------------------------
  Instead of the .pkla, install /etc/sudoers.d/camerad with:
     %user ALL=(root) NOPASSWD: /usr/bin/systemctl start camerad camerad-watchdog, \
                                /usr/bin/systemctl stop camerad camerad-watchdog, \
                                /usr/bin/systemctl restart camerad camerad-watchdog, \
                                /usr/bin/systemctl status camerad camerad-watchdog
  ...then change the execl() calls in camerad_watchdog.cpp (restart_unit /
  unit_is_active) to invoke "sudo systemctl".
