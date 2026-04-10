// -----------------------------------------------------------------------------
// @File    NkUIContext.cpp
// @Brief   Implémentation NkUIContext — état, IDs, animations, styles.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis
// @License Apache-2.0
// -----------------------------------------------------------------------------

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Context lifecycle and state-machine implementation.
 * Main data: BeginFrame/EndFrame, ID generation, theme/style handling.
 * Change this file when: Frame sequencing or context behavior changes.
 */

#include "NKUI/NkUIContext.h"
#include <cstring>
#include <cstdio>

namespace nkentseu
{
    namespace nkui
    {
        // ============================================================
        // Helper : graine pour la génération d'ID
        // ============================================================

        static NKUI_INLINE NkUIID MakeIDSeed(const NkUIContext& ctx) noexcept
        {
            NkUIID seed = (ctx.idDepth > 0) ? ctx.idStack[ctx.idDepth - 1] : 2166136261u;
            // Étend tous les ID générés par l'ID de la fenêtre courante pour éviter les collisions
            if (ctx.currentWindowId != NKUI_ID_NONE)
            {
                seed = NkHashInt(static_cast<int32>(ctx.currentWindowId), seed);
            }
            return seed;
        }

        // ============================================================
        // Thèmes prédéfinis
        // ============================================================

        NkUITheme NkUITheme::Default() noexcept
        {
            NkUITheme t;
            ::strcpy(t.name, "NkDefault");
            t.darkMode = false;
            return t;   // Les valeurs par défaut de NkUIColorPalette suffisent
        }

        NkUITheme NkUITheme::Dark() noexcept
        {
            NkUITheme t;
            ::strcpy(t.name, "NkDark");
            t.darkMode = true;
            auto& c = t.colors;
            c.bgPrimary      = { 32, 32, 36, 255 };
            c.bgSecondary    = { 40, 40, 46, 255 };
            c.bgTertiary     = { 28, 28, 32, 255 };
            c.bgWindow       = { 36, 36, 42, 255 };
            c.bgPopup        = { 44, 44, 52, 255 };
            c.bgHeader       = { 22, 22, 26, 255 };
            c.border         = { 62, 62, 72, 255 };
            c.borderFocus    = { 80, 140, 255, 255 };
            c.borderHover    = { 80, 80, 92, 255 };
            c.textPrimary    = { 220, 220, 228, 255 };
            c.textSecondary  = { 150, 150, 165, 255 };
            c.textDisabled   = { 90, 90, 100, 255 };
            c.textOnAccent   = { 255, 255, 255, 255 };
            c.accent         = { 80, 140, 255, 255 };
            c.accentHover    = { 100, 160, 255, 255 };
            c.accentActive   = { 60, 110, 220, 255 };
            c.accentDisabled = { 70, 70, 85, 255 };
            c.buttonBg       = { 55, 55, 65, 255 };
            c.buttonHover    = { 70, 70, 82, 255 };
            c.buttonActive   = { 45, 45, 56, 255 };
            c.buttonText     = { 210, 210, 220, 255 };
            c.checkBg        = { 50, 50, 60, 255 };
            c.checkMark      = { 80, 140, 255, 255 };
            c.sliderTrack    = { 55, 55, 68, 255 };
            c.sliderThumb    = { 80, 140, 255, 255 };
            c.inputBg        = { 42, 42, 50, 255 };
            c.inputText      = { 220, 220, 228, 255 };
            c.inputPlaceholder = { 100, 100, 115, 255 };
            c.inputCursor    = { 80, 140, 255, 255 };
            c.inputSelection = { 80, 140, 255, 60 };
            c.scrollBg       = { 38, 38, 46, 255 };
            c.scrollThumb    = { 80, 80, 95, 255 };
            c.scrollThumbHov = { 105, 105, 120, 255 };
            c.titleBarBg     = { 22, 22, 28, 255 };
            c.titleBarActive = { 18, 18, 22, 255 };
            c.titleBarText   = { 200, 200, 210, 255 };
            c.titleBarBtn    = { 70, 70, 82, 255 };
            c.titleBarBtnHov = { 100, 100, 115, 255 };
            c.separator      = { 55, 55, 68, 255 };
            c.tabBg          = { 38, 38, 48, 255 };
            c.tabActive      = { 52, 52, 64, 255 };
            c.tabText        = { 140, 140, 155, 255 };
            c.tabActiveText  = { 220, 220, 228, 255 };
            return t;
        }

