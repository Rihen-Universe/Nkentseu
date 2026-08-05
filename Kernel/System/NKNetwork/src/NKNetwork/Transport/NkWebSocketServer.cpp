// -----------------------------------------------------------------------------
// NkWebSocketServer.cpp — RFC 6455, cote serveur, perimetre « IDE local ».
// -----------------------------------------------------------------------------
#include "NKNetwork/Transport/NkWebSocketServer.h"
#include "NKContainers/String/NkStringHash.h"			  // NkHashSHA1
#include "NKContainers/String/Encoding/NkBase64.h"		  // NkBase64::NkEncode

namespace nkentseu {
	namespace net {

		namespace {
			// GUID impose par la RFC 6455 §4.2.2 : concatene a la cle du client avant
			// le SHA-1. C'est ce qui prouve au client qu'on parle bien WebSocket et
			// non un serveur HTTP quelconque qui repondrait 101 par hasard.
			const char *const kWsGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

			bool EqualNoCase(const char *a, const char *b, usize n) {
				for (usize i = 0; i < n; ++i) {
					char ca = a[i], cb = b[i];
					if (ca >= 'A' && ca <= 'Z')
						ca = static_cast<char>(ca + 32);
					if (cb >= 'A' && cb <= 'Z')
						cb = static_cast<char>(cb + 32);
					if (ca != cb)
						return false;
				}
				return true;
			}
		} // namespace

		NkWebSocketServer::~NkWebSocketServer() noexcept {
			Stop();
		}

		bool NkWebSocketServer::Start(uint16 port) noexcept {
			Stop();
			// Winsock EXIGE WSAStartup avant tout appel socket ; sans lui, socket()
			// echoue en boucle. PlatformInit() s'en charge et est idempotent : on
			// l'appelle ici plutot que d'exiger de l'appelant qu'il y pense — un
			// serveur qui ne demarre pas doit etre la faute du serveur, pas d'un
			// prerequis non documente a l'endroit ou on l'utilise.
			(void)NkSocket::PlatformInit();
			// LOOPBACK uniquement : ce canal expose l'etat de l'editeur (fichier
			// ouvert, selection, diagnostics). Il n'a rien a faire sur le reseau.
			// Port 0 = « que l'OS choisisse » — MAIS NkSocket::GetLocalAddr() renvoie
			// l'adresse DEMANDEE, pas celle attribuee (pas de getsockname), et le port
			// resterait donc a 0. Or ce port doit etre publie : c'est par lui que le
			// client nous trouve. On BALAYE donc une plage et on garde le premier port
			// libre — celui qu'on connait est alors celui qu'on ecoute.
			const uint16 premier = (port != 0) ? port : static_cast<uint16>(42000);
			const uint16 dernier = (port != 0) ? port : static_cast<uint16>(42099);
			bool ouvert = false;
			for (uint16 p = premier; p <= dernier; ++p) {
				const NkAddress addr(127, 0, 0, 1, p);
				if (mListen.Create(addr, NkSocket::Type::NK_TCP) != NkNetResult::NK_NET_OK)
					continue; // port occupe : on essaie le suivant
				if (mListen.SetNonBlocking(true) != NkNetResult::NK_NET_OK || mListen.Listen(4) != NkNetResult::NK_NET_OK) {
					mListen.Close();
					continue;
				}
				mPort = p;
				ouvert = true;
				break;
			}
			if (!ouvert)
				return false;
			mRunning = true;
			return true;
		}

		void NkWebSocketServer::Stop() noexcept {
			if (mHasClient)
				mClient.Close();
			if (mRunning)
				mListen.Close();
			mRunning = false;
			mHasClient = false;
			mHandshakeDone = false;
			mPort = 0;
			mRxRaw.Clear();
			mFragment.Clear();
			mMessages.Clear();
			mHandshakeRequest.Clear();
		}

		bool NkWebSocketServer::AcceptPending() noexcept {
			if (!mRunning || mHasClient)
				return false;
			NkAddress from;
			if (mListen.Accept(mClient, from) != NkNetResult::NK_NET_OK)
				return false;
			if (mClient.SetNonBlocking(true) != NkNetResult::NK_NET_OK) {
				mClient.Close();
				return false;
			}
			mHasClient = true;
			mHandshakeDone = false;
			mRxRaw.Clear();
			mFragment.Clear();
			mHandshakeRequest.Clear();
			return true;
		}

