// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkAutoRig.cpp — le rig automatique : reperes, modeles, controles, IK a deux
// os (voir NkAutoRig.h).
// =============================================================================
#include "NKAnima/Rig/NkAutoRig.h"
#include "NKAnima/Rig/NkRigMath.h"
#include "NKContainers/Utilities/NkSort.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace anim {

		using math::NkMat4f;
		using math::NkVec3f;
		using namespace rigm;

		const char *NkRigTemplateName(NkRigTemplate t) {
			switch (t) {
				case NkRigTemplate::NK_RigTemplate_Humanoide: return "Humanoide";
				case NkRigTemplate::NK_RigTemplate_Quadrupede: return "Quadrupede";
				case NkRigTemplate::NK_RigTemplate_Chaine: return "Chaine (queue, tentacule, serpent)";
				default: return "?";
			}
		}

		const char *NkRigControlKindName(NkRigControlKind k) {
			switch (k) {
				case NkRigControlKind::NK_RigControlKind_IK_Deux_Os: return "IK a deux os";
				case NkRigControlKind::NK_RigControlKind_Colonne: return "Colonne";
				case NkRigControlKind::NK_RigControlKind_Racine: return "Racine";
				case NkRigControlKind::NK_RigControlKind_Regard: return "Regard";
				default: return "?";
			}
		}

		namespace {
			struct Def {
					const char *id;
					const char *label;
			};
			const Def kHumanoide[] = {
				{"sommet_tete", "Sommet de la tête"}, {"menton", "Menton"},
				{"epaule.L", "Épaule gauche"},		   {"epaule.R", "Épaule droite"},
				{"coude.L", "Coude gauche"},		   {"coude.R", "Coude droit"},
				{"poignet.L", "Poignet gauche"},	   {"poignet.R", "Poignet droit"},
				{"main.L", "Bout de la main gauche"},  {"main.R", "Bout de la main droite"},
				{"aine", "Aine"},					   {"genou.L", "Genou gauche"},
				{"genou.R", "Genou droit"},			   {"cheville.L", "Cheville gauche"},
				{"cheville.R", "Cheville droite"},	   {"orteils.L", "Orteils gauches"},
				{"orteils.R", "Orteils droits"},
			};
			const Def kQuadrupede[] = {
				{"museau", "Bout du museau"},			 {"crane", "Crâne"},
				{"garrot", "Garrot"},					 {"bassin", "Bassin"},
				{"queue_base", "Base de la queue"},		 {"queue_bout", "Bout de la queue"},
				{"epaule_av.L", "Épaule avant gauche"},	 {"epaule_av.R", "Épaule avant droite"},
				{"coude_av.L", "Coude avant gauche"},	 {"coude_av.R", "Coude avant droit"},
				{"poignet_av.L", "Poignet avant gauche"}, {"poignet_av.R", "Poignet avant droit"},
				{"patte_av.L", "Patte avant gauche"},	 {"patte_av.R", "Patte avant droite"},
				{"hanche_ar.L", "Hanche arrière gauche"}, {"hanche_ar.R", "Hanche arrière droite"},
				{"genou_ar.L", "Genou arrière gauche"},	 {"genou_ar.R", "Genou arrière droit"},
				{"jarret_ar.L", "Jarret gauche"},		 {"jarret_ar.R", "Jarret droit"},
				{"patte_ar.L", "Patte arrière gauche"},	 {"patte_ar.R", "Patte arrière droite"},
			};
			const Def kChaine[] = {{"debut", "Début de la chaîne"}, {"fin", "Fin de la chaîne"}};

			void Liste(NkRigTemplate t, const Def *&d, uint32 &n) {
				if (t == NkRigTemplate::NK_RigTemplate_Quadrupede) {
					d = kQuadrupede;
					n = (uint32)(sizeof(kQuadrupede) / sizeof(kQuadrupede[0]));
				} else if (t == NkRigTemplate::NK_RigTemplate_Chaine) {
					d = kChaine;
					n = 2;
				} else {
					d = kHumanoide;
					n = (uint32)(sizeof(kHumanoide) / sizeof(kHumanoide[0]));
				}
			}

			void Poser(NkVector<NkRigLandmark> &l, const char *id, const NkVec3f &p) {
				const int32 i = NkRigFindLandmark(l, id);
				if (i >= 0) {
					l[(uint32)i].position = p;
				}
			}
			NkVec3f Lire(const NkVector<NkRigLandmark> &l, const char *id, const NkVec3f &repli = V(0.f, 0.f, 0.f)) {
				const int32 i = NkRigFindLandmark(l, id);
				return i >= 0 ? l[(uint32)i].position : repli;
			}
			/// « coude » + cote -> « coude.L »
			NkString Cote(const char *base, bool gauche) {
				NkString s(base);
				s.Append(gauche ? ".L" : ".R");
				return s;
			}

			// ── LES COUPES horizontales du maillage ───────────────────────────────
			struct Ilot {
					float32 x0 = 0.f, x1 = 0.f;
					float32 sx = 0.f, sz = 0.f;
					uint32 n = 0;
					float32 Cx() const {
						return n ? sx / (float32)n : (x0 + x1) * 0.5f;
					}
					float32 Cz() const {
						return n ? sz / (float32)n : 0.f;
					}
			};
			/// Les ilots (le long de X) de la coupe y : les aretes qui traversent le
			/// plan donnent des points, rassembles par cases de largeur `pas`.
			void Coupe(const NkSkinMesh &m, float32 y, float32 xmin, float32 pas, uint32 cases, NkVector<Ilot> &out) {
				out.Clear();
				NkVector<uint32> nb;
				NkVector<float32> sx, sz;
				nb.Resize(cases, 0);
				sx.Resize(cases, 0.f);
				sz.Resize(cases, 0.f);
				const uint32 nt = m.TriangleCount();
				for (uint32 t = 0; t < nt; ++t) {
					for (uint32 e = 0; e < 3; ++e) {
						const NkVec3f &p = m.positions[m.indices[t * 3 + e]];
						const NkVec3f &q = m.positions[m.indices[t * 3 + (e + 1) % 3]];
						if ((p.y - y) * (q.y - y) >= 0.f) {
							continue;
						}
						const float32 k = (y - p.y) / (q.y - p.y);
						const float32 x = p.x + (q.x - p.x) * k, z = p.z + (q.z - p.z) * k;
						int32 c = (int32)((x - xmin) / pas);
						c = c < 0 ? 0 : (c >= (int32)cases ? (int32)cases - 1 : c);
						nb[(uint32)c] += 1;
						sx[(uint32)c] += x;
						sz[(uint32)c] += z;
					}
				}
				// Dilatation d'une case : un trou d'une case n'est pas un vide.
				NkVector<uint8> occ;
				occ.Resize(cases, 0);
				for (uint32 c = 0; c < cases; ++c) {
					if (nb[c]) {
						occ[c] = 1;
						if (c > 0) {
							occ[c - 1] = 1;
						}
						if (c + 1 < cases) {
							occ[c + 1] = 1;
						}
					}
				}
				for (uint32 c = 0; c < cases;) {
					if (!occ[c]) {
						++c;
						continue;
					}
					Ilot il;
					il.x0 = xmin + (float32)c * pas;
					while (c < cases && occ[c]) {
						il.n += nb[c];
						il.sx += sx[c];
						il.sz += sz[c];
						++c;
					}
					il.x1 = xmin + (float32)c * pas;
					if (il.n > 0) {
						out.PushBack(il);
					}
				}
			}

			/// L'ilot qui contient x (ou le plus proche), -1 si aucun.
			int32 IlotDe(const NkVector<Ilot> &il, float32 x) {
				int32 best = -1;
				float32 d = 1e30f;
				for (uint32 i = 0; i < (uint32)il.Size(); ++i) {
					const float32 e = x < il[i].x0 ? il[i].x0 - x : (x > il[i].x1 ? x - il[i].x1 : 0.f);
					if (e < d) {
						d = e;
						best = (int32)i;
					}
				}
				return best;
			}

			/// L'axe principal d'un nuage (iteration de puissance sur la covariance).
			NkVec3f AxePrincipal(const NkVector<NkVec3f> &p, const NkVec3f &c) {
				float32 xx = 0, xy = 0, xz = 0, yy = 0, yz = 0, zz = 0;
				for (uint32 i = 0; i < (uint32)p.Size(); ++i) {
					const NkVec3f d = Sub(p[i], c);
					xx += d.x * d.x;
					xy += d.x * d.y;
					xz += d.x * d.z;
					yy += d.y * d.y;
					yz += d.y * d.z;
					zz += d.z * d.z;
				}
				NkVec3f v = V(1.f, 0.3f, 0.1f);
				for (int32 it = 0; it < 64; ++it) {
					v = Norm(V(xx * v.x + xy * v.y + xz * v.z, xy * v.x + yy * v.y + yz * v.z, xz * v.x + yz * v.y + zz * v.z), v);
				}
				return v;
			}

			NkVec3f Centre(const NkVector<NkVec3f> &p) {
				NkVec3f s = V(0.f, 0.f, 0.f);
				for (uint32 i = 0; i < (uint32)p.Size(); ++i) {
					s = Add(s, p[i]);
				}
				return p.Empty() ? s : Mul(s, 1.f / (float32)p.Size());
			}

			/// Le centre de la section du membre en P (points proches du plan normal a d).
			NkVec3f CentreSection(const NkVector<NkVec3f> &p, const NkVec3f &P, const NkVec3f &d, float32 epaisseur, float32 rayon) {
				NkVec3f s = V(0.f, 0.f, 0.f);
				uint32 n = 0;
				for (uint32 i = 0; i < (uint32)p.Size(); ++i) {
					const NkVec3f q = Sub(p[i], P);
					const float32 a = Dot(q, d);
					if (std::fabs(a) < epaisseur && Len(Sub(q, Mul(d, a))) < rayon) {
						s = Add(s, p[i]);
						++n;
					}
				}
				if (n < 3) {
					return P;
				}
				const NkVec3f c = Mul(s, 1.f / (float32)n);
				// Le centre se projette sur le plan de la section : la position le long du
				// membre reste celle demandee.
				return Add(P, Sub(Sub(c, P), Mul(d, Dot(Sub(c, P), d))));
			}
		} // namespace

		int32 NkRigFindLandmark(const NkVector<NkRigLandmark> &l, const char *id) {
			if (id == nullptr) {
				return -1;
			}
			for (uint32 i = 0; i < (uint32)l.Size(); ++i) {
				if (std::strcmp(l[i].id.CStr(), id) == 0) {
					return (int32)i;
				}
			}
			return -1;
		}

		bool NkRigMoveLandmark(NkVector<NkRigLandmark> &l, const char *id, const NkVec3f &p, bool symetrie) {
			const int32 i = NkRigFindLandmark(l, id);
			if (i < 0) {
				return false;
			}
			l[(uint32)i].position = p;
			l[(uint32)i].manuel = true;
			if (symetrie) {
				const NkString m = NkArmature::MirrorName(id);
				const int32 j = m.Empty() ? -1 : NkRigFindLandmark(l, m.CStr());
				if (j >= 0) {
					l[(uint32)j].position = MiroirX(p);
					l[(uint32)j].manuel = true;
				} else {
					// Un repere du milieu reste sur le plan de symetrie.
					l[(uint32)i].position.x = NkArmature::SideOfName(id) == NkBoneSide::NK_BoneSide_Centre ? 0.f : p.x;
				}
			}
			return true;
		}

		void NkRigDefaultLandmarks(NkRigTemplate t, const NkVec3f &bmin, const NkVec3f &bmax, NkVector<NkRigLandmark> &out) {
			const Def *d = nullptr;
			uint32 n = 0;
			Liste(t, d, n);
			out.Clear();
			for (uint32 i = 0; i < n; ++i) {
				NkRigLandmark r;
				r.id = NkString(d[i].id);
				r.label = NkString(d[i].label);
				out.PushBack(r);
			}
			const NkVec3f e = Sub(bmax, bmin);
			const float32 cx = (bmin.x + bmax.x) * 0.5f, cz = (bmin.z + bmax.z) * 0.5f;
			auto P = [&](float32 fx, float32 fy, float32 fz) { return V(cx + fx * e.x * 0.5f, bmin.y + fy * e.y, cz + fz * e.z * 0.5f); };
			if (t == NkRigTemplate::NK_RigTemplate_Humanoide) {
				// Un T : proportions moyennes (hauteur 1 ; aine a 0,47 ; epaules a 0,81).
				Poser(out, "sommet_tete", P(0.f, 1.f, 0.f));
				Poser(out, "menton", P(0.f, 0.87f, 0.f));
				Poser(out, "aine", P(0.f, 0.47f, 0.f));
				for (int32 s = 0; s < 2; ++s) {
					const bool g = s == 0;
					const float32 k = g ? 1.f : -1.f;
					Poser(out, Cote("epaule", g).CStr(), P(k * 0.22f, 0.81f, 0.f));
					Poser(out, Cote("coude", g).CStr(), P(k * 0.52f, 0.81f, 0.f));
					Poser(out, Cote("poignet", g).CStr(), P(k * 0.80f, 0.81f, 0.f));
					Poser(out, Cote("main", g).CStr(), P(k * 1.f, 0.81f, 0.f));
					Poser(out, Cote("genou", g).CStr(), P(k * 0.11f, 0.25f, 0.f));
					Poser(out, Cote("cheville", g).CStr(), P(k * 0.11f, 0.04f, 0.f));
					Poser(out, Cote("orteils", g).CStr(), P(k * 0.11f, 0.01f, 0.9f));
				}
			} else if (t == NkRigTemplate::NK_RigTemplate_Quadrupede) {
				Poser(out, "museau", P(0.f, 0.85f, 1.f));
				Poser(out, "crane", P(0.f, 0.95f, 0.75f));
				Poser(out, "garrot", P(0.f, 0.7f, 0.45f));
				Poser(out, "bassin", P(0.f, 0.7f, -0.55f));
				Poser(out, "queue_base", P(0.f, 0.72f, -0.75f));
				Poser(out, "queue_bout", P(0.f, 0.8f, -1.f));
				for (int32 s = 0; s < 2; ++s) {
					const bool g = s == 0;
					const float32 k = g ? 1.f : -1.f;
					Poser(out, Cote("epaule_av", g).CStr(), P(k * 0.5f, 0.6f, 0.45f));
					Poser(out, Cote("coude_av", g).CStr(), P(k * 0.5f, 0.35f, 0.4f));
					Poser(out, Cote("poignet_av", g).CStr(), P(k * 0.5f, 0.12f, 0.45f));
					Poser(out, Cote("patte_av", g).CStr(), P(k * 0.5f, 0.02f, 0.5f));
					Poser(out, Cote("hanche_ar", g).CStr(), P(k * 0.5f, 0.6f, -0.55f));
					Poser(out, Cote("genou_ar", g).CStr(), P(k * 0.5f, 0.38f, -0.45f));
					Poser(out, Cote("jarret_ar", g).CStr(), P(k * 0.5f, 0.18f, -0.65f));
					Poser(out, Cote("patte_ar", g).CStr(), P(k * 0.5f, 0.02f, -0.6f));
				}
			} else {
				Poser(out, "debut", P(0.f, 0.5f, -1.f));
				Poser(out, "fin", P(0.f, 0.5f, 1.f));
			}
		}

		// =====================================================================
		// LA DETECTION
		// =====================================================================
		namespace {
			bool DetecterHumanoide(const NkSkinMesh &m, const NkAutoRigOptions &opt, NkVector<NkRigLandmark> &l, NkAutoRigReport &rp) {
				const NkVec3f bmin = m.bmin, bmax = m.bmax;
				const float32 H = bmax.y - bmin.y, W = bmax.x - bmin.x;
				const float32 cx = (bmin.x + bmax.x) * 0.5f;
				if (!(H > 0.f) || !(W > 0.f)) {
					rp.message = NkString("maillage plat");
					return false;
				}
				rp.hauteur = H;
				const uint32 nc = opt.coupes < 24 ? 24 : opt.coupes;
				rp.coupes = nc;
				const uint32 cases = 200;
				const float32 pas = W * 1.02f / (float32)cases;
				const float32 xmin = bmin.x - W * 0.01f;
				NkVector<NkVector<Ilot>> coupes;
				coupes.Resize(nc);
				NkVector<float32> ys;
				ys.Resize(nc, 0.f);
				for (uint32 s = 0; s < nc; ++s) {
					ys[s] = bmin.y + H * ((float32)s + 0.5f) / (float32)nc;
					Coupe(m, ys[s], xmin, pas, cases, coupes[s]);
				}
				// ── L'AINE : la plus haute coupe ou les jambes sont SEPAREES (un vide
				//    au milieu, un ilot de chaque cote), en montant depuis le sol.
				float32 aineY = -1e30f;
				uint32 manque = 0;
				for (uint32 s = (uint32)(nc * 0.03f); s < (uint32)(nc * 0.7f); ++s) {
					const NkVector<Ilot> &il = coupes[s];
					bool vide = true, g = false, d = false;
					for (uint32 i = 0; i < (uint32)il.Size(); ++i) {
						if (il[i].x0 <= cx && il[i].x1 >= cx) {
							vide = false;
						}
						g = g || il[i].x0 > cx;
						d = d || il[i].x1 < cx;
					}
					if (vide && g && d) {
						aineY = ys[s] + H * 0.5f / (float32)nc;
						manque = 0;
					} else if (aineY > -1e29f && ++manque >= 2) {
						break;
					}
				}
				if (aineY < -1e29f) {
					rp.message = NkString("aine introuvable : aucune coupe ne montre deux jambes separees (le personnage est-il debout, Y en haut ?)");
					return false;
				}
				rp.aineY = aineY;
				// ── LA DEMI-LARGEUR DU TRONC : mediane, entre l'aine et la poitrine.
				NkVector<float32> demi;
				for (uint32 s = 0; s < nc; ++s) {
					if (ys[s] < aineY + 0.06f * H || ys[s] > aineY + 0.28f * H) {
						continue;
					}
					const int32 i = IlotDe(coupes[s], cx);
					if (i >= 0) {
						const Ilot &il = coupes[s][(uint32)i];
						demi.PushBack((cx - il.x0) > (il.x1 - cx) ? (cx - il.x0) : (il.x1 - cx));
					}
				}
				if (demi.Empty()) {
					rp.message = NkString("tronc introuvable au-dessus de l'aine");
					return false;
				}
				NkSort(demi.Data(), demi.Data() + demi.Size());
				const float32 demiTronc = demi[(uint32)demi.Size() / 2u];
				rp.demiTronc = demiTronc;
				// ── LES BRAS : les sommets loin du tronc, au-dessus des hanches.
				NkVec3f epaule[2], coude[2], poignet[2], main[2];
				for (int32 s = 0; s < 2; ++s) {
					const float32 k = s == 0 ? 1.f : -1.f;
					NkVector<NkVec3f> pts;
					for (uint32 v = 0; v < m.VertexCount(); ++v) {
						const NkVec3f &p = m.positions[v];
						if (p.y > aineY - 0.08f * H && k * (p.x - cx) > demiTronc * 1.3f) {
							pts.PushBack(p);
						}
					}
					if (pts.Size() < 12) {
						rp.message = NkString::Format("bras %s introuvable (rien au-dela de 1,3 fois le tronc)", s == 0 ? "gauche" : "droit");
						return false;
					}
					const NkVec3f c = Centre(pts);
					NkVec3f d = AxePrincipal(pts, c);
					if (k * d.x < 0.f) {
						d = Mul(d, -1.f);
					}
					float32 tMax = -1e30f;
					for (uint32 i = 0; i < (uint32)pts.Size(); ++i) {
						const float32 t = Dot(Sub(pts[i], c), d);
						tMax = t > tMax ? t : tMax;
					}
					const NkVec3f bout = Add(c, Mul(d, tMax));
					NkVec3f S;
					if (std::fabs(d.x) > 0.25f) {
						const float32 t = (cx + k * demiTronc * 0.85f - c.x) / d.x;
						S = Add(c, Mul(d, t));
					} else {
						// Bras le long du corps : l'epaule au haut du nuage.
						float32 haut = -1e30f;
						for (uint32 i = 0; i < (uint32)pts.Size(); ++i) {
							haut = pts[i].y > haut ? pts[i].y : haut;
						}
						S = V(cx + k * demiTronc * 0.85f, haut - 0.03f * H, c.z);
					}
					const float32 ep = 0.015f * H, ray = 0.08f * H;
					epaule[s] = S;
					coude[s] = CentreSection(pts, Lerp(S, bout, 0.42f), d, ep, ray);
					poignet[s] = CentreSection(pts, Lerp(S, bout, 0.755f), d, ep, ray);
					main[s] = Lerp(S, bout, 0.985f);
				}
				// ── LE COU : la coupe la plus etroite entre les epaules et le sommet.
				const float32 epauleY = (epaule[0].y + epaule[1].y) * 0.5f;
				float32 couY = epauleY + 0.05f * H, couL = 1e30f, couZ = 0.f;
				for (uint32 s = 0; s < nc; ++s) {
					if (ys[s] < epauleY + 0.02f * H || ys[s] > epauleY + 0.55f * (bmax.y - epauleY)) {
						continue;
					}
					const int32 i = IlotDe(coupes[s], cx);
					if (i < 0) {
						continue;
					}
					const Ilot &il = coupes[s][(uint32)i];
					if (il.x1 - il.x0 < couL) {
						couL = il.x1 - il.x0;
						couY = ys[s];
						couZ = il.Cz();
					}
				}
				float32 teteZ = couZ;
				{
					const uint32 s = nc - 2u;
					const int32 i = IlotDe(coupes[s], cx);
					teteZ = i >= 0 ? coupes[s][(uint32)i].Cz() : couZ;
				}
				// ── LES JAMBES : le centre de la section de chaque jambe ; les orteils.
				NkVec3f genou[2], cheville[2], orteils[2];
				for (int32 s = 0; s < 2; ++s) {
					const float32 k = s == 0 ? 1.f : -1.f;
					auto CentreJambe = [&](float32 y) -> NkVec3f {
						NkVector<Ilot> il;
						Coupe(m, y, xmin, pas, cases, il);
						int32 best = -1;
						for (uint32 i = 0; i < (uint32)il.Size(); ++i) {
							if (k * (il[i].Cx() - cx) > 0.f && (best < 0 || il[i].n > il[(uint32)best].n)) {
								best = (int32)i;
							}
						}
						return best >= 0 ? V(il[(uint32)best].Cx(), y, il[(uint32)best].Cz()) : V(cx + k * demiTronc * 0.5f, y, 0.f);
					};
					genou[s] = CentreJambe(bmin.y + (aineY - bmin.y) * 0.53f);
					cheville[s] = CentreJambe(bmin.y + (aineY - bmin.y) * 0.085f);
					// Les orteils : le sommet du pied le plus en avant (+Z).
					float32 zMax = -1e30f;
					NkVec3f bout = cheville[s];
					for (uint32 v = 0; v < m.VertexCount(); ++v) {
						const NkVec3f &p = m.positions[v];
						if (p.y < bmin.y + 0.06f * H && k * (p.x - cx) > 0.f && p.z > zMax) {
							zMax = p.z;
							bout = p;
						}
					}
					orteils[s] = V(bout.x, bmin.y + 0.015f * H, bout.z);
				}
				// La profondeur du bassin.
				float32 aineZ = 0.f;
				{
					NkVector<Ilot> il;
					Coupe(m, aineY + 0.03f * H, xmin, pas, cases, il);
					const int32 i = IlotDe(il, cx);
					aineZ = i >= 0 ? il[(uint32)i].Cz() : 0.f;
				}
				Poser(l, "sommet_tete", V(cx, bmax.y, teteZ));
				Poser(l, "menton", V(cx, couY + 0.03f * H, couZ));
				Poser(l, "aine", V(cx, aineY, aineZ));
				for (int32 s = 0; s < 2; ++s) {
					const bool g = s == 0;
					Poser(l, Cote("epaule", g).CStr(), epaule[s]);
					Poser(l, Cote("coude", g).CStr(), coude[s]);
					Poser(l, Cote("poignet", g).CStr(), poignet[s]);
					Poser(l, Cote("main", g).CStr(), main[s]);
					Poser(l, Cote("genou", g).CStr(), genou[s]);
					Poser(l, Cote("cheville", g).CStr(), cheville[s]);
					Poser(l, Cote("orteils", g).CStr(), orteils[s]);
				}
				rp.message = NkString::Format("humanoide : hauteur %.3f, aine a %.0f %% de la hauteur, tronc %.3f de large, %u coupes ; "
											  "suppose debout, Y en haut, face a +Z",
											  (double)H, (double)((aineY - bmin.y) / H * 100.f), (double)(demiTronc * 2.f), nc);
				return true;
			}

			bool DetecterQuadrupede(const NkSkinMesh &m, NkVector<NkRigLandmark> &l, NkAutoRigReport &rp) {
				const NkVec3f bmin = m.bmin, bmax = m.bmax;
				const float32 H = bmax.y - bmin.y, W = bmax.x - bmin.x, L = bmax.z - bmin.z;
				const float32 cx = (bmin.x + bmax.x) * 0.5f;
				if (!(H > 0.f) || !(L > 0.f)) {
					rp.message = NkString("maillage plat");
					return false;
				}
				rp.hauteur = H;
				// ── LES QUATRE PATTES : les sommets pres du sol, en quatre quarts.
				NkVector<NkVec3f> bas;
				float32 z0 = 1e30f, z1 = -1e30f;
				for (uint32 v = 0; v < m.VertexCount(); ++v) {
					const NkVec3f &p = m.positions[v];
					if (p.y < bmin.y + 0.12f * H) {
						bas.PushBack(p);
						z0 = p.z < z0 ? p.z : z0;
						z1 = p.z > z1 ? p.z : z1;
					}
				}
				const float32 zMil = (z0 + z1) * 0.5f;
				NkVec3f somme[4] = {V(0, 0, 0), V(0, 0, 0), V(0, 0, 0), V(0, 0, 0)};
				uint32 nb[4] = {0, 0, 0, 0}; // 0 av.L, 1 av.R, 2 ar.L, 3 ar.R
				for (uint32 i = 0; i < (uint32)bas.Size(); ++i) {
					const NkVec3f &p = bas[i];
					const uint32 q = (p.z >= zMil ? 0u : 2u) + (p.x >= cx ? 0u : 1u);
					somme[q] = Add(somme[q], p);
					++nb[q];
				}
				for (uint32 q = 0; q < 4; ++q) {
					if (nb[q] < 4) {
						rp.message = NkString("quatre pattes introuvables pres du sol (le corps est-il le long de Z, Y en haut ?)");
						return false;
					}
					somme[q] = Mul(somme[q], 1.f / (float32)nb[q]);
				}
				const float32 zAv = (somme[0].z + somme[1].z) * 0.5f, zAr = (somme[2].z + somme[3].z) * 0.5f;
				// ── LE VENTRE ET LE DOS entre les pattes.
				float32 ventre = 1e30f, dos = -1e30f;
				for (uint32 v = 0; v < m.VertexCount(); ++v) {
					const NkVec3f &p = m.positions[v];
					if (std::fabs(p.x - cx) < W * 0.12f && p.z < zAv && p.z > zAr) {
						ventre = p.y < ventre ? p.y : ventre;
						dos = p.y > dos ? p.y : dos;
					}
				}
				if (ventre > 1e29f) {
					ventre = bmin.y + 0.45f * H;
					dos = bmin.y + 0.75f * H;
				}
				const float32 ys = ventre + 0.6f * (dos - ventre);
				// ── LA TETE (vers +Z) et LA QUEUE (vers -Z).
				NkVec3f museau = V(cx, ys, bmax.z);
				float32 zMax = -1e30f, zMin = 1e30f;
				NkVec3f queue = V(cx, ys, bmin.z);
				for (uint32 v = 0; v < m.VertexCount(); ++v) {
					const NkVec3f &p = m.positions[v];
					if (p.y > ventre && p.z > zMax) {
						zMax = p.z;
						museau = p;
					}
					if (p.y > ventre && p.z < zMin) {
						zMin = p.z;
						queue = p;
					}
				}
				NkVector<NkVec3f> tete;
				for (uint32 v = 0; v < m.VertexCount(); ++v) {
					const NkVec3f &p = m.positions[v];
					if (p.z > museau.z - 0.2f * L && p.y > ventre) {
						tete.PushBack(p);
					}
				}
				const NkVec3f crane = tete.Empty() ? museau : Centre(tete);
				// LA QUEUE : la ou la coupe (en Z) du corps devient ETROITE derriere les
				// pattes arriere, puis les centres de ses deux bouts.
				float32 largeurRef = 0.f;
				auto Largeur = [&](float32 z) -> float32 {
					float32 x0 = 1e30f, x1 = -1e30f;
					for (uint32 v = 0; v < m.VertexCount(); ++v) {
						const NkVec3f &p = m.positions[v];
						if (std::fabs(p.z - z) < L / 120.f && p.y > ventre) {
							x0 = p.x < x0 ? p.x : x0;
							x1 = p.x > x1 ? p.x : x1;
						}
					}
					return x1 > x0 ? x1 - x0 : 0.f;
				};
				largeurRef = Largeur(zAr);
				float32 zQueue = zAr - 0.1f * L;
				for (float32 z = zAr; z > bmin.z; z -= L / 80.f) {
					if (Largeur(z) < 0.4f * largeurRef) {
						zQueue = z;
						break;
					}
				}
				NkVector<NkVec3f> qpts;
				float32 zBout = 1e30f;
				for (uint32 v = 0; v < m.VertexCount(); ++v) {
					const NkVec3f &p = m.positions[v];
					if (p.z < zQueue && p.y > ventre) {
						qpts.PushBack(p);
						zBout = p.z < zBout ? p.z : zBout;
					}
				}
				NkVec3f queueBase = V(cx, ys, zQueue), queueBout = V(cx, queue.y, queue.z);
				if (qpts.Size() >= 6) {
					NkVector<NkVec3f> debut, fin;
					for (uint32 i = 0; i < (uint32)qpts.Size(); ++i) {
						if (qpts[i].z > zQueue - 0.05f * L) {
							debut.PushBack(qpts[i]);
						}
						if (qpts[i].z < zBout + 0.04f * L) {
							fin.PushBack(qpts[i]);
						}
					}
					if (!debut.Empty()) {
						queueBase = Centre(debut);
					}
					if (!fin.Empty()) {
						queueBout = Centre(fin);
					}
				}
				Poser(l, "museau", V(cx, museau.y, museau.z));
				Poser(l, "crane", V(cx, crane.y, crane.z));
				Poser(l, "garrot", V(cx, ys, zAv));
				Poser(l, "bassin", V(cx, ys, zAr));
				Poser(l, "queue_base", V(cx, queueBase.y, queueBase.z));
				Poser(l, "queue_bout", V(cx, queueBout.y, queueBout.z));
				const float32 haut = ventre + 0.35f * (ys - ventre);
				for (int32 s = 0; s < 2; ++s) {
					const bool g = s == 0;
					const NkVec3f av = somme[s], ar = somme[2 + s];
					const NkVec3f topAv = V(av.x, haut, av.z), pAv = V(av.x, bmin.y + 0.02f * H, av.z);
					const NkVec3f topAr = V(ar.x, haut, ar.z), pAr = V(ar.x, bmin.y + 0.02f * H, ar.z);
					Poser(l, Cote("epaule_av", g).CStr(), topAv);
					Poser(l, Cote("coude_av", g).CStr(), Add(Lerp(topAv, pAv, 0.42f), V(0.f, 0.f, -0.02f * L)));
					Poser(l, Cote("poignet_av", g).CStr(), Lerp(topAv, pAv, 0.78f));
					Poser(l, Cote("patte_av", g).CStr(), pAv);
					Poser(l, Cote("hanche_ar", g).CStr(), topAr);
					Poser(l, Cote("genou_ar", g).CStr(), Add(Lerp(topAr, pAr, 0.38f), V(0.f, 0.f, 0.03f * L)));
					Poser(l, Cote("jarret_ar", g).CStr(), Add(Lerp(topAr, pAr, 0.7f), V(0.f, 0.f, -0.03f * L)));
					Poser(l, Cote("patte_ar", g).CStr(), pAr);
				}
				rp.message = NkString::Format("quadrupede : longueur %.3f, ventre a %.0f %% de la hauteur ; suppose debout, tete vers +Z", (double)L,
											  (double)((ventre - bmin.y) / H * 100.f));
				return true;
			}

			bool DetecterChaine(const NkSkinMesh &m, NkVector<NkRigLandmark> &l, NkAutoRigReport &rp) {
				if (m.VertexCount() < 2) {
					rp.message = NkString("maillage vide");
					return false;
				}
				const NkVec3f c = Centre(m.positions);
				const NkVec3f d = AxePrincipal(m.positions, c);
				float32 t0 = 1e30f, t1 = -1e30f;
				for (uint32 v = 0; v < m.VertexCount(); ++v) {
					const float32 t = Dot(Sub(m.positions[v], c), d);
					t0 = t < t0 ? t : t0;
					t1 = t > t1 ? t : t1;
				}
				const float32 marge = (t1 - t0) * 0.03f;
				Poser(l, "debut", Add(c, Mul(d, t0 + marge)));
				Poser(l, "fin", Add(c, Mul(d, t1 - marge)));
				rp.message = NkString::Format("chaine le long de l'axe principal (%.2f, %.2f, %.2f), longueur %.3f", (double)d.x, (double)d.y, (double)d.z,
											  (double)(t1 - t0));
				return true;
			}
		} // namespace

		bool NkRigDetectLandmarks(const NkSkinMesh &meshIn, NkRigTemplate t, const NkAutoRigOptions &opt, NkVector<NkRigLandmark> &out,
								  NkAutoRigReport *report) {
			NkAutoRigReport local;
			NkAutoRigReport &rp = report != nullptr ? *report : local;
			rp = NkAutoRigReport{};
			NkSkinMesh copie;
			const NkSkinMesh *pm = &meshIn;
			if (!(meshIn.Diagonal() > 0.f) && meshIn.VertexCount() > 0) {
				copie.positions = meshIn.positions;
				copie.indices = meshIn.indices;
				copie.BuildTopology();
				pm = &copie;
			}
			const NkSkinMesh &m = *pm;
			NkRigDefaultLandmarks(t, m.bmin, m.bmax, out);
			if (m.TriangleCount() == 0) {
				rp.message = NkString("maillage sans triangles");
				return false;
			}
			bool ok = false;
			if (t == NkRigTemplate::NK_RigTemplate_Humanoide) {
				ok = DetecterHumanoide(m, opt, out, rp);
			} else if (t == NkRigTemplate::NK_RigTemplate_Quadrupede) {
				ok = DetecterQuadrupede(m, out, rp);
			} else {
				ok = DetecterChaine(m, out, rp);
			}
			rp.ok = ok;
			return ok;
		}

		// =====================================================================
		// LA CONSTRUCTION
		// =====================================================================
		bool NkRigBuildArmature(NkRigTemplate t, const NkVector<NkRigLandmark> &l, const NkAutoRigOptions &opt, NkArmature &a) {
			a = NkArmature();
			if (t == NkRigTemplate::NK_RigTemplate_Chaine) {
				const NkVec3f d = Lire(l, "debut"), f = Lire(l, "fin");
				const uint32 n = opt.maillonsChaine < 1 ? 1 : (opt.maillonsChaine > 48 ? 48 : opt.maillonsChaine);
				int32 prec = -1;
				for (uint32 k = 0; k < n; ++k) {
					prec = a.Add("chaine", Lerp(d, f, (float32)k / (float32)n), Lerp(d, f, (float32)(k + 1) / (float32)n), prec, prec >= 0);
				}
				return a.Count() == n;
			}
			if (t == NkRigTemplate::NK_RigTemplate_Quadrupede) {
				const NkVec3f bassin = Lire(l, "bassin"), garrot = Lire(l, "garrot"), crane = Lire(l, "crane"), museau = Lire(l, "museau");
				const int32 hips = a.Add("hips", bassin, Lerp(bassin, garrot, 1.f / 3.f));
				const int32 spine = a.Add("spine", a.bones[(uint32)hips].tail, Lerp(bassin, garrot, 2.f / 3.f), hips, true);
				const int32 chest = a.Add("chest", a.bones[(uint32)spine].tail, garrot, spine, true);
				const int32 neck = a.Add("neck", garrot, crane, chest, true);
				(void)a.Add("head", crane, museau, neck, true);
				const NkVec3f qb = Lire(l, "queue_base"), qf = Lire(l, "queue_bout");
				int32 prec = hips;
				for (uint32 k = 0; k < 3; ++k) {
					prec = a.Add("tail", Lerp(qb, qf, (float32)k / 3.f), Lerp(qb, qf, (float32)(k + 1) / 3.f), prec, k > 0);
				}
				for (int32 s = 0; s < 2; ++s) {
					const bool g = s == 0;
					const int32 ua = a.Add(Cote("upperarm", g).CStr(), Lire(l, Cote("epaule_av", g).CStr()), Lire(l, Cote("coude_av", g).CStr()), chest, false);
					const int32 fa = a.Add(Cote("forearm", g).CStr(), a.bones[(uint32)ua].tail, Lire(l, Cote("poignet_av", g).CStr()), ua, true);
					(void)a.Add(Cote("hand", g).CStr(), a.bones[(uint32)fa].tail, Lire(l, Cote("patte_av", g).CStr()), fa, true);
					const int32 th = a.Add(Cote("thigh", g).CStr(), Lire(l, Cote("hanche_ar", g).CStr()), Lire(l, Cote("genou_ar", g).CStr()), hips, false);
					const int32 sh = a.Add(Cote("shin", g).CStr(), a.bones[(uint32)th].tail, Lire(l, Cote("jarret_ar", g).CStr()), th, true);
					(void)a.Add(Cote("foot", g).CStr(), a.bones[(uint32)sh].tail, Lire(l, Cote("patte_ar", g).CStr()), sh, true);
				}
				return true;
			}
			// ── HUMANOIDE ───────────────────────────────────────────────────────
			const NkVec3f aine = Lire(l, "aine"), menton = Lire(l, "menton"), sommet = Lire(l, "sommet_tete");
			const NkVec3f epG = Lire(l, "epaule.L"), epD = Lire(l, "epaule.R");
			const float32 H = sommet.y - Lire(l, "cheville.L").y > 1e-4f ? sommet.y - Lire(l, "cheville.L").y : 1.f;
			const float32 zColonne = (aine.z + menton.z) * 0.5f;
			const NkVec3f p0 = V(aine.x, aine.y + 0.04f * H, aine.z);
			const NkVec3f pn = V(aine.x, (epG.y + epD.y) * 0.5f + 0.01f * H, zColonne);
			const uint32 nv = opt.vertebres < 2 ? 2 : (opt.vertebres > 6 ? 6 : opt.vertebres);
			static const char *const kNoms[6] = {"hips", "spine", "chest", "chest.001", "chest.002", "chest.003"};
			const char *noms[6] = {"hips", "spine", "chest", "chest.001", "chest.002", "chest.003"};
			if (nv == 2) {
				noms[1] = "chest";
			}
			(void)kNoms;
			int32 prec = -1;
			for (uint32 k = 0; k < nv; ++k) {
				prec = a.Add(noms[k], Lerp(p0, pn, (float32)k / (float32)nv), Lerp(p0, pn, (float32)(k + 1) / (float32)nv), prec, prec >= 0);
			}
			const int32 hips = 0, chest = prec;
			const int32 neck = a.Add("neck", pn, V(menton.x, menton.y, menton.z), chest, true);
			(void)a.Add("head", a.bones[(uint32)neck].tail, sommet, neck, true);
			for (int32 s = 0; s < 2; ++s) {
				const bool g = s == 0;
				const NkVec3f ep = Lire(l, Cote("epaule", g).CStr()), co = Lire(l, Cote("coude", g).CStr());
				const NkVec3f po = Lire(l, Cote("poignet", g).CStr()), ma = Lire(l, Cote("main", g).CStr());
				const NkVec3f clav = V(pn.x + (ep.x - pn.x) * 0.18f, pn.y - 0.015f * H, (pn.z + ep.z) * 0.5f);
				const int32 sh = a.Add(Cote("shoulder", g).CStr(), clav, ep, chest, false);
				const int32 ua = a.Add(Cote("upperarm", g).CStr(), ep, co, sh, true);
				const int32 fa = a.Add(Cote("forearm", g).CStr(), co, po, ua, true);
				(void)a.Add(Cote("hand", g).CStr(), po, ma, fa, true);
				const NkVec3f ge = Lire(l, Cote("genou", g).CStr()), ch = Lire(l, Cote("cheville", g).CStr());
				const NkVec3f ot = Lire(l, Cote("orteils", g).CStr());
				const NkVec3f hanche = V(ge.x, p0.y - 0.035f * H, (aine.z + ge.z) * 0.5f);
				const int32 th = a.Add(Cote("thigh", g).CStr(), hanche, ge, hips, false);
				const int32 sn = a.Add(Cote("shin", g).CStr(), ge, ch, th, true);
				const NkVec3f balle = V(Lerp(ch, ot, 0.7f).x, ot.y + 0.01f * H, Lerp(ch, ot, 0.7f).z);
				const int32 ft = a.Add(Cote("foot", g).CStr(), ch, balle, sn, true);
				(void)a.Add(Cote("toe", g).CStr(), balle, ot, ft, true);
			}
			return true;
		}

		// =====================================================================
		// LES CONTROLES
		// =====================================================================
		void NkRigGenerateControls(NkRigTemplate t, const NkArmature &arm, NkVector<NkRigControl> &out) {
			out.Clear();
			auto Ik = [&](const char *nom, const char *a, const char *b, const char *c, const NkVec3f &dirPole) {
				const int32 ia = arm.Find(a), ib = arm.Find(b), ic = arm.Find(c);
				if (ia < 0 || ib < 0 || ic < 0) {
					return;
				}
				NkRigControl k;
				k.name = NkString(nom);
				k.kind = NkRigControlKind::NK_RigControlKind_IK_Deux_Os;
				k.chain.PushBack(ia);
				k.chain.PushBack(ib);
				k.chain.PushBack(ic);
				k.target = arm.bones[(uint32)ic].head;
				const NkVec3f coude = arm.bones[(uint32)ib].head;
				const float32 l = arm.Length((uint32)ia) + arm.Length((uint32)ib);
				// Le pole : du cote ou le coude (le genou) est deja plie, sinon `dirPole`.
				const NkVec3f milieu = Lerp(arm.bones[(uint32)ia].head, arm.bones[(uint32)ic].head, 0.5f);
				NkVec3f plie = Sub(coude, milieu);
				const NkVec3f axe = Norm(Sub(arm.bones[(uint32)ic].head, arm.bones[(uint32)ia].head));
				plie = Sub(plie, Mul(axe, Dot(plie, axe)));
				const NkVec3f dir = Len(plie) > l * 0.02f ? Norm(plie) : dirPole;
				k.pole = Add(coude, Mul(dir, l * 0.6f));
				out.PushBack(k);
			};
			auto Chaine = [&](const char *nom, NkRigControlKind kind, const char *const *os, uint32 n, bool bout) {
				NkRigControl k;
				k.name = NkString(nom);
				k.kind = kind;
				for (uint32 i = 0; i < n; ++i) {
					const int32 b = arm.Find(os[i]);
					if (b >= 0) {
						k.chain.PushBack(b);
					}
				}
				if (k.chain.Empty()) {
					return;
				}
				const uint32 last = (uint32)k.chain[(uint32)k.chain.Size() - 1u];
				k.target = bout ? arm.bones[last].tail : arm.bones[(uint32)k.chain[0]].head;
				out.PushBack(k);
			};
			const NkVec3f avant = V(0.f, 0.f, 1.f), arriere = V(0.f, 0.f, -1.f);
			if (t == NkRigTemplate::NK_RigTemplate_Humanoide) {
				static const char *const kRacine[1] = {"hips"};
				static const char *const kColonne[4] = {"hips", "spine", "chest", "chest.001"};
				Chaine("racine", NkRigControlKind::NK_RigControlKind_Racine, kRacine, 1, false);
				Chaine("colonne", NkRigControlKind::NK_RigControlKind_Colonne, kColonne, 4, true);
				Ik("IK_bras.L", "upperarm.L", "forearm.L", "hand.L", arriere);
				Ik("IK_bras.R", "upperarm.R", "forearm.R", "hand.R", arriere);
				Ik("IK_jambe.L", "thigh.L", "shin.L", "foot.L", avant);
				Ik("IK_jambe.R", "thigh.R", "shin.R", "foot.R", avant);
				static const char *const kTete[1] = {"head"};
				Chaine("regard", NkRigControlKind::NK_RigControlKind_Regard, kTete, 1, true);
			} else if (t == NkRigTemplate::NK_RigTemplate_Quadrupede) {
				static const char *const kRacine[1] = {"hips"};
				static const char *const kColonne[3] = {"hips", "spine", "chest"};
				Chaine("racine", NkRigControlKind::NK_RigControlKind_Racine, kRacine, 1, false);
				Chaine("colonne", NkRigControlKind::NK_RigControlKind_Colonne, kColonne, 3, true);
				Ik("IK_patte_av.L", "upperarm.L", "forearm.L", "hand.L", arriere);
				Ik("IK_patte_av.R", "upperarm.R", "forearm.R", "hand.R", arriere);
				Ik("IK_patte_ar.L", "thigh.L", "shin.L", "foot.L", avant);
				Ik("IK_patte_ar.R", "thigh.R", "shin.R", "foot.R", avant);
				static const char *const kTete[1] = {"head"};
				Chaine("regard", NkRigControlKind::NK_RigControlKind_Regard, kTete, 1, true);
			} else {
				NkVector<const char *> os;
				for (uint32 b = 0; b < arm.Count(); ++b) {
					os.PushBack(arm.bones[b].name.CStr());
				}
				Chaine("chaine", NkRigControlKind::NK_RigControlKind_Colonne, os.Data(), (uint32)os.Size(), true);
			}
		}

		// =====================================================================
		// L'IK A DEUX OS
		// =====================================================================
		bool NkSolveTwoBoneIK(const NkVec3f &a, const NkVec3f &b, const NkVec3f &c, const NkVec3f &cible, const NkVec3f &pole, NkVec3f &bOut,
							  NkVec3f &cOut) {
			const float32 lab = Dist(a, b), lbc = Dist(b, c);
			const NkVec3f at = Sub(cible, a);
			float32 lat = Len(at);
			const NkVec3f dir = Norm(at, Norm(Sub(c, a)));
			bool atteint = true;
			const float32 maxi = (lab + lbc) * 0.99999f, mini = std::fabs(lab - lbc) * 1.00001f + 1e-7f;
			if (lat > maxi) {
				lat = maxi;
				atteint = false;
			} else if (lat < mini) {
				lat = mini;
				atteint = false;
			}
			// La direction du coude : le pole projete sur le plan normal a `dir`.
			NkVec3f vp = Sub(pole, a);
			vp = Sub(vp, Mul(dir, Dot(vp, dir)));
			if (Len(vp) < 1e-6f) {
				vp = Sub(b, a);
				vp = Sub(vp, Mul(dir, Dot(vp, dir)));
			}
			if (Len(vp) < 1e-6f) {
				vp = std::fabs(dir.y) < 0.9f ? Cross(dir, V(0.f, 1.f, 0.f)) : Cross(dir, V(1.f, 0.f, 0.f));
			}
			const NkVec3f n = Norm(vp);
			float32 cosA = (lab * lab + lat * lat - lbc * lbc) / (2.f * lab * lat);
			cosA = cosA > 1.f ? 1.f : (cosA < -1.f ? -1.f : cosA);
			const float32 sinA = std::sqrt(1.f - cosA * cosA);
			bOut = Add(a, Add(Mul(dir, lab * cosA), Mul(n, lab * sinA)));
			cOut = Add(a, Mul(dir, lat));
			return atteint;
		}

		namespace {
			/// La rotation minimale de u vers v (axe, angle).
			void FromTo(const NkVec3f &u0, const NkVec3f &v0, NkVec3f &axe, float32 &angle) {
				const NkVec3f u = Norm(u0), v = Norm(v0);
				const NkVec3f c = Cross(u, v);
				const float32 s = Len(c), d = Dot(u, v);
				angle = std::atan2(s, d);
				if (s > 1e-7f) {
					axe = Mul(c, 1.f / s);
				} else {
					axe = std::fabs(u.y) < 0.9f ? Norm(Cross(u, V(0.f, 1.f, 0.f))) : Norm(Cross(u, V(1.f, 0.f, 0.f)));
				}
			}
			/// Tourne une matrice monde autour du point p (axes ET position).
			NkMat4f TournerAutour(const NkMat4f &m, const NkVec3f &axe, float32 angle, const NkVec3f &p) {
				if (std::fabs(angle) < 1e-9f) {
					return m;
				}
				NkMat4f r = m;
				const NkVec3f x = Tourner(AxeX(m), axe, angle), y = Tourner(AxeY(m), axe, angle), z = Tourner(AxeZ(m), axe, angle);
				const NkVec3f o = Add(p, Tourner(Sub(Origine(m), p), axe, angle));
				r.m00 = x.x;
				r.m01 = x.y;
				r.m02 = x.z;
				r.m10 = y.x;
				r.m11 = y.y;
				r.m12 = y.z;
				r.m20 = z.x;
				r.m21 = z.y;
				r.m22 = z.z;
				r.m30 = o.x;
				r.m31 = o.y;
				r.m32 = o.z;
				return r;
			}
		} // namespace

		bool NkApplyTwoBoneIK(NkMat4f *world, uint32 ia, uint32 ib, uint32 ic, const NkVec3f &cible, const NkVec3f &pole) {
			const NkVec3f a = Origine(world[ia]), b = Origine(world[ib]), c = Origine(world[ic]);
			NkVec3f b2, c2;
			const bool ok = NkSolveTwoBoneIK(a, b, c, cible, pole, b2, c2);
			NkVec3f axe1;
			float32 ang1;
			FromTo(Sub(b, a), Sub(b2, a), axe1, ang1);
			world[ia] = TournerAutour(world[ia], axe1, ang1, a);
			world[ib] = TournerAutour(world[ib], axe1, ang1, a);
			world[ic] = TournerAutour(world[ic], axe1, ang1, a);
			const NkVec3f cApres = Origine(world[ic]);
			NkVec3f axe2;
			float32 ang2;
			FromTo(Sub(cApres, b2), Sub(c2, b2), axe2, ang2);
			world[ib] = TournerAutour(world[ib], axe2, ang2, b2);
			world[ic] = TournerAutour(world[ic], axe2, ang2, b2);
			return ok;
		}

	} // namespace anim
} // namespace nkentseu
