#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkProjectLauncherHost.h — le LANCEUR DE PROJETS dans le monde NKGui.
//
// Le composant (Components/NkProjectLauncherModel.h) ne connait que le peintre
// du kit. Ce fichier est ce que TOUTE application NKGui ecrirait pour l'heberger
// -- ecrit une fois ici, pour qu'aucune ne le recopie :
//   - `NkLanceurPolices` : les trois tailles du lanceur (titre, intertitre,
//     petit texte), chargees en Inter et TELEVERSEES par le renderer de l'hote ;
//   - `NkLanceurPeindre` : peintre NKGui + entree NKGui -> composant, puis le
//     VRAI champ de saisie de la recherche (NkOverlayTextField) quand le
//     composant a pose le focus ;
//   - `NkLanceurCapturer` : la PHOTO SANS FENETRE NI GPU du lanceur d'une
//     application, par le meme composant et le meme peintre, rasterisee par
//     NkEditorRendererMemoire -- une capture par application, a la demande.
//   - `NkLanceurRecents` : une liste de projets recents sur disque, pour les
//     applications qui n'en avaient pas encore (NkAnimaEditor, PV3DE...).
//
// ⚠️ IDENTIFIANTS DE TEXTURE : les polices du lanceur prennent 0x4E4C0000..+2
//    ('NL'), loin de la police d'interface (0x4E4B4654, 'NKFT', et ses voisines
//    +3/+4/+16..) et des images de la coquille (0x4E4B0100 et suivants).
// =============================================================================

#include "NKEditorKit/Components/NkProjectLauncherModel.h"
#include "NKEditorKit/Components/NkGuiComponentPaint.h"
#include "NKEditorKit/NkEditorTextField.h"
#include "NKEditorKit/NkEditorRendererMemoire.h"
#include "NKEditorKit/NkIEditorRenderer.h"
#include "NKEditorKit/NkTheme.h"
#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiFont.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkPath.h"
#include "NKContainers/String/NkString.h"
#include <cstdio>
#include <cstdlib>

namespace nkentseu {
	namespace editorkit {

		static const uint32 kNkLanceurTexPolices = 0x4E4C0000u;

		// ── LES POLICES ─────────────────────────────────────────────────────────
		struct NkLanceurPolices {
				nkgui::NkGuiFont titre, intertitre, petite;
				float32 echelle = 0.f;

				/// Charge les trois tailles a `echelle` (DPI x densite de l'hote) et
				/// les televerse par `r` (nul = seulement en memoire, pour une sonde).
				/// Rechargeable : un changement d'echelle refait les atlas.
				bool Charger(float32 ech, NkIEditorRenderer *r, uint32 texBase = kNkLanceurTexPolices) {
					if (ech <= 0.f)
						ech = 1.f;
					echelle = ech;
					auto px = [&](float32 v) { return (float32)(int32)(v * ech + 0.5f); };
					const bool a = titre.LoadEmbedded(NkEmbeddedFontId::Inter, px(25.f), false);
					const bool b = intertitre.LoadEmbedded(NkEmbeddedFontId::Inter, px(16.f), false);
					const bool c = petite.LoadEmbedded(NkEmbeddedFontId::Inter, px(12.f), false);
					titre.texId = texBase;
					intertitre.texId = texBase + 1u;
					petite.texId = texBase + 2u;
					if (r) {
						if (a)
							r->UploadFontGray8(titre.TexId(), titre.pixels, titre.atlasW, titre.atlasH);
						if (b)
							r->UploadFontGray8(intertitre.TexId(), intertitre.pixels, intertitre.atlasW,
											   intertitre.atlasH);
						if (c)
							r->UploadFontGray8(petite.TexId(), petite.pixels, petite.atlasW, petite.atlasH);
					}
					return a && b && c;
				}
				bool Pretes() const {
					return echelle > 0.f;
				}
				void Poser(NkGuiComponentPaint &pc) const {
					pc.PoserPolicesLanceur(&titre, &intertitre, &petite);
				}
		};

		/// L'entree NKGui -> l'entree plate des composants.
		inline NkComponentInput NkLanceurEntree(const nkgui::NkGuiInput &in, float32 echelle) {
			NkComponentInput ci;
			ci.surfaceScale = echelle > 0.f ? echelle : 1.f;
			ci.mouseX = in.mousePos.x;
			ci.mouseY = in.mousePos.y;
			ci.wheel = in.wheel;
			ci.mouseDown = in.mouseDown[0];
			ci.mousePressed = in.mouseClicked[0];
			ci.mouseReleased = in.mouseReleased[0];
			ci.doubleClick = in.mouseDoubleClicked[0];
			ci.rightPressed = in.mouseClicked[1];
			ci.ctrl = in.ctrlDown;
			ci.shift = in.shiftDown;
			ci.alt = in.altDown;
			return ci;
		}

