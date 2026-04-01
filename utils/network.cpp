/**
 * @file    network.cpp
 * @brief   network functions, specifically TCP/IP sockets
 * @details TcpSocket class for client and server communications
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 * This contains the class member functions for a TcpSocket class,
 * suitable for both client and server network communications.
 *
 * For a client:
 * set this->host, this->port and call Connect(). 
 *
 * For a server:
 * at minimum, set this->port, this->blocking or instantiate using
 * (port, blocking, totime, id). Call Listen() then Accept(). 
 *
 * For both:
 * Call Read() and Write() to read/write from/to the socket. 
 * Use Poll() to wait for data to be ready to read.
 *
 */

#include "network.h"
#include "logentry.h"  // for logwrite() within the Network namespace

namespace Network {


  /**************** Network::UdpSocket::UdpSocket *****************************/
  /**
   * @fn         UdpSocket
   * @brief      UdpSocket class constructor
   * @param[in]  port_in, port on which server will multicast datagrams
   * @param[in]  group_in, 
   * @return     none
   *
   * Use this to construct a UDP multi-cast datagram server object
   *
   */
  UdpSocket::UdpSocket(int port_in, std::string group_in) {
    this->fd = -1;
    this->port = port_in;
    this->group = group_in;
    this->service_running = false;
  }
  /**************** Network::UdpSocket::UdpSocket *****************************/


  /**************** Network::UdpSocket::UdpSocket *****************************/
  /**
   * @fn         UdpSocket
   * @brief      default UdpSocket class constructor
   * @param[in]  none
   * @return     none
   *
   */
  UdpSocket::UdpSocket() {
    this->fd = -1;
    this->port = -1;
    this->group = "";
    this->service_running = false;
  }
  /**************** Network::UdpSocket::UdpSocket *****************************/


  /**************** Network::UdpSocket::UdpSocket *****************************/
  /**
   * @fn         UdpSocket
   * @brief      UdpSocket class copy constructor
   * @param[in]  obj reference to class object
   * @return     none
   *
   */
  UdpSocket::UdpSocket(const UdpSocket &obj) {
    fd = obj.fd;
    port = obj.port;
    group = obj.group;
    service_running = obj.service_running;
  }
  /**************** Network::UdpSocket::UdpSocket *****************************/


  /**************** Network::UdpSocket::~UdpSocket ****************************/
  /**
   * @fn         ~UdpSocket
   * @brief      UdpSocket class deconstructor
   * @param[in]  none
   * @return     none
   *
   */
  UdpSocket::~UdpSocket() {
    this->Close();
  };
  /**************** Network::UdpSocket::~UdpSocket ****************************/


