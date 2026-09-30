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
		/// SAISIR et COUTEAU agissent sur la matiere : ils servent en JEU.
		/// DEPLACER / TOURNER / ECHELLE (2026-09-29) : la selection, plus le
		/// gizmo du meme nom sur l'entite choisie (W / E / R, comme UE5).
		/// AJOUTES A LA FIN : les valeurs existantes ne bougent pas.
		enum class NkOutil : uint8 {
			NK_SELECTION = 0,
			NK_POSER,
			NK_EFFACER,
			NK_SAISIR,
			NK_COUTEAU,
			NK_DEPLACER,
			NK_TOURNER,
			NK_ECHELLE
		};

		/// L'outil choisit-il comme la Selection (clic = choisir, glisser = deplacer) ?
		inline bool NkOutilSelectionne(NkOutil o) noexcept {
			return o == NkOutil::NK_SELECTION || o == NkOutil::NK_DEPLACER || o == NkOutil::NK_TOURNER ||
				   o == NkOutil::NK_ECHELLE;
		}

		/// Les drapeaux d'EDITEUR d'une entite : l'oeil et le cadenas de l'Outliner
		/// (2026-09-30). Ils ne changent RIEN au jeu :
		///   cache   l'entite n'est pas dessinee dans la vue en EDITION (en jeu, le
		///           jeu montre tout -- l'oeil d'UE5 est celui de l'editeur, pas
		///           « cache en jeu ») ; elle ne se prend pas au clic ;
		///   verrou  elle ne se prend ni ne se deplace dans la vue ; l'Outliner et
		///           les Details la choisissent et la modifient toujours.
		/// Sauves avec la scene par NkScene::PhotographierAussi (objet « jeu » du
		/// fichier) : un ancien fichier, qui ne les porte pas, se relit tel quel.
		struct NkDrapeauxEditeur {
				bool cache = false;
				bool verrou = false;
		};

		/// Les etats d'UE5 : on EDITE une scene figee, on la JOUE, on la met en
		/// PAUSE. « Arreter » rend la scene d'avant le lancement (la photo).
		enum class NkEtatJeu : uint8 { NK_EDITION = 0, NK_JEU, NK_PAUSE };

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
				bool simuler = false; ///< --simuler : ouvrir directement en JEU

				NkEtatJeu etat = NkEtatJeu::NK_EDITION;
				/// La scene d'avant « Jouer ». Valide tant qu'on n'a pas « Arrete ».
				NkScene::NkPhoto photo;

				// --- Ce que « Poser » pose ------------------------------------
				/// Un acteur du catalogue de simulation (Unkeny/Simulation), ou —
				/// `acteurSimple` — l'entite elementaire d'origine (sprite + boite).
				NkActeurSim acteur = NkActeurSim::NK_BLOB;
				bool acteurSimple = false;
				/// Pinceau de fluide en cours (maintenir pour verser, en JEU).
				ecs::NkEntityId pinceau;
				uint32 graine = 20260929u;

				// --- Ressources ----------------------------------------------
				NkTextures2D textures;
				NkRessourcesSim ressources;

				// --- Affichage de la matiere -----------------------------------
				NkOptionsRenduParticules rendu;

				// --- Fichier ---------------------------------------------------
				NkString chemin;	 ///< vide = dossier de l'application / scene.nkscene
				NkString message;	 ///< derniere annonce (enregistre, erreur...)
				float32 messageAge = 99.f;

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
