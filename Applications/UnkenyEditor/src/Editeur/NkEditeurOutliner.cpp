//
// NkEditeurOutliner.cpp
// =============================================================================
// Description :
//   L'Outliner : la scene et ses entites, en ARBRE DU KIT (NkDrawTreeView),
//   colonnes Nom | Type, champ de recherche, pied « N entites (1 sel.) ».
//
// Caracteristiques :
//   - Le modele d'arbre est RECONSTRUIT a chaque trame depuis la scene : il
//     n'y a donc qu'une verite (la scene), jamais une liste a synchroniser.
//     L'etat propre a l'arbre (noeuds replies, defilement) survit, parce que
//     l'identifiant d'un noeud est celui de l'entite.
//   - La selection est celle du MODELE : cliquer dans le viseur la change ici,
//     cliquer ici la change dans le viseur.
//   - Double-clic sur une entite : la vue se centre dessus.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurInterface.h"

#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKEditorKit/Components/NkGuiComponentPaint.h"
#include "NKEditorKit/NkEditorScrollbar.h"
#include "NKEditorKit/NkEditorTextField.h"

namespace nkentseu {
	namespace editeur {

		using editorkit::NkRole;
		using nkgui::NkColor;
		using nkgui::NkRect;

		namespace {

			/// L'identifiant de noeud de la racine. 0 est reserve par le contrat
			/// du composant (« aucun »), 1 est la scene, les entites commencent a 2.
			constexpr nk_uint64 ID_RACINE = 1u;

			nk_uint64 IdNoeud(ecs::NkEntityId e) noexcept {
				return static_cast<nk_uint64>(e.Pack()) + 2u;
			}

			/// Le libelle d'une entite : son etiquette, ou son indice a defaut.
			NkString NomDe(NkScene &scene, ecs::NkEntityId id) {
				const NkEtiquette *e = scene.Monde().Get<NkEtiquette>(id);
				if (e != nullptr && e->nom[0] != '\0') {
					return NkString(e->nom);
				}
				return NkString::Format("Entite %u", static_cast<uint32>(id.index));
			}

			/// Double-clic : la vue se centre sur l'entite.
			void SurActivation(void *user, int32 index, const char *id) {
				(void)id;
				NkEditeurCadre &c = *static_cast<NkEditeurCadre *>(user);
				if (index < 0 || index >= static_cast<int32>(c.ui.arbreEntites.Size())) {
					return;
				}
				const ecs::NkEntityId e = c.ui.arbreEntites[static_cast<uint32>(index)];
				if (!c.m.scene.Monde().IsAlive(e)) {
					return;
				}
				c.m.selection = e;
				c.m.aSelection = true;
				NkVec2f centre;
				if (NkEditeurCentreSelection(c.m, centre)) {
					c.m.scene.Camera().PoserCentre(centre);
				}
			}

			void PreparerReglages(NkEditeurInterface &ui) {
				if (ui.arbrePret) {
					return;
				}
				ui.arbrePret = true;
				ui.arbreReglages.Bind(editorkit::NkTreeViewDecl());
				// L'en-tete, la recherche et le pied sont peints par l'Outliner :
				// ceux du composant disent « Arbre » et « noeud(s) », pas « Outliner »
				// et « entites ».
				ui.arbreReglages.SetParam("show_header", 0.f);
				ui.arbreReglages.SetParam("show_search", 0.f);
				ui.arbreReglages.SetParam("show_footer", 0.f);
				ui.arbreReglages.SetParam("show_visibility", 0.f);
				ui.arbreReglages.SetParam("show_type", 1.f);
				ui.arbreReglages.SetParam("indent_guides", 0.f);
				ui.arbreReglages.SetParam("multi_select", 0.f);
				ui.arbreReglages.SetParam("range_select", 0.f);
				// Le renommage vit dans les Details (le champ Nom) : le double-clic
				// ACTIVE, il n'ouvre pas une seconde saisie du meme nom.
				ui.arbreReglages.SetParam("activate_on_double_click", 1.f);
				ui.arbreReglages.SetMetric("row_h", 22.f);
			}

