#pragma once
// -----------------------------------------------------------------------------
// @File    NkAnimMix.h
// @Brief   LE MELANGE D'ANIMATIONS : poses, masques, melange et additif, arbres
//          de melange 1D / 2D, pistes de clips non lineaires (NLA), couches
//          d'un controleur, et la pose finale d'une instance.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE FICHIER EXISTE (2026-10-01 au soir, demande de Rihen)
// =============================================================================
//  « N'oublie pas qu'on peut integrer le MELANGE de plusieurs animations, que
//  ce soit dans NkAnimaEditor, Noge, Unkeny, NKScena ou PV3DE. »
//  Le depot avait des MORCEAUX : le fondu lineaire de la machine a etats,
//  NkBlendTree1D / 2D (os seulement, possedes par l'appelant, jamais pilotes par
//  la machine), NkBlendLocalTRS. Rien pour les couches, les masques, l'additif,
//  ni pour les pistes de PROPRIETES (les animations 2D d'Unkeny) ; le NLA de
//  Noge (NkNLATrack::Evaluate) attendait « une pile de poses ».
//  Ce fichier est cette pile, pour TOUS : une meme pose porte les os (3D) ET
//  les proprietes nommees (2D), et chaque operation les traite ensemble.
//
// =============================================================================
//  LE VOCABULAIRE
// =============================================================================
//   NkAnimPose       os LOCAUX (T, R, S), morphs, valeurs de proprietes ; chaque
//                    propriete porte sa COUVERTURE (0..1) : la part de la pose
//                    qui l'anime. Une propriete qu'un seul des deux clips anime
//                    se fond vers la valeur VIVANTE de l'objet (le consommateur
//                    applique `lerp(vivante, valeur, couverture)`).
//   NkAnimMask       les parties touchees : des articulations (et leurs
//                    descendants) ou des objets (« Bras » couvre « Bras/Main »).
//   NkBlendPose      a <- a vers b au poids w, masque facultatif.
//   NkMakeAdditive / NkApplyAdditive   la difference a une reference, rajoutee.
//   NkBlendSpaceDef  un ARBRE DE MELANGE 1D (vitesse) ou 2D (vitesse x
//                    direction) : des clips places a des points, leurs poids
//                    (voisins lineaires en 1D, « gradient band » d'Unity en 2D,
//                    exact sur chaque point), phases SYNCHRONISEES.
//   NkClipStripTrack (NkAnimation.h) les clips poses d'une sequence (NLA).
//   NkAnimController la machine de BASE, des COUCHES (une machine, un poids, un
//                    masque, remplace ou additif), les arbres et les masques
//                    NOMMES ; fichier .nkanimctl (sections 'LAYR', 'BLND',
//                    'MASK' en plus de celles de la machine, qu'un lecteur
//                    d'avant saute).
//   NkAdvanceController  une image d'UNE instance : chaque couche avance depuis
//                    son NkAnimLayerRuntime (fixe, copiable en octets : Unkeny
//                    le range dans un composant), puis la pose finale.
//
//  CE QU'IL NE FAIT PAS : il n'ecrit dans aucun objet. Le consommateur lit la
//  pose : Unkeny ecrit les proprietes (NkUnkenyAnimateur.cpp), Noge les os,
//  NkAnimaEditor l'apercu.
// -----------------------------------------------------------------------------

#include "NKAnima/Clip/NkAnimation.h"

namespace nkentseu {
	namespace anim {

		struct NkAnimMask;

		/// Retrouver un clip, un masque par leur NOM (le consommateur sait ou ils sont).
		using NkClipLookup = NkFunction<const NkAnimationClip *(const NkString &)>;

		// ── LA POSE ─────────────────────────────────────────────────────────────
		struct NkBoneTRS {
				NkVec3f t = {0.f, 0.f, 0.f};
				NkQuatf r;
				NkVec3f s = {1.f, 1.f, 1.f};
		};

		struct NkPropValue {
				NkString target;   ///< chemin de l'objet (« » = l'objet anime)
				NkString property; ///< « Composant.champ »
				NkAnimationClip::NkPropertyKind kind = NkAnimationClip::NkPropertyKind::NK_NUMBER;
				NkVec4f value = {0.f, 0.f, 0.f, 0.f};
				/// La COUVERTURE (0..1) : 1 = la pose fixe la valeur ; moins = elle se
				/// fond d'autant vers la valeur vivante de l'objet.
				float32 weight = 1.f;
		};

		class NkAnimPose {
			public:
				NkVector<NkBoneTRS> bones; ///< LOCAUX (relatifs au parent)
				NkVector<float32> morphs;
				NkVector<NkPropValue> props;
				/// Le clip dont viennent les os (noms, parents, ordre topologique) :
				/// les masques s'y resolvent, le FK s'y fait. Nul = pas d'os.
				const NkAnimationClip *skeleton = nullptr;

