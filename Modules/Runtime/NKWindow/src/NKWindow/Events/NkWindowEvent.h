#pragma once

// =============================================================================
// NkWindowEvent.h
// Hiérarchie complète des événements fenêtre.
//
// Contenu :
//   NkWindowTheme          — énumération des thèmes système
//   NkWindowState          — énumération des états de fenêtre
//   NkWindowEvent          — base abstraite (catégorie WINDOW)
//   NkWindowCreateEvent    — fenêtre créée
//   NkWindowCloseEvent     — requête de fermeture (pas une fermeture forcée)
//   NkWindowDestroyEvent   — fenêtre détruite
//   NkWindowPaintEvent     — demande de repaint (zone sale optionnelle)
//   NkWindowResizeEvent    — redimensionnement avec ancienne taille
//   NkWindowResizeBeginEvent / NkWindowResizeEndEvent
//   NkWindowMoveEvent      — déplacement avec ancienne position
//   NkWindowMoveBeginEvent / NkWindowMoveEndEvent
//   NkWindowFocusGainedEvent / NkWindowFocusLostEvent
//   NkWindowMinimizeEvent / NkWindowMaximizeEvent / NkWindowRestoreEvent
//   NkWindowFullscreenEvent / NkWindowWindowedEvent
//   NkWindowDpiEvent       — changement de DPI / scaling
//   NkWindowThemeEvent     — changement de thème système
//   NkWindowShownEvent / NkWindowHiddenEvent
// =============================================================================

#include "NkEvent.h"
#include "NKContainers/String/NkStringUtils.h"
#include "NkEventState.h"
#include <string>

namespace nkentseu {

    // =========================================================================
    // NkWindowTheme — thème système actif
    // =========================================================================

    enum class NkWindowTheme : NkU32 {
        NK_THEME_UNKNOWN       = 0,
        NK_THEME_LIGHT,         ///< Thème clair
        NK_THEME_DARK,          ///< Thème sombre
        NK_THEME_HIGH_CONTRAST  ///< Mode contraste élevé (accessibilité)
    };

    /// @brief Convertit un NkWindowTheme en string lisible
    inline const char* NkWindowThemeToString(NkWindowTheme t) noexcept {
        switch (t) {
            case NkWindowTheme::NK_THEME_LIGHT:         return "LIGHT";
            case NkWindowTheme::NK_THEME_DARK:          return "DARK";
            case NkWindowTheme::NK_THEME_HIGH_CONTRAST: return "HIGH_CONTRAST";
            default:                                     return "UNKNOWN";
        }
    }

    // =========================================================================
    // NkWindowState — état courant de la fenêtre
    // =========================================================================

    /// @brief Convertit un NkWindowState en string lisible
    inline const char* NkWindowStateToString(NkU32 code) noexcept {
        switch (code) {
            case 0:  return "UNDEFINED";    // NK_WINDOW_UNDEFINED
            case 1:  return "CREATED";      // NK_WINDOW_CREATED
            case 2:  return "MINIMIZED";    // NK_WINDOW_MINIMIZED
            case 3:  return "MAXIMIZED";    // NK_WINDOW_MAXIMIZED
            case 4:  return "FULLSCREEN";   // NK_WINDOW_FULLSCREEN
            case 5:  return "HIDDEN";       // NK_WINDOW_HIDDEN
            case 6:  return "CLOSED";       // NK_WINDOW_CLOSED
            default: return "UNKNOWN";
        }
    }

    // =========================================================================
    // NkWindowEvent — classe de base abstraite pour tous les événements fenêtre
    //
    // Catégorie : NK_CAT_WINDOW
    // Le windowId est transmis au constructeur de NkEvent.
    // =========================================================================

    class NkWindowEvent : public NkEvent {
        public:
            /// @brief Retourne la catégorie fenêtre
            NkU32 GetCategoryFlags() const override {
                return static_cast<NkU32>(NkEventCategory::NK_CAT_WINDOW);
            }

        protected:
            /// @brief Construit avec un identifiant de fenêtre
            explicit NkWindowEvent(NkU64 windowId = 0) noexcept
                : NkEvent(windowId) {}
    };

    // =========================================================================
    // NkWindowCreateEvent — fenêtre créée avec ses dimensions initiales
    // =========================================================================

