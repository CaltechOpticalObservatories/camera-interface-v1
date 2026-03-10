/*
 * message.h
 *
 */
#pragma once
#include <zmq.hpp>

namespace Message {

  using MessageHandler = std::function<std::string(const std::string &message)>;

  class Server {
    private:
      zmq::context_t context_;
      zmq::socket_t router_;
      zmq::socket_t pull_;

      MessageHandler handler_;

      std::atomic<bool> isrunning{false};

      std::string inproc_address;
      std::thread poll_thread;
      int zmqport;

    public:
      Server() : router_(context_, ZMQ_ROUTER), pull_(context_, ZMQ_PULL) {}

      std::string test(const std::string &message) { std::cerr << message << "\n"; return handler_(message); }
      void set_handler(MessageHandler handler) { handler_ = std::move(handler); }
      void start();
      void stop();
      void poll_loop() {
        zmq::message_t routing_frame;
        zmq::message_t messageid_frame;
        zmq::message_t payload_frame;

        std::string routing( static_cast<char*>(routing_frame.data()), routing_frame.size() );
        std::string messageid( static_cast<char*>(messageid_frame.data()), messageid_frame.size() );
        std::string payload( static_cast<char*>(payload_frame.data()), payload_frame.size() );
      }

  };

  class Client {
  };

  class xPubSub {
  };

  class xPubSubHandler {
  };

}
