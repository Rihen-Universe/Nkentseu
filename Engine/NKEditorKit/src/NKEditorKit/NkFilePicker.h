#pragma once
// -----------------------------------------------------------------------------
// @File    NkFilePicker.h
// @Brief   Coeur REUTILISABLE du selecteur de dossiers/fichiers (modal, style
//          Windows : arborescence a fleches d'expansion, disques/raccourcis,
//          creation/renommage/suppression de dossier, champ chemin, barres de
//          defilement, fenetre deplacable). INDEPENDANT de toute application :
//          ne contient QUE l'etat + la navigation filesystem (NKFileSystem).
//
//          Le RENDU et les ACTIONS de confirmation (charger / enregistrer /
//          scaffolding) sont la SPECIALISATION de l'application (ex. NKCode
//          derive de NkFilePickerState et dessine son propre DrawFolderPicker).
// @Author  Rihen
// @License Proprietary - Free to use and modify
// -----------------------------------------------------------------------------
#include "NKGui/NKGui.h"				  // rendu : NkGuiContext / NkGuiDrawList / NkGuiFont / NkGuiKey
#include "NKEditorKit/NkEditorTextField.h" // NkOverlayTextField (champ mono-ligne moteur)
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkPath.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKPlatform/NkEnv.h" // env::GetEnvVar (variables d'environnement maison)

namespace nkentseu {
	namespace editorkit {

		using namespace nkentseu;
		using namespace nkentseu::nkgui;

		// ── STYLE du picker : TOUTES les couleurs sont ici -> personnalisation =
		//    uniquement design/couleurs (l'app remplit et passe ce struct au rendu). ──
		struct NkFilePickerStyle {
				NkColor backdrop = {0, 0, 0, 160};	  // voile modal
				NkColor card = {22, 24, 29, 255};	  // fond de la fenetre
				NkColor border = {50, 55, 63, 255};	  // liseres
				NkColor accent = {15, 115, 213, 255}; // liseré titre / bouton principal
				NkColor confirmHover = {41, 133, 224, 255};
				NkColor text = {236, 237, 239, 255};
				NkColor textStrong = {255, 255, 255, 255};
				NkColor sub = {150, 156, 164, 255};
				NkColor rowHover = {33, 38, 46, 255};
				NkColor selection = {15, 115, 213, 90};
				NkColor treeBg = {16, 18, 22, 255};
				NkColor btn = {30, 34, 40, 255}; // bouton secondaire (repos)
				NkColor btnHover = {40, 46, 54, 255};
				NkColor folderIcon = {247, 154, 40, 220};
				NkColor fileIcon = {120, 130, 145, 230};
				NkColor menuBg = {26, 30, 37, 255};
				NkColor danger = {248, 81, 73, 255}; // « Supprimer »
				NkColor scrollTrack = {255, 255, 255, 16};
				NkColor scrollThumb = {80, 88, 98, 255};
				NkColor scrollThumbHover = {120, 130, 142, 255};
		};

		// ── Base REUTILISABLE du picker : etat + navigation (sans rendu, sans app) ──
		struct NkFilePickerState {
				// Modes generiques. PK_None = ferme. PK_Open/PK_PickFolder = choisir un
				// dossier ; PK_File = choisir un fichier ; PK_SaveFile = enregistrer.
				// PK_NewDir/PK_LoadDir/PK_Buf = usages applicatifs (routes a la confirmation
				// par l'app) ; l'enum vit ici pour rester partageable (les enumerateurs sont
				// accessibles via la classe derivee, ex. NkCodeDialogs::PK_SaveFile).
				enum PickFor { PK_None = 0, PK_Open, PK_NewDir, PK_LoadDir, PK_Buf, PK_File, PK_SaveFile, PK_PickFolder };

				char pickerSaveName[256] = {}; // mode PK_SaveFile : nom du fichier (avec extension)
				bool pickerSaveFocus = false;
				NkString pickerConfine; // si non-vide : parcours LIMITE a ce dossier + sous-dossiers

				bool PickerAllowed(const char *p) const {
					return pickerConfine.Empty() || PathIsAncestor(pickerConfine.CStr(), p);
				}

				NkVector<NkString> pickerFiles; // fichiers du dossier selectionne (mode PK_File)
				int32 pickerFileSel = -1;		// fichier choisi (index dans pickerFiles)
				bool pickerOpen = false;
				int32 pickerFor = PK_None;
				char pickerPath[512] = {};
				char *pickerBuf = nullptr;
				int32 pickerBufCap = 0;
				float32 pickerScroll = 0.f;	 // defilement vertical
				float32 pickerScrollX = 0.f; // defilement horizontal
				bool pickerEditing = false;	 // edition du champ chemin
				char pickerNew[128] = {};	 // nom du dossier a creer
				bool pickerNewFocus = false;
				int32 pickerDrag = 0;		 // 0 aucun, 1 thumb V, 2 thumb H
				float32 pickerDragOff = 0.f; // offset souris/thumb pendant le drag
				// Fenetre DEPLACABLE (barre de titre) : decalage vs position centree.
				float32 pickerWinOffX = 0.f, pickerWinOffY = 0.f;
				bool pickerWinDrag = false;
				float32 pickerWinDragX = 0.f, pickerWinDragY = 0.f;
				NkVector<NkString> pickerDirs; // (legacy) sous-dossiers du chemin courant