  /**************** Network::UdpSocket::Create ********************************/
  /**
   * @fn         Create
   * @brief      create a UDP multi-cast socket
   * @param[in]  none
   * @return     0 on success, -1 on error, 1 to indicate user-requested disable
   *
   * If the .cfg file contains ASYNCGROUP=none (any case) then do not create
   * the socket. Return a 1 so the caller knows that no socket was created
   * by user request, as opposed to an error.
   *
   */
  int UdpSocket::Create() {
    std::string function = "Network::UdpSocket::Create";
    std::stringstream errstm;

    // don't create more than one UDP multicast socket
    //
    if (this->service_running) {
      logwrite(function, "ERROR: service already running");
      return -1;
    }

    // don't do anything if the ASYNCGROUP is not initialized
    //
    if (this->group.empty()) {
      logwrite(function, "ERROR: ASYNCGROUP not initialized. Cannot create socket");
      return -1;
    }

    // the user can set ASYNCGROUP=none to disable the async status message port
    //
    try {
      std::transform( this->group.begin(), this->group.end(), this->group.begin(), ::toupper );    // make uppercase
    }
    catch (...) {
      logwrite(function, "error converting ASYNCGROUP to uppercase");
      return -1;
    }
    if (this->group == "NONE") {
      logwrite(function, "ASYNCGROUP=none. UDP multicast socket disabled.");
      return 1;
    }

    // now that there is a group, check that the port is initialized
    //
    if (this->port < 0) {
      logwrite(function, "ERROR: ASYNCPORT not initialized. Cannot create socket");
      return -1;
    }

    // now that there is a group and port, create the socket
    //
    if ( (this->fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1 ) {
      errstm << "error " << errno << " creating socket: " << strerror(errno);
      logwrite(function, errstm.str());
      return(-1);
    }

    // must enable loopback to allow more than one listener
    //
    int loopback = 1;
    setsockopt(this->fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loopback, sizeof(loopback));

    // set number of hops to 2
    //
    int timetolive = 2;
    setsockopt(this->fd, IPPROTO_IP, IP_MULTICAST_LOOP, &timetolive, sizeof(timetolive));

    // set up the source address
    //
    memset((struct sockaddr *) &this->addr, 0, (socklen_t)sizeof(this->addr));
    this->addr.sin_family = AF_INET;
    this->addr.sin_addr.s_addr = inet_addr(this->group.c_str());
    this->addr.sin_port = htons(this->port);
    setsockopt (this->fd, IPPROTO_IP, IP_MULTICAST_IF, &this->addr, sizeof(this->addr));

    this->service_running = (this->fd >= 0 ? true : false);

    return 0;
  }
  /**************** Network::UdpSocket::Create ********************************/


  /**************** Network::UdpSocket::Send **********************************/
  /**
   * @fn         Send
   * @brief      transmit the message to the UDP socket
   * @param[in]  none
   * @return     0 on success, -1 on error
   *
   */
  int UdpSocket::Send(std::string message) {
    std::string function = "Network::UdpSocket::Send";
    std::stringstream errstm;
    int nbytes;

    if ( !this->is_running() ) return 0;  // silently do nothing if the UDP multicast socket isn't running

    if ( ( nbytes = sendto( this->fd, message.c_str(), (size_t)message.length(), 0, 
                            (struct sockaddr*) &this->addr, (socklen_t)sizeof(this->addr) ) ) < 0 ) {
      errstm << "error " << errno << " calling sendto: " << strerror(errno);
      logwrite(function, errstm.str());
      return -1;
    }

    return 0;
  }
  /**************** Network::UdpSocket::Send **********************************/


  /**************** Network::UdpSocket::Listener ******************************/
  /**
   * @fn         Listener
   * @brief      creates a UDP listener, returns a file descriptor
   * @param[in]  none
   * @return     fd on success, -1 on error
   *
   */
  int UdpSocket::Listener( ) {
    std::string function = "Network::UdpSocket::Listener";
    std::stringstream message;

    // don't create more than one UDP multicast socket
    //
    if ( this->service_running ) {
      logwrite( function, "ERROR: service already running" );
      return -1;
    }

    // don't do anything if the ASYNCGROUP is not initialized
    //
    if ( this->group.empty() ) {
      logwrite( function, "ERROR: ASYNCGROUP not initialized. Cannot create socket" );
      return -1;
    }

    // the user can set ASYNCGROUP=none to disable the async status message port
    //
    try {
      std::transform( this->group.begin(), this->group.end(), this->group.begin(), ::toupper );    // make uppercase
    }
    catch (...) {
      logwrite( function, "error converting ASYNCGROUP to uppercase" );
      return -1;
    }
    if ( this->group == "NONE" ) {
      logwrite( function, "ASYNCGROUP=none. UDP multicast socket disabled." );
      return 1;
    }

    // now that there is a group, check that the port is initialized
    //
    if ( this->port < 0 ) {
      logwrite( function, "ERROR: ASYNCPORT not initialized. Cannot create socket" );
      return -1;
    }

    // now that there is a group and port, create the socket
    //
    if ( ( this->fd = socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 ) {
      message << "error " << errno << " creating socket: " << strerror( errno );
      logwrite(function, message.str());
      return(-1);
    }

    // allow multiple sockets to use the same PORT number
    //
    u_int yes = 1;
    if ( setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, (char*) &yes, sizeof(yes) ) < 0 ) {
      message << "ERROR: reusing ADDR failed: " << strerror( errno );
      logwrite( function, message.str() );
      return -1;
    }

    // set up the source address
    //
    memset( &this->addr, 0, sizeof( this->addr ) );
    this->addr.sin_family = AF_INET;
    this->addr.sin_addr.s_addr = htonl( INADDR_ANY ); // differs from sender
    this->addr.sin_port = htons( this->port );

    // bind to receive address
    //
    if ( bind( this->fd, (struct sockaddr*) &this->addr, sizeof(this->addr) ) < 0 ) {
      message << "ERROR binding to receive address: " << strerror( errno );
      logwrite( function, message.str() );
      return -1;
    }

    // use setsockopt() to request that the kernel join a multicast group
    //
    this->mreq.imr_multiaddr.s_addr = inet_addr( this->group.c_str() );
    this->mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if ( setsockopt( fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*) &this->mreq, sizeof(this->mreq) ) < 0 ) {
      message << "ERROR joining multicast group: " << strerror( errno );
      logwrite( function, message.str() );
      return -1;
    }

#ifdef LOGLEVEL_DEBUG
    message << "created UDP listening socket on fd " << this->fd;
    logwrite( function, message.str() );
#endif

    return( this->fd );
  }
  /**************** Network::UdpSocket::Listener ******************************/


