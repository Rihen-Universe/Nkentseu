#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Skeleton/ — LA structure de squelette du moteur (topologie + repos, monde) ; tout le reste la reference par index.
// -----------------------------------------------------------------------------
// FICHIER: NKAnima/NkSkeletonDef.h
// DESCRIPTION: L'ACTIF SQUELETTE PARTAGE — LA structure de squelette du moteur.
//
//   NkBoneDef      — la definition d'un os, invariante entre les instances
//   NkSkeletonDef  — l'actif partage (l'equivalent d'un USkeleton)
//
// =============================================================================
//  UNE SEULE STRUCTURE (decision de Rodolf, 2026-09-04)
// =============================================================================
//  Il y en avait deux : celle-ci (matrices bind / inverse-bind, MONDE) et
//  `NkRetargetSkeleton` (parent + bindLocal + names + topo, LOCAL). Rodolf a
//  tranche contre la recommandation de les garder distinctes : « on les unifie
//  dans NkAnima car elle sera utile pour plusieurs systemes qui en auront
//  besoin. » `NkRetargetSkeleton` n'existe plus. Le reciblage consomme celle-ci.
//
//  LA CONVENTION DE REPOS EST ABSORBEE UNE FOIS, A L'IMPORT. La structure
//  stocke le MONDE (bindPose / inverseBindPose : c'est ce que la peau consomme).
//  Le LOCAL — ce dont le reciblage a besoin — se DERIVE :
//      local(j) = inverse(bindPose(parent(j))) * bindPose(j)
//  Il n'est jamais stocke : deux representations de la meme donnee finiraient
//  par diverger. Un importateur qui n'a que des locaux (FBX, chaine d'essai)
//  passe par FromLocalBind(), qui fait la FK UNE fois et remplit le monde.
//
//  Ce qui reste dans Noge, et pourquoi : `NkBonePose` (par-instance, anime
//  chaque image) et `NkSkeleton` (le composant, copiable par l'ECS). C'est la
//  frontiere qui a fait passer le composant de 77 064 a 88 octets.
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NKMath.h"

#include <cstring>

namespace nkentseu {
	namespace anim {

		// La DEFINITION d'un os — invariante entre toutes les instances.
		// ⚠️ Aucune pose ici : c'est elle qui rendait le partage impossible.
		struct NkBoneDef {
				static constexpr uint32 kMaxBoneNameLen = 64u;
				char name[kMaxBoneNameLen] = {};
				int32 parent = -1;										   // index du parent (-1 = racine)
				math::NkMat4f bindPose = math::NkMat4f::Identity();		   // pose de repos, MONDE
				math::NkMat4f inverseBindPose = math::NkMat4f::Identity(); // son inverse (peau)
		};

		// L'ACTIF PARTAGE. Les instances le referencent par NkSharedPtr ; le
		// modifier apres creation modifierait TOUTES les instances — c'est le
		// contrat d'un USkeleton, on ne le cache pas.
		// ── LA conversion pose locale -> monde (2026-09-04), en UN endroit ──────
		// world[j] = world[parent(j)] * local[j], dans l'ordre `topo` (parent avant
		// enfant). Six boucles la redisaient (clip, retarget, IK de Noge, editeur
		// x3) ; elles appellent ceci. Deux boutons, pour l'edition :
		//   skip[j]     : le monde de j est deja fixe (chaine IK, joint saisi) --
		//                 on ne le recalcule pas, ses enfants en heritent ;
		//   rootsFixed  : les racines gardent le monde deja present dans `world`
		//                 (l'editeur pose la racine lui-meme) au lieu de local[j].
		// `topo` nul ou vide : ordre d'index, en supposant parent avant enfant --
		// la convention des exporteurs, et l'ancien comportement de l'IK.
		template <class ParentOf>
		inline void NkForwardKinematicsCore(const uint32 *topo, uint32 n, ParentOf parentOf,
											const math::NkMat4f *local, math::NkMat4f *world,
											const bool *skip, bool rootsFixed) {
			for (uint32 oi = 0; oi < n; ++oi) {
				const uint32 j = topo ? topo[oi] : oi;
				if (j >= n || (skip && skip[j]))
					continue;
				const int32 p = parentOf(j);
				if (p >= 0 && (uint32)p < n)
					world[j] = world[(uint32)p] * local[j];
				else if (!rootsFixed)
					world[j] = local[j];
			}
		}
		// Forme « tableaux bruts » : pour les topologies que possede un clip ou un
		// editeur (jointParent / jointTopo), sans NkSkeletonDef sous la main.
		inline void NkForwardKinematics(const int32 *parent, const uint32 *topo, uint32 n,
										const math::NkMat4f *local, math::NkMat4f *world,
										const bool *skip = nullptr, bool rootsFixed = false) {
			NkForwardKinematicsCore(topo, n, [parent](uint32 j) { return parent[j]; }, local, world, skip,
									rootsFixed);
		}

