// =============================================================================
// NkEditMesh.h — NKRenderer
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Maillage éditable en structure DEMI-ARÊTE (half-edge), support des faces à N
// sommets (n-gons), façon BMesh (Blender) / GMesh (Hoppe). Zero-STL.
//
// Rôle : source de vérité CPU pour l'édition de maillage (Edit Mode). Le GPU en
// est un cache : on TRIANGULE (fan) pour produire un mesh de rendu classique.
// Les faces restent des n-gons côté édition ; la triangulation est un détail
// d'affichage/export (choix, comme Blender).
//
// TODO (topologie avancée) — [2026-07-26] Une 2e structure demi-arête parallèle
// (Noge/Topology/NkHalfEdge.h::NkHalfEdgeMesh + NkBooleanOp.h, header-only, jamais
// implémentées ni incluses) a été supprimée au profit de CE maillage, mature et en
// production. Elle déclarait des capacités UNIQUES restées non implémentées : ops
// booléennes mesh BSP (Union/Subtract/Intersect), décimation QEM, subdivision
// Catmull-Clark, lissage Laplacien, analyse genus/caractéristique d'Euler. Si ces
// opérations sont voulues un jour, les implémenter comme FONCTIONS LIBRES opérant sur
// renderer::NkEditMesh (cette classe), et NON via une structure demi-arête concurrente.
// =============================================================================
#pragma once

#include "NkMeshSystem.h" // NkVertex3D + NkVector + NkVec3f/NkVec2f (transitif)

namespace nkentseu {
	namespace renderer {

		// Indices (handles). NK_EM_INVALID = absent.
		using NkEmId = uint32;
		static const NkEmId NK_EM_INVALID = 0xFFFFFFFFu;

		// ── Paramètres des commandes d'édition (niveau namespace : réutilisables par les
		//    modificateurs / l'IA, et défauts utilisables comme arguments par défaut). ──
		// offset < 0 => AUTO (8 % de la diagonale de bbox). offset == 0 => comportement
		// BLENDER (défaut) : la géométrie extrudée naît EXACTEMENT sur l'originale, elle
		// est SÉLECTIONNÉE, et c'est l'utilisateur qui la déplace ensuite (gizmo G/R/S,
		// axe normal par défaut ou contrainte X/Y/Z). Aucun déplacement automatique.
		struct NkExtrudeParams {
				// ── DIRECTION D'EXTRUSION (variantes de Blender) ─────────────────────
				// Region      : une SEULE direction pour tout le bloc = moyenne des
				//               normales des faces selectionnees (defaut, E dans Blender).
				// AlongNormals: chaque sommet part le long de SA propre normale (moyenne
				//               des faces selectionnees qui le touchent) — Alt+E « Extrude
				//               Faces Along Normals ». Sur une surface courbe, Region
				//               ecrase le relief alors qu'AlongNormals l'epaissit en
				//               suivant la forme : ce n'est PAS un detail cosmetique.
				// ToCursor    : chaque sommet va vers le point `target` (curseur 3D),
				//               chacun de sa propre distance -> convergence en pointe.
				// Individual (le booleen historique) reste orthogonal : il traite chaque
				// face separement au lieu de la region. Blender l'expose comme une entree
				// distincte du meme menu.
				enum Direction { Region = 0, AlongNormals = 1, ToCursor = 2 };

				bool individual = false;
				float32 offset = 0.f;
				int32 direction = Region;
				NkVec3f target = {0.f, 0.f, 0.f}; // ToCursor : point de convergence
		};

		struct NkMergeParams {
				// Modes de M (Merge) facon Blender. AJOUTES EN FIN (l'op est serialisee).
				//   AtCursor   fusionne au CURSEUR 3D (point fourni en espace maillage) ;
				//   Collapse   chaque ILOT CONNEXE de la selection fusionne vers SON centre
				//              (un merge par region, pas un merge global) ;
				//   ByDistance « Remove Doubles » : seuls les sommets selectionnes plus
				//              proches que `distance` fusionnent, par grappes.
				// First / Last designent bien, comme dans Blender, le PREMIER et le
				// DERNIER SELECTIONNE (ordre des gestes), grace au rang porte par
				// Vert::selOrder. Repli documente : si aucun sommet selectionne ne
				// porte de rang — selection posee par un chemin qui ne passe pas par
				// SetVertSelection (script, chargement) — on retombe sur le plus petit
				// et le plus grand INDICE. Mieux vaut un ordre arbitraire mais defini
				// qu'un refus d'operer.
				enum Mode { Center = 0, First = 1, Last = 2, AtCursor = 3, Collapse = 4, ByDistance = 5 };

				int32 mode = Center;
				NkVec3f point = {0.f, 0.f, 0.f}; // cible AtCursor (espace du maillage)
				float32 distance = 0.f;			 // ByDistance ; <= 0 => 0,1 % de la diagonale bbox
		};

		struct NkSubdivideParams {
				int32 cuts = 1;
		}; // faces sélectionnées, ou TOUT si rien n'est sélectionné

		// LOOP CUT : nombre de boucles insérées dans l'anneau de quads (façon Blender,
		// molette / touches). cuts=1 => une boucle au milieu.
		// slide : GLISSEMENT des boucles insérées LE LONG de l'anneau (le « edge slide »
		//   qui suit Ctrl+R dans Blender). 0 = position médiane (comportement historique) ;
		//   +1 / -1 = boucles rabattues sur l'une ou l'autre des deux boucles bordantes.
		//   Le SENS est cohérent sur TOUT l'anneau : il est établi en le parcourant (chaque
		//   arête de l'anneau retient si son sens « positif » va de son sommet canonique bas
		//   vers le haut, ou l'inverse) — sans quoi une arête sur deux glisserait à
		//   contresens, l'ordre canonique lo->hi n'ayant aucune raison d'être aligné sur la
		//   direction de l'anneau.
		struct NkLoopCutParams {
				int32 cuts = 1;
				float32 slide = 0.f; // -1 .. +1
		};

		// ── BEVEL (chanfrein) façon Blender — Ctrl+B (arêtes) / Ctrl+Shift+B (sommets) ──
		// offset : largeur du chanfrein, MESURÉE LE LONG des arêtes incidentes (proche du
		//   `offset_type='OFFSET'` de Blender). <= 0 => AUTO (6 % de la diagonale de bbox).
		//   Écrêtée par coin à 45 % de la longueur de l'arête -> jamais de repli.
		// segments : 1 = chanfrein PLAT (une bande de faces) ; N > 1 = ARRONDI (N bandes,
		//   profil circulaire obtenu par slerp autour du sommet — profil 0.5 de Blender).
		// vertexOnly : bevel de SOMMET (le coin devient une petite face) au lieu du bevel
		//   d'ARÊTE (chaque arête sélectionnée devient une bande de faces).
		struct NkBevelParams {
				float32 offset = 0.f;
				int32 segments = 1;
				bool vertexOnly = false;
		};

		// ── INSET FACES (I) façon Blender ──────────────────────────────────────────
		// thickness : rétrécissement dans le PLAN de la face. <= 0 => AUTO (8 % de la
		//   diagonale de bbox). Écrêté par coin à 45 % des arêtes incidentes.
		// depth : décalage de la face intérieure LE LONG DE LA NORMALE (creux si < 0).
		// individual : true = chaque face séparément (I puis I dans Blender) ; false =
		//   RÉGION (la sélection est traitée comme un bloc : seules les arêtes de BORD de
		//   la région engendrent la bande, les arêtes intérieures restent partagées).
		struct NkInsetParams {
				float32 thickness = 0.f;
				float32 depth = 0.f;
				bool individual = true;
		};

		// ── EDGE SPLIT / RIP (V) façon Blender ─────────────────────────────────────
		// gap : ÉCARTEMENT appliqué à chaque morceau détaché, le long de la normale
		//   moyenne de son groupe de faces. <= 0 => AUTO (1 % de la diagonale de bbox).
		// ⚠ POURQUOI UN ÉCART EST NÉCESSAIRE ICI : l'adjacence de ce maillage est
		//   POSITIONNELLE (LinkTwins apparie les demi-arêtes sur l'identité soudée, cf.
		//   BuildVertexMerge). Deux sommets laissés EXACTEMENT à la même place seraient
		//   donc immédiatement re-soudés — la déchirure ne survivrait pas au rebuild.
		//   L'écart par défaut est minuscule (1 %) : la topologie est réellement séparée
		//   sans déformation visible, et l'utilisateur écarte ensuite au gizmo (G).
		// ── SONDE DE PHASES DE `BuildFromPolygons` ──────────────────────────────
		// L'entonnoir traverse par TOUTE operation d'edition. `appels` est le
		// controle positif : zero = le chemin n'est pas parcouru, et alors aucune
		// mesure prise dessus ne veut rien dire.
		struct NkEmBfpPhases {
				float64 msLinkTwins = 0.0;
				float64 msRebuildEdges = 0.0;
				float64 msRecomputeNormals = 0.0;
				uint64 appels = 0;
		};
		// ── SONDE DE PHASES DU CHEMIN EN PLACE ──────────────────────────────────
		// Les quatre passes de remise en etat d'`ExtrudeSelectedFacesInPlace`. C'est
		// le chemin DEBRANCHE -- il ne coute rien a l'utilisateur aujourd'hui -- mais
		// c'est le seul ou la region (`touchees`) existe encore au moment ou les
		// passes tournent. L'autre chemin est ferme par le `Clear()` de
		// `BuildFromPolygons`.
		struct NkEmIpPhases {
				float64 msCompactDead = 0.0;
				float64 msVertHedge = 0.0;
				float64 msLinkTwins = 0.0;
				float64 msRecomputeNormals = 0.0;
				uint64 appels = 0;
				uint64 twinsLocaux = 0; ///< combien de fois le chemin LOCAL a ete pris
				uint64 compactRien = 0; ///< CompactDead sorti sans rien compacter
				uint64 compactFait = 0; ///< CompactDead a reellement recopie
		};
		NkEmIpPhases &NkEmIpPhasesGet();

		NkEmBfpPhases &NkEmBfpPhasesGet();
		bool NkEmBfpPhasesActives();

		struct NkEdgeSplitParams {
				float32 gap = 0.f;
		};

		// ── SPIN / RÉVOLUTION (J) façon Blender ────────────────────────────────────
		// Duplique la SÉLECTION en la faisant tourner autour d'un AXE, sur un ANGLE, en
		// N pas, et relie les copies successives par des faces (profil -> surface de
		// révolution : anneau, cylindre, tore…).
		// center / axis sont exprimés dans l'espace de la matrice passée à SpinSelected
		// (= modèle->monde côté éditeur, pour que le CURSEUR 3D serve de centre comme
		// dans Blender ; identité pour une op locale pure).
		// duplicate : true = copies ISOLÉES à chaque pas (Blender « Use Duplicates »),
		//   false (défaut) = copies RELIÉES par une bande de faces.
		struct NkSpinParams {
				NkVec3f center = {0.f, 0.f, 0.f};
				NkVec3f axis = {0.f, 1.f, 0.f};
				float32 angle = 6.2831853f; // radians (360° par défaut)
				int32 steps = 12;
				bool duplicate = false;
		};

		// ── PROPORTIONAL EDITING (touche O dans Blender) ──────────────────────────
		// Un deplacement de la selection ENTRAINE ses voisins, avec une influence qui
		// decroit avec la distance. C'est ce qui permet de deformer une surface sans
		// la plisser : sans lui, bouger un sommet cree un pic ; avec lui, on obtient
		// une bosse continue.
		//
		// La distance est mesuree en DROITE LIGNE (euclidienne) depuis le sommet
		// selectionne le plus proche, comme Blender par defaut. Une distance
		// TOPOLOGIQUE (nombre d'aretes) donnerait un resultat different sur un
		// maillage a densite variable ; ce n'est pas ce mode-ci.
		struct NkProportionalParams {
				// Courbes de Blender. Chacune repond a un besoin different : Smooth pour
				// une bosse organique, Sphere pour un dome net, Root pour un effet qui
				// s'attenue vite, Constant pour deplacer un bloc en bord franc.
				enum Falloff { Smooth = 0, Sphere = 1, Root = 2, Sharp = 3, Linear = 4, Constant = 5 };

				bool enabled = false;
				float32 radius = 0.f; // <= 0 => 25 % de la diagonale de la bbox
				int32 falloff = Smooth;
				bool connectedOnly = false; // reserve (distance topologique) — non implemente
		};

		// ── SYMETRIE DE MAILLAGE (Mesh Symmetry, 1 a 3 axes) ──────────────────────
		// Toute edition appliquee d'un cote est REJOUEE en miroir de l'autre. Blender
		// l'expose comme trois cases X / Y / Z cumulables.
		//
		// Le miroir est etabli par APPARIEMENT DE POSITIONS : pour chaque sommet
		// deplace, on cherche celui qui occupe (a `tolerance` pres) la position
		// symetrique dans le maillage AVANT deplacement, et on lui applique le
		// deplacement reflechi. On ne cree donc AUCUNE geometrie : la symetrie
		// suppose un maillage deja symetrique, exactement comme dans Blender.
		// Un sommet SUR le plan de symetrie est son propre miroir : son deplacement
		// est projete DANS le plan, sinon il quitterait l'axe et casserait la symetrie.
		struct NkSymmetryParams {
				bool x = false, y = false, z = false;
				float32 tolerance = 1e-4f; // appariement des positions miroir
				NkVec3f center = {0.f, 0.f, 0.f}; // plan(s) de symetrie passant par ce point

				bool Any() const {
					return x || y || z;
				}
		};

		// ── TO SPHERE (Shift+Alt+S) façon Blender ─────────────────────────────────
		// Deforme progressivement la selection vers une SPHERE : chaque sommet est
		// interpole entre sa position et sa projection sur la sphere centree sur
		// `center`, de rayon = distance MOYENNE des sommets selectionnes au centre :
		//     P' = lerp(P, center + normalize(P - center) * rayonMoyen, factor)
		// factor = 0 -> inchange · 1 -> sphere parfaite · > 1 autorise (comme Blender).
		// `center` est exprime dans l'espace du MAILLAGE (l'editeur y ramene son pivot
		// courant : median / boite englobante / curseur 3D / element actif).
		// individual = true : chaque FACE entierement selectionnee est spherisee autour
		// de SON propre barycentre (equivalent « origines individuelles »).
		struct NkToSphereParams {
			NkVec3f center = {0.f, 0.f, 0.f};
			float32 factor = 1.f;
			bool individual = false;
		};

		// ── SHRINK / FATTEN (Alt+S dans Blender) ──────────────────────────────────
		// Deplace les sommets selectionnes LE LONG DE LEUR NORMALE. La normale est
		// calculee sur l'identite SOUDEE (moyenne, ponderee par l'aire, des faces
		// incidentes a TOUTES les copies coincidentes du sommet) : sans cela un coin
		// duplique par face partirait dans 3 directions differentes et le maillage se
		// dechirerait. offset > 0 = gonfler · offset < 0 = retrecir.
		struct NkShrinkFattenParams {
			float32 offset = 0.f;
		};

