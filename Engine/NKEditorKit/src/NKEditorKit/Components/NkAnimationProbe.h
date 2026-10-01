#pragma once
// -----------------------------------------------------------------------------
// @File    NkAnimationProbe.h
// @Brief   Le banc de LA FRISE et DU GRAPHE D'ETATS (famille 31 de
//          NKEditorKitTest) : les modeles, puis les GESTES rejoues sur le
//          dessin, dans le peintre enregistreur -- sans fenetre.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
//  PRE-ENREGISTREMENT (2026-10-01, ecrit avant le premier lancement) :
//   frise   t1 poser une cle l'accroche a l'image et la remplace sur la meme image
//           t2 choisir, deplacer (fusion sur la meme image), supprimer ; annuler, refaire
//           t3 l'evaluation de repli : lineaire, paliers
//           t4 la lecture : boucle et une fois
//           t5 GESTE : un losange tenu et glisse de 10 images bouge sa cle de 10 images
//           t6 GESTE : un double-clic sur une piste y pose une cle
//           t7 GESTE : la regle frottee pose le curseur ; une case d'interpolation
//              change celle des cles choisies
//   graphe  g1 etats, transition, condition au genre du parametre, resume, entree
//           g2 retirer un etat emporte ses transitions ; annuler le rend ; sous-machine
//           g3 renommer un parametre suit dans les conditions ; le retirer les retire
//           g4 GESTE : double-clic sur le fond = un etat a ce point
//           g5 GESTE : glisser du rond de A vers B = la transition A -> B
//           g6 GESTE : « + Condition » puis un clic sur l'operateur le fait tourner
//           g7 GESTE : un etat glisse, et annuler le remet
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkRecordingPaint.h"
#include "NKEditorKit/Components/NkStateGraphModel.h"
#include "NKEditorKit/Components/NkTimelineModel.h"

#include <cmath>
#include <cstdio>

namespace nkentseu {
	namespace editorkit {
		namespace animprobe {

			struct Bilan {
					uint32 ok = 0;
					uint32 total = 0;
			};

			inline void Note(Bilan &b, bool ok, const char *quoi) {
				++b.total;
				b.ok += ok ? 1u : 0u;
				std::printf("  [%s] %s\n", ok ? " OK " : "ECHEC", quoi);
			}

			inline bool Pres(float32 a, float32 b, float32 tol = 1e-4f) {
				return std::fabs(a - b) <= tol;
			}

			/// Un hote minimal : le peintre enregistreur, une entree, une trame.
			struct HoteFrise {
					NkRecordingPaint p;
					NkComponentInput in;
					NkTimelineStyle s;
					NkTimelineHooks h;
					NkPaintRect rect{0.f, 0.f, 1280.f, 400.f};
					NkTimelineResult r;
					HoteFrise() {
						// Des roles DISTINCTS : le peintre enregistreur les rend injectifs.
						uint16 k = 1;
						s.panelBg = k++;
						s.headerBg = k++;
						s.rowAltBg = k++;
						s.border = k++;
						s.grid = k++;
						s.text = k++;
						s.textMuted = k++;
						s.accent = k++;
						s.textOnAccent = k++;
						s.key = k++;
						s.keySelected = k++;
						s.playhead = k++;
						for (int32 c = 0; c < 4; ++c) {
							s.channel[c] = k++;
						}
						s.buttonBg = k++;
						s.inputBg = k++;
						in.mouseX = -100.f;
						in.mouseY = -100.f;
					}
					void Trame(NkTimelineModel &m) {
						p.Reset();
						r = NkDrawTimeline(p, in, rect, m, s, h);
						in.mousePressed = false;
						in.mouseReleased = false;
						in.doubleClick = false;
						in.wheel = 0.f;
					}
					void Appui(NkTimelineModel &m, float32 x, float32 y) {
						in.mouseX = x;
						in.mouseY = y;
						in.mouseDown = true;
						in.mousePressed = true;
						Trame(m);
					}
					void Aller(NkTimelineModel &m, float32 x, float32 y) {
						in.mouseX = x;
						in.mouseY = y;
						Trame(m);
					}
					void Relache(NkTimelineModel &m) {
						in.mouseDown = false;
						in.mouseReleased = true;
						Trame(m);
					}
					void Clic(NkTimelineModel &m, float32 x, float32 y) {
						Appui(m, x, y);
						Relache(m);
					}
					/// Le centre de la ligne de la piste `id`, -1 si elle n'est pas montree.
					float32 YLigne(nk_uint64 id) const {
						for (uint32 k = 0; k < (uint32)r.rows.Size(); ++k) {
							if (r.rows[k].track == id) {
								return r.rows[k].y + r.rows[k].h * 0.5f;
							}
						}
						return -1.f;
					}
			};

