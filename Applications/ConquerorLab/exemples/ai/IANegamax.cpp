// =============================================================================
// IANegamax.cpp — une IA COMPLETE : negamax, alpha-beta, approfondissement
//                 iteratif, tri des coups, budget de temps respecte.
//
// CE QUE CE FICHIER EST
// ---------------------
// Le pendant d'IAMinimale.cpp : celle-ci prenait un coup au hasard, celle-ci
// REFLECHIT. C'est l'exemple de reference pour le sujet A2, et il est ecrit pour
// etre lu de haut en bas.
//
// LES QUATRE PIECES, DANS L'ORDRE OU ELLES COMPTENT
//
//   1. EVALUER      donner un nombre a une position. Sans cela, rien.
//   2. NEGAMAX      explorer, en exploitant que ce qui est bon pour moi est
//                   exactement l'oppose de ce qui est bon pour l'adversaire.
//   3. ALPHA-BETA   cesser d'explorer une branche des qu'elle ne peut plus
//                   changer la decision. Meme resultat, dix fois moins de noeuds.
//   4. APPROFONDISSEMENT ITERATIF
//                   chercher a profondeur 1, puis 2, puis 3... et s'arreter quand
//                   le temps est ecoule. C'est ce qui rend le budget TENABLE.
//
// POURQUOI NEGAMAX ET PAS MINIMAX
// -------------------------------
// Minimax ecrit deux fonctions : une qui maximise, une qui minimise. Negamax
// n'en ecrit qu'UNE, en observant que
//
//     min(a, b) == -max(-a, -b)
//
// Autrement dit : la valeur d'une position pour le joueur au trait est l'oppose
// de sa valeur pour l'autre. On evalue TOUJOURS du point de vue du joueur au
// trait, et on renvoie -Negamax(...) a l'appelant. Moitie moins de code, donc
// moitie moins d'endroits ou se tromper de signe — et c'est l'erreur numero un
// dans ce genre d'algorithme.
//
// A PLUS DE DEUX JOUEURS, NEGAMAX NE S'APPLIQUE PAS
// -------------------------------------------------
// L'identite ci-dessus suppose un jeu a somme nulle a DEUX joueurs. A trois ou
// quatre, « le gain de l'un est la perte de l'autre » est faux : il y a trois
// autres. Ce module le detecte et retombe sur une recherche a un coup — c'est
// une limite ASSUMEE et signalee, pas un oubli. Traiter le multijoueur demande
// max^n ou paranoid search, et c'est un autre sujet.
//
// CONTRAINTE DE THREAD (ConquerorAIABI.h)
// ---------------------------------------
// ChooseMove tourne sur un thread worker. Elle ne touche que l'etat qu'on lui
// passe — un clone prive — ne pose aucun verrou, et RESPECTE budgetMs. Depasser
// le budget fige l'interface : c'est un echec, pas un detail.
//
// POUR L'UTILISER : copier dans travail/ai/ et sauvegarder.
// =============================================================================

#include "Conqueror/ConquerorAIABI.h"
#include "Conqueror/ConquerorLog.h"

#include "NKTime/NkChrono.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <new>

using namespace nkentseu;
using namespace nkentseu::conqueror;

namespace {

	NkcAllocFn gAlloc = nullptr;
	NkcFreeFn  gFree  = nullptr;

	void *RawAlloc(usize n) { return gAlloc ? gAlloc(n) : std::malloc(n); }
	void  RawFree(void *p)  { if (!p) return; if (gFree) gFree(p); else std::free(p); }

	// -------------------------------------------------------------------------
	// Bornes. Tout est en tableau de taille fixe : `ChooseMove` est le chemin
	// CHAUD, appele des dizaines de milliers de fois par seconde en campagne. Une
	// allocation par noeud couterait plus cher que la recherche elle-meme.
	// -------------------------------------------------------------------------
	constexpr int32 kMaxDepth	  = 24;	  ///< garde-fou, jamais atteint en pratique
	constexpr int32 kMaxMoves	  = 512;  ///< coups legaux retenus par noeud
	constexpr int32 kInfinity	  = 1000000;
	constexpr int32 kWinScore	  = 100000;

