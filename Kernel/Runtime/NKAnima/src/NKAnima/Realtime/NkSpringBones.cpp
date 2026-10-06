// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkSpringBones.cpp — le mouvement secondaire a ressorts (voir l'en-tete).
// =============================================================================
#include "NKAnima/Realtime/NkSpringBones.h"

namespace nkentseu {
	namespace anim {
		namespace rt {

			namespace {
				char Bas(char c) {
					return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
				}
				/// `nom` contient-il `mot` (casse ignoree) ?
				bool Contient(const char *nom, const char *mot) {
					for (; *nom != '\0'; ++nom) {
						const char *a = nom, *b = mot;
						while (*a != '\0' && *b != '\0' && Bas(*a) == Bas(*b)) {
							++a;
							++b;
						}
						if (*b == '\0')
							return true;
					}
					return false;
				}
			} // namespace

			bool NkSpringBones::Setup(const NkRtRig &rig, const math::NkMat4f *bindWorld) {
				mChains.Clear();
				mColliders.Clear();
				mReady = false;
				if (bindWorld == nullptr || rig.Count() == 0u)
					return false;
				mRig = &rig;
				mBind.Resize(rig.Count());
				for (uint32 j = 0; j < rig.Count(); ++j)
					mBind[j] = bindWorld[j];
				mReady = true;
				return true;
			}

			int32 NkSpringBones::AddChain(const uint32 *joints, uint32 count, const NkSpringParams &params,
										  float32 weight) {
				if (!mReady || joints == nullptr || count == 0u)
					return -1;
				const NkRtRig &rig = *mRig;
				Chain ch;
				ch.params = params;
				ch.weight = weight;
				for (uint32 k = 0; k < count; ++k) {
					const uint32 j = joints[k];
					if (j >= rig.Count())
						return -1;
					const NkVec3f pj = Origin(mBind[j]);
					NkVec3f tip;
					if (k + 1u < count) {
						tip = Origin(mBind[joints[k + 1u]]);
					} else if (rig.FirstChild(j) >= 0) {
						tip = Origin(mBind[(uint32)rig.FirstChild(j)]);
					} else {
						// Le bout de la chaine : l'os prolonge son parent, de la meme longueur.
						const int32 p = rig.Parent(j);
						const NkVec3f pp = p >= 0 ? Origin(mBind[(uint32)p]) : Sub(pj, V3(0.f, 0.1f, 0.f));
						const NkVec3f d = Sub(pj, pp);
						const float32 l = Length(d) > 1e-4f ? Length(d) : 0.1f;
						tip = Madd(pj, Normalize(d, V3(0.f, -1.f, 0.f)), l);
					}
					Bone b;
					b.joint = j;
					b.length = Distance(tip, pj);
					if (b.length < 1e-5f)
						return -1;
					b.tipLocal = TransformPoint(mBind[j].Inverse(), tip);
					ch.bones.PushBack(b);
				}
				mChains.PushBack(ch);
				return (int32)mChains.Size() - 1;
			}

			uint32 NkSpringBones::AddChainsByName(const char *const *words, uint32 wordCount, const NkSpringParams &params) {
				if (!mReady)
					return 0u;
				const NkRtRig &rig = *mRig;
				const uint32 n = rig.Count();
				NkVector<uint8> pris;
				pris.Resize(n, (uint8)0);
				uint32 ajoutees = 0;
				auto Nomme = [&](uint32 j) {
					for (uint32 w = 0; w < wordCount; ++w) {
						if (Contient(rig.Name(j), words[w]))
							return true;
					}
					return false;
				};
				for (uint32 k = 0; k < n; ++k) {
					const uint32 j = rig.Order()[k];
					if (pris[j] || !Nomme(j))
						continue;
					const int32 p = rig.Parent(j);
					if (p >= 0 && Nomme((uint32)p))
						continue; // pas une tete de chaine
					NkVector<uint32> chaine;
					uint32 c = j;
					for (;;) {
						chaine.PushBack(c);
						pris[c] = 1;
						int32 suivant = -1;
						for (uint32 e = 0; e < n; ++e) {
							if (rig.Parent(e) == (int32)c && Nomme(e)) {
								suivant = (int32)e;
								break;
							}
						}
						if (suivant < 0)
							break;
						c = (uint32)suivant;
					}
					if (AddChain(chaine.Data(), (uint32)chaine.Size(), params) >= 0)
						++ajoutees;
				}
				return ajoutees;
			}

