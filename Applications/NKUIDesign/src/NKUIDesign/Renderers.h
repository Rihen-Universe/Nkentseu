#pragma once
// -----------------------------------------------------------------------------
// @File    Renderers.h
// @Brief   DESSINER UN DOCUMENT : du nom declare vers la fonction qui peint.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  LE MANQUE QUE CE FICHIER COMBLE, ET IL FAUT LE NOMMER AVANT DE LE CONTOURNER
// =============================================================================
//  Une `NkComponentDecl` decrit tout d'un composant — parametres, variantes,
//  jetons, metriques, greffes, evenements — **sauf comment le dessiner**. Elle
//  ne porte aucun pointeur de fonction, et c'est deliberе : elle est une
//  constante de compilation verifiable sans rien lier (cf. le bloc FRONTIERE de
//  `NkComponentDecl.h`), et un pointeur de fonction ferait entrer une adresse
//  d'execution dans une donnee censee n'en contenir aucune.
//
//  ⚠️ CONSEQUENCE, ET ELLE EST A SURVEILLER : **chaque hote doit tenir sa
//     propre table nom -> fonction.** Aujourd'hui il y a un hote et une entree,
//     donc ca ne coute rien. A quatre hotes (Nogee, NK3DModeler, NkAnimaEditor,
//     PV3DE) et huit composants, ce sera quatre tables a tenir a jour, et la
//     troisieme oubliera une entree — le composant sera declare, visible dans
//     toutes les palettes, et invisible a l'ecran dans une application.
//     **Manque porte au canal (cote GARDIEN DE LA FORME)**, avec la piste qui
//     me parait tenir : une table d'enregistrement d'execution a cote du
//     registre, sur le modele de `NkRoleRegistry`, qui laisserait la
//     declaration constante.
//
//  EN ATTENDANT, LE DEFAUT NE PASSE PAS EN SILENCE : un composant declare sans
//  fonction de dessin peint un CARTOUCHE portant son nom et la mention explicite
//  qu'il n'est pas branche. Un rectangle vide aurait laisse croire a un bug de
//  mise en page ; le cartouche dit exactement ce qui manque, et il occupe la
//  bonne place — donc l'agencement se travaille des maintenant, avant meme que
//  le composant sache se peindre.
//
// =============================================================================
//  CE QUE LE DOCUMENT DECRIT, ET CE QU'IL NE DECRIT PAS
// =============================================================================
//  Le document decrit **l'interface** : quels composants, imbriques comment,
//  dimensionnes par quelles regles. Il ne decrit PAS **les donnees** qu'ils
//  affichent — un navigateur de contenu placé dans un document ne transporte pas
//  une liste de fichiers.
//
//  L'hote fournit donc un contenu de demonstration, un par noeud. C'est une
//  separation, pas un raccourci : le jour ou une application reelle branche ses
//  vraies donnees, elle remplace ce fournisseur et ne touche ni au document, ni
//  au dessin.
//
// OU AJOUTER LA PROCHAINE CHOSE :
//   un composant de plus -> une entree dans `Draw()`, sous le `if` du precedent.
//   Rien d'autre a modifier : la palette, l'arbre, l'agencement et la sauvegarde
//   bouclent deja sur le registre et ne connaissent aucun nom.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkContentBrowserModel.h"
#include "NKEditorKit/Components/NkRecordingPaint.h"
#include "NKEditorKit/Components/NkTreeViewModel.h"

#include "Icons.h"
#include "Layout.h"
#include "Roles.h"

namespace nkuidesign {

	using nkentseu::editorkit::NkAssetEntry;
	using nkentseu::editorkit::NkComponentInput;
	using nkentseu::editorkit::NkComponentPaint;
	using nkentseu::editorkit::NkContentBrowserHooks;
	using nkentseu::editorkit::NkContentBrowserModel;
	using nkentseu::editorkit::NkContentBrowserStyle;
	using nkentseu::editorkit::NkTextAlign;
	using nkentseu::editorkit::NkTreeNode;
	using nkentseu::editorkit::NkTreeViewHooks;
	using nkentseu::editorkit::NkTreeViewModel;
	using nkentseu::editorkit::NkTreeViewStyle;

