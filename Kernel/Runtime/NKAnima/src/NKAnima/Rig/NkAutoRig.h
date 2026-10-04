#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Rig/ — le rig 3D : armature editable (tete/queue/roulis), rig automatique, document du rig.
// -----------------------------------------------------------------------------
// FICHIER: NKAnima/Rig/NkAutoRig.h
// DESCRIPTION: LE RIG AUTOMATIQUE (facon Mixamo, AccuRIG d'iClone) : a partir
//   d'un maillage SANS squelette, poser un squelette MODELE (humanoide,
//   quadrupede, chaine) en le calant sur le maillage.
//
//   1. DETECTER des REPERES (menton, poignets, coudes, aine, genoux...) par la
//      geometrie seule : la boite, des COUPES horizontales du maillage (on
//      compte ses ilots : deux jambes sous l'aine, un tronc au-dessus), la
//      symetrie gauche/droite, l'axe principal des bras.
//   2. L'UTILISATEUR CORRIGE les reperes a la main (ils se deplacent dans la
//      vue ; la symetrie deplace le repere miroir avec).
//   3. CONSTRUIRE l'armature depuis les reperes (noms .L/.R, parents, roulis).
//   4. ENGENDRER les CONTROLES (Rigify, Control Rig d'UE5) : IK a deux os des
//      bras et des jambes (cible + pole), la colonne, la racine, le regard.
//
// ⚠️ CONVENTIONS : Y en haut, le personnage regarde +Z, sa gauche est +X
//    (glTF). Un maillage couche ou tourne donne des reperes faux -- ils se
//    corrigent a la main, et le rapport dit ce qui a ete suppose.
// -----------------------------------------------------------------------------

#include "NKAnima/Rig/NkArmature.h"
#include "NKAnima/Skin/NkSkinMesh.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace anim {

		enum class NkRigTemplate : uint8 { NK_HUMANOIDE = 0, NK_QUADRUPEDE, NK_CHAINE, NK_COUNT };
		const char *NkRigTemplateName(NkRigTemplate t);

		/// Un REPERE du rig automatique.
		struct NkRigLandmark {
				NkString id;	///< « poignet.L » (le suffixe dit le cote, la symetrie s'en sert)
				NkString label; ///< « Poignet gauche »
				math::NkVec3f position{0.f, 0.f, 0.f};
				bool manuel = false; ///< deplace par l'utilisateur (ou l'IA) depuis la detection
		};

		struct NkAutoRigReport {
				bool ok = false;
				NkString message;	  ///< ce qui a ete trouve, ce qui a ete suppose
				float32 hauteur = 0.f;
				float32 aineY = 0.f;
				float32 demiTronc = 0.f;
				uint32 coupes = 0;
		};

		struct NkAutoRigOptions {
				uint32 coupes = 96;		 ///< coupes horizontales du maillage
				uint32 maillonsChaine = 6; ///< NK_CHAINE : nombre d'os
				uint32 vertebres = 3;	 ///< humanoide : os de la colonne (bassin compris)
		};

		/// Les reperes d'un modele, a leur place par defaut dans une boite (sans
		/// maillage : un point de depart que l'utilisateur pose lui-meme).
		void NkRigDefaultLandmarks(NkRigTemplate t, const math::NkVec3f &bmin, const math::NkVec3f &bmax, NkVector<NkRigLandmark> &out);
		/// DETECTE les reperes sur le maillage (positions de repos).
		bool NkRigDetectLandmarks(const NkSkinMesh &mesh, NkRigTemplate t, const NkAutoRigOptions &opt, NkVector<NkRigLandmark> &out,
								  NkAutoRigReport *report = nullptr);
		/// Le repere d'id `id`, ou -1.
		int32 NkRigFindLandmark(const NkVector<NkRigLandmark> &l, const char *id);
		/// Deplace un repere ; `symetrie` : son miroir (poignet.L <-> poignet.R) suit en miroir X.
		bool NkRigMoveLandmark(NkVector<NkRigLandmark> &l, const char *id, const math::NkVec3f &p, bool symetrie);
		/// CONSTRUIT l'armature du modele depuis les reperes.
		bool NkRigBuildArmature(NkRigTemplate t, const NkVector<NkRigLandmark> &l, const NkAutoRigOptions &opt, NkArmature &out);

		// =====================================================================
		// LES CONTROLES (Rigify / Control Rig)
		// =====================================================================
		enum class NkRigControlKind : uint8 {
			NK_IK_DEUX_OS = 0, ///< bras, jambe : cible de l'extremite + pole du coude/genou
			NK_COLONNE,		   ///< la colonne : une poignee au sommet, la chaine se courbe
			NK_RACINE,		   ///< deplace tout le personnage
			NK_REGARD,		   ///< la tete vise la cible
			NK_COUNT
		};
		const char *NkRigControlKindName(NkRigControlKind k);

		struct NkRigControl {
				NkString name;		  ///< « IK_bras.L »
				NkRigControlKind kind = NkRigControlKind::NK_IK_DEUX_OS;
				NkVector<int32> chain; ///< os, de la racine de la chaine a son bout
				math::NkVec3f target{0.f, 0.f, 0.f}; ///< au repos : le bout de la chaine
				math::NkVec3f pole{0.f, 0.f, 0.f};	 ///< IK : ou pointe le coude / le genou
				bool actif = true;
		};

		/// Les controles d'un modele, sur l'armature construite (par les NOMS du modele).
		void NkRigGenerateControls(NkRigTemplate t, const NkArmature &arm, NkVector<NkRigControl> &out);

		/// L'IK A DEUX OS en 3D : la chaine a -> b -> c (positions MONDE) atteint
		/// `cible`, le coude dans le plan (a, cible, pole). Rend les nouvelles
		/// positions de b et c ; faux si la cible est hors de portee (la chaine se
		/// tend alors vers elle).
		bool NkSolveTwoBoneIK(const math::NkVec3f &a, const math::NkVec3f &b, const math::NkVec3f &c, const math::NkVec3f &cible,
							  const math::NkVec3f &pole, math::NkVec3f &bOut, math::NkVec3f &cOut);
		/// La meme, sur des MATRICES monde de joints : `world[ia]`, `world[ib]`,
		/// `world[ic]` tournent (rotation minimale) pour que les joints arrivent sur
		/// les positions resolues ; les enfants de la chaine ne sont pas touches
		/// (l'appelant refait la FK).
		bool NkApplyTwoBoneIK(math::NkMat4f *world, uint32 ia, uint32 ib, uint32 ic, const math::NkVec3f &cible, const math::NkVec3f &pole);

	} // namespace anim
} // namespace nkentseu
