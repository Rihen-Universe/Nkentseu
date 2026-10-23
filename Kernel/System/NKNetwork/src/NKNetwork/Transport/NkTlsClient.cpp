// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkTlsClient.cpp — LE TLS D'UN CLIENT PAR SCHANNEL (Windows), REFUSE AILLEURS (23/10/2026).
//
// Le schema de Schannel (SSPI), cote client :
//   AcquireCredentialsHandle (UNISP_NAME, sortant, TLS 1.2) ;
//   InitializeSecurityContext en boucle : chaque jeton produit part au serveur, chaque reponse revient en entree,
//   jusqu'a SEC_E_OK (SEC_E_INCOMPLETE_MESSAGE : lire encore ; SECBUFFER_EXTRA : des octets deja recus qui
//   appartiennent a la suite) ;
//   EncryptMessage / DecryptMessage : [en-tete][donnees][queue] par enregistrement ;
//   ApplyControlToken(SCHANNEL_SHUTDOWN) + InitializeSecurityContext : le close_notify.
// Le certificat : verifie par Windows (SCH_CRED_AUTO_CRED_VALIDATION, le nom d'hote en cible) ; avec une epingle,
// verifie A LA MAIN (SCH_CRED_MANUAL_CRED_VALIDATION) : les octets DER du certificat recu doivent etre l'epingle.
// =============================================================================
#include "pch.h"
#include "NKNetwork/Transport/NkTlsClient.h"
#include "NKMemory/NkFunction.h"
#include "NKContainers/String/NkFormat.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#ifndef SECURITY_WIN32
#define SECURITY_WIN32
#endif
#include <windows.h>
#include <wincrypt.h>
#include <security.h>
#include <schannel.h>
#endif

namespace nkentseu {
	namespace net {

		bool NkTlsClient::Fail(const char *what, int32 status, bool cert) noexcept {
			mCertRejected = cert;
			mError = status ? NkPrintf("%s (0x%08X)", what, (unsigned)status) : NkString(what);
			return false;
		}

#if defined(NKENTSEU_PLATFORM_WINDOWS)

		namespace {
			inline CredHandle *Cred(uint64 *p) noexcept {
				return reinterpret_cast<CredHandle *>(p);
			}

			inline CtxtHandle *Ctx(uint64 *p) noexcept {
				return reinterpret_cast<CtxtHandle *>(p);
			}

			static_assert(sizeof(CredHandle) <= 2 * sizeof(uint64), "CredHandle tient dans mCred");
			static_assert(sizeof(CtxtHandle) <= 2 * sizeof(uint64), "CtxtHandle tient dans mCtx");

			/// Les refus de Schannel qui viennent du CERTIFICAT du serveur (et non du reseau ou du protocole).
			inline bool StatutCertificat(SECURITY_STATUS s) noexcept {
				return s == SEC_E_UNTRUSTED_ROOT || s == SEC_E_WRONG_PRINCIPAL || s == SEC_E_CERT_EXPIRED ||
					   s == SEC_E_CERT_UNKNOWN || s == (SECURITY_STATUS)CERT_E_UNTRUSTEDROOT ||
					   s == (SECURITY_STATUS)CERT_E_CN_NO_MATCH || s == (SECURITY_STATUS)CERT_E_EXPIRED ||
					   s == (SECURITY_STATUS)CERT_E_CHAINING || s == (SECURITY_STATUS)CERT_E_REVOKED;
			}

			/// Envoie tout (la socket est bloquante).
			inline bool ToutEnvoyer(NkSocket &s, const void *d, uint32 n) noexcept {
				return n == 0 || s.Send(d, n) == NkNetResult::NK_NET_OK;
			}
		} // namespace

		bool NkTlsClient::IsSupported() noexcept {
			return true;
		}

