#pragma once
// -----------------------------------------------------------------------------
// @File    NkAnimRetarget.h
// @Brief   RECIBLAGE d'animation : rejouer un clip d'un squelette sur un AUTRE.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// PROBLEME RESOLU
//   Une animation est authoree pour UN squelette. La rejouer sur un autre
//   personnage — plus grand, aux proportions differentes, a la pose de repos
//   differente — donne une pose cassee si l'on se contente de recopier les
//   transforms locaux. C'est la brique qui manque pour reutiliser une
//   bibliotheque de mouvements sur plusieurs personnages, et elle etait
//   explicitement notee « reellement non commencee » dans la roadmap NkAnima.
//
// LES TROIS REGLES, ET POURQUOI CHACUNE
//
//   1. ON TRANSFERE LE DELTA PAR RAPPORT AU REPOS, PAS LE TRANSFORM ABSOLU.
//      Deux rigs n'ont pas la meme pose de repos : l'un en T, l'autre en A, ou
//      simplement des axes d'os orientes autrement. Recopier la rotation locale
//      absolue impose au personnage cible la pose de repos de la SOURCE — bras
//      qui tombent, epaules tordues. On transfere donc l'ECART a la pose de
//      repos :
//              cible_locale = repos_cible × (repos_source)⁻¹ × source_locale
//      Le personnage part de SA pose de repos et subit le meme mouvement.
//      Corollaire verifiable : si la source est exactement a son repos, la cible
//      reste exactement au sien.
//
//   2. LES ROTATIONS SE TRANSFERENT, LES TRANSLATIONS NON — SAUF LA RACINE.
//      La longueur des os appartient au personnage, pas au mouvement : copier la
//      translation d'un os disloquerait le squelette cible. Seule la RACINE se
//      deplace dans le monde, et sa translation est mise a l'echelle du rapport
//      de TAILLE des deux squelettes. Sans ce facteur, un personnage deux fois
//      plus grand fait les memes pas qu'un petit : il patine, ou il vole.
//
//   3. UN OS NON APPARIE GARDE SA POSE DE REPOS.
//      Il serait tentant de laisser sa transform a l'identite : ce serait
//      effondrer l'os sur son parent. Les rigs n'ont jamais exactement les memes
//      os (doigts, os de torsion, os d'accessoire) ; ce cas est la norme, pas
//      l'exception.
//
// CE QUE CE MODULE NE FAIT PAS (et pourquoi c'est assume)
//   • Pas de correction de plantage de pied (IK de verrouillage au sol) : cela
//     demande la geometrie du sol et un solveur IK par frame — c'est la couche
//     au-dessus, qui utilisera NkIKSystem, deja livre.
//   • Pas d'appariement automatique par ANALYSE de la morphologie. L'appariement
//     se fait par NOM, avec normalisation, ou a la main. Deviner la structure
//     d'un rig inconnu produirait des correspondances fausses sans le dire.
//   • Pas de correction de volume (epaules qui s'interpenetrent) : c'est du
//     post-traitement de pose, pas du reciblage.
//
// UNE SEULE STRUCTURE DE SQUELETTE (2026-09-04)
//   Le reciblage consomme `NkSkeletonDef` — LA structure du moteur, partagee
//   par l'ECS, la peau et les systemes a venir. L'ancienne `NkRetargetSkeleton`
//   (locaux + noms + topo) N'EXISTE PLUS : ce qu'elle stockait se derive de
//   l'actif (BindLocal), ce qu'elle apportait (BuildTopo avec detection de
//   cycle, BindHeight) vit sur l'actif. La convention de repos est absorbee UNE
//   fois, a l'import (NkSkeletonDef::FromLocalBind) — jamais dans une seconde
//   structure qui la porterait en parallele.
//
// CPU PUR, zero GPU : testable en headless, meme pendant un entrainement.
// -----------------------------------------------------------------------------

#include "NKMath/NKMath.h"
#include "NKAnima/NkAnimation.h"
#include "NKAnima/NkSkeletonDef.h"