		// ── DISSOLVE (Ctrl+X) façon Blender ────────────────────────────────────────
		// À NE PAS CONFONDRE AVEC « SUPPRIMER » (X) : le dissolve retire l'élément en
		// GARDANT la surface connectée — les faces voisines fusionnent en un n-gon —,
		// là où la suppression laisse un TROU.
		//   Verts : les faces autour de chaque sommet sélectionné fusionnent en une seule.
		//   Edges : les deux faces de chaque arête sélectionnée fusionnent (exact inverse
		//           d'une subdivision d'arête).
		//   Faces : les faces sélectionnées CONTIGUËS fusionnent (arêtes intérieures
		//           retirées) en un seul n-gon.
		// mode : 0 = Verts, 1 = Edges, 2 = Faces (l'appelant le choisit selon le mode de
		// sélection actif V/E/F, comme le Ctrl+X contextuel de Blender).
		struct NkDissolveParams {
				int32 mode = 1;
		};

		class NkEditMesh {
			public:
				struct Vert {
						NkVec3f pos = {0.f, 0.f, 0.f};
						NkVec3f normal = {0.f, 1.f, 0.f};
						NkVec2f uv = {0.f, 0.f};
						// ── ATTRIBUTS CONSERVÉS POUR L'ALLER-RETOUR ───────────────────
						// Ces trois champs ne servent PAS à l'édition topologique. Ils
						// existent pour qu'entrer en mode édition puis en ressortir soit
						// une IDENTITÉ. Sans eux, Triangulate() les RÉINVENTAIT à la
						// sortie (tangent={1,0,0}, color=blanc) : la géométrie restait
						// intacte au micron près, mais le repère tangent de tout le
						// maillage changeait, donc son rendu aussi. Constaté sur les
						// sphères ET les cubes de la démo (écart max 220 sur 255).
						// Règle générale : ce que la structure ne sait pas représenter,
						// elle le perd SILENCIEUSEMENT.
						NkVec3f tangent = {1.f, 0.f, 0.f};
						NkVec2f uv2 = {0.f, 0.f};
						uint32 color = 0xFFFFFFFFu;
						NkEmId hedge = NK_EM_INVALID; // une demi-arête SORTANTE
						uint8 sel = 0;
						// ── ORDRE DE SÉLECTION (rang de clic) ─────────────────────
						// 0 = non sélectionné. Sinon : rang CROISSANT de sélection,
						// donné par un compteur monotone. C'est ce qui permet à Merge
						// At First / At Last de désigner le PREMIER et le DERNIER
						// SÉLECTIONNÉ, comme Blender, et non le plus petit et le plus
						// grand INDICE — deux choses qui n'ont aucune raison de
						// coïncider : l'indice reflète l'ordre de construction du
						// maillage, pas les gestes de l'utilisateur.
						// Le rang n'est PAS un identifiant : il ne survit pas à une
						// re-topologie (la sélection est réappliquée à plat), ce qui
						// est correct — après un merge, « le premier cliqué » n'existe
						// plus.
						uint32 selOrder = 0;
						// ── CYCLE DISQUE (etape 2) ────────────────────────────────
						// UNE arete incidente : la TETE du cycle disque de ce sommet.
						// ⚠ RENSEIGNEE SUR LE SEUL SOMMET REPRESENTANT. Les copies
						// coincidentes restent a NK_EM_INVALID et sont resolues a la
						// lecture par `canonOf` (cf. VertEdges / EdgeBetween). Une seule
						// source de verite : la divergence devient IRREPRESENTABLE au lieu
						// d'etre seulement improbable.
						// ⚠ CE N'EST PLUS UNE TRANCHE, C'EST UNE TETE DE CYCLE.
						// Avant : (diskStart, diskCount) dans un reservoir CONTIGU.
						// Une tranche contigue n'admet pas d'insertion au milieu : ajouter
						// UNE arete a un sommet obligeait a RELOGER toute sa tranche en fin
						// de reservoir, et l'ancienne devenait de l'espace mort.
						// Maintenant : une arete incidente, tete d'une liste CIRCULAIRE
						// DOUBLEMENT CHAINEE portee par les aretes elles-memes. Brancher
						// une arete ne deplace rien et ne perd rien.
						// MESUREE (banc --proto) : une extrusion en place coute 0,0049 ms
						// par face, PLATE sur x64 la taille du maillage.
						NkEmId diskEdge = NK_EM_INVALID;
				};

				// ── ARETE DE PREMIER PLAN (etape 1 du modele BMesh) ──────────────
				// PROBLEME RESOLU : dans une structure purement demi-arete, une arete
				// n'existe QU'A TRAVERS ses faces. Une arete « seule » (deux sommets
				// relies, sans face) n'a donc aucun moyen d'exister — c'est pourquoi F
				// sur deux sommets ne pouvait RIEN produire, alors que Blender cree un
				// segment. Chez Blender (BMesh) l'arete est une entite a part entiere ;
				// une arete sans face est simplement une arete a zero boucle radiale.
				//
				// CE QUE FAIT CETTE ETAPE : les aretes deviennent une LISTE PROPRE,
				// reconstruite depuis les demi-aretes (RebuildEdges) ET capable de
				// porter des aretes FILAIRES que rien ne deduit d'une face. Les faces
				// continuent d'etre parcourues par les demi-aretes : la bascule complete
				// (cycle radial, boucles BMLoop) viendra ensuite, sans rien changer a
				// l'API publique deja utilisee par l'editeur.
				//
				// IDENTITE : v0/v1 sont des indices de sommets SOUDES (representants de
				// BuildVertexMerge), pas des indices bruts. Sans cela, une primitive dont
				// les faces dupliquent leurs sommets (cube = 24) produirait des aretes en
				// double, chacune vue comme distincte.
				struct Edge {
						NkEmId v0 = NK_EM_INVALID;
						NkEmId v1 = NK_EM_INVALID;
						// Une demi-arete porteuse, ou NK_EM_INVALID pour une arete FILAIRE
						// (aucune face incidente). C'est exactement le cas que l'ancienne
						// structure ne savait pas representer.
						NkEmId hedge = NK_EM_INVALID;
						uint8 faceCount = 0; // 0 = filaire, 1 = bord, 2 = interieur, >2 = non manifold
						uint8 sel = 0;
						uint8 alive = 1;
						// ── CYCLE RADIAL (etape 2) ────────────────────────────────
						// TOUTES les demi-aretes qui portent cette arete, donc toutes les
						// faces incidentes, en liste circulaire chainee depuis `hedge`.
						//
						// POURQUOI c'est necessaire : `Hedge::twin` ne peut designer
						// QU'UNE opposee. Sur une arete partagee par TROIS faces (jonction
						// en T, tres courante des qu'on colle une cloison sur un mur),
						// l'appariement en retient deux et la troisieme devient invisible
						// pour tout parcours. Le cycle radial les porte TOUTES, donc le
						// non-manifold cesse d'etre un cas qu'on ignore pour devenir un cas
						// qu'on peut CONSTATER et traiter.
						//
						// ⚠ CE COMMENTAIRE DISAIT L'INVERSE, ET IL AVAIT RAISON A L'EPOQUE :
						// « tranche contigue plutot que liste chainee (BMesh en utilise
						// une) : la structure est reconstruite en bloc, jamais modifiee
						// arete par arete — une liste chainee n'apporterait que des
						// indirections ». Le raisonnement etait juste ; c'est sa PREMISSE
						// qui a cesse de l'etre, le jour ou l'on a voulu modifier la
						// structure arete par arete. Il est remplace, pas efface : une
						// decision renversee sans trace se reprend a l'identique.
						// TETE du cycle radial : c'est `hedge` ci-dessus. Les demi-aretes
						// suivantes se suivent par Hedge::rNext / rPrev.
						// `radialCount` est CONSERVE : il fait autorite sur le nombre de
						// faces incidentes et toute l'API publique le lit (EdgeIsWire,
						// EdgeIsBoundary, EdgeIsManifold, RadialTwin). Le recalculer en
						// parcourant le cycle a chaque question rendrait O(k) ce qui est
						// O(1), pour ne rien gagner.
						uint32 radialCount = 0;
						// ── CYCLE DISQUE, CHAINE PAR EXTREMITE ─────────────────────
						// Une arete appartient a DEUX cycles disque : celui de v0 et celui
						// de v1. Elle porte donc deux paires de voisins, et l'accesseur
						// choisit la bonne en comparant le sommet demande a v0/v1.
						// C'est la representation de BMesh, et la raison en est celle
						// mesuree : l'insertion devient locale.
						NkEmId dNext0 = NK_EM_INVALID, dPrev0 = NK_EM_INVALID;
						NkEmId dNext1 = NK_EM_INVALID, dPrev1 = NK_EM_INVALID;
				};

				struct Hedge {
						NkEmId origin = NK_EM_INVALID; // sommet d'origine
						NkEmId twin = NK_EM_INVALID;   // demi-arête opposée (autre face)
						NkEmId next = NK_EM_INVALID;   // suivante autour de la face
						NkEmId face = NK_EM_INVALID;   // face incidente
						// Arete de premier plan portee par cette demi-arete (etape 2).
						// Sans ce lien, passer d'une demi-arete a « son » arete demandait
						// une recherche par cle spatiale a chaque fois — c'est-a-dire de
						// RE-DEDUIRE une information que la structure connaissait deja.
						NkEmId edge = NK_EM_INVALID;
						// CYCLE RADIAL : les demi-aretes qui portent la meme arete, en
						// liste circulaire doublement chainee. La tete est Edge::hedge.
						// Double et non simple : retirer une boucle du cycle est O(1), et
						// c'est l'operation dont toute edition en place a besoin.
						NkEmId rNext = NK_EM_INVALID, rPrev = NK_EM_INVALID;
						uint8 alive = 1;			   // 0 = arête interne dissoute (quadify)
				};

				struct Face {
						NkEmId hedge = NK_EM_INVALID; // une demi-arête du bord (boucle via next)
						NkVec3f normal = {0.f, 1.f, 0.f};
						uint8 sel = 0;
						uint8 alive = 1; // 0 = supprimée (compactée plus tard)
						// OMBRAGE PAR FACE façon Blender (« Shade Flat » / « Shade Smooth »).
						// 0 = FLAT : les coins de la face portent la normale DE LA FACE -> arêtes
						//     franches, facettes visibles (comportement historique, défaut).
						// 1 = SMOOTH : les coins portent la normale MOYENNE des faces smooth
						//     incidentes au sommet SOUDÉ (identité topologique de BuildVertexMerge)
						//     -> surface lissée continue. Mixte autorisé (comme Blender).
						uint8 smooth = 0;
						// MATERIAU PAR FACE façon Blender (« material_index » du polygone).
						// Index dans `materialSlots` du maillage ; 0 = premier slot, et c'est
						// le defaut — un maillage qui ignore les materiaux se comporte comme
						// avant, toutes ses faces sur le slot 0.
						//
						// ⚠ LA CONNEXITE NE JOUE AUCUN ROLE. Deux faces qui ne partagent
						// aucune arete peuvent porter le meme index : c'est le sens meme de
						// « lier ou non ». Les sous-mailles sont DEDUITES de cet index
						// (BuildSubMeshRanges), jamais l'inverse — l'ordre des faces
						// n'impose plus le decoupage.
						//
						// ⚠ POURQUOI CE CHAMP NE PEUT PAS SE DEDUIRE, contrairement a
						// `smooth` juste au-dessus. `smooth` EST une propriete des normales :
						// BuildFromIndexed le RE-DEDUIT en comparant les normales des coins.
						// Un materiau, lui, n'est deductible de rien — aucune donnee
						// geometrique ne le porte. Il doit donc etre TRANSPORTE a travers le
						// round-trip ToPolygons/BuildFromPolygons, par lequel passe toute
						// operation d'edition.
						//
						// ⚠ CORRECTION DU 2026-08-22 — CETTE NOTE DISAIT AUSSI QUE `smooth`
						// « traverse les operations topologiques SANS etre transporte ».
						// C'ETAIT FAUX, et la mesure l'a montre : 12 faces lisses en donnaient
						// 0 apres subdivision. La re-derivation n'opere que dans
						// BuildFromIndexed ; BuildFromPolygons, lui, n'a aucune normale de
						// coin a interroger — il n'a que la parente, et il l'ignorait.
						// `smooth` est desormais transporte comme le materiau, par la MEME
						// entree FaceAttrib (arbitrage : une face fille herite de sa mere).
						//   > Un attribut « qui survit » parce qu'un AUTRE chemin le
						//   > reconstruit ne survit qu'aux operations qui passent par ce
						//   > chemin-la. La distinction ne se voit pas tant qu'on ne
						//   > l'exerce pas.
						uint16 material = 0;
						// LE TRAIT, cote FACE. Il voyage par `FaceAttrib`, comme le materiau et
						// l'ombrage, et pour la meme raison : une seule table de parente.
						uint8 trait = 0;
				};

				// ── SLOTS DE MATERIAU DU MAILLAGE ────────────────────────────────────
				// Le maillage possede la liste ; la face n'en porte que l'index. C'est le
				// modele Blender, et c'est lui qui permet a des faces DISJOINTES de
				// partager un slot sans etre rangees ensemble.
				struct MaterialSlot {
						uint32 id = 0;	 // identifiant opaque du materiau (0 = slot vide)
						uint8 alive = 1; // 0 = slot supprime
				};

				// ⚠ VALEURS SERIALISEES : on AJOUTE EN FIN, on ne renumerote JAMAIS —
				// meme regle que NkModifierType, et pour la meme raison : une scene
				// enregistree designe ses slots par ces entiers. Supprimer un slot du
				// milieu pose `alive = 0` et laisse un TROU ; ca ne decale pas les
				// suivants, sinon toutes les faces d'apres changeraient de materiau en
				// silence, sans qu'aucune erreur ne se declenche.
				NkVector<MaterialSlot> materialSlots;

				// ── ATTRIBUTS PAR FACE QUI TRAVERSENT LES OPERATIONS ────────────────
				// UNE SEULE TABLE POUR DEUX ATTRIBUTS, ET C'EST LA RAISON D'ETRE DE CE
				// TYPE. Le materiau a d'abord ete transporte seul, dans un
				// `NkVector<uint16>` parallele aux faces ; `smooth`, lui, retombait a 0 a
				// chaque round-trip (mesure : 12 -> 0 apres subdivision) parce qu'aucune
				// operation ne le portait.
				//
				// ⚠ LA TENTATION ETAIT D'AJOUTER UN SECOND TABLEAU PARALLELE. Elle est
				// refusee : chaque operation devrait alors repondre DEUX FOIS a la meme
				// question — « de quelle face vient cette face ? » — et deux reponses
				// finissent par diverger, sans qu'aucune erreur ne se declenche. Un seul
				// tableau d'attributs impose UNE table de parente, donc un desaccord
				// devient irrepresentable au lieu d'etre seulement improbable.
				//
				// ⚠ ET LE TROISIEME ATTRIBUT N'AURA PAS A CHOISIR SON CAMP : il s'ajoute
				// ici et suit la meme parente sans qu'aucune operation ne soit modifiee.
				// C'etait le motif ecrit de l'arbitrage du 2026-08-22.
				struct FaceAttrib {
						uint16 material = 0;
						uint8 smooth = 0;
						// L'INTENTION DE SELECTION VOYAGE AVEC LE RESTE. Elle est par FACE, comme
						// le materiau et l'ombrage, et elle traverse le meme aller-retour : c'est
						// ce qui permet a chaque operation de repondre « cette face etait-elle
						// choisie ? » sans redemander aux sommets -- la question qui s'effondre
						// des que les sommets choisis couvrent l'objet.
						uint8 sel = 0;
						// LE TRAIT : une zone DESSINEE SUR LA SURFACE, et c'est le quatrieme
						// attribut annonce par l'arbitrage du 2026-08-22 (« il s'ajoute ici et
						// suit la meme parente sans qu'aucune operation ne soit modifiee »).
						//
						// POURQUOI IL N'EST PAS `sel` : `sel` est ce que l'utilisateur DESIGNE
						// maintenant, et chaque clic l'efface. Un trait doit SURVIVRE a la
						// selection suivante -- on le trace, on regarde, on demande « creuse
						// ici » trois gestes plus tard. Deux intentions de duree differente ne
						// peuvent pas partager un champ.
						//
						// [!] ET C'EST CE QUI ATTACHE LE TRAIT A LA SURFACE. Le trait de
						//     sculpture existant vit en `NkVec3f` -- des coordonnees d'espace,
						//     donc il FLOTTE : deformez le maillage et il reste ou il etait.
						//     Ici il n'y a aucune coordonnee : le trait EST un sous-ensemble de
						//     faces, et une face qui bouge emporte le trait avec elle.
						//     Mesure : marque posee, bevel applique (6 faces -> 78), 78/78
						//     portent encore la marque.
						//
						// 0 = pas de trait. Les valeurs suivantes sont libres : un jour elles
						// numeroteront des traits distincts (« lisse ici, creuse la »), et ce
						// jour-la il faudra une table de zones NOMMEES -- un vrai lot, pas un
						// ajout. Tant qu'on trace puis qu'on agit, 0 ou 1 suffit.
						uint8 trait = 0;
				};