		struct NkSkeletonDef {
				NkVector<NkBoneDef> bones; // dimensionne au reel, aucun plafond
				NkVector<uint32> topo;	   // ordre topologique (parent avant enfant), cf. BuildTopo
				char path[256] = {};	   // provenance (ex-skeletonPath)

				[[nodiscard]] uint32 Count() const noexcept {
					return (uint32)bones.Size();
				}

				[[nodiscard]] int32 FindBone(const char *name) const noexcept {
					for (uint32 i = 0; i < Count(); ++i)
						if (std::strcmp(bones[(NkVector<NkBoneDef>::SizeType)i].name, name) == 0)
							return static_cast<int32>(i);
					return -1;
				}

				// Pose locale (une NkMat4f par joint, Count() entrees) -> monde. Voir
				// NkForwardKinematics ; `topo` vide -> ordre d'index.
				void LocalToWorld(const math::NkMat4f *local, math::NkMat4f *world, const bool *skip = nullptr,
								  bool rootsFixed = false) const {
					const uint32 n = Count();
					const uint32 *order = ((uint32)topo.Size() == n && n > 0) ? topo.Data() : nullptr;
					NkForwardKinematicsCore(order, n, [this](uint32 j) { return Parent(j); }, local, world, skip,
											rootsFixed);
				}
				[[nodiscard]] int32 Parent(uint32 j) const noexcept {
					return j < Count() ? bones[(NkVector<NkBoneDef>::SizeType)j].parent : -1;
				}

				// ── Pose de repos ─────────────────────────────────────────────
				// MONDE : stocke, donc une lecture. Plus de FK par joint.
				[[nodiscard]] math::NkMat4f BindWorld(uint32 j) const noexcept {
					return j < Count() ? bones[(NkVector<NkBoneDef>::SizeType)j].bindPose : math::NkMat4f::Identity();
				}
				[[nodiscard]] math::NkVec3f BindWorldPos(uint32 j) const noexcept {
					return BindWorld(j) * math::NkVec3f{0.f, 0.f, 0.f};
				}
				// LOCAL : DERIVE, jamais stocke.  local = inverse(monde(parent)) * monde(j)
				// Pour une racine, local == monde.
				[[nodiscard]] math::NkMat4f BindLocal(uint32 j) const noexcept {
					if (j >= Count())
						return math::NkMat4f::Identity();
					const NkBoneDef &b = bones[(NkVector<NkBoneDef>::SizeType)j];
					if (b.parent < 0 || (uint32)b.parent >= Count())
						return b.bindPose;
					return bones[(NkVector<NkBoneDef>::SizeType)b.parent].inverseBindPose * b.bindPose;
				}

				// Hauteur de la pose de repos = amplitude verticale entre le joint le
				// plus bas et le plus haut. Ne suppose AUCUNE convention de nommage :
				// marche sur un quadrupede, un bras robotise ou un personnage.
				[[nodiscard]] float32 BindHeight() const noexcept {
					const uint32 n = Count();
					if (n == 0)
						return 0.f;
					float32 lo = 1e30f, hi = -1e30f;
					for (uint32 i = 0; i < n; ++i) {
						const float32 y = BindWorldPos(i).y;
						if (y < lo)
							lo = y;
						if (y > hi)
							hi = y;
					}
					const float32 h = hi - lo;
					return (h > 0.f) ? h : 0.f;
				}

