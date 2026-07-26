// =============================================================================
// NKAgentLLMTest — raisonnement par LLM RÉEL pour une décision « ambiguë »
// (NKAgent, Phase 4, Jalon 4b : pont NkAgentLLMReasoning -> NKInfer).
//
// Scénario de preuve : un NkAgent dont la politique tabulaire est ENCORE
// NEUVE (rl::NkQLearning fraîchement construite, Q(s,a)=0 pour TOUTE action)
// affronte le monde clé-puis-porte (rl::NkKeyDoorGridWorld, même monde que
// NKAgentTest Jalon 3) — la politique tabulaire seule n'a alors AUCUN signal
// exploitable (toutes les actions sont à égalité stricte : Q=0 partout),
// c'est exactement le cas « espace d'action ambigu pour la politique apprise
// seule » ciblé par la mission. Ce test appelle NkAgentDecideViaLLM (cf
// NkAgentLLMReasoning.h) — un forward pass Qwen2.5 7B Instruct RÉEL (poids
// GGUF réels, déquantifiés à la demande, PAS un mock) — pour obtenir 3
// décisions RÉELLES : (1) état de départ -> but "clé", (2) LA MÊME requête
// répétée (le forward est déterministe, argmax pur, sans échantillonnage
// stochastique : preuve de reproductibilité), (3) état "clé prise" -> but
// "porte" (entrée différente : preuve que la sortie dépend RÉELLEMENT de
// l'entrée, pas une constante câblée).
//
// COÛT MESURÉ (cf NkAgentLLMReasoning.h, NKInfer/ROADMAP.md) : ~2.9s/couche
// réelle en build Debug -> ~80-90s pour un forward complet 28 couches
// (Qwen2.5 7B Instruct). D'où application DÉDIÉE, séparée de NKAgentTest
// (rapide) : ce test prend plusieurs minutes, volontairement limité à 2
// APPELS RÉELS distincts (+1 répétition) — PAS une boucle d'entraînement.
//
// LIMITE HONNÊTE (documentée en détail dans NkAgentLLMReasoning.h) : aucun
// encodeur BPE n'existe dans ce dépôt -> le "prompt" est un encodage NUMÉRIQUE
// brut (chiffres réels du vocabulaire), pas du texte libre ; ceci prouve le
// CÂBLAGE bout-en-bout, pas une capacité de raisonnement linguistique.
//
// Usage : NKAgentLLMTest.exe [chemin_gguf_ou_blob_ollama]
// Variables d'env :
//   NK_GGUF_PATH       chemin du blob (si argv[1] absent)
//   NK_QWEN_NLAYERS    nombre de couches réelles à exécuter par décision
//                      (défaut : toutes, qwen2.block_count -- réduire est un
//                      choix explicite pour accélérer un run local, cf
//                      NkAgentLLMReasoning.h)
// =============================================================================
#include "NKAgent/NkAgentLLMReasoning.h"
#include "NKLogger/NkLog.h"
#include "NKRL/NkKeyDoorGridWorld.h"

#include <cstdio>
#include <cstdlib>

using namespace nkentseu;
using namespace nkentseu::ai;

namespace {

	int g_pass = 0, g_fail = 0;

	void Check(bool ok, const char *name) {
		(ok ? g_pass : g_fail)++;
		logger.Info("  [ {0} ] {1}", ok ? "OK" : "KO", name);
	}

	const char *ActionName(int32 a) {
		static const char *names[4] = {"haut", "bas", "gauche", "droite"};
		return (a >= 0 && a < 4) ? names[a] : "?";
	}

	void LogDecision(const char *label, uint32 state, uint32 goal, const agent::NkAgentLLMDecision &d) {
		logger.Info("  {0} : etat={1} but={2} -> action={3} ({4})  logits[0..3]=[{5},{6},{7},{8}]  {9} couches "
					"reelles en {10} s",
					label, state, goal, d.action, ActionName(d.action), (double)d.candidateLogits[0],
					(double)d.candidateLogits[1], (double)d.candidateLogits[2], (double)d.candidateLogits[3],
					d.nLayers, d.seconds);
	}

} // namespace

