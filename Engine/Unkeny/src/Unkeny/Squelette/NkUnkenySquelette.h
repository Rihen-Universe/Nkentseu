//
// NkUnkenySquelette.h
// =============================================================================
// Description :
//   LE SQUELETTE 2D (2026-10-02, R30 de la feuille de route d'UnkenyEditor).
//   Plan valide par Rihen le 02/10 : « le squelette 2D vit dans NKAnima, comme
//   un squelette 3D CONTRAINT AU PLAN ; Unkeny a un composant Squelette 2D
//   (optionnel : un sprite reste un sprite) qui charge, joue et melange ces
//   animations via l'Animateur existant, et DEFORME le maillage 2D de R31 ».
//
//   Ce composant est le MIROIR, en donnees fixes, d'un anim::NkSkeleton2D
//   (NKAnima/Skeleton/NkSkeleton2D.h) : memes os (repos local au parent : x, y,
//   angle autour de Z, longueur, echelle), memes EMPLACEMENTS (« slots » de
//   Spine : l'attache montree et l'ordre de dessin, animables), meme fichier
//   (.nkskel, NkSauverSquelette2D). Les calculs sont ceux de NKAnima : la FK
//   (NkForwardKinematics), l'IK a deux os, les modeles de depart, le melange.
//   Unkeny n'ajoute que ce qui est propre au jeu : la POSE courante, la PEAU
//   (le maillage 2D de la meme entite, par les 4 os + 4 poids de chaque
//   sommet), et le MOUVEMENT SECONDAIRE (une chaine d'os confiee aux corps mous
//   XPBD : echarpe, cape, cheveux).
//
// Caracteristiques :
//   - DONNEES SEULEMENT, a CAPACITE FIXE (la regle de NkUnkenyComposants.h) :
//     32 os, 8 emplacements (4 attaches chacun), 4 chaines molles, 4 IK. La
//     photo (Jouer / Arreter, Ctrl+Z), le prefab et le .nkscene le portent
//     champ par champ (NkChampsSquelette2D, declare par NkScene::Init).
//   - UN OS A TOUJOURS SON PARENT AVANT LUI (indice plus petit) : la FK suit
//     l'ordre des indices, sans tri. Toutes les fonctions d'ici le gardent.
//   - LA PEAU passe par UN SEUL point : NkMaillageDeformer2D, appele par
//     NkMaillagePositionsMonde (rendu, prise au clic, boite de selection,
//     fenetre d'edition en jeu). Position d'un sommet pondere :
//         p' = Σ poids(k) x (monde_pose(os k) x inverse(monde_repos(os k))) x p
//     Un sommet sans poids suit l'entite, comme avant R30.
//   - LES EMPLACEMENTS designent des PARTIES du maillage (les attaches) : une
//     attache non montree est CACHEE, la montree prend l'ordre de l'emplacement
//     (NkEmplacementsParties2D, lu par le dessin). Rien n'est ecrit dans le
//     maillage : un apercu ne laisse aucune trace.
//
// ⚠️ LE PRIX : ~2,4 Ko de plus dans la photo de CHAQUE entite (NkPhotoEntite::
//    extra prend la taille de tous les composants decrits) -- a peser avant
//    d'augmenter les capacites. Le maillage en prend deja ~6,5.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYSQUELETTE_H__
#define __NKENTSEU_UNKENY_NKUNKENYSQUELETTE_H__

#include "NKAnima/Skeleton/NkSkeleton2D.h"
#include "Unkeny/Maillage/NkUnkenyMaillage.h"
#include "Unkeny/Scene/NkUnkenyChamps.h"

namespace nkentseu {
	namespace unkeny {

		class NkScene;

		static constexpr uint32 NK_SQUELETTE2D_OS_MAX = 32u;
		static constexpr uint32 NK_SQUELETTE2D_EMPLACEMENTS_MAX = 8u;
		static constexpr uint32 NK_SQUELETTE2D_ATTACHES_MAX = 4u;
		static constexpr uint32 NK_SQUELETTE2D_CHAINES_MAX = 4u;
		static constexpr uint32 NK_SQUELETTE2D_IK_MAX = 4u;

