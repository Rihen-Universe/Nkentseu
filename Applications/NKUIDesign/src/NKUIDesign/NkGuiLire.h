#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiLire.h
// @Brief   `.nkgui` -> le modèle d'édition. L'inverse de `NkGuiEcrire.h`.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  LE CHAÎNON QUI MANQUAIT — chantier A, le premier de R1
// =============================================================================
//  Document 5 §2.1 : « **ouvrir un `.nkgui` dans le modèle** — ✚ le premier
//  chantier (R1) », seule ligne du tableau des briques marquée « à construire ».
//  Et §2.2 : « 1. `NkUIDocument::FromArchive(const NkGuiArchive&)` : construit le
//  modèle, chaque nœud gardant sa référence d'archive (§1.1, invariant 3). Les
//  sections et membres inconnus restent bruts et sont listés. »
//
//  Mesuré avant d'écrire : `NkDocumentVersArchive` existe (modèle -> archive), et
//  **rien** ne fait l'inverse. NKUIDesign savait EXPORTER son document en `.nkgui`
//  et ne savait pas en OUVRIR un. C'est pourquoi la spécification interdit d'écrire
//  le moindre document d'interface avant ce chantier : « l'outil doit d'abord savoir
//  ouvrir un `.nkgui`, sinon il ne pourrait pas éditer sa propre interface ».
//
// =============================================================================
//  CE QUE CE LECTEUR NE FAIT PAS, ET IL FAUT LE LIRE AVANT DE S'EN SERVIR
// =============================================================================
//  ⚠️ IL NE REMPLACE PAS L'ARCHIVE, IL LA DOUBLE. Le document 5 §1.1 décrit TROIS
//     couches : l'archive possède **l'octet** (lexèmes, commentaires, lignes vides,
//     style), le modèle possède **le sens**, la méta ce qui n'est pas l'interface.
//     L'appelant GARDE donc l'archive à côté du modèle : c'est elle qui se réécrit,
//     par opérations localisées, et c'est le seul moyen de tenir l'invariant 2
//     (« une modification ne touche que ses lignes »).
//
//     Régénérer le fichier depuis le modèle — ce que `NkDocumentVersTexte` fait —
//     réécrirait TOUT : les commentaires disparaîtraient, et « les documents de la
//     famille sont très commentés » (doc 19 A : « un éditeur qui perd les
//     commentaires détruit la moitié de la valeur du fichier »).
//
//  ⚠️ IL NE LIT QUE `widgets`. Les sections `behavior`, `animation`, `theme`,
//     `layout`, `component`, `geometry`, `application`, `test` ne sont PAS perdues
//     pour autant : elles restent dans l'archive, que l'appelant garde. Elles ne
//     sont simplement pas encore représentées dans le modèle — et le rapport les
//     COMPTE, une par une, plutôt que de les passer sous silence.
//
//  ⚠️ ET CE QU'IL NE SAIT PAS PORTER, IL LE COMPTE. C'est le chiffre qui dit
//     « ouvrir ce fichier perd de l'information ». Sans lui, un aller-retour par le
//     modèle dégraderait les documents en silence — et le silence est exactement ce
//     que ce dépôt a passé la journée à traquer.
// -----------------------------------------------------------------------------

#ifndef __NKENTSEU_NKUIDESIGN_NKGUILIRE_H__
#define __NKENTSEU_NKUIDESIGN_NKGUILIRE_H__

#include "NKGui/Doc/NkGuiMonteur.h"
#include "NKSerialization/NkGui/NkGuiArchive.h"
#include "Document.h"

namespace nkuidesign {
	namespace guifmt {

		using nkentseu::NkArchive;
		using nkentseu::NkArchiveNode;
		using nkentseu::NkGuiArchive;
		using nkentseu::NkString;
		using nkentseu::NkStringView;
		using nkentseu::NkVector;
		using nkentseu::float32;
		using nkentseu::int32;
		using nkentseu::uint32;

		/// Ce qu'une lecture a construit, et ce qu'elle n'a pas su porter.
		///
		/// 🔴 `attributsNonPortes` EST LE CHIFFRE QUI COMPTE. Les autres disent que
		///    la lecture a marché ; celui-ci dit ce qu'elle a COÛTÉ. Un modèle qui
		///    reçoit 40 nœuds sur 40 mais laisse tomber `tooltip`, `shortcut` et
		///    `enabled` a l'air parfait dans les trois premiers compteurs.
		struct NkLectRapport {
				uint32 sections = 0;	 ///< sections `widgets` rencontrées
				uint32 blocs = 0;		 ///< blocs de widget rencontrés
				uint32 noeuds = 0;		 ///< nœuds créés dans le modèle
				uint32 attributsLus = 0; ///< attributs que le modèle a pris
				uint32 attributsNonPortes = 0; ///< ceux qu'il ne sait pas tenir
				uint32 sectionsNonLues = 0;	   ///< `behavior`, `theme`, `layout`…
				/// Les NOMS, pas seulement les comptes : « il manque quelque chose »
				/// sans dire quoi se cherche à la main. Bornés, pour qu'un document
				/// de mille pertes ne produise pas mille chaînes.
				NkVector<NkString> nonPortes;
				NkVector<NkString> sectionsIgnorees;

