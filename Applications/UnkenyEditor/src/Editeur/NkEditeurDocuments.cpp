//
// NkEditeurDocuments.cpp
// =============================================================================
// Description :
//   La barre des ONGLETS DE DOCUMENT (voir NkEditeurDocuments.h) : le premier
//   plan, la porte (quitter / entrer), fermer, suivre ce qui est ouvert, et la
//   barre elle-meme a droite de l'onglet de la scene.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurDocuments.h"

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurAssets.h"
#include "Editeur/NkEditeurContenu.h"
#include "Editeur/NkEditeurInterface.h"
#include "Editeur/NkEditeurPagesAnim.h"
#include "Script/NkEditeurGraphe.h"
#include "Script/NkEditeurScripts.h"

#include "NKCanvas/App/NkCanvasTexte.h"

#include <cstring>

namespace nkentseu {
	namespace editeur {

		using editorkit::NkRole;
		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {
			const NkDocAnim *Anim(const NkEditeurInterface &ui, nk_uint64 id) {
				for (uint32 i = 0; i < (uint32)ui.pagesAnim.docs.Size(); ++i) {
					if (ui.pagesAnim.docs[i].id == id) {
						return &ui.pagesAnim.docs[i];
					}
				}
				return nullptr;
			}

			int32 Asset(const NkEditeurInterface &ui, nk_uint64 id) {
				for (uint32 k = 0; k < ui.onglets.Size(); ++k) {
					if (ui.onglets[k]->id == id) {
						return static_cast<int32>(k);
					}
				}
				return -1;
			}

			NkEditeurGrapheEtat *Bp(NkEditeurModele &m, nk_uint64 id) {
				return m.scripts != nullptr ? NkEditeurGrapheParId(*m.scripts, id) : nullptr;
			}

			int32 Position(const NkDocuments &D, const NkDocumentOuvert &d) {
				for (uint32 i = 0; i < D.ordre.Size(); ++i) {
					if (D.ordre[i] == d) {
						return static_cast<int32>(i);
					}
				}
				return -1;
			}

			/// Le nom d'un fichier sans dossier ; sans extension si `tronc`.
			NkString NomFichier(const char *chemin, bool tronc) {
				if (chemin == nullptr) {
					return NkString();
				}
				const char *debut = chemin;
				for (const char *p = chemin; *p != '\0'; ++p) {
					if (*p == '/' || *p == '\\') {
						debut = p + 1;
					}
				}
				const char *point = tronc ? std::strrchr(debut, '.') : nullptr;
				return point != nullptr ? NkString(debut, static_cast<usize>(point - debut)) : NkString(debut);
			}

			/// Le document a-t-il des modifications non enregistrees ?
			bool Modifie(NkEditeurModele &m, NkEditeurInterface &ui, const NkDocumentOuvert &d) {
				switch (d.genre) {
					case NkGenreDocument::NK_ANIM: {
						const NkDocAnim *a = Anim(ui, d.cle);
						return a != nullptr && a->modifie;
					}
					case NkGenreDocument::NK_ASSET: {
						const int32 k = Asset(ui, d.cle);
						return k >= 0 && ui.onglets[static_cast<uint32>(k)]->modifie;
					}
					case NkGenreDocument::NK_BLUEPRINT: {
						const NkEditeurGrapheEtat *e = Bp(m, d.cle);
						return e != nullptr && e->modifie;
					}
					default:
						return false;
				}
			}

			/// La MARQUE d'un onglet, a gauche du nom : sa nature d'un coup d'oeil
			/// (la couleur de type d'Unreal ; le losange bleu d'un prefab).
			void Marque(NkEditeurCadre &c, const NkDocumentOuvert &d, const NkRect &r) {
				auto &dl = c.ctx.dl;
				const NkVec2 m{r.x + 12.f, r.y + r.h * 0.5f};
				switch (d.genre) {
					case NkGenreDocument::NK_ANIM: {
						const NkDocAnim *a = Anim(c.ui, d.cle);
						const bool clip = a == nullptr || a->genre == NkGenreDocAnim::NK_ANIMATION;
						const NkColor nature(c.theme.Get(clip ? NkRole::TypeAnim : NkRole::NodeActionHeader));
						dl.AddRectFilled(NkRect{m.x - 4.f, m.y - 4.f, 8.f, 8.f}, nature, 1.f);
						break;
					}
					case NkGenreDocument::NK_ASSET: {
						const int32 k = Asset(c.ui, d.cle);
						const NkGenreAsset g = k >= 0 ? c.ui.onglets[static_cast<uint32>(k)]->genre : NkGenreAsset::NK_AUCUN;
						if (g == NkGenreAsset::NK_PREFAB) {
							const NkColor bleu{90, 150, 235, 255};
							dl.AddTriangleFilled(NkVec2{m.x, m.y - 5.f}, NkVec2{m.x + 5.f, m.y}, NkVec2{m.x, m.y + 5.f}, bleu);
							dl.AddTriangleFilled(NkVec2{m.x, m.y - 5.f}, NkVec2{m.x, m.y + 5.f}, NkVec2{m.x - 5.f, m.y}, bleu);
						} else {
							NkColor col{150, 150, 150, 255};
							if (g == NkGenreAsset::NK_TEXTURE) {
								col = NkColor{110, 190, 110, 255};
							} else if (g == NkGenreAsset::NK_POLICE) {
								col = NkColor{220, 190, 90, 255};
							} else if (g == NkGenreAsset::NK_SON) {
								col = NkColor{230, 140, 70, 255};
							} else if (g == NkGenreAsset::NK_CONTROLEUR) {
								col = NkColor{170, 120, 220, 255};
							}
							dl.AddRectFilled(NkRect{m.x - 4.f, m.y - 4.f, 8.f, 8.f}, col, 1.f);
						}
						break;
					}
					case NkGenreDocument::NK_BLUEPRINT: {
						// Le bleu des Blueprints d'Unreal, et un fil entre deux noeuds.
						const NkColor bleu{60, 150, 250, 255};
						dl.AddRectFilled(NkRect{m.x - 5.f, m.y - 5.f, 10.f, 10.f}, bleu, 2.f);
						const NkColor fil{230, 240, 255, 255};
						dl.AddCircleFilled(NkVec2{m.x - 2.2f, m.y - 1.8f}, 1.3f, fil);
						dl.AddCircleFilled(NkVec2{m.x + 2.2f, m.y + 1.8f}, 1.3f, fil);
						dl.AddLine(NkVec2{m.x - 2.2f, m.y - 1.8f}, NkVec2{m.x + 2.2f, m.y + 1.8f}, fil, 1.f);
						break;
					}
					default:
						break;
				}
			}
		} // namespace