				NkVector<Vert> verts;
				NkVector<Hedge> hedges;
				NkVector<Face> faces;
				// ── L'INTENTION DE FACE, ET POURQUOI ELLE NE SE DEDUIT PAS ──────────
				// `FaceIsSelected` repondait « tous ses sommets sont-ils retenus ? ».
				// C'est juste tant que les sommets choisis ne couvrent pas l'objet, et
				// c'est faux des qu'ils le couvrent : deux faces OPPOSEES d'un cube ont
				// pour sommets les 8 coins, donc TOUTE face repondait oui -- extruder deux
				// faces en extrudait six.
				// `faceSelSnap` est la photo de `verts[i].sel` prise quand l'intention a
				// ete posee. Si les sommets ont bouge depuis, l'intention est perimee et
				// l'on retombe sur la deduction -- qui est alors la bonne reponse. Une
				// photo qu'on compare ne peut pas s'oublier ; une invalidation posee a la
				// main sur chaque site d'ecriture, si.
				NkVector<uint8> faceSelSnap;
				bool faceSelPorte = false; // une intention a-t-elle jamais ete posee ?
				bool faceSelOk = false;    // ... et etait-elle a jour au dernier Refresh ?
				// Aretes de premier plan. Reconstruites par RebuildEdges() apres toute
				// operation topologique ; les aretes FILAIRES y survivent (elles ne sont
				// deduites d'aucune face, donc rien d'autre ne peut les recreer).
				NkVector<Edge> edges;
				// Compteur monotone des rangs de selection (cf. Vert::selOrder). Jamais
				// remis a zero en cours d'edition : c'est un ORDRE, pas un compte.
				uint32 selCounter = 0;
				// Reservoirs des deux cycles BMesh (etape 2). Reconstruits en meme temps
				// que `edges` : ce sont des VUES sur la topologie, jamais une source.
				// ⚠ `radialPool` ET `diskPool` ONT DISPARU, ET C'EST LE CHANTIER.
				// C'etaient deux reservoirs CSR : toutes les incidences a plat, une
				// tranche contigue par entite. Rapides a parcourir, impossibles a
				// modifier localement — toute edition devait donc reconstruire la
				// topologie entiere (ToPolygons -> BuildFromPolygons).
				// Les cycles vivent maintenant DANS les entites (Edge::dNext0...,
				// Hedge::rNext...). Le parcours perd de la localite ; la mesure du
				// prototype disait qu'il faudrait SEIZE parcours integraux par edition
				// pour que l'echange soit perdant, et un editeur en fait un a trois.
				// ── IDENTITE SOUDEE, CONSERVEE AU LIEU D'ETRE RECALCULEE ────────
				// `canonOf[v]` : le sommet REPRESENTANT de la position de `v`. C'est le
				// resultat de BuildVertexMerge, que RebuildEdges calcule de toute facon.
				//
				// POURQUOI LE GARDER. `EdgeBetween` le recalculait A CHAQUE APPEL —
				// une table de hachage sur TOUS les sommets pour repondre a une
				// question que le cycle disque rend locale. MESURE (banc --perf) :
				// 20 appels sur 66 049 sommets coutaient 41 ms, et ce cout suivait la
				// taille du maillage. C'est-a-dire que la fonction qui EXISTE pour
				// montrer l'interet du cycle disque le detruisait en entrant.
				//
				// ⚠ CE N'EST PAS UN CACHE SUPPLEMENTAIRE A INVALIDER. Il a exactement
				// la meme duree de vie que `edges`, `radialPool` et `diskPool` : les
				// quatre sont poses par RebuildEdges et vides par Clear(). Recalculer
				// `canonOf` frais tout en lisant un `diskPool` perime rendait la fonction
				// plus FRAICHE que la donnee qu'elle consultait — une incoherence, pas
				// une precaution.
				NkVector<uint32> canonOf;

				// ── LE MASQUE DE SCULPTURE : UN POIDS PAR SOMMET ────────────────────
				// Blender : un masque protege une zone contre TOUTES les brosses, et
				// c'est lui qui rend l'outil Transform de sculpture utilisable (on
				// masque, puis on deplace la partie NON masquee).
				//
				// ⚠️ VIDE = AUCUN MASQUE, ET C'EST LE COUT ZERO. Tant que personne n'a
				//    peint, ce vecteur reste VIDE : un maillage de 249 906 sommets ne
				//    paie pas un octet pour une fonction qu'il n'utilise pas. Des qu'un
				//    poids est pose, il vaut 4 octets par sommet -- chiffre MESURE par
				//    le banc du masque, pas estime.
				//
				// ⚠️ PAR SOMMET ET NON PAR FACE, contrairement au TRAIT. Un masque doit
				//    etre DEGRADE (bord doux) pour que la deformation ne montre pas
				//    l'escalier des faces ; le trait, lui, est une zone qu'on designe, et
				//    une face y est ou n'y est pas. Deux natures, deux domiciles.
				//
				// ⚠️ CE QU'IL NE FAIT PAS ENCORE : traverser une operation TOPOLOGIQUE.
				//    `BuildFromPolygons` reconstruit `verts` et le masque n'est pas
				//    transporte -- c'est MESURE et ecrit (critere « subdivision » du
				//    banc), pas suppose. La sculpture, elle, ne change aucune topologie :
				//    c'est le cas qui compte aujourd'hui.
				//
				// 0 = libre (la brosse agit a plein), 1 = protege (elle n'agit pas du
				// tout), entre les deux = attenuation lineaire.
				NkVector<float32> vertMask;

				/// Le poids du sommet `v`. Rend 0 sans masque : la question a une
				/// reponse meme sans tableau, et cette reponse est « libre ».
				float32 MaskAt(uint32 v) const {
					return (v < (uint32)vertMask.Size()) ? vertMask[v] : 0.f;
				}
				/// Le tableau existe-t-il ? (≠ « un sommet est-il masque »)
				bool MaskExists() const {
					return !vertMask.Empty();
				}
				/// Alloue a la taille du maillage, a 0, et la SUIT : un maillage qui
				/// gagne des sommets voit les nouveaux naitre LIBRES.
				void MaskEnsure() {
					const uint32 vc = VertCount();
					const uint32 n = (uint32)vertMask.Size();
					if (n == vc)
						return;
					vertMask.Resize(vc);
					for (uint32 i = n; i < vc; ++i)
						vertMask[i] = 0.f;
				}
				/// Pose un poids, borne a [0..1]. Alloue a la demande.
				void MaskSet(uint32 v, float32 w) {
					if (v >= VertCount())
						return;
					MaskEnsure();
					if (w < 0.f)
						w = 0.f;
					if (w > 1.f)
						w = 1.f;
					vertMask[v] = w;
				}
				/// Combien de sommets sont masques AU MOINS a `seuil`.
				/// ⚠️ Le seuil est un parametre, pas une constante cachee : « masque »
				///    n'a pas le meme sens pour un affichage (tout ce qui se voit) et
				///    pour un critere de banc (ce qui protege vraiment).
				uint32 MaskedCount(float32 seuil = 0.001f) const {
					uint32 n = 0;
					for (uint32 i = 0; i < (uint32)vertMask.Size(); ++i)
						if (vertMask[i] >= seuil)
							++n;
					return n;
				}
				/// Somme des poids -- le critere CONTINU du banc : deux masques
				/// differents peuvent avoir le meme COMPTE, jamais la meme somme.
				float32 MaskSum() const {
					float32 s = 0.f;
					for (uint32 i = 0; i < (uint32)vertMask.Size(); ++i)
						s += vertMask[i];
					return s;
				}
				/// Tout demasquer ET LIBERER : « plus de masque » et « un masque
				/// partout a zero » doivent etre le MEME etat, sinon le cout memoire
				/// survivrait a la fonction.
				void MaskClearAll() {
					vertMask.Clear();
				}
				/// Tout masquer (poids `w`, 1 par defaut).
				void MaskFillAll(float32 w = 1.f) {
					MaskEnsure();
					if (w < 0.f)
						w = 0.f;
					if (w > 1.f)
						w = 1.f;
					for (uint32 i = 0; i < (uint32)vertMask.Size(); ++i)
						vertMask[i] = w;
				}
				/// Inverser : ce qui etait protege devient libre, et l'inverse.
				/// ⚠️ SUR UN MAILLAGE SANS MASQUE, inverser MASQUE TOUT -- c'est le
				///    comportement de Blender, et la seule lecture coherente de
				///    « l'inverse de rien ».
				void MaskInvert() {
					MaskEnsure();
					for (uint32 i = 0; i < (uint32)vertMask.Size(); ++i)
						vertMask[i] = 1.f - vertMask[i];
				}

				void Clear() {
					verts.Clear();
					hedges.Clear();
					faces.Clear();
					edges.Clear();
					// LES QUATRE VUES PARTENT ENSEMBLE. Elles etaient trois a rester
					// derriere : `edges` seule etait videe, et `radialPool`/`diskPool`
					// gardaient les tranches du maillage PRECEDENT. Personne ne les
					// lisait (les accesseurs passent par `edges`, vide, ou par
					// `diskCount`, remis a 0 par le Resize suivant), mais « personne ne
					// les lit » est un fait sur le code d'aujourd'hui, pas un invariant.
					// Avec `canonOf` la question cesse d'etre theorique : un tableau de
					// la BONNE TAILLE mais du MAILLAGE D'AVANT ne se distingue pas d'un
					// tableau valide.
					canonOf.Clear();
					// ⚠️ LE MASQUE PART AVEC LA TOPOLOGIE, ET C'EST UN CHOIX ECRIT.
					// `vertMask` est indexe par le NUMERO de sommet ; une
					// reconstruction renumerote. Le garder rendrait des poids justes
					// attribues aux mauvais sommets -- « un indice n'est pas un nom »,
					// et un masque faux est pire qu'un masque absent parce qu'il
					// protege ce qu'on voulait deformer sans le dire. Le transport a
					// travers une operation topologique est un lot a part ; tant
					// qu'il n'existe pas, l'oubli est MESURE et ANNONCE.
					vertMask.Clear();
					// ⚠ `materialSlots` N'EST PAS VIDE ICI, ET C'EST VOULU.
					// BuildFromPolygons appelle Clear() a chaque operation d'edition :
					// vider les slots ferait perdre la liste des materiaux du maillage a
					// la premiere extrusion, alors que seule la TOPOLOGIE est refaite.
					// Les slots appartiennent au maillage, pas a sa topologie courante.
				}

				uint32 VertCount() const {
					return (uint32)verts.Size();
				}

				uint32 FaceCount() const {
					return (uint32)faces.Size();
				}

				// Construit depuis un maillage indexé (triangles). Si quadify=true, fusionne
				// les paires de triangles coplanaires partageant une arête en QUADS (n-gons).
				// `triMaterial`, quand il est fourni, doit avoir ic/3 entrees et donne
				// l'index de materiau de chaque TRIANGLE. Absent (nullptr), toutes les
				// faces retombent sur le slot 0 -- comportement historique, donc aucun
				// appelant existant ne change.
				// `outMaterialChanged`, quand il est fourni, recoit le nombre de faces
				// SOURCE dont le materiau n'a pas ete retenu par une fusion (quadify).
				// Il vaut 0 quand quadify est faux : sans fusion, rien ne se perd.
				void BuildFromIndexed(const NkVertex3D *v, uint32 vc, const uint32 *idx, uint32 ic, bool quadify,
									  const uint16 *triMaterial = nullptr, uint32 *outMaterialChanged = nullptr);

				// Triangule toutes les faces (éventail) -> mesh de rendu. outTriFace[i] = id
				// de la face n-gon d'origine du i-ème triangle (pour le pick).
				// ⚠ CONTRAT 1:1 : outV[i] correspond EXACTEMENT à verts[i] (même nombre, même
				// ordre). L'éditeur s'appuie dessus (sélection, cage, pick, marqueurs).
				void Triangulate(NkVector<NkVertex3D> &outV, NkVector<uint32> &outIdx,
								 NkVector<NkEmId> &outTriFace) const;

				// Variante d'AFFICHAGE tenant compte de l'ombrage par face (Face::smooth).
				// Problème résolu : la structure ne porte qu'UNE normale par SOMMET, alors
				// qu'un ombrage FLAT en exige une par COIN. Quand des faces PARTAGENT un
				// sommet (sphère, grille…), l'ombrage plat est donc impossible à représenter
				// en 1:1 — on DÉDOUBLE ici les coins des faces FLAT qui se disputent un même
				// sommet (exactement ce que fait un moteur avec des « loops » Blender).
				//   • faces SMOOTH -> réutilisent le sommet 1:1 (normale moyenne soudée) ;
				//   • faces FLAT   -> la 1re écrit la normale de face dans le slot 1:1, les
				//     suivantes obtiennent une COPIE ajoutée en fin de tableau.
				// Conséquence : si aucun sommet n'est disputé (cas des primitives, qui
				// dupliquent déjà leurs coins par face) la sortie est STRICTEMENT identique à
				// Triangulate(). Sinon outV est plus grand que verts — ce maillage est un
				// PUR CACHE D'AFFICHAGE, jamais une source de sélection/topologie.
				void TriangulateShaded(NkVector<NkVertex3D> &outV, NkVector<uint32> &outIdx,
									   NkVector<NkEmId> &outTriFace) const;