	/// Comment un nom de role de theme devient un identifiant. L'hote fenetre
	/// passe `NkResolveRole` ; la sonde passe sa propre table, injective et sans
	/// theme. C'est ce qui permet au MEME code de dessin de tourner avec et sans
	/// GPU — et donc au temoin headless de mesurer ce que l'ecran montrera.
	using NkRoleResolver = uint16 (*)(const char *roleName);

	// ── L'HOTE ──────────────────────────────────────────────────────────────
	// Il porte ce que le document ne porte pas : le contenu de demonstration, la
	// resolution des roles, et le dernier evenement recu.
	struct NkDocumentHost {
			// ⚠️ LA VALEUR PAR DEFAUT EST LA RESOLUTION REELLE, et c'est
			//    STRUCTUREL, pas un confort. Ce champ valait `nullptr` : chaque hote
			//    devait poser SA resolution, et la sonde en a pose une AUTRE -- un
			//    hachage permissif qui ne rendait jamais NK_ROLE_INVALID. Resultat :
			//    68 essais verts sur un ecran magenta. Un defaut par defaut vaut
			//    mieux qu'un champ vide quand le champ vide autorise deux verites.
			NkRoleResolver resolve = &NkDesignResolveRole;
			/// Un modele de demonstration par noeud, aligne sur `doc.nodes`.
			NkVector<NkContentBrowserModel> demoModels;
			/// ⚠️ UN SECOND JEU, ET IL FAUT LES DEUX. Un composant garde son etat
			///    (ouverture, selection, defilement) DANS son modele : partager un
			///    seul modele entre deux natures de composant aurait fait
			///    disparaitre l'etat du premier des que le second est pose.
			NkVector<NkTreeViewModel> demoTrees;

			// Ce que les composants ont signale pendant la derniere passe. Les
			// evenements traversent donc le document jusqu'a l'application, comme
			// ils traversaient deja le composant seul.
			NkString lastEvent;
			int32 selectCount = 0, doubleClickCount = 0;

			/// A appeler quand le nombre de noeuds a change. Volontairement
			/// explicite : reconstruire a chaque image jetterait la selection
			/// courante de chaque navigateur a chaque image, et le defaut se
			/// manifesterait comme « le clic ne marche pas » — loin de sa cause.
			void SyncTo(const NkUIDocument &doc) {
				const uint32 n = (uint32)doc.nodes.Size();
				if ((uint32)demoModels.Size() == n && (uint32)demoTrees.Size() == n)
					return;
				demoModels.Clear();
				demoTrees.Clear();
				for (uint32 i = 0; i < n; ++i) {
					NkContentBrowserModel m;
					FillDemo(m);
					demoModels.PushBack(m);
					NkTreeViewModel t;
					FillDemoTree(t);
					demoTrees.PushBack(t);
				}
			}

			/// La nature d'un asset -> le role de theme qui la teinte. Les cinq
			/// roles `type_*` du coeur existent pour ca ; les prendre au hasard
			/// aurait fait mentir la capture.
			static uint16 RoleOfKind(const char *kind) {
				if (StrEq(kind, "dossier"))
					return NkDesignResolveRole("type_folder");
				if (StrEq(kind, "maillage"))
					return NkDesignResolveRole("type_mesh");
				if (StrEq(kind, "materiau"))
					return NkDesignResolveRole("type_mat");
				if (StrEq(kind, "texture"))
					return NkDesignResolveRole("type_tex");
				if (StrEq(kind, "animation"))
					return NkDesignResolveRole("type_anim");
				return NkDesignResolveRole("text_muted");
			}

