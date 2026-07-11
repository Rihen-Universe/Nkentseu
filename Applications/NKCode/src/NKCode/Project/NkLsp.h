#pragma once
// =============================================================================
// NkLsp.h — Client LSP minimal (clangd) : process a PIPES + JSON-RPC framing
//   Content-Length. Etape A : handshake initialize/initialized, didOpen /
//   didChange (synchronisation FULL), reception de publishDiagnostics.
//   Le compile-first (RunDiagnostics) reste le REPLI quand clangd est absent.
//
//   L'implementation Win32 (windows.h, CreateProcessW, pipes anonymes) est
//   ISOLEE dans NkLsp.cpp, comme NkPty. Le parseur JSON (NkJsonDoc) est un
//   mini-arbre a POOL d'indices (pas de recursion de types), suffisant pour
//   les messages LSP ; il vit aussi dans NkLsp.cpp.
// =============================================================================
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu;

		// ── Valeur JSON (pool a indices : `kids` referencent d'autres entrees du pool) ──
		struct NkJsonVal {
				uint8 kind = 0; // 0 null, 1 bool, 2 nombre, 3 chaine, 4 tableau, 5 objet
				bool b = false;
				double num = 0.0;
				NkString str;
				NkVector<NkString> keys; // objets : cle de chaque enfant
				NkVector<int32> kids;	 // objets/tableaux : index des enfants
		};

		class NkJsonDoc {
			public:
				bool Parse(const char *text, int32 len);
				const NkJsonVal *Root() const;
				const NkJsonVal *Member(const NkJsonVal *obj, const char *key) const;
				const NkJsonVal *At(const NkJsonVal *arr, int32 i) const;
				int32 Count(const NkJsonVal *v) const;						 // enfants (objet/tableau)
				const NkString *KeyAt(const NkJsonVal *obj, int32 i) const;	 // cle i d'un objet
				const NkJsonVal *ValAt(const NkJsonVal *obj, int32 i) const; // valeur i d'un objet

			private:
				NkVector<NkJsonVal> mPool;
				int32 mRoot = -1;
				int32 ParseValue(const char *s, int32 n, int32 &i);
		};

		// ── Process enfant a PIPES (stdin/stdout rediriges, lecture NON bloquante) ──
		class NkPipeProc {
			public:
				NkPipeProc() = default;
				~NkPipeProc();
				NkPipeProc(const NkPipeProc &) = delete;
				NkPipeProc &operator=(const NkPipeProc &) = delete;

				bool Start(const NkString &cmdline, const NkString &cwd);
				void Stop();
				bool Running();
				bool WriteData(const char *d, int32 n);
				int32 ReadAvail(char *dst, int32 cap); // 0 si rien (PeekNamedPipe)

			private:
				void *mChild = nullptr;	  // HANDLE du process
				void *mStdinW = nullptr;  // notre cote ECRITURE -> stdin de l'enfant
				void *mStdoutR = nullptr; // notre cote LECTURE <- stdout de l'enfant
		};

		// ── Diagnostic remonte par clangd (0-based, comme NkCodeDoc::Diag) ──
		struct NkLspDiag {
				int32 line = 0, col = 0;
				uint8 sev = 1; // 1 = erreur, 0 = avertissement (mappe depuis severity LSP)
				NkString msg;
		};

		// Edition de texte (reponse rename) : remplace [colStart, colEnd) de `line` par `text`.
		struct NkLspEdit {
				NkString path;
				int32 line = 0, colStart = 0, colEnd = 0;
				NkString text;
		};

		// Emplacement (reponse definition/references) — chemin natif + position 0-based.
		struct NkLspLoc {
				NkString path;
				int32 line = 0, col = 0;
		};

		class NkLspClient {
			public:
				// `clangdCmd` = chemin/commande clangd ; `ccDir` = dossier du compile_commands.json.
				bool Start(const NkString &clangdCmd, const NkString &rootDir, const NkString &ccDir);
				void Stop();
				bool Running();

				bool Ready() const {
					return mReady;
				}

				void Poll(); // draine stdout, decoupe les trames, traite les messages

				void DidOpen(const NkString &path, const NkString &text);
				void DidChange(const NkString &path, const NkString &text);

				// Requetes SEMANTIQUES (une a la fois) : reponse dans resKind/resLocs (consommee par l'etat).
				void ReqDefinition(const NkString &path, int32 line, int32 col);
				void ReqReferences(const NkString &path, int32 line, int32 col);
				void ReqHover(const NkString &path, int32 line, int32 col);
				void ReqRename(const NkString &path, int32 line, int32 col, const NkString &newName);
				int32 resKind = 0;			  // 0 = rien, 1 = definition, 2 = references, 3 = hover, 4 = rename
				NkVector<NkLspLoc> resLocs;	  // emplacements de la reponse
				NkString resHover;			  // markdown de la reponse hover (kind 3)
				NkVector<NkLspEdit> resEdits; // editions de la reponse rename (kind 4)

				// Derniers diagnostics recus (un fichier a la fois : celui qui vient d'etre publie).
				NkString diagPath;
				NkVector<NkLspDiag> diags;
				bool diagsFresh = false; // consomme par l'etat (pose a false apres lecture)

				NkVector<NkString> log; // lignes « [lsp] ... » a pousser dans OUTPUT (consommees par l'etat)

			private:
				NkPipeProc mProc;
				NkVector<char> mBuf; // accumulation stdout (framing Content-Length)
				int32 mNextId = 1;
				bool mReady = false;
				NkVector<NkString> mOpen;  // chemins ouverts (didOpen deja envoye)
				NkVector<int32> mVersions; // version de chaque document ouvert

				int32 mPendId = 0, mPendKind = 0; // requete en vol (id -> type)
				void ReqAt(const char *method, const NkString &path, int32 line, int32 col, int32 kind);
				void Send(const NkString &body);
				void HandleMessage(const char *json, int32 len);
				static NkString UriOf(const NkString &path);
				static NkString PathOfUri(const NkString &uri);
				static void JsonEscape(const char *in, NkString &out);
		};

	} // namespace nkcode
} // namespace nkentseu
