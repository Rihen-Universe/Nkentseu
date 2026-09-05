// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// NkGarment.cpp — mesures du corps et vêtements procéduraux (en-tête).
#include "NKPhysics/NkGarment.h"
#include "NKMath/NkFunctions.h"
#include <cstring>

namespace nkentseu {
	namespace physics {

		namespace {
			constexpr float32 kPi = 3.14159265358979f;

			NkVec3f JointPos(const NkSkeletonBind &s, int32 j) {
				if (j < 0 || (uint32)j >= s.count)
					return {0.f, 0.f, 0.f};
				return s.world[(uint32)j].TransformPoint(NkVec3f{0.f, 0.f, 0.f});
			}
			NkVec3f Unit(const NkVec3f &v, const NkVec3f &fallback) {
				const float32 l = v.Len();
				return l > 1e-6f ? v * (1.f / l) : fallback;
			}
			float32 Avg(float32 a, float32 b) {
				if (a > 0.f && b > 0.f)
					return 0.5f * (a + b);
				return a > 0.f ? a : b;
			}
			// point de l'axe (o + axis t) à la hauteur y (axis.y != 0), sinon o
			NkVec3f AtHeight(const NkVec3f &o, const NkVec3f &axis, float32 y) {
				if (math::NkAbs(axis.y) < 1e-4f)
					return o;
				return o + axis * ((y - o.y) / axis.y);
			}
		} // namespace