				// ── Représentation POLYGONES (n-gons) — CSR ─────────────────────────
				// Extrait les faces vivantes : sommets + boucles (face i = outFaceVerts
				// [outFaceStart[i] .. outFaceStart[i+1]]). outFaceStart a faceCount+1 entrées.
				// `outFaceAttrib`, quand il est fourni, recoit les attributs par face
				// (materiau ET ombrage) de chaque face vivante, dans le MEME ordre que
				// outFaceStart. C'est le seul moyen de les faire survivre a une operation
				// d'edition : toutes passent par ce round-trip, et il perdait jusqu'ici
				// tout ce que portait `Face`.
				void ToPolygons(NkVector<NkVertex3D> &outVerts, NkVector<uint32> &outFaceStart,
								NkVector<uint32> &outFaceVerts,
								NkVector<FaceAttrib> *outFaceAttrib = nullptr) const;
				// (Re)construit le half-edge depuis des n-gons (même format CSR).
				// `faceAttrib`, quand il est fourni, doit avoir `faceCount` entrees et
				// donne le materiau ET l'ombrage de chaque face reconstruite. Absent
				// (nullptr), toutes les faces retombent sur le slot 0 et sur FLAT — c'est
				// le comportement historique, et c'est pour ca que le parametre est
				// optionnel : aucun appelant existant ne change de comportement.
				void BuildFromPolygons(const NkVertex3D *v, uint32 vc, const uint32 *faceStart, uint32 faceCount,
									   const uint32 *faceVerts, const FaceAttrib *faceAttrib = nullptr);

				// ── SOUS-MAILLES DEDUITES DU MATERIAU PAR FACE ──────────────────────
				// Une plage de triangles consecutifs partageant le meme index de materiau.
				// ⚠ UN SLOT PEUT AVOIR PLUSIEURS PLAGES, et c'est le but : deux faces
				// non voisines qui portent le meme index produisent deux plages
				// distinctes sans etre rangees ensemble. C'est ce que le decoupage par
				// sous-mesh « range a la main » ne savait pas faire.
				struct SubMeshRange {
						uint16 material = 0;
						uint32 firstIndex = 0; // offset dans le tampon d'indices triangule
						uint32 indexCount = 0;
				};

				// Triangule (via TriangulateShaded, donc en respectant l'ombrage) puis
				// decoupe le tampon d'indices en plages par index de materiau. Les faces
				// sont parcourues dans leur ordre courant : une nouvelle plage s'ouvre a
				// chaque changement d'index. Rend aussi le nombre de slots DISTINCTS
				// rencontres, qui est en general plus petit que le nombre de plages.
				void BuildSubMeshRanges(NkVector<NkVertex3D> &outV, NkVector<uint32> &outIdx,
										NkVector<SubMeshRange> &outRanges,
										uint32 *outDistinctSlots = nullptr) const;

				// Affecte `slot` aux faces dont TOUS les coins sont selectionnes.
				// ⚠ REGLE DE BLENDER, adoptee telle quelle. Rodolf a demande « un groupe
				// de vertex ou de face ». Chez Blender un SOMMET NE PORTE PAS de
				// materiau : selectionner des sommets puis « Assign » affecte aux faces
				// entierement couvertes par la selection. On adopte cette regle plutot
				// que d'inventer un second mecanisme par sommet qu'il faudrait ensuite
				// reconcilier avec celui-ci.
				// ⚠ A NE PAS CONFONDRE avec les GROUPES DE SOMMETS (cf. NkModifierType,
				// « Mask par groupe / Vertex Weight »), qui sont un autre systeme et qui
				// n'existent pas encore.
				// Rend le nombre de faces affectees.
				uint32 AssignMaterialToSelectedFaces(uint16 slot);

				// ── SOUDURE (weld) DES SOMMETS COÏNCIDENTS ──────────────────────────
				// Les primitives et les imports DUPLIQUENT les sommets PAR FACE (cube = 24
				// sommets) pour porter des normales/UV distinctes. Conséquence : la topologie
				// n'est PAS manifold — aucune demi-arête n'a de `twin` vers la face voisine —
				// donc tout parcours qui TRAVERSE les faces échoue (anneau du loop cut,
				// boucles d'arêtes, futur knife) et la cage compte chaque arête en double.
				//
				// canon[i] = index du REPRÉSENTANT du groupe de sommets coïncidents de i (le
				// plus petit indice du groupe). Construit par GRILLE DE HACHAGE spatiale ->
				// O(n), jamais O(n²). C'est une IDENTITÉ TOPOLOGIQUE : deux coins coïncidents
				// sont « le même sommet » pour l'ADJACENCE, tandis que leurs ATTRIBUTS
				// (normale, UV) restent SÉPARÉS. Le rendu est donc strictement inchangé (pas
				// de lissage parasite, UV intacts) — c'est exactement le modèle de Blender :
				// maillage soudé + attributs portés par les coins (loops).
				//
				// 🔗 COORDINATION (2026-08-22) — LA COQUE INVERSEE DU CONTOUR TOON PASSE ICI.
				// Un contour par coque inversee a besoin d'une normale MOYENNEE PAR POSITION,
				// pas par indice de sommet : sur un cube aux coins dedoubles, extruder le long
				// de la normale d'indice ouvre les coins et le contour se dechire. `canon[]` est
				// exactement l'identite qu'il faut -- moyenner les normales des sommets qui
				// partagent un representant donne la normale continue attendue.
				// ⚠️ Si tu es l'agent rendu et que tu allais ecrire ta propre soudure spatiale :
				// celle-ci existe, elle est en O(n) par grille de hachage, et elle est deja
				// exercee par les familles `subsurf`, `linked` et `bmesh2` de NKEditMeshHarness.
				// Un second soudeur divergerait du premier au premier changement d'epsilon.
				void BuildVertexMerge(NkVector<uint32> &canon, float32 eps = 1e-4f) const;
				// Étend la sélection à TOUS les sommets coïncidents d'un sommet sélectionné :
				// sans ça, cliquer un coin ne sélectionne qu'une des N copies et les faces
				// voisines ne suivent pas (arête « à moitié » sélectionnée).
				void PropagateSelectionToCoincident();

				// Arêtes uniques (paires de sommets) pour la cage d'affichage.
				// ⚠ CAGE D'ÉDITION = CES arêtes-là (topologie n-gon), JAMAIS les arêtes des
				// triangles de rendu : sur un quad, la DIAGONALE de triangulation ne doit pas
				// apparaître (un quad = 4 arêtes, un n-gon = N arêtes), exactement comme Blender.
				void GetUniqueEdges(NkVector<uint32> &outPairs) const;

				// Sommets (dans l'ordre du bord) d'une face n-gon.
				void GetFaceVerts(NkEmId f, NkVector<NkEmId> &out) const;
				uint32 FaceSize(NkEmId f) const;
				// AIRE d'une face vivante (formule de Newell : somme des produits
				// vectoriels le long du cycle, / 2). Exacte pour une face plane, et une
				// approximation raisonnable pour une face gauche -- ce qui suffit a son
				// unique usage : DEPARTAGER deux contributeurs d'une fusion.
				// Rend 0 pour une face morte ou de moins de 3 coins.
				float32 FaceArea(NkEmId f) const; // nombre de sommets du bord
				// Une face est SELECTIONNEE si l'INTENTION le dit -- et, faute d'intention
				// a jour, si tous ses sommets le sont (deduction historique).
				// ⚠ Lit `faceSelOk`, pose par `RefreshFaceSel`. La validite se calcule UNE
				// fois par operation et non a chaque face : la comparaison est en O(sommets)
				// et ce test-ci vit dans des boucles sur les faces.
				bool FaceIsSelected(NkEmId f) const;
				// Pose l'intention de face (un octet par face) et photographie la selection
				// de sommets qui l'accompagne. C'est l'editeur qui appelle, au clic.
				void SetFaceSelection(const uint8 *flags, uint32 count);
				// L'intention est-elle encore a jour ? A appeler en TETE de toute operation
				// qui consomme une selection de faces.
				void RefreshFaceSel();
				bool FaceSelAJour() const { return faceSelOk; }
				// Les (au plus 2) faces incidentes à l'arête (a,b) — pour la normale d'arête.
				// Renvoie le nombre de faces trouvées (0..2).
				uint32 EdgeFaces(uint32 a, uint32 b, NkEmId &f0, NkEmId &f1) const;

				// ── BOUCLES / ANNEAUX (Alt+clic façon Blender) ───────────────────────
				// Ces parcours EXIGENT des twins corrects entre faces voisines : ils ne sont
				// possibles que grâce à la soudure topologique (cf. BuildVertexMerge).
				//
				// EDGE LOOP : depuis l'arête (a,b), suit la boucle qui CONTINUE TOUT DROIT à
				// travers les sommets (l'arête alignée dans la face voisine), dans les DEUX
				// sens, jusqu'à reboucler ou atteindre un bord. Sort des paires de sommets
				// (même format que GetUniqueEdges). Ne progresse qu'à travers des QUADS —
				// s'arrête proprement sur un pôle, un n-gon ou un bord.
				void GetEdgeLoop(uint32 a, uint32 b, NkVector<uint32> &outPairs) const;
				// FACE LOOP : anneau des faces TRAVERSÉES par l'arête (a,b) — de proche en
				// proche via l'arête opposée du quad (même parcours que le loop cut).
				void GetFaceLoop(uint32 a, uint32 b, NkVector<NkEmId> &outFaces) const;

				// Fusionne les paires de triangles CONSÉCUTIFS (2k,2k+1) adjacents et
				// coplanaires en QUADS. Adapté aux meshes triangulés quad-par-quad
				// (primitives, grilles). coplanarDot ~0.9995 (cube) à 0.98 (sphère fine).
				//
				// ⚠ C'EST UNE FUSION DE FACES, donc un endroit ou le MATERIAU PAR FACE
				// se perd. Regle arbitree (2026-08-22) : la face survivante prend le
				// materiau du CONTRIBUTEUR DOMINANT PAR L'AIRE ; a aire egale, l'INDICE
				// DE MATERIAU LE PLUS BAS. Deterministe (deux courses sur la meme entree
				// donnent le meme resultat, sur toute machine), visible (la couleur qui
				// couvrait le plus reste) et mesurable (une aire se verifie ; « le plus
				// pertinent » ne se verifie pas).
				//
				// ⚠ ET LE CAS QUI COMPTE LE PLUS EST CELUI OU LA REGLE NE SUFFIT PAS :
				// quand les deux triangles portent des materiaux DIFFERENTS, la fusion
				// perd de l'information QUELLE QUE SOIT la regle. Ce n'est pas a
				// corriger, c'est a RAPPORTER -- d'ou la valeur rendue : le nombre de
				// faces source dont le materiau n'a PAS ete retenu. Un zero rassure,
				// mais un zero ne s'invente pas : il se compte.
				//
				// ⚠ L'EGALITE D'AIRE EST TESTEE A UNE TOLERANCE RELATIVE (1e-6), pas au
				// bit pres : les deux moities d'un quad carre ont la meme aire
				// mathematiquement, mais l'ordre des sommations en float32 ne le garantit
				// pas. Sans tolerance, le departage « indice le plus bas » deviendrait
				// un tirage au sort dependant de l'arrondi -- c'est-a-dire non
				// deterministe pour l'utilisateur, qui voit deux triangles identiques.
				//
				// Rend le nombre de faces ayant PERDU leur materiau dans une fusion.
				uint32 Quadify(float32 coplanarDot = 0.985f);

				// Normales par face (produit vectoriel) puis par sommet, EN RESPECTANT
				// l'ombrage par face (Face::smooth) :
				//   • FLAT   : le sommet n'accumule que les faces FLAT qui le référencent par
				//     CE MÊME INDICE -> comme les primitives dupliquent leurs coins par face,
				//     chaque coin garde la normale de sa face (facettes franches).
				//   • SMOOTH : le sommet accumule les faces SMOOTH incidentes à son sommet
				//     SOUDÉ (canon, cf. BuildVertexMerge) -> les copies coïncidentes d'un même
				//     coin reçoivent la MÊME normale moyenne : surface lissée continue.
				// Pondération : par l'AIRE (le produit vectoriel non normalisé porte 2*aire du
				// triangle du coin) — choix classique, stable, insensible à la tessellation
				// fine ; l'alternative « par l'angle au sommet » n'apporte rien sur des
				// maillages quad/n-gon réguliers et coûte un acos par coin.
				void RecomputeNormals();

				// TO SPHERE (Shift+Alt+S) : spherise la selection autour de `center`.
				bool ToSphereSelected(const NkToSphereParams &p);
				// SHRINK / FATTEN : deplace la selection le long des normales SOUDEES.
				bool ShrinkFattenSelected(const NkShrinkFattenParams &p);

				// ── OMBRAGE FLAT / SMOOTH (façon Blender : Object > Shade Flat/Smooth, ou
				//    Mesh > Shading en Edit Mode sur les faces sélectionnées) ───────────────
				// Pose Face::smooth puis recalcule les normales. selectedOnly=true limite aux
				// faces SÉLECTIONNÉES (toutes leurs extrémités marquées Vert::sel) ; s'il n'y
				// en a aucune, l'appel retombe sur TOUTES les faces (comportement « objet »).
				// Renvoie true si au moins une face a changé d'état.
				bool SetShadeSmooth(bool smooth, bool selectedOnly = false);
				bool AnyFaceSmooth() const; // au moins une face lissée
				bool AllFacesSmooth() const; // toutes les faces vivantes lissées

				// ── COUCHE DE COMMANDES D'ÉDITION (paramétrée, découplée de l'UI) ────
				// Ces opérations agissent sur la SÉLECTION interne (Vert::sel, ou par
				// face = tous ses sommets sélectionnés) et mutent la topologie n-gon.
				// Elles NE touchent PAS au GPU : l'appelant régénère le rendu (Triangulate)
				// après coup. C'est la base pour l'undo/redo, les modificateurs (stack
				// non-destructif) et l'espace d'actions IA (NKAI). Chaque op renvoie true
				// si la topologie/géométrie a changé. Paramètres : Nk*Params (namespace).

				// Sélection interne (Vert::sel).
				void SelectAll();