			static void FillDemo(NkContentBrowserModel &m) {
				struct Row {
						const char *name;
						const char *kind;
						bool folder;
				};
				static const Row kRows[] = {
					{"Materiaux", "dossier", true},		  {"Maillages", "dossier", true},
					{"Textures", "dossier", true},		  {"caisse.nkmesh", "maillage", false},
					{"sol.nkmat", "materiau", false},	  {"bois_albedo.nktex", "texture", false},
					{"metal.nkmat", "materiau", false},	  {"perso.nkmesh", "maillage", false},
					{"ciel.nktex", "texture", false},	  {"herbe.nkmat", "materiau", false},
					{"rocher.nkmesh", "maillage", false}, {"eau.nkmat", "materiau", false},
				};
				m.entries.Clear();
				for (uint32 i = 0; i < sizeof(kRows) / sizeof(kRows[0]); ++i) {
					NkAssetEntry e;
					e.name = NkString(kRows[i].name);
					// ⚠️ CHEMIN NON VIDE, VOLONTAIREMENT : `path` est la charge que les
					//    evenements portent vers un blueprint. Un contenu de
					//    demonstration au chemin vide validerait une charge inutilisable
					//    sans que personne s'en apercoive.
					e.path = NkString("/projet/");
					e.path.Append(kRows[i].name);
					e.isFolder = kRows[i].folder;
					e.kindLabel = kRows[i].kind;
					// ⚠️ LE ROLE VIENT DE LA NATURE, PAS D'UN MODULO. La premiere
					//    ecriture posait `4 + (i % 5)` -- des identifiants pris au
					//    hasard dans l'enumeration du coeur. Ca « marchait » : le
					//    dessin recevait bien un role par entree. Mais un temoin
					//    visuel pris dessus montrait des textures peintes en
					//    couleur de texte, et personne n'aurait su si le defaut
					//    venait du composant ou de la donnee de demonstration.
					//    Une donnee de demonstration fausse rend une capture
					//    ininterpretable -- c'est le cout, et il est reel.
					e.kindRole = RoleOfKind(kRows[i].kind);
					m.entries.PushBack(e);
				}
				m.breadcrumb.Clear();
				m.breadcrumb.PushBack(NkString("projet"));
				m.breadcrumb.PushBack(NkString("assets"));
				m.breadcrumb.PushBack(NkString("niveau1"));
			}

			/// ⚠️ ORDRE PREFIXE OBLIGATOIRE (`NkTreeViewModel::IsWellFormed`) : un
			///    parent precede toujours ses enfants, et le premier enfant est le
			///    noeud SUIVANT. Ecrit a plat ci-dessous plutot que construit par
			///    une fonction recursive, precisement pour que l'ordre se LISE.
			static void FillDemoTree(NkTreeViewModel &t) {
				struct Row {
						int32 parent;
						const char *label;
						const char *kind;
				};
				static const Row kRows[] = {
					{-1, "Scene", "racine"},	  {0, "Environnement", "groupe"},
					{1, "Soleil", "lumiere"},	  {1, "Ciel", "lumiere"},
					{0, "Decor", "groupe"},		  {4, "Sol", "maillage"},
					{4, "Rocher", "maillage"},	  {4, "Caisse", "maillage"},
					{0, "Personnages", "groupe"}, {8, "Heros", "maillage"},
					{9, "Squelette", "os"},		  {8, "Garde", "maillage"},
				};
				t.nodes.Clear();
				for (uint32 i = 0; i < sizeof(kRows) / sizeof(kRows[0]); ++i) {
					NkTreeNode n;
					n.id = (nkentseu::nk_uint64)(i + 1);
					n.parent = kRows[i].parent;
					n.label = NkString(kRows[i].label);
					// Meme raison que chez le navigateur : le chemin est la CHARGE
					// qu'un evenement porte vers un blueprint. Vide, il validerait
					// une charge inutilisable sans que personne le remarque.
					n.path = NkString("/scene/");
					n.path.Append(kRows[i].label);
					n.kindLabel = kRows[i].kind;
					n.kindRole = RoleOfKind(kRows[i].kind);
					t.nodes.PushBack(n);
				}
				t.active = 6; // « Rocher »
				t.chosen.PushBack(6);
			}

			uint16 Role(const char *name) const {
				return resolve ? resolve(name) : (uint16)0;
			}

