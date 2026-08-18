#pragma once
// -----------------------------------------------------------------------------
// @File    Panels.h
// @Brief   Les panneaux de NkUIDesign : palette, composition, apercu, proprietes, IA.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUI EST DEDANS, ET CE QUI N'Y EST PAS
// =============================================================================
//  Etat au 2026-08-18. La tranche verticale du 18/08 (un composant, ses
//  reglages, sa sauvegarde) est devenue une APPLICATION :
//
//    - **palette**     : les composants DECLARES, lus dans le registre, poses
//                        dans le document ;
//    - **composition** : l'arbre — un composant DANS un autre, meme mecanisme
//                        aux deux echelles ;
//    - **agencement**  : taille et disposition a la souris, qui ecrivent des
//                        PROPRIETES et jamais une coordonnee ;
//    - **document**    : une interface complete se charge et se sauve, avec sa
//                        PROVENANCE posee des la creation ;
//    - **IA**          : la place — un prompt, un backend remplacable, et une
//                        sortie qui passe par la meme porte que la main.
//
//  ⚠️ HORS TRANCHE, nomme, differe, volontairement absent :
//    - les **blueprints** : les evenements sont declares avec leur charge, rien
//      ne s'y branche encore ;
//    - la **creation de composants ex nihilo** : on compose ce qui est declare ;
//    - le **dessin d'icones** ;
//    - l'**ancrage** au sens complet (marges par bord) — cf. `Document.h` ;
//    - le **modele specialise** — cf. `DesignAI.h`, qui livre sa place, pas lui.
//
// REGLE DE LECTURE POUR CE FICHIER : **aucun panneau ne connait le nom d'un
//   composant.** Palette, arbre, reglages et catalogue d'IA bouclent tous sur le
//   registre ou sur les tables de la declaration. Le seul endroit du programme ou
//   un nom de composant est ecrit en clair est `Renderers.h`, parce qu'il faut
//   bien appeler une fonction — et ce fichier-la dit pourquoi, et ce qu'il
//   faudrait pour s'en passer.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkGuiComponentPaint.h"
#include "NKEditorKit/NkEditorKit.h"
#include "NKEditorKit/NkTheme.h"
#include "NKFileSystem/NkFile.h"

#include "DesignAI.h"
#include "Renderers.h"

#include <cstdio>

namespace nkuidesign {

	using namespace nkentseu::editorkit;
	using namespace nkentseu::nkgui;

	static const char *kDocumentPath = "nkuidesign_document.nkuidoc";

	// ═══════════════════════════════════════════════════════════════════════════
	//  LE VOCABULAIRE DE MISE EN PAGE — ce qui manquait pour que ca RESSEMBLE a
	//  une interface
	// ═══════════════════════════════════════════════════════════════════════════
	//  ⚠️ NE PAS CONFONDRE AVEC UN COMPOSANT. Ce qui suit n'est ni declare, ni
	//     enregistrable, ni editable : ce sont des raccourcis d'AGENCEMENT pour
	//     les panneaux de l'editeur (le « mobilier »), bases sur des conteneurs
	//     que NKGui fournit deja. Aucun widget n'est reimplemente ici -- la regle
	//     du kit tient : on ASSEMBLE, on ne recree pas.
	//
	//  ⚠️ POURQUOI IL EXISTE, ET C'EST UNE DETTE ASSUMEE PUBLIQUEMENT. Rodolf,
	//     devant l'ecran du 18/08 : « la partie gauche et droite de cette
	//     interface est voulue, ou on est juste en train de faire des tests ? »
	//     La reponse honnete etait : **c'etait le mecanisme rendu visible**. Une
	//     pile de boutons pleine largeur et une liste de valeurs cliquables
	//     prouvent qu'une declaration se lit, se modifie et se sauve ; elles ne
	//     ressemblent pas a une interface. La regle qui en est sortie dit les deux
	//     devoirs : **le dire sans attendre la question**, et **ne pas s'y
	//     arreter**.
	//
	//  ⚠️ LE SYMPTOME QUI TRAHIT UN ECRAN NON DESSINE : les libelles tronques.
	//     « Rentrer dans le voisin du dessu… », « Metriques du document (px
	//     logiqu… », « max (<= mir… ». Aucune de ces coupes n'est un bug de
	//     rendu : c'est **l'absence de decision de mise en page**. Un bouton pleine
	//     largeur dans une colonne etroite coupe son texte ; un bouton pose dans un
	//     FLOT prend la largeur de son texte et passe a la ligne quand il n'y en a
	//     plus. La correction n'est donc pas une ellipse -- c'est un conteneur.
	namespace designkit {

		/// Une rangee de boutons qui **prennent la largeur de leur texte** et
		/// reviennent a la ligne toutes seules. C'est le remede exact aux libelles
		/// tronques : `BeginFlow` mesure chaque item, la colonne ne les ecrase plus.
		struct Flow {
				explicit Flow(NkGuiContext &c) : ctx(c) {
					nkgui::BeginFlow(ctx);
				}
				~Flow() {
					nkgui::EndFlow(ctx);
				}
				NkGuiContext &ctx;
		};

		/// Un choix parmi N, **sur une seule ligne**, cellules de largeur egale.
		/// Remplace la pile verticale de `Selectable` qui donnait la « liste de
		/// valeurs cliquables » : cinq modes de taille empiles prenaient cinq
		/// lignes et se lisaient comme un menu, alors que c'est UN reglage.
		///
		/// ⚠️ REND L'INDICE CHOISI, ou -1. Il ne modifie rien lui-meme : l'appelant
		///    ecrit, et c'est lui qui sait s'il doit marquer une edition humaine.
		inline int32 Segmented(NkGuiContext &ctx, const char *const *labels, int32 count,
							   int32 current) {
			if (count <= 0)
				return -1;
			const int32 n = count < 12 ? count : 12;

			// ⚠️ LES CELLULES SONT PROPORTIONNELLES AU TEXTE, PAS EGALES -- et
			//    c'est une correction MESUREE, pas une preference. La premiere
			//    version donnait a chacune la meme part : sur un panneau etroit,
			//    « expand » (le plus long des cinq modes) s'affichait « expanc ».
			//    **Le controle segmente venait de recreer la troncature qu'il
			//    devait supprimer.** Cinq parts egales pour cinq mots de longueurs
			//    differentes, c'est la meme faute que le bouton pleine largeur :
			//    une largeur decidee sans regarder le contenu.
			//
			//    On mesure donc avec le MEME moteur que le rendu
			//    (`NkGuiFont::MeasureWidth` -> `CalcTextSizeX`) : mesurer avec une
			//    approximation maison — « tant de pixels par caractere » — aurait
			//    fabrique une seconde verite, fausse des la premiere police
			//    proportionnelle.
			//
			//    Les poids restent NEGATIFS (parts de l'espace restant) et non des
			//    pixels fixes : en pixels, un panneau plus etroit que la somme des
			//    libelles deborderait au lieu de serrer.
			float32 poids[12];
			float32 total = 0.f;
			for (int32 i = 0; i < n; ++i) {
				const float32 w = ctx.font ? ctx.font->MeasureWidth(labels[i]) : 0.f;
				// Le « + 24 » est la marge interne que `Selectable` ajoute autour
				// de son libelle : sans elle, la cellule vaudrait exactement le
				// texte et le rognerait de la valeur du padding.
				poids[i] = -(w + 24.f);
				total += w + 24.f;
			}

			// ⚠️ ET SI LES LIBELLES NE TIENNENT PAS SUR UNE LIGNE ? MESURE, PUIS
			//    CHANGEMENT DE CONTENEUR. Les poids proportionnels partagent
			//    l'espace DISPONIBLE : quand le total necessaire le depasse, ils
			//    retrecissent tout **en meme proportion** — et six noms d'API dans
			//    une colonne etroite se sont affiches « autoopenglvulkandx11dx12softw ».
			//    C'est la TROISIEME fois que la troncature revient sous une forme
			//    nouvelle, et a chaque fois la cause est la meme : **une largeur
			//    decidee sans regarder si le contenu y tient**.
			//
			//    Le remede n'est pas un libelle plus court — les noms d'API sont un
			//    vocabulaire partage avec le fichier et la ligne de commande, les
			//    abreger ici enseignerait un mot que le fichier ne comprend pas.
			//    C'est le CONTENEUR qui change : au-dela de la place disponible, on
			//    passe en FLOT, qui met a la ligne. La forme « une ligne, N cases »
			//    est preferable — un choix exclusif se lit d'un coup d'oeil — mais
			//    **elle n'est pas preferable au point d'etre illisible**.
			const float32 dispo = ctx.layout.region.w;
			int32 chosen = -1;
			if (dispo > 0.f && total > dispo) {
				nkgui::BeginFlow(ctx);
				for (int32 i = 0; i < n; ++i)
					if (nkgui::Selectable(ctx, labels[i], i == current))
						chosen = i;
				nkgui::EndFlow(ctx);
				return chosen;
			}
			nkgui::BeginRow(ctx, 0.f, poids, n);
			for (int32 i = 0; i < n; ++i)
				if (nkgui::Selectable(ctx, labels[i], i == current))
					chosen = i;
			nkgui::EndRow(ctx);
			return chosen;
		}

