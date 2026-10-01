//
// NkEditeurMoteur.cpp
// =============================================================================
// Description :
//   Le moteur precompile d'une construction de jeu (voir l'en-tete) :
//   empreinte, cache, sceau, verrou.
//
// Caracteristiques :
//   - L'empreinte est un FNV-1a 64 bits sur (chemin relatif, taille, octets)
//     de chaque fichier, dans l'ordre alphabetique des chemins : non
//     cryptographique, mais une modification accidentelle qui garderait le
//     meme resultat a une chance sur 2^64. Mesure du 2026-10-01 : 1 387
//     fichiers, 40 Mo, moins d'une seconde en Debug.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Livraison/NkEditeurMoteur.h"

#include "Livraison/NkEditeurConstruire.h"
#include "Livraison/NkEditeurProcessus.h"

#include "NKContainers/Utilities/NkSort.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkPath.h"
#include "NKTime/NkDate.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace nkentseu {
	namespace editeur {

		namespace {
			/// Monte quand ce que le cache CONTIENT change de forme (le workspace
			/// du moteur, la facon de sceller) : un vieux cache ne doit pas etre
			/// pris pour un neuf.
			constexpr const char *NK_FORMAT_MOTEUR = "unkeny-moteur-1";

			constexpr uint64 NK_FNV_BASE = 0xcbf29ce484222325ull;
			constexpr uint64 NK_FNV_PREMIER = 0x100000001b3ull;

			uint64 Fnv(uint64 h, const void *octets, usize n) noexcept {
				const uint8 *p = static_cast<const uint8 *>(octets);
				for (usize i = 0; i < n; ++i) {
					h ^= p[i];
					h *= NK_FNV_PREMIER;
				}
				return h;
			}

			uint64 FnvTexte(uint64 h, const NkString &s) noexcept {
				h = Fnv(h, s.CStr(), s.Length());
				const uint8 zero = 0u;
				return Fnv(h, &zero, 1u); // separateur : « ab|c » != « a|bc »
			}

			NkString Oblique(const NkString &s) {
				NkString r;
				for (usize i = 0; i < s.Length(); ++i) {
					r.Append(s[i] == '\\' ? '/' : s[i]);
				}
				return r;
			}

			NkString Barre(const NkString &s) {
				const NkString o = Oblique(s);
				return (o.Empty() || o.EndsWith('/')) ? o : o + "/";
			}

			NkString Env(const char *nom) {
				const char *v = std::getenv(nom);
				return NkString(v != nullptr ? v : "");
			}

			char Minuscule(char c) noexcept {
				return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
			}

			bool EgalSansCasse(const char *a, const char *b) noexcept {
				if (a == nullptr || b == nullptr) {
					return false;
				}
				for (; *a != '\0' && *b != '\0'; ++a, ++b) {
					if (Minuscule(*a) != Minuscule(*b)) {
						return false;
					}
				}
				return *a == *b;
			}

			/// Taille et date d'un fichier (0, 0 s'il n'existe pas).
			void Stat(const NkString &chemin, int64 &taille, int64 &date) {
				taille = 0;
				date = 0;
				const NkPath p(chemin.CStr());
				const NkString nom = p.GetFileName();
				const NkVector<NkDirectoryEntry> e = NkDirectory::GetEntries(p.GetParent().ToString().CStr(), nom.CStr());
				for (usize i = 0; i < e.Size(); ++i) {
					if (e[i].IsFile && e[i].Name == nom) {
						taille = e[i].Size;
						date = e[i].ModificationTime;
						return;
					}
				}
			}

			/// Un fichier qui n'entre pas dans l'empreinte : la documentation et
			/// tout ce qui vit sous un .git (un sous-module a le sien).
			bool HorsEmpreinte(const NkString &relatif) {
				if (relatif.EndsWith(".md") || relatif.EndsWith(".MD")) {
					return true;
				}
				return relatif.StartsWith(".git") || relatif.Find("/.git") != NkString::npos;
			}

			/// Les variables d'environnement qu'un .jenga lit : `getenv("X"`,
			/// `environ.get("X"` et `environ["X"]`.
			void VariablesLues(const NkString &texte, NkVector<NkString> &noms) {
				static const char *kMotifs[] = {"getenv(\"", "environ.get(\"", "environ[\""};
				for (const char *motif : kMotifs) {
					usize pos = 0;
					while ((pos = texte.Find(motif, pos)) != NkString::npos) {
						pos += std::strlen(motif);
						NkString nom;
						while (pos < texte.Length() && texte[pos] != '"') {
							nom.Append(texte[pos++]);
						}
						// Le chemin du depot et du cache ne sont pas le moteur : un
						// depot copie ailleurs garde son empreinte.
						if (nom.Empty() || nom == NkString("NK_UNKENY_DEPOT") || nom == NkString("NK_UNKENY_CACHE_MOTEUR")) {
							continue;
						}
						bool deja = false;
						for (usize k = 0; k < noms.Size() && !deja; ++k) {
							deja = noms[k] == nom;
						}
						if (!deja) {
							noms.PushBack(nom);
						}
					}
				}
			}

			bool AvantTexte(const NkString &a, const NkString &b) noexcept {
				return std::strcmp(a.CStr(), b.CStr()) < 0;
			}

			/// Le nom d'une archive statique d'un projet (miroir de
			/// STATIC_LIB_NAMING de Jenga/Commands/Kit.py).
			NkString Archive(const NkString &projet, const char *systeme) {
				if (EgalSansCasse(systeme, "Windows")) {
					return projet + ".lib";
				}
				if (EgalSansCasse(systeme, "Android") || EgalSansCasse(systeme, "HarmonyOS")) {
					return NkString("lib") + projet + ".a";
				}
				return projet + ".a";
			}

			/// Les chaines entre `ouvre` et le `]` qui suit `debut` dans `texte`,
			/// delimitees par `guillemet`.
			NkVector<NkString> Chaines(const NkString &texte, usize debut, char guillemet) {
				NkVector<NkString> r;
				if (debut == NkString::npos) {
					return r;
				}
				const usize ouvre = texte.Find('[', debut);
				const usize ferme = ouvre == NkString::npos ? NkString::npos : texte.Find(']', ouvre);
				if (ferme == NkString::npos) {
					return r;
				}
				NkString courant;
				bool dedans = false;
				for (usize i = ouvre; i < ferme; ++i) {
					if (texte[i] == guillemet) {
						if (dedans) {
							r.PushBack(courant);
							courant = NkString();
						}
						dedans = !dedans;
					} else if (dedans) {
						courant.Append(texte[i]);
					}
				}
				return r;
			}

			/// La valeur de « cle: valeur » dans le texte du sceau.
			NkString Champ(const NkString &texte, const char *cle) {
				const NkString motif = NkString("\n") + cle + ": ";
				const NkString t = NkString("\n") + texte;
				const usize p = t.Find(motif.CStr());
				if (p == NkString::npos) {
					return NkString();
				}
				NkString v;
				for (usize i = p + motif.Length(); i < t.Length() && t[i] != '\n' && t[i] != '\r'; ++i) {
					v.Append(t[i]);
				}
				return v;
			}
		} // namespace

		// =====================================================================
		const char *NkModeMoteurNom(NkModeMoteur m) noexcept {
			return m == NkModeMoteur::NK_SOURCES ? "sources" : "precompile";
		}

		bool NkModeMoteurDepuis(const char *nom, NkModeMoteur &sortie) noexcept {
			if (EgalSansCasse(nom, "sources") || EgalSansCasse(nom, "source")) {
				sortie = NkModeMoteur::NK_SOURCES;
				return true;
			}
			if (EgalSansCasse(nom, "precompile") || EgalSansCasse(nom, "kit") || EgalSansCasse(nom, "cache")) {
				sortie = NkModeMoteur::NK_PRECOMPILE;
				return true;
			}
			return false;
		}

		bool NkMoteurPrecompilePossible(NkPlateformeJeu p, bool essai, NkString &raison) {
			switch (p) {
				case NkPlateformeJeu::NK_WINDOWS:
					raison = NkString("moteur precompile (kit Jenga), mesure sous Windows");
					return true;
				case NkPlateformeJeu::NK_LINUX:
				case NkPlateformeJeu::NK_MACOS:
				case NkPlateformeJeu::NK_WEB:
					// Le kit sait ranger lib/<Config>-<Systeme>/*.a pour ces trois-la
					// (une seule architecture) ; personne ne l'a encore construit.
					if (essai) {
						raison = NkString::Format("moteur precompile DEMANDE pour %s : le kit le permet mais ce n'est pas encore eprouve",
												  NkPlateformeJeuNom(p));
						return true;
					}
					raison = NkString::Format("moteur precompile pas encore eprouve pour %s : construit depuis les sources "
											  "(--moteur=precompile pour l'essayer)",
											  NkPlateformeJeuNom(p));
					return false;
				case NkPlateformeJeu::NK_ANDROID:
				case NkPlateformeJeu::NK_HARMONYOS:
					raison = NkString::Format("%s : un paquet porte une bibliotheque PAR ABI (arm64-v8a, x86_64...) et le kit n'en "
											  "range qu'une par configuration -- construit depuis les sources",
											  NkPlateformeJeuNom(p));
					return false;
				default:
					raison = NkString::Format("%s : moteur precompile non prevu (bundle et signature Xcode) -- construit depuis les sources",
											  NkPlateformeJeuNom(p));
					return false;
			}
		}

		NkString NkRacineCacheMoteur() {
			const NkString force = Env("NK_UNKENY_CACHE_MOTEUR");
			if (!force.Empty()) {
				return Barre(force);
			}
#if defined(_WIN32)
			NkString base = Env("LOCALAPPDATA");
			if (base.Empty()) {
				base = NkDirectory::GetAppDataDirectory().ToString();
			}
			return Barre(base) + "Unkeny/Moteur/";
#elif defined(__APPLE__)
			return Barre(Env("HOME")) + "Library/Caches/Unkeny/Moteur/";
#else
			const NkString xdg = Env("XDG_CACHE_HOME");
			return xdg.Empty() ? Barre(Env("HOME")) + ".cache/unkeny/moteur/" : Barre(xdg) + "unkeny/moteur/";
#endif
		}

		NkString NkVersionJenga() {
			static bool lu = false;
			static NkString version;
			if (lu) {
				return version;
			}
			lu = true;
			NkVector<NkString> lignes;
			NkEditeurProcessus p;
			if (!p.Lancer(NkString("jenga --version"), NkString())) {
				return version;
			}
			p.Attendre(
				[](const NkString &ligne, void *d) {
					static_cast<NkVector<NkString> *>(d)->PushBack(ligne);
				},
				&lignes);
			for (usize i = 0; i < lignes.Size(); ++i) {
				const usize k = lignes[i].Find("version ");
				if (k != NkString::npos) {
					version = NkString(lignes[i].SubStr(k + 8));
					version.Trim();
				}
			}
			return version;
		}

		NkString NkSignatureChaine(NkPlateformeJeu p) {
			NkDisponibilite dispo[NK_NB_PLATEFORMES];
			NkDetecterPlateformes(dispo);
			const NkString &raison = dispo[static_cast<int32>(p)].raison;
			NkString s = raison;
			// « clang-mingw : C:/msys64/ucrt64/bin/clang++.exe » : le fichier
			// apres « : », s'il en est un, signe la VERSION par sa taille et sa
			// date (pacman -Syu change les deux).
			const usize k = raison.Find(": ");
			if (k != NkString::npos) {
				const NkString chemin(raison.SubStr(k + 2));
				int64 taille = 0;
				int64 date = 0;
				Stat(chemin, taille, date);
				if (taille > 0) {
					s += NkString::Format(" [%lld octets, %lld]", static_cast<long long>(taille), static_cast<long long>(date));
				}
			}
			return s;
		}

		NkVector<NkString> NkDossiersDuMoteur() {
			NkVector<NkString> d;
			const NkVector<NkString> &modules = NkModulesDuJoueur();
			for (usize i = 0; i < modules.Size(); ++i) {
				const NkString &m = modules[i];
				const usize barre = m.RFind('/');
				d.PushBack(barre == NkString::npos ? NkString(".") : NkString(m.SubStr(0, barre)));
			}
			d.PushBack(NkString("config"));
			NkSort(d.Data(), d.Data() + d.Size(), &AvantTexte);
			return d;
		}

		bool NkEmpreinteDesDossiers(const NkString &depot, const NkVector<NkString> &dossiers, const NkString &extra,
									NkEmpreinteMoteur &e, NkString &erreur) {
			e = NkEmpreinteMoteur();
			const NkString racine = Barre(depot);
			NkVector<NkString> relatifs;
			for (usize i = 0; i < dossiers.Size(); ++i) {
				const NkString dossier = racine + dossiers[i];
				if (!NkDirectory::Exists(dossier.CStr())) {
					erreur = NkString("dossier du moteur introuvable : ") + dossier;
					return false;
				}
				const NkVector<NkString> f = NkDirectory::GetFiles(dossier.CStr(), "*", NkSearchOption::NK_ALL_DIRECTORIES);
				for (usize k = 0; k < f.Size(); ++k) {
					const NkString o = Oblique(f[k]);
					if (!o.StartsWith(racine.CStr())) {
						continue;
					}
					const NkString rel(o.SubStr(racine.Length()));
					if (!HorsEmpreinte(rel)) {
						relatifs.PushBack(rel);
					}
				}
			}
			// L'ordre du systeme de fichiers n'est pas une garantie : on trie.
			NkSort(relatifs.Data(), relatifs.Data() + relatifs.Size(), &AvantTexte);
			uint64 h = FnvTexte(NK_FNV_BASE, NkString(NK_FORMAT_MOTEUR));
			NkVector<NkString> variables;
			for (usize i = 0; i < relatifs.Size(); ++i) {
				if (i > 0u && relatifs[i] == relatifs[i - 1u]) {
					continue; // un dossier inclus dans un autre
				}
				const NkVector<nk_uint8> octets = NkFile::ReadAllBytes((racine + relatifs[i]).CStr());
				h = FnvTexte(h, relatifs[i]);
				const uint64 n = static_cast<uint64>(octets.Size());
				h = Fnv(h, &n, sizeof(n));
				if (!octets.Empty()) {
					h = Fnv(h, octets.Data(), octets.Size());
				}
				++e.fichiers;
				e.octets += n;
				if (relatifs[i].EndsWith(".jenga")) {
					NkString texte;
					for (usize k = 0; k < octets.Size(); ++k) {
						texte.Append(static_cast<char>(octets[k]));
					}
					VariablesLues(texte, variables);
				}
			}
			NkSort(variables.Data(), variables.Data() + variables.Size(), &AvantTexte);
			for (usize i = 0; i < variables.Size(); ++i) {
				e.environnement += NkString::Format("%s%s=%s", i == 0u ? "" : " ; ", variables[i].CStr(), Env(variables[i].CStr()).CStr());
			}
			h = FnvTexte(h, e.environnement);
			h = FnvTexte(h, extra);
			e.valeur = h;
			e.hex = NkString::Format("%016llx", static_cast<unsigned long long>(h));
			return true;
		}

		bool NkEmpreinteDuMoteur(const NkString &depot, NkPlateformeJeu p, NkEmpreinteMoteur &e, NkString &erreur) {
			const NkString jenga = NkVersionJenga();
			if (jenga.Empty()) {
				erreur = NkString("`jenga --version` ne repond pas : l'empreinte du moteur porte la version de Jenga");
				return false;
			}
			NkString extra = NkString("jenga=") + jenga + "\nchaine=" + NkSignatureChaine(p) + "\nmodules=";
			const NkVector<NkString> &modules = NkModulesDuJoueur();
			for (usize i = 0; i < modules.Size(); ++i) {
				extra += modules[i] + ";";
			}
			return NkEmpreinteDesDossiers(depot, NkDossiersDuMoteur(), extra, e, erreur);
		}

		NkCacheMoteur NkCacheDuMoteur(const NkString &racine, const NkString &empreinte, const char *systeme,
									  const char *config) {
			NkCacheMoteur c;
			c.empreinte = empreinte;
			c.systeme = NkString(systeme);
			c.config = NkString(config);
			c.racine = Barre(racine) + empreinte + "/";
			const NkString cible = NkString::Format("%s-%s", systeme, config);
			c.dossier = c.racine + cible + "/";
			c.kit = c.dossier + "UnkenyMoteur.jenga";
			c.sceau = c.dossier + "moteur.txt";
			c.chantier = c.racine + "chantier-" + cible + "/";
			c.verrou = c.racine + cible + ".verrou";
			return c;
		}

		bool NkCacheScelle(const NkCacheMoteur &c, NkString &pourquoi) {
			if (!NkFile::Exists(c.sceau.CStr())) {
				pourquoi = NkString("pas de sceau (moteur.txt) : a construire");
				return false;
			}
			const NkString sceau = NkFile::ReadAllText(c.sceau.CStr());
			if (Champ(sceau, "empreinte") != c.empreinte) {
				pourquoi = NkString("sceau d'une autre empreinte : ") + Champ(sceau, "empreinte");
				return false;
			}
			if (!NkFile::Exists(c.kit.CStr())) {
				pourquoi = NkString("sceau present mais UnkenyMoteur.jenga absent");
				return false;
			}
			const NkString dossierLib = c.dossier + "lib/" + Champ(sceau, "cible") + "/";
			const NkString archives = Champ(sceau, "archives");
			NkString nom;
			uint32 n = 0u;
			for (usize i = 0; i <= archives.Length(); ++i) {
				if (i < archives.Length() && archives[i] != ' ') {
					nom.Append(archives[i]);
					continue;
				}
				if (!nom.Empty()) {
					if (!NkFile::Exists((dossierLib + nom).CStr())) {
						pourquoi = NkString("archive manquante : ") + dossierLib + nom;
						return false;
					}
					++n;
				}
				nom = NkString();
			}
			if (n == 0u) {
				pourquoi = NkString("sceau sans archive");
				return false;
			}
			pourquoi = NkString::Format("kit scelle, %u archives", static_cast<unsigned>(n));
			return true;
		}

		NkVector<NkString> NkProjetsDuMoteur() {
			NkVector<NkString> p;
			const NkVector<NkString> &modules = NkModulesDuJoueur();
			for (usize i = 0; i < modules.Size(); ++i) {
				const NkString &m = modules[i];
				const usize barre = m.RFind('/');
				NkString nom = barre == NkString::npos ? m : NkString(m.SubStr(barre + 1));
				if (nom.EndsWith(".jenga")) {
					nom = NkString(nom.SubStr(0, nom.Length() - 6));
				}
				p.PushBack(nom);
			}
			return p;
		}

		NkVector<NkString> NkLiensSystemeDuKit(const NkString &texteKit, const char *config, const char *systeme) {
			const usize bloc = texteKit.Find("KIT_SYSTEM_LIBS = {");
			if (bloc == NkString::npos) {
				return NkVector<NkString>();
			}
			const NkString cle = NkString::Format("(\"%s\", \"%s\"):", config, systeme);
			const usize ligne = texteKit.Find(cle.CStr(), bloc);
			return Chaines(texteKit, ligne, '\'');
		}

		NkVector<NkString> NkLiensSystemeManquants(const NkString &texteKit, const NkString &texteJoueur, const char *config) {
			const NkVector<NkString> kit = NkLiensSystemeDuKit(texteKit, config, "Windows");
			const NkVector<NkString> joueur = Chaines(texteJoueur, texteJoueur.Find("_WIN_LINKS = ["), '"');
			NkVector<NkString> manquants;
			for (usize i = 0; i < joueur.Size(); ++i) {
				if (joueur[i] == NkString("vulkan-1")) {
					continue;
				}
				bool present = false;
				for (usize k = 0; k < kit.Size() && !present; ++k) {
					present = EgalSansCasse(kit[k].CStr(), joueur[i].CStr());
				}
				if (!present) {
					manquants.PushBack(joueur[i]);
				}
			}
			return manquants;
		}

		bool NkScellerMoteur(const NkCacheMoteur &c, const NkString &depot, const char *config, const char *systeme,
							 const NkString &versionJenga, float64 duree, NkVector<NkString> &details) {
			if (!NkFile::Exists(c.kit.CStr())) {
				details.PushBack(NkString("[moteur] le kit n'a pas ecrit ") + c.kit);
				return false;
			}
			const NkString cible = NkString::Format("%s-%s", config, systeme);
			const NkString dossierLib = c.dossier + "lib/" + cible + "/";
			const NkVector<NkString> projets = NkProjetsDuMoteur();
			NkString archives;
			for (usize i = 0; i < projets.Size(); ++i) {
				const NkString a = Archive(projets[i], systeme);
				if (!NkFile::Exists((dossierLib + a).CStr())) {
					details.PushBack(NkString("[moteur] archive absente du kit : ") + dossierLib + a);
					return false;
				}
				archives += (i == 0u ? "" : " ") + a;
			}
			const NkString texteKit = NkFile::ReadAllText(c.kit.CStr());
			const NkVector<NkString> liens = NkLiensSystemeDuKit(texteKit, config, systeme);
			NkString listeLiens;
			for (usize i = 0; i < liens.Size(); ++i) {
				listeLiens += (i == 0u ? "" : " ") + liens[i];
			}
			details.PushBack(NkString::Format("[moteur] kit : %u archives, %u bibliotheques systeme transmises (%s)",
											  static_cast<unsigned>(projets.Size()), static_cast<unsigned>(liens.Size()),
											  listeLiens.CStr()));
			if (EgalSansCasse(systeme, "Windows")) {
				// Le joueur lie lui-meme ces bibliotheques : une qui manque au kit
				// est un module qui s'en sert sans la declarer -- tout autre
				// consommateur du kit ne lierait pas (d3dcompiler, 30/09).
				const NkString joueur = NkFile::ReadAllText((Barre(depot) + "Applications/UnkenyPlayer/UnkenyPlayer.jenga").CStr());
				const NkVector<NkString> manquants = NkLiensSystemeManquants(texteKit, joueur, config);
				NkString liste;
				for (usize i = 0; i < manquants.Size(); ++i) {
					liste += (i == 0u ? "" : ", ") + manquants[i];
				}
				details.PushBack(manquants.Empty() ? NkString("[moteur] liens systeme : tout ce que le joueur lie, le kit le transmet")
												   : NkString("[moteur] AVERTISSEMENT liens systeme que le joueur lie et qu'aucun module ne declare : ") + liste);
			}
			const NkDate maintenant = NkDate::GetCurrent();
			NkString sceau;
			sceau += NkString("empreinte: ") + c.empreinte + "\n";
			sceau += NkString("cible: ") + cible + "\n";
			sceau += NkString("jenga: ") + versionJenga + "\n";
			sceau += NkString("depot: ") + Oblique(depot) + "\n";
			sceau += NkString::Format("construit: %04d-%02d-%02d\n", static_cast<int>(maintenant.GetYear()),
									  static_cast<int>(maintenant.GetMonth()), static_cast<int>(maintenant.GetDay()));
			sceau += NkString::Format("duree: %.1f s\n", duree);
			sceau += NkString("archives: ") + archives + "\n";
			sceau += NkString("liens: ") + listeLiens + "\n";
			// En DERNIER et par renommage : un lecteur voit le sceau entier, ou rien.
			const NkString provisoire = c.sceau + ".provisoire";
			if (!NkFile::WriteAllText(provisoire.CStr(), sceau.CStr()) || !NkFile::Move(provisoire.CStr(), c.sceau.CStr())) {
				details.PushBack(NkString("[moteur] ecriture du sceau impossible : ") + c.sceau);
				return false;
			}
			// Les objets du chantier (200 Mo en Debug) ne servent plus : le kit a
			// les archives. Une autre empreinte aura son propre chantier.
			NkDirectory::Delete(c.chantier.CStr(), true);
			details.PushBack(NkString("[moteur] scelle : ") + c.dossier);
			return true;
		}

		// =====================================================================
		// LE VERROU
		// =====================================================================
		NkVerrouMoteur::~NkVerrouMoteur() {
			Liberer();
		}

		bool NkVerrouMoteur::Prendre(const NkString &chemin) {
			if (mTenu) {
				return true;
			}
			NkDirectory::CreateRecursive(NkPath(chemin.CStr()).GetParent().ToString().CStr());
#if defined(_WIN32)
			const int32 n = ::MultiByteToWideChar(CP_UTF8, 0, chemin.CStr(), -1, nullptr, 0);
			NkVector<wchar_t> large;
			large.Resize(static_cast<usize>(n > 0 ? n : 1));
			large[0] = L'\0';
			if (n > 0) {
				::MultiByteToWideChar(CP_UTF8, 0, chemin.CStr(), -1, large.Data(), n);
			}
			SECURITY_ATTRIBUTES sa{};
			sa.nLength = sizeof(sa);
			sa.bInheritHandle = TRUE; // un jenga lance pendant qu'on le tient le garde
			// Partage 0 : une seconde ouverture echoue (ERROR_SHARING_VIOLATION)
			// tant qu'une poignee vit, dans ce processus ou dans un autre.
			HANDLE h = ::CreateFileW(large.Data(), GENERIC_READ | GENERIC_WRITE, 0, &sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (h == INVALID_HANDLE_VALUE) {
				return false;
			}
			mPoignee = h;
#else
			const int fd = ::open(chemin.CStr(), O_RDWR | O_CREAT, 0644);
			if (fd < 0) {
				return false;
			}
			if (::flock(fd, LOCK_EX | LOCK_NB) != 0) {
				::close(fd);
				return false;
			}
			mFd = fd;
#endif
			mTenu = true;
			return true;
		}

		void NkVerrouMoteur::Liberer() {
			if (!mTenu) {
				return;
			}
#if defined(_WIN32)
			::CloseHandle(static_cast<HANDLE>(mPoignee));
			mPoignee = nullptr;
#else
			::flock(mFd, LOCK_UN);
			::close(mFd);
			mFd = -1;
#endif
			mTenu = false;
		}

	} // namespace editeur
} // namespace nkentseu