			// ── Les ecouteurs, branches sur CHAQUE composant du document ──────
			static void OnSelect(void *u, int32 i, const char *path) {
				NkDocumentHost *h = (NkDocumentHost *)u;
				++h->selectCount;
				h->lastEvent = NkString("onSelect -> ");
				h->lastEvent.Append(path ? path : "");
				(void)i;
			}
			static void OnDoubleClick(void *u, int32 i, const char *path) {
				NkDocumentHost *h = (NkDocumentHost *)u;
				++h->doubleClickCount;
				h->lastEvent = NkString("onDoubleClick -> ");
				h->lastEvent.Append(path ? path : "");
				(void)i;
			}
	};

	namespace renderdetail {

		/// Le cartouche d'un composant declare mais sans dessin branche — voir le
		/// bloc en tete de fichier. Il occupe EXACTEMENT le rectangle du noeud :
		/// c'est ce qui permet de travailler l'agencement avant que le composant
		/// sache se peindre.
		inline void DrawPlaceholder(NkComponentPaint &p, const NkPaintRect &r, const char *name,
									const NkDocumentHost &host) {
			if (r.w <= 0.f || r.h <= 0.f)
				return;
			// ⚠️ NOMS CANONIQUES EN snake_case. `NkResolveRole` compare octet pour
			//    octet contre `themedetail::RoleNames()` ; « PanelBg » n'y figure
			//    pas et rendrait NK_ROLE_INVALID, donc du MAGENTA a l'ecran. C'est
			//    exactement ce qu'a montre le premier temoin visuel du 18/08.
			p.Outline(r, host.Role("border"), host.Role("panel_bg"), 2.f);
			NkPaintRect label = r;
			label.x += 6.f;
			label.w -= 12.f;
			label.h = p.LineHeight();
			label.y += 4.f;
			p.Text(label, name, host.Role("text"), NkTextAlign::Left);
			label.y += p.LineHeight() + 2.f;
			p.Text(label, "declare, dessin non branche", host.Role("text_muted"),
				   NkTextAlign::Left);
		}

		/// Le cadre : il n'affiche rien. Un liseré serait du mobilier d'editeur,
		/// et le mobilier d'editeur n'a rien a faire dans le rendu du document —
		/// sinon la sonde mesurerait le decor en croyant mesurer l'interface.
		inline void DrawFrame(NkComponentPaint &, const NkPaintRect &, const NkDocumentHost &) {}

		inline NkContentBrowserStyle BrowserStyle(const NkDocumentHost &host, const NkUINode &n) {
			NkContentBrowserStyle s;
			s.panelBg = host.Role(n.instance.TokenRole("panel_bg"));
			s.headerBg = host.Role(n.instance.TokenRole("header_bg"));
			s.border = host.Role(n.instance.TokenRole("border"));
			s.text = host.Role(n.instance.TokenRole("text"));
			s.textMuted = host.Role(n.instance.TokenRole("text_muted"));
			s.cardBg = host.Role(n.instance.TokenRole("card_bg"));
			s.cardFooterBg = host.Role(n.instance.TokenRole("card_footer_bg"));
			s.activeMark = host.Role(n.instance.TokenRole("active_mark"));
			s.chosenMark = host.Role(n.instance.TokenRole("chosen_mark"));
			s.folderTint = host.Role(n.instance.TokenRole("folder_tint"));
			// C'EST L'INSTANCE DU NOEUD QUI FOURNIT LES NOMBRES : deux navigateurs
			// poses dans le meme document peuvent donc avoir des reglages
			// differents, ce qui est exactement le sens de « une instance par
			// noeud ».
			s.values = &n.instance;
			return s;
		}

