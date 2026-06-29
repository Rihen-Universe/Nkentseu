#pragma once
// =============================================================================
// NkNewWorkspace.h — Assistant « Nouveau Workspace » (wizard 5 etapes) du
// launcher NKCode. D'apres le design Banani « NKCode IDE » (composants Wizard*)
// + la spec textuelle (sections 4 & 5).
//
// Mode PLEIN CADRE du launcher (meme barre de titre + sidebar nav, item
// « Nouveau Workspace » actif) ; le panneau central est remplace par ce wizard.
//
// PHASE 1 = MAQUETTE VISUELLE : barre d'etapes + navigation + Etape 1 complete ;
// Etapes 2..5 = pages titrees (a etoffer). Le fonctionnel (generation .jenga +
// creation des fichiers) viendra APRES la validation visuelle.
//
// L'etat (NkNewWsState) vit dans NkHomeState ; le panneau renvoie une action
// (0 = rien, 1 = annuler -> retour Accueil).
// =============================================================================
#include "NKCode/Shell/NkUi.h"
#include "NKCode/Shell/NkOpenWs.h"        // reutilise NkOwEdit (editeur caret) + NkOwIco
#include "NKCode/Project/NkCodeState.h"
#include "NKCode/Shell/Dialogs.h"
#include <cstdio>

namespace nkentseu {
namespace nkcode {

    // ── Un projet defini dans le wizard (etape 2 / dialog) ──
    struct NkWizProject {
        char     name[80] = "MonApp";
        int32    kind = 0;            // 0 consoleapp, 1 windowedapp, 2 staticlib, 3 sharedlib, 4 test
        int32    lang = 1;            // 0 C, 1 C++, 2 ObjC, 3 ObjC++, 4 Zig, 5 Rust
        int32    dialect = 3;         // index dans la table des dialectes
        char     location[200] = "app/";
        NkVector<NkString> sources;
        NkVector<NkString> includes;
        NkVector<NkString> dependsOn;
        bool     genMain = true, genReadme = true, genTest = false;
        bool     isStart = false;
        bool     dynVars = true;            // repertoires de build via variables dynamiques Jenga
        // — Filtres par plateforme (index 0 Windows, 1 Linux, 2 Android, 3 Web, 4 macOS) —
        struct OsFlt { bool on = false; int32 toolchain = -1; NkVector<NkString> links, defines; int32 androidKind = 1; int32 webMem = 1; };
        OsFlt    os[5];
        // — Filtres de configuration —
        NkVector<NkString> dbgDefines, relDefines;
        int32    dbgOpt = 0, relOpt = 1; bool dbgSym = true, relSym = false;
    };

    // ── Etat complet du wizard ──
    struct NkNewWsState {
        int32    step = 0;            // 0..4 (5 etapes)
        bool     inited = false;
        // Edition de texte partagee (un seul champ focus a la fois).
        int32    focus = -1, caret = 0; float32 blink = 0.f;
        // Combo deroulant ouvert (rendu differe en fin de panneau).
        int32    comboOpen = -1; NkRect comboR{}; const char* const* comboOpts = nullptr; int32 comboN = 0; int32* comboSel = nullptr;
        bool     comboJustOpened = false;   // ignore la frame d'ouverture pour la fermeture au clic exterieur
        // Defilement du corps du wizard (vertical + horizontal).
        float32  scroll = 0.f, scrollMax = 0.f, barOff = 0.f; bool barDrag = false;
        float32  hscroll = 0.f, hscrollMax = 0.f, hbarOff = 0.f; bool hbarDrag = false;
        // — Etape 1 : Workspace —
        char     wsName[120]   = "MonWorkspace";
        char     location[400] = {};       // dossier PARENT ou creer le workspace
        char     jengaFile[160] = {};
        bool     jengaManual = false;
        char     locBase[400] = {};
        bool     cfgDebug = true, cfgRelease = true;
        struct CustomCfg { NkString name; bool on = true; };
        NkVector<CustomCfg> customCfgs;        // configs ajoutees : cochables (on/off), pas retirees au clic
        char     newCfgName[60] = "Profiling";
        int32    newCfgBase = 1;            // 0 Debug, 1 Release (combo « Basee sur »)
        bool     gitInit = true, gitIgnore = true, gitCommit = false;
        // — Navigateur de dossiers (bouton Parcourir) —
        NkOpenWsState picker; bool picking = false;
        // — Etape 2 : Projets —
        NkVector<NkWizProject> projects;
        int32    startProj = -1;
        int32    dragProj = -1;                // reordonnancement (ordre de build) : index glisse
        // dialog « Ajouter / Editer un projet » (modale large, 3 etapes internes)
        bool     projDlg = false; int32 projEditIdx = -1; int32 projStep = 0; NkWizProject projDraft;
        char     projSrcAdd[160] = {}, projIncAdd[160] = {};
        char     projFltAdd[160] = {}; int32 projFltTarget = -1;   // ajout dans une liste de filtre (links/defines)
        float32  projScroll = 0.f, projScrollMax = 0.f;
        bool     projAdvanced = false;
        void OpenProjDlg(int32 editIdx) {
            projDlg = true; projEditIdx = editIdx; projStep = 0; focus = -1; comboOpen = -1; projScroll = 0.f;
            projSrcAdd[0] = projIncAdd[0] = projFltAdd[0] = '\0'; projFltTarget = -1;
            if (editIdx >= 0 && editIdx < (int32)projects.Size()) projDraft = projects[editIdx];
            else { NkWizProject p; p.sources.PushBack(NkString("src/**.cpp")); p.sources.PushBack(NkString("src/**.c"));
                   p.includes.PushBack(NkString("include")); projDraft = p; }
        }
        void CommitProjDlg() {
            if (!projDraft.name[0] || !ValidName(projDraft.name)) return;
            if (projEditIdx >= 0 && projEditIdx < (int32)projects.Size()) projects[projEditIdx] = projDraft;
            else projects.PushBack(projDraft);
            projDlg = false;
        }
        // — Etape 3 : Toolchains (placeholder) —
        bool     tcAuto = true;
        // — Etape 4 : Plateformes —
        bool     osWin = true, osLinux = true, osMac = false, osAndroid = true, osWeb = true,
                 osIos = false, osHarmony = false;
        bool     archX64 = true, archArm64 = true, archWasm32 = true, archX86 = false, archArm = false;

        void EnsureInit(NkCodeState* st) {
            if (inited) return; inited = true; (void)st;
            const NkString home = NkOpenWsState::Home();
            NkString base = (NkPath(home.CStr()) / "Projects").ToString();
            if (!NkDirectory::Exists(base.CStr())) base = home;
            int32 n = 0; for (; base.CStr()[n] && n + 1 < (int32)sizeof(locBase); ++n) locBase[n] = base.CStr()[n]; locBase[n] = '\0';
            std::snprintf(location, sizeof(location), "%s", locBase);   // emplacement = PARENT par defaut
            SyncDerived();
        }
        // jengaFile derive du nom (sauf override manuel). L'emplacement reste le PARENT saisi.
        void SyncDerived() { if (!jengaManual) std::snprintf(jengaFile, sizeof(jengaFile), "%s.jenga", wsName); }
        // Dossier final = <emplacement>/<NomWorkspace> (toujours append le nom).
        NkString FinalFolder() const { return (NkPath(location) / wsName).ToString(); }

