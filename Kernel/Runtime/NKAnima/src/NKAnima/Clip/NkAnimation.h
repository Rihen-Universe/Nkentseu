#pragma once
// Clip/ — les clips : cles, pistes, echantillonnage, melange, machine d'etats.
// =============================================================================
// NKAnima/NkAnimation.h — modele d'animation, substrat autonome
// -----------------------------------------------------------------------------
// Clips, echantillonnage, lecture, melange 1D/2D et machine a etats hierarchique.
// (HIERARCHIQUE pour de vrai depuis le 2026-09-29 : jusque-la ces lignes disaient
// « HFSM » d'une machine PLATE -- un niveau, trois conditions, aucune sous-machine.)
// AUCUN GPU, AUCUN peripherique, AUCUN format : ce fichier ne connait que
// Foundation. C'est ce qui permet a NkAnima, PV3DE, Noge et NKScena d'animer
// sans tirer le renderer -- et a une application 2D d'animer tout court, ce que
// la regle d'exclusivite NKCanvas/NKRenderer interdisait jusqu'ici.
//
// Extrait de NKAnima/NkAnimation.h le 2026-08-14, en
// application du bloc de decision « SUBSTRATS ANIMATION ET COMPORTEMENT »
// (CLAUDE.md du repertoire parent). Le corps est deplace TEL QUEL.
//
// CE QUI EST RESTE AU RENDERER, et pourquoi : la classe `NkAnimationSystem`
// (facade qui televerse les matrices, soumet les meshes skinnes et pilote le
// compute de morph) vit toujours dans NKRenderer/Tools/Animation/. Elle CONSOMME
// ce fichier. La frontiere est la : ce qui calcule une pose est ici, ce qui la
// dessine est la-bas.
//
// Categories couvertes : squelette/bones, morph targets, UV/texture, couleur,
// transform objet, camera, lumiere, post-process, proprietes nommees. Les trois
// dernieres ne sont que des COURBES nommees ; les appliquer est le travail du
// consommateur, pas d'ici.
//
//   NkAnimationClip   — donnees de keyframes (read-only pendant la lecture)
//   NkAnimationPlayer — joue un clip, maintient le temps courant
//   NkAnimationState  — snapshot evalue a un instant t
//   NkBlendTree1D/2D  — melange de clips, bone-local, AVANT la FK
//   NkAnimStateMachine— HFSM : sous-machines et etat d'entree, transitions vers et
//                       depuis un composite, any-state a chaque niveau, declencheurs
//                       consommes, conditions combinees, priorites, crossfade entre
//                       feuilles de niveaux differents, parametres partages,
//                       sauvegarde .nkanimctl (« controleur d'animation »)
//
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen — LICENCE : usage regi par le fichier LICENSE a la racine du depot
// =============================================================================

