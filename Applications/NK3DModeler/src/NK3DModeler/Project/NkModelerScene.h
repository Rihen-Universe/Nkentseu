#pragma once
// =============================================================================
// NkModelerScene.h — SERIALISATION DE LA SCENE dans le .nk3dm.
//
// C'est le contenu de la section « scene » du fichier projet : ce qui manquait
// pour qu'enregistrer un projet enregistre le TRAVAIL, et pas seulement son
// nom et sa date.
//
// PAR OU ELLE PASSE, ET PAR OU ELLE NE PASSE PAS
//   La scene vit dans NkDemo3D.cpp, derriere la facade NkDemo3DHost.h. Ce
//   fichier n'en connait RIEN d'autre : pas un tableau, pas un indice interne.
//   Tout ce qui manquait a la facade y a ete AJOUTE (ProjMatClear, ProjMatMax,
//   SetNodeIsModel, HierarchyResync) plutot que contourne.
//   Le reste -- noms de la hierarchie, onglets de scene, unites, vue par
//   scene -- vit dans NkModelerState (Shell/NkModelerInput.h) : c'est la que
//   l'application les tient, et les recopier ailleurs les ferait diverger.
//
// TRANSFORMATIONS : POSITION + ROTATION + ECHELLE, SEPAREES.
//   Jamais de matrice 4x4 dans le fichier. C'est la regle de
//   PRINCIPES_CONCEPTION.private.md (choix d'Unreal, verifie contre Blender) :
//   une matrice libre pourrait porter un cisaillement, que le modeleur ne sait
//   pas stocker -- il se perdrait en silence a la relecture.
//
// CHEMINS RELATIFS A LA RACINE DU PROJET (CONVENTIONS_FICHIERS.md §5.1).
//   Un projet deplace, copie ou envoye doit continuer de fonctionner : c'est la
//   raison d'etre du dossier. Une ressource prise HORS du projet garde son
//   chemin absolu -- la rendre relative produirait un chemin faux, ce qui est
//   pire qu'un chemin qui dit franchement d'ou il vient.
//
// LES INDICES DU FICHIER NE SONT PAS LES INDICES DE LA SESSION.
//   Un parent et un materiau sont designes par leur RANG DANS LE FICHIER, pas
//   par le numero de noeud de la session qui a enregistre. Les emplacements
//   sont recycles a la volee : s'y fier aurait fait pointer une parente sur ce
//   qui occupe le meme numero la fois suivante.
//
// ── CE QUI EST SAUVEGARDE ET RELU ────────────────────────────────────────────
//   * noeuds utilisateur : nature, sous-type, nom, position/rotation/echelle,
//     parente, masquage, verrou, exclusion du rendu, masque de transmission,
//     appartenance au document, drapeaux maillage-interne / model ;
//   * parametres de creation des primitives (segments, anneaux, auxiliaire) ;
//   * materiaux du projet : nom, couleur, rugosite, metallique, les quatre
//     canaux de texture (chemins relatifs), intensites relief et emissif,
//     teinte emissive, forme d'apercu -- et l'assignation materiau <-> noeud ;
//   * lumieres : type (le sous-type), couleur, intensite, portee, angles,
//     dimensions de surface, ombre, temperature, exposition, cookie ;
//   * cameras : champ, plans de clipping, ortho, capteur, unite de focale,
//     guides, passe-partout ;
//   * DOCUMENTS du projet -- TOUTES les scenes, ouvertes ou non : nom, scene
//     hote, scene vierge, unites, et la vue (pose de camera) de chacune ;
//   * NAVIGATEUR de projet : cartes et dossiers, leur nom, leur nature, leur
//     rangement, et le lien carte <-> scene qui permet de rouvrir ;
//   * VUES ouvertes (les onglets) et celle qui etait active.
//
// UN ONGLET N'EST PAS UNE SCENE. Ce fichier ecrit les DOCUMENTS ; les onglets
// ne sont qu'une disposition. C'est ce qui fait que fermer un onglet ne
// supprime plus rien : la scene reste dans le fichier et dans le navigateur.
//
// ── CE QUI N'EST PAS ENCORE SAUVEGARDE ───────────────────────────────────────
//   Dit ici ET a l'ecran d'accueil, parce qu'un silence ferait perdre du
//   travail :
//   * la GEOMETRIE EDITEE (sommets deplaces en mode Edition) : un maillage est
//     regenere depuis ses parametres de creation, pas relu ;
//   * les MODIFICATEURS (la pile n'a pas encore de modele de donnees) ;
//   * les objets de la SCENE DE DEMONSTRATION (noeuds 0..95) : ils
//     reapparaissent tels qu'a l'ouverture, seul leur masquage par scene
//     vierge est conserve ;
//   * le CONTENU des cartes du navigateur autres que les scenes : un
//     materiau, une texture, un graphe reviennent avec leur nom et leur
//     place, pas encore avec ce qu'ils portent ;
//   * les onglets d'EDITEUR d'asset (leur maquette est transitoire) ;
//   * l'ENVIRONNEMENT de rendu (ciel, brouillard, sol infini, ambiance,
//     ombres) et les reglages de SORTIE ;
//   * les listes du panneau Modele (groupes de vertex, shape keys, UV...) ;
//   * le curseur 3D et la selection.
// =============================================================================

#include "NK3DModeler/Shell/NkModelerScreens.h" // NkModelerState + NkActivateTab/NkStoreSceneView
#include "NK3DModeler/Viewport/NkDemo3DHost.h"
#include "NKSerialization/NkArchive.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"

#include <cstdio>

// <windows.h> redefinit GetObject en GetObjectA. NkArchive.h porte sa propre
// garde, mais elle ne joue qu'a SA premiere inclusion : si windows.h est arrive
// apres lui dans cette unite, les appels ci-dessous se rebrancheraient sur un
// symbole inexistant. On la repose donc ici, au plus pres des appels.
#ifdef GetObject
#undef GetObject
#endif

namespace nkentseu {
	namespace nk3d {

		// Version du format de la SECTION SCENE, distincte de celle du fichier
		// projet : la scene evoluera bien plus vite que l'entete du .nk3dm, et
		// les lier obligerait a invalider l'un pour l'autre.
		//
		// 1 -> 2 : les scenes ne sont plus les ONGLETS mais les DOCUMENTS du
		// projet (`documents`), le NAVIGATEUR est ecrit (`navigateur`), et les
		// onglets ouverts deviennent une simple liste de VUES (`vues`). Un
		// fichier de format 1 reste lisible : son tableau `scenes` est relu
		// comme la liste des documents, un onglet par document.
		static const int32 kSceneFormatVersion = 2;