        static bool ValidName(const char* s) {
            if (!s || !*s) return false;
            for (const char* p = s; *p; ++p) { const char c = *p;
                if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') return false; }
            return true;
        }
        bool Step1Valid() const {
            if (!ValidName(wsName)) return false;
            if (!jengaFile[0] || !ValidName(jengaFile) || !NkCodeState::EndsWithI(jengaFile, ".jenga")) return false;
            if (!location[0]) return false;
            return true;
        }
        void AddCustomCfg() {
            if (!newCfgName[0] || !ValidName(newCfgName)) return;
            if (StrEq(newCfgName, "Debug") || StrEq(newCfgName, "Release")) return;   // doublon
            for (usize i = 0; i < customCfgs.Size(); ++i) if (StrEq(customCfgs[i].name.CStr(), newCfgName)) { customCfgs[i].on = true; return; }
            CustomCfg c; c.name = newCfgName; c.on = true; customCfgs.PushBack(c);
        }
        void SetFocus(int32 id, const char* buf) {
            focus = id; blink = 0.f; int32 n = 0; while (buf[n]) ++n; caret = n;
        }

        // ── Generation du contenu .jenga depuis l'etat du wizard ──
        NkString BuildJenga() const {
            NkString s;
            s += "from Jenga import *\n";
            s += "from Jenga.GlobalToolchains import RegisterJengaGlobalToolchains\n\n";
            s += "with workspace(\""; s += wsName; s += "\"):\n";
            s += "    RegisterJengaGlobalToolchains()\n";
            // configurations
            { bool first = true; s += "    configurations([";
              auto addc = [&](const char* n) { if (!first) s += ", "; s += "\""; s += n; s += "\""; first = false; };
              if (cfgDebug) addc("Debug"); if (cfgRelease) addc("Release");
              for (usize i = 0; i < customCfgs.Size(); ++i) if (customCfgs[i].on) addc(customCfgs[i].name.CStr());
              s += "])\n"; }
            // targetoses
            s += "    targetoses([\n";
            { auto addos = [&](bool on, const char* e) { if (on) { s += "        TargetOS."; s += e; s += ",\n"; } };
              addos(osWin, "WINDOWS"); addos(osLinux, "LINUX"); addos(osMac, "MACOS"); addos(osAndroid, "ANDROID");
              addos(osWeb, "WEB"); addos(osIos, "IOS"); addos(osHarmony, "HARMONYOS"); }
            s += "    ])\n";
            // targetarchs
            s += "    targetarchs([\n";
            { auto adda = [&](bool on, const char* e) { if (on) { s += "        TargetArch."; s += e; s += ",\n"; } };
              adda(archX64, "X86_64"); adda(archArm64, "ARM64"); adda(archWasm32, "WASM32"); adda(archX86, "X86"); adda(archArm, "ARM"); }
            s += "    ])\n";
            if (startProj >= 0 && startProj < (int32)projects.Size()) { s += "    startproject(\""; s += projects[startProj].name; s += "\")\n"; }
            // projets
            const char* kinds[] = { "consoleapp", "windowedapp", "staticlib", "sharedlib", "test" };
            for (usize i = 0; i < projects.Size(); ++i) {
                const NkWizProject& p = projects[i];
                s += "\n    with project(\""; s += p.name; s += "\"):\n";
                s += "        "; s += (p.kind >= 0 && p.kind < 5 ? kinds[p.kind] : "consoleapp"); s += "()\n";
                s += "        location(\""; s += p.location; s += "\")\n";
                s += "        files([\"src/**.cpp\", \"src/**.c\"])\n";
                s += "        includedirs([\"include\"])\n";
            }
            return s;
        }
        // Genere le bloc « with project(...) » d'un projet (avec filtres). Indentation de base = 0.
        NkString BuildProjectJenga(const NkWizProject& p) const {
            static const char* KINDS[] = { "consoleapp", "windowedapp", "staticlib", "sharedlib", "test" };
            static const char* LANGS[] = { "C", "C++", "ObjC", "ObjC++", "Zig", "Rust" };
            static const char* DIALS[] = { "C++11", "C++14", "C++17", "C++20", "C++23" };
            static const char* TCS[]   = { "(auto)", "clang-mingw", "zig-linux-x64", "android-ndk", "emscripten", "clang-native", "clang-cl" };
            static const char* OPT[]   = { "Off", "Size", "Speed", "Full" };
            static const char* OSF[]   = { "system:Windows", "system:Linux", "system:Android", "system:Web", "system:macOS" };
            static const int32 MEM[]   = { 16, 32, 64, 128, 256 };
            auto lst = [](NkString& s, const NkVector<NkString>& v, const char* prefix) {
                s += "["; for (usize i = 0; i < v.Size(); ++i) { if (i) s += ", "; s += "\""; s += prefix; s += v[i].CStr(); s += "\""; } s += "]"; };
            NkString s;
            s += "with project(\""; s += p.name; s += "\"):\n";
            s += "    "; s += (p.kind >= 0 && p.kind < 5 ? KINDS[p.kind] : "consoleapp"); s += "()\n";
            s += "    language(\""; s += (p.lang >= 0 && p.lang < 6 ? LANGS[p.lang] : "C++"); s += "\")\n";
            s += "    dialect(\""; s += (p.dialect >= 0 && p.dialect < 5 ? DIALS[p.dialect] : "C++20"); s += "\")\n";
            s += "    location(\""; s += p.location; s += "\")\n";
            s += "    files("; lst(s, p.sources, p.location); s += ")\n";
            s += "    includedirs("; lst(s, p.includes, p.location); s += ")\n";
            if (p.dynVars) {
                s += "    objdir(\"%{wks.location}/Build/Obj/%{cfg.buildcfg}-%{cfg.system}/%{prj.name}\")\n";
                s += "    targetdir(\"%{wks.location}/Build/Bin/%{cfg.buildcfg}-%{cfg.system}/%{prj.name}\")\n";
            }
            if (!p.dependsOn.Empty()) { s += "    dependson("; lst(s, p.dependsOn, ""); s += ")\n"; }
            for (int32 i = 0; i < 4; ++i) {
                const NkWizProject::OsFlt& o = p.os[i];
                if (o.toolchain <= 0 && o.links.Empty() && o.defines.Empty() && !(i == 2 && o.androidKind != 1)) continue;
                s += "\n    with filter(\""; s += OSF[i]; s += "\"):\n";
                if (i == 2 && o.androidKind != 1) { s += "        "; s += (o.androidKind >= 0 && o.androidKind < 5 ? KINDS[o.androidKind] : "windowedapp"); s += "()\n"; }
                if (o.toolchain > 0) { s += "        usetoolchain(\""; s += TCS[o.toolchain]; s += "\")\n"; }
                if (i == 3) { char mb[12]; std::snprintf(mb, sizeof(mb), "%d", (o.webMem >= 0 && o.webMem < 5) ? MEM[o.webMem] : 32); s += "        emscripteninitialmemory("; s += mb; s += ")\n"; }
                if (!o.links.Empty())   { s += "        links("; lst(s, o.links, ""); s += ")\n"; }
                if (!o.defines.Empty()) { s += "        defines("; lst(s, o.defines, ""); s += ")\n"; }
            }
            auto cfgBlk = [&](const char* filt, const NkVector<NkString>& defs, int32 opt, bool sym) {
                s += "\n    with filter(\""; s += filt; s += "\"):\n";
                if (!defs.Empty()) { s += "        defines("; lst(s, defs, ""); s += ")\n"; }
                s += "        optimize(\""; s += (opt >= 0 && opt < 4 ? OPT[opt] : "Off"); s += "\")\n";
                s += "        symbols("; s += (sym ? "True" : "False"); s += ")\n";
            };
            cfgBlk("config:Debug", p.dbgDefines, p.dbgOpt, p.dbgSym);
            cfgBlk("config:Release", p.relDefines, p.relOpt, p.relSym);
            return s;
        }

        // Cree le dossier + le .jenga (+ .gitignore), puis charge le workspace. Renvoie true si OK.
        bool Generate(NkCodeDialogs* dlg) {
            if (!Step1Valid()) return false;
            const NkString folder = FinalFolder();
            if (!NkDirectory::CreateRecursive(folder.CStr())) return false;
            const NkString jpath = (NkPath(folder.CStr()) / jengaFile).ToString();
            if (!NkFile::WriteAllText(NkPath(jpath), BuildJenga())) return false;
            if (gitIgnore) NkFile::WriteAllText((NkPath(folder.CStr()) / ".gitignore"),
                NkString("Build/\n.jenga/\n*.o\n*.obj\n*.exe\n*.dll\n*.so\n*.a\n*.lib\n"));
            if (dlg) dlg->DoLoad(NkPath(folder.CStr()));   // ouvre dans l'editeur
            return true;
        }
    };

    // ── Combo deroulant fonctionnel (le menu est rendu en fin de panneau, par-dessus). ──
    inline void NkWizCombo(const NkUi& u, const NkRect& r, int32 id, NkNewWsState* w,
                           const char* const* opts, int32 n, int32* sel, bool blockBg) {
        const bool open = w->comboOpen == id;
        u.Panel(r, NkCol::input, open ? NkCol::primary : NkCol::border, NkR::md * u.S);
        const int32 s = (*sel >= 0 && *sel < n) ? *sel : 0;
        u.TextV(r.x + u.s(10), r.y, r.h, opts[s], NkCol::foreground);
        u.Icon("chevron-down", { r.x + r.w - u.s(18), r.y + (r.h - u.s(10)) * 0.5f, u.s(10), u.s(10) }, NkCol::mutedFg);
        if (!blockBg && u.Hit(r) && u.click) {
            if (open) w->comboOpen = -1;
            else { w->comboOpen = id; w->comboR = r; w->comboOpts = opts; w->comboN = n; w->comboSel = sel; w->comboJustOpened = true; }
        }
    }

    // ── Helpers de mise en page du wizard ──

    // Barre d'etapes : pastilles RONDES numerotees + connecteurs (fait=accent, actif=primary).
    // Restent rondes en tout etat (cercle plein, pas de bord carre).
    inline void NkWizSteps(const NkUi& u, const NkRect& r, int32 current, const char* const* labels, int32 count) {
        u.Rect(r, NkCol::sidebar);
        u.Rect({ r.x, r.y + r.h - 1.f, r.w, 1.f }, NkCol::border);
        const float32 dotD = u.s(30);
        const float32 segW = u.s(54);
        const float32 cellW = u.s(94);
        const float32 totalW = count * cellW + (count - 1) * segW;
        float32 x = r.x + (r.w - totalW) * 0.5f; if (x < r.x + u.s(20)) x = r.x + u.s(20);
        const float32 cy = r.y + u.s(16);          // haut de la pastille (plus de hauteur -> couvre le libelle)
        for (int32 i = 0; i < count; ++i) {
            const bool done = i < current, active = i == current;
            if (i > 0) {
                const float32 sx = x - segW;
                u.dl->AddRectFilled({ sx + u.s(2), cy + dotD * 0.5f - u.s(1.5f), segW - u.s(4), u.s(3) }, (done || active) ? NkCol::accent : NkCol::border, u.s(1.5f));
            }
            const NkRect dot = { x + (cellW - dotD) * 0.5f, cy, dotD, dotD };
            // halo discret pour l'etape active (reste rond)
            if (active) u.dl->AddRectFilled({ dot.x - u.s(3), dot.y - u.s(3), dotD + u.s(6), dotD + u.s(6) }, NkColor{ 15,115,213,60 }, (dotD + u.s(6)) * 0.5f);
            const NkColor bg = done ? NkCol::accent : (active ? NkCol::primary : NkCol::muted);
            u.dl->AddRectFilled(dot, bg, dotD * 0.5f);
            if (done) u.Icon("check", { dot.x + u.s(8), dot.y + u.s(8), dotD - u.s(16), dotD - u.s(16) }, NkCol::primaryFg);
            else { char num[4]; std::snprintf(num, sizeof(num), "%d", i + 1);
                   const float32 tw = u.TextW(num);
                   u.Text(dot.x + (dotD - tw) * 0.5f, dot.y + (dotD - u.Lh()) * 0.5f, num, active ? NkCol::primaryFg : NkCol::mutedFg); }
            const float32 lw = u.TextW(labels[i]);
            u.Text(x + (cellW - lw) * 0.5f, cy + dotD + u.s(8), labels[i],
                   active ? NkCol::foreground : (done ? NkCol::accent : NkCol::mutedFg));
            x += cellW + segW;
        }
    }

    // Libelle de section (majuscules, espace lettres).
    inline void NkWizLabel(const NkUi& u, float32 x, float32 y, const char* t) {
        u.Text(x, y, t, NkCol::mutedFg);
    }
    // Ligne d'indice (fleche accent + texte) sous un champ -> code .jenga genere.
    inline void NkWizHint(const NkUi& u, float32 x, float32 y, float32 w, const char* t) {
        u.Icon("arrow-right", { x, y, u.s(14), u.s(14) }, NkCol::accent);
        u.TextEllipsis(x + u.s(20), y, w - u.s(20), t, NkCol::mutedFg);
    }
    // Case a cocher (style accent, arrondie/lisse) ; renvoie true si bascule.
    inline bool NkWizCheck(const NkUi& u, float32 x, float32 cy, const char* label, bool val, bool blockBg) {
        const float32 bs = u.s(18);                                   // un peu plus grande
        const NkRect b = { x, cy - bs * 0.5f, bs, bs };
        const float32 lw = u.TextW(label);
        const NkRect hit = { x - u.s(3), cy - bs * 0.5f - u.s(4), bs + u.s(10) + lw + u.s(10), bs + u.s(8) };
        const bool hv = u.Hit(hit);
        u.Panel(b, val ? NkCol::accent : NkCol::input, val ? NkCol::accent : NkCol::border, NkR::md * u.S);  // coins lisses
        if (val) { const float32 s = bs;
            u.dl->AddLine({ b.x + s * 0.26f, b.y + s * 0.52f }, { b.x + s * 0.44f, b.y + s * 0.70f }, NkCol::primaryFg, u.s(2.1f));
            u.dl->AddLine({ b.x + s * 0.44f, b.y + s * 0.70f }, { b.x + s * 0.76f, b.y + s * 0.30f }, NkCol::primaryFg, u.s(2.1f)); }
        u.Text(x + bs + u.s(10), cy - u.Lh() * 0.5f, label, hv ? NkCol::foreground : NkCol::sidebarFg);
        return hv && u.click && !blockBg;
    }

