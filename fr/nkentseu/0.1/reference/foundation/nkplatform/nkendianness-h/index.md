# NkEndianness.h

`#include "NKPlatform/NkEndianness.h"`

## Types

- [nkentseu::platform::NkEndianness](../nkentseu-platform-nkendianness/index.md)

## Fonctions

### NkGetCompileTimeEndianness

```cpp
constexpr NkEndianness NkGetCompileTimeEndianness()
```

Détection d'endianness à la compilation (API NK)

### NkGetRuntimeEndianness

```cpp
NkEndianness NkGetRuntimeEndianness()
```

Détection d'endianness à l'exécution (API NK)

### NkIsLittleEndian

```cpp
constexpr bool NkIsLittleEndian()
```

Vérifier si le système est little-endian

### NkIsBigEndian

```cpp
constexpr bool NkIsBigEndian()
```

Vérifier si le système est big-endian

### ByteSwap16

```cpp
constexpr uint16_t ByteSwap16(uint16_t value)
```

Inverser les octets d'un entier 16 bits

### NkByteSwap16

```cpp
constexpr uint16_t NkByteSwap16(uint16_t value)
```

Alias NK pour ByteSwap16

### ByteSwap32

```cpp
constexpr uint32_t ByteSwap32(uint32_t value)
```

Inverser les octets d'un entier 32 bits

### NkByteSwap32

```cpp
constexpr uint32_t NkByteSwap32(uint32_t value)
```

Alias NK pour ByteSwap32

### ByteSwap64

```cpp
constexpr uint64_t ByteSwap64(uint64_t value)
```

Inverser les octets d'un entier 64 bits

### NkByteSwap64

```cpp
constexpr uint64_t NkByteSwap64(uint64_t value)
```

Alias NK pour ByteSwap64

### ByteSwap

```cpp
template <typename T> constexpr T ByteSwap(T value)
```

Byte-swap générique via template

### HostToNetwork16

```cpp
constexpr uint16_t HostToNetwork16(uint16_t value)
```

Convertir uint16 de host vers network byte order (big-endian)

### NkHostToNetwork16

```cpp
constexpr uint16_t NkHostToNetwork16(uint16_t value)
```

Alias NK pour HostToNetwork16

### HostToNetwork32

```cpp
constexpr uint32_t HostToNetwork32(uint32_t value)
```

Convertir uint32 de host vers network byte order (big-endian)

### NkHostToNetwork32

```cpp
constexpr uint32_t NkHostToNetwork32(uint32_t value)
```

Alias NK pour HostToNetwork32

### HostToNetwork64

```cpp
constexpr uint64_t HostToNetwork64(uint64_t value)
```

Convertir uint64 de host vers network byte order (big-endian)

### NkHostToNetwork64

```cpp
constexpr uint64_t NkHostToNetwork64(uint64_t value)
```

Alias NK pour HostToNetwork64

### NetworkToHost16

```cpp
constexpr uint16_t NetworkToHost16(uint16_t value)
```

Convertir uint16 de network vers host byte order

### NkNetworkToHost16

```cpp
constexpr uint16_t NkNetworkToHost16(uint16_t value)
```

Alias NK pour NetworkToHost16

### NetworkToHost32

```cpp
constexpr uint32_t NetworkToHost32(uint32_t value)
```

Convertir uint32 de network vers host byte order

### NkNetworkToHost32

```cpp
constexpr uint32_t NkNetworkToHost32(uint32_t value)
```

Alias NK pour NetworkToHost32

### NetworkToHost64

```cpp
constexpr uint64_t NetworkToHost64(uint64_t value)
```

Convertir uint64 de network vers host byte order

### NkNetworkToHost64

```cpp
constexpr uint64_t NkNetworkToHost64(uint64_t value)
```

Alias NK pour NetworkToHost64

### ToLittleEndian

```cpp
template <typename T> constexpr T ToLittleEndian(T value)
```

Convertir une valeur vers little-endian

### FromLittleEndian

```cpp
template <typename T> constexpr T FromLittleEndian(T value)
```

Convertir une valeur depuis little-endian

### ToBigEndian

```cpp
template <typename T> constexpr T ToBigEndian(T value)
```

Convertir une valeur vers big-endian

### FromBigEndian

```cpp
template <typename T> constexpr T FromBigEndian(T value)
```

Convertir une valeur depuis big-endian

### ByteSwapBuffer

```cpp
template <typename T> void ByteSwapBuffer(T *data, size_t count)
```

Inverser les octets de chaque élément d'un buffer

### BufferToLittleEndian

```cpp
template <typename T> void BufferToLittleEndian(T *data, size_t count)
```

Convertir un buffer vers little-endian

### BufferFromLittleEndian

```cpp
template <typename T> void BufferFromLittleEndian(T *data, size_t count)
```

Convertir un buffer depuis little-endian

### BufferToBigEndian