		// ── Mesures ──────────────────────────────────────────────────────────
		bool NkMeasureBody(const NkMannequin &m, const NkHumanoidMap &map, const NkSkeletonBind &skel,
						   NkBodyMeasures &o, uint32 *estimatedRadii) {
			o = NkBodyMeasures{};
			if (estimatedRadii)
				*estimatedRadii = 0;
			const int32 *J = map.joint;
			if (J[NK_HB_HIPS] < 0 || J[NK_HB_CHEST] < 0 || J[NK_HB_L_UPPER_ARM] < 0 || J[NK_HB_R_UPPER_ARM] < 0 ||
				J[NK_HB_L_UPPER_LEG] < 0 || J[NK_HB_R_UPPER_LEG] < 0)
				return false;
			float32 ylo = 1e30f, yhi = -1e30f;
			for (uint32 j = 0; j < skel.count; ++j) {
				const float32 y = JointPos(skel, (int32)j).y;
				if (y < ylo)
					ylo = y;
				if (y > yhi)
					yhi = y;
			}
			o.height = yhi - ylo;
			o.hips = JointPos(skel, J[NK_HB_HIPS]);
			o.chest = JointPos(skel, J[NK_HB_CHEST]);
			o.lUpperArm = JointPos(skel, J[NK_HB_L_UPPER_ARM]);
			o.rUpperArm = JointPos(skel, J[NK_HB_R_UPPER_ARM]);
			o.lShoulder = J[NK_HB_L_SHOULDER] >= 0 ? JointPos(skel, J[NK_HB_L_SHOULDER]) : o.lUpperArm;
			o.rShoulder = J[NK_HB_R_SHOULDER] >= 0 ? JointPos(skel, J[NK_HB_R_SHOULDER]) : o.rUpperArm;
			o.lElbow = J[NK_HB_L_FOREARM] >= 0 ? JointPos(skel, J[NK_HB_L_FOREARM]) : o.lUpperArm;
			o.rElbow = J[NK_HB_R_FOREARM] >= 0 ? JointPos(skel, J[NK_HB_R_FOREARM]) : o.rUpperArm;
			o.lHand = J[NK_HB_L_HAND] >= 0 ? JointPos(skel, J[NK_HB_L_HAND]) : o.lElbow;
			o.rHand = J[NK_HB_R_HAND] >= 0 ? JointPos(skel, J[NK_HB_R_HAND]) : o.rElbow;
			o.lUpLeg = JointPos(skel, J[NK_HB_L_UPPER_LEG]);
			o.rUpLeg = JointPos(skel, J[NK_HB_R_UPPER_LEG]);
			o.lKnee = J[NK_HB_L_LOWER_LEG] >= 0 ? JointPos(skel, J[NK_HB_L_LOWER_LEG]) : o.lUpLeg - NkVec3f{0.f, 0.45f * o.height * 0.5f, 0.f};
			o.rKnee = J[NK_HB_R_LOWER_LEG] >= 0 ? JointPos(skel, J[NK_HB_R_LOWER_LEG]) : o.rUpLeg - NkVec3f{0.f, 0.45f * o.height * 0.5f, 0.f};
			o.lFoot = J[NK_HB_L_FOOT] >= 0 ? JointPos(skel, J[NK_HB_L_FOOT]) : NkVec3f{o.lKnee.x, ylo, o.lKnee.z};
			o.rFoot = J[NK_HB_R_FOOT] >= 0 ? JointPos(skel, J[NK_HB_R_FOOT]) : NkVec3f{o.rKnee.x, ylo, o.rKnee.z};
			o.neck = J[NK_HB_NECK] >= 0 ? JointPos(skel, J[NK_HB_NECK]) : o.chest + NkVec3f{0.f, 0.1f * o.height, 0.f};
			o.head = J[NK_HB_HEAD] >= 0 ? JointPos(skel, J[NK_HB_HEAD]) : o.neck + NkVec3f{0.f, 0.05f * o.height, 0.f};
			o.headTop = J[NK_HB_HEAD_TOP] >= 0 ? JointPos(skel, J[NK_HB_HEAD_TOP]) : NkVec3f{o.head.x, yhi, o.head.z};
			// repère
			o.up = {0.f, 1.f, 0.f};
			NkVec3f r = o.rShoulder - o.lShoulder;
			r = r - o.up * r.Dot(o.up);
			o.right = Unit(r, NkVec3f{-1.f, 0.f, 0.f});
			o.forward = Unit(o.up.Cross(o.right), NkVec3f{0.f, 0.f, 1.f});
			o.shoulderWidth = (o.rShoulder - o.lShoulder).Len();
			o.hipWidth = (o.rUpLeg - o.lUpLeg).Len();
			o.torsoLength = o.neck.y - o.hips.y;
			o.legLength = Avg((o.lFoot - o.lUpLeg).Len(), (o.rFoot - o.rUpLeg).Len());
			o.armLength = Avg((o.lHand - o.lUpperArm).Len(), (o.rHand - o.rUpperArm).Len());
			// rayons depuis les capsules (0 si le mannequin n'en a pas -> estimés)
			auto est = [&](float32 v, float32 fallbackFrac) {
				if (v > 0.f)
					return v;
				if (estimatedRadii)
					++*estimatedRadii;
				return fallbackFrac * o.height;
			};
			o.waistRadius = est(math::NkMax(m.RadiusOf(J[NK_HB_HIPS]), m.RadiusTo(J[NK_HB_SPINE])), 0.085f);
			o.chestRadius = est(math::NkMax(m.RadiusOf(J[NK_HB_CHEST]), m.RadiusTo(J[NK_HB_CHEST])), 0.09f);
			o.neckRadius = est(J[NK_HB_NECK] >= 0 ? math::NkMax(m.RadiusOf(J[NK_HB_NECK]), m.RadiusTo(J[NK_HB_NECK])) : 0.f, 0.035f);
			o.headRadius = est(J[NK_HB_HEAD] >= 0 ? math::NkMax(m.RadiusOf(J[NK_HB_HEAD]), m.RadiusTo(J[NK_HB_HEAD])) : 0.f, 0.06f);
			o.thighRadius = est(Avg(m.RadiusOf(J[NK_HB_L_UPPER_LEG]), m.RadiusOf(J[NK_HB_R_UPPER_LEG])), 0.05f);
			o.calfRadius = est(J[NK_HB_L_LOWER_LEG] >= 0 ? Avg(m.RadiusOf(J[NK_HB_L_LOWER_LEG]), m.RadiusOf(J[NK_HB_R_LOWER_LEG])) : 0.f, 0.035f);
			o.upperArmRadius = est(Avg(m.RadiusOf(J[NK_HB_L_UPPER_ARM]), m.RadiusOf(J[NK_HB_R_UPPER_ARM])), 0.03f);
			o.forearmRadius = est(J[NK_HB_L_FOREARM] >= 0 ? Avg(m.RadiusOf(J[NK_HB_L_FOREARM]), m.RadiusOf(J[NK_HB_R_FOREARM])) : 0.f, 0.025f);
			o.pelvisRadius = est(math::NkMax(m.RadiusTo(J[NK_HB_L_UPPER_LEG]), m.RadiusTo(J[NK_HB_R_UPPER_LEG])), 0.07f);
			o.valid = true;
			return true;
		}

		// ── Noms ─────────────────────────────────────────────────────────────
		const char *NkGarmentCollisionName(NkGarmentCollision c) noexcept {
			switch (c) {
				case NkGarmentCollision::NK_FIELD: return "champ fin";
				case NkGarmentCollision::NK_EXACT: return "distance exacte";
				default: return "auto";
			}
		}

