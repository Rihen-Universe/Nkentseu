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
			/// L'ECHELLE DU DOCUMENT (le zoom de la vue), posee par la toile a
			/// chaque image (correction du 31/08 : « quand on zoome ou dezoome
			/// le texte garde sa taille... ca devrait evoluer comme les elements
			/// graphiques »). Le texte d'une FORME du document se dessine a
			/// corps pose x cette echelle — les rectangles, eux, arrivent deja
			/// projetes. 1 par defaut : la sonde et le temoin headless mesurent
			/// a l'identique. ⚠️ Le texte INTERNE des composants poses
			/// (tree_view, content_browser) ne suit pas encore : leurs rangees
			/// et leurs corps sont des constantes du kit — chantier nomme,
			/// pas glisse.
			float32 docScale = 1.f;
			/// LA LANGUE ACTIVE d'apercu/edition (multilingue 01/09) : vide = la
			/// langue principale (`texte`). Posee par la vue a chaque image — le
			/// changement est A CHAUD, rien ne se recharge (les largeurs de
			/// texte se mesurent au dessin, aucun cache a invalider ici).
			nkentseu::NkString langueDoc;
			/// Le noeud en cours d'EDITION EN PLACE (-1 = aucun) : son texte est
			/// dessine par le champ superpose TRANSPARENT — le dessiner aussi ici
			/// donnerait un double trait. Pose par la toile a chaque image.
			int32 editionNode = -1;
			/// Vrai quand l'edition en place porte sur L'ETIQUETTE de l'artboard
			/// `editionNode` : son CORPS se dessine encore, seule l'etiquette se
			/// tait (le champ superpose la remplace).
			bool editionEtiquette = false;
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
				// La demo vient des RESSOURCES (voir les chargeurs ci-dessous) ;
				// si elles manquent, les modeles restent vides et ca se VOIT.
				NkContentBrowserModel m0;
				NkChargerContenuDemo(
					m0, "Applications/NKUIDesign/design/mises_en_scene/demo_contenu.txt");
				NkTreeViewModel t0;
				NkChargerArbreDemo(
					t0, "Applications/NKUIDesign/design/mises_en_scene/demo_arbre.txt");
				for (uint32 i = 0; i < n; ++i) {
					demoModels.PushBack(m0);
					demoTrees.PushBack(t0);
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

			/// LES CHARGEURS DU CONTENU DE DÉMONSTRATION (cadrage Rodolf 31/08 :
			/// « ce qui est dans la zone infinie, c'est une démo » — la démo est une
			/// RESSOURCE chargée par un chemin normal, pas une table du code, même
			/// principe que les demo_ecran_NN.nkuidoc). Format ligne à ligne,
			/// sections `[...]`, `#` = commentaire, champs séparés par `|`.
			/// Rendent FAUX si la ressource manque : le modèle reste VIDE et le
			/// composant montre son état vide — jamais un décor repeint en silence.
			/// ⚠️ La sonde garde SA copie de la table (Probe.h, FillDemoModel) : un
			/// côté de la mesure doit venir d'ailleurs que du code testé.
			/// La nature INTERNEE : `kindLabel` est un `const char *` qui doit
			/// survivre au chargeur — le vocabulaire des natures est ferme, la
			/// ressource ne peut qu'en choisir une. Une nature inconnue devient
			/// « autre » (et se voit en couleur de texte neutre).
			static const char *NkNatureInternee(const char *n) {
				static const char *const kNatures[] = {"dossier", "maillage", "materiau",
													   "texture", "animation", "racine",
													   "groupe",  "lumiere",	"os"};
				for (uint32 i = 0; i < sizeof(kNatures) / sizeof(kNatures[0]); ++i)
					if (StrEq(n, kNatures[i]))
						return kNatures[i];
				return "autre";
			}
			static void NkLigneChamps(const char *q, const char *fin, NkString out[3],
									  int32 &nb) {
				nb = 0;
				const char *deb = q;
				for (const char *c = q; c <= fin && nb < 3; ++c) {
					if (c == fin || *c == '|') {
						out[nb++] = NkString(deb, (uint32)(c - deb));
						deb = c + 1;
					}
				}
			}
			static bool NkChargerContenuDemo(NkContentBrowserModel &m, const char *chemin) {
				m.entries.Clear();
				m.breadcrumb.Clear();
				m.kinds.Clear();
				m.folders.nodes.Clear();
				if (!nkentseu::NkFile::Exists(chemin))
					return false;
				NkString tout = nkentseu::NkFile::ReadAllText(chemin);
				const char *q = tout.Data();
				int32 section = 0; // 1 entrees, 2 chemin, 3 natures, 4 dossiers
				while (q && *q) {
					const char *fin = q;
					while (*fin && *fin != '\n')
						++fin;
					const char *utile = fin; // couper le commentaire de fin de ligne
					for (const char *c = q; c < fin; ++c)
						if (*c == '#') {
							utile = c;
							break;
						}
					while (utile > q && (utile[-1] == ' ' || utile[-1] == '\t' || utile[-1] == '\r'))
						--utile;
					if (utile > q && *q == '[') {
						section = (q[1] == 'e') ? 1 : (q[1] == 'c') ? 2 : (q[1] == 'n') ? 3
								  : (q[1] == 'd')		 ? 4
														 : 0;
					} else if (utile > q && *q != '#') {
						NkString ch[3];
						int32 nb = 0;
						NkLigneChamps(q, utile, ch, nb);
						if (section == 1 && nb >= 2) {
							NkAssetEntry e;
							e.name = ch[0];
							e.kindLabel = NkNatureInternee(ch[1].Data());
							e.isFolder = StrEq(e.kindLabel, "dossier");
							// ⚠️ CHEMIN NON VIDE, VOLONTAIREMENT : `path` est la charge
							//    que les evenements portent vers un blueprint.
							e.path = NkString("/projet/");
							e.path.Append(e.name);
							// ⚠️ LE ROLE VIENT DE LA NATURE, PAS D'UN MODULO — une
							//    donnee de demonstration fausse rend une capture
							//    ininterpretable (lecon de la premiere ecriture).
							e.kindRole = RoleOfKind(e.kindLabel);
							m.entries.PushBack(e);
						} else if (section == 2 && nb >= 1) {
							m.breadcrumb.PushBack(ch[0]);
						} else if (section == 3 && nb >= 1) {
							nkentseu::editorkit::NkBrowserKind kind;
							kind.label = ch[0];
							kind.role = RoleOfKind(ch[0].Data());
							m.kinds.PushBack(kind);
						} else if (section == 4 && nb >= 2) {
							nkentseu::editorkit::NkTreeNode nd;
							nd.id = (nkentseu::nk_uint64)(m.folders.nodes.Size() + 1);
							nd.parent = (int32)atoi(ch[0].Data());
							nd.label = ch[1];
							nd.path = NkString("/dossiers/");
							nd.path.Append(nd.label);
							nd.kindRole = NkDesignResolveRole("type_folder");
							m.folders.nodes.PushBack(nd);
						}
					}
					q = (*fin) ? fin + 1 : fin;
				}
				m.statusRight = NkString("Sauvegardé");
				return m.entries.Size() > 0;
			}

			/// ⚠️ ORDRE PREFIXE OBLIGATOIRE (`NkTreeViewModel::IsWellFormed`) : dans
			///    la ressource, un parent precede toujours ses enfants — le chargeur
			///    ne trie pas, il fait confiance au fichier et le composant VERIFIE.
			static bool NkChargerArbreDemo(NkTreeViewModel &t, const char *chemin) {
				t.nodes.Clear();
				t.chosen.Clear();
				t.active = 0;
				if (!nkentseu::NkFile::Exists(chemin))
					return false;
				NkString tout = nkentseu::NkFile::ReadAllText(chemin);
				const char *q = tout.Data();
				int32 section = 0; // 1 noeuds, 2 actif
				while (q && *q) {
					const char *fin = q;
					while (*fin && *fin != '\n')
						++fin;
					const char *utile = fin;
					for (const char *c = q; c < fin; ++c)
						if (*c == '#') {
							utile = c;
							break;
						}
					while (utile > q && (utile[-1] == ' ' || utile[-1] == '\t' || utile[-1] == '\r'))
						--utile;
					if (utile > q && *q == '[') {
						section = (q[1] == 'n') ? 1 : (q[1] == 'a') ? 2 : 0;
					} else if (utile > q && *q != '#') {
						NkString ch[3];
						int32 nb = 0;
						NkLigneChamps(q, utile, ch, nb);
						if (section == 1 && nb >= 3) {
							NkTreeNode n;
							n.id = (nkentseu::nk_uint64)(t.nodes.Size() + 1);
							n.parent = (int32)atoi(ch[0].Data());
							n.label = ch[1];
							// Meme raison que chez le navigateur : le chemin est la
							// CHARGE qu'un evenement porte vers un blueprint.
							n.path = NkString("/scene/");
							n.path.Append(n.label);
							n.kindLabel = NkNatureInternee(ch[2].Data());
							n.kindRole = RoleOfKind(n.kindLabel);
							t.nodes.PushBack(n);
						} else if (section == 2 && nb >= 1) {
							t.active = (nkentseu::nk_uint64)atoi(ch[0].Data());
							t.chosen.Clear();
							t.chosen.PushBack(t.active);
						}
					}
					q = (*fin) ? fin + 1 : fin;
				}
				return t.nodes.Size() > 0;
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
		/// « #rrggbb » -> 0xRRGGBBAA (alpha plein). Une graphie invalide rend un
		/// GRIS MOYEN visible (0x808080FF) — jamais un noir silencieux : la faute
		/// se voit sur la toile au lieu de se confondre avec un choix.
		inline nkentseu::uint32 NkGHexRGBA(const char *hex) {
			if (!hex || hex[0] != '#')
				return 0x808080FFu;
			nkentseu::uint32 v = 0;
			for (int i = 1; i <= 6; ++i) {
				const char c = hex[i];
				int d;
				if (c >= '0' && c <= '9')
					d = c - '0';
				else if (c >= 'a' && c <= 'f')
					d = c - 'a' + 10;
				else if (c >= 'A' && c <= 'F')
					d = c - 'A' + 10;
				else
					return 0x808080FFu;
				v = (v << 4) | (nkentseu::uint32)d;
			}
			return (v << 8) | 0xFFu;
		}

		/// LE FOND D'UN NŒUD, OPACITÉ COMPRISE — le seul endroit qui traduit la
		/// liste de remplissages en une couleur pour les peintres de FORME.
		/// ⚠️ LES FORMES NE COMPOSITENT PAS ENCORE, ET C'EST DIT ICI PLUTÔT QUE
		///    TU. Le rectangle, lui, peint TOUS ses remplissages empilés (voir
		///    son site) ; l'ellipse, le triangle, l'étoile, la flèche, l'image et
		///    l'avatar passent par une couleur unique — leurs primitives ne
		///    prennent qu'un `rgba`. Ils rendent donc le remplissage VISIBLE LE
		///    PLUS HAUT, ce que Lunacy montrerait aussi si les autres étaient
		///    couverts. Empiler chez eux demande de rappeler la primitive N fois
		///    avec le même polygone : faisable, non fait, nommé.
		inline nkentseu::uint32 NkGFondRGBA(const NkUINode &n) {
			const char *c = n.FondEffectif();
			if (!c)
				return 0x808080FFu;
			const nkentseu::uint32 rgba = NkGHexRGBA(c);
			const nkentseu::float32 o = n.FondOpacite();
			if (o >= 100.f)
				return rgba;
			const nkentseu::float32 k = o < 0.f ? 0.f : o * 0.01f;
			const nkentseu::uint32 a = (nkentseu::uint32)((rgba & 0xFFu) * k + 0.5f);
			return (rgba & 0xFFFFFF00u) | (a & 0xFFu);
		}

		/// LES QUATRE BANDES D'UNE BORDURE, POSÉES SELON SA POSITION.
		/// ⚠️ C'EST ICI QUE « intérieur / centre / extérieur » VEUT DIRE QUELQUE
		///    CHOSE. Le trait a une épaisseur `e` ; le rectangle peint est
		///    décalé de 0 (intérieur), e/2 (centré, à cheval) ou e (extérieur).
		///    Sans ce décalage, les trois valeurs auraient rendu le même dessin :
		///    un menu à trois entrées dont deux mentent.
		inline void NkGCadre(NkComponentPaint &p, const NkPaintRect &r, const NkBordure &b) {
			const nkentseu::float32 e = b.epaisseur > 0.f ? b.epaisseur : 1.f;
			nkentseu::float32 d = 0.f; // de combien le cadre sort du rectangle
			if (b.position == NkBordurePos::Centre)
				d = e * 0.5f;
			else if (b.position == NkBordurePos::Exterieur)
				d = e;
			const NkPaintRect q = {r.x - d, r.y - d, r.w + d * 2.f, r.h + d * 2.f};
			const nkentseu::uint32 base = NkGHexRGBA(b.couleur.Data());
			const nkentseu::float32 k =
				(b.opacite < 0.f ? 0.f : (b.opacite > 100.f ? 100.f : b.opacite)) * 0.01f;
			const nkentseu::uint32 a = (nkentseu::uint32)((base & 0xFFu) * k + 0.5f);
			const nkentseu::uint32 rgba = (base & 0xFFFFFF00u) | (a & 0xFFu);
			p.FillColor({q.x, q.y, q.w, e}, rgba, 0.f);					 // haut
			p.FillColor({q.x, q.y + q.h - e, q.w, e}, rgba, 0.f);		 // bas
			p.FillColor({q.x, q.y, e, q.h}, rgba, 0.f);					 // gauche
			p.FillColor({q.x + q.w - e, q.y, e, q.h}, rgba, 0.f);		 // droite
		}

		/// LES OMBRES PORTÉES D'UN NŒUD, peintes AVANT sa forme.
		/// ⚠️ L'OMBRE INTERNE N'EST PAS PEINTE, ET LE CODE LE DIT PLUTÔT QUE DE
		///    FAIRE SEMBLANT. Elle demande de découper l'intérieur de la forme
		///    (un masque), ce que ce peintre ne sait pas faire ; la rendre comme
		///    une ombre portée donnerait un dessin FAUX qui a l'air juste — la
		///    pire des sorties. Elle se règle dans l'Inspecteur, se sauve, se
		///    relit, et attend son peintre. C'est écrit ici et au rapport.
		inline void NkGOmbres(NkComponentPaint &p, const NkPaintRect &r, const NkUINode &n,
							  nkentseu::float32 rayon) {
			for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)n.effets.Size(); ++i) {
				const NkEffet &e = n.effets[i];
				if (!e.visible || e.couleur.Empty() || e.type != NkEffetType::OmbrePortee)
					continue;
				const nkentseu::uint32 base = NkGHexRGBA(e.couleur.Data());
				const nkentseu::float32 op =
					(e.opacite < 0.f ? 0.f : (e.opacite > 100.f ? 100.f : e.opacite)) * 0.01f;
				if (op <= 0.f)
					continue;
				// Le flou : `kAnneaux` couches concentriques, chacune plus large et
				// plus transparente. Une seule couche quand le flou est nul.
				const int32 kAnneaux = e.flou > 0.5f ? 4 : 1;
				for (int32 k = kAnneaux; k >= 1; --k) {
					const nkentseu::float32 t = (nkentseu::float32)k / (nkentseu::float32)kAnneaux;
					const nkentseu::float32 grossi = e.etendue + e.flou * t;
					// L'alpha décroît vers l'extérieur : la couche la plus large est
					// la plus pâle, sinon on peindrait une auréole nette.
					const nkentseu::float32 a01 = op / (nkentseu::float32)kAnneaux;
					const nkentseu::uint32 a =
						(nkentseu::uint32)((base & 0xFFu) * a01 + 0.5f);
					if (a == 0u)
						continue;
					const NkPaintRect q = {r.x + e.x - grossi, r.y + e.y - grossi,
										   r.w + grossi * 2.f, r.h + grossi * 2.f};
					p.FillColor(q, (base & 0xFFFFFF00u) | (a & 0xFFu), rayon + grossi);
				}
			}
		}

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
		/// UN CADRE D AGENCEMENT NE DESSINE RIEN, et c est voulu : il sert a
		/// repartir ses enfants, il n a pas de matiere propre.
		inline void DrawFrame(NkComponentPaint &, const NkPaintRect &, const NkDocumentHost &) {}

		/// UNE FORME POSEE SUR LA TOILE, ELLE, SE DESSINE.
		///
		/// ⚠️ C EST LE DEFAUT DU 2026-08-28, ET IL ETAIT ECRIT DEPUIS LE DEBUT.
		///    `Document.h` dit : « Vide = un CADRE : le noeud n affiche rien et
		///    sert a agencer. » Les formes de la toile ont ete creees avec un
		///    composant VIDE -- donc des cadres -- donc **invisibles par
		///    construction**. Rodolf a lance, vu du noir, et il avait raison.
		///
		///    Ni une machinerie debranchee, ni un probleme de region : les formes
		///    etaient calculees, projetees, publiees -- et dessinees par une
		///    fonction VIDE, `{}`, trois parametres sans nom.
		///
		/// ⚠️ LA MEME DISTINCTION QUI GOUVERNE LA POSITION GOUVERNE LA VISIBILITE,
		///    et c est ce qui evite de casser l existant : un noeud sous un parent
		///    `Free` est une FORME, un noeud sous `Column`/`Row`/`Grid`/`Anchor`
		///    est un CADRE. Le document du 18 aout n a aucun parent `Free` : son
		///    rendu ne bouge donc pas d un pixel.
		inline void DrawShape(NkComponentPaint &p, const NkPaintRect &r, const NkUINode &n,
							  const NkDocumentHost &host) {
			if (r.w <= 0.f || r.h <= 0.f)
				return;
			const char *name = n.label.Data();
			const char *shape = n.shape.Data();

			// ── LA NATURE DESSINÉE (2026-08-30, planche 22.0) ────────────────
			// Le vocabulaire vient de la spécification §4.2, la clé `forme` du
			// nœud. Une forme SANS nature garde l'ancien dessin (contour + nom) :
			// les documents d'avant cette clé ne changent pas d'un pixel.
			if (shape && StrEq(shape, "frame")) {
				// L'ARTBOARD (Banani V2) : carte BLANCHE (`artboard_bg`) sur la
				// toile claire, étiquette AU-DESSUS. Avec une CIBLE d'appareil
				// (clé `cible`, écran 26), l'étiquette est EXACTEMENT celle de la
				// maquette : « Connexion — Mobile 390 x 844 » — le nom du nœud,
				// un tiret, la cible. Sans cible : l'étiquette historique
				// « nom — L × H » (les documents d'avant ne bougent pas).
				// Couleur : `doc_muted` (#656d76, le gris de la toile CLAIRE —
				// pas le TextMuted de l'éditeur), corps 11 px.
				p.Fill(r, host.Role("artboard_bg"));
				p.OutlineSharp(r, host.Role("border"));
				if (name && *name) {
					const float32 lh = 14.f; // interligne d'un corps 11
					char etiquette[128];
					if (!n.target.Empty())
						snprintf(etiquette, sizeof(etiquette), "%s — %s", name,
								 n.target.Data());
					else
						snprintf(etiquette, sizeof(etiquette), "%s — %d × %d", name,
								 (int)(n.width.value + 0.5f), (int)(n.height.value + 0.5f));
					// MOBILIER d'éditeur (Lunacy/Figma : l'étiquette garde sa
					// taille écran quel que soit le zoom) — d'où PAS de
					// `docScale`, et `CorpsMaquette` pour suivre le réglage
					// unique de taille d'interface (TextHex rend désormais le
					// corps demandé EXACTEMENT).
					p.TextHex({r.x, r.y - lh - 10.f, r.w + 200.f, lh}, etiquette,
							  p.ColorOf(host.Role("doc_muted")), host.Role("doc_muted"),
							  NkTextAlign::Left, costume::CorpsMaquette(11.f), 0.f);
				}
				return;
			}
			if (shape && StrEq(shape, "rect")) {
				// Le RECTANGLE : l'apparence POSÉE prime (fond `fond`, rayon
				// `rayon`, bord `couleur_bord`/`bordure`) ; sans elle, les rôles
				// de contenu du thème — les documents d'avant ne bougent pas.
				const float32 rd = n.radius > 0.f ? n.radius : 4.f;
				// ── LES OMBRES PORTÉES, AVANT LA FORME ───────────────────────
				// ⚠️ AVANT, ET C'EST TOUTE LA DIFFÉRENCE ENTRE UNE OMBRE ET UNE
				//    TACHE. Peinte après, elle recouvrirait ce qu'elle est censée
				//    faire flotter. L'ordre du peintre EST la sémantique.
				// ⚠️ ET LE FLOU EST APPROCHÉ, PAS SIMULÉ : le peintre n'a pas de
				//    primitive floue, donc on empile quelques anneaux de plus en
				//    plus transparents. Ça DIT le flou sans le mentir — et le jour
				//    où une primitive existera, ce site est le seul à changer.
				NkGOmbres(p, r, n, rd);
				// ⚠️ LE RECTANGLE EMPILE SES REMPLISSAGES, DANS L'ORDRE DE LA
				//    LISTE — le DERNIER par-dessus, comme chez Lunacy. C'est le
				//    seul peintre de forme qui le fasse, parce que c'est le seul
				//    dont la primitive se rappelle sans coût : `FillColor` sur le
				//    même rectangle. Peindre uniquement le remplissage du dessus
				//    aurait rendu une liste de deux INDISTINGUABLE d'une liste
				//    d'un, dès que le premier est translucide.
				if (!n.fills.Empty()) {
					bool peint = false;
					for (uint32 fi = 0; fi < (uint32)n.fills.Size(); ++fi) {
						const NkRemplissage &f = n.fills[fi];
						if (!f.visible || f.couleur.Empty())
							continue;
						const uint32 base = NkGHexRGBA(f.couleur.Data());
						const float32 k = (f.opacite < 0.f ? 0.f
										   : f.opacite > 100.f ? 100.f
															   : f.opacite)
										  * 0.01f;
						const uint32 a = (uint32)((base & 0xFFu) * k + 0.5f);
						p.FillColor(r, (base & 0xFFFFFF00u) | (a & 0xFFu), rd);
						peint = true;
					}
					// TOUT masqué : on ne retombe PAS sur le rôle du thème — un
					// nœud dont l'utilisateur a fermé tous les yeux doit paraître
					// vide, pas « par défaut ».
					if (!peint)
						p.OutlineSharp(r, host.Role("border"));
				} else if (!n.fill.Empty())
					p.FillColor(r, NkGHexRGBA(n.fill.Data()), rd);
				else
					p.Fill(r, host.Role("doc_field_bg"), rd);
				// ── LES BORDURES : LA LISTE D'ABORD, LA CLÉ SIMPLE SINON ─────
				// ⚠️ ET LA POSITION EST HONORÉE, sinon c'était un champ que le
				//    fichier porte et que l'écran ignore. `NkGCadre` déplace les
				//    quatre bandes selon intérieur / centre / extérieur : c'est
				//    tout ce que « position » veut dire, et ça se voit.
				if (!n.borders.Empty()) {
					bool trace = false;
					for (uint32 bi = 0; bi < (uint32)n.borders.Size(); ++bi) {
						const NkBordure &b = n.borders[bi];
						if (!b.visible || b.couleur.Empty() || b.epaisseur <= 0.f)
							continue;
						NkGCadre(p, r, b);
						trace = true;
					}
					if (!trace && !n.FondEffectif())
						p.OutlineSharp(r, host.Role("border"));
				} else if (!n.borderColor.Empty()) {
					NkBordure b;
					b.couleur = n.borderColor;
					b.epaisseur = n.borderW > 0.f ? n.borderW : 1.f;
					b.position = NkBordurePos::Interieur; // le geste historique
					NkGCadre(p, r, b);
				} else if (!n.FondEffectif())
					p.OutlineSharp(r, host.Role("border"));
				return;
			}
			if (shape && StrEq(shape, "ellipse")) {
				// L'ELLIPSE (Lunacy : outil O). Le peintre peut ne pas savoir la
				// dessiner (défaut inerte de l'interface) : le repli est VISIBLE —
				// le contour de sa boîte + le nom, jamais un vide silencieux.
				if (!p.Ellipse(r, host.Role("doc_field_bg")))
					p.Outline(r, host.Role("border"), host.Role("input_bg"), r.h * 0.5f);
				return;
			}
			if (shape && (StrEq(shape, "line") || StrEq(shape, "line_up"))) {
				// LA LIGNE (Lunacy : outil L). Sa boîte est le rect ; la diagonale
				// tracée est descendante (`line`) ou montante (`line_up`) — les
				// deux valeurs étendent le vocabulaire §4.2 (qui n'a que `path`
				// pour l'oblique) : dit ici et dans le rapport, pas glissé.
				const bool monte = StrEq(shape, "line_up");
				const float32 y1 = monte ? r.y + r.h : r.y;
				const float32 y2 = monte ? r.y : r.y + r.h;
				if (!p.Line(r.x, y1, r.x + r.w, y2, host.Role("doc_text"), 2.f))
					p.Outline(r, host.Role("border"), host.Role("input_bg"), 1.f);
				return;
			}
			// ── LES FORMES DE LA VAGUE LUNACY (b) (31/08 : l'eventail du rail,
			//    captures 7/8) : triangle / pentagone / etoile / fleche —
			//    natures ADDITIVES du vocabulaire §4.2 (dites au rapport).
			//    L'apparence posee prime (`fond`), sinon le role de champ ; un
			//    peintre sans polygone replie sur le contour VISIBLE (le
			//    contrat d'Ellipse).
			if (shape
				&& (StrEq(shape, "triangle") || StrEq(shape, "pentagone")
					|| StrEq(shape, "etoile"))) {
				const uint32 rgba = n.FondEffectif() ? NkGFondRGBA(n)
													: p.ColorOf(host.Role("doc_field_bg"));
				const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
				const float32 dx = r.w * 0.5f, dy = r.h * 0.5f;
				// les sommets UNITAIRES (cercle inscrit, pointe en haut) —
				// precomputes : pas de trigonometrie a l'execution.
				static const float32 kTri[6] = {0.f, -1.f, 1.f, 1.f, -1.f, 1.f};
				static const float32 kPenta[10] = {0.f,		-1.f,	  .9511f, -.3090f, .5878f,
												   .8090f,	-.5878f, .8090f, -.9511f, -.3090f};
				static const float32 kEtoile[20] = {
					0.f,	 -1.f,	  .2246f,  -.3090f, .9511f,	 -.3090f, .3633f,  .1180f,
					.5878f,	 .8090f,  0.f,	   .3820f,	-.5878f, .8090f,  -.3633f, .1180f,
					-.9511f, -.3090f, -.2246f, -.3090f};
				const float32 *unit = StrEq(shape, "triangle") ? kTri
									  : StrEq(shape, "pentagone") ? kPenta
																  : kEtoile;
				const int32 nb = StrEq(shape, "triangle") ? 3
								 : StrEq(shape, "pentagone") ? 5
															 : 10;
				float32 xy[20];
				for (int32 i = 0; i < nb; ++i) {
					xy[i * 2] = cx + unit[i * 2] * dx;
					xy[i * 2 + 1] = cy + unit[i * 2 + 1] * dy;
				}
				if (!p.PolygonHex(xy, nb, rgba))
					p.Outline(r, host.Role("border"), host.Role("input_bg"), 4.f);
				return;
			}
			if (shape && StrEq(shape, "fleche")) {
				// LA FLECHE : le fut horizontal a mi-hauteur + la pointe pleine
				// a droite (l'eventail Lunacy « Line ▸ arrow »).
				const float32 ym = r.y + r.h * 0.5f;
				const uint32 rgba = n.FondEffectif() ? NkGFondRGBA(n)
													: p.ColorOf(host.Role("doc_text"));
				const float32 tete = r.w * 0.25f < 16.f ? (r.w * 0.25f) : 16.f;
				bool ok = p.Line(r.x, ym, r.x + r.w - tete * 0.6f, ym, host.Role("doc_text"),
								 2.f);
				const float32 xyT[6] = {r.x + r.w,		  ym,
										r.x + r.w - tete, ym - tete * 0.55f,
										r.x + r.w - tete, ym + tete * 0.55f};
				ok = p.PolygonHex(xyT, 3, rgba) && ok;
				if (!ok)
					p.Outline(r, host.Role("border"), host.Role("input_bg"), 1.f);
				return;
			}
			if (shape && StrEq(shape, "image")) {
				// L'IMAGE (famille Image du rail Lunacy, 3e retour 01/09) : le
				// CADRE D'IMAGE — fond, contour, diagonales et glyphe montagne.
				// La SOURCE d'image (fichier, bibliotheque de medias) est un
				// chantier nomme : ce cadre est l'objet reel du maquettage
				// (wireframe), pas une mise en scene — il se pose, se deplace,
				// se sauve, et dira son fichier le jour ou la cle existera.
				const uint32 rgba = n.FondEffectif() ? NkGFondRGBA(n)
													: p.ColorOf(host.Role("doc_field_bg"));
				p.FillColor(r, rgba, n.radius > 0.f ? n.radius : 2.f);
				p.OutlineSharp(r, host.Role("border"));
				(void)p.Line(r.x, r.y, r.x + r.w, r.y + r.h, host.Role("border"), 1.f);
				(void)p.Line(r.x, r.y + r.h, r.x + r.w, r.y, host.Role("border"), 1.f);
				// la « montagne » au centre, si la boite est assez grande
				if (r.w > 40.f && r.h > 30.f) {
					const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
					const float32 s = (r.w < r.h ? r.w : r.h) * 0.18f;
					const float32 xyM[6] = {cx - s, cy + s * 0.7f, cx, cy - s * 0.7f,
											cx + s, cy + s * 0.7f};
					(void)p.PolygonHex(xyM, 3, p.ColorOf(host.Role("doc_muted")));
				}
				return;
			}
			if (shape && StrEq(shape, "avatar")) {
				// L'AVATAR (la variante de la famille Image) : pastille de profil
				// — cercle plein + tete/epaules en creux (le vocabulaire des
				// listes d'utilisateurs). Meme contrat de repli que l'Ellipse.
				const uint32 rgba = n.FondEffectif() ? NkGFondRGBA(n)
													: p.ColorOf(host.Role("doc_field_bg"));
				const float32 d = r.w < r.h ? r.w : r.h;
				const NkPaintRect rc = {r.x + (r.w - d) * 0.5f, r.y + (r.h - d) * 0.5f, d, d};
				if (!p.Ellipse(rc, host.Role("doc_field_bg")))
					p.Outline(rc, host.Role("border"), host.Role("input_bg"), d * 0.5f);
				(void)rgba;
				// la tete (petit disque) et les epaules (triangle adouci)
				const float32 cx = rc.x + d * 0.5f;
				const NkPaintRect tete = {cx - d * 0.11f, rc.y + d * 0.24f, d * 0.22f, d * 0.22f};
				(void)p.Ellipse(tete, host.Role("doc_muted"));
				const float32 xyE[6] = {cx - d * 0.26f, rc.y + d * 0.78f, cx,
										rc.y + d * 0.52f, cx + d * 0.26f, rc.y + d * 0.78f};
				(void)p.PolygonHex(xyE, 3, p.ColorOf(host.Role("doc_muted")));
				return;
			}
			if (shape && StrEq(shape, "text")) {
				// Le TEXTE : son contenu, rien d'autre — ni fond ni cadre. Un
				// texte vide dessine son libellé de nœud en atténué, sinon une
				// forme fraîchement posée serait invisible. L'apparence POSÉE
				// prime : `couleur_texte`, `police_px`, `graisse`,
				// `texte_aligne` (§8ter, 31/08).
				// ⚠️ LE TEXTE SUIT LE ZOOM (correction du 31/08) : le corps
				//    dessiné = corps posé × `host.docScale` — comme les
				//    rectangles, qui arrivent déjà projetés. Sans corps posé, le
				//    corps de base de la maquette (12, le --text-base) — c'est
				//    aussi ce qui garde la couleur posée d'un texte sans taille
				//    (l'ancien chemin la perdait en repliant sur le rôle).
				// LA LANGUE ACTIVE (multilingue 01/09) : le texte de la langue
				// demandee ; MANQUANTE = repli VISIBLE — le texte principal en
				// attenue (jamais un vide silencieux), et le Rapport compte.
				bool traduit = true;
				const char *t = n.TexteEn(host.langueDoc.Data(), &traduit);
				const bool vide = !t || !*t;
				const uint16 roleTexte = host.Role(vide || !traduit ? "text_muted" : "doc_text");
				NkTextAlign al = NkTextAlign::Left;
				if (StrEq(n.alignText.Data(), "centre"))
					al = NkTextAlign::Center;
				else if (StrEq(n.alignText.Data(), "droite"))
					al = NkTextAlign::Right;
				if (!vide) {
					// non traduit = attenue MEME si une couleur est posee : le
					// repli doit se voir.
					const uint32 rgba = (n.textColor.Empty() || !traduit)
											? p.ColorOf(roleTexte)
											: NkGHexRGBA(n.textColor.Data());
					const float32 corps = (n.fontPx > 0.f ? n.fontPx : 12.f) * host.docScale;
					p.TextHex(r, t, rgba, roleTexte, al, corps, n.fontWeight);
				} else
					p.Text(r, name ? name : "Texte", roleTexte, al);
				return;
			}

			// ⚠️ `card_bg` EST UN JETON, PAS UN ROLE -- et c'est la cause du
			//    magenta que Rodolf voyait au milieu de la toile. Tous les autres
			//    sites passent par `instance.TokenRole("card_bg")`, qui traduit le
			//    jeton en role ; celui-ci passait le nom du JETON directement a
			//    `Role()`, qui ne connait que des roles. Resultat : NON RESOLU,
			//    donc magenta -- exactement ce que le repli franc doit faire.
			//
			//    Une forme posee n'a PAS d'instance de composant, donc pas de
			//    table de jetons : elle ne peut pas traduire. Il lui faut un vrai
			//    role, et on prend celui que `card_bg` DECLARE lui-meme comme
			//    defaut -- `InputBg`, dans `NkContentBrowserModel.h`, « fond
			//    de la vignette ». Ce n'est donc pas mon gout : c'est la reponse
			//    que la declaration donnait deja.
			p.Outline(r, host.Role("border"), host.Role("input_bg"), 1.f);
			if (!name || !*name)
				return;
			NkPaintRect label = r;
			label.x += 6.f;
			label.w -= 12.f;
			label.h = p.LineHeight();
			label.y += 4.f;
			p.Text(label, name, host.Role("text"), NkTextAlign::Left);
		}

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
			// Les jetons du mixte (2026-08-30).
			s.chipBg = host.Role(n.instance.TokenRole("chip_bg"));
			s.badgeText = host.Role(n.instance.TokenRole("badge_text"));
			s.statusBg = host.Role(n.instance.TokenRole("status_bg"));
			// Les chevrons de l'arbre de dossiers embarque — MEMES poignees que
			// l'arbre autonome : c'est la ligne qui manquait au tree_view le 29/08
			// (« il demandait ses icones, personne ne lui en donnait »), on ne la
			// re-paie pas ici.
			s.treeIcons = NkDesignTreeIcons();
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
	/// LE DOCUMENT DE DEMONSTRATION -- une fonction LIBRE, et c est le point.
	///
	/// ⚠️ IL VIVAIT DANS `DesignState`, DONC HORS DE PORTEE DU BANC. Ecrire
	///    dans le banc une copie de ce document aurait mesure la copie : la
	///    lecon de T5, « au moins un cote doit venir d ailleurs que du code
	///    teste ». Le controle 41d mesure donc CE document-ci, celui que
	///    Ctrl+N pose reellement.
	inline void NkBuildDemoDocument(NkUIDocument &doc) {
			doc.NewDocument("Interface de demonstration", NkAuthor::Humain);
			doc.nodes[0].layout.kind = NkLayoutKind::Column;

			const int32 header = doc.AddChild(0, "", NkAuthor::Humain);
			if (doc.IsValidIndex(header)) {
				doc.nodes[(uint32)header].label = NkString("Entete");
				doc.nodes[(uint32)header].height.mode = NkSizeMode::Fixed;
				doc.nodes[(uint32)header].height.value = 56.f;
				doc.nodes[(uint32)header].layout.kind = NkLayoutKind::Row;
			}
			// ⚠️ UNE PAGE DE TOILE, ET C EST LA REPONSE A « JE NE VOIS AUCUNE
			//    TOILE ». Mesure du 2026-08-28 : le document de demonstration
			//    ne portait QUE des agencements `column` et `row` -- donc
			//    AUCUN noeud pose a des coordonnees, donc rien a montrer. La
			//    machinerie etait branchee (le dessin et les rectangles
			//    publies passent par la projection ecran) et elle n avait
			//    simplement aucun contenu a projeter.
			//
			//    **Aucune de mes trois etapes ne s etait verifiee A L ECRAN.**
			//    Elles etaient mesurees a 126 controles sans fenetre ; il a
			//    fallu que quelqu un lance le binaire pour voir qu il n y
			//    avait rien a voir.
			const int32 page = doc.AddChild(0, "", NkAuthor::Humain);
			{
				doc.nodes[(uint32)page].label = NkString("Toile");
				doc.nodes[(uint32)page].layout.kind = NkLayoutKind::Free;
				doc.nodes[(uint32)page].height.mode = NkSizeMode::Fixed;
				doc.nodes[(uint32)page].height.value = 200.f;
				const int32 f1 = doc.AddChild(page, "", NkAuthor::Humain);
				doc.nodes[(uint32)f1].label = NkString("Forme A (95, 30)");
				doc.nodes[(uint32)f1].posX = 95.f;
				doc.nodes[(uint32)f1].posY = 30.f;
				doc.nodes[(uint32)f1].width.mode = NkSizeMode::Fixed;
				doc.nodes[(uint32)f1].width.value = 220.f;
				doc.nodes[(uint32)f1].height.mode = NkSizeMode::Fixed;
				doc.nodes[(uint32)f1].height.value = 90.f;
				const int32 f2 = doc.AddChild(page, "", NkAuthor::Humain);
				doc.nodes[(uint32)f2].label = NkString("Forme B (420, 80)");
				doc.nodes[(uint32)f2].posX = 420.f;
				doc.nodes[(uint32)f2].posY = 80.f;
				doc.nodes[(uint32)f2].width.mode = NkSizeMode::Fixed;
				doc.nodes[(uint32)f2].width.value = 160.f;
				doc.nodes[(uint32)f2].height.mode = NkSizeMode::Fixed;
				doc.nodes[(uint32)f2].height.value = 60.f;
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
	}

	/// LE DOCUMENT VIERGE — ce que « Nouveau projet > Vierge » crée (§3 :
	/// « canvas infini vide, une page "Page 1" créée par défaut »). La racine est
	/// la TOILE (agencement `Free` : on y POSE) ; « Page 1 » est un ARTBOARD
	/// (`forme = frame`, planche 22.0), lui-même `Free` pour recevoir des formes.
	/// Fonction LIBRE pour la même raison que le document de démonstration : un
	/// banc mesure LE document que le geste pose, pas une copie écrite dans le banc.
	inline void NkBuildBlankDocument(NkUIDocument &doc) {
			doc.NewDocument("Sans titre", NkAuthor::Humain);
			doc.nodes[0].label = NkString("Toile");
			doc.nodes[0].layout.kind = NkLayoutKind::Free;
			const int32 page = doc.AddChild(0, "", NkAuthor::Humain);
			if (doc.IsValidIndex(page)) {
				NkUINode &n = doc.nodes[(uint32)page];
				n.label = NkString("Page 1");
				n.shape = NkString("frame");
				n.layout.kind = NkLayoutKind::Free;
				n.posX = 80.f;
				n.posY = 48.f;
				n.width.mode = NkSizeMode::Fixed;
				n.width.value = 390.f;
				n.height.mode = NkSizeMode::Fixed;
				n.height.value = 844.f;
			}
	}

	inline void NkDrawDocument(NkComponentPaint &p, const NkComponentInput &in, const NkUIDocument &doc,
							   const NkLayoutResult &lay, NkDocumentHost &host, int32 node = 0) {
		if (!doc.IsValidIndex(node) || !lay.Has(node))
			return;
		const NkUINode &n = doc.nodes[(uint32)node];
		const NkPaintRect r = lay.At(node);

		if (n.IsFrame()) {
			// FORME ou CADRE ? Le PARENT le dit -- exactement comme pour la
			// position. Une forme posee se voit ; un cadre d agencement, non.
			// ⚠️ ANCHOR COMPTE COMME FREE ICI (mesure du 31/08, demo_ancrage :
			//    la barre `fond = #4c6fff` sous un parent anchor se dessinait
			//    en cadre invisible). Un enfant ANCRE est une forme posee dont
			//    seul le calcul de position differe -- son apparence se peint.
			const bool posee =
				n.parent >= 0
				&& (doc.nodes[(uint32)n.parent].layout.kind == NkLayoutKind::Free
					|| doc.nodes[(uint32)n.parent].layout.kind == NkLayoutKind::Anchor);
			// ⚠️ Le noeud en cours d'EDITION EN PLACE ne dessine pas son texte :
			//    le champ superpose TRANSPARENT le dessine a sa place — les deux
			//    ensemble donneraient un double trait (regle du 31/08).
			if (posee && node != host.editionNode)
				renderdetail::DrawShape(p, r, n, host);
			else if (posee && host.editionEtiquette) {
				// RENOMMAGE d'etiquette : le CORPS de l'artboard se dessine,
				// seule l'etiquette se tait (le champ superpose la remplace).
				p.Fill(r, host.Role("artboard_bg"));
				p.OutlineSharp(r, host.Role("border"));
			} else if (!posee)
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
