#pragma once
// =============================================================================
// NkAnimationSystem.h  — NKRenderer v4.0  (Tools/Animation/)
//
// Système d'animation de RENDU généraliste.
// "Animation" = toute valeur qui évolue dans le temps pour produire un résultat
// visuel. NkAnimationSystem ne se limite PAS aux bones : il anime TOUT ce qui
// peut être rendu.
//
// Catégories d'animation supportées :
//   ─ Skeletal / Bones  : skinning GPU, squelette, onion skinning
//   ─ Morph targets     : blend shapes FACS, visage, déformation mesh
//   ─ UV / Texture      : scrolling UV, flip-book sprite, atlas séquence
//   ─ Couleur / Matériau: interpolation albedo, emissive, metallic, roughness
//   ─ Transform objet   : position, rotation, échelle (sans ECS)
//   ─ Caméra            : dolly, zoom, shake, cut
//   ─ Lumière           : intensité, couleur, position, clignotement
//   ─ Post-process      : DOF, bloom, exposition, color grading animés
//   ─ Propriétés custom : float/vec2/vec3/vec4 nommés
//
// Architecture :
//   NkAnimationClip   — données de keyframes (read-only pendant la lecture)
//   NkAnimationPlayer — joue un clip, maintient le temps courant
//   NkAnimationState  — snapshot évalué à un instant t (résultats prêts à lire)
//   NkAnimationSystem — gestionnaire central, update + application au renderer
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Core/NkRendererConfig.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKContainers/Associative/NkHashMap.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace renderer {

		class NkRender3D;
		class NkMaterialInstance;
		class NkMeshSystem;
		struct NkPostConfig;
		struct NkGLTFMeshData;

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

				// ── Helpers ───────────────────────────────────────────────────────────
				void RecalcDuration();

				void AddBoneKey(uint32 boneIdx, float32 time, const NkMat4f &mat,
								NkInterpMode interp = NkInterpMode::NK_LINEAR);
				void ResizeBones(uint32 count);

				// ── Sérialisation BINAIRE .nkanim (M1) ────────────────────────────────
				// Format compact versionné (header NKAN + tracks d'os). Pas de JSON :
				// l'anim = beaucoup de keyframes (floats) -> binaire = compact + chargement
				// rapide sans parsing. Sert l'app NkAnima ET le moteur de jeu (rejouer un clip).
				bool SaveBinary(const NkString &path) const;
				bool LoadBinary(const NkString &path);

				// ── Import : bake une animation glTF en clip éditable (M1) ─────────────
				// Échantillonne la pose squelettique (EvaluateGLTFPose) à `fps` sur toute
				// la durée et stocke chaque frame en keyframes par os (matrices de
				// skinning) -> l'anim devient éditable + sauvegardable .nkanim, et
				// rejouable par NkAnimationPlayer. Engine-level (jeu + app NkAnima).
				bool BakeFromGLTF(const NkGLTFMeshData &data, int32 animIdx, float32 fps = 30.f);

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
		// (typiquement BakeFromGLTF du meme fichier, ex. Fox Survey/Walk/Run).
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
		// NkAnimStateMachine — machine a etats d'animation (idle -> walk -> jump).
		// Chaque etat = un clip OU un blend tree (1D/2D). Les transitions sont
		// declenchees par des parametres (bool / seuil float) et font un
		// crossfade sur `fadeDur` secondes — BONE-LOCAL (blend TRS par os puis
		// un seul FK, correct sur les rotations) quand les deux etats exposent
		// leur pose locale sur le meme squelette, sinon fallback matriciel
		// (fondus courts). Evenements de transition via SetTransitionCallback.
		// Update(dt) : evalue l'etat courant, teste les transitions, avance le
		// fondu. GetState() = pose finale a soumettre au renderer.
		// =========================================================================
		class NkAnimStateMachine {
			public:
				// Etats (retourne l'index de l'etat). Un seul des pointeurs.
				int32 AddState(const NkString &name, const NkAnimationClip *clip);
				int32 AddState(const NkString &name, NkBlendTree1D *tree);
				int32 AddState(const NkString &name, NkBlendTree2D *tree2d);

				// Transition from -> to declenchee quand :
				//   - param bool `paramName` == true (kind BOOL), ou
				//   - param float `paramName` >  threshold (kind FLOAT_GREATER), ou
				//   - param float `paramName` <  threshold (kind FLOAT_LESS).
				// from = -1 : depuis N'IMPORTE quel etat (any-state transition).
				enum class NkCondKind : uint8 { BOOL_TRUE, FLOAT_GREATER, FLOAT_LESS };
				void AddTransition(int32 from, int32 to, const NkString &paramName, NkCondKind kind,
								   float32 threshold = 0.f, float32 fadeDur = 0.25f);

				// Parametres pilotes par le gameplay.
				void SetBool(const NkString &name, bool v);
				void SetFloat(const NkString &name, float32 v);
				float32 GetFloat(const NkString &name) const;

				// Force un etat sans transition (init / teleport).
				void ForceState(int32 idx);

				int32 GetCurrentState() const {
					return mCurrent;
				}

				const NkString &GetCurrentStateName() const;

				void Update(float32 dt);

				const NkAnimationState &GetState() const {
					return mState;
				}

				// Evenements de transition : appele au DECLENCHEMENT (finished=false)
				// puis a la FIN du fondu (finished=true). Sert au gameplay (sons de
				// pas, verrous d'input pendant une action, etc.).
				using TransitionFn = NkFunction<void(const NkString &from, const NkString &to, bool finished)>;

				void SetTransitionCallback(TransitionFn fn) {
					mTransitionCb = fn;
				}

			private:
				struct State {
						NkString name;
						const NkAnimationClip *clip = nullptr;
						NkBlendTree1D *tree = nullptr;	 // possede par l'appelant
						NkBlendTree2D *tree2d = nullptr; // possede par l'appelant
						float32 time = 0.f;				 // temps local (clips)
				};

				struct Transition {
						int32 from = -1;
						int32 to = -1;
						NkString param;
						NkCondKind kind = NkCondKind::BOOL_TRUE;
						float32 threshold = 0.f;
						float32 fadeDur = 0.25f;
				};

				// Evalue l'etat : avance son horloge, remplit `out` (bones finaux +
				// morphs). Si l'etat expose une pose BONE-LOCALE (clip skeletalLocal
				// ou blend tree), `outLocal` la recoit et `outSkel` pointe le clip
				// squelette — sinon outLocal reste vide (fallback matriciel).
				void EvalState(int32 idx, float32 dt, NkAnimationState &out, NkVector<NkMat4f> &outLocal,
							   const NkAnimationClip *&outSkel);
				bool CondTrue(const Transition &tr) const;

				NkVector<State> mStates;
				NkVector<Transition> mTransitions;
				NkHashMap<NkString, bool> mBools;
				NkHashMap<NkString, float32> mFloats;
				int32 mCurrent = -1;
				int32 mNext = -1;	  // etat cible pendant un fondu (-1 = aucun)
				float32 mFadeT = 0.f; // temps restant du fondu
				float32 mFadeDur = 0.f;
				NkAnimationState mState;
				NkAnimationState mNextState;
				NkVector<NkMat4f> mLocalA, mLocalB; // poses locales pour crossfade bone-local
				TransitionFn mTransitionCb;
		};

		// Blend TRS-NLerp de deux matrices BONE-LOCALES (decompose T/R/S, lerp
		// T et S, NLerp rotation chemin court). Partage par le player (interp
		// keyframes), NkBlendTree1D (blend inter-clips) et la state machine.
		NkMat4f NkBlendLocalTRS(const NkMat4f &A, const NkMat4f &B, float32 a);

		// =========================================================================
		// NkAnimationSystem — gestionnaire central
		// =========================================================================
		class NkAnimationSystem {
			public:
				NkAnimationSystem() = default;
				~NkAnimationSystem();

				bool Init(NkIDevice *device, NkRender3D *r3d);
				void Shutdown();

				// ── Update ────────────────────────────────────────────────────────────
				void Update(float32 dt);

				// ── Clips ─────────────────────────────────────────────────────────────
				NkAnimationClip *CreateClip(const NkString &name);
				NkAnimationClip *FindClip(const NkString &name) const;
				void DestroyClip(NkAnimationClip *&clip);

				// ── Players ───────────────────────────────────────────────────────────
				NkAnimationPlayer *CreatePlayer(const NkString &name = "");
				NkAnimationPlayer *FindPlayer(const NkString &name) const;
				void DestroyPlayer(NkAnimationPlayer *&player);

				// ─────────────────────────────────────────────────────────────────────
				// Application du state au renderer
				// ─────────────────────────────────────────────────────────────────────

				// Skeletal — soumet un mesh skinné
				void ApplySkinnedMesh(NkMeshHandle mesh, NkMatInstHandle mat, const NkMat4f &baseWorld,
									  const NkAnimationPlayer &player);

				// Skeletal — onion skinning autour du frame courant
				void ApplyOnionSkin(NkMeshHandle mesh, NkMatInstHandle mat, const NkMat4f &baseWorld,
									const NkAnimationPlayer &player, const int32 *ghostOffsets, uint32 ghostCount,
									NkVec4f pastColor = {1.f, 0.3f, 0.3f, 0.5f},
									NkVec4f futureColor = {0.3f, 0.3f, 1.f, 0.5f});

				// Transform — calcule la matrice world depuis le state
				NkMat4f ApplyTransform(const NkAnimationPlayer &player, const NkMat4f &base = NkMat4f::Identity());

				// Matériau — écrit les paramètres animés dans une NkMaterialInstance
				void ApplyMaterial(NkMaterialInstance *inst, const NkAnimationPlayer &player);

				// Caméra — applique le state à une NkCamera3D
				void ApplyCamera(NkCamera3D &cam, const NkAnimationPlayer &player);

				// Lumière — applique le state à une NkLightDesc
				void ApplyLight(NkLightDesc &light, const NkAnimationPlayer &player);

				// Post-process — applique le state à un NkPostConfig
				void ApplyPostProcess(NkPostConfig &pp, const NkAnimationPlayer &player);

				// UV — retourne l'UV rect animé {u0,v0,u1,v1}
				NkVec4f GetAnimatedSpriteUV(const NkAnimationPlayer &player);

				// Squelette — dessine le squelette en debug
				void DrawSkeleton(const NkAnimationPlayer &player, const NkMat4f &baseWorld, const int32 *parentIdx,
								  NkVec4f boneColor = {1.f, 0.6f, 0.f, 1.f}, float32 boneSize = 0.05f);

				// Morph targets CPU/GPU
				NkMeshHandle ApplyMorphTargets(NkMeshHandle base, const NkMeshHandle *targets, const float32 *weights,
											   uint32 count, bool useGPU = false);

				uint32 GetPlayerCount() const {
					return (uint32)mPlayers.Size();
				}

			private:
				NkIDevice *mDevice = nullptr;
				NkRender3D *mR3D = nullptr;

				NkVector<NkAnimationClip *> mClips;
				NkVector<NkAnimationPlayer *> mPlayers;
				NkVector<NkMat4f> mTmpBones;
				NkPipelineHandle mMorphCompute;
		};

	} // namespace renderer
} // namespace nkentseu
