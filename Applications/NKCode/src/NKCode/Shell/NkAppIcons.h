#pragma once
// =============================================================================
// NkAppIcons.h — Chargement de TOUTES les icônes/logos de NKCode (extrait de
// main.cpp, modularisation) : upload GPU net (downscale progressif), rognage
// alpha, override utilisateur, TABLE UNIQUE kAppIcons, dossiers spéciaux,
// registre d'extensions data-driven (icons.cfg), activity bars.
// =============================================================================
#include "NKEditorKit/NkEditorKit.h"
#include "NKImage/NKImage.h"
#include "NKPlatform/NkEnv.h"
#include "NKContainers/String/NkFormat.h"
#include "NKCode/Shell/NkHome.h"

namespace nkentseu {
	namespace nkcode {

		// Charge logos + icônes (table unique) + manifeste extensions + activity
		// bars. À appeler après l'Init du shell (upload GPU) et le wiring de home.
		inline void NkLoadAppIcons(editorkit::NkEditorShell *shell, NkHomeState &home, NkCodeState &st) {
				// Charge une texture NETTE : PNG en priorite (repli SVG), puis REDIMENSIONNE
				// a ~ la taille d'affichage (tw x th) au filtre bilineaire. Sans mipmaps, une
				// texture bien plus grande que l'affichage est sous-echantillonnee (flou) ;
				// on l'amene donc proche de sa taille a l'ecran. `base` = nom sans extension.
				// box=true : letterbox dans un carre tw x th (icones -> taille uniforme).
				// box=false : upload a la taille ajustee fw x fh (wordmark -> ratio tight, sans bandes).
				auto upload = [&](NkImage &img, int32 tw, int32 th, int32 *outW, int32 *outH, bool box) -> uint32 {
					const int32 sw = img.Width(), sh = img.Height();
					if (sw <= 0 || sh <= 0)
						return 0;
					// Fit en PRESERVANT L'ASPECT (anti-deformation) dans tw x th.
					const float32 ar = (float32)sw / (float32)sh, tar = (float32)tw / (float32)th;
					int32 fw, fh;
					if (ar >= tar) {
						fw = tw;
						fh = (int32)((float32)tw / ar + 0.5f);
					} else {
						fh = th;
						fw = (int32)((float32)th * ar + 0.5f);
					}
					if (fw < 1)
						fw = 1;
					if (fh < 1)
						fh = 1;
					// Downscale PROGRESSIF par demi-pas : un bilinéaire direct 128->~35 ne prend que
					// 2x2 texels et CRÉNÈLE le line-art (pas de mipmaps). En halvant (128->64->35),
					// chaque étape moyenne 2x2 -> approxime un filtre surface -> icônes NETTES.
					NkImage *fitted;
					if (sw == fw && sh == fh) {
						fitted = &img;
					} else {
						NkImage *inter = nullptr;
						const NkImage *src = &img;
						int32 cw = sw, chh = sh;
						while (cw >= fw * 2 && chh >= fh * 2) {
							NkImage *half = src->Resize(cw / 2, chh / 2);
							if (!half)
								break;
							if (inter)
								inter->Free();
							inter = half;
							src = half;
							cw = half->Width();
							chh = half->Height();
						}
						fitted = src->Resize(fw, fh);
						if (inter)
							inter->Free();
						if (!fitted)
							fitted = &img;
					}
					uint32 id = 0;
					if (!box || (fw == tw && fh == th)) { // remplit la cible (ou pas de letterbox) -> direct
						id = shell->UploadRGBA(fitted->Pixels(), fw, fh);
						if (outW)
							*outW = fw;
						if (outH)
							*outH = fh;
					} else { // LETTERBOX dans un carre transparent
						NkImage *canvas = NkImage::Create((uint32)tw, (uint32)th, 4, 0u);
						if (canvas && canvas->IsValid()) {
							canvas->Blit(*fitted, (tw - fw) / 2, (th - fh) / 2);
							id = shell->UploadRGBA(canvas->Pixels(), tw, th);
							if (outW)
								*outW = tw;
							if (outH)
								*outH = th;
							canvas->Free();
						} else {
							id = shell->UploadRGBA(fitted->Pixels(), fw, fh);
							if (outW)
								*outW = fw;
							if (outH)
								*outH = fh;
						}
					}
					if (fitted != &img)
						fitted->Free();
					return id;
				};
				// Rogne les marges TRANSPARENTES (bounding box alpha) -> le glyphe remplit son
				// bitmap. Sans ca, une icone 128x128 avec grande marge interne parait plus PETITE
				// qu'une icone qui remplit son bitmap (ex. Ouvrir 40x32) -> tailles inegales.
				auto trimAlpha = [](NkImage &src) -> NkImage * {
					if (!src.IsValid() || src.Channels() < 4)
						return nullptr;
					const int32 w = src.Width(), h = src.Height(), ch = src.Channels();
					const uint8 *px = src.Pixels();
					if (!px)
						return nullptr;
					const usize stride = (usize)w * ch;
					int32 minX = w, minY = h, maxX = -1, maxY = -1;
					for (int32 yy = 0; yy < h; ++yy) {
						const uint8 *row = px + (usize)yy * stride;
						for (int32 xx = 0; xx < w; ++xx)
							if (row[(usize)xx * ch + 3] > 10) {
								if (xx < minX)
									minX = xx;
								if (xx > maxX)
									maxX = xx;
								if (yy < minY)
									minY = yy;
								if (yy > maxY)
									maxY = yy;
							}
					}
					if (maxX < minX || maxY < minY)
						return nullptr; // tout transparent
					if (minX == 0 && minY == 0 && maxX == w - 1 && maxY == h - 1)
						return nullptr; // deja bord-a-bord
					return src.Crop(minX, minY, maxX - minX + 1, maxY - minY + 1);
				};
				// Dossier d'OVERRIDE utilisateur (icones personnalisees, data-driven) : deposer un
				// PNG dans <ovrDir>icon/<Nom>.png remplace l'icone livree, sans recompiler.
				NkString ovrDirS;
				{
					const char *ad = env::GetEnvVar("APPDATA"); // API maison (NKPlatform/NkEnv.h)
					const char *hm = env::GetEnvVar("HOME");
					if (ad && *ad)
						ovrDirS = NkPrintf("%s/NKCode/", ad);
					else if (hm && *hm)
						ovrDirS = NkPrintf("%s/.config/nkcode/", hm);
					else
						ovrDirS = "Applications/NKCode/data/textures/";
				}
				const char *ovrDir = ovrDirS.CStr();
				// Candidat relatif a l'EXECUTABLE (distribution : l'utilisateur peut
				// lancer NKCode.exe depuis n'importe quel dossier -> les chemins
				// relatifs au CWD ne trouvent rien).
				const NkString edT = NkPath::GetExecutableDirectory().ToString();
				const NkString exeTex = edT.Empty() ? NkString("data/textures/") : (edT + "/data/textures/");
				auto loadTex = [&](const char *base, int32 tw, int32 th, int32 *outW = nullptr, int32 *outH = nullptr,
								   bool trim = true, bool box = true) -> uint32 {
					const char *dirs[] = {ovrDir, "Applications/NKCode/data/textures/", "data/textures/",
										  "NKCode/data/textures/", exeTex.CStr(), ""};
					auto put = [&](NkImage &img) -> uint32 { // rogne (option) puis upload
						NkImage *t = trim ? trimAlpha(img) : nullptr;
						uint32 id = upload(t ? *t : img, tw, th, outW, outH, box);
						if (t)
							t->Free();
						return id;
					};
					for (const char *const *d = dirs;; ++d) {
						// Outils MAISON : NkPrintf + NkFile::Exists (pas de snprintf/fopen).
						const NkString png = NkPrintf("%s%s.png", *d, base);
						if (NkFile::Exists(png.CStr())) {
							NkImage img;
							if (img.LoadFromFile(png.CStr()) && img.IsValid())
								return put(img);
						}
						const NkString svg = NkPrintf("%s%s.svg", *d, base);
						if (NkFile::Exists(svg.CStr())) {
							NkImage *im =
								NkSVGCodec::DecodeFromFile(svg.CStr(), tw * 2, th * 2); // rasterise large puis reduit = net
							if (im && im->IsValid()) {
								uint32 id = put(*im);
								im->Free();
								return id;
							}
							if (im)
								im->Free();
						}
						if (!**d)
							break;
					}
					return 0;
				};
				// Logos. Le wordmark COMPLET contient "nkcode" + sous-titre "INTELLIGENT IDE" :
				// illisible/flou s'il est reduit a la taille minuscule de la barre de titre. On
				// pre-redimensionne donc le wordmark cote CPU (filtre qualite) a ~ sa taille
				// d'affichage sidebar -> NET (sans mipmaps, uploader 512px puis laisser le GPU
				// sous-echantillonner produit du flou). La barre de titre utilise l'ICONE (nette)
				// + "nkcode" en police vectorielle (toujours net), conforme a la maquette.
				home.logoIcon = loadTex("logo/nkcode_icon", 48, 48); // icone barre de titre (elle seule, sans texte)
				// Wordmark en 2 versions PRETES : nkcode_white (fond sombre) + nkcode_dark (fond clair / theme Light).
				home.logoWord =
					loadTex("logo/nkcode_white", 360, 90, &home.wordW, &home.wordH, /*trim*/ true, /*box*/ false);
				home.logoWordDark =
					loadTex("logo/nkcode_dark", 360, 90, &home.wordWD, &home.wordHD, /*trim*/ true, /*box*/ false);
				shell->SetTitleLogo(home.logoIcon, 1.0f); // aspect>0 => icone carree SEULE (pas de texte "nkcode")
				// Icones : uploadees a la taille d'AFFICHAGE (DPI-aware) -> NET. Sans mipmaps,
				// uploader plus grand que l'ecran puis laisser le GPU reduire = flou. On
				// redimensionne donc la source 128px directement a ~ sa taille ecran (CPU, filtre
				// qualite). Les icones s'affichent ~13-22 px logiques -> upload ~26 px * DPI.
				const float32 dpi = shell->DpiScale();
				int32 IS = (int32)(32.f * dpi + 0.5f);
				if (IS < 24)
					IS = 24; // source 128px -> downscale progressif net
				nkcode::NkIcons &ic = home.icons;
				// ═══ TABLE UNIQUE des icônes de l'application ═══════════════════════
				// TOUTES les icônes nommées se déclarent ICI (champ <- data/textures/…)
				// et nulle part ailleurs : une ligne par icône, chargée par la boucle.
				{
					struct IconDef {
							uint32 *slot;
							const char *path;
					};
					const IconDef kAppIcons[] = {
						{&ic.accueil, "icon/Home"},
						{&ic.ouvrir, "icon/FolderOpened"},
						{&ic.ouvrirDossier, "icon/FolderOpened"},
						{&ic.nouveau, "icon/NewFile"},
						{&ic.cloner, "icon/RepoClone"},
						{&ic.toolchains, "icon/Tools"},
						{&ic.platforms, "icon/Vm"},
						{&ic.gear, "icon/SettingsGear"},
						{&ic.exemple, "icon/Book"},
						{&ic.star, "icon/StarFull"},
						{&ic.search, "icon/SearchC"},
						{&ic.workspace, "logo/workspace"}, // workspace.png est dans logo/
						// Navigateur « Ouvrir un Workspace »
						{&ic.back, "icon/ArrowLeft"},
						{&ic.forward, "icon/ArrowRight"},
						{&ic.up, "icon/ArrowUp"},
						{&ic.downArrow, "icon/ArrowDown"},
						{&ic.bureau, "icon/Bureau"},
						{&ic.disque, "icon/Disque"},
						{&ic.jenga, "icon/Jenga"},
						{&ic.valide, "icon/CheckAll"},
						{&ic.horloge, "icon/History"},
						{&ic.fichier, "icon/FileC"},
						{&ic.sort, "icon/Sort"},
						// Wizard projet : types + actions + validation
						{&ic.kConsole, "icon/TerminalC"},
						{&ic.kWindowed, "icon/EmptyWindow"},
						{&ic.kStatic, "icon/ArchiveC"},
						{&ic.kShared, "icon/Link"},
						{&ic.kTest, "icon/Beaker"},
						{&ic.kConfig, "icon/Settings"},
						{&ic.valideSimple, "icon/Check"},
						{&ic.editer, "icon/Edit"},
						{&ic.dependance, "icon/Dependance"},
						{&ic.creeProjet, "icon/NewFolder"},
						{&ic.fileCode, "icon/FileCode"},
						{&ic.plus, "icon/Add"},
						{&ic.corbeille, "icon/Trash"},
						{&ic.lock, "icon/Lock"},
						{&ic.clonerTel, "icon/CloudDownload"},
						{&ic.github, "icon/Github"},
						{&ic.oeilOuvert, "icon/Eye"},
						{&ic.oeilFermer, "icon/EyeClosed"},
						{&ic.rondI, "icon/Info"},
						// ── Vue principale IDE : vraies icones (Lucide -> assets reels) ──
						{&ic.hammer, "icon/Hammer"}, // build (marteau Lucide)
						// Activity bars (textures)
						{&ic.files, "icon/Files"},
						{&ic.android, "icon/Android"},
						{&ic.apple, "icon/Apple"},
						{&ic.windowsLogo, "icon/WindowsLogo"},
						{&ic.claude, "icon/Claude"},
						{&ic.sourceControl, "icon/SourceControl"},
						{&ic.liveShare, "icon/LiveShare"},
						{&ic.codeC, "icon/CodeC"},
						{&ic.warning, "icon/Warning"},
						{&ic.bug, "icon/DebugAlt"}, // debug
						{&ic.sparkles, "icon/Sparkle"}, // IA
						{&ic.zap, "icon/Eclaire"}, // moteur
						{&ic.chart, "icon/Graph"}, // profiler
						{&ic.puzzle, "icon/Extensions"}, // extensions
						{&ic.eraser, "icon/ClearAll"}, // nettoyer
						{&ic.rebuild, "icon/Sync"}, // rebuild
						{&ic.play, "icon/PlayC"}, // executer
						{&ic.monitor, "icon/Vm"}, // plateforme
						{&ic.flask, "icon/Beaker"}, // tests
						{&ic.layers, "icon/Layers"}, // solution
						{&ic.pkg, "icon/Package"}, // projet
						{&ic.globe, "icon/GlobeC"}, // web
						{&ic.pause, "icon/DebugPause"},
						{&ic.stop, "icon/DebugStop"},
						{&ic.gitPush, "icon/RepoPush"},
						{&ic.gitPull, "icon/RepoPull"},
						{&ic.split, "icon/SplitHorizontal"},
						{&ic.folderOpen, "icon/FolderOpen"},
						{&ic.folder, "icon/Folder"}, // dossier FERMÉ (0 si absent -> dessin au trait)
						// ── Dossiers Material (colorés, rendus SANS teinte) + toolbar explorateur ──
						{&ic.folderM, "icon/FolderM"},
						{&ic.folderMOpen, "icon/FolderMOpen"},
						{&ic.folderRoot, "icon/FolderRoot"},
						{&ic.folderRootOpen, "icon/FolderRootOpen"},
						{&ic.collapseAll, "icon/CollapseAll"},
						{&ic.newFile2, "icon/NewFile"},
						{&ic.newFolder, "icon/NewFolder"},
						{&ic.filter, "icon/Filter"},
						{&ic.fileText, "icon/FileText"},
						{&ic.fileCode2, "icon/FileCode2"},
						{&ic.filePlus, "icon/NewFile"},
						{&ic.code, "icon/Code"},
						{&ic.compare, "icon/GitCompare"},
						{&ic.blame, "icon/Account"},
						{&ic.exit, "icon/SignOut"},
						{&ic.tags, "icon/Tag"},
						{&ic.cloud, "icon/CloudC"},
						{&ic.docker, "icon/Docker"},
						{&ic.linux, "icon/TerminalLinux"},
					};
					for (const IconDef &d : kAppIcons)
						*d.slot = loadTex(d.path, IS, IS);
				}
				{ // dossiers SPÉCIAUX : nom -> paire fermée/ouverte (insensible à la casse)
					struct DPair {
							const char *stem;
							const char *names[6];
					};
					static const DPair kDirs[] = {
						{"FolderSrc", {"src", "source", "sources", nullptr}},
						{"FolderTest", {"test", "tests", "unitest", nullptr}},
						{"FolderInclude", {"include", "inc", "headers", nullptr}},
						{"FolderDocs", {"docs", "doc", "documentation", nullptr}},
						{"FolderResource", {"assets", "data", "media", "resources", "textures", nullptr}},
						{"FolderShader", {"shaders", "shader", nullptr}},
						{"FolderConfig", {"config", ".config", "settings", nullptr}},
						{"FolderGit", {".git", nullptr}},
						{"FolderScripts", {"scripts", "tools", nullptr}},
						{"FolderDist", {"build", "bin", "dist", "out", "obj", nullptr}},
					};
					for (const DPair &d : kDirs) {
						const uint32 tc = loadTex((NkString("icon/") + d.stem).CStr(), IS, IS);
						const uint32 to = loadTex((NkString("icon/") + d.stem + "Open").CStr(), IS, IS);
						if (tc || to)
							for (int32 j = 0; d.names[j]; ++j)
								ic.SetDir(d.names[j], tc ? tc : to, to ? to : tc);
					}
				}
				st.icons = &home.icons; // rend les icones accessibles aux panneaux/toolbar (via l'etat)
				{ // Activity bars : textures codicon (remplacent les dessins au trait du shell)
					const uint32 L[7] = {ic.files, ic.search,	 ic.sourceControl, ic.bug,
										 ic.liveShare, ic.puzzle, ic.chart}; // vues gauche 0..6
					// 100 Claude (vrai logo), 101 Codex, 102 Assistant (Maison), 103 NkAI (etincelle)
					const uint32 R[4] = {ic.claude, ic.codeC, ic.accueil, ic.sparkles};
					shell->SetActivityIcons(L, 7, ic.gear, R, 4);
				}
				// toggle liste/grille : pas d'asset adapte (`<>` et `↕` ne conviennent pas) -> dessine.

				// ── Registre d'extensions DATA-DRIVEN (icons.cfg) : .ext -> icone ──
				{
					NkVector<NkString> stemName;
					NkVector<uint32> stemTex; // cache stem -> texture (evite re-upload)
					auto loadStem = [&](const NkString &stem) -> uint32 {
						for (usize i = 0; i < stemName.Size(); ++i)
							if (stemName[i] == stem)
								return stemTex[i];
						const NkString base = NkString("icon/") + stem.CStr();
						const uint32 t = loadTex(base.CStr(), IS, IS);
						stemName.PushBack(stem);
						stemTex.PushBack(t);
						return t;
					};
					auto trim = [](NkString s) -> NkString {
						int32 a = 0, b = (int32)s.Length();
						const char *d = s.CStr();
						while (a < b && (d[a] == ' ' || d[a] == '\t'))
							++a;
						while (b > a && (d[b - 1] == ' ' || d[b - 1] == '\t' || d[b - 1] == '\r'))
							--b;
						return s.SubStr(a, b - a);
					};
					auto applyManifest = [&](const NkString &text) {
						const char *p = text.CStr();
						NkString line;
						auto flush = [&]() {
							NkString l = trim(line);
							if (!l.Empty() && l.CStr()[0] != '#') {
								int32 eq = -1;
								const char *d = l.CStr();
								for (int32 k = 0; d[k]; ++k)
									if (d[k] == '=') {
										eq = k;
										break;
									}
								if (eq > 0) {
									const NkString key = trim(l.SubStr(0, eq));
									const NkString val = trim(l.SubStr(eq + 1, l.Length() - eq - 1));
									if (!key.Empty() && key.CStr()[0] == '.' && !val.Empty())
										ic.SetExt(key.CStr(), loadStem(val));
								}
							}
							line.Clear();
						};
						for (;; ++p) {
							if (*p == '\n' || *p == '\0') {
								flush();
								if (*p == '\0')
									break;
							} else
								line += *p;
						}
					};
					// 1) defauts integres (au cas ou aucun manifeste n'est present) ;
					applyManifest(NkString(".cpp=Cpp\n.cc=Cpp\n.h=Header\n.hpp=Header\n.c=C\n.py=Python\n.rs=Rust\n.zig=Zig\n."
										   "jenga=Jenga\n.md=Markdown\n.txt=Texte\n.json=Json\n.png=Image\n.jpg=Image\n.zip="
										   "Archive\n.exe=Binaire\n.dll=Binaire\n"));
					// 2) manifeste livre (dernier candidat = a cote de l'EXECUTABLE, pour
					// une distribution lancee depuis un autre dossier) ;
					// 3) override utilisateur (applique en dernier -> gagne).
					const NkString exeMan = edT.Empty() ? NkString("data/icons.cfg") : (edT + "/data/icons.cfg");
					for (const char *mp : {"Applications/NKCode/data/icons.cfg", "data/icons.cfg", exeMan.CStr()})
						if (NkFile::Exists(mp)) {
							applyManifest(NkFile::ReadAllText(NkPath(mp)));
							break;
						}
					{
						const NkString uman = NkString(ovrDir) + "icons.cfg";
						if (NkFile::Exists(uman.CStr()))
							applyManifest(NkFile::ReadAllText(NkPath(uman.CStr())));
					}
				}
		}

	} // namespace nkcode
} // namespace nkentseu