    // Champ texte avec focus + caret (reutilise NkOwEdit). leftPad reserve l'icone eventuelle.
    inline void NkWizField(const NkUi& u, const NkRect& r, char* buf, int32 cap, int32 id,
                           NkNewWsState* w, float32 dt, bool blockBg, float32 leftPad, bool readOnly = false) {
        const bool foc = (w->focus == id) && !readOnly;
        u.Panel(r, readOnly ? NkCol::muted : NkCol::input, foc ? NkCol::primary : NkCol::border, NkR::md * u.S);
        if (foc) NkOwEdit(u, r, buf, cap, w->caret, w->blink, dt, leftPad);
        else u.TextEllipsis(r.x + leftPad, r.y + (r.h - u.Lh()) * 0.5f, r.w - leftPad - u.s(8), buf, (readOnly ? NkCol::mutedFg : (buf[0] ? NkCol::foreground : NkCol::mutedFg)));
        if (!readOnly && !blockBg && u.Hit(r) && u.click) w->SetFocus(id, buf);
    }

    // ── Pied de page : Precedent / Annuler / Suivant|Creer. Renvoie -1 prec, 0 rien, 1 annuler, 2 suivant. ──
    inline int32 NkWizFooter(const NkUi& u, const NkRect& r, int32 step, int32 lastStep, bool blockBg, bool nextEnabled,
                             const char* lastLabel = "Creer le Workspace", bool showCreateNow = true) {
        u.Rect(r, NkCol::sidebar);
        u.Rect({ r.x, r.y, r.w, 1.f }, NkCol::border);
        int32 act = 0;
        const float32 bh = u.s(34), by = r.y + (r.h - bh) * 0.5f;
        float32 rx = r.x + r.w - u.s(20);
        // Suivant / Creer (grise si etape invalide)
        const bool last = step == lastStep;
        const float32 nextW = last ? u.s(180) : u.s(120);
        const NkRect nextR = { rx - nextW, by, nextW, bh }; rx -= nextW + u.s(8);
        { const bool hv = nextEnabled && !blockBg && u.Hit(nextR);
          const NkColor nbg = !nextEnabled ? NkCol::muted : (hv ? NkColor{ 35,135,233,255 } : NkCol::primary);
          const NkColor nfg = nextEnabled ? NkCol::primaryFg : NkCol::mutedFg;
          u.Rect(nextR, nbg, NkR::md * u.S);
          const char* lbl = last ? lastLabel : "Suivant";
          const float32 tw = u.TextW(lbl);
          u.TextV(nextR.x + (nextW - tw - u.s(18)) * 0.5f, nextR.y, bh, lbl, nfg);
          u.Icon(last ? "check" : "arrow-right", { nextR.x + (nextW - tw - u.s(18)) * 0.5f + tw + u.s(6), nextR.y + (bh - u.s(13)) * 0.5f, u.s(13), u.s(13) }, nfg);
          if (hv && u.click) act = 2; }
        // Creer maintenant (sur les etapes NON finales : cree avec les valeurs par defaut)
        if (!last && showCreateNow) {
            const float32 crw = u.s(150); const NkRect crR = { rx - crw, by, crw, bh }; rx -= crw + u.s(8);
            const bool hv = nextEnabled && !blockBg && u.Hit(crR);
            u.Rect(crR, !nextEnabled ? NkCol::muted : (hv ? NkColor{ 46,170,90,255 } : NkCol::success), NkR::md * u.S);
            const float32 tw = u.TextW("Creer le Workspace");
            u.Icon("check", { crR.x + (crw - tw - u.s(20)) * 0.5f, crR.y + (bh - u.s(13)) * 0.5f, u.s(13), u.s(13) }, nextEnabled ? NkCol::primaryFg : NkCol::mutedFg);
            u.TextV(crR.x + (crw - tw - u.s(20)) * 0.5f + u.s(20), crR.y, bh, "Creer le Workspace", nextEnabled ? NkCol::primaryFg : NkCol::mutedFg);
            if (hv && u.click) act = 3;
        }
        // Annuler
        const float32 cw = u.s(96); const NkRect cancelR = { rx - cw, by, cw, bh }; rx -= cw + u.s(8);
        if (u.Button(cancelR, "Annuler", NkCol::muted, NkCol::hover, NkCol::foreground, NkR::md * u.S) && !blockBg) act = 1;
        // Precedent (a gauche)
        if (step > 0) {
            const NkRect prevR = { r.x + u.s(20), by, u.s(120), bh };
            const bool hv = !blockBg && u.Hit(prevR);
            u.Rect(prevR, hv ? NkCol::hover : NkCol::muted, NkR::md * u.S);
            u.Icon("arrow-left", { prevR.x + u.s(14), prevR.y + (bh - u.s(13)) * 0.5f, u.s(13), u.s(13) }, NkCol::foreground);
            u.TextV(prevR.x + u.s(34), prevR.y, bh, "Precedent", NkCol::foreground);
            if (hv && u.click) act = -1;
        }
        return act;
    }

    // Petit selecteur visuel (texte + chevron). Visuel uniquement pour l'instant.
    inline void NkWizSelect(const NkUi& u, const NkRect& r, const char* value) {
        u.Panel(r, NkCol::input, NkCol::border, NkR::md * u.S);
        u.TextV(r.x + u.s(10), r.y, r.h, value, NkCol::foreground);
        u.Icon("chevron-down", { r.x + r.w - u.s(18), r.y + (r.h - u.s(10)) * 0.5f, u.s(10), u.s(10) }, NkCol::mutedFg);
    }

    // ── ETAPE 1 : Informations du Workspace (2 colonnes). Renvoie la hauteur du contenu. ──
    inline float32 NkWizStep1(const NkUi& u, const NkRect& body, NkNewWsState* w, NkCodeDialogs* dlg, float32 dt, bool blockBg, const NkIcons& ic) {
        const float32 lh = u.Lh();
        // Colonnes : centre (champs principaux) + droite (Git + Nouvelle config).
        const float32 padL = u.s(28);
        const float32 rightW = (body.w > u.s(720)) ? u.s(290) : u.s(250);
        const float32 cx = body.x + padL;
        const float32 rx = body.x + body.w - u.s(28) - rightW;
        const float32 cw = (rx - u.s(34)) - cx;
        const float32 fH = u.s(32);

        const float32 secGap = u.s(38);     // espace GENEREUX entre sous-parties
        const float32 labGap = u.s(22);      // libelle -> champ
        const float32 hintGap = u.s(10);     // champ -> indice

        // ───────── COLONNE CENTRALE ─────────
        float32 y = body.y + u.s(22);
        // NOM DU WORKSPACE
        NkWizLabel(u, cx, y, "NOM DU WORKSPACE"); y += labGap;
        NkWizField(u, { cx, y, cw, fH }, w->wsName, (int32)sizeof(w->wsName), 1, w, dt, blockBg, u.s(10)); y += fH + hintGap;
        { char hint[220]; std::snprintf(hint, sizeof(hint), "Correspond a workspace(\"%s\") dans le .jenga", w->wsName);
          NkWizHint(u, cx, y, cw, hint); } y += secGap;

        // EMPLACEMENT
        NkWizLabel(u, cx, y, "EMPLACEMENT"); y += labGap;
        const float32 browseW = u.s(110);
        NkWizField(u, { cx, y, cw - browseW - u.s(8), fH }, w->location, (int32)sizeof(w->location), 2, w, dt, blockBg, u.s(10));
        { const NkRect br = { cx + cw - browseW, y, browseW, fH };
          const bool hv = !blockBg && u.Hit(br);
          u.Rect(br, hv ? NkCol::hover : NkCol::muted, NkR::md * u.S);
          NkOwIco(u, ic.ouvrirDossier, "folder-open", { br.x + u.s(14), br.y + (fH - u.s(14)) * 0.5f, u.s(14), u.s(14) }, NkCol::mutedFg);
          u.TextV(br.x + u.s(34), br.y, fH, "Parcourir", NkCol::foreground);
          if (hv && u.click && !blockBg && dlg) { w->focus = -1; dlg->BrowseInto(w->location, (int32)sizeof(w->location), "Emplacement du workspace"); } }
        y += fH + hintGap;
        { char fh[480]; const NkString ff = w->FinalFolder();
          std::snprintf(fh, sizeof(fh), "Dossier final : %s (cree s'il n'existe pas)", ff.CStr());
          NkWizHint(u, cx, y, cw, fh); } y += secGap;

        // NOM DU FICHIER .JENGA
        NkWizLabel(u, cx, y, "NOM DU FICHIER .JENGA"); y += labGap;
        NkWizField(u, { cx, y, cw - u.s(96), fH }, w->jengaFile, (int32)sizeof(w->jengaFile), 3, w, dt, blockBg, u.s(10));
        u.TextV(cx + cw - u.s(90), y, fH, w->jengaManual ? "(manuel)" : "(auto-rempli)", NkCol::mutedFg);
        y += fH + secGap;

        // CONFIGURATIONS DE BUILD (Debug + Release par defaut ; l'utilisateur ajoute le reste)
        NkWizLabel(u, cx, y, "CONFIGURATIONS DE BUILD"); y += labGap;
        const float32 cfgBoxH = u.s(80) + (w->customCfgs.Empty() ? 0.f : u.s(26) * ((w->customCfgs.Size() + 2) / 3));
        { const NkRect box = { cx, y, cw, cfgBoxH };
          u.Panel(box, NkCol::input, NkCol::border, NkR::md * u.S);
          float32 bx = box.x + u.s(16); const float32 bcy = box.y + u.s(24);
          if (NkWizCheck(u, bx, bcy, "Debug", w->cfgDebug, blockBg))     w->cfgDebug = !w->cfgDebug;     bx += u.s(100);
          if (NkWizCheck(u, bx, bcy, "Release", w->cfgRelease, blockBg)) w->cfgRelease = !w->cfgRelease; bx += u.s(108);
          for (usize i = 0; i < w->customCfgs.Size(); ++i) {           // config custom : clic = (de)cocher
              if (NkWizCheck(u, bx, bcy, w->customCfgs[i].name.CStr(), w->customCfgs[i].on, blockBg)) w->customCfgs[i].on = !w->customCfgs[i].on;
              bx += u.TextW(w->customCfgs[i].name.CStr()) + u.s(40);
          }
          // (l'ajout de configuration custom se fait via le formulaire « Nouvelle configuration » a droite)
          u.TextV(box.x + u.s(16), box.y + cfgBoxH - u.s(28), u.s(22), "Ajoutez-en via « Nouvelle configuration » -->", NkCol::mutedFg); }
        y += cfgBoxH + hintGap;
        { NkString cfgs; auto add = [&](bool on, const char* n) { if (on) { if (!cfgs.Empty()) cfgs += "\", \""; cfgs += n; } };
          add(w->cfgDebug, "Debug"); add(w->cfgRelease, "Release");
          for (usize i = 0; i < w->customCfgs.Size(); ++i) add(w->customCfgs[i].on, w->customCfgs[i].name.CStr());
          char hint[280]; std::snprintf(hint, sizeof(hint), "Correspond a configurations([\"%s\"]) dans le .jenga", cfgs.CStr());
          NkWizHint(u, cx, y, cw, hint); } y += secGap;

        // PROJET DE DEMARRAGE
        NkWizLabel(u, cx, y, "PROJET DE DEMARRAGE (startproject)"); y += labGap;
        { const NkRect pr = { cx, y, cw, fH };
          u.Panel(pr, NkCol::muted, NkCol::border, NkR::md * u.S);
          const char* txt = (w->startProj >= 0 && w->startProj < (int32)w->projects.Size())
              ? w->projects[w->startProj].name : "(sera defini apres creation des projets)";
          u.TextEllipsis(pr.x + u.s(10), pr.y + (fH - lh) * 0.5f, cw - u.s(18), txt, NkCol::mutedFg); }

        // ───────── COLONNE DROITE ─────────
        float32 ry = body.y + u.s(22);
        // GIT (dans un bloc)
        NkWizLabel(u, rx, ry, "GIT"); ry += labGap;
        { const NkRect box = { rx, ry, rightW, u.s(108) };
          u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
          float32 gy = box.y + u.s(20);
          if (NkWizCheck(u, box.x + u.s(14), gy, "Initialiser un depot Git", w->gitInit, blockBg)) w->gitInit = !w->gitInit; gy += u.s(30);
          if (NkWizCheck(u, box.x + u.s(14), gy, "Creer un .gitignore Jenga standard", w->gitIgnore, blockBg)) w->gitIgnore = !w->gitIgnore; gy += u.s(30);
          if (NkWizCheck(u, box.x + u.s(14), gy, "Premier commit automatique", w->gitCommit, blockBg)) w->gitCommit = !w->gitCommit; }
        ry += u.s(108) + secGap;

        // NOUVELLE CONFIGURATION (formulaire fonctionnel : ajoute a CONFIGURATIONS)
        NkWizLabel(u, rx, ry, "NOUVELLE CONFIGURATION"); ry += labGap;
        { const NkRect box = { rx, ry, rightW, u.s(188) };
          u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
          float32 by = box.y + u.s(14);
          u.Text(box.x + u.s(14), by, "Nom", NkCol::mutedFg); by += u.s(20);
          NkWizField(u, { box.x + u.s(14), by, rightW - u.s(28), fH }, w->newCfgName, (int32)sizeof(w->newCfgName), 10, w, dt, blockBg, u.s(10));
          by += fH + u.s(14);
          u.Text(box.x + u.s(14), by, "Basee sur", NkCol::mutedFg); by += u.s(20);
          { static const char* baseOpts[] = { "Debug", "Release" };
            NkWizCombo(u, { box.x + u.s(14), by, rightW - u.s(28), fH }, 20, w, baseOpts, 2, &w->newCfgBase, blockBg); }
          by += fH + u.s(16);
          const float32 bw = u.s(86), bh = u.s(30);
          if (u.Button({ box.x + rightW - u.s(14) - bw * 2 - u.s(8), by, bw, bh }, "Annuler", NkCol::muted, NkCol::hover, NkCol::foreground, NkR::md * u.S) && !blockBg)
              { w->newCfgName[0] = '\0'; if (w->focus == 10) w->focus = -1; }
          { const NkRect ar = { box.x + rightW - u.s(14) - bw, by, bw, bh };
            const bool ahv = !blockBg && u.Hit(ar);
            u.Rect(ar, ahv ? NkColor{ 35,135,233,255 } : NkCol::primary, NkR::md * u.S);
            u.TextV(ar.x + (bw - u.TextW("Ajouter")) * 0.5f, ar.y, bh, "Ajouter", NkCol::primaryFg);
            if (ahv && u.click) { w->AddCustomCfg(); } }
          ry = box.y + box.h; }

        // hauteur du contenu (max des deux colonnes) + marge basse
        const float32 contentH = ((y > ry ? y : ry) - body.y) + u.s(24);
        return contentH;
    }

