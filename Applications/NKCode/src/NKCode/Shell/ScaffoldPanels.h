#pragma once
// =============================================================================
// ScaffoldPanels.h — MAQUETTES des interfaces de l'IDE (NKGui, multi-plateforme).
//
// Strategie : on pose d'abord la STRUCTURE VISUELLE de toutes les interfaces
// definies dans important/interface.md, puis on les rend fonctionnelles une a une
// (cf. roadmap #2-#20). Chaque panneau ici est une maquette non fonctionnelle :
// en-tetes + lignes representatives (Selectable -> survol/clic visuels). 100 % NKGui.
// =============================================================================
#include "NKEditorKit/NkEditorKit.h"
#include "NKCode/Editor/NkTextDraw.h"

namespace nkentseu {
    namespace nkcode {

        using namespace nkentseu::editorkit;
        using namespace nkentseu::nkgui;

        // Une section = un en-tete + des lignes.
        struct ScaffoldSection { const char* header; const char* const* rows; int32 nRows; };

        // Panneau-maquette generique : titre + bandeau roadmap + sections de lignes.
        class ScaffoldPanel : public NkEditorPanel {
        public:
            ScaffoldPanel(const char* title, NkEditorDockSide side, const char* roadmap,
                          const ScaffoldSection* secs, int32 nSecs) noexcept
                : NkEditorPanel(title, side), mRoadmap(roadmap), mSecs(secs), mNSecs(nSecs) {
                SetOpen(false);   // ferme par defaut (dispo via le menu Affichage)
            }
            void OnUI(NkEditorFrameContext& ec) override {
                auto& ctx = ec.Ui();
                ec.Text(mTitle);
                if (mRoadmap) ec.Text(mRoadmap);
                ec.Separator();
                for (int32 s = 0; s < mNSecs; ++s) {
                    if (mSecs[s].header && *mSecs[s].header) ec.Text(mSecs[s].header);
                    for (int32 r = 0; r < mSecs[s].nRows; ++r)
                        Selectable(ctx, mSecs[s].rows[r], false);
                    if (s + 1 < mNSecs) ec.Separator();
                }
            }
        private:
            const char*            mRoadmap;
            const ScaffoldSection* mSecs;
            int32                  mNSecs;
        };

        // ── Donnees des maquettes (extraits representatifs d'interface.md) ──
        namespace scaffold {

            // §13 Recherche globale
            inline const char* const kSearchRows[] = {
                "Rechercher : [ renderer                 ]",
                "Remplacer  : [                          ]",
                "[Aa] [.*] [mot entier]   42 resultats / 8 fichiers",
                "v src/core/renderer.cpp   (12)",
                "v src/core/scene.cpp      (7)",
                "v src/ui/mainwindow.cpp   (3)",
            };
            inline const ScaffoldSection kSearch[] = { { "RECHERCHE & REMPLACEMENT", kSearchRows, 6 } };

            // §12 Problemes & diagnostics
            inline const char* const kProblemRows[] = {
                "x  renderer.cpp:145  fuite memoire potentielle (vertexBuffer)",
                "!  scene.cpp:89      variable inutilisee 'tmp'",
                "!  shader.glsl:32    identificateur non declare 'g_Data'",
                "i  main.cpp:23       TODO: gerer le cas d'erreur",
            };
            inline const ScaffoldSection kProblems[] = { { "PROBLEMES  (0 erreurs, 3 warnings)", kProblemRows, 4 } };

            // §7 Controle de version (Git)
            inline const char* const kGitStaged[] = {
                "+ renderer.cpp   [M]",
                "+ renderer.h     [M]",
            };
            inline const char* const kGitChanges[] = {
                "  shader.glsl    [M]",
                "  physics.cpp    [U]",
                "  scene.cpp      [M]",
            };
            inline const ScaffoldSection kGit[] = {
                { "SOURCE CONTROL  -  branche: main", kGitStaged, 2 },
                { "Changements indexes (2)",          kGitStaged, 2 },
                { "Changements (3)",                  kGitChanges, 3 },
            };

            // §5 Debogueur
            inline const char* const kDbgVars[] = {
                "v Locales",
                "  mesh        Mesh*    0x7f3a1b2c",
                "  shader      Shader*  0x7f4c0010",
                "  result      bool     true",
                "v Watch",
                "  mesh->vertexCount   12450",
            };
            inline const char* const kDbgStack[] = {
                "> Renderer::drawMesh()   renderer.cpp:145",
                "  Scene::render()        scene.cpp:89",
                "  main()                 main.cpp:23",
            };
            inline const ScaffoldSection kDebug[] = {
                { "VARIABLES",      kDbgVars,  6 },
                { "PILE D'APPELS",  kDbgStack, 3 },
            };

            // §6 Assistant IA
            inline const char* const kAiRows[] = {
                "Contexte : renderer.cpp:145",
                "Modele   : (local / cloud)",
                "/explain  /fix  /optimize  /test  /doc",
                "[ Poser une question...                ] [Envoyer]",
            };
            inline const ScaffoldSection kAi[] = { { "ASSISTANT IA", kAiRows, 4 } };

            // §14 Build & Taches
            inline const char* const kBuildRows[] = {
                "Cible : NKCode   Config : Debug   System : Windows",
                "[ Construire ] [ Recompiler ] [ Nettoyer ] [ Demarrer ]",
                "Taches : build / run / test / package / deploy",
                "Dernier build : OK (20/20)",
            };
            inline const ScaffoldSection kBuild[] = { { "BUILD & TACHES", kBuildRows, 4 } };

            // §15 Profiler
            inline const char* const kProfRows[] = {
                "FPS 144   Frame 6.9ms   GPU 4.2ms   CPU 2.7ms",
                "Draw calls 1042   Triangles 2.4M   VRAM 512MB",
                "v CPU (par fonction)",
                "  Renderer::drawMesh   38%",
                "  Scene::update        21%",
            };
            inline const ScaffoldSection kProfiler[] = { { "PROFILER", kProfRows, 5 } };

            // §10/§11 Moteur (Engine Bridge)
            inline const char* const kEngineRows[] = {
                "Moteur : Nkentseu   (deconnecte)",
                "[ Build & Run ] [ Hot Reload ] [ Sync Assets ]",
                "Logs moteur : (temps reel)",
                "Shader live debugger : pbr.frag",
                "Simulation : entites / contacts / contraintes",
            };
            inline const ScaffoldSection kEngine[] = { { "ENGINE BRIDGE", kEngineRows, 5 } };

            // §9 Extensions & Packages
            inline const char* const kExtRows[] = {
                "[ Rechercher des extensions...        ]",
                "CMake Tools          installee",
                "GLSL Language Support",
                "Nkentseu Bridge      local",
                "Paquets Jenga (dependson) : resolution transitive",
            };
            inline const ScaffoldSection kExtensions[] = { { "EXTENSIONS & PAQUETS", kExtRows, 5 } };

        } // namespace scaffold

    } // namespace nkcode
} // namespace nkentseu
