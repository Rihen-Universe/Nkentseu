// -----------------------------------------------------------------------------
// @File    NkAnimMix.cpp
// @Brief   Le melange d'animations (voir NkAnimMix.h) : poses, masques,
//          additif, arbres 1D / 2D, pistes de clips, couches, fichier.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NKAnima/Blend/NkAnimMix.h"

#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"

#include <cmath>
#include <cstring>

namespace nkentseu {
	namespace anim {

		namespace {
			using PK = NkAnimationClip::NkPropertyKind;

			constexpr uint32 kMagicCtl = 0x43414B4E;  // 'NKAC' (le meme que NkAnimation.cpp)
			constexpr uint32 kSectionLAYR = 0x5259414C; // 'LAYR'
			constexpr uint32 kSectionBLND = 0x444E4C42; // 'BLND'
			constexpr uint32 kSectionMASK = 0x4B53414D; // 'MASK'
			constexpr uint32 kMaxRecursion = 4;			// clips poses dans des clips poses

			float32 Clamp01(float32 v) {
				return v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
			}

			/// Le NLERP par le chemin court (NkQuat::SLerp reste fragile, cf. NkBlendLocalTRS).
			NkQuatf Nlerp(const NkQuatf &a, const NkQuatf &b, float32 w) {
				const float32 d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
				const float32 sg = d < 0.f ? -1.f : 1.f;
				NkQuatf q(a.x + (b.x * sg - a.x) * w, a.y + (b.y * sg - a.y) * w, a.z + (b.z * sg - a.z) * w,
						  a.w + (b.w * sg - a.w) * w);
				return q.Normalized();
			}

			NkBoneTRS Decomposer(const NkMat4f &m) {
				NkBoneTRS b;
				NkMat4f r;
				m.DecomposeTRS(b.t, r, b.s);
				b.r = NkQuatf(r);
				return b;
			}

			/// Un os d'une piste de matrices, au temps t : TRS lineaire / NLERP entre
			/// deux cles (le meme geste que SampleBoneLocalSlerp ; palier tenu).
			NkBoneTRS EchantillonOs(const NkAnimationTrack<NkMat4f> &tr, float32 t) {
				const uint32 n = tr.KeyCount();
				if (n == 0) {
					return NkBoneTRS();
				}
				if (n == 1 || t <= tr.GetKey(0).time) {
					return Decomposer(tr.GetKey(0).value);
				}
				if (t >= tr.GetKey(n - 1).time) {
					return Decomposer(tr.GetKey(n - 1).value);
				}
				uint32 hi = 1;
				while (hi < n && tr.GetKey(hi).time <= t) {
					++hi;
				}
				const NkKeyframe<NkMat4f> &ka = tr.GetKey(hi - 1);
				const NkKeyframe<NkMat4f> &kb = tr.GetKey(hi);
				const NkBoneTRS a = Decomposer(ka.value);
				if (ka.interp == NkInterpMode::NK_STEP) {
					return a;
				}
				const NkBoneTRS b = Decomposer(kb.value);
				const float32 d = kb.time - ka.time;
				const float32 u = d > 1e-6f ? (t - ka.time) / d : 0.f;
				NkBoneTRS r;
				r.t = {a.t.x + (b.t.x - a.t.x) * u, a.t.y + (b.t.y - a.t.y) * u, a.t.z + (b.t.z - a.t.z) * u};
				r.s = {a.s.x + (b.s.x - a.s.x) * u, a.s.y + (b.s.y - a.s.y) * u, a.s.z + (b.s.z - a.s.z) * u};
				r.r = Nlerp(a.r, b.r, u);
				return r;
			}

			bool EstSousObjet(const NkString &objet, const NkString &ancetre) {
				if (ancetre.Empty() || objet == ancetre) {
					return true;
				}
				const usize n = ancetre.Size();
				return objet.Size() > n && objet.CStr()[n] == '/' && std::strncmp(objet.CStr(), ancetre.CStr(), n) == 0;
			}

			// ── Les octets du fichier (.nkanimctl) ──────────────────────────────
			struct Ecrivain {
					NkVector<nk_uint8> buf;
					void raw(const void *p, usize n) {
						const nk_uint8 *b = (const nk_uint8 *)p;
						for (usize i = 0; i < n; ++i) {
							buf.PushBack(b[i]);
						}
					}
					void u8(uint8 v) {
						raw(&v, 1);
					}
					void u32(uint32 v) {
						raw(&v, 4);
					}
					void f32(float32 v) {
						raw(&v, 4);
					}
					void str(const NkString &s) {
						const uint32 n = (uint32)s.Size();
						u32(n);
						if (n) {
							raw(s.CStr(), n);
						}
					}
			};

			struct Lecteur {
					const nk_uint8 *p;
					usize n, off = 0;
					bool ok = true;
					Lecteur(const nk_uint8 *d, usize len) : p(d), n(len) {
					}
					bool need(usize c) {
						if (off + c > n) {
							ok = false;
							return false;
						}
						return true;
					}
					uint8 u8() {
						return need(1) ? p[off++] : (uint8)0;
					}
					uint32 u32() {
						if (!need(4)) {
							return 0;
						}
						uint32 v;
						std::memcpy(&v, p + off, 4);
						off += 4;
						return v;
					}
					float32 f32() {
						if (!need(4)) {
							return 0.f;
						}
						float32 v;
						std::memcpy(&v, p + off, 4);
						off += 4;
						return v;
					}
					NkString str() {
						const uint32 len = u32();
						if (!need(len)) {
							return NkString();
						}
						NkString s((const char *)(p + off), (usize)len);
						off += len;
						return s;
					}
			};

