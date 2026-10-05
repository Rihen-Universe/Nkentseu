#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Skin/ — la PEAU : le maillage qu'un squelette deforme, ses poids (automatiques et peints).
// -----------------------------------------------------------------------------
// FICHIER: NKAnima/Skin/NkSkinMesh.h
// DESCRIPTION: le maillage de la peau, vu par les calculs du rig : positions de
//   repos, triangles, et ce qu'on en DERIVE une fois (sommets soudes, voisins,
//   miroir X, normales, dedans/dehors).
//
// ⚠️ LES SOMMETS SONT SOUDES AVANT TOUT CALCUL DE VOISINAGE. Un maillage glTF
//    DOUBLE ses sommets le long des coutures d'UV (meme position, deux indices).
//    Sans soudure, la diffusion de chaleur s'arrete net a chaque couture -- deux
//    moities de bras recevraient des poids differents. Chaque sommet a donc un
//    REPRESENTANT (`rep`) ; les calculs se font sur les representants et se
//    recopient sur leurs doubles.
//
// Aucun format, aucun GPU : NKAnima ne connait que Foundation.
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace anim {

		struct NkSkinMesh {
				NkVector<math::NkVec3f> positions; ///< repos, espace modele
				NkVector<math::NkVec3f> normals;   ///< (ComputeNormals)
				NkVector<uint32> indices;		   ///< triangles

				// ── Derive (BuildTopology) ─────────────────────────────────────────
				NkVector<uint32> rep;	   ///< sommet -> indice (dans `soudes`) de son representant
				NkVector<uint32> soudes;   ///< un sommet par position distincte
				NkVector<uint32> adjDebut; ///< voisins (CSR) : adj[adjDebut[r] .. adjDebut[r+1])
				NkVector<uint32> adj;	   ///< indices dans `soudes`
				NkVector<int32> mirror;	   ///< sommet -> son miroir X (-1 : aucun) (BuildMirror)
				math::NkVec3f bmin{0.f, 0.f, 0.f}, bmax{0.f, 0.f, 0.f};

				uint32 VertexCount() const {
					return (uint32)positions.Size();
				}
				uint32 TriangleCount() const {
					return (uint32)indices.Size() / 3u;
				}
				float32 Diagonal() const;
				/// Soude, construit les voisins et la boite. A rappeler si la topologie change.
				void BuildTopology();
				/// Le miroir X de chaque sommet (plan x = 0), a `tol` pres (0 : 0,2 % de la diagonale).
				/// Rend le nombre de sommets apparies.
				uint32 BuildMirror(float32 tol = 0.f);
				/// Normales lissees (ponderees par l'aire), communes aux sommets soudes.
				void ComputeNormals();
				static void ComputeNormals(const math::NkVec3f *pos, uint32 n, const uint32 *idx, uint32 nIdx,
										   const NkVector<uint32> *rep, NkVector<math::NkVec3f> &out);
				/// Le point est-il DANS le volume ferme ? Parite de rayons sur trois axes
				/// (vote a la majorite : un rayon qui frole une arete ne decide pas seul).
				bool Contains(const math::NkVec3f &p) const;
				/// (05/10) LE MILIEU DU VOLUME sous un rayon `o + t d` (le rigging a la
				/// main : un os se pose AU CENTRE du bras, pas sur sa peau). Le rayon entre
				/// au premier impact et sort au suivant ; `out` = le milieu des deux. Un
				/// seul impact (maillage ouvert) : ce point. Faux si le rayon manque tout.
				/// `epaisseur` (optionnel) : la distance entre l'entree et la sortie.
				bool RayVolumeMiddle(const math::NkVec3f &o, const math::NkVec3f &d, math::NkVec3f &out, float32 *epaisseur = nullptr) const;
		};

	} // namespace anim
} // namespace nkentseu
