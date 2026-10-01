// =============================================================================
// NkUnkenyProprietes.h — les PROPRIETES ANIMABLES d'une entite, et le clip de
// proprietes qui les fait bouger (page Animation d'UnkenyEditor, R21/R22/R35)
//
// A QUOI SERT CE FICHIER
//   Une piste d'animation dit « Transform.position de l'objet Bras/Main ».
//   Ce fichier sait, pour une entite : QUELLES proprietes elle a, les LIRE et les
//   ECRIRE sous forme d'un NkVec4f (la valeur d'une cle de NKAnima), et appliquer
//   un clip entier a un instant donne. L'editeur s'en sert pour son apercu ; le
//   jeu pour jouer le clip (NkClipProprietes2D, ci-dessous).
//
// ⚠️ « TOUTE PROPRIETE REFLECHIE » (Rihen, 01/10 : « avec le temps on va ajouter
//    d'autres proprietes ») : trois sources, dans cet ordre, et AUCUNE ne demande
//    de toucher ce fichier pour une propriete de plus :
//      1. les proprietes du NOYAU d'Unkeny (transform, sprite, lumiere), qui ne
//         sont pas des composants decrits : la liste est ici ;
//      2. tout composant DECRIT champ par champ (NkScene::PhotographierAussi avec
//         ses NkChampSauve, NkUnkenyChamps.h) -- la reflexion qu'Unkeny a deja
//         pour sauver ; un composant de jeu decrit devient animable tout seul ;
//      3. tout composant REFLECHI par NKECS (NK_REFLECT_BEGIN, NkReflect.h).
//    L'INTERPOLATION suit le genre : nombre, vecteur et couleur glissent ;
//    booleen, entier, enumeration et reference d'asset vont PAR PALIERS.
//
// ⚠️ LES COULEURS passent par math::NkColor (0xRRGGBBAA <-> 0..1) : aucun
//    convertisseur maison.
//
// ⚠️ UN ENFANT S'ANIME DANS LE REPERE DE SON PARENT : « Transform.* » d'une
//    entite qui a un parent lit et ecrit sa place LOCALE (NkLocal2D), comme le
//    localPosition d'Unity ; la hierarchie recalcule son monde.
//
// LA ROTATION est en DEGRES dans les pistes (ce que l'humain regle), en radians
// dans le composant.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - une propriete du noyau      -> la table kNoyau du .cpp (et ses deux switch)
//   - une propriete d'un composant -> le DECRIRE (PhotographierAussi + champs)
// =============================================================================
#pragma once

#include "NKAnima/Blend/NkAnimMix.h" // (01/10 soir) le melange : poses de proprietes, NLA, couches
#include "NKAnima/Clip/NkAnimation.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKECS/NkECSDefines.h"

namespace nkentseu {
	namespace unkeny {
		class NkScene;

		using NkGenrePropriete = anim::NkAnimationClip::NkPropertyKind;

		struct NkProprieteAnimable {
				NkString nom;	  ///< « Transform.position » : la cle de la piste
				NkString libelle; ///< « Position »
				NkString groupe;  ///< « Transform » : le composant
				NkGenrePropriete genre = NkGenrePropriete::NK_NUMBER;
				uint8 canaux = 1; ///< 1..4
		};

		/// Les proprietes animables de `id`, dans un ordre STABLE (noyau, decrits,
		/// reflechis). Vide si l'entite est morte.
		void NkListerProprietesAnimables(NkScene &scene, ecs::NkEntityId id, NkVector<NkProprieteAnimable> &out);
		/// La description d'UNE propriete de `id` (faux si elle ne l'a pas).
		bool NkTrouverProprieteAnimable(NkScene &scene, ecs::NkEntityId id, const NkString &nom, NkProprieteAnimable &out);
		/// La valeur courante. Faux si l'entite n'a pas la propriete.
		bool NkLireProprieteAnimee(NkScene &scene, ecs::NkEntityId id, const NkString &nom, math::NkVec4f &v);
		/// Ecrit la valeur. Faux si l'entite n'a pas la propriete : une piste
		/// n'AJOUTE jamais de composant.
		bool NkEcrireProprieteAnimee(NkScene &scene, ecs::NkEntityId id, const NkString &nom, const math::NkVec4f &v);

