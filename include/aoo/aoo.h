/* Copyright (c) 2010-Now Christof Ressi, Winfried Ritsch and others. 
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#pragma once

#include "aoo_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

AOO_API const char *aoo_error_string(aoo_error e);

AOO_API void aoo_version(int32_t *major, int32_t *minor,
                         int32_t *patch, int32_t *pre);

AOO_API const char *aoo_version_string(void);

// call before aoo_initialize!
AOO_API void aoo_set_logfunction(aoo_logfunction fn);

#ifndef AOO_USE_ALLOCATOR
#define AOO_USE_ALLOCATOR 0
#endif

#if AOO_USE_ALLOCATOR
// call before aoo_initialize!
AOO_API void aoo_set_allocator(const aoo_allocator *alloc);

#endif

#ifndef AOO_DEBUG_MEMORY
 #define AOO_DEBUG_MEMORY 0
#endif

#ifndef AOO_DEBUG_DLL
 #define AOO_DEBUG_DLL 0
#endif

#ifndef AOO_DEBUG_TIMEFILTER
 #define AOO_DEBUG_TIMEFILTER 0
#endif

#ifndef AOO_DEBUG_RESAMPLING
 #define AOO_DEBUG_RESAMPLING 0
#endif

#ifndef AOO_DEBUG_AUDIO_BUFFER
 #define AOO_DEBUG_AUDIO_BUFFER 0
#endif

#ifndef AOO_DEBUG_JITTER_BUFFER
 #define AOO_DEBUG_JITTER_BUFFER 0
#endif

#ifndef AOO_CLIP_OUTPUT
#define AOO_CLIP_OUTPUT 0
#endif

/*////////// default values ////////////*/

// source buffer size in ms
#ifndef AOO_SOURCE_BUFSIZE
 #define AOO_SOURCE_BUFSIZE 25
#endif

// sink buffer size in ms
#ifndef AOO_SINK_BUFSIZE
 #define AOO_SINK_BUFSIZE 100
#endif

// time DLL filter bandwidth
#ifndef AOO_TIMEFILTER_BANDWIDTH
 #define AOO_TIMEFILTER_BANDWIDTH 0.012
#endif

// try to catch timing issues (e.g. blocking audio thread)
#ifndef AOO_TIMEFILTER_CHECK
 #define AOO_TIMEFILTER_CHECK 1
#endif

// the tolerance for deviations from the nominal block period
#ifndef AOO_TIMEFILTER_TOLERANCE
 #define AOO_TIMEFILTER_TOLERANCE 0.25
#endif

// ping interval (sink to source) in ms
#ifndef AOO_PING_INTERVAL
 #define AOO_PING_INTERVAL 1000
#endif

// resend buffer size in ms
#ifndef AOO_RESEND_BUFSIZE
 #define AOO_RESEND_BUFSIZE 1000
#endif

// send redundancy
#ifndef AOO_SEND_REDUNDANCY
 #define AOO_SEND_REDUNDANCY 1
#endif

// enable/disable packet resending
#ifndef AOO_RESEND_ENABLE
 #define AOO_RESEND_ENABLE 1
#endif

// interval between resend attempts in ms
#ifndef AOO_RESEND_INTERVAL
 #define AOO_RESEND_INTERVAL 10
#endif

// max. number of frames to request per call
#ifndef AOO_RESEND_MAXNUMFRAMES
 #define AOO_RESEND_MAXNUMFRAMES 16
#endif

// max. number of ms to wait before removing source
#ifndef AOO_SOURCE_TIMEOUT
 #define AOO_SOURCE_TIMEOUT 1000
#endif

// initialize AoO library - call only once!
AOO_API void aoo_initialize(void);

// terminate AoO library - call only once!
AOO_API void aoo_terminate(void);

/*//////////////////// OSC ////////////////////////////*/

