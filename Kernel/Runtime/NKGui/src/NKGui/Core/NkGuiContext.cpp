// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkGuiContext.cpp — contexte NKGui (Phase 2).
// =============================================================================
#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiFont.h"
#include "NKPlatform/NkEnv.h" // NK_GUI_INTROSPECT : activation sans recompiler
#include <cstddef> // offsetof — table de description des jetons
#include <cstdlib> // getenv : la mutation de banc de la garde des sentinelles

namespace nkentseu {
	namespace nkgui {

		const char *NkGuiVersion::String() noexcept {
			return "0.1.0";
		}

		namespace {
			// Contexte courant per-thread (non-singleton, comme NkUIContext).
			thread_local NkGuiContext *gCurrentContext = nullptr;
		} // namespace

		void SetCurrentContext(NkGuiContext *ctx) noexcept {
			gCurrentContext = ctx;
		}

		NkGuiContext *GetCurrentContext() noexcept {
			return gCurrentContext;
		}

		namespace {
			/// (10/10) LES MUTATIONS DU FOCUS DE PANNEAU : NK_FOCUS_MUTATION=<regle> retire
			/// UNE regle, pour prouver que le temoin sait rougir. RELUE A CHAQUE APPEL (pas
			/// de `static`) : le temoin arme et desarme la mutation dans le MEME processus,
			/// entre deux scenes -- le positif et le negatif sortent de la meme execution.
			/// Elle n'est lue que sur un clic, ou quand un texte est en frappe.
			bool FocusMutation(const char *regle) noexcept {
				const char *v = getenv("NK_FOCUS_MUTATION");
				if (!v || !regle)
					return false;
				while (*v && *v == *regle) {
					++v;
					++regle;
				}
				return *v == 0 && *regle == 0;
			}
		} // namespace

		bool NkGuiContext::Init(int32 width, int32 height) noexcept {
			viewW = width;
			viewH = height;
			// ⚠️ L'INTROSPECTION S'ALLUME SANS RECOMPILER, et elle reste MUETTE
			//    tant que personne ne le demande. Même modèle que NK_MENU_TRACE
			//    sur le chantier NKCraft : un instrument qui parle tout le
			//    temps finit désactivé, donc débranché le jour où il servirait.
			//    ⚠️ « 0 » ÉTEINT EXPLICITEMENT. Sans ce cas, `NK_GUI_INTROSPECT=0`
			//       — la façon dont tout le monde écrit « non » — allumerait
			//       l'instrument, parce que la variable est *définie*.
			if (const char *v = nkentseu::env::GetEnvVar("NK_GUI_INTROSPECT"))
				introspect.actif = (*v != 0 && !(v[0] == '0' && v[1] == 0));
			return true;
		}

		void NkGuiContext::Shutdown() noexcept {
			dl.Reset();
			if (GetCurrentContext() == this)
				SetCurrentContext(nullptr);
		}

