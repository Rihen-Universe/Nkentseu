#pragma once
// -----------------------------------------------------------------------------
// @File    NkFilePickerNavProbe.h
// @Brief   LE BANC DU RAIL DU SELECTEUR — depliage, chevron, historique. Sans
//          fenetre, sans GPU, sans souris : le peintre est un ENREGISTREUR, et
//          l'entree est une structure que l'on remplit.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  LA QUESTION QUE CHAQUE ESSAI POSE, ECRITE AVANT SA REPONSE
// =============================================================================
//  Rodolf, 06/09 : « ne reponds pas "ca marche" sur ① avec une sonde qui compte
//  des entrees emises. La sonde qui compte a deja menti trois fois cette
//  semaine. Pour ①, la question est : COMBIEN DE NIVEAUX DE PROFONDEUR DISTINCTS
//  LE RAIL DESSINE-T-IL avant et apres le depliage — et elle doit rendre 1
//  puis 2. »
//
//  ⚠️ POURQUOI UNE SONDE QUI COMPTE NE PEUT PAS Y REPONDRE, et c'est le coeur de
//     ce fichier : le rail avait un modele PLEIN et un dessin PLAT. Compter
//     `vue.folders.nodes` aurait rendu « 5 nœuds » sur un rail qui n'en peignait
//     qu'un seul niveau, parce que le peintre lisait « ouvert » la ou l'hote
//     lisait « ferme ». Le nombre aurait ete juste, la reponse fausse.
//
//  Ce banc ne regarde donc JAMAIS le modele pour repondre. Il lit LE FLUX DE
//  COMMANDES DE DESSIN (`NkRecordingPaint`) :
//    - un NIVEAU est une abscisse de LIBELLE distincte dans le rail. Le dessin
//      pose le libelle a `x = bord + marge + profondeur * pas` : deux abscisses
//      distinctes, deux profondeurs dessinees. C'est la seule grandeur qui ne
//      peut pas etre vraie pendant que l'ecran est faux ;
//    - un CHEVRON est la paire de segments d'epaisseur 1,3 que le composant
//      trace quand l'hote n'a pas d'atlas. Son APEX dit son sens : en bas =
//      « deplie », a droite = « replie » ;
//    - LA LIGNE ACTIVE est le remplissage pleine rangee au role `activeMark`.
//      C'est ce que Rodolf voit surligne, pas ce que le modele retient.
//
//  ⚠️ ON CLIQUE LA OU C'EST DESSINE. La position du clic n'est pas recalculee
//     depuis la mise en page — elle est LUE dans le flux, au centre des deux
//     segments du chevron. Un chevron peint ailleurs que la ou il se clique
//     ferait donc rougir l'essai, au lieu de passer inapercu.
//
// =============================================================================
//  CE QUE CE BANC NE PROUVE PAS — dit avec le resultat
// =============================================================================
//   - il n'ouvre AUCUNE fenetre : rien n'est prouve a l'ecran, ni couleur, ni
//     police, ni lisibilite. Les metriques de texte du peintre enregistreur sont
//     fictives et le disent (`NkRecordingPaint.h`) ;
//   - il n'exerce PAS le cadre du dialogue (`NkDrawFilePickerNav`), qui reclame
//     un `NkGuiContext` et une police reelle : il exerce la chaine
//     ETAT -> `ConstruireRail` -> `NkDrawContentBrowser` -> rail dessine, plus
//     `SuivreLeDepliage`, qui est le MEME code que le dialogue appelle ;
//   - il ne dit rien du glisser-deposer ni du dialogue d'apres-export ;
//   - ⚠️ **IL NE VOIT AUCUNE BARRE DE DEFILEMENT PEINTE** (famille 7). La barre
//     est `NkVScrollbar`, elle prend un `NkGuiContext` et une police, et c'est
//     l'hote qui l'appelle. Ce banc mesure ce dont elle depend et que le
//     composant seul possede : la gouttiere est-elle RESERVEE dans le dessin, et
//     le rectangle rapporte tombe-t-il exactement dessus. **Que le pouce se voie
//     et se glisse a l'ecran reste un geste humain, et il est demande comme
//     tel.**
//
//  REGIME COUVERT : un seul arbre de dossiers fabrique sur disque (3 dossiers de
//  premier niveau, 2 sous-dossiers chacun, plus 20 favoris pour la famille 7),
//  panneau 900x520, echelle 1.0, variante grille, filtre vide. Le DEFILEMENT du
//  rail est desormais couvert (famille 7), a la molette et par reconstruction.
//  NON couvert : rail vide, chemin illisible, profondeur > 3, glissement du
//  pouce de la barre.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkFilePickerNav.h"
#include "NKEditorKit/Components/NkRecordingPaint.h"

#include <cstdio>

namespace nkentseu {
	namespace editorkit {
		namespace navprobe {

			// ── LE COMPTEUR ─────────────────────────────────────────────────────
			struct Bilan {
					int32 total = 0;
					int32 ok = 0;
			};

			inline void Verifier(Bilan &b, bool condition, const char *nom, const char *detail) {
				++b.total;
				if (condition)
					++b.ok;
				printf("  [%s] %-6s %s%s%s\n", condition ? "ok" : "ROUGE", nom,
					   condition ? "" : "", detail ? detail : "", "");
			}

			// ── LA ZONE D'ESSAI SUR DISQUE ──────────────────────────────────────
			// ⚠️ ELLE EST FABRIQUEE, PAS TROUVEE. Mesurer le rail sur « un dossier du
			//    poste » ferait dependre le resultat de la machine — et un banc dont le
			//    verdict change avec la machine n'est pas un banc.
			struct Terrain {
					NkString racine;

					bool Poser(const char *base) {
						racine = (NkPath(base) / "sonde_selecteur_rail").ToString();
						NkDirectory::Delete(racine.CStr(), true); // repartir propre
						static const char *const kFeuilles[] = {
							"alpha/a1", "alpha/a2", "beta/b1", "beta/b2", "gamma/g1", "gamma/g2"};
						for (usize i = 0; i < sizeof(kFeuilles) / sizeof(kFeuilles[0]); ++i)
							if (!NkDirectory::CreateRecursive((NkPath(racine) / kFeuilles[i]).ToString().CStr()))
								return false;
						return NkDirectory::Exists(racine.CStr());
					}

					void Retirer() {
						if (!racine.Empty())
							NkDirectory::Delete(racine.CStr(), true);
					}
			};

			// ── LA LECTURE DU FLUX ──────────────────────────────────────────────
			/// La TRANCHE du rail dans le flux : du `PushClip` dont le rectangle est celui
			/// de l'arbre jusqu'a son `PopClip` apparie.
			/// ⚠️ ELLE REFUSE DE RENDRE UNE TRANCHE VIDE : si le rail n'a pas ete peint, un
			///    « 0 niveau » serait indiscernable d'un rail replie. L'appelant verifie
			///    `trouve` AVANT de lire quoi que ce soit.
			struct Tranche {
					bool trouve = false;
					uint32 debut = 0, fin = 0; // [debut, fin)
			};

			inline bool PresqueEgal(float32 a, float32 b, float32 eps = 0.6f) {
				const float32 d = a - b;
				return d < eps && d > -eps;
			}

