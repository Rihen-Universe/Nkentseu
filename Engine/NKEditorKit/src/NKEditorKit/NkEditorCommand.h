#pragma once
// -----------------------------------------------------------------------------
// @File    NkEditorCommand.h
// @Brief   Commande nommee + registre, base de la palette de commandes (Ctrl+P).
// @Author  Rihen
// @License Proprietary - Free to use and modify
//
// Inspiration VSCode : toute action de l'editeur est une commande nommee,
// invocable depuis la palette (Ctrl+P), un menu ou un raccourci. v1 : execution
// par clic / fleches+Entree dans la palette (le filtrage par frappe arrivera
// avec l'integration clavier complete). C'est aussi le point d'ancrage futur des
// extensions (NKCode/Extensions) qui enregistreront leurs propres commandes.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkEditorExport.h"

namespace nkentseu {
    namespace editorkit {

        // Callback d'une commande. `user` = donnee opaque fournie a l'enregistrement.
        using NkEditorCommandFn = void(*)(void* user);

        struct NKEDITORKIT_API NkEditorCommand {
            char              name[64]     = {};       ///< libelle affiche dans la palette
            char              shortcut[24] = {};       ///< raccourci affiche (cosmetique)
            NkEditorCommandFn fn           = nullptr;  ///< action
            void*             user         = nullptr;  ///< contexte passe a fn
        };

    } // namespace editorkit
} // namespace nkentseu
