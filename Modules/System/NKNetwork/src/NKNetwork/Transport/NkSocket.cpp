// =============================================================================
// NKNetwork/Transport/NkSocket.cpp
// =============================================================================
#include "NkSocket.h"
#include <cstring>

#if defined(NK_NET_PLATFORM_WINDOWS)
#   include <winsock2.h>
#   include <ws2tcpip.h>
    static int NkGetSockErr() { return WSAGetLastError(); }
    static bool NkWouldBlock(int err) { return err == WSAEWOULDBLOCK; }
#else
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#   include <unistd.h>
#   include <fcntl.h>
#   include <netdb.h>
#   include <errno.h>
    static int NkGetSockErr() { return errno; }
    static bool NkWouldBlock(int err) {
        return err == EAGAIN || err == EWOULDBLOCK;
    }
#endif

namespace nkentseu {
    namespace net {

        // =====================================================================
        // NkAddress implementation
        // =====================================================================
        NkAddress::NkAddress(const char* host, uint16 port) noexcept : mPort(port) {
            if (!host) return;
            if (std::strcmp(host, "0.0.0.0") == 0 ||
                std::strcmp(host, "any") == 0) {
                mIP.ipv4[0] = mIP.ipv4[1] = mIP.ipv4[2] = mIP.ipv4[3] = 0;
                mFamily = Family::IPv4; mValid = true; return;
            }
            if (std::strcmp(host, "localhost") == 0 ||
                std::strcmp(host, "127.0.0.1") == 0) {
                mIP.ipv4[0]=127; mIP.ipv4[1]=0; mIP.ipv4[2]=0; mIP.ipv4[3]=1;
                mFamily = Family::IPv4; mValid = true; return;
            }
            // IPv4 dotted
            uint32 a,b,c,d;
            if (sscanf(host, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
                mIP.ipv4[0]=(uint8)a; mIP.ipv4[1]=(uint8)b;
                mIP.ipv4[2]=(uint8)c; mIP.ipv4[3]=(uint8)d;
                mFamily = Family::IPv4; mValid = true; return;
            }
            // TODO: IPv6 parsing
            mValid = false;
        }

        NkAddress::NkAddress(uint8 a, uint8 b, uint8 c, uint8 d,
                               uint16 port) noexcept : mPort(port) {
            mIP.ipv4[0]=a; mIP.ipv4[1]=b; mIP.ipv4[2]=c; mIP.ipv4[3]=d;
            mFamily = Family::IPv4; mValid = true;
        }

#if defined(NK_NET_PLATFORM_WINDOWS) || defined(NK_NET_PLATFORM_POSIX)
        NkAddress::NkAddress(const sockaddr_in& addr) noexcept {
            mFamily = Family::IPv4;
            mPort   = ntohs(addr.sin_port);
            const uint8* ip = reinterpret_cast<const uint8*>(&addr.sin_addr.s_addr);
            mIP.ipv4[0]=ip[0]; mIP.ipv4[1]=ip[1];
            mIP.ipv4[2]=ip[2]; mIP.ipv4[3]=ip[3];
            mValid = true;
        }

        void NkAddress::ToSockAddr(sockaddr_storage& out, socklen_t& len) const noexcept {
            std::memset(&out, 0, sizeof(out));
            if (mFamily == Family::IPv4) {
                auto* a4 = reinterpret_cast<sockaddr_in*>(&out);
                a4->sin_family = AF_INET;
                a4->sin_port   = htons(mPort);
                std::memcpy(&a4->sin_addr.s_addr, mIP.ipv4, 4);
                len = sizeof(sockaddr_in);
            }
        }
#endif

        NkAddress NkAddress::Loopback(uint16 port, Family f) noexcept {
            return NkAddress(127, 0, 0, 1, port);
        }

        NkAddress NkAddress::Any(uint16 port, Family) noexcept {
            return NkAddress(0, 0, 0, 0, port);
        }

        NkString NkAddress::ToString() const noexcept {
            char buf[64];
            if (mFamily == Family::IPv4) {
                snprintf(buf, sizeof(buf), "%u.%u.%u.%u:%u",
                    mIP.ipv4[0], mIP.ipv4[1], mIP.ipv4[2], mIP.ipv4[3], mPort);
            } else {
                snprintf(buf, sizeof(buf), "[ipv6]:%u", mPort);
            }
            return NkString(buf);
        }

        bool NkAddress::operator==(const NkAddress& o) const noexcept {
            if (mFamily != o.mFamily || mPort != o.mPort) return false;
            if (mFamily == Family::IPv4)
                return std::memcmp(mIP.ipv4, o.mIP.ipv4, 4) == 0;
            return std::memcmp(mIP.ipv6, o.mIP.ipv6, 16) == 0;
        }

        // =====================================================================
        // NkSocket implementation
        // =====================================================================
        NkSocket::NkSocket(NkSocket&& o) noexcept
            : mHandle(o.mHandle), mType(o.mType), mLocalAddr(o.mLocalAddr) {
            o.mHandle = kNkInvalidSocket;
        }

        NkSocket& NkSocket::operator=(NkSocket&& o) noexcept {
            if (this != &o) {
                Close();
                mHandle    = o.mHandle;
                mType      = o.mType;
                mLocalAddr = o.mLocalAddr;
                o.mHandle  = kNkInvalidSocket;
            }
            return *this;
        }

        NkNetResult NkSocket::Create(const NkAddress& localAddr, Type type) noexcept {
            Close();
            mType      = type;
            mLocalAddr = localAddr;

            const int af      = AF_INET;  // IPv4 only pour l'instant
            const int stype   = (type == Type::UDP) ? SOCK_DGRAM  : SOCK_STREAM;
            const int proto   = (type == Type::UDP) ? IPPROTO_UDP : IPPROTO_TCP;

            mHandle = ::socket(af, stype, proto);
            if (mHandle == kNkInvalidSocket) {
                NK_NET_LOG_ERROR("socket() failed: %d", NkGetSockErr());
                return NkNetResult::SocketError;
            }

            // Bind
            sockaddr_storage ss;
            socklen_t ssLen;
            localAddr.ToSockAddr(ss, ssLen);

            if (::bind(mHandle, reinterpret_cast<sockaddr*>(&ss), ssLen) != 0) {
                int err = NkGetSockErr();
                NK_NET_LOG_ERROR("bind() failed: %d", err);
                Close();
                return NkNetResult::SocketError;
            }

            SetSendBufferSize(kNkSendBufferSize);
            SetRecvBufferSize(kNkRecvBufferSize);
            return NkNetResult::OK;
        }

        void NkSocket::Close() noexcept {
            if (mHandle != kNkInvalidSocket) {
                NK_NET_CLOSE_SOCKET(mHandle);
                mHandle = kNkInvalidSocket;
            }
        }

        NkNetResult NkSocket::SetNonBlocking(bool v) noexcept {
            if (!IsValid()) return NkNetResult::SocketError;
#if defined(NK_NET_PLATFORM_WINDOWS)
            u_long mode = v ? 1 : 0;
            if (ioctlsocket(mHandle, FIONBIO, &mode) != 0)
                return NkNetResult::SocketError;
#else
            int flags = ::fcntl(mHandle, F_GETFL, 0);
            if (v) flags |= O_NONBLOCK;
            else   flags &= ~O_NONBLOCK;
            if (::fcntl(mHandle, F_SETFL, flags) < 0)
                return NkNetResult::SocketError;
#endif
            mNonBlocking = v;
            return NkNetResult::OK;
        }

        NkNetResult NkSocket::SetBroadcast(bool v) noexcept {
            int opt = v ? 1 : 0;
            if (::setsockopt(mHandle, SOL_SOCKET, SO_BROADCAST,
                              reinterpret_cast<const char*>(&opt), sizeof(opt)) != 0)
                return NkNetResult::SocketError;
            return NkNetResult::OK;
        }

        NkNetResult NkSocket::SetNoDelay(bool v) noexcept {
            int opt = v ? 1 : 0;
            if (::setsockopt(mHandle, IPPROTO_TCP, TCP_NODELAY,
                              reinterpret_cast<const char*>(&opt), sizeof(opt)) != 0)
                return NkNetResult::SocketError;
            return NkNetResult::OK;
        }

        NkNetResult NkSocket::SetReuseAddr(bool v) noexcept {
            int opt = v ? 1 : 0;
            if (::setsockopt(mHandle, SOL_SOCKET, SO_REUSEADDR,
                              reinterpret_cast<const char*>(&opt), sizeof(opt)) != 0)
                return NkNetResult::SocketError;
            return NkNetResult::OK;
        }

        NkNetResult NkSocket::SetSendBufferSize(uint32 bytes) noexcept {
            int sz = (int)bytes;
            ::setsockopt(mHandle, SOL_SOCKET, SO_SNDBUF,
                          reinterpret_cast<const char*>(&sz), sizeof(sz));
            return NkNetResult::OK;
        }

        NkNetResult NkSocket::SetRecvBufferSize(uint32 bytes) noexcept {
            int sz = (int)bytes;
            ::setsockopt(mHandle, SOL_SOCKET, SO_RCVBUF,
                          reinterpret_cast<const char*>(&sz), sizeof(sz));
            return NkNetResult::OK;
        }

        NkNetResult NkSocket::SendTo(const void* data, uint32 size,
                                      const NkAddress& to) noexcept {
            if (!IsValid() || !data || size == 0) return NkNetResult::InvalidArg;
            if (size > kNkMaxPacketSize) return NkNetResult::PacketTooLarge;

            sockaddr_storage ss; socklen_t ssLen;
            to.ToSockAddr(ss, ssLen);

            const int sent = ::sendto(mHandle,
                                       reinterpret_cast<const char*>(data),
                                       (int)size, 0,
                                       reinterpret_cast<sockaddr*>(&ss), ssLen);
            if (sent == NK_NET_SOCKET_ERROR) {
                const int err = NkGetSockErr();
                if (NkWouldBlock(err)) return NkNetResult::BufferFull;
                return NkNetResult::SocketError;
            }
            return NkNetResult::OK;
        }

        NkNetResult NkSocket::RecvFrom(void* buf, uint32 bufSize,
                                        uint32& outSize, NkAddress& outFrom) noexcept {
            if (!IsValid() || !buf) return NkNetResult::InvalidArg;

            sockaddr_storage ss; socklen_t ssLen = sizeof(ss);
            const int recvd = ::recvfrom(mHandle,
                                          reinterpret_cast<char*>(buf),
                                          (int)bufSize, 0,
                                          reinterpret_cast<sockaddr*>(&ss), &ssLen);
            if (recvd == NK_NET_SOCKET_ERROR) {
                const int err = NkGetSockErr();
                if (NkWouldBlock(err)) { outSize = 0; return NkNetResult::OK; }
                return NkNetResult::SocketError;
            }
            outSize = (uint32)recvd;
            if (ss.ss_family == AF_INET) {
                outFrom = NkAddress(reinterpret_cast<const sockaddr_in&>(ss));
            }
            return NkNetResult::OK;
        }

        NkNetResult NkSocket::Listen(uint32 backlog) noexcept {
            if (::listen(mHandle, (int)backlog) != 0) return NkNetResult::SocketError;
            return NkNetResult::OK;
        }

        NkNetResult NkSocket::Connect(const NkAddress& remote) noexcept {
            sockaddr_storage ss; socklen_t ssLen;
            remote.ToSockAddr(ss, ssLen);
            if (::connect(mHandle, reinterpret_cast<sockaddr*>(&ss), ssLen) != 0) {
#if defined(NK_NET_PLATFORM_WINDOWS)
                if (WSAGetLastError() != WSAEWOULDBLOCK)
                    return NkNetResult::SocketError;
#else
                if (errno != EINPROGRESS) return NkNetResult::SocketError;
#endif
            }
            return NkNetResult::OK;
        }

        NkNetResult NkSocket::Accept(NkSocket& outClient, NkAddress& outAddr) noexcept {
            sockaddr_storage ss; socklen_t ssLen = sizeof(ss);
            NkSocketHandle client = ::accept(mHandle,
                                              reinterpret_cast<sockaddr*>(&ss),
                                              &ssLen);
            if (client == kNkInvalidSocket) {
                int err = NkGetSockErr();
                if (NkWouldBlock(err)) return NkNetResult::OK;
                return NkNetResult::SocketError;
            }
            outClient.Close();
            outClient.mHandle = client;
            outClient.mType   = Type::TCP;
            if (ss.ss_family == AF_INET)
                outAddr = NkAddress(reinterpret_cast<sockaddr_in&>(ss));
            return NkNetResult::OK;
        }

        NkNetResult NkSocket::Send(const void* data, uint32 size) noexcept {
            const int sent = ::send(mHandle,
                                     reinterpret_cast<const char*>(data),
                                     (int)size, 0);
            if (sent == NK_NET_SOCKET_ERROR) {
                if (NkWouldBlock(NkGetSockErr())) return NkNetResult::BufferFull;
                return NkNetResult::SocketError;
            }
            return NkNetResult::OK;
        }

        NkNetResult NkSocket::Recv(void* buf, uint32 bufSize, uint32& outSize) noexcept {
            const int recvd = ::recv(mHandle, reinterpret_cast<char*>(buf),
                                      (int)bufSize, 0);
            if (recvd == NK_NET_SOCKET_ERROR) {
                if (NkWouldBlock(NkGetSockErr())) { outSize = 0; return NkNetResult::OK; }
                return NkNetResult::SocketError;
            }
            outSize = (uint32)recvd;
            return NkNetResult::OK;
        }

        NkNetResult NkSocket::PlatformInit() noexcept {
#if defined(NK_NET_PLATFORM_WINDOWS)
            WSADATA wsa;
            if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
                NK_NET_LOG_ERROR("WSAStartup failed");
                return NkNetResult::SocketError;
            }
#endif
            return NkNetResult::OK;
        }

        void NkSocket::PlatformShutdown() noexcept {
#if defined(NK_NET_PLATFORM_WINDOWS)
            WSACleanup();
#endif
        }

        NkTimestampMs NkNetNowMs() noexcept {
#if defined(NK_NET_PLATFORM_WINDOWS)
            return (NkTimestampMs)GetTickCount64();
#else
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            return (NkTimestampMs)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif
        }

        NkPeerId NkPeerId::Generate() noexcept {
            // Simple : timestamp + valeur aléatoire
            static uint64 counter = 0;
            uint64 t = NkNetNowMs();
            return { (t << 20u) ^ (++counter * 6364136223846793005ULL + 1442695040888963407ULL) };
        }

    } // namespace net
} // namespace nkentseu