			inline Tranche TrancheDuRail(const NkRecordingPaint &rec, float32 railX, float32 railW) {
				Tranche t;
				for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i) {
					const NkPaintCmd &c = rec.cmds[i];
					if (c.op != NkPaintOp::PushClip || !PresqueEgal(c.x, railX)
						|| !PresqueEgal(c.w, railW))
						continue;
					int32 prof = 0;
					for (uint32 j = i; j < (uint32)rec.cmds.Size(); ++j) {
						if (rec.cmds[j].op == NkPaintOp::PushClip)
							++prof;
						else if (rec.cmds[j].op == NkPaintOp::PopClip) {
							--prof;
							if (prof == 0) {
								t.trouve = true;
								t.debut = i;
								t.fin = j;
								return t;
							}
						}
					}
					break;
				}
				return t;
			}

			/// LE NOMBRE DE NIVEAUX DE PROFONDEUR **DESSINES**. Un niveau = une abscisse de
			/// libelle distincte. On prend, par rangee (meme ordonnee), le texte le plus a
			/// GAUCHE : c'est le libelle ; ce qui est a sa droite est un complement de type.
			inline int32 NiveauxDessines(const NkRecordingPaint &rec, const Tranche &t) {
				float32 ysVus[256];
				float32 xMin[256];
				uint32 n = 0;
				for (uint32 i = t.debut; i < t.fin && n < 256u; ++i) {
					const NkPaintCmd &c = rec.cmds[i];
					if (c.op != NkPaintOp::Text || c.text.Empty())
						continue;
					uint32 k = 0;
					for (; k < n; ++k)
						if (PresqueEgal(ysVus[k], c.y, 1.0f))
							break;
					if (k == n) {
						ysVus[n] = c.y;
						xMin[n] = c.x;
						++n;
					} else if (c.x < xMin[k])
						xMin[k] = c.x;
				}
				float32 distinct[256];
				uint32 d = 0;
				for (uint32 i = 0; i < n; ++i) {
					uint32 k = 0;
					for (; k < d; ++k)
						if (PresqueEgal(distinct[k], xMin[i], 1.0f))
							break;
					if (k == d && d < 256u)
						distinct[d++] = xMin[i];
				}
				return (int32)d;
			}

			/// LES RANGEES DU RAIL, dans l'ordre du dessin : (ordonnee, abscisse du libelle
			/// le plus a gauche, libelle). C'est la seule lecture dont tout le reste depend.
			struct Rangees {
					float32 y[256];
					float32 x[256];
					NkString texte[256];
					uint32 n = 0;
			};

			inline Rangees LireRangees(const NkRecordingPaint &rec, const Tranche &t) {
				Rangees r;
				for (uint32 i = t.debut; i < t.fin && r.n < 256u; ++i) {
					const NkPaintCmd &c = rec.cmds[i];
					if (c.op != NkPaintOp::Text || c.text.Empty())
						continue;
					uint32 k = 0;
					for (; k < r.n; ++k)
						if (PresqueEgal(r.y[k], c.y, 1.0f))
							break;
					if (k == r.n) {
						r.y[r.n] = c.y;
						r.x[r.n] = c.x;
						r.texte[r.n] = c.text;
						++r.n;
					} else if (c.x < r.x[k]) {
						r.x[k] = c.x;
						r.texte[k] = c.text;
					}
				}
				return r;
			}

			/// LE NOMBRE DE RANGEES DESSINEES **SOUS** celle d'indice `i` ET PLUS INDENTEES
			/// QU'ELLE — c'est-a-dire ses enfants, tels que le rail les peint. On s'arrete
			/// des qu'une rangee revient a son niveau ou au-dessus.
			/// ⚠️ MESURE LOCALE, ET C'EST VOULU : un compte GLOBAL de niveaux ne sait pas
			///    distinguer « cette entree-ci s'est dépliée » de « une autre branche du
			///    rail etait deja profonde ». Sur le rail complet (sections, volumes,
			///    chemin revele), le compte global ne repond pas a la question posee.
			inline int32 EnfantsDessinesSous(const Rangees &r, uint32 i) {
				if (i >= r.n)
					return -1;
				int32 n = 0;
				for (uint32 k = i + 1u; k < r.n; ++k) {
					if (r.x[k] <= r.x[i] + 0.5f)
						break;
					++n;
				}
				return n;
			}

			/// ① LA LARGEUR DE LA ZONE DE CONTENU DU RAIL, LUE DANS LE DESSIN. L'arbre
			/// pousse un SECOND rognage, a l'interieur du sien, sur la zone ou il emet ses
			/// rangees : sa largeur dit donc ou le contenu s'arrete, c'est-a-dire si la
			/// gouttiere de defilement a REELLEMENT ete reservee. -1 = pas trouve.
			/// ⚠️ ON LIT LE DESSIN, PAS LE RECTANGLE RAPPORTE. Un composant qui
			///    rapporterait une gouttiere sans la reserver passerait un essai portant
			///    sur son rapport ; il ne passe pas celui-ci.
			/// ⚠️ ET SURTOUT PAS « la plus large bande peinte » : le fond du panneau
			///    couvre legitimement la gouttiere (elle a besoin d'un fond), donc cette
			///    mesure-la rendait la largeur ENTIERE et accusait un code juste. Premiere
			///    ecriture de cet essai, corrigee sur son propre rouge.
			inline float32 LargeurZoneDeContenu(const NkRecordingPaint &rec, const Tranche &t,
												float32 railX, float32 railW) {
				for (uint32 i = t.debut + 1u; i < t.fin; ++i) {
					const NkPaintCmd &c = rec.cmds[i];
					if (c.op != NkPaintOp::PushClip || !PresqueEgal(c.x, railX))
						continue;
					if (c.w < railW - 0.5f)
						return c.w;
				}
				return -1.f;
			}

			/// L'ORDONNEE D'UNE RANGEE DESIGNEE PAR SON LIBELLE, TELLE QU'ELLE EST PEINTE.
			/// C'est exactement la grandeur que la question de S8 ② demande : « a quelle
			/// position a l'ecran se trouve l'entree depliee avant et apres le depliage ».
			/// -1 = absente du dessin — ce qui n'est PAS « a la meme place », et c'est
			/// pourquoi l'essai teste aussi que la valeur est positive.
			inline float32 OrdonneeDuLibelle(const Rangees &r, const char *libelle) {
				for (uint32 k = 0; k < r.n; ++k)
					if (NkComponentDecl::StrEq(r.texte[k].CStr(), libelle))
						return r.y[k];
				return -1.f;
			}

			inline int32 IndexDuLibelle(const Rangees &r, const char *libelle) {
				for (uint32 k = 0; k < r.n; ++k)
					if (NkComponentDecl::StrEq(r.texte[k].CStr(), libelle))
						return (int32)k;
				return -1;
			}

			/// LE CHEVRON D'UNE RANGEE, LU DANS LE DESSIN. Les deux segments d'epaisseur 1,3
			/// que le composant trace faute d'atlas ; leur extremite COMMUNE est l'apex.
			/// `rangeeY` : -1 = le premier chevron du rail, sinon celui de cette rangee.
			struct Chevron {
					bool trouve = false;
					float32 cx = 0.f, cy = 0.f; // centre des deux segments (= centre du chevron)
					float32 apexX = 0.f, apexY = 0.f;
					bool Deplie() const { // apex EN BAS = deplie ; apex A DROITE = replie
						return (apexY - cy) > (apexX - cx);
					}
			};

