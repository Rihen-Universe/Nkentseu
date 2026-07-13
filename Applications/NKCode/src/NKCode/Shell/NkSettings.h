// =============================================================================
// NkSettings.h — Parametres du Launcher (plein cadre, nav==12). Sous-sidebar de
// categories (ancres) + page defilable : General, Chemins, Jenga, Theme, Git,
// Comptes. Detections REELLES (version Jenga, chemins jenga/git, coeurs CPU,
// compte GitHub via `gh`). Persistance dans ~/.nkcode/settings.cfg (key=value).
// Evite les doublons : « Toolchain » renvoie au gestionnaire existant (nav==10).
// =============================================================================
#pragma once
#include "NKCode/Shell/NkUi.h"
#include "NKEditorKit/NkEditorScrollbar.h"
#include "NKCode/Shell/NkOpenWs.h"		 // NkOwEditA, NkOwIco, NkWizLabel, Home
#include "NKCode/Shell/NkNewWorkspace.h" // NkNewWsState::TcWhich / TcRun
#include "NKCode/Shell/NkI18n.h"		 // NkT() : traductions multi-langue (temps reel)
#include "NKCode/Project/NkCodeState.h"
#include "NKCode/Shell/Dialogs.h"
#include "NKCode/Shell/NkShell.h" // NkCodeShellRun (ouvrir un dossier)
#include "NKThreading/NkThread.h"
#include "NKCore/NkAtomic.h"			  // NkAtomicBool
#include "NKContainers/String/NkFormat.h" // NkPrintf (formatage maison)
#include "NKPlatform/NkPlatformConfig.h"  // GetPlatformCapabilities (coeurs CPU maison, ex-std::thread)

namespace nkentseu {
	namespace nkcode {

		struct NkSettingsState {
				bool loaded = false, detected = false;
				// ── General ──
				int32 lang = 0;		   // 0 Francais, 1 English
				int32 openStartup = 0; // 0 Launcher, 1 Dernier WS
				char recentsMax[8] = "50";
				int32 groupBy = 0; // 0 Date, 1 Plateforme, 2 Langage
				// ── Chemins par defaut ──
				char projDir[320] = "";
				char buildDir[128] = "Build/";
				char cacheDir[320] = "";
				// ── Jenga ──
				char jengaPath[320] = "jenga";
				char gitPath[320] = "git";
				int32 jobs = 0; // 0 = auto (CPU-1)
				bool regTc = true, incCache = true, daemon = true;
				// ── Theme ──
				int32 theme = 0;			 // 0 Dark Pro, 1 Dark, 2 Midnight, 3 Light
				char accent[10] = "#F79A28"; // couleur d'accent par defaut (orange Dark Pro)
				int32 transparency = 80;	 // (obsolete : plus utilise, garde pour compat cfg)
				bool anim = true;
				// ── Git ──
				bool gitIndicators = true, gitFetch = true;
				// ── Comptes ──
				bool cloudSync = false;
				// ── Detections reelles ──
				NkString jengaVersion, jengaResolved, gitResolved, ghUser, glUser;
				bool ghConnected = false, glConnected = false;
				int32 cpuCount = 0;
				// ── UI ──
				int32 cat = 0;										  // categorie active (sous-sidebar)
				float32 scroll = 0.f, scrollMax = 0.f, jumpTo = -1.f; // jumpTo >=0 -> defile vers l'ancre
				int32 comboOpen = -1;								  // id du combo ouvert (-1 aucun)
				NkRect comboR{};
				const char *const *comboOpts = nullptr;
				int32 comboN = 0;
				int32 *comboSel = nullptr;
				bool comboJustOpened = false;
				bool sliderDrag = false;
				float32 anchor[7] = {}; // y (relatif contenu) de chaque section, pour la sous-sidebar
				bool dirty = false;		// reglages modifies non appliques
				// ── Focus des champs texte (un SEUL edite a la fois, comme les autres panneaux) ──
				int32 focusField = -1, fieldCaret = 0;
				float32 fieldBlink = 0.f;
				bool fieldClaim = false;
				// ── Detection sur thread de fond (anti-gel) ──
				threading::NkThread detThread;
				NkAtomicBool detBusy, detDone;
				NkString rjVer, rjRes, rgRes, rghU, rglU;
				bool rgh = false, rgl = false; // resultats transferes par le thread

				static NkString File() {
					return (NkPath(NkOpenWsState::Home().CStr()) / ".nkcode" / "settings.cfg").ToString();
				}

				// ── Persistance simple key=value ──
				void Save() {
					NkDirectory::CreateRecursive((NkPath(NkOpenWsState::Home().CStr()) / ".nkcode").ToString().CStr());
					NkString o;
					auto kv = [&](const char *k, const NkString &v) {
						o += k;
						o += "=";
						o += v;
						o += "\n";
					};
					auto ki = [&](const char *k, int32 v) { kv(k, NkPrintf("%d", v)); }; // NkPrintf maison
					ki("lang", lang);
					ki("openStartup", openStartup);
					kv("recentsMax", NkString(recentsMax));
					ki("groupBy", groupBy);
					kv("projDir", NkString(projDir));
					kv("buildDir", NkString(buildDir));
					kv("cacheDir", NkString(cacheDir));
					kv("jengaPath", NkString(jengaPath));
					kv("gitPath", NkString(gitPath));
					ki("jobs", jobs);
					ki("regTc", regTc);
					ki("incCache", incCache);
					ki("daemon", daemon);
					ki("theme", theme);
					kv("accent", NkString(accent));
					ki("transparency", transparency);
					ki("anim", anim);
					ki("gitIndicators", gitIndicators);
					ki("gitFetch", gitFetch);
					ki("cloudSync", cloudSync);
					NkFile::WriteAllText(NkPath(File().CStr()), o);
					dirty = false;
					MirrorToCloud(); // si cloudSync actif : recopie dans ~/Documents/NKCode (OneDrive/Dropbox)
				}

