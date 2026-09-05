// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// NkMannequin.cpp — capsules par os ajustées sur la peau, rôles d'un humanoïde, point-dans-maillage (en-tête).
#include "NKPhysics/NkMannequin.h"
#include "NKCollision/NkColTests.h"
#include "NKMath/NkFunctions.h"
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace physics {

		using nkentseu::math::NkSqrt;

		// ── Utilitaires locaux ───────────────────────────────────────────────
		namespace {

			NkVec3f JointPos(const NkSkeletonBind &s, uint32 j) {
				return s.world[j].TransformPoint(NkVec3f{0.f, 0.f, 0.f});
			}

			// Liste des enfants (CSR) et nombre de descendants par joint.
			struct Topo {
					NkVector<uint32> start, child, desc;
					void Build(const int32 *parent, uint32 n) {
						start.Resize(n + 1, 0u);
						for (uint32 j = 0; j < n; ++j) {
							const int32 p = parent[j];
							if (p >= 0 && (uint32)p < n)
								++start[(uint32)p + 1];
						}
						for (uint32 j = 0; j < n; ++j)
							start[j + 1] += start[j];
						child.Resize(start[n], 0u);
						NkVector<uint32> fill;
						fill.Resize(n, 0u);
						for (uint32 j = 0; j < n; ++j) {
							const int32 p = parent[j];
							if (p >= 0 && (uint32)p < n)
								child[start[(uint32)p] + fill[(uint32)p]++] = j;
						}
						desc.Resize(n, 0u);
						// descendants : remonter chaque joint vers ses ancêtres (n petit, O(n x profondeur))
						for (uint32 j = 0; j < n; ++j) {
							int32 p = parent[j];
							uint32 guard = 0;
							while (p >= 0 && (uint32)p < n && guard++ < n) {
								++desc[(uint32)p];
								p = parent[(uint32)p];
							}
						}
					}
					uint32 ChildCount(uint32 j) const {
						return start[j + 1] - start[j];
					}
					uint32 Child(uint32 j, uint32 k) const {
						return child[start[j] + k];
					}
					// l'enfant qui porte le plus de descendants (-1 si feuille)
					int32 MainChild(uint32 j) const {
						int32 best = -1;
						uint32 bd = 0;
						for (uint32 k = 0; k < ChildCount(j); ++k) {
							const uint32 c = Child(j, k);
							if (best < 0 || desc[c] > bd) {
								best = (int32)c;
								bd = desc[c];
							}
						}
						return best;
					}
			};

			// Indice de côté lu dans un nom : 1 = gauche, 2 = droite, 0 = rien.
			int32 SideHint(const char *name) {
				if (!name)
					return 0;
				char low[96];
				uint32 n = 0;
				for (const char *c = name; *c && n + 1 < sizeof(low); ++c) {
					char ch = *c;
					if (ch >= 'A' && ch <= 'Z')
						ch = (char)(ch - 'A' + 'a');
					low[n++] = ch;
				}
				low[n] = '\0';
				if (std::strstr(low, "left"))
					return 1;
				if (std::strstr(low, "right"))
					return 2;
				// jetons séparés par tout ce qui n'est pas une lettre / un chiffre : « l », « r »
				uint32 i = 0;
				while (i < n) {
					while (i < n && !((low[i] >= 'a' && low[i] <= 'z') || (low[i] >= '0' && low[i] <= '9')))
						++i;
					const uint32 s = i;
					while (i < n && ((low[i] >= 'a' && low[i] <= 'z') || (low[i] >= '0' && low[i] <= '9')))
						++i;
					if (i - s == 1) {
						if (low[s] == 'l')
							return 1;
						if (low[s] == 'r')
							return 2;
					}
				}
				return 0;
			}

			// Suit la chaîne principale depuis j (j compris), au plus `maxLen` joints ; s'arrête
			// AVANT de dépasser un joint à `stopBranch` enfants ou plus (ce joint est inclus).
			uint32 Follow(const Topo &t, uint32 j, uint32 maxLen, uint32 stopBranch, uint32 *out) {
				uint32 len = 0;
				int32 cur = (int32)j;
				while (cur >= 0 && len < maxLen) {
					out[len++] = (uint32)cur;
					if (stopBranch > 0 && t.ChildCount((uint32)cur) >= stopBranch)
						break;
					cur = t.MainChild((uint32)cur);
				}
				return len;
			}

			int32 CompareF(const void *a, const void *b) {
				const float32 x = *(const float32 *)a, y = *(const float32 *)b;
				return x < y ? -1 : (x > y ? 1 : 0);
			}

			float32 DistPointSegment(const NkVec3f &p, const NkVec3f &a, const NkVec3f &b) {
				const NkVec3f ab = b - a;
				const float32 ab2 = ab.Dot(ab);
				float32 t = ab2 > 1e-12f ? (p - a).Dot(ab) / ab2 : 0.f;
				t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
				return (p - (a + ab * t)).Len();
			}

			bool IsDescendant(const int32 *parent, uint32 n, uint32 j, uint32 ancestor) {
				int32 p = parent[j];
				uint32 guard = 0;
				while (p >= 0 && (uint32)p < n && guard++ < n) {
					if ((uint32)p == ancestor)
						return true;
					p = parent[(uint32)p];
				}
				return false;
			}

		} // namespace

		// ── NkHumanoidMap ────────────────────────────────────────────────────
		const char *NkHumanoidMap::RoleName(NkHumanBone b) noexcept {
			static const char *kNames[NK_HB_COUNT] = {
				"hanches",		  "colonne",		 "poitrine",	   "cou",			"tete",			 "sommet tete",
				"epaule G",		  "bras G",			 "avant-bras G",   "main G",		"epaule D",		 "bras D",
				"avant-bras D",	  "main D",			 "cuisse G",	   "jambe G",		"pied G",		 "cuisse D",
				"jambe D",		  "pied D"};
			return b < NK_HB_COUNT ? kNames[b] : "?";
		}

		bool NkHumanoidMap::Resolve(const NkSkeletonBind &skel) {
			for (uint32 k = 0; k < NK_HB_COUNT; ++k)
				joint[k] = -1;
			const uint32 n = skel.count;
			if (n < 3 || !skel.parent || !skel.world)
				return false;
			Topo t;
			t.Build(skel.parent, n);
			NkVector<NkVec3f> P;
			P.Resize(n);
			float32 ylo = 1e30f, yhi = -1e30f;
			for (uint32 j = 0; j < n; ++j) {
				P[j] = JointPos(skel, j);
				if (P[j].y < ylo)
					ylo = P[j].y;
				if (P[j].y > yhi)
					yhi = P[j].y;
			}
			const float32 height = (yhi - ylo) > 1e-3f ? (yhi - ylo) : 1.f;
			// racine : le joint sans parent qui porte le plus de descendants
			int32 root = -1;
			for (uint32 j = 0; j < n; ++j)
				if (skel.parent[j] < 0 && (root < 0 || t.desc[j] > t.desc[(uint32)root]))
					root = (int32)j;
			if (root < 0)
				return false;
			// direction d'une chaîne qui part de l'enfant c : vers son DEUXIÈME joint quand il existe
			// (une cuisse est à 5 cm sous les hanches, mais le genou à 45 cm ; une clavicule est
			// courte, le bras est long) -- mesuré : le seuil sur le premier joint ratait les jambes
			auto chainDir = [&](uint32 from, uint32 c) -> NkVec3f {
				uint32 ch[3];
				const uint32 len = Follow(t, c, 3, 0, ch);
				return P[ch[len > 1 ? 1 : 0]] - P[from];
			};
			// hanches : premier joint de la chaîne principale à au moins deux enfants dont un descend
			int32 hips = -1;
			{
				int32 cur = root;
				uint32 guard = 0;
				while (cur >= 0 && guard++ < n) {
					uint32 down = 0;
					for (uint32 k = 0; k < t.ChildCount((uint32)cur); ++k)
						if (chainDir((uint32)cur, t.Child((uint32)cur, k)).y < -0.05f * height)
							++down;
					if (t.ChildCount((uint32)cur) >= 2 && down >= 1) {
						hips = cur;
						break;
					}
					cur = t.MainChild((uint32)cur);
				}
				if (hips < 0)
					hips = root;
			}
			joint[NK_HB_HIPS] = hips;
			const NkVec3f hp = P[(uint32)hips];
			// enfants des hanches : jambes (descendent), colonne (monte, la plus peuplée)
			int32 legs[2] = {-1, -1};
			uint32 nlegs = 0;
			int32 spine = -1;
			for (uint32 k = 0; k < t.ChildCount((uint32)hips); ++k) {
				const uint32 c = t.Child((uint32)hips, k);
				const NkVec3f d = chainDir((uint32)hips, c);
				if (d.y < -0.05f * height) {
					if (nlegs < 2)
						legs[nlegs++] = (int32)c;
				} else if (spine < 0 || t.desc[c] > t.desc[(uint32)spine]) {
					spine = (int32)c;
				}
			}
			// côté d'une chaîne : nom, sinon +x = gauche
			auto sideOf = [&](uint32 j) -> int32 {
				int32 s = skel.names ? SideHint(skel.names[j]) : 0;
				if (s == 0) {
					// le nom de l'enfant principal peut porter le côté (« LeftLeg » sous « LeftUpLeg »)
					const int32 c = t.MainChild(j);
					if (c >= 0 && skel.names)
						s = SideHint(skel.names[(uint32)c]);
				}
				if (s == 0)
					s = (P[j].x - hp.x) >= 0.f ? 1 : 2;
				return s;
			};
			for (uint32 l = 0; l < nlegs; ++l) {
				uint32 chain[6];
				const uint32 len = Follow(t, (uint32)legs[l], 6, 0, chain);
				const int32 side = sideOf((uint32)legs[l]);
				const NkHumanBone up = side == 1 ? NK_HB_L_UPPER_LEG : NK_HB_R_UPPER_LEG;
				const NkHumanBone lo = side == 1 ? NK_HB_L_LOWER_LEG : NK_HB_R_LOWER_LEG;
				const NkHumanBone ft = side == 1 ? NK_HB_L_FOOT : NK_HB_R_FOOT;
				if (joint[up] >= 0)
					continue; // deux jambes du même côté : la seconde reste sans rôle, dit par Found()
				joint[up] = (int32)chain[0];
				if (len > 1)
					joint[lo] = (int32)chain[1];
				if (len > 2)
					joint[ft] = (int32)chain[2];
			}
			if (spine < 0)
				return false;
			joint[NK_HB_SPINE] = spine;
			// colonne : jusqu'au joint d'où partent les bras (>= 3 enfants, ou 2 enfants de côté)
			int32 chest = -1;
			{
				int32 cur = spine;
				uint32 guard = 0;
				while (cur >= 0 && guard++ < n) {
					const uint32 cc = t.ChildCount((uint32)cur);
					if (cc >= 3) {
						chest = cur;
						break;
					}
					if (cc == 2) {
						uint32 side = 0;
						for (uint32 k = 0; k < 2; ++k) {
							const NkVec3f d = P[t.Child((uint32)cur, k)] - P[(uint32)cur];
							if (math::NkAbs(d.x) > math::NkAbs(d.y))
								++side;
						}
						if (side == 2) {
							chest = cur;
							break;
						}
						cur = t.MainChild((uint32)cur);
						continue;
					}
					if (cc == 0) {
						chest = cur;
						break;
					}
					cur = t.MainChild((uint32)cur);
				}
				if (chest < 0)
					chest = spine;
			}
			joint[NK_HB_CHEST] = chest;
			const NkVec3f cp = P[(uint32)chest];
			for (uint32 k = 0; k < t.ChildCount((uint32)chest); ++k) {
				const uint32 c = t.Child((uint32)chest, k);
				uint32 chain[6];
				const uint32 len = Follow(t, c, 6, 3, chain);
				// direction de la chaîne : vers son deuxième joint si possible (une clavicule est courte)
				const NkVec3f d = P[chain[len > 1 ? 1 : 0]] - cp;
				const bool sideways = math::NkAbs(d.x) > math::NkAbs(d.y) && math::NkAbs(d.x) > math::NkAbs(d.z);
				if (sideways) {
					const int32 side = sideOf(c);
					const NkHumanBone sh = side == 1 ? NK_HB_L_SHOULDER : NK_HB_R_SHOULDER;
					const NkHumanBone ua = side == 1 ? NK_HB_L_UPPER_ARM : NK_HB_R_UPPER_ARM;
					const NkHumanBone fa = side == 1 ? NK_HB_L_FOREARM : NK_HB_R_FOREARM;
					const NkHumanBone ha = side == 1 ? NK_HB_L_HAND : NK_HB_R_HAND;
					if (joint[ua] >= 0)
						continue;
					uint32 o = 0; // décalage : clavicule courte devant le bras
					if (len >= 4) {
						const float32 l01 = (P[chain[1]] - P[chain[0]]).Len();
						const float32 l12 = (P[chain[2]] - P[chain[1]]).Len();
						if (l01 < 0.5f * l12) {
							joint[sh] = (int32)chain[0];
							o = 1;
						}
					}
					if (o < len)
						joint[ua] = (int32)chain[o];
					if (o + 1 < len)
						joint[fa] = (int32)chain[o + 1];
					if (o + 2 < len)
						joint[ha] = (int32)chain[o + 2];
				} else if (d.y > 0.f && joint[NK_HB_NECK] < 0) {
					joint[NK_HB_NECK] = (int32)c;
					uint32 hc[8];
					const uint32 hl = Follow(t, c, 8, 0, hc);
					joint[NK_HB_HEAD] = (int32)hc[hl > 1 ? 1 : 0];
					if (hl > 2)
						joint[NK_HB_HEAD_TOP] = (int32)hc[hl - 1];
				}
			}
			return joint[NK_HB_HIPS] >= 0 && joint[NK_HB_CHEST] >= 0 && joint[NK_HB_L_UPPER_LEG] >= 0 &&
				   joint[NK_HB_R_UPPER_LEG] >= 0 && joint[NK_HB_L_UPPER_ARM] >= 0 && joint[NK_HB_R_UPPER_ARM] >= 0;
		}

		// ── NkMannequin ──────────────────────────────────────────────────────
		bool NkMannequin::Build(const NkSkeletonBind &skel, const NkSkinSample *samples, uint32 sampleCount,
								const NkHumanoidMap *map) {
			mCaps.Clear();
			mJointCount = skel.count;
			const uint32 n = skel.count;
			if (n < 2 || !skel.parent || !skel.world)
				return false;
			NkVector<NkVec3f> P;
			P.Resize(n);
			for (uint32 j = 0; j < n; ++j)
				P[j] = JointPos(skel, j);
			// joints sautés (doigts) et capsules de main
			NkVector<uint8> skip;
			skip.Resize(n, (uint8)0);
			int32 hands[2] = {-1, -1};
			if (params.mergeFingers && map) {
				hands[0] = map->joint[NK_HB_L_HAND];
				hands[1] = map->joint[NK_HB_R_HAND];
				for (uint32 h = 0; h < 2; ++h)
					if (hands[h] >= 0)
						for (uint32 j = 0; j < n; ++j)
							if (IsDescendant(skel.parent, n, j, (uint32)hands[h]))
								skip[j] = 1;
			}
			// segments : un par joint non racine non sauté (owner = parent), plus un par main
			for (uint32 j = 0; j < n; ++j) {
				const int32 p = skel.parent[j];
				if (p < 0 || (uint32)p >= n || skip[j])
					continue;
				NkMannequinCapsule c;
				c.owner = p;
				c.joint = (int32)j;
				mCaps.PushBack(c);
			}
			for (uint32 h = 0; h < 2; ++h) {
				if (hands[h] < 0)
					continue;
				// de la main au descendant le plus lointain
				const uint32 hj = (uint32)hands[h];
				int32 far = -1;
				float32 fd = 0.f;
				for (uint32 j = 0; j < n; ++j)
					if (skip[j] && IsDescendant(skel.parent, n, j, hj)) {
						const float32 d = (P[j] - P[hj]).Len();
						if (d > fd) {
							fd = d;
							far = (int32)j;
						}
					}
				if (far < 0)
					continue;
				NkMannequinCapsule c;
				c.owner = (int32)hj;
				c.joint = far;
				mCaps.PushBack(c);
			}
			const uint32 nc = (uint32)mCaps.Size();
			// os dominant de chaque sommet ; les doigts comptent pour leur main
			NkVector<int32> dom;
			dom.Resize(sampleCount, -1);
			for (uint32 s = 0; s < sampleCount; ++s) {
				int32 b = -1;
				float32 bw = -1.f;
				for (uint32 k = 0; k < 4; ++k)
					if (samples[s].bone[k] >= 0 && (uint32)samples[s].bone[k] < n && samples[s].w[k] > bw) {
						bw = samples[s].w[k];
						b = samples[s].bone[k];
					}
				if (b >= 0 && skip[(uint32)b])
					for (uint32 h = 0; h < 2; ++h)
						if (hands[h] >= 0 && IsDescendant(skel.parent, n, (uint32)b, (uint32)hands[h]))
							b = hands[h];
				dom[s] = b;
			}
			// distances : chaque sommet va au plus proche des segments PORTÉS par son os dominant
			NkVector<uint32> count;
			count.Resize(nc, 0u);
			NkVector<int32> segOf;
			segOf.Resize(sampleCount, -1);
			NkVector<float32> dist;
			dist.Resize(sampleCount, 0.f);
			for (uint32 s = 0; s < sampleCount; ++s) {
				const int32 b = dom[s];
				if (b < 0)
					continue;
				int32 best = -1;
				float32 bd = 1e30f;
				for (uint32 c = 0; c < nc; ++c) {
					if (mCaps[c].owner != b)
						continue;
					const float32 d = DistPointSegment(samples[s].pos, P[(uint32)mCaps[c].owner], P[(uint32)mCaps[c].joint]);
					if (d < bd) {
						bd = d;
						best = (int32)c;
					}
				}
				if (best >= 0) {
					segOf[s] = best;
					dist[s] = bd;
					++count[(uint32)best];
				}
			}
			// quantile par segment
			NkVector<float32> buf;
			for (uint32 c = 0; c < nc; ++c) {
				NkMannequinCapsule &cap = mCaps[c];
				cap.samples = count[c];
				buf.Clear();
				for (uint32 s = 0; s < sampleCount; ++s)
					if (segOf[s] == (int32)c)
						buf.PushBack(dist[s]);
				if ((uint32)buf.Size() >= params.minSamples) {
					std::qsort(buf.Data(), buf.Size(), sizeof(float32), CompareF);
					float32 q = params.quantile < 0.f ? 0.f : (params.quantile > 1.f ? 1.f : params.quantile);
					uint32 idx = (uint32)(q * (float32)((uint32)buf.Size() - 1) + 0.5f);
					if (idx >= (uint32)buf.Size())
						idx = (uint32)buf.Size() - 1;
					cap.radius = buf[idx];
				} else {
					cap.radius = -1.f; // à hériter
				}
			}
			// héritage : un segment sans sommets prend le rayon du segment qui arrive à son os porteur
			for (uint32 pass = 0; pass < 4; ++pass)
				for (uint32 c = 0; c < nc; ++c) {
					if (mCaps[c].radius >= 0.f)
						continue;
					const float32 r = RadiusTo(mCaps[c].owner);
					if (r > 0.f)
						mCaps[c].radius = r;
				}
			for (uint32 c = 0; c < nc; ++c) {
				NkMannequinCapsule &cap = mCaps[c];
				if (cap.radius < params.minRadius)
					cap.radius = params.minRadius;
				const math::NkMat4f inv = skel.world[(uint32)cap.owner].Inverse();
				cap.localA = inv.TransformPoint(P[(uint32)cap.owner]);
				cap.localB = inv.TransformPoint(P[(uint32)cap.joint]);
			}
			return nc > 0;
		}

		void NkMannequin::Pose(const math::NkMat4f *jointWorld, uint32 jointCount,
							   NkVector<collision::NkShape> &out) const {
			out.Clear();
			const uint32 nc = (uint32)mCaps.Size();
			for (uint32 c = 0; c < nc; ++c) {
				const NkMannequinCapsule &cap = mCaps[c];
				if (cap.owner < 0 || (uint32)cap.owner >= jointCount)
					continue;
				const math::NkMat4f &W = jointWorld[(uint32)cap.owner];
				out.PushBack(collision::NkShape::Capsule3D(W.TransformPoint(cap.localA), W.TransformPoint(cap.localB),
														   cap.radius + params.margin));
			}
		}

		float32 NkMannequin::RadiusTo(int32 j) const noexcept {
			for (uint32 c = 0; c < (uint32)mCaps.Size(); ++c)
				if (mCaps[c].joint == j && mCaps[c].radius > 0.f)
					return mCaps[c].radius;
			return 0.f;
		}

		float32 NkMannequin::RadiusOf(int32 b) const noexcept {
			float32 r = 0.f;
			for (uint32 c = 0; c < (uint32)mCaps.Size(); ++c)
				if (mCaps[c].owner == b && mCaps[c].radius > r)
					r = mCaps[c].radius;
			return r;
		}

		// ── NkMeshInsideTester ───────────────────────────────────────────────
		namespace {
			// direction du rayon de parité : +x incliné de ~0,1° en y et ~0,05° en z (unitaire à 1e-6 près)
			constexpr float32 kRayDirX = 0.999998f, kRayDirY = 0.00171f, kRayDirZ = 0.00089f;
		} // namespace
		void NkMeshInsideTester::Build(const NkVec3f *verts, uint32 vertCount, const uint32 *indices, uint32 triCount,
									   uint32 cellsY, uint32 cellsZ) {
			mVerts = verts;
			mIdx = indices;
			mTriCount = triCount;
			mCY = cellsY < 1 ? 1 : cellsY;
			mCZ = cellsZ < 1 ? 1 : cellsZ;
			mCellStart.Clear();
			mCellTri.Clear();
			if (!verts || vertCount == 0 || !indices || triCount == 0) {
				mTriCount = 0;
				return;
			}
			mMin = mMax = verts[0];
			for (uint32 v = 1; v < vertCount; ++v) {
				const NkVec3f &q = verts[v];
				if (q.x < mMin.x) mMin.x = q.x;
				if (q.y < mMin.y) mMin.y = q.y;
				if (q.z < mMin.z) mMin.z = q.z;
				if (q.x > mMax.x) mMax.x = q.x;
				if (q.y > mMax.y) mMax.y = q.y;
				if (q.z > mMax.z) mMax.z = q.z;
			}
			const float32 ey = (mMax.y - mMin.y) > 1e-6f ? (mMax.y - mMin.y) : 1e-6f;
			const float32 ez = (mMax.z - mMin.z) > 1e-6f ? (mMax.z - mMin.z) : 1e-6f;
			// Le rayon est LÉGÈREMENT INCLINÉ (en-tête de Inside) : un rayon exactement selon +x tombe
			// sur les arêtes et sommets partagés d'un maillage régulier (la diagonale d'une face, une
			// colonne de sommets) et chaque arête compte deux fois -- mesuré : l'origine d'une boîte
			// [-1, 1]³ sortait « dehors ». La dérive du rayon sur la longueur du maillage gonfle les
			// seaux, de sorte que la cellule du point de départ contienne tout ce que le rayon peut
			// toucher.
			const float32 driftY = (kRayDirY / kRayDirX) * (mMax.x - mMin.x) + 1e-5f;
			const float32 driftZ = (kRayDirZ / kRayDirX) * (mMax.x - mMin.x) + 1e-5f;
			const uint32 ncell = mCY * mCZ;
			mCellStart.Resize(ncell + 1, 0u);
			auto cellRange = [&](uint32 tri, uint32 &y0, uint32 &y1, uint32 &z0, uint32 &z1) {
				const NkVec3f &a = mVerts[mIdx[tri * 3]], &b = mVerts[mIdx[tri * 3 + 1]], &c = mVerts[mIdx[tri * 3 + 2]];
				const float32 ymin = math::NkMin(a.y, math::NkMin(b.y, c.y)) - driftY, ymax = math::NkMax(a.y, math::NkMax(b.y, c.y));
				const float32 zmin = math::NkMin(a.z, math::NkMin(b.z, c.z)) - driftZ, zmax = math::NkMax(a.z, math::NkMax(b.z, c.z));
				auto cy = [&](float32 y) {
					int32 k = (int32)((y - mMin.y) / ey * (float32)mCY);
					return (uint32)(k < 0 ? 0 : (k >= (int32)mCY ? (int32)mCY - 1 : k));
				};
				auto cz = [&](float32 z) {
					int32 k = (int32)((z - mMin.z) / ez * (float32)mCZ);
					return (uint32)(k < 0 ? 0 : (k >= (int32)mCZ ? (int32)mCZ - 1 : k));
				};
				y0 = cy(ymin);
				y1 = cy(ymax);
				z0 = cz(zmin);
				z1 = cz(zmax);
			};
			for (uint32 t = 0; t < triCount; ++t) {
				uint32 y0, y1, z0, z1;
				cellRange(t, y0, y1, z0, z1);
				for (uint32 y = y0; y <= y1; ++y)
					for (uint32 z = z0; z <= z1; ++z)
						++mCellStart[y * mCZ + z + 1];
			}
			for (uint32 c = 0; c < ncell; ++c)
				mCellStart[c + 1] += mCellStart[c];
			mCellTri.Resize(mCellStart[ncell], 0u);
			NkVector<uint32> fill;
			fill.Resize(ncell, 0u);
			for (uint32 t = 0; t < triCount; ++t) {
				uint32 y0, y1, z0, z1;
				cellRange(t, y0, y1, z0, z1);
				for (uint32 y = y0; y <= y1; ++y)
					for (uint32 z = z0; z <= z1; ++z) {
						const uint32 c = y * mCZ + z;
						mCellTri[mCellStart[c] + fill[c]++] = t;
					}
			}
		}

		bool NkMeshInsideTester::Inside(const NkVec3f &p) const noexcept {
			if (mTriCount == 0)
				return false;
			if (p.y < mMin.y || p.y > mMax.y || p.z < mMin.z || p.z > mMax.z || p.x > mMax.x || p.x < mMin.x)
				return false;
			const float32 ey = (mMax.y - mMin.y) > 1e-6f ? (mMax.y - mMin.y) : 1e-6f;
			const float32 ez = (mMax.z - mMin.z) > 1e-6f ? (mMax.z - mMin.z) : 1e-6f;
			int32 ky = (int32)((p.y - mMin.y) / ey * (float32)mCY);
			int32 kz = (int32)((p.z - mMin.z) / ez * (float32)mCZ);
			ky = ky < 0 ? 0 : (ky >= (int32)mCY ? (int32)mCY - 1 : ky);
			kz = kz < 0 ? 0 : (kz >= (int32)mCZ ? (int32)mCZ - 1 : kz);
			const uint32 c = (uint32)ky * mCZ + (uint32)kz;
			collision::NkRay3D ray;
			ray.origin = p;
			ray.dir = {kRayDirX, kRayDirY, kRayDirZ}; // incliné : jamais exactement sur une arête (Build)
			ray.maxT = 1e30f;
			uint32 hits = 0;
			for (uint32 q = mCellStart[c]; q < mCellStart[c + 1]; ++q) {
				const uint32 t = mCellTri[q];
				collision::NkRayHit3D hit;
				if (collision::NkRayTriangle3D(ray, mVerts[mIdx[t * 3]], mVerts[mIdx[t * 3 + 1]], mVerts[mIdx[t * 3 + 2]],
											   hit))
					++hits;
			}
			return (hits & 1u) != 0u;
		}

		bool NkMeshInsideTester::InsideNearest(const NkVec3f &p, float32 *outDepth) const noexcept {
			if (outDepth)
				*outDepth = 0.f;
			if (mTriCount == 0)
				return false;
			float32 best2 = 1e30f;
			NkVec3f bestPoint{}, bestNormal{0.f, 1.f, 0.f};
			for (uint32 t = 0; t < mTriCount; ++t) {
				const NkVec3f &a = mVerts[mIdx[t * 3]], &b = mVerts[mIdx[t * 3 + 1]], &c = mVerts[mIdx[t * 3 + 2]];
				// point le plus proche du triangle (Ericson §5.1.5, régions de Voronoï)
				const NkVec3f ab = b - a, ac = c - a, ap = p - a;
				const float32 d1 = ab.Dot(ap), d2 = ac.Dot(ap);
				NkVec3f q;
				if (d1 <= 0.f && d2 <= 0.f) {
					q = a;
				} else {
					const NkVec3f bp = p - b;
					const float32 d3 = ab.Dot(bp), d4 = ac.Dot(bp);
					if (d3 >= 0.f && d4 <= d3) {
						q = b;
					} else {
						const float32 vc = d1 * d4 - d3 * d2;
						if (vc <= 0.f && d1 >= 0.f && d3 <= 0.f) {
							q = a + ab * (d1 / (d1 - d3));
						} else {
							const NkVec3f cp = p - c;
							const float32 d5 = ab.Dot(cp), d6 = ac.Dot(cp);
							if (d6 >= 0.f && d5 <= d6) {
								q = c;
							} else {
								const float32 vb = d5 * d2 - d1 * d6;
								if (vb <= 0.f && d2 >= 0.f && d6 <= 0.f) {
									q = a + ac * (d2 / (d2 - d6));
								} else {
									const float32 va = d3 * d6 - d5 * d4;
									if (va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f) {
										q = b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
									} else {
										const float32 den = 1.f / (va + vb + vc);
										q = a + ab * (vb * den) + ac * (vc * den);
									}
								}
							}
						}
					}
				}
				const NkVec3f d = p - q;
				const float32 l2 = d.Dot(d);
				if (l2 < best2) {
					best2 = l2;
					bestPoint = q;
					NkVec3f n = ab.Cross(ac);
					const float32 ln = n.Len();
					bestNormal = ln > 1e-12f ? n * (1.f / ln) : NkVec3f{0.f, 1.f, 0.f};
				}
			}
			const float32 signedDist = (p - bestPoint).Dot(bestNormal);
			if (outDepth)
				*outDepth = signedDist < 0.f ? -signedDist : 0.f;
			return signedDist < 0.f;
		}

		uint32 NkMeshInsideTester::CountInsideNearest(const NkVec3f *pts, uint32 count, float32 *outMaxDepth) const noexcept {
			uint32 n = 0;
			float32 worst = 0.f;
			for (uint32 i = 0; i < count; ++i) {
				float32 d = 0.f;
				if (InsideNearest(pts[i], &d)) {
					++n;
					if (d > worst)
						worst = d;
				}
			}
			if (outMaxDepth)
				*outMaxDepth = worst;
			return n;
		}

		uint32 NkMeshInsideTester::CountInside(const NkVec3f *pts, uint32 count, int32 *firstInside) const noexcept {
			uint32 n = 0;
			if (firstInside)
				*firstInside = -1;
			for (uint32 i = 0; i < count; ++i)
				if (Inside(pts[i])) {
					if (firstInside && *firstInside < 0)
						*firstInside = (int32)i;
					++n;
				}
			return n;
		}

	} // namespace physics
} // namespace nkentseu
