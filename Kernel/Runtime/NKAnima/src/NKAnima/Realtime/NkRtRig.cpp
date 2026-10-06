// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkRtRig.cpp — la vue du squelette, les mutations des bancs,
// l'humanoide d'essai.
// =============================================================================
#include "NKAnima/Realtime/NkRtRig.h"

#include "NKAnima/Realtime/NkRtMath.h"

namespace nkentseu {
	namespace anim {
		namespace rt {

			namespace {
				uint32 gMutations = 0u;

				char Lower(char c) {
					return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
				}
				bool EqualsNoCase(const char *a, const char *b) {
					if (a == nullptr || b == nullptr)
						return false;
					for (; *a != '\0' && *b != '\0'; ++a, ++b) {
						if (Lower(*a) != Lower(*b))
							return false;
					}
					return *a == '\0' && *b == '\0';
				}
				bool ContainsNoCase(const char *s, const char *w) {
					if (s == nullptr || w == nullptr || *w == '\0')
						return false;
					for (; *s != '\0'; ++s) {
						const char *a = s, *b = w;
						while (*a != '\0' && *b != '\0' && Lower(*a) == Lower(*b)) {
							++a;
							++b;
						}
						if (*b == '\0')
							return true;
					}
					return false;
				}
				bool EndsWithNoCase(const char *s, const char *suf) {
					uint32 ls = 0, lf = 0;
					while (s[ls] != '\0')
						++ls;
					while (suf[lf] != '\0')
						++lf;
					if (lf > ls)
						return false;
					return EqualsNoCase(s + (ls - lf), suf);
				}
			} // namespace

			void NkRtSetMutations(uint32 bits) noexcept {
				gMutations = bits;
			}
			uint32 NkRtMutations() noexcept {
				return gMutations;
			}

			bool NkRtRig::Build(const int32 *parents, uint32 count, const char *const *names) {
				mParent.Clear();
				mFirstChild.Clear();
				mChildCount.Clear();
				mOrder.Clear();
				mPos.Clear();
				mEnd.Clear();
				mNames.Clear();
				if (parents == nullptr || count == 0u)
					return false;
				for (uint32 j = 0; j < count; ++j) {
					if (parents[j] >= (int32)count || parents[j] == (int32)j)
						return false;
				}
				mParent.Resize(count);
				mFirstChild.Resize(count);
				mChildCount.Resize(count);
				mPos.Resize(count);
				mEnd.Resize(count);
				mNames.Resize(count);
				for (uint32 j = 0; j < count; ++j) {
					mParent[j] = parents[j];
					mFirstChild[j] = -1;
					mChildCount[j] = 0u;
					mNames[j] = (names != nullptr && names[j] != nullptr) ? NkString(names[j]) : NkString();
				}
				for (uint32 j = 0; j < count; ++j) {
					const int32 p = mParent[j];
					if (p >= 0) {
						if (mFirstChild[(uint32)p] < 0)
							mFirstChild[(uint32)p] = (int32)j;
						mChildCount[(uint32)p]++;
					}
				}
				// Ordre prefixe, sans recursion : une pile explicite de joints.
				NkVector<uint32> pile;
				for (uint32 r = count; r-- > 0;) {
					if (mParent[r] < 0)
						pile.PushBack(r);
				}
				while (!pile.Empty()) {
					const uint32 j = pile[(uint32)pile.Size() - 1u];
					pile.PopBack();
					mPos[j] = (uint32)mOrder.Size();
					mOrder.PushBack(j);
					// Les enfants, empiles a l'envers pour sortir dans l'ordre des indices.
					for (uint32 c = count; c-- > 0;) {
						if (mParent[c] == (int32)j)
							pile.PushBack(c);
					}
					if ((uint32)mOrder.Size() > count)
						break;
				}
				if ((uint32)mOrder.Size() != count) {
					// Un cycle : des joints jamais atteints depuis une racine.
					mParent.Clear();
					return false;
				}
				// La fin de chaque sous-arbre : en remontant l'ordre, le max des fins des enfants.
				for (uint32 k = count; k-- > 0;) {
					const uint32 j = mOrder[k];
					uint32 fin = k + 1u;
					for (uint32 c = 0; c < count; ++c) {
						if (mParent[c] == (int32)j && mEnd[c] > fin)
							fin = mEnd[c];
					}
					mEnd[j] = fin;
				}
				return true;
			}

			int32 NkRtRig::Find(const char *name) const noexcept {
				for (uint32 j = 0; j < Count(); ++j) {
					if (EqualsNoCase(mNames[j].CStr(), name))
						return (int32)j;
				}
				return -1;
			}

			int32 NkRtRig::FindLike(const char *const *words, uint32 wordCount, const char *const *suffixes,
									uint32 suffixCount) const noexcept {
				for (uint32 j = 0; j < Count(); ++j) {
					const char *n = mNames[j].CStr();
					bool mot = false;
					for (uint32 w = 0; w < wordCount && !mot; ++w)
						mot = ContainsNoCase(n, words[w]);
					if (!mot)
						continue;
					if (suffixCount == 0u)
						return (int32)j;
					for (uint32 s = 0; s < suffixCount; ++s) {
						if (EndsWithNoCase(n, suffixes[s]))
							return (int32)j;
					}
				}
				return -1;
			}

