// =============================================================================
// IAFacile.cpp — LA MEME IA QUE IAGloutonne, ecrite avec ConquerorFacile.h.
//
// A QUOI SERT CE FICHIER
// ----------------------
// A montrer ce que coute le contrat nu, et ce que coute la couche confortable.
// Les deux modules jouent EXACTEMENT le meme jeu — verifie : 20-20 l'un contre
// l'autre, ce qui est le resultat attendu de deux joueurs identiques.
//
//     IAGloutonne.cpp   contrat nu           ~240 lignes
//     IAFacile.cpp      ConquerorFacile.h    ~140 lignes, ce fichier
//
// La difference n'est pas seulement la longueur. C'est qu'ici, la partie qui
// DECIDE tient en quinze lignes qu'on peut lire a voix haute, et le reste est de
// la plomberie de module qu'on recopie sans y penser.
//
// COMMENCEZ PAR CELUI-CI. Lisez IAGloutonne.cpp ensuite, pour voir ce que la
// couche vous epargne — et pour savoir quoi faire le jour ou elle ne suffira
// plus. Elle n'ajoute AUCUNE capacite : tout ce qu'elle fait, on peut le faire
// sans elle, en plus long.
//
// POUR L'UTILISER : copier dans travail/ai/ et sauvegarder.
// =============================================================================

#include "Conqueror/ConquerorAIABI.h"
#include "Conqueror/ConquerorFacile.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <new>

using namespace nkentseu;
using namespace nkentseu::conqueror;
using namespace nkentseu::conqueror::facile;

namespace {

	NkcAllocFn gAlloc = nullptr;
	NkcFreeFn  gFree  = nullptr;

	void *RawAlloc(usize n) { return gAlloc ? gAlloc(n) : std::malloc(n); }
	void  RawFree(void *p)  { if (!p) return; if (gFree) gFree(p); else std::free(p); }

	struct AI {
			int32 poidsTotems = 100;
			int32 poidsMenace = 0;	 ///< a 0 par defaut : a vous de mesurer s'il aide
			char  schema[384];
			char  debug[128];
			int32 examines = 0;
	};

	// =========================================================================
	// NOTER UNE POSITION — la seule partie qui vous appartient vraiment.
	//
	// Comparez avec IAGloutonne.cpp : la meme chose y prend deux fois plus de
	// lignes, et compter les ennemis voisins y demanderait trois boucles
	// imbriquees. Ici c'est un appel.
	// =========================================================================
	int32 Noter(const Plateau &p, uint8 moi, const AI *ai) {
		if (p.Finie()) {
			const int32 gagnant = p.Vainqueur();
			if (gagnant == static_cast<int32>(moi)) return 1000000;
			if (gagnant == -1)						return 0;
			return -1000000;
		}

		int32 note = p.Avantage(moi) * ai->poidsTotems;

		// La « surface de contact » (NOTE_DESIGN §2) : combien de mes totems
		// touchent un ennemi. Le SIGNE de ce terme est une vraie question de
		// conception — offrir du contact, est-ce s'exposer ou attaquer ? C'est
		// pour cela qu'il est a zero par defaut : mesurez, ne devinez pas.
		if (ai->poidsMenace != 0) {
			int32 contact = 0;
			for (Case c : p)
				if (c.AMoi(moi))
					contact += p.CompteVoisins(c.ou, Voisin::Ennemi, moi);
			note -= contact * ai->poidsMenace;
		}
		return note;
	}

	// =========================================================================
	// CHOISIR — quinze lignes, et c'est tout l'algorithme glouton.
	// =========================================================================
	int32 V_ChooseMove(NkcAI self, const NkcRulesVTable *rules, NkcRules inst,
					   const NkcState st, NkcAIResult *out) {
		AI *ai = static_cast<AI *>(self);
		if (!ai || !rules || !inst || !st || !out) return 0;
		std::memset(out, 0, sizeof(*out));

		const Plateau ici(*rules, inst, st);
		const uint8	  moi = ici.QuiJoue();

		Coups<512> liste(*rules, inst, st);
		if (liste.Aucun()) return 0;	// l'atelier jouera PASSER

		// L'essai est cree UNE FOIS, hors de la boucle : en creer un par coup
		// candidat couterait une allocation par candidat.
		Essai essai(*rules, inst);
		if (!essai.Pret()) return 0;

		int32 meilleure = 0;
		int32 choisi	= 0;
		bool  premier	= true;

		for (int32 i = 0; i < liste.Nb(); ++i) {
			if (!essai.Joue(st, liste[i])) continue;	// le moteur a refuse
			const int32 note = Noter(essai.Plateau(), moi, ai);

			// `>` et non `>=` : a note egale on garde le PREMIER. L'ordre des
			// coups est fixe par le moteur, donc ce choix est REPRODUCTIBLE.
			if (premier || note > meilleure) {
				meilleure = note;
				choisi	  = i;
				premier	  = false;
			}
		}

		out->move		  = liste[choisi];
		out->scoreMilli	  = meilleure / 100;
		out->simulations  = static_cast<uint32>(liste.Nb());
		out->depthReached = 1;
		ai->examines	  = liste.Nb();
		return 1;
	}