				// Renseigne les chemins par défaut s'ils sont vides : dossier d'enregistrement (~/Projects)
				// et cache Jenga réel (~/.jenga/cache ou override). Rend ces réglages fonctionnels dès le 1er
				// lancement.
				void EnsureDefaults() {
					if (projDir[0] == '\0') {
						const NkString d = (NkPath(NkOpenWsState::Home().CStr()) / "Projects").ToString();
						NkStrCopy(projDir, sizeof(projDir), d.CStr()); // copie bornée maison (NkText.h)
					}
					if (cacheDir[0] == '\0') {
						const NkString d = NkOpenWsState::JengaCacheDir();
						NkStrCopy(cacheDir, sizeof(cacheDir), d.CStr());
					}
					if (StrEq(accent, "#00d4ff"))
						NkStrCopy(accent, sizeof(accent), "#F79A28"); // migre l'ancien accent cyan par defaut
					// jengaPath REEL : resout le vrai binaire (jenga embarque, override JENGA_EXE, sinon PATH)
					// au lieu du litteral "jenga". Prepare l'integration future (jenga+Python dans tools/).
					if (jengaPath[0] == '\0' || StrEq(jengaPath, "jenga")) {
						const NkString real = NkOpenWsState::DefaultJengaPath();
						if (!real.Empty())
							NkStrCopy(jengaPath, sizeof(jengaPath), real.CStr());
					}
				}

				// Ouvre un dossier dans l'explorateur de fichiers (cross-platform).
				static void RevealFolder(const char *path) {
					if (!path || !path[0])
						return;
					NkString c;
#if defined(_WIN32)
					c = "explorer \"";
					c += path;
					c += "\"";
#elif defined(__APPLE__)
					c = "open \"";
					c += path;
					c += "\"";
#else
					c = "xdg-open \"";
					c += path;
					c += "\"";
#endif
					NkCodeShellRun(c.CStr());
				}

				// Vide le cache Jenga : supprime le contenu du dossier puis le recree (fonctionnel, reel).
				void ClearCache() {
					if (cacheDir[0] == '\0')
						return;
					NkDirectory::Delete(cacheDir, true);
					NkDirectory::CreateRecursive(cacheDir);
				}

				void Load() {
					loaded = true;
					const NkString path = File();
					if (!NkFile::Exists(path.CStr())) {
						EnsureDefaults();
						return;
					}
					const NkString js = NkFile::ReadAllText(NkPath(path.CStr()));
					NkString line;
					for (const char *s = js.CStr();; ++s) {
						if (*s == '\n' || *s == '\0') {
							const char *c = line.CStr();
							const char *eq = nullptr;
							for (const char *p = c; *p; ++p)
								if (*p == '=') {
									eq = p;
									break;
								}
							if (eq) {
								NkString k;
								for (const char *p = c; p < eq; ++p)
									k += *p;
								NkString v;
								for (const char *p = eq + 1; *p; ++p)
									v += *p;
								auto is = [&](const char *n) { return StrEq(k.CStr(), n); };
								auto cp = [&](char *dst, int32 cap) { NkStrCopy(dst, (usize)cap, v.CStr()); };
								// Conversion maison (NkAtoi ne gère pas le signe : on le traite ici).
								const int32 iv =
									(v.CStr()[0] == '-') ? -NkAtoi(v.CStr() + 1) : NkAtoi(v.CStr());
								if (is("lang"))
									lang = iv;
								else if (is("openStartup"))
									openStartup = iv;
								else if (is("recentsMax"))
									cp(recentsMax, sizeof(recentsMax));
								else if (is("groupBy"))
									groupBy = iv;
								else if (is("projDir"))
									cp(projDir, sizeof(projDir));
								else if (is("buildDir"))
									cp(buildDir, sizeof(buildDir));
								else if (is("cacheDir"))
									cp(cacheDir, sizeof(cacheDir));
								else if (is("jengaPath"))
									cp(jengaPath, sizeof(jengaPath));
								else if (is("gitPath"))
									cp(gitPath, sizeof(gitPath));
								else if (is("jobs"))
									jobs = iv;
								else if (is("regTc"))
									regTc = iv != 0;
								else if (is("incCache"))
									incCache = iv != 0;
								else if (is("daemon"))
									daemon = iv != 0;
								else if (is("theme"))
									theme = iv;
								else if (is("accent"))
									cp(accent, sizeof(accent));
								else if (is("transparency"))
									transparency = iv;
								else if (is("anim"))
									anim = iv != 0;
								else if (is("gitIndicators"))
									gitIndicators = iv != 0;
								else if (is("gitFetch"))
									gitFetch = iv != 0;
								else if (is("cloudSync"))
									cloudSync = iv != 0;
							}
							line = NkString();
							if (*s == '\0')
								break;
						} else if (*s != '\r')
							line += *s;
					}
					EnsureDefaults(); // un settings.cfg existant avec projDir/cacheDir vides -> on renseigne les
									  // defauts
				}

				void Reset() {
					NkSettingsState d; // valeurs par defaut
					lang = d.lang;
					openStartup = d.openStartup;
					NkStrCopy(recentsMax, sizeof(recentsMax), d.recentsMax); // copie bornée maison (NkText.h)
					groupBy = d.groupBy;
					projDir[0] = '\0';
					NkStrCopy(buildDir, sizeof(buildDir), d.buildDir);
					cacheDir[0] = '\0';
					NkStrCopy(jengaPath, sizeof(jengaPath), d.jengaPath);
					NkStrCopy(gitPath, sizeof(gitPath), d.gitPath);
					jobs = d.jobs;
					regTc = d.regTc;
					incCache = d.incCache;
					daemon = d.daemon;
					theme = d.theme;
					NkStrCopy(accent, sizeof(accent), d.accent);
					transparency = d.transparency;
					anim = d.anim;
					gitIndicators = d.gitIndicators;
					gitFetch = d.gitFetch;
					cloudSync = d.cloudSync;
					dirty = true;
				}