		/// Un titre de section repliable. Un seul point de passage : le jour ou la
		/// charte donne une forme aux titres, elle se pose ICI et nulle part
		/// ailleurs.
		inline bool Section(NkGuiContext &ctx, const char *title) {
			return nkgui::CollapsingHeader(ctx, title);
		}

		/// Une ligne « cle : valeur » en DEUX COLONNES alignees, au lieu d'une
		/// phrase qui se fait couper. La cle est bornee, la valeur prend le reste.
		inline void KeyValue(NkGuiContext &ctx, const char *key, const char *value) {
			static const float32 kPoids[2] = {-2.f, -3.f};
			nkgui::BeginRow(ctx, 0.f, kPoids, 2);
			nkgui::Text(ctx, key);
			nkgui::Text(ctx, value ? value : "");
			nkgui::EndRow(ctx);
		}

	} // namespace designkit

	// ═══════════════════════════════════════════════════════════════════════════
	//  L'ETAT PARTAGE
	// ═══════════════════════════════════════════════════════════════════════════
	// ⚠️ IL NE CONTIENT AUCUNE VALEUR DE REGLAGE NI AUCUN RECTANGLE. Les reglages
	//    vivent dans l'instance de chaque noeud ; les rectangles se recalculent a
	//    chaque image et se jettent. C'est ce qui garantit que ce que l'apercu
	//    dessine est EXACTEMENT ce que la sauvegarde ecrit — deux copies auraient
	//    diverge des la premiere seance.
	struct DesignState {
			NkUIDocument doc;
			NkLayoutResult layout;
			NkDocumentHost host;
			NkTheme theme;

			NkDesignAI ai;
			NkFileBackend fileBackend;

			int32 selected = 0;		 ///< index de noeud ; 0 = la racine
			int32 paletteChoice = 0; ///< 0 = cadre, puis 1+ = index dans le registre
			NkString status;
			/// Ce que l'audit des roles a trouve au demarrage, en une ligne. Lu par
			/// `main` pour le journal : le journal du lancement doit porter l'etat
			/// des roles, sinon il faut ouvrir la fenetre pour l'apprendre -- et
			/// c'est precisement ce qui a coute la seance du 18/08.
			NkString roleAudit;

			// ── CE QUE `main` A RESOLU, RECOPIE ICI POUR L'AFFICHAGE ─────────
			// ⚠️ RECOPIE, ET NON RECALCULEE. Le panneau pourrait rappeler
			//    `NkGfxResolve` — et il afficherait alors une SECONDE verite, qui
			//    divergerait le jour ou l'un des deux appels changerait d'argument.
			//    C'est exactement le defaut qui a laisse vivre le magenta. Une
			//    seule resolution, au lancement ; l'interface en montre le
			//    resultat.
			NkString gfxEffective = NkString("?");
			NkString gfxSource = NkString("?");
			int32 prefsChoice = 0;		   ///< index dans la liste des API du panneau
			bool prefsNeedsRestart = false; ///< un enregistrement attend un relancement
			NkString prefsStatus;

			char promptBuf[512] = {0};

			/// Ecrit le backend choisi dans **notre** fichier, et nulle part
			/// ailleurs. Relit avant d'ecrire, remplace la seule ligne `gfx`,
			/// ecrit de facon atomique — voir `Backend.h`.
			void SavePrefs(const char *api) {
				if (NkGfxConfigSetKey(NkGfxConfigPath(), "gfx", api)) {
					prefsNeedsRestart = true;
					prefsStatus = NkString("Ecrit : gfx = ");
					prefsStatus.Append(api);
					prefsStatus.Append("  (nkuidesign.cfg)");
				} else {
					// ⚠️ UN ECHEC D'ECRITURE SE DIT AUSSI. Un bouton qui ne fait
					//    rien et ne dit rien est pire qu'un bouton absent : on
					//    croit avoir regle, et on mesure sur autre chose.
					prefsNeedsRestart = false;
					prefsStatus = NkString("ECHEC d'ecriture : le fichier n'a PAS ete modifie "
										   "(rien n'est perdu).");
				}
			}

			void Init() {
				theme = NkTheme::Dark();
				// Le registre est la SOURCE de la palette. On y inscrit ce que cette
				// application connait ; les autres composants s'y inscriront de leur
				// cote, et la palette les affichera sans qu'une ligne bouge ici.
				NkComponentRegistry::Register(NkContentBrowserDecl());
				// ⚠️ LE SECOND COMPOSANT REEL, et c'est lui qui rend l'affirmation
				//    « aucun panneau ne nomme un composant » verifiable. Jusqu'ici
				//    la palette bouclait sur un registre a UNE entree : elle
				//    « marchait » sans rien prouver.
				NkComponentRegistry::Register(NkTreeViewDecl());
				// ⚠️ `host.resolve` N'EST PLUS POSE ICI, et c'est le correctif du
				//    18/08 : sa valeur par defaut EST la resolution de
				//    l'application (`NkDesignResolveRole`). Tant que chaque hote
				//    posait la sienne, la sonde a pu en poser une autre -- un
				//    hachage permissif -- et mesurer autre chose que l'ecran.
				ai.SetBackend(&fileBackend);

				if (!LoadDoc())
					BuildStarterDocument();

				AuditDeclaredRoles();
			}