		/// Un OS : son repos (local au parent, ou au repere de l'entite pour une
		/// racine) et sa POSE (ce que l'animation, l'IK et l'editeur ecrivent).
		struct NkOs2D {
				char nom[16] = {};
				int8 parent = -1; ///< TOUJOURS plus petit que l'indice de l'os (-1 = racine)
				uint8 reserve = 0;
				// --- Le REPOS (« setup » de Spine) : m, radians ---------------
				float32 x = 0.f;
				float32 y = 0.f;
				float32 angle = 0.f;
				float32 longueur = 0.2f; ///< le long de son X local
				float32 ex = 1.f;
				float32 ey = 1.f;
				// --- La POSE (locale, comme le repos) ---------------------------
				float32 px = 0.f;
				float32 py = 0.f;
				float32 pangle = 0.f;
				float32 pex = 1.f;
				float32 pey = 1.f;
		};

		/// Un EMPLACEMENT (slot) : une place de dessin portee par un os, qui montre
		/// UNE de ses attaches (des parties du maillage) a un ordre de dessin.
		struct NkEmplacement2D {
				char nom[16] = {};
				int8 os = -1;
				/// Les attaches : des indices de PARTIES du maillage de la meme entite (-1 = libre).
				int8 attaches[NK_SQUELETTE2D_ATTACHES_MAX] = {-1, -1, -1, -1};
				int8 attache = 0;		///< l'attache montree (indice dans `attaches`, -1 = rien) -- ANIMEE
				int8 attacheRepos = 0;	///< celle du repos
				int16 ordre = 0;		///< l'ordre de dessin (le plus grand devant) -- ANIME
				int16 ordreRepos = 0;
		};

		/// Une CHAINE MOLLE : `nombre` os (chacun enfant du precedent, depuis
		/// `premier`) confies aux corps mous XPBD (NkParticules2D) en jeu. Le
		/// parent du premier os la TIENT ; le reste pend, se balance, suit.
		struct NkChaineMolle2D {
				int8 premier = -1;
				uint8 nombre = 0;
				bool decor = false;			 ///< touche le decor (corps rigides) ; faux : passe au travers
				float32 raideur = 0.9f;		 ///< des maillons [0,1]
				/// > 0 : des liens de FLEXION (saut d'un maillon), a la raideur du corps / 20 :
				/// une echarpe epaisse qui se plie moins. 0 (defaut) : une corde, elle pend.
				float32 flexion = 0.f;
				float32 amortissement = 0.04f;
				float32 masse = 0.05f; ///< kg par maillon
				/// La TRAINEE de l'air (1/s) : une echarpe se balance puis se pose ; sans
				/// elle, la chaine est un pendule qui ne s'arrete jamais.
				float32 trainee = 5.f;
				// --- TRANSITOIRES : l'etat EN JEU, jamais ecrit ---------------
				uint32 corps = 0u; ///< l'id STABLE du corps de particules
		};

		/// Une contrainte d'IK a deux os EN JEU : `os` (l'avant-bras) et son parent
		/// tournent pour que la tete de l'enfant (ou le bout de `os`) aille sur la
		/// tete de l'os `cible` (un os « poignee », anime ou pose).
		struct NkContrainteIK2D {
				int8 os = -1;
				int8 cible = -1;
				bool coudePositif = true; ///< le coude a gauche de la droite racine -> cible
				float32 melange = 1.f;
		};

		/// LE COMPOSANT.
		struct NkSquelette2D {
				uint8 nbOs = 0;
				uint8 nbEmplacements = 0;
				uint8 nbChaines = 0;
				uint8 nbIK = 0;
				/// Les os se dessinent-ils EN JEU dans la vue de l'editeur ?
				bool osVisibles = true;
				/// L'asset d'origine (« Contenu/Squelettes/Bodofia.nkskel »), vide = propre a l'entite.
				char source[64] = {};
				NkOs2D os[NK_SQUELETTE2D_OS_MAX];
				NkEmplacement2D emplacements[NK_SQUELETTE2D_EMPLACEMENTS_MAX];
				NkChaineMolle2D chaines[NK_SQUELETTE2D_CHAINES_MAX];
				NkContrainteIK2D ik[NK_SQUELETTE2D_IK_MAX];
		};