  /**************** Network::UdpSocket::Receive *******************************/
  /**
   * @fn         Receive
   * @brief      receive a UDP message from the Listener fd
   * @param[out] reference to string, to contain the message read
   * @return     number of bytes received
   *
   */
  int UdpSocket::Receive( std::string &message ) {
    char msgbuf[ UDPMSGLEN ];
    socklen_t addrlen = sizeof( this->addr );
    int nbytes = recvfrom ( this->fd,
                            msgbuf,
                            UDPMSGLEN,
                            0,
                            (struct sockaddr *) &this->addr,
                            &addrlen
                          );
    msgbuf[ nbytes ] = '\0';

    std::string msg( msgbuf, nbytes );
    message = msg;

    return nbytes;
  }
  /**************** Network::UdpSocket::Receive *******************************/


  /**************** Network::UdpSocket::Close *********************************/
  /**
   * @fn         Close
   * @brief      close the UDP socket connection
   * @param[in]  none
   * @return     0 on success, -1 on error
   *
   */
  int UdpSocket::Close() {
    int error = -1;
    if (this->fd >= 0) {               // if the file descriptor is valid
      if (close(this->fd) == 0) {      // then close it
        error = 0;
        this->fd = -1;
      }
      else {
        error = -1;                    // error closing file descriptor
     }
    }
    else {
      error = 0;                       // didn't start with a valid file descriptor
    }

    this->service_running = false;     // clear the service_running flag
    return (error);
  }
  /**************** Network::UdpSocket::Close *********************************/


  /**************** Network::TcpSocket::TcpSocket *****************************/
  /**
   * @fn         TcpSocket
   * @brief      TcpSocket class constructor
   * @param[in]  port_in, port which server will listen on
   * @param[in]  block_in, true|false -- will the connection be blocking?
   * @param[in]  totime_in, timeout time for poll, in msec
   * @param[in]  id_in, ID number (for keeping track of threads)
   * @return     none
   *
   * Use this to construct a server's listening socket object
   *
   */
  TcpSocket::TcpSocket(int port_in, bool block_in, int totime_in, int id_in) {
    this->port = port_in;
    this->blocking = block_in;
    this->totime = totime_in;
    this->id = id_in;
    this->fd = -1;
    this->listenfd = -1;
    this->host = "";
    this->addrs = NULL;
    this->connection_open = false;
  };
  /**************** Network::TcpSocket::TcpSocket *****************************/


  /**************** Network::TcpSocket::TcpSocket *****************************/
  /**
   * @fn         TcpSocket
   * @brief      TcpSocket class constructor
   * @param[in]  host, hostname to which client will connect
   * @param[in]  port_in, port to which client will connect
   * @return     none
   *
   * Use this to construct a client object
   *
   */
  TcpSocket::TcpSocket( std::string host, int port ) {
    this->host = host;
    this->port = port;
    this->totime = POLLTIMEOUT;    /// default Poll timeout in msec
    this->fd = -1;
    this->addrs = NULL;
    this->connection_open = false;
  }
  /**************** Network::TcpSocket::TcpSocket *****************************/


  /**************** Network::TcpSocket::TcpSocket *****************************/
  /**
   * @fn         TcpSocket
   * @brief      default TcpSocket class constructor
   * @param[in]  none
   * @return     none
   *
   */
  TcpSocket::TcpSocket() {
    this->port = -1;
    this->blocking = false;
    this->totime = POLLTIMEOUT;    /// default Poll timeout in msec
    this->id = -1;
    this->fd = -1;
    this->listenfd = -1;
    this->host = "";
    this->addrs = NULL;
    this->connection_open = false;
  };
  /**************** Network::TcpSocket::TcpSocket *****************************/


