#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkHomeLanceur.h — l'ACCUEIL de NKCode par le lanceur de projets PARTAGE
// (NKEditorKit, Components/NkProjectLauncherModel.h), le meme que NKCraft,
// NkAnimaEditor, NKUIDesign et PV3DE (2026-10-01).
//
// « COMBLER SANS DETRUIRE ». NKCode avait DEJA son lanceur (design Banani) :
// sa colonne (marque, navigation, versions), son assistant de workspace, le
// clonage, les toolchains, les plateformes, les parametres. Tout cela RESTE.
// Seule la page « Accueil » (nav == 0) est peinte par le composant partage,
// INCRUSTE (`colonne = false`) dans la colonne de NKCode, et peint aux couleurs
// de NKCode (le theme du kit est rempli depuis NkCol, la palette vivante).
// L'ancienne page reste a un clic : « Accueil classique » (le bouton propre a
// l'application), et revient par « Accueil Nkentseu » dans ses actions rapides.
// Le choix est garde dans les parametres (`accueil`).
//
// CE QU'IL OUVRE PASSE PAR LES PORTES DE NKCODE :
//   recent / epingle / courant -> dlg->DoLoad(RecentFolder) ;
//   epingler / retirer         -> PinRecent / UnpinRecent / RemoveRecent ;
//   Nouveau workspace          -> l'assistant (nav 2) ; Ouvrir -> nav 1 ;
//   Cloner                     -> nav 3 ;
//   un exemple Jenga           -> exCopyId + le selecteur PK_ExampleCopy, comme
//                                 la liste d'exemples de la page classique.
//
// ⚠️ CET EN-TETE EST INCLUS A LA FIN DE NkHome.h : il a besoin de NkHomeState
//    complet, et NkHome.h l'appelle (DrawHome) par une declaration anticipee.
// =============================================================================

#include "NKEditorKit/NkProjectLauncherHost.h"
#include "NKMath/NkColor.h"

namespace nkentseu {
	namespace nkcode {

		/// Le theme du KIT aux couleurs VIVANTES de NKCode (NkCol, posees par
		/// NkApplyTheme) : Dark Pro, Dark, Midnight, Light et l'accent choisi.
		inline editorkit::NkTheme NkCodeThemeLanceur() {
			using editorkit::NkRole;
			const auto &bg = NkCol::background;
			const bool sombre = ((int32)bg.r + (int32)bg.g + (int32)bg.b) < 3 * 128;
			editorkit::NkTheme t = sombre ? editorkit::NkTheme::Dark() : editorkit::NkTheme::Light();
			t.Set(NkRole::WindowBg, NkCol::background.ToUint32A());
			t.Set(NkRole::PanelBg, NkCol::surface.ToUint32A());
			t.Set(NkRole::PanelHeader, NkCol::hover.ToUint32A());
			t.Set(NkRole::InputBg, NkCol::input.ToUint32A());
			t.Set(NkRole::Border, NkCol::border.ToUint32A());
			t.Set(NkRole::Text, NkCol::foreground.ToUint32A());
			t.Set(NkRole::TextMuted, NkCol::mutedFg.ToUint32A());
			t.Set(NkRole::AccentUi, NkCol::accent.ToUint32A());
			t.Set(NkRole::AccentSel, NkCol::accent.ToUint32A());
			t.Set(NkRole::StatusErr, NkCol::danger.ToUint32A());
			return t;
		}

		struct NkCodeLanceurEtat {
				editorkit::NkProjectLauncherModel m;
				editorkit::NkLanceurPolices polices;
				NkVector<NkString> chemins; ///< `hote` des cartes -> chemin .jenga
				bool pret = false;
		};
		inline NkCodeLanceurEtat &NkCodeLanceur() {
			static NkCodeLanceurEtat e;
			return e;
		}

		/// Le premier modele qui est un EXEMPLE Jenga (avant : les trois portes).
		static const usize kNkCodePremierExemple = 3u;

