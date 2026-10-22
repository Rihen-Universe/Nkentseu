// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_websocket_entete_exige.cpp
//
// NkWebSocketServer::ExigerEnTete (22/10/2026) : le pont IDE de NKCode exige le jeton de Claude Code dans
// l'en-tete « x-claude-code-ide-authorization » ; un client sans lui doit etre REFUSE (401 puis fermeture), sans
// prendre la place du vrai client. Les clients de ce banc sont de vraies sockets TCP sur 127.0.0.1.
//
// PRE-ENREGISTREMENT :
//   (w1) SANS exigence (tout autre utilisateur du serveur) : un client sans jeton est ACCEPTE (101) -- rien ne
//        change pour qui n'appelle pas ExigerEnTete ;
//   (w2) avec exigence, le BON jeton : 101, HasClient() vrai, aucun refus compte ;
//   (w3) un MAUVAIS jeton : 401, HasClient() faux, un refus compte ; puis un client au bon jeton entre (la
//        place est restee libre) ;
//   (w4) le jeton ABSENT : 401, HasClient() faux ;
//   (w5) le NOM de l'en-tete sans tenir compte de la casse (comme HTTP) : « X-Claude-Code-Ide-Authorization »
//        au bon jeton est accepte.
//   (w6) (22/10) AccepterSousProtocole("mcp") : un client qui demande « mcp » (seul, ou dans une liste) le recoit
//        dans « Sec-WebSocket-Protocol » -- le CLI de Claude Code (Bun, `protocols:["mcp"]`) refuse sinon ;
//   (w7) sans AccepterSousProtocole : la reponse ne porte AUCUN sous-protocole (rien ne change pour les autres).
// Contre-epreuve (w6) : sans la reponse du sous-protocole dans TryHandshake, (w6) ROUGIT.
// Contre-epreuve (faite a la main le 22/10, voir Tests/agents_14-10/ETAT_nkcode-analyse.md) : sans le test de
// l'en-tete dans TryHandshake, (w3) et (w4) ROUGISSENT ; (w1), (w2), (w5) restent verts.
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKNetwork/Transport/NkWebSocketServer.h"
#include "NKNetwork/Transport/NkSocket.h"
#include "NKContainers/String/NkString.h"
#include "NKTime/NkChrono.h"

using namespace nkentseu;
using namespace nkentseu::net;

namespace {

	const char *const kEnTete = "x-claude-code-ide-authorization";
	const char *const kJeton = "0123456789abcdef0123456789abcdef";

	/// Ce qu'un client recoit en reponse a son ouverture : « 101 », « 401 », ou vide (rien en 2 s).
	struct Reponse {
			NkString texte;
			bool connecte = false;
	};

	bool Contient(const NkString &s, const char *motif) {
		const char *p = s.CStr();
		usize m = 0;
		while (motif[m])
			++m;
		for (usize i = 0; i + m <= s.Length(); ++i) {
			usize k = 0;
			while (k < m && p[i + k] == motif[k])
				++k;
			if (k == m)
				return true;
		}
		return false;
	}

	/// Ouvre une connexion WebSocket au serveur avec `ligneEnTete` (« Nom: valeur », ou nullptr), fait avancer le
	/// serveur jusqu'a la reponse, et rend ce que le client a lu. `client` reste ouvert (l'appelant le ferme).
	Reponse Ouvrir(NkWebSocketServer &srv, NkSocket &client, const char *ligneEnTete) {
		Reponse r;
		if (client.Create(NkAddress(127, 0, 0, 1, 0), NkSocket::Type::NK_TCP) != NkNetResult::NK_NET_OK)
			return r;
		if (client.Connect(NkAddress(127, 0, 0, 1, srv.Port())) != NkNetResult::NK_NET_OK)
			return r;
		r.connecte = true;
		(void)client.SetNonBlocking(true);
		NkString req = "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
					   "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n";
		if (ligneEnTete) {
			req += ligneEnTete;
			req += "\r\n";
		}
		req += "\r\n";
		(void)client.Send(req.CStr(), static_cast<uint32>(req.Length()));
		for (int32 tour = 0; tour < 200 && !Contient(r.texte, "\r\n\r\n"); ++tour) {
			srv.Poll();
			char buf[512];
			uint32 lu = 0;
			if (client.Recv(buf, static_cast<uint32>(sizeof(buf)), lu) == NkNetResult::NK_NET_OK && lu > 0)
				for (uint32 i = 0; i < lu; ++i)
					r.texte += buf[i];
			NkChrono::SleepMilliseconds(10);
		}
		srv.Poll();
		return r;
	}

	bool Accepte(const Reponse &r) {
		return Contient(r.texte, "101 Switching Protocols");
	}
	bool Refuse(const Reponse &r) {
		return Contient(r.texte, "401 Unauthorized");
	}

} // namespace

TEST_CASE(NKNetworkWebSocketEnTete, W1_SansExigenceRienNeChange) {
	NkWebSocketServer srv;
	ASSERT_TRUE(srv.Start(0));
	NkSocket c;
	const Reponse r = Ouvrir(srv, c, nullptr);
	ASSERT_TRUE_MSG(r.connecte, "connexion TCP impossible");
	ASSERT_TRUE_MSG(Accepte(r), r.texte.CStr());
	ASSERT_TRUE(srv.HasClient());
	ASSERT_TRUE(srv.RefusAuthentification() == 0u);
	c.Close();
	srv.Stop();
}

