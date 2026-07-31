#pragma once
// =============================================================================
// NkOpenWindows.h — Registre des fenetres NKCode ouvertes, pour les restaurer
// au lancement suivant.
//
// POURQUOI CE FICHIER EXISTE
//
// Une fenetre NKCode = un PROCESSUS (cf. NkHomeOpenNewWindow). Il n'existe donc
// aucun etat partage entre fenetres : chacune doit s'inscrire dans un fichier
// commun pour que la suivante sache ce qui etait ouvert.
//
// SEMANTIQUE : « restaurer les fenetres qui n'ont PAS ete explicitement
// fermees ». Les deux sorties sont distinctes dans le code, ce qui rend la
// regle applicable sans ambiguite :
//
//   - Croix de la barre de titre -> NkWindowCloseEvent (NkEditorShell.cpp).
//     L'utilisateur ferme CETTE fenetre : on se DESINSCRIT, elle ne revient pas.
//   - Ctrl+Q / menu Quitter      -> NkEditorShell::RequestClose().
//     L'utilisateur quitte l'application : on RESTE inscrit, la fenetre revient.
//   - Plantage / arret systeme   -> ni l'un ni l'autre : on reste inscrit, donc
//     la fenetre revient. C'est le comportement voulu (reprise apres crash).
//
// DETECTION DES ENTREES PERIMEES
//
// Le fichier contient `pid|chemin`. Au lancement, une entree dont le PID n'est
// plus vivant provient forcement d'une session precedente : c'est une candidate
// a la restauration. Une entree dont le PID tourne encore appartient a une
// fenetre ouverte MAINTENANT et doit etre laissee tranquille — sans ce test,
// ouvrir une seconde fenetre restaurerait la premiere en double.
// =============================================================================
#include "NKCode/Shell/NkUi.h"
#include "NKContainers/String/NkFormat.h" // NkPrintf (formatage maison)
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#include <signal.h>
#endif

namespace nkentseu {
	namespace nkcode {

		// Chemin du registre : a cote de settings.cfg, meme convention.
		inline NkString NkOpenWindowsPath(const NkString &home) {
			return (NkPath(home.CStr()) / ".nkcode" / "open_windows.cfg").ToString();
		}

		// PID du processus courant.
		inline int64 NkCurrentPid() {
#if defined(_WIN32)
			return static_cast<int64>(::GetCurrentProcessId());
#else
			return static_cast<int64>(::getpid());
#endif
		}

		// Le processus `pid` tourne-t-il encore ?
		inline bool NkPidAlive(int64 pid) {
			if (pid <= 0)
				return false;
#if defined(_WIN32)
			// SYNCHRONIZE suffit pour interroger l'etat ; on n'ouvre aucun droit
			// d'ecriture sur un processus tiers.
			HANDLE h = ::OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
			if (!h)
				return false;
			const DWORD w = ::WaitForSingleObject(h, 0);
			::CloseHandle(h);
			// WAIT_TIMEOUT = toujours en cours ; WAIT_OBJECT_0 = termine.
			return w == WAIT_TIMEOUT;
#else
			// signal 0 : ne signale rien, verifie seulement l'existence du process.
			return ::kill(static_cast<pid_t>(pid), 0) == 0;
#endif
		}

		// Une ligne du registre.
		struct NkOpenWindowEntry {
				int64 pid = 0;
				NkString path;
		};

		// Lit toutes les lignes `pid|chemin`. Fichier absent = liste vide.
		inline NkVector<NkOpenWindowEntry> NkOpenWindowsRead(const NkString &home) {
			NkVector<NkOpenWindowEntry> out;
			const NkString file = NkOpenWindowsPath(home);
			if (!NkFile::Exists(file.CStr()))
				return out;
			const NkString txt = NkFile::ReadAllText(NkPath(file.CStr()));
			NkString line;
			for (const char *s = txt.CStr();; ++s) {
				if (*s == '\n' || *s == '\r' || *s == '\0') {
					if (!line.Empty()) {
						// Decoupe sur le PREMIER '|' : un chemin peut contenir tout
						// le reste, y compris des caracteres exotiques.
						const char *bar = nullptr;
						for (const char *p = line.CStr(); *p; ++p)
							if (*p == '|') {
								bar = p;
								break;
							}
						if (bar) {
							NkString pidStr;
							for (const char *p = line.CStr(); p < bar; ++p)
								pidStr += *p;
							NkOpenWindowEntry e;
							e.pid = 0;
							for (const char *p = pidStr.CStr(); *p; ++p)
								if (*p >= '0' && *p <= '9')
									e.pid = e.pid * 10 + (*p - '0');
							for (const char *p = bar + 1; *p; ++p)
								e.path += *p;
							if (e.pid > 0 && !e.path.Empty())
								out.PushBack(e);
						}
					}
					line = NkString();
					if (*s == '\0')
						break;
					continue;
				}
				line += *s;
			}
			return out;
		}

