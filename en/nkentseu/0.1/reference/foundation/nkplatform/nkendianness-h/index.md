# NkEndianness.h

`#include "NKPlatform/NkEndianness.h"`

## Types

- [nkentseu::platform::NkEndianness](../nkentseu-platform-nkendianness/index.md)

## Functions

### NkGetCompileTimeEndianness

```cpp
constexpr NkEndianness NkGetCompileTimeEndianness()
```

Byte order detection at compile time (NK API)

### NkGetRuntimeEndianness

```cpp
NkEndianness NkGetRuntimeEndianness()
```

Byte order detection at run time (NK API)

### NkIsLittleEndian

```cpp
constexpr bool NkIsLittleEndian()
```

Checks whether the system is little-endian

### NkIsBigEndian

```cpp
constexpr bool NkIsBigEndian()
```

Checks whether the system is big-endian

### ByteSwap16

```cpp
constexpr uint16_t ByteSwap16(uint16_t value)
```

Reverses the bytes of a 16-bit integer

### NkByteSwap16

```cpp
constexpr uint16_t NkByteSwap16(uint16_t value)
```

NK alias for ByteSwap16

### ByteSwap32

```cpp
constexpr uint32_t ByteSwap32(uint32_t value)
```

Reverses the bytes of a 32-bit integer

### NkByteSwap32

```cpp
constexpr uint32_t NkByteSwap32(uint32_t value)
```

NK alias for ByteSwap32

### ByteSwap64

```cpp
constexpr uint64_t ByteSwap64(uint64_t value)
```

Reverses the bytes of a 64-bit integer

### NkByteSwap64

```cpp
constexpr uint64_t NkByteSwap64(uint64_t value)
```

NK alias for ByteSwap64

### ByteSwap

```cpp
template <typename T> constexpr T ByteSwap(T value)
```

Generic byte swap through a template

### HostToNetwork16

```cpp
constexpr uint16_t HostToNetwork16(uint16_t value)
```

Converts a uint16 from host to network byte order (big-endian)

### NkHostToNetwork16

```cpp
constexpr uint16_t NkHostToNetwork16(uint16_t value)
```

NK alias for HostToNetwork16

### HostToNetwork32

```cpp
constexpr uint32_t HostToNetwork32(uint32_t value)
```

Converts a uint32 from host to network byte order (big-endian)

### NkHostToNetwork32

```cpp
constexpr uint32_t NkHostToNetwork32(uint32_t value)
```

NK alias for HostToNetwork32

### HostToNetwork64

```cpp
constexpr uint64_t HostToNetwork64(uint64_t value)
```

Converts a uint64 from host to network byte order (big-endian)

### NkHostToNetwork64

```cpp
constexpr uint64_t NkHostToNetwork64(uint64_t value)
```

NK alias for HostToNetwork64

### NetworkToHost16

```cpp
constexpr uint16_t NetworkToHost16(uint16_t value)
```

Converts a uint16 from network to host byte order

### NkNetworkToHost16

```cpp
constexpr uint16_t NkNetworkToHost16(uint16_t value)
```

NK alias for NetworkToHost16

### NetworkToHost32

```cpp
constexpr uint32_t NetworkToHost32(uint32_t value)
```

Converts a uint32 from network to host byte order

### NkNetworkToHost32

```cpp
constexpr uint32_t NkNetworkToHost32(uint32_t value)
```

NK alias for NetworkToHost32

### NetworkToHost64

```cpp
constexpr uint64_t NetworkToHost64(uint64_t value)
```

Converts a uint64 from network to host byte order

### NkNetworkToHost64

```cpp
constexpr uint64_t NkNetworkToHost64(uint64_t value)
```

NK alias for NetworkToHost64

### ToLittleEndian

```cpp
template <typename T> constexpr T ToLittleEndian(T value)
```

Converts a value to little-endian

### FromLittleEndian

```cpp
template <typename T> constexpr T FromLittleEndian(T value)
```

Converts a value from little-endian

### ToBigEndian

```cpp
template <typename T> constexpr T ToBigEndian(T value)
```

Converts a value to big-endian

### FromBigEndian

```cpp
template <typename T> constexpr T FromBigEndian(T value)
```

Converts a value from big-endian

### ByteSwapBuffer

```cpp
template <typename T> void ByteSwapBuffer(T *data, size_t count)
```

Reverses the bytes of every element of a buffer

### BufferToLittleEndian

```cpp
template <typename T> void BufferToLittleEndian(T *data, size_t count)
```

Converts a buffer to little-endian

### BufferFromLittleEndian

```cpp
template <typename T> void BufferFromLittleEndian(T *data, size_t count)
```

