/**
 * @File    NkDuration.cpp
 * @Brief   Implémentation de NkDuration::ToString / ToStringPrecise.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Note Tout le reste de NkDuration est inline/constexpr dans le .h.
 *       Ce .cpp ne contient que les méthodes nécessitant snprintf
 *       (allocation NkString impossible en constexpr).
 *       <cmath> est intentionnellement absent — NkDuration n'en a pas besoin.
 */

#include "pch.h"
#include "NKTime/NkDuration.h"
#include <cstdio>

namespace nkentseu {

    using namespace time;

    NkString NkDuration::ToString() const {
        char buf[128];
        const int64 absNs = mNanoseconds < 0 ? -mNanoseconds : mNanoseconds;

        if      (absNs == 0)                   ::snprintf(buf, sizeof(buf), "0s");
        else if (absNs >= NS_PER_DAY)          ::snprintf(buf, sizeof(buf), "%.2fd",
                                                static_cast<float64>(mNanoseconds) / static_cast<float64>(NS_PER_DAY));
        else if (absNs >= NS_PER_HOUR)         ::snprintf(buf, sizeof(buf), "%.2fh",
                                                static_cast<float64>(mNanoseconds) / static_cast<float64>(NS_PER_HOUR));
        else if (absNs >= NS_PER_MINUTE)       ::snprintf(buf, sizeof(buf), "%.2fmin",
                                                static_cast<float64>(mNanoseconds) / static_cast<float64>(NS_PER_MINUTE));
        else if (absNs >= NS_PER_SECOND)       ::snprintf(buf, sizeof(buf), "%.3fs",
                                                static_cast<float64>(mNanoseconds) / static_cast<float64>(NS_PER_SECOND));
        else if (absNs >= NS_PER_MILLISECOND)  ::snprintf(buf, sizeof(buf), "%.3fms",
                                                static_cast<float64>(mNanoseconds) / static_cast<float64>(NS_PER_MILLISECOND));
        else if (absNs >= NS_PER_MICROSECOND)  ::snprintf(buf, sizeof(buf), "%.3fus",
                                                static_cast<float64>(mNanoseconds) / static_cast<float64>(NS_PER_MICROSECOND));
        else                                   ::snprintf(buf, sizeof(buf), "%lldns",
                                                static_cast<long long>(mNanoseconds));

        return NkString(buf);
    }

    NkString NkDuration::ToStringPrecise() const {
        char buf[64];
        ::snprintf(buf, sizeof(buf), "%lld ns", static_cast<long long>(mNanoseconds));
        return NkString(buf);
    }

} // namespace nkentseu