		// ── OUTILS DE CHEMIN ────────────────────────────────────────────────
		// Tout est ramene a '/' des l'entree : Windows accepte les deux, mais un
		// chemin ecrit avec des '\' ne se compare plus a celui que NKFileSystem
		// rend normalise -- la lecon deja payee par la couverture du projet.
		inline NkString NkScNorm(const char *p) {
			NkString out;
			if (!p)
				return out;
			for (const char *c = p; *c; ++c)
				out += (*c == '\\') ? '/' : *c;
			return out;
		}

		inline bool NkScIsAbs(const NkString &p) {
			if (p.Size() >= 1u && p[0] == '/')
				return true;
			return p.Size() >= 2u && p[1] == ':'; // « D: », lettre de volume
		}

		/// Chemin RELATIF a la racine du projet. Hors du projet : l'absolu est
		/// conserve tel quel -- fabriquer un « ../.. » donnerait un chemin qui
		/// casse des que le projet est deplace, ce que la relativisation
		/// cherchait justement a eviter.
		inline NkString NkScToRel(const NkString &root, const char *abs) {
			const NkString a = NkScNorm(abs);
			if (a.Empty() || root.Empty())
				return a;
			NkString pre = root;
			if (pre.Size() > 0u && pre[pre.Size() - 1u] != '/')
				pre += '/';
			if (a.Size() <= pre.Size())
				return a;
			for (NkString::SizeType i = 0; i < pre.Size(); ++i) {
				// Comparaison INSENSIBLE A LA CASSE : Windows ne distingue pas
				// « D:/Projets » de « d:/projets », et une texture prise par un
				// selecteur qui ecrit l'un pendant que le projet porte l'autre
				// serait sinon declaree hors du projet.
				char x = a[i], y = pre[i];
				if (x >= 'A' && x <= 'Z')
					x = (char)(x - 'A' + 'a');
				if (y >= 'A' && y <= 'Z')
					y = (char)(y - 'A' + 'a');
				if (x != y)
					return a;
			}
			return NkString(a.CStr() + pre.Size(),
							(NkString::SizeType)(a.Size() - pre.Size()));
		}

		/// Chemin ABSOLU depuis un chemin lu dans le fichier. Un chemin deja
		/// absolu passe tel quel : c'est une ressource externe assumee.
		inline NkString NkScToAbs(const NkString &root, const char *rel) {
			const NkString r = NkScNorm(rel);
			if (r.Empty() || root.Empty() || NkScIsAbs(r))
				return r;
			NkString out = root;
			if (out.Size() > 0u && out[out.Size() - 1u] != '/')
				out += '/';
			out += r;
			return out;
		}

		// ── LECTURE TOLERANTE ───────────────────────────────────────────────
		// Un champ absent prend sa valeur par defaut, il ne fait pas echouer la
		// lecture : c'est ce qui permet a un fichier ecrit par une version
		// anterieure de s'ouvrir sans conversion.
		inline int32 NkScInt(const NkArchive &a, const char *key, int32 def) {
			nk_int32 v = 0;
			return a.GetInt32(key, v) ? (int32)v : def;
		}
		inline float32 NkScFloat(const NkArchive &a, const char *key, float32 def) {
			nk_float32 v = 0.f;
			return a.GetFloat32(key, v) ? (float32)v : def;
		}
		inline bool NkScBool(const NkArchive &a, const char *key, bool def) {
			nk_bool v = false;
			return a.GetBool(key, v) ? (bool)v : def;
		}
		inline NkString NkScStr(const NkArchive &a, const char *key) {
			NkString v;
			(void)a.GetString(key, v);
			return v;
		}

		// Un vecteur s'ecrit {x,y,z} et non un tableau : les getters par CLE
		// convertissent entier <-> flottant tout seuls, alors qu'un tableau
		// rendrait des valeurs brutes qu'il faudrait typer a la main -- et un
		// « 1 » relu en entier la ou on attend un flottant est exactement le
		// genre de piege qui ne se voit qu'a la relecture.
		inline void NkScSetVec3(NkArchive &a, const char *key, const float32 *v) {
			NkArchive o;
			o.SetFloat32("x", v[0]);
			o.SetFloat32("y", v[1]);
			o.SetFloat32("z", v[2]);
			a.SetObject(key, o);
		}
		inline void NkScGetVec3(const NkArchive &a, const char *key, float32 *v, float32 dx,
								float32 dy, float32 dz) {
			v[0] = dx;
			v[1] = dy;
			v[2] = dz;
			NkArchive o;
			if (!a.GetObject(key, o))
				return;
			v[0] = NkScFloat(o, "x", dx);
			v[1] = NkScFloat(o, "y", dy);
			v[2] = NkScFloat(o, "z", dz);
		}

		// ── LE BLOC « vue » D'UN DOCUMENT ───────────────────────────────────────
		// La pose de camera ET ce que la scene AFFICHE (ombrage, surimpressions,
		// fond — Rihen, 10 aout : « il faut sauvegarder les proprietes de la
		// vue »). Deux chemins l'ecrivent (fichier projet, fichier scene) : le
		// bloc vit ICI pour qu'ils ne puissent pas diverger.
		inline void NkScViewWrite(NkArchive &cam, const NkModelerState &st, int32 d) {
			const float32 *cp = st.docCamPose[d];
			cam.SetBool("posee", st.docCamSet[d]);
			NkScSetVec3(cam, "cible", cp);
			cam.SetFloat32("distance", cp[3]);
			cam.SetFloat32("lacet", cp[4]);
			cam.SetFloat32("tangage", cp[5]);
			cam.SetBool("ortho", st.docCamOrtho[d]);
			const NkModelerState::NkDocView &v = st.docView[d];
			cam.SetBool("reglee", st.docViewSet[d]);
			cam.SetInt32("ombrage", v.ombrage);
			cam.SetInt32("lumiereUnie", v.lumiereUnie);
			cam.SetInt32("surimpressions", (int32)v.surimpressions);
			cam.SetInt32("fond", v.fond);
			cam.SetInt32("fondType", v.fondType);
			cam.SetFloat32("fondLuminosite", v.fondLum);
			NkScSetVec3(cam, "fondCouleur", v.fondPerso);
		}
		inline void NkScViewRead(const NkArchive &cam, NkModelerState &st, int32 d) {
			float32 *cp = st.docCamPose[d];
			NkScGetVec3(cam, "cible", cp, 0.f, 0.f, 0.f);
			cp[3] = NkScFloat(cam, "distance", 6.5f);
			cp[4] = NkScFloat(cam, "lacet", 0.7f);
			cp[5] = NkScFloat(cam, "tangage", 0.35f);
			st.docCamOrtho[d] = NkScBool(cam, "ortho", false);
			st.docCamSet[d] = NkScBool(cam, "posee", false);
			// Un fichier d'avant ces champs n'a pas « reglee » : le document
			// garde alors les defauts d'ouverture, pas des zeros relus.
			NkModelerState::NkDocView v;
			v.ombrage = NkScInt(cam, "ombrage", v.ombrage);
			v.lumiereUnie = NkScInt(cam, "lumiereUnie", v.lumiereUnie);
			v.surimpressions =
				(uint32)NkScInt(cam, "surimpressions", (int32)v.surimpressions);
			v.fond = NkScInt(cam, "fond", v.fond);
			v.fondType = NkScInt(cam, "fondType", v.fondType);
			v.fondLum = NkScFloat(cam, "fondLuminosite", v.fondLum);
			NkScGetVec3(cam, "fondCouleur", v.fondPerso, v.fondPerso[0],
						v.fondPerso[1], v.fondPerso[2]);
			st.docView[d] = v;
			st.docViewSet[d] = NkScBool(cam, "reglee", false);
		}