			void Section(Ecrivain &w, uint32 tag, const Ecrivain &s) {
				w.u32(tag);
				w.u32((uint32)s.buf.Size());
				w.raw(s.buf.Data(), s.buf.Size());
			}

			float32 ValeurParam(const NkAnimStateMachine &sm, const NkString &nom) {
				if (nom.Empty()) {
					return 0.f;
				}
				int32 i = sm.FindParam(nom, NkAnimStateMachine::NkParamKind::FLOAT);
				if (i < 0) {
					i = sm.FindParam(nom, NkAnimStateMachine::NkParamKind::BOOL);
				}
				return i >= 0 ? sm.GetParamValue((uint32)i) : 0.f;
			}
		} // namespace

		// =====================================================================
		// LA POSE
		// =====================================================================
		void NkAnimPose::Clear() {
			bones.Clear();
			morphs.Clear();
			props.Clear();
			skeleton = nullptr;
		}

		int32 NkAnimPose::FindProp(const NkString &target, const NkString &property) const {
			for (uint32 i = 0; i < (uint32)props.Size(); ++i) {
				if (props[i].target == target && props[i].property == property) {
					return (int32)i;
				}
			}
			return -1;
		}

		NkPropValue &NkAnimPose::Prop(const NkString &target, const NkString &property, NkAnimationClip::NkPropertyKind kind) {
			const int32 i = FindProp(target, property);
			if (i >= 0) {
				return props[(uint32)i];
			}
			NkPropValue v;
			v.target = target;
			v.property = property;
			v.kind = kind;
			v.weight = 0.f;
			props.PushBack(v);
			return props[props.Size() - 1];
		}

		void NkAnimPose::ToLocalMatrices(NkVector<NkMat4f> &out) const {
			out.Resize(bones.Size());
			for (uint32 j = 0; j < (uint32)bones.Size(); ++j) {
				const NkBoneTRS &b = bones[j];
				out[j] = NkMat4f::Translate(b.t) * b.r.Normalized().ToMat4() * NkMat4f::Scale(b.s);
			}
		}

		void NkAnimPose::FromLocalMatrices(const NkVector<NkMat4f> &in) {
			bones.Resize(in.Size());
			for (uint32 j = 0; j < (uint32)in.Size(); ++j) {
				bones[j] = Decomposer(in[j]);
			}
		}

		bool NkAnimPose::ToSkinning(NkVector<NkMat4f> &out) const {
			if (skeleton == nullptr || !skeleton->skeletalLocal || bones.Empty()) {
				return false;
			}
			ToLocalMatrices(out);
			skeleton->ApplyFKSkinning(out);
			return true;
		}

		float32 NkClipTime(const NkAnimationClip &clip, float32 elapsed) {
			const float32 d = clip.duration;
			if (d <= 1e-6f) {
				return 0.f;
			}
			if (clip.loop) {
				const float32 t = std::fmod(elapsed, d);
				return t < 0.f ? t + d : t;
			}
			return elapsed < 0.f ? 0.f : (elapsed > d ? d : elapsed);
		}

		void NkSampleClip(const NkAnimationClip &clip, float32 t, NkAnimPose &out, const NkClipLookup *lookup,
						  const NkVector<NkAnimMask> *masks, uint32 depth) {
			out.Clear();
			// Les os : locaux, quel que soit le mode du clip (en mode skinning, la
			// « pose » melangee est celle des matrices du clip -- comme le repli
			// matriciel de la machine).
			if (clip.boneCount > 0) {
				out.bones.Resize(clip.boneCount);
				for (uint32 j = 0; j < clip.boneCount; ++j) {
					out.bones[j] = (j < (uint32)clip.boneTracks.Size() && !clip.boneTracks[j].Empty())
									   ? EchantillonOs(clip.boneTracks[j], t)
									   : NkBoneTRS();
				}
				out.skeleton = &clip;
			}
			out.morphs.Resize((uint32)clip.morphTracks.Size(), 0.f);
			for (uint32 i = 0; i < (uint32)clip.morphTracks.Size(); ++i) {
				out.morphs[i] = clip.morphTracks[i].Evaluate(t);
			}
			// Les proprietes (2D d'Unkeny et toute propriete nommee) : couverture 1.
			for (uint32 i = 0; i < (uint32)clip.propertyTracks.Size(); ++i) {
				const NkAnimationClip::NkPropertyTrack &p = clip.propertyTracks[i];
				if (!p.curve.enabled || p.curve.Empty()) {
					continue; // une piste MUETTE n'anime rien
				}
				NkPropValue v;
				v.target = p.target;
				v.property = p.property;
				v.kind = p.kind;
				v.value = p.Evaluate(t);
				v.weight = 1.f;
				out.props.PushBack(v);
			}
			// Les clips poses (NLA), au-dessus des pistes propres du clip.
			if (lookup != nullptr && !clip.clipTracks.Empty() && depth < kMaxRecursion) {
				NkEvaluateStrips(clip.clipTracks, t, *lookup, out, masks, depth + 1);
			}
		}

		// =====================================================================
		// LES MASQUES
		// =====================================================================
		float32 NkAnimMask::TargetWeight(const NkString &target) const {
			if (entries.Empty()) {
				return 1.f;
			}
			int32 meilleure = -1;
			usize longueur = 0;
			for (uint32 i = 0; i < (uint32)entries.Size(); ++i) {
				const Entry &e = entries[i];
				const bool touche = target == e.path || (e.children && EstSousObjet(target, e.path));
				if (touche && (meilleure < 0 || e.path.Size() >= longueur)) {
					meilleure = (int32)i;
					longueur = e.path.Size();
				}
			}
			return meilleure >= 0 ? entries[(uint32)meilleure].weight : 0.f;
		}

