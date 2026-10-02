// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// -----------------------------------------------------------------------------
// FICHIER: NKAnima/Skeleton/NkSkeleton2D.cpp — le squelette 2D (voir l'en-tete).
// -----------------------------------------------------------------------------

#include "NKAnima/Skeleton/NkSkeleton2D.h"

#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"

#include <cmath>
#include <cstring>

namespace nkentseu {
	namespace anim {

		namespace {
			constexpr float32 kPi = 3.14159265358979f;
			constexpr uint32 kMagicSkel = 0x4B534B4E;  // 'NKSK'
			constexpr uint32 kVersionSkel = 1;
			constexpr uint32 kSectionBONE = 0x454E4F42; // 'BONE'
			constexpr uint32 kSectionPL2D = 0x44324C50; // 'PL2D' : LA marque du squelette 2D

			// ── Les octets (meme facture que .nkanimctl : sections etiquetees) ──
			struct Ecrivain {
					NkVector<nk_uint8> buf;
					void raw(const void *p, usize n) {
						const nk_uint8 *b = (const nk_uint8 *)p;
						for (usize i = 0; i < n; ++i) {
							buf.PushBack(b[i]);
						}
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
						const uint32 n = (uint32)s.Size();
						u32(n);
						if (n) {
							raw(s.CStr(), n);
						}
					}
					void section(uint32 tag, const Ecrivain &s) {
						u32(tag);
						u32((uint32)s.buf.Size());
						raw(s.buf.Data(), s.buf.Size());
					}
			};

			struct Lecteur {
					const nk_uint8 *p;
					usize n, off = 0;
					bool ok = true;
					Lecteur(const nk_uint8 *d, usize len) : p(d), n(len) {
					}
					bool need(usize c) {
						if (!ok || off + c > n) {
							ok = false;
							return false;
						}
						return true;
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
					int32 i32() {
						return (int32)u32();
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
					/// Un compte lu du fichier, borne par ce qui reste (un fichier abime
					/// n'annonce pas des milliards d'os -- la lecon du 30/09, NkAnimation.cpp).
					uint32 compte(usize tailleMin) {
						const uint32 c = u32();
						if (!ok || (tailleMin > 0 && c > (n - off) / tailleMin)) {
							ok = false;
							return 0u;
						}
						return c;
					}
			};

			NkVec2f Transformer(const NkMat4f &m, const NkVec2f &p) {
				return NkVec2f(m.m00 * p.x + m.m10 * p.y + m.m30, m.m01 * p.x + m.m11 * p.y + m.m31);
			}

			void CopierNom(char *dst, const char *src) {
				std::memset(dst, 0, NkBoneDef::kMaxBoneNameLen);
				if (src != nullptr) {
					std::strncpy(dst, src, NkBoneDef::kMaxBoneNameLen - 1);
				}
			}

			/// Recalcule le monde (et l'inverse) de `j` et de tous ses descendants
			/// depuis leurs locaux `local` (indices du squelette).
			void RefaireMonde(NkSkeletonDef &s, const NkVector<NkMat4f> &local) {
				const uint32 n = s.Count();
				NkVector<NkMat4f> monde;
				monde.Resize(n);
				s.LocalToWorld(local.Data(), monde.Data());
				for (uint32 k = 0; k < n; ++k) {
					s.bones[k].bindPose = monde[k];
					s.bones[k].inverseBindPose = monde[k].Inverse();
				}
			}
		} // namespace

		// =====================================================================
		// LA PLACE D'UN OS
		// =====================================================================
		float32 NkWrapAngle(float32 a) {
			while (a > kPi) {
				a -= 2.f * kPi;
			}
			while (a <= -kPi) {
				a += 2.f * kPi;
			}
			return a;
		}

		NkMat4f NkBone2DToMatrix(const NkBone2D &b) {
			const float32 c = std::cos(b.angle), s = std::sin(b.angle);
			NkMat4f m = NkMat4f::Identity();
			m.m00 = c * b.sx;
			m.m01 = s * b.sx;
			m.m10 = -s * b.sy;
			m.m11 = c * b.sy;
			m.m30 = b.x;
			m.m31 = b.y;
			return m;
		}

		NkBone2D NkBone2DFromMatrix(const NkMat4f &m) {
			NkBone2D b;
			b.x = m.m30;
			b.y = m.m31;
			b.sx = std::sqrt(m.m00 * m.m00 + m.m01 * m.m01);
			b.sy = std::sqrt(m.m10 * m.m10 + m.m11 * m.m11);
			b.angle = b.sx > 1e-8f ? std::atan2(m.m01, m.m00) : std::atan2(-m.m10, m.m11);
			return b;
		}

