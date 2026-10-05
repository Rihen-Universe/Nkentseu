// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkArmature.cpp — l'armature editable (voir NkArmature.h).
// =============================================================================
#include "NKAnima/Rig/NkArmature.h"
#include "NKAnima/Rig/NkRigMath.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace anim {

		using math::NkMat4f;
		using math::NkVec3f;
		using namespace rigm;

		namespace {
			constexpr float32 kEps = 1e-5f;

			/// Les paires de cotes reconnues dans un nom : la premiere moitie est la
			/// GAUCHE. Suffixes, prefixes, puis mots (dans cet ordre de priorite).
			struct Paire {
					const char *g;
					const char *d;
			};
			const Paire kSuffixes[] = {{".L", ".R"}, {".l", ".r"}, {"_L", "_R"}, {"_l", "_r"}, {"-L", "-R"}, {" L", " R"}};
			const Paire kPrefixes[] = {{"L_", "R_"}, {"l_", "r_"}, {"L.", "R."}};
			// Les infixes (« arm_joint_L_2 » de CesiumMan, « hand.L.001 ») avant les mots.
			const Paire kMots[] = {{"_L_", "_R_"}, {"_l_", "_r_"}, {".L.", ".R."}, {"-L-", "-R-"},
								   {"Left", "Right"}, {"left", "right"}, {"LEFT", "RIGHT"}, {"Gauche", "Droite"}, {"gauche", "droite"}, {"GAUCHE", "DROITE"}};

			bool FinitPar(const char *s, const char *f) {
				const usize ls = std::strlen(s), lf = std::strlen(f);
				return ls > lf && std::strcmp(s + ls - lf, f) == 0;
			}
			bool CommencePar(const char *s, const char *p) {
				const usize lp = std::strlen(p);
				return std::strlen(s) > lp && std::strncmp(s, p, lp) == 0;
			}

			/// Le nom miroir, et le cote du nom de depart (0 = aucun, 1 = gauche, 2 = droite).
			NkString Miroir(const char *nom, int32 &cote) {
				cote = 0;
				if (nom == nullptr || nom[0] == '\0') {
					return NkString();
				}
				const usize l = std::strlen(nom);
				for (const Paire &p : kSuffixes) {
					for (int32 k = 0; k < 2; ++k) {
						const char *de = k == 0 ? p.g : p.d;
						const char *vers = k == 0 ? p.d : p.g;
						if (FinitPar(nom, de)) {
							cote = k + 1;
							NkString r(nom, (usize)(l - std::strlen(de)));
							r.Append(vers);
							return r;
						}
					}
				}
				for (const Paire &p : kPrefixes) {
					for (int32 k = 0; k < 2; ++k) {
						const char *de = k == 0 ? p.g : p.d;
						const char *vers = k == 0 ? p.d : p.g;
						if (CommencePar(nom, de)) {
							cote = k + 1;
							NkString r(vers);
							r.Append(nom + std::strlen(de));
							return r;
						}
					}
				}
				for (const Paire &p : kMots) {
					for (int32 k = 0; k < 2; ++k) {
						const char *de = k == 0 ? p.g : p.d;
						const char *vers = k == 0 ? p.d : p.g;
						const char *trouve = std::strstr(nom, de);
						if (trouve != nullptr) {
							cote = k + 1;
							NkString r(nom, (usize)(trouve - nom));
							r.Append(vers);
							r.Append(trouve + std::strlen(de));
							return r;
						}
					}
				}
				return NkString();
			}

			/// Le repere d'un os par la convention de Blender (voir l'en-tete).
			void AxesOs(const NkVec3f &head, const NkVec3f &tail, float32 roll, NkVec3f &x, NkVec3f &y, NkVec3f &z) {
				y = Norm(Sub(tail, head));
				NkVec3f x0, z0;
				const float32 c = y.y;
				if (c > -0.99999f) {
					// R = la rotation minimale (0,1,0) -> y, appliquee a ex et ez :
					// R e = e c + v x e + v (v . e) / (1 + c), v = (0,1,0) x y.
					const NkVec3f v = V(y.z, 0.f, -y.x);
					const NkVec3f ex = V(1.f, 0.f, 0.f), ez = V(0.f, 0.f, 1.f);
					const float32 k = 1.f / (1.f + c);
					x0 = Add(Add(Mul(ex, c), Cross(v, ex)), Mul(v, Dot(v, ex) * k));
					z0 = Add(Add(Mul(ez, c), Cross(v, ez)), Mul(v, Dot(v, ez) * k));
				} else {
					// Os vers le BAS : un demi-tour autour de Z (le cas special de Blender).
					x0 = V(-1.f, 0.f, 0.f);
					z0 = V(0.f, 0.f, 1.f);
				}
				const float32 cr = std::cos(roll), sr = std::sin(roll);
				x = Sub(Mul(x0, cr), Mul(z0, sr));
				z = Add(Mul(x0, sr), Mul(z0, cr));
			}
		} // namespace

		// =====================================================================
		// LECTURE
		// =====================================================================
		int32 NkArmature::Find(const char *name) const {
			if (name == nullptr) {
				return -1;
			}
			for (uint32 i = 0; i < Count(); ++i) {
				if (std::strcmp(bones[i].name.CStr(), name) == 0) {
					return (int32)i;
				}
			}
			return -1;
		}

		int32 NkArmature::FindUid(uint32 uid) const {
			for (uint32 i = 0; i < Count(); ++i) {
				if (bones[i].uid == uid) {
					return (int32)i;
				}
			}
			return -1;
		}

		float32 NkArmature::Length(uint32 i) const {
			return i < Count() ? Dist(bones[i].head, bones[i].tail) : 0.f;
		}

		NkMat4f NkArmature::BoneMatrix(uint32 i) const {
			if (i >= Count()) {
				return NkMat4f::Identity();
			}
			const NkArmatureBone &b = bones[i];
			NkVec3f x, y, z;
			AxesOs(b.head, b.tail, b.roll, x, y, z);
			return Repere(x, y, z, b.head);
		}

		NkMat4f NkArmature::JointMatrix(uint32 i) const {
			if (i >= Count()) {
				return NkMat4f::Identity();
			}
			return BoneMatrix(i) * bones[i].jointOffset;
		}

		float32 NkArmature::EnvelopeRadius(uint32 i) const {
			if (i >= Count()) {
				return 0.f;
			}
			return bones[i].envelope > 0.f ? bones[i].envelope : Length(i) * 0.1f;
		}

		NkBoneSide NkArmature::SideOfName(const char *name) {
			int32 cote = 0;
			(void)Miroir(name, cote);
			return cote == 1 ? NkBoneSide::NK_BoneSide_Gauche : (cote == 2 ? NkBoneSide::NK_BoneSide_Droite : NkBoneSide::NK_BoneSide_Centre);
		}

		NkBoneSide NkArmature::Side(uint32 i) const {
			if (i >= Count()) {
				return NkBoneSide::NK_BoneSide_Centre;
			}
			const NkBoneSide parNom = SideOfName(bones[i].name.CStr());
			if (parNom != NkBoneSide::NK_BoneSide_Centre) {
				return parNom;
			}
			const float32 x = (bones[i].head.x + bones[i].tail.x) * 0.5f;
			const float32 tol = 1e-3f + Length(i) * 0.05f;
			return x > tol ? NkBoneSide::NK_BoneSide_Gauche : (x < -tol ? NkBoneSide::NK_BoneSide_Droite : NkBoneSide::NK_BoneSide_Centre);
		}

		NkString NkArmature::MirrorName(const char *name) {
			int32 cote = 0;
			return Miroir(name, cote);
		}

		bool NkArmature::IsAncestor(uint32 ancestor, uint32 i) const {
			int32 c = (int32)i;
			for (uint32 garde = 0; c >= 0 && garde <= Count(); ++garde) {
				if ((uint32)c == ancestor) {
					return true;
				}
				c = (uint32)c < Count() ? bones[(uint32)c].parent : -1;
			}
			return false;
		}

		bool NkArmature::Topo(NkVector<uint32> &out) const {
			const uint32 n = Count();
			out.Clear();
			NkVector<uint8> fait;
			fait.Resize(n, 0);
			uint32 emis = 0;
			for (uint32 tour = 0; emis < n && tour <= n; ++tour) {
				const uint32 avant = emis;
				for (uint32 i = 0; i < n; ++i) {
					if (fait[i]) {
						continue;
					}
					const int32 p = bones[i].parent;
					if (p < 0 || p >= (int32)n || fait[(uint32)p]) {
						out.PushBack(i);
						fait[i] = 1;
						++emis;
					}
				}
				if (emis == avant) {
					return false;
				}
			}
			return emis == n;
		}

		void NkArmature::RestJointWorld(NkVector<NkMat4f> &out) const {
			out.Resize(Count());
			for (uint32 i = 0; i < Count(); ++i) {
				out[i] = JointMatrix(i);
			}
		}

		NkMat4f NkArmature::RestJointLocal(uint32 i) const {
			if (i >= Count()) {
				return NkMat4f::Identity();
			}
			const int32 p = bones[i].parent;
			const NkMat4f j = JointMatrix(i);
			return (p >= 0 && (uint32)p < Count()) ? JointMatrix((uint32)p).Inverse() * j : j;
		}

		// =====================================================================
		// OPERATIONS
		// =====================================================================
		NkString NkArmature::UniqueName(const char *souhait, int32 sauf) const {
			NkString base(souhait != nullptr && souhait[0] != '\0' ? souhait : "Os");
			auto Pris = [&](const NkString &n) {
				for (uint32 i = 0; i < Count(); ++i) {
					if ((int32)i != sauf && bones[i].name == n) {
						return true;
					}
				}
				return false;
			};
			if (!Pris(base)) {
				return base;
			}
			// « Os.003 » -> on repart de « Os » (le suffixe numerique de Blender).
			NkString racine = base;
			const usize l = racine.Size();
			if (l > 4 && racine[(uint32)(l - 4)] == '.' && racine[(uint32)(l - 3)] >= '0' && racine[(uint32)(l - 3)] <= '9' &&
				racine[(uint32)(l - 2)] >= '0' && racine[(uint32)(l - 2)] <= '9' && racine[(uint32)(l - 1)] >= '0' &&
				racine[(uint32)(l - 1)] <= '9') {
				racine = NkString(base.CStr(), l - 4);
			}
			// Un nom de cote garde son suffixe de cote A LA FIN (« doigt.001.L ») : la
			// symetrie le relit.
			NkString cote;
			for (const Paire &p : kSuffixes) {
				for (int32 k = 0; k < 2 && cote.Empty(); ++k) {
					const char *s = k == 0 ? p.g : p.d;
					if (FinitPar(racine.CStr(), s)) {
						cote = NkString(s);
						racine = NkString(racine.CStr(), racine.Size() - std::strlen(s));
					}
				}
			}
			for (uint32 n = 1; n < 1000; ++n) {
				char suf[16];
				std::snprintf(suf, sizeof(suf), ".%03u", n);
				NkString essai = racine;
				essai.Append(suf);
				essai.Append(cote.CStr());
				if (!Pris(essai)) {
					return essai;
				}
			}
			return base;
		}

		int32 NkArmature::Add(const char *name, const NkVec3f &head, const NkVec3f &tail, int32 parent, bool connected) {
			if (parent >= (int32)Count()) {
				parent = -1;
			}
			NkArmatureBone b;
			b.name = UniqueName(name);
			b.uid = NewUid();
			b.parent = parent;
			b.head = head;
			b.tail = tail;
			b.connected = connected && parent >= 0;
			if (b.connected) {
				b.head = bones[(uint32)parent].tail;
			}
			if (Dist(b.head, b.tail) < kEps) {
				b.tail = rigm::Add(b.head, V(0.f, 0.1f, 0.f));
			}
			bones.PushBack(b);
			return (int32)Count() - 1;
		}

		int32 NkArmature::Extrude(uint32 i, const NkVec3f &tail) {
			if (i >= Count()) {
				return -1;
			}
			const NkArmatureBone src = bones[i];
			const int32 k = Add(src.name.CStr(), src.tail, tail, (int32)i, true);
			if (k >= 0) {
				bones[(uint32)k].deform = src.deform;
				bones[(uint32)k].group = src.group;
				bones[(uint32)k].segments = src.segments;
			}
			return k;
		}

		bool NkArmature::Subdivide(uint32 i, uint32 cuts) {
			if (i >= Count() || cuts == 0 || cuts > 64) {
				return false;
			}
			const NkArmatureBone src = bones[i];
			const uint32 n = cuts + 1;
			// Les enfants d'AVANT (les nouveaux os seront ajoutes apres eux).
			NkVector<uint32> enfants;
			for (uint32 k = 0; k < Count(); ++k) {
				if (bones[k].parent == (int32)i) {
					enfants.PushBack(k);
				}
			}
			bones[i].tail = Lerp(src.head, src.tail, 1.f / (float32)n);
			int32 precedent = (int32)i;
			for (uint32 s = 1; s < n; ++s) {
				const NkVec3f a = Lerp(src.head, src.tail, (float32)s / (float32)n);
				const NkVec3f b = Lerp(src.head, src.tail, (float32)(s + 1) / (float32)n);
				const int32 k = Add(src.name.CStr(), a, b, precedent, true);
				NkArmatureBone &nb = bones[(uint32)k];
				nb.roll = src.roll;
				nb.deform = src.deform;
				nb.group = src.group;
				nb.envelope = src.envelope;
				nb.segments = src.segments;
				nb.jointOffset = src.jointOffset; // meme direction, meme roulis : meme repere de joint
				precedent = k;
			}
			for (uint32 e = 0; e < (uint32)enfants.Size(); ++e) {
				bones[enfants[e]].parent = precedent;
			}
			return true;
		}

		bool NkArmature::Remove(uint32 i, NkVector<int32> *remap) {
			if (i >= Count()) {
				return false;
			}
			const int32 grandParent = bones[i].parent;
			for (uint32 k = 0; k < Count(); ++k) {
				if (bones[k].parent == (int32)i) {
					bones[k].parent = grandParent;
					bones[k].connected = false;
				}
			}
			if (remap != nullptr) {
				remap->Resize(Count());
				for (uint32 k = 0; k < Count(); ++k) {
					(*remap)[k] = k < i ? (int32)k : (k == i ? -1 : (int32)k - 1);
				}
			}
			bones.RemoveAt(i);
			for (uint32 k = 0; k < Count(); ++k) {
				if (bones[k].parent > (int32)i) {
					bones[k].parent -= 1;
				}
			}
			return true;
		}

		void NkArmature::SetHead(uint32 i, const NkVec3f &p) {
			if (i >= Count()) {
				return;
			}
			bones[i].head = p;
			const int32 par = bones[i].parent;
			if (bones[i].connected && par >= 0) {
				// La tete est COLLEE a la queue du parent : elle l'entraine, et avec
				// elle les tetes des freres connectes.
				bones[(uint32)par].tail = p;
				for (uint32 k = 0; k < Count(); ++k) {
					if (bones[k].parent == par && bones[k].connected) {
						bones[k].head = p;
					}
				}
			}
		}

		void NkArmature::SetTail(uint32 i, const NkVec3f &p) {
			if (i >= Count()) {
				return;
			}
			bones[i].tail = p;
			for (uint32 k = 0; k < Count(); ++k) {
				if (bones[k].parent == (int32)i && bones[k].connected) {
					bones[k].head = p;
				}
			}
		}

		void NkArmature::Translate(uint32 i, const NkVec3f &d) {
			if (i >= Count()) {
				return;
			}
			const NkVec3f h = rigm::Add(bones[i].head, d), t = rigm::Add(bones[i].tail, d);
			SetHead(i, h);
			SetTail(i, t);
		}

		void NkArmature::SetRoll(uint32 i, float32 radians) {
			if (i < Count()) {
				bones[i].roll = radians;
			}
		}

		bool NkArmature::SetParent(uint32 i, int32 parent, bool connected) {
			if (i >= Count() || parent >= (int32)Count()) {
				return false;
			}
			if (parent >= 0 && IsAncestor(i, (uint32)parent)) {
				return false; // un cycle : l'os deviendrait son propre ancetre
			}
			bones[i].parent = parent;
			bones[i].connected = connected && parent >= 0;
			if (bones[i].connected) {
				bones[i].head = bones[(uint32)parent].tail;
			}
			return true;
		}

		NkString NkArmature::Rename(uint32 i, const char *name) {
			if (i >= Count()) {
				return NkString();
			}
			bones[i].name = UniqueName(name, (int32)i);
			return bones[i].name;
		}

		int32 NkArmature::Symmetrize(uint32 i) {
			if (i >= Count()) {
				return -1;
			}
			const NkBoneSide cote = Side(i);
			if (cote == NkBoneSide::NK_BoneSide_Centre) {
				return -1;
			}
			NkString nomMiroir = MirrorName(bones[i].name.CStr());
			if (nomMiroir.Empty()) {
				// Un os de cote sans suffixe : il recoit le sien, son miroir l'autre.
				NkString n = bones[i].name;
				n.Append(cote == NkBoneSide::NK_BoneSide_Gauche ? ".L" : ".R");
				Rename(i, n.CStr());
				nomMiroir = MirrorName(bones[i].name.CStr());
			}
			const NkArmatureBone src = bones[i];
			int32 parentMiroir = src.parent;
			if (src.parent >= 0) {
				const NkString np = MirrorName(bones[(uint32)src.parent].name.CStr());
				const int32 pj = np.Empty() ? -1 : Find(np.CStr());
				if (pj >= 0) {
					parentMiroir = pj;
				}
			}
			int32 j = Find(nomMiroir.CStr());
			if (j < 0) {
				j = Add(nomMiroir.CStr(), MiroirX(src.head), MiroirX(src.tail), parentMiroir, false);
			} else if (j == (int32)i) {
				return -1;
			}
			NkArmatureBone &m = bones[(uint32)j];
			if (parentMiroir < 0 || !IsAncestor((uint32)j, (uint32)parentMiroir)) {
				m.parent = parentMiroir;
			}
			m.head = MiroirX(src.head);
			m.tail = MiroirX(src.tail);
			m.connected = src.connected && m.parent >= 0;
			m.roll = -src.roll;
			m.deform = src.deform;
			m.group = src.group;
			m.envelope = src.envelope;
			m.segments = src.segments;
			m.jointOffset = MiroirMatriceX(src.jointOffset);
			return j;
		}

		// =====================================================================
		// IMPORT DEPUIS DES JOINTS
		// =====================================================================
		NkArmature NkArmature::FromJoints(const NkMat4f *world, const int32 *parent, const NkString *names, uint32 n) {
			NkArmature a;
			if (world == nullptr || parent == nullptr || n == 0) {
				return a;
			}
			// L'echelle du squelette : sa plus grande etendue (pour les feuilles isolees).
			NkVec3f mn = Origine(world[0]), mx = mn;
			for (uint32 i = 1; i < n; ++i) {
				const NkVec3f p = Origine(world[i]);
				mn = V(p.x < mn.x ? p.x : mn.x, p.y < mn.y ? p.y : mn.y, p.z < mn.z ? p.z : mn.z);
				mx = V(p.x > mx.x ? p.x : mx.x, p.y > mx.y ? p.y : mx.y, p.z > mx.z ? p.z : mx.z);
			}
			const float32 echelle = Len(Sub(mx, mn)) > 1e-4f ? Len(Sub(mx, mn)) : 1.f;
			a.bones.Resize(n);
			for (uint32 i = 0; i < n; ++i) {
				NkArmatureBone &b = a.bones[i];
				b.uid = a.NewUid();
				b.parent = (parent[i] >= 0 && (uint32)parent[i] < n && parent[i] != (int32)i) ? parent[i] : -1;
				b.head = Origine(world[i]);
				char defaut[24];
				std::snprintf(defaut, sizeof(defaut), "os_%u", i);
				b.name = (names != nullptr && !names[i].Empty()) ? names[i] : NkString(defaut);
			}
			// Les QUEUES : vers les enfants.
			for (uint32 i = 0; i < n; ++i) {
				NkArmatureBone &b = a.bones[i];
				NkVector<uint32> enfants;
				for (uint32 k = 0; k < n; ++k) {
					if (a.bones[k].parent == (int32)i && Dist(a.bones[k].head, b.head) > echelle * 1e-4f) {
						enfants.PushBack(k);
					}
				}
				bool pose = false;
				if (enfants.Size() == 1) {
					b.tail = a.bones[enfants[0]].head;
					pose = true;
				} else if (enfants.Size() > 1) {
					// Plusieurs enfants (bassin -> colonne + cuisses, poitrine -> cou +
					// clavicules) : celui du MILIEU s'il y en a un et que les autres sont
					// de cote ; sinon leur moyenne (une main vers ses doigts).
					int32 milieu = -1, central = 0;
					float32 ecart = 0.f;
					for (uint32 e = 0; e < (uint32)enfants.Size(); ++e) {
						const float32 dx = std::fabs(a.bones[enfants[e]].head.x - b.head.x);
						ecart = dx > ecart ? dx : ecart;
					}
					for (uint32 e = 0; e < (uint32)enfants.Size(); ++e) {
						const float32 dx = std::fabs(a.bones[enfants[e]].head.x - b.head.x);
						if (dx < ecart * 0.25f) {
							milieu = (int32)enfants[e];
							++central;
						}
					}
					if (central == 1 && ecart > echelle * 1e-3f) {
						b.tail = a.bones[(uint32)milieu].head;
					} else {
						NkVec3f s = V(0.f, 0.f, 0.f);
						for (uint32 e = 0; e < (uint32)enfants.Size(); ++e) {
							s = rigm::Add(s, a.bones[enfants[e]].head);
						}
						b.tail = Mul(s, 1.f / (float32)enfants.Size());
					}
					pose = Dist(b.tail, b.head) > echelle * 1e-4f;
				}
				if (!pose) {
					// Une FEUILLE : prolongee dans la direction du parent, a la moitie
					// de sa longueur ; une racine isolee, le long de son axe Y.
					NkVec3f dir = Norm(AxeY(world[i]));
					float32 l = echelle * 0.05f;
					if (b.parent >= 0) {
						const NkVec3f ph = a.bones[(uint32)b.parent].head;
						if (Dist(ph, b.head) > echelle * 1e-4f) {
							dir = Norm(Sub(b.head, ph));
							l = Dist(ph, b.head) * 0.5f;
						}
					}
					l = l > echelle * 0.01f ? l : echelle * 0.01f;
					b.tail = rigm::Add(b.head, Mul(dir, l));
				}
			}
			// CONNECTE : la tete est sur la queue du parent.
			for (uint32 i = 0; i < n; ++i) {
				NkArmatureBone &b = a.bones[i];
				b.connected = b.parent >= 0 && Dist(b.head, a.bones[(uint32)b.parent].tail) < echelle * 1e-4f;
			}
			// ROULIS puis DECALAGE : JointMatrix(i) doit rendre world[i].
			for (uint32 i = 0; i < n; ++i) {
				NkArmatureBone &b = a.bones[i];
				NkVec3f x0, y, z0;
				AxesOs(b.head, b.tail, 0.f, x0, y, z0);
				// L'axe du joint le plus perpendiculaire a l'os, projete sur son plan.
				const NkVec3f jx = Norm(AxeX(world[i]), V(1.f, 0.f, 0.f));
				const NkVec3f jz = Norm(AxeZ(world[i]), V(0.f, 0.f, 1.f));
				const bool parX = std::fabs(Dot(jx, y)) <= std::fabs(Dot(jz, y));
				const NkVec3f cible = parX ? jx : jz;
				const NkVec3f proj = Sub(cible, Mul(y, Dot(cible, y)));
				float32 roll = 0.f;
				if (Len(proj) > 1e-4f) {
					const NkVec3f d = Norm(proj);
					const NkVec3f ref = parX ? x0 : z0; // aligne X sur X du joint (ou Z sur Z)
					// x = x0 cos r - z0 sin r est x0 tourne de +r autour de +Y (main
					// droite) : le roulis est l'angle signe de `ref` a `d` autour de Y.
					roll = std::atan2(Dot(Cross(ref, d), y), Dot(ref, d));
				}
				b.roll = roll;
				b.jointOffset = a.BoneMatrix(i).Inverse() * world[i];
			}
			return a;
		}

	} // namespace anim
} // namespace nkentseu
