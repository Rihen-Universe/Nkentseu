// =============================================================================
// NkMesh.cpp — maillage voxel + export OBJ (NKAI, Phase 6).
// =============================================================================
#include "NKGen/NkMesh.h"

#include <cstdio>
#include <cmath>

namespace nkentseu {
	namespace ai {
		namespace gen {

			// Ajoute un quad (a,b,c,d, CCW vu de l'extérieur) = 2 triangles.
			static void AddQuad(NkMesh &m, float ax, float ay, float az, float bx, float by, float bz, float cx,
								float cy, float cz, float dx, float dy, float dz) {
				uint32 base = m.positions.Size() / 3;
				float p[12] = {ax, ay, az, bx, by, bz, cx, cy, cz, dx, dy, dz};
				for (int i = 0; i < 12; ++i)
					m.positions.PushBack(p[i]);
				// (a,b,c) et (a,c,d)
				m.triangles.PushBack(base + 0);
				m.triangles.PushBack(base + 1);
				m.triangles.PushBack(base + 2);
				m.triangles.PushBack(base + 0);
				m.triangles.PushBack(base + 2);
				m.triangles.PushBack(base + 3);
			}

			NkMesh VoxelsToMesh(const float *v, uint32 G, float iso, float cell) {
				NkMesh mesh;
				auto occ = [&](int x, int y, int z) -> bool {
					if (x < 0 || y < 0 || z < 0 || x >= (int)G || y >= (int)G || z >= (int)G)
						return false;
					return v[(uint32)z * G * G + (uint32)y * G + (uint32)x] > iso;
				};

				for (int z = 0; z < (int)G; ++z)
					for (int y = 0; y < (int)G; ++y)
						for (int x = 0; x < (int)G; ++x) {
							if (!occ(x, y, z))
								continue;
							const float x0 = (float)x * cell, y0 = (float)y * cell, z0 = (float)z * cell;
							const float x1 = (float)(x + 1) * cell, y1 = (float)(y + 1) * cell,
										z1 = (float)(z + 1) * cell;

							// Une face n'est émise que si le voisin dans cette direction est vide.
							// -z (face avant)
							if (!occ(x, y, z - 1))
								AddQuad(mesh, x0, y0, z0, x0, y1, z0, x1, y1, z0, x1, y0, z0);
							// +z (arrière)
							if (!occ(x, y, z + 1))
								AddQuad(mesh, x0, y0, z1, x1, y0, z1, x1, y1, z1, x0, y1, z1);
							// -y (bas)
							if (!occ(x, y - 1, z))
								AddQuad(mesh, x0, y0, z0, x1, y0, z0, x1, y0, z1, x0, y0, z1);
							// +y (haut)
							if (!occ(x, y + 1, z))
								AddQuad(mesh, x0, y1, z0, x0, y1, z1, x1, y1, z1, x1, y1, z0);
							// -x (gauche)
							if (!occ(x - 1, y, z))
								AddQuad(mesh, x0, y0, z0, x0, y0, z1, x0, y1, z1, x0, y1, z0);
							// +x (droite)
							if (!occ(x + 1, y, z))
								AddQuad(mesh, x1, y0, z0, x1, y1, z0, x1, y1, z1, x1, y0, z1);
						}
				return mesh;
			}

			// ---- Naive Surface Nets ----------------------------------------
			// Coins d'une cellule cube : i = x + 2y + 4z.
			//   0(0,0,0) 1(1,0,0) 2(0,1,0) 3(1,1,0) 4(0,0,1) 5(1,0,1) 6(0,1,1) 7(1,1,1)
			static const int kCorner[8][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0},
											  {0, 0, 1}, {1, 0, 1}, {0, 1, 1}, {1, 1, 1}};
			// 12 arêtes (paires de coins différant d'un seul axe).
			static const int kEdge[12][2] = {
				{0, 1}, {2, 3}, {4, 5}, {6, 7}, // le long de x
				{0, 2}, {1, 3}, {4, 6}, {5, 7}, // le long de y
				{0, 4}, {1, 5}, {2, 6}, {3, 7}, // le long de z
			};