		NkNetResult NkTlsClient::Handshake(NkSocket &socket, const char *host, const uint8 *pinnedDer, usize pinnedSize,
										   uint32 timeoutMs) noexcept {
			Close();
			mSocket = &socket;
			mError = NkString();
			mCertRejected = false;
			mIn.Clear();
			mPlain.Clear();
			mPlainPos = 0;
			const bool epingle = pinnedDer && pinnedSize > 0;
			SCHANNEL_CRED sc = {};
			sc.dwVersion = SCHANNEL_CRED_VERSION;
			sc.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT;
			sc.dwFlags = SCH_CRED_NO_DEFAULT_CREDS | SCH_USE_STRONG_CRYPTO |
						 ((epingle || mutationSansVerification) ? SCH_CRED_MANUAL_CRED_VALIDATION
																: SCH_CRED_AUTO_CRED_VALIDATION);
			TimeStamp ts = {};
			SECURITY_STATUS st =
				::AcquireCredentialsHandleA(nullptr, const_cast<char *>(UNISP_NAME_A), SECPKG_CRED_OUTBOUND, nullptr,
											&sc, nullptr, nullptr, Cred(mCred), &ts);
			if (st != SEC_E_OK) {
				Fail("TLS : Schannel refuse les identifiants du client", (int32)st, false);
				return NkNetResult::NK_NET_SOCKET_ERROR;
			}
			mCredOk = true;
			const DWORD demandes = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY |
								   ISC_REQ_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM;
			char cible[256] = {};
			for (usize i = 0; host && host[i] && i + 1 < sizeof(cible); ++i)
				cible[i] = host[i];
			bool premier = true;
			NkVector<uint8> &in = mIn;
			for (int32 tours = 0; tours < 64; ++tours) {
				SecBuffer entree[2] = {};
				entree[0].BufferType = SECBUFFER_TOKEN;
				entree[0].pvBuffer = in.Empty() ? nullptr : in.Data();
				entree[0].cbBuffer = (unsigned long)in.Size();
				entree[1].BufferType = SECBUFFER_EMPTY;
				SecBufferDesc descEntree = {SECBUFFER_VERSION, 2, entree};
				SecBuffer sortie[1] = {};
				sortie[0].BufferType = SECBUFFER_TOKEN;
				SecBufferDesc descSortie = {SECBUFFER_VERSION, 1, sortie};
				DWORD attributs = 0;
				st = ::InitializeSecurityContextA(Cred(mCred), premier ? nullptr : Ctx(mCtx), cible, demandes, 0, 0,
												  premier ? nullptr : &descEntree, 0, Ctx(mCtx), &descSortie,
												  &attributs, &ts);
				if (premier && (st == SEC_E_OK || st == SEC_I_CONTINUE_NEEDED || st == SEC_E_INCOMPLETE_MESSAGE))
					mCtxOk = true;
				premier = false;
				// un jeton a envoyer (le ClientHello, puis les reponses)
				if (sortie[0].cbBuffer && sortie[0].pvBuffer) {
					const bool ok = ToutEnvoyer(socket, sortie[0].pvBuffer, sortie[0].cbBuffer);
					::FreeContextBuffer(sortie[0].pvBuffer);
					if (!ok) {
						Fail("TLS : poignee de main, envoi impossible", 0, false);
						Close();
						return NkNetResult::NK_NET_SOCKET_ERROR;
					}
				}
				if (st == SEC_E_INCOMPLETE_MESSAGE || (st == SEC_I_CONTINUE_NEEDED && in.Empty()) ||
					(st == SEC_I_CONTINUE_NEEDED && entree[1].BufferType != SECBUFFER_EXTRA)) {
					if (st == SEC_I_CONTINUE_NEEDED)
						in.Clear(); // tout a ete consomme : lire la suite
					// lire la suite du serveur
					NkSocket *tab[1] = {&socket};
					NkVector<uint32> prets;
					const NkNetResult sel = NkSocket::Select(NkSpan<NkSocket *>(tab, 1), timeoutMs, prets);
					if (sel != NkNetResult::NK_NET_OK || prets.Empty()) {
						Fail("TLS : poignee de main, le serveur ne repond pas", 0, false);
						Close();
						return NkNetResult::NK_NET_TIMEOUT;
					}
					uint8 b[16384];
					uint32 lu = 0;
					if (socket.Recv(b, sizeof(b), lu) != NkNetResult::NK_NET_OK || lu == 0) {
						Fail("TLS : poignee de main, le serveur a ferme la connexion", 0, false);
						Close();
						return NkNetResult::NK_NET_NOT_CONNECTED;
					}
					for (uint32 i = 0; i < lu; ++i)
						in.PushBack(b[i]);
					continue;
				}
				if (st == SEC_I_CONTINUE_NEEDED) { // des octets en trop : le debut de la reponse suivante
					const usize extra = entree[1].cbBuffer;
					NkVector<uint8> reste;
					for (usize i = in.Size() - extra; i < in.Size(); ++i)
						reste.PushBack(in[i]);
					in = reste;
					continue;
				}
				if (st == SEC_E_OK) {
					NkVector<uint8> reste; // des donnees deja chiffrees apres la poignee de main
					if (entree[1].BufferType == SECBUFFER_EXTRA && entree[1].cbBuffer > 0)
						for (usize i = in.Size() - entree[1].cbBuffer; i < in.Size(); ++i)
							reste.PushBack(in[i]);
					in = reste;
					break;
				}
				const bool cert = StatutCertificat(st);
				// le refus, nomme : ce que Rodolf lira dans le panneau du direct
				const char *pourquoi =
					st == SEC_E_UNTRUSTED_ROOT	  ? "TLS : certificat du serveur refuse par Windows (autorite inconnue)"
					: st == SEC_E_WRONG_PRINCIPAL ? "TLS : certificat du serveur refuse par Windows (pas ce nom d'hote)"
					: st == SEC_E_CERT_EXPIRED	  ? "TLS : certificat du serveur refuse par Windows (expire)"
					: cert						  ? "TLS : certificat du serveur refuse par Windows"
												  : "TLS : poignee de main refusee";
				Fail(pourquoi, (int32)st, cert);
				Close();
				return cert ? NkNetResult::NK_NET_AUTH_FAILED : NkNetResult::NK_NET_SOCKET_ERROR;
			}
			if (st != SEC_E_OK) {
				Fail("TLS : poignee de main sans fin", (int32)st, false);
				Close();
				return NkNetResult::NK_NET_SOCKET_ERROR;
			}
			// L'epingle : le certificat recu doit etre EXACTEMENT celui attendu (bancs : un certificat de test).
			if (epingle) {
				PCCERT_CONTEXT c = nullptr;
				st = ::QueryContextAttributesA(Ctx(mCtx), SECPKG_ATTR_REMOTE_CERT_CONTEXT, &c);
				bool egal = st == SEC_E_OK && c && (usize)c->cbCertEncoded == pinnedSize;
				for (usize i = 0; egal && i < pinnedSize; ++i)
					egal = c->pbCertEncoded[i] == pinnedDer[i];
				if (c)
					::CertFreeCertificateContext(c);
				if (!egal && !mutationEpingleIgnoree) {
					Fail("TLS : certificat du serveur refuse (ce n'est pas le certificat epingle)", 0, true);
					Close();
					return NkNetResult::NK_NET_AUTH_FAILED;
				}
			}
			SecPkgContext_StreamSizes tailles = {};
			st = ::QueryContextAttributesA(Ctx(mCtx), SECPKG_ATTR_STREAM_SIZES, &tailles);
			if (st != SEC_E_OK) {
				Fail("TLS : tailles d'enregistrement illisibles", (int32)st, false);
				Close();
				return NkNetResult::NK_NET_SOCKET_ERROR;
			}
			mHeader = tailles.cbHeader, mTrailer = tailles.cbTrailer, mMaxMessage = tailles.cbMaximumMessage;
			mOpen = true;
			return NkNetResult::NK_NET_OK;
		}

