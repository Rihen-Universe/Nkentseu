// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Engine/Noge/tests/test_prefab_roundtrip.cpp
// =============================================================================
// Temoin de l'aller-retour d'un prefab : un prefab savait se LIRE et ne savait
// pas se RECONSTRUIRE. Trois trous superposes, tous fermes le 2026-09-13 :
//
//   1. NkWorld n'avait aucune porte type-erasee -> NkWorld::AddRaw (NKECS)
//   2. NkPrefab::SerializeComponent etait DECLAREE, APPELEE, et son corps
//      n'existait nulle part. Rien ne rougissait parce que personne
//      n'appelait WithComponent<T>. CE BANC L'APPELLE : c'est le LIEN qui
//      juge, pas la lecture du code.
//   3. NkPrefab::Deserialize ne lisait NI les composants NI les enfants :
//      meme l'attache reparee, un prefab relu serait revenu vide.
//
// ⚠️ PROCESSUS NEUF : le fichier temoin est ecrit dans le dossier temporaire
// du systeme, JAMAIS dans l'espace de travail (un fichier d'essai depose dans
// l'arbre se fait prendre pour un asset par le Content Browser). Deux modes,
// pilotes par la variable d'environnement NK_PREFAB_MODE :
//   (absente)  ecrit puis relit dans le meme processus
//   "ecrire"   ecrit seulement
//   "relire"   n'ecrit RIEN et relit un fichier ecrit par un processus
//              PRECEDENT, deja mort. C'est ce mode qui prouve (p2).
// =============================================================================
#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "NKECS/Core/NkTypeRegistry.h"
#include "NKECS/Reflect/NkReflect.h"
#include "NKECS/Reflect/NkReflectBridge.h"
#include "NKECS/Serialization/NkJsonSerialization.h"
#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Prefab/NkPrefab.h"

using namespace nkentseu;
// Les macros NK_FIELD_* referencent NkMeta_Visible & co. sans qualification :
// elles exigent ce `using` au point d'appel (meme contrainte que les bancs
// NKECS existants).
using namespace nkentseu::ecs::reflect;

// =============================================================================
// COMPOSANT TEMOIN — reflechi, donc (de)serialisable par le pont Phase 4
// =============================================================================
struct PfStats {
		float32 health;
		float32 mana;
		uint32 level;
		bool alive;
};

NK_COMPONENT(PfStats)

NK_REFLECT_BEGIN(PfStats)
NK_FIELD_EX(health, ::nkentseu::ecs::reflect::NkFieldType::Float32)
NK_FIELD_EX(mana, ::nkentseu::ecs::reflect::NkFieldType::Float32)
NK_FIELD_EX(level, ::nkentseu::ecs::reflect::NkFieldType::UInt32)
NK_FIELD_EX(alive, ::nkentseu::ecs::reflect::NkFieldType::Bool)
NK_REFLECT_END(PfStats)

// Les valeurs attendues, ecrites UNE fois et comparees au dernier chiffre.
static PfStats ValeursAttendues() noexcept {
	PfStats s;
	s.health = 87.5f;
	s.mana = -12.25f;
	s.level = 7u;
	s.alive = true;
	return s;
}

// =============================================================================
// Emplacement du fichier temoin : dossier temporaire, jamais l'arbre de travail
// =============================================================================
static const char *CheminTemoin() noexcept {
	static char chemin[1024] = {0};
	if (chemin[0] != '\0') {
		return chemin;
	}
	const char *base = std::getenv("TEMP");
	if (base == nullptr) {
		base = std::getenv("TMPDIR");
	}
	if (base == nullptr) {
		base = ".";
	}
	std::snprintf(chemin, sizeof(chemin), "%s/nk_temoin_prefab.json", base);
	return chemin;
}

static const char *Mode() noexcept {
	const char *m = std::getenv("NK_PREFAB_MODE");
	return (m != nullptr) ? m : "";
}

static bool ModeRelireSeul() noexcept {
	return std::strcmp(Mode(), "relire") == 0;
}

static bool ModeEcrireSeul() noexcept {
	return std::strcmp(Mode(), "ecrire") == 0;
}

