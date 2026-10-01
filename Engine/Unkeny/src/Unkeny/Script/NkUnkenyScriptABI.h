//
// NkUnkenyScriptABI.h
// =============================================================================
// Description :
//   LA TABLE C entre Unkeny et ses scripts (document 01 d'UnkenyEditor, § 4.3).
//   Le Blueprint (par sa machine virtuelle) et le C++ (par sa DLL, ou lie en
//   statique dans le jeu construit) appellent LES MEMES fonctions : celles de
//   NkUnkHoteV1. Un noeud « Appliquer une impulsion » et
//   `hote->AppliquerImpulsion(...)` sont le meme code.
//
// Caracteristiques :
//   - AUTONOME : <stdint.h> et <string.h> seuls. Un script C++ l'inclut et
//     ne lie AUCUNE bibliotheque du moteur. Pourquoi : NKECS donne les
//     identifiants de composant par des variables d'en-tete ; une DLL qui le
//     lierait en aurait sa propre copie et lirait la mauvaise colonne, sans
//     erreur (document 01, § 4.3, regle 2).
//   - Aucun pointeur vers le moteur ne traverse : des VALEURS et des POIGNEES
//     d'entite (NkUnkEntite, verifiees a chaque appel par l'hote).
//   - Chaque fonction rend 1 (fait) ou 0 (refuse : entite morte, composant
//     absent, valeur non finie). Les resultats sortent par pointeur ; aucune
//     structure rendue par valeur a travers extern "C".
//   - Version MAJEURE / MINEURE : une majeure differente est refusee ; les
//     ajouts se font EN FIN de structure seulement, et `taille` dit ce que
//     l'hote connait (la lecon de ConquerorLab, ConquerorRulesABI.h).
//   - UN seul point d'entree d'evenement par classe (`Evenement`), avec un champ
//     `genre` : un evenement de plus ne change pas la table.
//
// ECRIRE UN SCRIPT C++ (le modele que l'editeur pose dans Contenu/Scripts) :
//
//     #include "Unkeny/Script/NkUnkenyScriptABI.h"
//     class Porte : public nkunk::Script {
//         public:
//             void ZoneEntree(NkUnkEntite autre, bool soiEstLaZone) override {
//                 if (NomEst(autre, "Joueur")) {
//                     NkUnkEntite porte = ParNom("Porte");
//                     Teleporter(porte, Position(porte) + nkunk::Vec2(0.f, 2.f));
//                 }
//             }
//     };
//     NK_UNKENY_CLASSE(Porte)
//
//   L'editeur genere le registre des classes (il lit les NK_UNKENY_CLASSE des
//   sources) : DLL rechargee a chaud dans l'editeur, liee en statique dans le
//   jeu construit. Aucun auto-enregistrement par objet statique : dans une
//   bibliotheque statique, l'editeur de liens jetterait l'unite (document 01,
//   § 5.6).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYSCRIPTABI_H__
#define __NKENTSEU_UNKENY_NKUNKENYSCRIPTABI_H__

#include <stdint.h>
#include <string.h>

#define NK_UNK_ABI_MAJEURE 1
#define NK_UNK_ABI_MINEURE 0

/// Le symbole qu'exporte une DLL de scripts (genere par l'editeur).
#define NK_UNK_POINT_ENTREE "nk_unkeny_module_v1"

#if defined(_WIN32)
#define NK_UNK_EXPORTE __declspec(dllexport)
#else
#define NK_UNK_EXPORTE __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

	/// Une entite : NkEntityId::Pack() (generation << 32 | index). 0 = aucune.
	typedef struct NkUnkEntite {
			uint64_t pack;
	} NkUnkEntite;

	// --- Les genres d'evenement -------------------------------------------------
	// AJOUTES A LA FIN : un module ancien ignore ce qu'il ne connait pas.
