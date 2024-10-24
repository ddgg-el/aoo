#include "client_endpoint.hpp"

#include "detail.hpp"
#include "server.hpp"

namespace aoo {
namespace net {

user* group::add_user(user&& usr) {
    if (!find_user(usr.name())) {
        users_.emplace_back(std::move(usr));
        return &users_.back();
    } else {
        return nullptr;
    }
}

user* group::find_user(std::string_view name) {
    for (auto& usr : users_) {
        if (usr.name() == name) {
            return &usr;
        }
    }
    return nullptr;
}

user* group::find_user(AooId id) {
    for (auto& usr : users_) {
        if (usr.id() == id) {
            return &usr;
        }
    }
    return nullptr;
}

user* group::find_user(const client_endpoint& client) {
    for (auto& usr : users_) {
        if (usr.client() == client.id()) {
            return &usr;
        }
    }
    return nullptr;
}

bool group::remove_user(AooId id) {
    for (auto it = users_.begin(); it != users_.end(); ++it) {
        if (it->id() == id) {
            users_.erase(it);
            return true;
        }
    }
    return false;
}

AooId group::get_next_user_id() {
#if 1
    // try to reclaim ID
    for (AooId i = 0; i < next_user_id_; ++i) {
        if (!find_user(i)) {
            return i;
        }
    }
#endif
    // make new ID
    return next_user_id_++;
}

//------------------- client_endpoint -------------------//

bool client_endpoint::match(const ip_address& addr) const {
    // match public UDP addresses!
    for (auto& a : public_addresses_) {
        if (a == addr){
            return true;
        }
    }
    return false;
}

void client_endpoint::send_message(const osc::OutboundPacketStream& msg) const {
    // prepend message size (int32_t)
    auto data = msg.Data() - 4;
    auto size = msg.Size() + 4;
    // we know that the buffer is not really constant
    aoo::to_bytes<int32_t>(msg.Size(), const_cast<char *>(data));
    try {
        replyfn_((const AooByte *)data, size);
    } catch (const socket_error& e) {
        LOG_WARNING("AooServer: send() failed for client "
                    << id_ << ": " << e.what());
        // TODO handle error
    }
}

void client_endpoint::send_error(Server& server, AooId token, AooRequestType type,
                                 AooError result, const AooResponseError *response) {
    const char *pattern;
    switch (type) {
    case kAooRequestLogin:
        pattern = kAooMsgClientLogin;
        break;
    case kAooRequestGroupJoin:
        pattern = kAooMsgClientGroupJoin;
        break;
    case kAooRequestGroupLeave:
        pattern = kAooMsgClientGroupLeave;
        break;
    case kAooRequestCustom:
        pattern = kAooMsgClientRequest;
        break;
    default:
        // TODO bug
        return;
    }

    auto msg = server.start_message();

    msg << osc::BeginMessage(pattern) << (int32_t)token << result;
    if (response) {
        msg << response->errorCode << response->errorMessage;
    } else {
        msg << (int32_t)0 << "";
    }
    msg << osc::EndMessage;

    send_message(msg);
}

void client_endpoint::send_notification(Server& server, const AooData &data) const {
    auto msg = server.start_message(data.size);

    msg << osc::BeginMessage(kAooMsgClientMessage)
        << osc::Blob(data.data, data.size) << osc::EndMessage;

    send_message(msg);
}

void client_endpoint::send_peer_join(Server& server, const group& grp, const user& usr,
                                     const client_endpoint& client) const {

    LOG_DEBUG("AooServer: send peer " << grp << "|" << usr << " " << client.public_addresses().front()
              << " to client " << id() << " " << public_addresses().front());

    auto msg = server.start_message(usr.metadata().size());

    AooFlag flags = 0;
    if (usr.group_creator()) {
        flags |= kAooPeerGroupCreator;
    }
    if (usr.persistent()) {
        flags |= kAooPeerPersistent;
    }

    msg << osc::BeginMessage(kAooMsgClientPeerJoin)
        << grp.name().c_str() << grp.id()
        << usr.name().c_str() << usr.id()
        << client.version().c_str() << (int32_t)flags
        << usr.metadata() << usr.relay_addr();
    // IP addresses
    msg << (int32_t)client.public_addresses().size();
    for (auto& addr : client.public_addresses()){
        msg << addr;
    }
    msg << osc::EndMessage;

    send_message(msg);
}

void client_endpoint::send_peer_leave(Server& server, const group& grp, const user& usr) const {
    LOG_DEBUG("AooServer: remove peer " << grp.name() << "|" << usr.name());

    auto msg = server.start_message();

    msg << osc::BeginMessage(kAooMsgClientPeerLeave)
        << grp.id() << usr.id() << osc::EndMessage;

    send_message(msg);
}

void client_endpoint::send_group_update(Server& server, const group& grp, AooId usr) {
    auto msg = server.start_message();

    msg << osc::BeginMessage(kAooMsgClientGroupChanged)
        << grp.id() << usr << grp.metadata() << osc::EndMessage;

    send_message(msg);
}

void client_endpoint::send_user_update(Server& server, const user& usr) {
    auto msg = server.start_message();

    msg << osc::BeginMessage(kAooMsgClientUserChanged)
        << usr.group() << usr.id() << usr.metadata() << osc::EndMessage;

    send_message(msg);
}

void client_endpoint::send_peer_update(Server& server, const user& peer) {
    auto msg = server.start_message();

    msg << osc::BeginMessage(kAooMsgClientPeerChanged)
        << peer.group() << peer.id() << peer.metadata() << osc::EndMessage;

    send_message(msg);
}

void client_endpoint::on_close(Server& server) {
    for (auto& gu : group_users_) {
        if (auto grp = server.find_group(gu.group)) {
            if (auto usr = grp->find_user(gu.user)) {
                server.on_user_left_group(*grp, *usr);
                server.do_remove_user_from_group(*grp, *usr);
            } else {
                LOG_ERROR("AooServer: client_endpoint::on_close: could not find user " << gu.user);
            }
        } else {
            LOG_ERROR("AooServer: client_endpoint::on_close: could not find group " << gu.group);
        }
    }
    group_users_.clear();
    id_ = kAooIdInvalid;
}

void client_endpoint::handle_data(Server &server, const AooByte *data, int32_t n) {
    receiver_.handle_message((const char *)data, n,
            [&](const osc::ReceivedPacket& packet) {
        osc::ReceivedMessage msg(packet);
        server.handle_message(*this, msg, packet.Size());
    });
}

std::pair<bool, double> client_endpoint::update(Server& server, aoo::time_tag now,
                                                const AooPingSettings &settings) {
    auto result = ping_timer_.update(now, settings);
    if (result.ping) {
        auto msg = server.start_message();

        msg << osc::BeginMessage(kAooMsgClientPing) << osc::EndMessage;

        send_message(msg);
    }
    if (result.state == ping_state::inactive) {
        if (active_) {
            active_ = false;
            return { true, result.wait };
        }
    }
    return { false, result.wait };
}

void client_endpoint::on_group_join(Server&, const group& grp, const user& usr) {
#if 1
    for (auto& gu : group_users_) {
        if (gu.group == grp.id() && gu.user == usr.id()) {
            LOG_ERROR("AooServer: client_endpoint::add_group_user: "
                      << "group user already exists");
            return;
        }
    }
#endif
    group_users_.emplace_back(group_user { grp.id(), usr.id() });
}

void client_endpoint::on_group_leave(Server& server, const group& grp,
                                     const user& usr, bool eject) {
    if (eject) {
        // tell client that it has been ejected from the group
        auto msg = server.start_message();

        msg << osc::BeginMessage(kAooMsgClientGroupEject)
            << grp.id() << osc::EndMessage;

        send_message(msg);
    }
    for (auto it = group_users_.begin(); it != group_users_.end(); ++it) {
        if (it->group == grp.id() && it->user == usr.id()) {
            group_users_.erase(it);
            return;
        }
    }
    LOG_ERROR("AooServer: on_group_leave: couldn't remove group user "
              << grp.id() << "|" << usr.id());
}

} // net
} // aoo