			// ── L'AUDIT DES ROLES, AU DEMARRAGE ET SANS ATTENDRE UNE IMAGE ──
			// ⚠️ POURQUOI ICI ET PAS AU PREMIER DESSIN : le dessin ne resout que
			//    les roles des composants REELLEMENT POSES dans le document
			//    courant. Un composant declare mais absent du document passerait
			//    donc l'audit sans etre regarde, et son magenta n'apparaitrait
			//    qu'au jour ou quelqu'un le pose. On resout TOUT ce qui est
			//    declare, tout de suite : le journal du demarrage dit alors l'etat
			//    de la BIBLIOTHEQUE, pas celui du document ouvert.
			//
			//    Il boucle sur le registre et ne nomme aucun composant.
			void AuditDeclaredRoles() {
				NkRoleAudit::Reset();
				const uint16 n = NkComponentRegistry::Count();
				for (uint16 c = 0; c < n; ++c) {
					const NkComponentDecl *d = NkComponentRegistry::At(c);
					if (!d)
						continue;
					for (uint16 t = 0; t < d->tokenCount; ++t)
						NkDesignResolveRole(d->tokens[t].defaultRole);
				}
				NkRoleAudit::Summary(roleAudit, 8);
			}

			/// Un document de depart qui montre les deux choses a montrer : une
			/// imbrication, et deux modes de taille cote a cote.
			void BuildStarterDocument() {
				doc.NewDocument("Interface de demonstration", NkAuthor::Humain);
				doc.nodes[0].layout.kind = NkLayoutKind::Column;

				const int32 header = doc.AddChild(0, "", NkAuthor::Humain);
				if (doc.IsValidIndex(header)) {
					doc.nodes[(uint32)header].label = NkString("Entete");
					doc.nodes[(uint32)header].height.mode = NkSizeMode::Fixed;
					doc.nodes[(uint32)header].height.value = 56.f;
					doc.nodes[(uint32)header].layout.kind = NkLayoutKind::Row;
				}
				const int32 body = doc.AddChild(0, "", NkAuthor::Humain);
				if (doc.IsValidIndex(body)) {
					doc.nodes[(uint32)body].label = NkString("Corps");
					doc.nodes[(uint32)body].layout.kind = NkLayoutKind::Row;
					// ⚠️ DEUX COMPOSANTS DE NATURES DIFFERENTES, COTE A COTE, ET
					//    C'EST LE POINT. Un document de demonstration a un seul
					//    composant montre une application qui marche ; deux
					//    montrent qu'elle ne connait aucun nom -- l'arbre a ete
					//    ajoute au registre sans qu'une ligne de la palette, de
					//    l'arbre de composition, des proprietes ou de la
					//    sauvegarde ne bouge.
					const int32 arbre = doc.AddChild(body, "tree_view", NkAuthor::Humain);
					if (doc.IsValidIndex(arbre)) {
						doc.nodes[(uint32)arbre].width.mode = NkSizeMode::Fixed;
						doc.nodes[(uint32)arbre].width.value = 260.f;
					}
					doc.AddChild(body, "content_browser", NkAuthor::Humain);
				}
				selected = 0;
				status = NkString("Document de demonstration cree.");
				host.demoModels.Clear();
				host.SyncTo(doc);
			}

			void Recompute(const NkPaintRect &surface) {
				host.SyncTo(doc);
				NkComputeLayout(doc, surface, layout);
			}

			void SaveDoc() {
				NkString out;
				doc.Save(out);
				status = nkentseu::NkFile::WriteAllText(kDocumentPath, out.Data())
							 ? NkString("Document enregistre : ")
							 : NkString("ECHEC d'ecriture : ");
				status.Append(kDocumentPath);
			}

			bool LoadDoc() {
				if (!nkentseu::NkFile::Exists(kDocumentPath))
					return false;
				const NkString text = nkentseu::NkFile::ReadAllText(kDocumentPath);
				uint32 unknown = 0;
				NkUIDocument loaded;
				if (!loaded.Load(text.Data(), &unknown)) {
					// ⚠️ ON NE CHARGE PAS A MOITIE. Un document dont la structure est
					//    incoherente laisse l'ancien en place et le DIT : recuperer un
					//    arbre a demi reconstruit serait pire que ne rien recuperer,
					//    parce que l'utilisateur croirait avoir retrouve son travail.
					status = NkString("Document illisible : rien n'a ete change.");
					return false;
				}
				doc = loaded;
				selected = 0;
				host.demoModels.Clear();
				host.SyncTo(doc);
				char b[192];
				snprintf(b, sizeof(b), "Document charge : %u noeud(s), %u composant(s) inconnu(s)",
						 doc.NodeCount(), unknown);
				status = NkString(b);
				return true;
			}
	};

	// ═══════════════════════════════════════════════════════════════════════════
	//  PANNEAU 1 — LA PALETTE
	// ═══════════════════════════════════════════════════════════════════════════
	// ⚠️ ELLE BOUCLE SUR LE REGISTRE, elle ne nomme aucun composant. Une liste
	//    ecrite en dur afficherait aujourd'hui les bons noms sans qu'aucune
	//    declaration soit lue — elle « marcherait » en ne prouvant rien, et le
	//    jour ou un second composant arrive elle ne l'afficherait pas.
	class PalettePanel : public NkEditorPanel {
		public:
			explicit PalettePanel(DesignState *st)
				: NkEditorPanel("Palette", NkEditorDockSide::NK_LEFT), mSt(st) {}

			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				ec.Text("Ce que la bibliotheque declare");
				ec.Separator();
				(void)ctx;

				// L'entree 0 n'est pas un composant : c'est le CADRE, un noeud qui
				// n'affiche rien et sert a agencer. Il est dans la palette parce
				// qu'une composition en a besoin avant qu'un composant conteneur
				// existe.
				if (Selectable(ctx, "Cadre (agence, n'affiche rien)", mSt->paletteChoice == 0))
					mSt->paletteChoice = 0;

				const uint16 n = NkComponentRegistry::Count();
				for (uint16 i = 0; i < n; ++i) {
					const NkComponentDecl *d = NkComponentRegistry::At(i);
					if (!d)
						continue;
					const char *label = (d->title && *d->title) ? d->title : d->name;
					if (Selectable(ctx, label, mSt->paletteChoice == (int32)i + 1))
						mSt->paletteChoice = (int32)i + 1;
				}

				ec.Separator();
				char b[192];
				designkit::KeyValue(ctx, "cible",
									mSt->doc.IsValidIndex(mSt->selected)
										? mSt->doc.nodes[(uint32)mSt->selected].label.Data()
										: "(aucune)");
				if (ec.Button("Poser dans la selection"))
					Place();

