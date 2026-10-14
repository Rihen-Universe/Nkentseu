// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkRtConstraints.cpp : les contraintes d'os par image (voir NkRtConstraints.h).
// =============================================================================
#include "NKAnima/Realtime/NkRtConstraints.h"
#include "NKAnima/Realtime/NkRtTwist.h"

namespace nkentseu {
	namespace anim {
		namespace rt {

			namespace {
				constexpr float32 kPi = 3.14159265358979f;
				uint32 gConstraintMutations = 0u;

				/// La fraction `w` du chemin de `from` a `to` (rotation dans le repere de
				/// `from`) : from * (from^-1 * to)^w. A 0 : `from` ; a 1 : `to`.
				NkRtQuat Dose(const NkRtQuat &from, const NkRtQuat &to, float32 w) {
					return QNormalize(QMul(from, QScale(QMul(QConj(from), to), w)));
				}

				/// L'angle (radians) entre deux directions, par atan2 : acos pres de 0 perd
				/// 0,02 degre en float (acos(1 - 6e-8) = 3,5e-4 rad), atan2 ne perd rien.
				float32 Angle(const NkVec3f &a, const NkVec3f &b) {
					return std::atan2(Length(Cross(a, b)), Dot(a, b));
				}

				/// Le monde du parent de `j`, ou l'identite pour une racine.
				NkMat4f ParentWorld(const NkRtRig &rig, const NkMat4f *world, uint32 j) {
					const int32 p = rig.Parent(j);
					return p >= 0 ? world[(uint32)p] : NkMat4f::Identity();
				}

				/// Chaque joint de la chaine existe et descend du precedent.
				bool ChaineValide(const NkRtRig &rig, const uint32 *chain, uint32 count) {
					if (chain == nullptr || count < 2u || count > kNkRtChainMax) {
						return false;
					}
					// MUTATION chain_no_check : la filiation n'est plus verifiee.
					const bool filiation = !NkRtConstraintMutated(NK_RTC_MUT_CHAIN_NO_CHECK);
					for (uint32 k = 0; k < count; ++k) {
						if (chain[k] >= rig.Count() ||
							(filiation && k > 0 && (chain[k] == chain[k - 1] || !rig.IsInSubtree(chain[k], chain[k - 1])))) {
							return false;
						}
					}
					return true;
				}

				/// LES POSITIONS DEVIENNENT DES ROTATIONS (la chaine IK et la chaine sur une
				/// courbe) : chaque joint vise la place voulue du suivant (rotation la plus
				/// courte : le roulis de la pose est garde), le bout prend `endRotation` s'il
				/// est donne ; puis le DOSAGE : chaque rotation LOCALE a la fraction w du chemin
				/// entre la pose donnee (inWorld, inParent) et la solution, reconstruite de la
				/// base vers le bout.
				void PoserChaine(const NkRtRig &rig, const NkMat4f *local, NkMat4f *world, const uint32 *chain, uint32 n, const NkVec3f *p, float32 w,
								 const NkMat4f *inWorld, const NkMat4f *inParent, const NkRtQuat *endRotation) {
					for (uint32 k = 0; k + 1 < n; ++k) {
						const uint32 c = chain[k];
						const NkVec3f cur = Origin(world[c]);
						const NkVec3f nxt = Origin(world[chain[k + 1]]);
						RotateAbout(world[c], QFromTo(Sub(nxt, cur), Sub(p[k + 1], cur)), cur);
						rig.RefreshDescendants(c, local, world);
					}
					const uint32 end = chain[n - 1];
					if (endRotation != nullptr) {
						world[end] = MatFrom(QNormalize(*endRotation), AxisScales(world[end]), Origin(world[end]));
						rig.RefreshDescendants(end, local, world);
					}
					if (w < 1.f) {
						NkRtQuat solved[kNkRtChainMax];
						for (uint32 k = 0; k < n; ++k) {
							const NkMat4f P = ParentWorld(rig, world, chain[k]);
							solved[k] = QMul(QConj(QFromMat(P)), QFromMat(world[chain[k]]));
						}
						for (uint32 k = 0; k < n; ++k) {
							const uint32 c = chain[k];
							const NkMat4f inLocal = inParent[k].Inverse() * inWorld[k];
							const NkRtQuat q = Dose(QFromMat(inLocal), solved[k], w);
							world[c] = ParentWorld(rig, world, c) * MatFrom(q, AxisScales(inLocal), Origin(inLocal));
							rig.RefreshDescendants(c, local, world);
						}
					}
				}

