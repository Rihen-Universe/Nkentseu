#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiEcouteurs.h
// @Brief   La table des ECOUTEURS : un widget qui recoit un evenement l'expose au
//          C++, route PAR IDENTIFIANT, avec une charge NOMMEE.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  D'OU CA VIENT
// =============================================================================
//  Rodolf, 27/09, sur la toile infinie : « ce panneau doit recevoir les
//  evenements clavier et souris [...] et s'il les recoit le code C++ doit les
//  ecouter et quand il ecoute il traite en consequence. » Puis, generalisant :
//  « donc chaque widget panel et autre expose ses evenements lorsqu'il les
//  recoit [...] vu qu'on pourrait avoir plusieurs viewports dans une application
//  ca sera plus facile ainsi. »
//
//  C'est plus juste que ce que je proposais. Ma version donnait au format un
//  crochet de PICKING -- « ou ai-je clique, quel noeud est-ce ? ». Inutile :
//  l'application connait DEJA sa geometrie, elle n'a besoin que des evenements.
//  Et le routage par identifiant fait que trois viewports dans une fenetre sont
//  trois identifiants, sans rien de particulier a prevoir.
//
// =============================================================================
//  🔴 CE FICHIER N'INVENTE AUCUN VOCABULAIRE, ET C'EST LE POINT
// =============================================================================
//  `NkGuiEvenements.h` (26/09) porte DEJA l'enumeration `NkGuiEvenement` : les
//  douze noms qu'un document peut ecrire dans `behavior "x" { on = Click }`. Et
//  il dit lui-meme lesquels ont une source :
//
//      NkGuiEvenementADesSources()  ->  Click, Changed, Hover.  Trois sur douze.
//
//  Les neuf autres -- DoubleClick, ContextMenu, Focus, Blur, KeyDown, Wheel,
//  Submit, DragStart, Drop -- y sont « reconnus, sans source » : le document peut
//  les nommer, ils sont comptes, ils ne se declenchent jamais.
//
//  Ce fichier n'est donc PAS un second systeme d'evenements : c'est **la source
//  qui manque a ces neuf-la**, et la porte par laquelle le C++ les recoit. J'ai
//  failli ecrire une enumeration `NkGuiEvtType` en parallele -- deux vocabulaires
//  pour le meme concept, exactement « une porte, pas neuf ». La lecture du
//  fichier voisin l'a evite.
//
//  📌 CONSEQUENCE A TENIR : chaque fois qu'une source s'ajoute ici,
//     `NkGuiEvenementADesSources()` doit l'admettre. C'est le garde-fou
//     « declarer n'est pas livrer » de ce couple de fichiers ; le laisser mentir
//     ferait croire livre ce qui n'est que reconnu.
//
// =============================================================================
//  POURQUOI PAS LA TABLE DE RAPPELS (`NkGuiRappels`)
// =============================================================================
//  Elle existe, elle marche, elle a la bonne forme. Trois raisons de ne pas s'en
//  servir, et la troisieme decide.
//
//   1. LE SENS DE CIRCULATION EST INVERSE. Un `Callback` part du DOCUMENT vers
//      l'application. Un evenement arrive de l'ENTREE et doit trouver le widget
//      sous le pointeur, ou celui qui a le focus.
//
//   2. LA CHARGE EST STRUCTUREE, PAS VARIADIQUE. `NkGuiAppelCallback` porte
//      `NkGuiValeur args[4]`. Un evenement porte une position en deux reperes,
//      trois boutons, trois modificateurs, une touche, deux molettes.
//
//   3. 🔴 ET C'EST CE QUI TRANCHE : une application qui lirait la coordonnee Y
//      dans `args[2]` serait « un indice n'est pas un nom ». Le jour ou un champ
//      s'ajoute au milieu, tous les consommateurs deviennent faux SANS QUE RIEN
//      NE CRIE -- la faute de position payee cinq fois le 20/09, et encore le
//      31/08 sur `NkEditorRailItem`. Une structure nommee ne se decale pas.
//
//  La FORME, elle, est copiee de `NkGuiRappels` (origine : 27/09) : table
//  `nom -> fonction`, plafond, et les compteurs branches/remplaces/refuses/
//  servis/sansDestinataire. « Trois tables posees par l'hote : une quatrieme qui
//  s'y ajoute ne demande rien de nouveau a apprendre. »
//
// =============================================================================
//  ⚠️ `NkGuiEvtDetail` EST EN AJOUT SEUL
// =============================================================================
//  Comme `NkEditorRailItem`, et pour la raison qui y est ecrite noir sur blanc :
//  elle finira initialisee par position quelque part. Un champ glisse AU MILIEU
//  decale tout ce qui suit, sans erreur de compilation quand les types
//  s'accordent. On ajoute a la fin, toujours.
// -----------------------------------------------------------------------------

#ifndef __NKENTSEU_NKGUI_DOC_NKGUIECOUTEURS_H__
#define __NKENTSEU_NKGUI_DOC_NKGUIECOUTEURS_H__