				void Clear();
				bool Empty() const {
					return bones.Empty() && morphs.Empty() && props.Empty();
				}
				int32 FindProp(const NkString &target, const NkString &property) const;
				/// La valeur (target, property), creee (couverture 0) si elle manque.
				NkPropValue &Prop(const NkString &target, const NkString &property,
								  NkAnimationClip::NkPropertyKind kind = NkAnimationClip::NkPropertyKind::NK_NUMBER);
				/// Les os en matrices LOCALES, et l'inverse.
				void ToLocalMatrices(NkVector<NkMat4f> &out) const;
				void FromLocalMatrices(const NkVector<NkMat4f> &in);
				/// Les matrices de SKINNING (FK + inverseBind du squelette) ; faux sans squelette local.
				bool ToSkinning(NkVector<NkMat4f> &out) const;
		};

		/// La pose d'un clip a `t` (temps du clip, NON replie) : os, morphs,
		/// proprietes ; puis ses pistes de CLIPS (NLA) si `lookup` les retrouve.
		/// `depth` borne la recursion (un clip pose qui a lui-meme des clips poses).
		void NkSampleClip(const NkAnimationClip &clip, float32 t, NkAnimPose &out, const NkClipLookup *lookup = nullptr,
						  const NkVector<NkAnimMask> *masks = nullptr, uint32 depth = 0);
		/// Le temps d'un clip pour un etat qui joue depuis `elapsed` s : replie si
		/// le clip boucle, borne sinon.
		float32 NkClipTime(const NkAnimationClip &clip, float32 elapsed);

		// ── LES MASQUES ─────────────────────────────────────────────────────────
		struct NkAnimMask {
				struct Entry {
						NkString path;		 ///< une articulation, ou un objet (« Bras »)
						float32 weight = 1.f;
						bool children = true; ///< ses descendants aussi
				};
				NkString name;
				NkVector<Entry> entries; ///< VIDE = tout, au poids 1

				/// Le poids d'une propriete de l'objet `target` (« Bras/Main » est sous
				/// « Bras »). L'entree la plus PRECISE l'emporte ; aucune = 0.
				float32 TargetWeight(const NkString &target) const;
				/// Le poids de chaque articulation d'un squelette (noms, parents).
				void BoneWeights(const NkAnimationClip &skeleton, NkVector<float32> &out) const;
		};
		const NkAnimMask *NkFindMask(const NkVector<NkAnimMask> *masks, const NkString &name);

		// ── LES OPERATIONS ──────────────────────────────────────────────────────
		/// a <- a melange vers b au poids w (0 = a, 1 = b). `boneWeights` (un poids
		/// par os, issu d'un masque) et `mask` (pour les proprietes) facultatifs.
		void NkBlendPose(NkAnimPose &a, const NkAnimPose &b, float32 w, const NkVector<float32> *boneWeights = nullptr,
						 const NkAnimMask *mask = nullptr);
		/// delta = pose - reference (os : t - t0, r0^-1 r, s / s0 ; proprietes : v - v0).
		void NkMakeAdditive(const NkAnimPose &pose, const NkAnimPose &reference, NkAnimPose &delta);
		/// base <- base + delta x w (os : t + dt w, r · nlerp(I, dr, w), s x (1 + (ds-1) w)).
		/// ⚠️ Seules les proprietes que la BASE anime recoivent l'additif.
		void NkApplyAdditive(NkAnimPose &base, const NkAnimPose &delta, float32 w,
							 const NkVector<float32> *boneWeights = nullptr, const NkAnimMask *mask = nullptr);

		// ── LES ARBRES DE MELANGE ───────────────────────────────────────────────
		struct NkBlendSample {
				NkString clip;
				float32 x = 0.f, y = 0.f; ///< sa place (y ignore en 1D)
				float32 rate = 1.f;		  ///< sa vitesse propre
		};

		struct NkBlendSpaceDef {
				NkString name;
				uint8 dims = 1; ///< 1 ou 2
				NkString paramX, paramY;
				bool syncPhase = true; ///< phases synchronisees (pied gauche = pied gauche)
				NkVector<NkBlendSample> samples;

				/// Les poids des echantillons en (x, y), de somme 1. 1D : les deux
				/// voisins ; 2D : « gradient band » (exact sur chaque point).
				void Weights(float32 x, float32 y, NkVector<float32> &out) const;
				NkBlendSample &AddSample(const NkString &clip, float32 x, float32 y = 0.f);
		};

		/// La pose d'un arbre a la PHASE `phase` (0..1) ; `outDuration` recoit la
		/// duree melangee (pour faire avancer la phase : phase += dt / duree).
		bool NkSampleBlendSpace(const NkBlendSpaceDef &bs, float32 x, float32 y, float32 phase, const NkClipLookup &lookup,
								NkAnimPose &out, float32 *outDuration = nullptr);

