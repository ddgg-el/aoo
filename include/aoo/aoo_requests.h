/* Copyright (c) 2010-Now Christof Ressi, Winfried Ritsch and others.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#pragma once

#include "aoo_config.h"
#include "aoo_defines.h"
#include "aoo_types.h"

AOO_PACK_BEGIN

/*--------------------------------------------*/

/** \brief request types */
AOO_ENUM(AooRequestType)
{
    /** error response */
    kAooRequestError = 0,
    /** connect to server */
    kAooRequestConnect,
    /** login to server */
    kAooRequestLogin,
    /** disconnect from server */
    kAooRequestDisconnect,
    /** join group */
    kAooRequestGroupJoin,
    /** leave group */
    kAooRequestGroupLeave,
    /** update group */
    kAooRequestGroupUpdate,
    /** update user */
    kAooRequestUserUpdate,
    /** custom request */
    kAooRequestCustom
};

/*----------------- request ------------------*/

/** \brief common header for all request structures */
#define AOO_REQUEST_HEADER  \
    AooRequestType type;    \
    AooUInt32 structSize;

/** \brief basic request */
typedef struct AooRequestBase
{
    AOO_REQUEST_HEADER
} AooRequestBase;

#define AOO_REQUEST_INIT(name, field) \
    kAooRequest##name, AOO_STRUCT_SIZE(AooRequest##name, field)

/*----------------- response ------------------*/

/** \brief common header for all response structures */
#define AOO_RESPONSE_HEADER AOO_REQUEST_HEADER

/** \brief basic response */
typedef struct AooResponseBase
{
    AOO_RESPONSE_HEADER
} AooResponseBase;

#define AOO_RESPONSE_INIT(name, field) \
    kAooRequest##name, AOO_STRUCT_SIZE(AooResponse##name, field)

/*----------------- error ------------------*/

/** \brief error response
 *
 * \attention always initialize with AOO_RESPONSE_ERROR_INIT()!
 */
typedef struct AooResponseError
{
    AOO_RESPONSE_HEADER
    /** platform- or user-specific error code */
    AooInt32 errorCode;
    /** discriptive error message */
    const AooChar *errorMessage;
} AooResponseError;

/** \brief default initializer for AooResponseError struct */
#define AOO_RESPONSE_ERROR_INIT() \
    { AOO_RESPONSE_INIT(Error, errorMessage), 0, NULL }

/*--------- connect (client-side) ---------*/

/** \brief connection request
 *
 * \attention always initialize with AOO_REQUEST_CONNECT_INIT()!
 */
typedef struct AooRequestConnect
{
    AOO_REQUEST_HEADER
    AooIpEndpoint address;
    const AooChar *password;
    const AooData *metadata;
} AooRequestConnect;

/** \brief default initializer for AooRequestConnect struct */
#define AOO_REQUEST_CONNECT_INIT() \
    { AOO_REQUEST_INIT(Connect, metadata), { NULL, 0 }, NULL, NULL }

/** \brief connection response
 *
 * \attention always initialize with AOO_RESPONSE_CONNECT_INIT()!
 */
typedef struct AooResponseConnect
{
    AOO_RESPONSE_HEADER
    AooId clientId;
    const AooChar *version;
    const AooData *metadata;
} AooResponseConnect;

/** \brief default initializer for AooResponseConnect struct */
#define AOO_RESPONSE_CONNECT_INIT() \
    { AOO_RESPONSE_INIT(Connect, metadata), kAooIdInvalid, NULL, NULL }

/*--------- disconnect (client-side) ----------*/

/** \brief disconnect request
 *
 * \attention always initialize with AOO_REQUEST_DISCONNECT_INIT()!
 */
typedef struct AooRequestDisconnect
{
    AOO_REQUEST_HEADER
} AooRequestDisconnect;

/** \brief default initializer for AooRequestDisconnect struct */
#define AOO_REQUEST_DISCONNECT_INIT() \
    { AOO_REQUEST_INIT(Disconnect, structSize) }

/** \brief disconnect response
 *
 * \attention always initialize with AOO_RESPONSE_DISCONNECT_INIT()!
 */
typedef struct AooResponseDisconnect
{
    AOO_RESPONSE_HEADER
} AooResponseDisconnect;

/** \brief default initializer for AooResponseDisconnect struct */
#define AOO_RESPONSE_DISCONNECT_INIT() \
    { AOO_RESPONSE_INIT(Disconnect, structSize) }

/*----------- login (server-side) ------------*/

/** \brief login request
 *
 * \attention always initialize with AOO_REQUEST_LOGIN_INIT()!
 */
typedef struct AooRequestLogin
{
    AOO_REQUEST_HEADER
    const AooChar *version;
    const AooChar *password;
    const AooData *metadata;
} AooRequestLogin;

