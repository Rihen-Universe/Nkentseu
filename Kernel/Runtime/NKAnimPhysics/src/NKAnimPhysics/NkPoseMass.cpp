// =============================================================================
// NKAnimPhysics/NkPoseMass.cpp — implémentation (voir NkPoseMass.h)
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#include "NKAnimPhysics/NkPoseMass.h"

namespace nkentseu {
	namespace animphys {

		using math::NkMat4f;
		using math::NkVec3f;

		// Minuscule ASCII d'un octet.
		static inline char LowerAscii(char c) {
			if (c >= 'A' && c <= 'Z')
				return (char)(c - 'A' + 'a');
			return c;
		}

		// `name` contient-il `kw` (kw en minuscules), en insensible à la casse ?
		static bool HasKw(const NkString &name, const char *kw) {
			const char *hay = name.Data();
			const int64 n = (int64)name.Size();
			int64 klen = 0;
			while (kw[klen] != '\0')
				++klen;
			if (klen == 0 || n < klen)
				return false;
			for (int64 i = 0; i + klen <= n; ++i) {
				int64 j = 0;
				while (j < klen && LowerAscii(hay[i + j]) == kw[j])
					++j;
				if (j == klen)
					return true;
			}
			return false;
		}

		// Poids de masse relatif d'un joint d'après son nom (fractions type Dempster,
		// exprimées en poids relatifs : seuls les RATIOS comptent pour le COM). Ordre
		// des tests = du plus spécifique au plus général (ex. "upperleg" avant "leg").
		static float32 MassWeightForName(const NkString &nm) {
			// Mains / avant-bras / bras (test avant "arm" générique).
			if (HasKw(nm, "hand") || HasKw(nm, "wrist"))
				return 1.0f;
			if (HasKw(nm, "forearm") || HasKw(nm, "lowerarm") || HasKw(nm, "elbow"))
				return 2.0f;
			if (HasKw(nm, "upperarm") || HasKw(nm, "shoulder") || HasKw(nm, "clavicle") || HasKw(nm, "arm"))
				return 3.0f;
			// Jambes (test "foot"/"shin" avant "leg").
			if (HasKw(nm, "foot") || HasKw(nm, "ankle") || HasKw(nm, "toe"))
				return 1.5f;
			if (HasKw(nm, "shin") || HasKw(nm, "calf") || HasKw(nm, "lowerleg") || HasKw(nm, "knee"))
				return 5.0f;
			if (HasKw(nm, "thigh") || HasKw(nm, "upperleg") || HasKw(nm, "upleg"))
				return 10.0f;
			// Tête / cou.
			if (HasKw(nm, "head") || HasKw(nm, "skull") || HasKw(nm, "neck"))
				return 8.0f;
			// Tronc (bassin le plus lourd, puis colonne/torse).
			if (HasKw(nm, "hip") || HasKw(nm, "pelvis") || HasKw(nm, "root"))
				return 15.0f;
			if (HasKw(nm, "spine") || HasKw(nm, "chest") || HasKw(nm, "thorax") || HasKw(nm, "abdomen") ||
				HasKw(nm, "torso") || HasKw(nm, "trunk"))
				return 12.0f;
			// Jambe générique (après thigh/shin/foot).
			if (HasKw(nm, "leg"))
				return 6.0f;
			// Non reconnu : petite masse résiduelle.
			return 2.0f;
		}

		void NkPoseMass::SetUniform(int32 jointCount) {
			jointMass.Clear();
			if (jointCount <= 0)
				return;
			jointMass.Reserve((nk_size)jointCount);
			for (int32 i = 0; i < jointCount; ++i)
				jointMass.PushBack(1.0f);
		}

		void NkPoseMass::SetAnthropometric(const NkVector<NkString> &jointNames) {
			jointMass.Clear();
			const int32 n = (int32)jointNames.Size();
			if (n <= 0)
				return;
			jointMass.Reserve((nk_size)n);
			for (int32 i = 0; i < n; ++i)
				jointMass.PushBack(MassWeightForName(jointNames[(nk_size)i]));
		}

		float32 NkPoseMass::TotalMass() const {
			float32 s = 0.f;
			for (nk_size i = 0; i < jointMass.Size(); ++i)
				s += jointMass[i];
			return s;
		}