		void NkGuiContext::BeginFrame(float32 dt) noexcept {
			// (10/10) LE FOCUS DE PANNEAU : une modale dessinee a l'image d'avant (le kit
			// compte `modalDepth`, l'hote declare `appModal`) -- lu AVANT leur remise a zero.
			const bool modalePrec = modalDepth > 0 || appModal;
			input.dt = dt;
			// Un rectangle pose et jamais consomme (widget conditionnel non
			// atteint) ne doit pas s'appliquer a la frame suivante.
			nextItemRectSet = false;
			input.NewFrame();	   // transitions clic/relâche
			// La zone IME est celle du champ focalise CETTE image : un champ qui
			// n'est plus dessine ne doit plus retenir la liste de candidats.
			imeZoneValid = false;


			// ── Glisser-deposer : cycle de vie (2026-08-17) ───────────────────
			// Le relachement laisse `dragActive` vrai PENDANT la frame du
			// relachement — les cibles lisent input.mouseReleased pour livrer —
			// et le nettoyage a lieu au NewFrame suivant. Un lacher hors de
			// toute cible se nettoie par le meme chemin : pas de livraison.
			if (dragEndPending) {
				dragActive = false;
				dragEndPending = false;
				dragDelivered = false;
				dragSourceId = NKGUI_ID_NONE;
				dragType[0] = '\0';
				dragPayloadSize = 0;
				dragGhost[0] = '\0';
			}
			if (dragActive && !input.mouseDown[0])
				dragEndPending = true;
			if (!input.mouseDown[0] && !dragActive)
				dragCandidateId = NKGUI_ID_NONE; // desarme un candidat jamais parti
			time += dt;			   // blink du caret
			hotIdPrev = hotId;	   // le survol résolu de la frame précédente
			hotId = NKGUI_ID_NONE; // re-calculé par les widgets (greedy)
			interact = NkGuiInteract::None;
			lastItemHovered = false;
			wantCursor = NkGuiCursor::Arrow;
			idDepth = 0;
			disabledDepth = 0;
			inputClickConsumed = false;
			curPopupLevel = -1; // le dessin reprend sur la couche principale
			// Routeur d'occlusion : la liste ecrite la frame PRECEDENTE devient la
			// liste LUE (stable toute la frame, comme hotIdPrev) ; on repart a zero
			// pour l'ecriture de cette frame.
			occlCount = occlCountNew;
			for (int32 i = 0; i < occlCountNew; ++i) {
				occlRects[i] = occlRectsNew[i];
				occlLayers[i] = occlLayersNew[i];
			}
			occlCountNew = 0;
			curInputLayer = 0;
			// ── LE FOCUS DE PANNEAU (10/10) : le clic de CETTE image, et s'il tombe sur
			//    une surcouche. Lu ICI, avec l'etat de l'image d'avant (popups ouverts,
			//    surfaces declarees) -- la meme source que `hotIdPrev` et l'occultation :
			//    le menu qui se referme sur ce clic compte encore comme menu.
			panneauProfondeur = 0;
			panneauReclame = NKGUI_ID_NONE;
			frappeCount = frappeCountNew;
			for (int32 i = 0; i < frappeCountNew; ++i)
				frappePanneaux[i] = frappePanneauxNew[i];
			frappeCountNew = 0;
			panneauClic = input.mouseClicked[0] || input.mouseClicked[1] || input.mouseClicked[2];
			panneauClicNeutre = false;
			if (panneauClic) {
				const NkVec2 p = input.mousePos;
				bool neutre = modalePrec || input.saisieReserveePrec;
				for (int32 i = 0; i < popupDepth && !neutre; ++i)
					neutre = NkGuiRectContains(popupRects[i], p);
				if (!neutre && popupDepth > 0)
					neutre = NkGuiRectContains(popupAnchor, p); // l'ancre d'un menu ouvert (la barre)
				if (!neutre) // la barre de menus : le clic qui OUVRE un menu appartient au menu
					neutre = NkGuiRectContains(menuBarRect, p);
				if (!neutre && titleBarH > 0.f) // la barre de titre de l'hote (deplacer, boutons)
					neutre = p.y >= 0.f && p.y < titleBarH;
				for (int32 i = 0; i < occlCount && !neutre; ++i)
					neutre = occlLayers[i] > 0 && NkGuiRectContains(occlRects[i], p);
				panneauClicNeutre = neutre && !FocusMutation("surcouche");
			}
			winCount = 0;		// pool de fenêtres ré-attribué cette frame
			curWindow = -1;
			curWindowId = NKGUI_ID_NONE;
			curWindowDocked = false;
			containerDepth = 0; // pile de conteneurs ré-attribuée
			overlayDepth = 0;
			for (uint32 i = 0; i < windowMeta.Size(); ++i) {
				windowMeta[i].hostRendered = false;
				windowMeta[i].frameDL = -1;
				windowMeta[i].dockDL = -2;
			} // #3 (dockDL=-2 : sentinelle « feuille non rendue »)
			dl.Reset();
			dlOverlay.Reset();
			modalDepth = 0; // la pile de modales se recompte a chaque frame
			// Le relevé d'introspection est un enregistrement DE CETTE TRAME :
			// il se vide avec les listes de dessin, jamais après coup. `Clear`
			// garde la capacité — pas de réallocation par trame.
			if (introspect.actif) {
				introspect.notes.Clear();
				introspect.perdues = 0;
			}
		}

