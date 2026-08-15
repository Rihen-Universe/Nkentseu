// =============================================================================
// benchmark_smoke.cpp — Ce qu'une ligne de journal COÛTE, en nanosecondes.
//
// POURQUOI CE FICHIER A ÉTÉ RÉÉCRIT LE 2026-08-15 :
//   Il existait, la ROADMAP l'annonçait comme « micro-bench du chemin Info() »…
//   et il était ENTIÈREMENT EN COMMENTAIRE. Trente-six lignes dont aucune ne
//   s'exécutait. Un banc qui ne mesure rien est pire qu'un banc absent : on
//   croit la question traitée.
//   Il mesurait par ailleurs le FORMATEUR seul, ce qui n'est pas ce qu'une
//   application paie — l'écriture domine — et se servait de `timespec_get`
//   plutôt que de NKTime.
//
// CE QU'IL MESURE, ET POURQUOI CES TROIS-LÀ :
//   1. vers un puits FICHIER  → ce que paie réellement une ligne émise ;
//   2. vers un puits NUL      → formatage + distribution, sans écriture ; la
//                               différence avec (1) isole le coût d'écriture ;
//   3. ligne FILTRÉE par le niveau → ce que coûte une ligne qu'on n'émet pas.
//      C'est le chiffre qui autorise à laisser des traces dans le code.
//
// POURQUOI PAS LE DÉBIT D'IMAGES : parce qu'on a essayé, et que ça n'a rien
// donné. Mesuré le 15/08 sur `NkCameraDemos --demo=viewer`, le débit varie de
// 35 à 55 img/s d'une exécution à l'autre sur une boucle plafonnée à 60 — soit
// un plancher de bruit de ±33 %. Un agrégat pareil ne peut RIEN résoudre en
// dessous ; le coût d'une ligne s'y noie. On mesure donc là où la dépense se
// produit, pas là où elle se dilue.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

// ⚠️ PAS DE `#include <Unitest/...>` ICI, ET C'EST DÉLIBÉRÉ.
//    Les deux autres fichiers de ce dossier s'appuient sur `Unitest`, cadre
//    fourni par JENGA (et non par ce dépôt : le chercher ici et conclure qu'il
//    n'existe pas serait chercher dans le mauvais référentiel — erreur commise
//    puis corrigée le 2026-08-15). Ils sont donc valides, mais ils ne
//    S'EXÉCUTENT PAS ici : l'exécution des tests est désactivée par politique de
//    workspace (`disableunittestexecution`), décision délibérée de Rodolf que
//    `jenga test` annonce clairement.
//    Ce banc est donc AUTONOME — son propre `main`, aucune dépendance de cadre —
//    pour rester lançable quelle que soit cette politique, sur le modèle de
//    `Kernel/Runtime/NKXR/tests/`. Compilation : `tests/build_bench.sh`.

#include "NKLogger/NkLogger.h"
#include "NKLogger/NkLogLevel.h"
#include "NKLogger/Sinks/NkFileSink.h"
#include "NKLogger/Sinks/NkNullSink.h"
#include "NKTime/NkChrono.h"

#include <cstdio>

using namespace nkentseu;

namespace {

	// Assez d'itérations pour que la granularité de l'horloge disparaisse,
	// assez peu pour que le fichier produit reste de taille raisonnable.
	constexpr int kIters = 20000;

	// Coût moyen d'un appel, en nanosecondes.
	float64 NkMesurer(NkLogger &log, const char *quoi) {
		// Un tour à vide AVANT la mesure : la première ligne paie l'ouverture du
		// fichier et le premier remplissage de cache. La compter reviendrait à
		// facturer l'initialisation à chaque ligne.
		log.Info("amorce");

		const NkElapsedTime t0 = NkChrono::Now();
		for (int i = 0; i < kIters; ++i) {
			log.Info("ligne de banc, ni trop courte ni trop longue");
		}
		const float64 totalNs = (float64)(NkChrono::Now() - t0).ToMicroseconds() * 1000.0;

		const float64 parLigne = totalNs / (float64)kIters;
		::printf("[NKLogger bench] %-28s %8.1f ns / ligne\n", quoi, parLigne);
		return parLigne;
	}

} // namespace

int main() {
	// (1) Puits FICHIER — ce que paie une ligne réellement émise.
	NkLogger versFichier("bench-fichier");
	versFichier.ClearSinks();
	versFichier.AddSink(memory::NkSharedPtr<NkISink>(new NkFileSink("logs/bench_nklogger.log")));
	versFichier.SetLevel(NkLogLevel::NK_INFO);
	const float64 nsFichier = NkMesurer(versFichier, "puits fichier");

	// (2) Puits NUL — formatage et distribution, sans écriture.
	NkLogger versNul("bench-nul");
	versNul.ClearSinks();
	versNul.AddSink(memory::NkSharedPtr<NkISink>(new NkNullSink()));
	versNul.SetLevel(NkLogLevel::NK_INFO);
	const float64 nsNul = NkMesurer(versNul, "puits nul (sans ecriture)");

	// (3) Ligne FILTRÉE — le niveau la rejette avant tout travail.
	NkLogger filtre("bench-filtre");
	filtre.ClearSinks();
	filtre.AddSink(memory::NkSharedPtr<NkISink>(new NkNullSink()));
	filtre.SetLevel(NkLogLevel::NK_ERROR); // INFO passe sous le seuil
	const float64 nsFiltre = NkMesurer(filtre, "ligne filtree (non emise)");

	::printf("[NKLogger bench] part ecriture : %.1f ns / ligne\n", nsFichier - nsNul);

	// Le banc doit MESURER, pas seulement tourner. Une durée nulle signifierait
	// que l'horloge n'a rien vu — cas 5 de la grille, « ça a tourné » pris pour
	// « ça tient ». On ÉCHOUE plutôt que de publier un zéro rassurant.
	int echecs = 0;
	if (!(nsFichier > 0.0)) {
		::printf("[NKLogger bench] ECHEC : duree nulle sur le puits fichier — l'horloge n'a rien vu.\n");
		++echecs;
	}
	if (!(nsNul > 0.0)) {
		::printf("[NKLogger bench] ECHEC : duree nulle sur le puits nul.\n");
		++echecs;
	}
	// Une ligne rejetée par le niveau doit coûter nettement moins qu'une ligne
	// émise — sinon le filtrage se ferait APRÈS le travail, ce qui serait un
	// défaut à part entière.
	if (!(nsFiltre < nsNul)) {
		::printf("[NKLogger bench] ECHEC : une ligne FILTREE (%.1f ns) ne coute pas moins "
				 "qu'une ligne emise (%.1f ns) — le filtrage se ferait APRES le travail.\n",
				 nsFiltre, nsNul);
		++echecs;
	}

	::printf("[NKLogger bench] %s\n", echecs == 0 ? "OK" : "ECHECS DETECTES");
	return echecs == 0 ? 0 : 1;
}