	// Verification du budget : tous les N noeuds, pas a chaque noeud. Lire
	// l'horloge coute un appel systeme sur certaines plateformes ; le faire un
	// million de fois par seconde couterait plus que la recherche.
	constexpr uint32 kClockMask = 1023u;

	// -------------------------------------------------------------------------
	// PARAMETRES exposes a l'atelier. Le panneau les affiche tout seul, a partir
	// du schema JSON : aucune ligne d'interface a ecrire (REGLES §1).
	// -------------------------------------------------------------------------
	enum ParamId : int32 {
		P_POIDS_MATERIEL = 0,	///< totems possedes
		P_POIDS_MOBILITE,		///< coups disponibles
		P_POIDS_MENACE,			///< ennemis adjacents a mes totems
		P_PROFONDEUR_MAX,		///< plafond, en plus du budget de temps
		P_TRI_DES_COUPS,		///< activer le tri (pour MESURER ce qu'il rapporte)
		P_COUNT
	};

	struct ParamDesc {
			const char *key, *label, *group, *type;
			int32		def, lo, hi;
	};

	const ParamDesc kParams[P_COUNT] = {
		{"poids_materiel", "Poids du materiel",	 "Evaluation", "int",  100,	  0, 1000},
		{"poids_mobilite", "Poids de la mobilite","Evaluation", "int",   10,	  0, 1000},
		{"poids_menace",   "Poids de la menace",  "Evaluation", "int",   25,	  0, 1000},
		{"profondeur_max", "Profondeur maximale", "Recherche",  "int",	  6,	  1,   24},
		{"tri_des_coups",  "Trier les coups",	  "Recherche",  "bool",	  1,	  0,	1},
	};

	// -------------------------------------------------------------------------
	// L'instance. Tout l'etat de la reflexion vit ici : rien de statique, rien de
	// global. C'est la condition pour que l'atelier puisse faire tourner
	// plusieurs parties EN PARALLELE sur des threads differents (campagne).
	// -------------------------------------------------------------------------
	struct AI {
			int32 params[P_COUNT];
			NkcAIConfig cfg;

			// --- table de coups, une ligne par profondeur ---
			// Allouee une fois pour toutes : `kMaxDepth * kMaxMoves * sizeof(NkcMove)`
			// fait quelques megaoctets, ce qui est le prix a payer pour ne jamais
			// allouer pendant la recherche.
			NkcMove moves[kMaxDepth][kMaxMoves];
			int32	scores[kMaxMoves];	 ///< tri du niveau racine

			// --- etats de travail, un par profondeur ---
			NkcState work[kMaxDepth];
			bool	 workReady = false;

			// --- garde-temps ---
			NkElapsedTime start;
			uint32		  budgetMs	= 1000;
			uint32		  nodes		= 0;
			bool		  outOfTime = false;

			// --- pour l'atelier ---
			uint32 lastNodes = 0;
			int32  lastDepth = 0;
			char   debugJson[4096];
			char   schemaBuf[1024];
	};