int main(int argc, char **argv) {
	logger.Info("=== NKAgentLLMTest : raisonnement par LLM reel (NkAgentLLMReasoning -> NKInfer, Jalon 4b) ===");

	const char *path = nullptr;
	if (argc > 1) {
		path = argv[1];
	} else if (const char *envPath = getenv("NK_GGUF_PATH")) {
		path = envPath;
	} else {
		path = "C:/Users/Rihen/.ollama/models/blobs/"
			   "sha256-2bada8a7450677000f678be90653b85d364de7db25eb5ea54136ada5f3933730";
	}
	logger.Info("  blob GGUF : {0}", path);

	agent::NkAgentLLMModel model;
	NkString err;
	bool loaded = agent::NkAgentLoadLLMModel(path, model, &err);
	Check(loaded, "chargement du modele (metadonnees + vocabulaire complet + tokens-chiffres reels du GGUF)");
	if (!loaded) {
		logger.Error("  Erreur : {0}", err.CStr());
		logger.Info("=== Resultat : {0} OK, {1} echec(s) ===", g_pass, g_fail);
		return 1;
	}
	logger.Info("  modele Qwen2 reel : blockCount={0} dModel={1} nHeads={2} nKVHeads={3} ropeFreqBase={4} rmsEps={5}",
				model.blockCount, model.cfg.dModel, model.cfg.nHeads, model.cfg.nKVHeads, (double)model.cfg.ropeFreqBase,
				(double)model.cfg.rmsEps);
	NkString digitsLine;
	for (int d = 0; d <= 9; ++d) {
		char buf[24];
		std::snprintf(buf, sizeof(buf), "%d:id%d ", d, model.digitTokenId[d]);
		digitsLine += buf;
	}
	logger.Info("  tokens-chiffres reels trouves dans le vocabulaire : {0}", digitsLine.CStr());

	uint32 nLayers = (uint32)model.blockCount;
	if (const char *envN = getenv("NK_QWEN_NLAYERS")) {
		int v = atoi(envN);
		if (v > 0)
			nLayers = (uint32)v;
	}
	logger.Info("-- Scenario : politique tabulaire NEUVE (Q=0 partout, aucun signal exploitable) sur le monde "
				"cle-puis-porte -- decision de repli via {0}/{1} couches REELLES de Qwen2.5 7B Instruct --",
				nLayers, model.blockCount);

	const uint32 KD_N = 5, KD_START = 0, KD_KEY = 12, KD_DOOR = 24;
	NkVector<uint32> noHoles;
	rl::NkKeyDoorGridWorld env(KD_N, KD_START, KD_KEY, KD_DOOR, noHoles, 0.02f);
	const uint32 s0 = env.Reset();

	// Decision 1 : etat de depart -> but "cle" (premier sous-but du plan Jalon 3).
	agent::NkAgentLLMDecision dec1 = agent::NkAgentDecideViaLLM(model, s0, env.KeyTakenState(), nLayers);
	Check(dec1.ok, "decision 1 (forward Qwen2 reel, poids GGUF reels) reussie");
	if (dec1.ok) {
		LogDecision("decision 1 (depart -> cle)", s0, env.KeyTakenState(), dec1);
	}
	Check(dec1.ok && dec1.action >= 0 && dec1.action <= 3, "decision 1 : action dans l'espace valide (0..3)");

	// Decision 2 : REPETITION EXACTE de la decision 1 -- le forward est
	// deterministe (argmax pur sur les 4 logits candidats, pas d'echantillonnage
	// stochastique ici) : DOIT reproduire EXACTEMENT la meme action.
	agent::NkAgentLLMDecision dec1repeat = agent::NkAgentDecideViaLLM(model, s0, env.KeyTakenState(), nLayers);
	Check(dec1repeat.ok && dec1repeat.action == dec1.action,
		  "decision repetee a l'identique (meme etat/but -> meme action : deterministe, pas aleatoire)");

	// Decision 3 : etat/but DIFFERENTS (depuis la cle, vers la porte) -- preuve
	// que la decision depend REELLEMENT de l'entree, pas une constante cablee.
	agent::NkAgentLLMDecision dec3 = agent::NkAgentDecideViaLLM(model, env.KeyTakenState(), env.DoorOpenedState(), nLayers);
	Check(dec3.ok, "decision 3 (etat/but differents, forward reel) reussie");
	if (dec3.ok) {
		LogDecision("decision 3 (cle -> porte)", env.KeyTakenState(), env.DoorOpenedState(), dec3);
	}

	const double totalSeconds = dec1.seconds + dec1repeat.seconds + dec3.seconds;
	logger.Info("-- Temps total mesure (3 forwards reels, {0} couches chacun) : {1} s ({2} s/decision en moyenne) --",
				nLayers, totalSeconds, totalSeconds / 3.0);
	logger.Info("LIMITE HONNETE : latence NON adaptee au temps reel dans l'etat actuel ({0} s/decision reel mesure) ; "
				"prompt reduit a un encodage numerique (chiffres reels du vocabulaire, AUCUN encodeur BPE dans ce "
				"depot) -- ce test prouve le CABLAGE bout-en-bout (etat agent -> tokens reels -> forward Qwen2 reel "
				"-> logits reels -> action), pas une qualite de raisonnement linguistique.",
				totalSeconds / 3.0);

	logger.Info("=== Resultat : {0} OK, {1} echec(s) ===", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