				void NoterNonPorte(const NkString &nom) noexcept {
					++attributsNonPortes;
					for (uint32 i = 0; i < (uint32)nonPortes.Size(); ++i)
						if (nonPortes[i].Compare(nom) == 0)
							return; // déjà nommé : on compte, on ne répète pas
					if (nonPortes.Size() < 32u)
						nonPortes.PushBack(nom);
				}
		};

		namespace detail {

			/// Les attributs que le modèle sait TENIR aujourd'hui. Tout le reste est
			/// compté — et cette liste est la seule vérité sur ce que la lecture
			/// conserve.
			///
			/// ⚠️ ELLE EST COURTE, ET C'EST HONNÊTE. `tooltip`, `shortcut`, `icon`,
			///    `enabled`, `visible`, `platform`, `gap`, `align`… voyagent dans
			///    l'archive mais n'ont pas de champ dans `NkUINode`. Les déclarer
			///    « portés » ici aurait fait croire à une fidélité qu'on n'a pas.
			inline bool NkLAttributPorte(const NkString &cle) noexcept {
				static const char *kPortes[] = {"label", "text", "size", "pos", "bind"};
				for (uint32 i = 0; i < (uint32)(sizeof(kPortes) / sizeof(kPortes[0])); ++i)
					if (cle.Compare(kPortes[i]) == 0)
						return true;
				return false;
			}

			/// `size = (220, 32)` est un JETON NU : deux nombres entre parenthèses.
			///
			/// ⚠️ ELLE APPELLE LE LECTEUR DU MONTEUR, elle ne le refait pas.
			///    `NkGNombresDansLexeme` est le site unique où ce dépôt transforme un
			///    lexème en nombres, et il y est pour deux raisons qu'un analyseur
			///    écrit ici perdrait : il ignore les délimiteurs (`(120, 80)` et
			///    `[1, 2, 3]` passent par la même porte), et il n'utilise pas `atof`,
			///    qui lit selon la CULTURE — en fr-FR, `atof("1.5")` rend 1.
			///    Un second analyseur, même juste aujourd'hui, dériverait du premier
			///    dès la première extension du format. Une dérivation en double n'est
			///    pas une sécurité : c'est deux vérités qui divergeront.
			inline bool NkLVec2(const NkArchive &b, const char *cle, float32 &x,
								float32 &y) noexcept {
				const NkArchiveNode *n = b.FindNode(NkStringView(cle));
				if (!n)
					return false;
				float32 v[4];
				if (nkentseu::nkgui::NkGNombresDansLexeme(n->Lexeme(), v, 4u) < 2u)
					return false;
				x = v[0];
				y = v[1];
				return true;
			}

			/// Un bloc de widget, et sa descendance.
			///
			/// ⚠️ LE CHEMIN SE CONSTRUIT EN DESCENDANT, et il est passé par VALEUR :
			///    chaque enfant reçoit celui de son parent plus son propre indice.
			///    Le partager par référence aurait fait porter à un frère le chemin
			///    de son aîné — un défaut qui ne se verrait qu'à la première
			///    modification, c'est-à-dire trop tard.
			inline void NkLBloc(const NkArchive &w, NkUIDocument &doc, int32 parent,
								NkVector<uint32> chemin, NkLectRapport &rap) noexcept {
				++rap.blocs;
				const int32 idx = doc.AddChild(parent, "", NkAuthor::Humain, "nkgui");
				if (idx < 0)
					return;
				++rap.noeuds;
				{
					NkUINode &n = doc.nodes[(uint32)idx];
					// LE RÔLE, VERBATIM. L'écrivain le rend inchangé quand il le
					// reconnaît (`NkRoleNkguiDuNoeud`, branche 1) : c'est ce qui fait
					// qu'un document lu puis réécrit garde ses rôles.
					n.role = NkString(NkGuiArchive::TypeOf(w));
					// L'IDENTIFIANT DU FICHIER — voir `NkUINode::idNkgui`. Sans lui,
					// l'écrivain en fabriquerait un et débrancherait toutes les
					// actions du document.
					n.idNkgui = NkString(NkGuiArchive::IdOf(w));
					n.refArchive = chemin;
					if (!n.idNkgui.Empty())
						n.label = n.idNkgui; // faute de mieux : le libellé se corrige
				}
				// ── LES ATTRIBUTS ────────────────────────────────────────────
				const NkArchiveNode *corps = w.FindNode(NkStringView(NkGuiArchive::KeyBody()));
				{
					NkString s;
					if (w.GetString(NkStringView("label"), s)) {
						doc.nodes[(uint32)idx].label = s;
						++rap.attributsLus;
					}
					if (w.GetString(NkStringView("text"), s)) {
						doc.nodes[(uint32)idx].text = s;
						++rap.attributsLus;
					}
					float32 x = 0.f, y = 0.f;
					if (NkLVec2(w, "size", x, y)) {
						NkUINode &n = doc.nodes[(uint32)idx];
						n.width.mode = NkSizeMode::Fixed;
						n.width.value = x;
						n.height.mode = NkSizeMode::Fixed;
						n.height.value = y;
						++rap.attributsLus;
					}
					if (NkLVec2(w, "pos", x, y)) {
						doc.nodes[(uint32)idx].posX = x;
						doc.nodes[(uint32)idx].posY = y;
						++rap.attributsLus;
					}
				}
				// Ce que le modèle ne sait pas tenir : COMPTÉ et NOMMÉ.
				{
					const NkVector<nkentseu::NkArchiveEntry> &ents = w.Entries();
					for (uint32 e = 0; e < (uint32)ents.Size(); ++e) {
						const NkString &cle = ents[e].key;
						// Les clés réservées portent la STRUCTURE (type, id, corps),
						// pas un attribut : le nœud les a déjà prises plus haut.
						if (NkGuiArchive::IsReservedKey(NkStringView(cle)))
							continue;
						if (!NkLAttributPorte(cle))
							rap.NoterNonPorte(cle);
					}
				}
				// ── LA DESCENDANCE ───────────────────────────────────────────
				if (!corps)
					return;
				for (uint32 k = 0; k < (uint32)corps->array.Size(); ++k) {
					if (!corps->array[k].IsObject() || !corps->array[k].object)
						continue;
					NkVector<uint32> sous = chemin;
					sous.PushBack(k);
					NkLBloc(*corps->array[k].object, doc, idx, sous, rap);
				}
			}

		} // namespace detail

