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
// Auteur : Rihen / Nkentseu 2026
// =============================================================================

namespace nkentseu {

    class NkLauncher
    {
    public:
        /// Ouvre une URL dans le navigateur par defaut du systeme.
        /// @param url URL https://... ou autre protocole supporte par l'OS.
        /// @return true si l'invocation systeme a reussi.
        static bool OpenURL(const char* url) noexcept;

        /// Ouvre un fichier dans l'application par defaut associee a son
        /// extension (ex: .pdf -> visionneuse PDF). Retourne true si OK.
        static bool OpenFile(const char* filePath) noexcept;

        /// Ouvre un dossier dans l'explorateur du systeme (Explorer / Finder /
        /// Nautilus / etc.). Retourne true si OK.
        static bool OpenFolder(const char* folderPath) noexcept;
    };

} // namespace nkentseu
