/* Copyright (c) 2010-Now Christof Ressi, Winfried Ritsch and others. 
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */


#include "aoo_common.hpp"

#include "aoo/codec/aoo_pcm.h"
#if USE_CODEC_OPUS
#include "aoo/codec/aoo_opus.h"
#endif

#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#define CLAMP(x, a, b) ((x) < (a) ? (a) : (x) > (b) ? (b) : (x))

/*/////////////////////////// helper functions ///////////////////////////////////////*/

int address_to_atoms(const aoo::ip_address& addr, int argc, t_atom *a)
{
    if (argc < 2){
        return 0;
    }
    SETSYMBOL(a, gensym(addr.name()));
    SETFLOAT(a + 1, addr.port());
    return 2;
}

int endpoint_to_atoms(const aoo::ip_address& addr, AooId id, int argc, t_atom *argv)
{
    if (argc < 3 || !addr.valid()){
        return 0;
    }
    SETSYMBOL(argv, gensym(addr.name()));
    SETFLOAT(argv + 1, addr.port());
    SETFLOAT(argv + 2, id);
    return 3;
}

static bool getarg(const char *name, void *x, int which,
                      int argc, const t_atom *argv, t_float &f, t_float def)
{
    if (argc > which){
        if (argv[which].a_type == A_SYMBOL){
            t_symbol *sym = argv[which].a_w.w_symbol;
            if (sym == gensym("auto")){
                f = def;
            } else {
                pd_error(x, "%s: bad '%s' argument '%s'", classname(x), name, sym->s_name);
                return false;
            }
        } else {
            f = atom_getfloat(argv + which);
        }
    } else {
        f = def;
    }
    return true;
}

void format_makedefault(AooFormatStorage &f, int nchannels)
{
    auto& fmt = (AooFormatPcm &)f;
    fmt.header.codec = kAooCodecPcm;
    fmt.header.size = sizeof(AooFormatPcm);
    fmt.header.blockSize = 64;
    fmt.header.sampleRate = sys_getsr();
    fmt.header.numChannels = nchannels;
    fmt.bitDepth = kAooPcmFloat32;
}

static int32_t format_getparam(void *x, int argc, t_atom *argv, int which,
                               const char *name, int32_t def)
{
    if (argc > which){
        if (argv[which].a_type == A_FLOAT){
            return argv[which].a_w.w_float;
        }
    #if 1
        t_symbol *s = atom_getsymbol(argv + which);
        if (s != gensym("auto")){
            pd_error(x, "%s: bad %s argument %s, using %d", classname(x), name, s->s_name, def);
        }
    #endif
    }
    return def;
}

bool format_parse(t_pd *x, AooFormatStorage &f, int argc, t_atom *argv,
                  int maxnumchannels)
{
    t_symbol *codec = atom_getsymbolarg(0, argc, argv);

    if (codec == gensym(kAooCodecPcm)){
        auto& fmt = (AooFormatPcm &)f;
        fmt.header.codec = kAooCodecPcm;
        fmt.header.size = sizeof(AooFormatPcm);
        fmt.header.numChannels = format_getparam(x, argc, argv, 1, "channels", maxnumchannels);
        fmt.header.blockSize = format_getparam(x, argc, argv, 2, "blocksize", 64);
        fmt.header.sampleRate = format_getparam(x, argc, argv, 3, "samplerate", sys_getsr());

        int bitdepth = format_getparam(x, argc, argv, 4, "bitdepth", 4);
        switch (bitdepth){
        case 2:
            fmt.bitDepth = kAooPcmInt16;
            break;
        case 3:
            fmt.bitDepth = kAooPcmInt24;
            break;
        case 4:
            fmt.bitDepth = kAooPcmFloat32;
            break;
        case 8:
            fmt.bitDepth = kAooPcmFloat64;
            break;
        default:
            pd_error(x, "%s: bad bitdepth argument %d", classname(x), bitdepth);
            return false;
        }
    }
#if USE_CODEC_OPUS
    else if (codec == gensym(kAooCodecOpus)){
        auto &fmt = (AooFormatOpus &)f;
        fmt.header.codec = kAooCodecOpus;
        fmt.header.size = sizeof(AooFormatOpus);
        fmt.header.numChannels = format_getparam(x, argc, argv, 1, "channels", maxnumchannels);
        fmt.header.blockSize = format_getparam(x, argc, argv, 2, "blocksize", 480); // 10ms
        fmt.header.sampleRate = format_getparam(x, argc, argv, 3, "samplerate", 48000);

        // bitrate ("auto", "max" or float)
        if (argc > 4){
            if (argv[4].a_type == A_SYMBOL){
                t_symbol *sym = argv[4].a_w.w_symbol;
                if (sym == gensym("auto")){
                    fmt.bitrate = OPUS_AUTO;
                } else if (sym == gensym("max")){
                    fmt.bitrate = OPUS_BITRATE_MAX;
                } else {
                    pd_error(x, "%s: bad bitrate argument '%s'", classname(x), sym->s_name);
                    return false;
                }
            } else {
                int bitrate = atom_getfloat(argv + 4);
                if (bitrate > 0){
                    fmt.bitrate = bitrate;
                } else {
                    pd_error(x, "%s: bitrate argument %d out of range", classname(x), bitrate);
                    return false;
                }
            }
        } else {
            fmt.bitrate = OPUS_AUTO;
        }
        // complexity ("auto" or 0-10)
        int complexity = format_getparam(x, argc, argv, 5, "complexity", OPUS_AUTO);
        if ((complexity < 0 || complexity > 10) && complexity != OPUS_AUTO){
            pd_error(x, "%s: complexity value %d out of range", classname(x), complexity);
            return false;
        }
        fmt.complexity = complexity;
        // signal type ("auto", "music", "voice")
        if (argc > 6){
            t_symbol *type = atom_getsymbol(argv + 6);
            if (type == gensym("auto")){
                fmt.signalType = OPUS_AUTO;
            } else if (type == gensym("music")){
                fmt.signalType = OPUS_SIGNAL_MUSIC;
            } else if (type == gensym("voice")){
                fmt.signalType = OPUS_SIGNAL_VOICE;
            } else {
                pd_error(x,"%s: unsupported signal type '%s'",
                         classname(x), type->s_name);
                return false;
            }
        } else {
            fmt.signalType = OPUS_AUTO;
        }
        // application type ("auto", "audio", "voip", "lowdelay"
        if (argc > 7){
            t_symbol *type = atom_getsymbol(argv + 7);
            if (type == gensym("auto") || type == gensym("audio")){
                fmt.applicationType = OPUS_APPLICATION_AUDIO;
            } else if (type == gensym("voip")){
                fmt.applicationType = OPUS_APPLICATION_VOIP;
            } else if (type == gensym("lowdelay")){
                fmt.applicationType = OPUS_APPLICATION_RESTRICTED_LOWDELAY;
            } else {
                pd_error(x,"%s: unsupported application type '%s'",
                         classname(x), type->s_name);
                return false;
            }
        } else {
            fmt.applicationType = OPUS_APPLICATION_AUDIO;
        }
    }
#endif
    else {
        pd_error(x, "%s: unknown codec '%s'", classname(x), codec->s_name);
        return false;
    }
    return true;
}

