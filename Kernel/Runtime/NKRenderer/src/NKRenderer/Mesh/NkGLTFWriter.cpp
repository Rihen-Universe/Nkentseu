// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkGLTFWriter.cpp — l'ecrivain .glb (voir NkGLTFWriter.h).
//
// LE FORMAT .glb : [en-tete 12 o : 'glTF', 2, longueur] [morceau JSON : longueur,
// 'JSON', texte complete d'espaces a 4 o] [morceau BIN : longueur, 'BIN\0',
// donnees completees de zeros a 4 o]. Chaque bufferView commence sur 4 o.
// =============================================================================
#include "NKRenderer/Mesh/NkGLTFWriter.h"

#include "NKFileSystem/NkFile.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace renderer {

		using math::NkMat4f;
		using math::NkVec2f;
		using math::NkVec3f;

		namespace {
			struct Tampon {
					NkVector<nk_uint8> octets;
					void Aligner() {
						while (octets.Size() % 4u) {
							octets.PushBack(0);
						}
					}
					uint32 Ajouter(const void *p, usize n) {
						Aligner();
						const uint32 debut = (uint32)octets.Size();
						const nk_uint8 *b = (const nk_uint8 *)p;
						for (usize i = 0; i < n; ++i) {
							octets.PushBack(b[i]);
						}
						return debut;
					}
			};

			void Num(NkString &j, float32 v) {
				char b[48];
				if (!(v == v) || std::fabs(v) > 3.0e38f) {
					v = 0.f; // un NaN ou un infini n'est pas du JSON
				}
				std::snprintf(b, sizeof(b), "%.9g", (double)v);
				j.Append(b);
			}
			void Ent(NkString &j, long long v) {
				char b[32];
				std::snprintf(b, sizeof(b), "%lld", v);
				j.Append(b);
			}
			/// Une chaine JSON (guillemets et barres echappes).
			void Texte(NkString &j, const NkString &s) {
				j.Append("\"");
				for (usize i = 0; i < s.Size(); ++i) {
					const char c = s[(uint32)i];
					if (c == '"' || c == '\\') {
						j.Append("\\");
						char t[2] = {c, 0};
						j.Append(t);
					} else if ((unsigned char)c < 0x20) {
						j.Append(" ");
					} else {
						char t[2] = {c, 0};
						j.Append(t);
					}
				}
				j.Append("\"");
			}

			struct Vue {
					uint32 offset, length;
					int32 target; ///< 34962 ARRAY_BUFFER, 34963 ELEMENT_ARRAY_BUFFER, 0 aucun
			};
			struct Accesseur {
					uint32 vue;
					uint32 composant; ///< 5126 FLOAT, 5123 UNSIGNED_SHORT, 5125 UNSIGNED_INT
					uint32 nombre;
					const char *type;
					bool bornes = false;
					float32 mn[3] = {0, 0, 0}, mx[3] = {0, 0, 0};
			};
		} // namespace

		bool NkWriteGLB(const NkString &path, const NkGLTFWriteDesc &d, NkString *why) {
			auto Refus = [&](const char *m) {
				if (why != nullptr) {
					*why = NkString(m);
				}
				return false;
			};
			if (d.positions == nullptr || d.vertexCount == 0 || d.indices == nullptr || d.indexCount < 3) {
				return Refus("maillage vide (positions ou triangles absents)");
			}
			const bool peau = d.jointCount > 0 && d.jointWorld != nullptr && d.jointParent != nullptr && d.joints4 != nullptr &&
							  d.weights4 != nullptr;
			for (uint32 i = 0; i < d.indexCount; ++i) {
				if (d.indices[i] >= d.vertexCount) {
					return Refus("un indice de triangle depasse le nombre de sommets");
				}
			}
			Tampon bin;
			NkVector<Vue> vues;
			NkVector<Accesseur> acc;
			auto VueDe = [&](const void *p, usize n, int32 cible) -> uint32 {
				Vue v;
				v.offset = bin.Ajouter(p, n);
				v.length = (uint32)n;
				v.target = cible;
				vues.PushBack(v);
				return (uint32)vues.Size() - 1u;
			};
			auto Acc = [&](uint32 vue, uint32 comp, uint32 n, const char *type) -> uint32 {
				Accesseur a;
				a.vue = vue;
				a.composant = comp;
				a.nombre = n;
				a.type = type;
				acc.PushBack(a);
				return (uint32)acc.Size() - 1u;
			};
			auto Bornes = [&](uint32 a, const NkVec3f *p, uint32 n) {
				Accesseur &x = acc[a];
				x.bornes = true;
				for (uint32 c = 0; c < 3; ++c) {
					x.mn[c] = 1e30f;
					x.mx[c] = -1e30f;
				}
				for (uint32 i = 0; i < n; ++i) {
					const float32 v[3] = {p[i].x, p[i].y, p[i].z};
					for (uint32 c = 0; c < 3; ++c) {
						x.mn[c] = v[c] < x.mn[c] ? v[c] : x.mn[c];
						x.mx[c] = v[c] > x.mx[c] ? v[c] : x.mx[c];
					}
				}
			};
			const uint32 nv = d.vertexCount;
			// Les attributs.
			NkVector<float32> f3;
			auto Vec3 = [&](const NkVec3f *p) -> uint32 {
				f3.Resize(nv * 3u);
				for (uint32 i = 0; i < nv; ++i) {
					f3[i * 3] = p[i].x;
					f3[i * 3 + 1] = p[i].y;
					f3[i * 3 + 2] = p[i].z;
				}
				return VueDe(f3.Data(), (usize)nv * 12u, 34962);
			};
			const uint32 aPos = Acc(Vec3(d.positions), 5126, nv, "VEC3");
			Bornes(aPos, d.positions, nv);
			int32 aNor = -1, aUv = -1, aJoi = -1, aPoi = -1;
			if (d.normals != nullptr) {
				aNor = (int32)Acc(Vec3(d.normals), 5126, nv, "VEC3");
			}
			if (d.uvs != nullptr) {
				NkVector<float32> f2;
				f2.Resize(nv * 2u);
				for (uint32 i = 0; i < nv; ++i) {
					f2[i * 2] = d.uvs[i].x;
					f2[i * 2 + 1] = d.uvs[i].y;
				}
				aUv = (int32)Acc(VueDe(f2.Data(), (usize)nv * 8u, 34962), 5126, nv, "VEC2");
			}
			if (peau) {
				NkVector<uint16> j4;
				NkVector<float32> w4;
				j4.Resize(nv * 4u);
				w4.Resize(nv * 4u);
				for (uint32 i = 0; i < nv; ++i) {
					float32 s = 0.f;
					for (uint32 k = 0; k < 4; ++k) {
						const float32 ji = d.joints4[i * 4 + k];
						const int32 j = (int32)(ji + 0.5f);
						const float32 w = d.weights4[i * 4 + k];
						j4[i * 4 + k] = (uint16)((j >= 0 && (uint32)j < d.jointCount && w > 0.f) ? j : 0);
						w4[i * 4 + k] = w > 0.f ? w : 0.f;
						s += w4[i * 4 + k];
					}
					// glTF exige des poids NORMALISES (somme 1).
					if (s > 1e-12f) {
						for (uint32 k = 0; k < 4; ++k) {
							w4[i * 4 + k] /= s;
						}
					} else {
						w4[i * 4] = 1.f;
					}
				}
				aJoi = (int32)Acc(VueDe(j4.Data(), (usize)nv * 8u, 34962), 5123, nv, "VEC4");
				aPoi = (int32)Acc(VueDe(w4.Data(), (usize)nv * 16u, 34962), 5126, nv, "VEC4");
			}
			const uint32 aIdx = Acc(VueDe(d.indices, (usize)d.indexCount * 4u, 34963), 5125, d.indexCount, "SCALAR");
			// Les cibles de morph.
			NkVector<int32> aMorphPos, aMorphNor;
			for (uint32 m = 0; m < (uint32)d.morphs.Size(); ++m) {
				const NkGLTFWriteMorph &mo = d.morphs[m];
				if (mo.dPos == nullptr) {
					return Refus("une forme n'a pas de deltas de position");
				}
				const uint32 a = Acc(Vec3(mo.dPos), 5126, nv, "VEC3");
				Bornes(a, mo.dPos, nv);
				aMorphPos.PushBack((int32)a);
				aMorphNor.PushBack(mo.dNormal != nullptr ? (int32)Acc(Vec3(mo.dNormal), 5126, nv, "VEC3") : -1);
			}
			int32 aIbm = -1;
			if (peau) {
				NkVector<float32> ibm;
				ibm.Resize(d.jointCount * 16u);
				for (uint32 j = 0; j < d.jointCount; ++j) {
					const NkMat4f m = d.inverseBind != nullptr ? d.inverseBind[j] : d.jointWorld[j].Inverse();
					for (uint32 e = 0; e < 16; ++e) {
						ibm[j * 16 + e] = m.data[e];
					}
				}
				aIbm = (int32)Acc(VueDe(ibm.Data(), (usize)d.jointCount * 64u, 0), 5126, d.jointCount, "MAT4");
			}
			bin.Aligner();

			// ── LE JSON ────────────────────────────────────────────────────────
			NkString j("{\"asset\":{\"version\":\"2.0\",\"generator\":\"Nkentseu NkGLTFWriter (NkAnimaEditor)\"}");
			// Les noeuds : les joints d'abord (0..J-1), puis le maillage (J).
			const uint32 noeudMaillage = peau ? d.jointCount : 0u;
			j.Append(",\"scene\":0,\"scenes\":[{\"nodes\":[");
			bool premier = true;
			if (peau) {
				for (uint32 k = 0; k < d.jointCount; ++k) {
					if (d.jointParent[k] < 0 || (uint32)d.jointParent[k] >= d.jointCount) {
						if (!premier) {
							j.Append(",");
						}
						Ent(j, k);
						premier = false;
					}
				}
			}
			if (!premier) {
				j.Append(",");
			}
			Ent(j, noeudMaillage);
			j.Append("]}],\"nodes\":[");
			if (peau) {
				for (uint32 k = 0; k < d.jointCount; ++k) {
					if (k > 0) {
						j.Append(",");
					}
					j.Append("{\"name\":");
					char defaut[24];
					std::snprintf(defaut, sizeof(defaut), "joint_%u", k);
					Texte(j, d.jointNames != nullptr && !d.jointNames[k].Empty() ? d.jointNames[k] : NkString(defaut));
					const int32 p = d.jointParent[k];
					const NkMat4f local = (p >= 0 && (uint32)p < d.jointCount) ? d.jointWorld[p].Inverse() * d.jointWorld[k] : d.jointWorld[k];
					j.Append(",\"matrix\":[");
					for (uint32 e = 0; e < 16; ++e) {
						if (e > 0) {
							j.Append(",");
						}
						Num(j, local.data[e]);
					}
					j.Append("]");
					bool enfants = false;
					for (uint32 c = 0; c < d.jointCount; ++c) {
						if (d.jointParent[c] == (int32)k) {
							j.Append(enfants ? "," : ",\"children\":[");
							Ent(j, c);
							enfants = true;
						}
					}
					if (enfants) {
						j.Append("]");
					}
					j.Append("}");
				}
				j.Append(",");
			}
			j.Append("{\"name\":");
			Texte(j, d.name);
			j.Append(",\"mesh\":0");
			if (peau) {
				j.Append(",\"skin\":0");
			}
			j.Append("}]");
			// Le maillage.
			j.Append(",\"meshes\":[{\"name\":");
			Texte(j, d.name);
			j.Append(",\"primitives\":[{\"attributes\":{\"POSITION\":");
			Ent(j, aPos);
			if (aNor >= 0) {
				j.Append(",\"NORMAL\":");
				Ent(j, aNor);
			}
			if (aUv >= 0) {
				j.Append(",\"TEXCOORD_0\":");
				Ent(j, aUv);
			}
			if (aJoi >= 0) {
				j.Append(",\"JOINTS_0\":");
				Ent(j, aJoi);
				j.Append(",\"WEIGHTS_0\":");
				Ent(j, aPoi);
			}
			j.Append("},\"indices\":");
			Ent(j, aIdx);
			j.Append(",\"material\":0,\"mode\":4");
			if (!d.morphs.Empty()) {
				j.Append(",\"targets\":[");
				for (uint32 m = 0; m < (uint32)d.morphs.Size(); ++m) {
					j.Append(m > 0 ? ",{\"POSITION\":" : "{\"POSITION\":");
					Ent(j, aMorphPos[m]);
					if (aMorphNor[m] >= 0) {
						j.Append(",\"NORMAL\":");
						Ent(j, aMorphNor[m]);
					}
					j.Append("}");
				}
				j.Append("]");
			}
			j.Append("}]");
			if (!d.morphs.Empty()) {
				j.Append(",\"weights\":[");
				for (uint32 m = 0; m < (uint32)d.morphs.Size(); ++m) {
					if (m > 0) {
						j.Append(",");
					}
					Num(j, d.morphs[m].defaultWeight);
				}
				j.Append("],\"extras\":{\"targetNames\":[");
				for (uint32 m = 0; m < (uint32)d.morphs.Size(); ++m) {
					if (m > 0) {
						j.Append(",");
					}
					Texte(j, d.morphs[m].name);
				}
				j.Append("]}");
			}
			j.Append("}]");
			// Le materiau.
			j.Append(",\"materials\":[{\"name\":\"Materiau\",\"pbrMetallicRoughness\":{\"baseColorFactor\":[");
			for (uint32 c = 0; c < 4; ++c) {
				if (c > 0) {
					j.Append(",");
				}
				Num(j, d.baseColor[c]);
			}
			j.Append("],\"metallicFactor\":0,\"roughnessFactor\":");
			Num(j, d.roughness);
			j.Append("}}]");
			// La peau.
			if (peau) {
				j.Append(",\"skins\":[{\"inverseBindMatrices\":");
				Ent(j, aIbm);
				j.Append(",\"joints\":[");
				for (uint32 k = 0; k < d.jointCount; ++k) {
					if (k > 0) {
						j.Append(",");
					}
					Ent(j, k);
				}
				j.Append("]}]");
			}
			// Les accesseurs, les vues, le tampon.
			j.Append(",\"accessors\":[");
			for (uint32 a = 0; a < (uint32)acc.Size(); ++a) {
				const Accesseur &x = acc[a];
				j.Append(a > 0 ? ",{\"bufferView\":" : "{\"bufferView\":");
				Ent(j, x.vue);
				j.Append(",\"componentType\":");
				Ent(j, x.composant);
				j.Append(",\"count\":");
				Ent(j, x.nombre);
				j.Append(",\"type\":\"");
				j.Append(x.type);
				j.Append("\"");
				if (x.bornes) {
					j.Append(",\"min\":[");
					Num(j, x.mn[0]);
					j.Append(",");
					Num(j, x.mn[1]);
					j.Append(",");
					Num(j, x.mn[2]);
					j.Append("],\"max\":[");
					Num(j, x.mx[0]);
					j.Append(",");
					Num(j, x.mx[1]);
					j.Append(",");
					Num(j, x.mx[2]);
					j.Append("]");
				}
				j.Append("}");
			}
			j.Append("],\"bufferViews\":[");
			for (uint32 v = 0; v < (uint32)vues.Size(); ++v) {
				j.Append(v > 0 ? ",{\"buffer\":0,\"byteOffset\":" : "{\"buffer\":0,\"byteOffset\":");
				Ent(j, vues[v].offset);
				j.Append(",\"byteLength\":");
				Ent(j, vues[v].length);
				if (vues[v].target != 0) {
					j.Append(",\"target\":");
					Ent(j, vues[v].target);
				}
				j.Append("}");
			}
			j.Append("],\"buffers\":[{\"byteLength\":");
			Ent(j, (long long)bin.octets.Size());
			j.Append("}]}");

			// ── L'ASSEMBLAGE .glb ──────────────────────────────────────────────
			while (j.Size() % 4u) {
				j.Append(" ");
			}
			const uint32 lj = (uint32)j.Size(), lb = (uint32)bin.octets.Size();
			const uint32 total = 12u + 8u + lj + 8u + lb;
			NkVector<nk_uint8> sortie;
			sortie.Reserve(total);
			auto U32 = [&](uint32 v) {
				for (uint32 k = 0; k < 4; ++k) {
					sortie.PushBack((nk_uint8)((v >> (8u * k)) & 0xFFu));
				}
			};
			U32(0x46546C67u); // 'glTF'
			U32(2u);
			U32(total);
			U32(lj);
			U32(0x4E4F534Au); // 'JSON'
			for (uint32 i = 0; i < lj; ++i) {
				sortie.PushBack((nk_uint8)j[i]);
			}
			U32(lb);
			U32(0x004E4942u); // 'BIN\0'
			for (uint32 i = 0; i < lb; ++i) {
				sortie.PushBack(bin.octets[i]);
			}
			if (!NkFile::WriteAllBytes(path.CStr(), sortie)) {
				return Refus("ecriture du fichier refusee");
			}
			return true;
		}

	} // namespace renderer
} // namespace nkentseu
