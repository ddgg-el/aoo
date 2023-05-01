#include "tcp_server.hpp"

#include "common/utils.hpp"

#include <algorithm>
#include <thread>
#include <utility>

namespace aoo {

void tcp_server::start(int port, accept_handler accept, receive_handler receive) {
    accept_handler_ = std::move(accept);
    receive_handler_ = std::move(receive);

    event_socket_ = socket_udp(0);
    if (event_socket_ < 0) {
        auto e = socket_errno();
        throw std::runtime_error("couldn't create event socket: "
                                 + socket_strerror(e));
    }

    listen_socket_ = socket_tcp(port);
    if (listen_socket_ < 0) {
        // cache errno
        auto e = socket_errno();
        // clean up
        socket_close(event_socket_);
        throw std::runtime_error("couldn't create/bind TCP socket: "
                                 + socket_strerror(e));
    }
    // listen
    if (listen(listen_socket_, SOMAXCONN) < 0){
        // cache errno
        auto e = socket_errno();
        // clean up
        socket_close(event_socket_);
        socket_close(listen_socket_);
        throw std::runtime_error("listen() failed: "
                                 + socket_strerror(e));
    }

    // prepare poll array for listen and event socket
    poll_array_.resize(2);

    poll_array_[listen_index].events = POLLIN;
    poll_array_[listen_index].revents = 0;
    poll_array_[listen_index].fd = listen_socket_;

    poll_array_[event_index].events = POLLIN;
    poll_array_[event_index].revents = 0;
    poll_array_[event_index].fd = event_socket_;

    LOG_DEBUG("tcp_server: start listening on port " << port);
}

void tcp_server::run() {
    last_error_ = 0;
    running_.store(true);

    loop();

    do_close();
}

void tcp_server::stop() {
    bool running = running_.exchange(false);
    if (running) {
        if (!socket_signal(event_socket_)) {
            // force wakeup by closing the socket.
            // this is not nice and probably undefined behavior,
            // the MSDN docs explicitly forbid it!
            socket_close(event_socket_);
        }
    }
}

void tcp_server::do_close() {
    // close listening socket
    if (listen_socket_ != invalid_socket) {
        LOG_DEBUG("tcp_server: stop listening");
        socket_close(listen_socket_);
        listen_socket_ = invalid_socket;
    }

    if (event_socket_ != invalid_socket) {
        socket_close(event_socket_);
        event_socket_ = invalid_socket;
    }

    // close client sockets
    for (auto& c : clients_){
        if (c.socket != invalid_socket) {
            socket_close(c.socket);
        }
    }

    clients_.clear();
    poll_array_.clear();
    stale_clients_.clear();
}

tcp_server::~tcp_server() {
    do_close();
}

int tcp_server::send(AooId client, const AooByte *data, AooSize size) {
    // LATER optimize look up
    for (auto& c : clients_) {
        if (c.id == client) {
            // there is some controversy on whether send() in blocking mode
            // might do partial writes. To be sure, we call send() in a loop.
            int32_t nbytes = 0;
            while (nbytes < size){
                auto result = ::send(c.socket, (const char *)data + nbytes, size - nbytes, 0);
                if (result >= 0){
                    nbytes += result;
                } else {
                    return -1;
                }
            }
            return size;
        }
    }
    LOG_ERROR("tcp_server: send(): unknown client (" << client << ")");
    return -1;
}

bool tcp_server::close(AooId client) {
    for (size_t i = 0; i < clients_.size(); ++i) {
        if (clients_[i].id == client) {
            socket_close(clients_[i].socket);
            // mark as stale (will be ignored in poll())
            clients_[i].socket = invalid_socket;
            poll_array_[client_index + i].fd = invalid_socket;
            stale_clients_.push_back(i);
            LOG_DEBUG("tcp_server: closed client " << client);
            return true;
        }
    }
    return false;
}

void tcp_server::loop() {
    while (running_.load()) {
        // NOTE: macOS/BSD requires the negative timeout to be exactly -1!
    #ifdef _WIN32
        int result = WSAPoll(poll_array_.data(), poll_array_.size(), -1);
    #else
        int result = ::poll(poll_array_.data(), poll_array_.size(), -1);
    #endif
        if (result < 0) {
        #ifdef _WIN32
            int err = WSAGetLastError();
        #else
            int err = errno;
        #endif
            if (err != EINTR) {
                LOG_ERROR("tcp_server: poll() failed (" << err << ")");
                break;
            }
        }

        // first receive from clients
        receive_from_clients();

        // finally accept new clients (modifies the client list!)
        accept_client();
    }
}

void tcp_server::receive_from_clients() {
    for (int i = 0; i < clients_.size(); ++i) {
        auto& fdp = poll_array_[client_index + i];
        auto revents = std::exchange(fdp.revents, 0);
        if (revents == 0) {
            continue; // no event
        }
    #ifdef _WIN32
        if (fdp.fd == INVALID_SOCKET) {
            // Windows does not simply ignore invalid socket descriptors,
            // instead it would return POLLNVAL...
            continue;
        }
    #endif
        auto& c = clients_[i];
        if ((revents & POLLIN) || (revents & POLLERR) || (revents & POLLHUP)) {
            if (revents & POLLERR) {
                LOG_DEBUG("tcp_server: POLLERR");
            } else if (revents & POLLHUP) {
                LOG_DEBUG("tcp_server: POLLHUP");
            }
            // receive data from client
            AooByte buffer[AOO_MAX_PACKET_SIZE];
            auto result = ::recv(c.socket, (char *)buffer, sizeof(buffer), 0);
            if (result > 0) {
                // success
                receive_handler_(c.id, 0, buffer, result);
                continue;
            } else {
                // failure
                if (result == 0) {
                    // client disconnected
                    LOG_DEBUG("tcp_server: connection closed by client");
                    on_error(c.id, 0);
                } else {
                    // error
                    int e = socket_errno();
                    if (e == EINTR) {
                        continue;
                    }
                    LOG_DEBUG("tcp_server: recv() failed: " << socket_strerror(e));
                    on_error(c.id, e);
                }
            }
        } else if (revents & POLLNVAL) {
            // invalid socket, shouldn't happen...
            LOG_DEBUG("tcp_server: POLLNVAL");
            on_error(c.id, EINVAL);
        } else {
            // shouldn't happen, ignore for now...
            LOG_ERROR("tcp_server: unexpected revent value " << revents);
            continue;
        }

        socket_close(c.socket);
        // mark as stale (will be ignored in poll())
        c.socket = invalid_socket;
        poll_array_[client_index + i].fd = invalid_socket;
        stale_clients_.push_back(i);
    }

#if 1
    // purge stale clients (if necessary)
    if (stale_clients_.size() > max_stale_clients) {
        clients_.erase(std::remove_if(clients_.begin(), clients_.end(),
                       [](auto& c) { return c.socket < 0; }), clients_.end());

        poll_array_.erase(std::remove_if(poll_array_.begin(), poll_array_.end(),
                       [](auto& p) { return p.fd < 0; }), poll_array_.end());

        stale_clients_.clear();
    }
#endif
}

void tcp_server::accept_client() {
    auto revents = std::exchange(poll_array_[listen_index].revents, 0);
    if (revents == 0) {
        return; // no event
    }

    // NB: also call accept() on POLLERR, so we may get the peer address
    if ((revents & POLLIN) || (revents & POLLERR)) {
        ip_address addr(ip_address::max_length);
        auto sock = (AooSocket)::accept(listen_socket_, addr.address_ptr(), addr.length_ptr());
        if (sock != invalid_socket) {
        #if 1
            // disable Nagle's algorithm
            int val = 1;
            if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
                           (char *)&val, sizeof(val)) != 0) {
                LOG_ERROR("tcp_server: couldn't set TCP_NODELAY");
            }
        #endif
            auto id = accept_handler_(0, addr, sock);
            if (id == kAooIdInvalid) {
                // user refused to accept client
                socket_close(sock);
                return;
            }

            if (!stale_clients_.empty()) {
                // reuse stale client
                auto index = stale_clients_.back();
                stale_clients_.pop_back();

                clients_[index] = client { addr, sock, id };
                poll_array_[client_index + index].fd = sock;
            } else {
                // add new client
                clients_.push_back(client { addr, sock, id });
                pollfd p;
                p.fd = sock;
                p.events = POLLIN;
                p.revents = 0;
                poll_array_.push_back(p);
            }
            LOG_DEBUG("tcp_server: accepted client " << addr << " " << id);
        } else {
            handle_accept_error(socket_errno(), addr);
        }
    } else if (revents & POLLHUP) {
        LOG_DEBUG("tcp_server: POLLHUP");
        // shouldn't happen on listening socket...
        on_error(socket_error(listen_socket_));
    #if 1
        running_.store(false); // quit server
    #endif
    } else if (revents & POLLNVAL) {
        LOG_DEBUG("tcp_server: POLLNVAL");
        on_error(EINVAL);
    #if 1
        running_.store(false); // quit server
    #endif
    } else {
        // just ignore for now
        LOG_ERROR("tcp_server: unexpected revent value " << revents);
    }
}