				/// Un point de la Catmull-Rom uniforme du segment (p1, p2), voisins p0 et p3.
				NkVec3f CatmullRom(const NkVec3f &p0, const NkVec3f &p1, const NkVec3f &p2, const NkVec3f &p3, float32 t) {
					const float32 t2 = t * t, t3 = t2 * t;
					const float32 a = -0.5f * t3 + t2 - 0.5f * t, b = 1.5f * t3 - 2.5f * t2 + 1.f;
					const float32 c = -1.5f * t3 + 2.f * t2 + 0.5f * t, d = 0.5f * t3 - 0.5f * t2;
					return Add(Add(Scale(p0, a), Scale(p1, b)), Add(Scale(p2, c), Scale(p3, d)));
				}

				/// Les deux axes de balancement (e1, e2) perpendiculaires a l'axe de torsion.
				void SwingFrame(const NkRtSwingTwistLimit &limit, NkVec3f &axis, NkVec3f &e1, NkVec3f &e2) {
					axis = Normalize(limit.twistAxis, V3(0.f, 1.f, 0.f));
					e1 = Sub(limit.swingAxis, Scale(axis, Dot(limit.swingAxis, axis)));
					e1 = Normalize(e1, AnyPerpendicular(axis));
					e2 = Cross(axis, e1);
				}
			} // namespace

			void NkRtConstraintSetMutations(uint32 bits) noexcept {
				gConstraintMutations = bits;
			}
			uint32 NkRtConstraintMutations() noexcept {
				return gConstraintMutations;
			}