			bool Contient(const NkVector<ecs::NkEntityId> &v, ecs::NkEntityId e) noexcept {
				for (uint32 i = 0; i < v.Size(); ++i) {
					if (v[i] == e) {
						return true;
					}
				}
				return false;
			}

			/// L'ordre de l'Outliner : celui de la trame d'avant, les disparues
			/// retirees, les nouvelles AJOUTEES A LA FIN.
			/// ⚠️ NE PAS PRENDRE L'ORDRE DE `Entites()` TEL QUEL : l'ECS range ses
			///    entites par ARCHETYPE, et ajouter un Sprite a une entite la faisait
			///    changer de ligne sous le curseur (mesure du 2026-09-29 : « Entite »
			///    passait de la 1re a la 8e place). Quadratique, et assume : un
			///    Outliner de quelques centaines de lignes.
			void Ordonner(NkEditeurInterface &ui, const NkVector<ecs::NkEntityId> &ids, NkVector<ecs::NkEntityId> &sortie) {
				sortie.Clear();
				for (uint32 i = 0; i < ui.ordreArbre.Size(); ++i) {
					if (Contient(ids, ui.ordreArbre[i])) {
						sortie.PushBack(ui.ordreArbre[i]);
					}
				}
				for (uint32 i = 0; i < ids.Size(); ++i) {
					if (!Contient(sortie, ids[i])) {
						sortie.PushBack(ids[i]);
					}
				}
				ui.ordreArbre = sortie;
			}

			void Reconstruire(NkEditeurCadre &c) {
				NkEditeurInterface &ui = c.ui;
				NkVector<ecs::NkEntityId> brut;
				c.m.scene.Entites(brut);
				NkVector<ecs::NkEntityId> ids;
				Ordonner(ui, brut, ids);
				ui.arbre.nodes.Clear();
				ui.arbreEntites.Clear();

				editorkit::NkTreeNode racine;
				racine.id = ID_RACINE;
				racine.parent = -1;
				racine.label = NkString("Scène");
				racine.kindLabel = "Monde";
				racine.kindRole = static_cast<uint16>(NkRole::TextMuted);
				ui.arbre.nodes.PushBack(racine);
				ui.arbreEntites.PushBack(ecs::NkEntityId::Invalid());

				for (uint32 i = 0; i < ids.Size(); ++i) {
					editorkit::NkTreeNode n;
					n.id = IdNoeud(ids[i]);
					n.parent = 0;
					n.label = NomDe(c.m.scene, ids[i]);
					// ⚠️ Chaine STATIQUE (NkEditeurTypeDe rend un litteral) : le
					//    noeud ne garde qu'un pointeur, il ne copie pas.
					n.kindLabel = NkEditeurTypeDe(c.m.scene, ids[i]);
					n.kindRole = static_cast<uint16>(NkRole::TextMuted);
					n.userTag = i + 1u;
					ui.arbre.nodes.PushBack(n);
					ui.arbreEntites.PushBack(ids[i]);
				}

				// La selection du MODELE est celle que l'arbre montre.
				ui.arbre.chosen.Clear();
				if (c.m.aSelection && c.m.scene.Monde().IsAlive(c.m.selection)) {
					ui.arbre.active = IdNoeud(c.m.selection);
					ui.arbre.chosen.PushBack(ui.arbre.active);
				} else {
					ui.arbre.active = 0;
				}
			}

