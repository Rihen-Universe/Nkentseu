#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiCoquille.h
// @Brief   MONTER UN DOCUMENT `.nkgui` DANS UNE APPLICATION : une bande, ses
//          actions nommees, ses zones nommees. La partie que toutes les
//          applications partagent.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE FICHIER EXISTE, ET POURQUOI IL EST ICI
// =============================================================================
//  Il est ne dans `Applications/NkAnimaEditor/` le 25/09, ou il a prouve le fil
//  complet : un document sur le disque -> une interface a l'ecran, sans
//  recompiler. Le 26/09, le meme cablage est accorde pour Nogee.
//
//  ⚠️ LE RECOPIER AURAIT ETE LA FAUTE QUE CE DEPOT NOMME. Deux exemplaires de la
//     meme semantique divergent -- *deux compteurs sans code commun*, *une
//     derivation en double : l'un publie, l'autre lit*. Le depot porte deja
//     quatre jeux d'icones pour la meme raison. On partage, on ne duplique pas.
//
//  ⚠️ ET LE DOMICILE N'EST PAS `NKEditorKit`. Ce fichier ne sait RIEN de la
//     coquille d'editeur, et c'est une condition, pas un hasard : NKGui est SOUS
//     NKEditorKit dans le graphe. Un en-tete de NKGui qui inclurait NKEditorKit
//     serait une inversion de couches -- la faute exacte que NKEditorKit a
//     corrigee le 2026-09-01 en RETIRANT NKCanvas de son jenga.
//
//     Ce qui connait `NkEditorFrameContext` et `NkEditorPanel` reste donc chez
//     CHAQUE application, et ne fait que quelques lignes : les crochets de bande
//     et la sous-classe de panneau. C'est la frontiere, et elle est verifiee --
//     le script de scission refuse de s'executer si un seul nom de NKEditorKit
//     traverse cette ligne.
//
// =============================================================================
//  OU IL VIT -- MEME REGLE QUE LE MONTEUR, POUR LA MEME RAISON
// =============================================================================
//  EN-TETE SEUL, non compile : `NKGui.jenga` fait `files(["src/**.cpp"])`, un
//  `.h` n'y ajoute rien, et NKGui ne gagne aucune dependance. C'est
//  l'APPLICATION qui l'inclut qui declare NKSerialization et NKFileSystem --
//  exactement ce que `NkGuiMonteur.h` dit de lui-meme, et pour le meme motif.
// -----------------------------------------------------------------------------

#include "NKContainers/String/NkString.h"
#include "NKFileSystem/NkFile.h"
#include "NKGui/Doc/NkGuiComposants.h"  // les composants se developpent avant le montage
#include "NKGui/Doc/NkGuiInteraction.h" // tire NkGuiMonteur + NkGuiArchive

namespace nkentseu {
	namespace nkgui {

		// =========================================================================
		//  CE QUE L'APPLICATION FOURNIT : des actions et des zones, par NOM
		// =========================================================================
		using NkActionFn = void (*)(void *);

		struct NkActionNommee {
				const char *nom = nullptr; ///< l'identifiant du bouton dans le document
				NkActionFn fn = nullptr;
				void *user = nullptr;
		};

		/// Ce que l'hôte sait peindre dans une zone `Host "nom"`.
		using NkZoneFn = bool (*)(NkGuiContext &, const NkRect &, void *);

		struct NkZoneNommee {
				const char *nom = nullptr;
				NkZoneFn fn = nullptr;
				void *user = nullptr;
		};

		// =========================================================================
		//  UNE BANDE : un fichier, son archive, son état, son exécution
		// =========================================================================
		/**
		 * ⚠️ UNE EXÉCUTION PAR BANDE, ET CE N'EST PAS DU CONFORT. `NkGuiInfosDoc`
		 *    est indexée par identifiant de widget, et `NkGuiMonteEtat` par clé de
		 *    `bind`. Deux documents partageant une exécution partageraient leurs
		 *    identifiants : deux boutons « fermer » dans deux bandes deviendraient
		 *    le même. Le format ne garantit l'unicité qu'à l'intérieur d'un fichier.
		 */
		class NkBandeDocument : public NkGuiExecution {
			public:
				// ── l'état de chargement, et il se dit ────────────────────────
				bool lu = false;		 ///< l'archive a été lue
				NkString chemin;		 ///< d'où elle vient
				NkString refus;			 ///< pourquoi elle n'a pas été lue (jamais vide si !lu)
				NkArchive doc; ///< le document
				NkGuiMonteEtat etat;	 ///< le magasin des valeurs éditées
				NkGuiMonteRapport rap;	 ///< le relevé du dernier montage

				// ── ce que ce montage a fait des actions ──────────────────────
				uint32 boutons = 0;			  ///< boutons montés à la dernière image
				uint32 actionsTirees = 0;	  ///< actions réellement appelées (cumul)
				uint32 actionsInconnues = 0;  ///< appuis sur un nom que personne ne sert (cumul)
				uint32 zonesRemplies = 0;	  ///< zones hôte servies à la dernière image