		void NkAnimMask::BoneWeights(const NkAnimationClip &skel, NkVector<float32> &out) const {
			const uint32 n = skel.boneCount > (uint32)skel.jointNames.Size() ? skel.boneCount : (uint32)skel.jointNames.Size();
			out.Resize(n, entries.Empty() ? 1.f : 0.f);
			if (entries.Empty()) {
				for (uint32 j = 0; j < n; ++j) {
					out[j] = 1.f;
				}
				return;
			}
			for (uint32 j = 0; j < n; ++j) {
				out[j] = 0.f;
				// L'articulation, puis ses ancetres : la premiere entree qui la nomme.
				int32 k = (int32)j;
				bool trouve = false;
				for (uint32 garde = 0; k >= 0 && !trouve && garde < 256; ++garde) {
					const NkString &nom = (uint32)k < (uint32)skel.jointNames.Size() ? skel.jointNames[(uint32)k] : NkString();
					for (uint32 e = 0; e < (uint32)entries.Size() && !nom.Empty(); ++e) {
						if (entries[e].path == nom && (entries[e].children || k == (int32)j)) {
							out[j] = entries[e].weight;
							trouve = true;
						}
					}
					k = (uint32)k < (uint32)skel.jointParent.Size() ? skel.jointParent[(uint32)k] : -1;
				}
			}
		}

		const NkAnimMask *NkFindMask(const NkVector<NkAnimMask> *masks, const NkString &name) {
			for (uint32 i = 0; masks != nullptr && !name.Empty() && i < (uint32)masks->Size(); ++i) {
				if ((*masks)[i].name == name) {
					return &(*masks)[i];
				}
			}
			return nullptr;
		}

		// =====================================================================
		// LES OPERATIONS
		// =====================================================================
		void NkBlendPose(NkAnimPose &a, const NkAnimPose &b, float32 w, const NkVector<float32> *boneWeights,
						 const NkAnimMask *mask) {
			if (w <= 0.f) {
				return;
			}
			w = w > 1.f ? 1.f : w;
			// Les os.
			if (!b.bones.Empty()) {
				if (a.bones.Empty()) {
					a.bones = b.bones;
					a.skeleton = b.skeleton;
				} else {
					const uint32 n = a.bones.Size() < b.bones.Size() ? (uint32)a.bones.Size() : (uint32)b.bones.Size();
					for (uint32 j = 0; j < n; ++j) {
						const float32 ww =
							w * (boneWeights != nullptr && j < (uint32)boneWeights->Size() ? (*boneWeights)[j] : 1.f);
						if (ww <= 0.f) {
							continue;
						}
						NkBoneTRS &x = a.bones[j];
						const NkBoneTRS &y = b.bones[j];
						x.t = {x.t.x + (y.t.x - x.t.x) * ww, x.t.y + (y.t.y - x.t.y) * ww, x.t.z + (y.t.z - x.t.z) * ww};
						x.s = {x.s.x + (y.s.x - x.s.x) * ww, x.s.y + (y.s.y - x.s.y) * ww, x.s.z + (y.s.z - x.s.z) * ww};
						x.r = Nlerp(x.r, y.r, ww);
					}
				}
			}
			// Les morphs.
			if (a.morphs.Size() < b.morphs.Size()) {
				a.morphs.Resize(b.morphs.Size(), 0.f);
			}
			for (uint32 i = 0; i < (uint32)b.morphs.Size(); ++i) {
				a.morphs[i] += (b.morphs[i] - a.morphs[i]) * w;
			}
			// Les proprietes, PAR COUVERTURE : une valeur que b n'anime pas recule
			// vers la valeur vivante ; une que a n'anime pas arrive depuis elle.
			for (uint32 i = 0; i < (uint32)a.props.Size(); ++i) {
				NkPropValue &pa = a.props[i];
				if (b.FindProp(pa.target, pa.property) < 0) {
					const float32 ww = w * (mask != nullptr ? mask->TargetWeight(pa.target) : 1.f);
					pa.weight *= 1.f - ww;
				}
			}
			for (uint32 i = 0; i < (uint32)b.props.Size(); ++i) {
				const NkPropValue &pb = b.props[i];
				const float32 ww = w * (mask != nullptr ? mask->TargetWeight(pb.target) : 1.f);
				if (ww <= 0.f) {
					continue;
				}
				const int32 ia = a.FindProp(pb.target, pb.property);
				if (ia < 0) {
					NkPropValue v = pb;
					v.weight = pb.weight * ww;
					a.props.PushBack(v);
					continue;
				}
				NkPropValue &pa = a.props[(uint32)ia];
				const float32 wa = pa.weight * (1.f - ww), wb = pb.weight * ww;
				const float32 tot = wa + wb;
				if (tot <= 1e-9f) {
					continue;
				}
				if (pb.kind == PK::NK_STEP) {
					// Un palier ne glisse pas : il bascule a mi-chemin.
					if (wb >= wa) {
						pa.value = pb.value;
					}
				} else {
					for (uint32 c = 0; c < 4; ++c) {
						pa.value[c] = (pa.value[c] * wa + pb.value[c] * wb) / tot;
					}
				}
				pa.weight = tot > 1.f ? 1.f : tot;
			}
		}