		void NkGuiContext::EndFrame() noexcept {
			// (Q8) L'INFOBULLE, EN DERNIER : rien ne se peint plus apres elle.
			if (tooltipPose && font && font->Valid()) {
				tooltipPose = false;
				dlOverlay.PushClipRect({0.f, 0.f, 1.0e9f, 1.0e9f}, false);
				dlOverlay.AddRectFilled(tooltipRect, NkColor{24, 26, 32, 245}, theme.rounding);
				dlOverlay.AddRect(tooltipRect, theme.border, 1.f, theme.rounding);
				dlOverlay.AddText(font->Face(), font->TexId(), {tooltipRect.x + tooltipPadX, tooltipBase}, tooltipTexte,
								  theme.text);
				dlOverlay.PopClipRect();
			}
			// ── Fenêtres : fusionner leurs draw-lists dans `dl` TRIÉES par z-order
			//    (recouvrement correct) + déterminer la fenêtre survolée (frame suivante).
			if (winCount > 0) {
				int32 order[WinMax];
				for (int32 i = 0; i < winCount; ++i)
					order[i] = i;
				for (int32 i = 1; i < winCount; ++i) { // tri insertion par z croissant
					const int32 k = order[i];
					int32 j = i - 1;
					while (j >= 0 && winZ[order[j]] > winZ[k]) {
						order[j + 1] = order[j];
						--j;
					}
					order[j + 1] = k;
				}
				for (int32 i = 0; i < winCount; ++i)
					dl.Append(winDL[order[i]]);

				const float32 th = ItemHeight();
				hoveredWindowId = NKGUI_ID_NONE;
				int32 bestZ = -2147483647;
				for (int32 i = 0; i < winCount; ++i) {
					const NkGuiWindowMeta &m = windowMeta[winMeta[i]];
					const NkRect hit = m.collapsed ? NkRect{m.rect.x, m.rect.y, m.rect.w, th} : m.rect;
					if (NkGuiRectContains(hit, input.mousePos) && m.zOrder > bestZ) {
						bestZ = m.zOrder;
						hoveredWindowId = m.id;
					}
				}
			} else {
				hoveredWindowId = NKGUI_ID_NONE;
			}

			// ── LE FOCUS DE PANNEAU (10/10), RESOLU EN FIN D'IMAGE : le panneau atteint
			//    par le clic le prend ; un clic qui n'en atteint aucun, hors surcouche,
			//    le retire.
			if (panneauReclame != NKGUI_ID_NONE) {
				if (!(panneauFocus != NKGUI_ID_NONE && FocusMutation("collant")))
					panneauFocus = panneauReclame;
			} else if (panneauClic && !panneauClicNeutre && !FocusMutation("garde"))
				panneauFocus = NKGUI_ID_NONE;
			// Fermeture de la chaîne de popups : Échap ferme le niveau le plus
			// profond ; un clic hors de TOUS les popups (et hors de l'ancre) ferme tout.
			if (popupDepth > 0) {
				if (input.KeyPressed(NkGuiKey::Escape)) {
					--popupDepth;
				} else if (input.mouseClicked[0]) {
					bool inside = NkGuiRectContains(popupAnchor, input.mousePos);
					for (int32 i = 0; i < popupDepth && !inside; ++i)
						if (NkGuiRectContains(popupRects[i], input.mousePos))
							inside = true;
					if (!inside)
						popupDepth = 0;
				}
			}
			// ANTI-GEL : aucun glissement légitime ne conserve activeId bouton RELÂCHÉ.
			// Si le widget détenant activeId a disparu (hôte redevenu flottant, onglet
			// caché, fenêtre fermée…) il ne libère jamais activeId et l'occlusion bloque
			// TOUTE interaction. Souris haute + activeId encore posé ⇒ on libère d'office.
			// ⚠️ « Haute » = haute depuis une frame COMPLÈTE (mousePrev aussi) : le front
			// mouseReleased n'est calculé qu'au NewFrame SUIVANT le passage à faux. Si on
			// libère dès mouseDown=false, une entrée posée APRÈS les widgets (overlay,
			// sonde pilotée) perd activeId une frame avant le front — et le clic validé
			// au relâchement (ButtonBehavior) ne peut JAMAIS aboutir. Pour l'entrée
			// événementielle réelle, ce resserrement ne change rien : le widget encore
			// vivant libère activeId lui-même en consommant le front, et un widget
			// disparu est libéré une frame plus tard — imperceptible. (Mesuré 2026-08-17 :
			// sonde --dragdrop-test, clic de dépliage jamais validé, diag hotId=activeId.)
			if (!input.mouseDown[0] && !input.mousePrev[0] && activeId != NKGUI_ID_NONE) {
				activeId = NKGUI_ID_NONE;
				movingWindowId = NKGUI_ID_NONE;
			}
			// Fin du drag d'ancrage non abouti (relâché hors DockSpace) : on libère.
			if (input.mouseReleased[0])
				movingWindowId = NKGUI_ID_NONE;
			// Défocus si un clic a eu lieu hors de tout champ texte.
			if (input.mouseClicked[0] && !inputClickConsumed)
				inputId = NKGUI_ID_NONE;
			input.wheel = 0.f; // molette consommée
			input.wheelH = 0.f;
			input.ClearPerFrameText(); // texte/touches consommés
		}

		void NkGuiContext::PushId(const char *s) noexcept {
			const NkGuiId seed = idDepth > 0 ? idStack[idDepth - 1] : 2166136261u;
			if (idDepth < 32)
				idStack[idDepth++] = NkGuiHashStr(s, seed);
		}

		void NkGuiContext::PushId(const void *p) noexcept {
			const NkGuiId seed = idDepth > 0 ? idStack[idDepth - 1] : 2166136261u;
			if (idDepth < 32)
				idStack[idDepth++] = NkGuiHashPtr(p, seed);
		}

		void NkGuiContext::PopId() noexcept {
			if (idDepth > 0)
				--idDepth;
		}

		NkGuiId NkGuiContext::GetId(const char *s) const noexcept {
			const NkGuiId seed = idDepth > 0 ? idStack[idDepth - 1] : 2166136261u;
			return NkGuiHashStr(s, seed);
		}

		// ── Layout ────────────────────────────────────────────────────────────
		void NkGuiContext::BeginLayout(const NkRect &region) noexcept {
			layout.region = region;
			layout.cursor = {region.x + layout.padding, region.y + layout.padding};
			layout.lineStartX = layout.cursor.x;
			layout.curLineH = 0.f;
			layout.prevItem = {layout.cursor.x, layout.cursor.y, 0.f, 0.f};
			layout.maxX = layout.cursor.x;
			layout.maxY = layout.cursor.y;
			layout.flow = 0; // vertical par défaut
			layout.gridIdx = 0;
		}

		float32 NkGuiContext::ContentWidth() const noexcept {
			const float32 w = (layout.region.x + layout.region.w - layout.padding) - layout.cursor.x;
			return w > 1.f ? w : 1.f;
		}

		float32 NkGuiContext::AvailHeight() const noexcept {
			const float32 h = (layout.region.y + layout.region.h - layout.padding) - layout.cursor.y;
			return h > 1.f ? h : 1.f;
		}

		float32 NkGuiContext::ItemHeight() const noexcept {
			const float32 lh = (font && font->Valid()) ? font->LineHeight() : 16.f;
			return lh + 2.f * theme.framePadY;
		}

		void NkGuiContext::SetNextItemRect(const NkRect &r) noexcept {
			nextItemRect = r;
			nextItemRectSet = true;
		}