	// =========================================================================
	// 1. EVALUER
	//
	// LE NOMBRE EST DU POINT DE VUE DE `side`, et `side` est TOUJOURS le joueur
	// au trait dans `st`. C'est la convention que negamax exige, et s'en ecarter
	// une seule fois donne une IA qui joue CONTRE elle-meme.
	//
	// Ce piege n'est pas theorique : la premiere version de ce fichier evaluait
	// toujours du point de vue du joueur RACINE tout en negant a chaque
	// changement de trait. Les deux conventions se contredisaient, et l'IA
	// perdait 16 a 20 contre un joueur ALEATOIRE. Rien ne plantait : elle
	// cherchait simplement, tres efficacement, le pire coup.
	//
	// Trois termes, et leur ordre d'importance est mesurable — c'est justement
	// pour cela qu'ils sont des PARAMETRES et pas des constantes :
	//
	//   MATERIEL   combien de totems j'ai de plus que lui. Le plus evident, et
	//              generalement le plus fort au palier 0.
	//   MOBILITE   combien de coups j'ai. Un joueur sans coup a perdu (REGLES
	//              §12.1) : la mobilite mesure la distance a la defaite.
	//   MENACE     combien de MES totems touchent un ennemi. C'est la « surface
	//              de contact » de NOTE_DESIGN §2 — la lecture tactique centrale
	//              du jeu, en un nombre.
	// =========================================================================
	int32 Evaluate(AI *ai, const NkcRulesVTable *rules, NkcRules inst,
				   const NkcState st, uint8 side) {
		NkcStateView v;
		std::memset(&v, 0, sizeof(v));
		rules->GetView(inst, st, &v);

		// Position terminale : on n'evalue pas, on CONSTATE. Et on prefere gagner
		// vite — d'ou la profondeur retranchee, sinon l'IA temporise indefiniment
		// devant un mat qu'elle sait avoir.
		if (v.finished) {
			const int32 w = rules->GetWinner(inst, st);
			if (w == static_cast<int32>(side)) return kWinScore;
			if (w == -1)					   return 0;		// nul
			return -kWinScore;
		}

		int32 mine = 0, theirs = 0;
		if (v.totemCount) {
			for (uint8 p = 0; p < v.playerCount; ++p) {
				if (p == side) mine += v.totemCount[p];
				else		   theirs += v.totemCount[p];
			}
		}

		// MENACE : mes totems qui touchent un ennemi. On passe par GetNeighbors
		// si le module le declare — sinon on ne SAIT PAS quel est le voisinage, et
		// on ne devine pas. Une evaluation qui devine faux est pire qu'une
		// evaluation qui ignore le terme.
		int32 contact = 0;
		if (rules->GetNeighbors && v.cells && v.coords) {
			NkcCoord nb[16];
			for (uint32 i = 0; i < v.cellCount; ++i) {
				if (v.cells[i].owner != static_cast<int8>(side)) continue;
				const uint32 n = rules->GetNeighbors(inst, v.coords[i], nb, 16);
				for (uint32 k = 0; k < n && k < 16; ++k) {
					for (uint32 j = 0; j < v.cellCount; ++j) {
						if (v.coords[j].q != nb[k].q || v.coords[j].r != nb[k].r) continue;
						const int8 o = v.cells[j].owner;
						if (o >= 0 && o != static_cast<int8>(side)) ++contact;
						break;
					}
				}
			}
		}

		// MOBILITE : le nombre de coups du joueur AU TRAIT. Attention au signe —
		// si ce n'est pas `me` qui joue, sa mobilite compte CONTRE nous.
		const uint32 legal = rules->GenerateLegalMoves(inst, st, ai->moves[kMaxDepth - 1],
													  kMaxMoves);
		const int32	 mob   = (v.current == side) ? static_cast<int32>(legal)
												: -static_cast<int32>(legal);

		return (mine - theirs) * ai->params[P_POIDS_MATERIEL]
			 + mob			  * ai->params[P_POIDS_MOBILITE]
			 - contact		  * ai->params[P_POIDS_MENACE];
	}

