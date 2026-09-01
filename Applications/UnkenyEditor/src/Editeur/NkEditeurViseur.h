// =============================================================================
// NkEditeurViseur.h — le viseur : la scene, la grille, la selection
//
// ⚠️ CE QUE LE VISEUR MONTRE EN PLUS DE LA SCENE, ET POURQUOI
//   L'aire d'APPAREIL SIMULE et sa ZONE SURE sont dessinees par-dessus. Sans
//   elles, l'editeur montre une scene sur un ecran de bureau et ne dit rien du
//   telephone — or c'est precisement la que les defauts se paient.
//   Ce depot a mesure onze defauts sur GemCrush dont SEPT etaient
//   structurellement invisibles depuis la machine de developpement.
// =============================================================================
#pragma once

#include "Editeur/NkEditeurAppareils.h"
#include "NKGui/Core/NkGuiContext.h"
#include "Unkeny/Unkeny.h"

namespace nkentseu {
	namespace editeur {

		using namespace nkentseu::unkeny;

		/// Dessine le viseur en entier. Rend les mesures du rendu, pour que le
		/// pied de page les affiche : sans compteur, « est-ce que le hors-champ
		/// fonctionne » n'a pas de reponse.
		NkStatsRendu NkDessinerViseur(nkgui::NkGuiDrawList &dl, NkScene &scene, const nkgui::NkRect &viseur,
									  const nkgui::NkRect &appareil, const NkTheme &th,
									  const NkProfilAppareil &profil, bool grille, bool collisionneurs,
									  const ecs::NkEntityId *selection);

		/// L'aire d'APPAREIL SIMULE, a l'interieur du viseur.
		///
		/// Elle garde le RAPPORT largeur/hauteur du profil, centree, avec une
		/// marge. C'est ce rapport qui permet de juger une mise en page mobile
		/// depuis un ecran de bureau -- l'etirer la rendrait mensongere.
		///
		/// ⚠️ Fonction LIBRE, et non plus une methode d'un objet de disposition :
		/// avec NKEditorKit, le viseur est un panneau ancre qui apprend son
		/// rectangle au moment ou il se dessine. Il n'y a plus de disposition
		/// calculee d'avance a qui poser la question.
		nkgui::NkRect NkAireAppareil(const nkgui::NkRect &viseur, const NkProfilAppareil &profil) noexcept;

		/// L'entite dont la boite contient ce point du MONDE. Rend aussi son
		/// centre, pour que l'appelant calcule le decalage de saisie.
		///
		/// ⚠️ On parcourt et on garde la DERNIERE trouvee : c'est celle qui est
		/// dessinee en dernier, donc celle du dessus. Prendre la premiere
		/// selectionnerait ce qui est CACHE — et l'utilisateur ne comprendrait
		/// pas pourquoi son clic attrape autre chose.
		bool NkEntiteSous(NkScene &scene, const NkVec2f &monde, ecs::NkEntityId &sortie, NkVec2f &centre);

	} // namespace editeur
} // namespace nkentseu
