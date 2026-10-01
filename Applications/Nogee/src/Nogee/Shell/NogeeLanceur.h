#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NogeeLanceur.h — LE LANCEUR DE NOGEE, PRET A BRANCHER (2026-10-01).
//
// ⚠️ IL N'EST PAS ENCORE BRANCHE, ET C'EST VOULU. L'interface de Nogee est en
//    travaux chez un autre agent (branche nogee/comme-unkeny) : ce fichier ne
//    touche a RIEN de Nogee. Il porte la TOUCHE de Nogee pour le lanceur de
//    projets partage (NKEditorKit, NkProjectLauncherShell.h) et UNE fonction
//    a appeler. Il ne depend que du kit : il compile seul (NKEditorKitTest
//    l'inclut et le photographie, famille 31).
//
// LE BRANCHEMENT (apres la fusion), dans Nogee.cpp, apres le montage de la
// coquille et avant `shell->Run()` :
//
//     #include "Nogee/Shell/NogeeLanceur.h"
//     nogee::NogeeLanceurPortes portes;
//     portes.ouvrirProjet = &MonOuvrirProjet;  // bool (void *, const NkString &chemin)
//     portes.creerProjet  = &MonCreerProjet;   // bool (void *, int32 modele)
//     portes.modeleBranche = &MonModeleBranche; // bool (void *, int32 modele) -- facultatif
//     portes.user = &monEtat;
//     nogee::NogeeBrancherLanceur(*shell, portes);
//
// et, en tete de nkmain, la photo sans fenetre :
//
//     bool clair = false;
//     const NkString cap = editorkit::NkLanceurCaptureDemandee(argc, argv, &clair);
//     if (!cap.Empty()) return nogee::NogeeCapturerLanceur(cap, clair);
//
// CE QU'IL MONTRE (spec. « Aetherion », design/01-specification-humaine.md §3) :
// la colonne Bibliotheque / Marketplace / Apprendre / Communaute / Installations,
// « + Nouveau projet », la recherche, la grille de cartes projets 16:9, et les
// modeles du §3.3 (Vide 3D, Troisieme personne, Premiere personne, Vehicule,
// Vide 2D, Vue de dessus 2D). Un modele que `modeleBranche` ne declare pas se
// montre « a brancher » -- jamais un faux bouton.
// =============================================================================

#include "NKEditorKit/NkProjectLauncherShell.h"
#include "NKWindow/Core/NkDialogs.h"
#include "NKWindow/Core/NkLauncher.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkPath.h"
#include <cstdio>

namespace nkentseu {
	namespace nogee {

		/// Les portes de Nogee : ce que le lanceur demande, Nogee le fait.
		struct NogeeLanceurPortes {
				/// Ouvre le projet (fichier ou dossier). VRAI = ouvert (le lanceur se ferme).
				bool (*ouvrirProjet)(void *user, const NkString &chemin) = nullptr;
				/// Cree un projet depuis le modele `modele` (indice des modeles ci-dessous).
				bool (*creerProjet)(void *user, int32 modele) = nullptr;
				/// Ce modele est-il branche ? Nul = aucun (tous « a brancher »).
				bool (*modeleBranche)(void *user, int32 modele) = nullptr;
				void *user = nullptr;
		};

