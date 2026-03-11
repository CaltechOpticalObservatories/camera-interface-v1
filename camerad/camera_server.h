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

  const int N_THREADS=10;

  class Server {
    private:
      std::atomic<uint32_t> cmd_id{0};
    public:
      Server();
      ~Server();

      Message::Server message_server;

      std::unique_ptr<Interface> interface;

      int blkport;

      NumberPool id_pool;
      std::map<int, std::shared_ptr<Network::TcpSocket>> socklist;
      std::mutex sock_block_mutex;
      std::atomic<int> threads_active;
      std::atomic<int> cmd_num;

      void configure_server();
      void exit_cleanly();
      void block_main(std::shared_ptr<Network::TcpSocket> socket);
      void doit(Network::TcpSocket sock);

      std::string dispatch(const std::string &message, Message::AckFunction send_ack);
  };
}

