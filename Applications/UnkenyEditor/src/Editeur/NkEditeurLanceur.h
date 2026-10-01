#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkEditeurLanceur.h — LE LANCEUR D'UNKENYEDITOR, PRET A BRANCHER (2026-10-01).
//
// C'est aussi la PREMIERE PIECE DU LAUNCHER DU MOTEUR (R23 de
// design/00-feuille-de-route.md : « projets, versions, modeles, actualites »).
//
// ⚠️ IL N'EST PAS ENCORE BRANCHE, ET C'EST VOULU. L'interface d'UnkenyEditor est
//    en travaux chez d'autres agents : ce fichier ne touche a RIEN de l'editeur.
//    Il porte la TOUCHE d'Unkeny pour le lanceur de projets partage (NKEditorKit,
//    NkProjectLauncherHost.h) et un HOTE a appeler dans l'image. Il ne depend que
//    du kit : il compile seul (NKEditorKitTest l'inclut et le photographie).
//
// UnkenyEditor n'est pas sur NkEditorShell (NkCanvasGuiApp) : il heberge donc
// le composant comme NKCraft et PV3DE, dans SA boucle :
//
//     #include "Editeur/NkEditeurLanceur.h"
//     static unkeny::NkEditeurLanceurHote lanceur;      // une fois
//     lanceur.portes.ouvrirProjet = ...;                // bool (void *, const NkString &)
//     lanceur.portes.creerProjet  = ...;                // bool (void *, int32 modele)
//     lanceur.portes.modeleBranche = ...;               // bool (void *, int32) -- facultatif
//     lanceur.televerser = ...;                         // bool (void *, uint32 tex, const uint8 *gris, int32 w, int32 h)
//     // dans l'image, AVANT les panneaux, tant que lanceur.actif :
//     if (lanceur.actif) { lanceur.Peindre(ctx, mTheme, echelle, {0, haut, W, H - haut}); return; }
//
// et, en tete de main, la photo sans fenetre :
//     if (!cap.Empty()) return unkeny::NkEditeurCapturerLanceur(cap, clair);
//
// LA TOUCHE : la GELEE (le logo d'Unkeny, NkFabriquerIcones : vert #3DDC97 sur
// #141414), les genres de jeux de la vision d'Unkeny, les deux scenes d'exemple
// qui existent deja (--exemple=nuit, --exemple=hud), la page Installations qui
// recevra les versions du moteur.
// =============================================================================

#include "NKEditorKit/NkProjectLauncherHost.h"
#include "NKWindow/Core/NkDialogs.h"
#include "NKWindow/Core/NkLauncher.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkPath.h"
#include "NKMath/NkColor.h"
#include <cstdio>

namespace nkentseu {
	namespace unkeny {

		/// LE LOGO : la gelee d'Unkeny, par le peintre du kit (memes couleurs que
		/// l'icone d'application de NkFabriquerIcones, theme sombre).
		inline void NkEditeurPeindreGelee(void *, editorkit::NkComponentPaint &p, const editorkit::NkPaintRect &r) {
			const float32 s = r.w < r.h ? r.w : r.h;
			const float32 x = r.x + (r.w - s) * 0.5f, y = r.y + (r.h - s) * 0.5f;
			p.FillColor({x, y, s, s}, math::NkColor(0x14, 0x14, 0x14).ToUint32A(), s * 0.24f);
			// Le corps : un dome (demi-disque) pose sur une base arrondie.
			const uint32 vert = math::NkColor(0x3D, 0xDC, 0x97).ToUint32A();
			float32 q[2 * 26];
			int32 n = 0;
			const float32 cx = x + s * 0.5f, cy = y + s * 0.56f, rr = s * 0.30f;
			for (int32 i = 0; i <= 24; ++i) {
				const float32 a = 3.1415927f + 3.1415927f * (float32)i / 24.f;
				q[n * 2] = cx + rr * math::NkCos(a);
				q[n * 2 + 1] = cy + rr * math::NkSin(a);
				++n;
			}
			(void)p.PolygonHex(q, n, vert);
			p.FillColor({cx - rr, cy - 1.f, rr * 2.f, s * 0.16f}, vert, s * 0.06f);
			// Le reflet et les deux yeux.
			p.FillColor({cx - rr * 0.55f, cy - rr * 0.62f, rr * 0.36f, rr * 0.16f},
						math::NkColor(0xE8, 0xFF, 0xF4).ToUint32A(), rr * 0.08f);
			const uint32 oeil = math::NkColor(0x10, 0x2A, 0x20).ToUint32A();
			p.FillColor({cx - rr * 0.42f, cy - rr * 0.10f, rr * 0.20f, rr * 0.28f}, oeil, rr * 0.10f);
			p.FillColor({cx + rr * 0.22f, cy - rr * 0.10f, rr * 0.20f, rr * 0.28f}, oeil, rr * 0.10f);
		}

