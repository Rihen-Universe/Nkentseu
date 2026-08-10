#pragma once
// =============================================================================
// NkModelerAssets.h — UN FICHIER PAR ASSET, sur le disque.
//
// DECISION DE RIHEN (8 aout 2026), et sa raison, qui est la bonne :
//   « que le projet sauvegarde chaque fichier sur le disque, comme ca chaque
//     type de fichier n'embarque que les donnees relatives ; le projet ne
//     contient que des liens vers ses fichiers et la configuration globale. »
//
// CE QUE CA CORRIGE STRUCTURELLEMENT. Tant que tout vivait dans le `.nk3dm`,
// un noeud portait un NUMERO DE DOCUMENT, et il suffisait qu'un numero soit
// faux pour qu'un maillage de model atterrisse dans une scene -- c'est arrive
// deux fois en une soiree. Ici, un fichier de scene n'a pas de champ ou loger
// un maillage de model : la fuite n'est plus corrigee, elle est IMPOSSIBLE.
//
// ── LA DISPOSITION SUR LE DISQUE ─────────────────────────────────────────────
//   MonProjet/
//     MonProjet.nk3dm     le PROJET : configuration globale + arbre du
//                         navigateur, chaque carte portant le chemin RELATIF
//                         de son fichier. Aucune donnee d'asset.
//     <les dossiers que l'utilisateur cree dans le navigateur>
//       Scene.nkscene     une scene : SES noeuds, ses unites, sa vue
//       Model_02.nkmesh   un model : sa racine et SES maillages
//       Metal.nkmat       un materiau
//
// Aucune arborescence n'est imposee (CONVENTIONS_FICHIERS.md §5) : le chemin
// d'un fichier suit les DOSSIERS DU NAVIGATEUR, qui appartiennent a
// l'utilisateur.
//
// ── L'EN-TETE FAIT FOI ───────────────────────────────────────────────────────
// Chaque fichier commence par `application` / `nature` / `format`. L'extension
// se trie a l'oeil, mais un `.nkmat` dont l'en-tete annonce une scene est LU
// comme une scene et signale la discordance (regle du depot).
//
// ── CE QUI N'A PAS ENCORE DE FICHIER, ET POURQUOI ────────────────────────────
// Texture, graphe procedural, dataset : leurs cartes ne portent AUCUN contenu
// aujourd'hui (une texture n'a pas d'image tant que l'import n'existe pas). On
// n'ecrit pas un fichier vide pour faire nombre -- « aucun nom n'est grave
// avant le premier octet ecrit ». Ces cartes vivent dans l'arbre du `.nk3dm`,
// avec leur nom et leur place, et le diront a l'ecran.
// =============================================================================

#include "NK3DModeler/Project/NkModelerScene.h" // helpers NkSc* + lecteur HERITE

#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFileWatcher.h"
// <windows.h> definit GetFreeSpace en MACRO : arrivee avant cet en-tete, elle
// transforme la declaration de NkFileSystem::GetFreeSpace en charabia. Meme
// piege que GetObject ci-dessous, meme remede -- on la retire au plus pres de
// l'inclusion qui la rencontre, jamais globalement.
#ifdef GetFreeSpace
#undef GetFreeSpace
#endif
#include "NKFileSystem/NkFileSystem.h"
#include "NKMemory/NkMemory.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/JSON/NkJSONReader.h"

#ifdef GetObject
#undef GetObject
#endif

namespace nkentseu {
	namespace nk3d {

		// Version de format des FICHIERS D'ASSET, distincte de celle du projet :
		// un materiau evoluera a son rythme, et lier les deux obligerait a
		// invalider l'un pour l'autre.
		static const int32 kAssetFormatVersion = 1;
		// Version de la DISPOSITION du projet. 3 = un fichier par asset. Les
		// formats 1 et 2 (tout dans le .nk3dm) restent lisibles : ils sont relus
		// par NkSceneRestore, puis reecrits en fichiers separes a l'enregistrement
		// suivant. C'est une MIGRATION, pas une rupture -- personne ne doit perdre
		// son projet parce que la disposition a change.
		static const int32 kProjectLayoutVersion = 3;

		// ── EN-TETE COMMUN ──────────────────────────────────────────────────────
		inline void NkAsHeader(NkArchive &o, const char *nature) {
			o.SetString("application", "NK3DModeler");
			o.SetString("nature", nature);
			o.SetInt32("format", kAssetFormatVersion);
		}
		/// L'en-tete fait foi. Renvoie faux si le fichier annonce autre chose --
		/// l'appelant le journalise et passe, il ne lit pas de travers.
		inline bool NkAsNatureIs(const NkArchive &in, const char *nature) {
			NkString n;
			if (!in.GetString("nature", n))
				return false;
			return n == NkString(nature);
		}

		/// Extension d'une nature de carte, ou nullptr si elle n'a pas encore de
		/// contenu a ecrire. La table des extensions elle-meme est gravee dans
		/// CONVENTIONS_FICHIERS.md §2 ; ici on ne fait que designer celles dont le
		/// modeleur sait REELLEMENT ecrire le contenu aujourd'hui.
		inline const char *NkAsExtFor(uint8 kind) {
			if (kind == 5)
				return "nkscene";
			if (kind == 6)
				return "nkmesh";
			if (kind == 2)
				return "nkmat";
			return nullptr;
		}
		inline const char *NkAsNatureFor(uint8 kind) {
			if (kind == 5)
				return "scene";
			if (kind == 6)
				return "model";
			if (kind == 2)
				return "materiau";
			return "";
		}

		// ── CHEMIN RELATIF D'UNE CARTE ──────────────────────────────────────────
		// Il suit les DOSSIERS DU NAVIGATEUR : ce que l'utilisateur voit a l'ecran
		// est ce qu'il retrouve dans l'explorateur de son systeme. Une arborescence
		// imposee par le logiciel en ferait une seconde, concurrente de la sienne.
		inline NkString NkAsFolderPath(const NkModelerState &st, int32 folder) {
			NkString parts[8];
			int32 n = 0;
			int32 cur = folder;
			for (int32 g = 0; g < 8 && cur >= 0 && cur < st.browserCount; ++g) {
				if (st.browserKind[cur] != 1)
					break; // seul un DOSSIER fait un niveau de chemin
				parts[n++] = st.browserNames[cur];
				cur = st.browserParent[cur];
			}
			NkString out;
			for (int32 i = n - 1; i >= 0; --i) {
				out += parts[i];
				out += '/';
			}
			return out;
		}

		/// Un nom de carte devient un nom de FICHIER : les caracteres interdits par
		/// le systeme sont remplaces, jamais rejetes. Refuser un nom obligerait
		/// l'utilisateur a renommer sa scene pour pouvoir l'enregistrer.
		inline NkString NkAsSafeName(const char *name) {
			NkString out;
			for (const char *c = name; c && *c; ++c) {
				const char x = *c;
				const bool bad = (x == '/' || x == '\\' || x == ':' || x == '*' ||
								  x == '?' || x == '"' || x == '<' || x == '>' ||
								  x == '|' || (unsigned char)x < 32);
				out += bad ? '_' : x;
			}
			if (out.Empty())
				out = "SansNom";
			return out;
		}

		inline NkString NkAsRelFor(const NkModelerState &st, int32 card) {
			const char *ext = NkAsExtFor(st.browserKind[card]);
			if (!ext)
				return NkString();
			NkString rel = NkAsFolderPath(st, st.browserParent[card]);
			rel += NkAsSafeName(st.browserNames[card]);
			rel += '.';
			rel += ext;
			return rel;
		}

		// ── ECRITURE / LECTURE D'UN FICHIER ─────────────────────────────────────
		inline bool NkAsWrite(const NkString &root, const NkString &rel, const NkArchive &a,
							  NkString *err) {
			const NkString abs = NkScToAbs(root, rel.CStr());
			// Le dossier du navigateur peut ne pas exister encore sur le disque :
			// il n'est cree qu'au moment ou quelque chose s'y range.
			const NkString::SizeType s = abs.RFind('/');
			if (s != NkString::npos) {
				const NkString dir(abs.CStr(), s);
				if (!dir.Empty() && !NkDirectory::Exists(dir.CStr()) &&
					!NkDirectory::CreateRecursive(dir.CStr())) {
					if (err)
						*err = NkString("dossier impossible a creer : ") + dir;
					return false;
				}
			}
			const NkString json = NkJSONWriter::WriteArchive(a, true, 2);
			if (!NkFile::WriteAllText(abs.CStr(), json.CStr())) {
				if (err)
					*err = NkString("ecriture impossible : ") + abs;
				return false;
			}
			return true;
		}

		inline bool NkAsRead(const NkString &root, const char *rel, NkArchive &out) {
			const NkString abs = NkScToAbs(root, rel);
			if (abs.Empty() || !NkFile::Exists(abs.CStr()))
				return false;
			const NkString text = NkFile::ReadAllText(abs.CStr());
			NkString perr;
			return NkJSONReader::ReadArchive(NkStringView(text.CStr()), out, &perr);
		}

		// ─────────────────────────────────────────────────────────────────────────
		// MATERIAUX
		// ─────────────────────────────────────────────────────────────────────────
		/// LES DEUX SENS DU LIEN CARTE <-> EMPLACEMENT. Un materiau du projet sans
		/// carte serait invisible dans le navigateur ; une carte sans emplacement
		/// serait un nom sans matiere -- « tout ce qui est fichier est un asset
		/// reel » (Rihen). On repare donc les deux manques au meme endroit.
		inline void NkBrowserSyncMats(NkModelerState &st) {
			const int32 matMax = demo::Demo3DHostProjMatMax();
			// 1. chaque emplacement occupe a sa carte
			for (int32 m = 0; m < matMax; ++m) {
				char nm[64];
				float32 alb[3];
				float32 rough = 0.f, metal = 0.f;
				if (!demo::Demo3DHostProjMatInfo(m, nm, (uint32)sizeof(nm), alb, &rough, &metal))
					continue;
				bool has = false;
				for (int32 b = 0; b < st.browserCount && !has; ++b)
					has = (st.browserKind[b] == 2 && st.browserMat[b] == m + 1);
				if (has || st.browserCount >= NkModelerState::kMaxBrowser)
					continue;
				const int32 c = st.browserCount++;
				st.browserKind[c] = 2;
				st.browserParent[c] = -1;
				st.browserSub[c] = 0;
				st.browserDoc[c] = 0;
				st.browserSrcNode[c] = 0;
				st.browserMat[c] = m + 1;
				st.browserFile[c][0] = 0;
				NkScPut(st.browserNames[c], (uint32)sizeof(st.browserNames[0]), nm);
			}
			// 2. chaque carte a son emplacement
			for (int32 b = 0; b < st.browserCount; ++b) {
				if (st.browserKind[b] != 2 || st.browserMat[b] > 0)
					continue;
				const int32 slot = demo::Demo3DHostProjMatCreate();
				if (slot < 0)
					continue; // plus d'emplacement : la carte reste sans matiere
				st.browserMat[b] = slot + 1;
				demo::Demo3DHostProjMatSetName(slot, st.browserNames[b]);
			}
		}

