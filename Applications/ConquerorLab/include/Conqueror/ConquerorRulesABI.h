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
		// VERSION DE L'ABI — deux nombres, et la difference compte.
		//
		// MAJEURE  incompatible : un champ change de type, de place, disparait,
		//          ou une fonction change de signature. Le module DOIT etre
		//          recompile ; l'atelier le refuse.
		//
		// MINEURE  ajout en FIN de vtable, rien d'autre. Un module plus ancien
		//          reste UTILISABLE : sa queue de vtable est nulle (l'hote fait
		//          un memset avant l'appel), et chaque entree optionnelle est
		//          deja gardee par un test de nullite.
		//
		// POURQUOI CETTE SEPARATION EXISTE
		// Il n'y avait qu'un seul nombre. Consequence : le jour ou j'ai ajoute
		// trois entrees a la fin de la vtable, TOUS les modules des stagiaires
		// devenaient « ABI 2, attendue 3 » — refuses, alors qu'ils tournaient
		// parfaitement. Verifie le 2026-08-07 : un module d'epoque joue une
		// partie complete sans un seul coup illegal ; seul le controle de
		// version l'ecartait.
		//
		// Faire payer a deux stagiaires la recompilation de tout leur travail
		// parce que J'AI ajoute une fonction est exactement le genre de friction
		// qu'un contrat est cense supprimer.
		//
		// LA REGLE, ET ELLE EST PLUS ETROITE QU'IL N'Y PARAIT
		// On n'ajoute qu'a la fin de la DERNIERE structure de la chaine :
		//   - fin de NkcRulesVTable        -> sur
		//   - fin de NkcRulesFactory       -> sur
		//   - fin de NkcRulesInfo          -> INTERDIT
		//
		// `info` precede `vtable` dans la fabrique : la faire grossir DECALE la
		// vtable, un module d'epoque ecrit alors sa table a l'ancien decalage,
		// l'atelier la lit au nouveau, et on appelle des adresses prises au
		// hasard. Ce n'est pas une crainte theorique — c'est exactement ce qui
		// s'est produit au premier essai, et `tests/NkcAbiCompat.cpp` a plante.
		//
		// Ce banc d'essai monte desormais la garde a chaque modification.
		// ---------------------------------------------------------------------
		inline constexpr uint32 kRulesAbiMajor = 3;
		inline constexpr uint32 kRulesAbiMinor = 1;

		/// Nom historique, conserve : c'est la MAJEURE. Les modules ecrits avant
		/// l'introduction de la mineure continuent de compiler sans modification.
		inline constexpr uint32 kRulesAbiVersion = kRulesAbiMajor;

		// Bornes dures : tableaux de taille fixe, aucune allocation a la frontiere.
		inline constexpr uint32 kMaxPlayers   = 4;
		inline constexpr uint32 kMaxFuseCells = 12;  // poids N4 = 10 N0, marge
		inline constexpr uint32 kMaxCells     = 512; // 42 cases en 6x7, marge large
		inline constexpr uint32 kMaxCellPoints = 12; // sommets d'une cellule dessinee

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
				uint32 abiVersion = kRulesAbiVersion;   ///< MAJEURE
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

				// ---- geometrie DECLAREE PAR LE MODULE (ABI 3) ---------------
				// TOUT CE BLOC EST OPTIONNEL : laisser a nullptr fait retomber
				// l'atelier sur la projection standard de `topology`.
				//
				// POURQUOI IL EXISTE
				// Le voisinage, les cases bloquees et la forme du plateau etaient
				// deja 100 % l'affaire du module : il choisit ses coordonnees, son
				// adjacence, ce qu'il declare bloque. UNE SEULE CHOSE lui echappait
				// — la PROJECTION ECRAN, deduite de `NkcTopology`, donc limitee a
				// l'hexagone et au carre. Un plateau en triangles, en octogones, a
				// cases de tailles inegales, ou carrement libre etait impossible a
				// AFFICHER, meme si les regles savaient le jouer.
				//
				// Ces deux entrees rendent la main au module. Elles ne renvoient
				// que de la PRESENTATION : ces flottants ne rentrent jamais dans la
				// logique de regles, et §17.1 reste entier.

				/// Centre de la cellule `c`, en unites de cellule (1.0 = un « pas »
				/// de grille). L'atelier cadre et met a l'echelle ensuite : ne vous
				/// souciez ni du zoom ni des pixels. Renvoyer 0 = « je ne sais pas »,
				/// l'atelier retombe alors sur la topologie POUR CETTE CELLULE.
				int32 (*GetCellCenter)(NkcRules self, NkcCoord c, float32 *outXY) = nullptr;

				/// Contour de la cellule `c` : `capPoints` paires (x, y) au plus,
				/// en unites de cellule, RELATIVES au centre. Renvoie le nombre de
				/// points ecrits (3 a kMaxCellPoints), ou 0 pour laisser l'atelier
				/// deduire la forme de la topologie.
				///
				/// C'est ce qui autorise un plateau qui n'est ni hexagonal ni carre.
				uint32 (*GetCellShape)(NkcRules self, NkcCoord c, float32 *outXY,
									   uint32 capPoints) = nullptr;

				/// Voisins de `c` : ecrit au plus `cap` coordonnees, renvoie le
				/// nombre TOTAL de voisins. nullptr -> l'appelant retombe sur
				/// `Neighbor(topology, ...)` de ConquerorGeometry.h.
				///
				/// POURQUOI CETTE ENTREE EXISTE
				/// Le voisinage a toujours ete l'affaire du module — il le code dans
				/// GenerateLegalMoves et personne ne le lui dispute. Le probleme
				/// etait que PERSONNE D'AUTRE ne pouvait le connaitre :
				///
				///   - une IA qui evalue une position (« combien d'ennemis touche
				///     cette case ? ») n'avait que `view.topology` pour le deviner.
				///     Sur une grille libre, elle devinait FAUX, silencieusement ;
				///   - l'atelier ne pouvait pas montrer le voisinage a l'ecran, donc
				///     un stagiaire qui se trompait d'adjacence n'avait aucun moyen
				///     de le VOIR.
				///
				/// Declarer son voisinage n'est donc pas une contrainte de plus :
				/// c'est ce qui rend une grille non standard utilisable par les
				/// autres. Entierement ENTIER — c'est de la regle, pas du dessin.
				uint32 (*GetNeighbors)(NkcRules self, NkcCoord c, NkcCoord *out,
									   uint32 cap) = nullptr;
		};

		struct NkcRulesFactory {
				NkcRulesInfo   info;
				NkcRulesVTable vtable;

				// ---- ajoute en MINEURE 1 ------------------------------------
				// ICI, ET SURTOUT PAS DANS NkcRulesInfo. Grossir `info` DECALERAIT
				// `vtable`, qui vient apres : un module d'epoque ecrirait sa table
				// a l'ancien decalage et l'atelier la lirait au nouveau. On appelle
				// alors des adresses prises au hasard.
				//
				// Ce n'est pas une crainte theorique : le banc tests/NkcAbiCompat.cpp
				// a plante des le premier essai, exactement pour cette raison.
				//
				// LA REGLE EXACTE est donc plus etroite que « ajouter a la fin » :
				// on n'ajoute qu'a la fin de la DERNIERE structure de la chaine.
				// Tout ce qui precede un champ existant est fige a jamais.
				uint32 abiMinor	   = kRulesAbiMinor;
				uint32 vtableBytes = 0;	 ///< rempli par NkcRulesStamp
		};

		/// A appeler en fin de votre FillFactory. Deux lignes que l'atelier lit
		/// pour savoir CONTRE QUELLE VERSION vous avez compile — et vous dire, le
		/// cas echeant, « ton module est plus ancien que l'atelier, telles
		/// fonctions ne sont pas disponibles » au lieu de le refuser en bloc.
		///
		/// L'oublier n'est pas une faute : les champs restent a zero, l'atelier
		/// lit « je ne sais pas » et se rabat sur les pointeurs de la vtable.
		inline void NkcRulesStamp(NkcRulesFactory *out) noexcept {
			if (!out) return;
			out->info.abiVersion  = kRulesAbiMajor;
			out->abiMinor	 = kRulesAbiMinor;
			out->vtableBytes = static_cast<uint32>(sizeof(NkcRulesVTable));
		}

		/// Hooks memoire injectes par l'atelier (NKMemory). Repli malloc/free si
		/// l'hote ne les pose pas — jamais new/delete bruts.
		using NkcAllocFn = void *(*)(usize size);
		using NkcFreeFn	 = void (*)(void *ptr);

		// ---------------------------------------------------------------------
		// JOURNAL — pourquoi il faut un canal explicite
		//
		// Un module est lie STATIQUEMENT a sa propre copie de Nkentseu. Son
		// `logger` n'est donc PAS celui de l'atelier : un logger.Infof() depuis un
		// module ecrit dans un journal que personne ne lit. Ce n'est pas un
		// oubli, c'est la consequence directe de l'isolation qui fait tout
		// l'interet du systeme — mais laisser le stagiaire sans retour serait
		// absurde.
		//
		// L'atelier injecte donc un puits, et le module y renvoie son journal.
		// Le brancher tient en UNE LIGNE : voir Conqueror/ConquerorLog.h.
		// ---------------------------------------------------------------------

		/// Niveaux, calques sur ceux de NKLogger.
		enum class NkcLogLevel : int32 {
			Trace = 0, Debug = 1, Info = 2, Warn = 3, Error = 4, Fatal = 5
		};

		/// `user` est reinjecte tel quel. `module` nomme la source (« mes_regles »),
		/// `text` est deja formate. L'appel peut venir de N'IMPORTE QUEL THREAD :
		/// une IA journalise depuis son worker.
		using NkcLogFn = void (*)(void *user, NkcLogLevel level,
								  const char *module, const char *text);

		// Les symboles qu'un module de regles peut exporter. Les deux premiers
		// sont OBLIGATOIRES, le troisieme est optionnel.
		using NkcRulesGetFactoryFn	 = void (*)(NkcRulesFactory *out);
		using NkcRulesSetAllocatorFn = void (*)(NkcAllocFn a, NkcFreeFn f);
		using NkcRulesSetLoggerFn	 = void (*)(NkcLogFn fn, void *user, const char *name);

	} // namespace conqueror
} // namespace nkentseu

#define NKC_RULES_SYM_GET_FACTORY "nkc_rules_get_factory"
#define NKC_RULES_SYM_SET_ALLOC	  "nkc_rules_set_allocator"
#define NKC_RULES_SYM_SET_LOGGER  "nkc_rules_set_logger"

#if defined(NKENTSEU_PLATFORM_WINDOWS) || defined(_WIN32)
#	define NKC_MODULE_EXPORT extern "C" __declspec(dllexport)
#else
#	define NKC_MODULE_EXPORT extern "C" __attribute__((visibility("default")))
#endif