#define AOO_MSG_SOURCE "/src"
#define AOO_MSG_SOURCE_LEN 4
#define AOO_MSG_SINK "/sink"
#define AOO_MSG_SINK_LEN 5
#define AOO_MSG_WILDCARD "/*"
#define AOO_MSG_WILDCARD_LEN 2
#define AOO_MSG_FORMAT "/format"
#define AOO_MSG_FORMAT_LEN 7
#define AOO_MSG_DATA "/data"
#define AOO_MSG_DATA_LEN 5
#define AOO_MSG_PING "/ping"
#define AOO_MSG_PING_LEN 5
#define AOO_MSG_INVITE "/invite"
#define AOO_MSG_INVITE_LEN 7
#define AOO_MSG_UNINVITE "/uninvite"
#define AOO_MSG_UNINVITE_LEN 9

// get the aoo_type and ID from an AoO OSC message, e.g. in /aoo/src/<id>/data
// returns the offset on success, 0 on fail
AOO_API int32_t aoo_parse_pattern(const char *msg, int32_t n,
                                  aoo_type *type, aoo_id *id);

// get the current NTP time
AOO_API uint64_t aoo_osctime_now(void);

// convert NTP time to seconds
AOO_API double aoo_osctime_to_seconds(uint64_t t);

// convert seconds to NTP time
AOO_API uint64_t aoo_osctime_from_seconds(double s);

// get time difference in seconds between two NTP timestamps
AOO_API double aoo_osctime_duration(uint64_t t1, uint64_t t2);

/*//////////////////// AoO events /////////////////////*/

#define AOO_EVENTQUEUESIZE 8

// event types
typedef enum aoo_event_type
{
    // source: received a ping from sink
    AOO_PING_EVENT = 0,
    // source: invited by sink
    AOO_INVITE_EVENT,
    // source: uninvited by sink
    AOO_UNINVITE_EVENT,
    // sink: source added
    AOO_SOURCE_ADD_EVENT,
    // sink: source removed
    AOO_SOURCE_REMOVE_EVENT,
    // sink: source format changed
    AOO_SOURCE_FORMAT_EVENT,
    // sink: source changed state
    AOO_SOURCE_STATE_EVENT,
    // sink: blocks have been lost
    AOO_BLOCK_LOST_EVENT,
    // sink: blocks arrived out of order
    AOO_BLOCK_REORDERED_EVENT,
    // sink: blocks have been resent
    AOO_BLOCK_RESENT_EVENT,
    // sink: large gap between blocks
    AOO_BLOCK_GAP_EVENT,
    // sink: source invitation timed out
    AOO_INVITE_TIMEOUT_EVENT,
} aoo_event_type;

#define AOO_ENDPOINT_EVENT  \
    int32_t type;           \
    aoo_id id;              \
    const void *address;    \
    int32_t addrlen;        \

// source event
typedef struct aoo_source_event
{
    AOO_ENDPOINT_EVENT
} aoo_source_event;

// sink event
typedef struct aoo_sink_event
{
    AOO_ENDPOINT_EVENT
} aoo_sink_event;

// source state event
typedef enum aoo_source_state
{
    AOO_SOURCE_STATE_STOP,
    AOO_SOURCE_STATE_PLAY
} aoo_source_state;

typedef struct aoo_source_state_event
{
    AOO_ENDPOINT_EVENT
    int32_t state;
} aoo_source_state_event;

// block events
struct _aoo_block_event
{
    AOO_ENDPOINT_EVENT
    int32_t count;
};

typedef struct _aoo_block_event aoo_block_lost_event;
typedef struct _aoo_block_event aoo_block_reordered_event;
typedef struct _aoo_block_event aoo_block_resent_event;
typedef struct _aoo_block_event aoo_block_gap_event;

// ping event
typedef struct aoo_ping_event {
    AOO_ENDPOINT_EVENT
    uint64_t tt1;
    uint64_t tt2;
    uint64_t tt3; // only for source
    int32_t lost_blocks; // only for source
} aoo_ping_event;

// format event
typedef struct aoo_format_event {
    AOO_ENDPOINT_EVENT
    const struct aoo_format *format;
} aoo_format_event;

/*//////////////////// AoO options ////////////////////*/