				// Vecteur des parents (le format que NkAnimationClip::jointParent porte).
				[[nodiscard]] NkVector<int32> ParentVector() const {
					NkVector<int32> out;
					out.Resize((NkVector<int32>::SizeType)Count());
					for (uint32 i = 0; i < Count(); ++i)
						out[(NkVector<int32>::SizeType)i] = bones[(NkVector<NkBoneDef>::SizeType)i].parent;
					return out;
				}

				// Construit `topo` depuis les parents (parents avant enfants). Renvoie
				// false si le graphe a un CYCLE — auquel cas toute FK boucherait a
				// l'infini. C'est le seul filet que l'ancienne structure apportait ;
				// on ne le perd pas.
				bool BuildTopo() {
					const uint32 n = Count();
					topo.Clear();
					if (n == 0)
						return true;
					NkVector<uint8> done;
					done.Resize((NkVector<uint8>::SizeType)n);
					for (uint32 i = 0; i < n; ++i)
						done[(NkVector<uint8>::SizeType)i] = 0;
					uint32 emitted = 0, guard = 0;
					while (emitted < n && guard++ <= n) {
						const uint32 before = emitted;
						for (uint32 i = 0; i < n; ++i) {
							if (done[(NkVector<uint8>::SizeType)i])
								continue;
							const int32 p = Parent(i);
							if (p < 0 || (p < (int32)n && done[(NkVector<uint8>::SizeType)p])) {
								topo.PushBack(i);
								done[(NkVector<uint8>::SizeType)i] = 1;
								emitted++;
							}
						}
						if (emitted == before)
							return false; // cycle
					}
					return emitted == n;
				}

				// ── LA CONVERSION, A L'IMPORT, UNE FOIS ───────────────────────
				// Un importateur qui ne connait que des transforms LOCAUX de repos
				// (relatifs au parent) construit l'actif ici : FK une seule fois,
				// monde et inverse-monde remplis, topo construit. Renvoie false si la
				// hierarchie a un cycle (aucun actif n'est alors produit a moitie).
				// Les noms sont copies (tronques a kMaxBoneNameLen-1).
				static bool FromLocalBind(const NkVector<int32> &parents, const NkVector<math::NkMat4f> &localBind,
										  const NkVector<const char *> &names, NkSkeletonDef &out) {
					const uint32 n = (uint32)parents.Size();
					if ((uint32)localBind.Size() != n)
						return false;
					out.bones.Clear();
					out.bones.Resize((NkVector<NkBoneDef>::SizeType)n);
					for (uint32 i = 0; i < n; ++i) {
						NkBoneDef &b = out.bones[(NkVector<NkBoneDef>::SizeType)i];
						b.parent = parents[(NkVector<int32>::SizeType)i];
						const char *nm =
							(i < (uint32)names.Size()) ? names[(NkVector<const char *>::SizeType)i] : nullptr;
						if (nm) {
							std::strncpy(b.name, nm, NkBoneDef::kMaxBoneNameLen - 1);
							b.name[NkBoneDef::kMaxBoneNameLen - 1] = '\0';
						}
					}
					if (!out.BuildTopo()) {
						out.bones.Clear();
						return false;
					}
					// FK par LA conversion (2026-09-04) -- la meme que le clip, le reciblage,
					// l'IK et l'editeur ; si elle se trompe, le test 0 du reciblage rougit.
					NkVector<math::NkMat4f> world;
					world.Resize((NkVector<math::NkMat4f>::SizeType)n);
					NkForwardKinematics(parents.Data(), out.topo.Data(), n, localBind.Data(), world.Data());
					for (uint32 k = 0; k < n; ++k) {
						NkBoneDef &b = out.bones[(NkVector<NkBoneDef>::SizeType)k];
						b.bindPose = world[(NkVector<math::NkMat4f>::SizeType)k];
						b.inverseBindPose = b.bindPose.Inverse();
					}
					return true;
				}
		};

	} // namespace anim
} // namespace nkentseu
