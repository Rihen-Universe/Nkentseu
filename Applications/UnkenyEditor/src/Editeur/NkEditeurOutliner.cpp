//
// NkEditeurOutliner.cpp
// =============================================================================
// Description :
//   L'Outliner : la scene et ses entites, en ARBRE DU KIT (NkDrawTreeView),
//   colonnes Oeil | Cadenas | Nom | Type, champ de recherche, pied
//   « N entites (1 sel.) ».
//
// Caracteristiques :
//   - Le modele d'arbre est RECONSTRUIT a chaque trame depuis la scene : il
//     n'y a donc qu'une verite (la scene), jamais une liste a synchroniser.
//     L'etat propre a l'arbre (noeuds replies, defilement) survit, parce que
//     l'identifiant d'un noeud est celui de l'entite.
//   - La selection est celle du MODELE : cliquer dans le viseur la change ici,
//     cliquer ici la change dans le viseur.
//   - Double-clic sur une entite : la vue se centre dessus (comme UE5).
//   - (2026-09-29) C'est un ARBRE : chaque entite sous son parent (la
//     hierarchie de la scene). Glisser une ligne sur une autre l'y RATTACHE,
//     sans qu'elle bouge a l'ecran ; la lacher dans le vide la detache. Clic
//     droit : le menu de l'entite (dont « Creer un prefab ») ; hors d'une ligne
//     (la racine, le vide), celui de la scene (2026-09-30, lot 1).
//   - L'OEIL et le CADENAS (2026-09-30) : NkDrapeauxEditeur, sauves dans la
//     scene. Oeil ferme = ni dessinee ni prise dans la vue en EDITION ;
//     cadenas = ni prise ni deplacee dans la vue. L'Outliner et les Details
//     choisissent et modifient TOUJOURS l'entite (sinon on ne pourrait plus
//     rouvrir l'oeil d'un objet cache, ni lire les proprietes d'un decor fige).
//   - Le RENOMMAGE EN PLACE (2026-09-30) : F2, « Renommer » des menus, ou le
//     clic LENT sur le nom d'une ligne deja choisie. Entree valide, Echap
//     annule, un clic ailleurs valide (contrat du kit).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurInterface.h"

#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKEditorKit/Components/NkGuiComponentPaint.h"
#include "NKEditorKit/NkEditorScrollbar.h"
#include "NKEditorKit/NkEditorTextField.h"
#include "NKEditorKit/NkThemeToGui.h"

#include <cmath>
#include <cstdio>

namespace nkentseu {
	namespace editeur {

		using editorkit::NkRole;
		using nkgui::NkColor;
		using nkgui::NkRect;

		namespace {

			/// L'identifiant de noeud de la racine. 0 est reserve par le contrat
			/// du composant (« aucun »), 1 est la scene, les entites commencent a 2.
			constexpr nk_uint64 ID_RACINE = 1u;

			/// Le clic LENT : au-dela d'un double-clic (0,40 s chez NKGui), en deca
			/// de l'impatience.
			constexpr float32 CLIC_LENT_DELAI = 0.5f;

			// ── LES POIGNEES D'ICONE DE L'OUTLINER ─────────────────────────────
			// Le kit ne dessine aucune icone (NkGuiComponentPaint::Icon peint un
			// carre) : NkPeintreOutliner, plus bas, les TRACE.
			constexpr uint16 ICONE_OEIL = 0x10u;
			constexpr uint16 ICONE_CADENAS = 0x11u;
			/// La NATURE d'une entite ET ses drapeaux, dans la poignee de son icone :
			/// 0x100 | nature << 4 | cacheHerite << 3 | verrouHerite << 2 | cache << 1 | verrou.
			constexpr uint16 ICONE_NATURE = 0x100u;
			enum : uint16 { NATURE_ENTITE = 0, NATURE_RIGIDE, NATURE_DECOR, NATURE_MOU, NATURE_LUMIERE, NATURE_EMETTEUR };

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