		// ── LES PISTES DE CLIPS (NLA) ───────────────────────────────────────────
		/// Pose les pistes de clips a `t` SUR `io` (la pose du dessous), de la
		/// premiere a la derniere : chaque piste melange ses clips (fondus et
		/// chevauchements), puis remplace (poids x couverture, masque) ou s'ajoute.
		void NkEvaluateStrips(const NkVector<NkClipStripTrack> &tracks, float32 t, const NkClipLookup &lookup, NkAnimPose &io,
							  const NkVector<NkAnimMask> *masks = nullptr, uint32 depth = 0);

		// ── LE CONTROLEUR : base, couches, arbres, masques ──────────────────────
		enum class NkLayerMode : uint8 { NK_OVERRIDE = 0, NK_ADDITIVE };

		struct NkAnimLayer {
				NkString name;
				float32 weight = 1.f;
				NkLayerMode mode = NkLayerMode::NK_OVERRIDE;
				NkString mask;		  ///< un masque du controleur, vide = tout
				NkString weightParam; ///< un parametre reel de la base qui MULTIPLIE le poids (vide = aucun)
				/// Sa machine : ses parametres sont lus dans la BASE (memes noms).
				NkAnimStateMachine machine;
		};

		static constexpr uint32 NK_ANIM_MAX_LAYERS = 4; ///< base comprise

		class NkAnimController {
			public:
				NkString name;
				NkAnimStateMachine base;
				NkVector<NkAnimLayer> layers; ///< au plus NK_ANIM_MAX_LAYERS - 1
				NkVector<NkBlendSpaceDef> blendSpaces;
				NkVector<NkAnimMask> masks;

				NkAnimLayer *AddLayer(const NkString &layerName, NkLayerMode mode = NkLayerMode::NK_OVERRIDE,
									  float32 weight = 1.f, const NkString &mask = NkString());
				NkBlendSpaceDef &AddBlendSpace(const NkString &spaceName, uint8 dims, const NkString &paramX,
											   const NkString &paramY = NkString());
				NkAnimMask &AddMask(const NkString &maskName);
				const NkBlendSpaceDef *FindBlendSpace(const NkString &spaceName) const;
				const NkAnimMask *FindMask(const NkString &maskName) const;
				/// Le controleur a-t-il de quoi MELANGER au-dela d'un clip a la fois ?
				bool HasMixing() const;

				/// .nkanimctl : la machine de base (ses sections), puis 'LAYR' (une
				/// machine complete par couche), 'BLND', 'MASK'. Un lecteur de machine
				/// seule (NkAnimStateMachine::LoadBinary) lit la base et saute le reste.
				void SaveToBytes(NkVector<nk_uint8> &out) const;
				bool LoadFromBytes(const nk_uint8 *data, usize size);
				bool SaveBinary(const NkString &path) const;
				bool LoadBinary(const NkString &path);
		};

		/// L'etat d'execution d'UNE couche pour UNE instance. FIXE et COPIABLE EN
		/// OCTETS (Unkeny le range dans un composant photographie).
		struct NkAnimLayerRuntime {
				NkAnimStateMachine::NkRuntime rt;
				float32 phaseCur = 0.f, phaseNext = 0.f; ///< phases des arbres (0..1)
				float32 durCur = 1.f, durNext = 1.f;	 ///< leurs durees melangees
				int32 lastCur = -1, lastNext = -1;
		};
		struct NkAnimControllerRuntime {
				NkAnimLayerRuntime layers[NK_ANIM_MAX_LAYERS]; ///< [0] = la base
		};

		/// La pose d'UNE machine a son etat `lr` (fondu en cours compris, avec la
		/// courbe de sa transition). `ctl` donne les arbres (nul : aucun) ; les
		/// parametres sont lus dans `params` (la base). `reference` (facultatif)
		/// recoit la pose des memes etats au temps 0 (l'additif s'y mesure).
		void NkEvaluateMachine(const NkAnimStateMachine &sm, const NkAnimLayerRuntime &lr, const NkAnimController *ctl,
							   const NkAnimStateMachine &params, const NkClipLookup &lookup, NkAnimPose &out,
							   NkAnimPose *reference = nullptr);

		/// UNE IMAGE d'une instance : la base puis chaque couche AVANCENT de `dt`
		/// depuis `rt` (parametres : ceux de `ctl.base`, que le consommateur pose
		/// avant), les phases des arbres avancent, et `out` recoit la pose finale.
		/// `advanceBase` faux : la base a DEJA avance (le consommateur l'a fait pour
		/// ses propres besoins, Unkeny) -- seules ses phases et les couches avancent.
		void NkAdvanceController(NkAnimController &ctl, NkAnimControllerRuntime &rt, float32 dt, const NkClipLookup &lookup,
								 NkAnimPose &out, bool advanceBase = true);

	} // namespace anim
} // namespace nkentseu