		/// Chemin relatif du `.nkmat` d'un emplacement, ou chaine vide. C'est par
		/// ce chemin qu'un noeud designe son materiau : un RANG n'aurait plus de
		/// sens des lors que chaque materiau vit dans son propre fichier.
		inline NkString NkAsMatPath(const NkModelerState &st, int32 slot) {
			if (slot < 0)
				return NkString();
			for (int32 b = 0; b < st.browserCount; ++b)
				if (st.browserKind[b] == 2 && st.browserMat[b] == slot + 1)
					return NkAsRelFor(st, b);
			return NkString();
		}
		inline int32 NkAsMatSlot(const NkModelerState &st, const NkString &rel) {
			if (rel.Empty())
				return -1;
			for (int32 b = 0; b < st.browserCount; ++b) {
				if (st.browserKind[b] != 2 || st.browserMat[b] <= 0)
					continue;
				if (NkString(st.browserFile[b]) == rel || NkAsRelFor(st, b) == rel)
					return st.browserMat[b] - 1;
			}
			return -1;
		}

		inline void NkAsMatCapture(NkArchive &o, const NkString &root, int32 slot) {
			NkAsHeader(o, "materiau");
			char nm[64];
			float32 alb[3] = {0.7f, 0.7f, 0.7f};
			float32 rough = 0.85f, metal = 0.f;
			if (!demo::Demo3DHostProjMatInfo(slot, nm, (uint32)sizeof(nm), alb, &rough, &metal))
				return;
			o.SetString("nom", nm);
			NkScSetVec3(o, "albedo", alb);
			o.SetFloat32("rugosite", rough);
			o.SetFloat32("metallique", metal);
			// Physique de surface (2026-08-09). Ecrites MEME a zero : un fichier
			// qui tait un champ oblige le lecteur a deviner le defaut.
			{
				float32 cc = 0.f, ccR = 0.f, sss = 0.f;
				demo::Demo3DHostProjMatSurface(slot, &cc, &ccR, &sss);
				o.SetFloat32("vernis", cc);
				o.SetFloat32("vernisRugosite", ccR);
				o.SetFloat32("diffusion", sss);
			}
			float32 nrm = 1.f, emiS = 1.f;
			demo::Demo3DHostProjMatChanStrength(slot, &nrm, &emiS);
			o.SetFloat32("relief", nrm);
			o.SetFloat32("emissifIntensite", emiS);
			float32 emi[3] = {0.f, 0.f, 0.f};
			demo::Demo3DHostProjMatEmissive(slot, emi);
			NkScSetVec3(o, "emissif", emi);
			o.SetInt32("apercu", demo::Demo3DHostProjMatPrevShape(slot));
			// LES CANAUX, indexes et non nommes un a un : la table des canaux vit
			// dans l'hote, la recopier ici la ferait diverger au premier ajout.
			NkArchive maps;
			const int32 chan = demo::Demo3DHostMatChanCount();
			for (int32 c = 0; c < chan; ++c) {
				char key[16];
				snprintf(key, sizeof(key), "c%d", (int)c);
				maps.SetString(key, NkScToRel(root, demo::Demo3DHostProjMatMap(slot, c)).CStr());
			}
			o.SetObject("cartes", maps);
		}

		inline void NkAsMatRestore(const NkArchive &in, const NkString &root, int32 slot,
								   int32 *texMiss) {
			const NkString nm = NkScStr(in, "nom");
			if (!nm.Empty())
				demo::Demo3DHostProjMatSetName(slot, nm.CStr());
			float32 alb[3];
			NkScGetVec3(in, "albedo", alb, 0.7f, 0.7f, 0.7f);
			demo::Demo3DHostProjMatSetParams(slot, alb, NkScFloat(in, "rugosite", 0.85f),
											 NkScFloat(in, "metallique", 0.f));
			// Physique de surface : defaut 0 — un .nkmat ecrit avant le 9 aout
			// n'a pas ces champs, et un materiau sans vernis ni diffusion est
			// exactement ce qu'il decrivait.
			demo::Demo3DHostProjMatSetSurface(slot, NkScFloat(in, "vernis", 0.f),
											  NkScFloat(in, "vernisRugosite", 0.f),
											  NkScFloat(in, "diffusion", 0.f));
			// LES INTENSITES AVANT LES CARTES : poser une normal map relit
			// l'intensite de relief au moment ou elle est posee. Dans l'autre ordre,
			// la carte serait branchee avec l'ancienne valeur.
			demo::Demo3DHostProjMatSetChanStrength(slot, NkScFloat(in, "relief", 1.f),
												   NkScFloat(in, "emissifIntensite", 1.f));
			float32 emi[3];
			NkScGetVec3(in, "emissif", emi, 0.f, 0.f, 0.f);
			demo::Demo3DHostProjMatSetEmissive(slot, emi);
			demo::Demo3DHostProjMatSetPrevShape(slot, NkScInt(in, "apercu", 1));
			NkArchive maps;
			if (!in.GetObject("cartes", maps))
				return;
			const int32 chan = demo::Demo3DHostMatChanCount();
			for (int32 c = 0; c < chan; ++c) {
				char key[16];
				snprintf(key, sizeof(key), "c%d", (int)c);
				const NkString rel = NkScStr(maps, key);
				if (rel.Empty())
					continue;
				// LE CHARGEMENT FAIT FOI : une texture introuvable n'est pas
				// memorisee. On la COMPTE, pour le dire -- un materiau qui perd sa
				// carte en silence passe pour un materiau mal regle.
				if (!demo::Demo3DHostProjMatSetMap(slot, c, NkScToAbs(root, rel.CStr()).CStr()) &&
					texMiss)
					++*texMiss;
			}
		}

		// ─────────────────────────────────────────────────────────────────────────
		// NOEUDS — partages par la scene et le model
		//
		// Les rangs sont LOCAUX AU FICHIER. Un fichier de scene ne peut donc pas
		// designer un noeud d'un autre fichier : c'est ce qui rend la fuite de
		// donnees impossible plutot que corrigee.
		// ─────────────────────────────────────────────────────────────────────────
		inline void NkAsNodesCapture(NkArchive &out, const NkString &root,
									 const NkModelerState &st, const NkVector<int32> &live) {
			const int32 nodeMax = demo::Demo3DHostNodeCount();
			NkVector<int32> rankOf;
			for (int32 n = 0; n < nodeMax; ++n)
				rankOf.PushBack(-1);
			for (usize k = 0; k < live.Size(); ++k)
				rankOf[(usize)live[k]] = (int32)k;

			NkVector<NkArchive> nodes;
			for (usize k = 0; k < live.Size(); ++k) {
				const int32 n = live[k];
				const int32 kind = demo::Demo3DHostUserKind(n);
				const int32 sub = demo::Demo3DHostUserSub(n);
				NkArchive nd;
				nd.SetInt32("nature", kind);
				nd.SetInt32("sousType", sub);
				nd.SetString("nom", (n < 176) ? st.customNames[n] : "");
				// PARENTE par RANG DANS CE FICHIER. Un parent hors du fichier (un
				// objet de la scene de demonstration) garde son numero brut : ceux-la
				// sont fixes, ils ne se recyclent pas.
				const int32 p = demo::Demo3DHostNodeParent(n);
				const int32 pr = (p >= 0 && p < nodeMax) ? rankOf[(usize)p] : -1;
				nd.SetInt32("parent", pr);
				if (p >= 0 && pr < 0)
					nd.SetInt32("parentFixe", p);
				// T + R + S SEPARES : jamais de matrice (le cisaillement n'est pas
				// representable, il se perdrait en silence).
				float32 pos[3], rot[3], scl[3];
				if (demo::Demo3DHostEmptyTransform(n, pos, rot, scl)) {
					NkScSetVec3(nd, "position", pos);
					NkScSetVec3(nd, "rotation", rot);
					NkScSetVec3(nd, "echelle", scl);
				}
				nd.SetBool("masque", demo::Demo3DHostObjectHidden(n));
				nd.SetBool("verrouille", demo::Demo3DHostObjectLocked(n));
				nd.SetBool("horsRendu", demo::Demo3DHostNodeNoRender(n));
				nd.SetInt32("transmission", demo::Demo3DHostNodeXmitMask(n));
				nd.SetBool("maillageInterne", demo::Demo3DHostNodeIsMesh(n));
				nd.SetBool("model", demo::Demo3DHostNodeIsModel(n));
				// MATERIAU par CHEMIN de son fichier : chaque materiau vit
				// desormais dans le sien, un rang ne designerait plus rien.
				nd.SetString("materiau",
							 NkAsMatPath(st, demo::Demo3DHostProjMatOf(n)).CStr());
				// PARAMETRES DE CREATION : sans eux, une sphere rechargee ne pourrait
				// plus etre reajustee -- son maillage serait la, mais le panneau
				// « Ajuster la creation » n'aurait plus rien a montrer.
				int32 segs = 0, rings = 0;
				float32 aux = 0.f;
				if (demo::Demo3DHostMeshParams(n, &segs, &rings, &aux)) {
					NkArchive cr;
					cr.SetInt32("segments", segs);
					cr.SetInt32("anneaux", rings);
					cr.SetFloat32("aux", aux);
					nd.SetObject("creation", cr);
				}
				if (kind == 5) { // LUMIERE — le TYPE est le sous-type, deja ecrit
					NkArchive li;
					float32 col[3] = {1.f, 1.f, 1.f}, inten = 8.f;
					if (demo::Demo3DHostUserLightParams(n, col, &inten)) {
						NkScSetVec3(li, "couleur", col);
						li.SetFloat32("intensite", inten);
					}
					float32 range = 10.f, inner = 25.f, outer = 35.f, aw = 1.f, ah = 1.f;
					bool shadow = true;
					int32 ltype = 0;
					if (demo::Demo3DHostLightEx(n, &range, &inner, &outer, &aw, &ah, &shadow,
												&ltype)) {
						li.SetFloat32("portee", range);
						li.SetFloat32("angleInterne", inner);
						li.SetFloat32("angleExterne", outer);
						li.SetFloat32("largeur", aw);
						li.SetFloat32("hauteur", ah);
						li.SetBool("ombre", shadow);
						// Loi d'attenuation (2026-08-09) : 0 heritee, 1 physique.
						li.SetInt32("loi", demo::Demo3DHostLightAttMode(n));
					}
					float32 tempK = 0.f, expo = 0.f;
					if (demo::Demo3DHostLightTempExp(n, &tempK, &expo)) {
						li.SetFloat32("temperature", tempK);
						li.SetFloat32("exposition", expo);
					}
					li.SetInt32("cookie", demo::Demo3DHostLightCookie(n));
					nd.SetObject("lumiere", li);
				}
				// CAMERA (un vide de sous-type 10). La FOCALE en millimetres n'est pas
				// ecrite : elle se DEDUIT du champ et du capteur -- l'ecrire donnerait
				// deux verites pour une meme grandeur.
				if (kind == 4 && sub == 10) {
					NkArchive ca;
					float32 fov = 50.f, nearC = 0.1f, farC = 100.f;
					if (demo::Demo3DHostCameraParams(n, &fov, &nearC, &farC)) {
						ca.SetFloat32("fov", fov);
						ca.SetFloat32("clipDebut", nearC);
						ca.SetFloat32("clipFin", farC);
					}
					ca.SetBool("ortho", demo::Demo3DHostCamOrtho(n));
					ca.SetBool("focaleEnMM", demo::Demo3DHostCamLensMM(n));
					ca.SetFloat32("capteur", demo::Demo3DHostCamSensor(n));
					ca.SetInt32("guides", demo::Demo3DHostCamGuides(n));
					float32 pa[4] = {0.f, 0.f, 0.f, 0.6f};
					demo::Demo3DHostCamPasse(n, pa);
					NkArchive pv;
					pv.SetFloat32("r", pa[0]);
					pv.SetFloat32("v", pa[1]);
					pv.SetFloat32("b", pa[2]);
					pv.SetFloat32("a", pa[3]);
					ca.SetObject("passePartout", pv);
					nd.SetObject("camera", ca);
				}
				nodes.PushBack(nd);
			}
			out.SetObjectArray("noeuds", nodes);
			(void)root;
		}

