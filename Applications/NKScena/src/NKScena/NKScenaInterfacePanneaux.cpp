// -----------------------------------------------------------------------------
// @File    NKScenaInterfacePanneaux.cpp
// @Brief   Les PANNEAUX de NKScena, peints par les pieces partagees de la famille
//          (NKEditorKit/Famille) : l'Outliner (LA SCENE et LES PISTES), les
//          Details en CARTES (entite, piste, cle, plan, sequence), le tiroir du
//          bas -- la FRISE partagee au premier onglet, puis Contenu, Journal,
//          Terminal.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NKScena/NKScenaInterface.h"
#include "NKScena/NKScenaLogo.h"
#include "NKScena/NKScenaPont.h"

#include "NKEditorKit/Components/NkGuiComponentPaint.h"
#include "Noge/ECS/Components/Core/NkTag.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Rendering/NkRenderComponents.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace nkscena {

		using namespace editorkit;
		using namespace ecs;
		using math::NkVec3f;
		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {
			constexpr nk_uint64 kNoeudScene = 2;
			constexpr nk_uint64 kNoeudPistes = 3;
			constexpr nk_uint64 kBitPiste = 0x4000000000000000ull;

			NkScenaInterface &Ui(void *u) {
				return *static_cast<NkScenaInterface *>(u);
			}

			const NkScenaInterface::NkLigne *Ligne(NkScenaInterface &ui, int32 index) {
				return index >= 0 && index < static_cast<int32>(ui.mLignes.Size()) ? &ui.mLignes[static_cast<uint32>(index)] : nullptr;
			}

			void SurActiver(void *u, int32 index) {
				NkScenaInterface &ui = Ui(u);
				const NkScenaInterface::NkLigne *l = Ligne(ui, index);
				if (l != nullptr && l->entite.IsValid()) {
					ui.M().scene.Choisir(l->entite);
					ui.M().scene.Cadrer(l->entite);
				} else if (l != nullptr && l->piste != 0) {
					// Le double-clic d'une piste : le curseur va a sa premiere cle.
					if (const NkTimelineTrack *t = ui.M().frise.Track(l->piste)) {
						if (!t->keys.Empty()) {
							ui.M().frise.SetCursor(t->keys[0].time);
						} else if (!t->clips.Empty()) {
							ui.M().frise.SetCursor(t->clips[0].start);
						}
					}
				}
			}

			void SurDrapeau(void *u, int32 index, bool oeil) {
				NkScenaInterface &ui = Ui(u);
				const NkScenaInterface::NkLigne *l = Ligne(ui, index);
				if (l == nullptr) {
					return;
				}
				if (l->entite.IsValid()) {
					if (oeil) {
						ui.M().scene.BasculerCache(l->entite);
					} else {
						ui.M().scene.BasculerVerrou(l->entite);
					}
					return;
				}
				if (l->piste == 0) {
					return;
				}
				NkTimelineModel &f = ui.M().frise;
				// L'oeil d'une piste la rend MUETTE (elle ne joue plus) ; le cadenas la
				// verrouille (ses cles ne bougent plus) -- pour tout l'objet a la fois.
				if (!l->objet.Empty()) {
					f.SetObjectFlag(l->objet, oeil ? 0 : 2, !f.ObjectFlag(l->objet, oeil ? 0 : 2));
				} else if (oeil) {
					f.ToggleMute(l->piste);
				} else {
					f.ToggleLock(l->piste);
				}
			}

			void SurRenommer(void *u, int32, const char *) {
				Ui(u).M().Annoncer("NKScena ne renomme pas la scène : ses pistes la nomment (renommer dans Nogee)", 2);
			}

			void SurMenu(void *u, int32 index, float32 x, float32 y) {
				NkScenaInterface &ui = Ui(u);
				const NkScenaInterface::NkLigne *l = Ligne(ui, index);
				if (l != nullptr && l->entite.IsValid()) {
					ui.M().scene.Choisir(l->entite);
					ui.menus.Ouvrir(SCENA_MENU_CTX_ENTITE, NkRect{x, y, 0.f, 0.f});
				} else {
					ui.mTempsMenuPlan = ui.M().frise.cursor;
					ui.menus.Ouvrir(SCENA_MENU_PISTE_PLUS, NkRect{x, y, 0.f, 0.f});
				}
			}

			void SurNouvelle(void *u) {
				NkScenaInterface &ui = Ui(u);
				const nkgui::NkVec2 p = ui.Gui().input.mousePos;
				ui.menus.Ouvrir(SCENA_MENU_PISTE_PLUS, NkRect{p.x, p.y, 0.f, 0.f});
			}

			NkFamilleNature NatureFamille(nogee::NogeeNature n) {
				switch (n) {
					case nogee::NogeeNature::Maillage:
						return NkFamilleNature::Maillage;
					case nogee::NogeeNature::Rigide:
						return NkFamilleNature::Rigide;
					case nogee::NogeeNature::Decor:
						return NkFamilleNature::Decor;
					case nogee::NogeeNature::Lumiere:
						return NkFamilleNature::Lumiere;
					case nogee::NogeeNature::Camera:
						return NkFamilleNature::Camera;
					default:
						return NkFamilleNature::Entite;
				}
			}

			const NkFamilleNatureFichier kNatures[] = {
				{".nkseq", "Séquence", NkRole::StatusErr, static_cast<uint8>(NkAssetIcone::Texte)},
				{".nkscene3d", "Scène 3D", NkRole::AccentUi, static_cast<uint8>(NkAssetIcone::Texte)},
				{".nkscene", "Scène (ancienne)", NkRole::AccentUi, static_cast<uint8>(NkAssetIcone::Texte)},
				{".png", "Image", NkRole::TypeTex, static_cast<uint8>(NkAssetIcone::Image)},
				{".jpg", "Image", NkRole::TypeTex, static_cast<uint8>(NkAssetIcone::Image)},
				{".mp4", "Vidéo", NkRole::TypeAnim, static_cast<uint8>(NkAssetIcone::Inconnu)},
				{".wav", "Son", NkRole::StatusWarn, static_cast<uint8>(NkAssetIcone::Inconnu)},
			};

			bool FinitPar(const NkString &s, const char *fin) {
				const usize n = std::strlen(fin);
				if (s.Length() < n) {
					return false;
				}
				const char *p = s.CStr() + s.Length() - n;
				for (usize i = 0; i < n; ++i) {
					char a = p[i];
					a = (a >= 'A' && a <= 'Z') ? static_cast<char>(a - 'A' + 'a') : a;
					if (a != fin[i]) {
						return false;
					}
				}
				return true;
			}

			const char *Fichier(const NkString &chemin) {
				const char *s = chemin.CStr();
				const char *r = s;
				for (const char *p = s; *p != '\0'; ++p) {
					if (*p == '/' || *p == '\\') {
						r = p + 1;
					}
				}
				return r;
			}

			// ── Les greffes de la frise ─────────────────────────────────────────
			bool EvaluerPiste(void *u, const NkTimelineTrack &t, float32 temps, float32 out[4]) {
				return NkScenaEvaluerPiste(Ui(u).M().frise, t, temps, out);
			}

			bool LireVivant(void *u, const NkTimelineTrack &t, float32 out[4]) {
				return Ui(u).M().LireVivant(t, out);
			}

			NkTimelineStyle StyleFrise() {
				// Les memes roles que NkAnimaEditor (Frise/NkAnimaFrise.cpp) : deux
				// editeurs de la famille, une seule frise, un seul aspect.
				NkTimelineStyle s;
				s.panelBg = (uint16)NkRole::PanelBg;
				s.headerBg = (uint16)NkRole::PanelHeader;
				s.rowAltBg = (uint16)NkRole::InputBg;
				s.border = (uint16)NkRole::Border;
				s.grid = (uint16)NkRole::GridLine;
				s.text = (uint16)NkRole::Text;
				s.textMuted = (uint16)NkRole::TextMuted;
				s.accent = (uint16)NkRole::AccentUi;
				s.textOnAccent = (uint16)NkRole::TextOnAccent;
				s.key = (uint16)NkRole::Text;
				s.keySelected = (uint16)NkRole::AccentSel;
				s.playhead = (uint16)NkRole::StatusErr;
				s.channel[0] = (uint16)NkRole::AxisX;
				s.channel[1] = (uint16)NkRole::AxisY;
				s.channel[2] = (uint16)NkRole::AxisZ;
				s.channel[3] = (uint16)NkRole::TextMuted;
				s.buttonBg = (uint16)NkRole::ButtonBg;
				s.inputBg = (uint16)NkRole::InputBg;
				s.rangeIn = (uint16)NkRole::StatusOk;
				s.rangeOut = (uint16)NkRole::StatusErr;
				s.marker = (uint16)NkRole::AccentSel;
				s.clip = (uint16)NkRole::TypeAnim;
				s.clipAlt = (uint16)NkRole::NodeActionHeader;
				s.tangent = (uint16)NkRole::AccentSel;
				return s;
			}
		} // namespace

		// =====================================================================
		// L'OUTLINER : la scene, puis les pistes
		// =====================================================================
		void NkScenaInterface::Outliner(NkFamilleCtx &c) {
			NkScenaModele &m = M();
			nogee::NogeeModele &sc = m.scene;
			if (!plan.voirOutliner) {
				return;
			}
			NkTreeViewModel &a = mOutliner.arbre;
			a.nodes.Clear();
			mLignes.Clear();
			auto Ajouter = [&](const NkTreeNode &n, const NkLigne &l) -> int32 {
				const int32 i = static_cast<int32>(a.nodes.Size());
				a.nodes.PushBack(n);
				mLignes.PushBack(l);
				return i;
			};
			NkTreeNode racine;
			racine.id = kNkFamilleRacine;
			racine.parent = -1;
			racine.label = m.nom;
			racine.kindLabel = "Séquence";
			racine.kindRole = static_cast<uint16>(NkRole::TextMuted);
			(void)Ajouter(racine, NkLigne{});

			// ── LA SCENE : ses entites, en arbre (le patron de Nogee) ────────────
			NkTreeNode tete;
			tete.id = kNoeudScene;
			tete.parent = 0;
			tete.label = NkString(Fichier(sc.chemin));
			tete.kindLabel = "Scène";
			tete.kindRole = static_cast<uint16>(NkRole::AccentUi);
			tete.icon = NkFamilleIconeNoeud(NkFamilleNature::Decor, false, false, false, false);
			const int32 iScene = Ajouter(tete, NkLigne{});
			NkVector<NkEntityId> ids;
			sc.Entites(ids);
			auto Ranger = [&](auto &&soi, NkEntityId e, int32 parent, uint32 profondeur) -> void {
				NkTreeNode n;
				n.id = e.Pack() + 16u;
				n.parent = parent;
				n.label = NkString(sc.Nom(e));
				n.kindLabel = m.APisteTransform(sc.Nom(e)) ? "animée" : sc.Type(e);
				const nogee::NogeeNature nat = sc.Nature(e);
				n.kindRole = static_cast<uint16>(m.APisteTransform(sc.Nom(e)) ? NkRole::StatusErr
												 : (nat == nogee::NogeeNature::Lumiere || nat == nogee::NogeeNature::Camera)
													 ? NkRole::AccentSel
													 : NkRole::TextMuted);
				n.icon = NkFamilleIconeNoeud(NatureFamille(nat), sc.EstCache(e), sc.EstVerrouille(e), false, false);
				n.hidden = sc.EstCache(e);
				NkLigne l;
				l.entite = e;
				const int32 moi = Ajouter(n, l);
				if (profondeur + 3u >= static_cast<uint32>(NkTreeViewModel::kMaxDepth)) {
					return;
				}
				for (uint32 k = 0; k < ids.Size(); ++k) {
					if (sc.Parent(ids[k]) == e) {
						soi(soi, ids[k], moi, profondeur + 1u);
					}
				}
			};
			for (uint32 k = 0; k < ids.Size(); ++k) {
				const NkEntityId p = sc.Parent(ids[k]);
				bool connu = false;
				for (uint32 j = 0; j < ids.Size() && p.IsValid(); ++j) {
					connu = connu || ids[j] == p;
				}
				if (!connu) {
					Ranger(Ranger, ids[k], iScene, 1u);
				}
			}

			// ── LES PISTES : une ligne par objet de la frise ─────────────────────
			NkTreeNode pistes;
			pistes.id = kNoeudPistes;
			pistes.parent = 0;
			pistes.label = "Pistes";
			pistes.kindLabel = "Frise";
			pistes.kindRole = static_cast<uint16>(NkRole::AccentUi);
			pistes.icon = NkFamilleIconeNoeud(NkFamilleNature::Entite, false, false, false, false);
			const int32 iPistes = Ajouter(pistes, NkLigne{});
			NkVector<NkString> vus;
			for (uint32 i = 0; i < m.frise.tracks.Size(); ++i) {
				const NkTimelineTrack &t = m.frise.tracks[i];
				const bool plans = t.kind == NkTimelineValueKind::Clips;
				bool deja = false;
				for (uint32 j = 0; j < vus.Size() && !plans; ++j) {
					deja = deja || vus[j] == t.object;
				}
				if (deja) {
					continue;
				}
				NkTreeNode n;
				n.id = kBitPiste | t.id;
				n.parent = iPistes;
				NkLigne l;
				l.piste = t.id;
				if (plans) {
					n.label = t.label.Empty() ? t.property : t.label;
					n.kindLabel = "plans";
					n.kindRole = static_cast<uint16>(NkRole::StatusErr);
					n.icon = NkFamilleIconeNoeud(NkFamilleNature::Camera, t.muted, t.locked, false, false);
					n.hidden = t.muted;
				} else {
					vus.PushBack(t.object);
					l.objet = t.object;
					n.label = t.object;
					n.kindLabel = "transformation";
					n.kindRole = static_cast<uint16>(NkRole::TextMuted);
					const bool muet = m.frise.ObjectFlag(t.object, 0);
					const bool verrou = m.frise.ObjectFlag(t.object, 2);
					const bool perdu = !m.Entite(t.object.CStr()).IsValid();
					if (perdu) {
						n.kindLabel = "entité absente";
						n.kindRole = static_cast<uint16>(NkRole::StatusWarn);
					}
					n.icon = NkFamilleIconeNoeud(NkFamilleNature::Entite, muet, verrou, false, false);
					n.hidden = muet;
				}
				(void)Ajouter(n, l);
			}

			// La selection montree : la piste choisie, sinon l'entite choisie.
			a.chosen.Clear();
			a.active = 0u;
			if (m.choix == NkScenaChoix::Piste || m.choix == NkScenaChoix::Plan || m.choix == NkScenaChoix::Cle) {
				const NkTimelineTrack *t = m.frise.Track(m.frise.activeTrack);
				if (t != nullptr) {
					for (uint32 i = 0; i < mLignes.Size(); ++i) {
						const bool memeObjet = !t->object.Empty() && mLignes[i].objet == t->object;
						if (mLignes[i].piste == t->id || memeObjet) {
							a.active = a.nodes[i].id;
						}
					}
				}
			} else if (sc.SelectionValide()) {
				a.active = sc.selection.Pack() + 16u;
			}
			if (a.active != 0u) {
				a.chosen.PushBack(a.active);
			}
			NkFamilleOutlinerRappels r;
			r.user = this;
			r.activer = &SurActiver;
			r.drapeau = &SurDrapeau;
			r.renommer = &SurRenommer;
			r.menu = &SurMenu;
			r.nouvelle = &SurNouvelle;
			const NkString pied = NkString::Format("%u entités  ·  %u pistes", static_cast<unsigned>(ids.Size()),
												   static_cast<unsigned>(m.frise.tracks.Size()));
			const NkFamilleOutlinerResultat res = NkFamilleDessinerOutliner(c, plan.outliner, mOutliner, r, pied.CStr(), mDt,
																			"Outliner", "+ Piste");
			if (res.selectionChangee) {
				const NkLigne *l = Ligne(*this, res.actif);
				if (l != nullptr && l->entite.IsValid()) {
					sc.Choisir(l->entite);
					m.choix = NkScenaChoix::Entite;
				} else if (l != nullptr && l->piste != 0) {
					m.frise.activeTrack = l->piste;
					m.choix = NkScenaChoix::Piste;
					const NkEntityId e = m.Entite(l->objet.CStr());
					if (e.IsValid()) {
						sc.Choisir(e);
					}
				} else {
					m.choix = NkScenaChoix::Sequence;
				}
			}
		}

		// =====================================================================
		// LES DETAILS
		// =====================================================================
		void NkScenaInterface::DetailsEntite(NkFamilleCtx &c, NkFamilleInspecteur &I, NkEntityId id) {
			NkScenaModele &m = M();
			nogee::NogeeModele &sc = m.scene;
			NkWorld &w = sc.Monde();
			const NkString nom(sc.Nom(id));
			if (NkTransform *tf = w.Get<NkTransform>(id)) {
				if (I.Carte(0, "Transform", NkFamilleIconeCarte::Transform, NkFamilleCategorie::General)) {
					float32 p[3] = {tf->localPosition.x, tf->localPosition.y, tf->localPosition.z};
					float32 r[3];
					NkScenaDegres(tf->localRotation, r);
					float32 s[3] = {tf->localScale.x, tf->localScale.y, tf->localScale.z};
					if (I.Vecteur("Position (m)", p, 3, 0.05f)) {
						tf->localPosition = NkVec3f{p[0], p[1], p[2]};
						tf->worldDirty = true;
					}
					if (I.Vecteur("Rotation (°)", r, 3, 0.5f)) {
						tf->localRotation = NkScenaRotation(r);
						tf->worldDirty = true;
					}
					if (I.Vecteur("Échelle", s, 3, 0.01f)) {
						tf->localScale = NkVec3f{s[0], s[1], s[2]};
						tf->worldDirty = true;
					}
					I.Ligne("Une pose se RETIENT par une clé (I) : sinon la piste la reprend.");
					I.FinCarte();
				}
			}
			if (NkCameraComponent *cam = w.Get<NkCameraComponent>(id)) {
				if (I.Carte(1, "Caméra", NkFamilleIconeCarte::Camera, NkFamilleCategorie::Rendu)) {
					float32 v = cam->fovDeg;
					if (I.Nombre("Champ (°)", v, 0.5f, 5.f, 170.f)) {
						cam->fovDeg = v;
					}
					const bool plan0 = m.CameraDuPlan(m.frise.cursor) == id;
					I.Info("À l'image", plan0 ? "OUI — c'est la caméra du plan" : "non (aucun plan ne la filme ici)");
					if (I.Boutons("Un plan de cette caméra au curseur") == 0) {
						mNomsMenu.Clear();
						mNomsMenu.PushBack(nom);
						mTempsMenuPlan = m.frise.cursor;
						Executer(SCENA_A_PLAN_DE);
					}
					I.FinCarte();
				}
			}
			if (I.Carte(2, "Séquence", NkFamilleIconeCarte::Animation, NkFamilleCategorie::Animation)) {
				const bool piste = m.APisteTransform(nom.CStr());
				uint32 cles = 0;
				for (uint32 i = 0; i < m.frise.tracks.Size(); ++i) {
					if (m.frise.tracks[i].object == nom) {
						cles += static_cast<uint32>(m.frise.tracks[i].keys.Size());
					}
				}
				I.Info("Piste", piste ? "transformation (position, rotation, échelle)" : "aucune");
				I.Info("Clés", NkString::Format("%u", static_cast<unsigned>(cles)).CStr());
				const int32 b = I.Boutons(piste ? "Poser une clé (I)" : "Ajouter sa piste", piste ? "Clé suivante »" : nullptr, true, piste);
				if (b == 0) {
					Executer(piste ? SCENA_A_CLES_ENTITE : SCENA_A_PISTE_ENTITE);
				} else if (b == 1) {
					m.frise.SetCursor(m.frise.NextKeyTime(m.frise.cursor));
				}
				I.FinCarte();
			}
			(void)c;
		}

		void NkScenaInterface::DetailsPiste(NkFamilleCtx &c, NkFamilleInspecteur &I) {
			NkScenaModele &m = M();
			NkTimelineModel &f = m.frise;
			NkTimelineTrack *t = f.Track(f.activeTrack);
			if (t == nullptr) {
				I.Vide("Aucune piste choisie.", "Cliquez une piste dans la frise ou l'Outliner.");
				return;
			}
			if (t->kind == NkTimelineValueKind::Clips) {
				if (I.Carte(3, "Plans caméra", NkFamilleIconeCarte::Camera, NkFamilleCategorie::Animation)) {
					I.Info("Plans", NkString::Format("%u", static_cast<unsigned>(t->clips.Size())).CStr());
					for (uint32 k = 0; k < t->clips.Size(); ++k) {
						const NkTimelineClip &cl = t->clips[k];
						const NkString lib = NkString::Format("Plan %u", static_cast<unsigned>(k + 1));
						const NkString val = NkString::Format("%s  ·  %.2f à %.2f s", cl.name.CStr(), static_cast<double>(cl.start),
															  static_cast<double>(cl.End()));
						I.Info(lib.CStr(), val.CStr());
					}
					I.Ligne("« + » sur la piste (ou un double-clic dans son vide) pose un plan.");
					bool muet = t->muted;
					if (I.Case("Muette (aucune coupe)", muet)) {
						f.ToggleMute(t->id);
					}
					I.FinCarte();
				}
				(void)c;
				return;
			}
			if (I.Carte(3, "Piste", NkFamilleIconeCarte::Transform, NkFamilleCategorie::Animation)) {
				const bool perdue = !m.Entite(t->object.CStr()).IsValid();
				I.Info("Entité", perdue ? NkString::Format("%s (ABSENTE de la scène)", t->object.CStr()).CStr() : t->object.CStr());
				I.Info("Propriété", t->property.CStr());
				const char *prefixe = NkScenaPrefixeCanal(t->property);
				I.Info("Canaux de Noge", prefixe != nullptr ? NkString::Format("%s.x / .y / .z", prefixe).CStr() : "—");
				I.Info("Clés", NkString::Format("%u", static_cast<unsigned>(t->keys.Size())).CStr());
				bool muet = t->muted;
				if (I.Case("Muette", muet)) {
					f.ToggleMute(t->id);
				}
				bool solo = t->solo;
				if (I.Case("Solo", solo)) {
					f.ToggleSolo(t->id);
				}
				bool verrou = t->locked;
				if (I.Case("Verrouillée", verrou)) {
					f.ToggleLock(t->id);
				}
				if (I.Boutons("Poser une clé au curseur (I)") == 0) {
					(void)m.PoserCle(t->id, f.cursor);
				}
				I.FinCarte();
			}
			(void)c;
		}

		void NkScenaInterface::DetailsCle(NkFamilleCtx &c, NkFamilleInspecteur &I) {
			NkScenaModele &m = M();
			NkTimelineModel &f = m.frise;
			// La PREMIERE cle choisie (et le nombre des autres).
			NkTimelineTrack *piste = nullptr;
			int32 k = -1;
			for (uint32 i = 0; i < f.tracks.Size() && k < 0; ++i) {
				for (uint32 j = 0; j < f.tracks[i].keys.Size(); ++j) {
					if (f.tracks[i].keys[j].selected) {
						piste = &f.tracks[i];
						k = static_cast<int32>(j);
						break;
					}
				}
			}
			if (piste == nullptr) {
				I.Vide("Aucune clé choisie.", "Cliquez un losange de la frise.");
				return;
			}
			// Un GESTE = un seul pas d'annulation (le patron de Nogee : `sEnCours`).
			static bool sEnCours = false;
			if (!c.ctx.input.mouseDown[0] && c.ctx.inputId == nkgui::NKGUI_ID_NONE) {
				sEnCours = false;
			}
			auto Avant = [&]() {
				if (!sEnCours) {
					f.PushUndo();
					sEnCours = true;
				}
			};
			if (I.Carte(4, "Clé", NkFamilleIconeCarte::Animation, NkFamilleCategorie::Animation)) {
				NkTimelineKey &cle = piste->keys[static_cast<uint32>(k)];
				I.Info("Piste", NkString::Format("%s / %s", piste->object.CStr(), piste->property.CStr()).CStr());
				const uint32 n = f.SelectionCount();
				if (n > 1u) {
					I.Info("Choisies", NkString::Format("%u clés (la première est montrée)", static_cast<unsigned>(n)).CStr());
				}
				I.Info("Instant", NkString::Format("%.3f s  ·  image %d", static_cast<double>(cle.time), f.FrameOf(cle.time)).CStr());
				float32 v[3] = {cle.v[0], cle.v[1], cle.v[2]};
				const char *lib = piste->property == NkString(kNkScenaRotation) ? "Valeur (°)" : "Valeur";
				if (!piste->locked && I.Vecteur(lib, v, 3, 0.01f)) {
					Avant();
					cle.v[0] = v[0];
					cle.v[1] = v[1];
					cle.v[2] = v[2];
					f.Touch();
				}
				static const char *const kInterp[6] = {"Palier", "Linéaire", "Courbe", "Entrée", "Sortie", "Entrée-sortie"};
				int32 ip = cle.interp < 6 ? cle.interp : 5;
				if (I.Choix("Vers la suivante", kInterp, 3, ip) && ip >= 0 && ip < 3) {
					f.SetSelectionInterp(static_cast<uint8>(ip));
				}
				int32 ip2 = cle.interp >= 3 && cle.interp < 6 ? cle.interp - 3 : -1;
				if (I.Choix("", kInterp + 3, 3, ip2) && ip2 >= 0) {
					f.SetSelectionInterp(static_cast<uint8>(ip2 + 3));
				}
				if (cle.interp >= 6) {
					I.Ligne("Rebond, élastique, recul : Noge les joue en entrée-sortie.", true);
				}
				const int32 b = I.Boutons("Aller à la clé", "Supprimer");
				if (b == 0) {
					f.SetCursor(cle.time);
				} else if (b == 1) {
					(void)f.DeleteSelection();
				}
				I.FinCarte();
			}
		}

		void NkScenaInterface::DetailsPlan(NkFamilleCtx &c, NkFamilleInspecteur &I) {
			NkScenaModele &m = M();
			NkTimelineModel &f = m.frise;
			nk_uint64 pisteId = 0;
			NkTimelineClip *cl = f.FindClip(f.activeClip, &pisteId);
			if (cl == nullptr) {
				I.Vide("Aucun plan choisi.", "Cliquez un plan sur la piste « Plans caméra ».");
				return;
			}
			static bool sEnCours = false;
			if (!c.ctx.input.mouseDown[0] && c.ctx.inputId == nkgui::NKGUI_ID_NONE) {
				sEnCours = false;
			}
			auto Avant = [&]() {
				if (!sEnCours) {
					f.PushUndo();
					sEnCours = true;
				}
			};
			if (I.Carte(5, "Plan caméra", NkFamilleIconeCarte::Camera, NkFamilleCategorie::Animation)) {
				const bool present = m.Entite(cl->name.CStr()).IsValid();
				I.Info("Caméra", present ? cl->name.CStr() : NkString::Format("%s (ABSENTE)", cl->name.CStr()).CStr());
				float32 debut = cl->start;
				if (I.Nombre("Début (s)", debut, 1.f / f.fps, 0.f, 3600.f)) {
					Avant();
					cl->start = f.Snap(debut);
					f.SortClips();
					f.Touch();
					cl = f.FindClip(f.activeClip);
				}
				if (cl != nullptr) {
					float32 duree = cl->length;
					if (I.Nombre("Durée (s)", duree, 1.f / f.fps, f.FrameDuration(), 3600.f)) {
						Avant();
						cl->length = duree;
						cl->sourceLength = duree;
						if (cl->End() > f.duration) {
							f.duration = cl->End();
						}
						f.Touch();
					}
					float32 fondu = cl->blendIn;
					if (I.Nombre("Fondu d'entrée (s)", fondu, 0.05f, 0.f, cl->length)) {
						Avant();
						cl->blendIn = fondu;
						f.Touch();
					}
					I.Info("Coupe", cl->blendIn > 1e-6f ? "fondu (Blend)" : "franche (Cut)");
					const int32 b = I.Boutons("Aller au début", "Regarder par elle");
					if (b == 0) {
						f.SetCursor(cl->start);
					} else if (b == 1) {
						f.SetCursor(cl->start);
						m.vueCamera = true;
					}
				}
				I.FinCarte();
			}
		}

		void NkScenaInterface::DetailsSequence(NkFamilleCtx &c, NkFamilleInspecteur &I) {
			NkScenaModele &m = M();
			NkTimelineModel &f = m.frise;
			if (I.Carte(6, "Séquence", NkFamilleIconeCarte::Animation, NkFamilleCategorie::General)) {
				I.Info("Fichier", m.chemin.CStr());
				I.Info("Scène", m.CheminScene().CStr());
				float32 duree = f.duration;
				if (I.Nombre("Durée (s)", duree, 0.1f, 0.1f, 3600.f)) {
					f.duration = duree;
					f.Touch();
				}
				float32 ips = f.fps;
				if (I.Nombre("Images / s", ips, 1.f, 1.f, 240.f)) {
					f.fps = ips;
					f.Touch();
				}
				I.Info("Plage de lecture", NkString::Format("%.2f à %.2f s", static_cast<double>(f.PlayStart()),
															static_cast<double>(f.PlayEnd()))
											   .CStr());
				if (m.CiblesPerdues() > 0u) {
					I.Ligne(NkString::Format("%u cible(s) absente(s) de la scène : elles n'animent rien.",
											 static_cast<unsigned>(m.CiblesPerdues()))
								.CStr(),
							true);
				}
				I.FinCarte();
			}
			if (I.Carte(7, "Rendu", NkFamilleIconeCarte::Camera, NkFamilleCategorie::Rendu)) {
				int32 w = static_cast<int32>(m.sortie.largeur);
				int32 h = static_cast<int32>(m.sortie.hauteur);
				if (I.Entier("Largeur (px)", w, 64, 7680)) {
					m.sortie.largeur = static_cast<uint32>(w);
					f.Touch();
				}
				if (I.Entier("Hauteur (px)", h, 64, 4320)) {
					m.sortie.hauteur = static_cast<uint32>(h);
					f.Touch();
				}
				I.Info("Dossier", m.sortie.dossier.CStr());
				I.Info("Fichiers", NkString::Format("%s_0001.png …", m.sortie.prefixe.CStr()).CStr());
				I.Info("Images", NkString::Format("%d", static_cast<int32>((f.PlayEnd() - f.PlayStart()) * f.fps + 0.5f)).CStr());
				const int32 b = I.Boutons("Rendre (Ctrl+R)", "Enregistrer (Ctrl+S)");
				if (b == 0) {
					Executer(SCENA_A_RENDRE);
				} else if (b == 1) {
					Executer(SCENA_A_ENREGISTRER);
				}
				I.FinCarte();
			}
			(void)c;
		}

		void NkScenaInterface::Details(NkFamilleCtx &c) {
			if (!plan.voirDetails || plan.details.w < 8.f || plan.details.h < 60.f) {
				return;
			}
			NkScenaModele &m = M();
			const NkRect &zone = plan.details;
			Gui().dl.AddRectFilled(zone, pal.panneau);
			static const char *const kOnglets[2] = {"Détails", "Séquence"};
			const float32 ongletsH = NkFamilleCotes::kOngletPanneau;
			(void)NkFamilleOnglets(c, NkRect{zone.x, zone.y, zone.w, ongletsH}, kOnglets, 2, mOngletDroite);
			const NkRect contenu{zone.x, zone.y + ongletsH, zone.w, zone.h - ongletsH};
			Gui().dl.PushClipRect(contenu, true);
			NkFamilleInspecteur I(c, mDetails, contenu);
			NkScenaChoix choix = mOngletDroite == 1 ? NkScenaChoix::Sequence : m.choix;
			if (choix == NkScenaChoix::Entite && !m.scene.SelectionValide()) {
				choix = NkScenaChoix::Sequence;
			}
			if (choix == NkScenaChoix::Rien) {
				choix = NkScenaChoix::Sequence;
			}
			// L'EN-TETE d'Unreal, puis les cartes de ce qui est choisi.
			NkString nom;
			NkString type;
			uint64 cle = 0;
			NkFamilleComposant comps[4];
			int32 n = 0;
			switch (choix) {
				case NkScenaChoix::Entite:
					nom = NkString(m.scene.Nom(m.scene.selection));
					type = NkString::Format("entité : %s", m.scene.Type(m.scene.selection));
					cle = m.scene.selection.Pack();
					comps[n++] = NkFamilleComposant{0, "Transform", NkFamilleIconeCarte::Transform};
					if (m.scene.Monde().Has<NkCameraComponent>(m.scene.selection)) {
						comps[n++] = NkFamilleComposant{1, "Caméra", NkFamilleIconeCarte::Camera};
					}
					comps[n++] = NkFamilleComposant{2, "Séquence", NkFamilleIconeCarte::Animation};
					break;
				case NkScenaChoix::Piste: {
					const NkTimelineTrack *t = m.frise.Track(m.frise.activeTrack);
					nom = t != nullptr ? (t->object.Empty() ? t->property : NkString::Format("%s / %s", t->object.CStr(), t->property.CStr()))
									   : NkString("Piste");
					type = "piste de la frise";
					cle = 0x1000000000ull + m.frise.activeTrack;
					comps[n++] = NkFamilleComposant{3, "Piste", NkFamilleIconeCarte::Transform};
					break;
				}
				case NkScenaChoix::Cle:
					nom = "Clé";
					type = NkString::Format("%u clé(s) choisie(s)", static_cast<unsigned>(m.frise.SelectionCount()));
					cle = 0x2000000000ull;
					comps[n++] = NkFamilleComposant{4, "Clé", NkFamilleIconeCarte::Animation};
					break;
				case NkScenaChoix::Plan: {
					const NkTimelineClip *cl = m.frise.FindClip(m.frise.activeClip);
					nom = cl != nullptr ? NkString::Format("Plan : %s", cl->name.CStr()) : NkString("Plan");
					type = "plan caméra";
					cle = 0x3000000000ull + m.frise.activeClip;
					comps[n++] = NkFamilleComposant{5, "Plan caméra", NkFamilleIconeCarte::Camera};
					break;
				}
				default:
					nom = m.nom;
					type = NkString::Format("séquence  ·  %.0f i/s  ·  %.2f s", static_cast<double>(m.frise.fps),
											static_cast<double>(m.frise.duration));
					cle = 0x4000000000ull;
					comps[n++] = NkFamilleComposant{6, "Séquence", NkFamilleIconeCarte::Animation};
					comps[n++] = NkFamilleComposant{7, "Rendu", NkFamilleIconeCarte::Camera};
					break;
			}
			NkString nouveau;
			// « + Ajouter » ajoute ce que NKScena sait ajouter : une PISTE.
			if (I.Entete(cle, nom.CStr(), nullptr, type.CStr(), menus.menu == SCENA_MENU_PISTE_PLUS, &nouveau)) {
				menus.Ouvrir(SCENA_MENU_PISTE_PLUS, mDetails.ajouter);
			}
			if (!nouveau.Empty() && choix == NkScenaChoix::Sequence) {
				m.nom = nouveau;
				m.frise.rootLabel = nouveau;
				m.frise.Touch();
			}
			char cleTexte[48];
			std::snprintf(cleTexte, sizeof(cleTexte), "s%llu", static_cast<unsigned long long>(cle));
			if (I.Debut(cleTexte, nom.CStr(), comps, n)) {
				switch (choix) {
					case NkScenaChoix::Entite:
						DetailsEntite(c, I, m.scene.selection);
						break;
					case NkScenaChoix::Piste:
						DetailsPiste(c, I);
						break;
					case NkScenaChoix::Cle:
						DetailsCle(c, I);
						break;
					case NkScenaChoix::Plan:
						DetailsPlan(c, I);
						break;
					default:
						DetailsSequence(c, I);
						break;
				}
				(void)I.Fin(nullptr, false);
			}
			Gui().dl.PopClipRect();
		}

		// =====================================================================
		// LE TIROIR : la FRISE (au premier plan), Contenu, Journal, Terminal
		// =====================================================================
		void NkScenaInterface::Frise(NkFamilleCtx &c, const NkRect &zone) {
			NkScenaModele &m = M();
			NkGuiComponentPaint peintre(Gui(), theme);
			NkTimelineHooks hooks;
			hooks.user = this;
			hooks.evaluate = &EvaluerPiste;
			hooks.readLive = &LireVivant;
			m.frise.rootLabel = m.nom;
			mFriseResultat = NkDrawTimeline(peintre, NkFamilleEntreeComposant(Gui()), NkPaintRect{zone.x, zone.y, zone.w, zone.h},
											m.frise, StyleFrise(), hooks);
			const NkTimelineResult &r = mFriseResultat;
			const nkgui::NkVec2 souris = Gui().input.mousePos;
			if (r.addTrackRequested) {
				menus.Ouvrir(SCENA_MENU_PISTE_PLUS, NkRect{souris.x, souris.y, 0.f, 0.f});
			}
			if (r.addClipRequested) {
				mTempsMenuPlan = r.requestTime;
				menus.Ouvrir(SCENA_MENU_PLAN_ICI, NkRect{souris.x, souris.y, 0.f, 0.f});
			}
			SuivreChoixDeLaFrise();
			if (m.frise.tracks.Empty()) {
				// La frise VIDE dit quoi faire (la regle de la famille : un vide qui parle).
				const NkRect &a = NkRect{r.area.x, r.area.y, r.area.w, r.area.h};
				NkFamilleTexteCentre(c.ctx.dl, c.police, a.x + a.w * 0.5f, a.y + a.h * 0.40f,
									 "La frise est vide : « + Piste » pose une piste (une entité, ou les plans caméra)",
									 pal.attenue);
				NkFamilleTexteCentre(c.ctx.dl, c.petite, a.x + a.w * 0.5f, a.y + a.h * 0.40f + 22.f,
									 "Fichier > Exemple montre une séquence faite : le joueur traverse, la caméra avance",
									 pal.attenue);
			}
		}

		void NkScenaInterface::SuivreChoixDeLaFrise() {
			NkScenaModele &m = M();
			const NkTimelineModel &f = m.frise;
			const uint32 sel = f.SelectionCount();
			if (!mFriseSuivie) {
				// La premiere trame ne fait que RELEVER : ce que la frise portait en
				// s'ouvrant (le plan pose par l'exemple...) n'est pas un choix.
				mFriseSuivie = true;
			} else if (f.activeClip != mFriseClipAvant && f.activeClip != 0) {
				m.choix = NkScenaChoix::Plan;
			} else if (sel != mFriseSelectionAvant && sel > 0u) {
				m.choix = NkScenaChoix::Cle;
			} else if (f.activeTrack != mFriseActiveAvant && f.activeTrack != 0) {
				m.choix = NkScenaChoix::Piste;
				if (const NkTimelineTrack *t = f.Track(f.activeTrack)) {
					const NkEntityId e = m.Entite(t->object.CStr());
					if (e.IsValid()) {
						m.scene.Choisir(e);
					}
				}
			} else if (sel == 0u && m.choix == NkScenaChoix::Cle) {
				m.choix = NkScenaChoix::Piste;
			}
			mFriseActiveAvant = f.activeTrack;
			mFriseClipAvant = f.activeClip;
			mFriseSelectionAvant = sel;
		}

		void NkScenaInterface::Tiroir(NkFamilleCtx &c) {
			const NkRect &zone = plan.tiroir;
			if (!plan.voirTiroir || zone.w < 8.f || zone.h < 40.f) {
				return;
			}
			static const char *const kOnglets[4] = {"Frise", "Contenu", "Journal", "Terminal"};
			const float32 ongletsH = NkFamilleCotes::kOngletPanneau;
			(void)NkFamilleOnglets(c, NkRect{zone.x, zone.y, zone.w, ongletsH}, kOnglets, 4, mOngletTiroir);
			const NkRect contenu{zone.x, zone.y + ongletsH, zone.w, zone.h - ongletsH};
			Gui().dl.PushClipRect(contenu, true);
			if (mOngletTiroir == 0) {
				Frise(c, contenu);
			} else if (mOngletTiroir == 1) {
				if (NkFamilleDessinerContenu(c, contenu, mContenu, kNatures, static_cast<int32>(sizeof(kNatures) / sizeof(kNatures[0])),
											 mDt)) {
					// Le double-clic : une sequence s'ouvre, une scene s'ouvre.
					const NkString f = mContenu.ouvert;
					if (FinitPar(f, ".nkseq")) {
						(void)M().Ouvrir(f.CStr());
					} else if (FinitPar(f, ".nkscene3d") || FinitPar(f, ".nkscene")) {
						(void)M().OuvrirScene(f.CStr());
					} else {
						M().Annoncer(NkString::Format("Ouvrir « %s » : pas dans NKScena", f.CStr()).CStr(), 2);
					}
				}
			} else if (mOngletTiroir == 2) {
				NkFamilleDessinerJournal(c, contenu, mJournal, mDt);
			} else {
				mTerminal.Dessiner(Gui(), Gui().dl, contenu, theme);
			}
			Gui().dl.PopClipRect();
		}

	} // namespace nkscena
} // namespace nkentseu