    // Tables d'options partagees (cartes type + combos).
    inline const char* const* NkWizKinds(int32* n)    { static const char* k[] = { "consoleapp", "windowedapp", "staticlib", "sharedlib", "test" }; if (n) *n = 5; return k; }
    inline const char* const* NkWizLangs(int32* n)    { static const char* k[] = { "C", "C++", "ObjC", "ObjC++", "Zig", "Rust" }; if (n) *n = 6; return k; }
    inline const char* const* NkWizDialects(int32* n) { static const char* k[] = { "C++11", "C++14", "C++17", "C++20", "C++23" }; if (n) *n = 5; return k; }
    inline const char* NkWizKindIcon(int32 k) { const char* ic_[] = { "terminal", "app-window", "archive", "link", "flask" }; return (k >= 0 && k < 5) ? ic_[k] : "file"; }

    // ── ETAPE 2 : Projets du workspace. Renvoie la hauteur du contenu. ──
    inline float32 NkWizStep2(const NkUi& u, const NkRect& body, NkNewWsState* w, float32 dt, bool blockBg, const NkIcons& ic) {
        (void)dt; (void)ic;
        const float32 lh = u.Lh();
        const float32 x = body.x + u.s(28), wkW = body.w - u.s(56);
        float32 y = body.y + u.s(22);

        // En-tete + bouton « Ajouter un projet »
        NkWizLabel(u, x, y + u.s(4), "PROJETS DU WORKSPACE");
        { const float32 aw = u.s(160); const NkRect addR = { x + wkW - aw, y, aw, u.s(30) };
          const bool hv = !blockBg && u.Hit(addR);
          u.Rect(addR, hv ? NkColor{ 35,135,233,255 } : NkCol::primary, NkR::md * u.S);
          const float32 tw = u.TextW("Ajouter un projet");
          u.Icon("plus", { addR.x + (aw - tw - u.s(20)) * 0.5f, addR.y + (u.s(30) - u.s(12)) * 0.5f, u.s(12), u.s(12) }, NkCol::primaryFg);
          u.TextV(addR.x + (aw - tw - u.s(20)) * 0.5f + u.s(20), addR.y, u.s(30), "Ajouter un projet", NkCol::primaryFg);
          if (hv && u.click) w->OpenProjDlg(-1); }
        y += u.s(40);

        if (w->projects.Empty()) {
            const NkRect box = { x, y, wkW, u.s(96) };
            u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
            const char* m1 = "Aucun projet pour l'instant.";
            const char* m2 = "Cliquez sur « + Ajouter un projet » (ou creez le workspace : un projet sera propose).";
            u.Text(box.x + (box.w - u.TextW(m1)) * 0.5f, box.y + u.s(30), m1, NkCol::foreground);
            u.Text(box.x + (box.w - u.TextW(m2)) * 0.5f, box.y + u.s(52), m2, NkCol::mutedFg);
            y += u.s(106);
        } else {
            int32 doEdit = -1, doDel = -1, doStar = -1;
            for (usize i = 0; i < w->projects.Size(); ++i) {
                const NkWizProject& p = w->projects[i];
                const float32 ch = u.s(96);
                const NkRect card = { x, y, wkW, ch };
                u.Panel(card, NkCol::surface, NkCol::border, NkR::md * u.S);
                // icone + nom
                NkOwIco(u, ic.ouvrirDossier, "folder", { card.x + u.s(14), card.y + u.s(14), u.s(18), u.s(18) }, NkCol::accent);
                u.Text(card.x + u.s(40), card.y + u.s(15), p.name, NkCol::foreground);
                // boutons droite : Star / Editer / Supprimer
                float32 bx = card.x + card.w - u.s(14);
                { const float32 tw = u.s(28); const NkRect r = { bx - tw, card.y + u.s(12), tw, u.s(24) }; bx -= tw + u.s(6);
                  const bool hv = !blockBg && u.Hit(r);
                  u.Rect(r, hv ? NkCol::hover : NkCol::muted, NkR::sm * u.S);
                  u.Icon("trash", { r.x + u.s(7), r.y + u.s(5), u.s(14), u.s(14) }, NkCol::danger);
                  if (hv && u.click) doDel = (int32)i; }
                { const float32 tw = u.s(28); const NkRect r = { bx - tw, card.y + u.s(12), tw, u.s(24) }; bx -= tw + u.s(6);
                  const bool hv = !blockBg && u.Hit(r);
                  u.Rect(r, hv ? NkCol::hover : NkCol::muted, NkR::sm * u.S);
                  u.Icon("pencil", { r.x + u.s(7), r.y + u.s(5), u.s(14), u.s(14) }, NkCol::foreground);
                  if (hv && u.click) doEdit = (int32)i; }
                { const bool isStart = (w->startProj == (int32)i);
                  const float32 tw = u.s(70); const NkRect r = { bx - tw, card.y + u.s(12), tw, u.s(24) }; bx -= tw + u.s(6);
                  const bool hv = !blockBg && u.Hit(r);
                  u.Panel(r, isStart ? NkCol::secondary : (hv ? NkCol::hover : NkCol::muted), isStart ? NkCol::accent : NkCol::border, NkR::sm * u.S);
                  u.Icon("star", { r.x + u.s(8), r.y + u.s(6), u.s(12), u.s(12) }, isStart ? NkCol::accent : NkCol::mutedFg);
                  u.TextV(r.x + u.s(24), r.y, u.s(24), "Start", isStart ? NkCol::accent : NkCol::mutedFg);
                  if (hv && u.click) doStar = (int32)i; }
                // ligne meta : type pill + langage/dialecte/dossier
                int32 nK = 0, nL = 0, nD = 0; const char* const* kinds = NkWizKinds(&nK); const char* const* langs = NkWizLangs(&nL); const char* const* dials = NkWizDialects(&nD);
                const char* kind = (p.kind >= 0 && p.kind < nK) ? kinds[p.kind] : "consoleapp";
                const bool isApp = (p.kind == 0 || p.kind == 1);
                { const float32 pw = u.TextW(kind) + u.s(16); const NkRect pill = { card.x + u.s(40), card.y + u.s(40), pw, u.s(18) };
                  u.Panel(pill, isApp ? NkCol::secondary : NkCol::muted, isApp ? NkCol::accent : NkCol::border, NkR::sm * u.S);
                  u.TextV(pill.x + u.s(8), pill.y - u.s(3), u.s(18), kind, isApp ? NkCol::accent : NkCol::mutedFg);
                  float32 mx = pill.x + pw + u.s(14); const float32 my = card.y + u.s(41);
                  char meta[200]; std::snprintf(meta, sizeof(meta), "Langage: %s  ·  Dialecte: %s  ·  Dossier: %s",
                      (p.lang >= 0 && p.lang < nL) ? langs[p.lang] : "C++", (p.dialect >= 0 && p.dialect < nD) ? dials[p.dialect] : "C++20", p.location);
                  u.Text(mx, my, meta, NkCol::mutedFg); }
                // ligne sources/includes/depend
                { NkString line = "Sources: ";
                  for (usize k = 0; k < p.sources.Size(); ++k) { if (k) line += ", "; line += p.sources[k].CStr(); }
                  line += "   Includes: ";
                  for (usize k = 0; k < p.includes.Size(); ++k) { if (k) line += ", "; line += p.includes[k].CStr(); }
                  if (!p.dependsOn.Empty()) { line += "   Depend de: "; for (usize k = 0; k < p.dependsOn.Size(); ++k) { if (k) line += ", "; line += p.dependsOn[k].CStr(); } }
                  u.TextEllipsis(card.x + u.s(40), card.y + u.s(64), card.w - u.s(54), line.CStr(), NkCol::mutedFg); }
                y += ch + u.s(12);
            }
            if (doStar >= 0) w->startProj = (w->startProj == doStar) ? -1 : doStar;
            if (doEdit >= 0) w->OpenProjDlg(doEdit);
            if (doDel >= 0)  { w->projects.Erase(w->projects.Begin() + doDel); if (w->startProj == doDel) w->startProj = -1; else if (w->startProj > doDel) w->startProj--; }

            // ORDRE DE BUILD
            y += u.s(10);
            NkWizLabel(u, x, y, "ORDRE DE BUILD"); u.Text(x + u.TextW("ORDRE DE BUILD") + u.s(8), y, "(glisser pour reordonner)", NkCol::mutedFg);
            y += u.s(24);
            float32 px = x;
            for (usize i = 0; i < w->projects.Size(); ++i) {
                const NkWizProject& p = w->projects[i];
                const float32 pw = u.TextW(p.name) + u.s(34);
                const NkRect pill = { px, y, pw, u.s(30) };
                const bool hv = !blockBg && u.Hit(pill);
                u.Panel(pill, (w->dragProj == (int32)i) ? NkCol::secondary : (hv ? NkCol::hover : NkCol::muted), NkCol::border, NkR::md * u.S);
                u.Icon("grip", { pill.x + u.s(8), pill.y + u.s(9), u.s(10), u.s(12) }, NkCol::mutedFg);
                u.TextV(pill.x + u.s(22), pill.y, u.s(30), p.name, NkCol::foreground);
                // drag pour reordonner
                if (hv && u.click) w->dragProj = (int32)i;
                px += pw;
                if (i + 1 < w->projects.Size()) { u.Icon("arrow-right", { px + u.s(6), y + u.s(9), u.s(12), u.s(12) }, NkCol::accent); px += u.s(24); }
            }
            // relachement du drag : echange avec la pilule survolee
            if (w->dragProj >= 0) {
                if (!u.down) {
                    float32 qx = x;
                    for (usize i = 0; i < w->projects.Size(); ++i) {
                        const float32 pw = u.TextW(w->projects[i].name) + u.s(34);
                        const NkRect pill = { qx, y, pw, u.s(30) };
                        if (u.Hit(pill) && (int32)i != w->dragProj) {
                            NkWizProject t = w->projects[w->dragProj]; w->projects.Erase(w->projects.Begin() + w->dragProj);
                            w->projects.Insert(w->projects.Begin() + i, t); break;
                        }
                        qx += pw + u.s(24);
                    }
                    w->dragProj = -1;
                }
            }
            y += u.s(40);
            NkWizHint(u, x, y, wkW, "Correspond a l'ordre de declaration with project(\"...\") dans le .jenga"); y += u.s(20);
        }
        return (y - body.y) + u.s(24);
    }