		void NkMakeAdditive(const NkAnimPose &pose, const NkAnimPose &reference, NkAnimPose &delta) {
			delta.Clear();
			delta.skeleton = pose.skeleton;
			delta.bones.Resize(pose.bones.Size());
			for (uint32 j = 0; j < (uint32)pose.bones.Size(); ++j) {
				NkBoneTRS d;
				if (j < (uint32)reference.bones.Size()) {
					const NkBoneTRS &p = pose.bones[j];
					const NkBoneTRS &r = reference.bones[j];
					d.t = {p.t.x - r.t.x, p.t.y - r.t.y, p.t.z - r.t.z};
					d.r = (r.r.Conjugate() * p.r).Normalized();
					auto div = [](float32 a, float32 b) { return (b > 1e-6f || b < -1e-6f) ? a / b : 1.f; };
					d.s = {div(p.s.x, r.s.x), div(p.s.y, r.s.y), div(p.s.z, r.s.z)};
				}
				delta.bones[j] = d;
			}
			delta.morphs.Resize(pose.morphs.Size(), 0.f);
			for (uint32 i = 0; i < (uint32)pose.morphs.Size(); ++i) {
				delta.morphs[i] = pose.morphs[i] - (i < (uint32)reference.morphs.Size() ? reference.morphs[i] : 0.f);
			}
			for (uint32 i = 0; i < (uint32)pose.props.Size(); ++i) {
				const NkPropValue &pp = pose.props[i];
				const int32 ir = reference.FindProp(pp.target, pp.property);
				if (ir < 0 || pp.kind == PK::NK_STEP) {
					continue; // un palier ne s'additionne pas
				}
				NkPropValue d = pp;
				for (uint32 c = 0; c < 4; ++c) {
					d.value[c] = pp.value[c] - reference.props[(uint32)ir].value[c];
				}
				delta.props.PushBack(d);
			}
		}

		void NkApplyAdditive(NkAnimPose &base, const NkAnimPose &delta, float32 w, const NkVector<float32> *boneWeights,
							 const NkAnimMask *mask) {
			if (w <= 0.f) {
				return;
			}
			const uint32 n = base.bones.Size() < delta.bones.Size() ? (uint32)base.bones.Size() : (uint32)delta.bones.Size();
			const NkQuatf identite;
			for (uint32 j = 0; j < n; ++j) {
				const float32 ww = w * (boneWeights != nullptr && j < (uint32)boneWeights->Size() ? (*boneWeights)[j] : 1.f);
				if (ww <= 0.f) {
					continue;
				}
				NkBoneTRS &b = base.bones[j];
				const NkBoneTRS &d = delta.bones[j];
				b.t = {b.t.x + d.t.x * ww, b.t.y + d.t.y * ww, b.t.z + d.t.z * ww};
				b.r = (b.r * Nlerp(identite, d.r, ww)).Normalized();
				b.s = {b.s.x * (1.f + (d.s.x - 1.f) * ww), b.s.y * (1.f + (d.s.y - 1.f) * ww), b.s.z * (1.f + (d.s.z - 1.f) * ww)};
			}
			for (uint32 i = 0; i < (uint32)base.morphs.Size() && i < (uint32)delta.morphs.Size(); ++i) {
				base.morphs[i] += delta.morphs[i] * w;
			}
			for (uint32 i = 0; i < (uint32)delta.props.Size(); ++i) {
				const NkPropValue &pd = delta.props[i];
				const int32 ib = base.FindProp(pd.target, pd.property);
				if (ib < 0 || pd.kind == PK::NK_STEP) {
					continue;
				}
				const float32 ww = w * (mask != nullptr ? mask->TargetWeight(pd.target) : 1.f);
				for (uint32 c = 0; c < 4; ++c) {
					base.props[(uint32)ib].value[c] += pd.value[c] * ww;
				}
			}
		}

		// =====================================================================
		// LES ARBRES DE MELANGE
		// =====================================================================
		NkBlendSample &NkBlendSpaceDef::AddSample(const NkString &clip, float32 x, float32 y) {
			NkBlendSample s;
			s.clip = clip;
			s.x = x;
			s.y = y;
			samples.PushBack(s);
			return samples[samples.Size() - 1];
		}

		void NkBlendSpaceDef::Weights(float32 x, float32 y, NkVector<float32> &out) const {
			const uint32 n = (uint32)samples.Size();
			out.Resize(n, 0.f);
			for (uint32 i = 0; i < n; ++i) {
				out[i] = 0.f;
			}
			if (n == 0) {
				return;
			}
			if (n == 1) {
				out[0] = 1.f;
				return;
			}
			if (dims <= 1) {
				// 1D : les deux VOISINS de x, lineairement ; au-dela des bouts, le bout.
				int32 lo = -1, hi = -1;
				for (uint32 i = 0; i < n; ++i) {
					const float32 sx = samples[i].x;
					if (sx <= x && (lo < 0 || sx > samples[(uint32)lo].x)) {
						lo = (int32)i;
					}
					if (sx >= x && (hi < 0 || sx < samples[(uint32)hi].x)) {
						hi = (int32)i;
					}
				}
				if (lo < 0) {
					out[(uint32)hi] = 1.f;
				} else if (hi < 0 || lo == hi) {
					out[(uint32)lo] = 1.f;
				} else {
					const float32 a = samples[(uint32)lo].x, b = samples[(uint32)hi].x;
					const float32 u = b - a > 1e-6f ? (x - a) / (b - a) : 0.f;
					out[(uint32)lo] = 1.f - u;
					out[(uint32)hi] += u;
				}
				return;
			}
			// 2D : « gradient band » (le Freeform Cartesian d'Unity) -- chaque
			// echantillon pese le MINIMUM de ses bandes vers les autres ; exact sur
			// chaque point, continu entre eux, sans triangulation.
			float32 somme = 0.f;
			for (uint32 i = 0; i < n; ++i) {
				float32 h = 1e30f;
				for (uint32 j = 0; j < n; ++j) {
					if (i == j) {
						continue;
					}
					const float32 dx = samples[j].x - samples[i].x, dy = samples[j].y - samples[i].y;
					const float32 dd = dx * dx + dy * dy;
					if (dd < 1e-9f) {
						continue;
					}
					const float32 v = 1.f - ((x - samples[i].x) * dx + (y - samples[i].y) * dy) / dd;
					h = v < h ? v : h;
				}
				h = h > 1e29f ? 1.f : (h < 0.f ? 0.f : h);
				out[i] = h;
				somme += h;
			}
			if (somme > 1e-9f) {
				for (uint32 i = 0; i < n; ++i) {
					out[i] /= somme;
				}
				return;
			}
			// Hors de toute bande : le plus proche.
			uint32 best = 0;
			float32 bd = 1e30f;
			for (uint32 i = 0; i < n; ++i) {
				const float32 dx = samples[i].x - x, dy = samples[i].y - y;
				if (dx * dx + dy * dy < bd) {
					bd = dx * dx + dy * dy;
					best = i;
				}
			}
			out[best] = 1.f;
		}