		NkBoneTRS NkBone2DToTRS(const NkBone2D &b) {
			NkBoneTRS t;
			t.t = NkVec3f(b.x, b.y, 0.f);
			const float32 h = b.angle * 0.5f;
			t.r = NkQuatf(0.f, 0.f, std::sin(h), std::cos(h));
			t.s = NkVec3f(b.sx, b.sy, 1.f);
			return t;
		}

		NkBone2D NkBone2DFromTRS(const NkBoneTRS &t) {
			NkBone2D b;
			b.x = t.t.x;
			b.y = t.t.y;
			// La part autour de Z de la rotation (exacte pour un os du plan).
			b.angle = NkWrapAngle(2.f * std::atan2(t.r.z, t.r.w));
			b.sx = t.s.x;
			b.sy = t.s.y;
			return b;
		}

		NkMat4f NkLockToPlane(const NkMat4f &m) {
			// La projection : l'axe X local ramene dans le plan donne l'angle et
			// l'echelle x ; l'axe Y, l'echelle y. Une matrice deja plane revient
			// telle quelle (a l'arrondi pres).
			return NkBone2DToMatrix(NkBone2DFromMatrix(m));
		}

		bool NkIsPlanar(const NkMat4f &m, float32 tol) {
			// Translation en z, axes X et Y hors du plan, axe Z penche, echelle z.
			if (std::fabs(m.m32) > tol || std::fabs(m.m02) > tol || std::fabs(m.m12) > tol || std::fabs(m.m20) > tol ||
				std::fabs(m.m21) > tol || std::fabs(m.m22 - 1.f) > tol) {
				return false;
			}
			return true;
		}

		// =====================================================================
		// LES EMPLACEMENTS
		// =====================================================================
		NkString NkSlot2DProperty(const NkString &slot, bool order) {
			NkString s("Emplacement[");
			s.Append(slot.CStr());
			s.Append(order ? "].ordre" : "].image");
			return s;
		}

		bool NkParseSlot2DProperty(const NkString &property, NkString &slot, bool &order) {
			const char *p = property.CStr();
			const char *pre = "Emplacement[";
			const usize lp = std::strlen(pre);
			if (property.Size() <= lp + 2 || std::strncmp(p, pre, lp) != 0) {
				return false;
			}
			const char *fin = std::strstr(p + lp, "].");
			if (fin == nullptr) {
				return false;
			}
			const char *champ = fin + 2;
			if (std::strcmp(champ, "image") == 0) {
				order = false;
			} else if (std::strcmp(champ, "ordre") == 0) {
				order = true;
			} else {
				return false;
			}
			slot = NkString(p + lp, (usize)(fin - (p + lp)));
			return !slot.Empty();
		}

		// =====================================================================
		// LE SQUELETTE 2D
		// =====================================================================
		void NkSkeleton2D::Clear() {
			skeleton.bones.Clear();
			skeleton.topo.Clear();
			skeleton.path[0] = '\0';
			lengths.Clear();
			slots.Clear();
		}

		int32 NkSkeleton2D::AddBone(const char *name, int32 parent, const NkBone2D &local, float32 length) {
			const uint32 n = Count();
			if (parent >= (int32)n || parent < -1) {
				logger.Warn("[NKAnima] squelette 2D : parent {0} inconnu pour '{1}'", parent, name != nullptr ? name : "");
				return -1;
			}
			NkBoneDef b;
			CopierNom(b.name, name);
			b.parent = parent;
			const NkMat4f l = NkBone2DToMatrix(local);
			b.bindPose = parent >= 0 ? skeleton.bones[(uint32)parent].bindPose * l : l;
			b.inverseBindPose = b.bindPose.Inverse();
			skeleton.bones.PushBack(b);
			lengths.PushBack(length > 0.f ? length : 0.f);
			skeleton.BuildTopo();
			return (int32)n;
		}

		int32 NkSkeleton2D::AddBoneHeadTail(const char *name, int32 parent, const NkVec2f &head, const NkVec2f &tail) {
			const float32 dx = tail.x - head.x, dy = tail.y - head.y;
			const float32 longueur = std::sqrt(dx * dx + dy * dy);
			const float32 angleMonde = longueur > 1e-6f ? std::atan2(dy, dx) : 0.f;
			// Le local = l'inverse du monde du parent * le monde voulu.
			NkBone2D monde;
			monde.x = head.x;
			monde.y = head.y;
			monde.angle = angleMonde;
			NkBone2D local = monde;
			if (parent >= 0 && (uint32)parent < Count()) {
				local = NkBone2DFromMatrix(skeleton.bones[(uint32)parent].inverseBindPose * NkBone2DToMatrix(monde));
			}
			return AddBone(name, parent, local, longueur);
		}

