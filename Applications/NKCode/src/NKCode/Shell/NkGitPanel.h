#pragma once
// =============================================================================
// NkGitPanel.h — panneau « Controle de version » (spec §7, roadmap #9).
//
// Remplace la maquette. TOUT ce qui s'affiche vient de commandes git reelles :
// branche courante, fichiers modifies, historique. Rien n'est simule — quand
// git est absent ou que le dossier n'est pas un depot, le panneau le DIT.
//
// Toutes les commandes passent par `git -C "<racine>"` plutot que par un
// `cd ... &&` : c'est la forme deja retenue ailleurs dans l'IDE, elle evite les
// differences de shell entre Windows et Unix et ne depend d'aucun repertoire
// courant.
//
// ── Ce qui est DELIBEREMENT absent ─────────────────────────────────────────
// Pas de `push`, pas de `pull`, pas de changement de branche. Ces trois-la
// touchent un depot distant ou l'arbre de travail de facon difficilement
// reversible, et le panneau n'a pas encore de quoi presenter un conflit ni une
// divergence. Les proposer serait offrir un bouton qui, le jour ou ca tourne
// mal, laisse l'utilisateur sans recours dans une interface qui ne sait pas
// lui expliquer ce qui s'est passe. Le terminal integre reste la pour ca.
// =============================================================================
#include "NKCode/Project/NkCodeState.h"
#include "NKEditorKit/NkEditorKit.h"
// X11 définit les MACROS None/Status : l'enum Cmd { None, Status, ... } de ce
// fichier explosait en « expected identifier » sur Linux (probe CI 2026-08-11).
// Le nettoyeur maison neutralise ces macros — et il doit venir APRÈS le
// dernier include qui touche X11 (les deux ci-dessus finissent par inclure
// NKWindow/Xlib, qui REPOSE les macros : en tête de fichier il ne servait à
// rien — 2e leçon du même probe).
#include "NKPlatform/NkX11Clean.h"
// `Status`, lui, ne PEUT PAS être neutralisé globalement : Xutil s'en sert
// comme TYPE (leçon déjà payée, documentée dans NkX11Clean.h). Undef LOCAL :
// ce header vient toujours après les en-têtes X11 de sa TU et n'utilise
// aucun type X11 — 3e leçon du même probe.
#ifdef Status
#undef Status
#endif

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu::editorkit;
		using namespace nkentseu::nkgui;

		class NkGitPanel : public NkEditorPanel {
			public:
				NkGitPanel(NkCodeState *st) noexcept
					: NkEditorPanel("Controle de version", NkEditorDockSide::NK_LEFT), mS(st) {
					SetOpen(false);
				}

				void OnUI(NkEditorFrameContext &ec) override {
					auto &ctx = ec.Ui();
					if (!mS || !mS->HasWorkspace()) {
						ec.Text("Aucun workspace ouvert.");
						return;
					}
					Poll(ec.dt);

					// ── En-tete : branche REELLE ────────────────────────────────────
					ec.Text(mBranch.Empty() ? "Branche : (inconnue)"
											: (NkString("Branche : ") + mBranch.CStr()).CStr());
					if (mNoGit) {
						// Distinguer les deux causes : « git absent » et « pas un depot »
						// n'appellent pas la meme action de la part de l'utilisateur.
						ec.Text(mDiag.Empty() ? "git indisponible ici." : mDiag.CStr());
						return;
					}
					if (ec.Button("Rafraichir"))
						mTimer = 999.f;
					ec.Separator();

					// ── Fichiers ────────────────────────────────────────────────────
					if (mFiles.Empty()) {
						ec.Text("Aucune modification.");
					} else {
						ec.Text(NkPrintf("%d fichier(s) modifie(s)", (int32)mFiles.Size()).CStr());
						for (usize i = 0; i < mFiles.Size(); ++i) {
							const GitFile &g = mFiles[i];
							// « XY chemin » : les deux lettres de git (index, arbre de
							// travail) sont conservees telles quelles. Les traduire ferait
							// perdre une notation que tout utilisateur de git connait deja.
							const NkString lbl = NkPrintf("%s  %s", g.code.CStr(), g.path.CStr());
							if (Selectable(ctx, lbl.CStr(), false))
								Open(g.path);
						}
						if (ec.Button("Tout indexer")) {
							Run(NkString("add -A"));
							mTimer = 999.f;
						}
					}
					ec.Separator();

					// ── Validation ──────────────────────────────────────────────────
					InputText(ctx, "Message", mMsg, static_cast<int32>(sizeof(mMsg)));
					if (ec.Button("Valider")) {
						if (!mMsg[0]) {
							mS->status = NkString("Message de validation vide.");
							mS->statusError = true;
						} else {
							// Le message part par -m, entre guillemets : un message contenant
							// un guillemet casserait la ligne, on les neutralise.
							NkString m;
							for (const char *p = mMsg; *p; ++p)
								if (*p != '"')
									m += *p;
							Run(NkString("commit -m \"") + m.CStr() + "\"");
							mMsg[0] = 0;
							mTimer = 999.f;
						}
					}
					ec.Separator();

					// ── Historique ──────────────────────────────────────────────────
					ec.Text("Derniers commits");
					for (usize i = 0; i < mLog.Size(); ++i)
						Selectable(ctx, mLog[i].CStr(), false);

					if (!mLast.Empty()) {
						ec.Separator();
						ec.Text(mLast.CStr()); // sortie de la derniere commande, telle quelle
					}
				}

			private:
				struct GitFile {
						NkString code; // les deux lettres de `git status --porcelain`
						NkString path;
				};

				// Une seule commande a la fois : git serialise de toute facon ses acces
				// a l'index, et lancer deux `add` concurrents produirait un verrou.
				void Run(const NkString &args) {
					if (mProc.Running())
						return;
					mAcc.Clear();
					mPending = Cmd::Action;
					mProc.Start(NkString("git -C \"") + mS->root.ToString().CStr() + "\" " + args.CStr());
				}

				void Poll(float32 dt) {
					mTimer += dt;
					mProc.Drain(mAcc);
					if (mProc.Running())
						return;

					if (mPending != Cmd::Aucune) {
						Consume();
						mPending = Cmd::Aucune;
						mAcc.Clear();
					}
					// L'historique s'enchaine juste apres le statut : une seule commande a
					// la fois, git serialisant de toute facon ses acces a l'index.
					if (mNeedLog) {
						mNeedLog = false;
						mAcc.Clear();
						mPending = Cmd::Journal;
						mProc.Start(NkString("git -C \"") + mS->root.ToString().CStr() +
									"\" log --oneline -n 12");
						return;
					}
					// Rafraichissement periodique : git status scanne le disque, on ne le
					// relance pas a chaque frame. Trois secondes, comme la detection de
					// modifications deja en place ailleurs dans l'IDE.
					if (mTimer < 3.f)
						return;
					mTimer = 0.f;
					mAcc.Clear();
					mPending = Cmd::Etat;
					mProc.Start(NkString("git -C \"") + mS->root.ToString().CStr() +
								"\" status --porcelain --branch");
				}

				void Consume() {
					if (mPending == Cmd::Action) {
						// La sortie d'une action (add/commit) est montree telle quelle :
						// c'est git qui explique le mieux ce que git vient de faire.
						mLast.Clear();
						for (usize i = 0; i < mAcc.Size() && i < 4; ++i) {
							if (!mLast.Empty())
								mLast += "\n";
							mLast += mAcc[i];
						}
						if (mProc.ExitCode() != 0 && mLast.Empty())
							mLast = NkString("La commande git a echoue.");
						return;
					}
					if (mPending == Cmd::Journal) {
						mLog.Clear();
						for (usize i = 0; i < mAcc.Size(); ++i)
							if (!mAcc[i].Empty())
								mLog.PushBack(mAcc[i]);
						return;
					}
					// ── Status ──────────────────────────────────────────────────────
					if (mProc.ExitCode() != 0) {
						mNoGit = true;
						mDiag.Clear();
						for (usize i = 0; i < mAcc.Size() && mDiag.Empty(); ++i)
							if (!mAcc[i].Empty())
								mDiag = mAcc[i]; // « not a git repository », « git introuvable »...
						return;
					}
					mNoGit = false;
					mFiles.Clear();
					for (usize i = 0; i < mAcc.Size(); ++i) {
						const NkString &l = mAcc[i];
						if (l.Size() < 3)
							continue;
						const char *c = l.CStr();
						if (c[0] == '#' && c[1] == '#') {
							// « ## branche...suivi [ahead 1] » -> on ne garde que le nom.
							const char *b = c + 2;
							while (*b == ' ')
								++b;
							NkString nom;
							for (; *b && *b != '.' && *b != ' '; ++b)
								nom += *b;
							mBranch = nom;
							continue;
						}
						GitFile g;
						g.code = NkString(c, 2);
						const char *p = c + 2;
						while (*p == ' ')
							++p;
						g.path = NkString(p);
						if (!g.path.Empty())
							mFiles.PushBack(g);
					}
					// L'historique se rafraichit avec le reste, mais une seule commande a
					// la fois : on l'enchaine au prochain tour.
					mNeedLog = true;
				}

				void Open(const NkString &rel) {
					if (!mS)
						return;
					const NkPath p = mS->root / rel.CStr();
					if (NkFile::Exists(p.ToString().CStr()))
						mS->OpenPath(p);
				}

				// Aucun de ces noms ne doit heurter les macros de X11 (None, Status,
				// Success, Bool...) : ce panneau se compile aussi sur le backend XLib.
				enum class Cmd { Aucune, Etat, Journal, Action };

				NkCodeState *mS = nullptr;
				NkProcess mProc;
				NkVector<NkString> mAcc;
				Cmd mPending = Cmd::Aucune;
				float32 mTimer = 999.f; // premier rafraichissement immediat
				bool mNoGit = false, mNeedLog = false;
				NkString mBranch, mDiag, mLast;
				NkVector<GitFile> mFiles;
				NkVector<NkString> mLog;
				char mMsg[256] = {0};
		};

	} // namespace nkcode
} // namespace nkentseu
