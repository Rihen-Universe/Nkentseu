// =============================================================================
// NKAnimation/NkAnimation.cpp — voir NkAnimation.h
// -----------------------------------------------------------------------------
// Corps deplace TEL QUEL depuis NKRenderer/Tools/Animation/NkAnimationSystem.cpp
// le 2026-08-14. Seuls l'espace de noms et les inclusions ont change : plus rien
// ici ne tire le renderer, NKRHI ni un chargeur de format.
// =============================================================================
#include "NKAnimation/NkAnimation.h"
#include "NKMemory/NkAllocator.h"
#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"
#include <cmath>
#include <cstring>
#include <cstdlib>

// Repris tel quel du fichier d origine (NkAnimationSystem.cpp l.16) : la coupe
// du 2026-08-14 l avait perdu avec le bloc d inclusions, et le build l a
// rattrape immediatement -- NK_PI n est defini NULLE PART ailleurs comme pi dans
// le depot (seulement en macro locale dans NkMeshSystem.cpp et NkRender2D.cpp).
#define NK_PI 3.14159265358979f

namespace nkentseu {
	namespace anim {


		// =========================================================================
		// NkAnimationTrack<T>::Lerp spécialisations
		// =========================================================================
		template <> float32 NkAnimationTrack<float32>::Lerp(const float32 &a, const float32 &b, float32 t) const {
			return a + (b - a) * t;
		}

		template <> NkVec2f NkAnimationTrack<NkVec2f>::Lerp(const NkVec2f &a, const NkVec2f &b, float32 t) const {
			return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
		}

		template <> NkVec3f NkAnimationTrack<NkVec3f>::Lerp(const NkVec3f &a, const NkVec3f &b, float32 t) const {
			return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
		}

		template <> NkVec4f NkAnimationTrack<NkVec4f>::Lerp(const NkVec4f &a, const NkVec4f &b, float32 t) const {
			// Slerp pour quaternions si besoin, lerp pour couleurs
			return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
		}

		template <> NkMat4f NkAnimationTrack<NkMat4f>::Lerp(const NkMat4f &a, const NkMat4f &b, float32 t) const {
			// Lerp matriciel composant-par-composant. NOTE : les boneTracks stockent
			// des MATRICES DE SKINNING bakées (global×inverseBind), pas des TRS d'os
			// locaux. Décomposer/SLERP une matrice de skinning (scale composite,
			// réflexions possibles) donne des poses FAUSSES -> on garde le lerp direct,
			// correct à haut fps (clés rapprochées). Le slerp TRS appartient au niveau
			// bone-LOCAL (cf RESTE M1 : stocker les tracks en TRS local + FK au sample).
			NkMat4f r;
			for (int i = 0; i < 4; i++)
				for (int j = 0; j < 4; j++)
					r[i][j] = a[i][j] + (b[i][j] - a[i][j]) * t;
			return r;
		}

		template <> int32 NkAnimationTrack<int32>::Lerp(const int32 &a, const int32 &b, float32 t) const {
			return (t < 0.5f) ? a : b; // step pour int (frame index)
		}

		// =========================================================================
		// NkAnimationClip helpers
		// =========================================================================
		void NkAnimationClip::RecalcDuration() {
			duration = 0.f;
			for (auto &t : boneTracks)
				if (!t.Empty())
					duration = fmaxf(duration, t.GetDuration());
			for (auto &t : morphTracks)
				if (!t.Empty())
					duration = fmaxf(duration, t.GetDuration());
			if (!uvOffset.Empty())
				duration = fmaxf(duration, uvOffset.GetDuration());
			if (!uvScale.Empty())
				duration = fmaxf(duration, uvScale.GetDuration());
			if (!spriteFrame.Empty())
				duration = fmaxf(duration, spriteFrame.GetDuration());
			if (!albedoColor.Empty())
				duration = fmaxf(duration, albedoColor.GetDuration());
			if (!emissiveColor.Empty())
				duration = fmaxf(duration, emissiveColor.GetDuration());
			if (!position.Empty())
				duration = fmaxf(duration, position.GetDuration());
			if (!rotation.Empty())
				duration = fmaxf(duration, rotation.GetDuration());
			if (!scale.Empty())
				duration = fmaxf(duration, scale.GetDuration());
			if (!cameraPosition.Empty())
				duration = fmaxf(duration, cameraPosition.GetDuration());
			if (!lightIntensity.Empty())
				duration = fmaxf(duration, lightIntensity.GetDuration());
			if (!ppExposure.Empty())
				duration = fmaxf(duration, ppExposure.GetDuration());
		}

		void NkAnimationClip::ResizeBones(uint32 count) {
			boneCount = count;
			while ((uint32)boneTracks.Size() < count)
				boneTracks.PushBack({});
		}

		void NkAnimationClip::AddBoneKey(uint32 boneIdx, float32 time, const NkMat4f &mat, NkInterpMode interp) {
			if (boneIdx >= (uint32)boneTracks.Size())
				ResizeBones(boneIdx + 1);
			boneTracks[boneIdx].AddKey(time, mat, interp);
			duration = fmaxf(duration, time);
		}

		// =====================================================================
		// Sérialisation BINAIRE .nkanim (M1) — format compact versionné
		// ---------------------------------------------------------------------
		// [magic 'NKAN'(u32)] [version(u32)] [nameLen(u32)+name] [duration(f32)]
		// [fps(f32)] [loop(u8)] [boneCount(u32)]
		//   par os : [nameLen(u32)+name] [enabled(u8)] [keyCount(u32)]
		//            par clé : [time(f32)] [mat(16*f32)] [interp(u8)]
		// (Section os uniquement en v1 — le header versionné permet d'ajouter
		//  morph/transform/material plus tard sans casser les fichiers.)
		// =====================================================================
		namespace {
			constexpr uint32 kNkAnimMagic = 0x4E414B4E; // 'NKAN' (little-endian)
			constexpr uint32 kNkAnimVersion = 2;		// v2 : + section squelette (mode local)

			struct ByteWriter {
					NkVector<nk_uint8> buf;

					void raw(const void *p, usize n) {
						const nk_uint8 *b = (const nk_uint8 *)p;
						for (usize i = 0; i < n; ++i)
							buf.PushBack(b[i]);
					}

					void u8(uint8 v) {
						raw(&v, 1);
					}

					void u32(uint32 v) {
						raw(&v, 4);
					}

					void i32(int32 v) {
						raw(&v, 4);
					}

					void f32(float32 v) {
						raw(&v, 4);
					}

