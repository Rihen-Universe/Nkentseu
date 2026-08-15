// =============================================================================
// NKAnimPhysics/NkContactDetector.cpp — impl (voir NkContactDetector.h)
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#include "NKAnimPhysics/NkContactDetector.h"
#include "NKAnimPhysics/NkPoseMass.h"
#include "NKAnimPhysics/NkBalance.h"

namespace nkentseu {
	namespace animphys {

		using math::NkVec3f;

		static inline float32 Dot3(const NkVec3f &a, const NkVec3f &b) {
			return a.x * b.x + a.y * b.y + a.z * b.z;
		}

		static inline NkVec3f Normalize3(const NkVec3f &a) {
			const float32 l2 = Dot3(a, a);
			if (l2 <= 1e-12f)
				return NkVec3f{0.f, 1.f, 0.f};
			const float32 inv = 1.0f / math::NkSqrt(l2);
			return NkVec3f{a.x * inv, a.y * inv, a.z * inv};
		}

		NkGroundContact NkContactDetector::DetectPlane(const NkVec3f &footWorld, const NkVec3f &planePoint,
													   const NkVec3f &planeNormal, float32 threshold) {
			NkGroundContact c;
			const NkVec3f n = Normalize3(planeNormal);
			// distance signée au plan (>0 au-dessus dans le sens de la normale).
			const NkVec3f rel{footWorld.x - planePoint.x, footWorld.y - planePoint.y, footWorld.z - planePoint.z};
			const float32 signedDist = Dot3(rel, n);
			c.inContact = (signedDist <= threshold);
			c.penetration = -signedDist; // >0 si sous le sol
			// projection orthogonale de l'extrémité sur le plan.
			c.point = NkVec3f{footWorld.x - signedDist * n.x, footWorld.y - signedDist * n.y,
							  footWorld.z - signedDist * n.z};
			return c;
		}

		int32 NkContactDetector::DetectSupportPoints(const NkVec3f *footWorld, int32 footCount,
													 const NkVec3f &planePoint, const NkVec3f &planeNormal,
													 float32 threshold, NkVector<NkVec3f> &outSupport,
													 NkVector<NkGroundContact> *outContacts) {
			outSupport.Clear();
			if (outContacts)
				outContacts->Clear();
			if (!footWorld || footCount <= 0)
				return 0;
			int32 nContact = 0;
			for (int32 i = 0; i < footCount; ++i) {
				NkGroundContact c = DetectPlane(footWorld[i], planePoint, planeNormal, threshold);
				if (outContacts)
					outContacts->PushBack(c);
				if (c.inContact) {
					outSupport.PushBack(c.point);
					++nContact;
				}
			}
			return nContact;
		}

		bool NkContactDetector::SelfTest() {
			const float32 eps = 1e-3f;
			const NkVec3f groundPt{0.f, 0.f, 0.f};
			const NkVec3f up{0.f, 1.f, 0.f};
			const float32 thr = 0.05f;

			// 1) Pied sous le sol -> contact, pénétration > 0, point projeté à y=0.
			{
				NkGroundContact c = DetectPlane(NkVec3f{0.3f, -0.1f, 0.2f}, groundPt, up, thr);
				if (!c.inContact)
					return false;
				if (c.penetration < 0.1f - eps || c.penetration > 0.1f + eps)
					return false;
				if (c.point.y > eps || c.point.y < -eps)
					return false;
				if (c.point.x < 0.3f - eps || c.point.z < 0.2f - eps)
					return false; // x/z inchangés
			}

			// 2) Pied au-dessus du seuil -> pas de contact.
			{
				NkGroundContact c = DetectPlane(NkVec3f{0.f, 0.5f, 0.f}, groundPt, up, thr);
				if (c.inContact)
					return false;
			}

			// 3) Pied juste au sol -> contact, pénétration ~0.
			{
				NkGroundContact c = DetectPlane(NkVec3f{0.f, 0.f, 0.f}, groundPt, up, thr);
				if (!c.inContact)
					return false;
				if (c.penetration > eps || c.penetration < -eps)
					return false;
			}

			// 4) 3 pieds dont 1 en l'air -> 2 points de support.
			{
				NkVec3f feet[3] = {NkVec3f{-0.2f, 0.f, 0.f}, NkVec3f{0.2f, 0.f, 0.f}, NkVec3f{0.f, 0.8f, 0.f}};
				NkVector<NkVec3f> sup;
				int32 n = DetectSupportPoints(feet, 3, groundPt, up, thr, sup);
				if (n != 2 || (int32)sup.Size() != 2)
					return false;
			}

			// 5) INTÉGRATION M3.1+M3.2+M3.3 : figure debout -> équilibrée ; penchée trop loin -> pas.
			{
				// Corps (positions monde) + noms pour la masse anthropométrique.
				NkVec3f body[5] = {NkVec3f{0.f, 1.0f, 0.f}, NkVec3f{0.f, 1.3f, 0.f}, NkVec3f{0.f, 1.7f, 0.f},
								   NkVec3f{-0.1f, 0.6f, 0.f}, NkVec3f{0.1f, 0.6f, 0.f}};
				NkVector<NkString> names;
				names.PushBack(NkString("Hips"));
				names.PushBack(NkString("Spine"));
				names.PushBack(NkString("Head"));
				names.PushBack(NkString("LeftUpLeg"));
				names.PushBack(NkString("RightUpLeg"));
				NkPoseMass pm;
				pm.SetAnthropometric(names);

				// 4 coins d'appui au sol (deux pieds avec une aire).
				NkVec3f feet[4] = {NkVec3f{-0.15f, 0.f, 0.1f}, NkVec3f{-0.15f, 0.f, -0.1f},
								   NkVec3f{0.15f, 0.f, 0.1f}, NkVec3f{0.15f, 0.f, -0.1f}};
				NkVector<NkVec3f> sup;
				const int32 n = DetectSupportPoints(feet, 4, groundPt, up, thr, sup);
				if (n != 4)
					return false;

				// Debout : COM au-dessus des pieds -> équilibré.
				NkVec3f com = pm.ComputeCOMFromPositions(body, 5);
				NkBalanceResult r = NkBalance::EvaluateStatic(com, sup.Data(), (int32)sup.Size(), up);
				if (!r.balanced)
					return false;

				// Penché : décale tout le corps de +1 m en x -> COM hors du polygone -> déséquilibré.
				NkVec3f leaned[5];
				for (int32 i = 0; i < 5; ++i)
					leaned[i] = NkVec3f{body[i].x + 1.0f, body[i].y, body[i].z};
				NkVec3f com2 = pm.ComputeCOMFromPositions(leaned, 5);
				NkBalanceResult r2 = NkBalance::EvaluateStatic(com2, sup.Data(), (int32)sup.Size(), up);
				if (r2.balanced)
					return false;
			}

			return true;
		}

	} // namespace animphys
} // namespace nkentseu