		/// Peint le lanceur dans `r` (NKGui) et rend la demande de l'utilisateur.
		/// `entree` faux = rien n'est entendu (une modale de l'hote est ouverte).
		inline NkProjectLauncherResult NkLanceurPeindre(nkgui::NkGuiContext &ctx, const NkTheme &theme,
														const NkLanceurPolices &pol, const NkPaintRect &r,
														NkProjectLauncherModel &m,
														const NkProjectLauncherStyle &s = NkProjectLauncherStyle(),
														const NkProjectLauncherHooks &h = NkProjectLauncherHooks(),
														float32 echelle = 1.f, bool entree = true) {
			NkGuiComponentPaint pc(ctx, theme);
			pol.Poser(pc);
			NkComponentInput ci = NkLanceurEntree(ctx.input, echelle);
			if (!entree) {
				ci = NkComponentInput();
				ci.surfaceScale = echelle > 0.f ? echelle : 1.f;
				ci.mouseX = ci.mouseY = -100000.f;
				m.rechercheFocus = false;
			}
			NkProjectLauncherResult res = NkDrawProjectLauncher(pc, ci, r, m, s, h);
			// LA RECHERCHE : le vrai champ (caret, selection, coller), la ou le
			// composant a reserve la place. Entree ou Echap rendent la main.
			if (entree && m.rechercheFocus && res.recherche.w > 4.f) {
				NkOverlayFieldStyle st;
				st.fond = false;
				st.bord = false;
				st.utf8 = true;
				const uint32 ct = theme.Get(NkRole::Text);
				st.texte = nkgui::NkColor{(uint8)((ct >> 24) & 0xFFu), (uint8)((ct >> 16) & 0xFFu),
										  (uint8)((ct >> 8) & 0xFFu), (uint8)(ct & 0xFFu)};
				NkOverlayTextField(ctx, ctx.DL(), ctx.font,
								   nkgui::NkRect{res.recherche.x, res.recherche.y, res.recherche.w, res.recherche.h},
								   m.filtre, (int32)sizeof(m.filtre), true, &st);
				if (ctx.input.KeyPressed(nkgui::NkGuiKey::Escape) || ctx.input.KeyPressed(nkgui::NkGuiKey::Enter))
					m.rechercheFocus = false;
			}
			return res;
		}

		/// L'ETAT d'une capture, pose par l'environnement (instrument de sonde ;
		/// sans les variables, rien ne change) : NK_LANCEUR_PAGE=<n> (la page de
		/// la colonne), NK_LANCEUR_VUE=liste|grille, NK_LANCEUR_FILTRE=<texte> (la
		/// recherche), NK_LANCEUR_TRI=nom|recents. Une seule lecture pour toutes
		/// les applications.
		inline void NkLanceurEtatDepuisEnv(NkProjectLauncherModel &m) {
			if (const char *v = std::getenv("NK_LANCEUR_PAGE"))
				if (*v)
					m.page = (int32)std::atoi(v);
			if (const char *v = std::getenv("NK_LANCEUR_VUE"))
				if (*v)
					m.vueListe = (v[0] == 'l' || v[0] == 'L');
			if (const char *v = std::getenv("NK_LANCEUR_TRI"))
				if (*v)
					m.tri = (uint8)((v[0] == 'n' || v[0] == 'N') ? 1u : 0u);
			if (const char *v = std::getenv("NK_LANCEUR_FILTRE")) {
				usize i = 0;
				for (; v[i] && i + 1u < sizeof(m.filtre); ++i)
					m.filtre[i] = v[i];
				m.filtre[i] = 0;
			}
		}

		// ── LA PHOTO SANS FENETRE ───────────────────────────────────────────────
		/// Une image que l'hote fournit a la capture (vignette, logo, bande) : les
		/// pixels RGBA8 et la poignee que le modele cite.
		struct NkLanceurImageCpu {
				uint32 id = 0u;
				const uint8 *px = nullptr;
				int32 w = 0, h = 0;
		};

		struct NkLanceurCaptureDesc {
				const char *chemin = nullptr;
				int32 largeur = 1600, hauteur = 900;
				float32 echelle = 1.15f; ///< la densite des applications (DPI 1 x 1,15)
				float32 sourisX = -100000.f, sourisY = -100000.f; ///< un survol se photographie aussi
				const NkLanceurImageCpu *images = nullptr;
				int32 nbImages = 0;
		};