					void str(const NkString &s) {
						uint32 n = (uint32)s.Size();
						u32(n);
						if (n)
							raw(s.CStr(), n);
					}

					void mat(const NkMat4f &m) {
						raw(m.data, 16 * sizeof(float32));
					}
			};

			struct ByteReader {
					const nk_uint8 *p;
					usize n, off = 0;
					bool ok = true;

					ByteReader(const nk_uint8 *d, usize len) : p(d), n(len) {
					}

					bool need(usize c) {
						if (off + c > n) {
							ok = false;
							return false;
						}
						return true;
					}

					uint8 u8() {
						if (!need(1))
							return 0;
						return p[off++];
					}

					uint32 u32() {
						if (!need(4))
							return 0;
						uint32 v;
						memcpy(&v, p + off, 4);
						off += 4;
						return v;
					}

					int32 i32() {
						if (!need(4))
							return 0;
						int32 v;
						memcpy(&v, p + off, 4);
						off += 4;
						return v;
					}

					float32 f32() {
						if (!need(4))
							return 0;
						float32 v;
						memcpy(&v, p + off, 4);
						off += 4;
						return v;
					}

					NkString str() {
						uint32 c = u32();
						NkString s;
						if (c && need(c)) {
							s = NkString((const char *)(p + off), c);
							off += c;
						}
						return s;
					}

					NkMat4f mat() {
						NkMat4f m;
						if (need(16 * sizeof(float32))) {
							memcpy(m.data, p + off, 16 * sizeof(float32));
							off += 16 * sizeof(float32);
						}
						return m;
					}
			};
		} // namespace

		bool NkAnimationClip::SaveBinary(const NkString &path) const {
			ByteWriter w;
			w.u32(kNkAnimMagic);
			w.u32(kNkAnimVersion);
			w.str(name);
			w.f32(duration);
			w.f32(fps);
			w.u8(loop ? 1 : 0);
			w.u32((uint32)boneTracks.Size());
			for (uint32 b = 0; b < (uint32)boneTracks.Size(); ++b) {
				const NkAnimationTrack<NkMat4f> &tr = boneTracks[b];
				w.str(tr.name);
				w.u8(tr.enabled ? 1 : 0);
				w.u32(tr.KeyCount());
				for (uint32 k = 0; k < tr.KeyCount(); ++k) {
					const NkKeyframe<NkMat4f> &kf = tr.GetKey(k);
					w.f32(kf.time);
					w.mat(kf.value);
					w.u8((uint8)kf.interp);
				}
			}
			// Section squelette (v2) : mode local + hiérarchie + inverseBind + topo.
			w.u8(skeletalLocal ? 1 : 0);
			w.u32((uint32)jointParent.Size());
			for (uint32 j = 0; j < (uint32)jointParent.Size(); ++j)
				w.i32(jointParent[j]);
			w.u32((uint32)jointInverseBind.Size());
			for (uint32 j = 0; j < (uint32)jointInverseBind.Size(); ++j)
				w.mat(jointInverseBind[j]);
			w.u32((uint32)jointTopo.Size());
			for (uint32 j = 0; j < (uint32)jointTopo.Size(); ++j)
				w.u32(jointTopo[j]);
			if (!NkFile::WriteAllBytes(path.CStr(), w.buf)) {
				logger.Errorf("[NkAnimClip] SaveBinary echec : %s\n", path.CStr());
				return false;
			}
			logger.Info("[NkAnimClip] Saved '{0}' : {1} os, {2} octets\n", path.CStr(), (uint32)boneTracks.Size(),
						(uint32)w.buf.Size());
			return true;
		}

		bool NkAnimationClip::LoadBinary(const NkString &path) {
			NkVector<nk_uint8> bytes = NkFile::ReadAllBytes(path.CStr());
			if (bytes.Empty()) {
				logger.Errorf("[NkAnimClip] LoadBinary vide/absent : %s\n", path.CStr());
				return false;
			}
			ByteReader r(bytes.Data(), bytes.Size());
			uint32 magic = r.u32(), ver = r.u32();
			if (magic != kNkAnimMagic) {
				logger.Errorf("[NkAnimClip] magic invalide (0x%08X) : %s\n", magic, path.CStr());
				return false;
			}
			if (ver < 1 || ver > kNkAnimVersion) {
				logger.Errorf("[NkAnimClip] version %u non supportee : %s\n", ver, path.CStr());
				return false;
			}
			name = r.str();
			duration = r.f32();
			fps = r.f32();
			loop = (r.u8() != 0);
			uint32 nb = r.u32();
			boneTracks.Clear();
			boneTracks.Resize(nb);
			boneCount = nb;
			for (uint32 b = 0; b < nb; ++b) {
				NkAnimationTrack<NkMat4f> &tr = boneTracks[b];
				tr.name = r.str();
				tr.enabled = (r.u8() != 0);
				uint32 nk = r.u32();
				for (uint32 k = 0; k < nk; ++k) {
					float32 t = r.f32();
					NkMat4f m = r.mat();
					uint8 interp = r.u8();
					tr.AddKey(t, m, (NkInterpMode)interp);
				}
			}
			// Section squelette (v2+).
			skeletalLocal = false;
			jointParent.Clear();
			jointInverseBind.Clear();
			jointTopo.Clear();
			if (ver >= 2) {
				skeletalLocal = (r.u8() != 0);
				uint32 np = r.u32();
				jointParent.Resize(np);
				for (uint32 j = 0; j < np; ++j)
					jointParent[j] = r.i32();
				uint32 ni = r.u32();
				jointInverseBind.Resize(ni);
				for (uint32 j = 0; j < ni; ++j)
					jointInverseBind[j] = r.mat();
				uint32 nt = r.u32();
				jointTopo.Resize(nt);
				for (uint32 j = 0; j < nt; ++j)
					jointTopo[j] = r.u32();
			}
			if (!r.ok) {
				logger.Errorf("[NkAnimClip] LoadBinary tronque : %s\n", path.CStr());
				return false;
			}
			logger.Info("[NkAnimClip] Loaded '{0}' : {1} os, dur={2}s fps={3}\n", path.CStr(), nb, duration, fps);
			return true;
		}