	// =========================================================================
	// 2 + 3. NEGAMAX AVEC ALPHA-BETA
	//
	// alpha : le meilleur score que le joueur au trait s'est DEJA garanti.
	// beta  : le meilleur score que l'adversaire s'est deja garanti ailleurs.
	//
	// Des que `score >= beta`, on arrete : l'adversaire ne laissera jamais la
	// partie arriver ici, puisqu'il a mieux ailleurs. C'est la « coupure beta »,
	// et c'est TOUT l'alpha-beta. Le resultat est EXACTEMENT celui du negamax nu ;
	// seul le nombre de noeuds change.
	//
	// L'efficacite depend entierement de l'ORDRE des coups : explorer le meilleur
	// en premier fait couper tout de suite. D'ou le tri, plus bas.
	// =========================================================================
	/// Rend la valeur de `st` DU POINT DE VUE DU JOUEUR AU TRAIT dans `st`.
	///
	/// Il n'y a plus de parametre `me` : c'etait lui la source du bug. Tant que
	/// la fonction porte a la fois « le joueur racine » et « le joueur au trait »,
	/// il existe un endroit ou l'on peut confondre les deux — et il suffit d'un.
	int32 Negamax(AI *ai, const NkcRulesVTable *rules, NkcRules inst, NkcState st,
				  int32 depth, int32 alpha, int32 beta, int32 ply) {
		// --- garde-temps ---
		if ((++ai->nodes & kClockMask) == 0) {
			const float64 ms = (NkChrono::Now() - ai->start).ToMilliseconds();
			if (ai->budgetMs > 0 && ms >= static_cast<float64>(ai->budgetMs))
				ai->outOfTime = true;
		}
		if (ai->outOfTime) return 0;   // valeur ignoree : l'appelant jette le niveau

		NkcStateView v;
		std::memset(&v, 0, sizeof(v));
		rules->GetView(inst, st, &v);
		const uint8 mover = v.current;   // c'est LUI le point de vue, ici et partout

		if (depth <= 0 || rules->IsFinished(inst, st) || ply >= kMaxDepth - 1)
			return Evaluate(ai, rules, inst, st, mover);

		NkcMove		*buf   = ai->moves[ply];
		const uint32 total = rules->GenerateLegalMoves(inst, st, buf, kMaxMoves);
		const uint32 n	   = total < kMaxMoves ? total : kMaxMoves;
		if (n == 0) return Evaluate(ai, rules, inst, st, mover);

		int32 best = -kInfinity;
		for (uint32 i = 0; i < n; ++i) {
			// On joue sur un CLONE, jamais sur l'etat recu. Le contrat l'exige, et
			// c'est aussi ce qui permet a la campagne de faire tourner plusieurs
			// parties en parallele sans se marcher dessus.
			rules->CloneState(inst, ai->work[ply], st);
			if (!rules->ApplyMove(inst, ai->work[ply], &buf[i], nullptr, nullptr))
				continue;   // coup refuse : le moteur a le dernier mot

			int32 score;
			NkcStateView after;
			std::memset(&after, 0, sizeof(after));
			rules->GetView(inst, ai->work[ply], &after);

			// L'enfant rend SA valeur, du point de vue de SON joueur au trait. On
			// la ramene au point de vue de `mover` : identique si c'est le meme
			// joueur qui rejoue (coup gratuit, tour multiple), opposee sinon.
			//
			// Oublier le cas « le meme joueur rejoue » est l'erreur classique : le
			// signe s'inverse une fois de trop et l'IA cherche son propre malheur.
			if (after.current == mover) {
				score = Negamax(ai, rules, inst, ai->work[ply], depth - 1,
								alpha, beta, ply + 1);
			} else {
				score = -Negamax(ai, rules, inst, ai->work[ply], depth - 1,
								 -beta, -alpha, ply + 1);
			}

			if (ai->outOfTime) return 0;
			if (score > best) best = score;
			if (best > alpha) alpha = best;
			if (alpha >= beta) break;	// COUPURE BETA
		}

		return best == -kInfinity ? Evaluate(ai, rules, inst, st, mover) : best;
	}

