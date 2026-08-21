#pragma once
// =============================================================================
// NkModelerScreens.h â€” les zones de l'ecran A, peintes une par une.
//
// Une fonction par zone, dans l'ordre ou elles apparaissent a l'ecran. Chacune
// recoit son rectangle et ne peint QUE dedans : c'est ce qui permettra de les
// deplacer, replier ou remplacer une par une sans toucher aux autres.
//
// Les valeurs affichees sont des EXEMPLES cales sur la maquette (Cube, Sphere,
// 8 sommets, 6 faces...). Elles seront remplacees par la scene reelle zone par
// zone -- c'est la marche a suivre convenue avec Rihen : construire, puis mettre
// a jour en developpant.
// =============================================================================

#include "NK3DModeler/Viewport/NkViewport3D.h"
#include "NK3DModeler/Viewport/NkDemo3DHost.h" // PORTAGE INTEGRAL de --demo=2
#include "NK3DModeler/Viewport/NkOutCompose.h" // formes d'incrustation : dimensions et noms
#include "NK3DModeler/Shell/NkModelerUI.h"
#include "NK3DModeler/Shell/NkModelerInput.h"
#include "NK3DModeler/Shell/NkModelerWidgets.h"
#include "NK3DModeler/Shell/NkModelerTables.h" // metriques, listes, catalogues
#include "NK3DModeler/Shell/NkModelerCommon.h"
#include "NK3DModeler/Shell/NkModelerViewport.h" // la vue 3D et ses surcouches
#include "NK3DModeler/Shell/NkModelerFileDialog.h" // choix d emplacement + nom
// DECLARATION ANTICIPEE : NkModelerAssets.h est inclus APRES cet en-tete,
// mais le panneau Materiau a besoin d'ecrire un .nkmat sur-le-champ (bouton
// « Nouveau »). La definition, elle, est bien vue plus loin dans la meme
// unite de traduction. Sans cela il faudrait soit reordonner les
// inclusions — risque a l'echelle de ce fichier — soit differer l'ecriture
// a la prochaine sauvegarde, ce qui n'est pas ce qui a ete demande.
namespace nkentseu {
	namespace nk3d {
		struct NkModelerState;
		bool NkProjectWriteAssets(const NkString &root, NkModelerState &st, NkString *err,
								  int32 onlyCard);
		void NkBrowserSyncMats(NkModelerState &st);
	} // namespace nk3d
} // namespace nkentseu
#include "NKEditorKit/NkShortcutTable.h"
// La scrollbar STANDARD de Nkentseu (celle de l'editeur de code) : Rihen la veut
// partout, et son en-tete demande explicitement de ne pas la redessiner ailleurs.
#include "NKEditorKit/NkEditorScrollbar.h"

#include <cstdio>

namespace nkentseu {
	namespace nk3d {



		// â”€â”€ BARRE DE MENUS â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		inline void PaintMenuBarI(NkModelerPainter &p, const NkRect &r, const char *projectName,
								  NkModelerState &st, NkHitRegistry &hit) {
			p.Fill(r, NkRole::PanelHeader);
			p.HLine(r.x, r.y + r.h - 1.f, r.w);
			// Poignee de deplacement : toute la barre, declaree AVANT les menus et
			// les boutons pour qu'ils la recouvrent.
			hit.Add("win.drag", r);

			const float32 logo = S(22.f);
			const float32 ly = r.y + (r.h - logo) * 0.5f;
			p.Fill({S(10.f), ly, logo, logo}, NkRole::AccentUi, 4.f);
			const float32 nkW = p.TextW("NK");
			p.TextV(S(10.f) + (logo - nkW) * 0.5f, ly, logo, "NK", NkRole::TextOnAccent);

			float32 x = S(10.f) + logo + S(16.f);
			static const char *const kMenus[] = {"Fichier", "Edition", "Fenetre", "Outils",
												 "Selection", "Objet", "Aide"};
			static const char *const kMenuKeys[] = {"menu.0", "menu.1", "menu.2", "menu.3",
													"menu.4", "menu.5", "menu.6"};
			for (int32 i = 0; i < 7; ++i) {
				const float32 w = p.TextW(kMenus[i]) + S(18.f);
				const NkRect mr{x - S(9.f), r.y + S(6.f), w, r.h - S(12.f)};
				const bool over = hit.Add(kMenuKeys[i], mr);
				// Un menu OUVERT reste marque meme si la souris est partie : sinon on ne
				// saurait plus quel menu a produit la liste affichee.
				if (st.openMenu == i)
					p.Fill(mr, NkRole::AccentUi, 3.f);
				else
					HoverFill(p, mr, over);
				p.TextV(x, r.y, r.h, kMenus[i], st.openMenu == i ? NkRole::TextOnAccent : NkRole::Text);
				if (hit.Clicked(kMenuKeys[i]))
					st.openMenu = (st.openMenu == i) ? -1 : i; // deuxieme clic = referme
				x += w;
			}

			const float32 nw = p.TextW(projectName);
			p.TextV(r.w * 0.5f - nw * 0.5f, r.y, r.h, projectName, NkRole::TextMuted);

			const float32 bw = S(30.f), bh = S(22.f);
			const float32 by = r.y + (r.h - bh) * 0.5f;
			// L'ICONE DU BOUTON CENTRAL SUIT L'ETAT DE LA FENETRE : carre quand elle
			// peut etre agrandie, deux carres superposes quand elle peut etre
			// restauree. Garder le meme dessin dans les deux cas obligerait a se
			// souvenir de ce qu'on a fait pour savoir ce que le bouton va faire.
			const NkIcon kWin[3] = {NkIcon::WinMin, st.maximized ? NkIcon::WinRestore : NkIcon::WinMax,
									NkIcon::WinClose};
			static const char *const kWinKeys[3] = {"win.min", "win.max", "win.close"};
			for (int32 i = 0; i < 3; ++i) {
				const float32 bx = r.w - kPad - (float32)(3 - i) * (bw + S(6.f));
				const NkRect br{bx, by, bw, bh};
				const bool over = hit.Add(kWinKeys[i], br);
				const bool close = (i == 2);
				if (close)
					p.Fill(br, over ? NkColor{240, 100, 85, 255} : NkColor{231, 76, 60, 255}, 3.f);
				else
					p.Outline(br, NkRole::Border, over ? NkRole::PanelBg : NkRole::PanelHeader, 3.f);
				p.IconV(bx + (bw - S(13.f)) * 0.5f, by, bh, kWin[i],
						close ? NkRole::TextOnAccent : NkRole::Text, 13.f);
			}
			if (hit.Clicked("win.min"))
				st.wantMinimize = true;
			if (hit.Clicked("win.max"))
				st.wantMaxRestore = true;
			// La FERMETURE passe par la confirmation si le document est modifie.
			// Fermer directement ferait perdre le travail sur un clic mal place, et
			// c'est le genre d'accident qu'on ne pardonne pas a un logiciel.
			if (hit.Clicked("win.close"))
				NkRequestClose(st); // meme politique que la croix de l'OS et Quitter

			// â”€â”€ DEPLACEMENT DE LA FENETRE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			// La barre de titre est la poignee, mais seulement la ou elle est VIDE :
			// declarer la zone en PREMIER laisse les menus et les boutons, declares
			// ensuite, la recouvrir. C'est la regle Â« la derniere zone gagne Â» qui
			// fait le tri, sans qu'on ait a lister les exceptions.
			//
			// Le drapeau est pose ici et consomme par la boucle : BeginDragMove BLOQUE
			// (boucle modale de l'OS) et le rappeler pendant la peinture reentrerait
			// dans la frame.
			// On retient OU l'on a saisi la barre, en fraction de largeur : c'est ce
			// qui permet de replacer la fenetre sous le curseur si le glissement part
			// d'un etat maximise (cf. main.cpp).
			if (hit.Clicked("win.drag")) {
				st.wantDragMove = true;
				const float32 w = r.w > 1.f ? r.w : 1.f;
				float32 f = (hit.Mouse().x - r.x) / w;
				st.dragFracX = f < 0.f ? 0.f : (f > 1.f ? 1.f : f);
			}
		}

		// â”€â”€ ONGLETS DE DOCUMENT â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// â”€â”€ ONGLETS DE SCENE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// UNE SEULE scene a l'ouverture. Demarrer sur deux onglets vides ferait croire
		// que l'un d'eux contient quelque chose, et obligerait a en fermer un avant
		// meme d'avoir commence. Le nom se modifie au DOUBLE-clic, le + en ajoute une.
		// Un MODEL et ses MESH sont deux natures distinctes : le conteneur
		// s'appelle model, sa matiere reste des maillages. Le premier mesh
		// prend un nom independant -- renommer l'un ne renomme pas l'autre.
		inline void NkHierComposeName(NkModelerState &st, const char *base0, int32 newNode);
		inline int32 NkModelFirstMesh(NkModelerState &st, int32 root) {
			const int32 m = demo::Demo3DHostEnsureModelMesh(root);
			if (m >= 0 && m < 176 && st.customNames[m][0] == 0)
				NkHierComposeName(st, "Mesh", m);
			return m;
		}
		// ── MARQUER « MODIFIE » ─────────────────────────────────────────────────
		// POINT DE PASSAGE UNIQUE. L'etat vit a DEUX niveaux qui doivent rester
		// d'accord : la scene (pour le marqueur d'onglet et la protection a la
		// fermeture) et le projet (pour l'invite d'enregistrement en quittant).
		// Les poser separement, c'est garantir qu'un jour l'un des deux sera
		// oublie -- et un marqueur qui ment sur du travail non enregistre coute
		// exactement ce qu'il devait proteger.
		inline void NkMarkDirty(NkModelerState &st) {
			st.dirty = true;
			const int32 dD = st.TabDoc(st.activeTab);
			if (dD >= 0)
				st.docDirty[dD] = true;
			// L'ambiance suit : les objets utilisateur occludent le GI voxel, et
			// « la scene a change » est UNE seule notion -- la pastille et la
			// grille d'occlusion se rafraichissent sur le meme evenement.
			demo::Demo3DHostGIMarkDirty();
		}

