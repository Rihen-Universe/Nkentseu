// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkShapeKeys.cpp — les formes (voir NkShapeKeys.h).
// =============================================================================
#include "NKAnima/Morph/NkShapeKeys.h"
#include "NKAnima/Clip/NkAnimation.h"
#include "NKAnima/Rig/NkRigMath.h"
#include "NKAnima/Rig/NkArmature.h"
#include "NKAnima/Skin/NkSkinMesh.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace anim {

		using math::NkMat4f;
		using math::NkVec3f;
		using namespace rigm;

		int32 NkShapeKeySet::Find(const char *name) const {
			if (name == nullptr) {
				return -1;
			}
			for (uint32 k = 0; k < Count(); ++k) {
				if (std::strcmp(keys[k].name.CStr(), name) == 0) {
					return (int32)k;
				}
			}
			return -1;
		}

		void NkShapeKeySet::SetBasis(const NkVec3f *pos, uint32 n) {
			if (keys.Empty() || VertexCount() != n) {
				keys.Clear();
				NkShapeKey b;
				b.name = NkString("Base");
				keys.PushBack(b);
			}
			keys[0].positions.Resize(n);
			for (uint32 i = 0; i < n; ++i) {
				keys[0].positions[i] = pos[i];
			}
		}

		NkString NkShapeKeySet::UniqueName(const char *souhait, int32 sauf) const {
			NkString base(souhait != nullptr && souhait[0] != '\0' ? souhait : "Forme");
			auto Pris = [&](const NkString &n) {
				for (uint32 k = 0; k < Count(); ++k) {
					if ((int32)k != sauf && keys[k].name == n) {
						return true;
					}
				}
				return false;
			};
			if (!Pris(base)) {
				return base;
			}
			for (uint32 i = 1; i < 1000; ++i) {
				char suf[16];
				std::snprintf(suf, sizeof(suf), ".%03u", i);
				NkString essai = base;
				essai.Append(suf);
				if (!Pris(essai)) {
					return essai;
				}
			}
			return base;
		}

		int32 NkShapeKeySet::AddFromBasis(const char *name) {
			if (keys.Empty()) {
				return -1;
			}
			NkShapeKey k;
			k.name = UniqueName(name);
			k.positions = keys[0].positions;
			keys.PushBack(k);
			return (int32)Count() - 1;
		}

		int32 NkShapeKeySet::AddFromPositions(const char *name, const NkVec3f *pos, uint32 n) {
			if (keys.Empty() || n != VertexCount() || pos == nullptr) {
				return -1;
			}
			NkShapeKey k;
			k.name = UniqueName(name);
			k.positions.Resize(n);
			for (uint32 i = 0; i < n; ++i) {
				k.positions[i] = pos[i];
			}
			keys.PushBack(k);
			return (int32)Count() - 1;
		}

		int32 NkShapeKeySet::AddFromMix(const char *name) {
			NkVector<NkVec3f> mix;
			Evaluate(mix);
			return AddFromPositions(name, mix.Data(), (uint32)mix.Size());
		}

		bool NkShapeKeySet::Remove(uint32 k) {
			if (k == 0 || k >= Count()) {
				return false; // la base ne se retire pas
			}
			keys.RemoveAt(k);
			for (uint32 i = 1; i < Count(); ++i) {
				if (keys[i].relativeTo == (int32)k) {
					keys[i].relativeTo = 0;
				} else if (keys[i].relativeTo > (int32)k) {
					keys[i].relativeTo -= 1;
				}
			}
			return true;
		}

		NkString NkShapeKeySet::Rename(uint32 k, const char *name) {
			if (k >= Count()) {
				return NkString();
			}
			keys[k].name = UniqueName(name, (int32)k);
			return keys[k].name;
		}

		void NkShapeKeySet::SetValue(uint32 k, float32 v) {
			if (k == 0 || k >= Count()) {
				return;
			}
			NkShapeKey &s = keys[k];
			s.value = v < s.sliderMin ? s.sliderMin : (v > s.sliderMax ? s.sliderMax : v);
		}

		void NkShapeKeySet::MoveVertices(uint32 k, const uint32 *verts, const float32 *factors, uint32 n, const NkVec3f &delta, bool symetrie,
										 const NkVector<int32> *mirror) {
			if (k >= Count() || verts == nullptr) {
				return;
			}
			NkVector<NkVec3f> &p = keys[k].positions;
			const uint32 nv = (uint32)p.Size();
			const bool miroir = symetrie && mirror != nullptr && (uint32)mirror->Size() == nv;
			// Un sommet deja deplace (choisi ET miroir d'un choisi) ne bouge qu'une fois.
			NkVector<uint8> fait;
			fait.Resize(nv, 0);
			for (uint32 i = 0; i < n; ++i) {
				const uint32 v = verts[i];
				if (v >= nv) {
					continue;
				}
				const float32 f = factors != nullptr ? factors[i] : 1.f;
				if (!fait[v]) {
					p[v] = Add(p[v], Mul(delta, f));
					fait[v] = 1;
				}
				if (miroir) {
					const int32 m = (*mirror)[v];
					if (m >= 0 && (uint32)m != v && !fait[(uint32)m]) {
						p[(uint32)m] = Add(p[(uint32)m], Mul(MiroirX(delta), f));
						fait[(uint32)m] = 1;
					}
				}
			}
		}

		const char *NkShapeBrushName(NkShapeBrush b) {
			switch (b) {
				case NkShapeBrush::NK_ShapeBrush_Saisir: return "Saisir";
				case NkShapeBrush::NK_ShapeBrush_Lisser: return "Lisser";
				case NkShapeBrush::NK_ShapeBrush_Gonfler: return "Gonfler";
				case NkShapeBrush::NK_ShapeBrush_Degonfler: return "Dégonfler";
				case NkShapeBrush::NK_ShapeBrush_Effacer: return "Effacer";
				default: return "?";
			}
		}

		void NkShapeKeySet::SculptVertices(uint32 k, NkShapeBrush brush, const uint32 *verts, const float32 *factors, uint32 n, float32 amount,
										   const NkSkinMesh &mesh, bool symetrie) {
			if (k == 0 || k >= Count() || verts == nullptr || brush == NkShapeBrush::NK_ShapeBrush_Saisir) {
				return;
			}
			NkVector<NkVec3f> &p = keys[k].positions;
			const uint32 nv = (uint32)p.Size();
			const NkShapeKey &s = keys[k];
			const int32 r = (s.relativeTo >= 0 && (uint32)s.relativeTo < Count() && s.relativeTo != (int32)k) ? s.relativeTo : 0;
			const NkVector<NkVec3f> &ref = keys[(uint32)r].positions;
			if ((uint32)ref.Size() != nv) {
				return;
			}
			// Les sommets touches (et leurs miroirs), chacun une fois.
			NkVector<uint32> vs;
			NkVector<float32> fs;
			NkVector<uint8> fait;
			fait.Resize(nv, 0);
			const bool miroir = symetrie && (uint32)mesh.mirror.Size() == nv;
			for (uint32 i = 0; i < n; ++i) {
				const uint32 v = verts[i];
				const float32 f = factors != nullptr ? factors[i] : 1.f;
				if (v >= nv || !(f > 0.f)) {
					continue;
				}
				if (!fait[v]) {
					fait[v] = 1;
					vs.PushBack(v);
					fs.PushBack(f > 1.f ? 1.f : f);
				}
				const int32 m = miroir ? mesh.mirror[v] : -1;
				if (m >= 0 && !fait[(uint32)m]) {
					fait[(uint32)m] = 1;
					vs.PushBack((uint32)m);
					fs.PushBack(f > 1.f ? 1.f : f);
				}
			}
			if (brush == NkShapeBrush::NK_ShapeBrush_Lisser) {
				// Les MOYENNES d'abord, sur les decalages d'avant le coup.
				const bool voisins = (uint32)mesh.rep.Size() == nv && !mesh.adjDebut.Empty();
				NkVector<NkVec3f> moy;
				moy.Resize(vs.Size(), NkVec3f{0.f, 0.f, 0.f});
				for (uint32 i = 0; i < (uint32)vs.Size(); ++i) {
					const uint32 v = vs[i];
					NkVec3f d = Sub(p[v], ref[v]);
					if (voisins) {
						const uint32 rv = mesh.rep[v];
						const uint32 a = mesh.adjDebut[rv], z = mesh.adjDebut[rv + 1u];
						if (z > a) {
							NkVec3f somme{0.f, 0.f, 0.f};
							for (uint32 q = a; q < z; ++q) {
								const uint32 w = mesh.soudes[mesh.adj[q]];
								somme = Add(somme, Sub(p[w], ref[w]));
							}
							d = Mul(somme, 1.f / (float32)(z - a));
						}
					}
					moy[i] = d;
				}
				for (uint32 i = 0; i < (uint32)vs.Size(); ++i) {
					const uint32 v = vs[i];
					const NkVec3f d = Sub(p[v], ref[v]);
					p[v] = Add(ref[v], Lerp(d, moy[i], fs[i]));
				}
				return;
			}
			const bool normales = (uint32)mesh.normals.Size() == nv;
			for (uint32 i = 0; i < (uint32)vs.Size(); ++i) {
				const uint32 v = vs[i];
				if (brush == NkShapeBrush::NK_ShapeBrush_Effacer) {
					p[v] = Lerp(p[v], ref[v], fs[i]);
				} else if (normales) {
					const float32 sens = brush == NkShapeBrush::NK_ShapeBrush_Gonfler ? 1.f : -1.f;
					p[v] = Add(p[v], Mul(mesh.normals[v], sens * amount * fs[i]));
				}
			}
		}

		int32 NkShapeKeySet::Mirror(uint32 k, const NkVector<int32> &mirror, const char *nouveauNom) {
			if (k == 0 || k >= Count() || (uint32)mirror.Size() != VertexCount()) {
				return -1;
			}
			NkString nom = nouveauNom != nullptr && nouveauNom[0] != '\0' ? NkString(nouveauNom) : NkArmature::MirrorName(keys[k].name.CStr());
			if (nom.Empty()) {
				nom = keys[k].name;
				nom.Append(".miroir");
			}
			int32 j = Find(nom.CStr());
			if (j == (int32)k) {
				return -1;
			}
			if (j < 0) {
				j = AddFromBasis(nom.CStr());
			}
			const NkVector<NkVec3f> &base = keys[0].positions;
			const NkVector<NkVec3f> &src = keys[k].positions;
			NkVector<NkVec3f> &dst = keys[(uint32)j].positions;
			// Le delta de chaque sommet passe a son miroir, en miroir.
			dst = base;
			for (uint32 v = 0; v < VertexCount(); ++v) {
				const int32 m = mirror[v];
				if (m < 0) {
					continue;
				}
				const NkVec3f d = Sub(src[v], base[v]);
				dst[(uint32)m] = Add(base[(uint32)m], MiroirX(d));
			}
			keys[(uint32)j].relativeTo = keys[k].relativeTo;
			keys[(uint32)j].sliderMin = keys[k].sliderMin;
			keys[(uint32)j].sliderMax = keys[k].sliderMax;
			return j;
		}

		void NkShapeKeySet::Evaluate(NkVector<NkVec3f> &out, const float32 *values) const {
			if (keys.Empty()) {
				out.Clear();
				return;
			}
			out = keys[0].positions;
			const uint32 nv = VertexCount();
			for (uint32 k = 1; k < Count(); ++k) {
				const NkShapeKey &s = keys[k];
				const float32 v = values != nullptr ? values[k] : s.value;
				if (s.mute || v == 0.f || (uint32)s.positions.Size() != nv) {
					continue;
				}
				const int32 r = (s.relativeTo >= 0 && (uint32)s.relativeTo < Count() && s.relativeTo != (int32)k) ? s.relativeTo : 0;
				const NkVector<NkVec3f> &ref = keys[(uint32)r].positions;
				for (uint32 i = 0; i < nv; ++i) {
					out[i].x += v * (s.positions[i].x - ref[i].x);
					out[i].y += v * (s.positions[i].y - ref[i].y);
					out[i].z += v * (s.positions[i].z - ref[i].z);
				}
			}
		}

		void NkShapeKeySet::Delta(uint32 k, NkVector<NkVec3f> &out) const {
			out.Clear();
			if (k == 0 || k >= Count()) {
				return;
			}
			const NkShapeKey &s = keys[k];
			const int32 r = (s.relativeTo >= 0 && (uint32)s.relativeTo < Count() && s.relativeTo != (int32)k) ? s.relativeTo : 0;
			out.Resize(VertexCount());
			for (uint32 i = 0; i < VertexCount(); ++i) {
				out[i] = Sub(s.positions[i], keys[(uint32)r].positions[i]);
			}
		}

		float32 NkShapeKeySet::DriverAngle(const NkShapeDriver &d, const NkMat4f &restLocal, const NkMat4f &poseLocal) {
			// La rotation du repos a la pose, dans le repere du repos.
			const NkMat4f delta = restLocal.Inverse() * poseLocal;
			float32 qx, qy, qz, qw;
			Quaternion(delta, qx, qy, qz, qw);
			if (qw < 0.f) {
				qx = -qx;
				qy = -qy;
				qz = -qz;
				qw = -qw;
			}
			const float32 k = 57.2957795f;
			if (d.axis >= 3) {
				const float32 w = qw > 1.f ? 1.f : qw;
				return 2.f * std::acos(w) * k;
			}
			// La TORSION autour de l'axe local (decomposition swing-twist).
			const float32 c = d.axis == 0 ? qx : (d.axis == 1 ? qy : qz);
			return 2.f * std::atan2(c, qw) * k;
		}

		uint32 NkShapeKeySet::ApplyDrivers(const NkString *boneNames, const NkMat4f *restLocal, const NkMat4f *poseLocal, uint32 boneCount) {
			uint32 changes = 0;
			for (uint32 k = 1; k < Count(); ++k) {
				NkShapeKey &s = keys[k];
				if (s.driver.kind != NkShapeDriverKind::NK_ShapeDriverKind_Rotation_Os) {
					continue;
				}
				int32 b = -1;
				for (uint32 i = 0; i < boneCount && b < 0; ++i) {
					if (boneNames[i] == s.driver.bone) {
						b = (int32)i;
					}
				}
				if (b < 0) {
					continue;
				}
				const float32 a = DriverAngle(s.driver, restLocal[(uint32)b], poseLocal[(uint32)b]);
				const float32 l = s.driver.angleMax - s.driver.angleMin;
				float32 v = std::fabs(l) > 1e-6f ? (a - s.driver.angleMin) / l : 0.f;
				v = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
				if (std::fabs(v - s.value) > 1e-6f) {
					++changes;
				}
				s.value = v;
			}
			return changes;
		}

		uint32 NkShapeKeySet::ApplyClip(const NkAnimationClip &clip, float32 t) {
			uint32 jouees = 0;
			for (uint32 i = 0; i < (uint32)clip.morphTracks.Size() && i < (uint32)clip.morphNames.Size(); ++i) {
				const int32 k = Find(clip.morphNames[i].CStr());
				if (k <= 0 || clip.morphTracks[i].KeyCount() == 0) {
					continue;
				}
				SetValue((uint32)k, clip.morphTracks[i].Evaluate(t));
				++jouees;
			}
			return jouees;
		}

	} // namespace anim
} // namespace nkentseu