		/// Recree les noeuds d'un fichier DANS LA SCENE HOTE ACTIVE. L'appelant a
		/// donc pose `Demo3DHostSetActiveScene` avant : ici on ne decide pas de
		/// l'appartenance, on la subit -- c'est exactement ce qui empeche un
		/// maillage de model de se retrouver dans une scene.
		/// Un maillage interne SANS MODEL ANCETRE est inatteignable : la hierarchie
		/// cache les maillages internes hors d'un editeur de model (NkHierNodeSkip),
		/// alors que la vue 3D les dessine. Rihen le decrit exactement ainsi --
		/// « il y a un model dans la vue 3D qui n'est pas dans la hierarchie, et
		/// quand je supprime ca ne supprime pas » : on ne peut pas le selectionner,
		/// donc pas l'effacer.
		///
		/// On ne le SUPPRIME PAS -- rien ne disparait sans un mot. On lui retire sa
		/// qualite de maillage interne : il redevient un objet ordinaire, visible,
		/// selectionnable, effacable. L'utilisateur decide.
		inline int32 NkAsRepairOrphanMeshes(const NkVector<int32> &made) {
			int32 fixed = 0;
			for (usize i = 0; i < made.Size(); ++i) {
				const int32 n = made[i];
				if (n < 0 || !demo::Demo3DHostNodeIsMesh(n))
					continue;
				bool owned = false;
				int32 cur = demo::Demo3DHostNodeParent(n);
				for (int32 g = 0; g < 256 && cur >= 0 && !owned; ++g) {
					owned = demo::Demo3DHostNodeIsModel(cur);
					cur = demo::Demo3DHostNodeParent(cur);
				}
				if (!owned) {
					demo::Demo3DHostSetNodeIsMesh(n, false);
					++fixed;
				}
			}
			return fixed;
		}

		inline void NkAsNodesRestore(const NkArchive &in, const NkString &root,
									 NkModelerState &st, bool archive, int32 *nodeMiss,
									 NkVector<int32> *outNodes, int32 *orphanFix = nullptr) {
			NkVector<NkArchive> nodes;
			(void)in.GetObjectArray("noeuds", nodes);
			NkVector<int32> nodeOf;
			for (usize i = 0; i < nodes.Size(); ++i) {
				const NkArchive &nd = nodes[i];
				const int32 kind = NkScInt(nd, "nature", 0);
				const int32 sub = NkScInt(nd, "sousType", 0);
				if (kind < 1 || kind > 10) {
					nodeOf.PushBack(-1);
					if (nodeMiss)
						++*nodeMiss;
					continue;
				}
				const int32 n = demo::Demo3DHostAddNode(kind, sub);
				nodeOf.PushBack(n);
				if (n < 0) {
					if (nodeMiss)
						++*nodeMiss; // plus d'emplacement libre
					continue;
				}
				const NkString nm = NkScStr(nd, "nom");
				if (n < 176)
					NkScPut(st.customNames[n], (uint32)sizeof(st.customNames[0]), nm.CStr());
				// L'hote garde une copie du nom : c'est lui qui nomme les fichiers
				// produits par la sortie.
				demo::Demo3DHostSetNodeLabel(n, nm.CStr());
				NkArchive cr;
				if (nd.GetObject("creation", cr))
					demo::Demo3DHostSetMeshParams(n, NkScInt(cr, "segments", 32),
												  NkScInt(cr, "anneaux", 16),
												  NkScFloat(cr, "aux", 0.15f));
				float32 pos[3], rot[3], scl[3];
				NkScGetVec3(nd, "position", pos, 0.f, 0.f, 0.f);
				NkScGetVec3(nd, "rotation", rot, 0.f, 0.f, 0.f);
				NkScGetVec3(nd, "echelle", scl, 1.f, 1.f, 1.f);
				demo::Demo3DHostSetEmptyTransform(n, pos, rot, scl);
				demo::Demo3DHostSetObjectHidden(n, NkScBool(nd, "masque", false));
				demo::Demo3DHostSetObjectLocked(n, NkScBool(nd, "verrouille", false));
				demo::Demo3DHostSetNodeNoRender(n, NkScBool(nd, "horsRendu", false));
				demo::Demo3DHostSetNodeXmitMask(n, NkScInt(nd, "transmission", 7));
				demo::Demo3DHostSetNodeIsMesh(n, NkScBool(nd, "maillageInterne", false));
				demo::Demo3DHostSetNodeIsModel(n, NkScBool(nd, "model", false));
				NkArchive li;
				if (kind == 5 && nd.GetObject("lumiere", li)) {
					float32 col[3];
					NkScGetVec3(li, "couleur", col, 1.f, 1.f, 1.f);
					demo::Demo3DHostSetUserLightParams(n, col, NkScFloat(li, "intensite", 8.f));
					demo::Demo3DHostSetLightEx(n, NkScFloat(li, "portee", 10.f),
											   NkScFloat(li, "angleInterne", 25.f),
											   NkScFloat(li, "angleExterne", 35.f),
											   NkScFloat(li, "largeur", 1.f),
											   NkScFloat(li, "hauteur", 1.f),
											   NkScBool(li, "ombre", true));
					demo::Demo3DHostSetLightTempExp(n, NkScFloat(li, "temperature", 0.f),
													NkScFloat(li, "exposition", 0.f));
					demo::Demo3DHostSetLightCookie(n, NkScInt(li, "cookie", -1));
					// Loi d'attenuation : defaut 0 (heritee) — un fichier ecrit
					// avant le 9 aout decrivait exactement ce comportement.
					demo::Demo3DHostSetLightAttMode(n, NkScInt(li, "loi", 0));
				}
				NkArchive ca;
				if (kind == 4 && sub == 10 && nd.GetObject("camera", ca)) {
					demo::Demo3DHostSetCameraParams(n, NkScFloat(ca, "fov", 50.f),
													NkScFloat(ca, "clipDebut", 0.1f),
													NkScFloat(ca, "clipFin", 100.f));
					demo::Demo3DHostSetCamOrtho(n, NkScBool(ca, "ortho", false));
					demo::Demo3DHostSetCamLensMM(n, NkScBool(ca, "focaleEnMM", false));
					demo::Demo3DHostSetCamSensor(n, NkScFloat(ca, "capteur", 36.f));
					demo::Demo3DHostSetCamGuides(n, NkScInt(ca, "guides", 0));
					NkArchive pv;
					if (ca.GetObject("passePartout", pv)) {
						const float32 pa[4] = {NkScFloat(pv, "r", 0.f), NkScFloat(pv, "v", 0.f),
											   NkScFloat(pv, "b", 0.f), NkScFloat(pv, "a", 0.6f)};
						demo::Demo3DHostSetCamPasse(n, pa);
					}
				}
			}
			// PARENTES ET MATERIAUX EN SECONDE PASSE : un parent peut etre ecrit
			// APRES son enfant -- le resoudre au vol echouerait une fois sur deux.
			for (usize i = 0; i < nodes.Size(); ++i) {
				if (i >= nodeOf.Size() || nodeOf[i] < 0)
					continue;
				const int32 n = nodeOf[i];
				const NkArchive &nd = nodes[i];
				const int32 pr = NkScInt(nd, "parent", -1);
				const int32 pf = NkScInt(nd, "parentFixe", -1);
				if (pr >= 0 && (usize)pr < nodeOf.Size() && nodeOf[(usize)pr] >= 0)
					(void)demo::Demo3DHostSetNodeParent(n, nodeOf[(usize)pr]);
				else if (pf >= 0)
					(void)demo::Demo3DHostSetNodeParent(n, pf);
				const int32 ms = NkAsMatSlot(st, NkScStr(nd, "materiau"));
				if (ms >= 0)
					demo::Demo3DHostProjMatAssign(n, ms);
			}
			// LA REPARATION APRES LES PARENTES : elle a besoin de l'arbre complet
			// pour savoir si un maillage a un model au-dessus de lui. Elle ne vaut
			// que pour une SCENE : dans un fichier de model, les maillages ont leur
			// racine par construction.
			if (!archive) {
				const int32 f = NkAsRepairOrphanMeshes(nodeOf);
				if (orphanFix)
					*orphanFix += f;
			}
			// L'ARCHIVAGE EN TOUT DERNIER : les noeuds ont ete construits comme des
			// noeuds ordinaires pour recevoir leur parente -- c'est elle qui tient un
			// model et ses maillages ensemble. Les retirer plus tot ferait travailler
			// la passe de parente sur un noeud absent.
			if (archive)
				for (usize i = 0; i < nodeOf.Size(); ++i)
					if (nodeOf[i] >= 0)
						demo::Demo3DHostSetNodeArchived(nodeOf[i], true);
			if (outNodes)
				*outNodes = nodeOf;
			(void)root;
		}