			uint16 NatureDe(NkScene &scene, ecs::NkEntityId id) {
				if (scene.Monde().Has<NkCorpsMou2D>(id)) {
					return NATURE_MOU;
				}
				if (const NkCorps2D *c = scene.Monde().Get<NkCorps2D>(id)) {
					return c->type == NkTypeCorps::NK_DYNAMIQUE ? NATURE_RIGIDE : NATURE_DECOR;
				}
				// Une lumiere, un emetteur : l'icone qu'ils ont dans le viseur.
				if (scene.Monde().Has<NkLumiere2D>(id)) {
					return NATURE_LUMIERE;
				}
				if (scene.Monde().Has<NkEmetteur2D>(id)) {
					return NATURE_EMETTEUR;
				}
				return NATURE_ENTITE;
			}

			/// Peint les icones de l'Outliner : oeil, cadenas, nature.
			///
			/// ⚠️ L'OEIL ET LE CADENAS SONT PEINTS A L'APPEL DE L'ICONE DE NATURE,
			///    pas au leur. Le kit choisit la poignee du cadenas par
			///    `NkTreeNode::locked` -- mais `locked`, pour lui, rend aussi la
			///    ligne INSELECTIONNABLE (NkTreeViewDraw.cpp), alors que le cadenas
			///    de l'editeur ne fige que la VUE. On ne pose donc jamais `locked` :
			///    le kit appelle, dans chaque ligne, l'oeil, le cadenas puis la
			///    nature ; le peintre retient les deux rectangles, et la poignee de
			///    nature, qui porte les deux drapeaux, les peint dans le bon etat.
			class NkPeintreOutliner : public editorkit::NkGuiComponentPaint {
				public:
					NkPeintreOutliner(nkgui::NkGuiContext &ctx, const editorkit::NkTheme &theme, const char *tamponRenommage) noexcept
						: NkGuiComponentPaint(ctx, theme), mCtxO(ctx), mTampon(tamponRenommage) {}

					void Icon(const editorkit::NkPaintRect &r, uint16 poignee, uint16 role) override {
						if (poignee == ICONE_OEIL) {
							mOeil = r;
							mAOeil = true;
							return;
						}
						if (poignee == ICONE_CADENAS) {
							mCadenas = r;
							mACadenas = true;
							return;
						}
						if (poignee >= ICONE_NATURE) {
							if (mAOeil) {
								Oeil(mOeil, (poignee & 2u) != 0u, (poignee & 8u) != 0u);
							}
							if (mACadenas) {
								Cadenas(mCadenas, (poignee & 1u) != 0u, (poignee & 4u) != 0u);
							}
							Nature(r, static_cast<uint16>((poignee >> 4) & 0xFu), role);
							// Le NOM commence apres cette icone : le clic lent le demande.
							const nkgui::NkVec2 s = mCtxO.input.mousePos;
							if (s.y >= r.y && s.y < r.y + r.h) {
								mLibelleX = r.x + r.w;
							}
						}
						// La racine (poignee 0) n'a ni oeil ni cadenas.
						mAOeil = false;
						mACadenas = false;
					}

					/// Le kit dessine le texte en cours de renommage avec LE tampon du
					/// modele : son rectangle est celui ou poser le champ de saisie.
					void Text(const editorkit::NkPaintRect &r, const char *s, uint16 role, editorkit::NkTextAlign align) override {
						if (s != nullptr && s == mTampon) {
							mSaisie = r;
							mASaisie = true;
						}
						NkGuiComponentPaint::Text(r, s, role, align);
					}

					float32 LibelleX() const noexcept {
						return mLibelleX;
					}
					bool Saisie(NkRect &r) const noexcept {
						r = NkRect{mSaisie.x, mSaisie.y, mSaisie.w, mSaisie.h};
						return mASaisie;
					}

				private:
					NkColor Couleur(NkRole role, uint8 alpha = 255) const {
						NkColor c = editorkit::NkThemeUnpack(ColorOf(static_cast<uint16>(role)));
						c.a = static_cast<uint8>((static_cast<uint32>(c.a) * alpha) / 255u);
						return c;
					}