/** \brief default initializer for AooRequestLogin struct */
#define AOO_REQUEST_LOGIN_INIT() \
    { AOO_REQUEST_INIT(Login, metadata), NULL, NULL, NULL }

/** \brief login response
 *
 * \attention always initialize with AOO_RESPONSE_LOGIN_INIT()!
 */
typedef struct AooResponseLogin
{
    AOO_RESPONSE_HEADER
    const AooData *metadata;
} AooResponseLogin;

/** \brief default initializer for AooResponseLogin struct */
#define AOO_RESPONSE_LOGIN_INIT() \
    { AOO_RESPONSE_INIT(Login, metadata), NULL }

/*-------- join group (server/client) -------*/

/** \brief request for joining a group
 *
 * \attention always initialize with AOO_REQUEST_GROUP_JOIN_INIT()!
 */
typedef struct AooRequestGroupJoin
{
    AOO_REQUEST_HEADER
    /*---------------------------- group ----------------------------*/
    const AooChar *groupName;
    const AooChar *groupPwd;
    AooId groupId; /* kAooIdInvalid if group does not exist (yet) */
    /** The client who creates the group may provide group metadata
     * in AooClient::joinGroup(). By default, the server just stores
     * the metadata and sends it to all subsequent users via this field.
     * However, it may also intercept the request and validate/modify the
     * metadata, or provide any kind of metadata it wants, by setting
     * AooResponseGroupJoin::groupMetadata. */
    const AooData *groupMetadata;
    /*---------------------------- user ---------------------------*/
    const AooChar *userName;
    const AooChar *userPwd;
    AooId userId; /* kAooIdInvalid if user does not exist (yet) */
    /** Each client may provide user metadata in AooClient::joinGroup().
     * By default, the server just stores the metadata and sends it to all
     * peers via AooEventPeer::metadata. However, it may also intercept
     * the request and validate/modify the metadata, or provide any kind of
     * metadata it wants, by setting AooResponseGroupJoin::userMetadata. */
    const AooData *userMetadata;
    /*--------------------------- other --------------------------*/
    /** (optional) UDP relay address provided by the user/client.
     * The server will forward it to all peers. */
    const AooIpEndpoint *relayAddress;
} AooRequestGroupJoin;

/** \brief default initializer for AooRequestGroupJoin struct */
#define AOO_REQUEST_GROUP_JOIN_INIT() \
    { AOO_REQUEST_INIT(GroupJoin, relayAddress), NULL, NULL, kAooIdInvalid, \
        NULL, NULL, NULL, kAooIdInvalid, NULL, NULL }

/** \brief response for joining a group
 *
 * \attention always initialize with AOO_RESPONSE_GROUP_JOIN_INIT()!
 */
typedef struct AooResponseGroupJoin
{
    AOO_RESPONSE_HEADER
    /*--------------------------- group -----------------------------*/
    /** group ID generated by the server */
    AooId groupId;
    AooGroupFlags groupFlags;
    /** (optional) group metadata validated/modified by the server. */
    const AooData *groupMetadata;
    /*--------------------------- user ------------------------------*/
    /** user Id generated by the server */
    AooId userId;
    AooUserFlags userFlags;
    /** (optional) user metadata validated/modified by the server. */
    const AooData *userMetadata;
    /*--------------------------- other -----------------------------*/
    /** (optional) private metadata that is only sent to the client.
     * For example, this can be used for state synchronization. */
    const AooData *privateMetadata;
    /** (optional) UDP relay address provided by the server.
     * For example, you may provide a group with a dedicated UDP
     * relay server.
     * If `hostName` is `NULL`, it means that the relay has the same
     * IP address(es) as the AOO server. */
    const AooIpEndpoint *relayAddress;
} AooResponseGroupJoin;

/** \brief default initializer for AooResponseGroupJoin struct */
#define AOO_RESPONSE_GROUP_JOIN_INIT() \
    { AOO_RESPONSE_INIT(GroupJoin, relayAddress), kAooIdInvalid, 0, NULL, \
        kAooIdInvalid, 0, NULL, NULL, NULL }

/*--------- leave group (server/client) ----------*/

/** \brief request for leaving a group
 *
 * \attention always initialize with AOO_RESPONSE_GROUP_LEAVE_INIT()!
 */
typedef struct AooRequestGroupLeave
{
    AOO_REQUEST_HEADER
    AooId group;
} AooRequestGroupLeave;

/** \brief default initializer for AooRequestGroupLeave struct */
#define AOO_REQUEST_GROUP_LEAVE_INIT() \
    { AOO_REQUEST_INIT(GroupLeave, group), kAooIdInvalid }

/** \brief response for leaving a group
 *
 * \attention always initialize with AOO_RESPONSE_GROUP_LEAVE_INIT()!
 */
typedef struct AooResponseGroupLeave
{
    AOO_REQUEST_HEADER
} AooResponseGroupLeave;

