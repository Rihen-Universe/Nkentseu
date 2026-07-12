#pragma once
// -----------------------------------------------------------------------------
// @File    NkDirBrowser.h
// @Brief   Coeur REUTILISABLE de navigation de dossiers pour un explorateur en
//          LISTE (curDir + historique arriere/avant + dossiers utilisateur). Base
//          du launcher NKCode (NkOpenWsState en derive et ajoute la couche .jenga :
//          detection workspace/projets, scan enrichi, rendu en colonnes). Ne
//          contient AUCUNE logique .jenga -> partageable par d'autres editeurs.
// @Author  Rihen
// @License Proprietary - Free to use and modify
// -----------------------------------------------------------------------------
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkPath.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKPlatform/NkEnv.h" // env::GetEnvVar

namespace nkentseu {
	namespace editorkit {

		using namespace nkentseu;

		// ── Base REUTILISABLE : dossier courant + historique + dossiers connus ──
		struct NkDirBrowserState {
				char curDir[600] = {};
				NkVector<NkString> history;
				int32 histPos = -1;
				bool scanned = false;   // l'app (re)scanne quand ce drapeau retombe a false
				int32 selected = -1;    // entree selectionnee (index dans la liste de l'app)
				float32 scroll = 0.f;   // defilement vertical de la liste
				NkString status;        // message d'operation (erreur)
				int32 renameIdx = -1;   // renommage inline en cours (-1 = aucun)
				int32 menuIdx = -1;     // menu contextuel ouvert (-1 = ferme)
				int32 confirmDel = -1;  // confirmation de suppression (-1 = aucune)

				static char Upc(char c) { return (c >= 'a' && c <= 'z') ? char(c - 'a' + 'A') : c; }

				static int32 CmpI(const char *a, const char *b) {
					for (; *a && *b; ++a, ++b) {
						char ca = Upc(*a), cb = Upc(*b);
						if (ca != cb)
							return ca < cb ? -1 : 1;
					}
					return *a ? 1 : (*b ? -1 : 0);
				}

				static bool SEq(const char *a, const char *b) {
					while (*a && *b) {
						if (*a != *b)
							return false;
						++a;
						++b;
					}
					return *a == *b;
				}

				static NkString Home() {
					const char *h = env::GetEnvVar("USERPROFILE"); // API maison (NkEnv.h)
					if (!h || !*h)
						h = env::GetEnvVar("HOME");
					return NkString((h && *h) ? h : ".");
				}

				// Dossier connu : preferer la redirection OneDrive si elle existe (sinon Explorer
				// montre plus de fichiers que le ~/Desktop local, qui est vide/perime).
				static NkString KnownFolder(const char *sub) {
					const char *od = env::GetEnvVar("OneDrive");
					if (od && *od) {
						NkString p = (NkPath(od) / sub).ToString();
						if (NkDirectory::Exists(p.CStr()))
							return p;
					}
					return (NkPath(Home().CStr()) / sub).ToString();
				}

				static NkString Desktop() { return KnownFolder("Desktop"); }
				static NkString Documents() { return KnownFolder("Documents"); }

				// Change de dossier ; empile l'historique (navigation avant/arriere) si demande.
				void SetDir(const char *dir, bool pushHistory) {
					int32 n = 0;
					for (; dir[n] && n + 1 < (int32)sizeof(curDir); ++n)
						curDir[n] = dir[n];
					curDir[n] = '\0';
					scanned = false;
					selected = -1;
					scroll = 0.f;
					status.Clear();
					renameIdx = -1;
					menuIdx = -1;
					confirmDel = -1;
					if (pushHistory) {
						while ((int32)history.Size() > histPos + 1)
							history.PopBack();
						history.PushBack(NkString(curDir));
						histPos = (int32)history.Size() - 1;
					}
				}

				void Up() {
					NkString parent = NkPath(curDir).GetParent().ToString();
					if (!parent.Empty() && !SEq(parent.CStr(), curDir))
						SetDir(parent.CStr(), true);
				}

				void Back() {
					if (histPos > 0) {
						--histPos;
						SetDir(history[histPos].CStr(), false);
					}
				}

				void Forward() {
					if (histPos + 1 < (int32)history.Size()) {
						++histPos;
						SetDir(history[histPos].CStr(), false);
					}
				}

				bool CanBack() const { return histPos > 0; }
				bool CanForward() const { return histPos + 1 < (int32)history.Size(); }
		};

	} // namespace editorkit
} // namespace nkentseu