		// =====================================================================
		// LECTURE
		// =====================================================================
		int32 NkSqueletteTrouverOs(const NkSquelette2D &s, const char *nom) noexcept;
		/// Le repos et la pose d'un os, a la maniere de NKAnima.
		anim::NkBone2D NkOsRepos2D(const NkOs2D &o) noexcept;
		anim::NkBone2D NkOsPose2D(const NkOs2D &o) noexcept;
		void NkOsPoserRepos2D(NkOs2D &o, const anim::NkBone2D &b) noexcept;
		void NkOsPoserPose2D(NkOs2D &o, const anim::NkBone2D &b) noexcept;
		/// Les matrices MONDE (repere de l'entite) du repos, de la pose : la FK de
		/// NKAnima (NkForwardKinematics). `monde` : nbOs entrees.
		void NkSqueletteMondeRepos(const NkSquelette2D &s, math::NkMat4f *monde) noexcept;
		void NkSqueletteMondePose(const NkSquelette2D &s, math::NkMat4f *monde) noexcept;
		/// La tete et la queue de l'os `j` dans une pose MONDE.
		NkVec2f NkSqueletteTete(const math::NkMat4f *monde, uint32 j) noexcept;
		NkVec2f NkSqueletteQueue(const NkSquelette2D &s, const math::NkMat4f *monde, uint32 j) noexcept;
		/// L'os dont le SEGMENT (tete -> queue) passe le plus pres de `p` (repere de
		/// l'entite), dans `rayon` ; -1 sinon. `pose` : la pose, sinon le repos.
		int32 NkSqueletteOsSous(const NkSquelette2D &s, const NkVec2f &p, float32 rayon, bool pose) noexcept;
		/// La pose differe-t-elle du repos ?
		bool NkSqueletteEnPose(const NkSquelette2D &s) noexcept;

		// =====================================================================
		// LA PEAU (UN SEUL point de branchement : NkMaillagePositionsMonde)
		// =====================================================================
		/// Les matrices de PEAU : monde_pose x inverse(monde_repos), une par os.
		void NkSqueletteMatricesPeau(const NkSquelette2D &s, math::NkMat4f *peau) noexcept;
		/// Les positions des sommets dans le REPERE DE L'ENTITE, deformees par le
		/// squelette (nul : au repos). Un sommet sans poids garde sa place.
		uint32 NkMaillageDeformer2D(const NkSquelette2D *s, const NkMaillage2D &m, NkVec2f *sortie) noexcept;
		/// Les sommets ponderes du maillage (au moins un poids non nul).
		uint32 NkSommetsPonderes2D(const NkMaillage2D &m) noexcept;
		/// L'ordre de dessin et la visibilite de chaque PARTIE selon les
		/// emplacements (une partie hors emplacement : son ordre, visible).
		void NkEmplacementsParties2D(const NkSquelette2D *s, const NkMaillage2D &m, int32 *ordre, bool *visible) noexcept;

