/**
 * @file     message.h
 * @brief    generic ZMQ messaging library
 * @details  This contains a Server class for accepting messages from processes,
 *           a Client class for sending messages to processes, and a
 *           PublisherSubscriber class for broadcasting and accepting
 *           one-to-many messages. The Client/Server classes use the ROUTER/DEALER
 *           pattern and the PubSub class uses (surprise!) the PUB/SUB pattern.
 *           This uses the header-only C++ bindings for libzmq.
 * @author   David Hale <dhale@astro.caltech.edu>
 * @date     2026-Mar-09
 *
 */

#pragma once

#include <zmq.hpp>
#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <future>
#include <unordered_map>
#include <mutex>
#include <optional>
#include <iostream>

namespace Message {

  /// callback passed to the message handler
  using AckFunction    = std::function<void(const std::string&)>;
  /// registered message handler
  using MessageHandler = std::function<std::string(const std::string &message, AckFunction send_ack)>;

  const std::string MSG_SERVICE_ADDRESS = "tcp://127.0.0.1";

  const std::string MSG_ACK  = "ACK";
  const std::string MSG_DONE = "DONE";
  const std::string MSG_ERR  = "ERROR";
  const std::string MSG_PING = "PING";
  const std::string MSG_PONG = "PONG";

  constexpr int HEARTBEAT_INTERVAL_MS     = 2000;  ///< send heartbeat at this rate
  constexpr int HEARTBEAT_TIMEOUT_MS      = 6000;  ///< drop if silent this long
  constexpr int HEARTBEAT_TIME_TO_LIVE_MS = 6000;  ///< maximum age for receiver message