TEST_CASE(NKNetworkWebSocketEnTete, W2_BonJetonAccepte) {
	NkWebSocketServer srv;
	ASSERT_TRUE(srv.Start(0));
	srv.ExigerEnTete(kEnTete, NkString(kJeton));
	NkSocket c;
	const NkString ligne = NkString(kEnTete) + ": " + kJeton;
	const Reponse r = Ouvrir(srv, c, ligne.CStr());
	ASSERT_TRUE_MSG(Accepte(r), r.texte.CStr());
	ASSERT_TRUE(srv.HasClient());
	ASSERT_TRUE(srv.RefusAuthentification() == 0u);
	c.Close();
	srv.Stop();
}

TEST_CASE(NKNetworkWebSocketEnTete, W3_MauvaisJetonRefusePuisPlaceLibre) {
	NkWebSocketServer srv;
	ASSERT_TRUE(srv.Start(0));
	srv.ExigerEnTete(kEnTete, NkString(kJeton));
	NkSocket intrus;
	const NkString faux = NkString(kEnTete) + ": ffffffffffffffffffffffffffffffff";
	const Reponse r = Ouvrir(srv, intrus, faux.CStr());
	ASSERT_TRUE_MSG(Refuse(r), r.texte.CStr());
	ASSERT_FALSE(srv.HasClient());
	ASSERT_TRUE(srv.RefusAuthentification() == 1u);
	intrus.Close();
	NkSocket vrai;
	const NkString bon = NkString(kEnTete) + ": " + kJeton;
	const Reponse r2 = Ouvrir(srv, vrai, bon.CStr());
	ASSERT_TRUE_MSG(Accepte(r2), r2.texte.CStr());
	ASSERT_TRUE(srv.HasClient());
	vrai.Close();
	srv.Stop();
}

TEST_CASE(NKNetworkWebSocketEnTete, W4_JetonAbsentRefuse) {
	NkWebSocketServer srv;
	ASSERT_TRUE(srv.Start(0));
	srv.ExigerEnTete(kEnTete, NkString(kJeton));
	NkSocket c;
	const Reponse r = Ouvrir(srv, c, nullptr);
	ASSERT_TRUE_MSG(Refuse(r), r.texte.CStr());
	ASSERT_FALSE(srv.HasClient());
	ASSERT_TRUE(srv.RefusAuthentification() == 1u);
	c.Close();
	srv.Stop();
}

TEST_CASE(NKNetworkWebSocketEnTete, W5_NomSansCasse) {
	NkWebSocketServer srv;
	ASSERT_TRUE(srv.Start(0));
	srv.ExigerEnTete(kEnTete, NkString(kJeton));
	NkSocket c;
	const NkString ligne = NkString("X-Claude-Code-Ide-Authorization: ") + kJeton;
	const Reponse r = Ouvrir(srv, c, ligne.CStr());
	ASSERT_TRUE_MSG(Accepte(r), r.texte.CStr());
	ASSERT_TRUE(srv.HasClient());
	c.Close();
	srv.Stop();
}

TEST_CASE(NKNetworkWebSocketEnTete, W6_SousProtocoleMcpRenvoye) {
	NkWebSocketServer srv;
	ASSERT_TRUE(srv.Start(0));
	srv.ExigerEnTete(kEnTete, NkString(kJeton));
	srv.AccepterSousProtocole("mcp");
	// comme le CLI : le jeton (casse du CLI) ET le sous-protocole demande
	NkSocket c;
	const NkString lignes = NkString("X-Claude-Code-Ide-Authorization: ") + kJeton + "\r\nSec-WebSocket-Protocol: mcp";
	const Reponse r = Ouvrir(srv, c, lignes.CStr());
	ASSERT_TRUE_MSG(Accepte(r), r.texte.CStr());
	ASSERT_TRUE_MSG(Contient(r.texte, "Sec-WebSocket-Protocol: mcp\r\n"), r.texte.CStr());
	ASSERT_TRUE(srv.SousProtocole() == NkString("mcp"));
	c.Close();
	// dans une liste
	NkSocket c2;
	const NkString liste = NkString("X-Claude-Code-Ide-Authorization: ") + kJeton + "\r\nSec-WebSocket-Protocol: chat, mcp";
	const Reponse r2 = Ouvrir(srv, c2, liste.CStr());
	ASSERT_TRUE_MSG(Contient(r2.texte, "Sec-WebSocket-Protocol: mcp\r\n"), r2.texte.CStr());
	c2.Close();
	srv.Stop();
}

TEST_CASE(NKNetworkWebSocketEnTete, W7_SansAcceptationAucunSousProtocole) {
	NkWebSocketServer srv;
	ASSERT_TRUE(srv.Start(0));
	NkSocket c;
	const Reponse r = Ouvrir(srv, c, "Sec-WebSocket-Protocol: mcp");
	ASSERT_TRUE_MSG(Accepte(r), r.texte.CStr());
	ASSERT_FALSE(Contient(r.texte, "Sec-WebSocket-Protocol"));
	ASSERT_TRUE(srv.SousProtocole().Empty());
	c.Close();
	srv.Stop();
}