		// =====================================================================
		// LE PREMIER PLAN
		// =====================================================================
		NkDocumentOuvert NkEditeurDocumentActif(const NkEditeurInterface &ui) noexcept {
			return ui.documents.actif;
		}

		bool NkEditeurSceneDevant(const NkEditeurInterface &ui) noexcept {
			return ui.documents.actif.genre == NkGenreDocument::NK_SCENE;
		}

		bool NkEditeurBlueprintDevant(const NkEditeurInterface &ui) noexcept {
			return ui.documents.actif.genre == NkGenreDocument::NK_BLUEPRINT;
		}

		bool NkEditeurDocumentExiste(NkEditeurModele &m, const NkEditeurInterface &ui, const NkDocumentOuvert &d) noexcept {
			switch (d.genre) {
				case NkGenreDocument::NK_SCENE:
					return true;
				case NkGenreDocument::NK_ANIM:
					return Anim(ui, d.cle) != nullptr;
				case NkGenreDocument::NK_ASSET:
					return Asset(ui, d.cle) >= 0;
				case NkGenreDocument::NK_BLUEPRINT:
					return Bp(m, d.cle) != nullptr;
				default:
					return false;
			}
		}

		void NkEditeurSynchroniserDocuments(NkEditeurModele &m, NkEditeurInterface &ui) {
			NkDocuments &D = ui.documents;
			// 1. Un document disparu sort de la barre.
			for (uint32 i = 0; i < D.ordre.Size();) {
				if (!NkEditeurDocumentExiste(m, ui, D.ordre[i])) {
					D.ordre.Erase(D.ordre.Begin() + i);
				} else {
					++i;
				}
			}
			// 2. Un document ouvert SANS elle (un banc sans interface) y entre, a
			//    droite, dans l'ordre de son systeme.
			auto Entrer = [&](const NkDocumentOuvert &d) {
				if (Position(D, d) < 0) {
					D.ordre.PushBack(d);
				}
			};
			for (uint32 i = 0; i < (uint32)ui.pagesAnim.docs.Size(); ++i) {
				Entrer(NkDoc(NkGenreDocument::NK_ANIM, ui.pagesAnim.docs[i].id));
			}
			for (uint32 k = 0; k < ui.onglets.Size(); ++k) {
				Entrer(NkDoc(NkGenreDocument::NK_ASSET, ui.onglets[k]->id));
			}
			if (m.scripts != nullptr) {
				for (uint32 g = 0; g < m.scripts->graphes.Size(); ++g) {
					Entrer(NkDoc(NkGenreDocument::NK_BLUEPRINT, m.scripts->graphes[g]->id));
				}
			}
			// 3. Le premier plan a disparu (ferme sans passer par la barre) : la
			//    scene revient -- et si c'etait un prefab, la scene mise de cote aussi.
			if (!NkEditeurDocumentExiste(m, ui, D.actif)) {
				D.actif = NkDocScene();
				if (ui.modePrefab != nullptr) {
					NkEditeurAssetQuitte(m, ui, 0);
				}
			}
		}

