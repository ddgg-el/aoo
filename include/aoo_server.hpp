/* Copyright (c) 2021 Christof Ressi
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/** \file
 * \brief C++ interface for AOO server
 *
 * 1. UDP: query -> get public IP + TCP server ID
 * 2. TCP: connect to TCP server -> get user ID
 * 3. TCP: join group -> get peer ID (start from 0) + relay server address
 */

#pragma once

#include "aoo_config.h"
#include "aoo_controls.h"
#include "aoo_defines.h"
#include "aoo_events.h"
#include "aoo_requests.h"
#include "aoo_types.h"

#if AOO_HAVE_CXX11
# include <memory>
#endif

/** \cond DO_NOT_DOCUMENT */
typedef struct AooServer AooServer;
/** \endcond */

/** \brief create a new AOO server instance
 *
 * \return new AooServer instance on success; `NULL` on failure
 */
AOO_API AooServer * AOO_CALL AooServer_new(void);

/** \brief destroy AOO server instance */
AOO_API void AOO_CALL AooServer_free(AooServer *server);

/*-----------------------------------------------------------*/

/** \brief AOO server interface */
struct AooServer {
public:
#if AOO_HAVE_CXX11
    /** \brief custom deleter for AooServer */
    class Deleter {
    public:
        void operator()(AooServer *obj){
            AooServer_free(obj);
        }
    };

    /** \brief smart pointer for AOO server instance */
    using Ptr = std::unique_ptr<AooServer, Deleter>;

    /** \brief create a new managed AOO server instance
     *
     * \return valid Ptr on success; empty Ptr failure
     */
    static Ptr create() {
        return Ptr(AooServer_new());
    }
#endif

    /*---------------------- methods ---------------------------*/

    /** \brief setup the server object (before calling run())
     *
     * \param settings settings objects; it might be modified to reflect the actual values.
     */
    virtual AooError AOO_CALL setup(AooServerSettings& settings) = 0;

    /** \brief run the server
     *
     * \param timeout the timeout in seconds
     *   - A number >= 0: the method only blocks up to the given duration.
     *     It returns #kAooOk if it did something, #kAooErrorWouldBlock
     *     if timed out, or any other error code if an error occured.
     *   - #kAooInfinite: blocks until quit() is called or an error occured.
     */
    virtual AooError AOO_CALL run(AooSeconds timeout) = 0;

    /** \brief receive and handle UDP packets (from internal UDP socket)
     *
     * \param timeout the timeout in seconds
     *   - A number >= 0: the method only blocks up to the given duration.
     *     It returns #kAooOk if it did something, #kAooErrorWouldBlock
     *     if timed out, or any other error code if an error occured.
     *   - #kAooInfinite: blocks until quit() is called or an error occured.
     */
    virtual AooError AOO_CALL receive(AooSeconds timeout) = 0;

    /** \brief handle UDP packet from external UDP socket */
    virtual AooError AOO_CALL handlePacket(
            const AooByte *data, AooInt32 size,
            const void *address, AooAddrSize addrlen) = 0;

    /** \brief stop the AOO client from another thread */
    virtual AooError AOO_CALL stop() = 0;

    /* event handling */

    /** \brief set event handler function and event handling mode
     *
     * \attention Not threadsafe - only call in the beginning!
     */
    virtual AooError AOO_CALL setEventHandler(
            AooEventHandler fn, void *user, AooEventMode mode) = 0;

    /** \brief check for pending events
     *
     * \note Threadsafe and RT-safe
     */
    virtual AooBool AOO_CALL eventsAvailable() = 0;

    /** \brief poll events
     *
     * \note Threadsafe and RT-safe, but not reentrant.
     *
     * This function will call the registered event handler one or more times.
     * \attention The event handler must have been registered with #kAooEventModePoll.
     */
    virtual AooError AOO_CALL pollEvents() = 0;

    /* request handling */

    /** \brief set request handler (to intercept client requests) */
    virtual AooError AOO_CALL setRequestHandler(
            AooRequestHandler cb, void *user, AooFlag flags) = 0;

    /** \brief handle request
     *
     * If `result` is `kAooErrorNone`, the request has been handled successfully
     * and `response` points to the corresponding response structure.
     *
     * Otherwise the request has failed or been denied; in this case `response`
     * is either `NULL` or points to an `AooResponseError` structure for more detailed
     * information about the error. For example, in the case of `kAooErrorSystem`,
     * the response may contain an OS-specific error code and error message.
     *
     * \attention The response must be properly initialized with `AOO_RESPONSE_INIT`.
     */
    virtual AooError AOO_CALL handleRequest(
            AooId client, AooId token, const AooRequest *request,
            AooError result, AooResponse *response) = 0;