		// Copie bornee vers un champ de taille fixe de l'etat du shell.
		inline void NkScPut(char *dst, uint32 cap, const char *src) {
			uint32 i = 0;
			for (; src && src[i] && i + 1u < cap; ++i)
				dst[i] = src[i];
			dst[i] = 0;
		}

		// ─────────────────────────────────────────────────────────────────────
		// CAPTURE : la scene vivante -> l'archive
		// ─────────────────────────────────────────────────────────────────────
		/// Vrai si cette scene hote est celle d'un EDITEUR D'ASSET ouvert. TOUT ce
		/// qui y vit -- le model ET ses maillages -- appartient a l'asset, donc
		/// s'ecrit comme une ARCHIVE, jamais comme le contenu d'une scene.
		///
		/// Le test porte sur la SCENE HOTE et non sur le seul noeud racine : les
		/// maillages du model y vivent aussi, et ne tester que la racine les
		/// faisait ecrire dans la premiere scene. Ils y reapparaissaient ensuite,
		/// VISIBLES EN VUE 3D et ABSENTS DE LA HIERARCHIE -- celle-ci cache les
		/// maillages internes hors d'un editeur de model. C'est mot pour mot le
		/// symptome des captures de Rihen du 8 aout.
		inline bool NkScIsAssetScene(const NkModelerState &st, int32 hostScene) {
			for (int32 d = 0; d < NkModelerState::kMaxDocs; ++d)
				if (st.docUsed[d] && st.docAssetEdit[d] &&
					(int32)st.docScene[d] == hostScene)
					return true;
			return false;
		}

		/// Scene HOTE a ecrire pour un noeud, ou -1 s'il ne doit PAS etre ecrit.
		///
		/// Trois sortes de documents transitoires, trois statuts :
		///  * ISOLATION -- le noeud est un VRAI noeud de scene, seulement deplace
		///    le temps de la vue. On ecrit sa scene D'ORIGINE ; ecrire le document
		///    d'isolation le rattacherait a une scene inexistante.
		///  * EDITEUR D'ASSET -- deja traite par NkScIsAssetScene : c'est une
		///    archive, pas un contenu de scene. On renvoie -1, et surtout PAS
		///    `docIsoHome` : il vaut 0 pour un editeur d'asset (seule l'isolation
		///    le pose), donc le model se deversait dans la premiere scene.
		///  * MAQUETTE d'un autre editeur -- rien a ecrire, elle sera detruite.
		inline int32 NkScWriteScene(const NkModelerState &st, int32 hostScene) {
			for (int32 d = 0; d < NkModelerState::kMaxDocs; ++d) {
				if (!st.docUsed[d] || !st.docTransient[d] ||
					(int32)st.docScene[d] != hostScene)
					continue;
				const bool iso = st.docIsoNode[d] > 0 && !st.docAssetEdit[d];
				return iso ? (int32)st.docIsoHome[d] : -1;
			}
			return hostScene;
		}