		/// Apres un enregistrement REUSSI : plus rien n'est en attente.
		/// L'ARBRE a change : cartes creees, renommees, deplacees, supprimees.
		/// POINT DE PASSAGE UNIQUE, comme NkMarkDirty pour les documents.
		inline void NkMarkTreeDirty(NkModelerState &st) {
			st.treeDirty = true;
			st.dirty = true;
		}

		inline void NkClearDirty(NkModelerState &st) {
			st.dirty = false;
			st.treeDirty = false;
			for (int32 d = 0; d < NkModelerState::kMaxDocs; ++d)
				st.docDirty[d] = false;
		}

		/// Apres l'enregistrement d'UN SEUL document : lui seul redevient propre.
		/// Le projet reste « modifie » tant qu'un autre document l'est -- sinon
		/// l'invite de fermeture laisserait partir du travail non enregistre, ce
		/// qui est exactement ce que la pastille promet d'eviter.
		inline void NkClearDirtyDoc(NkModelerState &st, int32 d) {
			if (d >= 0 && d < NkModelerState::kMaxDocs)
				st.docDirty[d] = false;
			// L'ARBRE est reecrit a CHAQUE enregistrement, meme quand un seul
			// fichier est ecrit : il porte les chemins, et un fichier ecrit dont
			// l'arbre ignorerait le chemin serait introuvable a la reouverture.
			st.treeDirty = false;
			bool any = false;
			for (int32 i = 0; i < NkModelerState::kMaxDocs && !any; ++i)
				any = st.docUsed[i] && !st.docTransient[i] && st.docDirty[i];
			st.dirty = any;
		}

		/// Carte du navigateur que l'onglet actif ENREGISTRE. Une scene passe par
		/// son document, un editeur d'asset par l'asset qu'il montre : les deux
		/// repondent a la meme question -- « quel fichier suis-je en train de
		/// regarder ? ».
		inline int32 NkActiveCard(const NkModelerState &st) {
			if (st.activeTab < 0 || st.activeTab >= st.sceneCount)
				return -1;
			if (st.sceneTabKind[st.activeTab] != 0) {
				const int32 a = st.sceneTabAsset[st.activeTab] - 1;
				return (a >= 0 && a < st.browserCount) ? a : -1;
			}
			const int32 d = st.TabDoc(st.activeTab);
			if (d < 0)
				return -1;
			const int32 c = st.docCard[d] - 1;
			return (c >= 0 && c < st.browserCount) ? c : -1;
		}

		inline void NkStoreSceneView(NkModelerState &st, int32 tab) {
			const int32 d = st.TabDoc(tab);
			if (d < 0)
				return;
			float32 *cp = st.docCamPose[d];
			bool ortho = false;
			demo::Demo3DHostGetCameraPose(cp, &cp[3], &cp[4], &cp[5], &ortho);
			st.docCamOrtho[d] = ortho;
			st.docCamSet[d] = true;
			// La vue, c'est aussi ce qu'on y AFFICHE (Rihen, 10 aout) : ombrage,
			// surimpressions et fond partent avec le document, pas avec l'onglet.
			NkModelerState::NkDocView &v = st.docView[d];
			v.ombrage = st.shading;
			v.lumiereUnie = st.solidLight;
			v.surimpressions = st.overlayMask;
			v.fond = st.bgChoice;
			v.fondType = st.bgType;
			v.fondLum = st.bgBrightness;
			for (int32 a = 0; a < 3; ++a)
				v.fondPerso[a] = st.bgCustom[a];
			st.docViewSet[d] = true;
		}

		// ── FERMER UN ONGLET ────────────────────────────────────────────────────
		// FERMER UNE VUE NE SUPPRIME RIEN (regle absolue de Rihen, 8 aout 2026).
		// Le document reste dans la table du projet, sa carte reste dans le
		// navigateur, et un double-clic dessus le ROUVRE avec son contenu. Avant,
		// l'onglet ETAIT la scene : sa fermeture l'effacait de l'enregistrement
		// suivant, en laissant ses noeuds orphelins dans le fichier.
		//
		// SEULE EXCEPTION, qui n'en est pas une : un document TRANSITOIRE (maquette
		// d'editeur d'asset, onglet d'isolation) n'a jamais rien ete d'autre que la
		// vue elle-meme. L'asset qu'il montrait, lui, vit dans le navigateur.
		inline void NkActivateTab(NkModelerState &st, int32 tb, bool force);
		inline void NkCloseSceneTab(NkModelerState &st, int32 i) {
			if (i < 0 || i >= st.sceneCount || st.sceneCount <= 1)
				return;
			const int32 d = st.TabDoc(i);
			// LA VUE DE L'ONGLET ACTIF n'est rangee qu'a la bascule : sans ce
			// rangement, fermer l'onglet sur lequel on vient de travailler perdrait
			// le regard qu'on venait d'y poser -- et ses unites.
			if (d >= 0 && i == st.activeTab) {
				if (st.sceneTabKind[i] == 0)
					NkStoreSceneView(st, i);
				st.docUnitSys[d] = st.unitSystem;
				st.docUnitLen[d] = st.unitLength;
				st.docUnitScale[d] = st.unitScale;
			}
			// Le noeud edite QUITTE la vue AVANT qu'elle disparaisse -- ferme en
			// arriere-plan, son sous-arbre restait echoue dans un document mort,
			// donc introuvable. Un ASSET est rearchive (il n'a pas de scene ou
			// rentrer), un noeud ISOLE retourne dans la sienne.
			if (d >= 0 && st.docIsoNode[d] > 0) {
				if (st.docAssetEdit[d])
					demo::Demo3DHostArchiveTree(st.docIsoNode[d] - 1, true);
				else
					demo::Demo3DHostMoveTreeScene(st.docIsoNode[d] - 1,
												  (int32)st.docIsoHome[d]);
			}
			if (d >= 0 && st.docTransient[d])
				st.DocFree(d);
			for (int32 k = i; k + 1 < st.sceneCount; ++k) {
				st.sceneTabKind[k] = st.sceneTabKind[k + 1];
				st.sceneTabAsset[k] = st.sceneTabAsset[k + 1];
				st.sceneTabDoc[k] = st.sceneTabDoc[k + 1];
			}
			st.sceneCount--;
			st.sceneTabDoc[st.sceneCount] = -1;
			// L'ACTIF suit : ferme avant lui son index recule ; ferme LUI-MEME,
			// l'environnement du nouvel actif s'applique.
			const bool wasAct = (i == st.activeTab);
			if (i < st.activeTab)
				st.activeTab--;
			if (st.activeTab >= st.sceneCount)
				st.activeTab = st.sceneCount - 1;
			if (wasAct)
				NkActivateTab(st, st.activeTab, true);
		}

