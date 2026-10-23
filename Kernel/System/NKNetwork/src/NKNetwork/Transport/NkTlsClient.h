// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#pragma once
// =============================================================================
// NkTlsClient.h — LE TLS D'UN CLIENT, SUR UNE SOCKET TCP DEJA CONNECTEE (23/10/2026).
//
// Pour le RTMPS qu'exige Facebook (le Direct de NKCode), et pour tout client de NKNetwork qui en aura besoin.
// Decision de Rodolf (23/10) : « oui » a Schannel, le TLS de WINDOWS -- aucune bibliotheque ajoutee, mbed-TLS
// (Externals/Libs/NKMbedTLS, l'option de NkHTTPClient) n'est pas touche. Hors Windows, IsSupported() est faux et
// Handshake() le dit (NK_NET_PLATFORM_UNSUPPORTED) : rien n'est promis.
//
//   NkSocket s;  s.Create(...); s.Connect(adresse);          // le TCP d'abord
//   NkTlsClient tls;
//   if (tls.Handshake(s, "live-api-s.facebook.com") != NkNetResult::NK_NET_OK)  tls.LastError() ...
//   tls.Send(octets, n);  tls.Recv(tampon, taille, lus);  tls.Close();
//
// LE CERTIFICAT DU SERVEUR est verifie par Windows (son magasin de confiance, le nom d'hote attendu, les dates).
// Un refus est DIT : LastError() le nomme, CertificateRejected() est vrai.
// L'EPINGLE (pin) : si `pinnedDer` est donne, le certificat du serveur doit etre EXACTEMENT ces octets DER, et rien
// d'autre n'est cru -- c'est ainsi que les bancs eprouvent le TLS contre un certificat de test cree pour eux, sans
// jamais toucher au magasin de confiance de la machine.
// TLS 1.2 (Facebook, YouTube et Twitch le servent ; TLS 1.3 viendra avec SCH_CREDENTIALS).
// =============================================================================
#include "NKNetwork/NkNetworkApi.h"
#include "NKNetwork/Core/NkNetDefines.h"
#include "NKNetwork/Transport/NkSocket.h"
#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace net {

		class NKENTSEU_NETWORK_CLASS_EXPORT NkTlsClient {
			public:
				NkTlsClient() noexcept = default;
				~NkTlsClient() noexcept;
				NkTlsClient(const NkTlsClient &) = delete;
				NkTlsClient &operator=(const NkTlsClient &) = delete;

				/// Vrai la ou ce client sait faire du TLS (Windows : Schannel).
				static bool IsSupported() noexcept;

				/// La poignee de main TLS sur `socket` (TCP, connectee, bloquante). `host` : le nom attendu dans le
				/// certificat (et envoye en SNI). `pinnedDer` / `pinnedSize` : l'epingle (voir l'en-tete), ou nul.
				[[nodiscard]] NkNetResult Handshake(NkSocket &socket, const char *host,
													const uint8 *pinnedDer = nullptr, usize pinnedSize = 0,
													uint32 timeoutMs = 10000) noexcept;

				/// Chiffre et envoie TOUT `size` octets.
				[[nodiscard]] NkNetResult Send(const void *data, uint32 size) noexcept;

				/// Des octets dechiffres : d'abord ceux deja en attente ; sinon UNE lecture de la socket (bloquante :
				/// l'appeler quand elle est prete, ou quand Pending() est vrai). `outSize` = 0 et NK_NET_OK : un
				/// enregistrement TLS n'est pas encore entier, rappeler. NK_NET_NOT_CONNECTED : le serveur a ferme.
				[[nodiscard]] NkNetResult Recv(void *buffer, uint32 capacity, uint32 &outSize) noexcept;

				/// Des octets deja dechiffres (ou des octets chiffres deja recus) attendent : Recv rendra sans attendre
				/// la socket.
				bool Pending() const noexcept;

				/// Le close_notify, puis le contexte libere (la socket reste a l'appelant).
				void Close() noexcept;

				bool IsOpen() const noexcept {
					return mOpen;
				}

				/// Le dernier refus, en clair (« certificat refuse : ... », « poignee de main : ... »).
				const NkString &LastError() const noexcept {
					return mError;
				}

				/// Vrai si le dernier refus vient du certificat du serveur (non reconnu, mauvais nom, epingle).
				bool CertificateRejected() const noexcept {
					return mCertRejected;
				}

				/// Contre-epreuve des bancs (tls-epingle-ignoree) : l'epingle n'est plus comparee.
				bool mutationEpingleIgnoree = false;
				/// Contre-epreuve des bancs (tls-sans-verification) : sans epingle, AUCUNE verification du certificat.
				bool mutationSansVerification = false;

			private:
				NkSocket *mSocket = nullptr;
				bool mOpen = false;
				bool mCertRejected = false;
				NkString mError;
				NkVector<uint8> mIn;	///< octets CHIFFRES recus, pas encore dechiffres
				NkVector<uint8> mPlain; ///< octets dechiffres, pas encore rendus
				usize mPlainPos = 0;
				// Le contexte Schannel (CredHandle, CtxtHandle, SecPkgContext_StreamSizes), opaque ici : il est decrit
				// dans le .cpp, qui seul inclut les en-tetes de securite de Windows.
				uint64 mCred[2] = {0, 0};
				uint64 mCtx[2] = {0, 0};
				bool mCredOk = false, mCtxOk = false;
				uint32 mHeader = 0, mTrailer = 0, mMaxMessage = 0;
				bool Fail(const char *what, int32 status, bool cert) noexcept;
		};

	} // namespace net
} // namespace nkentseu
