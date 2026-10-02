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
#include "NKGui/Doc/NkGuiEvenements.h"  // QUAND un `behavior` se declenche
#include "NKGui/Doc/NkGuiInclusions.h"  // et les `include` se resolvent avant eux
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

		// =====================================================================
		//  UN COMPOSANT APPELÉ ET REMPLI EN C++ (2026-09-28)
		// =====================================================================
		//  Rodolf, 28/09 : « faire un composant qui serait appelé et rempli en
		//  C++ — notre nkgui doit le permettre ». Et, le même jour, la raison :
		//  « un document est constitué de plusieurs composants, plusieurs
		//  pages ».
		//
		//  🔴 CE QUE ÇA CORRIGE. Une zone `Host` est un TROU que l'application
		//     peint à la main : elle y rappelle `MenuItem`, `Button`… c'est-à-dire
		//     qu'elle REDESSINE, en C++, ce que le document sait déjà décrire. Les
		//     quatre listes engendrées de NKUIDesign (backends, thèmes, panneaux,
		//     langues) vivaient comme ça, et leur apparence n'était donc PAS dans
		//     le document.
		//
		//     Un patron instancié, lui, rend la forme au document : le `.nkgui`
		//     dit à quoi ressemble UNE entrée, le C++ dit COMBIEN il y en a et
		//     avec quelles valeurs. C'est le même partage que le MVC déjà posé —
		//     le document est la Vue, l'application le Contrôleur.
		//
		//  ⚠️ LE PATRON DOIT AVOIR UNE SEULE RACINE, et c'est refusé sinon. Une
		//     instance à deux racines n'a pas d'identifiant unique : le nom que
		//     l'hôte donne irait à l'une des deux, et l'action de l'autre ne
		//     serait servie par personne — un widget muet, exactement ce que la
		//     sonde de coquille existe pour interdire.
		//
		//  ⚠️ L'IDENTIFIANT DE L'INSTANCE EST CELUI QUE L'HÔTE CHOISIT, et c'est
		//     ce qui rend la chose utile : la racine du patron le PREND (les
		//     descendants sont préfixés). La table d'actions de l'application sert
		//     donc l'entrée sans rien savoir du patron.

		/// Une valeur posée sur l'instance. `texte` = la valeur est une CHAÎNE
		/// (l'archive la cite elle-même) ; sinon `lexeme` est écrit tel quel
		/// (`true`, `12`, `#ff0000`).
		///
		/// ⚠️ DEUX CHAMPS PLUTÔT QU'UN, PARCE QUE CITER OU NON N'EST PAS UN
		///    DÉTAIL : écrire `label = Vulkan` sans guillemets donnerait un jeton,
		///    et `checked = "true"` entre guillemets donnerait une chaîne que
		///    personne ne lit comme un booléen.
		struct NkGuiValeurPatron {
				const char *cle = nullptr;
				const char *texte = nullptr;  ///< non nul : valeur chaîne
				const char *lexeme = nullptr; ///< sinon : jeton brut

				static NkGuiValeurPatron Texte(const char *c, const char *v) noexcept {
					NkGuiValeurPatron p;
					p.cle = c;
					p.texte = v;
					return p;
				}
				static NkGuiValeurPatron Brut(const char *c, const char *lex) noexcept {
					NkGuiValeurPatron p;
					p.cle = c;
					p.lexeme = lex;
					return p;
				}
				static NkGuiValeurPatron Booleen(const char *c, bool v) noexcept {
					return Brut(c, v ? "true" : "false");
				}
		};

		/// Monte UNE instance d'un patron, au curseur courant.
		/// Rend faux si le patron est inconnu, sans racine unique, ou sans nom
		/// d'instance — et ne dessine alors RIEN plutôt qu'un widget à moitié
		/// nommé.
		///
		/// `activeOut` (facultatif) : vrai si CETTE instance vient d'être activée.
		///
		/// 🔴 IL EXISTE PARCE QU'UNE LISTE ENGENDRÉE N'A PAS DE NOM D'ACTION
		///    STABLE. La table d'actions d'une application est écrite à la main,
		///    une ligne par identifiant ; les instances d'un patron, elles,
		///    naissent à l'exécution — « un dorsal par API disponible ». Les
		///    router par la table ferait compter chaque clic dans
		///    `actionsInconnues`.
		///    L'hôte qui instancie sait déjà ce que sa ligne `i` désigne : on lui
		///    rend donc l'activation, exactement comme `nkgui::MenuItem` la rend.
		///
		/// ⚠️ ET DANS CE CAS ON NE PASSE PAS `hooks` : les crochets de la bande
		///    appelleraient `Declencher` sur un nom que personne ne sert. La
		///    contrepartie est écrite : un patron monté sans crochets n'exécute
		///    pas les `behavior` qu'il porterait. Un patron d'entrée de liste n'en
		///    a pas ; le jour où l'un en porte, il faudra une table d'actions
		///    dynamique, et ce sera un autre chantier.
		inline bool NkGuiMonterPatron(NkGuiContext &ctx, const NkGuiPatrons &patrons,
									  const char *nomPatron, const char *idInstance,
									  const NkGuiValeurPatron *valeurs, uint32 nValeurs,
									  NkGuiMonteEtat &etat, NkGuiMonteRapport &rap,
									  NkGuiMonteHooks *hooks = nullptr,
									  bool *activeOut = nullptr) noexcept {
			if (activeOut)
				*activeOut = false;
			if (!idInstance || !*idInstance)
				return false;
			const NkArchive *def = patrons.Trouver(nomPatron);
			if (!def)
				return false;
			// ⚠️ UNE COPIE PAR INSTANCE, ET C'EST ASSUMÉ. Le patron est un modèle
			//    partagé : le modifier en place ferait que la deuxième instance
			//    hériterait des valeurs de la première. Les listes visées comptent
			//    quelques unités (six dorsaux, douze panneaux) — si un jour l'une
			//    d'elles compte des milliers de lignes, c'est CETTE copie qu'il
			//    faudra mesurer avant de la remplacer.
			NkArchive inst = *def;
			NkArchiveNode *corps = detail::NkGCCorpsMut(inst);
			if (!corps || corps->array.Size() != 1u)
				return false; // un patron = UNE racine (cf. la note ci-dessus)
			NkArchiveNode &racineN = corps->array[0];
			if (!racineN.IsObject() || !racineN.object)
				return false;
			NkArchive &racine = *racineN.object;

			detail::NkGCRenommage ren;
			detail::NkGCPoserId(racine, NkString(idInstance));
			detail::NkGCPrefixerIds(racineN, NkString(idInstance), ren);

			for (uint32 i = 0; i < nValeurs; ++i) {
				const NkGuiValeurPatron &v = valeurs[i];
				if (!v.cle || !*v.cle)
					continue;
				// L'identité ne se surcharge pas — même règle que `NkGCSurcharger`.
				if (NkGuiArchive::IsReservedKey(NkStringView(v.cle)))
					continue;
				if (v.texte)
					racine.SetString(NkStringView(v.cle), NkStringView(v.texte));
				else if (v.lexeme)
					NkGuiArchive::SetToken(racine, NkStringView(v.cle),
										   NkStringView(v.lexeme));
			}
			NkGuiMonteur::MonterBloc(ctx, inst, etat, rap, hooks);
			// ⚠️ LA CONDITION EST CELLE DE LA BANDE, AU MOT PRES -- relachement,
			//    survole, non grise. Elle est ECRITE ICI parce que l'instance ne
			//    passe PAS par `Declencher` (son nom n'est dans aucune table), et
			//    deux conditions ecrites separement finissent par diverger : si
			//    celle de `NkBandeDocument::Apres` change, celle-ci doit suivre.
			//    Le patron ayant UNE seule racine, le « dernier widget soumis »
			//    est bien l'instance.
			if (activeOut)
				*activeOut = ctx.IsItemHovered() && ctx.input.mouseReleased[0] && !ctx.IsDisabled();
			return true;
		}

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

				/// LES PATRONS, gardes apres developpement (28/09). Ce sont eux que
				/// `NkGuiMonterPatron` instancie a l'execution : sans cette table, un
				/// `component` declare dans le document n'existe plus une fois le
				/// document developpe.
				NkGuiPatrons patrons;

				/// Idem pour les `include` : combien résolus, et chaque refus nommé
				/// (introuvable, illisible, cycle, sans provenance).
				NkGuiRapportInclusions inclusions;

				/// Ce qui s'est produit pendant l'image courante. Vidé à chaque
				/// montage : un événement est un fait daté.
				NkGuiJournalEvenements evenements;

				/// La valeur de chaque donnée liée à l'image PRÉCÉDENTE. C'est la
				/// seule source possible de `Changed`.
				NkGuiMemoireValeurs memoire;

				/// Combien de comportements ont été déclenchés, combien ignorés
				/// faute d'événement, et ce que le document a demandé sans qu'on
				/// sache encore le servir.
				NkGuiRapportEvenements evts;

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

					// ── LES INCLUSIONS D'ABORD, ET L'ORDRE N'EST PAS ARBITRAIRE ───
					//
					//  Un document inclus apporte ses `component`. S'ils arrivaient
					//  APRÈS le développement, leurs instances auraient déjà été
					//  comptées comme des rôles inconnus — et le document se
					//  monterait amputé sans qu'on sache pourquoi.
					//
					//  ⚠️ LE DOSSIER DE BASE VIENT DE `chemin`, ET IL PEUT ÊTRE VIDE.
					//     `Adopter` prend un ARBRE : appelé directement — par un
					//     générateur en mémoire, par un banc — il n'a aucune
					//     provenance, donc aucun « à côté de moi ». Les `include`
					//     sont alors COMPTÉS non résolus, jamais cherchés dans le
					//     dossier courant du processus : le résultat dépendrait d'où
					//     l'application a été lancée. *Un outil qui devine masque la
					//     faute au lieu de la montrer.*
					inclusions = NkGuiRapportInclusions();
					NkGuiResoudreInclusions(doc, detail::NkGIDossierDe(chemin.CStr()).CStr(),
											inclusions);

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
					patrons = NkGuiPatrons();
					NkGuiDevelopperComposants(doc, composants, &patrons);

					NkGuiMonteur::Preparer(doc, etat);
					infos.Lire(doc);
					// (02/10) L'HABILLAGE : les polices du document (chemins relatifs
					// a son dossier) et ses ambiances (NkGuiHabillage.h).
					dossierDocument = NkGuiDossierDe(chemin.CStr());
					NkGuiLirePolices(doc, dossierDocument.CStr(), polices);
					NkGuiLireAmbiances(doc, ambiances);
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
					MonterInterne(ctx, nullptr);
				}

				/// Monte UNE racine nommée du document, et rien d'autre.
				///
				/// ⚠️ ELLE EXISTE PARCE QU'UNE COQUILLE POSE SES BANDES A DES INSTANTS
				///    DIFFERENTS. Sans elle il fallait un FICHIER par bande : NkAntenne en
				///    avait 25 là où 5 suffisent, NkAnimaEditor 4, NKUIDesign 2. Le manque
				///    était nommé dans ces trois applications ; `NkGuiMonteur` porte
				///    désormais `Monter(ctx, doc, "racine", …)` et cette méthode n'est que
				///    le fil jusqu'à lui.
				///
				/// ⚠️ ET LE NOM NON TROUVÉ NE SE TAIT PAS : `rap.racinesIntrouvables` et
				///    `rap.derniereRacineIntrouvable`. Une bande vide sans message serait
				///    indiscernable d'une bande qui n'affiche rien, et une faute de frappe
				///    dans un nom de racine est la faute la plus probable de ce mécanisme.
				void Monter(NkGuiContext &ctx, const char *racine) noexcept {
					MonterInterne(ctx, racine);
				}

			private:
				/// ⚠️ UN SEUL CORPS POUR LES DEUX PORTES. Tout ce qui entoure le montage —
				///    les compteurs remis à zéro, le journal d'événements vidé, le
				///    branchement, les comportements exécutés APRÈS — doit être identique,
				///    sinon une bande à racine nommée ne se comporterait pas comme une
				///    bande entière. Deux copies auraient divergé au premier ajout.
				void MonterInterne(NkGuiContext &ctx, const char *racine) noexcept {
					if (!lu) {
						PeindreRefus(ctx);
						return;
					}
					rap = NkGuiMonteRapport();
					boutons = 0;
					zonesRemplies = 0;
					ReinitialiserCompteurs();
					// ⚠️ LE JOURNAL SE VIDE ICI, AU DÉBUT DE L'IMAGE. Un événement
					//    est un FAIT DATÉ : le garder d'une image sur l'autre
					//    rejouerait son comportement indéfiniment, c'est-à-dire
					//    exactement le défaut que cette couche supprime.
					evenements.Vider();
					evts = NkGuiRapportEvenements();
					Brancher(ctx);
					if (racine)
						(void)NkGuiMonteur::Monter(ctx, doc, racine, etat, rap, this);
					else
						NkGuiMonteur::Monter(ctx, doc, etat, rap, this);
					// `Changed` se DÉDUIT : NKGui dit ce qu'une valeur vaut, jamais
					// qu'elle vient de changer. On confronte donc l'état monté à
					// celui de l'image précédente, APRÈS le montage.
					evts.faits = evenements.Nombre();
					// Les comportements APRÈS le montage — la limite (5) de
					// NkGuiInteraction.h : `n1.value` doit être la valeur que le widget
					// vient d'avoir, pas celle de l'image d'avant.
					ExecuterComportementsFiltres(doc);
					Debrancher(ctx);
				}

			public:
				// ⚠️ LA PORTEE SE REFERME ICI, ET CE N'EST PAS DU RANGEMENT. `actions`,
				//    `nbActions`, `zones` et `nbZones` sont posés DE L'EXTERIEUR
				//    (`PoserTables`), et `Declencher` est appelé par les hôtes. Laisser
				//    courir le `private:` de `MonterInterne` jusqu'au bas de la classe
				//    aurait rendu tout cela inaccessible — une erreur de compilation
				//    bruyante chez NkAnimaEditor et Nogee, pour un ajout qui ne les
				//    concerne pas.

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
					// ── `Changed` : ICI, parce qu'ICI SEULEMENT les deux noms se
					//    rencontrent ───────────────────────────────────────────
					//  Un `behavior "essai"` parle du WIDGET `essai`. Mais sa valeur
					//  vit dans l'état sous la clé de LIAISON — `essai.valeur`. Les
					//  deux noms ne sont ensemble qu'ici : `w` porte l'identifiant du
					//  widget, `e` porte sa clé et sa valeur.
					//
					//  ⚠️ PREMIÈRE VERSION, LE 26/09 : la comparaison se faisait après
					//     le montage, en parcourant l'état. Elle notait donc le
					//     changement sous `essai.valeur`, et le `behavior "essai"` ne
					//     se reconnaissait jamais dedans. Mesuré : image 2,
					//     `declenches=1` au lieu de 2 — le comportement était ignoré
					//     en silence, et seul le témoin sans événement partait.
					//
					//  ⚠️ ET ON NE « RÉPARE » PAS EN COUPANT LE SUFFIXE. Une liaison
					//     n'est pas tenue de porter le nom de son widget : `bind =
					//     joueur.vitesse` sur une case `vitesseX` est légitime.
					//     Deviner le lien par la graphie aurait marché sur l'exemple
					//     et menti sur le cas suivant.
					if (e && memoire.Confronter(NkStringView(e->cle.CStr()), e->b, e->f, e->texte))
						evenements.Noter(NkGuiEvenement::Changed, NkGuiArchive::IdOf(w));

					// ── `Hover` : le pointeur est sur CE widget ────────────────
					//  ⚠️ NOTÉ POUR TOUT RÔLE, pas seulement les boutons. Une `Tile`
					//     qui lance sa vignette au survol n'est pas un bouton, et une
					//     ligne de liste qui se met en avant non plus.
					if (ctx.IsItemHovered())
						evenements.Noter(NkGuiEvenement::Hover, NkGuiArchive::IdOf(w));

					if (!NkGMotEgal(role, "Button") && !NkGMotEgal(role, "RepeatButton")
						&& !NkGMotEgal(role, "MenuItem"))
						return;
					++boutons;
					// La condition de `ButtonBehavior` : relâchement, survolé, non grisé.
					if (!ctx.IsItemHovered() || !ctx.input.mouseReleased[0] || ctx.IsDisabled())
						return;
					// ── `Click` : NOTÉ LÀ OÙ L'ACTION SE TIRE, PAS À CÔTÉ ─────
					//  Une seconde condition écrite ailleurs aurait fini par diverger
					//  de celle-ci -- *deux compteurs sans code commun*. Le clic qui
					//  déclenche un `behavior` est EXACTEMENT celui qui tire l'action.
					evenements.Noter(NkGuiEvenement::Click, NkGuiArchive::IdOf(w));
					Declencher(NkGuiArchive::IdOf(w));
				}

				// ═════════════════════════════════════════════════════════════
				//  `Changed` — il se DÉDUIT, il ne s'observe pas
				// ═════════════════════════════════════════════════════════════
				//  NKGui ne dit jamais « cette valeur vient de changer » : elle dit
				//  ce qu'elle vaut. On confronte donc l'état monté à celui de
				//  l'image précédente, après le montage.
				//
				//  ⚠️ LA PREMIÈRE IMAGE NE DÉCLENCHE RIEN. Une valeur vue pour la
				//     première fois n'a pas changé : elle est APPARUE. Traiter
				//     l'apparition comme un changement ferait partir tous les
				//     comportements au premier tour — c'est-à-dire le comportement
				//     d'avant, en pire, puisqu'on croirait avoir un événement.
				void NoterLesChangements() noexcept {
					const uint32 n = etat.Taille();
					for (uint32 i = 0; i < n; ++i) {
						const NkGuiMonteEtat::Entree *e = etat.A(i);
						if (!e)
							continue;
						if (memoire.Confronter(NkStringView(e->cle.CStr()), e->b, e->f, e->texte))
							evenements.Noter(NkGuiEvenement::Changed, NkStringView(e->cle.CStr()));
					}
					evts.faits = evenements.Nombre();
				}

				// ═════════════════════════════════════════════════════════════
				//  QUAND un `behavior` se déclenche — la limite (5), levée
				// ═════════════════════════════════════════════════════════════
				//      behavior "boucle"           { ... }   chaque image, comme avant
				//      behavior "boucle"(Changed)  { ... }   seulement au changement
				//      behavior "sauver"(Click)    { ... }   seulement au clic
				//
				//  ⚠️ STRICTEMENT ADDITIF, ET C'EST LA CONDITION DU LOT. Sans
				//     parenthèse, le comportement passe à CHAQUE IMAGE, exactement
				//     comme avant. Les documents du corpus et les trois de
				//     NkAnimaEditor tournent sans une ligne de changement.
				//
				//  ⚠️ ET LE NOM QUI DÉCLENCHE EST CELUI DU `behavior`. C'est déjà la
				//     convention du format — un `behavior "boucle"` parle de la case
				//     `boucle`. En inventer une seconde (un attribut `cible`) aurait
				//     créé deux façons de nommer la même chose, et elles auraient
				//     divergé.
				void ExecuterComportementsFiltres(const NkArchive &d) noexcept {
					// ⚠️ LA MÊME REMISE À ZÉRO QUE `ExecuterComportements`, ET PAR LA
					//    MÊME PORTE. L'évaluateur garde ses variables d'une exécution
					//    à l'autre ; ne pas le réinitialiser ferait fuir `set x = ...`
					//    d'une image sur la suivante. On filtre QUI s'exécute, jamais
					//    COMMENT.
					eval.Reinitialiser();
					const NkArchiveNode *corps = NkGMonteCorps(d);
					if (!corps)
						return;
					for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
						if (!corps->array[i].IsObject() || !corps->array[i].object)
							continue;
						const NkArchive &sec = *corps->array[i].object;
						if (!NkGMotEgal(NkGuiArchive::TypeOf(sec), "behavior"))
							continue;

						// ⚠️ L'ÉVÉNEMENT EST UNE PROPRIÉTÉ, ET C'EST UNE MESURE, PAS UN
						//    GOÛT. La première écriture essayée était la parenthèse
						//    d'état — `behavior "boucle"(Changed) {` — parce que
						//    l'archive la lit déjà pour `appearance(Hover)`. Mesure du
						//    26/09, les deux formes côte à côte dans un même document :
						//    la forme SANS identifiant se lit, celle qui combine un
						//    identifiant CITÉ et une parenthèse **n'est pas reconnue
						//    comme un bloc** — elle tombe en tranche brute, exactement
						//    comme `behavior "x" graph {`. Le comportement disparaissait
						//    donc en silence, ce qui est le pire des trois échecs.
						//
						//    `on = Changed` emploie la grammaire des propriétés, que le
						//    lecteur porte depuis toujours. Et une propriété n'est PAS
						//    dans le corps exécutable (`$body`) : l'évaluateur ne la
						//    verra jamais comme une instruction.
						const NkString motEvt = NkGTexte(sec, "on", "");
						const NkGuiEvenement ev =
							NkGuiEvenementDepuisNom(NkStringView(motEvt.CStr()));

						// ⚠️ UN MOT INCONNU NE SE DEVINE PAS, ET NE PASSE PAS. Le
						//    faire tomber sur « chaque image » ferait d'une faute de
						//    frappe un comportement qui tourne en boucle -- vert à
						//    l'œil, faux en fond. On refuse, et on compte.
						if (ev == NkGuiEvenement::Inconnu) {
							++evts.evenementsInconnus;
							continue;
						}

						// Pas de parenthèse : le comportement d'avant, intact.
						if (ev == NkGuiEvenement::Aucun) {
							eval.ExecuterSection(sec);
							++evts.comportementsDeclenches;
							continue;
						}

						// ⚠️ RECONNU MAIS SANS SOURCE : on ne l'exécute pas, et on le
						//    COMPTE séparément d'un mot inconnu. L'auteur a écrit une
						//    intention juste que la machine ne sait pas encore
						//    servir -- ce n'est pas la même chose qu'une erreur.
						if (!NkGuiEvenementADesSources(ev)) {
							++evts.evenementsSansSource;
							continue;
						}

						if (evenements.ADeclenche(ev, NkGuiArchive::IdOf(sec))) {
							eval.ExecuterSection(sec);
							++evts.comportementsDeclenches;
						} else {
							++evts.comportementsIgnores;
						}
					}
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
