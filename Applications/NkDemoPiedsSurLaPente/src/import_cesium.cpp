// =============================================================================
// @File    Applications/NkDemoPiedsSurLaPente/src/import_cesium.cpp
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   L'import de CesiumMan.glb, dans SA PROPRE unité de compilation.
//
// ⚠️ POURQUOI UNE TROISIÈME UNITÉ, ET CE N'EST PAS UN GOÛT D'ARCHITECTURE :
//    `NkSpan` est déclaré DEUX FOIS dans le dépôt —
//       Kernel/Foundation/NKContainers/src/NKContainers/Views/NkSpan.h
//       Kernel/Runtime/NKECS/src/NKECS/NkECSDefines.h
//    et `Noge/Anim/NkLocomotion.h:67` fait `using namespace ecs;` **dans un
//    en-tête**. Dès qu'un autre en-tête tire la version NKContainers — ce que
//    fait `Noge/IO/NkGLTFIO.h` — les deux deviennent visibles et le compilateur
//    rend une trentaine de « reference to 'NkSpan' is ambiguous ».
//
//    Le dépôt connaît déjà ce défaut (« using namespace dans les en-têtes ») ;
//    le corriger est un lot en soi, pas le mien. On l'ÉVITE en n'incluant jamais
//    `NkLocomotion.h` et `NkGLTFIO.h` dans le même fichier.
//
//    ⚠️ C'est le SECOND conflit d'en-têtes de cette démo, après NKCanvas contre
//       NKRenderer (voir scene.h). Trois unités pour trois mondes qui ne
//       peuvent pas se voir : ce n'est pas une élégance, c'est un symptôme.
//
// @License Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "scene.h"

#include "Noge/IO/NkGLTFIO.h"
#include "NKLogger/NkLog.h"

#include <cstdio>
#include <cstring>

namespace eprouvette {

	using namespace nkentseu;

	// -------------------------------------------------------------------------
	// Le montage CesiumMan : les os designes par leur NOM.
	//
	// ⚠️ AUCUNE TABLE DE CORRESPONDANCE EN DUR. Les noms d'os des glTF reels ne
	//    suivent aucune convention (`mixamorig:LeftUpLeg`, `thigh.L`,
	//    `leg_joint_L_1`...). Ces trois noms-ci sont ceux de CE fichier, lus
	//    dans CE fichier, et ils sont AFFICHES a l'ecran : on ne pretend pas
	//    qu'ils vaudraient pour un autre modele. *Un nom devine serait aussi
	//    faux qu'un indice devine.*
	// -------------------------------------------------------------------------
	// Le squelette importe, GARDE ici : l'appelant le lit par pointeur opaque.
	// Statique et non libere : la demo vit le temps du processus, et un
	// squelette de 19 os ne justifie pas une gestion de duree de vie.
	namespace {
		ecs::NkSkeleton *gSkel = nullptr;
	} // namespace

	void *SqueletteCesiumMan() noexcept {
		return gSkel;
	}

	bool ImporterCesiumMan(Import &out) noexcept {
		NkGLTFImporter imp;
		const NkGLTFScene sc = imp.Import("Resources/Models/CesiumMan/CesiumMan.glb");
		if (!sc.IsValid() || sc.skeletons.Empty()) {
			// Un refus se DIT, et il dit CE QUI MANQUE.
			std::snprintf(out.refus, sizeof(out.refus),
						  "import refuse : %s", sc.IsValid() ? "aucun squelette dans le fichier"
													  : imp.GetLastError().CStr());
			return false;
		}
		// On COPIE le squelette : `sc` meurt a la fin de cette fonction, et la
		// definition est un `NkSharedPtr` -- la copie la partage, elle ne la
		// duplique pas.
		if (gSkel == nullptr)
			gSkel = new ecs::NkSkeleton();
		*gSkel = sc.skeletons[0];
		const ecs::NkSkeleton &sk = *gSkel;
		out.charge = true;
		out.osTotal = (int)sk.BoneCount();

		// Combien d'os portent un nom ? C'est LA mesure du fil pose ce jour.
		const anim::NkSkeletonDef *def = sk.def.Get();
		if (def != nullptr)
			for (uint32 i = 0; i < def->Count(); ++i) {
				if (def->bones[(NkVector<anim::NkBoneDef>::SizeType)i].name[0] != '\0')
					++out.osNommes;
				if (def->bones[(NkVector<anim::NkBoneDef>::SizeType)i].parent >= 0)
					++out.osAvecParent;
			}

		// La jambe gauche de CE fichier, par son nom.
		const char *noms[3] = {"leg_joint_L_1", "leg_joint_L_2", "leg_joint_L_3"};
		OsNomme *cibles[3] = {&out.cuisse, &out.mollet, &out.pied};
		for (int k = 0; k < 3; ++k) {
			std::strncpy(cibles[k]->nom, noms[k], sizeof(cibles[k]->nom) - 1);
			cibles[k]->indice = (int)sk.FindBone(noms[k]);
		}

		// ⚠️ LE NEGATIF DU NOM : un nom qui n'existe pas doit rendre un REFUS,
		//    jamais l'os 0. Un repli silencieux sur l'indice zero plierait la
		//    racine en croyant plier le pied -- et ce serait vert.
		out.indiceAbsent = (int)sk.FindBone("os_qui_n_existe_pas");
		if (out.indiceAbsent < 0)
			std::snprintf(out.refus, sizeof(out.refus),
						  "nom absent -> refus nomme (FindBone rend %d, pas 0)", out.indiceAbsent);
		else
			std::snprintf(out.refus, sizeof(out.refus),
						  "DANGER : un nom absent rend l'os %d au lieu d'un refus", out.indiceAbsent);

		// ⚠️ ET LA LIMITE DU PONT, MESUREE : `NkFootIKSystem` lit
		//    `Pose(i).localPosition` COMME une position monde. Sur un squelette
		//    plat c'est vrai ; sur CesiumMan, dont les os ont un parent, c'est
		//    faux. On lit les DEUX et on laisse l'ecart parler.
		if (out.pied.indice >= 0 && def != nullptr) {
			out.piedLocalY = sk.Pose((uint32)out.pied.indice).localPosition.y;
			const uint32 n = def->Count();
			NkVector<math::NkMat4f> loc, mon;
			loc.Resize((NkVector<math::NkMat4f>::SizeType)n);
			mon.Resize((NkVector<math::NkMat4f>::SizeType)n);
			for (uint32 i = 0; i < n; ++i) {
				const ecs::NkBonePose &bp = sk.Pose(i);
				loc[(NkVector<math::NkMat4f>::SizeType)i] =
					math::NkMat4f::TRS(bp.localPosition, bp.localRotation, bp.localScale);
			}
			def->LocalToWorld(loc.Data(), mon.Data());
			out.piedMondeY = mon[(NkVector<math::NkMat4f>::SizeType)out.pied.indice][3][1];
		}
		return true;
	}

} // namespace eprouvette