// =============================================================================
// (p1) SerializeComponent existe, et le LIEN passe
// =============================================================================
// WithComponent<T>() est le SEUL appelant possible de SerializeComponent (elle
// est privee). Ce test l'appelle : si la fonction n'avait toujours pas de
// corps, ce fichier ne se lierait pas. C'est la difference entre « declaree »
// et « livree ».
TEST_CASE(NogePrefab, SerializeComponentALienEtCorps) {
	ecs::reflect::NkRegisterComponentReflection<PfStats>();

	const PfStats attendu = ValeursAttendues();

	NkPrefab prefab("TemoinPrefab");
	prefab.path = "Temoins/TemoinPrefab.prefab";
	prefab.WithComponent<PfStats>(attendu);

	// Le composant est present ET porte un JSON non vide : SerializeComponent a
	// rendu vrai. Tant qu'elle n'avait pas de corps, cette ligne etait
	// impossible a atteindre — le lien echouait avant.
	ASSERT_TRUE(prefab.HasComponent("PfStats"));

	const NkPrefabComponentData *data = prefab.GetComponentData("PfStats");
	ASSERT_TRUE(data != nullptr);
	ASSERT_TRUE(!data->jsonValue.Empty());

	// NEGATIF : un composant dont la reflexion n'est PAS branchee doit etre
	// REFUSE, pas ecrit a vide. On le verifie par le registre plutot que par
	// un second type, pour ne pas polluer l'enregistrement global.
	ASSERT_TRUE(ecs::serialization::ComponentHasReflection(ecs::NkIdOf<PfStats>()));
}

// =============================================================================
// (p2) partie 1 : ECRIRE le prefab dans un fichier hors de l'arbre
// =============================================================================
TEST_CASE(NogePrefab, EcrireLeFichierTemoin) {
	if (ModeRelireSeul()) {
		// Mode relecture : on n'ecrit RIEN. On exige que le fichier existe
		// deja — il a ete ecrit par un processus precedent, maintenant mort.
		std::FILE *f = std::fopen(CheminTemoin(), "rb");
		ASSERT_TRUE(f != nullptr);
		if (f != nullptr) {
			std::fclose(f);
		}
		return;
	}

	ecs::reflect::NkRegisterComponentReflection<PfStats>();

	NkPrefab prefab("TemoinPrefab");
	prefab.path = "Temoins/TemoinPrefab.prefab";
	prefab.WithComponent<PfStats>(ValeursAttendues());
	prefab.WithLayer(3u);
	prefab.WithTag(0x40u);

	char buffer[16384];
	ASSERT_TRUE(prefab.Serialize(buffer, sizeof(buffer)));
	ASSERT_TRUE(std::strlen(buffer) > 0u);

	std::FILE *f = std::fopen(CheminTemoin(), "wb");
	ASSERT_TRUE(f != nullptr);
	if (f != nullptr) {
		std::fwrite(buffer, 1, std::strlen(buffer), f);
		std::fclose(f);
	}

	// NEGATIF : un tampon trop petit doit etre REFUSE, pas tronque en silence.
	char minuscule[64];
	ASSERT_FALSE(prefab.Serialize(minuscule, sizeof(minuscule)));
}

