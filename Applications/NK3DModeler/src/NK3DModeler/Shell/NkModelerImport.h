#pragma once
// -----------------------------------------------------------------------------
// @File    NkModelerImport.h
// @Brief   IMPORT D'UN FICHIER 3D : chargement par les chargeurs du moteur,
//          puis DECOMPOSITION -- les models distincts chacun dans leur fichier,
//          les sous-mesh d'un meme model dedans (specification de Rihen,
//          R40/R41 : la frontiere entre deux fichiers est le MODEL, declare
//          par le fichier ; la connexite ne sert jamais a l'import).
//
//          ETAT (17/08 soir, contrat d'import de Rodolf apres la 10e
//          relecture) : chaine COMPLETE -- chargement, decoupage par nom,
//          CREATION des noeuds (positions MONDE, sommets locaux rebases),
//          ARCHIVAGE EN PLACE (rien n'entre dans la scene) et ECRITURE des
//          `.nkmesh` sur le disque du projet, un par model : l'import EST le
//          geste d'ecriture (contrat point 1). Un model d'UNE tranche est un
//          noeud maillage DIRECT a sa propre origine -- aucun empty fabrique
//          si le fichier n'en declare pas (contrat point 5, defaut vu par
//          Rodolf sur les roues).
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
#include "NK3DModeler/Project/NkModelerAssets.h" // NkProjectWriteCard : l'UNIQUE ecrivain de carte
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

		/// LA CREATION (point 4 de l'eclatement, contrat du 17/08 soir) : chaque
		/// model de la decomposition devient des NOEUDS -- puis est ARCHIVE EN
		/// PLACE (invisible, hors hierarchie : rien n'entre dans la scene) et
		/// ECRIT sur le disque du projet (`.nkmesh`, un par model) par
		/// NkProjectWriteCard, l'unique ecrivain de carte. L'import EST le geste
		/// d'ecriture (contrat point 1) ; la scene ne recoit un model que par un
		/// glisser depuis le navigateur ou depuis le systeme.
		///
		/// UN MODEL D'UNE TRANCHE EST UN MAILLAGE DIRECT (contrat point 5) :
		/// pas de racine, le noeud maillage nait A SON ANCRE (le centre de sa
		/// boite -- pour une roue, le moyeu) et c'est LUI la carte. Rodolf a vu
		/// « le empty de chaque roue comme maillage de la roue » : le fichier
		/// (5 `o`, aucun groupe) ne declare aucun conteneur, on n'en fabrique
		/// aucun. Un model de PLUSIEURS tranches (plusieurs sous-mesh sous le
		/// meme nom : les primitives d'un node glTF, les groupes/materiaux d'un
		/// `o` OBJ) garde une racine EMPTY + un maillage par tranche : ici le
		/// fichier declare bien un regroupement, et un noeud de ce systeme ne
		/// porte qu'UN materiau (dette dite : pas de maillage multi-materiaux).
		/// Le dialogue d'import (contrat point 3) offrira « regrouper » /
		/// « eclater » ; ceci en est le defaut.
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
		/// l'ANCRE de sa tranche et ses sommets sont rebases autour d'elle --
		/// meme image a l'ecran, et l'origine est SUR la matiere (le gizmo
		/// aussi). Une racine (cas multi-tranches) nait au barycentre X/Z de
		/// ses maillages, Y=0 -- la moyenne exacte que Demo3DHostRecenterModel
		/// recalcule, donc `MESURE origine` doit dire ECART=(0, 0).
		inline bool NkImportCreate(NkModelerState &st, const renderer::NkGLTFMeshData &data,
								   const NkVector<NkImportModel> &models, const char *stem,
								   NkVector<int32> *cardsOut = nullptr) {
			// Un import cree des MODELS : dans un editeur de model, il n'a pas
			// de sens (un model ne contient pas de models). Refus NOMME.
			if (demo::Demo3DHostDocIsModel()) {
				snprintf(st.hierNote, sizeof(st.hierNote),
						 "Importer : ouvrez une SCENE (l'import cree des models)");
				return false;
			}
			// Un import ECRIT dans le projet : sans projet ouvert, il n'a nulle
			// part ou ecrire, et le dire vaut mieux qu'ecrire dans le vide.
			if (st.projectRoot.Empty()) {
				snprintf(st.hierNote, sizeof(st.hierNote),
						 "Importer : ouvrez un PROJET (l'import ecrit des .nkmesh dedans)");
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
			int32 modelsNes = 0, noeudsNes = 0, cartes = 0, fichiers = 0;
			bool plein = false, navPlein = false;
			NkString errEcr;
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
				// ── la racine, SEULEMENT si le fichier regroupe plusieurs tranches
				// sous ce nom. Une tranche = un maillage direct, sans conteneur.
				const bool direct = (nVives == 1);
				int32 root = -1;
				if (!direct) {
					const float32 rpos[3] = {sax / (float32)nVives, 0.f, saz / (float32)nVives};
					root = demo::Demo3DHostCreateModelRoot(rpos);
					if (root < 0) {
						plein = true;
						break; // on garde ce qui a pu naitre, et on le DIT plus bas
					}
					NkImpNodeName(st, root, mnm);
				}
				// ── passe B : un noeud maillage par tranche vive ──
				int32 top = root; // le noeud que la carte designe (racine, ou LE maillage)
				uint32 mVerts = 0, mIdx = 0;
				int32 pieces = 0;
				float32 topPos[3] = {0.f, 0.f, 0.f};
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
					// Le maillage direct porte le nom du MODEL (c'est lui l'objet) ;
					// un maillage sous racine porte le nom de SA tranche.
					const char *snm = direct ? mnm : sm.name.CStr();
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
					if (direct) {
						// UN OBJET ORDINAIRE, pas un « maillage interne » : le
						// drapeau IsMesh veut dire « matiere d'un model » -- la
						// hierarchie de scene CACHE ces noeuds (NkHierNodeSkip) et
						// la relecture retire le drapeau a tout maillage sans model
						// au-dessus (NkAsRepairOrphanMeshes). Un maillage direct n'a
						// pas de model : il se comporte comme un cube cree au menu,
						// dont la geometrie vit dans nkvpUserMesh (mesure : sans
						// cette ligne, le noeud depose serait invisible dans la
						// hierarchie et « repare » a la reouverture).
						demo::Demo3DHostSetNodeIsMesh(n, false);
						top = n;
						topPos[0] = a3[0];
						topPos[1] = a3[1];
						topPos[2] = a3[2];
					}
					mVerts += (uint32)lv.Size();
					mIdx += (uint32)li.Size();
					++pieces;
					++noeudsNes;
				}
				if (top < 0)
					continue; // rien n'a pu naitre pour ce model (plein) : dit plus bas
				++modelsNes;
				// ── ARCHIVE EN PLACE : le model et sa matiere sortent du rendu et
				// de la hierarchie sans copie -- « un import n'ajoute pas a la
				// scene » (contrat point 1). Pas de Demo3DHostArchiveNode : il
				// DUPLIQUERAIT et laisserait l'original vivant dans la scene.
				demo::Demo3DHostArchiveTree(top, true);
				// L'origine d'une racine est nee a la moyenne exacte : la mesure
				// doit dire ECART=(0, 0). Un maillage direct n'a rien a recentrer
				// (RecenterModel refuse un non-model, et c'est juste : son origine
				// EST son ancre).
				if (!direct)
					(void)demo::Demo3DHostRecenterModel(top);
				// ── carte navigateur + ECRITURE du .nkmesh, tout de suite ──
				int32 k6 = -1;
				bool ecrit = false;
				if (st.browserCount < NkModelerState::kMaxBrowser) {
					k6 = st.browserCount++;
					st.browserKind[k6] = 6;
					st.browserParent[k6] = st.browserFolder;
					st.browserSub[k6] = 0;
					st.browserDoc[k6] = 0;
					st.browserMat[k6] = 0;
					st.browserFile[k6][0] = 0;
					st.browserSrcNode[k6] = top + 1;
					NkBrowUniqueName(st, 6, st.browserFolder, mnm, st.browserNames[k6],
									 (uint32)sizeof(st.browserNames[0]));
					++cartes;
					if (cardsOut)
						cardsOut->PushBack(k6); // l'appelant (lacher OS) instanciera
					// L'IMPORT ECRIT : le fichier existe des la fin du geste, par le
					// meme ecrivain que « Enregistrer ». Le drapeau « a ecrire au
					// prochain enregistrement » ne s'arme que si l'ecriture ECHOUE --
					// alors la prochaine sauvegarde reprendra la carte, et l'echec
					// est nomme au lieu de rester muet.
					ecrit = NkProjectWriteCard(st.projectRoot, st, k6, &errEcr);
					if (ecrit)
						++fichiers;
					else
						st.browserOriginDirty[k6] = true;
				} else {
					navPlein = true;
				}
				NkLog::Instance().Infof(
					"[import] MESURE creation : model « %s » noeud=%d %s maillages=%d/%d "
					"verts=%u indices=%u origine=(%f, %f, %f) carte=%d fichier=%s",
					mnm, top, direct ? "DIRECT(sans empty)" : "racine+enfants", pieces,
					mo.subCount, mVerts, mIdx, topPos[0], topPos[1], topPos[2], k6,
					ecrit ? st.browserFile[k6] : "(non ecrit)");
			}
			// Le projet a change (cartes dans l'arbre du .nk3dm) : il le sait.
			if (modelsNes > 0)
				NkMarkDirty(st);
			// ⚠️ LE DIAGNOSTIC D'ABORD, LES COMPTES ENSUITE.
			// Ce message est peint dans le panneau HIERARCHIE, qui est etroit :
			// il est TRONQUE a une quarantaine de caracteres. Mesure du 27/08,
			// a l'ecran : « Import : 1 model(s), 1 maillage(s) - nav » -- le mot
			// « navigateur PLEIN » etait coupe. Le chemin n'etait donc pas muet,
			// il etait COUPE EXACTEMENT SUR SA RAISON : l'utilisateur lisait un
			// bilan rassurant d'un import qui n'avait rien produit d'atteignable.
			// Mettre la cause en TETE la rend lisible quelle que soit la largeur,
			// et ne coute pas une ligne de mise en page.
			if (plein)
				snprintf(st.hierNote, sizeof(st.hierNote),
						 "PLUS D'EMPLACEMENT - import incomplet (%d model(s), %d maillage(s))",
						 modelsNes, noeudsNes);
			else if (navPlein)
				snprintf(st.hierNote, sizeof(st.hierNote),
						 "NAVIGATEUR PLEIN (%d max) - rien d'atteignable, %d model(s) perdu(s)",
						 (int32)NkModelerState::kMaxBrowser, modelsNes);
			else if (fichiers < cartes)
				snprintf(st.hierNote, sizeof(st.hierNote),
						 "ECRITURE ECHOUEE - %d fichier(s) sur %d : %s", fichiers, cartes,
						 errEcr.Empty() ? "?" : errEcr.CStr());
			else
				snprintf(st.hierNote, sizeof(st.hierNote),
						 "Import : %d model(s), %d maillage(s), %d .nkmesh ecrit(s) - glissez la carte vers la scene",
						 modelsNes, noeudsNes, fichiers);
			// ── LE CONTROLE DE LA REGLE, ecrit PENDANT QU'IL ROUGIT ────────────
			// Regle de Rodolf elargie : « tout ce qu'un import produit doit etre
			// editable ». Ecrite ici comme une VERIFICATION et non comme une liste
			// de taches : elle se re-passe a chaque format ajoute, sans que
			// personne ait a tenir un inventaire a jour.
			// ⚠️ UN CONTROLE ECRIT QUAND LE DEFAUT EST VIVANT NAIT ROUGE. Ecrit
			// apres la correction, il naitrait vert et personne ne saurait jamais
			// s'il sait rougir. Celui-ci echoue AUJOURD'HUI sur la 1re ligne.
			if (getenv("NK_IMPORT_CHECK")) {
				const bool atteignable = (cartes > 0) || (modelsNes > 0 && !navPlein && !plein);
				const bool ecritures = (cartes == 0) || (fichiers == cartes);
				NkLog::Instance().Infof("[import] CONTROLE REGLE -- ce qui est produit "
										"doit etre ATTEIGNABLE et ACCEPTE par son mode");
				NkLog::Instance().Infof("[import]   1. atteignable (scene ou navigateur) : %s "
										"(models=%d cartes=%d navPlein=%d plein=%d)",
										atteignable ? "OK" : "ECHEC", modelsNes, cartes,
										navPlein ? 1 : 0, plein ? 1 : 0);
				NkLog::Instance().Infof("[import]   2. fichiers ecrits pour chaque carte : %s "
										"(%d/%d)",
										ecritures ? "OK" : "ECHEC", fichiers, cartes);
				NkLog::Instance().Infof("[import]   3. geometrie editable : %s",
										atteignable ? "a verifier en mode edition"
													: "NON EVALUABLE (rien d'atteignable)");
				NkLog::Instance().Infof("[import]   4. UV / textures : ABSENTS et ANNONCES "
										"tels quels par l'interface -- hors regle aujourd'hui");
				NkLog::Instance().Infof("[import] CONTROLE : %s",
										(atteignable && ecritures) ? "VERT" : "ROUGE");
			}
			return modelsNes > 0;
		}

		/// L'IMPORT COMPLET : charge, decoupe, journalise (`MESURE import`),
		/// puis CREE les noeuds. Tout refus est NOMME dans `st.hierNote` -- un
		/// refus silencieux serait indistinguable d'un bouton casse.
		inline bool NkImportFile(NkModelerState &st, const char *absPath,
								 NkVector<int32> *cardsOut = nullptr) {
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
			return NkImportCreate(st, data, models, stem, cardsOut);
		}

		/// INSTANCIER dans la scene active les cartes que l'import vient de
		/// creer -- c'est le geste « systeme -> scene » et « systeme -> hierarchie »
		/// du contrat (import + instanciation). Chaque carte est dupliquee depuis
		/// son archive (le MEME chemin que le depot depuis le navigateur,
		/// NkDropSpawnModel), puis posee a SON origine du fichier + `off3` :
		/// les models d'un meme fichier gardent leur disposition relative (la
		/// caisse et ses quatre roues restent une voiture) -- empiler cinq
		/// models au meme point aurait defait ce que le fichier declare.
		/// `off3 == nullptr` : coordonnees du fichier telles quelles
		/// (hierarchie). Rend le nombre de noeuds nes ; le dernier est selectionne.
		inline int32 NkImportInstantiate(NkModelerState &st, const NkVector<int32> &cards,
										 const float32 *off3) {
			int32 nes = 0, dernier = -1;
			for (usize i = 0; i < cards.Size(); ++i) {
				const int32 c = cards[i];
				if (c < 0 || c >= st.browserCount || st.browserKind[c] != 6 ||
					st.browserSrcNode[c] <= 0)
					continue;
				const int32 src = st.browserSrcNode[c] - 1;
				const int32 nn = demo::Demo3DHostDuplicateNode(src);
				if (nn < 0) {
					NkLog::Instance().Warnf("[import] instanciation : carte %d « %s » : "
											"plus d'emplacement", c, st.browserNames[c]);
					break;
				}
				float32 sp[3] = {0.f, 0.f, 0.f}, sr[3] = {0.f, 0.f, 0.f}, ss[3] = {1.f, 1.f, 1.f};
				(void)demo::Demo3DHostEmptyTransform(src, sp, sr, ss);
				// TOUJOURS pose explicitement : le double nait decale de
				// (0.45, 0, 0.45) « comme Blender » (Demo3DHostDuplicateNodeEx),
				// ce qui casserait la disposition du fichier. `off3 == nullptr`
				// = coordonnees du fichier telles quelles.
				{
					const float32 o0 = off3 ? off3[0] : 0.f, o1 = off3 ? off3[1] : 0.f,
								  o2 = off3 ? off3[2] : 0.f;
					const float32 np[3] = {sp[0] + o0, sp[1] + o1, sp[2] + o2};
					demo::Demo3DHostSetModelTransform(nn, np, sr, ss); // rotation/echelle de la source
				}
				// Le double porte le nom de la carte : sans lui, la hierarchie
				// afficherait « Cube.NNN » pour une roue.
				if (nn < 176)
					snprintf(st.customNames[nn], sizeof(st.customNames[0]), "%s", st.browserNames[c]);
				float32 gp[3] = {0.f, 0.f, 0.f}, gr[3] = {0.f, 0.f, 0.f}, gs[3] = {0.f, 0.f, 0.f};
				(void)demo::Demo3DHostEmptyTransform(nn, gp, gr, gs);
				NkLog::Instance().Infof("[import] MESURE instanciation : carte %d « %s » src=%d -> "
										"noeud=%d model=%d pose=(%f, %f, %f)",
										c, st.browserNames[c], src, nn,
										demo::Demo3DHostNodeIsModel(nn) ? 1 : 0, gp[0], gp[1], gp[2]);
				dernier = nn;
				++nes;
			}
			if (dernier >= 0)
				demo::Demo3DHostSelectEmptyNode(dernier);
			if (nes > 0)
				NkMarkDirty(st);
			return nes;
		}

		/// Barycentre X/Z des origines des cartes (Y = 0) : le point que le
		/// lacher sur la vue 3D amene sous le curseur. Rend faux si aucune carte.
		inline bool NkImportCardsCenter(const NkModelerState &st, const NkVector<int32> &cards,
										float32 *out3) {
			float32 sx = 0.f, sz = 0.f;
			int32 n = 0;
			for (usize i = 0; i < cards.Size(); ++i) {
				const int32 c = cards[i];
				if (c < 0 || c >= st.browserCount || st.browserSrcNode[c] <= 0)
					continue;
				float32 sp[3] = {0.f, 0.f, 0.f}, sr[3], ss[3];
				if (!demo::Demo3DHostEmptyTransform(st.browserSrcNode[c] - 1, sp, sr, ss))
					continue;
				sx += sp[0];
				sz += sp[2];
				++n;
			}
			if (n == 0)
				return false;
			out3[0] = sx / (float32)n;
			out3[1] = 0.f;
			out3[2] = sz / (float32)n;
			return true;
		}

		/// LE LACHER VENU DU SYSTEME, route par la zone (contrat point 2) :
		///   vue 3D       -> import + instanciation AU POINT DU LACHER (pick differe)
		///   hierarchie   -> import + instanciation aux coordonnees du fichier
		///   navigateur   -> import SEUL (cartes + fichiers, rien dans la scene)
		///   ailleurs     -> refus NOMME
		/// Appele par la boucle une fois les rects de la frame connus. Plusieurs
		/// fichiers = plusieurs imports, leurs cartes s'additionnent.
		inline void NkOsDropRoute(NkModelerState &st) {
			if (st.osDropCount <= 0)
				return;
			const nkgui::NkVec2 at{st.osDropX, st.osDropY};
			int32 zone = 4;
			if (NkHitRegistry::Contains(st.viewRect, at))
				zone = 1;
			else if (NkHitRegistry::Contains(st.hierRect, at))
				zone = 2;
			else if (NkHitRegistry::Contains(st.browserRect, at))
				zone = 3;
			NkLog::Instance().Infof("[import] MESURE lacher OS : %d fichier(s) a (%f, %f) zone=%d "
									"(1 vue, 2 hierarchie, 3 navigateur, 4 ailleurs)",
									st.osDropCount, at.x, at.y, zone);
			NkVector<int32> cards;
			if (zone == 4) {
				snprintf(st.hierNote, sizeof(st.hierNote),
						 "Deposez un fichier 3D sur la vue, la hierarchie ou le navigateur");
			} else {
				for (int32 i = 0; i < st.osDropCount; ++i)
					(void)NkImportFile(st, st.osDropPaths[i], &cards);
			}
			st.osDropCount = 0;
			if (cards.Empty())
				return; // refus deja nomme par NkImportFile (hierNote)
			if (zone == 3)
				return; // navigateur : import seul, c'est le contrat
			if (zone == 2) {
				const int32 n = NkImportInstantiate(st, cards, nullptr);
				snprintf(st.hierNote, sizeof(st.hierNote),
						 "Import : %d carte(s), %d objet(s) ajoute(s) a la scene (coordonnees du fichier)",
						 (int32)cards.Size(), n);
				return;
			}
			// Vue 3D : le point du monde vient du pick, qui repond a la frame
			// suivante -- on retient les cartes et on demande.
			st.osDropCardCount = 0;
			for (usize i = 0; i < cards.Size() && st.osDropCardCount < 64; ++i)
				st.osDropCards[st.osDropCardCount++] = cards[i];
			st.osDropPickPending = true;
			demo::Demo3DHostPickRequest(at.x, at.y);
		}

		/// La reponse du pick pour un lacher OS sur la vue 3D : instancie les
		/// cartes retenues, disposition du fichier conservee, barycentre X/Z
		/// amene au point du lacher. Sur un objet ou dans le vide, meme geste
		/// (pas de menu « enfant » : un fichier entier n'est pas une carte).
		inline void NkOsDropPickTake(NkModelerState &st) {
			if (!st.osDropPickPending)
				return;
			int32 node = -3;
			float32 w[3] = {0.f, 0.f, 0.f};
			if (!demo::Demo3DHostPickTake(&node, w))
				return;
			st.osDropPickPending = false;
			NkVector<int32> cards;
			for (int32 i = 0; i < st.osDropCardCount; ++i)
				cards.PushBack(st.osDropCards[i]);
			st.osDropCardCount = 0;
			float32 off[3] = {0.f, 0.f, 0.f};
			if (node != -2) { // -2 = hors du viseur : coordonnees du fichier
				float32 c3[3];
				if (NkImportCardsCenter(st, cards, c3)) {
					off[0] = w[0] - c3[0];
					off[1] = w[1] - c3[1];
					off[2] = w[2] - c3[2];
				}
			}
			const int32 n = NkImportInstantiate(st, cards, off);
			snprintf(st.hierNote, sizeof(st.hierNote),
					 "Import : %d carte(s), %d objet(s) poses au point du lacher",
					 (int32)cards.Size(), n);
		}

	} // namespace nk3d
} // namespace nkentseu