		NkRect NkGuiContext::NextItemRect(float32 w, float32 h) noexcept {
			// ── RECTANGLE POSE : il gagne, et il ne vaut qu'une fois ─────────
			// Consomme AVANT toute logique de flux : un rectangle pose n'est ni
			// une cellule de grille, ni une case de flex — c'est un ordre.
			if (nextItemRectSet) {
				nextItemRectSet = false;
				const NkRect rect = nextItemRect;
				layout.prevItem = rect;
				// Le curseur ne bouge pas (l'appelant place lui-meme), mais
				// l'etendue du contenu doit inclure ce rectangle : sinon un
				// panneau defilable ne verrait pas ce qu'on y a pose.
				if (rect.x + rect.w > layout.maxX)
					layout.maxX = rect.x + rect.w;
				if (rect.y + rect.h > layout.maxY)
					layout.maxY = rect.y + rect.h;
				return rect;
			}
			// ── HBox : flux horizontal (le curseur avance en X, pas de retour ligne) ──
			if (layout.flow == 1) {
				if (w <= 0.f)
					w = 120.f; // pas de « remplir » en HBox
				const NkRect rect = {layout.cursor.x, layout.cursor.y, w, h};
				layout.prevItem = rect;
				layout.cursor.x += w + layout.itemSpacingX;
				if (h > layout.curLineH)
					layout.curLineH = h;
				if (layout.cursor.x - layout.itemSpacingX > layout.maxX)
					layout.maxX = layout.cursor.x - layout.itemSpacingX;
				if (rect.y + h > layout.maxY)
					layout.maxY = rect.y + h;
				return rect;
			}
			// ── Grid : colonnes régulières, retour ligne automatique tous les gridCols ──
			if (layout.flow == 2) {
				const float32 cw = layout.gridColW > 0.f ? layout.gridColW : ContentWidth();
				if (w <= 0.f || w > cw)
					w = cw;
				const NkRect rect = {layout.cursor.x, layout.cursor.y, w, h};
				layout.prevItem = rect;
				if (h > layout.curLineH)
					layout.curLineH = h;
				if (rect.x + w > layout.maxX)
					layout.maxX = rect.x + w;
				++layout.gridIdx;
				if (layout.gridCols > 0 && layout.gridIdx >= layout.gridCols) { // fin de ligne
					layout.gridIdx = 0;
					layout.cursor.x = layout.lineStartX;
					layout.cursor.y += layout.curLineH + layout.itemSpacingY;
					layout.curLineH = 0.f;
				} else {
					layout.cursor.x += layout.gridColW + layout.itemSpacingX;
				}
				if (rect.y + h > layout.maxY)
					layout.maxY = rect.y + h;
				return rect;
			}
			// ── Row : rangée flex horizontale (largeur = cellule pré-calculée, hauteur étirée) ──
			if (layout.flow == 5) {
				const float32 cw =
					(layout.flexIdx < layout.flexCount) ? layout.flexSlots[layout.flexIdx] : (w > 0.f ? w : 60.f);
				const float32 ch = layout.flexCross > 0.f ? layout.flexCross : h;
				const NkRect rect = {layout.cursor.x, layout.cursor.y, cw, ch};
				layout.prevItem = rect;
				layout.cursor.x += cw + layout.itemSpacingX;
				if (layout.cursor.x - layout.itemSpacingX > layout.maxX)
					layout.maxX = layout.cursor.x - layout.itemSpacingX;
				if (rect.y + ch > layout.maxY)
					layout.maxY = rect.y + ch;
				++layout.flexIdx;
				return rect;
			}
			// ── Column : colonne flex verticale (hauteur = cellule pré-calculée, largeur étirée) ──
			if (layout.flow == 6) {
				const float32 ch = (layout.flexIdx < layout.flexCount) ? layout.flexSlots[layout.flexIdx]
																	   : (h > 0.f ? h : ItemHeight());
				const float32 cw = layout.flexCross > 0.f ? layout.flexCross : (w > 0.f ? w : ContentWidth());
				const NkRect rect = {layout.cursor.x, layout.cursor.y, cw, ch};
				layout.prevItem = rect;
				layout.cursor.y += ch + layout.itemSpacingY;
				if (rect.x + cw > layout.maxX)
					layout.maxX = rect.x + cw;
				if (layout.cursor.y > layout.maxY)
					layout.maxY = layout.cursor.y;
				++layout.flexIdx;
				return rect;
			}
			// ── Flow : comme HBox mais passe à la ligne quand ça déborde la région ──
			if (layout.flow == 4) {
				if (w <= 0.f)
					w = 120.f;
				const float32 rightEdge = layout.region.x + layout.region.w - layout.padding;
				if (layout.cursor.x > layout.lineStartX && layout.cursor.x + w > rightEdge) {
					layout.cursor.x = layout.lineStartX; // retour ligne
					layout.cursor.y += layout.curLineH + layout.itemSpacingY;
					layout.curLineH = 0.f;
				}
				const NkRect rect = {layout.cursor.x, layout.cursor.y, w, h};
				layout.prevItem = rect;
				layout.cursor.x += w + layout.itemSpacingX;
				if (h > layout.curLineH)
					layout.curLineH = h;
				if (rect.x + w > layout.maxX)
					layout.maxX = rect.x + w;
				if (rect.y + h > layout.maxY)
					layout.maxY = rect.y + h;
				return rect;
			}
			// ── Stack : tous les enfants superposés, ancrés dans la boîte stackW×stackH ──
			if (layout.flow == 3) {
				if (w <= 0.f)
					w = layout.stackW > 0.f ? layout.stackW : ContentWidth();
				if (h <= 0.f)
					h = layout.stackH;
				const float32 ax = static_cast<float32>(layout.stackAnchor % 3) * 0.5f; // 0, .5, 1
				const float32 ay = static_cast<float32>(layout.stackAnchor / 3) * 0.5f;
				const NkRect rect = {layout.lineStartX + ax * (layout.stackW - w),
									 layout.cursor.y + ay * (layout.stackH - h), w, h};
				layout.prevItem = rect;
				const float32 r = layout.lineStartX + layout.stackW, b = layout.cursor.y + layout.stackH;
				if (r > layout.maxX)
					layout.maxX = r;
				if (b > layout.maxY)
					layout.maxY = b;
				return rect; // curseur inchangé → superposition
			}
			// ── Vertical (défaut) : un item par ligne, le curseur descend ──
			if (w <= 0.f)
				w = ContentWidth();
			const NkRect rect = {layout.cursor.x, layout.cursor.y, w, h};
			layout.prevItem = rect;
			if (rect.x + rect.w > layout.maxX)
				layout.maxX = rect.x + rect.w; // largeur contenu
			if (h > layout.curLineH)
				layout.curLineH = h;
			layout.cursor.x = layout.lineStartX;
			layout.cursor.y += layout.curLineH + layout.itemSpacingY;
			if (layout.cursor.y > layout.maxY)
				layout.maxY = layout.cursor.y;
			layout.curLineH = 0.f;
			return rect;
		}