			/// Le champ de recherche. Sa couleur vient du theme ; le champ du kit
			/// n'en fournit que la saisie (caret, selection, copier-coller).
			void Recherche(NkEditeurCadre &c, const NkRect &r) {
				NkEditeurInterface &ui = c.ui;
				const nkgui::NkGuiInput &in = c.ctx.input;
				if (in.mouseClicked[0]) {
					ui.filtreFocus = NkEditeurDans(r, in.mousePos);
				}
				if (ui.filtreFocus && (in.KeyPressed(nkgui::NkGuiKey::Escape) || in.KeyPressed(nkgui::NkGuiKey::Enter))) {
					ui.filtreFocus = false;
				}
				c.ctx.dl.AddRectFilled(r, c.pal.champ, 2.f);
				c.ctx.dl.AddRect(r, ui.filtreFocus ? c.pal.accent : c.pal.bord, 1.f, 2.f);
				editorkit::NkOverlayFieldStyle st;
				st.fond = false;
				st.bord = false;
				st.texte = c.pal.texte;
				const NkRect champ{r.x + 6.f, r.y, r.w - 8.f, r.h};
				editorkit::NkOverlayTextField(c.ctx, c.ctx.dl, c.police, champ, ui.arbre.filter,
											  static_cast<int32>(sizeof(ui.arbre.filter)), ui.filtreFocus, &st);
				if (!ui.filtreFocus && ui.arbre.filter[0] == '\0') {
					const float32 ty = r.y + (r.h - renderer::NkTexteHauteurLigne(c.police, 16.f)) * 0.5f;
					renderer::NkTexte(c.ctx.dl, c.police, r.x + 9.f, ty, "Rechercher…", c.pal.attenue);
				}
			}

		} // namespace