typedef enum aoo_option
{
    // The source/sink ID
    aoo_opt_id = 0,
    // Stream format (aoo_format)
    // ---
    // The settings for the audio codec to be used for a stream.
    // If you want to set the format, you have to send the format
    // header, e.g. aoo_format_pcm.header. The format can only be
    // set in sources.
    // If you want to get the format, you have to pass a
    // aoo_format_storage, which is filled with the format.
    // Make sure that the 'size' field is set to the max. storage size!
    aoo_opt_format,
    // Reset the source/sink (NULL)
    aoo_opt_reset,
    // Start the source/sink (NULL)
    aoo_opt_start,
    // Stop the source/sink (NULL)
    aoo_opt_stop,
    // Buffer size in ms (int32_t)
    // ---
    // This is the size of the ring buffer
    // between the audio and network thread.
    // For the source, this can be rather small,
    // as you only have to compensate the latency
    // for thread signalling.
    // For the sink, a larger buffer size helps
    // to deal with network jitter, packet reordering
    // and packet loss. For local networks, small
    // buffersizes between 10-50ms should work,
    // but for unreliable/unpredictable networks
    // you might need to increased it significantly.
    aoo_opt_buffersize,
    // Time filter DLL bandwidth (float)
    // ---
    // The time DLL filter estimates the effective samplerate
    // and is used to compensate clock drift via dynamic resampling.
    // See the paper "Using a DLL to filter time" by Fons Adriaensen.
    aoo_opt_timefilter_bandwidth,
    // Sink channel onset (int32_t)
    // ---
    // The channel onset of the sink where a given source
    // should be received. For example, if the channel onset
    // is 5, a 2-channel source will be summed into sink
    // channels 5 and 6. The default is 0 (= the first channel).
    aoo_opt_channelonset,
    // Max. UDP packet size in bytes (int32_t)
    // ---
    // The default value of 512 should work across most
    // networks (even the internet). You might increase
    // this value for local networks. Larger packet sizes
    // have less overhead. If a audio block exceeds
    // the max. UDP packet size, it will be automatically
    // broken up into several "frames" in reassembled
    // in the sink.
    aoo_opt_packetsize,
    // Ping interval in ms (int32_t)
    // ---
    // The sink sends a periodic ping message to each
    // source to signify that it is actually receiving data.
    // For example, a application might choose to remove
    // a sink after the source hasn't received a ping
    // for a certain amount of time.
    aoo_opt_ping_interval,
    // Enable/disable resending (int32_t)
    aoo_opt_resend_enable,
    // Resend buffer size in ms (int32_t).
    // ---
    // The source keeps the last N ms of audio in a buffer,
    // so it can resend parts of it, if requested, e.g. to
    // handle packet loss.
    aoo_opt_resend_buffersize,
    // Resend interval in ms (int32_t)
    // ---
    // This is the interval between individual resend
    // attempts for a specific frame.
    // Since there is always a certain roundtrip delay
    // between source and sink, it makes sense to wait
    // between resend attempts to not spam the network
    // with redundant /resend messages.
    aoo_opt_resend_interval,
    // Max. number of frames to resend (int32_t)
    // ---
    // This is the max. number of frames to request
    // in a single call to sink_handle_message().
    aoo_opt_resend_maxnumframes,
    // Redundancy (int32_t)
    // ---
    // The number of times each frames is sent (default = 1)
    aoo_opt_redundancy,
    // Source timeout in ms (int32_t)
    // ---
    // Time to wait before removing inactive source.
    aoo_opt_source_timeout
} aoo_option;

#define AOO_ARG(x) &x, sizeof(x)
#define AOO_ARG_NULL 0, 0

/*//////////////////// AoO source /////////////////////*/

typedef struct aoo_format
{
    const char *codec;
    int32_t size;
    int32_t nchannels;
    int32_t samplerate;
    int32_t blocksize;
} aoo_format;

typedef struct aoo_format_storage
{
    aoo_format header;
    char data[256];
} aoo_format_storage;

// create a new AoO source instance
AOO_API aoo_source * aoo_source_new(aoo_id id, uint32_t flags);

// destroy the AoO source instance
AOO_API void aoo_source_free(aoo_source *src);

// setup the source - needs to be synchronized with other method calls!
AOO_API aoo_error aoo_source_setup(aoo_source *src, int32_t samplerate,
                                   int32_t blocksize, int32_t nchannels);