					/// Ouvert : discret. Ferme : une paupiere et trois cils, plus VIFS --
					/// une entite cachee doit se voir dans la liste. Ferme par un
					/// PARENT (`herite`) : la paupiere, ATTENUEE -- on voit qu'elle est
					/// cachee, et que ce n'est pas ici que ca se rouvre (lecon de
					/// NKCraft, NkTreeViewModel.h).
					void Oeil(const editorkit::NkPaintRect &r, bool cache, bool herite) {
						nkgui::NkGuiDrawList &dl = mCtxO.DL();
						const float32 cx = r.x + r.w * 0.5f;
						const float32 cy = r.y + r.h * 0.5f;
						const float32 a = 6.f;
						const float32 b = 3.4f;
						constexpr float32 PI = 3.14159265f;
						if (!cache && !herite) {
							const NkColor col = Couleur(NkRole::TextMuted, 200);
							nkgui::NkVec2 pts[18];
							for (int32 k = 0; k < 9; ++k) {
								const float32 t = static_cast<float32>(k) / 8.f;
								pts[k] = nkgui::NkVec2{cx - a + 2.f * a * t, cy - b * std::sin(PI * t)};
								pts[9 + k] = nkgui::NkVec2{cx + a - 2.f * a * t, cy + b * std::sin(PI * t)};
							}
							dl.AddPolyline(pts, 18, col, 1.2f, true);
							dl.AddCircleFilled(nkgui::NkVec2{cx, cy}, 1.8f, col);
							return;
						}
						const NkColor col = cache ? Couleur(NkRole::Text) : Couleur(NkRole::TextMuted, 150);
						nkgui::NkVec2 paupiere[9];
						for (int32 k = 0; k < 9; ++k) {
							const float32 t = static_cast<float32>(k) / 8.f;
							paupiere[k] = nkgui::NkVec2{cx - a + 2.f * a * t, cy - 1.f + b * 0.7f * std::sin(PI * t)};
						}
						dl.AddPolyline(paupiere, 9, col, 1.4f, false);
						for (int32 k = 1; k <= 3; ++k) {
							const float32 t = static_cast<float32>(k) / 4.f;
							const float32 x = cx - a + 2.f * a * t;
							const float32 y = cy - 1.f + b * 0.7f * std::sin(PI * t);
							dl.AddLine(nkgui::NkVec2{x, y}, nkgui::NkVec2{x + (t - 0.5f) * 2.f, y + 2.6f}, col, 1.2f);
						}
					}

					/// Ouvert : l'anse levee, discret. Ferme : le corps plein, vif ;
					/// ferme par un parent (`herite`) : plein, mais ATTENUE.
					void Cadenas(const editorkit::NkPaintRect &r, bool propre, bool herite) {
						nkgui::NkGuiDrawList &dl = mCtxO.DL();
						const float32 cx = r.x + r.w * 0.5f;
						const float32 cy = r.y + r.h * 0.5f;
						const bool verrou = propre || herite;
						const NkColor col = propre ? Couleur(NkRole::Text) : Couleur(NkRole::TextMuted, 150);
						const NkRect corps{cx - 4.f, cy - 0.5f, 8.f, 6.f};
						const float32 leve = verrou ? 0.f : 2.5f;
						const float32 ra = 2.6f;
						const float32 haut = cy - 0.5f - leve;
						nkgui::NkVec2 anse[9];
						constexpr float32 PI = 3.14159265f;
						for (int32 k = 0; k < 9; ++k) {
							const float32 ang = PI * static_cast<float32>(k) / 8.f;
							anse[k] = nkgui::NkVec2{cx - ra * std::cos(ang), haut - ra * std::sin(ang) - 0.5f};
						}
						dl.AddPolyline(anse, 9, col, 1.3f, false);
						// Les deux jambes de l'anse ; ouverte, la droite reste en l'air.
						dl.AddLine(nkgui::NkVec2{cx - ra, haut - 0.5f}, nkgui::NkVec2{cx - ra, cy - 0.5f}, col, 1.3f);
						if (verrou) {
							dl.AddLine(nkgui::NkVec2{cx + ra, haut - 0.5f}, nkgui::NkVec2{cx + ra, cy - 0.5f}, col, 1.3f);
							dl.AddRectFilled(corps, col, 1.5f);
						} else {
							dl.AddRect(corps, col, 1.f, 1.5f);
						}
					}

