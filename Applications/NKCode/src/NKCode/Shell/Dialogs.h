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
#include "NKEditorKit/NkFilePicker.h" // NkFilePickerState : coeur reutilisable du picker
#include "NKCode/Project/NkCodeState.h"
#include "NKCode/Project/NkCodeGen.h"
#include "NKCode/Shell/NkLoading.h" // ecran de chargement (section 14)
#include "NKWindow/Core/NkDialogs.h"
#include "NKContainers/String/NkFormat.h" // NkPrintf (formatage maison)
#include "NKPlatform/NkEnv.h"			  // env::GetEnvVar (variables d'environnement maison)

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu;
		using namespace nkentseu::editorkit;
		using namespace nkentseu::nkgui;

		struct NkHomeState; // forward (NkHome.h) — parametres launcher pour la modale Preferences

		// Etat des dialogues modaux (un seul a la fois). 0 = aucun.
		struct NkCodeDialogs : public NkFilePickerState {
				NkCodeState *st = nullptr;
				NkEditorShell *shell = nullptr; // pour appliquer l'etat d'UI du projet (ui.cfg)
				NkHomeState *home = nullptr;	// pose par main.cpp — settings/icones du launcher
				bool showPrefs = false;			// fenetre modale PREFERENCES (panneau launcher complet)
				bool showNewWs = false;			// fenetre modale NOUVEAU WORKSPACE (wizard launcher complet)
				bool wsAddAsRoot = false;		// intercepte le DoLoad du wizard -> ajout comme racine explorateur
				int32 showHelp = 0;				// fenetre dediee AIDE : 0 aucune, 1 raccourcis, 2 a propos
				float32 helpScroll = 0.f;		// defilement de la fenetre d'aide
				NkLoadingState loading;			// ecran de chargement (workspace -> editeur)

				enum Mode { None = 0, NewProject, NewWorkspace, SaveAs, Properties };

				int32 mode = None;
				bool showStart = true; // ecran de demarrage plein cadre (remplace l'editeur)
				char nameBuf[256] = {};
				int32 kindIdx = 0;
				int32 langIdx = 0;
				int32 projDialect = 0;		 // index NkDialects (NewProject)
				int32 projFocus = 0;		 // champ focus du dialogue NewProject
				int32 projWarn = 0;			 // 0=Defaut,1=Off,2=High,3=Extra,4=Everything
				float32 projScroll = 0.f;	 // defilement du formulaire projet
				// Workspace d'APPARTENANCE du nouveau projet : tout projet depend d'un
				// workspace. Seuls ceux OUVERTS dans CET editeur sont proposes
				// (st->wsPaths, scannes dans la racine chargee). -1 = courant (wsIdx).
				int32 projWsIdx = -1;
				float32 projContentH = 0.f; // hauteur mesuree du formulaire (frame precedente)
				char projFiles[160] = {};	 // motif fichiers (defaut src/**.<ext>)
				char projDefines[256] = {};	 // defines projet (csv)
				char projInc[256] = {};		 // includedirs (csv)
				char projLib[256] = {};		 // libdirs (csv)
				char projLinks[256] = {};	 // links (csv)
				char projDeps[256] = {};	 // dependson (csv)
				char projVersion[32] = {};	 // appversion (apps)
				char projPublisher[64] = {}; // apppublisher (apps)
				char projIcon[512] = {};	 // appicon (apps)
				bool justOpened = false;
				NkString status;		  // message d'erreur/succes affiche dans le dialogue
				int32 saveProjIdx = -1;	  // SaveAs : projet (dossier) de destination choisi
				float32 saveScroll = 0.f; // SaveAs : defilement de la liste de projets

				// ── Launcher : onglets (0 Recent, 1 Nouveau, 2 Charger) ──
				int32 launcherTab = 0;
				int32 launcherFocus = 0;							 // champ de saisie focus (onglet Nouveau)
				float32 newScrollY = 0.f;							 // defilement du formulaire Nouveau
				char wsName[128] = {};								 // Nouveau : nom du workspace
				char wsDir[512] = {};								 // Nouveau : repertoire cible
				bool wsCfg[4] = {true, true, false, false};			 // Debug, Release, Profile, Shipping
				bool wsPlat[8] = {};								 // plateformes/OS (index Systems())
				bool wsArch[5] = {true, false, false, false, false}; // x86_64, x86, arm64, arm, wasm32
				bool wsMakeProj = false;							 // creer un projet de demarrage
				char wsProjName[128] = {};							 // projet de demarrage : nom
				int32 wsProjKind = 0;								 // genre (index NkKinds)
				int32 wsProjLang = 0;								 // langage (index NkLangs)
				bool wsDutc = false;								 // disable unittest compilation
				bool wsDute = false;								 // disable unittest execution
				bool wsEnvFilled = false;							 // chemins SDK pre-remplis depuis l'env ?
				char androidSdk[512] = {}, androidNdk[512] = {}, javaJdk[512] = {};
				char harmonySdk[512] = {}, gdkPath[512] = {};
				char loadDir[512] = {}; // Charger : dossier choisi
				bool loadScanned = false;
				NkVector<NkString> foundPaths, foundNames; // Charger : workspaces trouves

				// ── Gestion des toolchains (interface dediee) ──
				bool tcOpen = false;
				float32 tcScroll = 0.f;
				int32 tcFocus = -1; // champ SDK en edition
				// Editeur d'un toolchain (ajout/modif -> jenga config toolchain add)
				bool tcEdit = false;
				int32 tcEditFocus = 0;
				char teName[64] = {}, teType[24] = {}, teOs[24] = {}, teArch[24] = {}, teEnv[24] = {};
				char teCc[512] = {}, teCxx[512] = {}, teAr[512] = {}, teTriple[128] = {}, teSysroot[512] = {};

				void TcEditNew() {
					tcEdit = true;
					tcEditFocus = 0;
					teName[0] = teType[0] = teOs[0] = teArch[0] = teEnv[0] = teCc[0] = teCxx[0] = teAr[0] =
						teTriple[0] = teSysroot[0] = '\0';
				}

				void TcEditFrom(const NkCodeState::ToolchainRow &r) {
					TcEditNew();
					CopyTo(teName, r.name.CStr(), 64);
					CopyTo(teType, r.family.CStr(), 24);
					CopyTo(teOs, r.os.CStr(), 24);
					CopyTo(teArch, r.arch.CStr(), 24);
					CopyTo(teEnv, r.env.CStr(), 24);
				}

				void TcEditSave() {
					if (!st || !teName[0])
						return;
					// JSON au format `jenga config toolchain add`
					NkString j;
					j += "{\n";
					j += "  \"type\": \"";
					j += teType[0] ? teType : "clang";
					j += "\",\n";
					j += "  \"target\": { \"os\": \"";
					j += teOs;
					j += "\", \"arch\": \"";
					j += teArch;
					j += "\", \"env\": \"";
					j += teEnv;
					j += "\" },\n";
					if (teCc[0]) {
						j += "  \"cc\": \"";
						j += JsonEsc(teCc);
						j += "\",\n";
					}
					if (teCxx[0]) {
						j += "  \"cxx\": \"";
						j += JsonEsc(teCxx);
						j += "\",\n";
					}
					if (teAr[0]) {
						j += "  \"ar\": \"";
						j += JsonEsc(teAr);
						j += "\",\n";
					}
					if (teTriple[0]) {
						j += "  \"target_triple\": \"";
						j += teTriple;
						j += "\",\n";
					}
					if (teSysroot[0]) {
						j += "  \"sysroot\": \"";
						j += JsonEsc(teSysroot);
						j += "\",\n";
					}
					j += "  \"cflags\": [], \"cxxflags\": [], \"ldflags\": []\n}\n";
					if (st->ToolchainAdd(teName, j))
						tcEdit = false;
				}

				static NkString JsonEsc(const char *s) { // echappe les backslash pour JSON
					NkString o;
					for (; s && *s; ++s) {
						if (*s == '\\')
							o += "\\\\";
						else
							o += *s;
					}
					return o;
				}

				// ── Assistant de création (choix du TYPE) : 0 Classe, 1 Struct, 2 Union (=> .h+.cpp),
				//    3 Enum (=> .h), 4 Python(.py), 5 Markdown(.md), 6 NKSL(.nksl), 7 Jenga(.jenga),
				//    8 Texte/Autre (extension demandée). ──
				int32 scafKind = 0;
				char scafName[128] = {};		// nom (type ou nom de fichier)
				char scafNs[128] = "nkentseu";	// namespace, sous-namespaces via `::`
				char scafBase[192] = {};		// classes mères (csv)
				char scafChildren[256] = {};	// classes filles (csv)
				char scafExt[24] = {};			// extension (mode Texte/Autre)
				int32 scafFocus = 0;			// 0 aucun, 1 nom, 2 ns, 3 base, 4 filles, 5 ext
				// Purpose APPLICATIF : « creer un dossier » (menu Fichier > Nouveau Dossier).
				// Le picker s'ouvre en mode dossier generique : navigation + bouton
				// « + Creer dossier » integres ; le bouton principal cree <emplacement>/<nom>.
				static constexpr int32 PK_NewFolder = 200;
				// Purpose APPLICATIF : « exporter le workspace » — choisir le dossier de
				// DESTINATION du zip ; la confirmation lance tar dans le terminal integre.
				static constexpr int32 PK_ExportZip = 201;
				// Purpose APPLICATIF : « cloner un exemple du launcher » — choisir le
				// dossier PARENT de destination ; la confirmation lance
				// `jenga examples copy <id> <destination>` (async) puis ouvre le clone.
				static constexpr int32 PK_ExampleCopy = 202;
				NkString exCopyId;			  // id de l'exemple choisi (ex. "01_hello_console")
				NkProcess exCopyProc;		  // copie async (jenga examples copy) — repli
				bool exCopyBusy = false;
				bool exCopyEmbedded = false; // copie via l'interpreteur embarque
				NkString exCopyDestFull;	  // <destination>/<id> — ouvert automatiquement une fois copie

				void OpenPicker(int32 purpose, const char *startDir, char *buf = nullptr, int32 cap = 0,
								const char *confine = nullptr, const char *fileExt = nullptr) {
					// Coeur generique dans NKEditorKit (NkFilePickerState) ; ici on ne fixe
					// que le dossier de depart par defaut = racine du workspace NKCode.
					pickWsJenga = false; // discriminateur re-arme par OpenWorkspaceDialog seul
					const char *start = (startDir && *startDir) ? startDir : (st ? st->root.ToString().CStr() : ".");
					OpenPickerBase(purpose, start, buf, cap, confine, fileExt);
				}

				// ── Specialisation du picker moteur (NkFilePickerState) : scaffolding + actions ──
				// « Nouveau fichier » = enregistrer un onglet actif VIDE -> assistant de creation.
				bool PickerIsNewFile() const {
					return pickerFor == PK_SaveFile && st && st->HasActive() &&
						   st->files[st->active].doc.GetText().Empty();
				}
				float32 PickerWindowHeight(float32 S) const override {
					return (PickerIsNewFile() ? 700.f : (pickerFor == PK_SaveFile ? 548.f : 500.f)) * S;
				}
				float32 PickerBottomReserve(float32 S) const override {
					return (PickerIsNewFile() ? 340.f : (pickerFor == PK_SaveFile ? 140.f : 96.f)) * S;
				}
				float32 PickerExtraHeight(float32 S) const override { return PickerIsNewFile() ? 300.f * S : 0.f; }
				bool PickerConfirmEnabled() const override {
					if (PickerIsNewFile())
						return scafName[0] != '\0' && (scafKind != 8 || scafExt[0] != '\0');
					if (pickerFor == PK_NewFolder) // bouton principal = CREER (nom requis)
						return pickerNew[0] != '\0';
					return NkFilePickerState::PickerConfirmEnabled();
				}
				const char *PickerConfirmLabel() const override {
					if (PickerIsNewFile())
						return "Creer";
					if (pickerFor == PK_NewFolder)
						return "Creer le dossier";
					if (pickerFor == PK_ExportZip)
						return "Exporter ici";
					if (pickerFor == PK_ExampleCopy)
						return "Cloner ici";
					return NkFilePickerState::PickerConfirmLabel();
				}
				void PickerClearExtraFocus() override { scafFocus = 0; }
				const char *PickerTitle() const override {
					if (pickerFor == PK_NewFolder)
						return "Creer un dossier - choisir l'emplacement";
					if (pickerFor == PK_ExportZip)
						return "Exporter le workspace - choisir la destination";
					if (pickerFor == PK_ExampleCopy)
						return "Cloner l'exemple - choisir l'emplacement";
					return NkFilePickerState::PickerTitle();
				}

				// Assistant de CREATION (scaffolding C++) dessine dans la region app du picker moteur.
				void DrawPickerExtra(NkGuiContext &ctx, NkGuiDrawList &dl, const NkGuiFont *f, const NkRect &region,
									 const NkFilePickerStyle &sty, bool click, bool &fieldClicked) override {
					const float32 S = ctx.S(1.f), lh = f->LineHeight(), asc = f->Ascent();
					const NkVec2 mp = ctx.input.mousePos;
					auto hit = [&](const NkRect &r) { return NkGuiRectContains(r, mp); };
					auto text = [&](float32 x, float32 yy, const char *s, const NkColor &c) {
						dl.AddText(f->Face(), f->TexId(), {x, yy + asc}, s, c);
					};
					const float32 cx = region.x, cwid = region.w, ny = region.y;
					auto field = [&](float32 fx, float32 fyy, float32 fw, char *buf, int32 cap, int32 fid,
									 const char *ph2) {
						const NkRect r = {fx, fyy, fw, 26.f * S};
						// Focus AVANT le dessin (place le curseur au clic, pas de faux double-clic).
						if (hit(r) && click) {
							scafFocus = fid;
							pickerSaveFocus = pickerNewFocus = pickerEditing = false;
							fieldClicked = true;
						}
						NkOverlayTextField(ctx, dl, f, r, buf, cap, scafFocus == fid);
						if (buf[0] == '\0' && scafFocus != fid && ph2)
							text(r.x + 10.f * S, r.y + (26.f * S - lh) * 0.5f, ph2, sty.sub);
					};
					const float32 halfW = (cwid - 10.f * S) * 0.5f;
					text(cx, ny + 40.f * S, "Type", sty.sub);
					const char *kinds[] = {"Classe",   "Struct", "Union", "Enum",		"Python",
										   "Markdown", "NKSL",	 "Jenga", "Texte/Autre"};
					float32 kx = cx, kyy = ny + 58.f * S;
					for (int32 k = 0; k < 9; ++k) {
						const float32 bw = f->MeasureWidth(kinds[k]) + 16.f * S;
						if (kx + bw > cx + cwid) {
							kx = cx;
							kyy += 28.f * S;
						}
						const NkRect r = {kx, kyy, bw, 24.f * S};
						const bool sel = (scafKind == k);
						dl.AddRectFilled(r, sel ? sty.accent : (hit(r) ? sty.rowHover : NkColor{24, 28, 34, 255}), 5.f * S);
						text(r.x + 8.f * S, r.y + (24.f * S - lh) * 0.5f, kinds[k], sel ? sty.textStrong : sty.text);
						if (hit(r) && click)
							scafKind = k;
						kx += bw + 6.f * S;
					}
					const float32 fy = kyy + 46.f * S;
					text(cx, fy - 18.f * S, "Nom", sty.sub);
					field(cx, fy, cwid, scafName, (int32)sizeof(scafName), 1, "ex: NkFoo");
					const int32 k = scafKind;
					const bool isCpp = (k <= 3), hasBase = (k == 0 || k == 1), isTexte = (k == 8);
					const float32 fy2 = fy + 46.f * S;
					if (isCpp) {
						text(cx, fy2 - 18.f * S, "Namespace ( :: pour sous-namespaces )", sty.sub);
						field(cx, fy2, hasBase ? halfW : cwid, scafNs, (int32)sizeof(scafNs), 2, "nkentseu::code");
						if (hasBase) {
							text(cx + halfW + 10.f * S, fy2 - 18.f * S, "Classes meres (csv)", sty.sub);
							field(cx + halfW + 10.f * S, fy2, halfW, scafBase, (int32)sizeof(scafBase), 3,
								  "NkBaseA, NkBaseB");
						}
					} else if (isTexte) {
						text(cx, fy2 - 18.f * S, "Extension", sty.sub);
						field(cx, fy2, halfW, scafExt, (int32)sizeof(scafExt), 5, ".txt");
					}
				}

				// Route le RESULTAT du picker moteur vers les actions NKCode (hors enregistrement).
				void RoutePickerResult() {
					const int32 purpose = pickerResultFor;
					NkString chosen(pickerResultPath);
					if (purpose == PK_Open)
						DoLoad(NkPath(chosen.CStr()));
					else if (purpose == PK_NewDir)
						CopyTo(wsDir, chosen.CStr(), (int32)sizeof(wsDir));
					else if (purpose == PK_LoadDir) {
						CopyTo(loadDir, chosen.CStr(), (int32)sizeof(loadDir));
						ScanLoad();
					} else if (purpose == PK_PickFolder && st)
						st->pickedFolder = chosen; // dossier QUELCONQUE -> l'explorateur le récupère
					else if (purpose == PK_NewFolder && pickerNew[0]) {
						// Bouton principal « Creer le dossier » : cree <emplacement>/<nom>.
						const NkPath np = NkPath(chosen.CStr()) / pickerNew;
						if (NkDirectory::CreateRecursive(np) && st)
							st->status = NkString("Dossier cree : ") + np.ToString().CStr();
						pickerNew[0] = '\0';
					} else if (purpose == PK_File && pickWsJenga) {
						// « Ouvrir un workspace » : .jenga choisi -> charge son dossier.
						pickWsJenga = false;
						if (wsOpenBuf[0])
							DoLoad(NkPath(wsOpenBuf).GetParent());
					} else if (purpose == PK_ExportZip && st) {
						// « Exporter » : zip du workspace VERS le dossier choisi, via le
						// terminal integre (commande visible). Nom = <workspace>-export.zip.
						NkString nm = (st->wsIdx >= 0 && st->wsIdx < (int32)st->wsNames.Size() &&
									   !st->wsNames[st->wsIdx].Empty())
										  ? st->wsNames[st->wsIdx]
										  : NkString("workspace");
						NkString out = chosen;
						out += "/";
						out += nm;
						out += "-export.zip";
						NkString cmd = "tar -a -c --exclude=Build --exclude=.git -f \"";
						cmd += out;
						cmd += "\" . && echo Exporte: ";
						cmd += out;
						st->termOpenCmd = cmd;
						st->termOpenKind = -1;
						st->termOpenAt = st->root.ToString();
						if (shell)
							shell->FocusPanel("TERMINAL");
					} else if (purpose == PK_ExampleCopy && !exCopyId.Empty() && !exCopyBusy) {
						// « Cloner un exemple » (launcher) : copie ASYNC via
						// `jenga examples copy <id> <destination>` ; a la fin (PollExampleCopy),
						// le clone est ouvert comme workspace.
						exCopyDestFull = chosen;
						exCopyDestFull += "/";
						exCopyDestFull += exCopyId;
						// Jenga EMBARQUE en priorite : sans ca, cloner un exemple echouait
						// sur une machine sans Python (cette copie etait le dernier appel
						// `jenga` en sous-processus du launcher).
						exCopyEmbedded = false;
						if (st && NkCodeState::UseEmbeddedJenga() && !NkEmbeddedJenga::Get().Running()) {
							NkEmbeddedJenga::Request req;
							req.kind = "cli";
							req.args.PushBack(NkString("examples"));
							req.args.PushBack(NkString("copy"));
							req.args.PushBack(exCopyId);
							req.args.PushBack(chosen);
							if (NkEmbeddedJenga::Get().Start(req)) {
								exCopyEmbedded = true;
								exCopyBusy = true;
								st->status = NkString("Clonage de l'exemple ") + exCopyId.CStr() + "...";
								return;
							}
						}
						NkString cmd("jenga examples copy ");
						cmd += exCopyId;
						cmd += " \"";
						cmd += chosen;
						cmd += "\"";
						if (exCopyProc.Start(cmd)) {
							exCopyBusy = true;
							if (st)
								st->status = NkString("Clonage de l'exemple ") + exCopyId.CStr() + "...";
						} else if (st)
							st->status = NkString("(clonage deja en cours)");
					}
					// PK_Buf / PK_File : le buffer cible est deja rempli par le moteur.
				}

				// A appeler chaque frame (launcher ET editeur) : draine la copie d'exemple
				// en cours ; quand elle finit, ouvre le clone comme workspace.
				void PollExampleCopy() {
					if (!exCopyBusy)
						return;
					NkVector<NkString> sink;
					int32 exit = 0;
					if (exCopyEmbedded) {
						NkVector<NkJengaProgressEvent> ignored;
						NkEmbeddedJenga::Get().Drain(sink, ignored);
						if (!NkEmbeddedJenga::Get().Done())
							return;
						exit = NkEmbeddedJenga::Get().ExitCode();
						exCopyEmbedded = false;
					} else {
						exCopyProc.Drain(sink); // sortie ignoree (statut via exit code)
						if (!exCopyProc.Done())
							return;
						exit = exCopyProc.ExitCode();
					}
					exCopyBusy = false;
					const NkString id = exCopyId;
					exCopyId.Clear();
					if (exit == 0 && NkDirectory::Exists(exCopyDestFull.CStr())) {
						if (st)
							st->status = NkString("Exemple clone : ") + exCopyDestFull.CStr();
						DoLoad(NkPath(exCopyDestFull.CStr())); // ouvre le clone (ecran de chargement)
					} else if (st)
						st->status = NkString("Echec du clonage de ") + id.CStr() +
									 " (verifier `jenga examples copy` dans un terminal)";
				}

				// ── Génération de squelette (scaffolding) selon l'extension ──────────────────
				static bool ScafIeq(const char *a, const char *b) {
					while (*a && *b) {
						char x = *a, y = *b;
						if (x >= 'A' && x <= 'Z')
							x += 32;
						if (y >= 'A' && y <= 'Z')
							y += 32;
						if (x != y)
							return false;
						++a;
						++b;
					}
					return *a == *b;
				}

				static bool ScafIsHeader(const char *e) {
					return ScafIeq(e, ".h") || ScafIeq(e, ".hpp") || ScafIeq(e, ".hh") || ScafIeq(e, ".hxx") ||
						   ScafIeq(e, ".inl");
				}

				static bool ScafIsSource(const char *e) {
					return ScafIeq(e, ".cpp") || ScafIeq(e, ".cc") || ScafIeq(e, ".cxx") || ScafIeq(e, ".c");
				}

				static bool IsCodeExt(const char *e) {
					return ScafIsHeader(e) || ScafIsSource(e);
				}

				static NkString ScafUpper(const NkString &s) {
					NkString o;
					for (const char *p = s.CStr(); *p; ++p) {
						char c = *p;
						if (c >= 'a' && c <= 'z')
							c -= 32;
						if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
							c = '_';
						o += c;
					}
					return o;
				}

				static NkString ScafIndent(const NkString &b, const char *ind) {
					NkString o;
					bool sol = true;
					for (const char *p = b.CStr(); *p; ++p) {
						if (sol && *p != '\n') {
							o += ind;
							sol = false;
						}
						o += *p;
						if (*p == '\n')
							sol = true;
					}
					return o;
				}

				static NkString ScafIndentN(const NkString &b, int32 levels) {
					NkString ind;
					for (int32 i = 0; i < levels; ++i)
						ind += "    ";
					return levels > 0 ? ScafIndent(b, ind.CStr()) : b;
				}

				static void ScafSplitNs(const char *ns, NkVector<NkString> &out) { // "a::b::c" -> [a,b,c]
					NkString cur;
					for (const char *p = ns ? ns : "";; ++p) {
						if (*p == '\0') {
							if (!cur.Empty())
								out.PushBack(cur);
							break;
						}
						if (*p == ':' && p[1] == ':') {
							if (!cur.Empty()) {
								out.PushBack(cur);
								cur = NkString();
							}
							++p;
						} else if (*p != ' ' && *p != '\t')
							cur += *p;
					}
				}

				static NkString ScafBases(const char *base) { // "A,B" -> " : public A, public B"
					NkString o(" : "), cur;
					bool first = true;
					auto emit = [&]() {
						if (cur.Empty())
							return;
						if (!first)
							o += ", ";
						o += "public ";
						o += cur;
						cur = NkString();
						first = false;
					};
					for (const char *p = base ? base : "";; ++p) {
						if (*p == ',' || *p == '\0') {
							emit();
							if (*p == '\0')
								break;
						} else if (*p != ' ' && *p != '\t')
							cur += *p;
					}
					return first ? NkString() : o;
				}

				// kind : 0 Classe, 1 Struct, 2 Union, 3 Enum. base = "A,B" -> heritage multiple.
				static NkString ScafElement(const NkString &name, int32 kind, const char *base) {
					NkString o;
					if (kind == 0) {
						o += "class ";
						o += name;
						o += ScafBases(base);
						o += " {\n    public:\n        ";
						o += name;
						o += "();\n        ~";
						o += name;
						o += "();\n\n    private:\n};\n";
					} else if (kind == 1) {
						o += "struct ";
						o += name;
						o += ScafBases(base);
						o += " {\n    public:\n        ";
						o += name;
						o += "();\n        ~";
						o += name;
						o += "();\n};\n";
					} else if (kind == 2) {
						o += "union ";
						o += name;
						o += " {\n};\n";
					} else if (kind == 3) {
						o += "enum class ";
						o += name;
						o += " {\n    Value1,\n    Value2,\n};\n";
					}
					return o;
				}

				static NkString ScafSourceImpl(const NkString &name, int32 kind) {
					NkString o;
					// kind <= 1 : classe (0) et structure (1) UNIQUEMENT.
					//
					// La condition etait « kind <= 2 », donc l'UNION (2) en
					// heritait : on ecrivait Name::Name() et Name::~Name() dans le
					// .cpp alors que l'en-tete d'une union n'en DECLARE aucun (cf.
					// ScafElement, dont le cas 2 rend « union Name {}; » nu). Le
					// code genere ne compilait donc pas, et l'utilisateur devait
					// corriger a la main a chaque union creee.
					if (kind <= 1) {
						o += name;
						o += "::";
						o += name;
						o += "() {\n}\n\n";
						o += name;
						o += "::~";
						o += name;
						o += "() {\n}\n";
					} else
						o += "// TODO\n";
					return o;
				}

				// Génère un .h (header=true) ou .cpp (header=false) avec sous-namespaces imbriqués.
				static NkString GenCode(const NkString &stem, bool header, int32 kind, const char *ns,
										const char *base) {
					NkVector<NkString> nsp;
					ScafSplitNs(ns, nsp);
					const int32 depth = static_cast<int32>(nsp.Size());
					const NkString body =
						ScafIndentN(header ? ScafElement(stem, kind, base) : ScafSourceImpl(stem, kind), depth);
					NkString o, guard;
					if (header) {
						guard = NkString("__NKENTSEU_") + ScafUpper(stem).CStr() + "_H__";
						o += "#pragma once\n#ifndef ";
						o += guard;
						o += "\n#define ";
						o += guard;
						o += "\n\n";
					} else {
						o += "#include \"";
						o += stem;
						o += ".h\"\n\n";
					}
					for (int32 i = 0; i < depth; ++i) {
						for (int32 j = 0; j < i; ++j)
							o += "    ";
						o += "namespace ";
						o += nsp[i].CStr();
						o += " {\n";
					}
					if (depth > 0)
						o += "\n";
					o += body;
					if (depth > 0)
						o += "\n";
					for (int32 i = depth - 1; i >= 0; --i) {
						for (int32 j = 0; j < i; ++j)
							o += "    ";
						o += "}\n";
					}
					if (header) {
						o += "\n#endif // ";
						o += guard;
						o += "\n";
					}
					return o;
				}

				// Crée le(s) fichier(s) selon le TYPE choisi, dans le dossier courant du picker.
				void DoScaffoldCreate() {
					if (!st || !st->HasActive() || !scafName[0])
						return;
					const char *base = PickerAllowed(pickerPath) ? pickerPath : pickerConfine.CStr();
					const NkString name = scafName;
					auto &f = st->files[st->active];
					const int32 k = scafKind;
					if (k <= 3) { // C++ : header (+ source pour classe/struct/union)
						f.doc.SetText(GenCode(name, true, k, scafNs, scafBase).CStr());
						st->SaveActiveAs(NkPath(base) / (name + ".h").CStr());
						if (k <= 2) {
							NkPath cpp = NkPath(base) / (name + ".cpp").CStr();
							NkFile::WriteAllText(cpp, GenCode(name, false, k, scafNs, scafBase));
							st->OpenPath(cpp);
						}
					} else {
						NkString ext, content;
						if (k == 4) {
							ext = ".py";
							content = NkString("\"\"\"") + name.CStr() + "\"\"\"\n\n";
						} else if (k == 5) {
							ext = ".md";
							content = NkString("# ") + name.CStr() + "\n";
						} else if (k == 6) {
							ext = ".nksl";
							content = NkString("// ") + name.CStr() + ".nksl\n";
						} else if (k == 7) {
							ext = ".jenga";
							content = NkString("# ") + name.CStr() + ".jenga\n";
						} else {
							NkString e = scafExt;
							if (!e.Empty() && e.CStr()[0] != '.')
								e = NkString(".") + e.CStr();
							ext = e.Empty() ? NkString(".txt") : e;
						}
						f.doc.SetText(content.CStr());
						st->SaveActiveAs(NkPath(base) / (name + ext.CStr()).CStr());
					}
					PickerCancel();
				}

				// Mode ENREGISTRER (buffer NON vide) : écrit l'onglet actif dans <dossier>/<nom saisi>.
				void DoSaveHere() {
					if (st && st->HasActive() && pickerSaveName[0]) {
						const char *base = PickerAllowed(pickerPath) ? pickerPath : pickerConfine.CStr();
						st->SaveActiveAs(NkPath(base) / pickerSaveName);
					}
					PickerCancel();
				}

				// Nettoyage APRES annulation d'un enregistrement (le moteur a deja ferme le
				// picker) : referme un onglet « + » resté vierge.
				void CancelSaveCleanup() {
					if (st && st->HasActive()) {
						auto &f = st->files[st->active];
						if (f.untitled && f.doc.GetText().Empty())
							st->CloseFile(st->active);
					}
				}

				// Parcourir pour choisir un FICHIER (executable de toolchain).
				void BrowseFile(char *dst, int32 cap, const char * /*title*/) {
					NkString start = dst[0] ? NkPath(dst).GetParent().ToString() : NkString();
					OpenPicker(PK_File, start.Empty() ? nullptr : start.CStr(), dst, cap);
				}

				void Open(int32 m) {
					mode = m;
					justOpened = true;
					status.Clear();
					if (m == NewProject || m == NewWorkspace)
						nameBuf[0] = '\0';
					if (m == NewProject)
						projWsIdx = -1; // defaut = workspace courant
					if (m == NewWorkspace)
						projFocus = 0; // focus initial = champ Nom
				}

				void ShowStart() {
					showStart = true;
					mode = None;
				}

				// Onglet Nouveau : parcourir le repertoire cible (picker NKGui).
				void BrowseNewDir() {
					OpenPicker(PK_NewDir, wsDir);
				}

				// Onglet Charger : parcourir un dossier (picker NKGui) puis scanner.
				void BrowseLoadDir() {
					OpenPicker(PK_LoadDir, loadDir[0] ? loadDir : (st ? st->root.ToString().CStr() : "."));
				}

				void ScanLoad() {
					loadScanned = true;
					if (loadDir[0])
						NkCodeState::ScanWorkspacesIn(NkPath(loadDir), foundPaths, foundNames);
					else {
						foundPaths.Clear();
						foundNames.Clear();
					}
				}

				// Parcourir pour un chemin SDK (Nouveau) via le picker NKGui.
				void BrowseInto(char *dst, int32 cap, const char * /*title*/) {
					OpenPicker(PK_Buf, dst, dst, cap);
				}

				// Onglet Nouveau : genere le workspace (toutes proprietes) puis le charge.
				void CreateNew() {
					int32 nSys = 0;
					const NkCodeState::SysDef *sys = NkCodeState::Systems(&nSys);
					static const char *osNames[8];
					for (int32 i = 0; i < nSys && i < 8; ++i)
						osNames[i] = sys[i].name;
					NkWorkspaceOpts o;
					o.name = wsName;
					for (int32 i = 0; i < 4; ++i)
						o.cfg[i] = wsCfg[i];
					o.os = wsPlat;
					o.osNames = osNames;
					o.nOs = nSys;
					for (int32 i = 0; i < 5; ++i)
						o.arch[i] = wsArch[i];
					o.startProject = (wsMakeProj && wsProjName[0]) ? wsProjName : ""; // startproject() si projet cree
					o.dutc = wsDutc;
					o.dute = wsDute;
					o.androidSdk = androidSdk;
					o.androidNdk = androidNdk;
					o.javaJdk = javaJdk;
					o.harmonySdk = harmonySdk;
					o.gdkPath = gdkPath;
					NkPath dir = wsDir[0] ? NkPath(wsDir) : st->root;
					NkString made = GenerateWorkspaceEx(dir, o);
					if (made.Empty()) {
						status = "Echec : nom invalide ou .jenga deja existant.";
						return;
					}
					// Projet de demarrage : genere le projet + son include() dans le workspace.
					if (wsMakeProj && wsProjName[0])
						GenerateProject(dir, NkPath(made.CStr()), wsProjName, wsProjKind, wsProjLang);
					loading.Start(dir, st, made.CStr()); // ecran de chargement (workspace fraichement cree)
				}

				// Pre-remplit les chemins SDK depuis les variables d'environnement (une fois).
				void FillEnvOnce() {
					if (wsEnvFilled)
						return;
					wsEnvFilled = true;
					auto envc = [](const char *a, const char *b) -> const char * {
						const char *v = env::GetEnvVar(a); // API maison (NkEnv.h)
						if ((!v || !*v) && b)
							v = env::GetEnvVar(b);
						return (v && *v) ? v : nullptr;
					};
					if (const char *v = envc("ANDROID_SDK_ROOT", "ANDROID_HOME"))
						CopyTo(androidSdk, v, 512);
					if (const char *v = envc("ANDROID_NDK_HOME", "ANDROID_NDK_ROOT"))
						CopyTo(androidNdk, v, 512);
					if (const char *v = envc("JAVA_HOME", nullptr))
						CopyTo(javaJdk, v, 512);
					if (const char *v = envc("OHOS_NDK_HOME", "OHOS_SDK"))
						CopyTo(harmonySdk, v, 512);
					if (const char *v = envc("GameDK", "GameDKLatest"))
						CopyTo(gdkPath, v, 512);
				}

				// Un OS (par nom) est-il coche dans le formulaire Nouveau ?
				bool OsChecked(const char *nm) const {
					int32 nSys = 0;
					const NkCodeState::SysDef *sys = NkCodeState::Systems(&nSys);
					for (int32 i = 0; i < nSys; ++i)
						if (wsPlat[i] && StrEqA(sys[i].name, nm))
							return true;
					return false;
				}

				static bool StrEqA(const char *a, const char *b) {
					if (!a || !b)
						return false;
					while (*a && *b) {
						if (*a != *b)
							return false;
						++a;
						++b;
					}
					return *a == *b;
				}

				// Onglet Charger : charge le workspace `i` trouve dans loadDir (via l'ecran de chargement).
				void LoadFoundAt(usize i) {
					if (!st || i >= foundPaths.Size())
						return;
					loading.Start(NkPath(loadDir), st, foundPaths[i].CStr());
				}

				// Selecteur de dossier CUSTOM (NKGui) pour ouvrir un workspace.
				void OpenFolderDialog() {
					OpenPicker(PK_Open, st ? st->root.ToString().CStr() : ".");
				}

				// Ouvrir un .jenga precis : PICKER MAISON en mode fichier (disque entier),
				// discrimine du « Ouvrir un fichier » generique par pickWsJenga.
				bool pickWsJenga = false;
				char wsOpenBuf[512] = {};
				void OpenWorkspaceDialog() {
					wsOpenBuf[0] = '\0';
					OpenPicker(PK_File, st ? st->root.ToString().CStr() : nullptr, wsOpenBuf,
							   (int32)sizeof(wsOpenBuf), nullptr, ".jenga"); // filtre : .jenga uniquement
					pickWsJenga = true; // APRES OpenPicker (qui remet le flag a false)
				}

				// Lance l'ecran de CHARGEMENT (section 14) : LoadFolder + etapes reelles.
				// La bascule vers l'editeur (LoadUiState + showStart=false) se fait quand loading.finished
				// (gere dans DrawHome). Une erreur .jenga affiche l'etat d'erreur inline (pas de bascule).
				void DoLoad(const NkPath &folder) {
					if (wsAddAsRoot && st) {
						// Wizard « Nouveau Workspace » lance DEPUIS L'EDITEUR (modale) :
						// le workspace cree est AJOUTE comme racine de l'explorateur
						// (add folder to workspace) — le workspace courant reste charge.
						wsAddAsRoot = false;
						st->pickedFolder = folder.ToString();
						st->status = NkString("Workspace cree : ") + folder.ToString().CStr();
						return;
					}
					loading.Start(folder, st);
				}

				void OpenSaveAs() {
					mode = SaveAs;
					justOpened = true;
					status.Clear();
					nameBuf[0] = '\0';
					saveProjIdx = -1;
					saveScroll = 0.f;
					if (st && st->HasActive() &&
						!st->files[st->active]
							 .untitled) { // prefill = nom du fichier actif (vide pour un nouveau sans-titre)
						NkString nm = st->files[st->active].Name();
						int32 i = 0;
						for (; nm.CStr()[i] && i + 1 < (int32)sizeof(nameBuf); ++i)
							nameBuf[i] = nm.CStr()[i];
						nameBuf[i] = '\0';
					}
				}

				// Enregistrer l'onglet actif via le GESTIONNAIRE DE FICHIERS natif (dossier + nom + extension).
				// `+`/Ctrl+S sur un sans-titre, ou « Ré-enregistrer » d'un fichier supprimé. Annuler sur un
				// onglet vierge (issu du +) le referme.
				void SaveActiveNative() { // ouvre NOTRE explorateur de dossiers custom en mode ENREGISTRER
					if (!st || !st->HasActive())
						return;
					auto &f = st->files[st->active];
					pickerSaveName[0] = '\0';
					scafName[0] = '\0';
					scafExt[0] = '\0';
					scafKind = 0;
					scafChildren[0] = '\0';
					scafBase[0] = '\0';
					scafFocus = 0;
					CopyTo(scafNs, "nkentseu", (int32)sizeof(scafNs));
					if (!f.untitled)
						CopyTo(pickerSaveName, f.Name().CStr(),
							   (int32)sizeof(pickerSaveName)); // buffer existant -> nom pré-rempli
					const NkString initDir = f.untitled ? st->root.ToString() : f.path.GetParent().ToString();
					const NkString confine = st->root.ToString(); // LIMITE le parcours au workspace + sous-dossiers
					OpenPicker(PK_SaveFile, initDir.CStr(), nullptr, 0, confine.CStr());
					pickerSaveFocus = true;
				}

				void Close() {
					mode = None;
				}
		};

		// ── Items injectes DANS le menu « Fichier » (via SetFileMenu) ──
		// Appele alors que BeginMenu("Fichier") est deja ouvert : on dessine les items
		// directement (creation, enregistrement, deploiement), pas un nouveau menu.
		inline void DrawFileMenu(NkEditorFrameContext &ec, NkCodeDialogs *d) {
			if (!d)
				return;
			auto &ctx = ec.Ui();
			NkCodeState *s = d->st;
			const bool hasWs = s && s->HasWorkspace();
			const bool hasFile = s && s->HasActive();

			if (MenuItem(ctx, "Ecran de demarrage..."))
				d->ShowStart();
			if (MenuItem(ctx, "Nouveau fichier", "Ctrl+N")) {
				if (s)
					s->NewFile();
			}
			if (MenuItem(ctx, "Nouveau projet...", nullptr, hasWs))
				d->Open(NkCodeDialogs::NewProject);
			if (MenuItem(ctx, "Nouveau workspace..."))
				d->Open(NkCodeDialogs::NewWorkspace);
			if (MenuItem(ctx, "Ouvrir un dossier..."))
				d->OpenFolderDialog();
			if (MenuItem(ctx, "Ouvrir un workspace (.jenga)..."))
				d->OpenWorkspaceDialog();

			if (MenuItem(ctx, "Enregistrer", "Ctrl+S", hasFile)) {
				if (s) {
					if (s->ActiveHasPath())
						s->SaveActive();
					else
						d->SaveActiveNative();
				}
			}
			if (MenuItem(ctx, "Enregistrer sous...", "Ctrl+Shift+S", hasFile))
				d->SaveActiveNative();
			if (MenuItem(ctx, "Enregistrer tout", nullptr, hasFile)) {
				if (s)
					s->SaveAll();
			}

			if (BeginMenu(ctx, "Proprietes")) {
				MenuItem(ctx, "Proprietes du projet...", nullptr, false);	 // TODO #5 (editeur de proprietes)
				MenuItem(ctx, "Proprietes du workspace...", nullptr, false); // TODO #5
				EndMenu(ctx);
			}
			if (BeginMenu(ctx, "Deploiement")) {
				// Empaquetage : actif des qu'un workspace est charge et que la
				// plateforme courante est empaquetable (XboxSeries ne l'est pas).
				const bool canPkg = s && s->HasWorkspace() && s->PackagePlatformArg() != nullptr;
				if (MenuItem(ctx, "Empaqueter (jenga package)", nullptr, canPkg) && s)
					s->DoPackage(nullptr); // type par defaut de la plateforme (decide par Jenga)
				// « Deployer » exige --device, et la detection d'appareils n'existe pas
				// encore (roadmap #2) : activer l'entree livrerait un bouton qui echoue
				// faute d'appareil. Reste grisee jusqu'a #2.
				MenuItem(ctx, "Deployer (jenga deploy)", nullptr, false); // TODO #2 puis #3
				if (MenuItem(ctx, "Creer un installateur (.jng)", nullptr,
							 canPkg && s->SupportsJngInstaller()) &&
					s)
					s->DoPackage("jng");
				MenuItem(ctx, "Gerer les emulateurs...", nullptr, false); // TODO #4
				EndMenu(ctx);
			}
		}

		// ── Reglage des flags d'etat CHAQUE FRAME (via SetAppMenu, appele
		// inconditionnellement dans la barre de menus, AVANT la decision de corps du
		// shell). NE PAS mettre dans DrawFileMenu : celui-ci n'est execute que quand le
		// menu Fichier est OUVERT -> le launcher n'apparaitrait qu'en ouvrant Fichier.
		inline void DrawAppFlags(NkEditorFrameContext &ec, NkCodeDialogs *d) {
			if (!d)
				return;
			d->PollExampleCopy(); // clonage d'exemple en cours (launcher OU editeur)
			auto &ctx = ec.Ui();
			const bool appWin = d->showPrefs || d->showNewWs || d->showHelp != 0;
			ctx.appModal = (d->mode != NkCodeDialogs::None) || d->pickerOpen || d->tcOpen || appWin;
			ctx.appFullScreen = d->showStart;
			// MODALITE des fenetres app (Preferences / Nouveau Workspace / Aide)
			// posee EN DEBUT DE FRAME — ce thunk tourne AVANT DrawPanels. La pose
			// dans l'overlay (fin de frame) ne suffisait pas : sur un clic HORS
			// fenetre, Update() venait de faire retomber popupDepth a 0 et les
			// panneaux (traites avant l'overlay) recevaient le clic. Ici on
			// re-force AVANT eux, rect = plein ecran pour la phase panneaux (la
			// modale raffine popupRects[0] a son propre rect ensuite).
			if (appWin) {
				if (ctx.popupDepth == 0)
					ctx.popupDepth = 1;
				const NkRect full = {0.f, 0.f, (float32)ctx.viewW, (float32)ctx.viewH};
				ctx.popupRects[0] = full;
				ctx.popupAnchor = full;
			}
		}

		// ── Ecran de demarrage PLEIN CADRE (via SetStartScreen) ──
		// Carte centree facon « page de demarrage » : sidebar (marque + actions) a
		// gauche, liste des workspaces recents a droite. Remplace l'editeur tant
		// qu'aucun workspace n'est charge. Palette sombre + accent violet (#7c6cf0).
		inline void DrawStartScreen(NkEditorFrameContext &ec, NkCodeDialogs *d) {
			if (!d || !d->showStart)
				return;
			auto &ctx = ec.Ui();
			const NkGuiFont *f = ctx.font;
			if (!f || !f->Valid())
				return;
			// Pompe `jenga info` (toolchains detectees + projets) meme sur le launcher.
			if (d->st) {
				d->st->ScanWorkspaces();
				d->st->LoadProjects();
				d->st->PollProjects();
				d->st->PollConfig();
			}
			auto &dl = ctx.DL();
			const float32 W = (float32)ctx.viewW, H = (float32)ctx.viewH;
			const float32 top = ctx.ItemHeight();
			const float32 asc = f->Ascent(), lh = f->LineHeight();
			const NkVec2 mp = ctx.input.mousePos;
			const bool click = ctx.input.mouseClicked[0];
			const float32 S = ctx.S(1.f);
			auto hit = [&](const NkRect &r) { return NkGuiRectContains(r, mp); };
			auto text = [&](float32 x, float32 y, const char *s, const NkColor &c) {
				dl.AddText(f->Face(), f->TexId(), {x, y + asc}, s, c);
			};

			// Palette — accent bleu #0F73D5 (option choisie ; secondaires orange/teal)
			const NkColor cBack = {11, 13, 16, 255};
			const NkColor cCard = {20, 22, 26, 255};
			const NkColor cSide = {26, 29, 34, 255};
			const NkColor cBorder = {42, 46, 53, 255};
			const NkColor cAccent = {15, 115, 213, 255}; // #0F73D5
			const NkColor cAccentH = {41, 133, 224, 255};
			const NkColor cText = {236, 237, 239, 255};
			const NkColor cSub = {140, 146, 154, 255};
			const NkColor cFaint = {112, 118, 126, 255};
			const NkColor cRowHov = {30, 34, 40, 255};
			const NkColor cSelBg = {22, 42, 64, 255};

			dl.AddRectFilled({0.f, top, W, H - top}, cBack);

			// Carte centree
			const float32 cw = 820.f * S, chh = 540.f * S;
			const float32 cx = (W - cw) * 0.5f, cy = top + (H - top - chh) * 0.5f;
			dl.AddRectFilled({cx, cy, cw, chh}, cCard, 18.f * S);
			dl.AddRect({cx, cy, cw, chh}, cBorder, 1.f);

			auto btn = [&](const NkRect &r, const char *s, bool en) -> bool {
				const bool hov = en && hit(r);
				dl.AddRectFilled(r, !en ? NkColor{30, 32, 40, 255} : hov ? cAccentH : cAccent, 8.f * S);
				const float32 tw = f->MeasureWidth(s);
				text(r.x + (r.w - tw) * 0.5f, r.y + (r.h - lh) * 0.5f, s, en ? NkColor{255, 255, 255, 255} : cFaint);
				return en && hov && click;
			};
			auto sbtn = [&](const NkRect &r, const char *s) -> bool { // bouton secondaire (bordure)
				const bool hov = hit(r);
				dl.AddRectFilled(r, hov ? NkColor{34, 34, 43, 255} : cCard, 8.f * S);
				dl.AddRect(r, hov ? NkColor{56, 56, 70, 255} : cBorder, 1.f);
				const float32 tw = f->MeasureWidth(s);
				text(r.x + (r.w - tw) * 0.5f, r.y + (r.h - lh) * 0.5f, s, cText);
				return hov && click;
			};
			auto check = [&](const NkRect &bx, bool &v, const char *label) {
				const NkRect box = {bx.x, bx.y, 18.f * S, 18.f * S};
				const bool hov = hit({bx.x, bx.y, bx.w, 18.f * S});
				dl.AddRectFilled(box, v ? cAccent : NkColor{31, 31, 39, 255}, 4.f * S);
				dl.AddRect(box, v ? cAccent : (hov ? NkColor{80, 80, 96, 255} : cBorder), 1.f);
				if (v) {
					dl.AddRectFilled({box.x + 4.f * S, box.y + 8.f * S, 4.f * S, 4.f * S}, NkColor{255, 255, 255, 255});
					dl.AddRectFilled({box.x + 7.f * S, box.y + 5.f * S, 7.f * S, 4.f * S}, NkColor{255, 255, 255, 255});
				}
				text(box.x + 26.f * S, bx.y + (18.f * S - lh) * 0.5f, label, cText);
				if (hov && click)
					v = !v;
			};

			// ── Sidebar gauche : marque + 3 onglets de navigation ──
			const float32 sw = 230.f * S;
			dl.AddRectFilled({cx, cy, sw, chh}, cSide, 18.f * S);
			dl.AddRectFilled({cx + sw - 18.f * S, cy, 18.f * S, chh}, cSide);
			dl.AddRectFilled({cx + sw - 1.f, cy + 12.f, 1.f, chh - 24.f}, cBorder);
			const float32 bx = cx + 20.f * S, by = cy + 22.f * S;
			dl.AddRectFilled({bx, by + 7.f * S, 8.f * S, 8.f * S}, cAccent, 2.f * S);
			dl.AddRectFilled({bx + 9.f * S, by, 8.f * S, 8.f * S}, cAccent, 2.f * S);
			dl.AddRectFilled({bx + 12.f * S, by + 12.f * S, 8.f * S, 8.f * S}, cAccent, 2.f * S);
			text(bx + 28.f * S, by, "NKCode", cText);

			const char *tabs[] = {"Recent", "Nouveau", "Charger un workspace"};
			const char *tabsub[] = {"Vos projets recents", "Creer un workspace", "Ouvrir un dossier"};
			float32 ny = cy + 72.f * S;
			for (int32 i = 0; i < 3; ++i) {
				const NkRect r = {cx + 16.f * S, ny, sw - 32.f * S, 50.f * S};
				const bool active = (d->launcherTab == i);
				const bool hov = hit(r);
				dl.AddRectFilled(r, active ? cSelBg : (hov ? NkColor{34, 34, 43, 255} : cSide), 12.f * S);
				if (active)
					dl.AddRect(r, cAccent, 1.f);
				dl.AddRectFilled({r.x + 11.f * S, r.y + 10.f * S, 30.f * S, 30.f * S},
								 active ? cAccent : NkColor{124, 108, 240, 40}, 8.f * S);
				text(r.x + 52.f * S, r.y + 8.f * S, tabs[i], active ? NkColor{255, 255, 255, 255} : cText);
				text(r.x + 52.f * S, r.y + 8.f * S + lh, tabsub[i], cSub);
				if (hov && click)
					d->launcherTab = i;
				ny += 58.f * S;
			}
			text(cx + 20.f * S, cy + chh - 28.f * S, "NKCode - Nkentseu", cFaint);

			// ── Zone principale (depend de l'onglet) ──
			const float32 mx = cx + sw + 22.f * S;
			const float32 mw = cw - sw - 44.f * S;
			const NkColor rowCols[] = {
				{15, 115, 213, 255}, {247, 154, 40, 255}, {10, 85, 95, 255}, {51, 177, 160, 255}};

			if (d->launcherTab == 0) {
				// ===== RECENT =====
				text(mx, cy + 22.f * S, "Workspaces recents", cText);
				float32 ry = cy + 60.f * S;
				const bool canCont = d->st && d->st->HasWorkspace();
				if (canCont) {
					const NkRect r = {mx, ry, mw, 50.f * S};
					const bool hov = hit(r);
					dl.AddRectFilled(r, cSelBg, 10.f * S);
					dl.AddRect(r, cAccent, 1.f);
					dl.AddRectFilled({r.x + 10.f * S, r.y + 10.f * S, 30.f * S, 30.f * S}, cAccent, 8.f * S);
					const char *curNm = (d->st->wsIdx >= 0 && d->st->wsIdx < (int32)d->st->wsNames.Size())
											? d->st->wsNames[d->st->wsIdx].CStr()
											: d->st->root.GetFileName().CStr();
					text(r.x + 52.f * S, r.y + 7.f * S, curNm, NkColor{255, 255, 255, 255});
					text(r.x + 52.f * S, r.y + 7.f * S + lh, d->st->root.ToString().CStr(), cSub);
					const char *badge = "courant";
					const float32 bw = f->MeasureWidth(badge) + 14.f * S;
					const NkRect pb = {r.x + r.w - bw - 14.f * S, r.y + (50.f * S - 18.f * S) * 0.5f, bw, 18.f * S};
					dl.AddRect(pb, cAccent, 1.f);
					text(pb.x + 7.f * S, pb.y + (18.f * S - lh) * 0.5f, badge, cAccent);
					if (hov && click) {
						if (d->shell)
							d->shell->LoadUiState(d->st->UiConfigPath().CStr());
						d->showStart = false;
						return;
					}
					ry += 56.f * S;
				}
				// Dessine une ligne workspace (icone + nom + chemin) avec, au survol, les
				// boutons EPINGLER/DESEPINGLER et RETIRER a droite. action: 1=charger,
				// 2=(des)epingler, 3=retirer.
				auto wsRow = [&](const NkString &path, const char *dispName, int32 colorIdx, bool pinnedRow) -> int32 {
					const NkRect r = {mx, ry, mw, 50.f * S};
					const bool hov = hit(r);
					if (hov)
						dl.AddRectFilled(r, cRowHov, 10.f * S);
					dl.AddRectFilled({r.x + 10.f * S, r.y + 10.f * S, 30.f * S, 30.f * S},
									 pinnedRow ? cAccent : rowCols[colorIdx % 4], 8.f * S);
					text(r.x + 52.f * S, r.y + 7.f * S, dispName, cText); // nom `with workspace(...)`
					text(r.x + 52.f * S, r.y + 7.f * S + lh, path.CStr(), NkColor{140, 140, 150, 255});
					int32 act = 0;
					const NkRect bRem = {r.x + r.w - 30.f * S, r.y + 15.f * S, 20.f * S, 20.f * S};
					const NkRect bPin = {r.x + r.w - 56.f * S, r.y + 15.f * S, 20.f * S, 20.f * S};
					if (hov || pinnedRow) {
						const bool hPin = hit(bPin), hRem = hit(bRem);
						dl.AddRectFilled(bPin, hPin ? NkColor{44, 52, 62, 255} : NkColor{0, 0, 0, 0}, 4.f * S);
						// glyphe epingle (petit losange) — plein si epingle
						dl.AddRectFilled({bPin.x + 8.f * S, bPin.y + 4.f * S, 4.f * S, 8.f * S},
										 pinnedRow ? cAccent : cSub);
						dl.AddRectFilled({bPin.x + 5.f * S, bPin.y + 11.f * S, 10.f * S, 3.f * S},
										 pinnedRow ? cAccent : cSub);
						dl.AddRectFilled(bRem, hRem ? NkColor{60, 34, 38, 255} : NkColor{0, 0, 0, 0}, 4.f * S);
						text(bRem.x + 6.f * S, bRem.y + (20.f * S - lh) * 0.5f, "x", NkColor{226, 114, 91, 255});
						if (click && hPin)
							act = 2;
						else if (click && hRem)
							act = 3;
					}
					if (act == 0 && hov && click)
						act = 1;
					ry += 56.f * S;
					return act;
				};
				// Epingles d'abord
				for (usize i = 0; d->st && i < d->st->pinned.Size(); ++i) {
					const NkString path = d->st->pinned[i];
					const char *nm = (i < d->st->pinnedNames.Size()) ? d->st->pinnedNames[i].CStr() : path.CStr();
					const int32 a = wsRow(path, nm, (int32)i, true);
					if (a == 1) {
						NkPath pp(path.CStr());
						d->DoLoad(pp.GetParent());
						return;
					}
					if (a == 2) {
						d->st->UnpinRecent(path);
						return;
					}
					if (a == 3) {
						d->st->RemoveRecent(path);
						return;
					}
					if (ry > cy + chh - 56.f * S)
						break;
				}
				if (d->st && d->st->recents.Empty() && d->st->pinned.Empty() && !canCont)
					text(mx, ry + 4.f, "(aucun workspace recent)", cFaint);
				for (usize i = 0; d->st && i < d->st->recents.Size(); ++i) {
					const NkString path = d->st->recents[i];
					NkPath pp(path.CStr());
					if (canCont && StrEq(pp.GetParent().ToString().CStr(), d->st->root.ToString().CStr()))
						continue;
					const char *nm = (i < d->st->recentNames.Size()) ? d->st->recentNames[i].CStr() : path.CStr();
					const int32 a = wsRow(path, nm, (int32)i, false);
					if (a == 1) {
						d->DoLoad(pp.GetParent());
						return;
					}
					if (a == 2) {
						d->st->PinRecent(path);
						return;
					}
					if (a == 3) {
						d->st->RemoveRecent(path);
						return;
					}
					if (ry > cy + chh - 56.f * S)
						break;
				}
			} else if (d->launcherTab == 1) {
				// ===== NOUVEAU : toutes les proprietes de creation (DSL Jenga) =====
				if (!d->wsDir[0] && d->st)
					NkCodeDialogs::CopyTo(d->wsDir, d->st->root.ToString().CStr(), (int32)sizeof(d->wsDir));
				d->FillEnvOnce();
				text(mx, cy + 22.f * S, "Nouveau workspace", cText);
				// Zone de formulaire defilante (clip + offset). Bouton Creer fixe en bas.
				const float32 footH = 50.f * S;
				const NkRect area = {mx, cy + 50.f * S, mw, chh - 50.f * S - footH - 12.f * S};
				const bool overArea = hit(area);
				if (overArea && ctx.input.wheel != 0.f) {
					d->newScrollY -= ctx.input.wheel * 36.f;
					ctx.input.wheel = 0.f;
				}
				dl.PushClipRect(area, true);
				float32 y = area.y - d->newScrollY;
				auto label = [&](const char *s) {
					text(mx, y, s, cSub);
					y += 22.f * S;
				};
				// Nom
				label("Nom");
				{
					const NkRect r = {mx, y, mw, 30.f * S};
					NkOverlayTextField(ctx, dl, f, r, d->wsName, (int32)sizeof(d->wsName), d->launcherFocus == 0);
					if (hit(r) && click)
						d->launcherFocus = 0;
				}
				y += 40.f * S;
				// Repertoire + Parcourir
				label("Repertoire");
				{
					const NkRect r = {mx, y, mw - 110.f * S, 30.f * S};
					NkOverlayTextField(ctx, dl, f, r, d->wsDir, (int32)sizeof(d->wsDir), d->launcherFocus == 1);
					if (hit(r) && click)
						d->launcherFocus = 1;
					if (sbtn({mx + mw - 100.f * S, y, 100.f * S, 30.f * S}, "Parcourir")) {
						d->BrowseNewDir();
					}
				}
				y += 40.f * S;
				// Configurations
				label("Configurations");
				{
					int32 nC = 0;
					const char *const *cN = NkConfigNames(&nC);
					for (int32 i = 0; i < nC; ++i)
						check({mx + (i % 2) * (mw * 0.5f), y + (i / 2) * 26.f * S, mw * 0.5f, 18.f * S}, d->wsCfg[i],
							  cN[i]);
					y += ((nC + 1) / 2) * 26.f * S + 12.f * S;
				}
				// Systemes cibles
				label("Systemes cibles (OS)");
				{
					int32 nSys = 0;
					const NkCodeState::SysDef *sys = NkCodeState::Systems(&nSys);
					for (int32 i = 0; i < nSys; ++i)
						check({mx + (i % 2) * (mw * 0.5f), y + (i / 2) * 26.f * S, mw * 0.5f, 18.f * S}, d->wsPlat[i],
							  sys[i].name);
					y += ((nSys + 1) / 2) * 26.f * S + 12.f * S;
				}
				// Architectures cibles
				label("Architectures cibles");
				{
					int32 nA = 0;
					const char *const *aN = NkArchNames(&nA);
					for (int32 i = 0; i < nA; ++i)
						check({mx + (i % 2) * (mw * 0.5f), y + (i / 2) * 26.f * S, mw * 0.5f, 18.f * S}, d->wsArch[i],
							  aN[i]);
					y += ((nA + 1) / 2) * 26.f * S + 12.f * S;
				}
				// Toolchains DETECTEES par Jenga (jenga info) — affichage reel.
				{
					const NkString hdr = NkPrintf("Toolchains detectees par Jenga (%d)",
												  (int)(d->st ? d->st->toolchains.Size() : 0)); // NkPrintf maison
					label(hdr.CStr());
				}
				if (d->st && d->st->toolchains.Empty()) {
					text(mx, y, "(detection en cours...)", cFaint);
					y += 24.f * S;
				}
				for (usize i = 0; d->st && i < d->st->toolchains.Size(); ++i) {
					const NkCodeState::ToolchainRow &t = d->st->toolchains[i];
					// surligne si la cible correspond a un OS coche
					const bool rel = d->OsChecked(t.os.CStr());
					const NkRect r = {mx, y, mw, 24.f * S};
					if (rel) {
						dl.AddRectFilled(r, NkColor{22, 42, 64, 255}, 4.f * S);
						dl.AddRect(r, cAccent, 1.f);
					}
					text(mx + 8.f * S, y + (24.f * S - lh) * 0.5f, t.name.CStr(),
						 rel ? NkColor{255, 255, 255, 255} : cText);
					const NkString meta = NkPrintf("%s  %s/%s %s", t.family.CStr(), t.os.CStr(), t.arch.CStr(),
												   t.env.CStr()); // NkPrintf maison
					text(mx + mw - f->MeasureWidth(meta.CStr()) - 8.f * S, y + (24.f * S - lh) * 0.5f, meta.CStr(),
						 cSub);
					y += 28.f * S;
				}
				if (sbtn({mx, y, 200.f * S, 30.f * S}, "Gerer les toolchains...")) {
					d->tcOpen = true;
				}
				y += 40.f * S;
				// Projet de demarrage (formulaire de projet complet)
				label("Projet de demarrage");
				check({mx, y, mw, 18.f * S}, d->wsMakeProj, "Creer un projet de demarrage avec le workspace");
				y += 26.f * S;
				if (d->wsMakeProj) {
					text(mx, y, "Nom du projet", cSub);
					y += 22.f * S;
					{
						const NkRect r = {mx, y, mw, 30.f * S};
						NkOverlayTextField(ctx, dl, f, r, d->wsProjName, (int32)sizeof(d->wsProjName),
										   d->launcherFocus == 2);
						if (hit(r) && click)
							d->launcherFocus = 2;
					}
					y += 38.f * S;
					text(mx, y, "Genre", cSub);
					y += 22.f * S;
					{
						int32 nk = 0;
						const NkKindDef *K = NkKinds(&nk);
						for (int32 i = 0; i < nk; ++i) {
							const NkRect r = {mx, y, mw, 24.f * S};
							const bool sel = (d->wsProjKind == i);
							dl.AddRectFilled(r, sel ? cSelBg : (hit(r) ? cRowHov : NkColor{24, 27, 32, 255}), 4.f * S);
							if (sel)
								dl.AddRect(r, cAccent, 1.f);
							text(r.x + 10.f * S, r.y + (24.f * S - lh) * 0.5f, K[i].label, sel ? cText : cSub);
							if (hit(r) && click)
								d->wsProjKind = i;
							y += 28.f * S;
						}
					}
					text(mx, y, "Langage", cSub);
					y += 22.f * S;
					{
						int32 nl = 0;
						const NkLangDef *L = NkLangs(&nl);
						float32 lx = mx;
						for (int32 i = 0; i < nl; ++i) {
							const float32 bw = f->MeasureWidth(L[i].label) + 22.f * S;
							const NkRect r = {lx, y, bw, 26.f * S};
							const bool sel = (d->wsProjLang == i);
							dl.AddRectFilled(r, sel ? cAccent : NkColor{30, 34, 40, 255}, 6.f * S);
							text(r.x + 11.f * S, r.y + (26.f * S - lh) * 0.5f, L[i].label,
								 sel ? NkColor{255, 255, 255, 255} : cText);
							if (hit(r) && click)
								d->wsProjLang = i;
							lx += bw + 8.f * S;
						}
						y += 36.f * S;
					}
				}
				// Tests unitaires
				label("Tests unitaires");
				check({mx, y, mw * 0.5f, 18.f * S}, d->wsDutc, "Desactiver la compilation (dutc)");
				check({mx, y + 24.f * S, mw, 18.f * S}, d->wsDute, "Desactiver l'execution (dute)");
				y += 52.f * S;
				// SDK conditionnels
				if (d->OsChecked("Android")) {
					label("Android SDK");
					{
						const NkRect r = {mx, y, mw - 110.f * S, 30.f * S};
						NkOverlayTextField(ctx, dl, f, r, d->androidSdk, 512, d->launcherFocus == 4);
						if (hit(r) && click)
							d->launcherFocus = 4;
						if (sbtn({mx + mw - 100.f * S, y, 100.f * S, 30.f * S}, "..."))
							d->BrowseInto(d->androidSdk, 512, "Android SDK");
					}
					y += 40.f * S;
					label("Android NDK");
					{
						const NkRect r = {mx, y, mw - 110.f * S, 30.f * S};
						NkOverlayTextField(ctx, dl, f, r, d->androidNdk, 512, d->launcherFocus == 5);
						if (hit(r) && click)
							d->launcherFocus = 5;
						if (sbtn({mx + mw - 100.f * S, y, 100.f * S, 30.f * S}, "..."))
							d->BrowseInto(d->androidNdk, 512, "Android NDK");
					}
					y += 40.f * S;
					label("Java JDK");
					{
						const NkRect r = {mx, y, mw - 110.f * S, 30.f * S};
						NkOverlayTextField(ctx, dl, f, r, d->javaJdk, 512, d->launcherFocus == 6);
						if (hit(r) && click)
							d->launcherFocus = 6;
						if (sbtn({mx + mw - 100.f * S, y, 100.f * S, 30.f * S}, "..."))
							d->BrowseInto(d->javaJdk, 512, "Java JDK");
					}
					y += 40.f * S;
				}
				if (d->OsChecked("HarmonyOS")) {
					label("HarmonyOS SDK");
					{
						const NkRect r = {mx, y, mw - 110.f * S, 30.f * S};
						NkOverlayTextField(ctx, dl, f, r, d->harmonySdk, 512, d->launcherFocus == 7);
						if (hit(r) && click)
							d->launcherFocus = 7;
						if (sbtn({mx + mw - 100.f * S, y, 100.f * S, 30.f * S}, "..."))
							d->BrowseInto(d->harmonySdk, 512, "HarmonyOS SDK");
					}
					y += 40.f * S;
				}
				if (d->OsChecked("XboxSeries")) {
					label("Xbox GDK");
					{
						const NkRect r = {mx, y, mw - 110.f * S, 30.f * S};
						NkOverlayTextField(ctx, dl, f, r, d->gdkPath, 512, d->launcherFocus == 8);
						if (hit(r) && click)
							d->launcherFocus = 8;
						if (sbtn({mx + mw - 100.f * S, y, 100.f * S, 30.f * S}, "..."))
							d->BrowseInto(d->gdkPath, 512, "Xbox GDK");
					}
					y += 40.f * S;
				}
				const float32 contentH = (y + d->newScrollY) - area.y;
				dl.PopClipRect();
				// clamp du defilement
				const float32 maxScroll = contentH - area.h > 0.f ? contentH - area.h : 0.f;
				if (d->newScrollY < 0.f)
					d->newScrollY = 0.f;
				if (d->newScrollY > maxScroll)
					d->newScrollY = maxScroll;
				// barre de defilement
				if (maxScroll > 0.f) {
					const float32 th = area.h * (area.h / contentH);
					const float32 tt = area.y + (area.h - th) * (d->newScrollY / maxScroll);
					dl.AddRectFilled({area.x + area.w - 4.f * S, tt, 4.f * S, th}, NkColor{70, 76, 84, 255}, 2.f * S);
				}
				// Pied fixe : status + bouton Creer
				const float32 fy = cy + chh - footH;
				if (!d->status.Empty())
					text(mx, fy - 22.f * S, d->status.CStr(), NkColor{240, 120, 120, 255});
				if (btn({mx, fy, 220.f * S, 36.f * S}, "Creer le workspace", d->wsName[0] != '\0')) {
					d->CreateNew();
					return;
				}
			} else {
				// ===== CHARGER UN WORKSPACE =====
				text(mx, cy + 22.f * S, "Charger un workspace", cText);
				float32 y = cy + 56.f * S;
				text(mx, y, "Dossier", cSub);
				y += 22.f * S;
				{
					const NkRect box = {mx, y, mw - 110.f * S, 30.f * S};
					dl.AddRectFilled(box, NkColor{31, 31, 39, 255}, 6.f * S);
					dl.AddRect(box, cBorder, 1.f);
					text(box.x + 10.f * S, box.y + (30.f * S - lh) * 0.5f, d->loadDir[0] ? d->loadDir : "(non choisi)",
						 d->loadDir[0] ? cText : cFaint);
					if (sbtn({mx + mw - 100.f * S, y, 100.f * S, 30.f * S}, "Parcourir")) {
						d->BrowseLoadDir();
						return;
					}
				}
				y += 48.f * S;
				text(mx, y, "Workspaces du dossier", cSub);
				y += 24.f * S;
				if (d->loadScanned && d->foundNames.Empty())
					text(mx, y, "Aucun workspace dans ce dossier - ouverture impossible.", NkColor{240, 120, 120, 255});
				else if (!d->loadScanned)
					text(mx, y, "Choisissez un dossier avec « Parcourir ».", cFaint);
				for (usize i = 0; i < d->foundNames.Size(); ++i) {
					const NkRect r = {mx, y, mw, 46.f * S};
					const bool hov = hit(r);
					if (hov)
						dl.AddRectFilled(r, cRowHov, 10.f * S);
					dl.AddRectFilled({r.x + 10.f * S, r.y + 9.f * S, 28.f * S, 28.f * S}, rowCols[i % 4], 8.f * S);
					text(r.x + 48.f * S, r.y + 6.f * S, d->foundNames[i].CStr(), cText);
					text(r.x + 48.f * S, r.y + 6.f * S + lh, d->foundPaths[i].CStr(), NkColor{140, 140, 150, 255});
					if (hov && click) {
						d->LoadFoundAt(i);
						return;
					}
					y += 52.f * S;
					if (y > cy + chh - 52.f * S)
						break;
				}
			}
		}

		// ── Interface de gestion des TOOLCHAINS (detectees par Jenga) ──
		inline void DrawToolchains(NkEditorFrameContext &ec, NkCodeDialogs *d) {
			auto &ctx = ec.Ui();
			const NkGuiFont *f = ctx.font;
			if (!f || !f->Valid())
				return;
			auto &dl = ctx.dlOverlay;
			const float32 W = (float32)ctx.viewW, H = (float32)ctx.viewH, S = ctx.S(1.f);
			const float32 asc = f->Ascent(), lh = f->LineHeight();
			const NkVec2 mp = ctx.input.mousePos;
			const bool click = ctx.input.mouseClicked[0];
			auto hit = [&](const NkRect &r) { return NkGuiRectContains(r, mp); };
			auto text = [&](float32 x, float32 y, const char *s, const NkColor &c) {
				dl.AddText(f->Face(), f->TexId(), {x, y + asc}, s, c);
			};
			const NkColor cCard = {22, 24, 29, 255}, cBorder = {50, 55, 63, 255}, cAccent = {15, 115, 213, 255};
			const NkColor cText = {236, 237, 239, 255}, cSub = {150, 156, 164, 255};
			auto sbtn = [&](const NkRect &r, const char *s) -> bool {
				const bool hov = hit(r);
				dl.AddRectFilled(r, hov ? NkColor{40, 46, 54, 255} : NkColor{30, 34, 40, 255}, 6.f * S);
				dl.AddRect(r, cBorder, 1.f);
				const float32 tw = f->MeasureWidth(s);
				text(r.x + (r.w - tw) * 0.5f, r.y + (r.h - lh) * 0.5f, s, cText);
				return hov && click;
			};
			auto pbtn = [&](const NkRect &r, const char *s, bool en) -> bool {
				const bool hov = en && hit(r);
				dl.AddRectFilled(r,
								 !en   ? NkColor{30, 34, 40, 255}
								 : hov ? NkColor{41, 133, 224, 255}
									   : cAccent,
								 6.f * S);
				const float32 tw = f->MeasureWidth(s);
				text(r.x + (r.w - tw) * 0.5f, r.y + (r.h - lh) * 0.5f, s, en ? NkColor{255, 255, 255, 255} : cSub);
				return en && hov && click;
			};

			const float32 pw = 660.f * S, ph = 540.f * S, px = (W - pw) * 0.5f, py = (H - ph) * 0.5f;
			dl.AddRectFilled({0.f, 0.f, W, H}, NkColor{0, 0, 0, 160});
			dl.AddRectFilled({px, py, pw, ph}, cCard, 10.f * S);
			dl.AddRect({px, py, pw, ph}, cBorder, 1.5f);
			const float32 cx = px + 20.f * S, cwid = pw - 40.f * S;

			// ===== Mode EDITION d'un toolchain (ajout / modif -> jenga config) =====
			if (d->tcEdit) {
				text(cx, py + 16.f * S, "Ajouter / modifier un toolchain", cText);
				text(cx, py + 16.f * S + lh + 2.f,
					 "Enregistre via 'jenga config toolchain add' (registre global ~/.jenga).", cSub);
				float32 y = py + 60.f * S;
				auto field = [&](const char *lab, char *buf, int32 cap, int32 id, bool browse) {
					text(cx, y, lab, cSub);
					const NkRect r = {cx + 130.f * S, y - 4.f * S, cwid - 130.f * S - (browse ? 40.f * S : 0.f),
									  26.f * S};
					NkOverlayTextField(ctx, dl, f, r, buf, cap, d->tcEditFocus == id);
					if (hit(r) && click)
						d->tcEditFocus = id;
					if (browse && sbtn({cx + cwid - 34.f * S, y - 4.f * S, 34.f * S, 26.f * S}, "..."))
						d->BrowseInto(buf, cap, lab);
					y += 32.f * S;
				};
				field("Nom", d->teName, 64, 0, false);
				field("Type (famille)", d->teType, 24, 1, false); // clang/gcc/msvc/emscripten/android-ndk/apple-clang
				field("Target OS", d->teOs, 24, 2, false);
				field("Architecture", d->teArch, 24, 3, false);
				field("Env", d->teEnv, 24, 4, false); // gnu/musl/msvc/mingw/android/ios
				field("Compilateur C (cc)", d->teCc, 512, 5, true);
				field("Compilateur C++ (cxx)", d->teCxx, 512, 6, true);
				field("Archiveur (ar)", d->teAr, 512, 7, true);
				field("Target triple", d->teTriple, 128, 8, false);
				field("Sysroot", d->teSysroot, 512, 9, true);
				if (!d->st->cfgStatus.Empty())
					text(cx, py + ph - 78.f * S, d->st->cfgStatus.CStr(), cSub);
				const float32 by = py + ph - 44.f * S;
				if (pbtn({px + pw - 200.f * S, by, 180.f * S, 32.f * S}, "Enregistrer", d->teName[0] != '\0'))
					d->TcEditSave();
				if (sbtn({px + pw - 290.f * S, by, 80.f * S, 32.f * S}, "Annuler"))
					d->tcEdit = false;
				if (ctx.input.KeyPressed(NkGuiKey::Escape))
					d->tcEdit = false;
				return;
			}

			// ===== Liste des toolchains detectees + actions =====
			text(cx, py + 16.f * S, "Toolchains detectees par Jenga", cText);
			text(cx, py + 16.f * S + lh + 2.f,
				 "Modifier/Supprimer un toolchain (registre Jenga) ou editer les chemins SDK.", cSub);

			const NkRect area = {cx, py + 64.f * S, cwid, ph - 64.f * S - 182.f * S};
			dl.AddRectFilled(area, NkColor{16, 18, 22, 255}, 6.f * S);
			dl.AddRect(area, cBorder, 1.f);
			if (hit(area) && ctx.input.wheel != 0.f) {
				d->tcScroll -= ctx.input.wheel * 34.f;
				ctx.input.wheel = 0.f;
			}
			dl.PushClipRect(area, true);
			float32 ly = area.y + 8.f * S - d->tcScroll;
			text(area.x + 12.f * S, ly, "Nom", cSub);
			text(area.x + 180.f * S, ly, "Famille", cSub);
			text(area.x + 290.f * S, ly, "Cible", cSub);
			text(area.x + 440.f * S, ly, "Env", cSub);
			ly += 26.f * S;
			int32 doRemove = -1, doEdit = -1;
			for (usize i = 0; d->st && i < d->st->toolchains.Size(); ++i) {
				const NkCodeState::ToolchainRow &t = d->st->toolchains[i];
				const NkRect row = {area.x + 4.f * S, ly - 2.f * S, area.w - 8.f * S, 26.f * S};
				if (NkGuiRectContains(row, mp) && hit(area))
					dl.AddRectFilled(row, NkColor{30, 34, 40, 255}, 4.f * S);
				text(area.x + 12.f * S, ly, t.name.CStr(), cText);
				text(area.x + 180.f * S, ly, t.family.CStr(), cSub);
				const NkString tgt = NkPrintf("%s/%s", t.os.CStr(), t.arch.CStr()); // NkPrintf maison
				text(area.x + 290.f * S, ly, tgt.CStr(), cSub);
				text(area.x + 440.f * S, ly, t.env.CStr(), cSub);
				const NkRect bE = {area.x + area.w - 120.f * S, ly - 2.f * S, 56.f * S, 22.f * S};
				const NkRect bR = {area.x + area.w - 58.f * S, ly - 2.f * S, 50.f * S, 22.f * S};
				// Toujours dessines (clippes a la zone) ; clic valide seulement si la
				// ligne est dans la partie VISIBLE (evite de cliquer un bouton hors-vue).
				const bool vis = (bE.y >= area.y && bE.y + bE.h <= area.y + area.h);
				if (sbtn(bE, "Modifier") && vis)
					doEdit = (int32)i;
				if (sbtn(bR, "Suppr.") && vis)
					doRemove = (int32)i;
				ly += 28.f * S;
			}
			const float32 contentH = (ly + d->tcScroll) - (area.y + 8.f * S);
			dl.PopClipRect();
			const float32 maxS = contentH - area.h > 0.f ? contentH - area.h : 0.f;
			if (d->tcScroll < 0.f)
				d->tcScroll = 0.f;
			if (d->tcScroll > maxS)
				d->tcScroll = maxS;
			if (doEdit >= 0) {
				d->TcEditFrom(d->st->toolchains[doEdit]);
				return;
			}
			if (doRemove >= 0) {
				d->st->ToolchainRemove(d->st->toolchains[doRemove].name.CStr());
				return;
			}

			// Chemins SDK editables (override via environnement)
			text(cx, area.y + area.h + 8.f * S, "Chemins SDK (override) :", cSub);

			struct PF {
					const char *lab;
					char *buf;
					int32 id;
			};

			const PF pfs[] = {{"Android SDK", d->androidSdk, 0},
							  {"Android NDK", d->androidNdk, 1},
							  {"Java JDK", d->javaJdk, 2},
							  {"HarmonyOS", d->harmonySdk, 3},
							  {"Xbox GDK", d->gdkPath, 4}};
			const float32 yy = area.y + area.h + 30.f * S, colA = cx, colB = cx + cwid * 0.5f + 8.f * S;
			for (int32 i = 0; i < 5; ++i) {
				const float32 bx = (i % 2 == 0) ? colA : colB;
				const float32 bw = cwid * 0.5f - 8.f * S;
				const float32 ry = yy + (i / 2) * 28.f * S;
				text(bx, ry, pfs[i].lab, cSub);
				const NkRect r = {bx + 90.f * S, ry - 4.f * S, bw - 124.f * S, 24.f * S};
				NkOverlayTextField(ctx, dl, f, r, pfs[i].buf, 512, d->tcFocus == pfs[i].id);
				if (hit(r) && click)
					d->tcFocus = pfs[i].id;
				if (sbtn({bx + bw - 30.f * S, ry - 4.f * S, 30.f * S, 24.f * S}, "..."))
					d->BrowseInto(pfs[i].buf, 512, pfs[i].lab);
			}

			const float32 by = py + ph - 44.f * S;
			if (!d->st->cfgStatus.Empty())
				text(cx, by - 22.f * S, d->st->cfgStatus.CStr(), cSub);
			if (pbtn({px + 20.f * S, by, 200.f * S, 32.f * S}, "+ Ajouter un toolchain", true)) {
				d->TcEditNew();
				return;
			}
			if (sbtn({px + 230.f * S, by, 180.f * S, 32.f * S}, "Rafraichir la detection")) {
				if (d->st)
					d->st->RequestReload();
			}
			if (sbtn({px + pw - 110.f * S, by, 90.f * S, 32.f * S}, "Fermer")) {
				d->tcOpen = false;
			}
			if (ctx.input.KeyPressed(NkGuiKey::Escape))
				d->tcOpen = false;
		}

		// ── Selecteur de dossier CUSTOM (NKGui) : modal centre, navigation arborescente ──
		// ── Selecteur de dossier/fichier : DELEGUE au widget moteur NkDrawFilePicker
		//    (NKEditorKit). NKCode ne fournit que le STYLE (couleurs, scrollbars
		//    theme-aware) et ROUTE le resultat (charger / enregistrer / scaffolding /
		//    dossier au workspace). Le rendu + la navigation vivent dans le moteur. ──
		inline void DrawFolderPicker(NkEditorFrameContext &ec, NkCodeDialogs *d) {
			auto &ctx = ec.Ui();
			NkFilePickerStyle sty;                       // couleurs par defaut (sombre) ...
			sty.scrollTrack = NkScrollTrack();           // ... + scrollbars THEME-AWARE NKCode
			sty.scrollThumb = NkScrollThumb(false);
			sty.scrollThumbHover = NkScrollThumb(true);
			// Capture AVANT le rendu : la confirmation ferme le picker (pickerFor remis a None).
			const bool wasNewFile = d->PickerIsNewFile();
			const int32 wasFor = d->pickerFor;
			NkDrawFilePicker(ctx, *d, sty);
			if (d->pickerConfirmed) {
				d->pickerConfirmed = false;
				if (wasFor == NkCodeDialogs::PK_SaveFile) {
					if (wasNewFile)
						d->DoScaffoldCreate();           // assistant de creation (scaffolding)
					else
						d->DoSaveHere();                 // enregistrer sous <dossier>/<nom>
				} else
					d->RoutePickerResult();              // charger / wsDir / loadDir / pickedFolder
			}
			if (d->pickerCancelled) {
				d->pickerCancelled = false;
				if (wasFor == NkCodeDialogs::PK_SaveFile)
					d->CancelSaveCleanup();              // referme l'onglet « + » vierge
			}
		}

		// ── Overlay modal (appele apres les panneaux via SetOverlay) ──
		inline void DrawOverlay(NkEditorFrameContext &ec, NkCodeDialogs *d) {
			if (!d)
				return;
			if (d->pickerOpen) {
				DrawFolderPicker(ec, d);
				return;
			}
			if (d->tcOpen) {
				DrawToolchains(ec, d);
				return;
			}
			if (d->mode == NkCodeDialogs::None)
				return;
			auto &ctx = ec.Ui();
			const NkGuiFont *f = ctx.font;
			if (!f || !f->Valid())
				return;
			auto &dl = ctx.dlOverlay;
			const float32 W = (float32)ctx.viewW, H = (float32)ctx.viewH;
			const float32 asc = f->Ascent(), lh = f->LineHeight();
			const NkVec2 mp = ctx.input.mousePos;
			const bool click = ctx.input.mouseClicked[0];
			auto hit = [&](const NkRect &r) { return NkGuiRectContains(r, mp); };
			auto text = [&](float32 x, float32 y, const char *s, const NkColor &c) {
				dl.AddText(f->Face(), f->TexId(), {x, y + asc}, s, c);
			};
			auto btn = [&](const NkRect &r, const char *s, bool en) -> bool {
				const bool hov = en && hit(r);
				dl.AddRectFilled(r,
								 !en   ? NkColor{30, 34, 40, 255}
								 : hov ? NkColor{56, 104, 184, 255}
									   : NkColor{40, 46, 54, 255},
								 5.f);
				dl.AddRect(r, NkColor{60, 66, 74, 255}, 1.f);
				const float32 tw = f->MeasureWidth(s);
				text(r.x + (r.w - tw) * 0.5f, r.y + (r.h - lh) * 0.5f, s,
					 en ? NkColor{230, 237, 243, 255} : NkColor{110, 118, 126, 255});
				return en && hov && click;
			};

			const bool isProj = (d->mode == NkCodeDialogs::NewProject);
			const bool isSaveAs = (d->mode == NkCodeDialogs::SaveAs);
			const float32 pw = 460.f, ph = isProj ? 560.f : (isSaveAs ? 452.f : 320.f), px = (W - pw) * 0.5f,
						  py = (H - ph) * 0.5f;
			dl.AddRectFilled({0.f, 0.f, W, H}, NkColor{0, 0, 0, 150});
			const NkRect panel = {px, py, pw, ph};
			// Fermeture par clic hors panneau (sauf frame d'ouverture).
			if (d->justOpened)
				d->justOpened = false;
			else if (click && !hit(panel)) {
				d->Close();
				ctx.appModal = false;
				return;
			}
			dl.AddRectFilled(panel, NkColor{28, 33, 40, 255}, 8.f);
			dl.AddRect(panel, NkColor{70, 78, 88, 255}, 1.5f);

			const char *title = isProj ? "Nouveau projet" : isSaveAs ? "Enregistrer sous" : "Nouveau workspace";
			text(px + 18.f, py + 16.f, title, NkColor{230, 237, 243, 255});

			const float32 cx = px + 20.f;
			const NkColor lblC = {160, 170, 180, 255};

			if (isSaveAs) {
				// ── Enregistrer sous : 1) PROJET (dossier), 2) nom + extension, 3) aperçu du chemin ──
				const auto &projs = d->st->cdb.projects; // {name, dir, ...}
				float32 y = py + 50.f;
				text(cx, y, "Projet de destination", lblC);
				y += 20.f;
				const NkRect listR = {cx, y, pw - 40.f, 196.f};
				dl.AddRectFilled(listR, NkColor{20, 24, 30, 255}, 4.f);
				dl.AddRect(listR, NkColor{60, 66, 74, 255}, 1.f);
				const bool lin = NkGuiRectContains(listR, mp);
				if (lin && ctx.input.wheel != 0.f) {
					d->saveScroll -= ctx.input.wheel * 30.f;
					ctx.input.wheel = 0.f;
				}
				const float32 rowH = 24.f;
				const float32 fullH = projs.Size() * rowH;
				const float32 maxS = fullH - listR.h > 0.f ? fullH - listR.h : 0.f;
				if (d->saveScroll < 0.f)
					d->saveScroll = 0.f;
				if (d->saveScroll > maxS)
					d->saveScroll = maxS;
				dl.PushClipRect(listR, true);
				for (usize i = 0; i < projs.Size(); ++i) {
					const float32 ry = listR.y + i * rowH - d->saveScroll;
					if (ry + rowH < listR.y || ry > listR.y + listR.h)
						continue;
					const NkRect r = {listR.x + 2.f, ry, listR.w - 4.f, rowH};
					const bool sel = (d->saveProjIdx == (int32)i);
					if (sel)
						dl.AddRectFilled(r, NkColor{38, 60, 92, 255}, 3.f);
					else if (hit(r) && lin)
						dl.AddRectFilled(r, NkColor{33, 39, 48, 255}, 3.f);
					text(r.x + 8.f, r.y + (rowH - lh) * 0.5f, projs[i].name.CStr(),
						 sel ? NkColor{230, 237, 243, 255} : NkColor{185, 193, 201, 255});
					if (lin && hit(r) && click)
						d->saveProjIdx = (int32)i;
				}
				dl.PopClipRect();
				if (maxS > 0.f) {
					const float32 th = listR.h * (listR.h / fullH);
					const float32 ty = listR.y + (listR.h - th) * (d->saveScroll / maxS);
					dl.AddRectFilled({listR.x + listR.w - 5.f, ty, 4.f, th}, NkColor{80, 88, 96, 255}, 2.f);
				}
				y += 204.f;
				text(cx, y, "Nom du fichier (avec extension, ex: MonFichier.cpp)", lblC);
				y += 20.f;
				const NkRect nr = {cx, y, pw - 40.f, 28.f};
				NkOverlayTextField(ctx, dl, f, nr, d->nameBuf, (int32)sizeof(d->nameBuf), true);
				y += 34.f;
				// Aperçu du chemin résolu.
				if (d->saveProjIdx >= 0 && d->saveProjIdx < (int32)projs.Size() && d->nameBuf[0]) {
					NkString full = projs[d->saveProjIdx].dir;
					full += "/";
					full += d->nameBuf;
					text(cx, y, (NkString("-> ") + full.CStr()).CStr(), NkColor{120, 180, 130, 255});
				}
			} else if (!isProj) {
				// ── Workspace : Nom + EMPLACEMENT. Le workspace cree est AJOUTE comme
				//    racine de l'explorateur (comme « Ajouter un dossier au workspace »)
				//    et identifie comme nouveau workspace — PAS de retour au launcher. ──
				float32 y = py + 52.f;
				text(cx, y, "Nom", lblC);
				y += 22.f;
				{
					const NkRect r = {cx, y, pw - 40.f, 28.f};
					if (hit(r) && click)
						d->projFocus = 0;
					NkOverlayTextField(ctx, dl, f, r, d->nameBuf, (int32)sizeof(d->nameBuf), d->projFocus == 0);
				}
				y += 40.f;
				text(cx, y, "Emplacement (le dossier <nom> y sera cree)", lblC);
				y += 22.f;
				{
					const NkRect r = {cx, y, pw - 82.f, 28.f};
					if (hit(r) && click)
						d->projFocus = 1;
					NkOverlayTextField(ctx, dl, f, r, d->wsDir, (int32)sizeof(d->wsDir), d->projFocus == 1);
					if (d->wsDir[0] == '\0' && d->projFocus != 1)
						text(r.x + 10.f, r.y + (28.f - lh) * 0.5f, d->st->root.ToString().CStr(),
							 NkColor{110, 118, 126, 255});
					if (btn({cx + pw - 76.f, y, 36.f, 28.f}, "...", true))
						d->BrowseNewDir(); // picker maison (PK_NewDir -> wsDir)
				}
				y += 40.f;
				text(cx, y, "Ajoute a l'explorateur comme NOUVEAU workspace (le courant reste charge).",
					 NkColor{120, 180, 130, 255});
			} else {
				// ── Projet : formulaire COMPLET, defilable ──
				int32 nk = 0, nl = 0, nd = 0;
				const NkKindDef *K = NkKinds(&nk);
				const NkLangDef *L = NkLangs(&nl);
				const char *const *D = NkDialects(&nd);
				const bool isApp = K[d->kindIdx >= 0 && d->kindIdx < nk ? d->kindIdx : 0].app;
				const bool isCpp = (d->langIdx == 0);
				const NkRect content = {px + 2.f, py + 42.f, pw - 4.f, ph - 42.f - 50.f};
				const bool mic = NkGuiRectContains(content, mp);
				if (mic && ctx.input.wheel != 0.f) {
					d->projScroll -= ctx.input.wheel * 30.f;
					ctx.input.wheel = 0.f;
				}
				// CLAMP AVANT le dessin (hauteur mesuree a la frame precedente) : sinon
				// une frame se dessine HORS bornes puis est re-clampee -> clignotement
				// du contenu quand la scrollbar est en butee haut/bas.
				{
					const float32 maxS0 = d->projContentH - content.h > 0.f ? d->projContentH - content.h : 0.f;
					if (d->projScroll < 0.f)
						d->projScroll = 0.f;
					if (d->projScroll > maxS0)
						d->projScroll = maxS0;
				}
				dl.PushClipRect(content, true);
				const float32 fw = pw - 40.f;
				float32 y = content.y + 6.f - d->projScroll;
				auto field = [&](const char *lab, char *buf, int32 cap, int32 id, bool browse) {
					text(cx, y, lab, lblC);
					y += 20.f;
					const NkRect r = {cx, y, fw - (browse ? 36.f : 0.f), 26.f};
					NkOverlayTextField(ctx, dl, f, r, buf, cap, d->projFocus == id);
					if (mic && hit(r) && click)
						d->projFocus = id;
					if (browse && btn({cx + fw - 32.f, y, 32.f, 26.f}, "...", true) && mic)
						d->BrowseInto(buf, cap, lab);
					y += 32.f;
				};
				// ── Workspace d'APPARTENANCE : tout projet depend d'un workspace. La
				//    liste = ceux de l'EXPLORATEUR (racine chargee + racines ajoutees,
				//    deja fusionnes dans wsPaths par ScanWorkspaces). Le CHOIX n'est
				//    propose que s'il y en a PLUSIEURS. ──
				text(cx, y, "Workspace d'appartenance", lblC);
				y += 22.f;
				{
					const int32 curIdx = d->st->wsIdx;
					if (d->st->wsPaths.Size() <= 1) {
						// Un seul workspace dans l'explorateur : pas de choix, on l'affiche.
						const NkString nm = (curIdx >= 0 && curIdx < (int32)d->st->wsNames.Size())
												? d->st->wsNames[curIdx]
												: NkString("(workspace courant)");
						text(cx + 2.f, y, nm.CStr(), NkColor{230, 237, 243, 255});
						y += 28.f;
					} else {
						const int32 selIdx = (d->projWsIdx >= 0 && d->projWsIdx < (int32)d->st->wsPaths.Size())
												 ? d->projWsIdx
												 : curIdx;
						for (usize i = 0; i < d->st->wsPaths.Size(); ++i) {
							const NkRect r = {cx, y, fw, 24.f};
							const bool sel = ((int32)i == selIdx);
							dl.AddRectFilled(r,
											 sel ? NkColor{38, 60, 92, 255}
												 : (mic && hit(r) ? NkColor{33, 39, 48, 255} : NkColor{24, 28, 34, 255}),
											 4.f);
							if (sel)
								dl.AddRect(r, NkColor{88, 166, 255, 255}, 1.f);
							NkString nm = (i < d->st->wsNames.Size() && !d->st->wsNames[i].Empty())
											  ? d->st->wsNames[i]
											  : d->st->wsPaths[i];
							if ((int32)i == curIdx)
								nm += " (courant)";
							dl.PushClipRect({r.x, r.y, r.w - 8.f, r.h}, true);
							text(r.x + 10.f, r.y + (24.f - lh) * 0.5f, nm.CStr(),
								 sel ? NkColor{230, 237, 243, 255} : NkColor{180, 188, 196, 255});
							dl.PopClipRect();
							if (mic && hit(r) && click)
								d->projWsIdx = (int32)i;
							y += 28.f;
						}
					}
					y += 6.f;
				}
				field("Nom du projet", d->nameBuf, (int32)sizeof(d->nameBuf), 0, false);
				// Genre (2 colonnes)
				text(cx, y, "Genre", lblC);
				y += 22.f;
				for (int32 i = 0; i < nk; ++i) {
					const NkRect r = {cx + (i % 2) * (fw * 0.5f + 4.f), y + (i / 2) * 28.f, fw * 0.5f - 4.f, 24.f};
					const bool sel = (d->kindIdx == i);
					dl.AddRectFilled(r,
									 sel ? NkColor{38, 60, 92, 255}
										 : (hit(r) ? NkColor{33, 39, 48, 255} : NkColor{24, 28, 34, 255}),
									 4.f);
					if (sel)
						dl.AddRect(r, NkColor{88, 166, 255, 255}, 1.f);
					text(r.x + 10.f, r.y + (r.h - lh) * 0.5f, K[i].label,
						 sel ? NkColor{230, 237, 243, 255} : NkColor{180, 188, 196, 255});
					if (mic && hit(r) && click)
						d->kindIdx = i;
				}
				y += ((nk + 1) / 2) * 28.f + 8.f;
				// Langage
				text(cx, y, "Langage", lblC);
				y += 22.f;
				float32 lx = cx;
				for (int32 i = 0; i < nl; ++i) {
					const float32 bw = f->MeasureWidth(L[i].label) + 20.f;
					if (btn({lx, y, bw, 26.f}, L[i].label, true) && mic)
						d->langIdx = i;
					if (d->langIdx == i)
						dl.AddRect({lx, y, bw, 26.f}, NkColor{88, 166, 255, 255}, 1.5f);
					lx += bw + 6.f;
				}
				y += 34.f;
				if (isCpp) {
					text(cx, y, "Dialecte C++", lblC);
					y += 22.f;
					float32 dx = cx;
					for (int32 i = 0; i < nd; ++i) {
						const float32 bw = f->MeasureWidth(D[i]) + 18.f;
						if (btn({dx, y, bw, 24.f}, D[i], true) && mic)
							d->projDialect = i;
						if (d->projDialect == i)
							dl.AddRect({dx, y, bw, 24.f}, NkColor{88, 166, 255, 255}, 1.5f);
						dx += bw + 6.f;
					}
					y += 32.f;
				}
				{
					const NkString ph2 = NkPrintf("Fichiers sources (defaut src/**.%s)",
												  L[d->langIdx >= 0 && d->langIdx < nl ? d->langIdx : 0].ext);
					field(ph2.CStr(), d->projFiles, (int32)sizeof(d->projFiles), 1, false);
				}
				field("Dossiers d'include (csv)", d->projInc, (int32)sizeof(d->projInc), 2, false);
				field("Bibliotheques a lier - links (csv)", d->projLinks, (int32)sizeof(d->projLinks), 3, false);
				field("Dossiers de bibliotheques - libdirs (csv)", d->projLib, (int32)sizeof(d->projLib), 4, false);
				field("Dependances - dependson (csv)", d->projDeps, (int32)sizeof(d->projDeps), 5, false);
				field("Defines (csv)", d->projDefines, (int32)sizeof(d->projDefines), 6, false);
				// Avertissements
				text(cx, y, "Avertissements", lblC);
				y += 22.f;
				{
					const char *wl[] = {"Defaut", "Off", "High", "Extra", "Everything"};
					float32 wx = cx;
					for (int32 i = 0; i < 5; ++i) {
						const float32 bw = f->MeasureWidth(wl[i]) + 16.f;
						if (btn({wx, y, bw, 24.f}, wl[i], true) && mic)
							d->projWarn = i;
						if (d->projWarn == i)
							dl.AddRect({wx, y, bw, 24.f}, NkColor{88, 166, 255, 255}, 1.5f);
						wx += bw + 5.f;
					}
				}
				y += 34.f;
				if (isApp) {
					text(cx, y, "Version", lblC);
					text(cx + fw * 0.5f + 6.f, y, "Editeur", lblC);
					y += 20.f;
					{
						const NkRect r = {cx, y, fw * 0.5f - 6.f, 26.f};
						NkOverlayTextField(ctx, dl, f, r, d->projVersion, (int32)sizeof(d->projVersion),
										   d->projFocus == 7);
						if (mic && hit(r) && click)
							d->projFocus = 7;
					}
					{
						const NkRect r = {cx + fw * 0.5f + 6.f, y, fw * 0.5f - 6.f, 26.f};
						NkOverlayTextField(ctx, dl, f, r, d->projPublisher, (int32)sizeof(d->projPublisher),
										   d->projFocus == 8);
						if (mic && hit(r) && click)
							d->projFocus = 8;
					}
					y += 32.f;
					field("Icone de l'application", d->projIcon, (int32)sizeof(d->projIcon), 9, true);
				}
				const float32 contentH = (y + d->projScroll) - (content.y + 6.f);
				d->projContentH = contentH; // memorise pour le clamp pre-dessin
				dl.PopClipRect();
				// barre de defilement
				const float32 maxS = contentH - content.h > 0.f ? contentH - content.h : 0.f;
				if (d->projScroll < 0.f)
					d->projScroll = 0.f;
				if (d->projScroll > maxS)
					d->projScroll = maxS;
				if (maxS > 0.f) {
					const float32 th = content.h * (content.h / contentH);
					const float32 ty = content.y + (content.h - th) * (d->projScroll / maxS);
					dl.AddRectFilled({content.x + content.w - 5.f, ty, 4.f, th}, NkColor{80, 88, 96, 255}, 2.f);
				}
			}

			if (!d->status.Empty())
				text(cx, py + ph - 70.f, d->status.CStr(), NkColor{240, 120, 120, 255});

			// ── Boutons Creer/Enregistrer / Annuler ──
			const float32 by = py + ph - 42.f;
			if (btn({px + pw - 110.f, by, 96.f, 30.f}, isSaveAs ? "Enregistrer" : "Creer", d->nameBuf[0] != '\0')) {
				if (isSaveAs) {
					// Destination = <dossier du PROJET choisi>/<nom>. Repli : chemin absolu tel quel,
					// sinon sous la racine du workspace.
					const auto &projs = d->st->cdb.projects;
					bool abs = false;
					for (const char *p = d->nameBuf; *p; ++p)
						if (*p == ':' || *p == '\\') {
							abs = true;
							break;
						}
					NkPath dest;
					if (d->saveProjIdx >= 0 && d->saveProjIdx < (int32)projs.Size())
						dest = NkPath(projs[d->saveProjIdx].dir.CStr()) / d->nameBuf;
					else if (abs)
						dest = NkPath(d->nameBuf);
					else
						dest = d->st->root / d->nameBuf;
					if (d->st->SaveActiveAs(dest)) {
						d->saveProjIdx = -1;
						d->Close();
						ctx.appModal = false;
						return;
					}
					d->status = "Echec : impossible d'ecrire le fichier.";
				} else {
					NkString made;
					if (isProj) {
						int32 nd = 0;
						const char *const *D = NkDialects(&nd);
						const char *warnL[] = {"", "Off", "High", "Extra", "Everything"};
						NkProjectOpts po;
						po.name = d->nameBuf;
						po.kindIdx = d->kindIdx;
						po.langIdx = d->langIdx;
						po.cppDialect =
							(d->langIdx == 0 && d->projDialect >= 0 && d->projDialect < nd) ? D[d->projDialect] : "";
						po.filesPattern = d->projFiles;
						po.defines = d->projDefines;
						po.includeDirs = d->projInc;
						po.libDirs = d->projLib;
						po.links = d->projLinks;
						po.dependsOn = d->projDeps;
						po.warnings = warnL[d->projWarn >= 0 && d->projWarn < 5 ? d->projWarn : 0];
						po.appVersion = d->projVersion;
						po.appPublisher = d->projPublisher;
						po.appIcon = d->projIcon;
						// Workspace d'APPARTENANCE : parmi ceux de l'EXPLORATEUR (wsPaths
						// = racine chargee + racines ajoutees) ; defaut = courant.
						// Racine du projet = dossier du .jenga choisi (vrai aussi pour
						// un workspace d'une racine secondaire).
						const int32 wsSel = (d->projWsIdx >= 0 && d->projWsIdx < (int32)d->st->wsPaths.Size())
												? d->projWsIdx
												: d->st->wsIdx;
						const NkPath wsJenga = NkPath(d->st->wsPaths[wsSel].CStr());
						made = GenerateProjectEx(wsJenga.GetParent(), wsJenga, po);
					} else {
						// ── Nouveau workspace : cree <emplacement>/<nom>/ + <nom>.jenga,
						//    puis l'AJOUTE comme racine de l'explorateur (meme mecanisme
						//    que « Ajouter un dossier au workspace ») — le workspace
						//    courant reste charge, PAS de retour au launcher. ──
						NkWorkspaceOpts wo;
						wo.name = d->nameBuf;
						const NkPath base = d->wsDir[0] ? NkPath(d->wsDir) : d->st->root;
						const NkPath wdir = base / NkSanitizeName(d->nameBuf).CStr();
						const NkString wmade = GenerateWorkspaceEx(wdir, wo);
						if (wmade.Empty()) {
							d->status = "Echec : nom invalide ou .jenga deja existant.";
						} else {
							d->st->pickedFolder = wdir.ToString(); // explorateur : NOUVELLE racine
							d->st->AddRecent(wmade);
							d->st->OpenPath(NkPath(wmade.CStr())); // ouvre le .jenga genere
							d->st->status = NkString("Workspace cree : ") + wdir.ToString().CStr();
							d->Close();
							ctx.appModal = false;
							return;
						}
					}
					if (isProj) {
						if (made.Empty()) {
							d->status = "Echec : nom invalide ou dossier deja existant.";
						} else {
							d->st->RequestReload();
							d->st->mWsScanned = false;
							d->st->ScanWorkspaces();
							d->st->OpenPath(NkPath(made.CStr())); // ouvre le .jenga genere
							d->Close();
							ctx.appModal = false;
							return;
						}
					}
				}
			}
			if (btn({px + pw - 218.f, by, 96.f, 30.f}, "Annuler", true)) {
				d->Close();
				ctx.appModal = false;
				return;
			}

			// Echap = annuler, Entree = creer (si nom valide)
			if (ctx.input.KeyPressed(NkGuiKey::Escape)) {
				d->Close();
				ctx.appModal = false;
				return;
			}

			ctx.appModal = true; // maintien
		}

	} // namespace nkcode
} // namespace nkentseu