		void NkAnimationClip::ApplyFKSkinning(NkVector<NkMat4f> &boneMats) const {
			const uint32 n = (uint32)jointTopo.Size();
			if (n == 0)
				return;
			NkVector<NkMat4f> global;
			global.Resize(n > (uint32)boneMats.Size() ? n : (uint32)boneMats.Size());
			// FK : global[j] = global[parent] × local[j]  (boneMats = locaux en entrée)
			for (uint32 oi = 0; oi < (uint32)jointTopo.Size(); ++oi) {
				uint32 j = jointTopo[oi];
				int32 p = (j < (uint32)jointParent.Size()) ? jointParent[j] : -1;
				NkMat4f local = (j < (uint32)boneMats.Size()) ? boneMats[j] : NkMat4f::Identity();
				global[j] = (p >= 0) ? (global[(uint32)p] * local) : local;
			}
			// skinning[j] = global[j] × inverseBind[j]  (écrit en place)
			for (uint32 j = 0; j < (uint32)boneMats.Size(); ++j) {
				NkMat4f ib = (j < (uint32)jointInverseBind.Size()) ? jointInverseBind[j] : NkMat4f::Identity();
				boneMats[j] = global[j] * ib;
			}
		}

		// Échantillonne une track de matrices BONE-LOCAL avec interp TRS-slerp (correct
		// car les locaux sont des transforms rigides — décompose, lerp T/S, SLERP rotation).
		static NkMat4f SampleBoneLocalSlerp(const NkAnimationTrack<NkMat4f> &tr, float32 t) {
			uint32 n = tr.KeyCount();
			if (n == 0)
				return NkMat4f::Identity();
			if (n == 1 || t <= tr.GetKey(0).time)
				return tr.GetKey(0).value;
			if (t >= tr.GetKey(n - 1).time)
				return tr.GetKey(n - 1).value;
			uint32 hi = 1;
			while (hi < n && tr.GetKey(hi).time <= t)
				++hi;
			uint32 lo = hi - 1;
			const NkKeyframe<NkMat4f> &ka = tr.GetKey(lo);
			const NkKeyframe<NkMat4f> &kb = tr.GetKey(hi);
			float32 dt = kb.time - ka.time;
			float32 a = (dt > 1e-6f) ? (t - ka.time) / dt : 0.f;
			// Interp TRS-NLerp : décompose A/B, lerp translation/scale, NLERP la rotation
			// (lerp de quaternions + normalize, chemin court via le signe du dot). Correct
			// sur des transforms LOCAUX rigides + visuellement propre. (NB : NkQuat::SLerp
			// pur reste buggé sur sa branche trigonométrique pour les grosses rotations ;
			// NLerp est suffisant et stable à 30fps. Cf NkAnima ROADMAP.) Debug : lerp
			// matriciel direct via NK_ANIM_LERPMAT.
			static int lerpmat = -1;
			if (lerpmat < 0) {
				const char *v = getenv("NK_ANIM_LERPMAT");
				lerpmat = (v && v[0] && v[0] != '0') ? 1 : 0;
			}
			if (lerpmat) {
				NkMat4f r;
				for (int i = 0; i < 4; i++)
					for (int j = 0; j < 4; j++)
						r[i][j] = ka.value[i][j] + (kb.value[i][j] - ka.value[i][j]) * a;
				return r;
			}
			return NkBlendLocalTRS(ka.value, kb.value, a);
		}

		// Blend TRS-NLerp de deux matrices BONE-LOCALES (cf. header). Extrait de
		// SampleBoneLocalSlerp pour etre partage avec NkBlendTree1D / state machine.
		NkMat4f NkBlendLocalTRS(const NkMat4f &A, const NkMat4f &B, float32 a) {
			if (a <= 0.f)
				return A;
			if (a >= 1.f)
				return B;
			NkVec3f ta, sa, tb, sb;
			NkMat4f ra, rb;
			A.DecomposeTRS(ta, ra, sa);
			B.DecomposeTRS(tb, rb, sb);
			NkQuatf qa(ra), qb(rb);
			NkVec3f tt = {ta.x + (tb.x - ta.x) * a, ta.y + (tb.y - ta.y) * a, ta.z + (tb.z - ta.z) * a};
			NkVec3f ss = {sa.x + (sb.x - sa.x) * a, sa.y + (sb.y - sa.y) * a, sa.z + (sb.z - sa.z) * a};
			float32 d = qa.x * qb.x + qa.y * qb.y + qa.z * qb.z + qa.w * qb.w;
			NkQuatf qbb = (d < 0.f) ? NkQuatf(-qb.x, -qb.y, -qb.z, -qb.w) : qb;
			NkQuatf qq(qa.x + (qbb.x - qa.x) * a, qa.y + (qbb.y - qa.y) * a, qa.z + (qbb.z - qa.z) * a,
					   qa.w + (qbb.w - qa.w) * a);
			qq = qq.Normalized();
			return NkMat4f::Translate(tt) * qq.ToMat4() * NkMat4f::Scale(ss);
		}

		void NkAnimationClip::BuildSpriteFlipBook(uint32 frameCount, float32 spriteFPS) {
			float32 frameDur = 1.f / spriteFPS;
			for (uint32 i = 0; i < frameCount; i++)
				spriteFrame.AddKey(i * frameDur, (int32)i, NkInterpMode::NK_STEP);
			RecalcDuration();
		}

		// ── Clips prédéfinis ──────────────────────────────────────────────────────
		NkAnimationClip *NkAnimationClip::MakeSpinClip(const NkString &name, float32 rpm, NkVec3f axis) {
			auto *c = memory::NkGetDefaultAllocator().New<NkAnimationClip>();
			c->name = name;
			c->duration = 60.f / fmaxf(rpm, 0.001f);
			// Rotation complète : deux keyframes (0 → 360)
			c->rotation.AddKey(0.f, {0, 0, 0, 1}, NkInterpMode::NK_LINEAR);
			c->rotation.AddKey(c->duration, {0, sinf(NK_PI) * axis.y, 0, cosf(NK_PI) * axis.y},
							   NkInterpMode::NK_LINEAR);
			return c;
		}

		NkAnimationClip *NkAnimationClip::MakeLightPulse(const NkString &name, float32 minI, float32 maxI,
														 float32 freq) {
			auto *c = memory::NkGetDefaultAllocator().New<NkAnimationClip>();
			c->name = name;
			float32 per = 1.f / fmaxf(freq, 0.001f);
			c->lightIntensity.AddKey(0.f, minI, NkInterpMode::NK_EASE_IN_OUT);
			c->lightIntensity.AddKey(per * 0.5f, maxI, NkInterpMode::NK_EASE_IN_OUT);
			c->lightIntensity.AddKey(per, minI, NkInterpMode::NK_EASE_IN_OUT);
			c->duration = per;
			c->loop = true;
			return c;
		}