			// ── 1. L'IK d'une chaine de N os ─────────────────────────────────────
			NkRtChainIKResult NkRtSolveChainIK(const NkRtRig &rig, const NkMat4f *local, NkMat4f *world, const uint32 *chain, uint32 count,
											   const NkVec3f &target, const NkRtChainIKSettings &settings) {
				NkRtChainIKResult res;
				if (!ChaineValide(rig, chain, count)) {
					return res; // chaque joint doit descendre du precedent
				}
				const uint32 n = count;
				const uint32 end = chain[n - 1];
				const float32 tol = settings.tolerance > 0.f ? settings.tolerance : 1e-6f;
				const float32 w = NkRtConstraintMutated(NK_RTC_MUT_WEIGHT_FULL) ? 1.f : Clamp(settings.weight, 0.f, 1.f);
				if (w <= 0.f) {
					res.distance = Distance(Origin(world[end]), target);
					res.reached = res.distance <= tol;
					return res; // la pose donnee, au bit pres
				}

				// La pose donnee : positions, longueurs, et de quoi doser ensuite.
				NkVec3f p[kNkRtChainMax];
				float32 len[kNkRtChainMax];
				NkMat4f inWorld[kNkRtChainMax], inParent[kNkRtChainMax];
				float32 total = 0.f;
				for (uint32 k = 0; k < n; ++k) {
					p[k] = Origin(world[chain[k]]);
					inWorld[k] = world[chain[k]];
					inParent[k] = ParentWorld(rig, world, chain[k]);
				}
				for (uint32 k = 0; k + 1 < n; ++k) {
					len[k] = Distance(p[k], p[k + 1]);
					total += len[k];
				}
				if (total < 1e-8f) {
					res.distance = Distance(Origin(world[end]), target);
					return res;
				}
				const NkVec3f base = p[0];
				const bool pole = settings.usePole && !NkRtConstraintMutated(NK_RTC_MUT_CHAIN_POLE_OFF);
				const bool borne = settings.maxBend < kPi - 1e-6f && !NkRtConstraintMutated(NK_RTC_MUT_BEND_FREE);

				uint32 it = 0;
				if (Distance(base, target) >= total) {
					// Hors d'allonge : la chaine se tend vers la cible.
					const NkVec3f dir = Normalize(Sub(target, base), Normalize(Sub(p[n - 1], base), V3(0.f, 1.f, 0.f)));
					for (uint32 k = 0; k + 1 < n; ++k) {
						p[k + 1] = Madd(p[k], dir, len[k]);
					}
				} else {
					// Une chaine DROITE ne sait pas de quel cote plier : avec un pole, on amorce
					// de ce cote les joints qui sont sur la droite base-bout. L'amorce fausse
					// les longueurs : FABRIK fait alors au moins une passe pour les rendre
					// exactes (sans elle, une chaine deja a sa cible gardait l'amorce : le
					// coude deplace de 2 %, mesure par CT11).
					bool amorce = false;
					if (pole && n >= 3) {
						const NkVec3f axe = Normalize(Sub(p[n - 1], base), V3(0.f, 1.f, 0.f));
						for (uint32 k = 1; k + 1 < n; ++k) {
							NkVec3f hors = Sub(p[k], base);
							hors = Sub(hors, Scale(axe, Dot(hors, axe)));
							if (Dot(hors, hors) > 1e-4f * len[k - 1] * len[k - 1]) {
								continue; // deja plie : il sait de quel cote
							}
							NkVec3f vp = Sub(settings.pole, p[k]);
							vp = Sub(vp, Scale(axe, Dot(vp, axe)));
							if (Dot(vp, vp) > 1e-12f) {
								p[k] = Madd(p[k], Normalize(vp, V3(0.f, 0.f, 0.f)), 0.02f * len[k - 1]);
								amorce = true;
							}
						}
					}
					for (it = 0; it < settings.iterations; ++it) {
						if (!(amorce && it == 0) && Distance(p[n - 1], target) <= tol) {
							break;
						}
						// Du bout vers la base : le bout sur la cible.
						p[n - 1] = target;
						for (uint32 k = n - 1; k-- > 0;) {
							NkVec3f seg = Normalize(Sub(p[k + 1], p[k]), V3(0.f, 1.f, 0.f)); // l'os k, de k vers k+1
							// Le pli du joint k+1 (entre l'os k et l'os k+1), borne aussi a
							// rebours : sans cela, la passe aller ecrase ce que la passe retour
							// a plie, et la chaine n'arrive pas (mesure : 52 mm a 0,9 L).
							if (borne && k + 2 < n) {
								const NkVec3f apres = Normalize(Sub(p[k + 2], p[k + 1]), seg);
								if (Angle(seg, apres) > settings.maxBend) {
									NkVec3f axe = Cross(apres, seg);
									axe = Dot(axe, axe) > 1e-14f ? Normalize(axe, AnyPerpendicular(apres)) : AnyPerpendicular(apres);
									seg = QRotate(QFromAxisAngle(axe, settings.maxBend), apres);
								}
							}
							p[k] = Madd(p[k + 1], seg, -len[k]);
						}
						// De la base vers le bout : la base recollee.  MUTATION chain_no_anchor.
						if (!NkRtConstraintMutated(NK_RTC_MUT_CHAIN_NO_ANCHOR)) {
							p[0] = base;
							for (uint32 k = 0; k + 1 < n; ++k) {
								NkVec3f dir = Normalize(Sub(p[k + 1], p[k]), V3(0.f, 1.f, 0.f));
								// Le pli du joint k (entre l'os d'avant et celui-ci) ramene au plus
								// grand admis, dans le meme plan.  MUTATION bend_free.
								if (borne && k >= 1) {
									const NkVec3f avant = Normalize(Sub(p[k], p[k - 1]), dir);
									if (Angle(avant, dir) > settings.maxBend) {
										NkVec3f axe = Cross(avant, dir);
										axe = Dot(axe, axe) > 1e-14f ? Normalize(axe, AnyPerpendicular(avant)) : AnyPerpendicular(avant);
										dir = QRotate(QFromAxisAngle(axe, settings.maxBend), avant);
									}
								}
								p[k + 1] = Madd(p[k], dir, len[k]);
							}
						}
					}
					// Le pole : chaque joint interieur tourne autour de la droite de ses deux
					// voisins (longueurs et extremites gardees) vers le cote du pole. Tourner
					// un joint deplace la droite de son voisin : quatre balayages.
					for (uint32 passe = 0; pole && n >= 3 && passe < 4u; ++passe) {
						for (uint32 k = 1; k + 1 < n; ++k) {
							const NkVec3f d = Sub(p[k + 1], p[k - 1]);
							if (Dot(d, d) < 1e-12f) {
								continue;
							}
							const NkVec3f axe = Normalize(d, V3(0.f, 1.f, 0.f));
							NkVec3f vj = Sub(p[k], p[k - 1]);
							NkVec3f vp = Sub(settings.pole, p[k - 1]);
							vj = Sub(vj, Scale(axe, Dot(vj, axe)));
							vp = Sub(vp, Scale(axe, Dot(vp, axe)));
							if (Dot(vj, vj) < 1e-12f || Dot(vp, vp) < 1e-12f) {
								continue;
							}
							const float32 angle = std::atan2(Dot(axe, Cross(vj, vp)), Dot(vj, vp));
							p[k] = Add(p[k - 1], QRotate(QFromAxisAngle(axe, angle), Sub(p[k], p[k - 1])));
						}
					}
				}

				PoserChaine(rig, local, world, chain, n, p, w, inWorld, inParent, settings.useEndRotation ? &settings.endRotation : nullptr);

				res.distance = Distance(Origin(world[end]), target);
				res.iterations = it;
				res.reached = res.distance <= tol;
				return res;
			}