// add a new sink (always threadsafe)
AOO_API aoo_error aoo_source_add_sink(aoo_source *src, const void *address, int32_t addrlen,
                                      aoo_id id, uint32_t flags);

// remove a sink (always threadsafe)
AOO_API aoo_error aoo_source_remove_sink(aoo_source *src, const void *address, int32_t addrlen, aoo_id id);

// remove all sinks (always threadsafe)
AOO_API void aoo_source_remove_all(aoo_source *src);

// handle messages from sinks (threadsafe, but not reentrant)
AOO_API aoo_error aoo_source_handle_message(aoo_source *src, const char *data, int32_t n,
                                          const void *address, int32_t addrlen);

// send outgoing messages - will call the reply function (threadsafe, but not reentrant)
AOO_API aoo_error aoo_source_send(aoo_source *src, aoo_sendfn fn, void *user);

// process audio blocks (threadsafe, but not reentrant)
// data:        array of channel data (non-interleaved)
// nsamples:    number of samples per channel
// t:           current NTP timestamp (see aoo_osctime_get)
AOO_API aoo_error aoo_source_process(aoo_source *src, const aoo_sample **data,
                                     int32_t nsamples, uint64_t t);

// check for pending events (always thread safe)
AOO_API aoo_bool aoo_source_events_available(aoo_source *src);

// poll events (threadsafe, but not reentrant)
// will call the event handler function one or more times
AOO_API aoo_error aoo_source_poll_events(aoo_source *src, aoo_eventhandler fn, void *user);

// set/get options (always threadsafe)
AOO_API aoo_error aoo_source_set_option(aoo_source *src, int32_t opt, void *p, int32_t size);

AOO_API aoo_error aoo_source_get_option(aoo_source *src, int32_t opt, void *p, int32_t size);

// set/get sink options (always threadsafe)
AOO_API aoo_error aoo_source_set_sinkoption(aoo_source *src, const void *address, int32_t addrlen,
                                            aoo_id id, int32_t opt, void *p, int32_t size);

AOO_API aoo_error aoo_source_get_sinkoption(aoo_source *src, const void *address, int32_t addrlen,
                                            aoo_id id, int32_t opt, void *p, int32_t size);

// wrapper functions for frequently used options

static inline aoo_error aoo_source_start(aoo_source *src) {
    return aoo_source_set_option(src, aoo_opt_start, AOO_ARG_NULL);
}

static inline aoo_error aoo_source_stop(aoo_source *src) {
    return aoo_source_set_option(src, aoo_opt_stop, AOO_ARG_NULL);
}

static inline aoo_error aoo_source_set_id(aoo_source *src, aoo_id id) {
    return aoo_source_set_option(src, aoo_opt_id, AOO_ARG(id));
}

static inline aoo_error aoo_source_get_id(aoo_source *src, aoo_id *id) {
    return aoo_source_get_option(src, aoo_opt_id, AOO_ARG(*id));
}

static inline aoo_error aoo_source_set_format(aoo_source *src, aoo_format *f) {
    return aoo_source_set_option(src, aoo_opt_format, (void *)f, sizeof(aoo_format));
}

static inline aoo_error aoo_source_get_format(aoo_source *src, aoo_format_storage *f) {
    return aoo_source_set_option(src, aoo_opt_format, AOO_ARG(*f));
}

static inline aoo_error aoo_source_set_buffersize(aoo_source *src, int32_t n) {
    return aoo_source_set_option(src, aoo_opt_buffersize, AOO_ARG(n));
}

static inline aoo_error aoo_source_get_buffersize(aoo_source *src, int32_t *n) {
    return aoo_source_get_option(src, aoo_opt_buffersize, AOO_ARG(*n));
}

static inline aoo_error aoo_source_set_timefilter_bandwidth(aoo_source *src, float n) {
    return aoo_source_set_option(src, aoo_opt_timefilter_bandwidth, AOO_ARG(n));
}

static inline aoo_error aoo_source_get_timefilter_bandwidth(aoo_source *src, float *n) {
    return aoo_source_get_option(src, aoo_opt_timefilter_bandwidth, AOO_ARG(*n));
}

static inline aoo_error aoo_source_set_packetsize(aoo_source *src, int32_t n) {
    return aoo_source_set_option(src, aoo_opt_packetsize, AOO_ARG(n));
}

