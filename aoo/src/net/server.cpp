/* Copyright (c) 2010-Now Christof Ressi, Winfried Ritsch and others.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "server.hpp"
#include "server_events.hpp"

#include <functional>
#include <algorithm>
#include <iostream>
#include <sstream>

#include "../binmsg.hpp"

//----------------------- Server --------------------------//

AOO_API AooServer * AOO_CALL AooServer_new(AooError *err) {
    try {
        if (err) {
            *err = kAooErrorNone;
        }
        return aoo::construct<aoo::net::Server>();
    } catch (const std::bad_alloc&) {
        if (err) {
            *err = kAooErrorOutOfMemory;
        }
        return nullptr;
    }
}

aoo::net::Server::Server() {
    sendbuffer_.resize(AOO_MAX_PACKET_SIZE);
}

AOO_API void AOO_CALL AooServer_free(AooServer *server){
    // cast to correct type because base class
    // has no virtual destructor!
    aoo::destroy(static_cast<aoo::net::Server *>(server));
}

aoo::net::Server::~Server() {}

AOO_API AooError AOO_CALL AooServer_setup(
    AooServer *server, AooServerSettings *settings)
{
    if (settings == nullptr) {
        return kAooErrorBadArgument;
    }
    return server->setup(*settings);
}

AooError AOO_CALL aoo::net::Server::setup(AooServerSettings& settings) {
    if (settings.portNumber == 0) {
        return kAooErrorBadArgument;
    }
    // NB: socketType will be modified!
    auto& type = settings.socketType;
    if ((type & kAooSocketIPv4Mapped) &&
            (!(type & kAooSocketIPv6) || (type & kAooSocketIPv4))) {
        LOG_ERROR("AooServer: combination of setup flags not allowed");
        return kAooErrorBadArgument;
    }

    bool external = settings.options & kAooServerExternalUDPSocket;
    if (external && (type == 0)) {
        // external UDP socket needs IP flags
        return kAooErrorBadArgument;
    } else if (type == 0) {
        type = kAooSocketDualStack; // default
    }

    if (external) {
        if (settings.sendFunc) {
            udp_sendfn_ = sendfn(settings.sendFunc, settings.userData);
        } else {
            return kAooErrorBadArgument;
        }
    } else {
        udp_sendfn_ = sendfn(Server::send, this);
    }

    // in case run() has been called in non-blocking mode
    close();

    // TODO: honor flags for UDP and TCP sockets! For now just use default.
    if (!external) {
        try {
            // TODO: settings
            udp_server_.start(settings.portNumber,
                [this](auto... args) { handle_udp_packet(args...);
            });
        } catch (const aoo::udp_error& e) {
            LOG_ERROR("AooServer: failed to start UDP server: " << e.what());
            aoo::socket_set_errno(e.code());
            return kAooErrorSocket;
        }
        // update socket flags
        type = socket_get_flags(udp_server_.socket());
    }

    try {
        tcp_server_.start(settings.portNumber,
            [this](auto... args) { return accept_client(args...); },
            [this](auto... args) { handle_client_data(args...); });
    } catch (const aoo::tcp_error& e) {
        LOG_ERROR("AooServer: failed to start TCP server: " << e.what());
        aoo::socket_set_errno(e.code());
        return kAooErrorSocket;
    }

    if (type & kAooSocketIPv6) {
        if (type & kAooSocketIPv4) {
            address_family_ = ip_address::Unspec; // both IPv6 and IPv4
        } else {
            address_family_ = ip_address::IPv6;
        }
    } else {
        address_family_ = ip_address::IPv4;
    }

    use_ipv4_mapped_ = type & kAooSocketIPv4Mapped;

    port_ = settings.portNumber;

    return kAooOk;
}

AOO_API AooError AOO_CALL AooServer_run(
    AooServer *server, AooBool nonBlocking)
{
    return server->run(nonBlocking);
}

AooError AOO_CALL aoo::net::Server::run(AooBool nonBlocking) {
    try {
        while  (tcp_server_.running()) {
            // first check client timers
            auto now = aoo::time_tag::now();
            double timeout = 1e9;

            if (!nonBlocking || now >= next_timeout_) {
                settings_lock_.lock();
                auto settings = ping_settings_;
                settings_lock_.unlock();

                std::vector<AooId> client_timeouts;

                for (auto& [id, client] : clients_) {
                    auto [timeout, wait] = client.update(*this, now, settings);
                    if (timeout) {
                        LOG_VERBOSE("AooServer: client " << id << " not responding");
                        client_timeouts.push_back(id);
                    } else if (wait < timeout) {
                        timeout = wait;
                    }
                }

                // remove timed-out clients
                for (auto& id : client_timeouts) {
                    remove_client(id, kAooErrorNotResponding, "client is not responding");
                }
            }

            // check and dispatch messages
            message_queue_.consume_all([this](const auto& msg) {
                dispatch_message(msg);
            });

            // finally wait for network events (with timeout)
            // NB: the TCP handler methods will lock the mutex to
            // prevent concurrent access from API methods.
            // The run() method itself must only be called after
            // the setup() method, so there is no race condition
            // regarding the TCP server itself.
            if (nonBlocking) {
                return tcp_server_.run(0) ? kAooOk : kAooErrorWouldBlock;
            } else {
                tcp_server_.run(timeout);
            }
        }

        // NB: in non-blocking mode, close() will be called in setup()!
        if (!nonBlocking) {
            close();
        }

        return kAooOk;
    }  catch (const aoo::tcp_error& e) {
        LOG_ERROR("AooServer: TCP server failed: " << e.what());
        aoo::socket_set_errno(e.code());

        return kAooErrorSocket;
    }
}

AOO_API AooError AOO_CALL AooServer_receiveUDP(
    AooServer *server, AooBool nonBlocking)
{
    return server->receiveUDP(nonBlocking);
}

AooError AOO_CALL aoo::net::Server::receiveUDP(AooBool nonBlocking) {
    try {
        if (nonBlocking) {
            return udp_server_.run(0) ? kAooOk : kAooErrorWouldBlock;
        } else {
            udp_server_.run();
            return kAooOk;
        }
    } catch (const aoo::udp_error& e) {
        LOG_ERROR("AooServer: UDP server error: " << e.what());
        socket_set_errno(e.code());
        return kAooErrorSocket;
    }
}

AOO_API AooError AOO_CALL Server_handlePacket(
    AooServer *server, const AooByte *data, AooInt32 size,
    const void *address, AooAddrSize addrlen)
{
    return server->handlePacket(data, size, address, addrlen);
}

AooError AOO_CALL aoo::net::Server::handlePacket(
    const AooByte *data, AooInt32 size,
    const void *address, AooAddrSize addrlen)
{
    aoo::ip_address addr((struct sockaddr *)address, addrlen);
    handle_udp_packet(data, size, addr);
    return kAooOk;
}

AOO_API AooError AOO_CALL AooServer_quit(AooServer *server)
{
    return server->quit();
}

AooError AOO_CALL aoo::net::Server::quit() {
    tcp_server_.stop();
    udp_server_.stop();
    return kAooOk;
}

AOO_API AooError AOO_CALL AooServer_setEventHandler(
    AooServer *server, AooEventHandler fn, void *user, AooEventMode mode) {
    return server->setEventHandler(fn, user, mode);
}

AooError AOO_CALL aoo::net::Server::setEventHandler(
    AooEventHandler fn, void *user, AooEventMode mode) {
    eventhandler_ = fn;
    eventcontext_ = user;
    eventmode_ = mode;
    return kAooOk;
}

AOO_API AooBool AOO_CALL AooServer_eventsAvailable(AooServer *server) {
    return server->eventsAvailable();
}

AooBool AOO_CALL aoo::net::Server::eventsAvailable(){
    return !events_.empty();
}

AOO_API AooError AOO_CALL AooServer_pollEvents(AooServer *server) {
    return server->pollEvents();
}

AooError AOO_CALL aoo::net::Server::pollEvents(){
    // always thread-safe
    event_handler fn(eventhandler_, eventcontext_, kAooThreadLevelUnknown);
    event_ptr e;
    while (events_.try_pop(e)){
        e->dispatch(fn);
    }
    return kAooOk;
}

AOO_API AooError AOO_CALL AooServer_setRequestHandler(
        AooServer *server, AooRequestHandler cb, void *user, AooFlag flags) {
    return server->setRequestHandler(cb, user, flags);
}

AooError AOO_CALL aoo::net::Server::setRequestHandler(
        AooRequestHandler cb, void *user, AooFlag flags) {
    request_handler_ = cb;
    request_context_ = user;
    return kAooOk;
}

AOO_API AooError AooServer_handleRequest(
        AooServer *server, AooId client, AooId token, const AooRequest *request,
        AooError result, AooResponse *response)
{
    return server->handleRequest(client, token, request, result, response);
}

// NB: might be called outside the TCP event loop (asynchronous request handling)
AooError AOO_CALL aoo::net::Server::handleRequest(
        AooId client, AooId token, const AooRequest *request,
        AooError result, AooResponse *response)
{
    if (!request) {
        return kAooErrorBadArgument;
    }

    sync::scoped_lock lock(mutex_); // writer lock

    auto c = find_client(client);
    if (!c) {
        return kAooErrorNotFound;
    }

    if (result == kAooErrorNone) {
        // request accepted

        // every request needs a response
        if (!response) {
            return kAooErrorBadArgument;
        }
        // just make sure that the response matches the request
        if (response->type != request->type) {
            return kAooErrorBadArgument;
        }

        switch (request->type) {
        case kAooRequestLogin:
            return do_login(*c, token, request->login, response->login);
        case kAooRequestGroupJoin:
            return do_group_join(*c, token, request->groupJoin, response->groupJoin);
        case kAooRequestGroupLeave:
            return do_group_leave(*c, token, request->groupLeave, response->groupLeave);
        case kAooRequestGroupUpdate:
            return do_group_update(*c, token, request->groupUpdate, response->groupUpdate);
        case kAooRequestUserUpdate:
            return do_user_update(*c, token, request->userUpdate, response->userUpdate);
        case kAooRequestCustom:
            return do_custom_request(*c, token, request->custom, response->custom);
        default:
            return kAooErrorNotImplemented;
        }
    } else {
        // request denied
        // response must be either AooRequestError or NULL
        if (response && response->type != kAooRequestError) {
            return kAooErrorBadArgument;
        }
        c->send_error(*this, token, request->type, result,
                      (const AooResponseError *)response);
    #if 1
        if (request->type == kAooRequestLogin) {
            // send event
            auto e = std::make_unique<client_login_event>(*c, result);
            send_event(std::move(e));
        }
    #endif

        return kAooOk;
    }
}

AOO_API AooError AOO_CALL AooServer_notifyClient(
        AooServer *server, AooId client, const AooData *data) {
    return server->notifyClient(client, *data);
}

AooError AOO_CALL aoo::net::Server::notifyClient(
        AooId client, const AooData &data) {
    sync::scoped_shared_lock lock(mutex_); // reader lock

    if (find_client(client)) {
        push_message(kAooIdInvalid, client, data);
        return kAooOk;
    } else {
        LOG_ERROR("AooServer: notifyClient: can't find client!");
        return kAooErrorNotFound;
    }
}

AOO_API AooError AOO_CALL AooServer_notifyGroup(
        AooServer *server, AooId group, AooId user, const AooData *data) {
    return server->notifyGroup(group, user, *data);
}

AooError AOO_CALL aoo::net::Server::notifyGroup(
        AooId group, AooId user, const AooData &data) {
    sync::scoped_shared_lock lock(mutex_); // reader lock

    if (auto g = find_group(group)) {
        if (user != kAooIdInvalid && !g->find_user(user)) {
            LOG_ERROR("AooServer: notifyGroup: can't find user "
                      << user << " in group " << group);
            return kAooErrorNotFound;
        }

        push_message(group, user, data);

        return kAooOk;
    } else {
        LOG_ERROR("AooServer: notifyGroup: can't find group " << group);
        return kAooErrorNotFound;
    }
}

AOO_API AooError AOO_CALL AooServer_findGroup(
        AooServer *server, const AooChar *name, AooId *id) {
    return server->findGroup(name, id);
}

AooError AOO_CALL aoo::net::Server::findGroup(
        const AooChar *name, AooId *id) {
    sync::scoped_shared_lock lock(mutex_); // reader lock

    if (auto grp = find_group(name)) {
        if (id) {
            *id = grp->id();
        }
        return kAooOk;
    } else {
        return kAooErrorNotFound;
    }
}

AOO_API AooError AOO_CALL AooServer_addGroup(
        AooServer *server, const AooChar *name, const AooChar *password,
        const AooData *metadata, const AooIpEndpoint *relayAddress, AooFlag flags, AooId *groupId) {
    return server->addGroup(name, password, metadata, relayAddress, flags, groupId);
}

AooError AOO_CALL aoo::net::Server::addGroup(
        const AooChar *name, const AooChar *password, const AooData *metadata,
        const AooIpEndpoint *relayAddress, AooFlag flags, AooId *groupId) {
    sync::scoped_lock lock(mutex_); // writer lock

    // this might "waste" a group ID, but we don't care.
    auto id = get_next_group_id();
    std::string hashed_pwd = password ? aoo::net::encrypt(password) : "";
    auto relay_addr = relayAddress ? ip_host(*relayAddress) : ip_host{};
    auto grp = group(name, hashed_pwd, id, metadata, relay_addr, kAooGroupPersistent);
    if (add_group(std::move(grp))) {
        if (groupId) {
            *groupId = id;
        }
        return kAooOk;
    } else {
        LOG_ERROR("AooServer: addGroup: group " << name << " already exists");
        return kAooErrorAlreadyExists;
    }
}

AOO_API AooError AOO_CALL AooServer_removeGroup(
        AooServer *server, AooId group) {
    return server->removeGroup(group);
}

AooError AOO_CALL aoo::net::Server::removeGroup(AooId group) {
    sync::scoped_lock lock(mutex_); // writer lock

    if (remove_group(group)) {
        return kAooOk;
    } else {
        LOG_ERROR("AooServer: removeGroup: group " << group << " not found");
        return kAooErrorNotFound;
    }
}

AOO_API AooError AOO_CALL AooServer_findUserInGroup(
        AooServer *server, AooId group, const AooChar *userName, AooId *userId) {
    return server->findUserInGroup(group, userName, userId);
}

AooError AOO_CALL aoo::net::Server::findUserInGroup(
        AooId group, const AooChar *userName, AooId *userId) {
    sync::scoped_shared_lock lock(mutex_); // reader lock

    if (auto grp = find_group(group)) {
        if (auto usr = grp->find_user(userName)) {
            if (userId) {
                *userId = usr->id();
            }
            return kAooOk;
        }
    }
    return kAooErrorNotFound;
}

AOO_API AooError AOO_CALL AooServer_addUserToGroup(
        AooServer *server, AooId group,
        const AooChar *userName, const AooChar *userPwd,
        const AooData *metadata, AooFlag flags, AooId *userId) {
    return server->addUserToGroup(group, userName, userPwd, metadata, flags, userId);
}

AooError AOO_CALL aoo::net::Server::addUserToGroup(
        AooId group, const AooChar *userName, const AooChar *userPwd,
        const AooData *metadata, AooFlag flags, AooId *userId) {
    sync::scoped_lock lock(mutex_); // writer lock

    if (auto g = find_group(group)) {
        auto id = g->get_next_user_id();
        std::string hashed_pwd = userPwd ? aoo::net::encrypt(userPwd) : "";
        auto usr = user(userName, hashed_pwd, id, g->id(), kAooIdInvalid,
                        metadata, ip_host{}, kAooUserPersistent);
        if (g->add_user(std::move(usr))) {
            if (userId) {
                *userId = id;
            }
            return kAooOk;
        } else {
            LOG_ERROR("AooServer: addUserToGroup: user " << userName
                      << " already exists in group " << group);
            return kAooErrorAlreadyExists;
        }
    } else {
        LOG_ERROR("AooServer: addUserToGroup: group " << group << " not found");
        return kAooErrorNotFound;
    }
}

AOO_API AooError AOO_CALL AooServer_removeUserFromGroup(
        AooServer *server, AooId group, AooId user) {
    return server->removeUserFromGroup(group, user);
}

AooError AOO_CALL aoo::net::Server::removeUserFromGroup(
        AooId group, AooId user) {
    sync::scoped_lock lock(mutex_); // writer lock

    if (auto grp = find_group(group)) {
        if (auto usr = grp->find_user(user)) {
            if (usr->active()) {
                // tell client that it has been kicked out of the group
                if (auto client = find_client(*usr)) {
                    client->on_group_leave(*this, *grp, *usr, true);
                } else {
                    LOG_ERROR("AooServer: removeUserFromGroup: can't find client for user " << *usr);
                }
                // notify peers
                on_user_left_group(*grp, *usr);
            }
            do_remove_user_from_group(*grp, *usr);
            return kAooOk;
        } else {
            LOG_ERROR("AooServer: removeUserToGroup: user "
                      << user << " not found  in group " << group);
        }
    } else {
        LOG_ERROR("AooServer: removeUserToGroup: group " << group << " not found");
    }
    return kAooErrorNotFound;
}

AOO_API AooError AOO_CALL AooServer_groupControl(
        AooServer *server, AooId group, AooCtl ctl,
        AooIntPtr index, void *data, AooSize size) {
    return server->groupControl(group, ctl, index, data, size);
}

template<typename T>
T& as(void *p){
    return *reinterpret_cast<T *>(p);
}

#define CHECKARG(type) assert(size == sizeof(type))

AooError AOO_CALL aoo::net::Server ::groupControl(
        AooId group, AooCtl ctl, AooIntPtr index,
        void *ptr, AooSize size) {
    // for simplicity, always take a writer lock.
    // LATER separate between read and write options
    sync::scoped_lock lock(mutex_);

    auto grp = find_group(group);
    if (!grp) {
        LOG_ERROR("AooServer: could not find group " << group);
        return kAooErrorNotFound;
    }

    switch (ctl) {
    case kAooCtlUpdateGroup:
    {
        CHECKARG(AooData*);
        auto md = as<const AooData*>(ptr);
        if (md) {
            update_group(*grp, *md);
        } else {
            return kAooErrorBadArgument;
        }
        break;
    }
    case kAooCtlUpdateUser:
    {
        auto usr = grp->find_user(index);
        if (!usr) {
            LOG_ERROR("AooServer: could not find user "
                      << index << " in group " << group);
            return kAooErrorNotFound;
        }
        CHECKARG(AooData*);
        auto md = as<const AooData*>(ptr);
        if (md) {
            update_user(*grp, *usr, *md);
        } else {
            return kAooErrorBadArgument;
        }
        break;
    }
    default:
        LOG_WARNING("AooServer: unsupported group control " << ctl);
        return kAooErrorNotImplemented;
    }

    return kAooOk;
}

AOO_API AooError AOO_CALL AooServer_control(
        AooServer *server, AooCtl ctl,
        AooIntPtr index, void *data, AooSize size)
{
    return server->control(ctl, index, data, size);
}

AooError AOO_CALL aoo::net::Server::control(
        AooCtl ctl, AooIntPtr index, void *ptr, AooSize size)
{
    switch (ctl) {
    case kAooCtlSetPassword:
    {
        sync::scoped_lock lock(mutex_); // writer lock

        CHECKARG(AooChar *);
        auto pwd = as<AooChar*>(ptr);
        if (pwd) {
            password_ = encrypt(pwd);
        } else {
            password_ = "";
        }
        break;
    }
    case kAooCtlSetRelayHost:
    {
        sync::scoped_lock lock(mutex_); // writer lock

        CHECKARG(AooIpEndpoint*);
        auto ep = as<AooIpEndpoint*>(ptr);
        if (ep) {
            relay_addr_ = ip_host(*ep);
        } else {
            relay_addr_ = ip_host{};
        }
        break;
    }
    case kAooCtlSetServerRelay:
        CHECKARG(AooBool);
        allow_relay_.store(as<AooBool>(ptr));
        break;
    case kAooCtlGetServerRelay:
        CHECKARG(AooBool);
        as<AooBool>(ptr) = allow_relay_.load();
        break;
    case kAooCtlSetGroupAutoCreate:
        CHECKARG(AooBool);
        group_auto_create_.store(as<AooBool>(ptr));
        break;
    case kAooCtlGetGroupAutoCreate:
        CHECKARG(AooBool);
        as<AooBool>(ptr) = group_auto_create_.load();
        break;
    case kAooCtlSetPingSettings:
        CHECKARG(AooPingSettings);
        settings_lock_.lock();
        ping_settings_ = as<AooPingSettings>(ptr);
        settings_lock_.unlock();
        break;
    case kAooCtlGetPingSettings:
        CHECKARG(AooPingSettings);
        settings_lock_.lock();
        as<AooPingSettings>(ptr) = ping_settings_;
        settings_lock_.unlock();
        break;
    default:
        LOG_WARNING("AooServer: unsupported control " << ctl);
        return kAooErrorNotImplemented;
    }
    return kAooOk;
}

namespace aoo {
namespace net {

client_endpoint * Server::find_client(AooId id) {
    auto it = clients_.find(id);
    if (it != clients_.end()) {
        return &it->second;
    } else {
        return nullptr;
    }
}

client_endpoint * Server::find_client(const ip_address& addr) {
    for (auto& client : clients_) {
        if (client.second.match(addr)) {
            return &client.second;
        }
    }
    return nullptr;
}

group* Server::add_group(group&& grp) {
    if (!find_group(grp.name())) {
        auto result = groups_.emplace(grp.id(), std::move(grp));
        if (result.second) {
            return &result.first->second;
        }
    }
    return nullptr;
}

group* Server::find_group(AooId id) {
    auto it = groups_.find(id);
    if (it != groups_.end()) {
        return &it->second;
    } else {
        return nullptr;
    }
}

group* Server::find_group(const std::string& name) {
    for (auto& grp : groups_) {
        if (grp.second.name() == name) {
            return &grp.second;
        }
    }
    return nullptr;
}

bool Server::remove_group(AooId id) {
    auto it = groups_.find(id);
    if (it == groups_.end()) {
        return false;
    }
    // if the group has been removed manually, it might still
    // contain users, so we have to notify them!
    auto& grp = it->second;
    for (auto& usr : grp.users()) {
        if (auto client = find_client(usr)) {
            client->on_group_leave(*this, grp, usr, true);
        } else {
            LOG_ERROR("AooServer: remove_group: can't find client for user " << usr);
        }
    }
    groups_.erase(it);
    return true;
}

void Server::update_group(group& grp, const AooData& md) {
    LOG_DEBUG("AooServer: update group " << grp);

    grp.set_metadata(md);

    for (auto& usr : grp.users()) {
        auto client = find_client(usr.client());
        if (client) {
            // kAooIdInvalid -> updated on the server
            client->send_group_update(*this, grp, kAooIdInvalid);
        } else {
            LOG_ERROR("AooServer: could not find client for user " << usr);
        }
    }
}

void Server::update_user(const group& grp, user& usr, const AooData& md) {
    LOG_DEBUG("AooServer: update group " << grp);

    usr.set_metadata(md);

    for (auto& member : grp.users()) {
        auto client = find_client(member.client());
        if (client) {
            if (member.id() == usr.id()) {
                client->send_user_update(*this, member);
            } else {
                client->send_peer_update(*this, member);
            }
        } else {
            LOG_ERROR("AooServer: could not find client for user " << usr);
        }
    }
}

void Server::on_user_joined_group(const group& grp, const user& usr,
                                  const client_endpoint& client) {
    LOG_DEBUG("AooServer: user " << usr << " joined group " << grp);
    // 1) send the new member to existing group members
    // 2) send existing group members to the new member
    for (auto& peer : grp.users()) {
        if ((peer.id() != usr.id()) && peer.active()) {
            if (auto other = find_client(peer)) {
                // notify new member
                client.send_peer_join(*this, grp, peer, *other);
                // notify existing member
                other->send_peer_join(*this, grp, usr, client);
            } else {
                LOG_ERROR("AooServer: user_joined_group: can't find client for peer " << peer);
            }
        }
    }

    // send event
    auto e = std::make_unique<group_join_event>(grp, usr);
    send_event(std::move(e));
}

void Server::on_user_left_group(const group& grp, const user& usr) {
    LOG_DEBUG("AooServer: user " << usr << " left group " << grp);
    // notify peers
    for (auto& peer : grp.users()) {
        if (peer.id() != usr.id()) {
            if (auto other = find_client(peer)) {
                other->send_peer_leave(*this, grp, usr);
            } else {
                LOG_ERROR("AooServer: user_left_group: can't find client for peer " << peer);
            }
        }
    }

    // send event
    auto e = std::make_unique<group_leave_event>(grp, usr);
    send_event(std::move(e));
}

void Server::do_remove_user_from_group(group& grp, user& usr) {
    if (usr.persistent()) {
        // just unset
        usr.unset();
    } else {
        // remove from group
        if (!grp.remove_user(usr.id())) {
            LOG_ERROR("AooServer: can't remove user " << usr << " from group " << grp);
        }
        // remove group if empty and not persistent
        if (!grp.persistent() && !grp.user_count()) {
            // send event
            auto e = std::make_unique<group_remove_event>(grp);
            send_event(std::move(e));

            // finally remove it
            remove_group(grp.id());
        }
    }
}

osc::OutboundPacketStream Server::start_message(size_t extra_size) {
    if (extra_size > 0) {
        auto total = AOO_MAX_PACKET_SIZE + extra_size;
        if (sendbuffer_.size() < total) {
            sendbuffer_.resize(total);
        }
    }
    // leave space for message size (int32_t)
    return osc::OutboundPacketStream(sendbuffer_.data() + 4, sendbuffer_.size() - 4);
}

AooId Server::accept_client(const aoo::ip_address& addr, aoo::tcp_server::reply_func fn) {
    sync::scoped_lock lock(mutex_); // writer lock; see run() method

    auto id = get_next_client_id();
    // TODO: check max. client count
    clients_.emplace(id, client_endpoint(id, fn));
    // force timer update! See run().
    next_timeout_.clear();

    LOG_DEBUG("AooServer: add client " << id);
    return id;
}

bool Server::remove_client(AooId id, AooError err, const std::string& msg) {
    auto it = clients_.find(id);
    if (it == clients_.end()) {
        LOG_ERROR("AooServer: removeClient: client " << id << " not found");
        return false;
    }
    // remove from group(s) and send notifications
    it->second.on_close(*this);

    if (it->second.active()) {
        // only send event if logged in!
        auto e = std::make_unique<client_logout_event>(id, err, msg);
        send_event(std::move(e));
    }

    clients_.erase(it);

    LOG_DEBUG("AooServer: removed client " << id);

    return true;
}

// called from TCP server
void Server::handle_client_data(AooId id, int err, const AooByte *data,
                                AooInt32 size, const aoo::ip_address& addr) {
    sync::scoped_lock lock(mutex_); // writer lock; see run() method

    auto client = find_client(id);
    if (!client) {
        LOG_ERROR("AooServer: handle_client_data: can't find client " << id);
        return;
    }
    if (size > 0) {
        try {
            client->handle_data(*this, data, size);
        } catch (const error& e) {
            LOG_ERROR("AooServer: could not handle client message: " << e.what());
            remove_client(id, e.code(), e.what());
        } catch (const osc::Exception& e) {
            LOG_ERROR("AooServer: malformed client message: " << e.what());
            remove_client(id, kAooErrorBadFormat, e.what());
        }
    } else if (err == 0) {
        // disconnected
        remove_client(id, kAooOk);
    } else {
        // socket error
        remove_client(id, kAooErrorSocket, socket_strerror(err));
    }
}

void Server::handle_ping(client_endpoint& client,
                         const osc::ReceivedMessage& msg) {
    // send reply
    auto reply = start_message();

    reply << osc::BeginMessage(kAooMsgClientPong) << osc::EndMessage;

    client.send_message(reply);
}

void Server::handle_pong(client_endpoint &client,
                         const osc::ReceivedMessage &msg) {
    client.handle_pong();
}

void Server::handle_message(client_endpoint& client,
                            const osc::ReceivedMessage& msg, int32_t size) {
    AooMsgType type;
    int32_t onset;
    auto err = parse_pattern((const AooByte *)msg.AddressPattern(), size, type, onset);
    if (err != kAooOk){
        throw error(kAooErrorBadFormat, "not an AOO message!");
    }

    try {
        if (type == kAooMsgTypeServer){
            auto pattern = msg.AddressPattern() + onset;
            LOG_DEBUG("AooServer: got server message " << pattern);
            if (!strcmp(pattern, kAooMsgLogin)){
                handle_login(client, msg);
            } else {
                // all other messages must be received after login!
                if (!client.active()) {
                    throw error(kAooErrorNotPermitted, "not logged in");
                }
                if (!strcmp(pattern, kAooMsgPing)){
                    handle_ping(client, msg);
                } else if (!strcmp(pattern, kAooMsgPong)) {
                    handle_pong(client, msg);
                } else if (!strcmp(pattern, kAooMsgGroupJoin)){
                    handle_group_join(client, msg);
                } else if (!strcmp(pattern, kAooMsgGroupLeave)){
                    handle_group_leave(client, msg);
                } else if (!strcmp(pattern, kAooMsgGroupUpdate)){
                    handle_group_update(client, msg);
                } else if (!strcmp(pattern, kAooMsgUserUpdate)){
                    handle_user_update(client, msg);
                } else if (!strcmp(pattern, kAooMsgRequest)){
                    handle_custom_request(client, msg);
                } else {
                    // NB: the client is supposed to check the server version
                    // and only send supported messages.
                    std::stringstream ss;
                    ss << "unknown server message " << pattern;
                    throw error(kAooErrorNotImplemented, ss.str());
                }
            }
        } else {
            std::stringstream ss;
            ss << "unexpected message " << msg.AddressPattern();
            throw error(kAooErrorBadFormat, ss.str());
        }
    } catch (const osc::WrongArgumentTypeException& e) {
        std::stringstream ss;
        ss << "wrong argument(s) for " << msg.AddressPattern() << " message";
        throw error(kAooErrorBadArgument, ss.str());
    } catch (const osc::MissingArgumentException& e) {
        std::stringstream ss;
        ss << "missing argument(s) for " << msg.AddressPattern() << " message";
        throw error(kAooErrorBadArgument, ss.str());
    } catch (const osc::Exception& e) {
        std::stringstream ss;
        ss << "malformed " << msg.AddressPattern() << " message: " << e.what();
        throw error(kAooErrorBadFormat, ss.str());
    }
}

//------------------------- login ------------------------------//

void Server::handle_login(client_endpoint& client, const osc::ReceivedMessage& msg)
{
    auto it = msg.ArgumentsBegin();
    auto token = (AooId)(it++)->AsInt32();
    auto version = (it++)->AsString();
    auto pwd = (it++)->AsString();
    auto metadata = osc_read_metadata(it); // optional
    // collect IP addresses
    auto addrcount = (it++)->AsInt32();
    for (int32_t i = 0; i < addrcount; ++i) {
        client.add_public_address(osc_read_address(it));
    }

    AooRequestLogin request;
    AOO_REQUEST_INIT(&request, Login, metadata);
    request.version = version;
    request.password = pwd;
    request.metadata = metadata ? &metadata.value() : nullptr;
    // check version
    if (auto err = check_version(version); err != kAooOk) {
        LOG_DEBUG("AooServer: client " << client.id() << ": version mismatch");
        client.send_error(*this, token, request.type, err);
        // send event
        auto e = std::make_unique<client_login_event>(client, err);
        send_event(std::move(e));
        return;
    }
    // check password
    if (!password_.empty() && pwd != password_) {
        LOG_DEBUG("AooServer: client " << client.id() << ": wrong password");
        client.send_error(*this, token, request.type, kAooErrorWrongPassword);
        // send event
        auto e = std::make_unique<client_login_event>(client, kAooErrorWrongPassword);
        send_event(std::move(e));
        return;
    }

    if (!handle_request(client, token, (AooRequest&)request)) {
        AooResponseLogin response;
        AOO_RESPONSE_INIT(&response, Login, metadata);
        response.metadata = nullptr;

        do_login(client, token, request, response);
    }
}

AooError Server::do_login(client_endpoint& client, AooId token,
                          const AooRequestLogin& request,
                          AooResponseLogin& response) {
    // flags
    AooFlag flags = 0;
    if (allow_relay_.load()) {
        flags |= kAooServerRelay;
    }

    client.activate(request.version);

    // send reply
    auto extra = response.metadata ? response.metadata->size : 0;
    auto msg = start_message(extra);

    msg << osc::BeginMessage(kAooMsgClientLogin)
        << token << kAooErrorNone
        << aoo_getVersionString() << client.id()
        << (int32_t)flags << metadata_view(response.metadata)
        << osc::EndMessage;

    client.send_message(msg);

    auto e = std::make_unique<client_login_event>(client, kAooOk, aoo::metadata(request.metadata));
    send_event(std::move(e));

    return kAooOk;
}

//----------------------- group_join ---------------------------//

void Server::handle_group_join(client_endpoint& client, const osc::ReceivedMessage& msg)
{
    auto it = msg.ArgumentsBegin();
    auto token = (AooId)(it++)->AsInt32();
    auto group_name = (it++)->AsString();
    auto group_pwd = (it++)->AsString();
    auto group_md = osc_read_metadata(it); // optional
    auto user_name = (it++)->AsString();
    auto user_pwd = (it++)->AsString();
    auto user_md = osc_read_metadata(it); // optional
    auto relay = osc_read_host(it); // optional

    AooRequestGroupJoin request;
    AOO_REQUEST_INIT(&request, GroupJoin, relayAddress);
    request.groupName = group_name;
    request.groupPwd = group_pwd;
    request.groupId = kAooIdInvalid; // to be created (override later)
    request.groupMetadata = group_md ? &group_md.value() : nullptr;
    request.userName = user_name;
    request.userPwd = user_pwd;
    request.userId = kAooIdInvalid; // to be created (override later)
    request.userMetadata = user_md ? &user_md.value() : nullptr;
    request.relayAddress = relay ? &relay.value() : nullptr;

    auto grp = find_group(request.groupName);
    user *usr = nullptr;
    if (grp) {
        request.groupId = grp->id();
        // check group password
        if (!grp->check_pwd(request.groupPwd)) {
            client.send_error(*this, token, request.type, kAooErrorWrongPassword);
            return;
        }
        usr = grp->find_user(request.userName);
        if (usr) {
            request.userId = usr->id();
            // check if someone is already logged in
            if (usr->active()) {
                client.send_error(*this, token, request.type, kAooErrorUserAlreadyExists);
                return;
            }
            // check user password
            if (!usr->check_pwd(request.userPwd)) {
                client.send_error(*this, token, request.type, kAooErrorWrongPassword);
                return;
            }
        } else {
            // user needs to be created
            // check if the client is allowed to create users
            if (!grp->user_auto_create()) {
                client.send_error(*this, token, request.type, kAooErrorCannotCreateUser);
                return;
            }
        }
    } else {
        // group needs to be created
        // check if the client is allowed to create groups
        if (!group_auto_create_.load()) {
            client.send_error(*this, token, request.type, kAooErrorCannotCreateGroup);
            return;
        }
    }

    if (!handle_request(client, token, (AooRequest&)request)) {
        AooResponseGroupJoin response;
        AOO_RESPONSE_INIT(&response, GroupJoin, relayAddress);
        response.groupId = 0;
        response.groupFlags = 0; // not used
        response.groupMetadata = nullptr;
        response.userId = 0;
        response.userFlags = 0; // not used
        response.userMetadata = nullptr;
        response.privateMetadata = nullptr;
        response.relayAddress = nullptr;

        do_group_join(client, token, request, response);
    }
}

AooError Server::do_group_join(client_endpoint &client, AooId token,
                               const AooRequestGroupJoin& request,
                               AooResponseGroupJoin& response) {
    bool did_create_group = false;
    // find/create group
    auto grp = find_group(request.groupId);
    if (!grp) {
        // prefer response group metadata
        auto group_md = response.groupMetadata ? response.groupMetadata : request.groupMetadata;
        // (optional) group relay address
        auto group_relay = response.relayAddress ? ip_host(*response.relayAddress) : ip_host{};

        grp = add_group(group(request.groupName, request.groupPwd, get_next_group_id(),
                              group_md, group_relay, 0));
        if (grp) {
            // send event
            auto e = std::make_unique<group_add_event>(*grp);
            send_event(std::move(e));
        } else {
            // group has been added in the meantime... LATER try to deal with this
            client.send_error(*this, token, request.type, kAooErrorCannotCreateGroup);
            return kAooErrorCannotCreateGroup;
        }
        did_create_group = true;
    }

    // find user or create it if necessary
    auto usr = grp->find_user(request.userId);
    if (!usr) {
        // prefer response user metadata
        auto user_md = response.userMetadata ? response.userMetadata : request.userMetadata;
        // (optional) user provided relay address
        auto user_relay = request.relayAddress ? ip_host(*request.relayAddress) : ip_host{};

        auto id = grp->get_next_user_id();
        AooFlag flags = did_create_group ? kAooUserGroupCreator : 0;
        usr = grp->add_user(user(request.userName, request.userPwd, id, grp->id(),
                                 client.id(), user_md, user_relay, flags));
        if (!usr) {
            // user has been added in the meantime... LATER try to deal with this
            client.send_error(*this, token, request.type, kAooErrorCannotCreateUser);
            return kAooErrorCannotCreateUser;
        }
    }

    // update response, so we may inspect it after calling handlingRequest()!
    response.groupId = grp->id();
    response.groupFlags = grp->flags();
    response.userId = usr->id();
    response.userFlags = usr->flags();

    client.on_group_join(*this, *grp, *usr);

    // prefer group relay over global relay address; both may be empty!
    auto relay_addr = grp->relay_addr().valid() ? grp->relay_addr() : relay_addr_;

    // send reply
    auto extra = grp->metadata().size() + usr->metadata().size() +
            (response.privateMetadata ? response.privateMetadata->size : 0);
    auto msg = start_message(extra);

    msg << osc::BeginMessage(kAooMsgClientGroupJoin)
        << token << kAooErrorNone
        << grp->id() << (int32_t)grp->flags() << grp->metadata()
        << usr->id() << (int32_t)usr->flags() << usr->metadata()
        << metadata_view(response.privateMetadata)
        << relay_addr
        << osc::EndMessage;

    client.send_message(msg);

    // after reply!
    on_user_joined_group(*grp, *usr, client);

    return kAooOk; // success
}

//---------------------- group_leave -------------------------//

void Server::handle_group_leave(client_endpoint& client, const osc::ReceivedMessage& msg){
    auto it = msg.ArgumentsBegin();
    auto token = (AooId)(it++)->AsInt32();
    auto group = (it++)->AsInt32();

    AooRequestGroupLeave request;
    AOO_REQUEST_INIT(&request, GroupLeave, group);
    request.group = group;

    AooResponseGroupLeave response;
    AOO_RESPONSE_INIT(&response, GroupLeave, structSize);

    if (!handle_request(client, token, (AooRequest&)request)) {
        do_group_leave(client, token, request, response);
    }
}

AooError Server::do_group_leave(client_endpoint& client, AooId token,
                                const AooRequestGroupLeave& request,
                                AooResponseGroupLeave& response) {
    AooError result = kAooErrorNone;

    if (auto grp = find_group(request.group)) {
        // find the user in the group that is associated with this client
        // LATER we might move this logic to the client side and send the
        // user ID to the server.
        if (auto usr = grp->find_user(client)) {
            on_user_left_group(*grp, *usr);

            client.on_group_leave(*this, *grp, *usr, false);

            do_remove_user_from_group(*grp, *usr);

            // send reply
            auto msg = start_message();

            msg << osc::BeginMessage(kAooMsgClientGroupLeave)
                << token << kAooErrorNone
                << osc::EndMessage;

            client.send_message(msg);

            return kAooOk;
        } else {
            result = kAooErrorNotGroupMember;
        }
    } else {
        result = kAooErrorGroupDoesNotExist;
    }
    client.send_error(*this, token, request.type, result);

    return result;
}

//----------------------- group_update --------------------------//

void Server::handle_group_update(client_endpoint& client, const osc::ReceivedMessage& msg)
{
    auto it = msg.ArgumentsBegin();
    auto token = (AooId)(it++)->AsInt32();
    auto group_id = (it++)->AsInt32();
    auto md = osc_read_metadata(it);
    if (!md) {
        throw osc::MalformedMessageException("missing data");
    }

    AooRequestGroupUpdate request;
    AOO_REQUEST_INIT(&request, GroupUpdate, groupMetadata);
    request.groupId = group_id;
    request.groupMetadata = *md;

    auto grp = find_group(request.groupId);
    if (!grp) {
        client.send_error(*this, token, request.type, kAooErrorGroupDoesNotExist);
        return;
    }
    // verify group membership (in addition to client-side check)
    if (!grp->find_user(client)) {
        client.send_error(*this, token, request.type, kAooErrorNotPermitted);
        return;
    }

    if (!handle_request(client, token, (AooRequest&)request)) {
        AooResponseGroupUpdate response;
        AOO_RESPONSE_INIT(&response, GroupUpdate, groupMetadata);
        response.groupMetadata = request.groupMetadata;

        do_group_update(client, token, request, response);
    }
}

AooError Server::do_group_update(client_endpoint &client, AooId token,
                                 const AooRequestGroupUpdate& request,
                                 AooResponseGroupUpdate& response) {
    auto grp = find_group(request.groupId);
    if (!grp) {
        LOG_ERROR("AooServer: could not find group");
        return kAooErrorNotFound;
    }

    auto usr = grp->find_user(client);
    if (!usr) {
        LOG_ERROR("AooServer: could not find user");
        return kAooErrorNotFound;
    }

    LOG_DEBUG("AooServer: group " << *grp << " updated by user " << *usr);

    grp->set_metadata(response.groupMetadata);

    // notify peers
    for (auto& p : grp->users()) {
        if (p.client() != client.id()) {
            if (auto c = find_client(p.client())) {
                c->send_group_update(*this, *grp, usr->id());
            } else {
                LOG_ERROR("AooServer: could not find client for user " << p);
            }
        }
    }

    // send reply
    auto msg = start_message(grp->metadata().size());

    msg << osc::BeginMessage(kAooMsgClientGroupUpdate)
        << token << kAooErrorNone << grp->metadata()
        << osc::EndMessage;

    client.send_message(msg);

    // send event
    auto e = std::make_unique<group_update_event>(*grp, *usr);
    send_event(std::move(e));

    return kAooOk; // success
}

//---------------------- user_update -------------------------//

void Server::handle_user_update(client_endpoint& client, const osc::ReceivedMessage& msg)
{
    auto it = msg.ArgumentsBegin();
    auto token = (AooId)(it++)->AsInt32();
    auto group_id = (it++)->AsInt32();
    auto md = osc_read_metadata(it);
    if (!md) {
        throw osc::MalformedMessageException("missing data");
    }

    AooRequestUserUpdate request;
    AOO_REQUEST_INIT(&request, UserUpdate, userMetadata);
    request.groupId = group_id;
    request.userMetadata = *md;

    auto grp = find_group(request.groupId);
    if (!grp) {
        client.send_error(*this, token, request.type, kAooErrorGroupDoesNotExist);
        return;
    }
    // verify group membership (in addition to client-side check)
    if (!grp->find_user(client)) {
        client.send_error(*this, token, request.type, kAooErrorNotPermitted);
        return;
    }

    if (!handle_request(client, token, (AooRequest&)request)) {
        AooResponseUserUpdate response;
        AOO_RESPONSE_INIT(&request, UserUpdate, userMetadata);
        response.userMetadata = request.userMetadata;

        do_user_update(client, token, request, response);
    }
}

AooError Server::do_user_update(client_endpoint &client, AooId token,
                                const AooRequestUserUpdate& request,
                                AooResponseUserUpdate& response) {
    auto grp = find_group(request.groupId);
    if (!grp) {
        LOG_ERROR("AooServer: could not find group");
        return kAooErrorNotFound;
    }

    auto usr = grp->find_user(client);
    if (!usr) {
        LOG_ERROR("AooServer: could not find user");
        return kAooErrorNotFound;
    }

    LOG_DEBUG("AooServer: update usr " << *usr);

    usr->set_metadata(response.userMetadata);

    // notify peers
    for (auto& member : grp->users()) {
        if (member.id() != usr->id()) {
            if (auto c = find_client(member.client())) {
                c->send_peer_update(*this, *usr);
            } else {
                LOG_ERROR("AooServer: could not find client for user " << member);
            }
        }
    }

    // send reply
    auto msg = start_message(usr->metadata().size());

    msg << osc::BeginMessage(kAooMsgClientUserUpdate)
        << token << kAooErrorNone << usr->metadata()
        << osc::EndMessage;

    client.send_message(msg);

    // send event
    auto e = std::make_unique<user_update_event>(*usr);
    send_event(std::move(e));

    return kAooOk; // success
}

//-------------------- custom_request ---------------------//

void Server::handle_custom_request(client_endpoint& client, const osc::ReceivedMessage& msg) {
    auto it = msg.ArgumentsBegin();
    auto token = (AooId)(it++)->AsInt32();
    auto flags = (it++)->AsInt32();
    auto data = osc_read_metadata(it);
    if (!data) {
        throw osc::MalformedMessageException("missing data");
    }

    AooRequestCustom request;
    AOO_REQUEST_INIT(&request, Custom, flags);
    request.data = *data;
    request.flags = flags;

    if (!handle_request(client, token, (AooRequest&)request)) {
        // requests must be handled by the user!
        client.send_error(*this, token, request.type, kAooErrorUnhandledRequest);
    }
}

AooError Server::do_custom_request(client_endpoint& client, AooId token,
                                   const AooRequestCustom& request,
                                   AooResponseCustom& response) {
    // send reply
    auto msg = start_message(response.data.size);

    msg << osc::BeginMessage(kAooMsgClientRequest)
        << token << kAooErrorNone << (int32_t)response.flags
        << metadata_view(&response.data) << osc::EndMessage;

    client.send_message(msg);

    return kAooOk;
}

//----------------------- UDP messages --------------------------//

void Server::handle_udp_packet(const AooByte *data, AooInt32 size,
                               const aoo::ip_address& addr) {
    AooMsgType type;
    int32_t onset;
    auto err = parse_pattern(data, size, type, onset);
    if (err != kAooOk){
        LOG_WARNING("AooServer: not an AOO NET message!");
        return;
    }

    if (type == kAooMsgTypeServer){
        handle_udp_message(data, size, onset, addr);
    } else if (type == kAooMsgTypeRelay){
        handle_relay(data, size, addr);
    } else {
        LOG_WARNING("AooServer: not a client message!");
    }
}

void Server::handle_udp_message(const AooByte *data, AooSize size, int onset,
                                const ip_address& addr) {
    if (binmsg_check(data, size)) {
        LOG_WARNING("AooServer: unsupported binary message");
        return;
    }
    // OSC format
    try {
        osc::ReceivedPacket packet((const char *)data, size);
        osc::ReceivedMessage msg(packet);

        auto pattern = msg.AddressPattern() + onset;
        LOG_DEBUG("AooServer: handle client UDP message " << pattern);

        if (!strcmp(pattern, kAooMsgPing)){
            handle_ping(msg, addr);
        } else if (!strcmp(pattern, kAooMsgQuery)) {
            handle_query(msg, addr);
        } else {
            LOG_ERROR("AooServer: unknown message " << pattern);
            return;
        }
    } catch (const osc::Exception& e){
        auto pattern = (const char *)data + onset;
        LOG_ERROR("AooServer: exception on handling " << pattern
                  << " message: " << e.what());
    }
}

void Server::handle_relay(const AooByte *data, AooSize size, const ip_address& addr) {
    if (!allow_relay_.load()) {
    #if AOO_DEBUG_RELAY
        LOG_DEBUG("AooServer: ignore relay message from " << addr);
    #endif
        return;
    }

    auto src_addr = addr.unmapped();

    auto check_addr = [&](ip_address& addr) {
        if (addr.is_ipv4_mapped()) {
            LOG_DEBUG("AooServer: relay destination must not be IPv4-mapped");
            return false;
        }
        if (address_family_ == ip_address::IPv6 && addr.type() == ip_address::IPv4) {
            if (use_ipv4_mapped_) {
                // map address to IPv4
                addr = addr.ipv4_mapped();
            } else {
                // cannot relay to IPv4 address with IPv6-only socket
                LOG_DEBUG("AooClient: cannot relay to destination address" << addr);
                return false;
            }
        } else if (address_family_ == ip_address::IPv4 && addr.type() == ip_address::IPv6) {
            // cannot relay to IPv6 address with IPv4-only socket
            LOG_DEBUG("AooClient: cannot relay to destination address" << addr);
            return false;
        }
        return true;
    };

    if (binmsg_check(data, size)) {
        // --- binary format ---
        ip_address dst_addr;
        auto onset = binmsg_read_relay(data, size, dst_addr);
        if (!check_addr(dst_addr)) {
            return;
        }
        if (onset > 0) {
        #if AOO_DEBUG_RELAY
            LOG_DEBUG("AooServer: forward binary relay message from " << addr << " to " << dst);
        #endif
            if (src_addr.type() == dst_addr.type()) {
                // simply replace the header (= rewrite address)
                binmsg_write_relay(const_cast<AooByte *>(data), size, src_addr);
                send_udp(dst_addr, data, size);
            } else {
                // rewrite whole message
                AooByte buf[AOO_MAX_PACKET_SIZE];
                auto result = write_relay_message(buf, sizeof(buf), data + onset,
                                                  size - onset, src_addr);
                if (result > 0) {
                    send_udp(dst_addr, buf, result + size);
                } else {
                    LOG_ERROR("AooServer: can't relay: buffer too small");
                }
            }
        } else {
            LOG_ERROR("AooServer: bad relay message");
        }
    } else {
        // --- OSC format ---
        try {
            osc::ReceivedPacket packet((const char *)data, size);
            osc::ReceivedMessage msg(packet);

            auto it = msg.ArgumentsBegin();
            auto dst_addr = osc_read_address(it);
            if (!check_addr(dst_addr)) {
                return;
            }

            const void *msgData;
            osc::osc_bundle_element_size_t msgSize;
            (it++)->AsBlob(msgData, msgSize);

            // don't prepend size for UDP message!
            char buf[AOO_MAX_PACKET_SIZE];
            osc::OutboundPacketStream out(buf, sizeof(buf));
            out << osc::BeginMessage(kAooMsgDomain kAooMsgRelay)
                << src_addr << osc::Blob(msgData, msgSize)
                << osc::EndMessage;

        #if AOO_DEBUG_RELAY
            LOG_DEBUG("AooServer: forward OSC relay message from " << addr << " to " << dst);
        #endif
            send_udp(dst_addr, (const AooByte *)out.Data(), out.Size());
        } catch (const osc::Exception& e){
            LOG_ERROR("AooServer: exception in handle_relay: " << e.what());
        }
    }
}

void Server::handle_ping(const osc::ReceivedMessage& msg, const ip_address& addr) {
    // reply with /pong message
    // NB: don't prepend size for UDP message!
    char buf[512];
    osc::OutboundPacketStream reply(buf, sizeof(buf));
    reply << osc::BeginMessage(kAooMsgClientPong)
          << osc::EndMessage;

    send_udp(addr, (const AooByte *)reply.Data(), reply.Size());
}

void Server::handle_query(const osc::ReceivedMessage& msg, const ip_address& addr) {
    // NB: do not prepend size for UDP message!
    char buf[AOO_MAX_PACKET_SIZE];
    osc::OutboundPacketStream reply(buf, sizeof(buf));
    reply << osc::BeginMessage(kAooMsgClientQuery)
          << addr.unmapped() // return unmapped(!) public IP
          << osc::EndMessage;

    send_udp(addr, (const AooByte *)reply.Data(), reply.Size());
}

AooId Server::get_next_client_id(){
    // LATER make random group ID
    return next_client_id_++;
}

AooId Server::get_next_group_id(){
    // LATER make random group ID
    return next_group_id_++;
}

void Server::push_message(AooId group, AooId user, const AooData& data) {
    message msg;
    msg.group = group;
    msg.user = user;
    msg.type = data.type;
    msg.data.assign(data.data, data.data + data.size);

    message_queue_.push(std::move(msg));

    tcp_server_.notify();
}

void Server::dispatch_message(const message &msg) {
    if (msg.group == kAooIdInvalid) {
        // client message
        if (auto c = find_client(msg.user)) {
            AooData data;
            data.type = msg.type;
            data.data = msg.data.data();
            data.size = msg.data.size();

            c->send_notification(*this, data);
        } else {
            LOG_WARNING("AooServer: cannot send message to client " << msg.user
                        << " because it does not exist anymore");
        }
    } else {
        // group/user message
        if (auto g = find_group(msg.group)) {
            AooData data;
            data.type = msg.type;
            data.data = msg.data.data();
            data.size = msg.data.size();

            if (msg.user == kAooIdInvalid) {
                // all users
                for (auto& u : g->users()) {
                    if (auto c = find_client(u)) {
                        c->send_notification(*this, data);
                    } else {
                        LOG_WARNING("AooServer: cannot send message to user "
                                    << msg.user << " in group " << msg.group
                                    << " because it does not exist anymore");
                    }
                }
            } else {
                // single user
                if (auto u = g->find_user(msg.user)) {
                    if (auto c = find_client(*u)) {
                        c->send_notification(*this, data);
                    } else {
                        LOG_ERROR("AooServer: cannot find client for user " << *u);
                    }
                } else {
                    LOG_WARNING("AooServer: cannot send message to user "
                                << msg.user << " in group " << msg.group
                                << " because it does not exist anymore");
                }
            }
        } else {
            LOG_WARNING("AooServer: cannot send message to group " << msg.group
                        << " because it does not exist anymore");
        }
    }
}

void Server::send_event(event_ptr e) {
    switch (eventmode_){
    case kAooEventModePoll:
        events_.push(std::move(e));
        break;
    case kAooEventModeCallback:
    {
        event_handler fn(eventhandler_, eventcontext_, kAooThreadLevelNetwork);
        e->dispatch(fn);
        break;
    }
    default:
        break;
    }
}

void Server::close() {
    // JC: need to close all the clients sockets without having them
    // send anything out, so that active communication between connected
    // peers can continue if the server goes down for maintainence
    clients_.clear();
    groups_.clear();
    message_queue_.clear();
}

} // net
} // aoo