#define NK_UNK_EV_DEBUT 0u			 ///< une fois, avant tout autre evenement de l'entite
#define NK_UNK_EV_TICK 1u			 ///< chaque image (NK_TRAME)
#define NK_UNK_EV_PAS_FIXE 2u		 ///< chaque pas fixe, AVANT la physique (le lieu des forces)
#define NK_UNK_EV_CONTACT_DEBUT 3u	 ///< choc entre corps solides, recu par les DEUX entites
#define NK_UNK_EV_CONTACT_FIN 4u
#define NK_UNK_EV_ZONE_ENTREE 5u	 ///< declencheur traverse, recu par la zone ET par l'entrant
#define NK_UNK_EV_ZONE_SORTIE 6u
#define NK_UNK_EV_ACTION_PRESSEE 7u	 ///< une ACTION (jamais une touche) vient d'etre pressee
#define NK_UNK_EV_ACTION_RELACHEE 8u
#define NK_UNK_EV_RECHARGE 9u		 ///< (C++) la classe vient d'etre rechargee a chaud
/// (2026-10-01) Un REPARTITEUR d'evenement appele (Blueprint) : `nomAction` porte
/// son nom ; un Blueprint le recoit par son noeud « Evenement <repartiteur> ».
#define NK_UNK_EV_PERSONNALISE 10u
#define NK_UNK_EV_NOMBRE 11u

	/// Ce qu'un script recoit. Les champs inutiles a un genre valent 0.
	typedef struct NkUnkEvenementV1 {
			uint32_t genre;
			uint32_t emplacement; ///< l'indice du script dans la liste de l'entite
			float dt;			  ///< TICK, PAS_FIXE
			float valeur;		  ///< ACTION_* : la valeur de l'axe (-1..1)
			NkUnkEntite soi;
			NkUnkEntite autre;	  ///< CONTACT_*, ZONE_*
			int32_t soiEstLaZone; ///< ZONE_* : 1 si `soi` est la zone, 0 s'il y entre
			int32_t action;		  ///< ACTION_* : l'indice de l'action
			const char *nomAction; ///< ACTION_* : son nom (« Sauter »), jamais nul
	} NkUnkEvenementV1;

	/// Les types d'une variable de script. Une valeur tient dans deux reels :
	/// reel (x), entier (x, exact jusqu'a 2^24), booleen (x != 0), vec2 (x, y).
