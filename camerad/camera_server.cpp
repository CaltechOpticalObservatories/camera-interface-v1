
#include <iostream>

#include "camera_server.h"
#include "network.h"

namespace Camera {

  AstroCam::Interface &Server::get_interface() { return interface; }

  void Server::block_main( std::shared_ptr<Network::TcpSocket> sock ) {
    this->threads_active.fetch_add(1);  // atomically increment threads_busy counter
    this->doit(*sock);
    sock->Close();
    this->threads_active.fetch_sub(1);  // atomically increment threads_busy counter
    this->id_pool.release_number( sock->id );
    return;
  }

  void Server::doit( Network::TcpSocket sock ) {
    std::string cmd, args;
    bool connection_open=true;
    long ret;

    while (connection_open) {
      int pollret;
      if ( ( pollret=sock.Poll() ) <= 0 ) {
        std::cerr << "Poll error\n";
        break;
      }

      std::string sbuf;
      char delim='\n';
      if ( ( ret=sock.Read( sbuf, delim ) ) <= 0 ) {
        if (ret<0) {
          std::cerr << "Read error\n";
          break;
        }
      }

      sbuf.erase(std::remove(sbuf.begin(), sbuf.end(), '\r' ), sbuf.end());
      sbuf.erase(std::remove(sbuf.begin(), sbuf.end(), '\n' ), sbuf.end());

      if ( sbuf.empty() ) sbuf="help";                 // no command automatically displays help

      std::cerr << "received: " << sbuf << "\n";

      try {
        std::size_t cmd_sep = sbuf.find_first_of(" "); // find the first space, which separates command from argument list

        cmd = sbuf.substr(0, cmd_sep);                 // cmd is everything up until that space

        if (cmd.empty()) {sock.Write("\n"); continue;} // acknowledge empty command so client doesn't time out

        if (cmd_sep == std::string::npos) {            // If no space was found,
          args="";                                     // then the arg list is empty,
        }
        else {
          args= sbuf.substr(cmd_sep+1);                // otherwise args is everything after that space.
        }

        if ( ++this->cmd_num == INT_MAX ) this->cmd_num = 0;

        std::cerr << "thread " << sock.id << " received command on fd " << sock.getfd() << " (" << this->cmd_num << ") : " << cmd << " " << args << "\n";
      }
      catch(...) { }

      std::string retstring;

      if ( cmd == "connect" ) {
        this->get_interface().connect_controller( args, retstring );
      }

      sock.Write(retstring+" "+std::string("DONE\n"));
    }
    return;
  }

}