		bool NkSampleBlendSpace(const NkBlendSpaceDef &bs, float32 x, float32 y, float32 phase, const NkClipLookup &lookup,
								NkAnimPose &out, float32 *outDuration) {
			out.Clear();
			NkVector<float32> w;
			bs.Weights(x, y, w);
			float32 total = 0.f, duree = 0.f;
			NkAnimPose p;
			bool premier = true;
			phase = phase - std::floor(phase);
			for (uint32 i = 0; i < (uint32)bs.samples.Size(); ++i) {
				if (w[i] <= 1e-5f || !lookup) {
					continue;
				}
				const NkAnimationClip *clip = lookup(bs.samples[i].clip);
				if (clip == nullptr) {
					continue;
				}
				const float32 rate = bs.samples[i].rate > 1e-3f ? bs.samples[i].rate : 1.f;
				const float32 d = clip->duration > 1e-4f ? clip->duration : 1.f;
				// Phases SYNCHRONISEES : tous a la meme fraction de leur cycle.
				NkSampleClip(*clip, phase * d, p, &lookup);
				duree += w[i] * d / rate;
				total += w[i];
				if (premier) {
					out = p;
					premier = false;
				} else {
					NkBlendPose(out, p, w[i] / total);
				}
			}
			if (outDuration != nullptr) {
				*outDuration = total > 1e-6f ? duree / total : 1.f;
			}
			return total > 0.f;
		}

		// =====================================================================
		// LES PISTES DE CLIPS (NLA)
		// =====================================================================
		void NkEvaluateStrips(const NkVector<NkClipStripTrack> &tracks, float32 t, const NkClipLookup &lookup, NkAnimPose &io,
							  const NkVector<NkAnimMask> *masks, uint32 depth) {
			if (!lookup) {
				return;
			}
			NkAnimPose piste, p, ref, delta;
			NkVector<float32> poidsOs;
			for (uint32 ti = 0; ti < (uint32)tracks.Size(); ++ti) {
				const NkClipStripTrack &tr = tracks[ti];
				if (tr.muted || tr.weight <= 0.f) {
					continue;
				}
				const NkAnimMask *mask = NkFindMask(masks, tr.mask);
				piste.Clear();
				float32 somme = 0.f;
				bool premier = true;
				for (uint32 k = 0; k < (uint32)tr.strips.Size(); ++k) {
					const NkClipStrip &st = tr.strips[k];
					const float32 w = st.WeightAt(t);
					if (w <= 0.f) {
						continue;
					}
					const NkAnimationClip *clip = lookup(st.clip);
					if (clip == nullptr) {
						continue;
					}
					const float32 local = st.LocalTime(t, clip->duration);
					NkSampleClip(*clip, local, p, &lookup, masks, depth + 1);
					if (tr.additive) {
						// L'additif se mesure a la premiere image de SON clip (UE5).
						NkSampleClip(*clip, 0.f, ref, &lookup, masks, depth + 1);
						NkMakeAdditive(p, ref, delta);
						poidsOs.Clear();
						if (mask != nullptr && io.skeleton != nullptr) {
							mask->BoneWeights(*io.skeleton, poidsOs);
						}
						NkApplyAdditive(io, delta, w * tr.weight, poidsOs.Empty() ? nullptr : &poidsOs, mask);
						continue;
					}
					// Les clips d'une piste qui se chevauchent : moyenne PONDEREE.
					somme += w;
					if (premier) {
						piste = p;
						premier = false;
					} else {
						NkBlendPose(piste, p, w / somme);
					}
				}
				if (tr.additive || somme <= 0.f) {
					continue;
				}
				// La piste se pose sur le dessous : son poids x sa couverture (un
				// clip seul a mi-fondu ne couvre que la moitie).
				const float32 influence = tr.weight * (somme > 1.f ? 1.f : somme);
				poidsOs.Clear();
				const NkAnimationClip *sq = io.skeleton != nullptr ? io.skeleton : piste.skeleton;
				if (mask != nullptr && sq != nullptr) {
					mask->BoneWeights(*sq, poidsOs);
				}
				NkBlendPose(io, piste, influence, poidsOs.Empty() ? nullptr : &poidsOs, mask);
			}
		}

		// =====================================================================
		// LE CONTROLEUR
		// =====================================================================
		NkAnimLayer *NkAnimController::AddLayer(const NkString &layerName, NkLayerMode mode, float32 weight,
												const NkString &mask) {
			if ((uint32)layers.Size() + 1 >= NK_ANIM_MAX_LAYERS) {
				return nullptr;
			}
			NkAnimLayer l;
			l.name = layerName;
			l.mode = mode;
			l.weight = weight;
			l.mask = mask;
			layers.PushBack(l);
			return &layers[layers.Size() - 1];
		}