#define NK_UNK_REEL 0u
#define NK_UNK_ENTIER 1u
#define NK_UNK_BOOLEEN 2u
#define NK_UNK_VEC2 3u

	/// LA TABLE. `ctx` est opaque : on le rend tel quel a chaque appel.
	typedef struct NkUnkHoteV1 {
			uint32_t taille;	 ///< sizeof de la table de l'HOTE : ce qui depasse n'existe pas
			uint16_t abiMajeure; ///< differente = module refuse
			uint16_t abiMineure; ///< ajouts EN FIN de structure seulement
			void *ctx;

			// --- Journal et temps ---------------------------------------------
			void (*Afficher)(void *ctx, NkUnkEntite soi, const char *texte);
			float (*Temps)(void *ctx); ///< secondes de jeu depuis Jouer

			// --- Entites ------------------------------------------------------
			int32_t (*Vivante)(void *ctx, NkUnkEntite e);
			int32_t (*Detruire)(void *ctx, NkUnkEntite e);
			/// La premiere entite VIVANTE dont l'etiquette vaut `nom`.
			int32_t (*ParNom)(void *ctx, const char *nom, NkUnkEntite *sortie);
			/// 1 si l'etiquette de `e` vaut `nom` (casse comprise).
			int32_t (*NomEst)(void *ctx, NkUnkEntite e, const char *nom);
			int32_t (*Nom)(void *ctx, NkUnkEntite e, char *tampon, uint32_t taille);
			int32_t (*Activer)(void *ctx, NkUnkEntite e, int32_t actif);

			// --- Transform ----------------------------------------------------
			int32_t (*Position)(void *ctx, NkUnkEntite e, float *x, float *y);
			/// Deplace sans que le solveur y voie une vitesse (TeleporterEntite).
			int32_t (*Teleporter)(void *ctx, NkUnkEntite e, float x, float y);
			int32_t (*Rotation)(void *ctx, NkUnkEntite e, float *radians);
			int32_t (*PoserRotation)(void *ctx, NkUnkEntite e, float radians);

			// --- Corps rigide (M1 : le pont 2D, en UN seul endroit) ------------
			int32_t (*Vitesse)(void *ctx, NkUnkEntite e, float *vx, float *vy);
			int32_t (*PoserVitesse)(void *ctx, NkUnkEntite e, float vx, float vy);
			int32_t (*AppliquerImpulsion)(void *ctx, NkUnkEntite e, float ix, float iy);
			/// ⚠️ Une force ne vit qu'UN pas fixe : la poser sous PAS_FIXE.
			int32_t (*AppliquerForce)(void *ctx, NkUnkEntite e, float fx, float fy);

			// --- Sprite -------------------------------------------------------
			int32_t (*PoserCouleur)(void *ctx, NkUnkEntite e, uint32_t rgba);
			int32_t (*PoserVisible)(void *ctx, NkUnkEntite e, int32_t visible);

			// --- Animation (NkAnimSprite2D, NkAnimateur2D) --------------------
			int32_t (*JouerClip)(void *ctx, NkUnkEntite e, int32_t clip);
			int32_t (*AnimParametre)(void *ctx, NkUnkEntite e, const char *nom, float valeur);
			int32_t (*AnimDeclencher)(void *ctx, NkUnkEntite e, const char *nom);

			// --- Effets (NkEmetteur2D, R34) -----------------------------------
			/// Rejoue l'effet depuis le debut (rafale comprise) et l'allume.
			int32_t (*JouerEffet)(void *ctx, NkUnkEntite e);
			int32_t (*ArreterEffet)(void *ctx, NkUnkEntite e);

			// --- Entree : des ACTIONS, jamais des touches ----------------------
			int32_t (*ActionParNom)(void *ctx, const char *nom, int32_t *indice);
			float (*ValeurAction)(void *ctx, int32_t indice);
			int32_t (*ActionEnfoncee)(void *ctx, int32_t indice);

			// --- Son (par NOM : l'identifiant depend de la session) ------------
			int32_t (*JouerSon)(void *ctx, const char *nom, float volume);

			// --- Variables du script (celles des Details, sauvees par nom) ----
			int32_t (*LireVariable)(void *ctx, NkUnkEntite soi, uint32_t emplacement, const char *nom, float *x,
									float *y);
			int32_t (*EcrireVariable)(void *ctx, NkUnkEntite soi, uint32_t emplacement, const char *nom, float x,
									  float y);
	} NkUnkHoteV1;

	/// Une variable EXPOSEE d'une classe C++ (montree et sauvee comme celles
	/// d'un Blueprint, dans NkScript2D).
	typedef struct NkUnkVariableV1 {
			const char *nom;
			uint32_t type; ///< NK_UNK_REEL...
			float x, y;	   ///< la valeur par defaut
	} NkUnkVariableV1;

	/// Une classe de script C++.
	typedef struct NkUnkClasseV1 {
			const char *nom;
			void *(*Creer)(void);
			void (*Detruire)(void *instance);
			/// 1 = fait ; 0 = le script a leve une exception (rattrapee a la
			/// frontiere : l'instance passe en faute, la scene continue).
			int32_t (*Evenement)(void *instance, const NkUnkHoteV1 *hote, const NkUnkEvenementV1 *ev);
			/// L'etat PRIVE a garder au rechargement a chaud : rend le nombre
			/// d'octets ecrits (0 = rien a garder). Peut etre nul.
			uint32_t (*Sauver)(void *instance, uint8_t *tampon, uint32_t taille);
			int32_t (*Relire)(void *instance, const uint8_t *tampon, uint32_t taille);
			uint32_t nbVariables;
			const NkUnkVariableV1 *variables;
	} NkUnkClasseV1;

	/// Ce que rend le point d'entree d'un module (DLL ou registre statique).
	typedef struct NkUnkModuleV1 {
			uint32_t taille;
			uint16_t abiMajeure;
			uint16_t abiMineure;
			uint32_t nbClasses;
			const NkUnkClasseV1 *const *classes;
	} NkUnkModuleV1;

	typedef const NkUnkModuleV1 *(*NkUnkPointEntreeV1)(void);

#ifdef __cplusplus
}
#endif

// =============================================================================
// Le confort C++ : une classe de base qui parle la table, et deux macros.
// Rien ici n'est lie au moteur : tout passe par NkUnkHoteV1.
// =============================================================================
#ifdef __cplusplus

namespace nkunk {

	struct Vec2 {
			float x = 0.f, y = 0.f;
			Vec2() = default;
			Vec2(float a, float b) : x(a), y(b) {
			}
			Vec2 operator+(const Vec2 &o) const {
				return Vec2(x + o.x, y + o.y);
			}
			Vec2 operator-(const Vec2 &o) const {
				return Vec2(x - o.x, y - o.y);
			}
			Vec2 operator*(float k) const {
				return Vec2(x * k, y * k);
			}
	};

	/// La classe de base d'un script C++. Surcharger les evenements utiles ;
	/// les aides appellent la table de l'hote (rien d'autre).
	class Script {
		public:
			virtual ~Script() {
			}

