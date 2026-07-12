// =============================================================================
// NkLsp.cpp — implementation du client LSP (pipes Win32 + JSON + protocole).
// =============================================================================
#include "NKCode/Project/NkLsp.h"

#include "NKContainers/String/NkFormat.h" // NkPrintf (formatage maison, ex-<cstdio>/<cstring>/<cstdlib>)

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace nkentseu {
	namespace nkcode {

		// ── NkJsonDoc ─────────────────────────────────────────────────────────
		static void JsonSkipWs(const char *s, int32 n, int32 &i) {
			while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
				++i;
		}

		static bool JsonParseString(const char *s, int32 n, int32 &i, NkString &out) {
			if (i >= n || s[i] != '"')
				return false;
			++i;
			while (i < n && s[i] != '"') {
				if (s[i] == '\\' && i + 1 < n) {
					const char e = s[i + 1];
					if (e == 'n')
						out += '\n';
					else if (e == 't')
						out += '\t';
					else if (e == 'r') { /* ignore */
					} else if (e == 'u') {
						// \uXXXX : approximation ASCII (les diagnostics clangd sont ASCII/UTF-8 direct)
						i += 4 < n - i ? 4 : n - i - 1;
						out += '?';
					} else
						out += e; // \" \\ \/ ...
					i += 2;
					continue;
				}
				out += s[i];
				++i;
			}
			if (i >= n)
				return false;
			++i; // '"'
			return true;
		}

		int32 NkJsonDoc::ParseValue(const char *s, int32 n, int32 &i) {
			JsonSkipWs(s, n, i);
			if (i >= n)
				return -1;
			const int32 self = static_cast<int32>(mPool.Size());
			mPool.PushBack(NkJsonVal{});
			const char c = s[i];
			if (c == '{') {
				mPool[self].kind = 5;
				++i;
				JsonSkipWs(s, n, i);
				if (i < n && s[i] == '}') {
					++i;
					return self;
				}
				while (i < n) {
					JsonSkipWs(s, n, i);
					NkString key;
					if (!JsonParseString(s, n, i, key))
						return -1;
					JsonSkipWs(s, n, i);
					if (i >= n || s[i] != ':')
						return -1;
					++i;
					const int32 kid = ParseValue(s, n, i);
					if (kid < 0)
						return -1;
					mPool[self].keys.PushBack(key);
					mPool[self].kids.PushBack(kid);
					JsonSkipWs(s, n, i);
					if (i < n && s[i] == ',') {
						++i;
						continue;
					}
					break;
				}
				JsonSkipWs(s, n, i);
				if (i >= n || s[i] != '}')
					return -1;
				++i;
				return self;
			}
			if (c == '[') {
				mPool[self].kind = 4;
				++i;
				JsonSkipWs(s, n, i);
				if (i < n && s[i] == ']') {
					++i;
					return self;
				}
				while (i < n) {
					const int32 kid = ParseValue(s, n, i);
					if (kid < 0)
						return -1;
					mPool[self].kids.PushBack(kid);
					JsonSkipWs(s, n, i);
					if (i < n && s[i] == ',') {
						++i;
						continue;
					}
					break;
				}
				JsonSkipWs(s, n, i);
				if (i >= n || s[i] != ']')
					return -1;
				++i;
				return self;
			}
			if (c == '"') {
				mPool[self].kind = 3;
				if (!JsonParseString(s, n, i, mPool[self].str))
					return -1;
				return self;
			}
			if (c == 't' || c == 'f') {
				mPool[self].kind = 1;
				mPool[self].b = (c == 't');
				i += (c == 't') ? 4 : 5;
				return i <= n ? self : -1;
			}
			if (c == 'n') {
				mPool[self].kind = 0;
				i += 4;
				return i <= n ? self : -1;
			}
			// nombre
			{
				mPool[self].kind = 2;
				char buf[48];
				int32 k = 0;
				while (i < n && k < 47 &&
					   ((s[i] >= '0' && s[i] <= '9') || s[i] == '-' || s[i] == '+' || s[i] == '.' || s[i] == 'e' ||
						s[i] == 'E')) {
					buf[k++] = s[i];
					++i;
				}
				buf[k] = 0;
				if (k == 0)
					return -1;
				float64 numv = 0.0;
				NkString(buf).ToDouble(numv); // conversion maison (ex-std::atof)
				mPool[self].num = numv;
				return self;
			}
		}

		bool NkJsonDoc::Parse(const char *text, int32 len) {
			mPool.Clear();
			mRoot = -1;
			int32 i = 0;
			mRoot = ParseValue(text, len, i);
			return mRoot >= 0;
		}

		const NkJsonVal *NkJsonDoc::Root() const {
			return (mRoot >= 0 && mRoot < static_cast<int32>(mPool.Size())) ? &mPool[mRoot] : nullptr;
		}

		const NkJsonVal *NkJsonDoc::Member(const NkJsonVal *obj, const char *key) const {
			if (!obj || obj->kind != 5)
				return nullptr;
			for (usize i = 0; i < obj->keys.Size(); ++i) {
				const char *a = obj->keys[i].CStr();
				const char *b = key;
				while (*a && *a == *b) {
					++a;
					++b;
				}
				if (!*a && !*b)
					return &mPool[obj->kids[i]];
			}
			return nullptr;
		}

		int32 NkJsonDoc::Count(const NkJsonVal *v) const {
			return v ? static_cast<int32>(v->kids.Size()) : 0;
		}

		const NkString *NkJsonDoc::KeyAt(const NkJsonVal *obj, int32 i) const {
			if (!obj || obj->kind != 5 || i < 0 || i >= static_cast<int32>(obj->keys.Size()))
				return nullptr;
			return &obj->keys[i];
		}

		const NkJsonVal *NkJsonDoc::ValAt(const NkJsonVal *obj, int32 i) const {
			if (!obj || obj->kind != 5 || i < 0 || i >= static_cast<int32>(obj->kids.Size()))
				return nullptr;
			return &mPool[obj->kids[i]];
		}

		const NkJsonVal *NkJsonDoc::At(const NkJsonVal *arr, int32 i) const {
			if (!arr || arr->kind != 4 || i < 0 || i >= static_cast<int32>(arr->kids.Size()))
				return nullptr;
			return &mPool[arr->kids[i]];
		}

		// ── NkPipeProc (Win32) ────────────────────────────────────────────────
		NkPipeProc::~NkPipeProc() {
			Stop();
		}

#if defined(_WIN32)
		bool NkPipeProc::Start(const NkString &cmdline, const NkString &cwd) {
			Stop();
			SECURITY_ATTRIBUTES sa{};
			sa.nLength = sizeof(sa);
			sa.bInheritHandle = TRUE;
			HANDLE inR = nullptr, inW = nullptr, outR = nullptr, outW = nullptr;
			if (!CreatePipe(&inR, &inW, &sa, 0))
				return false;
			if (!CreatePipe(&outR, &outW, &sa, 0)) {
				CloseHandle(inR);
				CloseHandle(inW);
				return false;
			}
			// NOTRE cote ne doit pas etre herite par l'enfant.
			SetHandleInformation(inW, HANDLE_FLAG_INHERIT, 0);
			SetHandleInformation(outR, HANDLE_FLAG_INHERIT, 0);

			STARTUPINFOW si{};
			si.cb = sizeof(si);
			si.dwFlags = STARTF_USESTDHANDLES;
			si.hStdInput = inR;
			si.hStdOutput = outW;
			si.hStdError = GetStdHandle(STD_ERROR_HANDLE); // stderr de clangd -> log parent (invisible)
			PROCESS_INFORMATION pi{};

			// UTF-8 -> UTF-16 (cmdline + cwd).
			wchar_t wcmd[2048];
			MultiByteToWideChar(CP_UTF8, 0, cmdline.CStr(), -1, wcmd, 2048);
			wchar_t wcwd[1024];
			const wchar_t *pcwd = nullptr;
			if (!cwd.Empty()) {
				MultiByteToWideChar(CP_UTF8, 0, cwd.CStr(), -1, wcwd, 1024);
				pcwd = wcwd;
			}
			const BOOL ok =
				CreateProcessW(nullptr, wcmd, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, pcwd, &si, &pi);
			CloseHandle(inR); // cotes ENFANT : plus besoin chez nous
			CloseHandle(outW);
			if (!ok) {
				CloseHandle(inW);
				CloseHandle(outR);
				return false;
			}
			CloseHandle(pi.hThread);
			mChild = pi.hProcess;
			mStdinW = inW;
			mStdoutR = outR;
			return true;
		}

		void NkPipeProc::Stop() {
			if (mStdinW) {
				CloseHandle(static_cast<HANDLE>(mStdinW));
				mStdinW = nullptr;
			}
			if (mChild) {
				// Fermeture de stdin -> clangd s'arrete ; on force apres un court delai.
				if (WaitForSingleObject(static_cast<HANDLE>(mChild), 300) == WAIT_TIMEOUT)
					TerminateProcess(static_cast<HANDLE>(mChild), 0);
				CloseHandle(static_cast<HANDLE>(mChild));
				mChild = nullptr;
			}
			if (mStdoutR) {
				CloseHandle(static_cast<HANDLE>(mStdoutR));
				mStdoutR = nullptr;
			}
		}

		bool NkPipeProc::Running() {
			if (!mChild)
				return false;
			return WaitForSingleObject(static_cast<HANDLE>(mChild), 0) == WAIT_TIMEOUT;
		}

		bool NkPipeProc::WriteData(const char *d, int32 n) {
			if (!mStdinW)
				return false;
			DWORD written = 0;
			return WriteFile(static_cast<HANDLE>(mStdinW), d, static_cast<DWORD>(n), &written, nullptr) &&
				   written == static_cast<DWORD>(n);
		}

		int32 NkPipeProc::ReadAvail(char *dst, int32 cap) {
			if (!mStdoutR)
				return 0;
			DWORD avail = 0;
			if (!PeekNamedPipe(static_cast<HANDLE>(mStdoutR), nullptr, 0, nullptr, &avail, nullptr) || avail == 0)
				return 0;
			DWORD toRead = avail < static_cast<DWORD>(cap) ? avail : static_cast<DWORD>(cap);
			DWORD got = 0;
			if (!ReadFile(static_cast<HANDLE>(mStdoutR), dst, toRead, &got, nullptr))
				return 0;
			return static_cast<int32>(got);
		}
#else
		bool NkPipeProc::Start(const NkString &, const NkString &) {
			return false;
		}

		void NkPipeProc::Stop() {
		}

		bool NkPipeProc::Running() {
			return false;
		}

		bool NkPipeProc::WriteData(const char *, int32) {
			return false;
		}

		int32 NkPipeProc::ReadAvail(char *, int32) {
			return 0;
		}
#endif

		// ── NkLspClient ───────────────────────────────────────────────────────
		void NkLspClient::JsonEscape(const char *in, NkString &out) {
			for (const char *p = in; *p; ++p) {
				const char c = *p;
				if (c == '"' || c == '\\') {
					out += '\\';
					out += c;
				} else if (c == '\n')
					out += "\\n";
				else if (c == '\r') { /* CRLF -> LF */
				} else if (c == '\t')
					out += "\\t";
				else if (static_cast<unsigned char>(c) < 0x20)
					out += ' ';
				else
					out += c;
			}
		}

		NkString NkLspClient::UriOf(const NkString &path) {
			NkString u("file:///");
			for (const char *p = path.CStr(); *p; ++p) {
				char c = *p;
				if (c == '\\')
					c = '/';
				if (c == ' ') {
					u += "%20";
					continue;
				}
				u += c;
			}
			return u;
		}

		NkString NkLspClient::PathOfUri(const NkString &uri) {
			const char *p = uri.CStr();
			// "file:///d%3A/..." ou "file:///D:/..."
			if (p[0] == 'f')
				p += 8; // strlen("file:///")
			NkString out;
			while (*p) {
				if (*p == '%' && p[1] && p[2]) { // %XX
					auto hex = [](char h) {
						if (h >= '0' && h <= '9')
							return h - '0';
						if (h >= 'a' && h <= 'f')
							return h - 'a' + 10;
						if (h >= 'A' && h <= 'F')
							return h - 'A' + 10;
						return 0;
					};
					out += static_cast<char>(hex(p[1]) * 16 + hex(p[2]));
					p += 3;
					continue;
				}
				out += (*p == '/') ? '\\' : *p;
				++p;
			}
			return out;
		}

		void NkLspClient::Send(const NkString &body) {
			const NkString hdr = NkPrintf("Content-Length: %d\r\n\r\n", static_cast<int32>(body.Size())); // maison
			mProc.WriteData(hdr.CStr(), static_cast<int32>(hdr.Size()));
			mProc.WriteData(body.CStr(), static_cast<int32>(body.Size()));
		}

		bool NkLspClient::Start(const NkString &clangdCmd, const NkString &rootDir, const NkString &ccDir) {
			NkString cmd = NkString("\"") + clangdCmd.CStr() + "\" --compile-commands-dir=\"" + ccDir.CStr() +
						   "\" --log=error --background-index";
			if (!mProc.Start(cmd, rootDir)) {
				log.PushBack(NkString("[lsp] impossible de lancer : ") + clangdCmd.CStr());
				return false;
			}
			// initialize (id 1)
			NkString root = UriOf(rootDir);
			NkString body("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"processId\":null,"
						  "\"rootUri\":\"");
			body += root.CStr();
			body += "\",\"capabilities\":{\"textDocument\":{\"publishDiagnostics\":{}}}}}";
			Send(body);
			log.PushBack(NkString("[lsp] clangd lance, initialize envoye (") + clangdCmd.CStr() + ")");
			return true;
		}

		void NkLspClient::Stop() {
			if (mProc.Running()) {
				Send(NkString("{\"jsonrpc\":\"2.0\",\"id\":9999,\"method\":\"shutdown\"}"));
				Send(NkString("{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}"));
			}
			mProc.Stop();
			mReady = false;
			mOpen.Clear();
			mVersions.Clear();
			mBuf.Clear();
		}

		bool NkLspClient::Running() {
			return mProc.Running();
		}

		void NkLspClient::DidOpen(const NkString &path, const NkString &text) {
			for (usize i = 0; i < mOpen.Size(); ++i) {
				const char *a = mOpen[i].CStr();
				const char *b = path.CStr();
				while (*a && *a == *b) {
					++a;
					++b;
				}
				if (!*a && !*b)
					return; // deja ouvert
			}
			mOpen.PushBack(path);
			mVersions.PushBack(1);
			NkString esc;
			JsonEscape(text.CStr(), esc);
			NkString body("{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{"
						  "\"uri\":\"");
			body += UriOf(path).CStr();
			body += "\",\"languageId\":\"cpp\",\"version\":1,\"text\":\"";
			body += esc.CStr();
			body += "\"}}}";
			Send(body);
		}

		void NkLspClient::DidChange(const NkString &path, const NkString &text) {
			int32 idx = -1;
			for (usize i = 0; i < mOpen.Size(); ++i) {
				const char *a = mOpen[i].CStr();
				const char *b = path.CStr();
				while (*a && *a == *b) {
					++a;
					++b;
				}
				if (!*a && !*b) {
					idx = static_cast<int32>(i);
					break;
				}
			}
			if (idx < 0) {
				DidOpen(path, text);
				return;
			}
			const int32 v = ++mVersions[static_cast<usize>(idx)];
			NkString esc;
			JsonEscape(text.CStr(), esc);
			NkString body =
				NkPrintf("{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":{\"textDocument\":{"
						 "\"uri\":\"%s\",\"version\":%d},\"contentChanges\":[{\"text\":\"",
						 UriOf(path).CStr(), v); // NkPrintf maison
			body += esc.CStr();
			body += "\"}]}}";
			Send(body);
		}

		void NkLspClient::ReqAt(const char *method, const NkString &path, int32 line, int32 col, int32 kind) {
			const int32 id = ++mNextId;
			mPendId = id;
			mPendKind = kind;
			const NkString body =
				NkPrintf("{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"textDocument/%s\",\"params\":{"
						 "\"textDocument\":{\"uri\":\"%s\"},\"position\":{\"line\":%d,\"character\":%d}%s}}",
						 id, method, UriOf(path).CStr(), line, col,
						 kind == 2 ? ",\"context\":{\"includeDeclaration\":true}" : ""); // NkPrintf maison
			Send(body);
		}

		void NkLspClient::ReqDefinition(const NkString &path, int32 line, int32 col) {
			ReqAt("definition", path, line, col, 1);
		}

		void NkLspClient::ReqHover(const NkString &path, int32 line, int32 col) {
			ReqAt("hover", path, line, col, 3);
		}

		void NkLspClient::ReqRename(const NkString &path, int32 line, int32 col, const NkString &newName) {
			const int32 id = ++mNextId;
			mPendId = id;
			mPendKind = 4;
			NkString esc;
			JsonEscape(newName.CStr(), esc);
			const NkString body =
				NkPrintf("{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"textDocument/rename\",\"params\":{"
						 "\"textDocument\":{\"uri\":\"%s\"},\"position\":{\"line\":%d,\"character\":%d},"
						 "\"newName\":\"%s\"}}",
						 id, UriOf(path).CStr(), line, col, esc.CStr()); // NkPrintf maison
			Send(body);
		}

		void NkLspClient::ReqReferences(const NkString &path, int32 line, int32 col) {
			ReqAt("references", path, line, col, 2);
		}

		void NkLspClient::Poll() {
			char tmp[8192];
			for (;;) {
				const int32 got = mProc.ReadAvail(tmp, sizeof(tmp));
				if (got <= 0)
					break;
				for (int32 i = 0; i < got; ++i)
					mBuf.PushBack(tmp[i]);
			}
			// Decoupe des trames « Content-Length: N\r\n\r\n{...} ».
			for (;;) {
				const int32 n = static_cast<int32>(mBuf.Size());
				if (n < 20)
					return;
				// cherche "Content-Length:"
				int32 h = -1;
				for (int32 i = 0; i + 15 < n; ++i) {
					if (mBuf[i] == 'C' && mBuf[i + 1] == 'o' && mBuf[i + 14] == ':') {
						h = i;
						break;
					}
				}
				if (h < 0)
					return;
				int32 j = h + 15;
				while (j < n && mBuf[j] == ' ')
					++j;
				int32 len = 0;
				while (j < n && mBuf[j] >= '0' && mBuf[j] <= '9') {
					len = len * 10 + (mBuf[j] - '0');
					++j;
				}
				// fin d'en-tetes : \r\n\r\n
				int32 e = -1;
				for (int32 i = j; i + 3 < n; ++i) {
					if (mBuf[i] == '\r' && mBuf[i + 1] == '\n' && mBuf[i + 2] == '\r' && mBuf[i + 3] == '\n') {
						e = i + 4;
						break;
					}
				}
				if (e < 0 || e + len > n)
					return; // trame incomplete
				HandleMessage(&mBuf[0] + e, len);
				// consomme [0, e+len)
				const int32 rest = n - (e + len);
				for (int32 i = 0; i < rest; ++i)
					mBuf[i] = mBuf[e + len + i];
				while (static_cast<int32>(mBuf.Size()) > rest)
					mBuf.PopBack();
			}
		}

		void NkLspClient::HandleMessage(const char *json, int32 len) {
			NkJsonDoc doc;
			if (!doc.Parse(json, len))
				return;
			const NkJsonVal *root = doc.Root();
			const NkJsonVal *method = doc.Member(root, "method");
			const NkJsonVal *id = doc.Member(root, "id");
			if (id && !method) { // REPONSE a une de nos requetes
				const int32 idn = static_cast<int32>(id->num);
				if (!mReady && idn == 1) { // initialize
					Send(NkString("{\"jsonrpc\":\"2.0\",\"method\":\"initialized\",\"params\":{}}"));
					mReady = true;
					log.PushBack(NkString("[lsp] clangd PRET (initialise)"));
					return;
				}
				if (idn == mPendId && mPendKind) { // definition / references / hover / rename
					const NkJsonVal *res = doc.Member(root, "result");
					resLocs.Clear();
					resKind = mPendKind;
					mPendId = 0;
					mPendKind = 0;
					resHover = NkString();
					resEdits.Clear();
					if (resKind == 3) { // hover : contents = string | {kind,value} | [MarkedString]
						const NkJsonVal *ct = doc.Member(res, "contents");
						if (ct && ct->kind == 3)
							resHover = ct->str;
						else if (ct && ct->kind == 5) {
							const NkJsonVal *v = doc.Member(ct, "value");
							if (v && v->kind == 3)
								resHover = v->str;
						} else if (ct && ct->kind == 4) {
							for (int32 i = 0;; ++i) {
								const NkJsonVal *e2 = doc.At(ct, i);
								if (!e2)
									break;
								if (e2->kind == 3)
									resHover += e2->str.CStr();
								else {
									const NkJsonVal *v = doc.Member(e2, "value");
									if (v && v->kind == 3)
										resHover += v->str.CStr();
								}
								resHover += '\n';
							}
						}
						return;
					}
					if (resKind == 4) { // rename : WorkspaceEdit.changes = { uri : [TextEdit] }
						const NkJsonVal *chg = doc.Member(res, "changes");
						for (int32 fi = 0; fi < doc.Count(chg); ++fi) {
							const NkString *uriK = doc.KeyAt(chg, fi);
							const NkJsonVal *arr2 = doc.ValAt(chg, fi);
							if (!uriK || !arr2)
								continue;
							const NkString ph = PathOfUri(*uriK);
							for (int32 i = 0;; ++i) {
								const NkJsonVal *ed = doc.At(arr2, i);
								if (!ed)
									break;
								const NkJsonVal *rg = doc.Member(ed, "range");
								const NkJsonVal *st = doc.Member(rg, "start");
								const NkJsonVal *en2 = doc.Member(rg, "end");
								const NkJsonVal *nt = doc.Member(ed, "newText");
								if (!st || !en2)
									continue;
								const NkJsonVal *sl = doc.Member(st, "line");
								const NkJsonVal *sc = doc.Member(st, "character");
								const NkJsonVal *el = doc.Member(en2, "line");
								const NkJsonVal *ec = doc.Member(en2, "character");
								if (!sl || !el || static_cast<int32>(sl->num) != static_cast<int32>(el->num))
									continue; // rename : plages mono-ligne uniquement
								NkLspEdit e3;
								e3.path = ph;
								e3.line = static_cast<int32>(sl->num);
								e3.colStart = sc ? static_cast<int32>(sc->num) : 0;
								e3.colEnd = ec ? static_cast<int32>(ec->num) : 0;
								e3.text = nt ? nt->str : NkString();
								resEdits.PushBack(e3);
							}
						}
						log.PushBack(
							NkPrintf("[lsp] rename : %d edition(s)", static_cast<int32>(resEdits.Size()))); // maison
						return;
					}
					auto pushLoc = [&](const NkJsonVal *loc) {
						if (!loc)
							return;
						const NkJsonVal *uri = doc.Member(loc, "uri");
						const NkJsonVal *range = doc.Member(loc, "range");
						if (!uri) { // LocationLink
							uri = doc.Member(loc, "targetUri");
							range = doc.Member(loc, "targetSelectionRange");
						}
						const NkJsonVal *st = doc.Member(range, "start");
						if (!uri || !st)
							return;
						NkLspLoc L2;
						L2.path = PathOfUri(uri->str);
						const NkJsonVal *ln2 = doc.Member(st, "line");
						const NkJsonVal *cl2 = doc.Member(st, "character");
						L2.line = ln2 ? static_cast<int32>(ln2->num) : 0;
						L2.col = cl2 ? static_cast<int32>(cl2->num) : 0;
						resLocs.PushBack(L2);
					};
					if (res && res->kind == 4) {
						for (int32 i = 0;; ++i) {
							const NkJsonVal *L3 = doc.At(res, i);
							if (!L3)
								break;
							pushLoc(L3);
						}
					} else if (res && res->kind == 5)
						pushLoc(res);
					log.PushBack(NkPrintf("[lsp] %s : %d emplacement(s)", resKind == 1 ? "definition" : "references",
										  static_cast<int32>(resLocs.Size()))); // NkPrintf maison
				}
				return;
			}
			if (!method || method->kind != 3)
				return;
			const char *m = method->str.CStr();
			auto isMethod = [&](const char *w) {
				const char *a = m;
				while (*a && *a == *w) {
					++a;
					++w;
				}
				return !*a && !*w;
			};
			// publishDiagnostics -> extrait uri + liste (line/character/severity/message)
			if (isMethod("textDocument/publishDiagnostics")) {
				const NkJsonVal *params = doc.Member(root, "params");
				const NkJsonVal *uri = doc.Member(params, "uri");
				const NkJsonVal *arr = doc.Member(params, "diagnostics");
				if (!uri || !arr)
					return;
				diagPath = PathOfUri(uri->str);
				diags.Clear();
				for (int32 i = 0;; ++i) {
					const NkJsonVal *dg = doc.At(arr, i);
					if (!dg)
						break;
					const NkJsonVal *range = doc.Member(dg, "range");
					const NkJsonVal *startp = doc.Member(range, "start");
					const NkJsonVal *lineV = doc.Member(startp, "line");
					const NkJsonVal *colV = doc.Member(startp, "character");
					const NkJsonVal *sevV = doc.Member(dg, "severity");
					const NkJsonVal *msgV = doc.Member(dg, "message");
					NkLspDiag d2;
					d2.line = lineV ? static_cast<int32>(lineV->num) : 0;
					d2.col = colV ? static_cast<int32>(colV->num) : 0;
					d2.sev = (sevV && static_cast<int32>(sevV->num) >= 2) ? 0 : 1; // 1=Error, 2=Warning...
					d2.msg = msgV ? msgV->str : NkString();
					diags.PushBack(d2);
				}
				diagsFresh = true;
				log.PushBack(NkPrintf("[lsp] %d diagnostic(s) <- %s", static_cast<int32>(diags.Size()),
									  diagPath.CStr())); // NkPrintf maison
			}
		}

	} // namespace nkcode
} // namespace nkentseu