		// =====================================================================
		// LES POIDS
		// =====================================================================
		/// Le poids (0..1) de l'os `os` sur le sommet `i`.
		float32 NkPoidsSommet2D(const NkMaillage2D &m, uint32 i, uint32 os) noexcept;
		/// Pose les poids d'UN sommet depuis une table (un poids par os, NK_SQUELETTE2D_OS_MAX
		/// entrees) : les 4 plus forts, normalises (somme NK_MAILLAGE2D_POIDS_PLEIN). Tout nul : sans os.
		void NkPoserPoidsSommet2D(NkMaillage2D &m, uint32 i, const float32 *parOs) noexcept;
		/// Chaque sommet ramene a une somme pleine (un sommet sans poids le reste).
		void NkNormaliserPoids2D(NkMaillage2D &m) noexcept;
		/// LE PINCEAU : les sommets (au repos) a moins de `rayon` de `centre` recoivent
		/// `force` x (1 - d/rayon) de l'os `os` (ou le perdent si `retirer`) ; les autres
		/// os du sommet se partagent le reste. Rend le nombre de sommets touches.
		uint32 NkPeindrePoids2D(NkMaillage2D &m, uint32 os, const NkVec2f &centre, float32 rayon, float32 force, bool retirer) noexcept;
		/// AUTO-POIDS PAR DISTANCE : chaque sommet, aux os dont le segment (repos) est
		/// le plus proche, en 1/d^2 (les 4 plus proches).
		void NkAutoPoidsDistance2D(const NkSquelette2D &s, NkMaillage2D &m) noexcept;
		/// AUTO-POIDS PAR CHALEUR (« bone heat » de Blender, Baran et Popovic 2007) :
		/// l'os le plus proche chauffe chaque sommet, la chaleur DIFFUSE le long des
		/// aretes du maillage ((-L + H) w = H p, resolu par Jacobi). Des poids lisses
		/// qui ne sautent pas d'une jambe a l'autre a travers le vide.
		void NkAutoPoidsChaleur2D(const NkSquelette2D &s, NkMaillage2D &m) noexcept;
		/// Une PARTIE = UN OS : une partie qui porte le nom d'un os lui donne ses
		/// sommets au poids plein. Rend le nombre de parties appariees.
		uint32 NkPoidsParParties2D(const NkSquelette2D &s, NkMaillage2D &m) noexcept;
		/// Retire l'os `j` des poids (les indices suivants descendent d'un cran).
		void NkPoidsSansOs2D(NkMaillage2D &m, uint32 j) noexcept;

		// =====================================================================
		// EDITION (l'editeur, les bancs, les scripts)
		// =====================================================================
		/// Un os par sa TETE et sa QUEUE au repos (repere de l'entite), enfant de
		/// `parent` (-1 : racine). Sa pose = son repos. Rend son indice, -1 (plein, nom pris...).
		int32 NkSqueletteAjouterOs(NkSquelette2D &s, const char *nom, int32 parent, const NkVec2f &tete, const NkVec2f &queue) noexcept;
		/// Retire l'os `j` : ses enfants passent a son parent (a leur place) ; les
		/// poids, emplacements, chaines et IK suivent. `m` (facultatif) : sa peau.
		bool NkSqueletteRetirerOs(NkSquelette2D &s, NkMaillage2D *m, uint32 j) noexcept;
		/// Le repos de `j` par sa tete et sa queue (repere de l'entite) ; ses enfants
		/// le suivent (leurs locaux gardes). Sa pose = son nouveau repos.
		void NkSqueletteReposTeteQueue(NkSquelette2D &s, uint32 j, const NkVec2f &tete, const NkVec2f &queue) noexcept;
		/// La pose revient au repos (tous les os ; emplacements compris).
		void NkSqueletteRetourRepos(NkSquelette2D &s) noexcept;
		/// LA SYMETRIE G/D : l'os `j` (nom en « G » ou « D ») et ses descendants sont
		/// recopies en miroir (x -> -x dans le repere de l'entite), noms inverses ;
		/// un os miroir qui existe deja est remis en miroir. Rend le nombre d'os touches.
		uint32 NkSqueletteSymetrie(NkSquelette2D &s, uint32 j) noexcept;
		/// UN MODELE DE DEPART (NKAnima), mis a la boite [lo, hi] : remplace les os.
		bool NkSqueletteModele(NkSquelette2D &s, anim::NkSkeleton2DTemplate t, const NkVec2f &lo, const NkVec2f &hi) noexcept;
		/// L'IK A DEUX OS sur la POSE : `mid` et son parent tournent pour que
		/// `effecteur` (repere local de `mid`) aille sur `cible` (repere de l'entite).
		bool NkSqueletteIK2D(NkSquelette2D &s, uint32 mid, const NkVec2f &effecteur, const NkVec2f &cible, bool coudePositif,
							 float32 melange = 1.f) noexcept;
		/// Le coude de la chaine (mid, son parent) est-il a GAUCHE de racine -> bout ?
		bool NkSqueletteCoudePositif2D(const NkSquelette2D &s, uint32 mid, const NkVec2f &effecteur) noexcept;
		/// Ajoute un emplacement sur l'os `os`, d'attache la partie `partie`. Rend son indice.
		int32 NkSqueletteAjouterEmplacement(NkSquelette2D &s, const char *nom, int32 os, int32 partie) noexcept;
		/// Ajoute une chaine molle de `nombre` os depuis `premier`. Rend son indice.
		int32 NkSqueletteAjouterChaine(NkSquelette2D &s, uint32 premier, uint32 nombre) noexcept;