		/// L'entite designee par `chemin` depuis `racine` : « » = la racine,
		/// « Bras/Main » = son enfant « Bras », puis l'enfant « Main » de celui-ci
		/// (noms de NkEtiquette). Invalide si le chemin ne mene a rien.
		ecs::NkEntityId NkResoudreCible(NkScene &scene, ecs::NkEntityId racine, const NkString &chemin);
		/// L'inverse : le chemin de `cible` depuis `racine` (« » si c'est elle).
		/// Faux si `cible` n'est pas un descendant de `racine`.
		bool NkCheminCible(NkScene &scene, ecs::NkEntityId racine, ecs::NkEntityId cible, NkString &chemin);

		/// Applique les pistes de PROPRIETES de `clip` a l'instant `t`, sur `racine`
		/// et ses descendants nommes. Rend le nombre de pistes appliquees (une
		/// piste dont la cible ou la propriete manque est sautee, sans bruit).
		uint32 NkAppliquerClipProprietes(NkScene &scene, ecs::NkEntityId racine, const anim::NkAnimationClip &clip,
										 float32 t);
		/// (2026-10-01 soir) Applique une POSE melangee (NKAnima, Blend/NkAnimMix.h) :
		/// chaque valeur de propriete, a sa COUVERTURE -- 1 = la valeur, moins = un
		/// fondu depuis la valeur vivante de l'objet (une propriete que seul un
		/// des deux clips d'un fondu anime). Rend le nombre de valeurs ecrites.
		uint32 NkAppliquerPoseProprietes(NkScene &scene, ecs::NkEntityId racine, const anim::NkAnimPose &pose);
		/// Retrouver un clip de proprietes ENREGISTRE par son nom (les clips poses
		/// d'une sequence, les etats d'un animateur, les arbres de melange).
		anim::NkClipLookup NkRechercheClipsProprietes();

		// --- Le clip de proprietes EN JEU ----------------------------------
		static const int32 NK_UNKENY_CLIP_NOM_MAX = 32;

		/// Le composant : QUEL clip joue, ou on en est. DONNEES SEULEMENT (comme
		/// NkAnimSprite2D) : le temps avance dans NkAvancerClipsProprietes. Un
		/// etat de l'Animateur qui designe une animation la fait jouer ici
		/// (NkUnkenyAnimateur.cpp).
		struct NkClipProprietes2D {
				char clip[NK_UNKENY_CLIP_NOM_MAX] = {};
				float32 temps = 0.f;
				float32 vitesse = 1.f;
				bool enPause = false;
				bool boucle = true;
				bool termine = false;
				/// Rempli par le systeme : le clip n'est pas enregistre (dit une fois).
				bool clipAbsent = false;
				/// Joue `nom` depuis le debut -- sauf s'il joue deja (et n'est pas
				/// termine) : rappele a chaque trame, il ne fige pas l'animation.
				void Jouer(const char *nom) noexcept;
		};

		/// Enregistre (ou REMPLACE) un clip sous `nom` ; il est COPIE.
		bool NkEnregistrerClipProprietes(const char *nom, const anim::NkAnimationClip &clip);
		/// Lit un .nkanim et l'enregistre sous `nom`.
		bool NkChargerClipProprietes(const char *nom, const char *chemin);
		const anim::NkAnimationClip *NkClipProprietesEnregistre(const char *nom);

		/// LE SYSTEME : avance chaque NkClipProprietes2D et applique son clip a son
		/// entite. NkScene::Pas l'appelle apres les animations de sprites.
		void NkAvancerClipsProprietes(NkScene &scene, float32 dt);

	} // namespace unkeny
} // namespace nkentseu