			void NkRtRig::LocalToWorld(const math::NkMat4f *local, math::NkMat4f *world) const noexcept {
				const uint32 n = Count();
				for (uint32 k = 0; k < n; ++k) {
					const uint32 j = mOrder[k];
					const int32 p = mParent[j];
					world[j] = p >= 0 ? world[(uint32)p] * local[j] : local[j];
				}
			}

			void NkRtRig::RefreshDescendants(uint32 j, const math::NkMat4f *local, math::NkMat4f *world) const noexcept {
				const uint32 fin = mEnd[j];
				for (uint32 k = mPos[j] + 1u; k < fin; ++k) {
					const uint32 c = mOrder[k];
					world[c] = world[(uint32)mParent[c]] * local[c];
				}
			}

			void NkRtRig::WorldToLocal(const math::NkMat4f *world, math::NkMat4f *local) const noexcept {
				for (uint32 j = 0; j < Count(); ++j) {
					const int32 p = mParent[j];
					local[j] = p >= 0 ? world[(uint32)p].Inverse() * world[j] : world[j];
				}
			}

			// ── L'humanoide d'essai ───────────────────────────────────────────────
			void NkRtMakeTestHumanoid(NkRtTestHumanoid &out, bool twistedBind) {
				struct Os {
						const char *nom;
						int32 parent;
						float32 x, y, z;
				};
				// De face vers +Z, la gauche du personnage vers +X, pieds au sol (y = 0).
				static const Os kOs[] = {
					{"hips", -1, 0.f, 0.95f, 0.f},				 // 0
					{"spine", 0, 0.f, 1.08f, -0.01f},			 // 1
					{"chest", 1, 0.f, 1.28f, -0.01f},			 // 2
					{"neck", 2, 0.f, 1.48f, 0.f},				 // 3
					{"head", 3, 0.f, 1.58f, 0.01f},				 // 4
					{"upperarm.L", 2, 0.19f, 1.43f, -0.01f},	 // 5
					{"forearm.L", 5, 0.24f, 1.15f, -0.02f},		 // 6
					{"hand.L", 6, 0.27f, 0.90f, 0.f},			 // 7
					{"upperarm.R", 2, -0.19f, 1.43f, -0.01f},	 // 8
					{"forearm.R", 8, -0.24f, 1.15f, -0.02f},	 // 9
					{"hand.R", 9, -0.27f, 0.90f, 0.f},			 // 10
					{"thigh.L", 0, 0.10f, 0.90f, 0.f},			 // 11
					{"shin.L", 11, 0.10f, 0.49f, 0.01f},		 // 12
					{"foot.L", 12, 0.10f, 0.08f, -0.01f},		 // 13
					{"toe.L", 13, 0.10f, 0.02f, 0.13f},			 // 14
					{"thigh.R", 0, -0.10f, 0.90f, 0.f},			 // 15
					{"shin.R", 15, -0.10f, 0.49f, 0.01f},		 // 16
					{"foot.R", 16, -0.10f, 0.08f, -0.01f},		 // 17
					{"toe.R", 17, -0.10f, 0.02f, 0.13f},		 // 18
					{"ponytail.0", 4, 0.f, 1.72f, -0.09f},		 // 19
					{"ponytail.1", 19, 0.f, 1.60f, -0.14f},		 // 20
					{"ponytail.2", 20, 0.f, 1.48f, -0.16f},		 // 21
					{"ponytail.3", 21, 0.f, 1.36f, -0.17f},		 // 22
				};
				const uint32 n = (uint32)(sizeof(kOs) / sizeof(kOs[0]));
				out.parents.Resize(n);
				out.bindLocal.Resize(n);
				out.names.Resize(n);
				NkVector<math::NkMat4f> monde;
				monde.Resize(n);
				for (uint32 j = 0; j < n; ++j) {
					out.parents[j] = kOs[j].parent;
					out.names[j] = kOs[j].nom;
					NkRtQuat q = QIdentity();
					if (twistedBind) {
						// Une rotation de repos arbitraire mais reproductible, par os.
						const float32 a = 0.7f + 0.9f * (float32)j;
						q = QFromAxisAngle(Normalize(V3(std::sin(a), std::cos(1.3f * a), std::sin(2.1f * a + 0.4f)),
													 V3(0.f, 1.f, 0.f)),
										   0.4f + 0.37f * (float32)(j % 5u));
					}
					monde[j] = MatFrom(q, V3(1.f, 1.f, 1.f), V3(kOs[j].x, kOs[j].y, kOs[j].z));
				}
				for (uint32 j = 0; j < n; ++j) {
					const int32 p = kOs[j].parent;
					out.bindLocal[j] = p >= 0 ? monde[(uint32)p].Inverse() * monde[j] : monde[j];
				}
				out.hips = 0;
				out.spine = 1;
				out.chest = 2;
				out.neck = 3;
				out.head = 4;
				out.upperArmL = 5;
				out.foreArmL = 6;
				out.handL = 7;
				out.upperArmR = 8;
				out.foreArmR = 9;
				out.handR = 10;
				out.thighL = 11;
				out.shinL = 12;
				out.footL = 13;
				out.toeL = 14;
				out.thighR = 15;
				out.shinR = 16;
				out.footR = 17;
				out.toeR = 18;
				out.tail0 = 19;
			}

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
