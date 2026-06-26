// =============================================================================
// NkGuiWidgets.cpp — widgets immédiats NKGui (Phase 2-3 : Button, Panel, Text).
// =============================================================================
#include "NKGui/Widgets/NkGuiWidgets.h"
#include "NKGui/Core/NkGuiFont.h"
#include "NKFont/NkFont.h"          // NkFontEncodeUTF8 / CalcTextSizeX
#include "NKContainers/String/NkString.h"  // NkString : parse (ToFloat) + format (Format)
#include "NKMath/NkFunctions.h"            // math::NkClamp / NkRound (zéro stdlib)
#include <cstdio>
#include <cstring>

namespace nkentseu {
    namespace nkgui {

        namespace {
            // Ligne de base centrée verticalement dans un rect, pour la police active.
            float32 CenteredBaseline(const NkGuiContext& ctx, const NkRect& r) noexcept {
                return r.y + (r.h - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent();
            }
            // Début du codepoint précédent / suivant (UTF-8) — pour le caret.
            int32 PrevCharStart(const char* s, int32 pos) noexcept {
                if (pos <= 0) return 0;
                --pos;
                while (pos > 0 && (static_cast<unsigned char>(s[pos]) & 0xC0u) == 0x80u) --pos;
                return pos;
            }
            int32 NextCharStart(const char* s, int32 len, int32 pos) noexcept {
                if (pos >= len) return len;
                ++pos;
                while (pos < len && (static_cast<unsigned char>(s[pos]) & 0xC0u) == 0x80u) ++pos;
                return pos;
            }
            // Position de caret (octets) la plus proche d'une abscisse `targetX`
            // (relative au début du texte), en mesurant les sous-chaînes.
            int32 CaretFromX(const NkFont* face, const char* buf, int32 len, float32 targetX) noexcept {
                if (!face || targetX <= 0.f) return 0;
                int32 pos = 0; float32 prevW = 0.f;
                while (pos < len) {
                    const int32   nx = NextCharStart(buf, len, pos);
                    const float32 w  = face->CalcTextSizeX(buf, buf + nx);
                    if (w >= targetX) return (targetX - prevW < w - targetX) ? pos : nx;
                    prevW = w; pos = nx;
                }
                return len;
            }
        } // namespace

        // Pré-déclarations (définitions plus bas).
        static bool TextEditField(NkGuiContext& ctx, const NkRect& field, char* buf, int32 bufSize,
                                  bool focused, NkGuiInputFlags flags = NkGuiInputFlags::None,
                                  int32 maxChars = -1) noexcept;
        // Popup générique (défini plus bas ; utilisé par ColorEdit pour le picker).
        static bool BeginPopupLevel(NkGuiContext& ctx, NkGuiId id, int32 level,
                                    const NkRect& rect, const NkRect& anchor) noexcept;
        // Frame de zone défilable (définies plus bas ; utilisées par BeginPanel).
        // `fillWidth` : en mode horizontal, l'auto-largeur remplit la LARGEUR VISIBLE
        // (pas 1e6) → le scroll H n'apparaît que si un contenu DÉPASSE réellement
        // (fenêtres). false = contenu à largeur naturelle déborde (boîte H dédiée).
        static bool BeginScrollFrame(NkGuiContext& ctx, NkGuiId id, const NkRect& area,
                                     bool horizontal, bool fillWidth = false) noexcept;
        static void EndScrollFrame(NkGuiContext& ctx) noexcept;
        static NkGuiScrollState ScrollGet(NkGuiContext& ctx, NkGuiId id) noexcept;
        static void ScrollSet(NkGuiContext& ctx, NkGuiId id, const NkGuiScrollState& s) noexcept;

        // Hook de style : retourne true si l'APP a dessiné l'élément (défaut sauté).
        static bool StyleDraw(NkGuiContext& ctx, NkGuiStyleKind kind, const NkRect& r, NkGuiId id,
                              const char* label, bool hovered, bool active, bool selected, bool disabled,
                              int32 value = 0) noexcept {
            if (!ctx.styleFn) return false;
            NkGuiStyleItem it;
            it.kind = kind; it.rect = r; it.id = id; it.label = label;
            it.hovered = hovered; it.active = active; it.selected = selected; it.disabled = disabled; it.value = value;
            return ctx.styleFn(ctx, it, ctx.styleUser);
        }
        // Variante pour les cibles de dock : route ctx.DL() vers l'OVERLAY le temps du
        // callback (les pastilles se dessinent au-dessus de tout, comme le défaut).
        static bool StyleDrawDockTarget(NkGuiContext& ctx, const NkRect& r, bool targeted, int32 dir) noexcept {
            if (!ctx.styleFn) return false;
            const int32 save = ctx.curPopupLevel;
            ctx.curPopupLevel = 0;                       // → ctx.DL() == dlOverlay
            const bool handled = StyleDraw(ctx, NkGuiStyleKind::DockTarget, r, NKGUI_ID_NONE, nullptr,
                                           targeted, targeted, false, false, dir);
            ctx.curPopupLevel = save;
            return handled;
        }

        void PanelBackground(NkGuiContext& ctx, const NkRect& r) noexcept {
            ctx.DL().AddRectFilled(r, ctx.theme.panel, ctx.theme.rounding);
            ctx.DL().AddRect(r, ctx.theme.border, 1.f);
        }

        float32 TextAt(NkGuiContext& ctx, const NkVec2& topLeft, const char* s, const NkColor& col) noexcept {
            if (!ctx.font || !ctx.font->Valid() || !s) return 0.f;
            // topLeft = coin haut-gauche → ligne de base = y + ascender.
            const float32 baseY = topLeft.y + ctx.font->Ascent();
            ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(), { topLeft.x, baseY }, s, col);
            return ctx.font->MeasureWidth(s);
        }

        float32 TextAt(NkGuiContext& ctx, const NkVec2& topLeft, const char* s) noexcept {
            return TextAt(ctx, topLeft, s, ctx.theme.text);
        }

        // Dessine un libellé centré dans un rect (helper interne).
        static void DrawCenteredLabel(NkGuiContext& ctx, const NkRect& r, const char* label,
                                      const NkColor& col) noexcept {
            if (!ctx.font || !ctx.font->Valid() || !label) return;
            const float32 tw    = ctx.font->MeasureWidth(label);
            const float32 tx    = r.x + (r.w - tw) * 0.5f;
            const float32 baseY = r.y + (r.h - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent();
            ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(), { tx, baseY }, label, col, r.w - 6.f);
        }

        bool ButtonEx(NkGuiContext& ctx, const char* label, const NkRect& r,
                      NkGuiButtonFlags flags, float32 repeatDelay, float32 repeatRate) noexcept {
            const NkGuiId id = ctx.GetId(label);
            bool hovered = false, held = false;
            const bool pressed = ctx.ButtonBehavior(id, r, flags, repeatDelay, repeatRate,
                                                    &hovered, &held);

            // Hook de style : l'app peut re-skinner totalement le bouton.
            if (StyleDraw(ctx, NkGuiStyleKind::Button, r, id, label, hovered, held, false, ctx.IsDisabled()))
                return pressed;

            const NkColor col = held    ? ctx.theme.buttonActive
                              : hovered ? ctx.theme.buttonHover
                                        : ctx.theme.button;
            ctx.DL().AddRectFilled(r, col, ctx.theme.rounding);
            ctx.DL().AddRect(r, ctx.theme.border, 1.f);
            const NkColor lc = ctx.IsDisabled() ? ctx.theme.textDisabled
                             : held              ? NkColor{ 255, 255, 255, 255 }
                                                 : ctx.theme.text;
            DrawCenteredLabel(ctx, r, label, lc);
            return pressed;
        }

        bool Button(NkGuiContext& ctx, const char* label, const NkRect& r) noexcept {
            return ButtonEx(ctx, label, r, NkGuiButtonFlags::None);
        }

        bool RepeatButton(NkGuiContext& ctx, const char* label, const NkRect& r,
                          float32 repeatDelay, float32 repeatRate) noexcept {
            return ButtonEx(ctx, label, r, NkGuiButtonFlags::Repeat, repeatDelay, repeatRate);
        }

        // ── Widgets auto-layout ─────────────────────────────────────────────────

        void Text(NkGuiContext& ctx, const char* s) noexcept {
            const float32 lh = (ctx.font && ctx.font->Valid()) ? ctx.font->LineHeight() : 16.f;
            const float32 w  = (ctx.font && ctx.font->Valid()) ? ctx.font->MeasureWidth(s) : 0.f;
            const NkRect  r  = ctx.NextItemRect(w, lh);
            TextAt(ctx, { r.x, r.y }, s, ctx.IsDisabled() ? ctx.theme.textDisabled : ctx.theme.text);
        }

        // Découpe `text` en lignes tenant dans `ww` : coupe aux ESPACES (mots), avec
        // repli au CARACTÈRE si un mot dépasse, et respecte les '\n' explicites. Remplit
        // outB/outE (début/fin de chaque ligne, sans les espaces de coupure) ; retourne
        // le nombre de lignes (≤ maxLines).
        static int32 WrapTextLines(const NkFont* face, const char* text, float32 ww,
                                   const char** outB, const char** outE, int32 maxLines) noexcept {
            int32 n = 0;
            const char* end = text; while (*end) ++end;
            const char* lineBegin = text;
            const char* lastFit   = text;     // fin du contenu qui tient sur la ligne
            const char* p = text;
            auto push = [&](const char* b, const char* e) {
                if (n < maxLines && e >= b) { outB[n] = b; outE[n] = e; ++n; }
            };
            while (p < end) {
                if (*p == '\n') { push(lineBegin, p); ++p; lineBegin = p; lastFit = p; continue; }
                if (*p == ' ' && p == lineBegin) { ++p; lineBegin = p; lastFit = p; continue; } // espaces de début
                const char* ws = p;                                  // début de mot
                while (p < end && *p != ' ' && *p != '\n') ++p;      // mot [ws, p)
                const char* we = p;
                if (face->CalcTextSizeX(lineBegin, we) > ww && lineBegin != ws) {
                    push(lineBegin, lastFit);                        // ne tient pas → couper avant le mot
                    lineBegin = ws;
                }
                if (face->CalcTextSizeX(lineBegin, we) > ww && lineBegin == ws) {
                    const char* q = ws;                              // mot seul trop long → coupe caractère
                    while (q < we) {
                        const char* qs = q;
                        NkFontDecodeUTF8(&q, we);
                        if (face->CalcTextSizeX(lineBegin, q) > ww && lineBegin != qs) {
                            push(lineBegin, qs); lineBegin = qs;
                        }
                    }
                }
                lastFit = we;
                while (p < end && *p == ' ') ++p;                    // sauter les espaces séparateurs
            }
            if (lastFit > lineBegin || n == 0) push(lineBegin, lastFit);
            return n;
        }

        void TextWrapped(NkGuiContext& ctx, const char* text, float32 wrapWidth) noexcept {
            if (!text || !*text || !ctx.font || !ctx.font->Valid()) return;
            const NkFont* face  = ctx.font->Face();
            const float32 lineH = ctx.font->LineHeight();
            float32 ww = (wrapWidth > 0.f) ? wrapWidth : ctx.ContentWidth();
            if (ww < 8.f) ww = 8.f;

            const char* lb[256]; const char* le[256];
            const int32 n = WrapTextLines(face, text, ww, lb, le, 256);

            const NkRect  r   = ctx.NextItemRect(ww, (n > 0 ? n : 1) * lineH);
            const NkColor col = ctx.IsDisabled() ? ctx.theme.textDisabled : ctx.theme.text;
            for (int32 i = 0; i < n; ++i) {
                const float32 baseY = r.y + i * lineH + ctx.font->Ascent();
                ctx.DL().AddTextRange(face, ctx.font->TexId(), { r.x, baseY }, lb[i], le[i], col);
            }
        }

        bool Button(NkGuiContext& ctx, const char* label) noexcept {
            const float32 tw = (ctx.font && ctx.font->Valid()) ? ctx.font->MeasureWidth(label) : 40.f;
            const NkRect  r  = ctx.NextItemRect(tw + ctx.theme.framePadX * 2.f + 6.f, ctx.ItemHeight());
            return Button(ctx, label, r);
        }

        // Cœur commun (binaire + tri-état) : dessine la case selon `state` et
        // retourne true si cliquée (l'appelant décide de la nouvelle valeur).
        static bool CheckboxCore(NkGuiContext& ctx, const char* label, NkGuiCheck state) noexcept {
            const float32 h   = ctx.ItemHeight();
            const float32 box = h - 8.f;
            const float32 tw  = (ctx.font && ctx.font->Valid()) ? ctx.font->MeasureWidth(label) : 0.f;
            const NkRect  r   = ctx.NextItemRect(box + 8.f + tw, h);
            const NkGuiId id  = ctx.GetId(label);

            bool hov = false, held = false;
            const bool pressed = ctx.ButtonBehavior(id, r, NkGuiButtonFlags::None, -1.f, -1.f, &hov, &held);

            const NkRect boxR = { r.x, r.y + (r.h - box) * 0.5f, box, box };
            // Hook de style : l'app peut redessiner la case (boîte + coche).
            if (!StyleDraw(ctx, NkGuiStyleKind::CheckMark, boxR, id, label, hov, held,
                           state == NkGuiCheck::On, ctx.IsDisabled())) {
            ctx.DL().AddRectFilled(boxR, hov ? ctx.theme.buttonHover : ctx.theme.button, 3.f);
            ctx.DL().AddRect(boxR, ctx.theme.border, 1.f);
            if (state == NkGuiCheck::On) {
                ctx.DL().AddRectFilled({ boxR.x + 4.f, boxR.y + 4.f, boxR.w - 8.f, boxR.h - 8.f },
                                     ctx.theme.accent, 2.f);
            } else if (state == NkGuiCheck::Mixed) {
                // Tiret central = état indéterminé.
                ctx.DL().AddRectFilled({ boxR.x + 4.f, boxR.y + boxR.h * 0.5f - 2.f, boxR.w - 8.f, 4.f },
                                     ctx.theme.accent, 1.f);
            }
            }
            if (ctx.font && ctx.font->Valid()) {
                ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
                               { boxR.x + box + 8.f, CenteredBaseline(ctx, r) }, label, ctx.theme.text);
            }
            return pressed;
        }

        bool Checkbox(NkGuiContext& ctx, const char* label, bool& value) noexcept {
            const bool pressed = CheckboxCore(ctx, label, value ? NkGuiCheck::On : NkGuiCheck::Off);
            if (pressed) value = !value;
            return pressed;
        }

        bool CheckboxTristate(NkGuiContext& ctx, const char* label, NkGuiCheck& state) noexcept {
            const bool pressed = CheckboxCore(ctx, label, state);
            if (pressed) state = (state == NkGuiCheck::On) ? NkGuiCheck::Off : NkGuiCheck::On;
            return pressed;
        }

        bool CheckBox3(NkGuiContext& ctx, const char* idStr, NkGuiCheck& state) noexcept {
            const float32 h   = ctx.ItemHeight();
            const float32 box = h - 8.f;
            const NkRect  r   = ctx.NextItemRect(box + 6.f, h);   // petite cellule (sans libellé)
            const NkGuiId id  = ctx.GetId(idStr);

            bool hov = false, held = false;
            const bool pressed = ctx.ButtonBehavior(id, r, NkGuiButtonFlags::None, -1.f, -1.f, &hov, &held);

            const NkRect boxR = { r.x, r.y + (r.h - box) * 0.5f, box, box };
            ctx.DL().AddRectFilled(boxR, hov ? ctx.theme.buttonHover : ctx.theme.button, 3.f);
            ctx.DL().AddRect(boxR, ctx.theme.border, 1.f);
            if (state == NkGuiCheck::On) {
                ctx.DL().AddRectFilled({ boxR.x + 4.f, boxR.y + 4.f, boxR.w - 8.f, boxR.h - 8.f },
                                     ctx.theme.accent, 2.f);
            } else if (state == NkGuiCheck::Mixed) {
                ctx.DL().AddRectFilled({ boxR.x + 4.f, boxR.y + boxR.h * 0.5f - 2.f, boxR.w - 8.f, 4.f },
                                     ctx.theme.accent, 1.f);
            }
            if (pressed) state = (state == NkGuiCheck::On) ? NkGuiCheck::Off : NkGuiCheck::On;
            return pressed;
        }

        // ── Helpers OPTIONNELS de sélection hiérarchique (façon hybride) ──────
        // L'app possède `states[]` (1 NkGuiCheck/nœud) + `parent[]` (index du parent
        // ou -1). Les widgets ne stockent rien : ces utilitaires appliquent la
        // cascade / l'agrégation sur les données de l'app, quand elle le décide.
        void NkGuiTreeCascade(NkGuiCheck* states, const int32* parent, int32 count,
                              int32 node, NkGuiCheck value) noexcept {
            if (!states || !parent || node < 0 || node >= count) return;
            if (value == NkGuiCheck::Mixed) value = NkGuiCheck::On;
            states[node] = value;
            for (int32 i = 0; i < count; ++i) {       // tout descendant de `node`
                int32 p = parent[i], guard = 0;
                while (p >= 0 && guard++ < count) {
                    if (p == node) { states[i] = value; break; }
                    p = parent[p];
                }
            }
        }

        void NkGuiTreeRecomputeMixed(NkGuiCheck* states, const int32* parent, int32 count) noexcept {
            if (!states || !parent || count <= 0) return;
            // Plusieurs passes (max = profondeur) → converge bottom-up sans tri.
            for (int32 pass = 0; pass < count; ++pass) {
                bool changed = false;
                for (int32 i = 0; i < count; ++i) {
                    bool hasChild = false, anyOn = false, anyOff = false, anyMixed = false;
                    for (int32 j = 0; j < count; ++j) {
                        if (parent[j] != i) continue;
                        hasChild = true;
                        if      (states[j] == NkGuiCheck::On)  anyOn = true;
                        else if (states[j] == NkGuiCheck::Off) anyOff = true;
                        else                                   anyMixed = true;
                    }
                    if (!hasChild) continue;                       // feuille : on n'y touche pas
                    const NkGuiCheck ns = (anyMixed || (anyOn && anyOff)) ? NkGuiCheck::Mixed
                                        : anyOn ? NkGuiCheck::On : NkGuiCheck::Off;
                    if (states[i] != ns) { states[i] = ns; changed = true; }
                }
                if (!changed) break;
            }
        }