		struct NkEditeurLanceurPortes {
				bool (*ouvrirProjet)(void *user, const NkString &chemin) = nullptr;
				bool (*creerProjet)(void *user, int32 modele) = nullptr;
				bool (*modeleBranche)(void *user, int32 modele) = nullptr;
				void *user = nullptr;
		};

		/// LA TOUCHE d'Unkeny.
		inline void NkEditeurRemplirLanceur(editorkit::NkProjectLauncherModel &m, const NkEditeurLanceurPortes *portes) {
			using namespace editorkit;
			using G = NkLanceurGlyphe;
			m.identite.nom = NkString("UnkenyEditor");
			m.identite.prefixe = NkString("Unkeny");
			m.identite.sousTitre = NkString("Le moteur de jeux de Nkentseu");
			m.identite.version = NkString("0.1.0");
			m.identite.extensions = NkString(".nkscene");
			m.identite.accent = math::NkColor(0x1E, 0x9E, 0x6A).ToUint32A();
			m.identite.glyphe = G::Manette;
			m.modeles.Clear();
			struct D {
					const char *nom, *cat, *desc;
					G g;
					uint32 teinte;
			};
			const D kModeles[] = {
				{"Jeu 2D vide", "2D", "Une scene 2D vide, une camera, une lumiere.", G::Vide, 0u},
				{"Plateforme 2D", "2D", "Un personnage qui court et saute sur des plateformes.", G::Personnage,
				 0x2E8ED8FFu},
				{"Vue de dessus", "2D", "Un heros vu de dessus dans une carte a tuiles.", G::Paysage, 0xE0A32EFFu},
				{"Jeu 3D vide", "3D", "Un sol, un ciel, une camera libre.", G::Cube, 0x8E5BE8FFu},
				{"Scene de nuit", "EXEMPLE", "Le feu de camp sous la lune : lumieres et ombres.", G::Lune,
				 0x5C8EE0FFu},
				{"HUD ancre", "EXEMPLE", "Quatre elements d'interface ancres aux coins de l'ecran.", G::Calques,
				 0xD94F4FFFu},
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
			(void)page("Projets", "", G::Projets, NkLanceurPageType::Projets);
			NkLanceurPage &ap = page("Apprendre", "Du premier niveau au jeu livre.", G::Apprendre, NkLanceurPageType::Liens);
			lien(ap, "Premier niveau", "Placer des formes, des acteurs, une camera.", G::Apprendre, "a venir", false);
			lien(ap, "Livrer un jeu", "Construire pour Windows, le Web et le mobile.", G::Installations, "a venir", false);
			NkLanceurPage &ac = page("Actualites", "Les nouveautes du moteur.", G::Etoile, NkLanceurPageType::Liens);
			lien(ac, "Journal des versions", "Ce qui change d'une version a l'autre.", G::Document, "a venir", false);
			NkLanceurPage &co = page("Communaute", "Partager ses jeux et ses questions.", G::Communaute,
									 NkLanceurPageType::Liens);
			lien(co, "Forum", "Les createurs de jeux Unkeny.", G::Communaute, "a venir", false);
			NkLanceurPage &in = page("Installations", "Les versions du moteur sur cette machine.", G::Installations,
									 NkLanceurPageType::Liens);
			{
				NkLanceurLien l;
				l.titre = NkString("UnkenyEditor 0.1.0");
				l.description = NkString("Installe dans ") + NkPath::GetExecutableDirectory().ToString();
				l.glyphe = G::Moteur;
				l.badge = NkString("installe");
				in.liens.PushBack(l);
			}
			lien(in, "Installer une version", "Les versions du moteur, cote a cote.", G::Installations, "a venir", false);
			m.astuce = NkString("Astuce : UnkenyPlayer --jeu=DOSSIER joue un jeu cuit, sans l'editeur.");
		}

		/// L'HOTE dans la boucle d'UnkenyEditor (NkCanvasGuiApp).
		struct NkEditeurLanceurHote {
				editorkit::NkProjectLauncherModel modele;
				editorkit::NkLanceurPolices polices;
				editorkit::NkLanceurRecents recents;
				NkEditeurLanceurPortes portes;
				/// Televerse un atlas de police gris (le dorsal de l'application).
				bool (*televerser)(void *user, uint32 texId, const uint8 *gris, int32 w, int32 h) = nullptr;
				void *userTeleverser = nullptr;
				bool actif = true;
				bool pret = false;

				/// Peint le lanceur dans `r` et agit. VRAI = un projet est ouvert
				/// (`actif` passe a faux : l'editeur reprend la main).
				bool Peindre(nkgui::NkGuiContext &ctx, const editorkit::NkTheme &theme, float32 echelle,
							 const editorkit::NkPaintRect &r) {
					using namespace editorkit;
					if (!pret) {
						pret = true;
						NkEditeurRemplirLanceur(modele, &portes);
						recents.Charger("UnkenyEditor");
					}
					if (polices.echelle != echelle) {
						(void)polices.Charger(echelle, nullptr);
						const nkgui::NkGuiFont *f[3] = {&polices.titre, &polices.intertitre, &polices.petite};
						for (const nkgui::NkGuiFont *x : f)
							if (televerser && x->Valid() && x->pixels)
								(void)televerser(userTeleverser, x->TexId(), x->pixels, x->atlasW, x->atlasH);
					}
					recents.Remplir(modele);
					modele.themeSombre = theme.IsDark();
					NkProjectLauncherHooks h;
					h.peindreLogo = &NkEditeurPeindreGelee;
					const NkProjectLauncherResult res =
						NkLanceurPeindre(ctx, theme, polices, r, modele, NkProjectLauncherStyle(), h, echelle, true);
					if (Agir(res))
						actif = false;
					return !actif;
				}

				bool Agir(const editorkit::NkProjectLauncherResult &r) {
					using namespace editorkit;
					const int32 i = r.index;
					const bool projetValide = i >= 0 && (usize)i < modele.projets.Size();
					auto ouvrir = [&](const NkString &c) {
						if (!portes.ouvrirProjet || !portes.ouvrirProjet(portes.user, c)) {
							modele.erreur = NkString("Ouverture impossible : ") + c;
							return false;
						}
						recents.Toucher(c, NkPath(c.CStr()).GetFileNameWithoutExtension(), NkLanceurAujourdhui());
						return true;
					};
					switch (r.action) {
						case NkLanceurAction::NouveauProjet:
						case NkLanceurAction::NouveauDepuisModele: {
							const int32 k = r.action == NkLanceurAction::NouveauProjet ? 0 : i;
							if (k < 0 || (usize)k >= modele.modeles.Size() || !modele.modeles[(usize)k].disponible ||
								!portes.creerProjet)
								return false;
							return portes.creerProjet(portes.user, k);
						}
						case NkLanceurAction::Ouvrir: {
							const NkDialogResult d = NkDialogs::OpenFileDialog("*.nkscene", "Ouvrir une scene Unkeny");
							return d.confirmed && !d.path.Empty() && ouvrir(d.path);
						}
						case NkLanceurAction::OuvrirRecent:
							return projetValide && modele.projets[(usize)i].etat == 0u &&
								   ouvrir(modele.projets[(usize)i].chemin);
						case NkLanceurAction::Epingler:
							if (projetValide)
								recents.BasculerEpingle((usize)modele.projets[(usize)i].hote);
							return false;
						case NkLanceurAction::Retirer:
							if (projetValide)
								recents.Retirer((usize)modele.projets[(usize)i].hote);
							return false;
						case NkLanceurAction::Purger:
							for (isize k = (isize)recents.entrees.Size() - 1; k >= 0; --k)
								if (!NkFile::Exists(recents.entrees[(usize)k].chemin.CStr()))
									recents.Retirer((usize)k);
							return false;
						case NkLanceurAction::OuvrirLien:
							if (r.url && *r.url)
								NkLauncher::OpenURL(r.url);
							return false;
						default:
							return false;
					}
				}
		};

		/// `--capture-lanceur=FICHIER.png [--theme-lanceur=clair]` : la photo sans
		/// fenetre ni GPU, par le MEME composant et le MEME modele.
		inline int NkEditeurCapturerLanceur(const NkString &chemin, bool clair) {
			using namespace editorkit;
			static NkProjectLauncherModel m;
			NkEditeurRemplirLanceur(m, nullptr);
			NkLanceurRecents rec;
			rec.Charger("UnkenyEditor");
			rec.Remplir(m);
			m.themeSombre = !clair;
			NkProjectLauncherHooks h;
			h.peindreLogo = &NkEditeurPeindreGelee;
			NkLanceurCaptureDesc d;
			d.chemin = chemin.CStr();
			char msg[512];
			const bool ok = NkLanceurCapturer(d, m, clair ? NkTheme::Light() : NkTheme::Dark(), h, NkProjectLauncherStyle(),
											  msg, (int32)sizeof(msg));
			std::printf("[capture-lanceur] UnkenyEditor : %s\n", msg);
			std::fflush(stdout);
			return ok ? 0 : 1;
		}

	} // namespace unkeny
} // namespace nkentseu