		NkAnimationClip *NkAnimationClip::MakeColorFade(const NkString &name, NkVec4f from, NkVec4f to, float32 dur,
														NkInterpMode interp) {
			auto *c = memory::NkGetDefaultAllocator().New<NkAnimationClip>();
			c->name = name;
			c->duration = dur;
			c->albedoColor.AddKey(0.f, from, interp);
			c->albedoColor.AddKey(dur, to, interp);
			return c;
		}

		NkAnimationClip *NkAnimationClip::MakeCameraShake(const NkString &name, float32 intensity, float32 dur,
														  float32 freq) {
			auto *c = memory::NkGetDefaultAllocator().New<NkAnimationClip>();
			c->name = name;
			c->duration = dur;
			uint32 steps = (uint32)(dur * freq);
			for (uint32 i = 0; i <= steps; i++) {
				float32 t = (float32)i / steps * dur;
				float32 env = 1.f - t / dur; // enveloppe d'atténuation
				float32 ox = (((i * 12345 + 7) % 100) / 100.f * 2.f - 1.f) * intensity * env;
				float32 oy = (((i * 23456 + 3) % 100) / 100.f * 2.f - 1.f) * intensity * env;
				c->cameraPosition.AddKey(t, {ox, oy, 0}, NkInterpMode::NK_LINEAR);
			}
			return c;
		}

		NkAnimationClip *NkAnimationClip::MakeProceduralWalk(uint32 boneCount, float32 dur) {
			auto *c = memory::NkGetDefaultAllocator().New<NkAnimationClip>();
			c->name = "ProceduralWalk";
			c->duration = dur;
			c->ResizeBones(boneCount);
			// Animer chaque bone avec une sinusoïde décalée
			for (uint32 b = 0; b < boneCount; b++) {
				float32 phase = (float32)b / (boneCount > 1 ? boneCount - 1 : 1) * NK_PI;
				uint32 steps = 16;
				for (uint32 s = 0; s <= steps; s++) {
					float32 t = (float32)s / steps * dur;
					float32 a = sinf(2 * NK_PI * t / dur + phase) * 0.3f;
					// Rotation Y simple
					NkMat4f mat;
					float32 ca = cosf(a), sa = sinf(a);
					mat = NkMat4f::Identity();
					mat[0][0] = ca;
					mat[2][0] = -sa;
					mat[0][2] = sa;
					mat[2][2] = ca;
					mat[3][1] = (float32)b * 0.15f;
					c->boneTracks[b].AddKey(t, mat, NkInterpMode::NK_LINEAR);
				}
			}
			return c;
		}

		// =========================================================================
		// NkAnimationPlayer
		// =========================================================================
		void NkAnimationPlayer::SetClip(const NkAnimationClip *clip, bool autoResize) {
			mClip = clip;
			mTime = 0.f;
			if (clip && autoResize) {
				mState.boneMatrices.Resize(clip->boneCount);
				for (auto &b : mState.boneMatrices)
					b = NkMat4f::Identity();
				mState.morphWeights.Resize((uint32)clip->morphTracks.Size(), 0.f);
			}
		}

		void NkAnimationPlayer::Play(NkPlayMode mode, float32 speed) {
			mMode = mode;
			mSpeed = speed;
			mPlaying = true;
			for (auto &mk : mMarkers)
				mk.fired = false;
		}

		void NkAnimationPlayer::Pause() {
			mPlaying = false;
		}

		void NkAnimationPlayer::Stop() {
			mPlaying = false;
			mTime = 0.f;
		}

		void NkAnimationPlayer::SeekTo(float32 t) {
			mTime = t;
		}

		float32 NkAnimationPlayer::GetNormTime() const {
			if (!mClip || mClip->duration <= 0.f)
				return 0.f;
			return mTime / mClip->duration;
		}

		int32 NkAnimationPlayer::GetFrame() const {
			if (!mClip)
				return 0;
			return (int32)(mTime * mClip->fps);
		}

		void NkAnimationPlayer::AddMarker(float32 t, const NkString &ev) {
			Marker mk;
			mk.time = t;
			mk.name = ev;
			mMarkers.PushBack(mk);
		}

		void NkAnimationPlayer::BlendTo(const NkAnimationClip *next, float32 blendDur) {
			mNextClip = next;
			mBlendT = blendDur;
			mBlendDur = blendDur;
		}

		void NkAnimationPlayer::WrapTime(float32 &t, float32 dur) {
			if (dur <= 0.f) {
				t = 0.f;
				return;
			}
			switch (mMode) {
				case NkPlayMode::NK_LOOP:
					while (t >= dur)
						t -= dur;
					while (t < 0.f)
						t += dur;
					break;
				case NkPlayMode::NK_PING_PONG: {
					float32 cycle = 2.f * dur;
					while (t >= cycle)
						t -= cycle;
					if (t > dur)
						t = cycle - t;
					break;
				}
				case NkPlayMode::NK_CLAMP:
					if (t > dur)
						t = dur;
					break;
				case NkPlayMode::NK_ONCE:
					if (t >= dur) {
						t = dur;
						mPlaying = false;
					}
					break;
			}
		}

		void NkAnimationPlayer::Update(float32 dt) {
			if (!mClip)
				return;

			if (mPlaying) {
				mTime += dt * mSpeed;
				WrapTime(mTime, mClip->duration);
			}

			// Markers
			for (auto &mk : mMarkers) {
				if (!mk.fired && mTime >= mk.time) {
					mk.fired = true;
					if (mMarkerCb)
						mMarkerCb(mk.name);
				}
			}

			// Blending
			if (mBlendT > 0.f && mNextClip) {
				mBlendT = fmaxf(0.f, mBlendT - dt);
				float32 w = mBlendT / fmaxf(mBlendDur, 0.001f);
				Evaluate(mClip, mTime, mState);
				Evaluate(mNextClip, mTime, mBlendState);
				BlendStates(mState, mBlendState, w);
				if (mBlendT <= 0.f) {
					mClip = mNextClip;
					mNextClip = nullptr;
				}
			} else {
				Evaluate(mClip, mTime, mState);
			}

			ComputeSpriteUV(mState, mClip);
			ComputeTransformMatrix(mState);
		}

