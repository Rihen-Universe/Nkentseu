// =============================================================================
// NkEditeurModele.h — l'etat que TOUS les panneaux partagent
//
// A QUOI SERT CE FICHIER
//   Avec NKEditorKit, un panneau est un OBJET a lui (NkEditorPanel), et le shell
//   les dessine chacun a son tour. Ils ne se voient pas entre eux : la
//   hierarchie doit connaitre la selection que le viseur vient de changer, et
//   l'inspecteur doit lire la meme. Cet etat vit donc ICI, et chaque panneau en
//   recoit une reference.
//
// ⚠️ POURQUOI PAS DE POINTEURS ENTRE PANNEAUX
//   Un panneau qui tient un pointeur vers un autre fige l'ordre de creation et
//   casse des qu'on en ferme un. Le modele au centre, les panneaux en peripherie :
//   fermer l'inspecteur ne change rien pour le viseur.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - un etat lu par PLUSIEURS panneaux  -> ici
//   - un etat propre a UN panneau        -> membre de ce panneau
// =============================================================================
#pragma once

#include "Editeur/NkEditeurAppareils.h"
#include "Unkeny/Unkeny.h"

namespace nkentseu {
	namespace editeur {

		using namespace nkentseu::unkeny;

		/// L'outil courant. Il decide de ce que fait un clic dans le viseur.
		enum class NkOutil : uint8 { NK_SELECTION = 0, NK_POSER, NK_EFFACER };

		/// L'etat partage de l'editeur.
		struct NkEditeurModele {
				NkScene scene;
				NkCarteTuiles carte;

				NkOutil outil = NkOutil::NK_SELECTION;
				int32 profil = 0;
				bool paysage = false;

				/// La physique tourne-t-elle ?
				///
				/// ⚠️ FAUX PAR DEFAUT, ET C'EST DELIBERE. Un editeur qui simule en
				/// permanence ne permet pas de POSER quoi que ce soit : l'objet
				/// tombe avant qu'on ait lache le bouton.
				bool simuler = false;

				bool voirCollisionneurs = true;
				bool voirGrille = true;

				ecs::NkEntityId selection;
				bool aSelection = false;

				/// Deplacement en cours : le decalage entre le point saisi et le
				/// centre de l'entite. Sans lui, l'entite saute pour se centrer
				/// sous le curseur des le premier pixel de glissement.
				bool deplace = false;
				NkVec2f decalageSaisie{0.f, 0.f};

				/// Panoramique du viseur (clic dans le vide, ou bouton droit).
				bool panoramique = false;
				NkVec2f dernierPointeur{0.f, 0.f};

				NkStatsRendu stats;
				uint32 graine = 20260901u;

				/// Le theme des couleurs du VISEUR (celui d Unkeny, pas celui du
				/// kit). Le chrome de l editeur est peint par NKEditorKit ; ce
				/// theme-ci ne sert qu au contenu 2D dessine dans le viseur.
				NkTheme theme;

				/// Le profil effectif, rotation comprise.
				NkProfilAppareil ProfilCourant() const noexcept {
					return paysage ? NkTourner(NkProfil(profil)) : NkProfil(profil);
				}

				/// Le pointeur de selection attendu par les fonctions de dessin
				/// (`nullptr` quand rien n'est selectionne).
				const ecs::NkEntityId *SelectionOuNul() const noexcept {
					return aSelection ? &selection : nullptr;
				}
		};

	} // namespace editeur
} // namespace nkentseu