		// =====================================================================
		// LA PORTE
		// =====================================================================
		bool NkEditeurActiverDocument(NkEditeurModele &m, NkEditeurInterface &ui, const NkDocumentOuvert &d) {
			NkEditeurSynchroniserDocuments(m, ui);
			if (!NkEditeurDocumentExiste(m, ui, d)) {
				return false;
			}
			NkDocuments &D = ui.documents;
			if (D.actif == d) {
				return true;
			}
			ui.menu = NkMenuEditeur::NK_AUCUN;
			// ── QUITTER celui d'avant : un son se tait, un prefab rend la scene ──
			if (D.actif.genre == NkGenreDocument::NK_ASSET || ui.modePrefab != nullptr) {
				NkEditeurAssetQuitte(m, ui, D.actif.genre == NkGenreDocument::NK_ASSET ? D.actif.cle : 0);
			}
			D.actif = NkDocScene();
			// ── ENTRER dans celui-ci ──
			if (d.genre == NkGenreDocument::NK_ASSET && !NkEditeurAssetEntre(m, ui, d.cle)) {
				return false; // un prefab refuse pendant le jeu : la scene reste devant
			}
			if (d.genre == NkGenreDocument::NK_BLUEPRINT && m.scripts != nullptr) {
				NkEditeurGrapheDevenirCourant(*m.scripts, d.cle);
			}
			D.actif = d;
			return true;
		}

		bool NkEditeurFermerDocument(NkEditeurModele &m, NkEditeurInterface &ui, const NkDocumentOuvert &d) {
			if (d.genre == NkGenreDocument::NK_SCENE) {
				return false;
			}
			NkEditeurSynchroniserDocuments(m, ui);
			if (!NkEditeurDocumentExiste(m, ui, d)) {
				return false;
			}
			NkDocuments &D = ui.documents;
			// Devant : son voisin de GAUCHE passe devant AVANT qu'il ne parte (un
			// prefab photographie son edition et rend la scene ; un son se tait).
			// A gauche du premier document, il y a la scene.
			if (D.actif == d) {
				const int32 p = Position(D, d);
				const NkDocumentOuvert voisin = p > 0 ? D.ordre[static_cast<uint32>(p - 1)] : NkDocScene();
				if (!NkEditeurActiverDocument(m, ui, voisin)) {
					NkEditeurActiverDocument(m, ui, NkDocScene());
				}
			}
			bool ok = false;
			switch (d.genre) {
				case NkGenreDocument::NK_ANIM:
					ok = NkEditeurDetruireDocAnim(m, ui, d.cle);
					break;
				case NkGenreDocument::NK_ASSET:
					ok = NkEditeurDetruireAsset(m, ui, d.cle);
					break;
				case NkGenreDocument::NK_BLUEPRINT:
					ok = m.scripts != nullptr && NkEditeurDetruireGraphe(*m.scripts, d.cle);
					break;
				default:
					break;
			}
			const int32 p = Position(D, d);
			if (p >= 0) {
				D.ordre.Erase(D.ordre.Begin() + p);
			}
			return ok;
		}

