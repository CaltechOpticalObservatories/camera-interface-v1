
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>

#include <iostream>
#include <cstring>
#include <vector>

constexpr size_t BUFSIZE=8192;
constexpr std::string_view usage() {
  return "usage: sendcmd [-h hostname] [-p port] [-t timeout] [-m mode] command\n";
}

int main(int argc, char *argv[]) {
  int sock;
  int timeout=10;
  struct timeval tvstart, tvend;
  int nread;
  std::string message;
  std::vector<char> response(BUFSIZE);
  std::string hostname = "localhost";
  struct sockaddr_in address;
  struct hostent *host_struct;
  int mode = 0;     // 0: wait, 1: not wait
  int port = 3031;  // set default port

  if ( argc==1 ) { std::cout << usage(); return 0; }

  try {
  for ( size_t i=1; i<argc; ++i ) {
    if (argv[i][0] == '-') {
      switch ( argv[i][1] ) {
        case 'h' : if ( i+1 < argc ) hostname = argv[++i];
                   else { std::cout << usage(); return 1; }
                   break;
        case 'p' : port = std::stoi( argv[++i] );
                   break;
        case 't' : timeout = std::stoi( argv[++i] );
                   break;
        case 'm' : mode = std::stoi( argv[++i] );
                   break;
        default:   std::cout << usage();
                   return 0;
      }
    } else message = argv[i];
  }
  }
  catch (...) { std::cout << usage(); return -1; }

  if ( ( host_struct = gethostbyname( hostname.c_str() ) ) == nullptr ) {
    std::cerr << "ERROR getting_mem: " << std::strerror(errno) << "\n";
    return -errno;
  }

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    std::cerr << "ERROR creating socket: " << std::strerror(errno) << "\n";
    return -errno;
  }

  if (fcntl (sock, F_SETFL, O_NONBLOCK) <0){
    std::cerr << "ERROR setting socket: " << std::strerror(errno) << "\n";
    close (sock);
    return -errno;
  }

  std::memcpy( &address.sin_addr.s_addr, host_struct->h_addr, host_struct->h_length );
  address.sin_family = AF_INET;
  address.sin_port = htons(port);

  if (gettimeofday (&tvstart, nullptr) <0) {
    std::cerr << "ERROR reading time: " << std::strerror(errno) << "\n";
    close (sock);
    return -errno;
  }

  if ( (connect(sock, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) == -1) &&
      (errno != EINPROGRESS) ) {
    std::cerr << "ERROR connecting: " << std::strerror(errno) << "\n";
    close (sock);
    return -errno;
  }

  // Wait for non-blocking connect to complete
  struct pollfd pfd = { sock, POLLOUT, 0 };
  int pret = poll(&pfd, 1, timeout * 1000);
  if ( pret <= 0 ) {
    std::cerr << "ERROR connect timeout: " << std::strerror(errno) << "\n";
    close(sock);
    return -ETIME;
  }
  // Verify the connection actually succeeded
  int sockerr = 0;
  socklen_t errlen = sizeof(sockerr);
  getsockopt(sock, SOL_SOCKET, SO_ERROR, &sockerr, &errlen);
  if ( sockerr != 0 ) {
    std::cerr << "ERROR connecting: " << std::strerror(sockerr) << "\n";
    close(sock);
    return -sockerr;
  }

  message += "\n";

  while ( ( nread = write( sock, message.c_str(), message.size() ) ) < 0 ) {
    if (errno != EAGAIN) {
      std::cerr << "ERROR writing message: " << std::strerror(errno) << "\n";
      close (sock);
      return -errno;
    }
    gettimeofday (&tvend, nullptr);
    if (tvend.tv_sec - tvstart.tv_sec >= timeout) {
      std::cerr << "TIMEOUT: " << std::strerror(errno) << "\n";
      close (sock);
      return -ETIME;
    }
  }

  if (mode == 0) {
    while ( ( nread = read( sock, response.data(), BUFSIZE ) ) < 0 ) {
      if (errno != EAGAIN) {
        std::cerr << "(sendcmd) nread:" << nread << " error:" << std::strerror(errno) << "\n";
        close (sock);
        return -errno;
      }
      usleep( 10000 );
      gettimeofday (&tvend, nullptr);
      if (tvend.tv_sec - tvstart.tv_sec >= timeout) {
        std::cerr << "TIMEOUT: " << std::strerror(errno) << "\n";
        close (sock);
        return -ETIME;
      }
    }
    std::cout << std::string( response.data(), nread ) << "\n";
  }
  else std::cout << "cmd_sent\n";

  return close(sock);
}

