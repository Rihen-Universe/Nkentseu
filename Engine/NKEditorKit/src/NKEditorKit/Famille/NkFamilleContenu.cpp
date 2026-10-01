// -----------------------------------------------------------------------------
// @File    NkFamilleContenu.cpp
// @Brief   Le tiroir « Contenu » de la famille (voir NkFamilleContenu.h).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NKEditorKit/Famille/NkFamilleContenu.h"
#include "NKEditorKit/Components/NkGuiComponentPaint.h"
#include "NKEditorKit/Components/NkSilhouettes.h"
#include "NKEditorKit/NkEditorScrollbar.h"
#include "NKEditorKit/NkEditorTextField.h"
#include "NKFileSystem/NkDirectory.h"

#include <cstring>

namespace nkentseu {
	namespace editorkit {

		using nkgui::NkRect;

		namespace {
			const char *const RACINE_NAV = "Contenu"; ///< la racine du projet dans le navigateur (Unreal : /Content)
			const char *const TOUT_NAV = "Tout";	  ///< Unreal : « All »
			constexpr nk_uint64 ID_CONTENU = 1000u;

			char Minuscule(char ch) {
				return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
			}

			/// L'extension (minuscules, avec le point) ; vide sans point.
			NkString Extension(const NkString &nom) {
				const char *s = nom.CStr();
				const char *point = nullptr;
				for (const char *p = s; *p != '\0'; ++p) {
					if (*p == '.') {
						point = p;
					}
				}
				if (point == nullptr || point == s) {
					return NkString();
				}
				NkString r;
				for (const char *p = point; *p != '\0'; ++p) {
					r.Append(Minuscule(*p));
				}
				return r;
			}

			bool Avant(const NkString &a, const NkString &b) {
				const char *x = a.CStr();
				const char *y = b.CStr();
				while (*x != '\0' && *y != '\0') {
					const char cx = Minuscule(*x), cy = Minuscule(*y);
					if (cx != cy) {
						return cx < cy;
					}
					++x;
					++y;
				}
				return *x == '\0' && *y != '\0';
			}

			void Trier(NkVector<NkString> &v) {
				for (uint32 i = 1; i < v.Size(); ++i) {
					for (uint32 j = i; j > 0 && Avant(v[j], v[j - 1]); --j) {
						const NkString t = v[j];
						v[j] = v[j - 1];
						v[j - 1] = t;
					}
				}
			}

			NkString Joindre(const NkString &a, const NkString &b) {
				if (a.Empty()) {
					return b;
				}
				NkString r = a;
				r.Append('/');
				r.Append(b);
				return r;
			}

			/// Les sous-dossiers de `absolu` (cachés exclus), recursivement, en ordre prefixe.
			void Parcourir(const NkString &absolu, const NkString &relatif, NkVector<NkString> &sortie, uint32 profondeur) {
				if (profondeur > 6u || sortie.Size() >= 400u) {
					return;
				}
				const NkVector<NkDirectoryEntry> ent = NkDirectory::GetEntries(absolu.CStr());
				NkVector<NkString> noms;
				for (uint32 i = 0; i < ent.Size(); ++i) {
					if (ent[i].IsDirectory && !ent[i].IsHidden && ent[i].Name.Length() > 0 && ent[i].Name.CStr()[0] != '.') {
						noms.PushBack(ent[i].Name);
					}
				}
				Trier(noms);
				for (uint32 i = 0; i < noms.Size(); ++i) {
					const NkString rel = Joindre(relatif, noms[i]);
					sortie.PushBack(rel);
					Parcourir(Joindre(absolu, noms[i]), rel, sortie, profondeur + 1u);
				}
			}

			/// « Contenu/a/b » -> « a/b » ; « Contenu » -> « » ; autre -> faux.
			bool RelatifDe(const char *nav, NkString &rel) {
				if (nav == nullptr) {
					return false;
				}
				const usize n = std::strlen(RACINE_NAV);
				if (std::strncmp(nav, RACINE_NAV, n) != 0 || (nav[n] != '\0' && nav[n] != '/')) {
					return false;
				}
				rel = NkString(nav[n] == '/' ? nav + n + 1 : nav + n);
				return true;
			}

