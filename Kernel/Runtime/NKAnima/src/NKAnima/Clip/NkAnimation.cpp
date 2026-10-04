// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKAnima/NkAnimation.cpp — voir NkAnimation.h
// -----------------------------------------------------------------------------
// Corps deplace TEL QUEL depuis NKRenderer/Tools/Animation/NkAnimationSystem.cpp
// le 2026-08-14. Seuls l'espace de noms et les inclusions ont change : plus rien
// ici ne tire le renderer, NKRHI ni un chargeur de format.
// =============================================================================
#include "NKAnima/Clip/NkAnimation.h"
#include "NKAnima/Skeleton/NkSkeletonDef.h" // LA conversion locale -> monde (2026-09-04)
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
			for (auto &p : propertyTracks)
				if (!p.curve.Empty())
					duration = fmaxf(duration, p.curve.GetDuration());
			// (01/10 soir) une sequence dure au moins jusqu'a la fin de son dernier clip.
			for (auto &tr : clipTracks)
				for (auto &st : tr.strips)
					duration = fmaxf(duration, st.End());
		}

		// ── (2026-10-01 soir) Fondus, tangentes, clips poses ────────────────────
		float32 NkFadeWeight(float32 u, NkFadeCurve curve) {
			u = u < 0.f ? 0.f : (u > 1.f ? 1.f : u);
			switch (curve) {
				case NkFadeCurve::NK_SMOOTH:
					return u * u * (3.f - 2.f * u);
				case NkFadeCurve::NK_EASE_IN:
					return u * u;
				case NkFadeCurve::NK_EASE_OUT:
					return 1.f - (1.f - u) * (1.f - u);
				default:
					return u;
			}
		}

		const char *NkFadeCurveName(NkFadeCurve curve) {
			switch (curve) {
				case NkFadeCurve::NK_SMOOTH:
					return "Douce";
				case NkFadeCurve::NK_EASE_IN:
					return "Entree";
				case NkFadeCurve::NK_EASE_OUT:
					return "Sortie";
				case NkFadeCurve::NK_LINEAR:
					return "Lineaire";
				default:
					return "?";
			}
		}

		float32 NkClipStrip::WeightAt(float32 t) const {
			if (muted || t < start || t > End() || length <= 0.f) {
				return 0.f;
			}
			float32 w = weight;
			if (blendIn > 1e-6f && t < start + blendIn) {
				w *= (t - start) / blendIn;
			}
			if (blendOut > 1e-6f && t > End() - blendOut) {
				w *= (End() - t) / blendOut;
			}
			return w < 0.f ? 0.f : w;
		}

		float32 NkClipStrip::LocalTime(float32 t, float32 sourceLength) const {
			float32 local = offset + (t - start) * rate;
			if (sourceLength <= 1e-6f) {
				return 0.f;
			}
			if (loop) {
				local = fmodf(local, sourceLength);
				return local < 0.f ? local + sourceLength : local;
			}
			return local < 0.f ? 0.f : (local > sourceLength ? sourceLength : local);
		}

		void NkAnimationClip::NkPropertyTrack::KeySlopes(uint32 k, uint32 c, float32 &in, float32 &out) const {
			in = out = 0.f;
			const uint32 n = curve.KeyCount();
			if (k >= n || c >= 4) {
				return;
			}
			if (k < (uint32)tangents.Size()) {
				const NkPropertyTangent &tg = tangents[k];
				if (tg.mode == NkTangentMode::NK_USER) {
					in = out = tg.out[c];
					return;
				}
				if (tg.mode == NkTangentMode::NK_BREAK) {
					in = tg.in[c];
					out = tg.out[c];
					return;
				}
				if (tg.mode == NkTangentMode::NK_FLAT) {
					return;
				}
			}
			// AUTO BORNEE (la meme que la frise du kit, NkTimelineModel::KeySlopes) :
			// plate aux extremites et sur un sommet ou un creux.
			if (k == 0 || k + 1 >= n) {
				return;
			}
			const NkKeyframe<NkVec4f> &p = curve.GetKey(k - 1);
			const NkKeyframe<NkVec4f> &m = curve.GetKey(k);
			const NkKeyframe<NkVec4f> &s = curve.GetKey(k + 1);
			const float32 dp = m.value[c] - p.value[c], ds = s.value[c] - m.value[c];
			if (dp * ds <= 0.f) {
				return;
			}
			const float32 dt = s.time - p.time;
			in = out = dt > 1e-6f ? (s.value[c] - p.value[c]) / dt : 0.f;
		}

		NkVec4f NkAnimationClip::NkPropertyTrack::Evaluate(float32 t) const {
			const uint32 n = curve.KeyCount();
			if (n < 2 || kind == NkPropertyKind::NK_STEP || t <= curve.GetKey(0).time || t >= curve.GetKey(n - 1).time) {
				return curve.Evaluate(t);
			}
			uint32 hi = 1;
			while (hi < n && curve.GetKey(hi).time <= t) {
				++hi;
			}
			const NkKeyframe<NkVec4f> &a = curve.GetKey(hi - 1);
			if (a.interp != NkInterpMode::NK_CUBIC) {
				return curve.Evaluate(t);
			}
			// Hermite cubique : la « Courbe » des frises, sur les pentes des cles.
			const NkKeyframe<NkVec4f> &b = curve.GetKey(hi);
			const float32 d = b.time - a.time;
			const float32 u = d > 1e-6f ? (t - a.time) / d : 0.f;
			const float32 u2 = u * u, u3 = u2 * u;
			const float32 h00 = 2.f * u3 - 3.f * u2 + 1.f, h10 = u3 - 2.f * u2 + u;
			const float32 h01 = -2.f * u3 + 3.f * u2, h11 = u3 - u2;
			NkVec4f r;
			for (uint32 c = 0; c < 4; ++c) {
				float32 inA, outA, inB, outB;
				KeySlopes(hi - 1, c, inA, outA);
				KeySlopes(hi, c, inB, outB);
				r[c] = h00 * a.value[c] + h10 * d * outA + h01 * b.value[c] + h11 * d * inB;
			}
			return r;
		}

		NkAnimationClip::NkPropertyTrack *NkAnimationClip::FindPropertyTrack(const NkString &target,
																			  const NkString &property) {
			for (uint32 i = 0; i < (uint32)propertyTracks.Size(); ++i) {
				if (propertyTracks[i].target == target && propertyTracks[i].property == property) {
					return &propertyTracks[i];
				}
			}
			return nullptr;
		}

		NkAnimationClip::NkPropertyTrack &NkAnimationClip::AddPropertyTrack(const NkString &target,
																			 const NkString &property, NkPropertyKind kind) {
			if (NkPropertyTrack *p = FindPropertyTrack(target, property)) {
				return *p;
			}
			NkPropertyTrack t;
			t.target = target;
			t.property = property;
			t.kind = kind;
			t.curve.name = property;
			propertyTracks.PushBack(t);
			return propertyTracks[propertyTracks.Size() - 1];
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
		//
		// ⚠️ Un CLIP s'écrit toujours en v2, octet pour octet comme avant : un
		// moteur d'avant le 2026-09-29 continue de lire les clips qu'on écrit.
		//
		// LA MACHINE À ÉTATS N'EST PAS UN CLIP, ET N'EST PLUS UN .nkanim
		// (décision de Rihen du 2026-09-30). Elle s'écrit dans SON format,
		// `.nkanimctl` — « contrôleur d'animation » : l'extension dit l'usage.
		// Règle de CONVENTIONS_FICHIERS.md §1 : deux fichiers qu'on dépose au même
		// endroit avec le même effet partagent leur extension, sinon ils en
		// changent. Le fichier se reconnaît à son MAGIC, 'NKAC', pas à son nom :
		//   [magic 'NKAC'(u32)] [version(u32)=1] [nbSections(u32)]
		//     par section : [étiquette(u32)] [taille(u32)] [contenu]
		// La section 'HFSM' porte la machine (voir NkAnimStateMachine::SaveToBytes).
		//
		// Le .nkanim v3 (29/09 → 30/09) : un corps de clip VIDE puis les mêmes
		// sections. C'est ainsi que la machine s'écrivait pendant un jour ; la
		// machine le RELIT toujours, le clip le REFUSE en le nommant.
		// ⚠️ Ni .nkanim ni .nkanimctl ne passent par NkAssetFileHeader : NKAnima
		// ne tire pas NKSerialization. La correspondance type <-> extension, elle,
		// est dans NKSerialization (NkAssetType::Animation / AnimationController,
		// NkAssetExtensionFor) — et nulle part ici.
		// =====================================================================
		namespace {
			constexpr uint32 kNkAnimMagic = 0x4E414B4E; // 'NKAN' (little-endian)
			constexpr uint32 kNkAnimVersion = 2;		// v2 : + section squelette (mode local) — version ÉCRITE pour un clip
			constexpr uint32 kNkAnimVersionSections = 3; // v3 : corps v2 + sections (machine, 29/09 -> 30/09), relu par la machine
			constexpr uint32 kNkAnimCtlMagic = 0x43414B4E;	 // 'NKAC' (little-endian) : .nkanimctl, contrôleur d'animation
			constexpr uint32 kNkAnimCtlVersion = 1;
			constexpr uint32 kNkAnimSectionHFSM = 0x4D534648; // 'HFSM' (little-endian)
			// (2026-10-01) Un clip a pistes de PROPRIETES : corps v2 + sections, dont
			// 'PROP'. Le numero 3 reste celui de la machine du 29/09 : un clip ne le
			// reprend pas, sinon un lecteur d'hier le prendrait pour une machine.
			constexpr uint32 kNkAnimVersionProprietes = 4;
			constexpr uint32 kNkAnimSectionPROP = 0x504F5250; // 'PROP' (little-endian)
			// (2026-10-01 soir) Les tangentes posees a la main des pistes de propriete,
			// les pistes de CLIPS (NLA), les courbes des fondus de transition.
			constexpr uint32 kNkAnimSectionTANG = 0x474E4154; // 'TANG'
			constexpr uint32 kNkAnimSectionCLPS = 0x53504C43; // 'CLPS'
			constexpr uint32 kNkAnimSectionFADE = 0x45444146; // 'FADE'
			// (2026-10-02, rig 3D) Les pistes de FORMES (shape keys) : une par forme, par NOM.
			constexpr uint32 kNkAnimSectionMRPH = 0x4850524D; // 'MRPH'
			// La disposition de la machine dans l'editeur (.nkanimctl, page Animateur).
			constexpr uint32 kNkAnimSectionGRPH = 0x48505247; // 'GRPH' (little-endian)

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

			// Le CORPS d'un clip : tout ce qui suit [magic][version]. Extrait de
			// SaveBinary / LoadBinary le 2026-09-29, sans changer un octet. La
			// machine le SAUTE encore quand elle relit un .nkanim v3.
			void WriteClipBody(ByteWriter &w, const NkAnimationClip &c) {
				w.str(c.name);
				w.f32(c.duration);
				w.f32(c.fps);
				w.u8(c.loop ? 1 : 0);
				w.u32((uint32)c.boneTracks.Size());
				for (uint32 b = 0; b < (uint32)c.boneTracks.Size(); ++b) {
					const NkAnimationTrack<NkMat4f> &tr = c.boneTracks[b];
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
				w.u8(c.skeletalLocal ? 1 : 0);
				w.u32((uint32)c.jointParent.Size());
				for (uint32 j = 0; j < (uint32)c.jointParent.Size(); ++j)
					w.i32(c.jointParent[j]);
				w.u32((uint32)c.jointInverseBind.Size());
				for (uint32 j = 0; j < (uint32)c.jointInverseBind.Size(); ++j)
					w.mat(c.jointInverseBind[j]);
				w.u32((uint32)c.jointTopo.Size());
				for (uint32 j = 0; j < (uint32)c.jointTopo.Size(); ++j)
					w.u32(c.jointTopo[j]);
			}

			// Lit le corps d'un clip de version `ver` (1, 2 ou 3). Rend le nombre
			// d'os lus ; l'appelant juge `r.ok`.
			uint32 ReadClipBody(ByteReader &r, uint32 ver, NkAnimationClip &c) {
				// ⚠️ Tout COMPTE lu du fichier est borne par ce qui reste a lire
				// (`taille` = octets minimum d'un element). Sans cette borne, un
				// fichier etranger ou abime annoncait des milliards d'os : Resize
				// geant, puis une boucle sans fin — mesure du 30/09, un .nkanimctl
				// mal etiquete lu comme clip bloquait le banc vingt minutes. Un
				// fichier valide n'est jamais touche par la borne.
				auto borne = [&r](uint32 compte, usize taille) -> uint32 {
					if (!r.ok || compte > (r.n - r.off) / taille) {
						r.ok = false;
						return 0u;
					}
					return compte;
				};
				c.name = r.str();
				c.duration = r.f32();
				c.fps = r.f32();
				c.loop = (r.u8() != 0);
				uint32 nb = borne(r.u32(), 9); // nom vide (4) + drapeau (1) + nombre de cles (4)
				c.boneTracks.Clear();
				c.boneTracks.Resize(nb);
				c.boneCount = nb;
				for (uint32 b = 0; b < nb; ++b) {
					NkAnimationTrack<NkMat4f> &tr = c.boneTracks[b];
					tr.name = r.str();
					tr.enabled = (r.u8() != 0);
					uint32 nk = borne(r.u32(), 4 + 16 * sizeof(float32) + 1); // temps + matrice + interp
					for (uint32 k = 0; k < nk; ++k) {
						float32 t = r.f32();
						NkMat4f m = r.mat();
						uint8 interp = r.u8();
						tr.AddKey(t, m, (NkInterpMode)interp);
					}
				}
				// Section squelette (v2+).
				c.skeletalLocal = false;
				c.jointParent.Clear();
				c.jointInverseBind.Clear();
				c.jointTopo.Clear();
				if (ver >= 2) {
					c.skeletalLocal = (r.u8() != 0);
					uint32 np = borne(r.u32(), 4);
					c.jointParent.Resize(np);
					for (uint32 j = 0; j < np; ++j)
						c.jointParent[j] = r.i32();
					uint32 ni = borne(r.u32(), 16 * sizeof(float32));
					c.jointInverseBind.Resize(ni);
					for (uint32 j = 0; j < ni; ++j)
						c.jointInverseBind[j] = r.mat();
					uint32 nt = borne(r.u32(), 4);
					c.jointTopo.Resize(nt);
					for (uint32 j = 0; j < nt; ++j)
						c.jointTopo[j] = r.u32();
				}
				// (R30, 02/10) Le fichier n'ecrit pas jointNames, mais chaque piste d'os
				// porte le NOM de son os : les noms du squelette en reviennent (les
				// masques par os, le reciblage, la frise et le squelette 2D d'Unkeny
				// retrouvent leurs os par nom). Un clip sans noms reste sans noms.
				c.jointNames.Clear();
				bool nomme = false;
				for (uint32 b = 0; b < nb; ++b)
					nomme = nomme || !c.boneTracks[b].name.Empty();
				if (nomme) {
					c.jointNames.Resize(nb);
					for (uint32 b = 0; b < nb; ++b)
						c.jointNames[b] = c.boneTracks[b].name;
				}
				return nb;
			}
		} // namespace

		bool NkAnimationClip::SaveBinary(const NkString &path) const {
			ByteWriter w;
			w.u32(kNkAnimMagic);
			// Sans piste de propriete : v2, OCTET POUR OCTET comme avant le 01/10 --
			// un moteur d'hier relit tout clip qu'il savait deja lire.
			const bool proprietes = !propertyTracks.Empty();
			// (01/10 soir) Des tangentes posees a la main, des pistes de clips :
			// deux sections de plus, seulement si elles ont quelque chose a dire.
			bool tangentes = false;
			for (uint32 i = 0; i < (uint32)propertyTracks.Size() && !tangentes; ++i) {
				for (uint32 k = 0; k < (uint32)propertyTracks[i].tangents.Size(); ++k) {
					tangentes = tangentes || propertyTracks[i].tangents[k].mode != NkTangentMode::NK_AUTO;
				}
			}
			const bool sequence = !clipTracks.Empty();
			// (02/10, rig 3D) Les pistes de FORMES : une section de plus, seulement s'il y en a.
			const bool formes = !morphTracks.Empty();
			const uint32 nbSections = (proprietes ? 1u : 0u) + (tangentes ? 1u : 0u) + (sequence ? 1u : 0u) + (formes ? 1u : 0u);
			w.u32(nbSections > 0 ? kNkAnimVersionProprietes : kNkAnimVersion);
			WriteClipBody(w, *this);
			if (nbSections > 0) {
				w.u32(nbSections);
			}
			if (proprietes) {
				// [nbSections] puis 'PROP' : [version(u32)=1] [nbPistes(u32)]
				//   par piste : [cible] [propriete] [genre(u8)] [active(u8)] [nbCles(u32)]
				//     par cle : [temps(f32)] [x y z w (4*f32)] [interp(u8)]
				ByteWriter sct;
				sct.u32(1);
				sct.u32((uint32)propertyTracks.Size());
				for (uint32 i = 0; i < (uint32)propertyTracks.Size(); ++i) {
					const NkPropertyTrack &p = propertyTracks[i];
					sct.str(p.target);
					sct.str(p.property);
					sct.u8((uint8)p.kind);
					sct.u8(p.curve.enabled ? 1 : 0);
					sct.u32(p.curve.KeyCount());
					for (uint32 k = 0; k < p.curve.KeyCount(); ++k) {
						const NkKeyframe<NkVec4f> &kf = p.curve.GetKey(k);
						sct.f32(kf.time);
						sct.f32(kf.value.x);
						sct.f32(kf.value.y);
						sct.f32(kf.value.z);
						sct.f32(kf.value.w);
						sct.u8((uint8)kf.interp);
					}
				}
				w.u32(kNkAnimSectionPROP);
				w.u32((uint32)sct.buf.Size());
				w.raw(sct.buf.Data(), sct.buf.Size());
			}
			if (tangentes) {
				// 'TANG' : [version(u32)=1] [nbPistes(u32)] par piste : [nbCles(u32)]
				//   par cle : [mode(u8)] et, si USER ou BREAK : [entree xyzw] [sortie xyzw]
				ByteWriter sct;
				sct.u32(1);
				sct.u32((uint32)propertyTracks.Size());
				for (uint32 i = 0; i < (uint32)propertyTracks.Size(); ++i) {
					const NkPropertyTrack &p = propertyTracks[i];
					sct.u32(p.curve.KeyCount());
					for (uint32 k = 0; k < p.curve.KeyCount(); ++k) {
						const NkPropertyTangent tg = k < (uint32)p.tangents.Size() ? p.tangents[k] : NkPropertyTangent();
						sct.u8((uint8)tg.mode);
						if (tg.mode == NkTangentMode::NK_USER || tg.mode == NkTangentMode::NK_BREAK) {
							for (uint32 c = 0; c < 4; ++c) {
								sct.f32(tg.in[c]);
							}
							for (uint32 c = 0; c < 4; ++c) {
								sct.f32(tg.out[c]);
							}
						}
					}
				}
				w.u32(kNkAnimSectionTANG);
				w.u32((uint32)sct.buf.Size());
				w.raw(sct.buf.Data(), sct.buf.Size());
			}
			if (sequence) {
				// 'CLPS' : [version(u32)=1] [nbPistes(u32)] par piste : [nom] [poids(f32)]
				//   [additif(u8)] [muette(u8)] [masque] [nbClips(u32)] par clip : [clip]
				//   [debut] [duree] [decalage] [vitesse] [fondu entree] [fondu sortie]
				//   [poids] (f32) [boucle(u8)] [muet(u8)]
				ByteWriter sct;
				sct.u32(1);
				sct.u32((uint32)clipTracks.Size());
				for (uint32 i = 0; i < (uint32)clipTracks.Size(); ++i) {
					const NkClipStripTrack &tr = clipTracks[i];
					sct.str(tr.name);
					sct.f32(tr.weight);
					sct.u8(tr.additive ? 1 : 0);
					sct.u8(tr.muted ? 1 : 0);
					sct.str(tr.mask);
					sct.u32((uint32)tr.strips.Size());
					for (uint32 k = 0; k < (uint32)tr.strips.Size(); ++k) {
						const NkClipStrip &st = tr.strips[k];
						sct.str(st.clip);
						sct.f32(st.start);
						sct.f32(st.length);
						sct.f32(st.offset);
						sct.f32(st.rate);
						sct.f32(st.blendIn);
						sct.f32(st.blendOut);
						sct.f32(st.weight);
						sct.u8(st.loop ? 1 : 0);
						sct.u8(st.muted ? 1 : 0);
					}
				}
				w.u32(kNkAnimSectionCLPS);
				w.u32((uint32)sct.buf.Size());
				w.raw(sct.buf.Data(), sct.buf.Size());
			}
			if (formes) {
				// 'MRPH' : [version(u32)=1] [nbPistes(u32)] par piste : [nom] [active(u8)]
				//   [nbCles(u32)] par cle : [temps(f32)] [valeur(f32)] [interp(u8)]
				ByteWriter sct;
				sct.u32(1);
				sct.u32((uint32)morphTracks.Size());
				for (uint32 i = 0; i < (uint32)morphTracks.Size(); ++i) {
					const NkAnimationTrack<float32> &tr = morphTracks[i];
					sct.str(i < (uint32)morphNames.Size() ? morphNames[i] : tr.name);
					sct.u8(tr.enabled ? 1 : 0);
					sct.u32(tr.KeyCount());
					for (uint32 k = 0; k < tr.KeyCount(); ++k) {
						const NkKeyframe<float32> &kf = tr.GetKey(k);
						sct.f32(kf.time);
						sct.f32(kf.value);
						sct.u8((uint8)kf.interp);
					}
				}
				w.u32(kNkAnimSectionMRPH);
				w.u32((uint32)sct.buf.Size());
				w.raw(sct.buf.Data(), sct.buf.Size());
			}
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
			// Un fichier d'une AUTRE nature se refuse en la NOMMANT : « magic
			// invalide » enverrait chercher une corruption qui n'existe pas.
			if (magic == kNkAnimCtlMagic) {
				logger.Errorf("[NkAnimClip] %s est un CONTROLEUR d'animation (.nkanimctl, machine a etats), pas un "
							  "clip : NkAnimStateMachine::LoadBinary le lit\n",
							  path.CStr());
				return false;
			}
			if (magic != kNkAnimMagic) {
				logger.Errorf("[NkAnimClip] magic invalide (0x%08X) : %s\n", magic, path.CStr());
				return false;
			}
			if (ver == kNkAnimVersionSections) {
				logger.Errorf("[NkAnimClip] %s est une machine a etats ecrite en .nkanim v3 (format du 29/09, remplace "
							  "par .nkanimctl), pas un clip : NkAnimStateMachine::LoadBinary le lit\n",
							  path.CStr());
				return false;
			}
			if (ver < 1 || (ver > kNkAnimVersion && ver != kNkAnimVersionProprietes)) {
				logger.Errorf("[NkAnimClip] version %u non supportee : %s\n", ver, path.CStr());
				return false;
			}
			const uint32 nb = ReadClipBody(r, ver, *this);
			propertyTracks.Clear();
			clipTracks.Clear();
			morphTracks.Clear();
			morphNames.Clear();
			// 'TANG' peut preceder ou suivre 'PROP' : appliquee apres la boucle.
			const nk_uint8 *tang = nullptr;
			uint32 tangTaille = 0;
			if (r.ok && ver == kNkAnimVersionProprietes) {
				// Les sections : 'PROP' lue, toute autre SAUTEE grace a sa taille.
				const uint32 nbSections = r.u32();
				for (uint32 sc = 0; sc < nbSections && r.ok; ++sc) {
					const uint32 tag = r.u32();
					const uint32 taille = r.u32();
					if (!r.need(taille)) {
						break;
					}
					ByteReader s(r.p + r.off, taille);
					r.off += taille;
					if (tag == kNkAnimSectionTANG) {
						tang = s.p;
						tangTaille = taille;
						continue;
					}
					if (tag == kNkAnimSectionMRPH) {
						// (02/10, rig 3D) Les pistes de formes, par nom.
						if (s.u32() != 1) {
							continue; // une version future : sautee
						}
						uint32 nt = s.u32();
						if (nt > (s.n - s.off) / 9u) {
							nt = 0;
						}
						for (uint32 i = 0; i < nt && s.ok; ++i) {
							NkAnimationTrack<float32> tr;
							tr.name = s.str();
							tr.enabled = s.u8() != 0;
							uint32 nk = s.u32();
							if (nk > (s.n - s.off) / 9u) {
								nk = 0;
								s.ok = false;
							}
							for (uint32 k = 0; k < nk && s.ok; ++k) {
								const float32 t = s.f32();
								const float32 v = s.f32();
								const uint8 interp = s.u8();
								tr.AddKey(t, v, (NkInterpMode)(interp <= (uint8)NkInterpMode::NK_BACK ? interp : 1u));
							}
							if (s.ok) {
								morphNames.PushBack(tr.name);
								morphTracks.PushBack(tr);
							}
						}
						continue;
					}
					if (tag == kNkAnimSectionCLPS) {
						if (s.u32() != 1) {
							continue; // une version future : sautee, le clip reste lisible
						}
						uint32 nt = s.u32();
						if (nt > (s.n - s.off) / 18u) {
							nt = 0;
						}
						for (uint32 i = 0; i < nt && s.ok; ++i) {
							NkClipStripTrack tr;
							tr.name = s.str();
							tr.weight = s.f32();
							tr.additive = s.u8() != 0;
							tr.muted = s.u8() != 0;
							tr.mask = s.str();
							uint32 ns = s.u32();
							if (ns > (s.n - s.off) / 34u) {
								ns = 0;
							}
							for (uint32 k = 0; k < ns && s.ok; ++k) {
								NkClipStrip st;
								st.clip = s.str();
								st.start = s.f32();
								st.length = s.f32();
								st.offset = s.f32();
								st.rate = s.f32();
								st.blendIn = s.f32();
								st.blendOut = s.f32();
								st.weight = s.f32();
								st.loop = s.u8() != 0;
								st.muted = s.u8() != 0;
								tr.strips.PushBack(st);
							}
							if (s.ok) {
								clipTracks.PushBack(tr);
							}
						}
						continue;
					}
					if (tag != kNkAnimSectionPROP) {
						continue;
					}
					if (s.u32() != 1) {
						logger.Errorf("[NkAnimClip] section PROP d'une version inconnue : %s\n", path.CStr());
						return false;
					}
					// Meme borne que ReadClipBody : un compte ne depasse jamais ce qui
					// reste a lire (14 o = deux noms vides, genre, drapeau, nombre de cles).
					uint32 np = s.u32();
					if (np > (s.n - s.off) / 14u) {
						s.ok = false;
						np = 0;
					}
					for (uint32 i = 0; i < np && s.ok; ++i) {
						NkPropertyTrack p;
						p.target = s.str();
						p.property = s.str();
						const uint8 genre = s.u8();
						p.kind = genre <= (uint8)NkPropertyKind::NK_STEP ? (NkPropertyKind)genre : NkPropertyKind::NK_NUMBER;
						p.curve.enabled = s.u8() != 0;
						p.curve.name = p.property;
						uint32 nk = s.u32();
						if (nk > (s.n - s.off) / 21u) { // temps + 4 flottants + interp
							s.ok = false;
							nk = 0;
						}
						for (uint32 k = 0; k < nk && s.ok; ++k) {
							const float32 t = s.f32();
							NkVec4f v;
							v.x = s.f32();
							v.y = s.f32();
							v.z = s.f32();
							v.w = s.f32();
							const uint8 interp = s.u8();
							p.curve.AddKey(t, v, (NkInterpMode)(interp <= (uint8)NkInterpMode::NK_BACK ? interp : 1u));
						}
						propertyTracks.PushBack(p);
					}
					if (!s.ok) {
						r.ok = false;
					}
				}
			}
			if (tang != nullptr) {
				ByteReader s(tang, tangTaille);
				if (s.u32() == 1u) {
					const uint32 np = s.u32();
					for (uint32 i = 0; i < np && s.ok && i < (uint32)propertyTracks.Size(); ++i) {
						NkPropertyTrack &p = propertyTracks[i];
						const uint32 nk = s.u32();
						p.tangents.Clear();
						for (uint32 k = 0; k < nk && s.ok; ++k) {
							NkPropertyTangent tg;
							const uint8 mode = s.u8();
							tg.mode = mode <= (uint8)NkTangentMode::NK_FLAT ? (NkTangentMode)mode : NkTangentMode::NK_AUTO;
							if (mode == (uint8)NkTangentMode::NK_USER || mode == (uint8)NkTangentMode::NK_BREAK) {
								for (uint32 c = 0; c < 4; ++c) {
									tg.in[c] = s.f32();
								}
								for (uint32 c = 0; c < 4; ++c) {
									tg.out[c] = s.f32();
								}
							}
							p.tangents.PushBack(tg);
						}
						if (p.tangents.Size() != p.curve.KeyCount()) {
							p.tangents.Clear(); // incoherent : toutes automatiques, sans casser le clip
						}
					}
				}
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
			// -- UNE seule conversion, dans Skeleton/ (2026-09-04). Les joints au-dela
			// de boneMats valent l'identite, comme avant.
			for (uint32 j = (uint32)boneMats.Size(); j < (uint32)global.Size(); ++j)
				global[j] = NkMat4f::Identity();
			for (uint32 j = 0; j < (uint32)boneMats.Size() && j < (uint32)global.Size(); ++j)
				global[j] = boneMats[j];
			::nkentseu::anim::NkForwardKinematics(jointParent.Data(), jointTopo.Data(), n, global.Data(), global.Data());
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

		// (R30, 02/10) Un os SANS CLE d'un clip local reste a son REPOS, derive des
		// inverses de repos du clip : local = inverseRepos(parent) x repos(os). Il
		// valait l'identite (l'os s'effondrait sur son parent) -- le meme repli que
		// NkSampleClip (Blend/NkAnimMix.cpp), pour que le lecteur et le melange
		// jouent la meme pose.
		static NkMat4f ReposLocal(const NkAnimationClip *clip, uint32 i) {
			if (!clip->skeletalLocal || i >= (uint32)clip->jointInverseBind.Size()) {
				return NkMat4f::Identity();
			}
			const int32 p = i < (uint32)clip->jointParent.Size() ? clip->jointParent[i] : -1;
			const NkMat4f monde = clip->jointInverseBind[i].Inverse();
			return (p >= 0 && (uint32)p < (uint32)clip->jointInverseBind.Size()) ? clip->jointInverseBind[(uint32)p] * monde : monde;
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
											: ReposLocal(clip, i);
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
								  : ReposLocal(clip, i);
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
		// -------------------------------------------------------------------------
		// HIERARCHIQUE depuis le 2026-09-29. Jusque-la, la doc disait « HFSM » et
		// le code etait PLAT : un niveau, trois conditions, aucune sous-machine.
		// Les ajouts ne touchent pas le chemin plat : un etat de la racine, une
		// transition a une condition et de priorite 0 donnent les memes choix,
		// dans le meme ordre, que l'ancien Update (voir PickTransition).
		// =========================================================================

		// ── Parametres ───────────────────────────────────────────────────────────
		NkVector<NkAnimStateMachine::Param> &NkAnimStateMachine::Params() {
			return mParamOwner ? mParamOwner->mParams : mParams;
		}

		const NkVector<NkAnimStateMachine::Param> &NkAnimStateMachine::Params() const {
			return mParamOwner ? mParamOwner->mParams : mParams;
		}

		NkAnimStateMachine::Param *NkAnimStateMachine::FindParamPtr(const NkString &name, NkParamKind kind) {
			NkVector<Param> &ps = Params();
			for (uint32 i = 0; i < (uint32)ps.Size(); ++i) {
				if (ps[i].kind == kind && ps[i].name == name) {
					return &ps[i];
				}
			}
			return nullptr;
		}

		const NkAnimStateMachine::Param *NkAnimStateMachine::FindParamPtr(const NkString &name, NkParamKind kind) const {
			const NkVector<Param> &ps = Params();
			for (uint32 i = 0; i < (uint32)ps.Size(); ++i) {
				if (ps[i].kind == kind && ps[i].name == name) {
					return &ps[i];
				}
			}
			return nullptr;
		}

		NkAnimStateMachine::Param &NkAnimStateMachine::DeclareParamRef(const NkString &name, NkParamKind kind) {
			if (Param *p = FindParamPtr(name, kind)) {
				return *p;
			}
			Param p;
			p.name = name;
			p.kind = kind;
			Params().PushBack(p);
			return Params()[Params().Size() - 1];
		}

		void NkAnimStateMachine::DeclareParam(const NkString &name, NkParamKind kind, float32 defaultValue) {
			Param &p = DeclareParamRef(name, kind);
			p.defaultValue = defaultValue;
			p.value = defaultValue;
		}

		void NkAnimStateMachine::SetBool(const NkString &name, bool v) {
			DeclareParamRef(name, NkParamKind::BOOL).value = v ? 1.f : 0.f;
		}

		void NkAnimStateMachine::SetFloat(const NkString &name, float32 v) {
			DeclareParamRef(name, NkParamKind::FLOAT).value = v;
		}

		float32 NkAnimStateMachine::GetFloat(const NkString &name) const {
			const Param *p = FindParamPtr(name, NkParamKind::FLOAT);
			return p ? p->value : 0.f;
		}

		bool NkAnimStateMachine::GetBool(const NkString &name) const {
			const Param *p = FindParamPtr(name, NkParamKind::BOOL);
			return p && p->value != 0.f;
		}

		void NkAnimStateMachine::SetTrigger(const NkString &name) {
			DeclareParamRef(name, NkParamKind::TRIGGER).value = 1.f;
		}

		void NkAnimStateMachine::ResetTrigger(const NkString &name) {
			if (Param *p = FindParamPtr(name, NkParamKind::TRIGGER)) {
				p->value = 0.f;
			}
		}

		bool NkAnimStateMachine::GetTrigger(const NkString &name) const {
			const Param *p = FindParamPtr(name, NkParamKind::TRIGGER);
			return p && p->value != 0.f;
		}

		uint32 NkAnimStateMachine::GetParamCount() const {
			return (uint32)Params().Size();
		}

		const NkString &NkAnimStateMachine::GetParamName(uint32 i) const {
			static NkString sEmpty;
			return i < (uint32)Params().Size() ? Params()[i].name : sEmpty;
		}

		NkAnimStateMachine::NkParamKind NkAnimStateMachine::GetParamKind(uint32 i) const {
			return i < (uint32)Params().Size() ? Params()[i].kind : NkParamKind::FLOAT;
		}

		float32 NkAnimStateMachine::GetParamValue(uint32 i) const {
			return i < (uint32)Params().Size() ? Params()[i].value : 0.f;
		}

		void NkAnimStateMachine::SetParamValue(uint32 i, float32 v) {
			if (i < (uint32)Params().Size()) {
				Params()[i].value = v;
			}
		}

		int32 NkAnimStateMachine::FindParam(const NkString &name, NkParamKind kind) const {
			const NkVector<Param> &ps = Params();
			for (uint32 i = 0; i < (uint32)ps.Size(); ++i) {
				if (ps[i].kind == kind && ps[i].name == name) {
					return (int32)i;
				}
			}
			return -1;
		}

		void NkAnimStateMachine::ResetParams() {
			NkVector<Param> &ps = Params();
			for (uint32 i = 0; i < (uint32)ps.Size(); ++i) {
				ps[i].value = ps[i].defaultValue;
			}
		}

		bool NkAnimStateMachine::ShareParametersWith(NkAnimStateMachine *owner) {
			if (owner == nullptr) {
				mParamOwner = nullptr;
				return true;
			}
			// On remonte au VRAI proprietaire : une chaine a -> b -> c lirait
			// sinon les parametres de b, qui ne sont plus les siens.
			NkAnimStateMachine *root = owner->mParamOwner ? owner->mParamOwner : owner;
			if (root == this) {
				logger.Errorf("[NkAnimSM] ShareParametersWith refuse : cycle de partage\n");
				return false;
			}
			mParamOwner = root;
			return true;
		}

		// ── Structure ────────────────────────────────────────────────────────────
		bool NkAnimStateMachine::ValidParent(int32 parent) const {
			if (parent == NK_ROOT) {
				return true;
			}
			return parent >= 0 && parent < (int32)mStates.Size() && mStates[(uint32)parent].composite;
		}

		int32 NkAnimStateMachine::AddStateImpl(int32 parent, const NkString &name, bool composite) {
			if (!ValidParent(parent)) {
				logger.Errorf("[NkAnimSM] '%s' refuse : le parent %d n'est pas une sous-machine\n", name.CStr(), parent);
				return -1;
			}
			const int32 depth = (parent == NK_ROOT) ? 0 : mStates[(uint32)parent].depth + 1;
			if (depth >= NK_MAX_DEPTH) {
				logger.Errorf("[NkAnimSM] '%s' refuse : profondeur %d > NK_MAX_DEPTH\n", name.CStr(), depth);
				return -1;
			}
			State s;
			s.name = name;
			s.parent = parent;
			s.depth = depth;
			s.composite = composite;
			mStates.PushBack(s);
			const int32 idx = (int32)mStates.Size() - 1;
			// Tant que rien n'a demarre, la feuille courante SUIT la definition :
			// c'est ce que faisait l'ancien « premier AddState = etat courant »,
			// generalise a une racine dont le premier etat est une sous-machine.
			if (!mStarted) {
				mCurrent = ResolveLeaf(NK_ROOT);
			}
			return idx;
		}

		int32 NkAnimStateMachine::AddState(const NkString &name, const NkAnimationClip *clip) {
			return AddState(NK_ROOT, name, clip);
		}

		int32 NkAnimStateMachine::AddState(const NkString &name, NkBlendTree1D *tree) {
			return AddState(NK_ROOT, name, tree);
		}

		int32 NkAnimStateMachine::AddState(const NkString &name, NkBlendTree2D *tree2d) {
			return AddState(NK_ROOT, name, tree2d);
		}

		int32 NkAnimStateMachine::AddState(int32 parent, const NkString &name, const NkAnimationClip *clip) {
			const int32 idx = AddStateImpl(parent, name, false);
			if (idx >= 0) {
				mStates[(uint32)idx].clip = clip;
				mStates[(uint32)idx].refKind = 1;
				mStates[(uint32)idx].ref = clip ? clip->name : NkString();
			}
			return idx;
		}

		int32 NkAnimStateMachine::AddState(int32 parent, const NkString &name, NkBlendTree1D *tree) {
			const int32 idx = AddStateImpl(parent, name, false);
			if (idx >= 0) {
				mStates[(uint32)idx].tree = tree;
				mStates[(uint32)idx].refKind = 2;
				mStates[(uint32)idx].ref = tree ? tree->name : NkString();
			}
			return idx;
		}

		int32 NkAnimStateMachine::AddState(int32 parent, const NkString &name, NkBlendTree2D *tree2d) {
			const int32 idx = AddStateImpl(parent, name, false);
			if (idx >= 0) {
				mStates[(uint32)idx].tree2d = tree2d;
				mStates[(uint32)idx].refKind = 3;
				mStates[(uint32)idx].ref = tree2d ? tree2d->name : NkString();
			}
			return idx;
		}

		int32 NkAnimStateMachine::AddEmptyState(const NkString &name, int32 parent) {
			return AddStateImpl(parent, name, false);
		}

		int32 NkAnimStateMachine::AddSubMachine(const NkString &name, int32 parent) {
			return AddStateImpl(parent, name, true);
		}

		bool NkAnimStateMachine::SetEntryState(int32 machine, int32 state) {
			if (!ValidParent(machine) || state < 0 || state >= (int32)mStates.Size() ||
				mStates[(uint32)state].parent != machine) {
				logger.Errorf("[NkAnimSM] SetEntryState refuse : %d n'est pas un enfant direct de %d\n", state, machine);
				return false;
			}
			if (machine == NK_ROOT) {
				mRootEntry = state;
			} else {
				mStates[(uint32)machine].entry = state;
			}
			if (!mStarted) {
				mCurrent = ResolveLeaf(NK_ROOT);
			}
			return true;
		}

		int32 NkAnimStateMachine::EntryOf(int32 machine) const {
			const int32 explicitEntry = (machine == NK_ROOT)
											? mRootEntry
											: ((machine >= 0 && machine < (int32)mStates.Size()) ? mStates[(uint32)machine].entry : -1);
			if (explicitEntry >= 0 && explicitEntry < (int32)mStates.Size() &&
				mStates[(uint32)explicitEntry].parent == machine) {
				return explicitEntry;
			}
			for (uint32 i = 0; i < (uint32)mStates.Size(); ++i) {
				if (mStates[i].parent == machine) {
					return (int32)i;
				}
			}
			return -1;
		}

		int32 NkAnimStateMachine::GetEntryState(int32 machine) const {
			return ValidParent(machine) ? EntryOf(machine) : -1;
		}

		int32 NkAnimStateMachine::ResolveLeaf(int32 state) const {
			int32 s = (state == NK_ROOT) ? EntryOf(NK_ROOT) : state;
			// La descente est bornee par la profondeur : une definition qu'on
			// aurait corrompue (entree qui remonte) ne peut pas boucler.
			for (int32 guard = 0; guard <= NK_MAX_DEPTH; ++guard) {
				if (s < 0 || s >= (int32)mStates.Size() || !mStates[(uint32)s].composite) {
					return s;
				}
				const int32 e = EntryOf(s);
				if (e < 0) {
					return s; // sous-machine vide : elle tient lieu d'etat vide
				}
				s = e;
			}
			return s;
		}

		void NkAnimStateMachine::SetStateTag(int32 state, int32 tag) {
			if (state >= 0 && state < (int32)mStates.Size()) {
				mStates[(uint32)state].tag = tag;
			}
		}

		int32 NkAnimStateMachine::GetStateTag(int32 state) const {
			return (state >= 0 && state < (int32)mStates.Size()) ? mStates[(uint32)state].tag : 0;
		}

		const NkString &NkAnimStateMachine::GetStateName(int32 state) const {
			static NkString sEmpty;
			return (state >= 0 && state < (int32)mStates.Size()) ? mStates[(uint32)state].name : sEmpty;
		}

		int32 NkAnimStateMachine::GetStateParent(int32 state) const {
			return (state >= 0 && state < (int32)mStates.Size()) ? mStates[(uint32)state].parent : NK_ROOT;
		}

		int32 NkAnimStateMachine::GetStateDepth(int32 state) const {
			return (state >= 0 && state < (int32)mStates.Size()) ? mStates[(uint32)state].depth : -1;
		}

		// ── Lecture complete pour un editeur (2026-10-01) ────────────────────────
		uint8 NkAnimStateMachine::GetStateRefKind(int32 state) const {
			return (state >= 0 && state < (int32)mStates.Size()) ? mStates[(uint32)state].refKind : (uint8)0;
		}

		const NkString &NkAnimStateMachine::GetStateRef(int32 state) const {
			static NkString sEmpty;
			return (state >= 0 && state < (int32)mStates.Size()) ? mStates[(uint32)state].ref : sEmpty;
		}

		void NkAnimStateMachine::SetStateClipRef(int32 state, const NkString &clipName, const NkAnimationClip *clip) {
			if (state < 0 || state >= (int32)mStates.Size() || mStates[(uint32)state].composite) {
				return;
			}
			State &st = mStates[(uint32)state];
			st.refKind = clipName.Empty() && clip == nullptr ? (uint8)0 : (uint8)1;
			st.ref = clipName;
			st.clip = clip;
			st.tree = nullptr;
			st.tree2d = nullptr;
		}

		void NkAnimStateMachine::SetStateBlendRef(int32 state, const NkString &spaceName, uint8 dims) {
			if (state < 0 || state >= (int32)mStates.Size() || mStates[(uint32)state].composite) {
				return;
			}
			State &st = mStates[(uint32)state];
			st.refKind = spaceName.Empty() ? (uint8)0 : (dims >= 2 ? (uint8)3 : (uint8)2);
			st.ref = spaceName;
			st.clip = nullptr;
			st.tree = nullptr;
			st.tree2d = nullptr;
		}

		int32 NkAnimStateMachine::GetTransitionFrom(uint32 t) const {
			return t < (uint32)mTransitions.Size() ? mTransitions[t].from : -1;
		}

		int32 NkAnimStateMachine::GetTransitionTo(uint32 t) const {
			return t < (uint32)mTransitions.Size() ? mTransitions[t].to : -1;
		}

		bool NkAnimStateMachine::IsAnyStateTransition(uint32 t) const {
			return t < (uint32)mTransitions.Size() && mTransitions[t].any;
		}

		int32 NkAnimStateMachine::GetTransitionScope(uint32 t) const {
			return t < (uint32)mTransitions.Size() ? mTransitions[t].scope : NK_ROOT;
		}

		float32 NkAnimStateMachine::GetTransitionFade(uint32 t) const {
			return t < (uint32)mTransitions.Size() ? mTransitions[t].fadeDur : 0.f;
		}

		int32 NkAnimStateMachine::GetTransitionPriority(uint32 t) const {
			return t < (uint32)mTransitions.Size() ? mTransitions[t].priority : 0;
		}

		uint32 NkAnimStateMachine::GetConditionCount(uint32 t) const {
			return t < (uint32)mTransitions.Size() ? (uint32)mTransitions[t].conds.Size() : 0u;
		}

		bool NkAnimStateMachine::GetCondition(uint32 t, uint32 k, NkString &param, NkCondKind &kind,
											  float32 &threshold) const {
			if (t >= (uint32)mTransitions.Size() || k >= (uint32)mTransitions[t].conds.Size()) {
				return false;
			}
			const Condition &c = mTransitions[t].conds[k];
			param = c.param;
			kind = c.kind;
			threshold = c.threshold;
			return true;
		}

		float32 NkAnimStateMachine::GetParamDefault(uint32 i) const {
			return i < (uint32)Params().Size() ? Params()[i].defaultValue : 0.f;
		}

		// ── Disposition dans l'editeur (2026-10-01) ──────────────────────────────
		void NkAnimStateMachine::SetStatePosition(int32 state, float32 x, float32 y) {
			if (state >= 0 && state < (int32)mStates.Size()) {
				State &st = mStates[(uint32)state];
				st.edX = x;
				st.edY = y;
				st.edPlaced = true;
			}
		}

		bool NkAnimStateMachine::GetStatePosition(int32 state, float32 &x, float32 &y) const {
			if (state < 0 || state >= (int32)mStates.Size() || !mStates[(uint32)state].edPlaced) {
				return false;
			}
			x = mStates[(uint32)state].edX;
			y = mStates[(uint32)state].edY;
			return true;
		}

		void NkAnimStateMachine::SetPseudoPosition(int32 machine, int32 pseudo, float32 x, float32 y) {
			if (pseudo < 0 || pseudo > 1 || (machine != NK_ROOT && !IsSubMachine(machine))) {
				return;
			}
			Pseudo *p = nullptr;
			for (uint32 i = 0; i < (uint32)mPseudos.Size(); ++i) {
				if (mPseudos[i].machine == machine) {
					p = &mPseudos[i];
				}
			}
			if (p == nullptr) {
				Pseudo n;
				n.machine = machine;
				mPseudos.PushBack(n);
				p = &mPseudos[mPseudos.Size() - 1];
			}
			p->x[pseudo] = x;
			p->y[pseudo] = y;
			p->placed[pseudo] = true;
		}

		bool NkAnimStateMachine::GetPseudoPosition(int32 machine, int32 pseudo, float32 &x, float32 &y) const {
			for (uint32 i = 0; pseudo >= 0 && pseudo <= 1 && i < (uint32)mPseudos.Size(); ++i) {
				if (mPseudos[i].machine == machine && mPseudos[i].placed[pseudo]) {
					x = mPseudos[i].x[pseudo];
					y = mPseudos[i].y[pseudo];
					return true;
				}
			}
			return false;
		}

		bool NkAnimStateMachine::IsSubMachine(int32 state) const {
			return state >= 0 && state < (int32)mStates.Size() && mStates[(uint32)state].composite;
		}

		int32 NkAnimStateMachine::FindState(const NkString &nameOrPath) const {
			const char *p = nameOrPath.CStr();
			bool chemin = false;
			for (const char *q = p; *q; ++q) {
				if (*q == '/') {
					chemin = true;
					break;
				}
			}
			if (!chemin) {
				for (uint32 i = 0; i < (uint32)mStates.Size(); ++i) {
					if (mStates[i].name == nameOrPath) {
						return (int32)i;
					}
				}
				return -1;
			}
			// Un chemin : chaque segment est cherche parmi les enfants du precedent.
			int32 parent = NK_ROOT;
			int32 found = -1;
			while (*p) {
				const char *fin = p;
				while (*fin && *fin != '/') {
					++fin;
				}
				const NkString seg(p, (usize)(fin - p));
				found = -1;
				for (uint32 i = 0; i < (uint32)mStates.Size(); ++i) {
					if (mStates[i].parent == parent && mStates[i].name == seg) {
						found = (int32)i;
						break;
					}
				}
				if (found < 0) {
					return -1;
				}
				parent = found;
				p = *fin ? fin + 1 : fin;
			}
			return found;
		}

		// ── Transitions ──────────────────────────────────────────────────────────
		void NkAnimStateMachine::AddTransition(int32 from, int32 to, const NkString &paramName, NkCondKind kind,
											   float32 threshold, float32 fadeDur) {
			// L'API plate est UNE transition a UNE condition, de priorite 0. Tout
			// from < 0 etait « depuis n'importe ou » (le test etait from >= 0) :
			// on le garde tel quel, -2 compris.
			const int32 t = AddTransitionEx(from, to, fadeDur, 0);
			AddCondition(t, paramName, kind, threshold);
		}

		int32 NkAnimStateMachine::AddTransitionEx(int32 from, int32 to, float32 fadeDur, int32 priority) {
			Transition tr;
			tr.from = from;
			tr.to = to;
			tr.any = from < 0;
			tr.scope = NK_ROOT;
			tr.fadeDur = fadeDur;
			tr.priority = priority;
			mTransitions.PushBack(tr);
			return (int32)mTransitions.Size() - 1;
		}

		int32 NkAnimStateMachine::AddAnyStateTransition(int32 scope, int32 to, float32 fadeDur, int32 priority) {
			if (!ValidParent(scope)) {
				logger.Errorf("[NkAnimSM] AddAnyStateTransition refuse : %d n'est pas une sous-machine\n", scope);
				return -1;
			}
			Transition tr;
			tr.from = -1;
			tr.to = to;
			tr.any = true;
			tr.scope = scope;
			tr.fadeDur = fadeDur;
			tr.priority = priority;
			mTransitions.PushBack(tr);
			return (int32)mTransitions.Size() - 1;
		}

		bool NkAnimStateMachine::AddCondition(int32 transition, const NkString &param, NkCondKind kind, float32 threshold) {
			if (transition < 0 || transition >= (int32)mTransitions.Size()) {
				return false;
			}
			Condition c;
			c.param = param;
			c.kind = kind;
			c.threshold = threshold;
			mTransitions[(uint32)transition].conds.PushBack(c);
			switch (kind) {
				case NkCondKind::BOOL_TRUE:
				case NkCondKind::BOOL_FALSE:
					DeclareParamRef(param, NkParamKind::BOOL);
					break;
				case NkCondKind::FLOAT_GREATER:
				case NkCondKind::FLOAT_LESS:
					DeclareParamRef(param, NkParamKind::FLOAT);
					break;
				case NkCondKind::TRIGGER:
					DeclareParamRef(param, NkParamKind::TRIGGER);
					break;
				case NkCondKind::TIME_IN_STATE:
					break; // aucun parametre : c'est l'horloge de la machine
			}
			return true;
		}

		bool NkAnimStateMachine::CondTrue(const Condition &c, int32 source) const {
			switch (c.kind) {
				case NkCondKind::BOOL_TRUE: {
					const Param *p = FindParamPtr(c.param, NkParamKind::BOOL);
					return p && p->value != 0.f;
				}
				case NkCondKind::BOOL_FALSE: {
					const Param *p = FindParamPtr(c.param, NkParamKind::BOOL);
					return !(p && p->value != 0.f);
				}
				case NkCondKind::TRIGGER: {
					const Param *p = FindParamPtr(c.param, NkParamKind::TRIGGER);
					return p && p->value != 0.f;
				}
				case NkCondKind::TIME_IN_STATE: {
					const float32 t = GetTimeInState(source);
					return t >= 0.f && t >= c.threshold;
				}
				default: {
					const Param *p = FindParamPtr(c.param, NkParamKind::FLOAT);
					const float32 v = p ? p->value : 0.f;
					return (c.kind == NkCondKind::FLOAT_GREATER) ? (v > c.threshold) : (v < c.threshold);
				}
			}
		}

		// ── Chemin actif ─────────────────────────────────────────────────────────
		int32 NkAnimStateMachine::BuildPath(int32 leaf, int32 *path) const {
			if (leaf < 0 || leaf >= (int32)mStates.Size()) {
				return 0;
			}
			const int32 n = mStates[(uint32)leaf].depth + 1;
			int32 s = leaf;
			for (int32 d = n - 1; d >= 0 && s >= 0; --d) {
				path[d] = s;
				s = mStates[(uint32)s].parent;
			}
			return n;
		}

		bool NkAnimStateMachine::IsInState(int32 state) const {
			if (state < 0 || state >= (int32)mStates.Size()) {
				return false;
			}
			int32 path[NK_MAX_DEPTH];
			const int32 n = BuildPath(mCurrent, path);
			const int32 d = mStates[(uint32)state].depth;
			return d < n && path[d] == state;
		}

		float32 NkAnimStateMachine::GetTimeInState(int32 state) const {
			if (!IsInState(state)) {
				return -1.f;
			}
			return mClock - mEnteredAt[mStates[(uint32)state].depth];
		}

		NkString NkAnimStateMachine::GetCurrentPath() const {
			int32 path[NK_MAX_DEPTH];
			const int32 n = BuildPath(mCurrent, path);
			NkString out;
			for (int32 d = 0; d < n; ++d) {
				if (d > 0) {
					out.Append("/");
				}
				out.Append(mStates[(uint32)path[d]].name.CStr());
			}
			return out;
		}

		void NkAnimStateMachine::MarkEntered(int32 from, int32 to) {
			int32 pa[NK_MAX_DEPTH];
			int32 pb[NK_MAX_DEPTH];
			const int32 na = BuildPath(from, pa);
			const int32 nb = BuildPath(to, pb);
			// Les niveaux communs aux deux chemins n'ont pas change d'etat : leur
			// temps continue (passer de idle a marche ne remet pas « Sol » a zero).
			int32 d = 0;
			while (d < na && d < nb && pa[d] == pb[d]) {
				++d;
			}
			for (; d < NK_MAX_DEPTH; ++d) {
				mEnteredAt[d] = mClock;
			}
		}

		void NkAnimStateMachine::EnsureStarted() {
			if (mStarted && mCurrent >= 0 && mCurrent < (int32)mStates.Size()) {
				return;
			}
			const int32 leaf = (mCurrent >= 0 && mCurrent < (int32)mStates.Size()) ? ResolveLeaf(mCurrent) : ResolveLeaf(NK_ROOT);
			mCurrent = leaf;
			if (mCurrent < 0) {
				return;
			}
			mStarted = true;
			for (int32 d = 0; d < NK_MAX_DEPTH; ++d) {
				mEnteredAt[d] = mClock;
			}
		}

		void NkAnimStateMachine::ForceState(int32 idx) {
			if (idx >= 0 && idx < (int32)mStates.Size()) {
				const int32 prev = mCurrent;
				mCurrent = ResolveLeaf(idx);
				mNext = -1;
				mFadeT = 0.f;
				if (!mStarted) {
					mStarted = true;
					for (int32 d = 0; d < NK_MAX_DEPTH; ++d) {
						mEnteredAt[d] = mClock;
					}
				} else {
					MarkEntered(prev, mCurrent);
				}
			}
		}

		void NkAnimStateMachine::Reset() {
			mNext = -1;
			mFadeT = 0.f;
			mFadeDur = 0.f;
			mCurrent = ResolveLeaf(NK_ROOT);
			mStarted = mCurrent >= 0;
			for (int32 d = 0; d < NK_MAX_DEPTH; ++d) {
				mEnteredAt[d] = mClock;
			}
			if (mCurrent >= 0) {
				mStates[(uint32)mCurrent].time = 0.f;
			}
		}

		const NkString &NkAnimStateMachine::GetCurrentStateName() const {
			static NkString sEmpty;
			return (mCurrent >= 0 && mCurrent < (int32)mStates.Size()) ? mStates[(uint32)mCurrent].name : sEmpty;
		}

		bool NkAnimStateMachine::SetTransitionCurve(int32 t, NkFadeCurve curve) {
			if (t < 0 || t >= (int32)mTransitions.Size() || (uint8)curve >= (uint8)NkFadeCurve::NK_COUNT) {
				return false;
			}
			mTransitions[(uint32)t].curve = curve;
			return true;
		}

		NkFadeCurve NkAnimStateMachine::GetTransitionCurve(uint32 t) const {
			return t < (uint32)mTransitions.Size() ? mTransitions[t].curve : NkFadeCurve::NK_LINEAR;
		}

		int32 NkAnimStateMachine::FindTransitionTo(int32 from, int32 to) const {
			if (from < 0 || to < 0 || from >= (int32)mStates.Size()) {
				return -1;
			}
			int32 path[NK_MAX_DEPTH];
			const int32 n = BuildPath(from, path);
			for (uint32 i = 0; i < (uint32)mTransitions.Size(); ++i) {
				const Transition &tr = mTransitions[i];
				if (ResolveLeaf(tr.to) != to) {
					continue;
				}
				for (int32 k = 0; k < n; ++k) {
					if ((!tr.any && tr.from == path[k]) || (tr.any && (tr.scope == NK_ROOT || tr.scope == path[k]))) {
						return (int32)i;
					}
				}
			}
			return -1;
		}

		float32 NkAnimStateMachine::GetFadeWeight() const {
			if (mNext < 0 || mFadeDur <= 0.f) {
				return 0.f;
			}
			return 1.f - (mFadeT > 0.f ? mFadeT / mFadeDur : 0.f);
		}

		int32 NkAnimStateMachine::PickTransition() const {
			int32 path[NK_MAX_DEPTH];
			const int32 n = BuildPath(mCurrent, path);
			auto onPath = [&](int32 s) {
				if (s < 0 || s >= (int32)mStates.Size()) {
					return false;
				}
				const int32 d = mStates[(uint32)s].depth;
				return d < n && path[d] == s;
			};
			int32 best = -1;
			int32 bestPrio = 0;
			int32 bestDepth = 0;
			for (uint32 i = 0; i < (uint32)mTransitions.Size(); i++) {
				const Transition &tr = mTransitions[i];
				// Cible hors bornes, ou deja active (l'ancien « tr.to == mCurrent »,
				// etendu aux ancetres : une any-state vers « Air » ne doit pas
				// re-entrer dans Air a chaque trame passee dedans).
				if (tr.to < 0 || tr.to >= (int32)mStates.Size() || onPath(tr.to)) {
					continue;
				}
				int32 depth = 0;
				int32 source = mCurrent;
				if (tr.any) {
					if (tr.scope != NK_ROOT && !onPath(tr.scope)) {
						continue;
					}
					depth = (tr.scope == NK_ROOT) ? 0 : mStates[(uint32)tr.scope].depth + 1;
					source = depth < n ? path[depth] : mCurrent;
				} else {
					if (!onPath(tr.from)) {
						continue;
					}
					depth = mStates[(uint32)tr.from].depth;
					source = tr.from;
				}
				// Strictement meilleure seulement : a egalite, la premiere ajoutee
				// garde la place — c'est le `break` de l'ancienne boucle.
				if (best >= 0 && !(tr.priority > bestPrio || (tr.priority == bestPrio && depth < bestDepth))) {
					continue;
				}
				bool ok = true;
				for (uint32 k = 0; k < (uint32)tr.conds.Size() && ok; ++k) {
					ok = CondTrue(tr.conds[k], source);
				}
				if (!ok) {
					continue;
				}
				best = (int32)i;
				bestPrio = tr.priority;
				bestDepth = depth;
			}
			return best;
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
			if (!st.clip) {
				// (01/10 soir) Un etat designe par son NOM (clip ou arbre que le
				// consommateur resout lui-meme, Unkeny) : son temps AVANCE quand
				// meme, pour qu'un melangeur externe (Blend/NkAnimMix.h) sache ou en
				// est le clip. Sans clip, rien d'autre ne le lit.
				st.time += dt;
				return;
			}
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
			EnsureStarted();
			if (mCurrent < 0 || mCurrent >= (int32)mStates.Size())
				return;
			mClock += dt;

			// Transitions (pas de re-trigger pendant un fondu).
			if (mNext < 0) {
				const int32 t = PickTransition();
				if (t >= 0) {
					const Transition &tr = mTransitions[(uint32)t];
					// Les declencheurs de CETTE transition sont consommes, et eux
					// seuls : un « saut » pose en meme temps qu'un « attaque » ne
					// perd pas l'attaque.
					for (uint32 k = 0; k < (uint32)tr.conds.Size(); ++k) {
						if (tr.conds[k].kind == NkCondKind::TRIGGER) {
							ResetTrigger(tr.conds[k].param);
						}
					}
					const int32 target = ResolveLeaf(tr.to);
					if (target >= 0 && target != mCurrent) {
						mNext = target;
						mFadeDur = tr.fadeDur > 1e-3f ? tr.fadeDur : 1e-3f;
						mFadeT = mFadeDur;
						mFadeCurve = tr.curve;
						mStates[(uint32)mNext].time = 0.f; // repart du debut
						if (mTransitionCb)
							mTransitionCb(mStates[(uint32)mCurrent].name, mStates[(uint32)mNext].name,
										  /*finished=*/false);
					}
				}
			}

			const NkAnimationClip *skelA = nullptr;
			EvalState(mCurrent, dt, mState, mLocalA, skelA);

			if (mNext >= 0) {
				const NkAnimationClip *skelB = nullptr;
				EvalState(mNext, dt, mNextState, mLocalB, skelB);
				mFadeT -= dt;
				// 0 -> 1, par la COURBE de la transition (lineaire par defaut).
				const float32 w = NkFadeWeight(1.f - (mFadeT > 0.f ? mFadeT / mFadeDur : 0.f), mFadeCurve);
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
					MarkEntered(prev, mCurrent);
					if (mTransitionCb)
						mTransitionCb(mStates[(uint32)prev].name, mStates[(uint32)mCurrent].name,
									  /*finished=*/true);
				}
			}
		}

		// ── Etat d'execution ─────────────────────────────────────────────────────
		NkAnimStateMachine::NkRuntime NkAnimStateMachine::GetRuntime() const {
			NkRuntime rt;
			rt.current = mStarted ? mCurrent : -1;
			rt.next = mNext;
			rt.fadeT = mFadeT;
			rt.fadeDur = mFadeDur;
			rt.clock = mClock;
			rt.currentTime = (mCurrent >= 0 && mCurrent < (int32)mStates.Size()) ? mStates[(uint32)mCurrent].time : 0.f;
			rt.nextTime = (mNext >= 0 && mNext < (int32)mStates.Size()) ? mStates[(uint32)mNext].time : 0.f;
			for (int32 d = 0; d < NK_MAX_DEPTH; ++d) {
				rt.enteredAt[d] = mEnteredAt[d];
			}
			return rt;
		}

		void NkAnimStateMachine::SetRuntime(const NkRuntime &rt) {
			const int32 n = (int32)mStates.Size();
			const bool curOK = rt.current >= 0 && rt.current < n && !mStates[(uint32)rt.current].composite;
			const bool nextOK = rt.next < 0 || (rt.next < n && !mStates[(uint32)rt.next].composite);
			mClock = rt.clock;
			for (int32 d = 0; d < NK_MAX_DEPTH; ++d) {
				mEnteredAt[d] = rt.enteredAt[d];
			}
			if (!curOK || !nextOK) {
				// Pas demarree, ou un etat que le modele n'a plus : on repart de
				// l'entree. Lire un index perime donnerait un etat quelconque.
				mStarted = false;
				mCurrent = ResolveLeaf(NK_ROOT);
				mNext = -1;
				mFadeT = 0.f;
				mFadeDur = 0.f;
				return;
			}
			mStarted = true;
			mCurrent = rt.current;
			mNext = rt.next;
			mFadeT = rt.fadeT;
			mFadeDur = rt.fadeDur;
			mStates[(uint32)mCurrent].time = rt.currentTime;
			if (mNext >= 0) {
				mStates[(uint32)mNext].time = rt.nextTime;
			}
		}

		// ── Sauvegarde .nkanimctl ────────────────────────────────────────────────
		// [magic 'NKAC'(u32)] [version(u32)=1] [nbSections(u32)]
		//   par section : [etiquette(u32)] [taille(u32)] [contenu]
		// (Le .nkanim v3 du 29/09 avait [magic 'NKAN'][3][corps de clip VIDE] en
		// tete, puis exactement les memes sections : c'est ce qui le rend relisible.)
		// Section 'HFSM' (contenu, version interne 1) :
		//   [version(u32)=1] [nbEtats(u32)]
		//     par etat : [nom] [parent(i32)] [genre(u8) 0 vide 1 clip 2 arbre1D
		//                3 arbre2D 4 sous-machine] [ref] [etiquette(i32)] [entree(i32)]
		//   [entreeRacine(i32)] [nbParams(u32)] par param : [nom] [genre(u8)] [defaut(f32)]
		//   [nbTransitions(u32)] par transition : [from(i32)] [to(i32)] [any(u8)]
		//     [portee(i32)] [fondu(f32)] [priorite(i32)] [nbConds(u32)]
		//     par condition : [param] [genre(u8)] [seuil(f32)]
		// Une etiquette de section inconnue est SAUTEE grace a sa taille : un
		// lecteur de 2026 lira un fichier de 2027 qui aura ajoute une section.
		void NkAnimStateMachine::SaveToBytes(NkVector<nk_uint8> &out) const {
			ByteWriter w;
			w.u32(kNkAnimCtlMagic);
			w.u32(kNkAnimCtlVersion);
			// (2026-10-01) La disposition de l'editeur, s'il y en a une, dans une
			// SECONDE section ('GRPH') : sans elle, le fichier est octet pour octet
			// celui d'avant ; avec, un lecteur d'avant la saute.
			bool disposition = !mPseudos.Empty();
			for (uint32 i = 0; i < (uint32)mStates.Size() && !disposition; ++i) {
				disposition = mStates[i].edPlaced;
			}
			// (01/10 soir) Les COURBES de fondu, s'il y en a une qui n'est pas lineaire
			// (section 'FADE') : sans elle, le fichier reste celui d'avant.
			bool courbes = false;
			for (uint32 i = 0; i < (uint32)mTransitions.Size() && !courbes; ++i) {
				courbes = mTransitions[i].curve != NkFadeCurve::NK_LINEAR;
			}
			w.u32(1u + (disposition ? 1u : 0u) + (courbes ? 1u : 0u));
			ByteWriter s;
			s.u32(1); // version de la section HFSM
			s.u32((uint32)mStates.Size());
			for (uint32 i = 0; i < (uint32)mStates.Size(); ++i) {
				const State &st = mStates[i];
				s.str(st.name);
				s.i32(st.parent);
				// Le genre est celui de la DECLARATION, pas celui du pointeur : un
				// clip non resolu au chargement reste un etat-clip a la sauvegarde.
				s.u8(st.composite ? (uint8)4 : st.refKind);
				s.str(st.ref);
				s.i32(st.tag);
				s.i32(st.entry);
			}
			s.i32(mRootEntry);
			const NkVector<Param> &ps = Params();
			s.u32((uint32)ps.Size());
			for (uint32 i = 0; i < (uint32)ps.Size(); ++i) {
				s.str(ps[i].name);
				s.u8((uint8)ps[i].kind);
				s.f32(ps[i].defaultValue);
			}
			s.u32((uint32)mTransitions.Size());
			for (uint32 i = 0; i < (uint32)mTransitions.Size(); ++i) {
				const Transition &tr = mTransitions[i];
				s.i32(tr.from);
				s.i32(tr.to);
				s.u8(tr.any ? 1 : 0);
				s.i32(tr.scope);
				s.f32(tr.fadeDur);
				s.i32(tr.priority);
				s.u32((uint32)tr.conds.Size());
				for (uint32 k = 0; k < (uint32)tr.conds.Size(); ++k) {
					s.str(tr.conds[k].param);
					s.u8((uint8)tr.conds[k].kind);
					s.f32(tr.conds[k].threshold);
				}
			}
			w.u32(kNkAnimSectionHFSM);
			w.u32((uint32)s.buf.Size());
			w.raw(s.buf.Data(), s.buf.Size());
			if (disposition) {
				// 'GRPH' : [version(u32)=1] [nbEtats(u32)] par etat : [place(u8)] [x(f32)] [y(f32)]
				//   [nbNiveaux(u32)] par niveau : [machine(i32)] 2 x ([place(u8)] [x(f32)] [y(f32)])
				ByteWriter g;
				g.u32(1);
				g.u32((uint32)mStates.Size());
				for (uint32 i = 0; i < (uint32)mStates.Size(); ++i) {
					g.u8(mStates[i].edPlaced ? 1 : 0);
					g.f32(mStates[i].edX);
					g.f32(mStates[i].edY);
				}
				g.u32((uint32)mPseudos.Size());
				for (uint32 i = 0; i < (uint32)mPseudos.Size(); ++i) {
					g.i32(mPseudos[i].machine);
					for (int32 k = 0; k < 2; ++k) {
						g.u8(mPseudos[i].placed[k] ? 1 : 0);
						g.f32(mPseudos[i].x[k]);
						g.f32(mPseudos[i].y[k]);
					}
				}
				w.u32(kNkAnimSectionGRPH);
				w.u32((uint32)g.buf.Size());
				w.raw(g.buf.Data(), g.buf.Size());
			}
			if (courbes) {
				// 'FADE' : [version(u32)=1] [nbTransitions(u32)] par transition : [courbe(u8)]
				ByteWriter f;
				f.u32(1);
				f.u32((uint32)mTransitions.Size());
				for (uint32 i = 0; i < (uint32)mTransitions.Size(); ++i) {
					f.u8((uint8)mTransitions[i].curve);
				}
				w.u32(kNkAnimSectionFADE);
				w.u32((uint32)f.buf.Size());
				w.raw(f.buf.Data(), f.buf.Size());
			}
			out = w.buf;
		}

		bool NkAnimStateMachine::LoadFromBytes(const nk_uint8 *data, usize size, const NkResolver &resolver) {
			if (data == nullptr || size == 0) {
				logger.Errorf("[NkAnimSM] LoadFromBytes : rien a lire\n");
				return false;
			}
			ByteReader r(data, size);
			const uint32 magic = r.u32();
			const uint32 ver = r.u32();
			if (magic == kNkAnimCtlMagic) {
				if (ver != kNkAnimCtlVersion) {
					logger.Errorf("[NkAnimSM] .nkanimctl v%u non supporte\n", ver);
					return false;
				}
			} else if (magic == kNkAnimMagic) {
				if (ver != kNkAnimVersionSections) {
					// v1/v2 : un clip seul. Ce n'est pas un fichier casse, c'est un
					// fichier d'une autre nature — NkAnimationClip::LoadBinary le lit.
					logger.Errorf("[NkAnimSM] .nkanim v%u = clip seul, sans machine a etats\n", ver);
					return false;
				}
				// Le .nkanim v3 du 29/09 : on saute son corps de clip vide, les
				// sections qui suivent sont celles d'un .nkanimctl.
				NkAnimationClip ignore;
				ReadClipBody(r, ver, ignore);
			} else {
				logger.Errorf("[NkAnimSM] magic invalide (0x%08X) : ni .nkanimctl ni .nkanim\n", magic);
				return false;
			}
			const uint32 nbSections = r.u32();
			bool trouve = false;
			NkAnimStateMachine lu;
			// La disposition de l'editeur ('GRPH'), appliquee APRES la boucle : elle
			// peut venir avant ou apres la section des etats.
			const nk_uint8 *grph = nullptr;
			uint32 grphTaille = 0;
			const nk_uint8 *fade = nullptr; // (01/10 soir) les courbes de fondu
			uint32 fadeTaille = 0;
			for (uint32 sct = 0; sct < nbSections && r.ok; ++sct) {
				const uint32 tag = r.u32();
				const uint32 taille = r.u32();
				if (!r.need(taille)) {
					break;
				}
				if (tag == kNkAnimSectionGRPH && grph == nullptr) {
					grph = r.p + r.off;
					grphTaille = taille;
					r.off += taille;
					continue;
				}
				if (tag == kNkAnimSectionFADE && fade == nullptr) {
					fade = r.p + r.off;
					fadeTaille = taille;
					r.off += taille;
					continue;
				}
				if (tag != kNkAnimSectionHFSM || trouve) {
					r.off += taille; // section inconnue : sautee, pas refusee
					continue;
				}
				ByteReader s(r.p + r.off, taille);
				r.off += taille;
				const uint32 sv = s.u32();
				if (sv != 1) {
					logger.Errorf("[NkAnimSM] section HFSM v%u non supportee\n", sv);
					return false;
				}
				const uint32 ns = s.u32();
				for (uint32 i = 0; i < ns && s.ok; ++i) {
					State st;
					st.name = s.str();
					st.parent = s.i32();
					const uint8 genre = s.u8();
					st.ref = s.str();
					st.tag = s.i32();
					st.entry = s.i32();
					st.composite = genre == 4;
					st.refKind = genre <= 3 ? genre : (uint8)0;
					// Le parent doit PRECEDER l'enfant (c'est l'ordre d'ecriture) :
					// sinon profondeur et chemin seraient faux sans le dire.
					if (st.parent != NK_ROOT &&
						(st.parent < 0 || st.parent >= (int32)i || !lu.mStates[(uint32)st.parent].composite)) {
						logger.Errorf("[NkAnimSM] etat %u : parent %d invalide\n", i, st.parent);
						return false;
					}
					st.depth = (st.parent == NK_ROOT) ? 0 : lu.mStates[(uint32)st.parent].depth + 1;
					if (st.depth >= NK_MAX_DEPTH) {
						logger.Errorf("[NkAnimSM] etat %u : trop profond\n", i);
						return false;
					}
					if (genre == 1 && !st.ref.Empty()) {
						st.clip = resolver.clip ? resolver.clip(st.ref) : nullptr;
					} else if (genre == 2 && !st.ref.Empty()) {
						st.tree = resolver.tree1D ? resolver.tree1D(st.ref) : nullptr;
					} else if (genre == 3 && !st.ref.Empty()) {
						st.tree2d = resolver.tree2D ? resolver.tree2D(st.ref) : nullptr;
					}
					if (genre >= 1 && genre <= 3 && !st.ref.Empty() && !st.clip && !st.tree && !st.tree2d) {
						logger.Warn("[NkAnimSM] etat '{0}' : '{1}' non resolu, l'etat reste vide\n", st.name.CStr(),
									st.ref.CStr());
					}
					lu.mStates.PushBack(st);
				}
				lu.mRootEntry = s.i32();
				const uint32 np = s.u32();
				for (uint32 i = 0; i < np && s.ok; ++i) {
					Param p;
					p.name = s.str();
					p.kind = (NkParamKind)s.u8();
					p.defaultValue = s.f32();
					p.value = p.defaultValue;
					if ((uint8)p.kind > (uint8)NkParamKind::TRIGGER) {
						logger.Errorf("[NkAnimSM] parametre '%s' : genre inconnu\n", p.name.CStr());
						return false;
					}
					lu.mParams.PushBack(p);
				}
				const uint32 nt = s.u32();
				for (uint32 i = 0; i < nt && s.ok; ++i) {
					Transition tr;
					tr.from = s.i32();
					tr.to = s.i32();
					tr.any = s.u8() != 0;
					tr.scope = s.i32();
					tr.fadeDur = s.f32();
					tr.priority = s.i32();
					const uint32 nc = s.u32();
					for (uint32 k = 0; k < nc && s.ok; ++k) {
						Condition c;
						c.param = s.str();
						c.kind = (NkCondKind)s.u8();
						c.threshold = s.f32();
						if ((uint8)c.kind > (uint8)NkCondKind::TIME_IN_STATE) {
							logger.Errorf("[NkAnimSM] condition de genre inconnu\n");
							return false;
						}
						tr.conds.PushBack(c);
					}
					lu.mTransitions.PushBack(tr);
				}
				if (!s.ok) {
					logger.Errorf("[NkAnimSM] section HFSM tronquee\n");
					return false;
				}
				trouve = true;
			}
			if (!r.ok) {
				logger.Errorf("[NkAnimSM] fichier de machine tronque\n");
				return false;
			}
			if (!trouve) {
				logger.Errorf("[NkAnimSM] aucune section HFSM dans ce fichier\n");
				return false;
			}
			// La definition remplace l'ancienne ; le rappel et le partage de
			// parametres, qui sont du CABLAGE et non de la definition, restent.
			if (fade != nullptr) {
				ByteReader f(fade, fadeTaille);
				if (f.u32() == 1u && f.u32() == (uint32)lu.mTransitions.Size()) {
					for (uint32 i = 0; i < (uint32)lu.mTransitions.Size() && f.ok; ++i) {
						const uint8 c = f.u8();
						lu.mTransitions[i].curve = c < (uint8)NkFadeCurve::NK_COUNT ? (NkFadeCurve)c : NkFadeCurve::NK_LINEAR;
					}
				}
			}
			mStates = lu.mStates;
			mTransitions = lu.mTransitions;
			mParams = lu.mParams;
			if (mParamOwner) {
				// Parametres partages : la declaration va chez le proprietaire,
				// sinon les conditions liraient des noms qu'il ne connait pas.
				for (uint32 i = 0; i < (uint32)lu.mParams.Size(); ++i) {
					if (!FindParamPtr(lu.mParams[i].name, lu.mParams[i].kind)) {
						DeclareParam(lu.mParams[i].name, lu.mParams[i].kind, lu.mParams[i].defaultValue);
					}
				}
			}
			mRootEntry = lu.mRootEntry;
			// La disposition de l'editeur : relue si elle est la et coherente (autant
			// d'etats), ignoree sinon -- elle n'a jamais d'effet sur la machine.
			mPseudos.Clear();
			if (grph != nullptr) {
				ByteReader g(grph, grphTaille);
				if (g.u32() == 1u && g.u32() == (uint32)mStates.Size()) {
					for (uint32 i = 0; i < (uint32)mStates.Size() && g.ok; ++i) {
						mStates[i].edPlaced = g.u8() != 0;
						mStates[i].edX = g.f32();
						mStates[i].edY = g.f32();
					}
					uint32 nn = g.u32();
					if (nn > (g.n - g.off) / 22u) {
						nn = 0;
					}
					for (uint32 i = 0; i < nn && g.ok; ++i) {
						Pseudo p;
						p.machine = g.i32();
						for (int32 k = 0; k < 2; ++k) {
							p.placed[k] = g.u8() != 0;
							p.x[k] = g.f32();
							p.y[k] = g.f32();
						}
						if (g.ok) {
							mPseudos.PushBack(p);
						}
					}
				}
			}
			mNext = -1;
			mFadeT = 0.f;
			mFadeDur = 0.f;
			mClock = 0.f;
			mStarted = false;
			mCurrent = ResolveLeaf(NK_ROOT);
			return true;
		}

		bool NkAnimStateMachine::SaveBinary(const NkString &path) const {
			NkVector<nk_uint8> bytes;
			SaveToBytes(bytes);
			if (!NkFile::WriteAllBytes(path.CStr(), bytes)) {
				logger.Errorf("[NkAnimSM] SaveBinary echec : %s\n", path.CStr());
				return false;
			}
			return true;
		}

		bool NkAnimStateMachine::LoadBinary(const NkString &path, const NkResolver &resolver) {
			NkVector<nk_uint8> bytes = NkFile::ReadAllBytes(path.CStr());
			if (bytes.Empty()) {
				logger.Errorf("[NkAnimSM] LoadBinary vide/absent : %s\n", path.CStr());
				return false;
			}
			return LoadFromBytes(bytes.Data(), bytes.Size(), resolver);
		}

	} // namespace anim
} // namespace nkentseu