			/// ⚠️ ON APPARIE D'ABORD, ON FILTRE ENSUITE. Filtrer les SEGMENTS par leur
			///    ordonnee attrapait le chevron de la rangee VOISINE : ses deux segments
			///    tombent a une demi-hauteur de rangee, donc dans la meme tolerance. On
			///    reconstitue donc tous les chevrons de la tranche, puis on retient celui
			///    dont le CENTRE tombe dans la bande de la rangee visee.
			inline Chevron PremierChevron(const NkRecordingPaint &rec, const Tranche &t,
										  float32 rangeeY, float32 hauteurRangee = 26.f) {
				const NkPaintCmd *a = nullptr;
				for (uint32 i = t.debut; i < t.fin; ++i) {
					const NkPaintCmd &c = rec.cmds[i];
					if (c.op != NkPaintOp::Line || !PresqueEgal(c.rounding, 1.3f, 0.01f))
						continue;
					if (!a) {
						a = &c;
						continue;
					}
					// l'extremite commune : (x+w, y+h) des deux segments
					const float32 ax = a->x + a->w, ay = a->y + a->h;
					const float32 bx = c.x + c.w, by = c.y + c.h;
					if (!PresqueEgal(ax, bx, 0.05f) || !PresqueEgal(ay, by, 0.05f)) {
						a = &c;
						continue;
					}
					Chevron ch;
					ch.trouve = true;
					ch.apexX = ax;
					ch.apexY = ay;
					// le centre du chevron : le barycentre des trois sommets du triangle
					ch.cx = (a->x + c.x + ax) / 3.f;
					ch.cy = (a->y + c.y + ay) / 3.f;
					a = nullptr;
					if (rangeeY < 0.f
						|| (ch.cy >= rangeeY - 2.f && ch.cy <= rangeeY + hauteurRangee))
						return ch;
				}
				return Chevron();
			}

			/// LE LIBELLE DE LA RANGEE ACTIVE, LU DANS LE DESSIN : le remplissage pleine
			/// rangee au role `activeMark`, puis le texte le plus a gauche a cette ordonnee.
			inline NkString LibelleActif(const NkRecordingPaint &rec, const Tranche &t,
										 uint16 roleActif, float32 railW) {
				float32 yActif = -1.f;
				for (uint32 i = t.debut; i < t.fin; ++i) {
					const NkPaintCmd &c = rec.cmds[i];
					// pleine rangee : la barre d'accent gauche porte le MEME role de
					// remplissage mais fait quelques pixels de large -- on l'ecarte.
					if (c.op == NkPaintOp::Fill && c.role == roleActif && c.w > railW * 0.5f) {
						yActif = c.y;
						break;
					}
				}
				if (yActif < 0.f)
					return NkString();
				NkString meilleur;
				float32 xMin = 1e9f;
				for (uint32 i = t.debut; i < t.fin; ++i) {
					const NkPaintCmd &c = rec.cmds[i];
					if (c.op != NkPaintOp::Text || c.text.Empty())
						continue;
					if (!PresqueEgal(c.y, yActif, 14.f))
						continue;
					if (c.x < xMin) {
						xMin = c.x;
						meilleur = c.text;
					}
				}
				return meilleur;
			}

			// ── LE MONTAGE : une image du volet, dans l'enregistreur ────────────
			// ⚠️ IL PASSE PAR `NkInstanceVoletSelecteur`, l'instance du DIALOGUE, pas par
			//    une instance a lui : c'est elle qui porte `tree_default_open`, et une
			//    copie locale aurait mesure la copie.
			struct Banc {
					NkRecordingPaint rec;
					NkFilePickerNavStyle style = NkStyleSelecteurDefaut();
					NkPaintRect zone = {0.f, 0.f, 900.f, 520.f};
					float32 railW = 0.f;

					NkContentBrowserResult Image(NkFilePickerNavState &fp, const NkComponentInput &in) {
						rec.Reset();
						NkComponentInstance &inst = NkInstanceVoletSelecteur(false);
						if (fp.largeurRail < NkFilePickerNavState::kRailMin)
							fp.largeurRail = NkFilePickerNavState::kRailMin;
						if (fp.largeurRail > zone.w * NkFilePickerNavState::kRailMax)
							fp.largeurRail = zone.w * NkFilePickerNavState::kRailMax;
						inst.SetParam("tree_width", fp.largeurRail / zone.w);
						NkContentBrowserStyle volet = style.volet;
						volet.values = &inst;
						railW = zone.w * NkBrowserParam(volet, "tree_width");
						NkContentBrowserHooks h;
						return NkDrawContentBrowser(rec, in, zone, fp.vue, volet, h);
					}
			};

			inline NkComponentInput Repos() {
				NkComponentInput in;
				in.surfaceScale = 1.f;
				in.mouseX = -1000.f;
				in.mouseY = -1000.f;
				return in;
			}