		// ── LE NAVIGATEUR REFLETE LES SCENES DU PROJET ──────────────────────────
		// Chaque document NON TRANSITOIRE obtient (ou met a jour) sa carte
		// « Scene » (kind 5) : c'est par son double-clic qu'on rouvre une scene
		// fermee (demande de Rihen : « on ne voit pas la scene sauvegardee dans le
		// navigateur »).
		//
		// On itere les DOCUMENTS, plus les onglets. Iterer les onglets ne donnait
		// de carte qu'aux scenes OUVERTES : les autres n'apparaissaient nulle part,
		// ce qui les rendait irrecuperables.
		inline void NkBrowserSyncScenes(NkModelerState &st) {
			for (int32 d = 0; d < NkModelerState::kMaxDocs; ++d) {
				if (!st.docUsed[d] || st.docTransient[d])
					continue;
				int32 e = st.docCard[d] - 1;
				if (e < 0 || e >= st.browserCount || st.browserKind[e] != 5 ||
					st.browserDoc[e] != d + 1) {
					if (st.browserCount >= NkModelerState::kMaxBrowser)
						continue; // navigateur plein : la scene reste sans carte
					e = st.browserCount++;
					st.browserKind[e] = 5;
					// A la RACINE : previsible tant que le selecteur de dossier
					// personnalise n'existe pas (chantier suivant) ; la carte se
					// range ensuite par glisser-deposer comme les autres.
					st.browserParent[e] = -1;
					st.browserSrcNode[e] = 0;
					st.browserSub[e] = 0;
					st.browserDoc[e] = d + 1;
					st.docCard[d] = e + 1;
				}
				NkWidgetState::Copy(st.browserNames[e], st.docName[d], 31u);
			}
		}
		// ── ACTIVER UN ONGLET ───────────────────────────────────────────────────
		// Un onglet EDITEUR est une SCENE A PART ENTIERE (Rihen) : la vue se vide
		// et une MAQUETTE de l'asset nait a l'origine, editable avec les memes
		// outils. En quittant l'editeur, la maquette disparait.
		inline void NkActivateTab(NkModelerState &st, int32 tb, bool force = false) {
			if (tb < 0 || tb >= st.sceneCount || (tb == st.activeTab && !force))
				return;
			const int32 dOld = st.TabDoc(st.activeTab);
			const int32 d = st.TabDoc(tb);
			if (d < 0)
				return; // onglet sans document : il n'y a rien a montrer
			if (st.sceneTabKind[st.activeTab] == 0 && tb != st.activeTab)
				NkStoreSceneView(st, st.activeTab); // la scene quittee garde sa vue
			// Quitter une vue d'edition : l'ASSET est rearchive, le noeud ISOLE
			// rentre dans sa scene. Meme regle qu'a la fermeture de l'onglet --
			// c'est le meme geste vu de deux endroits.
			if (dOld >= 0 && st.docIsoNode[dOld] > 0 && tb != st.activeTab) {
				if (st.docAssetEdit[dOld])
					demo::Demo3DHostArchiveTree(st.docIsoNode[dOld] - 1, true);
				else
					demo::Demo3DHostMoveTreeScene(st.docIsoNode[dOld] - 1,
												  (int32)st.docIsoHome[dOld]);
			}
			// CHAQUE SCENE A SES PROPRIETES : celles du quitte sont rangees.
			if (dOld >= 0) {
				st.docUnitSys[dOld] = st.unitSystem;
				st.docUnitLen[dOld] = st.unitLength;
				st.docUnitScale[dOld] = st.unitScale;
			}
			// LES REGLAGES RENDU AUSSI (par scene, Rihen 10 aout) : requete
			// DIFFEREE — le gestionnaire de projet capture l'etat du document
			// quitte puis applique l'instantane de l'active (l'etat vivant est
			// global a la vue, il n'a pas encore bouge au moment du differe).
			// Seules les SCENES possedent ces reglages, pas les editeurs d'asset.
			if (st.sceneTabKind[st.activeTab] == 0 && dOld >= 0)
				st.renduSwitchFrom = dOld;
			if (st.sceneTabKind[tb] == 0)
				st.renduSwitchTo = d;
			if (st.editPreviewNode > 0) {
				demo::Demo3DHostDeleteNode(st.editPreviewNode - 1, true);
				st.editPreviewNode = 0;
			}
			st.activeTab = tb;
			// BASCULE DE DOCUMENT : l'hote ne rend et ne liste plus que les
			// noeuds de CE document ; la selection ne traverse jamais.
			demo::Demo3DHostSetActiveScene((int32)st.docScene[d]);
			// L'hote doit savoir s'il sert un MODEL : la selection en vue 3D
			// n'y a pas la meme regle (mesh par mesh, contre model entier).
			demo::Demo3DHostSetDocIsModel(st.sceneTabKind[tb] == 7);
			demo::Demo3DHostDeselectAll();
			st.unitSystem = st.docUnitSys[d];
			st.unitLength = st.docUnitLen[d];
			st.unitScale = st.docUnitScale[d];
			if (st.unitScale < 0.001f)
				st.unitScale = 1.f; // document jamais visite : valeurs par defaut
			const uint8 tk = st.sceneTabKind[tb];
			if (tk == 0) {
				if (st.docCamSet[d])
					demo::Demo3DHostSetCameraPose(st.docCamPose[d], st.docCamPose[d][3],
												  st.docCamPose[d][4], st.docCamPose[d][5],
												  st.docCamOrtho[d]);
				else
					demo::Demo3DHostResetView();
				// L'AFFICHAGE de la scene revient avec elle : la synchronisation de
				// main.cpp poussera vers l'hote ce qui a change. Un document jamais
				// visite recoit les defauts d'ouverture — pas l'affichage de la
				// scene qu'on quitte.
				{
					const NkModelerState::NkDocView v =
						st.docViewSet[d] ? st.docView[d] : NkModelerState::NkDocView{};
					st.shading = v.ombrage;
					st.solidLight = v.lumiereUnie;
					st.overlayMask = v.surimpressions;
					st.bgChoice = v.fond;
					st.bgType = v.fondType;
					st.bgBrightness = v.fondLum;
					for (int32 a = 0; a < 3; ++a)
						st.bgCustom[a] = v.fondPerso[a];
				}
				return; // l'appartenance filtre deja les objets de la scene
			}
			// EDITEUR : scene VIDE + l'asset lui-meme.
			demo::Demo3DHostResetView(); // document neuf : vue d'ouverture
			const uint8 ek = (uint8)(tk - 1);
			const int32 ai = st.sceneTabAsset[tb] - 1;
			// ── L'ONGLET EDITE LE NOEUD LUI-MEME, JAMAIS UNE COPIE ──────────
			// Deux chemins y menent et ils partagent tout sauf la sortie :
			//   * ISOLATION -- le noeud est un objet de SCENE, il y retourne ;
			//   * ASSET du navigateur -- le noeud est une ARCHIVE, desarchivee le
			//     temps de la vue et rearchivee en sortant.
			// L'editeur d'asset travaillait auparavant sur un DUPLICATA detruit a
			// la fermeture : tout ce qu'on y faisait etait perdu, position comprise
			// (constate par Rihen). Un editeur qui n'edite rien est pire que pas
			// d'editeur du tout.
			if (st.docIsoNode[d] == 0 && ek == 6 && ai >= 0 && ai < st.browserCount) {
				int32 src = st.browserSrcNode[ai] - 1;
				if (src < 0) {
					// Carte creee par « + Model » : elle n'a pas encore de corps.
					// Il nait ICI et devient LE corps de la carte -- sinon chaque
					// ouverture repartait d'un cube neuf.
					src = demo::Demo3DHostAddNode(2, 0);
					if (src >= 0)
						st.browserSrcNode[ai] = src + 1;
				}
				if (src >= 0) {
					demo::Demo3DHostArchiveTree(src, false);
					st.docIsoNode[d] = src + 1;
					st.docAssetEdit[d] = true;
				}
			}
			if (st.docIsoNode[d] > 0) {
				const int32 iso = st.docIsoNode[d] - 1;
				// REVENIR sur l'onglet : l'asset a ete rearchive en le quittant,
				// il faut le remettre dans la vue. Sans cela, rouvrir l'editeur
				// d'un Model montrait une scene vide.
				if (st.docAssetEdit[d])
					demo::Demo3DHostArchiveTree(iso, false);
				demo::Demo3DHostMoveTreeScene(iso, (int32)st.docScene[d]);
				// Le seul parent d'un model est le model : ses maillages
				// reviennent tous a plat sous lui (Rihen).
				demo::Demo3DHostFlattenModel(iso);
				NkModelFirstMesh(st, iso); // le model et sa matiere
				demo::Demo3DHostSelectEmptyNode(iso);
				st.editPreviewNode = 0;
				return;
			}
			// Les autres natures d'asset (materiau, texture, graphe...) n'ont pas
			// encore de corps editable : leur onglet reste une scene vide plutot
			// qu'une maquette qui ferait croire qu'on edite quelque chose.
			st.editPreviewNode = 0;
		}
		inline void PaintTabsI(NkModelerPainter &p, const NkRect &r, NkModelerState &st,
							   NkHitRegistry &hit, NkWidgetState &ws, const nkgui::NkGuiInput &in) {
			p.Fill(r, NkRole::PanelBg);
			p.HLine(r.x, r.y + r.h - 1.f, r.w);
			float32 x = S(10.f);
			const float32 h = r.h - 2.f;
			char key[32];
			for (int32 i = 0; i < st.sceneCount; ++i) {
				const int32 di = st.TabDoc(i);
				if (di < 0)
					continue; // onglet sans document : anomalie, on ne peint rien
				const float32 tw = p.TextW(st.docName[di]) + S(44.f);
				const NkRect tr{x, r.y + 2.f, tw, h};
				snprintf(key, sizeof(key), "tab.%d", i);
				const bool over = hit.Add(key, tr);
				const bool on = (i == st.activeTab);
				p.Fill(tr, on ? NkRole::PanelHeader : (over ? NkRole::PanelBg : NkRole::InputBg), 3.f);
				if (st.sceneTabKind[i] != 0) {
					// Liseret de NATURE : distinguer d'un oeil les onglets EDITEUR
					// des scenes. La couleur vient du MEME point de passage que les
					// cartes du navigateur -- c'est la meme nature vue a deux
					// endroits, elle ne doit pas pouvoir en avoir deux couleurs.
					const uint8 k2 = (uint8)(st.sceneTabKind[i] - 1);
					const int32 aT = st.sceneTabAsset[i] - 1;
					const uint8 sT = (aT >= 0 && aT < st.browserCount) ? st.browserSub[aT] : 0;
					p.Fill({tr.x, tr.y, tr.w, 2.f}, NkAssetColor(p, k2, sT)); // en HAUT (Rihen)
				}
				snprintf(key, sizeof(key), "tab.name.%d", i);
				if (EditableText(p, hit, ws, in, key, {x + S(10.f), r.y, tw - S(32.f), r.h},
								 st.docName[di], on ? NkRole::Text : NkRole::TextMuted,
								 st.docName[di], 32u)) {
					// RENOMMER L'ONGLET RENOMME SA CARTE, TOUT DE SUITE. Le nom ne
					// se propageait qu'a l'enregistrement (via NkBrowserSyncScenes),
					// si bien que le navigateur affichait l'ancien nom entre-temps
					// (constate par Rihen). Symetrique du renommage depuis la carte :
					// chaque sens agit A LA VALIDATION, jamais en continu -- une
					// recopie a chaque frame ferait qu'un cote ecraserait l'autre.
					const int32 e9 = st.docCard[di] - 1;
					if (st.sceneTabKind[i] == 0 && e9 >= 0 && e9 < st.browserCount &&
						st.browserKind[e9] == 5)
						NkWidgetState::Copy(st.browserNames[e9], st.docName[di], 31u);
				}
				// PASTILLE ET CROIX PARTAGENT LE MEME EMPLACEMENT, mais leurs
				// conditions different -- et c'est important : la pastille « non
				// enregistre » vaut pour TOUTE scene, y compris la DERNIERE, alors
				// que la croix n'apparait que s'il reste plus d'une scene (fermer
				// la derniere laisserait l'application sans document). L'ancienne
				// imbrication mettait la pastille SOUS la condition de la croix :
				// avec une seule scene -- le cas le plus courant -- aucune
				// modification n'etait jamais signalee (constate par Rihen).
				{
					snprintf(key, sizeof(key), "tab.close.%d", i);
					const NkRect cr{x + tw - S(24.f), r.y + 2.f, S(20.f), h};
					const bool canClose = st.sceneCount > 1;
					const bool overClose = canClose && hit.Add(key, cr);
					// MARQUEUR « NON ENREGISTRE » (Rihen) : une PASTILLE prend la
					// place de la croix tant que la souris n'est pas dessus. MEME
					// emplacement, donc la largeur de l'onglet ne bouge pas quand une
					// scene devient modifiee -- des onglets qui changent de taille a
					// la premiere frappe rendraient la barre illisible. Au survol la
					// croix revient : on ferme sans avoir a viser ailleurs.
					if (st.docDirty[di] && !overClose) {
						const float32 d = S(7.f);
						p.Fill({cr.x + (cr.w - d) * 0.5f, cr.y + (cr.h - d) * 0.5f, d, d},
							   on ? NkRole::Text : NkRole::TextMuted, d * 0.5f);
					} else if (canClose) {
						HoverFill(p, cr, overClose, 2.f);
						p.IconV(x + tw - S(20.f), r.y, r.h, NkIcon::WinClose, NkRole::TextMuted,
								10.f);
					}
					if (canClose && hit.Clicked(key)) {
						// LA CROIX FERME, SANS RIEN DEMANDER. Il n'y a plus rien a
						// proteger : le document reste dans le projet et sa carte dans
						// le navigateur. L'ancienne boite « Fermer sans enregistrer »
						// disait vrai a l'epoque ou l'onglet ETAIT la scene ; la
						// garder maintenant ferait redouter une perte qui n'existe
						// plus. La pastille reste : elle dit que le PROJET n'est pas
						// enregistre, ce qui est toujours exact.
						NkCloseSceneTab(st, i);
						break; // la liste a change
					}
				}
				snprintf(key, sizeof(key), "tab.%d", i);
				{
					// Le clic sur le NOM bascule AUSSI l'onglet : la zone du nom
					// recouvre celle de l'onglet et lui VOLAIT le survol --
					// selectionner une scene exigeait de viser les bords (Rihen).
					char nk2[32];
					snprintf(nk2, sizeof(nk2), "tab.name.%d", i);
					if (hit.Clicked(key) || (!ws.IsEditing(nk2) && hit.Clicked(nk2)))
						NkActivateTab(st, i);
				}
				x += tw + 3.f;
				// MARQUEUR DE SEPARATION entre en-tetes d'onglets (regle de
				// Rihen) : sans lui, deux scenes cote a cote se confondaient.
				if (i + 1 < st.sceneCount) {
					p.VLine(x + 1.f, r.y + S(6.f), h - S(8.f));
					x += S(5.f);
				}
			}
			const NkRect ar{x + S(4.f), r.y + 2.f, S(24.f), h};
			HoverFill(p, ar, hit.Add("tab.add", ar));
			p.IconV(x + S(8.f), r.y, r.h, NkIcon::Add, NkRole::Text, 12.f);
			if (hit.Clicked("tab.add") && st.sceneCount < 8) {
				// UN NOUVEAU DOCUMENT, puis une vue dessus. Le nom par defaut est
				// NUMEROTE d'apres le nombre de documents et non d'onglets : numeroter
				// par onglet redonnait « Scene_2 » a une scene creee apres en avoir
				// ferme une -- deux scenes homonymes dans le meme projet.
				const int32 nd = st.DocAlloc();
				if (nd >= 0) {
					int32 used = 0;
					for (int32 q = 0; q < NkModelerState::kMaxDocs; ++q)
						if (st.docUsed[q] && !st.docTransient[q])
							++used;
					snprintf(st.docName[nd], 32, "Scene_%d", (int)used);
					// Une scene NEUVE nait VIERGE : les objets de la demo
					// appartiennent a la premiere scene.
					st.docBlank[nd] = true;
					st.docScene[nd] = (uint8)st.sceneIdNext++;
					const int32 nt = st.sceneCount++;
					st.sceneTabKind[nt] = 0;
					st.sceneTabAsset[nt] = 0;
					st.sceneTabDoc[nt] = nd;
					// LA CARTE NAIT AVEC LA SCENE, pas a l'enregistrement : une scene
					// qu'on ferme avant d'avoir enregistre doit rester retrouvable.
					NkBrowserSyncScenes(st);
					NkActivateTab(st, nt);
				}
			}
		}