				// -- LE TRAIT : UNE ZONE DESSINEE SUR LA SURFACE ---------------------
				//
				// Rodolf, 19/09 : « en mode edition on trace un trait, on dit au modele de
				// couper et reconstruire a partir du trace, ou creuse ici » -- et, le
				// lendemain, l'essentiel : « c'est mieux SUR LA SURFACE, comme ca ca
				// epouse la courbe une fois ».
				//
				// [!] LE TRAIT N'A DONC PAS DE COORDONNEES, ET C'EST TOUT LE POINT. Le
				//     trait de sculpture existant vit en `NkVec3f` : il FLOTTE, et une
				//     deformation le laisse ou il etait. Ici le trait EST un sous-ensemble
				//     de faces -- une face qui bouge emporte le trait avec elle, et une
				//     face qui se subdivise le transmet a ses filles par la meme parente
				//     que le materiau. Il n'y a qu'UNE verite sur ou il est.
				//
				// TraceTrait marque les faces dont le CENTRE tombe a moins de `rayon` du
				// point donne, et rend leur nombre. Additif : plusieurs appels dessinent
				// un trait continu, exactement comme les tampons d'une brosse.
				uint32 TraceTrait(const NkVec3f &point, float32 rayon, uint8 numero = 1);
				// Efface tout (numero = 0) ou un trait donne. Rend le nombre efface.
				uint32 EffaceTrait(uint8 numero = 0);
				// Combien de faces vivantes portent ce trait.
				uint32 CompteTrait(uint8 numero = 1) const;
				// COMBIEN DE FACES DU TRAIT ONT TOUTES LEURS VOISINES TRACEES.
				//
				// [!] CE N'EST PAS LA CONDITION DE SURVIE DU TRAIT, ET JE L'AI CRU.
				//     J'avais ecrit ici que c'en etait une : une operation ne donnant le
				//     trait aux faces neuves qu'a l'unanimite, un trait sans interieur
				//     n'aurait rien a transmettre. La mesure a refute la loi AVANT
				//     qu'elle ne serve -- balayage sur une sphere 20x20, bevel 0,02 :
				//
				//         20 tracees,  0 interieure  ->   0 apres bevel
				//         40 tracees,  0 interieure  ->  80 apres bevel
				//         60 tracees,  0 interieure  -> 320 apres bevel
				//         80 tracees, 20 interieures -> 560 apres bevel
				//
				//     A 40 et 60 faces il n'y a AUCUNE face interieure et le trait
				//     survit ; a 20 non plus, et il meurt. L'interieur ne separe donc
				//     pas les deux cas : des faces filles heritent par une PARENTE que
				//     ce compte ignore.
				//
				//     Ce compte reste une mesure utile -- il dit la compacite du trait --
				//     mais il ne doit fonder AUCUN refus tant que la vraie condition
				//     n'est pas trouvee. *Une loi refutee qu'on laisse ecrite comme vraie
				//     devient la consigne du lecteur suivant.*
				uint32 CompteTraitInterieur(uint8 numero = 1) const;
				//
				// [!] LA DESIGNATION PASSE PAR `sel`, ET AUCUN VERBE N'EST A ECRIRE.
				//     Sept verbes du contrat operent deja « sur la selection » (subdivide,
				//     extrude, inset, bevel, dissolve, delete, loopcut). « Creuse ici »
				//     n'est donc pas un verbe de plus : c'est ce transfert, puis un verbe
				//     qui existe. On ne touche ni au contrat ni a la table.
				//
				//     Le trait n'est PAS `sel` lui-meme parce que leurs durees different :
				//     `sel` est ce qu'on designe maintenant et chaque clic l'efface ; un
				//     trait doit survivre aux trois gestes qui separent le trace de la
				//     demande.
				uint32 SelectionnerTrait(uint8 numero = 1);
				void SelectNone();
				bool AnyVertSelected() const;

				// ── COMPOSANTES CONNEXES (« loose parts ») ────────────────────────
				// UN SOUS-MESH EST UNE COMPOSANTE CONNEXE. C'est la définition posée
				// par Rihen à travers un geste : `L` sous Blender sélectionne tout ce
				// qui est RELIÉ à l'élément survolé. Sa phrase le disait déjà — « un
				// assemblage de vertices, edges et faces reliés OU NON entre eux ».
				//
				// ⚠️ UNE SEULE PRIMITIVE POUR TOUS LES GESTES QUI EN DÉPENDENT :
				// `L` (sélectionner un îlot), `Ctrl+L` (étendre à ce qui est lié), et
				// plus tard `P` « separate by loose parts ». Deux implémentations
				// divergeraient au premier cas limite (sommet isolé, arête pendante,
				// face dégénérée) et « ce que L sélectionne » cesserait d'être « ce
				// que P sépare ».
				//
				// ⚠️ Le parcours se fait sur l'IDENTITÉ SOUDÉE (BuildVertexMerge), pas
				// sur les indices bruts : un cube importé duplique ses sommets par
				// face (24 pour 8 positions), et une connexité par indice brut y
				// verrait 6 îlots — un par face — au lieu d'un seul.
				//
				// compOf[i] = index de composante du sommet i (les copies coïncidentes
				// partagent le leur). Renvoie le NOMBRE de composantes.
				//
				// INVARIANTS VÉRIFIABLES, et ils font le test : la somme des tailles
				// des composantes égale le nombre de sommets, et aucun sommet
				// n'appartient à deux composantes.
				uint32 ComputeConnectedComponents(NkVector<int32> &compOf) const;

				// `L` — sélectionne tout ce qui est lié au sommet `seed`. `additive`
				// conserve la sélection courante (Blender ajoute ; L seul n'efface
				// pas).
				//
				// ⚠️ RENVOIE `true` SI LA SÉLECTION A CHANGÉ, pas si l'appel était
				// valide — c'est la convention des autres opérations de ce fichier
				// (« chaque op renvoie true si la topologie/géométrie a changé »).
				// Une graine hors limites, un îlot déjà entièrement sélectionné :
				// les deux rendent `false`, et c'est voulu. Un booléen qui répond
				// « oui » quand rien n'a bougé ne sert à aucun appelant — ni pour
				// empiler un historique, ni pour redessiner.
				bool SelectLinked(uint32 seed, bool additive = true);

				// `Ctrl+L` — étend la sélection COURANTE à tout ce qui lui est lié.
				// Même convention : `true` seulement si des sommets se sont ajoutés.
				// Sur une sélection VIDE, ne fait rien et rend `false` — « tout
				// sélectionner » serait une surprise, et un geste qui surprend est un
				// geste qu'on annule.
				bool SelectLinkedFromSelection();

				// EXTRUDE façon Blender : la nouvelle géométrie est créée À L'OFFSET DEMANDÉ
				// (0 par défaut = collée sur l'originale) et devient la SÉLECTION. Aucun
				// déplacement implicite : c'est l'utilisateur qui bouge ensuite.
				bool ExtrudeSelectedFaces(const NkExtrudeParams &p = NkExtrudeParams{});
				// ── EXTRUSION EN PLACE (branche REGION) ─────────────────────────────
				// ⚠ PUBLIQUE, ET PAS ENCORE BRANCHEE. Elle produit exactement le meme
				// maillage que `ExtrudeSelectedFaces` — la famille `enplace/` du
				// harnais compare les deux chemins cas par cas — mais elle reste plus
				// LENTE sur une edition locale tant que les quatre passes de remise en
				// etat heritees ne sont pas localisees.
				// Publique parce qu'un chemin que rien n'exerce pourrit : c'est le
				// harnais qui le tient en vie jusqu'a ce qu'il gagne.
				// `outTwinsLocaux` : rend 1 si le re-appariement LOCAL des jumelles a
				// pu s'appliquer, 0 s'il a fallu retomber sur `LinkTwins` global
				// (ambiguite positionnelle). A lire par le harnais : un repli
				// systematique laisserait le resultat juste et le chemin mort.
				bool ExtrudeSelectedFacesInPlace(const NkExtrudeParams &p, uint32 *outTwinsLocaux = nullptr);
				// Sommet sélectionné -> nouveau sommet + ARÊTE reliante (arête « fil », face
				// dégénérée à 2 sommets : pas de surface, mais une vraie arête éditable).
				bool ExtrudeSelectedVertices(const NkExtrudeParams &p = NkExtrudeParams{});
				// Arête sélectionnée -> nouvelle arête + FACE (quad) reliante.
				//
				// ⚠ LE QUAD CREE N'A PAS DE FACE MERE. Regle arbitree (2026-08-23) : une
				// face creee herite de la face a laquelle elle est GEOMETRIQUEMENT
				// ADJACENTE ; en cas d'ambiguite, celle qui apporte le plus de LONGUEUR
				// DE CONTOUR PARTAGE, egalite tranchee par l'INDICE LE PLUS BAS. C'est la
				// meme forme qu'aux sites de fusion (dominance par une mesure, egalite par
				// l'indice) : une seule regle a comprendre pour les deux familles.
				//
				// ⚠ ET ICI LE CRITERE DE LONGUEUR EST INERTE PAR CONSTRUCTION, ce qu'il
				// faut savoir avant de le croire discriminant : le quad partage avec ses
				// deux voisines EXACTEMENT le meme segment (a,b). Les deux longueurs sont
				// donc egales par definition, et c'est toujours l'indice le plus bas qui
				// tranche. Sur un maillage NON soude (un cube importe compte 24 sommets
				// pour 8 positions) l'arete n'a meme qu'UNE face incidente : aucune
				// ambiguite. Le critere est ecrit quand meme, parce que c'est la MEME
				// fonction qui sert au chanfrein, ou il discrimine vraiment.
				//
				// `outMaterialChanged` recoit le nombre de faces voisines dont le materiau
				// n'a pas ete retenu — la perte, comptee et non supposee.
				bool ExtrudeSelectedEdges(const NkExtrudeParams &p = NkExtrudeParams{},
										  uint32 *outMaterialChanged = nullptr);
				// ── X, ET CE QU'IL SUPPRIME SELON LE SOUS-MODE (Blender) ────────────
				// Blender ne supprime pas la meme chose en mode SOMMET, ARETE et FACE, et
				// la difference n'est pas cosmetique : en mode sommet, un coin choisi
				// emporte TOUT ce qui s'appuie sur lui. Chez nous X passait TOUJOURS par
				// la regle des faces (« toutes ses aretes retenues »), donc deux sommets
				// ou deux aretes ne supprimaient RIEN -- mesure du 17/09.
				//
				// ⚠ LE PREDICAT S'EXPRIME SUR LA FACE, PAS SUR LA CAGE. « Une de ses
				//   aretes est retenue » se lit « deux de ses sommets CONSECUTIFS le
				//   sont », ce qui ne depend ni de `edges` ni de la soudure. La cage est
				//   soudee par POSITION (un cube rend 12 aretes pour 24 sommets) : un
				//   predicat ecrit sur elle aurait designe des faces par un indice de
				//   sommet qui n'est qu'UNE des copies coincidentes.
				enum class DeleteMode : uint8 {
					Faces = 0, // une face part si elle est RETENUE (intention, ou deduction)
					Edges,     // ... si deux de ses sommets CONSECUTIFS sont retenus
					Verts      // ... si AU MOINS UN de ses sommets est retenu
				};
				// UNE SEULE IMPLEMENTATION, TROIS PORTES : trois copies divergeraient a la
				// premiere correction, et c'est celle qu'on oublie qui se ferait prendre
				// pour le produit.
				bool DeleteSelected(DeleteMode mode);
				bool DeleteSelectedFaces() { return DeleteSelected(DeleteMode::Faces); }
				bool DeleteSelectedEdges() { return DeleteSelected(DeleteMode::Edges); }
				bool DeleteSelectedVerts() { return DeleteSelected(DeleteMode::Verts); }
				// ── SELECTION ORDONNEE ──────────────────────────────────────────────
				// Pose la selection COMPLETE en une passe, tout en enregistrant l'ORDRE.
				// L'ordre est deduit des TRANSITIONS : un sommet qui passe de non
				// selectionne a selectionne recoit le rang suivant ; un sommet deja
				// selectionne garde le sien ; un sommet deselectionne perd le sien.
				// C'est ce qui permet a l'editeur de continuer a pousser son tableau
				// ENTIER a chaque frame (ce qu'il fait) sans ecraser l'historique :
				// seuls les changements reels comptent. Demander a l'appelant de signaler
				// chaque clic aurait disperse la responsabilite dans toute l'interface.
				void SetVertSelection(const uint8 *flags, uint32 count);
				// Rang courant du compteur (diagnostic / tests).
				uint32 SelectionStamp() const {
					return selCounter;
				}
				// Premier / dernier SELECTIONNE au sens de l'ordre des gestes. -1 si la
				// selection est vide. Repli sur l'indice si aucun rang n'est pose.
				int32 FirstSelected() const;
				int32 LastSelected() const;

				// ── SUBDIVISION DE SURFACE CATMULL-CLARK ────────────────────────────
				// Le lissage de Blender (« Subdivision Surface »), et non une simple
				// decoupe. La difference est de nature, pas de degre : une subdivision
				// LINEAIRE ajoute des sommets SUR la surface existante — la silhouette ne
				// bouge pas d'un micron, on n'obtient qu'un maillage plus dense. Catmull-
				// Clark DEPLACE les sommets vers la surface limite : un cube devient une
				// forme arrondie, ce qui est tout l'interet du modificateur.
				//
				// Regles appliquees (formulation standard, celle de Blender) :
				//   point de FACE   = barycentre des sommets de la face ;
				//   point d'ARETE   = moyenne des 2 sommets et des 2 points de face —
				//                     sur un BORD (une seule face), simple milieu, sinon la
				//                     bordure se retracterait vers l'interieur ;
				//   sommet DEPLACE  = (F + 2R + (n-3)V) / n, avec F la moyenne des points
				//                     de face voisins, R celle des MILIEUX d'aretes, n la
				//                     valence. Sur un bord : (M1 + 6V + M2) / 8, qui ne fait
				//                     intervenir QUE la bordure — c'est ce qui garde un bord
				//                     franc au lieu de l'aspirer vers la surface.
				//
				// Le calcul se fait sur l'identite SOUDEE : sans cela un cube (24 sommets
				// dupliques par face) aurait une valence de 2 partout et se disloquerait.
				// Les ATTRIBUTS (uv, uv2, couleur, tangente) restent PAR COIN et sont
				// interpoles a l'interieur de chaque face : les coutures d'UV survivent,
				// alors qu'un moyennage sur les sommets soudes les detruirait.
				//
				// levels : nombre d'applications (chacune multiplie les faces par ~4).
				// Renvoie false si le maillage est vide ou si rien n'a pu etre produit.
				bool SubdivideCatmullClark(int32 levels = 1);

				bool MergeSelectedVerts(const NkMergeParams &p = NkMergeParams{});
				bool MakeFaceFromSelected();

				// ── ARETES DE PREMIER PLAN ──────────────────────────────────────────
				// (Re)construit la liste d'aretes depuis les demi-aretes vivantes, en
				// PRESERVANT les aretes filaires existantes (rien d'autre ne pourrait les
				// recreer : elles ne sont incidentes a aucune face). A appeler apres toute
				// operation qui change la topologie.
				void RebuildEdges();

				// Nombre d'aretes vivantes (filaires comprises).
				uint32 EdgeCount() const;

				// ── ETAPE 2 : LES DEUX CYCLES DE BMESH ──────────────────────────────
				// L'etape 1 avait fait de l'arete une ENTITE ; elle restait deduite des
				// faces et ne savait rien de son voisinage. L'etape 2 lui donne son
				// CYCLE RADIAL (les faces qui la portent) et donne au sommet son CYCLE
				// DISQUE (les aretes qui en partent). Ce sont les deux parcours a partir
				// desquels Blender exprime boucles, anneaux, dissolution et bord.
				//
				// Ce que cela change concretement :
				//   - le NON-MANIFOLD devient representable et constatable. `twin` ne
				//     designe qu'une opposee : au-dela de deux faces par arete, un
				//     parcours fonde sur lui suit une branche ARBITRAIRE sans le dire ;
				//   - « les faces autour de cette arete » et « les aretes autour de ce
				//     sommet » sont en O(k) au lieu d'un balayage ou d'une table.
				//
				// Ces vues sont reconstruites par RebuildEdges(). Elles ne remplacent pas
				// les demi-aretes : elles s'y ajoutent, l'API publique existante etant
				// inchangee.

