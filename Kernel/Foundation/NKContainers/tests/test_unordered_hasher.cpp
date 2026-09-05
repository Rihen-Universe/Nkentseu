// =============================================================================
// test_unordered_hasher.cpp
//
// Le hacheur par defaut de NkUnorderedMap et NkUnorderedSet hachait les OCTETS
// DE L'OBJET cle. Pour NkString, ces octets contiennent un NkIAllocator*, une
// union char* / char[], une taille et une capacite : deux chaines de meme texte
// n'ont donc pas les memes octets, tombent dans deux seaux differents, et
// Contains repond faux juste apres une ecriture.
//
// Constate le 2026-09-05 : NkActionManager et NkAxisManager (NKEvent), tous
// deux indexes par NkString, n'enregistraient jamais rien. CreateAction ecrit
// dans mActions, AddCommand teste Contains, trouve faux, et sort. Aucune action
// ne partait, sans un seul message. Vingt autres emplois de
// NkUnorderedMap<NkString, ...> etaient dans le meme cas.
//
// Ces tests fixent le contrat : deux cles EGALES doivent se retrouver, quels
// que soient leurs octets.
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKContainers/NKContainers.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Associative/NkUnorderedMap.h"
#include "NKContainers/Associative/NkUnorderedSet.h"

using namespace nkentseu;

TEST_CASE(NKContainersUnorderedHasher, CleChaineSeRetrouveApresEcriture) {
	NkUnorderedMap<NkString, int> map;
	map[NkString("Sauter")] = 7;

	// Une AUTRE instance, de meme texte : c'est tout l'enjeu.
	ASSERT_TRUE(map.Contains(NkString("Sauter")));
	ASSERT_EQUAL(7, map[NkString("Sauter")]);
	ASSERT_FALSE(map.Contains(NkString("Tirer")));
}

TEST_CASE(NKContainersUnorderedHasher, CleChaineLongueHorsSSO) {
	// Assez longue pour partir sur le tas : les octets de l'objet contiennent
	// alors un pointeur, et deux copies pointent ailleurs.
	const char *texte = "une action au nom deliberement long pour sortir du petit tampon";
	NkUnorderedMap<NkString, int> map;
	map[NkString(texte)] = 42;

	ASSERT_TRUE(map.Contains(NkString(texte)));
	ASSERT_EQUAL(42, map[NkString(texte)]);
}

TEST_CASE(NKContainersUnorderedHasher, PlusieursClesChaines) {
	NkUnorderedMap<NkString, int> map;
	map[NkString("Sauter")] = 1;
	map[NkString("Tirer")] = 2;
	map[NkString("Courir")] = 3;

	ASSERT_EQUAL(3, static_cast<int>(map.Size()));
	ASSERT_EQUAL(1, map[NkString("Sauter")]);
	ASSERT_EQUAL(2, map[NkString("Tirer")]);
	ASSERT_EQUAL(3, map[NkString("Courir")]);

	// Une ecriture sur une cle existante remplace, elle n'ajoute pas.
	map[NkString("Tirer")] = 20;
	ASSERT_EQUAL(3, static_cast<int>(map.Size()));
	ASSERT_EQUAL(20, map[NkString("Tirer")]);
}

TEST_CASE(NKContainersUnorderedHasher, EnsembleDeChaines) {
	NkUnorderedSet<NkString> set;
	set.Insert(NkString("Horizontal"));
	set.Insert(NkString("Vertical"));
	set.Insert(NkString("Horizontal")); // doublon : ne doit rien ajouter

	ASSERT_EQUAL(2, static_cast<int>(set.Size()));
	ASSERT_TRUE(set.Contains(NkString("Horizontal")));
	ASSERT_TRUE(set.Contains(NkString("Vertical")));
	ASSERT_FALSE(set.Contains(NkString("Profondeur")));
}

TEST_CASE(NKContainersUnorderedHasher, ClesTrivialesInchangees) {
	// La seconde surcharge, celle qui hache les octets, reste la bonne pour les
	// types triviaux : ce test garde le comportement d'origine.
	NkUnorderedMap<int, int> map;
	map[1] = 10;
	map[2] = 20;

	ASSERT_TRUE(map.Contains(1));
	ASSERT_TRUE(map.Contains(2));
	ASSERT_FALSE(map.Contains(3));
	ASSERT_EQUAL(10, map[1]);
}
