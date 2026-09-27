#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiDispositions.h
// @Brief   P10 — la section `layout` : les dispositions d'amarrage, dans le document.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUE LA SPÉCIFICATION DEMANDE, MOT POUR MOT
// =============================================================================
//      layout "animation" {
//        dock "vue"         center
//        dock "scene"       left   0.16
//        dock "details"     right  0.22
//        dock "sequenceur"  bottom 0.30
//      }
//
//  « Un espace de travail EST une disposition. Aujourd'hui elle vivrait dans le
//  C++ ; elle doit vivre dans le document, comme le reste. » (doc 09 §1 P10)
//
// =============================================================================
//  CE FICHIER NE POSE AUCUN PANNEAU, ET C'EST LE POINT
// =============================================================================
//  ⚠️ LE MONTEUR N'AMARRE RIEN, ET NE LE POURRA JAMAIS. Amarrer, c'est décider
//     de la place d'un PANNEAU de l'application — un objet que le document ne
//     connaît pas et que le monteur ne possède pas. C'est l'hôte qui tient les
//     panneaux, comme c'est lui qui tient les actions, les zones, le thème et
//     les icônes.
//
//     Ce fichier fait donc ce que le document 09 décrit : il LIT la section,
//     l'expose, et compte ses refus. L'hôte applique. Le partage est le même que
//     pour `Host` : *le document dit OÙ, l'application garde QUAND et COMMENT.*
//
//  ⚠️ ET « ÉCRIRE LA SECTION » N'EST PAS RIEN. La spécification dit, pour
//     l'attente : « écrire la section (elle voyage et se compte, comme
//     `animation`), et l'application pose la disposition en dur ». Une section
//     lue et exposée franchit exactement la marche qui sépare « elle voyage »
//     de « quelqu'un peut s'en servir » — sans que le document change d'un
//     caractère le jour où l'hôte s'en saisit.
// =============================================================================
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKGui/Doc/NkGuiCibles.h" // P27 : la cible d'une disposition
#include "NKSerialization/NkGui/NkGuiArchive.h"

namespace nkentseu {
	namespace nkgui {

		/// Les cinq côtés d'un amarrage. `Centre` n'en est pas un : c'est la
		/// zone qui reste quand les quatre autres ont pris leur part.
		enum class NkGuiCote : uint8 {
			Centre = 0,
			Gauche,
			Droite,
			Haut,
			Bas
		};

		/// Un panneau et la place qu'il demande.
		struct NkGuiAmarrage {
				NkString panneau;  ///< le nom que l'hôte connaît
				NkGuiCote cote = NkGuiCote::Centre;
				/// La fraction de la zone parente, entre 0 et 1. ⚠️ ABSENTE pour
				/// `center` : le centre prend ce qui reste, il ne se dimensionne
				/// pas. Lui donner une fraction serait lui faire dire deux choses.
				float32 fraction = 0.f;
				bool aFraction = false;
		};

		/// Une disposition nommée — un espace de travail.
		struct NkGuiDisposition {
				NkString nom;
				NkVector<NkGuiAmarrage> amarrages;
				/// P27 — LA CIBLE DE CETTE DISPOSITION (27/09). `Toutes` = elle vaut
				/// partout, et c'est le défaut.
				///
				/// ⚠️ ELLE EST ICI PARCE QUE L'AMARRAGE EST CE QUI CHANGE LE PLUS D'UNE
				///    PLATEFORME À L'AUTRE. Une colonne d'outils à gauche sur un PC
				///    devient une barre en bas sur un téléphone : ce n'est pas une
				///    question de TAILLE — `sizeRel` ne sait pas déplacer un panneau d'un
				///    bord à l'autre — c'est une question de STRUCTURE. Deux `layout` de
				///    même nom et de cibles différentes cohabitent donc dans un seul
				///    document :
				///
				///     layout "defaut" {                    dock "outils" left 0.16   }
				///     layout "defaut" { platform = Mobile  dock "outils" bottom 0.3  }
				NkGuiCible cible = NkGuiCible::Toutes;
		};

		/// Ce qu'une lecture a trouvé, et ce qu'elle a REFUSÉ.
		///
		/// ⚠️ TROIS REFUS DISTINCTS, PARCE QU'ILS N'ONT PAS LA MÊME CAUSE. Un
		///    côté mal orthographié est une faute d'écriture ; une fraction hors
		///    de [0,1] est une faute de valeur ; deux centres sont une faute de
		///    conception. Les confondre rendrait le diagnostic inutile.
		struct NkGuiRapportDispositions {
				uint32 dispositions = 0;
				uint32 amarrages = 0;
				uint32 cotesInconnus = 0;
				uint32 fractionsHorsBornes = 0;
				uint32 centresMultiples = 0;
				NkVector<NkString> refuses;

				bool Propre() const noexcept {
					return cotesInconnus == 0u && fractionsHorsBornes == 0u
						   && centresMultiples == 0u;
				}
		};

		/// Lit toutes les sections `layout` d'un document.
		///
		/// ⚠️ ELLE N'EFFACE PAS `out`. Un espace de travail peut se composer de
		///    plusieurs documents — celui de l'application et un commun. Effacer
		///    d'office interdirait la composition.
		bool NkGuiLireDispositions(const NkArchive &doc, NkVector<NkGuiDisposition> &out,
								   NkGuiRapportDispositions &rap) noexcept;

		/// Le nom d'un côté, pour les traces et les messages. Jamais l'inverse :
		/// la lecture se fait par comparaison, pas par cette table.
		const char *NkGuiNomCote(NkGuiCote c) noexcept;

	} // namespace nkgui
} // namespace nkentseu

#include "NKGui/Doc/NkGuiDispositions.inl"
