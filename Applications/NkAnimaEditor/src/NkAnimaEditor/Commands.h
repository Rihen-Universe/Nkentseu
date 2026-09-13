#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Commands.h — LES fonctions que les commandes et les boutons appellent.
// -----------------------------------------------------------------------------
// Même découpe que NKCode (`Shell/NkAppCommands.h`) : le shell enregistre des
// fonctions `void(void*)`, l'application les écrit ici, et la barre d'outils
// appelle EXACTEMENT les mêmes.
//
// ⚠️ POURQUOI CE FICHIER N'INCLUT NI LE KIT NI NKAnima : c'est ici que passe la
// PREUVE. Le mode sans fenêtre (ExportCli.cpp) appelle ces fonctions-là, celles
// que le bouton appelle — pas un chemin parallèle, et JAMAIS un clic simulé.
// Si ce fichier tirait l'Editor Kit, ExportCli.cpp ne pourrait pas l'inclure
// (conflit de types NKRenderer/NKCanvas, cf. tête d'AnimBridge.h) et la preuve
// devrait dupliquer le geste — c'est-à-dire ne plus rien prouver.
// =============================================================================
#include "AnimBridge.h"

namespace nkanima {

	// « Fichier: Enregistrer » (Ctrl+S) et le bouton « Enregistrer ».
	inline void CmdSave(void * = nullptr) {
		AnimSave();
	}

	// « Fichier: Enregistrer sous... » une fois le chemin CHOISI : c'est la branche
	// de confirmation du sélecteur du kit, nommée pour être appelable sans lui.
	inline void CmdSaveAsConfirmed(const char *path) {
		AnimSetSavePath(path);
		AnimSave();
	}

} // namespace nkanima