        NkUITheme NkUITheme::Minimal() noexcept
        {
            NkUITheme t = Default();
            ::strcpy(t.name, "NkMinimal");
            t.metrics.cornerRadius = 2.f;
            t.metrics.borderWidth = 1.f;
            t.colors.border = NkColor::Gray(210);
            t.colors.buttonBg = NkColor::White();
            return t;
        }

        NkUITheme NkUITheme::HighContrast() noexcept
        {
            NkUITheme t;
            ::strcpy(t.name, "NkHighContrast");
            t.colors.bgPrimary = NkColor::White();
            t.colors.bgWindow = NkColor::White();
            t.colors.border = NkColor::Black();
            t.colors.textPrimary = NkColor::Black();
            t.colors.accent = { 0, 0, 180, 255 };
            t.metrics.borderWidth = 2.f;
            t.metrics.cornerRadius = 0.f;
            return t;
        }

        // ============================================================
        // NkUIContext : Init / Destroy / BeginFrame / EndFrame
        // ============================================================

        bool NkUIContext::Init(int32 vW, int32 vH, const NkUIFontConfig& fontConfig) noexcept
        {
            if (mInitialized) return true;
            if (!fontManager.Init(fontConfig)) return false;

            viewW = vW;
            viewH = vH;
            theme = NkUITheme::Default();

            // Initialise les DrawLists
            for (int32 i = 0; i < LAYER_COUNT; ++i)
            {
                if (!layers[i].Init(32768, 98304, 256)) return false;
            }
            dl = &layers[LAYER_WINDOWS];
            memory::NkSet(overrides, 0, sizeof(overrides));
            memory::NkSet(overrideUD, 0, sizeof(overrideUD));
            idDepth = 0;
            styleDepth = 0;
            numAnims = 0;
            cursor = {};
            cursorStart = {};
            lineHeight = 0;
            sameLine = false;
            mInitialized = true;
            return true;
        }

        void NkUIContext::Destroy() noexcept
        {
            for (int32 i = 0; i < LAYER_COUNT; ++i) layers[i].Destroy();
            mInitialized = false;
            fontManager.Destroy();
        }

        void NkUIContext::BeginFrame(const NkUIInputState& inp, float32 dt_) noexcept
        {
            input = inp;
            dt = dt_;
            time += dt_;
            ++frameNum;

            wheelConsumed = false;
            wheelHConsumed = false;
            for (int32 i = 0; i < 5; ++i)
            {
                mouseClickConsumed[i] = false;
                mouseReleaseConsumed[i] = false;
            }
            mouseCursor = NkUIMouseCursor::NK_ARROW;

            // Réinitialise les DrawLists
            for (int32 i = 0; i < LAYER_COUNT; ++i) layers[i].Reset();
            dl = &layers[LAYER_WINDOWS];

            // Réinitialise les états transitoires
            ClearHot();
            currentWindowId = NKUI_ID_NONE;
            cursor = { 0.f, 0.f };
            cursorStart = cursor;
            lineHeight = 0.f;
            sameLine = false;

            // Avance les animations
            StepAnimations();
        }

        void NkUIContext::EndFrame() noexcept
        {
            // Si aucun widget actif cette frame et que la souris est relâchée → clear
            if (!input.mouseDown[0]) ClearActive();
        }

        // ============================================================
        // Gestion des IDs
        // ============================================================

        NkUIID NkUIContext::GetID(const char* str) const noexcept
        {
            return NkHash(str, MakeIDSeed(*this));
        }

        NkUIID NkUIContext::GetID(int32 n) const noexcept
        {
            return NkHashInt(n, MakeIDSeed(*this));
        }

        NkUIID NkUIContext::GetID(const void* p) const noexcept
        {
            return NkHashPtr(p, MakeIDSeed(*this));
        }

        void NkUIContext::PushID(const char* str) noexcept
        {
            if (idDepth < 64) idStack[idDepth++] = GetID(str);
        }

        void NkUIContext::PushID(int32 n) noexcept
        {
            if (idDepth < 64) idStack[idDepth++] = GetID(n);
        }

        void NkUIContext::PopID() noexcept
        {
            if (idDepth > 0) --idDepth;
        }

