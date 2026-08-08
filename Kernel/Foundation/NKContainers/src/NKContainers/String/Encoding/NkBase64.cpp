// -----------------------------------------------------------------------------
// FICHIER: NKContainers/String/Encoding/NkBase64.cpp
// DESCRIPTION: Base64 (RFC 4648) — implementation.
// AUTEUR: Rihen
//
// Le header declarait ces quatre fonctions depuis fevrier 2026 SANS aucune
// implementation : personne ne les avait encore appelees, et une declaration
// jamais liee ne provoque aucune erreur. Le premier appel reel (poignee de main
// WebSocket, NKNetwork) a fait apparaitre le manque a l'edition de liens.
// -----------------------------------------------------------------------------
#include "NKContainers/String/Encoding/NkBase64.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace encoding {
		namespace base64 {

			namespace {
				const char *const kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

				// -1 = caractere hors alphabet. Table construite une fois.
				const int8 *DecodeTable() {
					static int8 t[256];
					static bool prete = false;
					if (!prete) {
						for (int32 i = 0; i < 256; ++i)
							t[i] = -1;
						for (int32 i = 0; i < 64; ++i)
							t[static_cast<uint8>(kAlphabet[i])] = static_cast<int8>(i);
						prete = true;
					}
					return t;
				}
			} // namespace

			NkString NkEncode(const uint8 *data, usize length) {
				NkString out;
				if (!data || length == 0)
					return out;
				out.Reserve(((length + 2) / 3) * 4);
				usize i = 0;
				for (; i + 2 < length; i += 3) {
					const uint32 v = (static_cast<uint32>(data[i]) << 16) | (static_cast<uint32>(data[i + 1]) << 8) |
									 static_cast<uint32>(data[i + 2]);
					out += kAlphabet[(v >> 18) & 0x3F];
					out += kAlphabet[(v >> 12) & 0x3F];
					out += kAlphabet[(v >> 6) & 0x3F];
					out += kAlphabet[v & 0x3F];
				}
				// Reste de 1 ou 2 octets : on complete avec '=' (RFC 4648 §4).
				const usize reste = length - i;
				if (reste == 1) {
					const uint32 v = static_cast<uint32>(data[i]) << 16;
					out += kAlphabet[(v >> 18) & 0x3F];
					out += kAlphabet[(v >> 12) & 0x3F];
					out += '=';
					out += '=';
				} else if (reste == 2) {
					const uint32 v = (static_cast<uint32>(data[i]) << 16) | (static_cast<uint32>(data[i + 1]) << 8);
					out += kAlphabet[(v >> 18) & 0x3F];
					out += kAlphabet[(v >> 12) & 0x3F];
					out += kAlphabet[(v >> 6) & 0x3F];
					out += '=';
				}
				return out;
			}

			bool NkDecode(NkStringView base64, uint8 *out, usize *outLength) {
				if (!outLength)
					return false;
				const int8 *t = DecodeTable();
				const char *s = base64.Data();
				const usize n = base64.Size();
				usize ecrits = 0;
				uint32 acc = 0;
				int32 bits = 0;
				for (usize i = 0; i < n; ++i) {
					const char c = s[i];
					if (c == '=' || c == '\n' || c == '\r' || c == ' ' || c == '\t')
						continue; // remplissage et blancs : ignores
					const int8 v = t[static_cast<uint8>(c)];
					if (v < 0)
						return false; // caractere invalide : on echoue plutot que de deviner
					acc = (acc << 6) | static_cast<uint32>(v);
					bits += 6;
					if (bits >= 8) {
						bits -= 8;
						if (out)
							out[ecrits] = static_cast<uint8>((acc >> bits) & 0xFF);
						++ecrits;
					}
				}
				*outLength = ecrits;
				return true;
			}

			NkString NkDecodeToString(NkStringView base64) {
				usize n = 0;
				if (!NkDecode(base64, nullptr, &n) || n == 0)
					return NkString();
				NkString out;
				out.Reserve(n);
				// Second passage avec un tampon : la premiere passe n'a servi qu'a
				// mesurer, ce qui evite d'allouer au hasard.
				NkVector<uint8> buf;
				buf.Resize(n);
				if (!NkDecode(base64, buf.Data(), &n))
					return NkString();
				for (usize i = 0; i < n; ++i)
					out += static_cast<char>(buf[i]);
				return out;
			}

			NkString NkEncodeString(NkStringView str) {
				return NkEncode(reinterpret_cast<const uint8 *>(str.Data()), str.Size());
			}

		} // namespace base64
	} // namespace encoding
} // namespace nkentseu