		/// Peint le lanceur hors ecran et l'ecrit en PNG. Rend vrai si le fichier
		/// est ecrit ; `message` dit ce qui a ete fait (ou pourquoi pas).
		inline bool NkLanceurCapturer(const NkLanceurCaptureDesc &d, NkProjectLauncherModel &m, const NkTheme &theme,
									  const NkProjectLauncherHooks &h = NkProjectLauncherHooks(),
									  const NkProjectLauncherStyle &s = NkProjectLauncherStyle(),
									  char *message = nullptr, int32 cap = 0) {
			const int32 W = d.largeur, H = d.hauteur;
			static nkgui::NkGuiContext ctx;
			static nkgui::NkGuiFont normale;
			static NkLanceurPolices pol;
			static float32 echChargee = 0.f;
			NkEditorRendererMemoire mem((uint32)W, (uint32)H, theme.Get(NkRole::WindowBg));
			ctx.Init(W, H);
			if (echChargee != d.echelle) {
				echChargee = d.echelle;
				(void)normale.LoadEmbedded(NkEmbeddedFontId::Inter, (float32)(int32)(13.f * d.echelle + 0.5f));
				(void)pol.Charger(d.echelle, nullptr);
			}
			ctx.font = &normale;
			mem.UploadFontGray8(normale.TexId(), normale.pixels, normale.atlasW, normale.atlasH);
			mem.UploadFontGray8(pol.titre.TexId(), pol.titre.pixels, pol.titre.atlasW, pol.titre.atlasH);
			mem.UploadFontGray8(pol.intertitre.TexId(), pol.intertitre.pixels, pol.intertitre.atlasW,
								pol.intertitre.atlasH);
			mem.UploadFontGray8(pol.petite.TexId(), pol.petite.pixels, pol.petite.atlasW, pol.petite.atlasH);
			for (int32 i = 0; i < d.nbImages && d.images; ++i)
				mem.UploadImageRGBA(d.images[i].id, d.images[i].px, d.images[i].w, d.images[i].h);
			NkLanceurEtatDepuisEnv(m);
			// DEUX images : la premiere pose ce que le composant calcule a la volee,
			// la seconde est photographiee.
			for (int32 k = 0; k < 2; ++k) {
				ctx.input.mousePos = {d.sourisX, d.sourisY};
				ctx.BeginFrame(1.f / 60.f);
				(void)NkLanceurPeindre(ctx, theme, pol, NkPaintRect{0.f, 0.f, (float32)W, (float32)H}, m, s, h, d.echelle,
									   true);
				mem.BeginFrame();
				mem.SubmitDrawList(ctx.dl, (uint32)W, (uint32)H);
				mem.SubmitDrawList(ctx.dlOverlay, (uint32)W, (uint32)H);
				if (k == 1)
					(void)mem.CaptureNext(d.chemin);
				mem.EndFrame();
				ctx.EndFrame();
			}
			const bool ok = mem.DerniereEcritureOk();
			if (message && cap > 0)
				snprintf(message, (size_t)cap, "%s %s (%dx%d, %u commande(s) sans texture)", ok ? "ECRITE" : "ECHEC",
						 d.chemin ? d.chemin : "(aucun chemin)", (int)W, (int)H, (unsigned)mem.TexturesInconnues());
			return ok;
		}

		/// Lit `--capture-lanceur=FICHIER.png` (ou la variable NK_CAPTURE_LANCEUR) et
		/// `--theme-lanceur=clair|sombre` (ou NK_THEME_LANCEUR). Rend le chemin, vide
		/// si aucune capture n'est demandee. Une seule lecture pour toutes les
		/// applications : le meme mot fait la meme chose partout.
		inline NkString NkLanceurCaptureDemandee(int32 argc, const NkString *argv, bool *clair = nullptr) {
			NkString chemin;
			bool themeClair = false;
			if (const char *v = std::getenv("NK_CAPTURE_LANCEUR"))
				if (*v)
					chemin = NkString(v);
			if (const char *v = std::getenv("NK_THEME_LANCEUR"))
				themeClair = (v[0] == 'c' || v[0] == 'C' || v[0] == 'l' || v[0] == 'L');
			for (int32 i = 0; i < argc && argv; ++i) {
				const NkString &a = argv[i];
				if (a.StartsWith("--capture-lanceur="))
					chemin = NkString(a.CStr() + 18);
				else if (a.StartsWith("--theme-lanceur="))
					themeClair = (a.CStr()[16] == 'c' || a.CStr()[16] == 'C' || a.CStr()[16] == 'l');
			}
			if (clair)
				*clair = themeClair;
			return chemin;
		}