        // ============================================================
        // Animations — smoothing exponentiel
        // ============================================================

        float32 NkUIContext::Animate(NkUIID id, float32 target, float32 speed) noexcept
        {
            if (!theme.anim.enabled) return target;

            // Cherche l'animation existante
            for (int32 i = 0; i < numAnims; ++i)
            {
                if (anims[i].id == id)
                {
                    anims[i].target = target;
                    anims[i].speed = speed;
                    return anims[i].value;
                }
            }

            // Crée une nouvelle animation
            if (numAnims < 512)
            {
                anims[numAnims++] = { id, target, target, speed };
            }
            return target;
        }

        void NkUIContext::StepAnimations() noexcept
        {
            if (!theme.anim.enabled) return;
            for (int32 i = 0; i < numAnims; ++i)
            {
                NkUIAnim& a = anims[i];
                // Smoothing exponentiel : value → target
                const float32 k = 1.f - ::expf(-a.speed * dt);
                a.value = a.value + (a.target - a.value) * k;
                // Snap si très proche
                if (::fabsf(a.value - a.target) < 0.001f) a.value = a.target;
            }
            // Les animations terminées ne sont pas supprimées (peu coûteux pour 512 entrées)
        }

        // ============================================================
        // Pile de style
        // ============================================================

        void NkUIContext::PushStyleColor(NkStyleVar var, NkColor col) noexcept
        {
            if (styleDepth >= 128) return;
            NkStyleVarEntry& e = styleStack[styleDepth++];
            e.var = var;

            // Applique la nouvelle couleur et sauvegarde l'ancienne
            switch (var)
            {
                case NkStyleVar::NK_BUTTON_BG:
                    e.prev.col = theme.colors.buttonBg;
                    theme.colors.buttonBg = col;
                    break;
                case NkStyleVar::NK_BUTTON_TEXT:
                    e.prev.col = theme.colors.buttonText;
                    theme.colors.buttonText = col;
                    break;
                case NkStyleVar::NK_TEXT_COLOR:
                    e.prev.col = theme.colors.textPrimary;
                    theme.colors.textPrimary = col;
                    break;
                default:
                    --styleDepth;
                    break;
            }
        }

        void NkUIContext::PushStyleVar(NkStyleVar var, float32 val) noexcept
        {
            if (styleDepth >= 128) return;
            NkStyleVarEntry& e = styleStack[styleDepth++];
            e.var = var;

            switch (var)
            {
                case NkStyleVar::NK_ITEM_SPACING:
                    e.prev.f = theme.metrics.itemSpacing;
                    theme.metrics.itemSpacing = val;
                    break;
                case NkStyleVar::NK_PADDING_X:
                    e.prev.f = theme.metrics.paddingX;
                    theme.metrics.paddingX = val;
                    break;
                case NkStyleVar::NK_PADDING_Y:
                    e.prev.f = theme.metrics.paddingY;
                    theme.metrics.paddingY = val;
                    break;
                case NkStyleVar::NK_CORNER_RADIUS:
                    e.prev.f = theme.metrics.cornerRadius;
                    theme.metrics.cornerRadius = val;
                    break;
                case NkStyleVar::NK_BORDER_WIDTH:
                    e.prev.f = theme.metrics.borderWidth;
                    theme.metrics.borderWidth = val;
                    break;
                case NkStyleVar::NK_ALPHA:
                    e.prev.f = globalAlpha;
                    globalAlpha = val;
                    break;
                default:
                    --styleDepth;
                    break;
            }
        }

        void NkUIContext::PopStyle(int32 count) noexcept
        {
            for (int32 i = 0; i < count && styleDepth > 0; ++i)
            {
                const NkStyleVarEntry& e = styleStack[--styleDepth];
                switch (e.var)
                {
                    case NkStyleVar::NK_BUTTON_BG:
                        theme.colors.buttonBg = e.prev.col;
                        break;
                    case NkStyleVar::NK_BUTTON_TEXT:
                        theme.colors.buttonText = e.prev.col;
                        break;
                    case NkStyleVar::NK_TEXT_COLOR:
                        theme.colors.textPrimary = e.prev.col;
                        break;
                    case NkStyleVar::NK_ITEM_SPACING:
                        theme.metrics.itemSpacing = e.prev.f;
                        break;
                    case NkStyleVar::NK_PADDING_X:
                        theme.metrics.paddingX = e.prev.f;
                        break;
                    case NkStyleVar::NK_PADDING_Y:
                        theme.metrics.paddingY = e.prev.f;
                        break;
                    case NkStyleVar::NK_CORNER_RADIUS:
                        theme.metrics.cornerRadius = e.prev.f;
                        break;
                    case NkStyleVar::NK_BORDER_WIDTH:
                        theme.metrics.borderWidth = e.prev.f;
                        break;
                    case NkStyleVar::NK_ALPHA:
                        globalAlpha = e.prev.f;
                        break;
                    default:
                        break;
                }
            }
        }