		void NkAnimationPlayer::Evaluate(const NkAnimationClip *clip, float32 t, NkAnimationState &s) {
			if (!clip)
				return;
			// Bones
			s.boneMatrices.Resize(clip->boneCount);
			if (clip->skeletalLocal && !clip->jointTopo.Empty()) {
				// Mode LOCAL : sample bone-local (slerp) -> FK -> skinning.
				for (uint32 i = 0; i < clip->boneCount; i++)
					s.boneMatrices[i] = (i < (uint32)clip->boneTracks.Size() && !clip->boneTracks[i].Empty())
											? SampleBoneLocalSlerp(clip->boneTracks[i], t)
											: NkMat4f::Identity();
				clip->ApplyFKSkinning(s.boneMatrices);
			} else {
				// Mode legacy : boneTracks = matrices de skinning directes.
				for (uint32 i = 0; i < clip->boneCount; i++)
					s.boneMatrices[i] = (i < (uint32)clip->boneTracks.Size() && !clip->boneTracks[i].Empty())
											? clip->boneTracks[i].Evaluate(t)
											: NkMat4f::Identity();
			}
			// Morph
			s.morphWeights.Resize((uint32)clip->morphTracks.Size(), 0.f);
			for (uint32 i = 0; i < (uint32)clip->morphTracks.Size(); i++)
				s.morphWeights[i] = clip->morphTracks[i].Evaluate(t);
			// UV
			if (!clip->uvOffset.Empty())
				s.uvOffset = clip->uvOffset.Evaluate(t);
			if (!clip->uvScale.Empty())
				s.uvScale = clip->uvScale.Evaluate(t);
			if (!clip->uvRotation.Empty())
				s.uvRotation = clip->uvRotation.Evaluate(t);
			if (!clip->spriteFrame.Empty())
				s.spriteFrame = clip->spriteFrame.Evaluate(t);
			// Matériau
			if (!clip->albedoColor.Empty())
				s.albedo = clip->albedoColor.Evaluate(t);
			if (!clip->emissiveColor.Empty()) {
				auto c = clip->emissiveColor.Evaluate(t);
				s.emissive = {c.x, c.y, c.z};
			}
			if (!clip->emissiveStrength.Empty())
				s.emissiveStrength = clip->emissiveStrength.Evaluate(t);
			if (!clip->metallic.Empty())
				s.metallic = clip->metallic.Evaluate(t);
			if (!clip->roughness.Empty())
				s.roughness = clip->roughness.Evaluate(t);
			if (!clip->opacity.Empty())
				s.opacity = clip->opacity.Evaluate(t);
			// Transform
			if (!clip->position.Empty())
				s.position = clip->position.Evaluate(t);
			if (!clip->rotation.Empty())
				s.rotation = clip->rotation.Evaluate(t);
			if (!clip->scale.Empty())
				s.scale = clip->scale.Evaluate(t);
			// Caméra
			if (!clip->cameraPosition.Empty())
				s.camPos = clip->cameraPosition.Evaluate(t);
			if (!clip->cameraTarget.Empty())
				s.camTarget = clip->cameraTarget.Evaluate(t);
			if (!clip->cameraFOV.Empty())
				s.camFOV = clip->cameraFOV.Evaluate(t);
			if (!clip->cameraDOFFocus.Empty())
				s.camDOFFocus = clip->cameraDOFFocus.Evaluate(t);
			if (!clip->cameraDOFAperture.Empty())
				s.camDOFApt = clip->cameraDOFAperture.Evaluate(t);
			// Lumière
			if (!clip->lightIntensity.Empty()) {
				s.lightIntensity = clip->lightIntensity.Evaluate(t);
			}
			if (!clip->lightColor.Empty()) {
				s.lightColor = clip->lightColor.Evaluate(t);
			}
			if (!clip->lightPosition.Empty()) {
				s.lightPos = clip->lightPosition.Evaluate(t);
			}
			if (!clip->lightRange.Empty()) {
				s.lightRange = clip->lightRange.Evaluate(t);
			}
			// Post-process
			if (!clip->ppExposure.Empty())
				s.ppExposure = clip->ppExposure.Evaluate(t);
			if (!clip->ppSaturation.Empty())
				s.ppSaturation = clip->ppSaturation.Evaluate(t);
			if (!clip->ppContrast.Empty())
				s.ppContrast = clip->ppContrast.Evaluate(t);
			if (!clip->ppBloomStrength.Empty())
				s.ppBloom = clip->ppBloomStrength.Evaluate(t);
			if (!clip->ppDOFFocus.Empty())
				s.ppDOFFocus = clip->ppDOFFocus.Evaluate(t);
			if (!clip->ppVignetteIntensity.Empty())
				s.ppVignette = clip->ppVignetteIntensity.Evaluate(t);
		}

		void NkAnimationPlayer::BlendStates(NkAnimationState &a, const NkAnimationState &b, float32 w) {
			// w=1 → a, w=0 → b
			float32 iw = 1.f - w;
			for (uint32 i = 0; i < (uint32)a.boneMatrices.Size() && i < (uint32)b.boneMatrices.Size(); i++) {
				for (int r = 0; r < 4; r++)
					for (int c = 0; c < 4; c++)
						a.boneMatrices[i][r][c] = a.boneMatrices[i][r][c] * w + b.boneMatrices[i][r][c] * iw;
			}
			auto lerpf = [](float32 x, float32 y, float32 t) { return x + (y - x) * t; };
			auto lerpv3 = [](NkVec3f x, NkVec3f y, float32 t) -> NkVec3f {
				return {x.x + (y.x - x.x) * t, x.y + (y.y - x.y) * t, x.z + (y.z - x.z) * t};
			};
			a.albedo.x = lerpf(a.albedo.x, b.albedo.x, iw);
			a.albedo.y = lerpf(a.albedo.y, b.albedo.y, iw);
			a.albedo.z = lerpf(a.albedo.z, b.albedo.z, iw);
			a.albedo.w = lerpf(a.albedo.w, b.albedo.w, iw);
			a.metallic = lerpf(a.metallic, b.metallic, iw);
			a.roughness = lerpf(a.roughness, b.roughness, iw);
			a.opacity = lerpf(a.opacity, b.opacity, iw);
			a.position = lerpv3(a.position, b.position, iw);
			a.lightIntensity = lerpf(a.lightIntensity, b.lightIntensity, iw);
			a.ppExposure = lerpf(a.ppExposure, b.ppExposure, iw);
		}

		void NkAnimationPlayer::ComputeSpriteUV(NkAnimationState &s, const NkAnimationClip *clip) {
			if (!clip || clip->spriteAtlasCols < 1 || clip->spriteAtlasRows < 1)
				return;
			uint32 f = (uint32)fmaxf(0.f, (float32)s.spriteFrame);
			uint32 total = clip->spriteAtlasCols * clip->spriteAtlasRows;
			f %= math::NkMax(1u, total);
			uint32 col = f % clip->spriteAtlasCols;
			uint32 row = f / clip->spriteAtlasCols;
			float32 cw = 1.f / clip->spriteAtlasCols;
			float32 rh = 1.f / clip->spriteAtlasRows;
			s.spriteUV = {col * cw, row * rh, (col + 1) * cw, (row + 1) * rh};
		}