		bool NkEditeurFermerDocumentDevant(NkEditeurModele &m, NkEditeurInterface &ui) {
			const NkDocumentOuvert d = ui.documents.actif;
			return d.genre != NkGenreDocument::NK_SCENE && NkEditeurFermerDocument(m, ui, d);
		}

		NkString NkEditeurLibelleDocument(NkEditeurModele &m, NkEditeurInterface &ui, const NkDocumentOuvert &d) {
			switch (d.genre) {
				case NkGenreDocument::NK_ANIM: {
					const NkDocAnim *a = Anim(ui, d.cle);
					if (a == nullptr) {
						return NkString();
					}
					return NkString::Format("%s : %s", a->genre == NkGenreDocAnim::NK_ANIMATION ? "Animation" : "Animateur", a->nom.CStr());
				}
				case NkGenreDocument::NK_ASSET: {
					const int32 k = Asset(ui, d.cle);
					return k >= 0 ? NomFichier(ui.onglets[static_cast<uint32>(k)]->nav.CStr(), false) : NkString();
				}
				case NkGenreDocument::NK_BLUEPRINT: {
					const NkEditeurGrapheEtat *e = Bp(m, d.cle);
					return e != nullptr ? NkString::Format("Blueprint : %s", NomFichier(e->ref.CStr(), true).CStr()) : NkString();
				}
				default:
					return NkString("Scène");
			}
		}

