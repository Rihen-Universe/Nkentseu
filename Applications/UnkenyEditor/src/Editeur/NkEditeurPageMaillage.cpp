//
// NkEditeurPageMaillage.cpp
// =============================================================================
// Description :
//   LA FENETRE D'EDITION DU MAILLAGE 2D (2026-10-02, R31) : un onglet de
//   document, a la maniere de l'editeur de sprite d'Unity (« Skinning Editor » :
//   geometrie, contour auto, densite) et de l'editeur de maillage d'UE5
//   (apercu au centre, Details a droite).
//
//     barre     outils (Selection Q, Ajouter A), Trianguler (T), densite -/+ et
//               Refaire, Faire une partie (P), Supprimer (Suppr), Texture et
//               Triangles (apercu), Enregistrer comme asset, Cadrer (F)
//     centre    la texture deformee, les PARTIES colorees, les aretes, les
//               sommets (choisis en ambre) ; molette = zoom, bouton du milieu
//               ou droit = panoramique ; glisser un sommet = le deplacer (ses
//               doubles de couture avec lui ; l'AIMANT colle aux autres sommets
//               et aux aretes) ; glisser dans le vide = cadre de choix ;
//               double-clic sur une arete = la couper
//     droite    les parties (couleur, nom, triangles, physique), la partie
//               choisie (physique, corps, collision, masse, friction, rebond,
//               raideur, forme, ordre), les liens (rigide, elastique, pivot ;
//               rupture), le maillage (comptes, triangles retournes)
//
//   Tout geste passe par le MODELE (NkEditeurMaillage.h) ou, pour le glisser,
//   retient la scene une fois au debut du geste : Ctrl+Z rend tout.
//   EN JEU, la page montre le maillage TEL QUE LA PHYSIQUE LE TIENT (lecture
//   seule) : les parties qui tombent, la gelee qui tremble.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurDocuments.h"
#include "Editeur/NkEditeurInterface.h"
#include "Editeur/NkEditeurMaillage.h"