		/// Les DONNEES du lanceur depuis l'etat vivant de NKCode (chaque image).
		inline void NkHomeLanceurRemplir(NkHomeState *H) {
			using namespace editorkit;
			using G = NkLanceurGlyphe;
			NkCodeLanceurEtat &L = NkCodeLanceur();
			NkProjectLauncherModel &m = L.m;
			NkCodeState *st = H ? H->st : nullptr;
			if (!L.pret) {
				L.pret = true;
				m.identite.nom = NkString("NKCode");
				m.identite.prefixe = NkString("NK");
				m.identite.motProjet = NkString("workspace");
				m.identite.motProjets = NkString("workspaces");
				m.identite.extensions = NkString(".jenga");
				m.identite.glyphe = G::Code;
				m.colonne = false; // la colonne de NKCode reste la sienne
				m.actionHote = NkString("Accueil classique");
				m.glypheActionHote = G::Liste;
				NkLanceurPage p;
				p.libelle = NkString("Accueil");
				p.titre = NkString("Accueil");
				p.glyphe = G::Projets;
				m.pages.PushBack(p);
				m.piedDePage = NkString("Ctrl+N : nouveau workspace · Ctrl+O : ouvrir · Ctrl+G : cloner un depot.");
			}

			// ── Les modeles : les trois portes de NKCode, puis les exemples Jenga
			//    (charges en tache de fond : la liste se complete toute seule). ──
			m.modeles.Clear();
			auto mod = [&](const char *nom, const char *cat, const char *desc, G g, uint32 teinte, bool dispo) {
				NkLanceurModele md;
				md.nom = NkString(nom);
				md.categorie = NkString(cat);
				md.description = NkString(desc);
				md.glyphe = g;
				md.couleur = teinte;
				md.disponible = dispo;
				m.modeles.PushBack(md);
			};
			mod("Nouveau workspace", "ASSISTANT", "Langage, plateformes, toolchains : l'assistant pas a pas.", G::Nouveau,
				0u, true);
			mod("Ouvrir un dossier", "WORKSPACE", "Un dossier qui porte un .jenga, ou un dossier de sources.", G::Ouvrir,
				math::NkColor(0x2E, 0xA0, 0x6B).ToUint32A(), true);
			mod("Cloner un depot Git", "GIT", "GitHub, GitLab, Bitbucket... cloner puis ouvrir.", G::Lien,
				math::NkColor(0x8E, 0x5B, 0xE8).ToUint32A(), true);
			if (st)
				for (usize i = 0; i < st->examples.Size() && i < 9u; ++i) {
					const NkCodeState::Example &e = st->examples[i];
					const NkString cat = e.difficulty.Empty() ? NkString("EXEMPLE") : e.difficulty;
					mod(e.id.CStr(), cat.CStr(), e.desc.CStr(), G::Code, math::NkColor(0x2E, 0x8E, 0xD8).ToUint32A(),
						!(H && H->dlg && H->dlg->exCopyBusy));
				}

			// ── Les workspaces : le courant, les epingles, les recents. ──
			m.projets.Clear();
			L.chemins.Clear();
			NkString courant;
			if (st && st->HasWorkspace() && st->wsIdx >= 0 && st->wsIdx < (int32)st->wsPaths.Size())
				courant = st->wsPaths[st->wsIdx];
			const int64 now = NkCodeState::NowEpoch();
			auto ajoute = [&](const NkString &chemin, const NkString &nom, bool epingle, const char *dateForcee) {
				for (usize k = 0; k < L.chemins.Size(); ++k)
					if (L.chemins[k] == chemin)
						return;
				NkLanceurProjet p;
				p.nom = nom.Empty() ? chemin : nom;
				p.chemin = chemin;
				p.epingle = epingle;
				p.date = dateForcee ? NkString(dateForcee)
									: NkCodeState::HumanAge(st->WorkspaceMeta(chemin.CStr()).activity, now);
				const NkString dossier = NkCodeState::RecentFolder(chemin.CStr()).ToString();
				p.etat = (NkDirectory::Exists(dossier.CStr()) || NkFile::Exists(chemin.CStr())) ? 0u : 1u;
				p.hote = (uint32)L.chemins.Size();
				L.chemins.PushBack(chemin);
				m.projets.PushBack(p);
			};
			if (st) {
				if (!courant.Empty())
					ajoute(courant, st->root.GetFileName(), st->IsPinned(courant.CStr()), "ouvert maintenant");
				for (usize i = 0; i < st->pinned.Size(); ++i)
					ajoute(st->pinned[i], i < st->pinnedNames.Size() ? st->pinnedNames[i] : NkString(), true, nullptr);
				for (usize i = 0; i < st->recents.Size(); ++i)
					ajoute(st->recents[i], i < st->recentNames.Size() ? st->recentNames[i] : NkString(), false, nullptr);
			}

		}