  /**************** Network::TcpSocket::TcpSocket *****************************/
  /**
   * @fn         TcpSocket
   * @brief      TcpSocket class copy constructor
   * @param[in]  obj reference to class object
   * @return     none
   *
   */
  TcpSocket::TcpSocket(const TcpSocket &obj) {
    port = obj.port;
    blocking = obj.blocking;
    totime = obj.totime;
    id = obj.id;
    fd = obj.fd;
    listenfd = obj.listenfd;
    host = obj.host;
    addrs = obj.addrs;
    connection_open = obj.connection_open;
  };
  /**************** Network::TcpSocket::TcpSocket *****************************/


  /**************** Network::TcpSocket::~TcpSocket ****************************/
  /**
   * @fn         ~TcpSocket
   * @brief      TcpSocket class deconstructor
   * @param[in]  none
   * @return     none
   *
   */
  TcpSocket::~TcpSocket() {
    this->Close();
  };
  /**************** Network::TcpSocket::~TcpSocket ****************************/


  /**************** Network::TcpSocket::Accept ********************************/
  /**
   * @fn         Accept
   * @brief      creates a new connected socket for pending connection
   * @param[in]  none
   * @return     fd (new connected socket file descriptor) or -1 on error
   *
   * Create new listening socket for pending connection on this->listenfd
   * and returns a new connected socket this->fd
   *
   */
  int TcpSocket::Accept() {
    this->clilen = (socklen_t)sizeof(this->cliaddr);
    this->fd = accept(this->listenfd, (struct sockaddr *) &this->cliaddr, &this->clilen);
    if (this->fd < 0) { perror("(Network::TcpSocket::Accept) error calling accept"); return -1; }
    return (this->fd);
  }
  /**************** Network::TcpSocket::Accept ********************************/


