// =============================================================================
// NkFBXLoader.cpp — NKRenderer (Mesh/) — FBX binaire (sous-ensemble geometrie).
// Voir NkFBXLoader.h. Zero-STL. Inflate des arrays via NkDeflate (NKImage).
// =============================================================================
#include "NkFBXLoader.h"
#include "NkMeshLoaderUtil.h"

#include "NKLogger/NkLog.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkPath.h" // GetDirectory() — resolution des textures relatives
#include "NKImage/Core/NkImage.h" // NkDeflate::Decompress (zlib)
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace renderer {

		namespace {
			using namespace meshutil;

			struct FbxProp {
					char type = 0;
					float64 scalar = 0.0;
					int64 scalarI = 0; // Y/C/I/L en ENTIER exact (scalar en perd la precision
									   // au-dela de 2^53 — critique pour les ID d'objets FBX,
									   // souvent de grands entiers 64-bit non representables en double).
					NkVector<float64> arrF; // arrays f/d (en f64)
					NkVector<int64> arrI;	// arrays i/l/b
					NkString str;			// S/R
			};

			struct FbxNode {
					NkString name;
					NkVector<FbxProp> props;
					NkVector<FbxNode> children;

					const FbxNode *Find(const char *n) const {
						for (uint32 i = 0; i < (uint32)children.Size(); ++i)
							if (children[(NkVector<FbxNode>::SizeType)i].name == NkString(n))
								return &children[(NkVector<FbxNode>::SizeType)i];
						return nullptr;
					}
			};

			struct Cur {
					const uint8 *p;
					const uint8 *end;
					bool ok = true;
			};

			uint8 RU8(Cur &c) {
				if (c.p >= c.end) {
					c.ok = false;
					return 0;
				}
				return *c.p++;
			}

			uint32 RU32(Cur &c) {
				if (c.p + 4 > c.end) {
					c.ok = false;
					return 0;
				}
				uint32 v = ReadU32LE(c.p);
				c.p += 4;
				return v;
			}

			uint64 RU64(Cur &c) {
				if (c.p + 8 > c.end) {
					c.ok = false;
					return 0;
				}
				uint64 v = ReadU64LE(c.p);
				c.p += 8;
				return v;
			}

			bool ReadProp(Cur &c, FbxProp &pr) {
				char t = (char)RU8(c);
				pr.type = t;
				switch (t) {
					case 'Y': {
						if (c.p + 2 > c.end) {
							c.ok = false;
							return false;
						}
						pr.scalarI = (int16)ReadU16LE(c.p);
						pr.scalar = (float64)pr.scalarI;
						c.p += 2;
					} break;
					case 'C': {
						pr.scalarI = RU8(c);
						pr.scalar = (float64)pr.scalarI;
					} break;
					case 'I': {
						pr.scalarI = (int32)RU32(c);
						pr.scalar = (float64)pr.scalarI;
					} break;
					case 'F': {
						if (c.p + 4 > c.end) {
							c.ok = false;
							return false;
						}
						pr.scalar = (float64)ReadF32LE(c.p);
						c.p += 4;
					} break;
					case 'D': {
						if (c.p + 8 > c.end) {
							c.ok = false;
							return false;
						}
						pr.scalar = ReadF64LE(c.p);
						c.p += 8;
					} break;
					case 'L': {
						pr.scalarI = (int64)RU64(c);
						pr.scalar = (float64)pr.scalarI;
					} break;
					case 'S':
					case 'R': {
						uint32 len = RU32(c);
						if (c.p + len > c.end) {
							c.ok = false;
							return false;
						}
						if (t == 'S')
							for (uint32 i = 0; i < len; ++i)
								pr.str.Append((char)c.p[i]);
						c.p += len;
					} break;
					case 'f':
					case 'd':
					case 'l':
					case 'i':
					case 'b': {
						uint32 arrLen = RU32(c);
						uint32 enc = RU32(c);
						uint32 cmpLen = RU32(c);
						if (!c.ok)
							return false;
						uint32 elemSz = (t == 'd' || t == 'l') ? 8 : (t == 'b') ? 1 : 4;
						uint64 rawSz = (uint64)arrLen * elemSz;
						const uint8 *data = nullptr;
						NkVector<uint8> tmp;
						if (enc == 1) {
							if (c.p + cmpLen > c.end) {
								c.ok = false;
								return false;
							}
							tmp.Resize((NkVector<uint8>::SizeType)rawSz);
							usize written = 0;
							if (!NkDeflate::Decompress(c.p, cmpLen, tmp.Data(), (usize)rawSz, written) ||
								written < rawSz) {
								c.ok = false;
								return false;
							}
							c.p += cmpLen;
							data = tmp.Data();
						} else {
							if (c.p + rawSz > c.end) {
								c.ok = false;
								return false;
							}
							data = c.p;
							c.p += rawSz;
						}
						const uint8 *d = data;
						for (uint32 i = 0; i < arrLen; ++i) {
							if (t == 'f') {
								pr.arrF.PushBack((float64)ReadF32LE(d));
								d += 4;
							} else if (t == 'd') {
								pr.arrF.PushBack(ReadF64LE(d));
								d += 8;
							} else if (t == 'i') {
								pr.arrI.PushBack((int64)(int32)ReadU32LE(d));
								d += 4;
							} else if (t == 'l') {
								pr.arrI.PushBack((int64)ReadU64LE(d));
								d += 8;
							} else {
								pr.arrI.PushBack((int64)d[0]);
								d += 1;
							}
						}
					} break;
					default:
						c.ok = false;
						return false;
				}
				return c.ok;
			}

			bool ParseNode(Cur &c, const uint8 *base, bool is64, FbxNode &node, bool &isNull) {
				isNull = false;
				uint64 endOff = is64 ? RU64(c) : (uint64)RU32(c);
				uint64 numProps = is64 ? RU64(c) : (uint64)RU32(c);
				uint64 propLen = is64 ? RU64(c) : (uint64)RU32(c);
				(void)propLen;
				uint8 nameLen = RU8(c);
				if (!c.ok)
					return false;
				if (endOff == 0 && numProps == 0 && nameLen == 0) {
					isNull = true;
					return true;
				}
				for (uint32 i = 0; i < nameLen; ++i)
					node.name.Append((char)RU8(c));
				for (uint64 i = 0; i < numProps; ++i) {
					FbxProp pr;
					if (!ReadProp(c, pr))
						return false;
					node.props.PushBack(static_cast<FbxProp &&>(pr));
				}
				while (c.p < base + endOff && c.ok) {
					FbxNode child;
					bool childNull = false;
					if (!ParseNode(c, base, is64, child, childNull))
						return false;
					if (childNull)
						break;
					node.children.PushBack(static_cast<FbxNode &&>(child));
				}
				if (endOff > 0)
					c.p = base + endOff; // realignement
				return c.ok;
			}

			// Premier prop array f64 / i64 d'un noeud.
			const NkVector<float64> *ArrF(const FbxNode *n) {
				if (!n)
					return nullptr;
				for (uint32 i = 0; i < (uint32)n->props.Size(); ++i)
					if (!n->props[(NkVector<FbxProp>::SizeType)i].arrF.Empty())
						return &n->props[(NkVector<FbxProp>::SizeType)i].arrF;
				return nullptr;
			}

			const NkVector<int64> *ArrI(const FbxNode *n) {
				if (!n)
					return nullptr;
				for (uint32 i = 0; i < (uint32)n->props.Size(); ++i)
					if (!n->props[(NkVector<FbxProp>::SizeType)i].arrI.Empty())
						return &n->props[(NkVector<FbxProp>::SizeType)i].arrI;
				return nullptr;
			}

			NkString StrOf(const FbxNode *n) {
				if (n)
					for (uint32 i = 0; i < (uint32)n->props.Size(); ++i)
						if (!n->props[(NkVector<FbxProp>::SizeType)i].str.Empty())
							return n->props[(NkVector<FbxProp>::SizeType)i].str;
				return NkString("");
			}

			// ════════════════════════════════════════════════════════════════
			//  Materiaux/textures FBX — graphe d'objets (Objects/Connections).
			// ════════════════════════════════════════════════════════════════

			// ID d'un objet FBX (Model/Geometry/Material/Texture...) : 1re prop
			// entiere (I/L/Y/C, stockee en scalarI EXACT — cf. FbxProp — un ID
			// FBX peut depasser 2^53 et perdrait sa precision en double).
			int64 FbxIdOf(const FbxNode &n) {
				for (uint32 i = 0; i < (uint32)n.props.Size(); ++i) {
					const char t = n.props[(NkVector<FbxProp>::SizeType)i].type;
					if (t == 'L' || t == 'I' || t == 'Y' || t == 'C')
						return n.props[(NkVector<FbxProp>::SizeType)i].scalarI;
				}
				return -1;
			}

			struct FbxIdEntry {
					int64 id = -1;
					const FbxNode *node = nullptr;
			};

			const FbxNode *FindById(const NkVector<FbxIdEntry> &list, int64 id) {
				if (id < 0)
					return nullptr;
				for (uint32 i = 0; i < (uint32)list.Size(); ++i)
					if (list[(NkVector<FbxIdEntry>::SizeType)i].id == id)
						return list[(NkVector<FbxIdEntry>::SizeType)i].node;
				return nullptr;
			}
			bool HasId(const NkVector<FbxIdEntry> &list, int64 id) {
				return FindById(list, id) != nullptr;
			}

			// Connexions FBX (section racine "Connections", noeuds "C") : relient
			// deux objets (OO, ex. Geometry -> Model proprietaire, Material ->
			// Model) ou un objet a une PROPRIETE nommee d'un autre objet (OP, ex.
			// Texture -> Material."DiffuseColor").
			struct FbxConn {
					int64 child = -1, parent = -1;
					NkString prop; // seulement pour OP
					bool isProp = false;
			};

			void ParseConnections(const NkVector<FbxNode> &roots, NkVector<FbxConn> &out) {
				for (uint32 i = 0; i < (uint32)roots.Size(); ++i) {
					if (!(roots[(NkVector<FbxNode>::SizeType)i].name == NkString("Connections")))
						continue;
					const FbxNode &conns = roots[(NkVector<FbxNode>::SizeType)i];
					for (uint32 j = 0; j < (uint32)conns.children.Size(); ++j) {
						const FbxNode &c = conns.children[(NkVector<FbxNode>::SizeType)j];
						if (!(c.name == NkString("C")) || c.props.Size() < 3)
							continue;
						FbxConn fc;
						fc.isProp = (c.props[0].str == NkString("OP"));
						fc.child = c.props[(NkVector<FbxProp>::SizeType)1].scalarI;
						fc.parent = c.props[(NkVector<FbxProp>::SizeType)2].scalarI;
						if (fc.isProp && c.props.Size() >= 4)
							fc.prop = c.props[(NkVector<FbxProp>::SizeType)3].str;
						out.PushBack(static_cast<FbxConn &&>(fc));
					}
					break;
				}
			}

			// Parent OO de `childId` (ex. Model proprietaire d'une Geometry),
			// filtre sur les objets presents dans `candidates` (pour ignorer les
			// relations vers d'autres types quand plusieurs existent).
			int64 FindOwnerOO(const NkVector<FbxConn> &conns, int64 childId, const NkVector<FbxIdEntry> &candidates) {
				for (uint32 i = 0; i < (uint32)conns.Size(); ++i) {
					const FbxConn &c = conns[(NkVector<FbxConn>::SizeType)i];
					if (c.isProp || c.child != childId)
						continue;
					if (HasId(candidates, c.parent))
						return c.parent;
				}
				return -1;
			}

			// Tous les enfants OO de `parentId`, filtres sur `candidates`.
			void FindChildrenOO(const NkVector<FbxConn> &conns, int64 parentId, const NkVector<FbxIdEntry> &candidates,
								NkVector<int64> &out) {
				for (uint32 i = 0; i < (uint32)conns.Size(); ++i) {
					const FbxConn &c = conns[(NkVector<FbxConn>::SizeType)i];
					if (c.isProp || c.parent != parentId)
						continue;
					if (HasId(candidates, c.child))
						out.PushBack(c.child);
				}
			}

			// Texture connectee a la propriete nommee `propName` du materiau
			// `materialId` (ex. "DiffuseColor", "NormalMap", "Bump").
			int64 FindTextureForProp(const NkVector<FbxConn> &conns, int64 materialId, const char *propName,
									 const NkVector<FbxIdEntry> &textures) {
				for (uint32 i = 0; i < (uint32)conns.Size(); ++i) {
					const FbxConn &c = conns[(NkVector<FbxConn>::SizeType)i];
					if (!c.isProp || c.parent != materialId || !(c.prop == NkString(propName)))
						continue;
					if (HasId(textures, c.child))
						return c.child;
				}
				return -1;
			}

			// Properties70 : noeuds "P" { nom, type, label, flags, val1 [,val2,val3] }
			// (meme disposition que la lecture UpAxis dans FinishFBX). `outVals`
			// doit pouvoir contenir `minVals` float32.
			bool GetP70(const FbxNode &obj, const char *propName, int32 minVals, float32 *outVals) {
				const FbxNode *p70 = obj.Find("Properties70");
				if (!p70)
					return false;
				for (uint32 i = 0; i < (uint32)p70->children.Size(); ++i) {
					const FbxNode &pn = p70->children[(NkVector<FbxNode>::SizeType)i];
					if (!(pn.name == NkString("P")) || (int32)pn.props.Size() < 4 + minVals)
						continue;
					if (!(pn.props[0].str == NkString(propName)))
						continue;
					for (int32 v = 0; v < minVals; ++v)
						outVals[v] = (float32)pn.props[(NkVector<FbxProp>::SizeType)(4 + v)].scalar;
					return true;
				}
				return false;
			}

			// ════════════════════════════════════════════════════════════════
			//  Squelette / scene-graph FBX -> out.nodes (etape (a) du chantier
			//  « FBX operationnel », 2026-08-17).
			// ════════════════════════════════════════════════════════════════

			// Nom d'un objet FBX depuis sa 1re prop chaine.
			//   binaire : "Nom\0\x01Classe"  -> tronquer au premier NUL ;
			//   ASCII   : "Classe::Nom"      -> garder ce qui suit le dernier "::".
			NkString ObjNameOf(const FbxNode &n) {
				NkString raw = StrOf(&n);
				NkString out;
				for (nk_size i = 0; i < raw.Length(); ++i) {
					const char ch = raw.CStr()[i];
					if (ch == '\0')
						break;
					out.Append(ch);
				}
				for (nk_size p = out.Length(); p >= 2; --p) {
					if (out.CStr()[p - 1] == ':' && out.CStr()[p - 2] == ':') {
						out = NkString(out.CStr() + p);
						break;
					}
				}
				return out;
			}

			// Euler FBX (degres) -> quaternion (x,y,z,w). `order` = RotationOrder
			// FBX (0=XYZ 1=XZY 2=YZX 3=YXZ 4=ZXY 5=ZYX) : axes appliques de
			// gauche a droite, donc q = q_dernier * q_milieu * q_premier
			// (convention Hamilton, vecteurs colonne — celle de NkQuatT).
			NkVec4f EulerDegToQuat(float32 ex, float32 ey, float32 ez, int32 order) {
				const NkQuatf qx(NkAngle(ex), NkVec3f{1.f, 0.f, 0.f});
				const NkQuatf qy(NkAngle(ey), NkVec3f{0.f, 1.f, 0.f});
				const NkQuatf qz(NkAngle(ez), NkVec3f{0.f, 0.f, 1.f});
				NkQuatf q;
				switch (order) {
					case 1: q = qy * qz * qx; break; // XZY
					case 2: q = qx * qz * qy; break; // YZX
					case 3: q = qz * qx * qy; break; // YXZ
					case 4: q = qy * qx * qz; break; // ZXY
					case 5: q = qx * qy * qz; break; // ZYX
					default: q = qz * qy * qx; break; // 0=XYZ (et 6=SphericXYZ, approx)
				}
				q.Normalize();
				return {q.x, q.y, q.z, q.w};
			}

			// Construit out.nodes depuis les Model FBX : nom, TRS local
			// (Properties70 "Lcl Translation"/"Lcl Rotation"/"Lcl Scaling",
			// euler degres + RotationOrder + PreRotation composee), hierarchie
			// via les connexions OO Model -> Model. `outNodeOfModel[i]` = index
			// dans out.nodes du i-eme Model de `models` (meme ordre) — sert aux
			// etapes skinning/animations pour retrouver un node par ID d'objet.
			// NON fait ici (inchange par rapport a avant) : appliquer ces
			// transforms a la geometrie — les sommets restent en espace local
			// de leur Geometry, comme le header l'a toujours dit.
			void ExtractNodes(const NkVector<FbxIdEntry> &models, const NkVector<FbxConn> &conns,
							  NkGLTFMeshData &out, NkVector<int32> &outNodeOfModel) {
				outNodeOfModel.Clear();
				for (uint32 m = 0; m < (uint32)models.Size(); ++m) {
					const FbxNode &mo = *models[(NkVector<FbxIdEntry>::SizeType)m].node;
					NkGLTFNode gn;
					gn.name = ObjNameOf(mo);
					float32 v3[3];
					if (GetP70(mo, "Lcl Translation", 3, v3))
						gn.translation = {v3[0], v3[1], v3[2]};
					if (GetP70(mo, "Lcl Scaling", 3, v3))
						gn.scale = {v3[0], v3[1], v3[2]};
					float32 orderF = 0.f;
					GetP70(mo, "RotationOrder", 1, &orderF);
					NkVec4f rot = {0.f, 0.f, 0.f, 1.f};
					if (GetP70(mo, "Lcl Rotation", 3, v3))
						rot = EulerDegToQuat(v3[0], v3[1], v3[2], (int32)orderF);
					// PreRotation (toujours en ordre XYZ, cf. spec FBX) : se
					// compose DEVANT la rotation animable. Mixamo en depend.
					if (GetP70(mo, "PreRotation", 3, v3)) {
						const NkVec4f pre = EulerDegToQuat(v3[0], v3[1], v3[2], 0);
						const NkQuatf qp(pre.x, pre.y, pre.z, pre.w);
						const NkQuatf qr(rot.x, rot.y, rot.z, rot.w);
						NkQuatf q = qp * qr;
						q.Normalize();
						rot = {q.x, q.y, q.z, q.w};
					}
					gn.rotation = rot;
					outNodeOfModel.PushBack((int32)out.nodes.Size());
					out.nodes.PushBack(static_cast<NkGLTFNode &&>(gn));
				}
				// Hierarchie : connexion OO enfant -> parent entre deux Model.
				for (uint32 i = 0; i < (uint32)conns.Size(); ++i) {
					const FbxConn &c = conns[(NkVector<FbxConn>::SizeType)i];
					if (c.isProp)
						continue;
					int32 childIdx = -1, parentIdx = -1;
					for (uint32 m = 0; m < (uint32)models.Size(); ++m) {
						const int64 id = models[(NkVector<FbxIdEntry>::SizeType)m].id;
						if (id == c.child)
							childIdx = (int32)m;
						if (id == c.parent)
							parentIdx = (int32)m;
					}
					if (childIdx >= 0 && parentIdx >= 0 && childIdx != parentIdx)
						out.nodes[(uint32)outNodeOfModel[(NkVector<int32>::SizeType)parentIdx]]
							.children.PushBack(outNodeOfModel[(NkVector<int32>::SizeType)childIdx]);
				}
			}

			// Charge (avec cache par ID) la texture FBX `texId` -> index dans
			// out.images. `RelativeFilename`/`FileName` peuvent contenir un chemin
			// ABSOLU de la machine d'export (Windows) : on ne garde que le nom de
			// fichier, resolu par rapport au dossier du .fbx (comme les .bin glTF
			// externes, cf. ResolveBufferURI). Echec de chargement = warning +
			// slot invalide (jamais d'invention de pixels).
			int32 LoadFbxTexture(int64 texId, const NkVector<FbxIdEntry> &textures, const NkString &baseDir,
								 NkGLTFMeshData &out, NkVector<int64> &texIdCache, NkVector<int32> &texImgCache) {
				for (uint32 i = 0; i < (uint32)texIdCache.Size(); ++i)
					if (texIdCache[(NkVector<int64>::SizeType)i] == texId)
						return texImgCache[(NkVector<int32>::SizeType)i];
				const FbxNode *tex = FindById(textures, texId);
				NkString rel = tex ? StrOf(tex->Find("RelativeFilename")) : NkString("");
				if (rel.Empty() && tex)
					rel = StrOf(tex->Find("FileName"));
				int32 idx = -1;
				if (tex && !rel.Empty()) {
					NkString fname = rel;
					for (nk_size p = fname.Length(); p > 0; --p) {
						char ch = fname.CStr()[p - 1];
						if (ch == '/' || ch == '\\') {
							fname = NkString(fname.CStr() + p);
							break;
						}
					}
					NkString full = baseDir;
					if (!full.Empty() && !full.EndsWith('/') && !full.EndsWith('\\'))
						full.Append('/');
					full.Append(fname);
					NkGLTFImage img;
					img.uri = rel;
					if (img.decoded.Load(full.CStr(), 4))
						img.valid = img.decoded.IsValid();
					if (!img.valid)
						NkLog::Instance().Warnf("[NkFBXLoader] texture introuvable : %s (relatif='%s')",
												full.CStr(), rel.CStr());
					idx = (int32)out.images.Size();
					out.images.PushBack(static_cast<NkGLTFImage &&>(img));
				}
				texIdCache.PushBack(texId);
				texImgCache.PushBack(idx);
				return idx;
			}

			// Construit (avec cache par ID) un NkGLTFMaterial depuis un noeud
			// Material FBX (Properties70 : DiffuseColor/DiffuseFactor/
			// TransparencyFactor/EmissiveColor/EmissiveFactor/ShininessExponent) +
			// textures connectees (DiffuseColor/NormalMap ou Bump/EmissiveColor).
			// Le modele Phong FBX n'a pas de notion PBR metal/rugosite native :
			// non-metallique par defaut, rugosite approximee depuis l'exposant de
			// brillance Phong si present (heuristique grossiere assumee, PAS une
			// conversion physique — aucune meilleure donnee cote FBX classique).
			int32 LoadFbxMaterial(int64 matId, const NkVector<FbxIdEntry> &materials,
								  const NkVector<FbxIdEntry> &textures, const NkVector<FbxConn> &conns,
								  const NkString &baseDir, NkGLTFMeshData &out, NkVector<int64> &matIdCache,
								  NkVector<int32> &matIdxCache, NkVector<int64> &texIdCache,
								  NkVector<int32> &texImgCache) {
				for (uint32 i = 0; i < (uint32)matIdCache.Size(); ++i)
					if (matIdCache[(NkVector<int64>::SizeType)i] == matId)
						return matIdxCache[(NkVector<int32>::SizeType)i];
				const FbxNode *mat = FindById(materials, matId);
				int32 idx = -1;
				if (mat) {
					NkGLTFMaterial gm;
					gm.name = StrOf(mat);
					float32 rgb[3];
					float32 factor;
					if (GetP70(*mat, "DiffuseColor", 3, rgb))
						gm.baseColorFactor = {rgb[0], rgb[1], rgb[2], 1.f};
					if (GetP70(*mat, "DiffuseFactor", 1, &factor))
						gm.baseColorFactor = {gm.baseColorFactor.x * factor, gm.baseColorFactor.y * factor,
											 gm.baseColorFactor.z * factor, gm.baseColorFactor.w};
					if (GetP70(*mat, "TransparencyFactor", 1, &factor))
						gm.baseColorFactor.w = 1.f - factor;
					if (GetP70(*mat, "EmissiveColor", 3, rgb)) {
						gm.emissiveFactor = {rgb[0], rgb[1], rgb[2]};
						if (GetP70(*mat, "EmissiveFactor", 1, &factor))
							gm.emissiveFactor = {gm.emissiveFactor.x * factor, gm.emissiveFactor.y * factor,
												gm.emissiveFactor.z * factor};
					}
					gm.metallicFactor = 0.f;
					gm.roughnessFactor = 0.5f;
					float32 shininess;
					if (GetP70(*mat, "ShininessExponent", 1, &shininess) ||
						GetP70(*mat, "Shininess", 1, &shininess)) {
						float32 r = 1.f - shininess / 100.f;
						gm.roughnessFactor = r < 0.f ? 0.f : (r > 1.f ? 1.f : r);
					}
					int64 diffTex = FindTextureForProp(conns, matId, "DiffuseColor", textures);
					if (diffTex >= 0)
						gm.baseColorImage = LoadFbxTexture(diffTex, textures, baseDir, out, texIdCache, texImgCache);
					int64 normTex = FindTextureForProp(conns, matId, "NormalMap", textures);
					if (normTex < 0)
						normTex = FindTextureForProp(conns, matId, "Bump", textures);
					if (normTex >= 0)
						gm.normalImage = LoadFbxTexture(normTex, textures, baseDir, out, texIdCache, texImgCache);
					int64 emisTex = FindTextureForProp(conns, matId, "EmissiveColor", textures);
					if (emisTex >= 0)
						gm.emissiveImage = LoadFbxTexture(emisTex, textures, baseDir, out, texIdCache, texImgCache);
					idx = (int32)out.materials.Size();
					out.materials.PushBack(static_cast<NkGLTFMaterial &&>(gm));
				}
				matIdCache.PushBack(matId);
				matIdxCache.PushBack(idx);
				return idx;
			}

			// ── Extrait une Geometry FBX dans out (un sous-mesh). ─────────────
			void ExtractGeometry(const FbxNode &geo, NkGLTFMeshData &out) {
				const NkVector<float64> *verts = ArrF(geo.Find("Vertices"));
				const NkVector<int64> *pvi = ArrI(geo.Find("PolygonVertexIndex"));
				if (!verts || !pvi || verts->Size() < 3)
					return;

				// Normales
				const FbxNode *leN = geo.Find("LayerElementNormal");
				const NkVector<float64> *norms = leN ? ArrF(leN->Find("Normals")) : nullptr;
				const NkVector<int64> *nIdx = leN ? ArrI(leN->Find("NormalsIndex")) : nullptr;
				NkString nMap = leN ? StrOf(leN->Find("MappingInformationType")) : NkString("");
				NkString nRef = leN ? StrOf(leN->Find("ReferenceInformationType")) : NkString("");
				bool nByPV = !(nMap == NkString("ByControlPoint") || nMap == NkString("ByVertice") ||
							   nMap == NkString("ByVertex"));
				bool nIdxToDir = (nRef == NkString("IndexToDirect") || nRef == NkString("Index"));

				// UV
				const FbxNode *leU = geo.Find("LayerElementUV");
				const NkVector<float64> *uvs = leU ? ArrF(leU->Find("UV")) : nullptr;
				const NkVector<int64> *uIdx = leU ? ArrI(leU->Find("UVIndex")) : nullptr;
				NkString uMap = leU ? StrOf(leU->Find("MappingInformationType")) : NkString("");
				NkString uRef = leU ? StrOf(leU->Find("ReferenceInformationType")) : NkString("");
				bool uByPV = !(uMap == NkString("ByControlPoint") || uMap == NkString("ByVertice") ||
							   uMap == NkString("ByVertex"));
				bool uIdxToDir = (uRef == NkString("IndexToDirect") || uRef == NkString("Index"));

				uint32 subStart = (uint32)out.indices.Size();
				uint32 baseV = (uint32)out.vertices.Size();
				bool anyNormal = false;
				uint32 white = PackRGBA(255, 255, 255);

				auto ctrlCount = (int64)(verts->Size() / 3);

				// emet un vertex pour le coin (ctrl, pvPos) et renvoie son index local (depuis baseV)
				auto emit = [&](int64 ctrl, int64 pvPos) -> uint32 {
					NkVertex3D v{};
					if (ctrl >= 0 && ctrl < ctrlCount) {
						v.pos = {(float32)(*verts)[(NkVector<float64>::SizeType)(ctrl * 3 + 0)],
								 (float32)(*verts)[(NkVector<float64>::SizeType)(ctrl * 3 + 1)],
								 (float32)(*verts)[(NkVector<float64>::SizeType)(ctrl * 3 + 2)]};
					}
					// normale
					if (norms) {
						int64 ni = nByPV ? pvPos : ctrl;
						if (nIdxToDir && nIdx && ni >= 0 && ni < (int64)nIdx->Size())
							ni = (*nIdx)[(NkVector<int64>::SizeType)ni];
						if (ni >= 0 && ni * 3 + 2 < (int64)norms->Size()) {
							v.normal = {(float32)(*norms)[(NkVector<float64>::SizeType)(ni * 3 + 0)],
										(float32)(*norms)[(NkVector<float64>::SizeType)(ni * 3 + 1)],
										(float32)(*norms)[(NkVector<float64>::SizeType)(ni * 3 + 2)]};
							anyNormal = true;
						}
					}
					// uv
					if (uvs) {
						int64 ui = uByPV ? pvPos : ctrl;
						if (uIdxToDir && uIdx && ui >= 0 && ui < (int64)uIdx->Size())
							ui = (*uIdx)[(NkVector<int64>::SizeType)ui];
						if (ui >= 0 && ui * 2 + 1 < (int64)uvs->Size()) {
							float32 uu = (float32)(*uvs)[(NkVector<float64>::SizeType)(ui * 2 + 0)];
							float32 vv = (float32)(*uvs)[(NkVector<float64>::SizeType)(ui * 2 + 1)];
							v.uv = {uu, 1.f - vv}; // FBX origine bas-gauche -> flip V
						}
					}
					v.tangent = {0.f, 0.f, 0.f};
					v.uv2 = v.uv;
					v.color = white;
					out.vertices.PushBack(v);
					return (uint32)out.vertices.Size() - 1;
				};

				// parcourt PolygonVertexIndex, triangulation fan par polygone
				uint32 poly[256];
				int32 polyN = 0;
				for (uint32 k = 0; k < (uint32)pvi->Size(); ++k) {
					int64 raw = (*pvi)[(NkVector<int64>::SizeType)k];
					bool last = raw < 0;
					int64 ctrl = last ? (~raw) : raw; // dernier index = ~idx
					if (polyN < 256)
						poly[polyN++] = emit(ctrl, (int64)k);
					if (last) {
						for (int32 i = 1; i + 1 < polyN; ++i) {
							out.indices.PushBack(poly[0] - baseV);
							out.indices.PushBack(poly[i] - baseV);
							out.indices.PushBack(poly[i + 1] - baseV);
						}
						polyN = 0;
					}
				}

				uint32 idxCount = (uint32)out.indices.Size() - subStart;
				if (idxCount == 0)
					return;
				if (!anyNormal) {
					// normales lissees locales a ce sous-mesh
					for (uint32 vi = baseV; vi < (uint32)out.vertices.Size(); ++vi)
						out.vertices[vi].normal = {0.f, 0.f, 0.f};
					for (uint32 ii = subStart; ii + 2 < subStart + idxCount; ii += 3) {
						uint32 a = out.indices[ii] + baseV, b = out.indices[ii + 1] + baseV,
							   cc = out.indices[ii + 2] + baseV;
						NkVec3f p0 = out.vertices[a].pos, p1 = out.vertices[b].pos, p2 = out.vertices[cc].pos;
						NkVec3f e1 = {p1.x - p0.x, p1.y - p0.y, p1.z - p0.z};
						NkVec3f e2 = {p2.x - p0.x, p2.y - p0.y, p2.z - p0.z};
						NkVec3f n = {e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x};
						out.vertices[a].normal = {out.vertices[a].normal.x + n.x, out.vertices[a].normal.y + n.y,
												  out.vertices[a].normal.z + n.z};
						out.vertices[b].normal = {out.vertices[b].normal.x + n.x, out.vertices[b].normal.y + n.y,
												  out.vertices[b].normal.z + n.z};
						out.vertices[cc].normal = {out.vertices[cc].normal.x + n.x, out.vertices[cc].normal.y + n.y,
												   out.vertices[cc].normal.z + n.z};
					}
					for (uint32 vi = baseV; vi < (uint32)out.vertices.Size(); ++vi) {
						NkVec3f &n = out.vertices[vi].normal;
						float32 len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
						if (len > 1e-8f) {
							n.x /= len;
							n.y /= len;
							n.z /= len;
						} else
							n = {0.f, 1.f, 0.f};
					}
				}

				NkSubMesh sm;
				sm.firstIndex = subStart;
				sm.indexCount = idxCount;
				sm.baseVertex = baseV;
				out.subMeshes.PushBack(sm);
				out.subMeshMaterial.PushBack(-1);
			}

			// ════════════════════════════════════════════════════════════════
			//  FBX ASCII (texte) -> meme arbre FbxNode que le binaire.
			//  Syntaxe : Name: prop, prop, "string" { enfants }
			//            Vertices: *N { a: 1,2,3,... }   (arrays)
			// ════════════════════════════════════════════════════════════════
			void SkipWSC(const char *&p, const char *end) { // ws + ';' comment
				for (;;) {
					while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
						++p;
					if (p < end && *p == ';') {
						while (p < end && *p != '\n')
							++p;
						continue;
					}
					break;
				}
			}

			// Lit le nom d'un noeud (jusqu'a ':'). false si pas de ':'.
			bool ReadAName(const char *&p, const char *end, NkString &name) {
				SkipWSC(p, end);
				const char *s = p;
				while (p < end && *p != ':' && *p != '{' && *p != '}' && *p != '\n')
					++p;
				if (p >= end || *p != ':')
					return false;
				const char *e = p;
				while (e > s && (e[-1] == ' ' || e[-1] == '\t'))
					--e;
				name.Clear();
				if (e > s)
					name.Append(s, (NkString::SizeType)(e - s));
				++p; // consomme ':'
				return !name.Empty();
			}

			// Corps d'array `{ a: nombres }` : detecte float/int via '.'/'e'.
			void ReadArrayBody(const char *&p, const char *end, FbxProp &pr) {
				bool isFloat = false;
				for (const char *q = p; q < end && *q != '}'; ++q)
					if (*q == '.' || *q == 'e' || *q == 'E') {
						isFloat = true;
						break;
					}
				pr.type = isFloat ? 'd' : 'i';
				while (p < end && *p != '}') {
					while (p < end && *p != '}' && !((*p >= '0' && *p <= '9') || *p == '-' || *p == '+' || *p == '.'))
						++p;
					if (p >= end || *p == '}')
						break;
					if (isFloat)
						pr.arrF.PushBack(ParseDouble(p, end));
					else
						pr.arrI.PushBack(ParseInt64(p, end));
				}
			}

			bool ParseANode(const char *&p, const char *end, FbxNode &node) {
				if (!ReadAName(p, end, node.name))
					return false;
				bool hasArray = false;
				for (;;) {
					while (p < end && (*p == ' ' || *p == '\t' || *p == '\r'))
						++p;
					if (p >= end)
						break;
					char ch = *p;
					if (ch == '{' || ch == '}' || ch == '\n')
						break;
					if (ch == '"') {
						++p;
						const char *s = p;
						while (p < end && *p != '"')
							++p;
						FbxProp pr;
						pr.type = 'S';
						if (p > s)
							pr.str.Append(s, (NkString::SizeType)(p - s));
						if (p < end)
							++p;
						node.props.PushBack(static_cast<FbxProp &&>(pr));
					} else if (ch == '*') {
						++p;
						ParseInt64(p, end);
						hasArray = true; // compteur ignore
					} else if ((ch >= '0' && ch <= '9') || ch == '-' || ch == '+' || ch == '.') {
						FbxProp pr;
						// Entier (ID d'objet FBX potentiellement grand) vs flottant : detecte
						// AVANT de consommer, comme ReadArrayBody — un ID parse via
						// ParseDouble perdrait sa precision au-dela de 2^53.
						bool isFloat = false;
						for (const char *q = p;
							 q < end && *q != ',' && *q != '{' && *q != '\n' && *q != ' ' && *q != '\t'; ++q)
							if (*q == '.' || *q == 'e' || *q == 'E') {
								isFloat = true;
								break;
							}
						if (isFloat) {
							pr.type = 'D';
							pr.scalar = ParseDouble(p, end);
						} else {
							pr.type = 'L';
							pr.scalarI = ParseInt64(p, end);
							pr.scalar = (float64)pr.scalarI;
						}
						node.props.PushBack(static_cast<FbxProp &&>(pr));
					} else {
						const char *s = p;
						while (p < end && *p != ',' && *p != '{' && *p != '\n' && *p != ' ' && *p != '\t')
							++p;
						FbxProp pr;
						pr.type = 'S';
						if (p > s)
							pr.str.Append(s, (NkString::SizeType)(p - s));
						node.props.PushBack(static_cast<FbxProp &&>(pr));
					}
					while (p < end && (*p == ' ' || *p == '\t' || *p == '\r'))
						++p;
					if (p < end && *p == ',') {
						++p;
						continue;
					}
					break;
				}
				SkipWSC(p, end);
				if (p < end && *p == '{') {
					++p;
					if (hasArray) {
						FbxProp pr;
						ReadArrayBody(p, end, pr);
						node.props.PushBack(static_cast<FbxProp &&>(pr));
						SkipWSC(p, end);
						if (p < end && *p == '}')
							++p;
					} else {
						for (;;) {
							SkipWSC(p, end);
							if (p >= end)
								break;
							if (*p == '}') {
								++p;
								break;
							}
							const char *save = p;
							FbxNode child;
							if (!ParseANode(p, end, child)) {
								while (p < end && *p != '\n' && *p != '}')
									++p;
								if (p < end && *p == '\n')
									++p;
								if (p <= save && p < end)
									++p;
								continue;
							}
							node.children.PushBack(static_cast<FbxNode &&>(child));
							if (p <= save && p < end)
								++p;
						}
					}
				}
				return true;
			}

			void ParseAsciiRoots(const char *p, const char *end, NkVector<FbxNode> &roots) {
				for (;;) {
					SkipWSC(p, end);
					if (p >= end)
						break;
					if (*p == '}') {
						++p;
						continue;
					}
					const char *save = p;
					FbxNode n;
					if (!ParseANode(p, end, n)) {
						if (p < end)
							++p;
						continue;
					}
					if (!n.name.Empty())
						roots.PushBack(static_cast<FbxNode &&>(n));
					if (p <= save && p < end)
						++p;
				}
			}

			// ── Tronc commun (binaire ET ascii) : extrait la geometrie + UpAxis.
			bool FinishFBX(NkVector<FbxNode> &roots, uint32 version, bool ascii, NkGLTFMeshData &out,
						   const NkString &path) {
				uint32 geoCount = 0;
				// Passe 1 : indexe les objets par ID (Geometry/Model/Material/
				// Texture) — necessaire pour resoudre Geometry -> Model
				// proprietaire -> Material(s) via les Connections (passe 2).
				NkVector<FbxIdEntry> geometries, models, materials, textures;
				for (uint32 i = 0; i < (uint32)roots.Size(); ++i) {
					if (!(roots[(NkVector<FbxNode>::SizeType)i].name == NkString("Objects")))
						continue;
					const FbxNode &objs = roots[(NkVector<FbxNode>::SizeType)i];
					for (uint32 j = 0; j < (uint32)objs.children.Size(); ++j) {
						const FbxNode &ch = objs.children[(NkVector<FbxNode>::SizeType)j];
						const int64 id = FbxIdOf(ch);
						if (ch.name == NkString("Geometry"))
							geometries.PushBack(FbxIdEntry{id, &ch});
						else if (ch.name == NkString("Model"))
							models.PushBack(FbxIdEntry{id, &ch});
						else if (ch.name == NkString("Material"))
							materials.PushBack(FbxIdEntry{id, &ch});
						else if (ch.name == NkString("Texture"))
							textures.PushBack(FbxIdEntry{id, &ch});
					}
					break; // une seule section Objects
				}
				NkVector<FbxConn> conns;
				ParseConnections(roots, conns);
				// Squelette / scene-graph (etape (a)) : nodes nommes + TRS +
				// hierarchie. Ne touche pas a la geometrie.
				NkVector<int32> nodeOfModel;
				ExtractNodes(models, conns, out, nodeOfModel);
				NkPath fp(path);
				NkString baseDir = fp.GetDirectory();
				NkVector<int64> matIdCache, texIdCache;
				NkVector<int32> matIdxCache, texImgCache;

				// Passe 2 : geometrie + resolution materiau par sous-mesh (via le
				// Model proprietaire de la Geometry — 1er materiau connecte si
				// plusieurs, cf. note LoadFbxMaterial/limitation ci-dessus).
				for (uint32 g = 0; g < (uint32)geometries.Size(); ++g) {
					const FbxIdEntry &ge = geometries[(NkVector<FbxIdEntry>::SizeType)g];
					const uint32 smBefore = (uint32)out.subMeshes.Size();
					ExtractGeometry(*ge.node, out);
					++geoCount;
					if ((uint32)out.subMeshes.Size() <= smBefore)
						continue; // geometrie vide/invalide : rien ajoute
					const int64 modelId = FindOwnerOO(conns, ge.id, models);
					if (modelId < 0)
						continue;
					NkVector<int64> matIds;
					FindChildrenOO(conns, modelId, materials, matIds);
					if (matIds.Empty())
						continue;
					const int32 matIdx =
						LoadFbxMaterial(matIds[0], materials, textures, conns, baseDir, out, matIdCache,
										matIdxCache, texIdCache, texImgCache);
					if (matIdx >= 0)
						out.subMeshMaterial[(NkVector<int32>::SizeType)(out.subMeshMaterial.Size() - 1)] = matIdx;
				}
				if (out.vertices.Empty() || out.indices.Empty()) {
					NkLog::Instance().Warnf("[NkFBXLoader] aucune geometrie exploitable dans %s", path.CStr());
					return false;
				}
				// UpAxis : GlobalSettings/Properties70/P "UpAxis" (1=Y, 2=Z).
				int32 upAxis = 1;
				for (uint32 i = 0; i < (uint32)roots.Size(); ++i) {
					if (!(roots[(NkVector<FbxNode>::SizeType)i].name == NkString("GlobalSettings")))
						continue;
					const FbxNode *p70 = roots[(NkVector<FbxNode>::SizeType)i].Find("Properties70");
					if (!p70)
						break;
					for (uint32 j = 0; j < (uint32)p70->children.Size(); ++j) {
						const FbxNode &pn = p70->children[(NkVector<FbxNode>::SizeType)j];
						if (!(pn.name == NkString("P")) || pn.props.Size() < 5)
							continue;
						if (pn.props[0].str == NkString("UpAxis"))
							upAxis = (int32)pn.props[(NkVector<FbxProp>::SizeType)4].scalar;
					}
					break;
				}
				if (getenv("NK_FBX_ZUP"))
					upAxis = 2; // cf. header : geometrie Z-up mal etiquetee
				if (upAxis == 2) {
					for (uint32 i = 0; i < (uint32)out.vertices.Size(); ++i) {
						NkVertex3D &v = out.vertices[i];
						v.pos = {v.pos.x, v.pos.z, -v.pos.y};
						v.normal = {v.normal.x, v.normal.z, -v.normal.y};
					}
				}
				ComputeBounds(out);
				out.debugName = path;
				NkLog::Instance().Infof(
					"[NkFBXLoader] OK '%s' (%s v%u) : %u geometries, %u verts, %u indices, %u sous-meshes, "
					"%u materiaux, %u textures, %u nodes",
					path.CStr(), ascii ? "ascii" : "binaire", version, geoCount, (uint32)out.vertices.Size(),
					(uint32)out.indices.Size(), (uint32)out.subMeshes.Size(), (uint32)out.materials.Size(),
					(uint32)out.images.Size(), (uint32)out.nodes.Size());
				return true;
			}
		} // namespace

		bool LoadFBX(const NkString &path, NkGLTFMeshData &out) {
			NkVector<nk_uint8> buf = NkFile::ReadAllBytes(path.CStr());
			if (buf.Size() < 27) {
				NkLog::Instance().Warnf("[NkFBXLoader] fichier introuvable/trop court : %s", path.CStr());
				return false;
			}
			const uint8 *base = buf.Data();
			const char *magic = "Kaydara FBX Binary";
			bool isBinary = true;
			for (int32 i = 0; magic[i]; ++i) {
				if ((char)base[i] != magic[i]) {
					isBinary = false;
					break;
				}
			}

			NkVector<FbxNode> roots;
			uint32 version = 0;

			if (isBinary) {
				version = ReadU32LE(base + 23);
				bool is64 = (version >= 7500);
				Cur c{base + 27, base + buf.Size(), true};
				while (c.p + (is64 ? 25 : 13) <= c.end && c.ok) {
					const uint8 *save = c.p;
					FbxNode n;
					bool isNull = false;
					if (!ParseNode(c, base, is64, n, isNull))
						break;
					if (isNull)
						break;
					if (n.name.Empty() && n.children.Empty() && n.props.Empty())
						break;
					roots.PushBack(static_cast<FbxNode &&>(n));
					if (c.p <= save)
						break; // securite anti-boucle
				}
			} else {
				// FBX ASCII : commence par un commentaire ';' ou 'FBXHeaderExtension'.
				ParseAsciiRoots((const char *)base, (const char *)base + buf.Size(), roots);
				// Version : FBXHeaderExtension/FBXVersion (optionnel, pour le log).
				for (uint32 i = 0; i < (uint32)roots.Size(); ++i) {
					const FbxNode &r = roots[(NkVector<FbxNode>::SizeType)i];
					if (!(r.name == NkString("FBXHeaderExtension")))
						continue;
					const FbxNode *fv = r.Find("FBXVersion");
					if (fv && !fv->props.Empty())
						version = (uint32)fv->props[0].scalar;
					break;
				}
			}

			if (roots.Empty()) {
				NkLog::Instance().Warnf("[NkFBXLoader] arbre vide (ni binaire ni ascii valide) : %s", path.CStr());
				return false;
			}
			return FinishFBX(roots, version, !isBinary, out, path);
		}

	} // namespace renderer
} // namespace nkentseu
