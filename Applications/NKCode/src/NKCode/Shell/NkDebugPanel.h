#pragma once
// =============================================================================
// NkDebugPanel.h — panneau « Debogueur » (spec §5, roadmap #10).
//
// ── Ce que ce panneau EST, et ce qu'il n'est pas ────────────────────────────
// Il liste les POINTS D'ARRET reels et permet d'y naviguer. C'est la partie du
// debogueur que NKCode possede vraiment : les points d'arret vivent dans les
// documents ouverts (`doc.breakpoints`) et sont deja transmis a `jenga gdb`
// sous forme de `--break fichier:ligne`.
//
// Il n'affiche NI variables, NI pile d'appels, NI threads, NI memoire — les
// quatre sous-panneaux que decrit la spec. Ce n'est pas un oubli : ces vues
// supposent une session GDB PILOTEE par l'IDE (protocole MI2 sur des tubes,
// commandes envoyees, reponses analysees). Aujourd'hui NKCode lance gdb dans
// le terminal integre et lui rend la main ; il n'a aucun canal pour interroger
// l'etat du programme arrete.
//
// Les dessiner quand meme, remplis de zeros ou d'exemples, donnerait un
// debogueur qui a l'air de fonctionner et ne rapporte rien de vrai — le defaut
// exact que la maquette avait, et qu'on remplace ici.
// =============================================================================
#include "NKCode/Project/NkCodeState.h"
#include "NKEditorKit/NkEditorKit.h"

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu::editorkit;
		using namespace nkentseu::nkgui;

		class NkDebugPanel : public NkEditorPanel {
			public:
				NkDebugPanel(NkCodeState *st) noexcept
					: NkEditorPanel("Debogueur", NkEditorDockSide::NK_LEFT), mS(st) {
					SetOpen(false);
				}

				void OnUI(NkEditorFrameContext &ec) override {
					auto &ctx = ec.Ui();
					if (!mS) {
						ec.Text("Indisponible.");
						return;
					}

					// ── Points d'arret, tous fichiers ouverts confondus ──────────────
					int32 total = 0;
					for (usize i = 0; i < mS->files.Size(); ++i)
						total += static_cast<int32>(mS->files[i].doc.breakpoints.Size());

					ec.Text(NkPrintf("Points d'arret : %d", total).CStr());
					ec.Separator();

					if (total == 0) {
						ec.Text("Aucun point d'arret.");
						ec.Text("Cliquez dans la gouttiere d'un fichier pour en poser un.");
					} else {
						for (usize i = 0; i < mS->files.Size(); ++i) {
							OpenFile &f = mS->files[i];
							for (usize b = 0; b < f.doc.breakpoints.Size(); ++b) {
								const int32 ln = f.doc.breakpoints[b];
								// Ligne affichee en 1-base, comme partout ailleurs (editeur,
								// compilateur, gdb) ; le stockage, lui, est 0-base.
								const NkString lbl = NkPrintf("%s:%d", f.Name().CStr(), ln + 1);
								if (Selectable(ctx, lbl.CStr(), false))
									Jump(static_cast<int32>(i), ln);
							}
						}
						if (ec.Button("Tout retirer"))
							for (usize i = 0; i < mS->files.Size(); ++i)
								mS->files[i].doc.breakpoints.Clear();
					}

					ec.Separator();
					// Dire franchement ce qui n'est pas la, plutot que de laisser croire
					// que le panneau est vide faute de session en cours.
					ec.Text("Variables, pile d'appels, threads et memoire ne sont pas");
					ec.Text("disponibles : ils exigent une session GDB pilotee par l'IDE.");
					ec.Text("Le deboguage passe aujourd'hui par le terminal integre (F5).");
				}

			private:
				void Jump(int32 fileIdx, int32 line) {
					if (!mS || fileIdx < 0 || fileIdx >= static_cast<int32>(mS->files.Size()))
						return;
					mS->active = fileIdx;
					OpenFile &f = mS->files[static_cast<usize>(fileIdx)];
					f.doc.curLine = line;
					f.doc.curCol = 0;
					f.doc.selLine = line;
					f.doc.selCol = 0;
					f.doc.ClampCursor();
					f.doc.ResetEditRun();
					f.doc.wantReveal = true;
				}

				NkCodeState *mS = nullptr;
		};

	} // namespace nkcode
} // namespace nkentseu