			inline NkTimelineModel FriseDeReference() {
				NkTimelineModel m;
				m.fps = 30.f;
				m.duration = 2.f;
				m.undoStack.Clear();
				const float32 v0[4] = {0.f, 0.f, 0.f, 0.f};
				const float32 v1[4] = {10.f, 5.f, 0.f, 0.f};
				m.AddTrack(1, "", "Transform.position", NkTimelineValueKind::Nombre, 2);
				m.AddTrack(2, "", "Sprite.visible", NkTimelineValueKind::Palier, 1);
				m.SetKey(1, 0.f, v0);
				m.SetKey(1, 1.f, v1);
				const float32 b1[4] = {1.f, 0.f, 0.f, 0.f};
				const float32 b0[4] = {0.f, 0.f, 0.f, 0.f};
				m.SetKey(2, 0.f, b1);
				m.SetKey(2, 0.5f, b0);
				m.undoStack.Clear();
				m.FrameAll();
				return m;
			}

			inline void FamilleFriseModele(Bilan &b) {
				std::printf("  -- frise : le modele --\n");
				{
					NkTimelineModel m = FriseDeReference();
					const float32 v[4] = {3.f, 0.f, 0.f, 0.f};
					const int32 k = m.SetKey(1, 0.51f, v); // 15,3 images -> la 15e
					const NkTimelineTrack *tr = m.Track(1);
					const bool accroche = k >= 0 && Pres(tr->keys[(uint32)k].time, 0.5f);
					m.SetKey(1, 0.505f, v); // la MEME image : remplace
					Note(b, accroche && tr->keys.Size() == 3 && m.KeyAt(*tr, 0.5f) == 1, "t1 une cle s'accroche a l'image, et la meme image la remplace");
				}
				{
					NkTimelineModel m = FriseDeReference();
					m.SelectKey(1, 1, false);			 // la cle a 1 s
					m.SelectKey(2, 1, true);			 // et celle a 0,5 s
					const uint32 avant = m.revision;
					m.PushUndo();
					m.MoveSelection(-0.5f); // 1 s -> 0,5 s ; 0,5 s -> 0 s (sur la cle de 0 : fusion)
					m.Normalize();
					const NkTimelineTrack *a = m.Track(1);
					const NkTimelineTrack *c = m.Track(2);
					const bool deplace = a->keys.Size() == 2 && Pres(a->keys[1].time, 0.5f);
					const bool fondu = c->keys.Size() == 1 && Pres(c->keys[0].v[0], 0.f); // la choisie l'emporte
					const bool supprime = m.DeleteSelection() && a->keys.Size() == 1 && c->keys.Empty();
					const bool annule = m.Undo() && m.Track(1)->keys.Size() == 2 && m.Undo() && m.Track(2)->keys.Size() == 2 &&
										Pres(m.Track(1)->keys[1].time, 1.f);
					const bool refait = m.Redo() && Pres(m.Track(1)->keys[1].time, 0.5f);
					Note(b, deplace && fondu && supprime && annule && refait && m.revision != avant,
						 "t2 choisir, deplacer (fusion), supprimer, annuler, refaire");
				}
				{
					NkTimelineModel m = FriseDeReference();
					float32 o[4];
					const bool lin = m.EvaluateFallback(*m.Track(1), 0.5f, o) && Pres(o[0], 5.f) && Pres(o[1], 2.5f);
					const bool pal = m.EvaluateFallback(*m.Track(2), 0.49f, o) && Pres(o[0], 1.f) &&
									 m.EvaluateFallback(*m.Track(2), 0.5f, o) && Pres(o[0], 0.f);
					NkTimelineTrack vide;
					Note(b, lin && pal && !m.EvaluateFallback(vide, 0.f, o), "t3 evaluation de repli : lineaire et paliers");
				}
				{
					NkTimelineModel m = FriseDeReference();
					m.playing = true;
					m.loop = true;
					m.SetCursor(1.9f);
					m.Advance(0.2f);
					const bool boucle = Pres(m.cursor, 0.1f, 1e-3f) && m.playing;
					m.loop = false;
					m.SetCursor(1.9f);
					m.Advance(0.2f);
					Note(b, boucle && Pres(m.cursor, 2.f) && !m.playing, "t4 lecture : boucle, puis une fois (arret au bout)");
				}
			}

