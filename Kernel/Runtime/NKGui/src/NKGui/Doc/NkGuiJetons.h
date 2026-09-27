#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiJetons.h
// @Brief   P1 — LES JETONS DE COULEUR `@nom`, et le thème qui les résout.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  LA DEMANDE, ET ELLE VIENT DE DEUX ENDROITS À LA FOIS
// =============================================================================
//  Rodolf, le 27/09/2026 : « on peut définir pour chaque application créée un
//  SYSTÈME DE THÈME, donc l'utilisateur décide lui-même de définir le thème de
//  son application ».
//
//  Et les deux spécifications d'interface le demandent, dans les mêmes termes :
//  « toutes les couleurs viennent de jetons de thème ; aucune couleur en dur
//  hors des jetons d'information » (NkAnimaEditor, doc 09 §0.3), et P1 figure en
//  tête des rôles « consommés, donc prioritaires pour le monteur » (NKUIDesign,
//  doc 21 §1).
//
// =============================================================================
//  POURQUOI LA RÉSOLUTION N'EST PAS UNE PASSE SUR L'ARCHIVE
// =============================================================================
//  Les deux autres passes d'avant-montage — les `include` et les `component` —
//  RÉÉCRIVENT l'archive : le contenu inclus prend la place de la ligne, le
//  patron prend la place de l'instance. La tentation était d'en écrire une
//  troisième qui remplace `@info.ecrit` par `#F79A28`.
//
//  🔴 ELLE AURAIT DÉTRUIT CE QU'ELLE SERT. Un document réécrit après cette passe
//     porterait la COULEUR et non le JETON : le fichier perdrait son thème à la
//     première sauvegarde, et l'éditeur qui l'ouvre ne verrait plus qu'un hexa.
//     Le jeton n'est pas une abréviation à développer, c'est **la valeur que le
//     document déclare** ; c'est le THÈME qui est l'information de second ordre.
//
//  ⚠️ ET UN THÈME SE CHANGE PENDANT QUE L'APPLICATION TOURNE. Une passe
//     d'avant-montage obligerait à relire le document à chaque changement de
//     thème. Ici, reposer la table suffit : l'image suivante peint le nouveau
//     thème. C'est exactement ce que « l'utilisateur décide du thème de son
//     application » demande.
//
//  La résolution vit donc **dans la lecture de la couleur**, et dans une seule.
//
// =============================================================================
//  ET IL N'Y AVAIT PAS UNE PORTE, IL Y EN AVAIT DEUX
// =============================================================================
//  🔴 MESURE DU 27/09, avant d'écrire une ligne : `NkGuiCouleur` (monteur,
//     3 appelants) et `NkGuiCouleurDepuisLexeme` (exécution, 3 appelants) sont
//     **deux implémentations indépendantes de la même syntaxe** — l'une passe
//     par `NkColorF::FromHex`, l'autre par `NkParseHex` et décale les octets à
//     la main. Elles s'accordent aujourd'hui ; rien ne les y oblige.
//
//     Y ajouter `@nom` DEUX FOIS aurait doublé la divergence au lieu de la
//     réduire. La seconde porte est donc devenue un renvoi vers la première.
//     *Deux analyseurs du même texte finissent par ne plus être d'accord sur ce
//     qu'il dit.*
// =============================================================================
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKGui/Core/NkGuiTypes.h" // NkColor (l'alias de la couche, pas NKMath en direct)
#include "NKSerialization/NkGui/NkGuiArchive.h"

namespace nkentseu {
	namespace nkgui {

		/// Les deux variantes qu'un thème porte. La spécification les nomme
		/// « Sombre (défaut) » et « Clair » (doc 09 §2).
		enum class NkGuiVarianteTheme : uint8 {
			Sombre = 0,
			Clair = 1
		};

		/// Un jeton : son nom AVEC son `@`, ses deux valeurs, et son sens.
		///
		/// ⚠️ LE SENS EST OBLIGATOIRE, ET CE N'EST PAS DE LA DOCUMENTATION. La
		///    règle de la famille est « n'écris une couleur que si elle porte une
		///    information » ; un jeton sans phrase ne dit pas QUELLE information,
		///    donc il n'a pas de raison d'exister. Le document 20 §11.1 le pose
		///    en toutes lettres : « un jeton sans phrase ne s'enregistre pas ».
		struct NkGuiJeton {
				NkString nom;	///< `@info.ecrit`, `@` compris
				NkString sens;	///< la phrase, obligatoire
				NkColor sombre{0, 0, 0, 255};
				NkColor clair{0, 0, 0, 255};
				bool aSombre = false;
				bool aClair = false;
		};

		/// Ce qu'une lecture de thème a trouvé, et ce qu'elle a REFUSÉ.
		///
		/// ⚠️ UN JETON REFUSÉ SE NOMME. Un thème qui laisserait tomber une entrée
		///    en silence ferait retomber la couleur sur « non résolue » très loin
		///    de là, dans un document qui n'y est pour rien.
		struct NkGuiRapportTheme {
				NkString nomTheme;			  ///< `theme "UE5 Rihen"`
				uint32 jetons = 0;			  ///< entrées retenues
				uint32 sansSens = 0;		  ///< refusées faute de phrase
				uint32 sansValeur = 0;		  ///< refusées faute de couleur lisible
				uint32 debordees = 0;		  ///< au-delà de la capacité de la table
				NkVector<NkString> refuses;	  ///< chacune, nommée

				bool Propre() const noexcept {
					return sansSens == 0u && sansValeur == 0u && debordees == 0u;
				}
		};