			// --- Les evenements ---------------------------------------------
			virtual void Debut() {
			}
			virtual void Tick(float dt) {
				(void)dt;
			}
			virtual void PasFixe(float dt) {
				(void)dt;
			}
			virtual void ContactDebut(NkUnkEntite autre) {
				(void)autre;
			}
			virtual void ContactFin(NkUnkEntite autre) {
				(void)autre;
			}
			virtual void ZoneEntree(NkUnkEntite autre, bool soiEstLaZone) {
				(void)autre;
				(void)soiEstLaZone;
			}
			virtual void ZoneSortie(NkUnkEntite autre, bool soiEstLaZone) {
				(void)autre;
				(void)soiEstLaZone;
			}
			virtual void ActionPressee(const char *action, float valeur) {
				(void)action;
				(void)valeur;
			}
			virtual void ActionRelachee(const char *action) {
				(void)action;
			}
			/// Apres un rechargement a chaud (a la place de Debut).
			virtual void Recharge() {
			}
			/// L'etat prive a garder au rechargement (voir NK_UNKENY_GARDER).
			virtual uint32_t Sauver(uint8_t *tampon, uint32_t taille) {
				(void)tampon;
				(void)taille;
				return 0u;
			}
			virtual bool Relire(const uint8_t *tampon, uint32_t taille) {
				(void)tampon;
				(void)taille;
				return false;
			}
			/// (2026-10-01) Un REPARTITEUR d'evenement d'un Blueprint, appele sur
			/// cette entite : son nom, l'entite qui l'a appele, et son premier
			/// parametre reel (0 s'il n'en a pas).
			virtual void Repartiteur(const char *nom, NkUnkEntite source, float valeur) {
				(void)nom;
				(void)source;
				(void)valeur;
			}

			// --- Les aides --------------------------------------------------
			NkUnkEntite Soi() const {
				return soi_;
			}
			static bool Valide(NkUnkEntite e) {
				return e.pack != 0u;
			}
			void Afficher(const char *texte) const {
				hote_->Afficher(hote_->ctx, soi_, texte);
			}
			float Temps() const {
				return hote_->Temps(hote_->ctx);
			}
			bool Vivante(NkUnkEntite e) const {
				return hote_->Vivante(hote_->ctx, e) != 0;
			}
			bool Detruire(NkUnkEntite e) const {
				return hote_->Detruire(hote_->ctx, e) != 0;
			}
			NkUnkEntite ParNom(const char *nom) const {
				NkUnkEntite e{0u};
				hote_->ParNom(hote_->ctx, nom, &e);
				return e;
			}
			bool NomEst(NkUnkEntite e, const char *nom) const {
				return hote_->NomEst(hote_->ctx, e, nom) != 0;
			}
			bool Activer(NkUnkEntite e, bool actif) const {
				return hote_->Activer(hote_->ctx, e, actif ? 1 : 0) != 0;
			}
			Vec2 Position(NkUnkEntite e) const {
				Vec2 p;
				hote_->Position(hote_->ctx, e, &p.x, &p.y);
				return p;
			}
			bool Teleporter(NkUnkEntite e, const Vec2 &p) const {
				return hote_->Teleporter(hote_->ctx, e, p.x, p.y) != 0;
			}
			float Rotation(NkUnkEntite e) const {
				float r = 0.f;
				hote_->Rotation(hote_->ctx, e, &r);
				return r;
			}
			bool PoserRotation(NkUnkEntite e, float r) const {
				return hote_->PoserRotation(hote_->ctx, e, r) != 0;
			}
			Vec2 Vitesse(NkUnkEntite e) const {
				Vec2 v;
				hote_->Vitesse(hote_->ctx, e, &v.x, &v.y);
				return v;
			}
			bool PoserVitesse(NkUnkEntite e, const Vec2 &v) const {
				return hote_->PoserVitesse(hote_->ctx, e, v.x, v.y) != 0;
			}
			bool Impulsion(NkUnkEntite e, const Vec2 &i) const {
				return hote_->AppliquerImpulsion(hote_->ctx, e, i.x, i.y) != 0;
			}
			bool Force(NkUnkEntite e, const Vec2 &f) const {
				return hote_->AppliquerForce(hote_->ctx, e, f.x, f.y) != 0;
			}
			bool PoserCouleur(NkUnkEntite e, uint32_t rgba) const {
				return hote_->PoserCouleur(hote_->ctx, e, rgba) != 0;
			}
			bool PoserVisible(NkUnkEntite e, bool v) const {
				return hote_->PoserVisible(hote_->ctx, e, v ? 1 : 0) != 0;
			}
			bool JouerClip(NkUnkEntite e, int32_t clip) const {
				return hote_->JouerClip(hote_->ctx, e, clip) != 0;
			}
			bool AnimParametre(NkUnkEntite e, const char *nom, float v) const {
				return hote_->AnimParametre(hote_->ctx, e, nom, v) != 0;
			}
			bool AnimDeclencher(NkUnkEntite e, const char *nom) const {
				return hote_->AnimDeclencher(hote_->ctx, e, nom) != 0;
			}
			bool JouerEffet(NkUnkEntite e) const {
				return hote_->JouerEffet(hote_->ctx, e) != 0;
			}
			bool ArreterEffet(NkUnkEntite e) const {
				return hote_->ArreterEffet(hote_->ctx, e) != 0;
			}
			float ValeurAction(const char *action) const {
				int32_t i = -1;
				return hote_->ActionParNom(hote_->ctx, action, &i) != 0 ? hote_->ValeurAction(hote_->ctx, i) : 0.f;
			}
			bool ActionEnfoncee(const char *action) const {
				int32_t i = -1;
				return hote_->ActionParNom(hote_->ctx, action, &i) != 0 && hote_->ActionEnfoncee(hote_->ctx, i) != 0;
			}
			bool JouerSon(const char *nom, float volume = 1.f) const {
				return hote_->JouerSon(hote_->ctx, nom, volume) != 0;
			}
			/// Une variable EXPOSEE (Details) ; `defaut` si elle manque.
			float Variable(const char *nom, float defaut = 0.f) const {
				float x = defaut, y = 0.f;
				hote_->LireVariable(hote_->ctx, soi_, emplacement_, nom, &x, &y);
				return x;
			}
			Vec2 Variable2(const char *nom, const Vec2 &defaut = Vec2()) const {
				Vec2 v = defaut;
				hote_->LireVariable(hote_->ctx, soi_, emplacement_, nom, &v.x, &v.y);
				return v;
			}
			bool PoserVariable(const char *nom, float x, float y = 0.f) const {
				return hote_->EcrireVariable(hote_->ctx, soi_, emplacement_, nom, x, y) != 0;
			}