			inline void FamilleFriseGestes(Bilan &b) {
				std::printf("  -- frise : les gestes --\n");
				{
					NkTimelineModel m = FriseDeReference();
					HoteFrise h;
					h.Trame(m);
					const float32 y = h.YLigne(1);
					const float32 x = NkTimelineTimeToX(m, h.r.area, 1.f);
					const float32 x2 = NkTimelineTimeToX(m, h.r.area, 1.f + 10.f / 30.f);
					h.Appui(m, x, y);
					const bool choisie = m.Track(1)->keys[1].selected;
					for (int32 k = 1; k <= 4; ++k) {
						h.Aller(m, x + (x2 - x) * (float32)k / 4.f, y);
					}
					h.Relache(m);
					const bool bouge = Pres(m.Track(1)->keys[1].time, 1.f + 10.f / 30.f, 1e-3f);
					const bool annulable = m.Undo() && Pres(m.Track(1)->keys[1].time, 1.f);
					Note(b, y > 0.f && choisie && bouge && annulable && h.r.rows.Size() == 3,
						 "t5 un losange glisse de 10 images bouge sa cle de 10 images (annulable)");
				}
				{
					NkTimelineModel m = FriseDeReference();
					HoteFrise h;
					h.Trame(m);
					const float32 y = h.YLigne(1);
					const float32 x = NkTimelineTimeToX(m, h.r.area, 1.5f);
					h.Appui(m, x, y);
					h.Relache(m);
					h.in.doubleClick = true;
					h.Appui(m, x, y);
					h.Relache(m);
					const NkTimelineTrack *tr = m.Track(1);
					const int32 k = m.KeyAt(*tr, 1.5f);
					Note(b, tr->keys.Size() == 3 && k == 2 && Pres(tr->keys[2].v[0], 10.f) && tr->keys[2].selected,
						 "t6 un double-clic sur une piste y pose une cle (valeur de la courbe), choisie");
				}
				{
					NkTimelineModel m = FriseDeReference();
					HoteFrise h;
					h.Trame(m);
					const float32 x = NkTimelineTimeToX(m, h.r.area, 0.4f);
					h.Clic(m, x, h.r.ruler.y + h.r.ruler.h * 0.5f);
					const bool curseur = Pres(m.cursor, 0.4f, 1e-3f) && m.FrameOf(m.cursor) == 12;
					m.SelectKey(1, 0, false);
					m.SelectKey(1, 1, true);
					const NkPaintRect bt = h.r.buttons[(uint8)NkTimelineButton::InterpFirst + (uint8)NkTimelineInterp::Sortie];
					h.Clic(m, bt.x + bt.w * 0.5f, bt.y + bt.h * 0.5f);
					const bool interp = m.Track(1)->keys[0].interp == (uint8)NkTimelineInterp::Sortie &&
										m.Track(1)->keys[1].interp == (uint8)NkTimelineInterp::Sortie &&
										m.SelectionInterp() == (uint8)NkTimelineInterp::Sortie;
					Note(b, bt.w > 0.f && curseur && interp, "t7 la regle pose le curseur ; une case d'interpolation change la selection");
				}
			}

			struct HoteGraphe {
					NkRecordingPaint p;
					NkComponentInput in;
					NkStateGraphStyle s;
					NkPaintRect rect{0.f, 0.f, 1200.f, 600.f};
					NkStateGraphResult r;
					HoteGraphe() {
						uint16 k = 1;
						s.canvasBg = k++;
						s.gridLine = k++;
						s.panelBg = k++;
						s.headerBg = k++;
						s.border = k++;
						s.text = k++;
						s.textMuted = k++;
						s.textOnAccent = k++;
						s.accent = k++;
						s.nodeBody = k++;
						s.nodeHeader = k++;
						s.nodeEntry = k++;
						s.subMachine = k++;
						s.wire = k++;
						s.pseudoEntry = k++;
						s.pseudoAny = k++;
						s.paramBool = k++;
						s.paramFloat = k++;
						s.paramTrigger = k++;
						s.buttonBg = k++;
						s.inputBg = k++;
						in.mouseX = -100.f;
						in.mouseY = -100.f;
					}
					void Trame(NkStateGraphModel &m) {
						p.Reset();
						r = NkDrawStateGraph(p, in, rect, m, s);
						in.mousePressed = false;
						in.mouseReleased = false;
						in.doubleClick = false;
						in.wheel = 0.f;
					}
					void Appui(NkStateGraphModel &m, float32 x, float32 y) {
						in.mouseX = x;
						in.mouseY = y;
						in.mouseDown = true;
						in.mousePressed = true;
						Trame(m);
					}
					void Aller(NkStateGraphModel &m, float32 x, float32 y) {
						in.mouseX = x;
						in.mouseY = y;
						Trame(m);
					}
					void Relache(NkStateGraphModel &m) {
						in.mouseDown = false;
						in.mouseReleased = true;
						Trame(m);
					}
					void Clic(NkStateGraphModel &m, float32 x, float32 y) {
						Appui(m, x, y);
						Relache(m);
					}
					void Glisser(NkStateGraphModel &m, float32 x0, float32 y0, float32 x1, float32 y1) {
						Appui(m, x0, y0);
						for (int32 k = 1; k <= 4; ++k) {
							Aller(m, x0 + (x1 - x0) * (float32)k / 4.f, y0 + (y1 - y0) * (float32)k / 4.f);
						}
						Relache(m);
					}
					const NkGraphNodeRect *Noeud(nk_uint64 id) const {
						for (uint32 i = 0; i < (uint32)r.nodeRects.Size(); ++i) {
							if (r.nodeRects[i].id == id) {
								return &r.nodeRects[i];
							}
						}
						return nullptr;
					}
					const NkGraphHit *Champ(NkGraphField f, int32 index = -1) const {
						for (uint32 i = 0; i < (uint32)r.hits.Size(); ++i) {
							if (r.hits[i].field == f && (index < 0 || r.hits[i].index == index)) {
								return &r.hits[i];
							}
						}
						return nullptr;
					}
			};