		// Reecrit le registre en entier.
		inline void NkOpenWindowsWrite(const NkString &home, const NkVector<NkOpenWindowEntry> &v) {
			const NkString dir = (NkPath(home.CStr()) / ".nkcode").ToString();
			if (!NkDirectory::Exists(dir.CStr()))
				NkDirectory::Create(dir.CStr());
			NkString s;
			for (usize i = 0; i < v.Size(); ++i) {
				s += NkPrintf("%lld", static_cast<long long>(v[i].pid));
				s += "|";
				s += v[i].path;
				s += "\n";
			}
			NkFile::WriteAllText(NkPath(NkOpenWindowsPath(home).CStr()), s);
		}

		// Inscrit la fenetre courante. `ws` vide (launcher sans workspace) = rien a
		// restaurer, on n'inscrit pas : rouvrir un launcher vide n'a aucun interet.
		inline void NkOpenWindowsRegister(const NkString &home, const NkString &ws) {
			if (ws.Empty())
				return;
			const int64 me = NkCurrentPid();
			NkVector<NkOpenWindowEntry> v = NkOpenWindowsRead(home);
			NkVector<NkOpenWindowEntry> keep;
			for (usize i = 0; i < v.Size(); ++i)
				if (v[i].pid != me)
					keep.PushBack(v[i]); // purge une eventuelle entree fantome du meme PID
			NkOpenWindowEntry e;
			e.pid = me;
			e.path = ws;
			keep.PushBack(e);
			NkOpenWindowsWrite(home, keep);
		}

		// Desinscrit la fenetre courante — appele UNIQUEMENT sur fermeture explicite
		// (croix de la barre de titre). Ne pas appeler depuis RequestClose() : Ctrl+Q
		// signifie « je quitte l'application », pas « je ferme ce workspace ».
		inline void NkOpenWindowsUnregister(const NkString &home) {
			const int64 me = NkCurrentPid();
			NkVector<NkOpenWindowEntry> v = NkOpenWindowsRead(home);
			NkVector<NkOpenWindowEntry> keep;
			for (usize i = 0; i < v.Size(); ++i)
				if (v[i].pid != me)
					keep.PushBack(v[i]);
			NkOpenWindowsWrite(home, keep);
		}

		// Renvoie les chemins laisses par des processus MORTS (session precedente) et
		// les retire du registre — pour qu'un second lancement ne les rouvre pas une
		// deuxieme fois. Les entrees encore vivantes sont conservees telles quelles.
		inline NkVector<NkString> NkOpenWindowsTakeStale(const NkString &home) {
			NkVector<NkString> stale;
			NkVector<NkOpenWindowEntry> v = NkOpenWindowsRead(home);
			NkVector<NkOpenWindowEntry> keep;
			for (usize i = 0; i < v.Size(); ++i) {
				if (NkPidAlive(v[i].pid)) {
					keep.PushBack(v[i]);
					continue;
				}
				// Le dossier a pu etre supprime/deplace entre deux sessions : on ne
				// tente pas de rouvrir ce qui n'existe plus.
				const char *p = v[i].path.CStr();
				if (p && p[0] && (NkDirectory::Exists(p) || NkFile::Exists(p))) {
					bool dup = false;
					for (usize k = 0; k < stale.Size(); ++k)
						if (stale[k] == v[i].path) {
							dup = true;
							break;
						}
					if (!dup)
						stale.PushBack(v[i].path);
				}
			}
			NkOpenWindowsWrite(home, keep);
			return stale;
		}

	} // namespace nkcode
} // namespace nkentseu
