#pragma once
// =============================================================================
// ConquerorRulesABI.h — contrat stable entre l'atelier de test et un moteur de
//                       regles de Conqueror.
//
// TYPES : tout vient de Nkentseu (NKCore/NkTypes.h). L'atelier pilote lui-meme
// la compilation des modules et leur passe les -I du depot, donc un module n'a
// jamais a se passer des types du moteur.
//
// COMMENT LE STAGIAIRE S'EN SERT
// ------------------------------
// Il n'ecrit PAS de DLL. Il depose un simple .cpp dans :
//
//     Build/ConquerorLab/rules/mes_regles.cpp
//
// L'atelier le detecte, le compile, le charge, et l'ajoute au menu deroulant.
// En cas d'erreur, les messages du compilateur s'affichent dans le panneau
// « Modules ». Aucun terminal, aucune manipulation de binaire.
//
// REFERENCE FONCTIONNELLE : REGLES_COMPLETES_v2.md
//   §4  plateau et topologies      §11 ressources
//   §5  totem et poids de fusion   §12 fin de partie
//   §6  structure d'un tour        §13 cas limites
//   §7  DUPLIQUER                  §14 table des parametres
//   §8  FUSIONNER                  §17 exigences non negociables
//
// TROIS REGLES D'OR (§17)
//   1. Aucun flottant dans la logique. Les Points de Conquete transitent en
//      DIXIEMES ENTIERS (100 == 10,0 PC). Seul SetParam/GetParam accepte un
//      float64, parce que c'est un canal de reglage, pas de jeu.
//   2. Le PRNG (generateur pseudo-aleatoire) est porte par l'etat, jamais
//      global : sans cela, deux clones d'une meme position piochent dans le
//      meme flux et le rejeu devient impossible.
//      NOTE : au palier 0 il n'y a AUCUN aleatoire dans Conqueror. Le champ
//      existe pour les artefacts aleatoires (palier 2) et les rollouts de l'IA.
//   3. Aucun ordre d'iteration de conteneur ne doit etre observable :
//      GenerateLegalMoves DOIT produire les coups dans un ordre stable.
// =============================================================================

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace conqueror {

		// ---------------------------------------------------------------------
		// Version de l'ABI. L'atelier refuse un module dont `abiVersion` differe.
		// ---------------------------------------------------------------------
		inline constexpr uint32 kRulesAbiVersion = 2;

		// Bornes dures : tableaux de taille fixe, aucune allocation a la frontiere.
		inline constexpr uint32 kMaxPlayers   = 4;
		inline constexpr uint32 kMaxFuseCells = 12;  // poids N4 = 10 N0, marge
		inline constexpr uint32 kMaxCells     = 512; // 42 cases en 6x7, marge large

		// ---------------------------------------------------------------------
		// Topologies (REGLES §4.1). Le voisinage est une propriete de la
		// topologie, jamais code en dur ailleurs.
		// ---------------------------------------------------------------------
		enum class NkcTopology : uint8 {
			HexPointy = 0,	///< 6 voisins, pointes haut/bas
			HexFlat	  = 1,	///< 6 voisins, pointes gauche/droite
			Square4	  = 2,	///< 4 voisins, orthogonaux
			Square8	  = 3	///< 8 voisins, + diagonales
		};

		/// Coordonnee. Axiale (q,r) en hexagone, cartesienne (q=x, r=y) en carre.
		/// Un seul type : c'est la topologie qui decide de l'interpretation.
		struct NkcCoord {
				int16 q = 0;
				int16 r = 0;
		};

		// Occupation d'une cellule (champ `owner`). Valeurs >= 0 : index du joueur.
		inline constexpr int8 kCellEmpty   = -1;
		inline constexpr int8 kCellBlocked = -2;

		/// Vue en lecture seule d'une cellule. L'atelier et l'IA ne voient QUE ca :
		/// la representation interne du module reste libre et privee.
		struct NkcCellView {
				int8  owner		 = kCellEmpty;	///< joueur, kCellEmpty ou kCellBlocked
				int8  level		 = 0;			///< 0..4
				int8  people	 = -1;			///< index de peuple, -1 si aucun
				uint8 powerUsed	 = 0;			///< pouvoir en reserve deja consomme
				int16 power		 = -1;			///< id du pouvoir en reserve, -1 si aucun
				int16 artefact	 = -1;			///< id d'artefact sur la case, -1 si aucun
		};

		/// Vue en lecture seule de l'etat complet. Les pointeurs appartiennent au
		/// MODULE et restent valides jusqu'au prochain appel mutant sur cet etat
		/// (ApplyMove, Setup, DeserializeState, CloneState). L'appelant ne libere
		/// jamais rien.
		struct NkcStateView {
				const NkcCellView *cells	 = nullptr;	///< cellCount entrees, index = index de case
				const NkcCoord	  *coords	 = nullptr;	///< cellCount entrees, meme indexation
				uint32			   cellCount = 0;

				NkcTopology topology	= NkcTopology::HexPointy;
				uint8		playerCount = 2;
				uint8		current		= 0;   ///< joueur au trait
				uint8		finished	= 0;

				int8   winner = -2;	 ///< index joueur, -1 = nul, -2 = en cours
				uint32 turn	  = 0;

				/// Ressources — kMaxPlayers entrees. `conquestTenths` est en DIXIEMES.
				const int32 *energy			= nullptr;
				const int32 *conquestTenths = nullptr;
				const int32 *totemCount		= nullptr;
		};

		// ---------------------------------------------------------------------
		// Coups (REGLES §6.2). POD comparable octet a octet APRES mise a zero :
		// le module DOIT remplir integralement les champs inutilises avec zero.
		// ---------------------------------------------------------------------
		enum class NkcMoveKind : uint8 {
			None	  = 0,
			Duplicate = 1,	///< from = totem source, to = case vide voisine
			Fuse	  = 2,	///< fuseCells[0..fuseCount-1], to = case du resultat
			Power	  = 3,	///< from = lanceur, to = cible, powerId
			Pass	  = 4	///< legal seulement si aucun autre coup ne l'est
		};

		struct NkcMove {
				NkcMoveKind kind		= NkcMoveKind::None;
				uint8		player		= 0;
				uint8		fuseCount	= 0;
				int8		targetLevel = -1;	///< Fuse : niveau vise, sinon -1

				int16 powerId = -1;				///< Power : id du pouvoir, sinon -1
				int16 _pad	  = 0;

				NkcCoord from;
				NkcCoord to;
				NkcCoord fuseCells[kMaxFuseCells];
		};

		// ---------------------------------------------------------------------
		// Evenements. Le moteur NE CONNAIT PAS l'animation : il decrit ce qui
		// s'est produit, la presentation decide comment le montrer.
		// ---------------------------------------------------------------------
		enum class NkcEventKind : uint8 {
			TotemDuplicated	 = 0,  ///< a = source, b = case creee
			TotemTransformed = 1,  ///< a = case retournee, value = ancien owner
			Cascade			 = 2,  ///< a = case declencheuse, value = nombre
			FusionPerformed	 = 3,  ///< a = case du resultat, value = niveau
			PowerActivated	 = 4,  ///< a = lanceur, b = cible, value = powerId
			ArtefactPlaced	 = 5,
			ArtefactExpired	 = 6,
			PlayerBlocked	 = 7,  ///< value = joueur bloque
			PlayerEliminated = 8,
			GameOver		 = 9   ///< value = vainqueur, -1 = nul
		};

		struct NkcEvent {
				NkcEventKind kind	= NkcEventKind::TotemDuplicated;
				uint8		 player = 0;
				uint16		 _pad	= 0;
				int32		 value	= 0;
				NkcCoord	 a;
				NkcCoord	 b;
		};

		/// Callback de collecte. `user` est reinjecte tel quel. Peut etre nullptr :
		/// une IA ne veut pas payer le cout des evenements pendant ses simulations.
		using NkcEventSink = void (*)(void *user, const NkcEvent *ev);

		// Handles opaques. L'atelier ne dereference JAMAIS ces pointeurs.
		using NkcRules = void *;  ///< instance de moteur de regles
		using NkcState = void *;  ///< etat de partie

		struct NkcRulesInfo {
				uint32 abiVersion = kRulesAbiVersion;
				char   name[64]	  = {};
				char   version[16] = {};
				char   author[64]  = {};
				uint8  supportsHex	  = 0;
				uint8  supportsSquare = 0;
				uint8  maxPlayers	  = 2;
				uint8  palier		  = 0;	///< 0, 1 ou 2 — cf. REGLES §15
		};

		// ---------------------------------------------------------------------
		// Table de fonctions. Convention de retour : 0 = echec, 1 = succes.
		// ---------------------------------------------------------------------
		struct NkcRulesVTable {

				// ---- cycle de vie ------------------------------------------
				NkcRules (*Create)() = nullptr;
				void (*Destroy)(NkcRules self) = nullptr;

				// ---- parametres (REGLES §14) --------------------------------
				// GetParamsSchemaJson decrit TOUS les reglages, ce qui permet a
				// l'atelier de generer son interface sans une ligne d'UI par
				// parametre. Format attendu :
				//   [{"key":"portee_duplication","label":"Portee","group":"Duplication",
				//     "type":"int","min":1,"max":3,"def":1,"val":1}, ...]
				// La chaine appartient au module, valide jusqu'au Destroy.
				const char *(*GetParamsSchemaJson)(NkcRules self) = nullptr;
				int32 (*SetParam)(NkcRules self, const char *key, float64 value) = nullptr;
				float64 (*GetParam)(NkcRules self, const char *key) = nullptr;

				// ---- plateau (REGLES §4) : une DONNEE, pas une constante ----
				// {"topology":"HEX_POINTY","cells":[[q,r],...],"blocked":[[q,r],...],
				//  "starts":[{"player":0,"q":0,"r":0,"level":0},...],
				//  "min_players":2,"max_players":2}
				int32 (*LoadBoardJson)(NkcRules self, const char *json) = nullptr;
				const char *(*GetBoardJson)(NkcRules self) = nullptr;

				// ---- etat ---------------------------------------------------
				NkcState (*CreateState)(NkcRules self) = nullptr;
				void (*DestroyState)(NkcRules self, NkcState st) = nullptr;
				/// Chemin CHAUD de l'IA : doit etre rapide et ne jamais allouer.
				void (*CloneState)(NkcRules self, NkcState dst, const NkcState src) = nullptr;

				/// Place les totems de depart et initialise le PRNG a partir de
				/// `seed`. Rejouer le meme (seed, suite de coups) DOIT redonner le
				/// meme etat, sur toute plateforme.
				int32 (*Setup)(NkcRules self, NkcState st, uint8 playerCount, uint64 seed) = nullptr;

				void (*GetView)(NkcRules self, const NkcState st, NkcStateView *out) = nullptr;

				// ---- regles -------------------------------------------------
				/// Ecrit au plus `cap` coups, renvoie le nombre TOTAL de coups
				/// legaux (peut depasser `cap`). L'ordre DOIT etre deterministe.
				uint32 (*GenerateLegalMoves)(NkcRules self, const NkcState st,
											 NkcMove *out, uint32 cap) = nullptr;

				int32 (*IsLegalMove)(NkcRules self, const NkcState st, const NkcMove *mv) = nullptr;

				/// Applique le coup et emet les evenements (sink peut etre nullptr).
				/// Renvoie 0 SANS RIEN MODIFIER si le coup est illegal.
				int32 (*ApplyMove)(NkcRules self, NkcState st, const NkcMove *mv,
								   NkcEventSink sink, void *user) = nullptr;

				int32 (*IsFinished)(NkcRules self, const NkcState st) = nullptr;
				int32 (*GetWinner)(NkcRules self, const NkcState st) = nullptr;  ///< -1 nul, -2 en cours

				/// Les 4 conditions de REGLES §12.1. L'atelier asserte en test que
				/// ceci equivaut a « aucun coup legal hors PASSER » : c'est le
				/// meilleur test d'integrite du generateur de coups.
				int32 (*IsPlayerBlocked)(NkcRules self, const NkcState st, uint8 player) = nullptr;

				// ---- serialisation / rejeu ----------------------------------
				/// Renvoie la taille ecrite, ou la taille NECESSAIRE si `cap` est
				/// insuffisant (rien n'est alors ecrit).
				uint32 (*SerializeState)(NkcRules self, const NkcState st, void *buf, uint32 cap) = nullptr;
				int32 (*DeserializeState)(NkcRules self, NkcState st, const void *buf, uint32 size) = nullptr;

				/// Empreinte 64 bits. Deux etats identiques DOIVENT donner la meme
				/// empreinte sur toute plateforme : outil de diagnostic du
				/// determinisme (REGLES §17.3).
				uint64 (*HashState)(NkcRules self, const NkcState st) = nullptr;
		};

		struct NkcRulesFactory {
				NkcRulesInfo   info;
				NkcRulesVTable vtable;
		};

		/// Hooks memoire injectes par l'atelier (NKMemory). Repli malloc/free si
		/// l'hote ne les pose pas — jamais new/delete bruts.
		using NkcAllocFn = void *(*)(usize size);
		using NkcFreeFn	 = void (*)(void *ptr);

		// Les DEUX symboles que tout module de regles doit exporter.
		using NkcRulesGetFactoryFn	 = void (*)(NkcRulesFactory *out);
		using NkcRulesSetAllocatorFn = void (*)(NkcAllocFn a, NkcFreeFn f);

	} // namespace conqueror
} // namespace nkentseu

#define NKC_RULES_SYM_GET_FACTORY "nkc_rules_get_factory"
#define NKC_RULES_SYM_SET_ALLOC	  "nkc_rules_set_allocator"

#if defined(NKENTSEU_PLATFORM_WINDOWS) || defined(_WIN32)
#	define NKC_MODULE_EXPORT extern "C" __declspec(dllexport)
#else
#	define NKC_MODULE_EXPORT extern "C" __attribute__((visibility("default")))
#endif
