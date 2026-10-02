#pragma once
// =============================================================================
// NkLoading.h — Écran de Chargement (section 14). Affiché ENTRE la sélection d'un
// workspace et l'éditeur. Progression PILOTÉE PAR LE VRAI chargement :
//   0 Lecture du .jenga        (LoadFolder synchrone)
//   1 Analyse des projets      (jenga info async -> projCount réel, erreur réelle)
//   2 Détection des toolchains  (déclarées dans le .jenga -> connues au parse)
//   3 IntelliSense  4 Indexation  5 Extensions  6 Session   (sous-systèmes roadmap : instant)
// Escape = annuler -> retour Launcher. Erreur .jenga -> état d'erreur inline.
// L'ÉTAT + la logique sont ici ; le DESSIN est dans NkHome.h (accès logo/thème).
// =============================================================================
#include "NKCode/Project/NkCodeState.h"
#include "NKCode/Shell/NkI18n.h"
#include "NKContainers/String/NkFormat.h" // NkPrintf (formatage maison)

namespace nkentseu {
	namespace nkcode {

		struct NkLoadingState {
				static const int32 STEPS = 7;
				bool active = false;
				bool finished = false; // -> l'appelant bascule vers l'éditeur
				bool error = false;
				NkPath folder;
				NkString wsName;
				int32 step = 0;			 // étape courante (0..STEPS)
				float32 stepTimer = 0.f; // temps passé sur l'étape courante
				float32 anim = 0.f;		 // horloge d'animation (spinner + fond)
				int32 projCount = 0;
				// Détails d'erreur
				NkString errLine, errHint;

				static const char *StepKey(int32 i) {
					static const char *K[] = {"load.s0", "load.s1", "load.s2", "load.s3",
											  "load.s4", "load.s5", "load.s6"};
					return (i >= 0 && i < STEPS) ? K[i] : "";
				}

				// Libellé affiché d'une étape : « Lecture de <ws>.jenga », « Analyse des projets (N projets) », …
				NkString StepLabel(int32 i) const {
					if (i == 0)
						return NkPrintf(NkT("load.s0"), wsName.CStr()); // NkPrintf maison
					if (i == 1 && projCount > 0)
						return NkPrintf("%s (%d %s)", NkT("load.s1"), projCount, NkT("load.projword"));
					return NkString(NkT(StepKey(i)));
				}

				// openJenga (optionnel) : sélectionne un .jenga précis du dossier (cas « plusieurs workspaces »).
				void Start(const NkPath &f, NkCodeState *st, const char *openJenga = nullptr) {
					active = true;
					finished = false;
					error = false;
					step = 0;
					stepTimer = 0.f;
					anim = 0.f;
					projCount = 0;
					errLine = NkString();
					errHint = NkString();
					folder = f;
					// Étape 0 : lecture réelle du .jenga.
					// Mutation de banc NK_NKCODE_MUTATION=exige-jenga : l'ancienne exigence
					// d'un workspace (avant la PR #54) -- le temoin (o1) doit rougir.
					if (NkCodeState::MutationNkCode("exige-jenga") && st->LoadFolder(f) && !st->HasWorkspace()) {
						active = true;
						error = true;
						wsName = f.GetFileName();
						errLine = NkT("load.err.nows");
						return;
					}
					if (!st->LoadFolder(f)) {
						// LoadFolder ne refuse QUE un chemin qui n'est pas un dossier (un
						// dossier sans .jenga s'ouvre en edition simple) : le dire tel quel.
						active = true;
						error = true;
						wsName = f.GetFileName();
						errLine = NkPrintf(NkT("load.err.nofolder"), f.ToString().CStr());
						return;
					}
					if (openJenga && *openJenga) { // sélectionne le workspace précis puis relance `jenga info`
						// Compare sans egard aux separateurs ni a la casse : wsPaths vient
						// de GetEntries (« \ » sous Windows), l'argument souvent de « / ».
						auto Pareil = [](const NkString &a, const char *b) {
							usize i = 0;
							for (; i < a.Length() && b[i]; ++i) {
								char x = a.CStr()[i], y = b[i];
								x = x == '\\' ? '/' : (x >= 'A' && x <= 'Z' ? (char)(x - 'A' + 'a') : x);
								y = y == '\\' ? '/' : (y >= 'A' && y <= 'Z' ? (char)(y - 'A' + 'a') : y);
								if (x != y)
									return false;
							}
							return i == a.Length() && b[i] == '\0';
						};
						for (usize i = 0; i < st->wsPaths.Size(); ++i)
							if (Pareil(st->wsPaths[i], openJenga)) {
								st->wsIdx = (int32)i;
								break;
							}
						st->OpenPath(NkPath(openJenga));
						st->RequestReload();
					}
					const NkString jp = (!st->wsPaths.Empty())
											? st->wsPaths[st->wsIdx < (int32)st->wsPaths.Size() ? st->wsIdx : 0]
											: NkString();
					wsName = jp.Empty() ? st->root.GetFileName() : NkCodeState::WorkspaceNameOf(jp.CStr());
					// TOUJOURS mémoriser dans les RÉCENTS : un workspace ouvert par DoLoad
					// (argument, « Ouvrir », dernier workspace au démarrage) n'y entrait
					// jamais -> il disparaissait du launcher dès qu'il n'était plus courant.
					st->AddRecent(jp.Empty() ? f.ToString() : jp);
					// LoadFolder a déjà relancé `jenga info` (RequestReload). Le poll se fait dans Tick.
				}