		void NkAnimationPlayer::ComputeTransformMatrix(NkAnimationState &s) {
			// TRS : Translation * Rotation(quat) * Scale
			// Quaternion → matrice
			float32 qx = s.rotation.x, qy = s.rotation.y, qz = s.rotation.z, qw = s.rotation.w;
			float32 x2 = qx * qx, y2 = qy * qy, z2 = qz * qz;
			NkMat4f rot = NkMat4f::Identity();
			rot[0][0] = 1 - 2 * (y2 + z2);
			rot[1][0] = 2 * (qx * qy - qz * qw);
			rot[2][0] = 2 * (qx * qz + qy * qw);
			rot[0][1] = 2 * (qx * qy + qz * qw);
			rot[1][1] = 1 - 2 * (x2 + z2);
			rot[2][1] = 2 * (qy * qz - qx * qw);
			rot[0][2] = 2 * (qx * qz - qy * qw);
			rot[1][2] = 2 * (qy * qz + qx * qw);
			rot[2][2] = 1 - 2 * (x2 + y2);
			NkMat4f trs = NkMat4f::Translate(s.position) * rot * NkMat4f::Scale(s.scale);
			s.transform = trs;
		}

		// =========================================================================
		// NkBlendTree1D
		// =========================================================================

		// Evaluation SQUELETTE (+morphs) d'un clip — sous-ensemble du
		// NkAnimationPlayer::Evaluate (les tracks materiau/camera ne sont pas
		// concernees par le blend d'animations de personnage).
		static void EvalSkeletalLocal(const NkAnimationClip *clip, float32 t, NkVector<NkMat4f> &outLocal) {
			outLocal.Resize(clip->boneCount);
			for (uint32 i = 0; i < clip->boneCount; i++)
				outLocal[i] = (i < (uint32)clip->boneTracks.Size() && !clip->boneTracks[i].Empty())
								  ? SampleBoneLocalSlerp(clip->boneTracks[i], t)
								  : NkMat4f::Identity();
		}

		void NkBlendTree1D::AddClip(const NkAnimationClip *clip, float32 pos) {
			if (!clip)
				return;
			Entry e;
			e.clip = clip;
			e.pos = pos;
			// Insertion triee par pos croissante.
			usize at = 0;
			while (at < mEntries.Size() && mEntries[at].pos < pos)
				++at;
			mEntries.Insert(mEntries.Begin() + at, e);
		}

		void NkBlendTree1D::SetParameter(float32 x) {
			if (mEntries.Empty()) {
				mParam = x;
				return;
			}
			const float32 lo = mEntries[0].pos;
			const float32 hi = mEntries[mEntries.Size() - 1].pos;
			mParam = x < lo ? lo : (x > hi ? hi : x);
		}

		void NkBlendTree1D::Update(float32 dt) {
			if (mEntries.Empty())
				return;
			const uint32 n = (uint32)mEntries.Size();

			// Voisins autour de mParam + poids de blend.
			uint32 hi = 0;
			while (hi < n && mEntries[hi].pos <= mParam)
				++hi;
			uint32 lo = (hi > 0) ? hi - 1 : 0;
			if (hi >= n)
				hi = n - 1;
			float32 w = 0.f;
			if (hi != lo) {
				const float32 span = mEntries[hi].pos - mEntries[lo].pos;
				w = (span > 1e-6f) ? (mParam - mEntries[lo].pos) / span : 0.f;
			}
			const NkAnimationClip *A = mEntries[lo].clip;
			const NkAnimationClip *B = mEntries[hi].clip;

			// Duree INTERPOLEE -> avance du temps NORMALISE (phases synchro :
			// les deux clips sont echantillonnes a la meme phase 0..1).
			const float32 durA = A->duration > 1e-4f ? A->duration : 1.f;
			const float32 durB = B->duration > 1e-4f ? B->duration : 1.f;
			const float32 dur = durA + (durB - durA) * w;
			mNormTime += dt * mSpeed / dur;
			mNormTime -= floorf(mNormTime);

			// Sample bone-LOCAL des deux clips puis blend par os AVANT le FK.
			if (A->skeletalLocal && !A->jointTopo.Empty()) {
				EvalSkeletalLocal(A, mNormTime * durA, mState.boneMatrices);
				if (w > 1e-4f && B != A) {
					EvalSkeletalLocal(B, mNormTime * durB, mScratch);
					const uint32 bc = (uint32)mState.boneMatrices.Size();
					for (uint32 i = 0; i < bc && i < (uint32)mScratch.Size(); i++)
						mState.boneMatrices[i] = NkBlendLocalTRS(mState.boneMatrices[i], mScratch[i], w);
				}
				mLocalPose = mState.boneMatrices; // pose locale pre-FK (crossfade SM)
				A->ApplyFKSkinning(mState.boneMatrices);
			} else {
				mLocalPose.Clear();
				// Mode legacy (matrices de skinning directes) : lerp matriciel,
				// approximation acceptable pour de petits ecarts de pose.
				mState.boneMatrices.Resize(A->boneCount);
				for (uint32 i = 0; i < A->boneCount; i++) {
					NkMat4f ma = (i < (uint32)A->boneTracks.Size() && !A->boneTracks[i].Empty())
									 ? A->boneTracks[i].Evaluate(mNormTime * durA)
									 : NkMat4f::Identity();
					if (w > 1e-4f && B != A && i < (uint32)B->boneTracks.Size() && !B->boneTracks[i].Empty()) {
						NkMat4f mb = B->boneTracks[i].Evaluate(mNormTime * durB);
						for (int r = 0; r < 4; r++)
							for (int c = 0; c < 4; c++)
								ma[r][c] = ma[r][c] + (mb[r][c] - ma[r][c]) * w;
					}
					mState.boneMatrices[i] = ma;
				}
			}

			// Morph weights : blend lineaire si les deux clips en ont.
			const uint32 nmA = (uint32)A->morphTracks.Size();
			const uint32 nmB = (uint32)B->morphTracks.Size();
			const uint32 nm = nmA > nmB ? nmA : nmB;
			mState.morphWeights.Resize(nm, 0.f);
			for (uint32 i = 0; i < nm; i++) {
				const float32 wa = (i < nmA) ? A->morphTracks[i].Evaluate(mNormTime * durA) : 0.f;
				const float32 wb = (i < nmB) ? B->morphTracks[i].Evaluate(mNormTime * durB) : 0.f;
				mState.morphWeights[i] = wa + (wb - wa) * w;
			}
		}

