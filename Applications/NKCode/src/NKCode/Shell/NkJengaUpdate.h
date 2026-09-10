#pragma once
// =============================================================================
// NkJengaUpdate.h — Mettre a jour le JENGA EMBARQUE, sans reinstaller NKCode.
//
// POURQUOI SEPARER LES DEUX MISES A JOUR
// ======================================
// NkUpdate.h met a jour NKCODE : il telecharge un installeur de 96 Mo, le lance,
// et NKCode redemarre. C'est lourd, et c'est justifie quand l'IDE lui-meme a
// change.
//
// Mais NKCode embarque AUSSI Jenga, et Jenga bouge bien plus souvent que l'IDE :
// trois versions en trois jours au moment ou ces lignes sont ecrites. Obliger
// l'utilisateur a reinstaller tout NKCode pour recuperer un correctif de Jenga
// serait disproportionne — et il ne le ferait pas.
//
// Or un Jenga perime ne se voit pas : il construit, sans le mot du DSL qu'un
// fichier de projet recent emploie, et l'erreur ne prononce jamais le mot
// Jenga. Le 10 septembre 2026, `appurl()` a fait echouer trois constructions
// pour cette seule raison.
//
// Ici, la mise a jour pese UN MEGAOCTET, ne demande aucun droit
// d'administrateur, et ne touche qu'un dossier : tools/jenga-src/Jenga.
//
// CE QU'ELLE FAIT, EN QUATRE TEMPS
// ================================
//   1. demande a GitHub la derniere release de Rihen-Universe/Jenga ;
//   2. compare son tag a la version LUE dans le Jenga embarque ;
//   3. telecharge le wheel (un ZIP d'environ 1 Mo) ;
//   4. l'extrait a cote, puis echange les dossiers.
//
// POURQUOI L'ECHANGE, ET PAS UNE EXTRACTION PAR-DESSUS
// ====================================================
// Extraire par-dessus l'ancien laisserait les fichiers SUPPRIMES entre les deux
// versions : un module retire continuerait d'exister et d'etre importe. On
// extrait donc a cote, puis on echange, puis on efface l'ancien. Si l'extraction
// echoue a mi-chemin, l'ancien dossier est toujours intact.
//
// LE REDEMARRAGE
// ==============
// L'interpreteur Python de NKCode garde en memoire les modules deja importes :
// remplacer les fichiers ne suffit pas a changer ce qui tourne. La mise a jour
// est donc effective AU PROCHAIN DEMARRAGE, et on le DIT plutot que de laisser
// croire au contraire. NKCode propose de redemarrer ; l'utilisateur decide.
//
// Reseau : `curl`, comme NkUpdate.h, lance par NkProcess -> asynchrone,
// l'interface ne gele jamais. Aucune dependance ajoutee.
// =============================================================================
#include "NKCode/Project/NkEmbeddedJenga.h" // EmbeddedVersion(), JengaSrcDir()
#include "NKCode/Project/NkProcess.h"
#include "NKCode/Project/NkText.h" // NkFindSub
#include "NKCode/Shell/NkUpdate.h" // NkUpdateState : JsonField, FirstReleaseSlice, CompareVersions
#include "NKContainers/String/NkFormat.h"
#include "NKFileSystem/NkFile.h"
#include "NKPlatform/NkEnv.h"

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu;

		// Depot PUBLIC de Jenga. Distinct de celui de NKCode : deux produits,
		// deux rythmes de publication.
		inline const char *NkJengaRepo() {
			return "Rihen-Universe/Jenga";
		}

		struct NkJengaUpdateState {
				// ── Verification ──
				bool checking = false;
				bool checked = false;
				bool available = false;
				NkString latest;  // tag distant, ex. "v2.8.0"
				NkString url;	  // URL du wheel
				NkString error;
				// ── Telechargement ──
				bool downloading = false;
				bool ready = false;
				NkString localWheel;
				// ── Installation ──
				bool installing = false;
				bool installed = false; // effective au prochain demarrage
				// ── Demandes de l'UI ──
				bool reqCheck = false;
				bool reqInstall = false;

				NkProcess proc;
				NkVector<NkString> lines;

			private:
				// Quelle etape le processus courant sert-il ? Sans ce marqueur, Poll()
				// ne saurait pas quoi faire de la sortie de curl : le meme NkProcess
				// sert successivement a interroger, telecharger et extraire.
				enum class Etape { Aucune, Verifier, Telecharger, Extraire, Echanger };
				Etape mEtape = Etape::Aucune;
				NkString mStage; // dossier d'extraction temporaire

			public:
				// La version EMBARQUEE, celle qui compte : ce que NKCode utilisera
				// vraiment, et non ce qu'un `jenga --version` du systeme raconte.
				static NkString Current() {
					return NkEmbeddedJenga::EmbeddedVersion();
				}

				// URL du PREMIER asset dont le nom finit par ".whl".
				//
				// On ne prend pas « le premier asset » : une release de Jenga en publie
				// quatre, dont deux archives d'exemples. Prendre au hasard donnerait un
				// zip d'exemples a la place du paquet.
				static NkString FindWheelUrl(const char *json) {
					if (!json)
						return NkString();
					const char *p = json;
					while ((p = NkFindSub(p, "\"browser_download_url\""))) {
						const NkString u = NkUpdateState::JsonField(p, "browser_download_url");
						p += 22;
						if (u.Empty())
							continue;
						const usize n = u.Size();
						if (n > 4) {
							const char *e = u.CStr() + (n - 4);
							if (e[0] == '.' && (e[1] == 'w' || e[1] == 'W') && (e[2] == 'h' || e[2] == 'H') &&
								(e[3] == 'l' || e[3] == 'L'))
								return u;
						}
					}
					return NkString();
				}

				// ── Verification ────────────────────────────────────────────────
				void StartCheck() {
					if (Busy())
						return;
					checking = true;
					checked = false;
					available = false;
					error.Clear();
					lines.Clear();
					mEtape = Etape::Verifier;
					// Comme pour NKCode : on demande la LISTE et non /releases/latest,
					// pour que les pre-versions comptent aussi.
					const NkString cmd = NkString("curl -s -L -m 20 -H \"Accept: application/vnd.github+json\" ") +
										 "\"https://api.github.com/repos/" + NkJengaRepo() + "/releases?per_page=5\"";
					if (!proc.Start(cmd)) {
						checking = false;
						mEtape = Etape::Aucune;
						error = NkString("verification impossible (curl deja en cours)");
					}
				}

				bool Busy() const {
					return checking || downloading || installing;
				}

			private:
				void StartDownload() {
					if (url.Empty()) {
						error = NkString("aucun paquet a telecharger dans cette version");
						return;
					}
					downloading = true;
					ready = false;
					error.Clear();
					lines.Clear();
					mEtape = Etape::Telecharger;
					localWheel = NkUpdateState::TempDir() + "/jenga-update.whl";
					const NkString cmd = NkString("curl -s -L -m 300 -o \"") + localWheel.CStr() + "\" " + url.CStr();
					if (!proc.Start(cmd)) {
						downloading = false;
						mEtape = Etape::Aucune;
						error = NkString("telechargement impossible (curl deja en cours)");
					}
				}

				// Extraction du wheel — c'est un ZIP ordinaire.
				//
				// On n'embarque pas de bibliotheque de decompression pour un fichier
				// par an : les deux systemes en fournissent une.
				//   Windows 10 et suivants : `tar` (bsdtar) lit le zip.
				//   Ailleurs : `unzip`.
				void StartExtract() {
					const NkString dest = NkEmbeddedJenga::JengaSrcDir();
					if (dest.Empty()) {
						error = NkString("aucun Jenga embarque a mettre a jour");
						return;
					}
					installing = true;
					error.Clear();
					lines.Clear();
					mEtape = Etape::Extraire;
					mStage = dest + "/.jenga-maj";
					NkString cmd;
#if defined(_WIN32)
					// rmdir puis mkdir : un reste d'une tentative precedente
					// melangerait deux versions sans que rien ne le dise.
					cmd = NkString("cmd /c rmdir /s /q \"") + mStage.CStr() + "\" 2>nul & mkdir \"" + mStage.CStr() +
						  "\" && tar -xf \"" + localWheel.CStr() + "\" -C \"" + mStage.CStr() + "\"";
#else
					cmd = NkString("rm -rf \"") + mStage.CStr() + "\" && mkdir -p \"" + mStage.CStr() +
						  "\" && unzip -o -q \"" + localWheel.CStr() + "\" -d \"" + mStage.CStr() + "\"";
#endif
					if (!proc.Start(cmd)) {
						installing = false;
						mEtape = Etape::Aucune;
						error = NkString("extraction impossible (processus deja en cours)");
					}
				}

				// L'echange : ancien de cote, neuf en place, ancien efface.
				//
				// L'ordre compte. On DEPLACE l'ancien avant de mettre le neuf : si la
				// machine s'arrete entre les deux, il reste un dossier « .ancien »
				// recuperable a la main, et jamais un dossier a moitie ecrase.
				void StartSwap() {
					const NkString dest = NkEmbeddedJenga::JengaSrcDir();
					const NkString vieux = dest + "/Jenga.ancien";
					const NkString neuf = mStage + "/Jenga";
					const NkString actuel = dest + "/Jenga";
					mEtape = Etape::Echanger;
					NkString cmd;
#if defined(_WIN32)
					cmd = NkString("cmd /c rmdir /s /q \"") + vieux.CStr() + "\" 2>nul & move \"" + actuel.CStr() +
						  "\" \"" + vieux.CStr() + "\" && move \"" + neuf.CStr() + "\" \"" + actuel.CStr() +
						  "\" && rmdir /s /q \"" + vieux.CStr() + "\" & rmdir /s /q \"" + mStage.CStr() + "\"";
#else
					cmd = NkString("rm -rf \"") + vieux.CStr() + "\"; mv \"" + actuel.CStr() + "\" \"" + vieux.CStr() +
						  "\" && mv \"" + neuf.CStr() + "\" \"" + actuel.CStr() + "\" && rm -rf \"" + vieux.CStr() +
						  "\" \"" + mStage.CStr() + "\"";
#endif
					if (!proc.Start(cmd)) {
						installing = false;
						mEtape = Etape::Aucune;
						error = NkString("remplacement impossible (processus deja en cours)");
					}
				}

			public:
				// A appeler CHAQUE FRAME.
				void Poll() {
					if (Busy() && proc.Running()) {
						proc.Drain(lines);
						return;
					}

					if (mEtape == Etape::Verifier && checking) {
						proc.Drain(lines);
						if (!proc.Done())
							return;
						checking = false;
						checked = true;
						mEtape = Etape::Aucune;
						NkString raw;
						for (usize i = 0; i < lines.Size(); ++i)
							raw += lines[i];
						lines.Clear();
						if (raw.Empty()) {
							error = NkString("aucune reponse (hors ligne ?)");
							return;
						}
						const NkString first = NkUpdateState::FirstReleaseSlice(raw.CStr());
						latest = NkUpdateState::JsonField(first.CStr(), "tag_name");
						if (latest.Empty()) {
							error = NkString("reponse inattendue de GitHub");
							return;
						}
						url = FindWheelUrl(first.CStr());
						const NkString cur = Current();
						if (cur.Empty()) {
							error = NkString("version du Jenga embarque illisible");
							return;
						}
						available = NkUpdateState::CompareVersions(latest.CStr(), cur.CStr()) > 0;
						if (available && url.Empty())
							error = NkString("version ") + latest.CStr() +
									" disponible, mais sans paquet .whl dans cette release";
						return;
					}

					if (mEtape == Etape::Telecharger && downloading) {
						proc.Drain(lines);
						if (!proc.Done())
							return;
						downloading = false;
						lines.Clear();
						if (proc.ExitCode() != 0 || !NkFile::Exists(localWheel.CStr())) {
							mEtape = Etape::Aucune;
							error = NkString("telechargement echoue");
							return;
						}
						ready = true;
						StartExtract(); // enchaine sans repasser par l'utilisateur
						return;
					}

					if (mEtape == Etape::Extraire && installing) {
						proc.Drain(lines);
						if (!proc.Done())
							return;
						lines.Clear();
						// On verifie le RESULTAT et non le code de sortie : sous Windows
						// la commande enchaine plusieurs outils, et un `2>nul` avale des
						// codes. La seule preuve qui vaille est le fichier attendu.
						const NkString temoin = mStage + "/Jenga/_version.py";
						if (!NkFile::Exists(temoin.CStr())) {
							installing = false;
							mEtape = Etape::Aucune;
							error = NkString("le paquet telecharge ne contient pas Jenga/_version.py");
							return;
						}
						StartSwap();
						return;
					}

					if (mEtape == Etape::Echanger && installing) {
						proc.Drain(lines);
						if (!proc.Done())
							return;
						installing = false;
						lines.Clear();
						mEtape = Etape::Aucune;
						const NkString cur = Current();
						// Meme regle : on relit ce qu'on vient d'ecrire. Un remplacement
						// qui rend 0 sans avoir change la version est un echec.
						if (cur.Empty()) {
							error = NkString("remplacement echoue : Jenga embarque introuvable");
							return;
						}
						installed = true;
						available = false;
						return;
					}

					if (reqInstall) {
						reqInstall = false;
						if (!Busy())
							StartDownload();
					}
					if (reqCheck) {
						reqCheck = false;
						StartCheck();
					}
				}

				// Libelle court pour le menu Aide et la barre d'etat.
				NkString StatusLabel() const {
					if (checking)
						return NkString("Jenga : recherche d'une mise a jour...");
					if (downloading)
						return NkString("Jenga : telechargement...");
					if (installing)
						return NkString("Jenga : installation...");
					if (installed)
						return NkString("Jenga mis a jour -- actif au prochain demarrage");
					if (!error.Empty())
						return NkString("Jenga : ") + error.CStr();
					if (available)
						return NkString("Jenga ") + latest.CStr() + " disponible";
					if (checked) {
						const NkString cur = Current();
						return NkString("Jenga est a jour (") + (cur.Empty() ? "?" : cur.CStr()) + ")";
					}
					return NkString();
				}
		};

	} // namespace nkcode
} // namespace nkentseu
