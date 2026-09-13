// =============================================================================
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// tests/test_raw_attach.cpp
// =============================================================================
// Temoin de « la brique » : attacher un composant a une entite SANS connaitre
// son type a la compilation (NkWorld::AddRaw / GetRaw / HasRaw / RemoveRaw).
//
// Ce que ce banc prouve, et ce qu'il refuse de prouver :
//
//   b1  AddRaw donne le MEME resultat que Add<T> : memes octets, meme
//       existence (Has<T> vrai), meme adresse (GetRaw == Get<T>).
//       NEGATIF 1 : un cid NON enregistre -> refus propre, aucune ecriture.
//       NEGATIF 2 : le temoin doit pouvoir rendre AUTRE CHOSE que zero — on
//                   mute un octet de la reference et le memcmp DOIT rougir.
//                   Un banc qui ne sait que rendre 0 ne mesure rien.
//       NEGATIF 3 : entite morte -> refus propre.
//
//   b2  le cout est DIT : sizeof(ComponentMeta) et sizeof(NkTypeInfo) sont
//       imprimes (aucun champ ajoute -> 0 octet par type), et le temps de
//       N attaches AddRaw est compare a N attaches Add<T> en RAPPORT.
//
// Standalone (pas de framework externe), comme les deux autres bancs NKECS.
// <chrono> n'est utilise QUE par le chronometre du banc — jamais par le moteur.
// =============================================================================

#include <chrono>
#include <cstdio>
#include <cstring>

#include "NKECS/Core/NkTypeRegistry.h"
#include "NKECS/Reflect/NkReflect.h"
#include "NKECS/World/NkWorld.h"

using namespace nkentseu;
using namespace nkentseu::ecs;

// ─── helpers de test ─────────────────────────────────────────────────────────
static int s_pass = 0;
static int s_fail = 0;