		NkBone2D NkSkeleton2D::BindLocal2D(uint32 j) const {
			return NkBone2DFromMatrix(skeleton.BindLocal(j));
		}

		void NkSkeleton2D::SetBindLocal2D(uint32 j, const NkBone2D &local) {
			const uint32 n = Count();
			if (j >= n) {
				return;
			}
			NkVector<NkMat4f> loc;
			loc.Resize(n);
			for (uint32 k = 0; k < n; ++k) {
				loc[k] = skeleton.BindLocal(k);
			}
			loc[j] = NkBone2DToMatrix(local);
			RefaireMonde(skeleton, loc);
		}

		void NkSkeleton2D::BindPose2D(NkVector<NkBone2D> &out) const {
			out.Resize(Count());
			for (uint32 k = 0; k < Count(); ++k) {
				out[k] = BindLocal2D(k);
			}
		}

		void NkSkeleton2D::PoseToWorld(const NkBone2D *local, NkMat4f *world) const {
			const uint32 n = Count();
			NkVector<NkMat4f> loc;
			loc.Resize(n);
			for (uint32 k = 0; k < n; ++k) {
				loc[k] = NkBone2DToMatrix(local[k]);
			}
			skeleton.LocalToWorld(loc.Data(), world);
		}

		NkVec2f NkSkeleton2D::Head(const NkMat4f *world, uint32 j) const {
			return j < Count() ? NkVec2f(world[j].m30, world[j].m31) : NkVec2f(0.f, 0.f);
		}

		NkVec2f NkSkeleton2D::Tail(const NkMat4f *world, uint32 j) const {
			if (j >= Count()) {
				return NkVec2f(0.f, 0.f);
			}
			const float32 l = j < (uint32)lengths.Size() ? lengths[j] : 0.f;
			return Transformer(world[j], NkVec2f(l, 0.f));
		}

		bool NkSkeleton2D::Validate(const char **why) const {
			auto faute = [why](const char *w) {
				if (why != nullptr) {
					*why = w;
				}
				return false;
			};
			const uint32 n = Count();
			if ((uint32)lengths.Size() != n) {
				return faute("une longueur par os");
			}
			for (uint32 k = 0; k < n; ++k) {
				const int32 p = Parent(k);
				if (p >= (int32)n || p < -1 || p == (int32)k) {
					return faute("un parent hors du squelette");
				}
				if (!NkIsPlanar(skeleton.bones[k].bindPose, 1e-3f)) {
					return faute("un os SORT DU PLAN (mode 2D)");
				}
				if (lengths[k] < 0.f) {
					return faute("une longueur negative");
				}
			}
			if (n > 0 && (uint32)skeleton.topo.Size() != n) {
				return faute("l'ordre topologique (un cycle ?)");
			}
			for (uint32 i = 0; i < (uint32)slots.Size(); ++i) {
				if (slots[i].bone < -1 || slots[i].bone >= (int32)n) {
					return faute("un emplacement sur un os inconnu");
				}
			}
			return true;
		}

		void NkSkeleton2D::PrepareClip(NkAnimationClip &clip, bool keyBind) const {
			const uint32 n = Count();
			clip.skeletalLocal = true;
			clip.ResizeBones(n);
			clip.boneCount = n;
			clip.jointParent.Resize(n);
			clip.jointInverseBind.Resize(n);
			clip.jointNames.Resize(n);
			for (uint32 k = 0; k < n; ++k) {
				clip.jointParent[k] = Parent(k);
				clip.jointInverseBind[k] = skeleton.bones[k].inverseBindPose;
				clip.jointNames[k] = NkString(skeleton.bones[k].name);
				// ⚠️ Le .nkanim n'ecrit pas jointNames, mais le NOM DE LA PISTE : c'est
				// lui qui fait le lien au squelette a la relecture.
				clip.boneTracks[k].name = NkString(skeleton.bones[k].name);
				if (keyBind && clip.boneTracks[k].Empty()) {
					clip.boneTracks[k].AddKey(0.f, NkBone2DToMatrix(BindLocal2D(k)), NkInterpMode::NK_LINEAR);
				}
			}
			clip.jointTopo.Resize((uint32)skeleton.topo.Size());
			for (uint32 k = 0; k < (uint32)skeleton.topo.Size(); ++k) {
				clip.jointTopo[k] = skeleton.topo[k];
			}
		}

