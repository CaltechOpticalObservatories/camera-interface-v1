/**
 * @file     camera_server.h
 * @brief    
 * @author   David Hale <dhale@astro.caltech.edu>
 *
 */

#pragma once

//#include <map>
//#include <memory>
//#include <atomic>
//#include <mutex>
#include <limits.h>
#include <signal.h>
#include <sys/types.h>
//#include <json.hpp>

#include "common.h"
#include "camera_interface.h"
#include "utilities.h"
#include "network.h"
#include "camerad_commands.h"
#include "message.h"

namespace Camera {

  class Server {
    private:
      std::atomic<int> cmd_num;
      int cmd_port;

    public:
      Server();
      ~Server();

      Message::Server message_server;

      std::unique_ptr<Interface> interface;

      void configure_server();
      void exit_cleanly();
      std::string dispatch(const std::string &message, Message::AckFunction send_ack);
  };
}

