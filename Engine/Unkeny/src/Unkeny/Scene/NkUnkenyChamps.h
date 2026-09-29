//
// NkUnkenyChamps.h
// =============================================================================
// Description :
//   La DESCRIPTION CHAMP PAR CHAMP d'un composant, telle que la sauvegarde, la
//   photo et les prefabs s'en servent : un nom, un genre, un decalage. Declaree
//   une fois (NkScene::PhotographierAussi<T>(nom, champs, n)), elle rend un
//   composant du jeu PORTABLE : ecrit par NOM de champ, il se relit sur une
//   autre plateforme et apres l'ajout d'un champ.
//
// Caracteristiques :
//   - Genres scalaires, vecteur 2D, texte fixe, TABLEAUX (y compris un champ
//     d'un tableau de structures : `nombre` et `pas`).
//   - Trois genres de REFERENCE, qui sont la raison d'etre du fichier :
//       NK_ENTITE  un ecs::NkEntityId, ecrit par l'identifiant STABLE de sa
//                  cible (NkIdentite2D) et rebranche a la relecture — et a
//                  NkScene::Restaurer, ou les poignees changent ;
//       NK_SON     un identifiant NkSons2D, ecrit par le NOM du son ;
//       NK_TEXTURE un identifiant NkTextures2D, ecrit par le NOM de la texture ;
//       NK_PREFAB  un identifiant NkPrefabs2D, ecrit par le NOM du prefab.
//     Un identifiant de session ecrit tel quel designe, a la reouverture, ce
//     qui a ete charge en n-ieme — donc un autre son, ou aucun.
//   - `transitoire` : un etat d'EXECUTION (une voix audio en cours) n'entre ni
//     dans le fichier ni dans la fusion d'un prefab.
//
// ⚠️ POURQUOI PAS NKReflection (mesure du 2026-09-29, la regle du depot)
//   Le pont NKECS <-> NKReflection (NkReflectBridge.h) existe et sert
//   NkEntitySerialization. Il ne convient pas ICI, pour trois raisons lues dans
//   le code et non supposees :
//     1. il range NkVec2f et les tableaux fixes en NK_STRUCT sans sous-classe :
//        NkReflectSerializer les PERD (et le dit : il rend false) — or
//        NkAnimSprite2D est un tableau de clips, NkVitesse2D un NkVec2f ;
//     2. ses tables se remplissent par objets statiques (NK_REFLECT_BEGIN) dans
//        l'unite de compilation du composant : dans une bibliotheque statique,
//        l'editeur de liens jette l'unite si rien ne la reference, et le
//        composant redevient muet sans erreur ;
//     3. il ne sait rien des REFERENCES : un son, une texture, une entite ne
//        s'ecrivent pas comme des entiers.
//   Cette description-ci ne remplace pas la reflexion : elle ne sert qu'a
//   sauver et a comparer, et elle vit a cote du composant qu'elle decrit.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYCHAMPS_H__
#define __NKENTSEU_UNKENY_NKUNKENYCHAMPS_H__

#include "NKCore/NkTypes.h"

#include <cstddef>
#include <cstring>

namespace nkentseu {
	namespace unkeny {

		/// Le genre d'un champ. AJOUTES A LA FIN : un fichier ne porte pas ces
		/// valeurs (il porte des noms de champ), mais une photo en memoire si.
		enum class NkTypeChamp : uint8 {
			NK_BOOL = 0,
			NK_I8,
			NK_U8,
			NK_I16,
			NK_U16,
			NK_I32,
			NK_U32,
			NK_I64,
			NK_U64,
			NK_F32,
			NK_F64,
			NK_VEC2,	///< math::NkVec2f
			NK_TEXTE,	///< char[N] termine par zero ; `taille` = N
			NK_ENTITE,	///< ecs::NkEntityId (8 octets) — ecrit par identifiant stable
			NK_SON,		///< uint32, identifiant NkSons2D — ecrit par nom
			NK_TEXTURE, ///< uint32, identifiant NkTextures2D — ecrit par nom
			NK_PREFAB	///< uint32, identifiant NkPrefabs2D — ecrit par nom
		};

		/// Un champ d'un composant.
		struct NkChampSauve {
				const char *nom = nullptr; ///< cle dans le fichier ; chaine STATIQUE (litteral)
				NkTypeChamp type = NkTypeChamp::NK_U8;
				uint32 decalage = 0; ///< offsetof du premier element
				uint32 taille = 0;	 ///< sizeof d'UN element
				uint32 nombre = 1;	 ///< > 1 : tableau
				uint32 pas = 0;		 ///< ecart entre deux elements ; 0 = `taille`
				bool transitoire = false;

				uint32 Pas() const noexcept {
					return pas != 0u ? pas : taille;
				}
		};