#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace anim {

		// Table d'appariement : pour chaque joint CIBLE, l'indice du joint SOURCE
		// (-1 = non apparie). Orientee cible parce que c'est la cible qu'on remplit :
		// tout joint cible doit recevoir une valeur, meme quand il n'a pas de source.
		struct NkRetargetMap {
				NkVector<int32> targetToSource;

				bool Valid(uint32 targetCount) const {
					return (uint32)targetToSource.Size() == targetCount;
				}

				uint32 MappedCount() const {
					uint32 n = 0;
					for (uint32 i = 0; i < (uint32)targetToSource.Size(); ++i)
						if (targetToSource[i] >= 0)
							n++;
					return n;
				}
		};

		struct NkRetargetParams {
				// Met la translation de la RACINE a l'echelle du rapport de taille des
				// deux squelettes. Desactivable pour un mouvement sur place (idle), ou
				// quand la racine est deja exprimee dans une unite normalisee.
				bool scaleRootTranslation = true;
				// Facteur impose (<= 0 : deduit du rapport des hauteurs de repos).
				float32 rootScale = 0.f;
				// Transfere aussi la translation des joints NON racine. Faux par defaut,
				// et ce defaut est le bon : la longueur des os appartient au personnage.
				// A n'activer que pour un rig ou les translations sont animees a dessein
				// (machoire coulissante, os telescopique).
				bool transferBoneTranslation = false;
		};

		class NkAnimRetarget {
			public:
				// ── APPARIEMENT PAR NOM ─────────────────────────────────────────────
				// Compare les noms NORMALISES : minuscules, sans espaces, sans tirets ni
				// soulignes, et sans prefixe de rig (tout ce qui precede le dernier ':',
				// ce qui absorbe « mixamorig:Hips » -> « hips »). Sans cette
				// normalisation, deux rigs decrivant le meme squelette n'apparient rien.
				// Renvoie le nombre de joints apparies.
				static uint32 BuildMapByName(const NkSkeletonDef &src, const NkSkeletonDef &dst, NkRetargetMap &out);

				// Normalisation exposee : l'interface l'utilisera pour montrer POURQUOI
				// deux noms s'apparient (ou pas), au lieu de laisser l'utilisateur
				// deviner.
				static NkString NormalizeJointName(const NkString &raw);

				// ── RECIBLAGE D'UNE POSE ────────────────────────────────────────────
				// srcLocal : transforms LOCAUX du squelette source a un instant donne.
				// outLocal : dimensionne a dst.Count(), rempli pour TOUS les joints —
				//            les non apparies recoivent leur pose de REPOS.
				// Renvoie false si les tailles sont incoherentes.
				static bool RetargetPose(const NkSkeletonDef &src, const NkSkeletonDef &dst, const NkRetargetMap &map,
										 const NkVector<NkMat4f> &srcLocal, NkVector<NkMat4f> &outLocal,
										 const NkRetargetParams &p = NkRetargetParams{});

				// ── RECIBLAGE D'UN CLIP ENTIER ──────────────────────────────────────
				// Produit un clip joue sur `dst`. Le clip source DOIT etre en mode LOCAL
				// (`skeletalLocal`) : un clip deja converti en matrices de skinning a
				// perdu la hierarchie, on ne peut plus rien recibler a partir de lui.
				// Les cles sont reprises telles quelles (memes temps) : le reciblage
				// change la POSE, jamais le RYTHME.
				static bool RetargetClip(const NkAnimationClip &srcClip, const NkSkeletonDef &src,
										 const NkSkeletonDef &dst, const NkRetargetMap &map, NkAnimationClip &outClip,
										 const NkRetargetParams &p = NkRetargetParams{});

				// Rapport de taille entre deux squelettes (hauteur de repos cible /
				// source). 1 si l'une des deux est degeneree — plutot que de diviser par
				// zero et d'expedier le personnage a l'infini.
				static float32 HeightRatio(const NkSkeletonDef &src, const NkSkeletonDef &dst);

				// Auto-test headless (aucun GPU).
				static bool SelfTest();
		};

	} // namespace anim
} // namespace nkentseu
