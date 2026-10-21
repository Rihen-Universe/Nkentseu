// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_adresse_et_quantification.cpp
//
// Deux retours d'apprenants du 21/10/2026 sur NKNetwork.
//
// (1) NkAddress::ToString() rendait « 172.172.172.172:172 » : la cause est dans
//     NkFormat (chaque « {} » relisait l'argument 0), voir
//     NKContainers/tests/test_format_accolades.cpp. Ici, le temoin cote reseau :
//     l'adresse 172.16.0.1:8080 que l'enonce fait afficher.
//
// (2) WriteF32Q bornait le code a steps - 1 : la derniere marche n'etait jamais
//     codee (500 m revenait 499,99 ; 360 degres revenait 359,5 au pas de 0,5).
//
// PRE-ENREGISTREMENT (ecrit avant la correction) :
//   (n1) NkAddress(172,16,0,1,8080) : ToString() == "172.16.0.1:8080",
//        HostString() == "172.16.0.1"
//   (q1) le minimum et le maximum reviennent EXACTS (egalite de float32) pour
//        (-500, 500, 0,01), (0, 360, 0,1), (0, 360, 0,5), (-0,7072, 0,7072, 1e-4)
//   (q2) balayage de 4 001 valeurs par plage (et les 50 dernieres marches
//        avant le maximum) : |lu - ecrit| <= demi-pas + bruit du float32
//        (bruit = 1e-6 x (|min| + |max|), bien plus petit qu'un demi-pas)
//   (q3) le FORMAT SUR LE FIL ne change pas : meme nombre de bits
//        (BitsRequired(steps), 17 pour (-500, 500, 0,01)) et le code k vaut
//        toujours min + k x (max - min) / steps ; seul le code « steps » (le
//        maximum), jamais emis avant, est maintenant emis
//   (q4) hors plage : ecreté au minimum et au maximum exacts
//   (q5) code corrompu (> steps) : la lecture rend le maximum, pas au-dela
//   (q6) parametres invalides (plage nulle) : ecriture et lecture consomment
//        le MEME bit ; la valeur suivante se relit intacte
//   (q7) plage plus petite qu'un demi-pas (steps arrondi a 0) : plus de
//        division par zero ; minimum et maximum exacts
// Contre-epreuve : remettre « if (quant >= steps) quant = steps - 1; » rend
// (q1), (q2) et (q3) rouges.
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKNetwork/Protocol/NkBitStream.h"
#include "NKNetwork/Transport/NkSocket.h"
#include "NKContainers/Views/NkSpan.h"
#include "NKContainers/Sequential/NkVector.h"

using namespace nkentseu;
using namespace nkentseu::net;

namespace {

	bool Egal(const NkString &obtenu, const char *attendu) {
		usize n = 0;
		while (attendu[n] != '\0') {
			++n;
		}
		if (static_cast<usize>(obtenu.Length()) != n) {
			return false;
		}
		const char *o = obtenu.CStr();
		for (usize i = 0; i < n; ++i) {
			if (o[i] != attendu[i]) {
				return false;
			}
		}
		return true;
	}

	float32 Abs(float32 x) {
		return x < 0.f ? -x : x;
	}

	struct Plage {
			float32 minV;
			float32 maxV;
			float32 prec;
	};

	const Plage kPlages[] = {
		{-500.f, 500.f, 0.01f},
		{0.f, 360.f, 0.1f},
		{0.f, 360.f, 0.5f},
		{-0.7072f, 0.7072f, 0.0001f},
		{-1000.f, 1000.f, 0.01f},
		{0.f, 1.f, 0.3f},
	};

	// Meme calcul que l'ecriture : le nombre de marches, arrondi.
	uint32 Marches(const Plage &p) {
		return static_cast<uint32>(((p.maxV - p.minV) / p.prec) + 0.5f);
	}

	float32 AllerRetour(float32 v, const Plage &p) {
		uint8 tampon[16] = {};
		NkBitWriter w(tampon, sizeof(tampon));
		w.WriteF32Q(v, p.minV, p.maxV, p.prec);
		NkBitReader r(tampon, sizeof(tampon));
		return r.ReadF32Q(p.minV, p.maxV, p.prec);
	}

} // namespace

TEST_CASE(NKNetworkRetours, N1_AdresseIPv4EnTexte) {
	const NkAddress a(172, 16, 0, 1, 8080);
	const NkString s = a.ToString();
	ASSERT_TRUE_MSG(Egal(s, "172.16.0.1:8080"), s.CStr());
	const NkString h = a.HostString();
	ASSERT_TRUE_MSG(Egal(h, "172.16.0.1"), h.CStr());
}

TEST_CASE(NKNetworkRetours, Q1_MinimumEtMaximumExacts) {
	for (const Plage &p : kPlages) {
		const float32 lmin = AllerRetour(p.minV, p);
		const float32 lmax = AllerRetour(p.maxV, p);
		ASSERT_TRUE_MSG(lmin == p.minV, NkFormat("min {} relu {}", p.minV, lmin).CStr());
		ASSERT_TRUE_MSG(lmax == p.maxV, NkFormat("max {} relu {}", p.maxV, lmax).CStr());
	}
}