		// ── .nkskel ─────────────────────────────────────────────────────────
		void NkSkeleton2D::SaveToBytes(NkVector<nk_uint8> &out) const {
			Ecrivain w;
			w.u32(kMagicSkel);
			w.u32(kVersionSkel);
			w.u32(2); // deux sections
			Ecrivain os;
			os.u32(Count());
			for (uint32 k = 0; k < Count(); ++k) {
				const NkBoneDef &b = skeleton.bones[k];
				os.str(NkString(b.name));
				os.i32(b.parent);
				for (uint32 c = 0; c < 16; ++c) {
					os.f32(b.bindPose.data[c]);
				}
			}
			w.section(kSectionBONE, os);
			Ecrivain pl;
			pl.u32(1); // version de la section
			pl.u32((uint32)lengths.Size());
			for (uint32 k = 0; k < (uint32)lengths.Size(); ++k) {
				pl.f32(lengths[k]);
			}
			pl.u32((uint32)slots.Size());
			for (uint32 i = 0; i < (uint32)slots.Size(); ++i) {
				const NkSlot2DDef &s = slots[i];
				pl.str(s.name);
				pl.i32(s.bone);
				pl.u32((uint32)s.attachments.Size());
				for (uint32 a = 0; a < (uint32)s.attachments.Size(); ++a) {
					pl.str(s.attachments[a]);
				}
				pl.i32(s.attachment);
				pl.i32(s.order);
			}
			w.section(kSectionPL2D, pl);
			out = w.buf;
		}

		namespace {
			/// La section 'BONE' -> le squelette (monde, inverse, topo).
			bool LireOs(Lecteur &r, NkSkeletonDef &s) {
				const uint32 n = r.compte(4 + 4 + 64);
				s.bones.Clear();
				s.bones.Resize(n);
				for (uint32 k = 0; k < n && r.ok; ++k) {
					NkBoneDef &b = s.bones[k];
					const NkString nom = r.str();
					CopierNom(b.name, nom.CStr());
					b.parent = r.i32();
					for (uint32 c = 0; c < 16; ++c) {
						b.bindPose.data[c] = r.f32();
					}
					b.inverseBindPose = b.bindPose.Inverse();
				}
				if (!r.ok) {
					return false;
				}
				for (uint32 k = 0; k < n; ++k) {
					if (s.bones[k].parent < -1 || s.bones[k].parent >= (int32)n) {
						return false;
					}
				}
				return s.BuildTopo();
			}

			/// Parcourt les sections : rend la section 'BONE' et la 'PL2D' (pointeurs dans data).
			bool Sections(const nk_uint8 *data, usize size, const nk_uint8 *&os, usize &nos, const nk_uint8 *&pl, usize &npl) {
				Lecteur r(data, size);
				if (r.u32() != kMagicSkel) {
					return false;
				}
				const uint32 version = r.u32();
				if (!r.ok || version == 0 || version > kVersionSkel) {
					return false;
				}
				const uint32 nb = r.compte(8);
				os = pl = nullptr;
				nos = npl = 0;
				for (uint32 i = 0; i < nb && r.ok; ++i) {
					const uint32 tag = r.u32();
					const uint32 taille = r.u32();
					if (!r.need(taille)) {
						return false;
					}
					if (tag == kSectionBONE) {
						os = data + r.off;
						nos = taille;
					} else if (tag == kSectionPL2D) {
						pl = data + r.off;
						npl = taille;
					}
					r.off += taille; // une section inconnue est SAUTEE
				}
				return r.ok && os != nullptr;
			}
		} // namespace