			void NkSpringBones::Reset(const math::NkMat4f *world) {
				for (uint32 i = 0; i < (uint32)mChains.Size(); ++i) {
					Chain &ch = mChains[i];
					for (uint32 k = 0; k < (uint32)ch.bones.Size(); ++k) {
						Bone &b = ch.bones[k];
						b.p = TransformPoint(world[b.joint], b.tipLocal);
						b.v = V3(0.f, 0.f, 0.f);
						b.prevTarget = b.p;
						b.prevAnchor = Origin(world[b.joint]);
					}
					ch.primed = true;
				}
			}

			NkVec3f NkSpringBones::Tip(uint32 i, uint32 k) const {
				if (i >= (uint32)mChains.Size() || k >= (uint32)mChains[i].bones.Size())
					return V3(0.f, 0.f, 0.f);
				return mChains[i].bones[k].p;
			}

			void NkSpringBones::Apply(const NkRtRig &rig, const math::NkMat4f *local, math::NkMat4f *world, float32 dt,
									  const NkRtGround *ground) {
				mLastMaxDev = 0.f;
				mLastMaxSpeed = 0.f;
				if (!mReady || dt <= 0.f)
					return;
				const bool explicite = NkRtMutated(NK_RT_MUT_SPRING_EXPLICIT);
				for (uint32 i = 0; i < (uint32)mChains.Size(); ++i) {
					Chain &ch = mChains[i];
					if (!ch.primed) {
						// La premiere image : les pointes partent de la pose animee.
						for (uint32 k = 0; k < (uint32)ch.bones.Size(); ++k) {
							Bone &b = ch.bones[k];
							b.p = TransformPoint(world[b.joint], b.tipLocal);
							b.v = V3(0.f, 0.f, 0.f);
							b.prevTarget = b.p;
							b.prevAnchor = Origin(world[b.joint]);
						}
						ch.primed = true;
					}
					const NkSpringParams &pr = ch.params;
					const NkVec3f g = Scale(gravity, pr.gravity);
					for (uint32 k = 0; k < (uint32)ch.bones.Size(); ++k) {
						Bone &b = ch.bones[k];
						const NkVec3f ancre = Origin(world[b.joint]);
						const NkVec3f cible = TransformPoint(world[b.joint], b.tipLocal);
						if (Distance(ancre, b.prevAnchor) > teleportDistance) {
							b.p = cible;
							b.v = V3(0.f, 0.f, 0.f);
							b.prevTarget = cible;
							b.prevAnchor = ancre;
						}
						uint32 nsub = 1u;
						if (!explicite) {
							const float32 pas = maxSubstep > 1e-4f ? maxSubstep : 1e-4f;
							nsub = (uint32)std::ceil(dt / pas);
							if (nsub < 1u)
								nsub = 1u;
							if (nsub > 64u)
								nsub = 64u;
						}
						if (NkRtMutated(NK_RT_MUT_COST))
							nsub += (uint32)std::ceil(dt * 20000.f);
						const float32 h = dt / (float32)nsub;
						// Le sol sous la pointe : un lancer par image, garde comme un plan.
						bool solOk = false;
						NkVec3f solP = V3(0.f, 0.f, 0.f), solN = V3(0.f, 1.f, 0.f);
						if (ground != nullptr) {
							NkRtRayHit hit;
							if (ground->Cast(Madd(cible, V3(0.f, 1.f, 0.f), 0.8f), V3(0.f, -1.f, 0.f), 2.f, hit)) {
								solOk = true;
								solP = hit.point;
								solN = Normalize(hit.normal, V3(0.f, 1.f, 0.f));
							}
						}
						// ⚠️ PRECISION : on integre en coordonnees RELATIVES a l'ancre (des
						// vecteurs de la taille de l'os), et la vitesse de l'ancre est prise
						// EXACTE (vAncre). En coordonnees monde, a petit pas, l'erreur
						// d'arrondi d'une position (1e-7 m a 3 m) divisee par h devenait une
						// vitesse parasite : le ressort ne se posait plus a 5 000 i/s.
						const NkVec3f vCible = Scale(Sub(cible, b.prevTarget), 1.f / dt);
						const NkVec3f vAncre = Scale(Sub(ancre, b.prevAnchor), 1.f / dt);
						const NkVec3f relCible0 = Sub(b.prevTarget, b.prevAnchor), relCible1 = Sub(cible, ancre);
						NkVec3f r = Sub(b.p, b.prevAnchor); // la pointe, relative a l'ancre
						for (uint32 s = 1; s <= nsub; ++s) {
							const float32 a = (float32)s / (float32)nsub;
							const NkVec3f A = Lerp(b.prevAnchor, ancre, a);
							const NkVec3f tRel = Lerp(relCible0, relCible1, a); // cible - ancre
							const NkVec3f r0 = r;
							if (explicite) {
								// MUTATION : Euler explicite, un pas par image.
								const NkVec3f acc = Add(Add(Scale(Sub(tRel, r0), pr.stiffness),
															Scale(Sub(vCible, b.v), pr.damping)),
														Add(Scale(b.v, -pr.drag), g));
								r = Add(r0, Scale(Sub(b.v, vAncre), h));
								b.v = Madd(b.v, acc, h);
							} else {
								const float32 den = 1.f + h * (pr.damping + pr.drag) + h * h * pr.stiffness;
								const NkVec3f num =
									Madd(b.v, Add(Add(Scale(Sub(tRel, r0), pr.stiffness), Scale(vCible, pr.damping)), g), h);
								b.v = Scale(num, 1.f / den);
								r = Add(r0, Scale(Sub(b.v, vAncre), h));
							}
							// La longueur de l'os.
							const NkVec3f dAnim = Normalize(tRel, V3(0.f, -1.f, 0.f));
							NkVec3f d = Normalize(r, dAnim);
							// L'angle maximal a l'os anime.
							const float32 cosA = Clamp(Dot(d, dAnim), -1.f, 1.f);
							const float32 ang = std::acos(cosA);
							if (ang > pr.maxAngle && ang > 1e-6f) {
								const NkRtQuat q = QScale(QFromTo(dAnim, d), pr.maxAngle / ang);
								d = QRotate(q, dAnim);
							}
							r = Scale(d, b.length);
							// Les spheres de collision et le sol (en monde, rarement actifs).
							for (uint32 c = 0; c < (uint32)mColliders.Size(); ++c) {
								const NkSpringCollider &col = mColliders[c];
								if (col.joint < 0 || (uint32)col.joint >= rig.Count())
									continue;
								const NkVec3f e = Sub(Add(A, r), TransformPoint(world[(uint32)col.joint], col.offset));
								const float32 l = Length(e);
								if (l < col.radius) {
									r = Add(r, Scale(Normalize(e, d), col.radius - l));
									r = Scale(Normalize(r, d), b.length);
								}
							}
							if (solOk) {
								// Projection EXACTE sur (sphere de l'os) inter (demi-espace au-dessus du
								// sol) : la composante normale remonte au minimum, la tangente garde
								// sa direction et la longueur de l'os est exacte.
								const float32 hMin = groundRadius - Dot(Sub(A, solP), solN);
								const float32 rn = Dot(r, solN);
								if (rn < hMin) {
									const float32 hn = hMin < b.length ? hMin : b.length;
									const NkVec3f rt = Sub(r, Scale(solN, rn));
									const float32 reste = b.length * b.length - hn * hn;
									r = Add(Scale(solN, hn), Scale(Normalize(rt, AnyPerpendicular(solN)),
																	 std::sqrt(reste > 0.f ? reste : 0.f)));
								}
							}
							if (!explicite)
								b.v = Add(Scale(Sub(r, r0), 1.f / h), vAncre);
						}
						b.p = Add(ancre, r);						b.prevTarget = cible;
						b.prevAnchor = ancre;
						const float32 vit = Length(b.v);
						if (vit > mLastMaxSpeed)
							mLastMaxSpeed = vit;
						// L'os tourne vers sa pointe simulee, au dosage de la chaine.
						NkRtQuat q = QFromTo(Sub(cible, ancre), Sub(b.p, ancre));
						if (ch.weight < 0.999f)
							q = QScale(q, Clamp(ch.weight, 0.f, 1.f));
						const float32 dev = QAngle(q);
						if (dev > mLastMaxDev)
							mLastMaxDev = dev;
						RotateAbout(world[b.joint], q, ancre);
						rig.RefreshDescendants(b.joint, local, world);
					}
				}
			}

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
