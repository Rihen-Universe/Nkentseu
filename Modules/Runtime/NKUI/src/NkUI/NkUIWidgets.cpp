// -----------------------------------------------------------------------------
// @File    NkUIWidgets.cpp
// @Brief   Implémentation de tous les widgets NkUI.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis
// @License Apache-2.0
// -----------------------------------------------------------------------------

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Widget behavior implementation and input/state handling.
 * Main data: Per-widget hit-test, state transitions, draw + interaction.
 * Change this file when: A specific widget misbehaves (focus, hover, click, value).
 */

#include "NKUI/NkUIWidgets.h"
#include "NKUI/NkUIWindow.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <cmath>

namespace nkentseu
{
    namespace nkui
    {
        // ---------------------------------------------------------------------
        // Helpers de label
        // ---------------------------------------------------------------------

        NkUILabelParts NkParseLabel(const char* raw) noexcept
        {
            NkUILabelParts p;
            if (!raw)
            {
                return p;
            }
            const char* sep = ::strstr(raw, "##");
            if (sep)
            {
                const int32 vlen = static_cast<int32>(sep - raw);
                ::strncpy(p.label, raw, vlen < 255 ? vlen : 255);
                ::strncpy(p.id, sep + 2, 127);
            }
            else
            {
                ::strncpy(p.label, raw, 255);
                ::strncpy(p.id, raw, 127);
            }
            return p;
        }

        NkUIID NkLabelToID(NkUIContext& ctx, const char* raw) noexcept
        {
            NkUILabelParts p = NkParseLabel(raw);
            return ctx.GetID(p.id[0] ? p.id : p.label);
        }

        static NkUIID ResolveLabelID(NkUIContext& ctx, NkUILayoutStack& ls, const NkUILabelParts& lp) noexcept
        {
            if (lp.id[0])
            {
                return ctx.GetID(lp.id);
            }

            NkUIID base = ctx.GetID(lp.label);
            if (const NkUILayoutNode* node = ls.Top())
            {
                const uint32 ordinal = static_cast<uint32>(node->itemCount);
                if (ordinal > 0u)
                {
                    base = NkHashInt(static_cast<int32>(ordinal), base);
                }
            }
            return base;
        }

        template <typename T, uint32 N>
        struct NkWidgetStateStore
        {
            NkUIID keys[N] = {};
            T values[N] = {};
            bool used[N] = {};
        };

        template <typename T, uint32 N>
        static T& AccessWidgetState(NkWidgetStateStore<T, N>& store, NkUIID id, const T& initValue = T{}) noexcept
        {
            const uint32 start = (N > 0u) ? (id % N) : 0u;
            for (uint32 i = 0; i < N; ++i)
            {
                const uint32 slot = (start + i) % N;
                if (store.used[slot])
                {
                    if (store.keys[slot] == id)
                    {
                        return store.values[slot];
                    }
                    continue;
                }
                store.used[slot] = true;
                store.keys[slot] = id;
                store.values[slot] = initValue;
                return store.values[slot];
            }
            // Table pleine : slot de repli déterministe
            const uint32 fallback = start;
            if (!store.used[fallback])
            {
                store.used[fallback] = true;
                store.keys[fallback] = id;
                store.values[fallback] = initValue;
            }
            return store.values[fallback];
        }