    // Petite liste editable (entrees + bouton x) + champ d'ajout. Renvoie la hauteur utilisee.
    inline float32 NkWizEditList(const NkUi& u, float32 x, float32 y, float32 wdt, NkVector<NkString>& items,
                                 char* addBuf, int32 addCap, int32 fieldId, NkNewWsState* w, float32 dt, bool blockBg) {
        const float32 fH = u.s(28);
        const float32 listH = u.s(8) + (items.Empty() ? u.s(22) : items.Size() * u.s(24)) + u.s(4);
        const NkRect box = { x, y, wdt, listH };
        u.Panel(box, NkCol::input, NkCol::border, NkR::md * u.S);
        int32 rm = -1;
        if (items.Empty()) u.TextV(box.x + u.s(10), box.y, u.s(28), "(vide)", NkCol::mutedFg);
        for (usize i = 0; i < items.Size(); ++i) {
            const float32 ry = box.y + u.s(6) + i * u.s(24);
            u.Text(box.x + u.s(10), ry, items[i].CStr(), NkCol::foreground);
            const NkRect xr = { box.x + wdt - u.s(24), ry - u.s(2), u.s(18), u.s(18) };
            const bool hv = !blockBg && u.Hit(xr);
            u.Icon("x", { xr.x + u.s(4), xr.y + u.s(4), u.s(10), u.s(10) }, hv ? NkCol::danger : NkCol::mutedFg);
            if (hv && u.click) rm = (int32)i;
        }
        if (rm >= 0) items.Erase(items.Begin() + rm);
        y += listH + u.s(6);
        // champ d'ajout + bouton
        const float32 addBtnW = u.s(90);
        NkWizField(u, { x, y, wdt - addBtnW - u.s(8), fH }, addBuf, addCap, fieldId, w, dt, blockBg, u.s(10));
        { const NkRect br = { x + wdt - addBtnW, y, addBtnW, fH };
          const bool hv = !blockBg && u.Hit(br);
          u.Rect(br, hv ? NkCol::hover : NkCol::muted, NkR::md * u.S);
          u.Icon("plus", { br.x + u.s(14), br.y + (fH - u.s(12)) * 0.5f, u.s(12), u.s(12) }, NkCol::foreground);
          u.TextV(br.x + u.s(30), br.y, fH, "Ajouter", NkCol::foreground);
          if (hv && u.click && addBuf[0]) { items.PushBack(NkString(addBuf)); addBuf[0] = '\0'; if (w->focus == fieldId) w->focus = -1; } }
        return (y + fH) - (box.y);
    }

    // ── Dialog « Ajouter / Editer un projet » (modal scrollable). ──
    // Options des combos du projet (toolchains, optimize, memoire web).
    inline const char* const* NkWizTcs(int32* n)  { static const char* k[] = { "(auto)", "clang-mingw", "zig-linux-x64", "android-ndk", "emscripten", "clang-native", "clang-cl" }; if (n) *n = 7; return k; }
    inline const char* const* NkWizOpt(int32* n)  { static const char* k[] = { "Off", "Size", "Speed", "Full" }; if (n) *n = 4; return k; }
    inline const char* const* NkWizMem(int32* n)  { static const char* k[] = { "16 MB", "32 MB", "64 MB", "128 MB", "256 MB" }; if (n) *n = 5; return k; }
    inline int32 NkWizMemMB(int32 i) { const int32 m[] = { 16, 32, 64, 128, 256 }; return (i >= 0 && i < 5) ? m[i] : 32; }

    // Liste de « pilules » editables (links/defines) : pills + bouton « + Ajouter » (champ partage).
    inline float32 NkProjPills(const NkUi& u, float32 x, float32 y, float32 maxW, NkVector<NkString>& items,
                               int32 listId, NkNewWsState* w, float32 dt, bool blockBg) {
        const float32 ph = u.s(22); float32 cx = x; int32 rm = -1;
        for (usize i = 0; i < items.Size(); ++i) {
            const float32 pw = u.TextW(items[i].CStr()) + u.s(26);
            if (cx + pw > x + maxW) { cx = x; y += ph + u.s(6); }
            const NkRect pill = { cx, y, pw, ph };
            u.Panel(pill, NkCol::muted, NkCol::border, NkR::sm * u.S);
            u.TextV(pill.x + u.s(8), pill.y - u.s(3), ph, items[i].CStr(), NkCol::foreground);
            const NkRect xr = { pill.x + pw - u.s(16), pill.y + u.s(4), u.s(13), u.s(13) };
            const bool hv = !blockBg && u.Hit(xr);
            u.Icon("x", { xr.x + u.s(2), xr.y + u.s(2), u.s(9), u.s(9) }, hv ? NkCol::danger : NkCol::mutedFg);
            if (hv && u.click) rm = (int32)i;
            cx += pw + u.s(6);
        }
        if (rm >= 0) items.Erase(items.Begin() + rm);
        // champ d'ajout (inline si cette liste est la cible) ou bouton « + Ajouter »
        if (w->projFltTarget == listId) {
            const float32 fw = u.s(120);
            if (cx + fw + u.s(70) > x + maxW) { cx = x; y += ph + u.s(6); }
            NkWizField(u, { cx, y - u.s(4), fw, u.s(26) }, w->projFltAdd, (int32)sizeof(w->projFltAdd), 200, w, dt, blockBg, u.s(8));
            const NkRect ok = { cx + fw + u.s(6), y - u.s(4), u.s(60), u.s(26) };
            const bool hv = !blockBg && u.Hit(ok);
            u.Rect(ok, hv ? NkColor{ 35,135,233,255 } : NkCol::primary, NkR::sm * u.S);
            u.TextV(ok.x + u.s(10), ok.y, u.s(26), "OK", NkCol::primaryFg);
            const bool commit = (hv && u.click) || u.ctx->input.KeyPressed(NkGuiKey::Enter);
            if (commit) { if (w->projFltAdd[0]) items.PushBack(NkString(w->projFltAdd)); w->projFltAdd[0] = '\0'; w->projFltTarget = -1; w->focus = -1; }
        } else {
            const NkRect ar = { cx, y - u.s(3), u.s(90), ph + u.s(4) };
            const bool hv = !blockBg && u.Hit(ar);
            u.Icon("plus", { ar.x + u.s(4), ar.y + u.s(7), u.s(11), u.s(11) }, hv ? NkCol::foreground : NkCol::mutedFg);
            u.TextV(ar.x + u.s(18), ar.y, ph + u.s(4), "Ajouter", hv ? NkCol::foreground : NkCol::mutedFg);
            if (hv && u.click) { w->projFltTarget = listId; w->projFltAdd[0] = '\0'; w->SetFocus(200, w->projFltAdd); }
        }
        return y + ph + u.s(8);
    }