		NkGarmentCollision NkGarmentDefaultCollision(NkGarmentKind k) noexcept {
			switch (k) {
				// serrés, toujours en contact : le champ lisse ce que la normale exacte fait sauter
				case NK_GARMENT_FOULARD: return NkGarmentCollision::NK_FIELD;
				// lâches, ils pendent et battent : la distance exacte ne « respire » pas
				case NK_GARMENT_CAPE:
				case NK_GARMENT_JUPE:
				case NK_GARMENT_ROBE: return NkGarmentCollision::NK_EXACT;
				// t-shirt, chemise, pantalon : entre les deux -- laissés en AUTO, c'est le serrage
				// mesuré qui tranche, et le chiffre est dit dans le journal de la sonde
				default: return NkGarmentCollision::NK_AUTO;
			}
		}

		NkGarmentCollision NkGarmentAutoCollision(float32 meanClearance, float32 threshold) noexcept {
			return meanClearance < threshold ? NkGarmentCollision::NK_FIELD : NkGarmentCollision::NK_EXACT;
		}

		const char *NkGarmentName(NkGarmentKind k) noexcept {
			static const char *kNames[NK_GARMENT_COUNT] = {"cape", "foulard", "jupe", "tshirt", "chemise", "robe", "pantalon"};
			return k < NK_GARMENT_COUNT ? kNames[k] : "?";
		}

		bool NkGarmentFromName(const char *name, NkGarmentKind &out) noexcept {
			if (!name)
				return false;
			for (uint32 k = 0; k < NK_GARMENT_COUNT; ++k)
				if (std::strcmp(name, NkGarmentName((NkGarmentKind)k)) == 0) {
					out = (NkGarmentKind)k;
					return true;
				}
			if (std::strcmp(name, "t-shirt") == 0) {
				out = NK_GARMENT_TSHIRT;
				return true;
			}
			return false;
		}

		// ── Briques ──────────────────────────────────────────────────────────
		void NkGarment::RingRows(const NkVec3f &top, const NkVec3f &axisDown, const NkVec3f &e1, const NkVec3f &e2,
								 float32 rTop, float32 rBot, float32 length, float32 spacing, uint32 &nAround,
								 uint32 &nDown, NkVector<NkVec3f> &rows, NkVector<NkVec3f> &dirs, float32 tilt) const {
			const float32 sp = spacing > 1e-4f ? spacing : 0.03f;
			const float32 rMax = math::NkMax(rTop, rBot);
			nAround = (uint32)(2.f * kPi * rMax / sp + 0.5f);
			if (nAround < 8)
				nAround = 8;
			nDown = (uint32)(length / sp + 0.5f) + 1u;
			if (nDown < 2)
				nDown = 2;
			rows.Resize(nAround * nDown);
			dirs.Resize(nAround * nDown);
			for (uint32 j = 0; j < nDown; ++j) {
				const float32 f = (float32)j / (float32)(nDown - 1);
				const NkVec3f c = top + axisDown * (length * f);
				const float32 r = rTop + (rBot - rTop) * f;
				for (uint32 i = 0; i < nAround; ++i) {
					const float32 a = 2.f * kPi * (float32)i / (float32)nAround;
					const NkVec3f u = e1 * math::NkCos(a) + e2 * math::NkSin(a);
					rows[j * nAround + i] = c + u * r;
					dirs[j * nAround + i] = tilt != 0.f ? Unit(u - axisDown * tilt, u) : u;
				}
			}
		}

		uint32 NkGarment::Tube(const NkVec3f &top, const NkVec3f &axisDown, const NkVec3f &e1, const NkVec3f &e2,
							   float32 rTop, float32 rBot, float32 length, const NkGarmentParams &p, uint32 &nAround,
							   uint32 &nDown, float32 tilt) {
			NkVector<NkVec3f> rows, dirs;
			RingRows(top, axisDown, e1, e2, rTop, rBot, length, p.spacing, nAround, nDown, rows, dirs, tilt);
			FitOutside(rows.Data(), dirs.Data(), (uint32)rows.Size());
			const float32 area = kPi * (rTop + rBot) * length; // tronc de cône, à l'apothème près
			const float32 mass = p.areaDensity * area;
			this->mass += mass;
			++pieces;
			return cloth.AppendPanel(nAround, nDown, rows.Data(), mass, true);
		}

