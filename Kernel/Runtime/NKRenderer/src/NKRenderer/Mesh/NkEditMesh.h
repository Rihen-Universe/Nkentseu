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
				bool individual = false;
				float32 offset = 0.f;
		};

		struct NkMergeParams {
				enum Mode { Center = 0, First = 1, Last = 2 };

				int32 mode = Center;
		};

		struct NkSubdivideParams {
				int32 cuts = 1;
		}; // faces sélectionnées, ou TOUT si rien n'est sélectionné

		// LOOP CUT : nombre de boucles insérées dans l'anneau de quads (façon Blender,
		// molette / touches). cuts=1 => une boucle au milieu.
		struct NkLoopCutParams {
				int32 cuts = 1;
		};

		class NkEditMesh {
			public:
				struct Vert {
						NkVec3f pos = {0.f, 0.f, 0.f};
						NkVec3f normal = {0.f, 1.f, 0.f};
						NkVec2f uv = {0.f, 0.f};
						NkEmId hedge = NK_EM_INVALID; // une demi-arête SORTANTE
						uint8 sel = 0;
				};

				struct Hedge {
						NkEmId origin = NK_EM_INVALID; // sommet d'origine
						NkEmId twin = NK_EM_INVALID;   // demi-arête opposée (autre face)
						NkEmId next = NK_EM_INVALID;   // suivante autour de la face
						NkEmId face = NK_EM_INVALID;   // face incidente
						uint8 alive = 1;			   // 0 = arête interne dissoute (quadify)
				};

				struct Face {
						NkEmId hedge = NK_EM_INVALID; // une demi-arête du bord (boucle via next)
						NkVec3f normal = {0.f, 1.f, 0.f};
						uint8 sel = 0;
						uint8 alive = 1; // 0 = supprimée (compactée plus tard)
				};

				NkVector<Vert> verts;
				NkVector<Hedge> hedges;
				NkVector<Face> faces;

				void Clear() {
					verts.Clear();
					hedges.Clear();
					faces.Clear();
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
				void Triangulate(NkVector<NkVertex3D> &outV, NkVector<uint32> &outIdx,
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

				// Fusionne les paires de triangles CONSÉCUTIFS (2k,2k+1) adjacents et
				// coplanaires en QUADS. Adapté aux meshes triangulés quad-par-quad
				// (primitives, grilles). coplanarDot ~0.9995 (cube) à 0.98 (sphère fine).
				void Quadify(float32 coplanarDot = 0.985f);

				// Normales par face (produit vectoriel) puis par sommet (moyenne).
				void RecomputeNormals();

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
				bool MergeSelectedVerts(const NkMergeParams &p = NkMergeParams{});
				bool MakeFaceFromSelected();
				bool SubdivideSelectedFaces(const NkSubdivideParams &p = NkSubdivideParams{});
				bool LoopCutFromSelectedEdge(const NkLoopCutParams &p = NkLoopCutParams{});
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
			ExtrudeEdges
		};

		struct NkMeshEditCommand {
				NkMeshEditOp op = NkMeshEditOp::None;
				NkVector<uint32> selection;			  // sommets sélectionnés à l'application
				NkExtrudeParams extrude;			  // (op == Extrude / ExtrudeVerts / ExtrudeEdges)
				NkMergeParams merge;				  // (op == Merge)
				NkSubdivideParams subdiv;			  // (op == Subdivide)
				NkLoopCutParams loopcut;			  // (op == LoopCut) nombre de coupes
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

		struct NkMeshModifier {
				NkModifierType type = NkModifierType::Mirror;
				bool enabled = true;
				// Mirror : miroir sur un axe au plan de l'origine (+ soudure des sommets sur le plan).
				int32 mirrorAxis = 0; // 0=X 1=Y 2=Z
				bool mirrorMerge = true;
				float32 mirrorMergeDist = 1e-3f;
				// Array : duplique le maillage `arrayCount` fois avec un décalage constant.
				int32 arrayCount = 3;
				NkVec3f arrayOffset = {2.f, 0.f, 0.f};
				// Subsurf : subdivise TOUT le maillage `subsurfLevels` fois (réutilise Subdivide).
				int32 subsurfLevels = 1;

				void Apply(NkEditMesh &m) const; // transforme `m` en place
		};

		class NkModifierStack {
			public:
				NkVector<NkMeshModifier> modifiers;

				bool Empty() const {
					return modifiers.Empty();
				}

				void Clear() {
					modifiers.Clear();
				}

				void Add(const NkMeshModifier &mod) {
					modifiers.PushBack(mod);
				}

				uint32 Count() const {
					return (uint32)modifiers.Size();
				}

				// out = base, puis chaque modificateur activé appliqué dans l'ordre.
				void Evaluate(const NkEditMesh &base, NkEditMesh &out) const;
		};

	} // namespace renderer
} // namespace nkentseu
