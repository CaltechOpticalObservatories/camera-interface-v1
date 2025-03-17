#pragma once

#include <map>
#include <memory>
#include <atomic>
#include <mutex>
#include <limits.h>
#include <json.hpp>

#include "astrocam_interface.h"
#include "utilities.h"
#include "network.h"
#include "camerad_commands.h"

namespace Camera {

  class Server {
    private:
      AstroCam::Interface interface;

    public:
      Server() :
       id_pool(10),
       threads_active(0) {
       }

      ~Server() = default;

      AstroCam::Interface &get_interface();
      std::map<int, AstroCam::Controller> &get_controllers();

      NumberPool id_pool;
      std::map<int, std::shared_ptr<Network::TcpSocket>> socklist;
      std::mutex sock_block_mutex;
      std::atomic<int> threads_active;
      std::atomic<int> cmd_num;

      void block_main(std::shared_ptr<Network::TcpSocket> socket);

      void doit(Network::TcpSocket sock);
  };
}