		// Cherche la fin des en-tetes (CRLFCRLF). Tant qu'elle n'est pas la, on
		// attend : une requete HTTP peut arriver en plusieurs morceaux TCP.
		bool NkWebSocketServer::TryHandshake() noexcept {
			const usize n = mRxRaw.Size();
			usize end = 0;
			for (usize i = 3; i < n; ++i)
				if (mRxRaw[i - 3] == '\r' && mRxRaw[i - 2] == '\n' && mRxRaw[i - 1] == '\r' && mRxRaw[i] == '\n') {
					end = i + 1;
					break;
				}
			if (end == 0)
				return false;

			NkString req;
			for (usize i = 0; i < end; ++i)
				req += static_cast<char>(mRxRaw[i]);
			mHandshakeRequest = req;
			mRxRaw.Erase(mRxRaw.Begin(), mRxRaw.Begin() + static_cast<isize>(end));

			const NkString key = RequestHeader("Sec-WebSocket-Key");
			if (key.Empty()) { // pas une ouverture WebSocket : on ferme sans ceremonie
				mClient.Close();
				mHasClient = false;
				return false;
			}
			// Accept = base64( SHA1( cle + GUID ) ) — RFC 6455 §4.2.2.
			const NkString concat = key + kWsGuid;
			uint8 digest[20] = {};
			string::NkHashSHA1(NkStringView(concat.CStr(), concat.Length()), digest);
			const NkString accept = encoding::base64::NkEncode(digest, 20);

			NkString resp = "HTTP/1.1 101 Switching Protocols\r\n"
							"Upgrade: websocket\r\n"
							"Connection: Upgrade\r\n"
							"Sec-WebSocket-Accept: ";
			resp += accept;
			resp += "\r\n\r\n";
			uint32 sent = 0;
			(void)sent;
			if (mClient.Send(resp.CStr(), static_cast<uint32>(resp.Length())) != NkNetResult::NK_NET_OK) {
				mClient.Close();
				mHasClient = false;
				return false;
			}
			mHandshakeDone = true;
			return true;
		}

		NkString NkWebSocketServer::RequestHeader(const char *name) const noexcept {
			if (!name || !*name || mHandshakeRequest.Empty())
				return NkString();
			usize nameLen = 0;
			while (name[nameLen])
				++nameLen;
			const char *s = mHandshakeRequest.CStr();
			const usize total = mHandshakeRequest.Length();
			usize lineStart = 0;
			for (usize i = 0; i <= total; ++i) {
				const bool eol = (i == total) || (s[i] == '\n');
				if (!eol)
					continue;
				usize lineEnd = i;
				while (lineEnd > lineStart && (s[lineEnd - 1] == '\r' || s[lineEnd - 1] == '\n'))
					--lineEnd;
				if (lineEnd > lineStart + nameLen && s[lineStart + nameLen] == ':' &&
					EqualNoCase(s + lineStart, name, nameLen)) {
					usize v = lineStart + nameLen + 1;
					while (v < lineEnd && (s[v] == ' ' || s[v] == '\t'))
						++v;
					NkString out;
					for (usize k = v; k < lineEnd; ++k)
						out += s[k];
					return out;
				}
				lineStart = i + 1;
			}
			return NkString();
		}