		// â”€â”€ BARRE D'OUTILS â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// LE MODE EST UN DEROULANT, PLUS UN COMMUTATEUR A DEUX ETATS. Rihen a
		// remarque que Â« Objet Â» et Â« Edition Â» faisaient doublon avec Â« Mode de
		// selection Â» -- et il a raison sur le fond : ce ne sont pas deux boutons,
		// c'est UNE liste de modes, qui va s'allonger (sculpt 2.5D, sculpt reel,
		// texturing, riggging...). Deux boutons cotes a cote auraient cesse de tenir
		// au troisieme mode.
		// Â« Mode de selection Â» reste a cote, et ne fait PAS doublon : il porte le
		// sous-mode sommet / arete / face, qui n'a de sens qu'EN edition.
		inline void PaintToolbar(NkModelerPainter &p, const NkRect &r, NkModelerState &st,
								 NkHitRegistry &hit, NkWidgetState &ws, NkComboPending &combo) {
			p.Fill(r, NkRole::PanelHeader);
			p.HLine(r.x, r.y + r.h - 1.f, r.w);
			// Editeurs sans design defini : pas de barre d'outils (Rihen).
			if (st.sceneTabKind[st.activeTab] != 0 &&
				st.sceneTabKind[st.activeTab] != 7)
				return;
			float32 x = kPad;
			const float32 ih = S(14.f);

			// Bouton icone + libelle, avec sa zone sensible. Une seule fonction pour
			// que tous reagissent pareil.
			// L'AIDE PASSE PAR LE MEME POINT QUE LE BOUTON. Un seul endroit la
			// declare pour toute la barre : un bouton ajoute plus tard ne peut pas
			// « oublier » son infobulle, il lui manquerait juste son texte.
			auto btn = [&](const char *key, NkIcon ic, const char *label,
						   const char *tip = nullptr) -> bool {
				const float32 w = ih + S(5.f) + p.TextW(label) + S(14.f);
				const NkRect br{x - S(7.f), r.y + S(5.f), w, r.h - S(10.f)};
				const bool over = hit.Add(key, br);
				NkHelp(over, tip);
				HoverFill(p, br, over);
				p.IconV(x, r.y, r.h, ic, NkRole::Text, 14.f);
				p.TextV(x + ih + S(5.f), r.y, r.h, label);
				x += w;
				return hit.Clicked(key);
			};

			// Le retour du bouton etait JETE : « Enregistrer » peignait son icone
			// et ne faisait RIEN (constate par Rihen). Il passe par le meme
			// differe que le menu Fichier -- une seule voie d'enregistrement.
			//
			// LE BOUTON EST AUSSI UN SIGNAL (demande de Rihen, facon Unreal) :
			// tant qu'il existe du travail non enregistre, il s'affiche en ACCENT
			// -- l'oeil le voit sans chercher la pastille de l'onglet. Peint
			// AVANT le btn() pour que le survol garde son retour visuel.
			if (st.dirty) {
				const float32 wS = ih + S(5.f) + p.TextW("Enregistrer") + S(14.f);
				p.Fill({x - S(7.f), r.y + S(5.f), wS, r.h - S(10.f)}, NkRole::AccentUi, 3.f);
				if (btn("tb.save", NkIcon::Save, "Enregistrer",
						"Enregistrer le projet et tous ses fichiers (Ctrl+S)"))
					st.projPending = 3;
			} else {
				// RIEN A ENREGISTRER = BOUTON INERTE (Rihen, 10 aout) : pas de
				// zone cliquable, icone et libelle en sourdine — l'etat se lit
				// d'un coup d'oeil, et un clic sans effet n'existe plus.
				const float32 w = ih + S(5.f) + p.TextW("Enregistrer") + S(14.f);
				p.IconV(x, r.y, r.h, NkIcon::Save, NkRole::TextMuted, 14.f);
				p.TextV(x + ih + S(5.f), r.y, r.h, "Enregistrer", NkRole::TextMuted);
				x += w;
			}
			p.VLine(x - S(4.f), r.y + S(7.f), r.h - S(14.f));
			x += S(6.f);

			// â”€â”€ DEROULANT DE MODE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			// Le deroulant a QUITTE la barre : les ESPACES au-dessus de la vue
			// portent desormais Objet / Edition / Sculpture 2.5D / Sculpture /
			// Texturing -- un seul endroit pour changer de mode (regle de
			// Rihen, le doublon barre + onglets pretait a confusion).
			const float32 cbH = S(22.f), cbY = r.y + (r.h - cbH) * 0.5f;

			// LE Â« MODE DE SELECTION Â» A ETE RETIRE D ICI. Rihen a demande a quoi
			// servait le Â« Face Â» a cote d Â« Objet Â» : c etait le sous-mode
			// sommet/arete/face, exactement la meme chose que les trois boutons de la
			// barre de la vue. Un doublon que j avais introduit sans le voir.
			// Il reste dans la VUE, ou il est a sa place : c est la qu on selectionne,
			// et c est la qu on doit pouvoir changer de sous-mode sans traverser
			// l ecran.

			// AJOUTER et MODIFICATEUR sont des LISTES, pas des boutons : ils ouvrent un
			// choix. Un bouton simple laisserait croire a une action immediate.
			{
				// Â« Ajouter Â» OUVRE LE MENU PAR CATEGORIES, il ne retient pas de
				// Â« primitive courante Â» : ajouter est une action, pas un reglage.
				const NkRect ar{x, cbY, S(96.f), cbH};
				const bool over = hit.Add("tb.addmenu", ar);
				NkHelp(over, "Ajouter un objet a la scene : primitive, lumiere, camera...");
				const bool open = ws.ComboOpen("tb.addmenu");
				// ENCADRE comme tous les deroulants de la barre : sans cadre il se
				// lisait comme une etiquette, pas comme une commande.
				p.Outline(ar, (over || open) ? NkRole::AccentUi : NkRole::Border,
						  open ? NkRole::AccentUi : NkRole::InputBg, 3.f);
				const NkRole fg = open ? NkRole::TextOnAccent : NkRole::Text;
				p.IconV(ar.x + S(8.f), cbY, cbH, NkIcon::Add, fg, 13.f);
				p.TextV(ar.x + S(26.f), cbY, cbH, "Ajouter", fg);
				p.IconV(ar.x + ar.w - S(16.f), cbY, cbH,
						open ? NkIcon::ChevronUp : NkIcon::ChevronDown, fg, 11.f);
				if (hit.Clicked("tb.addmenu")) {
					ws.ToggleCombo("tb.addmenu");
					st.addParentNode = -1; // depuis la barre : naissance a la racine
					st.addAnchor = ar; // le menu est peint apres tout le reste
				}
				x += S(104.f);
			}
			// (Le deroulant MODIFICATEUR a quitte la barre lui aussi : la
			// pastille Modificateur du panneau Proprietes porte deja l'ajout --
			// un seul endroit, regle de Rihen.)

			// Â« Ajouter Â» et Â« Modificateur Â» etaient ecrits DEUX FOIS : une fois en
			// deroulant (ci-dessus, celui qui marche) et une fois en bouton plat ici.
			// Le doublon est retire -- deux commandes identiques cote a cote font
			// douter qu'elles fassent la meme chose.

			// Reglages cale a DROITE : action de session et non de modelisation, la
			// distance visuelle dit cette difference de nature.
			{
				const float32 sw = ih + S(5.f) + p.TextW("Reglages");
				const NkRect br{r.w - kPad - sw - S(7.f), r.y + S(5.f), sw + S(14.f), r.h - S(10.f)};
				HoverFill(p, br, hit.Add("tb.settings", br));
				p.IconV(r.w - kPad - sw, r.y, r.h, NkIcon::Gear, NkRole::Text, 14.f);
				p.TextV(r.w - kPad - sw + ih + S(5.f), r.y, r.h, "Reglages");
			}
		}