			// ── LES ESSAIS ──────────────────────────────────────────────────────
			/// Rend son BILAN (essais tentes / essais verts) — pas un booleen : deux
			/// echecs ne doivent pas compter pour un, et l'hote additionne les deux
			/// nombres a son propre compte.
			inline Bilan Sonder() {
				Bilan b;
				printf("\nFamille 5 — le RAIL du selecteur : depliage, chevron, historique\n");

				Terrain terrain;
				const NkString base = NkDirectory::GetCurrentDirectory().ToString();
				if (!terrain.Poser(base.CStr())) {
					printf("  [ROUGE] 5-000 impossible de fabriquer le terrain d'essai sous %s\n",
						   base.CStr());
					++b.total; // un banc qui ne trouve pas son sujet ECHOUE, il n'applaudit pas
					return b;
				}

				Banc banc;
				char detail[512];

				// ── 5a — LE CONTROLE POSITIF DE L'INSTRUMENT ────────────────────
				// Sans lui, « 1 niveau » et « rien de peint » rendraient le meme chiffre.
				NkFilePickerNavState fp;
				fp.OpenPickerBase(NkFilePickerState::PK_PickFolder, terrain.racine.CStr(), nullptr, 0,
								  terrain.racine.CStr(), nullptr);
				fp.RelireDossier();
				fp.SuivreLeDepliage();
				banc.Image(fp, Repos());
				Tranche t = TrancheDuRail(banc.rec, banc.zone.x, banc.railW);
				snprintf(detail, sizeof(detail), "la tranche du rail existe dans le flux (%u..%u sur %u commandes)",
						 t.debut, t.fin, (uint32)banc.rec.cmds.Size());
				Verifier(b, t.trouve, "5a", detail);
				if (!t.trouve) {
					terrain.Retirer();
					printf("\n  L'instrument ne trouve pas son sujet : les essais suivants ne veulent rien dire.\n");
					return b;
				}
				int32 n0 = NiveauxDessines(banc.rec, t);
				snprintf(detail, sizeof(detail),
						 "le rail dessine au moins une rangee de libelle (niveaux = %d)", n0);
				Verifier(b, n0 >= 1, "5b", detail);

				// ── 5c — ① AVANT LE DEPLIAGE : UN SEUL NIVEAU ───────────────────
				snprintf(detail, sizeof(detail),
						 "rien n'est deplie -> le rail dessine %d niveau(x) de profondeur (attendu 1)",
						 n0);
				Verifier(b, n0 == 1, "5c", detail);

				// ── 5d — ①bis LE CHEVRON DIT L'ETAT REEL ────────────────────────
				// Rodolf, capture de 08h54 : « tous les chevrons du rail pointent deja vers
				// le bas, donc l'etat ouvert est affiche alors que rien n'est ouvert ».
				Chevron ch = PremierChevron(banc.rec, t, -1.f);
				snprintf(detail, sizeof(detail), "le chevron du rail est TRACE et lisible dans le flux");
				Verifier(b, ch.trouve, "5d", detail);
				if (ch.trouve) {
					snprintf(detail, sizeof(detail),
							 "rien n'est deplie -> le chevron est dessine REPLIE (apex a %+.1f,%+.1f du centre)",
							 (double)(ch.apexX - ch.cx), (double)(ch.apexY - ch.cy));
					Verifier(b, !ch.Deplie(), "5e", detail);
				}

				// ── 5f — ① APRES LE CLIC SUR LE CHEVRON : DEUX NIVEAUX ──────────
				// LE CLIC EST POSE LA OU LE CHEVRON EST DESSINE, pas la ou la mise en page
				// dirait qu'il devrait etre.
				int32 n1 = -1;
				if (ch.trouve) {
					NkComponentInput in = Repos();
					in.mouseX = ch.cx;
					in.mouseY = ch.cy;
					in.mousePressed = true;
					banc.Image(fp, in);		 // l'image qui RECOIT le clic
					fp.SuivreLeDepliage();	 // le MEME code que le dialogue appelle
					banc.Image(fp, Repos()); // l'image d'apres
					t = TrancheDuRail(banc.rec, banc.zone.x, banc.railW);
					n1 = t.trouve ? NiveauxDessines(banc.rec, t) : -1;
				}
				snprintf(detail, sizeof(detail),
						 "un clic sur le chevron -> le rail dessine %d niveau(x) (attendu 2)", n1);
				Verifier(b, n1 == 2, "5f", detail);

				// ── 5g — ① LE CHEVRON A SUIVI ───────────────────────────────────
				if (t.trouve) {
					const Chevron ch2 = PremierChevron(banc.rec, t, -1.f);
					snprintf(detail, sizeof(detail),
							 "apres le depliage, le chevron est dessine DEPLIE (apex a %+.1f,%+.1f)",
							 (double)(ch2.apexX - ch2.cx), (double)(ch2.apexY - ch2.cy));
					Verifier(b, ch2.trouve && ch2.Deplie(), "5g", detail);
				}

				// ── 5h — CONTROLE NEGATIF : REPLIER REVIENT A UN NIVEAU ─────────
				// Un essai qui ne sait que monter ne prouve pas que le depliage commande
				// quoi que ce soit : il pourrait mesurer un rail qui grossit tout seul.
				int32 n2 = -1;
				if (t.trouve) {
					const Chevron ch2 = PremierChevron(banc.rec, t, -1.f);
					if (ch2.trouve) {
						NkComponentInput in = Repos();
						in.mouseX = ch2.cx;
						in.mouseY = ch2.cy;
						in.mousePressed = true;
						banc.Image(fp, in);
						fp.SuivreLeDepliage();
						banc.Image(fp, Repos());
						const Tranche t2 = TrancheDuRail(banc.rec, banc.zone.x, banc.railW);
						n2 = t2.trouve ? NiveauxDessines(banc.rec, t2) : -1;
					}
				}
				snprintf(detail, sizeof(detail),
						 "un second clic replie -> le rail redescend a %d niveau(x) (attendu 1)", n2);
				Verifier(b, n2 == 1, "5h", detail);

				// ── 5i — ① HORS DU « DOSSIER COURANT » : le defaut que Rodolf voit ──
				// Le rail complet (sections « Acces rapide » / « Ce PC »). Le terrain est
				// pose en FAVORI pour y figurer sans dependre de la machine.
				// ⚠️ ICI LES SECTIONS EXISTENT, donc la profondeur de base est 2 (le titre,
				//    puis son entree). La question reste la meme : le rail dessine-t-il un
				//    niveau DE PLUS apres le clic ?
				{
					// LE FAVORI EST UN SOUS-DOSSIER DU TERRAIN, et le dialogue s'ouvre AILLEURS
					// (sur le repertoire courant du processus) : le favori n'est donc PAS un
					// ancetre du dossier affiche, et rien ne l'ouvre a notre place. Sans cette
					// precaution, la revelation du chemin courant l'aurait deja deplie et
					// l'essai aurait mesure son propre montage.
					const NkString favori = (NkPath(terrain.racine) / "zeta_favori").ToString();
					NkDirectory::CreateRecursive((NkPath(favori) / "z1").ToString().CStr());
					NkDirectory::CreateRecursive((NkPath(favori) / "z2").ToString().CStr());
					NkFilePickerNavState g;
					g.favoris.PushBack(favori);
					g.OpenPickerBase(NkFilePickerState::PK_PickFolder, base.CStr(), nullptr, 0, nullptr,
									 nullptr);
					g.RelireDossier();
					g.SuivreLeDepliage();
					banc.Image(g, Repos());
					Tranche tg = TrancheDuRail(banc.rec, banc.zone.x, banc.railW);
					Rangees rg = tg.trouve ? LireRangees(banc.rec, tg) : Rangees();
					const int32 iFav = IndexDuLibelle(rg, "zeta_favori");
					const int32 g0 = iFav >= 0 ? EnfantsDessinesSous(rg, (uint32)iFav) : -1;
					int32 g1 = -1;
					const Chevron cg = (tg.trouve && iFav >= 0)
										   ? PremierChevron(banc.rec, tg, rg.y[(uint32)iFav])
										   : Chevron();
					if (cg.trouve) {
						NkComponentInput in = Repos();
						in.mouseX = cg.cx;
						in.mouseY = cg.cy;
						in.mousePressed = true;
						banc.Image(g, in);
						g.SuivreLeDepliage();
						banc.Image(g, Repos());
						tg = TrancheDuRail(banc.rec, banc.zone.x, banc.railW);
						rg = tg.trouve ? LireRangees(banc.rec, tg) : Rangees();
						const int32 j = IndexDuLibelle(rg, "zeta_favori");
						g1 = j >= 0 ? EnfantsDessinesSous(rg, (uint32)j) : -1;
					}
					snprintf(detail, sizeof(detail),
							 "un favori d'« Acces rapide », hors du chemin courant : %d rangee(s) "
							 "dessinee(s) sous lui avant le clic, %d apres (attendu 0 puis 2)",
							 g0, g1);
					Verifier(b, cg.trouve && g0 == 0 && g1 == 2, "5i", detail);
				}

				// ── 5j / 5k / 5l — ③ PRECEDENT ET SUIVANT ───────────────────────
				// Rodolf : « apres trois navigations puis deux retours, quel chemin le rail
				// montre-t-il ? » -- et la reponse se lit sur LA RANGEE SURLIGNEE, pas dans
				// le modele.
				{
					NkFilePickerNavState h;
					h.OpenPickerBase(NkFilePickerState::PK_PickFolder, terrain.racine.CStr(), nullptr, 0,
									 terrain.racine.CStr(), nullptr);
					h.RelireDossier(); // visite 0 : la racine
					const char *const kPas[3] = {"alpha", "alpha/a1", "beta"};
					for (int32 k = 0; k < 3; ++k) {
						h.AllerA((NkPath(terrain.racine) / kPas[k]).ToString().CStr());
						h.RelireDossier();
					}
					// CONTROLE NEGATIF : rien n'a ete visite « en avant », donc rien a avancer.
					snprintf(detail, sizeof(detail),
							 "apres trois navigations, « suivant » est refuse (histoire = %u, pos = %d)",
							 (uint32)h.histoire.Size(), h.histoirePos);
					Verifier(b, !h.PeutAvancer() && h.histoire.Size() == 4u && h.histoirePos == 3, "5j",
							 detail);

					const bool r1 = h.Reculer();
					h.RelireDossier();
					const bool r2 = h.Reculer();
					h.RelireDossier();
					h.SuivreLeDepliage();
					banc.Image(h, Repos());
					const Tranche th = TrancheDuRail(banc.rec, banc.zone.x, banc.railW);
					const NkString actif =
						th.trouve ? LibelleActif(banc.rec, th, banc.style.volet.activeMark, banc.railW)
								  : NkString();
					snprintf(detail, sizeof(detail),
							 "trois navigations puis deux retours -> le rail surligne « %s » (attendu "
							 "« alpha ») ; chemin = %s",
							 actif.Empty() ? "(rien)" : actif.CStr(), h.pickerPath);
					Verifier(b,
							 r1 && r2 && !actif.Empty()
								 && NkComponentDecl::StrEq(actif.CStr(), "alpha"),
							 "5k", detail);

					// et « suivant » redevient possible, et ramene ou l'on etait
					const bool a1 = h.Avancer();
					h.RelireDossier();
					snprintf(detail, sizeof(detail),
							 "« suivant » redevient possible et ramene a %s (attendu .../alpha/a1)",
							 h.pickerPath);
					Verifier(b,
							 a1
								 && NkFilePickerState::PathSame(
										h.pickerPath,
										(NkPath(terrain.racine) / "alpha/a1").ToString().CStr()),
							 "5l", detail);

					// ── 5m — LA CLAUSE QUI NE S'EXERCE PAS TOUTE SEULE ──────────
					// ⚠️ ECRIT PARCE QU'UNE MUTATION A SURVECU, PAS PAR SYMETRIE. Retirer la
					//    ligne qui coupe le futur laissait 5j/5k/5l TOUS VERTS : aucun d'eux
					//    ne navigue depuis un point du passe, donc aucun n'atteignait cette
					//    clause. Une mutation qui ne casse rien est un resultat -- ici, le
					//    resultat etait « ce cas n'existe pas ».
					// Etat en entree : histoire = [racine, alpha, alpha/a1, beta], pos = 2.
					// On navigue AILLEURS depuis ce point : « beta » doit disparaitre.
					h.AllerA((NkPath(terrain.racine) / "gamma").ToString().CStr());
					h.RelireDossier();
					snprintf(detail, sizeof(detail),
							 "naviguer depuis un point du passe coupe le futur : histoire = %u, "
							 "pos = %d, « suivant » %s (attendu 4 / 3 / refuse)",
							 (uint32)h.histoire.Size(), h.histoirePos,
							 h.PeutAvancer() ? "encore possible" : "refuse");
					Verifier(b,
							 h.histoire.Size() == 4u && h.histoirePos == 3 && !h.PeutAvancer()
								 && NkFilePickerState::PathSame(
										h.histoire[3u].CStr(),
										(NkPath(terrain.racine) / "gamma").ToString().CStr()),
							 "5m", detail);
				}

				// ══ FAMILLE 6 — ② LE MENU CONTEXTUEL ET SES SEPT ACTIONS ═══════════
				// ⚠️ DEUX QUESTIONS DISTINCTES, ET LA PREMIERE EST CELLE QU'ON OUBLIE :
				//    (a) le clic droit EST-IL RAPPORTE, dans les deux volets ? Sans elle,
				//        sept actions parfaites resteraient inatteignables ;
				//    (b) chaque action fait-elle CE QU'ELLE DIT, sur le disque ?
				//    Le dessin du menu lui-meme (`NkCtxMenuDraw`) n'est PAS mesure ici : il
				//    reclame un `NkGuiContext` et une police. C'est dit dans l'en-tete.
				printf("\nFamille 6 — le menu contextuel du selecteur : ouverture et actions\n");

				// ── 6a — LE CLIC DROIT SUR UNE ENTREE DE LA GRILLE EST RAPPORTE ─
				{
					NkFilePickerNavState f6;
					f6.OpenPickerBase(NkFilePickerState::PK_PickFolder, terrain.racine.CStr(), nullptr,
									  0, terrain.racine.CStr(), nullptr);
					f6.RelireDossier();
					f6.SuivreLeDepliage();
					NkContentBrowserResult r0 = banc.Image(f6, Repos());
					// le libelle d'une entree de la GRILLE : a droite du rail
					float32 gx = -1.f, gy = -1.f;
					for (uint32 i = 0; i < (uint32)banc.rec.cmds.Size(); ++i) {
						const NkPaintCmd &c = banc.rec.cmds[i];
						if (c.op == NkPaintOp::Text && !c.text.Empty()
							&& c.x > banc.zone.x + banc.railW + 4.f
							&& NkComponentDecl::StrEq(c.text.CStr(), "alpha")) {
							gx = c.x + 2.f;
							gy = c.y + 4.f;
							break;
						}
					}
					snprintf(detail, sizeof(detail),
							 "le repos ne rapporte AUCUN clic droit (menuIndex = %d, attendu -2) et "
							 "l'entree « alpha » est peinte dans la grille",
							 r0.menuIndex);
					Verifier(b, r0.menuIndex == -2 && gx >= 0.f, "6a", detail);

					NkContentBrowserResult r1;
					if (gx >= 0.f) {
						NkComponentInput in = Repos();
						in.mouseX = gx;
						in.mouseY = gy;
						in.rightPressed = true;
						r1 = banc.Image(f6, in);
					}
					snprintf(detail, sizeof(detail),
							 "un clic droit sur « alpha » est rapporte : menuIndex = %d (attendu >= 0)",
							 r1.menuIndex);
					Verifier(b, r1.menuIndex >= 0, "6b", detail);

					// ── 6c — LE CLIC DROIT SUR LE RAIL EST RAPPORTE, AVEC SON CHEMIN ─
					// C'est celui qui n'existait PAS : le pont du navigateur ne relayait que
					// la selection de l'arbre.
					banc.Image(f6, Repos());
					Tranche t6 = TrancheDuRail(banc.rec, banc.zone.x, banc.railW);
					const Rangees r6 = t6.trouve ? LireRangees(banc.rec, t6) : Rangees();
					NkContentBrowserResult r2;
					if (r6.n > 0u) {
						NkComponentInput in = Repos();
						in.mouseX = r6.x[0] + 4.f;
						in.mouseY = r6.y[0] + 6.f;
						in.rightPressed = true;
						r2 = banc.Image(f6, in);
					}
					snprintf(detail, sizeof(detail),
							 "un clic droit sur le RAIL rapporte le chemin du nœud : « %s »",
							 r2.menuCheminRail.Empty() ? "(vide)" : r2.menuCheminRail.CStr());
					Verifier(b,
							 !r2.menuCheminRail.Empty()
								 && NkFilePickerState::PathSame(r2.menuCheminRail.CStr(),
																terrain.racine.CStr()),
							 "6c", detail);
				}

				// ── 6d..6k — LES ACTIONS, SUR LE DISQUE ─────────────────────────
				{
					NkFilePickerNavState a;
					a.OpenPickerBase(NkFilePickerState::PK_PickFolder, terrain.racine.CStr(), nullptr, 0,
									 terrain.racine.CStr(), nullptr);
					a.RelireDossier();

					// creer un fichier — et il NE navigue PAS
					char avant[512];
					NkFilePickerState::CopyTo(avant, a.pickerPath, (int32)sizeof(avant));
					a.OuvrirSaisie(NkFilePickerNavState::SaisieFichier, nullptr, nullptr, "note.txt");
					const bool cree = a.AppliquerSaisie();
					const NkString fic = (NkPath(terrain.racine) / "note.txt").ToString();
					snprintf(detail, sizeof(detail),
							 "« Créer un fichier » écrit note.txt et ne déplace pas le dossier "
							 "affiché (%s)",
							 a.pickerPath);
					Verifier(b, cree && NkFile::Exists(fic.CStr())
								   && NkFilePickerState::PathSame(avant, a.pickerPath),
							 "6d", detail);

					// controle NEGATIF : un nom impossible est REFUSE, et il le dit
					a.OuvrirSaisie(NkFilePickerNavState::SaisieFichier, nullptr, nullptr, "a/b.txt");
					const bool refuse = !a.AppliquerSaisie();
					snprintf(detail, sizeof(detail),
							 "un nom contenant « / » est refusé et la raison est dite : « %s »",
							 a.messageCreation.Empty() ? "(rien)" : a.messageCreation.CStr());
					Verifier(b, refuse && !a.messageCreation.Empty(), "6e", detail);

					// COPIER puis COLLER ailleurs : les DEUX existent
					a.menuCibles.Clear();
					a.menuCibles.PushBack(fic);
					a.MettreAuPressePapier(false);
					a.saisieDossier = (NkPath(terrain.racine) / "beta").ToString();
					const uint32 nCopie = a.Coller();
					const NkString copie = (NkPath(terrain.racine) / "beta/note.txt").ToString();
					snprintf(detail, sizeof(detail),
							 "copier/coller : %u traité(s), l'original ET la copie existent (%d / %d)",
							 nCopie, NkFile::Exists(fic.CStr()) ? 1 : 0,
							 NkFile::Exists(copie.CStr()) ? 1 : 0);
					Verifier(b, nCopie == 1u && NkFile::Exists(fic.CStr()) && NkFile::Exists(copie.CStr()),
							 "6f", detail);

					// COLLER SUR UN HOMONYME : on n'ecrase pas, on suffixe
					const uint32 nDeux = a.Coller();
					const NkString copie2 = (NkPath(terrain.racine) / "beta/note (2).txt").ToString();
					snprintf(detail, sizeof(detail),
							 "un second collage ne remplace rien : « note (2).txt » existe (%d), "
							 "« note.txt » aussi (%d)",
							 NkFile::Exists(copie2.CStr()) ? 1 : 0, NkFile::Exists(copie.CStr()) ? 1 : 0);
					Verifier(b, nDeux == 1u && NkFile::Exists(copie2.CStr()) && NkFile::Exists(copie.CStr()),
							 "6g", detail);

					// COUPER puis COLLER : la source DISPARAIT, et le presse-papiers se vide
					a.menuCibles.Clear();
					a.menuCibles.PushBack(fic);
					a.MettreAuPressePapier(true);
					a.saisieDossier = (NkPath(terrain.racine) / "gamma").ToString();
					const uint32 nCoupe = a.Coller();
					const NkString deplace = (NkPath(terrain.racine) / "gamma/note.txt").ToString();
					snprintf(detail, sizeof(detail),
							 "couper/coller : %u traité(s), la source a disparu (%d), la cible existe "
							 "(%d), le presse-papiers est vide (%d)",
							 nCoupe, NkFile::Exists(fic.CStr()) ? 0 : 1,
							 NkFile::Exists(deplace.CStr()) ? 1 : 0, a.pressePapier.Empty() ? 1 : 0);
					Verifier(b,
							 nCoupe == 1u && !NkFile::Exists(fic.CStr())
								 && NkFile::Exists(deplace.CStr()) && a.pressePapier.Empty(),
							 "6h", detail);

					// controle NEGATIF : coller un dossier DANS LUI-MEME est refuse
					// ⚠️ CE CAS NE PROUVE PAS QUE LA GARDE EXISTE, ET IL FAUT LE DIRE.
					//    Mutation faite le 06/09 : garde retiree -> le cas RESTE VERT, parce
					//    que Windows refuse de son cote de deplacer un dossier dans son propre
					//    sous-dossier. Ce qu'il atteste, c'est que l'operation N'A PAS LIEU ;
					//    pas qui l'a refusee. La garde protege en realite le chemin de COPIE
					//    (`CopierRecursif` boucle sans elle), et ce cas-la n'est PAS ecrit :
					//    son mode d'echec est un blocage, pas un rouge.
					a.menuCibles.Clear();
					a.menuCibles.PushBack((NkPath(terrain.racine) / "alpha").ToString());
					a.MettreAuPressePapier(true);
					a.saisieDossier = (NkPath(terrain.racine) / "alpha/a1").ToString();
					const uint32 nDedans = a.Coller();
					snprintf(detail, sizeof(detail),
							 "coller « alpha » dans « alpha/a1 » est refusé (%u traité(s)) et « alpha » "
							 "est intact (%d)",
							 nDedans,
							 NkDirectory::Exists((NkPath(terrain.racine) / "alpha").ToString().CStr())
								 ? 1
								 : 0);
					Verifier(b,
							 nDedans == 0u
								 && NkDirectory::Exists(
										(NkPath(terrain.racine) / "alpha").ToString().CStr()),
							 "6i", detail);
					a.pressePapier.Clear();
					a.presseCouper = false;

					// RENOMMER : l'ancien chemin s'en va, le nouveau arrive
					a.saisieDossier = NkString();
					a.OuvrirSaisie(NkFilePickerNavState::SaisieRenommer, nullptr, deplace.CStr(),
								   "carnet.txt");
					const bool renom = a.AppliquerSaisie();
					const NkString apres = (NkPath(terrain.racine) / "gamma/carnet.txt").ToString();
					snprintf(detail, sizeof(detail),
							 "renommer : l'ancien chemin a disparu (%d), le nouveau existe (%d)",
							 NkFile::Exists(deplace.CStr()) ? 0 : 1, NkFile::Exists(apres.CStr()) ? 1 : 0);
					Verifier(b, renom && !NkFile::Exists(deplace.CStr()) && NkFile::Exists(apres.CStr()),
							 "6j", detail);

					// SUPPRIMER : a la corbeille, donc absent du disque
					a.menuCibles.Clear();
					a.menuCibles.PushBack(apres);
					const uint32 nSupp = a.Supprimer();
					snprintf(detail, sizeof(detail),
							 "supprimer : %u mis à la corbeille, le chemin n'existe plus (%d)", nSupp,
							 NkFile::Exists(apres.CStr()) ? 0 : 1);
					Verifier(b, nSupp == 1u && !NkFile::Exists(apres.CStr()), "6k", detail);

					// controle NEGATIF : supprimer le dossier AFFICHE est refuse
					a.menuCibles.Clear();
					a.menuCibles.PushBack(NkString(a.pickerPath));
					const uint32 nSoi = a.Supprimer();
					snprintf(detail, sizeof(detail),
							 "supprimer le dossier affiché est refusé (%u) et il existe toujours (%d)",
							 nSoi, NkDirectory::Exists(a.pickerPath) ? 1 : 0);
					Verifier(b, nSoi == 0u && NkDirectory::Exists(a.pickerPath), "6l", detail);
				}

				// ══ FAMILLE 7 — ① LA GOUTTIERE, ② L'ENTREE QUI RESTE A SA PLACE ══════
				//
				// LES DEUX DEFAUTS DE S8, ET LES DEUX QUESTIONS QUE RODOLF POSE :
				//   ① « il n'y a pas de scrollbar vertical ni a gauche ni a droite pour
				//      montrer la profondeur » ;
				//   ② « quand je deplie chaque dossier a gauche, ca deplie mais ca me
				//      ramene en haut du contenu de gauche. »
				//
				// ⚠️ LA QUESTION DE ② N'EST PAS « le decalage est-il conserve ». C'est
				//    « A QUELLE POSITION A L'ECRAN SE TROUVE L'ENTREE DEPLIEE AVANT ET
				//    APRES LE DEPLIAGE », et les deux valeurs doivent etre EGALES. Un
				//    essai sur le nombre de pixels aurait ete vert sur un rail dont des
				//    rangees naissent au-dessus — c'est-a-dire sur le defaut suivant.
				//
				// ⚠️ CE QUE CETTE FAMILLE NE PROUVE PAS, ET IL FAUT LE LIRE AVEC SON
				//    RESULTAT : elle ne voit AUCUNE barre de defilement peinte. La barre
				//    est `NkVScrollbar`, elle prend un `NkGuiContext` et une police, et
				//    c'est l'hote (`NkDrawFilePickerNav`) qui l'appelle. Ce banc mesure
				//    ce dont la barre depend et que le composant seul possede : la
				//    gouttiere EST-ELLE RESERVEE dans le dessin, et le rectangle rapporte
				//    tombe-t-il exactement dessus. Que le pouce se voie et se glisse a
				//    l'ecran reste un geste humain, et il est demande comme tel.
				printf("\nFamille 7 — ① la gouttiere de defilement, ② l'entree qui reste a sa place\n");
				{
					// ── LE TERRAIN : UN RAIL QUI DEBORDE A COUP SUR ─────────────────
					// ⚠️ PAR DES FAVORIS, PAS PAR LE DOSSIER COURANT. Le nombre de
					//    volumes, de dossiers usuels et de recents depend de la machine ;
					//    un banc dont le rail deborde « chez moi » n'est pas un banc. Vingt
					//    favoris font vingt rangees, partout.
					NkFilePickerNavState f7;
					char nomFav[64];
					for (int32 i = 0; i < 20; ++i) {
						snprintf(nomFav, sizeof(nomFav), "fav_%02d", i);
						const NkString d = (NkPath(terrain.racine) / nomFav).ToString();
						NkDirectory::CreateRecursive((NkPath(d) / "x1").ToString().CStr());
						NkDirectory::CreateRecursive((NkPath(d) / "x2").ToString().CStr());
						f7.favoris.PushBack(d);
					}
					f7.OpenPickerBase(NkFilePickerState::PK_PickFolder, base.CStr(), nullptr, 0, nullptr,
									  nullptr);
					f7.RelireDossier();
					f7.SuivreLeDepliage();
					NkContentBrowserResult r7 = banc.Image(f7, Repos());

					// ── 7a — CONTROLE POSITIF DU MONTAGE : le rail DEBORDE ──────────
					// Sans lui, tout ce qui suit serait mesure sur un rail qui tient
					// entierement dans son panneau — c'est-a-dire sur le seul cas ou le
					// defilement ne peut pas se perdre.
					snprintf(detail, sizeof(detail),
							 "le rail deborde vraiment : contenu %.0f px pour une vue de %.0f px",
							 (double)r7.railDefilContenu, (double)r7.railDefilVue);
					Verifier(b, r7.railDefilContenu > r7.railDefilVue + 1.f, "7a", detail);

					// ── 7b — ① LA GOUTTIERE DU RAIL EST RAPPORTEE, ET COLLEE AU BORD ─
					const float32 bordRail = banc.zone.x + banc.railW;
					snprintf(detail, sizeof(detail),
							 "la gouttiere du rail : x=%.1f w=%.1f -> bord droit %.1f (bord du rail "
							 "%.1f)",
							 (double)r7.railDefilX, (double)r7.railDefilW,
							 (double)(r7.railDefilX + r7.railDefilW), (double)bordRail);
					Verifier(b,
							 r7.railDefilW > 0.f && r7.railDefilH > 0.f
								 && PresqueEgal(r7.railDefilX + r7.railDefilW, bordRail, 1.5f),
							 "7b", detail);

					// ── 7c — ① ET ELLE EST RESERVEE **DANS LE DESSIN**, pas seulement
					//         rapportee. La bande de section la plus large s'arrete avant.
					Tranche t7 = TrancheDuRail(banc.rec, banc.zone.x, banc.railW);
					const float32 zoneContenu =
						t7.trouve ? LargeurZoneDeContenu(banc.rec, t7, banc.zone.x, banc.railW) : -1.f;
					snprintf(detail, sizeof(detail),
							 "la zone de contenu du rail est rognee a %.1f px, la gouttiere commence "
							 "donc la (rail %.1f - gouttiere %.1f = %.1f)",
							 (double)zoneContenu, (double)banc.railW, (double)r7.railDefilW,
							 (double)(banc.railW - r7.railDefilW));
					Verifier(b, t7.trouve && PresqueEgal(zoneContenu, banc.railW - r7.railDefilW, 1.5f),
							 "7c", detail);

					// ── 7d — ① LA GOUTTIERE DE LA GRILLE, meme mesure a droite ───────
					const float32 bordVolet = banc.zone.x + banc.zone.w;
					snprintf(detail, sizeof(detail),
							 "la gouttiere de la grille : x=%.1f w=%.1f -> bord droit %.1f (bord du "
							 "volet %.1f)",
							 (double)r7.defilX, (double)r7.defilW, (double)(r7.defilX + r7.defilW),
							 (double)bordVolet);
					Verifier(b,
							 r7.defilW > 0.f && r7.defilH > 0.f
								 && PresqueEgal(r7.defilX + r7.defilW, bordVolet, 1.5f),
							 "7d", detail);

					// ── 7e — LE RAIL DEFILE VRAIMENT (controle positif de ②) ─────────
					// La molette, par le meme chemin que la main : `NkComponentInput`.
					// Sans cet essai, « l'entree n'a pas bouge » serait vrai sur un rail
					// qui n'a jamais defile — le cas ou le defaut ne peut pas se produire.
					{
						NkComponentInput in = Repos();
						in.mouseX = banc.zone.x + banc.railW * 0.5f;
						in.mouseY = banc.zone.y + banc.zone.h * 0.5f;
						in.wheel = -6.f; // vers le bas
						banc.Image(f7, in);
					}
					r7 = banc.Image(f7, Repos());
					snprintf(detail, sizeof(detail), "la molette a fait defiler le rail : scroll = %.1f px",
							 (double)f7.vue.folders.scroll);
					Verifier(b, f7.vue.folders.scroll > 1.f, "7e", detail);

					// ── 7f / 7g — ② LA QUESTION DE S8, POSEE MOT POUR MOT ───────────
					t7 = TrancheDuRail(banc.rec, banc.zone.x, banc.railW);
					Rangees rg7 = t7.trouve ? LireRangees(banc.rec, t7) : Rangees();
					// On cherche, PARMI LES RANGEES REELLEMENT PEINTES apres defilement,
					// un favori encore replie : c'est celui que Rodolf deplierait.
					char cible7[64] = {0};
					float32 yAvant = -1.f;
					int32 enfAvant = -1;
					Chevron ch7;
					for (uint32 k = 0; k < rg7.n && cible7[0] == 0; ++k) {
						for (int32 i = 0; i < 20; ++i) {
							snprintf(nomFav, sizeof(nomFav), "fav_%02d", i);
							if (!NkComponentDecl::StrEq(rg7.texte[k].CStr(), nomFav))
								continue;
							const Chevron c = PremierChevron(banc.rec, t7, rg7.y[k]);
							if (!c.trouve || c.Deplie())
								continue;
							snprintf(cible7, sizeof(cible7), "%s", nomFav);
							yAvant = rg7.y[k];
							enfAvant = EnfantsDessinesSous(rg7, k);
							ch7 = c;
							break;
						}
					}
					float32 yApres = -2.f;
					int32 enfApres = -1;
					if (cible7[0]) {
						NkComponentInput in = Repos();
						in.mouseX = ch7.cx;
						in.mouseY = ch7.cy;
						in.mousePressed = true;
						banc.Image(f7, in);		 // l'image qui RECOIT le clic
						f7.SuivreLeDepliage();	 // le rail est RECONSTRUIT ici
						banc.Image(f7, Repos()); // l'image d'apres
						const Tranche t8 = TrancheDuRail(banc.rec, banc.zone.x, banc.railW);
						const Rangees rg8 = t8.trouve ? LireRangees(banc.rec, t8) : Rangees();
						yApres = OrdonneeDuLibelle(rg8, cible7);
						const int32 j = IndexDuLibelle(rg8, cible7);
						enfApres = j >= 0 ? EnfantsDessinesSous(rg8, (uint32)j) : -1;
					}
					snprintf(detail, sizeof(detail),
							 "« %s » etait peint a y = %.1f avant le depliage, a y = %.1f apres "
							 "(les deux doivent etre EGAUX)",
							 cible7[0] ? cible7 : "(aucun favori replie a l'ecran)", (double)yAvant,
							 (double)yApres);
					Verifier(b, cible7[0] != 0 && yAvant > 0.f && PresqueEgal(yAvant, yApres, 1.0f),
							 "7f", detail);

					// ── 7g — CONTROLE NEGATIF : LE DEPLIAGE A BIEN EU LIEU ──────────
					// Sans lui, 7f serait vert sur un chevron qui ne fait rien : une entree
					// qui ne bouge pas parce que rien ne s'est passe n'est pas une entree
					// qui reste a sa place.
					snprintf(detail, sizeof(detail),
							 "le clic a bien deplie : %d rangee(s) sous « %s » avant, %d apres "
							 "(attendu 0 puis 2)",
							 enfAvant, cible7[0] ? cible7 : "(aucun)", enfApres);
					Verifier(b, enfAvant == 0 && enfApres == 2, "7g", detail);

					// ── 7h — ② LA SECONDE CAUSE : DES RANGEES NAISSENT AU-DESSUS ────
					// ⚠️ C'EST L'ESSAI QUI SEPARE « GARDER LE DECALAGE » DE « GARDER
					//    L'ENTREE », et sans lui la correction serait a moitie faite. Une
					//    NAVIGATION fait apparaitre la section « Recents » — un titre et
					//    une entree, DEUX RANGEES, tout en haut du rail — et reconstruit
					//    tout. Un defilement conserve en pixels ferait alors glisser de
					//    deux rangees ce que l'utilisateur regardait ; une ancre par
					//    IDENTITE le laisse au meme pixel.
					// ⚠️ ET C'EST LA PORTE DES APPLICATIONS, PAS UN CHAMP POUSSE A LA
					//    MAIN : `PoserRecent` est la fonction que NkUIDesign appelle apres
					//    un enregistrement ou une ouverture reussie (`Panels.h:1592`). Une
					//    premiere ecriture de cet essai supposait qu'une NAVIGATION
					//    remplissait les recents ; mesure faite, elle ne les touche pas —
					//    le rouge etait dans l'essai, pas dans le kit.
					{
						const Tranche t9 = TrancheDuRail(banc.rec, banc.zone.x, banc.railW);
						const Rangees rg9 = t9.trouve ? LireRangees(banc.rec, t9) : Rangees();
						// une rangee OBSERVEE : le premier favori peint apres defilement
						char obs[64] = {0};
						float32 yAvantNav = -1.f;
						for (uint32 k = 0; k < rg9.n && obs[0] == 0; ++k)
							for (int32 i = 0; i < 20; ++i) {
								snprintf(nomFav, sizeof(nomFav), "fav_%02d", i);
								if (NkComponentDecl::StrEq(rg9.texte[k].CStr(), nomFav)) {
									snprintf(obs, sizeof(obs), "%s", nomFav);
									yAvantNav = rg9.y[k];
									break;
								}
							}
						const int32 nRecentsAvant = (int32)f7.recents.Size();
						float32 yApresNav = -2.f;
						if (obs[0]) {
							// Deux enregistrements reussis, du point de vue de l'application :
							// la section « Recents » n'apparait qu'a partir de deux entrees
							// (la tete est le dossier courant, qui a sa propre section).
							NkFilePickerNavState::PoserRecent(
								f7.recents, (NkPath(terrain.racine) / "alpha").ToString().CStr());
							NkFilePickerNavState::PoserRecent(
								f7.recents, (NkPath(terrain.racine) / "beta").ToString().CStr());
							// La relecture est ce que l'hote declenche apres un enregistrement :
							// elle RECONSTRUIT le rail, donc elle exerce exactement le chemin
							// ou le defilement se perdait.
							f7.RelireDossier();
							banc.Image(f7, Repos());
							const Tranche tA = TrancheDuRail(banc.rec, banc.zone.x, banc.railW);
							const Rangees rgA = tA.trouve ? LireRangees(banc.rec, tA) : Rangees();
							yApresNav = OrdonneeDuLibelle(rgA, obs);
						}
						snprintf(detail, sizeof(detail),
								 "la section « Recents » nait au-dessus (%d recent(s) -> %d) et "
								 "« %s » reste au meme pixel : y = %.1f puis %.1f",
								 nRecentsAvant, (int32)f7.recents.Size(), obs[0] ? obs : "(aucun)",
								 (double)yAvantNav, (double)yApresNav);
						Verifier(b,
								 obs[0] != 0 && yAvantNav > 0.f
									 && (int32)f7.recents.Size() > nRecentsAvant
									 && PresqueEgal(yAvantNav, yApresNav, 1.0f),
								 "7h", detail);
					}
				}

				terrain.Retirer();
				printf("  -- familles 5 a 7 : %d/%d\n", b.ok, b.total);
				return b;
			}

		} // namespace navprobe
	}	  // namespace editorkit
} // namespace nkentseu