		// ─────────────────────────────────────────────────────────────────────────
		// UNE SCENE — .nkscene
		// ─────────────────────────────────────────────────────────────────────────
		/// Capture l'etat VIVANT des pastilles globales (rendu, environnement,
		/// sortie) dans `o`. Extrait de NkAsSceneCapture pour servir aussi les
		/// bascules d'onglet (reglages PAR SCENE, Rihen 10 aout).
		inline void NkAsRenduCapture(NkArchive &o, const NkString &root) {
			// LES REGLAGES DU PANNEAU RENDU VOYAGENT AVEC LA SCENE (Rihen,
			// 9 aout : ils n'etaient pas restaures au rechargement). Ils sont
			// GLOBAUX a la vue 3D — la derniere scene restauree les pose, ce
			// qui est le comportement attendu d'un projet a une scene.
			{
				NkArchive r;
				r.SetFloat32("ambiance", demo::Demo3DHostAmbient());
				float32 ac[3] = {1.f, 1.f, 1.f};
				demo::Demo3DHostAmbientColor(ac);
				NkScSetVec3(r, "ambianceCouleur", ac);
				bool sOn = false;
				float32 sc3[3] = {0.f, 0.f, 0.f}, sy = 0.f, sr = 0.f, stl = 1.f, smt = 0.f;
				int32 sp = 0;
				demo::Demo3DHostFloor(&sOn, sc3, &sy, &sr, &sp, &stl, &smt);
				NkArchive sol;
				sol.SetBool("actif", sOn);
				NkScSetVec3(sol, "couleur", sc3);
				sol.SetFloat32("hauteur", sy);
				sol.SetFloat32("rugosite", sr);
				sol.SetInt32("motif", sp);
				sol.SetFloat32("carreau", stl);
				sol.SetFloat32("metallique", smt);
				r.SetObject("sol", sol);
				bool bOn = false;
				float32 bc[3] = {0.f, 0.f, 0.f}, bd = 0.f, bs = 0.f, be = 0.f;
				int32 bm = 0;
				demo::Demo3DHostFog(&bOn, bc, &bd, &bs, &be, &bm);
				NkArchive br;
				br.SetBool("actif", bOn);
				NkScSetVec3(br, "couleur", bc);
				br.SetFloat32("densite", bd);
				br.SetFloat32("debut", bs);
				br.SetFloat32("fin", be);
				br.SetInt32("loi", bm);
				float32 gb = 0.f, gt = 0.f, gw = 0.f;
				bool gc = true;
				demo::Demo3DHostFogGround(&gb, &gt, &gw, &gc);
				br.SetFloat32("nappeAltitude", gb);
				br.SetFloat32("nappeEpaisseur", gt);
				br.SetFloat32("nappeSouffle", gw);
				br.SetBool("nappeSuitNuages", gc);
				r.SetObject("brouillard", br);
				float32 onb = 0.f, osb = 0.f, oso = 0.f;
				int32 oq = 1;
				if (demo::Demo3DHostShadowCfg(&onb, &osb, &oso, &oq)) {
					NkArchive om;
					om.SetFloat32("biaisNormal", onb);
					om.SetFloat32("biaisPente", osb);
					om.SetFloat32("douceur", oso);
					om.SetInt32("qualite", oq);
					r.SetObject("ombres", om);
				}
				bool aoOn = false;
				float32 aoR = 0.5f, aoI = 1.f;
				demo::Demo3DHostSSAO(&aoOn, &aoR, &aoI);
				NkArchive ao;
				ao.SetBool("actif", aoOn);
				ao.SetFloat32("rayon", aoR);
				ao.SetFloat32("intensite", aoI);
				r.SetObject("occlusion", ao);
				float32 pe = 1.f, pt = 0.85f, ps = 1.5f;
				bool pb = true;
				demo::Demo3DHostPostFx(&pe, &pb, &pt, &ps);
				NkArchive fx;
				fx.SetFloat32("exposition", pe);
				fx.SetBool("bloom", pb);
				fx.SetFloat32("seuil", pt);
				fx.SetFloat32("intensite", ps);
				r.SetObject("expositionBloom", fx);
				o.SetObject("rendu", r);
			}
			// ── ENVIRONNEMENT (pastille ciel/monde) — meme regle que « rendu » :
			// tout ce que le panneau propose voyage avec la scene (Rihen, 10 aout).
			{
				NkArchive e;
				e.SetBool("cielVisible", demo::Demo3DHostSkyVisible());
				e.SetFloat32("cielIntensite", demo::Demo3DHostSkyIntensity());
				e.SetInt32("modele", demo::Demo3DHostSkyModel());
				e.SetBool("ambianceParEnv", demo::Demo3DHostAmbientUseEnv());
				float32 sd[3] = {0.f, -1.f, 0.f};
				float32 turb = 2.5f, sunI = 1.f;
				bool disc = true;
				demo::Demo3DHostSkySun(sd, &turb, &disc, &sunI);
				NkScSetVec3(e, "soleilDirection", sd);
				e.SetFloat32("turbidite", turb);
				e.SetBool("disque", disc);
				e.SetFloat32("soleilIntensite", sunI);
				float32 sc[3] = {1.f, 1.f, 1.f};
				demo::Demo3DHostSkySunColor(sc);
				NkScSetVec3(e, "soleilCouleur", sc);
				e.SetBool("soleilEclaire", demo::Demo3DHostSkySunLightsScene());
				e.SetInt32("soleilSource", demo::Demo3DHostSkySunSource());
				e.SetFloat32("etoileTemperature", demo::Demo3DHostSkyAlienTemp());
				e.SetFloat32("nuagesVitesse", demo::Demo3DHostSkyCloudSpeed());
				float32 stI = 0.f, stD = 0.f;
				demo::Demo3DHostSkyStars(&stI, &stD);
				e.SetFloat32("etoilesIntensite", stI);
				e.SetFloat32("etoilesDensite", stD);
				float32 rot = 0.f, sht = 0.f;
				demo::Demo3DHostSkyStarMotion(&rot, &sht);
				e.SetFloat32("cielRotation", rot);
				e.SetFloat32("filantes", sht);
				bool cOn = false;
				float32 cCov = 0.f, cDen = 0.f, cScl = 0.f, cCol[3] = {1.f, 1.f, 1.f};
				demo::Demo3DHostSkyClouds(&cOn, &cCov, &cDen, &cScl, cCol);
				NkArchive nu;
				nu.SetBool("actif", cOn);
				nu.SetFloat32("couverture", cCov);
				nu.SetFloat32("densite", cDen);
				nu.SetFloat32("echelle", cScl);
				NkScSetVec3(nu, "couleur", cCol);
				e.SetObject("nuages", nu);
				const int32 nM = demo::Demo3DHostSkyMoonCount();
				e.SetInt32("lunes", nM);
				for (int32 m = 0; m < nM && m < 2; ++m) {
					float32 el = 0.f, az = 0.f, sz = 0.f, br = 0.f, mc[3] = {1.f, 1.f, 1.f};
					demo::Demo3DHostSkyMoon(m, &el, &az, &sz, &br, mc);
					bool man = false;
					float32 ph = 0.f;
					demo::Demo3DHostSkyMoonPhase(m, &man, &ph);
					NkArchive lu;
					lu.SetFloat32("elevation", el);
					lu.SetFloat32("azimut", az);
					lu.SetFloat32("taille", sz);
					lu.SetFloat32("eclat", br);
					NkScSetVec3(lu, "couleur", mc);
					lu.SetBool("phaseManuelle", man);
					lu.SetFloat32("phase", ph);
					char k[8];
					snprintf(k, sizeof(k), "l%d", (int)m);
					e.SetObject(k, lu);
				}
				float32 top[3], hor[3], gnd[3];
				demo::Demo3DHostEnvSky(top, hor, gnd);
				NkScSetVec3(e, "degradeHaut", top);
				NkScSetVec3(e, "degradeHorizon", hor);
				NkScSetVec3(e, "degradeSol", gnd);
				e.SetString("hdr", NkScToRel(root, demo::Demo3DHostHdrPath()).CStr());
				o.SetObject("environnement", e);
			}
			// ── SORTIE (pastille Output) : resolution, destination, incrustations.
			{
				NkArchive so;
				int32 src = -1, w = 1920, h = 1080, pct = 100, fmt = 0;
				bool tr = false;
				demo::Demo3DHostOutMain(&src, &w, &h, &pct, &fmt, &tr);
				so.SetInt32("source", src);
				so.SetInt32("largeur", w);
				so.SetInt32("hauteur", h);
				so.SetInt32("pourcent", pct);
				so.SetInt32("format", fmt);
				so.SetBool("transparent", tr);
				so.SetString("dossier", demo::Demo3DHostOutDir());
				so.SetString("nom", demo::Demo3DHostOutName());
				so.SetString("nomVue", demo::Demo3DHostCaptureName(1));
				so.SetString("nomTutoriel", demo::Demo3DHostCaptureName(2));
				const int32 im = demo::Demo3DHostOutInsetMax();
				for (int32 i = 0; i < im; ++i) {
					int32 isrc = -1, ish = 0;
					float32 xy[2] = {0.f, 0.f}, szv[2] = {0.f, 0.f}, bord = 0.f;
					float32 bc[3] = {1.f, 1.f, 1.f}, op = 1.f;
					if (!demo::Demo3DHostOutInset(i, &isrc, &ish, xy, szv, &bord, bc, &op))
						continue;
					NkArchive inc;
					inc.SetInt32("source", isrc);
					inc.SetInt32("forme", ish);
					inc.SetFloat32("x", xy[0]);
					inc.SetFloat32("y", xy[1]);
					inc.SetFloat32("largeur", szv[0]);
					inc.SetFloat32("hauteur", szv[1]);
					inc.SetFloat32("lisere", bord);
					NkScSetVec3(inc, "lisereCouleur", bc);
					inc.SetFloat32("opacite", op);
					inc.SetBool("fichierPropre", demo::Demo3DHostOutInsetOwnFile(i));
					inc.SetBool("fichierForme", demo::Demo3DHostOutInsetOwnShaped(i));
					char k[8];
					snprintf(k, sizeof(k), "i%d", (int)i);
					so.SetObject(k, inc);
				}
				o.SetObject("sortie", so);
			}
		}

