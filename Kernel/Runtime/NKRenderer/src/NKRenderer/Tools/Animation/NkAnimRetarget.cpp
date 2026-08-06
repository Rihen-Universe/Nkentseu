// -----------------------------------------------------------------------------
// @File    NkAnimRetarget.cpp
// @Brief   Reciblage d'animation entre squelettes. Voir l'en-tete pour les regles.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NKRenderer/Tools/Animation/NkAnimRetarget.h"

namespace nkentseu {
	namespace renderer {

		// ── NkRetargetSkeleton ──────────────────────────────────────────────────
		NkMat4f NkRetargetSkeleton::BindWorld(uint32 j) const {
			// FK en REMONTANT la chaine : on compose les locaux du joint jusqu'a la
			// racine. Remonter plutot que descendre evite d'exiger `topo` — utile
			// justement quand on est en train de le construire.
			if (j >= Count())
				return NkMat4f::Identity();
			NkMat4f acc = bindLocal[j];
			int32 p = parent[j];
			uint32 guard = 0;
			while (p >= 0 && p < (int32)Count() && guard++ < 4096u) {
				acc = bindLocal[(uint32)p] * acc;
				p = parent[(uint32)p];
			}
			return acc;
		}

		NkVec3f NkRetargetSkeleton::BindWorldPos(uint32 j) const {
			// FK en remontant la chaine : on compose les locaux du joint jusqu'a la
			// racine. Remonter plutot que descendre evite d'exiger `topo` — utile
			// justement quand on est en train de le construire.
			return BindWorld(j) * NkVec3f{0.f, 0.f, 0.f};
		}