		inline void NkSceneCapture(NkArchive &out, const NkString &root, NkModelerState &st) {
			out.SetInt32("format", kSceneFormatVersion);

			// L'ONGLET COURANT n'a pas encore range sa vue ni ses unites dans son
			// document : elles ne le sont qu'a la BASCULE d'onglet
			// (NkActivateTab). Sans ce rangement, enregistrer sans changer
			// d'onglet perdrait justement la vue sur laquelle on vient de
			// travailler.
			{
				const int32 dA = st.TabDoc(st.activeTab);
				if (dA >= 0) {
					if (st.sceneTabKind[st.activeTab] == 0)
						NkStoreSceneView(st, st.activeTab);
					st.docUnitSys[dA] = st.unitSystem;
					st.docUnitLen[dA] = st.unitLength;
					st.docUnitScale[dA] = st.unitScale;
				}
			}

			// ── DOCUMENTS DU PROJET ─────────────────────────────────────────
			// On ecrit les DOCUMENTS, pas les onglets. C'est le correctif central :
			// n'ecrire que les onglets ouverts effacait du fichier toute scene
			// fermee, en y laissant ses noeuds orphelins.
			// Les documents TRANSITOIRES (maquette d'editeur, isolation) sont
			// ecartes : ils n'existent que le temps d'une vue.
			NkVector<int32> docRank; // indice de document -> rang dans le fichier
			for (int32 d = 0; d < NkModelerState::kMaxDocs; ++d)
				docRank.PushBack(-1);
			NkVector<NkArchive> docs;
			for (int32 d = 0; d < NkModelerState::kMaxDocs; ++d) {
				if (!st.docUsed[d] || st.docTransient[d])
					continue;
				docRank[(usize)d] = (int32)docs.Size();
				NkArchive s;
				s.SetString("nom", st.docName[d]);
				// SCENE HOTE : c'est elle qui relie un document a ses noeuds. Elle
				// est conservee telle quelle plutot que renumerotee -- les noeuds
				// portent le meme numero, et les renumeroter des deux cotes
				// n'apporterait qu'une occasion de se tromper.
				s.SetInt32("scene", (int32)st.docScene[d]);
				s.SetBool("vierge", st.docBlank[d]);
				s.SetInt32("uniteSysteme", st.docUnitSys[d]);
				s.SetInt32("uniteLongueur", st.docUnitLen[d]);
				s.SetFloat32("uniteEchelle",
							 st.docUnitScale[d] > 0.001f ? st.docUnitScale[d] : 1.f);
				// LA VUE DE LA SCENE : rouvrir un projet doit reposer le regard
				// la ou on l'avait laisse -- pose de camera ET affichage.
				NkArchive cam;
				NkScViewWrite(cam, st, d);
				s.SetObject("vue", cam);
				docs.PushBack(s);
			}
			out.SetObjectArray("documents", docs);

			// ── MATERIAUX DU PROJET ─────────────────────────────────────────
			const int32 matMax = demo::Demo3DHostProjMatMax();
			NkVector<int32> matRank; // emplacement -> rang dans le fichier
			for (int32 i = 0; i < matMax; ++i)
				matRank.PushBack(-1);
			NkVector<NkArchive> mats;
			const int32 chanCount = demo::Demo3DHostMatChanCount();
			for (int32 i = 0; i < matMax; ++i) {
				char nm[64];
				float32 alb[3] = {0.7f, 0.7f, 0.7f};
				float32 rough = 0.85f, metal = 0.f;
				if (!demo::Demo3DHostProjMatInfo(i, nm, (uint32)sizeof(nm), alb, &rough, &metal))
					continue; // emplacement libre : l'iteration saute les trous
				matRank[(usize)i] = (int32)mats.Size();
				NkArchive m;
				m.SetString("nom", nm);
				NkScSetVec3(m, "albedo", alb);
				m.SetFloat32("rugosite", rough);
				m.SetFloat32("metallique", metal);
				float32 nrm = 1.f, emiS = 1.f;
				demo::Demo3DHostProjMatChanStrength(i, &nrm, &emiS);
				m.SetFloat32("relief", nrm);
				m.SetFloat32("emissifIntensite", emiS);
				float32 emi[3] = {0.f, 0.f, 0.f};
				demo::Demo3DHostProjMatEmissive(i, emi);
				NkScSetVec3(m, "emissif", emi);
				m.SetInt32("apercu", demo::Demo3DHostProjMatPrevShape(i));
				// LES QUATRE CANAUX, indexes et non nommes un a un : la table des
				// canaux vit dans l'hote, la recopier ici la ferait diverger au
				// premier canal ajoute.
				NkArchive maps;
				for (int32 c = 0; c < chanCount; ++c) {
					char key[16];
					snprintf(key, sizeof(key), "c%d", (int)c);
					const NkString rel =
						NkScToRel(root, demo::Demo3DHostProjMatMap(i, c));
					maps.SetString(key, rel.CStr());
				}
				m.SetObject("cartes", maps);
				mats.PushBack(m);
			}
			out.SetObjectArray("materiaux", mats);

			// ── NOEUDS UTILISATEUR ──────────────────────────────────────────
			// Un emplacement dont la nature est nulle n'a jamais servi ou a ete
			// recycle : ce n'est pas un noeud, c'est un trou.
			const int32 nodeMax = demo::Demo3DHostNodeCount();
			NkVector<int32> rankOf;
			for (int32 n = 0; n < nodeMax; ++n)
				rankOf.PushBack(-1);
			NkVector<int32> live;
			for (int32 n = 0; n < nodeMax; ++n) {
				if (demo::Demo3DHostUserKind(n) == 0)
					continue; // emplacement jamais servi, ou recycle : un trou
				// LES ARCHIVES SONT ECRITES. Elles sont invisibles et hors
				// hierarchie, mais ce sont elles qui PORTENT la geometrie des
				// assets « Model » du navigateur : les ecarter rendait la carte
				// avec son nom et sans son contenu.
				const bool arch = demo::Demo3DHostNodeArchived(n) ||
								  NkScIsAssetScene(st, demo::Demo3DHostNodeScene(n));
				if (demo::Demo3DHostNodeDeleted(n) && !arch)
					continue;
				// MAQUETTE d'un editeur d'asset : elle n'appartient a aucune scene
				// et sera detruite en quittant l'onglet. L'ecrire la faisait entrer
				// dans la premiere scene a la relecture (defaut constate par Rihen).
				if (!arch && NkScWriteScene(st, demo::Demo3DHostNodeScene(n)) < 0)
					continue;
				rankOf[(usize)n] = (int32)live.Size();
				live.PushBack(n);
			}

			NkVector<NkArchive> nodes;
			for (usize k = 0; k < live.Size(); ++k) {
				const int32 n = live[k];
				const int32 kind = demo::Demo3DHostUserKind(n);
				const int32 sub = demo::Demo3DHostUserSub(n);
				NkArchive nd;
				nd.SetInt32("nature", kind);
				nd.SetInt32("sousType", sub);
				nd.SetString("nom", (n < 176) ? st.customNames[n] : "");
				// UNE ARCHIVE N'EST DANS AUCUNE SCENE : c'est un asset du
				// navigateur, pas un objet pose quelque part. Lui donner un
				// document la ferait apparaitre dans cette scene a la relecture.
				const bool isArch = demo::Demo3DHostNodeArchived(n) ||
									NkScIsAssetScene(st, demo::Demo3DHostNodeScene(n));
				nd.SetBool("archive", isArch);
				nd.SetInt32("document",
							isArch ? -1 : NkScWriteScene(st, demo::Demo3DHostNodeScene(n)));
				// PARENTE : par RANG DANS LE FICHIER. Un parent qui n'est pas un
				// noeud utilisateur (un objet de la scene de demonstration) garde
				// son numero brut -- ceux-la sont fixes, ils ne se recyclent pas.
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
				// MATERIAU : par rang dans le fichier, comme la parente.
				const int32 mi = demo::Demo3DHostProjMatOf(n);
				nd.SetInt32("materiau",
							(mi >= 0 && mi < matMax) ? matRank[(usize)mi] : -1);
				// PARAMETRES DE CREATION : sans eux, une sphere rechargee ne
				// pourrait plus etre reajustee -- son maillage serait la, mais le
				// panneau « Ajuster la creation » n'aurait plus rien a montrer.
				int32 segs = 0, rings = 0;
				float32 aux = 0.f;
				if (demo::Demo3DHostMeshParams(n, &segs, &rings, &aux)) {
					NkArchive cr;
					cr.SetInt32("segments", segs);
					cr.SetInt32("anneaux", rings);
					cr.SetFloat32("aux", aux);
					nd.SetObject("creation", cr);
				}
				// LUMIERE : le TYPE est le sous-type, deja ecrit plus haut.
				if (kind == 5) {
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
					}
					float32 tempK = 0.f, expo = 0.f;
					if (demo::Demo3DHostLightTempExp(n, &tempK, &expo)) {
						li.SetFloat32("temperature", tempK);
						li.SetFloat32("exposition", expo);
					}
					li.SetInt32("cookie", demo::Demo3DHostLightCookie(n));
					nd.SetObject("lumiere", li);
				}
				// CAMERA (un vide de sous-type 10). La FOCALE en millimetres n'est
				// pas ecrite : elle se DEDUIT du champ et du capteur -- l'ecrire
				// donnerait deux verites pour une meme grandeur.
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

			// ── NAVIGATEUR DE PROJET ────────────────────────────────────────
			// Il naissait VIDE a chaque lancement : ses dossiers, ses cartes et
			// leur rangement etaient perdus a la fermeture. Les liens (dossier
			// parent, document d'une carte de scene, noeud source d'un mesh) sont
			// ecrits par RANG DANS LE FICHIER, jamais par indice de session --
			// meme regle que la parente des noeuds, et pour la meme raison : les
			// emplacements se recyclent.
			//
			// CE QUI N'Y EST PAS ENCORE : le CONTENU des cartes de materiau, de
			// texture et de graphe. La carte revient avec son nom et sa place,
			// pas encore avec ce qu'elle porte -- c'est le chantier « persister
			// tous les types de fichiers », par type et dans les deux modes.
			NkVector<int32> browRank; // indice de carte -> rang dans le fichier
			for (int32 b = 0; b < st.browserCount; ++b)
				browRank.PushBack(-1);
			{
				int32 next = 0;
				for (int32 b = 0; b < st.browserCount; ++b)
					if (st.browserKind[b] != 255)
						browRank[(usize)b] = next++;
			}
			NkVector<NkArchive> brow;
			for (int32 b = 0; b < st.browserCount; ++b) {
				if (st.browserKind[b] == 255)
					continue; // carte supprimee : un trou, pas une carte
				NkArchive c;
				c.SetInt32("nature", (int32)st.browserKind[b]);
				c.SetString("nom", st.browserNames[b]);
				const int32 pp = st.browserParent[b];
				c.SetInt32("parent",
						   (pp >= 0 && pp < st.browserCount) ? browRank[(usize)pp] : -1);
				c.SetInt32("sousType", (int32)st.browserSub[b]);
				const int32 dc = st.browserDoc[b] - 1;
				c.SetInt32("document",
						   (dc >= 0 && dc < NkModelerState::kMaxDocs) ? docRank[(usize)dc] : -1);
				const int32 sn = st.browserSrcNode[b] - 1;
				c.SetInt32("noeudSource",
						   (sn >= 0 && sn < nodeMax) ? rankOf[(usize)sn] : -1);
				brow.PushBack(c);
			}
			out.SetObjectArray("navigateur", brow);

			// ── VUES OUVERTES (les onglets) ─────────────────────────────────
			// Ce ne sont plus les scenes, seulement la DISPOSITION dans laquelle
			// on a laisse l'application. Un onglet dont le document est
			// transitoire n'est pas ecrit : sa maquette n'existera plus.
			NkVector<NkArchive> views;
			int32 activeRank = 0;
			for (int32 t = 0; t < st.sceneCount && t < 8; ++t) {
				const int32 d = st.TabDoc(t);
				if (d < 0 || st.docTransient[d] || docRank[(usize)d] < 0)
					continue;
				if (t == st.activeTab)
					activeRank = (int32)views.Size();
				NkArchive v;
				v.SetInt32("document", docRank[(usize)d]);
				v.SetInt32("nature", (int32)st.sceneTabKind[t]);
				const int32 ai = st.sceneTabAsset[t] - 1;
				v.SetInt32("asset",
						   (ai >= 0 && ai < st.browserCount) ? browRank[(usize)ai] : -1);
				views.PushBack(v);
			}
			out.SetObjectArray("vues", views);
			out.SetInt32("vueActive", activeRank);
		}