		void NkGuiContext::SameLine(float32 spacingX) noexcept {
			const float32 sx = spacingX >= 0.f ? spacingX : layout.itemSpacingX;
			layout.cursor.x = layout.prevItem.x + layout.prevItem.w + sx;
			layout.cursor.y = layout.prevItem.y;
			layout.curLineH = layout.prevItem.h;
		}

		void NkGuiContext::Spacing(float32 px) noexcept {
			layout.cursor.y += (px >= 0.f ? px : layout.itemSpacingY);
		}

		void NkGuiContext::Indent(float32 w) noexcept {
			layout.lineStartX += w;
			layout.cursor.x = layout.lineStartX;
		}

		bool NkGuiContext::IsNodeOpen(NkGuiId id) const noexcept {
			for (uint32 i = 0; i < openNodes.Size(); ++i)
				if (openNodes[i] == id)
					return true;
			return false;
		}

		void NkGuiContext::SetNodeOpen(NkGuiId id, bool open) noexcept {
			for (uint32 i = 0; i < openNodes.Size(); ++i) {
				if (openNodes[i] == id) {
					if (!open) {
						openNodes[i] = openNodes[openNodes.Size() - 1];
						openNodes.PopBack();
					}
					return;
				}
			}
			if (open)
				openNodes.PushBack(id);
		}

		int32 NkGuiContext::GetTabIndex(NkGuiId bar) const noexcept {
			for (uint32 i = 0; i < tabBarKeys.Size(); ++i)
				if (tabBarKeys[i] == bar)
					return tabBarSel[i];
			return -1;
		}

		void NkGuiContext::SetTabIndex(NkGuiId bar, int32 idx) noexcept {
			for (uint32 i = 0; i < tabBarKeys.Size(); ++i)
				if (tabBarKeys[i] == bar) {
					tabBarSel[i] = idx;
					return;
				}
			tabBarKeys.PushBack(bar);
			tabBarSel.PushBack(idx);
		}

		bool NkGuiContext::IsHovered(const NkRect &r) const noexcept {
			// Un widget actif ailleurs capture le pointeur : pas de survol.
			if (activeId != NKGUI_ID_NONE)
				return false;
			return NkGuiRectContains(r, input.mousePos);
		}

		namespace {
			int32 FindKey(const NkVector<NkGuiId> &keys, NkGuiId id) noexcept {
				for (uint32 i = 0; i < keys.Size(); ++i)
					if (keys[i] == id)
						return static_cast<int32>(i);
				return -1;
			}
		} // namespace

		void NkGuiContext::BeginSelectList(const char *id, bool *mask, int32 count, NkGuiSelectFlags flags) noexcept {
			curSelList = GetId(id);
			selMask = mask;
			selCount = count;
			selMulti = NkGuiHasSelectFlag(flags, NkGuiSelectFlags::MultiSelect);
			selIdx = 0;
			const int32 si = FindKey(selKeys, curSelList);
			selFocus = (si >= 0) ? selFocusStore[si] : -1;
			selAnchor = (si >= 0) ? selAnchorStore[si] : -1;

			// Navigation clavier (seulement si cette liste a le focus).
			if (activeSelList == curSelList && count > 0 && mask) {
				int32 nf = selFocus;
				if (input.KeyPressedRepeat(NkGuiKey::Down))
					nf = (nf < 0) ? 0 : (nf + 1 < count ? nf + 1 : count - 1);
				if (input.KeyPressedRepeat(NkGuiKey::Up))
					nf = (nf < 0) ? 0 : (nf - 1 >= 0 ? nf - 1 : 0);
				if (nf != selFocus) {
					selFocus = nf;
					if (selMulti && input.shiftDown) {
						const int32 a = selAnchor < 0 ? selFocus : selAnchor;
						for (int32 i = 0; i < count; ++i)
							mask[i] = false;
						const int32 lo = a < selFocus ? a : selFocus, hi = a < selFocus ? selFocus : a;
						for (int32 i = lo; i <= hi; ++i)
							mask[i] = true;
					} else if (!selMulti) {
						for (int32 i = 0; i < count; ++i)
							mask[i] = false;
						if (selFocus >= 0)
							mask[selFocus] = true;
						selAnchor = selFocus;
					}
				}
				if (input.KeyPressed(NkGuiKey::Enter) && selFocus >= 0) {
					if (selMulti)
						mask[selFocus] = !mask[selFocus];
					else {
						for (int32 i = 0; i < count; ++i)
							mask[i] = false;
						mask[selFocus] = true;
					}
					selAnchor = selFocus;
				}
			}
		}