		/// Construit le modèle d'édition depuis une archive `.nkgui` lue.
		///
		/// ⚠️ L'APPELANT GARDE L'ARCHIVE. Ce modèle est une VUE éditable ; l'octet
		///    reste à l'archive, et c'est elle qu'on réécrit. Voir l'en-tête.
		inline bool NkArchiveVersDocument(const NkArchive &ar, NkUIDocument &doc,
										  NkLectRapport &rap) noexcept {
			const NkArchiveNode *corps = ar.FindNode(NkStringView(NkGuiArchive::KeyBody()));
			if (!corps)
				return false;
			// ⚠️ LA TOILE N'EXISTE PAS AVANT QU'ON LA DEMANDE. `NkUIDocument()` est
			//    `= default` : il laisse `nodes` VIDE, et la racine (nœud 0) naît
			//    dans `NewDocument`. Sans cet appel, `AddChild(0, …)` échoue sur
			//    `IsValidIndex(0)` et rend -1 — silencieusement, pour chaque bloc.
			//    Mesuré : 23 blocs lus, 0 nœud construit, aucune erreur signalée.
			//    On ne le fait QUE sur un document vierge : l'appelant qui a déjà
			//    appelé `NewDocument` (avec son titre, son auteur) le garde.
			if (doc.nodes.Empty())
				doc.NewDocument("Interface", NkAuthor::Humain);
			for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
				if (!corps->array[i].IsObject() || !corps->array[i].object)
					continue;
				const NkArchive &sec = *corps->array[i].object;
				const NkString nom(NkGuiArchive::TypeOf(sec));
				if (nom.Compare("widgets") != 0) {
					// ⚠️ NON LUE N'EST PAS PERDUE : la section reste dans l'archive,
					//    que l'appelant garde. Mais elle se COMPTE et se NOMME — un
					//    document dont le `behavior` n'arrive pas au modèle doit
					//    pouvoir se lire dans le relevé, pas se découvrir à l'usage.
					++rap.sectionsNonLues;
					bool deja = false;
					for (uint32 k = 0; k < (uint32)rap.sectionsIgnorees.Size(); ++k)
						if (rap.sectionsIgnorees[k].Compare(nom) == 0)
							deja = true;
					if (!deja && rap.sectionsIgnorees.Size() < 16u)
						rap.sectionsIgnorees.PushBack(nom);
					continue;
				}
				++rap.sections;
				const NkArchiveNode *cw = sec.FindNode(NkStringView(NkGuiArchive::KeyBody()));
				if (!cw)
					continue;
				for (uint32 k = 0; k < (uint32)cw->array.Size(); ++k) {
					if (!cw->array[k].IsObject() || !cw->array[k].object)
						continue;
					NkVector<uint32> chemin;
					chemin.PushBack(i);
					chemin.PushBack(k);
					// Les widgets de premier niveau sont les enfants de la TOILE
					// (nœud 0) — symétrique de l'écrivain, qui écrit
					// `racine.children[i]` dans `widgets`.
					detail::NkLBloc(*cw->array[k].object, doc, 0, chemin, rap);
				}
			}
			return true;
		}

	} // namespace guifmt
} // namespace nkuidesign

#endif // __NKENTSEU_NKUIDESIGN_NKGUILIRE_H__

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