        // ============================================================
        // Shape overrides
        // ============================================================

        void NkUIContext::SetShapeOverride(NkUIWidgetType type, NkUIShapeOverrideFn fn, const void* ud) noexcept
        {
            const int32 idx = static_cast<int32>(type);
            if (idx <= 0 || idx >= static_cast<int32>(NkUIWidgetType::NK_COUNT)) return;
            overrides[idx] = fn;
            overrideUD[idx] = ud;
        }

        void NkUIContext::ClearShapeOverride(NkUIWidgetType type) noexcept
        {
            const int32 idx = static_cast<int32>(type);
            if (idx > 0 && idx < static_cast<int32>(NkUIWidgetType::NK_COUNT)) overrides[idx] = nullptr;
        }

        bool NkUIContext::CallShapeOverride(NkUIWidgetType type,
                                            NkRect rect,
                                            NkUIWidgetState state,
                                            const char* label,
                                            float32 val) noexcept
        {
            const int32 idx = static_cast<int32>(type);
            if (idx <= 0 || idx >= static_cast<int32>(NkUIWidgetType::NK_COUNT) || !overrides[idx]) return false;
            NkUIShapeOverrideCtx sctx = { dl, rect, state, &theme, overrideUD[idx], label, val };
            overrides[idx](sctx);
            return true;
        }

        // ============================================================
        // Helpers de layout
        // ============================================================

        void NkUIContext::AdvanceCursor(NkVec2 size) noexcept
        {
            if (sameLine)
            {
                cursor.x += size.x + theme.metrics.itemSpacing;
                if (size.y > lineHeight) lineHeight = size.y;
                sameLine = false;
            }
            else
            {
                cursor.y += size.y + theme.metrics.itemSpacing;
                cursor.x = cursorStart.x;
                lineHeight = 0;
            }
        }

        void NkUIContext::SameLine(float32 offsetX, float32 spacing) noexcept
        {
            const float32 sp = spacing >= 0 ? spacing : theme.metrics.itemSpacing;
            if (offsetX > 0) cursor.x = cursorStart.x + offsetX;
            else cursor.x += sp;
            cursor.y -= theme.metrics.itemSpacing;   // remonte d'une ligne
            sameLine = true;
        }

        void NkUIContext::NewLine() noexcept
        {
            cursor.y += lineHeight + theme.metrics.itemSpacing;
            cursor.x = cursorStart.x;
            lineHeight = 0;
            sameLine = false;
        }

        void NkUIContext::Spacing(float32 pixels) noexcept
        {
            cursor.y += pixels > 0 ? pixels : theme.metrics.itemSpacing;
        }

        // ============================================================
        // Couleurs par type+état
        // ============================================================

        NkColor NkUIContext::GetColor(NkUIWidgetType type, NkUIWidgetState state) const noexcept
        {
            const auto& c = theme.colors;
            if (HasState(state, NkUIWidgetState::NK_DISABLED)) return c.accentDisabled;

            switch (type)
            {
                case NkUIWidgetType::NK_BUTTON:
                    if (HasState(state, NkUIWidgetState::NK_ACTIVE))  return c.buttonActive;
                    if (HasState(state, NkUIWidgetState::NK_HOVERED)) return c.buttonHover;
                    return c.buttonBg;

                case NkUIWidgetType::NK_CHECKBOX:
                case NkUIWidgetType::NK_RADIO:
                    return c.checkBg;

                case NkUIWidgetType::NK_SLIDER_FLOAT:
                case NkUIWidgetType::NK_SLIDER_INT:
                    if (HasState(state, NkUIWidgetState::NK_ACTIVE) || HasState(state, NkUIWidgetState::NK_HOVERED))
                        return c.accentHover;
                    return c.sliderThumb;

                case NkUIWidgetType::NK_INPUT_TEXT:
                case NkUIWidgetType::NK_INPUT_INT:
                case NkUIWidgetType::NK_INPUT_FLOAT:
                    if (HasState(state, NkUIWidgetState::NK_FOCUSED)) return c.inputBg;
                    return c.inputBg;

                default:
                    return c.bgSecondary;
            }
        }

