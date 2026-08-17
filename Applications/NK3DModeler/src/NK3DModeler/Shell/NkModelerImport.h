#pragma once
// -----------------------------------------------------------------------------
// @File    NkModelerImport.h
// @Brief   IMPORT D'UN FICHIER 3D : chargement par les chargeurs du moteur,
//          puis DECOMPOSITION -- les models distincts chacun dans leur fichier,
//          les sous-mesh d'un meme model dedans (specification de Rihen,
//          R40/R41 : la frontiere entre deux fichiers est le MODEL, declare
//          par le fichier ; la connexite ne sert jamais a l'import).
//
//          ETAT (17/08) : chaine COMPLETE -- chargement, decoupage par nom,
//          CREATION des noeuds (racine + un noeud maillage par sous-mesh,
//          positions MONDE, sommets locaux rebases), archivage + carte
//          navigateur par model. Les `.nkmesh` partent A LA SAUVEGARDE,
//          jamais ici (temoin negatif de la grille du point 4).
//
//          ECART DECLARE, pas cache : la GEOMETRIE importee suit la meme
//          dette que la geometrie editee (NkModelerScene.h, « ce qui n'est
//          pas encore sauvegarde ») -- un `.nkmesh` ecrit les noeuds, leurs
//          origines et leurs noms, pas encore les sommets. Dans LA SESSION,
//          l'editeur de model travaille sur l'ARCHIVE vivante : la geometrie
//          y est reelle.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NK3DModeler/Shell/NkModelerScreens.h" // etat + hote + NkBrowUniqueName/NkMarkDirty
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

		/// Le RADICAL d'un chemin (nom de fichier sans dossier ni extension) :
		/// c'est le nom de repli d'un model que le fichier n'a pas nomme.
		inline void NkImpStem(const char *path, char *out, uint32 cap) {
			int32 ls = 0, cut = 0;
			for (int32 i = 0; path[i]; ++i) {
				if (path[i] == '/' || path[i] == '\\')
					cut = i + 1;
				ls = i + 1;
			}
			int32 dot = ls;
			for (int32 i = ls - 1; i > cut; --i)
				if (path[i] == '.') {
					dot = i;
					break;
				}
			uint32 k = 0;
			for (int32 i = cut; i < dot && k + 1 < cap; ++i)
				out[k++] = path[i];
			out[k] = 0;
			if (!out[0])
				snprintf(out, cap, "Import");
		}

		/// Nomme un noeud DES DEUX COTES : l'application (customNames, ce que la
		/// hierarchie affiche et ce que la capture ecrit dans le fichier) et
		/// l'hote (le label qui nomme les fichiers produits par la sortie). La
		/// relecture d'un projet fait exactement ces deux gestes.
		inline void NkImpNodeName(NkModelerState &st, int32 node, const char *nm) {
			if (node >= 0 && node < 176)
				snprintf(st.customNames[node], sizeof(st.customNames[0]), "%s", nm);
			demo::Demo3DHostSetNodeLabel(node, nm);
		}

		/// LA CREATION (point 4 de l'eclatement) : chaque model de la
		/// decomposition devient racine (empty, model) + UN NOEUD MAILLAGE PAR
		/// SOUS-MESH dans la scene active, puis est ARCHIVE pour sa carte
		/// navigateur -- le MEME chemin que le glisser hierarchie -> navigateur.
		/// AUCUNE ecriture disque ici : les `.nkmesh` partent a la SAUVEGARDE
		/// (temoin negatif de la grille). La carte est marquee
		/// `browserOriginDirty` : son consommateur (NkProjectWriteAssets) dit
		/// « a ecrire au prochain enregistrement, meme partiel », et un model
		/// qui n'existe qu'en memoire est exactement cela.
		///
		/// TRANCHES -- mesure du 17/08, les deux chargeurs ne remplissent pas
		/// pareil : glTF ecrit des indices LOCAUX a la primitive et baseVertex
		/// porte le decalage (NkGLTFLoader.cpp:1069) ; OBJ ecrit des indices
		/// GLOBAUX et baseVertex=0 (NkOBJLoader.cpp:199). L'unique lecture
		/// juste pour les deux : global = indices[firstIndex + i] + baseVertex.
		/// On EXTRAIT les sommets utiles et on REBASE (0..n-1) : passer le
		/// buffer entier dupliquerait la geometrie de TOUT le fichier dans
		/// chaque model.
		///
		/// SOMMETS LOCAUX, POSITIONS MONDE : chaque noeud maillage nait a
		/// l'ANCRE de sa tranche (le centre de sa boite) et ses sommets sont
		/// rebases autour d'elle -- meme image a l'ecran, et l'origine est SUR
		/// la matiere (le gizmo aussi). La racine nait au barycentre X/Z de ses
		/// maillages, Y=0 (la hauteur porte la pose au sol -- regle du
		/// recentrage) : Demo3DHostRecenterModel retrouvera exactement cette
		/// moyenne a l'archivage, donc `MESURE origine` doit dire ECART=(0, 0)
		/// des la naissance. C'est le temoin.
		inline bool NkImportCreate(NkModelerState &st, const renderer::NkGLTFMeshData &data,
								   const NkVector<NkImportModel> &models, const char *stem) {
			// Un import cree des MODELS : dans un editeur de model, il n'a pas
			// de sens (un model ne contient pas de models). Refus NOMME.
			if (demo::Demo3DHostDocIsModel()) {
				snprintf(st.hierNote, sizeof(st.hierNote),
						 "Importer : ouvrez une SCENE (l'import cree des models)");
				return false;
			}
			const uint32 vTotal = (uint32)data.vertices.Size();
			const uint32 iTotal = (uint32)data.indices.Size();
			if (vTotal == 0 || iTotal == 0)
				return false;
			// Table globale -> local d'UNE tranche. Remise a -1 par liste des
			// entrees touchees (jamais un balayage de tout le buffer par
			// tranche).
			NkVector<int32> remap;
			remap.Resize((usize)vTotal, -1);
			int32 modelsNes = 0, noeudsNes = 0, cartes = 0;
			bool plein = false, navPlein = false;
			for (usize mi = 0; mi < models.Size() && !plein; ++mi) {
				const NkImportModel &mo = models[mi];
				const char *mnm = (mo.name && mo.name[0]) ? mo.name : stem;
				// ── passe A : l'ancre de chaque tranche, et la moyenne X/Z des
				// tranches NON VIDES (les seules qui feront un noeud -- c'est
				// sur les noeuds que le recentrage prendra sa moyenne).
				NkVector<float32> anc; // x,y,z par tranche (plat, pas de NkVec ici)
				NkVector<uint8> vive;
				float32 sax = 0.f, saz = 0.f;
				int32 nVives = 0;
				for (int32 s = 0; s < mo.subCount; ++s) {
					const renderer::NkSubMesh &sm = data.subMeshes[(uint32)(mo.firstSub + s)];
					float32 mn[3] = {1e30f, 1e30f, 1e30f}, mx[3] = {-1e30f, -1e30f, -1e30f};
					uint32 cnt = 0;
					for (uint32 i = 0; i < sm.indexCount && sm.firstIndex + i < iTotal; ++i) {
						uint32 gi = data.indices[sm.firstIndex + i] + sm.baseVertex;
						if (gi >= vTotal)
							gi = 0; // meme garde anti-debordement que le chargeur
						const auto &p = data.vertices[gi].pos;
						const float32 v3[3] = {p.x, p.y, p.z};
						for (int32 a = 0; a < 3; ++a) {
							if (v3[a] < mn[a])
								mn[a] = v3[a];
							if (v3[a] > mx[a])
								mx[a] = v3[a];
						}
						++cnt;
					}
					const bool ok = cnt > 0;
					vive.PushBack(ok ? 1 : 0);
					for (int32 a = 0; a < 3; ++a)
						anc.PushBack(ok ? (mn[a] + mx[a]) * 0.5f : 0.f);
					if (ok) {
						sax += anc[(usize)(s * 3 + 0)];
						saz += anc[(usize)(s * 3 + 2)];
						++nVives;
					}
				}
				if (nVives == 0) {
					NkLog::Instance().Warnf("[import] model « %s » : aucune geometrie, saute", mnm);
					continue;
				}
				// ── la racine, au barycentre X/Z de sa future matiere ──
				const float32 rpos[3] = {sax / (float32)nVives, 0.f, saz / (float32)nVives};
				const int32 root = demo::Demo3DHostCreateModelRoot(rpos);
				if (root < 0) {
					plein = true;
					break; // on garde ce qui a pu naitre, et on le DIT plus bas
				}
				NkImpNodeName(st, root, mnm);
				++modelsNes;
				// ── passe B : un noeud maillage par tranche vive ──
				NkVector<const char *> nomPiece; // pour renommer l'archive apres
				uint32 mVerts = 0, mIdx = 0;
				int32 pieces = 0;
				for (int32 s = 0; s < mo.subCount && !plein; ++s) {
					if (!vive[(usize)s])
						continue;
					const renderer::NkSubMesh &sm = data.subMeshes[(uint32)(mo.firstSub + s)];
					const float32 *a3 = &anc[(usize)(s * 3)];
					NkVector<renderer::NkVertex3D> lv;
					NkVector<uint32> li, touche;
					for (uint32 i = 0; i < sm.indexCount && sm.firstIndex + i < iTotal; ++i) {
						uint32 gi = data.indices[sm.firstIndex + i] + sm.baseVertex;
						if (gi >= vTotal)
							gi = 0;
						if (remap[gi] < 0) {
							remap[gi] = (int32)lv.Size();
							renderer::NkVertex3D v = data.vertices[gi];
							v.pos.x -= a3[0]; // LOCAL au noeud : l'ancre devient (0,0,0)
							v.pos.y -= a3[1];
							v.pos.z -= a3[2];
							lv.PushBack(v);
							touche.PushBack(gi);
						}
						li.PushBack((uint32)remap[gi]);
					}
					const char *snm = sm.name.CStr();
					if (!snm || !snm[0])
						snm = mnm;
					const int32 n = demo::Demo3DHostCreateMeshNode(
						root, lv.Data(), (uint32)lv.Size(), li.Data(), (uint32)li.Size(),
						a3, snm);
					for (usize t = 0; t < touche.Size(); ++t)
						remap[touche[t]] = -1; // la table redevient vierge pour la suivante
					if (n < 0) {
						plein = true;
						break;
					}
					NkImpNodeName(st, n, snm);
					nomPiece.PushBack(snm);
					mVerts += (uint32)lv.Size();
					mIdx += (uint32)li.Size();
					++pieces;
					++noeudsNes;
				}
				// ── archive + carte navigateur : le MEME chemin que le glisser
				// hierarchie -> navigateur (NkModelerHierarchy.h). L'archivage
				// recentre l'archive (Demo3DHostRecenterModel y est appele) :
				// la racine etant nee a la moyenne exacte, la ligne `MESURE
				// origine` de ce depot DOIT dire ECART=(0, 0). Chaque ligne de
				// la grille peut me donner tort.
				int32 arc = -1, k6 = -1;
				if (st.browserCount < NkModelerState::kMaxBrowser) {
					arc = demo::Demo3DHostArchiveNode(root);
					k6 = st.browserCount++;
					st.browserKind[k6] = 6;
					st.browserParent[k6] = st.browserFolder;
					st.browserSub[k6] = 0;
					st.browserDoc[k6] = 0;
					st.browserMat[k6] = 0;
					st.browserFile[k6][0] = 0;
					st.browserSrcNode[k6] = (arc >= 0 ? arc : root) + 1;
					NkBrowUniqueName(st, 6, st.browserFolder, mnm, st.browserNames[k6],
									 (uint32)sizeof(st.browserNames[0]));
					// A ECRIRE AU PROCHAIN ENREGISTREMENT, meme partiel : le
					// consommateur du drapeau (NkProjectWriteAssets) dit
					// exactement cela, et cette carte n'a pas encore de fichier.
					st.browserOriginDirty[k6] = true;
					++cartes;
					// ── les NOMS de l'archive. HostDuplicateTree parcourt les
					// sources par slot croissant et HostAllocUser prend toujours
					// le PLUS PETIT slot libre d'un ensemble qui ne fait que
					// retrecir pendant la boucle : les maillages du double
					// naissent donc en slots croissants, dans l'ordre des
					// sources -- k-ieme (croissant) chez l'un = k-ieme chez
					// l'autre. Sans ces noms, le fichier de model ecrirait des
					// noeuds anonymes et la reouverture montrerait « Cube.NNN ».
					if (arc >= 0) {
						NkImpNodeName(st, arc, mnm);
						int32 k = 0;
						const int32 nodeMax = demo::Demo3DHostNodeCount();
						for (int32 c = 0; c < nodeMax && k < (int32)nomPiece.Size(); ++c)
							if (demo::Demo3DHostNodeInnerMeshOf(c, arc))
								NkImpNodeName(st, c, nomPiece[(usize)k++]);
					}
				} else {
					navPlein = true;
				}
				NkLog::Instance().Infof(
					"[import] MESURE creation : model « %s » root=%d maillages=%d/%d "
					"verts=%u indices=%u racine=(%f, 0, %f) arc=%d carte=%d",
					mnm, root, pieces, mo.subCount, mVerts, mIdx, rpos[0], rpos[2], arc, k6);
			}
			// Le projet a change (noeuds + cartes) : il le sait.
			if (modelsNes > 0)
				NkMarkDirty(st);
			if (plein)
				snprintf(st.hierNote, sizeof(st.hierNote),
						 "Import INCOMPLET (%d model(s), %d maillage(s)) : plus d'emplacement",
						 modelsNes, noeudsNes);
			else if (navPlein)
				snprintf(st.hierNote, sizeof(st.hierNote),
						 "Import : %d model(s), %d maillage(s) - navigateur PLEIN, cartes partielles",
						 modelsNes, noeudsNes);
			else
				snprintf(st.hierNote, sizeof(st.hierNote),
						 "Import : %d model(s), %d maillage(s), %d carte(s) - .nkmesh a la sauvegarde",
						 modelsNes, noeudsNes, cartes);
			return modelsNes > 0;
		}

		/// L'IMPORT COMPLET : charge, decoupe, journalise (`MESURE import`),
		/// puis CREE les noeuds. Tout refus est NOMME dans `st.hierNote` -- un
		/// refus silencieux serait indistinguable d'un bouton casse.
		inline bool NkImportFile(NkModelerState &st, const char *absPath) {
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
			char stem[32];
			NkImpStem(absPath, stem, (uint32)sizeof(stem));
			return NkImportCreate(st, data, models, stem);
		}

	} // namespace nk3d
} // namespace nkentseu