		// ── LES RECENTS SUR DISQUE (pour qui n'en avait pas) ────────────────────
		// Un fichier texte par application, dans le dossier de donnees de
		// l'utilisateur : « P|R chemin|nom|date » par ligne (P = epingle). Les
		// applications qui ont DEJA leur liste (NKCraft, NKCode, NKUIDesign) la
		// gardent : on ne double pas une liste qui existe.
		struct NkLanceurRecents {
				struct Entree {
						NkString chemin, nom, date;
						bool epingle = false;
				};
				NkVector<Entree> entrees;
				NkString fichier;

				/// `application` = nom du dossier (« NkAnimaEditor »).
				void Charger(const char *application) {
					entrees.Clear();
					NkString base = NkDirectory::GetAppDataDirectory().ToString();
					if (base.Empty())
						base = NkString(".");
					fichier = base + "/" + application + "/recents_lanceur.cfg";
					if (const char *v = std::getenv("NK_RECENTS_LANCEUR"))
						if (*v)
							fichier = NkString(v);
					const NkString txt = NkFile::ReadAllText(fichier.CStr());
					NkString ligne;
					auto flush = [&]() {
						if (ligne.Size() > 2u && (ligne[0] == 'P' || ligne[0] == 'R') && ligne[1] == ' ') {
							Entree e;
							e.epingle = ligne[0] == 'P';
							NkString parts[3];
							int32 k = 0;
							for (const char *c = ligne.CStr() + 2; *c; ++c) {
								if (*c == '|' && k < 2) {
									++k;
									continue;
								}
								parts[k] += *c;
							}
							e.chemin = parts[0];
							e.nom = parts[1];
							e.date = parts[2];
							if (!e.chemin.Empty())
								entrees.PushBack(e);
						}
						ligne.Clear();
					};
					for (const char *c = txt.CStr(); *c; ++c) {
						if (*c == '\n' || *c == '\r')
							flush();
						else
							ligne += *c;
					}
					flush();
				}
				void Enregistrer() const {
					if (fichier.Empty())
						return;
					const NkString dossier = NkPath(fichier.CStr()).GetParent().ToString();
					if (!dossier.Empty())
						(void)NkDirectory::CreateRecursive(dossier.CStr());
					NkString out;
					for (int32 passe = 0; passe < 2; ++passe)
						for (usize i = 0; i < entrees.Size(); ++i) {
							const Entree &e = entrees[i];
							if (e.epingle != (passe == 0))
								continue;
							out += e.epingle ? "P " : "R ";
							out += e.chemin;
							out += "|";
							out += e.nom;
							out += "|";
							out += e.date;
							out += "\n";
						}
					(void)NkFile::WriteAllText(fichier.CStr(), out.CStr());
				}
				/// En tete (apres les epingles), sans doublon, 24 au plus.
				void Toucher(const NkString &chemin, const NkString &nom, const NkString &date) {
					bool epingle = false;
					for (usize i = 0; i < entrees.Size(); ++i)
						if (entrees[i].chemin == chemin) {
							epingle = entrees[i].epingle;
							entrees.Erase(entrees.Begin() + i);
							break;
						}
					Entree e;
					e.chemin = chemin;
					e.nom = nom;
					e.date = date;
					e.epingle = epingle;
					entrees.Insert(entrees.Begin(), e);
					while (entrees.Size() > 24u)
						entrees.PopBack();
					Enregistrer();
				}
				void Retirer(usize i) {
					if (i < entrees.Size()) {
						entrees.Erase(entrees.Begin() + i);
						Enregistrer();
					}
				}
				void BasculerEpingle(usize i) {
					if (i < entrees.Size()) {
						entrees[i].epingle = !entrees[i].epingle;
						Enregistrer();
					}
				}
				/// Remplit les projets du modele (etat « introuvable » si le fichier
				/// a disparu ; `hote` = indice dans `entrees`).
				void Remplir(NkProjectLauncherModel &m) const {
					m.projets.Clear();
					for (int32 passe = 0; passe < 2; ++passe)
						for (usize i = 0; i < entrees.Size(); ++i) {
							const Entree &e = entrees[i];
							if (e.epingle != (passe == 0))
								continue;
							NkLanceurProjet p;
							p.nom = e.nom.Empty() ? NkPath(e.chemin.CStr()).GetFileNameWithoutExtension()
												  : e.nom;
							p.chemin = e.chemin;
							p.date = e.date;
							p.epingle = e.epingle;
							p.etat = (NkFile::Exists(e.chemin.CStr()) || NkDirectory::Exists(e.chemin.CStr())) ? 0u : 1u;
							p.hote = (uint32)i;
							m.projets.PushBack(p);
						}
				}
		};

	} // namespace editorkit
} // namespace nkentseu