		template <class Keep>
		uint32 NkGarment::TubeMasked(const NkVec3f &top, const NkVec3f &axisDown, const NkVec3f &e1, const NkVec3f &e2,
									 float32 rTop, float32 rBot, float32 length, const NkGarmentParams &p, uint32 &nAround,
									 uint32 &nDown, Keep keep, NkVector<int32> &indexOf) {
			NkVector<NkVec3f> rows, dirs;
			RingRows(top, axisDown, e1, e2, rTop, rBot, length, p.spacing, nAround, nDown, rows, dirs);
			NkVector<uint8> mask;
			mask.Resize(nAround * nDown);
			uint32 present = 0;
			for (uint32 j = 0; j < nDown; ++j)
				for (uint32 i = 0; i < nAround; ++i) {
					const bool k = keep(i, j, rows[j * nAround + i]);
					mask[j * nAround + i] = k ? 1 : 0;
					if (k)
						++present;
				}
			FitOutside(rows.Data(), dirs.Data(), (uint32)rows.Size());
			const float32 area = kPi * (rTop + rBot) * length * (float32)present / (float32)(nAround * nDown);
			const float32 mass = p.areaDensity * area;
			this->mass += mass;
			++pieces;
			return cloth.AppendPanelMasked(nAround, nDown, rows.Data(), mask.Data(), mass, true, &indexOf);
		}

		uint32 NkGarment::SewLists(const NkVector<uint32> &a, const NkVector<uint32> &b) {
			const NkVec3f *X = cloth.Positions();
			uint32 n = 0;
			for (uint32 i = 0; i < (uint32)a.Size(); ++i) {
				uint32 best = 0;
				float32 bd = 1e30f;
				for (uint32 k = 0; k < (uint32)b.Size(); ++k) {
					const float32 d = (X[a[i]] - X[b[k]]).LenSq();
					if (d < bd) {
						bd = d;
						best = b[k];
					}
				}
				if (bd > 1e-8f && (uint32)b.Size() > 0) {
					cloth.AddDistance(a[i], best, NkCloth::STRUCTURAL);
					++n;
				}
			}
			seams += n;
			return n;
		}

		void NkGarment::FitAgainst(int32 rootJoint, const NkSkeletonBind &skel, int32 excludeA, int32 excludeB) {
			const uint32 n = (uint32)mFitShapes.Size();
			mFitOn.Resize(n);
			auto under = [&](int32 j, int32 root) {
				uint32 guard = 0;
				while (j >= 0 && (uint32)j < skel.count && guard++ < skel.count) {
					if (j == root)
						return true;
					j = skel.parent[(uint32)j];
				}
				return false;
			};
			for (uint32 k = 0; k < n; ++k) {
				const int32 j = mFitJoint[k];
				bool on = rootJoint < 0 || under(j, rootJoint);
				if (on && ((excludeA >= 0 && under(j, excludeA)) || (excludeB >= 0 && under(j, excludeB))))
					on = false;
				mFitOn[k] = on ? 1 : 0;
			}
		}

		bool NkGarment::InsideAny(const NkVec3f &p, NkVec3f *outNearest) const {
			const uint32 nk = (uint32)mFitShapes.Size();
			bool inside = false;
			float32 bestDepth = -1.f;
			for (uint32 k = 0; k < nk; ++k) {
				if (!mFitOn[k])
					continue;
				const collision::NkShape &s = mFitShapes[k];
				if (s.type != collision::NkShapeType::NK_SPHERE && s.type != collision::NkShapeType::NK_CAPSULE3D)
					continue;
				const NkVec3f ab = s.p1 - s.p0;
				const float32 ab2 = ab.Dot(ab);
				NkVec3f c = s.p0;
				if (s.type == collision::NkShapeType::NK_CAPSULE3D && ab2 > 1e-12f) {
					float32 t = (p - s.p0).Dot(ab) / ab2;
					t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
					c = s.p0 + ab * t;
				}
				const NkVec3f d = p - c;
				const float32 l = d.Len();
				const float32 R = s.radius + mFitMargin;
				if (l < R) {
					inside = true;
					if (outNearest && R - l > bestDepth) {
						bestDepth = R - l;
						*outNearest = c + (l > 1e-6f ? d * (1.f / l) : NkVec3f{0.f, 1.f, 0.f}) * R;
					}
				}
			}
			return inside;
		}

		void NkGarment::FitOutside(NkVec3f *pts, const NkVec3f *dirs, uint32 count) const {
			if (mFitShapes.Empty() && !mFitMesh)
				return;
			const float32 step = 0.002f; // 2 mm par pas de marche, 15 cm au plus
			// hors des capsules ET hors du maillage (en-tête de Build)
			auto inside = [&](const NkVec3f &q) { return InsideAny(q) || (mFitMesh && mFitMesh->Inside(q)); };
			for (uint32 i = 0; i < count; ++i) {
				const NkVec3f start = pts[i];
				bool out = false;
				for (uint32 m = 0; m < 75; ++m) {
					if (!inside(pts[i])) {
						out = true;
						break;
					}
					pts[i] += dirs[i] * step;
				}
				if (out)
					continue;
				// repli : la direction suit un os ; projeter vers la surface la plus proche (4 passes)
				pts[i] = start;
				for (uint32 pass = 0; pass < 4; ++pass) {
					NkVec3f nearest;
					if (!InsideAny(pts[i], &nearest))
						break;
					pts[i] = nearest;
				}
			}
		}

