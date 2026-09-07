#pragma once
// -----------------------------------------------------------------------------
// @File    Renderers.h
// @Brief   DESSINER UN DOCUMENT : du nom declare vers la fonction qui peint.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
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

#include "ComposantsBase.h" // les composants de base : declares ET dessines
#include "Sommets.h" // la table UNIQUE des sommets (peintre + mode points)
#include "NKMath/NkEarcut.h" // les morceaux convexes d'un trace edite (Q94) : la porte sans allocation
#include "Transfo.h" // rotation et miroirs : LE MEME calcul pour le dessin et le clic
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
		// ── LE RESOLVEUR COURANT : pose par celui qui dessine, explicite ─────────
		/// Une couleur peut etre une REFERENCE (« @cle ») a une variable du document.
		/// Le peintre ne connait pas le document : celui qui dessine POSE le
		/// resolveur (NkDrawDocument a la racine ; l'inspecteur avant ses apercus).
		/// Une reference vers une variable absente rend MAGENTA et se note --
		/// jamais silencieusement noire.
		struct NkResolveurCouleur {
				const NkUIDocument *doc = nullptr;
				const char *derniereAbsente = nullptr; ///< la cle de la derniere reference non trouvee
		};
		inline NkResolveurCouleur &NkResolveurCourant() {
			static NkResolveurCouleur r;
			return r;
		}
		inline void NkPoserResolveur(const NkUIDocument *doc) {
			NkResolveurCourant().doc = doc;
			NkResolveurCourant().derniereAbsente = nullptr;
		}
		// ── LE FOURNISSEUR D'IMAGES : pose par l'application (son cache), explicite ──
		/// Le peintre ne charge rien : il DEMANDE une source (handle du dorsal, taille,
		/// pixels) a celui qui la possede. Absente -> faux : le peintre peint le damier
		/// et NOTE le chemin, l'inspecteur le dit. Sans fournisseur (une sonde nue) :
		/// tout est absent, et c'est dit de la meme facon.
		struct NkImageSource {
				nkentseu::uint32 handle = 0;
				nkentseu::int32 w = 0, h = 0;
				const nkentseu::uint8 *pixels = nullptr;
		};
		typedef bool (*NkObtenirImageFn)(void *user, const char *chemin, NkImageSource &out);
		struct NkFournisseurImages {
				NkObtenirImageFn obtenir = nullptr;
				void *user = nullptr;
				const char *derniereAbsente = nullptr; ///< le chemin de la derniere image non trouvee
		};
		inline NkFournisseurImages &NkFournisseurCourant() {
			static NkFournisseurImages f;
			return f;
		}
		inline void NkPoserFournisseurImages(NkObtenirImageFn fn, void *user) {
			NkFournisseurCourant().obtenir = fn;
			NkFournisseurCourant().user = user;
			NkFournisseurCourant().derniereAbsente = nullptr;
		}

		/// La couleur d'un texte de couleur du document : litteral ou reference.
		inline nkentseu::uint32 NkGCouleur(const char *c) {
			if (!NkEstReference(c))
				return NkGHexRGBA(c);
			NkResolveurCouleur &r = NkResolveurCourant();
			const char *v = r.doc ? r.doc->ResoudreCouleur(c) : nullptr;
			if (!v) {
				r.derniereAbsente = c;
				return 0xFF00FFFFu; // MAGENTA : la variable est absente, et ca se voit
			}
			return NkGHexRGBA(v);
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
			const nkentseu::uint32 rgba = NkGCouleur(c);
			const nkentseu::float32 o = n.FondOpacite();
			if (o >= 100.f)
				return rgba;
			const nkentseu::float32 k = o < 0.f ? 0.f : o * 0.01f;
			const nkentseu::uint32 a = (nkentseu::uint32)((rgba & 0xFFu) * k + 0.5f);
			return (rgba & 0xFFFFFF00u) | (a & 0xFFu);
		}

		/// LES QUATRE RAYONS EFFECTIFS d'un nœud, dans l'ordre horaire depuis le
		/// haut-gauche. Passe par `RayonCoin` — la porte, jamais `radius` en direct.
		inline void NkGRayons(const NkUINode &n, nkentseu::float32 out[4]) {
			for (nkentseu::uint32 i = 0; i < 4u; ++i)
				out[i] = n.RayonCoin(i);
		}

		/// BORNER QUATRE RAYONS CONTRE LA BOÎTE — la règle, en un seul endroit.
		///
		/// 🔴 ELLE MANQUAIT, ET RODOLF L'A PHOTOGRAPHIÉE : un bouton de 36 px de
		///    haut portait un arrondi de 50,50 au coin haut-droit — presque trois
		///    fois le maximum possible. Les arcs se chevauchaient, la géométrie se
		///    repliait, et le bord droit partait en oblique.
		///
		/// ⚠️ ON BORNE AU DESSIN, JAMAIS À LA SAISIE. La valeur voulue reste dans
		///    le document : si Rodolf agrandit le bouton, son 50,50 revient tout
		///    seul. *Un réglage écrasé à la saisie est une donnée perdue ; un
		///    réglage borné au dessin est une donnée respectée.*
		///
		/// DEUX BORNES, ET LA SECONDE EST CELLE QU'ON OUBLIE :
		///   1. chaque rayon ≤ la moitié de la plus petite dimension ;
		///   2. **deux coins d'un MÊME BORD ne peuvent pas totaliser plus que ce
		///      bord** — c'est la règle CSS, et c'est elle qui empêche les
		///      chevauchements en biais quand les quatre rayons sont libres. La
		///      première seule laisse passer 18 + 18 sur un bord de 30.
		inline void NkGBornerRayons(nkentseu::float32 w, nkentseu::float32 h,
									const nkentseu::float32 R[4], nkentseu::float32 out[4]) {
			const nkentseu::float32 demi = (w < h ? w : h) * 0.5f;
			for (nkentseu::uint32 i = 0; i < 4u; ++i) {
				const nkentseu::float32 v = R[i] < 0.f ? 0.f : R[i];
				out[i] = v > demi ? demi : v;
			}
			// La règle CSS : le facteur d'échelle le plus contraignant des quatre
			// bords s'applique à TOUS les rayons — sinon deux bords voisins se
			// borneraient l'un après l'autre et le résultat dépendrait de l'ordre.
			nkentseu::float32 k = 1.f;
			const nkentseu::float32 bords[4][3] = {
				{out[0] + out[1], w, 0.f}, // haut
				{out[1] + out[2], h, 0.f}, // droite
				{out[3] + out[2], w, 0.f}, // bas
				{out[0] + out[3], h, 0.f}, // gauche
			};
			for (nkentseu::uint32 b = 0; b < 4u; ++b) {
				const nkentseu::float32 somme = bords[b][0];
				const nkentseu::float32 cote = bords[b][1];
				if (somme > cote && somme > 0.f) {
					const nkentseu::float32 kb = cote / somme;
					if (kb < k)
						k = kb;
				}
			}
			if (k < 1.f)
				for (nkentseu::uint32 i = 0; i < 4u; ++i)
					out[i] *= k;
		}

		/// UN RECTANGLE À QUATRE RAYONS DIFFÉRENTS, composé depuis `FillColor`.
		///
		/// 🔴 POURQUOI LE COMPOSER ICI PLUTÔT QUE L'AJOUTER AU NOYAU : NKGui
		///    n'expose qu'UN rayon (`AddRectFilled(r, col, rounding)`) et le
		///    noyau est tenu par un autre agent ce soir. Composer dans
		///    l'application donne le résultat exact sans toucher une ligne
		///    partagée — et le jour où le noyau saura les quatre coins, c'est
		///    CETTE fonction qui change, pas ses vingt appelants.
		///
		/// La composition : quatre carrés d'angle (chacun arrondi à SON rayon)
		/// plus deux bandes croisées qui remplissent le milieu. Les trois coins
		/// parasites de chaque carré tombent sous les bandes.
		///
		/// ⚠️ LE CHEMIN UNIFORME EST PRÉSERVÉ : quand les quatre rayons sont
		///    égaux, on appelle `FillColor` une seule fois, exactement comme
		///    avant. Sans ça, tout document existant aurait changé de flux de
		///    commandes — le témoin aurait crié sur un dessin identique.
		/// LE CONTOUR ARRONDI d'un rectangle, en polygone (sens horaire a l'ecran),
		/// `seg` segments par arc ; rayons deja bornes (NkGBornerRayons). Rend le
		/// nombre de points ; `out` recoit 2 floats par point (au plus 4*(seg+1)).
		// ════════════════════════════════════════════════════════════════════
		//  LE CALCUL D'UN DEGRADE — « solide et robuste » (Rodolf, 04/09)
		// ════════════════════════════════════════════════════════════════════
		/// 🔑 L'ESPACE D'INTERPOLATION EST NOMME : **sRGB direct**, sur les valeurs
		///    telles qu'elles sont stockees — c'est ce que font Lunacy et Figma par
		///    defaut. Un espace non dit est une convention cachee, et on en a deja
		///    paye une cette semaine avec l'angle. (Lunacy propose LCH/OKLCH/OKLAB
		///    comme MODELES DE SAISIE — capture 04/09 — pas comme espace de melange :
		///    le jour ou l'un d'eux servira au melange, c'est ICI que ca se change.)
		///
		/// ⚠️ ET L'INTERPOLATION SE FAIT SUR LA COULEUR PREMULTIPLIEE des qu'une
		///    opacite varie. « Rouge opaque -> transparent » interpole naivement
		///    passe par du GRIS SALE, parce qu'on tire vers un rgba(0,0,0,0) dont
		///    les composantes ne veulent rien dire. Premultiplie, le rouge reste
		///    rouge en s'effacant. C'est le defaut le plus visible d'un degrade mal
		///    fait, et il est INVISIBLE sur un banc qui n'essaie que des arrets
		///    opaques — la sonde 53 en contient un transparent.
		struct NkArretsTries {
				enum { kMax = 32 };
				nkentseu::uint32 ordre[kMax] = {};
				nkentseu::uint32 nb = 0;
				/// Les arrets par POSITION CROISSANTE — jamais l'ordre du fichier :
				/// l'utilisateur glisse un arret par-dessus un autre, et le document
				/// garde l'ordre de saisie. Tri par insertion, stable : deux arrets a
				/// la MEME position gardent leur ordre relatif, et c'est ce qui fait la
				/// COUPURE FRANCHE (une fonctionnalite, pas un bug).
				explicit NkArretsTries(const NkDegrade &g) {
					for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)g.arrets.Size() && nb < kMax; ++i) {
						nkentseu::uint32 k = nb;
						while (k > 0 && g.arrets[ordre[k - 1]].position > g.arrets[i].position) {
							ordre[k] = ordre[k - 1];
							--k;
						}
						ordre[k] = i;
						++nb;
					}
				}
		};
		/// La couleur d'un degrade en `t` (0..1), en RGBA (alpha dans l'octet bas).
		/// Zero arret : transparent (l'appelant ne doit pas appeler). UN arret :
		/// SA couleur partout — un uni, pas une division par zero. Avant le premier
		/// et apres le dernier : la couleur du bord, jamais du noir.
		inline nkentseu::uint32 NkCouleurDegradeEn(const NkDegrade &g, nkentseu::float32 t) {
			const NkArretsTries tri(g);
			if (tri.nb == 0u)
				return 0u;
			auto rgbaDe = [&](nkentseu::uint32 i) -> nkentseu::uint32 {
				const NkArretDegrade &a = g.arrets[i];
				const nkentseu::uint32 base = NkGCouleur(a.couleur.Data());
				const nkentseu::float32 op =
					(a.opacite < 0.f ? 0.f : (a.opacite > 100.f ? 100.f : a.opacite)) * 0.01f;
				const nkentseu::uint32 al = (nkentseu::uint32)((nkentseu::float32)(base & 0xFFu) * op + 0.5f);
				return (base & 0xFFFFFF00u) | (al & 0xFFu);
			};
			if (tri.nb == 1u)
				return rgbaDe(tri.ordre[0]);
			const nkentseu::float32 p0 = g.arrets[tri.ordre[0]].position;
			const nkentseu::float32 pN = g.arrets[tri.ordre[tri.nb - 1u]].position;
			if (t <= p0)
				return rgbaDe(tri.ordre[0]);
			if (t >= pN)
				return rgbaDe(tri.ordre[tri.nb - 1u]);
			nkentseu::uint32 s = 0;
			while (s + 2u < tri.nb && g.arrets[tri.ordre[s + 1u]].position < t)
				++s;
			const nkentseu::uint32 A = tri.ordre[s], B = tri.ordre[s + 1u];
			const nkentseu::float32 pa = g.arrets[A].position, pb = g.arrets[B].position;
			const nkentseu::float32 span = pb - pa;
			nkentseu::float32 k = span > 0.0001f ? (t - pa) / span : 1.f; // doublon = coupure franche
			if (k < 0.f)
				k = 0.f;
			if (k > 1.f)
				k = 1.f;
			const nkentseu::uint32 ca = rgbaDe(A), cb = rgbaDe(B);
			const nkentseu::float32 aa = (nkentseu::float32)(ca & 0xFFu) / 255.f;
			const nkentseu::float32 ab = (nkentseu::float32)(cb & 0xFFu) / 255.f;
			const nkentseu::float32 al = aa + (ab - aa) * k;
			nkentseu::uint32 out = 0u;
			for (nkentseu::int32 dec = 24; dec >= 8; dec -= 8) {
				const nkentseu::float32 va = (nkentseu::float32)((ca >> dec) & 0xFFu) * aa; // premultipliee
				const nkentseu::float32 vb = (nkentseu::float32)((cb >> dec) & 0xFFu) * ab;
				nkentseu::float32 v = va + (vb - va) * k;
				if (al > 0.0001f)
					v /= al; // et on revient en couleur droite pour le peintre
				if (v < 0.f)
					v = 0.f;
				if (v > 255.f)
					v = 255.f;
				out |= ((nkentseu::uint32)(v + 0.5f)) << dec;
			}
			return out | ((nkentseu::uint32)(al * 255.f + 0.5f) & 0xFFu);
		}
		/// AJOUTER UN ARRET EN `t` : sa couleur et son opacite sont celles que le
		/// degrade a DEJA a cet endroit -- l'arret nait invisible, et c'est le
		/// geste suivant qui le colore. Rend son indice, ou -1 si la liste est
		/// pleine.
		///
		/// 🔑 UNE SEULE FONCTION POUR DEUX GESTES : le clic sur la barre du
		///    popover et (quand la toile viendra) le clic sur le SEGMENT du
		///    degrade, que Rodolf a etabli le 04/09. Deux copies auraient
		///    diverge au premier ajustement.
		inline nkentseu::int32 NkAjouterArretDegrade(NkDegrade &g, nkentseu::float32 t,
													 nkentseu::uint32 maxArrets = 32u) {
			if ((nkentseu::uint32)g.arrets.Size() >= maxArrets)
				return -1;
			if (t < 0.f)
				t = 0.f;
			if (t > 1.f)
				t = 1.f;
			const nkentseu::uint32 c = NkCouleurDegradeEn(g, t);
			char hx[12];
			snprintf(hx, sizeof(hx), "#%02x%02x%02x", (nkentseu::uint32)((c >> 24) & 0xFFu),
					 (nkentseu::uint32)((c >> 16) & 0xFFu), (nkentseu::uint32)((c >> 8) & 0xFFu));
			NkArretDegrade ar;
			ar.position = t;
			ar.couleur = NkString(hx);
			ar.opacite = g.Actif() ? (nkentseu::float32)(c & 0xFFu) * 100.f / 255.f : 100.f;
			g.arrets.PushBack(ar);
			return (nkentseu::int32)g.arrets.Size() - 1;
		}
		// ════════════════════════════════════════════════════════════════════
		//  LES POIGNEES DE DEGRADE SUR LA TOILE — la geometrie, UNE fois
		// ════════════════════════════════════════════════════════════════════
		/// L'AXE d'un degrade lineaire dans le repere du rectangle : du point 0 %
		/// au point 100 %. Meme convention d'angle que le peintre (0 = haut -> bas,
		/// horaire), meme etendue (w|sin| + h|cos|) : ce que la poignee montre est
		/// exactement ce que la bande peint.
		struct NkAxeDegrade {
				nkentseu::float32 ax = 0.f, ay = 0.f, bx = 0.f, by = 0.f;
		};
		inline NkAxeDegrade NkAxeDegradeDe(const NkPaintRect &r, const NkDegrade &g) {
			nkentseu::float32 s = 0.f, c = 1.f;
			NkSinCosDeg(g.angle, s, c);
			const nkentseu::float32 dx = -s, dy = c;
			const nkentseu::float32 et = r.w * (dx < 0.f ? -dx : dx) + r.h * (dy < 0.f ? -dy : dy);
			const nkentseu::float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
			NkAxeDegrade a;
			a.ax = cx - dx * et * 0.5f;
			a.ay = cy - dy * et * 0.5f;
			a.bx = cx + dx * et * 0.5f;
			a.by = cy + dy * et * 0.5f;
			return a;
		}
		/// La poignee de l'arret `t` sur l'axe.
		inline void NkPoigneeDegrade(const NkAxeDegrade &a, nkentseu::float32 t, nkentseu::float32 &x,
									 nkentseu::float32 &y) {
			x = a.ax + (a.bx - a.ax) * t;
			y = a.ay + (a.by - a.ay) * t;
		}
		/// Le parametre (0..1) du point le plus proche sur l'axe -- ce qu'un glisser ecrit.
		inline nkentseu::float32 NkParamSurAxeDegrade(const NkAxeDegrade &a, nkentseu::float32 px,
													  nkentseu::float32 py) {
			const nkentseu::float32 vx = a.bx - a.ax, vy = a.by - a.ay;
			const nkentseu::float32 l2 = vx * vx + vy * vy;
			if (l2 <= 0.0001f)
				return 0.f;
			nkentseu::float32 t = ((px - a.ax) * vx + (py - a.ay) * vy) / l2;
			if (t < 0.f)
				t = 0.f;
			if (t > 1.f)
				t = 1.f;
			return t;
		}
		/// 🔑 QUI PREND LE CLIC -- UNE SEULE DECISION, ORDONNEE : la poignee la plus
		///    proche si le point est a moins de `tolPoignee` d'elle (>= 0 : son
		///    indice) ; SINON le segment s'il est a moins de `tolSegment` (-2 :
		///    ajouter un arret ici) ; sinon rien (-1 : la toile). Ecrite ainsi, et
		///    pas comme deux tests independants, pour qu'une tentative de saisir une
		///    poignee ne cree jamais un arret parasite.
		inline nkentseu::int32 NkQuiPrendLeClicDegrade(const NkAxeDegrade &a, const NkDegrade &g,
													   nkentseu::float32 px, nkentseu::float32 py,
													   nkentseu::float32 tolPoignee,
													   nkentseu::float32 tolSegment) {
			nkentseu::int32 meilleur = -1;
			nkentseu::float32 d2min = tolPoignee * tolPoignee;
			for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)g.arrets.Size(); ++i) {
				nkentseu::float32 hx = 0.f, hy = 0.f;
				NkPoigneeDegrade(a, g.arrets[i].position, hx, hy);
				const nkentseu::float32 d2 = (px - hx) * (px - hx) + (py - hy) * (py - hy);
				if (d2 <= d2min) {
					d2min = d2;
					meilleur = (nkentseu::int32)i;
				}
			}
			if (meilleur >= 0)
				return meilleur;
			const nkentseu::float32 t = NkParamSurAxeDegrade(a, px, py);
			nkentseu::float32 sx = 0.f, sy = 0.f;
			NkPoigneeDegrade(a, t, sx, sy);
			const nkentseu::float32 d2 = (px - sx) * (px - sx) + (py - sy) * (py - sy);
			return d2 <= tolSegment * tolSegment ? -2 : -1;
		}
		/// Le remplissage dont les poignees se montrent : le PLUS HAUT visible qui
		/// porte un degrade LINEAIRE (le seul peint -- une poignee sur un degrade
		/// invisible serait un dessin faux). -1 sinon.
		/// ① LA GEOMETRIE d'un degrade radial / angulaire / losange dans le repere du
		///    rectangle : l'origine et les rayons en PIXELS. UNE geometrie, lue par le
		///    peintre et par le pointage -- ce que la poignee montre est ce que la bande peint.
		struct NkGeomDegrade {
				nkentseu::float32 ox = 0.f, oy = 0.f, rx = 1.f, ry = 1.f;
		};
		inline NkGeomDegrade NkGeomDegradeDe(const NkPaintRect &r, const NkDegrade &g) {
			NkGeomDegrade m;
			m.ox = r.x + r.w * g.origineX;
			m.oy = r.y + r.h * g.origineY;
			m.rx = r.w * (g.rayonX < 0.02f ? 0.02f : g.rayonX);
			m.ry = r.h * (g.rayonY < 0.02f ? 0.02f : g.rayonY);
			return m;
		}
		// ════════════════════════════════════════════════════════════════════
		//  UN GESTE POUR LES QUATRE GENRES (Rodolf, 04/09 soir, huit captures radial)
		//  Le genre ne dit que OU sont les pastilles : l'AXE. Lineaire : du point 0 %
		//  au point 100 % a travers le centre (NkAxeDegradeDe). Radial, losange,
		//  angulaire : de l'ORIGINE vers le bord, dans la direction de l'angle, sur la
		//  longueur du rayon Y. Les verbes sont les memes partout : DANS le disque
		//  d'une pastille on GLISSE le long de l'axe ; dans l'ANNEAU autour on TOURNE
		//  l'axe autour du centre / de l'origine ; le centre deplace l'origine.
		// ════════════════════════════════════════════════════════════════════
		static constexpr nkentseu::float32 kNkDisquePoignee = 8.f;  ///< le rayon du disque saisissable (couvre ce qui est dessine : 7 + 1)
		static constexpr nkentseu::float32 kNkAnneauPoignee = 10.f; ///< l'anneau de rotation, juste autour du disque
		static constexpr nkentseu::float32 kNkTolSegment = 5.f;	 ///< la distance au segment qui ajoute un arret
		inline NkAxeDegrade NkAxeDegradeGenre(nkentseu::int32 genre, const NkPaintRect &r, const NkDegrade &g) {
			if (genre == 0)
				return NkAxeDegradeDe(r, g);
			const NkGeomDegrade m = NkGeomDegradeDe(r, g);
			nkentseu::float32 s = 0.f, c = 1.f;
			NkSinCosDeg(g.angle, s, c);
			NkAxeDegrade a;
			a.ax = m.ox;
			a.ay = m.oy;
			a.bx = m.ox - s * m.ry;
			a.by = m.oy + c * m.ry;
			return a;
		}
		/// ⑤ LE « OU » D'UNE PASTILLE D'ARRET, par genre : sur l'axe (lineaire, radial, losange)
		///    ou SUR LE CERCLE de portee pour l'angulaire (fraction de tour depuis l'axe,
		///    horaire -- captures de Rodolf). C'est la seule chose que le genre change.
		inline void NkPoigneeDegradeGenre(nkentseu::int32 genre, const NkPaintRect &r, const NkDegrade &g,
										  nkentseu::float32 t, nkentseu::float32 &x, nkentseu::float32 &y) {
			if (genre == 2) {
				const NkGeomDegrade m = NkGeomDegradeDe(r, g);
				nkentseu::float32 s = 0.f, c = 1.f;
				NkSinCosDeg(g.angle + 360.f * t, s, c);
				x = m.ox - s * m.ry;
				y = m.oy + c * m.ry;
				return;
			}
			NkPoigneeDegrade(NkAxeDegradeGenre(genre, r, g), t, x, y);
		}
		/// Le parametre (0..1) qu'un glisser ecrit : le long de l'axe, ou, pour l'angulaire, la
		/// fraction de tour du pointeur depuis l'axe (jamais au-dela d'un tour).
		inline nkentseu::float32 NkParamDegradeGenre(nkentseu::int32 genre, const NkPaintRect &r, const NkDegrade &g,
													 nkentseu::float32 px, nkentseu::float32 py) {
			if (genre == 2) {
				const NkGeomDegrade m = NkGeomDegradeDe(r, g);
				nkentseu::float32 a = nkentseu::math::NkAtan2(-(px - m.ox), py - m.oy) * 57.2957795f - g.angle;
				while (a < 0.f)
					a += 360.f;
				while (a >= 360.f)
					a -= 360.f;
				return a / 360.f;
			}
			return NkParamSurAxeDegrade(NkAxeDegradeGenre(genre, r, g), px, py);
		}
		/// Les deux carres de l'angulaire, SUR le cercle, a +90 et +135 degres de l'axe : ils
		/// reglent le rayon et tournent avec l'axe (`axe_tourne_carres_suivent`).
		inline void NkCarreAngulaire(const NkPaintRect &r, const NkDegrade &g, nkentseu::int32 k, nkentseu::float32 &x,
									 nkentseu::float32 &y) {
			const NkGeomDegrade m = NkGeomDegradeDe(r, g);
			nkentseu::float32 s = 0.f, c = 1.f;
			NkSinCosDeg(g.angle + (k == 0 ? 90.f : 135.f), s, c);
			x = m.ox - s * m.ry;
			y = m.oy + c * m.ry;
		}
		/// Le point autour duquel l'axe TOURNE : le centre du rectangle (lineaire) ou l'origine.
		inline void NkPivotDegrade(nkentseu::int32 genre, const NkPaintRect &r, const NkDegrade &g, nkentseu::float32 &x,
								   nkentseu::float32 &y) {
			if (genre == 0) {
				x = r.x + r.w * 0.5f;
				y = r.y + r.h * 0.5f;
			} else {
				const NkGeomDegrade m = NkGeomDegradeDe(r, g);
				x = m.ox;
				y = m.oy;
			}
		}
		/// L'angle de l'axe qui pointe du pivot vers (px, py) -- pour une pastille a t < 0,5
		/// du lineaire, l'axe pointe a l'oppose (c'est le bout 0 % qu'on tient).
		inline nkentseu::float32 NkAngleAxeVers(nkentseu::int32 genre, nkentseu::float32 t, nkentseu::float32 vx,
												 nkentseu::float32 vy) {
			if (genre == 0 && t < 0.5f) {
				vx = -vx;
				vy = -vy;
			}
			nkentseu::float32 a = nkentseu::math::NkAtan2(-vx, vy) * 57.2957795f;
			while (a < 0.f)
				a += 360.f;
			while (a >= 360.f)
				a -= 360.f;
			return a;
		}
		/// L'AIMANT de rotation (captures `aimant_0_guide_rouge`) : a moins de 4 degres d'un
		/// multiple de 45, l'angle s'y colle -- 0 et 90 d'abord, comme Lunacy.
		inline nkentseu::float32 NkAimantAngle(nkentseu::float32 a, bool &aimante) {
			aimante = false;
			for (nkentseu::int32 k = 0; k <= 8; ++k) {
				const nkentseu::float32 c = 45.f * (nkentseu::float32)k;
				const nkentseu::float32 d = a - c;
				if ((d < 0.f ? -d : d) <= 4.f) {
					aimante = true;
					return k == 8 ? 0.f : c;
				}
			}
			return a;
		}
		/// 🔑 QUI PREND LE CLIC, ET DANS QUELLE ZONE -- UNE decision ordonnee, pour les
		///    quatre genres : le DISQUE d'une pastille d'arret (>= 0, zone 0 : glisser), le
		///    disque du centre (-3, zone 0 : deplacer l'origine), le carre du cote (-4) /
		///    du haut (-5) (zone 0 : les rayons), puis l'ANNEAU d'une pastille d'arret
		///    (>= 0, zone 1 : tourner), puis le segment (-2 : ajouter un arret), sinon la
		///    toile (-1). Le centre prime sur un arret pose dessus (le 0 % y est).
		inline nkentseu::int32 NkPointageDegrade(nkentseu::int32 genre, const NkPaintRect &r, const NkDegrade &g,
												 nkentseu::float32 px, nkentseu::float32 py, nkentseu::int32 &zone,
												 bool anneaux = true, nkentseu::float32 *distance2 = nullptr) {
			zone = 0;
			if (distance2)
				*distance2 = 1e30f;
			const NkAxeDegrade a = NkAxeDegradeGenre(genre, r, g);
			const nkentseu::float32 d2Disque = kNkDisquePoignee * kNkDisquePoignee;
			const nkentseu::float32 rAnneau = kNkDisquePoignee + kNkAnneauPoignee;
			auto d2 = [&](nkentseu::float32 x, nkentseu::float32 y) { return (px - x) * (px - x) + (py - y) * (py - y); };
			auto rendre = [&](nkentseu::int32 code, nkentseu::float32 q) {
				if (distance2)
					*distance2 = q;
				return code;
			};
			nkentseu::int32 meilleur = -1;
			nkentseu::float32 d2min = d2Disque;
			for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)g.arrets.Size(); ++i) {
				nkentseu::float32 hx = 0.f, hy = 0.f;
				NkPoigneeDegradeGenre(genre, r, g, g.arrets[i].position, hx, hy);
				const nkentseu::float32 q = d2(hx, hy);
				if (q <= d2min) {
					d2min = q;
					meilleur = (nkentseu::int32)i;
				}
			}
			if (genre != 0) {
				// LE CENTRE : il prime sur l'arret POSE DESSUS (le 0 %), jamais sur un arret
				// voisin plus proche du pointeur (a petit zoom l'axe fait douze pixels, et le
				// milieu tombait dans le disque du centre -- mesure, sonde 60k)
				const NkGeomDegrade m = NkGeomDegradeDe(r, g);
				const nkentseu::float32 dc = d2(m.ox, m.oy);
				if (dc <= d2Disque && (meilleur < 0 || g.arrets[(nkentseu::uint32)meilleur].position <= 0.03f || dc < d2min))
					return rendre(-3, dc);
			}
			if (meilleur >= 0)
				return rendre(meilleur, d2min);
			if (genre == 2) {
				// ⑤ les deux carres SUR le cercle (rayon) -- ils tournent avec l'axe
				for (nkentseu::int32 k = 0; k < 2; ++k) {
					nkentseu::float32 qx = 0.f, qy = 0.f;
					NkCarreAngulaire(r, g, k, qx, qy);
					if (d2(qx, qy) <= d2Disque)
						return rendre(k == 0 ? -4 : -5, d2(qx, qy));
				}
			} else if (genre != 0) {
				const NkGeomDegrade m = NkGeomDegradeDe(r, g);
				if (d2(m.ox + m.rx, m.oy) <= d2Disque)
					return rendre(-4, d2(m.ox + m.rx, m.oy));
				if (d2(m.ox, m.oy - m.ry) <= d2Disque)
					return rendre(-5, d2(m.ox, m.oy - m.ry));
			}
			// l'anneau : la pastille la plus proche, si le pointeur est dans sa couronne --
			// seulement quand on l'a demande (popover ouvert sur ce remplissage, regle ⑥)
			if (anneaux) {
				d2min = rAnneau * rAnneau;
				for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)g.arrets.Size(); ++i) {
					nkentseu::float32 hx = 0.f, hy = 0.f;
					NkPoigneeDegradeGenre(genre, r, g, g.arrets[i].position, hx, hy);
					const nkentseu::float32 q = d2(hx, hy);
					if (q <= d2min) {
						d2min = q;
						meilleur = (nkentseu::int32)i;
					}
				}
				if (meilleur >= 0) {
					zone = 1;
					return rendre(meilleur, d2min);
				}
			}
			// le segment (ou, pour l'angulaire, le cercle) : un arret nait ici
			const nkentseu::float32 tt = NkParamDegradeGenre(genre, r, g, px, py);
			nkentseu::float32 sx = 0.f, sy = 0.f;
			NkPoigneeDegradeGenre(genre, r, g, tt, sx, sy);
			const nkentseu::float32 qs = d2(sx, sy);
			return qs <= kNkTolSegment * kNkTolSegment ? rendre(-2, qs) : -1;
		}
		inline nkentseu::int32 NkRemplissageDegradeToile(const NkUINode &n) {
			for (nkentseu::uint32 i = (nkentseu::uint32)n.fills.Size(); i > 0; --i) {
				const NkRemplissage &f = n.fills[i - 1];
				if (!f.visible || !f.degrade.Actif())
					continue;
				return (nkentseu::int32)(i - 1); // ① les quatre genres ont leurs poignees
			}
			return -1;
		}
		/// LE REMPLISSAGE IMAGE RECADRE (cadrage « crop ») qui porte les poignees de
		/// crop sur la toile (05/09) : le dernier visible, -1 sinon.
		inline nkentseu::int32 NkRemplissageImageCropToile(const NkUINode &n) {
			for (nkentseu::uint32 i = (nkentseu::uint32)n.fills.Size(); i > 0; --i) {
				const NkRemplissage &f = n.fills[i - 1];
				if (!f.visible || !f.genre.Data() || !NkComponentDecl::StrEq(f.genre.Data(), "image"))
					continue;
				if (!f.cadrage.Data() || !NkComponentDecl::StrEq(f.cadrage.Data(), "crop"))
					continue;
				return (nkentseu::int32)(i - 1);
			}
			return -1;
		}
		/// L'IMAGE ENTIERE AUTOUR DE LA FENETRE : le rect `r` montre la portion
		/// [cropX, cropX + cropW] x [cropY, cropY + cropH] de l'image ; l'image entiere
		/// est donc le cadre `out`, plus grand, dont `r` est la fenetre.
		/// LA FENETRE QUE « COUVRIR » MONTRE, en coordonnees d'image (0..1) -- le crop
		/// INITIAL quand on passe en cadrage `crop`.
		///
		/// 🔴 POURQUOI ELLE EXISTE (retour de Rodolf, 05/09) : « ce n'est pas modifiable et je
		///    ne sais meme pas si c'est visible ». Passer en Crop posait `crop = 0 0 1 1`,
		///    c'est-a-dire *toute l'image etiree a la boite du noeud* : le cadre de l'image
		///    entiere avait alors EXACTEMENT la geometrie du noeud, ses huit poignees tombaient
		///    SUR celles de la forme, et c'est la forme qui gagnait. Invisible, et le geste
		///    deplacait le noeud.
		/// La relation de Lunacy : l'image garde son echelle, le NOEUD est une FENETRE dessus.
		/// On entre donc en crop sur la fenetre que `fill` (couvrir) montrait deja -- l'ecran ne
		/// change pas d'un pixel a la bascule, et le cadre est plus grand que le noeud des que
		/// l'image et la boite n'ont pas le meme rapport.
		/// ⚠️ MEME RAPPORT = MEME BOITE, et c'est dit : une image 4:3 dans une boite 4:3 rend
		///    `0 0 1 1`, donc un cadre confondu avec le noeud. Rien a inventer la : le crop ne
		///    cache rien, il n'y a rien a tirer.
		inline void NkCropCouvrant(const NkPaintRect &r, nkentseu::float32 iw, nkentseu::float32 ih,
								   nkentseu::float32 out[4]) {
			out[0] = 0.f;
			out[1] = 0.f;
			out[2] = 1.f;
			out[3] = 1.f;
			if (iw <= 0.f || ih <= 0.f || r.w <= 0.f || r.h <= 0.f)
				return;
			const nkentseu::float32 k = (r.w / iw > r.h / ih) ? r.w / iw : r.h / ih; // couvrir
			const nkentseu::float32 dw = iw * k, dh = ih * k;
			const nkentseu::float32 cw = dw > 0.f ? r.w / dw : 1.f, ch = dh > 0.f ? r.h / dh : 1.f;
			out[2] = cw > 1.f ? 1.f : cw;
			out[3] = ch > 1.f ? 1.f : ch;
			out[0] = (1.f - out[2]) * 0.5f;
			out[1] = (1.f - out[3]) * 0.5f;
		}

		inline void NkGCadreImageEntiere(const NkPaintRect &r, const NkRemplissage &f, NkPaintRect &out) {
			const nkentseu::float32 w = f.cropW > 0.001f ? f.cropW : 1.f, h = f.cropH > 0.001f ? f.cropH : 1.f;
			out.w = r.w / w;
			out.h = r.h / h;
			out.x = r.x - f.cropX * out.w;
			out.y = r.y - f.cropY * out.h;
		}
		/// Les huit poignees du cadre : 0..3 les coins (HG, HD, BD, BG), 4..7 les milieux
		/// (H, D, B, G) -- `xy` recoit seize flottants.
		inline void NkPoigneesCrop(const NkPaintRect &e, nkentseu::float32 *xy) {
			const nkentseu::float32 x0 = e.x, x1 = e.x + e.w, y0 = e.y, y1 = e.y + e.h;
			const nkentseu::float32 xm = e.x + e.w * 0.5f, ym = e.y + e.h * 0.5f;
			const nkentseu::float32 p[16] = {x0, y0, x1, y0, x1, y1, x0, y1, xm, y0, x1, ym, xm, y1, x0, ym};
			for (nkentseu::uint32 i = 0; i < 16u; ++i)
				xy[i] = p[i];
		}
		/// Les bits d'aretes d'une poignee de crop, comme ceux des poignees de forme :
		/// 1 = gauche, 2 = droite, 4 = haut, 8 = bas.
		inline nkentseu::uint8 NkPoigneeCropBits(nkentseu::int32 k) {
			static const nkentseu::uint8 b[8] = {1u | 4u, 2u | 4u, 2u | 8u, 1u | 8u, 4u, 2u, 8u, 1u};
			return (k >= 0 && k < 8) ? b[k] : 0u;
		}
		/// LA DECISION : la poignee de crop la plus proche du pointeur a moins de `tol`,
		/// -1 sinon ; `d2` recoit le carre de sa distance (pour l'arbitrage avec les
		/// poignees de forme : la plus proche gagne).
		inline nkentseu::int32 NkPointageCrop(const NkPaintRect &e, nkentseu::float32 mx, nkentseu::float32 my,
											  nkentseu::float32 tol, nkentseu::float32 *d2) {
			nkentseu::float32 p[16];
			NkPoigneesCrop(e, p);
			nkentseu::int32 best = -1;
			nkentseu::float32 bd = tol * tol;
			for (nkentseu::int32 k = 0; k < 8; ++k) {
				const nkentseu::float32 dx = mx - p[k * 2], dy = my - p[k * 2 + 1];
				const nkentseu::float32 q = dx * dx + dy * dy;
				if (q <= bd) {
					bd = q;
					best = k;
				}
			}
			if (d2)
				*d2 = best >= 0 ? bd : 1e30f;
			return best;
		}
		/// LE NOMBRE DE BANDES SUIT LA TAILLE DESSINEE : 24 bandes suffisent pour
		/// une pastille, pas pour un fond de 800 px. Une bande par 2 px, bornee.
		inline nkentseu::int32 NkBandesDegrade(nkentseu::float32 etendue) {
			nkentseu::int32 n = (nkentseu::int32)(etendue * 0.5f + 0.5f);
			if (n < 24)
				n = 24;
			if (n > 192)
				n = 192;
			return n;
		}
		/// DECOUPER un polygone par un polygone CONVEXE (Sutherland-Hodgman, chaque arete du
		/// convexe est un demi-plan). `out` recoit au plus `cap` points ; rend leur nombre.
		/// C'est ce qui fait qu'un disque, un secteur ou un losange SUIT le contour arrondi.
		inline nkentseu::uint32 NkGDecouperParConvexe(const nkentseu::float32 *poly, nkentseu::uint32 np,
													  const nkentseu::float32 *clip, nkentseu::uint32 nc,
													  nkentseu::float32 *out, nkentseu::uint32 cap) {
			if (np < 3u || nc < 3u || cap < 3u)
				return 0u;
			// l'orientation du convexe : le signe de son aire dit de quel cote est l'interieur
			nkentseu::float32 aire = 0.f;
			for (nkentseu::uint32 k = 0; k < nc; ++k) {
				const nkentseu::uint32 j = (k + 1u) % nc;
				aire += clip[k * 2] * clip[j * 2 + 1] - clip[j * 2] * clip[k * 2 + 1];
			}
			const nkentseu::float32 signe = aire >= 0.f ? 1.f : -1.f;
			nkentseu::float32 a[128], b[128];
			nkentseu::uint32 n = np < 64u ? np : 64u;
			for (nkentseu::uint32 k = 0; k < n * 2u; ++k)
				a[k] = poly[k];
			for (nkentseu::uint32 e = 0; e < nc && n >= 3u; ++e) {
				const nkentseu::uint32 j = (e + 1u) % nc;
				const nkentseu::float32 ex = clip[j * 2] - clip[e * 2], ey = clip[j * 2 + 1] - clip[e * 2 + 1];
				auto valeur = [&](nkentseu::float32 x, nkentseu::float32 y) -> nkentseu::float32 {
					return signe * (ex * (y - clip[e * 2 + 1]) - ey * (x - clip[e * 2]));
				};
				nkentseu::uint32 m = 0u;
				for (nkentseu::uint32 k = 0; k < n && m < 62u; ++k) {
					const nkentseu::uint32 q = (k + 1u) % n;
					const nkentseu::float32 ax = a[k * 2], ay = a[k * 2 + 1], bx = a[q * 2], by = a[q * 2 + 1];
					const nkentseu::float32 va = valeur(ax, ay), vb = valeur(bx, by);
					if (va >= 0.f) {
						b[m * 2] = ax;
						b[m * 2 + 1] = ay;
						++m;
					}
					if ((va >= 0.f) != (vb >= 0.f) && m < 62u) {
						const nkentseu::float32 f = va / (va - vb);
						b[m * 2] = ax + (bx - ax) * f;
						b[m * 2 + 1] = ay + (by - ay) * f;
						++m;
					}
				}
				n = m;
				for (nkentseu::uint32 k = 0; k < n * 2u; ++k)
					a[k] = b[k];
			}
			if (n < 3u)
				return 0u;
			const nkentseu::uint32 nOut = n < cap ? n : cap;
			for (nkentseu::uint32 k = 0; k < nOut * 2u; ++k)
				out[k] = a[k];
			return nOut;
		}
		/// ②-2 LE MODE DE FUSION QUE LE GPU DONNE EXACTEMENT : Multiply, Screen, Darken,
		///    Lighten, Plus Lighter -- rend le NkPaintBlend a pousser ; Alpha pour normal
		///    ET pour les treize autres (Overlay, Soft Light... : ils lisent la destination,
		///    ils sont enregistres et dits, pas approximes).
		inline NkComponentPaint::NkPaintBlend NkFusionExacte(const NkString &fusion) {
			if (fusion.Empty())
				return NkComponentPaint::NkPaintBlend::Alpha;
			const char *c = fusion.Data();
			if (StrEq(c, "multiply"))
				return NkComponentPaint::NkPaintBlend::Multiply;
			if (StrEq(c, "screen"))
				return NkComponentPaint::NkPaintBlend::Screen;
			if (StrEq(c, "darken"))
				return NkComponentPaint::NkPaintBlend::Darken;
			if (StrEq(c, "lighten"))
				return NkComponentPaint::NkPaintBlend::Lighten;
			if (StrEq(c, "plus-lighter"))
				return NkComponentPaint::NkPaintBlend::PlusLighter;
			return NkComponentPaint::NkPaintBlend::Alpha;
		}
		/// La garde : pousse le mode a la construction, le retire a la destruction --
		/// un `continue` dans la boucle des remplissages ne laisse jamais un mode derriere lui.
		struct NkGardeFusion {
				NkComponentPaint &p;
				bool actif;
				NkGardeFusion(NkComponentPaint &peintre, const NkString &fusion) : p(peintre), actif(false) {
					const NkComponentPaint::NkPaintBlend b = NkFusionExacte(fusion);
					if (b != NkComponentPaint::NkPaintBlend::Alpha) {
						p.PushBlend(b);
						actif = true;
					}
				}
				~NkGardeFusion() {
					if (actif)
						p.PopBlend();
				}
		};
		/// Le genre de degrade, tranche UNE fois : 0 lineaire (et tout type inconnu, dit),
		/// 1 radial, 2 angulaire, 3 losange.
		inline nkentseu::int32 NkGenreDegrade(const NkDegrade &g) {
			if (g.type.Empty() || StrEq(g.type.Data(), "lineaire"))
				return 0;
			if (StrEq(g.type.Data(), "radial"))
				return 1;
			if (StrEq(g.type.Data(), "angulaire"))
				return 2;
			if (StrEq(g.type.Data(), "losange"))
				return 3;
			return 0; // un type nomme que ce peintre ne connait pas : peint en lineaire
		}
		inline nkentseu::uint32 NkGContourArrondi(const NkPaintRect &r, const nkentseu::float32 c[4],
												  nkentseu::float32 *out, nkentseu::uint32 seg) {
			const nkentseu::float32 x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h;
			const nkentseu::float32 cxs[4] = {x0 + c[0], x1 - c[1], x1 - c[2], x0 + c[3]};
			const nkentseu::float32 cys[4] = {y0 + c[0], y0 + c[1], y1 - c[2], y1 - c[3]};
			const nkentseu::float32 a0s[4] = {180.f, 270.f, 0.f, 90.f};
			nkentseu::uint32 n = 0;
			for (nkentseu::uint32 k = 0; k < 4u; ++k) {
				if (c[k] <= 0.f) {
					out[n * 2] = cxs[k];
					out[n * 2 + 1] = cys[k];
					++n;
					continue;
				}
				for (nkentseu::uint32 i = 0; i <= seg; ++i) {
					nkentseu::float32 s = 0.f, co = 1.f;
					NkSinCosDeg(a0s[k] + 90.f * (nkentseu::float32)i / (nkentseu::float32)seg, s, co);
					out[n * 2] = cxs[k] + c[k] * co;
					out[n * 2 + 1] = cys[k] + c[k] * s;
					++n;
				}
			}
			return n;
		}
		inline void NkGRectCoins(NkComponentPaint &p, const NkPaintRect &r,
								 nkentseu::uint32 rgba, const nkentseu::float32 R[4]) {
			if (r.w <= 0.f || r.h <= 0.f)
				return;
			if (R[0] == R[1] && R[1] == R[2] && R[2] == R[3]) {
				p.FillColor(r, rgba, R[0]);
				return;
			}
			nkentseu::float32 c[4];
			NkGBornerRayons(r.w, r.h, R, c);
			// ── LES QUATRE COINS, PUIS QUATRE BANDES ET LE CENTRE ────────────
			// 🔴 MA PREMIERE DECOMPOSITION LAISSAIT DES TROUS, et c'est le defaut
			//    que Rodolf a photographie : deux bandes prises sur le MAXIMUM
			//    des rayons adjacents. Avec un coin a 18 et ses voisins a 4, la
			//    bande s'arretait a 18 du bord alors que le carre d'angle voisin
			//    n'en couvrait que 8 : il restait une ENCOCHE de 10 x 4 en bas a
			//    droite -- exactement celle de sa capture.
			//    ⚠️ *Une decomposition qui marche quand les quatre valeurs sont
			//       egales n'est pas verifiee : c'est le cas ou elle ne peut pas
			//       echouer.* Il fallait le cas dissymetrique pour la voir, et
			//       c'est precisement celui que l'arrondi par coin vient d'ouvrir.
			//
			// Les six regions, sans recouvrement obligatoire ni trou possible :
			// quatre carres d'angle, une bande par bord (entre SES deux coins),
			// et le centre.
			const nkentseu::float32 x0 = r.x, y0 = r.y;
			const nkentseu::float32 x1 = r.x + r.w, y1 = r.y + r.h;
			const nkentseu::float32 hautMax = c[0] > c[1] ? c[0] : c[1];
			const nkentseu::float32 basMax = c[3] > c[2] ? c[3] : c[2];
			const nkentseu::float32 gauMax = c[0] > c[3] ? c[0] : c[3];
			const nkentseu::float32 droMax = c[1] > c[2] ? c[1] : c[2];
			if (c[0] > 0.f)
				p.FillColor({x0, y0, c[0] * 2.f, c[0] * 2.f}, rgba, c[0]);
			if (c[1] > 0.f)
				p.FillColor({x1 - c[1] * 2.f, y0, c[1] * 2.f, c[1] * 2.f}, rgba, c[1]);
			if (c[2] > 0.f)
				p.FillColor({x1 - c[2] * 2.f, y1 - c[2] * 2.f, c[2] * 2.f, c[2] * 2.f}, rgba,
							c[2]);
			if (c[3] > 0.f)
				p.FillColor({x0, y1 - c[3] * 2.f, c[3] * 2.f, c[3] * 2.f}, rgba, c[3]);
			// bande HAUTE : entre les deux coins du haut, sur la hauteur du plus
			// grand des deux -- le bord y est droit.
			if (x1 - c[1] - (x0 + c[0]) > 0.f && hautMax > 0.f)
				p.FillColor({x0 + c[0], y0, (x1 - c[1]) - (x0 + c[0]), hautMax}, rgba, 0.f);
			if (x1 - c[2] - (x0 + c[3]) > 0.f && basMax > 0.f)
				p.FillColor({x0 + c[3], y1 - basMax, (x1 - c[2]) - (x0 + c[3]), basMax}, rgba,
							0.f);
			if (y1 - c[3] - (y0 + c[0]) > 0.f && gauMax > 0.f)
				p.FillColor({x0, y0 + c[0], gauMax, (y1 - c[3]) - (y0 + c[0])}, rgba, 0.f);
			if (y1 - c[2] - (y0 + c[1]) > 0.f && droMax > 0.f)
				p.FillColor({x1 - droMax, y0 + c[1], droMax, (y1 - c[2]) - (y0 + c[1])}, rgba,
							0.f);
			// le CENTRE : ce que les quatre bandes n'ont pas couvert.
			const nkentseu::float32 cx0 = x0 + gauMax, cx1 = x1 - droMax;
			const nkentseu::float32 cy0 = y0 + hautMax, cy1 = y1 - basMax;
			if (cx1 - cx0 > 0.f && cy1 - cy0 > 0.f)
				p.FillColor({cx0, cy0, cx1 - cx0, cy1 - cy0}, rgba, 0.f);
		}

		/// LES QUATRE BANDES D'UNE BORDURE, POSÉES SELON SA POSITION.
		/// ⚠️ C'EST ICI QUE « intérieur / centre / extérieur » VEUT DIRE QUELQUE
		///    CHOSE. Le trait a une épaisseur `e` ; le rectangle peint est
		///    décalé de 0 (intérieur), e/2 (centré, à cheval) ou e (extérieur).
		///    Sans ce décalage, les trois valeurs auraient rendu le même dessin :
		///    un menu à trois entrées dont deux mentent.
		/// 🔴 ELLE PEIGNAIT QUATRE RECTANGLES DROITS, RAYON CODÉ À `0.f` — et
		///    surtout, **elle ne recevait pas le rayon** : un contour carré par
		///    construction, quel que soit l'arrondi de la forme (Rodolf, 02/09 :
		///    *« pourquoi quand je mets les bordures, les bordures ne suivent pas
		///    les arrondis ? »*).
		///
		/// ⚠️ ET LE REMÈDE N'ÉTAIT PAS D'ARRONDIR LES QUATRE BANDES : quatre
		///    rectangles aux bouts ronds ne font pas un contour, ils font quatre
		///    rectangles aux bouts ronds. On peint un ANNEAU — la forme extérieure
		///    pleine, puis `interieur` par-dessus en retrait de l'épaisseur.
		///
		/// ⚠️ `interieur` EST CE QU'IL FAUT REMETTRE AU MILIEU, et c'est pour ça
		///    que l'appelant le passe : sans lui, l'anneau serait un disque plein
		///    qui masquerait le fond du nœud. Un alpha nul veut dire « ne rien
		///    remettre » — le cas du nœud sans fond.
		///
		/// LE RAYON EFFECTIF SUIT LE DÉCALAGE : un contour extérieur est plus
		/// grand, donc plus arrondi ; un contour intérieur, l'inverse. C'est ce
		/// que « position » veut dire quand la forme est arrondie, et ça ne se
		/// voyait pas tant que tout était carré.
		/// LE CORPS : un ANNEAU de `e` px, en couleur DEJA RESOLUE.
		/// 🔑 Scinde du cadre nomme pour qu'un appelant qui n'a PAS de
		///    `NkBordure` -- le repli du theme, qui n'a qu'un role -- puisse
		///    dessiner le MEME anneau au lieu d'un rectangle a angles droits.
		/// LE CADRE PAR COTE : quatre epaisseurs (haut, droite, bas, gauche) qui se
		/// raccordent dans l'arc -- rayon exterieur d'un coin = R + debord du coin,
		/// rayon interieur = exterieur - epaisseur du coin. Jointure aux coins DROITS :
		/// « rond » = coin exterieur arrondi de l'epaisseur ; « biseau » = coin coupe
		/// (polygone ; sans polygone, repli sur l'onglet) ; sinon l'onglet.
		/// LE DAMIER d'une image absente : deux gris, carreaux de 8 px, dans la
		/// boite (le fond arrondi d'abord, puis les carreaux clairs qui restent dans
		/// le rectangle -- un carreau ne suit pas l'arc : c'est un apercu, dit tel).
		// ── LE CONTOUR D'UN TRACE EDITE, ET SES MORCEAUX CONVEXES (Q94, 05/09) ─────────
		/// Rodolf : « lorsqu'on edite un graphique qui etait en degrade, son degrade
		/// disparait et refuse de s'appliquer par la suite ». LA CAUSE : un noeud a
		/// sommets (la premiere modification en mode edition les materialise) prenait le
		/// chemin `NkGTraceEdite` -- UNE couleur, puis retour -- et la boucle des
		/// remplissages (degrade, image) n'etait jamais atteinte. La donnee etait intacte
		/// (le fichier gardait ses arrets) : c'est le peintre qui la taisait, et
		/// re-choisir « Lineaire » ecrivait dans une liste que ce chemin ne lisait pas.
		/// DESORMAIS un trace est une forme comme une autre pour le remplissage : il donne
		/// un CONTOUR, et tout ce qui se peint par bandes ou par cellules se rogne par ses
		/// MORCEAUX CONVEXES -- le contour lui-meme s'il est convexe, ses triangles sinon
		/// (`NkEarcutVers`, la porte sans allocation, la meme que le peintre du kit).
		inline nkentseu::uint32 NkGContourTrace(const NkPaintRect &r, const NkUINode &n, const NkMat2D &mEff,
												nkentseu::float32 *xy, nkentseu::uint32 cap) {
			if (n.sommets.Empty() && mEff.Identite())
				return 0u;
			const nkentseu::uint32 nb = NkContourDe(n, r, xy, cap);
			if (nb < 3u)
				return 0u;
			NkMatContour(mEff, xy, nb);
			return nb;
		}
		struct NkGMorceaux {
				nkentseu::float32 pts[128u * 6u]; ///< au plus 126 triangles x 3 points x (x, y)
				nkentseu::uint32 debut[128u], nb[128u];
				nkentseu::uint32 n = 0u;
		};
		inline bool NkGEstConvexe(const nkentseu::float32 *xy, nkentseu::uint32 nb) {
			nkentseu::int32 signe = 0;
			for (nkentseu::uint32 k = 0; k < nb; ++k) {
				const nkentseu::uint32 j = (k + 1u) % nb, l = (k + 2u) % nb;
				const nkentseu::float32 ax = xy[j * 2] - xy[k * 2], ay = xy[j * 2 + 1] - xy[k * 2 + 1];
				const nkentseu::float32 bx = xy[l * 2] - xy[j * 2], by = xy[l * 2 + 1] - xy[j * 2 + 1];
				const nkentseu::float32 c = ax * by - ay * bx;
				if (c > -1e-4f && c < 1e-4f)
					continue;
				const nkentseu::int32 sg = c > 0.f ? 1 : -1;
				if (signe == 0)
					signe = sg;
				else if (sg != signe)
					return false;
			}
			return true;
		}
		inline void NkGMorceauxDe(const nkentseu::float32 *xy, nkentseu::uint32 nb, NkGMorceaux &m) {
			using namespace nkentseu;
			m.n = 0u;
			if (!xy || nb < 3u || nb > 128u)
				return;
			auto entier = [&]() {
				for (uint32 k = 0; k < nb * 2u; ++k)
					m.pts[k] = xy[k];
				m.debut[0] = 0u;
				m.nb[0] = nb;
				m.n = 1u;
			};
			if (NkGEstConvexe(xy, nb)) {
				entier();
				return;
			}
			math::NkVec2f pts[128];
			::nkentseu::detail::NkEarcutNode<float32> noeuds[128];
			uint32 idx[126u * 3u];
			for (uint32 i = 0; i < nb; ++i)
				pts[i] = math::NkVec2f(xy[i * 2], xy[i * 2 + 1]);
			const uint32 nbTri = ::nkentseu::NkEarcutVers<float32>(pts, nb, noeuds, 128u, idx, 126u * 3u);
			if (nbTri == 0u) { // contour degenere ou qui se croise : le contour entier, dit -- une forme qui montre quelque chose
				entier();
				return;
			}
			for (uint32 t = 0; t < nbTri; ++t) {
				m.debut[t] = t * 6u;
				m.nb[t] = 3u;
				for (uint32 k = 0; k < 3u; ++k) {
					m.pts[t * 6u + k * 2u] = pts[idx[t * 3u + k]].x;
					m.pts[t * 6u + k * 2u + 1u] = pts[idx[t * 3u + k]].y;
				}
			}
			m.n = nbTri;
		}
		/// Un polygone CONVEXE rogne par chaque morceau ; chaque coupe non vide est remise a `emettre`.
		template <class F>
		inline void NkGRognerParMorceaux(const nkentseu::float32 *forme, nkentseu::uint32 nf, const NkGMorceaux &m, F &&emettre) {
			for (nkentseu::uint32 k = 0; k < m.n; ++k) {
				nkentseu::float32 coupe[128];
				const nkentseu::uint32 ncp = NkGDecouperParConvexe(forme, nf, m.pts + m.debut[k], m.nb[k], coupe, 64u);
				if (ncp >= 3u)
					emettre(coupe, ncp);
			}
		}

		// ── L'ANNEAU D'UN TRACE EDITE (05/09) ────────────────────────────────────
		/// La bordure d'un polygone quelconque (convexe ou non) : le contour DECALE le
		/// long des normales de ses aretes, en position interieure / centree /
		/// exterieure, avec les JOINTURES que le modele porte (onglet / rond / biseau)
		/// -- la famille des bordures du rect (`NkGCadreCotes`), generalisee.
		/// Par arete : un quadrilatere entre les points de decalage ; au sommet, le cote
		/// qui se RECOUVRE est ramene a l'intersection des deux droites decalees et le
		/// cote qui S'OUVRE recoit un COIN (le point d'onglet, l'arc, ou la coupe du
		/// biseau) en eventail depuis le point de l'autre cote. Les pieces se partagent
		/// leurs aretes -- ni recouvrement ni trou -- et chacune est CONVEXE ; un
		/// contour concave a ses coins qui s'ouvrent vers l'interieur : la meme regle,
		/// les cotes echanges. L'onglet trop aigu devient un biseau (limite ~4
		/// epaisseurs, comme partout). Un cote a decalage nul (position interieure ou
		/// exterieure) ne fait ni coin ni recouvrement. Rend faux si le peintre ne
		/// sait pas le polygone -- l'appelant montre alors le contour de la boite.
		inline bool NkGAnneauTrace(NkComponentPaint &p, const nkentseu::float32 *xy, nkentseu::uint32 nb, nkentseu::uint32 rgba,
								   nkentseu::float32 epaisseur, NkBordurePos position, const char *jointure) {
			using namespace nkentseu;
			if (!xy || nb < 3u || nb > 128u)
				return false;
			const float32 e = epaisseur > 0.f ? epaisseur : 1.f;
			const float32 dOut = position == NkBordurePos::Centre ? e * 0.5f : (position == NkBordurePos::Exterieur ? e : 0.f);
			const float32 dIn = position == NkBordurePos::Centre ? e * 0.5f : (position == NkBordurePos::Interieur ? e : 0.f);
			const bool rond = jointure && StrEq(jointure, "rond");
			const bool biseau = jointure && StrEq(jointure, "biseau");
			// l'orientation : aire signee > 0 -> l'interieur est a gauche des aretes
			float64 aire = 0.0;
			for (uint32 i = 0; i < nb; ++i) {
				const uint32 j = (i + 1u) % nb;
				aire += (float64)xy[i * 2] * xy[j * 2 + 1] - (float64)xy[j * 2] * xy[i * 2 + 1];
			}
			const float32 sgn = aire >= 0.0 ? 1.f : -1.f;
			// les aretes : direction unitaire et normale SORTANTE
			float32 ex[128], ey[128], nx[128], ny[128];
			for (uint32 i = 0; i < nb; ++i) {
				const uint32 j = (i + 1u) % nb;
				float32 dx = xy[j * 2] - xy[i * 2], dy = xy[j * 2 + 1] - xy[i * 2 + 1];
				const float32 L = NkLongueur2D(dx, dy);
				if (L < 1e-5f) { // arete degeneree : la normale de la precedente
					const uint32 ip = (i + nb - 1u) % nb;
					ex[i] = i ? ex[ip] : 1.f;
					ey[i] = i ? ey[ip] : 0.f;
				} else {
					ex[i] = dx / L;
					ey[i] = dy / L;
				}
				nx[i] = sgn * ey[i];
				ny[i] = -sgn * ex[i];
			}
			// par sommet et par cote : le premier et le dernier point du cote, et si le cote s'ouvre
			float32 fPx[128], fPy[128], lPx[128], lPy[128], fMx[128], fMy[128], lMx[128], lMy[128];
			bool ouvertP[128], ouvertM[128];
			for (uint32 j = 0; j < nb; ++j) {
				const uint32 jp = (j + nb - 1u) % nb;
				const float32 vx = xy[j * 2], vy = xy[j * 2 + 1];
				const float32 cross = ex[jp] * ey[j] - ey[jp] * ex[j];
				const bool convexe = cross * sgn > 1e-6f; // le contour tourne vers l'interieur : le cote exterieur s'ouvre
				const bool droit = cross * sgn > -1e-6f && cross * sgn < 1e-6f;
				const float32 dot = nx[jp] * nx[j] + ny[jp] * ny[j];
				const float32 un = 1.f + dot;
				for (int32 cote = 0; cote < 2; ++cote) {
					const float32 sens = cote == 0 ? 1.f : -1.f;
					const float32 d = cote == 0 ? dOut : dIn;
					float32 *fx = cote == 0 ? fPx : fMx, *fy = cote == 0 ? fPy : fMy, *lx = cote == 0 ? lPx : lMx, *ly = cote == 0 ? lPy : lMy;
					bool *ouv = cote == 0 ? ouvertP : ouvertM;
					const bool sOuvre = !droit && (cote == 0 ? convexe : !convexe);
					if (d <= 0.f) { // pas de decalage de ce cote : le sommet lui-meme
						fx[j] = lx[j] = vx;
						fy[j] = ly[j] = vy;
						ouv[j] = false;
					} else if (sOuvre) {
						fx[j] = vx + sens * nx[jp] * d;
						fy[j] = vy + sens * ny[jp] * d;
						lx[j] = vx + sens * nx[j] * d;
						ly[j] = vy + sens * ny[j] * d;
						ouv[j] = true;
					} else if (un > 0.05f) { // le cote qui se recouvre : l'intersection des deux droites decalees
						fx[j] = lx[j] = vx + sens * (nx[jp] + nx[j]) * d / un;
						fy[j] = ly[j] = vy + sens * (ny[jp] + ny[j]) * d / un;
						ouv[j] = false;
					} else { // rebroussement : le point brut, le recouvrement est accepte et dit
						fx[j] = vx + sens * nx[jp] * d;
						fy[j] = vy + sens * ny[jp] * d;
						lx[j] = vx + sens * nx[j] * d;
						ly[j] = vy + sens * ny[j] * d;
						ouv[j] = false;
					}
				}
			}
			bool su = true;
			// les aretes : [dernier-(i), premier-(i+1), premier+(i+1), dernier+(i)]
			for (uint32 i = 0; i < nb && su; ++i) {
				const uint32 j = (i + 1u) % nb;
				float32 q[8] = {lMx[i], lMy[i], fMx[j], fMy[j], fPx[j], fPy[j], lPx[i], lPy[i]};
				// les points confondus (decalage nul) ne font pas un polygone degenere
				float32 u[8];
				uint32 nu = 0u;
				for (uint32 k = 0; k < 4u; ++k) {
					const uint32 kp = (k + 3u) % 4u;
					if (nu && q[k * 2] == q[kp * 2] && q[k * 2 + 1] == q[kp * 2 + 1])
						continue;
					u[nu * 2] = q[k * 2];
					u[nu * 2 + 1] = q[k * 2 + 1];
					++nu;
				}
				if (nu >= 3u && !p.PolygonHex(u, (int32)nu, rgba))
					su = false;
			}
			// les coins : sur le cote qui s'ouvre, en eventail depuis le point de l'autre cote
			for (uint32 j = 0; j < nb && su; ++j) {
				const uint32 jp = (j + nb - 1u) % nb;
				for (int32 cote = 0; cote < 2 && su; ++cote) {
					const bool *ouv = cote == 0 ? ouvertP : ouvertM;
					if (!ouv[j])
						continue;
					const float32 sens = cote == 0 ? 1.f : -1.f;
					const float32 d = cote == 0 ? dOut : dIn;
					const float32 vx = xy[j * 2], vy = xy[j * 2 + 1];
					const float32 ax = cote == 0 ? lMx[j] : lPx[j], ay = cote == 0 ? lMy[j] : lPy[j]; // l'autre cote (un seul point)
					const float32 f0x = cote == 0 ? fPx[j] : fMx[j], f0y = cote == 0 ? fPy[j] : fMy[j];
					const float32 l0x = cote == 0 ? lPx[j] : lMx[j], l0y = cote == 0 ? lPy[j] : lMy[j];
					float32 poly[2 * 40];
					uint32 np = 0u;
					auto pousser = [&](float32 px, float32 py) {
						if (np < 40u) {
							poly[np * 2] = px;
							poly[np * 2 + 1] = py;
							++np;
						}
					};
					pousser(ax, ay);
					pousser(f0x, f0y);
					const float32 dot = nx[jp] * nx[j] + ny[jp] * ny[j];
					const float32 un = 1.f + dot;
					if (rond) {
						// l'arc de n1 a n2 autour du sommet, un point tous les ~15 degres
						const float32 cross = nx[jp] * ny[j] - ny[jp] * nx[j];
						float32 ang = 0.f;
						{ // l'angle entre les deux normales, par atan2 des composantes
							const float32 c = dot < -1.f ? -1.f : (dot > 1.f ? 1.f : dot);
							const float32 sn = cross < 0.f ? -cross : cross;
							ang = (float32)nkentseu::math::NkAtan2((float64)sn, (float64)c);
						}
						const uint32 seg = ang > 0.f ? (uint32)(ang / 0.2618f) + 1u : 1u;
						const float32 pas = ang / (float32)seg;
						for (uint32 k = 1; k < seg; ++k) {
							const float32 a = (cross >= 0.f ? 1.f : -1.f) * pas * (float32)k;
							float32 sa = 0.f, ca = 1.f;
							{ // rotation de n1 de l'angle a
								sa = (float32)nkentseu::math::NkSin((float64)a);
								ca = (float32)nkentseu::math::NkCos((float64)a);
							}
							const float32 rx = nx[jp] * ca - ny[jp] * sa, ry = nx[jp] * sa + ny[jp] * ca;
							pousser(vx + sens * rx * d, vy + sens * ry * d);
						}
					} else if (!biseau && un > 0.125f) { // l'onglet, sauf trop aigu (limite ~4 epaisseurs)
						pousser(vx + sens * (nx[jp] + nx[j]) * d / un, vy + sens * (ny[jp] + ny[j]) * d / un);
					}
					pousser(l0x, l0y);
					if (np >= 3u && !p.PolygonHex(poly, (int32)np, rgba))
						su = false;
				}
			}
			return su;
		}

		/// LES CINQ CADRAGES PEINTS, LA ROTATION, LE CONTOUR QUI ROGNE (05/09).
		/// La source vient du fournisseur ; l'image est POSEE dans un repere tourne
		/// autour du centre de la boite (la rotation de l'image, meme convention que
		/// celle du noeud : horaire, y vers le bas) sous la forme d'une CELLULE
		/// (rectangle de destination + fenetre uv) ; la cellule, ramenee au document,
		/// est ROGNEE par le contour arrondi (`NkGDecouperParConvexe`, comme les
		/// bandes du degrade) et emise en UN polygone texture, l'uv de chaque sommet
		/// obtenu par l'inverse de la pose -- exact, pas echantillonne.
		///   Fill    : couvre la boite (le plus grand des deux rapports), rogne ;
		///   Fit     : contient (le plus petit), des bandes restent nues ;
		///   Stretch : la boite ;
		///   Tile    : des cellules a la taille NATURELLE de l'image, depuis le coin
		///             haut-gauche, assez pour couvrir le disque englobant (la boite
		///             peut etre tournee) ; plafond de 4096 cellules -- au-dela, la
		///             tuile est agrandie d'autant, et c'est dit ici ;
		///   Crop    : la boite, fenetre `crop` de l'image.
		/// Rend faux si la source manque (le chemin est note) ou si le peintre ne sait
		/// pas peindre une image : l'appelant peint le damier.
		inline bool NkGPeindreImage(NkComponentPaint &p, const NkPaintRect &r, const nkentseu::float32 R[4],
									const NkRemplissage &f, const nkentseu::float32 *trace = nullptr,
									nkentseu::uint32 traceNb = 0u,
									nkentseu::float32 heritee = 1.f) {
			using namespace nkentseu;
			NkFournisseurImages &fi = NkFournisseurCourant();
			NkImageSource src;
			if (f.image.Empty() || !fi.obtenir || !fi.obtenir(fi.user, f.image.Data(), src) || src.w <= 0 || src.h <= 0) {
				fi.derniereAbsente = f.image.Empty() ? nullptr : f.image.Data();
				return false;
			}
			if (r.w <= 0.f || r.h <= 0.f)
				return true;
			float32 cb[4];
			NkGBornerRayons(r.w, r.h, R, cb);
			float32 contour[80];
			const uint32 nc = NkGContourArrondi(r, cb, contour, 8u);
			// Q94 : un trace edite se rogne par ses morceaux convexes, pas par l'arrondi de la boite
			NkGMorceaux morceaux;
			if (trace && traceNb >= 3u)
				NkGMorceauxDe(trace, traceNb, morceaux);
			else
				NkGMorceauxDe(contour, nc, morceaux);
			const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
			float32 s = 0.f, c = 1.f;
			NkSinCosDeg(f.rotationImage, s, c);
			const float32 iw = (float32)src.w, ih = (float32)src.h;
			const char *cad = f.cadrage.Data() ? f.cadrage.Data() : "";
			const bool fit = NkComponentDecl::StrEq(cad, "fit"), stretch = NkComponentDecl::StrEq(cad, "stretch"),
					   tile = NkComponentDecl::StrEq(cad, "tile"), crop = NkComponentDecl::StrEq(cad, "crop");
			bool su = true;
			auto emettre = [&](float32 x0, float32 y0, float32 w, float32 h, float32 u0, float32 v0, float32 u1, float32 v1) {
				if (w <= 0.f || h <= 0.f)
					return;
				// la cellule (repere tourne, centre) -> le document
				const float32 lx[4] = {x0, x0 + w, x0 + w, x0}, ly[4] = {y0, y0, y0 + h, y0 + h};
				float32 forme[8];
				for (uint32 i = 0; i < 4u; ++i) {
					forme[i * 2] = cx + lx[i] * c - ly[i] * s;
					forme[i * 2 + 1] = cy + lx[i] * s + ly[i] * c;
				}
				// rognee par chaque morceau du contour (une cellule hors du contour ne donne rien : pas un echec)
				NkGRognerParMorceaux(forme, 4u, morceaux, [&](const float32 *coupe, uint32 ncp) {
					float32 uv[128];
					for (uint32 i = 0; i < ncp; ++i) {
						const float32 px = coupe[i * 2] - cx, py = coupe[i * 2 + 1] - cy;
						const float32 x = px * c + py * s, y = -px * s + py * c; // l'inverse de la pose
						uv[i * 2] = u0 + (x - x0) / w * (u1 - u0);
						uv[i * 2 + 1] = v0 + (y - y0) / h * (v1 - v0);
					}
					if (!p.ImagePolygone(coupe, uv, (int32)ncp, src.handle,
										 f.opacite * (heritee < 0.f ? 0.f : (heritee > 1.f ? 1.f : heritee))))
						su = false;
				});
			};
			if (tile) {
				// le disque englobant, majore par (w + h) / 2 >= demi-diagonale : pas de racine ici
				const float32 Rd = (r.w + r.h) * 0.5f;
				float32 tw = iw, th = ih;
				auto compte = [&](float32 t, float32 &n) { n = (float32)(int32)(2.f * Rd / t) + 3.f; };
				float32 nx = 0.f, ny = 0.f;
				compte(tw, nx);
				compte(th, ny);
				while (nx * ny > 4096.f) { // le plafond : la tuile double jusqu'a tenir
					tw *= 2.f;
					th *= 2.f;
					compte(tw, nx);
					compte(th, ny);
				}
				const float32 ox = -r.w * 0.5f, oy = -r.h * 0.5f; // la premiere tuile au coin haut-gauche
				auto plancher = [](float32 v) { const int32 i = (int32)v; return (float32)i > v ? i - 1 : i; };
				const int32 i0 = plancher((-Rd - ox) / tw) - 1, j0 = plancher((-Rd - oy) / th) - 1;
				for (int32 j = j0; (float32)(j - j0) < ny + 2.f; ++j)
					for (int32 i = i0; (float32)(i - i0) < nx + 2.f; ++i)
						emettre(ox + (float32)i * tw, oy + (float32)j * th, tw, th, 0.f, 0.f, 1.f, 1.f);
				return su;
			}
			float32 dw = r.w, dh = r.h, u0 = 0.f, v0 = 0.f, u1 = 1.f, v1 = 1.f;
			if (stretch) {
			} else if (fit) {
				const float32 k = (r.w / iw < r.h / ih) ? r.w / iw : r.h / ih;
				dw = iw * k;
				dh = ih * k;
			} else if (crop) {
				u0 = f.cropX;
				v0 = f.cropY;
				u1 = f.cropX + f.cropW;
				v1 = f.cropY + f.cropH;
			} else { // fill, le defaut
				const float32 k = (r.w / iw > r.h / ih) ? r.w / iw : r.h / ih;
				dw = iw * k;
				dh = ih * k;
			}
			emettre(-dw * 0.5f, -dh * 0.5f, dw, dh, u0, v0, u1, v1);
			return su;
		}

		/// ⑤ (07/09) LE FACTEUR D'ALPHA, EN UN SEUL SITE. La même expression de
		/// bornage était écrite CINQ fois (damier, deux fois pour les bordures, les
		/// ombres, les remplissages) : y ajouter l'opacité du nœud aurait été cinq
		/// occasions d'en oublier une. `heritee` est déjà un facteur 0..1 -- le
		/// produit des opacités du nœud et de tous ses ancêtres.
		inline nkentseu::float32 NkKOpacite(nkentseu::float32 pourcent,
											nkentseu::float32 heritee) {
			const nkentseu::float32 c =
				(pourcent < 0.f ? 0.f : (pourcent > 100.f ? 100.f : pourcent)) * 0.01f;
			const nkentseu::float32 h = heritee < 0.f ? 0.f : (heritee > 1.f ? 1.f : heritee);
			return c * h;
		}

		inline void NkGDamier(NkComponentPaint &p, const NkPaintRect &r, const nkentseu::float32 R[4],
							  nkentseu::float32 opacite, nkentseu::float32 heritee = 1.f) {
			const nkentseu::float32 k = NkKOpacite(opacite, heritee);
			const nkentseu::uint32 a = (nkentseu::uint32)(255.f * k + 0.5f) & 0xFFu;
			NkGRectCoins(p, r, 0x9A9A9A00u | a, R);
			const nkentseu::float32 c = 8.f;
			nkentseu::int32 ny = 0;
			for (nkentseu::float32 y = r.y; y < r.y + r.h; y += c, ++ny) {
				nkentseu::int32 nx = 0;
				for (nkentseu::float32 x = r.x; x < r.x + r.w; x += c, ++nx) {
					if (((nx + ny) & 1) == 0)
						continue;
					const nkentseu::float32 w = (x + c > r.x + r.w) ? r.x + r.w - x : c;
					const nkentseu::float32 h = (y + c > r.y + r.h) ? r.y + r.h - y : c;
					p.FillColor({x, y, w, h}, 0xD4D4D400u | a, 0.f);
				}
			}
		}
		inline void NkGCadreCotes(NkComponentPaint &p, const NkPaintRect &r, nkentseu::uint32 rgba,
								  const nkentseu::float32 eC[4], NkBordurePos position,
								  const nkentseu::float32 R[4], nkentseu::uint32 interieur,
								  const char *jointure) {
			nkentseu::float32 e[4], d[4];
			for (nkentseu::uint32 i = 0; i < 4u; ++i) {
				e[i] = eC[i] > 0.f ? eC[i] : 1.f;
				d[i] = position == NkBordurePos::Centre ? e[i] * 0.5f
					   : position == NkBordurePos::Exterieur ? e[i] : 0.f;
			}
			// coin k : ses deux cotes (haut-gauche, haut-droite, bas-droite, bas-gauche)
			const nkentseu::uint32 cA[4] = {0u, 0u, 2u, 2u}, cB[4] = {3u, 1u, 1u, 3u};
			const bool rond = jointure && StrEq(jointure, "rond");
			const bool biseau = jointure && StrEq(jointure, "biseau");
			const NkPaintRect q = {r.x - d[3], r.y - d[0], r.w + d[3] + d[1], r.h + d[0] + d[2]};
			nkentseu::float32 Rext[4], Rint[4], ek[4];
			bool unCoinDroit = false;
			for (nkentseu::uint32 k = 0; k < 4u; ++k) {
				const nkentseu::float32 dk = d[cA[k]] > d[cB[k]] ? d[cA[k]] : d[cB[k]];
				ek[k] = e[cA[k]] > e[cB[k]] ? e[cA[k]] : e[cB[k]];
				if (R[k] > 0.f)
					Rext[k] = R[k] + dk;
				else {
					Rext[k] = rond ? ek[k] : 0.f;
					unCoinDroit = true;
				}
				const nkentseu::float32 ri = Rext[k] - ek[k];
				Rint[k] = ri > 0.f ? ri : 0.f;
			}
			bool peint = false;
			if (biseau && unCoinDroit && q.w > 0.f && q.h > 0.f) {
				// l'exterieur en polygone : chaque coin droit est coupe de son epaisseur
				nkentseu::float32 xy[16];
				nkentseu::uint32 n = 0;
				const nkentseu::float32 x0 = q.x, y0 = q.y, x1 = q.x + q.w, y1 = q.y + q.h;
				auto coin = [&](nkentseu::uint32 k, nkentseu::float32 cx, nkentseu::float32 cy,
								nkentseu::float32 sx, nkentseu::float32 sy) {
					if (R[k] > 0.f || ek[k] <= 0.f) {
						xy[n * 2] = cx;
						xy[n * 2 + 1] = cy;
						++n;
						return;
					}
					// deux points : le long de l'axe x puis de l'axe y (sens horaire)
					if ((sx > 0.f) == (sy > 0.f)) {
						xy[n * 2] = cx; xy[n * 2 + 1] = cy + sy * ek[k]; ++n;
						xy[n * 2] = cx + sx * ek[k]; xy[n * 2 + 1] = cy; ++n;
					} else {
						xy[n * 2] = cx + sx * ek[k]; xy[n * 2 + 1] = cy; ++n;
						xy[n * 2] = cx; xy[n * 2 + 1] = cy + sy * ek[k]; ++n;
					}
				};
				coin(0u, x0, y0, 1.f, 1.f);
				coin(1u, x1, y0, -1.f, 1.f);
				coin(2u, x1, y1, -1.f, -1.f);
				coin(3u, x0, y1, 1.f, -1.f);
				peint = p.PolygonHex(xy, (int32)n, rgba);
			}
			if (!peint)
				NkGRectCoins(p, q, rgba, Rext);
			if ((interieur & 0xFFu) != 0u) {
				const NkPaintRect qi = {q.x + e[3], q.y + e[0], q.w - e[3] - e[1], q.h - e[0] - e[2]};
				if (qi.w > 0.f && qi.h > 0.f)
					NkGRectCoins(p, qi, interieur, Rint);
			}
		}
		inline void NkGCadreRGBA(NkComponentPaint &p, const NkPaintRect &r,
								 nkentseu::uint32 rgba, nkentseu::float32 epaisseur,
								 NkBordurePos position, const nkentseu::float32 R[4],
								 nkentseu::uint32 interieur) {
			const nkentseu::float32 e = epaisseur > 0.f ? epaisseur : 1.f;
			const nkentseu::float32 eC[4] = {e, e, e, e};
			NkGCadreCotes(p, r, rgba, eC, position, R, interieur, "");
		}

		/// LA PORTE NOMMEE : une `NkBordure` (couleur en texte, opacite) devient
		/// une couleur resolue, puis c'est le meme anneau. Comportement inchange.
		inline nkentseu::uint32 NkGBordureRGBA(const NkBordure &b,
											   nkentseu::float32 heritee = 1.f) {
			const nkentseu::uint32 base = NkGCouleur(b.couleur.Data());
			const nkentseu::float32 k = NkKOpacite(b.opacite, heritee);
			const nkentseu::uint32 a = (nkentseu::uint32)((base & 0xFFu) * k + 0.5f);
			return (base & 0xFFFFFF00u) | (a & 0xFFu);
		}
		inline void NkGCadre(NkComponentPaint &p, const NkPaintRect &r, const NkBordure &b,
							 const nkentseu::float32 R[4], nkentseu::uint32 interieur,
							 nkentseu::float32 heritee = 1.f) {
			const nkentseu::uint32 base = NkGCouleur(b.couleur.Data());
			const nkentseu::float32 k = NkKOpacite(b.opacite, heritee);
			const nkentseu::uint32 a = (nkentseu::uint32)((base & 0xFFu) * k + 0.5f);
			const nkentseu::float32 eC[4] = {b.Cote(0), b.Cote(1), b.Cote(2), b.Cote(3)};
			NkGCadreCotes(p, r, (base & 0xFFFFFF00u) | (a & 0xFFu), eC, b.position, R, interieur,
						  b.jointure.Data());
		}

		/// LE CADRE DU THEME (1 px, un role), QUI SUIT LES ARRONDIS.
		///
		/// 🔴 IL REMPLACE `OutlineSharp` SUR LES CHEMINS DE REPLI, ET C'EST LE
		///    MEME DEFAUT QUE RODOLF A PHOTOGRAPHIE. `OutlineSharp` n'a AUCUN
		///    parametre de rayon -- pas « un rayon a zero » : aucun. Sur un noeud
		///    arrondi, il tracait donc un rectangle a angles droits par-dessus un
		///    fond arrondi, et le trait depassait aux quatre coins.
		///
		/// ⚠️ CE N'ETAIT PAS UN OUBLI ISOLE : l'inventaire du 03/09 a trouve SIX
		///    chemins qui dessinaient la forme d'un noeud sans son rayon. La cause
		///    est structurelle -- `rounding` a une VALEUR PAR DEFAUT de `0.f` dans
		///    l'interface du peintre, donc l'oublier compile en silence, et
		///    `OutlineSharp` n'offre meme pas le parametre. *Un defaut qui revient
		///    trois fois n'est pas trois etourderies : c'est une signature trop
		///    facile a mal appeler.*
		/// ⚠️ `interieur` N'A PAS DE VALEUR PAR DEFAUT, ET C'EST VOULU.
		///    Ce peintre n'a NI primitive d'anneau NI masque : `NkGCadreRGBA`
		///    peint la forme en couleur de bord, puis REPEINT l'interieur avec
		///    la couleur qu'on lui donne. Passer 0 ne laisse donc pas un trou :
		///    ca laisse un BLOC PLEIN. Un defaut a 0 aurait rendu l'erreur
		///    silencieuse -- exactement le piege de `rounding = 0.f` qu'on
		///    vient de payer trois fois. *Le parametre qu'on peut oublier est
		///    celui qu'on oubliera.*
		inline void NkGCadreTheme(NkComponentPaint &p, const NkPaintRect &r,
								  const nkentseu::float32 R[4], nkentseu::uint16 role,
								  nkentseu::uint32 interieur) {
			NkGCadreRGBA(p, r, p.ColorOf(role), 1.f, NkBordurePos::Interieur, R,
						 interieur);
		}

		/// LES OMBRES PORTÉES D'UN NŒUD, peintes AVANT sa forme.
		/// ⚠️ L'OMBRE INTERNE N'EST PAS PEINTE, ET LE CODE LE DIT PLUTÔT QUE DE
		///    FAIRE SEMBLANT. Elle demande de découper l'intérieur de la forme
		///    (un masque), ce que ce peintre ne sait pas faire ; la rendre comme
		///    une ombre portée donnerait un dessin FAUX qui a l'air juste — la
		///    pire des sorties. Elle se règle dans l'Inspecteur, se sauve, se
		///    relit, et attend son peintre. C'est écrit ici et au rapport.
		/// LES OMBRES PORTEES, PAR COIN : chaque anneau suit les quatre rayons du
		/// noeud (un coin droit reste droit, un coin arrondi s'arrondit de R +
		/// grossi). Le flou est approche par des anneaux de plus en plus
		/// transparents -- le peintre n'a pas de primitive floue, et ca se dit.
		inline void NkGOmbres(NkComponentPaint &p, const NkPaintRect &r, const NkUINode &n,
							  nkentseu::float32 heritee = 1.f) {
			nkentseu::float32 R[4], c[4];
			NkGRayons(n, R);
			NkGBornerRayons(r.w, r.h, R, c);
			for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)n.effets.Size(); ++i) {
				const NkEffet &e = n.effets[i];
				if (!e.visible || e.couleur.Empty() || e.type != NkEffetType::OmbrePortee)
					continue;
				const nkentseu::uint32 base = NkGCouleur(e.couleur.Data());
				const nkentseu::float32 op = NkKOpacite(e.opacite, heritee);
				if (op <= 0.f)
					continue;
				const int32 kAnneaux = e.flou > 0.5f ? 4 : 1;
				for (int32 k = kAnneaux; k >= 1; --k) {
					const nkentseu::float32 t = (nkentseu::float32)k / (nkentseu::float32)kAnneaux;
					const nkentseu::float32 grossi = e.etendue + e.flou * t;
					const nkentseu::float32 a01 = op / (nkentseu::float32)kAnneaux;
					const nkentseu::uint32 a =
						(nkentseu::uint32)((base & 0xFFu) * a01 + 0.5f);
					if (a == 0u)
						continue;
					const NkPaintRect q = {r.x + e.x - grossi, r.y + e.y - grossi,
										   r.w + grossi * 2.f, r.h + grossi * 2.f};
					// par coin : un coin arrondi grossit de `grossi` ; un coin droit ne
					// s'adoucit que du flou (grossi > 0), jamais d'un rayon invente
					nkentseu::float32 Rq[4];
					for (nkentseu::uint32 j = 0; j < 4u; ++j)
						Rq[j] = c[j] > 0.f ? c[j] + grossi : (e.flou > 0.5f ? grossi : 0.f);
					NkGRectCoins(p, q, (base & 0xFFFFFF00u) | (a & 0xFFu), Rq);
				}
			}
		}

		// ═══════════════════════════════════════════════════════════════════════
		//  LE TRACÉ ÉDITÉ D'UNE BOÎTE — écrit UNE fois pour rect ET ellipse
		// ═══════════════════════════════════════════════════════════════════════
		/// Peint la forme par sa LISTE DE SOMMETS quand elle en a une, et rend
		/// vrai. Rend faux quand il n'y a rien d'édité : l'appelant garde alors
		/// son peintre paramétrique **mot pour mot**.
		///
		/// ⚠️ CE PEINTRE EXISTE PARCE QUE `rect` ET `ellipse` SONT DES CHEMINS
		///    FRÈRES, et la porte du 28/08 demande de traiter le groupe **dès
		///    l'écriture**, pas après que l'un des deux ait mordu. Les deux
		///    branches sont à soixante lignes l'une de l'autre ; écrit deux fois,
		///    ce code aurait divergé au premier ajustement d'arrondi.
		///
		/// ⚠️ ET LE REPLI EST UN REFUS EXPLICITE, PAS UN DESSIN APPROCHÉ : si la
		///    liste existe mais que le peintre n'a pas de primitive polygone, on
		///    rend **faux** et l'appelant reprend la main. Peindre le rectangle
		///    d'origine par-dessus une forme déformée aurait donné *un dessin faux
		///    qui a l'air juste* — la pire des sorties, déjà nommée sur l'ombre
		///    interne.
		inline bool NkGTraceEdite(NkComponentPaint &p, const NkPaintRect &r, const NkUINode &n,
								  const NkDocumentHost &host, const NkMat2D &mEff) {
			// ⚠️ DEUX RAISONS D'EMPRUNTER CE CHEMIN, PAS UNE, et c'est pour ça que
			//    la condition a grossi au lieu d'être doublée : une forme se peint
			//    par sa liste de points soit parce qu'on a **édité ses sommets**,
			//    soit parce qu'elle est **transformée** (tournée, retournée). Les
			//    deux produisent la même chose — un contour quelconque — et un
			//    second peintre écrit à côté aurait divergé au premier arrondi.
			// LA MATRICE EFFECTIVE, PAS LA TRANSFORMEE PROPRE : elle porte deja
			// celles des ancetres, chacune autour de SON centre.
			if (n.sommets.Empty() && mEff.Identite())
				return false;
			nkentseu::float32 xy[256];
			const nkentseu::uint32 nb = NkContourDe(n, r, xy, 128);
			if (nb < 3)
				return false;
			// ⚠️ LA TRANSFORMATION S'APPLIQUE APRÈS LE CONTOUR, JAMAIS AVANT : les
			//    arcs d'arrondi se calculent dans le repère propre de la forme.
			//    Tournés d'abord, les rayons auraient été bornés contre des côtés
			//    déjà pivotés — un arrondi qui change de taille quand on tourne
			//    l'objet.
			NkMatContour(mEff, xy, nb);
			const nkentseu::uint32 rgba =
				n.FondEffectif() ? NkGFondRGBA(n) : p.ColorOf(host.Role("doc_field_bg"));
			return p.PolygonHex(xy, (nkentseu::int32)nb, rgba);
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
			p.TextHex(label, name, p.ColorOf(host.Role("text")), host.Role("text"),
					  NkTextAlign::Left, 12.f * host.docScale, 0.f);
			label.y += p.LineHeight() + 2.f;
			p.TextHex(label, "declare, dessin non branche",
					  p.ColorOf(host.Role("text_muted")), host.Role("text_muted"),
					  NkTextAlign::Left, 12.f * host.docScale, 0.f);
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
		/// `mEff` : LA COMPOSITION DU NOEUD ET DE SES ANCETRES, calculee une
		/// fois par `NkDrawDocument` et lue AUSSI par le pointage. Deux calculs
		/// auraient laisse un objet se voir a un endroit et se cliquer a un autre.
		/// ⑤ `heritee` : le produit des opacités du nœud et de tous ses ancêtres,
		/// 0..1. Défaut 1 -- les appelants d'aperçu (la palette, les vignettes) ne
		/// changent donc pas d'un pixel.
		inline void DrawShape(NkComponentPaint &p, const NkPaintRect &r, const NkUINode &n,
							  const NkDocumentHost &host, const NkMat2D &mEff,
							  nkentseu::float32 heritee = 1.f) {
			// Une forme OUVERTE (ligne) n'a besoin que d'une dimension : une ligne
			// horizontale a une hauteur NULLE, et elle doit se voir (sonde 52).
			if (NkFormeOuverte(n) ? (r.w <= 0.f && r.h <= 0.f) : (r.w <= 0.f || r.h <= 0.f))
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
				// ⚠️ L'ARTBOARD IGNORAIT SON PROPRE RAYON, sur ses DEUX portes
				//    (dessin normal et renommage d'etiquette). Une page a
				//    laquelle Rodolf donne un arrondi se peignait carree.
				float32 Ra[4];
				NkGRayons(n, Ra);
				NkGBornerRayons(r.w, r.h, Ra, Ra);
				const uint32 fondArt = p.ColorOf(host.Role("artboard_bg"));
				NkGRectCoins(p, r, fondArt, Ra);
				NkGCadreTheme(p, r, Ra, host.Role("border"), fondArt);
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
				// ⚠️ UN RECTANGLE DONT ON A ÉDITÉ LES SOMMETS N'EST PLUS UN
				//    RECTANGLE. Dès que la liste existe, il se peint par son TRACÉ
				//    — sans quoi déplacer un coin en mode points aurait bougé la
				//    poignée et **rien d'autre** : un mode d'édition parfaitement
				//    enregistré et parfaitement invisible, exactement le défaut que
				//    la mutation 3 du mode points avait démasqué en Q42.
				// ── LES OMBRES PORTÉES, AVANT LA FORME ───────────────────────
				// ⚠️ AVANT, ET C'EST TOUTE LA DIFFÉRENCE ENTRE UNE OMBRE ET UNE
				//    TACHE. Peinte après, elle recouvrirait ce qu'elle est censée
				//    faire flotter. L'ordre du peintre EST la sémantique.
				// ⚠️ ET LE FLOU EST APPROCHÉ, PAS SIMULÉ : le peintre n'a pas de
				//    primitive floue, donc on empile quelques anneaux de plus en
				//    plus transparents. Ça DIT le flou sans le mentir — et le jour
				//    où une primitive existera, ce site est le seul à changer.
				NkGOmbres(p, r, n, heritee); // par coin : l'ombre lit les quatre rayons du noeud
				// ⚠️ ET LE TRACÉ ÉDITÉ PASSE APRÈS L'OMBRE, POUR LA MÊME RAISON.
				//    Sortir avant `NkGOmbres` aurait fait DISPARAÎTRE l'ombre au
				//    moment précis où l'on déplace un coin — une propriété perdue
				//    par un geste qui n'a rien à voir avec elle.
				// (Q94, 05/09) `NkGTraceEdite` ne sort plus ici : le trace edite est un contour
				// (`traceXY`, calcule plus haut) que les remplissages ci-dessous honorent --
				// uni, degrade, image -- au lieu d'une seule couleur.
				// ⚠️ LE RECTANGLE EMPILE SES REMPLISSAGES, DANS L'ORDRE DE LA
				//    LISTE — le DERNIER par-dessus, comme chez Lunacy. C'est le
				//    seul peintre de forme qui le fasse, parce que c'est le seul
				//    dont la primitive se rappelle sans coût : `FillColor` sur le
				//    même rectangle. Peindre uniquement le remplissage du dessus
				//    aurait rendu une liste de deux INDISTINGUABLE d'une liste
				//    d'un, dès que le premier est translucide.
				// 🔴 `fondPeint` DIT CE QUE LE PEINTRE A FAIT, PAS CE QUE LE MODÈLE
				//    DÉCLARE — et c'est toute la correction du 02/09 (Rodolf : *« un
				//    rectangle neuf a une bordure que je n'ai pas demandée »*).
				//
				//    Le nœud est PROPRE : `CreerForme` n'écrit ni bordure ni fond,
				//    et le fichier ne porte aucune clé. Le contour venait d'ici : le
				//    repli de bordure se déclenche sur `!n.FondEffectif()`, qui
				//    demande *« le modèle déclare-t-il un fond ? »*. Or la branche
				//    juste en dessous EN PEINT UN — le fond de rôle du thème. Les
				//    deux répondaient donc l'inverse l'une de l'autre à la même
				//    question : *cette forme a-t-elle un fond ?*
				//
				// ⚠️ ET LE REPLI RESTE, IL NE DISPARAÎT PAS. Une forme dont
				//    l'utilisateur a masqué TOUS ses remplissages doit rester
				//    visible : là, rien n'est peint, et le contour est la seule
				//    chose qui la rende repérable à l'œil. Ce qu'on corrige n'est
				//    pas « il y a un repli », c'est *« le repli se trompait de
				//    question »*.
				// ── LE DEGRADE, PEINT PAR BANDES ─────────────────────────────
				// ⚠️ LE PEINTRE N'A PAS DE PRIMITIVE DE DEGRADE, et on ne lui en
				//    ajoute pas une pour ce lot : on peint des BANDES de couleur
				//    interpolee avec `FillColor`, la porte qui existe deja.
				//    Consequences assumees, et c'est pour ca qu'elles sont
				//    ecrites : ca traverse TOUS les backends sans code par
				//    backend, ca passe par le peintre ENREGISTREUR (donc le
				//    temoin le voit), et la finesse est bornee par le nombre de
				//    bandes. *Une approximation nommee vaut mieux qu'une
				//    primitive inventee dans le noyau pour un premier jet.*
				// ⚠️ L'ANGLE N'EST PAS HONORE ENCORE : les bandes sont
				//    HORIZONTALES. Le dire ici plutot que laisser croire qu'un
				//    angle de 90° fait quelque chose -- un champ que le fichier
				//    porte et que l'ecran ignore est exactement le defaut que ce
				//    chantier repare ailleurs.
				// ── LE TRACE EDITE DONNE UN CONTOUR (Q94, 05/09) -- lu par l'uni, le degrade et
				//    l'image ci-dessous, au lieu du chemin a une couleur qui sortait avant eux ──
				float32 traceXY[256];
				const uint32 traceNb = NkGContourTrace(r, n, mEff, traceXY, 128u);
				// Le repli, pour un peintre sans polygone : des bandes droites, MEME
				// calcul de couleur (une seule verite).
				auto peindreDegradeBandes = [&](const NkDegrade &g) {
					if (!g.Actif())
						return false;
					const int32 kB = renderdetail::NkBandesDegrade(r.h);
					for (int32 b = 0; b < kB; ++b) {
						const float32 t0 = (float32)b / (float32)kB;
						const float32 t1 = (float32)(b + 1) / (float32)kB;
						const NkPaintRect rb{r.x, r.y + r.h * t0, r.w, r.h * (t1 - t0) + 0.5f};
						p.FillColor(rb, renderdetail::NkCouleurDegradeEn(g, (t0 + t1) * 0.5f), 0.f);
					}
					return true;
				};
				// ── LE DEGRADE SUIT LA FORME, ET HONORE SON ANGLE ───────────────────
				// Le contour arrondi (8 segments par arc) est decoupe en bandes
				// perpendiculaires a l'axe (Sutherland-Hodgman sur un convexe) ; chaque
				// bande est un polygone, donc elle suit l'arc exactement. Angle : la
				// meme convention que la rotation du noeud (horaire, y vers le bas) --
				// 0 = haut->bas, 90 = droite->gauche, 180 = bas->haut, 270 = gauche->droite.
				// L'etendue de l'axe est celle du rectangle projete (w|sin| + h|cos|).
				auto peindreDegrade = [&](const NkDegrade &g) {
					if (!g.Actif())
						return false;
					if (r.w <= 0.f || r.h <= 0.f)
						return true;
					float32 Rg[4], cg[4];
					NkGRayons(n, Rg);
					NkGBornerRayons(r.w, r.h, Rg, cg);
					float32 contour[80];
					const uint32 nc = NkGContourArrondi(r, cg, contour, 8u);
					// Q94 : un trace edite se rogne par ses morceaux convexes (le contour, ou ses triangles)
					renderdetail::NkGMorceaux morceaux;
					if (traceNb >= 3u)
						renderdetail::NkGMorceauxDe(traceXY, traceNb, morceaux);
					else
						renderdetail::NkGMorceauxDe(contour, nc, morceaux);
					const float32 *contourPlein = traceNb >= 3u ? traceXY : contour;
					const uint32 ncPlein = traceNb >= 3u ? traceNb : nc;
					float32 s = 0.f, c = 1.f;
					NkSinCosDeg(g.angle, s, c);
					const float32 dx = -s, dy = c; // l'axe : (0,1) tourne de `angle`
					// ── RADIAL, ANGULAIRE, LOSANGE : la meme methode, des bandes qui SUIVENT la
					//    forme (Rodolf, 04/09 : « les autres ne correspondent pas vraiment » -- ils
					//    etaient peints comme un lineaire). Radial et losange : des disques /
					//    losanges concentriques peints de l'EXTERIEUR vers l'interieur (t = 1 sur
					//    la demi-boite ; les coins, au-dela, portent la couleur du dernier arret) ;
					//    angulaire : des secteurs depuis le centre, l'origine = l'angle. Chaque
					//    bande est decoupee par le contour arrondi : elle suit l'arc exactement.
					const int32 genre = renderdetail::NkGenreDegrade(g);
					if (genre != 0) {
						// ① l'origine et les rayons : la MEME geometrie que les poignees de la toile
						const renderdetail::NkGeomDegrade gm = renderdetail::NkGeomDegradeDe(r, g);
						const float32 cx0 = gm.ox, cy0 = gm.oy;
						const float32 rx = gm.rx, ry = gm.ry;
						float32 forme[96];
						bool su = true;
						if (genre == 1 || genre == 3) {
							// le fond : la couleur du dernier arret sur tout le contour (les coins)
							su = p.PolygonHex(contourPlein, (int32)ncPlein, renderdetail::NkCouleurDegradeEn(g, 1.f));
							const int32 kB = renderdetail::NkBandesDegrade(2.f * (rx > ry ? rx : ry));
							for (int32 b = kB - 1; b >= 0 && su; --b) {
								const float32 t1 = (float32)(b + 1) / (float32)kB;
								const float32 tm = ((float32)b + 0.5f) / (float32)kB;
								uint32 nf = 0u;
								if (genre == 1) {
									for (uint32 k = 0; k < 32u; ++k) {
										float32 sk = 0.f, ck = 1.f;
										NkSinCosDeg(360.f * (float32)k / 32.f, sk, ck);
										forme[nf * 2] = cx0 + rx * t1 * ck;
										forme[nf * 2 + 1] = cy0 + ry * t1 * sk;
										++nf;
									}
								} else {
									forme[0] = cx0 + rx * t1; forme[1] = cy0;
									forme[2] = cx0; forme[3] = cy0 + ry * t1;
									forme[4] = cx0 - rx * t1; forme[5] = cy0;
									forme[6] = cx0; forme[7] = cy0 - ry * t1;
									nf = 4u;
								}
								renderdetail::NkGRognerParMorceaux(forme, nf, morceaux, [&](const float32 *cp, uint32 ncp) {
									if (!p.PolygonHex(cp, (int32)ncp, renderdetail::NkCouleurDegradeEn(g, tm)))
										su = false;
								});
							}
						} else {
							// angulaire : des secteurs de 360/kB degres, l'origine dans la direction de l'axe
							const int32 kB = renderdetail::NkBandesDegrade(2.f * (r.w + r.h));
							const float32 R = 2.f * (r.w + r.h); // au-dela de la boite quelle que soit l'origine : le contour decoupe
							for (int32 b = 0; b < kB && su; ++b) {
								const float32 a0 = g.angle + 360.f * (float32)b / (float32)kB;
								const float32 a1 = g.angle + 360.f * (float32)(b + 1) / (float32)kB;
								const float32 tm = ((float32)b + 0.5f) / (float32)kB;
								uint32 nf = 0u;
								forme[nf * 2] = cx0; forme[nf * 2 + 1] = cy0; ++nf;
								for (uint32 k = 0; k < 3u; ++k) {
									float32 sk = 0.f, ck = 1.f;
									NkSinCosDeg(a0 + (a1 - a0) * (float32)k / 2.f, sk, ck);
									// la direction de l'axe pour l'angle a : (-sin a, cos a), horaire, y vers le bas
									forme[nf * 2] = cx0 - sk * R;
									forme[nf * 2 + 1] = cy0 + ck * R;
									++nf;
								}
								renderdetail::NkGRognerParMorceaux(forme, nf, morceaux, [&](const float32 *cp, uint32 ncp) {
									if (!p.PolygonHex(cp, (int32)ncp, renderdetail::NkCouleurDegradeEn(g, tm)))
										su = false;
								});
							}
						}
						if (su)
							return true;
						return peindreDegradeBandes(g); // ce peintre n'a pas de polygone : repli
					}
					const float32 etendue = r.w * (dx < 0.f ? -dx : dx) + r.h * (dy < 0.f ? -dy : dy);
					if (etendue <= 0.001f)
						return peindreDegradeBandes(g);
					const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
					const int32 kBandes = renderdetail::NkBandesDegrade(etendue);
					bool polygoneSu = true;
					for (int32 b = 0; b < kBandes && polygoneSu; ++b) {
						const float32 t0 = (float32)b / (float32)kBandes;
						const float32 t1 = (float32)(b + 1) / (float32)kBandes;
						// la bande : t0 <= t < t1, avec t = ((P - C).d) / etendue + 0.5
						for (uint32 mk = 0; mk < morceaux.n && polygoneSu; ++mk) { // chaque morceau convexe
						float32 poly[96], tmp[96];
						uint32 np = morceaux.nb[mk];
						for (uint32 k = 0; k < np * 2u; ++k)
							poly[k] = morceaux.pts[morceaux.debut[mk] + k];
						for (int32 cote = 0; cote < 2 && np >= 3u; ++cote) {
							// demi-plan garde : cote 0 -> t >= t0 ; cote 1 -> t <= t1
							const float32 seuil = (cote == 0 ? t0 : t1) - 0.5f;
							auto valeur = [&](float32 x, float32 y) -> float32 {
								const float32 tp = ((x - cx) * dx + (y - cy) * dy) / etendue - seuil;
								return cote == 0 ? tp : -tp; // >= 0 : garde
							};
							uint32 nt = 0;
							for (uint32 k = 0; k < np; ++k) {
								const float32 ax = poly[k * 2], ay = poly[k * 2 + 1];
								const uint32 j = (k + 1u) % np;
								const float32 bx = poly[j * 2], by = poly[j * 2 + 1];
								const float32 va = valeur(ax, ay), vb = valeur(bx, by);
								if (va >= 0.f) {
									tmp[nt * 2] = ax;
									tmp[nt * 2 + 1] = ay;
									++nt;
								}
								if ((va >= 0.f) != (vb >= 0.f) && nt < 46u) {
									const float32 f = va / (va - vb);
									tmp[nt * 2] = ax + (bx - ax) * f;
									tmp[nt * 2 + 1] = ay + (by - ay) * f;
									++nt;
								}
								if (nt >= 46u)
									break;
							}
							np = nt;
							for (uint32 k = 0; k < np * 2u; ++k)
								poly[k] = tmp[k];
						}
						if (np < 3u)
							continue; // la bande ne touche pas ce morceau (coins tres arrondis, ou triangle hors bande)
						// la couleur au MILIEU de la bande : une bande represente son
						// intervalle, pas son bord (sinon la derniere n'atteint jamais
						// la couleur du dernier arret)
						const uint32 colB = renderdetail::NkCouleurDegradeEn(g, (t0 + t1) * 0.5f);
						if (!p.PolygonHex(poly, (int32)np, colB))
							polygoneSu = false; // ce peintre n'a pas de polygone : repli
						} // les morceaux
					}
					if (!polygoneSu)
						return peindreDegradeBandes(g);
					return true;
				};
				// LES QUATRE RAYONS EFFECTIFS, lus par la porte du modèle.
				float32 Rc[4];
				NkGRayons(n, Rc);
				// L'UNI : le contour du trace edite s'il y en a un (et si le peintre sait le
				// polygone), la boite arrondie sinon -- une seule porte pour les trois sites
				auto peindreUni = [&](uint32 rgba) {
					if (traceNb < 3u || !p.PolygonHex(traceXY, (int32)traceNb, rgba))
						NkGRectCoins(p, r, rgba, Rc);
				};
				// La couleur du fond réellement peint — l'anneau de bordure la
				// remet au milieu (voir NkGCadre). 0 = aucun fond.
				uint32 rgbaFond = 0u;
				bool fondPeint = false;
				if (!n.fills.Empty()) {
					bool peint = false;
					for (uint32 fi = 0; fi < (uint32)n.fills.Size(); ++fi) {
						const NkRemplissage &f = n.fills[fi];
						if (!f.visible)
							continue;
						// ②-2 le mode de fusion EXACT de ce remplissage, pousse au peintre le temps
						//     de le peindre (Multiply, Screen, Darken, Lighten, Plus Lighter)
						const renderdetail::NkGardeFusion gardeFusion(p, f.fusion);
						// LE DEGRADE PRIME SUR LA COULEUR UNIE du meme
						// remplissage : c'est ce que l'utilisateur a pose en
						// dernier, et les deux ne peuvent pas coexister a
						// l'ecran.
						// UN REMPLISSAGE IMAGE : la source n'est pas chargee (le peintre n'a
						// pas de primitive d'image, le document pas de source) -- on peint
						// le DAMIER, comme le popover de Lunacy pour une image absente, et
						// c'est dit dans l'inspecteur. Le cadrage sera lu par le vrai peintre.
						if (f.EstImage()) {
							// LA SOURCE, si le fournisseur la donne et si le peintre sait
							// peindre une image ; sinon le damier, et l'absence est notee
							if (!NkGPeindreImage(p, r, Rc, f, traceNb >= 3u ? traceXY : nullptr, traceNb,
												 heritee))
								NkGDamier(p, r, Rc, f.opacite, heritee);
							peint = true;
							continue;
						}
						if (peindreDegrade(f.degrade)) {
							peint = true;
							continue;
						}
						if (f.couleur.Empty())
							continue;
						const uint32 base = NkGCouleur(f.couleur.Data());
						const float32 k = NkKOpacite(f.opacite, heritee);
						const uint32 a = (uint32)((base & 0xFFu) * k + 0.5f);
						const uint32 rgbaF = (base & 0xFFFFFF00u) | (a & 0xFFu);
						peindreUni(rgbaF);
						// ⚠️ ON RETIENT LE DERNIER FOND PEINT : c'est lui que
						//    l'anneau de bordure devra remettre au milieu. Sans
						//    ça, la bordure arrondie masquerait le remplissage.
						rgbaFond = rgbaF;
						peint = true;
					}
					// TOUT masqué : on ne retombe PAS sur le rôle du thème — un
					// nœud dont l'utilisateur a fermé tous les yeux doit paraître
					// vide, pas « par défaut ».
					if (!peint) {
					// 🔴 `OutlineSharp` ET NON UN ANNEAU ARRONDI, ET C'EST UNE
					//    LIMITE, PAS UN CHOIX. Ici AUCUN fond n'a ete peint : il
					//    n'existe donc pas de couleur pour creuser l'interieur, et
					//    ce peintre n'a ni primitive d'anneau ni masque.
					//    `OutlineSharp` (`AddRect`, non rempli) est la SEULE forme
					//    creuse disponible -- et elle ne sait pas arrondir.
					//    J'ai essaye l'anneau : il peignait un BLOC PLEIN couleur
					//    bordure par-dessus le noeud, et le banc l'a vu.
					//    Manque porte au canal : une primitive de contour arrondi
					//    (ou un decoupage arrondi) dans NkComponentPaint.
						p.OutlineSharp(r, host.Role("border"));
					}
					fondPeint = peint;
				} else if (!n.fill.Empty()) {
					rgbaFond = NkGCouleur(n.fill.Data());
					peindreUni(rgbaFond);
					fondPeint = true;
				} else {
					rgbaFond = p.ColorOf(host.Role("doc_field_bg"));
					peindreUni(rgbaFond);
					fondPeint = true; // le fond de rôle EST un fond
				}
				// ── LES BORDURES D'UN TRACE EDITE : L'ANNEAU (05/09) ──────────────
				// Une epaisseur UNIFORME (« par cote » n'a plus de sens sans quatre cotes --
				// l'inspecteur grise la rangee et le dit), la position et la jointure du
				// modele ; sans polygone au peintre, le contour de la boite dit qu'il y a
				// une bordure.
				if (traceNb >= 3u) {
					bool unTrait = false;
					auto anneau = [&](const NkBordure &b) {
						if (!b.visible || b.couleur.Empty() || b.epaisseur <= 0.f)
							return;
						unTrait = true;
						if (!NkGAnneauTrace(p, traceXY, traceNb, NkGBordureRGBA(b, heritee), b.epaisseur, b.position, b.jointure.Data()))
							p.OutlineSharp(r, host.Role("border"));
					};
					for (uint32 bi = 0; bi < (uint32)n.borders.Size(); ++bi)
						anneau(n.borders[bi]);
					if (n.borders.Empty() && !n.borderColor.Empty()) {
						NkBordure b;
						b.couleur = n.borderColor;
						b.epaisseur = n.borderW > 0.f ? n.borderW : 1.f;
						b.position = NkBordurePos::Interieur; // le geste historique
						anneau(b);
					}
					if (!unTrait && !fondPeint)
						p.OutlineSharp(r, host.Role("border"));
					return;
				}
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
						NkGCadre(p, r, b, Rc, rgbaFond, heritee);
						trace = true;
					}
					if (!trace && !fondPeint) {
					// 🔴 `OutlineSharp` ET NON UN ANNEAU ARRONDI, ET C'EST UNE
					//    LIMITE, PAS UN CHOIX. Ici AUCUN fond n'a ete peint : il
					//    n'existe donc pas de couleur pour creuser l'interieur, et
					//    ce peintre n'a ni primitive d'anneau ni masque.
					//    `OutlineSharp` (`AddRect`, non rempli) est la SEULE forme
					//    creuse disponible -- et elle ne sait pas arrondir.
					//    J'ai essaye l'anneau : il peignait un BLOC PLEIN couleur
					//    bordure par-dessus le noeud, et le banc l'a vu.
					//    Manque porte au canal : une primitive de contour arrondi
					//    (ou un decoupage arrondi) dans NkComponentPaint.
						p.OutlineSharp(r, host.Role("border"));
					}
				} else if (!n.borderColor.Empty()) {
					NkBordure b;
					b.couleur = n.borderColor;
					b.epaisseur = n.borderW > 0.f ? n.borderW : 1.f;
					b.position = NkBordurePos::Interieur; // le geste historique
					NkGCadre(p, r, b, Rc, rgbaFond, heritee);
				} else if (!fondPeint) {
					// 🔴 `OutlineSharp` ET NON UN ANNEAU ARRONDI, ET C'EST UNE
					//    LIMITE, PAS UN CHOIX. Ici AUCUN fond n'a ete peint : il
					//    n'existe donc pas de couleur pour creuser l'interieur, et
					//    ce peintre n'a ni primitive d'anneau ni masque.
					//    `OutlineSharp` (`AddRect`, non rempli) est la SEULE forme
					//    creuse disponible -- et elle ne sait pas arrondir.
					//    J'ai essaye l'anneau : il peignait un BLOC PLEIN couleur
					//    bordure par-dessus le noeud, et le banc l'a vu.
					//    Manque porte au canal : une primitive de contour arrondi
					//    (ou un decoupage arrondi) dans NkComponentPaint.
					p.OutlineSharp(r, host.Role("border"));
				}
				return;
			}
			if (shape && StrEq(shape, "ellipse")) {
				// L'ELLIPSE (Lunacy : outil O). Le peintre peut ne pas savoir la
				// dessiner (défaut inerte de l'interface) : le repli est VISIBLE —
				// le contour de sa boîte + le nom, jamais un vide silencieux.
				// ⚠️ LE CHEMIN FRÈRE DU RECTANGLE, ET IL EST TRAITÉ AU MÊME MOMENT :
				//    une ellipse éditée au sommet est un TRACÉ, plus une ellipse.
				//    L'écrire ici en même temps que là-haut est le geste de la porte
				//    du 28/08 — écrit plus tard, ce serait un rect qui se déforme et
				//    une ellipse qui n'obéit pas, sans que rien ne le dise.
				if (NkGTraceEdite(p, r, n, host, mEff))
					return;
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
				float32 ax = r.x, ay = monte ? r.y + r.h : r.y;
				float32 bx = r.x + r.w, by = monte ? r.y : r.y + r.h;
				// LA BORDURE D'UNE LIGNE : sa premiere bordure visible donne couleur,
				// epaisseur et EXTREMITES (plate / ronde / carree) ; sans bordure, le
				// trait d'avant (role doc_text, 2 px, plate).
				const NkBordure *bd = nullptr;
				for (uint32 bi = 0; bi < (uint32)n.borders.Size() && !bd; ++bi)
					if (n.borders[bi].visible && !n.borders[bi].couleur.Empty() && n.borders[bi].epaisseur > 0.f)
						bd = &n.borders[bi];
				const float32 ep = bd ? bd->epaisseur : 2.f;
				const char *ext = bd ? bd->extremite.Data() : "";
				float32 ux = bx - ax, uy = by - ay;
				const float32 lg2 = ux * ux + uy * uy;
				if (lg2 > 0.0001f) {
					float32 inv = 1.f;
					{ // 1/sqrt par Newton, sans <cmath>
						float32 g = lg2 > 1.f ? lg2 * 0.5f : 1.f;
						for (int32 it = 0; it < 24; ++it)
							g = 0.5f * (g + lg2 / g);
						inv = 1.f / g;
					}
					ux *= inv;
					uy *= inv;
				} else {
					ux = 1.f;
					uy = 0.f;
				}
				if (ext && StrEq(ext, "carree")) { // prolongee d'une demi-epaisseur
					ax -= ux * ep * 0.5f;
					ay -= uy * ep * 0.5f;
					bx += ux * ep * 0.5f;
					by += uy * ep * 0.5f;
				}
				const uint32 rgbaL = bd ? NkGBordureRGBA(*bd, heritee)
										: p.ColorOf(host.Role("doc_text"));
				bool ok = false;
				if (bd) { // couleur propre : le trait est un quadrilatere (rgba)
					const float32 nx = -uy * ep * 0.5f, ny = ux * ep * 0.5f;
					const float32 quad[8] = {ax + nx, ay + ny, bx + nx, by + ny, bx - nx, by - ny, ax - nx, ay - ny};
					ok = p.PolygonHex(quad, 4, rgbaL);
				}
				if (!ok)
					ok = p.Line(ax, ay, bx, by, host.Role("doc_text"), ep);
				if (ok && ext && StrEq(ext, "ronde")) { // deux disques aux bouts
					for (int32 bout = 0; bout < 2; ++bout) {
						const float32 cx = bout ? bx : ax, cy = bout ? by : ay;
						float32 disque[32];
						for (uint32 k = 0; k < 16u; ++k) {
							float32 s = 0.f, c = 1.f;
							NkSinCosDeg(22.5f * (float32)k, s, c);
							disque[k * 2] = cx + c * ep * 0.5f;
							disque[k * 2 + 1] = cy + s * ep * 0.5f;
						}
						p.PolygonHex(disque, 16, rgbaL); // sans polygone : plate, et c'est dit ici
					}
				}
				if (!ok)
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
				// ⚠️ LES SOMMETS VIENNENT DE `Sommets.h`, PAS D'UNE TABLE LOCALE.
				//    Le mode points doit poser une poignee SUR CHAQUE SOMMET
				//    DESSINE : deux tables auraient donne des poignees qui derivent
				//    du dessin au premier ajustement d'une etoile.
				// ⚠️ ET C'EST `NkContourDe`, PAS `NkSommetsDe` : le contour est la
				//    meme liste d'ancres, arcs d'arrondi compris. L'appel a ete
				//    change ICI EN MEME TEMPS que rect et ellipse ont recu le leur
				//    -- laisser le polygone sur les ancres nues aurait rendu
				//    l'arrondi par sommet visible sur deux formes sur cinq, et
				//    silencieusement inerte sur les trois autres.
				float32 xy[256];
				const uint32 nb = NkContourDe(n, r, xy, 128);
				// ⚠️ LE MEME GESTE QUE POUR LE RECT ET L'ELLIPSE, AU MEME MOMENT :
				//    un polygone tourne se peint tourne. Ecrit seulement la-haut,
				//    l'etoile aurait ete la seule forme a ignorer la rotation.
				NkMatContour(mEff, xy, nb);
				if (nb == 0 || !p.PolygonHex(xy, (int32)nb, rgba))
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
				// ⚠️ LE FOND SUIVAIT LE RAYON UNIFORME, LE CONTOUR AUCUN : le
				//    coin etait donc arrondi ET souligne d'un angle droit.
				//    Les deux lisent maintenant LES QUATRE rayons.
				float32 Ri[4];
				NkGRayons(n, Ri);
				// Le repli historique a 2 px quand AUCUN coin n'est pose --
				// garde tel quel : le retirer changerait le dessin de tous les
				// cadres d'image existants, ce qui n'est pas l'objet ici.
				if (Ri[0] <= 0.f && Ri[1] <= 0.f && Ri[2] <= 0.f && Ri[3] <= 0.f)
					Ri[0] = Ri[1] = Ri[2] = Ri[3] = 2.f;
				NkGBornerRayons(r.w, r.h, Ri, Ri);
				NkGRectCoins(p, r, rgba, Ri);
				// Le fond VIENT d'etre peint : sa couleur creuse l'anneau.
				NkGCadreTheme(p, r, Ri, host.Role("border"), rgba);
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
											: NkGCouleur(n.textColor.Data());
					const float32 corps = (n.fontPx > 0.f ? n.fontPx : 12.f) * host.docScale;
					p.TextHex(r, t, rgba, roleTexte, al, corps, n.fontWeight);
				} else
					// LE MEME CORPS QUE LA BRANCHE D'AU-DESSUS : un repli qui ne
					// suit pas le zoom est le defaut que Rodolf a signale DEUX fois.
					p.TextHex(r, name ? name : "Texte", p.ColorOf(roleTexte), roleTexte, al,
							  (n.fontPx > 0.f ? n.fontPx : 12.f) * host.docScale, n.fontWeight);
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
			p.TextHex(label, name, p.ColorOf(host.Role("text")), host.Role("text"),
					  NkTextAlign::Left, 12.f * host.docScale, 0.f);
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

	/// NkMat2D (document) -> NkPaintTransform (peintre) : memes six coefficients,
	/// meme convention (colonnes x, y, 1). Le pont existe pour que le document
	/// ne depende pas du type du kit dans Transfo.h.
	inline nkentseu::editorkit::NkPaintTransform NkPaintTransformDe(const NkMat2D &m) {
		nkentseu::editorkit::NkPaintTransform t;
		t.a = m.a;
		t.b = m.b;
		t.c = m.c;
		t.d = m.d;
		t.e = m.e;
		t.f = m.f;
		return t;
	}

	/// ⑤ `opaciteHeritee` : le produit des opacités des ancêtres, 0..1. Il
	/// descend PAR LA RECURSION, comme `masque` se transmet par son `return` --
	/// aucun drapeau n'est recopié sur les descendants, donc rien ne peut dériver
	/// quand un nœud change de parent.
	inline void NkDrawDocument(NkComponentPaint &p, const NkComponentInput &in, const NkUIDocument &doc,
							   const NkLayoutResult &lay, NkDocumentHost &host, int32 node = 0,
							   nkentseu::float32 opaciteHeritee = 1.f) {
		if (node == 0)
			renderdetail::NkPoserResolveur(&doc); // les references « @cle » se resolvent ici
		if (!doc.IsValidIndex(node) || !lay.Has(node))
			return;
		// ── MASQUE : NI LUI, NI SA DESCENDANCE (vague 2) ────────────────────
		// ⚠️ LE `return` EST AVANT LA RECURSION, ET C'EST CE QUI DONNE
		//    L'HERITAGE GRATUITEMENT : on ne descend pas dans les enfants d'un
		//    nœud masque, donc le sous-arbre entier disparait sans qu'aucun
		//    drapeau soit recopie sur les descendants. *Un drapeau recopie est un
		//    drapeau qui derive au premier nœud deplace.*
		// 📌 `verrouille` NE CHANGE RIEN ICI : un objet verrouille reste
		//    parfaitement VISIBLE -- c'est le pointage qui l'ignore. Les deux
		//    drapeaux ont deux effets, et les confondre donnerait soit un verrou
		//    invisible, soit un masque qu'on attrape encore.
		if (doc.nodes[(uint32)node].masque)
			return;
		const NkUINode &n = doc.nodes[(uint32)node];
		const NkPaintRect r = lay.At(node);
		// ⑤ L'OPACITÉ EFFECTIVE de ce nœud : la sienne, fois celle de ses ancêtres.
		// ⚠️ UNE OPACITÉ NULLE NE COUPE PAS LA RÉCURSION, contrairement à `masque` :
		//    les deux ne disent pas la même chose. Un nœud masqué est RETIRÉ ; un
		//    nœud à 0 % est toujours là, simplement invisible -- et le pointage doit
		//    continuer de le trouver. Confondre les deux ferait disparaître un objet
		//    qu'on ne pourrait plus rattraper qu'en le cherchant dans l'arbre.
		const nkentseu::float32 opaciteEff =
			opaciteHeritee * renderdetail::NkKOpacite(n.opacite, 1.f);
		// ⑥ (07/09) LE MODE DE FUSION DU NŒUD, VERSION PARTIELLE : par COMMANDE.
		//    La garde couvre le dessin du nœud ET sa descendance (elle vit jusqu'au
		//    bout de la fonction) -- c'est ce qu'un mode de calque veut dire. La pile
		//    de mélange de NKGui est une VRAIE pile (profondeur 16) : un enfant qui
		//    pose le sien reprend la main pour ses propres commandes, puis rend.
		// ⚠️ ON REUTILISE `NkGardeFusion`, celle des remplissages -- on n'en écrit
		//    pas une seconde. Cinq modes exacts au GPU, treize « enregistrés, pas
		//    peints » : la garde ne pousse rien pour ceux-là, donc rien n'est
		//    approché. *Un repli qui reste plausible est pire qu'un refus.*
		// ⚠️ CE N'EST PAS UN CALQUE COMPOSITÉ UNE SEULE FOIS : le nœud se fond avec
		//    ce qui est déjà sur la toile. Le vrai calque demande une cible hors
		//    écran -- chiffré (Q121), pas commencé.
		const renderdetail::NkGardeFusion gardeFusionNoeud(p, n.fusion);
		// ── LA MATRICE DU NOEUD, DANS LE PEINTRE ──────────────────────────
		// Tout ce que ce noeud dessine (forme, composant, texte) passe par
		// elle : rotation, miroirs, echelle, ancetres compris. Depilee AVANT
		// les enfants : chacun empile sa propre matrice effective, deja
		// composee par NkMatEffective -- le peintre n'emboite jamais deux
		// niveaux du document. Aucune sortie anticipee entre Push et Pop.
		// Un noeud droit n'empile RIEN : il emet exactement ce qu'il emettait,
		// et un cadre d'agencement reste invisible (sonde 41c).
		const NkMat2D mEff = NkMatEffective(doc, lay, node);
		const bool empile = !mEff.Identite();
		if (empile)
			p.PushTransform(NkPaintTransformDe(mEff));

		// UN GROUPE NE PEINT RIEN (§15.13) : un groupe declare (genre) sans forme
		// ni composant n'a pas de dessin propre -- son apparence est celle de ses
		// feuilles. Les noeuds non declares gardent leur chemin d'avant.
		if (n.shape.Empty() && n.component.Empty() && !n.genre.Empty()) {
			// rien
		} else if (n.IsFrame()) {
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
				// UNE SEULE COMPOSITION, LUE PAR LES DEUX CHEMINS : celle que le
				// pointage utilise deja (`NkPointDansNoeud` -> `NkMatEffective`).
				renderdetail::DrawShape(p, r, n, host, NkMat2D{}, opaciteEff); // le peintre transforme
			else if (posee && host.editionEtiquette) {
				// RENOMMAGE d'etiquette : le CORPS de l'artboard se dessine,
				// seule l'etiquette se tait (le champ superpose la remplace).
				// ⚠️ L'ARTBOARD IGNORAIT SON PROPRE RAYON, sur ses DEUX portes
				//    (dessin normal et renommage d'etiquette). Une page a
				//    laquelle Rodolf donne un arrondi se peignait carree.
				float32 Ra[4];
				renderdetail::NkGRayons(n, Ra);
				renderdetail::NkGBornerRayons(r.w, r.h, Ra, Ra);
				const uint32 fondArt = p.ColorOf(host.Role("artboard_bg"));
				renderdetail::NkGRectCoins(p, r, fondArt, Ra);
				renderdetail::NkGCadreTheme(p, r, Ra, host.Role("border"), fondArt);
			} else if (!posee)
				renderdetail::DrawFrame(p, r, host);
		} else if (basiques::NkBasiqueDe(n.component.Data())) {
			// ── LES COMPOSANTS DE BASE ────────────────────────────────────────
			// 🔑 UNE SEULE BRANCHE POUR HUIT COMPOSANTS, et ce n'est pas une
			//    economie de frappe : `NkDessinerBasique` lit LA TABLE, donc le
			//    neuvieme se dessinera sans qu'on revienne ici. *Une branche par
			//    composant aurait fait huit endroits ou oublier le neuvieme.*
			//
			// ⚠️ LES ROLES SONT RESOLUS ICI, PAS DANS LE DESSIN : le composant de
			//    base ne connait pas le theme, il recoit des roles deja resolus.
			//    C'est ce qui lui permet de vivre dans un fichier sans dependre du
			//    theme de l'editeur -- et ce qui rendra son extraction vers le kit
			//    mecanique le jour venu.
			// ⚠️ LE NOEUD NE PORTE NI VARIANTE NI PARAMETRES AUJOURD'HUI :
			//    `NkUINode` n'a que `component` (le nom). On dessine donc l'ETAT
			//    PAR DEFAUT declare, et **on ne l'invente pas** -- lire un champ
			//    qui n'existe pas aurait ete un mensonge de plus. La variante et
			//    les parametres par instance sont un ajout de MODELE (cles
			//    additives), nomme au rapport comme le lot suivant.
			//    Le LIBELLE, lui, vient du nœud : `text` existe deja, et c'est ce
			//    qui rend ces composants utiles des maintenant.
			const float32 vparam = basiques::NkValeurParDefaut(n.component.Data());
			basiques::NkDessinerBasique(
				p, r, n.component.Data(), vparam, n.text.Data(), 0u,
				host.Role("accent_ui"), host.Role("text_on_accent"), host.Role("doc_field_bg"),
				host.Role("border"), host.Role("doc_text"), host.Role("text_muted"),
				n.RayonCoin(0), host.docScale);
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
		if (empile)
			p.PopTransform(); // avant les enfants : chacun empile la sienne

		for (uint32 i = 0; i < (uint32)n.children.Size(); ++i)
			NkDrawDocument(p, in, doc, lay, host, n.children[i], opaciteEff);
	}

} // namespace nkuidesign