#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKGui/Core/NkGuiInput.h" // NkGuiKey
#include "NKGui/Core/NkGuiTypes.h" // NkVec2, NkRect
#include "NKGui/Doc/NkGuiEvenements.h" // LE vocabulaire : NkGuiEvenement

namespace nkentseu {
	namespace nkgui {

		/// La charge d'un evenement remis a un ecouteur.
		///
		/// ⚠️ DEUX POSITIONS, ET CE N'EST PAS UNE COMMODITE. `ecran` est en pixels de
		///    fenetre ; `local` est relatif au coin de la zone. Un ecouteur qui n'a que
		///    l'ecran doit soustraire le rectangle lui-meme -- donc chaque ecouteur
		///    refait le meme calcul, et celui qui le fait mal se trompe en silence. Le
		///    monteur connait le rectangle : il soustrait UNE FOIS.
		///
		/// ⚠️ IL N'Y A PAS DE COORDONNEE MONDE, ET C'EST DELIBERE. Le monde appartient a
		///    l'ECOUTEUR : sa transformation est a lui (pan, zoom -- et pour un viewport
		///    3D une camera qui n'a rien d'affine). Le format ne peut pas la connaitre
		///    sans se mettre a decrire des scenes, ce qu'il refuse de faire. `local` est
		///    la frontiere juste : le dernier repere que le format possede vraiment.
		struct NkGuiEvtDetail {
				NkGuiEvenement type = NkGuiEvenement::Aucun; ///< LE vocabulaire du 26/09
				NkString zone; ///< l'identifiant du widget qui expose -- la cle de routage
				NkVec2 ecran{0.f, 0.f};
				NkVec2 local{0.f, 0.f};
				NkRect rect{0.f, 0.f, 0.f, 0.f}; ///< la zone, telle que le montage l'a posee
				/// Bouge : le deplacement depuis l'image d'avant.
				/// Wheel : (wheelH, wheel) -- l'horizontale en x, la verticale en y.
				NkVec2 delta{0.f, 0.f};
				int32 bouton = -1;					///< 0=gauche 1=droit 2=milieu ; -1 = aucun
				NkGuiKey touche = NkGuiKey::Count;	///< `Count` = aucune
				uint32 codePoint = 0;				///< saisie de texte, pas une touche
				bool ctrl = false;
				bool maj = false;
				bool alt = false;
				bool survole = false; ///< le pointeur est dans la zone a cet instant
				bool focus = false;	  ///< la zone a le focus clavier

				// ═══════════════════════════════════════════════════════════
				//  AJOUTE A LA FIN — Rodolf, 27/09 : « le systeme d'evenements
				//  doit etre complet et toucher meme les custom event gamepad
				//  bref tout. »
				// ═══════════════════════════════════════════════════════════

				/// De quel appareil vient le pointeur.
				///
				/// 🔴 C'EST CE CHAMP QUI EVITE DE TRIPLER LE VOCABULAIRE. Souris,
				///    doigt et stylet emettent les MEMES `PointerDown/Move/Up` ; ce
				///    qui les distingue se lit ICI, pas dans le nom de l'evenement.
				///    Sans ca, un document devrait ecrire deux comportements pour un
				///    seul geste -- donc en oublier un sur mobile, ou personne ne
				///    teste. `NkPointerEvent.h` existe pour cette raison.
				enum class Appareil : uint8 { Inconnu = 0, Souris, Tactile, Stylet };
				Appareil pointeur = Appareil::Souris;
				/// L'identifiant du contact. ⚠️ INDISPENSABLE AU MULTI-TOUCHE : deux
				/// doigts emettent deux suites d'evenements entrelacees, et sans cet
				/// identifiant elles sont indiscernables -- un pincement ressemblerait
				/// a un doigt qui teleporte.
				int32 pointeurId = 0;
				/// Pression du stylet, 0..1. Vaut 1 pour une souris (appui franc) et
				/// 0 quand l'appareil n'en rend pas -- jamais « indefini », qui
				/// obligerait chaque lecteur a inventer sa valeur de repli.
				float32 pression = 1.f;

				/// La manette : son indice (0..n), son bouton, son axe et sa valeur.
				/// `manette` vaut -1 hors des evenements de manette.
				///
				/// ⚠️ LE BOUTON ET L'AXE RESTENT DES ENTIERS ICI, PAS LES ENUMERATIONS
				///    DE NKEvent (`NkGamepadButton`, `NkGamepadAxis`). NKGui ne depend
				///    pas de NKEvent, et l'y faire dependre pour deux enumerations
				///    aurait elargi la coupe du noyau -- la meme lecon que NKImage a
				///    donnee ce soir. L'hote convertit, et c'est une ligne chez lui.
				int32 manette = -1;
				int32 boutonManette = -1;
				int32 axeManette = -1;
				float32 valeurAxe = 0.f;

