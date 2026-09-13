#pragma once

// =============================================================================
// NkLauncher.h
// -----------------------------------------------------------------------------
// API portable pour lancer le navigateur / explorateur du systeme sur :
//   - une URL (https://...)
//   - un fichier (ouvrir avec l'app par defaut)
//   - un dossier (ouvrir dans l'explorateur)
//
// Implementations par plateforme :
//   Windows    : ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL)
//   Linux      : system("xdg-open ...")
//   macOS      : system("open ...")
//   Android    : Intent.ACTION_VIEW via JNI (utilise nk_android_global_app)
//   iOS        : UIApplication openURL (Obj-C, branche via .mm)
//   Emscripten : EM_ASM(window.open(...))
//
// Toutes les methodes retournent true si l'invocation systeme a ete declenchee
// avec succes (n'attend pas la reponse du navigateur). false si erreur (URL
// nulle, plateforme non supportee, JNI rate, etc.).
//
// Securite : aucune validation ni sanitization de l'URL -- l'appelant est
// responsable de fournir une URL "trustworthy" (jamais une string venant
// directement d'un user-input non valide, pour eviter command injection sur
// Linux/macOS qui passent par system()).
//
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================

namespace nkentseu {

	class NkLauncher {
		public:
			/// Ouvre une URL dans le navigateur par defaut du systeme.
			/// @param url URL https://... ou autre protocole supporte par l'OS.
			/// @return true si l'invocation systeme a reussi.
			static bool OpenURL(const char *url) noexcept;

			/// Ouvre un fichier dans l'application par defaut associee a son
			/// extension (ex: .pdf -> visionneuse PDF). Retourne true si OK.
			static bool OpenFile(const char *filePath) noexcept;

			/// Ouvre un dossier dans l'explorateur du systeme (Explorer / Finder /
			/// Nautilus / etc.). Retourne true si OK.
			static bool OpenFolder(const char *folderPath) noexcept;

			/// Ouvre le dossier CONTENANT `filePath` en y SELECTIONNANT le fichier, quand
			/// le systeme sait le faire. Sinon, ouvre simplement le dossier.
			/// Ajoutee le 2026-09-05 : c'est la SEULE chose que `NKPlatform/NkShell`
			/// (ecrit le meme jour, supprime depuis) savait faire de plus que ce lanceur.
			/// Deux lanceurs pour le meme service, c'etait un doublon de plus.
			/// @return true si quelque chose a ete ouvert.
			/// @note Windows : `explorer /select,"..."` ; macOS : `open -R`.
			/// @note ⚠️ LINUX : AUCUN STANDARD FREEDESKTOP ne permet de selectionner un
			///       fichier -- chaque gestionnaire a son option. On ouvre le DOSSIER, et
			///       c'est dit ici plutot que decouvert.
			static bool RevealFile(const char *filePath) noexcept;

			/// LE CHEMIN DANS LA FORME QUE LE SYSTEME EXIGE (2026-09-06).
			///
			/// ⚠️ POURQUOI ELLE EST PUBLIQUE PLUTOT QUE CACHEE : les trois fonctions
			///    ci-dessus l'utilisent, mais leur effet -- ce qui part vers
			///    `ShellExecute` -- n'est observable par aucun banc. Une conversion
			///    enfouie serait donc du code que rien ne peut faire rougir. Exposee,
			///    elle se mesure ; et le defaut qu'elle corrige ne se remesure pas a la
			///    main.
			///
			/// **Windows** : les barres OBLIQUES deviennent des CONTRE-OBLIQUES. Le
			/// depot manipule ses chemins en `/` (`NkPath` normalise ainsi), or
			/// `explorer.exe /select,"D:/a/b.png"` est une LIGNE DE COMMANDE : Windows
			/// n'y reconnait pas la barre oblique, abandonne l'analyse et ouvre son
			/// dossier par defaut **en rendant un succes**. C'est le defaut « ouvrir le
			/// dossier ouvre le mauvais dossier ».
			/// **Ailleurs** : copie a l'identique -- `/` EST le separateur.
			///
			/// @return false si le chemin ne tient pas dans `cap` (on REFUSE plutot que
			///         de tronquer : un chemin tronque reste valide et designe autre
			///         chose), ou si un argument est nul.
			static bool ToNativePath(const char *path, char *out, unsigned long long cap) noexcept;
	};

} // namespace nkentseu
