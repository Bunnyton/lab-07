#ifndef LAB_07_HTTP_SERVER_HPP_
#define LAB_07_HTTP_SERVER_HPP_

//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: HTTP server, synchronous
//
//------------------------------------------------------------------------------

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/config.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <suggest.hpp>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

using json = nlohmann::json;


// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template< class Body, class Allocator, class Send>

void handle_request(
    Suggest &sug,
    const std::shared_ptr<std::timed_mutex> &mutex,
    http::request<Body, http::basic_fields<Allocator>>&& req,
    Send&& send)
{
  // Returns a bad request response
  auto const bad_request =
      [&req](beast::string_view why)
      {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = std::string(why);
        res.prepare_payload(); // полезная нагрузка
        return res;
      };

  auto const not_found = [&req](beast::string_view target) {
    http::response<http::string_body> res{http::status::not_found,
                                          req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = "The resource '" + std::string(target) +
                 "' was not found.";
    res.prepare_payload();
    return res;
  };

  if (req.method() != http::verb::post) {
    return send(bad_request("Unknown HTTP-method. Y"
                            "ou should use POST method"));
  }

  // Путь запроса должен быть "/v1/api/suggest".
  if (req.target() != "/v1/api/suggest") {
    return send(not_found(req.target()));
  }

  json input_body;
  try{
    input_body = json::parse(req.body());
  } catch (std::exception& e){
    return send(bad_request(e.what()));
  }

  boost::optional<std::string> input;
  try {
    input = input_body.at("input").get<std::string>();
  } catch (std::exception& e){
    return send(bad_request(R"(JSON format: {"input" : "<user_input">})"));
  }
  if (!input.has_value()){
    return send(bad_request(R"(JSON format: {"input" : "<user_input">})"));
  }
  mutex->lock();
  http::string_body::value_type body  = sug.suggest_request(input.value());
  mutex->unlock();

  auto const size = body.size();

  http::response<http::string_body> res{
      std::piecewise_construct, std::make_tuple(std::move(body)),
      std::make_tuple(http::status::ok, req.version())};
  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(http::field::content_type, "application/json");
  res.content_length(size);
  res.keep_alive(req.keep_alive());
  return send(std::move(res));
}

//------------------------------------------------------------------------------

// Report a failure
void
fail(beast::error_code ec, char const* what)
{
  std::cerr << what << ": " << ec.message() << "\n";
}

// This is the C++11 equivalent of a generic lambda.
// The function object is used to send an HTTP message.
template<class Stream>
struct send_lambda
{
  Stream& stream_;
  bool& close_;
  beast::error_code& ec_;

  explicit
  send_lambda(
      Stream& stream,
      bool& close,
      beast::error_code& ec)
      : stream_(stream)
      , close_(close)
      , ec_(ec)
  {
  }

  template<bool isRequest, class Body, class Fields>
  void
  operator()(http::message<isRequest, Body, Fields>&& msg) const
  {
    // Determine if we should close the connection after
    close_ = msg.need_eof();

    // We need the serializer here because the serializer requires
    // a non-const file_body, and the message oriented version of
    // http::write only works with const messages.
    http::serializer<isRequest, Body, Fields> sr{msg};
    http::write(stream_, sr, ec_);
  }
};

// Handles an HTTP server connection
void do_session(tcp::socket& socket, Suggest &sug, const std::shared_ptr<std::timed_mutex> &mutex)
{
  bool close = false;
  beast::error_code ec;

  // This buffer is required to persist across reads
  beast::flat_buffer buffer;

  // This lambda is used to send messages
  send_lambda<tcp::socket> lambda{socket, close, ec};

  for(;;)
  {
    // Read a request
    http::request<http::string_body> req;
    http::read(socket, buffer, req, ec);

    std::cout << req;

    if(ec == http::error::end_of_stream)
      break;
    if(ec)
      return fail(ec, "read");

    // Send the response
    handle_request(sug, mutex, std::move(req), lambda);
    if(ec)
      return fail(ec, "write");
    if(close)
    {
      // This means we should close the connection, usually because
      // the response indicated the "Connection: close" semantic.
      break;
    }
  }

  // Send a TCP shutdown
  socket.shutdown(tcp::socket::shutdown_send, ec);

  // At this point the connection is closed gracefully
}

#endif  // LAB_07_HTTP_SERVER_HPP_