		float32 NkRetargetSkeleton::BindHeight() const {
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

		bool NkRetargetSkeleton::BuildTopo() {
			const uint32 n = Count();
			topo.Clear();
			if (n == 0)
				return true;
			NkVector<uint8> done;
			done.Resize(n);
			for (uint32 i = 0; i < n; ++i)
				done[i] = 0;
			// Passes successives : a chaque tour on emet les joints dont le parent est
			// deja emis. Si un tour n'emet RIEN alors qu'il reste des joints, il y a un
			// cycle — on le dit plutot que de boucler.
			uint32 emitted = 0, guard = 0;
			while (emitted < n && guard++ <= n) {
				uint32 before = emitted;
				for (uint32 i = 0; i < n; ++i) {
					if (done[i])
						continue;
					const int32 p = parent[i];
					if (p < 0 || (p < (int32)n && done[(uint32)p])) {
						topo.PushBack(i);
						done[i] = 1;
						emitted++;
					}
				}
				if (emitted == before)
					return false; // cycle
			}
			return emitted == n;
		}

		// ── Normalisation des noms ──────────────────────────────────────────────
		NkString NkAnimRetarget::NormalizeJointName(const NkString &raw) {
			const char *s = raw.CStr();
			if (!s)
				return NkString("");
			// Prefixe de rig : tout ce qui precede le DERNIER ':' est jete. Les
			// exportateurs en ajoutent systematiquement (« mixamorig:Hips »), et deux
			// rigs decrivant le meme squelette n'apparieraient rien sans cela.
			const char *colon = nullptr;
			for (const char *p = s; *p; ++p)
				if (*p == ':')
					colon = p;
			if (colon)
				s = colon + 1;
			NkString out;
			for (const char *p = s; *p; ++p) {
				const char c = *p;
				// Separateurs jetes : « Left_Arm », « left-arm » et « LeftArm » doivent
				// donner la meme chose, sinon l'appariement depend du gout de l'exportateur.
				if (c == ' ' || c == '_' || c == '-' || c == '.')
					continue;
				const char lower[2] = {(char)((c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c), 0};
				out += lower;
			}
			return out;
		}

		uint32 NkAnimRetarget::BuildMapByName(const NkRetargetSkeleton &src, const NkRetargetSkeleton &dst,
											  NkRetargetMap &out) {
			const uint32 ns = src.Count(), nd = dst.Count();
			out.targetToSource.Clear();
			out.targetToSource.Resize(nd);
			for (uint32 i = 0; i < nd; ++i)
				out.targetToSource[i] = -1;
			if (ns == 0 || nd == 0)
				return 0;
			NkVector<NkString> srcNorm;
			srcNorm.Resize(ns);
			for (uint32 i = 0; i < ns; ++i)
				srcNorm[i] = (i < (uint32)src.names.Size()) ? NormalizeJointName(src.names[i]) : NkString("");
			uint32 matched = 0;
			for (uint32 j = 0; j < nd; ++j) {
				const NkString dn =
					(j < (uint32)dst.names.Size()) ? NormalizeJointName(dst.names[j]) : NkString("");
				if (dn.Empty())
					continue;
				for (uint32 i = 0; i < ns; ++i) {
					if (srcNorm[i].Empty() || srcNorm[i] != dn)
						continue;
					out.targetToSource[j] = (int32)i;
					matched++;
					break;
				}
			}
			return matched;
		}

		float32 NkAnimRetarget::HeightRatio(const NkRetargetSkeleton &src, const NkRetargetSkeleton &dst) {
			const float32 hs = src.BindHeight(), hd = dst.BindHeight();
			// Un squelette degenere (tous les joints au meme endroit) donnerait une
			// division par zero et expedierait le personnage a l'infini. On rend 1 :
			// aucune mise a l'echelle vaut mieux qu'une mise a l'echelle absurde.
			if (hs <= 1e-6f || hd <= 1e-6f)
				return 1.f;
			return hd / hs;
		}

		// ── Reciblage d'une pose ────────────────────────────────────────────────
		bool NkAnimRetarget::RetargetPose(const NkRetargetSkeleton &src, const NkRetargetSkeleton &dst,
										  const NkRetargetMap &map, const NkVector<NkMat4f> &srcLocal,
										  NkVector<NkMat4f> &outLocal, const NkRetargetParams &p) {
			const uint32 ns = src.Count(), nd = dst.Count();
			if (nd == 0 || !map.Valid(nd))
				return false;
			if ((uint32)srcLocal.Size() < ns)
				return false;
			outLocal.Resize(nd);
			const float32 rs = (p.rootScale > 0.f) ? p.rootScale : HeightRatio(src, dst);
			for (uint32 j = 0; j < nd; ++j) {
				// REGLE 3 : un joint non apparie garde sa pose de REPOS. L'identite
				// l'effondrerait sur son parent, et les rigs different toujours par
				// quelques os — c'est la norme, pas l'exception.
				const int32 si = map.targetToSource[j];
				if (si < 0 || (uint32)si >= ns) {
					outLocal[j] = (j < (uint32)dst.bindLocal.Size()) ? dst.bindLocal[j] : NkMat4f::Identity();
					continue;
				}
				const NkMat4f &srcBind = src.bindLocal[(uint32)si];
				const NkMat4f &dstBind = dst.bindLocal[j];
				// REGLE 1 : on transfere l'ECART au repos, pas la transform absolue.
				//   delta  = repos_source⁻¹ × source_courante
				//   sortie = repos_cible × delta
				// Verifiable : source au repos -> delta = identite -> cible au repos.
				const NkMat4f delta = srcBind.Inverse() * srcLocal[(uint32)si];
				NkMat4f res = dstBind * delta;

				// REGLE 2 : la translation appartient au personnage, sauf pour la racine.
				const bool isRoot = (j < (uint32)dst.parent.Size()) && (dst.parent[j] < 0);
				if (!isRoot && !p.transferBoneTranslation) {
					// On remet la translation du REPOS : l'os garde sa longueur, seule
					// son orientation suit le mouvement.
					const NkVec3f keep = dstBind * NkVec3f{0.f, 0.f, 0.f};
					const NkVec3f cur = res * NkVec3f{0.f, 0.f, 0.f};
					res = NkMat4f::Translate(keep - cur) * res;
				} else if (isRoot && p.scaleRootTranslation) {
					// La racine se deplace dans le monde : sa translation est mise a
					// l'echelle du rapport de taille. Sans cela, un grand personnage fait
					// les pas d'un petit et patine.
					const NkVec3f bindT = dstBind * NkVec3f{0.f, 0.f, 0.f};
					const NkVec3f curT = res * NkVec3f{0.f, 0.f, 0.f};
					const NkVec3f moved = curT - bindT;
					res = NkMat4f::Translate(bindT + moved * rs - curT) * res;
				}
				outLocal[j] = res;
			}
			return true;
		}

		// ── Reciblage d'un clip ─────────────────────────────────────────────────
		bool NkAnimRetarget::RetargetClip(const NkAnimationClip &srcClip, const NkRetargetSkeleton &src,
										  const NkRetargetSkeleton &dst, const NkRetargetMap &map,
										  NkAnimationClip &outClip, const NkRetargetParams &p) {
			// Un clip deja converti en matrices de SKINNING a perdu la hierarchie : il
			// n'y a plus de transform local a recibler. On refuse plutot que de
			// produire une pose absurde a partir de donnees incompatibles.
			if (!srcClip.skeletalLocal)
				return false;
			const uint32 nd = dst.Count();
			if (nd == 0 || !map.Valid(nd))
				return false;

			outClip.name = srcClip.name;
			outClip.duration = srcClip.duration;
			outClip.fps = srcClip.fps;
			outClip.loop = srcClip.loop;
			outClip.skeletalLocal = true;
			outClip.boneCount = nd;
			outClip.jointParent = dst.parent;
			outClip.jointTopo = dst.topo;
			// L'inverseBind reste celui de la CIBLE : c'est son maillage qu'on va
			// deformer. Reprendre celui de la source deformerait le bon squelette avec
			// la mauvaise pose de reference.
			// L'inverseBind est celui de la CIBLE : c'est son maillage qu'on deformera.
			// On le DEDUIT de sa pose de repos (inverse de la position monde de repos)
			// plutot que de le laisser a l'identite : a l'identite, le skinning
			// appliquerait la pose complete au lieu de l'ecart au repos, et le
			// personnage exploserait des la premiere frame.
			outClip.jointInverseBind.Clear();
			outClip.jointInverseBind.Resize(nd);
			for (uint32 j = 0; j < nd; ++j)
				outClip.jointInverseBind[j] = dst.BindWorld(j).Inverse();

			// TOUS LES TEMPS DE CLES sont repris tels quels : le reciblage change la
			// POSE, jamais le RYTHME. Rechantillonner introduirait un flou temporel
			// que personne n'a demande.
			NkVector<float32> times;
			for (uint32 b = 0; b < (uint32)srcClip.boneTracks.Size(); ++b) {
				const auto &tr = srcClip.boneTracks[b];
				for (uint32 k = 0; k < tr.KeyCount(); ++k) {
					const float32 t = tr.GetKey(k).time;
					bool seen = false;
					for (uint32 q = 0; q < (uint32)times.Size(); ++q)
						if (times[q] <= t + 1e-6f && times[q] >= t - 1e-6f) {
							seen = true;
							break;
						}
					if (!seen)
						times.PushBack(t);
				}
			}
			if (times.Empty())
				times.PushBack(0.f);
			// Tri croissant (insertion : les pistes en portent peu, et l'ordre des
			// cles doit etre strictement monotone pour l'interpolation).
			for (uint32 i = 1; i < (uint32)times.Size(); ++i) {
				const float32 v = times[i];
				uint32 k = i;
				while (k > 0 && times[k - 1] > v) {
					times[k] = times[k - 1];
					k--;
				}
				times[k] = v;
			}

			outClip.boneTracks.Clear();
			outClip.boneTracks.Resize(nd);
			NkVector<NkMat4f> srcPose, dstPose;
			const uint32 ns = src.Count();
			srcPose.Resize(ns);
			for (uint32 ti = 0; ti < (uint32)times.Size(); ++ti) {
				const float32 t = times[ti];
				for (uint32 b = 0; b < ns; ++b) {
					srcPose[b] = (b < (uint32)srcClip.boneTracks.Size() && srcClip.boneTracks[b].KeyCount() > 0)
									 ? srcClip.boneTracks[b].Evaluate(t)
									 : ((b < (uint32)src.bindLocal.Size()) ? src.bindLocal[b] : NkMat4f::Identity());
				}
				if (!RetargetPose(src, dst, map, srcPose, dstPose, p))
					return false;
				for (uint32 j = 0; j < nd; ++j)
					outClip.boneTracks[j].AddKey(t, dstPose[j]);
			}
			return true;
		}

		// ── AUTO-TEST ───────────────────────────────────────────────────────────
		// Chaque cas est choisi pour qu'une implantation NAIVE y ECHOUE. Un test que
		// la version cassee passerait aussi ne prouve rien — lecon deja payee sur le
		// detecteur de conflits de raccourcis et sur l'ombrage par angle.
		namespace {
			// Squelette a 3 joints en chaine verticale : racine -> milieu -> bout.
			// `lift` = longueur de chaque segment, `tilt` = rotation de repos du
			// milieu (c'est elle qui simule la difference T-pose / A-pose).
			NkRetargetSkeleton MakeChain(float32 lift, float32 tiltDeg, const char *prefix) {
				NkRetargetSkeleton s;
				s.parent.PushBack(-1);
				s.parent.PushBack(0);
				s.parent.PushBack(1);
				const float32 a = tiltDeg * 3.14159265f / 180.f;
				NkMat4f rot = NkMat4f::RotationZ(NkAngle::FromRad(a));
				s.bindLocal.PushBack(NkMat4f::Identity());
				s.bindLocal.PushBack(NkMat4f::Translate({0.f, lift, 0.f}) * rot);
				s.bindLocal.PushBack(NkMat4f::Translate({0.f, lift, 0.f}));
				NkString n0(prefix), n1(prefix), n2(prefix);
				n0 += "Hips";
				n1 += "Spine";
				n2 += "Head";
				s.names.PushBack(n0);
				s.names.PushBack(n1);
				s.names.PushBack(n2);
				s.BuildTopo();
				return s;
			}
			bool Near(float32 a, float32 b, float32 eps = 1e-3f) {
				const float32 d = a - b;
				return (d < 0.f ? -d : d) <= eps;
			}
			bool NearV(const NkVec3f &a, const NkVec3f &b, float32 eps = 1e-3f) {
				return Near(a.x, b.x, eps) && Near(a.y, b.y, eps) && Near(a.z, b.z, eps);
			}
			// Position MONDE d'un joint pour une pose LOCALE donnee.
			NkVec3f WorldOf(const NkRetargetSkeleton &sk, const NkVector<NkMat4f> &local, uint32 j) {
				NkMat4f acc = local[j];
				int32 p = sk.parent[j];
				uint32 guard = 0;
				while (p >= 0 && guard++ < 4096u) {
					acc = local[(uint32)p] * acc;
					p = sk.parent[(uint32)p];
				}
				return acc * NkVec3f{0.f, 0.f, 0.f};
			}
		} // namespace

		bool NkAnimRetarget::SelfTest() {
			bool ok = true;

			// 1) APPARIEMENT PAR NOM malgre les prefixes d'exportateur et les
			//    separateurs. Sans normalisation, « mixamorig:Spine » et « spine »
			//    n'apparieraient RIEN — cas le plus courant en pratique.
			{
				NkRetargetSkeleton a = MakeChain(1.f, 0.f, "mixamorig:");
				NkRetargetSkeleton b = MakeChain(1.f, 0.f, "");
				b.names[1] = NkString("SPINE");	 // casse differente
				b.names[2] = NkString("He_ad");	 // souligne parasite
				NkRetargetMap m;
				ok = ok && (BuildMapByName(a, b, m) == 3);
			}

			// 2) IDENTITE : recibler un squelette sur LUI-MEME ne doit RIEN changer.
			//    C'est le garde-fou minimal ; s'il tombe, tout le reste est faux.
			{
				NkRetargetSkeleton a = MakeChain(1.f, 20.f, "");
				NkRetargetMap m;
				BuildMapByName(a, a, m);
				NkVector<NkMat4f> pose = a.bindLocal;
				pose[1] = pose[1] * NkMat4f::RotationZ(NkAngle::FromRad(0.5f));
				NkVector<NkMat4f> out;
				ok = ok && RetargetPose(a, a, m, pose, out);
				ok = ok && NearV(WorldOf(a, out, 2), WorldOf(a, pose, 2));
			}

			// 3) LE CAS QUI COMPTE — POSES DE REPOS DIFFERENTES.
			//    Source au repos plat, cible au repos inclinee de 30°. Si la source est
			//    a SA pose de repos, la cible doit rester a LA SIENNE. Une recopie
			//    naive du transform local imposerait le repos de la source a la cible :
			//    le bout du squelette cible se retrouverait a la verticale au lieu de
			//    rester incline. C'est exactement le bug « bras qui tombent » du
			//    reciblage naif.
			{
				NkRetargetSkeleton src = MakeChain(1.f, 0.f, "");
				NkRetargetSkeleton dst = MakeChain(1.f, 30.f, "");
				NkRetargetMap m;
				BuildMapByName(src, dst, m);
				NkVector<NkMat4f> out;
				ok = ok && RetargetPose(src, dst, m, src.bindLocal, out);
				// Le bout de la cible doit etre PILE a sa position de repos.
				ok = ok && NearV(WorldOf(dst, out, 2), dst.BindWorldPos(2));
				// Et il ne doit PAS etre a la position de repos de la source (sinon on
				// aurait recopie la pose de la source : le test ne prouverait rien).
				ok = ok && !NearV(dst.BindWorldPos(2), src.BindWorldPos(2), 1e-2f);
			}

			// 4) LES OS NE S'ETIRENT PAS. Cible deux fois plus grande : apres
			//    reciblage d'une pose animee, la LONGUEUR de chaque os de la cible doit
			//    rester la sienne. Transferer la translation la ramenerait a celle de
			//    la source — le personnage se disloquerait.
			{
				NkRetargetSkeleton src = MakeChain(1.f, 0.f, "");
				NkRetargetSkeleton dst = MakeChain(2.f, 0.f, "");
				NkRetargetMap m;
				BuildMapByName(src, dst, m);
				NkVector<NkMat4f> pose = src.bindLocal;
				pose[1] = pose[1] * NkMat4f::RotationZ(NkAngle::FromRad(0.7f));
				NkVector<NkMat4f> out;
				ok = ok && RetargetPose(src, dst, m, pose, out);
				const float32 len = (WorldOf(dst, out, 2) - WorldOf(dst, out, 1)).Len();
				ok = ok && Near(len, 2.f, 1e-2f); // l'os de la cible mesure 2, pas 1
			}

			// 5) LA RACINE SUIT L'ECHELLE. Cible deux fois plus grande, racine
			//    deplacee d'une unite : la cible doit avancer de DEUX. Sans ce facteur,
			//    un grand personnage ferait les pas d'un petit et patinerait.
			{
				NkRetargetSkeleton src = MakeChain(1.f, 0.f, "");
				NkRetargetSkeleton dst = MakeChain(2.f, 0.f, "");
				NkRetargetMap m;
				BuildMapByName(src, dst, m);
				NkVector<NkMat4f> pose = src.bindLocal;
				pose[0] = NkMat4f::Translate({1.f, 0.f, 0.f});
				NkVector<NkMat4f> out;
				ok = ok && RetargetPose(src, dst, m, pose, out);
				ok = ok && Near((out[0] * NkVec3f{0.f, 0.f, 0.f}).x, 2.f, 1e-2f);
				// Et desactivable : sur place, la racine ne doit pas etre amplifiee.
				NkRetargetParams np;
				np.scaleRootTranslation = false;
				NkVector<NkMat4f> out2;
				ok = ok && RetargetPose(src, dst, m, pose, out2, np);
				ok = ok && Near((out2[0] * NkVec3f{0.f, 0.f, 0.f}).x, 1.f, 1e-2f);
			}

			// 6) OS NON APPARIE -> POSE DE REPOS, jamais l'identite. Un joint laisse a
			//    l'identite s'effondrerait sur son parent ; les rigs different toujours
			//    par quelques os, ce cas est la norme.
			{
				NkRetargetSkeleton src = MakeChain(1.f, 0.f, "");
				NkRetargetSkeleton dst = MakeChain(1.f, 0.f, "");
				dst.names[2] = NkString("OsQuiNExistePasChezLaSource");
				NkRetargetMap m;
				ok = ok && (BuildMapByName(src, dst, m) == 2);
				NkVector<NkMat4f> out;
				ok = ok && RetargetPose(src, dst, m, src.bindLocal, out);
				ok = ok && NearV(out[2] * NkVec3f{0.f, 0.f, 0.f}, dst.bindLocal[2] * NkVec3f{0.f, 0.f, 0.f});
				// et surtout PAS l'identite (qui donnerait l'origine)
				ok = ok && !NearV(out[2] * NkVec3f{0.f, 0.f, 0.f}, NkVec3f{0.f, 0.f, 0.f});
			}

			// 7) CLIP ENTIER : les TEMPS de cles sont repris tels quels — le reciblage
			//    change la POSE, jamais le RYTHME. Et un clip deja converti en matrices
			//    de skinning doit etre REFUSE : la hierarchie y est perdue, on ne peut
			//    plus rien recibler.
			{
				NkRetargetSkeleton src = MakeChain(1.f, 0.f, "");
				NkRetargetSkeleton dst = MakeChain(2.f, 15.f, "");
				NkRetargetMap m;
				BuildMapByName(src, dst, m);
				NkAnimationClip clip;
				clip.skeletalLocal = true;
				clip.boneCount = 3;
				clip.duration = 1.f;
				clip.boneTracks.Resize(3);
				clip.boneTracks[1].AddKey(0.f, src.bindLocal[1]);
				clip.boneTracks[1].AddKey(0.5f, src.bindLocal[1] * NkMat4f::RotationZ(NkAngle::FromRad(0.4f)));
				clip.boneTracks[1].AddKey(1.f, src.bindLocal[1]);
				NkAnimationClip out;
				ok = ok && RetargetClip(clip, src, dst, m, out);
				ok = ok && (out.boneCount == 3) && (out.boneTracks.Size() == 3);
				ok = ok && (out.boneTracks[1].KeyCount() == 3);
				ok = ok && Near(out.boneTracks[1].GetKey(1).time, 0.5f);
				ok = ok && Near(out.duration, 1.f);
				// Refus d'un clip en matrices de skinning.
				NkAnimationClip skin = clip;
				skin.skeletalLocal = false;
				NkAnimationClip dummy;
				ok = ok && !RetargetClip(skin, src, dst, m, dummy);
			}

			// 8) GARDES. Cycle dans la hierarchie -> BuildTopo doit REFUSER plutot que
			//    boucler ; squelette degenere -> rapport 1 plutot qu'une division par
			//    zero qui expedierait le personnage a l'infini.
			{
				NkRetargetSkeleton bad;
				bad.parent.PushBack(1);
				bad.parent.PushBack(0);
				bad.bindLocal.PushBack(NkMat4f::Identity());
				bad.bindLocal.PushBack(NkMat4f::Identity());
				ok = ok && !bad.BuildTopo();
				NkRetargetSkeleton flat = MakeChain(0.f, 0.f, "");
				ok = ok && Near(HeightRatio(flat, flat), 1.f);
			}
			return ok;
		}

	} // namespace renderer
} // namespace nkentseu
