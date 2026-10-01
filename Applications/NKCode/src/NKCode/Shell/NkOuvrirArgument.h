#pragma once
// =============================================================================
// NkOuvrirArgument.h — CE QUE NKCODE OUVRE QUAND ON LUI DONNE UN CHEMIN (01/10).
//
//   NKCode.exe [<workspace>] [<fichier>]
//     <workspace> : un DOSSIER (avec ou sans workspace : sans, il s'ouvre en
//                   edition simple, comme dans VS Code), un .jenga de WORKSPACE (ce
//                   workspace precis), ou un FICHIER quelconque -- un .jenga qui
//                   n'est PAS un workspace (un projet seul) en est un : NKCode
//                   remonte alors de son dossier jusqu'au premier qui porte un
//                   workspace (.jenga avec 'with workspace') et ouvre le fichier
//                   dedans ; s'il n'y en a aucun, il ouvre le dossier du fichier
//                   en edition simple, et le fichier dans un onglet.
//     <fichier>   : le fichier a ouvrir dans le workspace (un onglet actif).
//
// POURQUOI : UnkenyEditor lancait « NKCode.exe <script.cpp> ». Le premier
// argument etait pris pour un DOSSIER, LoadFolder refusait un fichier, et
// l'ecran de chargement disait « Aucun workspace (.jenga avec 'with workspace')
// dans ce dossier » (capture de Rihen, 01/10 21:28). Un IDE a qui l'on donne un
// fichier doit l'ouvrir, dans le projet qui le contient -- comme VS Code ou
// Visual Studio.
// =============================================================================
#include "NKCode/Project/NkCodeState.h"
#include "NKContainers/String/NkString.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkPath.h"

namespace nkentseu {
	namespace nkcode {

		struct NkArgOuverture {
				NkString dossier; ///< le dossier a charger (racine du workspace)
				NkString jenga;	  ///< le .jenga a choisir (vide : le seul, ou aucun)
				NkString fichier; ///< le fichier a ouvrir ensuite (vide : aucun)
		};

		/// « \ » -> « / », sans barre finale.
		inline NkString NkArgPropre(const char *c) {
			NkString r(c ? c : "");
			char *p = const_cast<char *>(r.CStr());
			for (usize i = 0; i < r.Length(); ++i)
				if (p[i] == '\\')
					p[i] = '/';
			while (r.Length() > 3u && r.CStr()[r.Length() - 1u] == '/')
				r = NkString(r.CStr(), r.Length() - 1u);
			return r;
		}

		/// Le dossier parent (« C:/a/b/c.cpp » -> « C:/a/b »), vide a la racine.
		inline NkString NkArgParent(const NkString &c) {
			const char *s = c.CStr();
			const char *barre = nullptr;
			for (const char *q = s; *q; ++q)
				if (*q == '/')
					barre = q;
			if (!barre || barre == s)
				return NkString();
			// « C:/x » -> « C:/ » (la racine d'un lecteur garde sa barre)
			if (barre - s == 2 && s[1] == ':')
				return NkString(s, 3u);
			return NkString(s, (usize)(barre - s));
		}

		/// Le premier dossier, en remontant depuis `depart` (8 niveaux au plus), qui
		/// porte un workspace Jenga. Vide si aucun.
		inline NkString NkArgDossierDuWorkspace(const NkString &depart) {
			NkString d = depart;
			for (int32 k = 0; k < 8 && !d.Empty(); ++k) {
				NkVector<NkString> chemins, noms;
				NkCodeState::ScanWorkspacesIn(NkPath(d.CStr()), chemins, noms);
				if (!chemins.Empty())
					return d;
				const NkString p = NkArgParent(d);
				if (p.Empty() || p == d)
					break;
				d = p;
			}
			return NkString();
		}

		/// Ce qu'il faut charger et ouvrir pour les arguments `a` (et `b`).
		inline NkArgOuverture NkResoudreArgument(const char *a, const char *b) {
			NkArgOuverture o;
			const NkString p = NkArgPropre(a);
			if (NkDirectory::Exists(p.CStr())) {
				o.dossier = p;
			} else if (NkFile::Exists(p.CStr())) {
				const usize n = p.Length();
				const bool estJenga = n > 6u && (p.CStr()[n - 6] == '.') &&
									  (p.CStr()[n - 5] == 'j' || p.CStr()[n - 5] == 'J') &&
									  (p.CStr()[n - 4] == 'e' || p.CStr()[n - 4] == 'E') &&
									  (p.CStr()[n - 3] == 'n' || p.CStr()[n - 3] == 'N') &&
									  (p.CStr()[n - 2] == 'g' || p.CStr()[n - 2] == 'G') &&
									  (p.CStr()[n - 1] == 'a' || p.CStr()[n - 1] == 'A');
				const NkString parent = NkArgParent(p);
				// (02/10) Un .jenga qui ne DECLARE PAS de workspace (un projet seul) est
				// un fichier comme un autre : il s'ouvrait en dossier vide, sans onglet.
				// Mutation de banc NK_NKCODE_MUTATION=jenga-fichier : l'ancien traitement.
				const bool workspace =
					estJenga && (NkCodeState::EstWorkspaceJenga(p.CStr()) || NkCodeState::MutationNkCode("jenga-fichier"));
				if (workspace) {
					o.dossier = parent;
					// Le choisir n'a de sens que si le dossier en porte plusieurs (sinon
					// le choix ouvrirait en plus le .jenga dans un onglet).
					NkVector<NkString> chemins, noms;
					NkCodeState::ScanWorkspacesIn(NkPath(parent.CStr()), chemins, noms);
					if (chemins.Size() > 1u)
						o.jenga = p;
				} else {
					o.fichier = p;
					const NkString ws = NkArgDossierDuWorkspace(parent);
					o.dossier = ws.Empty() ? parent : ws; // sans workspace : edition simple du dossier
				}
			} else {
				o.dossier = p; // absent : l'ecran de chargement le dira (load.err.nofolder)
			}
			if (b && *b)
				o.fichier = NkArgPropre(b);
			// Mutation de banc NK_NKCODE_MUTATION=fichier : le fichier n'est pas ouvert.
			if (NkCodeState::MutationNkCode("fichier"))
				o.fichier = NkString();
			return o;
		}

	} // namespace nkcode
} // namespace nkentseu
