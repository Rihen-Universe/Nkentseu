#pragma once
// =============================================================================
// NkAiPanel.h — Panneau ASSISTANT IA (chat) fonctionnel.
//   UI : sélecteur de fournisseur + liste de messages (bulles) + zone de saisie.
//   Backends (via `curl`, appel ASYNCHRONE NkProcess -> ne gèle pas l'UI) :
//     0) Claude API (Anthropic) — clé dans NKCODE_ANTHROPIC_KEY / ANTHROPIC_API_KEY
//     1) Ollama local          — http://localhost:11434 (aucune clé)
//     2) Maison (à venir)      — réservé pour l'IA maison de Rihen (stub désactivé)
//   Non-streaming pour ce 1er jet (réponse complète). Parsing JSON minimal.
// =============================================================================
#include "NKEditorKit/NkEditorKit.h"
#include "NKCode/Project/NkCodeState.h"
#include "NKCode/Project/NkProcess.h"
#include "NKCode/Editor/NkTextDraw.h"   // NkEncodeU8 (décodage \uXXXX -> UTF-8)
#include "NKCode/Shell/NkI18n.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
namespace nkcode {

    using namespace nkentseu::editorkit;
    using namespace nkentseu::nkgui;

    class AiPanel : public NkEditorPanel {
    public:
        explicit AiPanel(NkCodeState* s)
            : NkEditorPanel("Assistant IA", NkEditorDockSide::NK_RIGHT), mS(s) {
            mInput[0] = 0;
        }