		// ─────────────────────────────────────────────────────────────────────
		// RESTITUTION : l'archive -> la scene vivante
		//
		// LE CHARGEMENT REMPLACE : on vide avant de reconstruire. Empiler deux
		// scenes serait le pire des resultats -- rien ne dirait laquelle est le
		// projet qu'on vient d'ouvrir.
		//
		// Renvoie faux si la section est inexploitable (format inconnu, vue 3D
		// pas encore prete) : dans ce cas RIEN n'est touche. Renvoie vrai avec
		// `err` rempli quand le chargement s'est fait mais qu'une partie
		// manquait -- l'appelant l'affiche, il ne le tait pas.
		// ─────────────────────────────────────────────────────────────────────
		inline bool NkSceneRestore(const NkArchive &in, const NkString &root, NkModelerState &st,
								   NkString *err) {
			const int32 fmt = NkScInt(in, "format", 0);
			char msg[220];
			// PAS DE SCENE N'EST PAS UNE ERREUR. Un projet cree avant que la
			// serialisation existe, ou un projet simplement vide, n'a pas de champ
			// `format` -- NkProjectLoad rend alors une section VIDE. Le refuser
			// faisait apparaitre une alerte a l'ouverture de tout ancien projet,
			// alors qu'il n'y a rien a restaurer et que rien n'est perdu.
			// L'utilisateur ouvre son projet, la vue est vide : c'est exact.
			if (fmt <= 0) {
				if (err)
					err->Clear();
				return true;
			}
			// UN FORMAT INCONNU SE REFUSE, il ne se lit pas a moitie : des champs
			// dont on ignore le sens produiraient une scene silencieusement
			// fausse, et l'utilisateur l'enregistrerait par-dessus la bonne.
			if (fmt > kSceneFormatVersion) {
				if (err) {
					snprintf(msg, sizeof(msg),
							 "scene ecrite par une version plus recente (format %d, connu %d) : "
							 "non chargee",
							 (int)fmt, (int)kSceneFormatVersion);
					*err = msg;
				}
				return false;
			}
			// La reconstruction passe par la creation de noeuds, donc par le
			// moteur : sans vue 3D initialisee elle ne creerait rien, et le
			// projet s'ouvrirait vide sans que rien ne le dise.
			if (!demo::Demo3DHostReady()) {
				if (err)
					*err = "vue 3D pas encore prete : scene non chargee";
				return false;
			}

			// ── VIDER ───────────────────────────────────────────────────────
			demo::Demo3DHostViewCamera(-1); // on ne regarde plus une camera qui va disparaitre
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
			// LE NAVIGATEUR ET LES DOCUMENTS SONT VIDES AUSSI : ouvrir un projet
			// par-dessus un autre laissait sinon les cartes du precedent, qui
			// pointaient sur des scenes qui n'existaient plus.
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
			}
			for (int32 d = 0; d < NkModelerState::kMaxDocs; ++d)
				st.DocFree(d);

