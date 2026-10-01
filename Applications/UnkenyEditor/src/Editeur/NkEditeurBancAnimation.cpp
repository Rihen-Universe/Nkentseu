//
// NkEditeurBancAnimation.cpp
// =============================================================================
// Description :
//   `UnkenyEditor --selftest` : les pages ANIMATION et ANIMATEUR (2026-10-01,
//   R21/R22/R35). Compte A PART (« BANC ANIMATION ») : les bancs d'avant gardent
//   leurs comptes. Les gestes passent par la VRAIE trame (NkEditeurDessinerTrame)
//   et la souris par NkEditeurSouris, comme depuis l'OS -- jamais la vraie souris.
//   Et `--captures-animation=DOSSIER` : les deux pages, HORS ECRAN, en PNG.
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   PAGE ANIMATION
//   (a1) Fenetre > Animation sur une entite : un onglet de document, la page
//        occupe le corps
//   (a2) « + Propriete » propose Transform.position (et la couleur du sprite) ;
//        un clic l'ajoute, avec une cle AU CURSEUR de la valeur de l'entite
//   (a3) POSER : la regle a l'image 15, puis frotter la case X de la piste de
//        100 px : une seconde cle a 0,5 s, X + 1,00
//   (a4) INTERPOLATION : a 0,25 s, la courbe de NKAnima rend X + 0,5 (lineaire) ;
//        la case « Entree » sur les deux cles : X + 0,25
//   (a5) DEPLACER la cle de l'image 15 a l'image 20 ; SUPPRIMER (Suppr) ; Ctrl+Z
//        la rend
//   (a6) L'APERCU N'ECRIT PAS LA SCENE : apres une trame de la page, l'entite a
//        sa position d'avant ; appliquer le clip a 0,67 s la DEPLACE vraiment
//   (a7) LECTURE : Espace lance la frise, le curseur avance ; « Jouer en jeu »
//        puis Jouer : la scene ANIME l'entite (NkClipProprietes2D) ; Arreter la rend
//   (a8) ENREGISTRER : Contenu/Animations/Heros.nkanim ; RELU dans une autre
//        interface, la piste et ses cles reviennent
//   SEQUENCE (2026-10-01 soir : la frise facon UE5 et le melange)
//   (c1) les conversions : une piste de CLIPS (deux clips qui se chevauchent),
//        une piste MUETTE, une tangente a la main font l'aller-retour
//        frise -> clip -> frise
//   (c2) le jeu joue la SEQUENCE : a 0,75 s, mi-fondu entre A (x 0) et B (x 1),
//        la position est a 0,5 ; l'apercu d'une trame ne laisse rien
//   (c3) « + Piste » propose d'abord une piste de clips ; son « + » ouvre le
//        choix des animations ; un clic y pose le clip au curseur
//   (b7) (01/10 soir) le graphe d'un CONTROLEUR : une couche « Tir » (masque
//        « Torse », additive, poids 0,5) et un arbre de melange 1D sur
//        « vitesse », une courbe douce : graphe -> controleur -> graphe garde tout,
//        et le .nkanimctl ecrit par l'editeur se relit par Unkeny
//   PAGE ANIMATEUR
//   (b1) Fenetre > Animateur : « + Reel », puis deux double-clics sur la toile =
//        deux etats aux points du clic
//   (b2) glisser du rond du premier vers le second = la transition ; « + Condition »
//        (sur le parametre) ; frotter son seuil de 50 px : 0,50
//   (b3) double-clic sur un etat, taper « idle », Entree : renomme
//   (b4) ENREGISTRER le .nkanimctl ; NKAnima le RELIT (etats, transition, condition,
//        positions) ; rouvert dans une autre interface : le meme graphe
//   (b5) EN JEU : « Appliquer a l'entite », Jouer, Reel = 1 : la transition TIRE
//        (l'entite est dans le second etat) et la page l'allume (etat actif,
//        transition tiree)
//   (b6) les onglets : l'onglet de la scene la ramene (la vue se dessine) ; celui
//        d'un document le remet au premier plan ; sa croix le ferme
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurBancTrame.h"
#include "Editeur/NkEditeurContenu.h"
#include "Editeur/NkEditeurPagesAnim.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKGui/Core/NkGuiDrawListRaster.h"
#include "NKImage/Core/NkImage.h"
#include "Unkeny/Anim/NkUnkenyAnimateur.h"
#include "Unkeny/Simulation/NkUnkenyActeurs.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		namespace {
			int32 gE = 0;
			int32 gR = 0;

			void Temoin(bool ok, const char *quoi, float32 v = 0.f) {
				std::printf("  [%s] %-74s %9.4f\n", ok ? " OK " : "ECHEC", quoi, static_cast<double>(v));
				(ok ? gR : gE)++;
			}

			bool Pres(float32 a, float32 b, float32 tol = 1e-3f) {
				return std::fabs(a - b) <= tol;
			}

			/// Le heros du banc : un sprite colore, une identite.
			ecs::NkEntityId PoserHeros(NkEditeurModele &m) {
				const ecs::NkEntityId h = NkEditeurCreerEntite(m, "Heros", NkVec2f(1.f, 2.f));
				NkSprite2D s;
				s.taille = NkVec2f(1.f, 1.5f);
				s.couleur = 0x4FA3E0FFu;
				m.scene.Monde().Add<NkSprite2D>(h, s);
				m.scene.AssurerUid(h);
				return h;
			}

			NkVec2f PositionDe(NkEditeurModele &m, ecs::NkEntityId id) {
				const NkTransform2D *t = m.scene.Monde().IsAlive(id) ? m.scene.Monde().Get<NkTransform2D>(id) : nullptr;
				return t != nullptr ? t->position : NkVec2f(-999.f, -999.f);
			}

			nkgui::NkVec2 Centre(const editorkit::NkPaintRect &r) {
				return nkgui::NkVec2{r.x + r.w * 0.5f, r.y + r.h * 0.5f};
			}

			nkgui::NkVec2 Centre(const nkgui::NkRect &r) {
				return nkgui::NkVec2{r.x + r.w * 0.5f, r.y + r.h * 0.5f};
			}

			const editorkit::NkTimelineRow *Ligne(const NkEditeurInterface &ui, nk_uint64 piste) {
				for (uint32 k = 0; k < (uint32)ui.pagesAnim.frise.rows.Size(); ++k) {
					if (ui.pagesAnim.frise.rows[k].track == piste) {
						return &ui.pagesAnim.frise.rows[k];
					}
				}
				return nullptr;
			}

			const editorkit::NkGraphHit *Champ(const NkEditeurInterface &ui, editorkit::NkGraphField f, int32 index = -1) {
				for (uint32 i = 0; i < (uint32)ui.pagesAnim.graphe.hits.Size(); ++i) {
					const editorkit::NkGraphHit &h = ui.pagesAnim.graphe.hits[i];
					if (h.field == f && (index < 0 || h.index == index)) {
						return &h;
					}
				}
				return nullptr;
			}

			const editorkit::NkGraphNodeRect *RectNoeud(const NkEditeurInterface &ui, nk_uint64 id) {
				for (uint32 i = 0; i < (uint32)ui.pagesAnim.graphe.nodeRects.Size(); ++i) {
					if (ui.pagesAnim.graphe.nodeRects[i].id == id) {
						return &ui.pagesAnim.graphe.nodeRects[i];
					}
				}
				return nullptr;
			}

			/// La trame courante, RASTERISEE sans fenetre ni GPU (NkGuiDrawListRaster,
			/// la sonde de NkAnimaEditor), puis ecrite en PNG (NKImage) -- le meme
			/// geste que les captures des formes (NkEditeurBancFormes.cpp).
			bool EcrirePng(NkEditeurBancTrame &T, const char *chemin) {
				memory::NkAllocator &tas = memory::NkGetDefaultAllocator();
				nkgui::NkGuiDrawListRaster *ras = tas.New<nkgui::NkGuiDrawListRaster>();
				const int32 w = static_cast<int32>(T.W), h = static_cast<int32>(T.H);
				ras->Init(w, h);
				ras->Effacer(0x141414FFu);
				ras->PoserTexture(T.police->TexId(), T.police->pixels, T.police->atlasW, T.police->atlasH, 1);
				for (uint32 k = 0; k < T.m.textures.Nombre(); ++k) {
					const uint32 id = NkTextures2D::kPremierId + k;
					int32 tw = 0, th = 0;
					if (T.m.textures.Taille(id, tw, th) && T.m.textures.Pixels(id) != nullptr) {
						ras->PoserTexture(id, T.m.textures.Pixels(id), tw, th, 4);
					}
				}
				ras->Rasteriser(T.pctx->dl);
				ras->Rasteriser(T.pctx->dlOverlay);
				NkImage img = NkImage::Wrap(const_cast<uint8 *>(ras->Pixels()), w, h, NkImagePixelFormat::NK_RGBA32);
				const bool ok = img.SavePNG(chemin);
				std::printf("  capture %s : %s\n", chemin, ok ? "ok" : "ECHEC");
				tas.Delete(ras);
				return ok;
			}

			NkEditeurModele *NouveauModele(const char *dossier) {
				NkEditeurModele *pm = memory::NkGetDefaultAllocator().New<NkEditeurModele>();
				NkCreerRessourcesSim(pm->ressources, &pm->textures, nullptr);
				NkEditeurNouvelleScene(*pm);
				NkDirectory::Delete(dossier, true);
				NkString contenu(dossier);
				contenu.Append("/projet/Contenu");
				NkDirectory::CreateRecursive(contenu.CStr());
				pm->chemin = NkString(dossier);
				pm->chemin.Append("/projet/scene.nkscene");
				return pm;
			}

			void Agrandir(NkEditeurBancTrame &T) {
				T.W = 1600.f;
				T.H = 900.f;
				T.pctx->Init(1600, 900);
			}

			// =================================================================
			// LA PAGE ANIMATION
			// =================================================================
			void BancPageAnimation() {
				NkEditeurModele *pm = NouveauModele("banc_anim");
				NkEditeurModele &m = *pm;
				const ecs::NkEntityId heros = PoserHeros(m);
				const NkVec2f p0 = PositionDe(m, heros);
				NkEditeurBancTrame *pt = memory::NkGetDefaultAllocator().New<NkEditeurBancTrame>(m);
				NkEditeurBancTrame &T = *pt;
				Agrandir(T);
				NkEditeurInterface &ui = T.Ui();
				T.Trame();
				// (a1)
				NkEditeurCadre cadre = T.Cadre();
				NkEditeurExecuter(cadre, NK_A_ANIM_ANIMATION);
				T.Trame();
				T.Trame();
				NkDocAnim *d = NkEditeurDocAnimActif(ui);
				Temoin(d != nullptr && d->genre == NkGenreDocAnim::NK_ANIMATION && ui.pagesAnim.onglets.Size() == 1u &&
						   ui.pagesAnim.page.w > 0.f && ui.pagesAnim.frise.area.w > 0.f && d->nom == NkString("Heros"),
					   "(a1) Fenetre > Animation : un onglet, la page occupe le corps");
				if (d == nullptr) {
					memory::NkGetDefaultAllocator().Delete(pt);
					memory::NkGetDefaultAllocator().Delete(pm);
					return;
				}
				// (a2) « + Propriete » puis Transform.position.
				const editorkit::NkPaintRect ajout = ui.pagesAnim.frise.buttons[(uint8)editorkit::NkTimelineButton::AddTrack];
				T.Clic(0, Centre(ajout).x, Centre(ajout).y);
				T.Trame();
				int32 kPos = -1, kCoul = -1;
				for (uint32 i = 0; i < (uint32)d->propositions.Size(); ++i) {
					if (d->propositions[i].propriete.nom == NkString("Transform.position") && d->propositions[i].cible.Empty()) {
						kPos = (int32)i;
					}
					if (d->propositions[i].propriete.nom == NkString("Sprite.couleur")) {
						kCoul = (int32)i;
					}
				}
				const bool ouvert = d->choix;
				if (kPos >= 0) {
					const nkgui::NkRect r = d->propositions[(uint32)kPos].rect;
					T.Clic(0, Centre(r).x, Centre(r).y);
				}
				T.Trame();
				const editorkit::NkTimelineTrack *piste = d->frise.tracks.Empty() ? nullptr : &d->frise.tracks[0];
				const bool ajoutee = piste != nullptr && piste->property == NkString("Transform.position") && piste->keys.Size() == 1u &&
									 Pres(piste->keys[0].v[0], p0.x) && Pres(piste->keys[0].v[1], p0.y) && !d->choix;
				Temoin(ouvert && kPos >= 0 && kCoul >= 0 && ajoutee,
					   "(a2) « + Propriete » : position et couleur proposees ; un clic = piste + cle", (float32)d->propositions.Size());
				if (piste == nullptr) {
					memory::NkGetDefaultAllocator().Delete(pt);
					memory::NkGetDefaultAllocator().Delete(pm);
					return;
				}
				const nk_uint64 idPiste = piste->id;
				// (a3) l'image 15 a la regle, puis frotter la case X de 100 px.
				const editorkit::NkPaintRect regle = ui.pagesAnim.frise.ruler;
				const float32 x15 = editorkit::NkTimelineTimeToX(d->frise, ui.pagesAnim.frise.area, 0.5f);
				T.Clic(0, x15, regle.y + regle.h * 0.5f);
				const bool image15 = d->frise.FrameOf(d->frise.cursor) == 15;
				const editorkit::NkTimelineRow *ligne = Ligne(ui, idPiste);
				if (ligne != nullptr) {
					const nkgui::NkVec2 cx = Centre(ligne->channel[0]);
					T.Glisser(cx.x, cx.y, cx.x + 100.f, cx.y, 5);
				}
				T.Trame();
				piste = d->frise.Track(idPiste);
				const int32 k15 = piste != nullptr ? d->frise.KeyAt(*piste, 0.5f) : -1;
				const float32 x15v = k15 >= 0 ? piste->keys[(uint32)k15].v[0] : -999.f;
				Temoin(image15 && ligne != nullptr && piste->keys.Size() == 2u && Pres(x15v, p0.x + 1.f, 0.02f),
					   "(a3) poser : regle a l'image 15, frotter X de 100 px -> cle a 0,5 s, X+1", x15v - p0.x);
				// (a4) l'interpolation : celle du CLIP (NKAnima), pas du kit.
				T.Trame();
				const anim::NkAnimationClip::NkPropertyTrack *pc = d->clip.FindPropertyTrack("", "Transform.position");
				const float32 xLin = pc != nullptr ? pc->curve.Evaluate(0.25f).x - p0.x : -999.f;
				d->frise.SelectAll();
				const editorkit::NkPaintRect entree = ui.pagesAnim.frise.buttons[(uint8)editorkit::NkTimelineButton::InterpFirst +
																				  (uint8)editorkit::NkTimelineInterp::Entree];
				T.Clic(0, Centre(entree).x, Centre(entree).y);
				T.Trame();
				pc = d->clip.FindPropertyTrack("", "Transform.position");
				const float32 xIn = pc != nullptr ? pc->curve.Evaluate(0.25f).x - p0.x : -999.f;
				Temoin(Pres(xLin, 0.5f, 0.02f) && Pres(xIn, 0.25f, 0.02f) && pc != nullptr &&
						   pc->curve.GetKey(0).interp == anim::NkInterpMode::NK_EASE_IN,
					   "(a4) interpolation NKAnima : lineaire 0,5 a mi-chemin, Entree 0,25", xIn);
				// (a5) deplacer la cle de l'image 15 a l'image 20, la supprimer, l'annuler.
				ligne = Ligne(ui, idPiste);
				d->frise.ClearSelection();
				const float32 xa = editorkit::NkTimelineTimeToX(d->frise, ui.pagesAnim.frise.area, 0.5f);
				const float32 xb = editorkit::NkTimelineTimeToX(d->frise, ui.pagesAnim.frise.area, 20.f / 30.f);
				const float32 yl = ligne != nullptr ? ligne->y + ligne->h * 0.5f : -100.f;
				T.Glisser(xa, yl, xb, yl, 6);
				piste = d->frise.Track(idPiste);
				const bool deplacee = piste != nullptr && piste->keys.Size() == 2u && Pres(piste->keys[1].time, 20.f / 30.f) &&
									  d->frise.KeyAt(*piste, 0.5f) < 0;
				T.Clic(0, xb, yl); // la choisir seule
				T.Touche(nkgui::NkGuiKey::Delete);
				const bool supprimee = d->frise.Track(idPiste)->keys.Size() == 1u;
				T.Touche(nkgui::NkGuiKey::Z, true);
				const bool rendue =
					d->frise.Track(idPiste)->keys.Size() == 2u && Pres(d->frise.Track(idPiste)->keys[1].time, 20.f / 30.f);
				Temoin(deplacee && supprimee && rendue, "(a5) deplacer (image 15 -> 20), supprimer (Suppr), Ctrl+Z la rend");
				// (a6) l'apercu ne laisse rien ; appliquer le clip DEPLACE l'entite.
				d->frise.SetCursor(20.f / 30.f);
				T.Trame();
				const NkVec2f apresTrame = PositionDe(m, heros);
				const uint32 n = unkeny::NkAppliquerClipProprietes(m.scene, heros, d->clip, 20.f / 30.f);
				const NkVec2f applique = PositionDe(m, heros);
				m.scene.Monde().Get<NkTransform2D>(heros)->position = p0;
				Temoin(Pres(apresTrame.x, p0.x) && Pres(apresTrame.y, p0.y) && n == 1u && Pres(applique.x, p0.x + 1.f, 0.02f),
					   "(a6) l'apercu rend la scene ; le clip applique deplace vraiment l'entite", applique.x - p0.x);
				// (a7) lecture : Espace ; puis « Jouer en jeu » et Jouer.
				d->frise.SetCursor(0.f);
				T.Touche(nkgui::NkGuiKey::Space);
				for (int32 k = 0; k < 10; ++k) {
					T.Trame();
				}
				const bool lit = d->frise.playing && d->frise.cursor > 0.1f;
				T.Touche(nkgui::NkGuiKey::Space);
				const nkgui::NkRect jouerJeu = ui.pagesAnim.boutons[(uint8)NkBoutonPage::NK_ATTACHER];
				T.Clic(0, Centre(jouerJeu).x, Centre(jouerJeu).y);
				const unkeny::NkClipProprietes2D *cp = m.scene.Monde().Get<unkeny::NkClipProprietes2D>(heros);
				const bool attache = cp != nullptr && std::strcmp(cp->clip, "Heros") == 0;
				const uint64 uid = m.scene.Uid(heros);
				NkEditeurJouer(m);
				// 0,35 s de jeu (la trame du jeu est plafonnee a 0,05 s : sept pas), a
				// peu pres a mi-chemin de la cle de 0,67 s (Entree : moins de la moitie).
				for (int32 k = 0; k < 7; ++k) {
					NkEditeurAvancer(m, 0.05f);
				}
				const float32 xJeu = PositionDe(m, m.scene.EntiteParUid(uid)).x - p0.x;
				NkEditeurArreter(m);
				const float32 xArret = PositionDe(m, m.scene.EntiteParUid(uid)).x;
				Temoin(lit && attache && xJeu > 0.05f && xJeu < 0.95f && Pres(xArret, p0.x),
					   "(a7) Espace lit la frise ; en Jouer la scene anime l'entite ; Arreter la rend", xJeu);
				// (a8) enregistrer, relire ailleurs.
				T.Trame();
				const nkgui::NkRect enr = ui.pagesAnim.boutons[(uint8)NkBoutonPage::NK_ENREGISTRER];
				T.Clic(0, Centre(enr).x, Centre(enr).y);
				const NkString chemin = d->chemin;
				const bool ecrit = NkFile::Exists(chemin.CStr()) && !d->modifie;
				NkEditeurInterface *autre = memory::NkGetDefaultAllocator().New<NkEditeurInterface>();
				const bool relu = NkEditeurOuvrirAnimation(m, *autre, chemin.CStr());
				NkDocAnim *d2 = NkEditeurDocAnimActif(*autre);
				const bool memes = d2 != nullptr && d2->frise.tracks.Size() == 1u && d2->frise.tracks[0].keys.Size() == 2u &&
								   Pres(d2->frise.tracks[0].keys[1].time, 20.f / 30.f) &&
								   Pres(d2->frise.tracks[0].keys[1].v[0], p0.x + 1.f, 0.02f) &&
								   d2->frise.tracks[0].keys[0].interp == (uint8)editorkit::NkTimelineInterp::Entree;
				memory::NkGetDefaultAllocator().Delete(autre);
				Temoin(ecrit && relu && memes && chemin.EndsWith("Animations/Heros.nkanim"),
					   "(a8) Enregistrer : Contenu/Animations/Heros.nkanim, relu ailleurs a l'identique");
				memory::NkGetDefaultAllocator().Delete(pt);
				memory::NkGetDefaultAllocator().Delete(pm);
				NkDirectory::Delete("banc_anim", true);
			}

			// =================================================================
			// LA PAGE ANIMATEUR
			// =================================================================
			void BancPageAnimateur() {
				NkEditeurModele *pm = NouveauModele("banc_animateur");
				NkEditeurModele &m = *pm;
				const ecs::NkEntityId heros = PoserHeros(m);
				const uint64 uid = m.scene.Uid(heros);
				NkEditeurBancTrame *pt = memory::NkGetDefaultAllocator().New<NkEditeurBancTrame>(m);
				NkEditeurBancTrame &T = *pt;
				Agrandir(T);
				NkEditeurInterface &ui = T.Ui();
				T.Trame();
				NkEditeurCadre cadre = T.Cadre();
				NkEditeurExecuter(cadre, NK_A_ANIM_ANIMATEUR);
				T.Trame();
				T.Trame();
				NkDocAnim *d = NkEditeurDocAnimActif(ui);
				if (d == nullptr || d->genre != NkGenreDocAnim::NK_ANIMATEUR) {
					Temoin(false, "(b1) Fenetre > Animateur ouvre un document");
					memory::NkGetDefaultAllocator().Delete(pt);
					memory::NkGetDefaultAllocator().Delete(pm);
					return;
				}
				editorkit::NkStateGraphModel &g = d->graphe;
				// (b1) « + Reel » puis deux etats par double-clic.
				const editorkit::NkPaintRect reel = ui.pagesAnim.graphe.buttons[(uint8)editorkit::NkStateGraphButton::AddFloat];
				T.Clic(0, Centre(reel).x, Centre(reel).y);
				const editorkit::NkPaintRect toile = ui.pagesAnim.graphe.canvas;
				float32 ax = 0.f, ay = 0.f, bx = 0.f, by = 0.f;
				editorkit::NkStateGraphToScreen(g, toile, 1.f, -140.f, 0.f, ax, ay);
				editorkit::NkStateGraphToScreen(g, toile, 1.f, 160.f, 0.f, bx, by);
				T.DoubleClic(ax, ay);
				T.DoubleClic(bx, by);
				T.Trame();
				const bool deux = g.nodes.Size() == 2u && g.params.Size() == 1u &&
								  g.params[0].kind == (uint8)editorkit::NkGraphParamKind::Float;
				const nk_uint64 a = deux ? g.nodes[0].id : 0;
				const nk_uint64 b = deux ? g.nodes[1].id : 0;
				Temoin(deux && Pres(g.Node(a)->x, -140.f, 1.f) && Pres(g.Node(b)->x, 160.f, 1.f),
					   "(b1) « + Reel », deux double-clics : deux etats aux points du clic", (float32)g.nodes.Size());
				if (!deux) {
					memory::NkGetDefaultAllocator().Delete(pt);
					memory::NkGetDefaultAllocator().Delete(pm);
					return;
				}
				// (b2) la transition, sa condition, son seuil.
				const editorkit::NkGraphNodeRect *ra = RectNoeud(ui, a);
				const editorkit::NkGraphNodeRect *rb = RectNoeud(ui, b);
				if (ra != nullptr && rb != nullptr) {
					const nkgui::NkVec2 port = Centre(ra->port);
					const nkgui::NkVec2 cible = Centre(rb->rect);
					T.Glisser(port.x, port.y, cible.x, cible.y, 6);
				}
				const nk_uint64 t = g.selectedTransition;
				T.Trame();
				const editorkit::NkGraphHit *plus = Champ(ui, editorkit::NkGraphField::CondAdd);
				if (plus != nullptr) {
					const nkgui::NkVec2 p = Centre(plus->rect);
					T.Clic(0, p.x, p.y);
				}
				T.Trame();
				const editorkit::NkGraphHit *seuil = Champ(ui, editorkit::NkGraphField::CondValue, 0);
				if (seuil != nullptr) {
					const nkgui::NkVec2 p = Centre(seuil->rect);
					T.Glisser(p.x, p.y, p.x + 50.f, p.y, 5);
				}
				const editorkit::NkGraphTransition *tr = g.Transition(t);
				const bool cond = tr != nullptr && tr->from == a && tr->to == b && tr->conditions.Size() == 1u &&
								  tr->conditions[0].param == g.params[0].name &&
								  tr->conditions[0].op == (uint8)editorkit::NkGraphCondOp::Greater &&
								  Pres(tr->conditions[0].threshold, 0.5f, 0.01f);
				Temoin(cond, "(b2) rond A -> B : la transition ; « + Condition » ; seuil frotte a 0,50",
					   tr != nullptr && !tr->conditions.Empty() ? tr->conditions[0].threshold : -1.f);
				// (b3) renommer A en « idle ».
				ra = RectNoeud(ui, a);
				if (ra != nullptr) {
					const nkgui::NkVec2 p = Centre(ra->rect);
					T.DoubleClic(p.x - 30.f, p.y);
				}
				T.Trame();
				T.Taper("idle");
				T.Touche(nkgui::NkGuiKey::Enter);
				Temoin(g.Node(a) != nullptr && g.Node(a)->name == NkString("idle") && !d->renomme,
					   "(b3) double-clic sur un etat, « idle », Entree : renomme");
				// (b4) enregistrer, relire par NKAnima, rouvrir.
				T.Trame();
				const nkgui::NkRect enr = ui.pagesAnim.boutons[(uint8)NkBoutonPage::NK_ENREGISTRER];
				T.Clic(0, Centre(enr).x, Centre(enr).y);
				anim::NkAnimStateMachine lue;
				const bool lu = NkFile::Exists(d->chemin.CStr()) && lue.LoadBinary(d->chemin);
				float32 px = 0.f, py = 0.f;
				NkString nomParam;
				anim::NkAnimStateMachine::NkCondKind genre = anim::NkAnimStateMachine::NkCondKind::BOOL_TRUE;
				float32 s = 0.f;
				const int32 idle = lu ? lue.FindState("idle") : -1;
				const bool forme = lu && lue.GetStateCount() == 2 && idle >= 0 && lue.GetTransitionCount() == 1u &&
								   lue.GetConditionCount(0) == 1u && lue.GetCondition(0, 0, nomParam, genre, s) &&
								   genre == anim::NkAnimStateMachine::NkCondKind::FLOAT_GREATER && Pres(s, 0.5f, 0.01f) &&
								   lue.GetStatePosition(idle, px, py) && Pres(px, -140.f, 1.f);
				NkEditeurInterface *autre = memory::NkGetDefaultAllocator().New<NkEditeurInterface>();
				const bool rouvert = NkEditeurOuvrirAnimateur(m, *autre, d->chemin.CStr());
				NkDocAnim *d2 = NkEditeurDocAnimActif(*autre);
				const bool meme = d2 != nullptr && d2->graphe.nodes.Size() == 2u && d2->graphe.transitions.Size() == 1u &&
								  d2->graphe.transitions[0].conditions.Size() == 1u && d2->graphe.params.Size() == 1u;
				memory::NkGetDefaultAllocator().Delete(autre);
				Temoin(forme && rouvert && meme,
					   "(b4) .nkanimctl : NKAnima le relit (etats, condition, positions) ; rouvert = meme graphe");
				// (b5) en jeu, la transition tire, et la page l'allume.
				T.Trame();
				const nkgui::NkRect appliquer = ui.pagesAnim.boutons[(uint8)NkBoutonPage::NK_ATTACHER];
				T.Clic(0, Centre(appliquer).x, Centre(appliquer).y);
				const unkeny::NkAnimateur2D *an = m.scene.Monde().Get<unkeny::NkAnimateur2D>(heros);
				const bool porte = an != nullptr && std::strcmp(an->modele, d->nom.CStr()) == 0;
				const nkgui::NkRect jouer = ui.pagesAnim.boutons[(uint8)NkBoutonPage::NK_JOUER];
				T.Clic(0, Centre(jouer).x, Centre(jouer).y);
				const bool enJeu = m.etat == NkEtatJeu::NK_JEU;
				NkEditeurAvancer(m, 0.05f);
				unkeny::NkAnimateur2D *aj = m.scene.Monde().Get<unkeny::NkAnimateur2D>(m.scene.EntiteParUid(uid));
				const NkString avant = aj != nullptr ? unkeny::NkEtatAnimateur2D(*aj) : NkString();
				T.Trame();
				const bool avantVu = g.live && g.liveState == a;
				if (aj != nullptr) {
					aj->Poser(g.params[0].name.CStr(), 1.f);
				}
				for (int32 k = 0; k < 10; ++k) { // 0,5 s : le fondu de 0,25 s est fini
					NkEditeurAvancer(m, 0.05f);
				}
				aj = m.scene.Monde().Get<unkeny::NkAnimateur2D>(m.scene.EntiteParUid(uid));
				const NkString apres = aj != nullptr ? unkeny::NkEtatAnimateur2D(*aj) : NkString();
				T.Trame();
				const bool allume = g.live && g.liveState == b && g.firedTransition == t;
				const nkgui::NkRect arreter = ui.pagesAnim.boutons[(uint8)NkBoutonPage::NK_JOUER];
				T.Clic(0, Centre(arreter).x, Centre(arreter).y);
				Temoin(porte && enJeu && avant == NkString("idle") && avantVu && apres == g.Node(b)->name && allume &&
						   m.etat == NkEtatJeu::NK_EDITION,
					   "(b5) Appliquer, Jouer, Reel = 1 : la transition tire, la page allume l'etat");
				// (b6) les onglets.
				m.selection = m.scene.EntiteParUid(uid);
				m.aSelection = true;
				NkEditeurExecuter(cadre, NK_A_ANIM_ANIMATION);
				T.Trame();
				const bool deuxOnglets = ui.pagesAnim.onglets.Size() == 2u;
				const nkgui::NkRect scene = ui.ongletScene;
				T.Clic(0, scene.x + 20.f, scene.y + scene.h * 0.5f);
				T.Trame();
				const bool sceneDevant = !NkEditeurPageAnimOuverte(ui) && ui.viseur.w > 0.f;
				const nkgui::NkRect premier = ui.pagesAnim.onglets[0];
				T.Clic(0, premier.x + 30.f, premier.y + premier.h * 0.5f);
				const bool premierDevant =
					NkEditeurDocAnimActif(ui) != nullptr && NkEditeurDocAnimActif(ui)->genre == NkGenreDocAnim::NK_ANIMATEUR;
				const nkgui::NkRect croix = ui.pagesAnim.croix[1];
				T.Clic(0, Centre(croix).x, Centre(croix).y);
				T.Trame();
				Temoin(deuxOnglets && sceneDevant && premierDevant && ui.pagesAnim.docs.Size() == 1u,
					   "(b6) onglets : la scene revient, un document revient, sa croix le ferme");
				memory::NkGetDefaultAllocator().Delete(pt);
				memory::NkGetDefaultAllocator().Delete(pm);
				NkDirectory::Delete("banc_animateur", true);
			}
			// =================================================================
			// (01/10 soir) LA SEQUENCE : pistes de clips, muet, tangentes
			// =================================================================
			anim::NkAnimationClip ClipPosition(const char *nom, float32 x) {
				anim::NkAnimationClip c;
				c.name = nom;
				c.duration = 1.f;
				anim::NkAnimationClip::NkPropertyTrack &p =
					c.AddPropertyTrack("", "Transform.position", anim::NkAnimationClip::NkPropertyKind::NK_VEC2);
				p.curve.AddKey(0.f, math::NkVec4f(x, 0.f, 0.f, 0.f));
				p.curve.AddKey(1.f, math::NkVec4f(x, 0.f, 0.f, 0.f));
				return c;
			}

			void BancPageSequence() {
				// (c1) les conversions.
				{
					editorkit::NkTimelineModel f;
					f.duration = 2.f;
					f.AddTrack(1, "", "Transform.rotation", editorkit::NkTimelineValueKind::Nombre, 1);
					const float32 a[4] = {0.f}, b[4] = {90.f}, z[4] = {0.f};
					f.SetKey(1, 0.f, a, (uint8)editorkit::NkTimelineInterp::Courbe);
					f.SetKey(1, 1.f, b, (uint8)editorkit::NkTimelineInterp::Courbe);
					f.SetKey(1, 2.f, z);
					f.Track(1)->keys[1].tangent = (uint8)editorkit::NkTimelineTangent::Unifiee;
					f.Track(1)->keys[1].tanIn[0] = f.Track(1)->keys[1].tanOut[0] = 40.f;
					f.Track(1)->muted = true;
					f.AddTrack(2, "", "Clips", editorkit::NkTimelineValueKind::Clips, 1);
					const nk_uint64 ca = f.AddClip(2, "banc_seq_a", 0.f, 1.f);
					const nk_uint64 cb = f.AddClip(2, "banc_seq_b", 0.5f, 1.f);
					f.Clip(2, ca)->blendOut = 0.5f;
					f.Clip(2, cb)->blendIn = 0.5f;
					anim::NkAnimationClip clip;
					NkClipDepuisFrise(f, clip);
					const bool versClip = clip.clipTracks.Size() == 1u && clip.clipTracks[0].strips.Size() == 2u &&
										  Pres(clip.clipTracks[0].strips[1].blendIn, 0.5f) && clip.propertyTracks.Size() == 1u &&
										  !clip.propertyTracks[0].curve.enabled && clip.propertyTracks[0].tangents.Size() == 3u &&
										  clip.propertyTracks[0].tangents[1].mode == anim::NkTangentMode::NK_USER;
					editorkit::NkTimelineModel g;
					NkFriseDepuisClip(clip, g);
					const editorkit::NkTimelineTrack *pc = g.tracks.Size() == 2u ? &g.tracks[1] : nullptr;
					const bool versFrise = pc != nullptr && pc->kind == editorkit::NkTimelineValueKind::Clips && pc->clips.Size() == 2u &&
										   Pres(pc->clips[1].start, 0.5f) && g.tracks[0].muted &&
										   g.tracks[0].keys[1].tangent == (uint8)editorkit::NkTimelineTangent::Unifiee &&
										   Pres(g.tracks[0].keys[1].tanOut[0], 40.f);
					Temoin(versClip && versFrise, "(c1) piste de clips, piste muette, tangente : frise -> clip -> frise");
				}
				// (c2) et (c3) dans la page.
				unkeny::NkEnregistrerClipProprietes("banc_seq_a", ClipPosition("banc_seq_a", 0.f));
				unkeny::NkEnregistrerClipProprietes("banc_seq_b", ClipPosition("banc_seq_b", 1.f));
				NkEditeurModele *pm = NouveauModele("banc_seq");
				NkEditeurModele &m = *pm;
				const ecs::NkEntityId heros = PoserHeros(m);
				const NkVec2f p0 = PositionDe(m, heros);
				NkEditeurBancTrame *pt = memory::NkGetDefaultAllocator().New<NkEditeurBancTrame>(m);
				NkEditeurBancTrame &T = *pt;
				Agrandir(T);
				NkEditeurInterface &ui = T.Ui();
				T.Trame();
				NkEditeurCadre cadre = T.Cadre();
				NkEditeurExecuter(cadre, NK_A_ANIM_ANIMATION);
				T.Trame();
				T.Trame();
				NkDocAnim *d = NkEditeurDocAnimActif(ui);
				if (d == nullptr) {
					Temoin(false, "(c2) la page Animation s'ouvre");
					memory::NkGetDefaultAllocator().Delete(pt);
					memory::NkGetDefaultAllocator().Delete(pm);
					return;
				}
				// (c3) « + Piste » -> « Piste de clips » -> son « + » -> un clip.
				const editorkit::NkPaintRect ajout = ui.pagesAnim.frise.buttons[(uint8)editorkit::NkTimelineButton::AddTrack];
				T.Clic(0, Centre(ajout).x, Centre(ajout).y);
				T.Trame();
				const bool premiere = !d->propositions.Empty() && d->propositions[0].pisteClips;
				if (premiere) {
					const nkgui::NkRect r = d->propositions[0].rect;
					T.Clic(0, Centre(r).x, Centre(r).y);
				}
				T.Trame();
				const editorkit::NkTimelineTrack *pc = d->frise.tracks.Empty() ? nullptr : &d->frise.tracks[0];
				const bool piste = pc != nullptr && pc->kind == editorkit::NkTimelineValueKind::Clips;
				const editorkit::NkTimelineRow *ligne = piste ? Ligne(ui, pc->id) : nullptr;
				int32 kA = -1;
				if (ligne != nullptr) {
					T.Clic(0, Centre(ligne->keyButton).x, Centre(ligne->keyButton).y);
					T.Trame();
					for (uint32 i = 0; i < (uint32)d->propositions.Size(); ++i) {
						if (d->propositions[i].clip == NkString("banc_seq_a")) {
							kA = (int32)i;
						}
					}
				}
				const bool choixClips = d->choix && d->choixClips && kA >= 0;
				if (kA >= 0) {
					const nkgui::NkRect r = d->propositions[(uint32)kA].rect;
					T.Clic(0, Centre(r).x, Centre(r).y);
				}
				T.Trame();
				pc = d->frise.tracks.Empty() ? nullptr : &d->frise.tracks[0];
				const bool pose = pc != nullptr && pc->clips.Size() == 1u && pc->clips[0].name == NkString("banc_seq_a") &&
								  Pres(pc->clips[0].length, 1.f) && !d->choix;
				Temoin(premiere && piste && choixClips && pose,
					   "(c3) « + Piste » : piste de clips ; son « + » propose les animations ; un clic pose", (float32)d->propositions.Size());
				// (c2) le second clip, en fondu croise, puis le jeu et l'apercu.
				if (pc != nullptr && pose) {
					const nk_uint64 idA = pc->clips[0].id;
					const nk_uint64 idB = d->frise.AddClip(pc->id, "banc_seq_b", 0.5f, 1.f, 1.f);
					d->frise.Clip(pc->id, idA)->blendOut = 0.5f;
					d->frise.Clip(pc->id, idB)->blendIn = 0.5f;
					d->frise.Touch();
				}
				d->frise.SetCursor(0.75f);
				T.Trame();
				T.Trame();
				const NkVec2f apresTrame = PositionDe(m, heros);
				NkClipDepuisFrise(d->frise, d->clip);
				unkeny::NkAppliquerClipProprietes(m.scene, heros, d->clip, 0.75f);
				const NkVec2f joue = PositionDe(m, heros);
				Temoin(Pres(apresTrame.x, p0.x) && Pres(joue.x, 0.5f, 0.01f) && d->clip.clipTracks.Size() == 1u,
					   "(c2) la sequence en jeu : mi-fondu a 0,75 s (x 0,5) ; l'apercu ne laisse rien", joue.x);
				memory::NkGetDefaultAllocator().Delete(pt);
				memory::NkGetDefaultAllocator().Delete(pm);
				NkDirectory::Delete("banc_seq", true);
			}
			void BancGrapheControleur() {
				editorkit::NkStateGraphModel g;
				g.AddParam("vitesse", (uint8)editorkit::NkGraphParamKind::Float);
				const nk_uint64 loco = g.AddBlendTree("Locomotion", 0, 0.f, 0.f);
				g.AddBlendSample(loco, "repos", 0.f);
				g.AddBlendSample(loco, "course", 3.f);
				const nk_uint64 saut = g.AddState("Saut", 0, 300.f, 0.f, "saut");
				const nk_uint64 t = g.AddTransition(loco, saut);
				g.Transition(t)->curve = 1;
				const nk_uint64 tir = g.AddLayer("Tir");
				g.Node(tir)->layerAdditive = true;
				g.Node(tir)->layerWeight = 0.5f;
				g.Node(tir)->layerMask = "Torse";
				g.AddState("Viser", tir, 0.f, 0.f, "visee");
				anim::NkAnimController ctl;
				NkVector<nk_uint64> nds;
				const bool compile = NkControleurDepuisGraphe(g, ctl, nds);
				const anim::NkBlendSpaceDef *bs = ctl.FindBlendSpace("Locomotion");
				const bool vers = compile && ctl.base.GetStateCount() == 2 && ctl.base.GetStateRefKind(0) == 2 && bs != nullptr &&
								  bs->samples.Size() == 2u && bs->paramX == NkString("vitesse") && ctl.layers.Size() == 1u &&
								  ctl.layers[0].mode == anim::NkLayerMode::NK_ADDITIVE && ctl.layers[0].machine.GetStateCount() == 1 &&
								  ctl.FindMask("Torse") != nullptr && ctl.base.GetTransitionCurve(0) == anim::NkFadeCurve::NK_SMOOTH;
				editorkit::NkStateGraphModel h;
				NkVector<nk_uint64> nds2;
				NkGrapheDepuisControleur(ctl, h, nds2);
				uint32 couches = 0, arbres = 0, viser = 0;
				for (uint32 i = 0; i < (uint32)h.nodes.Size(); ++i) {
					couches += h.nodes[i].layerRoot ? 1u : 0u;
					arbres += h.nodes[i].blendTree && h.nodes[i].samples.Size() == 2u ? 1u : 0u;
					viser += h.nodes[i].name == NkString("Viser") && h.LayerOf(h.nodes[i].id) != 0 ? 1u : 0u;
				}
				const bool retour = couches == 1u && arbres == 1u && viser == 1u && h.transitions.Size() == 1u && h.transitions[0].curve == 1u;
				ctl.SaveBinary("banc_ctl_melange.nkanimctl");
				const bool lu = unkeny::NkChargerModeleAnimateur("banc_ctl_melange", "banc_ctl_melange.nkanimctl");
				const anim::NkAnimController *rl = unkeny::NkControleurAnimateur("banc_ctl_melange");
				NkFile::Delete("banc_ctl_melange.nkanimctl");
				Temoin(vers && retour && lu && rl != nullptr && rl->layers.Size() == 1u && rl->FindBlendSpace("Locomotion") != nullptr,
					   "(b7) couche, arbre 1D, courbe : graphe -> controleur -> graphe ; relu par Unkeny");
			}
		} // namespace

		int32 NkEditeurLancerBancAnimation() {
			gE = gR = 0;
			std::printf("\nUnkenyEditor — banc des pages Animation et Animateur (01/10)\n\n");
			BancPageAnimation();
			BancPageSequence();
			BancPageAnimateur();
			BancGrapheControleur();
			std::printf("\nBANC ANIMATION %s : %d reussis, %d echec\n", gE == 0 ? "REUSSI" : "ECHOUE", gR, gE);
			return gE == 0 ? 0 : 1;
		}

		int32 NkEditeurCapturesAnimation(const char *dossier) {
			NkDirectory::CreateRecursive(dossier);
			NkEditeurModele *pm = NouveauModele("captures_anim");
			NkEditeurModele &m = *pm;
			const ecs::NkEntityId heros = PoserHeros(m);
			m.scene.Monde().Get<NkTransform2D>(heros)->position = NkVec2f(0.f, 0.f);
			const ecs::NkEntityId lanterne = NkEditeurCreerEntite(m, "Lanterne", NkVec2f(0.6f, 0.4f));
			NkSprite2D sl;
			sl.taille = NkVec2f(0.35f, 0.35f);
			sl.couleur = 0xF2C14EFFu;
			m.scene.Monde().Add<NkSprite2D>(lanterne, sl);
			NkEditeurRattacher(m, lanterne, heros);
			m.selection = heros;
			m.aSelection = true;
			NkEditeurBancTrame *pt = memory::NkGetDefaultAllocator().New<NkEditeurBancTrame>(m);
			NkEditeurBancTrame &T = *pt;
			Agrandir(T);
			NkEditeurInterface &ui = T.Ui();
			int32 erreurs = 0;
			auto fichier = [&](const char *nom) {
				NkString c(dossier);
				c.Append("/");
				c.Append(nom);
				return c;
			};
			T.Trame();
			// 1. La page Animation : un saut, un tour, un clignement de couleur, la
			//    lanterne qui balance, un palier de visibilite.
			NkEditeurOuvrirAnimation(m, ui, nullptr);
			NkDocAnim *d = NkEditeurDocAnimActif(ui);
			if (d != nullptr) {
				editorkit::NkTimelineModel &f = d->frise;
				f.duration = 1.f;
				f.AddTrack(1, "", "Transform.position", editorkit::NkTimelineValueKind::Nombre, 2).label = "Position";
				f.AddTrack(2, "", "Transform.rotation", editorkit::NkTimelineValueKind::Nombre, 1).label = "Rotation (degres)";
				f.AddTrack(3, "", "Sprite.couleur", editorkit::NkTimelineValueKind::Couleur, 4).label = "Couleur";
				f.AddTrack(5, "", "Sprite.visible", editorkit::NkTimelineValueKind::Palier, 1).label = "Visible";
				f.AddTrack(4, "Lanterne", "Transform.position", editorkit::NkTimelineValueKind::Nombre, 2).label = "Position";
				const float32 p0[4] = {0.f, 0.f, 0.f, 0.f}, p1[4] = {0.f, 1.2f, 0.f, 0.f};
				f.SetKey(1, 0.f, p0, (uint8)editorkit::NkTimelineInterp::Sortie);
				f.SetKey(1, 0.4f, p1, (uint8)editorkit::NkTimelineInterp::Entree);
				f.SetKey(1, 0.8f, p0);
				const float32 r0[4] = {0.f}, r1[4] = {180.f}, r2[4] = {360.f};
				f.SetKey(2, 0.f, r0);
				f.SetKey(2, 0.5f, r1, (uint8)editorkit::NkTimelineInterp::EntreeSortie);
				f.SetKey(2, 1.f, r2);
				const float32 c0[4] = {0.31f, 0.64f, 0.88f, 1.f}, c1[4] = {0.95f, 0.45f, 0.2f, 1.f};
				f.SetKey(3, 0.f, c0);
				f.SetKey(3, 0.5f, c1);
				f.SetKey(3, 1.f, c0);
				const float32 l0[4] = {0.6f, 0.4f, 0.f, 0.f}, l1[4] = {0.75f, 0.2f, 0.f, 0.f};
				f.SetKey(4, 0.f, l0, (uint8)editorkit::NkTimelineInterp::EntreeSortie);
				f.SetKey(4, 0.5f, l1, (uint8)editorkit::NkTimelineInterp::EntreeSortie);
				f.SetKey(4, 1.f, l0);
				const float32 v1[4] = {1.f}, v0[4] = {0.f};
				f.SetKey(5, 0.f, v1);
				f.SetKey(5, 0.85f, v0);
				f.SetKey(5, 0.9f, v1);
				f.ClearSelection();
				f.SelectKey(1, 1, true);
				f.SelectKey(4, 1, true);
				f.SetCursor(0.4f);
				f.FrameAll();
				f.activeTrack = 1;
				for (int32 k = 0; k < 4; ++k) {
					T.Trame();
				}
				erreurs += EcrirePng(T, fichier("01_animation_feuille.png").CStr()) ? 0 : 1;
				// 2. L'editeur de courbes, sur la position du heros.
				f.curveMode = true;
				f.curveAutoFit = true;
				T.Trame();
				T.Trame();
				erreurs += EcrirePng(T, fichier("02_animation_courbes.png").CStr()) ? 0 : 1;
				// 3. Le choix « + Propriete ».
				f.curveMode = false;
				T.Trame();
				const editorkit::NkPaintRect ajout = ui.pagesAnim.frise.buttons[(uint8)editorkit::NkTimelineButton::AddTrack];
				T.Clic(0, ajout.x + ajout.w * 0.5f, ajout.y + ajout.h * 0.5f);
				T.Trame();
				erreurs += EcrirePng(T, fichier("03_animation_proprietes.png").CStr()) ? 0 : 1;
				d->choix = false;
				// (01/10 soir) 7. Les TANGENTES : la position en « Courbe », la cle du
				// sommet choisie (unifiee), dans l'editeur de courbes.
				for (uint32 k = 0; k < (uint32)f.Track(1)->keys.Size(); ++k) {
					f.Track(1)->keys[k].interp = (uint8)editorkit::NkTimelineInterp::Courbe;
				}
				f.ClearSelection();
				f.SelectKey(1, 1, false);
				f.SetSelectionTangent((uint8)editorkit::NkTimelineTangent::Unifiee);
				f.Track(1)->keys[1].tanIn[1] = f.Track(1)->keys[1].tanOut[1] = -2.f;
				f.Touch();
				f.curveMode = true;
				f.curveAutoFit = true;
				f.activeTrack = 1;
				for (int32 k = 0; k < 3; ++k) {
					T.Trame();
				}
				erreurs += EcrirePng(T, fichier("07_animation_courbes_tangentes.png").CStr()) ? 0 : 1;
				// 8. Une SEQUENCE : une piste de clips (deux animations qui se fondent,
				// une boucle), des marqueurs, la plage de lecture.
				{
					anim::NkAnimationClip marche, course;
					marche.name = "Marche";
					marche.duration = 0.4f;
					course.name = "Course";
					course.duration = 0.3f;
					unkeny::NkEnregistrerClipProprietes("Marche", marche);
					unkeny::NkEnregistrerClipProprietes("Course", course);
				}
				f.curveMode = false;
				f.ClearSelection();
				f.duration = 2.f;
				f.AddTrack(9, "", "Clips", editorkit::NkTimelineValueKind::Clips, 1).label = "Clips";
				const nk_uint64 clipA = f.AddClip(9, "Marche", 0.f, 1.1f, 0.4f);
				const nk_uint64 clipB = f.AddClip(9, "Course", 0.8f, 1.f, 0.3f);
				f.Clip(9, clipA)->blendOut = 0.3f;
				f.Clip(9, clipB)->blendIn = 0.3f;
				f.Clip(9, clipB)->weight = 0.8f;
				f.AddTrack(10, "", "Clips additifs", editorkit::NkTimelineValueKind::Clips, 1).label = "Respiration";
				f.Track(10)->additive = true;
				f.Track(10)->mask = "Lanterne";
				f.AddClip(10, "Marche", 0.2f, 1.4f, 0.4f);
				f.SelectClip(clipB, false);
				f.AddMarker(0.5f, "Impact");
				f.AddMarker(1.4f, "Fin du pas");
				f.SetPlayRange(0.1f, 1.8f);
				f.Track(5)->muted = true;
				f.Track(3)->solo = false;
				f.SetCursor(0.95f);
				f.FrameAll();
				for (int32 k = 0; k < 4; ++k) {
					T.Trame();
				}
				erreurs += EcrirePng(T, fichier("08_animation_pistes_de_clips.png").CStr()) ? 0 : 1;
			}
			// 4. La page Animateur : le modele « plateforme » d'Unkeny (Sol / Air).
			m.selection = heros;
			m.aSelection = true;
			m.scene.Monde().Add<unkeny::NkAnimateur2D>(heros, unkeny::NkCreerAnimateur2D("plateforme"));
			NkEditeurCadre cadre = T.Cadre();
			NkEditeurExecuter(cadre, NK_A_ANIM_ANIMATEUR);
			NkDocAnim *a = NkEditeurDocAnimActif(ui);
			if (a != nullptr && a->genre == NkGenreDocAnim::NK_ANIMATEUR) {
				a->graphe.clips.PushBack("Heros");
				a->graphe.clips.PushBack("marche");
				a->graphe.clips.PushBack("saut");
				for (int32 k = 0; k < 3; ++k) {
					T.Trame();
				}
				// La transition sur le declencheur « saut » choisie : l'inspecteur la montre.
				for (uint32 i = 0; i < (uint32)a->graphe.transitions.Size(); ++i) {
					if (a->graphe.transitions[i].conditions.Size() == 1u &&
						a->graphe.transitions[i].conditions[0].op == (uint8)editorkit::NkGraphCondOp::Trigger) {
						a->graphe.selectedTransition = a->graphe.transitions[i].id;
					}
				}
				T.Trame();
				erreurs += EcrirePng(T, fichier("04_animateur_graphe.png").CStr()) ? 0 : 1;
				// 5. Dans la sous-machine « Sol », EN JEU : l'etat actif s'allume.
				if (const editorkit::NkGraphNode *sol = a->graphe.FindByName("Sol", 0)) {
					a->graphe.level = sol->id;
					a->graphe.selectedTransition = 0;
					a->graphe.selectedNode = 0;
					a->grapheCadre = false;
				}
				const uint64 uid = m.scene.Uid(heros);
				NkEditeurJouer(m);
				NkEditeurAvancer(m, 0.05f);
				if (unkeny::NkAnimateur2D *an = m.scene.Monde().Get<unkeny::NkAnimateur2D>(m.scene.EntiteParUid(uid))) {
					an->Poser("vitesse", 2.f);
				}
				NkEditeurAvancer(m, 0.05f);
				for (int32 k = 0; k < 3; ++k) {
					T.Trame();
				}
				erreurs += EcrirePng(T, fichier("05_animateur_en_jeu.png").CStr()) ? 0 : 1;
				NkEditeurArreter(m);
				// (01/10 soir) 9. Un ARBRE DE MELANGE a la racine (marche / course sur
				// « vitesse »), choisi : l'inspecteur montre ses animations.
				editorkit::NkStateGraphModel &g = a->graphe;
				g.level = 0;
				g.selectedTransition = 0;
				const nk_uint64 arbre = g.AddBlendTree("Locomotion", 0, 170.f, 210.f);
				g.AddBlendSample(arbre, "Heros", 0.f);
				g.AddBlendSample(arbre, "marche", 1.f);
				g.AddBlendSample(arbre, "saut", 3.f);
				g.selectedNode = arbre;
				a->grapheCadre = false;
				for (int32 k = 0; k < 3; ++k) {
					T.Trame();
				}
				erreurs += EcrirePng(T, fichier("09_animateur_arbre_de_melange.png").CStr()) ? 0 : 1;
				// 10. Une COUCHE « Tir » (le haut du corps), ouverte : ses reglages.
				g.masks.PushBack("Lanterne");
				const nk_uint64 couche = g.AddLayer("Tir");
				g.Node(couche)->layerWeight = 0.75f;
				g.Node(couche)->layerMask = "Lanterne";
				g.AddState("Viser", couche, 0.f, 0.f, "marche");
				g.AddState("Tirer", couche, 320.f, 0.f, "saut");
				g.level = couche;
				g.selectedNode = 0;
				a->grapheCadre = false;
				for (int32 k = 0; k < 3; ++k) {
					T.Trame();
				}
				erreurs += EcrirePng(T, fichier("10_animateur_couche.png").CStr()) ? 0 : 1;
			}
			// 6. Le THEME CLAIR : la page Animation.
			if (d != nullptr && !ui.pagesAnim.docs.Empty()) {
				T.theme = editorkit::NkTheme::Light();
				T.pal = NkEditeurPalette(T.theme);
				NkEditeurActiverDocument(m, ui, NkDoc(NkGenreDocument::NK_ANIM, ui.pagesAnim.docs[0].id));
				for (int32 k = 0; k < 3; ++k) {
					T.Trame();
				}
				erreurs += EcrirePng(T, fichier("06_animation_theme_clair.png").CStr()) ? 0 : 1;
			}
			memory::NkGetDefaultAllocator().Delete(pt);
			memory::NkGetDefaultAllocator().Delete(pm);
			NkDirectory::Delete("captures_anim", true);
			return erreurs == 0 ? 0 : 1;
		}

	} // namespace editeur
} // namespace nkentseu
