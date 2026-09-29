#pragma once
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Applications/NK3DModeler/src/NK3DModeler/Shell/NkModelerVertexColor.h
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// LES COULEURS PAR SOMMET, ET POURQUOI UN MATERIAU POSE NE SE VOYAIT PAS (Q15).
//
// Rodolf, 22/09 : « les maillages obtenus de Tripo ne sont pas textures, et ce
// qui est genant c'est qu'ils sont bizarres : on ne peut pas modifier le
// materiau facilement -- ca a un effet que je ne comprends pas, contrairement a
// l'assemblage. »
//
// ⚠️ LA CAUSE EST MESUREE, DE BOUT EN BOUT, ET NON SUPPOSEE :
//   1. le .glb de TripoSR porte POSITION, NORMAL et **COLOR_0**, et n'a ni
//      TEXCOORD_0, ni materiau, ni image (releve sur un fichier reel : 51 476
//      sommets, 102 940 triangles, `materials: null`, `textures: None`) ;
//   2. `NkGLTFLoader.cpp` range COLOR_0 dans `NkVertex3D::color`, et met du
//      BLANC quand l'attribut manque -- d'ou la difference avec l'assemblage,
//      dont les sommets sont blancs ;
//   3. le nuanceur de sommets PBR fait `vColor = aColor * uObj.tint`, et le
//      fragment `albSample = texture(tAlbedo, vUV) * vColor`. La couleur du
//      materiau est donc **MULTIPLIEE** par celle du sommet, jamais posee a sa
//      place. Sur un maillage colore, poser du gris assombrit, poser du rouge
//      ne garde que le canal rouge de ce qui etait deja la.
//
// CE QUE FAIT CE FICHIER, ET CE QU'IL NE FAIT PAS.
// Il neutralise les couleurs de sommet D'UN NOEUD, en gardant les originales,
// et les rend. Rien d'autre. ⚠️ IL NE TOUCHE PAS AU NUANCEUR, et c'est un choix :
// ajouter un uniforme `useVertexColor` demanderait le meme correctif dans les
// six dorsaux (NkSL, GL, VK, DX11, DX12, Metal) et dans le bloc d'uniformes ;
// une seule divergence, et l'objet serait gris ici et colore la. Reecrire le
// champ `color` de la copie CPU agit sur les six d'un coup, parce qu'il n'y a
// rien a accorder.
// Le cout est une reecriture de tampon par bascule -- un geste rare, decide par
// l'utilisateur -- contre un risque de parite permanent.
// -----------------------------------------------------------------------------
#include "NK3DModeler/Viewport/NkDemo3DHost.h"
#include "NKContainers/Sequential/NkVector.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace nk3d {

		/// Le pas d'un sommet et l'emplacement de sa couleur sont LUS A L'HOTE, jamais
		/// devines : `Demo3DHostVertexBytes()` est deja la verite de l'ecriture des
		/// fichiers. La couleur est le DERNIER champ de `NkVertex3D` (uint32), donc a
		/// `pas - 4`. ⚠️ Si un jour un champ est ajoute APRES elle, ce calcul ment ; la
		/// garde ci-dessous le dit au lieu de peindre n'importe quoi.
		inline uint32 NkVcOffsetCouleur(uint32 pas) {
			return pas >= 8u ? pas - 4u : 0u;
		}

		struct NkVcEntree {
				int32 noeud = -1;
				bool actif = true;   ///< les couleurs du maillage sont-elles en service ?
				NkVector<uint32> origine; ///< ce que le fichier portait, jamais perdu
		};

		inline NkVector<NkVcEntree> &NkVcTable() {
			static NkVector<NkVcEntree> t;
			return t;
		}

		inline NkVcEntree *NkVcTrouver(int32 noeud) {
			NkVector<NkVcEntree> &t = NkVcTable();
			for (usize i = 0; i < t.Size(); ++i)
				if (t[i].noeud == noeud)
					return &t[i];
			return nullptr;
		}

		/// Le noeud porte-t-il des couleurs de sommet NON BLANCHES ? Si oui, il entre
		/// dans la table avec ses couleurs d'origine. Rend le nombre de sommets
		/// colores (0 = rien a faire, et c'est le cas de tout l'assemblage).
		inline int32 NkVcDetecter(int32 noeud) {
			if (NkVcTrouver(noeud))
				return -1; // deja connu
			const void *v = nullptr;
			const uint32 *idx = nullptr;
			uint32 nv = 0, ni = 0;
			if (!demo::Demo3DHostMeshData(noeud, &v, &nv, &idx, &ni) || !v || !nv)
				return 0;
			const uint32 pas = demo::Demo3DHostVertexBytes();
			const uint32 off = NkVcOffsetCouleur(pas);
			if (!off)
				return 0;
			const uint8 *b = (const uint8 *)v;
			int32 colores = 0;
			for (uint32 i = 0; i < nv; ++i) {
				uint32 c = 0;
				memcpy(&c, b + (usize)i * pas + off, 4);
				if ((c & 0x00FFFFFFu) != 0x00FFFFFFu)
					++colores;
			}
			if (!colores)
				return 0;
			NkVcEntree e;
			e.noeud = noeud;
			e.actif = true; // ALLUME A L'IMPORT : on montre ce que le fichier porte
			e.origine.Resize(nv);
			for (uint32 i = 0; i < nv; ++i)
				memcpy(&e.origine[(usize)i], b + (usize)i * pas + off, 4);
			NkVcTable().PushBack(e);
			return colores;
		}

		inline bool NkVcPresent(int32 noeud) {
			return NkVcTrouver(noeud) != nullptr;
		}

		inline bool NkVcActif(int32 noeud) {
			const NkVcEntree *e = NkVcTrouver(noeud);
			return e ? e->actif : false;
		}

		/// Pose l'interrupteur et REECRIT les sommets. Rend vrai si quelque chose a
		/// change. ⚠️ On repasse par `Demo3DHostSetMeshData` -- la meme porte que
		/// l'import et que la relecture -- plutot que d'ecrire dans le tampon rendu
		/// par `MeshData` : celui-la n'est valide que jusqu'a la prochaine operation,
		/// et une ecriture dedans serait vraie a l'oeil et perdue au premier geste.
		inline bool NkVcRegler(int32 noeud, bool actif) {
			NkVcEntree *e = NkVcTrouver(noeud);
			if (!e || e->actif == actif)
				return false;
			const void *v = nullptr;
			const uint32 *idx = nullptr;
			uint32 nv = 0, ni = 0;
			if (!demo::Demo3DHostMeshData(noeud, &v, &nv, &idx, &ni) || !v || !nv)
				return false;
			const uint32 pas = demo::Demo3DHostVertexBytes();
			const uint32 off = NkVcOffsetCouleur(pas);
			if (!off || nv != (uint32)e->origine.Size())
				return false;
			NkVector<uint8> tampon;
			tampon.Resize((usize)nv * pas);
			memcpy(tampon.Data(), v, (usize)nv * pas);
			NkVector<uint32> ind;
			ind.Resize(ni);
			if (ni)
				memcpy(ind.Data(), idx, (usize)ni * sizeof(uint32));
			for (uint32 i = 0; i < nv; ++i) {
				uint32 c = 0xFFFFFFFFu;
				if (actif)
					c = e->origine[(usize)i];
				else {
					// LE BLANC GARDE L'ALPHA D'ORIGINE : le fragment jette le pixel
					// sous 0,01 d'alpha, et ecraser l'alpha a 1 rendrait visible ce
					// que le fichier voulait transparent.
					const uint32 a = e->origine[(usize)i] & 0xFF000000u;
					c = 0x00FFFFFFu | a;
				}
				memcpy(tampon.Data() + (usize)i * pas + off, &c, 4);
			}
			const bool ok = demo::Demo3DHostSetMeshData(noeud, tampon.Data(), nv, ind.Data(), ni);
			if (ok)
				e->actif = actif;
			return ok;
		}

		/// UN MATERIAU POSE GAGNE (Q15.1). Appelee quand l'utilisateur pose une
		/// couleur sur un noeud : si ce noeud porte des couleurs de sommet, elles
		/// s'eteignent -- sinon la couleur choisie serait MULTIPLIEE par elles et
		/// l'utilisateur verrait un resultat qu'aucun reglage n'explique. Rend vrai
		/// quand l'extinction a eu lieu, pour que l'appelant le DISE.
		inline bool NkVcMateriauGagne(int32 noeud) {
			return NkVcActif(noeud) && NkVcRegler(noeud, false);
		}

	} // namespace nk3d
} // namespace nkentseu