		void NkGuiContext::ApplySelectClick(int32 idx) noexcept {
			if (!selMask || idx < 0 || idx >= selCount)
				return;
			activeSelList = curSelList;
			selFocus = idx;
			if (!selMulti) {
				for (int32 i = 0; i < selCount; ++i)
					selMask[i] = false;
				selMask[idx] = true;
				selAnchor = idx;
			} else if (input.shiftDown) {
				const int32 a = selAnchor < 0 ? idx : selAnchor;
				for (int32 i = 0; i < selCount; ++i)
					selMask[i] = false;
				const int32 lo = a < idx ? a : idx, hi = a < idx ? idx : a;
				for (int32 i = lo; i <= hi; ++i)
					selMask[i] = true;
			} else if (input.ctrlDown) {
				selMask[idx] = !selMask[idx];
				selAnchor = idx;
			} else {
				for (int32 i = 0; i < selCount; ++i)
					selMask[i] = false;
				selMask[idx] = true;
				selAnchor = idx;
			}
		}

		void NkGuiContext::EndSelectList() noexcept {
			const int32 si = FindKey(selKeys, curSelList);
			if (si >= 0) {
				selFocusStore[si] = selFocus;
				selAnchorStore[si] = selAnchor;
			} else {
				selKeys.PushBack(curSelList);
				selFocusStore.PushBack(selFocus);
				selAnchorStore.PushBack(selAnchor);
			}
			curSelList = NKGUI_ID_NONE;
			selMask = nullptr;
			selCount = 0;
		}

		void NkGuiContext::BeginDisabled(bool disabled) noexcept {
			const bool eff = disabled || IsDisabled(); // une fois désactivé, le reste l'est
			if (disabledDepth < 16)
				disabledStack[disabledDepth++] = eff;
		}

		void NkGuiContext::EndDisabled() noexcept {
			if (disabledDepth > 0)
				--disabledDepth;
		}

		bool NkGuiContext::ItemHoverable(const NkRect &r, NkGuiId id) noexcept {
			// Routeur d'occlusion UNIFIE : un widget n'est jamais survolable si une
			// surface flottante d'une couche SUPERIEURE (modal, palette, popover
			// declares via PushOcclusion) recouvre le pointeur — quel que soit
			// l'ordre de dessin des panneaux.
			if (!PointReachable(input.mousePos))
				return false;
			// Désactivé : aucune interaction.
			if (IsDisabled())
				return false;
			// Un popup ouvert capture le pointeur : un widget ne réagit pas si le
			// pointeur est au-dessus d'un popup PLUS PROFOND que le niveau courant
			// (couche principale = -1 ; un item de menu ne capture pas sous son
			// sous-menu déployé).
			for (int32 i = curPopupLevel + 1; i < popupDepth; ++i)
				if (NkGuiRectContains(popupRects[i], input.mousePos))
					return false;
			// Occlusion par fenêtre : hors popup, un widget ne réagit que si SA fenêtre
			// est celle survolée au-dessus (curWindowId). Bloque le fond ET les fenêtres
			// recouvertes. (hoveredWindowId/curWindowId = NONE pour le fond hors fenêtre.)
			if (curPopupLevel < 0 && hoveredWindowId != NKGUI_ID_NONE && hoveredWindowId != curWindowId)
				return false;
			// Hors du CLIP courant (zone défilable, panneau) : pas d'interaction —
			// un item scrollé hors-vue ne doit pas capturer le pointeur.
			if (!NkGuiRectContains(DL().CurrentClip(), input.mousePos))
				return false;
			// Bloqué si un AUTRE widget capture le pointeur.
			if (activeId != NKGUI_ID_NONE && activeId != id)
				return false;
			if (!NkGuiRectContains(r, input.mousePos))
				return false;
			// Greedy : le DERNIER widget soumis sous le pointeur écrase hotId →
			// celui dessiné par-dessus gagne. On ne déclare « survolé » QUE le
			// front-most de la frame précédente (hotIdPrev) ; le widget masqué
			// dessous, lui, met à jour hotId mais retourne false → ne capture pas.
			hotId = id;
			return hotIdPrev == id;
		}