		NkNetResult NkTlsClient::Send(const void *data, uint32 size) noexcept {
			if (!mOpen || !mSocket)
				return NkNetResult::NK_NET_NOT_CONNECTED;
			const uint8 *d = static_cast<const uint8 *>(data);
			NkVector<uint8> paquet;
			for (uint32 fait = 0; fait < size;) {
				const uint32 n = (size - fait) < mMaxMessage ? (size - fait) : mMaxMessage;
				paquet.Resize((usize)mHeader + n + mTrailer);
				memory::NkCopy(paquet.Data() + mHeader, d + fait, n);
				SecBuffer b[4] = {};
				b[0].BufferType = SECBUFFER_STREAM_HEADER, b[0].pvBuffer = paquet.Data(), b[0].cbBuffer = mHeader;
				b[1].BufferType = SECBUFFER_DATA, b[1].pvBuffer = paquet.Data() + mHeader, b[1].cbBuffer = n;
				b[2].BufferType = SECBUFFER_STREAM_TRAILER, b[2].pvBuffer = paquet.Data() + mHeader + n,
				b[2].cbBuffer = mTrailer;
				b[3].BufferType = SECBUFFER_EMPTY;
				SecBufferDesc desc = {SECBUFFER_VERSION, 4, b};
				const SECURITY_STATUS st = ::EncryptMessage(Ctx(mCtx), 0, &desc, 0);
				if (st != SEC_E_OK) {
					Fail("TLS : chiffrement impossible", (int32)st, false);
					return NkNetResult::NK_NET_SOCKET_ERROR;
				}
				if (!ToutEnvoyer(*mSocket, paquet.Data(), b[0].cbBuffer + b[1].cbBuffer + b[2].cbBuffer))
					return NkNetResult::NK_NET_SOCKET_ERROR;
				fait += n;
			}
			return NkNetResult::NK_NET_OK;
		}