				// ── Detections REELLES (popen lents) — executees sur un thread de fond (anti-gel) ──
				void DetectSync() {
					// jenga : chemin + version
					NkString jp = NkNewWsState::TcWhich("jenga");
					if (!jp.Empty())
						jengaResolved = jp;
					{
						NkString inner = "jenga --version 2>&1";
#if defined(_WIN32)
						NkString cmd = "\"";
						cmd += inner;
						cmd += "\""; // fix cmd /c
#else
						NkString cmd = inner;
#endif
						const NkString oRaw = NkNewWsState::TcRun(cmd.CStr());
						// 1) Retire les séquences ANSI (ESC [ ... lettre) : sinon on capte les chiffres
						//    des codes couleur (ex "\x1b[36m" -> "36") au lieu de la vraie version.
						NkString o;
						for (const char *c = oRaw.CStr(); *c;) {
							if ((unsigned char)*c == 0x1b) {
								++c;
								if (*c == '[')
									++c;
								while (*c && !((*c >= '@' && *c <= '~')))
									++c;
								if (*c)
									++c;
							} else {
								o += *c;
								++c;
							}
						}
						// 2) Premier motif de version RÉEL : X.Y[.Z] (point obligatoire, >= 3 caractères).
						NkString v;
						for (const char *c = o.CStr(); *c; ++c) {
							if (*c >= '0' && *c <= '9') {
								const char *p = c;
								NkString cand;
								bool dot = false;
								for (; *p && ((*p >= '0' && *p <= '9') || *p == '.'); ++p) {
									cand += *p;
									if (*p == '.')
										dot = true;
								}
								if (dot && cand.Size() >= 3) {
									v = cand;
									break;
								}
								c = p;
								if (!*c)
									break;
							}
						}
						jengaVersion = v;
					}
					// git : chemin
					{
						NkString g = NkNewWsState::TcWhich("git");
						if (!g.Empty())
							gitResolved = g;
					}
					// GitHub via gh (facultatif) : `gh auth status` -> "Logged in ... as USER"
					{
						NkString inner = "gh auth status 2>&1";
#if defined(_WIN32)
						NkString cmd = "\"";
						cmd += inner;
						cmd += "\"";
#else
						NkString cmd = inner;
#endif
						const NkString o = NkNewWsState::TcRun(cmd.CStr());
						ghConnected = o.Contains("Logged in");
						if (ghConnected) {
							const char *a = o.CStr();
							const char *p = nullptr;
							for (const char *c = a; *c; ++c)
								if (c[0] == 'a' && c[1] == 's' && c[2] == ' ') {
									p = c + 3;
									break;
								}
							if (p) {
								NkString u2;
								for (; *p && *p != ' ' && *p != '\n' && *p != '('; ++p)
									u2 += *p;
								ghUser = u2;
							}
						}
					}
					// GitLab via glab (facultatif) : `glab auth status` -> "Logged in to gitlab.com as USER"
					{
						NkString inner = "glab auth status 2>&1";
#if defined(_WIN32)
						NkString cmd = "\"";
						cmd += inner;
						cmd += "\"";
#else
						NkString cmd = inner;
#endif
						const NkString o = NkNewWsState::TcRun(cmd.CStr());
						glConnected = o.Contains("Logged in");
						if (glConnected) {
							const char *a = o.CStr();
							const char *p = nullptr;
							for (const char *c = a; *c; ++c)
								if (c[0] == 'a' && c[1] == 's' && c[2] == ' ') {
									p = c + 3;
									break;
								}
							if (p) {
								NkString u2;
								for (; *p && *p != ' ' && *p != '\n' && *p != '('; ++p)
									u2 += *p;
								glUser = u2;
							}
						}
					}
				}

				// Lance la detection sur un thread de fond (instance jetable) -> l'UI ne gele pas.
				void StartDetectAsync() {
					if (detBusy.Load())
						return;
					if (detThread.Joinable())
						detThread.Join();
					detDone.Store(false);
					detBusy.Store(true);
					detThread = threading::NkThread([this](void *) {
						NkSettingsState tmp;
						tmp.DetectSync(); // popen lents hors thread UI
						rjVer = tmp.jengaVersion;
						rjRes = tmp.jengaResolved;
						rgRes = tmp.gitResolved;
						rghU = tmp.ghUser;
						rgh = tmp.ghConnected;
						rglU = tmp.glUser;
						rgl = tmp.glConnected;
						detDone.Store(true);
					});
				}

				void PollDetect() {
					if (!detDone.Load())
						return;
					if (detThread.Joinable())
						detThread.Join();
					jengaVersion = rjVer;
					jengaResolved = rjRes;
					gitResolved = rgRes;
					ghUser = rghU;
					ghConnected = rgh;
					glUser = rglU;
					glConnected = rgl;
					detected = true;
					detDone.Store(false);
					detBusy.Store(false);
				}

				// A appeler chaque frame : cpu instantane + detection async si besoin.
				void EnsureDetected() {
					if (cpuCount < 1) {
						// API maison (NKPlatform) : ex-std::thread::hardware_concurrency().
						cpuCount = (int32)platform::GetPlatformCapabilities().logicalProcessorCount;
						if (cpuCount < 1)
							cpuCount = 1;
					}
					PollDetect();
					if (!detected && !detBusy.Load())
						StartDetectAsync();
				}

				// ── Comptes : connexion/deconnexion GitHub (gh) & GitLab (glab) dans un terminal ──
				void GhAuth(bool connect) {
#if defined(_WIN32)
					NkString c = "start \"\" cmd /k gh auth ";
					c += connect ? "login" : "logout";
#else
					NkString c = "x-terminal-emulator -e sh -c \"gh auth ";
					c += connect ? "login" : "logout";
					c += "; read -n1\"";
#endif
					NkCodeShellRun(c.CStr());
					detected = false; // re-detecte l'etat apres l'operation
				}

				void GlAuth(bool connect) {
#if defined(_WIN32)
					NkString c = "start \"\" cmd /k glab auth ";
					c += connect ? "login" : "logout";
#else
					NkString c = "x-terminal-emulator -e sh -c \"glab auth ";
					c += connect ? "login" : "logout";
					c += "; read -n1\"";
#endif
					NkCodeShellRun(c.CStr());
					detected = false;
				}

				// Synchronisation cloud (sans backend) : miroir de settings.cfg dans ~/Documents/NKCode,
				// dossier typiquement synchronise par OneDrive/Dropbox. Reel et fonctionnel.
				static NkString CloudFile() {
					return (NkPath(NkOpenWsState::Documents().CStr()) / "NKCode" / "settings.cfg").ToString();
				}

				void MirrorToCloud() {
					if (!cloudSync)
						return;
					const NkString src = File();
					if (!NkFile::Exists(src.CStr()))
						return;
					const NkString dst = CloudFile();
					NkDirectory::CreateRecursive(NkPath(dst.CStr()).GetParent().ToString().CStr());
					NkFile::WriteAllText(NkPath(dst.CStr()), NkFile::ReadAllText(NkPath(src.CStr())));
				}
		};

		// Champ texte AVEC focus (un seul edite a la fois) — meme cablage que les autres panneaux.
		inline bool NkSetField(const NkUi &u, const NkRect &r, char *buf, int32 cap, int32 id, NkSettingsState *s,
							   float32 dt, bool blockBg, float32 leftPad) {
			const bool foc = (s->focusField == id);
			u.Panel(r, NkCol::input, foc ? NkCol::primary : NkCol::border, NkR::md * u.S);
			bool ent = false;
			if (foc)
				ent = NkOwEdit(u, r, buf, cap, s->fieldCaret, s->fieldBlink, dt, leftPad);
			else
				u.TextEllipsis(r.x + leftPad, r.y + (r.h - u.Lh()) * 0.5f, r.w - leftPad - u.s(8), buf,
							   buf[0] ? NkCol::foreground : NkCol::mutedFg);
			if (!blockBg && u.Hit(r) && u.click) {
				s->focusField = id;
				s->fieldBlink = 0.f;
				int32 n = 0;
				while (buf[n])
					++n;
				s->fieldCaret = n;
				s->fieldClaim = true;
			}
			return ent;
		}