				// Arete portee par une demi-arete. NK_EM_INVALID si inconnue.
				NkEmId EdgeOfHedge(NkEmId h) const {
					return (h < (NkEmId)hedges.Size()) ? hedges[h].edge : NK_EM_INVALID;
				}
				// Arete reliant deux sommets (indices bruts ; l'identite soudee est
				// resolue en interne). NK_EM_INVALID si aucune.
				NkEmId EdgeBetween(uint32 a, uint32 b) const;
				// CYCLE RADIAL : demi-aretes portant l'arete (une par face incidente).
				uint32 EdgeHedges(NkEmId e, NkVector<NkEmId> &out) const;
				// CYCLE RADIAL, cote faces : faces incidentes, sans doublon.
				uint32 EdgeFaces(NkEmId e, NkVector<NkEmId> &out) const;
				// Face incidente a `e` AUTRE que `f`. NK_EM_INVALID si l'arete est un
				// bord (une seule face) OU non manifold (le « de l'autre cote » n'a
				// alors pas de sens, et en choisir un au hasard serait un mensonge).
				NkEmId EdgeOtherFace(NkEmId e, NkEmId f) const;
				// CYCLE DISQUE : aretes incidentes au sommet (identite soudee).
				uint32 VertEdges(uint32 v, NkVector<NkEmId> &out) const;
				// Sommet REPRESENTANT de `v` selon l'identite soudee courante. Repli sur
				// `v` tant que RebuildEdges n'a pas ete appele : un maillage sans aretes
				// n'a pas encore d'identite soudee, et en inventer une ici la ferait
				// diverger de celle que RebuildEdges posera.
				uint32 VertOwner(uint32 v) const {
					return (v < (uint32)canonOf.Size()) ? canonOf[v] : v;
				}

				uint32 RadialCount(NkEmId e) const {
					return (e < (NkEmId)edges.Size() && edges[e].alive) ? edges[e].radialCount : 0u;
				}
				bool EdgeIsWire(NkEmId e) const {
					return e < (NkEmId)edges.Size() && edges[e].alive && edges[e].radialCount == 0;
				}
				bool EdgeIsBoundary(NkEmId e) const {
					return e < (NkEmId)edges.Size() && edges[e].alive && edges[e].radialCount == 1;
				}
				bool EdgeIsManifold(NkEmId e) const {
					return e < (NkEmId)edges.Size() && edges[e].alive && edges[e].radialCount == 2;
				}
				bool EdgeIsNonManifold(NkEmId e) const {
					return e < (NkEmId)edges.Size() && edges[e].alive && edges[e].radialCount > 2;
				}
				// JUMELLE RADIALE : l'opposee de `h` selon le cycle radial.
				// Difference avec `Hedge::twin`, et raison d'etre de cette fonction : sur
				// une arete portee par TROIS faces ou plus, `twin` designe une opposee
				// ARBITRAIRE (celle que l'appariement a retenue) et tout parcours qui s'y
				// fie bascule silencieusement sur une branche que l'utilisateur n'a pas
				// choisie. Ici on REFUSE : NK_EM_INVALID, et le parcours s'arrete — ce que
				// fait Blender sur une arete non manifold.
				// Repli : si les aretes n'ont pas encore ete construites (edge == INVALID),
				// on rend `twin`, pour ne rien casser chez un appelant qui n'a pas appele
				// RebuildEdges().
				NkEmId RadialTwin(NkEmId h) const {
					if (h >= (NkEmId)hedges.Size())
						return NK_EM_INVALID;
					const NkEmId e = hedges[h].edge;
					if (e == NK_EM_INVALID || e >= (NkEmId)edges.Size())
						return hedges[h].twin;
					if (edges[e].radialCount != 2)
						return NK_EM_INVALID; // bord, filaire ou non manifold : pas d'oppose unique
					// Cycle a DEUX elements : la suivante EST l'opposee. Plus de
					// balayage de tranche, et plus de repli sur un indice hors bornes.
					const NkEmId o = hedges[h].rNext;
					if (o != NK_EM_INVALID && o != h)
						return o;
					return hedges[h].twin;
				}

				// Nombre d'aretes non manifold — un chiffre a surveiller apres toute
				// operation topologique : il ne devrait jamais augmenter par accident.
				uint32 NonManifoldEdgeCount() const;

				// Cree une arete FILAIRE entre deux sommets, si elle n'existe pas deja.
				// Renvoie l'index de l'arete, ou NK_EM_INVALID en cas d'echec.
				NkEmId AddWireEdge(uint32 a, uint32 b);

				// F sur EXACTEMENT deux sommets selectionnes : cree le segment qui les
				// relie, comme Blender. Renvoie false si la selection n'a pas exactement
				// deux sommets topologiques distincts, ou si l'arete existe deja.
				bool MakeEdgeFromSelected();

				// ── LOT 5 : DEPLACEMENT AVEC INFLUENCE ET SYMETRIE ──────────────────
				// Deplace la selection de `delta`, en propageant aux voisins selon
				// `prop` et en rejouant en miroir selon `sym`. C'est le point d'entree
				// unique du mouvement de sommets : l'editeur passe par lui pour que
				// proportional editing et symetrie s'appliquent PARTOUT de la meme
				// facon, plutot que d'etre reimplantes a chaque outil.
				// Renvoie false si rien n'est selectionne.
				bool MoveSelected(const NkVec3f &delta, const NkProportionalParams &prop = NkProportionalParams{},
								  const NkSymmetryParams &sym = NkSymmetryParams{});

				// Poids d'influence d'un sommet a la distance `d` pour un rayon `r`.
				// Expose pour que l'editeur puisse DESSINER le cercle d'influence avec
				// exactement la meme courbe que celle appliquee.
				static float32 ProportionalWeight(float32 d, float32 r, int32 falloff);
				bool SubdivideSelectedFaces(const NkSubdivideParams &p = NkSubdivideParams{});
				bool LoopCutFromSelectedEdge(const NkLoopCutParams &p = NkLoopCutParams{});

				// ── BEVEL / CHANFREIN (Ctrl+B, Ctrl+Shift+B) ────────────────────────
				// p.vertexOnly == false : BEVEL D'ARÊTE. Chaque arête dont les DEUX
				//   extrémités sont sélectionnées (et qui possède bien deux faces) est
				//   remplacée par une BANDE de p.segments face(s) ; les faces voisines
				//   reculent de p.offset le long de leurs arêtes. Les coins où plusieurs
				//   arêtes chanfreinées se rejoignent reçoivent une face de RACCORD.
				// p.vertexOnly == true : BEVEL DE SOMMET. Chaque sommet sélectionné est
				//   remplacé par une petite face (le coin est coupé), chaque face
				//   incidente gagnant un sommet supplémentaire.
				// ⚠ LIMITES ASSUMÉES : opère sur une copie SOUDÉE du maillage (les copies
				//   coïncidentes d'un coin fusionnent — c'est le modèle Blender) ; les
				//   sommets/arêtes de BORD (sans jumeau) sont ignorés pour les faces de
				//   raccord ; l'offset est mesuré LE LONG des arêtes (sur un coin non
				//   perpendiculaire la largeur perçue diffère donc légèrement de Blender) ;
				//   aucun traitement particulier des arêtes CONCAVES ni des auto-
				//   intersections quand l'offset est grand (l'écrêtage à 45 % l'évite).
				//
				// ⚠ LE CHANFREIN CREE DES FACES SANS MERE — c'est meme sa raison d'etre.
				// Regle arbitree (2026-08-23), la meme que pour l'extrusion d'aretes :
				// une face creee herite de la face a laquelle elle est GEOMETRIQUEMENT
				// ADJACENTE ; en cas d'ambiguite, celle qui apporte le plus de LONGUEUR
				// DE CONTOUR PARTAGE, egalite tranchee par l'INDICE LE PLUS BAS.
				// Trois familles de faces sortent d'ici, et elles n'heritent pas pareil :
				//   (a) faces d'origine aux coins remplaces -> elles ONT une mere ;
				//   (b) BANDE d'une arete chanfreinee -> deux voisines, ponderees par la
				//       longueur du contour partage LE LONG de l'arete (elles different
				//       des que les reculs des deux cotes different) ;
				//   (c) face de RACCORD a un sommet -> toutes les faces incidentes,
				//       ponderees par la longueur de l'anneau posee sur chacune.
				//
				// ⚠ UNE BANDE ENTIERE PORTE UN SEUL MATERIAU, meme a plusieurs segments.
				// Geometriquement, seuls le PREMIER et le DERNIER quad d'une bande
				// touchent une face voisine ; ceux du milieu ne touchent que l'arc. Leur
				// donner un materiau « propre » ferait apparaitre une couture au milieu
				// d'un chanfrein — une frontiere de couleur la ou la geometrie est
				// continue. Un chanfrein est UNE surface.
				//
				// ⚠ CAS DEGENERE ASSUME : quand les deux aretes d'un coin sont
				// chanfreinees, le point de recul est unique (ptPrev == ptNext) et la
				// longueur posee sur chaque face incidente vaut ZERO. Toutes les
				// ponderations sont alors egales et c'est l'indice le plus bas qui
				// tranche — ce qui est exactement la clause d'egalite de la regle, pas
				// une exception a part.
				//
				// `outMaterialChanged` recoit le nombre de faces voisines dont le materiau
				// n'a pas ete retenu.
				bool BevelSelected(const NkBevelParams &p = NkBevelParams{},
								   uint32 *outMaterialChanged = nullptr);

				// ── INSET FACES (I) ─────────────────────────────────────────────────
				// Insère une face plus PETITE à l'intérieur de chaque face sélectionnée,
				// reliée au contour d'origine par une BANDE de quads. Modes individual /
				// region (cf. NkInsetParams). La sélection passe sur la face intérieure,
				// comme dans Blender (on peut enchaîner I, ou E pour extruder).
				// ⚠ LIMITE : le rétrécissement est calculé par bissectrice de coin (exact
				//   sur les faces CONVEXES ; une face très concave peut s'auto-intersecter
				//   pour une épaisseur proche du rayon inscrit).
				bool InsetSelectedFaces(const NkInsetParams &p = NkInsetParams{});

				// ── EDGE SPLIT (V) — DÉ-SOUDURE LOCALE ──────────────────────────────
				// Sépare les arêtes sélectionnées : autour de chaque sommet touché, le
				// « ventilateur » de faces est découpé en GROUPES délimités par les arêtes
				// sélectionnées, et chaque groupe reçoit sa PROPRE copie du sommet. Les
				// faces de part et d'autre ne partagent donc plus rien le long de ces
				// arêtes (twins recalculés : ces demi-arêtes deviennent des bords).
				// ⚠ CAS PARTICULIERS / LIMITES :
				//   • une arête SEULE au milieu d'un ventilateur fermé ne coupe pas le
				//     ventilateur (on peut encore en faire le tour) : la topologie reste
				//     connexe, comme dans Blender. Il faut une CHAÎNE/BOUCLE d'arêtes pour
				//     détacher réellement une région ;
				//   • les arêtes de BORD (sans jumeau) sont déjà « ouvertes » -> ignorées ;
				//   • l'écart `gap` est obligatoire (cf. NkEdgeSplitParams).
				bool SplitSelectedEdges(const NkEdgeSplitParams &p = NkEdgeSplitParams{});

				// ── EDGE SPLIT PAR LISTE D'ARETES — LA PORTE DES COUTURES ───────────
				// MEME decoupe que ci-dessus (meme corps : les deux passent par
				// `SplitImpl`), mais les aretes sont DESIGNEES au lieu d'etre deduites
				// d'une selection de sommets.
				//
				// POURQUOI CETTE SURCHARGE EXISTE. `SplitSelectedEdges` decoupe l'arete
				// dont LES DEUX EXTREMITES sont selectionnees. Une couture UV, elle, est
				// un ensemble d'ARETES, et les deux ne sont pas traduisibles l'un dans
				// l'autre : sur un cube deplie en croix, les sommets touches par les 7
				// coutures couvrent presque toutes les aretes — on ne decouperait pas la
				// croix, on pulveriserait le cube. Mesure, pas supposee.
				//
				// `keepGeometry` : AUCUN ecart. Une de-soudure de couture est
				// TOPOLOGIQUE — les positions ne bougent pas d'un bit, sans quoi deplier
				// un modele le deformerait. C'est l'inverse du « rip » facon Blender (V),
				// qui ecarte justement pour montrer la dechirure.
				// ⚠ A NE PAS CONFONDRE avec `NkEdgeSplitParams::gap = 0`, qui ne signifie
				// PAS « ecart nul » mais « ecart AUTOMATIQUE de 1 % de la diagonale ».
				// Cette convention contredit celle du meme fichier (cf. NkExtrudeParams
				// l.31-36 : « offset < 0 => AUTO ; offset == 0 => la geometrie nait
				// EXACTEMENT sur l'originale »). Le defaut n'est PAS corrige ici : un
				// defaut qui change de sens casse silencieusement ses appelants. Il est
				// signale, et `keepGeometry` le contourne en forcant l'ecart a zero apres
				// que la regle existante a joue.
				//
				// `outDuplicated` recoit le nombre de sommets crees. Zero quand aucune
				// arete ne separe reellement un ventilateur : c'est un resultat, pas un
				// echec (cf. la limite de l'arete isolee, ci-dessus).
				bool SplitEdges(const NkEmId *edges, uint32 count, bool keepGeometry = true,
								uint32 *outDuplicated = nullptr);

				// ── SPIN / RÉVOLUTION (J) ───────────────────────────────────────────
				// Le PROFIL tourné = les arêtes dont les deux extrémités sont
				// sélectionnées (mode relié) ou les faces sélectionnées (mode duplicate).
				// La géométrie d'origine est CONSERVÉE (comme Blender) ; la sélection
				// passe sur le DERNIER anneau, pour enchaîner un autre spin ou un merge.
				// Sur 360° le dernier anneau retombe exactement sur le premier : la
				// soudure positionnelle (LinkTwins) referme le volume automatiquement.
				// localToSpin : matrice modèle->espace de p.center/p.axis (éditeur : la
				// transform monde de l'objet, pour utiliser le curseur 3D comme centre).
				// ⚠ LIMITE : l'orientation des faces créées est décidée par un test RADIAL
				//   (normale sortante par rapport à l'axe), ce qui convient aux profils de
				//   révolution usuels ; un profil qui croise l'axe peut sortir retourné.
				bool SpinSelected(const NkSpinParams &p, const NkMat4f &localToSpin = NkMat4f::Identity(),
								  uint32 *outMaterialChanged = nullptr);