		// ═══════════════════════════════════════════════════════════════════
		//  LE FOCUS DE PANNEAU (2026-10-10) — voir NkGuiContext.h
		// ═══════════════════════════════════════════════════════════════════
		void NkGuiContext::PanneauDebut(NkGuiId id, const NkRect &r) noexcept {
			if (panneauProfondeur < PanneauPileMax) {
				panneauPile[panneauProfondeur] = id;
				panneauChampDebut[panneauProfondeur] = inputId;
			}
			++panneauProfondeur; // au-dela de la pile : compte, pour que PanneauFin reste apparie
			if (id == NKGUI_ID_NONE)
				return;
			// LE CLIC QUI L'ATTEINT : l'entree que voit le panneau (une coquille qui le
			// masque sous une modale lui rend un clic efface et une souris « nulle part »),
			// et la regle d'ItemHoverable, sans le survol d'image d'avant (le dernier
			// panneau declare gagne en fin d'image, comme `hotId`).
			if (!(input.mouseClicked[0] || input.mouseClicked[1] || input.mouseClicked[2]))
				return;
			const NkVec2 p = input.mousePos;
			if (!NkGuiRectContains(r, p))
				return;
			if (!PointReachable(p)) // une surface d'une couche superieure le couvre
				return;
			if (!FocusMutation("popup"))
				for (int32 i = curPopupLevel + 1; i < popupDepth; ++i)
					if (NkGuiRectContains(popupRects[i], p)) // un popup plus profond le couvre
						return;
			if (curPopupLevel < 0 && hoveredWindowId != NKGUI_ID_NONE && hoveredWindowId != curWindowId)
				return; // une autre fenetre est au-dessus
			panneauReclame = id;
		}

		void NkGuiContext::PanneauFin() noexcept {
			if (panneauProfondeur <= 0)
				return;
			--panneauProfondeur;
			if (panneauProfondeur >= PanneauPileMax)
				return;
			// LE CHAMP QUI A PRIS LE FOCUS ICI : `inputId` a change pendant ce panneau. Le
			// plus INTERIEUR gagne : il se referme le premier, et l'exterieur trouve la
			// paternite deja ecrite.
			if (inputId != NKGUI_ID_NONE && inputId != panneauChampDebut[panneauProfondeur] && champPanneauId != inputId) {
				champPanneauId = inputId;
				champPanneau = panneauPile[panneauProfondeur];
			}
		}

		void NkGuiContext::NoterFrappe() noexcept {
			const NkGuiId p = PanneauCourant();
			for (int32 i = 0; i < frappeCountNew; ++i)
				if (frappePanneauxNew[i] == p)
					return;
			if (frappeCountNew < FrappeMax)
				frappePanneauxNew[frappeCountNew++] = p;
			else // trop de champs a la fois : on ne sait plus a qui -- « ailleurs » pour tous
				frappePanneauxNew[FrappeMax - 1] = NKGUI_ID_NONE;
		}

		bool NkGuiContext::TexteEnFrappeHors(NkGuiId id) const noexcept {
			bool chezLui = false, ailleurs = false;
			for (int32 i = 0; i < frappeCount; ++i)
				(frappePanneaux[i] == id ? chezLui : ailleurs) = true;
			for (int32 i = 0; i < frappeCountNew; ++i)
				(frappePanneauxNew[i] == id ? chezLui : ailleurs) = true;
			if (inputId != NKGUI_ID_NONE) // un champ de NKGui : son panneau, s'il est connu
				((champPanneauId == inputId && champPanneau == id) ? chezLui : ailleurs) = true;
			if (ailleurs)
				return true;
			// contre-epreuve : la paternite ignoree, un texte en frappe chez lui compte comme ailleurs
			return chezLui && FocusMutation("proprietaire");
		}

		bool NkGuiContext::PanneauAuClavier(NkGuiId id) const noexcept {
			if (id == NKGUI_ID_NONE || panneauFocus != id)
				return false;
			if (popupDepth > 0) // un menu ouvert a le clavier (Echap, fleches, Entree)
				return false;
			if (input.saisieReservee || input.saisieReserveePrec) // une modale l'a reserve
				return false;
			if (TexteEnFrappeHors(id) && !FocusMutation("frappe"))
				return false;
			return true;
		}

		bool NkGuiContext::ButtonBehavior(NkGuiId id, const NkRect &r, NkGuiButtonFlags flags,
										  float32 repeatDelayOverride, float32 repeatRateOverride, bool *outHovered,
										  bool *outHeld) noexcept {
			const bool hovered = ItemHoverable(r, id); // respecte le z-ordre
			const bool repeat = NkGuiHasFlag(flags, NkGuiButtonFlags::Repeat);

			bool pressed = false;
			if (hovered && input.mouseClicked[0]) {
				activeId = id;
				if (repeat)
					pressed = true; // rafale : déclenche dès l'appui
			}

			const bool held = (activeId == id);
			if (held) {
				interact = NkGuiInteract::EditWidget;
				// Rafale : déclenchements typematic tant que maintenu DANS le rect.
				if (repeat && input.mouseDown[0] && NkGuiRectContains(r, input.mousePos)) {
					const float32 rd = repeatDelayOverride >= 0.f ? repeatDelayOverride : repeatDelay;
					const float32 rr = repeatRateOverride >= 0.f ? repeatRateOverride : repeatRate;
					const float32 t1 = input.mouseDownDur[0];
					if (NkGuiCalcTypematic(t1 - input.dt, t1, rd, rr) > 0)
						pressed = true;
				}
				if (input.mouseReleased[0]) {
					// Sans Repeat : clic validé au relâchement DANS le rect.
					if (!repeat && NkGuiRectContains(r, input.mousePos))
						pressed = true;
					activeId = NKGUI_ID_NONE;
				}
			} else if (hovered) {
				interact = NkGuiInteract::HoverWidget;
			}

			if (outHovered)
				*outHovered = hovered;
			if (outHeld)
				*outHeld = held;
			lastItemHovered = hovered; // pour IsItemHovered() / SetTooltip
			lastItemId = id;		   // pour le glisser-deposer (source/cible)
			lastItemRect = r;
			return pressed;
		}