    /* push notifications */

    /** \brief send custom push notification to client;
     *
     * if `client` is #kAooIdAll, all clients are notified.
     */
    virtual AooError AOO_CALL notifyClient(
            AooId client, const AooData &data) = 0;

    /** \brief send custom push notification to group member(s);
     *
     * if `user` is #kAooIdAll, all group members are notified.
     */
    virtual AooError AOO_CALL notifyGroup(
            AooId group, AooId user, const AooData &data) = 0;

    /* group management */

    /** \brief find a group by name */
    virtual AooError AOO_CALL findGroup(
            const AooChar *name, AooId *id) = 0;

    /** \brief add a group
     * By default, the metadata is passed to clients
     * via AooResponseGroupJoin::groupMetadata. */
    virtual AooError AOO_CALL addGroup(
            const AooChar *name, const AooChar *password,
            const AooData *metadata, const AooIpEndpoint *relayAddress,
            AooFlag flags, AooId *groupId) = 0;

    /** \brief remove a group */
    virtual AooError AOO_CALL removeGroup(
            AooId group) = 0;

    /** \brief find a user in a group by name */
    virtual AooError AOO_CALL findUserInGroup(
            AooId group, const AooChar *userName, AooId *userId) = 0;

    /** \brief add user to group
     * By default, the metadata is passed to peers via AooEventPeer::metadata. */
    virtual AooError AOO_CALL addUserToGroup(
            AooId group, const AooChar *userName, const AooChar *userPwd,
            const AooData *metadata, AooFlag flags, AooId *userId) = 0;

    /** \brief remove user from group */
    virtual AooError AOO_CALL removeUserFromGroup(
            AooId group, AooId user) = 0;

    /** \brief group control interface
     *
     * Not to be used directly.
     */
    virtual AooError AOO_CALL groupControl(
            AooId group, AooCtl ctl, AooIntPtr index,
            void *data, AooSize size) = 0;

    /** \brief control interface
     *
     * Not to be used directly.
     */
    virtual AooError AOO_CALL control(
            AooCtl ctl, AooIntPtr index, void *data, AooSize size) = 0;

    /*--------------------------------------------*/
    /*         type-safe control functions        */
    /*--------------------------------------------*/

    /** \brief set the server password */
    AooError setPassword(const AooChar *pwd)
    {
        return control(kAooCtlSetPassword, (AooIntPtr)pwd, NULL, 0);
    }

    /** \brief set external (global) relay host
     *
     *  If `hostName` is `NULL`, it means that the relay has
     *  the same IP address(es) as the AOO server.
     *  If `ep` is `NULL`, the relay host is unset. */
    AooError setRelayHost(const AooIpEndpoint *ep)
    {
        return control(kAooCtlSetRelayHost, (AooIntPtr)ep, NULL, 0);
    }

    /** \brief enabled/disable internal relay */
    AooError setUseInternalRelay(AooBool b)
    {
        return control(kAooCtlSetUseInternalRelay, 0, AOO_ARG(b));
    }

    /** \brief check if internal relay is enabled */
    AooError getUseInternalRelay(AooBool& b)
    {
        return control(kAooCtlGetUseInternalRelay, 0, AOO_ARG(b));
    }

    /** \brief enable/disable automatic group creation */
    AooError setGroupAutoCreate(AooBool b)
    {
        return control(kAooCtlSetGroupAutoCreate, 0, AOO_ARG(b));
    }

    /** \brief check if automatic group creation is enabled */
    AooError getGroupAutoCreate(AooBool& b)
    {
        return control(kAooCtlGetGroupAutoCreate, 0, AOO_ARG(b));
    }

    /** \brief Set client ping settings */
    AooError setPingSettings(const AooPingSettings& settings) {
        return control(kAooCtlSetPingSettings, 0, AOO_ARG(settings));
    }

    /** \brief Get client ping settings */
    AooError getPingSettings(AooPingSettings& settings) {
        return control(kAooCtlGetPingSettings, 0, AOO_ARG(settings));
    }

    /*--------------------------------------------------*/
    /*         type-safe group control functions        */
    /*--------------------------------------------------*/

    /** \brief update group metadata */
    AooError updateGroup(AooId group, const AooData *metadata)
    {
        return groupControl(group, kAooCtlUpdateGroup, 0, AOO_ARG(metadata));
    }

    /** \brief update user metadata */
    AooError updateUser(AooId group, AooId user, const AooData *metadata)
    {
        return groupControl(group, kAooCtlUpdateUser, user, AOO_ARG(metadata));
    }
protected:
    ~AooServer(){} // non-virtual!
};
