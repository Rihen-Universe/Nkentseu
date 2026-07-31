// =============================================================================
// NkEditMesh.h — NKRenderer
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
						// Tranche dans `diskPool` : les aretes incidentes a ce sommet.
						// Renseignee sur le sommet REPRESENTANT de l'identite soudee (les
						// copies coincidentes pointent la meme tranche) — sinon, sur une
						// primitive qui duplique ses sommets par face, chaque copie ne
						// verrait qu'un tiers de ses aretes.
						uint32 diskStart = 0;
						uint32 diskCount = 0;
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
						// Tranche dans `radialPool` : TOUTES les demi-aretes qui portent
						// cette arete, donc toutes les faces incidentes.
						//
						// POURQUOI c'est necessaire : `Hedge::twin` ne peut designer
						// QU'UNE opposee. Sur une arete partagee par TROIS faces (jonction
						// en T, tres courante des qu'on colle une cloison sur un mur),
						// l'appariement en retient deux et la troisieme devient invisible
						// pour tout parcours. Le cycle radial les porte TOUTES, donc le
						// non-manifold cesse d'etre un cas qu'on ignore pour devenir un cas
						// qu'on peut CONSTATER et traiter.
						//
						// Tranche contigue plutot que liste chainee (BMesh en utilise une) :
						// la structure est reconstruite en bloc par RebuildEdges, jamais
						// modifiee arete par arete — une liste chainee n'apporterait ici que
						// des indirections et des invariants supplementaires a maintenir.
						uint32 radialStart = 0;
						uint32 radialCount = 0;
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
				};

				NkVector<Vert> verts;
				NkVector<Hedge> hedges;
				NkVector<Face> faces;
				// Aretes de premier plan. Reconstruites par RebuildEdges() apres toute
				// operation topologique ; les aretes FILAIRES y survivent (elles ne sont
				// deduites d'aucune face, donc rien d'autre ne peut les recreer).
				NkVector<Edge> edges;
				// Compteur monotone des rangs de selection (cf. Vert::selOrder). Jamais
				// remis a zero en cours d'edition : c'est un ORDRE, pas un compte.
				uint32 selCounter = 0;
				// Reservoirs des deux cycles BMesh (etape 2). Reconstruits en meme temps
				// que `edges` : ce sont des VUES sur la topologie, jamais une source.
				NkVector<NkEmId> radialPool; // demi-aretes, groupees par arete
				NkVector<NkEmId> diskPool;   // aretes, groupees par sommet representant

				void Clear() {
					verts.Clear();
					hedges.Clear();
					faces.Clear();
					edges.Clear();
				}

				uint32 VertCount() const {
					return (uint32)verts.Size();
				}

				uint32 FaceCount() const {
					return (uint32)faces.Size();
				}

				// Construit depuis un maillage indexé (triangles). Si quadify=true, fusionne
				// les paires de triangles coplanaires partageant une arête en QUADS (n-gons).
				void BuildFromIndexed(const NkVertex3D *v, uint32 vc, const uint32 *idx, uint32 ic, bool quadify);

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
				void ToPolygons(NkVector<NkVertex3D> &outVerts, NkVector<uint32> &outFaceStart,
								NkVector<uint32> &outFaceVerts) const;
				// (Re)construit le half-edge depuis des n-gons (même format CSR).
				void BuildFromPolygons(const NkVertex3D *v, uint32 vc, const uint32 *faceStart, uint32 faceCount,
									   const uint32 *faceVerts);

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
				uint32 FaceSize(NkEmId f) const; // nombre de sommets du bord
				// Une face est SÉLECTIONNÉE si TOUS ses sommets le sont (convention Blender).
				bool FaceIsSelected(NkEmId f) const;
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
				void Quadify(float32 coplanarDot = 0.985f);

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
				void SelectNone();
				bool AnyVertSelected() const;

				// EXTRUDE façon Blender : la nouvelle géométrie est créée À L'OFFSET DEMANDÉ
				// (0 par défaut = collée sur l'originale) et devient la SÉLECTION. Aucun
				// déplacement implicite : c'est l'utilisateur qui bouge ensuite.
				bool ExtrudeSelectedFaces(const NkExtrudeParams &p = NkExtrudeParams{});
				// Sommet sélectionné -> nouveau sommet + ARÊTE reliante (arête « fil », face
				// dégénérée à 2 sommets : pas de surface, mais une vraie arête éditable).
				bool ExtrudeSelectedVertices(const NkExtrudeParams &p = NkExtrudeParams{});
				// Arête sélectionnée -> nouvelle arête + FACE (quad) reliante.
				bool ExtrudeSelectedEdges(const NkExtrudeParams &p = NkExtrudeParams{});
				bool DeleteSelectedFaces();
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
					const uint32 s0 = edges[e].radialStart;
					for (uint32 k = 0; k < 2; ++k) {
						if (s0 + k >= (uint32)radialPool.Size())
							break;
						if (radialPool[s0 + k] != h)
							return radialPool[s0 + k];
					}
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
				bool BevelSelected(const NkBevelParams &p = NkBevelParams{});

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
				bool SpinSelected(const NkSpinParams &p, const NkMat4f &localToSpin = NkMat4f::Identity());

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
				bool DissolveSelected(const NkDissolveParams &p = NkDissolveParams{});
				// planePoint / planeNormal sont exprimés dans l'espace de `localToPlaneSpace`
				// (= matrice modèle→monde côté éditeur ; identité pour une op locale pure IA).
				bool BisectByPlane(const NkVec3f &planePoint, const NkVec3f &planeNormal,
								   const NkMat4f &localToPlaneSpace);

			private:
				// Lie les jumeaux (twin) via une table de hachage sur (min,max) des sommets.
				void LinkTwins();
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
			ShrinkFatten
		};

		struct NkMeshEditCommand {
				NkMeshEditOp op = NkMeshEditOp::None;
				NkVector<uint32> selection;			  // sommets sélectionnés à l'application
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

				// Rejoue toutes les commandes (dans l'ordre) sur `mesh`. Renvoie le nb appliquées.
				uint32 ReplayOnto(NkEditMesh &mesh) const;
				// Sérialisation binaire autonome (magic "NMEC", versionnée) — persiste une
				// session sur disque : données d'imitation IA + modificateurs sauvegardables.
				void Serialize(NkVector<uint8> &out) const;
				bool Deserialize(const uint8 *data, uint32 size);

			private:
				NkVector<NkMeshEditCommand> mCommands;
		};

		// ── STACK DE MODIFICATEURS (non-destructif, façon Blender) ──────────────────
		// Un modificateur = transformation PARAMÉTRIQUE d'un maillage (Mirror/Array/Subsurf).
		// La pile s'évalue sur un maillage de BASE (jamais modifié) -> maillage affiché :
		//   base → mod0(params) → mod1(params) → … → résultat.
		// Changer un paramètre = ré-évaluer la pile (la base reste éditable dessous).
		// Fondation directe : ces modificateurs sont aussi des ACTIONS composables pour l'IA.
		enum class NkModifierType : uint8 { Mirror = 0, Array = 1, Subsurf = 2 };

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