		// =====================================================================
		// LA BARRE
		// =====================================================================
		void NkEditeurDessinerOngletsDocuments(NkEditeurCadre &c, float32 x) {
			NkEditeurInterface &ui = c.ui;
			NkDocuments &D = ui.documents;
			NkEditeurSynchroniserDocuments(c.m, ui);
			auto &dl = c.ctx.dl;
			const nkgui::NkGuiInput &in = c.ctx.input;
			const NkRect &b = ui.barreOnglets;
			D.rects.Clear();
			D.croix.Clear();
			// Les releves des anciens systemes, gardes pour les bancs qui y visent :
			// les onglets d'animation dans l'ordre de leurs documents, ceux des assets.
			ui.pagesAnim.onglets.Clear();
			ui.pagesAnim.croix.Clear();
			ui.ongletsRects.Clear();
			ui.ongletsFermer.Clear();
			const uint32 n = D.ordre.Size();
			if (n == 0u) {
				D.tasses = 0;
				return;
			}
			// ── Les largeurs : [ marque  nom  ●  ✕ ]. La place du point est TOUJOURS
			//    reservee (comme l'onglet de la scene) : la croix ne bouge pas a la
			//    premiere modification.
			const float32 avant = 24.f;	 // marque
			const float32 apres = 14.f + 22.f; // point, croix
			NkVector<NkString> libelles;
			NkVector<float32> largeurs;
			float32 total = 0.f;
			for (uint32 i = 0; i < n; ++i) {
				libelles.PushBack(NkEditeurLibelleDocument(c.m, ui, D.ordre[i]));
				const float32 tw = renderer::NkTexteLargeur(c.police, libelles[i].CStr());
				largeurs.PushBack(tw);
				total += avant + tw + apres + 3.f;
			}
			// ── PLEINE, la barre TASSE les noms les plus longs (jamais hors de la
			//    fenetre) : un plafond commun, les noms courts gardent leur largeur.
			const float32 dispo = b.x + b.w - 8.f - x;
			float32 plafond = 1.0e9f;
			D.tasses = 0;
			if (total > dispo) {
				float32 place = dispo - static_cast<float32>(n) * (avant + apres + 3.f);
				uint32 longs = n;
				NkVector<uint8> court;
				court.Resize(n);
				for (uint32 i = 0; i < n; ++i) {
					court[i] = 0u;
				}
				for (uint32 tour = 0; tour < n && longs > 0u; ++tour) {
					plafond = place / static_cast<float32>(longs);
					bool change = false;
					for (uint32 i = 0; i < n; ++i) {
						if (court[i] == 0u && largeurs[i] <= plafond) {
							court[i] = 1u;
							place -= largeurs[i];
							--longs;
							change = true;
						}
					}
					if (!change) {
						break;
					}
				}
				plafond = plafond < 18.f ? 18.f : plafond;
				for (uint32 i = 0; i < n; ++i) {
					D.tasses += largeurs[i] > plafond ? 1 : 0;
				}
			}
			NkDocumentOuvert aFermer = NkDocScene(), aActiver = NkDocScene();
			bool fermer = false, activer = false;
			const float32 lh = renderer::NkTexteHauteurLigne(c.police, 16.f);
			for (uint32 i = 0; i < n; ++i) {
				const NkDocumentOuvert &d = D.ordre[i];
				const float32 tw = largeurs[i] < plafond ? largeurs[i] : plafond;
				const float32 w = avant + tw + apres;
				const NkRect r{x, b.y + 3.f, w, b.h - 3.f};
				const bool actif = d == D.actif;
				const bool survol = NkEditeurDans(r, in.mousePos);
				dl.AddRectFilled(r, actif ? c.pal.panneau : (survol ? c.pal.boutonSurvol : c.pal.fond), 2.f);
				if (actif) {
					dl.AddRectFilled(NkRect{r.x, r.y, r.w, 2.f}, c.pal.accent);
				}
				Marque(c, d, r);
				const NkRect texte{r.x + avant, r.y, tw, r.h};
				dl.PushClipRect(texte, true);
				renderer::NkTexte(dl, c.police, texte.x, r.y + (r.h - lh) * 0.5f, libelles[i].CStr(), actif ? c.pal.texte : c.pal.attenue);
				dl.PopClipRect();
				if (Modifie(c.m, ui, d)) {
					dl.AddCircleFilled(NkVec2{r.x + avant + tw + 7.f, r.y + r.h * 0.5f}, 3.5f, c.pal.selection);
				}
				const NkRect cx{r.x + r.w - 20.f, r.y + (r.h - 16.f) * 0.5f, 16.f, 16.f};
				const bool surX = NkEditeurDans(cx, in.mousePos);
				if (surX) {
					dl.AddRectFilled(cx, c.pal.boutonSurvol, 2.f);
				}
				// La croix est TRACEE (deux traits), jamais un glyphe.
				const NkColor teinte = surX ? c.pal.texte : c.pal.attenue;
				dl.AddLine(NkVec2{cx.x + 4.5f, cx.y + 4.5f}, NkVec2{cx.x + cx.w - 4.5f, cx.y + cx.h - 4.5f}, teinte, 1.4f);
				dl.AddLine(NkVec2{cx.x + cx.w - 4.5f, cx.y + 4.5f}, NkVec2{cx.x + 4.5f, cx.y + cx.h - 4.5f}, teinte, 1.4f);
				D.rects.PushBack(r);
				D.croix.PushBack(cx);
				if (d.genre == NkGenreDocument::NK_ANIM) {
					ui.pagesAnim.onglets.PushBack(r);
					ui.pagesAnim.croix.PushBack(cx);
				} else if (d.genre == NkGenreDocument::NK_ASSET) {
					ui.ongletsRects.PushBack(r);
					ui.ongletsFermer.PushBack(cx);
				}
				// La croix, ou le clic du MILIEU (Unreal, les navigateurs) : fermer.
				if ((in.mouseClicked[0] && surX) || (in.mouseClicked[2] && survol)) {
					aFermer = d;
					fermer = true;
				} else if (in.mouseClicked[0] && survol) {
					aActiver = d;
					activer = true;
				}
				x += w + 3.f;
			}
			// Apres le parcours : jamais la liste qu'on parcourt.
			if (fermer) {
				NkEditeurFermerDocument(c.m, ui, aFermer);
			} else if (activer) {
				NkEditeurActiverDocument(c.m, ui, aActiver);
			}
		}

	} // namespace editeur
} // namespace nkentseu