			// --- Pose par le thunk avant chaque evenement (ne pas toucher) ---
			const NkUnkHoteV1 *hote_ = nullptr;
			NkUnkEntite soi_{0u};
			uint32_t emplacement_ = 0u;
	};

	/// Le thunk d'evenement d'une classe : pose le contexte, aiguille, et
	/// RATTRAPE toute exception (document 01, § 8.2, N0). ⚠️ Pas de noexcept ici :
	/// une exception y appellerait std::terminate (le piege des crochets de Noge).
	template <class T> int32_t Evenement(void *instance, const NkUnkHoteV1 *hote, const NkUnkEvenementV1 *ev) {
		try {
			Script *s = static_cast<Script *>(static_cast<T *>(instance));
			s->hote_ = hote;
			s->soi_ = ev->soi;
			s->emplacement_ = ev->emplacement;
			switch (ev->genre) {
				case NK_UNK_EV_DEBUT:
					s->Debut();
					break;
				case NK_UNK_EV_TICK:
					s->Tick(ev->dt);
					break;
				case NK_UNK_EV_PAS_FIXE:
					s->PasFixe(ev->dt);
					break;
				case NK_UNK_EV_CONTACT_DEBUT:
					s->ContactDebut(ev->autre);
					break;
				case NK_UNK_EV_CONTACT_FIN:
					s->ContactFin(ev->autre);
					break;
				case NK_UNK_EV_ZONE_ENTREE:
					s->ZoneEntree(ev->autre, ev->soiEstLaZone != 0);
					break;
				case NK_UNK_EV_ZONE_SORTIE:
					s->ZoneSortie(ev->autre, ev->soiEstLaZone != 0);
					break;
				case NK_UNK_EV_ACTION_PRESSEE:
					s->ActionPressee(ev->nomAction, ev->valeur);
					break;
				case NK_UNK_EV_ACTION_RELACHEE:
					s->ActionRelachee(ev->nomAction);
					break;
				case NK_UNK_EV_RECHARGE:
					s->Recharge();
					break;
				case NK_UNK_EV_PERSONNALISE:
					s->Repartiteur(ev->nomAction, ev->autre, ev->valeur);
					break;
				default:
					break; // un genre d'un hote plus recent : ignore
			}
			return 1;
		} catch (...) {
			return 0;
		}
	}
	template <class T> void *Creer() {
		try {
			return static_cast<void *>(new T());
		} catch (...) {
			return nullptr;
		}
	}
	template <class T> void Detruire(void *instance) {
		try {
			delete static_cast<T *>(instance);
		} catch (...) {
		}
	}
	template <class T> uint32_t Sauver(void *instance, uint8_t *tampon, uint32_t taille) {
		try {
			return static_cast<Script *>(static_cast<T *>(instance))->Sauver(tampon, taille);
		} catch (...) {
			return 0u;
		}
	}
	template <class T> int32_t Relire(void *instance, const uint8_t *tampon, uint32_t taille) {
		try {
			return static_cast<Script *>(static_cast<T *>(instance))->Relire(tampon, taille) ? 1 : 0;
		} catch (...) {
			return 0;
		}
	}

} // namespace nkunk

