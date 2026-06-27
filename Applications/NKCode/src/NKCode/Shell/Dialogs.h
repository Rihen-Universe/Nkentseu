#pragma once
// =============================================================================
// Dialogs.h — Menus applicatifs (Projet / Deploiement) + dialogues modaux.
//   - Menu « Projet »      : Nouveau projet…, Nouveau workspace…, Proprietes…
//   - Menu « Deploiement » : Empaqueter, Deployer, Creer un installateur (stubs).
//   - Overlay modal        : assistant de creation (genere les .jenga).
// Le shell appelle DrawAppMenu (dans la barre de menus) et DrawOverlay (apres
// les panneaux). ctx.appModal est leve tant qu'un dialogue est ouvert.
// =============================================================================
#include "NKEditorKit/NkEditorKit.h"
#include "NKCode/Project/NkCodeState.h"
#include "NKCode/Project/NkCodeGen.h"

namespace nkentseu {
namespace nkcode {

    using namespace nkentseu;
    using namespace nkentseu::editorkit;
    using namespace nkentseu::nkgui;

    // Etat des dialogues modaux (un seul a la fois). 0 = aucun.
    struct NkCodeDialogs {
        NkCodeState* st = nullptr;
        enum Mode { None = 0, NewProject, NewWorkspace, Properties };
        int32   mode      = None;
        char    nameBuf[96] = {};
        int32   kindIdx   = 0;
        int32   langIdx   = 0;
        bool    justOpened = false;
        NkString status;          // message d'erreur/succes affiche dans le dialogue

        void Open(int32 m) { mode = m; justOpened = true; status.Clear(); if (m == NewProject || m == NewWorkspace) nameBuf[0] = '\0'; }
        void Close() { mode = None; }
    };

    // ── Menus de la barre (appeles dans BuildMenuBar via SetAppMenu) ──
    inline void DrawAppMenu(NkEditorFrameContext& ec, NkCodeDialogs* d) {
        if (!d) return;
        auto& ctx = ec.Ui();
        const bool hasWs = d->st && d->st->HasWorkspace();

        if (BeginMenu(ctx, "Projet")) {
            if (MenuItem(ctx, "Nouveau projet...", nullptr, hasWs)) d->Open(NkCodeDialogs::NewProject);
            if (MenuItem(ctx, "Nouveau workspace...")) d->Open(NkCodeDialogs::NewWorkspace);
            MenuItem(ctx, "Proprietes du projet...", nullptr, false);     // TODO #5 (editeur de proprietes)
            MenuItem(ctx, "Proprietes du workspace...", nullptr, false);  // TODO #5
            EndMenu(ctx);
        }
        if (BeginMenu(ctx, "Deploiement")) {
            MenuItem(ctx, "Empaqueter (jenga package)", nullptr, false);        // TODO #3
            MenuItem(ctx, "Deployer (jenga deploy)", nullptr, false);           // TODO #3
            MenuItem(ctx, "Creer un installateur (.jng)", nullptr, false);      // TODO #3
            MenuItem(ctx, "Gerer les emulateurs...", nullptr, false);           // TODO #4
            EndMenu(ctx);
        }
        // Leve le flag modal AVANT le masquage d'input du corps (la barre de menus
        // est dessinee avant cette decision dans la boucle du shell).
        ctx.appModal = (d->mode != NkCodeDialogs::None);
    }