Converts a buffer from little-endian

### BufferToBigEndian

```cpp
template <typename T> void BufferToBigEndian(T *data, size_t count)
```

Converts a buffer to big-endian

### BufferFromBigEndian

```cpp
template <typename T> void BufferFromBigEndian(T *data, size_t count)
```

Converts a buffer from big-endian

### ReadUnaligned16

```cpp
uint16_t ReadUnaligned16(const void *ptr)
```

Reads a uint16 from a possibly unaligned address

### ReadUnaligned32

```cpp
uint32_t ReadUnaligned32(const void *ptr)
```

Reads a uint32 from a possibly unaligned address

### ReadUnaligned64

```cpp
uint64_t ReadUnaligned64(const void *ptr)
```

Reads a uint64 from a possibly unaligned address

### WriteUnaligned16

```cpp
void WriteUnaligned16(void *ptr, uint16_t value)
```

Writes a uint16 to a possibly unaligned address

### WriteUnaligned32

```cpp
void WriteUnaligned32(void *ptr, uint32_t value)
```

Writes a uint32 to a possibly unaligned address

### WriteUnaligned64

```cpp
void WriteUnaligned64(void *ptr, uint64_t value)
```

Writes a uint64 to a possibly unaligned address

### ReadLE16

```cpp
uint16_t ReadLE16(const void *ptr)
```

Reads a little-endian uint16 from unaligned memory

### ReadLE32

```cpp
uint32_t ReadLE32(const void *ptr)
```

Reads a little-endian uint32 from unaligned memory

### ReadLE64

```cpp
uint64_t ReadLE64(const void *ptr)
```

Reads a little-endian uint64 from unaligned memory

### ReadBE16

```cpp
uint16_t ReadBE16(const void *ptr)
```

Reads a big-endian uint16 from unaligned memory

### ReadBE32

```cpp
uint32_t ReadBE32(const void *ptr)
```

Reads a big-endian uint32 from unaligned memory

### ReadBE64

```cpp
uint64_t ReadBE64(const void *ptr)
```

Reads a big-endian uint64 from unaligned memory

### WriteLE16

```cpp
void WriteLE16(void *ptr, uint16_t value)
```

Writes a little-endian uint16 to unaligned memory

### WriteLE32

```cpp
void WriteLE32(void *ptr, uint32_t value)
```

Writes a little-endian uint32 to unaligned memory

### WriteLE64

```cpp
void WriteLE64(void *ptr, uint64_t value)
```

Writes a little-endian uint64 to unaligned memory

### WriteBE16

```cpp
void WriteBE16(void *ptr, uint16_t value)
```

Writes a big-endian uint16 to unaligned memory

### WriteBE32

```cpp
void WriteBE32(void *ptr, uint32_t value)
```

Writes a big-endian uint32 to unaligned memory

### WriteBE64

```cpp
void WriteBE64(void *ptr, uint64_t value)
```

Writes a big-endian uint64 to unaligned memory

## Macros

### NKENTSEU_FORCE_INLINE

```cpp
#define NKENTSEU_FORCE_INLINE
```



### NK_LITTLE_ENDIAN

```cpp
#define NK_LITTLE_ENDIAN
```

Macro telling whether the platform is little-endian

### NK_BIG_ENDIAN

```cpp
#define NK_BIG_ENDIAN
```

Macro telling whether the platform is big-endian

### NK_ENDIAN_UNKNOWN

```cpp
#define NK_ENDIAN_UNKNOWN
```

Macro signalling an unknown byte order

### NK_BSWAP16

```cpp
#define NK_BSWAP16(x)
```

Short alias for ByteSwap16

### NK_BSWAP32

```cpp
#define NK_BSWAP32(x)
```

Short alias for ByteSwap32

### NK_BSWAP64

```cpp
#define NK_BSWAP64(x)
```

Short alias for ByteSwap64

### NK_HTON16

```cpp
#define NK_HTON16(x)
```

Short alias for HostToNetwork16 (htons equivalent)

### NK_HTON32

```cpp
#define NK_HTON32(x)
```

Short alias for HostToNetwork32 (htonl equivalent)

### NK_HTON64

```cpp
#define NK_HTON64(x)
```

Short alias for HostToNetwork64

### NK_NTOH16

```cpp
#define NK_NTOH16(x)
```

Short alias for NetworkToHost16 (ntohs equivalent)

### NK_NTOH32

```cpp
#define NK_NTOH32(x)
```

Short alias for NetworkToHost32 (ntohl equivalent)

### NK_NTOH64

```cpp
#define NK_NTOH64(x)
```

Short alias for NetworkToHost64