		// ── Petits widgets locaux ─────────────────────────────────────────────────
		// Case a cocher (ligne cliquable). Renvoie true si l'etat a change.
		inline bool NkSetCheck(const NkUi &u, float32 x, float32 y, float32 w, const char *label, bool *val,
							   bool blockBg) {
			const float32 bs = u.s(18);
			const NkRect box = {x, y, bs, bs};
			const NkRect row = {x, y - u.s(4), w, bs + u.s(8)};
			const bool hv = !blockBg && u.Hit(row);
			u.Panel(box, *val ? NkCol::primary : NkCol::input, *val ? NkCol::primary : NkCol::border, NkR::sm * u.S);
			if (*val)
				NkOwIco(u, 0u, "check", {box.x + u.s(3), box.y + u.s(3), u.s(12), u.s(12)}, NkCol::primaryFg);
			u.TextV(x + bs + u.s(10), y - u.s(4), bs + u.s(8), label, hv ? NkCol::foreground : NkCol::sidebarFg);
			if (hv && u.click) {
				*val = !*val;
				return true;
			}
			return false;
		}

		// Combo (ferme). Ouvre le deroulant via l'etat partage s->combo*. Renvoie true si clic (ouvre).
		inline void NkSetCombo(const NkUi &u, const NkRect &r, const char *const *opts, int32 n, int32 *sel, int32 id,
							   NkSettingsState *s, bool blockBg) {
			const bool open = (s->comboOpen == id);
			u.Panel(r, NkCol::input, open ? NkCol::primary : NkCol::border, NkR::md * u.S);
			const int32 si = (*sel >= 0 && *sel < n) ? *sel : 0;
			u.TextEllipsis(r.x + u.s(10), r.y + (r.h - u.Lh()) * 0.5f, r.w - u.s(34), opts[si], NkCol::foreground);
			u.Icon("chevron-down", {r.x + r.w - u.s(20), r.y + (r.h - u.s(12)) * 0.5f, u.s(12), u.s(12)},
				   NkCol::mutedFg);
			if (!blockBg && u.Hit(r) && u.click) {
				if (open)
					s->comboOpen = -1;
				else {
					s->comboOpen = id;
					s->comboR = r;
					s->comboOpts = opts;
					s->comboN = n;
					s->comboSel = sel;
					s->comboJustOpened = true;
				}
			}
		}

		// Slider 0..100. Renvoie true si modifie.
		inline bool NkSetSlider(const NkUi &u, const NkRect &r, int32 *val, NkSettingsState *s, bool blockBg) {
			const float32 ty = r.y + r.h * 0.5f;
			u.dl->AddRectFilled({r.x, ty - u.s(3), r.w, u.s(6)}, NkCol::muted, u.s(3));
			float32 f = (*val) / 100.f;
			if (f < 0.f)
				f = 0.f;
			if (f > 1.f)
				f = 1.f;
			u.dl->AddRectFilled({r.x, ty - u.s(3), r.w * f, u.s(6)}, NkCol::primary, u.s(3));
			const NkRect kn = {r.x + r.w * f - u.s(7), ty - u.s(8), u.s(14), u.s(16)};
			const bool hv = !blockBg && (u.Hit(kn) || u.Hit(r));
			u.dl->AddRectFilled(kn, (s->sliderDrag || hv) ? NkColor{96, 104, 114, 255} : NkColor{70, 76, 84, 255},
								u.s(3));
			bool changed = false;
			if (!blockBg && u.click && u.Hit(r))
				s->sliderDrag = true;
			if (s->sliderDrag) {
				if (!u.down)
					s->sliderDrag = false;
				else {
					float32 t = (u.mp.x - r.x) / r.w;
					if (t < 0.f)
						t = 0.f;
					if (t > 1.f)
						t = 1.f;
					const int32 nv = (int32)(t * 100.f + 0.5f);
					if (nv != *val) {
						*val = nv;
						changed = true;
					}
				}
			}
			return changed;
		}

		// Scrollbar verticale autonome (drag statique par pointeur de scroll).
		inline void NkVScrollS(const NkUi &u, const NkRect &area, float32 contentH, float32 &scroll) {
			const float32 maxS = contentH > area.h ? contentH - area.h : 0.f;
			if (scroll < 0.f)
				scroll = 0.f;
			if (scroll > maxS)
				scroll = maxS;
			if (maxS <= 0.5f)
				return;
			const float32 sw = editorkit::NkScrollbarWidth();
			const NkRect track = {area.x + area.w - sw, area.y, sw, area.h};
			editorkit::NkVScrollbar(*u.ctx, *u.dl, track, scroll, contentH, area.h,
									(uint32)reinterpret_cast<usize>(&scroll), u.s(28)); // scrollbar standard
		}

