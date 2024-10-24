#pragma once

#include "../detail.hpp"
#include "aoo_requests.h"

#include <exception>

// OSC address patterns

#define kAooMsgGroupJoin \
    kAooMsgGroup kAooMsgJoin

#define kAooMsgGroupLeave \
    kAooMsgGroup kAooMsgLeave

#define kAooMsgGroupEject \
    kAooMsgGroup kAooMsgEject

#define kAooMsgGroupUpdate \
    kAooMsgGroup kAooMsgUpdate

#define kAooMsgGroupChanged \
    kAooMsgGroup kAooMsgChanged

#define kAooMsgUserUpdate \
    kAooMsgUser kAooMsgUpdate

#define kAooMsgUserChanged \
    kAooMsgUser kAooMsgChanged

#define kAooMsgPeerJoin \
    kAooMsgPeer kAooMsgJoin

#define kAooMsgPeerLeave \
    kAooMsgPeer kAooMsgLeave

#define kAooMsgPeerChanged \
    kAooMsgPeer kAooMsgChanged

// peer messages

#define kAooMsgPeerPing \
    kAooMsgDomain kAooMsgPeer kAooMsgPing

#define kAooMsgPeerPong \
    kAooMsgDomain kAooMsgPeer kAooMsgPong

#define kAooMsgPeerMessage \
    kAooMsgDomain kAooMsgPeer kAooMsgMessage

#define kAooMsgPeerAck \
    kAooMsgDomain kAooMsgPeer kAooMsgAck

// client messages

#define kAooMsgClientPing \
    kAooMsgDomain kAooMsgClient kAooMsgPing

#define kAooMsgClientPong \
    kAooMsgDomain kAooMsgClient kAooMsgPong

#define kAooMsgClientLogin \
    kAooMsgDomain kAooMsgClient kAooMsgLogin

#define kAooMsgClientQuery \
    kAooMsgDomain kAooMsgClient kAooMsgQuery

#define kAooMsgClientGroupJoin \
    kAooMsgDomain kAooMsgClient kAooMsgGroupJoin

#define kAooMsgClientGroupLeave \
    kAooMsgDomain kAooMsgClient kAooMsgGroupLeave

#define kAooMsgClientGroupEject\
    kAooMsgDomain kAooMsgClient kAooMsgGroupEject

#define kAooMsgClientGroupUpdate \
    kAooMsgDomain kAooMsgClient kAooMsgGroupUpdate

#define kAooMsgClientUserUpdate \
    kAooMsgDomain kAooMsgClient kAooMsgUserUpdate

#define kAooMsgClientGroupChanged \
    kAooMsgDomain kAooMsgClient kAooMsgGroupChanged

#define kAooMsgClientUserChanged \
    kAooMsgDomain kAooMsgClient kAooMsgUserChanged

#define kAooMsgClientPeerChanged \
    kAooMsgDomain kAooMsgClient kAooMsgPeerChanged

#define kAooMsgClientRequest \
    kAooMsgDomain kAooMsgClient kAooMsgRequest

#define kAooMsgClientPeerJoin \
    kAooMsgDomain kAooMsgClient kAooMsgPeerJoin

#define kAooMsgClientPeerLeave \
    kAooMsgDomain kAooMsgClient kAooMsgPeerLeave

#define kAooMsgClientMessage \
    kAooMsgDomain kAooMsgClient kAooMsgMessage

// server messages

#define kAooMsgServerLogin \
    kAooMsgDomain kAooMsgServer kAooMsgLogin

#define kAooMsgServerQuery \
    kAooMsgDomain kAooMsgServer kAooMsgQuery

#define kAooMsgServerPing \
    kAooMsgDomain kAooMsgServer kAooMsgPing

#define kAooMsgServerPong \
kAooMsgDomain kAooMsgServer kAooMsgPong

#define kAooMsgServerGroupJoin \
    kAooMsgDomain kAooMsgServer kAooMsgGroupJoin

#define kAooMsgServerGroupLeave \
    kAooMsgDomain kAooMsgServer kAooMsgGroupLeave

#define kAooMsgServerGroupUpdate \
    kAooMsgDomain kAooMsgServer kAooMsgGroupUpdate

#define kAooMsgServerUserUpdate \
    kAooMsgDomain kAooMsgServer kAooMsgUserUpdate

#define kAooMsgServerRequest \
    kAooMsgDomain kAooMsgServer kAooMsgRequest


namespace aoo {
namespace net {

class error : public std::exception {
public:
    error(AooError code, std::string msg)
        : code_(code), msg_(std::move(msg)) {}

    const char *what() const noexcept override {
        return msg_.c_str();
    }

    AooError code() const { return code_; }
private:
    AooError code_;
    std::string msg_;
};

AooError parse_pattern(const AooByte *msg, int32_t n,
                       AooMsgType& type, int32_t& offset);

AooSize write_relay_message(AooByte *buffer, AooSize bufsize,
                            const AooByte *msg, AooSize msgsize,
                            const ip_address& addr, bool binary);

std::string encrypt(std::string_view input);

struct ip_host {
    ip_host() = default;
    ip_host(std::string_view _name, int _port)
        : name(_name), port(_port) {}
    ip_host(const AooIpEndpoint& ep)
        : name(ep.hostName ? ep.hostName : ""), port(ep.port) {}

    std::string name;
    int port = 0;

    bool valid() const {
        // NB: name might be empty (= origin)!
        return port > 0;
    }

    bool operator==(const ip_host& other) const {
        return name == other.name && port == other.port;
    }

    bool operator!=(const ip_host& other) const {
        return !operator==(other);
    }
};

inline osc::OutboundPacketStream& operator<<(osc::OutboundPacketStream& msg, const ip_host& host) {
    if (host.valid()) {
        msg << host.name.c_str() << host.port;
    } else {
        msg << osc::Nil << osc::Nil;
    }
    return msg;
}

inline std::optional<AooIpEndpoint> osc_read_host(osc::ReceivedMessageArgumentIterator& it) {
    if (it->IsNil()) {
        it++; it++;
        return std::nullopt;
    } else {
        AooIpEndpoint ep;
        ep.hostName = (it++)->AsString();
        ep.port = (it++)->AsInt32();
        // NB: empty string is allowed (= origin)!
        if (ep.port > 0) {
            return ep;
        } else {
            throw osc::MalformedPacketException("bad port argument for IP endpoint");
        }
    }
}

} // namespace net
} // namespace aoo