			inline void FamilleGrapheModele(Bilan &b) {
				std::printf("  -- graphe : le modele --\n");
				{
					NkStateGraphModel m;
					const int32 pv = m.AddParam("vitesse", (uint8)NkGraphParamKind::Float);
					const int32 ps = m.AddParam("auSol", (uint8)NkGraphParamKind::Bool, 1.f);
					const nk_uint64 idle = m.AddState("idle", 0, 0.f, 0.f);
					const nk_uint64 marche = m.AddState("marche", 0, 250.f, 0.f);
					const nk_uint64 t = m.AddTransition(idle, marche);
					m.AddCondition(t, "vitesse", 255, 0.1f);
					m.AddCondition(t, "auSol");
					const NkGraphTransition *tr = m.Transition(t);
					const bool ops = tr != nullptr && tr->conditions.Size() == 2 &&
									 tr->conditions[0].op == (uint8)NkGraphCondOp::Greater &&
									 tr->conditions[1].op == (uint8)NkGraphCondOp::IsTrue;
					const NkString res = tr != nullptr ? m.Summary(*tr) : NkString();
					const bool entree = m.EntryOf(0) == idle;
					m.SetEntry(0, marche);
					Note(b, pv == 0 && ps == 1 && ops && res == NkString("vitesse > 0.10 ET auSol") && entree &&
								m.EntryOf(0) == marche && m.AddTransition(idle, idle) == 0,
						 "g1 etats, transition, conditions au genre du parametre, resume, entree");
				}
				{
					NkStateGraphModel m;
					const nk_uint64 sol = m.AddSubMachine("Sol", 0, 0.f, 0.f);
					const nk_uint64 idle = m.AddState("idle", sol, 0.f, 0.f);
					const nk_uint64 saut = m.AddState("saut", 0, 300.f, 0.f);
					m.AddTransition(idle, saut);
					m.AddAnyTransition(sol, idle);
					const bool hier = m.PathOf(idle) == NkString("Sol/idle") && m.RepresentativeAt(idle, 0) == sol &&
									  m.RepresentativeAt(saut, sol) == 0 && m.EntryOf(sol) == idle;
					m.RemoveNode(sol);
					const bool parti = m.nodes.Size() == 1 && m.transitions.Empty();
					const bool rendu = m.Undo() && m.nodes.Size() == 3 && m.transitions.Size() == 2;
					Note(b, hier && parti && rendu, "g2 sous-machine, chemin ; retirer emporte les transitions ; annuler les rend");
				}
				{
					NkStateGraphModel m;
					m.AddParam("v", (uint8)NkGraphParamKind::Float);
					const nk_uint64 a = m.AddState("a", 0, 0.f, 0.f);
					const nk_uint64 c = m.AddState("c", 0, 1.f, 0.f);
					const nk_uint64 t = m.AddTransition(a, c);
					m.AddCondition(t);
					const bool renomme = m.RenameParam(0, "vitesse") && m.Transition(t)->conditions[0].param == NkString("vitesse");
					const bool retire = m.RemoveParam(0) && m.Transition(t)->conditions.Empty();
					Note(b, renomme && retire && !m.RenameNode(a, "c") && m.RenameNode(a, "attente"),
						 "g3 renommer un parametre suit dans les conditions ; le retirer les retire ; noms uniques");
				}
			}