			NkVec3f NkRtPoleFromPose(const NkMat4f *world, uint32 a, uint32 b, uint32 c, float32 distance, const NkVec3f &repli) noexcept {
				const NkVec3f A = Origin(world[a]), B = Origin(world[b]), C = Origin(world[c]);
				const NkVec3f ac = Sub(C, A);
				const float32 l2 = Dot(ac, ac);
				const NkVec3f pied = l2 > 1e-12f ? Madd(A, ac, Dot(Sub(B, A), ac) / l2) : A;
				// MUTATION pole_flat : le pied du milieu SUR la droite (a, c) : un pole qui ne
				// dit pas de quel cote plier.
				if (NkRtConstraintMutated(NK_RTC_MUT_POLE_FLAT)) {
					return pied;
				}
				const NkVec3f hors = Sub(B, pied);
				const float32 h = Length(hors);
				const NkVec3f dir = h > 1e-6f * (Length(ac) + 1e-6f) ? Scale(hors, 1.f / h) : repli;
				return Madd(B, dir, distance);
			}

			// ── 1 bis. La chaine sur une courbe ─────────────────────────────────────
			float32 NkRtSolveSplineIK(const NkRtRig &rig, const NkMat4f *local, NkMat4f *world, const uint32 *chain, uint32 count,
									  const NkVec3f *points, uint32 pointCount, const NkRtSplineIKSettings &settings) {
				if (!ChaineValide(rig, chain, count) || points == nullptr || pointCount < 2u || pointCount > kNkRtSplineMaxPoints) {
					return -1.f;
				}
				const uint32 n = count;
				const float32 w = NkRtConstraintMutated(NK_RTC_MUT_WEIGHT_FULL) ? 1.f : Clamp(settings.weight, 0.f, 1.f);
				if (w <= 0.f) {
					return 0.f; // la pose donnee, au bit pres
				}
				NkVec3f p[kNkRtChainMax];
				float32 len[kNkRtChainMax];
				NkMat4f inWorld[kNkRtChainMax], inParent[kNkRtChainMax];
				for (uint32 k = 0; k < n; ++k) {
					p[k] = Origin(world[chain[k]]);
					inWorld[k] = world[chain[k]];
					inParent[k] = ParentWorld(rig, world, chain[k]);
				}
				for (uint32 k = 0; k + 1 < n; ++k) {
					len[k] = Distance(p[k], p[k + 1]);
				}

				// La courbe, translatee sur la base, echantillonnee en ligne brisee fine.
				// Les bouts sont prolonges par reflexion (2 P0 - P1) : la courbe passe par
				// tous les points.  MUTATION spline_chord : le polygone des points.
				const NkVec3f decalage = Sub(p[0], points[0]);
				NkVec3f q[kNkRtSplineMaxPoints];
				for (uint32 i = 0; i < pointCount; ++i) {
					q[i] = Add(points[i], decalage);
				}
				const uint32 parSeg = settings.samplesPerSegment < 1u ? 1u : (settings.samplesPerSegment > 64u ? 64u : settings.samplesPerSegment);
				constexpr uint32 kMaxEch = (kNkRtSplineMaxPoints - 1u) * 64u + 1u;
				NkVec3f ech[kMaxEch];
				uint32 ne = 0;
				const bool corde = NkRtConstraintMutated(NK_RTC_MUT_SPLINE_CHORD);
				for (uint32 i = 0; i + 1 < pointCount; ++i) {
					const NkVec3f p1 = q[i], p2 = q[i + 1];
					const NkVec3f p0 = i > 0 ? q[i - 1] : Sub(Scale(p1, 2.f), p2);
					const NkVec3f p3 = i + 2 < pointCount ? q[i + 2] : Sub(Scale(p2, 2.f), p1);
					for (uint32 j = (i == 0 ? 0u : 1u); j <= parSeg; ++j) {
						const float32 t = (float32)j / (float32)parSeg;
						ech[ne++] = corde ? Lerp(p1, p2, t) : CatmullRom(p0, p1, p2, p3, t);
					}
				}

				// Chaque joint : le premier point de la ligne brisee, apres le precedent, a la
				// distance de son os (intersection d'une sphere et d'un segment).
				float32 horsCourbe = 0.f;
				uint32 seg = 0;			// le segment (ech[seg], ech[seg+1]) ou l'on en est
				float32 tSeg = 0.f;		// et la position sur lui
				for (uint32 k = 1; k < n; ++k) {
					const NkVec3f c = p[k - 1];
					const float32 r = len[k - 1];
					bool trouve = false;
					while (seg + 1 < ne && !trouve) {
						const NkVec3f A = ech[seg], B = ech[seg + 1];
						const NkVec3f d = Sub(B, A), f = Sub(A, c);
						const float32 a2 = Dot(d, d), b2 = 2.f * Dot(f, d), c2 = Dot(f, f) - r * r;
						const float32 disc = b2 * b2 - 4.f * a2 * c2;
						if (a2 > 1e-14f && disc >= 0.f) {
							const float32 s = std::sqrt(disc);
							const float32 t1 = (-b2 - s) / (2.f * a2), t2 = (-b2 + s) / (2.f * a2);
							// La racine la plus petite qui soit devant la position courante.
							const float32 t = t1 >= tSeg - 1e-6f && t1 <= 1.f ? t1 : t2;
							if (t >= tSeg - 1e-6f && t <= 1.f) {
								p[k] = Madd(A, d, t);
								tSeg = t;
								trouve = true;
								break;
							}
						}
						++seg;
						tSeg = 0.f;
					}
					if (!trouve) {
						// Au-dela de la courbe : droit dans la tangente de fin.
						const NkVec3f tangente = Normalize(Sub(ech[ne - 1], ech[ne - 2]), V3(0.f, 1.f, 0.f));
						p[k] = Madd(c, tangente, r);
						horsCourbe += r;
						seg = ne; // la suite aussi
					}
				}
				PoserChaine(rig, local, world, chain, n, p, w, inWorld, inParent, settings.useEndRotation ? &settings.endRotation : nullptr);
				return horsCourbe;
			}