static inline aoo_error aoo_source_get_packetsize(aoo_source *src, int32_t *n) {
    return aoo_source_get_option(src, aoo_opt_packetsize, AOO_ARG(*n));
}

static inline aoo_error aoo_source_set_ping_interval(aoo_source *src, int32_t n) {
    return aoo_source_set_option(src, aoo_opt_ping_interval, AOO_ARG(n));
}

static inline aoo_error aoo_source_get_ping_interval(aoo_source *src, int32_t *n) {
    return aoo_source_get_option(src, aoo_opt_ping_interval, AOO_ARG(*n));
}

static inline aoo_error aoo_source_set_resend_buffersize(aoo_source *src, int32_t n) {
    return aoo_source_set_option(src, aoo_opt_resend_buffersize, AOO_ARG(n));
}

static inline aoo_error aoo_source_get_resend_buffersize(aoo_source *src, int32_t *n) {
    return aoo_source_get_option(src, aoo_opt_resend_buffersize, AOO_ARG(*n));
}

static inline aoo_error aoo_source_set_redundancy(aoo_source *src, int32_t n) {
    return aoo_source_set_option(src, aoo_opt_redundancy, AOO_ARG(n));
}

static inline aoo_error aoo_source_get_redundancy(aoo_source *src, int32_t *n) {
    return aoo_source_get_option(src, aoo_opt_redundancy, AOO_ARG(*n));
}

static inline aoo_error aoo_source_set_sink_channelonset(aoo_source *src, const void *address,
                                                       int32_t addrlen, aoo_id id, int32_t onset) {
    return aoo_source_set_sinkoption(src, address, addrlen, id, aoo_opt_channelonset, AOO_ARG(onset));
}

static inline aoo_error aoo_source_get_sink_channelonset(aoo_source *src, const void *address,
                                                       int32_t addrlen, aoo_id id, int32_t *onset) {
    return aoo_source_get_sinkoption(src, address, addrlen, id, aoo_opt_channelonset, AOO_ARG(*onset));
}

/*//////////////////// AoO sink /////////////////////*/

// create a new AoO sink instance
AOO_API aoo_sink * aoo_sink_new(aoo_id id, uint32_t flags);

// destroy the AoO sink instance
AOO_API void aoo_sink_free(aoo_sink *sink);

// setup the sink - needs to be synchronized with other method calls!
AOO_API aoo_error aoo_sink_setup(aoo_sink *sink, int32_t samplerate,
                                 int32_t blocksize, int32_t nchannels);

// invite a source (always threadsafe)
AOO_API aoo_error aoo_sink_invite_source(aoo_sink *sink, const void *address,
                                         int32_t addrlen, aoo_id id);

// uninvite a source (always threadsafe)
AOO_API aoo_error aoo_sink_uninvite_source(aoo_sink *sink, const void *address,
                                           int32_t addrlen, aoo_id id);

// uninvite all sources (always threadsafe)
AOO_API aoo_error aoo_sink_uninvite_all(aoo_sink *sink);

// handle messages from sources (threadsafe, but not reentrant)
AOO_API aoo_error aoo_sink_handle_message(aoo_sink *sink, const char *data, int32_t n,
                                          const void *address, int32_t addrlen);

// send outgoing messages - will call the reply function (threadsafe, but not reentrant)
AOO_API aoo_error aoo_sink_send(aoo_sink *sink, aoo_sendfn fn, void *user);

// process audio (threadsafe, but not reentrant)
AOO_API aoo_error aoo_sink_process(aoo_sink *sink, aoo_sample **data,
                                   int32_t nsamples, uint64_t t);

// check for pending events (always thread safe)
AOO_API aoo_bool aoo_sink_events_available(aoo_sink *sink);

// poll events (threadsafe, but not reentrant)
// will call the event handler function one or more times
AOO_API aoo_error aoo_sink_poll_events(aoo_sink *sink, aoo_eventhandler fn, void *user);

// set/get options (always threadsafe)
AOO_API aoo_error aoo_sink_set_option(aoo_sink *sink, int32_t opt, void *p, int32_t size);

