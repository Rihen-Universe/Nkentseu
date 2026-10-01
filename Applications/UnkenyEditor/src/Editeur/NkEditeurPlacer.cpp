// -----------------------------------------------------------------------------
// FICHIER: Editeur/NkEditeurPlacer.cpp
// DESCRIPTION: « Placer des acteurs » (document 02 §4) : le catalogue, le
//              panneau a onglets verticaux, le glisser vers la vue, les menus
//              Formes / Collisionneur / Volumes et la plage d'actions 1500-1599.
//              Voir NkEditeurPlacer.h.
//
// ⚠️ CE QUE LE CATALOGUE NE PROPOSE PAS, ET POURQUOI
//   Le document cite « Point de depart du joueur », « Camera » et « Texte » :
//   Unkeny n'a ni composant de point de depart, ni camera ENTITE (la camera est
//   celle de la scene, NkVue2D), ni texte de scene. Le panneau ne les invente
//   pas ; il les ajoutera le jour ou le moteur les portera.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Editeur/NkEditeurPlacer.h"
#include "Editeur/NkEditeurLumiere.h"

#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKEditorKit/NkEditorTextField.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {
			constexpr float32 ONGLETS_L = 54.f;	   ///< la colonne des onglets verticaux
			constexpr float32 ONGLET_H = 52.f;
			constexpr float32 RANGEE_H = 30.f;	   ///< une rangee de la liste
			constexpr float32 SEUIL_GLISSER = 5.f; ///< px : en deca, c'est un clic
			constexpr uint32 RECENTS_MAX = 12u;

			uint32 Bit(NkOngletPlacer o) noexcept {
				return 1u << static_cast<uint32>(o);
			}

			/// Le catalogue, construit une fois : les formes, puis la base, les
			/// lumieres, les effets, les volumes et la matiere de simulation.
			struct NkCatalogue {
					NkElementPlacer e[64];
					int32 n = 0;
					void Ajouter(const char *nom, const char *aide, NkNaturePlacer nature, int32 indice, uint32 onglets) {
						if (n < 64) {
							e[n++] = NkElementPlacer{nom, aide, nature, indice, onglets | Bit(NkOngletPlacer::NK_TOUT)};
						}
					}
			};

			const NkCatalogue &Catalogue() {
				static NkCatalogue c;
				if (c.n > 0) {
					return c;
				}
				const uint32 base = Bit(NkOngletPlacer::NK_BASE);
				const uint32 lum = Bit(NkOngletPlacer::NK_LUMIERES);
				const uint32 formes = Bit(NkOngletPlacer::NK_FORMES);
				const uint32 eff = Bit(NkOngletPlacer::NK_EFFETS);
				const uint32 vol = Bit(NkOngletPlacer::NK_VOLUMES);
				// --- Base (ce qu'Unkeny porte) ------------------------------------
				c.Ajouter("Acteur vide", "Un transform et un nom : le parent d'un groupe, un repère.", NkNaturePlacer::NK_VIDE, 0, base);
				c.Ajouter("Sprite", "Un sprite carré, sa boîte de collision et un corps.", NkNaturePlacer::NK_SPRITE, 0, base);
				c.Ajouter("Personnage", "Capsule debout, corps dynamique sans rotation, contrôleur de personnage (flèches, Espace).",
						  NkNaturePlacer::NK_PERSONNAGE, 0, base);
				c.Ajouter("Caisse", "Corps RIGIDE de 20 kg.", NkNaturePlacer::NK_SIM, static_cast<int32>(NkActeurSim::NK_CAISSE), base);
				c.Ajouter("Balle", "Corps RIGIDE rond, qui rebondit.", NkNaturePlacer::NK_SIM, static_cast<int32>(NkActeurSim::NK_BALLE), base);
				c.Ajouter("Corps mou", "Un blob visqueux (matière de NKPhysics).", NkNaturePlacer::NK_SIM,
						  static_cast<int32>(NkActeurSim::NK_BLOB), base);
				c.Ajouter("Son", "Une source sonore (choisir le son dans les Détails).", NkNaturePlacer::NK_SON, 0, base);
				c.Ajouter("Déclencheur", "Une zone qui détecte sans repousser (fin de niveau, ramassage).",
						  NkNaturePlacer::NK_DECLENCHEUR, 0, base | vol);
				// --- Lumieres (l'eclairage 2D) ------------------------------------
				c.Ajouter("Lumière ponctuelle", "Une lampe, une torche : un disque de lumière.", NkNaturePlacer::NK_LUMIERE,
						  static_cast<int32>(NkTypeLumiere2D::NK_PONCTUELLE), lum);
				c.Ajouter("Projecteur", "La lumière dans un cône.", NkNaturePlacer::NK_LUMIERE, static_cast<int32>(NkTypeLumiere2D::NK_SPOT), lum);
				c.Ajouter("Lumière directionnelle", "Le soleil, la lune : partout, d'une direction.", NkNaturePlacer::NK_LUMIERE,
						  static_cast<int32>(NkTypeLumiere2D::NK_DIRECTIONNELLE), lum);
				// --- Formes 2D -----------------------------------------------------
				c.Ajouter("Rectangle", "Remplissage, contour, coins arrondis ; collision : boîte.", NkNaturePlacer::NK_FORME,
						  static_cast<int32>(NkGenreForme2D::NK_RECTANGLE), formes);
				c.Ajouter("Carré", "Un rectangle de côtés égaux.", NkNaturePlacer::NK_CARRE, static_cast<int32>(NkGenreForme2D::NK_RECTANGLE), formes);
				c.Ajouter("Cercle", "Collision : cercle exact.", NkNaturePlacer::NK_FORME, static_cast<int32>(NkGenreForme2D::NK_CERCLE), formes);
				c.Ajouter("Ellipse", "Collision : polygone (dynamique) ou contour (décor).", NkNaturePlacer::NK_FORME,
						  static_cast<int32>(NkGenreForme2D::NK_ELLIPSE), formes);
				c.Ajouter("Triangle", "Collision : polygone exact.", NkNaturePlacer::NK_FORME, static_cast<int32>(NkGenreForme2D::NK_TRIANGLE), formes);
				c.Ajouter("Étoile", "Nombre de branches, rayon intérieur ; décor : contour exact.", NkNaturePlacer::NK_FORME,
						  static_cast<int32>(NkGenreForme2D::NK_ETOILE), formes);
				c.Ajouter("Polygone régulier", "Nombre de côtés (3 à 32).", NkNaturePlacer::NK_FORME,
						  static_cast<int32>(NkGenreForme2D::NK_POLYGONE_REGULIER), formes);
				c.Ajouter("Capsule", "Collision : capsule exacte.", NkNaturePlacer::NK_FORME, static_cast<int32>(NkGenreForme2D::NK_CAPSULE), formes);
				c.Ajouter("Ligne / chaîne", "Une ligne brisée épaisse : une pente, un fil.", NkNaturePlacer::NK_FORME,
						  static_cast<int32>(NkGenreForme2D::NK_LIGNE), formes);
				c.Ajouter("Polygone libre", "Points à la main (Détails) : une colline, une flèche.", NkNaturePlacer::NK_FORME,
						  static_cast<int32>(NkGenreForme2D::NK_POLYGONE_LIBRE), formes);
				// --- Effets (emetteurs) --------------------------------------------
				for (int32 p = 1; p < static_cast<int32>(NkPresetEffet2D::NK_COUNT); ++p) {
					c.Ajouter(NkNomPresetEffet2D(static_cast<NkPresetEffet2D>(p)), "Un émetteur de particules visuelles.",
							  NkNaturePlacer::NK_EFFET, p, eff);
				}
				// --- Volumes (collision sans image) -------------------------------
				c.Ajouter("Volume bloquant (boîte)", "Il arrête les corps et ne se voit pas en jeu.", NkNaturePlacer::NK_VOLUME,
						  static_cast<int32>(NkCollisionEditeur::NK_BOITE), vol);
				c.Ajouter("Volume bloquant (cercle)", "Il arrête les corps et ne se voit pas en jeu.", NkNaturePlacer::NK_VOLUME,
						  static_cast<int32>(NkCollisionEditeur::NK_CERCLE), vol);
				c.Ajouter("Volume bloquant (capsule)", "Il arrête les corps et ne se voit pas en jeu.", NkNaturePlacer::NK_VOLUME,
						  static_cast<int32>(NkCollisionEditeur::NK_CAPSULE), vol);
				c.Ajouter("Volume bloquant (polygone)", "Sommets à régler dans la vue (Éditer la collision).", NkNaturePlacer::NK_VOLUME,
						  static_cast<int32>(NkCollisionEditeur::NK_POLYGONE), vol);
				// --- La matiere de simulation (dans Tout) -------------------------
				for (int32 i = 0; i < static_cast<int32>(NkActeurSim::NK_COUNT); ++i) {
					const NkInfoActeurSim &info = NkActeurSimInfo(static_cast<NkActeurSim>(i));
					// Le pont se TRACE entre deux points : il reste a l'outil Poser.
					if (info.trace || i == static_cast<int32>(NkActeurSim::NK_CAISSE) || i == static_cast<int32>(NkActeurSim::NK_BALLE) ||
						i == static_cast<int32>(NkActeurSim::NK_BLOB)) {
						continue;
					}
					c.Ajouter(info.nom, info.description, NkNaturePlacer::NK_SIM, i, 0u);
				}
				return c;
			}

			/// Minuscules ASCII, sans accents (les plus courants du francais) : « eto »
			/// trouve « Étoile ».
			void Plier(const char *s, char *out, uint32 cap) {
				uint32 n = 0;
				for (const unsigned char *p = reinterpret_cast<const unsigned char *>(s); *p != 0u && n + 1u < cap; ++p) {
					unsigned char ch = *p;
					if (ch == 0xC3u && p[1] != 0u) {
						const unsigned char d = p[1];
						++p;
						if ((d >= 0x80u && d <= 0x85u) || (d >= 0xA0u && d <= 0xA5u)) {
							ch = 'a';
						} else if (d == 0x87u || d == 0xA7u) {
							ch = 'c';
						} else if ((d >= 0x88u && d <= 0x8Bu) || (d >= 0xA8u && d <= 0xABu)) {
							ch = 'e';
						} else if ((d >= 0x8Cu && d <= 0x8Fu) || (d >= 0xACu && d <= 0xAFu)) {
							ch = 'i';
						} else if ((d >= 0x92u && d <= 0x96u) || (d >= 0xB2u && d <= 0xB6u)) {
							ch = 'o';
						} else if ((d >= 0x99u && d <= 0x9Cu) || (d >= 0xB9u && d <= 0xBCu)) {
							ch = 'u';
						} else {
							continue;
						}
					} else if (ch >= 'A' && ch <= 'Z') {
						ch = static_cast<unsigned char>(ch - 'A' + 'a');
					}
					out[n++] = static_cast<char>(ch);
				}
				out[n] = '\0';
			}

			bool Correspond(const char *nom, const char *filtre) {
				if (filtre == nullptr || filtre[0] == '\0') {
					return true;
				}
				char a[96], b[64];
				Plier(nom, a, sizeof(a));
				Plier(filtre, b, sizeof(b));
				return std::strstr(a, b) != nullptr;
			}

			/// Un nom libre dans la scene : « Étoile », puis « Étoile 2 »...
			NkString NomLibre(NkScene &s, const char *base) {
				int32 pris = 0;
				const usize l = std::strlen(base);
				s.Monde().Query<NkEtiquette>().ForEach([&](ecs::NkEntityId, NkEtiquette &e) {
					if (std::strncmp(e.nom, base, l) == 0 && (e.nom[l] == '\0' || e.nom[l] == ' ')) {
						++pris;
					}
				});
				return pris == 0 ? NkString(base) : NkString::Format("%s %d", base, pris + 1);
			}

			void Choisir(NkEditeurModele &m, ecs::NkEntityId e) {
				if (e.IsValid()) {
					m.selection = e;
					m.aSelection = true;
				}
			}

			/// Une forme CACHEE qui marque un volume : NkDessinerFormes ne peint pas
			/// le collisionneur d'une entite qui a une forme, et celle-ci ne se
			/// dessine pas. Le volume ne se voit qu'en edition (surcouche
			/// « Collisionneurs »), comme les Blocking Volumes d'UE5.
			void MarquerInvisible(NkScene &s, ecs::NkEntityId e) {
				NkRenduForme2D f;
				f.visible = false;
				f.collisionSuit = false;
				s.Monde().Add<NkRenduForme2D>(e, f);
			}

			NkCollisionneur2D CollisionneurDeBase(NkCollisionEditeur k, const NkVec2f &demi) {
				NkCollisionneur2D c;
				const float32 r = math::NkMax(math::NkMin(demi.x, demi.y), 0.05f);
				switch (k) {
					case NkCollisionEditeur::NK_CERCLE:
						c.forme = NkForme2D::NK_CERCLE;
						c.rayon = r;
						break;
					case NkCollisionEditeur::NK_CAPSULE:
						c.forme = NkForme2D::NK_CAPSULE;
						if (demi.x >= demi.y) {
							c.rayon = demi.y;
							c.demiTaille = NkVec2f(math::NkMax(0.f, demi.x - demi.y), 0.f);
						} else {
							c.rayon = demi.x;
							c.demiTaille = NkVec2f(math::NkMax(0.f, demi.y - demi.x), 0.f);
							c.rotation = 1.5707963f;
						}
						break;
					case NkCollisionEditeur::NK_POLYGONE: {
						// Un pentagone inscrit dans la boite : de quoi tirer ses sommets.
						c.forme = NkForme2D::NK_POLYGONE;
						c.nbSommets = 5;
						c.boucle = true;
						for (uint32 i = 0; i < 5u; ++i) {
							const float32 a = 1.5707963f + 6.2831853f * static_cast<float32>(i) / 5.f;
							c.sommets[i] = NkVec2f(demi.x * std::cos(a), demi.y * std::sin(a));
						}
						break;
					}
					default:
						c.forme = NkForme2D::NK_BOITE;
						c.demiTaille = demi;
						break;
				}
				return c;
			}

			/// La demi-boite VISIBLE de l'entite : sa forme, son sprite, sinon 0,5.
			NkVec2f DemiVisible(NkScene &s, ecs::NkEntityId id) {
				if (const NkRenduForme2D *f = s.Monde().Get<NkRenduForme2D>(id)) {
					if (f->visible) {
						return NkDemiBoiteForme2D(*f);
					}
				}
				if (const NkSprite2D *sp = s.Monde().Get<NkSprite2D>(id)) {
					return NkVec2f(math::NkAbs(sp->taille.x) * 0.5f, math::NkAbs(sp->taille.y) * 0.5f);
				}
				if (const NkCollisionneur2D *c = s.Monde().Get<NkCollisionneur2D>(id)) {
					if (c->forme == NkForme2D::NK_BOITE) {
						return c->demiTaille;
					}
					if (c->forme == NkForme2D::NK_CERCLE) {
						return NkVec2f(c->rayon, c->rayon);
					}
				}
				return NkVec2f(0.5f, 0.5f);
			}

			ecs::NkEntityId PoserVolume(NkEditeurModele &m, NkCollisionEditeur k, const NkVec2f &monde, bool declencheur) {
				NkEditeurRetenir(m);
				NkScene &s = m.scene;
				const ecs::NkEntityId e = s.Creer(NomLibre(s, declencheur ? "Déclencheur" : "Volume bloquant").CStr(), monde);
				MarquerInvisible(s, e);
				NkCollisionneur2D c = CollisionneurDeBase(k, NkVec2f(0.5f, 0.5f));
				c.declencheur = declencheur;
				s.Monde().Add<NkCollisionneur2D>(e, c);
				NkCorps2D b;
				b.type = NkTypeCorps::NK_STATIQUE;
				s.AjouterCorps(e, b);
				Choisir(m, e);
				return e;
			}

			// =================================================================
			// LES ICONES (tracees : la police n'a pas ces glyphes)
			// =================================================================
			void IconeOnglet(nkgui::NkGuiDrawList &dl, NkOngletPlacer o, float32 cx, float32 cy, const NkColor &col) {
				auto P = [](float32 x, float32 y) { return NkVec2{x, y}; };
				switch (o) {
					case NkOngletPlacer::NK_FAVORIS: {
						NkVec2 pts[10];
						for (int32 k = 0; k < 10; ++k) {
							const float32 a = -1.5707963f + 3.14159265f * static_cast<float32>(k) / 5.f;
							const float32 r = (k % 2 == 0) ? 9.f : 4.f;
							pts[k] = P(cx + r * std::cos(a), cy + r * std::sin(a));
						}
						for (int32 k = 0; k < 10; ++k) {
							dl.AddTriangleFilled(P(cx, cy), pts[k], pts[(k + 1) % 10], col);
						}
						break;
					}
					case NkOngletPlacer::NK_RECENTS:
						dl.AddCircle(P(cx, cy), 8.f, col, 1.6f);
						dl.AddLine(P(cx, cy), P(cx, cy - 5.5f), col, 1.6f);
						dl.AddLine(P(cx, cy), P(cx + 4.f, cy + 2.f), col, 1.6f);
						break;
					case NkOngletPlacer::NK_BASE:
						// Un petit bonhomme (un acteur) : tete et epaules.
						dl.AddCircleFilled(P(cx, cy - 4.f), 4.f, col);
						dl.AddTriangleFilled(P(cx - 7.f, cy + 9.f), P(cx + 7.f, cy + 9.f), P(cx, cy + 1.f), col);
						break;
					case NkOngletPlacer::NK_LUMIERES:
						dl.AddCircleFilled(P(cx, cy - 2.f), 6.f, col);
						dl.AddRectFilled(NkRect{cx - 3.f, cy + 5.f, 6.f, 4.f}, col);
						break;
					case NkOngletPlacer::NK_FORMES:
						dl.AddCircleFilled(P(cx - 3.f, cy - 2.f), 5.f, col);
						dl.AddTriangleFilled(P(cx + 1.f, cy + 9.f), P(cx + 10.f, cy + 9.f), P(cx + 5.5f, cy), col);
						dl.AddRect(NkRect{cx - 9.f, cy + 2.f, 8.f, 7.f}, col, 1.4f);
						break;
					case NkOngletPlacer::NK_EFFETS: {
						const NkVec2 f[4] = {P(cx, cy - 9.f), P(cx + 6.f, cy + 2.f), P(cx, cy + 9.f), P(cx - 6.f, cy + 2.f)};
						dl.AddTriangleFilled(f[0], f[1], f[2], col);
						dl.AddTriangleFilled(f[0], f[2], f[3], col);
						break;
					}
					case NkOngletPlacer::NK_VOLUMES:
						for (int32 k = 0; k < 4; ++k) {
							const float32 t = -8.f + 4.5f * static_cast<float32>(k);
							dl.AddLine(P(cx + t, cy - 8.f), P(cx + t + 2.5f, cy - 8.f), col, 1.5f);
							dl.AddLine(P(cx + t, cy + 8.f), P(cx + t + 2.5f, cy + 8.f), col, 1.5f);
							dl.AddLine(P(cx - 8.f, cy + t), P(cx - 8.f, cy + t + 2.5f), col, 1.5f);
							dl.AddLine(P(cx + 8.f, cy + t), P(cx + 8.f, cy + t + 2.5f), col, 1.5f);
						}
						break;
					default:
						for (int32 i = 0; i < 2; ++i) {
							for (int32 j = 0; j < 2; ++j) {
								dl.AddRectFilled(NkRect{cx - 8.f + 9.f * static_cast<float32>(i), cy - 8.f + 9.f * static_cast<float32>(j), 7.f, 7.f}, col, 1.f);
							}
						}
						break;
				}
			}

			/// L'icone d'un element, dans le carre `r`.
			void IconeElement(nkgui::NkGuiDrawList &dl, const NkElementPlacer &el, const NkRect &r, const NkColor &texte) {
				const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
				auto P = [](float32 x, float32 y) { return NkVec2{x, y}; };
				switch (el.nature) {
					case NkNaturePlacer::NK_FORME:
					case NkNaturePlacer::NK_CARRE: {
						NkRenduForme2D f = NkFormeParDefaut(static_cast<NkGenreForme2D>(el.indice));
						if (el.nature == NkNaturePlacer::NK_CARRE) {
							f.taille = NkVec2f(1.f, 1.f);
						}
						f.epaisseurContour = 0.f;
						if (f.genre == NkGenreForme2D::NK_LIGNE) {
							f.epaisseur = 0.25f;
						}
						NkDessinerFormeVignette(dl, f, NkRect{r.x + 2.f, r.y + 2.f, r.w - 4.f, r.h - 4.f});
						break;
					}
					case NkNaturePlacer::NK_SIM: {
						const NkInfoActeurSim &info = NkActeurSimInfo(static_cast<NkActeurSim>(el.indice));
						const NkColor c(info.couleur); // 0xRRGGBBAA (math::NkColor)
						if (info.rigide && el.indice == static_cast<int32>(NkActeurSim::NK_CAISSE)) {
							dl.AddRectFilled(NkRect{cx - 8.f, cy - 8.f, 16.f, 16.f}, c, 2.f);
						} else {
							dl.AddCircleFilled(P(cx, cy + 1.f), 8.f, c);
							dl.AddCircleFilled(P(cx + 4.f, cy - 4.f), 4.f, c);
						}
						break;
					}
					case NkNaturePlacer::NK_LUMIERE:
						for (int32 k = 0; k < 8; ++k) {
							const float32 a = 0.785398f * static_cast<float32>(k);
							dl.AddLine(P(cx + std::cos(a) * 6.f, cy + std::sin(a) * 6.f), P(cx + std::cos(a) * 10.f, cy + std::sin(a) * 10.f),
									   NkColor{250, 214, 120, 255}, 1.5f);
						}
						dl.AddCircleFilled(P(cx, cy), 4.5f, NkColor{255, 226, 150, 255});
						break;
					case NkNaturePlacer::NK_EFFET: {
						const NkVec2 f[4] = {P(cx, cy - 10.f), P(cx + 7.f, cy + 2.f), P(cx, cy + 10.f), P(cx - 7.f, cy + 2.f)};
						dl.AddTriangleFilled(f[0], f[1], f[2], NkColor{255, 130, 50, 255});
						dl.AddTriangleFilled(f[0], f[2], f[3], NkColor{255, 130, 50, 255});
						dl.AddCircleFilled(P(cx, cy + 4.f), 3.5f, NkColor{255, 220, 120, 255});
						break;
					}
					case NkNaturePlacer::NK_VOLUME:
					case NkNaturePlacer::NK_DECLENCHEUR: {
						const NkColor v = el.nature == NkNaturePlacer::NK_DECLENCHEUR ? NkColor{240, 190, 60, 255} : NkColor{0, 224, 122, 255};
						for (int32 k = 0; k < 4; ++k) {
							const float32 t = -9.f + 5.f * static_cast<float32>(k);
							dl.AddLine(P(cx + t, cy - 9.f), P(cx + t + 3.f, cy - 9.f), v, 1.5f);
							dl.AddLine(P(cx + t, cy + 9.f), P(cx + t + 3.f, cy + 9.f), v, 1.5f);
							dl.AddLine(P(cx - 9.f, cy + t), P(cx - 9.f, cy + t + 3.f), v, 1.5f);
							dl.AddLine(P(cx + 9.f, cy + t), P(cx + 9.f, cy + t + 3.f), v, 1.5f);
						}
						break;
					}
					case NkNaturePlacer::NK_PERSONNAGE:
						dl.AddCircleFilled(P(cx, cy - 6.f), 4.f, NkColor{110, 200, 255, 255});
						dl.AddRectFilled(NkRect{cx - 4.f, cy - 2.f, 8.f, 12.f}, NkColor{110, 200, 255, 255}, 4.f);
						break;
					case NkNaturePlacer::NK_SPRITE:
						dl.AddRect(NkRect{cx - 9.f, cy - 8.f, 18.f, 16.f}, texte, 1.3f);
						dl.AddTriangleFilled(P(cx - 7.f, cy + 6.f), P(cx - 1.f, cy - 1.f), P(cx + 4.f, cy + 6.f), texte);
						dl.AddCircleFilled(P(cx + 4.f, cy - 3.f), 2.f, texte);
						break;
					case NkNaturePlacer::NK_SON:
						dl.AddRectFilled(NkRect{cx - 8.f, cy - 3.f, 4.f, 6.f}, texte);
						dl.AddTriangleFilled(P(cx - 4.f, cy - 3.f), P(cx + 1.f, cy - 8.f), P(cx + 1.f, cy + 8.f), texte);
						dl.AddTriangleFilled(P(cx - 4.f, cy - 3.f), P(cx + 1.f, cy + 8.f), P(cx - 4.f, cy + 3.f), texte);
						dl.AddLine(P(cx + 4.f, cy - 4.f), P(cx + 4.f, cy + 4.f), texte, 1.3f);
						dl.AddLine(P(cx + 7.f, cy - 7.f), P(cx + 7.f, cy + 7.f), texte, 1.3f);
						break;
					default: {
						// L'acteur vide : le losange des entites sans visuel de la vue.
						const NkVec2 l[4] = {P(cx, cy - 8.f), P(cx + 8.f, cy), P(cx, cy + 8.f), P(cx - 8.f, cy)};
						dl.AddPolyline(l, 4, texte, 1.5f, true);
						break;
					}
				}
			}

			/// Le filtre de l'onglet `o` sur l'element `k`.
			bool DansOnglet(const NkEditeurInterface &ui, NkOngletPlacer o, int32 k, const NkElementPlacer &el) {
				if (o == NkOngletPlacer::NK_FAVORIS) {
					return k < 64 && (ui.placerFavoris & (1ull << static_cast<uint64>(k))) != 0u;
				}
				if (o == NkOngletPlacer::NK_RECENTS) {
					for (uint32 i = 0; i < ui.placerRecents.Size(); ++i) {
						if (ui.placerRecents[i] == k) {
							return true;
						}
					}
					return false;
				}
				return (el.onglets & Bit(o)) != 0u;
			}
		} // namespace

		// =====================================================================
		const char *NkNomOngletPlacer(NkOngletPlacer o) noexcept {
			switch (o) {
				case NkOngletPlacer::NK_FAVORIS: return "Favoris";
				case NkOngletPlacer::NK_RECENTS: return "Récents";
				case NkOngletPlacer::NK_BASE: return "Base";
				case NkOngletPlacer::NK_LUMIERES: return "Lumières";
				case NkOngletPlacer::NK_FORMES: return "Formes";
				case NkOngletPlacer::NK_EFFETS: return "Effets";
				case NkOngletPlacer::NK_VOLUMES: return "Volumes";
				default: return "Tout";
			}
		}

		const char *NkNomCollisionEditeur(NkCollisionEditeur k) noexcept {
			switch (k) {
				case NkCollisionEditeur::NK_BOITE: return "Boîte";
				case NkCollisionEditeur::NK_CERCLE: return "Cercle";
				case NkCollisionEditeur::NK_CAPSULE: return "Capsule";
				case NkCollisionEditeur::NK_POLYGONE: return "Polygone";
				case NkCollisionEditeur::NK_DEPUIS_FORME: return "Depuis la forme";
				case NkCollisionEditeur::NK_DEPUIS_SPRITE: return "Depuis le contour du sprite";
				default: return "Collisionneur";
			}
		}

		const NkElementPlacer *NkEditeurCataloguePlacer(int32 &nombre) noexcept {
			const NkCatalogue &c = Catalogue();
			nombre = c.n;
			return c.e;
		}

		// =====================================================================
		// POSER
		// =====================================================================
		ecs::NkEntityId NkEditeurPoserForme(NkEditeurModele &m, NkGenreForme2D g, const NkVec2f &monde, bool carre) {
			NkEditeurRetenir(m);
			NkRenduForme2D f = NkFormeParDefaut(g);
			if (carre) {
				f.taille = NkVec2f(1.f, 1.f);
			}
			// Un DECOR SOLIDE par defaut (un corps statique), comme un maillage
			// pose dans UE5 a sa collision : ce qui tombe dessus s'y arrete. Les
			// Details le rendent dynamique (Corps > Type) ou sans collision.
			const NkString nom = NomLibre(m.scene, carre ? "Carré" : NkNomGenreForme2D(g));
			const ecs::NkEntityId e = NkPoserForme2D(m.scene, f, monde, nom.CStr(), true,
													 m.scene.PhysiqueActive() ? static_cast<int32>(NkTypeCorps::NK_STATIQUE) : -1);
			Choisir(m, e);
			NkEditeurAnnoncer(m, NkString::Format("%s posé(e) — Détails > Forme 2D pour la régler", nom.CStr()).CStr());
			return e;
		}

		bool NkEditeurAjouterForme(NkEditeurModele &m, ecs::NkEntityId id, NkGenreForme2D g) {
			if (!m.scene.Monde().IsAlive(id)) {
				return false;
			}
			NkEditeurRetenir(m);
			NkRenduForme2D f = NkFormeParDefaut(g);
			// Une forme sur un sprite prend sa taille : elle le remplace a l'oeil.
			if (const NkSprite2D *sp = m.scene.Monde().Get<NkSprite2D>(id)) {
				f.taille = NkVec2f(math::NkAbs(sp->taille.x), math::NkAbs(sp->taille.y));
			}
			// Un collisionneur DEJA la n'est pas remplace : la forme le suit si on
			// le demande (Details > Collision > Depuis la forme).
			f.collisionSuit = !m.scene.Monde().Has<NkCollisionneur2D>(id);
			if (m.scene.Monde().Has<NkRenduForme2D>(id)) {
				m.scene.Monde().Set<NkRenduForme2D>(id, f);
			} else {
				m.scene.Monde().Add<NkRenduForme2D>(id, f);
			}
			NkEditeurAnnoncer(m, NkString::Format("Forme 2D ajoutée : %s", NkNomGenreForme2D(g)).CStr());
			return true;
		}

		bool NkEditeurAjouterCollision(NkEditeurModele &m, ecs::NkEntityId id, NkCollisionEditeur k) {
			NkScene &s = m.scene;
			if (!s.Monde().IsAlive(id) || s.Monde().Has<NkCorpsMou2D>(id)) {
				NkEditeurAnnoncer(m, "Pas de collisionneur rigide sur de la matière (elle touche déjà tout)");
				return false;
			}
			NkCollisionneur2D c;
			if (const NkCollisionneur2D *avant = s.Monde().Get<NkCollisionneur2D>(id)) {
				c = *avant; // couche, masque, declencheur : gardes
			}
			const NkCorps2D *corps = s.Monde().Get<NkCorps2D>(id);
			const bool dynamique = corps != nullptr && corps->type == NkTypeCorps::NK_DYNAMIQUE;
			if (k == NkCollisionEditeur::NK_DEPUIS_FORME) {
				const NkRenduForme2D *f = s.Monde().Get<NkRenduForme2D>(id);
				if (f == nullptr) {
					NkEditeurAnnoncer(m, "Pas de forme 2D sur cette entité : rien à suivre");
					return false;
				}
				NkEditeurRetenir(m);
				const NkRenduForme2D copie = *f;
				NkCollisionneurDepuisForme(copie, dynamique, c);
			} else if (k == NkCollisionEditeur::NK_DEPUIS_SPRITE) {
				const NkSprite2D *sp = s.Monde().Get<NkSprite2D>(id);
				int32 w = 0, h = 0;
				const uint8 *px = sp != nullptr && sp->texId != 0u ? m.textures.Pixels(sp->texId) : nullptr;
				if (px == nullptr || !m.textures.Taille(sp->texId, w, h)) {
					NkEditeurAnnoncer(m, "Pas d'image (sprite texturé) sur cette entité : pas de contour à suivre");
					return false;
				}
				NkEditeurRetenir(m);
				const NkSprite2D copie = *sp;
				if (!NkCollisionneurDepuisSprite(copie, px, w, h, c)) {
					NkEditeurAnnoncer(m, "Image entièrement transparente : pas de contour");
					return false;
				}
			} else {
				NkEditeurRetenir(m);
				const NkCollisionneur2D base = CollisionneurDeBase(k, DemiVisible(s, id));
				c.forme = base.forme;
				c.demiTaille = base.demiTaille;
				c.rayon = base.rayon;
				c.rotation = base.rotation;
				c.nbSommets = base.nbSommets;
				c.boucle = base.boucle;
				for (uint32 i = 0; i < NK_COLLISION_SOMMETS_MAX; ++i) {
					c.sommets[i] = base.sommets[i];
				}
				c.decalage = NkVec2f(0.f, 0.f);
			}
			if (s.Monde().Has<NkCollisionneur2D>(id)) {
				s.Monde().Set<NkCollisionneur2D>(id, c);
			} else {
				s.Monde().Add<NkCollisionneur2D>(id, c);
			}
			// Regle a la main : la forme ne le refait plus (sauf « depuis la forme »).
			if (NkRenduForme2D *f = s.Monde().Get<NkRenduForme2D>(id)) {
				f->collisionSuit = k == NkCollisionEditeur::NK_DEPUIS_FORME;
			}
			if (s.Monde().Has<NkCorps2D>(id)) {
				s.ActualiserCorps(id);
			}
			NkEditeurAnnoncer(m, NkString::Format("Collisionneur : %s%s", NkNomCollisionEditeur(k),
												  s.Monde().Has<NkCorps2D>(id) ? "" : " (sans corps : Détails > Collision pour le rendre solide)")
									 .CStr());
			return true;
		}

		void NkEditeurFormeChangee(NkEditeurModele &m, ecs::NkEntityId id) {
			const NkRenduForme2D *f = m.scene.Monde().Get<NkRenduForme2D>(id);
			if (f != nullptr && f->collisionSuit && m.scene.Monde().Has<NkCollisionneur2D>(id)) {
				NkRefaireCollisionneurForme(m.scene, id);
			}
		}

		ecs::NkEntityId NkEditeurPoserElement(NkEditeurModele &m, NkEditeurInterface *ui, int32 k, const NkVec2f &monde) {
			int32 n = 0;
			const NkElementPlacer *cat = NkEditeurCataloguePlacer(n);
			if (k < 0 || k >= n) {
				return ecs::NkEntityId::Invalid();
			}
			const NkElementPlacer &el = cat[k];
			NkScene &s = m.scene;
			ecs::NkEntityId e = ecs::NkEntityId::Invalid();
			switch (el.nature) {
				case NkNaturePlacer::NK_FORME:
				case NkNaturePlacer::NK_CARRE:
					e = NkEditeurPoserForme(m, static_cast<NkGenreForme2D>(el.indice), monde, el.nature == NkNaturePlacer::NK_CARRE);
					break;
				case NkNaturePlacer::NK_VIDE:
					NkEditeurRetenir(m);
					e = NkEditeurCreerEntite(m, NomLibre(s, "Acteur").CStr(), monde);
					break;
				case NkNaturePlacer::NK_SPRITE:
				case NkNaturePlacer::NK_SIM: {
					// Le geste « Poser » de toujours, sans changer l'outil arme.
					NkEditeurRetenir(m);
					const NkActeurSim acteurArme = m.acteur;
					const bool simpleArme = m.acteurSimple;
					m.acteurSimple = el.nature == NkNaturePlacer::NK_SPRITE;
					if (el.nature == NkNaturePlacer::NK_SIM) {
						m.acteur = static_cast<NkActeurSim>(el.indice);
					}
					e = NkEditeurPoser(m, monde);
					m.acteur = acteurArme;
					m.acteurSimple = simpleArme;
					break;
				}
				case NkNaturePlacer::NK_PERSONNAGE: {
					NkEditeurRetenir(m);
					NkRenduForme2D f = NkFormeParDefaut(NkGenreForme2D::NK_CAPSULE);
					f.taille = NkVec2f(0.6f, 1.4f); // debout : la capsule suit le grand axe
					f.remplissage = 0x6EC8FFFFu;
					f.epaisseurContour = 0.04f;
					f.couche = 5;
					e = NkPoserForme2D(s, f, monde, NomLibre(s, "Personnage").CStr(), true, -1);
					NkCorps2D b;
					b.type = NkTypeCorps::NK_DYNAMIQUE;
					b.masse = 70.f;
					b.rotationBloquee = true; // un personnage de plateforme ne bascule pas
					b.friction = 0.2f;
					if (s.PhysiqueActive()) {
						s.AjouterCorps(e, b);
					}
					// Le controleur de personnage d'Unkeny (NkUnkenyControles.h) : les
					// actions du joueur 1 en Jouer (fleches / ZQSD, Espace).
					s.Monde().Add<NkControleRigide2D>(e, NkControleRigide2D());
					Choisir(m, e);
					break;
				}
				case NkNaturePlacer::NK_SON:
					NkEditeurRetenir(m);
					e = NkEditeurCreerEntite(m, NomLibre(s, "Son").CStr(), monde);
					NkEditeurAjouterComposant(m, e, NkComposantEditeur::NK_SOURCE);
					break;
				case NkNaturePlacer::NK_DECLENCHEUR:
					e = PoserVolume(m, NkCollisionEditeur::NK_BOITE, monde, true);
					break;
				case NkNaturePlacer::NK_VOLUME:
					e = PoserVolume(m, static_cast<NkCollisionEditeur>(el.indice), monde, false);
					break;
				case NkNaturePlacer::NK_LUMIERE:
					NkEditeurRetenir(m);
					e = NkEditeurPoserLumiere(m, monde, static_cast<NkTypeLumiere2D>(el.indice));
					break;
				case NkNaturePlacer::NK_EFFET:
					NkEditeurRetenir(m);
					e = NkEditeurPoserEffet(m, monde, static_cast<NkPresetEffet2D>(el.indice));
					break;
			}
			if (!e.IsValid()) {
				return e;
			}
			Choisir(m, e);
			if (ui != nullptr) {
				// Les Recents : le plus recent en tete, sans doublon.
				for (uint32 i = 0; i < ui->placerRecents.Size(); ++i) {
					if (ui->placerRecents[i] == k) {
						ui->placerRecents.RemoveAt(i);
						break;
					}
				}
				ui->placerRecents.Insert(ui->placerRecents.Begin(), k);
				while (ui->placerRecents.Size() > RECENTS_MAX) {
					ui->placerRecents.RemoveAt(ui->placerRecents.Size() - 1u);
				}
			}
			if (el.nature != NkNaturePlacer::NK_FORME && el.nature != NkNaturePlacer::NK_CARRE) {
				NkEditeurAnnoncer(m, NkString::Format("« %s » posé", el.nom).CStr());
			}
			return e;
		}

		// =====================================================================
		// LES MENUS ET LES ACTIONS
		// =====================================================================
		namespace {
			NkEntreeMenu Entree(const char *libelle, int32 action, bool actif = true) {
				NkEntreeMenu e;
				e.libelle = NkString(libelle);
				e.action = action;
				e.actif = actif;
				return e;
			}
			NkEntreeMenu Titre(const char *libelle) {
				NkEntreeMenu e;
				e.libelle = NkString(libelle);
				e.actif = false;
				return e;
			}
			NkEntreeMenu Trait() {
				NkEntreeMenu e;
				e.separateur = true;
				return e;
			}
		} // namespace

		void NkEditeurMenuFormes(NkVector<NkEntreeMenu> &out, int32 base) {
			out.PushBack(Trait());
			out.PushBack(Titre("Formes 2D"));
			for (int32 g = 0; g < static_cast<int32>(NkGenreForme2D::NK_COUNT); ++g) {
				out.PushBack(Entree(NkNomGenreForme2D(static_cast<NkGenreForme2D>(g)), base + g));
			}
		}

		void NkEditeurMenuVolumes(NkVector<NkEntreeMenu> &out, int32 base) {
			out.PushBack(Trait());
			out.PushBack(Titre("Volumes et collision"));
			for (int32 k = 0; k <= static_cast<int32>(NkCollisionEditeur::NK_POLYGONE); ++k) {
				out.PushBack(Entree(NkString::Format("Volume bloquant (%s)", NkNomCollisionEditeur(static_cast<NkCollisionEditeur>(k))).CStr(),
									base + k));
			}
			out.PushBack(Entree("Déclencheur (zone)", NK_A_DECLENCHEUR_ICI));
		}

		void NkEditeurMenuCollisions(NkEditeurCadre &c, NkVector<NkEntreeMenu> &out) {
			NkEditeurModele &m = c.m;
			if (!m.aSelection || !m.scene.Monde().IsAlive(m.selection) || m.scene.Monde().Has<NkCorpsMou2D>(m.selection)) {
				return;
			}
			ecs::NkWorld &w = m.scene.Monde();
			const ecs::NkEntityId id = m.selection;
			out.PushBack(Trait());
			out.PushBack(Titre(w.Has<NkCollisionneur2D>(id) ? "Collisionneur (remplace l'actuel)" : "Collisionneur"));
			for (int32 k = 0; k < static_cast<int32>(NkCollisionEditeur::NK_COUNT); ++k) {
				const NkCollisionEditeur ke = static_cast<NkCollisionEditeur>(k);
				if (ke == NkCollisionEditeur::NK_DEPUIS_FORME && !w.Has<NkRenduForme2D>(id)) {
					continue;
				}
				if (ke == NkCollisionEditeur::NK_DEPUIS_SPRITE) {
					const NkSprite2D *sp = w.Get<NkSprite2D>(id);
					if (sp == nullptr || sp->texId == 0u) {
						continue;
					}
				}
				out.PushBack(Entree(NkNomCollisionEditeur(ke), NK_A_COLLISION + k));
			}
			if (!w.Has<NkRenduForme2D>(id)) {
				NkEditeurMenuFormes(out, NK_A_FORME_SELECTION);
			}
		}

		bool NkEditeurActionPlacer(NkEditeurCadre &c, int32 action) {
			if (action < NK_A_FORME || action > NK_A_FORME + 99) {
				return false;
			}
			NkEditeurModele &m = c.m;
			NkEditeurInterface &ui = c.ui;
			const int32 nbGenres = static_cast<int32>(NkGenreForme2D::NK_COUNT);
			const NkVec2f centre = m.scene.Camera().Centre();
			if (action >= NK_A_FORME && action < NK_A_FORME + nbGenres) {
				NkEditeurPoserForme(m, static_cast<NkGenreForme2D>(action - NK_A_FORME), centre);
			} else if (action >= NK_A_FORME_ICI && action < NK_A_FORME_ICI + nbGenres) {
				NkEditeurPoserForme(m, static_cast<NkGenreForme2D>(action - NK_A_FORME_ICI), ui.pointContexte);
			} else if (action >= NK_A_FORME_SELECTION && action < NK_A_FORME_SELECTION + nbGenres) {
				if (m.aSelection) {
					NkEditeurAjouterForme(m, m.selection, static_cast<NkGenreForme2D>(action - NK_A_FORME_SELECTION));
				}
			} else if (action >= NK_A_COLLISION && action < NK_A_COLLISION + static_cast<int32>(NkCollisionEditeur::NK_COUNT)) {
				if (m.aSelection) {
					NkEditeurAjouterCollision(m, m.selection, static_cast<NkCollisionEditeur>(action - NK_A_COLLISION));
				}
			} else if (action >= NK_A_VOLUME_ICI && action < NK_A_DECLENCHEUR_ICI) {
				PoserVolume(m, static_cast<NkCollisionEditeur>(action - NK_A_VOLUME_ICI), ui.pointContexte, false);
			} else if (action == NK_A_DECLENCHEUR_ICI) {
				PoserVolume(m, NkCollisionEditeur::NK_BOITE, ui.pointContexte, true);
			} else if (action == NK_A_VOIR_PLACER) {
				ui.voirPlacer = !ui.voirPlacer;
			} else if (action == NK_A_EDITER_COLLISION) {
				ui.editionCollision = !ui.editionCollision;
				ui.poigneeTenue = -1;
				if (ui.editionCollision && !(m.aSelection && m.scene.Monde().Has<NkCollisionneur2D>(m.selection))) {
					NkEditeurAnnoncer(m, "Édition de la collision : choisissez une entité qui a un collisionneur");
				}
			} else if (action >= NK_A_PLACER) {
				NkEditeurPoserElement(m, &ui, action - NK_A_PLACER, centre);
			}
			return true;
		}

		// =====================================================================
		// LE PANNEAU
		// =====================================================================
		void NkEditeurDessinerPlacer(NkEditeurCadre &c) {
			NkEditeurInterface &ui = c.ui;
			const NkRect zone = ui.placer;
			if (!ui.voirPlacer || zone.w < 40.f || zone.h < 80.f) {
				ui.placerAppui = -1;
				ui.placerGlisse = false;
				return;
			}
			nkgui::NkGuiContext &ctx = c.ctx;
			const nkgui::NkGuiInput &in = ctx.input;
			auto &dl = ctx.dl;
			dl.AddRectFilled(zone, c.pal.panneau);

			// ── L'onglet du panneau (« Place Actors » d'UE5) ────────────────
			static const char *kTitre[1] = {"Placer des acteurs"};
			int32 seul = 0;
			const float32 titreH = 26.f;
			NkEditeurOnglets(c, NkRect{zone.x, zone.y, zone.w, titreH}, kTitre, 1, seul);

			// ── La recherche ────────────────────────────────────────────────
			const NkRect recherche{zone.x + 6.f, zone.y + titreH + 5.f, zone.w - 12.f, 22.f};
			if (in.mouseClicked[0]) {
				ui.placerFiltreFocus = NkEditeurDans(recherche, in.mousePos);
			}
			if (ui.placerFiltreFocus && in.KeyPressed(nkgui::NkGuiKey::Escape)) {
				ui.placerFiltre[0] = '\0';
				ui.placerFiltreFocus = false;
			}
			dl.AddRectFilled(recherche, c.pal.champ, 3.f);
			dl.AddRect(recherche, ui.placerFiltreFocus ? c.pal.accent : c.pal.bord, 1.f, 3.f);
			// La loupe.
			dl.AddCircle(NkVec2{recherche.x + 11.f, recherche.y + 10.f}, 4.5f, c.pal.attenue, 1.4f);
			dl.AddLine(NkVec2{recherche.x + 14.f, recherche.y + 13.f}, NkVec2{recherche.x + 18.f, recherche.y + 17.f}, c.pal.attenue, 1.6f);
			{
				editorkit::NkOverlayFieldStyle st;
				st.fond = false;
				st.bord = false;
				st.texte = c.pal.texte;
				const NkRect champ{recherche.x + 22.f, recherche.y, recherche.w - 26.f, recherche.h};
				editorkit::NkOverlayTextField(ctx, dl, c.petite, champ, ui.placerFiltre, static_cast<int32>(sizeof(ui.placerFiltre)),
											  ui.placerFiltreFocus, &st);
				if (ui.placerFiltre[0] == '\0' && !ui.placerFiltreFocus) {
					const float32 ty = recherche.y + (recherche.h - renderer::NkTexteHauteurLigne(c.petite, 12.f)) * 0.5f;
					renderer::NkTexte(dl, c.petite, champ.x + 2.f, ty, "Rechercher des acteurs", c.pal.attenue);
				}
			}

			const float32 haut = recherche.y + recherche.h + 6.f;
			const float32 aideH = 34.f;
			const NkRect corps{zone.x, haut, zone.w, zone.y + zone.h - haut - aideH};

			// ── Les onglets verticaux ───────────────────────────────────────
			const int32 nbOnglets = static_cast<int32>(NkOngletPlacer::NK_COUNT);
			const NkRect colonne{corps.x, corps.y, ONGLETS_L, corps.h};
			dl.AddRectFilled(colonne, c.pal.fond);
			// Les huit onglets tiennent TOUJOURS dans la colonne : ils se tassent
			// (et perdent leur libelle sous 40 px), jamais ne disparaissent.
			float32 ongletH = (colonne.h - 8.f) / static_cast<float32>(nbOnglets);
			ongletH = ongletH > ONGLET_H ? ONGLET_H : (ongletH < 22.f ? 22.f : ongletH);
			const bool libelles = ongletH >= 40.f;
			for (int32 o = 0; o < nbOnglets; ++o) {
				const NkRect r{colonne.x + 3.f, colonne.y + 4.f + static_cast<float32>(o) * ongletH, ONGLETS_L - 6.f, ongletH - 4.f};
				ui.placerOngletsRects[o] = r;
				const bool actif = ui.placerOnglet == o;
				const bool survol = NkEditeurDans(r, in.mousePos);
				if (actif) {
					dl.AddRectFilled(r, c.pal.accent, 4.f);
				} else if (survol) {
					dl.AddRectFilled(r, c.pal.boutonSurvol, 4.f);
				}
				const NkColor col = actif ? c.pal.surAccent : c.pal.texte;
				IconeOnglet(dl, static_cast<NkOngletPlacer>(o), r.x + r.w * 0.5f, libelles ? r.y + 16.f : r.y + r.h * 0.5f, col);
				if (libelles) {
					renderer::NkTexteCentre(dl, c.petite, r.x + r.w * 0.5f, r.y + 30.f, NkNomOngletPlacer(static_cast<NkOngletPlacer>(o)), col);
				}
				if (survol && in.mouseClicked[0]) {
					ui.placerOnglet = o;
					ui.placerDefil = 0.f;
				}
			}

			// ── La liste ────────────────────────────────────────────────────
			int32 n = 0;
			const NkElementPlacer *cat = NkEditeurCataloguePlacer(n);
			ui.placerRects.Resize(static_cast<usize>(n));
			for (int32 k = 0; k < n; ++k) {
				ui.placerRects[static_cast<uint32>(k)] = NkRect{0.f, 0.f, 0.f, 0.f};
			}
			const NkRect liste{colonne.x + colonne.w + 4.f, corps.y + 2.f, corps.w - colonne.w - 8.f, corps.h - 4.f};
			const NkOngletPlacer onglet = static_cast<NkOngletPlacer>(ui.placerOnglet);
			// L'ordre : celui du catalogue, ou celui des Recents.
			NkVector<int32> ordre;
			if (onglet == NkOngletPlacer::NK_RECENTS) {
				for (uint32 i = 0; i < ui.placerRecents.Size(); ++i) {
					if (ui.placerRecents[i] >= 0 && ui.placerRecents[i] < n && Correspond(cat[ui.placerRecents[i]].nom, ui.placerFiltre)) {
						ordre.PushBack(ui.placerRecents[i]);
					}
				}
			} else {
				// Une recherche cherche dans TOUT, comme celle d'UE5.
				const NkOngletPlacer o = ui.placerFiltre[0] != '\0' ? NkOngletPlacer::NK_TOUT : onglet;
				for (int32 k = 0; k < n; ++k) {
					if (DansOnglet(ui, o, k, cat[k]) && Correspond(cat[k].nom, ui.placerFiltre)) {
						ordre.PushBack(k);
					}
				}
			}
			const float32 total = static_cast<float32>(ordre.Size()) * RANGEE_H;
			if (NkEditeurDans(liste, in.mousePos) && in.wheel != 0.f) {
				ui.placerDefil -= in.wheel * RANGEE_H * 2.f;
			}
			const float32 defilMax = total - liste.h > 0.f ? total - liste.h : 0.f;
			ui.placerDefil = ui.placerDefil < 0.f ? 0.f : (ui.placerDefil > defilMax ? defilMax : ui.placerDefil);
			dl.PushClipRect(liste, true);
			const int32 enCours = ui.placerAppui;
			for (uint32 i = 0; i < ordre.Size(); ++i) {
				const int32 k = ordre[i];
				const NkElementPlacer &el = cat[k];
				const NkRect r{liste.x, liste.y + static_cast<float32>(i) * RANGEE_H - ui.placerDefil, liste.w, RANGEE_H - 2.f};
				if (r.y + r.h < liste.y || r.y > liste.y + liste.h) {
					continue;
				}
				ui.placerRects[static_cast<uint32>(k)] = r;
				const bool survol = NkEditeurDans(r, in.mousePos) && NkEditeurDans(liste, in.mousePos);
				const NkRect etoile{r.x + r.w - 20.f, r.y + (r.h - 16.f) * 0.5f, 16.f, 16.f};
				const bool favori = k < 64 && (ui.placerFavoris & (1ull << static_cast<uint64>(k))) != 0u;
				dl.AddRectFilled(r, enCours == k ? c.pal.boutonSurvol : (survol ? c.pal.entete : c.pal.bouton), 3.f);
				const NkRect icone{r.x + 4.f, r.y + 3.f, r.h - 6.f, r.h - 6.f};
				dl.AddRectFilled(icone, c.pal.fond, 3.f);
				IconeElement(dl, el, icone, c.pal.texte);
				const float32 ty = r.y + (r.h - renderer::NkTexteHauteurLigne(c.police, 16.f)) * 0.5f;
				dl.PushClipRect(NkRect{icone.x + icone.w + 6.f, r.y, r.w - icone.w - 34.f, r.h}, true);
				renderer::NkTexte(dl, c.police, icone.x + icone.w + 8.f, ty, el.nom, c.pal.texte);
				dl.PopClipRect();
				if (survol || favori) {
					// L'etoile des Favoris (UE5) : pleine si favori.
					NkVec2 pts[10];
					const float32 ex = etoile.x + 8.f, ey = etoile.y + 8.f;
					for (int32 j = 0; j < 10; ++j) {
						const float32 a = -1.5707963f + 3.14159265f * static_cast<float32>(j) / 5.f;
						const float32 rr = (j % 2 == 0) ? 7.f : 3.f;
						pts[j] = NkVec2{ex + rr * std::cos(a), ey + rr * std::sin(a)};
					}
					if (favori) {
						for (int32 j = 0; j < 10; ++j) {
							dl.AddTriangleFilled(NkVec2{ex, ey}, pts[j], pts[(j + 1) % 10], c.pal.selection);
						}
					} else {
						dl.AddPolyline(pts, 10, c.pal.attenue, 1.2f, true);
					}
				}
				if (survol && in.mouseClicked[0] && ui.placerAppui < 0) {
					if (NkEditeurDans(etoile, in.mousePos) && k < 64) {
						ui.placerFavoris ^= (1ull << static_cast<uint64>(k));
					} else {
						ui.placerAppui = k;
						ui.placerGlisse = false;
						ui.placerDepart = in.mousePos;
					}
				}
			}
			dl.PopClipRect();
			if (ordre.Empty()) {
				const char *vide = onglet == NkOngletPlacer::NK_FAVORIS ? "Aucun favori : l'étoile d'une ligne l'y met."
								   : onglet == NkOngletPlacer::NK_RECENTS ? "Rien de posé pour l'instant."
																		  : "Aucun résultat.";
				renderer::NkTexte(dl, c.petite, liste.x + 4.f, liste.y + 6.f, vide, c.pal.attenue);
			}

			// ── L'aide, en bas : ce que fait le geste, et l'element survole ─
			{
				const NkRect aide{zone.x + 6.f, zone.y + zone.h - aideH + 2.f, zone.w - 12.f, aideH - 4.f};
				dl.AddRectFilled(NkRect{zone.x, aide.y - 3.f, zone.w, 1.f}, c.pal.bord);
				int32 survole = -1;
				for (uint32 i = 0; i < ordre.Size(); ++i) {
					const NkRect &r = ui.placerRects[static_cast<uint32>(ordre[i])];
					if (r.w > 0.f && NkEditeurDans(r, in.mousePos) && NkEditeurDans(liste, in.mousePos)) {
						survole = ordre[i];
					}
				}
				dl.PushClipRect(aide, true);
				renderer::NkTexte(dl, c.petite, aide.x, aide.y, survole >= 0 ? cat[survole].aide : "Glisser dans la vue : poser au point lâché.",
								  c.pal.attenue);
				renderer::NkTexte(dl, c.petite, aide.x, aide.y + 14.f,
								  survole >= 0 ? "Clic : au centre de la vue · glisser : où vous voulez" : "Clic : au centre de la vue.", c.pal.attenue);
				dl.PopClipRect();
			}

			// ── La cloison panneau | Outliner (elle se tire, comme les autres) ─
			{
				const NkRect prise{zone.x + zone.w - 2.f, zone.y, 6.f, zone.h};
				const bool survol = NkEditeurDans(prise, in.mousePos);
				if (survol && in.mouseClicked[0] && ui.cloisonTenue < 0 && ui.placerAppui < 0) {
					ui.placerCloison = true;
				}
				if (ui.placerCloison) {
					if (in.mouseDown[0]) {
						ui.largeurPlacer = in.mousePos.x - zone.x;
					} else {
						ui.placerCloison = false;
					}
				}
				if (survol || ui.placerCloison) {
					ctx.wantCursor = nkgui::NkGuiCursor::ResizeEW;
					dl.AddRectFilled(NkRect{zone.x + zone.w, zone.y, 4.f, zone.h}, c.pal.accent);
				}
			}

			// ── Le geste : clic = au centre ; glisser = la ou on lache ──────
			if (ui.placerAppui >= 0) {
				const int32 k = ui.placerAppui;
				const float32 dx = in.mousePos.x - ui.placerDepart.x, dy = in.mousePos.y - ui.placerDepart.y;
				if (!ui.placerGlisse && dx * dx + dy * dy > SEUIL_GLISSER * SEUIL_GLISSER) {
					ui.placerGlisse = true;
				}
				if (in.mouseReleased[0] || !in.mouseDown[0]) {
					NkEditeurModele &m = c.m;
					if (!ui.placerGlisse) {
						NkEditeurPoserElement(m, &ui, k, m.scene.Camera().Centre());
					} else if (NkEditeurDans(ui.viseur, in.mousePos) && !NkEditeurDans(ui.barreFlottante, in.mousePos)) {
						const NkVec2f monde = m.scene.Camera().EcranVersMonde(NkVec2f(in.mousePos.x, in.mousePos.y));
						// Un VOLUME lache SUR une entite : il devient son collisionneur
						// (le geste d'UE5 « deposer un composant sur un acteur »).
						ecs::NkEntityId sous;
						const bool volume = cat[k].nature == NkNaturePlacer::NK_VOLUME;
						if (volume && m.etat == NkEtatJeu::NK_EDITION && NkEditeurPrendreSous(m, monde, sous)) {
							if (NkEditeurAjouterCollision(m, sous, static_cast<NkCollisionEditeur>(cat[k].indice))) {
								m.selection = sous;
								m.aSelection = true;
							}
						} else {
							NkEditeurPoserElement(m, &ui, k, monde);
						}
					}
					ui.placerAppui = -1;
					ui.placerGlisse = false;
				} else if (ui.placerGlisse) {
					// Le FANTOME : l'icone et le nom sous le curseur ; sur la vue, la
					// croix du point de pose.
					auto &over = ctx.dlOverlay;
					const char *nom = cat[k].nom;
					const float32 w = renderer::NkTexteLargeur(c.police, nom) + 40.f;
					const NkRect g{in.mousePos.x + 14.f, in.mousePos.y + 10.f, w, 26.f};
					over.AddRectFilled(g, c.pal.entete, 3.f);
					const bool surVue = NkEditeurDans(ui.viseur, in.mousePos);
					over.AddRect(g, surVue ? c.pal.selection : c.pal.accent, 1.f, 3.f);
					IconeElement(over, cat[k], NkRect{g.x + 3.f, g.y + 3.f, 20.f, 20.f}, c.pal.texte);
					renderer::NkTexte(over, c.police, g.x + 28.f, g.y + (g.h - renderer::NkTexteHauteurLigne(c.police, 16.f)) * 0.5f, nom,
									  c.pal.texte);
					if (surVue) {
						const NkVec2 p{in.mousePos.x, in.mousePos.y};
						over.AddLine(NkVec2{p.x - 8.f, p.y}, NkVec2{p.x + 8.f, p.y}, c.pal.selection, 1.5f);
						over.AddLine(NkVec2{p.x, p.y - 8.f}, NkVec2{p.x, p.y + 8.f}, c.pal.selection, 1.5f);
					}
				}
			}
		}

	} // namespace editeur
} // namespace nkentseu