    class NkWindowCreateEvent final : public NkWindowEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_WINDOW_CREATE)

            /// @param width    Largeur initiale en pixels
            /// @param height   Hauteur initiale en pixels
            /// @param windowId Identifiant de la fenêtre créée
            NkWindowCreateEvent(NkU32 width = 0, NkU32 height = 0, NkU64 windowId = 0) noexcept
                : NkWindowEvent(windowId), mWidth(width), mHeight(height) {}

            NkEvent* Clone() const override { return new NkWindowCreateEvent(*this); }

            NkString ToString() const override {
                return NkString::Fmt("WindowCreate({0}x{1})", mWidth, mHeight);
            }

            /// @brief Largeur initiale de la fenêtre (pixels)
            NkU32 GetWidth()  const noexcept { return mWidth; }
            /// @brief Hauteur initiale de la fenêtre (pixels)
            NkU32 GetHeight() const noexcept { return mHeight; }

        private:
            NkU32 mWidth  = 0; ///< Largeur initiale
            NkU32 mHeight = 0; ///< Hauteur initiale
    };

    // =========================================================================
    // NkWindowCloseEvent — requête de fermeture
    //
    // IMPORTANT : Ceci est une REQUÊTE, pas une fermeture forcée.
    // L'application doit écouter cet événement et décider si elle accepte.
    // Si l'événement est consommé (MarkHandled()), la fermeture est annulée.
    // Une fermeture forcée (ex: shutdown OS) est signalée par mForced = true.
    // =========================================================================

    class NkWindowCloseEvent final : public NkWindowEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_WINDOW_CLOSE)

            /// @param forced   true si la fermeture vient du système (non annulable)
            /// @param windowId Identifiant de la fenêtre
            explicit NkWindowCloseEvent(bool forced = false, NkU64 windowId = 0) noexcept
                : NkWindowEvent(windowId), mForced(forced) {}

            NkEvent* Clone() const override { return new NkWindowCloseEvent(*this); }

            NkString ToString() const override {
                return NkString("WindowClose(forced=") + (mForced ? "true" : "false") + ")";
            }

            /// @brief Retourne true si la fermeture est imposée par le système (non annulable)
            bool IsForced() const noexcept { return mForced; }

        private:
            bool mForced = false; ///< Fermeture imposée par l'OS (shutdown, etc.)
    };

    // =========================================================================
    // NkWindowDestroyEvent — fenêtre définitivement détruite
    //
    // Émis après NkWindowCloseEvent, quand la fenêtre est effectivement détruite.
    // Ne pas accéder aux ressources de la fenêtre après cet événement.
    // =========================================================================

    class NkWindowDestroyEvent final : public NkWindowEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_WINDOW_DESTROY)

            explicit NkWindowDestroyEvent(NkU64 windowId = 0) noexcept
                : NkWindowEvent(windowId) {}

            NkEvent* Clone() const override { return new NkWindowDestroyEvent(*this); }

            NkString ToString() const override { return "WindowDestroy()"; }
    };

    // =========================================================================
    // NkWindowPaintEvent — demande de repaint
    //
    // Si IsFullPaint() retourne true, toute la fenêtre doit être redessinée.
    // Sinon, seule la zone sale (DirtyX/Y/W/H) a besoin d'être mise à jour.
    // =========================================================================

    class NkWindowPaintEvent final : public NkWindowEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_WINDOW_PAINT)

            /// @brief Repaint complet de la fenêtre
            explicit NkWindowPaintEvent(NkU64 windowId = 0) noexcept
                : NkWindowEvent(windowId)
                , mDirtyX(0), mDirtyY(0), mDirtyW(0), mDirtyH(0)
                , mFull(true)
            {}

            /// @brief Repaint d'une zone rectangulaire spécifique (zone sale)
            NkWindowPaintEvent(NkI32 x, NkI32 y, NkU32 w, NkU32 h, NkU64 windowId = 0) noexcept
                : NkWindowEvent(windowId)
                , mDirtyX(x), mDirtyY(y), mDirtyW(w), mDirtyH(h)
                , mFull(false)
            {}

            NkEvent* Clone() const override { return new NkWindowPaintEvent(*this); }

            NkString ToString() const override {
                if (mFull) return "WindowPaint(full)";
                return NkString::Fmt("WindowPaint({0},{1} {2}x{3})",
                    mDirtyX, mDirtyY, mDirtyW, mDirtyH);
            }

            /// @brief Retourne true si toute la fenêtre doit être redessinée
            bool  IsFullPaint() const noexcept { return mFull;    }
            /// @brief Coordonnée X de la zone sale
            NkI32 GetDirtyX()   const noexcept { return mDirtyX;  }
            /// @brief Coordonnée Y de la zone sale
            NkI32 GetDirtyY()   const noexcept { return mDirtyY;  }
            /// @brief Largeur de la zone sale
            NkU32 GetDirtyW()   const noexcept { return mDirtyW;  }
            /// @brief Hauteur de la zone sale
            NkU32 GetDirtyH()   const noexcept { return mDirtyH;  }

        private:
            NkI32 mDirtyX = 0; ///< Origine X de la zone invalide
            NkI32 mDirtyY = 0; ///< Origine Y de la zone invalide
            NkU32 mDirtyW = 0; ///< Largeur de la zone invalide
            NkU32 mDirtyH = 0; ///< Hauteur de la zone invalide
            bool  mFull   = true; ///< Repaint complet ?
    };

    // =========================================================================
    // NkWindowResizeEvent — redimensionnement avec comparaison avant/après
    // =========================================================================

    class NkWindowResizeEvent final : public NkWindowEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_WINDOW_RESIZE)

            /// @param w / h         Nouvelle taille
            /// @param prevW / prevH Ancienne taille (0 si inconnue)
            NkWindowResizeEvent(NkU32 w = 0, NkU32 h = 0,
                                NkU32 prevW = 0, NkU32 prevH = 0,
                                NkU64 windowId = 0) noexcept
                : NkWindowEvent(windowId)
                , mWidth(w), mHeight(h)
                , mPrevWidth(prevW), mPrevHeight(prevH)
            {}

            NkEvent* Clone() const override { return new NkWindowResizeEvent(*this); }

            NkString ToString() const override {
                return NkString::Fmt("WindowResize({0}x{1} prev={2}x{3})",
                    mWidth, mHeight, mPrevWidth, mPrevHeight);
            }

            NkU32 GetWidth()      const noexcept { return mWidth;      }
            NkU32 GetHeight()     const noexcept { return mHeight;     }
            NkU32 GetPrevWidth()  const noexcept { return mPrevWidth;  }
            NkU32 GetPrevHeight() const noexcept { return mPrevHeight; }

            /// @brief Retourne true si la fenêtre a rétréci dans au moins une dimension
            bool GotSmaller() const noexcept {
                return mWidth < mPrevWidth || mHeight < mPrevHeight;
            }
            /// @brief Retourne true si la fenêtre a grandi dans au moins une dimension
            bool GotLarger() const noexcept {
                return mWidth > mPrevWidth || mHeight > mPrevHeight;
            }

        private:
            NkU32 mWidth      = 0; ///< Nouvelle largeur
            NkU32 mHeight     = 0; ///< Nouvelle hauteur
            NkU32 mPrevWidth  = 0; ///< Ancienne largeur
            NkU32 mPrevHeight = 0; ///< Ancienne hauteur
    };

    // =========================================================================
    // NkWindowResizeBeginEvent / NkWindowResizeEndEvent
    // Émis au début et à la fin d'une opération de redimensionnement.
    // =========================================================================

    class NkWindowResizeBeginEvent final : public NkWindowEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_WINDOW_RESIZE_BEGIN)
            explicit NkWindowResizeBeginEvent(NkU64 windowId = 0) noexcept
                : NkWindowEvent(windowId) {}
            NkEvent*    Clone()    const override { return new NkWindowResizeBeginEvent(*this); }
            NkString ToString() const override { return "WindowResizeBegin()"; }
    };

    class NkWindowResizeEndEvent final : public NkWindowEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_WINDOW_RESIZE_END)
            explicit NkWindowResizeEndEvent(NkU64 windowId = 0) noexcept
                : NkWindowEvent(windowId) {}
            NkEvent*    Clone()    const override { return new NkWindowResizeEndEvent(*this); }
            NkString ToString() const override { return "WindowResizeEnd()"; }
    };

    // =========================================================================
    // NkWindowMoveEvent — déplacement avec ancienne position
    // =========================================================================

    class NkWindowMoveEvent final : public NkWindowEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_WINDOW_MOVE)

            /// @param x / y         Nouvelle position (coin supérieur gauche, écran)
            /// @param prevX / prevY Ancienne position (0,0 si inconnue)
            NkWindowMoveEvent(NkI32 x = 0, NkI32 y = 0,
                            NkI32 prevX = 0, NkI32 prevY = 0,
                            NkU64 windowId = 0) noexcept
                : NkWindowEvent(windowId)
                , mX(x), mY(y), mPrevX(prevX), mPrevY(prevY)
            {}

            NkEvent* Clone() const override { return new NkWindowMoveEvent(*this); }

            NkString ToString() const override {
                return NkString::Fmt("WindowMove({0},{1} prev={2},{3})",
                    mX, mY, mPrevX, mPrevY);
            }

            NkI32 GetX()     const noexcept { return mX;     }
            NkI32 GetY()     const noexcept { return mY;     }
            NkI32 GetPrevX() const noexcept { return mPrevX; }
            NkI32 GetPrevY() const noexcept { return mPrevY; }
            /// @brief Delta horizontal depuis la dernière position
            NkI32 GetDeltaX() const noexcept { return mX - mPrevX; }
            /// @brief Delta vertical depuis la dernière position
            NkI32 GetDeltaY() const noexcept { return mY - mPrevY; }

        private:
            NkI32 mX     = 0; ///< Nouvelle position X
            NkI32 mY     = 0; ///< Nouvelle position Y
            NkI32 mPrevX = 0; ///< Ancienne position X
            NkI32 mPrevY = 0; ///< Ancienne position Y
    };

    // =========================================================================
    // NkWindowMoveBeginEvent / NkWindowMoveEndEvent
    // =========================================================================

    class NkWindowMoveBeginEvent final : public NkWindowEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_WINDOW_MOVE_BEGIN)
            explicit NkWindowMoveBeginEvent(NkU64 windowId = 0) noexcept
                : NkWindowEvent(windowId) {}
            NkEvent*    Clone()    const override { return new NkWindowMoveBeginEvent(*this); }
            NkString ToString() const override { return "WindowMoveBegin()"; }
    };

    class NkWindowMoveEndEvent final : public NkWindowEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_WINDOW_MOVE_END)
            explicit NkWindowMoveEndEvent(NkU64 windowId = 0) noexcept
                : NkWindowEvent(windowId) {}
            NkEvent*    Clone()    const override { return new NkWindowMoveEndEvent(*this); }
            NkString ToString() const override { return "WindowMoveEnd()"; }
    };

    // =========================================================================
    // NkWindowFocusGainedEvent / NkWindowFocusLostEvent
    // =========================================================================

    class NkWindowFocusGainedEvent final : public NkWindowEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_WINDOW_FOCUS_GAINED)
            explicit NkWindowFocusGainedEvent(NkU64 windowId = 0) noexcept
                : NkWindowEvent(windowId) {}
            NkEvent*    Clone()    const override { return new NkWindowFocusGainedEvent(*this); }
            NkString ToString() const override { return "WindowFocusGained()"; }
    };

    class NkWindowFocusLostEvent final : public NkWindowEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_WINDOW_FOCUS_LOST)
            explicit NkWindowFocusLostEvent(NkU64 windowId = 0) noexcept
                : NkWindowEvent(windowId) {}
            NkEvent*    Clone()    const override { return new NkWindowFocusLostEvent(*this); }
            NkString ToString() const override { return "WindowFocusLost()"; }
    };

    // =========================================================================
    // NkWindowMinimizeEvent / NkWindowMaximizeEvent / NkWindowRestoreEvent
    // =========================================================================

    class NkWindowMinimizeEvent final : public NkWindowEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_WINDOW_MINIMIZE)
            explicit NkWindowMinimizeEvent(NkU64 windowId = 0) noexcept
                : NkWindowEvent(windowId) {}
            NkEvent*    Clone()    const override { return new NkWindowMinimizeEvent(*this); }
            NkString ToString() const override { return "WindowMinimize()"; }
    };

    class NkWindowMaximizeEvent final : public NkWindowEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_WINDOW_MAXIMIZE)
            explicit NkWindowMaximizeEvent(NkU64 windowId = 0) noexcept
                : NkWindowEvent(windowId) {}
            NkEvent*    Clone()    const override { return new NkWindowMaximizeEvent(*this); }
            NkString ToString() const override { return "WindowMaximize()"; }
    };

    class NkWindowRestoreEvent final : public NkWindowEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_WINDOW_RESTORE)
            explicit NkWindowRestoreEvent(NkU64 windowId = 0) noexcept
                : NkWindowEvent(windowId) {}
            NkEvent*    Clone()    const override { return new NkWindowRestoreEvent(*this); }
            NkString ToString() const override { return "WindowRestore()"; }
    };

    // =========================================================================
    // NkWindowFullscreenEvent / NkWindowWindowedEvent
    // =========================================================================

    class NkWindowFullscreenEvent final : public NkWindowEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_WINDOW_FULLSCREEN)
            explicit NkWindowFullscreenEvent(NkU64 windowId = 0) noexcept
                : NkWindowEvent(windowId) {}
            NkEvent*    Clone()    const override { return new NkWindowFullscreenEvent(*this); }
            NkString ToString() const override { return "WindowFullscreen()"; }
    };

    class NkWindowWindowedEvent final : public NkWindowEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_WINDOW_WINDOWED)
            explicit NkWindowWindowedEvent(NkU64 windowId = 0) noexcept
                : NkWindowEvent(windowId) {}
            NkEvent*    Clone()    const override { return new NkWindowWindowedEvent(*this); }
            NkString ToString() const override { return "WindowWindowed()"; }
    };

    // =========================================================================
    // NkWindowDpiEvent — changement de DPI / facteur d'échelle
    // =========================================================================

    class NkWindowDpiEvent final : public NkWindowEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_WINDOW_DPI_CHANGE)

            /// @param scale     Nouveau facteur d'échelle (ex: 1.5 = 150%)
            /// @param prevScale Ancien facteur d'échelle
            /// @param dpi       Valeur DPI physique (ex: 144 pour 150% sur 96 DPI)
            NkWindowDpiEvent(NkF32 scale     = 1.0f,
                            NkF32 prevScale = 1.0f,
                            NkU32 dpi       = 96,
                            NkU64 windowId  = 0) noexcept
                : NkWindowEvent(windowId)
                , mScale(scale), mPrevScale(prevScale), mDpi(dpi)
            {}

            NkEvent* Clone() const override { return new NkWindowDpiEvent(*this); }

            NkString ToString() const override {
                return NkString::Fmt("WindowDpi(scale={0:.3} dpi={1})", mScale, mDpi);
            }

            /// @brief Nouveau facteur d'échelle
            NkF32 GetScale()     const noexcept { return mScale;     }
            /// @brief Ancien facteur d'échelle
            NkF32 GetPrevScale() const noexcept { return mPrevScale; }
            /// @brief Valeur DPI physique
            NkU32 GetDpi()       const noexcept { return mDpi;       }

        private:
            NkF32 mScale     = 1.0f; ///< Nouveau facteur d'échelle
            NkF32 mPrevScale = 1.0f; ///< Ancien facteur d'échelle
            NkU32 mDpi       = 96;   ///< Valeur DPI physique
    };

    // =========================================================================
    // NkWindowThemeEvent — changement de thème système
    // =========================================================================

    class NkWindowThemeEvent final : public NkWindowEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_WINDOW_THEME_CHANGE)

            explicit NkWindowThemeEvent(NkWindowTheme theme   = NkWindowTheme::NK_THEME_UNKNOWN,
                                        NkU64         windowId = 0) noexcept
                : NkWindowEvent(windowId), mTheme(theme) {}

            NkEvent* Clone() const override { return new NkWindowThemeEvent(*this); }

            NkString ToString() const override {
                return NkString("WindowTheme(") + NkWindowThemeToString(mTheme) + ")";
            }

            /// @brief Thème système actif
            NkWindowTheme GetTheme()  const noexcept { return mTheme; }
            /// @brief Retourne true si le thème sombre est actif
            bool          IsDark()    const noexcept { return mTheme == NkWindowTheme::NK_THEME_DARK; }
            /// @brief Retourne true si le thème clair est actif
            bool          IsLight()   const noexcept { return mTheme == NkWindowTheme::NK_THEME_LIGHT; }

        private:
            NkWindowTheme mTheme = NkWindowTheme::NK_THEME_UNKNOWN; ///< Thème actif
    };

    // =========================================================================
    // NkWindowShownEvent / NkWindowHiddenEvent
    // =========================================================================

    class NkWindowShownEvent final : public NkWindowEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_WINDOW_SHOWN)
            explicit NkWindowShownEvent(NkU64 windowId = 0) noexcept
                : NkWindowEvent(windowId) {}
            NkEvent*    Clone()    const override { return new NkWindowShownEvent(*this); }
            NkString ToString() const override { return "WindowShown()"; }
    };

    class NkWindowHiddenEvent final : public NkWindowEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_WINDOW_HIDDEN)
            explicit NkWindowHiddenEvent(NkU64 windowId = 0) noexcept
                : NkWindowEvent(windowId) {}
            NkEvent*    Clone()    const override { return new NkWindowHiddenEvent(*this); }
            NkString ToString() const override { return "WindowHidden()"; }
    };

} // namespace nkentseu