AOO_API aoo_error aoo_sink_get_option(aoo_sink *sink, int32_t opt, void *p, int32_t size);

// set/get source options (always threadsafe)
AOO_API aoo_error aoo_sink_set_sourceoption(aoo_sink *sink, const void *address, int32_t addrlen,
                                            aoo_id id, int32_t opt, void *p, int32_t size);

AOO_API aoo_error aoo_sink_get_sourceoption(aoo_sink *sink, const void *address, int32_t addrlen,
                                            aoo_id id, int32_t opt, void *p, int32_t size);

// wrapper functions for frequently used options

static inline aoo_error aoo_sink_set_id(aoo_sink *sink, aoo_id id) {
    return aoo_sink_set_option(sink, aoo_opt_id, AOO_ARG(id));
}

static inline aoo_error aoo_sink_get_id(aoo_sink *sink, aoo_id *id) {
    return aoo_sink_get_option(sink, aoo_opt_id, AOO_ARG(*id));
}

static inline aoo_error aoo_sink_reset(aoo_sink *sink) {
    return aoo_sink_set_option(sink, aoo_opt_reset, AOO_ARG_NULL);
}

static inline aoo_error aoo_sink_set_buffersize(aoo_sink *sink, int32_t n) {
    return aoo_sink_set_option(sink, aoo_opt_buffersize, AOO_ARG(n));
}

static inline aoo_error aoo_sink_get_buffersize(aoo_sink *sink, int32_t *n) {
    return aoo_sink_get_option(sink, aoo_opt_buffersize, AOO_ARG(*n));
}

static inline aoo_error aoo_sink_set_timefilter_bandwith(aoo_sink *sink, float n) {
    return aoo_sink_set_option(sink, aoo_opt_timefilter_bandwidth, AOO_ARG(n));
}

static inline aoo_error aoo_sink_get_timefilter_bandwidth(aoo_sink *sink, float *n) {
    return aoo_sink_get_option(sink, aoo_opt_timefilter_bandwidth, AOO_ARG(*n));
}

static inline aoo_error aoo_sink_set_packetsize(aoo_sink *sink, int32_t n) {
    return aoo_sink_set_option(sink, aoo_opt_packetsize, AOO_ARG(n));
}

static inline aoo_error aoo_sink_get_packetsize(aoo_sink *sink, int32_t *n) {
    return aoo_sink_get_option(sink, aoo_opt_packetsize, AOO_ARG(*n));
}

static inline aoo_error aoo_sink_set_resend_enable(aoo_sink *sink, int32_t b) {
    return aoo_sink_set_option(sink, aoo_opt_resend_enable, AOO_ARG(b));
}

static inline aoo_error aoo_sink_get_resend_enable(aoo_sink *sink, int32_t *b) {
    return aoo_sink_get_option(sink, aoo_opt_resend_enable, AOO_ARG(*b));
}

static inline aoo_error aoo_sink_set_resend_interval(aoo_sink *sink, int32_t n) {
    return aoo_sink_set_option(sink, aoo_opt_resend_interval, AOO_ARG(n));
}

static inline aoo_error aoo_sink_get_resend_interval(aoo_sink *sink, int32_t *n) {
    return aoo_sink_get_option(sink, aoo_opt_resend_interval, AOO_ARG(*n));
}

static inline aoo_error aoo_sink_set_resend_maxnumframes(aoo_sink *sink, int32_t n) {
    return aoo_sink_set_option(sink, aoo_opt_resend_maxnumframes, AOO_ARG(n));
}

static inline aoo_error aoo_sink_get_resend_maxnumframes(aoo_sink *sink, int32_t *n) {
    return aoo_sink_get_option(sink, aoo_opt_resend_maxnumframes, AOO_ARG(*n));
}

static inline aoo_error aoo_sink_set_source_timeout(aoo_sink *sink, int32_t n) {
    return aoo_sink_set_option(sink, aoo_opt_source_timeout, AOO_ARG(n));
}

static inline aoo_error aoo_sink_get_source_timeout(aoo_sink *sink, int32_t *n) {
    return aoo_sink_get_option(sink, aoo_opt_source_timeout, AOO_ARG(*n));
}