        // ============================================================
        // Thème JSON — parsing simple
        // ============================================================

        // Mini parseur JSON pour les thèmes NkUI
        // Format supporté : objet plat {"key": value, ...}
        // Valeurs : chaîne, nombre, booléen, tableau de 4 nombres (couleur RGBA)

        static NkColor ParseJSONColor(const char* val) noexcept
        {
            // Format : [R, G, B, A] ou "RRGGBBAA" hexadécimal
            NkColor c = { 0, 0, 0, 255 };
            if (!val) return c;
            while (*val == ' ') ++val;
            if (*val == '[')
            {
                ++val;
                float32 comp[4] = { 0, 0, 0, 255 };
                for (int32 i = 0; i < 4; ++i)
                {
                    while (*val == ' ' || *val == ',') ++val;
                    if (*val == ']') break;
                    char* e;
                    comp[i] = ::strtof(val, &e);
                    if (e == val) break;
                    val = e;
                }
                c.r = static_cast<uint8>(comp[0]);
                c.g = static_cast<uint8>(comp[1]);
                c.b = static_cast<uint8>(comp[2]);
                c.a = static_cast<uint8>(comp[3]);
            }
            else if (*val == '"')
            {
                ++val;
                uint32 hex = static_cast<uint32>(::strtoul(val, nullptr, 16));
                c.r = (hex >> 24) & 0xFF;
                c.g = (hex >> 16) & 0xFF;
                c.b = (hex >>  8) & 0xFF;
                c.a = hex & 0xFF;
            }
            return c;
        }

        bool NkUIContext::ParseThemeJSON(const char* json, usize /*len*/) noexcept
        {
            if (!json) return false;
            const char* p = json;
            while (*p && *p != '{') ++p;
            if (!*p) return false;
            ++p;

            while (*p && *p != '}')
            {
                while (*p == ' ' || *p == '\n' || *p == '\r' || *p == ',') ++p;
                if (*p == '}') break;

                // Lit la clé
                if (*p != '"')
                {
                    ++p;
                    continue;
                }
                ++p;
                const char* keyStart = p;
                while (*p && *p != '"') ++p;
                const usize keyLen = p - keyStart;
                ++p;
                while (*p == ' ' || *p == ':') ++p;

                // Lit la valeur
                char key[64] = {};
                memory::NkCopy(key, keyStart, keyLen < 63 ? keyLen : 63);

                // Valeur numérique
                if (*p == '-' || (*p >= '0' && *p <= '9'))
                {
                    char* e;
                    const float32 v = ::strtof(p, &e);
                    p = e;
                    // Mappe les clés de métriques
                    if      (::strcmp(key, "corner_radius") == 0)  theme.metrics.cornerRadius = v;
                    else if (::strcmp(key, "item_height")   == 0)  theme.metrics.itemHeight   = v;
                    else if (::strcmp(key, "padding_x")     == 0)  theme.metrics.paddingX     = v;
                    else if (::strcmp(key, "padding_y")     == 0)  theme.metrics.paddingY     = v;
                    else if (::strcmp(key, "spacing")       == 0)  theme.metrics.itemSpacing  = v;
                    else if (::strcmp(key, "border_width")  == 0)  theme.metrics.borderWidth  = v;
                    else if (::strcmp(key, "title_height")  == 0)  theme.metrics.titleBarHeight = v;
                }
                // Valeur booléenne
                else if (::strncmp(p, "true", 4) == 0 || ::strncmp(p, "false", 5) == 0)
                {
                    const bool bv = (*p == 't');
                    p += bv ? 4 : 5;
                    if      (::strcmp(key, "dark_mode")  == 0)  theme.darkMode = bv;
                    else if (::strcmp(key, "animations") == 0)  theme.anim.enabled = bv;
                }
                // Valeur chaîne
                else if (*p == '"')
                {
                    ++p;
                    const char* vs = p;
                    while (*p && *p != '"') ++p;
                    char val[128] = {};
                    memory::NkCopy(val, vs, p - vs < 127 ? p - vs : 127);
                    ++p;
                    if (::strcmp(key, "name") == 0) ::strncpy(theme.name, val, 63);
                }
                // Tableau (couleur ou autre)
                else if (*p == '[')
                {
                    const char* vs = p;
                    while (*p && *p != ']') ++p;
                    if (*p == ']') ++p;
                    char val[128] = {};
                    memory::NkCopy(val, vs, p - vs < 127 ? p - vs : 127);
                    const NkColor col = ParseJSONColor(val);
                    // Mappe les clés de couleurs
                    if      (::strcmp(key, "accent")      == 0)  theme.colors.accent       = col;
                    else if (::strcmp(key, "bg_primary")  == 0)  theme.colors.bgPrimary    = col;
                    else if (::strcmp(key, "bg_window")   == 0)  theme.colors.bgWindow     = col;
                    else if (::strcmp(key, "text")        == 0)  theme.colors.textPrimary  = col;
                    else if (::strcmp(key, "button_bg")   == 0)  theme.colors.buttonBg     = col;
                    else if (::strcmp(key, "border")      == 0)  theme.colors.border       = col;
                    else if (::strcmp(key, "title_bg")    == 0)  theme.colors.titleBarBg   = col;
                }
                else
                {
                    ++p;
                }
            }
            return true;
        }

