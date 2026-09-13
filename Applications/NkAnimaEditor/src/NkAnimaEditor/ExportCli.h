#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// ExportCli.h — mode SANS FENÊTRE de NkAnimaEditor : éditer, écrire, relire.
// -----------------------------------------------------------------------------
// Jusqu'au 2026-09-13 l'éditeur ne savait pas écrire : on chargeait, on posait en
// IK, on animait sur la timeline — et rien ne sortait du processus. Ce fichier
// ajoute la sortie ET sa preuve, sans jamais simuler ni souris ni clavier : tout
// passe par des DRAPEAUX de ligne de commande, lus AVANT la création du shell,
// donc sans ouvrir la moindre fenêtre.
//
//   --export=<f.nkanim>   charge le modèle, applique --edits poses-clés scriptées
//                         d'amplitude --amp, écrit <f.nkanim>, et SORT.
//   --verify=<f.nkanim>   processus NEUF : relit SEULEMENT le fichier (aucun
//                         modèle, aucun bake), et SORT.
//   --digest=<f.txt>      empreinte TEXTE des nombres du clip : chaque flottant
//                         en HEXA de son motif binaire — donc comparable au
//                         DERNIER CHIFFRE, sans arrondi décimal.
//   --edits=<n>           nombre de poses-clés à insérer (défaut 3).
//   --amp=<f>             amplitude de l'édition (défaut 1).
//   --mutate=<k>          VOLET NÉGATIF : en mode verify, déplace la clé k APRÈS
//                         relecture. L'empreinte DOIT alors différer ; si elle
//                         reste identique, c'est la comparaison qui ne prouve rien.
//
// Les types restent volontairement primitifs : cet en-tête est inclus par main.cpp,
// qui tire l'Editor Kit (NKCanvas) et ne doit voir ni NKAnima ni NKRenderer.
// =============================================================================

namespace nkanima {

	struct NkExportCliArgs {
			const char *modelPath = nullptr;  // modèle à charger (mode export)
			const char *exportPath = nullptr; // --export=
			const char *verifyPath = nullptr; // --verify=
			const char *digestPath = nullptr; // --digest=
			unsigned int edits = 3;			  // --edits=
			float amp = 1.f;				  // --amp=
			int mutateKey = -1;				  // --mutate=
			// ── Preuves du second lot (2026-09-13) ───────────────────────────
			// --save-button=<f> : édite, pose le chemin, puis appelle `CmdSave` —
			//                     LA fonction que le bouton « Enregistrer » appelle.
			// --save-as=<f>     : appelle `CmdSaveAsConfirmed`, la branche de
			//                     confirmation du sélecteur de fichiers du kit.
			// --scrub=<t>       : pose le curseur à t SANS lecture, puis écrit
			//                     l'empreinte de la POSE (16 flottants par joint).
			// --play-to=<t>     : atteint t EN LISANT, même empreinte. Les deux
			//                     doivent coïncider AU BIT : c'est ce qui dit que le
			//                     scrub est JUSTE, pas seulement qu'il bouge.
			const char *saveButtonPath = nullptr;
			const char *saveAsPath = nullptr;
			float scrubTime = -1.f;
			float playToTime = -1.f;
			// ── Sonde d'EQUILIBRE (2026-09-13) ───────────────────────────────
			// --balance=<t> : pose le curseur à t et écrit dans --digest= le rapport
			//                 complet — régime de masse, COM uniforme ET courant,
			//                 appuis, polygone, verdict, marge. Il lit ce que le
			//                 viewport affiche : la même fonction, pas une copie.
			// --lean=<rad>  : penche le TORSE de tant de radians avant de juger. Une
			//                 pose franchement déséquilibrée, fabriquée sans main.
			// --foot=<f>    : demi-côté de l'empreinte d'un appui, en fraction de la
			//                 taille du personnage (-1 = garder le défaut). À 0, le
			//                 polygone se reduit aux contacts bruts.
			bool balance = false;
			float balanceTime = 0.f;
			float lean = 0.f;
			float footPrint = -1.f;
	};

	// Reconnaît et consomme un argument. true = c'était un drapeau d'export.
	bool ExportCliParseArg(const char *arg, NkExportCliArgs &out);
	// Un mode sans fenêtre a-t-il été demandé ?
	bool ExportCliWanted(const NkExportCliArgs &a);
	// Exécute le mode demandé. 0 = succès ; toute autre valeur = échec (le code
	// dit LEQUEL, et le journal le dit en clair).
	int ExportCliRun(const NkExportCliArgs &a);

} // namespace nkanima