		/// La taille qu'un genre EXIGE (0 = libre : le texte).
		inline uint32 NkTailleDuGenre(NkTypeChamp t) noexcept {
			switch (t) {
				case NkTypeChamp::NK_BOOL:
				case NkTypeChamp::NK_I8:
				case NkTypeChamp::NK_U8:
					return 1u;
				case NkTypeChamp::NK_I16:
				case NkTypeChamp::NK_U16:
					return 2u;
				case NkTypeChamp::NK_I32:
				case NkTypeChamp::NK_U32:
				case NkTypeChamp::NK_F32:
				case NkTypeChamp::NK_SON:
				case NkTypeChamp::NK_TEXTURE:
				case NkTypeChamp::NK_PREFAB:
					return 4u;
				case NkTypeChamp::NK_I64:
				case NkTypeChamp::NK_U64:
				case NkTypeChamp::NK_F64:
				case NkTypeChamp::NK_VEC2:
				case NkTypeChamp::NK_ENTITE:
					return 8u;
				case NkTypeChamp::NK_TEXTE:
				default:
					return 0u;
			}
		}

		/// Un champ est-il coherent avec le composant de `tailleComposant`
		/// octets ? (genre et taille d'accord, dernier element dans le composant)
		/// Une description fausse ecrirait hors du composant : elle est refusee
		/// a la declaration, pas decouverte a la relecture.
		inline bool NkChampValide(const NkChampSauve &c, uint32 tailleComposant) noexcept {
			if (c.nom == nullptr || c.nom[0] == '\0' || c.nombre == 0u || c.taille == 0u) {
				return false;
			}
			const uint32 exige = NkTailleDuGenre(c.type);
			if (exige != 0u && exige != c.taille) {
				return false;
			}
			const uint32 fin = c.decalage + (c.nombre - 1u) * c.Pas() + c.taille;
			return fin <= tailleComposant;
		}

		/// Les deux valeurs du champ `c` (tous ses elements) sont-elles egales ?
		/// Octet a octet : c'est ce que « l'utilisateur n'y a pas touche » veut
		/// dire pour la fusion d'un prefab (une valeur recopiee est recopiee
		/// exactement).
		inline bool NkChampEgal(const NkChampSauve &c, const uint8 *a, const uint8 *b) noexcept {
			for (uint32 i = 0; i < c.nombre; ++i) {
				const uint32 at = c.decalage + i * c.Pas();
				if (std::memcmp(a + at, b + at, c.taille) != 0) {
					return false;
				}
			}
			return true;
		}

		/// Recopie le champ `c` (tous ses elements) de `src` vers `dst`.
		inline void NkChampCopier(const NkChampSauve &c, uint8 *dst, const uint8 *src) noexcept {
			for (uint32 i = 0; i < c.nombre; ++i) {
				const uint32 at = c.decalage + i * c.Pas();
				std::memcpy(dst + at, src + at, c.taille);
			}
		}

	} // namespace unkeny
} // namespace nkentseu

/// Un champ simple : NK_UNKENY_CHAMP(NkSource2D, volume, NkTypeChamp::NK_F32)
#define NK_UNKENY_CHAMP(Type, champ, genre)                                                                            \
	::nkentseu::unkeny::NkChampSauve {                                                                                 \
		#champ, genre, static_cast<::nkentseu::uint32>(offsetof(Type, champ)),                                         \
			static_cast<::nkentseu::uint32>(sizeof(static_cast<Type *>(nullptr)->champ)), 1u, 0u, false               \
	}

/// Un etat d'execution : decrit (pour la comparaison), jamais ecrit.
#define NK_UNKENY_CHAMP_TRANSITOIRE(Type, champ, genre)                                                                \
	::nkentseu::unkeny::NkChampSauve {                                                                                 \
		#champ, genre, static_cast<::nkentseu::uint32>(offsetof(Type, champ)),                                         \
			static_cast<::nkentseu::uint32>(sizeof(static_cast<Type *>(nullptr)->champ)), 1u, 0u, true                \
	}

/// Un champ d'un TABLEAU DE STRUCTURES, ecrit sous la cle `cle` :
/// NK_UNKENY_CHAMP_TABLEAU(NkAnimSprite2D, clips, NkClipSprite, nombre, NkTypeChamp::NK_U16, "clips.nombre")
#define NK_UNKENY_CHAMP_TABLEAU(Type, tableau, Element, champ, genre, cle)                                            \
	::nkentseu::unkeny::NkChampSauve {                                                                                 \
		cle, genre, static_cast<::nkentseu::uint32>(offsetof(Type, tableau) + offsetof(Element, champ)),              \
			static_cast<::nkentseu::uint32>(sizeof(static_cast<Element *>(nullptr)->champ)),                          \
			static_cast<::nkentseu::uint32>(sizeof(static_cast<Type *>(nullptr)->tableau) / sizeof(Element)),          \
			static_cast<::nkentseu::uint32>(sizeof(Element)), false                                                   \
	}

#endif // __NKENTSEU_UNKENY_NKUNKENYCHAMPS_H__