		namespace {
			/// Un enregistrement TLS ENTIER attend dans `in` (son en-tete de 5 octets dit sa longueur).
			inline bool EnregistrementEntier(const NkVector<uint8> &in) noexcept {
				if (in.Size() < 5)
					return false;
				const usize n = ((usize)in[3] << 8) | (usize)in[4];
				return in.Size() >= 5 + n;
			}
		} // namespace

		bool NkTlsClient::Pending() const noexcept {
			// Rendra sans toucher la socket : des octets clairs, ou un enregistrement chiffre deja entier.
			return mPlainPos < mPlain.Size() || EnregistrementEntier(mIn);
		}

		NkNetResult NkTlsClient::Recv(void *buffer, uint32 capacity, uint32 &outSize) noexcept {
			outSize = 0;
			if (!mOpen || !mSocket)
				return NkNetResult::NK_NET_NOT_CONNECTED;
			uint8 *dst = static_cast<uint8 *>(buffer);
			auto rendre = [&]() {
				const usize dispo = mPlain.Size() - mPlainPos;
				const uint32 n = (uint32)(dispo < capacity ? dispo : capacity);
				memory::NkCopy(dst, mPlain.Data() + mPlainPos, n);
				mPlainPos += n;
				if (mPlainPos >= mPlain.Size()) {
					mPlain.Clear();
					mPlainPos = 0;
				}
				outSize = n;
			};
			if (mPlainPos < mPlain.Size()) {
				rendre();
				return NkNetResult::NK_NET_OK;
			}
			bool socketLue = false; // UNE lecture de la socket au plus par appel : l'appelant l'a vue prete
			for (int32 tours = 0; tours < 8; ++tours) {
				if (!EnregistrementEntier(mIn)) {
					if (socketLue)
						return NkNetResult::NK_NET_OK; // pas encore entier : rappeler quand la socket sera prete
					socketLue = true;
					uint8 b[16384];
					uint32 lu = 0;
					const NkNetResult r = mSocket->Recv(b, sizeof(b), lu);
					if (r != NkNetResult::NK_NET_OK || lu == 0)
						return r == NkNetResult::NK_NET_OK ? NkNetResult::NK_NET_NOT_CONNECTED : r;
					for (uint32 i = 0; i < lu; ++i)
						mIn.PushBack(b[i]);
					continue;
				}
				SecBuffer b[4] = {};
				b[0].BufferType = SECBUFFER_DATA, b[0].pvBuffer = mIn.Data(), b[0].cbBuffer = (unsigned long)mIn.Size();
				b[1].BufferType = b[2].BufferType = b[3].BufferType = SECBUFFER_EMPTY;
				SecBufferDesc desc = {SECBUFFER_VERSION, 4, b};
				const SECURITY_STATUS st = ::DecryptMessage(Ctx(mCtx), &desc, 0, nullptr);
				if (st == SEC_E_INCOMPLETE_MESSAGE)
					return NkNetResult::NK_NET_OK;
				if (st == SEC_I_CONTEXT_EXPIRED) { // close_notify du serveur
					mIn.Clear();
					return NkNetResult::NK_NET_NOT_CONNECTED;
				}
				if (st != SEC_E_OK) {
					Fail(st == SEC_I_RENEGOTIATE ? "TLS : le serveur demande une renegociation (non prise en charge)"
												 : "TLS : dechiffrement impossible",
						 (int32)st, false);
					return NkNetResult::NK_NET_SOCKET_ERROR;
				}
				NkVector<uint8> reste;
				for (int32 i = 1; i < 4; ++i) {
					if (b[i].BufferType == SECBUFFER_DATA && b[i].cbBuffer)
						for (unsigned long k = 0; k < b[i].cbBuffer; ++k)
							mPlain.PushBack(static_cast<const uint8 *>(b[i].pvBuffer)[k]);
					if (b[i].BufferType == SECBUFFER_EXTRA && b[i].cbBuffer)
						for (usize k = mIn.Size() - b[i].cbBuffer; k < mIn.Size(); ++k)
							reste.PushBack(mIn[k]);
				}
				mIn = reste;
				if (mPlainPos < mPlain.Size()) {
					rendre();
					return NkNetResult::NK_NET_OK;
				}
			}
			return NkNetResult::NK_NET_OK;
		}

