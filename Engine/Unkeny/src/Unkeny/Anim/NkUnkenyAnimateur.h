// =============================================================================
// NkUnkenyAnimateur.h — la MACHINE A ETATS d'animation d'un personnage 2D
//
// A QUOI SERT CE FICHIER
//   Choisir QUEL clip de sprite jouer d'apres ce que fait le personnage : il
//   attend, il marche, il saute, il tombe. Le jeu pose des PARAMETRES
//   (« vitesse », « auSol », « saut »...) ; la machine a etats HIERARCHIQUE de
//   NKAnima (anim::NkAnimStateMachine) decide de l'etat ; l'etat designe un
//   clip du NkAnimSprite2D de la meme entite.
//
// ⚠️ UNKENY NE REECRIT PAS DE MACHINE A ETATS
//   Sous-machines, any-state, declencheurs, priorites, conditions combinees :
//   tout le CHOIX est celui de NKAnima. Ce fichier n'est qu'un PONT —
//   parametres du composant -> machine, etat de la machine -> clip de sprite.
//   Et NkAnimSprite2D reste tel quel : c'est toujours lui qui fait defiler les
//   images ; l'animateur ne fait que lui dire `Jouer(clip)`.
//
// ⚠️ UN MODELE, N PERSONNAGES — ET POURQUOI LE COMPOSANT RESTE DES DONNEES
//   Une NkAnimStateMachine est un OBJET (vecteurs, chaines). La ranger dans un
//   composant violerait la regle de NkUnkenyComposants.h : un composant se
//   copie bit a bit, se sauvegarde et s'inspecte. On separe donc :
//     - le MODELE : la definition (etats, transitions), UNE par nom, dans un
//       registre (NkEnregistrerModeleAnimateur) ;
//     - le COMPOSANT NkAnimateur2D : le nom du modele, les valeurs des
//       parametres, et l'ETAT D'EXECUTION de la machine (NkRuntime de NKAnima,
//       une valeur de taille fixe faite pour cela).
//   A chaque trame, le systeme charge l'etat d'un personnage dans le modele,
//   le fait avancer, puis le range. Dix gobelins = un modele, dix composants.
//   ⚠️ Consequence : un modele ne doit porter que des etats VIDES (ou des
//   clips) — un blend tree y serait partage par tous, horloge comprise.
//
// L'ETAT -> LE CLIP
//   L'ETIQUETTE d'un etat (NkAnimStateMachine::SetStateTag) est l'index du clip
//   dans le NkAnimSprite2D. L'animateur ne rejoue que quand l'ETAT CHANGE :
//   rappeler Jouer a chaque trame relancerait un clip UNE_FOIS termine (un
//   saut qui recommencerait en boucle en l'air).
//
// LE MODELE FOURNI : « plateforme »
//     Sol  (sous-machine, entree idle)    idle   -> clip 0
//                                         marche -> clip 1
//     Air  (sous-machine, entree saut)    saut   -> clip 2
//                                         chute  -> clip 3
//   Parametres : vitesse (reel, |vx|), auSol (bool, vrai par defaut),
//   saut (declencheur), vitesseVerticale (reel).
//     idle -> marche       vitesse > 0,1
//     marche -> idle       vitesse < 0,1
//     Sol -> Air/saut      saut                         (priorite 1)
//     Sol -> Air/chute     auSol faux                   (on quitte un bord)
//     saut -> chute        vitesseVerticale < 0
//     Air -> Sol/marche    auSol ET vy < 0,01 ET vitesse > 0,1   (priorite 1)
//     Air -> Sol           auSol ET vy < 0,01
//
// SAUVEGARDE
//   NkAnimateur2D est declare par NkScene::Init (PhotographierAussi) : il vit
//   dans les photos (Jouer / Arreter) et dans les .nkscene, sous le nom
//   « NkAnimateur2D ». Un fichier ecrit avant le 2026-09-29 n'a pas cette cle :
//   il se relit tel quel. Les MODELES ne sont pas dans la scene : ce sont des
//   definitions, enregistrees par le jeu ou lues d'un .nkanimctl (le fichier
//   de « controleur d'animation » de NKAnima, NkChargerModeleAnimateur).
//   ⚠️ Comme tout composant photographie, il est ecrit EN OCTETS : changer sa
//   taille rend les sauvegardes precedentes muettes sur lui (relues sans lui,
//   pas corrompues — NkChargerScene compare la taille).
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - un parametre de plus  -> NK_UNKENY_ANIM_PARAMS_MAX (change la taille !)
//   - un modele fourni      -> le .cpp, a cote de « plateforme », ou chez le jeu
//   - un fondu de POSE      -> pas ici : une image de sprite ne se fond pas
// =============================================================================
#pragma once