			NkString Navigateur(const NkString &rel) {
				return rel.Empty() ? NkString(RACINE_NAV) : Joindre(NkString(RACINE_NAV), rel);
			}

			struct NkPontContenu {
					NkFamilleContenu *c;
					bool ouvert = false;
			};

			void SurNavigation(void *user, const char *chemin) {
				NkPontContenu &p = *static_cast<NkPontContenu *>(user);
				NkString rel;
				if (chemin != nullptr && std::strcmp(chemin, TOUT_NAV) == 0) {
					p.c->tout = true;
				} else if (RelatifDe(chemin, rel)) {
					p.c->Aller(rel);
				}
			}

			void SurSelection(void *user, int32, const char *chemin) {
				NkPontContenu &p = *static_cast<NkPontContenu *>(user);
				p.c->actif = NkString(chemin != nullptr ? chemin : "");
			}

			void SurDoubleClic(void *user, int32 index, const char *chemin) {
				NkPontContenu &p = *static_cast<NkPontContenu *>(user);
				NkFamilleContenu &c = *p.c;
				if (index < 0 || index >= static_cast<int32>(c.modele.entries.Size())) {
					return;
				}
				NkString rel;
				if (c.modele.entries[static_cast<uint32>(index)].isFolder) {
					if (RelatifDe(chemin, rel)) {
						c.Aller(rel);
					}
					return;
				}
				c.ouvert = c.Disque(chemin);
				p.ouvert = true;
			}

			void Preparer(NkFamilleContenu &c, const NkFamilleNatureFichier *natures, int32 n) {
				if (c.pret) {
					return;
				}
				c.pret = true;
				c.reglages.Bind(NkContentBrowserDecl());
				// LA VARIANTE D'UNREAL (UnkenyEditor, document 02 §3) : sept zones.
				c.reglages.SetVariantByName("unreal");
				c.reglages.SetParam("show_badge", 0.f);
				c.reglages.SetParam("tree_width", 0.17f);
				c.reglages.SetParam("multi_select", 1.f);
				c.modele.thumbSize = 72.f;
				c.modele.kinds.Clear();
				for (int32 k = 0; k < n; ++k) {
					// Une puce par LIBELLE (deux extensions d'une meme nature n'en font qu'une).
					bool deja = false;
					for (uint32 i = 0; i < c.modele.kinds.Size(); ++i) {
						deja = deja || c.modele.kinds[i].label == NkString(natures[k].libelle);
					}
					if (!deja) {
						NkBrowserKind kind;
						kind.label = NkString(natures[k].libelle);
						kind.role = static_cast<uint16>(natures[k].role);
						c.modele.kinds.PushBack(kind);
					}
				}
				c.modele.folders.toggled.PushBack(ID_CONTENU);
			}

			void Relire(NkFamilleContenu &c, float32 dt) {
				c.age += dt;
				if (!c.perime && c.age < 1.f) {
					return;
				}
				c.sousDossiers.Clear();
				Parcourir(c.racine, NkString(), c.sousDossiers, 0u);
				// Un dossier supprime hors de l'editeur : on remonte a la racine.
				bool existe = c.courant.Empty();
				for (uint32 i = 0; i < c.sousDossiers.Size() && !existe; ++i) {
					existe = c.sousDossiers[i] == c.courant;
				}
				if (!existe) {
					c.courant = NkString();
				}
				c.age = 0.f;
				c.perime = false;
			}