		bool NkSkeleton2D::LoadFromBytes(const nk_uint8 *data, usize size, bool *was2D) {
			const nk_uint8 *os = nullptr, *pl = nullptr;
			usize nos = 0, npl = 0;
			if (was2D != nullptr) {
				*was2D = false;
			}
			if (data == nullptr || !Sections(data, size, os, nos, pl, npl)) {
				logger.Warn("[NKAnima] .nkskel illisible (magic, version ou section 'BONE')");
				return false;
			}
			NkSkeleton2D lu;
			Lecteur ro(os, nos);
			if (!LireOs(ro, lu.skeleton)) {
				logger.Warn("[NKAnima] .nkskel : section 'BONE' abimee (ou cycle)");
				return false;
			}
			const uint32 n = lu.Count();
			if (pl != nullptr) {
				Lecteur rp(pl, npl);
				const uint32 version = rp.u32();
				const uint32 nl = rp.compte(4);
				lu.lengths.Resize(n, 0.f);
				for (uint32 k = 0; k < nl && rp.ok; ++k) {
					const float32 l = rp.f32();
					if (k < n) {
						lu.lengths[k] = l;
					}
				}
				const uint32 ns = rp.compte(4 + 4 + 4 + 8);
				for (uint32 i = 0; i < ns && rp.ok; ++i) {
					NkSlot2DDef s;
					s.name = rp.str();
					s.bone = rp.i32();
					const uint32 na = rp.compte(4);
					for (uint32 a = 0; a < na && rp.ok; ++a) {
						s.attachments.PushBack(rp.str());
					}
					s.attachment = rp.i32();
					s.order = rp.i32();
					lu.slots.PushBack(s);
				}
				if (!rp.ok || version == 0) {
					logger.Warn("[NKAnima] .nkskel : section 'PL2D' abimee");
					return false;
				}
				if (was2D != nullptr) {
					*was2D = true;
				}
			} else {
				// Un squelette 3D : accepte s'il tient dans le plan, longueurs deduites
				// de son premier enfant.
				lu.lengths.Resize(n, 0.f);
				for (uint32 k = 0; k < n; ++k) {
					for (uint32 e = 0; e < n; ++e) {
						if (lu.Parent(e) == (int32)k) {
							const NkVec3f d = lu.skeleton.BindLocal(e) * NkVec3f(0.f, 0.f, 0.f);
							lu.lengths[k] = std::sqrt(d.x * d.x + d.y * d.y);
							break;
						}
					}
				}
			}
			const char *pourquoi = nullptr;
			if (!lu.Validate(&pourquoi)) {
				logger.Warn("[NKAnima] .nkskel refuse en 2D : {0}", pourquoi != nullptr ? pourquoi : "?");
				return false;
			}
			*this = lu;
			return true;
		}

		bool NkSkeleton2D::SaveBinary(const NkString &path) const {
			NkVector<nk_uint8> bytes;
			SaveToBytes(bytes);
			return NkFile::WriteAllBytes(path.CStr(), bytes);
		}

		bool NkSkeleton2D::LoadBinary(const NkString &path, bool *was2D) {
			NkVector<nk_uint8> bytes = NkFile::ReadAllBytes(path.CStr());
			if (bytes.Empty() || !LoadFromBytes(bytes.Data(), bytes.Size(), was2D)) {
				return false;
			}
			std::strncpy(skeleton.path, path.CStr(), sizeof(skeleton.path) - 1);
			skeleton.path[sizeof(skeleton.path) - 1] = '\0';
			return true;
		}

		bool NkLoadSkeletonDef(const NkString &path, NkSkeletonDef &out, bool *is2D) {
			NkVector<nk_uint8> bytes = NkFile::ReadAllBytes(path.CStr());
			const nk_uint8 *os = nullptr, *pl = nullptr;
			usize nos = 0, npl = 0;
			if (bytes.Empty() || !Sections(bytes.Data(), bytes.Size(), os, nos, pl, npl)) {
				return false;
			}
			Lecteur r(os, nos);
			NkSkeletonDef lu;
			if (!LireOs(r, lu)) {
				return false;
			}
			if (is2D != nullptr) {
				*is2D = pl != nullptr;
			}
			out = lu;
			std::strncpy(out.path, path.CStr(), sizeof(out.path) - 1);
			return true;
		}

		// =====================================================================
		// CLES ET ECHANTILLONS
		// =====================================================================
		void NkAddBoneKey2D(NkAnimationClip &clip, uint32 bone, float32 t, const NkBone2D &local, NkInterpMode interp) {
			if (bone >= (uint32)clip.boneTracks.Size()) {
				clip.ResizeBones(bone + 1);
			}
			NkAnimationTrack<NkMat4f> &tr = clip.boneTracks[bone];
			const int32 k = tr.FindKeyAtTime(t, 1e-4f);
			if (k >= 0) {
				tr.RemoveKeyAt((uint32)k); // une cle par instant : la nouvelle remplace
			}
			tr.AddKey(t, NkBone2DToMatrix(local), interp);
			if (t > clip.duration) {
				clip.duration = t;
			}
		}

		NkBone2D NkSampleBone2D(const NkAnimationClip &clip, uint32 bone, float32 t) {
			NkAnimPose p;
			// Un clip d'un os, le temps d'un echantillon : le meme chemin que le jeu.
			NkSampleClip(clip, t, p);
			if (bone >= (uint32)p.bones.Size()) {
				return NkBone2D();
			}
			return NkBone2DFromTRS(p.bones[bone]);
		}

		void NkPoseToBones2D(const NkAnimPose &pose, NkVector<NkBone2D> &out) {
			out.Resize(pose.bones.Size());
			for (uint32 k = 0; k < (uint32)pose.bones.Size(); ++k) {
				out[k] = NkBone2DFromTRS(pose.bones[k]);
			}
		}

