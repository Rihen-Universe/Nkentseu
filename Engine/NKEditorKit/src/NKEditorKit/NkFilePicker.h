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
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkPath.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKPlatform/NkEnv.h" // env::GetEnvVar (variables d'environnement maison)

namespace nkentseu {
	namespace editorkit {

		using namespace nkentseu;

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

	} // namespace editorkit
} // namespace nkentseu