		// =========================================================================
		// NkBlendTree2D
		// =========================================================================
		void NkBlendTree2D::AddClip(const NkAnimationClip *clip, NkVec2f pos) {
			if (!clip)
				return;
			Entry e;
			e.clip = clip;
			e.pos = pos;
			mEntries.PushBack(e);
		}

		void NkBlendTree2D::SetParameter(NkVec2f p) {
			mParam = p;
		}

		void NkBlendTree2D::Update(float32 dt) {
			if (mEntries.Empty())
				return;
			const uint32 n = (uint32)mEntries.Size();

			// ── Poids inverse-distance (Shepard, p=2), hit exact = clip pur ──────
			float32 wsum = 0.f;
			int32 exact = -1;
			for (uint32 i = 0; i < n; i++) {
				const float32 dx = mEntries[i].pos.x - mParam.x;
				const float32 dy = mEntries[i].pos.y - mParam.y;
				const float32 d2 = dx * dx + dy * dy;
				if (d2 < 1e-8f) {
					exact = (int32)i;
					break;
				}
				mEntries[i].weight = 1.f / d2;
				wsum += mEntries[i].weight;
			}
			if (exact >= 0) {
				for (uint32 i = 0; i < n; i++)
					mEntries[i].weight = (i == (uint32)exact) ? 1.f : 0.f;
			} else if (wsum > 1e-12f) {
				for (uint32 i = 0; i < n; i++)
					mEntries[i].weight /= wsum;
			}

			// ── Duree ponderee -> temps normalise (phases synchro) ──────────────
			float32 dur = 0.f;
			for (uint32 i = 0; i < n; i++) {
				const float32 di = mEntries[i].clip->duration > 1e-4f ? mEntries[i].clip->duration : 1.f;
				dur += mEntries[i].weight * di;
			}
			if (dur < 1e-4f)
				dur = 1.f;
			mNormTime += dt * mSpeed / dur;
			mNormTime -= floorf(mNormTime);

			// ── Blend bone-LOCAL CUMULATIF : result = melange progressif ────────
			// resultat := blend(resultat, pose_i, w_i / accW) — equivalent au
			// barycentre des N poses sans etendre NkBlendLocalTRS a N entrees.
			const NkAnimationClip *skel = mEntries[0].clip;
			if (skel->skeletalLocal && !skel->jointTopo.Empty()) {
				float32 accW = 0.f;
				bool first = true;
				for (uint32 i = 0; i < n; i++) {
					const float32 w = mEntries[i].weight;
					if (w < 1e-4f)
						continue;
					const NkAnimationClip *c = mEntries[i].clip;
					const float32 dc = c->duration > 1e-4f ? c->duration : 1.f;
					if (first) {
						EvalSkeletalLocal(c, mNormTime * dc, mLocalPose);
						accW = w;
						first = false;
						continue;
					}
					EvalSkeletalLocal(c, mNormTime * dc, mScratch);
					const float32 a = w / (accW + w);
					const uint32 bc = (uint32)mLocalPose.Size();
					for (uint32 b = 0; b < bc && b < (uint32)mScratch.Size(); b++)
						mLocalPose[b] = NkBlendLocalTRS(mLocalPose[b], mScratch[b], a);
					accW += w;
				}
				if (first)
					EvalSkeletalLocal(skel, mNormTime * skel->duration, mLocalPose);
				mState.boneMatrices = mLocalPose;
				skel->ApplyFKSkinning(mState.boneMatrices);
			} else {
				mLocalPose.Clear();
			}

			// ── Morph weights : moyenne ponderee ─────────────────────────────────
			uint32 nm = 0;
			for (uint32 i = 0; i < n; i++)
				if ((uint32)mEntries[i].clip->morphTracks.Size() > nm)
					nm = (uint32)mEntries[i].clip->morphTracks.Size();
			mState.morphWeights.Resize(nm, 0.f);
			for (uint32 m = 0; m < nm; m++) {
				float32 acc = 0.f;
				for (uint32 i = 0; i < n; i++) {
					const NkAnimationClip *c = mEntries[i].clip;
					if (m < (uint32)c->morphTracks.Size()) {
						const float32 dc = c->duration > 1e-4f ? c->duration : 1.f;
						acc += mEntries[i].weight * c->morphTracks[m].Evaluate(mNormTime * dc);
					}
				}
				mState.morphWeights[m] = acc;
			}
		}

		// =========================================================================
		// NkAnimStateMachine
		// =========================================================================
		int32 NkAnimStateMachine::AddState(const NkString &name, const NkAnimationClip *clip) {
			State s;
			s.name = name;
			s.clip = clip;
			mStates.PushBack(s);
			if (mCurrent < 0)
				mCurrent = (int32)mStates.Size() - 1;
			return (int32)mStates.Size() - 1;
		}

		int32 NkAnimStateMachine::AddState(const NkString &name, NkBlendTree1D *tree) {
			State s;
			s.name = name;
			s.tree = tree;
			mStates.PushBack(s);
			if (mCurrent < 0)
				mCurrent = (int32)mStates.Size() - 1;
			return (int32)mStates.Size() - 1;
		}

		int32 NkAnimStateMachine::AddState(const NkString &name, NkBlendTree2D *tree2d) {
			State s;
			s.name = name;
			s.tree2d = tree2d;
			mStates.PushBack(s);
			if (mCurrent < 0)
				mCurrent = (int32)mStates.Size() - 1;
			return (int32)mStates.Size() - 1;
		}

		void NkAnimStateMachine::AddTransition(int32 from, int32 to, const NkString &paramName, NkCondKind kind,
											   float32 threshold, float32 fadeDur) {
			Transition tr;
			tr.from = from;
			tr.to = to;
			tr.param = paramName;
			tr.kind = kind;
			tr.threshold = threshold;
			tr.fadeDur = fadeDur;
			mTransitions.PushBack(tr);
		}

		void NkAnimStateMachine::SetBool(const NkString &name, bool v) {
			mBools[name] = v;
		}

		void NkAnimStateMachine::SetFloat(const NkString &name, float32 v) {
			mFloats[name] = v;
		}

		float32 NkAnimStateMachine::GetFloat(const NkString &name) const {
			const float32 *p = mFloats.Find(name);
			return p ? *p : 0.f;
		}

