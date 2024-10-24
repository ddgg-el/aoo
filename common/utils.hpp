/* Copyright (c) 2010-Now Christof Ressi, Winfried Ritsch and others. 
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#pragma once

#include <stdint.h>

/*------------------ alloca -----------------------*/
#ifdef _WIN32
# include <malloc.h> // MSVC or mingw on windows
# ifdef _MSC_VER
#  define alloca _alloca
# endif
#elif defined(__linux__) || defined(__APPLE__)
# include <alloca.h> // linux, mac, mingw, cygwin
#else
# include <stdlib.h> // BSDs for example
#endif

/*------------------ endianess -------------------*/
    // endianess check taken from Pure Data (d_osc.c)
#if defined(__FreeBSD__) || defined(__APPLE__) || defined(__FreeBSD_kernel__) \
    || defined(__OpenBSD__)
#include <machine/endian.h>
#endif

#if defined(__linux__) || defined(__CYGWIN__) || defined(__GNU__) || \
    defined(__EMSCRIPTEN__) || \
    defined(ANDROID)
#include <endian.h>
#endif

#ifdef __MINGW32__
# include <sys/param.h>
#endif

#ifdef _MSC_VER
/* _MSVC lacks byte order macros */
 #ifndef LITTLE_ENDIAN
  #define LITTLE_ENDIAN 1234
 #endif
 #ifndef BIG_ENDIAN
  #define BIG_ENDIAN 4321
 #endif
 #define BYTE_ORDER LITTLE_ENDIAN
#endif

#if !defined(BYTE_ORDER)
 #error No byte order defined
#endif

namespace aoo {

// simple scope guard class that relies on CTAD (since C++17)
template<typename T>
class scope_guard {
public:
    scope_guard(const T& fn)
        : fn_(fn) {
        static_assert(noexcept(fn()), "scope_guard function must be noexcept!");
    }

    ~scope_guard() {
        if (active_) fn_();
    }

    void dismiss() { active_ = false; }
private:
    T fn_;
    bool active_ = true;
};

template<typename T>
constexpr bool is_pow2(T i){
    return (i & (i - 1)) == 0;
}

template<typename T, typename B>
T from_bytes(const B *b){
    static_assert(sizeof(B) == 1, "from_bytes() expects byte argument");
    union {
        T t;
        uint8_t b[sizeof(T)];
    } c;
#if BYTE_ORDER == BIG_ENDIAN
    memcpy(c.b, b, sizeof(T));
#else
    for (size_t i = 0; i < sizeof(T); ++i){
        c.b[i] = b[sizeof(T) - i - 1];
    }
#endif
    return c.t;
}

template<typename T, typename B>
T read_bytes(const B *& b){
    auto pos = b;
    b += sizeof(T);
    return aoo::from_bytes<T, B>(pos);
}

template<typename T, typename B>
void to_bytes(T v, B *b){
    static_assert(sizeof(B) == 1, "to_bytes() expects byte argument");
    union {
        T t;
        uint8_t b[sizeof(T)];
    } c;
    c.t = v;
#if BYTE_ORDER == BIG_ENDIAN
    memcpy(b, c.b, sizeof(T));
#else
    for (size_t i = 0; i < sizeof(T); ++i){
        b[i] = c.b[sizeof(T) - i - 1];
    }
#endif
}

template<typename T, typename B>
void write_bytes(T v, B *& b){
    aoo::to_bytes<T, B>(v, b);
    b += sizeof(T);
}

template<typename T>
T clamp(T in, T low, T high){
    if (in > high) return high;
    else if (in < low) return low;
    else return in;
}

} // aoo
