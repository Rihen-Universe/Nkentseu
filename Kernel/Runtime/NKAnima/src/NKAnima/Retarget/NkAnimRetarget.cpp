// -----------------------------------------------------------------------------
// @File    NkAnimRetarget.cpp
// @Brief   Reciblage d'animation entre squelettes. Voir l'en-tete pour les regles.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NKAnima/Retarget/NkAnimRetarget.h"

namespace nkentseu {
	namespace anim {

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

		uint32 NkAnimRetarget::BuildMapByName(const NkSkeletonDef &src, const NkSkeletonDef &dst, NkRetargetMap &out) {
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
				srcNorm[i] = NormalizeJointName(NkString(src.bones[i].name));
			uint32 matched = 0;
			for (uint32 j = 0; j < nd; ++j) {
				const NkString dn = NormalizeJointName(NkString(dst.bones[j].name));
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

		float32 NkAnimRetarget::HeightRatio(const NkSkeletonDef &src, const NkSkeletonDef &dst) {
			const float32 hs = src.BindHeight(), hd = dst.BindHeight();
			// Un squelette degenere (tous les joints au meme endroit) donnerait une
			// division par zero et expedierait le personnage a l'infini. On rend 1 :
			// aucune mise a l'echelle vaut mieux qu'une mise a l'echelle absurde.
			if (hs <= 1e-6f || hd <= 1e-6f)
				return 1.f;
			return hd / hs;
		}

		// ── Reciblage d'une pose ────────────────────────────────────────────────
		bool NkAnimRetarget::RetargetPose(const NkSkeletonDef &src, const NkSkeletonDef &dst, const NkRetargetMap &map,
										  const NkVector<NkMat4f> &srcLocal, NkVector<NkMat4f> &outLocal,
										  const NkRetargetParams &p) {
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
					outLocal[j] = dst.BindLocal(j);
					continue;
				}
				// Le repos LOCAL se DERIVE de l'actif (monde) : une seule source de
				// verite, absorbee une fois a l'import.
				const NkMat4f srcBind = src.BindLocal((uint32)si);
				const NkMat4f dstBind = dst.BindLocal(j);
				// REGLE 1 : on transfere l'ECART au repos, pas la transform absolue.
				//   delta  = repos_source⁻¹ × source_courante
				//   sortie = repos_cible × delta
				// Verifiable : source au repos -> delta = identite -> cible au repos.
				const NkMat4f delta = srcBind.Inverse() * srcLocal[(uint32)si];
				NkMat4f res = dstBind * delta;

				// REGLE 2 : la translation appartient au personnage, sauf pour la racine.
				const bool isRoot = dst.Parent(j) < 0;
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
		bool NkAnimRetarget::RetargetClip(const NkAnimationClip &srcClip, const NkSkeletonDef &src,
										  const NkSkeletonDef &dst, const NkRetargetMap &map, NkAnimationClip &outClip,
										  const NkRetargetParams &p) {
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
			outClip.jointParent = dst.ParentVector();
			outClip.jointTopo = dst.topo;
			// L'inverseBind est celui de la CIBLE : c'est son maillage qu'on deformera.
			// L'actif le STOCKE deja (inverseBindPose) : plus rien a deduire, et une
			// seule source de verite pour la peau et le reciblage.
			outClip.jointInverseBind.Clear();
			outClip.jointInverseBind.Resize(nd);
			for (uint32 j = 0; j < nd; ++j)
				outClip.jointInverseBind[j] = dst.bones[j].inverseBindPose;

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
									 : src.BindLocal(b);
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
			//
			// Depuis l'unification : les LOCAUX sont convertis UNE fois, ici, par
			// NkSkeletonDef::FromLocalBind — exactement le chemin qu'un importateur
			// prendra. Le test 0 verifie que cette conversion se retourne.
			NkSkeletonDef MakeChain(float32 lift, float32 tiltDeg, const char *prefix, NkVector<NkMat4f> *outLocal = nullptr) {
				NkVector<int32> parents;
				parents.PushBack(-1);
				parents.PushBack(0);
				parents.PushBack(1);
				const float32 a = tiltDeg * 3.14159265f / 180.f;
				NkMat4f rot = NkMat4f::RotationZ(NkAngle::FromRad(a));
				NkVector<NkMat4f> local;
				local.PushBack(NkMat4f::Identity());
				local.PushBack(NkMat4f::Translate({0.f, lift, 0.f}) * rot);
				local.PushBack(NkMat4f::Translate({0.f, lift, 0.f}));
				NkString n0(prefix), n1(prefix), n2(prefix);
				n0 += "Hips";
				n1 += "Spine";
				n2 += "Head";
				NkVector<const char *> names;
				names.PushBack(n0.CStr());
				names.PushBack(n1.CStr());
				names.PushBack(n2.CStr());
				NkSkeletonDef s;
				NkSkeletonDef::FromLocalBind(parents, local, names, s);
				if (outLocal)
					*outLocal = local;
				return s;
			}
			bool Near(float32 a, float32 b, float32 eps = 1e-3f) {
				const float32 d = a - b;
				return (d < 0.f ? -d : d) <= eps;
			}
			bool NearV(const NkVec3f &a, const NkVec3f &b, float32 eps = 1e-3f) {
				return Near(a.x, b.x, eps) && Near(a.y, b.y, eps) && Near(a.z, b.z, eps);
			}
			bool NearM(const NkMat4f &a, const NkMat4f &b, float32 eps = 1e-4f) {
				for (int r = 0; r < 4; ++r)
					for (int c = 0; c < 4; ++c)
						if (!Near(a[r][c], b[r][c], eps))
							return false;
				return true;
			}
			// Position MONDE d'un joint pour une pose LOCALE donnee.
			NkVec3f WorldOf(const NkSkeletonDef &sk, const NkVector<NkMat4f> &local, uint32 j) {
				NkMat4f acc = local[j];
				int32 p = sk.Parent(j);
				uint32 guard = 0;
				while (p >= 0 && guard++ < 4096u) {
					acc = local[(uint32)p] * acc;
					p = sk.Parent((uint32)p);
				}
				return acc * NkVec3f{0.f, 0.f, 0.f};
			}
			// Pose de repos locale de tous les joints (ce que l'ancien `bindLocal` stockait).
			NkVector<NkMat4f> RestLocal(const NkSkeletonDef &sk) {
				NkVector<NkMat4f> out;
				for (uint32 j = 0; j < sk.Count(); ++j)
					out.PushBack(sk.BindLocal(j));
				return out;
			}
		} // namespace

		bool NkAnimRetarget::SelfTest() {
			bool ok = true;

			// 0) LA CONVERSION SE RETOURNE — le temoin de l'unification (2026-09-04).
			//    La convention de repos est absorbee UNE fois a l'import (locaux ->
			//    monde) ; le reciblage lit le local DERIVE. Sur une chaine dont le
			//    repos est INCLINE (30°, le cas ou la convention differe), chaque
			//    local derive doit redonner le local d'origine a epsilon pres, et
			//    l'inverseBind stocke doit etre l'inverse exact du monde. Sans cela,
			//    la conversion serait fausse ET silencieuse : tout le reste passerait
			//    sur une pose de repos deformee.
			{
				NkVector<NkMat4f> local;
				NkSkeletonDef s = MakeChain(1.f, 30.f, "", &local);
				ok = ok && (s.Count() == 3) && (s.topo.Size() == 3);
				for (uint32 j = 0; j < 3; ++j) {
					ok = ok && NearM(s.BindLocal(j), local[j]);
					ok = ok && NearM(s.BindWorld(j) * s.bones[j].inverseBindPose, NkMat4f::Identity());
				}
				// Et le monde n'est PAS le local : sinon la conversion n'aurait rien fait.
				ok = ok && !NearM(s.BindWorld(2), local[2]);
			}

			// 1) APPARIEMENT PAR NOM malgre les prefixes d'exportateur et les
			//    separateurs. Sans normalisation, « mixamorig:Spine » et « spine »
			//    n'apparieraient RIEN — cas le plus courant en pratique.
			{
				NkSkeletonDef a = MakeChain(1.f, 0.f, "mixamorig:");
				NkSkeletonDef b = MakeChain(1.f, 0.f, "");
				std::strncpy(b.bones[1].name, "SPINE", NkBoneDef::kMaxBoneNameLen - 1); // casse differente
				std::strncpy(b.bones[2].name, "He_ad", NkBoneDef::kMaxBoneNameLen - 1); // souligne parasite
				NkRetargetMap m;
				ok = ok && (BuildMapByName(a, b, m) == 3);
			}

			// 2) IDENTITE : recibler un squelette sur LUI-MEME ne doit RIEN changer.
			//    C'est le garde-fou minimal ; s'il tombe, tout le reste est faux.
			{
				NkSkeletonDef a = MakeChain(1.f, 20.f, "");
				NkRetargetMap m;
				BuildMapByName(a, a, m);
				NkVector<NkMat4f> pose = RestLocal(a);
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
			//    reciblage naif. — Memes attentes qu'avant l'unification, a epsilon :
			//    c'est le temoin « avant/apres » du reciblage lui-meme.
			{
				NkSkeletonDef src = MakeChain(1.f, 0.f, "");
				NkSkeletonDef dst = MakeChain(1.f, 30.f, "");
				NkRetargetMap m;
				BuildMapByName(src, dst, m);
				NkVector<NkMat4f> out;
				ok = ok && RetargetPose(src, dst, m, RestLocal(src), out);
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
				NkSkeletonDef src = MakeChain(1.f, 0.f, "");
				NkSkeletonDef dst = MakeChain(2.f, 0.f, "");
				NkRetargetMap m;
				BuildMapByName(src, dst, m);
				NkVector<NkMat4f> pose = RestLocal(src);
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
				NkSkeletonDef src = MakeChain(1.f, 0.f, "");
				NkSkeletonDef dst = MakeChain(2.f, 0.f, "");
				NkRetargetMap m;
				BuildMapByName(src, dst, m);
				NkVector<NkMat4f> pose = RestLocal(src);
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
				NkSkeletonDef src = MakeChain(1.f, 0.f, "");
				NkSkeletonDef dst = MakeChain(1.f, 0.f, "");
				std::strncpy(dst.bones[2].name, "OsQuiNExistePasChezLaSource", NkBoneDef::kMaxBoneNameLen - 1);
				NkRetargetMap m;
				ok = ok && (BuildMapByName(src, dst, m) == 2);
				NkVector<NkMat4f> out;
				ok = ok && RetargetPose(src, dst, m, RestLocal(src), out);
				ok = ok && NearV(out[2] * NkVec3f{0.f, 0.f, 0.f}, dst.BindLocal(2) * NkVec3f{0.f, 0.f, 0.f});
				// et surtout PAS l'identite (qui donnerait l'origine)
				ok = ok && !NearV(out[2] * NkVec3f{0.f, 0.f, 0.f}, NkVec3f{0.f, 0.f, 0.f});
			}

			// 7) CLIP ENTIER : les TEMPS de cles sont repris tels quels — le reciblage
			//    change la POSE, jamais le RYTHME. Et un clip deja converti en matrices
			//    de skinning doit etre REFUSE : la hierarchie y est perdue, on ne peut
			//    plus rien recibler.
			{
				NkSkeletonDef src = MakeChain(1.f, 0.f, "");
				NkSkeletonDef dst = MakeChain(2.f, 15.f, "");
				NkRetargetMap m;
				BuildMapByName(src, dst, m);
				NkAnimationClip clip;
				clip.skeletalLocal = true;
				clip.boneCount = 3;
				clip.duration = 1.f;
				clip.boneTracks.Resize(3);
				const NkMat4f b1 = src.BindLocal(1);
				clip.boneTracks[1].AddKey(0.f, b1);
				clip.boneTracks[1].AddKey(0.5f, b1 * NkMat4f::RotationZ(NkAngle::FromRad(0.4f)));
				clip.boneTracks[1].AddKey(1.f, b1);
				NkAnimationClip out;
				ok = ok && RetargetClip(clip, src, dst, m, out);
				ok = ok && (out.boneCount == 3) && (out.boneTracks.Size() == 3);
				ok = ok && (out.boneTracks[1].KeyCount() == 3);
				ok = ok && Near(out.boneTracks[1].GetKey(1).time, 0.5f);
				ok = ok && Near(out.duration, 1.f);
				// L'inverseBind du clip EST celui de l'actif cible (une seule source).
				ok = ok && NearM(out.jointInverseBind[2], dst.bones[2].inverseBindPose);
				// Refus d'un clip en matrices de skinning.
				NkAnimationClip skin = clip;
				skin.skeletalLocal = false;
				NkAnimationClip dummy;
				ok = ok && !RetargetClip(skin, src, dst, m, dummy);
			}

			// 8) GARDES. Cycle dans la hierarchie -> BuildTopo doit REFUSER plutot que
			//    boucler (et FromLocalBind ne doit produire aucun actif a moitie) ;
			//    squelette degenere -> rapport 1 plutot qu'une division par zero qui
			//    expedierait le personnage a l'infini.
			{
				NkVector<int32> badParents;
				badParents.PushBack(1);
				badParents.PushBack(0);
				NkVector<NkMat4f> badLocal;
				badLocal.PushBack(NkMat4f::Identity());
				badLocal.PushBack(NkMat4f::Identity());
				NkVector<const char *> noNames;
				NkSkeletonDef bad;
				ok = ok && !NkSkeletonDef::FromLocalBind(badParents, badLocal, noNames, bad);
				ok = ok && (bad.Count() == 0);
				NkSkeletonDef flat = MakeChain(0.f, 0.f, "");
				ok = ok && Near(HeightRatio(flat, flat), 1.f);
			}
			return ok;
		}

	} // namespace anim
} // namespace nkentseu