static inline aoo_error aoo_sink_reset_source(aoo_sink *sink, const void *address,
                                            int32_t addrlen, aoo_id id) {
    return aoo_sink_set_sourceoption(sink, address, addrlen, id, aoo_opt_reset, AOO_ARG_NULL);
}

static inline aoo_error aoo_sink_get_source_format(aoo_sink *sink, const void *address,
                                                 int32_t addrlen, aoo_id id, aoo_format_storage *f) {
    return aoo_sink_get_sourceoption(sink, address, addrlen, id, aoo_opt_format, AOO_ARG(*f));
}

/*//////////////////// Codec API //////////////////////////*/

#define AOO_CODEC_MAXSETTINGSIZE 256

typedef void* (*aoo_codec_new)(void);

typedef void (*aoo_codec_free)(void *);

// set the codec format.
// the 'aoo_format' struct is validated and updated accordingly.
typedef aoo_error (*aoo_codec_setformat)(
        void *,         // the encoder/decoder instance
        aoo_format *    // the format (validated and updated on success)
);

// get the codec format.
// 'aoo_format' must be large enough to hold the codec format, e.g. 'aoo_format_storage'.
// NOTE: the 'size' member of 'aoo_format' must be initialized to the format storage size.
// on success, it is updated to the actual size.
typedef aoo_error (*aoo_codec_getformat)(
        void *,         // the encoder/decoder instance
        aoo_format *    // format storage
);

// encode samples to bytes
typedef aoo_error (*aoo_codec_encode)(
        void *,             // the encoder instance
        const aoo_sample *, // input samples (interleaved)
        int32_t,            // number of samples
        char *,             // output buffer
        int32_t *           // max. buffer size (updated to actual size)
);

// decode bytes to samples
typedef aoo_error (*aoo_codec_decode)(
        void *,         // the decoder instance
        const char *,   // input bytes
        int32_t,        // input size
        aoo_sample *,   // output samples (interleaved)
        int32_t *       // max. number of samples (updated to actual number)

);

// serialize format options (everything after the 'aoo_format' header)
typedef aoo_error (*aoo_codec_serialize)(
        const aoo_format *, // source format
        char *,             // option buffer
        int32_t *           // buffer size (updated to actual size)
);

// deserialize format options (everything after the 'aoo_format' header).
// 'aoo_format' must be large enough to hold the codec options, e.g. 'aoo_format_storage'.
typedef aoo_error (*aoo_codec_deserialize)(
        const aoo_format *, // format header
        const char *,       // option buffer
        int32_t,            // buffer size
        aoo_format *        // format storage
                            // NOTE: the 'size' field must be initialized to the max.
                            // available storage size and is updated to the actual size.
);

typedef struct aoo_codec
{
    const char *name;
    // encoder
    aoo_codec_new encoder_new;
    aoo_codec_free encoder_free;
    aoo_codec_setformat encoder_setformat;
    aoo_codec_getformat encoder_getformat;
    aoo_codec_encode encoder_encode;
    // decoder
    aoo_codec_new decoder_new;
    aoo_codec_free decoder_free;
    aoo_codec_setformat decoder_setformat;
    aoo_codec_getformat decoder_getformat;
    aoo_codec_decode decoder_decode;
    // helpers
    aoo_codec_serialize serialize;
    aoo_codec_deserialize deserialize;
    void *reserved[18];

} aoo_codec;

// register an external codec plugin
AOO_API aoo_error aoo_register_codec(const char *name, const aoo_codec *codec);

// The type of 'aoo_register_codec', which gets passed to codec plugins
// to register themselves.
typedef aoo_error (*aoo_codec_registerfn)(const char *, const aoo_codec *);

// NOTE: AOO doesn't support dynamic plugin loading out of the box,
// but it is quite easy to implement on your own:
// Usually, you would put one or more codecs in a shared library
// and export a single function like the following:
//
// void aoo_setup(aoo_codec_registerfn fn, const aoo_allocator *alloc);
//
// In your host application, you would then scan directories for shared libraries,
// check if they export a function named 'aoo_setup', and if yes, called it with
// 'aoo_register_codec' and an (optional) 'aoo_allocator' instance.

#ifdef __cplusplus
} // extern "C"
#endif