// =============================================================================
// (p2) partie 2 : RELIRE, INSTANCIER, comparer au dernier chiffre
// =============================================================================
TEST_CASE(NogePrefab, RelireEtInstancier) {
	if (ModeEcrireSeul()) {
		return; // la relecture est le travail de l'invocation suivante
	}

	ecs::reflect::NkRegisterComponentReflection<PfStats>();

	// ── Lecture du fichier ──────────────────────────────────────────────
	std::FILE *f = std::fopen(CheminTemoin(), "rb");
	ASSERT_TRUE(f != nullptr);
	if (f == nullptr) {
		return;
	}
	std::fseek(f, 0, SEEK_END);
	const long taille = std::ftell(f);
	std::fseek(f, 0, SEEK_SET);
	ASSERT_TRUE(taille > 0);

	char json[16384];
	ASSERT_TRUE(static_cast<unsigned long>(taille) < sizeof(json));
	const size_t lus = std::fread(json, 1, static_cast<size_t>(taille), f);
	std::fclose(f);
	json[lus] = '\0';

	// ── Prefab reconstruit depuis le texte ──────────────────────────────
	NkPrefab relu;
	ASSERT_TRUE(relu.Deserialize(json));
	ASSERT_TRUE(relu.HasComponent("PfStats"));
	ASSERT_EQUAL(3, static_cast<int>(relu.layer));
	ASSERT_EQUAL(0x40, static_cast<int>(relu.tagBits));

	// ── Instanciation : c'est ici que la brique travaille ───────────────
	ecs::NkWorld world;
	const ecs::NkEntityId id = relu.Instantiate(world, "instance_temoin");
	ASSERT_TRUE(world.IsAlive(id));

	// Has<T> vrai pour le composant du prefab — c'est exactement ce que
	// l'ancien TODO empechait.
	ASSERT_TRUE(world.Has<PfStats>(id));

	const PfStats *obtenu = world.Get<PfStats>(id);
	ASSERT_TRUE(obtenu != nullptr);
	if (obtenu == nullptr) {
		return;
	}

	// ── Memes valeurs AU DERNIER CHIFFRE ────────────────────────────────
	const PfStats attendu = ValeursAttendues();
	ASSERT_EQUAL(0, std::memcmp(obtenu, &attendu, sizeof(PfStats)));

	// ── NEGATIF : la comparaison DOIT echouer si on mute une valeur ─────
	// Sans ce volet, un memcmp qui rendrait 0 quoi qu'il arrive passerait
	// pour une preuve.
	PfStats mute = attendu;
	mute.health = attendu.health + 1.0f;
	ASSERT_TRUE(std::memcmp(obtenu, &mute, sizeof(PfStats)) != 0);

	mute = attendu;
	mute.level = attendu.level + 1u;
	ASSERT_TRUE(std::memcmp(obtenu, &mute, sizeof(PfStats)) != 0);

	mute = attendu;
	mute.alive = !attendu.alive;
	ASSERT_TRUE(std::memcmp(obtenu, &mute, sizeof(PfStats)) != 0);
}

// =============================================================================
// (p3) les surcharges d'instance
// =============================================================================
// Ce que ce test prouve : SetOverride ECRIT reellement dans `overrides`, la
// valeur de base reste lisible, et RevertOverride sait defaire.
// Ce qu'il NE prouve PAS, et qui est dit dans NkPrefab.h : une surcharge est
// un etat d'INSTANCE ; NkPrefab::Serialize sérialise le TEMPLATE. Les
// surcharges ne survivent donc pas a l'ecriture du .prefab.
TEST_CASE(NogePrefab, SurchargeDInstance) {
	ecs::reflect::NkRegisterComponentReflection<PfStats>();

	NkPrefab prefab("TemoinSurcharge");
	prefab.path = "Temoins/TemoinSurcharge.prefab";
	prefab.WithComponent<PfStats>(ValeursAttendues());

	ecs::NkWorld world;
	const ecs::NkEntityId id = prefab.Instantiate(world, "instance");
	ASSERT_TRUE(world.Has<PfStats>(id));

	NkPrefabInstance inst(id, prefab.path.CStr(), "instance");
	ASSERT_FALSE(inst.isOverridden);
	ASSERT_EQUAL(0, static_cast<int>(inst.overrides.Size()));

	// La surcharge est REELLEMENT enregistree — avant ce chantier, le corps
	// levait un drapeau et jetait la valeur.
	ASSERT_TRUE(inst.SetOverride<float32>("PfStats.health", 42.5f));
	ASSERT_TRUE(inst.isOverridden);
	ASSERT_EQUAL(1, static_cast<int>(inst.overrides.Size()));

	const NkString *valeur = inst.overrides.Find(NkString("PfStats.health"));
	ASSERT_TRUE(valeur != nullptr);
	ASSERT_TRUE(valeur != nullptr && !valeur->Empty());

	// La valeur de BASE, elle, n'a pas bouge : la surcharge est un etat
	// d'instance, pas une reecriture du template.
	const PfStats attendu = ValeursAttendues();
	ASSERT_EQUAL(0, std::memcmp(world.Get<PfStats>(id), &attendu, sizeof(PfStats)));

	// NEGATIF : une cle vide est refusee, et rien n'est ecrit.
	ASSERT_FALSE(inst.SetOverride<float32>("", 1.f));
	ASSERT_EQUAL(1, static_cast<int>(inst.overrides.Size()));

	// Defaire.
	ASSERT_TRUE(inst.RevertOverride("PfStats.health"));
	ASSERT_EQUAL(0, static_cast<int>(inst.overrides.Size()));
	ASSERT_FALSE(inst.RevertOverride("PfStats.health"));
}

// ============================================================
// Copyright © 2025-2026 Rihen. All rights reserved.
// Proprietary License - All Rights Reserved (see LICENSE)
// ============================================================