		/// Applique a l'hote les blocs presents dans `in` (absent = intouche).
		inline void NkAsRenduRestore(const NkArchive &in, const NkString &root,
									 NkModelerState &st) {
			// LES REGLAGES DU PANNEAU RENDU, si la scene les porte. Un fichier
			// anterieur au 9 aout n'a pas ce bloc : on ne touche a RIEN (les
			// valeurs courantes restent), au lieu d'imposer des defauts.
			{
				NkArchive r;
				if (in.GetObject("rendu", r)) {
					demo::Demo3DHostSetAmbient(NkScFloat(r, "ambiance", demo::Demo3DHostAmbient()));
					float32 ac[3];
					NkScGetVec3(r, "ambianceCouleur", ac, 1.f, 1.f, 1.f);
					demo::Demo3DHostSetAmbientColor(ac);
					NkArchive sol;
					if (r.GetObject("sol", sol)) {
						float32 sc3[3];
						NkScGetVec3(sol, "couleur", sc3, 0.55f, 0.55f, 0.55f);
						demo::Demo3DHostSetFloor(
							NkScBool(sol, "actif", false), sc3, NkScFloat(sol, "hauteur", 0.f),
							NkScFloat(sol, "rugosite", 0.9f), NkScInt(sol, "motif", 0),
							NkScFloat(sol, "carreau", 1.f), NkScFloat(sol, "metallique", 0.f));
					}
					NkArchive br;
					if (r.GetObject("brouillard", br)) {
						float32 bc[3];
						NkScGetVec3(br, "couleur", bc, 0.65f, 0.71f, 0.78f);
						demo::Demo3DHostSetFog(NkScBool(br, "actif", false), bc,
											   NkScFloat(br, "densite", 0.02f),
											   NkScFloat(br, "debut", 10.f),
											   NkScFloat(br, "fin", 60.f), NkScInt(br, "loi", 0));
						demo::Demo3DHostSetFogGround(NkScFloat(br, "nappeAltitude", 0.f),
													 NkScFloat(br, "nappeEpaisseur", 0.f),
													 NkScFloat(br, "nappeSouffle", 0.f),
													 NkScBool(br, "nappeSuitNuages", true));
					}
					NkArchive om;
					if (r.GetObject("ombres", om)) {
						demo::Demo3DHostSetShadowCfg(NkScFloat(om, "biaisNormal", 0.f),
													 NkScFloat(om, "biaisPente", 0.f),
													 NkScFloat(om, "douceur", 0.002f),
													 NkScInt(om, "qualite", 1));
						st.shadowQual = NkScInt(om, "qualite", 1);
					}
					NkArchive ao;
					if (r.GetObject("occlusion", ao)) {
						demo::Demo3DHostSetSSAO(NkScBool(ao, "actif", false),
												NkScFloat(ao, "rayon", 0.5f),
												NkScFloat(ao, "intensite", 1.f));
					}
					NkArchive fx;
					if (r.GetObject("expositionBloom", fx)) {
						demo::Demo3DHostSetPostFx(NkScFloat(fx, "exposition", 1.f),
												  NkScBool(fx, "bloom", true),
												  NkScFloat(fx, "seuil", 0.85f),
												  NkScFloat(fx, "intensite", 1.5f));
					}
				}
			}
			// ── ENVIRONNEMENT, si la scene le porte. La regeneration du ciel
			// (ApplySky) se fait UNE fois a la fin : c'est une convolution CPU.
			{
				NkArchive e;
				if (in.GetObject("environnement", e)) {
					demo::Demo3DHostSetSkyVisible(NkScBool(e, "cielVisible", true));
					demo::Demo3DHostSetSkyIntensity(NkScFloat(e, "cielIntensite", 1.f));
					demo::Demo3DHostSetSkyModel(NkScInt(e, "modele", 0));
					demo::Demo3DHostSetAmbientUseEnv(NkScBool(e, "ambianceParEnv", false));
					float32 sd[3];
					NkScGetVec3(e, "soleilDirection", sd, 0.f, -1.f, 0.f);
					demo::Demo3DHostSetSkySun(sd, NkScFloat(e, "turbidite", 2.5f),
											  NkScBool(e, "disque", true),
											  NkScFloat(e, "soleilIntensite", 1.f));
					float32 sc[3];
					NkScGetVec3(e, "soleilCouleur", sc, 1.f, 1.f, 1.f);
					demo::Demo3DHostSetSkySunColor(sc);
					demo::Demo3DHostSetSkySunLightsScene(NkScBool(e, "soleilEclaire", false));
					demo::Demo3DHostSetSkySunSource(NkScInt(e, "soleilSource", -1));
					demo::Demo3DHostSetSkyAlienTemp(NkScFloat(e, "etoileTemperature", 5778.f));
					demo::Demo3DHostSetSkyCloudSpeed(NkScFloat(e, "nuagesVitesse", 0.f));
					demo::Demo3DHostSetSkyStars(NkScFloat(e, "etoilesIntensite", 0.f),
												NkScFloat(e, "etoilesDensite", 0.5f));
					demo::Demo3DHostSetSkyStarMotion(NkScFloat(e, "cielRotation", 0.f),
													 NkScFloat(e, "filantes", 0.f));
					NkArchive nu;
					if (e.GetObject("nuages", nu)) {
						float32 cc[3];
						NkScGetVec3(nu, "couleur", cc, 1.f, 1.f, 1.f);
						demo::Demo3DHostSetSkyClouds(NkScBool(nu, "actif", false),
													 NkScFloat(nu, "couverture", 0.5f),
													 NkScFloat(nu, "densite", 0.5f),
													 NkScFloat(nu, "echelle", 1.f), cc);
					}
					const int32 nM = NkScInt(e, "lunes", 0);
					demo::Demo3DHostSetSkyMoonCount(nM);
					for (int32 m = 0; m < nM && m < 2; ++m) {
						char k[8];
						snprintf(k, sizeof(k), "l%d", (int)m);
						NkArchive lu;
						if (!e.GetObject(k, lu))
							continue;
						float32 mc[3];
						NkScGetVec3(lu, "couleur", mc, 1.f, 1.f, 1.f);
						demo::Demo3DHostSetSkyMoon(m, NkScFloat(lu, "elevation", 30.f),
												   NkScFloat(lu, "azimut", 0.f),
												   NkScFloat(lu, "taille", 0.5f),
												   NkScFloat(lu, "eclat", 1.f), mc);
						demo::Demo3DHostSetSkyMoonPhase(m, NkScBool(lu, "phaseManuelle", false),
														NkScFloat(lu, "phase", 0.f));
					}
					float32 top[3], hor[3], gnd[3];
					NkScGetVec3(e, "degradeHaut", top, 0.18f, 0.28f, 0.45f);
					NkScGetVec3(e, "degradeHorizon", hor, 0.55f, 0.62f, 0.72f);
					NkScGetVec3(e, "degradeSol", gnd, 0.22f, 0.2f, 0.18f);
					demo::Demo3DHostSetEnvSky(top, hor, gnd);
					const NkString hdr = NkScStr(e, "hdr");
					if (!hdr.Empty())
						(void)demo::Demo3DHostLoadHdr(NkScToAbs(root, hdr.CStr()).CStr());
					(void)demo::Demo3DHostApplySky();
				}
			}
			// ── SORTIE, si la scene la porte.
			{
				NkArchive so;
				if (in.GetObject("sortie", so)) {
					demo::Demo3DHostSetOutMain(
						NkScInt(so, "source", -1), NkScInt(so, "largeur", 1920),
						NkScInt(so, "hauteur", 1080), NkScInt(so, "pourcent", 100),
						NkScInt(so, "format", 0), NkScBool(so, "transparent", false));
					const NkString od = NkScStr(so, "dossier");
					if (!od.Empty())
						demo::Demo3DHostSetOutDir(od.CStr());
					const NkString on = NkScStr(so, "nom");
					if (!on.Empty())
						demo::Demo3DHostSetOutName(on.CStr());
					const NkString nv = NkScStr(so, "nomVue");
					if (!nv.Empty())
						demo::Demo3DHostSetCaptureName(1, nv.CStr());
					const NkString nt = NkScStr(so, "nomTutoriel");
					if (!nt.Empty())
						demo::Demo3DHostSetCaptureName(2, nt.CStr());
					const int32 im = demo::Demo3DHostOutInsetMax();
					for (int32 i = 0; i < im; ++i) {
						char k[8];
						snprintf(k, sizeof(k), "i%d", (int)i);
						NkArchive inc;
						if (!so.GetObject(k, inc))
							continue;
						float32 xy[2] = {NkScFloat(inc, "x", 0.02f), NkScFloat(inc, "y", 0.02f)};
						float32 szv[2] = {NkScFloat(inc, "largeur", 0.25f),
										  NkScFloat(inc, "hauteur", 0.25f)};
						float32 bc[3];
						NkScGetVec3(inc, "lisereCouleur", bc, 1.f, 1.f, 1.f);
						demo::Demo3DHostSetOutInset(i, NkScInt(inc, "source", -1),
													NkScInt(inc, "forme", 0), xy, szv,
													NkScFloat(inc, "lisere", 0.f), bc,
													NkScFloat(inc, "opacite", 1.f));
						demo::Demo3DHostSetOutInsetOwnFile(i, NkScBool(inc, "fichierPropre", false));
						demo::Demo3DHostSetOutInsetOwnShaped(i, NkScBool(inc, "fichierForme", false));
					}
				}
			}
		}

		inline void NkAsSceneCapture(NkArchive &o, const NkString &root, NkModelerState &st,
									 int32 d) {
			NkAsHeader(o, "scene");
			o.SetString("nom", st.docName[d]);
			o.SetBool("vierge", st.docBlank[d]);
			o.SetInt32("uniteSysteme", st.docUnitSys[d]);
			o.SetInt32("uniteLongueur", st.docUnitLen[d]);
			o.SetFloat32("uniteEchelle", st.docUnitScale[d] > 0.001f ? st.docUnitScale[d] : 1.f);
			// LA VUE DE LA SCENE : rouvrir un projet doit reposer le regard la ou on
			// l'avait laisse.
			NkArchive cam;
			const float32 *cp = st.docCamPose[d];
			cam.SetBool("posee", st.docCamSet[d]);
			NkScSetVec3(cam, "cible", cp);
			cam.SetFloat32("distance", cp[3]);
			cam.SetFloat32("lacet", cp[4]);
			cam.SetFloat32("tangage", cp[5]);
			cam.SetBool("ortho", st.docCamOrtho[d]);
			o.SetObject("vue", cam);
			// ── REGLAGES RENDU/ENVIRONNEMENT/SORTIE : PAR SCENE (Rihen, 10 aout).
			// Document ACTIF : on capture l'etat VIVANT (et on rafraichit son
			// instantane). Document INACTIF : on ecrit son INSTANTANE — avant,
			// « Enregistrer tout » recopiait les reglages de l'onglet actif dans
			// TOUTES les scenes. Sans instantane (jamais visite, vieux fichier) :
			// l'etat vivant, comme avant.
			{
				const int32 dAct = st.TabDoc(st.activeTab);
				NkArchive blocks;
				bool useSnap = false;
				if (d != dAct) {
					NkArchive t;
					useSnap = st.docRendu[d].GetObject("rendu", t) ||
							  st.docRendu[d].GetObject("environnement", t) ||
							  st.docRendu[d].GetObject("sortie", t);
				}
				if (useSnap) {
					blocks = st.docRendu[d];
				} else {
					NkAsRenduCapture(blocks, root);
					if (d == dAct)
						st.docRendu[d] = blocks;
				}
				NkArchive b1;
				if (blocks.GetObject("rendu", b1))
					o.SetObject("rendu", b1);
				NkArchive b2;
				if (blocks.GetObject("environnement", b2))
					o.SetObject("environnement", b2);
				NkArchive b3;
				if (blocks.GetObject("sortie", b3))
					o.SetObject("sortie", b3);
			}
			// SES noeuds, et EUX SEULS : ceux dont la scene hote est la sienne, en
			// ecartant les archives (qui sont des assets, pas des objets poses).
			const int32 host = (int32)st.docScene[d];
			const int32 nodeMax = demo::Demo3DHostNodeCount();
			NkVector<int32> live;
			for (int32 n = 0; n < nodeMax; ++n) {
				if (demo::Demo3DHostUserKind(n) == 0 || demo::Demo3DHostNodeDeleted(n))
					continue;
				if (demo::Demo3DHostNodeScene(n) != host)
					continue;
				live.PushBack(n);
			}
			NkAsNodesCapture(o, root, st, live);
		}