					/// La nature de l'entite, en une forme : bulle (matiere), carre
					/// (rigide), bande (decor), losange (entite nue).
					void Nature(const editorkit::NkPaintRect &r, uint16 nature, uint16 role) {
						nkgui::NkGuiDrawList &dl = mCtxO.DL();
						const float32 cx = r.x + r.w * 0.5f;
						const float32 cy = r.y + r.h * 0.5f;
						const NkColor col = editorkit::NkThemeUnpack(ColorOf(role));
						switch (nature) {
							case NATURE_MOU:
								dl.AddCircleFilled(nkgui::NkVec2{cx - 1.f, cy + 1.f}, 4.f, col);
								dl.AddCircleFilled(nkgui::NkVec2{cx + 2.4f, cy - 1.8f}, 2.4f, col);
								break;
							case NATURE_RIGIDE:
								dl.AddRectFilled(NkRect{cx - 4.f, cy - 4.f, 8.f, 8.f}, col, 1.5f);
								break;
							case NATURE_LUMIERE:
								for (int32 k = 0; k < 8; ++k) {
									const float32 a = 0.785398f * static_cast<float32>(k);
									dl.AddLine(nkgui::NkVec2{cx + std::cos(a) * 3.5f, cy + std::sin(a) * 3.5f},
											   nkgui::NkVec2{cx + std::cos(a) * 6.f, cy + std::sin(a) * 6.f}, col, 1.1f);
								}
								dl.AddCircleFilled(nkgui::NkVec2{cx, cy}, 2.6f, col);
								break;
							case NATURE_EMETTEUR: {
								const nkgui::NkVec2 f[4] = {nkgui::NkVec2{cx, cy - 6.f}, nkgui::NkVec2{cx + 4.f, cy + 1.f}, nkgui::NkVec2{cx, cy + 5.f},
															nkgui::NkVec2{cx - 4.f, cy + 1.f}};
								dl.AddTriangleFilled(f[0], f[1], f[2], col);
								dl.AddTriangleFilled(f[0], f[2], f[3], col);
								break;
							}
							case NATURE_DECOR:
								dl.AddRectFilled(NkRect{cx - 5.5f, cy + 0.5f, 11.f, 3.5f}, col, 1.f);
								dl.AddLine(nkgui::NkVec2{cx - 4.f, cy - 2.5f}, nkgui::NkVec2{cx + 4.f, cy - 2.5f}, col, 1.f);
								break;
							default: {
								const nkgui::NkVec2 pts[4] = {nkgui::NkVec2{cx, cy - 4.5f}, nkgui::NkVec2{cx + 4.5f, cy}, nkgui::NkVec2{cx, cy + 4.5f},
															  nkgui::NkVec2{cx - 4.5f, cy}};
								dl.AddPolyline(pts, 4, col, 1.3f, true);
								break;
							}
						}
					}

					nkgui::NkGuiContext &mCtxO;
					const char *mTampon;
					editorkit::NkPaintRect mOeil{};
					editorkit::NkPaintRect mCadenas{};
					editorkit::NkPaintRect mSaisie{};
					bool mAOeil = false;
					bool mACadenas = false;
					bool mASaisie = false;
					float32 mLibelleX = 1.0e9f;
			};

			NkEditeurCadre &Cadre(void *user) {
				return *static_cast<NkEditeurCadre *>(user);
			}

			/// L'entite de la ligne `index`, ou Invalid (la racine, hors bornes).
			ecs::NkEntityId EntiteDeLigne(NkEditeurCadre &c, int32 index) {
				if (index <= 0 || index >= static_cast<int32>(c.ui.arbreEntites.Size())) {
					return ecs::NkEntityId::Invalid();
				}
				const ecs::NkEntityId e = c.ui.arbreEntites[static_cast<uint32>(index)];
				return c.m.scene.Monde().IsAlive(e) ? e : ecs::NkEntityId::Invalid();
			}

			/// L'entite d'un noeud (0 et la racine : aucune).
			ecs::NkEntityId EntiteDuNoeud(const NkEditeurInterface &ui, nk_uint64 id) {
				for (uint32 i = 1; i < ui.arbre.nodes.Size() && i < ui.arbreEntites.Size(); ++i) {
					if (ui.arbre.nodes[i].id == id) {
						return ui.arbreEntites[i];
					}
				}
				return ecs::NkEntityId::Invalid();
			}