		// =====================================================================
		// LES MODELES DE DEPART
		// =====================================================================
		const char *NkSkeleton2DTemplateName(NkSkeleton2DTemplate t) {
			switch (t) {
				case NkSkeleton2DTemplate::NK_HUMANOID:
					return "Humanoide";
				case NkSkeleton2DTemplate::NK_QUADRUPED:
					return "Quadrupede";
				case NkSkeleton2DTemplate::NK_BIRD:
					return "Oiseau";
				case NkSkeleton2DTemplate::NK_CREATURE:
					return "Creature libre";
				default:
					return "?";
			}
		}

		NkString NkMirrorBoneName(const NkString &name) {
			const usize n = name.Size();
			if (n < 2) {
				return NkString();
			}
			const char der = name.CStr()[n - 1];
			if (der != 'G' && der != 'D') {
				return NkString();
			}
			NkString s(name.CStr(), n - 1);
			s.Append(der == 'G' ? "D" : "G");
			return s;
		}

		namespace {
			/// Un os du modele : nom, parent (nom), tete et queue en FRACTIONS de la
			/// boite (x : -0,5..0,5 de la largeur autour du centre ; y : 0..1 de la
			/// hauteur depuis le bas).
			struct OsModele {
					const char *nom;
					const char *parent;
					float32 tx, ty, qx, qy;
			};

			bool Construire(const OsModele *os, uint32 n, const NkVec2f &lo, const NkVec2f &hi, NkSkeleton2D &out) {
				out.Clear();
				const float32 w = hi.x - lo.x, h = hi.y - lo.y;
				const float32 cx = (lo.x + hi.x) * 0.5f;
				for (uint32 i = 0; i < n; ++i) {
					const int32 p = os[i].parent != nullptr ? out.FindBone(os[i].parent) : -1;
					const NkVec2f tete(cx + os[i].tx * w, lo.y + os[i].ty * h);
					const NkVec2f queue(cx + os[i].qx * w, lo.y + os[i].qy * h);
					if (out.AddBoneHeadTail(os[i].nom, p, tete, queue) < 0) {
						return false;
					}
				}
				return out.Validate();
			}
		} // namespace