  /**************** Network::TcpSocket::Listen ********************************/
  /**
   * @fn         Listen
   * @brief      create a TCP listening socket
   * @param[in]  none
   * @return     listenfd (listening file descriptor), or -1 on error
   *
   * This creates a TCP listening socket that will be used to accept
   * incomming connections.
   *
   */
  int  TcpSocket::Listen() {
    std::string function = "Network::TcpSocket::Listen";
    std::stringstream errstm;

    // create the socket
    //
    if ( (this->listenfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1 ) {
      errstm << "error " << errno << " creating socket: " << strerror(errno);
      logwrite(function, errstm.str());
      return(-1);
    }

    // allow re-binding to port while previous connection is in TIME_WAIT
    //
    int on=1;
    setsockopt(this->listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    // allow kernel to linger on close --
    // if there is any data remaining in the socket send buffer then then process
    // sleeps until data is sent and acknowledged by the peer or until POLLTIMEOUT.
    //
    struct linger so_linger;
    so_linger.l_onoff = 1;
    so_linger.l_linger = POLLTIMEOUT;
    setsockopt( this->listenfd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger) );

    // get address information and bind it to the socket
    //
    struct sockaddr_in servaddr = {0};
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(this->port);

    if ( bind(this->listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0 ) {
      errstm << "error " << errno << " binding to fd " << this->listenfd << " on port " << this->port << ": " << strerror(errno);
      logwrite(function, errstm.str());
      return(-1);
    }

    // start listening
    //
    if (listen(this->listenfd, LISTENQ)!=0) {
      errstm << "error " << errno << " listening to fd " << this->listenfd << " on port " << this->port << ": " << strerror(errno);
      logwrite(function, errstm.str());
      return(-1);
    }

    return(this->listenfd);
  }
  /**************** Network::TcpSocket::Listen ********************************/


  /**************** Network::TcpSocket::Poll **********************************/
  /**
   * @fn         Poll
   * @brief      polls a single file descriptor to wait for incoming data to read
   * @param[in]  none
   * @return     0 on timeout, -1 on error, a non-negative value on success
   *
   * This function is overloaded. One version accepts a timeout to be passed in,
   * and the other version takes no argument and uses the default timeout that
   * was set for the class object.
   *
   */
  int TcpSocket::Poll() {                /// uses default timeout
    return( this->Poll( this->totime ) );
  }
  /**************** Network::TcpSocket::Poll **********************************/


  /**************** Network::TcpSocket::Poll **********************************/
  /**
   * @fn         Poll
   * @brief      polls a single file descriptor to wait for incoming data to read
   * @param[in]  timeout
   * @return     0 on timeout, -1 on error, a non-negative value on success
   *
   * This function is overloaded. One version accepts a timeout to be passed in,
   * and the other version takes no argument and uses the default timeout that
   * was set for the class object.
   *
   */
  int TcpSocket::Poll( int timeout ) {   // uses timeout arg
    std::string function = "Network::TcpSocket::Poll";
    std::stringstream message;
    struct pollfd poll_struct;
    poll_struct.events = POLLIN;
    poll_struct.fd     = this->fd;

    int ret = poll( &poll_struct, 1, timeout );
    short revents = poll_struct.revents;

    if ( ( revents & POLLHUP ) || ( revents & POLLERR) || ( revents & POLLNVAL ) ) {
      message.str(""); message << ( revents & POLLHUP  ? "POLLHUP "  : "" )
                               << ( revents & POLLERR  ? "POLLERR "  : "" )
                               << ( revents & POLLNVAL ? "POLLNVAL " : "" )
                               << "on socket " << this->host << "/" << this->port << " fd " << this->fd;
      logwrite( function, message.str() );
      return -1;
    }

    return( ret );
  }
  /**************** Network::TcpSocket::Poll **********************************/


  /***** Network::TcpSocket::Connect ******************************************/
  /**
   * @brief      connect to this->host on this->port
   * @return     0 on success, -1 on error
   * @throws     std::runtime_error
   *
   * this->host and this->port need to be specified prior to calling Connect()
   * On success, this function will set this->fd with the open socket file descriptor.
   *
   * getaddrinfo() will allocate memory which must be freed by TcpSocket::Close()
   *
   * connect will timeout after CONNECT_TIMEOUT_SEC seconds (defined in network.h)
   *
   */
  int TcpSocket::Connect() {
    std::string function = "Network::TcpSocket::Connect";
    std::stringstream errstm;
    int flags=-1;

    struct addrinfo hints = {0};       /// destination for getaddrinfo

    hints.ai_family = AF_INET;         /// IPv4 address only
    hints.ai_socktype = SOCK_STREAM ;  /// non-blocking, connection-based socket
    hints.ai_protocol = IPPROTO_TCP;   /// TCP socket
    hints.ai_flags = AI_NUMERICSERV;   /// numeric port number

    // Return one (or more) socket address structures which identify the requested host and port
    // Call TcpSocket::Close() to free memory allocated by this call.
    //
    int status = getaddrinfo(this->host.c_str(), std::to_string(this->port).c_str(), &hints, &this->addrs);

    if ( status != 0 ) {
      errstm << "connecting to host " << this->host << " port " << this->port << " : " << gai_strerror(status);
      throw std::runtime_error(errstm.str());
    }

    // Loop though the results returned by getaddrinfo and attempt to create a socket and connect to it.
    // There should only be one when a host:port is specified but this form is more general.
    //
    for (struct addrinfo *sa = this->addrs; sa != NULL; sa = sa->ai_next) {
      // create the socket, which returns a file descriptor on success
      //
      this->fd = socket(sa->ai_family, sa->ai_socktype, sa->ai_protocol);

      if (this->fd == -1) continue;    // try another entry in the sock addr struct

      // before setting to non-block get the current flags
      //
      if ((flags = fcntl(this->fd, F_GETFL, 0)) < 0) {
        errstm << "getting socket file descriptor flags: " << std::strerror(errno);
        throw std::runtime_error(errstm.str());
      }

      // set socket to non-blocking so that it can timeout on failure to connect
      //
      if ( fcntl( this->fd, F_SETFL, flags | O_NONBLOCK ) == -1 ) {
        errstm << "setting non-block flag on fd " << this->fd << ": " << std::strerror(errno);
        throw std::runtime_error(errstm.str());
      }

      // connect to the socket file descriptor
      //
      int retval = connect( this->fd, sa->ai_addr, sa->ai_addrlen );
      if ( retval == 0 ) {
        break;                                  // connected immediately
      }
      else if ( retval == -1 && errno == EINPROGRESS ) {
        // didn't connect immediately so call select to wait for it,
        // which will provide the timeout mechanism
        //
        struct timeval timeout;
        timeout.tv_sec  = CONNECT_TIMEOUT_SEC;  // timeout in seconds
        timeout.tv_usec = 0;
        fd_set wfds;                            // set of write file descriptors for select
        FD_ZERO( &wfds );                       // zero the set
        FD_SET( this->fd, &wfds );              // add this fd to the set

        // on success, select will return the number of fds ready
        //
        retval = select( this->fd+1, NULL, &wfds, NULL, &timeout );
        if ( retval == 0 ) {                    // none ready is timeout
          errstm << "timeout (" << errno << ") connecting to " << this->host << "/" << this->port
                 << ": " << std::strerror(errno);
          throw std::runtime_error(errstm.str());
        }
        if ( retval == -1 ) {                   // error calling select
          errstm << "errno " << errno << " connecting to " << this->host << "/" << this->port
                 << " on fd " << this->fd << ": " << std::strerror(errno);
          throw std::runtime_error(errstm.str());
        }

        // select got an fd but still must check if socket is usable
        //
        int err;
        socklen_t len = sizeof(err);
        retval = getsockopt( this->fd, SOL_SOCKET, SO_ERROR, &err, &len );
        if ( retval != 0 ) {
          errstm << "errno " << errno << " getting socket error code for fd " << this->fd << ": " << std::strerror(errno);
          throw std::runtime_error(errstm.str());
        }
        if ( err != 0 ) {
          errstm << "errno " << errno << " connecting to " << this->host << "/" << this->port
                 << " on fd " << this->fd << ": " << std::strerror(errno);
          throw std::runtime_error(errstm.str());
        }

        break;  // by now it's a success
      }
      else {
        errstm << "errno " << errno << " connecting to " << this->host << "/" << this->port 
                           << " on fd " << this->fd << ": " << std::strerror(errno);
        throw std::runtime_error(errstm.str());
      }
    }

    // restore flags
    //
    if ( flags >= 0 && (fcntl(this->fd, F_SETFL, flags) < 0) ) {
      errstm << "errno " << errno << " setting socket file descriptor flags: " << std::strerror(errno);
      throw std::runtime_error(errstm.str());
    }

    this->connection_open = (this->fd >= 0 ? true : false);

    if (this->fd < 0) throw std::runtime_error("no socket file descriptors");

    return (this->fd >= 0 ? 0 : -1);
  }
  /***** Network::TcpSocket::Connect ******************************************/


  /**************** Network::TcpSocket::Close *********************************/
  /**
   * @fn         Close
   * @brief      close a socket connection and free memory allocated by getaddrinfo()
   * @param[in]  none
   * @return     0 on success, -1 on error
   *
   */
  int TcpSocket::Close() {
    int error = -1;
#ifdef LOGLEVEL_DEBUG
    int oldfd = this->fd;
#endif

    if (this->fd >= 0) {               // if the file descriptor is valid
      if (close(this->fd) == 0) {      // then close it
        error = 0;
        this->fd = -1;
      }
      else {
        error = -1;                    // error closing file descriptor
     }
    }
    else {
      error = 0;                       // didn't start with a valid file descriptor
    }

    if (this->addrs != NULL) {         // free memory allocated by getaddrinfo()
      freeaddrinfo(this->addrs);
      this->addrs = NULL;
    }

    this->connection_open = false;     // clear the connection_open flag

#ifdef LOGLEVEL_DEBUG
    std::stringstream message;
    message << "[DEBUG] closed socket " << this->host << "/" << this->port << " connection to fd " << oldfd;
    if ( oldfd >= 0 ) logwrite( "Network::TcpSocket::Close", message.str() );
#endif

    return (error);
  }
  /**************** Network::TcpSocket::Close *********************************/


  /**************** Network::TcpSocket::Write *********************************/
  /**
   * @fn         Write
   * @brief      write data to a socket
   * @param[in]  msg_in
   * @return     number of bytes written
   *
   */
  int TcpSocket::Write(std::string msg_in) {
    size_t bytes_sent;       // total bytes sent
    int    this_write;       // bytes sent with this write()
    const char* buf = msg_in.c_str();
    bytes_sent = 0;

    while (bytes_sent < msg_in.length()) {
      do
        this_write = write(this->fd, buf, msg_in.length() - bytes_sent);
      while ((this_write < 0) && (errno == EINTR));
      if (this_write <= 0) return this_write;
      bytes_sent += this_write;
      buf += this_write;
    }
    return bytes_sent;
  }
  /**************** Network::TcpSocket::Write *********************************/


  /**************** Network::TcpSocket::Read **********************************/
  /**
   * @fn         Read
   * @brief      read data from connected socket
   * @param[in]  buf, pointer to buffer
   * @param[in]  count, number of bytes to read
   * @return     number of bytes read or -1 on error
   *
   * If data not immediately available then wait for up to POLLTIMEOUT
   *
   * This function is overloaded; this version accepts a pointer to a
   * buffer and the number of bytes to read.
   *
   */
  int TcpSocket::Read(void* buf, size_t count) {
    std::string function = "Network::TcpSocket::Read[cbuf]";
    std::stringstream message;
    int nread;

    // get the time now for timeout purposes
    //
    std::chrono::steady_clock::time_point tstart = std::chrono::steady_clock::now();

    while ( ( nread = read( this->fd, buf, count ) ) < 0 ) {
      if ( errno != EAGAIN ) {
        message << "ERROR reading data on fd " << this->fd << ": " << strerror(errno);
        logwrite( function, message.str() );
        break;
      }

      // get time now and check for timeout
      //
      std::chrono::steady_clock::time_point tnow = std::chrono::steady_clock::now();

      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tnow - tstart).count();

      if ( elapsed > POLLTIMEOUT ) {
        message << "ERROR: timeout waiting for data on fd " << this->fd;
        logwrite( function, message.str() );
        break;
      }
    }
    return( nread );
  }
  /**************** Network::TcpSocket::Read **********************************/


  /**************** Network::TcpSocket::Read **********************************/
  /**
   * @fn         Read
   * @brief      read data from connected socket until delimeter char
   * @param[out] std::string retstring
   * @param[in]  char delimiter
   * @return     number of bytes read or -1 on error
   *
   * If data not immediately available then wait for up to POLLTIMEOUT
   *
   * This function is overloaded; this version accepts a reference to a string
   * and a delimiter char to read until.
   *
   */
  int TcpSocket::Read(std::string &retstring, char delim) {
    std::string function = "Network::TcpSocket::Read[delim]";
    std::stringstream message;
    std::stringstream bufstream;
    int nread, bytesread=0;
    char buf[2];
    memset(buf,'\0',2);

    // get the time now for timeout purposes
    //
    std::chrono::steady_clock::time_point tstart = std::chrono::steady_clock::now();

    while ( 1 ) {
      nread = read( this->fd, buf, 1 );  // read a byte at a time
      if ( nread<0 ) {
        message << "ERROR reading data on fd " << this->fd << ": " << strerror(errno);
        logwrite( function, message.str() );
        break;
      }
      if ( nread == 0 ) {
        message << "no data on socket " << this->host << "/" << this->port << " fd " << this->fd << ": closing connection";
        logwrite( function, message.str() );
        this->Close();
        break;
      }
      bytesread++;                       // keep count of total bytes read
      bufstream << buf;                  // build up return string from each byte read

      // get time now and check for timeout
      //
      std::chrono::steady_clock::time_point tnow = std::chrono::steady_clock::now();

      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tnow - tstart).count();

      if ( elapsed > POLLTIMEOUT ) {
        message << "ERROR: timeout waiting for data on fd " << this->fd;
        logwrite( function, message.str() );
        break;
      }

      if ( strchr(buf, delim) ) break;   // break when the delim character is found
    }

    retstring = bufstream.str();         // assign the assembled stream to the return string

    if ( nread <= 0 ) return( nread );   // return error
    else return( bytesread );            // or bytes read
  }
  /**************** Network::TcpSocket::Read **********************************/