			/// Double-clic : la vue se centre sur l'entite.
			void SurActivation(void *user, int32 index, const char *id) {
				(void)id;
				NkEditeurCadre &c = Cadre(user);
				const ecs::NkEntityId e = EntiteDeLigne(c, index);
				if (!e.IsValid()) {
					return;
				}
				c.m.selection = e;
				c.m.aSelection = true;
				// Le MEME cadrage que F : la vue va sur l'entite, meme hors du cadre.
				NkEditeurDemanderCadrage(c, false);
			}

			/// L'oeil ou le cadenas d'une ligne. La valeur du kit (tiree de
			/// `hidden` / `locked`, que l'on ne pose pas) est ignoree : on BASCULE
			/// l'etat de la scene.
			void SurDrapeau(void *user, int32 index, const char *id, uint8 drapeau, bool valeur) {
				(void)id;
				(void)valeur;
				NkEditeurCadre &c = Cadre(user);
				const ecs::NkEntityId e = EntiteDeLigne(c, index);
				if (!e.IsValid()) {
					return;
				}
				// ⚠️ UN ETAT HERITE NE SE BASCULE PAS SUR L'ENFANT : poser son propre
				//    drapeau ne rouvrirait rien (le parent le tient toujours), et le
				//    clic paraitrait sans effet. On le DIT.
				if (drapeau == static_cast<uint8>(editorkit::NkTreeFlag::Visible)) {
					if (!NkEditeurEstCache(c.m, e) && NkEditeurCacheHerite(c.m, e)) {
						NkEditeurAnnoncer(c.m, "Cachee par un parent : c'est son oeil qu'il faut rouvrir");
						return;
					}
					NkEditeurCacher(c.m, e, !NkEditeurEstCache(c.m, e));
				} else {
					if (!NkEditeurEstVerrouille(c.m, e) && NkEditeurVerrouHerite(c.m, e)) {
						NkEditeurAnnoncer(c.m, "Verrouillee par un parent : c'est son cadenas qu'il faut ouvrir");
						return;
					}
					NkEditeurVerrouiller(c.m, e, !NkEditeurEstVerrouille(c.m, e));
				}
			}

			/// Le kit rend le nom valide (et different) : il va dans l'etiquette.
			void SurRenommage(void *user, int32 index, const char *id, const char *ancien, const char *nouveau) {
				(void)id;
				(void)ancien;
				NkEditeurCadre &c = Cadre(user);
				const ecs::NkEntityId e = EntiteDeLigne(c, index);
				// Un nom VIDE n'est pas un nom : la ligne afficherait « Entite 12 ».
				if (e.IsValid() && nouveau != nullptr && nouveau[0] != '\0') {
					NkEditeurRenommer(c.m, e, nouveau);
				}
			}