			inline void FamilleGrapheGestes(Bilan &b) {
				std::printf("  -- graphe : les gestes --\n");
				NkStateGraphModel m;
				m.AddParam("vitesse", (uint8)NkGraphParamKind::Float);
				const nk_uint64 idle = m.AddState("idle", 0, -150.f, 0.f);
				m.undoStack.Clear();
				HoteGraphe h;
				h.Trame(m);
				// g4 : double-clic sur le fond, a droite de idle.
				float32 sx = 0.f, sy = 0.f;
				NkStateGraphToScreen(m, h.r.canvas, 1.f, 150.f, 0.f, sx, sy);
				h.Clic(m, sx, sy);
				h.in.doubleClick = true;
				h.Clic(m, sx, sy);
				nk_uint64 neuf = 0;
				for (uint32 i = 0; i < (uint32)m.nodes.Size(); ++i) {
					if (m.nodes[i].id != idle) {
						neuf = m.nodes[i].id;
					}
				}
				const NkGraphNode *n = m.Node(neuf);
				Note(b, n != nullptr && Pres(n->x, 150.f, 0.5f) && Pres(n->y, 0.f, 0.5f) && m.selectedNode == neuf,
					 "g4 double-clic sur le fond : un etat a ce point, choisi");
				// g5 : du rond d'idle vers le nouvel etat.
				h.Trame(m);
				const NkGraphNodeRect *a = h.Noeud(idle);
				const NkGraphNodeRect *c = h.Noeud(neuf);
				nk_uint64 t = 0;
				if (a != nullptr && c != nullptr) {
					h.Glisser(m, a->port.x + a->port.w * 0.5f, a->port.y + a->port.h * 0.5f, c->rect.x + c->rect.w * 0.5f,
							  c->rect.y + c->rect.h * 0.5f);
					t = m.selectedTransition;
				}
				const NkGraphTransition *tr = m.Transition(t);
				Note(b, tr != nullptr && tr->from == idle && tr->to == neuf && m.transitions.Size() == 1,
					 "g5 glisser du rond de A vers B trace la transition A -> B");
				// g6 : « + Condition », puis l'operateur clique : > devient <.
				h.Trame(m);
				const NkGraphHit *ajout = h.Champ(NkGraphField::CondAdd);
				if (ajout != nullptr) {
					h.Clic(m, ajout->rect.x + ajout->rect.w * 0.5f, ajout->rect.y + ajout->rect.h * 0.5f);
				}
				const NkGraphHit *op = h.Champ(NkGraphField::CondOp, 0);
				const bool cond = tr != nullptr && m.Transition(t)->conditions.Size() == 1 &&
								  m.Transition(t)->conditions[0].op == (uint8)NkGraphCondOp::Greater;
				if (op != nullptr) {
					const NkPaintRect rc = op->rect;
					h.Clic(m, rc.x + rc.w * 0.5f, rc.y + rc.h * 0.5f);
				}
				Note(b, cond && op != nullptr && m.Transition(t)->conditions[0].op == (uint8)NkGraphCondOp::Less &&
							m.Transition(t)->conditions[0].param == NkString("vitesse"),
					 "g6 « + Condition » (au premier parametre), puis l'operateur tourne : > -> <");
				// g7 : glisser idle de 40 px vers le bas, puis annuler.
				h.Trame(m);
				a = h.Noeud(idle);
				const float32 y0 = m.Node(idle)->y;
				if (a != nullptr) {
					h.Glisser(m, a->rect.x + a->rect.w * 0.3f, a->rect.y + a->rect.h * 0.5f, a->rect.x + a->rect.w * 0.3f,
							  a->rect.y + a->rect.h * 0.5f + 40.f);
				}
				const bool bouge = Pres(m.Node(idle)->y, y0 + 40.f / m.zoom, 0.5f);
				const bool annule = m.Undo() && Pres(m.Node(idle)->y, y0);
				Note(b, a != nullptr && bouge && annule, "g7 un etat glisse (a l'echelle du zoom) ; annuler le remet");
			}

			inline Bilan Sonder() {
				Bilan b;
				FamilleFriseModele(b);
				FamilleFriseGestes(b);
				FamilleGrapheModele(b);
				FamilleGrapheGestes(b);
				return b;
			}

		} // namespace animprobe
	} // namespace editorkit
} // namespace nkentseu