	// =========================================================================
	// TRI DES COUPS
	//
	// L'alpha-beta ne coupe que si le bon coup vient tot. Un tri grossier suffit :
	// on prefere les coups qui retournent le plus d'ennemis, mesures en jouant le
	// coup pour de faux et en comparant les comptes de totems.
	//
	// C'est un PARAMETRE (`tri_des_coups`) exactement pour qu'on puisse MESURER
	// ce qu'il rapporte, au lieu de le croire. Coupez-le, relancez une campagne,
	// comparez le nombre de noeuds : c'est l'exercice 3 du chapitre 8.
	// =========================================================================
	void OrderMoves(AI *ai, const NkcRulesVTable *rules, NkcRules inst,
					const NkcState st, NkcMove *buf, int32 n, uint8 me) {
		if (!ai->params[P_TRI_DES_COUPS] || n < 2) return;

		for (int32 i = 0; i < n; ++i) {
			rules->CloneState(inst, ai->work[0], st);
			ai->scores[i] = -kInfinity;
			if (!rules->ApplyMove(inst, ai->work[0], &buf[i], nullptr, nullptr)) continue;
			NkcStateView v;
			std::memset(&v, 0, sizeof(v));
			rules->GetView(inst, ai->work[0], &v);
			ai->scores[i] = v.totemCount ? v.totemCount[me] : 0;
		}

		// Tri par insertion : n vaut quelques dizaines, et un tri stable garde
		// l'ordre du moteur pour les coups a score egal — donc le DETERMINISME.
		for (int32 i = 1; i < n; ++i) {
			const NkcMove m = buf[i];
			const int32	  s = ai->scores[i];
			int32		  j = i - 1;
			while (j >= 0 && ai->scores[j] < s) {
				buf[j + 1]		 = buf[j];
				ai->scores[j + 1] = ai->scores[j];
				--j;
			}
			buf[j + 1]		 = m;
			ai->scores[j + 1] = s;
		}
	}

	// =========================================================================
	// 4. APPROFONDISSEMENT ITERATIF
	//
	// On cherche a profondeur 1, on garde le meilleur coup. Puis 2. Puis 3. Quand
	// le temps est ecoule au milieu d'un niveau, on JETTE ce niveau et on garde le
	// resultat du precedent — complet, donc fiable.
	//
	// Cela parait du gaspillage : on refait le travail a chaque fois. Ce n'est
	// pas le cas. D'abord parce que le cout croit geometriquement (le dernier
	// niveau coute plus que tous les precedents reunis). Ensuite et surtout parce
	// que c'est LA SEULE FACON de respecter un budget de temps : sans lui, il
	// faudrait deviner la profondeur atteignable, et se tromper signifie soit
	// figer l'interface, soit ne rien chercher.
	// =========================================================================
	int32 Search(AI *ai, const NkcRulesVTable *rules, NkcRules inst,
				 const NkcState st, NkcAIResult *out, uint8 me) {
		NkcMove		*root  = ai->moves[0];
		const uint32 total = rules->GenerateLegalMoves(inst, st, root, kMaxMoves);
		const int32	 n	   = static_cast<int32>(total < kMaxMoves ? total : kMaxMoves);
		if (n == 0) return 0;

		OrderMoves(ai, rules, inst, st, root, n, me);

		NkcMove best	  = root[0];
		int32	bestScore = 0;
		int32	reached	  = 0;

		int32 cap = ai->params[P_PROFONDEUR_MAX];
		if (cap > kMaxDepth - 2) cap = kMaxDepth - 2;
		if (ai->cfg.maxSimulations > 0 && cap > 4) cap = 4;

		for (int32 depth = 1; depth <= cap; ++depth) {
			int32	levelBest	 = -kInfinity;
			NkcMove levelBestMove = root[0];

			for (int32 i = 0; i < n; ++i) {
				rules->CloneState(inst, ai->work[1], st);
				if (!rules->ApplyMove(inst, ai->work[1], &root[i], nullptr, nullptr)) continue;

				NkcStateView after;
				std::memset(&after, 0, sizeof(after));
				rules->GetView(inst, ai->work[1], &after);

				// Meme conversion qu'a l'interieur de l'arbre : la valeur rendue
				// est celle du joueur au trait APRES le coup.
				const int32 child = Negamax(ai, rules, inst, ai->work[1], depth - 1,
											-kInfinity, kInfinity, 2);
				const int32 s	  = (after.current == me) ? child : -child;

				if (ai->outOfTime) break;
				if (s > levelBest) { levelBest = s; levelBestMove = root[i]; }
			}

			// LE POINT QUI FAIT TOUT : on ne garde le resultat d'un niveau que
			// s'il a ete PARCOURU EN ENTIER. Un niveau interrompu a examine les
			// premiers coups seulement, et le « meilleur » qu'il propose est le
			// meilleur d'un echantillon arbitraire.
			if (ai->outOfTime) break;
			best	  = levelBestMove;
			bestScore = levelBest;
			reached	  = depth;
		}

		out->move		  = best;
		out->scoreMilli	  = bestScore > kWinScore / 2	? 1000
						  : bestScore < -kWinScore / 2	? -1000
						  : static_cast<int32>(bestScore / 10);
		out->simulations  = ai->nodes;
		out->depthReached = static_cast<uint32>(reached);
		out->hitBudget	  = ai->outOfTime ? 1 : 0;

		ai->lastNodes = ai->nodes;
		ai->lastDepth = reached;
		return 1;
	}