				// ── Arborescence du picker (style Windows : fleches d'expansion) ──
				struct PickNode {
						NkString path, name;
						int32 depth = 0;
						bool hasKids = false;
						bool open = false;
				};

				NkVector<PickNode> pickerTree;
				int32 pickerSel = -1; // ligne selectionnee (= dossier choisi)
				// clic-droit : menu contextuel + renommage inline
				int32 pickMenu = -1;
				float32 pickMenuX = 0.f, pickMenuY = 0.f;
				int32 pickRename = -1;
				char pickRenameBuf[256] = {};
				bool pickRenameFocus = false;

				// ── RESULTAT de confirmation (a consommer par l'app apres le rendu) ──
				bool pickerConfirmed = false; // l'utilisateur a valide (chemin choisi)
				bool pickerCancelled = false; // l'utilisateur a annule (bouton Annuler)
				int32 pickerResultFor = PK_None;
				char pickerResultPath[512] = {};
				char pickerResultName[256] = {}; // nom saisi (mode enregistrer)

				// ── Points de SPECIALISATION (surcharges par la classe derivee applicative) :
				//    hauteurs, contenu supplementaire (ex. assistant de scaffolding), libelle
				//    et etat du bouton de validation. Defauts = picker generique. ──
				virtual ~NkFilePickerState() = default;
				virtual float32 PickerWindowHeight(float32 S) const { return (pickerFor == PK_SaveFile ? 548.f : 500.f) * S; }
				virtual float32 PickerBottomReserve(float32 S) const { return (pickerFor == PK_SaveFile ? 140.f : 96.f) * S; }
				virtual float32 PickerExtraHeight(float32 /*S*/) const { return 0.f; } // region app sous l'arbre
				virtual void DrawPickerExtra(NkGuiContext & /*ctx*/, NkGuiDrawList & /*dl*/, const NkGuiFont * /*f*/,
											 const NkRect & /*region*/, const NkFilePickerStyle & /*sty*/, bool /*click*/,
											 bool & /*fieldClicked*/) {}
				virtual bool PickerConfirmEnabled() const {
					if (pickerFor == PK_File)
						return pickerFileSel >= 0 && pickerFileSel < (int32)pickerFiles.Size();
					if (pickerFor == PK_SaveFile)
						return pickerSaveName[0] != '\0';
					return true;
				}
				virtual const char *PickerConfirmLabel() const {
					if (pickerFor == PK_SaveFile)
						return "Enregistrer ici";
					if (pickerFor == PK_File)
						return "Selectionner ce fichier";
					return "Selectionner ce dossier";
				}
				virtual void PickerClearExtraFocus() {} // l'app y remet le focus de SES champs a 0

				// ── Comparaison de chemins : `dir` est-il ancetre (ou egal) de `file` ? ──
				static bool PathIsAncestor(const char *dir, const char *file) {
					int32 dl = 0;
					while (dir[dl])
						++dl;
					int32 fl = 0;
					while (file[fl])
						++fl;
					while (dl > 0 && (dir[dl - 1] == '/' || dir[dl - 1] == '\\'))
						--dl; // ignore un separateur final sur `dir`
					if (dl == 0 || dl > fl)
						return false;
					for (int32 k = 0; k < dl; ++k) {
						char a = dir[k], b = file[k];
						if (a == '\\')
							a = '/';
						if (b == '\\')
							b = '/';
						if (a >= 'A' && a <= 'Z')
							a += 32;
						if (b >= 'A' && b <= 'Z')
							b += 32;
						if (a != b)
							return false;
					}
					return fl == dl || file[dl] == '/' || file[dl] == '\\'; // frontiere de dossier
				}

				static void CopyTo(char *dst, const char *src, int32 cap) {
					int32 i = 0;
					if (src)
						for (; src[i] && i + 1 < cap; ++i)
							dst[i] = src[i];
					dst[i] = '\0';
				}

				void PickBeginRename(int32 i) {
					if (i < 0 || i >= (int32)pickerTree.Size() || pickerTree[i].depth == 0)
						return; // pas les racines
					pickRename = i;
					CopyTo(pickRenameBuf, pickerTree[i].name.CStr(), (int32)sizeof(pickRenameBuf));
					pickRenameFocus = true;
					pickMenu = -1;
				}

				void PickRenameCommit() {
					if (pickRename < 0 || pickRename >= (int32)pickerTree.Size()) {
						pickRename = -1;
						return;
					}
					PickNode &n = pickerTree[pickRename];
					if (pickRenameBuf[0]) {
						const NkString parent = NkPath(n.path.CStr()).GetParent().ToString();
						const NkString dst = (NkPath(parent.CStr()) / pickRenameBuf).ToString();
						if (NkDirectory::Move(n.path.CStr(), dst.CStr())) {
							const bool wasOpen = n.open;
							const int32 idx = pickRename;
							if (wasOpen)
								TogglePickerNode(idx); // replie (chemins enfants perimes)
							pickerTree[idx].path = dst;
							pickerTree[idx].name = pickRenameBuf;
						}
					}
					pickRename = -1;
				}

				void PickDelete(int32 i) {
					if (i < 0 || i >= (int32)pickerTree.Size() || pickerTree[i].depth == 0)
						return; // pas les racines
					if (NkDirectory::MoveToTrash(pickerTree[i].path.CStr())) {
						const int32 d0 = pickerTree[i].depth;
						while (i + 1 < (int32)pickerTree.Size() && pickerTree[i + 1].depth > d0)
							pickerTree.Erase(pickerTree.Begin() + i + 1);
						pickerTree.Erase(pickerTree.Begin() + i);
						if (pickerSel == i)
							pickerSel = -1;
					}
					pickMenu = -1;
				}

