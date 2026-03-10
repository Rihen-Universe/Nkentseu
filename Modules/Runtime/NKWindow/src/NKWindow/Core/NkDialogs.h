#pragma once

// =============================================================================
// NkDialogs.h
// Boîtes de dialogue natives cross-platform (open/save file, message, couleur).
// Implémentations pour Windows, Linux (Zenity), macOS (osascript), et stubs.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NkTypes.h"
#include "NKContainers/String/NkString.h"
#include <string>
#include <vector>
#include <cstdlib> // pour system()
#include <cstdio>  // pour popen()

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

	// ---------------------------------------------------------------------------
	// Résultat d'un dialogue
	// ---------------------------------------------------------------------------

	struct NkDialogResult {
		bool confirmed = false; ///< true si l'utilisateur a validé
		NkString path;		///< Chemin sélectionné (file dialogs)
		uint32 color = 0;		///< Couleur choisie RGBA (color picker)
	};

	// ---------------------------------------------------------------------------
	// NkDialogs — interface statique
	// ---------------------------------------------------------------------------

	class NkDialogs {
		public:
			/**
			 * @brief Ouvre un dialogue de sélection de fichier.
			 * @param filter Filtre type "*.png;*.jpg" (peut être vide pour tous)
			 * @param title  Titre de la boîte de dialogue.
			 */
			static NkDialogResult OpenFileDialog(const NkString &filter = "*.*", const NkString &title = "Open File");

			/**
			 * @brief Ouvre un dialogue de sauvegarde de fichier.
			 * @param defaultExt Extension par défaut sans point (ex: "png")
			 * @param title  Titre de la boîte de dialogue.
			 */
			static NkDialogResult SaveFileDialog(const NkString &defaultExt = "", const NkString &title = "Save File");

			/**
			 * @brief Affiche une boîte de message.
			 * @param message Corps du message.
			 * @param title   Titre de la fenêtre.
			 * @param type    0=info, 1=warning, 2=error
			 */
			static void OpenMessageBox(const NkString &message, const NkString &title = "Message", int type = 0);

			/**
			 * @brief Ouvre un sélecteur de couleur.
			 * @param initial Couleur initiale RGBA.
			 */
			static NkDialogResult ColorPicker(uint32 initial = 0xFFFFFFFF);
	};

} // namespace nkentseu