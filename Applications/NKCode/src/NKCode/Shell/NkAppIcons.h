#pragma once
// =============================================================================
// NkAppIcons.h — Chargement de TOUTES les icônes/logos de NKCode (extrait de
// main.cpp, modularisation) : upload GPU net (downscale progressif), rognage
// alpha, override utilisateur, TABLE UNIQUE kAppIcons, dossiers spéciaux,
// registre d'extensions data-driven (icons.cfg), activity bars.
// =============================================================================
#include "NKCore/NkTraits.h" // traits::NkMove (NkImage est deplacable, pas copiable)
#include "NKEditorKit/NkEditorKit.h"
#include "NKImage/NKImage.h"
#include "NKPlatform/NkEnv.h"
#include "NKContainers/String/NkFormat.h"
#include "NKCode/Shell/NkHome.h"
#include "NKCode/Shell/NkAppData.h" // NkCodeData : data/ quel que soit le dossier de lancement
#include "NKCode/Shell/NkJeuxIcones.h" // (01/10) jeux d'icones installables : chaine de recherche
#include "NKCode/Shell/NkSyntheseSegments.h" // (01/10) icones des segments de vues
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace nkcode {

		// ── (01/10) LES JEUX D'ICONES : un cache par (jeu, variante de theme) ──────
		// Basculer de jeu ne recharge rien deux fois : la table d'un jeu deja vu est
		// reprise telle quelle (memes textures). Revenir au jeu d'avant rend donc
		// EXACTEMENT la table d'avant -- c'est ce que le banc (NkBancApparences.h)
		// verifie.
		struct NkAppIconsCache {
				editorkit::NkEditorShell *shell = nullptr;
				NkHomeState *home = nullptr;
				NkCodeState *st = nullptr;
				NkString cleCourante; ///< « pastilles » / « trait|clair » ...
				NkString jeuCourant;
				NkVector<NkString> cles;
				NkVector<NkIcons> tables;
				/// Mutation de banc (NK_BANC_MUTATION=sansrepli) : la chaine perd
				/// l'actuel -- une icone absente du jeu devient un trou.
				bool sansRepli = false;
		};
		inline NkAppIconsCache &NkAppIconsEtat() {
			static NkAppIconsCache c;
			return c;
		}

		/// La cle de cache d'un jeu pour un theme : la variante ne compte que si le
		/// jeu (ou un de ses replis) en declare.
		inline NkString NkAppIconsCle(const char *jeu, bool clair) {
			// L'id EFFECTIF : un jeu choisi mais desinstalle, c'est Pastilles.
			NkString c = NkCodeJeuxIcones().catalogue.Empty() ? NkString(jeu ? jeu : "")
															   : NkCodeJeuxIcones().Effectif(jeu);
			if (NkCodeJeuxIcones().DependDuTheme(jeu))
				c += clair ? "|clair" : "|sombre";
			return c;
		}

		/// Publie une table d'icones : home.icons, st.icons, barres d'activite.
		inline void NkPublierIcones(editorkit::NkEditorShell *shell, NkHomeState &home, NkCodeState &st,
									const NkIcons &table) {
			home.icons = table;
			st.icons = &home.icons; // rend les icones accessibles aux panneaux/toolbar (via l'etat)
			const NkIcons &t = home.icons;
			const uint32 L[7] = {t.files, t.search, t.sourceControl, t.bug, t.liveShare, t.puzzle, t.chart};
			// 100 Claude (vrai logo), 101 Codex, 102 Assistant (Maison), 103 NkAI (etincelle)
			const uint32 R[4] = {t.claude, t.codeC, t.accueil, t.sparkles};
			shell->SetActivityIcons(L, 7, t.gear, R, 4);
			// Les segments de vues de la Synthese : memes icones que les vues.
			uint32 *seg = NkSegmentsIcones();
			seg[0] = t.files;
			seg[1] = t.search;
			seg[2] = t.sourceControl;
			seg[3] = t.jenga;
			seg[4] = t.puzzle;
			seg[5] = t.liveShare;
		}

		inline void NkChargerJeuIcones(editorkit::NkEditorShell *shell, NkHomeState &home, NkCodeState &st,
									   const char *jeu, bool clair, bool logos);

		/// Rend le jeu `jeu` actif (variante du theme `clair`). Ne fait rien si c'est
		/// deja lui. Rend vrai si la table a change.
		inline bool NkAppliquerJeuIcones(const char *jeu, bool clair) {
			NkAppIconsCache &c = NkAppIconsEtat();
			if (!c.shell || !c.home || !c.st)
				return false;
			const NkString cle = NkAppIconsCle(jeu, clair);
			if (cle == c.cleCourante)
				return false;
			NkChargerJeuIcones(c.shell, *c.home, *c.st, jeu, clair, false);
			return true;
		}

		// Charge logos + icônes (table unique) + manifeste extensions + activity
		// bars, pour UN jeu d'icones. Appele au demarrage (NkLoadAppIcons, logos
		// compris) puis a chaque bascule de jeu ou de theme (NkAppliquerJeuIcones).
		inline void NkChargerJeuIcones(editorkit::NkEditorShell *shell, NkHomeState &home, NkCodeState &st,
									   const char *jeu, bool clair, bool logos) {
				NkAppIconsCache &cache = NkAppIconsEtat();
				cache.shell = shell;
				cache.home = &home;
				cache.st = &st;
				const NkString cleCache = NkAppIconsCle(jeu, clair);
				// Deja vu : on reprend la table telle quelle (aucun re-televersement).
				for (usize k = 0; k < cache.cles.Size(); ++k)
					if (cache.cles[k] == cleCache) {
						cache.cleCourante = cleCache;
						cache.jeuCourant = NkString(jeu ? jeu : "");
						NkPublierIcones(shell, home, st, cache.tables[k]);
						return;
					}
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
					// `fitted` INVALIDE signifie « rien a redimensionner (ou echec) : on
					// televerse `img` telle quelle ». NkImage est un TYPE VALEUR : chaque
					// etape libere ses pixels seule en sortant de la portee.
					NkImage fitted;
					if (sw != fw || sh != fh) {
						NkImage inter;			   // etape courante du demi-pas (PROPRIETAIRE)
						const NkImage *src = &img; // simple OBSERVATEUR : ne possede rien
						int32 cw = sw, chh = sh;
						while (cw >= fw * 2 && chh >= fh * 2) {
							NkImage half = src->Resize(cw / 2, chh / 2);
							if (!half.IsValid())
								break;
							cw = half.Width();
							chh = half.Height();
							// Le move-assign libere l'etape precedente et adopte la
							// nouvelle : `src` designe toujours le meme objet `inter`.
							inter = traits::NkMove(half);
							src = &inter;
						}
						fitted = src->Resize(fw, fh);
					}
					// Repli sur la source quand le redimensionnement n'a pas eu lieu.
					const NkImage &use = fitted.IsValid() ? fitted : img;
					uint32 id = 0;
					if (!box || (fw == tw && fh == th)) { // remplit la cible (ou pas de letterbox) -> direct
						id = shell->UploadRGBA(use.Pixels(), fw, fh);
						if (outW)
							*outW = fw;
						if (outH)
							*outH = fh;
					} else { // LETTERBOX dans un carre transparent
						NkImage canvas = NkImage::Create((uint32)tw, (uint32)th, 4, 0u);
						if (canvas.IsValid()) {
							canvas.Blit(use, (tw - fw) / 2, (th - fh) / 2);
							id = shell->UploadRGBA(canvas.Pixels(), tw, th);
							if (outW)
								*outW = tw;
							if (outH)
								*outH = th;
						} else {
							id = shell->UploadRGBA(use.Pixels(), fw, fh);
							if (outW)
								*outW = fw;
							if (outH)
								*outH = fh;
						}
					}
					return id;
				};
				// Rogne les marges TRANSPARENTES (bounding box alpha) -> le glyphe remplit son
				// bitmap. Sans ca, une icone 128x128 avec grande marge interne parait plus PETITE
				// qu'une icone qui remplit son bitmap (ex. Ouvrir 40x32) -> tailles inegales.
				// Rend une image INVALIDE quand il n'y a rien a rogner (l'appelant garde
				// alors la source), jamais un pointeur : NkImage est un type valeur.
				auto trimAlpha = [](NkImage &src) -> NkImage {
					if (!src.IsValid() || src.Channels() < 4)
						return NkImage();
					const int32 w = src.Width(), h = src.Height(), ch = src.Channels();
					const uint8 *px = src.Pixels();
					if (!px)
						return NkImage();
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
						return NkImage(); // tout transparent
					if (minX == 0 && minY == 0 && maxX == w - 1 && maxY == h - 1)
						return NkImage(); // deja bord-a-bord
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
				// Le dossier des textures livrees, cherche UNE fois (NkAppData.h) :
				// dossier courant, dossier de l'EXECUTABLE (paquet), puis en remontant
				// jusqu'au depot. Sans la remontee, NKCode lance depuis Build/Bin/...
				// n'avait ni icones ni logo (un carre bleu a sa place).
				const NkString texDir = NkCodeDataDir("textures");
				logger.Info("[NKCode] textures (logo, icones) : {0}\n", texDir.Empty() ? "(introuvables)" : texDir.CStr());
				// (01/10) LES JEUX D'ICONES (extensions de donnees) : catalogue data/extensions/ +
				// installees %APPDATA%/NKCode/extensions/, decouverts UNE fois ; puis la chaine
				// du jeu choisi -- override, jeu (variante du theme), replis, Pastilles, base.
				NkJeuxIcones &jeux = NkCodeJeuxIcones();
				if (jeux.catalogue.Empty()) {
					jeux.dossierBase = texDir.Empty() ? NkString() : texDir + "icon/";
					jeux.dossierOverride = NkString(ovrDir) + "icon/";
					// data/ = le dossier des textures sans « textures/ » (« @data/ » des manifestes)
					jeux.dossierData = texDir.Size() > 9 ? texDir.SubStr(0, texDir.Size() - 9) : NkString();
					jeux.catalogue = NkCodeDataDir("extensions", false);
					jeux.installees = NkString(ovrDir) + "extensions/";
					jeux.Decouvrir();
					for (usize k = 0; k < jeux.jeux.Size(); ++k)
						logger.Info("[NKCode] jeu d'icones « {0} » ({1}) {2} : {3}\n", jeux.jeux[k].cle.CStr(),
									jeux.jeux[k].titre.CStr(),
									jeux.jeux[k].integre ? "integre" : jeux.jeux[k].installe ? "installe" : "catalogue",
									jeux.jeux[k].images.CStr());
				}
				NkVector<NkMaillonIcones> chaine = jeux.Chaine(jeu, clair ? "clair" : "sombre");
				if (cache.sansRepli && chaine.Size() > 1)
					chaine.PopBack(); // MUTATION DE BANC : plus d'actuel en bout de chaine
				nkcode::NkIcons fresh; // la table de CE jeu (le cache la garde)
				nkcode::NkIcons &ic = fresh;
				auto loadTex = [&](const char *base, int32 tw, int32 th, int32 *outW = nullptr, int32 *outH = nullptr,
								   bool trim = true, bool box = true) -> uint32 {
					const char *dirs[] = {ovrDir, texDir.CStr(), ""};
					auto put = [&](NkImage &img) -> uint32 { // rogne (option) puis upload
						NkImage t;
						if (trim)
							t = trimAlpha(img);
						return upload(t.IsValid() ? t : img, tw, th, outW, outH, box);
					};
					// « icon/<Nom> » : par la CHAINE du jeu (override d'abord, l'actuel en dernier).
					if (base[0] == 'i' && base[1] == 'c' && base[2] == 'o' && base[3] == 'n' && base[4] == '/') {
						const NkIconeTrouvee f = jeux.Resoudre(chaine, base + 5);
						if (f.chemin.Empty())
							return 0;
						NkImage img;
						if (f.chemin.EndsWith(".svg"))
							img = NkSVGCodec::DecodeFromFile(f.chemin.CStr(), tw * 2, th * 2); // large puis reduit = net
						else
							(void)img.LoadFromFile(f.chemin.CStr());
						if (!img.IsValid())
							return 0;
						NkImage t;
						if (trim && f.rogner)
							t = trimAlpha(img);
						const uint32 id = upload(t.IsValid() ? t : img, tw, th, outW, outH, box);
						if (f.mono && id)
							ic.SetMono(id);
						return id;
					}
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
							NkImage im =
								NkSVGCodec::DecodeFromFile(svg.CStr(), tw * 2, th * 2); // rasterise large puis reduit = net
							if (im.IsValid())
								return put(im);
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
				if (logos) {
				home.logoIcon = loadTex("logo/nkcode_icon", 48, 48); // icone barre de titre (elle seule, sans texte)
				// Wordmark en 2 versions PRETES : nkcode_white (fond sombre) + nkcode_dark (fond clair / theme Light).
				home.logoWord =
					loadTex("logo/nkcode_white", 360, 90, &home.wordW, &home.wordH, /*trim*/ true, /*box*/ false);
				home.logoWordDark =
					loadTex("logo/nkcode_dark", 360, 90, &home.wordWD, &home.wordHD, /*trim*/ true, /*box*/ false);
				shell->SetTitleLogo(home.logoIcon, 1.0f); // aspect>0 => icone carree SEULE (pas de texte "nkcode")
				}
				// Icones : uploadees a la taille d'AFFICHAGE (DPI-aware) -> NET. Sans mipmaps,
				// uploader plus grand que l'ecran puis laisser le GPU reduire = flou. On
				// redimensionne donc la source 128px directement a ~ sa taille ecran (CPU, filtre
				// qualite). Les icones s'affichent ~13-22 px logiques -> upload ~26 px * DPI.
				const float32 dpi = shell->DpiScale();
				int32 IS = (int32)(32.f * dpi + 0.5f);
				if (IS < 24)
					IS = 24; // source 128px -> downscale progressif net
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
				if (logos) // Les barres d'activite sont opt-in dans le kit : NKCode les demande.
					shell->SetActivityBars(true, true);
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
									else if (key == NkString("*") && !val.Empty())
										ic.defaultFile = loadStem(val); // (01/10) extension non listee
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
					// 2) manifeste livre (NkAppData.h : dossier courant, a cote de
					// l'EXECUTABLE, puis en remontant jusqu'au depot) ;
					// 3) override utilisateur (applique en dernier -> gagne).
					// (01/10) celui de l'actuel puis ceux des replis et du jeu (le jeu gagne) ;
					// sans manifeste de jeu (data/icons absent), data/icons.cfg comme avant.
					const NkVector<NkString> tables = jeux.TablesExtensions(jeu);
					if (tables.Empty()) {
						const NkString man = NkCodeData("icons.cfg");
						if (!man.Empty())
							applyManifest(NkFile::ReadAllText(NkPath(man.CStr())));
					}
					for (usize k = 0; k < tables.Size(); ++k)
						if (NkFile::Exists(tables[k].CStr()))
							applyManifest(NkFile::ReadAllText(NkPath(tables[k].CStr())));
					{
						const NkString uman = NkString(ovrDir) + "icons.cfg";
						if (NkFile::Exists(uman.CStr()))
							applyManifest(NkFile::ReadAllText(NkPath(uman.CStr())));
					}
				}
				// Publie la table du jeu et la garde (revenir a ce jeu la reprendra telle quelle).
				cache.cles.PushBack(cleCache);
				cache.tables.PushBack(fresh);
				cache.cleCourante = cleCache;
				cache.jeuCourant = NkString(jeu ? jeu : "");
				NkPublierIcones(shell, home, st, fresh);
		}

		// Charge logos + icônes (table unique) + manifeste extensions + activity
		// bars. À appeler après l'Init du shell (upload GPU) et le wiring de home.
		// (01/10) Le jeu vient des reglages (`jeuIcones`, defaut « pastilles ») ;
		// sa variante, du theme.
		inline void NkLoadAppIcons(editorkit::NkEditorShell *shell, NkHomeState &home, NkCodeState &st) {
			if (!home.settings.loaded)
				home.settings.Load();
			if (const char *m = env::GetEnvVar("NK_BANC_MUTATION"))
				NkAppIconsEtat().sansRepli = (m[0] == 's'); // « sansrepli »
			NkChargerJeuIcones(shell, home, st, home.settings.jeuIcones, NkThemeIdEstClair(home.settings.theme),
							   /*logos*/ true);
		}

	} // namespace nkcode
} // namespace nkentseu
