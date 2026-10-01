//
// NkEditeurProjet.cpp
// =============================================================================
// Les reglages du projet (NkEditeurProjet.h).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurProjet.h"

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurContenu.h"
#include "Livraison/NkEditeurConstruire.h"

#include "NKContainers/Sequential/NkVector.h"
#include "NKEditorKit/Components/NkContentBrowserDisque.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"

#include <cstring>

namespace nkentseu {
	namespace editeur {

		namespace {
			NkString Fichier(NkEditeurModele &m) {
				NkString f = NkEditeurDossierProjet(m);
				f.Append(NK_PROJET_REGLAGES);
				return f;
			}

			/// Les lignes du fichier (sans fin de ligne).
			void Lignes(const NkString &texte, NkVector<NkString> &sortie) {
				sortie.Clear();
				const char *p = texte.CStr();
				while (p != nullptr && *p != '\0') {
					const char *f = p;
					while (*f != '\0' && *f != '\n') {
						++f;
					}
					usize n = static_cast<usize>(f - p);
					if (n > 0u && p[n - 1u] == '\r') {
						--n;
					}
					sortie.PushBack(NkString(p, n));
					p = *f == '\n' ? f + 1 : f;
				}
			}

			/// La ligne porte-t-elle la cle (« cle=... », espaces tolere avant « = ») ?
			bool EstCle(const NkString &ligne, const char *cle, usize &debutValeur) {
				const usize n = std::strlen(cle);
				if (ligne.Length() < n || std::strncmp(ligne.CStr(), cle, n) != 0) {
					return false;
				}
				usize i = n;
				while (i < ligne.Length() && ligne.CStr()[i] == ' ') {
					++i;
				}
				if (i >= ligne.Length() || ligne.CStr()[i] != '=') {
					return false;
				}
				++i;
				while (i < ligne.Length() && ligne.CStr()[i] == ' ') {
					++i;
				}
				debutValeur = i;
				return true;
			}

			/// Le chemin absolu normalise (« / »), pour comparer des dossiers.
			NkString Normal(const NkString &chemin) {
				NkString c = chemin;
				const bool abs = !c.Empty() && (c.CStr()[0] == '/' || c.CStr()[0] == '\\' || (c.Length() > 1u && c.CStr()[1] == ':'));
				if (!abs) {
					NkString r = NkDirectory::GetCurrentDirectory().ToString();
					r.Append('/');
					r.Append(c);
					c = r;
				}
				return editorkit::NkDisqueNormaliser(c.CStr());
			}
		} // namespace

		NkString NkEditeurProjetLire(NkEditeurModele &m, const char *cle) {
			const NkString f = Fichier(m);
			if (cle == nullptr || !NkFile::Exists(f.CStr())) {
				return NkString();
			}
			NkVector<NkString> lignes;
			Lignes(NkFile::ReadAllText(f.CStr()), lignes);
			for (uint32 i = 0; i < lignes.Size(); ++i) {
				usize v = 0;
				if (EstCle(lignes[i], cle, v)) {
					return NkString(lignes[i].CStr() + v);
				}
			}
			return NkString();
		}

		bool NkEditeurProjetEcrire(NkEditeurModele &m, const char *cle, const char *valeur) {
			if (cle == nullptr || cle[0] == '\0') {
				return false;
			}
			const NkString f = Fichier(m);
			NkVector<NkString> lignes;
			if (NkFile::Exists(f.CStr())) {
				Lignes(NkFile::ReadAllText(f.CStr()), lignes);
			} else {
				lignes.PushBack(NkString("# Unkeny - les reglages de ce projet (NkEditeurProjet.h). Une cle=valeur par ligne."));
			}
			NkString neuve(cle);
			neuve.Append('=');
			neuve.Append(valeur != nullptr ? valeur : "");
			bool remplacee = false;
			for (uint32 i = 0; i < lignes.Size(); ++i) {
				usize v = 0;
				if (EstCle(lignes[i], cle, v)) {
					lignes[i] = neuve;
					remplacee = true;
				}
			}
			if (!remplacee) {
				lignes.PushBack(neuve);
			}
			NkString texte;
			for (uint32 i = 0; i < lignes.Size(); ++i) {
				texte.Append(lignes[i]);
				texte.Append('\n');
			}
			return NkFile::WriteAllText(f.CStr(), texte.CStr());
		}

		bool NkEditeurProjetReel(NkEditeurModele &m) {
			// La scene de secours vit dans <AppData>/UnkenyEditor (NkEditeurChemin) :
			// son dossier n'est le projet de personne.
			const NkString secours = Normal((NkDirectory::GetAppDataDirectory() / "UnkenyEditor").ToString());
			const NkString projet = Normal(NkEditeurDossierProjet(m));
			return !secours.Empty() && !projet.StartsWith(secours.CStr());
		}

		NkString NkEditeurSortieDuProjet(NkEditeurModele &m) {
			if (!NkEditeurProjetReel(m)) {
				return NkSortieParDefaut();
			}
			const NkString retenue = NkEditeurProjetLire(m, NK_PROJET_SORTIE);
			const NkString projet = NkEditeurDossierProjet(m);
			if (retenue.Empty()) {
				return projet + "Construit";
			}
			// Relatif : sous le dossier du projet.
			const bool abs = retenue.CStr()[0] == '/' || retenue.CStr()[0] == '\\' || (retenue.Length() > 1u && retenue.CStr()[1] == ':');
			return abs ? retenue : projet + retenue;
		}

		bool NkEditeurRetenirSortie(NkEditeurModele &m, const char *sortie) {
			if (sortie == nullptr || sortie[0] == '\0' || !NkEditeurProjetReel(m)) {
				return false;
			}
			// SOUS le projet : relatif (le projet se deplace avec ses reglages).
			const NkString projet = Normal(NkEditeurDossierProjet(m));
			const NkString s = Normal(NkString(sortie));
			NkString valeur(sortie);
			if (!projet.Empty() && s.Length() > projet.Length() && s.StartsWith(projet.CStr()) &&
				(projet.CStr()[projet.Length() - 1u] == '/' || s.CStr()[projet.Length()] == '/')) {
				usize i = projet.Length();
				while (i < s.Length() && s.CStr()[i] == '/') {
					++i;
				}
				valeur = NkString(s.CStr() + i);
			}
			return NkEditeurProjetEcrire(m, NK_PROJET_SORTIE, valeur.CStr());
		}

	} // namespace editeur
} // namespace nkentseu