			// ── 1 ter. L'allonge douce ─────────────────────────────────────────────
			float32 NkRtSoftReachDistance(float32 d, float32 totalLength, float32 softness) noexcept {
				// MUTATION soft_reach_off : la distance telle quelle.
				if (softness <= 0.f || NkRtConstraintMutated(NK_RTC_MUT_SOFT_REACH_OFF)) {
					return d;
				}
				const float32 s = softness < totalLength ? softness : totalLength;
				const float32 ds = totalLength - s;
				if (d <= ds) {
					return d;
				}
				return ds + s * (1.f - std::exp(-(d - ds) / s));
			}

			NkVec3f NkRtSoftReachTarget(const NkVec3f &base, const NkVec3f &target, float32 totalLength, float32 softness) noexcept {
				const NkVec3f v = Sub(target, base);
				const float32 d = Length(v);
				if (d < 1e-9f) {
					return target;
				}
				const float32 d2 = NkRtSoftReachDistance(d, totalLength, softness);
				return d2 == d ? target : Madd(base, v, d2 / d);
			}

			// ── 2. La visee ──────────────────────────────────────────────────────
			float32 NkRtAimBone(const NkRtRig &rig, const NkMat4f *local, NkMat4f *world, uint32 bone, const NkVec3f &target,
								const NkRtAimSettings &settings) {
				if (bone >= rig.Count()) {
					return -1.f;
				}
				const NkVec3f pos = Origin(world[bone]);
				const NkRtQuat q0 = QFromMat(world[bone]);
				const NkVec3f aimL = Normalize(settings.aimAxis, V3(0.f, 0.f, 1.f));
				const NkVec3f a0 = QRotate(q0, aimL);
				const NkVec3f d = Normalize(Sub(target, pos), a0);
				const float32 w = NkRtConstraintMutated(NK_RTC_MUT_WEIGHT_FULL) ? 1.f : Clamp(settings.weight, 0.f, 1.f);
				if (w <= 0.f) {
					return Angle(a0, d);
				}

				// Le cone, autour de la visee de la pose donnee.  MUTATION aim_no_limit.
				NkVec3f dl = d;
				const float32 maxA = Clamp(settings.maxAngle, 0.f, kPi);
				const float32 ang = Angle(a0, d);
				if (ang > maxA && !NkRtConstraintMutated(NK_RTC_MUT_AIM_NO_LIMIT)) {
					NkVec3f axe = Cross(a0, d);
					if (Dot(axe, axe) < 1e-12f) {
						axe = Cross(a0, settings.upWorld); // la cible juste derriere : on tourne par le haut
					}
					axe = Normalize(axe, AnyPerpendicular(a0));
					dl = QRotate(QFromAxisAngle(axe, maxA), a0);
				}

				// L'orientation pleine.  MUTATION aim_no_up : plus court chemin seul.
				NkRtQuat full;
				if (settings.keepUp && !NkRtConstraintMutated(NK_RTC_MUT_AIM_NO_UP)) {
					NkVec3f upL = Sub(settings.upAxis, Scale(aimL, Dot(settings.upAxis, aimL)));
					upL = Normalize(upL, AnyPerpendicular(aimL));
					NkVec3f upW = Sub(settings.upWorld, Scale(dl, Dot(settings.upWorld, dl)));
					if (Dot(upW, upW) < 1e-10f) {
						// Viser le haut du monde : le haut de l'os garde celui de la pose.
						const NkVec3f u0 = QRotate(q0, upL);
						upW = Sub(u0, Scale(dl, Dot(u0, dl)));
					}
					upW = Normalize(upW, AnyPerpendicular(dl));
					NkMat4f fl = NkMat4f::Identity(), fw = NkMat4f::Identity();
					SetAxes(fl, aimL, upL, Cross(aimL, upL));
					SetAxes(fw, dl, upW, Cross(dl, upW));
					full = QMul(QFromMat(fw), QConj(QFromMat(fl)));
				} else {
					full = QMul(QFromTo(a0, dl), q0);
				}

				const NkRtQuat q = Dose(q0, full, w);
				world[bone] = MatFrom(q, AxisScales(world[bone]), pos);
				rig.RefreshDescendants(bone, local, world);
				return Angle(QRotate(q, aimL), d);
			}