		void NkWebSocketServer::DecodeFrames() noexcept {
			for (;;) {
				const usize n = mRxRaw.Size();
				if (n < 2)
					return;
				const uint8 b0 = mRxRaw[0], b1 = mRxRaw[1];
				const bool fin = (b0 & 0x80) != 0;
				const uint8 opcode = static_cast<uint8>(b0 & 0x0F);
				const bool masked = (b1 & 0x80) != 0;
				uint64 len = static_cast<uint64>(b1 & 0x7F);
				usize off = 2;
				if (len == 126) {
					if (n < off + 2)
						return;
					len = (static_cast<uint64>(mRxRaw[off]) << 8) | mRxRaw[off + 1];
					off += 2;
				} else if (len == 127) {
					if (n < off + 8)
						return;
					len = 0;
					for (int32 i = 0; i < 8; ++i)
						len = (len << 8) | mRxRaw[off + static_cast<usize>(i)];
					off += 8;
				}
				uint8 mask[4] = {};
				if (masked) {
					if (n < off + 4)
						return;
					for (int32 i = 0; i < 4; ++i)
						mask[i] = mRxRaw[off + static_cast<usize>(i)];
					off += 4;
				}
				if (n < off + static_cast<usize>(len))
					return; // trame incomplete : on attend le reste

				// Charge utile demasquee (un client DOIT masquer — RFC 6455 §5.3).
				NkVector<uint8> payload;
				payload.Reserve(static_cast<usize>(len));
				for (uint64 i = 0; i < len; ++i) {
					uint8 c = mRxRaw[off + static_cast<usize>(i)];
					if (masked)
						c = static_cast<uint8>(c ^ mask[i & 3]);
					payload.PushBack(c);
				}
				mRxRaw.Erase(mRxRaw.Begin(), mRxRaw.Begin() + static_cast<isize>(off + static_cast<usize>(len)));

				switch (opcode) {
					case 0x0: // continuation
					case 0x1: // texte
					case 0x2: // binaire (traite comme du texte : notre protocole est du JSON)
						for (usize i = 0; i < payload.Size(); ++i)
							mFragment.PushBack(payload[i]);
						if (fin) {
							NkString msg;
							for (usize i = 0; i < mFragment.Size(); ++i)
								msg += static_cast<char>(mFragment[i]);
							mMessages.PushBack(msg);
							mFragment.Clear();
						}
						break;
					case 0x8: // close : on renvoie un close et on ferme
						(void)SendFrame(0x8, nullptr, 0);
						mClient.Close();
						mHasClient = false;
						mHandshakeDone = false;
						return;
					case 0x9: // ping -> pong avec la MEME charge utile (RFC 6455 §5.5.3)
						(void)SendFrame(0xA, payload.Empty() ? nullptr : payload.Data(), payload.Size());
						break;
					default: // pong (0xA) et inconnus : rien a faire
						break;
				}
			}
		}

		void NkWebSocketServer::Poll() noexcept {
			if (!mRunning)
				return;
			AcceptPending();
			if (!mHasClient)
				return;

			uint8 buf[4096];
			for (;;) {
				uint32 got = 0;
				const NkNetResult r = mClient.Recv(buf, static_cast<uint32>(sizeof(buf)), got);
				// En TCP, une reception de ZERO octet SANS erreur signifie que le pair
				// a ferme. Sans cette distinction, le serveur croyait un client encore
				// present pour toujours et n'en acceptait plus jamais d'autre : la
				// premiere deconnexion le rendait sourd.
				// NK_NET_NOT_CONNECTED = fermeture ordonnee du pair (distinct de
				// « rien a lire », qui rend OK avec 0 octet).
				if (r == NkNetResult::NK_NET_NOT_CONNECTED) {
					mClient.Close();
					mHasClient = false;
					mHandshakeDone = false;
					mRxRaw.Clear();
					mFragment.Clear();
					return;
				}
				if (r != NkNetResult::NK_NET_OK || got == 0)
					break; // rien de disponible pour l'instant (socket non bloquante)
				for (uint32 i = 0; i < got; ++i)
					mRxRaw.PushBack(buf[i]);
			}
			if (!mHandshakeDone) {
				if (!TryHandshake())
					return;
			}
			DecodeFrames();
		}

		bool NkWebSocketServer::SendFrame(uint8 opcode, const uint8 *data, usize len) noexcept {
			if (!mHasClient)
				return false;
			NkVector<uint8> f;
			f.PushBack(static_cast<uint8>(0x80 | opcode)); // FIN + opcode
			// Un SERVEUR ne masque jamais (RFC 6455 §5.1) : bit de masque a 0.
			if (len < 126) {
				f.PushBack(static_cast<uint8>(len));
			} else if (len <= 0xFFFF) {
				f.PushBack(126);
				f.PushBack(static_cast<uint8>((len >> 8) & 0xFF));
				f.PushBack(static_cast<uint8>(len & 0xFF));
			} else {
				f.PushBack(127);
				for (int32 i = 7; i >= 0; --i)
					f.PushBack(static_cast<uint8>((static_cast<uint64>(len) >> (i * 8)) & 0xFF));
			}
			for (usize i = 0; i < len; ++i)
				f.PushBack(data[i]);
			return mClient.Send(f.Data(), static_cast<uint32>(f.Size())) == NkNetResult::NK_NET_OK;
		}

		bool NkWebSocketServer::SendText(const NkString &payload) noexcept {
			if (!HasClient())
				return false;
			return SendFrame(0x1, reinterpret_cast<const uint8 *>(payload.CStr()), payload.Length());
		}

		bool NkWebSocketServer::PopMessage(NkString &out) noexcept {
			if (mMessages.Empty())
				return false;
			out = mMessages[0];
			mMessages.Erase(mMessages.Begin());
			return true;
		}

	} // namespace net
} // namespace nkentseu