		inline void NkAsSceneRestore(const NkArchive &in, const NkString &root,
									 NkModelerState &st, int32 d, int32 *nodeMiss,
									 int32 *orphanFix) {
			NkString nm = NkScStr(in, "nom");
			if (!nm.Empty())
				NkScPut(st.docName[d], (uint32)sizeof(st.docName[0]), nm.CStr());
			st.docBlank[d] = NkScBool(in, "vierge", false);
			st.docUnitSys[d] = NkScInt(in, "uniteSysteme", 0);
			st.docUnitLen[d] = NkScInt(in, "uniteLongueur", 0);
			st.docUnitScale[d] = NkScFloat(in, "uniteEchelle", 1.f);
			NkArchive cam;
			st.docCamSet[d] = false;
			if (in.GetObject("vue", cam)) {
				float32 *cp = st.docCamPose[d];
				NkScGetVec3(cam, "cible", cp, 0.f, 0.f, 0.f);
				cp[3] = NkScFloat(cam, "distance", 6.5f);
				cp[4] = NkScFloat(cam, "lacet", 0.7f);
				cp[5] = NkScFloat(cam, "tangage", 0.35f);
				st.docCamOrtho[d] = NkScBool(cam, "ortho", false);
				st.docCamSet[d] = NkScBool(cam, "posee", false);
			}
			demo::Demo3DHostSetActiveScene((int32)st.docScene[d]);
			// Reglages par scene : appliquer PUIS memoriser l'instantane du
			// document — c'est lui qui re-sera applique aux bascules d'onglet.
			NkAsRenduRestore(in, root, st);
			{
				NkArchive snap;
				NkArchive t;
				if (in.GetObject("rendu", t))
					snap.SetObject("rendu", t);
				if (in.GetObject("environnement", t))
					snap.SetObject("environnement", t);
				if (in.GetObject("sortie", t))
					snap.SetObject("sortie", t);
				st.docRendu[d] = snap;
			}
			NkAsNodesRestore(in, root, st, false, nodeMiss, nullptr, orphanFix);
		}

		// ─────────────────────────────────────────────────────────────────────────
		// UN MODEL — .nkmesh
		// ─────────────────────────────────────────────────────────────────────────
		inline void NkAsModelCapture(NkArchive &o, const NkString &root, NkModelerState &st,
									 int32 card) {
			NkAsHeader(o, "model");
			o.SetString("nom", st.browserNames[card]);
			const int32 srcN = st.browserSrcNode[card] - 1;
			if (srcN < 0)
				return; // carte sans corps : le fichier dit son nom, pas plus
			// LA RACINE ET SES MAILLAGES. Le parcours d'appartenance vit dans
			// l'hote (HostIsInnerMeshOf, partage avec le deplacement de document) :
			// le refaire ici finirait par ne plus emporter les memes noeuds.
			const int32 nodeMax = demo::Demo3DHostNodeCount();
			NkVector<int32> live;
			live.PushBack(srcN);
			for (int32 n = 0; n < nodeMax; ++n) {
				if (n == srcN || demo::Demo3DHostUserKind(n) == 0)
					continue;
				if (demo::Demo3DHostNodeInnerMeshOf(n, srcN))
					live.PushBack(n);
			}
			NkAsNodesCapture(o, root, st, live);
		}

		inline void NkAsModelRestore(const NkArchive &in, const NkString &root,
									 NkModelerState &st, int32 card, int32 *nodeMiss) {
			// UN MODEL N'EST DANS AUCUNE SCENE. Il nait dans la scene hote 0 parce
			// qu'il faut bien un numero, puis il est ARCHIVE : invisible, hors
			// hierarchie, et donc incapable d'apparaitre dans une scene. C'est ce
			// qui manquait quand tout vivait dans un seul fichier.
			demo::Demo3DHostSetActiveScene(0);
			NkVector<int32> made;
			NkAsNodesRestore(in, root, st, true, nodeMiss, &made);
			st.browserSrcNode[card] = made.Empty() || made[0] < 0 ? 0 : made[0] + 1;
		}

		// ─────────────────────────────────────────────────────────────────────────
		// LE PROJET — l'arbre du navigateur, les vues, et RIEN d'autre
		// ─────────────────────────────────────────────────────────────────────────
		inline void NkProjectTreeCapture(NkArchive &o, NkModelerState &st) {
			o.SetInt32("disposition", kProjectLayoutVersion);
			// LE CLASSEMENT EST UNE CONFIGURATION DU PROJET, pas un etat de session :
			// retrouver son navigateur trie comme on l'a laisse fait partie de
			// « rouvrir son projet ».
			o.SetInt32("triNavigateur", st.browSort);
			o.SetBool("triDecroissant", st.browSortDesc);
			// ── NAVIGATEUR : dossiers, cartes, et le CHEMIN de leur fichier ──
			// Les liens internes (dossier parent) restent des RANGS DANS LE FICHIER,
			// jamais des indices de session : les emplacements se recyclent.
			NkVector<int32> rank;
			for (int32 b = 0; b < st.browserCount; ++b)
				rank.PushBack(-1);
			{
				int32 next = 0;
				for (int32 b = 0; b < st.browserCount; ++b)
					if (st.browserKind[b] != 255)
						rank[(usize)b] = next++;
			}
			NkVector<NkArchive> cards;
			NkVector<int32> docRank; // document -> rang de SA carte
			for (int32 d = 0; d < NkModelerState::kMaxDocs; ++d)
				docRank.PushBack(-1);
			for (int32 b = 0; b < st.browserCount; ++b) {
				if (st.browserKind[b] == 255)
					continue; // carte supprimee : un trou, pas une carte
				NkArchive c;
				c.SetInt32("nature", (int32)st.browserKind[b]);
				c.SetString("nom", st.browserNames[b]);
				const int32 pp = st.browserParent[b];
				c.SetInt32("parent", (pp >= 0 && pp < st.browserCount) ? rank[(usize)pp] : -1);
				c.SetInt32("sousType", (int32)st.browserSub[b]);
				// LE LIEN VERS SON FICHIER. Une carte sans fichier (texture, graphe,
				// dataset) ecrit une chaine vide : elle existe, elle n'a pas encore
				// de contenu, et le fichier le DIT plutot que de le taire.
				c.SetString("fichier", st.browserFile[b]);
				cards.PushBack(c);
				const int32 dd = st.browserDoc[b] - 1;
				if (dd >= 0 && dd < NkModelerState::kMaxDocs)
					docRank[(usize)dd] = rank[(usize)b];
			}
			o.SetObjectArray("navigateur", cards);

			// ── VUES OUVERTES : la DISPOSITION, pas les scenes ──
			NkVector<NkArchive> views;
			int32 activeRank = 0;
			for (int32 t = 0; t < st.sceneCount && t < 8; ++t) {
				const int32 d = st.TabDoc(t);
				if (d < 0 || st.docTransient[d] || docRank[(usize)d] < 0)
					continue;
				if (t == st.activeTab)
					activeRank = (int32)views.Size();
				NkArchive v;
				v.SetInt32("carte", docRank[(usize)d]);
				views.PushBack(v);
			}
			o.SetObjectArray("vues", views);
			o.SetInt32("vueActive", activeRank);
		}

		/// Ecrit UN FICHIER PAR ASSET sous la racine du projet, puis met a jour le
		/// chemin memorise de chaque carte. Renvoie faux a la premiere ecriture
		/// impossible : un projet a moitie ecrit doit se signaler, pas se taire.
		/// `onlyCard >= 0` : n'ecrit QUE cette carte (plus les materiaux, cf.
		/// ci-dessous). C'est « Enregistrer » -- il porte sur le fichier qu'on
		/// regarde, comme partout ailleurs. `onlyCard < 0` : tout le projet, c'est
		/// « Enregistrer tout ».
		///
		/// LES MATERIAUX SONT TOUJOURS ECRITS, et c'est assume : ils se modifient
		/// depuis le panneau de proprietes pendant qu'une SCENE est active, et rien
		/// ne dit encore lequel a change. Les ecarter ferait perdre un reglage de
		/// matiere -- infiniment pire que reecrire quelques petits fichiers. Le
		/// jour ou chaque materiau portera son propre etat « modifie », ce filtre
		/// deviendra le meme que pour les autres.
		inline bool NkProjectWriteAssets(const NkString &root, NkModelerState &st,
										 NkString *err, int32 onlyCard = -1) {
			// Toute scene et tout materiau ont leur carte AVANT l'ecriture : c'est
			// la carte qui porte le chemin du fichier.
			NkBrowserSyncScenes(st);
			NkBrowserSyncMats(st);
			// La vue de l'onglet actif n'est rangee dans son document qu'a la
			// bascule : sans ce rangement, enregistrer sans changer d'onglet
			// perdrait le regard qu'on vient d'y poser.
			{
				const int32 dA = st.TabDoc(st.activeTab);
				if (dA >= 0) {
					if (st.sceneTabKind[st.activeTab] == 0)
						NkStoreSceneCam(st, st.activeTab);
					st.docUnitSys[dA] = st.unitSystem;
					st.docUnitLen[dA] = st.unitLength;
					st.docUnitScale[dA] = st.unitScale;
				}
			}
			for (int32 b = 0; b < st.browserCount; ++b) {
				const uint8 k = st.browserKind[b];
				if (k == 255 || !NkAsExtFor(k))
					continue;
				if (onlyCard >= 0 && b != onlyCard && k != 2)
					continue; // « Enregistrer » : ce fichier-ci, et les materiaux
				const NkString rel = NkAsRelFor(st, b);
				if (rel.Empty())
					continue;
				NkArchive a;
				if (k == 5) {
					const int32 d = st.browserDoc[b] - 1;
					if (d < 0 || d >= NkModelerState::kMaxDocs || !st.docUsed[d])
						continue;
					NkAsSceneCapture(a, root, st, d);
				} else if (k == 6) {
					NkAsModelCapture(a, root, st, b);
				} else {
					const int32 m = st.browserMat[b] - 1;
					if (m < 0)
						continue;
					NkAsMatCapture(a, root, m);
				}
				if (!NkAsWrite(root, rel, a, err))
					return false;
				// La DATE sert au classement du navigateur. Relevee ICI, a
				// l'ecriture, plutot qu'interrogee a la peinture : trente-deux
				// appels au systeme de fichiers par image couteraient plus cher que
				// tout le navigateur.
				st.browserTime[b] =
					NkFileSystem::GetLastWriteTime(NkScToAbs(root, rel.CStr()).CStr());
				// RENOMMEE OU DEPLACEE : l'ancien fichier est retire. Sans cela le
				// dossier du projet accumulerait des orphelins que plus rien ne
				// designe -- et qu'on prendrait, plus tard, pour du travail perdu.
				const NkString old(st.browserFile[b]);
				if (!old.Empty() && !(old == rel))
					(void)NkFile::Delete(NkScToAbs(root, old.CStr()).CStr());
				NkScPut(st.browserFile[b], (uint32)sizeof(st.browserFile[0]), rel.CStr());
			st.browserTime[b] =
				NkFileSystem::GetLastWriteTime(NkScToAbs(root, rel.CStr()).CStr());
			}
			return true;
		}