			NkMesh SurfaceNets(const float *f, uint32 nx, uint32 ny, uint32 nz, float iso, float cell) {
				NkMesh mesh;
				if (nx < 2 || ny < 2 || nz < 2)
					return mesh;
				const uint32 Cx = nx - 1, Cy = ny - 1, Cz = nz - 1; // nb de cellules

				auto sample = [&](uint32 x, uint32 y, uint32 z) -> float {
					return f[(uint32)x + nx * ((uint32)y + ny * (uint32)z)];
				};
				auto solid = [&](uint32 x, uint32 y, uint32 z) -> bool { return sample(x, y, z) > iso; };
				// Index de sommet par cellule (-1 = pas de surface).
				NkVector<int32> buffer;
				buffer.Resize(Cx * Cy * Cz);
				for (uint32 i = 0; i < buffer.Size(); ++i)
					buffer[i] = -1;
				auto cellIdx = [&](uint32 x, uint32 y, uint32 z) -> uint32 { return x + Cx * (y + Cy * z); };

				// 1) Un sommet par cellule traversant l'iso-surface.
				for (uint32 z = 0; z < Cz; ++z)
					for (uint32 y = 0; y < Cy; ++y)
						for (uint32 x = 0; x < Cx; ++x) {
							float g[8];
							int mask = 0;
							for (int i = 0; i < 8; ++i) {
								g[i] = sample(x + kCorner[i][0], y + kCorner[i][1], z + kCorner[i][2]);
								if (g[i] > iso)
									mask |= (1 << i);
							}
							if (mask == 0 || mask == 255)
								continue; // cellule entièrement in/out

							// Position = moyenne des points d'intersection des arêtes.
							float vx = 0.f, vy = 0.f, vz = 0.f;
							int ec = 0;
							for (int e = 0; e < 12; ++e) {
								int a = kEdge[e][0], b = kEdge[e][1];
								bool sa = (mask >> a) & 1, sb = (mask >> b) & 1;
								if (sa == sb)
									continue;
								float fa = g[a], fb = g[b];
								float d = fb - fa;
								float t = (d > 1e-6f || d < -1e-6f) ? (iso - fa) / d : 0.5f;
								vx += (float)kCorner[a][0] + t * (float)(kCorner[b][0] - kCorner[a][0]);
								vy += (float)kCorner[a][1] + t * (float)(kCorner[b][1] - kCorner[a][1]);
								vz += (float)kCorner[a][2] + t * (float)(kCorner[b][2] - kCorner[a][2]);
								++ec;
							}
							if (ec == 0)
								continue;
							vx /= ec;
							vy /= ec;
							vz /= ec;
							int32 vi = (int32)(mesh.positions.Size() / 3);
							mesh.positions.PushBack(((float)x + vx) * cell);
							mesh.positions.PushBack(((float)y + vy) * cell);
							mesh.positions.PushBack(((float)z + vz) * cell);
							buffer[cellIdx(x, y, z)] = vi;
						}

				// 2) Relier les cellules : pour chaque arête de grille traversant l'iso,
				//    un quad joint les 4 cellules qui la partagent.
				for (uint32 z = 0; z < Cz; ++z)
					for (uint32 y = 0; y < Cy; ++y)
						for (uint32 x = 0; x < Cx; ++x) {
							int32 v0 = buffer[cellIdx(x, y, z)];
							if (v0 < 0)
								continue;
							bool s0 = solid(x, y, z);

							// 3 arêtes émanant du coin (x,y,z) le long de +x,+y,+z.
							for (int d = 0; d < 3; ++d) {
								uint32 sx = x + (d == 0 ? 1 : 0);
								uint32 sy = y + (d == 1 ? 1 : 0);
								uint32 sz = z + (d == 2 ? 1 : 0);
								bool sd = solid(sx, sy, sz);
								if (s0 == sd)
									continue; // pas de changement de signe

								// Axes perpendiculaires (u,v) à d.
								int u = (d + 1) % 3, w = (d + 2) % 3;
								// Les 4 cellules autour de l'arête : offsets (0,0),(-1,0),(-1,-1),(0,-1) en (u,w).
								const int off[4][2] = {{0, 0}, {-1, 0}, {-1, -1}, {0, -1}};
								int32 q[4];
								bool ok = true;
								for (int c = 0; c < 4; ++c) {
									int cx = (int)x, cy = (int)y, cz = (int)z;
									int du = off[c][0], dv = off[c][1];
									if (u == 0)
										cx += du;
									else if (u == 1)
										cy += du;
									else
										cz += du;
									if (w == 0)
										cx += dv;
									else if (w == 1)
										cy += dv;
									else
										cz += dv;
									if (cx < 0 || cy < 0 || cz < 0) {
										ok = false;
										break;
									}
									q[c] = buffer[cellIdx((uint32)cx, (uint32)cy, (uint32)cz)];
									if (q[c] < 0) {
										ok = false;
										break;
									}
								}
								if (!ok)
									continue;

								// Deux triangles ; winding selon le côté solide (normales cohérentes).
								if (!s0) {
									mesh.triangles.PushBack(q[0]);
									mesh.triangles.PushBack(q[1]);
									mesh.triangles.PushBack(q[2]);
									mesh.triangles.PushBack(q[0]);
									mesh.triangles.PushBack(q[2]);
									mesh.triangles.PushBack(q[3]);
								} else {
									mesh.triangles.PushBack(q[0]);
									mesh.triangles.PushBack(q[2]);
									mesh.triangles.PushBack(q[1]);
									mesh.triangles.PushBack(q[0]);
									mesh.triangles.PushBack(q[3]);
									mesh.triangles.PushBack(q[2]);
								}
							}
						}
				return mesh;
			}