		bool NkMakeSkeleton2D(NkSkeleton2DTemplate t, const NkVec2f &lo, const NkVec2f &hi, NkSkeleton2D &out) {
			if (hi.x - lo.x <= 1e-5f || hi.y - lo.y <= 1e-5f) {
				return false;
			}
			switch (t) {
				case NkSkeleton2DTemplate::NK_HUMANOID: {
					// De face. Les bras pendent le long du corps, les pieds en dehors.
					static const OsModele k[] = {
						{"Hanches", nullptr, 0.f, 0.47f, 0.f, 0.57f},
						{"Torse", "Hanches", 0.f, 0.57f, 0.f, 0.74f},
						{"Cou", "Torse", 0.f, 0.74f, 0.f, 0.80f},
						{"Tete", "Cou", 0.f, 0.80f, 0.f, 0.97f},
						{"BrasG", "Torse", -0.20f, 0.72f, -0.25f, 0.56f},
						{"AvantBrasG", "BrasG", -0.25f, 0.56f, -0.28f, 0.42f},
						{"MainG", "AvantBrasG", -0.28f, 0.42f, -0.29f, 0.35f},
						{"BrasD", "Torse", 0.20f, 0.72f, 0.25f, 0.56f},
						{"AvantBrasD", "BrasD", 0.25f, 0.56f, 0.28f, 0.42f},
						{"MainD", "AvantBrasD", 0.28f, 0.42f, 0.29f, 0.35f},
						{"CuisseG", "Hanches", -0.11f, 0.47f, -0.12f, 0.26f},
						{"JambeG", "CuisseG", -0.12f, 0.26f, -0.12f, 0.06f},
						{"PiedG", "JambeG", -0.12f, 0.06f, -0.20f, 0.01f},
						{"CuisseD", "Hanches", 0.11f, 0.47f, 0.12f, 0.26f},
						{"JambeD", "CuisseD", 0.12f, 0.26f, 0.12f, 0.06f},
						{"PiedD", "JambeD", 0.12f, 0.06f, 0.20f, 0.01f},
					};
					return Construire(k, sizeof(k) / sizeof(k[0]), lo, hi, out);
				}
				case NkSkeleton2DTemplate::NK_QUADRUPED: {
					// De profil, tourne vers +X. G et D se recouvrent (decales d'un rien).
					static const OsModele k[] = {
						{"Bassin", nullptr, -0.28f, 0.62f, -0.05f, 0.64f},
						{"Dos", "Bassin", -0.05f, 0.64f, 0.22f, 0.66f},
						{"Cou", "Dos", 0.22f, 0.66f, 0.32f, 0.80f},
						{"Tete", "Cou", 0.32f, 0.80f, 0.48f, 0.76f},
						{"Queue1", "Bassin", -0.28f, 0.62f, -0.40f, 0.66f},
						{"Queue2", "Queue1", -0.40f, 0.66f, -0.49f, 0.60f},
						{"EpauleAvG", "Dos", 0.20f, 0.62f, 0.22f, 0.36f},
						{"PatteAvG", "EpauleAvG", 0.22f, 0.36f, 0.21f, 0.04f},
						{"EpauleAvD", "Dos", 0.17f, 0.62f, 0.19f, 0.36f},
						{"PatteAvD", "EpauleAvD", 0.19f, 0.36f, 0.18f, 0.04f},
						{"CuisseArG", "Bassin", -0.24f, 0.60f, -0.20f, 0.34f},
						{"PatteArG", "CuisseArG", -0.20f, 0.34f, -0.25f, 0.04f},
						{"CuisseArD", "Bassin", -0.27f, 0.60f, -0.23f, 0.34f},
						{"PatteArD", "CuisseArD", -0.23f, 0.34f, -0.28f, 0.04f},
					};
					return Construire(k, sizeof(k) / sizeof(k[0]), lo, hi, out);
				}
				case NkSkeleton2DTemplate::NK_BIRD: {
					static const OsModele k[] = {
						{"Corps", nullptr, -0.18f, 0.42f, 0.16f, 0.50f},
						{"Cou", "Corps", 0.16f, 0.50f, 0.24f, 0.70f},
						{"Tete", "Cou", 0.24f, 0.70f, 0.36f, 0.76f},
						{"Bec", "Tete", 0.36f, 0.76f, 0.49f, 0.72f},
						{"AileG", "Corps", 0.04f, 0.56f, -0.14f, 0.80f},
						{"PlumesG", "AileG", -0.14f, 0.80f, -0.40f, 0.94f},
						{"AileD", "Corps", 0.00f, 0.56f, -0.18f, 0.78f},
						{"PlumesD", "AileD", -0.18f, 0.78f, -0.44f, 0.90f},
						{"Queue", "Corps", -0.18f, 0.42f, -0.48f, 0.36f},
						{"CuisseG", "Corps", 0.02f, 0.40f, 0.04f, 0.20f},
						{"TarseG", "CuisseG", 0.04f, 0.20f, 0.10f, 0.01f},
						{"CuisseD", "Corps", -0.02f, 0.40f, 0.00f, 0.20f},
						{"TarseD", "CuisseD", 0.00f, 0.20f, 0.06f, 0.01f},
					};
					return Construire(k, sizeof(k) / sizeof(k[0]), lo, hi, out);
				}
				case NkSkeleton2DTemplate::NK_CREATURE: {
					// Aucune forme imposee : une racine au centre, le reste se pose.
					static const OsModele k[] = {
						{"Racine", nullptr, 0.f, 0.35f, 0.f, 0.65f},
					};
					return Construire(k, 1u, lo, hi, out);
				}
				default:
					return false;
			}
		}

		// =====================================================================
		// L'IK A DEUX OS
		// =====================================================================
		bool NkSolveTwoBoneIK2D(const NkVec2f &root, float32 l1, float32 l2, const NkVec2f &target, bool bendPositive,
								float32 &angle1, float32 &angle2) {
			const float32 dx = target.x - root.x, dy = target.y - root.y;
			float32 d = std::sqrt(dx * dx + dy * dy);
			const float32 base = d > 1e-7f ? std::atan2(dy, dx) : 0.f;
			const float32 lmax = l1 + l2, lmin = std::fabs(l1 - l2);
			bool atteinte = true;
			if (l1 <= 1e-7f || l2 <= 1e-7f) {
				angle1 = base;
				angle2 = base;
				return false;
			}
			if (d >= lmax) {
				// Hors de portee : la chaine se TEND vers la cible.
				angle1 = base;
				angle2 = base;
				return d <= lmax + 1e-5f;
			}
			if (d < lmin) {
				d = lmin;
				atteinte = d - std::sqrt(dx * dx + dy * dy) < 1e-5f;
			}
			// Loi des cosinus : alpha a la racine, beta au coude (angle interieur).
			float32 ca = (l1 * l1 + d * d - l2 * l2) / (2.f * l1 * d);
			float32 cb = (l1 * l1 + l2 * l2 - d * d) / (2.f * l1 * l2);
			ca = ca < -1.f ? -1.f : (ca > 1.f ? 1.f : ca);
			cb = cb < -1.f ? -1.f : (cb > 1.f ? 1.f : cb);
			const float32 alpha = std::acos(ca), beta = std::acos(cb);
			const float32 sg = bendPositive ? 1.f : -1.f;
			angle1 = NkWrapAngle(base + sg * alpha);
			angle2 = NkWrapAngle(angle1 - sg * (kPi - beta));
			return atteinte;
		}