				// ── les tables que l'application pose ─────────────────────────
				const NkActionNommee *actions = nullptr;
				uint32 nbActions = 0;
				const NkZoneNommee *zones = nullptr;
				uint32 nbZones = 0;

				/// Ce que le développement des composants a fait, et ce qu'il a
				/// REFUSÉ de faire. Rempli par `Adopter`, lisible ensuite : un
				/// composant qu'on ne sait pas développer ne doit pas disparaître
				/// en silence.
				NkGuiRapportComposants composants;

				// ═════════════════════════════════════════════════════════════
				//  LA FAÇADE PREND UN ARBRE, JAMAIS UN CHEMIN
				// ═════════════════════════════════════════════════════════════
				/**
				 * Décision de conception de Rodolf, 25/09 : *« je ne veux rien perdre. »*
				 * Lire le document au lancement (on redessine sans recompiler) et
				 * l'embarquer à la compilation (rien à livrer, rien à lire) **ne sont pas
				 * deux produits** : ce sont deux CONFIGURATIONS DE CONSTRUCTION du même
				 * document.
				 *
				 * ⚠️ ET LE PIÈGE EST NOMMÉ : si l'embarquement émettait du C++ qui appelle
				 *    les widgets, il existerait **deux implémentations de ce que signifie
				 *    un document**, et elles divergeraient. Ce dépôt a déjà payé ce motif
				 *    (*deux compteurs sans code commun*, *une dérivation en double : l'un
				 *    publie, l'autre lit*). La parade n'est pas la vigilance, c'est la
				 *    forme : **le monteur reste la seule implémentation de la sémantique**,
				 *    et l'embarquement ne fait qu'économiser la lecture du texte.
				 *
				 * C'est pourquoi `Adopter` prend un **ARBRE DÉJÀ ANALYSÉ** et que la
				 * lecture du disque n'est qu'**un appelant parmi d'autres**. Le jour où un
				 * générateur émettra l'arbre en mémoire, il appellera `Adopter` et **rien
				 * d'autre ne changera ici**.
				 */
				bool Adopter(const NkArchive &arbre) noexcept {
					lu = false;
					refus.Clear();
					doc = arbre;

					// ── LES COMPOSANTS SE DÉVELOPPENT ICI, ET NULLE PART AILLEURS ──
					//
					//  Un `component "Nom" { ... }` est un PATRON, pas une vue. Ses
					//  instances sont remplacées par ce qu'il contient AVANT que le
					//  monteur ne voie le document : le monteur ne connaît donc pas
					//  les composants, et il n'a pas à les connaître.
					//
					//  ⚠️ C'EST `doc` QU'ON DÉVELOPPE, PAS `arbre`. Le document de
					//     l'auteur garde ses composants : c'est lui qui se réécrit
					//     sur disque, et l'aller-retour du format doit rester vrai.
					//     Développer la source ferait grossir le fichier à chaque
					//     enregistrement, et le composant disparaîtrait au premier.
					//
					//  ⚠️ ET UN REFUS N'EMPÊCHE PAS LE MONTAGE. Un composant à deux
					//     racines, ou qui s'instancie lui-même, est retiré et COMPTÉ.
					//     Le document se monte alors incomplet — et `composants`
					//     dit où. Refuser tout le document punirait les neuf autres
					//     instances qui, elles, sont justes.
					NkGuiDevelopperComposants(doc, composants);

					NkGuiMonteur::Preparer(doc, etat);
					infos.Lire(doc);
					NkGuiExecution::etat = &etat;
					eval.rappel = &NkBandeDocument::SurCallback;
					eval.rappelUser = this;
					lu = true;
					return true;
				}

				/// UN appelant de `Adopter` : celui qui lit le texte sur le disque.
				/// Rend faux AVEC un refus nommé — jamais une bande vide.
				///
				/// ⚠️ `ReadAllBytes`, JAMAIS `ReadAllText`. Ce dépôt a payé : `WriteAllText`
				///    écrit en CRLF et `ReadAllText` renormalise — l'écart serait masqué des
				///    deux côtés à la fois. Un document est une suite d'octets.
				bool ChargerDepuisFichier(const char *ch) noexcept {
					lu = false;
					refus.Clear();
					chemin = NkString(ch);
					const NkVector<nk_uint8> octets = NkFile::ReadAllBytes(ch);
					if (octets.Size() == 0u) {
						refus = NkString("fichier introuvable ou vide");
						return false;
					}
					NkArchive arbre;
					NkGuiDiag err;
					if (!NkGuiArchive::Read((const char *)octets.Data(), (uint32)octets.Size(), arbre, err)) {
						// LE REFUS EST NOMMÉ, et il porte la ligne. Une fenêtre vide n'est
						// pas un échec acceptable : ce soir même, une démo a été déclarée
						// livrée parce qu'elle « s'ouvre et tient », et elle s'ouvrait vide.
						refus = NkPrefixeRefus(err);
						return false;
					}
					const NkString garde = chemin;
					const bool ok = Adopter(arbre);
					chemin = garde; // `Adopter` ne connaît aucun chemin : c'est le sujet
					return ok;
				}

