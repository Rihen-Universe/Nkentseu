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
			// ⚠️ LE BLEU EST LE PRIMAIRE DE NKCODE (selection, bouton principal, cadre
			//    de « Nouveau Workspace ») ; l'ORANGE est son accent secondaire (les
			//    exemples, l'etoile). Mesure sur la capture « avant » du 01/10.
			t.Set(NkRole::AccentUi, NkCol::primary.ToUint32A());
			t.Set(NkRole::AccentSel, NkCol::accent.ToUint32A());
			t.Set(NkRole::TextOnAccent, NkCol::primaryFg.ToUint32A());
			t.Set(NkRole::StatusErr, NkCol::danger.ToUint32A());
			return t;
		}

		/// Le style : l'ETOILE de NKCode pour epingler.
		inline editorkit::NkProjectLauncherStyle NkCodeStyleLanceur() {
			editorkit::NkProjectLauncherStyle s;
			s.glypheEpingle = editorkit::NkLanceurGlyphe::Etoile;
			// NKCode ecrit TOUT dans sa police d'interface (les sous-titres des
			// actions, les en-tetes de groupe) : pas de « petit texte » a part.
			s.policePetite = 0u;
			s.rayonChamp = 6.f;
			return s;
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

		/// Le premier modele qui est un EXEMPLE Jenga (avant : les quatre actions
		/// rapides de la page classique, dans le meme ordre).
		static const usize kNkCodePremierExemple = 4u;

		/// Les DONNEES du lanceur depuis l'etat vivant de NKCode (chaque image).
		/// ⚠️ LA DISPOSITION EST CELLE DE NKCODE, pas celle des autres
		///    applications : une recherche en tete (pas de grand titre), la liste
		///    detaillee des workspaces groupee par date (Langages, Configs,
		///    Plateformes, Projets, Modifie -- les champs de NkWorkspaceCard), et a
		///    droite les ACTIONS RAPIDES puis les EXEMPLES JENGA avec leur
		///    recherche. Les libelles viennent de NkT (les huit langues de NKCode).
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
				m.enTete = false;  // NKCode ouvre sur sa recherche, pas sur un titre
				m.disposition = NkLanceurDisposition::ColonneDroite;
				m.vueListe = true; // ses cartes de workspace sont des LIGNES detaillees
				m.actionHote = NkString("Accueil classique");
				m.glypheActionHote = G::Liste;
				NkLanceurPage p;
				p.libelle = NkString("Accueil");
				p.glyphe = G::Projets;
				m.pages.PushBack(p);
				m.piedDePage = NkString("Ctrl+N : nouveau workspace · Ctrl+O : ouvrir · Ctrl+G : cloner un depot.");
			}

			// ── La colonne de droite : les ACTIONS RAPIDES de NKCode (memes libelles,
			//    meme ordre, la premiere mise en avant), puis les EXEMPLES JENGA. ──
			m.modeles.Clear();
			const NkString gActions(NkT("home.actions"));
			auto action = [&](const char *nom, const char *desc, G g, uint32 teinte, bool accentue) {
				NkLanceurModele md;
				md.nom = NkString(nom);
				md.description = NkString(desc);
				md.glyphe = g;
				md.couleur = teinte;
				md.accentue = accentue;
				md.groupe = gActions;
				m.modeles.PushBack(md);
			};
			action(NkT("nav.newws"), NkT("qa.newws.sub"), G::Nouveau, NkCol::primary.ToUint32A(), true);
			action(NkT("qa.openws"), NkT("qa.openws.sub"), G::Ouvrir, NkCol::mutedFg.ToUint32A(), false);
			action(NkT("qa.openfolder"), NkT("qa.openfolder.sub"), G::Dossier, NkCol::mutedFg.ToUint32A(), false);
			action(NkT("qa.clone"), NkT("qa.clone.sub"), G::Lien, NkCol::secondaryFg.ToUint32A(), false);
			const NkString gExemples =
				NkPrintf("%s (%d)", NkT("home.examples"), st ? (int)st->examples.Size() : 0);
			m.groupeAvecRecherche = gExemples;
			if (st)
				for (usize i = 0; i < st->examples.Size(); ++i) {
					const NkCodeState::Example &e = st->examples[i];
					NkLanceurModele md;
					md.nom = e.id;
					md.description = e.platforms.Empty() ? e.desc : e.platforms;
					md.categorie = e.desc;
					md.glyphe = G::Apprendre;
					md.couleur = NkCol::accent.ToUint32A();
					md.ligne = true;
					md.groupe = gExemples;
					md.disponible = !(H && H->dlg && H->dlg->exCopyBusy);
					m.modeles.PushBack(md);
				}
			if (!st || st->examples.Empty()) {
				// La liste vient de `jenga examples list`, en tache de fond : en
				// attendant, l'en-tete existe et le dit, pas de colonne muette.
				NkLanceurModele md;
				md.nom = NkString(NkT("home.examples"));
				md.description = NkString("...");
				md.glyphe = G::Apprendre;
				md.couleur = NkCol::accent.ToUint32A();
				md.ligne = true;
				md.disponible = false;
				md.groupe = gExemples;
				m.modeles.PushBack(md);
			}

			// ── La liste : le courant, les epingles, les recents -- groupes comme
			//    la page classique (EPINGLES, puis AUJOURD'HUI, CETTE SEMAINE...). ──
			m.projets.Clear();
			L.chemins.Clear();
			NkString courant;
			if (st && st->HasWorkspace() && st->wsIdx >= 0 && st->wsIdx < (int32)st->wsPaths.Size())
				courant = st->wsPaths[st->wsIdx];
			const int64 now = NkCodeState::NowEpoch();
			const uint32 tuiles[4] = {NkCol::primary.ToUint32A(), NkCol::accent.ToUint32A(), NkCol::secondary.ToUint32A(),
									  math::NkColor(51, 177, 160).ToUint32A()};
			auto ajoute = [&](const NkString &chemin, const NkString &nom, bool epingle, bool estCourant) {
				for (usize k = 0; k < L.chemins.Size(); ++k)
					if (L.chemins[k] == chemin)
						return;
				const NkCodeState::WsMeta meta = st->WorkspaceMeta(chemin.CStr());
				NkLanceurProjet p;
				p.nom = estCourant ? st->root.GetFileName() : (nom.Empty() ? chemin : nom);
				p.chemin = chemin;
				p.epingle = epingle;
				p.glyphe = G::Pile;
				p.couleur = estCourant ? NkCol::primary.ToUint32A() : tuiles[L.chemins.Size() % 4u];
				const NkString age = NkCodeState::HumanAge(meta.activity, now);
				p.date = age;
				p.groupe = epingle ? NkString(NkT("home.pinned"))
								   : NkString(NkHomeBucketLabel(NkCodeState::AgeBucket(meta.activity, now)));
				auto detail = [&](const char *lib, const NkString &val, bool ligne) {
					if (val.Empty())
						return;
					NkLanceurDetail d;
					d.libelle = NkString(lib);
					d.valeur = val;
					d.nouvelleLigne = ligne;
					p.details.PushBack(d);
				};
				detail("Langages: ", estCourant ? NkString("C++20") : (meta.langVer.Empty() ? NkString("C++") : meta.langVer),
					   false);
				detail("Configs: ", estCourant ? st->infoConfigs : meta.configs, false);
				detail("Plateformes: ", estCourant ? st->infoOSes : meta.platforms, true);
				if (meta.projCount > 0 || !meta.projects.Empty()) {
					NkString pr = meta.projects;
					if (meta.projCount > 0)
						pr += meta.projCountExact ? NkPrintf("  (%d)", meta.projCount) : NkPrintf("  (~%d)", meta.projCount);
					detail(NkT("card.projects"), pr, true);
				}
				if (estCourant)
					detail(NkT("card.lastbuild"), st->ConfigName(), true);
				detail("Modifie: ", age, !estCourant);
				const NkString dossier = NkCodeState::RecentFolder(chemin.CStr()).ToString();
				p.etat = (NkDirectory::Exists(dossier.CStr()) || NkFile::Exists(chemin.CStr())) ? 0u : 1u;
				p.hote = (uint32)L.chemins.Size();
				L.chemins.PushBack(chemin);
				m.projets.PushBack(p);
			};
			if (st) {
				for (usize i = 0; i < st->pinned.Size(); ++i)
					ajoute(st->pinned[i], i < st->pinnedNames.Size() ? st->pinnedNames[i] : NkString(), true,
						   st->pinned[i] == courant);
				if (!courant.Empty())
					ajoute(courant, NkString(), st->IsPinned(courant.CStr()), true);
				for (usize i = 0; i < st->recents.Size(); ++i)
					ajoute(st->recents[i], i < st->recentNames.Size() ? st->recentNames[i] : NkString(), false, false);
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
								 NkCodeStyleLanceur(), NkProjectLauncherHooks(), ech, entree);
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
						H->nav = 2; // Nouveau workspace : l'assistant
					else if (i == 1 || i == 2)
						H->nav = 1; // Ouvrir un workspace / un dossier
					else if (i == 3)
						H->nav = 3; // Cloner depuis Git
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
			const bool ok = NkLanceurCapturer(d, m, NkCodeThemeLanceur(), NkProjectLauncherHooks(), NkCodeStyleLanceur(),
											  msg, (int32)sizeof(msg));
			std::printf("[capture-lanceur] NKCode : %s\n", msg);
			std::fflush(stdout);
			return ok ? 0 : 1;
		}

	} // namespace nkcode
} // namespace nkentseu