        bool NkUIContext::LoadThemeJSON(const char* path) noexcept
        {
            FILE* f = ::fopen(path, "rb");
            if (!f) return false;
            ::fseek(f, 0, SEEK_END);
            const long sz = ::ftell(f);
            ::fseek(f, 0, SEEK_SET);
            if (sz <= 0)
            {
                ::fclose(f);
                return false;
            }
            char* buf = static_cast<char*>(memory::NkAlloc(sz + 1));
            if (!buf)
            {
                ::fclose(f);
                return false;
            }
            ::fread(buf, 1, sz, f);
            buf[sz] = 0;
            ::fclose(f);
            const bool ok = ParseThemeJSON(buf, sz);
            memory::NkFree(buf);
            return ok;
        }

        void NkUIContext::SaveThemeJSON(const char* path) const noexcept
        {
            FILE* f = ::fopen(path, "wb");
            if (!f) return;
            const auto& c = theme.colors;
            const auto& m = theme.metrics;
            ::fprintf(f, "{\n");
            ::fprintf(f, "  \"name\": \"%s\",\n", theme.name);
            ::fprintf(f, "  \"dark_mode\": %s,\n", theme.darkMode ? "true" : "false");
            ::fprintf(f, "  \"animations\": %s,\n", theme.anim.enabled ? "true" : "false");

            // Métriques
            ::fprintf(f, "  \"corner_radius\": %.1f,\n", m.cornerRadius);
            ::fprintf(f, "  \"item_height\": %.1f,\n",   m.itemHeight);
            ::fprintf(f, "  \"padding_x\": %.1f,\n",     m.paddingX);
            ::fprintf(f, "  \"padding_y\": %.1f,\n",     m.paddingY);
            ::fprintf(f, "  \"spacing\": %.1f,\n",       m.itemSpacing);
            ::fprintf(f, "  \"border_width\": %.1f,\n",  m.borderWidth);
            ::fprintf(f, "  \"title_height\": %.1f,\n",  m.titleBarHeight);

            // Couleurs
            auto wc = [](FILE* ff, const char* k, NkColor col)
            {
                ::fprintf(ff, "  \"%s\": [%d, %d, %d, %d]", k, col.r, col.g, col.b, col.a);
            };
            wc(f, "accent",      c.accent);       ::fprintf(f, ",\n");
            wc(f, "bg_primary",  c.bgPrimary);    ::fprintf(f, ",\n");
            wc(f, "bg_window",   c.bgWindow);     ::fprintf(f, ",\n");
            wc(f, "text",        c.textPrimary);  ::fprintf(f, ",\n");
            wc(f, "button_bg",   c.buttonBg);     ::fprintf(f, ",\n");
            wc(f, "border",      c.border);       ::fprintf(f, ",\n");
            wc(f, "title_bg",    c.titleBarBg);   ::fprintf(f, "\n");

            ::fprintf(f, "}\n");
            ::fclose(f);
        }

    } // namespace nkui
} // namespace nkentseu