    // ── PROJET — Etape 1 : Definition (2 colonnes). ──
    inline float32 NkProjDef(const NkUi& u, const NkRect& body, NkNewWsState* w, NkCodeDialogs* dlg, float32 dt, bool blockBg, const NkIcons& ic) {
        (void)dlg;
        NkWizProject& p = w->projDraft;
        const float32 padL = u.s(24);
        const float32 rightW = (body.w > u.s(760)) ? u.s(300) : u.s(250);
        const float32 cx = body.x + padL, rx = body.x + body.w - u.s(24) - rightW, cw = (rx - u.s(28)) - cx, fH = u.s(30);

        // ── colonne gauche ──
        float32 y = body.y + u.s(16);
        NkWizLabel(u, cx, y, "NOM DU PROJET"); y += u.s(20);
        NkWizField(u, { cx, y, cw, fH }, p.name, (int32)sizeof(p.name), 100, w, dt, blockBg, u.s(10)); y += fH + u.s(16);
        NkWizLabel(u, cx, y, "TYPE DE PROJET (kind)"); y += u.s(22);
        { int32 nK = 0; const char* const* kinds = NkWizKinds(&nK);
          const float32 gap = u.s(8), kcw = (cw - gap * (nK - 1)) / nK, kch = u.s(52);
          for (int32 k = 0; k < nK; ++k) {
              const NkRect c = { cx + k * (kcw + gap), y, kcw, kch };
              const bool selk = (p.kind == k); const bool hv = !blockBg && u.Hit(c);
              u.Panel(c, selk ? NkCol::secondary : NkCol::input, selk ? NkCol::primary : NkCol::border, NkR::md * u.S);
              u.Icon(NkWizKindIcon(k), { c.x + (kcw - u.s(18)) * 0.5f, c.y + u.s(8), u.s(18), u.s(18) }, selk ? NkCol::primary : NkCol::mutedFg);
              const float32 tw = u.TextW(kinds[k]); u.Text(c.x + (kcw - tw) * 0.5f, c.y + u.s(30), kinds[k], selk ? NkCol::primary : NkCol::mutedFg);
              if (hv && u.click) p.kind = k;
          }
          y += kch + u.s(8);
          const char* desc[] = { "Executable console - main() standard", "Application avec interface graphique", "Bibliotheque statique (.lib/.a)", "Bibliotheque partagee (.dll/.so)", "Suite de tests Unitest" };
          NkWizHint(u, cx, y, cw, (p.kind >= 0 && p.kind < 5) ? desc[p.kind] : ""); y += u.s(28); }
        NkWizLabel(u, cx, y, "LANGAGE & DIALECTE"); y += u.s(20);
        { int32 nL = 0, nD = 0; const char* const* langs = NkWizLangs(&nL); const char* const* dials = NkWizDialects(&nD);
          const float32 cwid = (cw - u.s(16)) * 0.5f;
          u.Text(cx, y, "Langage", NkCol::mutedFg); u.Text(cx + cwid + u.s(16), y, "Dialecte", NkCol::mutedFg);
          NkWizCombo(u, { cx, y + u.s(16), cwid, fH }, 110, w, langs, nL, &p.lang, blockBg);
          NkWizCombo(u, { cx + cwid + u.s(16), y + u.s(16), cwid, fH }, 111, w, dials, nD, &p.dialect, blockBg);
          y += u.s(16) + fH + u.s(16); }
        NkWizLabel(u, cx, y, "DOSSIER DU PROJET (location)"); y += u.s(20);
        NkWizField(u, { cx, y, cw - u.s(38), fH }, p.location, (int32)sizeof(p.location), 101, w, dt, blockBg, u.s(10));
        { const NkRect br = { cx + cw - u.s(32), y, u.s(32), fH }; u.Rect(br, NkCol::muted, NkR::md * u.S);
          u.Icon("folder", { br.x + u.s(9), br.y + u.s(9), u.s(14), u.s(14) }, NkCol::mutedFg); }
        y += fH + u.s(16);
        NkWizLabel(u, cx, y, "FICHIERS SOURCES (files)"); y += u.s(20);
        y += NkWizEditList(u, cx, y, cw, p.sources, w->projSrcAdd, (int32)sizeof(w->projSrcAdd), 102, w, dt, blockBg) + u.s(14);
        NkWizLabel(u, cx, y, "DOSSIERS D'INCLUDE (includedirs)"); y += u.s(20);
        y += NkWizEditList(u, cx, y, cw, p.includes, w->projIncAdd, (int32)sizeof(w->projIncAdd), 103, w, dt, blockBg) + u.s(8);

        // ── colonne droite ──
        float32 ry = body.y + u.s(16);
        NkWizLabel(u, rx, ry, "REPERTOIRES DE BUILD"); ry += u.s(20);
        { const NkRect box = { rx, ry, rightW, u.s(120) };
          u.Panel(box, NkCol::input, NkCol::border, NkR::md * u.S);
          u.Text(box.x + u.s(12), box.y + u.s(10), "Obj", NkCol::mutedFg);
          u.TextEllipsis(box.x + u.s(12), box.y + u.s(26), rightW - u.s(24), "%{wks.location}/Build/Obj/%{cfg.buildcfg}-%{cfg.system}/%{prj.name}", NkCol::foreground);
          u.Text(box.x + u.s(12), box.y + u.s(56), "Bin", NkCol::mutedFg);
          u.TextEllipsis(box.x + u.s(12), box.y + u.s(72), rightW - u.s(24), "%{wks.location}/Build/Bin/%{cfg.buildcfg}-%{cfg.system}/%{prj.name}", NkCol::foreground); }
        ry += u.s(128);
        if (NkWizCheck(u, rx, ry + u.s(8), "Utiliser les variables dynamiques Jenga", p.dynVars, blockBg)) p.dynVars = !p.dynVars; ry += u.s(34);
        NkWizLabel(u, rx, ry, "DEPENDANCES (links + dependson)"); ry += u.s(20);
        { const float32 boxH = u.s(40) + (w->projects.Empty() ? u.s(22) : (w->projects.Size()) * u.s(26));
          const NkRect box = { rx, ry, rightW, boxH };
          u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
          float32 dy = box.y + u.s(14); bool any = false;
          for (usize i = 0; i < w->projects.Size(); ++i) {
              if ((int32)i == w->projEditIdx) continue; any = true;
              const char* nm = w->projects[i].name; bool dep = false; int32 di = -1;
              for (usize k = 0; k < p.dependsOn.Size(); ++k) if (StrEq(p.dependsOn[k].CStr(), nm)) { dep = true; di = (int32)k; break; }
              if (NkWizCheck(u, box.x + u.s(12), dy, nm, dep, blockBg)) { if (dep && di >= 0) p.dependsOn.Erase(p.dependsOn.Begin() + di); else p.dependsOn.PushBack(NkString(nm)); }
              dy += u.s(26);
          }
          if (!any) { u.TextV(box.x + u.s(12), dy - u.s(8), u.s(22), "(aucun autre projet)", NkCol::mutedFg); }
          ry = box.y + boxH + u.s(16); }
        NkWizLabel(u, rx, ry, "GENERER LES FICHIERS DE DEMARRAGE"); ry += u.s(20);
        { const NkRect box = { rx, ry, rightW, u.s(96) };
          u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
          float32 gy = box.y + u.s(18);
          if (NkWizCheck(u, box.x + u.s(12), gy, "main.cpp avec Hello World", p.genMain, blockBg)) p.genMain = !p.genMain; gy += u.s(26);
          if (NkWizCheck(u, box.x + u.s(12), gy, "README.md du projet", p.genReadme, blockBg)) p.genReadme = !p.genReadme; gy += u.s(26);
          if (NkWizCheck(u, box.x + u.s(12), gy, "Fichier de tests (Unitest)", p.genTest, blockBg)) p.genTest = !p.genTest; }
        ry += u.s(106);

        return ((y > ry ? y : ry) - body.y) + u.s(20);
    }

    // ── PROJET — Etape 2 : Filtres par plateforme + par config (2 colonnes). ──
    inline float32 NkProjFilters(const NkUi& u, const NkRect& body, NkNewWsState* w, float32 dt, bool blockBg, const NkIcons& ic) {
        (void)ic;
        NkWizProject& p = w->projDraft;
        const float32 rightW = (body.w > u.s(760)) ? u.s(300) : u.s(250);
        const float32 cx = body.x + u.s(24), rx = body.x + body.w - u.s(24) - rightW, cw = (rx - u.s(28)) - cx, fH = u.s(28);
        int32 nTc = 0; const char* const* tcs = NkWizTcs(&nTc);
        int32 nK = 0; const char* const* kinds = NkWizKinds(&nK);

        float32 y = body.y + u.s(14);
        NkWizLabel(u, cx, y, "FILTRES PAR PLATEFORME"); u.Text(cx + u.TextW("FILTRES PAR PLATEFORME") + u.s(8), y, "- comportements specifiques", NkCol::mutedFg); y += u.s(24);
        struct OsRow { const char* name; const char* filt; int32 idx; };
        const OsRow rows[] = { { "WINDOWS", "system:Windows", 0 }, { "LINUX", "system:Linux", 1 }, { "ANDROID", "system:Android", 2 }, { "WEB", "system:Web", 3 } };
        for (int32 r = 0; r < 4; ++r) {
            NkWizProject::OsFlt& o = p.os[rows[r].idx];
            const bool isAndroid = rows[r].idx == 2, isWeb = rows[r].idx == 3;
            float32 inner = u.s(96) + (isAndroid ? u.s(28) : 0.f);
            const NkRect box = { cx, y, cw, inner };
            u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
            u.Text(box.x + u.s(14), box.y + u.s(10), rows[r].name, NkCol::foreground);
            u.Text(box.x + u.s(14) + u.TextW(rows[r].name) + u.s(8), box.y + u.s(10), rows[r].filt, NkCol::mutedFg);
            float32 fy = box.y + u.s(32);
            if (isAndroid) { u.Text(box.x + u.s(14), fy + u.s(6), "Type override", NkCol::mutedFg);
                NkWizCombo(u, { box.x + u.s(120), fy, u.s(150), fH }, 300 + rows[r].idx * 10 + 1, w, kinds, nK, &o.androidKind, blockBg); fy += u.s(32); }
            u.Text(box.x + u.s(14), fy + u.s(6), "Toolchain", NkCol::mutedFg);
            NkWizCombo(u, { box.x + u.s(120), fy, u.s(160), fH }, 300 + rows[r].idx * 10, w, tcs, nTc, &o.toolchain, blockBg);
            if (isWeb) { u.Text(box.x + u.s(290), fy + u.s(6), "Memory", NkCol::mutedFg);
                int32 nM = 0; const char* const* mem = NkWizMem(&nM);
                NkWizCombo(u, { box.x + u.s(350), fy, u.s(90), fH }, 300 + rows[r].idx * 10 + 2, w, mem, nM, &o.webMem, blockBg); }
            fy += u.s(32);
            u.Text(box.x + u.s(14), fy + u.s(2), "Links", NkCol::mutedFg);
            NkProjPills(u, box.x + u.s(120), fy, cw - u.s(140), o.links, rows[r].idx * 2, w, dt, blockBg);
            // defines : sur une ligne suivante si besoin (pour rester simple, sous links)
            y += inner + u.s(10);
        }
        NkWizHint(u, cx, y, cw, "Genere des blocs with filter(\"system:X\"): dans le .jenga"); y += u.s(24);

        // ── colonne droite : filtres de configuration ──
        float32 ry = body.y + u.s(14);
        NkWizLabel(u, rx, ry, "FILTRES DE CONFIGURATION"); ry += u.s(24);
        int32 nO = 0; const char* const* opt = NkWizOpt(&nO);
        struct CfgRow { const char* name; const char* filt; NkVector<NkString>* defs; int32* o; bool* sym; int32 base; };
        CfgRow cfgs[] = { { "DEBUG", "config:Debug", &p.dbgDefines, &p.dbgOpt, &p.dbgSym, 320 }, { "RELEASE", "config:Release", &p.relDefines, &p.relOpt, &p.relSym, 322 } };
        for (int32 c = 0; c < 2; ++c) {
            const NkRect box = { rx, ry, rightW, u.s(118) };
            u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
            u.Text(box.x + u.s(12), box.y + u.s(10), cfgs[c].name, NkCol::foreground);
            u.Text(box.x + u.s(12) + u.TextW(cfgs[c].name) + u.s(8), box.y + u.s(10), cfgs[c].filt, NkCol::mutedFg);
            u.Text(box.x + u.s(12), box.y + u.s(34), "Defines", NkCol::mutedFg);
            NkProjPills(u, box.x + u.s(80), box.y + u.s(32), rightW - u.s(92), *cfgs[c].defs, cfgs[c].base, w, dt, blockBg);
            u.Text(box.x + u.s(12), box.y + u.s(64), "Optimize", NkCol::mutedFg);
            NkWizCombo(u, { box.x + u.s(80), box.y + u.s(60), u.s(110), fH }, cfgs[c].base + 1, w, opt, nO, cfgs[c].o, blockBg);
            u.Text(box.x + u.s(12), box.y + u.s(92), "Symbols", NkCol::mutedFg);
            if (NkWizCheck(u, box.x + u.s(80), box.y + u.s(98), "Activer", *cfgs[c].sym, blockBg)) *cfgs[c].sym = !*cfgs[c].sym;
            ry += u.s(126);
        }
        NkWizHint(u, rx, ry, rightW, "Genere des blocs with filter(\"config:X\"): dans le .jenga"); ry += u.s(24);

        return ((y > ry ? y : ry) - body.y) + u.s(20);
    }