		void NkEditeurDessinerOutliner(NkEditeurCadre &c) {
			NkEditeurInterface &ui = c.ui;
			const NkRect zone = ui.outliner;
			if (!ui.voirOutliner || zone.w < 8.f || zone.h < 60.f) {
				return;
			}
			auto &dl = c.ctx.dl;
			dl.AddRectFilled(zone, c.pal.panneau);
			PreparerReglages(ui);
			Reconstruire(c);

			// ── L'en-tete : le nom du panneau, et « + Entite » ────────────────
			const float32 enteteH = 26.f;
			const NkRect entete{zone.x, zone.y, zone.w, enteteH};
			dl.AddRectFilled(entete, c.pal.entete);
			renderer::NkTexte(dl, c.police, entete.x + 8.f,
							  entete.y + (enteteH - renderer::NkTexteHauteurLigne(c.police, 16.f)) * 0.5f, "Outliner",
							  c.pal.texte);
			const float32 bw = renderer::NkTexteLargeur(c.petite, "+ Entité") + 14.f;
			const NkRect plus{entete.x + entete.w - bw - 4.f, entete.y + 3.f, bw, enteteH - 6.f};
			if (NkEditeurBouton(c, plus, "", false)) {
				NkEditeurExecuter(c, NK_A_NOUVELLE_ENTITE);
			}
			renderer::NkTexteDansBoite(dl, c.petite, plus, "+ Entité", c.pal.texte);

			// ── La recherche ──────────────────────────────────────────────────
			const NkRect recherche{zone.x + 4.f, zone.y + enteteH + 3.f, zone.w - 8.f, 22.f};
			Recherche(c, recherche);

			// ── Les colonnes ──────────────────────────────────────────────────
			const float32 colonnesY = recherche.y + recherche.h + 3.f;
			const float32 colonnesH = 20.f;
			const NkRect colonnes{zone.x, colonnesY, zone.w, colonnesH};
			dl.AddRectFilled(colonnes, c.pal.entete);
			dl.AddRectFilled(NkRect{zone.x, colonnesY + colonnesH - 1.f, zone.w, 1.f}, c.pal.bord);
			const float32 cty = colonnesY + (colonnesH - renderer::NkTexteHauteurLigne(c.petite, 12.f)) * 0.5f;
			renderer::NkTexte(dl, c.petite, zone.x + 10.f, cty, "Nom", c.pal.attenue);
			// Le composant cale le type a droite, avant la gouttiere de defilement.
			renderer::NkTexteADroite(dl, c.petite, zone.x + zone.w - 22.f, cty, "Type", c.pal.attenue);

			// ── L'arbre du kit ────────────────────────────────────────────────
			const float32 piedH = 22.f;
			const float32 arbreY = colonnesY + colonnesH;
			const NkRect arbreR{zone.x, arbreY, zone.w, zone.y + zone.h - piedH - arbreY};
			editorkit::NkTreeViewStyle s;
			s.values = &ui.arbreReglages;
			s.panelBg = static_cast<uint16>(NkRole::PanelBg);
			s.headerBg = static_cast<uint16>(NkRole::PanelHeader);
			s.border = static_cast<uint16>(NkRole::Border);
			s.text = static_cast<uint16>(NkRole::Text);
			s.textMuted = static_cast<uint16>(NkRole::TextMuted);
			s.rowHover = static_cast<uint16>(NkRole::InputBg);
			// Le BLEU dit la selection dans une LISTE (UI_SPEC §3.2) ; l'ambre est
			// reserve a la selection dans la scene, dans le viseur.
			s.activeMark = static_cast<uint16>(NkRole::AccentUi);
			s.activeText = static_cast<uint16>(NkRole::TextOnAccent);
			s.chosenMark = static_cast<uint16>(NkRole::AccentUi);
			s.guide = static_cast<uint16>(NkRole::Border);
			s.dropMark = static_cast<uint16>(NkRole::AccentUi);
			s.iconTint = static_cast<uint16>(NkRole::TextMuted);
			s.dimTint = static_cast<uint16>(NkRole::TextMuted);
			editorkit::NkTreeViewHooks hooks;
			hooks.user = &c;
			hooks.onActivate = &SurActivation;

			const editorkit::NkComponentInput ci = NkEditeurEntreeComposant(c.ctx);
			editorkit::NkGuiComponentPaint peintre(c.ctx, c.theme);
			const editorkit::NkTreeViewResult res = editorkit::NkDrawTreeView(
				peintre, ci, editorkit::NkPaintRect{arbreR.x, arbreR.y, arbreR.w, arbreR.h}, ui.arbre, s, hooks);

			if (res.selectionChanged) {
				const int32 k = ui.arbre.IndexOf(ui.arbre.active);
				if (k > 0 && k < static_cast<int32>(ui.arbreEntites.Size())) {
					c.m.selection = ui.arbreEntites[static_cast<uint32>(k)];
					c.m.aSelection = true;
				} else {
					// La racine, ou le vide : rien n'est selectionne.
					c.m.aSelection = false;
				}
			}
			// La barre de defilement standard du kit, dans la gouttiere que le
			// composant a reservee et nous a rapportee.
			if (res.defilContenu > res.defilVue && res.defilW > 0.f && res.defilH > 0.f) {
				editorkit::NkVScrollbar(c.ctx, dl, NkRect{res.defilX, res.defilY, res.defilW, res.defilH}, ui.arbre.scroll,
										res.defilContenu, res.defilVue, c.ctx.GetId("outliner.defil"), res.defilPas);
			}

			// ── Le pied : le compte, et la selection ──────────────────────────
			const NkRect pied{zone.x, zone.y + zone.h - piedH, zone.w, piedH};
			dl.AddRectFilled(pied, c.pal.entete);
			dl.AddRectFilled(NkRect{pied.x, pied.y, pied.w, 1.f}, c.pal.bord);
			const uint32 n = ui.arbreEntites.Size() > 0 ? ui.arbreEntites.Size() - 1u : 0u;
			const NkString compte = NkString::Format("%u entités (%u sél.)", n, c.m.aSelection ? 1u : 0u);
			renderer::NkTexte(dl, c.petite, pied.x + 8.f,
							  pied.y + (piedH - renderer::NkTexteHauteurLigne(c.petite, 12.f)) * 0.5f, compte.CStr(),
							  c.pal.attenue);
		}

	} // namespace editeur
} // namespace nkentseu