/// Declare une classe de script : `NK_UNKENY_CLASSE(Porte)`. Definit la fonction
/// C `NkUnkClasse_Porte`, que le registre genere par l'editeur appelle.
#define NK_UNKENY_CLASSE(T)                                                                                            \
	extern "C" const NkUnkClasseV1 *NkUnkClasse_##T(void) {                                                            \
		static const NkUnkClasseV1 c = {#T,                &nkunk::Creer<T>, &nkunk::Detruire<T>, &nkunk::Evenement<T>,   \
										&nkunk::Sauver<T>, &nkunk::Relire<T>, 0u,                  nullptr};                \
		return &c;                                                                                                     \
	}

/// La meme, avec des variables EXPOSEES (montrees dans les Details) :
///   NK_UNKENY_CLASSE_VARIABLES(Porte, NK_UNKENY_REEL("hauteur", 2.f))
#define NK_UNKENY_CLASSE_VARIABLES(T, ...)                                                                             \
	extern "C" const NkUnkClasseV1 *NkUnkClasse_##T(void) {                                                            \
		static const NkUnkVariableV1 v[] = {__VA_ARGS__};                                                              \
		static const NkUnkClasseV1 c = {#T,                                                                            \
										&nkunk::Creer<T>,                                                              \
										&nkunk::Detruire<T>,                                                           \
										&nkunk::Evenement<T>,                                                          \
										&nkunk::Sauver<T>,                                                             \
										&nkunk::Relire<T>,                                                             \
										static_cast<uint32_t>(sizeof(v) / sizeof(v[0])),                               \
										v};                                                                            \
		return &c;                                                                                                     \
	}
#define NK_UNKENY_REEL(nom, defaut) NkUnkVariableV1{nom, NK_UNK_REEL, static_cast<float>(defaut), 0.f}
#define NK_UNKENY_ENTIER(nom, defaut) NkUnkVariableV1{nom, NK_UNK_ENTIER, static_cast<float>(defaut), 0.f}
#define NK_UNKENY_BOOLEEN(nom, defaut) NkUnkVariableV1{nom, NK_UNK_BOOLEEN, (defaut) ? 1.f : 0.f, 0.f}
#define NK_UNKENY_VEC2(nom, x, y) NkUnkVariableV1{nom, NK_UNK_VEC2, static_cast<float>(x), static_cast<float>(y)}

/// Garde un membre COPIABLE BIT A BIT a travers le rechargement a chaud :
///   struct Etat { int ouvertures = 0; } etat;  NK_UNKENY_GARDER(etat)
/// (dans le corps de la classe). Une taille changee entre deux versions n'est
/// pas relue : l'etat repart de sa valeur initiale, sans rien casser.
#define NK_UNKENY_GARDER(membre)                                                                                       \
	uint32_t Sauver(uint8_t *tampon, uint32_t taille) override {                                                       \
		if (taille < sizeof(membre)) {                                                                                 \
			return 0u;                                                                                                 \
		}                                                                                                              \
		memcpy(tampon, &membre, sizeof(membre));                                                                       \
		return static_cast<uint32_t>(sizeof(membre));                                                                  \
	}                                                                                                                  \
	bool Relire(const uint8_t *tampon, uint32_t taille) override {                                                     \
		if (taille != sizeof(membre)) {                                                                                \
			return false;                                                                                              \
		}                                                                                                              \
		memcpy(&membre, tampon, sizeof(membre));                                                                       \
		return true;                                                                                                   \
	}

#endif // __cplusplus

#endif // __NKENTSEU_UNKENY_NKUNKENYSCRIPTABI_H__