		// ── TABLE DE DESCRIPTION DES JETONS DE THEME ───────────────────────────
		// Ecrite UNE fois, lue par tout ce qui doit enumerer le theme sans en
		// connaitre les champs : selecteur de theme, serialiseur, et le futur
		// NKUIEditor. Ajouter un jeton = une ligne ici + un champ dans la struct ;
		// le banc temoin verifie que les deux restent en phase.
		namespace {
#define NKGUI_TOK_C(field, grp)                                                                                        	{#field, grp, NkGuiTokenType::Color, static_cast<uint16>(offsetof(NkGuiTheme, field))}
#define NKGUI_TOK_S(field, grp)                                                                                        	{#field, grp, NkGuiTokenType::Scalar, static_cast<uint16>(offsetof(NkGuiTheme, field))}

			const NkGuiTokenDesc kTokens[] = {
				// surfaces
				NKGUI_TOK_C(bgPrimary, "surface"),
				NKGUI_TOK_C(panel, "surface"),
				NKGUI_TOK_C(header, "surface"),
				NKGUI_TOK_C(card, "surface"),
				NKGUI_TOK_C(track, "surface"),
				NKGUI_TOK_C(scrim, "surface"),
				NKGUI_TOK_C(shadow, "surface"),
				// controles
				NKGUI_TOK_C(button, "controle"),
				NKGUI_TOK_C(buttonHover, "controle"),
				NKGUI_TOK_C(buttonActive, "controle"),
				NKGUI_TOK_C(rowHover, "controle"),
				NKGUI_TOK_C(selection, "controle"),
				NKGUI_TOK_C(scrollbar, "controle"),
				NKGUI_TOK_C(scrollbarHover, "controle"),
				// onglets
				NKGUI_TOK_C(tabBar, "onglet"),
				NKGUI_TOK_C(tab, "onglet"),
				NKGUI_TOK_C(tabHover, "onglet"),
				NKGUI_TOK_C(tabActive, "onglet"),
				// traits
				NKGUI_TOK_C(border, "trait"),
				NKGUI_TOK_C(separator, "trait"),
				// texte
				NKGUI_TOK_C(text, "texte"),
				NKGUI_TOK_C(textDisabled, "texte"),
				NKGUI_TOK_C(textMuted, "texte"),
				NKGUI_TOK_C(onAccent, "texte"),
				// etats
				NKGUI_TOK_C(accent, "etat"),
				NKGUI_TOK_C(success, "etat"),
				NKGUI_TOK_C(warning, "etat"),
				NKGUI_TOK_C(danger, "etat"),
				NKGUI_TOK_C(info, "etat"),
				// geometrie
				NKGUI_TOK_S(rounding, "geometrie"),
				NKGUI_TOK_S(roundingSmall, "geometrie"),
				NKGUI_TOK_S(roundingLarge, "geometrie"),
				NKGUI_TOK_S(borderThickness, "geometrie"),
				NKGUI_TOK_S(framePadX, "geometrie"),
				NKGUI_TOK_S(framePadY, "geometrie"),
			};
#undef NKGUI_TOK_C
#undef NKGUI_TOK_S

			// Comparaison de noms sans <cstring> (zero-STL, et strcmp tirerait la
			// libc la ou la Bare n'en aura pas).
			bool NkGuiTokNameEq(const char *a, const char *b) noexcept {
				if (!a || !b)
					return false;
				while (*a && *b) {
					if (*a != *b)
						return false;
					++a;
					++b;
				}
				return *a == *b;
			}

			const NkGuiTokenDesc *NkGuiFindTok(const char *name, NkGuiTokenType type) noexcept {
				const int32 n = static_cast<int32>(sizeof(kTokens) / sizeof(kTokens[0]));
				for (int32 i = 0; i < n; ++i)
					if (kTokens[i].type == type && NkGuiTokNameEq(kTokens[i].name, name))
						return &kTokens[i];
				return nullptr;
			}
		} // namespace

		const NkGuiTokenDesc *NkGuiThemeTokens(int32 *count) noexcept {
			if (count)
				*count = static_cast<int32>(sizeof(kTokens) / sizeof(kTokens[0]));
			return kTokens;
		}

		NkColor *NkGuiThemeColor(NkGuiTheme &theme, const char *name) noexcept {
			const NkGuiTokenDesc *d = NkGuiFindTok(name, NkGuiTokenType::Color);
			if (!d)
				return nullptr;
			return reinterpret_cast<NkColor *>(reinterpret_cast<uint8 *>(&theme) + d->offset);
		}

		float32 *NkGuiThemeScalar(NkGuiTheme &theme, const char *name) noexcept {
			const NkGuiTokenDesc *d = NkGuiFindTok(name, NkGuiTokenType::Scalar);
			if (!d)
				return nullptr;
			return reinterpret_cast<float32 *>(reinterpret_cast<uint8 *>(&theme) + d->offset);
		}

	} // namespace nkgui
} // namespace nkentseu