		/// LA TOUCHE de Nogee.
		inline void NogeeRemplirLanceur(editorkit::NkProjectLauncherModel &m, const NogeeLanceurPortes *portes) {
			using namespace editorkit;
			using G = NkLanceurGlyphe;
			m.identite.nom = NkString("Nogee");
			m.identite.prefixe = NkString("Noge");
			m.identite.sousTitre = NkString("Editeur du moteur Noge — Nkentseu");
			m.identite.version = NkString("0.1.0");
			m.identite.extensions = NkString(".nogee");
			m.identite.glyphe = G::Moteur;
			m.modeles.Clear();
			struct D {
					const char *nom, *cat, *desc;
					G g;
					uint32 teinte;
			};
			const D kModeles[] = {
				{"Vide 3D", "JEUX", "Une scene 3D vide : un sol, une lumiere, une camera.", G::Cube, 0u},
				{"Troisieme personne", "JEUX", "Un personnage suivi par la camera, pret a courir et sauter.",
				 G::Personnage, 0x2EA06BFFu},
				{"Premiere personne", "JEUX", "La camera dans les yeux du joueur : viser, marcher, interagir.",
				 G::Camera, 0xD94F4FFFu},
				{"Vehicule", "JEUX", "Un vehicule pilotable sur un terrain d'essai.", G::Manette, 0xE0A32EFFu},
				{"Vide 2D", "JEUX", "Une scene 2D vide, camera orthographique.", G::Vide, 0x8E5BE8FFu},
				{"Vue de dessus 2D", "JEUX", "Un personnage vu de dessus sur une carte a tuiles.", G::Paysage,
				 0x2E8ED8FFu},
			};
			int32 i = 0;
			for (const D &d : kModeles) {
				NkLanceurModele md;
				md.nom = NkString(d.nom);
				md.categorie = NkString(d.cat);
				md.description = NkString(d.desc);
				md.glyphe = d.g;
				md.couleur = d.teinte;
				md.disponible = portes && portes->modeleBranche && portes->modeleBranche(portes->user, i);
				if (!md.disponible)
					md.raison = NkString("a brancher");
				m.modeles.PushBack(md);
				++i;
			}
			m.pages.Clear();
			auto page = [&](const char *lib, const char *st, G g, NkLanceurPageType t) -> NkLanceurPage & {
				NkLanceurPage p;
				p.libelle = NkString(lib);
				p.sousTitre = NkString(st);
				p.glyphe = g;
				p.type = t;
				m.pages.PushBack(p);
				return m.pages[m.pages.Size() - 1u];
			};
			auto lien = [](NkLanceurPage &pg, const char *t, const char *d, G g, const char *badge, bool dispo) {
				NkLanceurLien l;
				l.titre = NkString(t);
				l.description = NkString(d);
				l.glyphe = g;
				l.badge = NkString(badge);
				l.disponible = dispo;
				pg.liens.PushBack(l);
			};
			(void)page("Bibliotheque", "", G::Projets, NkLanceurPageType::Projets);
			NkLanceurPage &mk = page("Marketplace", "Environnements, personnages, greffons.", G::Etoile,
									 NkLanceurPageType::Liens);
			lien(mk, "Environnements", "Decors complets a deposer dans une scene.", G::Paysage, "a venir", false);
			lien(mk, "Personnages", "Personnages animes, squelettes et clips.", G::Personnage, "a venir", false);
			lien(mk, "Greffons", "Outils et extensions de l'editeur.", G::Reglages, "a venir", false);
			NkLanceurPage &ap = page("Apprendre", "Les gestes de l'editeur, du niveau au jeu livre.", G::Apprendre,
									 NkLanceurPageType::Liens);
			lien(ap, "Premiers pas", "Placer des acteurs, eclairer, jouer dans l'editeur.", G::Apprendre, "a venir", false);
			lien(ap, "Blueprints et C++", "Scripter le jeu : graphes visuels et code.", G::Code, "a venir", false);
			NkLanceurPage &co = page("Communaute", "Partager, demander, montrer.", G::Communaute, NkLanceurPageType::Liens);
			lien(co, "Forum", "Les createurs de jeux Nkentseu.", G::Communaute, "a venir", false);
			NkLanceurPage &in = page("Installations", "Les versions du moteur sur cette machine.", G::Installations,
									 NkLanceurPageType::Liens);
			{
				NkLanceurLien l;
				l.titre = NkString("Nogee 0.1.0");
				l.description = NkString("Installe dans ") + NkPath::GetExecutableDirectory().ToString();
				l.glyphe = G::Moteur;
				l.badge = NkString("installe");
				in.liens.PushBack(l);
			}
			lien(in, "Installer une version", "Choisir et installer une autre version du moteur.", G::Installations,
				 "a venir", false);
			m.astuce = NkString("Astuce : double-cliquez une carte pour ouvrir le projet.");
		}

		struct NogeeLanceurEtat {
				editorkit::NkLanceurCoquille lanceur;
				editorkit::NkLanceurRecents recents;
				NogeeLanceurPortes portes;
		};
		inline NogeeLanceurEtat &NogeeLanceur() {
			static NogeeLanceurEtat e;
			return e;
		}