				/// Monte la bande dans la région que la coquille vient de poser.
				void Monter(NkGuiContext &ctx) noexcept {
					if (!lu) {
						PeindreRefus(ctx);
						return;
					}
					rap = NkGuiMonteRapport();
					boutons = 0;
					zonesRemplies = 0;
					ReinitialiserCompteurs();
					Brancher(ctx);
					NkGuiMonteur::Monter(ctx, doc, etat, rap, this);
					// Les comportements APRÈS le montage — la limite (5) de
					// NkGuiInteraction.h : `n1.value` doit être la valeur que le widget
					// vient d'avoir, pas celle de l'image d'avant.
					ExecuterComportements(doc);
					Debrancher(ctx);
				}

				// ── crochet du monteur : la zone hôte, par NOM ────────────────
				bool RemplirHote(NkGuiContext &ctx, const char *nom, const NkRect &zone) noexcept override {
					for (uint32 i = 0; i < nbZones; ++i) {
						if (zones[i].nom && zones[i].fn && NkGMotEgal(NkStringView(nom), zones[i].nom)) {
							if (zones[i].fn(ctx, zone, zones[i].user)) {
								++zonesRemplies;
								return true;
							}
							return false; // il a dit non : le monteur peindra ses hachures
						}
					}
					return false;
				}

				// ── crochet du monteur : l'appui, dérivé (voir l'en-tête) ─────
				void Apres(NkGuiContext &ctx, const NkArchive &w, NkStringView role,
						   NkGuiMonteEtat::Entree *e) noexcept override {
					// L'exécution garde ses propres devoirs (anneau de focus, donnée
					// vivante, EndDisabled) : on l'appelle AVANT d'ajouter les nôtres.
					NkGuiExecution::Apres(ctx, w, role, e);
					// ⚠️ `MenuItem` EST DE LA MEME FAMILLE, ET CE N'EST PAS UNE SUPPOSITION :
					//    il passe par le MEME `ctx.ButtonBehavior(...)` que `Button`
					//    (`NkGuiWidgets.cpp:5303`), donc il pose `lastItemHovered` de la meme
					//    facon, donc la meme condition le detecte. Une entree de menu qui
					//    n'agirait pas serait un menu purement decoratif.
					if (!NkGMotEgal(role, "Button") && !NkGMotEgal(role, "RepeatButton")
						&& !NkGMotEgal(role, "MenuItem"))
						return;
					++boutons;
					// La condition de `ButtonBehavior` : relâchement, survolé, non grisé.
					if (!ctx.IsItemHovered() || !ctx.input.mouseReleased[0] || ctx.IsDisabled())
						return;
					Declencher(NkGuiArchive::IdOf(w));
				}

				/// Le nom de l'action est l'identifiant du bouton.
				void Declencher(NkStringView nom) noexcept {
					for (uint32 i = 0; i < nbActions; ++i) {
						if (actions[i].nom && actions[i].fn && NkGMotEgal(nom, actions[i].nom)) {
							actions[i].fn(actions[i].user);
							++actionsTirees;
							return;
						}
					}
					// PERSONNE NE SERT CE NOM. On le compte plutôt que de le taire :
					// même motif que `hotesNonRemplis`.
					++actionsInconnues;
				}

			private:
				/// `Callback "nom"(...)` émis par un `behavior` : même table que les boutons.
				static void SurCallback(const NkGuiAppelCallback &a, void *user) noexcept {
					auto *b = (NkBandeDocument *)user;
					if (b)
						b->Declencher(NkStringView(a.nom.Data(), (usize)a.nom.Size()));
				}

				static NkString NkPrefixeRefus(const NkGuiDiag &err) noexcept {
					// Le diagnostic du lecteur, tel quel : il porte déjà la ligne et la
					// colonne. Le reformuler ferait perdre ce qu'il sait.
					return NkString(err.message);
				}

				/// Un document qu'on n'a pas pu lire ne laisse pas une bande vide : il
				/// ÉCRIT pourquoi, là où il aurait dû se monter.
				void PeindreRefus(NkGuiContext &ctx) noexcept {
					if (!ctx.font || !ctx.font->Valid())
						return;
					const NkRect r = ctx.layout.region;
					NkString m = NkString("Document d'interface refusé : ");
					m += chemin;
					m += NkString("  —  ");
					m += refus.Size() ? refus : NkString("raison inconnue");
					ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
									 {r.x + 8.f, r.y + 4.f}, m.CStr(), NkColor{232, 96, 80, 255});
				}
		};

	} // namespace nkgui
} // namespace nkentseu

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