#define EXPECT_TRUE(expr)                                                                                              \
	do {                                                                                                               \
		if (!(expr)) {                                                                                                 \
			printf("  FAIL [%s:%d] %s\n", __FILE__, __LINE__, #expr);                                                  \
			++s_fail;                                                                                                  \
		} else {                                                                                                       \
			++s_pass;                                                                                                  \
		}                                                                                                              \
	} while (0)

#define EXPECT_EQ(a, b) EXPECT_TRUE((a) == (b))

// =============================================================================
// COMPOSANTS DE TEST
// =============================================================================

// Trivialement copiable : le cas courant.
struct RawPose {
		float32 x;
		float32 y;
		float32 z;
		uint32 flags;
};

NK_COMPONENT(RawPose)

// NON trivialement copiable : prouve que AddRaw passe bien par les hooks
// destruct/copyConstruct du ComponentMeta et ne fait pas un memcpy aveugle.
struct RawCounted {
		int value = 0;
		static int sCtor;
		static int sDtor;
		static int sCopy;

		RawCounted() noexcept {
			++sCtor;
		}

		RawCounted(const RawCounted &o) noexcept : value(o.value) {
			++sCopy;
		}

		RawCounted &operator=(const RawCounted &o) noexcept {
			value = o.value;
			return *this;
		}

		~RawCounted() noexcept {
			++sDtor;
		}
};

int RawCounted::sCtor = 0;
int RawCounted::sDtor = 0;
int RawCounted::sCopy = 0;

NK_COMPONENT(RawCounted)

// Tag : taille nulle, aucun slot a ecrire.
struct RawTag {};

NK_COMPONENT(RawTag)

// Sert au chronometre.
struct RawBench {
		float32 a;
		float32 b;
		float32 c;
		float32 d;
};

NK_COMPONENT(RawBench)

// =============================================================================
// b2 — LE COUT, DIT EN OCTETS
// =============================================================================
static void TestCout() {
	printf("[TEST] b2 — cout en octets par type\n");

	// Aucun champ n'a ete ajoute a ces deux structures : le cout par type de la
	// brique est de ZERO octet. Ces deux nombres sont la pour que le prochain
	// qui touche a ces structures voie tout de suite si elles ont grossi.
	printf("    sizeof(ecs::ComponentMeta)       = %u octets\n", static_cast<uint32>(sizeof(ComponentMeta)));
	printf("    sizeof(reflect::NkTypeInfo)      = %u octets\n",
		   static_cast<uint32>(sizeof(reflect::NkTypeInfo)));
	printf("    kMaxComponentTypes               = %u\n", static_cast<uint32>(kMaxComponentTypes));
	printf("    table ComponentMeta du registre  = %u octets\n",
		   static_cast<uint32>(sizeof(ComponentMeta) * kMaxComponentTypes));

	// Un ComponentMeta non vide est la condition meme du refus propre :
	// s'il etait vide, AddRaw n'aurait rien pour refuser.
	EXPECT_TRUE(sizeof(ComponentMeta) > 0);
}

// =============================================================================
// b1 — MEME RESULTAT QUE Add<T>
// =============================================================================
static void TestMemeResultatQueAdd() {
	printf("[TEST] b1 — AddRaw donne le meme resultat que Add<T>\n");

	NkWorld world;

	const RawPose value{1.5f, -2.5f, 10.25f, 0xABCDu};

	// Chemin de reference : le patron.
	const NkEntityId ref = world.CreateEntity();
	world.Add<RawPose>(ref, value);

	// Chemin type-erase : que des octets et un identifiant.
	const NkEntityId raw = world.CreateEntity();
	const NkComponentId cid = NkIdOf<RawPose>();
	EXPECT_TRUE(world.AddRaw(raw, cid, &value));

	// ── existence ───────────────────────────────────────────────────────
	EXPECT_TRUE(world.Has<RawPose>(raw));
	EXPECT_TRUE(world.HasRaw(raw, cid));
	EXPECT_TRUE(world.Get<RawPose>(raw) != nullptr);

	// ── memes octets, au dernier bit ────────────────────────────────────
	const RawPose *viaTemplate = world.Get<RawPose>(ref);
	const RawPose *viaRaw = world.Get<RawPose>(raw);
	EXPECT_TRUE(viaTemplate != nullptr && viaRaw != nullptr);
	EXPECT_EQ(std::memcmp(viaRaw, &value, sizeof(RawPose)), 0);
	EXPECT_EQ(std::memcmp(viaRaw, viaTemplate, sizeof(RawPose)), 0);

	// ── les deux portes donnent la meme adresse ─────────────────────────
	EXPECT_TRUE(world.GetRaw(raw, cid) == static_cast<const void *>(viaRaw));

	// ── NEGATIF 2 : le temoin sait rendre autre chose que zero ──────────
	// Si ce memcmp restait a 0 apres mutation, il ne mesurerait rien.
	RawPose mute = value;
	reinterpret_cast<unsigned char *>(&mute)[0] ^= 0x01u;
	EXPECT_TRUE(std::memcmp(viaRaw, &mute, sizeof(RawPose)) != 0);
	printf("    temoin non muet : memcmp(mute) = %d (doit etre != 0)\n",
		   std::memcmp(viaRaw, &mute, sizeof(RawPose)));

	// ── ecrasement d'un composant deja present ──────────────────────────
	const RawPose second{9.f, 8.f, 7.f, 0x1234u};
	EXPECT_TRUE(world.AddRaw(raw, cid, &second));
	EXPECT_EQ(std::memcmp(world.Get<RawPose>(raw), &second, sizeof(RawPose)), 0);

	// ── retrait ─────────────────────────────────────────────────────────
	EXPECT_TRUE(world.RemoveRaw(raw, cid));
	EXPECT_TRUE(!world.HasRaw(raw, cid));
	EXPECT_TRUE(!world.Has<RawPose>(raw));
	EXPECT_TRUE(world.GetRaw(raw, cid) == nullptr);
	// Deux fois de suite : le second retrait doit dire faux, pas planter.
	EXPECT_TRUE(!world.RemoveRaw(raw, cid));
}

// =============================================================================
// b1 — composant NON trivialement copiable
// =============================================================================
static void TestTypeNonTrivial() {
	printf("[TEST] b1 — type non trivialement copiable (hooks, pas memcpy)\n");

	NkWorld world;
	const NkEntityId e = world.CreateEntity();

	RawCounted src;
	src.value = 4242;

	const int copiesAvant = RawCounted::sCopy;
	EXPECT_TRUE(world.AddRaw(e, NkIdOf<RawCounted>(), &src));

	// Le hook copyConstruct du ComponentMeta a bien ete emprunte : sans lui,
	// un memcpy aveugle n'aurait incremente aucun compteur.
	EXPECT_TRUE(RawCounted::sCopy > copiesAvant);

	const RawCounted *stored = world.Get<RawCounted>(e);
	EXPECT_TRUE(stored != nullptr);
	EXPECT_EQ(stored->value, 4242);
	printf("    copies constatees : %d (avant %d)\n", RawCounted::sCopy, copiesAvant);
}

// =============================================================================
// b1 — tag (taille nulle)
// =============================================================================
static void TestTag() {
	printf("[TEST] b1 — tag de taille nulle\n");

	NkWorld world;
	const NkEntityId e = world.CreateEntity();
	const NkComponentId cid = NkIdOf<RawTag>();

	// Un tag s'attache : son unique donnee est le bit de masque.
	EXPECT_TRUE(world.AddRaw(e, cid, nullptr));
	EXPECT_TRUE(world.HasRaw(e, cid));
	EXPECT_TRUE(world.Has<RawTag>(e));

	// ... mais il n'a rien a pointer : GetRaw rend nullptr SANS que cela
	// veuille dire « absent ». C'est HasRaw qui repond a la question.
	EXPECT_TRUE(world.GetRaw(e, cid) == nullptr);
}

// =============================================================================
// NEGATIF 1 et 3 — refus propres
// =============================================================================
static void TestRefusPropres() {
	printf("[TEST] b1 NEGATIF — type non enregistre, entite morte\n");

	NkWorld world;
	const NkEntityId e = world.CreateEntity();
	world.Add<RawPose>(e, RawPose{1.f, 2.f, 3.f, 4u});

	// ── NEGATIF 1 : cid jamais attribue ─────────────────────────────────
	// On choisit un identifiant dans les bornes mais au-dela de ce que le
	// registre a distribue : le refus doit venir du registre, pas d'un
	// depassement de tableau.
	const uint32 distribues = NkTypeRegistry::Global().Count();
	const NkComponentId inconnu = static_cast<NkComponentId>(kMaxComponentTypes - 1u);
	EXPECT_TRUE(inconnu >= distribues); // sinon le test ne teste rien
	EXPECT_TRUE(NkTypeRegistry::Global().Get(inconnu) == nullptr);

	const uint32 vivantesAvant = world.EntityCount();
	const unsigned char bidon[64] = {0xFFu};

	EXPECT_TRUE(!world.AddRaw(e, inconnu, bidon));
	EXPECT_TRUE(!world.HasRaw(e, inconnu));
	EXPECT_TRUE(world.GetRaw(e, inconnu) == nullptr);
	EXPECT_TRUE(!world.RemoveRaw(e, inconnu));

	// Aucune ecriture sauvage : l'entite est exactement dans l'etat d'avant.
	EXPECT_EQ(world.EntityCount(), vivantesAvant);
	EXPECT_TRUE(world.Has<RawPose>(e));
	const RawPose attendu{1.f, 2.f, 3.f, 4u};
	EXPECT_EQ(std::memcmp(world.Get<RawPose>(e), &attendu, sizeof(RawPose)), 0);

	// Un cid hors bornes est refuse de la meme facon.
	EXPECT_TRUE(!world.AddRaw(e, static_cast<NkComponentId>(kMaxComponentTypes + 7u), bidon));

	// ── NEGATIF 3 : entite morte ────────────────────────────────────────
	const NkEntityId mort = world.CreateEntity();
	world.Destroy(mort);
	const RawPose v{5.f, 6.f, 7.f, 8u};
	EXPECT_TRUE(!world.AddRaw(mort, NkIdOf<RawPose>(), &v));
	EXPECT_TRUE(!world.HasRaw(mort, NkIdOf<RawPose>()));
	EXPECT_TRUE(world.GetRaw(mort, NkIdOf<RawPose>()) == nullptr);
}

// =============================================================================
// Le chemin du retour : nom -> identifiant
// =============================================================================
static void TestNomVersId() {
	printf("[TEST] NkTypeRegistry::FindIdByName\n");

	NkTypeRegistry &reg = NkTypeRegistry::Global();

	EXPECT_EQ(reg.FindIdByName("RawPose"), NkIdOf<RawPose>());
	EXPECT_EQ(reg.FindIdByName("RawCounted"), NkIdOf<RawCounted>());

	// Negatif : un nom que personne n'a enregistre, et le nullptr.
	EXPECT_EQ(reg.FindIdByName("CeTypeNExistePas"), kInvalidComponentId);
	EXPECT_EQ(reg.FindIdByName(nullptr), kInvalidComponentId);
	// Sensible a la casse — dit, pour que personne ne le decouvre en production.
	EXPECT_EQ(reg.FindIdByName("rawpose"), kInvalidComponentId);

	// Aller-retour complet : type -> nom -> identifiant -> attache.
	NkWorld world;
	const NkEntityId e = world.CreateEntity();
	const char *nom = reg.TypeName<RawPose>();
	const NkComponentId cid = reg.FindIdByName(nom);
	EXPECT_EQ(cid, NkIdOf<RawPose>());
	const RawPose v{3.f, 2.f, 1.f, 0u};
	EXPECT_TRUE(world.AddRaw(e, cid, &v));
	EXPECT_TRUE(world.Has<RawPose>(e));
}

// =============================================================================
// b2 — le chronometre : un RAPPORT, jamais une milliseconde absolue
// =============================================================================
static void TestChrono() {
	printf("[TEST] b2 — AddRaw vs Add<T> (rapport)\n");

	constexpr uint32 kCount = 20000u;
	using Clock = std::chrono::steady_clock;

	const RawBench v{1.f, 2.f, 3.f, 4.f};
	const NkComponentId cid = NkIdOf<RawBench>();

	// Add<T>
	double nsTemplate = 0.0;
	{
		NkWorld world;
		const Clock::time_point t0 = Clock::now();
		for (uint32 i = 0; i < kCount; ++i) {
			const NkEntityId e = world.CreateEntity();
			world.Add<RawBench>(e, v);
		}
		nsTemplate = std::chrono::duration<double, std::nano>(Clock::now() - t0).count();
	}

	// AddRaw
	double nsRaw = 0.0;
	{
		NkWorld world;
		const Clock::time_point t0 = Clock::now();
		for (uint32 i = 0; i < kCount; ++i) {
			const NkEntityId e = world.CreateEntity();
			world.AddRaw(e, cid, &v);
		}
		nsRaw = std::chrono::duration<double, std::nano>(Clock::now() - t0).count();
	}

	const double parAttacheTemplate = nsTemplate / static_cast<double>(kCount);
	const double parAttacheRaw = nsRaw / static_cast<double>(kCount);
	const double rapport = (parAttacheTemplate > 0.0) ? (parAttacheRaw / parAttacheTemplate) : 0.0;

	printf("    %u attaches — Add<T> %.1f ns/attache, AddRaw %.1f ns/attache, rapport %.2fx\n", kCount,
		   parAttacheTemplate, parAttacheRaw, rapport);

	// On ne DECLARE aucun seuil en millisecondes : une machine chargee le
	// ferait mentir. On verifie seulement que les deux chemins ont bien
	// travaille (chrono non nul) — le rapport est une INFORMATION, pas un
	// critere de reussite.
	EXPECT_TRUE(nsTemplate > 0.0);
	EXPECT_TRUE(nsRaw > 0.0);
}

// =============================================================================
// main
// =============================================================================
int main() {
	printf("======================================================\n");
	printf("  NKECS — attache type-erasee (la brique)\n");
	printf("======================================================\n");

	TestCout();
	TestMemeResultatQueAdd();
	TestTypeNonTrivial();
	TestTag();
	TestRefusPropres();
	TestNomVersId();
	TestChrono();

	printf("======================================================\n");
	printf("  Results: %d passed, %d failed\n", s_pass, s_fail);
	printf("======================================================\n");
	return (s_fail == 0) ? 0 : 1;
}

// ============================================================
// Copyright © 2025-2026 Rihen. All rights reserved.
// Proprietary License - All Rights Reserved (see LICENSE)
// ============================================================