			int32 texMiss = 0, nodeMiss = 0;

			// ── MATERIAUX (avant les noeuds : ils s'y assignent) ────────────
			NkVector<NkArchive> mats;
			(void)in.GetObjectArray("materiaux", mats);
			NkVector<int32> matSlot; // rang fichier -> emplacement reel
			const int32 chanCount = demo::Demo3DHostMatChanCount();
			for (usize i = 0; i < mats.Size(); ++i) {
				const int32 slot = demo::Demo3DHostProjMatCreate();
				matSlot.PushBack(slot);
				if (slot < 0)
					continue;
				const NkArchive &m = mats[i];
				const NkString nm = NkScStr(m, "nom");
				if (!nm.Empty())
					demo::Demo3DHostProjMatSetName(slot, nm.CStr());
				float32 alb[3];
				NkScGetVec3(m, "albedo", alb, 0.7f, 0.7f, 0.7f);
				demo::Demo3DHostProjMatSetParams(slot, alb, NkScFloat(m, "rugosite", 0.85f),
												 NkScFloat(m, "metallique", 0.f));
				// LES INTENSITES AVANT LES CARTES : poser une normal map relit
				// l'intensite de relief au moment ou elle est posee. Dans l'autre
				// ordre, la carte serait branchee avec l'ancienne valeur.
				demo::Demo3DHostProjMatSetChanStrength(
					slot, NkScFloat(m, "relief", 1.f), NkScFloat(m, "emissifIntensite", 1.f));
				float32 emi[3];
				NkScGetVec3(m, "emissif", emi, 0.f, 0.f, 0.f);
				demo::Demo3DHostProjMatSetEmissive(slot, emi);
				demo::Demo3DHostProjMatSetPrevShape(slot, NkScInt(m, "apercu", 1));
				NkArchive maps;
				if (!m.GetObject("cartes", maps))
					continue;
				for (int32 c = 0; c < chanCount; ++c) {
					char key[16];
					snprintf(key, sizeof(key), "c%d", (int)c);
					const NkString rel = NkScStr(maps, key);
					if (rel.Empty())
						continue;
					const NkString abs = NkScToAbs(root, rel.CStr());
					// LE CHARGEMENT FAIT FOI (regle de l'hote) : une texture
					// introuvable n'est pas memorisee. On la COMPTE, pour le dire
					// -- un materiau qui perd sa carte en silence passe pour un
					// materiau mal regle.
					if (!demo::Demo3DHostProjMatSetMap(slot, c, abs.CStr()))
						++texMiss;
				}
			}