TEST_CASE(NKNetworkRetours, Q2_ErreurAuPlusUnDemiPas) {
	for (const Plage &p : kPlages) {
		const uint32 steps = Marches(p);
		const float32 range = p.maxV - p.minV;
		const float32 pas = range / static_cast<float32>(steps);
		const float32 tol = 0.5f * pas + 1e-6f * (Abs(p.minV) + Abs(p.maxV));
		float32 pire = 0.f;
		float32 pireV = 0.f;
		for (uint32 i = 0; i <= 4000u; ++i) {
			const float32 v = p.minV + range * (static_cast<float32>(i) / 4000.f);
			const float32 e = Abs(AllerRetour(v, p) - v);
			if (e > pire) {
				pire = e;
				pireV = v;
			}
		}
		for (uint32 k = 0; k < 50u; ++k) {
			const float32 v = p.maxV - pas * (static_cast<float32>(k) * 0.37f);
			if (v < p.minV) {
				break; // peu de marches (0..1 au pas de 0,3) : on sortirait de la plage, ecretee
			}
			const float32 e = Abs(AllerRetour(v, p) - v);
			if (e > pire) {
				pire = e;
				pireV = v;
			}
		}
		ASSERT_TRUE_MSG(pire <= tol, NkFormat("plage [{}, {}] pas {} : erreur {} a v = {} (tolerance {})", p.minV,
											  p.maxV, p.prec, pire, pireV, tol)
										 .CStr());
	}
}

TEST_CASE(NKNetworkRetours, Q3_FormatSurLeFilInchange) {
	const Plage p = {-500.f, 500.f, 0.01f};
	const uint32 steps = Marches(p);
	ASSERT_EQUAL(100000u, steps);

	// 17 bits : BitsRequired(100 000).
	uint8 tampon[16] = {};
	NkBitWriter w(tampon, sizeof(tampon));
	w.WriteF32Q(p.maxV, p.minV, p.maxV, p.prec);
	w.WriteF32Q(p.minV, p.minV, p.maxV, p.prec);
	w.WriteF32Q(0.f, p.minV, p.maxV, p.prec);
	w.WriteF32Q(125.f, p.minV, p.maxV, p.prec);
	ASSERT_EQUAL(9u, w.BytesWritten()); // 4 x 17 = 68 bits -> 9 octets

	NkBitReader r(tampon, sizeof(tampon));
	const uint32 cMax = r.ReadBits(17);
	const uint32 cMin = r.ReadBits(17);
	const uint32 cZero = r.ReadBits(17);
	const uint32 cVal = r.ReadBits(17);
	ASSERT_TRUE_MSG(cMax == steps, NkFormat("code du maximum {}", cMax).CStr());
	ASSERT_EQUAL(0u, cMin);
	ASSERT_EQUAL(50000u, cZero);
	ASSERT_EQUAL(62500u, cVal);
}

TEST_CASE(NKNetworkRetours, Q4_HorsPlageEcrete) {
	const Plage p = {-500.f, 500.f, 0.01f};
	ASSERT_TRUE(AllerRetour(600.f, p) == 500.f);
	ASSERT_TRUE(AllerRetour(-600.f, p) == -500.f);
}

TEST_CASE(NKNetworkRetours, Q5_CodeCorrompuBorneAuMaximum) {
	uint8 tampon[16] = {};
	NkBitWriter w(tampon, sizeof(tampon));
	w.WriteBits(0x1FFFFu, 17); // 131 071 > 100 000 marches
	NkBitReader r(tampon, sizeof(tampon));
	const float32 v = r.ReadF32Q(-500.f, 500.f, 0.01f);
	ASSERT_TRUE_MSG(v == 500.f, NkFormat("code corrompu relu {}", v).CStr());
}

TEST_CASE(NKNetworkRetours, Q6_ParametresInvalidesRestentAlignes) {
	uint8 tampon[16] = {};
	NkBitWriter w(tampon, sizeof(tampon));
	w.WriteF32Q(1.f, 5.f, 5.f, 0.1f); // plage nulle
	w.WriteU8(0xA5u);
	w.WriteF32Q(1.f, 0.f, 10.f, 0.f); // precision nulle
	w.WriteU8(0x3Cu);
	NkBitReader r(tampon, sizeof(tampon));
	ASSERT_TRUE(r.ReadF32Q(5.f, 5.f, 0.1f) == 5.f);
	ASSERT_EQUAL(0xA5u, static_cast<uint32>(r.ReadU8()));
	ASSERT_TRUE(r.ReadF32Q(0.f, 10.f, 0.f) == 0.f);
	ASSERT_EQUAL(0x3Cu, static_cast<uint32>(r.ReadU8()));
}