```cpp
template <typename T> void BufferToBigEndian(T *data, size_t count)
```

Convertir un buffer vers big-endian

### BufferFromBigEndian

```cpp
template <typename T> void BufferFromBigEndian(T *data, size_t count)
```

Convertir un buffer depuis big-endian

### ReadUnaligned16

```cpp
uint16_t ReadUnaligned16(const void *ptr)
```

Lire un uint16 depuis une adresse potentiellement non-alignée

### ReadUnaligned32

```cpp
uint32_t ReadUnaligned32(const void *ptr)
```

Lire un uint32 depuis une adresse potentiellement non-alignée

### ReadUnaligned64

```cpp
uint64_t ReadUnaligned64(const void *ptr)
```

Lire un uint64 depuis une adresse potentiellement non-alignée

### WriteUnaligned16

```cpp
void WriteUnaligned16(void *ptr, uint16_t value)
```

Écrire un uint16 vers une adresse potentiellement non-alignée

### WriteUnaligned32

```cpp
void WriteUnaligned32(void *ptr, uint32_t value)
```

Écrire un uint32 vers une adresse potentiellement non-alignée

### WriteUnaligned64

```cpp
void WriteUnaligned64(void *ptr, uint64_t value)
```

Écrire un uint64 vers une adresse potentiellement non-alignée

### ReadLE16

```cpp
uint16_t ReadLE16(const void *ptr)
```

Lire un uint16 little-endian depuis mémoire non-alignée

### ReadLE32

```cpp
uint32_t ReadLE32(const void *ptr)
```

Lire un uint32 little-endian depuis mémoire non-alignée

### ReadLE64

```cpp
uint64_t ReadLE64(const void *ptr)
```

Lire un uint64 little-endian depuis mémoire non-alignée

### ReadBE16

```cpp
uint16_t ReadBE16(const void *ptr)
```

Lire un uint16 big-endian depuis mémoire non-alignée

### ReadBE32

```cpp
uint32_t ReadBE32(const void *ptr)
```

Lire un uint32 big-endian depuis mémoire non-alignée

### ReadBE64

```cpp
uint64_t ReadBE64(const void *ptr)
```

Lire un uint64 big-endian depuis mémoire non-alignée

### WriteLE16

```cpp
void WriteLE16(void *ptr, uint16_t value)
```

Écrire un uint16 little-endian vers mémoire non-alignée

### WriteLE32

```cpp
void WriteLE32(void *ptr, uint32_t value)
```

Écrire un uint32 little-endian vers mémoire non-alignée

### WriteLE64

```cpp
void WriteLE64(void *ptr, uint64_t value)
```

Écrire un uint64 little-endian vers mémoire non-alignée

### WriteBE16

```cpp
void WriteBE16(void *ptr, uint16_t value)
```

Écrire un uint16 big-endian vers mémoire non-alignée

### WriteBE32

```cpp
void WriteBE32(void *ptr, uint32_t value)
```

Écrire un uint32 big-endian vers mémoire non-alignée

### WriteBE64

```cpp
void WriteBE64(void *ptr, uint64_t value)
```

Écrire un uint64 big-endian vers mémoire non-alignée

## Macros

### NKENTSEU_FORCE_INLINE

```cpp
#define NKENTSEU_FORCE_INLINE
```



### NK_LITTLE_ENDIAN

```cpp
#define NK_LITTLE_ENDIAN
```

Macro indiquant si la plateforme est little-endian

### NK_BIG_ENDIAN

```cpp
#define NK_BIG_ENDIAN
```

Macro indiquant si la plateforme est big-endian

### NK_ENDIAN_UNKNOWN

```cpp
#define NK_ENDIAN_UNKNOWN
```

Macro indiquant une endianness inconnue

### NK_BSWAP16

```cpp
#define NK_BSWAP16(x)
```

Alias court pour ByteSwap16

### NK_BSWAP32

```cpp
#define NK_BSWAP32(x)
```

Alias court pour ByteSwap32

### NK_BSWAP64

```cpp
#define NK_BSWAP64(x)
```

Alias court pour ByteSwap64

### NK_HTON16

```cpp
#define NK_HTON16(x)
```

Alias court pour HostToNetwork16 (équivalent htons)

### NK_HTON32

```cpp
#define NK_HTON32(x)
```

Alias court pour HostToNetwork32 (équivalent htonl)

### NK_HTON64

```cpp
#define NK_HTON64(x)
```

Alias court pour HostToNetwork64

### NK_NTOH16

```cpp
#define NK_NTOH16(x)
```

Alias court pour NetworkToHost16 (équivalent ntohs)

### NK_NTOH32

```cpp
#define NK_NTOH32(x)
```

Alias court pour NetworkToHost32 (équivalent ntohl)

### NK_NTOH64

```cpp
#define NK_NTOH64(x)
```

Alias court pour NetworkToHost64

