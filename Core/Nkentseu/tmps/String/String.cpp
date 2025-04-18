#include "pch.h"
#include "String.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <locale>
#include <cwctype>

namespace nkentseu
{
    // std::ostream& operator<<(std::ostream& os, const String& str) {
    //     if constexpr (std::is_same_v<char8, char>) {
    //         return os.write(str.Data(), str.Length());
    //     } else if constexpr (sizeof(char8) == sizeof(char8)) {
    //         return os.write(reinterpret_cast<const char8*>(str.Data()), str.Length());
    //     } else {
    //         // Fallback: affichage caractère par caractère
    //         for (usize i = 0; i < str.Length(); ++i) {
    //             os.put(static_cast<char8>(str.Data()[i]));
    //         }
    //         return os;
    //     }
    // }
} // namespace nkentseu
