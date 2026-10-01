#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkProjectLauncherProbe.h — la SONDE du lanceur de projets partage, sans
// fenetre ni GPU (NKEditorKitTest, famille 31).
//
// Elle ne regarde pas une image : elle REJOUE des gestes (l'entree plate des
// composants) et lit la DEMANDE rendue. Un lanceur qui dessine juste mais dont
// « Nouveau projet » ne rend rien est le defaut le plus couteux qui soit -- il
// a l'air de marcher.
//   31a  sans geste, aucune demande ; les trois projets sont visibles ;
//   31b  un clic au centre de « Nouveau projet » rend NouveauProjet ;
//   31c  un clic au centre de « Ouvrir... » rend Ouvrir ;
//   31d  la recherche filtre (nom ET dossier), sans casse ;
//   31e  le tri par nom ne perd ni ne double aucun projet ;
//   31f  incruste (sans colonne) + bouton d'hote : son clic rend ActionHote ;
//   31g  chaque glyphe de la bibliotheque dessine quelque chose ;
//   31h  la page de liens se peint et donne un contenu mesurable ;
//   31i  une capture sans fenetre s'ecrit (si un dossier est donne).
// =============================================================================

#include "NKEditorKit/NkProjectLauncherHost.h"
#include <cstdio>

namespace nkentseu {
	namespace editorkit {
		namespace lanceurprobe {

			struct Bilan {
					uint32 ok = 0;
					uint32 total = 0;
			};

			inline void Note(Bilan &b, bool ok, const char *quoi) {
				++b.total;
				if (ok)
					++b.ok;
				std::printf("  [%s] %s\n", ok ? " OK " : "ECHEC", quoi);
				std::fflush(stdout);
			}

			/// Un peintre qui COMPTE : tout est accepte, rien n'est dessine.
			class Compteur final : public NkComponentPaint {
				public:
					uint32 polygones = 0, aplats = 0, textes = 0;
					uint32 ColorOf(uint16) const override {
						return 0xFFFFFFFFu;
					}
					float32 LineHeight() const override {
						return 14.f;
					}
					float32 TextWidth(const char *s) const override {
						float32 w = 0.f;
						for (const char *c = s; c && *c; ++c)
							w += 7.f;
						return w;
					}
					void Fill(const NkPaintRect &, uint16, float32) override {
						++aplats;
					}
					void FillColor(const NkPaintRect &, uint32, float32) override {
						++aplats;
					}
					void Outline(const NkPaintRect &, uint16, uint16, float32) override {
						++aplats;
					}
					void OutlineSharp(const NkPaintRect &, uint16) override {}
					void HLine(float32, float32, float32, uint16) override {}
					void VLine(float32, float32, float32, uint16) override {}
					void Text(const NkPaintRect &, const char *, uint16, NkTextAlign) override {
						++textes;
					}
					void Icon(const NkPaintRect &, uint16, uint16) override {}
					bool PolygonHex(const float32 *, int32 count, uint32) override {
						if (count >= 3)
							++polygones;
						return true;
					}
					void PushClip(const NkPaintRect &) override {}
					void PopClip() override {}
			};

			inline NkProjectLauncherModel ModeleEssai() {
				NkProjectLauncherModel m;
				m.identite.nom = NkString("Essai");
				m.identite.prefixe = NkString("Es");
				m.identite.extensions = NkString(".essai");
				const char *noms[3] = {"Alpha", "beta", "Gamma"};
				const char *chemins[3] = {"C:/Projets/Alpha/alpha.essai", "C:/Projets/Beta/beta.essai",
										  "D:/Autres/Zeta/gamma.essai"};
				for (int32 i = 0; i < 3; ++i) {
					NkLanceurProjet p;
					p.nom = NkString(noms[i]);
					p.chemin = NkString(chemins[i]);
					p.date = NkString("2026-10-01");
					p.hote = (uint32)i;
					m.projets.PushBack(p);
				}
				NkLanceurModele md;
				md.nom = NkString("Vide");
				md.description = NkString("Un projet sans rien dedans.");
				m.modeles.PushBack(md);
				NkLanceurPage pp;
				pp.libelle = NkString("Projets");
				m.pages.PushBack(pp);
				NkLanceurPage pl;
				pl.libelle = NkString("Apprendre");
				pl.type = NkLanceurPageType::Liens;
				NkLanceurLien l;
				l.titre = NkString("Documentation");
				l.description = NkString("Tout le manuel, une page par commande, avec des exemples a recopier.");
				l.url = NkString("https://exemple.invalid/doc");
				pl.liens.PushBack(l);
				m.pages.PushBack(pl);
				return m;
			}