// (s1) SendSome (21/10/2026) : sur un socket NON BLOQUANT dont le pair ne lit pas, les envois finissent par
// rendre NK_NET_BUFFER_FULL sans bloquer ; chaque NK_NET_OK dit combien d'octets sont partis ; le pair, quand
// il lit, recoit EXACTEMENT ces octets, dans l'ordre (aucun perdu, aucun en trop). Tout sur 127.0.0.1.
TEST_CASE(NKNetworkRetours, S1_SendSomeDitCeQuiPart) {
	(void)NkSocket::PlatformInit();
	const uint16 port = 47911;
	NkSocket ecoute, client, serveur;
	ASSERT_TRUE(ecoute.Create(NkAddress(127, 0, 0, 1, port), NkSocket::Type::NK_TCP) == NkNetResult::NK_NET_OK);
	ASSERT_TRUE(ecoute.Listen(1) == NkNetResult::NK_NET_OK);
	ASSERT_TRUE(client.Create(NkAddress::Any(0), NkSocket::Type::NK_TCP) == NkNetResult::NK_NET_OK);
	ASSERT_TRUE(client.Connect(NkAddress(127, 0, 0, 1, port)) == NkNetResult::NK_NET_OK);
	NkAddress de;
	ASSERT_TRUE(ecoute.Accept(serveur, de) == NkNetResult::NK_NET_OK);
	ASSERT_TRUE(client.SetNonBlocking(true) == NkNetResult::NK_NET_OK);

	// Remplir : des blocs de 64 Ko numerotes octet par octet (octet k = k % 251), jusqu'au refus.
	static uint8 bloc[65536];
	uint64 envoyes = 0;
	uint32 partiels = 0, pleins = 0;
	for (uint32 essai = 0; essai < 4096u && pleins < 3u; ++essai) {
		for (uint32 i = 0; i < sizeof(bloc); ++i)
			bloc[i] = static_cast<uint8>((envoyes + i) % 251u);
		uint32 parti = 0;
		const NkNetResult r = client.SendSome(bloc, sizeof(bloc), parti);
		if (r == NkNetResult::NK_NET_BUFFER_FULL) {
			ASSERT_EQUAL(0u, parti);
			++pleins;
			continue;
		}
		ASSERT_TRUE(r == NkNetResult::NK_NET_OK);
		ASSERT_TRUE(parti > 0u && parti <= sizeof(bloc));
		if (parti < sizeof(bloc))
			++partiels;
		envoyes += parti;
	}
	ASSERT_TRUE_MSG(pleins >= 1u, "le tampon ne s'est jamais rempli : SendSome a-t-il bloque ?");
	ASSERT_TRUE(envoyes > 0u);

	// Lire tout ce qui est parti, et verifier la suite des octets. Lecture non bloquante : si l'envoi a
	// annonce plus qu'il n'a envoye, la lecture s'arrete (500 ms sans rien) au lieu d'attendre toujours.
	ASSERT_TRUE(serveur.SetNonBlocking(true) == NkNetResult::NK_NET_OK);
	static uint8 lu[65536];
	uint64 recus = 0;
	bool suiteJuste = true;
	for (uint32 tour = 0; tour < 1000000u && recus < envoyes; ++tour) {
		uint32 n = 0;
		if (serveur.Recv(lu, sizeof(lu), n) != NkNetResult::NK_NET_OK)
			break;
		if (n == 0u) {
			NkSocket *tab[1] = {&serveur};
			NkVector<uint32> prets;
			if (NkSocket::Select(NkSpan<NkSocket *>(tab, 1), 500u, prets) != NkNetResult::NK_NET_OK || prets.Empty())
				break; // plus rien ne vient
			continue;
		}
		for (uint32 i = 0; i < n; ++i)
			suiteJuste = suiteJuste && lu[i] == static_cast<uint8>((recus + i) % 251u);
		recus += n;
	}
	ASSERT_TRUE_MSG(recus == envoyes, NkFormat("envoyes {} octets, recus {}", envoyes, recus).CStr());
	ASSERT_TRUE(suiteJuste);
	// Un envoi partiel n'est pas exige : MESURE du 21/10, Windows n'en rend jamais sur 127.0.0.1 (tout ou
	// rien, quel que soit SO_SNDBUF) ; Linux et macOS en rendent. Le compte reste juste dans les deux cas.
	(void)partiels;
	client.Close();
	serveur.Close();
	ecoute.Close();
}

TEST_CASE(NKNetworkRetours, Q7_PlagePlusPetiteQuUnDemiPas) {
	const Plage p = {0.f, 0.1f, 1.f}; // (0,1 / 1) arrondi = 0 marche
	const float32 lmin = AllerRetour(0.f, p);
	const float32 lmax = AllerRetour(0.1f, p);
	ASSERT_TRUE_MSG(lmin == 0.f, NkFormat("min relu {}", lmin).CStr());
	ASSERT_TRUE_MSG(lmax == 0.1f, NkFormat("max relu {}", lmax).CStr());
}