		/// LA fonction a appeler (cf. l'en-tete). Montre le lanceur au demarrage.
		inline void NogeeBrancherLanceur(editorkit::NkEditorShell &shell, const NogeeLanceurPortes &portes) {
			using namespace editorkit;
			NogeeLanceurEtat &E = NogeeLanceur();
			E.portes = portes;
			NogeeRemplirLanceur(E.lanceur.modele, &E.portes);
			E.recents.Charger("Nogee");
			E.lanceur.themeParCoquille = true;
			E.lanceur.rafraichir = [](void *, NkLanceurCoquille &l) { NogeeLanceur().recents.Remplir(l.modele); };
			E.lanceur.agir = [](void *, NkLanceurCoquille &l, const NkProjectLauncherResult &r) -> bool {
				NogeeLanceurEtat &S = NogeeLanceur();
				const NogeeLanceurPortes &P = S.portes;
				const int32 i = r.index;
				const bool projetValide = i >= 0 && (usize)i < l.modele.projets.Size();
				auto ouvrir = [&](const NkString &c) {
					if (!P.ouvrirProjet || !P.ouvrirProjet(P.user, c)) {
						l.modele.erreur = NkString("Ouverture impossible : ") + c;
						return false;
					}
					S.recents.Toucher(c, NkPath(c.CStr()).GetFileNameWithoutExtension(), NkLanceurAujourdhui());
					return true;
				};
				switch (r.action) {
					case NkLanceurAction::NouveauProjet:
					case NkLanceurAction::NouveauDepuisModele: {
						const int32 k = r.action == NkLanceurAction::NouveauProjet ? 0 : i;
						if (k < 0 || (usize)k >= l.modele.modeles.Size() || !l.modele.modeles[(usize)k].disponible ||
							!P.creerProjet)
							return false;
						return P.creerProjet(P.user, k);
					}
					case NkLanceurAction::Ouvrir: {
						const NkDialogResult d = NkDialogs::OpenFileDialog("*.*", "Ouvrir un projet Nogee");
						return d.confirmed && !d.path.Empty() && ouvrir(d.path);
					}
					case NkLanceurAction::OuvrirRecent:
						return projetValide && l.modele.projets[(usize)i].etat == 0u &&
							   ouvrir(l.modele.projets[(usize)i].chemin);
					case NkLanceurAction::Epingler:
						if (projetValide)
							S.recents.BasculerEpingle((usize)l.modele.projets[(usize)i].hote);
						return false;
					case NkLanceurAction::Retirer:
						if (projetValide)
							S.recents.Retirer((usize)l.modele.projets[(usize)i].hote);
						return false;
					case NkLanceurAction::Purger:
						for (isize k = (isize)S.recents.entrees.Size() - 1; k >= 0; --k)
							if (!NkFile::Exists(S.recents.entrees[(usize)k].chemin.CStr()))
								S.recents.Retirer((usize)k);
						return false;
					case NkLanceurAction::OuvrirLien:
						if (r.url && *r.url)
							NkLauncher::OpenURL(r.url);
						return false;
					default:
						return false;
				}
			};
			E.lanceur.Brancher(shell);
		}

		/// `--capture-lanceur=FICHIER.png [--theme-lanceur=clair]` : la photo sans
		/// fenetre ni GPU, par le MEME composant et le MEME modele.
		inline int NogeeCapturerLanceur(const NkString &chemin, bool clair) {
			using namespace editorkit;
			static NkProjectLauncherModel m;
			NogeeRemplirLanceur(m, nullptr);
			NkLanceurRecents rec;
			rec.Charger("Nogee");
			rec.Remplir(m);
			m.themeBasculable = true;
			m.themeSombre = !clair;
			NkLanceurCaptureDesc d;
			d.chemin = chemin.CStr();
			char msg[512];
			const bool ok = NkLanceurCapturer(d, m, clair ? NkTheme::Light() : NkTheme::Dark(), NkProjectLauncherHooks(),
											  NkProjectLauncherStyle(), msg, (int32)sizeof(msg));
			std::printf("[capture-lanceur] Nogee : %s\n", msg);
			std::fflush(stdout);
			return ok ? 0 : 1;
		}

	} // namespace nogee
} // namespace nkentseu