				const NkComponentDecl *d = Chosen();
				if (d) {
					// ⚠️ DEUX PHRASES DE SIX NOMBRES SONT DEVENUES SIX LIGNES
					//    ALIGNEES. « 4 parametres, 3 variantes, 10 jetons » se
					//    lisait comme une phrase — donc se coupait comme une
					//    phrase. Un chiffre par ligne, la cle a gauche : rien a
					//    tronquer, et on compare deux composants d'un coup d'oeil.
					if (designkit::Section(ctx, "Ce que ce composant declare")) {
						snprintf(b, sizeof(b), "%u", d->paramCount);
						designkit::KeyValue(ctx, "parametres", b);
						snprintf(b, sizeof(b), "%u", d->variantCount);
						designkit::KeyValue(ctx, "variantes", b);
						snprintf(b, sizeof(b), "%u", d->tokenCount);
						designkit::KeyValue(ctx, "jetons", b);
						snprintf(b, sizeof(b), "%u", d->metricCount);
						designkit::KeyValue(ctx, "metriques", b);
						snprintf(b, sizeof(b), "%u", d->hookCount);
						designkit::KeyValue(ctx, "greffes", b);
						snprintf(b, sizeof(b), "%u", d->eventCount);
						designkit::KeyValue(ctx, "evenements", b);
					}
					// ⚠️ `TextWrapped`, PAS `Text` : un resume de deux lignes est
					//    fait pour revenir a la ligne, pas pour etre coupe.
					nkgui::TextWrapped(ctx, d->summary ? d->summary : "");
				} else {
					nkgui::TextWrapped(ctx, "Un cadre ne declare rien : il agence ses enfants.");
				}

				// ── CE QUE LA CANONISATION A RATTRAPE ────────────────────────
				// ⚠️ SANS CETTE LIGNE, LE CORRECTIF (a) SE RETOURNERAIT CONTRE
				//    NOUS : l'ecran serait juste, les declarations resteraient
				//    fausses, et personne n'irait verifier -- « une protection qui
				//    empeche d'aller verifier ». Elle est la liste de travail de
				//    la correction a la source, pas une decoration.
				if (NkRoleAudit::RescuedCount() > 0) {
					ec.Separator();
					snprintf(b, sizeof(b), "%u", NkRoleAudit::RescuedCount());
					designkit::KeyValue(ctx, "roles PascalCase", b);
					nkgui::TextWrapped(ctx, "Rattrapes par la canonisation ; a corriger a la source "
											"(NKEditorKit). L'ecran est juste, la declaration ne "
											"l'est pas.");
				}
			}

