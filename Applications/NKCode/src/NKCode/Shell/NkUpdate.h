#pragma once
// =============================================================================
// NkUpdate.h — Mises a jour in-app (ROADMAP Phase 13).
//
//   Demande Rihen : l'utilisateur doit etre NOTIFIE quand une mise a jour
//   existe ; s'il ACCEPTE, la mise a jour se fait SANS reinstallation manuelle,
//   puis NKCode REDEMARRE.
//
//   STRATEGIE — on ne remplace PAS les fichiers a la main. NKCode est distribue
//   avec un vrai installeur Inno Setup (scripts/MakeNkCodeDist.py --installer) :
//   on telecharge le nouveau `setup.exe` et on le lance. Inno reconnait son
//   AppId, met a jour EN PLACE (aucune desinstallation prealable), puis relance
//   NKCode via sa section [Run]. C'est le patron des applications de bureau
//   reelles : bien plus sur qu'un remplacement de DLL/exe a chaud (fichiers
//   verrouilles par le process en cours, mise a jour partielle si coupure...).
//
//   Reseau : `curl` (deja utilise par le panneau IA) lance via NkProcess ->
//   asynchrone, l'interface ne gele jamais. Aucune dependance ajoutee.
// =============================================================================
#include "NKCode/Project/NkProcess.h"
#include "NKCode/Project/NkText.h" // NkFindSub (recherche de sous-chaine maison)
#include "NKCode/Shell/NkShell.h"  // NkCodeShellRun (lancement de l'installeur)
#include "NKCode/Shell/NkUi.h"	   // NkCodeVersion() : source unique de la version
#include "NKContainers/String/NkFormat.h"
#include "NKFileSystem/NkFile.h"
#include "NKPlatform/NkEnv.h"

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu;

		// Depot PUBLIC des binaires de test (distinct du depot moteur).
		inline const char *NkUpdateRepo() {
			return "Rihen-Universe/NKCode-Beta";
		}

		struct NkUpdateState {
				// ── Etat de la verification ──
				bool checking = false;
				bool checked = false;	 // une verification a abouti (succes ou echec)
				bool available = false;	 // une version PLUS RECENTE existe
				bool dismissed = false;	 // « Plus tard » : ne plus notifier cette session
				NkString latest;		 // tag distant, ex. "v0.1.0-beta.2"
				NkString url;			 // URL du setup .exe a telecharger
				NkString error;			 // message d'erreur lisible (vide si tout va bien)
				// ── Etat du telechargement ──
				bool downloading = false;
				bool ready = false;		 // installeur telecharge, pret a etre lance
				NkString localSetup;	 // chemin du setup telecharge
				// ── Demande de l'UI ──
				bool reqCheck = false;	 // menu Aide > Rechercher les mises a jour
				bool reqInstall = false; // l'utilisateur a accepte

				NkProcess proc;
				NkVector<NkString> lines;

			private:
				bool mAuto = false; // la verification automatique du demarrage a-t-elle eu lieu ?

			public:
				// ── Comparaison de versions TOLERANTE ────────────────────────────
				// Gere « v0.1.0-beta », « 0.1.0-beta.2 », « 1.2 ». Regle : on compare
				// d'abord les nombres dans l'ordre ou ils apparaissent (le « 2 » de
				// « beta.2 » compte donc comme un composant), et une version SANS
				// suffixe est consideree PLUS RECENTE qu'une pre-version de memes
				// nombres (0.1.0 > 0.1.0-beta).
				static void NumsOf(const char *s, int32 *out, int32 &n, bool &pre) {
					n = 0;
					pre = false;
					if (!s)
						return;
					for (const char *p = s; *p; ++p) {
						if (*p == '-' || *p == '+')
							pre = true; // debut d'un suffixe de pre-version
						if (*p >= '0' && *p <= '9') {
							int32 v = 0;
							while (*p >= '0' && *p <= '9') {
								v = v * 10 + (*p - '0');
								++p;
							}
							if (n < 6)
								out[n++] = v;
							// Le `while` a laisse `p` SUR le separateur et le `++p` de la
							// boucle va le sauter : on teste donc le suffixe ICI, sinon un
							// « - » collé aux chiffres (« 0.1.0-beta ») passait inapercu et
							// la version etait jugee EGALE a « 0.1.0 » (bug attrape par le
							// harnais de test de CompareVersions).
							if (*p == '-' || *p == '+')
								pre = true;
							if (!*p)
								break;
						}
					}
				}

				// > 0 si `a` est plus recent que `b`, < 0 si plus ancien, 0 si egal.
				static int32 CompareVersions(const char *a, const char *b) {
					int32 na = 0, nb = 0;
					int32 va[6] = {0}, vb[6] = {0};
					bool pa = false, pb = false;
					NumsOf(a, va, na, pa);
					NumsOf(b, vb, nb, pb);
					const int32 n = (na > nb) ? na : nb;
					for (int32 i = 0; i < n; ++i) {
						const int32 x = (i < na) ? va[i] : 0;
						const int32 y = (i < nb) ? vb[i] : 0;
						if (x != y)
							return (x > y) ? 1 : -1;
					}
					if (pa != pb)
						return pa ? -1 : 1; // pre-version < version finale
					return 0;
				}

				// Extrait la valeur d'une cle JSON "cle":"valeur" (sortie de l'API
				// GitHub : suffisant, pas besoin d'un vrai analyseur).
				static NkString JsonField(const char *json, const char *key) {
					if (!json || !key)
						return NkString();
					const NkString pat = NkString("\"") + key + "\"";
					const char *p = json;
					const usize kl = pat.Size();
					for (; *p; ++p) {
						bool hit = true;
						for (usize i = 0; i < kl; ++i)
							if (p[i] != pat.CStr()[i]) {
								hit = false;
								break;
							}
						if (!hit)
							continue;
						p += kl;
						while (*p && *p != ':')
							++p;
						if (*p == ':')
							++p;
						while (*p == ' ' || *p == '\t')
							++p;
						if (*p != '"')
							return NkString();
						++p;
						NkString out;
						for (; *p && *p != '"'; ++p) {
							if (*p == '\\' && p[1]) { // dechappement minimal
								++p;
								out += (*p == 'n') ? '\n' : *p;
							} else
								out += *p;
						}
						return out;
					}
					return NkString();
				}

				// Fin du PREMIER objet release : la reponse est une LISTE
				// (« /releases ») ; le 2e « tag_name » marque la release suivante. On
				// borne la recherche d'assets a la premiere, sinon on risquerait de
				// proposer l'installeur d'une version PLUS ANCIENNE quand la plus
				// recente n'en publie pas.
				static NkString FirstReleaseSlice(const char *json) {
					if (!json)
						return NkString();
					const char *a = NkFindSub(json, "\"tag_name\"");
					if (!a)
						return NkString(json);
					const char *b = NkFindSub(a + 10, "\"tag_name\"");
					NkString out;
					for (const char *p = json; *p && (!b || p < b); ++p)
						out += *p;
					return out;
				}

				// URL du PREMIER asset dont le nom se termine par "setup.exe".
				static NkString FindSetupUrl(const char *json) {
					if (!json)
						return NkString();
					// Parcourt les occurrences de "browser_download_url" et retient
					// celle qui pointe vers un installeur.
					const char *p = json;
					while ((p = NkFindSub(p, "\"browser_download_url\""))) {
						const NkString u = JsonField(p, "browser_download_url");
						p += 22;
						if (u.Empty())
							continue;
						const usize n = u.Size();
						if (n > 9) { // se termine par "setup.exe" ?
							const char *e = u.CStr() + (n - 9);
							bool same = true;
							const char *w = "setup.exe";
							for (int32 i = 0; i < 9; ++i) {
								char x = e[i], y = w[i];
								if (x >= 'A' && x <= 'Z')
									x = static_cast<char>(x + 32);
								if (x != y) {
									same = false;
									break;
								}
							}
							if (same)
								return u;
						}
					}
					return NkString();
				}

				static NkString TempDir() {
					const char *t = env::GetEnvVar("TEMP");
					if (!t || !*t)
						t = env::GetEnvVar("TMP");
					return (t && *t) ? NkString(t) : NkString(".");
				}

				// ── Verification (asynchrone) ───────────────────────────────────
				void StartCheck() {
					if (checking || downloading)
						return;
					checking = true;
					checked = false;
					available = false;
					error.Clear();
					lines.Clear();
					// -s silencieux, -L suit les redirections, -m 20 borne l'attente :
					// une machine hors ligne ne doit pas laisser l'etat « en cours ».
					//
					// ⚠️ `/releases/latest` renvoie 404 tant qu'aucune release STABLE
					// n'existe : GitHub en EXCLUT les pre-releases. NKCode etant
					// distribue en pre-release (beta), on demande la LISTE et on prend
					// la premiere entree (la plus recente, pre-releases comprises).
					// Verifie contre l'API reelle : /releases/latest -> 404,
					// /releases -> v0.1.0-beta.1 (prerelease=true).
					const NkString cmd = NkString("curl -s -L -m 20 -H \"Accept: application/vnd.github+json\" ") +
										 "\"https://api.github.com/repos/" + NkUpdateRepo() + "/releases?per_page=5\"";
					if (!proc.Start(cmd)) {
						checking = false;
						error = NkString("verification impossible (curl deja en cours)");
					}
				}

				// Verification AUTOMATIQUE au demarrage, une seule fois par session.
				void AutoCheckOnce() {
					if (mAuto)
						return;
					mAuto = true;
					StartCheck();
				}

				void StartDownload() {
					if (downloading || url.Empty())
						return;
					downloading = true;
					ready = false;
					error.Clear();
					lines.Clear();
					localSetup = TempDir() + "/NKCode-update-setup.exe";
					const NkString cmd = NkString("curl -s -L -m 900 -o \"") + localSetup.CStr() + "\" " + url.CStr();
					if (!proc.Start(cmd)) {
						downloading = false;
						error = NkString("telechargement impossible (curl deja en cours)");
					}
				}

				// A appeler CHAQUE FRAME. Retourne true si l'appelant doit QUITTER
				// (installeur lance : il met a jour en place puis relance NKCode).
				bool Poll() {
					if (checking) {
						proc.Drain(lines);
						if (proc.Done()) {
							checking = false;
							checked = true;
							NkString raw;
							for (usize i = 0; i < lines.Size(); ++i)
								raw += lines[i];
							lines.Clear();
							if (raw.Empty()) {
								error = NkString("aucune reponse (hors ligne ?)");
								return false;
							}
							// Reponse = LISTE de releases ; on ne regarde que la premiere.
							const NkString first = FirstReleaseSlice(raw.CStr());
							latest = JsonField(first.CStr(), "tag_name");
							if (latest.Empty()) {
								error = NkString("reponse inattendue de GitHub");
								return false;
							}
							url = FindSetupUrl(first.CStr());
							available = CompareVersions(latest.CStr(), NkCodeVersion()) > 0;
							// Version plus recente publiee SANS installeur (uniquement des
							// archives .tar.xz, comme la beta.1) : on ne peut pas mettre a
							// jour automatiquement -> on le DIT et le menu renverra vers la
							// page des releases (telechargement manuel).
							if (available && url.Empty())
								error = NkString("version ") + latest.CStr() +
										" disponible (telechargement manuel : pas d'installeur dans cette release)";
						}
						return false;
					}
					if (downloading) {
						proc.Drain(lines);
						if (proc.Done()) {
							downloading = false;
							lines.Clear();
							const bool ok = proc.ExitCode() == 0 && NkFile::Exists(localSetup.CStr());
							if (!ok) {
								error = NkString("telechargement echoue");
								return false;
							}
							ready = true;
						}
						return false;
					}
					if (reqInstall && ready) {
						reqInstall = false;
						// /SILENT : progression visible mais aucune question ;
						// Inno met a jour EN PLACE (meme AppId) puis relance NKCode.
						const NkString cmd = NkString("start \"\" \"") + localSetup.CStr() + "\" /SILENT /NORESTART";
						NkCodeShellRun(cmd.CStr());
						return true; // l'appelant ferme NKCode (fichiers a remplacer)
					}
					if (reqInstall && !ready) {
						// Le drapeau est TOUJOURS consomme, meme sans URL : sinon il
						// resterait arme indefiniment et relancerait la logique a chaque
						// frame.
						reqInstall = false;
						if (!url.Empty())
							StartDownload();
						else
							error = NkString("aucun installeur a telecharger pour cette version");
					}
					if (reqCheck) {
						reqCheck = false;
						StartCheck();
					}
					return false;
				}

				// Libelle court pour la barre d'etat / le menu Aide.
				NkString StatusLabel() const {
					if (checking)
						return NkString("Recherche de mises a jour...");
					if (downloading)
						return NkString("Telechargement de la mise a jour...");
					if (ready)
						return NkString("Mise a jour prete a installer");
					if (!error.Empty())
						return NkString("Mise a jour : ") + error.CStr();
					if (available)
						return NkString("Version ") + latest.CStr() + " disponible";
					if (checked)
						return NkString("NKCode est a jour (") + NkCodeVersion() + ")";
					return NkString();
				}
		};

	} // namespace nkcode
} // namespace nkentseu