		bool NkApplyTwoBoneIK2D(const NkSkeleton2D &s, NkBone2D *local, uint32 mid, const NkVec2f &effector, const NkVec2f &target,
								bool bendPositive, float32 weight) {
			const uint32 n = s.Count();
			if (mid >= n || local == nullptr) {
				return false;
			}
			const int32 hautI = s.Parent(mid);
			if (hautI < 0) {
				return false; // il faut DEUX os
			}
			const uint32 haut = (uint32)hautI;
			NkVector<NkMat4f> monde;
			monde.Resize(n);
			s.PoseToWorld(local, monde.Data());
			// La geometrie : racine = tete du bras ; milieu = tete de l'avant-bras ;
			// bout = l'effecteur. Les longueurs viennent de la pose (echelles comprises).
			const NkVec2f racine(monde[haut].m30, monde[haut].m31);
			const NkVec2f milieu(monde[mid].m30, monde[mid].m31);
			const NkVec2f bout = Transformer(monde[mid], effector);
			const float32 l1 = std::sqrt((milieu.x - racine.x) * (milieu.x - racine.x) + (milieu.y - racine.y) * (milieu.y - racine.y));
			const float32 l2 = std::sqrt((bout.x - milieu.x) * (bout.x - milieu.x) + (bout.y - milieu.y) * (bout.y - milieu.y));
			float32 a1 = 0.f, a2 = 0.f;
			const bool ok = NkSolveTwoBoneIK2D(racine, l1, l2, target, bendPositive, a1, a2);
			// Les angles MONDE actuels des deux segments -> la rotation a ajouter.
			const float32 s1 = std::atan2(milieu.y - racine.y, milieu.x - racine.x);
			const float32 s2 = std::atan2(bout.y - milieu.y, bout.x - milieu.x);
			const float32 w = weight < 0.f ? 0.f : (weight > 1.f ? 1.f : weight);
			const float32 d1 = NkWrapAngle(a1 - s1) * w;
			// Le segment 2 tourne deja de d1 avec son parent : il ne lui reste que la difference.
			const float32 d2 = NkWrapAngle((a2 - s2) - (a1 - s1)) * w;
			local[haut].angle = NkWrapAngle(local[haut].angle + d1);
			local[mid].angle = NkWrapAngle(local[mid].angle + d2);
			return ok;
		}

		// =====================================================================
		// LES EMPLACEMENTS D'UNE POSE
		// =====================================================================
		void NkEvaluateSlots2D(const NkSkeleton2D &s, const NkAnimPose &pose, int32 *attachment, int32 *order) {
			for (uint32 i = 0; i < (uint32)s.slots.Size(); ++i) {
				attachment[i] = s.slots[i].attachment;
				order[i] = s.slots[i].order;
			}
			for (uint32 k = 0; k < (uint32)pose.props.Size(); ++k) {
				const NkPropValue &v = pose.props[k];
				NkString nom;
				bool estOrdre = false;
				if (!v.target.Empty() || v.weight <= 0.5f || !NkParseSlot2DProperty(v.property, nom, estOrdre)) {
					continue;
				}
				for (uint32 i = 0; i < (uint32)s.slots.Size(); ++i) {
					if (s.slots[i].name == nom) {
						const int32 x = (int32)std::floor(v.value.x + 0.5f);
						(estOrdre ? order : attachment)[i] = x;
					}
				}
			}
		}

		void NkDrawOrder2D(const int32 *order, uint32 n, uint32 *sorted) {
			for (uint32 i = 0; i < n; ++i) {
				sorted[i] = i;
			}
			// Insertion STABLE : a ordre egal, l'ordre de declaration.
			for (uint32 i = 1; i < n; ++i) {
				const uint32 v = sorted[i];
				uint32 j = i;
				while (j > 0 && order[sorted[j - 1]] > order[v]) {
					sorted[j] = sorted[j - 1];
					--j;
				}
				sorted[j] = v;
			}
		}

	} // namespace anim
} // namespace nkentseu