		// â”€â”€ EN-TETE DE PANNEAU â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Onglet + croix, comme la maquette. Rendu ici une seule fois : quatre
		// panneaux le partagent, et il n'y a donc qu'un endroit a corriger.
		// La croix de l'en-tete REFERME LE PANNEAU. Elle etait dessinee mais morte,
		// et c'est justement la contrepartie de la largeur minimale : puisqu'on ne
		// peut plus reduire un panneau jusqu'a le faire disparaitre, il faut un geste
		// franc pour le fermer -- et une poignee visible pour le rouvrir.
		//
		// `show` peut etre nul : certains en-tetes (l'onglet d'un panneau qui ne se
		// ferme pas) gardent la croix desactivee plutot que de la faire disparaitre,
		// pour que la rangee d'en-tetes reste alignee.
		inline float32 PaintPanelTab(NkModelerPainter &p, const NkRect &r, const char *title,
									 NkHitRegistry *hit = nullptr, bool *show = nullptr,
									 const char *key = nullptr,
									 // FLECHE de repli, pas une croix : la croix disait
									 // « fermer », le geste est « replier » (Rihen).
									 NkIcon closeIcon = NkIcon::ChevronRight) {
			const float32 h = 26.f;
			p.Fill({r.x, r.y, r.w, h}, NkRole::PanelHeader);
			p.TextV(r.x + kPad, r.y, h, title);
			const NkRect cr{r.x + r.w - 24.f, r.y + 3.f, 20.f, h - 6.f};
			bool over = false;
			if (hit && show && key) {
				over = hit->Add(key, cr);
				if (over)
					p.Fill(cr, NkRole::AccentUi, 2.f);
				if (hit->Clicked(key))
					*show = false;
			}
			p.IconV(r.x + r.w - 20.f, r.y, h, closeIcon,
					over ? NkRole::TextOnAccent : NkRole::Text, 11.f);
			p.HLine(r.x, r.y + h - 1.f, r.w);
			return r.y + h;
		}

		// â”€â”€ POIGNEE DE REOUVERTURE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Un panneau ferme doit laisser une trace : sans elle, l'utilisateur qui a
		// clique la croix n'a plus aucun moyen de deviner comment revenir en arriere,
		// et le menu n'est pas un moyen de DEVINER -- c'est un moyen de retrouver ce
		// qu'on sait deja exister.
		// Le chevron pointe vers l'endroit ou le panneau reapparaitra.
		inline void PaintPanelHandle(NkModelerPainter &p, const NkRect &r, NkHitRegistry &hit,
									 const char *key, bool &show, NkIcon icon) {
			if (r.w <= 0.f || r.h <= 0.f)
				return;
			const bool over = hit.Add(key, r);
			p.Fill(r, over ? NkRole::AccentUi : NkRole::PanelHeader);
			if (r.w > r.h) {
				p.HLine(r.x, r.y, r.w);
				p.IconV(r.x + r.w * 0.5f - 7.f, r.y, r.h, icon,
						over ? NkRole::TextOnAccent : NkRole::TextMuted, 12.f);
			} else {
				p.VLine(r.x + (r.x < 4.f ? r.w - 1.f : 0.f), r.y, r.h);
				p.IconV(r.x + r.w * 0.5f - 7.f, r.y + r.h * 0.5f - 13.f, 26.f, icon,
						over ? NkRole::TextOnAccent : NkRole::TextMuted, 12.f);
			}
			if (over)
				hit.WantCursor(NkCursorWant::Hand);
			if (hit.Clicked(key))
				show = true;
		}

		// Champ de recherche, avec sa loupe. Sans l'icone, un champ vide au texte
		// grise se confond avec une etiquette desactivee.
		// CHAMP DE RECHERCHE REEL : bordure, saisie, effacement, et un filtre que
		// l'appelant applique. L'ancien etait un dessin -- une boite grise avec le
		// mot Â« Rechercher Â» -- qui ne recevait aucun clic et ne filtrait rien.
		inline float32 PaintSearch(NkModelerPainter &p, const NkRect &r, float32 y,
								   NkHitRegistry &hit, NkWidgetState &ws,
								   const nkgui::NkGuiInput &in, const char *key, char *buf) {
			const float32 h = 22.f;
			const NkRect fr{r.x + 6.f, y + 4.f, r.w - 12.f, h};
			// BORDURE : sans elle, le champ se confond avec le fond du panneau et
			// rien ne dit qu'on peut y ecrire.
			const bool editing = ws.IsEditing(key);
			p.Outline(fr, editing ? NkRole::AccentUi : NkRole::Border, NkRole::InputBg, 3.f);
			p.IconV(fr.x + 6.f, fr.y, h, NkIcon::Search, NkRole::TextMuted, 12.f);
			const NkRect tr{fr.x + 24.f, fr.y, fr.w - 48.f, h};
			if (editing) {
				p.TextV(tr.x, tr.y, h, ws.editBuf);
				const float32 cw = p.TextW(ws.editBuf);
				p.Fill({tr.x + cw + 1.f, tr.y + 3.f, 1.f, h - 6.f}, NkRole::Text);
				for (int32 i = 0; i < in.charCount; ++i) {
					const uint32 c = in.chars[i];
					if (c >= 32u && c < 127u && ws.editLen < 30u) {
						ws.editBuf[ws.editLen++] = (char)c;
						ws.editBuf[ws.editLen] = 0;
					}
				}
				if (in.KeyPressed(nkgui::NkGuiKey::Backspace) && ws.editLen > 0)
					ws.editBuf[--ws.editLen] = 0;
				// La recherche s'applique A CHAQUE FRAPPE : attendre Entree pour
				// filtrer une liste n'aurait aucun sens, on cherche justement en
				// voyant le resultat se reduire.
				NkWidgetState::Copy(buf, ws.editBuf, 31u);
				hit.Add(key, fr);
				if (in.KeyPressed(nkgui::NkGuiKey::Enter)
					|| in.KeyPressed(nkgui::NkGuiKey::Escape)
					|| (hit.AnyClick() && !hit.IsHovered(key)))
					ws.EndEdit();
			} else {
				const bool over = hit.Add(key, fr);
				if (over)
					hit.WantCursor(NkCursorWant::Hand);
				if (*buf)
					p.TextV(tr.x, tr.y, h, buf, NkRole::Text);
				else
					p.TextV(tr.x, tr.y, h, "Rechercher", NkRole::TextMuted);
				// SIMPLE clic, pas double : un champ de recherche s'ouvre au premier
				// clic. Le double-clic est reserve au renommage, ou il protege d'un
				// renommage accidentel -- ici il n'y a rien a proteger.
				if (hit.Clicked(key))
					ws.BeginEdit(key, buf);
			}
			// Croix d'effacement, seulement s'il y a quelque chose a effacer.
			if (*buf) {
				char ck[48];
				snprintf(ck, sizeof(ck), "%s.clear", key);
				const NkRect cr{fr.x + fr.w - 22.f, fr.y + 3.f, 18.f, h - 6.f};
				HoverFill(p, cr, hit.Add(ck, cr), 2.f);
				p.IconV(cr.x + 3.f, cr.y, cr.h, NkIcon::WinClose, NkRole::TextMuted, 10.f);
				if (hit.Clicked(ck)) {
					buf[0] = 0;
					if (editing)
						ws.EndEdit();
				}
			}
			return y + h + 8.f;
		}