		NkBlendSpaceDef &NkAnimController::AddBlendSpace(const NkString &spaceName, uint8 dims, const NkString &paramX,
														 const NkString &paramY) {
			for (uint32 i = 0; i < (uint32)blendSpaces.Size(); ++i) {
				if (blendSpaces[i].name == spaceName) {
					return blendSpaces[i];
				}
			}
			NkBlendSpaceDef b;
			b.name = spaceName;
			b.dims = dims < 1 ? (uint8)1 : (dims > 2 ? (uint8)2 : dims);
			b.paramX = paramX;
			b.paramY = paramY;
			blendSpaces.PushBack(b);
			return blendSpaces[blendSpaces.Size() - 1];
		}

		NkAnimMask &NkAnimController::AddMask(const NkString &maskName) {
			for (uint32 i = 0; i < (uint32)masks.Size(); ++i) {
				if (masks[i].name == maskName) {
					return masks[i];
				}
			}
			NkAnimMask m;
			m.name = maskName;
			masks.PushBack(m);
			return masks[masks.Size() - 1];
		}

		const NkBlendSpaceDef *NkAnimController::FindBlendSpace(const NkString &spaceName) const {
			for (uint32 i = 0; i < (uint32)blendSpaces.Size(); ++i) {
				if (blendSpaces[i].name == spaceName) {
					return &blendSpaces[i];
				}
			}
			return nullptr;
		}

		const NkAnimMask *NkAnimController::FindMask(const NkString &maskName) const {
			return NkFindMask(&masks, maskName);
		}

		bool NkAnimController::HasMixing() const {
			if (!layers.Empty() || !blendSpaces.Empty()) {
				return true;
			}
			for (int32 i = 0; i < base.GetStateCount(); ++i) {
				if (!base.IsSubMachine(i) && base.GetStateRefKind(i) != 0 && !base.GetStateRef(i).Empty()) {
					return true;
				}
			}
			return false;
		}

		void NkAnimController::SaveToBytes(NkVector<nk_uint8> &out) const {
			base.SaveToBytes(out);
			if (out.Size() < 12) {
				return;
			}
			Ecrivain w;
			uint32 ajout = 0;
			if (!layers.Empty()) {
				// 'LAYR' : [version=1] [n] par couche : [nom] [poids] [mode(u8)] [masque]
				//   [parametre de poids] [taille(u32)] [la machine, fichier complet]
				Ecrivain s;
				s.u32(1);
				s.u32((uint32)layers.Size());
				for (uint32 i = 0; i < (uint32)layers.Size(); ++i) {
					const NkAnimLayer &l = layers[i];
					s.str(l.name);
					s.f32(l.weight);
					s.u8((uint8)l.mode);
					s.str(l.mask);
					s.str(l.weightParam);
					NkVector<nk_uint8> m;
					l.machine.SaveToBytes(m);
					s.u32((uint32)m.Size());
					s.raw(m.Data(), m.Size());
				}
				Section(w, kSectionLAYR, s);
				++ajout;
			}
			if (!blendSpaces.Empty()) {
				// 'BLND' : [version=1] [n] par arbre : [nom] [dims(u8)] [paramX] [paramY]
				//   [synchro(u8)] [nbEch] par echantillon : [clip] [x] [y] [vitesse]
				Ecrivain s;
				s.u32(1);
				s.u32((uint32)blendSpaces.Size());
				for (uint32 i = 0; i < (uint32)blendSpaces.Size(); ++i) {
					const NkBlendSpaceDef &b = blendSpaces[i];
					s.str(b.name);
					s.u8(b.dims);
					s.str(b.paramX);
					s.str(b.paramY);
					s.u8(b.syncPhase ? 1 : 0);
					s.u32((uint32)b.samples.Size());
					for (uint32 k = 0; k < (uint32)b.samples.Size(); ++k) {
						s.str(b.samples[k].clip);
						s.f32(b.samples[k].x);
						s.f32(b.samples[k].y);
						s.f32(b.samples[k].rate);
					}
				}
				Section(w, kSectionBLND, s);
				++ajout;
			}
			if (!masks.Empty()) {
				// 'MASK' : [version=1] [n] par masque : [nom] [nbEntrees] par entree :
				//   [chemin] [poids] [descendants(u8)]
				Ecrivain s;
				s.u32(1);
				s.u32((uint32)masks.Size());
				for (uint32 i = 0; i < (uint32)masks.Size(); ++i) {
					s.str(masks[i].name);
					s.u32((uint32)masks[i].entries.Size());
					for (uint32 k = 0; k < (uint32)masks[i].entries.Size(); ++k) {
						s.str(masks[i].entries[k].path);
						s.f32(masks[i].entries[k].weight);
						s.u8(masks[i].entries[k].children ? 1 : 0);
					}
				}
				Section(w, kSectionMASK, s);
				++ajout;
			}
			if (ajout == 0) {
				return; // une machine seule : le fichier d'avant, octet pour octet
			}
			uint32 nb = 0;
			std::memcpy(&nb, out.Data() + 8, 4);
			nb += ajout;
			std::memcpy(out.Data() + 8, &nb, 4);
			for (uint32 i = 0; i < (uint32)w.buf.Size(); ++i) {
				out.PushBack(w.buf[i]);
			}
		}