		inline NkTreeViewStyle TreeStyle(const NkDocumentHost &host, const NkUINode &n) {
			NkTreeViewStyle s;
			s.panelBg = host.Role(n.instance.TokenRole("panel_bg"));
			s.headerBg = host.Role(n.instance.TokenRole("header_bg"));
			s.border = host.Role(n.instance.TokenRole("border"));
			s.text = host.Role(n.instance.TokenRole("text"));
			s.textMuted = host.Role(n.instance.TokenRole("text_muted"));
			s.rowHover = host.Role(n.instance.TokenRole("row_hover"));
			s.activeMark = host.Role(n.instance.TokenRole("active_mark"));
			s.activeText = host.Role(n.instance.TokenRole("active_text"));
			s.chosenMark = host.Role(n.instance.TokenRole("chosen_mark"));
			s.guide = host.Role(n.instance.TokenRole("guide"));
			s.dropMark = host.Role(n.instance.TokenRole("drop_mark"));
			s.iconTint = host.Role(n.instance.TokenRole("icon_tint"));
			s.dimTint = host.Role(n.instance.TokenRole("dim_tint"));
			// ⚠️ LA LIGNE QUI MANQUAIT, et elle tenait en un appel. Le composant
			//    emettait 29 commandes `Icon` par image, toutes a poignee NULLE :
			//    il DEMANDAIT ses icones depuis le debut, personne ne lui en
			//    donnait. Le contrat est pourtant explicite dans sa declaration —
			//    la poignee est opaque, l'hote la choisit. C'est une absence que
			//    seule l'utilisation revele : aucune relecture du composant ne
			//    pouvait la montrer, puisqu'il fait exactement ce qu'il annonce.
			s.icons = NkDesignTreeIcons();
			s.values = &n.instance;
			return s;
		}

	} // namespace renderdetail

	// ── LE RENDU D'UN DOCUMENT ──────────────────────────────────────────────
	// Parcours en profondeur : un parent se peint avant ses enfants, donc les
	// enfants se posent par-dessus. C'est l'ordre attendu de toute composition,
	// et il rend l'imbrication visible sans aucune notion de plan.
	inline void NkDrawDocument(NkComponentPaint &p, const NkComponentInput &in, const NkUIDocument &doc,
							   const NkLayoutResult &lay, NkDocumentHost &host, int32 node = 0) {
		if (!doc.IsValidIndex(node) || !lay.Has(node))
			return;
		const NkUINode &n = doc.nodes[(uint32)node];
		const NkPaintRect r = lay.At(node);

		if (n.IsFrame()) {
			renderdetail::DrawFrame(p, r, host);
		} else if (StrEq(n.component.Data(), "content_browser")) {
			if ((uint32)node < (uint32)host.demoModels.Size() && r.w > 0.f && r.h > 0.f) {
				NkContentBrowserHooks hooks;
				hooks.user = &host;
				hooks.onSelect = &NkDocumentHost::OnSelect;
				hooks.onDoubleClick = &NkDocumentHost::OnDoubleClick;
				nkentseu::editorkit::NkDrawContentBrowser(p, in, r, host.demoModels[(uint32)node],
														  renderdetail::BrowserStyle(host, n), hooks);
			}
		} else if (StrEq(n.component.Data(), "tree_view")) {
			// ⚠️ LE SECOND COMPOSANT REEL, et il n'a rien coute d'autre que ces
			//    huit lignes : la palette, l'arbre de composition, l'agencement,
			//    les proprietes et la sauvegarde le connaissaient DEJA, parce
			//    qu'aucun d'eux ne nomme un composant. C'est la premiere fois que
			//    cette affirmation est verifiee sur autre chose qu'une
			//    declaration ecrite par celui qui la teste.
			if ((uint32)node < (uint32)host.demoTrees.Size() && r.w > 0.f && r.h > 0.f) {
				NkTreeViewHooks hooks;
				hooks.user = &host;
				hooks.onSelect = &NkDocumentHost::OnSelect;
				nkentseu::editorkit::NkDrawTreeView(p, in, r, host.demoTrees[(uint32)node],
													renderdetail::TreeStyle(host, n), hooks);
			}
		} else {
			// Declare dans le registre, mais aucune fonction de dessin ici.
			renderdetail::DrawPlaceholder(p, r, n.label.Data(), host);
		}

		for (uint32 i = 0; i < (uint32)n.children.Size(); ++i)
			NkDrawDocument(p, in, doc, lay, host, n.children[i]);
	}

} // namespace nkuidesign