void tcp_server::handle_accept_error(int code, const ip_address &addr) {
    // avoid spamming the console with repeated errors
    thread_local ip_address last_addr;
    if (code != last_error_ || addr != last_addr) {
        if (addr.valid()) {
            LOG_DEBUG("tcp_server: could not accept " << addr
                      << ": " << socket_strerror(code));
        } else {
            LOG_DEBUG("tcp_server: accept() failed: " << socket_strerror(code));
        }
    }
    last_error_ = code;
    last_addr = addr;
    // handle error
    switch (code) {
    // We ran out of open file descriptors.
#ifdef _WIN32
    case WSAEMFILE:
#else
    case EMFILE:
    case ENFILE:
#endif
        // Sleep to avoid hogging the CPU, but do not stop the server
        // because we might be able to recover.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        break;

    // non-fatal errors
    // TODO: should we rather explicitly check for fatal errors instead?
#ifdef _WIN32
    // remote peer has terminated the connection
    case WSAECONNRESET:
#else
    // remote peer has terminated the connection
    case ECONNABORTED:
    // reached process/system limit for open file descriptors
    // interrupted by signal handler
    case EINTR:
    // not allowed by firewall rules
    case EPERM:
#endif
#ifdef __linux__
    // On Linux, also ignore TCP/IP errors! See man page for accept().
    case ENETDOWN:
    case EPROTO:
    case ENOPROTOOPT:
    case EHOSTDOWN:
    case ENONET:
    case EHOSTUNREACH:
    case EOPNOTSUPP:
    case ENETUNREACH:
#endif
        break;

    default:
        // fatal error
        on_error(code);
#if 1
        running_.store(false); // quit server
#endif
        break;
    }
}

} // aoo
