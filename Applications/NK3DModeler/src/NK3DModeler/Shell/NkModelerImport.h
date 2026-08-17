#pragma once
// -----------------------------------------------------------------------------
// @File    NkModelerImport.h
// @Brief   IMPORT D'UN FICHIER 3D : chargement par les chargeurs du moteur,
//          puis DECOMPOSITION -- les models distincts chacun dans leur fichier,
//          les sous-mesh d'un meme model dedans (specification de Rihen,
//          R40/R41 : la frontiere entre deux fichiers est le MODEL, declare
//          par le fichier ; la connexite ne sert jamais a l'import).
//
//          ETAT : chargement + ANALYSE livres (le decoupage par nom est mesure
//          et journalise). La CREATION des noeuds vient ensuite -- elle attend
//          la lecture de la semantique de transform des noeuds maillage
//          (EnsureModelMesh pose (0,0,0) « locale neutre » dans un systeme
//          etabli en positions MONDE : le consommateur doit etre lu avant
//          d'ecrire, c'est le piege nkvpEmptyPos du corpus).
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NK3DModeler/Shell/NkModelerInput.h"
#include "NKRenderer/Mesh/NkOBJLoader.h"
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKRenderer/Mesh/NkFBXLoader.h"
#include "NKRenderer/Mesh/NkDAELoader.h"
#include "NKRenderer/Mesh/NkPLYLoader.h"
#include "NKRenderer/Mesh/NkSTLLoader.h"
#include "NKRenderer/Mesh/NkUSDALoader.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace nk3d {

		// Suffixe insensible a la casse -- le picker du kit n'a qu'un filtre
		// MONO-extension (EndsWithI sur pickerFileExt), il ne sait pas dire
		// « les sept formats 3D ». On ouvre donc sans filtre et on valide ICI,
		// a la confirmation, avec un refus NOMME (regle du depot : un refus
		// silencieux est indistinguable d'un bouton casse).
		inline bool NkImpEndsWithI(const char *s, const char *suf) {
			int32 ls = 0, lu = 0;
			while (s[ls])
				++ls;
			while (suf[lu])
				++lu;
			if (lu > ls)
				return false;
			for (int32 i = 0; i < lu; ++i) {
				char a = s[ls - lu + i], b = suf[i];
				if (a >= 'A' && a <= 'Z')
					a = (char)(a - 'A' + 'a');
				if (b >= 'A' && b <= 'Z')
					b = (char)(b - 'A' + 'a');
				if (a != b)
					return false;
			}
			return true;
		}

		/// Charge `path` par LE chargeur que son extension designe. Rend faux si
		/// l'extension n'est d'aucun des sept formats, ou si le chargeur echoue --
		/// et dit lequel des deux dans `why` (l'appelant l'affiche, il ne devine
		/// pas).
		inline bool NkImportLoad(const char *path, renderer::NkGLTFMeshData &out,
								 const char **why) {
			*why = nullptr;
			bool ok = false, connu = true;
			if (NkImpEndsWithI(path, ".obj"))
				ok = renderer::LoadOBJ(NkString(path), out);
			else if (NkImpEndsWithI(path, ".gltf") || NkImpEndsWithI(path, ".glb"))
				ok = renderer::LoadGLTF(NkString(path), out);
			else if (NkImpEndsWithI(path, ".fbx"))
				ok = renderer::LoadFBX(NkString(path), out);
			else if (NkImpEndsWithI(path, ".dae"))
				ok = renderer::LoadDAE(NkString(path), out);
			else if (NkImpEndsWithI(path, ".ply"))
				ok = renderer::LoadPLY(NkString(path), out);
			else if (NkImpEndsWithI(path, ".stl"))
				ok = renderer::LoadSTL(NkString(path), out);
			else if (NkImpEndsWithI(path, ".usda"))
				ok = renderer::LoadUSDA(NkString(path), out);
			else
				connu = false;
			if (!connu)
				*why = "Format non reconnu : .obj .gltf .glb .fbx .dae .ply .stl .usda";
			else if (!ok)
				*why = "Le fichier n'a pas pu etre lu (voir le journal)";
			return connu && ok;
		}

		/// Un MODEL de la decomposition : une plage CONTIGUE de sous-mesh qui
		/// partagent le meme nom. La frontiere est DECLAREE par le fichier
		/// (`o` en OBJ, le noeud en glTF/FBX) -- rien n'est calcule, la
		/// connexite ne sert jamais ici (R40 : decouper par connexite
		/// eclaterait un model que l'artiste voulait entier).
		struct NkImportModel {
				int32 firstSub = 0; // index du premier sous-mesh dans data.subMeshes
				int32 subCount = 0;
				const char *name = ""; // pointe dans data.subMeshes[].name -- ne survit pas a data
		};

		/// Decoupe `data` en models par nom de sous-mesh CONTIGU. Un fichier sans
		/// aucun nom rend UN model (repli decide en R41 : les exports reels
		/// portent des marqueurs ; un fichier qui n'en a pas est un defaut de
		/// fichier, pas une decision de produit).
		inline int32 NkImportSplitByName(const renderer::NkGLTFMeshData &data,
										 NkVector<NkImportModel> &out) {
			out.Clear();
			const int32 n = (int32)data.subMeshes.Size();
			for (int32 s = 0; s < n; ++s) {
				const char *nm = data.subMeshes[(uint32)s].name.CStr();
				if (!out.Empty()) {
					NkImportModel &last = out[(uint32)out.Size() - 1];
					// MEME NOM -> meme model. La comparaison est stricte : deux
					// « o » homonymes NON contigus font deux models, comme dans
					// le fichier -- on ne fusionne pas ce que l'auteur a separe.
					if (NkString(last.name) == NkString(nm)) {
						last.subCount++;
						continue;
					}
				}
				NkImportModel m;
				m.firstSub = s;
				m.subCount = 1;
				m.name = nm;
				out.PushBack(m);
			}
			return (int32)out.Size();
		}

		/// L'ANALYSE : charge, decoupe, journalise, et resume dans `st.hierNote`.
		/// C'est la moitie LECTURE du plan d'eclatement -- elle prouve la chaine
		/// bouton -> picker -> chargeur -> decoupage sur un fichier reel, et sa
		/// trace est ce qu'on confrontera a la creation des noeuds.
		inline bool NkImportAnalyze(NkModelerState &st, const char *absPath) {
			renderer::NkGLTFMeshData data;
			const char *why = nullptr;
			if (!NkImportLoad(absPath, data, &why)) {
				snprintf(st.hierNote, sizeof(st.hierNote), "%s", why);
				NkLog::Instance().Warnf("[import] '%s' : %s", absPath, why);
				return false;
			}
			NkVector<NkImportModel> models;
			const int32 nm = NkImportSplitByName(data, models);
			NkLog::Instance().Infof("[import] MESURE import : '%s' -> %d model(s), "
									"%u sous-mesh, %u verts, %u indices",
									absPath, nm, (uint32)data.subMeshes.Size(),
									(uint32)data.vertices.Size(), (uint32)data.indices.Size());
			for (int32 i = 0; i < nm; ++i)
				NkLog::Instance().Infof("[import]   model %d : « %s », %d sous-mesh",
										i, models[(uint32)i].name[0] ? models[(uint32)i].name
																	 : "(sans nom)",
										models[(uint32)i].subCount);
			// LE RESUME EST VISIBLE, PAS SEULEMENT JOURNALISE : la creation des
			// noeuds n'existant pas encore, un import qui ne dirait rien serait
			// un second bouton mort. Le message dit CE QUI EST FAIT et ce qui ne
			// l'est pas.
			snprintf(st.hierNote, sizeof(st.hierNote),
					 "Import analyse : %d model(s), %u sous-mesh - creation des noeuds a venir",
					 nm, (uint32)data.subMeshes.Size());
			return true;
		}

	} // namespace nk3d
} // namespace nkentseu
