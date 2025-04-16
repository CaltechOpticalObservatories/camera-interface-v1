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
//#include <json.hpp>

#include "camera_interface.h"
#include "utilities.h"
#include "network.h"
#include "camerad_commands.h"

namespace Camera {

  const int N_THREADS=10;

  class Server {
    public:
      Server();
      ~Server();

      Interface* interface;

      NumberPool id_pool;
      std::map<int, std::shared_ptr<Network::TcpSocket>> socklist;
      std::mutex sock_block_mutex;
      std::atomic<int> threads_active;
      std::atomic<int> cmd_num;

      void block_main(std::shared_ptr<Network::TcpSocket> socket);
      void doit(Network::TcpSocket sock);
  };
}

