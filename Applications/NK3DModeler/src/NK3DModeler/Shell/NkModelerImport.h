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
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NK3DModeler/Shell/NkModelerScreens.h" // etat + hote + NkBrowUniqueName/NkMarkDirty
#include "NK3DModeler/Project/NkModelerAssets.h" // NkProjectWriteCard : l'UNIQUE ecrivain de carte
#include "NKRenderer/Mesh/NkOBJLoader.h"
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKRenderer/Mesh/NkFBXLoader.h"

#include <cstdarg> // va_list : NkImportNote
#include "NKRenderer/Mesh/NkDAELoader.h"
#include "NKRenderer/Mesh/NkPLYLoader.h"
#include "NKRenderer/Mesh/NkSTLLoader.h"
#include "NKRenderer/Mesh/NkUSDALoader.h"
#include "NKLogger/NkLog.h"
#include "NK3DModeler/Shell/NkModelerToast.h" // le resultat SE VOIT, il ne se note pas

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

		/// Pose la note du panneau Hierarchie ET l'ecrit au JOURNAL.
		///
		/// 🔴 Pourquoi les deux. Le 2026-09-05, Rodolf a rapporte « l'import de
		/// XBot echoue ». Mesure : `LoadFBX` lit le fichier parfaitement
		/// (99 796 sommets, 65 articulations, 2 modeles) — c'est
		/// `NkImportCreate` qui refusait, faute de projet ouvert, en ecrivant
		/// dans `st.hierNote` et **nulle part ailleurs**. Une note dans un
		/// panneau qu'on ne regarde pas se confond avec un plantage.
		///
		/// `refus` distingue le ton : un refus part en avertissement (il demande
		/// un geste), un compte-rendu part en information.
		inline void NkImportNote(NkModelerState &st, NkToastKind kind, const char *fmt, ...) {
			char buf[248];
			va_list ap;
			va_start(ap, fmt);
			vsnprintf(buf, sizeof(buf), fmt, ap);
			va_end(ap);
			snprintf(st.hierNote, sizeof(st.hierNote), "%s", buf);
			// 🔴 LA TROISIEME DESTINATION, celle qui manquait : L'ECRAN. La note
			// du panneau et le journal restent (l'une pour le contexte, l'autre
			// pour la trace), mais aucune des deux n'oblige a regarder.
			NkToastPush(kind, buf);
			if (kind == NkToastKind::Succes)
				NkLog::Instance().Infof("[import] %s", buf);
			else
				NkLog::Instance().Warnf("[import] %s", buf);
		}
		/// Compatibilite d'appel : `true` = refus, `false` = compte-rendu.
		inline void NkImportNote(NkModelerState &st, bool refus, const char *fmt, ...) {
			char buf[248];
			va_list ap;
			va_start(ap, fmt);
			vsnprintf(buf, sizeof(buf), fmt, ap);
			va_end(ap);
			NkImportNote(st, refus ? NkToastKind::Refus : NkToastKind::Succes, "%s", buf);
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

		/// LES MATERIAUX ET LEURS TEXTURES, du fichier vers le projet.
		///
		/// 🔴 CE QUE LA MESURE DIT, ET CE QU'ELLE NE DIT PAS (2026-09-06).
		/// Rodolf : « les import n'importent pas les textures ni les materiaux ».
		/// C'est vrai, et la question etait de savoir OU ca se perd -- le lecteur
		/// ne lit pas, ou l'import ne publie pas ? Deux endroits differents.
		/// Journal de sa session de 08h53 :
		///
		///   [NkGLTFLoader] '...042082ea....glb' : 249906 vertices, 1499568
		///                  indices, 1 submeshes, **0 materials, 0 images**
		///   [NkFBXLoader]  OK '...cables.fbx' : 1 geometries, 1200 verts,
		///                  1 sous-meshes, **1 materiaux, 1 textures**, 1 nodes
		///
		/// ⚠️ Le mannequin du GLB est gris parce que SON FICHIER NE PORTE AUCUN
		/// MATERIAU -- verifie dans le JSON du .glb lui-meme : les sections
		/// `materials`, `images` et `textures` n'existent pas, et la primitive
		/// n'a que l'attribut POSITION. Sa capture ne prouve donc RIEN sur
		/// l'import des materiaux, et les cartes `Materiau`, `Materiau.000` a
		/// `.004` et `Texture` de son navigateur PREEXISTENT a son geste.
		///
		/// C'est `cables.fbx` qui tranche : le lecteur LIT bien 1 materiau et
		/// 1 texture, et l'import n'en publiait AUCUN. Le defaut est donc ici,
		/// pas dans les chargeurs.
		///
		/// CE QUI EST ECRIT : une TEXTURE = un fichier PNG dans le projet (les
		/// pixels sont deja decodes par le chargeur, embarques comme references)
		/// -- c'est la regle d'import de CONVENTIONS_FICHIERS.md, « copier en
		/// gardant l'origine ». Un MATERIAU = un emplacement du projet + sa
		/// carte + son `.nkmat`, par le meme ecrivain que « Enregistrer ».
		///
		/// `slotOf` rend, pour chaque materiau du FICHIER, l'emplacement du
		/// PROJET (-1 si la creation a echoue : plus d'emplacement libre).
		inline void NkImportMaterials(NkModelerState &st, const renderer::NkGLTFMeshData &data,
									  const char *stem, NkVector<int32> &slotOf, int32 *texEcrites,
									  int32 *matNees) {
			slotOf.Clear();
			if (texEcrites)
				*texEcrites = 0;
			if (matNees)
				*matNees = 0;
			const int32 nMat = (int32)data.materials.Size();
			if (nMat <= 0)
				return;
			// ── 1. LES IMAGES SUR LE DISQUE ─────────────────────────────────
			// Une par entree de `data.images`, ecrite UNE FOIS meme si trois
			// canaux la designent. Chemin ABSOLU retenu : c'est ce que l'hote
			// attend (`Demo3DHostProjMatSetMap`), et la sauvegarde le rend
			// relatif au projet toute seule (NkScToRel, NkModelerAssets.h:359).
			NkVector<NkString> imgPath;
			const int32 nImg = (int32)data.images.Size();
			const NkString dossier = NkAsFolderPath(st, st.browserFolder);
			for (int32 i = 0; i < nImg; ++i) {
				imgPath.PushBack(NkString());
				if (!data.images[(uint32)i].valid)
					continue;
				// Le nom vient du FICHIER quand il en donne un (`uri`, rempli par
				// le chargeur FBX avec le RelativeFilename), sinon du radical du
				// modele + le rang -- jamais d'un compteur seul, qui ne dirait
				// pas de quel import il vient.
				char base[48];
				const char *uri = data.images[(uint32)i].uri.CStr();
				if (uri && uri[0])
					NkImpStem(uri, base, (uint32)sizeof(base));
				else
					snprintf(base, sizeof(base), "%s_tex%d", stem, i);
				NkString rel = dossier;
				rel.Append(base);
				rel.Append(".png");
				const NkString abs = NkScToAbs(st.projectRoot, rel.CStr());
				if (data.images[(uint32)i].decoded.SavePNG(abs.CStr())) {
					imgPath[(uint32)i] = abs;
					if (texEcrites)
						++*texEcrites;
					NkLog::Instance().Infof("[import] MESURE texture : image %d -> '%s' (%ux%u)", i,
											rel.CStr(),
											(unsigned)data.images[(uint32)i].decoded.Width(),
											(unsigned)data.images[(uint32)i].decoded.Height());
				} else {
					// ECHEC NOMME, jamais avale : sans cette ligne un materiau
					// arriverait sans sa texture et on accuserait le materiau.
					NkLog::Instance().Warnf("[import] texture NON ECRITE : image %d -> '%s'", i,
											rel.CStr());
				}
			}
			// ── 2. LES MATERIAUX ────────────────────────────────────────────
			for (int32 m = 0; m < nMat; ++m) {
				const renderer::NkGLTFMaterial &gm = data.materials[(uint32)m];
				const int32 slot = demo::Demo3DHostProjMatCreate();
				slotOf.PushBack(slot);
				if (slot < 0) {
					// PLAFOND ATTEINT, et il est NOMME (Rodolf, point ④) : le
					// projet n'accepte que kNkvpMaxProjMats emplacements.
					NkImportNote(st, NkToastKind::Partiel,
								 "Import PARTIEL : le materiau « %s » n'a PAS pu etre cree -- le projet "
								 "n'a plus d'emplacement de materiau libre. Que faire : supprimez des "
								 "materiaux inutilises dans le navigateur, puis relancez l'import.",
								 gm.name.Empty() ? "(sans nom)" : gm.name.CStr());
					continue;
				}
				char nm[32];
				if (!gm.name.Empty())
					snprintf(nm, sizeof(nm), "%s", gm.name.CStr());
				else
					snprintf(nm, sizeof(nm), "%s_mat%d", stem, m);
				demo::Demo3DHostProjMatSetName(slot, nm);
				const float32 alb[3] = {gm.baseColorFactor.x, gm.baseColorFactor.y,
										gm.baseColorFactor.z};
				demo::Demo3DHostProjMatSetParams(slot, alb, gm.roughnessFactor, gm.metallicFactor);
				// L'EMISSION PORTE SA FORCE (KHR_materials_emissive_strength) :
				// la separer de la teinte est ce que fait le format, et les
				// melanger ici rendrait 1000 possible sans que rien le dise.
				{
					const float32 emi[3] = {gm.emissiveFactor.x * gm.emissiveStrength,
											gm.emissiveFactor.y * gm.emissiveStrength,
											gm.emissiveFactor.z * gm.emissiveStrength};
					demo::Demo3DHostProjMatSetEmissive(slot, emi);
				}
				// Opacite : `baseColorFactor.w`, et seulement si le fichier dit
				// BLEND. En OPAQUE une alpha < 1 ne veut rien dire -- la lire
				// quand meme rendrait transparents des materiaux qui ne le sont
				// pas (c'est la famille du champ dont le sens depend d'un autre).
				{
					float32 a = 1.f, aniso = 0.f, sheen = 0.f;
					demo::Demo3DHostProjMatPBRExtra(slot, &a, &aniso, &sheen);
					demo::Demo3DHostProjMatSetPBRExtra(
						slot, gm.alphaMode == 2 ? gm.baseColorFactor.w : 1.f, aniso, sheen);
				}
				// LES QUATRE CANAUX (cf. NkDemo3DHost.h) : 0 couleur, 1 normale,
				// 2 ORM, 3 emissif. glTF empaquette deja rugosite/metal en
				// `metallicRoughnessImage` -- c'est l'ORM du canal 2.
				const int32 canal[4] = {gm.baseColorImage, gm.normalImage,
										gm.metallicRoughnessImage, gm.emissiveImage};
				int32 posees = 0;
				for (int32 c = 0; c < 4; ++c) {
					const int32 ii = canal[c];
					if (ii < 0 || ii >= nImg || imgPath[(uint32)ii].Empty())
						continue;
					if (demo::Demo3DHostProjMatSetMap(slot, c, imgPath[(uint32)ii].CStr()))
						++posees;
				}
				if (gm.normalImage >= 0) {
					float32 nrm = 1.f, emiF = 1.f;
					demo::Demo3DHostProjMatChanStrength(slot, &nrm, &emiF);
					demo::Demo3DHostProjMatSetChanStrength(slot, gm.normalScale, emiF);
				}
				// LA CARTE, puis le `.nkmat` : la carte porte le chemin, elle
				// vient donc d'abord (meme ordre que « + Materiau »).
				const int32 cb = st.CardAdd();
				st.Card(cb).kind = 2;
				st.Card(cb).parent = st.browserFolder;
				st.Card(cb).sub = 0;
				st.Card(cb).doc = 0;
				st.Card(cb).srcNode = 0;
				st.Card(cb).mat = slot + 1;
				st.Card(cb).file[0] = 0;
				NkBrowUniqueName(st, 2, st.browserFolder, nm, st.Card(cb).name,
								 (uint32)NkModelerState::kCardNameCap);
				// Le nom RETENU est celui de la carte : sans ca, deux imports du
				// meme fichier donneraient deux cartes distinctes portant le meme
				// nom cote materiau, et le panneau ne saurait plus les separer.
				demo::Demo3DHostProjMatSetName(slot, st.Card(cb).name);
				NkString errM;
				const bool ecrit = NkProjectWriteCard(st.projectRoot, st, cb, &errM);
				if (matNees)
					++*matNees;
				NkLog::Instance().Infof(
					"[import] MESURE materiau : « %s » -> emplacement=%d carte=%d albedo=(%f, %f, %f) "
					"rugosite=%f metal=%f textures posees=%d/4 fichier=%s",
					st.Card(cb).name, slot, cb, alb[0], alb[1], alb[2], gm.roughnessFactor,
					gm.metallicFactor, posees, ecrit ? st.Card(cb).file : "(non ecrit)");
			}
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

		/// LA SCENE HOTE OU L'IMPORT DEPOSE SES ARCHIVES -- et pourquoi ce n'est
		/// PAS toujours le document actif.
		///
		/// 🔴 MESURE du 2026-09-06, et c'est la vraie cause du refus que Rodolf
		/// conteste. Un noeud nait dans `nkvpSceneOf = nkvpCurScene`, donc dans
		/// le document ACTIF (`HostAllocUser`, NkDemo3D.cpp). Or
		/// `NkScWriteScene` (NkModelerScene.h) rend **-1** -- « ne pas ecrire »
		/// -- pour toute scene hote qui appartient a un document TRANSITOIRE :
		/// editeur d'asset ou isolation. Un import lance depuis un onglet de
		/// MODEL faisait donc naitre son archive dans une scene que
		/// l'enregistrement JETTE : la carte partait dans le `.nk3dm`, sa
		/// geometrie non, et le model rouvert aurait ete vide.
		///
		/// Le refus etait donc une PROTECTION, jamais ecrite comme telle. Rodolf
		/// a raison sur la regle -- « l'import est une operation du projet » --
		/// et le retirer seul aurait installe une perte silencieuse. On corrige
		/// la cause : l'import depose dans une scene DURABLE, quel que soit
		/// l'onglet ouvert.
		///
		/// Rend -1 si aucun document durable n'existe : l'appelant refuse et le
		/// dit, il ne devine pas.
		inline int32 NkImpArchiveScene(const NkModelerState &st) {
			// Cas courant, et le moins surprenant : l'onglet actif EST une scene.
			const int32 dA = st.TabDoc(st.activeTab);
			if (dA >= 0 && !st.docTransient[dA])
				return (int32)st.docScene[dA];
			// Sinon la PREMIERE scene durable du projet. Le choix n'a pas
			// d'effet visible -- l'archive est invisible dans tous les documents
			// (`nkvpDeleted`) -- il decide seulement du fichier qui la portera.
			for (int32 d = 0; d < NkModelerState::kMaxDocs; ++d)
				if (st.docUsed[d] && !st.docTransient[d])
					return (int32)st.docScene[d];
			return -1;
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
			// Un import ECRIT dans le projet : sans projet ouvert, il n'a nulle
			// part ou ecrire, et le dire vaut mieux qu'ecrire dans le vide.
			if (st.projectRoot.Empty()) {
				NkImportNote(st, NkToastKind::Refus,
							 "Import impossible : aucun PROJET ouvert. L'import ecrit un .nkmesh par model "
							 "dans le dossier du projet. Que faire : Fichier > Nouveau projet, ou "
							 "Fichier > Ouvrir un projet, puis relancez l'import.");
				return false;
			}
			const uint32 vTotal = (uint32)data.vertices.Size();
			const uint32 iTotal = (uint32)data.indices.Size();
			if (vTotal == 0 || iTotal == 0) {
				// Refusait en SILENCE : ni note, ni journal. Un fichier lu mais
				// vide est le cas le plus deroutant de tous — tout a « marche »
				// et rien n'apparait.
				NkImportNote(st, NkToastKind::Refus,
							 "Import impossible : le fichier a bien ete LU, mais il ne contient aucune "
							 "geometrie (%u sommets, %u indices). Que faire : verifiez l'export -- le "
							 "maillage a peut-etre ete exporte sans ses sommets, ou seule une armature "
							 "a ete ecrite.",
							 (unsigned)vTotal, (unsigned)iTotal);
				return false;
			}
			// ── L'IMPORT EST UNE OPERATION DU PROJET, PAS DE L'ONGLET ────────
			// (Rodolf, 2026-09-06 : « il devrait juste importer et mettre dans
			// le dossier, il n'y a pas de probleme a importer que ce soit quand
			// un onglet de model est ouvert ou pas ».)
			// La scene hote est forcee sur une scene DURABLE le temps de la
			// creation, puis rendue telle qu'elle etait. Voir NkImpArchiveScene
			// pour la mesure qui l'exige.
			const int32 scArchive = NkImpArchiveScene(st);
			if (scArchive < 0) {
				NkImportNote(st, NkToastKind::Refus,
							 "Import impossible : ce projet n'a aucune scene DURABLE ou deposer l'archive "
							 "(seuls des onglets d'edition sont ouverts). Que faire : Scene > Nouvelle "
							 "scene, puis relancez l'import.");
				return false;
			}
			const int32 scAvant = demo::Demo3DHostActiveScene();
			demo::Demo3DHostSetActiveScene(scArchive);
			// ── LES MATERIAUX ET LES TEXTURES, AVANT LES NOEUDS ─────────────
			// Avant, parce qu'un noeud doit pouvoir recevoir son emplacement au
			// moment ou il nait : `Demo3DHostCreateMeshNode` lui pose deja le
			// materiau par DEFAUT, et repasser dessus apres coup demanderait de
			// retrouver quel sous-mesh a fait quel noeud -- une seconde table a
			// tenir d'accord avec la premiere.
			NkVector<int32> matSlot; // materiau du FICHIER -> emplacement du PROJET
			int32 texEcrites = 0, matNees = 0;
			NkImportMaterials(st, data, stem, matSlot, &texEcrites, &matNees);
			// Table globale -> local d'UNE tranche. Remise a -1 par liste des
			// entrees touchees (jamais un balayage de tout le buffer par
			// tranche).
			NkVector<int32> remap;
			remap.Resize((usize)vTotal, -1);
			int32 modelsNes = 0, noeudsNes = 0, cartes = 0, fichiers = 0;
			bool plein = false;
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
					NkImportNote(st, NkToastKind::Partiel,
								 "Import PARTIEL : le model « %s » du fichier a ete SAUTE -- il ne porte "
								 "aucune geometrie (un empty, ou une armature seule).",
								 mnm);
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
					// ── LE MATERIAU DE LA TRANCHE ────────────────────────────
					// `subMeshMaterial` est PARALLELE a `subMeshes` : c'est le
					// fichier qui dit quelle matiere va sur quelle tranche, on ne
					// le devine pas. -1 = la tranche n'en declare aucun, et le
					// materiau par defaut pose a la naissance reste le bon.
					{
						const int32 si = mo.firstSub + s;
						const int32 fm = (si < (int32)data.subMeshMaterial.Size())
											 ? data.subMeshMaterial[(uint32)si]
											 : -1;
						if (fm >= 0 && fm < (int32)matSlot.Size() && matSlot[(uint32)fm] >= 0)
							demo::Demo3DHostProjMatAssign(n, matSlot[(uint32)fm]);
					}
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
				k6 = st.CardAdd();
				st.Card(k6).kind = 6;
				st.Card(k6).parent = st.browserFolder;
				st.Card(k6).sub = 0;
				st.Card(k6).doc = 0;
				st.Card(k6).mat = 0;
				st.Card(k6).file[0] = 0;
				st.Card(k6).srcNode = top + 1;
				NkBrowUniqueName(st, 6, st.browserFolder, mnm, st.Card(k6).name,
								 (uint32)NkModelerState::kCardNameCap);
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
					st.Card(k6).originDirty = true;
				NkLog::Instance().Infof(
					"[import] MESURE creation : model « %s » noeud=%d %s maillages=%d/%d "
					"verts=%u indices=%u origine=(%f, %f, %f) carte=%d fichier=%s",
					mnm, top, direct ? "DIRECT(sans empty)" : "racine+enfants", pieces,
					mo.subCount, mVerts, mIdx, topPos[0], topPos[1], topPos[2], k6,
					ecrit ? st.Card(k6).file : "(non ecrit)");
			}
			// LA SCENE ACTIVE REVIENT TELLE QU'ELLE ETAIT. Elle est rendue ICI et
			// pas plus haut : tout ce qui precede fait naitre des noeuds, et
			// `HostAllocUser` lit `nkvpCurScene` a chaque naissance.
			demo::Demo3DHostSetActiveScene(scAvant);
			NkLog::Instance().Infof("[import] MESURE scene d'archive : depot=%d actif=%d rendu=%d "
									"docModel=%d",
									scArchive, scAvant, demo::Demo3DHostActiveScene(),
									demo::Demo3DHostDocIsModel() ? 1 : 0);
			// Le projet a change (cartes dans l'arbre du .nk3dm) : il le sait.
			if (modelsNes > 0)
				NkMarkDirty(st);
			if (plein)
				NkImportNote(st, NkToastKind::Partiel,
							 "Import PARTIEL : %d model(s) et %d maillage(s) sont entres, les suivants ont "
							 "ete SAUTES -- la scene n'a plus d'emplacement de noeud libre. Que faire : "
							 "supprimez des objets de la scene, ou importez dans une scene neuve.",
							 modelsNes, noeudsNes);
			else if (fichiers < cartes)
				NkImportNote(st, NkToastKind::Partiel,
							 "Import PARTIEL : %d carte(s) creee(s), mais seulement %d fichier(s) .nkmesh "
							 "ecrit(s) sur %d. Raison : %s. Que faire : les cartes non ecrites seront "
							 "reprises au prochain Enregistrer -- verifiez les droits du dossier du projet.",
							 cartes, fichiers, cartes,
							 errEcr.Empty() ? "non precisee" : errEcr.CStr());
			else
				// LE BANDEAU DIT CE QUI EST ENTRE, Y COMPRIS QUAND C'EST ZERO --
				// et c'est deliberé. « 0 materiau(x) » sur un fichier qui n'en
				// porte pas est une INFORMATION : c'est exactement ce qui aurait
				// evite a Rodolf de lire son mannequin gris comme un defaut de
				// l'import, alors que son .glb ne declare aucun materiau.
				NkImportNote(st, NkToastKind::Succes,
							 "Import reussi : %d model(s), %d maillage(s), %d materiau(x), %d texture(s), "
							 "%d fichier(s) .nkmesh ecrit(s) dans le projet. Ils sont dans le NAVIGATEUR "
							 "-- glissez une carte vers la scene pour la poser.",
							 modelsNes, noeudsNes, matNees, texEcrites, fichiers);
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
				// `NkImportNote` ecrit DEJA au journal : le second appel faisait
				// deux lignes pour un seul refus.
				NkImportNote(st, NkToastKind::Refus, "Import impossible de '%s' : %s", absPath ? absPath : "(null)",
							 why ? why : "raison inconnue");
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

		/// L'IMPORT D'UNE LISTE. C'est la forme que Rodolf demande (2026-09-06 :
		/// « dans le cas de l'import de NK3DModeler je dois pouvoir charger
		/// plusieurs modeles »), et elle est ecrite MAINTENANT bien que le
		/// selecteur du kit ne rende encore qu'UN chemin : la selection multiple
		/// est le chantier de l'agent du kit, pas le mien, et le jour ou elle
		/// arrive il n'y aura qu'un appelant a changer -- pas la chaine.
		///
		/// ⚠️ UN ECHEC N'ARRETE PAS LES SUIVANTS, et c'est deliberé : sur dix
		/// fichiers laches ensemble, s'arreter au troisieme perdrait les sept
		/// bons sans le dire. Chaque refus est deja NOMME par `NkImportFile` ;
		/// on rend ici le COMPTE, et le total part au journal.
		/// Rend le nombre de fichiers dont l'import a produit au moins un model.
		inline int32 NkImportFiles(NkModelerState &st, const char *const *paths, int32 count,
								   NkVector<int32> *cardsOut = nullptr) {
			if (!paths || count <= 0)
				return 0;
			int32 ok = 0;
			for (int32 i = 0; i < count; ++i) {
				if (!paths[i] || !paths[i][0])
					continue;
				if (NkImportFile(st, paths[i], cardsOut))
					++ok;
			}
			if (count > 1)
				NkLog::Instance().Infof("[import] MESURE lot : %d fichier(s) demande(s), %d "
										"aboutis, %d carte(s) au total",
										count, ok, cardsOut ? (int32)cardsOut->Size() : -1);
			return ok;
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
				if (c < 0 || c >= st.BrowserCount() || st.Card(c).kind != 6 ||
					st.Card(c).srcNode <= 0)
					continue;
				const int32 src = st.Card(c).srcNode - 1;
				const int32 nn = demo::Demo3DHostDuplicateNode(src);
				if (nn < 0) {
					NkImportNote(st, NkToastKind::Partiel,
								 "Import PARTIEL : la carte « %s » n'a PAS pu etre posee dans la scene -- "
								 "plus d'emplacement de noeud libre. Elle existe dans le navigateur : "
								 "videz la scene, puis glissez-la.",
								 st.Card(c).name);
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
					snprintf(st.customNames[nn], sizeof(st.customNames[0]), "%s", st.Card(c).name);
				float32 gp[3] = {0.f, 0.f, 0.f}, gr[3] = {0.f, 0.f, 0.f}, gs[3] = {0.f, 0.f, 0.f};
				(void)demo::Demo3DHostEmptyTransform(nn, gp, gr, gs);
				NkLog::Instance().Infof("[import] MESURE instanciation : carte %d « %s » src=%d -> "
										"noeud=%d model=%d pose=(%f, %f, %f)",
										c, st.Card(c).name, src, nn,
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
				if (c < 0 || c >= st.BrowserCount() || st.Card(c).srcNode <= 0)
					continue;
				float32 sp[3] = {0.f, 0.f, 0.f}, sr[3], ss[3];
				if (!demo::Demo3DHostEmptyTransform(st.Card(c).srcNode - 1, sp, sr, ss))
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
				NkImportNote(st, NkToastKind::Refus,
							 "Fichier lache HORS d'une zone qui l'accepte. Que faire : deposez-le sur la "
							 "VUE 3D (il se pose sous le curseur), sur la HIERARCHIE (aux coordonnees du "
							 "fichier) ou sur le NAVIGATEUR (import seul, rien dans la scene).");
			} else {
				// LE LACHER PORTE DEJA UNE LISTE : il passe donc par la porte de
				// liste, comme le bouton. Deux chemins d'import auraient diverge
				// au premier reglage qu'on ajoute (dialogue d'import, contrat
				// point 3).
				const char *ptrs[NkModelerState::kMaxOsDrop];
				int32 n = 0;
				for (int32 i = 0; i < st.osDropCount && n < (int32)NkModelerState::kMaxOsDrop; ++i)
					ptrs[n++] = st.osDropPaths[i];
				(void)NkImportFiles(st, ptrs, n, &cards);
			}
			st.osDropCount = 0;
			if (cards.Empty())
				return; // refus deja nomme par NkImportFile (hierNote)
			if (zone == 3)
				return; // navigateur : import seul, c'est le contrat
			if (zone == 2) {
				const int32 n = NkImportInstantiate(st, cards, nullptr);
				NkImportNote(st, n > 0 ? NkToastKind::Succes : NkToastKind::Partiel,
							 "Import reussi : %d carte(s) creee(s), %d objet(s) ajoute(s) a la scene aux "
							 "coordonnees du fichier.",
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
			NkImportNote(st, n > 0 ? NkToastKind::Succes : NkToastKind::Partiel,
						 "Import reussi : %d carte(s) creee(s), %d objet(s) poses au point du lacher.",
						 (int32)cards.Size(), n);
		}

	} // namespace nk3d
} // namespace nkentseu
