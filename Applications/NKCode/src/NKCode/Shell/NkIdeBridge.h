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
					NkString requete;
					while (mWs.PopMessage(requete))
						TraiterMcp(requete);
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
				// ── Serveur MCP (JSON-RPC 2.0) ──────────────────────────────────────
				// Le CLI ne parle pas un protocole maison : il se connecte a l'IDE comme
				// CLIENT MCP — ses outils s'appellent « mcp__ide__getDiagnostics »,
				// « mcp__ide__executeCode ». Il faut donc repondre a `initialize`,
				// `tools/list` et `tools/call`.
				//
				// L'extraction des champs est VOLONTAIREMENT rudimentaire (recherche de
				// "method" et "id") plutot qu'un analyseur JSON complet : le protocole
				// est etroit, les messages viennent d'un emetteur unique et connu, et un
				// analyseur maison serait une surface de bugs sans contrepartie.
				static NkString ChampTexte(const NkString &json, const char *cle) {
					NkString motif = NkString("\"") + cle + "\"";
					const char *p = NkFindSub(json.CStr(), motif.CStr());
					if (!p)
						return NkString();
					p += motif.Length();
					while (*p == ' ' || *p == ':')
						++p;
					if (*p != '"')
						return NkString();
					++p;
					NkString v;
					while (*p && *p != '"')
						v += *p++;
					return v;
				}

				// `id` peut etre un nombre OU une chaine : on le renvoie tel qu'ecrit,
				// pour le recopier a l'identique dans la reponse (exigence JSON-RPC).
				static NkString ChampId(const NkString &json) {
					const char *p = NkFindSub(json.CStr(), "\"id\"");
					if (!p)
						return NkString();
					p += 4;
					while (*p == ' ' || *p == ':')
						++p;
					NkString v;
					if (*p == '"') {
						v += *p++;
						while (*p && *p != '"')
							v += *p++;
						if (*p)
							v += *p;
						return v;
					}
					while (*p && *p != ',' && *p != '}' && *p != ' ')
						v += *p++;
					return v;
				}

				void Repondre(const NkString &id, const NkString &resultJson) {
					if (id.Empty())
						return; // notification : aucune reponse attendue
					NkString m = "{\"jsonrpc\":\"2.0\",\"id\":";
					m += id;
					m += ",\"result\":";
					m += resultJson;
					m += "}";
					(void)mWs.SendText(m);
				}

				void TraiterMcp(const NkString &req) {
					const NkString methode = ChampTexte(req, "method");
					const NkString id = ChampId(req);
					if (methode.Empty())
						return;

					if (methode == "initialize") {
						// On annonce la version que le CLI embarque deja (2024-11-05) et la
						// seule capacite qu'on offre : des outils.
						NkString r = "{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{\"tools\":{}},"
									 "\"serverInfo\":{\"name\":\"NKCode\",\"version\":\"";
						r += NkCodeVersion();
						r += "\"}}";
						Repondre(id, r);
						return;
					}
					if (methode == "tools/list") {
						// On n'annonce QUE ce que NKCode sait vraiment faire. Le CLI connait
						// aussi openDiff et executeCode, mais NKCode n'a ni vue de
						// comparaison ni cellules de calepin : les declarer ferait appeler
						// l'agent dans le vide, et attendre.
						Repondre(id,
								 "{\"tools\":["
								 "{\"name\":\"getDiagnostics\","
								 "\"description\":\"Diagnostics (erreurs et avertissements) des fichiers ouverts dans NKCode\","
								 "\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"required\":[]}},"
								 "{\"name\":\"openFile\","
								 "\"description\":\"Ouvre un fichier dans l'editeur NKCode et l'affiche a l'utilisateur\","
								 "\"inputSchema\":{\"type\":\"object\",\"properties\":{\"filePath\":{\"type\":\"string\","
								 "\"description\":\"Chemin du fichier a ouvrir\"}},\"required\":[\"filePath\"]}}"
								 "]}");
						return;
					}
					if (methode == "tools/call") {
						const NkString outil = ChampTexte(req, "name");
						if (outil == "openFile") {
							const NkString chemin = ChampTexte(req, "filePath");
							if (chemin.Empty() || !NkFile::Exists(chemin.CStr())) {
								Repondre(id, NkString("{\"content\":[{\"type\":\"text\",\"text\":\"fichier introuvable : ") +
												 JsonEscape(chemin).CStr() + "\"}],\"isError\":true}");
								return;
							}
							// Sur le THREAD UI : Tick() est appele depuis le dessin de la barre
							// d'outils, donc toucher l'etat de l'editeur est sur ici.
							if (mState)
								mState->OpenPath(NkPath(chemin.CStr()));
							Repondre(id, NkString("{\"content\":[{\"type\":\"text\",\"text\":\"ouvert : ") +
											 JsonEscape(chemin).CStr() + "\"}],\"isError\":false}");
							return;
						}
						if (outil == "getDiagnostics") {
							Repondre(id, NkString("{\"content\":[{\"type\":\"text\",\"text\":\"") +
											 JsonEscape(Diagnostics()).CStr() + "\"}],\"isError\":false}");
							return;
						}
						// Outil inconnu : on le DIT, au lieu de laisser le CLI attendre.
						Repondre(id, NkString("{\"content\":[{\"type\":\"text\",\"text\":\"outil inconnu : ") +
										 JsonEscape(outil).CStr() + "\"}],\"isError\":true}");
						return;
					}
					// `notifications/initialized` et le reste : rien a repondre.
				}

				// Diagnostics de TOUS les fichiers ouverts, en texte lisible. Les lignes
				// sont ramenees en base 1 : c'est ainsi qu'un humain — et l'agent — les
				// designe, alors qu'elles sont stockees en base 0.
				NkString Diagnostics() const {
					if (!mState)
						return NkString("(aucun workspace)");
					NkString out;
					int32 total = 0;
					for (usize i = 0; i < mState->files.Size(); ++i) {
						const OpenFile &f = mState->files[i];
						if (f.doc.diags.Empty())
							continue;
						out += f.path.ToString();
						out += "\n";
						for (usize k = 0; k < f.doc.diags.Size(); ++k) {
							const auto &d = f.doc.diags[k];
							out += NkPrintf("  %s ligne %d, colonne %d : ", d.sev == 1 ? "erreur" : "avertissement",
											d.line + 1, d.col + 1);
							out += d.msg;
							out += "\n";
							++total;
						}
					}
					if (total == 0)
						return NkString("Aucun diagnostic dans les fichiers ouverts.");
					return NkPrintf("%d diagnostic(s) :\n", total) + out.CStr();
				}

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