  /***** Message::timestamp ***************************************************/
  /**
   * @brief      returns a timestamp string for logging to stderr
   */
  inline std::string timestamp() {
    auto now = std::chrono::system_clock::now();
    auto us  = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch())%1000000;
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    if (gmtime_r(&time, &tm)==NULL) { perror("gmtime_r failed"); return ""; }
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
    char out[28];
    std::snprintf(out, sizeof(out), "%s.%06ld", buf, us.count());
    return std::string(out);
  }

  /***** Message::Server ******************************************************/
  /**
   * @brief      defines a server that accepts messages, calls a message handler,
   *             and returns replies
   */
  class Server {
    private:
      zmq::context_t context;
      zmq::socket_t router;
      zmq::socket_t pull;

      MessageHandler handler;  ///< user-provided callback function to handle messages

      std::atomic<bool> isrunning{false};

      std::string inproc_address{"inproc://replies"};  ///< in-process memory pipe
      std::thread poll_thread;
      int zmqport{-1};

    public:
      Server() : router(context, ZMQ_ROUTER), pull(context, ZMQ_PULL) {}

      /***** Message::Server::get_isrunning ***********************************/
      bool get_isrunning() const { return isrunning; }

      /***** Message::Server::set_handler *************************************/
      /**
       * @brief      registers the message handling function
       */
      void set_handler(MessageHandler h) { handler = std::move(h); }

      /***** Message::Server::start *******************************************/
      /**
       * @brief      binds the router and pull sockets and gets the polling thread started
       * @details    The pull socket is how I receive data back from the worker thread.
       * @throws     std::runtime_error
       */
      void start(int port) {
        if (isrunning) throw std::runtime_error("Message::Server::start already running");
        if (port>0) zmqport=port; else throw std::runtime_error("invalid port '"+std::to_string(port)+"'");
        router.set(zmq::sockopt::linger, 0);
        router.set(zmq::sockopt::heartbeat_ivl,     HEARTBEAT_INTERVAL_MS);
        router.set(zmq::sockopt::heartbeat_timeout, HEARTBEAT_TIMEOUT_MS);
        router.set(zmq::sockopt::heartbeat_ttl,     HEARTBEAT_TIME_TO_LIVE_MS);

        router.bind(Message::MSG_SERVICE_ADDRESS+":"+std::to_string(zmqport));

        pull.bind(inproc_address);

        isrunning=true;

        std::cerr << timestamp() << "  (Message::Server::start) messaging server started on port " << zmqport << "\n";

        poll_thread = std::thread(&Server::poll_loop, this);
      }

      /***** Message::Server::stop ********************************************/
      /**
       * @brief      signals the poll loop to stop and waits for the thread to exit
       */
      void stop() {
        isrunning=false;
        if (poll_thread.joinable()) poll_thread.join();
        std::cerr << timestamp() << "  (Message::Server::stop) messaging server on port " << zmqport << " stopped\n";
      }

    private:
      /***** Message::Server::poll_loop ***************************************/
      /**
       * @brief      watches the ZMQ sockets and spawns worker to handle incoming message
       */
      void poll_loop() {
        zmq::pollitem_t items[] = {
          { router.handle(), 0, ZMQ_POLLIN, 0 },  ///< incoming messages from clients
          { pull.handle(),   0, ZMQ_POLLIN, 0 }   ///< completed replies from worker threads
        };

        while (isrunning) {
          // wait up to 100ms for activity on either item
          zmq::poll(items, 2, std::chrono::milliseconds(100));

          // a dealer sent a message
          if (items[0].revents & ZMQ_POLLIN) {
            zmq::message_t routing_frame, messageid_frame, payload_frame;

            // received the frames from the router
            auto frame1 = router.recv(routing_frame,   zmq::recv_flags::none);
            auto frame2 = router.recv(messageid_frame, zmq::recv_flags::none);
            auto frame3 = router.recv(payload_frame,   zmq::recv_flags::none);

            if (!frame1 || !frame2 || !frame3) {
              std::cerr << timestamp() << "  (Message::Server::poll_loop) ERROR receiving ZMQ message frames from router port " << zmqport << "\n";
              continue;
            }

            // pass that message to a worker thread
            std::string routing  ( static_cast<char*>(routing_frame.data()),   routing_frame.size()   );
            std::string messageid( static_cast<char*>(messageid_frame.data()), messageid_frame.size() );
            std::string payload  ( static_cast<char*>(payload_frame.data()),   payload_frame.size()   );

            std::thread(&Message::Server::worker, this, routing, messageid, payload).detach();
          }

          // the worker thread has pushed back a reply
          if (items[1].revents & ZMQ_POLLIN) {
            zmq::message_t routing_frame, messageid_frame, reply_frame;

            // received the frames from the worker
            auto frame1 = pull.recv(routing_frame,   zmq::recv_flags::none);
            auto frame2 = pull.recv(messageid_frame, zmq::recv_flags::none);
            auto frame3 = pull.recv(reply_frame,     zmq::recv_flags::none);

            if (!frame1 || !frame2 || !frame3) {
              std::cerr << timestamp() << "  (Message::Server::poll_loop) ERROR receiving ZMQ reply frames from worker\n";
              continue;
            }

            // push them to the router
            router.send(routing_frame,   zmq::send_flags::sndmore);
            router.send(messageid_frame, zmq::send_flags::sndmore);
            router.send(reply_frame,     zmq::send_flags::none);
          }
        }
      }

      /***** Message::Server::worker ******************************************/
      /**
       * @brief      process incoming message using supplied callback handler
       * @param[in]  routing    ZMQ-internal routing identity, identifies DEALER
       *                        so ROUTER can route reply to the correct client
       * @param[in]  messageid  client-generated message ID echoed back so the
       *                        client can match reply with request
       * @param[in]  payload    the message from the client (command, etc.)
       */
      void worker(const std::string &routing, const std::string &messageid, const std::string &payload) {

        // intercept PING and reply PONG
        if (payload==Message::MSG_PING) {
          push_reply(routing, messageid, Message::MSG_PONG);
          return;
        }

        AckFunction ack = std::bind(&Server::push_reply, this, routing, messageid, std::placeholders::_1);

        // call my message handler function
        // this is an external function that must have been registered
        //
        std::string reply = handler ? handler(payload, ack) : "no handler registered";

        push_reply(routing, messageid, reply);
      }

      /***** Message::Server::push_reply **************************************/
      /**
       * @brief      delivers reply to poll_loop
       * @details    This creates a temporary socket connected to an in-process
       *             memory pipe.
       * @param[in]  routing    the DEALER's routing identity (ZMQ-internal)
       * @param[in]  messageid  message ID frame (client-generated and owned)
       * @param[in]  reply      reply string to deliver
       */
      void push_reply(const std::string &routing, const std::string &messageid, const std::string &reply) {
        // create a transient ZMQ push socket
        //
        zmq::socket_t push(context, ZMQ_PUSH);
        push.set(zmq::sockopt::linger, 0);

        // connect to the inproc address
        push.connect(inproc_address);

        // push the frames back to the router, matching the reply to the router and messageid
        push.send(zmq::buffer(routing),   zmq::send_flags::sndmore);
        push.send(zmq::buffer(messageid), zmq::send_flags::sndmore);
        push.send(zmq::buffer(reply),     zmq::send_flags::none);
      }
  };

  /***** Message::Client ******************************************************/
  /**
   * @brief      defines a DEALER client for sending messages to ROUTER server
   */
  class Client {
    private:
      struct PendingRequest {
        std::optional<std::promise<std::string>> ack_promise;
        std::promise<std::string> complete_promise;
      };

      zmq::context_t    context;
      std::string       endpoint;
      std::thread       recv_thread;
      std::atomic<bool> isconnected{false};
      std::mutex        send_mtx;
      std::mutex        pending_mtx;

      zmq::socket_t sock;
      std::unordered_map<std::string, PendingRequest> pending;

      std::atomic<uint32_t> messageid{0};

    public:
      Client() : sock(context, ZMQ_DEALER) {}

      ~Client() { disconnect(); }

      struct RequestHandle {
        RequestHandle() = default;
        RequestHandle(RequestHandle&&) = default;
        RequestHandle& operator=(RequestHandle&&) = default;
        RequestHandle(const RequestHandle&) = delete;
        RequestHandle& operator=(const RequestHandle&) = delete;

        bool expects_ack{false};
        std::future<std::string> ack_future;
        std::future<std::string> complete_future;

        std::string wait_ack(int timeout_ms=3000) {
          if (!ack_future.valid()) throw std::runtime_error("Message::Client::wait_ack ACK not expected");
          if (ack_future.wait_for(std::chrono::milliseconds(timeout_ms))==std::future_status::timeout) {
            throw std::runtime_error("Message::Client::wait_ack timeout");
          }
          return ack_future.get();
        }

        std::string wait_complete(int timeout_ms=60000) {
          if (!complete_future.valid()) throw std::runtime_error("Message::Client::wait_complete invalid future");
          if (complete_future.wait_for(std::chrono::milliseconds(timeout_ms))==std::future_status::timeout) {
            throw std::runtime_error("Message::Client::wait_complete timeout");
          }
          return complete_future.get();
        }
      };

      /***** Message::Client::connect *****************************************/
      /**
       * @brief      connects this ZMQ DEALER to remote ROUTER socket
       * @param[in]  endpoint_in  name:port of the ROUTER Server
       * @throws     std::runtime_error
       */
      void connect(const std::string &endpoint_in) {
        if (isconnected) throw std::runtime_error("Message::Client::connect already connected");
        endpoint = endpoint_in;
        isconnected = true;
        sock.set(zmq::sockopt::linger,               0);
        sock.set(zmq::sockopt::heartbeat_ivl,     HEARTBEAT_INTERVAL_MS);
        sock.set(zmq::sockopt::heartbeat_timeout, HEARTBEAT_TIMEOUT_MS);
        sock.connect(endpoint);

        recv_thread = std::thread(&Client::recv_loop, this);
      }

      /***** Message::Client::disconnect **************************************/
      /**
       * @brief      disconnects from ZMQ ROUTER socket
       */
      void disconnect() {
        isconnected=false;
        if (recv_thread.joinable()) recv_thread.join();
      }

      /***** Message::Client::reconnect ***************************************/
      /**
       * @brief      reconnect to last ZMQ ROUTER socket
       */
      void reconnect() {
        disconnect();
        connect(endpoint);
      }

      /***** Message::Client::send ********************************************/
      /**
       * @brief      send a message and return a handle to wait for reply
       * @details    This returns immediately with a handle containing two futures.
       *             Call handle.wait_complete() to block until server signals
       *             completion (DONE|ERROR).
       *             If an ACK is desired, pass sendack=true and call handle.wait_ack()
       *             to confirm receipt, then handle.wait_complete().
       * @param[in]  message        message string to send to server
       * @param[in]  sendack        false|true to allow sending immediate ACK
       * @return     RequestHandle  complete_future and optionally ack_future
       * @throws     std::runtime_error
       */
      RequestHandle send(const std::string &message, bool sendack=false) {
        if (endpoint.empty()) throw std::runtime_error("Message::Client::send not connected");

        const std::string id = std::to_string(messageid.fetch_add(1, std::memory_order_relaxed));

        RequestHandle handle;
        {
        std::lock_guard<std::mutex> lock(pending_mtx);
        auto &req = pending[id];
        if (sendack) {
          req.ack_promise.emplace();
          handle.ack_future = req.ack_promise->get_future();
        }

        handle.expects_ack     = sendack;
        handle.complete_future = req.complete_promise.get_future();
        }

        {
        std::lock_guard<std::mutex> lock(send_mtx);
        sock.send(zmq::buffer(id),      zmq::send_flags::sndmore);
        sock.send(zmq::buffer(message), zmq::send_flags::none);
        }

        return handle;
      }

      /***** Message::Client::is_alive ****************************************/
      /**
       * @brief      sends a PING to the server
       * @param[in]  timeout_ms  override default timeout
       * @return     true|false
       */
      bool is_alive(int timeout_ms=1000) {
        try {
          auto handle = send(Message::MSG_PING);
          return handle.wait_complete(timeout_ms).find(Message::MSG_PONG) != std::string::npos;
        }
        catch(...) { return false; }
      }

    private:
      /***** Message::Client::recv_loop ***************************************/
      /**
       * @brief      receives replies and fulfils pending promises
       * @details    run as a thread
       */
      void recv_loop() {
        zmq::pollitem_t item = { sock.handle(), 0, ZMQ_POLLIN, 0 };

        while (isconnected) {
          if (zmq::poll(&item, 1, std::chrono::milliseconds(100))==0) continue;

          zmq::message_t messageid_frame, reply_frame;
          auto frame1 = sock.recv(messageid_frame, zmq::recv_flags::none);
          auto frame2 = sock.recv(reply_frame,     zmq::recv_flags::none);

          if (!frame1 || !frame2) {
            std::cerr << timestamp() << "  (Message::Client::recv_loop) ERROR receiving ZMQ reply frames from " << endpoint << "\n";
            continue;
          }

          std::string id    (static_cast<char*>(messageid_frame.data()), messageid_frame.size());
          std::string reply (static_cast<char*>(reply_frame.data()),     reply_frame.size());

          bool is_ack = (reply==Message::MSG_ACK);
          bool is_complete = (reply.find(Message::MSG_PONG) != std::string::npos ||
                              reply.find(Message::MSG_DONE) != std::string::npos ||
                              reply.find(Message::MSG_ERR)  != std::string::npos);

          std::lock_guard<std::mutex> lock(pending_mtx);

          auto it = pending.find(id);
          if (it==pending.end()) continue;

          auto &req = it->second;

          if (is_ack && req.ack_promise.has_value()) {
            req.ack_promise->set_value(reply);
          }
          else
          if (is_complete) {
            req.complete_promise.set_value(reply);
            pending.erase(it);
          }
          else {
            std::cerr << timestamp() << "  (Message::Client::recv_loop) unrecognized reply '"
                                         << reply << "' for id " << id << " ignored\n";
          }
        }
      }
  };

  class xPubSub {
  };

  class xPubSubHandler {
  };

}