		bool NkAnimController::LoadFromBytes(const nk_uint8 *data, usize size) {
			if (!base.LoadFromBytes(data, size)) {
				return false;
			}
			layers.Clear();
			blendSpaces.Clear();
			masks.Clear();
			Lecteur r(data, size);
			if (r.u32() != kMagicCtl) {
				return true; // un .nkanim v3 (machine du 29/09) : la base seule
			}
			r.u32(); // version
			const uint32 nb = r.u32();
			for (uint32 sct = 0; sct < nb && r.ok; ++sct) {
				const uint32 tag = r.u32();
				const uint32 taille = r.u32();
				if (!r.need(taille)) {
					break;
				}
				Lecteur s(r.p + r.off, taille);
				r.off += taille;
				if (tag == kSectionLAYR && s.u32() == 1u) {
					const uint32 n = s.u32();
					for (uint32 i = 0; i < n && s.ok && i + 1 < NK_ANIM_MAX_LAYERS; ++i) {
						NkAnimLayer l;
						l.name = s.str();
						l.weight = s.f32();
						const uint8 mode = s.u8();
						l.mode = mode == 1 ? NkLayerMode::NK_ADDITIVE : NkLayerMode::NK_OVERRIDE;
						l.mask = s.str();
						l.weightParam = s.str();
						const uint32 tm = s.u32();
						if (!s.need(tm)) {
							break;
						}
						const bool ok = l.machine.LoadFromBytes(s.p + s.off, tm);
						s.off += tm;
						if (ok) {
							layers.PushBack(l);
						}
					}
				} else if (tag == kSectionBLND && s.u32() == 1u) {
					const uint32 n = s.u32();
					for (uint32 i = 0; i < n && s.ok; ++i) {
						NkBlendSpaceDef b;
						b.name = s.str();
						b.dims = s.u8();
						b.paramX = s.str();
						b.paramY = s.str();
						b.syncPhase = s.u8() != 0;
						uint32 ne = s.u32();
						if (ne > (s.n - s.off) / 16u) {
							ne = 0;
						}
						for (uint32 k = 0; k < ne && s.ok; ++k) {
							NkBlendSample e;
							e.clip = s.str();
							e.x = s.f32();
							e.y = s.f32();
							e.rate = s.f32();
							b.samples.PushBack(e);
						}
						if (s.ok) {
							blendSpaces.PushBack(b);
						}
					}
				} else if (tag == kSectionMASK && s.u32() == 1u) {
					const uint32 n = s.u32();
					for (uint32 i = 0; i < n && s.ok; ++i) {
						NkAnimMask m;
						m.name = s.str();
						uint32 ne = s.u32();
						if (ne > (s.n - s.off) / 9u) {
							ne = 0;
						}
						for (uint32 k = 0; k < ne && s.ok; ++k) {
							NkAnimMask::Entry e;
							e.path = s.str();
							e.weight = s.f32();
							e.children = s.u8() != 0;
							m.entries.PushBack(e);
						}
						if (s.ok) {
							masks.PushBack(m);
						}
					}
				}
			}
			return true;
		}

		bool NkAnimController::SaveBinary(const NkString &path) const {
			NkVector<nk_uint8> bytes;
			SaveToBytes(bytes);
			return NkFile::WriteAllBytes(path.CStr(), bytes);
		}

		bool NkAnimController::LoadBinary(const NkString &path) {
			NkVector<nk_uint8> bytes = NkFile::ReadAllBytes(path.CStr());
			return !bytes.Empty() && LoadFromBytes(bytes.Data(), bytes.Size());
		}

		// =====================================================================
		// L'EVALUATION D'UNE MACHINE ET D'UNE INSTANCE
		// =====================================================================
		namespace {
			/// La pose d'un etat feuille : son clip, ou son arbre de melange.
			bool PoseEtat(const NkAnimStateMachine &sm, int32 etat, float32 ecoule, float32 phase, const NkAnimController *ctl,
						  const NkAnimStateMachine &params, const NkClipLookup &lookup, NkAnimPose &out, float32 *duree) {
				out.Clear();
				if (etat < 0 || etat >= sm.GetStateCount() || !lookup) {
					return false;
				}
				const uint8 genre = sm.GetStateRefKind(etat);
				const NkString &ref = sm.GetStateRef(etat);
				if (ref.Empty()) {
					return false;
				}
				const NkVector<NkAnimMask> *masks = ctl != nullptr ? &ctl->masks : nullptr;
				if (genre == 1) {
					const NkAnimationClip *clip = lookup(ref);
					if (clip == nullptr) {
						return false;
					}
					NkSampleClip(*clip, NkClipTime(*clip, ecoule), out, &lookup, masks);
					if (duree != nullptr) {
						*duree = clip->duration > 1e-4f ? clip->duration : 1.f;
					}
					return true;
				}
				if ((genre == 2 || genre == 3) && ctl != nullptr) {
					const NkBlendSpaceDef *bs = ctl->FindBlendSpace(ref);
					if (bs == nullptr) {
						return false;
					}
					return NkSampleBlendSpace(*bs, ValeurParam(params, bs->paramX), ValeurParam(params, bs->paramY), phase,
											  lookup, out, duree);
				}
				return false;
			}

			void EvaluerMachine(const NkAnimStateMachine &sm, const NkAnimLayerRuntime &lr, const NkAnimController *ctl,
								const NkAnimStateMachine &params, const NkClipLookup &lookup, NkAnimPose &out,
								NkAnimPose *reference, float32 *dureeCur, float32 *dureeNext) {
				const NkAnimStateMachine::NkRuntime &rt = lr.rt;
				PoseEtat(sm, rt.current, rt.currentTime, lr.phaseCur, ctl, params, lookup, out, dureeCur);
				if (reference != nullptr) {
					PoseEtat(sm, rt.current, 0.f, 0.f, ctl, params, lookup, *reference, nullptr);
				}
				if (rt.next < 0) {
					return;
				}
				// Le FONDU en cours, par la courbe de SA transition.
				NkAnimPose suivant;
				PoseEtat(sm, rt.next, rt.nextTime, lr.phaseNext, ctl, params, lookup, suivant, dureeNext);
				const float32 u = rt.fadeDur > 1e-6f ? 1.f - (rt.fadeT > 0.f ? rt.fadeT / rt.fadeDur : 0.f) : 1.f;
				const int32 t = sm.FindTransitionTo(rt.current, rt.next);
				const NkFadeCurve courbe = t >= 0 ? sm.GetTransitionCurve((uint32)t) : NkFadeCurve::NK_LINEAR;
				NkBlendPose(out, suivant, NkFadeWeight(u, courbe));
				if (reference != nullptr) {
					NkAnimPose r2;
					PoseEtat(sm, rt.next, 0.f, 0.f, ctl, params, lookup, r2, nullptr);
					NkBlendPose(*reference, r2, NkFadeWeight(u, courbe));
				}
			}