#include "NKAnima/Blend/NkAnimMix.h"
#include "NKAnima/Clip/NkAnimation.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace ecs {
		class NkWorld;
	}
	namespace unkeny {
		class NkScene;

		/// Huit parametres couvrent un personnage (vitesse, sol, saut, chute,
		/// attaque, touche, mort, accroupi).
		static const int32 NK_UNKENY_ANIM_PARAMS_MAX = 8;
		/// Longueur d'un nom de parametre, zero final COMPRIS.
		static const int32 NK_UNKENY_ANIM_NOM_MAX = 20;
		/// Longueur d'un nom de modele, zero final compris.
		static const int32 NK_UNKENY_ANIM_MODELE_MAX = 24;

		enum class NkGenreParamAnim : uint8 {
			NK_BOOL = 0,   ///< 0 ou 1
			NK_REEL,	   ///< un nombre
			NK_DECLENCHEUR ///< pose par Declencher, remis a 0 par la machine quand une transition le CONSOMME
		};

		struct NkParamAnimateur2D {
				char nom[NK_UNKENY_ANIM_NOM_MAX] = {};
				NkGenreParamAnim genre = NkGenreParamAnim::NK_REEL;
				float32 valeur = 0.f;
		};

		/// Le composant. DONNEES SEULEMENT (voir l'en-tete).
		struct NkAnimateur2D {
				char modele[NK_UNKENY_ANIM_MODELE_MAX] = {};
				NkParamAnimateur2D params[NK_UNKENY_ANIM_PARAMS_MAX];
				uint8 nbParams = 0;
				bool enPause = false;
				/// Rempli par le systeme : le modele n'est pas enregistre. Sert a
				/// ne le DIRE qu'une fois, et a l'inspecteur.
				bool modeleAbsent = false;
				/// ⚠️ Ne pas l'ecrire a la main : c'est l'etat de la machine,
				/// range par le systeme. `execution.current` = l'etat courant
				/// (index dans le modele), -1 = pas encore demarre.
				anim::NkAnimStateMachine::NkRuntime execution;

				/// Un parametre reel. Ajoute s'il manque (dans la limite de
				/// NK_UNKENY_ANIM_PARAMS_MAX) ; rend false s'il n'y a plus de place.
				bool Poser(const char *nom, float32 v) noexcept;
				bool PoserBool(const char *nom, bool v) noexcept;
				/// Un declencheur : il reste pose jusqu'a ce qu'une transition
				/// le consomme. « saut » pose pendant un fondu n'est pas perdu.
				bool Declencher(const char *nom) noexcept;
				/// La valeur d'un parametre (bool : 0 ou 1), 0 s'il manque.
				float32 Valeur(const char *nom) const noexcept;
				/// L'index du parametre, -1 s'il manque.
				int32 Parametre(const char *nom) const noexcept;
		};

		/// Un animateur pret a l'emploi : le nom du modele, et SES parametres
		/// avec leurs valeurs par defaut (l'inspecteur les montre des la
		/// creation). Modele inconnu : aucun parametre, et le systeme le dira.
		NkAnimateur2D NkCreerAnimateur2D(const char *modele);

		// --- Le registre des modeles ---------------------------------------
		/// Enregistre (ou REMPLACE) un modele. La machine est COPIEE : l'appelant
		/// garde la sienne. Les entites en cours gardent leur etat ; un etat que
		/// le nouveau modele n'a plus repart de l'entree (NkRuntime le garantit).
		bool NkEnregistrerModeleAnimateur(const char *nom, const anim::NkAnimStateMachine &machine);
		/// Lit un modele d'un .nkanimctl et l'enregistre. Un .nkanim v3 de machine
		/// (ecrit du 29 au 30/09) se lit aussi ; un .nkanim de CLIP est refuse.
		/// L'extension a donner au fichier : NkAssetExtensionFor(
		/// NkAssetType::AnimationController) — le fichier, lui, se reconnait a
		/// son magic, pas a son nom.
		bool NkChargerModeleAnimateur(const char *nom, const char *chemin);
		/// Le modele, ou nul. « plateforme » existe toujours.
		anim::NkAnimStateMachine *NkModeleAnimateur(const char *nom);
		uint32 NkNbModelesAnimateur();
		const char *NkNomModeleAnimateur(uint32 i);

		/// « Sol/marche » : le chemin de l'etat courant, pour un inspecteur ou
		/// un journal. Vide si le modele manque ou si rien n'a demarre.
		NkString NkEtatAnimateur2D(const NkAnimateur2D &a);

		// --- (2026-10-01 soir) LE MELANGE ------------------------------------
		// Un modele est desormais un CONTROLEUR de NKAnima (anim::NkAnimController,
		// Blend/NkAnimMix.h) : sa machine de base (celle d'avant, toujours rendue
		// par NkModeleAnimateur), des COUCHES (une machine, un poids, un masque
		// d'objets, remplace ou additif), des ARBRES DE MELANGE 1D / 2D et des
		// MASQUES nommes. Le .nkanimctl les porte (sections que l'ancien lecteur
		// saute).
		//
		// L'etat de melange d'une entite (les couches au-dela de la base, les
		// phases des arbres) vit dans NkMelangeAnimateur2D, AJOUTE PAR LE SYSTEME.
		// Il n'est pas photographie : c'est un etat de lecture, il repart a chaque
		// « Jouer » (la base, elle, est dans NkAnimateur2D::execution).
		struct NkMelangeAnimateur2D {
				anim::NkAnimControllerRuntime execution;
		};

		/// Enregistre (ou REMPLACE) un controleur complet (copie).
		bool NkEnregistrerControleurAnimateur(const char *nom, const anim::NkAnimController &controleur);
		/// Le controleur, ou nul (NkModeleAnimateur rend sa machine de base).
		anim::NkAnimController *NkControleurAnimateur(const char *nom);

		/// LE MELANGE EN JEU, apres NkAvancerClipsProprietes dans NkScene::Pas :
		/// pour chaque animateur dont le controleur MELANGE (des etats qui jouent
		/// des clips de proprietes ou des arbres, des couches), la pose melangee de
		/// NKAnima -- fondu de transition a sa courbe, arbre selon ses parametres,
		/// couches sous leurs masques -- est ecrite dans les proprietes de l'entite
		/// et de ses descendants nommes (NkAppliquerPoseProprietes). Une entite qui
		/// n'a ni NkClipProprietes2D ni couche ni arbre n'est pas touchee.
		void NkMelangerAnimateurs(NkScene &scene, float32 dt);

		/// LE SYSTEME : pour chaque NkAnimateur2D, fait avancer son modele avec
		/// SES parametres et SON etat, puis, si l'etat a change, joue le clip
		/// de l'etiquette dans le NkAnimSprite2D de la meme entite.
		/// NkScene::Pas l'appelle juste AVANT NkAvancerAnimations : le clip
		/// choisi a cette trame y est avance a cette trame.
		void NkAvancerAnimateurs(ecs::NkWorld &monde, float32 dt);

	} // namespace unkeny
} // namespace nkentseu