        void OnUI(NkEditorFrameContext& ec) override {
            auto& ctx = ec.Ui();
            auto& dl  = ctx.DL();
            const NkRect r = dl.CurrentClip();
            if (!mGreeted) { mMsgs.PushBack({ 2, NkString(NkT("ai.hello")) }); mGreeted = true; }   // locale correcte (posée après construction)
            Poll();   // draine la réponse en cours

            const float32 pad = ctx.S(8.f);
            const float32 hdrH = ctx.ItemHeight() + ctx.S(6.f);
            const float32 inH  = ctx.S(76.f);   // zone de saisie
            const NkRect hdr = { r.x, r.y, r.w, hdrH };
            const NkRect body= { r.x, r.y + hdrH, r.w, r.h - hdrH - inH };
            const NkRect inR = { r.x, r.y + r.h - inH, r.w, inH };

            // ── En-tête : titre + fournisseur ──
            dl.AddRectFilled(hdr, ctx.theme.header);
            dl.AddRectFilled({ hdr.x, hdr.y + hdrH - 1.f, hdr.w, 1.f }, ctx.theme.border);
            if (ctx.font && ctx.font->Valid()) {
                const float32 by = hdr.y + (hdrH - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent();
                dl.AddText(ctx.font->Face(), ctx.font->TexId(), { hdr.x + pad, by }, NkT("ai.title"), ctx.theme.text);
                // Boutons fournisseur (segmented) à droite.
                const char* pv[3] = { "Claude", "Ollama", NkT("ai.home") };
                float32 bx = hdr.x + hdr.w - pad;
                for (int32 i = 2; i >= 0; --i) {
                    const float32 tw = ctx.font->MeasureWidth(pv[i]) + ctx.S(16.f);
                    const NkRect pb = { bx - tw, hdr.y + (hdrH - ctx.ItemHeight()) * 0.5f, tw, ctx.ItemHeight() };
                    const bool sel = (mProvider == i), dis = (i == 2);
                    const bool hov = !dis && NkGuiRectContains(pb, ctx.input.mousePos);
                    dl.AddRectFilled(pb, sel ? ctx.theme.buttonActive : (hov ? ctx.theme.buttonHover : ctx.theme.button), ctx.theme.rounding);
                    const NkColor fg = dis ? ctx.theme.textDisabled : (sel ? NkColor{ 255,255,255,255 } : ctx.theme.text);
                    dl.AddText(ctx.font->Face(), ctx.font->TexId(), { pb.x + ctx.S(8.f), by }, pv[i], fg);
                    if (hov && ctx.input.mouseClicked[0]) mProvider = i;
                    bx -= tw + ctx.S(4.f);
                }
            }

            // ── Corps : messages (bulles) défilables ──
            dl.AddRectFilled(body, ctx.theme.bgPrimary);
            DrawMessages(ctx, body);

            // ── Saisie ──
            dl.AddRectFilled(inR, ctx.theme.header);
            dl.AddRectFilled({ inR.x, inR.y, inR.w, 1.f }, ctx.theme.border);
            const float32 btnW = ctx.S(64.f);
            const NkRect field = { inR.x + pad, inR.y + pad, inR.w - btnW - pad * 3.f, inH - pad * 2.f };
            const NkRect send  = { field.x + field.w + pad, inR.y + pad, btnW, inH - pad * 2.f };
            const bool submitted = InputTextMultiline(ctx, "##aiInput", mInput, sizeof(mInput), field,
                                                      NkGuiInputFlags::None, sizeof(mInput) - 1);
            // Bouton Envoyer.
            const bool canSend = !mBusy && mInput[0] != 0;
            const bool sHov = canSend && NkGuiRectContains(send, ctx.input.mousePos);
            dl.AddRectFilled(send, canSend ? (sHov ? ctx.theme.buttonHover : ctx.theme.accent) : ctx.theme.button, ctx.theme.rounding);
            if (ctx.font && ctx.font->Valid()) {
                const char* sl = mBusy ? "…" : NkT("ai.send");
                const float32 tw = ctx.font->MeasureWidth(sl);
                dl.AddText(ctx.font->Face(), ctx.font->TexId(),
                           { send.x + (send.w - tw) * 0.5f, send.y + (send.h - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent() },
                           sl, canSend ? NkColor{ 255,255,255,255 } : ctx.theme.textDisabled);
            }
            const bool clickSend = canSend && sHov && ctx.input.mouseClicked[0];
            // Ctrl+Entrée dans le champ = envoyer.
            const bool keySend = canSend && submitted;
            if (clickSend || keySend) Send();
        }

    private:
        struct Msg { int32 role; NkString text; };   // role: 0 user, 1 assistant, 2 système/erreur
        NkCodeState*   mS;
        NkVector<Msg>  mMsgs;
        char           mInput[8192];
        int32          mProvider = 0;                 // 0 Claude, 1 Ollama, 2 Maison
        NkProcess          mProc;
        NkVector<NkString> mRespLines;                // accumule la sortie curl entre frames
        bool           mBusy = false;
        bool           mGreeted = false;
        float32        mScroll = 0.f;
        bool           mStick = true;                 // colle en bas tant qu'on n'a pas scrollé

        // ── Découpe `s` en lignes tenant dans maxW (mots ; repli caractère si besoin). ──
        static void WrapLines(NkGuiContext& ctx, const char* s, float32 maxW, NkVector<NkString>& out) {
            out.Clear();
            const bool valid = ctx.font && ctx.font->Valid();
            char line[1024]; int32 n = 0; int32 lastSpace = -1;
            auto push = [&]() { line[n] = 0; out.PushBack(NkString(line)); n = 0; lastSpace = -1; };
            for (const char* p = s; ; ++p) {
                const char c = *p;
                if (c == '\0') { if (n > 0 || out.Empty()) push(); break; }
                if (c == '\r') continue;
                if (c == '\n') { push(); continue; }
                if (n < 1022) { line[n] = c; if (c == ' ') lastSpace = n; ++n; line[n] = 0; }
                if (valid && ctx.font->MeasureWidth(line) > maxW && n > 1) {
                    if (lastSpace > 0 && lastSpace < n - 1) {
                        char save[1024]; int32 k = 0; for (int32 j = lastSpace + 1; j < n; ++j) save[k++] = line[j]; save[k] = 0;
                        line[lastSpace] = 0; out.PushBack(NkString(line));
                        n = 0; lastSpace = -1; for (int32 j = 0; save[j]; ++j) { line[n++] = save[j]; } line[n] = 0;
                    } else {
                        const char last = line[n - 1]; line[n - 1] = 0; out.PushBack(NkString(line));
                        n = 0; lastSpace = -1; line[n++] = last; line[n] = 0;
                    }
                }
            }
        }

        void DrawMessages(NkGuiContext& ctx, const NkRect& body) {
            auto& dl = ctx.DL();
            const float32 lineH = ctx.font && ctx.font->Valid() ? ctx.font->LineHeight() : 16.f;
            const float32 asc   = ctx.font && ctx.font->Valid() ? ctx.font->Ascent() : 12.f;
            const float32 pad = ctx.S(10.f), bubPad = ctx.S(8.f), gap = ctx.S(8.f);
            const float32 maxBub = body.w * 0.82f;
            const float32 textMaxW = maxBub - bubPad * 2.f;

            // 1) mesure de la hauteur totale.
            NkVector<NkVector<NkString>> wrapped; float32 total = pad;
            for (usize i = 0; i < mMsgs.Size(); ++i) {
                NkVector<NkString> ls; WrapLines(ctx, mMsgs[i].text.CStr(), textMaxW, ls);
                total += ls.Size() * lineH + bubPad * 2.f + gap;
                wrapped.PushBack(ls);
            }
            const float32 viewH = body.h;
            const float32 maxScroll = total > viewH ? (total - viewH) : 0.f;
            // molette.
            if (NkGuiRectContains(body, ctx.input.mousePos) && ctx.input.wheel != 0.f) {
                mScroll -= ctx.input.wheel * lineH * 3.f; ctx.input.wheel = 0.f; mStick = false;
            }
            if (mStick) mScroll = maxScroll;
            if (mScroll < 0.f) mScroll = 0.f; if (mScroll > maxScroll) mScroll = maxScroll;
            if (mScroll >= maxScroll - 1.f) mStick = true;

            // 2) rendu (clippé).
            dl.PushClipRect(body, true);
            float32 y = body.y + pad - mScroll;
            for (usize i = 0; i < mMsgs.Size(); ++i) {
                const Msg& m = mMsgs[i];
                const NkVector<NkString>& ls = wrapped[i];
                float32 tw = 0.f; for (usize k = 0; k < ls.Size(); ++k) { const float32 w = ctx.font ? ctx.font->MeasureWidth(ls[k].CStr()) : 0.f; if (w > tw) tw = w; }
                const float32 bw = tw + bubPad * 2.f;
                const float32 bh = ls.Size() * lineH + bubPad * 2.f;
                const bool user = (m.role == 0);
                const float32 bx = user ? (body.x + body.w - pad - bw) : (body.x + pad);
                if (y + bh >= body.y && y <= body.y + body.h) {
                    NkColor bg = user ? ctx.theme.buttonActive : (m.role == 2 ? ctx.theme.header : ctx.theme.panel);
                    dl.AddRectFilled({ bx, y, bw, bh }, bg, ctx.theme.rounding);
                    if (m.role != 0) dl.AddRect({ bx, y, bw, bh }, ctx.theme.border, 1.f);
                    const NkColor fg = user ? NkColor{ 255,255,255,255 } : (m.role == 2 ? ctx.theme.textDisabled : ctx.theme.text);
                    if (ctx.font && ctx.font->Valid())
                        for (usize k = 0; k < ls.Size(); ++k)
                            dl.AddText(ctx.font->Face(), ctx.font->TexId(), { bx + bubPad, y + bubPad + k * lineH + asc }, ls[k].CStr(), fg);
                }
                y += bh + gap;
            }
            dl.PopClipRect();
        }

        // ── JSON : échappe une chaîne pour un littéral JSON. ──
        static NkString JsonEsc(const char* s) {
            NkString o;
            for (const char* p = s; *p; ++p) {
                const unsigned char c = (unsigned char)*p;
                switch (c) {
                    case '\"': o += "\\\""; break;
                    case '\\': o += "\\\\"; break;
                    case '\n': o += "\\n";  break;
                    case '\r': o += "\\r";  break;
                    case '\t': o += "\\t";  break;
                    default: if (c < 0x20) { char b[8]; std::snprintf(b, sizeof(b), "\\u%04x", c); o += b; } else { char b[2] = { (char)c, 0 }; o += b; } break;
                }
            }
            return o;
        }
        // ── JSON : extrait la valeur string de la 1re occurrence de "key":"..." après `from`. ──
        static NkString JsonStr(const char* json, const char* key) {
            NkString pat = NkString("\"") + key + "\"";
            const char* k = std::strstr(json, pat.CStr());
            if (!k) return NkString();
            const char* p = k + pat.Size();
            while (*p && *p != ':') ++p; if (*p) ++p; while (*p == ' ' || *p == '\t' || *p == '\n') ++p;
            if (*p != '\"') return NkString(); ++p;
            NkString o;
            while (*p && *p != '\"') {
                if (*p == '\\' && p[1]) {
                    ++p; char c = *p;
                    if (c == 'n') o += "\n"; else if (c == 't') o += "\t"; else if (c == 'r') { }
                    else if (c == 'u' && p[1] && p[2] && p[3] && p[4]) { char h[5] = { p[1],p[2],p[3],p[4],0 }; long cp = std::strtol(h, nullptr, 16); char u8[5]; int32 n = NkEncodeU8((uint32)cp, u8); u8[n] = 0; o += u8; p += 4; }
                    else { char b[2] = { c, 0 }; o += b; }
                    ++p;
                } else { char b[2] = { *p, 0 }; o += b; ++p; }
            }
            return o;
        }

        NkString ReqPath() const { return (mS->root / ".nkcode" / "ai_req.json").ToString(); }

        NkString BuildJson() const {
            const bool ollama = (mProvider == 1);
            NkString j = "{";
            if (ollama) { j += "\"model\":\"llama3.2\",\"stream\":false,"; }
            else { j += "\"model\":\"claude-sonnet-5\",\"max_tokens\":1024,"; }
            j += "\"messages\":[";
            bool first = true;
            for (usize i = 0; i < mMsgs.Size(); ++i) {
                if (mMsgs[i].role != 0 && mMsgs[i].role != 1) continue;   // system/erreur exclus
                if (!first) j += ",";
                first = false;
                j += "{\"role\":\""; j += (mMsgs[i].role == 0 ? "user" : "assistant"); j += "\",\"content\":\"";
                j += JsonEsc(mMsgs[i].text.CStr()); j += "\"}";
            }
            j += "]}";
            return j;
        }

        void Send() {
            if (mBusy || mInput[0] == 0) return;
            if (mProvider == 2) { mMsgs.PushBack({ 2, NkString(NkT("ai.homesoon")) }); return; }
            mMsgs.PushBack({ 0, NkString(mInput) });
            mInput[0] = 0; mStick = true;

            // Corps de requête -> fichier temporaire (évite l'enfer des quotes en ligne de commande).
            const NkString path = ReqPath();
            const NkString body = BuildJson();
            FILE* fp = std::fopen(path.CStr(), "wb");
            if (!fp) { mMsgs.PushBack({ 2, NkString(NkT("ai.errtmp")) }); return; }
            std::fwrite(body.CStr(), 1, body.Size(), fp); std::fclose(fp);

            NkString cmd;
            if (mProvider == 1) {
                cmd = NkString("curl -s -X POST http://localhost:11434/api/chat -H \"content-type: application/json\" -d @\"") + path.CStr() + "\"";
            } else {
                const char* key = std::getenv("NKCODE_ANTHROPIC_KEY");
                if (!key || !*key) key = std::getenv("ANTHROPIC_API_KEY");
                if (!key || !*key) { mMsgs.PushBack({ 2, NkString(NkT("ai.errkey")) }); return; }
                cmd = NkString("curl -s -X POST https://api.anthropic.com/v1/messages -H \"x-api-key: ") + key
                    + "\" -H \"anthropic-version: 2023-06-01\" -H \"content-type: application/json\" -d @\"" + path.CStr() + "\"";
            }
            mRespLines.Clear();
            if (!mProc.Start(cmd)) { mMsgs.PushBack({ 2, NkString(NkT("ai.errbusy")) }); return; }
            mBusy = true;
        }

        void Poll() {
            if (!mBusy) return;
            mProc.Drain(mRespLines);          // accumule (Drain n'efface pas `out`)
            if (!mProc.Done()) return;
            mBusy = false;
            NkString raw; for (usize i = 0; i < mRespLines.Size(); ++i) raw += mRespLines[i].CStr();
            mRespLines.Clear();
            if (raw.Empty()) { mMsgs.PushBack({ 2, NkString(NkT("ai.errnet")) }); mStick = true; return; }
            const char* j = raw.CStr();
            // Erreur API ?
            if (std::strstr(j, "\"error\"")) {
                NkString em = JsonStr(j, "message");
                mMsgs.PushBack({ 2, (NkString(NkT("ai.errapi")) + " " + (em.Empty() ? raw.CStr() : em.CStr())) });
                mStick = true; return;
            }
            // Réponse : Claude -> "text" ; Ollama -> "content".
            NkString ans = (mProvider == 1) ? JsonStr(j, "content") : JsonStr(j, "text");
            if (ans.Empty()) ans = JsonStr(j, "content");
            if (ans.Empty()) ans = raw;   // dernier recours : brut
            mMsgs.PushBack({ 1, ans });
            mStick = true;
        }
    };

} // namespace nkcode
} // namespace nkentseu