			NkMesh SurfaceNetsQuads(const float *f, uint32 nx, uint32 ny, uint32 nz, float iso, float cell) {
				NkMesh mesh;
				if (nx < 2 || ny < 2 || nz < 2)
					return mesh;
				const uint32 Cx = nx - 1, Cy = ny - 1, Cz = nz - 1;

				auto sample = [&](uint32 x, uint32 y, uint32 z) -> float { return f[x + nx * (y + ny * z)]; };
				auto solid = [&](uint32 x, uint32 y, uint32 z) -> bool { return sample(x, y, z) > iso; };
				NkVector<int32> buffer;
				buffer.Resize(Cx * Cy * Cz);
				for (uint32 i = 0; i < buffer.Size(); ++i)
					buffer[i] = -1;
				auto cellIdx = [&](uint32 x, uint32 y, uint32 z) -> uint32 { return x + Cx * (y + Cy * z); };

				// 1) Sommets (identique à SurfaceNets).
				for (uint32 z = 0; z < Cz; ++z)
					for (uint32 y = 0; y < Cy; ++y)
						for (uint32 x = 0; x < Cx; ++x) {
							float g[8];
							int mask = 0;
							for (int i = 0; i < 8; ++i) {
								g[i] = sample(x + kCorner[i][0], y + kCorner[i][1], z + kCorner[i][2]);
								if (g[i] > iso)
									mask |= (1 << i);
							}
							if (mask == 0 || mask == 255)
								continue;
							float vx = 0.f, vy = 0.f, vz = 0.f;
							int ec = 0;
							for (int e = 0; e < 12; ++e) {
								int a = kEdge[e][0], b = kEdge[e][1];
								if (((mask >> a) & 1) == ((mask >> b) & 1))
									continue;
								float fa = g[a], fb = g[b], d = fb - fa;
								float t = (d > 1e-6f || d < -1e-6f) ? (iso - fa) / d : 0.5f;
								vx += (float)kCorner[a][0] + t * (float)(kCorner[b][0] - kCorner[a][0]);
								vy += (float)kCorner[a][1] + t * (float)(kCorner[b][1] - kCorner[a][1]);
								vz += (float)kCorner[a][2] + t * (float)(kCorner[b][2] - kCorner[a][2]);
								++ec;
							}
							if (ec == 0)
								continue;
							vx /= ec;
							vy /= ec;
							vz /= ec;
							int32 vi = (int32)(mesh.positions.Size() / 3);
							mesh.positions.PushBack(((float)x + vx) * cell);
							mesh.positions.PushBack(((float)y + vy) * cell);
							mesh.positions.PushBack(((float)z + vz) * cell);
							buffer[cellIdx(x, y, z)] = vi;
						}

				// 2) Faces QUAD (pas de triangulation).
				for (uint32 z = 0; z < Cz; ++z)
					for (uint32 y = 0; y < Cy; ++y)
						for (uint32 x = 0; x < Cx; ++x) {
							int32 v0 = buffer[cellIdx(x, y, z)];
							if (v0 < 0)
								continue;
							bool s0 = solid(x, y, z);
							for (int d = 0; d < 3; ++d) {
								uint32 sx = x + (d == 0 ? 1 : 0), sy = y + (d == 1 ? 1 : 0), sz = z + (d == 2 ? 1 : 0);
								if (s0 == solid(sx, sy, sz))
									continue;
								int u = (d + 1) % 3, w = (d + 2) % 3;
								const int off[4][2] = {{0, 0}, {-1, 0}, {-1, -1}, {0, -1}};
								int32 q[4];
								bool ok = true;
								for (int c = 0; c < 4; ++c) {
									int cx = (int)x, cy = (int)y, cz = (int)z, du = off[c][0], dv = off[c][1];
									if (u == 0)
										cx += du;
									else if (u == 1)
										cy += du;
									else
										cz += du;
									if (w == 0)
										cx += dv;
									else if (w == 1)
										cy += dv;
									else
										cz += dv;
									if (cx < 0 || cy < 0 || cz < 0) {
										ok = false;
										break;
									}
									q[c] = buffer[cellIdx((uint32)cx, (uint32)cy, (uint32)cz)];
									if (q[c] < 0) {
										ok = false;
										break;
									}
								}
								if (!ok)
									continue;
								if (!s0) {
									mesh.quads.PushBack(q[0]);
									mesh.quads.PushBack(q[1]);
									mesh.quads.PushBack(q[2]);
									mesh.quads.PushBack(q[3]);
								} else {
									mesh.quads.PushBack(q[0]);
									mesh.quads.PushBack(q[3]);
									mesh.quads.PushBack(q[2]);
									mesh.quads.PushBack(q[1]);
								}
							}
						}
				return mesh;
			}