		/// Relit l'arbre du navigateur puis CHAQUE fichier d'asset. `err` recoit un
		/// resume de ce qui manquait -- jamais un silence.
		inline bool NkProjectTreeRestore(const NkArchive &in, const NkString &root,
										 NkModelerState &st, NkString *err) {
			if (!demo::Demo3DHostReady()) {
				if (err)
					*err = "vue 3D pas encore prete : projet non charge";
				return false;
			}
			// ── VIDER ──────────────────────────────────────────────────────
			demo::Demo3DHostViewCamera(-1);
			demo::Demo3DHostDeselectAll();
			demo::Demo3DHostSelectEmptyNode(-1);
			st.camViewNode = 0;
			st.addAdjustNode = -1;
			st.activeEmpty = -1;
			st.editPreviewNode = 0;
			st.selectedObject = -1;
			st.hierNote[0] = 0;
			const int32 nodeMax = demo::Demo3DHostNodeCount();
			for (int32 n = 0; n < nodeMax; ++n)
				if (demo::Demo3DHostUserKind(n) != 0 && !demo::Demo3DHostNodeDeleted(n))
					demo::Demo3DHostDeleteNode(n, false);
			for (int32 n = 96; n < nodeMax && n < 176; ++n)
				st.customNames[n][0] = 0;
			demo::Demo3DHostProjMatClear();
			st.browserCount = 0;
			st.browserFolder = -1;
			st.browClip = -1;
			st.browMenuIdx = -1;
			for (int32 b = 0; b < NkModelerState::kMaxBrowser; ++b) {
				st.browserKind[b] = 255;
				st.browserNames[b][0] = 0;
				st.browserParent[b] = -1;
				st.browserSub[b] = 0;
				st.browserSrcNode[b] = 0;
				st.browserDoc[b] = 0;
				st.browserMat[b] = 0;
				st.browserFile[b][0] = 0;
			}
			for (int32 d = 0; d < NkModelerState::kMaxDocs; ++d)
				st.DocFree(d);
			st.sceneIdNext = 1;

			// ── L'ARBRE ────────────────────────────────────────────────────
			st.browSort = NkScInt(in, "triNavigateur", 0);
			st.browSortDesc = NkScBool(in, "triDecroissant", false);
			NkVector<NkArchive> cards;
			(void)in.GetObjectArray("navigateur", cards);
			NkVector<int32> cardOf;
			for (usize i = 0; i < cards.Size(); ++i) {
				if (st.browserCount >= NkModelerState::kMaxBrowser) {
					cardOf.PushBack(-1);
					continue;
				}
				const int32 c = st.browserCount++;
				cardOf.PushBack(c);
				st.browserKind[c] = (uint8)(NkScInt(cards[i], "nature", 1) & 0xFF);
				NkScPut(st.browserNames[c], (uint32)sizeof(st.browserNames[0]),
						NkScStr(cards[i], "nom").CStr());
				st.browserSub[c] = (uint8)(NkScInt(cards[i], "sousType", 0) & 0xFF);
				NkScPut(st.browserFile[c], (uint32)sizeof(st.browserFile[0]),
						NkScStr(cards[i], "fichier").CStr());
			}
			// Parent en SECONDE PASSE : un dossier peut etre ecrit apres son contenu.
			for (usize i = 0; i < cards.Size(); ++i) {
				if (i >= cardOf.Size() || cardOf[i] < 0)
					continue;
				const int32 pr = NkScInt(cards[i], "parent", -1);
				if (pr >= 0 && (usize)pr < cardOf.Size() && cardOf[(usize)pr] >= 0)
					st.browserParent[cardOf[i]] = cardOf[(usize)pr];
			}

			int32 texMiss = 0, nodeMiss = 0, fileMiss = 0, orphanFix = 0;

			// ── LES MATERIAUX D'ABORD : les noeuds s'y assignent par chemin ──
			for (int32 b = 0; b < st.browserCount; ++b) {
				if (st.browserKind[b] != 2)
					continue;
				const int32 slot = demo::Demo3DHostProjMatCreate();
				if (slot < 0)
					continue;
				st.browserMat[b] = slot + 1;
				demo::Demo3DHostProjMatSetName(slot, st.browserNames[b]);
				NkArchive a;
				if (!st.browserFile[b][0] || !NkAsRead(root, st.browserFile[b], a)) {
					if (st.browserFile[b][0])
						++fileMiss;
					continue;
				}
				if (!NkAsNatureIs(a, "materiau")) {
					++fileMiss; // l'en-tete fait foi : on ne lit pas de travers
					continue;
				}
				NkAsMatRestore(a, root, slot, &texMiss);
			}

			// ── LES MODELS : archives, dans aucune scene ──
			for (int32 b = 0; b < st.browserCount; ++b) {
				if (st.browserKind[b] != 6)
					continue;
				NkArchive a;
				if (!st.browserFile[b][0] || !NkAsRead(root, st.browserFile[b], a)) {
					if (st.browserFile[b][0])
						++fileMiss;
					continue;
				}
				if (!NkAsNatureIs(a, "model")) {
					++fileMiss;
					continue;
				}
				NkAsModelRestore(a, root, st, b, &nodeMiss);
			}

			// ── LES SCENES : chacune son document, chacune ses noeuds ──
			for (int32 b = 0; b < st.browserCount; ++b) {
				if (st.browserKind[b] != 5)
					continue;
				const int32 d = st.DocAlloc();
				if (d < 0)
					continue;
				st.browserDoc[b] = d + 1;
				st.docCard[d] = b + 1;
				// LE NUMERO DE SCENE HOTE EST ATTRIBUE ICI, il n'est plus dans le
				// fichier : rien ne peut donc plus le rendre faux. C'etait le champ
				// par lequel les donnees fuyaient d'un type a l'autre.
				st.docScene[d] = (uint8)(st.sceneIdNext++ & 0xFF);
				NkScPut(st.docName[d], (uint32)sizeof(st.docName[0]), st.browserNames[b]);
				NkArchive a;
				if (!st.browserFile[b][0] || !NkAsRead(root, st.browserFile[b], a)) {
					if (st.browserFile[b][0])
						++fileMiss;
					continue;
				}
				if (!NkAsNatureIs(a, "scene")) {
					++fileMiss;
					continue;
				}
				NkAsSceneRestore(a, root, st, d, &nodeMiss, &orphanFix);
			}

			// UN PROJET A TOUJOURS AU MOINS UNE SCENE : sans document, l'application
			// n'aurait rien a montrer ni a enregistrer.
			bool anyDoc = false;
			for (int32 d = 0; d < NkModelerState::kMaxDocs && !anyDoc; ++d)
				anyDoc = st.docUsed[d] && !st.docTransient[d];
			if (!anyDoc) {
				const int32 d = st.DocAlloc();
				if (d >= 0) {
					NkScPut(st.docName[d], (uint32)sizeof(st.docName[0]), "Scene");
					st.docScene[d] = (uint8)(st.sceneIdNext++ & 0xFF);
				}
			}
			NkBrowserSyncScenes(st);

			// ── LES VUES ───────────────────────────────────────────────────
			NkVector<NkArchive> views;
			(void)in.GetObjectArray("vues", views);
			int32 cnt = 0;
			for (usize v = 0; v < views.Size() && cnt < 8; ++v) {
				const int32 cr = NkScInt(views[v], "carte", -1);
				if (cr < 0 || (usize)cr >= cardOf.Size() || cardOf[(usize)cr] < 0)
					continue;
				const int32 d = st.browserDoc[cardOf[(usize)cr]] - 1;
				if (d < 0 || d >= NkModelerState::kMaxDocs || !st.docUsed[d])
					continue;
				st.sceneTabDoc[cnt] = d;
				st.sceneTabKind[cnt] = 0;
				st.sceneTabAsset[cnt] = 0;
				++cnt;
			}
			if (cnt == 0) {
				for (int32 d = 0; d < NkModelerState::kMaxDocs && cnt < 1; ++d) {
					if (!st.docUsed[d] || st.docTransient[d])
						continue;
					st.sceneTabDoc[cnt] = d;
					st.sceneTabKind[cnt] = 0;
					st.sceneTabAsset[cnt] = 0;
					++cnt;
				}
			}
			for (int32 t = cnt; t < 8; ++t)
				st.sceneTabDoc[t] = -1;
			st.sceneCount = cnt > 0 ? cnt : 1;
			int32 at = NkScInt(in, "vueActive", 0);
			if (at < 0 || at >= st.sceneCount)
				at = 0;
			// Les unites de l'onglet actif sont posees AVANT la bascule :
			// NkActivateTab commence par ranger celles de l'onglet quitte -- le meme
			// ici -- et ecraserait sinon ce qu'on vient de restaurer.
			st.activeTab = at;
			{
				const int32 dA = st.TabDoc(at);
				if (dA >= 0) {
					st.unitSystem = st.docUnitSys[dA];
					st.unitLength = st.docUnitLen[dA];
					st.unitScale = st.docUnitScale[dA] > 0.001f ? st.docUnitScale[dA] : 1.f;
				}
			}
			NkActivateTab(st, at, true);
			// Le detecteur de hierarchie reprend son cliche ICI : sans cela, la
			// premiere frame verrait chaque parent « avoir bouge » et trainerait ses
			// enfants, deja poses. L'ambiance aussi : les objets recrees sont des
			// occludeurs du GI voxel.
			demo::Demo3DHostHierarchyResync();
			demo::Demo3DHostGIMarkDirty();

			if (err) {
				err->Clear();
				if (texMiss > 0 || nodeMiss > 0 || fileMiss > 0 || orphanFix > 0) {
					char msg[240];
					snprintf(msg, sizeof(msg),
							 "projet charge : %d fichier(s) d'asset illisible(s) ou absent(s), "
							 "%d texture(s) introuvable(s), %d objet(s) non recree(s), "
							 "%d maillage(s) sans model rendu(s) visible(s)",
							 (int)fileMiss, (int)texMiss, (int)nodeMiss, (int)orphanFix);
					*err = msg;
				}
			}
			return true;
		}

		// ─────────────────────────────────────────────────────────────────────────
		// LE DISQUE FAIT FOI SUR CE QUI EXISTE (decision de Rihen, 8 aout 2026)
		//
		// Le `.nk3dm` garde l'ORDRE et la configuration globale ; l'EXISTENCE d'un
		// asset vient du balayage du dossier. Un `.nkmesh` depose a la main
		// apparait donc dans le navigateur, et un fichier efface a la main
		// disparait -- « supprimer un fichier sur le disque doit mettre a jour
		// l'application, et vice versa ».
		//
		// ⚠ RAPPEL, a ne pas perdre : le `.nk3dm` aura une SECONDE vie -- tout
		// conserver dans un seul fichier (mode empaquete). « Le disque fait foi »
		// ne vaut donc que pour le mode LIE. C'est pourquoi les captures ci-dessus
		// ne connaissent que des `NkArchive` et jamais un fichier : le mode
		// empaquete n'aura qu'a rediriger les memes archives.
		// ─────────────────────────────────────────────────────────────────────────