#ifndef __NKENTSEU_NKANIMATION_NKANIMATION_H__
#define __NKENTSEU_NKANIMATION_NKANIMATION_H__

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKContainers/Associative/NkHashMap.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace anim {

		// Meme choix que NkRendererTypes.h l.33 pour le namespace renderer : les
		// types math sont importes, de sorte que NkMat4f/NkVec3f/NkVec4f restent
		// ecrits sans qualification dans tout ce fichier. Le corps deplace n'a
		// ainsi PAS ete retouche -- 49 references de type auraient du l'etre.
		using namespace math;

		// =========================================================================
		// Mode d'interpolation
		// =========================================================================
		enum class NkInterpMode : uint8 {
			NK_STEP,		// Valeur constante jusqu'au prochain keyframe
			NK_LINEAR,		// Lerp
			NK_CUBIC,		// Spline Catmull-Rom
			NK_EASE_IN,		// Acceleration au départ
			NK_EASE_OUT,	// Décélération à l'arrivée
			NK_EASE_IN_OUT, // Les deux
			NK_BOUNCE,		// Rebond
			NK_ELASTIC,		// Élastique
			NK_BACK,		// Léger dépassement
		};

		// =========================================================================
		// Mode de lecture
		// =========================================================================
		enum class NkPlayMode : uint8 {
			NK_ONCE,	  // Une seule fois, puis stop
			NK_LOOP,	  // Boucle infinie
			NK_PING_PONG, // Aller-retour
			NK_CLAMP,	  // Figé à la dernière frame
		};

		// =========================================================================
		// Keyframe générique
		// =========================================================================
		template <typename T> struct NkKeyframe {
				float32 time;
				T value;
				NkInterpMode interp = NkInterpMode::NK_LINEAR;
		};

		// =========================================================================
		// NkAnimationTrack<T> — courbe d'animation d'une valeur typée
		// =========================================================================
		template <typename T> class NkAnimationTrack {
			public:
				NkString name;
				bool enabled = true;

				void AddKey(float32 t, const T &v, NkInterpMode interp = NkInterpMode::NK_LINEAR) {
					NkKeyframe<T> kf;
					kf.time = t;
					kf.value = v;
					kf.interp = interp;
					uint32 i = 0;
					while (i < (uint32)mKeys.Size() && mKeys[i].time < t)
						i++;
					mKeys.Insert(mKeys.Begin() + i, kf);
				}

				T Evaluate(float32 t) const {
					if (mKeys.Empty())
						return T{};
					uint32 n = (uint32)mKeys.Size();
					if (t <= mKeys[0].time)
						return mKeys[0].value;
					if (t >= mKeys[n - 1].time)
						return mKeys[n - 1].value;
					uint32 hi = 1;
					while (hi < n && mKeys[hi].time <= t)
						hi++;
					uint32 lo = hi - 1;
					float32 dt = mKeys[hi].time - mKeys[lo].time;
					float32 a = (dt > 1e-6f) ? (t - mKeys[lo].time) / dt : 0.f;
					return Lerp(mKeys[lo].value, mKeys[hi].value, EaseAlpha(a, mKeys[lo].interp));
				}

				bool Empty() const {
					return mKeys.Empty();
				}

				float32 GetDuration() const {
					return mKeys.Empty() ? 0.f : mKeys[mKeys.Size() - 1].time;
				}

				uint32 KeyCount() const {
					return (uint32)mKeys.Size();
				}

				const NkKeyframe<T> &GetKey(uint32 i) const {
					return mKeys[i];
				} // serialisation

				// ── Édition (timeline) ────────────────────────────────────────────────
				// Indice de la clé au temps ~t (tolérance), ou -1.
				int32 FindKeyAtTime(float32 t, float32 tol = 1e-4f) const {
					for (uint32 i = 0; i < (uint32)mKeys.Size(); ++i) {
						float32 d = mKeys[i].time - t;
						if (d < 0.f)
							d = -d;
						if (d <= tol)
							return (int32)i;
					}
					return -1;
				}

				bool RemoveKeyAt(uint32 i) {
					if (i >= (uint32)mKeys.Size())
						return false;
					mKeys.Erase(mKeys.Begin() + i);
					return true;
				}

				bool SetKeyInterp(uint32 i, NkInterpMode m) {
					if (i >= (uint32)mKeys.Size())
						return false;
					mKeys[i].interp = m;
					return true;
				}

				// Déplace la clé i au temps newTime (re-trie). Retourne le nouvel indice.
				int32 MoveKey(uint32 i, float32 newTime) {
					if (i >= (uint32)mKeys.Size())
						return -1;
					NkKeyframe<T> kf = mKeys[i];
					kf.time = newTime;
					mKeys.Erase(mKeys.Begin() + i);
					uint32 j = 0;
					while (j < (uint32)mKeys.Size() && mKeys[j].time < newTime)
						++j;
					mKeys.Insert(mKeys.Begin() + j, kf);
					return (int32)j;
				}

			private:
				NkVector<NkKeyframe<T>> mKeys;

				static float32 EaseAlpha(float32 a, NkInterpMode m) {
					switch (m) {
						case NkInterpMode::NK_STEP:
							return 0.f;
						case NkInterpMode::NK_EASE_IN:
							return a * a;
						case NkInterpMode::NK_EASE_OUT:
							return 1.f - (1.f - a) * (1.f - a);
						case NkInterpMode::NK_EASE_IN_OUT:
							return a < 0.5f ? 2.f * a * a : 1.f - (-2.f * a + 2.f) * (-2.f * a + 2.f) * 0.5f;
						case NkInterpMode::NK_BOUNCE: {
							float32 n = 1.f - a;
							float32 d = 2.75f, b = 7.5625f;
							if (n < 1.f / d)
								return 1.f - b * n * n;
							if (n < 2.f / d) {
								n -= 1.5f / d;
								return 1.f - (b * n * n + 0.75f);
							}
							if (n < 2.5f / d) {
								n -= 2.25f / d;
								return 1.f - (b * n * n + 0.9375f);
							}
							n -= 2.625f / d;
							return 1.f - (b * n * n + 0.984375f);
						}
						case NkInterpMode::NK_ELASTIC: {
							float32 c4 = (2.f * 3.14159f) / 3.f;
							if (a <= 0.f)
								return 0.f;
							if (a >= 1.f)
								return 1.f;
							return (float32)powf(2.f, -10.f * a) * sinf((a * 10.f - 0.75f) * c4) + 1.f;
						}
						case NkInterpMode::NK_BACK: {
							float32 c1 = 1.70158f, c3 = c1 + 1.f;
							return 1.f + c3 * (a - 1.f) * (a - 1.f) * (a - 1.f) + c1 * (a - 1.f) * (a - 1.f);
						}
						default:
							return a;
					}
				}

				// Lerp générique — overloads dans .cpp
				T Lerp(const T &a, const T &b, float32 t) const;
		};

		// =========================================================================
		// NkAnimationClip — ensemble de tracks décrivant une animation complète
		// =========================================================================
		class NkAnimationClip {
			public:
				NkString name;
				float32 duration = 0.f;
				float32 fps = 30.f;
				bool loop = true;

				// ── Skeletal ─────────────────────────────────────────────────────────
				NkVector<NkAnimationTrack<NkMat4f>> boneTracks; // un par bone
				uint32 boneCount = 0;

				// Mode LOCAL (M1) : si true, boneTracks = matrices bone-LOCAL (relatives
				// au parent JOINT), PAS des matrices de skinning. Le player fait alors FK
				// (global = parent×local) + skinning (global×inverseBind) au sample. Avantages :
				// interp slerp CORRECTE (transforms rigides locaux, pas de scale composite),
				// édition de pose naturelle (timeline), retargetable. Squelette requis :
				bool skeletalLocal = false;
				NkVector<int32> jointParent;		// parent de chaque joint (-1 = racine)
				NkVector<NkMat4f> jointInverseBind; // inverseBind par joint
				NkVector<uint32> jointTopo;			// ordre topo (parent avant enfant)
				// Noms des joints (node glTF "name" — VIDE si le fichier n'en a pas).
				// Ajout 2026-08-17, additif : alimente la distribution de masse
				// anthropométrique (NkPoseMass::SetAnthropometric) et la détection
				// des appuis par nom (foot/ankle) côté éditeur. Un clip sans noms
				// reste valide : les consommateurs retombent sur la masse uniforme.
				NkVector<NkString> jointNames;
				// Convertit en place des matrices bone-LOCAL en matrices de SKINNING
				// (FK hiérarchique + inverseBind). Utilisé par le player en mode local.
				void ApplyFKSkinning(NkVector<NkMat4f> &boneLocalToSkin) const;

				// ── Morph targets ─────────────────────────────────────────────────────
				NkVector<NkAnimationTrack<float32>> morphTracks;
				NkVector<NkString> morphNames;

				// ── UV / Sprite ───────────────────────────────────────────────────────
				NkAnimationTrack<NkVec2f> uvOffset;
				NkAnimationTrack<NkVec2f> uvScale;
				NkAnimationTrack<float32> uvRotation;
				NkAnimationTrack<int32> spriteFrame;
				uint32 spriteAtlasCols = 1;
				uint32 spriteAtlasRows = 1;

				// ── Matériau ──────────────────────────────────────────────────────────
				NkAnimationTrack<NkVec4f> albedoColor;
				NkAnimationTrack<NkVec3f> emissiveColor;
				NkAnimationTrack<float32> emissiveStrength;
				NkAnimationTrack<float32> metallic;
				NkAnimationTrack<float32> roughness;
				NkAnimationTrack<float32> opacity;
				NkHashMap<NkString, NkAnimationTrack<float32>> customFloats;
				NkHashMap<NkString, NkAnimationTrack<NkVec4f>> customVec4s;

				// ── Transform objet ───────────────────────────────────────────────────
				NkAnimationTrack<NkVec3f> position;
				NkAnimationTrack<NkVec4f> rotation; // quaternion (x,y,z,w)
				NkAnimationTrack<NkVec3f> scale;

				// ── Caméra ────────────────────────────────────────────────────────────
				NkAnimationTrack<NkVec3f> cameraPosition;
				NkAnimationTrack<NkVec3f> cameraTarget;
				NkAnimationTrack<float32> cameraFOV;
				NkAnimationTrack<float32> cameraDOFFocus;
				NkAnimationTrack<float32> cameraDOFAperture;

				// ── Lumière ───────────────────────────────────────────────────────────
				NkAnimationTrack<float32> lightIntensity;
				NkAnimationTrack<NkVec3f> lightColor;
				NkAnimationTrack<NkVec3f> lightPosition;
				NkAnimationTrack<float32> lightRange;

				// ── Post-process ──────────────────────────────────────────────────────
				NkAnimationTrack<float32> ppExposure;
				NkAnimationTrack<float32> ppSaturation;
				NkAnimationTrack<float32> ppContrast;
				NkAnimationTrack<float32> ppBloomStrength;
				NkAnimationTrack<float32> ppDOFFocus;
				NkAnimationTrack<float32> ppVignetteIntensity;

				// ── Pistes de PROPRIETES (2026-10-01, pages Animation d'Unkeny, R21/R35) ──
				// Une piste = UNE propriete NOMMEE d'un objet : « Transform.position »
				// de « » (l'objet anime lui-meme) ou de « Bras/Main » (un descendant,
				// designe par le chemin de ses noms). La valeur tient dans un NkVec4f :
				// nombre (x), vecteur (xy, xyz, xyzw), couleur (rgba, 0..1). Le GENRE dit
				// comment la lire ; « par paliers » (booleen, entier, enumeration,
				// reference d'asset) pose ses cles en NK_STEP : la valeur ne glisse pas
				// d'une image a l'autre, elle saute.
				// ⚠️ AJOUTER UNE PROPRIETE NE TOUCHE PAS CE FICHIER : c'est une piste de
				//    plus, nommee. NKAnima ne sait pas ce qu'est un composant et ne
				//    l'applique pas : le consommateur (Unkeny) lit Evaluate et ecrit sa
				//    propriete. Les pistes fixes plus haut (position, albedoColor...)
				//    restent celles du rendu 3D ; celles-ci servent les editeurs.
				enum class NkPropertyKind : uint8 { NK_NUMBER = 0, NK_VEC2, NK_VEC3, NK_VEC4, NK_COLOR, NK_STEP };
				struct NkPropertyTrack {
						NkString target;   ///< chemin de l'objet ("" = l'objet anime)
						NkString property; ///< « Composant.champ »
						NkPropertyKind kind = NkPropertyKind::NK_NUMBER;
						NkAnimationTrack<NkVec4f> curve;
				};
				NkVector<NkPropertyTrack> propertyTracks;
				/// La piste (target, property), ou nul.
				NkPropertyTrack *FindPropertyTrack(const NkString &target, const NkString &property);
				/// La piste (target, property), creee vide si elle manque (le genre
				/// n'est pose qu'a la creation).
				NkPropertyTrack &AddPropertyTrack(const NkString &target, const NkString &property, NkPropertyKind kind);

				// ── Helpers ───────────────────────────────────────────────────────────
				void RecalcDuration();

				void AddBoneKey(uint32 boneIdx, float32 time, const NkMat4f &mat,
								NkInterpMode interp = NkInterpMode::NK_LINEAR);
				void ResizeBones(uint32 count);

				// ── Sérialisation BINAIRE .nkanim (M1) ────────────────────────────────
				// Format compact versionné (header NKAN + tracks d'os). Pas de JSON :
				// l'anim = beaucoup de keyframes (floats) -> binaire = compact + chargement
				// rapide sans parsing. Sert l'app NkAnima ET le moteur de jeu (rejouer un clip).
				// (2026-10-01) Un clip qui porte des pistes de PROPRIETES s'ecrit en v4
				// (corps v2 + section 'PROP') ; sans elles, toujours en v2, octet pour
				// octet comme avant.
				bool SaveBinary(const NkString &path) const;
				bool LoadBinary(const NkString &path);

				// ── Import glTF : PLUS ICI ─────────────────────────────────────────────
				// `BakeFromGLTF` etait une methode de cette classe ; c'etait le SEUL
				// lien entre le modele d'animation et le chargeur glTF, et il suffisait
				// a retenir toute l'animation dans le renderer. Depuis le 2026-08-14
				// c'est une FONCTION LIBRE, du cote qui connait le format :
				//     renderer::BakeClipFromGLTF(data, animIdx, fps, clip)
				//     -> NKRenderer/Mesh/NkGLTFAnimBake.h
				// Ne pas la reintroduire ici : ce fichier ne doit connaitre aucun format.

				void BuildSpriteFlipBook(uint32 frameCount, float32 spriteFPS = 12.f);

				// Clips prédéfinis
				static NkAnimationClip *MakeSpinClip(const NkString &name, float32 rpm, NkVec3f axis = {0, 1, 0});
				static NkAnimationClip *MakeLightPulse(const NkString &name, float32 minI, float32 maxI,
													   float32 freq = 1.f);
				static NkAnimationClip *MakeColorFade(const NkString &name, NkVec4f from, NkVec4f to, float32 dur,
													  NkInterpMode interp = NkInterpMode::NK_EASE_IN_OUT);
				static NkAnimationClip *MakeCameraShake(const NkString &name, float32 intensity, float32 dur,
														float32 freq = 15.f);
				static NkAnimationClip *MakeProceduralWalk(uint32 boneCount, float32 dur = 1.f);
		};

		// =========================================================================
		// NkAnimationState — état évalué, prêt à être appliqué au renderer
		// =========================================================================
		struct NkAnimationState {
				// Skeletal
				NkVector<NkMat4f> boneMatrices;
				// Morph
				NkVector<float32> morphWeights;
				// UV
				NkVec2f uvOffset = {0, 0};
				NkVec2f uvScale = {1, 1};
				float32 uvRotation = 0.f;
				int32 spriteFrame = 0;
				NkVec4f spriteUV = {0, 0, 1, 1}; // x0,y0,x1,y1 dans l'atlas
				// Matériau
				NkVec4f albedo = {1, 1, 1, 1};
				NkVec3f emissive = {0, 0, 0};
				float32 emissiveStrength = 0.f;
				float32 metallic = 0.f;
				float32 roughness = 0.5f;
				float32 opacity = 1.f;
				// Transform
				NkVec3f position = {0, 0, 0};
				NkVec4f rotation = {0, 0, 0, 1};
				NkVec3f scale = {1, 1, 1};
				NkMat4f transform;
				// Caméra
				NkVec3f camPos = {0, 0, 5};
				NkVec3f camTarget = {0, 0, 0};
				float32 camFOV = 65.f;
				float32 camDOFFocus = 10.f;
				float32 camDOFApt = 0.1f;
				// Lumière
				float32 lightIntensity = 1.f;
				NkVec3f lightColor = {1, 1, 1};
				NkVec3f lightPos = {0, 5, 0};
				float32 lightRange = 10.f;
				// Post-process
				float32 ppExposure = 1.f;
				float32 ppSaturation = 1.f;
				float32 ppContrast = 1.f;
				float32 ppBloom = 0.04f;
				float32 ppDOFFocus = 10.f;
				float32 ppVignette = 0.f;
		};

		// =========================================================================
		// NkAnimationPlayer — joue un NkAnimationClip
		// =========================================================================
		class NkAnimationPlayer {
			public:
				NkString name;

				void SetClip(const NkAnimationClip *clip, bool autoResize = true);

				const NkAnimationClip *GetClip() const {
					return mClip;
				}

				void Play(NkPlayMode mode = NkPlayMode::NK_LOOP, float32 speed = 1.f);
				void Pause();
				void Stop();
				void SeekTo(float32 t);

				bool IsPlaying() const {
					return mPlaying;
				}

				float32 GetTime() const {
					return mTime;
				}

				float32 GetNormTime() const;
				int32 GetFrame() const;

				void Update(float32 dt);

				const NkAnimationState &GetState() const {
					return mState;
				}

				// Crossfade vers un autre clip
				void BlendTo(const NkAnimationClip *next, float32 blendDur = 0.25f);

				bool IsBlending() const {
					return mBlendT > 0.f;
				}

				// Événements temporels (markers)
				using MarkerFn = NkFunction<void(const NkString &)>;
				void AddMarker(float32 t, const NkString &ev);

				void SetMarkerCallback(MarkerFn fn) {
					mMarkerCb = fn;
				}

			private:
				const NkAnimationClip *mClip = nullptr;
				const NkAnimationClip *mNextClip = nullptr;
				float32 mTime = 0.f;
				float32 mSpeed = 1.f;
				bool mPlaying = false;
				NkPlayMode mMode = NkPlayMode::NK_LOOP;
				float32 mBlendT = 0.f;
				float32 mBlendDur = 0.f;
				NkAnimationState mState;
				NkAnimationState mBlendState;
				MarkerFn mMarkerCb;

				struct Marker {
						float32 time;
						NkString name;
						bool fired = false;
				};

				NkVector<Marker> mMarkers;

				void Evaluate(const NkAnimationClip *clip, float32 t, NkAnimationState &out);
				void BlendStates(NkAnimationState &a, const NkAnimationState &b, float32 w);
				void WrapTime(float32 &t, float32 dur);
				void ComputeSpriteUV(NkAnimationState &s, const NkAnimationClip *clip);
				void ComputeTransformMatrix(NkAnimationState &s);
		};

		// =========================================================================
		// NkBlendTree1D — blend space 1D (ex. idle / walk / run pilotes par la
		// vitesse). N clips places sur un axe parametrique ; SetParameter(x)
		// choisit les 2 voisins et les melange BONE-LOCAL (TRS-NLerp par os,
		// AVANT le FK) — le blend est donc correct sur les rotations, pas un
		// lerp de matrices de skinning. Les PHASES sont synchronisees : le temps
		// avance en NORMALISE (0..1) sur une duree interpolee entre les 2 clips
		// (le pied gauche du walk reste le pied gauche du run).
		// Prerequis : clips en mode skeletalLocal partageant le MEME squelette
		// (typiquement BakeClipFromGLTF du meme fichier, ex. Fox Survey/Walk/Run).
		// =========================================================================
		class NkBlendTree1D {
			public:
				NkString name;

				// Ajoute un clip a la position parametrique `pos` (insertion triee).
				void AddClip(const NkAnimationClip *clip, float32 pos);

				// Parametre de blend (clampe sur [posMin, posMax]).
				void SetParameter(float32 x);

				float32 GetParameter() const {
					return mParam;
				}

				void SetSpeed(float32 s) {
					mSpeed = s;
				}

				// Avance le temps normalise et evalue la pose melangee.
				void Update(float32 dt);

				const NkAnimationState &GetState() const {
					return mState;
				}

				float32 GetNormTime() const {
					return mNormTime;
				}

				void SeekNorm(float32 nt) {
					mNormTime = nt - floorf(nt);
				}

				uint32 GetClipCount() const {
					return (uint32)mEntries.Size();
				}

				// Pose BONE-LOCALE melangee (avant FK) — utilisee par la state
				// machine pour un crossfade bone-local correct entre etats.
				// Vide si les clips ne sont pas en mode skeletalLocal.
				const NkVector<NkMat4f> &GetLocalPose() const {
					return mLocalPose;
				}

				// Clip de reference du squelette (parents/topo/inverseBind) pour
				// refaire le FK apres un blend externe des poses locales.
				const NkAnimationClip *GetSkeletonClip() const {
					return mEntries.Empty() ? nullptr : mEntries[0].clip;
				}

			private:
				struct Entry {
						const NkAnimationClip *clip = nullptr;
						float32 pos = 0.f;
				};

				NkVector<Entry> mEntries; // triees par pos croissante
				float32 mParam = 0.f;
				float32 mNormTime = 0.f; // temps normalise 0..1 (phases synchro)
				float32 mSpeed = 1.f;
				NkAnimationState mState;
				NkVector<NkMat4f> mLocalPose; // pose locale melangee (pre-FK)
				NkVector<NkMat4f> mScratch;	  // locaux du clip B pendant le blend
		};

		// =========================================================================
		// NkBlendTree2D — blend space 2D (ex. direction × vitesse d'un perso).
		// N clips places a des POINTS 2D ; SetParameter(x, y) melange les clips
		// par ponderation inverse-distance normalisee (Shepard, puissance 2 —
		// simple et robuste pour des echantillons epars ; hit exact = clip pur).
		// Meme discipline que le 1D : blend BONE-LOCAL cumulatif avant FK,
		// phases synchronisees sur la duree ponderee. Prerequis identiques
		// (clips skeletalLocal, meme squelette).
		// =========================================================================
		class NkBlendTree2D {
			public:
				NkString name;

				void AddClip(const NkAnimationClip *clip, NkVec2f pos);
				void SetParameter(NkVec2f p);

				NkVec2f GetParameter() const {
					return mParam;
				}

				void SetSpeed(float32 s) {
					mSpeed = s;
				}

				void Update(float32 dt);

				const NkAnimationState &GetState() const {
					return mState;
				}

				const NkVector<NkMat4f> &GetLocalPose() const {
					return mLocalPose;
				}

				const NkAnimationClip *GetSkeletonClip() const {
					return mEntries.Empty() ? nullptr : mEntries[0].clip;
				}

				uint32 GetClipCount() const {
					return (uint32)mEntries.Size();
				}

			private:
				struct Entry {
						const NkAnimationClip *clip = nullptr;
						NkVec2f pos = {0.f, 0.f};
						float32 weight = 0.f; // poids normalise de la derniere eval
				};

				NkVector<Entry> mEntries;
				NkVec2f mParam = {0.f, 0.f};
				float32 mNormTime = 0.f;
				float32 mSpeed = 1.f;
				NkAnimationState mState;
				NkVector<NkMat4f> mLocalPose;
				NkVector<NkMat4f> mScratch;
		};

		// =========================================================================
		// NkAnimStateMachine — machine a etats HIERARCHIQUE d'animation (HFSM).
		// -------------------------------------------------------------------------
		// Un etat FEUILLE porte un clip, un blend tree (1D/2D) ou rien (etat vide,
		// utile quand le consommateur lit l'ETIQUETTE de l'etat — Unkeny y range
		// le clip de sprite a jouer). Un etat COMPOSITE est une sous-machine : il
		// contient d'autres etats et entre par son etat d'entree (le premier
		// ajoute, ou celui de SetEntryState). Les poses ne viennent que des
		// feuilles ; une transition vers un composite descend jusqu'a sa feuille
		// d'entree, et le fondu se fait entre FEUILLES, quel que soit leur niveau.
		//
		// Les transitions sont declenchees par des parametres PARTAGES par tous
		// les niveaux (une sous-machine n'a pas de parametres a elle) : bool,
		// seuil float, declencheur CONSOMME au tir, temps passe dans l'etat. Une
		// transition peut porter plusieurs conditions (ET) ; pour un OU, deux
		// transitions. Le crossfade dure `fadeDur` secondes — BONE-LOCAL (blend
		// TRS par os puis un seul FK, correct sur les rotations) quand les deux
		// feuilles exposent leur pose locale sur le meme squelette, sinon
		// fallback matriciel (fondus courts). Evenements via SetTransitionCallback.
		//
		// ⚠️ ORDRE DE CHOIX, et il est ecrit parce qu'il decide des conflits :
		//   a chaque Update (hors fondu), parmi les transitions APPLICABLES —
		//   celles dont l'etat source est sur le chemin actif (la feuille courante
		//   ou l'un de ses ancetres), et les « any-state » dont la portee est sur
		//   ce chemin (la racine l'est toujours) — dont la cible n'est pas deja
		//   active et dont toutes les conditions tiennent, on prend :
		//     1. la plus haute `priority` ;
		//     2. a priorite egale, le niveau le plus ENGLOBANT (quitter « Sol »
		//        l'emporte sur passer de « idle » a « marche » a l'interieur) ;
		//     3. a egalite encore, la premiere ajoutee.
		//   L'API plate d'avant le 2026-09-29 (tout a la racine, priorite 0) tombe
		//   donc exactement sur son ancienne regle : la premiere transition vraie
		//   dans l'ordre d'ajout.
		//
		// Update(dt) : teste les transitions, evalue la feuille courante, avance
		// le fondu. GetState() = pose finale a soumettre au renderer.
		// =========================================================================
		class NkAnimStateMachine {
			public:
				// La racine n'est pas un etat : c'est le conteneur implicite de tout
				// ce qu'on ajoute sans parent. -1 la designe partout ou un parent ou
				// une portee est attendu — la valeur du `from = -1` de l'API plate,
				// qui voulait deja dire « depuis n'importe ou ».
				static constexpr int32 NK_ROOT = -1;

				// Profondeur maximale (les etats de la racine sont au niveau 0).
				// Bornee pour que l'etat d'execution (NkRuntime) reste une valeur de
				// taille FIXE, copiable bit a bit : c'est ce qui permet a Unkeny de
				// le ranger dans un composant, donc de le sauvegarder.
				static constexpr int32 NK_MAX_DEPTH = 8;

				// ── Etats de la racine (API plate, inchangee) ───────────────────
				// Retourne l'index de l'etat. Un seul des pointeurs.
				int32 AddState(const NkString &name, const NkAnimationClip *clip);
				int32 AddState(const NkString &name, NkBlendTree1D *tree);
				int32 AddState(const NkString &name, NkBlendTree2D *tree2d);

				// ── Hierarchie (2026-09-29) ─────────────────────────────────────
				// `parent` = NK_ROOT ou l'index d'une sous-machine. Rend -1 (et le
				// journalise) si le parent n'est pas une sous-machine ou si la
				// profondeur depasserait NK_MAX_DEPTH. Les index restent GLOBAUX :
				// une seule numerotation pour tous les niveaux.
				int32 AddSubMachine(const NkString &name, int32 parent = NK_ROOT);
				int32 AddState(int32 parent, const NkString &name, const NkAnimationClip *clip);
				int32 AddState(int32 parent, const NkString &name, NkBlendTree1D *tree);
				int32 AddState(int32 parent, const NkString &name, NkBlendTree2D *tree2d);
				// Etat sans pose : le consommateur lit son etiquette (SetStateTag).
				int32 AddEmptyState(const NkString &name, int32 parent = NK_ROOT);

				// Etat d'entree d'une sous-machine (ou de la racine, NK_ROOT). Par
				// defaut : son premier enfant. `state` doit etre un enfant DIRECT.
				bool SetEntryState(int32 machine, int32 state);
				int32 GetEntryState(int32 machine) const;

				// Etiquette libre d'un etat (0 par defaut). La machine ne la lit
				// pas ; elle la rend. Unkeny : l'index du clip de sprite a jouer.
				void SetStateTag(int32 state, int32 tag);
				int32 GetStateTag(int32 state) const;

				// ── Transitions ─────────────────────────────────────────────────
				// Transition from -> to declenchee quand :
				//   - param bool `paramName` == true (kind BOOL), ou
				//   - param float `paramName` >  threshold (kind FLOAT_GREATER), ou
				//   - param float `paramName` <  threshold (kind FLOAT_LESS).
				// from = -1 : depuis N'IMPORTE quel etat (any-state transition).
				//
				// ⚠️ Les trois premieres valeurs sont figees (0, 1, 2) : ce sont
				// celles des appelants existants ET du fichier .nkanim v3. Les
				// ajouts du 2026-09-29 viennent APRES, jamais entre.
				enum class NkCondKind : uint8 {
					BOOL_TRUE,
					FLOAT_GREATER,
					FLOAT_LESS,
					BOOL_FALSE,	   // param bool == false (absent = false : la condition tient)
					TRIGGER,	   // declencheur pose par SetTrigger, CONSOMME quand la transition tire
					TIME_IN_STATE, // l'etat source est actif depuis au moins `threshold` s
				};
				void AddTransition(int32 from, int32 to, const NkString &paramName, NkCondKind kind,
								   float32 threshold = 0.f, float32 fadeDur = 0.25f);

				// Transition SANS condition, a completer par AddCondition (toutes
				// doivent tenir : ET). Sans aucune condition, elle tire des que son
				// etat source est actif. `from` peut etre une sous-machine : elle
				// tient alors tant que la feuille courante est dedans. from < 0 :
				// any-state de la racine, comme dans l'API plate. Rend son index.
				int32 AddTransitionEx(int32 from, int32 to, float32 fadeDur = 0.25f, int32 priority = 0);
				// « Depuis n'importe quel etat » de la sous-machine `scope` (NK_ROOT
				// = de partout). Applicable tant que la feuille courante est dans
				// `scope`. Rend son index, ou -1 si `scope` n'est pas une sous-machine.
				int32 AddAnyStateTransition(int32 scope, int32 to, float32 fadeDur = 0.25f, int32 priority = 0);
				// Ajoute une condition a la transition `transition` (index rendu par
				// AddTransitionEx / AddAnyStateTransition). Le parametre est declare
				// au passage, du genre que la condition implique.
				bool AddCondition(int32 transition, const NkString &param, NkCondKind kind, float32 threshold = 0.f);

				uint32 GetTransitionCount() const {
					return (uint32)mTransitions.Size();
				}

				// ── Parametres pilotes par le gameplay ──────────────────────────
				// Un nom peut exister comme bool ET comme float (deux parametres
				// distincts) : c'etait deja le cas avec les deux tables d'avant.
				enum class NkParamKind : uint8 { BOOL, FLOAT, TRIGGER };

				void SetBool(const NkString &name, bool v);
				void SetFloat(const NkString &name, float32 v);
				float32 GetFloat(const NkString &name) const;
				bool GetBool(const NkString &name) const;

				// Un declencheur reste pose jusqu'a ce qu'une transition le
				// CONSOMME (ou ResetTrigger) : poser « saut » pendant un fondu ne
				// le perd pas, il tirera a la fin du fondu.
				void SetTrigger(const NkString &name);
				void ResetTrigger(const NkString &name);
				bool GetTrigger(const NkString &name) const;

				// Declaration explicite (valeur par defaut, genre). Facultative :
				// Set* et AddCondition declarent aussi. Sert a la sauvegarde et a
				// l'inspecteur, qui veulent la liste complete.
				void DeclareParam(const NkString &name, NkParamKind kind, float32 defaultValue = 0.f);
				uint32 GetParamCount() const;
				const NkString &GetParamName(uint32 i) const;
				NkParamKind GetParamKind(uint32 i) const;
				// bool / declencheur : 0 ou 1.
				float32 GetParamValue(uint32 i) const;
				void SetParamValue(uint32 i, float32 v);
				int32 FindParam(const NkString &name, NkParamKind kind) const;
				// Toutes les valeurs reviennent a leur defaut (declencheurs baisses).
				void ResetParams();

				// PARTAGE entre machines : cette machine lit et ecrit desormais les
				// parametres de `owner` (couches haut du corps / locomotion d'un
				// meme personnage). nullptr = revenir aux siens. Refuse un cycle.
				// ⚠️ Un declencheur partage est consomme par la PREMIERE machine qui
				// tire dessus.
				bool ShareParametersWith(NkAnimStateMachine *owner);

				// ── Etat courant ────────────────────────────────────────────────
				// Force un etat sans transition (init / teleport). Une sous-machine
				// y entre par son etat d'entree.
				void ForceState(int32 idx);
				// Retour a l'entree de la racine, sans fondu (parametres gardes).
				void Reset();

				// La FEUILLE courante (pendant un fondu : celle qu'on quitte).
				int32 GetCurrentState() const {
					return mCurrent;
				}

				const NkString &GetCurrentStateName() const;
				// La feuille visee pendant un fondu, -1 sinon.
				int32 GetNextState() const {
					return mNext;
				}
				// Avancement du fondu, 0 -> 1 ; 0 hors fondu.
				float32 GetFadeWeight() const;
				// Vrai si `state` est la feuille courante OU l'un de ses ancetres.
				bool IsInState(int32 state) const;
				// "Sol/marche" : les noms du chemin actif, de la racine a la feuille.
				NkString GetCurrentPath() const;
				// Temps passe dans `state` s'il est actif, -1 sinon.
				float32 GetTimeInState(int32 state) const;

				// ── Lecture de la structure (inspecteur, sauvegarde) ────────────
				int32 GetStateCount() const {
					return (int32)mStates.Size();
				}

				const NkString &GetStateName(int32 state) const;
				int32 GetStateParent(int32 state) const;
				int32 GetStateDepth(int32 state) const;
				bool IsSubMachine(int32 state) const;
				// Par nom ("marche") ou par chemin ("Sol/marche"). -1 si absent.
				int32 FindState(const NkString &nameOrPath) const;

				// ── Lecture complete, pour un EDITEUR (2026-10-01, page Animateur) ──
				// Ce qu'il faut pour redessiner la machine en graphe sans rien perdre :
				// la reference d'un etat, chaque transition et chacune de ses
				// conditions, le defaut d'un parametre. Lecture seule : l'editeur
				// RECONSTRUIT une machine neuve par les Add* quand on enregistre.
				// Genre de reference : 0 vide, 1 clip, 2 arbre 1D, 3 arbre 2D.
				uint8 GetStateRefKind(int32 state) const;
				const NkString &GetStateRef(int32 state) const;
				// Un etat-CLIP designe par son NOM (refKind 1), le clip resolu ou non :
				// c'est ce qu'ecrit un editeur qui n'a pas le clip en memoire. `clip`
				// peut etre nul (l'etat ne pose rien, comme un clip non resolu).
				void SetStateClipRef(int32 state, const NkString &clipName, const NkAnimationClip *clip = nullptr);
				int32 GetTransitionFrom(uint32 t) const;
				int32 GetTransitionTo(uint32 t) const;
				bool IsAnyStateTransition(uint32 t) const;
				int32 GetTransitionScope(uint32 t) const;
				float32 GetTransitionFade(uint32 t) const;
				int32 GetTransitionPriority(uint32 t) const;
				uint32 GetConditionCount(uint32 t) const;
				bool GetCondition(uint32 t, uint32 k, NkString &param, NkCondKind &kind, float32 &threshold) const;
				float32 GetParamDefault(uint32 i) const;

				// ── Disposition dans l'EDITEUR (2026-10-01) ─────────────────────
				// La place de chaque etat dans le graphe, et celle des deux pseudo-
				// noeuds d'un niveau (« Entree », « N'importe quel etat »). La machine
				// ne les lit pas ; elle les GARDE et les ecrit dans une section a part
				// ('GRPH') du .nkanimctl, sautee par un lecteur qui ne la connait pas.
				// Une machine sans disposition s'ecrit octet pour octet comme avant.
				void SetStatePosition(int32 state, float32 x, float32 y);
				bool GetStatePosition(int32 state, float32 &x, float32 &y) const;
				// `machine` = NK_ROOT ou une sous-machine ; `pseudo` 0 = Entree, 1 = N'importe quel etat.
				void SetPseudoPosition(int32 machine, int32 pseudo, float32 x, float32 y);
				bool GetPseudoPosition(int32 machine, int32 pseudo, float32 &x, float32 &y) const;

				void Update(float32 dt);

				const NkAnimationState &GetState() const {
					return mState;
				}

				// Evenements de transition : appele au DECLENCHEMENT (finished=false)
				// puis a la FIN du fondu (finished=true). Sert au gameplay (sons de
				// pas, verrous d'input pendant une action, etc.). Les noms sont ceux
				// des FEUILLES.
				using TransitionFn = NkFunction<void(const NkString &from, const NkString &to, bool finished)>;

				void SetTransitionCallback(TransitionFn fn) {
					mTransitionCb = fn;
				}

				// ── Etat d'EXECUTION, separe de la DEFINITION ───────────────────
				// Tout ce qui change pendant Update, et rien d'autre : une valeur de
				// taille fixe, copiable bit a bit. Sert a sauvegarder une partie, et
				// a faire tourner UNE definition pour N personnages (Unkeny : un
				// modele partage, l'etat de chacun dans son composant).
				// ⚠️ Les parametres n'y sont PAS (le consommateur les tient), ni
				// l'horloge interne des blend trees, qui appartiennent a l'appelant.
				struct NkRuntime {
						int32 current = -1; // -1 = pas encore demarree : entrera par la racine
						int32 next = -1;
						float32 fadeT = 0.f;
						float32 fadeDur = 0.f;
						float32 clock = 0.f;
						float32 currentTime = 0.f; // temps local du clip de la feuille courante
						float32 nextTime = 0.f;
						float32 enteredAt[NK_MAX_DEPTH] = {}; // horloge a l'entree de l'etat actif de chaque niveau
				};

				NkRuntime GetRuntime() const;
				// Un index hors bornes (modele change depuis) fait repartir la
				// machine de son entree plutot que de lire n'importe quoi.
				void SetRuntime(const NkRuntime &rt);

				// ── Sauvegarde .nkanimctl (2026-09-30) ──────────────────────────
				// La machine a SON format et SON extension, `.nkanimctl` (« controleur
				// d'animation », decision de Rihen du 30/09 : un clip et une machine
				// ne se deposent pas au meme endroit avec le meme effet). Le fichier
				// se reconnait a son magic 'NKAC' ; l'appelant le NOMME en .nkanimctl
				// (NkAssetExtensionFor(NkAssetType::AnimationController), dans
				// NKSerialization — NKAnima ne tire pas ce module).
				// Contenu, section 'HFSM' : etats, hierarchie, entrees, etiquettes,
				// parametres et leurs defauts, transitions et conditions. Les clips et
				// blend trees sont designes par leur NOM : `resolver` les retrouve au
				// chargement (un nom non resolu laisse l'etat vide, et le dit).
				// LoadBinary relit AUSSI le .nkanim v3 du 29/09 (meme contenu, ancien
				// emballage) ; il refuse proprement un .nkanim v1/v2, qui est un clip.
				struct NkResolver {
						NkFunction<const NkAnimationClip *(const NkString &)> clip;
						NkFunction<NkBlendTree1D *(const NkString &)> tree1D;
						NkFunction<NkBlendTree2D *(const NkString &)> tree2D;
				};

				bool SaveBinary(const NkString &path) const;
				bool LoadBinary(const NkString &path, const NkResolver &resolver = NkResolver());
				void SaveToBytes(NkVector<nk_uint8> &out) const;
				bool LoadFromBytes(const nk_uint8 *data, usize size, const NkResolver &resolver = NkResolver());

			private:
				struct State {
						NkString name;
						const NkAnimationClip *clip = nullptr;
						NkBlendTree1D *tree = nullptr;	 // possede par l'appelant
						NkBlendTree2D *tree2d = nullptr; // possede par l'appelant
						float32 time = 0.f;				 // temps local (clips)
						// Hierarchie (2026-09-29).
						int32 parent = NK_ROOT;
						int32 depth = 0;
						int32 entry = -1; // sous-machine : etat d'entree explicite (-1 = premier enfant)
						int32 tag = 0;
						bool composite = false;
						uint8 refKind = 0; // 0 vide, 1 clip, 2 arbre 1D, 3 arbre 2D (sauvegarde)
						NkString ref;	   // nom du clip / de l'arbre, garde pour la sauvegarde
						// Disposition dans l'editeur (2026-10-01) : lue et ecrite, jamais interpretee.
						float32 edX = 0.f, edY = 0.f;
						bool edPlaced = false;
				};

				// Les pseudo-noeuds d'un niveau dans l'editeur (2026-10-01).
				struct Pseudo {
						int32 machine = NK_ROOT;
						float32 x[2] = {0.f, 0.f};
						float32 y[2] = {0.f, 0.f};
						bool placed[2] = {false, false};
				};

				struct Condition {
						NkString param;
						NkCondKind kind = NkCondKind::BOOL_TRUE;
						float32 threshold = 0.f;
				};

				struct Transition {
						int32 from = -1;
						int32 to = -1;
						bool any = false;		// « depuis n'importe ou » dans `scope`
						int32 scope = NK_ROOT;	// portee d'une any-state
						NkVector<Condition> conds;
						float32 fadeDur = 0.25f;
						int32 priority = 0;
				};

				struct Param {
						NkString name;
						NkParamKind kind = NkParamKind::FLOAT;
						float32 value = 0.f;
						float32 defaultValue = 0.f;
				};

				// Evalue l'etat : avance son horloge, remplit `out` (bones finaux +
				// morphs). Si l'etat expose une pose BONE-LOCALE (clip skeletalLocal
				// ou blend tree), `outLocal` la recoit et `outSkel` pointe le clip
				// squelette — sinon outLocal reste vide (fallback matriciel).
				void EvalState(int32 idx, float32 dt, NkAnimationState &out, NkVector<NkMat4f> &outLocal,
							   const NkAnimationClip *&outSkel);
				bool CondTrue(const Condition &c, int32 source) const;

				int32 AddStateImpl(int32 parent, const NkString &name, bool composite);
				bool ValidParent(int32 parent) const;
				int32 EntryOf(int32 machine) const;
				int32 ResolveLeaf(int32 state) const;
				// Remplit path[0..n-1] (niveau 0 -> feuille) ; rend n.
				int32 BuildPath(int32 leaf, int32 *path) const;
				// Note l'entree dans les niveaux ou `to` quitte le chemin de `from`.
				void MarkEntered(int32 from, int32 to);
				void EnsureStarted();
				int32 PickTransition() const;

				NkVector<Param> &Params();
				const NkVector<Param> &Params() const;
				Param *FindParamPtr(const NkString &name, NkParamKind kind);
				const Param *FindParamPtr(const NkString &name, NkParamKind kind) const;
				Param &DeclareParamRef(const NkString &name, NkParamKind kind);

				NkVector<State> mStates;
				NkVector<Transition> mTransitions;
				NkVector<Param> mParams;
				NkVector<Pseudo> mPseudos; // disposition des pseudo-noeuds (editeur)
				NkAnimStateMachine *mParamOwner = nullptr; // non nul : parametres partages (ShareParametersWith)
				int32 mRootEntry = -1;
				int32 mCurrent = -1;
				int32 mNext = -1;	  // etat cible pendant un fondu (-1 = aucun)
				float32 mFadeT = 0.f; // temps restant du fondu
				float32 mFadeDur = 0.f;
				float32 mClock = 0.f;
				float32 mEnteredAt[NK_MAX_DEPTH] = {};
				bool mStarted = false; // faux tant qu'aucun Update / ForceState n'a fixe l'etat
				NkAnimationState mState;
				NkAnimationState mNextState;
				NkVector<NkMat4f> mLocalA, mLocalB; // poses locales pour crossfade bone-local
				TransitionFn mTransitionCb;
		};

		// Blend TRS-NLerp de deux matrices BONE-LOCALES (decompose T/R/S, lerp
		// T et S, NLerp rotation chemin court). Partage par le player (interp
		// keyframes), NkBlendTree1D (blend inter-clips) et la state machine.
		NkMat4f NkBlendLocalTRS(const NkMat4f &A, const NkMat4f &B, float32 a);


	} // namespace anim
} // namespace nkentseu

#endif // __NKENTSEU_NKANIMATION_NKANIMATION_H__