    // ── Champ de saisie mono-ligne minimal pour l'overlay (positionne en absolu) ──
    // Edite `buf` quand `focused` ; renvoie via les codepoints tapes + Backspace.
    inline void NkOverlayTextField(NkGuiContext& ctx, NkGuiDrawList& dl, const NkGuiFont* f,
                                   const NkRect& r, char* buf, int32 cap, bool focused) {
        const float32 asc = f ? f->Ascent() : 0.f, lh = f ? f->LineHeight() : 0.f;
        dl.AddRectFilled(r, NkColor{ 22, 27, 34, 255 }, 4.f);
        dl.AddRect(r, focused ? NkColor{ 88, 166, 255, 255 } : NkColor{ 48, 54, 61, 255 }, 1.f);
        if (focused) {
            // Backspace
            if (ctx.input.KeyPressedRepeat(NkGuiKey::Backspace)) {
                int32 n = 0; while (buf[n]) ++n;
                if (n > 0) { // retire 1 octet (ASCII suffit pour un nom sanitize)
                    buf[n - 1] = '\0';
                }
            }
            // Caracteres tapes cette frame (ASCII imprimable)
            for (int32 i = 0; i < ctx.input.charCount; ++i) {
                const uint32 cp = ctx.input.chars[i];
                if (cp >= 32 && cp < 127) {
                    int32 n = 0; while (buf[n]) ++n;
                    if (n + 1 < cap) { buf[n] = (char)cp; buf[n + 1] = '\0'; }
                }
            }
        }
        if (f) {
            dl.AddText(f->Face(), f->TexId(), { r.x + 8.f, r.y + (r.h - lh) * 0.5f + asc },
                       buf[0] ? buf : "", NkColor{ 230, 237, 243, 255 });
            if (focused) {
                const float32 cw = buf[0] ? f->MeasureWidth(buf) : 0.f;
                dl.AddRectFilled({ r.x + 8.f + cw + 1.f, r.y + 5.f, 1.5f, r.h - 10.f }, NkColor{ 200, 210, 220, 255 });
            }
        }
    }