				// ── BALAYAGE LE LONG D'UNE COURBE (21/09) ───────────────────────────
				// Un PROFIL 2D (dans le plan normal au chemin) BALAYE le long d'une
				// polyligne : c'est ce qui manquait pour un tuyau, une anse, une rampe,
				// une moulure -- et pour le COYAU d'un toit chinois, dont la courbure
				// ne s'obtient ni par revolution ni par extrusion droite.
				//
				// ⚠️ L'ORIENTATION EST A ROTATION MINIMALE (« double reflection », Wang
				//    et al. 2008), PAS UN REPERE DE FRENET. Le repere de Frenet se
				//    RETOURNE aux points d'inflexion et n'existe pas sur un segment
				//    droit (courbure nulle) : un tuyau construit ainsi se vrille d'un
				//    demi-tour au milieu, sans que rien ne le signale. La rotation
				//    minimale transporte le repere d'un point au suivant par deux
				//    reflexions, ce qui est stable sur une droite comme dans une boucle.
				//
				// `profil` : `np` points (x, y) dans le plan (normale, binormale).
				// `chemin` : `nc` points (>= 2). `ferme` : le profil est un contour
				// FERME (le dernier point rejoint le premier) -> le balayage produit un
				// tube ; `bouchons` ajoute alors les deux faces d'extremite.
				// Le maillage courant est REMPLACE. Rend faux (et ne touche a rien) si
				// les tableaux sont trop petits.
				//
				// LE SENS DU PROFIL EST NORMALISE ICI. L'appelant dessine une section ;
				// il n'a pas a savoir que son sens de parcours decide de l'endroit et de
				// l'envers. L'aire signee du contour est calculee, et le profil est
				// parcouru a l'envers quand il faut.
				//
				// ⚠️ LA CIBLE EST LA CONVENTION DU DEPOT, ET ELLE EST L'INVERSE DE CELLE
				//    DES MANUELS. Mesure du 21/09 : le cube unite du modeleur a un
				//    volume signe de -1,000000, la sphere -0,515, le cylindre -0,520 --
				//    et tous trois recoivent de RecomputeNormals des normales qui
				//    pointent DEHORS (banc sweep/sens-du-profil : cube-reference =
				//    1,000). Viser l'aire positive, « comme dans les livres », donne un
				//    tube dont 100 % des normales pointent DEDANS. Cette ligne a ete
				//    ecrite a l'envers une premiere fois, et c'est la primitive du
				//    depot, pas un manuel, qui a tranche.
				//
				// `refInitiale` : LA DIRECTION DU PREMIER AXE DU PROFIL. Sans elle, le
				// repere de depart est choisi arbitrairement (un vecteur non colineaire
				// a la premiere tangente) : la section est bien orientee LE LONG du
				// chemin, mais son roulis de depart est celui du hasard. Une tuile
				// ronde doit avoir son dos EN HAUT ; c'est le seul moyen de le dire.
				// Le vecteur est orthogonalise a la tangente ; s'il lui est colineaire,
				// il est ignore et le choix arbitraire reprend.
				bool BuildSweep(const NkVec2f *profil, uint32 np, const NkVec3f *chemin, uint32 nc, bool ferme = true,
								bool bouchons = true, const NkVec3f *refInitiale = nullptr);

				// ── DISSOLVE (Ctrl+X) — fusion en n-gon, PAS un trou ────────────────
				// Principe unique aux trois modes : on marque un ensemble d'arêtes à
				// RETIRER, puis on reparcourt le CONTOUR de chaque région ainsi fusionnée
				// (en sautant les arêtes retirées via les jumeaux) — ce qui reconstruit
				// directement un cycle de demi-arêtes propre, donc un n-gon manifold.
				// ⚠ LIMITES ASSUMÉES :
				//   • arête de BORD (sans jumeau) : rien à fusionner -> ignorée ;
				//   • arête dont les deux côtés sont la MÊME face : ignorée (dégénérée) ;
				//   • une région fusionnée qui possède un TROU produit deux contours, donc
				//     deux faces distinctes (le n-gon à trou n'existe pas ici, ni dans un
				//     maillage polygonal classique) ;
				//   • la face résultante peut être NON PLANE (autorisé, comme Blender) ;
				//   • sommet de bord ou de valence < 3 en mode Verts : ignoré ;
				//   • les sommets devenus inutilisés sont COMPACTÉS (pas de sommet isolé).
				// ⚠ DISSOLVE FUSIONNE DES FACES : meme regle que Quadify, et pour la
				// meme raison. La face fusionnee prend le materiau du CONTRIBUTEUR
				// DOMINANT PAR L'AIRE de sa region ; a aire egale, l'INDICE LE PLUS BAS.
				// `outMaterialChanged`, quand il est fourni, recoit le nombre de faces
				// source dont le materiau n'a pas ete retenu -- la perte, comptee et non
				// supposee.
				bool DissolveSelected(const NkDissolveParams &p = NkDissolveParams{},
									  uint32 *outMaterialChanged = nullptr);
				// planePoint / planeNormal sont exprimés dans l'espace de `localToPlaneSpace`
				// (= matrice modèle→monde côté éditeur ; identité pour une op locale pure IA).
				bool BisectByPlane(const NkVec3f &planePoint, const NkVec3f &planeNormal,
								   const NkMat4f &localToPlaneSpace);

			private:
				// ── CORPS UNIQUE DE L'EDGE SPLIT ───────────────────────────────────
				// `SplitSelectedEdges` et `SplitEdges` sont deux PORTES sur la meme
				// decoupe de ventilateur. Elles ne different que par la facon de designer
				// les aretes a couper : deduites de la selection de sommets pour la
				// premiere (`edgeList == nullptr`), donnees telles quelles pour la
				// seconde.
				//
				// UNE SEULE IMPLEMENTATION, ET C'EST UNE REGLE DE CE FICHIER : une
				// deuxieme decoupe de ventilateur ecrite a cote de celle-ci divergerait,
				// exactement comme la structure demi-arete concurrente supprimee en
				// juillet (cf. l'en-tete du fichier). Le chemin historique passe par
				// `edgeList == nullptr` et reste, ligne pour ligne, ce qu'il etait.
				bool SplitImpl(const NkEdgeSplitParams &p, const NkEmId *edgeList, uint32 edgeCount,
							   bool keepGeometry, uint32 *outDuplicated);

				// Lie les jumeaux (twin) via une table de hachage sur (min,max) des sommets.
				void LinkTwins();
				// Retire du tableau les faces et demi-aretes mortes. Le chemin par la
				// soupe de polygones les faisait disparaitre sans le dire ; une operation
				// EN PLACE doit le faire explicitement, sinon `faces.Size()` diverge
				// alors qu aucune topologie n a bouge.
				// `aRemapper` : liste d'indices de demi-aretes que l'appelant detient et
				// que la renumerotation doit suivre. Les entrees mortes en sont
				// RETIREES. Cf. le commentaire dans la definition.
				void CompactDead(NkVector<NkEmId> *aRemapper = nullptr);
				// Re-apparie les jumelles du SEUL voisinage touche. Rend false si
				// l'appariement positionnel est ambigu -- l'appelant retombe alors sur
				// `LinkTwins()`. Cf. le commentaire au-dessus de la definition.
				bool LinkTwinsLocal(const NkVector<NkEmId> &touchees, const NkVector<uint32> &copies, uint32 nv0);

				// ── ACCES AUX CHAINAGES ─────────────────────────────────────────
				// ⚠ AUCUNE REFERENCE RENDUE, JAMAIS. Un `NkEmId &` sur un champ de
				// `edges` serait plus court a ecrire — et le premier PushBack qui
				// relogerait le tableau en ferait une adresse liberee. Le piege a
				// deja ete paye une fois (lire `diskPool` en le remplissant).
				NkEmId DiskNext(NkEmId e, uint32 v) const {
					return (edges[e].v0 == (NkEmId)v) ? edges[e].dNext0 : edges[e].dNext1;
				}
				NkEmId DiskPrev(NkEmId e, uint32 v) const {
					return (edges[e].v0 == (NkEmId)v) ? edges[e].dPrev0 : edges[e].dPrev1;
				}
				void DiskSetNext(NkEmId e, uint32 v, NkEmId x) {
					if (edges[e].v0 == (NkEmId)v)
						edges[e].dNext0 = x;
					else
						edges[e].dNext1 = x;
				}
				void DiskSetPrev(NkEmId e, uint32 v, NkEmId x) {
					if (edges[e].v0 == (NkEmId)v)
						edges[e].dPrev0 = x;
					else
						edges[e].dPrev1 = x;
				}
				// Branche `e` sur le cycle disque du sommet REPRESENTANT `r`, EN FIN
				// de cycle. Aucun deplacement, aucune copie, aucun espace perdu.
				void DiskAppend(uint32 r, NkEmId e);
				// Branche `h` sur le cycle radial de `e`, EN FIN de cycle.
				void RadialAppend(NkEmId e, NkEmId h);
				// Une face polygone (indices [s..e[ dans fv) est sélectionnée si TOUS ses
				// sommets le sont (Vert::sel).
				bool PolyFaceSelected(const NkVector<uint32> &fv, uint32 s, uint32 e) const;
				// Recopie une sélection par-sommet (indexée sur le nouveau maillage) dans Vert::sel.
				void ApplyVertSel(const NkVector<uint8> &vsel);
				// Sous-étape de subdivision (une passe Catmull-Clark).
				bool SubdivideSelectedOnce();
		};

		// ── HISTORIQUE UNDO/REDO (mémento) ──────────────────────────────────────
		// Stocke des SNAPSHOTS complets de NkEditMesh (les maillages d'édition sont
		// petits -> simple et robuste pour toute topologie, approche edit-mode Blender).
		// Modèle : Commit(pré-état) AVANT une commande mutante ; Undo/Redo échangent
		// l'état courant avec la pile. Réutilisable (éditeur, rejeu IA). Cap par défaut 64.
		class NkEditHistory {
			public:
				void Clear();

				void SetLimit(uint32 n) {
					mLimit = (n < 1u) ? 1u : n;
				}

				// À appeler AVANT une commande mutante réussie, avec l'état d'AVANT la
				// mutation : empile le point de retour et invalide la pile de redo.
				void Commit(const NkEditMesh &preState);

				bool CanUndo() const {
					return !mUndo.Empty();
				}

				bool CanRedo() const {
					return !mRedo.Empty();
				}

				// Échange `mesh` avec l'état précédent/suivant. false si rien à faire.
				bool Undo(NkEditMesh &mesh);
				bool Redo(NkEditMesh &mesh);

				uint32 UndoCount() const {
					return (uint32)mUndo.Size();
				}

				uint32 RedoCount() const {
					return (uint32)mRedo.Size();
				}

			private:
				NkVector<NkEditMesh> mUndo; // états passés (sommet = le plus récent)
				NkVector<NkEditMesh> mRedo; // états annulés (rejouables)
				uint32 mLimit = 64u;
		};

		// ── COMMANDE D'ÉDITION SÉRIALISABLE (la couche de commandes rendue DONNÉE) ──
		// Représente UNE opération d'édition comme une DONNÉE (type + paramètres +
		// sélection au moment de l'application). Deux usages clés :
		//   • MODIFICATEURS non-destructifs : une pile de commandes rejouée depuis un
		//     maillage de base (mirror/array/subsurf = des commandes paramétrées).
		//   • IA (NKAI) : espace d'actions + données d'IMITATION (on enregistre les
		//     sessions de modélisation, on rejoue / on apprend une policy).
// X11 (Xlib) définit `None` en macro (0) et casserait NkMeshEditOp::None sur le
// chemin Linux/XLib — même famille de pollution que `Bool` (cf. #undef Bool ailleurs).
#ifdef None
#undef None
#endif

		enum class NkMeshEditOp : uint8 {
			None = 0,
			Extrude,
			Delete,
			Merge,
			MakeFace,
			Subdivide,
			LoopCut,
			Bisect,
			Move,
			// AJOUTÉS EN FIN d'énumération (l'op est sérialisée en uint8 : ne jamais
			// réordonner, sinon les sessions .nkmec existantes deviendraient fausses).
			ExtrudeVerts,
			ExtrudeEdges,
			Bevel,
			Inset,
			EdgeSplit,
			Spin,
			Dissolve,
			ToSphere,
			ShrinkFatten,
			// AJOUTEES EN FIN (l'op est serialisee en uint8) : X ne supprime pas la
			// meme chose selon le sous-mode, exactement comme Blender.
			DeleteEdges,
			DeleteVerts,
			// AJOUTEE EN FIN (l'op est serialisee en uint8) : un COUP DE BROSSE.
			// Il entre dans la couche de commandes -- et non a cote -- pour une
			// raison que Rodolf a posee le 19/09 : les corrections a la main
			// doivent SURVIVRE a une regeneration. On regenere la base depuis le
			// document, puis on REJOUE la pile. Une sculpture qui ne serait pas
			// une commande serait perdue au premier tour de la spirale.
			Sculpt,
			// AJOUTEE EN FIN (l'op est serialisee en uint8) : LE MASQUE EN BLOC --
			// tout masquer, tout demasquer, inverser. Ces trois gestes sont des
			// COMMANDES et non des appels directs, pour la raison qui a fait entrer
			// le coup de brosse : ce qui n'est pas une commande ne s'annule pas, ne
			// se rejoue pas, et disparait au premier tour de la spirale de
			// regeneration.
			MaskAll
		};

		// ── PARAMETRES D'UN COUP DE BROSSE ─────────────────────────────
		// ⚠️ AUTO-SUFFISANTS, ET C'EST LE POINT. On enregistre les VALEURS
		//    EFFECTIVES du geste, jamais le NOM de la brosse qui l'a produit.
		//    Une brosse est une donnee, donc un fichier, donc quelque chose que
		//    l'utilisateur peut modifier demain. Si la commande renvoyait a
		//    « dessiner », rejouer la session apres un reglage de « dessiner »
		//    reproduirait un AUTRE geste que celui qui a ete fait -- et sans rien
		//    dire. C'est « un chiffre voyage sans sa condition » applique a un
		//    geste : ce qui est rejoue doit porter tout ce dont il depend.
		//    Le nom reste, mais pour le JOURNAL et l'affichage, jamais pour
		//    retrouver des reglages a l'execution.
		struct NkSculptCmdParams {
			float32 radius = 0.25f;   ///< unites monde
			float32 strength = 0.5f;
			float32 hardness = 0.5f;
			float32 dir = 1.f;        ///< +1 sort de la surface, -1 y entre
			uint8 falloff = 0;        ///< NkSculptFalloffKind
			uint8 primitive = 0;      ///< NkSculptOp
			char brushName[48] = {};  ///< pour le journal et l'affichage UNIQUEMENT
		};

		// ── LE MASQUE EN BLOC ───────────────────────────────────────────
		// `mode` : 0 = tout DEMASQUER (et liberer), 1 = tout MASQUER a `poids`,
		// 2 = INVERSER. ⚠️ Inverser un maillage SANS masque le masque
		// entierement -- c'est Blender, et la seule lecture coherente de
		// « l'inverse de rien ».
		struct NkMaskAllParams {
			uint8 mode = 0;
			float32 poids = 1.f;
		};

		struct NkMeshEditCommand {
				NkMeshEditOp op = NkMeshEditOp::None;
				NkVector<uint32> selection;			  // sommets sélectionnés à l'application
				// ── L'INTENTION DE FACE, ENREGISTREE AVEC LA COMMANDE (v10) ──────────
				// ⚠️ SANS ELLE, DEUX GESTES HUMAINS DIFFERENTS S'ECRIVAIENT PAREIL.
				//    Mesure du 14/09 : sur un cube, « deux faces opposees » et « tout
				//    selectionner » allument LES MEMES SOMMETS -- 370 octets identiques,
				//    et un fichier parfaitement valide. Pire que l'egalite des octets :
				//    au rejeu, `ExtrudeSelectedFaces` re-deduisait les faces depuis les
				//    sommets et en extrudait SIX la ou la main en avait extrude DEUX.
				//    Le journal ne se contentait pas d'oublier l'intention, il en
				//    rejouait une AUTRE.
				//
				//    Un octet par face, tel que l'editeur l'a pose au clic (c'est le
				//    meme tableau que `SetFaceSelection` consomme). VIDE = aucune
				//    intention enregistree : le rejeu retombe alors sur la deduction
				//    historique, qui est exactement la semantique des sessions v9 et
				//    anterieures. Un fichier d'hier garde donc le sens qu'il avait.
				NkVector<uint8> faceSel;
				NkExtrudeParams extrude;			  // (op == Extrude / ExtrudeVerts / ExtrudeEdges)
				NkMergeParams merge;				  // (op == Merge)
				NkSubdivideParams subdiv;			  // (op == Subdivide)
				NkLoopCutParams loopcut;			  // (op == LoopCut) nombre de coupes
				NkBevelParams bevel;				  // (op == Bevel) largeur / segments / mode sommet
				NkInsetParams inset;				  // (op == Inset) épaisseur / profondeur / individual
				NkEdgeSplitParams esplit;			  // (op == EdgeSplit) écartement de la déchirure
				NkSpinParams spin;					  // (op == Spin) centre / axe / angle / pas
				NkMat4f spinXform = NkMat4f::Identity(); // (op == Spin) modèle -> espace du spin
				NkDissolveParams dissolve;			  // (op == Dissolve) mode Verts/Edges/Faces
				NkToSphereParams tosphere;			  // (op == ToSphere) centre / facteur
				NkShrinkFattenParams shrinkfatten;	  // (op == ShrinkFatten) deplacement le long des normales
				NkVec3f planePoint = {0.f, 0.f, 0.f}; // (op == Bisect)
				NkVec3f planeNormal = {0.f, 1.f, 0.f};
				NkMat4f bisectXform = NkMat4f::Identity();
				NkVector<NkVec3f> moveDeltas; // (op == Move) delta par sommet (aligné sur selection)