			// ── 3. Les limites d'angle ───────────────────────────────────────────
			void NkRtMeasureSwingTwist(const NkRtQuat &local, const NkRtQuat &ref, const NkRtSwingTwistLimit &limit, float32 &s1, float32 &s2,
									   float32 &twist) noexcept {
				NkVec3f axis, e1, e2;
				SwingFrame(limit, axis, e1, e2);
				const NkRtQuat d = QNormalize(QMul(QConj(ref), local));
				NkRtQuat swing, tw;
				NkRtSwingTwist(d, axis, swing, tw);
				const NkVec3f sv = QToRotVec(swing);
				s1 = Dot(sv, e1);
				s2 = Dot(sv, e2);
				twist = NkRtTwistAngle(d, axis);
			}

			bool NkRtLimitSwingTwist(NkRtQuat &local, const NkRtQuat &ref, const NkRtSwingTwistLimit &limit) noexcept {
				if (NkRtConstraintMutated(NK_RTC_MUT_LIMIT_OFF)) {
					return false;
				}
				NkVec3f axis, e1, e2;
				SwingFrame(limit, axis, e1, e2);
				float32 s1 = 0.f, s2 = 0.f, tw = 0.f;
				NkRtMeasureSwingTwist(local, ref, limit, s1, s2, tw);
				bool borne = false;
				const float32 a1 = limit.swing1 > 1e-6f ? limit.swing1 : 1e-6f;
				const float32 a2 = limit.swing2 > 1e-6f ? limit.swing2 : 1e-6f;
				const float32 k = (s1 / a1) * (s1 / a1) + (s2 / a2) * (s2 / a2);
				if (k > 1.f + 1e-6f) {
					const float32 f = 1.f / std::sqrt(k);
					s1 *= f;
					s2 *= f;
					borne = true;
				}
				if (tw < limit.twistMin) {
					tw = limit.twistMin;
					borne = true;
				} else if (tw > limit.twistMax) {
					tw = limit.twistMax;
					borne = true;
				}
				if (!borne) {
					return false; // deja admise : rendue au bit pres
				}
				const NkRtQuat swing = QFromRotVec(Add(Scale(e1, s1), Scale(e2, s2)));
				local = QNormalize(QMul(ref, QMul(swing, QFromAxisAngle(axis, tw))));
				return true;
			}