			/// Clic droit sur une ligne : elle est choisie, et son menu s'ouvre --
			/// le meme que dans la vue (Renommer, Dupliquer, Supprimer...).
			/// ⚠️ (2026-09-30, lot 1) HORS D'UNE LIGNE D'ENTITE -- la racine « Scene »,
			///    le vide sous les lignes --, la fonction RENDAIT SANS RIEN FAIRE : c'est
			///    justement la qu'on clique pour creer (UE5), et le clic « ne faisait
			///    rien ». Le menu de la scene s'y ouvre (NK_CTX_ARBRE).
			void SurMenu(void *user, int32 index, float32 x, float32 y) {
				NkEditeurCadre &c = Cadre(user);
				const ecs::NkEntityId e = EntiteDeLigne(c, index);
				if (!e.IsValid()) {
					NkEditeurOuvrirMenu(c, NkMenuEditeur::NK_CTX_ARBRE, NkRect{x, y, 0.f, 0.f});
					return;
				}
				c.m.selection = e;
				c.m.aSelection = true;
				NkEditeurOuvrirMenu(c, NkMenuEditeur::NK_CTX_ENTITE, NkRect{x, y, 0.f, 0.f});
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
				ui.arbreReglages.SetParam("show_visibility", 1.f);
				ui.arbreReglages.SetParam("show_lock", 1.f);
				ui.arbreReglages.SetParam("show_type", 1.f);
				ui.arbreReglages.SetParam("indent_guides", 0.f);
				ui.arbreReglages.SetParam("multi_select", 0.f);
				ui.arbreReglages.SetParam("range_select", 0.f);
				// Le double-clic CADRE (UE5) ; le renommage est F2, le menu, ou le
				// clic lent -- ouverts par l'Outliner, pas par le kit.
				ui.arbreReglages.SetParam("activate_on_double_click", 1.f);
				// Les freres n'ont pas d'ordre a la main (celui de l'Outliner est
				// l'ordre d'arrivee) : proposer « avant / apres » promettrait un geste
				// qui n'existe pas. Deposer, c'est RATTACHER.
				ui.arbreReglages.SetParam("drop_into_only", 1.f);
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

			/// Pose le noeud de `ids[i]` sous `parentNoeud`, puis ses enfants.
			void Placer(NkEditeurCadre &c, const NkVector<ecs::NkEntityId> &ids, const NkVector<ecs::NkEntityId> &parents,
						uint32 i, int32 parentNoeud, uint32 profondeur) {
				NkEditeurInterface &ui = c.ui;
				editorkit::NkTreeNode n;
				n.id = IdNoeud(ids[i]);
				n.parent = parentNoeud;
				n.label = NomDe(c.m.scene, ids[i]);
				// ⚠️ Chaine STATIQUE (NkEditeurTypeDe rend un litteral) : le
				//    noeud ne garde qu'un pointeur, il ne copie pas.
				n.kindLabel = NkEditeurTypeDe(c.m.scene, ids[i]);
				const uint16 nature = NatureDe(c.m.scene, ids[i]);
				n.kindRole = static_cast<uint16>(nature == NATURE_MOU	   ? NkRole::AxisZ
												 : nature == NATURE_LUMIERE || nature == NATURE_EMETTEUR ? NkRole::AccentSel
																										 : NkRole::TextMuted);
				const bool cache = NkEditeurEstCache(c.m, ids[i]);
				const bool verrou = NkEditeurEstVerrouille(c.m, ids[i]);
				const bool cacheHerite = NkEditeurCacheHerite(c.m, ids[i]);
				const bool verrouHerite = NkEditeurVerrouHerite(c.m, ids[i]);
				n.icon = static_cast<uint16>(ICONE_NATURE | (nature << 4) | (cacheHerite ? 8u : 0u) | (verrouHerite ? 4u : 0u) |
											 (cache ? 2u : 0u) | (verrou ? 1u : 0u));
				// `hidden` ne change que l'icone chez le kit : on le pose, pour qui
				// lirait le modele. `locked` JAMAIS (voir NkPeintreOutliner).
				n.hidden = cache;
				n.userTag = i + 1u;
				const int32 moi = static_cast<int32>(ui.arbre.nodes.Size());
				ui.arbre.nodes.PushBack(n);
				ui.arbreEntites.PushBack(ids[i]);
				// Le composant borne sa profondeur (kMaxDepth) : au-dela, les enfants
				// seraient mal ranges — ils restent dans la scene, pas dans l'arbre.
				if (profondeur + 2u >= static_cast<uint32>(editorkit::NkTreeViewModel::kMaxDepth)) {
					return;
				}
				for (uint32 k = 0; k < ids.Size(); ++k) {
					if (parents[k] == ids[i]) {
						Placer(c, ids, parents, k, moi, profondeur + 1u);
					}
				}
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

				// (2026-09-29) L'ARBRE. Le composant veut ses noeuds en ordre PREFIXE
				// (un parent, puis toute sa descendance) : on place chaque racine, puis
				// ses enfants dans l'ordre stable de l'Outliner, recursivement.
				NkVector<ecs::NkEntityId> parents;
				parents.Resize(ids.Size());
				for (uint32 i = 0; i < ids.Size(); ++i) {
					const ecs::NkEntityId p = c.m.scene.Parent(ids[i]);
					parents[i] = Contient(ids, p) ? p : ecs::NkEntityId::Invalid();
				}
				for (uint32 i = 0; i < ids.Size(); ++i) {
					if (!parents[i].IsValid()) {
						Placer(c, ids, parents, i, 0, 0u);
					}
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

			/// Ouvre la saisie en place sur le noeud `noeud`, tout le nom choisi :
			/// la premiere touche le remplace, comme partout.
			void OuvrirRenommage(NkEditeurCadre &c, nk_uint64 noeud) {
				NkEditeurInterface &ui = c.ui;
				const int32 k = ui.arbre.IndexOf(noeud);
				if (k <= 0) {
					return;
				}
				ui.arbre.renaming = noeud;
				std::snprintf(ui.arbre.renameBuf, sizeof(ui.arbre.renameBuf), "%s", ui.arbre.nodes[static_cast<uint32>(k)].label.CStr());
				ui.arbre.renameCommit = false;
				ui.arbre.renameCancel = false;
				// Aucun clic n'ouvre cette saisie A CETTE TRAME (F2, le menu d'une
				// trame passee, le clic lent d'il y a une demi-seconde) : le kit n'a
				// rien a manger.
				ui.arbre.renameEatClick = false;
				c.ctx.input.wantSelectAll = true;
				ui.nomFocus = false;
				ui.filtreFocus = false;
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

			// « Renommer » (F2, menus) : la saisie s'ouvre AVANT le dessin, pour que
			// le kit la peigne des cette trame.
			if (ui.renommerEnPlace) {
				ui.renommerEnPlace = false;
				if (c.m.aSelection && c.m.scene.Monde().IsAlive(c.m.selection)) {
					OuvrirRenommage(c, IdNoeud(c.m.selection));
				}
			}
			// La ligne choisie AVANT ce clic : le clic lent ne vise qu'elle.
			const nk_uint64 activeAvant = ui.arbre.active;

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
			// Les deux etats de chaque colonne ont la MEME poignee : c'est l'icone
			// de nature qui porte l'etat (voir NkPeintreOutliner).
			s.icons.eyeOpen = ICONE_OEIL;
			s.icons.eyeClosed = ICONE_OEIL;
			s.icons.lockOpen = ICONE_CADENAS;
			s.icons.lockClosed = ICONE_CADENAS;
			editorkit::NkTreeViewHooks hooks;
			hooks.user = &c;
			hooks.onActivate = &SurActivation;
			hooks.onToggleFlag = &SurDrapeau;
			hooks.onRename = &SurRenommage;
			hooks.onContextMenu = &SurMenu;

			editorkit::NkComponentInput ci = NkEditeurEntreeComposant(c.ctx);
			// ── Le glisser d'une ligne (2026-09-29) ──────────────────────────────
			// Le composant sait QUELLE ligne est saisie (dragSource) ; c'est l'hote
			// qui dit qu'un glisser est en cours. Il ne commence qu'au-dela de 4 px
			// et seulement si l'appui etait DANS l'arbre : sinon un clic de selection
			// serait un depot, et un glisser parti du viseur reparenterait.
			{
				const nkgui::NkGuiInput &in = c.ctx.input;
				if (in.mouseClicked[0]) {
					ui.appuiArbre = NkEditeurDans(arbreR, in.mousePos);
					ui.departGlisseArbre = in.mousePos;
					ui.glisseArbre = false;
				}
				if (in.mouseDown[0] && ui.appuiArbre && !ui.glisseArbre && ui.arbre.dragSource != 0) {
					const float32 dx = in.mousePos.x - ui.departGlisseArbre.x;
					const float32 dy = in.mousePos.y - ui.departGlisseArbre.y;
					ui.glisseArbre = dx * dx + dy * dy > 16.f;
				}
				if (ui.glisseArbre) {
					ci.dragType = "unkeny.entite";
					ci.dragReleased = in.mouseReleased[0];
				}
			}
			NkPeintreOutliner peintre(c.ctx, c.theme, ui.arbre.renameBuf);
			const editorkit::NkTreeViewResult res = editorkit::NkDrawTreeView(
				peintre, ci, editorkit::NkPaintRect{arbreR.x, arbreR.y, arbreR.w, arbreR.h}, ui.arbre, s, hooks);

			// Le depot : APRES le dessin, jamais pendant (NkTreeViewResult).
			if (res.dropAccepted && ui.glisseArbre) {
				const ecs::NkEntityId source = EntiteDuNoeud(ui, res.dropSource);
				const ecs::NkEntityId cible = EntiteDuNoeud(ui, res.dropTarget);
				if (c.m.scene.Monde().IsAlive(source)) {
					if (c.m.scene.Monde().IsAlive(cible)) {
						NkEditeurRattacher(c.m, source, cible);
					} else {
						NkEditeurDetacher(c.m, source); // la racine « Scene », ou le vide
					}
				}
			}
			if (res.dropRefusedCycle && ui.glisseArbre) {
				NkEditeurAnnoncer(c.m, "Rattachement refuse : une entite ne descend pas d'elle-meme");
			}
			if (c.ctx.input.mouseReleased[0]) {
				ui.glisseArbre = false;
				ui.appuiArbre = false;
				ui.arbre.dragSource = 0;
			}

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

			// ── Le renommage en place : la SAISIE, par-dessus la boite du kit ──
			// Le kit dessine la boite et ne sait pas taper (NkTreeViewModel.h,
			// « contournement assume ») : le champ du kit -- caret, selection,
			// copier-coller -- se pose sur la meme ligne et ecrit dans le meme
			// tampon. Entree valide, Echap annule ; un clic ailleurs valide (le kit).
			const nkgui::NkGuiInput &in = c.ctx.input;
			if (ui.arbre.renaming != 0) {
				NkRect saisie;
				if (peintre.Saisie(saisie)) {
					editorkit::NkOverlayFieldStyle st;
					st.texte = c.pal.texte;
					const NkRect champ{saisie.x - 5.f, saisie.y + 2.f, saisie.w + 10.f, saisie.h - 4.f};
					// 32 : la taille de NkEtiquette::nom. Taper au-dela serait perdu
					// en silence au moment de valider.
					editorkit::NkOverlayTextField(c.ctx, dl, c.police, champ, ui.arbre.renameBuf, 32, true, &st);
				}
				if (in.KeyPressed(nkgui::NkGuiKey::Enter)) {
					ui.arbre.renameCommit = true;
				} else if (in.KeyPressed(nkgui::NkGuiKey::Escape)) {
					ui.arbre.renameCancel = true;
				}
			}

			// ── Le clic LENT : un clic sur le NOM d'une ligne deja choisie ─────
			// Il part apres CLIC_LENT_DELAI si rien ne l'annule : un second clic
			// (c'est un double-clic : il cadre), un glisser, une autre selection.
			const NkVec2f souris(in.mousePos.x, in.mousePos.y);
			if (in.mouseDoubleClicked[0]) {
				ui.clicLentNoeud = 0;
			} else if (in.mouseClicked[0]) {
				const bool surNom = res.survoleIndex > 0 && ui.arbre.renaming == 0 && activeAvant != 0 &&
									ui.arbre.nodes[static_cast<uint32>(res.survoleIndex)].id == activeAvant &&
									souris.x >= peintre.LibelleX();
				ui.clicLentNoeud = surNom ? activeAvant : 0;
				ui.clicLentAge = 0.f;
				ui.clicLentPos = souris;
			}
			if (ui.clicLentNoeud != 0) {
				ui.clicLentAge += ui.dt;
				const float32 dx = souris.x - ui.clicLentPos.x;
				const float32 dy = souris.y - ui.clicLentPos.y;
				if (dx * dx + dy * dy > 16.f || ui.arbre.active != ui.clicLentNoeud || ui.arbre.renaming != 0) {
					ui.clicLentNoeud = 0;
				} else if (ui.clicLentAge >= CLIC_LENT_DELAI && !in.mouseDown[0]) {
					// Ouvert a la trame SUIVANTE, AVANT le dessin, comme F2.
					// ⚠️ Pas ici : `wantSelectAll` est efface en fin de trame
					//    (NkGuiContext::EndFrame), et le champ de saisie est deja
					//    passe -- le nom ne serait pas choisi, et la frappe s'y
					//    ajouterait au lieu de le remplacer. (Le noeud vise est la
					//    ligne active, donc la selection.)
					ui.renommerEnPlace = true;
					ui.clicLentNoeud = 0;
				}
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