		// Filtre insensible a la casse : Â« cu Â» trouve Â« Cube Â». Un filtre sensible
		// obligerait a connaitre la casse exacte de ce qu'on cherche, ce qui est
		// exactement ce qu'on ne sait pas quand on cherche.
		inline bool NkNameMatches(const char *name, const char *filter) {
			if (!filter || !*filter)
				return true;
			if (!name)
				return false;
			for (const char *a = name; *a; ++a) {
				const char *x = a;
				const char *y = filter;
				while (*x && *y) {
					char cx = *x, cy = *y;
					if (cx >= 'A' && cx <= 'Z')
						cx = (char)(cx - 'A' + 'a');
					if (cy >= 'A' && cy <= 'Z')
						cy = (char)(cy - 'A' + 'a');
					if (cx != cy)
						break;
					++x;
					++y;
				}
				if (!*y)
					return true;
			}
			return false;
		}

		// â”€â”€ HIERARCHIE (gauche) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Arbre REPLIABLE, noms MODIFIABLES, deux colonnes d'etat (oeil, cadenas), et
		// un clic dans le VIDE qui deselectionne.
		//
		// Ce dernier point compte plus qu'il n'en a l'air : sans lui, une fois un
		// objet selectionne on ne peut plus revenir a Â« rien de selectionne Â» sans
		// passer par un menu. Or Â« rien Â» est un etat legitime -- c'est celui ou les
		// commandes de scene s'appliquent.
		// La hierarchie liste LA SCENE, plus des lignes inventees. Chaque ligne est
		// un slot du viewport : les indices sont stables (tableau a trous), donc les
		// cles de zones cliquables restent valides d'une image a l'autre.
		inline NkIcon NkObjectIcon(int32 type) {
			switch (type) {
				case nk3d::kVpObjLightPoint:
				case nk3d::kVpObjLightSun:
				case nk3d::kVpObjLightSpot:
					return NkIcon::Light;
				case nk3d::kVpObjCamera:
					return NkIcon::Camera;
				case nk3d::kVpObjEmpty:
					return NkIcon::Gizmo;
				default:
					return NkIcon::Mesh;
			}
		}

		inline const char *NkObjectTypeName(int32 type) {
			switch (type) {
				case nk3d::kVpObjLightPoint:
				case nk3d::kVpObjLightSun:
				case nk3d::kVpObjLightSpot:
					return "Lumiere";
				case nk3d::kVpObjCamera:
					return "Camera";
				case nk3d::kVpObjEmpty:
					return "Repere";
				default:
					return "Maillage";
			}
		}

		// â”€â”€ BARRE DE DEFILEMENT SAISISSABLE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// La molette marchait ; la barre n'etait qu'un DESSIN (p.VScroll) --
		// Â« le scrollbar n'est pas fonctionnel Â» (Rihen). Le pouce suit la
		// souris tant que le bouton reste enfonce, meme hors de la glissiere :
		// le geste appartient a la barre ou il a commence (propDragKey).
		inline void NkScrollDrag(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
								 const char *key, const NkRect &area, float32 contentH,
								 float32 &offset) {
			if (contentH > area.h && area.h > 0.f) {
				// Le DESSIN reste colle au bord droit ; seule la ZONE DE SAISIE
				// s'arrete 4 px avant lui -- les SPLITTERS de panneau, declares
				// apres tout le reste, possedent ces derniers pixels et voleraient
				// le clic. Plus large que le dessin (6 px) : une cible de 6 px se
				// rate.
				const NkRect track{area.x + area.w - S(16.f), area.y, S(12.f), area.h};
				hit.Add(key, track);
				const bool mine = (strcmp(st.propDragKey, key) == 0);
				if (hit.MouseDown() && (hit.IsHovered(key) || mine)) {
					if (!st.propDragKey[0] && hit.IsHovered(key))
						snprintf(st.propDragKey, sizeof(st.propDragKey), "%s", key);
					if (strcmp(st.propDragKey, key) == 0) {
						float32 th = area.h * (area.h / contentH);
						if (th < 24.f)
							th = 24.f;
						float32 t = (hit.Mouse().y - area.y - th * 0.5f) / (area.h - th);
						if (t < 0.f)
							t = 0.f;
						if (t > 1.f)
							t = 1.f;
						offset = t * (contentH - area.h);
					}
				} else if (mine && !hit.MouseDown()) {
					st.propDragKey[0] = 0;
				}
			}
			p.VScroll(area, contentH, offset);
		}

		// ── LA SEULE BARRE DE DEFILEMENT DE L'APPLICATION ──────────────────────
		// Celle de NKEditorKit, la meme que l'editeur de code et que le panneau
		// des proprietes (Rihen : la hierarchie et les deux cotes du navigateur
		// doivent lui ressembler). La gouttiere est prise DANS la zone, a droite :
		// une barre posee par-dessus le contenu masquerait la derniere colonne.
		// Renvoie la zone utile restante, celle qu'il faut clipper et peindre.
		inline NkRect NkPaintVScroll(NkModelerPainter &p, nkgui::NkGuiContext *guiCtx,
									 const NkRect &area, float32 contentH, float32 &scroll,
									 uint32 id) {
			const float32 sbW = editorkit::NkScrollbarWidth();
			if (area.w <= sbW * 2.f || area.h <= sbW * 2.f)
				return area;
			const NkRect track{area.x + area.w - sbW, area.y, sbW, area.h};
			if (guiCtx) {
				// LE HARNAIS ET LES FLECHES, PLUS SOMBRES (Rihen). Le skin
				// utilisateur de NKEditorKit existe pour ca : on le pose une fois,
				// et TOUTES les barres de l'application suivent -- y compris
				// celle des proprietes, qui doit rester identique aux autres.
				auto &sk = editorkit::NkScrollbarUserSkin();
				if (!sk.custom) {
					sk.custom = true;
					sk.colors.track = NkColor{0, 0, 0, 46};
					sk.colors.thumb = NkColor{44, 49, 58, 255};
					sk.colors.thumbHover = NkColor{62, 69, 80, 255};
					sk.colors.arrowHover = NkColor{26, 30, 38, 255};
				}
				// LE FOND DE LA GOUTTIERE EST OPAQUE : la barre de NKEditorKit
				// peint une piste translucide, si bien que les lignes de selection
				// peintes avant elle transparaissaient au travers (Rihen).
				p.Fill(track, NkRole::PanelBg);
				editorkit::NkVScrollbar(*guiCtx, guiCtx->dl, track, scroll,
										contentH > area.h ? contentH : area.h + 1.f, area.h, id,
										kRowH);
			} else {
				p.VScroll(area, contentH, scroll);
			}
			return {area.x, area.y, area.w - sbW, area.h};
		}