	// =========================================================================
	// La plomberie du module. On la recopie, on n'y pense plus.
	// =========================================================================
	NkcAI V_Create() {
		void *mem = RawAlloc(sizeof(AI));
		return mem ? static_cast<NkcAI>(new (mem) AI()) : nullptr;
	}

	void V_Destroy(NkcAI self) {
		if (!self) return;
		AI *a = static_cast<AI *>(self);
		a->~AI();
		RawFree(a);
	}

	void V_Configure(NkcAI, const NkcAIConfig *) {}

	const char *V_GetParamsSchemaJson(NkcAI self) {
		AI *a = static_cast<AI *>(self);
		std::snprintf(a->schema, sizeof(a->schema),
					  "[{\"key\":\"poids_totems\",\"label\":\"Poids des totems\","
					  "\"group\":\"Evaluation\",\"type\":\"int\","
					  "\"min\":0,\"max\":1000,\"def\":100,\"val\":%d},"
					  "{\"key\":\"poids_menace\",\"label\":\"Poids de la menace\","
					  "\"group\":\"Evaluation\",\"type\":\"int\","
					  "\"min\":0,\"max\":1000,\"def\":0,\"val\":%d}]",
					  a->poidsTotems, a->poidsMenace);
		return a->schema;
	}

	int32 V_SetParam(NkcAI self, const char *key, float64 value) {
		AI *a = static_cast<AI *>(self);
		if (!a || !key) return 0;
		int32 v = static_cast<int32>(value < 0 ? value - 0.5 : value + 0.5);
		if (v < 0)	  v = 0;	// BORNER, pas refuser (le banc d'essai le verifie)
		if (v > 1000) v = 1000;
		if (std::strcmp(key, "poids_totems") == 0) { a->poidsTotems = v; return 1; }
		if (std::strcmp(key, "poids_menace") == 0) { a->poidsMenace = v; return 1; }
		return 0;
	}

	void V_OnMovePlayed(NkcAI, const NkcMove *) {}
	void V_Reset(NkcAI) {}

	const char *V_GetDebugJson(NkcAI self) {
		AI *a = static_cast<AI *>(self);
		std::snprintf(a->debug, sizeof(a->debug), "{\"coups_examines\":%d}", a->examines);
		return a->debug;
	}

	void FillFactory(NkcAIFactory *out) {
		if (!out) return;
		std::memset(out, 0, sizeof(*out));
		std::snprintf(out->info.name, sizeof(out->info.name), "IAFacile");
		std::snprintf(out->info.version, sizeof(out->info.version), "1.0.0");
		std::snprintf(out->info.author, sizeof(out->info.author), "Cours ConquerorLab");
		out->info.difficultyCount = 1;
		out->info.isDeterministic = 1;
		out->info.isThreadSafe	  = 1;

		out->vtable.Create				= V_Create;
		out->vtable.Destroy				= V_Destroy;
		out->vtable.Configure			= V_Configure;
		out->vtable.GetParamsSchemaJson	= V_GetParamsSchemaJson;
		out->vtable.SetParam			= V_SetParam;
		out->vtable.ChooseMove			= V_ChooseMove;
		out->vtable.OnMovePlayed		= V_OnMovePlayed;
		out->vtable.Reset				= V_Reset;
		out->vtable.GetDebugJson		= V_GetDebugJson;

		NkcAIStamp(out);
	}

} // namespace

NKC_MODULE_EXPORT void nkc_ai_set_allocator(NkcAllocFn a, NkcFreeFn f) { gAlloc = a; gFree = f; }
NKC_MODULE_EXPORT void nkc_ai_get_factory(NkcAIFactory *out) { FillFactory(out); }