		void NkTlsClient::Close() noexcept {
			if (mOpen && mSocket && mCtxOk) { // le close_notify
				DWORD jeton = SCHANNEL_SHUTDOWN;
				SecBuffer b0 = {sizeof(jeton), SECBUFFER_TOKEN, &jeton};
				SecBufferDesc d0 = {SECBUFFER_VERSION, 1, &b0};
				if (::ApplyControlToken(Ctx(mCtx), &d0) == SEC_E_OK) {
					SecBuffer b1 = {0, SECBUFFER_TOKEN, nullptr};
					SecBufferDesc d1 = {SECBUFFER_VERSION, 1, &b1};
					DWORD attributs = 0;
					TimeStamp ts = {};
					const SECURITY_STATUS st = ::InitializeSecurityContextA(
						Cred(mCred), Ctx(mCtx), nullptr,
						ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY |
							ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM,
						0, 0, nullptr, 0, Ctx(mCtx), &d1, &attributs, &ts);
					if ((st == SEC_E_OK || st == SEC_I_CONTEXT_EXPIRED) && b1.pvBuffer && b1.cbBuffer)
						(void)ToutEnvoyer(*mSocket, b1.pvBuffer, b1.cbBuffer);
					if (b1.pvBuffer)
						::FreeContextBuffer(b1.pvBuffer);
				}
			}
			if (mCtxOk)
				::DeleteSecurityContext(Ctx(mCtx));
			if (mCredOk)
				::FreeCredentialsHandle(Cred(mCred));
			mCtxOk = mCredOk = mOpen = false;
			mCred[0] = mCred[1] = mCtx[0] = mCtx[1] = 0;
			mIn.Clear();
			mPlain.Clear();
			mPlainPos = 0;
		}

#else // ── hors Windows : pas encore de TLS, et on le dit ──

		bool NkTlsClient::IsSupported() noexcept {
			return false;
		}

		NkNetResult NkTlsClient::Handshake(NkSocket &, const char *, const uint8 *, usize, uint32) noexcept {
			Fail("TLS : disponible sous Windows seulement pour l'instant (Schannel)", 0, false);
			return NkNetResult::NK_NET_PLATFORM_UNSUPPORTED;
		}

		NkNetResult NkTlsClient::Send(const void *, uint32) noexcept {
			return NkNetResult::NK_NET_PLATFORM_UNSUPPORTED;
		}

		NkNetResult NkTlsClient::Recv(void *, uint32, uint32 &outSize) noexcept {
			outSize = 0;
			return NkNetResult::NK_NET_PLATFORM_UNSUPPORTED;
		}

		bool NkTlsClient::Pending() const noexcept {
			return false;
		}

		void NkTlsClient::Close() noexcept {
			mOpen = false;
		}

#endif

		NkTlsClient::~NkTlsClient() noexcept {
			Close();
		}

	} // namespace net
} // namespace nkentseu