        static int32 NkEncodeUTF8(uint32 cp, char out[4]) noexcept
        {
            if (cp <= 0x7Fu)
            {
                out[0] = static_cast<char>(cp);
                return 1;
            }
            if (cp <= 0x7FFu)
            {
                out[0] = static_cast<char>(0xC0u | ((cp >> 6) & 0x1Fu));
                out[1] = static_cast<char>(0x80u | (cp & 0x3Fu));
                return 2;
            }
            if (cp <= 0xFFFFu)
            {
                out[0] = static_cast<char>(0xE0u | ((cp >> 12) & 0x0Fu));
                out[1] = static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
                out[2] = static_cast<char>(0x80u | (cp & 0x3Fu));
                return 3;
            }
            if (cp <= 0x10FFFFu)
            {
                out[0] = static_cast<char>(0xF0u | ((cp >> 18) & 0x07u));
                out[1] = static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu));
                out[2] = static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
                out[3] = static_cast<char>(0x80u | (cp & 0x3Fu));
                return 4;
            }
            return 0;
        }

        static void NkPopLastUTF8(char* buf) noexcept
        {
            if (!buf) return;
            int32 len = static_cast<int32>(::strlen(buf));
            if (len <= 0) return;
            int32 i = len - 1;
            while (i > 0 && (static_cast<uint8>(buf[i]) & 0xC0u) == 0x80u)
            {
                --i;
            }
            buf[i] = 0;
        }

        static bool NkAppendUTF8(char* buf, int32 bufSize, uint32 cp) noexcept
        {
            if (!buf || bufSize <= 1) return false;
            char encoded[4] = {};
            const int32 bytes = NkEncodeUTF8(cp, encoded);
            if (bytes <= 0) return false;
            const int32 len = static_cast<int32>(::strlen(buf));
            if (len + bytes >= bufSize) return false;
            for (int32 i = 0; i < bytes; ++i)
            {
                buf[len + i] = encoded[i];
            }
            buf[len + bytes] = 0;
            return true;
        }

        static int32 NkDecodeUTF8Codepoint(const char* s, uint32& cp) noexcept
        {
            if (!s || !s[0])
            {
                cp = 0;
                return 0;
            }
            const uint8 c0 = static_cast<uint8>(s[0]);
            if (c0 < 0x80u)
            {
                cp = c0;
                return 1;
            }
            if ((c0 & 0xE0u) == 0xC0u && s[1])
            {
                cp = ((c0 & 0x1Fu) << 6) | (static_cast<uint8>(s[1]) & 0x3Fu);
                return 2;
            }
            if ((c0 & 0xF0u) == 0xE0u && s[1] && s[2])
            {
                cp = ((c0 & 0x0Fu) << 12) |
                     ((static_cast<uint8>(s[1]) & 0x3Fu) << 6) |
                     (static_cast<uint8>(s[2]) & 0x3Fu);
                return 3;
            }
            if ((c0 & 0xF8u) == 0xF0u && s[1] && s[2] && s[3])
            {
                cp = ((c0 & 0x07u) << 18) |
                     ((static_cast<uint8>(s[1]) & 0x3Fu) << 12) |
                     ((static_cast<uint8>(s[2]) & 0x3Fu) << 6) |
                     (static_cast<uint8>(s[3]) & 0x3Fu);
                return 4;
            }
            cp = '?';
            return 1;
        }

        static float32 NkMeasureCodepointAdvance(const NkUIFont& font, uint32 cp) noexcept
        {
            if (cp == ' ')
            {
                return font.metrics.spaceWidth;
            }
            if (cp == '\t')
            {
                return font.metrics.tabWidth;
            }
            if (font.atlas && font.config.enableAtlas)
            {
                const NkUIGlyph* g = font.atlas->Find(cp);
                if (g)
                {
                    return g->advanceX;
                }
            }
            return font.size * 0.55f;
        }

        static float32 NkMeasureWrappedTextHeight(const NkUIFont& font,
                                                  const char* text,
                                                  float32 wrapWidth,
                                                  float32 lineSpacing = 1.f) noexcept
        {
            const float32 lineHBase = font.metrics.lineHeight > 0.f ? font.metrics.lineHeight : font.size;
            const float32 lineH = lineHBase * (lineSpacing > 0.f ? lineSpacing : 1.f);
            if (!text || !text[0])
            {
                return lineH;
            }

            if (wrapWidth < 1.f)
            {
                wrapWidth = 1.f;
            }

            float32 x = 0.f;
            int32 lines = 1;
            for (int32 i = 0; text[i];)
            {
                uint32 cp = '?';
                const int32 charLen = NkDecodeUTF8Codepoint(&text[i], cp);
                if (charLen <= 0)
                {
                    break;
                }
                i += charLen;

                if (cp == '\r') continue;
                if (cp == '\n')
                {
                    x = 0.f;
                    ++lines;
                    continue;
                }

                const float32 adv = NkMeasureCodepointAdvance(font, cp);
                if (x > 0.f && x + adv > wrapWidth)
                {
                    ++lines;
                    x = 0.f;
                    if (cp == ' ') continue;
                }
                x += adv;
            }

            return lineH * static_cast<float32>(lines);
        }

        // ---------------------------------------------------------------------
        // Structures pour la répétition des touches
        // ---------------------------------------------------------------------

        struct NkKeyRepeatTracker
        {
            bool    wasDown = false;
            float32 held    = 0.f;
            float32 next    = 0.f;
        };

        struct NkTextRepeatState
        {
            NkKeyRepeatTracker backspace;
            NkKeyRepeatTracker del;
            NkKeyRepeatTracker enter;
            NkKeyRepeatTracker left;
            NkKeyRepeatTracker right;
            NkKeyRepeatTracker up;
            NkKeyRepeatTracker down;
        };

        struct NkNumericInputState
        {
            bool editing = false;
            char buf[64] = {};
        };

        struct NkTextEditState
        {
            bool    initialized = false;
            int32   caretByte   = 0;
            float32 scrollX     = 0.f;
            float32 scrollY     = 0.f;
            float32 preferredX  = -1.f;
        };

        struct NkTextLineInfo
        {
            int32   start = 0;
            int32   end   = 0;
            float32 width = 0.f;
        };

        static int32 NkClampByteIndex(const char* buf, int32 index) noexcept
        {
            if (!buf) return 0;
            const int32 len = static_cast<int32>(::strlen(buf));
            if (index < 0) return 0;
            if (index > len) return len;
            return index;
        }

        static int32 NkFindPrevUTF8Index(const char* buf, int32 from) noexcept
        {
            if (!buf) return 0;
            const int32 len = static_cast<int32>(::strlen(buf));
            int32 i = from;
            if (i > len) i = len;
            if (i <= 0) return 0;
            --i;
            while (i > 0 && (static_cast<uint8>(buf[i]) & 0xC0u) == 0x80u) --i;
            return i;
        }

        static int32 NkFindNextUTF8Index(const char* buf, int32 from) noexcept
        {
            if (!buf) return 0;
            const int32 len = static_cast<int32>(::strlen(buf));
            int32 i = from;
            if (i < 0) i = 0;
            if (i >= len) return len;
            uint32 cp = 0;
            const int32 n = NkDecodeUTF8Codepoint(&buf[i], cp);
            if (n <= 0) return i + 1;
            i += n;
            if (i > len) i = len;
            return i;
        }

        static float32 NkGlyphAdvance(const NkUIFont& font, uint32 cp, float32 charSpacing) noexcept
        {
            float32 adv = NkMeasureCodepointAdvance(font, cp);
            if (charSpacing > 0.f && cp != '\n' && cp != '\r') adv += charSpacing;
            return adv;
        }

        static float32 NkMeasureUTF8Range(const NkUIFont& font,
                                          const char* text,
                                          int32 startByte,
                                          int32 endByte,
                                          float32 charSpacing) noexcept
        {
            if (!text) return 0.f;
            const int32 len = static_cast<int32>(::strlen(text));
            int32 i = startByte;
            if (i < 0) i = 0;
            if (i > len) i = len;
            int32 end = endByte;
            if (end < i) end = i;
            if (end > len) end = len;

            float32 width = 0.f;
            while (i < end && text[i])
            {
                uint32 cp = '?';
                const int32 n = NkDecodeUTF8Codepoint(&text[i], cp);
                if (n <= 0) break;
                if (cp == '\r' || cp == '\n') break;
                width += NkGlyphAdvance(font, cp, charSpacing);
                i += n;
            }
            return width;
        }

        static bool NkInsertUTF8At(char* buf, int32 bufSize, int32 byteIndex, uint32 cp, int32* outNewByte) noexcept
        {
            if (!buf || bufSize <= 1) return false;
            const int32 len = static_cast<int32>(::strlen(buf));
            int32 pos = byteIndex;
            if (pos < 0) pos = 0;
            if (pos > len) pos = len;

            char encoded[4] = {};
            const int32 n = NkEncodeUTF8(cp, encoded);
            if (n <= 0 || len + n >= bufSize) return false;

            ::memmove(buf + pos + n, buf + pos, static_cast<size_t>(len - pos + 1));
            ::memcpy(buf + pos, encoded, static_cast<size_t>(n));
            if (outNewByte) *outNewByte = pos + n;
            return true;
        }

        static bool NkInsertCharAt(char* buf, int32 bufSize, int32 byteIndex, char c, int32* outNewByte) noexcept
        {
            if (!buf || bufSize <= 1) return false;
            const int32 len = static_cast<int32>(::strlen(buf));
            int32 pos = byteIndex;
            if (pos < 0) pos = 0;
            if (pos > len) pos = len;
            if (len + 1 >= bufSize) return false;

            ::memmove(buf + pos + 1, buf + pos, static_cast<size_t>(len - pos + 1));
            buf[pos] = c;
            if (outNewByte) *outNewByte = pos + 1;
            return true;
        }

        static bool NkEraseUTF8Backspace(char* buf, int32 byteIndex, int32* outNewByte) noexcept
        {
            if (!buf) return false;
            const int32 len = static_cast<int32>(::strlen(buf));
            int32 pos = byteIndex;
            if (pos < 0) pos = 0;
            if (pos > len) pos = len;
            if (pos <= 0) return false;

            const int32 prev = NkFindPrevUTF8Index(buf, pos);
            ::memmove(buf + prev, buf + pos, static_cast<size_t>(len - pos + 1));
            if (outNewByte) *outNewByte = prev;
            return true;
        }

        static bool NkEraseUTF8Delete(char* buf, int32 byteIndex, int32* outNewByte) noexcept
        {
            if (!buf) return false;
            const int32 len = static_cast<int32>(::strlen(buf));
            int32 pos = byteIndex;
            if (pos < 0) pos = 0;
            if (pos > len) pos = len;
            if (pos >= len) return false;

            const int32 next = NkFindNextUTF8Index(buf, pos);
            ::memmove(buf + pos, buf + next, static_cast<size_t>(len - next + 1));
            if (outNewByte) *outNewByte = pos;
            return true;
        }

        static int32 NkFindCaretFromX(const NkUIFont& font,
                                      const char* text,
                                      int32 startByte,
                                      int32 endByte,
                                      float32 x,
                                      float32 charSpacing) noexcept
        {
            if (!text) return 0;
            if (x <= 0.f) return startByte;
            int32 i = startByte;
            int32 last = startByte;
            float32 cursorX = 0.f;
            while (i < endByte && text[i])
            {
                const int32 charStart = i;
                uint32 cp = '?';
                const int32 n = NkDecodeUTF8Codepoint(&text[i], cp);
                if (n <= 0) break;
                i += n;
                const float32 adv = NkGlyphAdvance(font, cp, charSpacing);
                if (x < cursorX + adv * 0.5f)
                {
                    return charStart;
                }
                cursorX += adv;
                last = i;
            }
            return last;
        }

        static int32 NkBuildTextLines(const NkUIFont& font,
                                      const char* text,
                                      bool wrap,
                                      float32 wrapWidth,
                                      float32 charSpacing,
                                      NkTextLineInfo* outLines,
                                      int32 maxLines,
                                      float32* outMaxWidth) noexcept
        {
            if (!outLines || maxLines <= 0) return 0;
            if (wrapWidth < 1.f) wrapWidth = 1.f;

            const int32 len = text ? static_cast<int32>(::strlen(text)) : 0;
            float32 maxW = 0.f;
            int32 lineCount = 0;
            int32 lineStart = 0;
            int32 i = 0;
            float32 x = 0.f;

            while (i < len)
            {
                uint32 cp = '?';
                const int32 n = NkDecodeUTF8Codepoint(&text[i], cp);
                if (n <= 0) break;

                if (cp == '\r')
                {
                    i += n;
                    continue;
                }

                if (cp == '\n')
                {
                    if (lineCount < maxLines)
                    {
                        outLines[lineCount++] = { lineStart, i, x };
                    }
                    if (x > maxW) maxW = x;
                    i += n;
                    lineStart = i;
                    x = 0.f;
                    continue;
                }

                const float32 adv = NkGlyphAdvance(font, cp, charSpacing);
                if (wrap && x > 0.f && (x + adv) > wrapWidth)
                {
                    if (lineCount < maxLines)
                    {
                        outLines[lineCount++] = { lineStart, i, x };
                    }
                    if (x > maxW) maxW = x;
                    lineStart = i;
                    x = 0.f;
                    continue;
                }

                x += adv;
                i += n;
            }

            if (lineCount < maxLines)
            {
                outLines[lineCount++] = { lineStart, len, x };
            }
            if (x > maxW) maxW = x;
            if (outMaxWidth) *outMaxWidth = maxW;
            return lineCount;
        }

        static int32 NkFindLineIndexForCaret(const NkTextLineInfo* lines, int32 lineCount, int32 caretByte) noexcept
        {
            if (!lines || lineCount <= 0) return 0;
            for (int32 i = 0; i < lineCount; ++i)
            {
                if (caretByte >= lines[i].start && caretByte <= lines[i].end) return i;
            }
            return lineCount - 1;
        }

        static NkVec2 NkCaretPixelFromLines(const NkUIFont& font,
                                            const char* text,
                                            const NkTextLineInfo* lines,
                                            int32 lineCount,
                                            int32 caretByte,
                                            float32 lineHeight,
                                            float32 charSpacing) noexcept
        {
            if (!lines || lineCount <= 0) return { 0.f, 0.f };
            const int32 line = NkFindLineIndexForCaret(lines, lineCount, caretByte);
            const int32 start = lines[line].start;
            const int32 end   = lines[line].end;
            int32 clamped = caretByte;
            if (clamped < start) clamped = start;
            if (clamped > end)   clamped = end;
            const float32 x = NkMeasureUTF8Range(font, text, start, clamped, charSpacing);
            const float32 y = static_cast<float32>(line) * lineHeight;
            return { x, y };
        }

        static NkUITextInputOptions ResolveTextInputOptions(const NkUITextInputOptions* opts) noexcept
        {
            NkUITextInputOptions o{};
            if (opts)
            {
                o = *opts;
            }
            if (o.repeatDelay < 0.f) o.repeatDelay = 0.f;
            if (o.repeatRate < 0.01f) o.repeatRate = 0.01f;
            if (o.multilineLineSpacing < 1.f) o.multilineLineSpacing = 1.f;
            if (o.multilineCharSpacing < 0.f) o.multilineCharSpacing = 0.f;
            return o;
        }

        static bool ConsumeRepeat(const NkUIContext& ctx,
                                  NkKey key,
                                  bool allowRepeat,
                                  float32 delay,
                                  float32 rate,
                                  NkKeyRepeatTracker& st) noexcept
        {
            const bool down = ctx.input.IsKeyDown(key);
            if (!down)
            {
                st = {};
                return false;
            }

            if (ctx.input.IsKeyPressed(key) || !st.wasDown)
            {
                st.wasDown = true;
                st.held = 0.f;
                st.next = delay;
                return true;
            }

            if (!allowRepeat)
            {
                return false;
            }

            st.held += ctx.dt;
            if (st.held >= st.next)
            {
                while (st.held >= st.next)
                {
                    st.next += rate;
                }
                return true;
            }
            return false;
        }

        static bool WindowAllowsInputForWidget(const NkUIContext& ctx, NkUIID windowId, NkUIID widgetId) noexcept
        {
            if (!ctx.wm)
            {
                return true;
            }
            if (windowId == NKUI_ID_NONE)
            {
                // Widget racine : ne pas capturer si une fenêtre flottante est sous le curseur
                if (ctx.wm->movingId != NKUI_ID_NONE || ctx.wm->resizingId != NKUI_ID_NONE)
                {
                    return false;
                }
                return ctx.wm->hoveredId == NKUI_ID_NONE;
            }
            if (ctx.activeId != NKUI_ID_NONE && ctx.activeId != widgetId && ctx.input.mouseDown[0])
            {
                return false;
            }

            if (ctx.wm->movingId != NKUI_ID_NONE)
            {
                return ctx.wm->movingId == windowId;
            }
            if (ctx.wm->resizingId != NKUI_ID_NONE)
            {
                return ctx.wm->resizingId == windowId;
            }
            if (ctx.wm->hoveredId != NKUI_ID_NONE && ctx.wm->hoveredId != windowId)
            {
                return (widgetId != NKUI_ID_NONE) && ctx.IsActive(widgetId);
            }
            return true;
        }

        static float32 TextBaselineY(const NkRect& r, const NkUIFont& f) noexcept
        {
            const float32 asc = (f.metrics.ascender > 0.f) ? f.metrics.ascender : (f.size * 0.8f);
            const float32 descRaw = (f.metrics.descender >= 0.f) ? f.metrics.descender : -f.metrics.descender;
            const float32 desc = (descRaw > 0.f) ? descRaw : (f.size * 0.2f);
            const float32 lineH = (f.metrics.lineHeight > 0.f) ? f.metrics.lineHeight : (asc + desc);
            const float32 top = r.y + (r.h - lineH) * 0.5f;
            return top + asc;
        }

        static float32 TextVisualBaselineY(const NkRect& r, const NkUIFont& f) noexcept
        {
            const float32 asc = (f.metrics.ascender > 0.f) ? f.metrics.ascender : (f.size * 0.8f);
            const float32 descRaw = (f.metrics.descender >= 0.f) ? f.metrics.descender : -f.metrics.descender;
            const float32 desc = (descRaw > 0.f) ? descRaw : (f.size * 0.2f);
            const float32 glyphH = asc + desc;
            const float32 top = r.y + (r.h - glyphH) * 0.5f;
            return top + asc;
        }

        static float32 TextRenderYCentered(const NkRect& r, const NkUIFont& f) noexcept
        {
            return TextBaselineY(r, f);
        }

        static float32 FontAscender(const NkUIFont& f) noexcept
        {
            return (f.metrics.ascender > 0.f) ? f.metrics.ascender : (f.size * 0.8f);
        }

        static NKUI_INLINE void ResetLayoutNode(NkUILayoutNode& node) noexcept
        {
            node = NkUILayoutNode{};
        }

        // ---------------------------------------------------------------------
        // CalcState – état d’un widget (hover, actif, focus)
        // ---------------------------------------------------------------------

        NkUIWidgetState NkUI::CalcState(NkUIContext& ctx, NkUIID id, NkRect r, bool enabled) noexcept
        {
            NkUIWidgetState s = NkUIWidgetState::NK_NORMAL;
            if (!enabled) return NkUIWidgetState::NK_DISABLED;

            const bool windowAllowsInput = WindowAllowsInputForWidget(ctx, ctx.currentWindowId, id);
            const bool activeOtherWidget = (ctx.activeId != NKUI_ID_NONE && ctx.activeId != id && ctx.input.mouseDown[0]);
            const bool canHover = windowAllowsInput && !activeOtherWidget;

            if (canHover && ctx.IsHovered(r))
            {
                ctx.SetHot(id);
                s = s | NkUIWidgetState::NK_HOVERED;
                if (ctx.IsMouseClicked(0) && (ctx.activeId == NKUI_ID_NONE || ctx.activeId == id))
                {
                    ctx.SetActive(id);
                }
            }
            if (ctx.IsActive(id)) s = s | NkUIWidgetState::NK_ACTIVE;
            if (ctx.IsFocused(id)) s = s | NkUIWidgetState::NK_FOCUSED;
            return s;
        }

        // ---------------------------------------------------------------------
        // DrawWidgetBg – fond et bordure standard
        // ---------------------------------------------------------------------

        void NkUI::DrawWidgetBg(NkUIDrawList& dl, const NkUITheme& t, NkRect r, NkUIWidgetType type, NkUIWidgetState state) noexcept
        {
            const auto& c = t.colors;
            const auto& m = t.metrics;
            const float32 rx = m.cornerRadius;
            NkColor bg, border;
            const bool hov = HasState(state, NkUIWidgetState::NK_HOVERED);
            const bool act = HasState(state, NkUIWidgetState::NK_ACTIVE);
            const bool dis = HasState(state, NkUIWidgetState::NK_DISABLED);
            const bool foc = HasState(state, NkUIWidgetState::NK_FOCUSED);

            switch (type)
            {
                case NkUIWidgetType::NK_BUTTON:
                    bg = dis ? c.accentDisabled : (act ? c.buttonActive : (hov ? c.buttonHover : c.buttonBg));
                    border = foc ? c.borderFocus : (hov ? c.borderHover : c.border);
                    break;
                case NkUIWidgetType::NK_INPUT_TEXT:
                case NkUIWidgetType::NK_INPUT_INT:
                case NkUIWidgetType::NK_INPUT_FLOAT:
                    bg = dis ? NkColor::Gray(240) : c.inputBg;
                    border = foc ? c.borderFocus : (hov ? c.borderHover : c.border);
                    break;
                case NkUIWidgetType::NK_CHECKBOX:
                case NkUIWidgetType::NK_RADIO:
                    bg = dis ? NkColor::Gray(240) : c.checkBg;
                    border = foc ? c.borderFocus : c.border;
                    break;
                default:
                    bg = c.bgSecondary;
                    border = c.border;
                    break;
            }
            dl.AddRectFilled(r, bg, rx);
            dl.AddRect(r, border, m.borderWidth, rx);
        }

        // ---------------------------------------------------------------------
        // DrawLabel – texte centré ou aligné à gauche avec padding
        // ---------------------------------------------------------------------

        void NkUI::DrawLabel(NkUIDrawList& dl, NkUIFont& f, const NkUITheme& t, NkRect r, const char* text, bool centered) noexcept
        {
            if (!text || !*text) return;
            const float32 tw = f.MeasureWidth(text);
            float32 tx;
            if (centered) tx = r.x + (r.w - tw) * 0.5f;
            else          tx = r.x + t.metrics.paddingX;
            const float32 ty = TextBaselineY(r, f);
            f.RenderText(dl, { tx, ty }, text, t.colors.textPrimary, r.w - t.metrics.paddingX * 2, true);
        }

        // ---------------------------------------------------------------------
        // Layout helpers
        // ---------------------------------------------------------------------

        void NkUI::BeginRow(NkUIContext& ctx, NkUILayoutStack& ls, float32 height) noexcept
        {
            NkUILayoutNode* parent = ls.Top();
            NkUILayoutNode* node = ls.Push();
            if (!node) return;
            ResetLayoutNode(*node);
            node->type = NkUILayoutType::NK_ROW;
            const float32 px = parent ? (parent->cursor.x - parent->scrollX) : ctx.cursor.x;
            const float32 py = parent ? (parent->cursor.y - parent->scrollY) : ctx.cursor.y;
            float32 pw = parent ? (parent->bounds.x + parent->bounds.w - px) : (static_cast<float32>(ctx.viewW) - px);
            if (pw < 1.f) pw = 1.f;
            node->bounds = { px, py, pw, height > 0 ? height : ctx.GetItemHeight() };
            node->cursor = { node->bounds.x, node->bounds.y };
            node->lineH = 0;
        }

        void NkUI::EndRow(NkUIContext& ctx, NkUILayoutStack& ls) noexcept
        {
            NkUILayoutNode* node = ls.Top();
            if (!node) return;
            const float32 h = node->lineH > 0 ? node->lineH : node->bounds.h;
            ls.Pop();
            NkUILayoutNode* parent = ls.Top();
            if (parent)
            {
                NkUILayout::AdvanceItem(ctx, ls, { node->bounds.x, node->bounds.y, node->bounds.w, h + ctx.theme.metrics.itemSpacing });
            }
            else
            {
                ctx.AdvanceCursor({ 0, h + ctx.theme.metrics.itemSpacing });
            }
        }

        void NkUI::BeginColumn(NkUIContext& ctx, NkUILayoutStack& ls, float32 width) noexcept
        {
            NkUILayoutNode* parent = ls.Top();
            NkUILayoutNode* node = ls.Push();
            if (!node) return;
            ResetLayoutNode(*node);
            node->type = NkUILayoutType::NK_COLUMN;
            const float32 px = parent ? (parent->cursor.x - parent->scrollX) : ctx.cursor.x;
            const float32 py = parent ? (parent->cursor.y - parent->scrollY) : ctx.cursor.y;
            float32 pw = parent ? (parent->bounds.x + parent->bounds.w - px) : (static_cast<float32>(ctx.viewW) - px);
            const float32 ph = parent ? parent->bounds.h : static_cast<float32>(ctx.viewH);
            if (pw < 1.f) pw = 1.f;
            node->bounds = { px, py, width > 0 ? width : pw, ph };
            node->cursor = { node->bounds.x, node->bounds.y };
        }

        void NkUI::EndColumn(NkUIContext& ctx, NkUILayoutStack& ls) noexcept
        {
            NkUILayoutNode* node = ls.Top();
            if (!node) return;
            const NkRect b = node->bounds;
            const float32 h = node->cursor.y - b.y;
            ls.Pop();
            NkUILayoutNode* parent = ls.Top();
            if (parent)
            {
                NkUILayout::AdvanceItem(ctx, ls, { b.x, b.y, b.w, h });
            }
            else
            {
                ctx.AdvanceCursor({ b.w, h });
            }
        }

        void NkUI::BeginGroup(NkUIContext& ctx, NkUILayoutStack& ls, const char* label, bool border) noexcept
        {
            NkUILayoutNode* parent = ls.Top();
            NkUILayoutNode* node = ls.Push();
            if (!node) return;
            ResetLayoutNode(*node);
            node->type = NkUILayoutType::NK_GROUP;
            const float32 px = parent ? (parent->cursor.x - parent->scrollX) : ctx.cursor.x;
            const float32 py = parent ? (parent->cursor.y - parent->scrollY) : ctx.cursor.y;
            float32 pw = parent ? (parent->bounds.x + parent->bounds.w - px) : (static_cast<float32>(ctx.viewW) - px);
            const float32 ph = parent ? parent->bounds.h : static_cast<float32>(ctx.viewH);
            const float32 pad = ctx.theme.metrics.windowPadX;
            if (pw < 1.f) pw = 1.f;
            node->bounds = { px, py, pw, ph };
            node->cursor = { px + pad, py + (label && *label ? ctx.theme.metrics.itemHeight : pad) };
            (void)border;
        }

        void NkUI::EndGroup(NkUIContext& ctx, NkUILayoutStack& ls) noexcept
        {
            NkUILayoutNode* node = ls.Top();
            if (!node) return;
            const NkRect b = node->bounds;
            const float32 h = node->cursor.y - b.y + ctx.theme.metrics.windowPadY;
            ls.Pop();
            if (NkUILayoutNode* parent = ls.Top())
            {
                NkUILayout::AdvanceItem(ctx, ls, { b.x, b.y, b.w, h });
            }
            else
            {
                ctx.AdvanceCursor({ b.w, h });
            }
        }

        bool NkUI::BeginGrid(NkUIContext& ctx, NkUILayoutStack& ls, int32 cols, float32 cellH) noexcept
        {
            NkUILayoutNode* parent = ls.Top();
            NkUILayoutNode* node = ls.Push();
            if (!node) return false;
            ResetLayoutNode(*node);
            node->type = NkUILayoutType::NK_GRID;
            const float32 px = parent ? (parent->cursor.x - parent->scrollX) : ctx.cursor.x;
            const float32 py = parent ? (parent->cursor.y - parent->scrollY) : ctx.cursor.y;
            float32 pw = parent ? (parent->bounds.x + parent->bounds.w - px) : (static_cast<float32>(ctx.viewW) - px);
            if (pw < 1.f) pw = 1.f;
            node->bounds = { px, py, pw, cellH > 0 ? cellH : ctx.GetItemHeight() };
            node->cursor = { px, py };
            node->gridCols = cols > 0 ? cols : 1;
            node->gridCol = 0;
            node->gridCellW = pw / node->gridCols;
            return true;
        }

        void NkUI::EndGrid(NkUIContext& ctx, NkUILayoutStack& ls) noexcept
        {
            EndColumn(ctx, ls);
        }

        bool NkUI::BeginScrollRegion(NkUIContext& ctx, NkUILayoutStack& ls,
                                     const char* id, NkRect rect,
                                     float32* scrollX, float32* scrollY) noexcept
        {
            NkUILayoutNode* node = ls.Push();
            if (!node) return false;
            ResetLayoutNode(*node);
            node->type = NkUILayoutType::NK_SCROLL;
            node->bounds = rect;
            node->id = ctx.GetID(id);
            node->scrollXPtr = scrollX;
            node->scrollYPtr = scrollY;
            node->scrollX = scrollX ? *scrollX : 0;
            node->scrollY = scrollY ? *scrollY : 0;
            node->cursor = { rect.x, rect.y };
            node->contentSize = {};
            ctx.dl->PushClipRect(rect);
            return true;
        }

        void NkUI::EndScrollRegion(NkUIContext& ctx, NkUILayoutStack& ls) noexcept
        {
            NkUILayoutNode* node = ls.Top();
            if (!node) return;
            ctx.dl->PopClipRect();
            const auto& m = ctx.theme.metrics;
            const bool canInteract = WindowAllowsInputForWidget(ctx, ctx.currentWindowId, node->id);

            if (canInteract && ctx.IsHovered(node->bounds) && !ctx.wheelConsumed && ctx.input.mouseWheel != 0.f)
            {
                node->scrollY -= ctx.input.mouseWheel * m.itemHeight * 3.f;
                ctx.wheelConsumed = true;
            }
            if (canInteract && ctx.IsHovered(node->bounds) && !ctx.wheelHConsumed && ctx.input.mouseWheelH != 0.f)
            {
                node->scrollX -= ctx.input.mouseWheelH * m.itemHeight * 3.f;
                ctx.wheelHConsumed = true;
            }

            const float32 maxY = node->contentSize.y > node->bounds.h ? (node->contentSize.y - node->bounds.h) : 0.f;
            const float32 maxX = node->contentSize.x > node->bounds.w ? (node->contentSize.x - node->bounds.w) : 0.f;
            if (node->scrollY < 0.f) node->scrollY = 0.f;
            if (node->scrollY > maxY) node->scrollY = maxY;
            if (node->scrollX < 0.f) node->scrollX = 0.f;
            if (node->scrollX > maxX) node->scrollX = maxX;

            // Barre verticale
            if (node->contentSize.y > node->bounds.h)
            {
                NkRect track = { node->bounds.x + node->bounds.w - m.scrollbarWidth,
                                 node->bounds.y,
                                 m.scrollbarWidth,
                                 node->bounds.h };
                NkUILayout::DrawScrollbar(ctx, *ctx.dl, true, track,
                                          node->contentSize.y, node->bounds.h,
                                          node->scrollY, NkHashInt(node->id, 1));
                if (node->scrollYPtr) *node->scrollYPtr = node->scrollY;
            }
            // Barre horizontale
            if (node->contentSize.x > node->bounds.w)
            {
                NkRect track = { node->bounds.x,
                                 node->bounds.y + node->bounds.h - m.scrollbarWidth,
                                 node->bounds.w,
                                 m.scrollbarWidth };
                NkUILayout::DrawScrollbar(ctx, *ctx.dl, false, track,
                                          node->contentSize.x, node->bounds.w,
                                          node->scrollX, NkHashInt(node->id, 2));
                if (node->scrollXPtr) *node->scrollXPtr = node->scrollX;
            }
            ls.Pop();
        }

        bool NkUI::BeginSplit(NkUIContext& ctx, NkUILayoutStack& ls,
                              const char* id, NkRect rect, bool vertical, float32* ratio) noexcept
        {
            NkUILayoutNode* node = ls.Push();
            if (!node) return false;
            ResetLayoutNode(*node);
            node->type = NkUILayoutType::NK_SPLIT;
            node->bounds = rect;
            node->id = ctx.GetID(id);
            node->splitVertical = vertical;
            node->splitRatio = ratio ? *ratio : 0.5f;
            node->splitRatioPtr = ratio;
            node->splitSecondPane = false;
            // Dessin du splitter et mise à jour du ratio
            NkUILayout::DrawSplitter(ctx, *ctx.dl, rect, vertical, node->splitRatio, node->id);
            if (ratio) *ratio = node->splitRatio;
            // Calcule les bounds du premier panneau
            node->cursor = { rect.x, rect.y };
            if (vertical)
                node->bounds = { rect.x, rect.y, rect.w * node->splitRatio - 2, rect.h };
            else
                node->bounds = { rect.x, rect.y, rect.w, rect.h * node->splitRatio - 2 };
            return true;
        }

        bool NkUI::BeginSplitPane2(NkUIContext& ctx, NkUILayoutStack& ls) noexcept
        {
            NkUILayoutNode* node = ls.Top();
            if (!node || node->type != NkUILayoutType::NK_SPLIT) return false;
            node->splitSecondPane = true;
            const NkRect& full = node->bounds;
            const float32 r = node->splitRatio;
            if (node->splitVertical)
            {
                node->bounds = { full.x + full.w + 4, full.y, full.w * (1 - r) - 2, full.h };
            }
            else
            {
                // simplification
            }
            node->cursor = { node->bounds.x, node->bounds.y };
            return true;
        }

        void NkUI::EndSplit(NkUIContext& ctx, NkUILayoutStack& ls) noexcept
        {
            if (NkUILayoutNode* node = ls.Top())
            {
                if (!node->splitSecondPane) return;
            }
            ls.Pop();
        }

        void NkUI::SetNextWidth(NkUIContext&, NkUILayoutStack& ls, float32 w) noexcept
        {
            if (NkUILayoutNode* n = ls.Top()) n->nextW = NkUIConstraint::Fixed_(w);
        }

        void NkUI::SetNextHeight(NkUIContext&, NkUILayoutStack& ls, float32 h) noexcept
        {
            if (NkUILayoutNode* n = ls.Top()) n->nextH = NkUIConstraint::Fixed_(h);
        }

        void NkUI::SetNextWidthPct(NkUIContext&, NkUILayoutStack& ls, float32 p) noexcept
        {
            if (NkUILayoutNode* n = ls.Top()) n->nextW = NkUIConstraint::Percent_(p);
        }

        void NkUI::SetNextGrow(NkUIContext&, NkUILayoutStack& ls) noexcept
        {
            if (NkUILayoutNode* n = ls.Top()) n->nextW = NkUIConstraint::Grow_();
        }

        void NkUI::SameLine(NkUIContext& ctx, NkUILayoutStack& ls, float32 offset, float32 spacing) noexcept
        {
            NkUILayoutNode* n = ls.Top();
            if (n)
            {
                n->cursor.y -= ctx.GetItemHeight() + ctx.theme.metrics.itemSpacing;
                n->cursor.x += offset > 0 ? offset : (spacing >= 0 ? spacing : ctx.theme.metrics.itemSpacing);
            }
            else
            {
                ctx.SameLine(offset, spacing);
            }
        }

        void NkUI::NewLine(NkUIContext& ctx, NkUILayoutStack& ls) noexcept
        {
            if (NkUILayoutNode* n = ls.Top())
            {
                n->cursor.x = n->bounds.x;
                n->cursor.y += ctx.GetItemHeight();
            }
            else
            {
                ctx.NewLine();
            }
        }

        void NkUI::Spacing(NkUIContext& ctx, NkUILayoutStack& ls, float32 px) noexcept
        {
            const float32 sp = px > 0 ? px : ctx.theme.metrics.sectionSpacing;
            if (NkUILayoutNode* n = ls.Top())
            {
                n->cursor.y += sp;
            }
            else
            {
                ctx.Spacing(sp);
            }
        }

        void NkUI::Separator(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl) noexcept
        {
            NkUILayoutNode* n = ls.Top();
            const float32 x = n ? (n->cursor.x - n->scrollX) : ctx.cursor.x;
            const float32 y = n ? (n->cursor.y - n->scrollY) : ctx.cursor.y;
            float32 w = n ? (n->bounds.x + n->bounds.w - x) : static_cast<float32>(ctx.viewW);
            if (w < 1.f) w = 1.f;
            dl.AddLine({ x, y + 3 }, { x + w, y + 3 }, ctx.theme.colors.separator, 1.f);
            if (n) n->cursor.y += 8;
            else ctx.Spacing(8);
        }

        // ---------------------------------------------------------------------
        // Texte
        // ---------------------------------------------------------------------

        void NkUI::Text(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font,
                        const char* text, NkColor col) noexcept
        {
            if (!text || !*text) return;
            const float32 tw = font.MeasureWidth(text);
            const NkRect r = NkUILayout::NextItemRect(ctx, ls, tw + ctx.GetPaddingX() * 2, ctx.GetItemHeight());
            const NkColor c = (col.a == 0) ? ctx.theme.colors.textPrimary : col;
            font.RenderText(dl, { r.x, TextBaselineY(r, font) }, text, c);
            NkUILayout::AdvanceItem(ctx, ls, r);
        }

        void NkUI::TextSmall(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* text) noexcept
        {
            Text(ctx, ls, dl, font, text, ctx.theme.colors.textSecondary);
        }

        void NkUI::TextColored(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, NkColor col, const char* text) noexcept
        {
            Text(ctx, ls, dl, font, text, col);
        }

        void NkUI::TextWrapped(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* text) noexcept
        {
            NkUILayoutNode* n = ls.Top();
            const float32 x = n ? (n->cursor.x - n->scrollX) : ctx.cursor.x;
            const float32 y = n ? (n->cursor.y - n->scrollY) : ctx.cursor.y;
            float32 w = n ? (n->bounds.x + n->bounds.w - x) : static_cast<float32>(ctx.viewW);
            if (w < 1.f) w = 1.f;
            const int32 lines = (int32)(font.MeasureWidth(text) / w) + 1;
            const float32 h = font.metrics.lineHeight * lines + ctx.GetPaddingY() * 2;
            const NkRect r = { x, y, w, h };
            font.RenderTextWrapped(dl, { r.x + ctx.GetPaddingX(), r.y + ctx.GetPaddingY(), r.w - ctx.GetPaddingX() * 2, r.h },
                                   text, ctx.theme.colors.textPrimary);
            NkUILayout::AdvanceItem(ctx, ls, r);
        }

        void NkUI::LabelValue(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font,
                              const char* label, const char* value) noexcept
        {
            BeginRow(ctx, ls);
            Text(ctx, ls, dl, font, label, ctx.theme.colors.textSecondary);
            Text(ctx, ls, dl, font, value);
            EndRow(ctx, ls);
        }

        // ---------------------------------------------------------------------
        // Button
        // ---------------------------------------------------------------------

        bool NkUI::Button(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font,
                          const char* label, NkVec2 size) noexcept
        {
            const NkUILabelParts lp = NkParseLabel(label);
            const NkUIID id = ResolveLabelID(ctx, ls, lp);
            const float32 tw = font.MeasureWidth(lp.label);
            const float32 ph = ctx.GetItemHeight();
            const float32 pw = size.x > 0 ? size.x : tw + ctx.GetPaddingX() * 2 + 4;
            const float32 ph2 = size.y > 0 ? size.y : ph;
            const NkRect r = NkUILayout::NextItemRect(ctx, ls, pw, ph2);
            const NkUIWidgetState state = CalcState(ctx, id, r);

            if (!ctx.CallShapeOverride(NkUIWidgetType::NK_BUTTON, r, state, lp.label))
            {
                DrawWidgetBg(dl, ctx.theme, r, NkUIWidgetType::NK_BUTTON, state);
                DrawLabel(dl, font, ctx.theme, r, lp.label, true);
            }
            NkUILayout::AdvanceItem(ctx, ls, r);
            ctx.lastId = id;

            // Animation de pression
            ctx.Animate(id, HasState(state, NkUIWidgetState::NK_HOVERED) ? 1.f : 0.f);

            // Clic : relâché et était actif
            if (HasState(state, NkUIWidgetState::NK_ACTIVE) && ctx.IsMouseReleased(0) && ctx.IsHovered(r))
            {
                ctx.ClearActive();
                return true;
            }
            return false;
        }

        bool NkUI::ButtonSmall(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* label) noexcept
        {
            ctx.PushStyleVar(NkStyleVar::NK_PADDING_X, 5.f);
            ctx.PushStyleVar(NkStyleVar::NK_PADDING_Y, 3.f);
            const bool r = Button(ctx, ls, dl, font, label);
            ctx.PopStyle(2);
            return r;
        }

        bool NkUI::ButtonImage(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl,
                               uint32 texId, NkVec2 size, NkColor tint) noexcept
        {
            const NkUIID id = ctx.GetID(static_cast<int32>(texId));
            const NkRect r = NkUILayout::NextItemRect(ctx, ls, size.x, size.y);
            const NkUIWidgetState state = CalcState(ctx, id, r);
            dl.AddImage(texId, r, { 0, 0 }, { 1, 1 }, tint);
            NkUILayout::AdvanceItem(ctx, ls, r);
            return HasState(state, NkUIWidgetState::NK_ACTIVE) && ctx.IsMouseReleased(0) && ctx.IsHovered(r);
        }

        bool NkUI::InvisibleButton(NkUIContext& ctx, NkUILayoutStack& ls, const char* label, NkVec2 size) noexcept
        {
            const NkUIID id = ctx.GetID(label);
            const NkRect r = NkUILayout::NextItemRect(ctx, ls, size.x, size.y);
            const NkUIWidgetState state = CalcState(ctx, id, r);
            NkUILayout::AdvanceItem(ctx, ls, r);
            return HasState(state, NkUIWidgetState::NK_ACTIVE) && ctx.IsMouseReleased(0) && ctx.IsHovered(r);
        }

        // ---------------------------------------------------------------------
        // Checkbox
        // ---------------------------------------------------------------------

        bool NkUI::Checkbox(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font,
                            const char* label, bool& value) noexcept
        {
            const NkUILabelParts lp = NkParseLabel(label);
            const NkUIID id = ResolveLabelID(ctx, ls, lp);
            const float32 sz = ctx.theme.metrics.checkboxSize;
            const float32 tw = font.MeasureWidth(lp.label);
            const NkRect r = NkUILayout::NextItemRect(ctx, ls, sz + ctx.GetPaddingX() + tw, ctx.GetItemHeight());

            const NkRect box = { r.x, r.y + (r.h - sz) * 0.5f, sz, sz };
            const NkUIWidgetState state = CalcState(ctx, id, r);
            bool changed = false;

            if (HasState(state, NkUIWidgetState::NK_ACTIVE) && ctx.IsMouseReleased(0) && ctx.IsHovered(r))
            {
                value = !value;
                changed = true;
                ctx.ClearActive();
            }

            if (!ctx.CallShapeOverride(NkUIWidgetType::NK_CHECKBOX, r,
                                       value ? state | NkUIWidgetState::NK_CHECKED : state,
                                       lp.label, (float32)value))
            {
                // Case à cocher
                dl.AddRectFilled(box, ctx.theme.colors.checkBg, ctx.theme.metrics.cornerRadiusSm);
                dl.AddRect(box, HasState(state, NkUIWidgetState::NK_FOCUSED) ?
                               ctx.theme.colors.borderFocus : ctx.theme.colors.border,
                           1.f, ctx.theme.metrics.cornerRadiusSm);
                if (value)
                {
                    const float32 anim = ctx.Animate(id, 1.f, 12.f);
                    if (anim > 0.1f)
                    {
                        NkColor mk = { ctx.theme.colors.checkMark.r, ctx.theme.colors.checkMark.g,
                                       ctx.theme.colors.checkMark.b, static_cast<uint8>(anim * 255) };
                        dl.AddCheckMark({ box.x + 2, box.y + 2 }, sz - 4, mk);
                    }
                }
                else
                {
                    ctx.Animate(id, 0.f, 12.f);
                }
                // Libellé
                font.RenderText(dl, { r.x + sz + ctx.GetPaddingX(), TextBaselineY(r, font) },
                                lp.label, ctx.theme.colors.textPrimary);
            }
            NkUILayout::AdvanceItem(ctx, ls, r);
            return changed;
        }

        // ---------------------------------------------------------------------
        // RadioButton
        // ---------------------------------------------------------------------

        bool NkUI::RadioButton(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font,
                               const char* label, int32& selected, int32 value) noexcept
        {
            const NkUILabelParts lp = NkParseLabel(label);
            const NkUIID id = NkHashInt(value, ResolveLabelID(ctx, ls, lp));
            const float32 sz = ctx.theme.metrics.radioSize;
            const float32 tw = font.MeasureWidth(lp.label);
            const NkRect r = NkUILayout::NextItemRect(ctx, ls, sz + ctx.GetPaddingX() + tw, ctx.GetItemHeight());
            const NkVec2 center = { r.x + sz * 0.5f, r.y + r.h * 0.5f };
            const NkUIWidgetState state = CalcState(ctx, id, r);
            bool changed = false;

            if (HasState(state, NkUIWidgetState::NK_ACTIVE) && ctx.IsMouseReleased(0) && ctx.IsHovered(r))
            {
                selected = value;
                changed = true;
                ctx.ClearActive();
            }

            dl.AddCircleFilled(center, sz * 0.5f, ctx.theme.colors.checkBg);
            dl.AddCircle(center, sz * 0.5f, ctx.theme.colors.border, 1.f);
            if (selected == value)
            {
                const float32 anim = ctx.Animate(id, 1.f, 12.f);
                dl.AddCircleFilled(center, sz * 0.3f * anim, ctx.theme.colors.checkMark);
            }
            else
            {
                ctx.Animate(id, 0.f, 12.f);
            }
            font.RenderText(dl, { r.x + sz + ctx.GetPaddingX(), TextBaselineY(r, font) },
                            lp.label, ctx.theme.colors.textPrimary);
            NkUILayout::AdvanceItem(ctx, ls, r);
            return changed;
        }

        // ---------------------------------------------------------------------
        // Toggle
        // ---------------------------------------------------------------------

        bool NkUI::Toggle(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font,
                          const char* label, bool& value) noexcept
        {
            const NkUILabelParts lp = NkParseLabel(label);
            const NkUIID id = ResolveLabelID(ctx, ls, lp);
            const float32 tw2 = ctx.GetItemHeight() * 1.8f;
            const float32 th = ctx.GetItemHeight() * 0.6f;
            const float32 textW = font.MeasureWidth(lp.label);
            const NkRect r = NkUILayout::NextItemRect(ctx, ls, tw2 + ctx.GetPaddingX() + textW, ctx.GetItemHeight());
            const NkRect track = { r.x, r.y + (r.h - th) * 0.5f, tw2, th };
            const NkUIWidgetState state = CalcState(ctx, id, r);
            bool changed = false;

            if (HasState(state, NkUIWidgetState::NK_ACTIVE) && ctx.IsMouseReleased(0) && ctx.IsHovered(r))
            {
                value = !value;
                changed = true;
                ctx.ClearActive();
            }

            const float32 anim = ctx.Animate(id, value ? 1.f : 0.f, 10.f);
            const NkColor trackCol = ctx.theme.colors.scrollBg.Lerp(ctx.theme.colors.checkMark, anim);
            const float32 thumbX = track.x + th * 0.1f + (track.w - th * 1.2f) * anim;
            const NkRect thumb = { thumbX, track.y + th * 0.1f, th * 0.8f, th * 0.8f };

            dl.AddRectFilled(track, trackCol, track.h * 0.5f);
            dl.AddCircleFilled({ thumb.x + thumb.w * 0.5f, thumb.y + thumb.h * 0.5f },
                               thumb.w * 0.5f, NkColor::White());
            font.RenderText(dl, { r.x + tw2 + ctx.GetPaddingX(), TextBaselineY(r, font) },
                            lp.label, ctx.theme.colors.textPrimary);
            NkUILayout::AdvanceItem(ctx, ls, r);
            return changed;
        }

        // ---------------------------------------------------------------------
        // SliderFloat
        // ---------------------------------------------------------------------

        bool NkUI::SliderFloat(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font,
                               const char* label, float32& value,
                               float32 vmin, float32 vmax,
                               const char* fmt, float32 width,
                               const NkUISliderValueOptions* valueOpts) noexcept
        {
            const NkUILabelParts lp = NkParseLabel(label);
            const NkUIID id = ResolveLabelID(ctx, ls, lp);
            NkUISliderValueOptions opts{};
            if (valueOpts) opts = *valueOpts;

            char valBuf[32];
            ::snprintf(valBuf, sizeof(valBuf), fmt ? fmt : "%.2f", value);
            const float32 valW = font.MeasureWidth(valBuf);
            const float32 valueExtra = valW + opts.gap + 4.f;

            float32 labelW = 0.f;
            if (lp.label[0])
            {
                labelW = font.MeasureWidth(lp.label) + ctx.GetPaddingX();
                if (opts.placement == NkUISliderValuePlacement::NK_IN_LABEL)
                {
                    labelW += valW + 6.f;
                }
            }

            float32 reservedLeft = 0.f, reservedRight = 0.f;
            if (opts.placement == NkUISliderValuePlacement::NK_LEFT_OF_SLIDER)  reservedLeft  = valueExtra;
            if (opts.placement == NkUISliderValuePlacement::NK_RIGHT_OF_SLIDER) reservedRight = valueExtra;

            const float32 asc = FontAscender(font);
            const float32 descRaw = (font.metrics.descender >= 0.f) ? font.metrics.descender : -font.metrics.descender;
            const float32 desc = (descRaw > 0.f) ? descRaw : (font.size * 0.2f);
            const float32 lineH = (font.metrics.lineHeight > 0.f) ? font.metrics.lineHeight : (asc + desc);

            float32 valuePadTop = 0.f, valuePadBottom = 0.f;
            if (opts.placement == NkUISliderValuePlacement::NK_ABOVE_THUMB) valuePadTop    = lineH + opts.gap;
            if (opts.placement == NkUISliderValuePlacement::NK_BELOW_THUMB) valuePadBottom = lineH + opts.gap;

            float32 totalW = width > 0 ? width : (200.f + labelW + reservedLeft + reservedRight);
            if (totalW < 40.f) totalW = 40.f;
            const float32 sliderH = ctx.GetItemHeight();
            const NkRect r = NkUILayout::NextItemRect(ctx, ls, totalW, sliderH + valuePadTop + valuePadBottom);

            NkRect sliderR = {
                r.x + reservedLeft,
                r.y + valuePadTop,
                r.w - labelW - reservedLeft - reservedRight,
                sliderH
            };
            if (sliderR.w < 10.f) sliderR.w = 10.f;

            const NkUIWidgetState state = CalcState(ctx, id, sliderR);
            bool changed = false;

            // Interaction
            if (HasState(state, NkUIWidgetState::NK_ACTIVE))
            {
                const float32 t = (ctx.input.mousePos.x - sliderR.x) / sliderR.w;
                const float32 newVal = vmin + (vmax - vmin) * ::fmaxf(0, ::fminf(1, t));
                if (newVal != value)
                {
                    value = newVal;
                    changed = true;
                }
                if (!ctx.input.mouseDown[0]) ctx.ClearActive();
            }

            const float32 t = (vmax > vmin) ? (value - vmin) / (vmax - vmin) : 0.f;

            if (!ctx.CallShapeOverride(NkUIWidgetType::NK_SLIDER_FLOAT, r, state, lp.label, t))
            {
                // Track
                const float32 th = ctx.theme.metrics.sliderTrackH;
                const NkRect track = { sliderR.x, sliderR.y + (sliderR.h - th) * 0.5f, sliderR.w, th };
                dl.AddRectFilled(track, ctx.theme.colors.sliderTrack, th * 0.5f);
                // Fill
                const NkRect fill = { track.x, track.y, track.w * t, track.h };
                if (fill.w > 0) dl.AddRectFilled(fill, ctx.theme.colors.sliderThumb, th * 0.5f);
                // Thumb
                const float32 tw2 = ctx.theme.metrics.sliderThumbW;
                const NkVec2 thumbC = { sliderR.x + sliderR.w * t, sliderR.y + sliderR.h * 0.5f };
                const NkColor thumbC2 = (HasState(state, NkUIWidgetState::NK_ACTIVE) || HasState(state, NkUIWidgetState::NK_HOVERED))
                                        ? ctx.theme.colors.accentHover : ctx.theme.colors.sliderThumb;
                dl.AddCircleFilled(thumbC, tw2 * 0.5f, thumbC2);
                dl.AddCircle(thumbC, tw2 * 0.5f, ctx.theme.colors.border, 1.f);
                if (HasState(state, NkUIWidgetState::NK_ACTIVE) && ::fabsf(ctx.input.mouseDelta.x) > 0.01f)
                {
                    const int32 dir = (ctx.input.mouseDelta.x >= 0.f) ? 0 : 2;
                    dl.AddArrow({ thumbC.x, thumbC.y - tw2 * 0.9f }, 7.f, dir, ctx.theme.colors.textSecondary);
                }

                // Valeur
                if (opts.placement != NkUISliderValuePlacement::NK_HIDDEN &&
                    opts.placement != NkUISliderValuePlacement::NK_IN_LABEL)
                {
                    float32 tx = 0.f, ty = TextBaselineY(sliderR, font);
                    switch (opts.placement)
                    {
                        case NkUISliderValuePlacement::NK_ABOVE_THUMB:
                        {
                            const float32 ax = opts.followThumb ? thumbC.x : (sliderR.x + sliderR.w * 0.5f);
                            tx = ax - valW * 0.5f;
                            ty = sliderR.y - opts.gap;
                        } break;
                        case NkUISliderValuePlacement::NK_BELOW_THUMB:
                        {
                            const float32 ax = opts.followThumb ? thumbC.x : (sliderR.x + sliderR.w * 0.5f);
                            tx = ax - valW * 0.5f;
                            ty = sliderR.y + sliderR.h + opts.gap + asc;
                        } break;
                        case NkUISliderValuePlacement::NK_LEFT_OF_SLIDER:
                        {
                            tx = sliderR.x - opts.gap - 4.f - valW;
                            ty = TextBaselineY(sliderR, font);
                        } break;
                        case NkUISliderValuePlacement::NK_RIGHT_OF_SLIDER:
                        {
                            tx = sliderR.x + sliderR.w + opts.gap + 4.f;
                            ty = TextBaselineY(sliderR, font);
                        } break;
                        default: break;
                    }
                    tx += opts.offset.x;
                    ty += opts.offset.y;
                    font.RenderText(dl, { tx, ty }, valBuf, ctx.theme.colors.textSecondary);
                }

                // Label
                if (lp.label[0])
                {
                    const float32 labelX = r.x + r.w - labelW + ctx.GetPaddingX() * 0.5f;
                    const float32 labelY = TextBaselineY(sliderR, font);
                    if (opts.placement == NkUISliderValuePlacement::NK_IN_LABEL)
                    {
                        char labelValue[320];
                        ::snprintf(labelValue, sizeof(labelValue), "%s: %s", lp.label, valBuf);
                        font.RenderText(dl, { labelX, labelY }, labelValue, ctx.theme.colors.textPrimary);
                    }
                    else
                    {
                        font.RenderText(dl, { labelX, labelY }, lp.label, ctx.theme.colors.textPrimary);
                    }
                }
            }
            NkUILayout::AdvanceItem(ctx, ls, r);
            return changed;
        }

        bool NkUI::SliderInt(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font,
                             const char* label, int32& value, int32 vmin, int32 vmax, float32 width,
                             const NkUISliderValueOptions* valueOpts) noexcept
        {
            float32 fv = static_cast<float32>(value);
            const bool r = SliderFloat(ctx, ls, dl, font, label, fv,
                                       static_cast<float32>(vmin), static_cast<float32>(vmax),
                                       "%.0f", width, valueOpts);
            value = static_cast<int32>(fv + 0.5f);
            return r;
        }

        bool NkUI::SliderFloat2(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font,
                                const char* label, float32* values, float32 vmin, float32 vmax, float32 width,
                                const NkUISliderValueOptions* valueOpts) noexcept
        {
            bool changed = false;
            char buf[128];
            ::snprintf(buf, sizeof(buf), "%s##0", label);
            SetNextWidth(ctx, ls, width > 0 ? width * 0.5f : 0);
            changed |= SliderFloat(ctx, ls, dl, font, buf, values[0], vmin, vmax, "%.2f", width > 0 ? width * 0.5f : 0, valueOpts);
            SameLine(ctx, ls, 0, 4.f);
            ::snprintf(buf, sizeof(buf), "##%s1", label);
            SetNextWidth(ctx, ls, width > 0 ? width * 0.5f : 0);
            changed |= SliderFloat(ctx, ls, dl, font, buf, values[1], vmin, vmax, "%.2f", width > 0 ? width * 0.5f : 0, valueOpts);
            return changed;
        }

        bool NkUI::DragFloat(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font,
                             const char* label, float32& value,
                             float32 speed, float32 vmin, float32 vmax) noexcept
        {
            const NkUILabelParts lp = NkParseLabel(label);
            const NkUIID id = ResolveLabelID(ctx, ls, lp);
            const float32 w = 100.f + font.MeasureWidth(lp.label) + ctx.GetPaddingX();
            const NkRect r = NkUILayout::NextItemRect(ctx, ls, w, ctx.GetItemHeight());
            const NkUIWidgetState state = CalcState(ctx, id, r);
            bool changed = false;
            if (HasState(state, NkUIWidgetState::NK_ACTIVE))
            {
                value += ctx.input.mouseDelta.x * speed;
                if (value < vmin) value = vmin;
                if (value > vmax) value = vmax;
                changed = (ctx.input.mouseDelta.x != 0);
                if (!ctx.input.mouseDown[0]) ctx.ClearActive();
            }
            DrawWidgetBg(dl, ctx.theme, r, NkUIWidgetType::NK_INPUT_FLOAT, state);
            char buf[32];
            ::snprintf(buf, sizeof(buf), "%.3f", value);
            DrawLabel(dl, font, ctx.theme, r, buf, true);
            NkUILayout::AdvanceItem(ctx, ls, r);
            return changed;
        }

        bool NkUI::DragInt(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font,
                           const char* label, int32& value, float32 speed, int32 vmin, int32 vmax) noexcept
        {
            float32 fv = static_cast<float32>(value);
            const bool r = DragFloat(ctx, ls, dl, font, label, fv, speed,
                                     static_cast<float32>(vmin), static_cast<float32>(vmax));
            value = static_cast<int32>(fv + 0.5f);
            if (value < vmin) value = vmin;
            if (value > vmax) value = vmax;
            return r;
        }

        // ---------------------------------------------------------------------
        // InputText
        // ---------------------------------------------------------------------

        bool NkUI::InputText(NkUIContext& ctx, NkUILayoutStack& ls,
                             NkUIDrawList& dl, NkUIFont& font,
                             const char* label, char* buf, int32 bufSize, float32 width,
                             const NkUITextInputOptions* optsPtr) noexcept
        {
            if (!buf || bufSize <= 0) return false;

            const NkUITextInputOptions opts = ResolveTextInputOptions(optsPtr);
            const NkUILabelParts lp = NkParseLabel(label);
            const NkUIID id = ResolveLabelID(ctx, ls, lp);
            const float32 labelW = lp.label[0] ? font.MeasureWidth(lp.label) + ctx.GetPaddingX() * 2.f : 0.f;
            const float32 inputW = width > 0 ? width : 200.f;
            const float32 totalW = inputW + labelW;
            const NkRect r = NkUILayout::NextItemRect(ctx, ls, totalW, ctx.GetItemHeight());
            const NkRect inputR = { r.x, r.y, inputW, r.h };
            const NkUIWidgetState state = CalcState(ctx, id, inputR);
            const bool canInteract = WindowAllowsInputForWidget(ctx, ctx.currentWindowId, id);

            static NkWidgetStateStore<NkTextEditState, 2048> sEditStore;
            NkTextEditState& edit = AccessWidgetState(sEditStore, id, NkTextEditState{});

            int32 len = static_cast<int32>(::strlen(buf));
            if (!edit.initialized)
            {
                edit.initialized = true;
                edit.caretByte = len;
            }
            edit.caretByte = NkClampByteIndex(buf, edit.caretByte);

            const float32 padX = ctx.GetPaddingX();
            NkRect textRect = { inputR.x + padX, inputR.y, inputR.w - padX * 2.f, inputR.h };
            if (textRect.w < 1.f) textRect.w = 1.f;

            if (canInteract && ctx.IsHovered(inputR) && ctx.ConsumeMouseClick(0))
            {
                ctx.SetFocus(id);
                ctx.SetActive(id);
                const float32 localX = ctx.input.mousePos.x - textRect.x + edit.scrollX;
                edit.caretByte = NkFindCaretFromX(font, buf, 0, len, localX, 0.f);
                edit.preferredX = -1.f;
            }
            if (ctx.IsActive(id) && !ctx.input.mouseDown[0]) ctx.ClearActive();

            bool changed = false;
            static NkWidgetStateStore<NkTextRepeatState, 2048> sRepeatStore;
            NkTextRepeatState& rep = AccessWidgetState(sRepeatStore, id, NkTextRepeatState{});

            if (ctx.IsFocused(id))
            {
                if (ctx.input.IsKeyPressed(NkKey::NK_ESCAPE) || ctx.input.IsKeyPressed(NkKey::NK_TAB))
                {
                    ctx.SetFocus(NKUI_ID_NONE);
                }
                else
                {
                    const bool moveLeft  = ConsumeRepeat(ctx, NkKey::NK_LEFT,  true, opts.repeatDelay, opts.repeatRate, rep.left);
                    const bool moveRight = ConsumeRepeat(ctx, NkKey::NK_RIGHT, true, opts.repeatDelay, opts.repeatRate, rep.right);
                    if (moveLeft)
                    {
                        edit.caretByte = NkFindPrevUTF8Index(buf, edit.caretByte);
                        edit.preferredX = -1.f;
                    }
                    if (moveRight)
                    {
                        edit.caretByte = NkFindNextUTF8Index(buf, edit.caretByte);
                        edit.preferredX = -1.f;
                    }
                    if (ctx.input.IsKeyPressed(NkKey::NK_HOME))
                    {
                        edit.caretByte = 0;
                        edit.preferredX = -1.f;
                    }
                    if (ctx.input.IsKeyPressed(NkKey::NK_END))
                    {
                        edit.caretByte = static_cast<int32>(::strlen(buf));
                        edit.preferredX = -1.f;
                    }

                    const bool eraseBack = ConsumeRepeat(ctx, NkKey::NK_BACK, opts.repeatBackspace,
                                                         opts.repeatDelay, opts.repeatRate, rep.backspace);
                    const bool eraseDel  = ConsumeRepeat(ctx, NkKey::NK_DELETE, opts.repeatDelete,
                                                         opts.repeatDelay, opts.repeatRate, rep.del);
                    if (eraseBack)
                    {
                        int32 newCaret = edit.caretByte;
                        if (NkEraseUTF8Backspace(buf, edit.caretByte, &newCaret))
                        {
                            edit.caretByte = newCaret;
                            changed = true;
                        }
                    }
                    if (eraseDel)
                    {
                        int32 newCaret = edit.caretByte;
                        if (NkEraseUTF8Delete(buf, edit.caretByte, &newCaret))
                        {
                            edit.caretByte = newCaret;
                            changed = true;
                        }
                    }

                    if (opts.allowTextInput)
                    {
                        for (int32 i = 0; i < ctx.input.numInputChars; ++i)
                        {
                            const uint32 c = ctx.input.inputChars[i];
                            if (c == '\r' || c == '\n') continue;
                            if (c == '\b' || c == 127u) continue;
                            if (c >= 32u)
                            {
                                int32 newCaret = edit.caretByte;
                                if (NkInsertUTF8At(buf, bufSize, edit.caretByte, c, &newCaret))
                                {
                                    edit.caretByte = newCaret;
                                    changed = true;
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                rep = {};
            }

            len = static_cast<int32>(::strlen(buf));
            edit.caretByte = NkClampByteIndex(buf, edit.caretByte);

            const float32 caretX = NkMeasureUTF8Range(font, buf, 0, edit.caretByte, 0.f);
            const float32 textW = NkMeasureUTF8Range(font, buf, 0, len, 0.f);
            const float32 visibleW = textRect.w;
            if (ctx.IsFocused(id))
            {
                if (caretX < edit.scrollX) edit.scrollX = caretX;
                const float32 rightLimit = edit.scrollX + visibleW - 2.f;
                if (caretX > rightLimit) edit.scrollX = caretX - (visibleW - 2.f);
            }
            if (edit.scrollX < 0.f) edit.scrollX = 0.f;
            const float32 maxScrollX = (textW > visibleW) ? (textW - visibleW + 2.f) : 0.f;
            if (edit.scrollX > maxScrollX) edit.scrollX = maxScrollX;

            const NkUIWidgetState drawState = ctx.IsFocused(id) ? (state | NkUIWidgetState::NK_FOCUSED) : state;
            DrawWidgetBg(dl, ctx.theme, inputR, NkUIWidgetType::NK_INPUT_TEXT, drawState);

            const char* displayText = (buf && buf[0]) ? buf : "";
            const bool hasText = displayText[0] != 0;
            const NkColor textCol = hasText ? ctx.theme.colors.inputText : ctx.theme.colors.inputPlaceholder;

            dl.PushClipRect(textRect);
            if (hasText)
            {
                font.RenderText(dl,
                                { textRect.x - edit.scrollX, TextBaselineY(inputR, font) },
                                displayText, textCol, -1.f, false);
            }
            if (ctx.IsFocused(id) && static_cast<int32>(ctx.time * 2.f) % 2 == 0)
            {
                const float32 cx = textRect.x - edit.scrollX + caretX;
                const float32 asc = (font.metrics.ascender > 0.f) ? font.metrics.ascender : (font.size * 0.8f);
                const float32 descRaw = (font.metrics.descender >= 0.f) ? font.metrics.descender : -font.metrics.descender;
                const float32 desc = (descRaw > 0.f) ? descRaw : (font.size * 0.2f);
                const float32 glyphH = asc + desc;
                const float32 cy = inputR.y + (inputR.h - glyphH) * 0.5f;
                dl.AddLine({ cx, cy }, { cx, cy + glyphH }, ctx.theme.colors.inputCursor, 1.5f);
            }
            dl.PopClipRect();

            if (lp.label[0])
            {
                font.RenderText(dl,
                                { r.x + inputW + ctx.GetPaddingX() * 0.5f, TextBaselineY(r, font) },
                                lp.label, ctx.theme.colors.textPrimary);
            }
            NkUILayout::AdvanceItem(ctx, ls, r);
            return changed;
        }

        bool NkUI::InputInt(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font,
                            const char* label, int32& value, const NkUITextInputOptions* opts) noexcept
        {
            const NkUILabelParts lp = NkParseLabel(label);
            const NkUIID id = ResolveLabelID(ctx, ls, lp);
            static NkWidgetStateStore<NkNumericInputState, 2048> sInputStore;
            NkNumericInputState& st = AccessWidgetState(sInputStore, id, NkNumericInputState{});

            if (!st.editing || !ctx.IsFocused(id))
            {
                ::snprintf(st.buf, sizeof(st.buf), "%d", value);
            }

            const bool textChanged = InputText(ctx, ls, dl, font, label, st.buf, static_cast<int32>(sizeof(st.buf)), 0.f, opts);
            if (ctx.IsFocused(id)) st.editing = true;
            else                   st.editing = false;

            if (textChanged)
            {
                char* end = nullptr;
                const long parsed = ::strtol(st.buf, &end, 10);
                if (end && end != st.buf)
                {
                    value = static_cast<int32>(parsed);
                    return true;
                }
            }
            return false;
        }

        bool NkUI::InputFloat(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font,
                              const char* label, float32& value, const char* fmt,
                              const NkUITextInputOptions* opts) noexcept
        {
            const NkUILabelParts lp = NkParseLabel(label);
            const NkUIID id = ResolveLabelID(ctx, ls, lp);
            static NkWidgetStateStore<NkNumericInputState, 2048> sInputStore;
            NkNumericInputState& st = AccessWidgetState(sInputStore, id, NkNumericInputState{});

            if (!st.editing || !ctx.IsFocused(id))
            {
                ::snprintf(st.buf, sizeof(st.buf), fmt ? fmt : "%.3f", value);
            }

            const bool textChanged = InputText(ctx, ls, dl, font, label, st.buf, static_cast<int32>(sizeof(st.buf)), 0.f, opts);
            if (ctx.IsFocused(id)) st.editing = true;
            else                   st.editing = false;

            if (textChanged)
            {
                char* end = nullptr;
                const float32 parsed = ::strtof(st.buf, &end);
                if (end && end != st.buf)
                {
                    value = parsed;
                    return true;
                }
            }
            return false;
        }

        bool NkUI::InputMultiline(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font,
                                  const char* label, char* buf, int32 bufSize, NkVec2 size,
                                  const NkUITextInputOptions* optsPtr) noexcept
        {
            if (!buf || bufSize <= 0) return false;

            const NkUITextInputOptions opts = ResolveTextInputOptions(optsPtr);
            const NkUILabelParts lp = NkParseLabel(label);
            const NkUIID id = ResolveLabelID(ctx, ls, lp);
            const float32 w = size.x > 0 ? size.x : 200.f;
            const float32 h = size.y > 0 ? size.y : 80.f;
            const NkRect r = NkUILayout::NextItemRect(ctx, ls, w, h);
            const NkUIWidgetState state = CalcState(ctx, id, r);
            const bool canInteract = WindowAllowsInputForWidget(ctx, ctx.currentWindowId, id);

            static NkWidgetStateStore<NkTextEditState, 2048> sEditStore;
            NkTextEditState& edit = AccessWidgetState(sEditStore, id, NkTextEditState{});
            static NkWidgetStateStore<NkTextRepeatState, 2048> sRepeatStore;
            NkTextRepeatState& rep = AccessWidgetState(sRepeatStore, id, NkTextRepeatState{});

            int32 len = static_cast<int32>(::strlen(buf));
            if (!edit.initialized)
            {
                edit.initialized = true;
                edit.caretByte = len;
            }
            edit.caretByte = NkClampByteIndex(buf, edit.caretByte);

            const float32 padX = ctx.GetPaddingX();
            const float32 padY = ctx.GetPaddingY();
            const float32 sbW = ctx.theme.metrics.scrollbarWidth;
            NkRect inner = { r.x + padX, r.y + padY, r.w - padX * 2.f, r.h - padY * 2.f };
            if (inner.w < 1.f) inner.w = 1.f;
            if (inner.h < 1.f) inner.h = 1.f;

            const float32 asc = FontAscender(font);
            const float32 lineHBase = (font.metrics.lineHeight > 0.f) ? font.metrics.lineHeight : font.size;
            const float32 lineH = lineHBase * opts.multilineLineSpacing;

            constexpr int32 kMaxLines = 2048;
            NkTextLineInfo lines[kMaxLines] = {};
            float32 contentW = 0.f;
            int32 lineCount = NkBuildTextLines(font, buf, opts.multilineWrap, inner.w, opts.multilineCharSpacing, lines, kMaxLines, &contentW);
            if (lineCount <= 0)
            {
                lines[0] = { 0, 0, 0.f };
                lineCount = 1;
            }

            if (canInteract && ctx.IsHovered(r) && ctx.ConsumeMouseClick(0))
            {
                ctx.SetFocus(id);
                ctx.SetActive(id);
                const float32 localY = (ctx.input.mousePos.y - inner.y) + edit.scrollY;
                int32 lineIdx = static_cast<int32>(localY / lineH);
                if (lineIdx < 0) lineIdx = 0;
                if (lineIdx >= lineCount) lineIdx = lineCount - 1;
                const float32 localX = (ctx.input.mousePos.x - inner.x) + (opts.multilineWrap ? 0.f : edit.scrollX);
                edit.caretByte = NkFindCaretFromX(font, buf, lines[lineIdx].start, lines[lineIdx].end, localX, opts.multilineCharSpacing);
                edit.preferredX = -1.f;
            }
            if (ctx.IsActive(id) && !ctx.input.mouseDown[0]) ctx.ClearActive();

            bool changed = false;
            bool insertedNewLineFromChars = false;
            if (ctx.IsFocused(id))
            {
                if (ctx.input.IsKeyPressed(NkKey::NK_ESCAPE))
                {
                    ctx.SetFocus(NKUI_ID_NONE);
                }
                else
                {
                    const bool moveLeft  = ConsumeRepeat(ctx, NkKey::NK_LEFT,  true, opts.repeatDelay, opts.repeatRate, rep.left);
                    const bool moveRight = ConsumeRepeat(ctx, NkKey::NK_RIGHT, true, opts.repeatDelay, opts.repeatRate, rep.right);
                    const bool moveUp    = ConsumeRepeat(ctx, NkKey::NK_UP,    true, opts.repeatDelay, opts.repeatRate, rep.up);
                    const bool moveDown  = ConsumeRepeat(ctx, NkKey::NK_DOWN,  true, opts.repeatDelay, opts.repeatRate, rep.down);

                    if (moveLeft)
                    {
                        edit.caretByte = NkFindPrevUTF8Index(buf, edit.caretByte);
                        edit.preferredX = -1.f;
                    }
                    if (moveRight)
                    {
                        edit.caretByte = NkFindNextUTF8Index(buf, edit.caretByte);
                        edit.preferredX = -1.f;
                    }

                    int32 caretLine = NkFindLineIndexForCaret(lines, lineCount, edit.caretByte);
                    if (ctx.input.IsKeyPressed(NkKey::NK_HOME))
                    {
                        edit.caretByte = lines[caretLine].start;
                        edit.preferredX = -1.f;
                    }
                    if (ctx.input.IsKeyPressed(NkKey::NK_END))
                    {
                        edit.caretByte = lines[caretLine].end;
                        edit.preferredX = -1.f;
                    }

                    if (moveUp || moveDown)
                    {
                        const NkVec2 caretPos = NkCaretPixelFromLines(font, buf, lines, lineCount, edit.caretByte, lineH, opts.multilineCharSpacing);
                        const float32 targetX = (edit.preferredX >= 0.f) ? edit.preferredX : caretPos.x;
                        edit.preferredX = targetX;
                        int32 targetLine = caretLine + (moveUp ? -1 : 1);
                        if (targetLine < 0) targetLine = 0;
                        if (targetLine >= lineCount) targetLine = lineCount - 1;
                        edit.caretByte = NkFindCaretFromX(font, buf, lines[targetLine].start, lines[targetLine].end, targetX, opts.multilineCharSpacing);
                    }

                    const bool eraseBack = ConsumeRepeat(ctx, NkKey::NK_BACK, opts.repeatBackspace,
                                                         opts.repeatDelay, opts.repeatRate, rep.backspace);
                    const bool eraseDel  = ConsumeRepeat(ctx, NkKey::NK_DELETE, opts.repeatDelete,
                                                         opts.repeatDelay, opts.repeatRate, rep.del);
                    if (eraseBack)
                    {
                        int32 newCaret = edit.caretByte;
                        if (NkEraseUTF8Backspace(buf, edit.caretByte, &newCaret))
                        {
                            edit.caretByte = newCaret;
                            changed = true;
                            edit.preferredX = -1.f;
                        }
                    }
                    if (eraseDel)
                    {
                        int32 newCaret = edit.caretByte;
                        if (NkEraseUTF8Delete(buf, edit.caretByte, &newCaret))
                        {
                            edit.caretByte = newCaret;
                            changed = true;
                            edit.preferredX = -1.f;
                        }
                    }

                    if (opts.allowTextInput)
                    {
                        for (int32 i = 0; i < ctx.input.numInputChars; ++i)
                        {
                            const uint32 c = ctx.input.inputChars[i];
                            if (c == '\r') continue;
                            if (c == '\n')
                            {
                                if (opts.allowEnterNewLine)
                                {
                                    int32 newCaret = edit.caretByte;
                                    if (NkInsertCharAt(buf, bufSize, edit.caretByte, '\n', &newCaret))
                                    {
                                        edit.caretByte = newCaret;
                                        changed = true;
                                        insertedNewLineFromChars = true;
                                        edit.preferredX = -1.f;
                                    }
                                }
                                continue;
                            }
                            if (c == '\b' || c == 127u) continue;
                            if (c >= 32u)
                            {
                                int32 newCaret = edit.caretByte;
                                if (NkInsertUTF8At(buf, bufSize, edit.caretByte, c, &newCaret))
                                {
                                    edit.caretByte = newCaret;
                                    changed = true;
                                    edit.preferredX = -1.f;
                                }
                            }
                        }
                    }

                    if (opts.allowEnterNewLine && !insertedNewLineFromChars)
                    {
                        const bool addEnter = ConsumeRepeat(ctx, NkKey::NK_ENTER, true,
                                                            opts.repeatDelay, opts.repeatRate, rep.enter);
                        if (addEnter)
                        {
                            int32 newCaret = edit.caretByte;
                            if (NkInsertCharAt(buf, bufSize, edit.caretByte, '\n', &newCaret))
                            {
                                edit.caretByte = newCaret;
                                changed = true;
                                edit.preferredX = -1.f;
                            }
                        }
                    }
                    else
                    {
                        rep.enter = {};
                    }
                }
            }
            else
            {
                rep = {};
            }

            len = static_cast<int32>(::strlen(buf));
            edit.caretByte = NkClampByteIndex(buf, edit.caretByte);

            float32 viewW = inner.w;
            float32 viewH = inner.h;
            bool showVScroll = false;
            bool showHScroll = false;
            float32 contentH = lineH;

            for (int32 pass = 0; pass < 3; ++pass)
            {
                lineCount = NkBuildTextLines(font, buf, opts.multilineWrap, viewW, opts.multilineCharSpacing, lines, kMaxLines, &contentW);
                if (lineCount <= 0)
                {
                    lines[0] = { 0, 0, 0.f };
                    lineCount = 1;
                    contentW = 0.f;
                }
                contentH = static_cast<float32>(lineCount) * lineH;
                showVScroll = contentH > viewH + 0.5f;
                showHScroll = (!opts.multilineWrap) && (contentW > viewW + 0.5f);
                const float32 nextW = inner.w - (showVScroll ? sbW : 0.f);
                const float32 nextH = inner.h - (showHScroll ? sbW : 0.f);
                const float32 clampedW = nextW > 1.f ? nextW : 1.f;
                const float32 clampedH = nextH > 1.f ? nextH : 1.f;
                const bool stableW = ::fabsf(clampedW - viewW) < 0.5f;
                const bool stableH = ::fabsf(clampedH - viewH) < 0.5f;
                viewW = clampedW;
                viewH = clampedH;
                if (stableW && stableH) break;
            }

            const float32 maxScrollX = showHScroll ? ((contentW > viewW) ? (contentW - viewW) : 0.f) : 0.f;
            const float32 maxScrollY = (contentH > viewH) ? (contentH - viewH) : 0.f;

            if (canInteract && ctx.IsHovered(r))
            {
                if (!ctx.wheelConsumed && ctx.input.mouseWheel != 0.f)
                {
                    edit.scrollY -= ctx.input.mouseWheel * ctx.theme.metrics.itemHeight * 3.f;
                    ctx.wheelConsumed = true;
                }
                if (!ctx.wheelHConsumed && ctx.input.mouseWheelH != 0.f)
                {
                    edit.scrollX -= ctx.input.mouseWheelH * ctx.theme.metrics.itemHeight * 3.f;
                    ctx.wheelHConsumed = true;
                }
            }

            const NkVec2 caretPos = NkCaretPixelFromLines(font, buf, lines, lineCount, edit.caretByte, lineH, opts.multilineCharSpacing);
            if (ctx.IsFocused(id))
            {
                if (!opts.multilineWrap)
                {
                    if (caretPos.x < edit.scrollX) edit.scrollX = caretPos.x;
                    const float32 right = edit.scrollX + viewW - 2.f;
                    if (caretPos.x > right) edit.scrollX = caretPos.x - (viewW - 2.f);
                }
                else
                {
                    edit.scrollX = 0.f;
                }

                if (caretPos.y < edit.scrollY) edit.scrollY = caretPos.y;
                const float32 caretBottom = caretPos.y + lineH;
                const float32 viewBottom = edit.scrollY + viewH;
                if (caretBottom > viewBottom) edit.scrollY = caretBottom - viewH;
            }

            if (edit.scrollX < 0.f) edit.scrollX = 0.f;
            if (edit.scrollX > maxScrollX) edit.scrollX = maxScrollX;
            if (edit.scrollY < 0.f) edit.scrollY = 0.f;
            if (edit.scrollY > maxScrollY) edit.scrollY = maxScrollY;

            const NkUIWidgetState drawState = ctx.IsFocused(id) ? (state | NkUIWidgetState::NK_FOCUSED) : state;
            DrawWidgetBg(dl, ctx.theme, r, NkUIWidgetType::NK_INPUT_TEXT, drawState);

            const NkRect textRect = { inner.x, inner.y, viewW, viewH };
            dl.PushClipRect(textRect);
            for (int32 lineIdx = 0; lineIdx < lineCount; ++lineIdx)
            {
                const float32 lineTop = textRect.y + static_cast<float32>(lineIdx) * lineH - edit.scrollY;
                if (lineTop > textRect.y + textRect.h) break;
                if (lineTop + lineH < textRect.y) continue;

                const float32 baseline = lineTop + asc;
                float32 x = textRect.x - (opts.multilineWrap ? 0.f : edit.scrollX);
                int32 i = lines[lineIdx].start;
                while (i < lines[lineIdx].end && buf[i])
                {
                    uint32 cp = '?';
                    const int32 n = NkDecodeUTF8Codepoint(&buf[i], cp);
                    if (n <= 0) break;
                    if (cp != ' ' && cp != '\t' && cp != '\r' && cp != '\n')
                    {
                        font.RenderChar(dl, { x, baseline }, cp, ctx.theme.colors.inputText);
                    }
                    x += NkGlyphAdvance(font, cp, opts.multilineCharSpacing);
                    i += n;
                }
            }
            if (ctx.IsFocused(id) && static_cast<int32>(ctx.time * 2.f) % 2 == 0)
            {
                const float32 cx = textRect.x - (opts.multilineWrap ? 0.f : edit.scrollX) + caretPos.x;
                const float32 cy = textRect.y - edit.scrollY + caretPos.y;
                dl.AddLine({ cx, cy }, { cx, cy + lineH }, ctx.theme.colors.inputCursor, 1.5f);
            }
            dl.PopClipRect();

            if (showVScroll)
            {
                const NkRect track = { textRect.x + textRect.w, textRect.y, sbW, textRect.h };
                NkUILayout::DrawScrollbar(ctx, dl, true, track, contentH, textRect.h, edit.scrollY, NkHashInt(id, 201));
            }
            if (showHScroll)
            {
                const NkRect track = { textRect.x, textRect.y + textRect.h, textRect.w, sbW };
                NkUILayout::DrawScrollbar(ctx, dl, false, track, contentW, textRect.w, edit.scrollX, NkHashInt(id, 202));
            }

            NkUILayout::AdvanceItem(ctx, ls, r);
            return changed;
        }

        // ---------------------------------------------------------------------
        // Combo
        // ---------------------------------------------------------------------

        bool NkUI::Combo(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font,
                         const char* label, int32& selected,
                         const char* const* items, int32 numItems, float32 width) noexcept
        {
            static NkUIID sOpenComboId = NKUI_ID_NONE;

            const NkUILabelParts lp = NkParseLabel(label);
            const NkUIID id = ResolveLabelID(ctx, ls, lp);
            const float32 inputW = width > 0 ? width : 160.f;
            const float32 labelW = lp.label[0] ? font.MeasureWidth(lp.label) + ctx.GetPaddingX() * 2 : 0;
            const NkRect r = NkUILayout::NextItemRect(ctx, ls, inputW + labelW, ctx.GetItemHeight());
            const NkRect comboR = { r.x, r.y, inputW, r.h };
            const NkUIWidgetState state = CalcState(ctx, id, comboR);
            bool changed = false;
            const bool canInteract = WindowAllowsInputForWidget(ctx, ctx.currentWindowId, id);
            const bool comboHovered = canInteract && ctx.IsHovered(comboR);
            const bool fullHovered = canInteract && ctx.IsHovered(r);
            bool popupOpen = (sOpenComboId == id);

            if (fullHovered && (ctx.activeId == NKUI_ID_NONE || ctx.activeId == id) && ctx.ConsumeMouseClick(0))
            {
                ctx.SetActive(id);
                if (popupOpen)
                {
                    sOpenComboId = NKUI_ID_NONE;
                    ctx.SetFocus(NKUI_ID_NONE);
                }
                else
                {
                    sOpenComboId = id;
                    ctx.SetFocus(id);
                }
                popupOpen = (sOpenComboId == id);
            }
            if (ctx.IsActive(id) && !ctx.input.mouseDown[0]) ctx.ClearActive();

            DrawWidgetBg(dl, ctx.theme, comboR, NkUIWidgetType::NK_COMBO, state);
            if (selected >= 0 && selected < numItems)
            {
                font.RenderText(dl, { comboR.x + ctx.GetPaddingX(), TextBaselineY(comboR, font) },
                                items[selected], ctx.theme.colors.inputText,
                                comboR.w - ctx.theme.metrics.comboArrowW - ctx.GetPaddingX(), true);
            }
            // Flèche
            dl.AddArrow({ comboR.x + comboR.w - ctx.theme.metrics.comboArrowW * 0.5f, comboR.y + comboR.h * 0.5f },
                        8.f, 1, ctx.theme.colors.textSecondary);

            // Popup déroulante
            if (popupOpen)
            {
                const float32 popH = ctx.GetItemHeight() * ::fminf(numItems, 8.f) + 4;
                const NkRect popup = { comboR.x, comboR.y + comboR.h, comboR.w, popH };
                const bool popupCanInteract = true;
                // Layer popup
                NkUIDrawList* prevDL = ctx.dl;
                ctx.dl = &ctx.layers[NkUIContext::LAYER_POPUPS];
                ctx.dl->AddRectFilled(popup, ctx.theme.colors.bgPopup, ctx.theme.metrics.cornerRadius);
                ctx.dl->AddRect(popup, ctx.theme.colors.border, 1.f, ctx.theme.metrics.cornerRadius);
                ctx.dl->PushClipRect(popup);
                for (int32 i = 0; i < numItems; ++i)
                {
                    const NkRect item = { popup.x + 2, popup.y + 2 + i * ctx.GetItemHeight(), popup.w - 4, ctx.GetItemHeight() };
                    const bool hov = popupCanInteract && ctx.IsHovered(item);
                    if (hov) ctx.dl->AddRectFilled(item, ctx.theme.colors.accent, ctx.theme.metrics.cornerRadiusSm);
                    font.RenderText(*ctx.dl, { item.x + ctx.GetPaddingX(), TextBaselineY(item, font) },
                                    items[i], hov ? ctx.theme.colors.textOnAccent : ctx.theme.colors.textPrimary);
                    if (hov && ctx.ConsumeMouseClick(0))
                    {
                        selected = i;
                        changed = true;
                        sOpenComboId = NKUI_ID_NONE;
                        ctx.SetFocus(NKUI_ID_NONE);
                        ctx.ClearActive();
                    }
                }
                ctx.dl->PopClipRect();
                ctx.dl = prevDL;
                // Ferme si clic ailleurs
                if (ctx.IsMouseClicked(0) && !ctx.IsHovered(popup) && !ctx.IsHovered(r))
                {
                    sOpenComboId = NKUI_ID_NONE;
                    ctx.SetFocus(NKUI_ID_NONE);
                    ctx.ClearActive();
                }
            }
            if (lp.label[0])
            {
                font.RenderText(dl, { r.x + inputW + ctx.GetPaddingX() * 0.5f, TextBaselineY(r, font) },
                                lp.label, ctx.theme.colors.textPrimary);
            }
            NkUILayout::AdvanceItem(ctx, ls, r);
            return changed;
        }

        // ---------------------------------------------------------------------
        // ListBox
        // ---------------------------------------------------------------------

        bool NkUI::ListBox(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font,
                           const char* label, int32& selected,
                           const char* const* items, int32 numItems, int32 visibleCount,
                           const NkUIListBoxOptions* optsPtr) noexcept
        {
            const NkUIListBoxOptions* opts = optsPtr;
            const NkUILabelParts lp = NkParseLabel(label);
            const NkUIID id = ResolveLabelID(ctx, ls, lp);
            const float32 h = ctx.GetItemHeight() * visibleCount + 4;
            const NkRect r = NkUILayout::NextItemRect(ctx, ls, 200.f, h);
            const bool canInteract = WindowAllowsInputForWidget(ctx, ctx.currentWindowId, id);

            static NkWidgetStateStore<float32, 2048> sScrollYStore;
            float32& scrollY = AccessWidgetState(sScrollYStore, id, 0.f);
            const float32 itemH = ctx.GetItemHeight();
            const float32 contentH = numItems * itemH + 4.f;
            const float32 maxScroll = contentH > r.h ? (contentH - r.h) : 0.f;

            if (canInteract && ctx.IsHovered(r) && !ctx.wheelConsumed && ctx.input.mouseWheel != 0.f)
            {
                scrollY -= ctx.input.mouseWheel * itemH * 2.f;
                ctx.wheelConsumed = true;
            }
            if (scrollY < 0.f) scrollY = 0.f;
            if (scrollY > maxScroll) scrollY = maxScroll;

            const bool multiSelect = (opts && opts->multiSelect && opts->selectedMask && opts->selectedMaskCount >= numItems);
            const bool ctrlDown = ctx.input.ctrl;
            const float32 sbW = (contentH > r.h) ? ctx.theme.metrics.scrollbarWidth : 0.f;

            dl.AddRectFilled(r, ctx.theme.colors.inputBg, ctx.theme.metrics.cornerRadius);
            dl.AddRect(r, ctx.theme.colors.border, 1.f, ctx.theme.metrics.cornerRadius);
            dl.PushClipRect(r);

            bool changed = false;
            for (int32 i = 0; i < numItems; ++i)
            {
                const NkRect item = { r.x + 2, r.y + 2 + i * itemH - scrollY, r.w - 4 - sbW, itemH };
                if (item.y + item.h < r.y || item.y > r.y + r.h) continue;

                const bool sel = multiSelect ? opts->selectedMask[i] : (i == selected);
                const bool hov = canInteract && ctx.IsHovered(item);

                if (sel || hov)
                {
                    dl.AddRectFilled(item, sel ? ctx.theme.colors.accent : ctx.theme.colors.buttonHover,
                                     ctx.theme.metrics.cornerRadiusSm);
                }

                font.RenderText(dl, { item.x + ctx.GetPaddingX(), TextBaselineY(item, font) },
                                items[i], (sel || hov) ? ctx.theme.colors.textOnAccent : ctx.theme.colors.textPrimary);

                if (hov && ctx.ConsumeMouseClick(0))
                {
                    if (multiSelect)
                    {
                        bool allowToggle = true;
                        if (opts->requireCtrlForMulti && !ctrlDown)
                        {
                            for (int32 k = 0; k < numItems; ++k)
                            {
                                if (k != i && opts->selectedMask[k])
                                {
                                    opts->selectedMask[k] = false;
                                    changed = true;
                                }
                            }
                            if (!opts->selectedMask[i])
                            {
                                opts->selectedMask[i] = true;
                                changed = true;
                            }
                            allowToggle = false;
                        }

                        if (allowToggle)
                        {
                            if (opts->toggleSelection)
                            {
                                opts->selectedMask[i] = !opts->selectedMask[i];
                                changed = true;
                            }
                            else if (!opts->selectedMask[i])
                            {
                                opts->selectedMask[i] = true;
                                changed = true;
                            }
                        }
                        selected = i;
                    }
                    else
                    {
                        if (selected != i)
                        {
                            selected = i;
                            changed = true;
                        }
                    }
                }
            }
            dl.PopClipRect();

            if (contentH > r.h)
            {
                const NkRect track = {
                    r.x + r.w - ctx.theme.metrics.scrollbarWidth,
                    r.y,
                    ctx.theme.metrics.scrollbarWidth,
                    r.h
                };
                NkUILayout::DrawScrollbar(ctx, dl, true, track, contentH, r.h, scrollY, NkHashInt(id, 101));
            }

            NkUILayout::AdvanceItem(ctx, ls, r);
            return changed;
        }

        // ---------------------------------------------------------------------
        // ProgressBar
        // ---------------------------------------------------------------------

        void NkUI::ProgressBar(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl,
                               float32 fraction, NkVec2 size, const char* overlay) noexcept
        {
            const float32 w = size.x > 0 ? size.x : 200.f;
            const float32 h = size.y > 0 ? size.y : ctx.GetItemHeight() * 0.5f;
            const NkRect r = NkUILayout::NextItemRect(ctx, ls, w, h);
            const float32 rx = ctx.theme.metrics.cornerRadius;
            dl.AddRectFilled(r, ctx.theme.colors.sliderTrack, rx);
            const float32 f = fraction < 0 ? 0 : fraction > 1 ? 1 : fraction;
            if (f > 0)
            {
                NkRect fill = r;
                fill.w *= f;
                dl.AddRectFilled(fill, ctx.theme.colors.accent, rx);
            }
            dl.AddRect(r, ctx.theme.colors.border, 1.f, rx);
            if (overlay && *overlay)
            {
                NkUIFont fallbackFont;
                const NkUIFont* drawFont = ctx.GetActiveFont();
                if (!drawFont)
                {
                    const float32 baseSize = ctx.theme.fonts.small.size > 0.f ? ctx.theme.fonts.small.size : 12.f;
                    const float32 maxFit = (r.h > 3.f) ? (r.h - 2.f) : r.h;
                    fallbackFont.size = (maxFit > 1.f && baseSize > maxFit) ? maxFit : baseSize;
                    if (fallbackFont.size < 6.f) fallbackFont.size = 6.f;
                    fallbackFont.metrics.ascender = fallbackFont.size * 0.78f;
                    fallbackFont.metrics.descender = fallbackFont.size * 0.22f;
                    fallbackFont.metrics.lineHeight = fallbackFont.metrics.ascender + fallbackFont.metrics.descender;
                    fallbackFont.metrics.spaceWidth = fallbackFont.size * 0.45f;
                    fallbackFont.metrics.tabWidth = fallbackFont.metrics.spaceWidth * 4.f;
                    drawFont = &fallbackFont;
                }

                const float32 maxTextW = r.w > 4.f ? (r.w - 4.f) : r.w;
                const float32 textW = drawFont->MeasureWidth(overlay);
                const float32 drawW = textW < maxTextW ? textW : maxTextW;
                const float32 tx = r.x + (r.w - drawW) * 0.5f;
                const float32 ty = TextRenderYCentered(r, *drawFont);
                dl.PushClipRect(r);
                drawFont->RenderText(dl, { tx, ty }, overlay, ctx.theme.colors.textPrimary, maxTextW, true);
                dl.PopClipRect();
            }
            NkUILayout::AdvanceItem(ctx, ls, r);
        }

        void NkUI::ProgressBarVertical(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl,
                                       float32 fraction, NkVec2 size, const char* overlay) noexcept
        {
            const float32 w = size.x > 0 ? size.x : (ctx.GetItemHeight() * 0.7f);
            const float32 h = size.y > 0 ? size.y : 140.f;
            const NkRect r = NkUILayout::NextItemRect(ctx, ls, w, h);
            const float32 rx = ctx.theme.metrics.cornerRadius;

            dl.AddRectFilled(r, ctx.theme.colors.sliderTrack, rx);
            const float32 f = fraction < 0.f ? 0.f : (fraction > 1.f ? 1.f : fraction);
            if (f > 0.f)
            {
                NkRect fill = r;
                fill.y = r.y + r.h * (1.f - f);
                fill.h = r.h * f;
                dl.AddRectFilled(fill, ctx.theme.colors.accent, rx);
            }
            dl.AddRect(r, ctx.theme.colors.border, 1.f, rx);

            if (overlay && *overlay)
            {
                NkUIFont fallbackFont;
                const NkUIFont* drawFont = ctx.GetActiveFont();
                if (!drawFont)
                {
                    const float32 baseSize = ctx.theme.fonts.small.size > 0.f ? ctx.theme.fonts.small.size : 12.f;
                    const float32 maxFit = (r.h > 3.f) ? (r.h - 2.f) : r.h;
                    fallbackFont.size = (maxFit > 1.f && baseSize > maxFit) ? maxFit : baseSize;
                    if (fallbackFont.size < 6.f) fallbackFont.size = 6.f;
                    fallbackFont.metrics.ascender = fallbackFont.size * 0.78f;
                    fallbackFont.metrics.descender = fallbackFont.size * 0.22f;
                    fallbackFont.metrics.lineHeight = fallbackFont.metrics.ascender + fallbackFont.metrics.descender;
                    fallbackFont.metrics.spaceWidth = fallbackFont.size * 0.45f;
                    fallbackFont.metrics.tabWidth = fallbackFont.metrics.spaceWidth * 4.f;
                    drawFont = &fallbackFont;
                }

                const float32 maxTextW = r.w > 4.f ? (r.w - 4.f) : r.w;
                const float32 textW = drawFont->MeasureWidth(overlay);
                const float32 drawW = textW < maxTextW ? textW : maxTextW;
                const float32 tx = r.x + (r.w - drawW) * 0.5f;
                const float32 ty = TextRenderYCentered(r, *drawFont);
                dl.PushClipRect(r);
                drawFont->RenderText(dl, { tx, ty }, overlay, ctx.theme.colors.textPrimary, maxTextW, true);
                dl.PopClipRect();
            }

            NkUILayout::AdvanceItem(ctx, ls, r);
        }

        void NkUI::ProgressBarCircular(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font,
                                       float32 fraction, float32 diameter, const char* overlay) noexcept
        {
            const float32 d = diameter > 0.f ? diameter : 72.f;
            const NkRect r = NkUILayout::NextItemRect(ctx, ls, d, d);
            const NkVec2 c = { r.x + r.w * 0.5f, r.y + r.h * 0.5f };
            const float32 radius = (r.w < r.h ? r.w : r.h) * 0.5f - 2.f;
            const float32 thickness = radius * 0.2f;
            const float32 f = fraction < 0.f ? 0.f : (fraction > 1.f ? 1.f : fraction);

            dl.AddCircle(c, radius, ctx.theme.colors.sliderTrack, thickness);
            if (f > 0.f)
            {
                const float32 start = -NKUI_PI * 0.5f;
                const float32 end = start + f * NKUI_PI * 2.f;
                dl.AddArc(c, radius, start, end, ctx.theme.colors.accent, thickness);
            }

            if (overlay && *overlay)
            {
                const float32 maxTextW = r.w > 4.f ? (r.w - 4.f) : r.w;
                const float32 textW = font.MeasureWidth(overlay);
                const float32 drawW = textW < maxTextW ? textW : maxTextW;
                const float32 tx = r.x + (r.w - drawW) * 0.5f;
                const float32 ty = TextRenderYCentered(r, font);
                dl.PushClipRect(r);
                font.RenderText(dl, { tx, ty }, overlay, ctx.theme.colors.textPrimary, maxTextW, true);
                dl.PopClipRect();
            }

            NkUILayout::AdvanceItem(ctx, ls, r);
        }

        // ---------------------------------------------------------------------
        // ColorButton
        // ---------------------------------------------------------------------

        bool NkUI::ColorButton(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font,
                               const char* label, NkColor& col, float32 size) noexcept
        {
            const NkUILabelParts lp = NkParseLabel(label);
            const NkUIID id = ResolveLabelID(ctx, ls, lp);
            const float32 tw = font.MeasureWidth(lp.label);
            const NkRect r = NkUILayout::NextItemRect(ctx, ls, size + ctx.GetPaddingX() + tw, ctx.GetItemHeight());
            const NkRect box = { r.x, r.y + (r.h - size) * 0.5f, size, size };
            const NkUIWidgetState state = CalcState(ctx, id, box);
            // Damier pour l'alpha
            dl.AddRectFilled(box, NkColor::Gray(200), ctx.theme.metrics.cornerRadiusSm);
            dl.AddRectFilled({ box.x, box.y, box.w * 0.5f, box.h * 0.5f }, NkColor::White());
            dl.AddRectFilled({ box.x + box.w * 0.5f, box.y + box.h * 0.5f, box.w * 0.5f, box.h * 0.5f }, NkColor::White());
            // Couleur par-dessus
            dl.AddRectFilled(box, col, ctx.theme.metrics.cornerRadiusSm);
            dl.AddRect(box, HasState(state, NkUIWidgetState::NK_HOVERED) ? ctx.theme.colors.borderHover : ctx.theme.colors.border,
                       1.f, ctx.theme.metrics.cornerRadiusSm);
            if (lp.label[0])
            {
                font.RenderText(dl, { box.x + size + ctx.GetPaddingX(), TextBaselineY(r, font) },
                                lp.label, ctx.theme.colors.textPrimary);
            }
            NkUILayout::AdvanceItem(ctx, ls, r);
            return HasState(state, NkUIWidgetState::NK_ACTIVE) && ctx.IsMouseReleased(0) && ctx.IsHovered(box);
        }

        // ---------------------------------------------------------------------
        // ColorPicker (simplifié)
        // ---------------------------------------------------------------------

        bool NkUI::ColorPicker(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font,
                               const char* label, NkColor& col) noexcept
        {
            const float32 sz = ctx.theme.metrics.colorPickerW;
            const NkRect r = NkUILayout::NextItemRect(ctx, ls, sz, sz + 30);
            dl.AddColorWheel(r, col.r / 255.f, col.g / 255.f, col.b / 255.f);
            NkUILayout::AdvanceItem(ctx, ls, r);
            return false; // interaction complète en session suivante
        }

        // ---------------------------------------------------------------------
        // TreeNode
        // ---------------------------------------------------------------------

        struct NkTreeNodeStore
        {
            static constexpr uint32 CAP = 8192;
            NkUIID ids[CAP]    = {};
            bool   open[CAP]   = {};
            bool   used[CAP]   = {};
            uint32 count       = 0;

            bool& Lookup(NkUIID id, bool defaultVal = false) noexcept
            {
                const uint32 slot0 = id % CAP;
                for (uint32 k = 0; k < CAP; ++k)
                {
                    const uint32 s = (slot0 + k) % CAP;
                    if (used[s] && ids[s] == id) return open[s];
                    if (!used[s])
                    {
                        used[s] = true;
                        ids[s] = id;
                        open[s] = defaultVal;
                        ++count;
                        return open[s];
                    }
                }
                // Table pleine : on écrase le slot
                open[slot0] = defaultVal;
                return open[slot0];
            }
        };

        static NkTreeNodeStore sTreeStore;

        static float32 TreeBaseY(const NkRect& r, const NkUIFont& f) noexcept
        {
            const float32 asc  = f.metrics.ascender  > 0.f ? f.metrics.ascender  : f.size * 0.8f;
            const float32 desc = f.metrics.descender >= 0.f ? f.metrics.descender : -f.metrics.descender;
            const float32 lh   = f.metrics.lineHeight > 0.f ? f.metrics.lineHeight
                                                            : asc + (desc > 0.f ? desc : f.size * 0.2f);
            return r.y + (r.h - lh) * .5f + asc;
        }

        bool NkUI::TreeNode(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font,
                            const char* label, bool* openPtr) noexcept
        {
            const NkUILabelParts lp = NkParseLabel(label);
            NkUIID id = ctx.GetID(lp.id[0] ? lp.id : lp.label);
            {
                NkUILayoutNode* n = ls.Top();
                if (n && !lp.id[0])
                {
                    id = NkHashInt(static_cast<int32>(n->itemCount), id);
                }
            }

            bool& storedOpen = sTreeStore.Lookup(id, false);
            bool& open = openPtr ? *openPtr : storedOpen;

            const float32 h = ctx.GetItemHeight();
            NkUILayoutNode* n = ls.Top();
            float32 availW = n
                ? (n->bounds.x + n->bounds.w - (n->cursor.x - n->scrollX))
                : (static_cast<float32>(ctx.viewW) - ctx.cursor.x);
            if (availW < 40.f) availW = 40.f;

            const NkRect r = NkUILayout::NextItemRect(ctx, ls, availW, h);
            const NkUIWidgetState state = CalcState(ctx, id, r);
            if (HasState(state, NkUIWidgetState::NK_ACTIVE) &&
                ctx.IsMouseReleased(0) && ctx.IsHovered(r))
            {
                open = !open;
                if (openPtr) storedOpen = open;
                ctx.ClearActive();
            }

            if (HasState(state, NkUIWidgetState::NK_HOVERED))
            {
                dl.AddRectFilled(r, ctx.theme.colors.buttonHover, ctx.theme.metrics.cornerRadiusSm);
            }

            const float32 anim = ctx.Animate(NkHashInt(static_cast<int32>(id), 0xA77), open ? 1.f : 0.f, 12.f);
            const int32 arrowDir = (anim > 0.5f) ? 1 : 0;
            const float32 arrowX = r.x + ctx.theme.metrics.treeIndent * 0.35f + 4.f;
            dl.AddArrow({ arrowX, r.y + h * .5f }, 7.f, arrowDir, ctx.theme.colors.textSecondary);

            const NkColor lblCol = HasState(state, NkUIWidgetState::NK_DISABLED)
                                    ? ctx.theme.colors.textDisabled
                                    : ctx.theme.colors.textPrimary;
            const float32 textX = r.x + ctx.theme.metrics.treeIndent * 0.35f + 14.f;
            font.RenderText(dl, { textX, TreeBaseY(r, font) }, lp.label, lblCol,
                            r.w - (textX - r.x) - 4.f, true);

            NkUILayout::AdvanceItem(ctx, ls, r);

            if (open && n)
            {
                n->cursor.x += ctx.theme.metrics.treeIndent;
            }
            return open;
        }

        void NkUI::TreePop(NkUIContext& ctx, NkUILayoutStack& ls) noexcept
        {
            if (NkUILayoutNode* n = ls.Top())
            {
                n->cursor.x -= ctx.theme.metrics.treeIndent;
                if (n->cursor.x < n->bounds.x) n->cursor.x = n->bounds.x;
            }
        }

        // ---------------------------------------------------------------------
        // Table
        // ---------------------------------------------------------------------

        struct TableState
        {
            int32   cols;
            float32 w;
            float32 y;
            int32   row;
            int32   col;
            NkUIID  id;
            int32   selectedRow;
        };
        static TableState sTables[64] = {};
        static int32 sTableDepth = 0;
        static NkWidgetStateStore<int32, 2048> sTableSelectionStore;

        bool NkUI::BeginTable(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont&,
                              const char* id, int32 columns, float32 width) noexcept
        {
            if (sTableDepth >= 64) return false;
            TableState& ts = sTables[sTableDepth++];
            ts.cols = columns > 0 ? columns : 1;
            ts.w = width > 0 ? width : (ls.Top() ? ls.Top()->bounds.w : 200.f);
            NkUILayoutNode* n = ls.Top();
            const float32 baseX = n ? (n->cursor.x - n->scrollX) : ctx.cursor.x;
            const float32 baseY = n ? (n->cursor.y - n->scrollY) : ctx.cursor.y;
            ts.y = baseY;
            ts.row = -1;
            ts.col = 0;
            ts.id = ctx.GetID((id && id[0]) ? id : "##table");
            ts.selectedRow = AccessWidgetState(sTableSelectionStore, ts.id, -1);
            // Ligne d'en-tête
            const NkRect header = { baseX, baseY, ts.w, ctx.GetItemHeight() };
            dl.AddRectFilled(header, ctx.theme.colors.bgHeader);
            // Prépare la grille
            BeginGrid(ctx, ls, columns, ctx.GetItemHeight());
            return true;
        }

        void NkUI::TableHeader(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* label) noexcept
        {
            NkRect r = NkUILayout::NextItemRect(ctx, ls, 0, ctx.GetItemHeight());
            font.RenderText(dl, { r.x + ctx.GetPaddingX(), TextBaselineY(r, font) },
                            label, ctx.theme.colors.textOnAccent);
            NkUILayout::AdvanceItem(ctx, ls, r);
        }

        bool NkUI::TableNextRow(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, bool striped) noexcept
        {
            if (sTableDepth <= 0) return false;
            TableState& ts = sTables[sTableDepth - 1];
            ++ts.row;
            ts.col = 0;
            NkUILayoutNode* n = ls.Top();
            if (n) n->gridCol = 0;

            const float32 rowX = n ? (n->cursor.x - n->scrollX) : ctx.cursor.x;
            const float32 rowY = n ? (n->cursor.y - n->scrollY) : ctx.cursor.y;
            const NkRect r = { rowX, rowY, ts.w, ctx.GetItemHeight() };
            const NkUIID rowId = NkHashInt(ts.id, ts.row);
            const bool canInteract = WindowAllowsInputForWidget(ctx, ctx.currentWindowId, rowId);
            const bool hovered = canInteract && ctx.IsHovered(r);
            if (hovered && ctx.ConsumeMouseClick(0))
            {
                ts.selectedRow = ts.row;
                AccessWidgetState(sTableSelectionStore, ts.id, -1) = ts.selectedRow;
            }

            if (ts.row == ts.selectedRow)
            {
                NkColor sel = ctx.theme.colors.accent;
                sel.a = 90;
                dl.AddRectFilled(r, sel, ctx.theme.metrics.cornerRadiusSm);
            }
            else if (striped && (ts.row % 2 == 0))
            {
                dl.AddRectFilled(r, ctx.theme.colors.bgSecondary);
            }
            return true;
        }

        void NkUI::TableNextCell(NkUIContext& ctx, NkUILayoutStack& ls) noexcept
        {
            if (sTableDepth > 0) ++sTables[sTableDepth - 1].col;
        }

        void NkUI::EndTable(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl) noexcept
        {
            if (sTableDepth > 0)
            {
                const TableState& ts = sTables[sTableDepth - 1];
                AccessWidgetState(sTableSelectionStore, ts.id, -1) = ts.selectedRow;
            }
            EndGrid(ctx, ls);
            if (sTableDepth > 0) --sTableDepth;
        }

        // ---------------------------------------------------------------------
        // Tooltip
        // ---------------------------------------------------------------------

        static char    sTooltipText[512]    = {};
        static float32 sTooltipHoverTime    = 0.f;
        static NkUIID  sTooltipLastHotId    = NKUI_ID_NONE;
        static bool    sTooltipPendingSet   = false;
        static float32 sTooltipTimer        = 0;

        void NkUI::SetTooltip(NkUIContext& ctx, const char* text) noexcept
        {
            if (text && *text)
            {
                ::strncpy(sTooltipText, text, 511);
                sTooltipText[511] = 0;
                sTooltipPendingSet = true;
            }
            else
            {
                sTooltipText[0] = 0;
                sTooltipPendingSet = false;
            }
            (void)ctx;
        }

        void NkUI::Tooltip(NkUIContext& ctx, NkUIDrawList& dl, NkUIFont& font, const char* text) noexcept
        {
            if (!text || !*text) return;

            const NkUIID curHot = ctx.hotId;
            if (curHot != sTooltipLastHotId)
            {
                sTooltipLastHotId = curHot;
                sTooltipHoverTime = 0.f;
            }
            else
            {
                sTooltipHoverTime += ctx.dt;
            }

            if (sTooltipHoverTime < ctx.theme.metrics.tooltipDelay) return;

            const float32 padX = ctx.theme.metrics.tooltipPadX;
            const float32 padY = ctx.theme.metrics.tooltipPadY;
            const float32 tw = font.MeasureWidth(text);

            const float32 asc  = font.metrics.ascender  > 0.f ? font.metrics.ascender  : font.size * 0.8f;
            const float32 desc = font.metrics.descender >= 0.f ? font.metrics.descender : -font.metrics.descender;
            const float32 lh   = font.metrics.lineHeight > 0.f ? font.metrics.lineHeight
                                                               : asc + (desc > 0.f ? desc : font.size * 0.2f);
            const float32 th   = lh + padY * 2.f;
            const float32 bw   = tw + padX * 2.f;

            float32 tx = ctx.input.mousePos.x + 14.f;
            float32 ty = ctx.input.mousePos.y + 14.f;
            if (tx + bw > ctx.viewW) tx = ctx.input.mousePos.x - bw - 6.f;
            if (ty + th  > ctx.viewH) ty = ctx.input.mousePos.y - th - 4.f;
            if (tx < 2.f) tx = 2.f;
            if (ty < 2.f) ty = 2.f;

            const NkRect r = { tx, ty, bw, th };

            NkUIDrawList& odl = ctx.layers[NkUIContext::LAYER_OVERLAY];
            odl.AddShadow(r, 6.f, { 0,0,0,60 }, { 1,2 });
            odl.AddRectFilled(r, ctx.theme.colors.tooltip, ctx.theme.metrics.cornerRadiusSm);
            odl.AddRect(r, ctx.theme.colors.border.WithAlpha(100), 1.f, ctx.theme.metrics.cornerRadiusSm);

            const float32 bly = r.y + (r.h - lh) * .5f + asc;
            font.RenderText(odl, { tx + padX, bly }, text, ctx.theme.colors.tooltipText);
        }

        void NkUI::FlushPendingTooltip(NkUIContext& ctx, NkUIFont& font) noexcept
        {
            if (!sTooltipPendingSet)
            {
                sTooltipLastHotId = NKUI_ID_NONE;
                sTooltipHoverTime = 0.f;
                return;
            }
            sTooltipPendingSet = false;

            const NkUIID curHot = ctx.hotId;
            if (curHot != sTooltipLastHotId)
            {
                sTooltipLastHotId = curHot;
                sTooltipHoverTime = 0.f;
            }
            else
            {
                sTooltipHoverTime += ctx.dt;
            }

            if (sTooltipHoverTime < ctx.theme.metrics.tooltipDelay) return;
            if (!sTooltipText[0]) return;

            const float32 padX = ctx.theme.metrics.tooltipPadX;
            const float32 padY = ctx.theme.metrics.tooltipPadY;
            const float32 tw = font.MeasureWidth(sTooltipText);
            const float32 asc  = font.metrics.ascender  > 0.f ? font.metrics.ascender  : font.size * 0.8f;
            const float32 desc = font.metrics.descender >= 0.f ? font.metrics.descender : -font.metrics.descender;
            const float32 lh   = font.metrics.lineHeight > 0.f ? font.metrics.lineHeight
                                                               : asc + (desc > 0.f ? desc : font.size * 0.2f);
            const float32 bw = tw + padX * 2.f;
            const float32 th = lh + padY * 2.f;

            float32 tx = ctx.input.mousePos.x + 14.f;
            float32 ty = ctx.input.mousePos.y + 14.f;
            if (tx + bw > ctx.viewW) tx = ctx.input.mousePos.x - bw - 6.f;
            if (ty + th  > ctx.viewH) ty = ctx.input.mousePos.y - th  - 4.f;
            if (tx < 2.f) tx = 2.f;
            if (ty < 2.f) ty = 2.f;

            const NkRect r = { tx, ty, bw, th };
            NkUIDrawList& odl = ctx.layers[NkUIContext::LAYER_OVERLAY];
            odl.AddShadow(r, 6.f, { 0,0,0,60 }, { 1,2 });
            odl.AddRectFilled(r, ctx.theme.colors.tooltip, ctx.theme.metrics.cornerRadiusSm);
            odl.AddRect(r, ctx.theme.colors.border.WithAlpha(100), 1.f, ctx.theme.metrics.cornerRadiusSm);
            const float32 bly = r.y + (r.h - lh) * .5f + asc;
            font.RenderText(odl, { tx + padX, bly }, sTooltipText, ctx.theme.colors.tooltipText);
        }

    } // namespace nkui
} // namespace nkentseu