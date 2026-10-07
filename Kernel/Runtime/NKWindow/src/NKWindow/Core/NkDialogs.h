#pragma once

// =============================================================================
// NkDialogs.h
// Boîtes de dialogue natives cross-platform (open/save file, message, couleur).
// Implémentations pour Windows, Linux (Zenity), macOS (osascript), et stubs.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Functional/NkFunction.h"
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
			NkString path;			///< Chemin sélectionné (file dialogs)
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
			static NkDialogResult SaveFileDialog(const NkString &defaultExt = "", const NkString &title = "Save File",
												 const NkString &initialDir = "");

			/**
			 * @brief Ouvre un dialogue de sélection de DOSSIER (façon "Open Folder").
			 * @param title Titre de la boîte de dialogue.
			 * @return path = dossier choisi ; confirmed=false si annulé/non supporté.
			 */
			static NkDialogResult OpenFolderDialog(const NkString &title = "Selectionner un dossier");

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

			// -------------------------------------------------------------------
			// (07/10/2026) UNE FENETRE CACHEE N'OUVRE AUCUNE BOITE DU SYSTEME
			// -------------------------------------------------------------------
			// Une application lancee par une sonde (`NK_FENETRE_CACHEE=1`) cache sa
			// fenetre. Une boite du systeme, elle, surgissait quand meme sur l'ecran,
			// et tenait l'application jusqu'a ce que quelqu'un la ferme (NKCraft, le
			// 07/10 a 09 h 00 : un chemin de projet refuse, trois minutes de boite
			// modale sur l'ecran de travail).
			// La regle vit ICI, a la seule porte par laquelle les applications ouvrent
			// une boite du systeme : bloquee, chaque fonction ECRIT ce qu'elle aurait
			// montre (sortie standard) et rend « non confirme ». Fenetre visible, rien
			// ne change.

			/// Vrai si les boites du systeme sont refusees : `NK_FENETRE_CACHEE` posee
			/// (ni vide ni « 0 »), ou `SetBlocked(true)`.
			static bool Blocked();
			/// L'hote peut l'imposer. `false` rend la main a la variable d'environnement.
			static void SetBlocked(bool blocked);
			/// Combien de boites ont ete refusees depuis le lancement (banc).
			static uint32 RefusedCount();

			// -------------------------------------------------------------------
			// Variantes ASYNCHRONES (callback)
			// -------------------------------------------------------------------
			// Requises sur mobile : iOS présente ses pickers de façon asynchrone
			// (UIDocumentPickerViewController) — une API synchrone bloquerait la
			// runloop et provoquerait un interblocage. Sur desktop, ces variantes
			// enveloppent simplement l'implémentation synchrone (callback immédiat).
			// Le callback est invoqué sur le thread principal.
			using Callback = NkFunction<void(const NkDialogResult &)>;

			static void OpenFileDialogAsync(const Callback &cb, const NkString &filter = "*.*",
											const NkString &title = "Open File");
			static void SaveFileDialogAsync(const Callback &cb, const NkString &defaultExt = "",
											const NkString &title = "Save File");
			static void OpenFolderDialogAsync(const Callback &cb, const NkString &title = "Selectionner un dossier");
	};

} // namespace nkentseu