#pragma once

#include <stdint.h>
#include <string>
#include <vector>

#ifdef _WIN32
typedef int socklen_t;
struct sockaddr;
#else
#include <sys/socket.h>
#endif

#ifndef AOO_NET_USE_IPv6
#define AOO_NET_USE_IPv6 1
#endif

namespace aoo {

/*///////////// IP address ////////*/

class ip_address {
public:
    enum ip_type {
        Unspec,
        IPv4,
        IPv6
    };

    static std::vector<ip_address> get_list(const std::string& host, int port,
                                            ip_type type);

    ip_address();
    ip_address(const struct sockaddr *sa, socklen_t len);
    ip_address(uint32_t ipv4, int port);
    ip_address(const std::string& host, int port, ip_type type);

    ip_address(const ip_address& other);
    ip_address& operator=(const ip_address& other);

    bool operator==(const ip_address& other) const;

    const char* name() const;

    const char* name_unmapped() const;

    int port() const;

    bool valid() const;

    ip_type type() const;

    bool is_ipv4_mapped() const;

    const struct sockaddr *address() const {
        return (const struct sockaddr *)&address_;
    }
    struct sockaddr *address_ptr() {
        return (struct sockaddr *)&address_;
    }
    socklen_t length() const {
        return length_;
    }
    socklen_t *length_ptr() {
        return &length_;
    }
private:
    static const char *get_name(const struct sockaddr *addr);
#ifdef _WIN32
    // avoid including <winsock2.h>
    // large enough to hold both sockaddr_in
    // and sockaddr_in6 (max. 32 bytes)
    struct {
        int16_t __ss_family;
        char __ss_pad1[6];
        int64_t __ss_align;
        char __ss_pad2[16];
    } address_;
#else
    struct sockaddr_storage address_;
#endif
    socklen_t length_;
};


/*///////////// socket //////////////////*/

int socket_init();

int socket_udp();

int socket_tcp();

int socket_close(int socket);

int socket_connect(int socket, const ip_address& addr, float timeout);

int socket_bind(int socket, int port);

int socket_sendto(int socket, const char *buf, int size,
                  const ip_address& address);

int socket_receive(int socket, char *buf, int size,
                   ip_address* addr, int32_t timeout);

int socket_setsendbufsize(int socket, int bufsize);

int socket_setrecvbufsize(int socket, int bufsize);

int socket_set_nonblocking(int socket, bool nonblocking);

bool socket_signal(int socket, int port);

int socket_errno();

int socket_strerror(int err, char *buf, int size);

std::string socket_strerror(int err);

void socket_error_print(const char *label = nullptr);

/*/////////////////// endpoint /////////////////*/

class endpoint {
public:
    endpoint(int socket, const ip_address& address)
        : socket_(socket), address_(address) {}
    endpoint(const endpoint&) = delete;
    endpoint& operator=(const endpoint&) = delete;

    int send(const char *data, int size) const {
        return socket_sendto(socket_, data, size, address_);
    }

    static int32_t send(void *x, const char *data, int32_t size){
        return static_cast<endpoint *>(x)->send(data, size);
    }

    const ip_address& address() const {
        return address_;
    }
private:
    int socket_;
    ip_address address_;
};

} // aoo