		void NkGarment::PinRow(uint32 first, uint32 count, int32 bone, const NkSkeletonBind &skel) {
			if (bone < 0 || (uint32)bone >= skel.count)
				return;
			const math::NkMat4f inv = skel.world[(uint32)bone].Inverse();
			for (uint32 i = 0; i < count; ++i) {
				NkGarmentPin pin;
				pin.particle = first + i;
				pin.bone = bone;
				pin.local = inv.TransformPoint(cloth.Positions()[first + i]);
				cloth.Pin(first + i, true);
				pins.PushBack(pin);
			}
		}

		uint32 NkGarment::Sew(uint32 firstA, uint32 countA, uint32 firstB, uint32 countB) {
			const NkVec3f *X = cloth.Positions();
			uint32 n = 0;
			for (uint32 a = 0; a < countA; ++a) {
				uint32 best = firstB;
				float32 bd = 1e30f;
				for (uint32 b = 0; b < countB; ++b) {
					const float32 d = (X[firstA + a] - X[firstB + b]).LenSq();
					if (d < bd) {
						bd = d;
						best = firstB + b;
					}
				}
				if (bd > 1e-8f) { // deux particules confondues : pas de direction pour la contrainte
					cloth.AddDistance(firstA + a, best, NkCloth::STRUCTURAL);
					++n;
				}
			}
			seams += n;
			return n;
		}

		// racine du sous-arbre d'un bras : l'épaule si elle existe, sinon le haut du bras
		static int32 ArmRoot(const NkHumanoidMap &map, uint32 side) {
			const int32 sh = map.joint[side == 0 ? NK_HB_L_SHOULDER : NK_HB_R_SHOULDER];
			return sh >= 0 ? sh : map.joint[side == 0 ? NK_HB_L_UPPER_ARM : NK_HB_R_UPPER_ARM];
		}