			NkMesh DecimateClustering(const NkMesh &src, uint32 gridResolution) {
				NkMesh out;
				const uint32 vc = src.VertexCount();
				const uint32 G = gridResolution < 1 ? 1 : gridResolution;
				if (vc == 0)
					return out;

				// Boîte englobante.
				float mn[3] = {1e30f, 1e30f, 1e30f};
				float mx[3] = {-1e30f, -1e30f, -1e30f};
				for (uint32 i = 0; i < vc; ++i)
					for (int k = 0; k < 3; ++k) {
						float p = src.positions[i * 3 + k];
						if (p < mn[k])
							mn[k] = p;
						if (p > mx[k])
							mx[k] = p;
					}
				float ext[3];
				for (int k = 0; k < 3; ++k) {
					ext[k] = mx[k] - mn[k];
					if (ext[k] < 1e-6f)
						ext[k] = 1e-6f;
				}

				// Accumulateurs par cellule (grille dense G³).
				const uint32 NC = G * G * G;
				NkVector<float> acc;
				acc.Resize(NC * 3);
				NkVector<uint32> cnt;
				cnt.Resize(NC);
				NkVector<int32> vidx;
				vidx.Resize(NC); // index du sommet de sortie par cellule
				for (uint32 i = 0; i < NC; ++i) {
					acc[i * 3] = acc[i * 3 + 1] = acc[i * 3 + 2] = 0.f;
					cnt[i] = 0;
					vidx[i] = -1;
				}

				auto cellOf = [&](uint32 v) -> uint32 {
					uint32 c[3];
					for (int k = 0; k < 3; ++k) {
						float t = (src.positions[v * 3 + k] - mn[k]) / ext[k]; // [0,1]
						int ci = (int)(t * (float)G);
						if (ci < 0)
							ci = 0;
						if (ci >= (int)G)
							ci = (int)G - 1;
						c[k] = (uint32)ci;
					}
					return c[0] + G * (c[1] + G * c[2]);
				};

				// 1) Accumuler les positions par cellule.
				NkVector<uint32> remap;
				remap.Resize(vc);
				for (uint32 v = 0; v < vc; ++v) {
					uint32 cell = cellOf(v);
					acc[cell * 3 + 0] += src.positions[v * 3 + 0];
					acc[cell * 3 + 1] += src.positions[v * 3 + 1];
					acc[cell * 3 + 2] += src.positions[v * 3 + 2];
					cnt[cell]++;
					remap[v] = cell; // provisoire : cellule -> convertie en index de sortie après
				}

				// 2) Créer un sommet moyen par cellule occupée.
				for (uint32 cell = 0; cell < NC; ++cell) {
					if (cnt[cell] == 0)
						continue;
					int32 ni = (int32)(out.positions.Size() / 3);
					out.positions.PushBack(acc[cell * 3 + 0] / (float)cnt[cell]);
					out.positions.PushBack(acc[cell * 3 + 1] / (float)cnt[cell]);
					out.positions.PushBack(acc[cell * 3 + 2] / (float)cnt[cell]);
					vidx[cell] = ni;
				}

				// 3) Remapper les triangles, supprimer les dégénérés.
				const uint32 tc = src.TriangleCount();
				for (uint32 t = 0; t < tc; ++t) {
					uint32 a = vidx[remap[src.triangles[t * 3 + 0]]];
					uint32 b = vidx[remap[src.triangles[t * 3 + 1]]];
					uint32 c = vidx[remap[src.triangles[t * 3 + 2]]];
					if (a == b || b == c || a == c)
						continue; // dégénéré -> supprimé
					out.triangles.PushBack(a);
					out.triangles.PushBack(b);
					out.triangles.PushBack(c);
				}
				return out;
			}