int format_to_atoms(const AooFormat &f, int argc, t_atom *argv)
{
    if (argc < 3){
        error("AooFormat_toatoms: too few atoms!");
        return 0;
    }
    t_symbol *codec = gensym(f.codec);
    SETSYMBOL(argv, codec);
    SETFLOAT(argv + 1, f.numChannels);
    SETFLOAT(argv + 2, f.blockSize);
    SETFLOAT(argv + 3, f.sampleRate);

    if (codec == gensym(kAooCodecPcm)){
        // pcm <blocksize> <samplerate> <channels> <bitdepth>
        if (argc < 5){
            error("format_to_atoms: too few atoms for pcm format!");
            return 0;
        }
        auto& fmt = (AooFormatPcm &)f;
        int nbits;
        switch (fmt.bitDepth){
        case kAooPcmInt16:
            nbits = 2;
            break;
        case kAooPcmInt24:
            nbits = 3;
            break;
        case kAooPcmFloat32:
            nbits = 4;
            break;
        case kAooPcmFloat64:
            nbits = 8;
            break;
        default:
            nbits = 0;
        }
        SETFLOAT(argv + 4, nbits);
        return 5;
    }
#if USE_CODEC_OPUS
    else if (codec == gensym(kAooCodecOpus)){
        // opus <blocksize> <samplerate> <channels> <bitrate> <complexity> <signaltype> <application>
        if (argc < 8){
            error("format_to_atoms: too few atoms for opus format!");
            return 0;
        }
        auto& fmt = (AooFormatOpus &)f;
    #if 0
        SETFLOAT(argv + 4, fmt.bitrate);
    #else
        // workaround for bug in opus_multistream_encoder (as of opus v1.3.2)
        // where OPUS_GET_BITRATE would always return OPUS_AUTO.
        // We have no chance to get the actual bitrate for "auto" and "max",
        // so we return the symbols instead.
        switch (fmt.bitrate){
        case OPUS_AUTO:
            SETSYMBOL(argv + 4, gensym("auto"));
            break;
        case OPUS_BITRATE_MAX:
            SETSYMBOL(argv + 4, gensym("max"));
            break;
        default:
            SETFLOAT(argv + 4, fmt.bitrate);
            break;
        }
    #endif
        // complexity
        SETFLOAT(argv + 5, fmt.complexity);
        // signal type
        t_symbol *signaltype;
        switch (fmt.signalType){
        case OPUS_SIGNAL_MUSIC:
            signaltype = gensym("music");
            break;
        case OPUS_SIGNAL_VOICE:
            signaltype = gensym("voice");
            break;
        default:
            signaltype = gensym("auto");
            break;
        }
        SETSYMBOL(argv + 6, signaltype);
        // application type
        t_symbol *apptype;
        switch (fmt.applicationType){
        case OPUS_APPLICATION_VOIP:
            apptype = gensym("voip");
            break;
        case OPUS_APPLICATION_RESTRICTED_LOWDELAY:
            apptype = gensym("lowdelay");
            break;
        default:
            apptype = gensym("audio");
            break;
        }
        SETSYMBOL(argv + 7, apptype);
        return 8;
    }
#endif
    else {
        error("format_to_atoms: unknown format %s!", codec->s_name);
    }
    return 0;
}