    // ── Overlay modal (appele apres les panneaux via SetOverlay) ──
    inline void DrawOverlay(NkEditorFrameContext& ec, NkCodeDialogs* d) {
        if (!d || d->mode == NkCodeDialogs::None) return;
        auto& ctx = ec.Ui();
        const NkGuiFont* f = ctx.font;
        if (!f || !f->Valid()) return;
        auto& dl = ctx.dlOverlay;
        const float32 W = (float32)ctx.viewW, H = (float32)ctx.viewH;
        const float32 asc = f->Ascent(), lh = f->LineHeight();
        const NkVec2 mp = ctx.input.mousePos;
        const bool   click = ctx.input.mouseClicked[0];
        auto hit = [&](const NkRect& r) { return NkGuiRectContains(r, mp); };
        auto text = [&](float32 x, float32 y, const char* s, const NkColor& c) { dl.AddText(f->Face(), f->TexId(), { x, y + asc }, s, c); };
        auto btn = [&](const NkRect& r, const char* s, bool en) -> bool {
            const bool hov = en && hit(r);
            dl.AddRectFilled(r, !en ? NkColor{ 30, 34, 40, 255 } : hov ? NkColor{ 56, 104, 184, 255 } : NkColor{ 40, 46, 54, 255 }, 5.f);
            dl.AddRect(r, NkColor{ 60, 66, 74, 255 }, 1.f);
            const float32 tw = f->MeasureWidth(s);
            text(r.x + (r.w - tw) * 0.5f, r.y + (r.h - lh) * 0.5f, s, en ? NkColor{ 230, 237, 243, 255 } : NkColor{ 110, 118, 126, 255 });
            return en && hov && click;
        };

        const bool isProj = (d->mode == NkCodeDialogs::NewProject);
        const float32 pw = 460.f, ph = isProj ? 320.f : 220.f, px = (W - pw) * 0.5f, py = (H - ph) * 0.5f;
        dl.AddRectFilled({ 0.f, 0.f, W, H }, NkColor{ 0, 0, 0, 150 });
        const NkRect panel = { px, py, pw, ph };
        // Fermeture par clic hors panneau (sauf frame d'ouverture).
        if (d->justOpened) d->justOpened = false;
        else if (click && !hit(panel)) { d->Close(); ctx.appModal = false; return; }
        dl.AddRectFilled(panel, NkColor{ 28, 33, 40, 255 }, 8.f);
        dl.AddRect(panel, NkColor{ 70, 78, 88, 255 }, 1.5f);

        const char* title = isProj ? "Nouveau projet" : "Nouveau workspace";
        text(px + 18.f, py + 16.f, title, NkColor{ 230, 237, 243, 255 });

        const float32 cx = px + 20.f;
        float32 y = py + 52.f;
        text(cx, y, "Nom", NkColor{ 160, 170, 180, 255 }); y += 22.f;
        NkOverlayTextField(ctx, dl, f, { cx, y, pw - 40.f, 28.f }, d->nameBuf, (int32)sizeof(d->nameBuf), true);
        y += 42.f;

        if (isProj) {
            int32 nk = 0, nl = 0; const NkKindDef* K = NkKinds(&nk); const NkLangDef* L = NkLangs(&nl);
            // Genre
            text(cx, y, "Genre", NkColor{ 160, 170, 180, 255 }); y += 22.f;
            // Liste verticale de genres (boutons radio simples)
            for (int32 i = 0; i < nk; ++i) {
                const NkRect r = { cx, y, pw - 40.f, 26.f };
                const bool sel = (d->kindIdx == i);
                dl.AddRectFilled(r, sel ? NkColor{ 38, 60, 92, 255 } : (hit(r) ? NkColor{ 33, 39, 48, 255 } : NkColor{ 24, 28, 34, 255 }), 4.f);
                if (sel) dl.AddRect(r, NkColor{ 88, 166, 255, 255 }, 1.f);
                text(r.x + 10.f, r.y + (r.h - lh) * 0.5f, K[i].label, sel ? NkColor{ 230, 237, 243, 255 } : NkColor{ 180, 188, 196, 255 });
                if (hit(r) && click) d->kindIdx = i;
                y += 30.f;
            }
            // Langage (combo horizontal de boutons)
            y += 4.f;
            text(cx, y, "Langage", NkColor{ 160, 170, 180, 255 }); y += 22.f;
            float32 lx = cx;
            for (int32 i = 0; i < nl; ++i) {
                const float32 bw = f->MeasureWidth(L[i].label) + 22.f;
                if (btn({ lx, y, bw, 26.f }, L[i].label, true)) d->langIdx = i;
                if (d->langIdx == i) dl.AddRect({ lx, y, bw, 26.f }, NkColor{ 88, 166, 255, 255 }, 1.5f);
                lx += bw + 8.f;
            }
            y += 34.f;
        }

        if (!d->status.Empty())
            text(cx, py + ph - 70.f, d->status.CStr(), NkColor{ 240, 120, 120, 255 });

        // ── Boutons Creer / Annuler ──
        const float32 by = py + ph - 42.f;
        if (btn({ px + pw - 110.f, by, 96.f, 30.f }, isProj ? "Creer" : "Creer", d->nameBuf[0] != '\0')) {
            NkString made;
            if (isProj) made = GenerateProject(d->st->root, NkPath(d->st->wsPaths[d->st->wsIdx].CStr()),
                                               d->nameBuf, d->kindIdx, d->langIdx);
            else        made = GenerateWorkspace(d->st->root, d->nameBuf);
            if (made.Empty()) {
                d->status = isProj ? "Echec : nom invalide ou dossier deja existant."
                                   : "Echec : nom invalide ou .jenga deja existant.";
            } else {
                d->st->RequestReload();
                d->st->ScanWorkspaces();
                d->st->OpenPath(NkPath(made.CStr()));   // ouvre le .jenga genere
                d->Close(); ctx.appModal = false; return;
            }
        }
        if (btn({ px + pw - 218.f, by, 96.f, 30.f }, "Annuler", true)) { d->Close(); ctx.appModal = false; return; }

        // Echap = annuler, Entree = creer (si nom valide)
        if (ctx.input.KeyPressed(NkGuiKey::Escape)) { d->Close(); ctx.appModal = false; return; }

        ctx.appModal = true;   // maintien
    }

} // namespace nkcode
} // namespace nkentseu