  /**************** Network::TcpSocket::Read **********************************/
  /**
   * @fn         Read
   * @brief      read data from connected socket until an end string
   * @param[out] std::string retstring
   * @param[in]  std::string endstr
   * @return     number of bytes read or -1 on error
   *
   * If data not immediately available then wait for up to POLLTIMEOUT
   *
   * This function is overloaded; this version accepts a reference to a string
   * and an end string to read until.
   *
   */
  int TcpSocket::Read(std::string &retstring, std::string endstr) {
    std::string function = "Network::TcpSocket::Read[endstr]";
    std::stringstream message;
    std::stringstream bufstream;
    int nread, bytesread=0;
    const int bufsz=8192;                // read buffer in chunks
    char buf[bufsz+1];
    memset(buf,'\0',bufsz+1);

    // get the time now for timeout purposes
    //
    std::chrono::steady_clock::time_point tstart = std::chrono::steady_clock::now();

    while ( 1 ) {
      nread = read( this->fd, buf, bufsz );
      if ( nread<0 ) {
        message << "ERROR reading socket " << this->host << "/" << this->port << " on fd " << this->fd << ": " << strerror(errno);
        logwrite( function, message.str() );
        break;
      }
      if ( nread == 0 ) {
        message << "ERROR no data from socket " << this->host << "/" << this->port << " on fd " << this->fd << ": closing connection";
        logwrite( function, message.str() );
        this->Close();
        break;
      }
      bytesread += nread;                // keep count of total bytes read
      bufstream << buf;                  // build up return string from each byte read

      // get time now and check for timeout
      //
      std::chrono::steady_clock::time_point tnow = std::chrono::steady_clock::now();

      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tnow - tstart).count();

      if ( elapsed > POLLTIMEOUT ) {
        message << "ERROR: timeout waiting for data on fd " << this->fd;
        logwrite( function, message.str() );
        break;
      }

      // Break when the endstr is found in the buffer read
      //
      if ( bufstream.str().find( endstr ) != std::string::npos ) break;
    }