		// Ligne « Transmettre » d'un parent : quelles composantes de SA
		// transformation atteignent ses enfants (option par transformation,
		// idee de Rihen).
		// `r` est le rectangle de TRAVAIL : les trois boutons se calent sur SA
		// largeur. Ils se calculaient auparavant sur la largeur du panneau entier,
		// et debordaient donc du cadre d'un groupe (constate par Rihen).
		inline void NkXmitRow(NkModelerPainter &p, NkHitRegistry &hit, const NkRect &r,
							  const NkRect &rr, float32 &yy, int32 node) {
			(void)rr;
			const float32 labW = S(90.f);
			p.TextV(r.x, yy, kRowH, "Transmettre", NkRole::TextMuted);
			int32 mask = demo::Demo3DHostNodeXmitMask(node);
			static const char *const kXm[3] = {"Pos", "Rot", "Ech"};
			const float32 bw = (r.w - labW - S(8.f)) / 3.f;
			char kx[24];
			for (int32 b = 0; b < 3; ++b) {
				const NkRect br{r.x + labW + (float32)b * (bw + S(4.f)), yy + S(2.f), bw,
								kRowH - S(4.f)};
				snprintf(kx, sizeof(kx), "prop.xmit.%d", b);
				hit.Add(kx, br);
				const bool on2 = ((mask >> b) & 1) != 0;
				if (on2)
					p.Fill(br, NkRole::AccentUi, 3.f);
				else
					p.Outline(br, NkRole::Border, NkRole::PanelHeader, 3.f);
				const float32 tw = p.TextW(kXm[b]);
				p.TextV(br.x + (br.w - tw) * 0.5f, yy, kRowH, kXm[b],
						on2 ? NkRole::TextOnAccent : NkRole::Text);
				if (hit.Clicked(kx))
					mask ^= (1 << b);
			}
			demo::Demo3DHostSetNodeXmitMask(node, mask);
			yy += kRowH;
		}
		inline void NkHierNodeName(NkModelerState &st, int32 node, char *out, uint32 cap);
		// Nature d'un noeud utilisateur -> libelle et icone de la hierarchie.
		inline const char *NkUserKindLabel(int32 k) {
			static const char *const kL[11] = {"Empty",  "Maillage", "Maillage",
											   "Maillage", "Empty",	"Lumiere",
											   "Texte",	"Courbe",	"Surface",
											   "Metaball", "Maillage"};
			return (k >= 0 && k <= 10) ? kL[k] : "Empty";
		}
		inline NkIcon NkUserKindIcon(int32 k) {
			switch (k) {
				case 1:
				case 2:
				case 3:
					return NkIcon::Mesh;
				case 5:
					return NkIcon::Light;
				case 6:
					return NkIcon::Text3D;
				case 7:
					return NkIcon::CurveBezier;
				case 8:
					return NkIcon::SurfacePatch;
				case 9:
					return NkIcon::Metaball;
				case 10:
					return NkIcon::CircleEdge;
				default:
					return NkIcon::Cursor;
			}
		}
		// Nom d'un double/colle : « base.NNN » (base = nom AFFICHE de la
		// source, suffixe .NNN existant coupe pour ne pas empiler).
		inline void NkHierComposeName(NkModelerState &st, const char *base0, int32 newNode) {
			if (newNode < 96 || newNode >= 160 || !base0 || !base0[0])
				return;
			char base[24];
			snprintf(base, sizeof(base), "%s", base0);
			const int32 bl = (int32)strlen(base);
			if (bl > 4 && base[bl - 4] == '.' && base[bl - 3] >= '0' && base[bl - 3] <= '9' &&
				base[bl - 2] >= '0' && base[bl - 2] <= '9' && base[bl - 1] >= '0' &&
				base[bl - 1] <= '9')
				base[bl - 4] = 0;
			snprintf(st.customNames[newNode], 24, "%.19s.%03d", base, (newNode - 96) + 1);
		}
		// Nom UNIQUE par (dossier, nature) : Base, Base_02, Base_03... (Rihen).
		inline void NkBrowUniqueName(NkModelerState &st, uint8 kind, int32 parent,
									 const char *base, char *out, uint32 cap) {
			for (int32 n7 = 1; n7 < 1000; ++n7) {
				if (n7 == 1)
					snprintf(out, cap, "%s", base);
				else
					snprintf(out, cap, "%s_%02d", base, n7);
				bool taken = false;
				for (int32 j7 = 0; j7 < st.browserCount; ++j7) {
					if (st.browserNames[j7] == out)
						continue; // soi-meme (nom en cours d'ecriture)
					if (st.browserKind[j7] == kind && st.browserParent[j7] == parent &&
						strcmp(st.browserNames[j7], out) == 0) {
						taken = true;
						break;
					}
				}
				if (!taken)
					return;
			}
		}
		// Homonyme de MEME NATURE dans un dossier (hors src et supprimes).
		inline int32 NkBrowFindSame(NkModelerState &st, int32 dest, uint8 kind,
									const char *name, int32 excl) {
			for (int32 j8 = 0; j8 < st.browserCount; ++j8)
				if (j8 != excl && st.browserKind[j8] == kind &&
					st.browserParent[j8] == dest &&
					strcmp(st.browserNames[j8], name) == 0)
					return j8;
			return -1;
		}
		/// Chemin RELATIF du dossier d'une carte-dossier, termine par « / » --
		/// c'est cette barre finale qui dit « dossier » a la file de suppression.
		inline NkString NkBrowFolderRel(const NkModelerState &st, int32 card) {
			NkString parts[8];
			int32 n = 0, cur = card;
			for (int32 g = 0; g < 8 && cur >= 0 && cur < st.browserCount; ++g) {
				if (st.browserKind[cur] != 1)
					break;
				parts[n++] = st.browserNames[cur];
				cur = st.browserParent[cur];
			}
			NkString out;
			for (int32 i = n - 1; i >= 0; --i) {
				out += parts[i];
				out += '/';
			}
			return out;
		}
		// ── CLASSEMENT DU NAVIGATEUR ────────────────────────────────────────────
		// Comparaison de noms INSENSIBLE A LA CASSE, comme l'explorateur : « arbre »
		// et « Arbre » doivent se suivre, pas se retrouver aux deux bouts de la
		// liste selon la majuscule.
		inline int32 NkBrowNameCmp(const char *a, const char *b) {
			for (int32 i = 0;; ++i) {
				char x = a[i], y = b[i];
				if (x >= 'A' && x <= 'Z')
					x = (char)(x - 'A' + 'a');
				if (y >= 'A' && y <= 'Z')
					y = (char)(y - 'A' + 'a');
				if (x != y)
					return (x < y) ? -1 : 1;
				if (!x)
					return 0;
			}
		}

		/// Vrai si `a` doit passer AVANT `b`.
		inline bool NkBrowBefore(const NkModelerState &st, int32 a, int32 b) {
			// LES DOSSIERS D'ABORD, toujours -- meme en ordre decroissant. Un
			// dossier n'est pas un element de la liste, c'est le chemin vers la
			// suite ; le renvoyer en bas oblige a le chercher.
			const bool fa = st.browserKind[a] == 1, fb = st.browserKind[b] == 1;
			if (fa != fb)
				return fa;
			int32 c = 0;
			if (st.browSort == 1) { // TYPE, puis nom a type egal
				c = (int32)st.browserKind[a] - (int32)st.browserKind[b];
				if (c == 0)
					c = NkBrowNameCmp(st.browserNames[a], st.browserNames[b]);
			} else if (st.browSort == 2) { // DATE, puis nom a date egale
				const nk_int64 ta = st.browserTime[a], tb = st.browserTime[b];
				c = (ta < tb) ? -1 : ((ta > tb) ? 1 : 0);
				if (c == 0)
					c = NkBrowNameCmp(st.browserNames[a], st.browserNames[b]);
			} else {
				c = NkBrowNameCmp(st.browserNames[a], st.browserNames[b]);
			}
			// Le SENS ne s'applique qu'au critere, jamais a la regle des dossiers.
			return st.browSortDesc ? (c > 0) : (c < 0);
		}

		/// Cartes VISIBLES du dossier courant, dans l'ordre de classement. Tri par
		/// insertion : le navigateur tient trente-deux cartes, un tri savant y
		/// couterait plus en lecture qu'il ne rapporterait en cycles.
		inline int32 NkBrowVisible(const NkModelerState &st, int32 *out, int32 cap) {
			int32 n = 0;
			for (int32 i = 0; i < st.browserCount && n < cap; ++i) {
				if (st.browserKind[i] == 255 || st.browserParent[i] != st.browserFolder)
					continue;
				// Filtre par TYPE. Les DOSSIERS restent toujours visibles : ils sont
				// le chemin vers le reste, pas un resultat de recherche.
				if (st.browFilter != 0u && st.browserKind[i] != 1 &&
					(st.browFilter & (1u << st.browserKind[i])) == 0u)
					continue;
				if (!NkNameMatches(st.browserNames[i], st.searchBrowser))
					continue;
				int32 k = n++;
				while (k > 0 && NkBrowBefore(st, i, out[k - 1])) {
					out[k] = out[k - 1];
					--k;
				}
				out[k] = i;
			}
			return n;
		}