		private:
			const NkComponentDecl *Chosen() const {
				if (mSt->paletteChoice <= 0)
					return nullptr;
				return NkComponentRegistry::At((uint16)(mSt->paletteChoice - 1));
			}
			void Place() {
				const NkComponentDecl *d = Chosen();
				const int32 created = mSt->doc.AddChild(mSt->selected, d ? d->name : "", NkAuthor::Humain);
				if (created < 0) {
					mSt->status = NkString("Pose refusee : cible invalide ou composant inconnu.");
					return;
				}
				mSt->selected = created;
				mSt->host.SyncTo(mSt->doc);
				mSt->status = NkString("Pose : ");
				mSt->status.Append(mSt->doc.nodes[(uint32)created].label);
			}
			DesignState *mSt;
	};

	// ═══════════════════════════════════════════════════════════════════════════
	//  PANNEAU 2 — L'ARBRE DE COMPOSITION
	// ═══════════════════════════════════════════════════════════════════════════
	// « une interface complete est un composant qui en contient d'autres — meme
	// mecanisme aux deux echelles ». Cet arbre est la vue de ce mecanisme.
	class CompositionPanel : public NkEditorPanel {
		public:
			explicit CompositionPanel(DesignState *st)
				: NkEditorPanel("Composition", NkEditorDockSide::NK_LEFT), mSt(st) {}

			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				ec.Text(mSt->doc.title.Data());

				// ── L'ARBRE, DANS UNE ZONE DEFILABLE ─────────────────────────
				// ⚠️ IL ETAIT POSE A NU dans le panneau : un document de trente
				//    noeuds poussait tout le reste — operations, metriques,
				//    boutons — hors de l'ecran, sans barre pour y revenir. Un
				//    panneau dont le contenu depend de la taille du document n'est
				//    pas un panneau, c'est une liste qui a debordе.
				const NkRect arbre = ctx.NextItemRect(-1.f, 220.f);
				if (nkgui::BeginChild(ctx, "nkuidesign.arbre", arbre, true)) {
					DrawNode(ctx, 0, 0);
					nkgui::EndChild(ctx);
				}

				// ── Les operations de structure, EN FLOT ─────────────────────
				// ⚠️ TOUTES ECRIVENT UN PARENT OU UN RANG. Aucune ne deplace un
				//    rectangle : reordonner n'est pas glisser.
				// ⚠️ ET LEURS LIBELLES ONT ETE RACCOURCIS APRES MESURE, pas par
				//    gout : « Rentrer dans le voisin du dessus » s'affichait
				//    « Rentrer dans le voisin du dessu… ». Deux corrections, et il
				//    faut les deux — un libelle qui tient, ET un conteneur qui
				//    mesure (`BeginFlow` donne a chaque bouton la largeur de son
				//    texte et passe a la ligne). L'une sans l'autre retronquerait
				//    au premier panneau retreci.
				{
					designkit::Flow f(ctx);
					if (nkgui::Button(ctx, "Monter"))
						Reorder(-1);
					if (nkgui::Button(ctx, "Descendre"))
						Reorder(+1);
					if (nkgui::Button(ctx, "Imbriquer"))
						NestIntoPreviousSibling();
					if (nkgui::Button(ctx, "Sortir"))
						Outdent();
					if (nkgui::Button(ctx, "Supprimer"))
						Remove();
				}

				// ── LES METRIQUES DU DOCUMENT ────────────────────────────────
				// Une valeur, tous les noeuds qui la nomment. C'est le benefice
				// direct de la regle « un espacement se nomme » : l'aeration de
				// l'interface entiere se regle ici, pas noeud par noeud.
				if (designkit::Section(ctx, "Metriques du document")) {
					for (uint32 i = 0; i < (uint32)mSt->doc.metrics.Size(); ++i) {
						float32 v = mSt->doc.metrics[i].value;
						if (nkgui::DragFloat(ctx, mSt->doc.metrics[i].name.Data(), v, 0.25f, 0.f, 64.f))
							mSt->doc.metrics[i].value = v;
					}
				}

				if (designkit::Section(ctx, "Document")) {
					// ⚠️ TROIS CHIFFRES, TROIS LIGNES « cle : valeur » — au lieu de
					//    la phrase « 5 noeud(s) — 0 pose(s) par l'IA, 0 corrige(s) »
					//    qui s'affichait « … 0 pose(s) par l'IA, 0… ». Une phrase se
					//    fait couper ; deux colonnes alignees, non.
					char b[64];
					snprintf(b, sizeof(b), "%u", mSt->doc.NodeCount());
					designkit::KeyValue(ctx, "noeuds", b);
					snprintf(b, sizeof(b), "%u", mSt->doc.CountByAuthor(NkAuthor::IA));
					designkit::KeyValue(ctx, "poses par l'IA", b);
					snprintf(b, sizeof(b), "%u", mSt->doc.CountCorrected());
					designkit::KeyValue(ctx, "corriges", b);
					{
						designkit::Flow f(ctx);
						if (nkgui::Button(ctx, "Enregistrer"))
							mSt->SaveDoc();
						if (nkgui::Button(ctx, "Recharger"))
							mSt->LoadDoc();
						if (nkgui::Button(ctx, "Nouveau"))
							mSt->BuildStarterDocument();
					}
				}

				ec.Separator();
				nkgui::TextWrapped(ctx, mSt->status.Data() ? mSt->status.Data() : "");
			}

		private:
			void DrawNode(NkGuiContext &ctx, int32 node, int32 depth) {
				if (!mSt->doc.IsValidIndex(node))
					return;
				const NkUINode &n = mSt->doc.nodes[(uint32)node];
				char line[256];
				char indent[32];
				int32 k = 0;
				for (; k < depth * 2 && k < 30; ++k)
					indent[k] = ' ';
				indent[k] = 0;
				// La provenance se lit DANS l'arbre : un document ou l'on ne voit pas
				// ce que la machine a pose est un document ou personne ne relit ce que
				// la machine a pose.
				const char *mark = n.prov.corrected				  ? " [ia/corrige]"
								   : (n.prov.author == NkAuthor::IA) ? " [ia]"
																	 : "";
				snprintf(line, sizeof(line), "%s%s%s%s", indent, n.label.Data(),
						 n.IsFrame() ? " (cadre)" : "", mark);
				if (Selectable(ctx, line, node == mSt->selected))
					mSt->selected = node;
				for (uint32 i = 0; i < (uint32)n.children.Size(); ++i)
					DrawNode(ctx, n.children[i], depth + 1);
			}

			int32 RankInParent(int32 node) const {
				if (!mSt->doc.IsValidIndex(node))
					return -1;
				const int32 p = mSt->doc.nodes[(uint32)node].parent;
				if (!mSt->doc.IsValidIndex(p))
					return -1;
				const NkVector<int32> &kids = mSt->doc.nodes[(uint32)p].children;
				for (uint32 i = 0; i < (uint32)kids.Size(); ++i)
					if (kids[i] == node)
						return (int32)i;
				return -1;
			}
			void Reorder(int32 delta) {
				const int32 r = RankInParent(mSt->selected);
				if (r < 0)
					return;
				if (mSt->doc.MoveChild(mSt->selected, r + delta))
					mSt->doc.MarkHumanEdit(mSt->selected);
			}
			/// Imbriquer : le noeud entre DANS son voisin du dessus. C'est le geste
			/// « poser un composant dans un autre » sous sa forme clavier ; a la
			/// souris ce sera un glisser, et il appellera exactement le meme
			/// `Reparent`.
			void NestIntoPreviousSibling() {
				const int32 r = RankInParent(mSt->selected);
				if (r <= 0)
					return;
				const int32 p = mSt->doc.nodes[(uint32)mSt->selected].parent;
				const int32 target = mSt->doc.nodes[(uint32)p].children[(uint32)(r - 1)];
				if (mSt->doc.Reparent(mSt->selected, target)) {
					mSt->doc.MarkHumanEdit(mSt->selected);
					mSt->status = NkString("Imbrique dans : ");
					mSt->status.Append(mSt->doc.nodes[(uint32)target].label);
				}
			}
			void Outdent() {
				if (!mSt->doc.IsValidIndex(mSt->selected))
					return;
				const int32 p = mSt->doc.nodes[(uint32)mSt->selected].parent;
				if (!mSt->doc.IsValidIndex(p))
					return;
				const int32 gp = mSt->doc.nodes[(uint32)p].parent;
				if (!mSt->doc.IsValidIndex(gp))
					return;
				if (mSt->doc.Reparent(mSt->selected, gp))
					mSt->doc.MarkHumanEdit(mSt->selected);
			}
			void Remove() {
				NkVector<int32> remap;
				if (!mSt->doc.RemoveSubtree(mSt->selected, &remap)) {
					mSt->status = NkString("La racine ne se supprime pas.");
					return;
				}
				// ⚠️ LA SUPPRESSION RENUMEROTE : la selection courante est perimee et
				//    doit etre reparee ICI, sinon elle designerait un autre noeud — un
				//    defaut qui ne se voit qu'a la modification suivante, donc loin de
				//    sa cause.
				mSt->selected = 0;
				mSt->host.demoModels.Clear();
				mSt->host.SyncTo(mSt->doc);
				mSt->status = NkString("Noeud supprime.");
			}
			DesignState *mSt;
	};

	// ═══════════════════════════════════════════════════════════════════════════
	//  PANNEAU 3 — L'APERCU
	// ═══════════════════════════════════════════════════════════════════════════
	// C'est le CONSOMMATEUR. Il n'affiche pas une image du document : il calcule
	// sa mise en page et appelle les fonctions de dessin memes que l'application
	// finale appellera.
	class PreviewPanel : public NkEditorPanel {
		public:
			explicit PreviewPanel(DesignState *st)
				: NkEditorPanel("Apercu", NkEditorDockSide::NK_CENTER), mSt(st) {}

			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				ec.Text("Le document, dessine par le kit. Cliquez pour selectionner ;");
				ec.Text("tirez le bord droit ou bas d'un noeud pour changer sa TAILLE.");

				// ── LE REPLI FRANC, A L'ENDROIT OU LE MAGENTA APPARAIT ───────
				// ⚠️ C'EST LA MOITIE (b) DU CORRECTIF DU 18/08. Le magenta de
				//    `NkTheme::Get` disait « un role est faux » et rien d'autre :
				//    ni lequel, ni combien, ni dans quel composant. Il a fallu
				//    lire trois fichiers pour le savoir. Ce bandeau le DIT, juste
				//    au-dessus du dessin fautif -- et il ne s'affiche pas quand
				//    tout va bien, sinon on cesserait de le lire.
				if (NkRoleAudit::FaultCount() > 0) {
					NkString resume;
					NkRoleAudit::Summary(resume, 6);
					ec.Text("!! ROLE(S) DE THEME NON RESOLU(S) -- ce qui suit est peint en magenta :");
					ec.Text(resume.Data());
				}
				const NkRect area = ctx.NextItemRect(-1.f, 520.f);
				if (area.w <= 0.f || area.h <= 0.f)
					return;

				const NkPaintRect surface = {area.x, area.y, area.w, area.h};
				mSt->Recompute(surface);

				NkComponentInput in;
				in.surfaceScale = 1.f; ///< ⚠️ A BRANCHER sur le DPI reel de la surface
				in.mouseX = ctx.input.mousePos.x;
				in.mouseY = ctx.input.mousePos.y;
				in.wheel = ctx.input.wheel;
				in.mouseDown = ctx.input.mouseDown[0];
				in.mousePressed = ctx.input.mouseClicked[0];
				in.mouseReleased = ctx.input.mouseReleased[0];
				in.doubleClick = ctx.input.mouseDoubleClicked[0];
				in.rightPressed = ctx.input.mouseClicked[1];
				in.ctrl = ctx.input.ctrlDown;
				in.shift = ctx.input.shiftDown;

				HandleMouse(in);

				// ⚠️ `NkDesignPaint`, PAS `NkGuiComponentPaint` : c'est lui qui
				//    traduit les poignees de CETTE application en dessins. Le
				//    peintre du kit peint un carre pour toute poignee non nulle —
				//    correct pour lui (il ne connait l'enumeration de personne),
				//    insuffisant pour un chevron, qui doit dire « ouvert » ou
				//    « ferme ». Il surcharge `Icon` et RIEN d'autre.
				NkDesignPaint paint(ctx, mSt->theme);
				NkDrawDocument(paint, in, mSt->doc, mSt->layout, mSt->host);

				// Le liseré de selection se peint APRES le document et n'en fait pas
				// partie : c'est du mobilier d'editeur. La sonde ne le voit pas, et
				// c'est voulu — elle mesure l'interface, pas le decor.
				if (mSt->layout.Has(mSt->selected))
					// ⚠️ « AccentUi » ETAIT ECRIT ICI, et ce liseré etait donc MAGENTA
					//    lui aussi -- dans mon propre fichier, pas dans celui d'un
					//    autre agent. La resolution canonise desormais, mais le nom
					//    canonique s'ecrit quand meme : la canonisation est un filet,
					//    pas une dispense.
					paint.OutlineSharp(mSt->layout.At(mSt->selected),
									   NkDesignResolveRole("accent_ui"));
			}

		private:
			// ── LA SOURIS, TRADUITE ──────────────────────────────────────────
			// ⚠️ RIEN ICI N'ECRIT UNE POSITION. Un clic ecrit une SELECTION ; un
			//    glisser de bord ecrit une TAILLE ou un POIDS (`NkResizeByDrag`).
			//    C'est la difference entre un outil de design et un constructeur
			//    d'interfaces, et elle se joue exactement dans cette fonction.
			void HandleMouse(const NkComponentInput &in) {
				const float32 kHandle = 6.f;
				if (in.mousePressed) {
					const int32 hit = NkPickNode(mSt->doc, mSt->layout, in.mouseX, in.mouseY);
					if (hit >= 0) {
						mSt->selected = hit;
						const NkPaintRect r = mSt->layout.At(hit);
						const bool nearRight = in.mouseX >= r.x + r.w - kHandle;
						const bool nearBottom = in.mouseY >= r.y + r.h - kHandle;
						if (nearRight || nearBottom) {
							mDragging = true;
							mDragHorizontal = nearRight;
							mDragNode = hit;
						}
					}
				}
				if (mDragging && in.mouseDown) {
					const float32 delta = mDragHorizontal ? in.mouseX - mLastX : in.mouseY - mLastY;
					if (delta != 0.f)
						NkResizeByDrag(mSt->doc, mSt->layout, mDragNode, mDragHorizontal, delta);
				}
				if (!in.mouseDown)
					mDragging = false;
				mLastX = in.mouseX;
				mLastY = in.mouseY;
			}

			DesignState *mSt;
			bool mDragging = false;
			bool mDragHorizontal = true;
			int32 mDragNode = -1;
			float32 mLastX = 0.f, mLastY = 0.f;
	};

	// ═══════════════════════════════════════════════════════════════════════════
	//  PANNEAU 4 — LES PROPRIETES
	// ═══════════════════════════════════════════════════════════════════════════
	// ⚠️ AUCUN WIDGET DE COMPOSANT N'EST ECRIT EN DUR ICI. La section « reglages »
	//    BOUCLE sur les tables de la declaration : ajouter un parametre au
	//    composant le fait apparaitre SANS TOUCHER A CE FICHIER. C'est ce qui
	//    distingue « lire la declaration » de « connaitre le composant » — un
	//    panneau ecrit a la main afficherait les memes curseurs aujourd'hui et
	//    mentirait des le premier ajout.
	class PropertiesPanel : public NkEditorPanel {
		public:
			explicit PropertiesPanel(DesignState *st)
				: NkEditorPanel("Proprietes", NkEditorDockSide::NK_RIGHT), mSt(st) {}

			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				if (!mSt->doc.IsValidIndex(mSt->selected)) {
					ec.Text("Aucun noeud selectionne.");
					return;
				}
				NkUINode &n = mSt->doc.nodes[(uint32)mSt->selected];

				ec.Text(n.label.Data());
				ec.Separator();
				if (designkit::Section(ctx, "Provenance"))
					DrawProvenance(ec, ctx, n);
				if (designkit::Section(ctx, "Largeur"))
					DrawSizing(ec, ctx, n.width);
				if (designkit::Section(ctx, "Hauteur"))
					DrawSizing(ec, ctx, n.height);
				if (designkit::Section(ctx, "Agencement de ses enfants"))
					DrawLayout(ec, ctx, n);
				if (designkit::Section(ctx, "Reglages du composant"))
					DrawComponentSettings(ec, ctx, n);
			}

		private:
			void Edited() {
				mSt->doc.MarkHumanEdit(mSt->selected);
			}

			void DrawProvenance(NkEditorFrameContext &ec, NkGuiContext &ctx, const NkUINode &n) {
				(void)ec;
				char b[192];
				snprintf(b, sizeof(b), "%s%s%s", NkAuthorName(n.prov.author),
						 n.prov.verified ? " · rejouee" : "", n.prov.corrected ? " · corrigee" : "");
				designkit::KeyValue(ctx, "auteur", b);
				if (n.prov.origin.Length() > 0)
					designkit::KeyValue(ctx, "origine", n.prov.origin.Data());
				// ⚠️ AUCUNE CASE A COCHER ICI, ET C'EST DELIBERE. « rejouee » est un
				//    constat de mesure, « corrigee » se deduit d'une edition : les
				//    rendre cochables ferait entrer dans le corpus des tampons poses a
				//    la main, c'est-a-dire du bruit qui ressemble a du signal.
			}

			// ⚠️ LES CINQ MODES ETAIENT CINQ `Selectable` EMPILES -- c'est ce que
			//    Rodolf a vu comme « une liste de valeurs cliquables ». Cinq lignes
			//    pour UN reglage, et rien ne disait qu'elles s'excluaient. Un
			//    controle segmente le dit par sa forme : une ligne, N cases, une
			//    seule allumee.
			void DrawSizing(NkEditorFrameContext &ec, NkGuiContext &ctx, NkSizeDecl &s) {
				(void)ec;
				const char *modes[(uint32)NkSizeMode::Count];
				for (uint8 i = 0; i < (uint8)NkSizeMode::Count; ++i)
					modes[i] = NkSizeModeName((NkSizeMode)i);
				const int32 pick =
					designkit::Segmented(ctx, modes, (int32)NkSizeMode::Count, (int32)s.mode);
				if (pick >= 0) {
					s.mode = (NkSizeMode)pick;
					Edited();
				}
				if (s.mode == NkSizeMode::Fixed) {
					if (nkgui::DragFloat(ctx, "taille (px)", s.value, 1.f, 0.f, 1200.f))
						Edited();
				} else if (s.mode == NkSizeMode::Weight) {
					if (nkgui::DragFloat(ctx, "poids", s.value, 0.05f, 0.f, 8.f))
						Edited();
				}
				// ⚠️ « max (<= min : non borne) » S'AFFICHAIT « max (<= mir… ».
				//    Le libelle porte maintenant le nom, et la convention est dite
				//    UNE fois sous les deux champs -- une explication repetee dans
				//    un libelle est une explication qui sera coupee.
				static const float32 kPoids[2] = {-1.f, -1.f};
				nkgui::BeginRow(ctx, 0.f, kPoids, 2);
				if (nkgui::DragFloat(ctx, "min", s.minVal, 1.f, 0.f, 800.f))
					Edited();
				if (nkgui::DragFloat(ctx, "max", s.maxVal, 1.f, 0.f, 1600.f))
					Edited();
				nkgui::EndRow(ctx);
				nkgui::TextWrapped(ctx, "max <= min : non borne.");
			}

			void DrawLayout(NkEditorFrameContext &ec, NkGuiContext &ctx, NkUINode &n) {
				(void)ec;
				const char *kinds[(uint32)NkLayoutKind::Count];
				for (uint8 i = 0; i < (uint8)NkLayoutKind::Count; ++i)
					kinds[i] = NkLayoutKindName((NkLayoutKind)i);
				const int32 pick =
					designkit::Segmented(ctx, kinds, (int32)NkLayoutKind::Count, (int32)n.layout.kind);
				if (pick >= 0) {
					n.layout.kind = (NkLayoutKind)pick;
					Edited();
				}
				// ⚠️ NI GOUTTIERE NI MARGE EN NOMBRE ICI, ET C'EST LA REGLE DU KIT :
				//    un espacement est du STYLE, il se NOMME. Le noeud designe deux
				//    metriques ; leurs VALEURS s'editent une fois pour tout le
				//    document, dans le panneau Composition. Mettre un curseur de
				//    pixels ici ferait exister la valeur a deux endroits, et l'editeur
				//    n'en changerait qu'un.
				char nm[160];
				snprintf(nm, sizeof(nm), "%s (%0.1f px)", n.spacingName.Data(),
						 mSt->doc.Metric(n.spacingName.Data()));
				designkit::KeyValue(ctx, "espacement", nm);
				snprintf(nm, sizeof(nm), "%s (%0.1f px)", n.padName.Data(),
						 mSt->doc.Metric(n.padName.Data()));
				designkit::KeyValue(ctx, "remplissage", nm);
				if (n.layout.kind == NkLayoutKind::Grid) {
					float32 cols = (float32)n.layout.gridColumns;
					if (ec.SliderFloat("colonnes", cols, 1.f, 8.f)) {
						n.layout.gridColumns = (uint16)(cols + 0.5f);
						Edited();
					}
				}
				ec.Text("Alignement transverse");
				const char *aligns[(uint32)NkAlign::Count];
				for (uint8 i = 0; i < (uint8)NkAlign::Count; ++i)
					aligns[i] = NkAlignName((NkAlign)i);
				const int32 pickA =
					designkit::Segmented(ctx, aligns, (int32)NkAlign::Count, (int32)n.layout.crossAlign);
				if (pickA >= 0) {
					n.layout.crossAlign = (NkAlign)pickA;
					Edited();
				}
			}

			void DrawComponentSettings(NkEditorFrameContext &ec, NkGuiContext &ctx, NkUINode &n) {
				const NkComponentDecl *d = n.IsFrame() ? nullptr : n.instance.Decl();
				if (!d) {
					ec.Text("Un cadre n'a pas de reglages : il agence.");
					return;
				}
				ec.Text("Representation");
				for (uint16 i = 0; i < d->variantCount; ++i) {
					const bool cur = n.instance.HasVariant() && n.instance.Variant() == (int32)i;
					if (Selectable(ctx, d->variants[i].label, cur)) {
						n.instance.SetVariant((int32)i);
						Edited();
					}
				}

				ec.Separator();
				ec.Text("Parametres");
				for (uint16 i = 0; i < d->paramCount; ++i) {
					const NkParamDecl &pd = d->params[i];
					float32 v = n.instance.Param(pd.name);
					if (pd.kind == NkParamKind::Bool) {
						bool b = v > 0.5f;
						if (ec.Checkbox(pd.label, b)) {
							n.instance.SetParam(pd.name, b ? 1.f : 0.f);
							Edited();
						}
					} else {
						// ⚠️ LES BORNES VIENNENT DE LA DECLARATION, pas de l'editeur. Un
						//    editeur qui bornerait lui-meme creerait une seconde verite,
						//    et le jour ou la declaration change, le curseur resterait
						//    sur l'ancienne.
						const float32 lo = pd.maxVal > pd.minVal ? pd.minVal : 0.f;
						const float32 hi = pd.maxVal > pd.minVal ? pd.maxVal : 1.f;
						if (ec.SliderFloat(pd.label, v, lo, hi)) {
							n.instance.SetParam(pd.name, v);
							Edited();
						}
					}
				}

				ec.Separator();
				ec.Text("Metriques (px logiques)");
				for (uint16 i = 0; i < d->metricCount; ++i) {
					const NkMetricDecl &md = d->metrics[i];
					float32 v = n.instance.Metric(md.name);
					// Plage d'edition derivee du defaut declare : faute de bornes dans
					// `NkMetricDecl`, on ouvre autour de la valeur declaree plutot que
					// d'inventer un maximum absolu.
					if (ec.SliderFloat(md.name, v, 0.f, md.defVal * 4.f + 8.f)) {
						n.instance.SetMetric(md.name, v);
						Edited();
					}
				}

				ec.Separator();
				ec.Text("Jetons de theme");
				for (uint16 i = 0; i < d->tokenCount; ++i) {
					const NkTokenDecl &td = d->tokens[i];
					char line[192];
					snprintf(line, sizeof(line), "%s = %s%s", td.name, n.instance.TokenRole(td.name),
							 n.instance.IsTokenOverridden(td.name) ? "  (modifie)" : "");
					ec.Text(line);
				}
				// ⚠️ LA REAFFECTATION D'UN JETON N'A TOUJOURS PAS DE WIDGET, et c'est
				//    une absence NOMMEE : il faudrait un selecteur de role, qui est un
				//    composant a part entiere (il en existe deja un dans NK3DModeler).
				//    L'ecrire ici en serait une copie de plus. Le mecanisme, lui, est
				//    en place et teste.

				ec.Separator();
				ec.Text("Evenements exposes (declares, non branches)");
				for (uint16 i = 0; i < d->eventCount; ++i) {
					const NkEventDecl &e = d->events[i];
					char line[256];
					int32 k = snprintf(line, sizeof(line), "%s(", e.name);
					for (uint8 a = 0; a < e.argCount && k > 0 && k < (int32)sizeof(line); ++a)
						k += snprintf(line + k, sizeof(line) - (uint32)k, "%s%s: %s", a ? ", " : "",
									  e.args[a].name, NkArgTypeName(e.args[a].kind));
					if (k > 0 && k < (int32)sizeof(line))
						snprintf(line + k, sizeof(line) - (uint32)k, ")");
					ec.Text(line);
				}
				ec.Separator();
				if (ec.Button("Reglages : tout reinitialiser")) {
					n.instance.ResetAll();
					Edited();
				}
				char b[128];
				snprintf(b, sizeof(b), "%u ecart(s) par rapport a la declaration",
						 n.instance.OverrideCount());
				ec.Text(b);
			}

			DesignState *mSt;
	};

	// ═══════════════════════════════════════════════════════════════════════════
	//  PANNEAU 5 — LES PREFERENCES
	// ═══════════════════════════════════════════════════════════════════════════
	//  Il ferme la chaine demandee par Rodolf : **Preferences (interface) ->
	//  fichier de config -> lu par defaut au demarrage**. Les deux derniers
	//  maillons existaient ; celui-ci est le premier.
	//
	//  ⚠️ POURQUOI UN PANNEAU ET PAS LA FENETRE « Preferences » DE LA COQUILLE.
	//     Elle existe (`NkEditorShell::OpenPreferences`, categories Polices /
	//     Theme) et ce serait sa place — mais `DrawPreferences` est **privee** et
	//     la coquille n'expose **aucun point de greffe** pour qu'une application y
	//     ajoute une categorie. La toucher demanderait de modifier NKEditorKit,
	//     que je ne tiens pas. **Manque porte au canal** ; en attendant, un
	//     panneau, qui a l'avantage d'etre testable et de ne rien casser chez
	//     personne.
	//
	//  ⚠️ CE QUI EST ECRIT, ET CE QUI NE L'EST PAS. On ecrit **`nkuidesign.cfg`**,
	//     a cote de l'executable. **Jamais `~/.nkcode_*.cfg`** : la coquille les
	//     lit inconditionnellement (`NkLoadTheme`, `NkLoadFontPrefs`), donc cette
	//     application en HERITE — mais NKCode est en pause depuis des semaines et
	//     il fonctionne. Ecrire chez lui pour regler une autre application
	//     casserait une application au repos : un cout pur, pour rien.
	class PreferencesPanel : public NkEditorPanel {
		public:
			explicit PreferencesPanel(DesignState *st)
				: NkEditorPanel("Preferences", NkEditorDockSide::NK_RIGHT), mSt(st) {}

			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				ec.Text("Backend graphique");
				nkgui::TextWrapped(ctx, "Le reglage est ecrit dans nkuidesign.cfg, a cote de "
										"l'executable. Il vaut pour tous les lancements suivants.");
				ec.Separator();

				// ── CE QUI TOURNE MAINTENANT, ET QUI L'A DECIDE ──────────────
				// ⚠️ LES DEUX LIGNES COMPTENT AUTANT. Afficher le backend sans
				//    dire QUI l'a choisi laisserait un utilisateur regler « opengl »
				//    ici, voir « dx11 » se lancer (parce qu'une variable
				//    d'environnement gagne) et n'avoir aucun moyen de comprendre.
				designkit::KeyValue(ctx, "en cours", mSt->gfxEffective.Data());
				// ⚠️ PAS UN `KeyValue` POUR LA SOURCE : « fichier de configuration
				//    nkuidesign.cfg (cle gfx) » ne tient dans aucune colonne, et
				//    s'affichait « fichier de configura… ». Une valeur longue n'est
				//    pas une valeur de tableau : elle se met a la ligne.
				ec.Text("decide par");
				nkgui::TextWrapped(ctx, mSt->gfxSource.Data());
				ec.Separator();

				// ── LE CHOIX ─────────────────────────────────────────────────
				// Les memes noms que la ligne de commande et le fichier : un seul
				// vocabulaire pour les quatre sources, sinon l'interface enseigne
				// un mot que le fichier ne comprend pas.
				static const char *kApis[] = {"auto", "opengl", "vulkan", "dx11", "dx12", "software"};
				const int32 pick = designkit::Segmented(ctx, kApis, 6, mSt->prefsChoice);
				if (pick >= 0)
					mSt->prefsChoice = pick;

				if (ec.Button("Enregistrer dans nkuidesign.cfg"))
					mSt->SavePrefs(kApis[mSt->prefsChoice < 6 ? mSt->prefsChoice : 0]);

				// ── LE REDEMARRAGE, ANNONCE ──────────────────────────────────
				// ⚠️ REGLE DE RODOLF : ce qui implique un redemarrage le DIT ; ce
				//    qui n'en a pas besoin ne le demande pas. Le backend graphique
				//    en implique un — le contexte est cree une fois, au lancement.
				//    La langue, elle, n'en impliquera pas : NKGui devra la gerer a
				//    chaud, et ce panneau ne doit pas prendre l'habitude de
				//    reclamer un redemarrage pour tout.
				if (mSt->prefsNeedsRestart) {
					ec.Separator();
					nkgui::TextWrapped(ctx, "!! Enregistre. Le backend graphique ne change qu'au "
											"PROCHAIN lancement : le contexte est cree une fois, au "
											"demarrage. Fermez et relancez pour l'appliquer.");
				}
				if (!mSt->prefsStatus.Empty()) {
					ec.Separator();
					nkgui::TextWrapped(ctx, mSt->prefsStatus.Data());
				}
			}

		private:
			DesignState *mSt;
	};

	// ═══════════════════════════════════════════════════════════════════════════
	//  PANNEAU 6 — L'IA
	// ═══════════════════════════════════════════════════════════════════════════
	// La place, pas le modele. Ce panneau ne sait rien de ce qu'il y a derriere le
	// backend, et c'est exactement ce qui permettra de le remplacer par Ilyana
	// sans toucher a une ligne d'ici.
	class AIPanel : public NkEditorPanel {
		public:
			explicit AIPanel(DesignState *st)
				: NkEditorPanel("IA", NkEditorDockSide::NK_BOTTOM), mSt(st) {}

			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				char b[256];
				snprintf(b, sizeof(b), "Backend : %s%s",
						 mSt->ai.Backend() ? mSt->ai.Backend()->Name() : "-",
						 (mSt->ai.Backend() && mSt->ai.Backend()->IsAvailable()) ? "" : " (indisponible)");
				ec.Text(b);
				ec.Text("Decrivez l'interface voulue. Elle produit une DECLARATION,");
				ec.Text("posee dans l'arbre comme si vous l'aviez posee vous-meme.");
				InputText(ctx, "Demande", mSt->promptBuf, (int32)sizeof(mSt->promptBuf));

				if (ec.Button("Demander a l'IA"))
					Ask();
				if (ec.Button("Verifier le document par rejeu"))
					Replay();

				ec.Separator();
				ec.Text(mLast.Data() ? mLast.Data() : "");
				ec.Separator();
				// ⚠️ CE QUI N'EST PAS LA, DIT DANS L'INTERFACE ELLE-MEME. Un editeur
				//    muet sur ce qu'il ne fait pas se fait reprocher des absences qu'il
				//    n'a jamais promises.
				ec.Text("Aucun modele specialise n'existe encore : il s'entrainera sur");
				ec.Text("les documents produits ici. Backend reseau : absent (le client");
				ec.Text("HTTP doit monter dans un module partage, pas etre recopie ici).");
			}

		private:
			void Ask() {
				const NkAIResult r = mSt->ai.Ask(mSt->promptBuf, mSt->doc, mSt->selected);
				char b[320];
				if (r.Accepted()) {
					snprintf(b, sizeof(b), "Acceptee : %u noeud(s) poses, rejeu conforme.", r.nodesAdded);
					mSt->host.SyncTo(mSt->doc);
					mSt->selected = r.graftedRoot;
				} else {
					snprintf(b, sizeof(b), "REFUSEE — %s. Le document n'a pas bouge.",
							 NkAIVerdictName(r.verdict));
				}
				mLast = NkString(b);
				if (r.detail.Length() > 0) {
					mLast.Append("  ");
					mLast.Append(r.detail);
				}
			}
			void Replay() {
				const uint32 diffs = NkDesignAI::ReplayDiffs(mSt->doc, mSt->ai.replaySurface);
				char b[192];
				snprintf(b, sizeof(b), "Rejeu du document : %u divergence(s)%s", diffs,
						 diffs == 0 ? " — fidele." : " — NON fidele.");
				mLast = NkString(b);
				if (diffs == 0)
					mSt->doc.MarkVerified(0);
			}
			DesignState *mSt;
			NkString mLast;
	};

} // namespace nkuidesign