    // ── PROJET — Etape 3 : Resume (apercu .jenga + ce qui sera cree). ──
    inline float32 NkProjSummary(const NkUi& u, const NkRect& body, NkNewWsState* w, bool blockBg, const NkIcons& ic) {
        (void)blockBg; (void)ic;
        const NkWizProject& p = w->projDraft;
        const float32 rightW = (body.w > u.s(760)) ? u.s(300) : u.s(250);
        const float32 cx = body.x + u.s(24), rx = body.x + body.w - u.s(24) - rightW, cw = (rx - u.s(28)) - cx;
        float32 y = body.y + u.s(14);
        NkWizLabel(u, cx, y, "EXTRAIT .JENGA"); u.Text(cx + u.TextW("EXTRAIT .JENGA") + u.s(8), y, "- sera insere dans le workspace", NkCol::mutedFg); y += u.s(22);
        const NkString code = w->BuildProjectJenga(p);
        // compte les lignes pour la hauteur
        int32 lines = 1; for (const char* s = code.CStr(); *s; ++s) if (*s == '\n') ++lines;
        const float32 lh = u.Lh(), codeH = lines * (lh + u.s(3)) + u.s(20);
        const NkRect box = { cx, y, cw, codeH };
        u.Panel(box, NkColor{ 18,21,26,255 }, NkCol::border, NkR::md * u.S);
        u.dl->PushClipRect(box, true);
        float32 ly = box.y + u.s(10); int32 ln = 1; NkString line;
        for (const char* s = code.CStr(); ; ++s) {
            if (*s == '\n' || *s == '\0') {
                char num[8]; std::snprintf(num, sizeof(num), "%d", ln);
                u.Text(box.x + u.s(10), ly, num, NkCol::mutedFg);
                u.Text(box.x + u.s(40), ly, line.CStr(), NkCol::foreground);
                ly += lh + u.s(3); ++ln; line.Clear(); if (*s == '\0') break;
            } else if (*s != '\r') line += *s;
        }
        u.dl->PopClipRect();
        y += codeH + u.s(10);

        // colonne droite : ce qui sera cree + resume
        float32 ry = body.y + u.s(14);
        NkWizLabel(u, rx, ry, "CE QUI SERA CREE"); ry += u.s(22);
        auto created = [&](const char* path) { const NkRect b = { rx, ry, rightW, u.s(30) };
            u.Panel(b, NkCol::surface, NkCol::border, NkR::md * u.S);
            u.Icon("check-circle", { b.x + u.s(10), b.y + u.s(8), u.s(14), u.s(14) }, NkCol::success);
            u.TextV(b.x + u.s(32), b.y, u.s(30), path, NkCol::foreground); ry += u.s(36); };
        char b1[256]; if (p.genMain) { std::snprintf(b1, sizeof(b1), "%ssrc/main.cpp", p.location); created(b1); }
        { char b2[256]; std::snprintf(b2, sizeof(b2), "%sinclude/", p.location); created(b2); }
        if (p.genReadme) { char b3[256]; std::snprintf(b3, sizeof(b3), "%sREADME.md", p.location); created(b3); }
        ry += u.s(10);
        NkWizLabel(u, rx, ry, "RESUME DU PROJET"); ry += u.s(22);
        auto chip = [&](const char* drawn, const char* txt) { const NkRect b = { rx, ry, rightW, u.s(30) };
            u.Panel(b, NkCol::surface, NkCol::border, NkR::md * u.S);
            u.Icon(drawn, { b.x + u.s(10), b.y + u.s(8), u.s(14), u.s(14) }, NkCol::accent);
            u.TextV(b.x + u.s(32), b.y, u.s(30), txt, NkCol::foreground); ry += u.s(36); };
        int32 nK = 0, nD = 0; const char* const* kinds = NkWizKinds(&nK); const char* const* dials = NkWizDialects(&nD);
        char c1[120]; std::snprintf(c1, sizeof(c1), "%s - %s", (p.kind >= 0 && p.kind < nK) ? kinds[p.kind] : "consoleapp", (p.dialect >= 0 && p.dialect < nD) ? dials[p.dialect] : "C++20"); chip("terminal", c1);
        chip("folder", p.location);
        { int32 dc = 0; for (int32 i = 0; i < 4; ++i) if (p.os[i].on || p.os[i].toolchain > 0 || !p.os[i].links.Empty() || !p.os[i].defines.Empty()) ++dc;
          char c2[80]; std::snprintf(c2, sizeof(c2), "%d plateforme(s) filtree(s)", dc); chip("cpu", c2); }
        { char c3[80]; std::snprintf(c3, sizeof(c3), "%d dependance(s)", (int32)p.dependsOn.Size()); chip("link", c3); }
        return ((y > ry ? y : ry) - body.y) + u.s(20);
    }

    // ── Modale large « Nouveau / Editer projet » : 3 etapes internes (Definition / Filtres / Resume). ──
    inline void NkWizProjDialog(const NkUi& u, const NkRect& r, NkNewWsState* w, NkCodeDialogs* dlg, float32 dt, bool blockBg, const NkIcons& ic) {
        NkWizProject& p = w->projDraft;
        u.dl->AddRectFilled(r, NkColor{ 0,0,0,160 }, 0.f);                       // voile
        const float32 mw = (r.w > u.s(960)) ? u.s(900) : (r.w - u.s(40));
        const float32 mh = (r.h > u.s(680)) ? u.s(640) : (r.h - u.s(30));
        const NkRect modal = { r.x + (r.w - mw) * 0.5f, r.y + (r.h - mh) * 0.5f, mw, mh };
        u.Panel(modal, NkCol::background, NkCol::border, NkR::lg * u.S);
        // En-tete
        NkOwIco(u, ic.nouveau, "plus-circle", { modal.x + u.s(22), modal.y + u.s(14), u.s(16), u.s(16) }, NkCol::primary);
        u.Text(modal.x + u.s(46), modal.y + u.s(15), w->projEditIdx >= 0 ? "EDITER LE PROJET" : "NOUVEAU PROJET", NkCol::foreground);
        { char st3[20]; std::snprintf(st3, sizeof(st3), "Etape %d sur 3", w->projStep + 1);
          u.Text(modal.x + mw - u.s(22) - u.TextW(st3), modal.y + u.s(15), st3, NkCol::mutedFg); }
        // Barre d'etapes
        const float32 sbH = u.s(70);
        const char* labels[] = { "Definition", "Filtres", "Resume" };
        NkWizSteps(u, { modal.x, modal.y + u.s(38), mw, sbH }, w->projStep, labels, 3);
        // Corps defilable
        const float32 footH = u.s(54);
        const NkRect view = { modal.x, modal.y + u.s(38) + sbH, mw, mh - u.s(38) - sbH - footH };
        if (u.Hit(view) && u.ctx->input.wheel != 0.f && !blockBg) { w->projScroll -= u.ctx->input.wheel * u.s(40); u.ctx->input.wheel = 0.f;
            if (w->projScroll < 0.f) w->projScroll = 0.f; if (w->projScroll > w->projScrollMax) w->projScroll = w->projScrollMax; }
        const NkRect bodyR = { view.x, view.y - w->projScroll, view.w, view.h };
        u.dl->PushClipRect(view, true);
        float32 contentH = view.h;
        switch (w->projStep) {
            case 0: contentH = NkProjDef(u, bodyR, w, dlg, dt, blockBg, ic); break;
            case 1: contentH = NkProjFilters(u, bodyR, w, dt, blockBg, ic); break;
            case 2: contentH = NkProjSummary(u, bodyR, w, blockBg, ic); break;
            default: break;
        }
        u.dl->PopClipRect();
        w->projScrollMax = (contentH > view.h) ? (contentH - view.h) : 0.f;
        if (w->projScroll > w->projScrollMax) w->projScroll = w->projScrollMax;
        if (w->projScrollMax > 0.5f) {
            const float32 sw = u.s(10); const NkRect track = { view.x + view.w - sw - u.s(4), view.y, sw, view.h };
            u.dl->AddRectFilled(track, NkColor{ 18,21,26,160 }, sw * 0.5f);
            float32 thh = view.h * (view.h / (view.h + w->projScrollMax)); if (thh < u.s(28)) thh = u.s(28);
            const float32 ty = view.y + (view.h - thh) * (w->projScroll / w->projScrollMax);
            u.dl->AddRectFilled({ track.x + u.s(2), ty, sw - u.s(4), thh }, NkColor{ 70,76,84,255 }, (sw - u.s(4)) * 0.5f);
        }
        // Pied : Precedent / Annuler / Suivant|Creer
        const bool valid = p.name[0] && NkNewWsState::ValidName(p.name);
        const int32 fa = NkWizFooter(u, { modal.x, modal.y + mh - footH, mw, footH }, w->projStep, 2, blockBg, valid, "Creer le Projet", false);
        if (fa == 1) { w->projDlg = false; w->focus = -1; }
        else if (fa == -1 && w->projStep > 0) { w->projStep--; w->focus = -1; w->projScroll = 0.f; }
        else if ((fa == 2 || fa == 3) && valid) { if (w->projStep < 2) { w->projStep++; w->focus = -1; w->projScroll = 0.f; } else w->CommitProjDlg(); }
    }

