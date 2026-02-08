#include "connector.h"
#include <iostream>

Connector::Connector(net::io_context& ioc,
                     ssl::context& ssl_ctx,
                     const std::string& host,
                     const std::string& port,
                     const std::string& path)
    : ioc_(ioc),
      ssl_ctx_(ssl_ctx),
      resolver_(net::make_strand(ioc)),
      ws_(net::make_strand(ioc), ssl_ctx),
      host_(host),
      port_(port),
      path_(path) {}

void Connector::start() {
    resolver_.async_resolve(
        host_, port_,
        beast::bind_front_handler(&Connector::on_resolve, shared_from_this()));
}

void Connector::on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
    if (ec) {
        std::cerr << "Resolve error: " << ec.message() << std::endl;
        return;
    }

    beast::get_lowest_layer(ws_).async_connect(
        results,
        beast::bind_front_handler(&Connector::on_connect, shared_from_this()));
}

void Connector::on_connect(beast::error_code ec,
                            tcp::resolver::results_type::endpoint_type) {
    if (ec) {
        std::cerr << "Connect error: " << ec.message() << std::endl;
        return;
    }

    ws_.next_layer().async_handshake(
        ssl::stream_base::client,
        beast::bind_front_handler(&Connector::on_ssl_handshake, shared_from_this()));
}

void Connector::on_ssl_handshake(beast::error_code ec) {
    if (ec) {
        std::cerr << "SSL handshake error: " << ec.message() << std::endl;
        return;
    }

    ws_.async_handshake(
        host_, path_,
        beast::bind_front_handler(&Connector::on_ws_handshake, shared_from_this()));
}

void Connector::on_ws_handshake(beast::error_code ec) {
    if (ec) {
        std::cerr << "WebSocket handshake error: " << ec.message() << std::endl;
        return;
    }

    do_read();
}

void Connector::do_read() {
    ws_.async_read(
        buffer_,
        [self = shared_from_this()](beast::error_code ec, std::size_t) {
            if (ec) {
                std::cerr << "Read error: " << ec.message() << std::endl;
                return;
            }

            std::string msg = beast::buffers_to_string(self->buffer_.data());
            self->buffer_.consume(self->buffer_.size());
            self->on_message(msg);
            self->do_read();
        });
}

void Connector::send(const std::string& msg) {
    ws_.async_write(
        net::buffer(msg),
        [](beast::error_code ec, std::size_t) {
            if (ec) {
                std::cerr << "Write error: " << ec.message() << std::endl;
            }
        });
}