		inline int32 NkSettingsPanel(const NkUi &u, const NkRect &r, NkSettingsState *s, NkCodeState *st,
									 NkCodeDialogs *dlg, float32 dt, const NkIcons &ic, int32 *navOut) {
			if (!s->loaded)
				s->Load();
			s->EnsureDetected(); // cpu instantane + detection (popen) sur thread de fond -> pas de gel
			NkI18nSet(s->lang);	 // langue courante -> l'UI se traduit en TEMPS REEL (aucun redemarrage)
			int32 result = 0;
			s->fieldClaim = false; // un champ posera true s'il capte le clic (sinon clic vide = defocus)
			const bool blockBg = (s->comboOpen >= 0) || NkTxtMenu().open || (dlg && dlg->pickerOpen);

			// ── En-tete ──
			const float32 hH = u.s(54);
			u.Rect({r.x, r.y, r.w, hH}, NkCol::sidebar);
			u.Rect({r.x, r.y + hH - 1.f, r.w, 1.f}, NkCol::border);
			NkOwIco(u, ic.gear, "settings", {r.x + u.s(28), r.y + (hH - u.s(18)) * 0.5f, u.s(18), u.s(18)},
					NkCol::primary);
			u.Text(r.x + u.s(54), r.y + (hH - u.Lh()) * 0.5f, NkT("settings.title"), NkCol::foreground);
			if (s->dirty) {
				const char *un = NkT("settings.unapplied");
				u.Text(r.x + r.w - u.s(28) - u.TextW(un), r.y + (hH - u.Lh()) * 0.5f, un, NkCol::accent);
			}

			// ── Sous-sidebar categories (ancres) ──
			const float32 catW = u.s(150);
			const NkRect cats = {r.x, r.y + hH, catW, r.h - hH};
			u.Rect(cats, NkCol::background);
			u.Rect({cats.x + cats.w - 1.f, cats.y, 1.f, cats.h}, NkCol::border);
			// Vrais onglets SÉPARÉS (une page par catégorie) — Toolchain retiré (géré par nav==10 hors settings).
			const char *CAT[] = {"cat.general", "cat.paths",	"cat.jenga",	"cat.theme",
								 "cat.git",		"cat.accounts", "cat.shortcuts"};
			const int32 NCAT = 7;
			{
				float32 cy = cats.y + u.s(12);
				for (int32 i = 0; i < NCAT; ++i) {
					const NkRect ir = {cats.x + u.s(8), cy, cats.w - u.s(16), u.s(32)};
					const bool act = (s->cat == i);
					const bool hv = !blockBg && u.Hit(ir);
					if (act)
						u.Rect(ir, NkCol::selection, NkR::md * u.S);
					else if (hv)
						u.Rect(ir, NkCol::hover, NkR::md * u.S);
					if (act)
						u.Rect({ir.x, ir.y + u.s(6), u.s(3), ir.h - u.s(12)}, NkCol::primary, u.s(2));
					u.TextV(ir.x + u.s(14), ir.y, ir.h, NkT(CAT[i]), act ? NkCol::foreground : NkCol::sidebarFg);
					if (hv && u.click && s->cat != i) {
						s->cat = i;
						s->scroll = 0.f;
						s->focusField = -1;
						s->comboOpen = -1;
					} // change d'onglet
					cy += u.s(36);
				}
			}
			if (s->cat >= NCAT)
				s->cat = 0;
			(void)navOut;

			// ── Zone de contenu defilable ──
			const float32 footH = u.s(56);
			const NkRect body = {r.x + catW, r.y + hH, r.w - catW, r.h - hH - footH};
			u.Rect(body, NkCol::background);
			const float32 cx = body.x + u.s(28);
			const float32 cw = body.w - u.s(28) - u.s(28) - u.s(12);
			u.dl->PushClipRect(body, true);
			float32 y = body.y + u.s(18) - s->scroll;
			auto label = [&](const char *t) {
				NkWizLabel(u, cx, y, t);
				y += u.s(26);
			};
			auto rowLabel = [&](const char *t) { u.Text(cx, y + u.s(7), t, NkCol::sidebarFg); };
			const float32 ctrlX = cx + u.s(210);
			const float32 ctrlW = cw - u.s(210);
			auto pathRow = [&](const char *lab, char *buf, int32 cap, bool browse, int32 fid) {
				rowLabel(lab);
				const float32 bw = browse ? u.s(36) : 0.f;
				const NkRect fr = {ctrlX, y, ctrlW - bw - (browse ? u.s(8) : 0.f), u.s(30)};
				NkSetField(u, fr, buf, cap, fid, s, dt, blockBg, u.s(10));
				if (browse) {
					const NkRect br = {fr.x + fr.w + u.s(8), y, u.s(36), u.s(30)};
					const bool hv = !blockBg && u.Hit(br);
					u.Rect(br, hv ? NkCol::hover : NkCol::muted, NkR::md * u.S);
					NkOwIco(u, ic.ouvrirDossier, "folder", {br.x + u.s(11), br.y + u.s(9), u.s(14), u.s(14)},
							NkCol::mutedFg);
					if (hv && u.click && dlg)
						dlg->BrowseInto(buf, cap, lab);
				}
				y += u.s(40);
				(void)fid;
			};

			// ═══ ONGLET 0 : GÉNÉRAL ═══
			if (s->cat == 0) {
				label(NkT("gen.header"));
				{
					rowLabel(NkT("gen.lang"));
					NkSetCombo(u, {ctrlX, y, u.s(200), u.s(30)}, NkI18nNames(), NK_I18N_LANGS, &s->lang, 1, s, blockBg);
					y += u.s(40);
				}
				{
					rowLabel(NkT("gen.startup"));
					static const char *L[2];
					L[0] = NkT("gen.launcher");
					L[1] = NkT("gen.lastws");
					NkSetCombo(u, {ctrlX, y, u.s(200), u.s(30)}, L, 2, &s->openStartup, 2, s, blockBg);
					y += u.s(40);
				}
				{
					rowLabel(NkT("gen.recmax"));
					NkSetField(u, {ctrlX, y, u.s(120), u.s(30)}, s->recentsMax, (int32)sizeof(s->recentsMax), 30, s, dt,
							   blockBg, u.s(10));
					y += u.s(40);
				}
				{
					rowLabel(NkT("gen.groupby"));
					static const char *L[3];
					L[0] = NkT("gen.date");
					L[1] = NkT("gen.platform");
					L[2] = NkT("gen.language");
					NkSetCombo(u, {ctrlX, y, u.s(200), u.s(30)}, L, 3, &s->groupBy, 3, s, blockBg);
					y += u.s(44);
				}
			}
			// ═══ ONGLET 1 : CHEMINS PAR DÉFAUT ═══
			else if (s->cat == 1) {
				label(NkT("set.pathshdr"));
				pathRow(NkT("set.projdir"), s->projDir, sizeof(s->projDir), true, 20);
				pathRow(NkT("set.builddir"), s->buildDir, sizeof(s->buildDir), false, 21);
				pathRow(NkT("set.cachejenga"), s->cacheDir, sizeof(s->cacheDir), true, 22);
				{
					const float32 bw1 = u.TextW(NkT("set.cacheopen")) + u.s(38);
					const NkRect b1 = {ctrlX, y, bw1, u.s(28)};
					const bool h1 = !blockBg && u.Hit(b1);
					u.Rect(b1, h1 ? NkCol::hover : NkCol::secondary, NkR::md * u.S);
					NkOwIco(u, ic.ouvrirDossier, "folder-open", {b1.x + u.s(10), b1.y + u.s(8), u.s(12), u.s(12)},
							NkCol::primary);
					u.TextV(b1.x + u.s(28), b1.y, u.s(28), NkT("set.cacheopen"), NkCol::primary);
					if (h1 && u.click)
						NkSettingsState::RevealFolder(s->cacheDir);
					const float32 bw2 = u.TextW(NkT("set.cacheclear")) + u.s(38);
					const NkRect b2 = {b1.x + bw1 + u.s(8), y, bw2, u.s(28)};
					const bool h2 = !blockBg && u.Hit(b2);
					u.Rect(b2, h2 ? NkColor{200, 70, 70, 255} : NkCol::muted, NkR::md * u.S);
					NkOwIco(u, ic.corbeille, "trash", {b2.x + u.s(10), b2.y + u.s(8), u.s(12), u.s(12)},
							h2 ? NkCol::primaryFg : NkCol::danger);
					u.TextV(b2.x + u.s(28), b2.y, u.s(28), NkT("set.cacheclear"),
							h2 ? NkCol::primaryFg : NkCol::danger);
					if (h2 && u.click)
						s->ClearCache();
					y += u.s(38);
				}
			}
			// ═══ ONGLET 2 : JENGA ═══
			else if (s->cat == 2) {
				label(NkT("set.jengahdr"));
				pathRow(NkT("set.jengapath"), s->jengaPath, sizeof(s->jengaPath), true, 23);
				{
					rowLabel(NkT("set.versiondet"));
					const bool ok = !s->jengaVersion.Empty();
					NkString v =
						ok ? (NkString("Jenga ") + s->jengaVersion)
						   : (s->detBusy.Load() ? NkString(NkT("set.detecting")) : NkString(NkT("set.notdet")));
					u.Text(ctrlX, y + u.s(6), v.CStr(), ok ? NkCol::foreground : NkCol::mutedFg);
					NkOwIco(u, ok ? ic.valideSimple : 0u, ok ? "check-circle" : "alert-triangle",
							{ctrlX + u.TextW(v.CStr()) + u.s(10), y + u.s(7), u.s(13), u.s(13)},
							ok ? NkCol::success : NkCol::accent);
					const NkRect dr = {ctrlX + ctrlW - u.s(96), y, u.s(96), u.s(28)};
					const bool hv = !blockBg && u.Hit(dr);
					u.Rect(dr, hv ? NkCol::hover : NkCol::secondary, NkR::md * u.S);
					NkOwIco(u, ic.search, "search", {dr.x + u.s(10), dr.y + u.s(8), u.s(12), u.s(12)}, NkCol::primary);
					u.TextV(dr.x + u.s(28), dr.y, u.s(28), NkT("set.detect"), NkCol::primary);
					if (hv && u.click) {
						s->detected = false;
					}
					y += u.s(40);
				}
				{
					rowLabel(NkT("set.jobs"));
					static char JOB[8][20];
					static const char *JP[8];
					static int32 jn = 0;
					if (jn == 0 || jn != (s->cpuCount < 7 ? s->cpuCount + 1 : 8)) {
						jn = s->cpuCount < 7 ? s->cpuCount + 1 : 8;
						NkStrCopy(JOB[0], 20, NkT("set.jobsauto")); // copie bornée maison (NkText.h)
						JP[0] = JOB[0];
						for (int32 k = 1; k < jn; ++k) {
							NkStrCopy(JOB[k], 20, NkPrintf("%d", k).CStr()); // NkPrintf maison
							JP[k] = JOB[k];
						}
					}
					NkSetCombo(u, {ctrlX, y, u.s(200), u.s(30)}, JP, jn, &s->jobs, 4, s, blockBg);
					y += u.s(40);
				}
				if (NkSetCheck(u, cx, y, cw, NkT("set.regtc"), &s->regTc, blockBg))
					s->dirty = true;
				y += u.s(28);
				if (NkSetCheck(u, cx, y, cw, NkT("set.inccache"), &s->incCache, blockBg))
					s->dirty = true;
				y += u.s(28);
				if (NkSetCheck(u, cx, y, cw, NkT("set.daemon"), &s->daemon, blockBg))
					s->dirty = true;
				y += u.s(32);
			}
			// ═══ ONGLET 3 : THÈME ═══
			else if (s->cat == 3) {
				label(NkT("set.themehdr"));
				{
					rowLabel(NkT("set.theme"));
					NkSetCombo(u, {ctrlX, y, u.s(200), u.s(30)}, NkThemeNames(), NK_THEME_COUNT, &s->theme, 5, s,
							   blockBg);
					y += u.s(40);
				}
				{
					rowLabel(NkT("set.accent"));
					NkColor ac = NkCol::primary; // apercu (parse hex simple)
					{
						const char *h = s->accent;
						if (h[0] == '#') {
							auto hx = [&](char c) -> int32 {
								if (c >= '0' && c <= '9')
									return c - '0';
								if (c >= 'a' && c <= 'f')
									return c - 'a' + 10;
								if (c >= 'A' && c <= 'F')
									return c - 'A' + 10;
								return 0;
							};
							if (h[1] && h[2] && h[3] && h[4] && h[5] && h[6])
								ac = NkColor{(uint8)(hx(h[1]) * 16 + hx(h[2])), (uint8)(hx(h[3]) * 16 + hx(h[4])),
											 (uint8)(hx(h[5]) * 16 + hx(h[6])), 255};
						}
					}
					u.dl->AddRectFilled({ctrlX, y + u.s(4), u.s(28), u.s(22)}, ac, NkR::sm * u.S);
					NkSetField(u, {ctrlX + u.s(36), y, u.s(120), u.s(30)}, s->accent, (int32)sizeof(s->accent), 36, s,
							   dt, blockBg, u.s(10));
					y += u.s(44);
				}
				// Apercu du theme : bandeau montrant les couleurs cles (temps reel).
				{
					rowLabel(NkT("set.preview"));
					const NkRect pv = {ctrlX, y, ctrlW, u.s(30)};
					u.Panel(pv, NkCol::background, NkCol::border, NkR::md * u.S);
					const NkColor sw[] = {NkCol::primary, NkCol::accent,  NkCol::success,
										  NkCol::danger,  NkCol::surface, NkCol::sidebar};
					for (int32 k = 0; k < 6; ++k)
						u.dl->AddRectFilled({pv.x + u.s(8) + k * u.s(30), pv.y + u.s(7), u.s(24), u.s(16)}, sw[k],
											NkR::sm * u.S);
					u.Text(pv.x + u.s(8) + 6 * u.s(30) + u.s(8), pv.y + u.s(8), "Aa", NkCol::foreground);
					y += u.s(40);
				}
			}
			// ═══ ONGLET 4 : GIT ═══
			else if (s->cat == 4) {
				label(NkT("set.githdr"));
				pathRow(NkT("set.gitpath"), s->gitPath, sizeof(s->gitPath), true, 24);
				{
					rowLabel(NkT("set.versiondet"));
					const bool ok = !s->gitResolved.Empty();
					u.Text(ctrlX, y + u.s(6), ok ? s->gitResolved.CStr() : NkT("set.gitnotfound"),
						   ok ? NkCol::foreground : NkCol::mutedFg);
					NkOwIco(u, ok ? ic.valideSimple : 0u, ok ? "check-circle" : "alert-triangle",
							{ctrlX + ctrlW - u.s(18), y + u.s(7), u.s(13), u.s(13)},
							ok ? NkCol::success : NkCol::accent);
					y += u.s(40);
				}
				if (NkSetCheck(u, cx, y, cw, NkT("set.gitind"), &s->gitIndicators, blockBg))
					s->dirty = true;
				y += u.s(28);
				if (NkSetCheck(u, cx, y, cw, NkT("set.gitfetch"), &s->gitFetch, blockBg))
					s->dirty = true;
				y += u.s(32);
			}
			// ═══ ONGLET 5 : COMPTES ═══
			else if (s->cat == 5) {
				label(NkT("set.acchdr"));
				{
					rowLabel("GitHub");
					const bool ok = s->ghConnected;
					NkString t = ok ? (NkString(NkT("set.connected")) + s->ghUser) : NkString(NkT("set.notconn"));
					u.Text(ctrlX, y + u.s(6), t.CStr(), ok ? NkCol::success : NkCol::mutedFg);
					const NkRect br = {ctrlX + ctrlW - u.s(110), y, u.s(110), u.s(28)};
					const bool hv = !blockBg && u.Hit(br);
					u.Rect(br, hv ? NkCol::hover : (ok ? NkCol::muted : NkColHover(NkCol::primary)), NkR::md * u.S);
					u.TextV(br.x + (br.w - u.TextW(ok ? NkT("set.disconnect") : NkT("set.connect"))) * 0.5f, br.y,
							u.s(28), ok ? NkT("set.disconnect") : NkT("set.connect"),
							ok ? NkCol::foreground : NkCol::primaryFg);
					if (hv && u.click) {
						s->GhAuth(!ok);
					}
					y += u.s(40);
				}
				{
					rowLabel("GitLab");
					const bool ok = s->glConnected;
					NkString t = ok ? (NkString(NkT("set.connected")) + s->glUser) : NkString(NkT("set.notconn"));
					u.Text(ctrlX, y + u.s(6), t.CStr(), ok ? NkCol::success : NkCol::mutedFg);
					const NkRect br = {ctrlX + ctrlW - u.s(110), y, u.s(110), u.s(28)};
					const bool hv = !blockBg && u.Hit(br);
					u.Rect(br, hv ? NkCol::hover : (ok ? NkCol::muted : NkColHover(NkCol::primary)), NkR::md * u.S);
					u.TextV(br.x + (br.w - u.TextW(ok ? NkT("set.disconnect") : NkT("set.connect"))) * 0.5f, br.y,
							u.s(28), ok ? NkT("set.disconnect") : NkT("set.connect"),
							ok ? NkCol::foreground : NkCol::primaryFg);
					if (hv && u.click) {
						s->GlAuth(!ok);
					}
					y += u.s(40);
				}
				if (NkSetCheck(u, cx, y, cw, NkT("set.cloudsync"), &s->cloudSync, blockBg))
					s->dirty = true;
				y += u.s(8);
				u.TextEllipsis(cx, y + u.s(4), cw, NkT("set.cloudhint"), NkCol::mutedFg);
				y += u.s(28);
			}
			// ═══ ONGLET 6 : RACCOURCIS CLAVIER & ACTIONS SOURIS ═══
			else if (s->cat == 6) {
				label(NkT("cat.shortcuts"));
				const float32 colGap = u.s(24);
				const float32 leftW = (cw - colGap) * 0.46f;
				const float32 rightX = cx + leftW + colGap, rightW = cw - leftW - colGap;
				float32 yL = y, yR = y;
				// ── Colonne gauche : raccourcis clavier ──
				u.Text(cx, yL, NkT("sc.hdr.kbd"), NkCol::mutedFg);
				yL += u.s(24);

				struct ScKey {
						const char *keys;
						const char *dk;
				};

				static const ScKey K[] = {
					{"Ctrl+N", "sc.k.newws"},
					{"Ctrl+O", "sc.k.openws"},
					{"Ctrl+Shift+O", "sc.k.opendir"},
					{"Ctrl+G", "sc.k.clone"},
					{"Ctrl+,", "sc.k.settings"},
					{"Ctrl+1", "sc.k.gohome"},
					{"Ctrl+2", "sc.k.goopen"},
					{"Ctrl+3", "sc.k.gonew"},
					{"Ctrl+4", "sc.k.goclone"},
					{"Ctrl+5", "sc.k.gotc"},
					{"Ctrl+6", "sc.k.goset"},
					{"\xE2\x86\x91 / \xE2\x86\x93", "sc.k.navlist"},
					{"Enter", "sc.k.openrec"},
					{"Delete", "sc.k.delrec"},
					{"F2", "sc.k.rename"},
					{"Ctrl+F", "sc.k.filter"},
					{"Ctrl+R", "sc.k.refresh"},
					{"Ctrl+P", "sc.k.pin"},
					{"Ctrl+Shift+C", "sc.k.copypath"},
					{"Ctrl+Q", "sc.k.quit"},
					{"F1", "sc.k.help"},
					{"Escape", "sc.k.cancel"},
				};
				const float32 chipCol = u.s(120);
				for (const ScKey &e : K) {
					const float32 kw = u.TextW(e.keys) + u.s(16);
					u.Panel({cx, yL + u.s(2), kw, u.s(22)}, NkCol::muted, NkCol::border, NkR::sm * u.S);
					u.Text(cx + u.s(8), yL + u.s(6), e.keys, NkCol::primary);
					u.TextEllipsis(cx + chipCol, yL + u.s(6), leftW - chipCol, NkT(e.dk), NkCol::foreground);
					yL += u.s(30);
				}
				// ── Colonne droite : actions souris (tableau) ──
				u.Text(rightX, yR, NkT("sc.hdr.mouse"), NkCol::mutedFg);
				yR += u.s(24);
				const float32 cG = rightX, cZ = rightX + rightW * 0.30f, cR = rightX + rightW * 0.60f;
				u.Text(cG, yR, NkT("sc.col.geste"), NkCol::mutedFg);
				u.Text(cZ, yR, NkT("sc.col.zone"), NkCol::mutedFg);
				u.Text(cR, yR, NkT("sc.col.result"), NkCol::mutedFg);
				yR += u.s(20);
				u.Rect({rightX, yR, rightW, 1.f}, NkCol::border);
				yR += u.s(8);

				struct ScM {
						const char *g;
						const char *z;
						const char *r;
				};

				static const ScM M[] = {
					{"sc.g.lclick", "sc.z.navitem", "sc.r.navto"},
					{"sc.g.lclick", "sc.z.wscard", "sc.r.openws"},
					{"sc.g.dclick", "sc.z.wscard", "sc.r.openws2"},
					{"sc.g.rclick", "sc.z.wscard", "sc.r.ctxmenu"},
					{"sc.g.star", "sc.z.wscard2", "sc.r.pin"},
					{"sc.g.dots", "sc.z.wscard2", "sc.r.ctxmenu"},
					{"sc.g.drag", "sc.z.wscard", "sc.r.reorderpin"},
					{"sc.g.drag", "sc.z.projlist", "sc.r.buildorder"},
					{"sc.g.lclick", "sc.z.wizbar", "sc.r.gotostep"},
					{"sc.g.dclick", "sc.z.titlebar", "sc.r.maxrestore"},
					{"sc.g.drag", "sc.z.titlebar", "sc.r.movewin"},
					{"sc.g.dragc", "sc.z.window", "sc.r.resize"},
					{"sc.g.wheel", "sc.z.anywhere", "sc.r.scroll"},
					{"sc.g.clickx", "sc.z.titlebar", "sc.r.closeapp"},
				};
				for (const ScM &e : M) {
					u.TextEllipsis(cG, yR, rightW * 0.29f, NkT(e.g), NkCol::foreground);
					u.TextEllipsis(cZ, yR, rightW * 0.29f, NkT(e.z), NkCol::sidebarFg);
					u.TextEllipsis(cR, yR, rightW * 0.40f, NkT(e.r), NkCol::foreground);
					yR += u.s(26);
				}
				y = (yL > yR) ? yL : yR;
			}

			const float32 contentH = (y + s->scroll) - (body.y + u.s(18));
			s->scrollMax = (contentH > body.h) ? (contentH - body.h + u.s(24)) : 0.f;
			u.dl->PopClipRect();
			// molette + clamp
			if (!blockBg && u.Hit(body) && u.ctx->input.wheel != 0.f) {
				s->scroll -= u.ctx->input.wheel * u.s(48);
				u.ctx->input.wheel = 0.f;
			}
			if (s->scroll < 0.f)
				s->scroll = 0.f;
			if (s->scroll > s->scrollMax)
				s->scroll = s->scrollMax;
			NkVScrollS(u, body, contentH, s->scroll);

			// ── Pied : Reinitialiser / Appliquer ──
			const NkRect foot = {r.x + catW, r.y + r.h - footH, r.w - catW, footH};
			u.Rect(foot, NkCol::sidebar);
			u.Rect({foot.x, foot.y, foot.w, 1.f}, NkCol::border);
			const float32 by = foot.y + (footH - u.s(34)) * 0.5f;
			const char *applyT = NkT("btn.apply");
			const char *resetT = NkT("btn.resetall");
			{
				const float32 aw = u.TextW(applyT) + u.s(52);
				const NkRect ar = {foot.x + foot.w - u.s(20) - aw, by, aw, u.s(34)};
				const bool hv = !blockBg && u.Hit(ar);
				u.Rect(ar, hv ? NkColHover(NkCol::primary) : NkCol::primary, NkR::md * u.S);
				NkOwIco(u, ic.valideSimple, "check", {ar.x + u.s(16), ar.y + u.s(10), u.s(14), u.s(14)},
						NkCol::primaryFg);
				u.TextV(ar.x + u.s(36), ar.y, u.s(34), applyT, NkCol::primaryFg);
				if (hv && u.click)
					s->Save();
			}
			{
				const float32 rw = u.TextW(resetT) + u.s(28);
				const NkRect rr = {foot.x + foot.w - u.s(20) - (u.TextW(applyT) + u.s(52)) - u.s(10) - rw, by, rw,
								   u.s(34)};
				if (u.Button(rr, resetT, NkCol::muted, NkCol::hover, NkCol::foreground, NkR::md * u.S))
					s->Reset();
			}

			// ── Deroulant du combo (rendu PAR-DESSUS) ──
			if (s->comboOpen >= 0 && s->comboOpts && s->comboSel) {
				const float32 ih = u.s(28);
				float32 ddw = s->comboR.w;
				for (int32 k = 0; k < s->comboN; ++k) {
					const float32 tw = u.TextW(s->comboOpts[k]) + u.s(30);
					if (tw > ddw)
						ddw = tw;
				}
				float32 ddx = s->comboR.x;
				if (ddx + ddw > r.x + r.w - u.s(8))
					ddx = r.x + r.w - u.s(8) - ddw;
				float32 ddy = s->comboR.y + s->comboR.h + u.s(2);
				const float32 ddh = s->comboN * ih + u.s(6);
				if (ddy + ddh > r.y + r.h - u.s(8))
					ddy = s->comboR.y - ddh - u.s(2);
				const NkRect dd = {ddx, ddy, ddw, ddh};
				u.dl->AddRectFilled({dd.x + u.s(2), dd.y + u.s(3), dd.w, dd.h}, NkColor{0, 0, 0, 90}, NkR::md * u.S);
				u.Panel(dd, NkCol::surface, NkCol::primary, NkR::md * u.S);
				bool chose = false;
				for (int32 k = 0; k < s->comboN; ++k) {
					const NkRect ir = {dd.x + u.s(4), dd.y + u.s(3) + k * ih, dd.w - u.s(8), ih};
					const bool hv = u.Hit(ir);
					if (hv || k == *s->comboSel)
						u.Rect(ir, NkCol::hover, NkR::sm * u.S);
					u.TextEllipsis(ir.x + u.s(8), ir.y + (ih - u.Lh()) * 0.5f, dd.w - u.s(20), s->comboOpts[k],
								   NkCol::foreground);
					if (hv && u.click) {
						*s->comboSel = k;
						chose = true;
						s->dirty = true;
					}
				}
				if (chose || (u.click && !u.Hit(dd) && !s->comboJustOpened))
					s->comboOpen = -1;
				s->comboJustOpened = false;
			}

			// Clic dans le vide (hors champ, hors combo) -> le champ actif perd le focus.
			if (u.click && !blockBg && !s->fieldClaim && u.Hit(r))
				s->focusField = -1;
			if (u.ctx->input.KeyPressed(NkGuiKey::Escape)) {
				if (s->comboOpen >= 0)
					s->comboOpen = -1;
				else if (s->focusField >= 0)
					s->focusField = -1;
				else
					result = 1;
			}
			return result;
		}

	} // namespace nkcode
} // namespace nkentseu