		/// La table posée par l'hôte. Capacité fixe : ce noyau est zero-STL, et
		/// un thème de famille compte quelques dizaines de jetons.
		///
		/// ⚠️ LA TABLE APPARTIENT À L'HÔTE, LE MONTEUR N'EN GARDE QU'UN POINTEUR.
		///    Ce dépôt a payé la leçon (« le registre garde un pointeur : déclarer
		///    par valeur = segfault mouvant ») : la table doit vivre AUSSI
		///    LONGTEMPS que les montages qui la lisent. Une table locale à une
		///    fonction qui rend la main est un défaut, pas une commodité.
		class NkGuiTableJetons {
			public:
				static const uint32 kMax = 128u;

				void Vider() noexcept {
					mNb = 0u;
				}

				uint32 Nombre() const noexcept {
					return mNb;
				}

				const NkGuiJeton &A(uint32 i) const noexcept {
					return mJetons[i];
				}

				NkGuiVarianteTheme Variante() const noexcept {
					return mVariante;
				}

				/// Changer de variante ne relit RIEN : c'est ce qui rend le
				/// basculement Sombre / Clair immédiat.
				void PoserVariante(NkGuiVarianteTheme v) noexcept {
					mVariante = v;
				}

				bool Ajouter(const NkGuiJeton &j) noexcept {
					if (mNb >= kMax)
						return false;
					mJetons[mNb++] = j;
					return true;
				}

				/// Rend la couleur du jeton `nom` (avec son `@`) dans la variante
				/// courante. Faux = jeton inconnu, ou sans valeur pour CETTE
				/// variante — deux causes distinctes, jamais confondues avec
				/// « noir ».
				bool Resoudre(NkStringView nom, NkColor &out) const noexcept {
					for (uint32 i = 0; i < mNb; ++i) {
						if (!MemeNom(NkStringView(mJetons[i].nom.CStr()), nom))
							continue;
						const NkGuiJeton &j = mJetons[i];
						if (mVariante == NkGuiVarianteTheme::Clair) {
							if (!j.aClair)
								return false;
							out = j.clair;
							return true;
						}
						if (!j.aSombre)
							return false;
						out = j.sombre;
						return true;
					}
					return false;
				}

			private:
				static bool MemeNom(NkStringView a, NkStringView b) noexcept {
					if (a.Size() != b.Size())
						return false;
					for (usize i = 0; i < a.Size(); ++i)
						if (a.Data()[i] != b.Data()[i])
							return false;
					return true;
				}

				NkGuiJeton mJetons[kMax];
				uint32 mNb = 0u;
				NkGuiVarianteTheme mVariante = NkGuiVarianteTheme::Sombre;
		};

		/// Le registre : l'hôte pose SA table, le lecteur de couleur la consulte.
		///
		/// ⚠️ POURQUOI UN REGISTRE ET PAS UN PARAMÈTRE. La couleur se lit dans six
		///    endroits dont aucun n'a de contexte sous la main
		///    (`NkGuiLireApparenceRepos(w)` ne reçoit qu'un bloc d'archive). Faire
		///    descendre une table jusque-là aurait changé la signature de tout le
		///    monteur pour une propriété que le document déclare. C'est la même
		///    forme que les tables d'actions et de zones, déjà posées par l'hôte.
		NkGuiTableJetons *NkGuiJetonsPoses() noexcept;
		void NkGuiPoserJetons(NkGuiTableJetons *table) noexcept;

		// =====================================================================
		//  P12 — LE JEU D'ICÔNES, POSÉ PAR L'HÔTE
		// =====================================================================
		//  `icon` est au schéma de `Button`, `ToggleButton`, `Tile`,
		//  `SplitButton`, `MenuItem`… et les spécifications d'interface en
		//  écrivent des centaines (noms Lucide). Le monteur ne peut pas les
		//  porter : un jeu d'icônes est une RESSOURCE de l'application, comme
		//  ses actions et ses zones.
		//
		//  ⚠️ MÊME FORME QUE LES JETONS, ET C'EST DÉLIBÉRÉ. Trois tables posées
		//     par l'hôte (actions, zones, jetons) : une quatrième qui s'y
		//     ajoute ne demande rien de nouveau à apprendre. Et `NkGuiIconSet`
		//     distingue déjà `Glyph` de `Fallback` : le monteur peut donc
		//     COMPTER une icône manquante au lieu de peindre un carré muet.
		class NkGuiIconSet;
		const NkGuiIconSet *NkGuiIconesPosees() noexcept;
		void NkGuiPoserIcones(const NkGuiIconSet *jeu) noexcept;

		/// ⚠️ DÉCLARÉE ICI, DÉFINIE DANS `NkGuiMonteur.h` — et c'est voulu. Lire
		///    un thème, c'est lire des couleurs : si ce fichier en analysait une
		///    lui-même, il y aurait **trois** analyseurs de la même syntaxe là où
		///    P1 vient justement d'en ramener deux à un. La déclaration suffit :
		///    la définition est dans la même unité de traduction, plus bas.
		inline bool NkGuiCouleur(NkStringView lex, NkColor &out) noexcept;

		/// Lit la section `theme "nom" { jeton "@x" { sombre = #..., clair = #...,
		/// sens = "..." } }` d'un document et remplit la table.
		///
		/// ⚠️ ELLE N'EFFACE PAS LA TABLE. Un thème peut se composer de plusieurs
		///    documents (`Commun/theme.nkgui` puis celui de l'application) ;
		///    effacer d'office interdirait la surcharge. L'appelant vide s'il veut
		///    repartir de zéro.
		bool NkGuiLireTheme(const NkArchive &doc, NkGuiTableJetons &table,
							NkGuiRapportTheme &rap) noexcept;

	} // namespace nkgui
} // namespace nkentseu

#include "NKGui/Doc/NkGuiJetons.inl"