			void Reconstruire(NkFamilleContenu &c, const NkFamilleNatureFichier *natures, int32 nbNatures) {
				NkContentBrowserModel &m = c.modele;
				// ── Le rail : « Contenu » et son arbre ─────────────────────────
				m.folders.nodes.Clear();
				NkTreeNode r;
				r.id = ID_CONTENU;
				r.parent = -1;
				r.label = NkString(RACINE_NAV);
				r.path = NkString(RACINE_NAV);
				r.kindRole = static_cast<uint16>(NkRole::TypeFolder);
				r.silhouette = static_cast<uint8>(NkAssetIcone::Dossier);
				m.folders.nodes.PushBack(r);
				nk_uint64 actifRail = c.tout ? 0u : ID_CONTENU;
				for (uint32 k = 0; k < c.sousDossiers.Size(); ++k) {
					const NkString &rel = c.sousDossiers[k];
					usize coupe = 0;
					bool aParent = false;
					for (usize i = 0; i < static_cast<usize>(rel.Length()); ++i) {
						if (rel.CStr()[i] == '/') {
							coupe = i;
							aParent = true;
						}
					}
					int32 parent = 0;
					if (aParent) {
						const NkString cheminParent = Navigateur(NkString(rel.CStr(), coupe));
						for (int32 j = static_cast<int32>(m.folders.nodes.Size()) - 1; j > 0; --j) {
							if (m.folders.nodes[static_cast<uint32>(j)].path == cheminParent) {
								parent = j;
								break;
							}
						}
					}
					NkTreeNode n;
					n.id = ID_CONTENU + 1u + k;
					n.parent = parent;
					n.label = NkString(rel.CStr() + (aParent ? coupe + 1 : 0));
					n.path = Navigateur(rel);
					n.kindRole = static_cast<uint16>(NkRole::TypeFolder);
					n.silhouette = static_cast<uint8>(NkAssetIcone::Dossier);
					m.folders.nodes.PushBack(n);
					if (!c.tout && rel == c.courant) {
						actifRail = n.id;
					}
				}
				m.folders.active = actifRail;
				m.folders.chosen.Clear();
				if (actifRail != 0u) {
					m.folders.chosen.PushBack(actifRail);
				}
				// ── Le fil d'Ariane ─────────────────────────────────────────────
				m.breadcrumb.Clear();
				m.breadcrumb.PushBack(NkString(TOUT_NAV));
				if (!c.tout) {
					m.breadcrumb.PushBack(NkString(RACINE_NAV));
					const char *d = c.courant.CStr();
					while (*d != '\0') {
						const char *f = d;
						while (*f != '\0' && *f != '/') {
							++f;
						}
						m.breadcrumb.PushBack(NkString(d, static_cast<usize>(f - d)));
						d = *f == '/' ? f + 1 : f;
					}
				}
				m.cheminCourant = c.tout ? NkString(TOUT_NAV) : Navigateur(c.courant);
				m.nomProjet = c.nomProjet;

				// ── Les cartes : les dossiers, puis les fichiers ───────────────
				m.entries.Clear();
				if (c.tout) {
					NkAssetEntry d;
					d.name = NkString(RACINE_NAV);
					d.path = d.name;
					d.isFolder = true;
					d.kindLabel = "Dossier";
					d.kindRole = static_cast<uint16>(NkRole::TypeFolder);
					d.icone = static_cast<uint8>(NkAssetIcone::Dossier);
					m.entries.PushBack(d);
				} else {
					const NkString absolu = c.courant.Empty() ? c.racine : Joindre(c.racine, c.courant);
					const NkVector<NkDirectoryEntry> ent = NkDirectory::GetEntries(absolu.CStr());
					for (int32 passe = 0; passe < 2; ++passe) {
						NkVector<NkString> noms;
						NkVector<uint32> indices;
						for (uint32 i = 0; i < ent.Size(); ++i) {
							const bool dossier = ent[i].IsDirectory;
							if ((passe == 0) != dossier || ent[i].IsHidden || ent[i].Name.Length() == 0 || ent[i].Name.CStr()[0] == '.') {
								continue;
							}
							noms.PushBack(ent[i].Name);
							indices.PushBack(i);
						}
						// Le tri par nom, l'indice suit.
						for (uint32 i = 1; i < noms.Size(); ++i) {
							for (uint32 j = i; j > 0 && Avant(noms[j], noms[j - 1]); --j) {
								const NkString t = noms[j];
								noms[j] = noms[j - 1];
								noms[j - 1] = t;
								const uint32 u = indices[j];
								indices[j] = indices[j - 1];
								indices[j - 1] = u;
							}
						}
						for (uint32 i = 0; i < noms.Size(); ++i) {
							const NkDirectoryEntry &e = ent[indices[i]];
							NkAssetEntry a;
							a.isFolder = passe == 0;
							a.path = Navigateur(Joindre(c.courant, noms[i]));
							a.taille = e.Size;
							a.dateModif = e.ModificationTime;
							if (a.isFolder) {
								a.name = noms[i];
								a.kindLabel = "Dossier";
								a.kindRole = static_cast<uint16>(NkRole::TypeFolder);
								a.icone = static_cast<uint8>(NkAssetIcone::Dossier);
							} else {
								// Sans EXTENSION, comme Unreal : le type est ecrit dessous.
								const NkString ext = Extension(noms[i]);
								a.name = NkString(noms[i].CStr(), noms[i].Length() - ext.Length());
								a.kindLabel = "Fichier";
								a.kindRole = static_cast<uint16>(NkRole::TextMuted);
								a.icone = static_cast<uint8>(NkAssetIcone::Inconnu);
								for (int32 k = 0; k < nbNatures; ++k) {
									if (ext == NkString(natures[k].extension)) {
										a.kindLabel = natures[k].libelle;
										a.kindRole = static_cast<uint16>(natures[k].role);
										a.icone = natures[k].icone;
										break;
									}
								}
							}
							m.entries.PushBack(a);
						}
					}
				}
				// La selection relue du CHEMIN garde.
				m.active = -1;
				m.chosen.Clear();
				for (uint32 i = 0; i < m.entries.Size(); ++i) {
					if (!c.actif.Empty() && m.entries[i].path == c.actif) {
						m.active = static_cast<int32>(i);
						m.chosen.PushBack(static_cast<int32>(i));
					}
				}
				m.statusRight = NkString();
			}
		} // namespace