			void ComputeNormals(NkMesh &mesh) {
				const uint32 vc = mesh.VertexCount();
				mesh.normals.Resize(vc * 3);
				for (uint32 i = 0; i < vc * 3; ++i)
					mesh.normals[i] = 0.f;

				auto accum = [&](uint32 a, uint32 b, uint32 c) {
					const float *pa = &mesh.positions[a * 3];
					const float *pb = &mesh.positions[b * 3];
					const float *pc = &mesh.positions[c * 3];
					float ux = pb[0] - pa[0], uy = pb[1] - pa[1], uz = pb[2] - pa[2];
					float vx = pc[0] - pa[0], vy = pc[1] - pa[1], vz = pc[2] - pa[2];
					// Produit vectoriel NON normalisé : magnitude ~ 2*aire -> pondération par aire.
					float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
					uint32 idx[3] = {a, b, c};
					for (int k = 0; k < 3; ++k) {
						mesh.normals[idx[k] * 3 + 0] += nx;
						mesh.normals[idx[k] * 3 + 1] += ny;
						mesh.normals[idx[k] * 3 + 2] += nz;
					}
				};
				const uint32 tc = mesh.TriangleCount();
				for (uint32 t = 0; t < tc; ++t)
					accum(mesh.triangles[t * 3 + 0], mesh.triangles[t * 3 + 1], mesh.triangles[t * 3 + 2]);
				const uint32 qc = mesh.QuadCount();
				for (uint32 q = 0; q < qc; ++q) { // quad -> 2 triangles pour l'accumulation
					uint32 a = mesh.quads[q * 4 + 0], b = mesh.quads[q * 4 + 1], c = mesh.quads[q * 4 + 2],
						   d = mesh.quads[q * 4 + 3];
					accum(a, b, c);
					accum(a, c, d);
				}
				for (uint32 i = 0; i < vc; ++i) {
					float *n = &mesh.normals[i * 3];
					float L = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
					if (L > 1e-8f) {
						n[0] /= L;
						n[1] /= L;
						n[2] /= L;
					} else {
						n[0] = 0.f;
						n[1] = 1.f;
						n[2] = 0.f;
					}
				}
			}