		// ── Vêtements ────────────────────────────────────────────────────────
		bool NkGarment::Build(NkGarmentKind k, const NkBodyMeasures &b, const NkHumanoidMap &map,
							  const NkSkeletonBind &skel, const NkGarmentParams &p, const NkMannequin *mannequin,
							  const NkMeshInsideTester *bodyMesh) {
			kind = k;
			collision = NkGarmentDefaultCollision(k);
			cloth.Clear();
			pins.Clear();
			seams = 0;
			pieces = 0;
			mass = 0.f;
			mFitShapes.Clear();
			mFitJoint.Clear();
			mFitOn.Clear();
			if (!b.valid)
				return false;
			mFitMargin = p.thickness + 0.5f * p.ease;
			mFitMesh = bodyMesh;
			if (mannequin) {
				mannequin->Pose(skel.world, skel.count, mFitShapes);
				for (uint32 c = 0; c < mannequin->CapsuleCount() && c < (uint32)mFitShapes.Size(); ++c)
					mFitJoint.PushBack(mannequin->Capsule(c).joint);
				while ((uint32)mFitJoint.Size() < (uint32)mFitShapes.Size())
					mFitJoint.PushBack(-1);
				for (uint32 c = 0; c < (uint32)mFitShapes.Size(); ++c)
					cloth.colliders.PushBack(mFitShapes[c]);
			}
			FitAgainst(-1, skel);
			cloth.params.thickness = p.thickness;
			cloth.params.friction = p.friction;
			cloth.params.damping = p.damping;
			cloth.params.substeps = p.substeps;
			cloth.params.iterations = p.iterations;
			cloth.params.selfCollision = p.selfCollision;
			// Le rythme des colliders SUIT la methode, mesure le 05/09 sur la meme course :
			//  - distance exacte (piece lache) : une fois par sous-pas suffit -- la cape passe de
			//    28,5 a 8,52 ms avec la meme physique (etirement moyen 1,58 -> 1,35 %) ;
			//  - champ fin (piece serree) : il faut CHAQUE iteration -- le foulard passe de 0,18 a
			//    13,91 % d etirement et de 0 a 3 particules sous la peau si on l espace. Il touche
			//    en permanence : entre deux resolutions, les contraintes le retirent dans le corps.
			cloth.params.collidersEveryIteration = (collision != NkGarmentCollision::NK_EXACT);
			const int32 *J = map.joint;
			const NkVec3f down = b.up * -1.f;
			const float32 sp = p.spacing;
			const float32 kneeY = 0.5f * (b.lKnee.y + b.rKnee.y);
			const float32 shoulderY = 0.5f * (b.lShoulder.y + b.rShoulder.y);
			uint32 na = 0, nd = 0;
			switch (k) {
				case NK_GARMENT_CAPE: {
					const float32 w = b.shoulderWidth + 0.20f;
					const float32 L = (b.neck.y - 0.02f) - (kneeY - 0.05f);
					const uint32 nx = (uint32)(w / sp + 0.5f) + 1u, ny = (uint32)(L / sp + 0.5f) + 1u;
					const NkVec3f back = b.forward * -(b.chestRadius + p.ease + 0.02f);
					const NkVec3f origin = NkVec3f{b.chest.x, b.neck.y - 0.02f, b.chest.z} + back - b.right * (0.5f * w);
					const float32 m = p.areaDensity * w * L;
					mass += m;
					++pieces;
					NkVector<NkVec3f> rows, dirs;
					rows.Resize(nx * ny);
					dirs.Resize(nx * ny, b.forward * -1.f); // la cape s'écarte du dos
					for (uint32 j = 0; j < ny; ++j)
						for (uint32 i = 0; i < nx; ++i)
							rows[j * nx + i] = origin + b.right * (sp * (float32)i) + down * (sp * (float32)j);
					FitOutside(rows.Data(), dirs.Data(), (uint32)rows.Size());
					const uint32 first = cloth.AppendPanel(nx, ny, rows.Data(), m, false);
					PinRow(first, nx, J[NK_HB_CHEST], skel);
					break;
				}
				case NK_GARMENT_FOULARD: {
					const int32 bone = J[NK_HB_NECK] >= 0 ? J[NK_HB_NECK] : J[NK_HB_CHEST];
					// col droit de 9 cm (quatre rangées), du milieu du cou jusqu'aux épaules ; les rangées
					// basses qui rencontrent les épaules sont poussées vers le haut par l'ajustement (repli).
					// Un col évasé sur des bras en T-pose traversait les capsules des bras (mesuré, 295 %).
					const float32 rTop = b.neckRadius + p.ease;
					const NkVec3f top = b.neck + b.up * 0.05f;
					const uint32 first = Tube(top, down, b.right, b.forward, rTop, rTop + 0.02f, 0.09f, p, na, nd, 1.f);
					PinRow(first, na, bone, skel);
					break;
				}
				case NK_GARMENT_JUPE: {
					const float32 rTop = math::NkMax(b.waistRadius, 0.5f * b.hipWidth + b.thighRadius) + p.ease;
					const NkVec3f top = b.hips + b.up * 0.06f;
					const float32 L = top.y - (kneeY - 0.03f);
					const uint32 first = Tube(top, down, b.right, b.forward, rTop, rTop * 1.5f, L, p, na, nd);
					PinRow(first, na, J[NK_HB_HIPS], skel);
					break;
				}
				case NK_GARMENT_TSHIRT:
				case NK_GARMENT_CHEMISE:
				case NK_GARMENT_ROBE: {
					const bool longSleeve = (k == NK_GARMENT_CHEMISE);
					// CORPS : un tube À EMMANCHURES. Un anneau plein à la hauteur des épaules est
					// traversé par les bras en T-pose (mesuré : 55 mm de pénétration sur une épingle,
					// 108 % d'étirement) ; un vrai t-shirt a deux trous. Le tube part AU-DESSUS des
					// épaules (rayon de l'épaule + marge) : sa rangée haute est l'encolure-épaules,
					// épinglée à la poitrine ; les cases au-dessus de l'aisselle ET au-delà du départ
					// du bras sont retirées : c'est l'emmanchure, dont le bord reçoit la manche.
					const float32 shoulderR = math::NkMax(b.upperArmRadius, 0.04f);
					const float32 topY = shoulderY + shoulderR + p.ease;
					const float32 armpitY = shoulderY - b.upperArmRadius - p.ease;
					// l'emmanchure commence là où la capsule du bras commence (départ du bras - son rayon)
					const float32 armStartX = math::NkMin(math::NkAbs(b.lUpperArm.x - b.chest.x), math::NkAbs(b.rUpperArm.x - b.chest.x)) -
											  b.upperArmRadius;
					const NkVec3f top{b.chest.x, topY, b.chest.z};
					const float32 bottomY = (k == NK_GARMENT_CHEMISE) ? b.hips.y - 0.04f : b.hips.y + 0.03f;
					const float32 L = top.y - bottomY;
					const float32 rTop = b.chestRadius + p.ease, rBot = math::NkMax(b.waistRadius, b.chestRadius * 0.9f) + p.ease;
					NkVector<int32> bodyMap;
					const NkVec3f chestC = b.chest;
					const NkVec3f rightDir = b.right;
					const uint32 body = TubeMasked(
						top, down, b.right, b.forward, rTop, rBot, L, p, na, nd,
						[&](uint32, uint32, const NkVec3f &q) {
							const float32 lateral = math::NkAbs((q - chestC).Dot(rightDir));
							return !(q.y > armpitY && lateral > armStartX - p.ease); // emmanchure
						},
						bodyMap);
					const uint32 bodyAround = na, bodyDown = nd;
					// épingles : la rangée haute (celles qui existent)
					for (uint32 i = 0; i < bodyAround; ++i)
						if (bodyMap[i] >= 0) {
							PinRow((uint32)bodyMap[i], 1, J[NK_HB_CHEST], skel);
						}
					// manches : un tube autour de chaque bras, cousu au BORD de l'emmanchure
					for (uint32 side = 0; side < 2; ++side) {
						const NkVec3f ua = side == 0 ? b.lUpperArm : b.rUpperArm;
						const NkVec3f el = side == 0 ? b.lElbow : b.rElbow;
						const NkVec3f ha = side == 0 ? b.lHand : b.rHand;
						const NkVec3f axis = Unit(el - ua, side == 0 ? b.right * -1.f : b.right);
						const float32 upper = (el - ua).Len();
						const float32 len = longSleeve ? (ha - ua).Len() - 0.03f : 0.55f * upper;
						const float32 r0 = b.upperArmRadius + p.ease + 0.01f;
						const float32 r1 = longSleeve ? b.forearmRadius + p.ease + 0.01f : r0;
						NkVec3f e1 = Unit(b.forward - axis * b.forward.Dot(axis), b.forward);
						NkVec3f e2 = Unit(axis.Cross(e1), b.up);
						FitAgainst(J[side == 0 ? NK_HB_L_UPPER_ARM : NK_HB_R_UPPER_ARM], skel); // la manche contre SON bras
						const uint32 sleeve = Tube(ua + axis * 0.02f, axis, e1, e2, r0, r1, len, p, na, nd);
						FitAgainst(-1, skel);
						// bord de l'emmanchure de ce côté : cases présentes voisines d'une case absente
						NkVector<uint32> edge, ring;
						for (uint32 i = 0; i < na; ++i)
							ring.PushBack(sleeve + i);
						const float32 sideSign = side == 0 ? 1.f : -1.f;
						for (uint32 j = 0; j < bodyDown; ++j)
							for (uint32 i = 0; i < bodyAround; ++i) {
								const int32 me = bodyMap[j * bodyAround + i];
								if (me < 0)
									continue;
								const float32 lat = (cloth.Positions()[(uint32)me] - chestC).Dot(rightDir) * -sideSign;
								if (lat < 0.f)
									continue; // l'autre côté
								const int32 nb[4] = {bodyMap[j * bodyAround + (i + 1) % bodyAround],
													 bodyMap[j * bodyAround + (i + bodyAround - 1) % bodyAround],
													 j > 0 ? bodyMap[(j - 1) * bodyAround + i] : me,
													 j + 1 < bodyDown ? bodyMap[(j + 1) * bodyAround + i] : me};
								if (nb[0] < 0 || nb[1] < 0 || nb[2] < 0 || nb[3] < 0)
									edge.PushBack((uint32)me);
							}
						SewLists(ring, edge);
					}
					if (k == NK_GARMENT_ROBE) {
						// jupe non épinglée, cousue au bas du corsage
						const float32 rS = math::NkMax(b.waistRadius, 0.5f * b.hipWidth + b.thighRadius) + p.ease;
						const NkVec3f stop = NkVec3f{b.hips.x, bottomY - 0.5f * sp, b.hips.z};
						const float32 Ls = stop.y - (kneeY - 0.03f);
						const uint32 skirt = Tube(stop, down, b.right, b.forward, rS, rS * 1.6f, Ls, p, na, nd);
						Sew(skirt, na, body + bodyAround * (bodyDown - 1), bodyAround);
					}
					break;
				}
				case NK_GARMENT_PANTALON: {
					// la ceinture est un EMPIÈCEMENT : de la taille jusque SOUS le bassin (mesuré : des
					// jambes qui partaient dans les capsules hanches -> cuisse voyaient leur paroi interne
					// marcher au-delà de la ligne médiane, dans l'autre cuisse : 320 % d'étirement)
					const float32 rBand = math::NkMax(math::NkMax(b.waistRadius, 0.5f * b.hipWidth + b.thighRadius), b.pelvisRadius) + p.ease;
					const NkVec3f top = b.hips + b.up * 0.06f;
					const float32 bandL = top.y - (b.hips.y - b.pelvisRadius - p.ease - 0.01f);
					const uint32 band = Tube(top, down, b.right, b.forward, rBand, rBand, bandL, p, na, nd);
					const uint32 bandAround = na, bandDown = nd;
					PinRow(band, bandAround, J[NK_HB_HIPS], skel);
					const float32 yBandBottom = top.y - bandL;
					for (uint32 side = 0; side < 2; ++side) {
						const NkVec3f ul = side == 0 ? b.lUpLeg : b.rUpLeg;
						const NkVec3f ft = side == 0 ? b.lFoot : b.rFoot;
						const NkVec3f axis = Unit(ft - ul, down);
						const float32 r0 = b.thighRadius + p.ease + 0.01f, r1 = b.calfRadius + p.ease + 0.01f;
						// deux tubes de rayon r0 centrés sur des cuisses à hipWidth / 2 de l'axe se
						// croisent dans la cuisse opposée (mesuré : 95-110 % d'étirement) : chaque tube est
						// décalé vers l'extérieur pour que sa paroi interne soit sur la ligne médiane
						const float32 shift = math::NkMax(0.f, r0 - 0.5f * b.hipWidth);
						const NkVec3f outward = (side == 0 ? b.right * -1.f : b.right) * shift;
						const NkVec3f legTop = AtHeight(ul, axis, yBandBottom - 0.5f * sp) + outward;
						const float32 len = (ft + outward - legTop).Len() - 0.06f;
						NkVec3f e1 = Unit(b.right - axis * b.right.Dot(axis), b.right);
						NkVec3f e2 = Unit(axis.Cross(e1), b.forward);
						FitAgainst(J[side == 0 ? NK_HB_L_UPPER_LEG : NK_HB_R_UPPER_LEG], skel); // la jambe contre SA jambe
						const uint32 leg = Tube(legTop, axis, e1, e2, r0, r1, len, p, na, nd);
						FitAgainst(-1, skel);
						Sew(leg, na, band + bandAround * (bandDown - 1), bandAround);
					}
					break;
				}
				default:
					return false;
			}
			return cloth.ParticleCount() > 0;
		}