		void NkAnimStateMachine::ForceState(int32 idx) {
			if (idx >= 0 && idx < (int32)mStates.Size()) {
				mCurrent = idx;
				mNext = -1;
				mFadeT = 0.f;
			}
		}

		const NkString &NkAnimStateMachine::GetCurrentStateName() const {
			static NkString sEmpty;
			return (mCurrent >= 0 && mCurrent < (int32)mStates.Size()) ? mStates[(uint32)mCurrent].name : sEmpty;
		}

		bool NkAnimStateMachine::CondTrue(const Transition &tr) const {
			if (tr.kind == NkCondKind::BOOL_TRUE) {
				const bool *b = mBools.Find(tr.param);
				return b && *b;
			}
			const float32 *f = mFloats.Find(tr.param);
			const float32 v = f ? *f : 0.f;
			return (tr.kind == NkCondKind::FLOAT_GREATER) ? (v > tr.threshold) : (v < tr.threshold);
		}

		void NkAnimStateMachine::EvalState(int32 idx, float32 dt, NkAnimationState &out, NkVector<NkMat4f> &outLocal,
										   const NkAnimationClip *&outSkel) {
			outLocal.Clear();
			outSkel = nullptr;
			State &st = mStates[(uint32)idx];
			if (st.tree) {
				st.tree->Update(dt);
				out = st.tree->GetState();
				outLocal = st.tree->GetLocalPose();
				outSkel = st.tree->GetSkeletonClip();
				return;
			}
			if (st.tree2d) {
				st.tree2d->Update(dt);
				out = st.tree2d->GetState();
				outLocal = st.tree2d->GetLocalPose();
				outSkel = st.tree2d->GetSkeletonClip();
				return;
			}
			if (!st.clip)
				return;
			st.time += dt;
			const float32 dur = st.clip->duration > 1e-4f ? st.clip->duration : 1.f;
			st.time -= floorf(st.time / dur) * dur;
			// Pose squelette (+ morphs) — les tracks materiau/camera ne sont pas
			// evaluees par la state machine v1.
			out.boneMatrices.Resize(st.clip->boneCount);
			if (st.clip->skeletalLocal && !st.clip->jointTopo.Empty()) {
				EvalSkeletalLocal(st.clip, st.time, outLocal);
				outSkel = st.clip;
				out.boneMatrices = outLocal;
				st.clip->ApplyFKSkinning(out.boneMatrices);
			} else {
				for (uint32 i = 0; i < st.clip->boneCount; i++)
					out.boneMatrices[i] = (i < (uint32)st.clip->boneTracks.Size() && !st.clip->boneTracks[i].Empty())
											  ? st.clip->boneTracks[i].Evaluate(st.time)
											  : NkMat4f::Identity();
			}
			out.morphWeights.Resize((uint32)st.clip->morphTracks.Size(), 0.f);
			for (uint32 i = 0; i < (uint32)st.clip->morphTracks.Size(); i++)
				out.morphWeights[i] = st.clip->morphTracks[i].Evaluate(st.time);
		}

		void NkAnimStateMachine::Update(float32 dt) {
			if (mCurrent < 0 || mCurrent >= (int32)mStates.Size())
				return;

			// Transitions (pas de re-trigger pendant un fondu).
			if (mNext < 0) {
				for (uint32 i = 0; i < (uint32)mTransitions.Size(); i++) {
					const Transition &tr = mTransitions[i];
					if (tr.to < 0 || tr.to >= (int32)mStates.Size() || tr.to == mCurrent)
						continue;
					if (tr.from >= 0 && tr.from != mCurrent)
						continue;
					if (!CondTrue(tr))
						continue;
					mNext = tr.to;
					mFadeDur = tr.fadeDur > 1e-3f ? tr.fadeDur : 1e-3f;
					mFadeT = mFadeDur;
					mStates[(uint32)mNext].time = 0.f; // repart du debut
					if (mTransitionCb)
						mTransitionCb(mStates[(uint32)mCurrent].name, mStates[(uint32)mNext].name,
									  /*finished=*/false);
					break;
				}
			}

			const NkAnimationClip *skelA = nullptr;
			EvalState(mCurrent, dt, mState, mLocalA, skelA);

			if (mNext >= 0) {
				const NkAnimationClip *skelB = nullptr;
				EvalState(mNext, dt, mNextState, mLocalB, skelB);
				mFadeT -= dt;
				const float32 w = 1.f - (mFadeT > 0.f ? mFadeT / mFadeDur : 0.f); // 0 -> 1
				// Crossfade BONE-LOCAL si les deux etats exposent leur pose locale
				// sur le meme squelette : blend TRS par os PUIS un seul FK —
				// correct sur les rotations. Sinon fallback lerp matriciel des
				// matrices de skinning (fondus courts).
				const bool localOK = !mLocalA.Empty() && !mLocalB.Empty() && skelA &&
									 mLocalA.Size() == mLocalB.Size();
				if (localOK) {
					const uint32 bc = (uint32)mLocalA.Size();
					mState.boneMatrices.Resize(bc);
					for (uint32 i = 0; i < bc; i++)
						mState.boneMatrices[i] = NkBlendLocalTRS(mLocalA[i], mLocalB[i], w);
					skelA->ApplyFKSkinning(mState.boneMatrices);
				} else {
					const uint32 bc = (uint32)mState.boneMatrices.Size();
					const uint32 bn = (uint32)mNextState.boneMatrices.Size();
					for (uint32 i = 0; i < bc && i < bn; i++) {
						NkMat4f &a = mState.boneMatrices[i];
						const NkMat4f &b = mNextState.boneMatrices[i];
						for (int r = 0; r < 4; r++)
							for (int c = 0; c < 4; c++)
								a[r][c] = a[r][c] + (b[r][c] - a[r][c]) * w;
					}
				}
				const uint32 mc = (uint32)mState.morphWeights.Size();
				const uint32 mn = (uint32)mNextState.morphWeights.Size();
				for (uint32 i = 0; i < mc && i < mn; i++)
					mState.morphWeights[i] += (mNextState.morphWeights[i] - mState.morphWeights[i]) * w;
				if (mFadeT <= 0.f) {
					const int32 prev = mCurrent;
					mCurrent = mNext;
					mNext = -1;
					if (mTransitionCb)
						mTransitionCb(mStates[(uint32)prev].name, mStates[(uint32)mCurrent].name,
									  /*finished=*/true);
				}
			}
		}

	} // namespace anim
} // namespace nkentseu