		inline void NkMarkTreeDirty(NkModelerState &st);
		inline void NkBrowDelRec(NkModelerState &st, int32 root2) {
			NkMarkTreeDirty(st);
			int32 stk[64];
			int32 sp2 = 0;
			stk[sp2++] = root2;
			while (sp2 > 0) {
				const int32 s2 = stk[--sp2];
				// LE FICHIER SUIT LA CARTE (Rihen). Il part en CORBEILLE, pas au
				// neant : une suppression de trop doit pouvoir se rattraper -- meme
				// exigence que « fermer un onglet ne supprime rien ».
				if (st.browserFile[s2][0]) {
					st.DelPendPush(st.browserFile[s2]);
					st.browserFile[s2][0] = 0;
				} else if (st.browserKind[s2] == 1) {
					// Un DOSSIER n'a pas de fichier : c'est son repertoire qui part.
					st.DelPendPush(NkBrowFolderRel(st, s2).CStr());
				}
				st.browserKind[s2] = 255;
				for (int32 j4 = 0; j4 < st.browserCount; ++j4)
					if (st.browserParent[j4] == s2 && st.browserKind[j4] != 255 && sp2 < 63)
						stk[sp2++] = j4;
			}
			if (st.browserFolder == root2)
				st.browserFolder = -1;
			if (st.selectedAsset == root2)
				st.selectedAsset = -1;
		}
		// Copie RECURSIVE avec noms uniques par niveau (et souvenir de source).
		inline void NkBrowCopyRecU(NkModelerState &st, int32 src, int32 par) {
			int32 stk[64][2];
			int32 sp2 = 0;
			stk[sp2][0] = src;
			stk[sp2][1] = par;
			++sp2;
			while (sp2 > 0) {
				--sp2;
				const int32 s2 = stk[sp2][0];
				const int32 p2 = stk[sp2][1];
				if (st.browserCount >= NkModelerState::kMaxBrowser)
					break;
				const int32 k4 = st.browserCount++;
				st.browserKind[k4] = st.browserKind[s2];
				st.browserParent[k4] = p2;
				st.browserSrcNode[k4] = st.browserSrcNode[s2];
				NkBrowUniqueName(st, st.browserKind[s2], p2, st.browserNames[s2],
								 st.browserNames[k4], 32);
				if (st.browserKind[s2] == 1)
					for (int32 j4 = 0; j4 < k4; ++j4)
						if (st.browserParent[j4] == s2 && st.browserKind[j4] != 255 &&
							sp2 < 63) {
							stk[sp2][0] = j4;
							stk[sp2][1] = k4;
							++sp2;
						}
			}
		}
		// DEPLACER en REMPLACANT : dossier homonyme = FUSION recursive (le
		// contenu migre et l'identite se reverifie a chaque niveau -- Windows).
		inline void NkBrowMoveReplace(NkModelerState &st, int32 src, int32 dest) {
			const int32 dup = NkBrowFindSame(st, dest, st.browserKind[src],
											 st.browserNames[src], src);
			if (dup < 0) {
				st.browserParent[src] = dest;
				return;
			}
			if (st.browserKind[src] == 1) {
				for (int32 c8 = 0; c8 < st.browserCount; ++c8)
					if (st.browserKind[c8] != 255 && st.browserParent[c8] == src)
						NkBrowMoveReplace(st, c8, dup);
				st.browserKind[src] = 255; // la coquille vide disparait
				if (st.browserFolder == src)
					st.browserFolder = dup;
			} else {
				// FICHIER homonyme : ON DEMANDE (file du dialogue) -- plus de
				// remplacement silencieux pendant une fusion (Rihen).
				if (st.browConfQN < 32) {
					st.browConfQ[st.browConfQN][0] = src;
					st.browConfQ[st.browConfQN][1] = dest;
					st.browConfQCopy &= ~(1u << st.browConfQN);
					st.browConfQN++;
				}
			}
		}
		// Remplacement EXPLICITE d'un seul element (choix du dialogue).
		inline void NkBrowReplaceOne(NkModelerState &st, int32 src, int32 dest,
									 bool isCopy) {
			const int32 dup = NkBrowFindSame(st, dest, st.browserKind[src],
											 st.browserNames[src], src);
			if (dup >= 0)
				NkBrowDelRec(st, dup);
			if (isCopy)
				NkBrowCopyRecU(st, src, dest);
			else
				st.browserParent[src] = dest;
		}
		inline int32 NkBrowCopyOne(NkModelerState &st, int32 src, int32 par) {
			if (st.browserCount >= NkModelerState::kMaxBrowser)
				return -1;
			const int32 k4 = st.browserCount++;
			st.browserKind[k4] = st.browserKind[src];
			st.browserParent[k4] = par;
			st.browserSrcNode[k4] = st.browserSrcNode[src];
			snprintf(st.browserNames[k4], 32, "%s", st.browserNames[src]);
			return k4;
		}
		inline void NkBrowCopyReplace(NkModelerState &st, int32 src, int32 dest) {
			const int32 dup = NkBrowFindSame(st, dest, st.browserKind[src],
											 st.browserNames[src], src);
			if (dup >= 0 && st.browserKind[src] == 1) {
				for (int32 c8 = 0; c8 < st.browserCount; ++c8)
					if (st.browserKind[c8] != 255 && st.browserParent[c8] == src)
						NkBrowCopyReplace(st, c8, dup);
				return;
			}
			if (dup >= 0) {
				// fichier homonyme en COPIE : la file du dialogue tranchera
				if (st.browConfQN < 32) {
					st.browConfQ[st.browConfQN][0] = src;
					st.browConfQ[st.browConfQN][1] = dest;
					st.browConfQCopy |= (1u << st.browConfQN);
					st.browConfQN++;
				}
				return;
			}
			const int32 nk8 = NkBrowCopyOne(st, src, dest);
			if (nk8 >= 0 && st.browserKind[src] == 1)
				for (int32 c8 = 0; c8 < st.browserCount; ++c8)
					if (c8 != nk8 && st.browserKind[c8] != 255 &&
						st.browserParent[c8] == src)
						NkBrowCopyReplace(st, c8, nk8);
		}
		// DEMANDE de transfert : sans homonyme on agit ; sinon le DIALOGUE
		// Renommer / Remplacer / Arreter tranche (regle de Rihen).
		inline void NkBrowRequestTransfer(NkModelerState &st, int32 src, int32 dest,
										  bool isCopy, float32 mx, float32 my) {
			if (src < 0 || st.browserKind[src] == 255)
				return;
			if (!isCopy && st.browserParent[src] == dest)
				return; // deja la
			const int32 dup = NkBrowFindSame(st, dest, st.browserKind[src],
											 st.browserNames[src], src);
			if (dup < 0) {
				if (isCopy)
					NkBrowCopyRecU(st, src, dest);
				else
					st.browserParent[src] = dest;
				return;
			}
			st.browConfSrc = src;
			st.browConfDest = dest;
			st.browConfCopy = isCopy;
			st.browConfX = mx;
			st.browConfY = my;
		}
		// ── REGLE MODEL / MESH (Rihen) ──────────────────────────────────────
		// Model -> Model : parente possible (hierarchie de scene classique).
		// Model -> Mesh  : CONTENANCE (le model contient ses maillages).
		// Mesh  -> Mesh  : JAMAIS de parente -- un maillage est une DONNEE
		// geometrique, pas un noeud de hierarchie. Les maillages d'un meme
		// model sont donc forcement FRERES.
		// Dans un editeur de Model, la seule cible de parente legitime est
		// donc la RACINE du model.
		inline int32 NkModelRootOf(const NkModelerState &st) {
			if (st.sceneTabKind[st.activeTab] != 7)
				return -1; // pas dans un editeur de Model
			const int32 d = st.TabDoc(st.activeTab);
			if (d >= 0 && st.docIsoNode[d] > 0)
				return st.docIsoNode[d] - 1;
			return st.editPreviewNode - 1;
		}
		// Cible de parente AUTORISEE pour un noeud depose sur `dest`.
		// -1 = refuser le depot.
		inline int32 NkParentTargetAllowed(const NkModelerState &st, int32 dest) {
			const int32 mr = NkModelRootOf(st);
			if (mr < 0)
				return dest; // scene : Model -> Model, libre
			return mr;	   // model : tout maillage est FRERE sous la racine
		}
		inline void NkHierNameNewNode(NkModelerState &st, int32 srcNode, int32 newNode) {
			char b[24];
			NkHierNodeName(st, srcNode, b, sizeof(b));
			NkHierComposeName(st, b, newNode);
		}
		// D'OU VIENT LE VERROU ? Le sien, ou celui du premier ancetre cadenasse.
		// Sert a EXPLIQUER un refus de selection : un clic sans effet passe sinon
		// pour une panne (leçon d'un vrai depannage avec Rihen).
		inline void NkHierLockedName(NkModelerState &st, int32 node, char *out, uint32 cap) {
			char nm[24];
			if (demo::Demo3DHostObjectLocked(node)) {
				NkHierNodeName(st, node, nm, sizeof(nm));
				snprintf(out, cap, "%s est verrouille -- cliquez son cadenas pour l'ouvrir.",
						 nm);
				return;
			}
			int32 cur = demo::Demo3DHostNodeParent(node);
			for (int32 g = 0; g < 96 && cur >= 0; ++g) {
				if (demo::Demo3DHostObjectLocked(cur)) {
					NkHierNodeName(st, cur, nm, sizeof(nm));
					snprintf(out, cap,
							 "Verrouille par son parent %s -- ouvrez SON cadenas.", nm);
					return;
				}
				cur = demo::Demo3DHostNodeParent(cur);
			}
			out[0] = 0;
		}
		// ── L'ICONE D'UN NOEUD, PAR SA NATURE ──────────────────────────────────
		// Un seul endroit la decide, pour que la hierarchie et le panneau de
		// proprietes montrent TOUJOURS le meme dessin (Rihen). C'est aussi ce qui
		// manquait a la camera : son sous-type se perdait, et elle heritait de
		// l'icone des empties.
		inline NkIcon NkNodeIcon(int32 node) {
			if (node >= 86 && node < 90)
				return NkIcon::Light; // lumieres natives de la demo
			if (demo::Demo3DHostNodeIsModel(node))
				return NkIcon::Cube3D; // le conteneur
			if (demo::Demo3DHostNodeIsMesh(node))
				return NkIcon::Mesh; // sa matiere
			const int32 uk = node >= 96 ? demo::Demo3DHostUserKind(node) : 0;
			if (uk == 5)
				return NkIcon::Light;
			if (uk == 4) {
				// Les EMPTIES se distinguent par leur sous-type : la camera a le
				// sien, l'image de reference aussi.
				const int32 us = demo::Demo3DHostUserSub(node);
				if (us == 10)
					return NkIcon::Camera;
				if (us == 11)
					return NkIcon::ImageRef;
				return NkIcon::EmptyAxes;
			}
			if (uk >= 1 && uk <= 3)
				return NkUserKindIcon(uk);
			return node >= 90 ? NkIcon::EmptyAxes : NkIcon::Mesh;
		}
		// Le noeud CONTIENT-IL des maillages ? Alors c'est un MODEL : sa geometrie
		// vit dans ses maillages, pas en lui.
		inline bool NkNodeHasMeshKids(int32 node) {
			const int32 nc = demo::Demo3DHostNodeCount();
			for (int32 c = 0; c < nc; ++c)
				if (demo::Demo3DHostNodeIsMesh(c) &&
					demo::Demo3DHostNodeParent(c) == node &&
					!demo::Demo3DHostNodeDeleted(c))
					return true;
			return false;
		}
		// Le noeud a-t-il des enfants VIVANTS (ni supprimes ni slots libres) ?
		inline bool NkHierHasLiveKids(int32 node) {
			const int32 nc = demo::Demo3DHostNodeCount();
			for (int32 c = 0; c < nc; ++c)
				if (!NkHierNodeSkip(c) && demo::Demo3DHostNodeParent(c) == node)
					return true;
			return false;
		}
		// Le noeud est-il un DESCENDANT de anc dans l'arbre de parente ?
		inline bool NkHierIsDescendant(int32 node, int32 anc) {
			int32 cur = node;
			for (int32 g = 0; g < 96 && cur >= 0; ++g) {
				cur = demo::Demo3DHostNodeParent(cur);
				if (cur == anc)
					return true;
			}
			return false;
		}
		// Case « Propager aux enfants » d'une propriete commune parent/enfant.
		inline bool NkPropagateCheck(NkModelerPainter &p, NkHitRegistry &hit, const NkRect &r,
									 float32 y, const char *key, bool &on) {
			const NkRect cb{r.x + kPad, y + S(5.f), S(12.f), S(12.f)};
			hit.Add(key, cb);
			p.Outline(cb, on ? NkRole::AccentUi : NkRole::Border,
					  on ? NkRole::AccentUi : NkRole::InputBg, 2.f);
			p.TextV(cb.x + S(18.f), y, kRowH, "Propager aux enfants", NkRole::TextMuted);
			if (hit.Clicked(key)) {
				on = !on;
				return true;
			}
			return false;
		}


	} // namespace nk3d
} // namespace nkentseu