		void NkGarment::UpdatePins(const math::NkMat4f *jointWorld, uint32 jointCount) {
			for (uint32 k = 0; k < (uint32)pins.Size(); ++k) {
				const NkGarmentPin &pin = pins[k];
				if (pin.bone < 0 || (uint32)pin.bone >= jointCount)
					continue;
				cloth.SetPinTarget(pin.particle, jointWorld[(uint32)pin.bone].TransformPoint(pin.local));
			}
		}

		void NkGarment::SnapPins(const math::NkMat4f *jointWorld, uint32 jointCount) {
			for (uint32 k = 0; k < (uint32)pins.Size(); ++k) {
				const NkGarmentPin &pin = pins[k];
				if (pin.bone < 0 || (uint32)pin.bone >= jointCount)
					continue;
				const NkVec3f t = jointWorld[(uint32)pin.bone].TransformPoint(pin.local);
				cloth.SetPosition(pin.particle, t);
				cloth.SetPinTarget(pin.particle, t);
			}
		}

		// ── Chapeau ──────────────────────────────────────────────────────────
		bool NkHat::Build(const NkBodyMeasures &b, const NkHumanoidMap &map, const NkSkeletonBind &skel) {
			bone = map.joint[NK_HB_HEAD] >= 0 ? map.joint[NK_HB_HEAD] : map.joint[NK_HB_NECK];
			if (!b.valid || bone < 0 || (uint32)bone >= skel.count)
				return false;
			radius = b.headRadius + 0.02f;
			height = 0.12f;
			// le chapeau est posé sur le sommet de la tête, son centre à mi-hauteur
			const NkVec3f center = b.headTop - b.up * (0.25f * radius) + b.up * (0.5f * height);
			local = skel.world[(uint32)bone].Inverse() * math::NkMat4f::Translate(center);
			return true;
		}

		math::NkMat4f NkHat::World(const math::NkMat4f *jointWorld, uint32 jointCount) const {
			if (bone < 0 || (uint32)bone >= jointCount)
				return math::NkMat4f::Identity();
			return jointWorld[(uint32)bone] * local;
		}

	} // namespace physics
} // namespace nkentseu