			void BuildInterleavedPN(const NkMesh &mesh, NkVector<float> &outV, NkVector<uint32> &outI) {
				NkMesh tmp = mesh;
				if (!tmp.HasNormals())
					ComputeNormals(tmp);
				const uint32 vc = tmp.VertexCount();
				outV.Resize(vc * 6);
				for (uint32 i = 0; i < vc; ++i) {
					outV[i * 6 + 0] = tmp.positions[i * 3 + 0];
					outV[i * 6 + 1] = tmp.positions[i * 3 + 1];
					outV[i * 6 + 2] = tmp.positions[i * 3 + 2];
					outV[i * 6 + 3] = tmp.normals[i * 3 + 0];
					outV[i * 6 + 4] = tmp.normals[i * 3 + 1];
					outV[i * 6 + 5] = tmp.normals[i * 3 + 2];
				}
				outI.Resize(0);
				for (uint32 i = 0; i < tmp.triangles.Size(); ++i)
					outI.PushBack(tmp.triangles[i]);
				// Quads -> triangles pour un index buffer standard.
				const uint32 qc = tmp.QuadCount();
				for (uint32 q = 0; q < qc; ++q) {
					uint32 a = tmp.quads[q * 4 + 0], b = tmp.quads[q * 4 + 1], c = tmp.quads[q * 4 + 2],
						   d = tmp.quads[q * 4 + 3];
					outI.PushBack(a);
					outI.PushBack(b);
					outI.PushBack(c);
					outI.PushBack(a);
					outI.PushBack(c);
					outI.PushBack(d);
				}
			}

			bool SaveMeshObj(const char *path, const NkMesh &mesh) {
				FILE *f = fopen(path, "wb");
				if (!f)
					return false;
				fprintf(f, "# NKGen — maillage généré (voxels -> triangles)\n");
				fprintf(f, "o nkgen_shape\n");
				const uint32 vc = mesh.VertexCount();
				for (uint32 i = 0; i < vc; ++i)
					fprintf(f, "v %.4f %.4f %.4f\n", mesh.positions[i * 3 + 0], mesh.positions[i * 3 + 1],
							mesh.positions[i * 3 + 2]);
				const bool hasN = mesh.HasNormals();
				if (hasN)
					for (uint32 i = 0; i < vc; ++i)
						fprintf(f, "vn %.4f %.4f %.4f\n", mesh.normals[i * 3 + 0], mesh.normals[i * 3 + 1],
								mesh.normals[i * 3 + 2]);
				const uint32 tc = mesh.TriangleCount(); // indices 1-based ; v//vn si normales
				for (uint32 i = 0; i < tc; ++i) {
					uint32 a = mesh.triangles[i * 3 + 0] + 1, b = mesh.triangles[i * 3 + 1] + 1,
						   c = mesh.triangles[i * 3 + 2] + 1;
					if (hasN)
						fprintf(f, "f %u//%u %u//%u %u//%u\n", a, a, b, b, c, c);
					else
						fprintf(f, "f %u %u %u\n", a, b, c);
				}
				const uint32 qc = mesh.QuadCount(); // faces quad (f a b c d)
				for (uint32 i = 0; i < qc; ++i) {
					uint32 a = mesh.quads[i * 4 + 0] + 1, b = mesh.quads[i * 4 + 1] + 1, c = mesh.quads[i * 4 + 2] + 1,
						   d = mesh.quads[i * 4 + 3] + 1;
					if (hasN)
						fprintf(f, "f %u//%u %u//%u %u//%u %u//%u\n", a, a, b, b, c, c, d, d);
					else
						fprintf(f, "f %u %u %u %u\n", a, b, c, d);
				}
				fclose(f);
				return true;
			}

		} // namespace gen
	} // namespace ai
} // namespace nkentseu