	// =========================================================================
	// La vtable
	// =========================================================================
	NkcAI V_Create() {
		void *mem = RawAlloc(sizeof(AI));
		if (!mem) return nullptr;
		AI *ai = new (mem) AI();
		for (int32 i = 0; i < P_COUNT; ++i) ai->params[i] = kParams[i].def;
		return static_cast<NkcAI>(ai);
	}

	void V_Destroy(NkcAI self) {
		if (!self) return;
		AI *ai = static_cast<AI *>(self);
		ai->~AI();
		RawFree(ai);
	}

	/// La difficulte pilote la PROFONDEUR et le budget, pas la qualite du code.
	/// Une IA facile n'est pas une IA bete : c'est la meme, qui regarde moins loin.
	void V_Configure(NkcAI self, const NkcAIConfig *cfg) {
		AI *ai = static_cast<AI *>(self);
		if (!ai || !cfg) return;
		ai->cfg		 = *cfg;
		ai->budgetMs = cfg->budgetMs;

		static const int32 kDepthByLevel[5] = {1, 2, 4, 6, 8};
		const int32		   lv = static_cast<int32>(cfg->difficulty);
		ai->params[P_PROFONDEUR_MAX] =
			kDepthByLevel[(lv >= 0 && lv < 5) ? lv : 1];
	}

	const char *V_GetParamsSchemaJson(NkcAI self) {
		AI	 *ai = static_cast<AI *>(self);
		char *w	 = ai->schemaBuf;
		usize left = sizeof(ai->schemaBuf);
		int32 k	   = std::snprintf(w, left, "[");
		w += k; left -= static_cast<usize>(k);
		for (int32 i = 0; i < P_COUNT; ++i) {
			k = std::snprintf(w, left,
							  "%s{\"key\":\"%s\",\"label\":\"%s\",\"group\":\"%s\","
							  "\"type\":\"%s\",\"min\":%d,\"max\":%d,\"def\":%d,\"val\":%d}",
							  i ? "," : "", kParams[i].key, kParams[i].label,
							  kParams[i].group, kParams[i].type, kParams[i].lo,
							  kParams[i].hi, kParams[i].def, ai->params[i]);
			if (k <= 0 || static_cast<usize>(k) >= left) break;
			w += k; left -= static_cast<usize>(k);
		}
		std::snprintf(w, left, "]");
		return ai->schemaBuf;
	}

	int32 V_SetParam(NkcAI self, const char *key, float64 value) {
		AI *ai = static_cast<AI *>(self);
		if (!ai || !key) return 0;
		for (int32 i = 0; i < P_COUNT; ++i) {
			if (std::strcmp(kParams[i].key, key) != 0) continue;
			int32 v = static_cast<int32>(value < 0 ? value - 0.5 : value + 0.5);
			// BORNAGE, pas refus : le contrat demande qu'une valeur hors plage
			// soit ramenee dans la plage, et le banc d'essai le verifie.
			if (v < kParams[i].lo) v = kParams[i].lo;
			if (v > kParams[i].hi) v = kParams[i].hi;
			ai->params[i] = v;
			return 1;
		}
		return 0;
	}

