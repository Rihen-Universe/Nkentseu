// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkSkinWeights.cpp — les poids de peau : stockage, automatiques, peinture,
// verification (voir NkSkinWeights.h).
// =============================================================================
#include "NKAnima/Skin/NkSkinWeights.h"
#include "NKAnima/Rig/NkRigMath.h"
#include "NKContainers/Utilities/NkSort.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace anim {

		using math::NkVec3f;
		using namespace rigm;

		// =====================================================================
		// STOCKAGE
		// =====================================================================
		void NkSkinWeights::Resize(uint32 n) {
			data.Resize((usize)n * kSlots, NkSkinInfluence{});
			count = n;
		}

		float32 NkSkinWeights::Get(uint32 v, int32 bone) const {
			if (v >= count || bone < 0) {
				return 0.f;
			}
			const NkSkinInfluence *s = Of(v);
			for (uint32 k = 0; k < kSlots; ++k) {
				if (s[k].bone == bone) {
					return s[k].weight;
				}
			}
			return 0.f;
		}

		void NkSkinWeights::Set(uint32 v, int32 bone, float32 w) {
			if (v >= count || bone < 0) {
				return;
			}
			NkSkinInfluence *s = Of(v);
			if (!(w > 1e-7f)) {
				for (uint32 k = 0; k < kSlots; ++k) {
					if (s[k].bone == bone) {
						s[k].bone = -1;
						s[k].weight = 0.f;
					}
				}
				return;
			}
			int32 vide = -1, faible = -1;
			for (uint32 k = 0; k < kSlots; ++k) {
				if (s[k].bone == bone) {
					s[k].weight = w;
					return;
				}
				if (s[k].bone < 0 && vide < 0) {
					vide = (int32)k;
				}
				if (s[k].bone >= 0 && (faible < 0 || s[k].weight < s[(uint32)faible].weight)) {
					faible = (int32)k;
				}
			}
			if (vide >= 0) {
				s[(uint32)vide].bone = bone;
				s[(uint32)vide].weight = w;
			} else if (faible >= 0 && s[(uint32)faible].weight < w) {
				s[(uint32)faible].bone = bone;
				s[(uint32)faible].weight = w;
			}
		}

		uint32 NkSkinWeights::InfluenceCount(uint32 v) const {
			if (v >= count) {
				return 0;
			}
			uint32 n = 0;
			const NkSkinInfluence *s = Of(v);
			for (uint32 k = 0; k < kSlots; ++k) {
				n += (s[k].bone >= 0 && s[k].weight > 0.f) ? 1u : 0u;
			}
			return n;
		}

		float32 NkSkinWeights::Sum(uint32 v) const {
			if (v >= count) {
				return 0.f;
			}
			float32 t = 0.f;
			const NkSkinInfluence *s = Of(v);
			for (uint32 k = 0; k < kSlots; ++k) {
				t += s[k].bone >= 0 ? s[k].weight : 0.f;
			}
			return t;
		}

		void NkSkinWeights::Normalize(uint32 v) {
			if (!verrous.Empty() && v < count) {
				// (05/10) Les verrous gardent leur poids ; les os libres prennent le reste.
				const float32 l = LockedSum(v);
				const float32 libres = Sum(v) - l;
				if (libres > 1e-12f) {
					const float32 k = (l < 1.f ? 1.f - l : 0.f) / libres;
					NkSkinInfluence *s = Of(v);
					for (uint32 i = 0; i < kSlots; ++i) {
						if (s[i].bone >= 0 && !Locked(s[i].bone)) {
							s[i].weight *= k;
						}
					}
				}
				return;
			}
			const float32 t = Sum(v);
			if (t > 1e-12f) {
				NkSkinInfluence *s = Of(v);
				for (uint32 k = 0; k < kSlots; ++k) {
					if (s[k].bone >= 0) {
						s[k].weight /= t;
					}
				}
			}
		}

		void NkSkinWeights::Lock(int32 bone, bool on) {
			if (bone < 0) {
				return;
			}
			if ((uint32)bone >= (uint32)verrous.Size()) {
				if (!on) {
					return;
				}
				verrous.Resize((usize)bone + 1u, 0);
			}
			verrous[(uint32)bone] = on ? 1 : 0;
		}

		bool NkSkinWeights::Locked(int32 bone) const {
			return bone >= 0 && (uint32)bone < (uint32)verrous.Size() && verrous[(uint32)bone] != 0;
		}

		float32 NkSkinWeights::LockedSum(uint32 v, int32 sauf) const {
			if (verrous.Empty() || v >= count) {
				return 0.f;
			}
			float32 s = 0.f;
			const NkSkinInfluence *in = Of(v);
			for (uint32 i = 0; i < kSlots; ++i) {
				if (in[i].bone >= 0 && in[i].bone != sauf && Locked(in[i].bone)) {
					s += in[i].weight;
				}
			}
			return s;
		}

		void NkSkinWeights::NormalizeAll() {
			for (uint32 v = 0; v < count; ++v) {
				Normalize(v);
			}
		}

		void NkSkinWeights::Limit(uint32 v, uint32 n) {
			if (v >= count) {
				return;
			}
			NkSkinInfluence *s = Of(v);
			// Tri decroissant des huit cases (les vides en dernier).
			for (uint32 a = 0; a < kSlots; ++a) {
				for (uint32 b = a + 1; b < kSlots; ++b) {
					const float32 wa = s[a].bone >= 0 ? s[a].weight : -1.f;
					const float32 wb = s[b].bone >= 0 ? s[b].weight : -1.f;
					if (wb > wa) {
						const NkSkinInfluence t = s[a];
						s[a] = s[b];
						s[b] = t;
					}
				}
			}
			for (uint32 k = n; k < kSlots; ++k) {
				s[k].bone = -1;
				s[k].weight = 0.f;
			}
			Normalize(v);
		}

		void NkSkinWeights::LimitAll(uint32 n) {
			for (uint32 v = 0; v < count; ++v) {
				Limit(v, n);
			}
		}

		uint32 NkSkinWeights::Prune(float32 seuil) {
			uint32 retires = 0;
			for (uint32 v = 0; v < count; ++v) {
				NkSkinInfluence *s = Of(v);
				int32 fort = -1;
				for (uint32 k = 0; k < kSlots; ++k) {
					if (s[k].bone >= 0 && (fort < 0 || s[k].weight > s[(uint32)fort].weight)) {
						fort = (int32)k;
					}
				}
				for (uint32 k = 0; k < kSlots; ++k) {
					// La plus forte reste toujours : nettoyer ne doit pas laisser un sommet SANS poids.
					if (s[k].bone >= 0 && (int32)k != fort && s[k].weight < seuil) {
						s[k].bone = -1;
						s[k].weight = 0.f;
						++retires;
					}
				}
				Normalize(v);
			}
			return retires;
		}

		void NkSkinWeights::ToGpu(uint32 v, float32 idx[4], float32 w[4]) const {
			for (uint32 k = 0; k < 4; ++k) {
				idx[k] = 0.f;
				w[k] = 0.f;
			}
			if (v >= count) {
				w[0] = 1.f;
				return;
			}
			NkSkinInfluence tri[kSlots];
			std::memcpy(tri, Of(v), sizeof(tri));
			for (uint32 a = 0; a < kSlots; ++a) {
				for (uint32 b = a + 1; b < kSlots; ++b) {
					const float32 wa = tri[a].bone >= 0 ? tri[a].weight : -1.f;
					const float32 wb = tri[b].bone >= 0 ? tri[b].weight : -1.f;
					if (wb > wa) {
						const NkSkinInfluence t = tri[a];
						tri[a] = tri[b];
						tri[b] = t;
					}
				}
			}
			float32 t = 0.f;
			for (uint32 k = 0; k < 4; ++k) {
				if (tri[k].bone >= 0 && tri[k].weight > 0.f) {
					idx[k] = (float32)tri[k].bone;
					w[k] = tri[k].weight;
					t += w[k];
				}
			}
			if (t > 1e-12f) {
				for (uint32 k = 0; k < 4; ++k) {
					w[k] /= t;
				}
			} else {
				// SANS POIDS : le GPU le rattache a l'os 0 (le sommet reste visible) ;
				// la verification, elle, le compte comme « sans poids ».
				w[0] = 1.f;
			}
		}

		void NkSkinWeights::FromGpu(uint32 v, const float32 idx[4], const float32 w[4]) {
			if (v >= count) {
				return;
			}
			NkSkinInfluence *s = Of(v);
			for (uint32 k = 0; k < kSlots; ++k) {
				s[k] = NkSkinInfluence{};
			}
			for (uint32 k = 0; k < 4; ++k) {
				if (w[k] > 0.f) {
					const int32 b = (int32)(idx[k] + 0.5f);
					Set(v, b, Get(v, b) + w[k]);
				}
			}
		}

		void NkSkinWeights::RemapBones(const NkVector<int32> &remap, const NkVector<int32> &heritier) {
			const uint32 nb = (uint32)remap.Size();
			if (!verrous.Empty()) {
				// (05/10) Les verrous suivent leur os.
				NkVector<uint8> nv;
				for (uint32 b = 0; b < nb && b < (uint32)verrous.Size(); ++b) {
					if (verrous[b] != 0 && remap[b] >= 0) {
						if ((uint32)nv.Size() <= (uint32)remap[b]) {
							nv.Resize((usize)remap[b] + 1u, 0);
						}
						nv[(uint32)remap[b]] = 1;
					}
				}
				verrous = nv;
			}
			for (uint32 v = 0; v < count; ++v) {
				NkSkinInfluence avant[kSlots];
				std::memcpy(avant, Of(v), sizeof(avant));
				NkSkinInfluence *s = Of(v);
				for (uint32 k = 0; k < kSlots; ++k) {
					s[k] = NkSkinInfluence{};
				}
				for (uint32 k = 0; k < kSlots; ++k) {
					int32 b = avant[k].bone;
					if (b < 0 || (uint32)b >= nb || !(avant[k].weight > 0.f)) {
						continue;
					}
					// L'os retire legue son poids a son heritier (en remontant si besoin).
					uint32 garde = 0;
					while (b >= 0 && (uint32)b < nb && remap[(uint32)b] < 0 && garde++ < nb) {
						b = (uint32)b < (uint32)heritier.Size() ? heritier[(uint32)b] : -1;
					}
					if (b < 0 || (uint32)b >= nb || remap[(uint32)b] < 0) {
						continue;
					}
					const int32 nv = remap[(uint32)b];
					Set(v, nv, Get(v, nv) + avant[k].weight);
				}
				Normalize(v);
			}
		}

		uint32 NkSkinWeights::MirrorX(const NkSkinMesh &mesh, const NkArmature &arm, bool depuisGauche) {
			if ((uint32)mesh.mirror.Size() != count) {
				return 0;
			}
			// L'os miroir de chaque os (lui-meme au centre).
			NkVector<int32> osMiroir;
			osMiroir.Resize(arm.Count(), -1);
			for (uint32 b = 0; b < arm.Count(); ++b) {
				const NkString n = NkArmature::MirrorName(arm.bones[b].name.CStr());
				const int32 m = n.Empty() ? -1 : arm.Find(n.CStr());
				osMiroir[b] = m >= 0 ? m : (int32)b;
			}
			const float32 eps = (mesh.Diagonal() > 0.f ? mesh.Diagonal() : 1.f) * 1e-4f;
			uint32 copies = 0;
			for (uint32 v = 0; v < count; ++v) {
				const float32 x = mesh.positions[v].x;
				const bool source = depuisGauche ? x > eps : x < -eps;
				const int32 m = mesh.mirror[v];
				if (!source || m < 0 || (uint32)m == v) {
					continue;
				}
				NkSkinInfluence src[kSlots];
				std::memcpy(src, Of(v), sizeof(src));
				NkSkinInfluence *d = Of((uint32)m);
				for (uint32 k = 0; k < kSlots; ++k) {
					d[k] = NkSkinInfluence{};
				}
				for (uint32 k = 0; k < kSlots; ++k) {
					if (src[k].bone >= 0 && (uint32)src[k].bone < arm.Count()) {
						Set((uint32)m, osMiroir[(uint32)src[k].bone], src[k].weight);
					}
				}
				++copies;
			}
			return copies;
		}

		// =====================================================================
		// LES POIDS AUTOMATIQUES
		// =====================================================================
		const char *NkAutoWeightMethodName(NkAutoWeightMethod m) {
			switch (m) {
				case NkAutoWeightMethod::NK_AutoWeightMethod_Chaleur: return "Chaleur (diffusion sur la surface)";
				case NkAutoWeightMethod::NK_AutoWeightMethod_Voxels_Geodesiques: return "Voxels geodesiques (distance dans le volume)";
				case NkAutoWeightMethod::NK_AutoWeightMethod_Proximite: return "Proximite (os le plus proche)";
				default: return "?";
			}
		}

		namespace {
			/// LA GRILLE DE VOXELS du volume : surface (triangles), exterieur (rempli
			/// depuis les bords), interieur = le reste.
			struct Grille {
					NkVec3f origine{0.f, 0.f, 0.f};
					float32 h = 1.f;
					int32 nx = 0, ny = 0, nz = 0;
					NkVector<uint8> etat; ///< 0 exterieur, 1 surface, 2 interieur

					uint32 Index(int32 x, int32 y, int32 z) const {
						return (uint32)((z * ny + y) * nx + x);
					}
					bool Dedans(int32 x, int32 y, int32 z) const {
						return x >= 0 && y >= 0 && z >= 0 && x < nx && y < ny && z < nz;
					}
					bool Solide(int32 x, int32 y, int32 z) const {
						return Dedans(x, y, z) && etat[Index(x, y, z)] != 0;
					}
					void Cellule(const NkVec3f &p, int32 &x, int32 &y, int32 &z) const {
						x = (int32)std::floor((p.x - origine.x) / h);
						y = (int32)std::floor((p.y - origine.y) / h);
						z = (int32)std::floor((p.z - origine.z) / h);
					}
					NkVec3f Centre(int32 x, int32 y, int32 z) const {
						return V(origine.x + ((float32)x + 0.5f) * h, origine.y + ((float32)y + 0.5f) * h,
								 origine.z + ((float32)z + 0.5f) * h);
					}

					uint32 Construire(const NkSkinMesh &m, uint32 resolution) {
						const NkVec3f e = Sub(m.bmax, m.bmin);
						float32 grand = e.x > e.y ? e.x : e.y;
						grand = grand > e.z ? grand : e.z;
						if (!(grand > 0.f)) {
							return 0;
						}
						h = grand / (float32)(resolution < 8 ? 8 : resolution);
						origine = Sub(m.bmin, V(2.f * h, 2.f * h, 2.f * h));
						nx = (int32)std::ceil(e.x / h) + 5;
						ny = (int32)std::ceil(e.y / h) + 5;
						nz = (int32)std::ceil(e.z / h) + 5;
						etat.Clear();
						etat.Resize((usize)nx * (usize)ny * (usize)nz, 0);
						// SURFACE : chaque triangle echantillonne a moins d'un demi-voxel.
						const uint32 nt = m.TriangleCount();
						for (uint32 t = 0; t < nt; ++t) {
							const NkVec3f a = m.positions[m.indices[t * 3]], b = m.positions[m.indices[t * 3 + 1]],
										  c = m.positions[m.indices[t * 3 + 2]];
							float32 l = Dist(a, b);
							l = Dist(b, c) > l ? Dist(b, c) : l;
							l = Dist(c, a) > l ? Dist(c, a) : l;
							const int32 pas = (int32)std::ceil(l / (h * 0.5f)) + 1;
							for (int32 i = 0; i <= pas; ++i) {
								for (int32 j = 0; j <= pas - i; ++j) {
									const float32 u = (float32)i / (float32)pas, v = (float32)j / (float32)pas;
									const NkVec3f p = Add(Add(Mul(a, 1.f - u - v), Mul(b, u)), Mul(c, v));
									int32 x, y, z;
									Cellule(p, x, y, z);
									if (Dedans(x, y, z)) {
										etat[Index(x, y, z)] = 1;
									}
								}
							}
						}
						// EXTERIEUR : remplissage depuis le coin (la marge de deux voxels est vide).
						NkVector<uint8> ext;
						ext.Resize(etat.Size(), 0);
						NkVector<uint32> pile;
						pile.PushBack(0);
						ext[0] = 1;
						while (!pile.Empty()) {
							const uint32 i = pile[(uint32)pile.Size() - 1u];
							pile.PopBack();
							const int32 x = (int32)(i % (uint32)nx), y = (int32)((i / (uint32)nx) % (uint32)ny),
										z = (int32)(i / ((uint32)nx * (uint32)ny));
							const int32 d[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
							for (int32 k = 0; k < 6; ++k) {
								const int32 a = x + d[k][0], b = y + d[k][1], c = z + d[k][2];
								if (!Dedans(a, b, c)) {
									continue;
								}
								const uint32 j = Index(a, b, c);
								if (!ext[j] && etat[j] == 0) {
									ext[j] = 1;
									pile.PushBack(j);
								}
							}
						}
						uint32 solides = 0;
						for (uint32 i = 0; i < (uint32)etat.Size(); ++i) {
							if (etat[i] == 0 && !ext[i]) {
								etat[i] = 2;
							}
							solides += etat[i] != 0 ? 1u : 0u;
						}
						return solides;
					}

					/// Le segment [a, b] reste-t-il DANS le volume ? (la « visibilite » de
					/// la chaleur : l'os doit voir le sommet sans traverser l'air.)
					bool SegmentInterieur(const NkVec3f &a, const NkVec3f &b) const {
						const float32 l = Dist(a, b);
						const int32 pas = (int32)std::ceil(l / (h * 0.5f));
						int32 dehors = 0;
						for (int32 k = 1; k < pas; ++k) {
							const NkVec3f p = Lerp(a, b, (float32)k / (float32)pas);
							int32 x, y, z;
							Cellule(p, x, y, z);
							dehors += Solide(x, y, z) ? 0 : 1;
						}
						// Un echantillon sur huit dehors est pardonne : l'arrondi d'un bord.
						return dehors <= (pas > 8 ? pas / 8 : 1);
					}
			};

			/// Un tas binaire (distance, voxel) pour Dijkstra.
			struct Tas {
					struct E {
							float32 d;
							uint32 i;
					};
					NkVector<E> t;
					void Pousser(float32 d, uint32 i) {
						t.PushBack(E{d, i});
						uint32 k = (uint32)t.Size() - 1u;
						while (k > 0) {
							const uint32 p = (k - 1u) / 2u;
							if (t[p].d <= t[k].d) {
								break;
							}
							const E x = t[p];
							t[p] = t[k];
							t[k] = x;
							k = p;
						}
					}
					E Tirer() {
						const E r = t[0];
						t[0] = t[(uint32)t.Size() - 1u];
						t.PopBack();
						uint32 k = 0;
						const uint32 n = (uint32)t.Size();
						for (;;) {
							const uint32 a = 2u * k + 1u, b = a + 1u;
							uint32 m = k;
							if (a < n && t[a].d < t[m].d) {
								m = a;
							}
							if (b < n && t[b].d < t[m].d) {
								m = b;
							}
							if (m == k) {
								break;
							}
							const E x = t[m];
							t[m] = t[k];
							t[k] = x;
							k = m;
						}
						return r;
					}
			};

			/// Les poids DENSES (un par representant et par os deformant) -> les
			/// influences de chaque sommet, nettoyees, limitees, normalisees.
			void Ranger(const NkSkinMesh &m, const NkVector<int32> &os, const NkVector<float32> &dense,
						const NkAutoWeightOptions &opt, NkSkinWeights &out) {
				const uint32 n = m.VertexCount(), nb = (uint32)os.Size();
				out.Resize(n);
				for (uint32 v = 0; v < n; ++v) {
					NkSkinInfluence *s = out.Of(v);
					for (uint32 k = 0; k < NkSkinWeights::kSlots; ++k) {
						s[k] = NkSkinInfluence{};
					}
					const uint32 r = m.rep[v];
					for (uint32 b = 0; b < nb; ++b) {
						const float32 w = dense[(usize)r * nb + b];
						if (w > 1e-6f) {
							out.Set(v, os[b], w);
						}
					}
					out.Normalize(v);
				}
				out.Prune(opt.pruneBelow);
				out.LimitAll(opt.maxInfluences > 0 && opt.maxInfluences <= NkSkinWeights::kSlots ? opt.maxInfluences : 4u);
				out.Prune(opt.pruneBelow);
			}

			/// Lissage des poids denses sur la surface (moyenne avec les voisins).
			void Lisser(const NkSkinMesh &m, uint32 nb, NkVector<float32> &dense, uint32 passes) {
				const uint32 ns = (uint32)m.soudes.Size();
				NkVector<float32> tmp;
				for (uint32 p = 0; p < passes; ++p) {
					tmp = dense;
					for (uint32 r = 0; r < ns; ++r) {
						const uint32 a = m.adjDebut[r], z = m.adjDebut[r + 1];
						if (z <= a) {
							continue;
						}
						const float32 inv = 1.f / (float32)(z - a);
						for (uint32 b = 0; b < nb; ++b) {
							float32 s = 0.f;
							for (uint32 k = a; k < z; ++k) {
								s += tmp[(usize)m.adj[k] * nb + b];
							}
							dense[(usize)r * nb + b] = 0.5f * tmp[(usize)r * nb + b] + 0.5f * s * inv;
						}
					}
				}
			}
		} // namespace

		bool NkAutoWeights(const NkSkinMesh &meshIn, const NkArmature &arm, const NkAutoWeightOptions &opt, NkSkinWeights &out,
						   NkAutoWeightReport *report) {
			NkAutoWeightReport rapportLocal;
			NkAutoWeightReport &rp = report != nullptr ? *report : rapportLocal;
			rp = NkAutoWeightReport{};
			// La topologie est NECESSAIRE (soudure, voisins) : une copie si elle manque.
			NkSkinMesh copie;
			const NkSkinMesh *pm = &meshIn;
			if ((uint32)meshIn.rep.Size() != meshIn.VertexCount() || meshIn.adjDebut.Empty()) {
				copie.positions = meshIn.positions;
				copie.indices = meshIn.indices;
				copie.BuildTopology();
				pm = &copie;
			}
			const NkSkinMesh &m = *pm;
			NkVector<int32> os;
			for (uint32 b = 0; b < arm.Count(); ++b) {
				if (arm.bones[b].deform && arm.Length(b) > 1e-7f) {
					os.PushBack((int32)b);
				}
			}
			rp.osDeformants = (uint32)os.Size();
			if (m.VertexCount() == 0 || m.TriangleCount() == 0) {
				rp.message = NkString("maillage vide");
				return false;
			}
			if (os.Empty()) {
				rp.message = NkString("aucun os deformant");
				return false;
			}
			const uint32 ns = (uint32)m.soudes.Size(), nb = (uint32)os.Size();
			const float32 diag = m.Diagonal() > 0.f ? m.Diagonal() : 1.f;
			NkVector<float32> dense;
			dense.Resize((usize)ns * nb, 0.f);

			// Distances representant -> os (segments au repos).
			NkVector<float32> dist;
			dist.Resize((usize)ns * nb, 0.f);
			for (uint32 r = 0; r < ns; ++r) {
				const NkVec3f p = m.positions[m.soudes[r]];
				for (uint32 b = 0; b < nb; ++b) {
					const NkArmatureBone &o = arm.bones[(uint32)os[b]];
					dist[(usize)r * nb + b] = DistanceSegment(p, o.head, o.tail);
				}
			}

			if (opt.method == NkAutoWeightMethod::NK_AutoWeightMethod_Proximite) {
				for (uint32 r = 0; r < ns; ++r) {
					for (uint32 b = 0; b < nb; ++b) {
						const float32 d = dist[(usize)r * nb + b] / diag + 1e-4f;
						dense[(usize)r * nb + b] = 1.f / (d * d * d * d);
					}
				}
				Ranger(m, os, dense, opt, out);
				rp.ok = true;
				rp.message = NkString("proximite");
				return true;
			}

			Grille g;
			rp.voxelsSolides = g.Construire(m, opt.method == NkAutoWeightMethod::NK_AutoWeightMethod_Chaleur ? 64u : opt.voxelResolution);

			if (opt.method == NkAutoWeightMethod::NK_AutoWeightMethod_Voxels_Geodesiques) {
				if (rp.voxelsSolides == 0) {
					rp.message = NkString("la voxelisation n'a rien rendu");
					return false;
				}
				const uint32 nv = (uint32)g.etat.Size();
				NkVector<float32> D;
				D.Resize(nv, 1e30f);
				NkVector<float32> dVox; // distance geodesique (os b) au voxel de chaque representant
				dVox.Resize((usize)ns * nb, 1e30f);
				// Le voxel solide le plus proche de chaque representant (une fois).
				NkVector<int32> voxDe;
				voxDe.Resize(ns, -1);
				NkVector<float32> ecartVox;
				ecartVox.Resize(ns, 0.f);
				for (uint32 r = 0; r < ns; ++r) {
					const NkVec3f p = m.positions[m.soudes[r]];
					int32 x, y, z;
					g.Cellule(p, x, y, z);
					float32 meilleur = 1e30f;
					for (int32 dx = -1; dx <= 1; ++dx) {
						for (int32 dy = -1; dy <= 1; ++dy) {
							for (int32 dz = -1; dz <= 1; ++dz) {
								if (g.Solide(x + dx, y + dy, z + dz)) {
									const float32 d = Dist(p, g.Centre(x + dx, y + dy, z + dz));
									if (d < meilleur) {
										meilleur = d;
										voxDe[r] = (int32)g.Index(x + dx, y + dy, z + dz);
									}
								}
							}
						}
					}
					ecartVox[r] = meilleur < 1e29f ? meilleur : 0.f;
				}
				const float32 hs[4] = {0.f, g.h, g.h * 1.41421356f, g.h * 1.7320508f};
				for (uint32 b = 0; b < nb; ++b) {
					for (uint32 i = 0; i < nv; ++i) {
						D[i] = 1e30f;
					}
					Tas tas;
					const NkArmatureBone &o = arm.bones[(uint32)os[b]];
					const float32 l = Dist(o.head, o.tail);
					const int32 pas = (int32)std::ceil(l / (g.h * 0.5f)) + 1;
					uint32 graines = 0;
					for (int32 k = 0; k <= pas; ++k) {
						const NkVec3f p = Lerp(o.head, o.tail, (float32)k / (float32)pas);
						int32 x, y, z;
						g.Cellule(p, x, y, z);
						for (int32 dx = 0; dx <= 1; ++dx) {
							for (int32 dy = 0; dy <= 1; ++dy) {
								for (int32 dz = 0; dz <= 1; ++dz) {
									if (!g.Solide(x + dx, y + dy, z + dz)) {
										continue;
									}
									const uint32 i = g.Index(x + dx, y + dy, z + dz);
									const float32 d = DistanceSegment(g.Centre(x + dx, y + dy, z + dz), o.head, o.tail);
									if (d < D[i]) {
										D[i] = d;
										tas.Pousser(d, i);
										++graines;
									}
								}
							}
						}
					}
					if (graines == 0) {
						// L'os est HORS du volume : il part du voxel solide le plus proche.
						float32 meilleur = 1e30f;
						uint32 mi = 0;
						const NkVec3f c = Lerp(o.head, o.tail, 0.5f);
						for (uint32 i = 0; i < nv; ++i) {
							if (g.etat[i] == 0) {
								continue;
							}
							const int32 x = (int32)(i % (uint32)g.nx), y = (int32)((i / (uint32)g.nx) % (uint32)g.ny),
										z = (int32)(i / ((uint32)g.nx * (uint32)g.ny));
							const float32 d = DistanceSegment(g.Centre(x, y, z), o.head, o.tail);
							if (d < meilleur) {
								meilleur = d;
								mi = i;
							}
						}
						(void)c;
						D[mi] = meilleur;
						tas.Pousser(meilleur, mi);
					}
					while (!tas.t.Empty()) {
						const Tas::E e = tas.Tirer();
						if (e.d > D[e.i]) {
							continue;
						}
						const int32 x = (int32)(e.i % (uint32)g.nx), y = (int32)((e.i / (uint32)g.nx) % (uint32)g.ny),
									z = (int32)(e.i / ((uint32)g.nx * (uint32)g.ny));
						for (int32 dx = -1; dx <= 1; ++dx) {
							for (int32 dy = -1; dy <= 1; ++dy) {
								for (int32 dz = -1; dz <= 1; ++dz) {
									const int32 k = (dx != 0) + (dy != 0) + (dz != 0);
									if (k == 0 || !g.Solide(x + dx, y + dy, z + dz)) {
										continue;
									}
									const uint32 j = g.Index(x + dx, y + dy, z + dz);
									const float32 nd = e.d + hs[k];
									if (nd < D[j]) {
										D[j] = nd;
										tas.Pousser(nd, j);
									}
								}
							}
						}
					}
					for (uint32 r = 0; r < ns; ++r) {
						if (voxDe[r] >= 0 && D[(uint32)voxDe[r]] < 1e29f) {
							dVox[(usize)r * nb + b] = D[(uint32)voxDe[r]] + ecartVox[r];
						}
					}
				}
				// Decroissance : w = (1 / d)^falloff, d en fraction de la diagonale.
				const float32 f = opt.falloff > 0.5f ? opt.falloff : 4.f;
				for (uint32 r = 0; r < ns; ++r) {
					float32 dmin = 1e30f;
					for (uint32 b = 0; b < nb; ++b) {
						dmin = dVox[(usize)r * nb + b] < dmin ? dVox[(usize)r * nb + b] : dmin;
					}
					for (uint32 b = 0; b < nb; ++b) {
						const float32 d = dVox[(usize)r * nb + b];
						if (d > 1e29f) {
							continue;
						}
						// Relatif au plus proche : evite les debordements de (1/d)^4.
						const float32 q = (dmin + g.h * 0.25f) / (d + g.h * 0.25f);
						dense[(usize)r * nb + b] = std::pow(q, f);
					}
				}
				// Normaliser AVANT de lisser : chaque representant pese pareil.
				for (uint32 r = 0; r < ns; ++r) {
					float32 s = 0.f;
					for (uint32 b = 0; b < nb; ++b) {
						s += dense[(usize)r * nb + b];
					}
					if (s > 0.f) {
						for (uint32 b = 0; b < nb; ++b) {
							dense[(usize)r * nb + b] /= s;
						}
					} else {
						// Inatteignable (une piece isolee) : l'os le plus proche a vol d'oiseau.
						uint32 bm = 0;
						for (uint32 b = 1; b < nb; ++b) {
							bm = dist[(usize)r * nb + b] < dist[(usize)r * nb + bm] ? b : bm;
						}
						dense[(usize)r * nb + bm] = 1.f;
					}
				}
				Lisser(m, nb, dense, opt.smoothPasses);
				Ranger(m, os, dense, opt, out);
				rp.ok = true;
				rp.message = NkString::Format("voxels geodesiques : %u voxels solides, pas %.4f", rp.voxelsSolides, (double)g.h);
				return true;
			}

			// ── LA CHALEUR (Baran & Popovic) ──────────────────────────────────────
			// (K + A H) w_b = A H p_b, K = laplacien cotangent, A = aire du sommet,
			// H = 1 / d^2 (d = distance a l'os le plus proche VISIBLE), p_b = 1 si b
			// est cet os. Gradient conjugue, preconditionneur de Jacobi.
			NkVector<float32> poidsArete;
			poidsArete.Resize(m.adj.Size(), 0.f);
			NkVector<float32> aire;
			aire.Resize(ns, 0.f);
			auto Arete = [&](uint32 a, uint32 b) -> int32 {
				for (uint32 k = m.adjDebut[a]; k < m.adjDebut[a + 1]; ++k) {
					if (m.adj[k] == b) {
						return (int32)k;
					}
				}
				return -1;
			};
			const uint32 nt = m.TriangleCount();
			for (uint32 t = 0; t < nt; ++t) {
				const uint32 r[3] = {m.rep[m.indices[t * 3]], m.rep[m.indices[t * 3 + 1]], m.rep[m.indices[t * 3 + 2]]};
				if (r[0] == r[1] || r[1] == r[2] || r[2] == r[0]) {
					continue;
				}
				const NkVec3f p[3] = {m.positions[m.soudes[r[0]]], m.positions[m.soudes[r[1]]], m.positions[m.soudes[r[2]]]};
				const float32 a2 = Len(Cross(Sub(p[1], p[0]), Sub(p[2], p[0])));
				for (uint32 c = 0; c < 3; ++c) {
					aire[r[c]] += a2 / 6.f;
					// cot de l'angle en c, poids de l'arete opposee (c+1, c+2).
					const NkVec3f u = Sub(p[(c + 1) % 3], p[c]), w = Sub(p[(c + 2) % 3], p[c]);
					const float32 cr = Len(Cross(u, w));
					const float32 cot = cr > 1e-20f ? Dot(u, w) / cr : 0.f;
					const uint32 a = r[(c + 1) % 3], b = r[(c + 2) % 3];
					const int32 ab = Arete(a, b), ba = Arete(b, a);
					if (ab >= 0) {
						poidsArete[(uint32)ab] += 0.5f * cot;
					}
					if (ba >= 0) {
						poidsArete[(uint32)ba] += 0.5f * cot;
					}
				}
			}
			// Les cotangentes negatives (triangles obtus) casseraient la definie-
			// positivite : bornees a une petite valeur positive.
			float32 moyenne = 0.f;
			for (uint32 k = 0; k < (uint32)poidsArete.Size(); ++k) {
				moyenne += std::fabs(poidsArete[k]);
			}
			moyenne = poidsArete.Empty() ? 1.f : moyenne / (float32)poidsArete.Size();
			for (uint32 k = 0; k < (uint32)poidsArete.Size(); ++k) {
				const float32 mini = moyenne * 1e-3f + 1e-9f;
				poidsArete[k] = poidsArete[k] > mini ? poidsArete[k] : mini;
			}
			// L'os le plus proche VISIBLE de chaque representant.
			NkVector<float32> chaleur; // A * H
			chaleur.Resize(ns, 0.f);
			NkVector<int32> proche;
			proche.Resize(ns, -1);
			NkVector<uint32> ordre;
			ordre.Resize(nb, 0);
			for (uint32 r = 0; r < ns; ++r) {
				const NkVec3f p = m.positions[m.soudes[r]];
				for (uint32 b = 0; b < nb; ++b) {
					ordre[b] = b;
				}
				const float32 *dr = dist.Data() + (usize)r * nb;
				NkSort(ordre.Data(), ordre.Data() + nb, [dr](uint32 a, uint32 b) { return dr[a] < dr[b]; });
				int32 choisi = -1;
				for (uint32 k = 0; k < nb && k < 6 && choisi < 0; ++k) {
					const NkArmatureBone &o = arm.bones[(uint32)os[ordre[k]]];
					float32 t = 0.f;
					(void)DistanceSegment(p, o.head, o.tail, &t);
					const NkVec3f q = Lerp(o.head, o.tail, t);
					// Recule d'un voxel vers l'interieur : le sommet est SUR la surface.
					const NkVec3f depart = Lerp(p, q, g.h / (Dist(p, q) + 1e-12f) > 1.f ? 1.f : g.h / (Dist(p, q) + 1e-12f));
					if (rp.voxelsSolides == 0 || g.SegmentInterieur(depart, q)) {
						choisi = (int32)ordre[k];
					}
				}
				if (choisi < 0) {
					choisi = (int32)ordre[0]; // aucun os visible : le plus proche (comme Blender)
				}
				proche[r] = choisi;
				float32 d = dr[(uint32)choisi];
				d = d > diag * 1e-3f ? d : diag * 1e-3f;
				chaleur[r] = aire[r] / (d * d);
			}
			// Le gradient conjugue, un os a la fois. Un os qui n'est le plus proche
			// d'AUCUN sommet a un second membre nul : sa solution est nulle (il sera
			// orphelin -- la verification le dira).
			NkVector<float32> x, rr, z, pp, ap, diagK;
			x.Resize(ns, 0.f);
			rr.Resize(ns, 0.f);
			z.Resize(ns, 0.f);
			pp.Resize(ns, 0.f);
			ap.Resize(ns, 0.f);
			diagK.Resize(ns, 0.f);
			for (uint32 r = 0; r < ns; ++r) {
				float32 s = 0.f;
				for (uint32 k = m.adjDebut[r]; k < m.adjDebut[r + 1]; ++k) {
					s += poidsArete[k];
				}
				diagK[r] = s + chaleur[r];
			}
			auto Produit = [&](const NkVector<float32> &in, NkVector<float32> &res) {
				for (uint32 r = 0; r < ns; ++r) {
					float32 s = diagK[r] * in[r];
					for (uint32 k = m.adjDebut[r]; k < m.adjDebut[r + 1]; ++k) {
						s -= poidsArete[k] * in[m.adj[k]];
					}
					res[r] = s;
				}
			};
			for (uint32 b = 0; b < nb; ++b) {
				bool utile = false;
				for (uint32 r = 0; r < ns && !utile; ++r) {
					utile = proche[r] == (int32)b;
				}
				if (!utile) {
					continue;
				}
				// x0 = p_b ; r = A H p - K' x0.
				for (uint32 r = 0; r < ns; ++r) {
					x[r] = proche[r] == (int32)b ? 1.f : 0.f;
				}
				Produit(x, ap);
				float32 normeB = 0.f;
				for (uint32 r = 0; r < ns; ++r) {
					const float32 bb = proche[r] == (int32)b ? chaleur[r] : 0.f;
					rr[r] = bb - ap[r];
					normeB += bb * bb;
					z[r] = rr[r] / (diagK[r] > 1e-20f ? diagK[r] : 1.f);
					pp[r] = z[r];
				}
				normeB = std::sqrt(normeB) + 1e-30f;
				float32 rz = 0.f;
				for (uint32 r = 0; r < ns; ++r) {
					rz += rr[r] * z[r];
				}
				uint32 it = 0;
				for (; it < 800; ++it) {
					Produit(pp, ap);
					float32 pap = 0.f;
					for (uint32 r = 0; r < ns; ++r) {
						pap += pp[r] * ap[r];
					}
					if (!(std::fabs(pap) > 1e-30f)) {
						break;
					}
					const float32 alpha = rz / pap;
					float32 nr = 0.f;
					for (uint32 r = 0; r < ns; ++r) {
						x[r] += alpha * pp[r];
						rr[r] -= alpha * ap[r];
						nr += rr[r] * rr[r];
					}
					if (std::sqrt(nr) / normeB < 1e-5f) {
						++it;
						break;
					}
					float32 rzn = 0.f;
					for (uint32 r = 0; r < ns; ++r) {
						z[r] = rr[r] / (diagK[r] > 1e-20f ? diagK[r] : 1.f);
						rzn += rr[r] * z[r];
					}
					const float32 beta = rzn / (rz > 1e-30f ? rz : 1e-30f);
					rz = rzn;
					for (uint32 r = 0; r < ns; ++r) {
						pp[r] = z[r] + beta * pp[r];
					}
				}
				rp.iterations += it;
				for (uint32 r = 0; r < ns; ++r) {
					dense[(usize)r * nb + b] = x[r] > 0.f ? x[r] : 0.f;
				}
			}
			// Un representant sans chaleur nette (solution numerique nulle) : son os proche.
			for (uint32 r = 0; r < ns; ++r) {
				float32 s = 0.f;
				for (uint32 b = 0; b < nb; ++b) {
					s += dense[(usize)r * nb + b];
				}
				if (!(s > 1e-6f)) {
					dense[(usize)r * nb + (uint32)proche[r]] = 1.f;
				}
			}
			Ranger(m, os, dense, opt, out);
			rp.ok = true;
			rp.message = NkString::Format("chaleur : %u iterations de gradient conjugue, %u representants", rp.iterations, ns);
			return true;
		}

		// =====================================================================
		// LA PEINTURE
		// =====================================================================
		const char *NkBrushModeName(NkBrushMode m) {
			switch (m) {
				case NkBrushMode::NK_BrushMode_Ajouter: return "Ajouter";
				case NkBrushMode::NK_BrushMode_Soustraire: return "Soustraire";
				case NkBrushMode::NK_BrushMode_Lisser: return "Lisser";
				case NkBrushMode::NK_BrushMode_Remplacer: return "Remplacer";
				default: return "?";
			}
		}

		namespace {
			/// Pose `nw` pour `bone` sur `v`, en gardant la somme a 1 si demande : les
			/// AUTRES os se partagent le reste en proportion (la « normalisation
			/// automatique » de Blender). Un os seul reste a 1.
			void PoserNormalise(NkSkinWeights &w, uint32 v, int32 bone, float32 nw, bool autoNorm) {
				// (05/10) Un os VERROUILLE ne se peint pas ; les verrous des autres
				// bornent ce qu'il peut prendre et ne sont pas repris.
				if (w.Locked(bone)) {
					return;
				}
				const float32 l = w.LockedSum(v, bone);
				const float32 libre = l < 1.f ? 1.f - l : 0.f;
				nw = nw < 0.f ? 0.f : (nw > 1.f ? 1.f : nw);
				if (!autoNorm) {
					w.Set(v, bone, nw);
					return;
				}
				nw = nw > libre ? libre : nw;
				const float32 ancien = w.Get(v, bone);
				const float32 autres = w.Sum(v) - ancien - l;
				if (autres <= 1e-6f) {
					if (nw > 1e-6f) {
						w.Set(v, bone, libre);
					}
					return;
				}
				const float32 k = (libre - nw) / autres;
				NkSkinInfluence *s = w.Of(v);
				for (uint32 i = 0; i < NkSkinWeights::kSlots; ++i) {
					if (s[i].bone >= 0 && s[i].bone != bone && !w.Locked(s[i].bone)) {
						s[i].weight *= k;
						if (s[i].weight < 1e-6f) {
							s[i].bone = -1;
							s[i].weight = 0.f;
						}
					}
				}
				w.Set(v, bone, nw);
			}

			float32 NouvellePeinture(const NkWeightBrush &br, float32 ancien, float32 moyenne, float32 f) {
				switch (br.mode) {
					case NkBrushMode::NK_BrushMode_Ajouter: return ancien + f;
					case NkBrushMode::NK_BrushMode_Soustraire: return ancien - f;
					case NkBrushMode::NK_BrushMode_Remplacer: return ancien + (br.value - ancien) * f;
					case NkBrushMode::NK_BrushMode_Lisser: return ancien + (moyenne - ancien) * f;
					default: return ancien;
				}
			}
		} // namespace

		uint32 NkApplyBrush(NkSkinWeights &w, const NkSkinMesh &mesh, const NkArmature &arm, int32 bone, const NkWeightBrush &br,
							const uint32 *verts, const float32 *factors, uint32 n) {
			if (bone < 0 || (uint32)bone >= arm.Count() || verts == nullptr || factors == nullptr) {
				return 0;
			}
			int32 osMiroir = bone;
			if (br.symmetryX) {
				const NkString nm = NkArmature::MirrorName(arm.bones[(uint32)bone].name.CStr());
				const int32 j = nm.Empty() ? -1 : arm.Find(nm.CStr());
				osMiroir = j >= 0 ? j : bone;
			}
			const bool miroir = br.symmetryX && (uint32)mesh.mirror.Size() == w.VertexCount();
			const bool voisins = (uint32)mesh.rep.Size() == w.VertexCount() && !mesh.adjDebut.Empty();
			// Les MOYENNES d'abord (lissage) : calculees sur les poids d'AVANT la
			// touche, pour que l'ordre des sommets ne change rien.
			auto Moyenne = [&](uint32 v, int32 b) -> float32 {
				if (!voisins) {
					return w.Get(v, b);
				}
				const uint32 r = mesh.rep[v];
				const uint32 a = mesh.adjDebut[r], z = mesh.adjDebut[r + 1];
				if (z <= a) {
					return w.Get(v, b);
				}
				float32 s = 0.f;
				for (uint32 k = a; k < z; ++k) {
					s += w.Get(mesh.soudes[mesh.adj[k]], b);
				}
				return s / (float32)(z - a);
			};
			NkVector<float32> moy, moyM;
			if (br.mode == NkBrushMode::NK_BrushMode_Lisser) {
				moy.Resize(n, 0.f);
				moyM.Resize(n, 0.f);
				for (uint32 i = 0; i < n; ++i) {
					moy[i] = verts[i] < w.VertexCount() ? Moyenne(verts[i], bone) : 0.f;
					const int32 mv = miroir && verts[i] < w.VertexCount() ? mesh.mirror[verts[i]] : -1;
					moyM[i] = mv >= 0 ? Moyenne((uint32)mv, osMiroir) : 0.f;
				}
			}
			uint32 changes = 0;
			for (uint32 i = 0; i < n; ++i) {
				const uint32 v = verts[i];
				const float32 f = factors[i] * br.strength;
				if (v >= w.VertexCount() || !(f > 0.f)) {
					continue;
				}
				const float32 avant = w.Get(v, bone);
				PoserNormalise(w, v, bone, NouvellePeinture(br, avant, moy.Empty() ? 0.f : moy[i], f), br.autoNormalize);
				changes += std::fabs(w.Get(v, bone) - avant) > 1e-7f ? 1u : 0u;
				if (miroir) {
					const int32 mv = mesh.mirror[v];
					if (mv >= 0 && (uint32)mv != v) {
						const float32 am = w.Get((uint32)mv, osMiroir);
						PoserNormalise(w, (uint32)mv, osMiroir, NouvellePeinture(br, am, moyM.Empty() ? 0.f : moyM[i], f), br.autoNormalize);
						changes += std::fabs(w.Get((uint32)mv, osMiroir) - am) > 1e-7f ? 1u : 0u;
					}
				}
			}
			return changes;
		}

		uint32 NkApplyWeightValues(NkSkinWeights &w, const NkSkinMesh &mesh, const NkArmature &arm, int32 bone, const NkWeightBrush &br,
								   const uint32 *verts, const float32 *values, uint32 n, float32 force) {
			if (bone < 0 || (uint32)bone >= arm.Count() || verts == nullptr || values == nullptr || !(force > 0.f)) {
				return 0;
			}
			int32 osMiroir = bone;
			if (br.symmetryX) {
				const NkString nm = NkArmature::MirrorName(arm.bones[(uint32)bone].name.CStr());
				const int32 j = nm.Empty() ? -1 : arm.Find(nm.CStr());
				osMiroir = j >= 0 ? j : bone;
			}
			const bool miroir = br.symmetryX && (uint32)mesh.mirror.Size() == w.VertexCount();
			const float32 f = force > 1.f ? 1.f : force;
			uint32 changes = 0;
			for (uint32 i = 0; i < n; ++i) {
				const uint32 v = verts[i];
				if (v >= w.VertexCount()) {
					continue;
				}
				const float32 avant = w.Get(v, bone);
				PoserNormalise(w, v, bone, avant + (values[i] - avant) * f, br.autoNormalize);
				changes += std::fabs(w.Get(v, bone) - avant) > 1e-7f ? 1u : 0u;
				if (miroir) {
					const int32 mv = mesh.mirror[v];
					if (mv >= 0 && (uint32)mv != v) {
						const float32 am = w.Get((uint32)mv, osMiroir);
						PoserNormalise(w, (uint32)mv, osMiroir, am + (values[i] - am) * f, br.autoNormalize);
					}
				}
			}
			return changes;
		}

		// =====================================================================
		// LA VERIFICATION
		// =====================================================================
		NkWeightCheck NkCheckWeights(const NkSkinWeights &w, const NkArmature &arm, float32 tol) {
			NkWeightCheck c;
			c.vertices = w.VertexCount();
			NkVector<float32> maxParOs;
			maxParOs.Resize(arm.Count(), 0.f);
			for (uint32 v = 0; v < w.VertexCount(); ++v) {
				const uint32 n = w.InfluenceCount(v);
				c.maxInfluences = n > c.maxInfluences ? n : c.maxInfluences;
				if (n == 0) {
					++c.sansPoids;
					if (c.exemplesSansPoids.Size() < 8) {
						c.exemplesSansPoids.PushBack(v);
					}
					continue;
				}
				if (std::fabs(w.Sum(v) - 1.f) > tol) {
					++c.sommesFausses;
				}
				if (n > NkSkinWeights::kGpu) {
					++c.tropInfluences;
				}
				const NkSkinInfluence *s = w.Of(v);
				for (uint32 k = 0; k < NkSkinWeights::kSlots; ++k) {
					if (s[k].bone >= 0 && (uint32)s[k].bone < arm.Count() && s[k].weight > maxParOs[(uint32)s[k].bone]) {
						maxParOs[(uint32)s[k].bone] = s[k].weight;
					}
				}
			}
			for (uint32 b = 0; b < arm.Count(); ++b) {
				if (arm.bones[b].deform && maxParOs[b] < 1e-4f) {
					c.osOrphelins.PushBack((int32)b);
				}
				if (!arm.bones[b].deform && maxParOs[b] >= 1e-4f) {
					c.osNonDeformants.PushBack((int32)b);
				}
			}
			return c;
		}

		NkString NkWeightCheck::Describe(const NkArmature &arm) const {
			NkString r;
			if (sansPoids > 0) {
				r.Append(NkString::Format("%u sommet(s) sans poids. ", sansPoids).CStr());
			}
			if (sommesFausses > 0) {
				r.Append(NkString::Format("%u sommet(s) dont la somme n'est pas 1 (normaliser). ", sommesFausses).CStr());
			}
			if (tropInfluences > 0) {
				r.Append(NkString::Format("%u sommet(s) tires par plus de 4 os (limiter a 4). ", tropInfluences).CStr());
			}
			if (!osOrphelins.Empty()) {
				r.Append(NkString::Format("%u os deformant(s) orphelin(s) :", (uint32)osOrphelins.Size()).CStr());
				for (uint32 k = 0; k < (uint32)osOrphelins.Size() && k < 6; ++k) {
					r.Append(" ");
					r.Append(arm.bones[(uint32)osOrphelins[k]].name.CStr());
				}
				r.Append(osOrphelins.Size() > 6 ? "... " : ". ");
			}
			if (!osNonDeformants.Empty()) {
				r.Append(NkString::Format("%u os de controle porte(nt) des poids. ", (uint32)osNonDeformants.Size()).CStr());
			}
			if (r.Empty()) {
				r = NkString::Format("Les poids sont sains : %u sommets, au plus %u os par sommet, sommes a 1.", vertices, maxInfluences);
			}
			return r;
		}

	} // namespace anim
} // namespace nkentseu