		/// Carte portant ce chemin relatif, ou -1.
		inline int32 NkAsCardForFile(const NkModelerState &st, const NkString &rel) {
			for (int32 b = 0; b < st.browserCount; ++b)
				if (st.browserKind[b] != 255 && NkString(st.browserFile[b]) == rel)
					return b;
			return -1;
		}

		/// Dossier-carte correspondant a un chemin de dossier relatif (« a/b/ »),
		/// CREE au besoin. Un fichier depose dans un sous-dossier doit retrouver sa
		/// place dans l'arbre, pas atterrir a la racine.
		inline int32 NkAsEnsureFolder(NkModelerState &st, const NkString &dirRel) {
			int32 parent = -1;
			NkString part;
			for (NkString::SizeType i = 0; i <= dirRel.Size(); ++i) {
				const char c = (i < dirRel.Size()) ? dirRel[i] : '/';
				if (c != '/') {
					part += c;
					continue;
				}
				if (part.Empty())
					continue;
				int32 found = -1;
				for (int32 b = 0; b < st.browserCount && found < 0; ++b)
					if (st.browserKind[b] == 1 && st.browserParent[b] == parent &&
						NkString(st.browserNames[b]) == part)
						found = b;
				if (found < 0) {
					if (st.browserCount >= NkModelerState::kMaxBrowser)
						return parent;
					found = st.browserCount++;
					st.browserKind[found] = 1;
					st.browserParent[found] = parent;
					st.browserSub[found] = 0;
					st.browserDoc[found] = 0;
					st.browserMat[found] = 0;
					st.browserSrcNode[found] = 0;
					st.browserFile[found][0] = 0;
					NkScPut(st.browserNames[found], (uint32)sizeof(st.browserNames[0]),
							part.CStr());
				}
				parent = found;
				part.Clear();
			}
			return parent;
		}

		/// Retire une carte que le disque n'a plus. Ce qu'elle portait s'en va avec
		/// elle : le document d'une scene, les noeuds archives d'un model. Les
		/// ONGLETS qui la montraient se referment -- une vue sur un fichier efface
		/// n'a plus rien a montrer, et la laisser ouverte ferait reecrire le fichier
		/// au prochain enregistrement, donc annulerait la suppression.
		inline void NkAsDropCard(NkModelerState &st, int32 b) {
			const uint8 k = st.browserKind[b];
			if (k == 5) {
				const int32 d = st.browserDoc[b] - 1;
				if (d >= 0 && d < NkModelerState::kMaxDocs && st.docUsed[d]) {
					for (int32 t = st.sceneCount - 1; t >= 0; --t)
						if (st.TabDoc(t) == d)
							NkCloseSceneTab(st, t);
					const int32 host = (int32)st.docScene[d];
					const int32 nodeMax = demo::Demo3DHostNodeCount();
					for (int32 n = 0; n < nodeMax; ++n)
						if (demo::Demo3DHostUserKind(n) != 0 &&
							!demo::Demo3DHostNodeDeleted(n) &&
							demo::Demo3DHostNodeScene(n) == host)
							demo::Demo3DHostDeleteNode(n, false);
					st.DocFree(d);
				}
			} else if (k == 6) {
				const int32 n = st.browserSrcNode[b] - 1;
				if (n >= 0) {
					for (int32 t = st.sceneCount - 1; t >= 0; --t) {
						const int32 dt = st.TabDoc(t);
						if (dt >= 0 && st.docIsoNode[dt] == n + 1)
							NkCloseSceneTab(st, t);
					}
					demo::Demo3DHostDeleteNode(n, true);
				}
			}
			st.browserKind[b] = 255;
			st.browserFile[b][0] = 0;
			if (st.browserFolder == b)
				st.browserFolder = -1;
			if (st.selectedAsset == b)
				st.selectedAsset = -1;
		}

		/// Charge un fichier trouve sur le disque dont aucune carte ne parle, et lui
		/// fabrique sa carte. Renvoie faux si le fichier n'est pas exploitable --
		/// on ne cree alors AUCUNE carte : une carte sans contenu serait un mensonge.
		inline bool NkAsAdoptFile(NkModelerState &st, const NkString &root,
								  const NkString &rel) {
			NkArchive a;
			if (!NkAsRead(root, rel.CStr(), a))
				return false;
			// L'EN-TETE FAIT FOI, pas l'extension : un fichier renomme a la main
			// reste lu pour ce qu'il est.
			uint8 kind = 255;
			if (NkAsNatureIs(a, "scene"))
				kind = 5;
			else if (NkAsNatureIs(a, "model"))
				kind = 6;
			else if (NkAsNatureIs(a, "materiau"))
				kind = 2;
			else
				return false;
			if (st.browserCount >= NkModelerState::kMaxBrowser)
				return false;
			// Le dossier de l'arbre suit le dossier du disque.
			NkString dirRel;
			const NkString::SizeType s = rel.RFind('/');
			if (s != NkString::npos)
				dirRel = NkString(rel.CStr(), s + 1u);
			const int32 parent = NkAsEnsureFolder(st, dirRel);
			const int32 b = st.browserCount++;
			st.browserKind[b] = kind;
			st.browserParent[b] = parent;
			st.browserSub[b] = 0;
			st.browserDoc[b] = 0;
			st.browserMat[b] = 0;
			st.browserSrcNode[b] = 0;
			NkScPut(st.browserFile[b], (uint32)sizeof(st.browserFile[0]), rel.CStr());
			st.browserTime[b] =
				NkFileSystem::GetLastWriteTime(NkScToAbs(root, rel.CStr()).CStr());
			// Le nom vient du FICHIER : c'est lui qui fait foi maintenant.
			{
				NkString base = (s == NkString::npos)
									? rel
									: NkString(rel.CStr() + s + 1u,
											   (NkString::SizeType)(rel.Size() - s - 1u));
				const NkString::SizeType d = base.RFind('.');
				if (d != NkString::npos)
					base = NkString(base.CStr(), d);
				NkString nm = NkScStr(a, "nom");
				NkScPut(st.browserNames[b], (uint32)sizeof(st.browserNames[0]),
						nm.Empty() ? base.CStr() : nm.CStr());
			}
			int32 miss = 0, orph = 0, texMiss = 0;
			if (kind == 2) {
				const int32 slot = demo::Demo3DHostProjMatCreate();
				if (slot < 0)
					return false;
				st.browserMat[b] = slot + 1;
				NkAsMatRestore(a, root, slot, &texMiss);
			} else if (kind == 6) {
				NkAsModelRestore(a, root, st, b, &miss);
			} else {
				const int32 d = st.DocAlloc();
				if (d < 0)
					return false;
				st.browserDoc[b] = d + 1;
				st.docCard[d] = b + 1;
				st.docScene[d] = (uint8)(st.sceneIdNext++ & 0xFF);
				NkScPut(st.docName[d], (uint32)sizeof(st.docName[0]), st.browserNames[b]);
				NkAsSceneRestore(a, root, st, d, &miss, &orph);
			}
			return true;
		}

		/// RECONCILIE l'arbre avec le disque. Appelee apres un chargement et a
		/// chaque signal du surveillant. Renvoie le nombre de differences traitees
		/// -- zero veut dire « rien n'a change », et c'est le cas le plus frequent
		/// (nos propres ecritures declenchent le surveillant elles aussi).
		inline int32 NkProjectRescan(const NkString &root, NkModelerState &st) {
			if (root.Empty() || !NkDirectory::Exists(root.CStr()) || !demo::Demo3DHostReady())
				return 0;
			int32 changes = 0;
			// ── CE QUE LE DISQUE PORTE ──────────────────────────────────────
			static const char *const kPat[3] = {"*.nkscene", "*.nkmesh", "*.nkmat"};
			NkVector<NkString> found;
			for (int32 p = 0; p < 3; ++p) {
				const NkVector<NkString> f = NkDirectory::GetFiles(
					root.CStr(), kPat[p], NkSearchOption::NK_ALL_DIRECTORIES);
				for (usize i = 0; i < f.Size(); ++i)
					found.PushBack(NkScToRel(root, f[i].CStr()));
			}
			// ── FICHIERS NOUVEAUX : on les adopte ───────────────────────────
			for (usize i = 0; i < found.Size(); ++i) {
				if (NkAsCardForFile(st, found[i]) >= 0)
					continue;
				if (NkAsAdoptFile(st, root, found[i]))
					++changes;
			}
			// ── FICHIERS DISPARUS : la carte s'en va ────────────────────────
			// Seules les cartes qui ONT DEJA ete ecrites sont concernees : une
			// carte creee a l'instant et pas encore enregistree n'a pas de fichier,
			// et la faire disparaitre serait effacer un travail tout neuf.
			for (int32 b = 0; b < st.browserCount; ++b) {
				if (st.browserKind[b] == 255 || !st.browserFile[b][0])
					continue;
				bool alive = false;
				const NkString rel(st.browserFile[b]);
				for (usize i = 0; i < found.Size() && !alive; ++i)
					alive = (found[i] == rel);
				if (alive)
					continue;
				NkAsDropCard(st, b);
				++changes;
			}
			if (changes > 0) {
				demo::Demo3DHostHierarchyResync();
				demo::Demo3DHostGIMarkDirty();
			}
			return changes;
		}

		// ── LE SURVEILLANT ──────────────────────────────────────────────────────
		// ⚠ LE RAPPEL ARRIVE SUR UN AUTRE FIL (NkFileWatcher a son propre thread).
		// Il ne touche donc QUE ce drapeau -- jamais l'etat du modeleur, jamais
		// l'hote 3D. Toucher `NkModelerState` depuis ce fil produirait des
		// corruptions impossibles a reproduire. La reconciliation, elle, se fait
		// sur le fil principal, entre deux frames.
		struct NkProjectWatch : public NkFileWatcherCallback {
				NkFileWatcher *w = nullptr;
				volatile bool signaled = false;
				NkString root;

				void OnFileChanged(const NkFileChangeEvent &e) override {
					(void)e;
					signaled = true;
				}

				/// Surveille `newRoot`. Idempotent : rappeler avec la meme racine ne
				/// relance rien -- sinon chaque frame recreerait un thread.
				void Watch(const NkString &newRoot) {
					if (w && root == newRoot)
						return;
					Stop();
					if (newRoot.Empty() || !NkDirectory::Exists(newRoot.CStr()))
						return;
					root = newRoot;
					// Placement new sur une allocation NKMemory : jamais `new`, et
					// jamais de melange avec le tas de la CRT (regle du depot --
					// c'est la corruption c0000374 deja payee).
					void *mem = memory::NkAlloc(sizeof(NkFileWatcher));
					if (!mem)
						return;
					w = new (mem) NkFileWatcher(newRoot.CStr(), this, true);
					if (!w->Start()) {
						w->~NkFileWatcher();
						memory::NkFree(mem);
						w = nullptr;
					}
				}
				void Stop() {
					if (!w)
						return;
					w->Stop();
					w->~NkFileWatcher();
					memory::NkFree(w);
					w = nullptr;
					root.Clear();
				}
		};

	} // namespace nk3d
} // namespace nkentseu