				/// Le nom d'un `Custom`.
				///
				/// 🔴 LE FORMAT ROUTE LE FAIT ET LE NOM, PAS LA CHARGE. Ce qu'un
				///    evenement custom transporte appartient a qui l'a defini :
				///    pretendre le typer ici aurait donne un tableau de valeurs
				///    positionnelles, c'est-a-dire « un indice n'est pas un nom » --
				///    la faute exacte que cette structure existe pour eviter.
				///    L'application qui emet et celle qui ecoute partagent deja leur
				///    propre canal ; le format leur dit QUAND et LEQUEL.
				NkString customNom;

				/// `Resize` : la nouvelle taille. `Moved` : la nouvelle position.
				NkVec2 taille{0.f, 0.f};
		};

		/// Ce qu'un ecouteur rend : vrai = « je l'ai traite, ne le passe pas plus loin ».
		///
		/// ⚠️ LA VALEUR DE RETOUR N'EST PAS DECORATIVE. Sans elle, une toile qui prend
		///    la molette pour zoomer ne pourrait pas empecher le panneau qui la contient
		///    de defiler en meme temps -- c'est le defaut « la toile defile A TRAVERS le
		///    menu » que la molette reservee a deja du corriger une famille plus haut
		///    (`NkGuiInput::ReserverMolette`, 05/09).
		using NkGuiEcouteurFn = bool (*)(const NkGuiEvtDetail &, void *);

		/// La table `identifiant -> ecouteur`.
		class NkGuiEcouteurs {
			public:
				static constexpr uint32 kMax = 64;

				/// Branche un ecouteur. Remplace s'il y en avait un ; REFUSE au-dela du
				/// plafond -- et le refus se COMPTE, il ne se tait pas : une table pleine
				/// qui laisse tomber en silence fait chercher le defaut dans le document,
				/// c'est-a-dire au mauvais endroit.
				bool Brancher(const char *nom, NkGuiEcouteurFn fn, void *user) noexcept {
					if (!nom || !*nom || !fn) {
						++refuses;
						return false;
					}
					for (uint32 i = 0; i < mNb; ++i) {
						if (mTable[i].nom.Compare(nom) == 0) {
							mTable[i].fn = fn;
							mTable[i].user = user;
							++remplaces;
							return true;
						}
					}
					if (mNb >= kMax) {
						++refuses;
						dernierRefuse = NkString(nom);
						return false;
					}
					mTable[mNb].nom = NkString(nom);
					mTable[mNb].fn = fn;
					mTable[mNb].user = user;
					++mNb;
					++branches;
					return true;
				}

				/// Remet l'evenement a son destinataire. Rend ce que l'ecouteur a rendu,
				/// et FAUX quand il n'y en a pas -- en retenant le nom demande.
				bool Servir(const NkGuiEvtDetail &e) noexcept {
					for (uint32 i = 0; i < mNb; ++i) {
						if (mTable[i].nom.Compare(e.zone) != 0)
							continue;
						++servis;
						const bool pris = mTable[i].fn(e, mTable[i].user);
						if (pris)
							++traites;
						return pris;
					}
					// 🔴 LE NOM, PAS SEULEMENT LE COMPTE. « un evenement sans
					//    destinataire » ne dit pas LEQUEL ; avec le nom, ca devient
					//    « tu as ecrit toileDesing ». C'est la lecon de
					//    `racinesIntrouvables`, du meme mois, et elle a deja fait
					//    gagner une demi-heure.
					++sansDestinataire;
					dernierSansDestinataire = e.zone;
					return false;
				}

				bool ADesEcouteurs() const noexcept {
					return mNb > 0u;
				}

				void Vider() noexcept {
					for (uint32 i = 0; i < mNb; ++i) {
						mTable[i] = Entree();
					}
					mNb = 0;
				}

				void ReinitialiserCompteurs() noexcept {
					servis = traites = sansDestinataire = 0;
					dernierSansDestinataire = NkString();
				}

				// ── LE RELEVE ────────────────────────────────────────────────
				uint32 branches = 0;
				uint32 remplaces = 0;
				uint32 refuses = 0;
				uint32 servis = 0;
				uint32 traites = 0; ///< servis dont l'ecouteur a rendu vrai
				uint32 sansDestinataire = 0;
				NkString dernierSansDestinataire;
				NkString dernierRefuse;

			private:
				struct Entree {
						NkString nom;
						NkGuiEcouteurFn fn = nullptr;
						void *user = nullptr;
				};
				Entree mTable[kMax];
				uint32 mNb = 0;
		};

		/// La table POSEE par l'hote -- comme les actions, les zones, les jetons et les
		/// icones. Nulle tant que personne n'en pose, et alors le monteur compte ses
		/// evenements au lieu de les perdre.
		NkGuiEcouteurs *NkGuiEcouteursPoses() noexcept;
		void NkGuiPoserEcouteurs(NkGuiEcouteurs *table) noexcept;

	} // namespace nkgui
} // namespace nkentseu

#endif // __NKENTSEU_NKGUI_DOC_NKGUIECOUTEURS_H__

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