    // ── Page titree generique (etapes 3..5 a etoffer) ──
    inline void NkWizPlaceholder(const NkUi& u, const NkRect& body, const char* title, const char* sub) {
        const float32 cx = body.x + body.w * 0.5f, cy = body.y + body.h * 0.5f;
        u.Icon("layers", { cx - u.s(18), cy - u.s(46), u.s(36), u.s(36) }, NkCol::muted);
        const float32 tw = u.TextW(title);
        u.Text(cx - tw * 0.5f, cy, title, NkCol::foreground);
        const float32 sw = u.TextW(sub);
        u.Text(cx - sw * 0.5f, cy + u.s(22), sub, NkCol::mutedFg);
    }

    // ── Panneau wizard « Nouveau Workspace ». Renvoie 1 si Annuler (retour Accueil). ──
    inline int32 NkNewWsPanel(const NkUi& u, const NkRect& r, NkNewWsState* w, NkCodeState* st,
                              NkCodeDialogs* dlg, float32 dt, const NkIcons& ic) {
        (void)dlg;
        w->EnsureInit(st);
        if (w->focus == 3) w->jengaManual = true;   // l'utilisateur edite le nom de fichier
        w->SyncDerived();
        // Un combo ouvert OU le picker de dossier modal capturent les evenements du fond.
        // Un combo, le picker modal OU le dialog projet gelent la page + le pied.
        const bool blockBg = (w->comboOpen >= 0) || (dlg && dlg->pickerOpen) || w->projDlg;

        u.Rect(r, NkCol::background);

        // ── En-tete ──
        const float32 hH = u.s(44);
        u.Rect({ r.x, r.y, r.w, hH }, NkCol::background);
        NkOwIco(u, ic.nouveau, "plus-circle", { r.x + u.s(28), r.y + (hH - u.s(18)) * 0.5f, u.s(18), u.s(18) }, NkCol::primary);
        u.Text(r.x + u.s(54), r.y + (hH - u.Lh()) * 0.5f, "NOUVEAU WORKSPACE", NkCol::foreground);
        { char st5[24]; std::snprintf(st5, sizeof(st5), "Etape %d sur 5", w->step + 1);
          const float32 tw = u.TextW(st5);
          u.Text(r.x + r.w - u.s(28) - tw, r.y + (hH - u.Lh()) * 0.5f, st5, NkCol::mutedFg); }

        // ── Barre d'etapes ──
        const float32 sbH = u.s(78);
        const char* labels[] = { "Workspace", "Projets", "Toolchains", "Plateformes", "Resume" };
        NkWizSteps(u, { r.x, r.y + hH, r.w, sbH }, w->step, labels, 5);

        // ── Corps (defilable verticalement) ──
        const float32 footH = u.s(56);
        const NkRect view = { r.x, r.y + hH + sbH, r.w, r.h - hH - sbH - footH };
        if (u.Hit(view) && u.ctx->input.wheel != 0.f && !blockBg) {
            w->scroll -= u.ctx->input.wheel * u.s(40); u.ctx->input.wheel = 0.f;
            if (w->scroll < 0.f) w->scroll = 0.f; if (w->scroll > w->scrollMax) w->scroll = w->scrollMax;
        }
        // Largeur de contenu minimale (sinon scroll horizontal) : les 2 colonnes + marges.
        const float32 minW = u.s(860);
        const float32 bodyW = (view.w < minW) ? minW : view.w;
        w->hscrollMax = (bodyW > view.w) ? (bodyW - view.w) : 0.f;
        if (w->hscroll > w->hscrollMax) w->hscroll = w->hscrollMax; if (w->hscroll < 0.f) w->hscroll = 0.f;
        const NkRect body = { view.x - w->hscroll, view.y - w->scroll, bodyW, view.h };
        u.dl->PushClipRect(view, true);
        float32 contentH = view.h;
        switch (w->step) {
            case 0: contentH = NkWizStep1(u, body, w, dlg, dt, blockBg, ic); break;
            case 1: contentH = NkWizStep2(u, body, w, dt, blockBg, ic); break;
            case 2: NkWizPlaceholder(u, view, "Etape 3 — Toolchains", "(a venir : detection + selection + assignation par plateforme)"); break;
            case 3: NkWizPlaceholder(u, view, "Etape 4 — Plateformes & Architectures", "(a venir : OS cibles + archs + options Android/Emscripten)"); break;
            case 4: NkWizPlaceholder(u, view, "Etape 5 — Resume & Generation", "(a venir : apercu du .jenga + liste des fichiers crees)"); break;
            default: break;
        }
        u.dl->PopClipRect();
        w->scrollMax = (contentH > view.h) ? (contentH - view.h) : 0.f;
        if (w->scroll > w->scrollMax) w->scroll = w->scrollMax;
        // Scrollbar verticale.
        if (w->scrollMax > 0.5f) {
            const float32 sw = u.s(10);
            const NkRect track = { view.x + view.w - sw - u.s(2), view.y, sw, view.h };
            u.dl->AddRectFilled(track, NkColor{ 18,21,26,160 }, sw * 0.5f);
            float32 thh = view.h * (view.h / (view.h + w->scrollMax)); if (thh < u.s(28)) thh = u.s(28);
            const float32 ty = view.y + (view.h - thh) * (w->scroll / w->scrollMax);
            const NkRect thumb = { track.x + u.s(2), ty, sw - u.s(4), thh };
            const bool hov = u.Hit(thumb);
            if (w->barDrag) { if (!u.down) w->barDrag = false;
                else { const float32 t = (u.mp.y - w->barOff - view.y) / (view.h - thh);
                       w->scroll = t * w->scrollMax; if (w->scroll < 0.f) w->scroll = 0.f; if (w->scroll > w->scrollMax) w->scroll = w->scrollMax; } }
            else if (hov && u.click && !blockBg) { w->barDrag = true; w->barOff = u.mp.y - ty; }
            u.dl->AddRectFilled(thumb, (w->barDrag || hov) ? NkColor{ 96,104,114,255 } : NkColor{ 56,63,72,255 }, (sw - u.s(4)) * 0.5f);
        }
        // Scrollbar horizontale (si le contenu depasse en largeur).
        if (w->hscrollMax > 0.5f) {
            const float32 sh = u.s(10);
            const float32 availW = view.w - (w->scrollMax > 0.5f ? u.s(14) : 0.f);
            const NkRect track = { view.x, view.y + view.h - sh, availW, sh };
            u.dl->AddRectFilled(track, NkColor{ 18,21,26,160 }, sh * 0.5f);
            float32 thw = availW * (view.w / bodyW); if (thw < u.s(28)) thw = u.s(28);
            const float32 tx = track.x + (availW - thw) * (w->hscroll / w->hscrollMax);
            const NkRect thumb = { tx, track.y + u.s(2), thw, sh - u.s(4) };
            const bool hov = u.Hit(thumb);
            if (w->hbarDrag) { if (!u.down) w->hbarDrag = false;
                else { const float32 t = (u.mp.x - w->hbarOff - track.x) / (availW - thw);
                       w->hscroll = t * w->hscrollMax; if (w->hscroll < 0.f) w->hscroll = 0.f; if (w->hscroll > w->hscrollMax) w->hscroll = w->hscrollMax; } }
            else if (hov && u.click && !blockBg) { w->hbarDrag = true; w->hbarOff = u.mp.x - tx; }
            u.dl->AddRectFilled(thumb, (w->hbarDrag || hov) ? NkColor{ 96,104,114,255 } : NkColor{ 56,63,72,255 }, (sh - u.s(4)) * 0.5f);
        }

        // ── Pied (Suivant grise si etape invalide) ──
        int32 result = 0;
        const bool nextOk = (w->step != 0) || w->Step1Valid();
        const int32 fa = NkWizFooter(u, { r.x, r.y + r.h - footH, r.w, footH }, w->step, 4, blockBg, nextOk);
        if (fa == 1) result = 1;                                  // Annuler -> Accueil
        else if (fa == -1 && w->step > 0) { w->step--; w->focus = -1; w->scroll = 0.f; }
        else if (fa == 2 && nextOk) { if (w->step < 4) { w->step++; w->focus = -1; w->scroll = 0.f; } else { if (w->Generate(dlg)) result = 1; } }
        else if (fa == 3 && nextOk) { if (w->Generate(dlg)) result = 1; }   // Creer maintenant (defauts)

        // ── Modale « Ajouter / Editer un projet » (3 etapes, par-dessus la page) ──
        if (w->projDlg) NkWizProjDialog(u, r, w, dlg, dt, (w->comboOpen >= 0), ic);

        // ── Dropdown de combo (rendu PAR-DESSUS tout) ──
        if (w->comboOpen >= 0 && w->comboOpts && w->comboSel) {
            const float32 ih = u.s(26);
            const NkRect dd = { w->comboR.x, w->comboR.y + w->comboR.h + u.s(2), w->comboR.w, w->comboN * ih + u.s(6) };
            u.dl->AddRectFilled({ dd.x + u.s(2), dd.y + u.s(3), dd.w, dd.h }, NkColor{ 0,0,0,90 }, NkR::md * u.S);
            u.Panel(dd, NkCol::surface, NkCol::primary, NkR::md * u.S);
            bool chose = false;
            for (int32 k = 0; k < w->comboN; ++k) {
                const NkRect ir = { dd.x + u.s(4), dd.y + u.s(3) + k * ih, dd.w - u.s(8), ih };
                const bool hv = u.Hit(ir);
                if (hv || k == *w->comboSel) u.Rect(ir, NkCol::hover, NkR::sm * u.S);
                u.TextV(ir.x + u.s(8), ir.y, ih, w->comboOpts[k], NkCol::foreground);
                if (hv && u.click) { *w->comboSel = k; chose = true; }
            }
            // Ferme des qu'on clique HORS de la liste (sauf la frame d'ouverture).
            if (chose || (u.click && !u.Hit(dd) && !w->comboJustOpened)) w->comboOpen = -1;
            w->comboJustOpened = false;
        }

        // Echap -> ferme combo, sinon dialog projet, sinon defocus, sinon annuler.
        if (u.ctx->input.KeyPressed(NkGuiKey::Escape)) {
            if (w->comboOpen >= 0) w->comboOpen = -1;
            else if (w->projDlg) { w->projDlg = false; w->focus = -1; }
            else if (w->focus >= 0) w->focus = -1;
            else result = 1;
        }
        return result;
    }

} // namespace nkcode
} // namespace nkentseu