        bool SliderFloat(NkGuiContext& ctx, const char* label, float32& value,
                         float32 vmin, float32 vmax) noexcept {
            const float32 h   = ctx.ItemHeight();
            const NkRect  r   = ctx.NextItemRect(0.f, h);   // remplit la largeur
            const NkGuiId id  = ctx.GetId(label);

            const float32 trkW  = (r.w * 0.55f > 40.f) ? r.w * 0.55f : 40.f;
            const NkRect  track = { r.x, r.y + (r.h - 6.f) * 0.5f, trkW, 6.f };

            const bool hovered = ctx.ItemHoverable(track, id);
            if (hovered && ctx.input.mouseClicked[0]) ctx.activeId = id;
            bool changed = false;
            if (ctx.activeId == id) {
                ctx.interact = NkGuiInteract::EditWidget;
                const float32 denom = track.w > 1.f ? track.w : 1.f;
                float32 t = (ctx.input.mousePos.x - track.x) / denom;
                if (t < 0.f) t = 0.f; else if (t > 1.f) t = 1.f;
                const float32 nv = vmin + t * (vmax - vmin);
                if (nv != value) { value = nv; changed = true; }
                if (ctx.input.mouseReleased[0]) ctx.activeId = NKGUI_ID_NONE;
            }

            float32 tt = (vmax > vmin) ? (value - vmin) / (vmax - vmin) : 0.f;
            if (tt < 0.f) tt = 0.f; else if (tt > 1.f) tt = 1.f;

            ctx.DL().AddRectFilled(track, ctx.theme.track, 3.f);
            ctx.DL().AddRectFilled({ track.x, track.y, track.w * tt, track.h }, ctx.theme.accent, 3.f);
            ctx.DL().AddCircleFilled({ track.x + track.w * tt, track.y + track.h * 0.5f }, 7.f,
                                   (ctx.activeId == id) ? NkColor{ 255, 255, 255, 255 } : ctx.theme.buttonHover);

            if (ctx.font && ctx.font->Valid()) {
                const float32 baseY = CenteredBaseline(ctx, r);
                char buf[32];
                ::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(value));
                const float32 vx = track.x + track.w + 12.f;
                ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(), { vx, baseY }, buf, ctx.theme.text);
                ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
                               { vx + ctx.font->MeasureWidth(buf) + 14.f, baseY }, label, ctx.theme.text);
            }
            return changed;
        }

        // ── Champs numériques : DragFloat/DragInt + steppers ─────────────────
        // Cœur : GLISSER horizontal pour changer la valeur, DOUBLE-CLIC pour la saisir
        // au clavier, boutons -/+ optionnels (stepBtns>0). Zéro stdlib : NkString pour
        // parse/format, NKMath pour clamp/round. Retourne true si la valeur a changé.
        static bool DragScalar(NkGuiContext& ctx, const char* label, float32* v,
                               float32 speed, float32 vmin, float32 vmax,
                               bool isInt, float32 stepBtns, NkGuiDragDir dir) noexcept {
            const float32 h     = ctx.ItemHeight();
            const NkRect   rowR = ctx.NextItemRect(0.f, h);
            const NkGuiId  id   = ctx.GetId(label);
            const float32 labelW = (ctx.font && ctx.font->Valid() && label && *label)
                                 ? ctx.font->MeasureWidth(label) + 14.f : 0.f;
            const NkRect area = { rowR.x, rowR.y, rowR.w - labelW, rowR.h };

            NkRect field = area, minusR{ 0.f, 0.f, 0.f, 0.f }, plusR{ 0.f, 0.f, 0.f, 0.f };
            const bool hasBtn = stepBtns > 0.f;
            if (hasBtn) {
                const float32 bw = area.h;
                minusR = { area.x, area.y, bw, area.h };
                plusR  = { area.x + area.w - bw, area.y, bw, area.h };
                field  = { area.x + bw + 2.f, area.y, area.w - 2.f * bw - 4.f, area.h };
            }
            bool changed = false;
            auto fmtVal = [&]() -> NkString {
                return isInt ? NkString::Format("%d", static_cast<int32>(math::NkRound(*v)))
                             : NkString::Format("%.3f", static_cast<double>(*v));
            };

            // ── Édition TEXTE (déclenchée au double-clic) ────────────────────
            if (ctx.dragEditId == id) {
                ctx.inputId = id; ctx.inputClickConsumed = true;
                const bool clickAway = ctx.input.mouseClicked[0] && !NkGuiRectContains(field, ctx.input.mousePos);
                const bool submitted = TextEditField(ctx, field, ctx.dragBuf,
                                                     static_cast<int32>(sizeof(ctx.dragBuf)),
                                                     true, NkGuiInputFlags::CharsDecimal);
                bool done = false;
                if (ctx.input.KeyPressed(NkGuiKey::Escape)) done = true;
                else if (submitted || clickAway) {
                    float32 nv = *v;
                    NkString(ctx.dragBuf).ToFloat(nv);                 // parse (Nkentseu)
                    if (isInt) nv = math::NkRound(nv);
                    nv = math::NkClamp(nv, vmin, vmax);
                    if (nv != *v) { *v = nv; changed = true; }
                    done = true;
                }
                if (done) { ctx.dragEditId = NKGUI_ID_NONE; ctx.inputId = NKGUI_ID_NONE; }
                if (hasBtn) {
                    ctx.DL().AddRectFilled(minusR, ctx.theme.button, 3.f); DrawCenteredLabel(ctx, minusR, "-", ctx.theme.textDisabled);
                    ctx.DL().AddRectFilled(plusR,  ctx.theme.button, 3.f); DrawCenteredLabel(ctx, plusR,  "+", ctx.theme.textDisabled);
                }
                if (ctx.font && ctx.font->Valid() && label && *label)
                    ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
                                     { area.x + area.w + 12.f, CenteredBaseline(ctx, area) }, label, ctx.theme.text);
                return changed;
            }

            // ── Boutons -/+ (ids uniques par champ via PushId) ───────────────
            if (hasBtn) {
                ctx.PushId(label);
                if (Button(ctx, "-", minusR)) { *v = math::NkClamp(*v - stepBtns, vmin, vmax); changed = true; }
                if (Button(ctx, "+", plusR))  { *v = math::NkClamp(*v + stepBtns, vmin, vmax); changed = true; }
                ctx.PopId();
            }

            // ── Glisser sur le champ (double-clic = saisie). Direction configurable. ─
            const bool over = ctx.ItemHoverable(field, id);
            // Curseur ↔/↕ pendant le survol/le glisser (l'app l'applique si elle veut).
            if (over || ctx.activeId == id)
                ctx.wantCursor = (dir == NkGuiDragDir::Vertical) ? NkGuiCursor::ResizeNS : NkGuiCursor::ResizeEW;
            if (over && ctx.input.mouseDoubleClicked[0]) {
                ctx.dragEditId = id; ctx.inputId = id; ctx.activeId = NKGUI_ID_NONE;
                const NkString s = fmtVal();
                const char* c = s.CStr(); int32 l = 0;
                while (l < static_cast<int32>(sizeof(ctx.dragBuf)) - 1 && c && c[l]) { ctx.dragBuf[l] = c[l]; ++l; }
                ctx.dragBuf[l] = '\0'; ctx.inputCaret = l; ctx.inputScroll = 0.f;
            } else if (over && ctx.input.mouseClicked[0]) {
                ctx.activeId = id;
                ctx.dragLastX = ctx.input.mousePos.x;
                ctx.dragLastY = ctx.input.mousePos.y;
            }
            if (ctx.activeId == id) {
                ctx.interact = NkGuiInteract::EditWidget;
                float32 delta = 0.f;
                if (dir == NkGuiDragDir::Horizontal || dir == NkGuiDragDir::Both)
                    delta += ctx.input.mousePos.x - ctx.dragLastX;
                if (dir == NkGuiDragDir::Vertical   || dir == NkGuiDragDir::Both)
                    delta -= ctx.input.mousePos.y - ctx.dragLastY;     // haut = augmente
                ctx.dragLastX = ctx.input.mousePos.x;
                ctx.dragLastY = ctx.input.mousePos.y;
                if (delta != 0.f) { *v = math::NkClamp(*v + delta * speed, vmin, vmax); changed = true; }
                if (ctx.input.mouseReleased[0]) ctx.activeId = NKGUI_ID_NONE;
            }

            // ── Rendu du champ + valeur + label (les boutons sont déjà dessinés) ─
            const bool active = (ctx.activeId == id);
            ctx.DL().AddRectFilled(field, ctx.theme.track, ctx.theme.rounding);
            ctx.DL().AddRect(field, (active || over) ? ctx.theme.accent : ctx.theme.border, active ? 1.5f : 1.f);
            { const NkString s = fmtVal(); DrawCenteredLabel(ctx, field, s.CStr(), ctx.theme.text); }
            if (ctx.font && ctx.font->Valid() && label && *label)
                ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
                                 { area.x + area.w + 12.f, CenteredBaseline(ctx, area) }, label, ctx.theme.text);
            return changed;
        }

        bool DragFloat(NkGuiContext& ctx, const char* label, float32& v,
                       float32 speed, float32 vmin, float32 vmax, NkGuiDragDir dir) noexcept {
            return DragScalar(ctx, label, &v, speed, vmin, vmax, false, 0.f, dir);
        }
        bool DragInt(NkGuiContext& ctx, const char* label, int32& v,
                     float32 speed, int32 vmin, int32 vmax, NkGuiDragDir dir) noexcept {
            float32 fv = static_cast<float32>(v);
            const bool c = DragScalar(ctx, label, &fv, speed,
                                      static_cast<float32>(vmin), static_cast<float32>(vmax), true, 0.f, dir);
            if (c) v = static_cast<int32>(math::NkRound(fv));
            return c;
        }
        bool InputFloat(NkGuiContext& ctx, const char* label, float32& v,
                        float32 step, float32 vmin, float32 vmax, NkGuiDragDir dir) noexcept {
            return DragScalar(ctx, label, &v, step * 0.25f, vmin, vmax, false, step, dir);
        }
        bool InputInt(NkGuiContext& ctx, const char* label, int32& v,
                      int32 step, int32 vmin, int32 vmax, NkGuiDragDir dir) noexcept {
            float32 fv = static_cast<float32>(v);
            const bool c = DragScalar(ctx, label, &fv, static_cast<float32>(step) * 0.25f,
                                      static_cast<float32>(vmin), static_cast<float32>(vmax),
                                      true, static_cast<float32>(step), dir);
            if (c) v = static_cast<int32>(math::NkRound(fv));
            return c;
        }

        // Compte les codepoints UTF-8 dans [b, e) (octets non-continuation).
        static int32 CountCp(const char* b, const char* e) noexcept {
            int32 n = 0;
            for (const char* p = b; p < e; ++p)
                if ((static_cast<unsigned char>(*p) & 0xC0u) != 0x80u) ++n;
            return n;
        }
        // Filtre/transforme un codepoint selon les flags ; 0 = rejeté.
        static uint32 FilterChar(uint32 cp, NkGuiInputFlags flags) noexcept {
            if (NkGuiHasInputFlag(flags, NkGuiInputFlags::NoBlank) && cp == ' ') return 0u;
            if (NkGuiHasInputFlag(flags, NkGuiInputFlags::Uppercase) && cp >= 'a' && cp <= 'z') cp -= 32u;
            if (NkGuiHasInputFlag(flags, NkGuiInputFlags::CharsDecimal)) {
                const bool ok = (cp >= '0' && cp <= '9') || cp == '.' || cp == '+' || cp == '-' || cp == ',';
                if (!ok) return 0u;
            }
            if (NkGuiHasInputFlag(flags, NkGuiInputFlags::CharsHex)) {
                const bool ok = (cp >= '0' && cp <= '9') || (cp >= 'a' && cp <= 'f') || (cp >= 'A' && cp <= 'F');
                if (!ok) return 0u;
            }
            return cp;
        }

        // Cœur d'édition de texte à un RECT donné. Suppose le champ déjà focalisé
        // (l'appelant gère le focus). Gère : clic→caret, saisie filtrée (flags),
        // flèches (répétition), Home/End, backspace/delete, limite de caractères,
        // masque mot de passe, lecture seule. Dessine fond/bordure/texte/caret.
        // Réutilisé par InputText ET le renommage inline. Retourne true si Entrée.
        static bool TextEditField(NkGuiContext& ctx, const NkRect& field, char* buf, int32 bufSize,
                                  bool focused, NkGuiInputFlags flags, int32 maxChars) noexcept {
            int32 len = 0; while (len < bufSize - 1 && buf[len]) ++len;
            int32 caret = ctx.inputCaret;
            if (caret > len) caret = len; if (caret < 0) caret = 0;

            const float32 padX     = 6.f;
            const bool     pw       = NkGuiHasInputFlag(flags, NkGuiInputFlags::Password);
            const bool     readOnly = NkGuiHasInputFlag(flags, NkGuiInputFlags::ReadOnly);

            bool submitted = false;
            if (focused) {
                ctx.interact = NkGuiInteract::EditWidget;
                // Clic → caret. En mot de passe : fin de champ (repositionnement aux flèches).
                if (ctx.input.mouseClicked[0] && NkGuiRectContains(field, ctx.input.mousePos)
                    && ctx.font && ctx.font->Valid()) {
                    if (pw) caret = len;
                    else {
                        const float32 originX = (field.x + padX) - ctx.inputScroll;
                        caret = CaretFromX(ctx.font->Face(), buf, len, ctx.input.mousePos.x - originX);
                    }
                    ctx.inputClickConsumed = true;
                }
                if (!readOnly) {
                    for (int32 k = 0; k < ctx.input.charCount; ++k) {
                        uint32 cp = ctx.input.chars[k];
                        if (cp < 32u) continue;                   // ignore les contrôles
                        cp = FilterChar(cp, flags);
                        if (cp == 0u) continue;                   // rejeté par les flags
                        if (maxChars >= 0 && CountCp(buf, buf + len) >= maxChars) continue;  // limite atteinte
                        char tmp[5] = {};
                        const int32 n = NkFontEncodeUTF8(cp, tmp, 5);
                        if (n > 0 && len + n < bufSize) {
                            ::memmove(buf + caret + n, buf + caret, static_cast<usize>(len - caret + 1));
                            ::memcpy(buf + caret, tmp, static_cast<usize>(n));
                            caret += n; len += n;
                        }
                    }
                    if (ctx.input.KeyPressedRepeat(NkGuiKey::Backspace) && caret > 0) {
                        const int32 p = PrevCharStart(buf, caret); const int32 n = caret - p;
                        ::memmove(buf + p, buf + caret, static_cast<usize>(len - caret + 1));
                        caret = p; len -= n;
                    }
                    if (ctx.input.KeyPressedRepeat(NkGuiKey::Delete) && caret < len) {
                        const int32 nx = NextCharStart(buf, len, caret); const int32 n = nx - caret;
                        ::memmove(buf + caret, buf + nx, static_cast<usize>(len - nx + 1));
                        len -= n;
                    }
                }
                // Navigation caret (autorisée même en lecture seule).
                if (ctx.input.KeyPressedRepeat(NkGuiKey::Left)  && caret > 0)   caret = PrevCharStart(buf, caret);
                if (ctx.input.KeyPressedRepeat(NkGuiKey::Right) && caret < len) caret = NextCharStart(buf, len, caret);
                if (ctx.input.KeyPressed(NkGuiKey::Home))  caret = 0;
                if (ctx.input.KeyPressed(NkGuiKey::End))   caret = len;
                if (ctx.input.KeyPressed(NkGuiKey::Enter)) submitted = true;
                ctx.inputCaret = caret;
            }

            // Rendu du champ (fond plus sombre si lecture seule).
            ctx.DL().AddRectFilled(field, readOnly ? NkColor{ 28, 30, 36, 255 } : ctx.theme.track, ctx.theme.rounding);
            ctx.DL().AddRect(field, focused ? ctx.theme.accent : ctx.theme.border, focused ? 1.5f : 1.f);

            const NkRect clip = { field.x + padX, field.y, field.w - padX * 2.f, field.h };
            ctx.DL().PushClipRect(clip, true);
            if (ctx.font && ctx.font->Valid()) {
                const float32 baseY = CenteredBaseline(ctx, field);
                // Chaîne affichée : points si mot de passe.
                char mask[260];
                const char* draw = buf;
                float32 caretW;
                if (pw) {
                    int32 cpTot = CountCp(buf, buf + len); if (cpTot > 256) cpTot = 256;
                    for (int32 i = 0; i < cpTot; ++i) mask[i] = '*';
                    mask[cpTot] = '\0';
                    int32 cpBefore = CountCp(buf, buf + caret); if (cpBefore > cpTot) cpBefore = cpTot;
                    caretW = ctx.font->Face()->CalcTextSizeX(mask, mask + cpBefore);
                    draw = mask;
                } else {
                    caretW = ctx.font->Face()->CalcTextSizeX(buf, buf + caret);
                }
                if (focused) {
                    const float32 viewW = clip.w;
                    if (caretW - ctx.inputScroll > viewW) ctx.inputScroll = caretW - viewW;
                    if (caretW - ctx.inputScroll < 0.f)   ctx.inputScroll = caretW;
                    if (ctx.inputScroll < 0.f) ctx.inputScroll = 0.f;
                }
                const float32 tx = clip.x - (focused ? ctx.inputScroll : 0.f);
                const NkColor tc = readOnly ? ctx.theme.textDisabled : ctx.theme.text;
                ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(), { tx, baseY }, draw, tc);
                if (focused && (static_cast<int32>(ctx.time * 2.f) & 1) == 0) {
                    ctx.DL().AddRectFilled({ tx + caretW, field.y + 5.f, 1.5f, field.h - 10.f }, tc);
                }
            }
            ctx.DL().PopClipRect();
            return submitted;
        }

        bool InputTextEx(NkGuiContext& ctx, const char* label, char* buf, int32 bufSize,
                         NkGuiInputFlags flags, int32 maxChars) noexcept {
            if (!buf || bufSize <= 1) return false;
            const float32 h    = ctx.ItemHeight();
            const NkRect  rowR = ctx.NextItemRect(0.f, h);
            const NkGuiId id   = ctx.GetId(label);

            const float32 labelW = (ctx.font && ctx.font->Valid() && label && *label)
                                 ? ctx.font->MeasureWidth(label) + 14.f : 0.f;
            const NkRect field = { rowR.x, rowR.y, rowR.w - labelW, rowR.h };

            // Focus au clic (clic direct = réactif ; pas de gate hotIdPrev ici).
            // Le placement du caret sous le pointeur est géré par TextEditField.
            const bool over = NkGuiRectContains(field, ctx.input.mousePos)
                           && (ctx.activeId == NKGUI_ID_NONE || ctx.activeId == id);
            if (over) ctx.hotId = id;
            if (over && ctx.input.mouseClicked[0] && ctx.inputId != id) {
                ctx.inputId = id;
                ctx.inputScroll = 0.f;
            }
            const bool focused   = (ctx.inputId == id);
            const bool submitted = TextEditField(ctx, field, buf, bufSize, focused, flags, maxChars);

            if (ctx.font && ctx.font->Valid() && label && *label) {
                ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
                               { field.x + field.w + 12.f, CenteredBaseline(ctx, field) }, label, ctx.theme.text);
            }
            return submitted;
        }

        bool InputText(NkGuiContext& ctx, const char* label, char* buf, int32 bufSize) noexcept {
            return InputTextEx(ctx, label, buf, bufSize, NkGuiInputFlags::None, -1);
        }

        // ── Helpers de lignes (multi-ligne) — texte = lignes séparées par '\n' ─
        static int32 MlCountLines(const char* b, int32 len) noexcept {
            int32 n = 1; for (int32 i = 0; i < len; ++i) if (b[i] == '\n') ++n; return n;
        }
        static int32 MlLineStart(const char* b, int32 len, int32 line) noexcept {
            if (line <= 0) return 0;
            int32 cnt = 0;
            for (int32 i = 0; i < len; ++i) if (b[i] == '\n') { if (++cnt == line) return i + 1; }
            return len;
        }
        static int32 MlLineEnd(const char* b, int32 len, int32 ls) noexcept {
            int32 i = ls; while (i < len && b[i] != '\n') ++i; return i;
        }
        static int32 MlLineOf(const char* b, int32 len, int32 off) noexcept {
            int32 n = 0; for (int32 i = 0; i < off && i < len; ++i) if (b[i] == '\n') ++n; return n;
        }

        // Champ de saisie MULTI-LIGNE dans `rect` : édition 2D (Entrée = saut de ligne,
        // flèches ↑/↓ entre lignes en gardant la colonne, Home/End par ligne), clic 2D,
        // auto-défilement vers le caret + molette + scrollbar. Flags : ReadOnly, filtres
        // (CharsDecimal/Hex/Uppercase/NoBlank) + maxChars. Retourne true si le texte a
        // changé. `idStr` = id stable.
        bool InputTextMultiline(NkGuiContext& ctx, const char* idStr, char* buf, int32 bufSize,
                                const NkRect& rect, NkGuiInputFlags flags, int32 maxChars) noexcept {
            if (!buf || bufSize <= 1) return false;
            const NkGuiId id    = ctx.GetId(idStr);
            const bool readOnly = NkGuiHasInputFlag(flags, NkGuiInputFlags::ReadOnly);
            const NkFont* face  = (ctx.font && ctx.font->Valid()) ? ctx.font->Face() : nullptr;
            const float32 lineH = (ctx.font && ctx.font->Valid()) ? ctx.font->LineHeight() : 16.f;
            const float32 padX = 6.f, padY = 4.f;

            // Focus au clic.
            const bool over = NkGuiRectContains(rect, ctx.input.mousePos)
                           && (ctx.activeId == NKGUI_ID_NONE || ctx.activeId == id);
            if (over) ctx.hotId = id;
            if (over && ctx.input.mouseClicked[0] && ctx.inputId != id) ctx.inputId = id;
            const bool focused = (ctx.inputId == id);

            // Fond + bordure.
            ctx.DL().AddRectFilled(rect, readOnly ? NkColor{ 28, 30, 36, 255 } : ctx.theme.track, ctx.theme.rounding);
            ctx.DL().AddRect(rect, focused ? ctx.theme.accent : ctx.theme.border, focused ? 1.5f : 1.f);

            int32 len = 0; while (len < bufSize - 1 && buf[len]) ++len;
            int32 caret = ctx.inputCaret; if (caret > len) caret = len; if (caret < 0) caret = 0;
            bool changed = false;

            NkGuiScrollState st = ScrollGet(ctx, id);
            const float32 sbW   = 12.f;
            const NkRect  inner = { rect.x + padX, rect.y + padY, rect.w - padX * 2.f - sbW, rect.h - padY * 2.f };

            if (focused) {
                ctx.interact = NkGuiInteract::EditWidget;
                // Clic → caret 2D (ligne par y, colonne par x).
                if (ctx.input.mouseClicked[0] && over && face) {
                    float32 relY = ctx.input.mousePos.y - inner.y + st.y;
                    int32 line = (relY > 0.f) ? static_cast<int32>(relY / lineH) : 0;
                    const int32 nL = MlCountLines(buf, len);
                    if (line >= nL) line = nL - 1;
                    const int32 ls = MlLineStart(buf, len, line);
                    const int32 le = MlLineEnd(buf, len, ls);
                    caret = ls + CaretFromX(face, buf + ls, le - ls, ctx.input.mousePos.x - inner.x);
                    ctx.inputClickConsumed = true;
                }
                if (!readOnly) {
                    for (int32 k = 0; k < ctx.input.charCount; ++k) {
                        uint32 cp = ctx.input.chars[k];
                        if (cp < 32u) continue;
                        cp = FilterChar(cp, flags); if (cp == 0u) continue;
                        if (maxChars >= 0 && CountCp(buf, buf + len) >= maxChars) continue;  // limite
                        char tmp[5] = {}; const int32 n = NkFontEncodeUTF8(cp, tmp, 5);
                        if (n > 0 && len + n < bufSize) {
                            ::memmove(buf + caret + n, buf + caret, static_cast<usize>(len - caret + 1));
                            ::memcpy(buf + caret, tmp, static_cast<usize>(n));
                            caret += n; len += n; changed = true;
                        }
                    }
                    // Entrée = saut de ligne (pas de validation en multi-ligne).
                    if (ctx.input.KeyPressed(NkGuiKey::Enter) && len + 1 < bufSize
                        && (maxChars < 0 || CountCp(buf, buf + len) < maxChars)) {
                        ::memmove(buf + caret + 1, buf + caret, static_cast<usize>(len - caret + 1));
                        buf[caret] = '\n'; caret += 1; len += 1; changed = true;
                    }
                    if (ctx.input.KeyPressedRepeat(NkGuiKey::Backspace) && caret > 0) {
                        const int32 p = PrevCharStart(buf, caret); const int32 n = caret - p;
                        ::memmove(buf + p, buf + caret, static_cast<usize>(len - caret + 1));
                        caret = p; len -= n; changed = true;
                    }
                    if (ctx.input.KeyPressedRepeat(NkGuiKey::Delete) && caret < len) {
                        const int32 nx = NextCharStart(buf, len, caret); const int32 n = nx - caret;
                        ::memmove(buf + caret, buf + nx, static_cast<usize>(len - nx + 1));
                        len -= n; changed = true;
                    }
                }
                // Navigation (autorisée en lecture seule).
                if (ctx.input.KeyPressedRepeat(NkGuiKey::Left)  && caret > 0)   caret = PrevCharStart(buf, caret);
                if (ctx.input.KeyPressedRepeat(NkGuiKey::Right) && caret < len) caret = NextCharStart(buf, len, caret);
                if ((ctx.input.KeyPressedRepeat(NkGuiKey::Up) || ctx.input.KeyPressedRepeat(NkGuiKey::Down)) && face) {
                    const int32 cl  = MlLineOf(buf, len, caret);
                    const int32 cls = MlLineStart(buf, len, cl);
                    const float32 cx = face->CalcTextSizeX(buf + cls, buf + caret);   // colonne pixel courante
                    const int32 nL  = MlCountLines(buf, len);
                    int32 tl = cl + (ctx.input.KeyPressedRepeat(NkGuiKey::Up) ? -1 : 1);
                    if (tl >= 0 && tl < nL) {
                        const int32 tls = MlLineStart(buf, len, tl);
                        const int32 tle = MlLineEnd(buf, len, tls);
                        caret = tls + CaretFromX(face, buf + tls, tle - tls, cx);
                    }
                }
                if (ctx.input.KeyPressed(NkGuiKey::Home)) caret = MlLineStart(buf, len, MlLineOf(buf, len, caret));
                if (ctx.input.KeyPressed(NkGuiKey::End))  caret = MlLineEnd(buf, len, MlLineStart(buf, len, MlLineOf(buf, len, caret)));
                ctx.inputCaret = caret;
            }

            // Molette (survol).
            if (over && ctx.input.wheel != 0.f) st.y -= ctx.input.wheel * 36.f;

            // Auto-défilement vers le caret.
            const int32   caretLine = MlLineOf(buf, len, caret);
            const float32 caretY    = caretLine * lineH;
            if (focused) {
                if (caretY - st.y < 0.f) st.y = caretY;
                if (caretY + lineH - st.y > inner.h) st.y = caretY + lineH - inner.h;
            }
            const int32   nLines   = MlCountLines(buf, len);
            const float32 contentH = nLines * lineH;
            float32 maxY = contentH - inner.h; if (maxY < 0.f) maxY = 0.f;
            if (st.y < 0.f) st.y = 0.f; if (st.y > maxY) st.y = maxY;

            // Rendu des lignes visibles + caret.
            ctx.DL().PushClipRect(inner, true);
            if (face) {
                const NkColor tc = readOnly ? ctx.theme.textDisabled : ctx.theme.text;
                int32 line = (st.y > 0.f) ? static_cast<int32>(st.y / lineH) : 0;
                for (; line < nLines; ++line) {
                    const float32 y = inner.y + line * lineH - st.y;
                    if (y >= inner.y + inner.h) break;
                    const int32 ls = MlLineStart(buf, len, line);
                    const int32 le = MlLineEnd(buf, len, ls);
                    ctx.DL().AddTextRange(face, ctx.font->TexId(),
                                          { inner.x, y + ctx.font->Ascent() }, buf + ls, buf + le, tc);
                }
                if (focused && (static_cast<int32>(ctx.time * 2.f) & 1) == 0) {
                    const int32 cls = MlLineStart(buf, len, caretLine);
                    const float32 cx = inner.x + face->CalcTextSizeX(buf + cls, buf + caret);
                    const float32 cy = inner.y + caretLine * lineH - st.y;
                    ctx.DL().AddRectFilled({ cx, cy + 2.f, 1.5f, lineH - 4.f }, tc);
                }
            }
            ctx.DL().PopClipRect();

            // Scrollbar verticale (draggable) si débordement.
            if (maxY > 0.f) {
                const NkRect track = { rect.x + rect.w - sbW, rect.y, sbW, rect.h };
                ctx.DL().AddRectFilled(track, NkColor{ 20, 22, 28, 255 }, 0.f);
                float32 thumbH = rect.h * (inner.h / contentH);
                if (thumbH < 24.f) thumbH = 24.f; if (thumbH > track.h) thumbH = track.h;
                const float32 thumbY = track.y + (st.y / maxY) * (track.h - thumbH);
                const NkRect  thumb  = { track.x + 2.f, thumbY, sbW - 4.f, thumbH };
                const NkGuiId sbId = id ^ 0x4D4C5343u;
                bool hov = false, held = false;
                ctx.ButtonBehavior(sbId, thumb, NkGuiButtonFlags::None, -1.f, -1.f, &hov, &held);
                if (ctx.activeId == sbId && (track.h - thumbH) > 0.f) {
                    st.y = ((ctx.input.mousePos.y - track.y - thumbH * 0.5f) / (track.h - thumbH)) * maxY;
                    if (st.y < 0.f) st.y = 0.f; if (st.y > maxY) st.y = maxY;
                }
                ctx.DL().AddRectFilled(thumb, (hov || ctx.activeId == sbId) ? ctx.theme.buttonHover : ctx.theme.button, 3.f);
            }

            st.maxY = maxY;
            ScrollSet(ctx, id, st);
            return changed;
        }

        void Separator(NkGuiContext& ctx) noexcept {
            const NkRect r = ctx.NextItemRect(0.f, 1.f);
            ctx.DL().AddRectFilled({ r.x, r.y, r.w, 1.f }, ctx.theme.border);
        }

        bool TreeNode(NkGuiContext& ctx, const char* label) noexcept {
            const float32 h  = ctx.ItemHeight();
            const NkRect  r  = ctx.NextItemRect(0.f, h);
            const NkGuiId id = ctx.GetId(label);
            bool open = ctx.IsNodeOpen(id);

            bool hov = false, held = false;
            if (ctx.ButtonBehavior(id, r, NkGuiButtonFlags::None, -1.f, -1.f, &hov, &held)) {
                open = !open; ctx.SetNodeOpen(id, open);
            }
            if (hov || held) ctx.DL().AddRectFilled(r, ctx.theme.header, 3.f);

            // Flèche : pointe vers le bas (ouvert) ou vers la droite (fermé).
            const float32 a  = h * 0.22f;
            const NkVec2  cc = { r.x + 9.f, r.y + h * 0.5f };
            if (open) {
                ctx.DL().AddTriangleFilled({ cc.x - a, cc.y - a * 0.6f }, { cc.x + a, cc.y - a * 0.6f },
                                         { cc.x, cc.y + a * 0.8f }, ctx.theme.text);
            } else {
                ctx.DL().AddTriangleFilled({ cc.x - a * 0.6f, cc.y - a }, { cc.x - a * 0.6f, cc.y + a },
                                         { cc.x + a * 0.8f, cc.y }, ctx.theme.text);
            }
            if (ctx.font && ctx.font->Valid()) {
                ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
                               { r.x + 24.f, CenteredBaseline(ctx, r) }, label, ctx.theme.text);
            }
            if (open) ctx.Indent(18.f);
            return open;
        }

        void TreePop(NkGuiContext& ctx) noexcept { ctx.Indent(-18.f); }

        // Dessine le chevron d'un nœud (bas = ouvert, droite = fermé).
        static void DrawTreeArrow(NkGuiContext& ctx, const NkRect& r, float32 h, bool open) noexcept {
            const float32 a  = h * 0.22f;
            const NkVec2  cc = { r.x + 9.f, r.y + h * 0.5f };
            if (open)
                ctx.DL().AddTriangleFilled({ cc.x - a, cc.y - a * 0.6f }, { cc.x + a, cc.y - a * 0.6f },
                                         { cc.x, cc.y + a * 0.8f }, ctx.theme.text);
            else
                ctx.DL().AddTriangleFilled({ cc.x - a * 0.6f, cc.y - a }, { cc.x - a * 0.6f, cc.y + a },
                                         { cc.x + a * 0.8f, cc.y }, ctx.theme.text);
        }

        // Nœud d'arbre RENOMMABLE : clic = ouvrir/fermer, double-clic (ou Maj+Entrée)
        // = édition inline du libellé (dans `label`, tampon mutable). `idStr` = id
        // STABLE. Entrée valide, Échap annule. Retourne l'état ouvert.
        bool TreeNodeEditable(NkGuiContext& ctx, const char* idStr,
                              char* label, int32 bufSize, bool allowRename) noexcept {
            if (!label || bufSize <= 1) return false;
            const float32 h  = ctx.ItemHeight();
            const NkRect  r  = ctx.NextItemRect(0.f, h);
            const NkGuiId id = ctx.GetId(idStr);
            bool open = ctx.IsNodeOpen(id);
            const NkRect editR = { r.x + 22.f, r.y, r.w - 22.f, r.h };  // après le chevron

            // ── Mode renommage ────────────────────────────────────────────────
            if (ctx.renameId == id) {
                const bool clickAway = ctx.input.mouseClicked[0]
                                    && !NkGuiRectContains(editR, ctx.input.mousePos);
                ctx.inputId = id;
                ctx.inputClickConsumed = true;
                const bool submitted = TextEditField(ctx, editR, label, bufSize, true);
                if (ctx.input.KeyPressed(NkGuiKey::Escape)) {
                    int32 n = 0;
                    while (n < bufSize - 1 && ctx.renameBackup[n]) { label[n] = ctx.renameBackup[n]; ++n; }
                    label[n] = '\0';
                    ctx.renameId = NKGUI_ID_NONE; ctx.inputId = NKGUI_ID_NONE;
                } else if (submitted || clickAway) {
                    ctx.renameId = NKGUI_ID_NONE; ctx.inputId = NKGUI_ID_NONE;
                }
                DrawTreeArrow(ctx, r, h, open);
                if (open) ctx.Indent(18.f);
                return open;
            }

            bool hov = false, held = false;
            const bool clicked = ctx.ButtonBehavior(id, r, NkGuiButtonFlags::None, -1.f, -1.f, &hov, &held);
            if (clicked) { open = !open; ctx.SetNodeOpen(id, open); }

            // Double-clic → renommer (sauf allowRename=false), et on annule la
            // bascule du 1er clic qui a précédé.
            const bool startRename = (hov && ctx.input.mouseDoubleClicked[0])
                                  || (hov && ctx.input.KeyPressed(NkGuiKey::Enter) && ctx.input.shiftDown);
            if (startRename && allowRename && !ctx.IsDisabled()) {
                open = !open; ctx.SetNodeOpen(id, open);   // restaure l'état d'avant le double-clic
                ctx.renameId    = id;
                ctx.inputId     = id;
                int32 l = 0; while (l < bufSize - 1 && label[l]) ++l;
                ctx.inputCaret  = l;
                ctx.inputScroll = 0.f;
                int32 n = 0; while (n < 127 && label[n]) { ctx.renameBackup[n] = label[n]; ++n; }
                ctx.renameBackup[n] = '\0';
            }

            if (hov || held) ctx.DL().AddRectFilled(r, ctx.theme.header, 3.f);
            DrawTreeArrow(ctx, r, h, open);
            if (ctx.font && ctx.font->Valid()) {
                ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
                               { r.x + 24.f, CenteredBaseline(ctx, r) }, label, ctx.theme.text);
            }
            if (open) ctx.Indent(18.f);
            return open;
        }

        bool Selectable(NkGuiContext& ctx, const char* label, bool selected) noexcept {
            const float32 h  = ctx.ItemHeight();
            const NkRect  r  = ctx.NextItemRect(0.f, h);   // ligne pleine largeur
            const NkGuiId id = ctx.GetId(label);

            bool hov = false, held = false;
            const bool clicked = ctx.ButtonBehavior(id, r, NkGuiButtonFlags::None, -1.f, -1.f, &hov, &held);

            // Hook de style (re-skin total de la ligne).
            if (StyleDraw(ctx, NkGuiStyleKind::Selectable, r, id, label, hov, held, selected, ctx.IsDisabled()))
                return clicked;

            if (selected)   ctx.DL().AddRectFilled(r, ctx.theme.selection, 3.f);
            else if (hov)   ctx.DL().AddRectFilled(r, ctx.theme.header, 3.f);

            const NkColor lc = ctx.IsDisabled() ? ctx.theme.textDisabled
                             : selected          ? NkColor{ 255, 255, 255, 255 }
                                                  : ctx.theme.text;
            if (ctx.font && ctx.font->Valid()) {
                ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
                               { r.x + 6.f, CenteredBaseline(ctx, r) }, label, lc);
            }
            return clicked;
        }

        bool SelectableEditable(NkGuiContext& ctx, const char* idStr,
                                char* label, int32 bufSize, bool selected, bool allowRename) noexcept {
            if (!label || bufSize <= 1) return false;
            const float32 h  = ctx.ItemHeight();
            const NkRect  r  = ctx.NextItemRect(0.f, h);
            const NkGuiId id = ctx.GetId(idStr);   // id STABLE (≠ libellé édité)

            // ── Mode renommage : éditeur inline à la place ────────────────────
            if (ctx.renameId == id) {
                const bool clickAway = ctx.input.mouseClicked[0]
                                    && !NkGuiRectContains(r, ctx.input.mousePos);
                ctx.inputId = id;
                ctx.inputClickConsumed = true;
                const bool submitted = TextEditField(ctx, r, label, bufSize, true);
                if (ctx.input.KeyPressed(NkGuiKey::Escape)) {
                    int32 n = 0;
                    while (n < bufSize - 1 && ctx.renameBackup[n]) { label[n] = ctx.renameBackup[n]; ++n; }
                    label[n] = '\0';
                    ctx.renameId = NKGUI_ID_NONE; ctx.inputId = NKGUI_ID_NONE;
                } else if (submitted || clickAway) {
                    ctx.renameId = NKGUI_ID_NONE; ctx.inputId = NKGUI_ID_NONE;
                }
                return false;
            }

            bool hov = false, held = false;
            const bool clicked = ctx.ButtonBehavior(id, r, NkGuiButtonFlags::None, -1.f, -1.f, &hov, &held);

            // Double-clic (ou Maj+Entrée pendant survol) → renommer — sauf si
            // l'appelant refuse le renommage de cet élément (allowRename=false).
            const bool startRename = (hov && ctx.input.mouseDoubleClicked[0])
                                  || (hov && ctx.input.KeyPressed(NkGuiKey::Enter) && ctx.input.shiftDown);
            if (startRename && allowRename && !ctx.IsDisabled()) {
                ctx.renameId    = id;
                ctx.inputId     = id;
                int32 l = 0; while (l < bufSize - 1 && label[l]) ++l;
                ctx.inputCaret  = l;
                ctx.inputScroll = 0.f;
                int32 n = 0; while (n < 127 && label[n]) { ctx.renameBackup[n] = label[n]; ++n; }
                ctx.renameBackup[n] = '\0';
            }

            if (selected)   ctx.DL().AddRectFilled(r, ctx.theme.selection, 3.f);
            else if (hov)   ctx.DL().AddRectFilled(r, ctx.theme.header, 3.f);

            const NkColor lc = ctx.IsDisabled() ? ctx.theme.textDisabled
                             : selected          ? NkColor{ 255, 255, 255, 255 }
                                                  : ctx.theme.text;
            if (ctx.font && ctx.font->Valid()) {
                ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
                               { r.x + 6.f, CenteredBaseline(ctx, r) }, label, lc);
            }
            return clicked;
        }

        bool SelectItem(NkGuiContext& ctx, const char* label) noexcept {
            const int32   idx = ctx.selIdx++;
            const float32 h   = ctx.ItemHeight();
            const NkRect  r   = ctx.NextItemRect(0.f, h);
            const NkGuiId id  = ctx.GetId(label);

            bool hov = false, held = false;
            const bool clicked = ctx.ButtonBehavior(id, r, NkGuiButtonFlags::None, -1.f, -1.f, &hov, &held);
            if (clicked) ctx.ApplySelectClick(idx);

            const bool selected = ctx.selMask && idx >= 0 && idx < ctx.selCount && ctx.selMask[idx];
            const bool focused  = (ctx.activeSelList == ctx.curSelList) && (idx == ctx.selFocus);

            if (selected)  ctx.DL().AddRectFilled(r, ctx.theme.selection, 3.f);
            else if (hov)  ctx.DL().AddRectFilled(r, ctx.theme.header, 3.f);
            if (focused)   ctx.DL().AddRect(r, ctx.theme.accent, 1.5f);   // liseré de focus clavier

            const NkColor lc = ctx.IsDisabled() ? ctx.theme.textDisabled
                             : selected          ? NkColor{ 255, 255, 255, 255 }
                                                  : ctx.theme.text;
            if (ctx.font && ctx.font->Valid()) {
                ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
                               { r.x + 8.f, CenteredBaseline(ctx, r) }, label, lc);
            }
            return clicked;
        }

        // Élément sélectionnable RENOMMABLE (double-clic / F2 → édition inline).
        // `idStr` = identifiant STABLE (ne change pas quand on édite le libellé).
        // `label` = tampon mutable (le rename écrit dedans). Entrée valide,
        // Échap annule (restaure la sauvegarde). Retourne true au clic de sélection.
        bool SelectItemEditable(NkGuiContext& ctx, const char* idStr,
                                char* label, int32 bufSize, bool allowRename) noexcept {
            if (!label || bufSize <= 1) return false;
            const int32   idx = ctx.selIdx++;
            const float32 h   = ctx.ItemHeight();
            const NkRect  r   = ctx.NextItemRect(0.f, h);
            const NkGuiId id  = ctx.GetId(idStr);

            // ── Mode renommage : on dessine l'éditeur de texte à la place ─────
            if (ctx.renameId == id) {
                const bool clickAway = ctx.input.mouseClicked[0]
                                    && !NkGuiRectContains(r, ctx.input.mousePos);
                ctx.inputId = id;                       // garde le focus sur l'éditeur
                ctx.inputClickConsumed = true;          // empêche EndFrame de défocaliser
                const bool submitted = TextEditField(ctx, r, label, bufSize, true);
                if (ctx.input.KeyPressed(NkGuiKey::Escape)) {
                    int32 n = 0;
                    while (n < bufSize - 1 && ctx.renameBackup[n]) { label[n] = ctx.renameBackup[n]; ++n; }
                    label[n] = '\0';
                    ctx.renameId = NKGUI_ID_NONE;
                    ctx.inputId  = NKGUI_ID_NONE;
                } else if (submitted || clickAway) {     // Entrée / clic ailleurs = valide
                    ctx.renameId = NKGUI_ID_NONE;
                    ctx.inputId  = NKGUI_ID_NONE;
                }
                return false;
            }

            bool hov = false, held = false;
            const bool clicked = ctx.ButtonBehavior(id, r, NkGuiButtonFlags::None, -1.f, -1.f, &hov, &held);
            if (clicked) ctx.ApplySelectClick(idx);

            const bool selected = ctx.selMask && idx >= 0 && idx < ctx.selCount && ctx.selMask[idx];
            const bool focused  = (ctx.activeSelList == ctx.curSelList) && (idx == ctx.selFocus);

            // Double-clic OU Maj+Entrée sur l'item focalisé → entrer en renommage
            // (sauf si l'appelant refuse via allowRename=false).
            const bool startRename = (hov && ctx.input.mouseDoubleClicked[0])
                                  || (focused && ctx.input.KeyPressed(NkGuiKey::Enter)
                                      && ctx.input.shiftDown);
            if (startRename && allowRename && !ctx.IsDisabled()) {
                ctx.renameId    = id;
                ctx.inputId     = id;
                int32 len = 0; while (len < bufSize - 1 && label[len]) ++len;
                ctx.inputCaret  = len;
                ctx.inputScroll = 0.f;
                int32 n = 0; while (n < 127 && label[n]) { ctx.renameBackup[n] = label[n]; ++n; }
                ctx.renameBackup[n] = '\0';
            }

            if (selected)  ctx.DL().AddRectFilled(r, ctx.theme.selection, 3.f);
            else if (hov)  ctx.DL().AddRectFilled(r, ctx.theme.header, 3.f);
            if (focused)   ctx.DL().AddRect(r, ctx.theme.accent, 1.5f);

            const NkColor lc = ctx.IsDisabled() ? ctx.theme.textDisabled
                             : selected          ? NkColor{ 255, 255, 255, 255 }
                                                  : ctx.theme.text;
            if (ctx.font && ctx.font->Valid()) {
                ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
                               { r.x + 8.f, CenteredBaseline(ctx, r) }, label, lc);
            }
            return clicked;
        }

        int32 TabBarEx(NkGuiContext& ctx, const char* id, const char* const* labels,
                       int32 count, const bool* enabled) noexcept {
            if (count <= 0) return 0;
            const NkGuiId bar = ctx.GetId(id);
            int32 sel = ctx.GetTabIndex(bar);
            if (sel < 0 || sel >= count) { sel = 0; ctx.SetTabIndex(bar, 0); }
            const float32 h = ctx.ItemHeight();

            for (int32 i = 0; i < count; ++i) {
                const bool    en  = (enabled == nullptr) || enabled[i];
                const float32 tw  = ((ctx.font && ctx.font->Valid()) ? ctx.font->MeasureWidth(labels[i]) : 40.f) + 22.f;
                if (i > 0) ctx.SameLine(4.f);
                const NkRect  r   = ctx.NextItemRect(tw, h);
                const NkGuiId tid = NkGuiHashStr(labels[i], bar);

                bool hov = false, held = false;
                if (en) {
                    if (ctx.ButtonBehavior(tid, r, NkGuiButtonFlags::None, -1.f, -1.f, &hov, &held)) {
                        sel = i; ctx.SetTabIndex(bar, i);
                    }
                }
                const bool    selected = (i == sel);
                const NkColor bg = selected ? ctx.theme.panel
                                 : (en && hov) ? ctx.theme.buttonHover
                                             : ctx.theme.button;
                ctx.DL().AddRectFilled(r, bg, 4.f);
                if (selected) ctx.DL().AddRectFilled({ r.x, r.y + r.h - 3.f, r.w, 3.f }, ctx.theme.accent);
                else          ctx.DL().AddRect(r, ctx.theme.border, 1.f);
                const NkColor lc = !en      ? ctx.theme.textDisabled
                                 : selected ? ctx.theme.text
                                             : NkColor{ 180, 185, 196, 255 };
                DrawCenteredLabel(ctx, r, labels[i], lc);
            }
            return sel;
        }

        int32 TabBar(NkGuiContext& ctx, const char* id, const char* const* labels, int32 count) noexcept {
            return TabBarEx(ctx, id, labels, count, nullptr);
        }

        int32 TabBarEditable(NkGuiContext& ctx, const char* id, char* const* labels, int32 count,
                             int32 labelBufSize, const bool* enabled, const bool* allowRename) noexcept {
            if (count <= 0 || labelBufSize <= 1) return 0;
            const NkGuiId bar = ctx.GetId(id);
            int32 sel = ctx.GetTabIndex(bar);
            if (sel < 0 || sel >= count) { sel = 0; ctx.SetTabIndex(bar, 0); }
            const float32 h = ctx.ItemHeight();

            for (int32 i = 0; i < count; ++i) {
                const bool en    = (enabled == nullptr) || enabled[i];
                const bool canRn = (allowRename == nullptr) || allowRename[i];
                // ID STABLE par index (≠ libellé, qui change au renommage).
                NkGuiId tid = bar ^ static_cast<uint32>(i + 1); tid *= 16777619u; if (!tid) tid = 1u;
                const float32 labW = (ctx.font && ctx.font->Valid()) ? ctx.font->MeasureWidth(labels[i]) : 40.f;
                const float32 tw   = labW + 22.f;
                if (i > 0) ctx.SameLine(4.f);

                // ── Onglet en cours de renommage : éditeur inline ─────────────
                if (ctx.renameId == tid) {
                    const float32 ew = (tw > 90.f) ? tw : 90.f;
                    const NkRect  r  = ctx.NextItemRect(ew, h);
                    const bool clickAway = ctx.input.mouseClicked[0] && !NkGuiRectContains(r, ctx.input.mousePos);
                    ctx.inputId = tid; ctx.inputClickConsumed = true;
                    const bool submitted = TextEditField(ctx, r, labels[i], labelBufSize, true);
                    if (ctx.input.KeyPressed(NkGuiKey::Escape)) {
                        int32 n = 0;
                        while (n < labelBufSize - 1 && ctx.renameBackup[n]) { labels[i][n] = ctx.renameBackup[n]; ++n; }
                        labels[i][n] = '\0';
                        ctx.renameId = NKGUI_ID_NONE; ctx.inputId = NKGUI_ID_NONE;
                    } else if (submitted || clickAway) {
                        ctx.renameId = NKGUI_ID_NONE; ctx.inputId = NKGUI_ID_NONE;
                    }
                    continue;
                }

                const NkRect r = ctx.NextItemRect(tw, h);
                bool hov = false, held = false;
                if (en) {
                    if (ctx.ButtonBehavior(tid, r, NkGuiButtonFlags::None, -1.f, -1.f, &hov, &held)) {
                        sel = i; ctx.SetTabIndex(bar, i);
                    }
                }
                // Double-clic → renommer (sauf refus).
                if (en && canRn && hov && ctx.input.mouseDoubleClicked[0]) {
                    ctx.renameId    = tid;
                    ctx.inputId     = tid;
                    int32 l = 0; while (l < labelBufSize - 1 && labels[i][l]) ++l;
                    ctx.inputCaret  = l;
                    ctx.inputScroll = 0.f;
                    int32 n = 0; while (n < 127 && labels[i][n]) { ctx.renameBackup[n] = labels[i][n]; ++n; }
                    ctx.renameBackup[n] = '\0';
                }

                const bool    selected = (i == sel);
                const NkColor bg = selected ? ctx.theme.panel
                                 : (en && hov) ? ctx.theme.buttonHover
                                             : ctx.theme.button;
                ctx.DL().AddRectFilled(r, bg, 4.f);
                if (selected) ctx.DL().AddRectFilled({ r.x, r.y + r.h - 3.f, r.w, 3.f }, ctx.theme.accent);
                else          ctx.DL().AddRect(r, ctx.theme.border, 1.f);
                const NkColor lc = !en      ? ctx.theme.textDisabled
                                 : selected ? ctx.theme.text
                                             : NkColor{ 180, 185, 196, 255 };
                DrawCenteredLabel(ctx, r, labels[i], lc);
            }
            return sel;
        }

        bool BeginPanel(NkGuiContext& ctx, const char* title, const NkRect& r) noexcept {
            ctx.DL().AddRectFilled(r, ctx.theme.panel, ctx.theme.rounding);   // fond

            float32 top = r.y;
            if (title && *title) {
                const float32 th = ctx.ItemHeight();
                ctx.DL().AddRectFilled({ r.x, r.y, r.w, th }, ctx.theme.header, ctx.theme.rounding);
                ctx.DL().AddRectFilled({ r.x, r.y + th - 1.f, r.w, 1.f }, ctx.theme.border); // séparateur sous l'en-tête
                if (ctx.font && ctx.font->Valid()) {
                    ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
                                   { r.x + 10.f, r.y + (th - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent() },
                                   title, ctx.theme.text);
                }
                top = r.y + th;
            }

            // Bordure EN DERNIER → entoure tout le panneau (en-tête inclus), n'est
            // plus recouverte par le fond de la barre de titre.
            ctx.DL().AddRect(r, ctx.theme.border, 1.f);

            // Le contenu (sous l'en-tête) est une ZONE DÉFILABLE : si les widgets
            // dépassent la hauteur visible, une scrollbar apparaît automatiquement
            // (comme une fenêtre). Pas de débordement → aucune scrollbar (identique
            // à avant). Id dérivé du titre (titres de panneaux supposés distincts).
            const NkRect  content = { r.x, top, r.w, r.y + r.h - top };
            const NkGuiId id      = ctx.GetId((title && *title) ? title : "##panel");
            return BeginScrollFrame(ctx, id, content, false);
        }

        void EndPanel(NkGuiContext& ctx) noexcept { EndScrollFrame(ctx); }

        // ── Fenêtres flottantes ─────────────────────────────────────────────────
        namespace {
            // Forward-decls des helpers de dock (définis plus bas, même namespace anonyme)
            // pour le mode HÔTE de Begin (#3).
            void DockComputeRects(NkGuiContext&, int32, const NkRect&) noexcept;
            void DockRenderNode(NkGuiContext&, int32) noexcept;
            void DockWindowIntoHost(NkGuiContext&, NkGuiId hostId, NkGuiId winId, int32 zone) noexcept;
            void DockDragTargets(NkGuiContext&, int32* rootRef, const NkRect& rect, NkGuiId hostId) noexcept;

            NkGuiWindowMeta* WinFind(NkGuiContext& ctx, NkGuiId id, int32& outIdx) noexcept {
                for (uint32 i = 0; i < ctx.windowMeta.Size(); ++i)
                    if (ctx.windowMeta[i].id == id) { outIdx = static_cast<int32>(i); return &ctx.windowMeta[i]; }
                return nullptr;
            }
            // Redim multi-bords : 4 coins + 4 bords. Ancres = wr0 (positions figées de la
            // frame) ; met à jour `wr`. Bits de masque : G=1 D=2 H=4 B=8.
            void WindowBorders(NkGuiContext& ctx, NkGuiId id, NkRect& wr, const NkRect& wr0,
                               float32 minW, float32 minH) noexcept {
                const float32 b = 6.f, cs = 14.f;
                const float32 x = wr0.x, y = wr0.y, w = wr0.w, h = wr0.h;
                struct Z { NkRect r; uint8 m; bool ns; };
                const Z z[8] = {
                    { { x - b,          y - b,          cs,           cs       }, 0x5, false }, // coin HG
                    { { x + w - cs + b, y - b,          cs,           cs       }, 0x6, false }, // coin HD
                    { { x - b,          y + h - cs + b, cs,           cs       }, 0x9, false }, // coin BG
                    { { x + w - cs + b, y + h - cs + b, cs,           cs       }, 0xA, false }, // coin BD
                    { { x - b,          y + cs,         2.f * b,      h - 2.f * cs }, 0x1, false }, // bord G
                    { { x + w - b,      y + cs,         2.f * b,      h - 2.f * cs }, 0x2, false }, // bord D
                    { { x + cs,         y - b,          w - 2.f * cs, 2.f * b      }, 0x4, true  }, // bord H
                    { { x + cs,         y + h - b,      w - 2.f * cs, 2.f * b      }, 0x8, true  }, // bord B
                };
                for (int32 i = 0; i < 8; ++i) {
                    const NkGuiId zid = id ^ (0x51000000u + static_cast<NkGuiId>(i));
                    bool hv = false, hd = false;
                    ctx.ButtonBehavior(zid, z[i].r, NkGuiButtonFlags::None, -1.f, -1.f, &hv, &hd);
                    if (hv || ctx.activeId == zid) ctx.wantCursor = z[i].ns ? NkGuiCursor::ResizeNS : NkGuiCursor::ResizeEW;
                    if (ctx.activeId == zid) {
                        const uint8 m = z[i].m;
                        if (m & 0x2) wr.w = math::NkMax(minW, ctx.input.mousePos.x - x);
                        if (m & 0x8) wr.h = math::NkMax(minH, ctx.input.mousePos.y - y);
                        if (m & 0x1) { const float32 right = x + w; float32 nx = ctx.input.mousePos.x; if (nx > right - minW) nx = right - minW; wr.x = nx; wr.w = right - nx; }
                        if (m & 0x4) { const float32 bot   = y + h; float32 ny = ctx.input.mousePos.y; if (ny > bot   - minH) ny = bot   - minH; wr.y = ny; wr.h = bot   - ny; }
                    }
                }
            }
            // #3 : dessine le CHROME + l'arbre d'un hôte de dock flottant — UNE SEULE FOIS
            // par frame (à la 1re des {hôte, ses onglets} rencontrée → ordre-indépendant :
            // le contenu des onglets se dessine APRÈS, donc au-dessus du fond de l'hôte).
            // Alloue (ou réutilise) le slot winDL de l'hôte cette frame → l'hôte vit sur
            // la MÊME couche que les fenêtres flottantes (z-order correct).
            int32 EnsureHostSlot(NkGuiContext& ctx, NkGuiWindowMeta* m, int32 mi) noexcept {
                if (m->frameDL >= 0) return m->frameDL;
                if (ctx.winCount >= NkGuiContext::WinMax) return -1;
                const int32 k = ctx.winCount++;
                ctx.winDL[k].Reset();
                ctx.winMeta[k] = mi; ctx.winZ[k] = m->zOrder;
                m->frameDL = k;
                return k;
            }
            void EnsureHostRendered(NkGuiContext& ctx, NkGuiWindowMeta* m, int32 mi, NkGuiId hostId) noexcept {
                if (m->hostRendered) return;
                m->hostRendered = true;
                const int32   slot    = EnsureHostSlot(ctx, m, mi);
                const int32   saveWin = ctx.curWindow; const NkGuiId saveId = ctx.curWindowId;
                ctx.curWindow = slot; ctx.curWindowId = hostId;   // dessine + interagit dans le winDL de l'hôte
                NkRect& hr = m->hostRect;
                const float32 tt = ctx.ItemHeight();
                const NkRect  wr0 = hr;
                const float32 minW = 200.f, minH = tt + 90.f;
                if (ctx.input.mouseClicked[0] && ctx.hoveredWindowId == hostId) m->zOrder = ++ctx.windowZTop;  // passage devant
                if (slot >= 0) ctx.winZ[slot] = m->zOrder;
                const NkRect titleR0 = { wr0.x, wr0.y, wr0.w, tt };
                { bool hv = false, hd = false;                 // déplacement (barre de titre)
                  ctx.ButtonBehavior(hostId ^ 0x70571u, titleR0, NkGuiButtonFlags::None, -1.f, -1.f, &hv, &hd);
                  if (ctx.activeId == (hostId ^ 0x70571u)) {
                      if (ctx.input.mouseClicked[0]) ctx.winDragOff = { ctx.input.mousePos.x - wr0.x, ctx.input.mousePos.y - wr0.y };
                      hr.x = ctx.input.mousePos.x - ctx.winDragOff.x;
                      hr.y = ctx.input.mousePos.y - ctx.winDragOff.y;
                  } }
                WindowBorders(ctx, hostId ^ 0x80571u, hr, wr0, minW, minH);
                if (hr.w < minW) hr.w = minW; if (hr.h < minH) hr.h = minH;
                ctx.DL().AddRectFilled(hr, ctx.theme.panel, ctx.theme.rounding);
                const NkRect titleR = { hr.x, hr.y, hr.w, tt };
                ctx.DL().AddRectFilled(titleR, ctx.theme.header, ctx.theme.rounding);
                for (int32 g = 0; g < 3; ++g)                  // poignée (3 points) = « déplacer »
                    ctx.DL().AddRectFilled({ hr.x + 8.f + g * 5.f, hr.y + tt * 0.5f - 1.f, 2.f, 2.f }, ctx.theme.textDisabled);
                ctx.DL().AddRect(hr, ctx.theme.border, 1.f);
                const NkRect treeR = { hr.x, hr.y + tt, hr.w, hr.h - tt };
                DockComputeRects(ctx, m->hostRoot, treeR);
                DockRenderNode(ctx, m->hostRoot);              // tab bars + fond contenu + pose dockRect/dockActiveTab
                ctx.DL().AddLine({ hr.x + hr.w - 3.f, hr.y + hr.h - 12.f }, { hr.x + hr.w - 12.f, hr.y + hr.h - 3.f }, ctx.theme.border, 1.5f);
                DockDragTargets(ctx, &m->hostRoot, treeR, hostId);   // boussole complète (ajout 3e+ fenêtre)
                m->rect = m->hostRect;                         // sync (survol EndFrame + revert flottant)
                ctx.curWindow = saveWin; ctx.curWindowId = saveId;
            }
            // Ouvre le contenu de l'onglet `m` ancré. `dlSlot` = winDL de l'hôte (-1 =
            // couche fond du DockSpace central) ; `hostWinId` = id de l'hôte (occlusion).
            bool BeginDockedTabContent(NkGuiContext& ctx, NkGuiWindowMeta* m, NkGuiId id, int32 dlSlot, NkGuiId hostWinId) noexcept {
                ctx.curWindow = dlSlot; ctx.curWindowId = (dlSlot >= 0) ? hostWinId : NKGUI_ID_NONE; ctx.curWindowDocked = false;
                if (!m->dockActiveTab) return false;           // onglet inactif → contenu caché
                ctx.curWindowDocked = true;
                ctx.winSavedLayout  = ctx.layout;
                BeginScrollFrame(ctx, id ^ 0x5555u, m->dockRect, /*horizontal=*/true, /*fillWidth=*/true);
                return true;
            }
            // Racine de l'arbre contenant `leaf` (remonte les parents).
            int32 DockRootOf(NkGuiContext& ctx, int32 leaf) noexcept {
                int32 n = leaf, guard = 0;
                while (n >= 0 && ctx.dockNodes[n].parent >= 0 && guard++ < 512) n = ctx.dockNodes[n].parent;
                return n;
            }
            // Fenêtre-HÔTE dont hostRoot == `root` (NONE = central ou aucune). Vérité = l'arbre,
            // PAS le champ dockHost (qui se désynchronise lors des splits) → resync chaque frame.
            NkGuiId DockHostOfRoot(NkGuiContext& ctx, int32 root) noexcept {
                if (root < 0 || root == ctx.dockRoot) return NKGUI_ID_NONE;
                for (uint32 i = 0; i < ctx.windowMeta.Size(); ++i)
                    if (ctx.windowMeta[i].hostRoot == root) return ctx.windowMeta[i].id;
                return NKGUI_ID_NONE;
            }
        }

        bool Begin(NkGuiContext& ctx, const char* title, bool* open, NkGuiWindowFlags flags) noexcept {
            if (open && !*open) return false;                       // fermée → non affichée
            const NkGuiId id = ctx.GetId(title);
            int32 mi = -1;
            NkGuiWindowMeta* m = WinFind(ctx, id, mi);
            if (!m) {
                NkGuiWindowMeta nm; nm.id = id;
                const float32 off = static_cast<float32>(ctx.windowMeta.Size() % 8) * 28.f;
                nm.rect   = { 90.f + off, 80.f + off, 320.f, 210.f };
                nm.zOrder = ++ctx.windowZTop;
                ctx.windowMeta.PushBack(nm);
                mi = static_cast<int32>(ctx.windowMeta.Size()) - 1;
                m  = &ctx.windowMeta[mi];
            }
            // Mémorise le titre (les onglets de dock en ont besoin avant ce Begin).
            { int32 n = 0; while (n < 47 && title[n]) { m->title[n] = title[n]; ++n; } m->title[n] = '\0'; }

            // ── #3 : HÔTE de dock flottant ? (priorité : un hôte est AUSSI un onglet) ──
            if (m->hostRoot >= 0) {
                // Hôte dégénéré (≤1 onglet restant) → redevient une fenêtre flottante.
                if (ctx.dockNodes[m->hostRoot].kind == 2 && ctx.dockNodes[m->hostRoot].winCount <= 1) {
                    m->hostRoot = -1; m->dockNode = -1; m->dockHost = NKGUI_ID_NONE; m->dockActiveTab = false; m->rect = m->hostRect;
                } else {
                    EnsureHostRendered(ctx, m, mi, id);        // dessine chrome+arbre (1×/frame, winDL → pose dockDL)
                    m = &ctx.windowMeta[mi];
                    return BeginDockedTabContent(ctx, m, id, m->dockDL, id);  // l'onglet de l'hôte lui-même
                }
            }

            // ── ANCRÉE ? rendu dans la zone de dock (central OU hôte flottant) ──
            if (m->dockNode >= 0) {
                // ROBUSTESSE : la fenêtre est-elle VRAIMENT dans sa feuille ? (sinon nœud
                // périmé après transitions central↔hôte → on redevient flottante au lieu
                // de DISPARAÎTRE).
                bool inLeaf = false;
                if (m->dockNode < static_cast<int32>(ctx.dockNodes.Size())) {
                    const NkGuiDockNode& L = ctx.dockNodes[m->dockNode];
                    for (int32 i = 0; i < L.winCount && !inLeaf; ++i) if (L.windows[i] == id) inLeaf = true;
                }
                if (!inLeaf) {
                    m->dockNode = -1; m->dockHost = NKGUI_ID_NONE; m->dockActiveTab = false;
                } else {
                    // VÉRITÉ = l'ARBRE (pas le champ dockHost, qui se désynchronise aux splits) :
                    // on remonte à la racine de la feuille, on retrouve l'hôte propriétaire, on le
                    // rend (→ pose dockDL) et on RESYNC dockHost. Élimine l'onglet fantôme + le
                    // doublon flottant (fenêtre listée dans un hôte mais rendue à part).
                    const int32   root   = DockRootOf(ctx, m->dockNode);
                    const NkGuiId hostId = DockHostOfRoot(ctx, root);   // NONE = central
                    if (hostId != NKGUI_ID_NONE) {
                        int32 hmi; NkGuiWindowMeta* H = WinFind(ctx, hostId, hmi);
                        if (H && H->hostRoot >= 0) EnsureHostRendered(ctx, H, hmi, hostId);
                        m = &ctx.windowMeta[mi];
                        m->dockHost = hostId;                  // resync depuis l'arbre
                    } else if (root != ctx.dockRoot) {
                        // racine ni centrale ni hôte → feuille DÉTACHÉE → flottante.
                        m = &ctx.windowMeta[mi]; m->dockNode = -1; m->dockHost = NKGUI_ID_NONE; m->dockActiveTab = false;
                    } else {
                        m = &ctx.windowMeta[mi]; m->dockHost = NKGUI_ID_NONE;   // central
                    }
                    // Filet : si malgré tout le fond n'a pas été peint cette frame (dockDL==-2),
                    // la feuille n'a pas été rendue → flottante (jamais de contenu sans cadre).
                    if (m->dockNode >= 0 && m->dockDL == -2) {
                        m->dockNode = -1; m->dockHost = NKGUI_ID_NONE; m->dockActiveTab = false;
                    } else if (m->dockNode >= 0) {
                        // dockDL = LA draw-list où le FOND a été peint → le contenu la suit.
                        return BeginDockedTabContent(ctx, m, id, m->dockDL, m->dockHost);
                    }
                }
            }

            if (ctx.winCount >= NkGuiContext::WinMax) return false;
            const int32 k = ctx.winCount++;
            ctx.winDL[k].Reset();
            ctx.winMeta[k]    = mi;
            ctx.curWindow     = k;
            ctx.curWindowId   = id;
            ctx.winSavedLayout = ctx.layout;
            // Passage devant : clic dans la fenêtre du dessus → z max.
            if (ctx.input.mouseClicked[0] && ctx.hoveredWindowId == id) m->zOrder = ++ctx.windowZTop;
            ctx.winZ[k] = m->zOrder;

            const bool hasTitle = !NkGuiHasWinFlag(flags, NkGuiWindowFlags::NoTitleBar);
            const bool canMove  = hasTitle && !NkGuiHasWinFlag(flags, NkGuiWindowFlags::NoMove);
            const bool canResize= !NkGuiHasWinFlag(flags, NkGuiWindowFlags::NoResize);
            const bool canCol   = hasTitle && !NkGuiHasWinFlag(flags, NkGuiWindowFlags::NoCollapse);
            const bool canClose = hasTitle && open && !NkGuiHasWinFlag(flags, NkGuiWindowFlags::NoClose);
            const float32 th = hasTitle ? ctx.ItemHeight() : 0.f;
            NkRect&       wr  = m->rect;
            const NkRect  wr0 = wr;                 // ancres figées (préhension)
            const float32 minW = 140.f, minH = th + 50.f;

            // ── PHASE 1 : INTERACTIONS — mettent à jour `wr` AVANT le dessin (sinon le
            //    contenu, dessiné à la nouvelle position, « court après » le chrome). ──
            bool closeHov = false;
            if (hasTitle) {
                if (canCol) {
                    const NkRect cr = { wr0.x + 2.f, wr0.y, th, th };
                    bool hv = false, hd = false;
                    if (ctx.ButtonBehavior(id ^ 0x1111u, cr, NkGuiButtonFlags::None, -1.f, -1.f, &hv, &hd)) m->collapsed = !m->collapsed;
                }
                if (canClose) {
                    const NkRect xr = { wr0.x + wr0.w - th, wr0.y, th, th };
                    bool hd = false;
                    if (ctx.ButtonBehavior(id ^ 0x2222u, xr, NkGuiButtonFlags::None, -1.f, -1.f, &closeHov, &hd) && open) *open = false;
                }
                if (canMove) {
                    const float32 lp = canCol ? th : 0.f, rp = canClose ? th : 0.f;
                    const NkRect drag = { wr0.x + lp, wr0.y, wr0.w - lp - rp, th };
                    bool hv = false, hd = false;
                    ctx.ButtonBehavior(id ^ 0x3333u, drag, NkGuiButtonFlags::None, -1.f, -1.f, &hv, &hd);
                    if (ctx.activeId == (id ^ 0x3333u)) {
                        if (ctx.input.mouseClicked[0]) ctx.winDragOff = { ctx.input.mousePos.x - wr0.x, ctx.input.mousePos.y - wr0.y };
                        wr.x = ctx.input.mousePos.x - ctx.winDragOff.x;
                        wr.y = ctx.input.mousePos.y - ctx.winDragOff.y;
                        ctx.movingWindowId = id;            // → le DockSpace propose l'ancrage
                    }
                }
            }
            if (canResize && !m->collapsed) WindowBorders(ctx, id, wr, wr0, minW, minH);
            if (wr.w < minW) wr.w = minW; if (wr.h < minH) wr.h = minH;

            // ── PHASE 2 : DESSIN à la position FINALE ──
            const float32 bodyH = m->collapsed ? th : wr.h;
            const NkRect  winR  = { wr.x, wr.y, wr.w, bodyH };
            ctx.DL().AddRectFilled(winR, ctx.theme.panel, ctx.theme.rounding);
            if (hasTitle) {
                const NkRect tb = { wr.x, wr.y, wr.w, th };
                ctx.DL().AddRectFilled(tb, ctx.theme.header, ctx.theme.rounding);
                float32 textX = wr.x + 10.f;
                if (canCol) {
                    const float32 a = th * 0.15f, cx = wr.x + 2.f + th * 0.5f, cy = wr.y + th * 0.5f;
                    if (m->collapsed) ctx.DL().AddTriangleFilled({ cx - a * 0.6f, cy - a }, { cx - a * 0.6f, cy + a }, { cx + a, cy }, ctx.theme.text);
                    else              ctx.DL().AddTriangleFilled({ cx - a, cy - a * 0.6f }, { cx + a, cy - a * 0.6f }, { cx, cy + a }, ctx.theme.text);
                    textX = wr.x + 2.f + th;
                }
                if (ctx.font && ctx.font->Valid())
                    ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(), { textX, CenteredBaseline(ctx, tb) }, title, ctx.theme.text);
                if (canClose) {
                    const NkColor xcol = closeHov ? NkColor{ 230, 90, 90, 255 } : ctx.theme.text;
                    const float32 cx = wr.x + wr.w - th * 0.5f, cy = wr.y + th * 0.5f, s = th * 0.17f;
                    ctx.DL().AddLine({ cx - s, cy - s }, { cx + s, cy + s }, xcol, 1.6f);
                    ctx.DL().AddLine({ cx - s, cy + s }, { cx + s, cy - s }, xcol, 1.6f);
                }
            }
            ctx.DL().AddRect(winR, ctx.theme.border, 1.f);
            if (canResize && !m->collapsed) {   // grip du coin bas-droit (indice visuel)
                ctx.DL().AddLine({ wr.x + wr.w - 3.f, wr.y + wr.h - 13.f }, { wr.x + wr.w - 13.f, wr.y + wr.h - 3.f }, ctx.theme.border, 1.5f);
                ctx.DL().AddLine({ wr.x + wr.w - 3.f, wr.y + wr.h - 8.f },  { wr.x + wr.w - 8.f,  wr.y + wr.h - 3.f }, ctx.theme.border, 1.5f);
            }

            if (m->collapsed) { ctx.curWindow = -1; ctx.curWindowId = NKGUI_ID_NONE; return false; }

            // ── #3 (opt-in) : une AUTRE fenêtre flottante draggée sur celle-ci → BOUSSOLE
            //    complète (centre = onglet, ◄►▲▼ = split) ; lâcher = fusionne en hôte. ──
            if (ctx.windowDockingEnabled && ctx.movingWindowId != NKGUI_ID_NONE && ctx.movingWindowId != id
                && NkGuiRectContains({ wr.x, wr.y + th, wr.w, wr.h - th }, ctx.input.mousePos)) {
                const NkRect  cr = { wr.x, wr.y + th, wr.w, wr.h - th };
                const NkVec2  c  = { cr.x + cr.w * 0.5f, cr.y + cr.h * 0.5f };
                const float32 g = 34.f, ps = 26.f;
                const NkVec2 pos[5] = { c, { c.x - g, c.y }, { c.x + g, c.y }, { c.x, c.y - g }, { c.x, c.y + g } };
                int32 zone = -1;
                for (int32 d = 0; d < 5; ++d) { const NkRect pr = { pos[d].x - ps*0.5f, pos[d].y - ps*0.5f, ps, ps }; if (NkGuiRectContains(pr, ctx.input.mousePos)) zone = d; }
                if (zone >= 0) {
                    NkRect prev = winR;
                    if      (zone == 1) prev.w *= 0.5f;
                    else if (zone == 2) { prev.x += winR.w * 0.5f; prev.w *= 0.5f; }
                    else if (zone == 3) prev.h *= 0.5f;
                    else if (zone == 4) { prev.y += winR.h * 0.5f; prev.h *= 0.5f; }
                    ctx.dlOverlay.AddRectFilled(prev, NkColor{ 96, 165, 250, 70 }, ctx.theme.rounding);
                    ctx.dlOverlay.AddRect(prev, ctx.theme.accent, 2.f);
                }
                for (int32 d = 0; d < 5; ++d) {
                    const NkRect pr = { pos[d].x - ps*0.5f, pos[d].y - ps*0.5f, ps, ps };
                    const bool on = (zone == d);
                    if (StyleDrawDockTarget(ctx, pr, on, d)) continue;
                    ctx.dlOverlay.AddRectFilled(pr, on ? ctx.theme.accent : NkColor{ 58, 64, 80, 235 }, 4.f);
                    ctx.dlOverlay.AddRect(pr, ctx.theme.border, 1.f);
                    const NkColor ic = on ? NkColor{ 20, 24, 30, 255 } : ctx.theme.text;
                    const float32 cx = pos[d].x, cy = pos[d].y, a = 5.f;
                    if      (d == 0) ctx.dlOverlay.AddRectFilled({ cx - a, cy - a, 2.f*a, 2.f*a }, ic, 1.f);
                    else if (d == 1) ctx.dlOverlay.AddTriangleFilled({ cx - a, cy }, { cx + a*0.5f, cy - a }, { cx + a*0.5f, cy + a }, ic);
                    else if (d == 2) ctx.dlOverlay.AddTriangleFilled({ cx + a, cy }, { cx - a*0.5f, cy - a }, { cx - a*0.5f, cy + a }, ic);
                    else if (d == 3) ctx.dlOverlay.AddTriangleFilled({ cx, cy - a }, { cx - a, cy + a*0.5f }, { cx + a, cy + a*0.5f }, ic);
                    else             ctx.dlOverlay.AddTriangleFilled({ cx, cy + a }, { cx - a, cy - a*0.5f }, { cx + a, cy - a*0.5f }, ic);
                }
                if (zone >= 0 && ctx.input.mouseReleased[0]) {
                    DockWindowIntoHost(ctx, id, ctx.movingWindowId, zone);
                    ctx.movingWindowId = NKGUI_ID_NONE; ctx.activeId = NKGUI_ID_NONE;
                }
            }

            // ── Contenu DÉFILABLE V + H : si ça déborde la fenêtre → scrollbars. `fillWidth`
            //    → l'auto-largeur remplit la fenêtre ; le scroll H n'apparaît que si un
            //    contenu (large explicite, image, SameLine…) DÉPASSE vraiment. ──
            const NkRect content = { wr.x, wr.y + th, wr.w, wr.h - th };
            BeginScrollFrame(ctx, id ^ 0x5555u, content, /*horizontal=*/true, /*fillWidth=*/true);
            return true;
        }

        void EndWindow(NkGuiContext& ctx) noexcept {
            if (ctx.curWindowDocked) { EndScrollFrame(ctx); ctx.curWindowDocked = false; ctx.curWindow = -1; ctx.curWindowId = NKGUI_ID_NONE; return; }
            if (ctx.curWindow < 0) return;
            EndScrollFrame(ctx);                 // ferme la zone défilable (restaure clip + layout)
            ctx.curWindow   = -1;
            ctx.curWindowId = NKGUI_ID_NONE;
        }

        // ── DOCKING : arbre de nœuds (split/feuille) ────────────────────────────
        namespace {
            int32 DockNew(NkGuiContext& ctx, uint8 kind) noexcept {
                NkGuiDockNode n; n.kind = kind;
                ctx.dockNodes.PushBack(n);
                return static_cast<int32>(ctx.dockNodes.Size()) - 1;
            }
            void DockComputeRects(NkGuiContext& ctx, int32 ni, const NkRect& r) noexcept {
                if (ni < 0 || ni >= static_cast<int32>(ctx.dockNodes.Size())) return;
                ctx.dockNodes[ni].rect = r;
                if (ctx.dockNodes[ni].kind != 1) return;
                const bool  vert = ctx.dockNodes[ni].vertical;
                const float32 ratio = ctx.dockNodes[ni].ratio;
                const int32 c0 = ctx.dockNodes[ni].child0, c1 = ctx.dockNodes[ni].child1;
                const float32 sp = 4.f;
                if (vert) {
                    const float32 w0 = (r.w - sp) * ratio;
                    DockComputeRects(ctx, c0, { r.x, r.y, w0, r.h });
                    DockComputeRects(ctx, c1, { r.x + w0 + sp, r.y, r.w - w0 - sp, r.h });
                } else {
                    const float32 h0 = (r.h - sp) * ratio;
                    DockComputeRects(ctx, c0, { r.x, r.y, r.w, h0 });
                    DockComputeRects(ctx, c1, { r.x, r.y + h0 + sp, r.w, r.h - h0 - sp });
                }
            }
            int32 DockLeafAt(NkGuiContext& ctx, int32 ni, NkVec2 p) noexcept {
                if (ni < 0) return -1;
                const NkGuiDockNode& n = ctx.dockNodes[ni];
                if (!NkGuiRectContains(n.rect, p)) return -1;
                if (n.kind == 1) { const int32 a = DockLeafAt(ctx, n.child0, p); return a >= 0 ? a : DockLeafAt(ctx, n.child1, p); }
                return ni;
            }
            void DockCollapseLeaf(NkGuiContext& ctx, int32 leaf) noexcept {
                const int32 p = ctx.dockNodes[leaf].parent;
                if (p < 0) return;                                   // racine vide : on la garde
                const int32 sib = (ctx.dockNodes[p].child0 == leaf) ? ctx.dockNodes[p].child1 : ctx.dockNodes[p].child0;
                const int32 savedParent = ctx.dockNodes[p].parent;
                ctx.dockNodes[p] = ctx.dockNodes[sib];               // p devient le frère
                ctx.dockNodes[p].parent = savedParent;
                if (ctx.dockNodes[p].kind == 1) {
                    ctx.dockNodes[ctx.dockNodes[p].child0].parent = p;
                    ctx.dockNodes[ctx.dockNodes[p].child1].parent = p;
                } else {
                    for (int32 i = 0; i < ctx.dockNodes[p].winCount; ++i) {
                        int32 wmi; NkGuiWindowMeta* wm = WinFind(ctx, ctx.dockNodes[p].windows[i], wmi);
                        if (wm) wm->dockNode = p;
                    }
                }
            }
            // Détache `winId` de SA feuille actuelle (sans effet de bord de drag) : retire des
            // onglets, recale activeTab, collapse la feuille si vide. À appeler AVANT de ré-ancrer
            // ailleurs, SINON la fenêtre reste listée dans 2 feuilles → onglet fantôme + contenu
            // invisible (fond repeint sur 2 couches selon l'ordre de rendu).
            void DockDetachFromLeaf(NkGuiContext& ctx, NkGuiId winId) noexcept {
                int32 wmi; NkGuiWindowMeta* wm = WinFind(ctx, winId, wmi);
                if (!wm || wm->dockNode < 0) return;
                const int32 leaf = wm->dockNode;
                if (leaf >= static_cast<int32>(ctx.dockNodes.Size())) { wm->dockNode = -1; return; }
                NkGuiDockNode& L = ctx.dockNodes[leaf];
                int32 idx = -1; for (int32 i = 0; i < L.winCount; ++i) if (L.windows[i] == winId) idx = i;
                if (idx < 0) { wm->dockNode = -1; return; }
                for (int32 j = idx; j < L.winCount - 1; ++j) L.windows[j] = L.windows[j + 1];
                --L.winCount;
                if (L.activeTab >= L.winCount) L.activeTab = L.winCount > 0 ? L.winCount - 1 : 0;
                wm->dockNode = -1; wm->dockHost = NKGUI_ID_NONE;
                if (L.winCount == 0) DockCollapseLeaf(ctx, leaf);
            }
            void UndockWindow(NkGuiContext& ctx, NkGuiId winId) noexcept {
                int32 wmi; NkGuiWindowMeta* wm = WinFind(ctx, winId, wmi);
                if (!wm || wm->dockNode < 0) return;
                const int32 leaf = wm->dockNode;
                NkGuiDockNode& L = ctx.dockNodes[leaf];
                int32 idx = -1; for (int32 i = 0; i < L.winCount; ++i) if (L.windows[i] == winId) idx = i;
                if (idx < 0) return;
                for (int32 j = idx; j < L.winCount - 1; ++j) L.windows[j] = L.windows[j + 1];
                --L.winCount;
                if (L.activeTab >= L.winCount) L.activeTab = L.winCount > 0 ? L.winCount - 1 : 0;
                wm->dockNode = -1; wm->dockHost = NKGUI_ID_NONE;    // redevient flottante
                wm->rect = { ctx.input.mousePos.x - 60.f, ctx.input.mousePos.y - 10.f, wm->floatRect.w, wm->floatRect.h };
                wm->zOrder = ++ctx.windowZTop;
                ctx.activeId = winId ^ 0x3333u;                     // continue en glissement flottant
                ctx.winDragOff = { 60.f, 10.f };
                ctx.movingWindowId = winId;
                // TRANSFERT D'HÔTE : si on désancre la fenêtre qui EST l'hôte de son arbre, le
                // rôle d'hôte (hostRoot + hostRect) passe à une autre fenêtre encore dans l'arbre.
                // Sinon l'ancien hôte reste « hôte fantôme » (rend le chrome mais dockNode=-1 →
                // son contenu part en couche de fond, invisible). Cause du bug vidéo (Calque C).
                if (wm->hostRoot >= 0) {
                    const int32  oldRoot = wm->hostRoot;
                    const NkRect hrect   = wm->hostRect;
                    wm->hostRoot = -1;                              // l'ancien hôte flotte désormais
                    int32 hi = -1;
                    for (uint32 k = 0; k < ctx.windowMeta.Size(); ++k) {
                        const NkGuiWindowMeta& w2 = ctx.windowMeta[k];
                        if (w2.id != winId && w2.dockNode >= 0 && DockRootOf(ctx, w2.dockNode) == oldRoot) { hi = static_cast<int32>(k); break; }
                    }
                    if (hi >= 0) {
                        const NkGuiId heir = ctx.windowMeta[hi].id;
                        ctx.windowMeta[hi].hostRoot = oldRoot;     // le nouvel hôte porte le chrome
                        ctx.windowMeta[hi].hostRect = hrect;
                        for (uint32 k = 0; k < ctx.windowMeta.Size(); ++k) {   // réaffecte dockHost de l'arbre
                            NkGuiWindowMeta& w2 = ctx.windowMeta[k];
                            if (w2.dockNode >= 0 && DockRootOf(ctx, w2.dockNode) == oldRoot)
                                w2.dockHost = (w2.id == heir) ? NKGUI_ID_NONE : heir;   // l'hôte lui-même n'a pas de dockHost
                        }
                    }
                }
                if (L.winCount == 0) DockCollapseLeaf(ctx, leaf);
            }
            void DockWindow(NkGuiContext& ctx, NkGuiId winId, int32 leaf, int32 zone) noexcept {
                int32 wmi; NkGuiWindowMeta* wm = WinFind(ctx, winId, wmi);
                if (!wm) return;
                // Déjà ancrée AILLEURS ? On la retire d'abord de son ancienne feuille (sinon
                // double appartenance → onglet fantôme + contenu invisible). DockNodes ne
                // réalloue pas ici → l'index `leaf` cible reste valide.
                if (wm->dockNode >= 0 && wm->dockNode != leaf) { DockDetachFromLeaf(ctx, winId); wm = WinFind(ctx, winId, wmi); if (!wm) return; }
                wm->floatRect = wm->rect;
                wm->dockHost  = NKGUI_ID_NONE;     // central par défaut ; DockWindowIntoHost le repositionne après
                if (ctx.dockNodes[leaf].winCount == 0) zone = 0;     // feuille vide → onglet
                if (zone == 0) {
                    NkGuiDockNode& L = ctx.dockNodes[leaf];
                    L.kind = 2;
                    if (L.winCount < 8) { L.windows[L.winCount++] = winId; L.activeTab = L.winCount - 1; }
                    wm->dockNode = leaf; wm->dockActiveTab = true;
                    return;
                }
                const NkGuiDockNode old = ctx.dockNodes[leaf];      // contenu existant
                const int32 keep  = DockNew(ctx, 2);
                const int32 fresh = DockNew(ctx, 2);                // (PushBack → ré-accès par index)
                ctx.dockNodes[keep] = old; ctx.dockNodes[keep].kind = 2; ctx.dockNodes[keep].parent = leaf;
                NkGuiDockNode f; f.kind = 2; f.parent = leaf; f.windows[0] = winId; f.winCount = 1; f.activeTab = 0;
                ctx.dockNodes[fresh] = f;
                for (int32 i = 0; i < ctx.dockNodes[keep].winCount; ++i) {
                    int32 ki; NkGuiWindowMeta* kw = WinFind(ctx, ctx.dockNodes[keep].windows[i], ki);
                    if (kw) kw->dockNode = keep;
                }
                wm = WinFind(ctx, winId, wmi); if (wm) { wm->dockNode = fresh; wm->dockActiveTab = true; }
                NkGuiDockNode& S = ctx.dockNodes[leaf];
                S.kind = 1; S.winCount = 0;
                S.vertical = (zone == 1 || zone == 2);
                const bool firstIsNew = (zone == 1 || zone == 3);   // gauche/haut → nouvelle feuille en 1er
                S.child0 = firstIsNew ? fresh : keep;
                S.child1 = firstIsNew ? keep : fresh;
                S.ratio  = 0.5f;
            }
            // #3 : ancre `winId` dans la fenêtre flottante `hostId` → `hostId` devient un
            // HÔTE de dock (onglets/splits flottants). Crée l'hôte (feuille [host]) au besoin.
            void DockWindowIntoHost(NkGuiContext& ctx, NkGuiId hostId, NkGuiId winId, int32 zone) noexcept {
                if (hostId == winId) return;
                int32 hmi; NkGuiWindowMeta* H = WinFind(ctx, hostId, hmi);
                int32 ami; NkGuiWindowMeta* A = WinFind(ctx, winId, ami);
                if (!H || !A) return;
                if (H->hostRoot < 0) {                         // crée l'hôte (feuille = [host])
                    const int32 leaf = DockNew(ctx, 2);
                    H = WinFind(ctx, hostId, hmi);             // (PushBack → ré-accès)
                    NkGuiDockNode& L = ctx.dockNodes[leaf];
                    L.kind = 2; L.parent = -1; L.windows[0] = hostId; L.winCount = 1; L.activeTab = 0;
                    H->hostRoot = leaf; H->dockNode = leaf; H->dockActiveTab = true; H->dockHost = NKGUI_ID_NONE;
                    H->hostRect = H->rect;
                }
                DockWindow(ctx, winId, H->dockNode, zone);     // onglet (0) ou split dans l'arbre de l'hôte
                int32 ai; NkGuiWindowMeta* A2 = WinFind(ctx, winId, ai);
                if (A2) A2->dockHost = hostId;                 // marque : ancrée dans cet hôte flottant
            }
            // Split GLOBAL : enveloppe TOUT l'arbre dans un nouveau split, la fenêtre
            // occupant une bande pleine hauteur/largeur d'un côté du DockSpace. side 1G,2D,3H,4B.
            void DockWindowRootEdge(NkGuiContext& ctx, NkGuiId winId, int32 side, int32* rootRef) noexcept {
                if (!rootRef || *rootRef < 0) return;
                int32 mi; NkGuiWindowMeta* wm = WinFind(ctx, winId, mi);
                if (!wm) return;
                wm->floatRect = wm->rect;
                const int32 oldRoot = *rootRef;
                const int32 fresh = DockNew(ctx, 2);
                const int32 split = DockNew(ctx, 1);               // (PushBack → ré-accès par index)
                NkGuiDockNode f; f.kind = 2; f.parent = split; f.windows[0] = winId; f.winCount = 1; f.activeTab = 0;
                ctx.dockNodes[fresh] = f;
                ctx.dockNodes[oldRoot].parent = split;
                NkGuiDockNode& S = ctx.dockNodes[split];
                S.kind = 1; S.parent = -1; S.winCount = 0;
                S.vertical = (side == 1 || side == 2);
                const bool firstIsNew = (side == 1 || side == 3);
                S.child0 = firstIsNew ? fresh : oldRoot;
                S.child1 = firstIsNew ? oldRoot : fresh;
                S.ratio  = firstIsNew ? 0.25f : 0.75f;             // la bande ≈ 1/4
                *rootRef = split;
                wm = WinFind(ctx, winId, mi); if (wm) { wm->dockNode = fresh; wm->dockActiveTab = true; }
            }
            void DockRenderNode(NkGuiContext& ctx, int32 ni) noexcept {
                if (ni < 0) return;
                const NkGuiDockNode node = ctx.dockNodes[ni];       // copie (sécurité)
                if (node.kind == 2) {
                    const NkRect  r  = node.rect;
                    const float32 th = ctx.ItemHeight();
                    if (node.winCount == 0) {
                        ctx.DL().AddRectFilled(r, ctx.theme.bgPrimary, 0.f);
                        ctx.DL().AddRect(r, ctx.theme.border, 1.f);
                        return;
                    }
                    int32 active = node.activeTab; if (active >= node.winCount) active = node.winCount - 1;
                    ctx.DL().AddRectFilled({ r.x, r.y, r.w, th }, ctx.theme.tabBar, 0.f);   // fond barre (≠ onglets)
                    // Réserve à DROITE : bouton « + » (si activé) + « ▾ » overflow (si débordement).
                    const float32 addW = ctx.dockTabAddButton ? th : 0.f;
                    float32 totalTabW = 4.f;
                    for (int32 i = 0; i < node.winCount; ++i) {
                        int32 wmi; NkGuiWindowMeta* wm = WinFind(ctx, node.windows[i], wmi);
                        totalTabW += ((ctx.font && ctx.font->Valid()) ? ctx.font->MeasureWidth(wm ? wm->title : "?") : 40.f) + 22.f;
                    }
                    const bool    overflow = (totalTabW > r.w - addW);
                    const float32 availW   = r.w - addW - (overflow ? th : 0.f);
                    ctx.DL().PushClipRect({ r.x, r.y, availW, th }, true);   // onglets clippés (réserve à droite)
                    int32 draggedIdx = -1, targetIdx = -1;
                    float32 tx = r.x + 2.f;
                    for (int32 i = 0; i < node.winCount; ++i) {
                        int32 wmi; NkGuiWindowMeta* wm = WinFind(ctx, node.windows[i], wmi);
                        const char* title = wm ? wm->title : "?";
                        const float32 tw = ((ctx.font && ctx.font->Valid()) ? ctx.font->MeasureWidth(title) : 40.f) + 22.f;
                        const NkRect tab = { tx, r.y, tw, th };
                        const NkGuiId tid = node.windows[i] ^ 0xDA8u;
                        bool hov = false, hd = false;
                        const bool clicked = ctx.ButtonBehavior(tid, tab, NkGuiButtonFlags::None, -1.f, -1.f, &hov, &hd);
                        const bool isActive = (i == active);
                        ctx.DL().AddRectFilled(tab, isActive ? ctx.theme.tabActive : (hov ? ctx.theme.tabHover : ctx.theme.tab), 0.f);
                        if (isActive) ctx.DL().AddRectFilled({ tab.x, r.y + th - 2.f, tab.w, 2.f }, ctx.theme.accent);
                        // Séparateur vertical entre onglets (distingue les en-têtes adjacents).
                        if (i < node.winCount - 1)
                            ctx.DL().AddRectFilled({ tab.x + tw - 1.f, r.y + 4.f, 1.f, th - 8.f }, ctx.theme.border);
                        if (ctx.font && ctx.font->Valid())
                            ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(), { tab.x + 6.f, CenteredBaseline(ctx, tab) }, title, ctx.theme.text, tw - 10.f);
                        if (clicked) ctx.dockNodes[ni].activeTab = i;
                        // Onglet tenu : tiré vers le BAS = désancre ; sinon = réordonne (horizontal).
                        if (ctx.activeId == tid) {
                            if (ctx.input.mousePos.y > r.y + th + 22.f) { ctx.DL().PopClipRect(); UndockWindow(ctx, node.windows[i]); return; }
                            draggedIdx = i;
                        }
                        if (ctx.input.mousePos.x >= tab.x && ctx.input.mousePos.x < tab.x + tw
                         && ctx.input.mousePos.y >= r.y && ctx.input.mousePos.y < r.y + th) targetIdx = i;
                        tx += tw;
                    }
                    ctx.DL().PopClipRect();
                    if (draggedIdx >= 0 && targetIdx >= 0 && targetIdx != draggedIdx) {   // réordonne en direct
                        NkGuiDockNode& L = ctx.dockNodes[ni];
                        const NkGuiId moved = L.windows[draggedIdx];
                        if (targetIdx > draggedIdx) for (int32 j = draggedIdx; j < targetIdx; ++j) L.windows[j] = L.windows[j + 1];
                        else                        for (int32 j = draggedIdx; j > targetIdx; --j) L.windows[j] = L.windows[j - 1];
                        L.windows[targetIdx] = moved;
                        L.activeTab = targetIdx;
                    }
                    // « ▾ » overflow : ouvre un popup listant TOUS les onglets (dont les cachés).
                    if (overflow) {
                        const NkRect  ovR = { r.x + availW, r.y, th, th };
                        const NkGuiId ovId = static_cast<NkGuiId>(0x0FF00000u) + static_cast<NkGuiId>(ni);
                        bool oh = false, od = false;
                        if (ctx.ButtonBehavior(ovId, ovR, NkGuiButtonFlags::None, -1.f, -1.f, &oh, &od)) {
                            ctx.dockOverflowNode  = ni;
                            ctx.dockOverflowPopup = ovId ^ 0x9999u;
                            ctx.dockOverflowPos   = { ovR.x - 150.f + th, ovR.y + th };
                            ctx.OpenPopupAt(ctx.dockOverflowPopup, ctx.dockOverflowPos);
                        }
                        ctx.DL().AddRectFilled(ovR, oh ? ctx.theme.tabHover : ctx.theme.tabBar, 0.f);
                        const float32 cx = ovR.x + th * 0.5f, cy = ovR.y + th * 0.5f, a = 4.f;
                        ctx.DL().AddTriangleFilled({ cx - a, cy - a * 0.5f }, { cx + a, cy - a * 0.5f }, { cx, cy + a * 0.7f }, ctx.theme.text);
                    }
                    // « + » : ajoute un onglet (opt-in via ctx.dockTabAddButton).
                    if (ctx.dockTabAddButton) {
                        const NkRect  addR = { r.x + r.w - addW, r.y, addW, th };
                        const NkGuiId addId = static_cast<NkGuiId>(0x0ADD0000u) + static_cast<NkGuiId>(ni);
                        bool ah = false, ad = false;
                        if (ctx.ButtonBehavior(addId, addR, NkGuiButtonFlags::None, -1.f, -1.f, &ah, &ad)) ctx.dockTabAddNode = ni;
                        ctx.DL().AddRectFilled(addR, ah ? ctx.theme.tabHover : ctx.theme.tabBar, 0.f);
                        const float32 cx = addR.x + addW * 0.5f, cy = addR.y + th * 0.5f, a = 5.f;
                        ctx.DL().AddRectFilled({ cx - a, cy - 1.f, 2.f * a, 2.f }, ctx.theme.text);
                        ctx.DL().AddRectFilled({ cx - 1.f, cy - a, 2.f, 2.f * a }, ctx.theme.text);
                    }
                    const NkRect content = { r.x, r.y + th, r.w, r.h - th };
                    ctx.DL().AddRectFilled(content, ctx.theme.panel, 0.f);
                    ctx.DL().AddRect(r, ctx.theme.border, 1.f);
                    for (int32 i = 0; i < node.winCount; ++i) {
                        int32 wmi; NkGuiWindowMeta* wm = WinFind(ctx, node.windows[i], wmi);
                        // dockDL = la draw-list où l'on vient de peindre le FOND : le contenu de
                        // l'onglet DOIT s'y dessiner (sinon fond et texte divergent de couche).
                        if (wm) { wm->dockNode = ni; wm->dockActiveTab = (i == active); wm->dockRect = content; wm->dockDL = ctx.curWindow; }
                    }
                } else if (node.kind == 1) {
                    DockRenderNode(ctx, node.child0);
                    DockRenderNode(ctx, node.child1);
                    const NkRect  c0  = ctx.dockNodes[node.child0].rect;
                    const NkGuiId sid = static_cast<NkGuiId>(0x5D000000u) + static_cast<NkGuiId>(ni);
                    const float32 sw = 4.f;              // largeur VISUELLE du splitter
                    NkRect sbVis, sbHit;                 // zone de PRÉHENSION élargie (facile à attraper)
                    if (node.vertical) {
                        sbVis = { c0.x + c0.w,        node.rect.y, sw,             node.rect.h };
                        sbHit = { c0.x + c0.w - 3.f,  node.rect.y, sw + 6.f,       node.rect.h };
                    } else {
                        sbVis = { node.rect.x, c0.y + c0.h,        node.rect.w, sw       };
                        sbHit = { node.rect.x, c0.y + c0.h - 3.f,  node.rect.w, sw + 6.f };
                    }
                    bool hov = false, hd = false;
                    ctx.ButtonBehavior(sid, sbHit, NkGuiButtonFlags::None, -1.f, -1.f, &hov, &hd);
                    const bool act = (ctx.activeId == sid);
                    if (hov || act) ctx.wantCursor = node.vertical ? NkGuiCursor::ResizeEW : NkGuiCursor::ResizeNS;
                    if (act) {                           // glisser → redimensionne les 2 zones (chacune dans sa part)
                        const float32 ratio = node.vertical ? (ctx.input.mousePos.x - node.rect.x) / node.rect.w
                                                            : (ctx.input.mousePos.y - node.rect.y) / node.rect.h;
                        ctx.dockNodes[ni].ratio = math::NkClamp(ratio, 0.08f, 0.92f);
                    }
                    ctx.DL().AddRectFilled(sbVis, (hov || act) ? ctx.theme.accent : ctx.theme.border, 0.f);
                }
            }
            // Boussole + DROP sur un arbre de dock (central OU hôte). `rootRef` = variable
            // racine (MAJ au split global) ; `hostId` = hôte flottant (NONE = central → pose dockHost).
            void DockDragTargets(NkGuiContext& ctx, int32* rootRef, const NkRect& rect, NkGuiId hostId) noexcept {
                if (ctx.movingWindowId == NKGUI_ID_NONE || ctx.movingWindowId == hostId) return;
                if (!rootRef || *rootRef < 0 || !NkGuiRectContains(rect, ctx.input.mousePos)) return;
                const NkVec2  mouse = ctx.input.mousePos;
                const float32 ps = 26.f, th = ctx.ItemHeight();
                const NkColor pillBg{ 58, 64, 80, 235 }, band{ 96, 165, 250, 80 };
                const int32   leaf = DockLeafAt(ctx, *rootRef, mouse);
                int32 zone = -1, edge = -1;
                const bool overTabBar = (leaf >= 0 && ctx.dockNodes[leaf].winCount > 0
                    && NkGuiRectContains({ ctx.dockNodes[leaf].rect.x, ctx.dockNodes[leaf].rect.y, ctx.dockNodes[leaf].rect.w, th }, mouse));
                if (overTabBar) {                              // (1) barre d'onglets → onglet
                    const NkRect lr = ctx.dockNodes[leaf].rect;
                    ctx.dlOverlay.AddRectFilled({ lr.x, lr.y, lr.w, th }, NkColor{ 96, 165, 250, 110 }, 0.f);
                    ctx.dlOverlay.AddRect({ lr.x, lr.y, lr.w, th }, ctx.theme.accent, 2.f);
                    zone = 0;
                } else {
                    const float32 em = 32.f;                   // (2) bord = split global (ligne)
                    const float32 dL = mouse.x - rect.x, dR = rect.x + rect.w - mouse.x;
                    const float32 dT = mouse.y - rect.y, dB = rect.y + rect.h - mouse.y;
                    float32 mn = em;
                    if (dL < mn) { mn = dL; edge = 1; }
                    if (dR < mn) { mn = dR; edge = 2; }
                    if (dT < mn) { mn = dT; edge = 3; }
                    if (dB < mn) { mn = dB; edge = 4; }
                    if (edge >= 1) {
                        NkRect prev = rect, bar; const float32 bw = 6.f;
                        if      (edge == 1) { prev.w *= 0.25f; bar = { rect.x, rect.y, bw, rect.h }; }
                        else if (edge == 2) { prev.x += rect.w * 0.75f; prev.w *= 0.25f; bar = { rect.x + rect.w - bw, rect.y, bw, rect.h }; }
                        else if (edge == 3) { prev.h *= 0.25f; bar = { rect.x, rect.y, rect.w, bw }; }
                        else                { prev.y += rect.h * 0.75f; prev.h *= 0.25f; bar = { rect.x, rect.y + rect.h - bw, rect.w, bw }; }
                        ctx.dlOverlay.AddRectFilled(prev, band, 0.f);
                        ctx.dlOverlay.AddRect(prev, ctx.theme.accent, 2.f);
                        if (!StyleDrawDockTarget(ctx, bar, true, 5)) ctx.dlOverlay.AddRectFilled(bar, ctx.theme.accent, 0.f);
                    } else if (leaf >= 0) {                    // (3) boussole de feuille (cluster centré)
                        const NkRect lr = ctx.dockNodes[leaf].rect;
                        const NkRect ct = { lr.x, lr.y + th, lr.w, lr.h - th };
                        const NkVec2 c  = { ct.x + ct.w * 0.5f, ct.y + ct.h * 0.5f };
                        const float32 g = 34.f;
                        const NkVec2 pos[5] = { c, { c.x - g, c.y }, { c.x + g, c.y }, { c.x, c.y - g }, { c.x, c.y + g } };
                        for (int32 d = 0; d < 5; ++d) {
                            const NkRect pr = { pos[d].x - ps * 0.5f, pos[d].y - ps * 0.5f, ps, ps };
                            if (NkGuiRectContains(pr, mouse)) zone = d;
                        }
                        if (zone >= 0) {
                            NkRect prev = lr;
                            if      (zone == 1) prev.w *= 0.5f;
                            else if (zone == 2) { prev.x += lr.w * 0.5f; prev.w *= 0.5f; }
                            else if (zone == 3) prev.h *= 0.5f;
                            else if (zone == 4) { prev.y += lr.h * 0.5f; prev.h *= 0.5f; }
                            ctx.dlOverlay.AddRectFilled(prev, band, 0.f);
                            ctx.dlOverlay.AddRect(prev, ctx.theme.accent, 2.f);
                        }
                        for (int32 d = 0; d < 5; ++d) {
                            const NkRect pr = { pos[d].x - ps * 0.5f, pos[d].y - ps * 0.5f, ps, ps };
                            const bool on = (zone == d);
                            if (StyleDrawDockTarget(ctx, pr, on, d)) continue;
                            ctx.dlOverlay.AddRectFilled(pr, on ? ctx.theme.accent : pillBg, 4.f);
                            ctx.dlOverlay.AddRect(pr, ctx.theme.border, 1.f);
                            const NkColor ic = on ? NkColor{ 20, 24, 30, 255 } : ctx.theme.text;
                            const float32 cx = pos[d].x, cy = pos[d].y, a = 5.f;
                            if      (d == 0) ctx.dlOverlay.AddRectFilled({ cx - a, cy - a, 2.f * a, 2.f * a }, ic, 1.f);
                            else if (d == 1) ctx.dlOverlay.AddTriangleFilled({ cx - a, cy }, { cx + a * 0.5f, cy - a }, { cx + a * 0.5f, cy + a }, ic);
                            else if (d == 2) ctx.dlOverlay.AddTriangleFilled({ cx + a, cy }, { cx - a * 0.5f, cy - a }, { cx - a * 0.5f, cy + a }, ic);
                            else if (d == 3) ctx.dlOverlay.AddTriangleFilled({ cx, cy - a }, { cx - a, cy + a * 0.5f }, { cx + a, cy + a * 0.5f }, ic);
                            else             ctx.dlOverlay.AddTriangleFilled({ cx, cy + a }, { cx - a, cy - a * 0.5f }, { cx + a, cy - a * 0.5f }, ic);
                        }
                    }
                }
                ctx.dockTargetNode = leaf; ctx.dockTargetZone = (edge >= 1 ? edge : zone);
                if (ctx.input.mouseReleased[0]) {
                    NkGuiId dropped = ctx.movingWindowId;
                    if      (edge >= 1)              DockWindowRootEdge(ctx, dropped, edge, rootRef);
                    else if (leaf >= 0 && zone >= 0) DockWindow(ctx, dropped, leaf, zone);
                    else                             dropped = NKGUI_ID_NONE;
                    if (dropped != NKGUI_ID_NONE) {
                        if (hostId != NKGUI_ID_NONE) { int32 di; NkGuiWindowMeta* dm = WinFind(ctx, dropped, di); if (dm) dm->dockHost = hostId; }
                        DockComputeRects(ctx, *rootRef, rect);
                        ctx.activeId = NKGUI_ID_NONE;
                    }
                    ctx.movingWindowId = NKGUI_ID_NONE; ctx.dockTargetNode = -1;
                }
            }
        } // namespace

        void DockSpace(NkGuiContext& ctx, const char* idStr, const NkRect& rect) noexcept {
            ctx.dockSpaceId   = ctx.GetId(idStr);
            ctx.dockSpaceRect = rect;
            if (ctx.dockRoot < 0) { NkGuiDockNode n; n.kind = 2; ctx.dockNodes.PushBack(n); ctx.dockRoot = static_cast<int32>(ctx.dockNodes.Size()) - 1; }
            ctx.dockTabAddNode = -1;                       // (re)posé par un clic « + » cette frame
            DockComputeRects(ctx, ctx.dockRoot, rect);
            ctx.DL().AddRectFilled(rect, ctx.theme.bgPrimary, 0.f);
            DockRenderNode(ctx, ctx.dockRoot);

            // Popup OVERFLOW : liste des onglets de la feuille (clic = active l'onglet).
            if (ctx.dockOverflowNode >= 0 && ctx.dockOverflowNode < static_cast<int32>(ctx.dockNodes.Size())
                && ctx.IsPopupOpen(ctx.dockOverflowPopup)) {
                const NkGuiDockNode& nd = ctx.dockNodes[ctx.dockOverflowNode];
                const float32 ih = ctx.ItemHeight();
                const NkRect  pr = { ctx.dockOverflowPos.x, ctx.dockOverflowPos.y, 160.f, nd.winCount * (ih + 2.f) + 12.f };
                if (BeginPopupLevel(ctx, ctx.dockOverflowPopup, 0, pr, { pr.x, pr.y - ih, pr.w, ih })) {
                    for (int32 i = 0; i < nd.winCount; ++i) {
                        int32 wmi; NkGuiWindowMeta* wm = WinFind(ctx, nd.windows[i], wmi);
                        if (Selectable(ctx, wm ? wm->title : "?", i == nd.activeTab)) {
                            ctx.dockNodes[ctx.dockOverflowNode].activeTab = i;
                            ctx.ClosePopup();
                        }
                    }
                    EndPopup(ctx);
                }
            }

            // Boussole + drop sur l'arbre central (centre/4 splits/onglet/bord) — partagé.
            DockDragTargets(ctx, &ctx.dockRoot, rect, NKGUI_ID_NONE);
        }

        void DockBuilderDock(NkGuiContext& ctx, const char* windowTitle, int32 zone) noexcept {
            const NkGuiId wid = ctx.GetId(windowTitle);
            if (ctx.dockRoot < 0) { NkGuiDockNode n; n.kind = 2; ctx.dockNodes.PushBack(n); ctx.dockRoot = static_cast<int32>(ctx.dockNodes.Size()) - 1; }
            int32 mi; NkGuiWindowMeta* m = WinFind(ctx, wid, mi);
            if (!m) { NkGuiWindowMeta nm; nm.id = wid; ctx.windowMeta.PushBack(nm); mi = static_cast<int32>(ctx.windowMeta.Size()) - 1; m = &ctx.windowMeta[mi]; }
            if (m->dockNode >= 0) return;                          // déjà ancrée
            int32 leaf = ctx.dockRoot;                             // descend jusqu'à une feuille
            while (ctx.dockNodes[leaf].kind == 1) leaf = ctx.dockNodes[leaf].child0;
            DockWindow(ctx, wid, leaf, zone);
        }

        void DockWindowIntoWindow(NkGuiContext& ctx, const char* hostTitle, const char* winTitle) noexcept {
            const NkGuiId h = ctx.GetId(hostTitle), w = ctx.GetId(winTitle);
            int32 i;
            if (!WinFind(ctx, h, i)) { NkGuiWindowMeta nm; nm.id = h; ctx.windowMeta.PushBack(nm); }
            if (!WinFind(ctx, w, i)) { NkGuiWindowMeta nm; nm.id = w; ctx.windowMeta.PushBack(nm); }
            int32 wi; NkGuiWindowMeta* wm = WinFind(ctx, w, wi);
            if (wm && wm->dockNode >= 0) return;                   // déjà ancrée
            DockWindowIntoHost(ctx, h, w, 0);
        }

        int32 DockTabAddRequest(NkGuiContext& ctx) noexcept { return ctx.dockTabAddNode; }

        void DockAddTab(NkGuiContext& ctx, const char* windowTitle, int32 node) noexcept {
            if (node < 0 || node >= static_cast<int32>(ctx.dockNodes.Size()) || ctx.dockNodes[node].kind != 2) return;
            const NkGuiId wid = ctx.GetId(windowTitle);
            int32 mi; NkGuiWindowMeta* m = WinFind(ctx, wid, mi);
            if (!m) { NkGuiWindowMeta nm; nm.id = wid; ctx.windowMeta.PushBack(nm); mi = static_cast<int32>(ctx.windowMeta.Size()) - 1; m = &ctx.windowMeta[mi]; }
            if (m->dockNode >= 0) return;                          // déjà ancrée
            DockWindow(ctx, wid, node, 0);                         // onglet
        }

        void DockSpaceOverViewport(NkGuiContext& ctx, float32 topMargin) noexcept {
            DockSpace(ctx, "##Viewport", { 0.f, topMargin,
                      static_cast<float32>(ctx.viewW), static_cast<float32>(ctx.viewH) - topMargin });
        }

        // ── Zones défilables (frame de scroll empilée) ───────────────────────
        static NkGuiScrollState ScrollGet(NkGuiContext& ctx, NkGuiId id) noexcept {
            for (uint32 i = 0; i < ctx.scrollKeys.Size(); ++i)
                if (ctx.scrollKeys[i] == id) return ctx.scrollVals[i];
            return {};
        }
        static void ScrollSet(NkGuiContext& ctx, NkGuiId id, const NkGuiScrollState& s) noexcept {
            for (uint32 i = 0; i < ctx.scrollKeys.Size(); ++i)
                if (ctx.scrollKeys[i] == id) { ctx.scrollVals[i] = s; return; }
            ctx.scrollKeys.PushBack(id); ctx.scrollVals.PushBack(s);
        }

        static constexpr float32 kScrollBarW = 12.f;

        // Ouvre une zone défilable sur `area` (le fond/bordure/en-tête sont déjà
        // dessinés par l'appelant). Réserve les gouttières selon les barres de la
        // FRAME PRÉCÉDENTE (anti-oscillation). Empile une frame (imbrication OK).
        static bool BeginScrollFrame(NkGuiContext& ctx, NkGuiId id, const NkRect& area, bool horizontal, bool fillWidth) noexcept {
            if (ctx.childDepth >= NkGuiContext::ChildMax) return false;
            NkGuiScrollState st = ScrollGet(ctx, id);

            if (NkGuiRectContains(area, ctx.input.mousePos)) {
                bool modHeld = false;
                switch (ctx.hscrollMod) {
                    case NkGuiKeyMod::None:  modHeld = true;                break;
                    case NkGuiKeyMod::Shift: modHeld = ctx.input.shiftDown; break;
                    case NkGuiKeyMod::Ctrl:  modHeld = ctx.input.ctrlDown;  break;
                    case NkGuiKeyMod::Alt:   modHeld = ctx.input.altDown;   break;
                }
                if (horizontal && (modHeld || ctx.input.wheelH != 0.f))
                    st.x -= (ctx.input.wheelH != 0.f ? ctx.input.wheelH : ctx.input.wheel) * 36.f;
                else
                    st.y -= ctx.input.wheel * 36.f;
            }
            // Bornage par les MAX de la frame précédente → empêche l'overscroll (sinon
            // 1 frame de contenu décalé puis re-clamp à End = clignotement en butée).
            if (st.x < 0.f) st.x = 0.f; if (st.x > st.maxX) st.x = st.maxX;
            if (st.y < 0.f) st.y = 0.f; if (st.y > st.maxY) st.y = st.maxY;

            const float32 gV = st.barV ? kScrollBarW : 0.f;                  // gouttières (frame préc.)
            const float32 gH = (horizontal && st.barH) ? kScrollBarW : 0.f;
            const NkRect inner = { area.x, area.y, area.w - gV, area.h - gH };

            NkGuiChildFrame& f = ctx.childStack[ctx.childDepth++];
            f.id = id; f.area = area; f.horizontal = horizontal;
            f.scrollX = st.x; f.scrollY = st.y;
            f.contentTop  = area.y - st.y;
            f.contentLeft = inner.x - st.x;
            f.savedLayout = ctx.layout;

            ctx.DL().PushClipRect(inner, true);
            // `fillWidth` : auto-largeur = largeur visible (le scroll H n'apparaît que sur
            // débordement réel) ; sinon largeur « infinie » (contenu naturellement large).
            const float32 regionW = (horizontal && !fillWidth) ? 1.0e6f : inner.w;
            ctx.BeginLayout({ f.contentLeft, f.contentTop, regionW, 1.0e6f });
            return true;
        }

        static void EndScrollFrame(NkGuiContext& ctx) noexcept {
            if (ctx.childDepth <= 0) return;
            const NkGuiChildFrame f = ctx.childStack[--ctx.childDepth];
            const bool horiz = f.horizontal;
            const NkGuiScrollState prev = ScrollGet(ctx, f.id);
            const float32 gV = prev.barV ? kScrollBarW : 0.f;
            const float32 gH = (horiz && prev.barH) ? kScrollBarW : 0.f;
            const float32 viewH = f.area.h - gH;
            const float32 viewW = f.area.w - gV;

            const float32 contentH = ctx.layout.cursor.y - f.contentTop;
            const float32 contentW = ctx.layout.maxX     - f.contentLeft;
            float32 maxY = contentH - viewH; if (maxY < 0.f) maxY = 0.f;
            float32 maxX = horiz ? (contentW - viewW) : 0.f; if (maxX < 0.f) maxX = 0.f;
            float32 sx = f.scrollX; if (sx > maxX) sx = maxX;
            float32 sy = f.scrollY; if (sy > maxY) sy = maxY;

            ctx.DL().PopClipRect();

            const bool barV = maxY > 0.f;
            const bool barH = horiz && maxX > 0.f;
            if (barV) {
                const NkRect track = { f.area.x + f.area.w - kScrollBarW, f.area.y, kScrollBarW, f.area.h - gH };
                ctx.DL().AddRectFilled(track, NkColor{ 20, 22, 28, 255 }, 0.f);
                float32 thumbH = (f.area.h - gH) * ((f.area.h - gH) / contentH);
                if (thumbH < 24.f) thumbH = 24.f; if (thumbH > track.h) thumbH = track.h;
                const float32 thumbY = track.y + (sy / maxY) * (track.h - thumbH);
                const NkRect  thumb  = { track.x + 2.f, thumbY, kScrollBarW - 4.f, thumbH };
                bool hov = false, held = false;
                ctx.ButtonBehavior(f.id ^ 0x5C12AB37u, thumb, NkGuiButtonFlags::None, -1.f, -1.f, &hov, &held);
                if (ctx.activeId == (f.id ^ 0x5C12AB37u) && (track.h - thumbH) > 0.f) {
                    sy = ((ctx.input.mousePos.y - track.y - thumbH * 0.5f) / (track.h - thumbH)) * maxY;
                    if (sy < 0.f) sy = 0.f; if (sy > maxY) sy = maxY;
                }
                ctx.DL().AddRectFilled(thumb, (hov || ctx.activeId == (f.id ^ 0x5C12AB37u)) ? ctx.theme.buttonHover : ctx.theme.button, 3.f);
            }
            if (barH) {
                const NkRect track = { f.area.x, f.area.y + f.area.h - kScrollBarW, f.area.w - gV, kScrollBarW };
                ctx.DL().AddRectFilled(track, NkColor{ 20, 22, 28, 255 }, 0.f);
                float32 thumbW = (f.area.w - gV) * ((f.area.w - gV) / contentW);
                if (thumbW < 24.f) thumbW = 24.f; if (thumbW > track.w) thumbW = track.w;
                const float32 thumbX = track.x + (sx / maxX) * (track.w - thumbW);
                const NkRect  thumb  = { thumbX, track.y + 2.f, thumbW, kScrollBarW - 4.f };
                bool hov = false, held = false;
                ctx.ButtonBehavior(f.id ^ 0x37AB125Cu, thumb, NkGuiButtonFlags::None, -1.f, -1.f, &hov, &held);
                if (ctx.activeId == (f.id ^ 0x37AB125Cu) && (track.w - thumbW) > 0.f) {
                    sx = ((ctx.input.mousePos.x - track.x - thumbW * 0.5f) / (track.w - thumbW)) * maxX;
                    if (sx < 0.f) sx = 0.f; if (sx > maxX) sx = maxX;
                }
                ctx.DL().AddRectFilled(thumb, (hov || ctx.activeId == (f.id ^ 0x37AB125Cu)) ? ctx.theme.buttonHover : ctx.theme.button, 3.f);
            }

            NkGuiScrollState ns;
            ns.x = sx; ns.y = sy; ns.maxX = maxX; ns.maxY = maxY; ns.barV = barV; ns.barH = barH;
            ScrollSet(ctx, f.id, ns);
            ctx.layout = f.savedLayout;
        }

        bool BeginChild(NkGuiContext& ctx, const char* idStr, const NkRect& rect,
                        bool border, bool horizontal) noexcept {
            const NkGuiId id = ctx.GetId(idStr);
            if (border) {
                ctx.DL().AddRectFilled(rect, ctx.theme.track, ctx.theme.rounding);
                ctx.DL().AddRect(rect, ctx.theme.border, 1.f);
            }
            return BeginScrollFrame(ctx, id, rect, horizontal);
        }
        void EndChild(NkGuiContext& ctx) noexcept { EndScrollFrame(ctx); }

        bool BeginListBox(NkGuiContext& ctx, const char* idStr, const NkRect& rect) noexcept {
            return BeginChild(ctx, idStr, rect, true, false);
        }
        void EndListBox(NkGuiContext& ctx) noexcept { EndChild(ctx); }

        // ── Table ─────────────────────────────────────────────────────────────
        // Largeurs persistantes (resize) par table.
        static NkGuiTableW* TableWidthsFind(NkGuiContext& ctx, NkGuiId id) noexcept {
            for (uint32 i = 0; i < ctx.tblKeys.Size(); ++i)
                if (ctx.tblKeys[i] == id) return &ctx.tblWidths[i];
            return nullptr;
        }
        // Résout les largeurs (fixe / étirable / resize utilisateur) → bornes tblColX.
        // Idempotent : ne recalcule qu'une fois par frame (tblXComputed).
        static void TableEnsureLayout(NkGuiContext& ctx) noexcept {
            if (ctx.tblXComputed) return;
            const int32 n = ctx.tblCols;
            float32 fixedTotal = 0.f; int32 stretchN = 0;
            for (int32 c = 0; c < n; ++c) {
                const float32 w = ctx.tblUserW[c] > 0.f ? ctx.tblUserW[c]
                                : (ctx.tblSetupW[c] > 0.f ? ctx.tblSetupW[c] : -1.f);
                if (w > 0.f) fixedTotal += w; else ++stretchN;
            }
            float32 stretchW = stretchN > 0 ? (ctx.tblWidth - fixedTotal) / static_cast<float32>(stretchN) : 0.f;
            if (stretchW < 28.f) stretchW = 28.f;   // largeur minimale d'une colonne étirable
            float32 x = ctx.tblX;
            for (int32 c = 0; c < n; ++c) {
                ctx.tblColX[c] = x;
                const float32 w = ctx.tblUserW[c] > 0.f ? ctx.tblUserW[c]
                                : (ctx.tblSetupW[c] > 0.f ? ctx.tblSetupW[c] : stretchW);
                x += w;
            }
            ctx.tblColX[n] = x;
            ctx.tblXComputed = true;
        }
        // Referme le clip de la cellule courante (si actif).
        static void TableEndCell(NkGuiContext& ctx) noexcept {
            if (ctx.tblCellClip) { ctx.DL().PopClipRect(); ctx.tblCellClip = false; }
        }

        bool BeginTable(NkGuiContext& ctx, const char* idStr, int32 columns,
                        NkGuiTableFlags flags) noexcept {
            if (columns < 1) return false;
            if (columns > NkGuiTableMaxCols) columns = NkGuiTableMaxCols;
            const NkGuiId id = ctx.GetId(idStr);
            ctx.tblId        = id;
            ctx.tblFlags     = static_cast<uint32>(flags);
            ctx.tblCols      = columns;
            ctx.tblSetup     = 0;
            ctx.tblCurCol    = -1;
            ctx.tblRowIdx    = -1;
            ctx.tblCellClip  = false;
            ctx.tblXComputed = false;
            ctx.tblRowH      = ctx.ItemHeight();
            ctx.tblAvailW    = ctx.ContentWidth();
            ctx.tblX         = ctx.layout.cursor.x;
            ctx.tblY         = ctx.layout.cursor.y;
            ctx.tblSavedLayout = ctx.layout;
            const NkGuiTableW* tw = TableWidthsFind(ctx, id);
            // Largeur globale : override persisté (ResizableOuter) borné à [120, dispo], sinon auto-remplir.
            const float32 ovr = tw ? tw->total : 0.f;
            ctx.tblWidth   = (ovr >= 120.f && ovr <= ctx.tblAvailW) ? ovr : ctx.tblAvailW;
            ctx.tblSortCol = tw ? tw->sortCol : -1;   // tri persisté (Sortable)
            ctx.tblSortAsc = tw ? tw->sortAsc : true;
            for (int32 c = 0; c < columns; ++c) {
                ctx.tblSetupW[c] = 0.f;
                ctx.tblUserW [c] = tw ? tw->w[c] : 0.f;
                ctx.tblColLbl[c] = nullptr;
            }
            return true;
        }

        void TableSetupColumn(NkGuiContext& ctx, const char* label, float32 width) noexcept {
            if (ctx.tblId == NKGUI_ID_NONE || ctx.tblSetup >= ctx.tblCols) return;
            const int32 i   = ctx.tblSetup++;
            ctx.tblColLbl[i] = label;
            ctx.tblSetupW[i] = width;   // >0 = px fixe ; <=0 = étirable
        }

        void TableHeadersRow(NkGuiContext& ctx) noexcept {
            if (ctx.tblId == NKGUI_ID_NONE) return;
            TableEnsureLayout(ctx);
            const int32   n       = ctx.tblCols;
            const float32 h       = ctx.tblRowH;
            const bool    borders = (ctx.tblFlags & static_cast<uint32>(NkGuiTableFlags::Borders)) != 0;
            const bool    resize  = (ctx.tblFlags & static_cast<uint32>(NkGuiTableFlags::Resizable)) != 0;
            const bool    sortable= (ctx.tblFlags & static_cast<uint32>(NkGuiTableFlags::Sortable)) != 0;
            const NkRect  hdr     = { ctx.tblX, ctx.tblY, ctx.tblColX[n] - ctx.tblX, h };
            ctx.DL().AddRectFilled(hdr, ctx.theme.header, 0.f);

            for (int32 c = 0; c < n; ++c) {
                const NkRect cell = { ctx.tblColX[c], ctx.tblY, ctx.tblColX[c + 1] - ctx.tblColX[c], h };
                // En-tête CLIQUABLE (tri) : clic → cycle de la colonne (asc↔desc).
                if (sortable) {
                    bool hh = false, hd = false;
                    const NkGuiId hid = ctx.tblId ^ (0x540F0000u + static_cast<NkGuiId>(c));
                    const bool clicked = ctx.ButtonBehavior(hid, cell, NkGuiButtonFlags::None, -1.f, -1.f, &hh, &hd);
                    if (hh) ctx.DL().AddRectFilled(cell, ctx.theme.buttonHover, 0.f);  // survol
                    if (clicked) {
                        if (ctx.tblSortCol == c) ctx.tblSortAsc = !ctx.tblSortAsc;
                        else { ctx.tblSortCol = c; ctx.tblSortAsc = true; }
                    }
                }
                if (ctx.tblColLbl[c] && ctx.font && ctx.font->Valid()) {
                    ctx.DL().PushClipRect({ cell.x, cell.y, cell.w - 4.f, cell.h }, true);
                    ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
                                   { cell.x + 6.f, CenteredBaseline(ctx, cell) },
                                   ctx.tblColLbl[c], ctx.theme.text, cell.w - 22.f);
                    ctx.DL().PopClipRect();
                }
                // Flèche de tri sur la colonne active (triangle, pas de glyphe police).
                if (sortable && c == ctx.tblSortCol) {
                    const float32 cx = cell.x + cell.w - 11.f;
                    const float32 cy = cell.y + cell.h * 0.5f;
                    if (ctx.tblSortAsc)
                        ctx.DL().AddTriangleFilled({ cx, cy - 4.f }, { cx - 4.f, cy + 3.f }, { cx + 4.f, cy + 3.f }, ctx.theme.accent);
                    else
                        ctx.DL().AddTriangleFilled({ cx - 4.f, cy - 3.f }, { cx + 4.f, cy - 3.f }, { cx, cy + 4.f }, ctx.theme.accent);
                }
                if (borders && c > 0)
                    ctx.DL().AddRectFilled({ ctx.tblColX[c], ctx.tblY, 1.f, h }, ctx.theme.border);
            }

            // Redimensionnement : zone de préhension sur chaque séparateur interne.
            if (resize) {
                for (int32 c = 1; c < n; ++c) {
                    const float32 bx   = ctx.tblColX[c];
                    const NkRect  grab = { bx - 3.f, ctx.tblY, 6.f, h };
                    const NkGuiId gid  = ctx.tblId ^ (0x7AB10000u + static_cast<NkGuiId>(c));
                    bool hov = false, held = false;
                    ctx.ButtonBehavior(gid, grab, NkGuiButtonFlags::None, -1.f, -1.f, &hov, &held);
                    if (hov || ctx.activeId == gid) ctx.wantCursor = NkGuiCursor::ResizeEW;
                    if (ctx.activeId == gid) {
                        // Redistribue ENTRE les deux colonnes voisines : la frontière
                        // bouge, mais les bords c-1 (gauche) et c+1 (droite) restent
                        // fixes → la largeur GLOBALE du tableau ne change pas. La somme
                        // (col c-1 + col c) est constante → colonnes étirables intactes.
                        float32 nb       = ctx.input.mousePos.x;
                        const float32 lo = ctx.tblColX[c - 1] + 28.f;   // largeur mini gauche
                        const float32 hi = ctx.tblColX[c + 1] - 28.f;   // largeur mini droite
                        if (hi >= lo) {
                            if (nb < lo) nb = lo;
                            if (nb > hi) nb = hi;
                            ctx.tblUserW[c - 1] = nb - ctx.tblColX[c - 1];
                            ctx.tblUserW[c]     = ctx.tblColX[c + 1] - nb;  // compense
                        }
                        ctx.DL().AddRectFilled({ bx - 1.f, ctx.tblY, 2.f, h }, ctx.theme.accent);
                    }
                }
            }
            if (borders)   // séparateur sous l'en-tête
                ctx.DL().AddRectFilled({ hdr.x, ctx.tblY + h - 1.f, hdr.w, 1.f }, ctx.theme.border);
            ctx.tblY += h;
        }

        void TableNextRow(NkGuiContext& ctx, float32 minHeight) noexcept {
            if (ctx.tblId == NKGUI_ID_NONE) return;
            TableEnsureLayout(ctx);
            TableEndCell(ctx);
            const bool borders = (ctx.tblFlags & static_cast<uint32>(NkGuiTableFlags::Borders)) != 0;
            if (ctx.tblRowIdx >= 0) {
                ctx.tblY += ctx.tblRowH;                                          // après la ligne précédente
                if (borders) ctx.DL().AddRectFilled({ ctx.tblX, ctx.tblY, ctx.tblColX[ctx.tblCols] - ctx.tblX, 1.f }, ctx.theme.border);
            }
            ++ctx.tblRowIdx;
            ctx.tblCurCol = -1;
            const float32 base = ctx.ItemHeight();
            ctx.tblRowH = (minHeight > base) ? minHeight : base;
            // Zébrures : fond alterné (RowBg).
            if ((ctx.tblFlags & static_cast<uint32>(NkGuiTableFlags::RowBg)) != 0) {
                const NkColor& a = ctx.theme.panel;
                const NkColor  b = { static_cast<uint8>(a.r + 10 > 255 ? 255 : a.r + 10),
                                     static_cast<uint8>(a.g + 10 > 255 ? 255 : a.g + 10),
                                     static_cast<uint8>(a.b + 10 > 255 ? 255 : a.b + 10), 255 };
                const NkRect row = { ctx.tblX, ctx.tblY, ctx.tblColX[ctx.tblCols] - ctx.tblX, ctx.tblRowH };
                ctx.DL().AddRectFilled(row, (ctx.tblRowIdx & 1) ? b : a, 0.f);
            }
        }

        bool TableSetColumnIndex(NkGuiContext& ctx, int32 n) noexcept {
            if (ctx.tblId == NKGUI_ID_NONE || n < 0 || n >= ctx.tblCols) return false;
            TableEndCell(ctx);
            ctx.tblCurCol = n;
            const bool borders = (ctx.tblFlags & static_cast<uint32>(NkGuiTableFlags::Borders)) != 0;
            if (borders && n > 0)
                ctx.DL().AddRectFilled({ ctx.tblColX[n], ctx.tblY, 1.f, ctx.tblRowH }, ctx.theme.border);
            const NkRect cell = { ctx.tblColX[n], ctx.tblY, ctx.tblColX[n + 1] - ctx.tblColX[n], ctx.tblRowH };
            ctx.DL().PushClipRect({ cell.x, cell.y, cell.w - 1.f, cell.h }, true);
            ctx.tblCellClip = true;
            // Layout sur la cellule (petit padding) : les widgets remplissent la cellule.
            ctx.layout.padding = 4.f;
            ctx.BeginLayout(cell);
            return true;
        }

        bool TableNextColumn(NkGuiContext& ctx) noexcept {
            if (ctx.tblId == NKGUI_ID_NONE) return false;
            int32 c = ctx.tblCurCol + 1;
            if (c >= ctx.tblCols) { TableNextRow(ctx, 0.f); c = 0; }
            return TableSetColumnIndex(ctx, c);
        }

        void EndTable(NkGuiContext& ctx) noexcept {
            if (ctx.tblId == NKGUI_ID_NONE) return;
            TableEndCell(ctx);
            const int32   n       = ctx.tblCols;
            const bool    borders = (ctx.tblFlags & static_cast<uint32>(NkGuiTableFlags::Borders)) != 0;
            const float32 top     = ctx.tblSavedLayout.cursor.y;
            const float32 bottom  = (ctx.tblRowIdx >= 0) ? ctx.tblY + ctx.tblRowH : ctx.tblY;
            const NkRect  outer   = { ctx.tblX, top, ctx.tblColX[n] - ctx.tblX, bottom - top };
            if (borders) ctx.DL().AddRect(outer, ctx.theme.border, 1.f);

            // Persiste les largeurs (resize) pour la frame suivante.
            NkGuiTableW* tw = TableWidthsFind(ctx, ctx.tblId);
            if (!tw) { ctx.tblKeys.PushBack(ctx.tblId); ctx.tblWidths.PushBack(NkGuiTableW{}); tw = &ctx.tblWidths[ctx.tblWidths.Size() - 1]; }
            for (int32 c = 0; c < n; ++c) tw->w[c] = ctx.tblUserW[c];

            // ── Resize du BORD EXTERNE (opt-in) → change la largeur GLOBALE ────
            // Poignée sur le bord droit du tableau ; on override tblWidth (les colonnes
            // étirables absorbent le delta). Borné à [120, largeur dispo].
            float32 newTotal = tw->total;
            if ((ctx.tblFlags & static_cast<uint32>(NkGuiTableFlags::ResizableOuter)) != 0) {
                const NkRect  grab = { ctx.tblColX[n] - 3.f, top, 6.f, bottom - top };
                const NkGuiId gid  = ctx.tblId ^ 0x7AB1FFFFu;
                bool hov = false, held = false;
                ctx.ButtonBehavior(gid, grab, NkGuiButtonFlags::None, -1.f, -1.f, &hov, &held);
                if (hov || ctx.activeId == gid) ctx.wantCursor = NkGuiCursor::ResizeEW;
                if (ctx.activeId == gid) {
                    float32 nt = ctx.input.mousePos.x - ctx.tblX;
                    if (nt < 120.f)           nt = 120.f;
                    if (nt > ctx.tblAvailW)   nt = ctx.tblAvailW;
                    newTotal = nt;                                      // appliqué la frame suivante
                    ctx.DL().AddRectFilled({ ctx.tblColX[n] - 1.f, top, 2.f, bottom - top }, ctx.theme.accent);
                }
            }
            tw->total   = newTotal;
            tw->sortCol = ctx.tblSortCol;   // persiste le tri
            tw->sortAsc = ctx.tblSortAsc;

            // Restaure le layout externe + avance le curseur sous la table.
            ctx.layout = ctx.tblSavedLayout;
            ctx.layout.cursor.x = ctx.layout.lineStartX;
            ctx.layout.cursor.y = bottom + ctx.layout.itemSpacingY;
            if (outer.x + outer.w > ctx.layout.maxX) ctx.layout.maxX = outer.x + outer.w;
            ctx.tblId = NKGUI_ID_NONE;
        }

        bool TableGetSortColumn(NkGuiContext& ctx, int32* outCol, bool* outAscending) noexcept {
            if (outCol)       *outCol       = ctx.tblSortCol;
            if (outAscending) *outAscending = ctx.tblSortAsc;
            return ctx.tblSortCol >= 0;
        }

        // ── Graphiques ────────────────────────────────────────────────────────
        void ProgressBar(NkGuiContext& ctx, float32 fraction, const char* overlay) noexcept {
            fraction = math::NkClamp(fraction, 0.f, 1.f);
            const float32 h = ctx.ItemHeight();
            const NkRect  r = ctx.NextItemRect(0.f, h);
            ctx.DL().AddRectFilled(r, ctx.theme.track, ctx.theme.rounding);
            if (fraction > 0.f)
                ctx.DL().AddRectFilled({ r.x, r.y, r.w * fraction, r.h }, ctx.theme.accent, ctx.theme.rounding);
            ctx.DL().AddRect(r, ctx.theme.border, 1.f);
            NkString pct;
            const char* txt = overlay;
            if (!txt) { pct = NkString::Format("%d%%", static_cast<int32>(fraction * 100.f + 0.5f)); txt = pct.CStr(); }
            DrawCenteredLabel(ctx, r, txt, ctx.theme.text);
        }

        // Cœur partagé PlotLines / PlotHistogram.
        static void PlotEx(NkGuiContext& ctx, const char* label, const float32* values, int32 count,
                           float32 minV, float32 maxV, float32 height, bool histogram) noexcept {
            const float32 h = height > 0.f ? height : 60.f;
            const NkRect  r = ctx.NextItemRect(0.f, h);
            ctx.DL().AddRectFilled(r, ctx.theme.track, ctx.theme.rounding);
            ctx.DL().AddRect(r, ctx.theme.border, 1.f);
            if (!values || count < 1) return;

            // Échelle : AUTO si maxV<=minV.
            float32 lo = minV, hi = maxV;
            if (hi <= lo) {
                lo = values[0]; hi = values[0];
                for (int32 i = 1; i < count; ++i) { lo = math::NkMin(lo, values[i]); hi = math::NkMax(hi, values[i]); }
            }
            if (hi <= lo) hi = lo + 1.f;

            const float32 pad = 3.f;
            const NkRect  g   = { r.x + pad, r.y + pad, r.w - 2.f * pad, r.h - 2.f * pad };
            const bool hov = NkGuiRectContains(r, ctx.input.mousePos)
                          && NkGuiRectContains(ctx.DL().CurrentClip(), ctx.input.mousePos);
            int32 hoverIdx = -1;

            const float32 invRange = g.h / (hi - lo);
            // y d'une valeur (0 = haut).
            const auto yOf = [&](float32 v) -> float32 { return g.y + g.h - (v - lo) * invRange; };

            if (histogram) {
                const float32 base = (lo < 0.f && hi > 0.f) ? yOf(0.f) : g.y + g.h;
                const float32 bw   = g.w / static_cast<float32>(count);
                for (int32 i = 0; i < count; ++i) {
                    const float32 vx = g.x + bw * static_cast<float32>(i);
                    const float32 vy = yOf(values[i]);
                    const bool    hb = hov && ctx.input.mousePos.x >= vx && ctx.input.mousePos.x < vx + bw;
                    if (hb) hoverIdx = i;
                    const float32 y0 = math::NkMin(vy, base), y1 = math::NkMax(vy, base);
                    ctx.DL().AddRectFilled({ vx + 1.f, y0, bw - 2.f, y1 - y0 }, hb ? ctx.theme.buttonHover : ctx.theme.accent, 0.f);
                }
            } else {
                const float32 dx = count > 1 ? g.w / static_cast<float32>(count - 1) : 0.f;
                for (int32 i = 1; i < count; ++i)
                    ctx.DL().AddLine({ g.x + dx * (i - 1), yOf(values[i - 1]) },
                                     { g.x + dx * i,       yOf(values[i])     }, ctx.theme.accent, 1.6f);
                if (hov && count > 1) {
                    int32 idx = static_cast<int32>(math::NkRound((ctx.input.mousePos.x - g.x) / dx));
                    hoverIdx = math::NkClamp(idx, 0, count - 1);
                    const NkVec2 hp = { g.x + dx * hoverIdx, yOf(values[hoverIdx]) };
                    ctx.DL().AddLine({ hp.x, g.y }, { hp.x, g.y + g.h }, ctx.theme.border, 1.f);
                    ctx.DL().AddCircleFilled(hp, 3.f, ctx.theme.text);
                }
            }

            if (!ctx.font || !ctx.font->Valid()) return;
            const float32 baseY = r.y + ctx.font->Ascent() + 3.f;
            if (label && *label)
                ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(), { r.x + 5.f, baseY }, label, ctx.theme.textDisabled);
            // ⭐ valeur sous le curseur affichée en direct (amélioration vs ImGui).
            if (hoverIdx >= 0) {
                const NkString s  = NkString::Format("%.2f", values[hoverIdx]);
                const float32  tw = ctx.font->MeasureWidth(s.CStr());
                ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(), { r.x + r.w - tw - 5.f, baseY }, s.CStr(), ctx.theme.text);
            }
        }

        void PlotLines(NkGuiContext& ctx, const char* label, const float32* values, int32 count,
                       float32 minV, float32 maxV, float32 height) noexcept {
            PlotEx(ctx, label, values, count, minV, maxV, height, false);
        }
        void PlotHistogram(NkGuiContext& ctx, const char* label, const float32* values, int32 count,
                           float32 minV, float32 maxV, float32 height) noexcept {
            PlotEx(ctx, label, values, count, minV, maxV, height, true);
        }

        // ── Couleur ───────────────────────────────────────────────────────────
        namespace {
            uint8   ColQ(float32 v) noexcept { v = math::NkClamp(v, 0.f, 1.f); return static_cast<uint8>(v * 255.f + 0.5f); }
            NkColor ColFromF(const float32* c, bool alpha) noexcept {
                return NkColor{ ColQ(c[0]), ColQ(c[1]), ColQ(c[2]), alpha ? ColQ(c[3]) : static_cast<uint8>(255) };
            }
            int32 PickerFind(NkGuiContext& ctx, NkGuiId id) noexcept {
                for (uint32 i = 0; i < ctx.pickerKeys.Size(); ++i) if (ctx.pickerKeys[i] == id) return static_cast<int32>(i);
                return -1;
            }
            // HSV courant (préserve la teinte mémorisée si le RGB n'a pas changé hors-picker).
            NkVec4 PickerGetHSV(NkGuiContext& ctx, NkGuiId id, const float32* col) noexcept {
                const int32 idx = PickerFind(ctx, id);
                if (idx >= 0) {
                    const NkVec4 st = ctx.pickerHSV[idx];
                    const NkColor rgb = NkColor::FromHSV(math::NkHSV(st.x, st.y, st.z));
                    const float32 tol = 1.6f / 255.f;
                    if (math::NkAbs(rgb.r / 255.f - col[0]) < tol && math::NkAbs(rgb.g / 255.f - col[1]) < tol
                     && math::NkAbs(rgb.b / 255.f - col[2]) < tol)
                        return NkVec4{ st.x, st.y, st.z, col[3] };   // garde la teinte
                }
                const math::NkHSV h = ColFromF(col, true).ToHSV();
                return NkVec4{ h.hue, h.saturation, h.value, col[3] };
            }
            void PickerSetHSV(NkGuiContext& ctx, NkGuiId id, const NkVec4& v) noexcept {
                const int32 idx = PickerFind(ctx, id);
                if (idx >= 0) ctx.pickerHSV[idx] = v;
                else { ctx.pickerKeys.PushBack(id); ctx.pickerHSV.PushBack(v); }
            }
            // Vrai tant qu'on glisse dans `region` (clic dedans → capture, suit la souris hors-zone).
            bool ColorDragRegion(NkGuiContext& ctx, NkGuiId subid, const NkRect& region) noexcept {
                const bool hov = ctx.ItemHoverable(region, subid);
                if (hov && ctx.input.mouseClicked[0]) ctx.activeId = subid;
                if (ctx.activeId == subid) {
                    if (ctx.input.mouseReleased[0]) ctx.activeId = NKGUI_ID_NONE;
                    return true;
                }
                return false;
            }
            void DrawHueBar(NkGuiContext& ctx, const NkRect& r) noexcept {
                const float32 seg = r.h / 6.f;
                static const float32 H6[7] = { 0.f, 60.f, 120.f, 180.f, 240.f, 300.f, 360.f };
                for (int32 i = 0; i < 6; ++i) {
                    const NkColor a = NkColor::FromHSV(math::NkHSV(H6[i],     100.f, 100.f));
                    const NkColor b = NkColor::FromHSV(math::NkHSV(H6[i + 1], 100.f, 100.f));
                    ctx.DL().AddRectFilledMultiColor({ r.x, r.y + seg * i, r.w, seg + 0.6f }, a, a, b, b);
                }
                ctx.DL().AddRect(r, ctx.theme.border, 1.f);
            }
            // Barre de VALEUR verticale : HSV(H,S,100) en haut → noir en bas. Glisser → V.
            bool DrawValueBar(NkGuiContext& ctx, NkGuiId subid, const NkRect& r, float32 H, float32 S, float32& V) noexcept {
                const NkColor top = NkColor::FromHSV(math::NkHSV(H, S, 100.f));
                ctx.DL().AddRectFilledMultiColor(r, top, top, NkColor{ 0, 0, 0, 255 }, NkColor{ 0, 0, 0, 255 });
                ctx.DL().AddRect(r, ctx.theme.border, 1.f);
                bool ch = false;
                if (ColorDragRegion(ctx, subid, r)) { V = math::NkClamp(1.f - (ctx.input.mousePos.y - r.y) / r.h, 0.f, 1.f) * 100.f; ch = true; }
                const float32 vy = r.y + (1.f - V / 100.f) * r.h;
                ctx.DL().AddRect({ r.x - 2.f, vy - 2.f, r.w + 4.f, 4.f }, NkColor{ 255, 255, 255, 255 }, 1.5f);
                return ch;
            }
            // Picker RADIAL : disque plein (hexagon=false) OU hexagone dégradé (hexagon=true).
            // teinte = angle, saturation = rayon/R, valeur fournie. Glisser → H,S.
            bool PickRadial(NkGuiContext& ctx, NkGuiId id, const NkRect& area, bool hexagon,
                            float32& H, float32& S, float32 V) noexcept {
                const float32 R   = area.w * 0.5f - 1.f;
                const NkVec2  ctr { area.x + area.w * 0.5f, area.y + area.h * 0.5f };
                const NkColor cC  = NkColor::FromHSV(math::NkHSV(H, 0.f, V));   // centre = gris(V)
                const int32   N   = hexagon ? 6 : 64;
                const float32 st  = math::NK_2PI_F / static_cast<float32>(N);
                const float32 off = hexagon ? (math::NK_PI_F / 6.f) : 0.f;     // hexagone pointe en haut
                for (int32 i = 0; i < N; ++i) {
                    const float32 a0 = off + i * st, a1 = off + (i + 1) * st;
                    const NkColor c0 = NkColor::FromHSV(math::NkHSV((i * st) / math::NK_2PI_F * 360.f, 100.f, V));
                    const NkColor c1 = NkColor::FromHSV(math::NkHSV(((i + 1) * st) / math::NK_2PI_F * 360.f, 100.f, V));
                    const NkVec2  p0 { ctr.x + math::NkCos(a0) * R, ctr.y + math::NkSin(a0) * R };
                    const NkVec2  p1 { ctr.x + math::NkCos(a1) * R, ctr.y + math::NkSin(a1) * R };
                    ctx.DL().AddTriangleMultiColor(ctr, p0, p1, cC, c0, c1);
                }
                bool ch = false;
                const float32 dx = ctx.input.mousePos.x - ctr.x, dy = ctx.input.mousePos.y - ctr.y;
                const float32 dist = math::NkSqrt(dx * dx + dy * dy);
                const NkGuiId did = id ^ 0xC010C001u;
                const bool inArea = NkGuiRectContains(area, ctx.input.mousePos)
                                 && NkGuiRectContains(ctx.DL().CurrentClip(), ctx.input.mousePos);
                if (inArea && ctx.input.mouseClicked[0] && ctx.activeId == NKGUI_ID_NONE && dist <= R + 3.f) ctx.activeId = did;
                if (ctx.activeId == did) {
                    if (ctx.input.mouseReleased[0]) ctx.activeId = NKGUI_ID_NONE;
                    float32 ang = math::NkAtan2(dy, dx); if (ang < 0.f) ang += math::NK_2PI_F;
                    H = ang / math::NK_2PI_F * 360.f;
                    S = math::NkClamp(dist / R, 0.f, 1.f) * 100.f;
                    ch = true;
                }
                const float32 ca = H / 360.f * math::NK_2PI_F, rr = S / 100.f * R;
                const NkVec2 cp{ ctr.x + math::NkCos(ca) * rr, ctr.y + math::NkSin(ca) * rr };
                ctx.DL().AddCircleFilled(cp, 5.f, NkColor{ 0, 0, 0, 255 });
                ctx.DL().AddCircleFilled(cp, 3.5f, NkColor{ 255, 255, 255, 255 });
                return ch;
            }
            // Palette NID D'ABEILLE : grille de pastilles hexagonales (teinte × saturation).
            // Clic = choisit la couleur de la cellule. Dimensions : cols×rows.
            bool PickHoney(NkGuiContext& ctx, const NkRect& area, float32* col,
                           float32& H, float32& S, float32& V) noexcept {
                const int32   cols = 7, rows = 5;
                const float32 rHex = 15.f, hsp = 1.732f * rHex, vsp = 1.5f * rHex;
                bool ch = false;
                for (int32 r = 0; r < rows; ++r) {
                    const float32 cy   = area.y + rHex + r * vsp;
                    const float32 xoff = (r & 1) ? hsp * 0.5f : 0.f;
                    for (int32 c = 0; c < cols; ++c) {
                        const float32 cx  = area.x + rHex + xoff + c * hsp;
                        const float32 hue = static_cast<float32>(c) / cols * 360.f;
                        const float32 sat = 100.f * static_cast<float32>(r + 1) / rows;
                        const NkColor cc  = NkColor::FromHSV(math::NkHSV(hue, sat, 100.f));
                        NkVec2 corner[6];
                        for (int32 k = 0; k < 6; ++k) {
                            const float32 ang = (30.f + k * 60.f) * 0.0174533f;
                            corner[k] = { cx + math::NkCos(ang) * rHex, cy + math::NkSin(ang) * rHex };
                        }
                        for (int32 k = 0; k < 6; ++k)
                            ctx.DL().AddTriangleFilled({ cx, cy }, corner[k], corner[(k + 1) % 6], cc);
                        const float32 dx = ctx.input.mousePos.x - cx, dy = ctx.input.mousePos.y - cy;
                        const bool hovCell = (dx * dx + dy * dy < rHex * rHex)
                                          && NkGuiRectContains(ctx.DL().CurrentClip(), ctx.input.mousePos);
                        if (hovCell) {
                            for (int32 k = 0; k < 6; ++k)
                                ctx.DL().AddLine(corner[k], corner[(k + 1) % 6], NkColor{ 255, 255, 255, 255 }, 1.5f);
                            if (ctx.input.mouseClicked[0]) {
                                col[0] = cc.r / 255.f; col[1] = cc.g / 255.f; col[2] = cc.b / 255.f;
                                H = hue; S = sat; V = 100.f; ch = true;
                            }
                        }
                    }
                }
                return ch;
            }
        } // namespace

        bool ColorButton(NkGuiContext& ctx, const char* idStr, const float32* col,
                         float32 w, float32 h, NkGuiColorFlags /*flags*/) noexcept {
            const float32 hh = h > 0.f ? h : ctx.ItemHeight();
            const float32 ww = w > 0.f ? w : hh * 1.6f;
            const NkRect  r  = ctx.NextItemRect(ww, hh);
            bool hov = false, held = false;
            const bool clicked = ctx.ButtonBehavior(ctx.GetId(idStr), r, NkGuiButtonFlags::None, -1.f, -1.f, &hov, &held);
            ctx.DL().AddRectFilled(r, ColFromF(col, false), ctx.theme.rounding);
            ctx.DL().AddRect(r, hov ? ctx.theme.accent : ctx.theme.border, 1.f);
            return clicked;
        }

        bool ColorPicker4(NkGuiContext& ctx, const char* label, float32* col, NkGuiColorFlags flags) noexcept {
            const NkGuiId id       = ctx.GetId(label);
            const bool    noAlpha  = NkGuiHasColorFlag(flags, NkGuiColorFlags::NoAlpha);
            const bool    wheel    = NkGuiHasColorFlag(flags, NkGuiColorFlags::Wheel);
            const bool    noInputs = NkGuiHasColorFlag(flags, NkGuiColorFlags::NoInputs);
            const NkVec4  hsv      = PickerGetHSV(ctx, id, col);
            float32 H = hsv.x, S = hsv.y, V = hsv.z, A = noAlpha ? 1.f : col[3];
            bool changed = false;

            const NkColor WHITE{ 255, 255, 255, 255 }, BLACK{ 0, 0, 0, 255 };
            const bool disc   = NkGuiHasColorFlag(flags, NkGuiColorFlags::Disc);
            const bool hexg   = NkGuiHasColorFlag(flags, NkGuiColorFlags::Hexagon);
            const bool honey  = NkGuiHasColorFlag(flags, NkGuiColorFlags::Honeycomb);
            const bool radial = disc || hexg;
            const bool bar    = !wheel && !radial && !honey;
            const float32 sq = 160.f, barW = 16.f, gap = 12.f;
            const float32 honeyW = 205.f, honeyH = 132.f;
            const float32 mainW = honey ? honeyW : sq;
            const float32 mainH = honey ? honeyH : sq;
            const int32   nBars = (noAlpha ? 0 : 1) + (bar ? 1 : 0) + (radial ? 1 : 0);
            const float32 totalW = mainW + nBars * (gap + barW);
            const NkRect  area = ctx.NextItemRect(totalW, mainH);
            const NkColor hueCol = NkColor::FromHSV(math::NkHSV(H, 100.f, 100.f));
            float32 sbX = area.x + mainW + gap;   // x de la prochaine barre latérale

            if (radial) {
                changed |= PickRadial(ctx, id, { area.x, area.y, sq, sq }, hexg, H, S, V);
                const NkRect valR = { sbX, area.y, barW, sq }; sbX += barW + gap;
                changed |= DrawValueBar(ctx, id ^ 0xC010C002u, valR, H, S, V);
            } else if (honey) {
                changed |= PickHoney(ctx, { area.x, area.y, honeyW, honeyH }, col, H, S, V);
            } else if (wheel) {
                // ── Roue de teinte + triangle SV (circulaire ET triangulaire) ──
                const float32 R = sq * 0.5f;
                const NkVec2  ctr{ area.x + R, area.y + R };
                const float32 rOut = R - 1.f, rIn = R * 0.74f;
                const int32   NSEG = 64;
                for (int32 i = 0; i < NSEG; ++i) {
                    const float32 a0 = static_cast<float32>(i) / NSEG * math::NK_2PI_F;
                    const float32 a1 = static_cast<float32>(i + 1) / NSEG * math::NK_2PI_F;
                    const NkColor c0 = NkColor::FromHSV(math::NkHSV(a0 / math::NK_2PI_F * 360.f, 100.f, 100.f));
                    const NkColor c1 = NkColor::FromHSV(math::NkHSV(a1 / math::NK_2PI_F * 360.f, 100.f, 100.f));
                    const float32 x0 = math::NkCos(a0), y0 = math::NkSin(a0), x1 = math::NkCos(a1), y1 = math::NkSin(a1);
                    const NkVec2 i0{ ctr.x + x0 * rIn, ctr.y + y0 * rIn }, o0{ ctr.x + x0 * rOut, ctr.y + y0 * rOut };
                    const NkVec2 i1{ ctr.x + x1 * rIn, ctr.y + y1 * rIn }, o1{ ctr.x + x1 * rOut, ctr.y + y1 * rOut };
                    ctx.DL().AddTriangleMultiColor(i0, o0, o1, c0, c0, c1);
                    ctx.DL().AddTriangleMultiColor(i0, o1, i1, c0, c1, c1);
                }
                const float32 ha = H / 360.f * math::NK_2PI_F, r3 = rIn - 1.f;
                const float32 T2 = 2.0943951f, T4 = 4.1887902f;   // 120°, 240°
                const NkVec2 vHue  { ctr.x + math::NkCos(ha) * r3,      ctr.y + math::NkSin(ha) * r3 };
                const NkVec2 vWhite{ ctr.x + math::NkCos(ha + T2) * r3, ctr.y + math::NkSin(ha + T2) * r3 };
                const NkVec2 vBlack{ ctr.x + math::NkCos(ha + T4) * r3, ctr.y + math::NkSin(ha + T4) * r3 };
                ctx.DL().AddTriangleMultiColor(vHue, vWhite, vBlack, hueCol, WHITE, BLACK);

                const float32 dx = ctx.input.mousePos.x - ctr.x, dy = ctx.input.mousePos.y - ctr.y;
                const float32 dist = math::NkSqrt(dx * dx + dy * dy);
                const NkGuiId ringId = id ^ 0xC010B001u, triId = id ^ 0xC010B002u;
                const bool inArea = NkGuiRectContains(area, ctx.input.mousePos)
                                 && NkGuiRectContains(ctx.DL().CurrentClip(), ctx.input.mousePos);
                if (inArea && ctx.input.mouseClicked[0] && ctx.activeId == NKGUI_ID_NONE) {
                    if (dist >= rIn - 3.f && dist <= rOut + 3.f) ctx.activeId = ringId;
                    else if (dist < rIn)                         ctx.activeId = triId;
                }
                if (ctx.activeId == ringId) {
                    if (ctx.input.mouseReleased[0]) ctx.activeId = NKGUI_ID_NONE;
                    float32 ang = math::NkAtan2(dy, dx); if (ang < 0.f) ang += math::NK_2PI_F;
                    H = ang / math::NK_2PI_F * 360.f; changed = true;
                } else if (ctx.activeId == triId) {
                    if (ctx.input.mouseReleased[0]) ctx.activeId = NKGUI_ID_NONE;
                    // Barycentrique de la souris dans (vHue, vWhite, vBlack).
                    const float32 e0x = vWhite.x - vHue.x, e0y = vWhite.y - vHue.y;
                    const float32 e1x = vBlack.x - vHue.x, e1y = vBlack.y - vHue.y;
                    const float32 e2x = ctx.input.mousePos.x - vHue.x, e2y = ctx.input.mousePos.y - vHue.y;
                    const float32 d00 = e0x * e0x + e0y * e0y, d01 = e0x * e1x + e0y * e1y, d11 = e1x * e1x + e1y * e1y;
                    const float32 d20 = e2x * e0x + e2y * e0y, d21 = e2x * e1x + e2y * e1y;
                    const float32 den = d00 * d11 - d01 * d01;
                    float32 wW = den != 0.f ? (d11 * d20 - d01 * d21) / den : 0.f;
                    float32 wB = den != 0.f ? (d00 * d21 - d01 * d20) / den : 0.f;
                    float32 wH = 1.f - wW - wB;
                    if (wH < 0.f) wH = 0.f; if (wW < 0.f) wW = 0.f; if (wB < 0.f) wB = 0.f;
                    const float32 sum = wH + wW + wB; if (sum > 0.f) { wH /= sum; wW /= sum; }
                    V = (wH + wW) * 100.f;
                    S = (wH + wW) > 0.f ? wH / (wH + wW) * 100.f : 0.f;
                    changed = true;
                }

                // Curseurs : teinte (sur l'anneau) + SV (dans le triangle).
                const float32 ha2 = H / 360.f * math::NK_2PI_F, rMid = (rIn + rOut) * 0.5f;
                const NkVec2 hp{ ctr.x + math::NkCos(ha2) * rMid, ctr.y + math::NkSin(ha2) * rMid };
                ctx.DL().AddCircleFilled(hp, 5.f, BLACK); ctx.DL().AddCircleFilled(hp, 3.5f, WHITE);
                const float32 wH2 = (S / 100.f) * (V / 100.f), wW2 = (1.f - S / 100.f) * (V / 100.f), wB2 = 1.f - V / 100.f;
                const NkVec2 svc{ vHue.x * wH2 + vWhite.x * wW2 + vBlack.x * wB2,
                                  vHue.y * wH2 + vWhite.y * wW2 + vBlack.y * wB2 };
                ctx.DL().AddCircleFilled(svc, 5.f, BLACK); ctx.DL().AddCircleFilled(svc, 3.5f, WHITE);
            } else {
                // ── Carré SV (2 passes) + barre de teinte ──
                const NkRect svR  = { area.x, area.y, sq, sq };
                const NkRect hueR = { sbX, area.y, barW, sq }; sbX += barW + gap;
                if (ColorDragRegion(ctx, id ^ 0xC010A001u, svR)) {
                    S = math::NkClamp((ctx.input.mousePos.x - svR.x) / svR.w, 0.f, 1.f) * 100.f;
                    V = math::NkClamp(1.f - (ctx.input.mousePos.y - svR.y) / svR.h, 0.f, 1.f) * 100.f;
                    changed = true;
                }
                if (ColorDragRegion(ctx, id ^ 0xC010A002u, hueR)) {
                    H = math::NkClamp((ctx.input.mousePos.y - hueR.y) / hueR.h, 0.f, 1.f) * 359.99f;
                    changed = true;
                }
                ctx.DL().AddRectFilledMultiColor(svR, WHITE, hueCol, hueCol, WHITE);
                ctx.DL().AddRectFilledMultiColor(svR, NkColor{ 0, 0, 0, 0 }, NkColor{ 0, 0, 0, 0 }, BLACK, BLACK);
                ctx.DL().AddRect(svR, ctx.theme.border, 1.f);
                DrawHueBar(ctx, hueR);
                const NkVec2 svc = { svR.x + (S / 100.f) * svR.w, svR.y + (1.f - V / 100.f) * svR.h };
                ctx.DL().AddCircleFilled(svc, 5.f, BLACK); ctx.DL().AddCircleFilled(svc, 3.5f, WHITE);
                const float32 hy = hueR.y + (H / 360.f) * hueR.h;
                ctx.DL().AddRect({ hueR.x - 2.f, hy - 2.f, hueR.w + 4.f, 4.f }, WHITE, 1.5f);
            }

            // Barre alpha (couleur opaque → transparente sur fond gris), à droite (sbX).
            if (!noAlpha) {
                const NkRect alfR = { sbX, area.y, barW, mainH };
                if (ColorDragRegion(ctx, id ^ 0xC010A003u, alfR)) {
                    A = math::NkClamp(1.f - (ctx.input.mousePos.y - alfR.y) / alfR.h, 0.f, 1.f);
                    changed = true;
                }
                NkColor full = NkColor::FromHSV(math::NkHSV(H, S, V)); full.a = 255;
                NkColor clr  = full; clr.a = 0;
                ctx.DL().AddRectFilled(alfR, NkColor{ 90, 90, 90, 255 });
                ctx.DL().AddRectFilledMultiColor(alfR, full, full, clr, clr);
                ctx.DL().AddRect(alfR, ctx.theme.border, 1.f);
                const float32 ay = alfR.y + (1.f - A) * alfR.h;
                ctx.DL().AddRect({ alfR.x - 2.f, ay - 2.f, alfR.w + 4.f, 4.f }, WHITE, 1.5f);
            }

            if (changed) {
                const NkColor rgb = NkColor::FromHSV(math::NkHSV(H, S, V));
                col[0] = rgb.r / 255.f; col[1] = rgb.g / 255.f; col[2] = rgb.b / 255.f;
                if (!noAlpha) col[3] = A;
            }

            // ── Champs numériques RGB + HSV (éditables, glisser/double-clic) ──
            if (!noInputs) {
                ctx.PushId(label);
                const NkColor cur = ColFromF(col, true);
                int32 Rr = cur.r, Gg = cur.g, Bb = cur.b;
                bool rgbCh = false;
                rgbCh |= DragInt(ctx, "R", Rr, 1.f, 0, 255);
                rgbCh |= DragInt(ctx, "G", Gg, 1.f, 0, 255);
                rgbCh |= DragInt(ctx, "B", Bb, 1.f, 0, 255);
                if (rgbCh) {
                    col[0] = Rr / 255.f; col[1] = Gg / 255.f; col[2] = Bb / 255.f;
                    const math::NkHSV nh = ColFromF(col, true).ToHSV();
                    H = nh.hue; S = nh.saturation; V = nh.value; changed = true;
                }
                int32 Hi = static_cast<int32>(H + 0.5f), Si = static_cast<int32>(S + 0.5f), Vi = static_cast<int32>(V + 0.5f);
                bool hsvCh = false;
                hsvCh |= DragInt(ctx, "H", Hi, 1.f, 0, 360);
                hsvCh |= DragInt(ctx, "S", Si, 1.f, 0, 100);
                hsvCh |= DragInt(ctx, "V", Vi, 1.f, 0, 100);
                if (hsvCh) {
                    H = static_cast<float32>(Hi); S = static_cast<float32>(Si); V = static_cast<float32>(Vi);
                    const NkColor rgb = NkColor::FromHSV(math::NkHSV(H, S, V));
                    col[0] = rgb.r / 255.f; col[1] = rgb.g / 255.f; col[2] = rgb.b / 255.f; changed = true;
                }
                if (!noAlpha) {
                    int32 Ai = static_cast<int32>(A * 255.f + 0.5f);
                    if (DragInt(ctx, "A", Ai, 1.f, 0, 255)) { A = Ai / 255.f; col[3] = A; changed = true; }
                }
                ctx.PopId();
            }
            PickerSetHSV(ctx, id, NkVec4{ H, S, V, A });
            return changed;
        }

        bool ColorEdit4(NkGuiContext& ctx, const char* label, float32* col, NkGuiColorFlags flags) noexcept {
            const NkGuiId id      = ctx.GetId(label);
            const bool    noAlpha = NkGuiHasColorFlag(flags, NkGuiColorFlags::NoAlpha);
            const float32 h       = ctx.ItemHeight();
            const NkRect  row     = ctx.NextItemRect(0.f, h);
            bool changed = false;

            // Pastille cliquable (gauche).
            const NkRect sw = { row.x, row.y, h * 1.4f, h };
            bool shov = false, sheld = false;
            const bool sclick = ctx.ButtonBehavior(id, sw, NkGuiButtonFlags::None, -1.f, -1.f, &shov, &sheld);
            ctx.DL().AddRectFilled(sw, ColFromF(col, false), ctx.theme.rounding);
            ctx.DL().AddRect(sw, (shov || ctx.IsPopupOpen(id)) ? ctx.theme.accent : ctx.theme.border, 1.f);
            if (sclick) { if (ctx.IsPopupOpen(id)) ctx.ClosePopup(); else ctx.OpenPopup(id); }

            // Code hexadécimal + label.
            if (ctx.font && ctx.font->Valid()) {
                const NkColor c = ColFromF(col, true);
                const NkString hex = noAlpha
                    ? NkString::Format("#%02X%02X%02X", static_cast<uint32>(c.r), static_cast<uint32>(c.g), static_cast<uint32>(c.b))
                    : NkString::Format("#%02X%02X%02X%02X", static_cast<uint32>(c.r), static_cast<uint32>(c.g), static_cast<uint32>(c.b), static_cast<uint32>(c.a));
                const float32 hx = sw.x + sw.w + 10.f;
                ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(), { hx, CenteredBaseline(ctx, row) }, hex.CStr(), ctx.theme.text);
                if (label && *label)
                    ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
                                     { hx + ctx.font->MeasureWidth(hex.CStr()) + 14.f, CenteredBaseline(ctx, row) },
                                     label, ctx.theme.text);
            }

            // Popup picker sous la pastille (dimensionné selon mode + champs).
            if (ctx.IsPopupOpen(id)) {
                const bool noInputs = NkGuiHasColorFlag(flags, NkGuiColorFlags::NoInputs);
                const float32 visualW = (NkGuiHasColorFlag(flags, NkGuiColorFlags::Wheel) ? 160.f : 160.f + 12.f + 16.f)
                                      + (noAlpha ? 0.f : 12.f + 16.f);
                const float32 pw = visualW + 24.f;
                const int32   nFields = noInputs ? 0 : (6 + (noAlpha ? 0 : 1));
                const float32 ph = 160.f + 16.f + nFields * (ctx.ItemHeight() + ctx.layout.itemSpacingY);
                NkRect pr = { sw.x, sw.y + h + 2.f, pw, ph };
                if (pr.y + pr.h > static_cast<float32>(ctx.viewH)) pr.y = sw.y - ph - 2.f;
                if (pr.y < 0.f) pr.y = 2.f;
                if (BeginPopupLevel(ctx, id, 0, pr, sw)) {
                    changed |= ColorPicker4(ctx, "##pick", col, flags);
                    EndPopup(ctx);
                }
            }
            return changed;
        }

        // ── Image / Icône ───────────────────────────────────────────────────────
        void Image(NkGuiContext& ctx, uint32 texId, float32 w, float32 h,
                   NkColor tint, NkVec2 uv0, NkVec2 uv1) noexcept {
            const float32 ww = w > 0.f ? w : ctx.ContentWidth();
            const float32 hh = h > 0.f ? h : ww;
            const NkRect  r  = ctx.NextItemRect(ww, hh);
            ctx.DL().AddImage(texId, r, uv0, uv1, tint);
            ctx.DL().AddRect(r, ctx.theme.border, 1.f);
        }

        bool ImageButton(NkGuiContext& ctx, const char* idStr, uint32 texId, float32 w, float32 h,
                         NkColor tint, NkVec2 uv0, NkVec2 uv1) noexcept {
            const float32 pad = 4.f;
            const float32 ww  = (w > 0.f ? w : ctx.ItemHeight()) + 2.f * pad;
            const float32 hh  = (h > 0.f ? h : ctx.ItemHeight()) + 2.f * pad;
            const NkRect  r   = ctx.NextItemRect(ww, hh);
            bool hov = false, held = false;
            const bool clicked = ctx.ButtonBehavior(ctx.GetId(idStr), r, NkGuiButtonFlags::None, -1.f, -1.f, &hov, &held);
            ctx.DL().AddRectFilled(r, held ? ctx.theme.buttonActive : hov ? ctx.theme.buttonHover : ctx.theme.button, ctx.theme.rounding);
            ctx.DL().AddImage(texId, { r.x + pad, r.y + pad, r.w - 2.f * pad, r.h - 2.f * pad }, uv0, uv1, tint);
            ctx.DL().AddRect(r, hov ? ctx.theme.accent : ctx.theme.border, 1.f);
            return clicked;
        }

        bool TableCellText(NkGuiContext& ctx, const char* idStr, char* buf, int32 bufSize,
                           bool editable) noexcept {
            if (!buf || bufSize <= 1) return false;
            const NkRect  field = ctx.layout.region;   // = la cellule ouverte par TableNextColumn
            const NkGuiId id    = ctx.GetId(idStr);     // id STABLE (≠ texte édité)

            // ── Édition inline (même schéma que SelectableEditable) ───────────
            if (editable && ctx.renameId == id) {
                const bool clickAway = ctx.input.mouseClicked[0]
                                    && !NkGuiRectContains(field, ctx.input.mousePos);
                ctx.inputId            = id;
                ctx.inputClickConsumed = true;
                const bool submitted   = TextEditField(ctx, field, buf, bufSize, true);
                if (ctx.input.KeyPressed(NkGuiKey::Escape)) {
                    int32 n = 0;
                    while (n < bufSize - 1 && ctx.renameBackup[n]) { buf[n] = ctx.renameBackup[n]; ++n; }
                    buf[n] = '\0';
                    ctx.renameId = NKGUI_ID_NONE; ctx.inputId = NKGUI_ID_NONE;
                    return false;
                }
                if (submitted || clickAway) {
                    ctx.renameId = NKGUI_ID_NONE; ctx.inputId = NKGUI_ID_NONE;
                    return true;                                   // édition validée
                }
                return false;
            }

            // ── Affichage (+ amorce d'édition au double-clic si editable) ─────
            bool hov = false, held = false;
            if (editable) {
                ctx.ButtonBehavior(id, field, NkGuiButtonFlags::None, -1.f, -1.f, &hov, &held);
                if (hov) ctx.wantCursor = NkGuiCursor::Text;
                if (hov && ctx.input.mouseDoubleClicked[0] && !ctx.IsDisabled()) {
                    ctx.renameId = id; ctx.inputId = id;
                    int32 l = 0; while (l < bufSize - 1 && buf[l]) ++l;
                    ctx.inputCaret  = l;
                    ctx.inputScroll = 0.f;
                    int32 n = 0; while (n < 127 && buf[n]) { ctx.renameBackup[n] = buf[n]; ++n; }
                    ctx.renameBackup[n] = '\0';
                }
                if (hov) ctx.DL().AddRectFilled(field, ctx.theme.header, 0.f);   // indice « éditable »
            }
            if (ctx.font && ctx.font->Valid()) {
                ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
                               { field.x + 5.f, CenteredBaseline(ctx, field) },
                               buf, ctx.IsDisabled() ? ctx.theme.textDisabled : ctx.theme.text,
                               field.w - 9.f);
            }
            return false;
        }

        // ── Popups / overlay (pile, sous-menus imbriqués) ────────────────────
        // Dessine le popup `id` au NIVEAU `level` dans la couche overlay (au-dessus).
        // `anchor` = déclencheur du 1er niveau (ne ferme pas au clic). Sauve/restaure
        // le layout. Retourne true si ce niveau est ouvert pour cet id.
        static bool BeginPopupLevel(NkGuiContext& ctx, NkGuiId id, int32 level,
                                    const NkRect& rect, const NkRect& anchor) noexcept {
            if (level < 0 || level >= NkGuiContext::PopupMax) return false;
            if (level >= ctx.popupDepth || ctx.popupStack[level] != id) return false;
            ctx.popupRects[level] = rect;
            if (level == 0) ctx.popupAnchor = anchor;
            ctx.popupSaved[level] = ctx.layout;
            ctx.curPopupLevel     = level;               // → ctx.DL() = overlay
            // Clip EN PREMIER, intersect=false : un popup est une surface de PREMIER
            // PLAN ; son clip ne doit PAS être réduit par celui du menu parent (sinon
            // un sous-menu, qui sort du parent vers la droite, serait clippé à zéro).
            // Doit précéder le dessin du fond, sinon le fond reste sous le clip parent.
            // Fond + bordure dessinés HORS clip (clip plein écran temporaire) : un
            // popup est une surface de PREMIER PLAN, il ne doit pas être réduit par le
            // clip du menu parent, ET sa bordure (bords droit/bas) ne doit pas être
            // rognée par un scissor serré == rect. Puis clip serré pour le CONTENU seul.
            ctx.DL().PushClipRect({ 0.f, 0.f, 1.0e9f, 1.0e9f }, false);   // plein écran
            ctx.DL().AddRectFilled(rect, ctx.theme.panel, ctx.theme.rounding);
            ctx.DL().AddRect(rect, ctx.theme.border, 1.f);
            ctx.DL().PopClipRect();
            ctx.DL().PushClipRect(rect, false);                          // contenu
            ctx.BeginLayout(rect);
            return true;
        }

        void EndPopup(NkGuiContext& ctx) noexcept {
            const int32 level = ctx.curPopupLevel;
            if (level < 0) return;
            ctx.DL().PopClipRect();
            ctx.layout        = ctx.popupSaved[level];   // restaure le layout du parent
            ctx.curPopupLevel = level - 1;               // revient au parent / principale
        }

        bool BeginCombo(NkGuiContext& ctx, const char* label, const char* preview, int32 itemCount) noexcept {
            const float32 h    = ctx.ItemHeight();
            const NkRect  rowR = ctx.NextItemRect(0.f, h);
            const NkGuiId id   = ctx.GetId(label);

            const float32 labelW = (ctx.font && ctx.font->Valid() && label && *label)
                                 ? ctx.font->MeasureWidth(label) + 14.f : 0.f;
            const NkRect field = { rowR.x, rowR.y, rowR.w - labelW, rowR.h };

            bool hov = false, held = false;
            const bool clicked = ctx.ButtonBehavior(id, field, NkGuiButtonFlags::None, -1.f, -1.f, &hov, &held);
            if (clicked) { if (ctx.IsPopupOpen(id)) ctx.ClosePopup(); else ctx.OpenPopup(id); }
            const bool open = ctx.IsPopupOpen(id);

            // Champ (couche principale).
            ctx.DL().AddRectFilled(field, ctx.theme.track, ctx.theme.rounding);
            ctx.DL().AddRect(field, (hov || open) ? ctx.theme.accent : ctx.theme.border, open ? 1.5f : 1.f);
            if (ctx.font && ctx.font->Valid() && preview) {
                const NkRect clip = { field.x + 6.f, field.y, field.w - 26.f, field.h };
                ctx.DL().PushClipRect(clip, true);
                ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
                                 { field.x + 8.f, CenteredBaseline(ctx, field) }, preview, ctx.theme.text);
                ctx.DL().PopClipRect();
            }
            // Chevron bas.
            const float32 a  = h * 0.16f;
            const NkVec2  cc = { field.x + field.w - 13.f, field.y + field.h * 0.5f };
            ctx.DL().AddTriangleFilled({ cc.x - a, cc.y - a * 0.5f }, { cc.x + a, cc.y - a * 0.5f },
                                       { cc.x, cc.y + a * 0.8f }, ctx.theme.text);
            if (ctx.font && ctx.font->Valid() && label && *label) {
                ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
                                 { field.x + field.w + 12.f, CenteredBaseline(ctx, field) }, label, ctx.theme.text);
            }

            if (!open) return false;

            // Popup sous le champ (rabattu vers le haut si ça déborde en bas).
            const int32   n  = itemCount < 1 ? 1 : itemCount;
            const float32 ph = n * (h + ctx.layout.itemSpacingY) + 14.f;
            NkRect pr = { field.x, field.y + field.h + 2.f, field.w, ph };
            if (pr.y + pr.h > static_cast<float32>(ctx.viewH)) pr.y = field.y - ph - 2.f;
            return BeginPopupLevel(ctx, id, 0, pr, field);
        }

        void EndCombo(NkGuiContext& ctx) noexcept { EndPopup(ctx); }

        // ── Menus (barre + sous-menus imbriqués + menu contextuel) ───────────
        // Taille auto-mesurée d'un menu (mesurée une frame, appliquée la suivante).
        static NkVec2 MenuSizeGet(NkGuiContext& ctx, NkGuiId id) noexcept {
            for (uint32 i = 0; i < ctx.menuKeys.Size(); ++i)
                if (ctx.menuKeys[i] == id) return ctx.menuSizes[i];
            return { 170.f, ctx.ItemHeight() + 10.f };     // défaut (1re frame)
        }
        static void MenuSizeSet(NkGuiContext& ctx, NkGuiId id, NkVec2 s) noexcept {
            for (uint32 i = 0; i < ctx.menuKeys.Size(); ++i)
                if (ctx.menuKeys[i] == id) { ctx.menuSizes[i] = s; return; }
            ctx.menuKeys.PushBack(id); ctx.menuSizes.PushBack(s);
        }

        bool BeginMenuBar(NkGuiContext& ctx, const NkRect& rect) noexcept {
            ctx.DL().AddRectFilled(rect, ctx.theme.header, 0.f);
            ctx.DL().AddRectFilled({ rect.x, rect.y + rect.h - 1.f, rect.w, 1.f }, ctx.theme.border);
            ctx.menuBarRect = rect;
            ctx.menuBarX    = rect.x + 4.f;
            return true;
        }
        void EndMenuBar(NkGuiContext&) noexcept {}

        // Entrée de menu DÉROULANT — unifiée : dans une MenuBar (curPopupLevel<0 →
        // titre horizontal, popup dessous) OU dans un menu (sous-menu : ligne avec
        // ▶, flyout à droite). Récursif → sous-sous-menus à profondeur arbitraire.
        bool BeginMenu(NkGuiContext& ctx, const char* label) noexcept {
            const int32   level = ctx.curPopupLevel + 1;
            const NkGuiId id    = ctx.GetId(label);
            const bool    inBar = (ctx.curPopupLevel < 0);
            const float32 tw    = (ctx.font && ctx.font->Valid()) ? ctx.font->MeasureWidth(label) : 40.f;

            NkRect titleR; NkVec2 popupAt; NkRect anchor;
            if (inBar) {
                const float32 h = ctx.menuBarRect.h;
                titleR  = { ctx.menuBarX, ctx.menuBarRect.y, tw + 20.f, h };
                ctx.menuBarX += tw + 20.f;
                popupAt = { titleR.x, titleR.y + titleR.h };     // popup SOUS le titre
                anchor  = ctx.menuBarRect;                        // toute la barre = ancre
            } else {
                const float32 h = ctx.ItemHeight();
                titleR  = ctx.NextItemRect(0.f, h);
                const int32 L = ctx.curPopupLevel;                // mesure dans le menu parent
                if (L >= 0 && ctx.menuMeasureId[L] != NKGUI_ID_NONE) {
                    if (tw + 34.f > ctx.menuMeasureW[L]) ctx.menuMeasureW[L] = tw + 34.f;
                    ctx.menuMeasureH[L] += h + ctx.layout.itemSpacingY;
                }
                // Flyout au bord DROIT du menu parent (rect réel), sans chevaucher
                // le contenu (évite l'artefact de bordure dans la zone de recouvrement).
                const NkRect& pr0 = ctx.popupRects[ctx.curPopupLevel];
                popupAt = { pr0.x + pr0.w - 1.f, titleR.y - 1.f };
                anchor  = ctx.popupAnchor;
            }

            bool hov = false, held = false;
            const bool clicked = ctx.ButtonBehavior(id, titleR, NkGuiButtonFlags::None, -1.f, -1.f, &hov, &held);
            const bool curOpen = (ctx.popupDepth > level && ctx.popupStack[level] == id);
            if (inBar) {
                if (clicked) { if (curOpen) ctx.ClosePopup(); else ctx.OpenPopupLevel(id, level); }
                else if (hov && ctx.popupDepth > 0 && ctx.popupStack[0] != id) ctx.OpenPopupLevel(id, level);
            } else {
                if (hov || clicked) ctx.OpenPopupLevel(id, level);  // sous-menu : survol ouvre
            }
            const bool open = (ctx.popupDepth > level && ctx.popupStack[level] == id);

            // Titre / entrée.
            if (hov || open) ctx.DL().AddRectFilled(titleR, ctx.theme.buttonHover, inBar ? 0.f : 3.f);
            if (inBar) {
                DrawCenteredLabel(ctx, titleR, label, ctx.theme.text);
            } else if (ctx.font && ctx.font->Valid()) {
                ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
                                 { titleR.x + 8.f, CenteredBaseline(ctx, titleR) }, label, ctx.theme.text);
                const float32 a  = ctx.ItemHeight() * 0.16f;       // chevron ▶ (a un sous-menu)
                const NkVec2  cc = { titleR.x + titleR.w - 12.f, titleR.y + titleR.h * 0.5f };
                ctx.DL().AddTriangleFilled({ cc.x - a * 0.6f, cc.y - a }, { cc.x - a * 0.6f, cc.y + a },
                                           { cc.x + a * 0.8f, cc.y }, ctx.theme.text);
            }
            if (!open) return false;

            // Popup auto-dimensionné, rabattu si débordement.
            const NkVec2 sz = MenuSizeGet(ctx, id);
            NkRect pr = { popupAt.x, popupAt.y, sz.x, sz.y };
            if (pr.x + pr.w > static_cast<float32>(ctx.viewW))
                pr.x = (inBar ? (titleR.x + titleR.w - pr.w) : (titleR.x - pr.w + 2.f));
            if (pr.x < 0.f) pr.x = 0.f;
            if (pr.y + pr.h > static_cast<float32>(ctx.viewH)) pr.y = static_cast<float32>(ctx.viewH) - pr.h;
            if (pr.y < 0.f) pr.y = 0.f;

            const bool o = BeginPopupLevel(ctx, id, level, pr, anchor);
            if (o) { ctx.menuMeasureId[level] = id; ctx.menuMeasureW[level] = 0.f; ctx.menuMeasureH[level] = 0.f; }
            return o;
        }

        void EndMenu(NkGuiContext& ctx) noexcept {
            const int32 L = ctx.curPopupLevel;
            if (L >= 0 && ctx.menuMeasureId[L] != NKGUI_ID_NONE) {
                const float32 w = (ctx.menuMeasureW[L] > 60.f ? ctx.menuMeasureW[L] : 60.f) + 26.f;
                MenuSizeSet(ctx, ctx.menuMeasureId[L], { w, ctx.menuMeasureH[L] + 10.f });
                ctx.menuMeasureId[L] = NKGUI_ID_NONE;
            }
            EndPopup(ctx);
        }

        bool MenuItem(NkGuiContext& ctx, const char* label, const char* shortcut, bool enabled) noexcept {
            const float32 h  = ctx.ItemHeight();
            const NkRect  r  = ctx.NextItemRect(0.f, h);
            const NkGuiId id = ctx.GetId(label);
            const int32   L  = ctx.curPopupLevel;

            // Mesure pour l'auto-dimensionnement du menu courant.
            if (L >= 0 && ctx.menuMeasureId[L] != NKGUI_ID_NONE) {
                const float32 lw = (ctx.font && ctx.font->Valid()) ? ctx.font->MeasureWidth(label) : 40.f;
                const float32 sw = (shortcut && ctx.font && ctx.font->Valid())
                                 ? ctx.font->MeasureWidth(shortcut) + 30.f : 12.f;
                if (lw + sw > ctx.menuMeasureW[L]) ctx.menuMeasureW[L] = lw + sw;
                ctx.menuMeasureH[L] += h + ctx.layout.itemSpacingY;
            }

            bool clicked = false, hov = false, held = false;
            if (enabled) {
                clicked = ctx.ButtonBehavior(id, r, NkGuiButtonFlags::None, -1.f, -1.f, &hov, &held);
                if (hov) {
                    ctx.DL().AddRectFilled(r, ctx.theme.selection, 3.f);
                    // Survol d'un item simple → referme tout sous-menu de ce niveau.
                    if (ctx.popupDepth > L + 1) ctx.popupDepth = L + 1;
                }
            }
            const NkColor lc = !enabled ? ctx.theme.textDisabled
                             : hov       ? NkColor{ 255, 255, 255, 255 } : ctx.theme.text;
            if (ctx.font && ctx.font->Valid()) {
                ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
                                 { r.x + 8.f, CenteredBaseline(ctx, r) }, label, lc);
                if (shortcut) {
                    const float32 sw = ctx.font->MeasureWidth(shortcut);
                    ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
                                     { r.x + r.w - sw - 10.f, CenteredBaseline(ctx, r) },
                                     shortcut, ctx.theme.textDisabled);
                }
            }
            if (clicked) ctx.ClosePopup();           // valider ferme toute la chaîne
            return clicked;
        }

        // Menu CONTEXTUEL : popup de niveau 0 ancré à ctx.popupPos (clic droit).
        // L'app l'ouvre via ctx.OpenPopupAt(ctx.GetId(idStr), mousePos).
        bool BeginPopupMenu(NkGuiContext& ctx, const char* idStr) noexcept {
            const NkGuiId id = ctx.GetId(idStr);
            if (!(ctx.popupDepth > 0 && ctx.popupStack[0] == id)) return false;
            const NkVec2 sz = MenuSizeGet(ctx, id);
            NkRect pr = { ctx.popupPos.x, ctx.popupPos.y, sz.x, sz.y };
            if (pr.x + pr.w > static_cast<float32>(ctx.viewW)) pr.x = static_cast<float32>(ctx.viewW) - pr.w;
            if (pr.y + pr.h > static_cast<float32>(ctx.viewH)) pr.y = static_cast<float32>(ctx.viewH) - pr.h;
            const NkRect anchor = { ctx.popupPos.x, ctx.popupPos.y, 1.f, 1.f };
            const bool o = BeginPopupLevel(ctx, id, 0, pr, anchor);
            if (o) { ctx.menuMeasureId[0] = id; ctx.menuMeasureW[0] = 0.f; ctx.menuMeasureH[0] = 0.f; }
            return o;
        }
        void EndPopupMenu(NkGuiContext& ctx) noexcept { EndMenu(ctx); }

        // ── Tooltip (infobulle au survol) ────────────────────────────────────
        // Dessine une bulle à la position souris, dans la couche OVERLAY (au-dessus
        // de TOUT, popups inclus). À appeler quand un widget est survolé, typiquement
        // `if (Button(...)) ...; if (ctx.IsItemHovered()) SetTooltip(ctx, "...");`.
        void SetTooltip(NkGuiContext& ctx, const char* text) noexcept {
            if (!text || !*text || !ctx.font || !ctx.font->Valid()) return;
            const float32 padX = 9.f, padY = 6.f;
            const float32 tw = ctx.font->MeasureWidth(text);
            const float32 th = ctx.font->LineHeight();
            const NkVec2  m  = ctx.input.mousePos;
            NkRect r = { m.x + 14.f, m.y + 18.f, tw + padX * 2.f, th + padY * 2.f };
            if (r.x + r.w > static_cast<float32>(ctx.viewW)) r.x = static_cast<float32>(ctx.viewW) - r.w;
            if (r.x < 0.f) r.x = 0.f;
            if (r.y + r.h > static_cast<float32>(ctx.viewH)) r.y = m.y - r.h - 6.f; // au-dessus si déborde
            if (r.y < 0.f) r.y = 0.f;

            // OVERLAY direct + clip plein écran (jamais rogné, bordure complète).
            ctx.dlOverlay.PushClipRect({ 0.f, 0.f, 1.0e9f, 1.0e9f }, false);
            ctx.dlOverlay.AddRectFilled(r, NkColor{ 24, 26, 32, 245 }, ctx.theme.rounding);
            ctx.dlOverlay.AddRect(r, ctx.theme.border, 1.f);
            ctx.dlOverlay.AddText(ctx.font->Face(), ctx.font->TexId(),
                                  { r.x + padX, CenteredBaseline(ctx, r) }, text, ctx.theme.text);
            ctx.dlOverlay.PopClipRect();
        }

    } // namespace nkgui
} // namespace nkentseu