			// ── NOEUDS ──────────────────────────────────────────────────────
			NkVector<NkArchive> nodes;
			(void)in.GetObjectArray("noeuds", nodes);
			NkVector<int32> nodeOf;	   // rang fichier -> noeud reel
			NkVector<int32> nodeHosts; // scenes hotes reellement peuplees
			const int32 sceneBefore = demo::Demo3DHostActiveScene();
			for (usize i = 0; i < nodes.Size(); ++i) {
				const NkArchive &nd = nodes[i];
				const int32 kind = NkScInt(nd, "nature", 0);
				const int32 sub = NkScInt(nd, "sousType", 0);
				if (kind < 1 || kind > 10) {
					nodeOf.PushBack(-1);
					++nodeMiss;
					continue;
				}
				// IL NAIT DANS SON DOCUMENT : un noeud appartient a UNE scene, et
				// le creer dans la mauvaise le rendrait invisible dans la sienne.
				// UNE ARCHIVE n'appartient a aucune scene : elle nait n'importe ou
				// (elle sera retiree de la vue juste apres) et ne compte pas dans
				// le releve des scenes peuplees -- sinon elle en ferait naitre une.
				const bool isArch = NkScBool(nd, "archive", false);
				const int32 hostOf = isArch ? 0 : NkScInt(nd, "document", 0);
				if (!isArch) {
					// On RETIENT les scenes hotes reellement peuplees : un fichier
					// ecrit avant ce correctif contient des noeuds dont la scene a
					// ete effacee a la fermeture de son onglet. Sans ce releve, ils
					// resteraient invisibles pour toujours (cf. le fichier de Rihen,
					// « document: 1 » sans scene de rang 1).
					bool known = false;
					for (usize h = 0; h < nodeHosts.Size(); ++h)
						if (nodeHosts[h] == hostOf) {
							known = true;
							break;
						}
					if (!known && hostOf >= 0)
						nodeHosts.PushBack(hostOf);
				}
				demo::Demo3DHostSetActiveScene(hostOf < 0 ? 0 : hostOf);
				const int32 n = demo::Demo3DHostAddNode(kind, sub);
				nodeOf.PushBack(n);
				if (n < 0) {
					++nodeMiss; // plus d'emplacement libre
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
					demo::Demo3DHostSetUserLightParams(n, col,
													   NkScFloat(li, "intensite", 8.f));
					demo::Demo3DHostSetLightEx(
						n, NkScFloat(li, "portee", 10.f), NkScFloat(li, "angleInterne", 25.f),
						NkScFloat(li, "angleExterne", 35.f), NkScFloat(li, "largeur", 1.f),
						NkScFloat(li, "hauteur", 1.f), NkScBool(li, "ombre", true));
					demo::Demo3DHostSetLightTempExp(n, NkScFloat(li, "temperature", 0.f),
													NkScFloat(li, "exposition", 0.f));
					demo::Demo3DHostSetLightCookie(n, NkScInt(li, "cookie", -1));
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
											   NkScFloat(pv, "b", 0.f),
											   NkScFloat(pv, "a", 0.6f)};
						demo::Demo3DHostSetCamPasse(n, pa);
					}
				}
			}
			demo::Demo3DHostSetActiveScene(sceneBefore);

			// ── PARENTES ET MATERIAUX, EN SECONDE PASSE ─────────────────────
			// Un parent peut etre ecrit APRES son enfant dans le fichier : le
			// resoudre au vol echouerait une fois sur deux.
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
				const int32 mr = NkScInt(nd, "materiau", -1);
				if (mr >= 0 && (usize)mr < matSlot.Size() && matSlot[(usize)mr] >= 0)
					demo::Demo3DHostProjMatAssign(n, matSlot[(usize)mr]);
			}

			// ── MAILLAGES ORPHELINS : REPARATION ────────────────────────────
			// Un fichier ecrit avant le correctif du 8 aout contient des maillages
			// internes deverses dans une scene sans leur model. Ils y sont
			// INATTEIGNABLES : la hierarchie cache les maillages internes hors d'un
			// editeur de model, alors que la vue 3D les dessine -- « ca apparait
			// dans la vue 3D mais pas dans la hierarchie » (Rihen).
			//
			// On ne les SUPPRIME PAS : rien ne doit disparaitre sans un mot. On
			// leur retire leur qualite de maillage interne, ce qui les fait
			// reapparaitre dans la hierarchie comme des objets ordinaires --
			// l'utilisateur les voit, les deplace ou les efface en connaissance.
			int32 orphanMesh = 0;
			for (usize i = 0; i < nodes.Size(); ++i) {
				if (i >= nodeOf.Size() || nodeOf[i] < 0)
					continue;
				const int32 n = nodeOf[i];
				if (NkScBool(nodes[i], "archive", false) || !demo::Demo3DHostNodeIsMesh(n))
					continue;
				bool owned = false;
				int32 cur = demo::Demo3DHostNodeParent(n);
				for (int32 g = 0; g < 256 && cur >= 0 && !owned; ++g) {
					owned = demo::Demo3DHostNodeIsModel(cur);
					cur = demo::Demo3DHostNodeParent(cur);
				}
				if (!owned) {
					demo::Demo3DHostSetNodeIsMesh(n, false);
					++orphanMesh;
				}
			}

			// ── LES ARCHIVES SORTENT DE LA VUE EN TOUT DERNIER ──────────────
			// Elles ont ete construites comme des noeuds ordinaires pour recevoir
			// leur geometrie, leur materiau, leurs reglages -- et surtout leur
			// PARENTE : c'est elle qui tient un model et ses maillages ensemble.
			// Les retirer plus tot ferait travailler la passe de parente sur un
			// noeud absent, et le model reviendrait en morceaux.
			for (usize i = 0; i < nodes.Size(); ++i) {
				if (i >= nodeOf.Size() || nodeOf[i] < 0)
					continue;
				if (NkScBool(nodes[i], "archive", false))
					demo::Demo3DHostSetNodeArchived(nodeOf[i], true);
			}

			// ── DOCUMENTS DU PROJET ─────────────────────────────────────────
			// `documents` (format 2) ou, a defaut, `scenes` (format 1) : la liste
			// des scenes y avait le meme contenu, sous le nom des onglets qu'elle
			// confondait avec elles. Le champ de la scene hote a change de nom
			// (`document` -> `scene`) : on relit les deux.
			NkVector<NkArchive> docs;
			bool oldLayout = false;
			if (!in.GetObjectArray("documents", docs) || docs.Empty()) {
				docs.Clear();
				(void)in.GetObjectArray("scenes", docs);
				oldLayout = true;
			}
			NkVector<int32> docOf; // rang fichier -> indice de document
			int32 maxHost = 0;
			for (usize t = 0; t < docs.Size(); ++t) {
				const NkArchive &s = docs[t];
				const int32 d = st.DocAlloc();
				docOf.PushBack(d);
				if (d < 0)
					continue; // table pleine : le document manque, et on le dira
				NkString nm = NkScStr(s, "nom");
				if (nm.Empty())
					nm = "Scene";
				NkScPut(st.docName[d], (uint32)sizeof(st.docName[0]), nm.CStr());
				const int32 host = NkScInt(s, "scene", NkScInt(s, "document", 0));
				st.docScene[d] = (uint8)(host & 0xFF);
				if (host > maxHost)
					maxHost = host;
				st.docBlank[d] = NkScBool(s, "vierge", false);
				st.docUnitSys[d] = NkScInt(s, "uniteSysteme", 0);
				st.docUnitLen[d] = NkScInt(s, "uniteLongueur", 0);
				st.docUnitScale[d] = NkScFloat(s, "uniteEchelle", 1.f);
				NkArchive cam;
				st.docCamSet[d] = false;
				if (s.GetObject("vue", cam))
					NkScViewRead(cam, st, d);
			}
			// ── SCENES ORPHELINES : RECUPERATION ────────────────────────────
			// Un fichier ecrit avant ce correctif porte des noeuds dont la scene
			// a disparu du fichier quand son onglet a ete ferme. Leur donner une
			// scene les rend enfin visibles et modifiables, au lieu de les
			// laisser inaccessibles dans un fichier qui les contient pourtant.
			// Le nom dit d'ou elle vient : c'est une reparation, pas un choix de
			// l'utilisateur, et il doit pouvoir la renommer en connaissance.
			int32 rescued = 0;
			for (usize h = 0; h < nodeHosts.Size(); ++h) {
				bool covered = false;
				for (int32 d = 0; d < NkModelerState::kMaxDocs && !covered; ++d)
					if (st.docUsed[d] && (int32)st.docScene[d] == nodeHosts[h])
						covered = true;
				if (covered)
					continue;
				const int32 d = st.DocAlloc();
				if (d < 0)
					break;
				snprintf(msg, sizeof(msg), "Scene recuperee %d", (int)(rescued + 1));
				NkScPut(st.docName[d], (uint32)sizeof(st.docName[0]), msg);
				st.docScene[d] = (uint8)(nodeHosts[h] & 0xFF);
				docOf.PushBack(d);
				if (nodeHosts[h] > maxHost)
					maxHost = nodeHosts[h];
				++rescued;
			}

			// UN PROJET A TOUJOURS AU MOINS UNE SCENE : un fichier ancien ou vide
			// laisserait sinon l'application sans document, donc sans rien a
			// montrer ni a enregistrer.
			if (docOf.Empty()) {
				const int32 d = st.DocAlloc();
				if (d >= 0) {
					NkScPut(st.docName[d], (uint32)sizeof(st.docName[0]), "Scene");
					st.docScene[d] = 0;
					docOf.PushBack(d);
				}
			}
			// Le prochain numero de scene hote doit passer APRES tous ceux du
			// fichier : le reprendre a 1 ferait naitre une nouvelle scene dans une
			// scene hote deja peuplee.
			st.sceneIdNext = maxHost + 1;

			// ── NAVIGATEUR DE PROJET ────────────────────────────────────────
			NkVector<NkArchive> brow;
			(void)in.GetObjectArray("navigateur", brow);
			NkVector<int32> browOf; // rang fichier -> indice de carte
			for (usize b = 0; b < brow.Size(); ++b) {
				if (st.browserCount >= NkModelerState::kMaxBrowser) {
					browOf.PushBack(-1);
					continue;
				}
				const int32 c = st.browserCount++;
				browOf.PushBack(c);
				const NkArchive &e = brow[b];
				st.browserKind[c] = (uint8)(NkScInt(e, "nature", 1) & 0xFF);
				NkScPut(st.browserNames[c], (uint32)sizeof(st.browserNames[0]),
						NkScStr(e, "nom").CStr());
				st.browserSub[c] = (uint8)(NkScInt(e, "sousType", 0) & 0xFF);
				st.browserParent[c] = -1; // resolu en seconde passe
				st.browserDoc[c] = 0;
				st.browserSrcNode[c] = 0;
			}
			// SECONDE PASSE : parent, document et noeud source. Un parent peut
			// etre ecrit APRES son enfant -- le resoudre au vol echouerait une
			// fois sur deux, meme lecon que la parente des noeuds.
			for (usize b = 0; b < brow.Size(); ++b) {
				if (b >= browOf.Size() || browOf[b] < 0)
					continue;
				const int32 c = browOf[b];
				const NkArchive &e = brow[b];
				const int32 pr = NkScInt(e, "parent", -1);
				if (pr >= 0 && (usize)pr < browOf.Size() && browOf[(usize)pr] >= 0)
					st.browserParent[c] = browOf[(usize)pr];
				const int32 dr = NkScInt(e, "document", -1);
				if (dr >= 0 && (usize)dr < docOf.Size() && docOf[(usize)dr] >= 0) {
					const int32 d = docOf[(usize)dr];
					st.browserDoc[c] = d + 1;
					st.docCard[d] = c + 1;
				}
				const int32 sr = NkScInt(e, "noeudSource", -1);
				if (sr >= 0 && (usize)sr < nodeOf.Size() && nodeOf[(usize)sr] >= 0)
					st.browserSrcNode[c] = nodeOf[(usize)sr] + 1;
			}
			// Toute scene sans carte en recoit une : c'est la seule facon de la
			// rouvrir apres avoir ferme son onglet.
			NkBrowserSyncScenes(st);

			// ── VUES OUVERTES (les onglets) ─────────────────────────────────
			NkVector<NkArchive> views;
			(void)in.GetObjectArray("vues", views);
			int32 cnt = 0;
			for (usize v = 0; v < views.Size() && cnt < 8; ++v) {
				const int32 dr = NkScInt(views[v], "document", -1);
				if (dr < 0 || (usize)dr >= docOf.Size() || docOf[(usize)dr] < 0)
					continue;
				st.sceneTabDoc[cnt] = docOf[(usize)dr];
				// Onglets de SCENE uniquement : les vues d'EDITEUR d'asset portent
				// une maquette transitoire, qui n'existe plus a la relecture.
				st.sceneTabKind[cnt] = 0;
				st.sceneTabAsset[cnt] = 0;
				++cnt;
			}
			// Format 1, ou aucune vue exploitable : un onglet par document, dans
			// l'ordre du fichier -- c'est exactement ce que l'ancien format
			// decrivait.
			if (cnt == 0) {
				for (usize k = 0; k < docOf.Size() && cnt < 8; ++k) {
					if (docOf[k] < 0)
						continue;
					st.sceneTabDoc[cnt] = docOf[k];
					st.sceneTabKind[cnt] = 0;
					st.sceneTabAsset[cnt] = 0;
					++cnt;
				}
			}
			for (int32 t = cnt; t < 8; ++t)
				st.sceneTabDoc[t] = -1;
			st.sceneCount = cnt > 0 ? cnt : 1;
			int32 at = NkScInt(in, oldLayout ? "sceneActive" : "vueActive", 0);
			if (at < 0 || at >= st.sceneCount)
				at = 0;
			// Les unites de l'onglet actif sont posees AVANT la bascule :
			// NkActivateTab commence par ranger celles de l'onglet quitte -- le
			// meme ici -- et ecraserait sinon ce qu'on vient de restaurer.
			st.activeTab = at;
			{
				const int32 dA = st.TabDoc(at);
				if (dA >= 0) {
					st.unitSystem = st.docUnitSys[dA];
					st.unitLength = st.docUnitLen[dA];
					st.unitScale = st.docUnitScale[dA] > 0.001f ? st.docUnitScale[dA] : 1.f;
				}
			}
			NkActivateTab(st, at, true); // scene hote, vue et unites, par le chemin habituel

			// LE DETECTEUR DE HIERARCHIE reprend son cliche ICI : sans cela, la
			// premiere frame verrait chaque parent « avoir bouge » depuis la
			// scene precedente et trainerait ses enfants, deja poses.
			demo::Demo3DHostHierarchyResync();
			// L'AMBIANCE aussi : les objets recrees sont des occludeurs du GI
			// voxel -- sans invalidation, la grille resterait celle de la scene
			// PRECEDENTE jusqu'au premier geste.
			demo::Demo3DHostGIMarkDirty();

			if (err) {
				err->Clear();
				if (texMiss > 0 || nodeMiss > 0 || rescued > 0 || orphanMesh > 0) {
					// TOUTE REPARATION SE DIT. Une scene ou un objet qui reapparait
					// sous un nom ou une nature que l'utilisateur n'a pas choisis
					// doit etre annonce, sinon il passe pour un parasite.
					snprintf(msg, sizeof(msg),
							 "projet charge : %d texture(s) introuvable(s), %d objet(s) non "
							 "recree(s), %d scene(s) orpheline(s) recuperee(s), %d maillage(s) "
							 "sans model rendu(s) visible(s)",
							 (int)texMiss, (int)nodeMiss, (int)rescued, (int)orphanMesh);
					*err = msg;
				}
			}
			return true;
		}

	} // namespace nk3d
} // namespace nkentseu