		// =====================================================================
		// NKANIMA : LE MEME SQUELETTE, LE MEME FICHIER, LES MEMES CLIPS
		// =====================================================================
		/// Vers / depuis NKAnima. Les attaches d'un emplacement y sont NOMMEES par le
		/// nom de leur partie dans `m` (« Partie 3 » si `m` est nul).
		bool NkSqueletteVersNKAnima(const NkSquelette2D &s, const NkMaillage2D *m, anim::NkSkeleton2D &out);
		bool NkSqueletteDepuisNKAnima(NkSquelette2D &s, const anim::NkSkeleton2D &in, const NkMaillage2D *m);
		/// Le .nkskel (NKAnima/Skeleton/NkSkeleton2D.h). La source n'est pas changee.
		bool NkSauverSquelette2D(const NkSquelette2D &s, const NkMaillage2D *m, const char *chemin);
		bool NkChargerSquelette2D(NkSquelette2D &s, const NkMaillage2D *m, const char *chemin);
		/// La pose melangee de NKAnima (os par NOM, emplacements par leurs pistes)
		/// ecrite dans la pose du squelette. Rend le nombre d'os ecrits. Un os que
		/// le clip ne cle pas garde sa pose.
		uint32 NkSqueletteAppliquerPose(NkSquelette2D &s, const anim::NkAnimPose &pose) noexcept;
		/// Une CLE de la pose courante dans `clip` a `t`, pour les os coches dans
		/// `choisis` (nul : tous). Le clip est prepare pour ce squelette s'il ne l'est
		/// pas (mode local, noms, parents, repos cle a 0). Rend le nombre de cles.
		uint32 NkSqueletteCle(const NkSquelette2D &s, anim::NkAnimationClip &clip, float32 t, const uint8 *choisis = nullptr,
							  anim::NkInterpMode interp = anim::NkInterpMode::NK_LINEAR);
		/// L'os du squelette qui porte la piste d'os `j` du clip (par nom), -1 sinon.
		int32 NkSqueletteOsDuClip(const NkSquelette2D &s, const anim::NkAnimationClip &clip, uint32 j) noexcept;
		/// Le chemin d'un os (« Hanches/Torse/BrasG »), pour la frise et les masques.
		NkString NkSqueletteCheminOs(const NkSquelette2D &s, uint32 j);

		// =====================================================================
		// EN JEU
		// =====================================================================
		/// A chaque pas fixe, AVANT la physique : les chaines molles naissent (une
		/// scene a particules), leur maillon de tete suit l'os qui la tient.
		void NkSquelettesAvantPasFixe(NkScene &scene, float32 dt);
		/// Apres les animations (NkScene::Pas) : les IK en jeu, puis les chaines
		/// molles reecrivent la pose de leurs os depuis leurs particules.
		void NkSquelettesApresAnimation(NkScene &scene, float32 dt);
		/// Detruit les corps des chaines d'UN squelette ; remet leurs transitoires a zero.
		void NkSqueletteDetruirePhysique(NkScene &scene, NkSquelette2D &s) noexcept;
		/// Remet les transitoires a zero SANS rien detruire (la photo les a recopies).
		void NkSqueletteOublierPhysique(NkSquelette2D &s) noexcept;

		/// Les champs du composant (sauvegarde, photo, prefab), declares par NkScene::Init.
		const NkChampSauve *NkChampsSquelette2D(uint32 &nombre) noexcept;

	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYSQUELETTE_H__