		NkVec3f NkPoseMass::ComputeCOMFromPositions(const NkVec3f *worldPos, int32 count) const {
			if (!worldPos || count <= 0 || (int32)jointMass.Size() != count)
				return NkVec3f{0.f, 0.f, 0.f};
			NkVec3f acc{0.f, 0.f, 0.f};
			float32 total = 0.f;
			for (int32 j = 0; j < count; ++j) {
				const float32 m = jointMass[(nk_size)j];
				acc.x += m * worldPos[j].x;
				acc.y += m * worldPos[j].y;
				acc.z += m * worldPos[j].z;
				total += m;
			}
			if (total <= 0.f)
				return NkVec3f{0.f, 0.f, 0.f};
			const float32 inv = 1.0f / total;
			return NkVec3f{acc.x * inv, acc.y * inv, acc.z * inv};
		}

		NkVec3f NkPoseMass::ComputeCOM(const NkMat4f *jointWorld, int32 count) const {
			if (!jointWorld || count <= 0 || (int32)jointMass.Size() != count)
				return NkVec3f{0.f, 0.f, 0.f};
			NkVec3f acc{0.f, 0.f, 0.f};
			float32 total = 0.f;
			for (int32 j = 0; j < count; ++j) {
				const NkVec3f p = jointWorld[j] * NkVec3f{0.f, 0.f, 0.f}; // colonne translation = position monde
				const float32 m = jointMass[(nk_size)j];
				acc.x += m * p.x;
				acc.y += m * p.y;
				acc.z += m * p.z;
				total += m;
			}
			if (total <= 0.f)
				return NkVec3f{0.f, 0.f, 0.f};
			const float32 inv = 1.0f / total;
			return NkVec3f{acc.x * inv, acc.y * inv, acc.z * inv};
		}

		bool NkPoseMass::SelfTest() {
			const float32 eps = 1e-4f;

			// 1) Masse uniforme + positions symétriques -> COM = barycentre (x≈0).
			{
				NkPoseMass pm;
				pm.SetUniform(2);
				NkVec3f pos[2] = {NkVec3f{-1.f, 0.f, 0.f}, NkVec3f{1.f, 0.f, 0.f}};
				NkVec3f com = pm.ComputeCOMFromPositions(pos, 2);
				if (com.x > eps || com.x < -eps || com.y > eps || com.z > eps)
					return false;
			}

			// 2) Cas PONDÉRÉ : masses [1,3] aux x=[0,2] -> COM.x = (0·1 + 2·3)/4 = 1.5.
			{
				NkPoseMass pm;
				pm.jointMass.PushBack(1.0f);
				pm.jointMass.PushBack(3.0f);
				NkVec3f pos[2] = {NkVec3f{0.f, 0.f, 0.f}, NkVec3f{2.f, 0.f, 0.f}};
				NkVec3f com = pm.ComputeCOMFromPositions(pos, 2);
				if (com.x < 1.5f - eps || com.x > 1.5f + eps)
					return false;
			}

			// 3) MONOTONIE : augmenter la masse du joint 0 rapproche le COM du joint 0.
			{
				NkPoseMass a;
				a.jointMass.PushBack(1.0f);
				a.jointMass.PushBack(1.0f);
				NkPoseMass b;
				b.jointMass.PushBack(5.0f);
				b.jointMass.PushBack(1.0f);
				NkVec3f pos[2] = {NkVec3f{0.f, 0.f, 0.f}, NkVec3f{10.f, 0.f, 0.f}};
				NkVec3f comA = a.ComputeCOMFromPositions(pos, 2);
				NkVec3f comB = b.ComputeCOMFromPositions(pos, 2);
				if (!(comB.x < comA.x))
					return false; // plus de masse en 0 => COM plus proche de 0
			}

			// 4) Anthropométrique : la tête pèse plus qu'une main ; le bassin plus que la tête.
			{
				NkVector<NkString> names;
				names.PushBack(NkString("mixamorig:Head"));
				names.PushBack(NkString("mixamorig:LeftHand"));
				names.PushBack(NkString("mixamorig:Hips"));
				NkPoseMass pm;
				pm.SetAnthropometric(names);
				if (pm.jointMass.Size() != 3)
					return false;
				const float32 head = pm.jointMass[0];
				const float32 hand = pm.jointMass[1];
				const float32 hips = pm.jointMass[2];
				if (!(head > hand))
					return false;
				if (!(hips > head))
					return false;
			}

			// 5) Garde-fous : count incohérent -> {0,0,0} (pas de crash).
			{
				NkPoseMass pm;
				pm.SetUniform(3);
				NkVec3f pos[2] = {NkVec3f{1.f, 2.f, 3.f}, NkVec3f{4.f, 5.f, 6.f}};
				NkVec3f com = pm.ComputeCOMFromPositions(pos, 2); // 2 != 3
				if (com.x != 0.f || com.y != 0.f || com.z != 0.f)
					return false;
			}

			return true;
		}

	} // namespace animphys
} // namespace nkentseu