			inline NkComponentInput Clic(float32 x, float32 y) {
				NkComponentInput in;
				in.mouseX = x;
				in.mouseY = y;
				in.mousePressed = true;
				in.mouseDown = true;
				return in;
			}
			inline NkComponentInput Rien() {
				NkComponentInput in;
				in.mouseX = in.mouseY = -100000.f;
				return in;
			}

			/// `dossierCapture` non nul : 31i ecrit `lanceur_essai.png` dedans.
			inline Bilan Sonder(const char *dossierCapture = nullptr) {
				Bilan b;
				const NkPaintRect r{0.f, 0.f, 1600.f, 900.f};
				const NkProjectLauncherStyle s;
				const NkProjectLauncherHooks h;
				Compteur p;

				NkProjectLauncherModel m = ModeleEssai();
				NkProjectLauncherResult res = NkDrawProjectLauncher(p, Rien(), r, m, s, h);
				Note(b, res.action == NkLanceurAction::Aucune && res.projetsVisibles == 3,
					 "31a sans geste : aucune demande, 3 projets visibles");

				const NkPaintRect bn = res.boutonNouveau, bo = res.boutonOuvrir;
				res = NkDrawProjectLauncher(p, Clic(bn.x + bn.w * 0.5f, bn.y + bn.h * 0.5f), r, m, s, h);
				Note(b, bn.w > 0.f && res.action == NkLanceurAction::NouveauProjet,
					 "31b un clic sur « Nouveau » rend NouveauProjet");
				res = NkDrawProjectLauncher(p, Clic(bo.x + bo.w * 0.5f, bo.y + bo.h * 0.5f), r, m, s, h);
				Note(b, bo.w > 0.f && res.action == NkLanceurAction::Ouvrir, "31c un clic sur « Ouvrir... » rend Ouvrir");

				std::snprintf(m.filtre, sizeof(m.filtre), "%s", "ALP");
				const int32 v1 = NkDrawProjectLauncher(p, Rien(), r, m, s, h).projetsVisibles;
				std::snprintf(m.filtre, sizeof(m.filtre), "%s", "autres");
				const int32 v2 = NkDrawProjectLauncher(p, Rien(), r, m, s, h).projetsVisibles;
				std::snprintf(m.filtre, sizeof(m.filtre), "%s", "zzz");
				const int32 v3 = NkDrawProjectLauncher(p, Rien(), r, m, s, h).projetsVisibles;
				m.filtre[0] = 0;
				Note(b, v1 == 1 && v2 == 1 && v3 == 0, "31d la recherche filtre le nom ET le dossier, sans casse");

				m.tri = 1u;
				const int32 vt = NkDrawProjectLauncher(p, Rien(), r, m, s, h).projetsVisibles;
				m.tri = 0u;
				Note(b, vt == 3, "31e le tri par nom garde les 3 projets");

				NkProjectLauncherModel mi = ModeleEssai();
				mi.colonne = false;
				mi.actionHote = NkString("Accueil classique");
				res = NkDrawProjectLauncher(p, Rien(), r, mi, s, h);
				const NkPaintRect bh = res.boutonHote;
				res = NkDrawProjectLauncher(p, Clic(bh.x + bh.w * 0.5f, bh.y + bh.h * 0.5f), r, mi, s, h);
				Note(b, bh.w > 0.f && bh.x < r.w && res.action == NkLanceurAction::ActionHote,
					 "31f incruste : le bouton d'hote rend ActionHote");

				uint32 muets = 0;
				for (uint32 g = 1; g < (uint32)NkLanceurGlyphe::Count; ++g) {
					Compteur c;
					NkLanceurPeindreGlyphe(c, (NkLanceurGlyphe)g, {0.f, 0.f, 24.f, 24.f}, 0xFFFFFFFFu);
					if (c.polygones + c.aplats == 0u) {
						++muets;
						std::printf("         glyphe %u : rien de dessine\n", (unsigned)g);
					}
				}
				Note(b, muets == 0u, "31g chaque glyphe dessine quelque chose");

				m.page = 1;
				res = NkDrawProjectLauncher(p, Rien(), r, m, s, h);
				Note(b, m.page == 1 && res.contenuH > 0.f, "31h la page de liens se peint (contenu mesurable)");
				m.page = 0;

				if (dossierCapture && *dossierCapture) {
					(void)NkDirectory::CreateRecursive(dossierCapture);
					NkString c(dossierCapture);
					c.Append("/lanceur_essai.png");
					NkLanceurCaptureDesc d;
					d.chemin = c.CStr();
					NkProjectLauncherModel mc = ModeleEssai();
					char msg[512];
					const bool ok = NkLanceurCapturer(d, mc, NkTheme::Dark(), h, s, msg, (int32)sizeof(msg));
					std::printf("         %s\n", msg);
					Note(b, ok, "31i la capture sans fenetre s'ecrit");
				}
				return b;
			}

		} // namespace lanceurprobe
	} // namespace editorkit
} // namespace nkentseu