#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKGui/Widgets/NkGuiWidgets.h"
#include "NKMemory/NKMemory.h"
#include "Unkeny/Maillage/NkUnkenyMaillagePhysique.h"
#include "Unkeny/Rendu/NkUnkenyRendu.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {
			constexpr float32 HAUTEUR_BARRE = 36.f;
			constexpr float32 LARGEUR_PANNEAU = 320.f;
			constexpr float32 HAUTEUR_ETAT = 22.f;
			constexpr float32 RANG = 24.f;
			constexpr float32 PRISE_SOMMET_PX = 7.f;
			constexpr float32 PRISE_ARETE_PX = 5.f;

			NkColor Couleur(uint32 rgba, uint8 alpha = 255u) {
				const math::NkColor c(rgba);
				return NkColor{c.r, c.g, c.b, alpha};
			}

			const char *NomPhysique(uint8 p) {
				switch (static_cast<NkPhysiquePartie2D>(p)) {
					case NkPhysiquePartie2D::NK_RIGIDE:
						return "rigide";
					case NkPhysiquePartie2D::NK_MOLLE:
						return "molle";
					default:
						return "aucune";
				}
			}

			const char *NomLien(uint8 g) {
				switch (static_cast<NkGenreLienParties2D>(g)) {
					case NkGenreLienParties2D::NK_ELASTIQUE:
						return "élastique";
					case NkGenreLienParties2D::NK_PIVOT:
						return "pivot";
					default:
						return "rigide";
				}
			}

			bool Dans(const NkRect &r, const NkVec2 &p) {
				return NkEditeurDans(r, p);
			}

			/// Les sommets choisis s'etendent a leurs DOUBLES (une couture est un point).
			void EtendreAuxDoubles(const NkMaillage2D &ml, uint8 *choisis) {
				for (uint32 i = 0; i < ml.nbSommets; ++i) {
					if (choisis[i] == 0u) {
						continue;
					}
					uint8 d[NK_MAILLAGE2D_SOMMETS_MAX];
					const uint32 n = NkDoublesSommet2D(ml, i, d, NK_MAILLAGE2D_SOMMETS_MAX);
					for (uint32 k = 0; k < n; ++k) {
						choisis[d[k]] = 1u;
					}
				}
			}

			uint32 NbChoisis(const NkDocMaillage &d, const NkMaillage2D &ml) {
				uint32 n = 0;
				for (uint32 i = 0; i < ml.nbSommets; ++i) {
					n += d.choisis[i] != 0u ? 1u : 0u;
				}
				return n;
			}

			void ViderChoix(NkDocMaillage &d) {
				std::memset(d.choisis, 0, sizeof(d.choisis));
			}

			/// Les positions LOCALES telles qu'on les montre : au repos en edition ; EN
			/// JEU, la ou la physique tient les sommets (ramenees dans le repere de
			/// l'entite).
			void PositionsMontrees(NkEditeurModele &m, ecs::NkEntityId e, const NkMaillage2D &ml, NkVec2f *sortie) {
				const NkTransform2D *t = m.scene.Monde().Get<NkTransform2D>(e);
				if (m.etat == NkEtatJeu::NK_EDITION || t == nullptr || !ml.construit) {
					for (uint32 i = 0; i < ml.nbSommets; ++i) {
						sortie[i] = ml.positions[i];
					}
					return;
				}
				NkVec2f w[NK_MAILLAGE2D_SOMMETS_MAX];
				NkMaillagePositionsMonde(m.scene, e, *t, ml, w);
				const float32 c = math::NkCos(-t->rotation), s = math::NkSin(-t->rotation);
				for (uint32 i = 0; i < ml.nbSommets; ++i) {
					const float32 x = w[i].x - t->position.x, y = w[i].y - t->position.y;
					const float32 ex = t->echelle.x != 0.f ? t->echelle.x : 1.f, ey = t->echelle.y != 0.f ? t->echelle.y : 1.f;
					sortie[i] = NkVec2f((x * c - y * s) / ex, (x * s + y * c) / ey);
				}
			}

			/// La vue cadree sur le maillage (sa boite, avec une marge).
			void Cadrer(NkDocMaillage &d, const NkMaillage2D &ml) {
				if (d.vue.w <= 0.f || d.vue.h <= 0.f) {
					return;
				}
				NkVec2f mn(-0.5f, -0.5f), mx(0.5f, 0.5f);
				if (ml.nbSommets > 0u) {
					mn = mx = ml.positions[0];
					for (uint32 i = 1; i < ml.nbSommets; ++i) {
						mn = NkVec2f(math::NkMin(mn.x, ml.positions[i].x), math::NkMin(mn.y, ml.positions[i].y));
						mx = NkVec2f(math::NkMax(mx.x, ml.positions[i].x), math::NkMax(mx.y, ml.positions[i].y));
					}
				}
				const float32 w = math::NkMax(mx.x - mn.x, 0.2f), h = math::NkMax(mx.y - mn.y, 0.2f);
				d.centre = (mn + mx) * 0.5f;
				d.zoom = math::NkClamp(math::NkMin(d.vue.w / (w * 1.35f), d.vue.h / (h * 1.35f)), 10.f, 4000.f);
				d.cadre = true;
			}
		} // namespace

		NkVec2f NkEditeurMaillageVersEcran(const NkDocMaillage &d, const NkVec2f &l) {
			return NkVec2f(d.vue.x + d.vue.w * 0.5f + (l.x - d.centre.x) * d.zoom, d.vue.y + d.vue.h * 0.5f - (l.y - d.centre.y) * d.zoom);
		}

		NkVec2f NkEditeurMaillageDepuisEcran(const NkDocMaillage &d, const NkVec2f &e) {
			return NkVec2f(d.centre.x + (e.x - d.vue.x - d.vue.w * 0.5f) / d.zoom, d.centre.y - (e.y - d.vue.y - d.vue.h * 0.5f) / d.zoom);
		}

		namespace {
			// =================================================================
			// LA VUE CENTRALE
			// =================================================================
			/// Le sommet sous `p` (pixels), ou -1.
			int32 SommetSous(const NkDocMaillage &d, const NkVec2f *pos, uint32 n, const NkVec2 &p) {
				int32 meilleur = -1;
				float32 dMin = PRISE_SOMMET_PX * PRISE_SOMMET_PX;
				for (uint32 i = 0; i < n; ++i) {
					const NkVec2f e = NkEditeurMaillageVersEcran(d, pos[i]);
					const float32 dx = e.x - p.x, dy = e.y - p.y;
					if (dx * dx + dy * dy <= dMin) {
						dMin = dx * dx + dy * dy;
						meilleur = static_cast<int32>(i);
					}
				}
				return meilleur;
			}

			/// L'arete sous `p` (pixels) : ses deux sommets, ou faux.
			bool AreteSous(const NkDocMaillage &d, const NkMaillage2D &ml, const NkVec2f *pos, const NkVec2 &p, uint32 &a, uint32 &b) {
				float32 dMin = PRISE_ARETE_PX;
				bool trouve = false;
				for (uint32 t = 0; t < ml.nbTriangles; ++t) {
					for (uint32 e = 0; e < 3u; ++e) {
						const uint8 i = ml.triangles[t * 3u + e], j = ml.triangles[t * 3u + (e + 1u) % 3u];
						const NkVec2f ea = NkEditeurMaillageVersEcran(d, pos[i]), eb = NkEditeurMaillageVersEcran(d, pos[j]);
						const float32 dd = NkDistanceSegment2D(NkVec2f(p.x, p.y), ea, eb);
						if (dd < dMin) {
							dMin = dd;
							a = i;
							b = j;
							trouve = true;
						}
					}
				}
				return trouve;
			}

			/// L'AIMANT de la page : le point `cible` (repere du maillage) colle au
			/// sommet NON choisi le plus proche, sinon a l'arete la plus proche (dont
			/// aucun bout n'est choisi), dans `rayonPx`.
			bool Aimanter(const NkDocMaillage &d, const NkMaillage2D &ml, const NkVec2f &cible, float32 rayonPx, NkVec2f &sortie) {
				const float32 r = rayonPx / d.zoom;
				float32 dMin = r;
				bool trouve = false;
				for (uint32 i = 0; i < ml.nbSommets; ++i) {
					if (d.choisis[i] != 0u) {
						continue;
					}
					const float32 dx = ml.positions[i].x - cible.x, dy = ml.positions[i].y - cible.y;
					const float32 dd = math::NkSqrt(dx * dx + dy * dy);
					if (dd < dMin) {
						dMin = dd;
						sortie = ml.positions[i];
						trouve = true;
					}
				}
				if (trouve) {
					return true;
				}
				for (uint32 t = 0; t < ml.nbTriangles; ++t) {
					for (uint32 e = 0; e < 3u; ++e) {
						const uint8 i = ml.triangles[t * 3u + e], j = ml.triangles[t * 3u + (e + 1u) % 3u];
						if (d.choisis[i] != 0u || d.choisis[j] != 0u) {
							continue;
						}
						const NkVec2f a = ml.positions[i], b = ml.positions[j];
						const float32 abx = b.x - a.x, aby = b.y - a.y;
						const float32 l2 = abx * abx + aby * aby;
						float32 k = l2 > 1.0e-12f ? ((cible.x - a.x) * abx + (cible.y - a.y) * aby) / l2 : 0.f;
						k = math::NkClamp(k, 0.f, 1.f);
						const NkVec2f q(a.x + abx * k, a.y + aby * k);
						const float32 dd = math::NkSqrt((q.x - cible.x) * (q.x - cible.x) + (q.y - cible.y) * (q.y - cible.y));
						if (dd < dMin) {
							dMin = dd;
							sortie = q;
							trouve = true;
						}
					}
				}
				return trouve;
			}

			void DessinerVue(NkEditeurCadre &c, NkDocMaillage &d, ecs::NkEntityId e, NkMaillage2D &ml) {
				NkEditeurModele &m = c.m;
				auto &dl = c.ctx.dl;
				const NkRect &v = d.vue;
				const nkgui::NkGuiInput &in = c.ctx.input;
				const bool edition = m.etat == NkEtatJeu::NK_EDITION;
				// Le fond : un damier discret (la transparence de la texture se voit).
				dl.PushClipRect(v, true);
				dl.AddRectFilled(v, NkColor{30, 31, 34, 255});
				{
					const float32 cote = 16.f;
					for (float32 y = v.y; y < v.y + v.h; y += cote) {
						for (float32 x = v.x + (static_cast<int32>((y - v.y) / cote) % 2 == 0 ? 0.f : cote); x < v.x + v.w; x += 2.f * cote) {
							dl.AddRectFilled(NkRect{x, y, cote, cote}, NkColor{36, 37, 41, 255});
						}
					}
				}
				// La grille (0,1 m ; 1 m plus marque) et les axes du maillage.
				{
					const NkVec2f mn = NkEditeurMaillageDepuisEcran(d, NkVec2f(v.x, v.y + v.h));
					const NkVec2f mx = NkEditeurMaillageDepuisEcran(d, NkVec2f(v.x + v.w, v.y));
					const float32 pas = d.zoom > 60.f ? 0.1f : 1.f;
					for (float32 x = std::floor(mn.x / pas) * pas; x <= mx.x; x += pas) {
						const NkVec2f a = NkEditeurMaillageVersEcran(d, NkVec2f(x, 0.f));
						const bool fort = std::fabs(x - std::floor(x + 0.5f)) < pas * 0.01f;
						dl.AddLine(NkVec2{a.x, v.y}, NkVec2{a.x, v.y + v.h}, fort ? NkColor{255, 255, 255, 22} : NkColor{255, 255, 255, 9}, 1.f);
					}
					for (float32 y = std::floor(mn.y / pas) * pas; y <= mx.y; y += pas) {
						const NkVec2f a = NkEditeurMaillageVersEcran(d, NkVec2f(0.f, y));
						const bool fort = std::fabs(y - std::floor(y + 0.5f)) < pas * 0.01f;
						dl.AddLine(NkVec2{v.x, a.y}, NkVec2{v.x + v.w, a.y}, fort ? NkColor{255, 255, 255, 22} : NkColor{255, 255, 255, 9}, 1.f);
					}
					const NkVec2f o = NkEditeurMaillageVersEcran(d, NkVec2f(0.f, 0.f));
					dl.AddLine(NkVec2{v.x, o.y}, NkVec2{v.x + v.w, o.y}, NkColor{215, 85, 85, 110}, 1.f);
					dl.AddLine(NkVec2{o.x, v.y}, NkVec2{o.x, v.y + v.h}, NkColor{95, 195, 105, 110}, 1.f);
				}
				NkVec2f pos[NK_MAILLAGE2D_SOMMETS_MAX];
				PositionsMontrees(m, e, ml, pos);
				NkVec2f ecran[NK_MAILLAGE2D_SOMMETS_MAX];
				for (uint32 i = 0; i < ml.nbSommets; ++i) {
					ecran[i] = NkEditeurMaillageVersEcran(d, pos[i]);
				}
				// ── 1. L'apercu : la texture deformee (ou les couleurs de sommet) ──
				if (d.texture && ml.nbTriangles > 0u) {
					NkVue2D cam; // une camera qui envoie le repere du maillage dans la vue
					cam.PoserViseur(v);
					cam.PoserCentre(d.centre);
					cam.PoserZoom(d.zoom);
					NkDessinerMaillage2D(dl, cam, ml, pos);
				}
				// ── 2. Les PARTIES colorees (un voile par triangle) ──
				for (uint32 t = 0; t < ml.nbTriangles; ++t) {
					const uint32 k = ml.partieTriangle[t];
					const uint8 alpha = d.texture ? (ml.nbParties > 1u ? 70u : 28u) : 140u;
					const NkColor col = Couleur(NkCouleurPartie2D(ml, k), static_cast<int32>(k) == d.partie && ml.nbParties > 1u ? alpha + 40u : alpha);
					dl.AddTriangleFilled(ecran[ml.triangles[t * 3u]], ecran[ml.triangles[t * 3u + 1u]], ecran[ml.triangles[t * 3u + 2u]], col);
				}
				// ── 3. Les aretes (couleur de leur partie) ──
				if (d.triangles) {
					for (uint32 t = 0; t < ml.nbTriangles; ++t) {
						const NkColor col = Couleur(NkCouleurPartie2D(ml, ml.partieTriangle[t]), 230u);
						for (uint32 k = 0; k < 3u; ++k) {
							dl.AddLine(ecran[ml.triangles[t * 3u + k]], ecran[ml.triangles[t * 3u + (k + 1u) % 3u]], col, 1.f);
						}
					}
				}
				// ── 4. Le geste en cours (edition seulement) ──
				const bool dedans = Dans(v, in.mousePos) && c.ui.menu == NkMenuEditeur::NK_AUCUN;
				const NkVec2f souris = NkEditeurMaillageDepuisEcran(d, NkVec2f(in.mousePos.x, in.mousePos.y));
				// Molette : zoom autour du curseur ; milieu ou droit : panoramique.
				if (dedans && in.wheel != 0.f) {
					const float32 k = in.wheel > 0.f ? 1.15f : 1.f / 1.15f;
					const NkVec2f avant = souris;
					d.zoom = math::NkClamp(d.zoom * k, 10.f, 4000.f);
					const NkVec2f apres = NkEditeurMaillageDepuisEcran(d, NkVec2f(in.mousePos.x, in.mousePos.y));
					d.centre = d.centre + (avant - apres);
				}
				if (dedans && (in.mouseClicked[1] || in.mouseClicked[2])) {
					d.geste = 3;
					d.appui = NkVec2f(in.mousePos.x, in.mousePos.y);
				}
				if (d.geste == 3) {
					if (in.mouseDown[1] || in.mouseDown[2]) {
						d.centre = d.centre - NkVec2f((in.mousePos.x - d.appui.x) / d.zoom, -(in.mousePos.y - d.appui.y) / d.zoom);
						d.appui = NkVec2f(in.mousePos.x, in.mousePos.y);
					} else {
						d.geste = 0;
					}
				}
				const int32 sous = SommetSous(d, pos, ml.nbSommets, in.mousePos);
				d.aimantVu = false;
				if (edition && dedans && in.mouseDoubleClicked[0] && sous < 0 && d.outil == NkOutilMaillage::NK_SELECTION) {
					// Double-clic sur une arete : elle se coupe en son milieu.
					uint32 a = 0, b = 0;
					if (AreteSous(d, ml, pos, in.mousePos, a, b)) {
						const int32 s = NkEditeurMaillageCouper(m, e, a, b);
						ViderChoix(d);
						if (s >= 0) {
							d.choisis[static_cast<uint32>(s)] = 1u;
						}
						d.geste = 0;
						dl.PopClipRect();
						return; // le maillage a change sous nos pieds : la trame suivante le redessine
					}
				} else if (edition && dedans && in.mouseClicked[0]) {
					if (d.outil == NkOutilMaillage::NK_AJOUTER) {
						NkVec2f p = souris;
						NkVec2f colle;
						if (c.ui.aimant && Aimanter(d, ml, p, c.ui.aimantRayonPx, colle)) {
							p = colle;
						}
						const int32 s = NkEditeurMaillageAjouter(m, e, p);
						ViderChoix(d);
						if (s >= 0) {
							d.choisis[static_cast<uint32>(s)] = 1u;
						}
						dl.PopClipRect();
						return;
					}
					uint32 a = 0, b = 0;
					if (sous >= 0) {
						if (in.shiftDown) {
							uint8 g[NK_MAILLAGE2D_SOMMETS_MAX];
							const uint32 n = NkDoublesSommet2D(ml, static_cast<uint32>(sous), g, NK_MAILLAGE2D_SOMMETS_MAX);
							const uint8 nouveau = d.choisis[sous] != 0u ? 0u : 1u;
							for (uint32 k = 0; k < n; ++k) {
								d.choisis[g[k]] = nouveau;
							}
						} else if (d.choisis[sous] == 0u) {
							ViderChoix(d);
							d.choisis[sous] = 1u;
							EtendreAuxDoubles(ml, d.choisis);
						}
						d.geste = d.choisis[sous] != 0u ? 1 : 0;
						d.saisi = pos[sous];
					} else if (AreteSous(d, ml, pos, in.mousePos, a, b)) {
						if (!in.shiftDown) {
							ViderChoix(d);
						}
						d.choisis[a] = 1u;
						d.choisis[b] = 1u;
						EtendreAuxDoubles(ml, d.choisis);
						d.geste = 1;
						d.saisi = souris;
					} else {
						if (!in.shiftDown) {
							ViderChoix(d);
						}
						d.geste = 2;
					}
					d.appui = NkVec2f(in.mousePos.x, in.mousePos.y);
					d.dernier = souris;
					d.retenu = false;
					for (uint32 i = 0; i < ml.nbSommets; ++i) {
						d.depart[i] = ml.positions[i];
						d.departUV[i] = ml.uvs[i];
					}
				}
				if (d.geste == 1 && edition) {
					if (!in.mouseDown[0]) {
						d.geste = 0;
					} else {
						NkVec2f cible = d.saisi + (souris - d.dernier);
						// L'AIMANT (Ctrl l'inverse, comme l'accrochage de la scene).
						const bool aimant = c.ui.aimant != in.ctrlDown;
						NkVec2f colle;
						if (aimant && Aimanter(d, ml, cible, c.ui.aimantRayonPx, colle)) {
							cible = colle;
							d.aimantVu = true;
							d.aimantPoint = colle;
						}
						const NkVec2f delta = cible - d.saisi;
						const float32 bouge = std::fabs(in.mousePos.x - d.appui.x) + std::fabs(in.mousePos.y - d.appui.y);
						if (bouge > 2.f) {
							if (!d.retenu) {
								// UNE photo par geste : Ctrl+Z rend le maillage d'avant le glisser.
								NkEditeurRetenir(m);
								d.retenu = true;
							}
							NkMaillage2D *vif = m.scene.Monde().Get<NkMaillage2D>(e);
							// « UV collees » : la texture reste a sa place -- l'UV de la
							// nouvelle position est LUE dans le maillage d'avant le geste.
							NkMaillage2D *avant = nullptr;
							if (vif != nullptr && d.uvCollees) {
								avant = memory::NkGetDefaultAllocator().New<NkMaillage2D>();
								if (avant != nullptr) {
									*avant = *vif;
									for (uint32 i = 0; i < avant->nbSommets; ++i) {
										avant->positions[i] = d.depart[i];
										avant->uvs[i] = d.departUV[i];
									}
								}
							}
							for (uint32 i = 0; vif != nullptr && i < vif->nbSommets; ++i) {
								if (d.choisis[i] != 0u) {
									vif->positions[i] = d.depart[i] + delta;
									uint32 col = 0u;
									if (avant == nullptr || !NkMaillageEchantillonner2D(*avant, vif->positions[i], vif->uvs[i], col)) {
										vif->uvs[i] = d.departUV[i];
									}
								}
							}
							if (avant != nullptr) {
								memory::NkGetDefaultAllocator().Delete(avant);
							}
						}
					}
				}
				if (d.geste == 2) {
					const NkRect cadre{math::NkMin(d.appui.x, in.mousePos.x), math::NkMin(d.appui.y, in.mousePos.y),
									   std::fabs(in.mousePos.x - d.appui.x), std::fabs(in.mousePos.y - d.appui.y)};
					dl.AddRectFilled(cadre, NkColor{90, 150, 235, 40});
					dl.AddRect(cadre, NkColor{90, 150, 235, 200}, 1.f);
					if (!in.mouseDown[0]) {
						for (uint32 i = 0; i < ml.nbSommets; ++i) {
							if (Dans(cadre, NkVec2{ecran[i].x, ecran[i].y})) {
								d.choisis[i] = 1u;
							}
						}
						EtendreAuxDoubles(ml, d.choisis);
						d.geste = 0;
					}
				}
				// ── 5. Les sommets (choisis en ambre ; survole cerne de blanc) ──
				for (uint32 i = 0; i < ml.nbSommets; ++i) {
					const NkVec2f &p = ecran[i];
					const bool choisi = d.choisis[i] != 0u;
					const NkColor fond = choisi ? c.pal.selection : NkColor{240, 240, 240, 230};
					dl.AddRectFilled(NkRect{p.x - 3.5f, p.y - 3.5f, 7.f, 7.f}, NkColor{20, 20, 22, 230}, 1.f);
					dl.AddRectFilled(NkRect{p.x - 2.5f, p.y - 2.5f, 5.f, 5.f}, fond, 1.f);
					if (static_cast<int32>(i) == sous && edition) {
						dl.AddRect(NkRect{p.x - 5.f, p.y - 5.f, 10.f, 10.f}, NkColor{255, 255, 255, 220}, 1.2f);
					}
				}
				if (d.aimantVu) {
					const NkVec2f a = NkEditeurMaillageVersEcran(d, d.aimantPoint);
					dl.AddCircle(NkVec2{a.x, a.y}, 8.f, NkColor{255, 90, 200, 230}, 1.6f);
				}
				// L'outil Ajouter montre ou le sommet tomberait.
				if (edition && dedans && d.outil == NkOutilMaillage::NK_AJOUTER) {
					dl.AddCircle(in.mousePos, 5.f, c.pal.accent, 1.4f);
					dl.AddLine(NkVec2{in.mousePos.x - 8.f, in.mousePos.y}, NkVec2{in.mousePos.x + 8.f, in.mousePos.y}, c.pal.accent, 1.f);
					dl.AddLine(NkVec2{in.mousePos.x, in.mousePos.y - 8.f}, NkVec2{in.mousePos.x, in.mousePos.y + 8.f}, c.pal.accent, 1.f);
				}
				if (!edition) {
					const NkRect bandeau{v.x, v.y, v.w, 24.f};
					dl.AddRectFilled(bandeau, NkColor{200, 120, 40, 200});
					renderer::NkTexteDansBoite(dl, c.petite, bandeau,
											   "EN JEU : le maillage suit ses parties (lecture seule) — « Arrêter » pour l'éditer", NkColor{20, 20, 20, 255});
				} else if (ml.nbSommets == 0u) {
					renderer::NkTexteDansBoite(dl, c.police, NkRect{v.x, v.y + v.h * 0.5f - 20.f, v.w, 40.f},
											   "Maillage vide : l'outil « Ajouter » (A) pose des sommets ; au troisième, les triangles naissent.",
											   c.pal.attenue);
				}
				dl.PopClipRect();
				dl.AddRect(v, c.pal.bord, 1.f);
			}

			// =================================================================
			// LE PANNEAU DE DROITE
			// =================================================================
			struct NkPanneau {
					NkEditeurCadre &c;
					NkDocMaillage &d;
					ecs::NkEntityId e;
					NkRect r;
					float32 y;
					bool actif;
			};

			void Titre(NkPanneau &P, const char *texte) {
				auto &dl = P.c.ctx.dl;
				P.y += 6.f;
				const NkRect t{P.r.x, P.y, P.r.w, 22.f};
				dl.AddRectFilled(t, P.c.pal.entete);
				renderer::NkTexte(dl, P.c.police, t.x + 8.f, t.y + (t.h - renderer::NkTexteHauteurLigne(P.c.police, 16.f)) * 0.5f, texte,
								  P.c.pal.texte);
				P.y += t.h + 4.f;
			}

			void Ligne(NkPanneau &P, const char *texte, bool vif = false) {
				renderer::NkTexte(P.c.ctx.dl, P.c.petite, P.r.x + 10.f, P.y + 3.f, texte, vif ? P.c.pal.texte : P.c.pal.attenue);
				P.y += 18.f;
			}

			/// Un choix segmente ; rend l'indice clique, ou -1. `rects` (facultatif) recoit les boutons.
			int32 Segments(NkPanneau &P, const char *libelle, const char *const *noms, int32 n, int32 valeur, NkRect *rects = nullptr) {
				auto &dl = P.c.ctx.dl;
				const float32 lw = 96.f;
				renderer::NkTexte(dl, P.c.petite, P.r.x + 10.f, P.y + 5.f, libelle, P.c.pal.attenue);
				const float32 x0 = P.r.x + lw, w = (P.r.w - lw - 8.f) / static_cast<float32>(n);
				int32 clique = -1;
				for (int32 k = 0; k < n; ++k) {
					const NkRect b{x0 + static_cast<float32>(k) * w, P.y, w - 2.f, RANG - 2.f};
					if (rects != nullptr) {
						rects[k] = b;
					}
					if (NkEditeurBouton(P.c, b, "", k == valeur, P.actif)) {
						clique = k;
					}
					renderer::NkTexteDansBoite(dl, P.c.petite, b, noms[k], k == valeur ? P.c.pal.surAccent : P.c.pal.texte);
				}
				P.y += RANG + 2.f;
				return clique;
			}

			/// Un nombre ; un changement RETIENT la scene une fois par geste (le glisser
			/// d'un champ est UN geste pour Ctrl+Z).
			bool Nombre(NkPanneau &P, const char *libelle, float32 &v, float32 pas, float32 vmin, float32 vmax) {
				nkgui::NkGuiContext &ctx = P.c.ctx;
				renderer::NkTexte(ctx.dl, P.c.petite, P.r.x + 10.f, P.y + 5.f, libelle, P.c.pal.attenue);
				float32 copie = v;
				const NkRect champ{P.r.x + 96.f, P.y, P.r.w - 104.f, RANG - 2.f};
				bool change = false;
				if (P.actif) {
					ctx.PushId(libelle);
					ctx.SetNextItemRect(champ);
					change = nkgui::DragFloat(ctx, "##v", copie, pas, vmin, vmax);
					ctx.PopId();
				} else {
					// EN JEU (ou sous un menu) : la valeur se LIT, elle ne se tire pas.
					ctx.dl.AddRectFilled(champ, P.c.pal.champ, 2.f);
					renderer::NkTexteDansBoite(ctx.dl, P.c.petite, champ, NkString::Format("%.3f", static_cast<double>(v)).CStr(),
											   P.c.pal.attenue);
				}
				P.y += RANG + 2.f;
				if (change && copie != v) {
					if (!P.d.retenu) {
						NkEditeurRetenir(P.c.m);
						P.d.retenu = true;
					}
					v = copie;
					return true;
				}
				return false;
			}

			bool Case(NkPanneau &P, const char *libelle, bool &v) {
				auto &dl = P.c.ctx.dl;
				const NkRect b{P.r.x + 10.f, P.y + 3.f, 16.f, 16.f};
				dl.AddRectFilled(b, P.c.pal.champ, 2.f);
				dl.AddRect(b, P.c.pal.bord, 1.f, 2.f);
				if (v) {
					dl.AddLine(NkVec2{b.x + 3.f, b.y + 8.f}, NkVec2{b.x + 7.f, b.y + 12.f}, P.c.pal.accent, 2.f);
					dl.AddLine(NkVec2{b.x + 7.f, b.y + 12.f}, NkVec2{b.x + 13.f, b.y + 4.f}, P.c.pal.accent, 2.f);
				}
				renderer::NkTexte(dl, P.c.petite, b.x + 24.f, P.y + 4.f, libelle, P.c.pal.texte);
				const NkRect zone{b.x, P.y, P.r.w - 20.f, 22.f};
				P.y += 24.f;
				if (P.actif && Dans(zone, P.c.ctx.input.mousePos) && P.c.ctx.input.mouseClicked[0]) {
					NkEditeurRetenir(P.c.m);
					v = !v;
					return true;
				}
				return false;
			}

			bool Bouton(NkPanneau &P, const char *texte, bool actif = true) {
				const NkRect b{P.r.x + 10.f, P.y, P.r.w - 20.f, RANG};
				const bool clic = NkEditeurBouton(P.c, b, texte, false, P.actif && actif);
				P.y += RANG + 4.f;
				return clic;
			}

			void DessinerPanneau(NkEditeurCadre &c, NkDocMaillage &d, ecs::NkEntityId e) {
				NkEditeurModele &m = c.m;
				auto &dl = c.ctx.dl;
				dl.AddRectFilled(d.panneau, c.pal.panneau);
				dl.AddRectFilled(NkRect{d.panneau.x, d.panneau.y, 1.f, d.panneau.h}, c.pal.bord);
				dl.PushClipRect(d.panneau, true);
				NkPanneau P{c, d, e, NkRect{d.panneau.x + 1.f, d.panneau.y, d.panneau.w - 1.f, d.panneau.h}, d.panneau.y,
							m.etat == NkEtatJeu::NK_EDITION && c.ui.menu == NkMenuEditeur::NK_AUCUN};
				// Un champ relache : le geste suivant sera retenu a son tour.
				if (!c.ctx.input.mouseDown[0] && d.geste == 0) {
					d.retenu = false;
				}
				NkMaillage2D *ml = m.scene.Monde().Get<NkMaillage2D>(e);
				if (ml == nullptr) {
					dl.PopClipRect();
					return;
				}
				// ── Les parties ──
				Titre(P, "Parties");
				for (uint32 k = 0; k < NK_MAILLAGE2D_PARTIES_MAX; ++k) {
					d.rangeesParties[k] = NkRect{0.f, 0.f, 0.f, 0.f};
				}
				for (uint32 k = 0; k < ml->nbParties; ++k) {
					const NkRect r{P.r.x + 6.f, P.y, P.r.w - 12.f, RANG};
					d.rangeesParties[k] = r;
					const bool choisie = static_cast<int32>(k) == d.partie;
					if (choisie) {
						dl.AddRectFilled(r, NkColor{c.pal.accent.r, c.pal.accent.g, c.pal.accent.b, 60}, 3.f);
					} else if (Dans(r, c.ctx.input.mousePos)) {
						dl.AddRectFilled(r, c.pal.boutonSurvol, 3.f);
					}
					dl.AddRectFilled(NkRect{r.x + 6.f, r.y + 6.f, 12.f, 12.f}, Couleur(NkCouleurPartie2D(*ml, k)), 2.f);
					uint32 nt = 0;
					for (uint32 t = 0; t < ml->nbTriangles; ++t) {
						nt += ml->partieTriangle[t] == k ? 1u : 0u;
					}
					const NkString txt = NkString::Format("%s   %u tri.   %s", ml->parties[k].nom[0] != '\0' ? ml->parties[k].nom : "(partie)", nt,
														  NomPhysique(ml->parties[k].physique));
					renderer::NkTexte(dl, c.police, r.x + 26.f, r.y + (r.h - renderer::NkTexteHauteurLigne(c.police, 16.f)) * 0.5f, txt.CStr(),
									  c.pal.texte);
					if (Dans(r, c.ctx.input.mousePos) && c.ctx.input.mouseClicked[0] && c.ui.menu == NkMenuEditeur::NK_AUCUN) {
						d.partie = static_cast<int32>(k);
					}
					P.y += RANG + 2.f;
				}
				if (ml->nbParties == 0u) {
					Ligne(P, "(aucun triangle : posez des sommets)");
				} else {
					Ligne(P, "Choisir des sommets, puis P : une partie.");
				}
				if (d.partie >= static_cast<int32>(ml->nbParties)) {
					d.partie = 0;
				}
				// ── La partie choisie ──
				if (ml->nbParties > 0u) {
					const uint32 k = static_cast<uint32>(d.partie);
					Titre(P, NkString::Format("Partie : %s", ml->parties[k].nom).CStr());
					static const char *const kPhys[3] = {"Aucune", "Rigide", "Molle"};
					const int32 ph = Segments(P, "Physique", kPhys, 3, ml->parties[k].physique, d.physique);
					if (ph >= 0) {
						NkEditeurMaillagePhysique(m, e, k, static_cast<NkPhysiquePartie2D>(ph));
						ml = m.scene.Monde().Get<NkMaillage2D>(e);
					}
					NkPartieMaillage2D &p = ml->parties[k];
					const NkPhysiquePartie2D phys = static_cast<NkPhysiquePartie2D>(p.physique);
					if (phys == NkPhysiquePartie2D::NK_AUCUNE) {
						Ligne(P, "Elle suit l'entité (son transform, ou son corps).");
					}
					if (phys == NkPhysiquePartie2D::NK_RIGIDE) {
						static const char *const kCorps[3] = {"Statique", "Cinématique", "Dynamique"};
						const int32 tc = Segments(P, "Corps", kCorps, 3, p.typeCorps);
						if (tc >= 0 && tc != p.typeCorps) {
							NkEditeurRetenir(m);
							p.typeCorps = static_cast<uint8>(tc);
						}
						Case(P, "Collisionneur depuis son contour", p.collision);
						Nombre(P, "Masse (kg)", p.masse, 0.05f, 0.01f, 1000.f);
						Nombre(P, "Friction", p.friction, 0.01f, 0.f, 2.f);
						Nombre(P, "Rebond", p.rebond, 0.01f, 0.f, 1.f);
					}
					if (phys == NkPhysiquePartie2D::NK_MOLLE) {
						Nombre(P, "Masse (kg)", p.masse, 0.05f, 0.01f, 1000.f);
						Nombre(P, "Raideur", p.raideur, 0.01f, 0.01f, 1.f);
						Nombre(P, "Forme", p.forme, 0.005f, 0.f, 1.f);
						Nombre(P, "Friction", p.friction, 0.01f, 0.f, 2.f);
						Nombre(P, "Rebond", p.rebond, 0.01f, 0.f, 1.f);
						Ligne(P, "Une particule par sommet ; ses arêtes en liens.");
					}
					float32 ordre = static_cast<float32>(p.ordre);
					if (Nombre(P, "Ordre de dessin", ordre, 0.1f, -32.f, 32.f)) {
						p.ordre = static_cast<int32>(std::floor(ordre + 0.5f));
					}
					if (k > 0u && Bouton(P, "Rendre ses triangles au Corps (retirer la partie)")) {
						NkEditeurMaillageRetirerPartie(m, e, k);
						ViderChoix(d);
						d.partie = 0;
						ml = m.scene.Monde().Get<NkMaillage2D>(e);
					}
					// ── Les liens de la partie ──
					Titre(P, "Liens");
					bool aucun = true;
					for (uint32 l = 0; ml != nullptr && l < ml->nbLiens; ++l) {
						NkLienParties2D &x = ml->liens[l];
						if (x.a != k && x.b != k) {
							continue;
						}
						aucun = false;
						const uint32 autre = x.a == k ? x.b : x.a;
						const NkRect r{P.r.x + 6.f, P.y, P.r.w - 12.f, RANG};
						const bool choisi = static_cast<int32>(l) == d.lien;
						if (choisi) {
							dl.AddRectFilled(r, NkColor{c.pal.accent.r, c.pal.accent.g, c.pal.accent.b, 50}, 3.f);
						}
						dl.AddRectFilled(NkRect{r.x + 6.f, r.y + 6.f, 12.f, 12.f}, Couleur(NkCouleurPartie2D(*ml, autre)), 2.f);
						const NkString txt = NkString::Format("avec %s : %s%s", ml->parties[autre].nom, NomLien(x.genre), x.casse ? " (cassé)" : "");
						renderer::NkTexte(dl, c.petite, r.x + 26.f, r.y + 5.f, txt.CStr(), c.pal.texte);
						const NkRect croix{r.x + r.w - 22.f, r.y + 2.f, 20.f, r.h - 4.f};
						if (NkEditeurBouton(c, croix, "x", false, P.actif)) {
							NkEditeurMaillageDelier(m, e, l);
							d.lien = -1;
							ml = m.scene.Monde().Get<NkMaillage2D>(e);
							P.y += RANG + 2.f;
							break;
						}
						if (Dans(r, c.ctx.input.mousePos) && c.ctx.input.mouseClicked[0] && !Dans(croix, c.ctx.input.mousePos)) {
							d.lien = static_cast<int32>(l);
						}
						P.y += RANG + 2.f;
					}
					if (aucun) {
						Ligne(P, "Aucun lien : elle va seule.");
					}
					// Relier a une autre partie (une par bouton).
					for (uint32 j = 0; ml != nullptr && j < ml->nbParties; ++j) {
						if (j == k) {
							continue;
						}
						bool deja = false;
						for (uint32 l = 0; l < ml->nbLiens; ++l) {
							deja = deja || (ml->liens[l].a == k && ml->liens[l].b == j) || (ml->liens[l].a == j && ml->liens[l].b == k);
						}
						if (deja) {
							continue;
						}
						if (Bouton(P, NkString::Format("Relier à « %s »%s", ml->parties[j].nom, NkPartiesVoisines2D(*ml, k, j) ? " (voisine)" : "").CStr())) {
							d.lien = NkEditeurMaillageLier(m, e, k, j, NkGenreLienParties2D::NK_RIGIDE);
							ml = m.scene.Monde().Get<NkMaillage2D>(e);
						}
					}
					if (ml != nullptr && d.lien >= 0 && d.lien < static_cast<int32>(ml->nbLiens)) {
						NkLienParties2D &x = ml->liens[d.lien];
						static const char *const kGenres[3] = {"Rigide", "Élastique", "Pivot"};
						const int32 g = Segments(P, "Genre", kGenres, 3, x.genre);
						if (g >= 0 && g != x.genre) {
							NkEditeurRetenir(m);
							x.genre = static_cast<uint8>(g);
						}
						Nombre(P, x.genre == static_cast<uint8>(NkGenreLienParties2D::NK_ELASTIQUE) ? "Rupture (m)" : "Rupture (N)", x.rupture,
							   x.genre == static_cast<uint8>(NkGenreLienParties2D::NK_ELASTIQUE) ? 0.01f : 1.f, 0.f, 100000.f);
						if (x.genre == static_cast<uint8>(NkGenreLienParties2D::NK_ELASTIQUE)) {
							Nombre(P, "Raideur (N/m)", x.raideur, 1.f, 0.f, 100000.f);
							Nombre(P, "Amortiss.", x.amortissement, 0.1f, 0.f, 1000.f);
						}
						Ligne(P, "Rupture 0 : incassable.");
					}
				}
				// ── La COULEUR des sommets choisis (la couleur par sommet multiplie la
				//    texture : une joue rosie, un bord assombri) ──
				if (ml != nullptr && NbChoisis(d, *ml) > 0u) {
					Titre(P, NkString::Format("Sommets choisis : %u", NbChoisis(d, *ml)).CStr());
					uint32 premier = 0;
					while (premier < ml->nbSommets && d.choisis[premier] == 0u) {
						++premier;
					}
					const math::NkColorF cf = math::NkColor(ml->couleurs[premier]).ToColorF();
					float32 col[4] = {cf.r, cf.g, cf.b, cf.a};
					renderer::NkTexte(dl, c.petite, P.r.x + 10.f, P.y + 5.f, "Couleur", c.pal.attenue);
					c.ctx.PushId("maillage.couleur");
					c.ctx.SetNextItemRect(NkRect{P.r.x + 96.f, P.y, P.r.w - 104.f, RANG - 2.f});
					const bool change = P.actif && nkgui::ColorEdit4(c.ctx, "##v", col);
					c.ctx.PopId();
					P.y += RANG + 2.f;
					if (change) {
						if (!d.retenu) {
							NkEditeurRetenir(m);
							d.retenu = true;
						}
						ml = m.scene.Monde().Get<NkMaillage2D>(e);
						const uint32 rgba = math::NkColor(math::NkColorF(col[0], col[1], col[2], col[3])).ToUint32A();
						for (uint32 i = 0; ml != nullptr && i < ml->nbSommets; ++i) {
							if (d.choisis[i] != 0u) {
								ml->couleurs[i] = rgba;
							}
						}
					}
				}
				// ── Le maillage ──
				if (ml != nullptr) {
					Titre(P, "Le maillage");
					Ligne(P, NkString::Format("%u sommets, %u triangles, %u parties, %u liens", ml->nbSommets, ml->nbTriangles, ml->nbParties, ml->nbLiens).CStr(), true);
					const uint32 ret = NkTrianglesRetournes2D(*ml);
					Ligne(P, ret == 0u ? "Aucun triangle retourné." : NkString::Format("%u triangle(s) RETOURNÉ(S) : Trianguler (T)", ret).CStr(), ret != 0u);
					Case(P, "Les parties se touchent entre elles", ml->partiesSeTouchent);
					if (ml->source[0] != '\0') {
						Ligne(P, NkString::Format("Asset : %s", ml->source).CStr());
					}
				}
				dl.PopClipRect();
			}
		} // namespace

		// =====================================================================
		// LA PAGE
		// =====================================================================
		bool NkEditeurDessinerPageMaillage(NkEditeurCadre &c) {
			NkEditeurInterface &ui = c.ui;
			NkDocMaillage *pd = NkEditeurDocMaillageActif(ui);
			if (pd == nullptr) {
				return false;
			}
			NkDocMaillage &d = *pd;
			NkEditeurModele &m = c.m;
			auto &dl = c.ctx.dl;
			const NkRect zone{ui.ecran.x, ui.barreOutils.y, ui.ecran.w, ui.statut.y - ui.barreOutils.y};
			dl.AddRectFilled(zone, c.pal.fond);
			const ecs::NkEntityId e = NkEditeurCibleMaillage(m, d);
			NkMaillage2D *ml = e.IsValid() ? m.scene.Monde().Get<NkMaillage2D>(e) : nullptr;
			// ── La barre ──
			const NkRect barre{zone.x, zone.y, zone.w, HAUTEUR_BARRE};
			dl.AddRectFilled(barre, c.pal.entete);
			dl.AddRectFilled(NkRect{barre.x, barre.y + barre.h - 1.f, barre.w, 1.f}, c.pal.bord);
			const float32 ty = barre.y + (barre.h - renderer::NkTexteHauteurLigne(c.police, 16.f)) * 0.5f;
			float32 x = barre.x + 12.f;
			dl.AddRectFilled(NkRect{x, barre.y + barre.h * 0.5f - 5.f, 10.f, 10.f}, NkColor{110, 200, 150, 255}, 2.f);
			x += 18.f;
			const NkString titre = NkEditeurLibelleMaillage(m, d);
			renderer::NkTexte(dl, c.police, x, ty, titre.CStr(), c.pal.texte);
			x += renderer::NkTexteLargeur(c.police, titre.CStr()) + 20.f;
			if (ml == nullptr) {
				renderer::NkTexteDansBoite(dl, c.police, NkRect{zone.x, zone.y + zone.h * 0.4f, zone.w, 40.f},
										   "Cette entité n'a plus de maillage 2D (supprimée, ou Ctrl+Z). Fermez l'onglet (Ctrl+W).", c.pal.attenue);
				return true;
			}
			const bool edition = m.etat == NkEtatJeu::NK_EDITION;
			const bool libre = c.ui.menu == NkMenuEditeur::NK_AUCUN;
			const float32 bh = barre.h - 8.f;
			auto B = [&](NkBoutonMaillage b, const char *texte, bool enfonce, bool actif) {
				const float32 w = renderer::NkTexteLargeur(c.police, texte) + 18.f;
				const NkRect r{x, barre.y + 4.f, w, bh};
				d.boutons[static_cast<uint32>(b)] = r;
				x += w + 4.f;
				return NkEditeurBouton(c, r, texte, enfonce, actif && libre);
			};
			auto Separer = [&]() { x += 10.f; };
			if (B(NkBoutonMaillage::NK_SELECTION, "Sélection (Q)", d.outil == NkOutilMaillage::NK_SELECTION, edition)) {
				d.outil = NkOutilMaillage::NK_SELECTION;
			}
			if (B(NkBoutonMaillage::NK_AJOUTER, "Ajouter (A)", d.outil == NkOutilMaillage::NK_AJOUTER, edition)) {
				d.outil = NkOutilMaillage::NK_AJOUTER;
			}
			Separer();
			if (B(NkBoutonMaillage::NK_TRIANGULER, "Trianguler (T)", false, edition && ml->nbSommets >= 3u)) {
				NkEditeurMaillageTrianguler(m, e);
				ViderChoix(d);
			}
			if (B(NkBoutonMaillage::NK_DENSITE_MOINS, "-", false, edition && d.densite > 2)) {
				--d.densite;
			}
			const NkString dens = NkString::Format("Densité %d", d.densite);
			renderer::NkTexte(dl, c.police, x, ty, dens.CStr(), c.pal.texte);
			x += renderer::NkTexteLargeur(c.police, dens.CStr()) + 4.f;
			if (B(NkBoutonMaillage::NK_DENSITE_PLUS, "+", false, edition && d.densite < 16)) {
				++d.densite;
			}
			if (B(NkBoutonMaillage::NK_REFAIRE, "Refaire", false, edition && ml->nbTriangles > 0u)) {
				NkEditeurMaillageRefaire(m, e, d.densite);
				ViderChoix(d);
			}
			Separer();
			ml = m.scene.Monde().Get<NkMaillage2D>(e);
			const uint32 nChoisis = ml != nullptr ? NbChoisis(d, *ml) : 0u;
			if (B(NkBoutonMaillage::NK_FAIRE_PARTIE, "Faire une partie (P)", false, edition && nChoisis >= 3u)) {
				const int32 k = NkEditeurMaillageFairePartie(m, e, d.choisis);
				if (k >= 0) {
					d.partie = k;
				}
				ViderChoix(d);
			}
			if (B(NkBoutonMaillage::NK_SUPPRIMER, "Supprimer", false, edition && nChoisis > 0u)) {
				NkEditeurMaillageSupprimer(m, e, d.choisis);
				ViderChoix(d);
			}
			Separer();
			if (B(NkBoutonMaillage::NK_TEXTURE, "Texture", d.texture, true)) {
				d.texture = !d.texture;
			}
			if (B(NkBoutonMaillage::NK_TRIANGLES, "Triangles", d.triangles, true)) {
				d.triangles = !d.triangles;
			}
			if (B(NkBoutonMaillage::NK_UV, "UV collées", d.uvCollees, edition)) {
				d.uvCollees = !d.uvCollees;
			}
			if (B(NkBoutonMaillage::NK_CADRER, "Cadrer (F)", false, true)) {
				d.cadre = false;
			}
			if (B(NkBoutonMaillage::NK_ASSET, "Enregistrer comme asset", false, edition && ml != nullptr && ml->nbTriangles > 0u)) {
				NkEditeurMaillageEnregistrerAsset(m, e);
				c.ui.maillagesFrais = false;
				c.ui.contenuPerime = true;
			}
			ml = m.scene.Monde().Get<NkMaillage2D>(e);
			if (ml == nullptr) {
				return true;
			}
			// ── La vue, le panneau, l'etat ──
			const float32 corpsY = barre.y + barre.h;
			d.panneau = NkRect{zone.x + zone.w - LARGEUR_PANNEAU, corpsY, LARGEUR_PANNEAU, zone.y + zone.h - corpsY - HAUTEUR_ETAT};
			d.vue = NkRect{zone.x + 6.f, corpsY + 6.f, zone.w - LARGEUR_PANNEAU - 12.f, zone.y + zone.h - corpsY - HAUTEUR_ETAT - 12.f};
			if (!d.cadre) {
				Cadrer(d, *ml);
			}
			DessinerVue(c, d, e, *ml);
			ml = m.scene.Monde().Get<NkMaillage2D>(e);
			if (ml == nullptr) {
				return true;
			}
			DessinerPanneau(c, d, e);
			ml = m.scene.Monde().Get<NkMaillage2D>(e);
			const NkRect etat{zone.x, zone.y + zone.h - HAUTEUR_ETAT, zone.w, HAUTEUR_ETAT};
			dl.AddRectFilled(etat, c.pal.entete);
			const NkVec2f souris = NkEditeurMaillageDepuisEcran(d, NkVec2f(c.ctx.input.mousePos.x, c.ctx.input.mousePos.y));
			const NkString aide =
				d.outil == NkOutilMaillage::NK_AJOUTER
					? NkString("Ajouter : clic = un sommet (dedans il coupe son triangle, dehors il se relie au bord). Q : Sélection.")
					: NkString::Format("%u choisi(s)  |  clic, Maj+clic, cadre : choisir  ·  glisser : déplacer%s  ·  double-clic sur une arête : la couper  ·  "
									   "Suppr, P, T, Ctrl+Z",
									   ml != nullptr ? NbChoisis(d, *ml) : 0u, c.ui.aimant ? " (aimant)" : "");
			renderer::NkTexte(dl, c.petite, etat.x + 10.f, etat.y + 4.f, aide.CStr(), c.pal.attenue);
			const NkString coord = NkString::Format("x %.3f  y %.3f  |  %.0f px/m", static_cast<double>(souris.x), static_cast<double>(souris.y),
													static_cast<double>(d.zoom));
			renderer::NkTexte(dl, c.petite, etat.x + etat.w - 10.f - renderer::NkTexteLargeur(c.petite, coord.CStr()), etat.y + 4.f, coord.CStr(),
							  c.pal.attenue);
			return true;
		}

		// =====================================================================
		// LE CLAVIER
		// =====================================================================
		bool NkEditeurPageMaillageAuClavier(NkEditeurCadre &c) {
			NkDocMaillage *pd = NkEditeurDocMaillageActif(c.ui);
			if (pd == nullptr) {
				return false;
			}
			NkDocMaillage &d = *pd;
			NkEditeurModele &m = c.m;
			const nkgui::NkGuiInput &in = c.ctx.input;
			using nkgui::NkGuiKey;
			const ecs::NkEntityId e = NkEditeurCibleMaillage(m, d);
			NkMaillage2D *ml = e.IsValid() ? m.scene.Monde().Get<NkMaillage2D>(e) : nullptr;
			if (in.ctrlDown) {
				// Ctrl+Z / Ctrl+Y : l'historique de la SCENE (le maillage en est un
				// composant) ; le choix de sommets ne survit pas (les indices changent).
				if (in.KeyPressed(NkGuiKey::Z) || in.KeyPressed(NkGuiKey::Y)) {
					const bool refaire = in.KeyPressed(NkGuiKey::Y) || in.shiftDown;
					refaire ? NkEditeurRefaire(m) : NkEditeurAnnuler(m);
					ViderChoix(d);
					return true;
				}
				if (in.KeyPressed(NkGuiKey::A) && ml != nullptr) {
					for (uint32 i = 0; i < ml->nbSommets; ++i) {
						d.choisis[i] = 1u;
					}
					return true;
				}
				return false; // Ctrl+S, Ctrl+W... : la scene les garde
			}
			if (ml == nullptr || m.etat != NkEtatJeu::NK_EDITION) {
				return false;
			}
			if (in.KeyPressed(NkGuiKey::Delete) || in.KeyPressed(NkGuiKey::Backspace)) {
				NkEditeurMaillageSupprimer(m, e, d.choisis);
				ViderChoix(d);
				return true;
			}
			if (in.KeyPressed(NkGuiKey::P)) {
				const int32 k = NkEditeurMaillageFairePartie(m, e, d.choisis);
				if (k >= 0) {
					d.partie = k;
				}
				ViderChoix(d);
				return true;
			}
			if (in.KeyPressed(NkGuiKey::T)) {
				NkEditeurMaillageTrianguler(m, e);
				ViderChoix(d);
				return true;
			}
			if (in.KeyPressed(NkGuiKey::A)) {
				d.outil = NkOutilMaillage::NK_AJOUTER;
				return true;
			}
			if (in.KeyPressed(NkGuiKey::Q)) {
				d.outil = NkOutilMaillage::NK_SELECTION;
				return true;
			}
			if (in.KeyPressed(NkGuiKey::F)) {
				d.cadre = false;
				return true;
			}
			if (in.KeyPressed(NkGuiKey::Escape)) {
				ViderChoix(d);
				return true;
			}
			return false;
		}

	} // namespace editeur
} // namespace nkentseu