		inline void NkHomeLanceurPanel(NkEditorFrameContext &ec, const NkRect &panel, NkHomeState *H) {
			using namespace editorkit;
			if (!H)
				return;
			NkCodeLanceurEtat &L = NkCodeLanceur();
			NkProjectLauncherModel &m = L.m;
			NkCodeState *st = H->st;
			nkgui::NkGuiContext &ctx = ec.Ui();
			const float32 ech = ctx.S(1.f) > 0.f ? ctx.S(1.f) : 1.f;
			if (L.polices.echelle != ech)
				(void)L.polices.Charger(ech, (H->dlg && H->dlg->shell) ? H->dlg->shell->Renderer() : nullptr);
			NkHomeLanceurRemplir(H);

			const bool entree = !(H->dlg && H->dlg->pickerOpen);
			const NkProjectLauncherResult res =
				NkLanceurPeindre(ctx, NkCodeThemeLanceur(), L.polices, NkPaintRect{panel.x, panel.y, panel.w, panel.h}, m,
								 NkProjectLauncherStyle(), NkProjectLauncherHooks(), ech, entree);
			if (!st)
				return;
			const int32 i = res.index;
			const bool projetValide = i >= 0 && (usize)i < m.projets.Size();
			const NkString chemin = projetValide ? L.chemins[(usize)m.projets[(usize)i].hote] : NkString();
			switch (res.action) {
				case NkLanceurAction::NouveauProjet:
					H->nav = 2;
					break;
				case NkLanceurAction::Ouvrir:
					H->nav = 1;
					break;
				case NkLanceurAction::NouveauDepuisModele:
					if (i == 0)
						H->nav = 2;
					else if (i == 1)
						H->nav = 1;
					else if (i == 2)
						H->nav = 3;
					else if (i >= (int32)kNkCodePremierExemple && H->dlg && !H->dlg->exCopyBusy && !H->dlg->pickerOpen) {
						const usize k = (usize)i - kNkCodePremierExemple;
						if (k < st->examples.Size()) {
							// LA porte de la liste d'exemples : le selecteur demande
							// l'emplacement, `jenga examples copy` clone, le clone s'ouvre.
							H->dlg->exCopyId = st->examples[k].id;
							const char *homeDir = env::GetEnvVar("USERPROFILE");
							if (!homeDir || !*homeDir)
								homeDir = env::GetEnvVar("HOME");
							H->dlg->OpenPicker(NkCodeDialogs::PK_ExampleCopy, (homeDir && *homeDir) ? homeDir : ".");
						}
					}
					break;
				case NkLanceurAction::OuvrirRecent:
					if (projetValide && H->dlg && m.projets[(usize)i].etat == 0u)
						H->dlg->DoLoad(NkCodeState::RecentFolder(chemin.CStr()));
					break;
				case NkLanceurAction::Epingler:
					if (projetValide) {
						if (st->IsPinned(chemin.CStr()))
							st->UnpinRecent(chemin);
						else
							st->PinRecent(chemin);
					}
					break;
				case NkLanceurAction::Retirer:
					if (projetValide)
						st->RemoveRecent(chemin);
					break;
				case NkLanceurAction::Purger:
					for (usize k = 0; k < m.projets.Size(); ++k)
						if (m.projets[k].etat != 0u)
							st->RemoveRecent(L.chemins[(usize)m.projets[k].hote]);
					break;
				case NkLanceurAction::ActionHote:
					H->settings.accueil = 1; // l'accueil classique, garde dans les parametres
					H->settings.Save();
					break;
				default:
					break;
			}
		}

		/// `--capture-lanceur=FICHIER.png [--theme-lanceur=clair]` : la photo sans
		/// fenetre ni GPU, par le MEME composant, aux couleurs de NKCode (ses
		/// parametres, son accent) et avec SES recents. ⚠️ Dans l'application, la
		/// colonne Banani de NKCode tient la place de la colonne du composant ;
		/// la photo, elle, montre la colonne du composant a la touche de NKCode.
		inline int NkHomeLanceurCapturer(const NkString &chemin, bool clair) {
			using namespace editorkit;
			static NkCodeState st;
			st.LoadRecents();
			static NkHomeState H;
			H.st = &st;
			if (!H.settings.loaded)
				H.settings.Load();
			NkApplyTheme(clair ? 3 : H.settings.theme, H.settings.accent);
			NkHomeLanceurRemplir(&H);
			NkProjectLauncherModel m = NkCodeLanceur().m;
			m.colonne = true;
			m.identite.sousTitre = NkString("IDE Jenga — Nkentseu");
			m.themeSombre = !clair;
			NkLanceurCaptureDesc d;
			d.chemin = chemin.CStr();
			char msg[512];
			const bool ok = NkLanceurCapturer(d, m, NkCodeThemeLanceur(), NkProjectLauncherHooks(),
											  NkProjectLauncherStyle(), msg, (int32)sizeof(msg));
			std::printf("[capture-lanceur] NKCode : %s\n", msg);
			std::fflush(stdout);
			return ok ? 0 : 1;
		}

	} // namespace nkcode
} // namespace nkentseu