    retstring = bufstream.str();         // assign the assembled stream to the return string

    if ( nread <= 0 ) return( nread );   // return error
    else return( bytesread );            // or bytes read
  }
  /**************** Network::TcpSocket::Read **********************************/


  /**************** Network::TcpSocket::Bytes_ready ***************************/
  /**
   * @fn         Bytes_ready
   * @brief      get the number of bytes available on the socket file descriptor this->fd
   * @param[in]  none
   * @return     number of bytes read
   *
   */
  int TcpSocket::Bytes_ready() {
    int bytesready=-1;
    if ( ioctl(this->fd, FIONREAD, &bytesready) < 0 ) {
      perror("(Network::TcpSocket::Bytes_ready) ioctl error");
    }
    return(bytesready);
  }
  /**************** Network::TcpSocket::Bytes_ready ***************************/


  /**************** Network::TcpSocket::is_readable ***************************/
  /**
   * @brief      check if socket has data available to read
   * @param[in]  timeout_ms  poll timeout in milliseconds (0 = non-blocking check)
   * @return     true if data is available, false otherwise
   *
   */
  bool TcpSocket::is_readable(int timeout_ms) {
    struct pollfd pfd{};
    pfd.fd = this->fd;
    pfd.events = POLLIN;
    return (poll(&pfd, 1, timeout_ms) > 0) && (pfd.revents & POLLIN);
  }
  /**************** Network::TcpSocket::is_readable ***************************/


  /**************** Network::TcpSocket::set_tcp_nodelay ***********************/
  /**
   * @brief      enable or disable TCP_NODELAY to control Nagle's algorithm
   * @param[in]  enable  true to disable Nagle (low latency), false to enable it
   * @return     0 on success, -1 on error
   *
   */
  int TcpSocket::set_tcp_nodelay(bool enable) {
    int flag = enable ? 1 : 0;
    return setsockopt(this->fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
  }
  /**************** Network::TcpSocket::set_tcp_nodelay ***********************/


  /**************** Network::TcpSocket::set_recv_buf_size *********************/
  /**
   * @brief      set the socket receive buffer size
   * @param[in]  size  desired buffer size in bytes
   * @return     0 on success, -1 on error
   *
   */
  int TcpSocket::set_recv_buf_size(int size) {
    return setsockopt(this->fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
  }
  /**************** Network::TcpSocket::set_recv_buf_size *********************/


  /**************** Network::TcpSocket::set_send_buf_size *********************/
  /**
   * @brief      set the socket send buffer size
   * @param[in]  size  desired buffer size in bytes
   * @return     0 on success, -1 on error
   *
   */
  int TcpSocket::set_send_buf_size(int size) {
    return setsockopt(this->fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
  }
  /**************** Network::TcpSocket::set_send_buf_size *********************/


  /**************** Network::TcpSocket::Flush *********************************/
  /**
   * @fn         Flush
   * @brief      flush a socket by reading until it's empty
   * @param[in]  none
   * @return     none
   *
   */
  void TcpSocket::Flush() {
    struct pollfd poll_struct;
    poll_struct.events = POLLIN;
    poll_struct.fd     = this->fd;  // poll the current file descriptor

    poll( &poll_struct, 1, 1000 );  // poll up to 1 sec

    while ( true ) {
      char buf[1024];
      int len = recv( poll_struct.fd, buf, sizeof(buf), MSG_DONTWAIT );
      if ( len == -1 ) break;
    }
    return;
  }
  /**************** Network::TcpSocket::Flush *********************************/

}
