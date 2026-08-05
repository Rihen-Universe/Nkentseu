#pragma once
// =============================================================================
// NkIdeBridge.h — NKCode se declare comme IDE aupres du CLI Claude Code, et lui
// pousse son etat EN TEMPS REEL (fichier actif, selection).
//
// ── Protocole (OBSERVE, pas suppose) ─────────────────────────────────────────
// Decouverte : un IDE ouvre un serveur WebSocket local et depose
//     ~/.claude/ide/<port>.lock
// contenant (forme relevee sur les fichiers deposes par VSCode) :
//     {"pid":N,"workspaceFolders":["C:\\..."],"ideName":"...",
//      "transport":"ws","runningInWindows":true,"authToken":"<uuid>"}
// Le CLI liste ce dossier, choisit l'entree dont le workspace correspond au
// sien, et se connecte au port avec le jeton.
//
// Messages : des notifications facon JSON-RPC. Le schema de `selection_changed`
// vient du validateur du CLI lui-meme :
//     { method: "selection_changed",
//       params: { selection: { start:{line,character}, end:{line,character} },
//                 text: string, filePath: string } }
// Les lignes sont indexees a ZERO (le CLI fait line+1 pour l'affichage) — c'est
// l'erreur de conversion evidente, elle est donc traitee au seul endroit ou l'on
// convertit.
//
// ── Ce que ca change ─────────────────────────────────────────────────────────
// Sans ce pont, l'agent ne connait le contexte que par le resume colle en tete
// de chaque message (fichier actif, ligne, selection). Avec, il VOIT l'editeur
// evoluer : la selection qu'on vient de faire, le fichier qu'on vient d'ouvrir.
// =============================================================================
#include "NKNetwork/Transport/NkWebSocketServer.h"
#include "NKCode/Project/NkCodeState.h"
#include "NKCode/Shell/NkOpenWs.h"      // NkOpenWsState::Home()
#include "NKCode/Shell/NkOpenWindows.h" // NkCurrentPid()
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKContainers/String/NkFormat.h" // NkPrintf

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu;

		class NkIdeBridge {
			public:
				// Demarre le serveur et depose le fichier de decouverte. Sans workspace,
				// on ne se declare PAS : le CLI apparie les IDE par dossier de travail,
				// une entree sans workspace ne servirait a personne.
				bool Start(NkCodeState *st) {
					if (mRunning || !st || !st->HasWorkspace())
						return false;
					if (!mWs.Start(0)) // 0 : port libre choisi par l'OS
						return false;
					mState = st;
					mWorkspace = st->root.ToString();
					mToken = MakeToken();
					mLock = LockPath(mWs.Port());
					if (!WriteLock()) {
						mWs.Stop();
						return false;
					}
					mRunning = true;
					return true;
				}

				// Retire le fichier de decouverte AVANT de fermer : un lock qui survit
				// designe un port mort, et le CLI perdrait du temps a s'y connecter.
				void Stop() {
					if (!mRunning)
						return;
					if (!mLock.Empty())
						NkFile::Delete(mLock.CStr());
					mWs.Stop();
					mRunning = false;
					mLock.Clear();
				}

				~NkIdeBridge() {
					Stop();
				}

				// Le SERVEUR ecoute-t-il ? Distinct de Connected(), qui dit si le CLI
				// s'est effectivement attache. On peut ecouter longtemps sans client.
				bool Running() const {
					return mRunning;
				}

				bool Connected() const {
					return mRunning && const_cast<net::NkWebSocketServer &>(mWs).HasClient();
				}

				uint16 Port() const {
					return mWs.Port();
				}

				// A appeler CHAQUE FRAME. Fait avancer le socket et n'emet que lorsque
				// l'etat a REELLEMENT change : reenvoyer la meme selection a chaque frame
				// noierait l'agent sous des notifications identiques.
				void Tick() {
					if (!mRunning)
						return;
					mWs.Poll();
					NkString ignored;
					while (mWs.PopMessage(ignored)) {
						// Requetes du CLI (getDiagnostics, openFile, openDiff...) : le
						// transport est en place, le traitement viendra. On draine pour ne
						// pas laisser la file grossir.
					}
					if (!mWs.HasClient() || !mState || !mState->HasActive())
						return;

					OpenFile &f = mState->files[mState->active];
					const NkString path = f.path.ToString();
					// L'ancre (selLine/selCol) peut etre APRES le curseur — selection faite
					// vers le haut. On ordonne donc les bornes : le protocole attend
					// start <= end, pas « ancre puis curseur ».
					int32 l0 = f.doc.selLine, c0 = f.doc.selCol;
					int32 l1 = f.doc.curLine, c1 = f.doc.curCol;
					if (l0 > l1 || (l0 == l1 && c0 > c1)) {
						int32 t;
						t = l0; l0 = l1; l1 = t;
						t = c0; c0 = c1; c1 = t;
					}
					if (path == mLastPath && l0 == mLastL0 && c0 == mLastC0 && l1 == mLastL1 && c1 == mLastC1)
						return; // rien de neuf : on se tait
					mLastPath = path;
					mLastL0 = l0;
					mLastC0 = c0;
					mLastL1 = l1;
					mLastC1 = c1;

					const NkString sel = f.doc.HasSel() ? f.doc.GetSelectedText() : NkString();
					NkString msg = "{\"method\":\"selection_changed\",\"params\":{\"selection\":{\"start\":{\"line\":";
					msg += NkPrintf("%d", l0);
					msg += ",\"character\":";
					msg += NkPrintf("%d", c0);
					msg += "},\"end\":{\"line\":";
					msg += NkPrintf("%d", l1);
					msg += ",\"character\":";
					msg += NkPrintf("%d", c1);
					msg += "}},\"text\":\"";
					msg += JsonEscape(sel);
					msg += "\",\"filePath\":\"";
					msg += JsonEscape(path);
					msg += "\"}}";
					(void)mWs.SendText(msg);
				}

			private:
				static NkString LockPath(uint16 port) {
					const NkString dir = (NkPath(NkOpenWsState::Home().CStr()) / ".claude" / "ide").ToString();
					NkDirectory::CreateRecursive(dir.CStr());
					return (NkPath(dir.CStr()) / NkPrintf("%d.lock", static_cast<int32>(port)).CStr()).ToString();
				}

				// Jeton d'appariement. Il ne protege pas un secret : il evite qu'un autre
				// processus local se fasse passer pour le CLI attendu. Derive de l'horloge
				// et du PID, sans dependance a un generateur cryptographique.
				NkString MakeToken() const {
					const uint64 t = static_cast<uint64>(NkCodeState::MTimeOf(mWorkspace.CStr()));
					const uint64 p = static_cast<uint64>(NkCurrentPid());
					return NkPrintf("nkcode-%llx-%llx", static_cast<unsigned long long>(t),
									static_cast<unsigned long long>(p));
				}

				// Antislash et guillemets DOIVENT etre echappes : un chemin Windows en
				// contient a chaque separateur, et un JSON invalide fait taire le pont
				// sans le moindre message.
				static NkString JsonEscape(const NkString &in) {
					NkString o;
					for (const char *p = in.CStr(); *p; ++p) {
						switch (*p) {
							case '\\':
								o += "\\\\";
								break;
							case '"':
								o += "\\\"";
								break;
							case '\n':
								o += "\\n";
								break;
							case '\r':
								o += "\\r";
								break;
							case '\t':
								o += "\\t";
								break;
							default:
								if (static_cast<unsigned char>(*p) < 0x20)
									o += ' ';
								else
									o += *p;
						}
					}
					return o;
				}

				bool WriteLock() {
					NkString j = "{\"pid\":";
					j += NkPrintf("%d", static_cast<int32>(NkCurrentPid()));
					j += ",\"workspaceFolders\":[\"";
					j += JsonEscape(mWorkspace);
					j += "\"],\"ideName\":\"NKCode\",\"transport\":\"ws\",\"runningInWindows\":";
#if defined(_WIN32)
					j += "true";
#else
					j += "false";
#endif
					j += ",\"authToken\":\"";
					j += mToken;
					j += "\"}";
					return NkFile::WriteAllText(NkPath(mLock.CStr()), j);
				}

				net::NkWebSocketServer mWs;
				NkCodeState *mState = nullptr;
				bool mRunning = false;
				NkString mLock, mToken, mWorkspace;

				// Dernier etat EMIS : sert a n'envoyer que les changements reels.
				NkString mLastPath;
				int32 mLastL0 = -1, mLastC0 = -1, mLastL1 = -1, mLastC1 = -1;
		};

	} // namespace nkcode
} // namespace nkentseu
