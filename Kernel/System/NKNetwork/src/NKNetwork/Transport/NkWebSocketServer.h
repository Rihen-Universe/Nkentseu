#pragma once
// -----------------------------------------------------------------------------
// @File    NkWebSocketServer.h
// @Brief   Serveur WebSocket (RFC 6455) minimal et sans dependance externe.
// @Author  Rihen
// @License Proprietary - Free to use and modify
//
// NKNetwork savait deja faire du TCP serveur (NkSocket : Listen/Accept) et
// mentionnait WebSocket dans sa feuille de route, sans l'implementer — hors
// WebAssembly, ou c'est le NAVIGATEUR qui le fournit (emscripten/websocket.h).
// Cette brique comble ce manque cote natif.
//
// Perimetre VOLONTAIREMENT restreint a ce qu'un IDE local echange :
//   - poignee de main HTTP « Upgrade » (Sec-WebSocket-Key -> Accept) ;
//   - trames TEXTE, ping/pong, close ;
//   - un seul client a la fois, en boucle non bloquante.
// Pas de TLS, pas de compression, pas de multiplexage : sur une boucle locale,
// ils n'apportent rien et chacun serait une source de bugs supplementaire.
//
// La poignee de main REUTILISE la cryptographie du moteur (NkHashSHA1 et
// NkBase64) : il n'y a aucune raison de reecrire SHA-1, et une deuxieme
// implementation serait une deuxieme chose a maintenir juste.
//
// Usage :
//     NkWebSocketServer ws;
//     ws.Start(0);                       // 0 = port libre choisi par l'OS
//     ws.Poll();                         // a appeler regulierement
//     if (ws.HasClient()) ws.SendText("{...}");
//     NkString msg;
//     while (ws.PopMessage(msg)) { ... } // messages recus depuis le dernier Poll
// -----------------------------------------------------------------------------
#include "NKNetwork/Transport/NkSocket.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace net {

		using namespace nkentseu;

		class NKENTSEU_NETWORK_CLASS_EXPORT NkWebSocketServer {
			public:
				NkWebSocketServer() noexcept = default;
				~NkWebSocketServer() noexcept;

				NkWebSocketServer(const NkWebSocketServer &) = delete;
				NkWebSocketServer &operator=(const NkWebSocketServer &) = delete;

				// Ecoute sur 127.0.0.1:port. `port` = 0 -> l'OS en choisit un libre,
				// recuperable par Port(). Volontairement LOOPBACK : ce canal expose
				// l'etat de l'editeur, il n'a rien a faire sur le reseau.
				bool Start(uint16 port = 0) noexcept;
				void Stop() noexcept;

				bool IsRunning() const noexcept {
					return mRunning;
				}

				// Port REELLEMENT attribue (utile quand on a demande 0).
				uint16 Port() const noexcept {
					return mPort;
				}

				bool HasClient() const noexcept {
					return mHasClient && mHandshakeDone;
				}

				// Avance l'etat : accepte un client, lit ce qui est disponible, repond
				// aux ping. Ne bloque jamais. A appeler a chaque frame.
				void Poll() noexcept;

				// Envoie une trame TEXTE. false si aucun client connecte.
				bool SendText(const NkString &payload) noexcept;

				// Depile un message recu. false quand il n'y en a plus.
				bool PopMessage(NkString &out) noexcept;

				// Chemin d'un en-tete HTTP de la requete d'ouverture (ex. « Authorization »).
				// Vide si absent. Sert a verifier un jeton d'authentification.
				NkString RequestHeader(const char *name) const noexcept;

			private:
				bool AcceptPending() noexcept;
				bool TryHandshake() noexcept;	  // consomme mRxRaw jusqu'a la fin des en-tetes
				void DecodeFrames() noexcept;	  // consomme mRxRaw -> mMessages
				bool SendFrame(uint8 opcode, const uint8 *data, usize len) noexcept;

				NkSocket mListen;
				NkSocket mClient;
				bool mRunning = false;
				bool mHasClient = false;
				bool mHandshakeDone = false;
				uint16 mPort = 0;

				NkVector<uint8> mRxRaw;			 // octets bruts recus, pas encore interpretes
				NkVector<uint8> mFragment;		 // message en cours (trames fragmentees)
				NkVector<NkString> mMessages;	 // messages complets, prets a etre depiles
				NkString mHandshakeRequest;		 // requete HTTP d'ouverture (pour RequestHeader)
		};

	} // namespace net
} // namespace nkentseu