				static bool DirHasSubdirs(const char *path) {
					NkVector<NkDirectoryEntry> e =
						NkDirectory::GetEntries(NkPath(path), "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
					for (usize i = 0; i < e.Size(); ++i)
						if (e[i].IsDirectory && !e[i].IsHidden && e[i].Name.CStr()[0] != '.')
							return true;
					return false;
				}

				void AddPickRoot(const char *path, const char *label) {
					if (!path || !*path || !NkDirectory::Exists(path))
						return;
					PickNode n;
					n.path = path;
					n.name = label;
					n.hasKids = DirHasSubdirs(path);
					pickerTree.PushBack(n);
				}

				void BuildPickerTree() {
					pickerTree.Clear();
					pickerSel = -1;
					pickerScroll = 0.f;
					if (!pickerConfine.Empty()) { // parcours LIMITE : un seul noeud racine (le dossier de reference)
						NkString nm = NkPath(pickerConfine).GetFileName();
						if (nm.Empty())
							nm = pickerConfine;
						AddPickRoot(pickerConfine.CStr(), nm.CStr());
						return;
					}
					// ── Raccourcis (facon Windows) : Accueil / Bureau / Documents / Favoris ──
					const char *home = env::GetEnvVar("USERPROFILE"); // API maison (NkEnv.h)
					if (!home || !*home)
						home = env::GetEnvVar("HOME");
					auto known = [&](const char *sub) -> NkString {
						const char *od = env::GetEnvVar("OneDrive");
						if (od && *od) {
							NkString p = (NkPath(od) / sub).ToString();
							if (NkDirectory::Exists(p.CStr()))
								return p;
						}
						return home ? (NkPath(home) / sub).ToString() : NkString();
					};
					if (home && *home)
						AddPickRoot(home, "Accueil");
					{
						const NkString d = known("Desktop");
						if (!d.Empty())
							AddPickRoot(d.CStr(), "Bureau");
					}
					{
						const NkString d = known("Documents");
						if (!d.Empty())
							AddPickRoot(d.CStr(), "Documents");
					}
					// Favoris (~/.nkcode/favorites.txt : « chemin<TAB>alias » par ligne)
					if (home && *home) {
						const NkString favF = (NkPath(home) / ".nkcode" / "favorites.txt").ToString();
						if (NkFile::Exists(favF.CStr())) {
							const NkString txt = NkFile::ReadAllText(NkPath(favF.CStr()));
							NkString line;
							for (const char *p = txt.CStr();; ++p) {
								if (*p == '\n' || *p == '\0') {
									if (!line.Empty()) {
										const char *s = line.CStr();
										int32 tab = -1;
										for (int32 k = 0; s[k]; ++k)
											if (s[k] == '\t') {
												tab = k;
												break;
											}
										NkString path, alias;
										if (tab >= 0) {
											for (int32 k = 0; k < tab; ++k)
												path += s[k];
											for (int32 k = tab + 1; s[k]; ++k)
												alias += s[k];
										} else
											path = line;
										if (!path.Empty())
											AddPickRoot(path.CStr(), alias.Empty() ? path.CStr() : alias.CStr());
									}
									line.Clear();
									if (*p == '\0')
										break;
								} else if (*p != '\r')
									line += *p;
							}
						}
					}
// ── Disques ──
#ifdef _WIN32
					for (char c = 'C'; c <= 'Z'; ++c) {
						const char root[4] = {c, ':', '/', 0};
						if (NkDirectory::Exists(root)) {
							PickNode n;
							n.path = root;
							n.name = root;
							n.hasKids = DirHasSubdirs(root);
							pickerTree.PushBack(n);
						}
					}
#else
					AddPickRoot("/", "/");
#endif
				}

				void TogglePickerNode(int32 i) {
					if (i < 0 || i >= (int32)pickerTree.Size() || !pickerTree[i].hasKids)
						return;
					const int32 d0 = pickerTree[i].depth;
					if (pickerTree[i].open) { // replier : retire les descendants
						pickerTree[i].open = false;
						while (i + 1 < (int32)pickerTree.Size() && pickerTree[i + 1].depth > d0)
							pickerTree.Erase(pickerTree.Begin() + i + 1);
					} else { // deplier : insere les sous-dossiers
						pickerTree[i].open = true;
						const NkString base = pickerTree[i].path;
						NkVector<NkDirectoryEntry> e =
							NkDirectory::GetEntries(NkPath(base), "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
						int32 ins = i + 1;
						for (usize k = 0; k < e.Size(); ++k)
							if (e[k].IsDirectory && !e[k].IsHidden && e[k].Name.CStr()[0] != '.') {
								PickNode cn;
								cn.path = (NkPath(base.CStr()) / e[k].Name.CStr()).ToString();
								cn.name = e[k].Name;
								cn.depth = d0 + 1;
								cn.hasKids = DirHasSubdirs(cn.path.CStr());
								pickerTree.Insert(pickerTree.Begin() + ins, cn);
								++ins;
							}
					}
				}

				void PickerCreateFolder() {
					if (!pickerNew[0])
						return;
					// Cree sous le dossier SELECTIONNE (sinon le chemin courant), sans reconstruire tout l'arbre.
					const NkString parent = (pickerSel >= 0 && pickerSel < (int32)pickerTree.Size())
												? pickerTree[pickerSel].path
												: NkString(pickerPath);
					if (NkDirectory::CreateRecursive(NkPath(parent.CStr()) / pickerNew)) {
						pickerNew[0] = '\0';
						if (pickerSel >= 0 && pickerSel < (int32)pickerTree.Size()) {
							pickerTree[pickerSel].hasKids = true;
							if (pickerTree[pickerSel].open) {
								TogglePickerNode(pickerSel);
								TogglePickerNode(pickerSel);
							} // replie+deplie = rafraichit
							else
								TogglePickerNode(pickerSel); // deplie pour montrer le nouveau
						}
					}
				}

				// ── Ouverture GENERIQUE : construit l'arbre a partir de `startDir`. L'app
				//    (OpenPicker) fixe le `purpose`, le buffer cible et la zone `confine`. ──
				void OpenPickerBase(int32 purpose, const char *startDir, char *buf, int32 cap, const char *confine) {
					pickerOpen = true;
					pickerFor = purpose;
					pickerBuf = buf;
					pickerBufCap = cap;
					pickerScroll = 0.f;
					pickerWinOffX = pickerWinOffY = 0.f; // recentre a chaque ouverture
					pickerWinDrag = false;
					pickerEditing = false;
					pickerConfine = (confine && *confine) ? NkString(confine) : NkString(); // parcours limite (opt-in)
					const char *start = (startDir && *startDir) ? startDir : ".";
					CopyTo(pickerPath, start, (int32)sizeof(pickerPath));
					if (!PickerAllowed(pickerPath))
						CopyTo(pickerPath, pickerConfine.CStr(), (int32)sizeof(pickerPath)); // depart dans la zone autorisee
					BuildPickerTree();
					if (purpose == PK_File)
						ScanPickerFiles(pickerPath);
				}

				void ScanPicker() {
					pickerDirs.Clear();
					NkVector<NkDirectoryEntry> e =
						NkDirectory::GetEntries(NkPath(pickerPath), "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
					for (usize i = 0; i < e.Size(); ++i)
						if (e[i].IsDirectory && e[i].Name.CStr()[0] != '.')
							pickerDirs.PushBack(e[i].Name);
				}

				void PickerGoto(const NkPath &p) {
					CopyTo(pickerPath, p.ToString().CStr(), (int32)sizeof(pickerPath));
					pickerScroll = 0.f;
					ScanPicker();
				}

				void PickerUp() { PickerGoto(NkPath(pickerPath).GetParent()); }

				void PickerEnter(const char *sub) { PickerGoto(NkPath(pickerPath) / sub); }

				void PickerCancel() {
					pickerOpen = false;
					pickerFor = PK_None;
					pickerBuf = nullptr;
					pickerFileSel = -1;
					pickerFiles.Clear();
				}

				// Liste les FICHIERS (non-dossiers) du repertoire courant (mode PK_File).
				void ScanPickerFiles(const char *dir) {
					pickerFiles.Clear();
					pickerFileSel = -1;
					if (!dir || !dir[0])
						return;
					NkVector<NkDirectoryEntry> e =
						NkDirectory::GetEntries(NkPath(dir), "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
					for (usize i = 0; i < e.Size(); ++i)
						if (!e[i].IsDirectory && e[i].Name.CStr()[0] != '.')
							pickerFiles.PushBack(e[i].Name);
				}
		};

		// ── RENDU REUTILISABLE du picker (modal centre, fenetre deplacable). Toutes les
		//    couleurs viennent de `sty`. La confirmation depose un RESULTAT dans `fp`
		//    (fp.pickerConfirmed/pickerCancelled + pickerResult*) que l'app consomme. Les
		//    specificites app (assistant scaffolding, hauteurs) passent par les virtuals. ──
		inline void NkDrawFilePicker(NkGuiContext &ctx, NkFilePickerState &fp, const NkFilePickerStyle &sty) {
			const NkGuiFont *f = ctx.font;
			if (!f || !f->Valid())
				return;
			auto &dl = ctx.dlOverlay;
			const float32 W = (float32)ctx.viewW, H = (float32)ctx.viewH, S = ctx.S(1.f);
			const float32 asc = f->Ascent(), lh = f->LineHeight();
			const NkVec2 mp = ctx.input.mousePos;
			const bool click = ctx.input.mouseClicked[0];
			const bool rclick = ctx.input.mouseClicked[1];
			auto hit = [&](const NkRect &r) { return NkGuiRectContains(r, mp); };
			auto text = [&](float32 x, float32 y, const char *s, const NkColor &c) {
				dl.AddText(f->Face(), f->TexId(), {x, y + asc}, s, c);
			};
			auto sbtn = [&](const NkRect &r, const char *s) -> bool {
				const bool hov = hit(r);
				dl.AddRectFilled(r, hov ? sty.btnHover : sty.btn, 6.f * S);
				dl.AddRect(r, sty.border, 1.f);
				const float32 tw = f->MeasureWidth(s);
				text(r.x + (r.w - tw) * 0.5f, r.y + (r.h - lh) * 0.5f, s, sty.text);
				return hov && click;
			};
			auto pbtn = [&](const NkRect &r, const char *s, bool en) -> bool {
				const bool hov = en && hit(r);
				dl.AddRectFilled(r, !en ? sty.btn : hov ? sty.confirmHover : sty.accent, 6.f * S);
				const float32 tw = f->MeasureWidth(s);
				text(r.x + (r.w - tw) * 0.5f, r.y + (r.h - lh) * 0.5f, s, en ? sty.textStrong : sty.sub);
				return en && hov && click;
			};

			const bool saveMode = (fp.pickerFor == NkFilePickerState::PK_SaveFile);
			const bool fileMode = (fp.pickerFor == NkFilePickerState::PK_File);
			const bool pickFolderMode = (fp.pickerFor == NkFilePickerState::PK_PickFolder);
			// Contenu SUPPLEMENTAIRE demande par l'app (ex. assistant de creation NKCode).
			const float32 extraH = fp.PickerExtraHeight(S);
			const bool hasExtra = extraH > 0.f;
			const float32 pw = 580.f * S, ph = fp.PickerWindowHeight(S);
			const float32 px = (W - pw) * 0.5f + fp.pickerWinOffX, py = (H - ph) * 0.5f + fp.pickerWinOffY;
			const bool down = ctx.input.mouseDown[0];
			bool fieldClicked = false; // un champ de saisie a-t-il ete clique cette frame ?
			dl.AddRectFilled({0.f, 0.f, W, H}, sty.backdrop);
			dl.AddRectFilled({px, py, pw, ph}, sty.card, 10.f * S);
			dl.AddRect({px, py, pw, ph}, sty.border, 1.5f);
			// ── BARRE DE TITRE deplacable (lisere accent + drag) ──
			const float32 tbH = 40.f * S;
			dl.AddRectFilled({px, py, pw, 3.f * S}, sty.accent, 10.f * S);
			text(px + 20.f * S, py + 16.f * S,
				 saveMode		 ? "Enregistrer le fichier - choisir le dossier"
				 : fileMode		 ? "Choisir un fichier (executable)"
				 : pickFolderMode ? "Ajouter un dossier au workspace"
								  : "Choisir un dossier",
				 sty.text);
			{
				const NkRect titleBar = {px, py, pw - 44.f * S, tbH}; // hors bouton ✕ (a droite)
				if (click && hit(titleBar)) {
					fp.pickerWinDrag = true;
					fp.pickerWinDragX = mp.x - px;
					fp.pickerWinDragY = mp.y - py;
				}
				if (fp.pickerWinDrag && down) {
					fp.pickerWinOffX = (mp.x - fp.pickerWinDragX) - (W - pw) * 0.5f;
					fp.pickerWinOffY = (mp.y - fp.pickerWinDragY) - (H - ph) * 0.5f;
				}
				if (!down)
					fp.pickerWinDrag = false;
			}

			// Champ chemin (selection courante) + Aller (saisie libre). PAS de « Remonter » -> arbre.
			const float32 cx = px + 20.f * S, cwid = pw - 40.f * S;
			float32 y = py + 50.f * S;
			{
				const NkRect r = {cx, y, cwid - 96.f * S, 30.f * S};
				NkOverlayTextField(ctx, dl, f, r, fp.pickerPath, (int32)sizeof(fp.pickerPath), fp.pickerEditing);
				if (hit(r) && click) {
					fp.pickerEditing = true;
					fp.pickerNewFocus = false;
					fp.pickerSaveFocus = false;
					fp.PickerClearExtraFocus();
					fieldClicked = true;
				}
				if (sbtn({cx + cwid - 84.f * S, y, 84.f * S, 30.f * S}, "Aller")) {
					if (!fp.PickerAllowed(fp.pickerPath))
						NkFilePickerState::CopyTo(fp.pickerPath, fp.pickerConfine.CStr(), (int32)sizeof(fp.pickerPath));
					fp.pickerSel = -1;
					fp.pickerEditing = false;
					if (fileMode)
						fp.ScanPickerFiles(fp.pickerPath);
				}
			}
			y += 42.f * S;

			// Arborescence des dossiers : fleche d'expansion sur les dossiers NON VIDES, clic = selectionner.
			const float32 barW = 10.f * S;
			const NkRect area = {cx, y, cwid, ph - (y - py) - fp.PickerBottomReserve(S)};
			const NkRect inner = {area.x, area.y, area.w - barW, area.h - barW}; // zone hors barres
			dl.AddRectFilled(area, sty.treeBg, 6.f * S);
			dl.AddRect(area, sty.border, 1.f);
			if (hit(inner) && ctx.input.wheel != 0.f) {
				fp.pickerScroll -= ctx.input.wheel * 34.f;
				ctx.input.wheel = 0.f;
			}
			const float32 rowH = 28.f * S, rowStep = 30.f * S, indent = 16.f * S;
			float32 contentW = 0.f;
			for (usize i = 0; i < fp.pickerTree.Size(); ++i) {
				const float32 w =
					fp.pickerTree[i].depth * indent + f->MeasureWidth(fp.pickerTree[i].name.CStr()) + 60.f * S;
				if (w > contentW)
					contentW = w;
			}
			// UN SEUL contentH/maxS/maxX (sinon pre-clamp et clamp final divergent -> clignotement bas).
			const int32 totalRows = (int32)fp.pickerTree.Size() + (fileMode ? (int32)fp.pickerFiles.Size() + 1 : 0);
			const float32 contentH = totalRows * rowStep + 12.f * S;
			const float32 maxS = contentH - inner.h > 0.f ? contentH - inner.h : 0.f;
			const float32 maxX = contentW - inner.w > 0.f ? contentW - inner.w : 0.f;
			// Clamp AVANT de dessiner (sinon overshoot d'1 frame aux limites = clignotement).
			if (fp.pickerScroll < 0.f)
				fp.pickerScroll = 0.f;
			if (fp.pickerScroll > maxS)
				fp.pickerScroll = maxS;
			if (fp.pickerScrollX < 0.f)
				fp.pickerScrollX = 0.f;
			if (fp.pickerScrollX > maxX)
				fp.pickerScrollX = maxX;
			const bool menuOpen = (fp.pickMenu >= 0); // un menu contextuel ouvert capture les clics
			dl.PushClipRect(inner, true);
			int32 doToggle = -1, doSelect = -1;
			float32 ly = inner.y + 6.f * S - fp.pickerScroll;
			for (usize i = 0; i < fp.pickerTree.Size(); ++i) {
				const auto &n = fp.pickerTree[i];
				const float32 rowW = (contentW > inner.w ? contentW : inner.w) - 8.f * S;
				const NkRect r = {inner.x + 4.f * S - fp.pickerScrollX, ly, rowW, rowH};
				if (ly + rowH > inner.y && ly < inner.y + inner.h) {
					const bool sel = ((int32)i == fp.pickerSel);
					const bool hov = NkGuiRectContains(r, mp) && hit(inner);
					if (sel)
						dl.AddRectFilled(r, sty.selection, 5.f * S);
					else if (hov)
						dl.AddRectFilled(r, sty.rowHover, 5.f * S);
					const float32 ix = r.x + 8.f * S + n.depth * indent;
					if (n.hasKids) { // chevron ► (replie) / ▼ (deplie) — NEUTRE (pas bleu)
						const float32 ax = ix, ay = r.y + rowH * 0.5f, s = 4.f * S;
						const NkColor ac = n.open ? sty.text : sty.sub;
						if (n.open) {
							dl.AddLine({ax - s, ay - s * 0.5f}, {ax, ay + s * 0.6f}, ac, 1.6f * S);
							dl.AddLine({ax + s, ay - s * 0.5f}, {ax, ay + s * 0.6f}, ac, 1.6f * S);
						} else {
							dl.AddLine({ax - s * 0.5f, ay - s}, {ax + s * 0.6f, ay}, ac, 1.6f * S);
							dl.AddLine({ax - s * 0.5f, ay + s}, {ax + s * 0.6f, ay}, ac, 1.6f * S);
						}
					}
					const float32 fx = ix + 14.f * S; // icone dossier
					dl.AddRectFilled({fx, r.y + 9.f * S, 16.f * S, 11.f * S}, sty.folderIcon, 2.f * S);
					dl.AddRectFilled({fx, r.y + 7.f * S, 8.f * S, 4.f * S}, sty.folderIcon, 1.f * S);
					if (fp.pickRename == (int32)i) { // renommage inline
						const NkRect fr = {fx + 22.f * S, r.y + 2.f * S, rowW - (fx + 22.f * S - r.x) - 8.f * S,
										   rowH - 4.f * S};
						NkOverlayTextField(ctx, dl, f, fr, fp.pickRenameBuf, (int32)sizeof(fp.pickRenameBuf),
										   fp.pickRenameFocus);
						if (ctx.input.KeyPressed(NkGuiKey::Enter))
							fp.PickRenameCommit();
						if (ctx.input.KeyPressed(NkGuiKey::Escape))
							fp.pickRename = -1;
					} else {
						text(fx + 22.f * S, r.y + (rowH - lh) * 0.5f, n.name.CStr(), sty.text);
						if (hov && click && !menuOpen) {
							const NkRect arrowR = {ix - 8.f * S, r.y, 18.f * S, rowH};
							if (n.hasKids && NkGuiRectContains(arrowR, mp))
								doToggle = (int32)i;
							else
								doSelect = (int32)i;
						}
						if (hov && rclick && !menuOpen) {
							fp.pickMenu = (int32)i;
							fp.pickMenuX = mp.x;
							fp.pickMenuY = mp.y;
							fp.pickerSel = (int32)i;
						}
					}
				}
				ly += rowStep;
			}
			// Fichiers du dossier selectionne (mode PK_File) — listes a la suite de l'arbre.
			int32 doFile = -1;
			if (fileMode) {
				const float32 rowW = (contentW > inner.w ? contentW : inner.w) - 8.f * S;
				if (ly + rowH > inner.y && ly < inner.y + inner.h)
					text(inner.x + 8.f * S - fp.pickerScrollX, ly + (rowH - lh) * 0.5f,
						 "Fichiers du dossier selectionne :", sty.sub);
				ly += rowStep;
				for (usize i = 0; i < fp.pickerFiles.Size(); ++i) {
					const NkRect r = {inner.x + 4.f * S - fp.pickerScrollX, ly, rowW, rowH};
					if (ly + rowH > inner.y && ly < inner.y + inner.h) {
						const bool sel = ((int32)i == fp.pickerFileSel);
						const bool hov = NkGuiRectContains(r, mp) && hit(inner);
						if (sel)
							dl.AddRectFilled(r, sty.selection, 5.f * S);
						else if (hov)
							dl.AddRectFilled(r, sty.rowHover, 5.f * S);
						const float32 fx = r.x + 8.f * S + indent;
						dl.AddRectFilled({fx, r.y + 6.f * S, 13.f * S, 15.f * S}, sty.fileIcon, 2.f * S);
						text(fx + 20.f * S, r.y + (rowH - lh) * 0.5f, fp.pickerFiles[i].CStr(), sty.text);
						if (hov && click && !menuOpen)
							doFile = (int32)i;
					}
					ly += rowStep;
				}
				if (fp.pickerFiles.Empty() && ly < inner.y + inner.h)
					text(inner.x + 16.f * S - fp.pickerScrollX, ly - rowStep + (rowH - lh) * 0.5f + rowStep,
						 "(aucun fichier)", sty.sub);
			}
			dl.PopClipRect();
			if (fp.pickerTree.Empty())
				text(inner.x + 12.f * S, inner.y + 10.f * S, "(aucun lecteur)", sty.sub);
			if (doSelect >= 0) {
				fp.pickerSel = doSelect;
				NkFilePickerState::CopyTo(fp.pickerPath, fp.pickerTree[doSelect].path.CStr(),
										  (int32)sizeof(fp.pickerPath));
				fp.pickerEditing = false;
				if (fileMode)
					fp.ScanPickerFiles(fp.pickerPath);
			} else if (doToggle >= 0)
				fp.TogglePickerNode(doToggle);
			if (doFile >= 0)
				fp.pickerFileSel = doFile;
			// gestion du drag des thumbs
			if (!down)
				fp.pickerDrag = 0;
			// barre V (toujours visible)
			{
				const NkRect track = {area.x + area.w - barW, inner.y, barW, inner.h};
				dl.AddRectFilled(track, sty.scrollTrack, 3.f * S);
				const float32 th = maxS > 0.f ? inner.h * (inner.h / contentH) : inner.h;
				const float32 tt = inner.y + (maxS > 0.f ? (inner.h - th) * (fp.pickerScroll / maxS) : 0.f);
				const NkRect thumb = {track.x + 2.f * S, tt, barW - 4.f * S, th};
				if (click && hit(thumb)) {
					fp.pickerDrag = 1;
					fp.pickerDragOff = mp.y - tt;
				}
				if (fp.pickerDrag == 1 && maxS > 0.f)
					fp.pickerScroll = ((mp.y - fp.pickerDragOff - inner.y) / (inner.h - th)) * maxS;
				dl.AddRectFilled(thumb, fp.pickerDrag == 1 ? sty.accent : (hit(thumb) ? sty.scrollThumbHover : sty.scrollThumb),
								 3.f * S);
			}
			// barre H (toujours visible)
			{
				const NkRect track = {inner.x, area.y + area.h - barW, inner.w, barW};
				dl.AddRectFilled(track, sty.scrollTrack, 3.f * S);
				const float32 tw = maxX > 0.f ? inner.w * (inner.w / contentW) : inner.w;
				const float32 tt = inner.x + (maxX > 0.f ? (inner.w - tw) * (fp.pickerScrollX / maxX) : 0.f);
				const NkRect thumb = {tt, track.y + 2.f * S, tw, barW - 4.f * S};
				if (click && hit(thumb)) {
					fp.pickerDrag = 2;
					fp.pickerDragOff = mp.x - tt;
				}
				if (fp.pickerDrag == 2 && maxX > 0.f)
					fp.pickerScrollX = ((mp.x - fp.pickerDragOff - inner.x) / (inner.w - tw)) * maxX;
				dl.AddRectFilled(thumb, fp.pickerDrag == 2 ? sty.accent : (hit(thumb) ? sty.scrollThumbHover : sty.scrollThumb),
								 3.f * S);
			}
			if (fp.pickerScroll < 0.f)
				fp.pickerScroll = 0.f;
			if (fp.pickerScroll > maxS)
				fp.pickerScroll = maxS;
			if (fp.pickerScrollX < 0.f)
				fp.pickerScrollX = 0.f;
			if (fp.pickerScrollX > maxX)
				fp.pickerScrollX = maxX;

			// Ligne creation de dossier : champ + bouton
			const float32 ny = area.y + area.h + 8.f * S;
			{
				const NkRect r = {cx, ny, cwid - 150.f * S, 30.f * S};
				NkOverlayTextField(ctx, dl, f, r, fp.pickerNew, (int32)sizeof(fp.pickerNew), fp.pickerNewFocus);
				if (hit(r) && click) {
					fp.pickerNewFocus = true;
					fp.pickerEditing = false;
					fp.pickerSaveFocus = false;
					fp.PickerClearExtraFocus();
					fieldClicked = true;
				}
				if (fp.pickerNew[0] == '\0' && !fp.pickerNewFocus)
					text(r.x + 10.f * S, r.y + (30.f * S - lh) * 0.5f, "nom du nouveau dossier", sty.sub);
				if (sbtn({cx + cwid - 140.f * S, ny, 140.f * S, 30.f * S}, "+ Creer dossier"))
					fp.PickerCreateFolder();
			}
			// Mode ENREGISTRER (sans contenu app) : simple champ « nom du fichier (avec extension) ».
			if (saveMode && !hasExtra) {
				const float32 fy = ny + 56.f * S;
				text(cx, fy - 18.f * S, "Nom du fichier (avec extension)", sty.sub);
				const NkRect r = {cx, fy, cwid, 30.f * S};
				NkOverlayTextField(ctx, dl, f, r, fp.pickerSaveName, (int32)sizeof(fp.pickerSaveName), fp.pickerSaveFocus);
				if (hit(r) && click) {
					fp.pickerSaveFocus = true;
					fp.pickerEditing = false;
					fp.pickerNewFocus = false;
					fp.PickerClearExtraFocus();
					fieldClicked = true;
				}
				if (fp.pickerSaveName[0] == '\0' && !fp.pickerSaveFocus)
					text(r.x + 10.f * S, r.y + (30.f * S - lh) * 0.5f, "ex: MonFichier.cpp", sty.sub);
			}
			// Contenu SUPPLEMENTAIRE de l'app (ex. assistant de scaffolding NKCode).
			if (hasExtra) {
				const NkRect region = {cx, ny, cwid, extraH};
				fp.DrawPickerExtra(ctx, dl, f, region, sty, click, fieldClicked);
			}
			// Clic AILLEURS (arbre, vide, boutons) -> desactive la saisie des champs.
			if (click && !fieldClicked) {
				fp.pickerEditing = false;
				fp.pickerNewFocus = false;
				fp.pickerSaveFocus = false;
				fp.PickerClearExtraFocus();
			}

			// Boutons bas (geles si un menu contextuel est ouvert). Confirmation UNIFIEE :
			// depose un RESULTAT ; l'app route l'action (charger/enregistrer/scaffold/…).
			const float32 by = py + ph - 44.f * S;
			if (!menuOpen &&
				pbtn({px + pw - 200.f * S, by, 180.f * S, 32.f * S}, fp.PickerConfirmLabel(), fp.PickerConfirmEnabled())) {
				NkString chosen(fp.pickerPath);
				if (fileMode && fp.pickerFileSel >= 0 && fp.pickerFileSel < (int32)fp.pickerFiles.Size())
					chosen = (NkPath(fp.pickerPath) / fp.pickerFiles[fp.pickerFileSel].CStr()).ToString();
				if ((fp.pickerFor == NkFilePickerState::PK_Buf || fileMode) && fp.pickerBuf)
					NkFilePickerState::CopyTo(fp.pickerBuf, chosen.CStr(), fp.pickerBufCap);
				fp.pickerResultFor = fp.pickerFor;
				NkFilePickerState::CopyTo(fp.pickerResultPath, chosen.CStr(), (int32)sizeof(fp.pickerResultPath));
				NkFilePickerState::CopyTo(fp.pickerResultName, fp.pickerSaveName, (int32)sizeof(fp.pickerResultName));
				fp.pickerConfirmed = true;
				fp.PickerCancel();
				return;
			}
			if (!menuOpen && sbtn({px + pw - 290.f * S, by, 80.f * S, 32.f * S}, "Annuler")) {
				fp.pickerResultFor = fp.pickerFor;
				fp.pickerCancelled = true;
				fp.PickerCancel();
				return;
			}

			// ── Menu contextuel (clic droit) : Nouveau dossier / Renommer / Supprimer ──
			if (fp.pickMenu >= 0 && fp.pickMenu < (int32)fp.pickerTree.Size()) {
				const bool isRoot = fp.pickerTree[fp.pickMenu].depth == 0;

				struct MI {
						const char *lab;
						int32 act;
						bool en;
				};

				const MI items[] = {{"Nouveau dossier", 0, true}, {"Renommer", 1, !isRoot}, {"Supprimer", 2, !isRoot}};
				const float32 mw = 180.f * S, ih = 28.f * S;
				float32 mx = fp.pickMenuX, my = fp.pickMenuY;
				if (mx + mw > W)
					mx = W - mw - 4.f * S;
				if (my + ih * 3 + 8.f * S > H)
					my = H - ih * 3 - 8.f * S;
				const NkRect menu = {mx, my, mw, ih * 3 + 8.f * S};
				dl.AddRectFilled(menu, sty.menuBg, 6.f * S);
				dl.AddRect(menu, sty.border, 1.f);
				int32 chosen = -1;
				for (int32 k = 0; k < 3; ++k) {
					const NkRect ir = {menu.x + 4.f * S, menu.y + 4.f * S + k * ih, mw - 8.f * S, ih};
					const bool hov = items[k].en && hit(ir);
					if (hov)
						dl.AddRectFilled(ir, sty.rowHover, 4.f * S);
					const NkColor tc = !items[k].en ? sty.sub : (items[k].act == 2 ? sty.danger : sty.text);
					text(ir.x + 12.f * S, ir.y + (ih - lh) * 0.5f, items[k].lab, tc);
					if (hov && click)
						chosen = items[k].act;
				}
				const int32 node = fp.pickMenu;
				if (chosen == 0) {
					fp.pickMenu = -1;
					fp.pickerSel = node;
					fp.pickerNewFocus = true;
					fp.pickerEditing = false;
				} else if (chosen == 1)
					fp.PickBeginRename(node);
				else if (chosen == 2)
					fp.PickDelete(node);
				else if (click && !hit(menu))
					fp.pickMenu = -1;
			}

			if (ctx.input.KeyPressed(NkGuiKey::Escape)) {
				if (fp.pickMenu >= 0)
					fp.pickMenu = -1;
				else if (fp.pickRename >= 0)
					fp.pickRename = -1;
				else {
					fp.PickerCancel();
					return;
				}
			}
		}

	} // namespace editorkit
} // namespace nkentseu