			/// Les PHASES des arbres suivent les changements d'etat : le suivant
			/// devenu courant garde la sienne, un fondu neuf repart de zero.
			void AvancerPhases(NkAnimLayerRuntime &lr, float32 dt) {
				const NkAnimStateMachine::NkRuntime &rt = lr.rt;
				if (rt.current != lr.lastCur) {
					if (rt.current == lr.lastNext && lr.lastNext >= 0) {
						lr.phaseCur = lr.phaseNext;
						lr.durCur = lr.durNext;
					} else {
						lr.phaseCur = 0.f;
					}
				}
				if (rt.next != lr.lastNext) {
					lr.phaseNext = 0.f;
				}
				lr.phaseCur += dt / (lr.durCur > 1e-3f ? lr.durCur : 1e-3f);
				lr.phaseCur -= std::floor(lr.phaseCur);
				if (rt.next >= 0) {
					lr.phaseNext += dt / (lr.durNext > 1e-3f ? lr.durNext : 1e-3f);
					lr.phaseNext -= std::floor(lr.phaseNext);
				}
				lr.lastCur = rt.current;
				lr.lastNext = rt.next;
			}

			/// Les parametres d'une couche sont ceux de la BASE (memes noms) ; un
			/// declencheur que la couche CONSOMME l'est aussi dans la base.
			void CopierParams(const NkAnimStateMachine &base, NkAnimStateMachine &m) {
				for (uint32 i = 0; i < m.GetParamCount(); ++i) {
					const int32 j = base.FindParam(m.GetParamName(i), m.GetParamKind(i));
					if (j >= 0) {
						m.SetParamValue(i, base.GetParamValue((uint32)j));
					}
				}
			}
		} // namespace

		void NkEvaluateMachine(const NkAnimStateMachine &sm, const NkAnimLayerRuntime &lr, const NkAnimController *ctl,
							   const NkAnimStateMachine &params, const NkClipLookup &lookup, NkAnimPose &out,
							   NkAnimPose *reference) {
			EvaluerMachine(sm, lr, ctl, params, lookup, out, reference, nullptr, nullptr);
		}

		void NkAdvanceController(NkAnimController &ctl, NkAnimControllerRuntime &rt, float32 dt, const NkClipLookup &lookup,
								 NkAnimPose &out, bool advanceBase) {
			out.Clear();
			// 1. La BASE.
			{
				NkAnimLayerRuntime &lr = rt.layers[0];
				if (advanceBase) {
					ctl.base.SetRuntime(lr.rt);
					ctl.base.Update(dt);
					lr.rt = ctl.base.GetRuntime();
				}
				AvancerPhases(lr, dt);
				EvaluerMachine(ctl.base, lr, &ctl, ctl.base, lookup, out, nullptr, &lr.durCur, &lr.durNext);
			}
			// 2. Les COUCHES, dans l'ordre : chacune remplace (sous son masque) ou
			//    s'ajoute a ce qui est dessous.
			NkAnimPose couche, ref, delta;
			NkVector<float32> poidsOs;
			for (uint32 i = 0; i < (uint32)ctl.layers.Size() && i + 1 < NK_ANIM_MAX_LAYERS; ++i) {
				NkAnimLayer &L = ctl.layers[i];
				NkAnimLayerRuntime &lr = rt.layers[i + 1];
				CopierParams(ctl.base, L.machine);
				// Les declencheurs poses AVANT : ceux que la couche consomme partent aussi de la base.
				NkVector<uint32> poses;
				for (uint32 k = 0; k < L.machine.GetParamCount(); ++k) {
					if (L.machine.GetParamKind(k) == NkAnimStateMachine::NkParamKind::TRIGGER && L.machine.GetParamValue(k) > 0.5f) {
						poses.PushBack(k);
					}
				}
				L.machine.SetRuntime(lr.rt);
				L.machine.Update(dt);
				lr.rt = L.machine.GetRuntime();
				for (uint32 k = 0; k < (uint32)poses.Size(); ++k) {
					if (L.machine.GetParamValue(poses[k]) < 0.5f) {
						ctl.base.ResetTrigger(L.machine.GetParamName(poses[k]));
					}
				}
				AvancerPhases(lr, dt);
				float32 w = L.weight;
				if (!L.weightParam.Empty()) {
					w *= Clamp01(ValeurParam(ctl.base, L.weightParam));
				}
				const bool additif = L.mode == NkLayerMode::NK_ADDITIVE;
				EvaluerMachine(L.machine, lr, &ctl, ctl.base, lookup, couche, additif ? &ref : nullptr, &lr.durCur, &lr.durNext);
				if (w <= 0.f || couche.Empty()) {
					continue;
				}
				const NkAnimMask *mask = ctl.FindMask(L.mask);
				poidsOs.Clear();
				const NkAnimationClip *sq = out.skeleton != nullptr ? out.skeleton : couche.skeleton;
				if (mask != nullptr && sq != nullptr) {
					mask->BoneWeights(*sq, poidsOs);
				}
				if (additif) {
					NkMakeAdditive(couche, ref, delta);
					NkApplyAdditive(out, delta, w, poidsOs.Empty() ? nullptr : &poidsOs, mask);
				} else {
					NkBlendPose(out, couche, w, poidsOs.Empty() ? nullptr : &poidsOs, mask);
				}
			}
		}

	} // namespace anim
} // namespace nkentseu