	int32 V_ChooseMove(NkcAI self, const NkcRulesVTable *rules, NkcRules inst,
					   const NkcState st, NkcAIResult *out) {
		AI *ai = static_cast<AI *>(self);
		if (!ai || !rules || !inst || !st || !out) return 0;
		std::memset(out, 0, sizeof(*out));

		NkcStateView v;
		std::memset(&v, 0, sizeof(v));
		rules->GetView(inst, st, &v);
		const uint8 me = v.current;

		// Etats de travail : alloues a la PREMIERE reflexion, pas dans Create.
		// On a besoin de `rules` pour les creer, et Create ne le recoit pas.
		if (!ai->workReady) {
			for (int32 i = 0; i < kMaxDepth; ++i) ai->work[i] = rules->CreateState(inst);
			ai->workReady = true;
		}

		ai->start	  = NkChrono::Now();
		ai->nodes	  = 0;
		ai->outOfTime = false;

		// PLUS DE DEUX JOUEURS : negamax ne s'applique pas (voir l'en-tete). On
		// le DIT, et on retombe sur une recherche a un coup — qui reste correcte,
		// simplement myope.
		if (v.playerCount > 2) {
			NKC_LOG_WARN("plus de 2 joueurs : negamax ne s'applique pas, "
						 "recherche limitee a 1 coup");
			ai->params[P_PROFONDEUR_MAX] = 1;
		}

		const int32 ok = Search(ai, rules, inst, st, out, me);
		if (!ok) return 0;

		out->elapsedMs = static_cast<uint32>(
			(NkChrono::Now() - ai->start).ToMilliseconds());
		return 1;
	}

	void V_OnMovePlayed(NkcAI, const NkcMove *) {
		// Rien : cette IA ne garde pas d'arbre entre deux coups. Un MCTS le
		// ferait, et c'est precisement le sujet A2.
	}

	void V_Reset(NkcAI self) {
		AI *ai = static_cast<AI *>(self);
		if (ai) { ai->lastNodes = 0; ai->lastDepth = 0; }
	}

	const char *V_GetDebugJson(NkcAI self) {
		AI *ai = static_cast<AI *>(self);
		std::snprintf(ai->debugJson, sizeof(ai->debugJson),
					  "{\"nodes\":%u,\"depth\":%d}",
					  static_cast<unsigned>(ai->lastNodes), ai->lastDepth);
		return ai->debugJson;
	}

	void FillFactory(NkcAIFactory *out) {
		if (!out) return;
		std::memset(out, 0, sizeof(*out));
		std::snprintf(out->info.name, sizeof(out->info.name), "IANegamax");
		std::snprintf(out->info.version, sizeof(out->info.version), "1.0.0");
		std::snprintf(out->info.author, sizeof(out->info.author), "Cours ConquerorLab");
		out->info.difficultyCount = 5;
		out->info.isDeterministic = 1;	 // aucun aleatoire : (etat, params) -> meme coup
		out->info.isThreadSafe	  = 1;	 // aucun etat global : instances independantes

		out->vtable.Create				= V_Create;
		out->vtable.Destroy				= V_Destroy;
		out->vtable.Configure			= V_Configure;
		out->vtable.GetParamsSchemaJson	= V_GetParamsSchemaJson;
		out->vtable.SetParam			= V_SetParam;
		out->vtable.ChooseMove			= V_ChooseMove;
		out->vtable.OnMovePlayed		= V_OnMovePlayed;
		out->vtable.Reset				= V_Reset;
		out->vtable.GetDebugJson		= V_GetDebugJson;

		NkcAIStamp(out);   // versions d'ABI + taille de vtable
	}

} // namespace

NKC_MODULE_EXPORT void nkc_ai_set_allocator(NkcAllocFn a, NkcFreeFn f) { gAlloc = a; gFree = f; }
NKC_MODULE_EXPORT void nkc_ai_get_factory(NkcAIFactory *out) { FillFactory(out); }
NKC_MODULE_LOGGING(ai)