/** \brief default initializer for AooResponseGroupLeave struct */
#define AOO_RESPONSE_GROUP_LEAVE_INIT() \
    { AOO_REQUEST_INIT(GroupLeave, structSize) }

/*------------ update group metadata -------------*/

/** \brief request for updating a group
 *
 * \attention always initialize with AOO_REQUEST_GROUP_UPDATE_INIT()!
 */
typedef struct AooRequestGroupUpdate
{
    AOO_REQUEST_HEADER
    AooId groupId;
    AooData groupMetadata;
} AooRequestGroupUpdate;

/** \brief default initializer for AooRequestGroupUpdate struct */
#define AOO_REQUEST_GROUP_UPDATE_INIT() \
    { AOO_REQUEST_INIT(GroupUpdate, groupMetadata), kAooIdInvalid, \
    { kAooDataUnspecified, NULL, 0 } }

/** \brief response for updating a group
 *
 * \attention always initialize with AOO_RESPONSE_GROUP_UPDATE_INIT()!
 */
typedef struct AooResponseGroupUpdate
{
    AOO_RESPONSE_HEADER
    AooData groupMetadata;
} AooResponseGroupUpdate;

/** \brief default initializer for AooResponseGroupJoin struct */
#define AOO_RESPONSE_GROUP_UPDATE_INIT() \
    { AOO_RESPONSE_INIT(GroupUpdate, groupMetadata), \
        { kAooDataUnspecified, NULL, 0 } }

/*------------ update user metadata -------------*/

/** \brief request for updating a user
 *
 * \attention always initialize with AOO_REQUEST_USER_UPDATE_INIT()!
 */
typedef struct AooRequestUserUpdate
{
    AOO_REQUEST_HEADER
    AooId groupId;
    AooData userMetadata;
} AooRequestUserUpdate;

/** \brief default initializer for AooRequestUserUpdate struct */
#define AOO_REQUEST_USER_UPDATE_INIT() \
    { AOO_REQUEST_INIT(UserUpdate, userMetadata), kAooIdInvalid, \
        { kAooDataUnspecified, NULL, 0 } }

/** \brief response for updating a user
 *
 * \attention always initialize with AOO_RESPONSE_USER_UPDATE_INIT()!
 */
typedef struct AooResponseUserUpdate
{
    AOO_RESPONSE_HEADER
    AooData userMetadata;
} AooResponseUserUpdate;

/** \brief default initializer for AooResponseUserJoin struct */
#define AOO_RESPONSE_USER_UPDATE_INIT() \
    { AOO_RESPONSE_INIT(UserUpdate, userMetadata), \
        { kAooDataUnspecified, NULL, 0 } }

/*------- custom request (server/client) --------*/

/** \brief custom client request
 *
 * \attention always initialize with AOO_REQUEST_CUSTOM_INIT()!
 */
typedef struct AooRequestCustom
{
    AOO_REQUEST_HEADER
    AooData data;
    AooFlag flags; /* TODO: do we need this? */
} AooRequestCustom;

/** \brief default initializer for AooRequestCustom struct */
#define AOO_REQUEST_CUSTOM_INIT() \
    { AOO_REQUEST_INIT(Custom, flags), { kAooDataUnspecified, NULL, 0 }, 0 }

/** \brief custom server response
 *
 * \attention always initialize with AOO_RESPONSE_CUSTOM_INIT()!
 */
typedef struct AooResponseCustom
{
    AOO_RESPONSE_HEADER
    AooData data;
    AooFlag flags; /* TODO: do we need this? */
} AooResponseCustom;

/** \brief default initializer for AooResponseCustom struct */
#define AOO_RESPONSE_CUSTOM_INIT() \
    { AOO_RESPONSE_INIT(Custom, flags), { kAooDataUnspecified, NULL, 0 }, 0 }

/*--------------------------------------------*/

/** \brief union of all client requests */
union AooRequest
{
    AooRequestType type; /** request type */
    AooRequestConnect connect;
    AooRequestDisconnect disconnect;
    AooRequestLogin login;
    AooRequestGroupJoin groupJoin;
    AooRequestGroupLeave groupLeave;
    AooRequestGroupUpdate groupUpdate;
    AooRequestUserUpdate userUpdate;
    AooRequestCustom custom;
};

/*--------------------------------------------*/

/** \brief union of all client responses */
union AooResponse
{
    AooRequestType type; /** request type */
    AooResponseError error;
    AooResponseConnect connect;
    AooResponseDisconnect disconnect;
    AooResponseLogin login;
    AooResponseGroupJoin groupJoin;
    AooResponseGroupLeave groupLeave;
    AooResponseGroupUpdate groupUpdate;
    AooResponseUserUpdate userUpdate;
    AooResponseCustom custom;
};

/*--------------------------------------------*/

AOO_PACK_END