		void NkFamilleContenu::Aller(const NkString &rel) {
			tout = false;
			courant = rel;
			actif = NkString();
			modele.scroll = 0.f;
			perime = true;
		}

		NkString NkFamilleContenu::Disque(const char *nav) const {
			NkString rel;
			if (!RelatifDe(nav, rel)) {
				return NkString();
			}
			return rel.Empty() ? racine : Joindre(racine, rel);
		}

		bool NkFamilleDessinerContenu(NkFamilleCtx &c, const NkRect &zone, NkFamilleContenu &contenu,
									  const NkFamilleNatureFichier *natures, int32 nbNatures, float32 dt) {
			if (zone.w < 40.f || zone.h < 40.f) {
				return false;
			}
			Preparer(contenu, natures, nbNatures);
			Relire(contenu, dt);
			Reconstruire(contenu, natures, nbNatures);

			NkContentBrowserStyle s;
			s.values = &contenu.reglages;
			s.panelBg = static_cast<uint16>(NkRole::PanelBg);
			s.headerBg = static_cast<uint16>(NkRole::PanelHeader);
			s.border = static_cast<uint16>(NkRole::Border);
			s.text = static_cast<uint16>(NkRole::Text);
			s.textMuted = static_cast<uint16>(NkRole::TextMuted);
			// Unreal : la vignette sur un fond PLUS SOMBRE que le panneau, le pied
			// un ton au-dessus ; le survol eclaircit franchement.
			s.cardBg = static_cast<uint16>(NkRole::WindowBg);
			s.cardFooterBg = static_cast<uint16>(NkRole::PanelHeader);
			s.activeMark = static_cast<uint16>(NkRole::AccentUi);
			s.chosenMark = static_cast<uint16>(NkRole::AccentSel);
			s.folderTint = static_cast<uint16>(NkRole::TypeFolder);
			s.chipBg = static_cast<uint16>(NkRole::PanelHeader);
			s.badgeText = static_cast<uint16>(NkRole::PanelBg);
			s.statusBg = static_cast<uint16>(NkRole::PanelHeader);
			s.cardHover = static_cast<uint16>(NkRole::Border);
			s.addMark = static_cast<uint16>(NkRole::StatusOk);
			s.dragMark = static_cast<uint16>(NkRole::AccentSel);
			s.textOnAccent = static_cast<uint16>(NkRole::TextOnAccent);

			NkPontContenu pont{&contenu};
			NkContentBrowserHooks hooks;
			hooks.user = &pont;
			hooks.onSelect = &SurSelection;
			hooks.onNavigate = &SurNavigation;
			hooks.onDoubleClick = &SurDoubleClic;

			const NkComponentInput ci = NkFamilleEntreeComposant(c.ctx);
			NkGuiComponentPaint peintre(c.ctx, c.theme);
			const NkContentBrowserResult res = NkDrawContentBrowser(peintre, ci, NkPaintRect{zone.x, zone.y, zone.w, zone.h},
																	contenu.modele, s, hooks);
			if (res.defilContenu > res.defilVue && res.defilW > 0.f && res.defilH > 0.f) {
				NkVScrollbar(c.ctx, c.ctx.dl, NkRect{res.defilX, res.defilY, res.defilW, res.defilH}, contenu.modele.scroll,
							 res.defilContenu, res.defilVue, c.ctx.GetId("famille.contenu.defil"), res.defilPas);
			}
			if (res.railDefilContenu > res.railDefilVue && res.railDefilW > 0.f && res.railDefilH > 0.f) {
				NkVScrollbar(c.ctx, c.ctx.dl, NkRect{res.railDefilX, res.railDefilY, res.railDefilW, res.railDefilH},
							 contenu.modele.folders.scroll, res.railDefilContenu, res.railDefilVue,
							 c.ctx.GetId("famille.contenu.rail"), res.railDefilPas);
			}
			// Le fil d'Ariane : une miette rend son INDICE ; on refait le chemin.
			if (res.navigatedCrumb >= 0) {
				if (res.navigatedCrumb == 0) {
					contenu.tout = true;
					contenu.actif = NkString();
				} else {
					NkString rel;
					for (int32 k = 2; k <= res.navigatedCrumb && k < static_cast<int32>(contenu.modele.breadcrumb.Size()); ++k) {
						rel = Joindre(rel, contenu.modele.breadcrumb[static_cast<uint32>(k)]);
					}
					contenu.Aller(rel);
				}
			}
			// La recherche du composant garde son champ : la saisie se pose dessus.
			if (contenu.modele.searchFocused && res.rechercheW > 0.f) {
				const nkgui::NkGuiInput &in = c.ctx.input;
				if (in.KeyPressed(nkgui::NkGuiKey::Escape) || in.KeyPressed(nkgui::NkGuiKey::Enter)) {
					contenu.modele.searchFocused = false;
				}
				NkOverlayFieldStyle st;
				st.fond = false;
				st.bord = false;
				st.texte = c.pal.texte;
				st.utf8 = true;
				const NkRect champ{res.rechercheX, res.rechercheY, res.rechercheW - 4.f, res.rechercheH};
				c.ctx.dl.AddRectFilled(NkRect{champ.x, champ.y + 1.f, champ.w, champ.h - 2.f}, c.pal.fond);
				NkOverlayTextField(c.ctx, c.ctx.dl, c.police, champ, contenu.modele.filter,
								   static_cast<int32>(sizeof(contenu.modele.filter)), contenu.modele.searchFocused, &st);
			}
			return pont.ouvert;
		}

	} // namespace editorkit
} // namespace nkentseu