			bool NkRtLimitBone(const NkRtRig &rig, const NkMat4f *local, NkMat4f *world, uint32 bone, const NkRtQuat &refLocal,
							   const NkRtSwingTwistLimit &limit) {
				if (bone >= rig.Count()) {
					return false;
				}
				const NkMat4f P = ParentWorld(rig, world, bone);
				const NkMat4f L = P.Inverse() * world[bone];
				NkRtQuat q = QFromMat(L);
				if (!NkRtLimitSwingTwist(q, refLocal, limit)) {
					return false;
				}
				world[bone] = P * MatFrom(q, AxisScales(L), Origin(L));
				rig.RefreshDescendants(bone, local, world);
				return true;
			}

			// ── 4. La copie de rotation ──────────────────────────────────────────
			float32 NkRtCopyRotation(const NkRtRig &rig, const NkMat4f *local, NkMat4f *world, uint32 source, const NkRtQuat &sourceRefLocal,
									 uint32 target, const NkRtCopyRotationSettings &settings) {
				if (source >= rig.Count() || target >= rig.Count() || source == target) {
					return 0.f;
				}
				const float32 f = NkRtConstraintMutated(NK_RTC_MUT_WEIGHT_FULL) ? 1.f : settings.factor;
				if (f == 0.f) {
					return 0.f; // la pose donnee, au bit pres
				}
				// L'ecart de la source a sa reference, dans SON repere de reference.
				const NkMat4f Ps = ParentWorld(rig, world, source);
				const NkRtQuat sLocal = QFromMat(Ps.Inverse() * world[source]);
				NkRtQuat D = QNormalize(QMul(QConj(sourceRefLocal), sLocal));
				if (settings.twistOnly) {
					NkRtQuat swing, twist;
					NkRtSwingTwist(D, Normalize(settings.twistAxis, V3(0.f, 1.f, 0.f)), swing, twist);
					D = twist;
				}
				D = QScale(D, f);
				const NkVec3f pivot = Origin(world[target]);
				if (settings.space == NkRtCopySpace::NK_LOCAL) {
					const NkMat4f Pt = ParentWorld(rig, world, target);
					const NkMat4f L = Pt.Inverse() * world[target];
					world[target] = Pt * MatFrom(QNormalize(QMul(QFromMat(L), D)), AxisScales(L), Origin(L));
				} else {
					// Dans le monde : la place qu'aurait la source a sa reference sous son parent
					// ACTUEL.  MUTATION copy_world_naive : sa reference du monde prise sans le
					// parent (la rotation du parent passe pour une rotation de la source).
					NkRtQuat refMonde = QMul(QFromMat(Ps), sourceRefLocal);
					if (NkRtConstraintMutated(NK_RTC_MUT_COPY_WORLD_NAIVE)) {
						refMonde = sourceRefLocal;
						D = QScale(QNormalize(QMul(QConj(refMonde), QFromMat(world[source]))), f);
					}
					const NkRtQuat Dw = QNormalize(QMul(QMul(refMonde, D), QConj(refMonde)));
					RotateAbout(world[target], Dw, pivot);
				}
				rig.RefreshDescendants(target, local, world);
				return QAngle(D);
			}

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