				// (op == Sculpt) LE TRAIT, DANS LE REPERE DE L'OBJET.
				// ⚠️ JAMAIS EN PIXELS ECRAN : un trait en pixels ne survit pas a une
				//    rotation de camera. Jamais par indices d'elements non plus : ils
				//    sont reconstruits a chaque changement de topologie, donc ils
				//    deviennent SILENCIEUSEMENT faux -- et la sculpture EST du
				//    remaillage. Une polyligne de points ne reference AUCUN element du
				//    maillage : elle survit a la camera comme au remaillage, et c'est
				//    ce qui la rend rejouable.
				NkSculptCmdParams sculpt;
				NkMaskAllParams maskAll; // (op == MaskAll) tout masquer / demasquer / inverser
				NkVector<NkVec3f> sculptPoints;  // centres des tampons
				NkVector<NkVec3f> sculptNormals; // normale au point de pose (meme taille)

				// Pose la sélection sur `m` puis exécute l'op. true si la géométrie a changé.
				bool Apply(NkEditMesh &m) const;
		};

		// Journal de commandes : enregistre une session, la rejoue sur un maillage.
		class NkMeshEditRecorder {
			public:
				void Clear() {
					mCommands.Clear();
				}

				void Push(const NkMeshEditCommand &c) {
					mCommands.PushBack(c);
				}

				uint32 Count() const {
					return (uint32)mCommands.Size();
				}

				const NkMeshEditCommand &At(uint32 i) const {
					return mCommands[i];
				}

				// LA VERSION DU FICHIER RELU (0 = rien n'a ete relu ; l'ecriture est
				// toujours a la version courante). Elle existe pour qu'un affichage
				// puisse DIRE « cette session est anterieure a l'intention de face »
				// plutot que de rejouer une semantique ancienne en silence.
				uint32 Version() const {
					return mVersion;
				}

				// Rejoue toutes les commandes (dans l'ordre) sur `mesh`. Renvoie le nb appliquées.
				uint32 ReplayOnto(NkEditMesh &mesh) const;
				// Sérialisation binaire autonome (magic "NMEC", versionnée) — persiste une
				// session sur disque : données d'imitation IA + modificateurs sauvegardables.
				void Serialize(NkVector<uint8> &out) const;
				bool Deserialize(const uint8 *data, uint32 size);

			private:
				NkVector<NkMeshEditCommand> mCommands;
				uint32 mVersion = 0u;
		};

		// ── STACK DE MODIFICATEURS (non-destructif, façon Blender) ──────────────────
		// Un modificateur = transformation PARAMÉTRIQUE d'un maillage (Mirror/Array/Subsurf).
		// La pile s'évalue sur un maillage de BASE (jamais modifié) -> maillage affiché :
		//   base → mod0(params) → mod1(params) → … → résultat.
		// Changer un paramètre = ré-évaluer la pile (la base reste éditable dessous).
		// Fondation directe : ces modificateurs sont aussi des ACTIONS composables pour l'IA.
		// ⚠ VALEURS SERIALISEES : on AJOUTE EN FIN, on ne renumerote jamais. Une
		// scene enregistree designe ses modificateurs par ces entiers.
		// Faisabilite : ne figurent ici que les modificateurs realisables avec le
		// maillage SEUL. Ceux de Blender qui exigent un AUTRE objet (Shrinkwrap,
		// Curve, Lattice, Hook, Armature, Mesh/Surface Deform, Boolean, Warp), des
		// GROUPES DE SOMMETS (Mask par groupe, Vertex Weight *), une TEXTURE
		// (Displace, UV Project), une SIMULATION (Cloth, Fluid, Ocean, Soft Body,
		// particules) ou un systeme de POILS sont hors de portee tant que ces
		// systemes n'existent pas — les lister sans les faire serait mentir.
		enum class NkModifierType : uint8 {
			Mirror = 0,
			Array = 1,
			Subsurf = 2,
			Solidify = 3,	   // epaissit une surface : coque interne + bordure
			Triangulate = 4,   // n-gons -> triangles
			Weld = 5,		   // soude les sommets sous une distance
			Bevel = 6,		   // chanfreine toutes les aretes
			Screw = 7,		   // revolution du profil autour d'un axe
			EdgeSplit = 8,	   // dedouble les aretes vives (angle)
			Decimate = 9,	   // simplifie : dissout les aretes quasi coplanaires
			Build = 10,		   // ne montre qu'une PROPORTION des faces (animable)
			Mask = 11,		   // ne garde que les faces selectionnees
			Cast = 12,		   // projette vers sphere / cylindre / cube
			SimpleDeform = 13, // torsion / courbure / effilement / etirement
			Smooth = 14,	   // relaxe les sommets vers la moyenne des voisins
			Wave = 15,		   // ondulation (la phase est faite pour etre animee)
			SmoothByAngle = 16 // ombrage doux sous un angle, franc au-dela
		};

		// ── PARAMETRE ADRESSABLE PAR NOM ───────────────────────────────────────────
		// Chaque modificateur publie la LISTE de ses parametres : nom stable, libelle,
		// type, et ou le lire dans la structure. Cela sert trois choses a la fois :
		//   1. une interface peut se construire toute seule, sans connaitre les
		//      modificateurs un par un ;
		//   2. l'IA et le rejeu peuvent regler « arrayCount » sans code dedie ;
		//   3. et surtout, demande de Rihen : une ANIMATION pourra plus tard marquer
		//      N'IMPORTE QUEL parametre. Une courbe d'animation designe une cible par
		//      (identifiant du modificateur, nom du parametre) — deux choses STABLES,
		//      qui survivent au reordonnancement de la pile.
		// C'est pour cela que `name` ne doit JAMAIS etre renomme une fois publie : ce
		// n'est pas un libelle, c'est une CLE. `label` est la, lui, pour l'affichage.
		enum class NkModParamType : uint8 { Bool = 0, Int, Float, Vec3 };

		struct NkModParam {
				const char *name = "";	// CLE stable (animation, rejeu, serialisation)
				const char *label = ""; // libelle affichable, librement modifiable
				NkModParamType type = NkModParamType::Float;
				uint32 offset = 0;			   // position du champ dans NkMeshModifier
				float32 minV = 0.f, maxV = 0.f; // bornes indicatives (0/0 = libre)
		};

		struct NkMeshModifier {
				NkModifierType type = NkModifierType::Mirror;
				bool enabled = true;
				// IDENTIFIANT STABLE, attribue par la pile. Il ne change ni au
				// reordonnancement, ni a la desactivation, ni a la duplication (la copie
				// en recoit un neuf). C'est l'ancre d'une future courbe d'animation :
				// pointer un modificateur par son INDICE se casserait des qu'on le
				// remonte d'un cran dans la pile, ce que Blender permet a tout moment.
				uint32 id = 0;
				// Mirror : miroir sur un axe au plan de l'origine (+ soudure des sommets sur le plan).
				int32 mirrorAxis = 0; // 0=X 1=Y 2=Z
				bool mirrorMerge = true;
				float32 mirrorMergeDist = 1e-3f;
				// Array : duplique le maillage `arrayCount` fois avec un décalage constant.
				int32 arrayCount = 3;
				NkVec3f arrayOffset = {2.f, 0.f, 0.f};
				// Subsurf : `subsurfLevels` applications sur TOUT le maillage.
				// subsurfSimple : false (defaut) = CATMULL-CLARK, le lissage de Blender ;
				//   true = mode « Simple » de Blender, subdivision LINEAIRE qui densifie
				//   sans deformer. Les deux existent chez Blender parce qu'ils repondent a
				//   des besoins differents : lisser une forme, ou densifier pour sculpter.
				int32 subsurfLevels = 1;
				bool subsurfSimple = false;

				// ── LOT AJOUTE ──────────────────────────────────────────────────────
				// Solidify : epaissit une surface. offset -1 = vers l'interieur,
				// +1 = vers l'exterieur, 0 = de part et d'autre (convention Blender).
				float32 solidifyThickness = 0.05f;
				float32 solidifyOffset = -1.f;
				bool solidifyRim = true; // referme le bord, sinon la coque reste ouverte

				// Triangulate : les faces de moins de `minVerts` cotes sont laissees
				// telles quelles (un quad reste un quad si minVerts vaut 5).
				int32 triangulateMinVerts = 4;

				// Weld : distance de soudure. C'est « Remove Doubles » en modificateur.
				float32 weldDistance = 0.001f;

				// Bevel : largeur mesuree le long des aretes, et nombre de segments.
				float32 bevelWidth = 0.05f;
				int32 bevelSegments = 1;

				// Screw : revolution du profil. angle en degres, height = pas d'helice.
				int32 screwSteps = 12;
				float32 screwAngle = 360.f;
				float32 screwHeight = 0.f;
				int32 screwAxis = 1; // 0=X 1=Y 2=Z

				// EdgeSplit : angle diedre au-dela duquel l'arete est dedoublee.
				float32 edgeSplitAngle = 30.f;

				// Decimate : angle en dessous duquel deux faces voisines sont jugees
				// coplanaires et leur arete dissoute (mode « Planar » de Blender).
				float32 decimateAngle = 5.f;

				// Build : proportion de faces conservees, 0..1. Ce parametre existe POUR
				// etre anime — c'est le seul modificateur dont l'interet est le temps.
				float32 buildRatio = 1.f;

				// Mask : ne garde que les faces selectionnees (invert = le complement).
				bool maskInvert = false;

				// Cast : 0=sphere 1=cylindre 2=cube. factor 0 = rien, 1 = forme pure.
				// radius <= 0 : deduit du maillage (rayon moyen) — un rayon impose a
				// zero ferait imploser le modele, ce qui n'est jamais l'intention.
				int32 castType = 0;
				float32 castFactor = 0.5f;
				float32 castRadius = 0.f;

				// SimpleDeform : 0=torsion 1=courbure 2=effilement 3=etirement.
				// angle en degres (torsion/courbure), factor pour effilement/etirement.
				int32 deformMode = 0;
				float32 deformAngle = 45.f;
				float32 deformFactor = 0.5f;
				int32 deformAxis = 1; // 0=X 1=Y 2=Z

				// Smooth : relaxation laplacienne. factor 0..1, repetee `repeat` fois.
				float32 smoothFactor = 0.5f;
				int32 smoothRepeat = 1;

				// Wave : ondulation radiale. `phase` est le parametre a animer.
				float32 waveHeight = 0.1f;
				float32 waveWidth = 0.5f;
				float32 wavePhase = 0.f;
				int32 waveAxis = 1; // axe le long duquel l'onde deplace les sommets

				// SmoothByAngle : ombrage doux en dessous de cet angle diedre, franc
				// au-dela. C'est « Auto Smooth » de Blender, devenu un modificateur.
				float32 autoSmoothAngle = 30.f;

				void Apply(NkEditMesh &m) const; // transforme `m` en place

				// ── ACCES GENERIQUE AUX PARAMETRES ──────────────────────────────────
				// Les scalaires (Bool / Int / Float) passent par float32 : c'est le type
				// d'une courbe d'animation, et la conversion est faite ICI plutot que
				// chez chaque appelant — sinon chacun arrondirait a sa facon.
				uint32 ParamCount() const;
				const NkModParam *ParamAt(uint32 i) const;
				const NkModParam *FindParam(const char *name) const;
				bool GetParam(const char *name, float32 &out) const;
				bool SetParam(const char *name, float32 v);
				bool GetParamVec3(const char *name, NkVec3f &out) const;
				bool SetParamVec3(const char *name, const NkVec3f &v);
		};

		// Nom lisible d'un type de modificateur (interface, journal, serialisation).
		const char *NkModifierTypeName(NkModifierType t);
		// Table des parametres d'un type donne.
		const NkModParam *NkModifierParams(NkModifierType t, uint32 &count);

		class NkModifierStack {
			public:
				NkVector<NkMeshModifier> modifiers;

				bool Empty() const {
					return modifiers.Empty();
				}

				void Clear() {
					modifiers.Clear();
					mNextId = 1;
				}

				// Empile et renvoie l'IDENTIFIANT STABLE attribue. Cet identifiant est ce
				// qu'il faut retenir ailleurs (animation, interface), jamais l'indice.
				uint32 Add(const NkMeshModifier &mod) {
					NkMeshModifier m = mod;
					m.id = mNextId++;
					modifiers.PushBack(m);
					return m.id;
				}

				uint32 Count() const {
					return (uint32)modifiers.Size();
				}

				// ── GESTION DE LA PILE (facon Blender) ──────────────────────────────
				// L'ORDRE EST SIGNIFIANT : miroir puis tableau ne donne pas la meme chose
				// que tableau puis miroir. Pouvoir remonter/descendre un modificateur
				// n'est donc pas un confort d'interface, c'est un parametre de resultat.
				bool Remove(uint32 index);
				bool MoveUp(uint32 index);	 // vers le HAUT = evalue plus TOT
				bool MoveDown(uint32 index); // vers le BAS = evalue plus TARD
				bool SetEnabled(uint32 index, bool on);
				// Duplique le modificateur (la copie recoit un identifiant NEUF : deux
				// entrees animables independamment, sinon une courbe piloterait les deux).
				bool Duplicate(uint32 index);

				int32 IndexOfId(uint32 id) const;
				NkMeshModifier *FindById(uint32 id);
				const NkMeshModifier *FindById(uint32 id) const;

				// APPLIQUER (Blender : « Apply ») : cuit CE modificateur dans le maillage
				// de BASE et le retire de la pile. Le maillage editable devient le
				// resultat — l'operation est donc DESTRUCTIVE, c'est tout son objet.
				//
				// Comme Blender, on autorise a appliquer un modificateur qui n'est PAS le
				// premier, mais le resultat ne sera alors pas celui qu'on voyait a
				// l'ecran : l'affichage montre la pile ENTIERE, alors qu'on ne cuit que ce
				// modificateur-la, sans ceux qui le precedent. `outWarnNotFirst` le
				// signale a l'appelant, a charge pour lui de prevenir l'utilisateur —
				// plutot que de refuser (Blender ne refuse pas) ou de se taire.
				bool ApplyToBase(uint32 index, NkEditMesh &base, bool *outWarnNotFirst = nullptr);

				// out = base, puis chaque modificateur ACTIVE applique dans l'ordre.
				void Evaluate(const NkEditMesh &base, NkEditMesh &out) const;

			private:
				uint32 mNextId = 1;
		};

	} // namespace renderer
} // namespace nkentseu