				void Cancel() {
					active = false;
					finished = false;
					error = false;
				}

				void Tick(NkCodeState *st, float32 dt) {
					if (!active || error || finished)
						return;
					anim += dt;
					stepTimer += dt;
					// Dossier SANS workspace Jenga (mode edition simple) : rien a analyser
					// (LoadProjects() est deja no-op) -> on traverse juste les etapes
					// jusqu'a la fin, sans jamais risquer le garde-fou 45s de l'etape 1
					// (qui suppose un `jenga info` reellement lance).
					if (!st->HasWorkspace()) {
						if (stepTimer > 0.25f)
							finished = true;
						return;
					}
					st->LoadProjects(); // DEMARRE `jenga info` (idempotent) — sinon rien ne le lance pendant le
										// chargement
					st->PollProjects(); // puis draine chaque frame
					auto adv = [&]() {
						++step;
						stepTimer = 0.f;
					};
					switch (step) {
						case 0:
							if (stepTimer > 0.20f)
								adv();
							break; // lecture du .jenga (faite dans Start)
						case 1:	   // analyse des projets (réel)
							if (st->InfoParsed()) {
								projCount = st->TotalProjectCount(); // meme definition que la carte du launcher
								if (st->InfoHasError() && projCount == 0) {
									error = true;
									errLine = st->InfoErrorLine();
									errHint = HintOf(errLine);
									return;
								}
								if (stepTimer > 0.25f)
									adv();
							} else if (stepTimer > 45.f) { // garde-fou : jenga info bloqué (gros workspaces = marge)
								error = true;
								errLine = NkT("load.err.timeout");
								errHint = NkString();
							}
							break;
						case 2:
							if (stepTimer > 0.30f)
								adv();
							break; // toolchains (déclarées dans le .jenga)
						case 3:
							if (stepTimer > 0.25f)
								adv();
							break; // IntelliSense (roadmap)
						case 4:
							if (stepTimer > 0.25f)
								adv();
							break; // indexation (roadmap)
						case 5:
							if (stepTimer > 0.20f)
								adv();
							break; // extensions (roadmap)
						case 6:
							if (stepTimer > 0.20f) {
								finished = true;
							}
							break; // session -> fin (bascule geree par DrawHome)
					}
				}

				// Fraction de progression (0..1), l'étape active comptant à moitié.
				float32 Progress() const {
					if (error)
						return (float32)step / (float32)STEPS;
					const float32 base = (float32)step + 0.5f;
					float32 p = base / (float32)STEPS;
					if (p > 1.f)
						p = 1.f;
					return p;
				}

				// Cause probable, best-effort, à partir du message d'erreur Python.
				static NkString HintOf(const NkString &e) {
					if (e.Contains("is not defined") || e.Contains("NameError"))
						return NkT("load.hint.import");
					if (e.Contains("SyntaxError"))
						return NkT("load.hint.syntax");
					if (e.Contains("No module") || e.Contains("ModuleNotFound"))
						return NkT("load.hint.module");
					if (e.Contains("IndentationError"))
						return NkT("load.hint.indent");
					return NkString();
				}
		};

	} // namespace nkcode
} // namespace nkentseu
