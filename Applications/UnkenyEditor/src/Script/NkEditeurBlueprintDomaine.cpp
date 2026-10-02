// -----------------------------------------------------------------------------
// FICHIER: UnkenyEditor/Script/NkEditeurBlueprintDomaine.cpp
// DESCRIPTION: Le DOMAINE de la toile pour les Blueprints d'Unkeny : la
//              categorie et la forme de chaque noeud (charte de Rihen), les
//              libelles, les champs, la trace de Simuler, et le BLOC de code
//              des noeuds Expression / Si (expression) / Code -- colore,
//              editable, ses « Snippets ».
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Script/NkEditeurBlueprint.h"

#include "Script/NkBpExpression.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		using editorkit::NkFormeNoeud;
		using editorkit::NkGenreChamp;
		using editorkit::NkNatureNoeud;
		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {
			NkEditeurBlueprintEtat &E(void *d) {
				return *static_cast<NkEditeurBlueprintEtat *>(d);
			}
			const char *TypeDe(const graph::NkNodeGraph &g, const graph::NkSocket &s) {
				const NkString *t = g.TypeName(s.type);
				return t != nullptr ? t->CStr() : "?";
			}
			bool EstCode(const graph::NkNode &n) {
				return n.type == NK_BP_EXPRESSION || n.type == NK_BP_SI_EXPRESSION || n.type == NK_BP_CODE;
			}

			NkColor CouleurType(void *, const graph::NkNodeGraph &g, graph::NkTypeId t) {
				const NkString *nom = g.TypeName(t);
				return editorkit::NkCouleurTypeNodal(nom != nullptr ? nom->CStr() : nullptr);
			}

			NkNatureNoeud Nature(void *, const graph::NkNode &n) {
				switch (NkBpGenre(n)) {
					case NkGenreNoeudBp::NK_EVENEMENT:
					case NkGenreNoeudBp::NK_REP_EVENEMENT:
						return NkNatureNoeud::NK_EVENEMENT;
					case NkGenreNoeudBp::NK_SI:
					case NkGenreNoeudBp::NK_SEQUENCE:
					case NkGenreNoeudBp::NK_POUR:
					case NkGenreNoeudBp::NK_TANT_QUE:
					case NkGenreNoeudBp::NK_SI_EXPRESSION:
						return NkNatureNoeud::NK_FLUX;
					case NkGenreNoeudBp::NK_CODE:
					case NkGenreNoeudBp::NK_REP_APPELER:
						return NkNatureNoeud::NK_ACTION;
					case NkGenreNoeudBp::NK_NATIF: {
						const NkProtoBp *p = NkBpProto(n.type.CStr());
						uint32 nb = 0;
						return p != nullptr && unkeny::NkNatifsBp(nb)[p->natif].pur ? NkNatureNoeud::NK_VALEUR : NkNatureNoeud::NK_ACTION;
					}
					case NkGenreNoeudBp::NK_MATH:
					case NkGenreNoeudBp::NK_EXPRESSION:
						return NkNatureNoeud::NK_VALEUR;
					case NkGenreNoeudBp::NK_SOI:
						return NkNatureNoeud::NK_CONTEXTE;
					case NkGenreNoeudBp::NK_VAR_GET:
					case NkGenreNoeudBp::NK_VAR_SET:
					case NkGenreNoeudBp::NK_LIRE_VAR:
					case NkGenreNoeudBp::NK_ECRIRE_VAR:
						return NkNatureNoeud::NK_VARIABLE;
					case NkGenreNoeudBp::NK_APPEL_FN:
					case NkGenreNoeudBp::NK_FN_ENTREE:
					case NkGenreNoeudBp::NK_FN_RETOUR:
					case NkGenreNoeudBp::NK_MACRO:
					case NkGenreNoeudBp::NK_MACRO_ENTREE:
					case NkGenreNoeudBp::NK_MACRO_SORTIE:
						return NkNatureNoeud::NK_FONCTION;
					default:
						return NkNatureNoeud::NK_DEFAUT;
				}
			}

			NkFormeNoeud Forme(void *, const graph::NkNode &n) {
				switch (NkBpGenre(n)) {
					case NkGenreNoeudBp::NK_VAR_GET:
						return NkFormeNoeud::NK_COMPACT;
					case NkGenreNoeudBp::NK_RELAIS:
						return NkFormeNoeud::NK_RELAIS;
					case NkGenreNoeudBp::NK_COMMENTAIRE:
						return NkFormeNoeud::NK_COMMENTAIRE;
					default:
						return NkFormeNoeud::NK_NORMAL;
				}
			}

			NkString SousTitre(void *, const graph::NkNodeGraph &g, const graph::NkNode &n) {
				switch (NkBpGenre(n)) {
					case NkGenreNoeudBp::NK_EVENEMENT:
					case NkGenreNoeudBp::NK_REP_EVENEMENT:
						return NkString("Événement");
					case NkGenreNoeudBp::NK_SI:
					case NkGenreNoeudBp::NK_SEQUENCE:
					case NkGenreNoeudBp::NK_POUR:
					case NkGenreNoeudBp::NK_TANT_QUE:
						return NkString("Flot");
					case NkGenreNoeudBp::NK_SI_EXPRESSION:
						return NkString("Flot · condition écrite");
					case NkGenreNoeudBp::NK_CODE:
						return NkString("Code · instructions");
					case NkGenreNoeudBp::NK_EXPRESSION:
						return NkString("Code · expression");
					case NkGenreNoeudBp::NK_NATIF: {
						const NkProtoBp *p = NkBpProto(n.type.CStr());
						uint32 nb = 0;
						if (p == nullptr) {
							return NkString();
						}
						const unkeny::NkNatifBp &x = unkeny::NkNatifsBp(nb)[p->natif];
						return NkString(x.pur ? "Valeur · " : "Action · ") + x.categorie;
					}
					case NkGenreNoeudBp::NK_MATH:
						return NkString("Maths");
					case NkGenreNoeudBp::NK_SOI:
						return NkString("Contexte");
					case NkGenreNoeudBp::NK_VAR_SET:
					case NkGenreNoeudBp::NK_ECRIRE_VAR:
						return NkString("Variable");
					case NkGenreNoeudBp::NK_LIRE_VAR:
						return NkString("Variable (ancien nœud)");
					case NkGenreNoeudBp::NK_APPEL_FN:
						return NkString("Fonction appelée");
					case NkGenreNoeudBp::NK_FN_ENTREE:
						return NkString("Entrée de la fonction");
					case NkGenreNoeudBp::NK_FN_RETOUR:
						return NkString("Retour de la fonction");
					case NkGenreNoeudBp::NK_MACRO:
						return NkString("Macro");
					case NkGenreNoeudBp::NK_MACRO_ENTREE:
					case NkGenreNoeudBp::NK_MACRO_SORTIE:
						return NkString("Frontière de la macro");
					case NkGenreNoeudBp::NK_REP_APPELER:
						return NkString("Répartiteur");
					default:
						(void)g;
						return NkString();
				}
			}

			NkString LibellePrise(void *, const graph::NkNodeGraph &g, const graph::NkNode &n, const graph::NkSocket &s) {
				(void)g;
				const NkGenreNoeudBp genre = NkBpGenre(n);
				if (s.family == graph::NkSocketFamily::Exec) {
					struct L {
							const char *cle;
							const char *lib;
					};
					static const L k[] = {{"exec", ""},		{"suite", "Terminé"}, {"vrai", "Vrai"},	  {"faux", "Faux"},
										  {"alors0", "Alors 0"}, {"alors1", "Alors 1"}, {"alors2", "Alors 2"}, {"corps", "Corps de boucle"},
										  {"fini", "Terminé"}};
					for (const L &l : k) {
						if (s.name == l.cle) {
							// Un evenement : sa suite « Déclencher » (planche 03).
							if (genre == NkGenreNoeudBp::NK_EVENEMENT || genre == NkGenreNoeudBp::NK_REP_EVENEMENT) {
								return NkString("Déclencher");
							}
							return NkString(l.lib);
						}
					}
					return s.name;
				}
				if (s.name == "soiEstLaZone") {
					return NkString("soi est la zone");
				}
				if (genre == NkGenreNoeudBp::NK_VAR_SET && s.name == "valeur") {
					return s.dir == graph::NkSocketDir::Input ? NkBpPropTexte(g, n, "var") : NkString("valeur");
				}
				return s.name;
			}

			NkString TexteDefaut(void *, const graph::NkNodeGraph &g, const graph::NkNode &n, const graph::NkSocket &s) {
				const char *t = TypeDe(g, s);
				if (std::strcmp(t, "entite") == 0) {
					return NkString(); // une entite libre vaut « soi » : pas de champ
				}
				NkString v = NkBpTexteDefaut(g, n, s);
				if (v.Empty() && (std::strcmp(t, "texte") == 0 || std::strcmp(t, "asset") == 0)) {
					v = "…"; // un texte vide reste saisissable
				}
				return v;
			}
			bool PoserDefaut(void *d, graph::NkNodeGraph &g, graph::NkNodeId n, const char *prise, const char *texte) {
				const char *t = std::strcmp(texte, "…") == 0 ? "" : texte;
				const bool ok = NkBpPoserDefaut(g, n, prise, t);
				if (ok) {
					E(d).modifie = true;
				}
				return ok;
			}
			NkGenreChamp Champ(void *, const graph::NkNodeGraph &g, const graph::NkNode &, const graph::NkSocket &s) {
				const char *t = TypeDe(g, s);
				if (std::strcmp(t, "booleen") == 0) {
					return NkGenreChamp::NK_CASE;
				}
				if (std::strcmp(t, "couleur") == 0) {
					return NkGenreChamp::NK_NUANCIER;
				}
				if (std::strcmp(t, "entite") == 0) {
					return NkGenreChamp::NK_AUCUN;
				}
				return NkGenreChamp::NK_TEXTE;
			}

			int32 Compte(void *d, const graph::NkNodeGraph &g, const graph::NkNode &n) {
				NkEditeurBlueprintEtat &e = E(d);
				if (e.trace == nullptr || e.trace->evenements == 0u) {
					return -1;
				}
				// Le graphe de `g` dans le document : son indice fait le code du noeud.
				for (uint32 i = 0; i < e.doc.graphes.Size(); ++i) {
					if (&e.doc.graphes[i].graphe == &g) {
						const NkGenreNoeudBp genre = NkBpGenre(n);
						if (genre == NkGenreNoeudBp::NK_COMMENTAIRE) {
							return -1;
						}
						return static_cast<int32>(e.trace->Compte(NkBpCodeNoeud(i, n.id)));
					}
				}
				return -1;
			}

			graph::NkNodeId CreerRelais(void *d, graph::NkNodeGraph &g, const char *type, float32 x, float32 y) {
				E(d).modifie = true;
				return NkBpCreerRelais(g, type, x, y);
			}
			graph::NkNodeId CreerCommentaire(void *d, graph::NkNodeGraph &g, float32 x, float32 y, float32 w, float32 h) {
				E(d).modifie = true;
				return NkBpCreerCommentaire(g, "Commentaire", x, y, w, h);
			}
			void TailleCommentaire(void *, const graph::NkNodeGraph &g, const graph::NkNode &n, float32 &w, float32 &h) {
				w = NkBpPropReel(g, n, "w", 320.f);
				h = NkBpPropReel(g, n, "h", 180.f);
			}
			void PoserTailleCommentaire(void *, graph::NkNodeGraph &g, graph::NkNodeId n, float32 w, float32 h) {
				NkBpPoserPropReel(g, n, "w", w);
				NkBpPoserPropReel(g, n, "h", h);
			}
			bool Supprimable(void *, const graph::NkNode &n) {
				const NkGenreNoeudBp genre = NkBpGenre(n);
				return genre != NkGenreNoeudBp::NK_FN_ENTREE && genre != NkGenreNoeudBp::NK_MACRO_ENTREE && genre != NkGenreNoeudBp::NK_MACRO_SORTIE;
			}
			float32 LargeurMin(void *, const graph::NkNode &n) {
				return EstCode(n) ? 330.f : 0.f;
			}

			// ═════════════════════════════════════════════════════════════════
			// LE BLOC DE CODE : « + ajouter une entrée », le code colore, « Snippets »
			// ═════════════════════════════════════════════════════════════════
			constexpr float32 kRangeeAjout = 22.f;
			constexpr float32 kLigneCode = 16.f;
			constexpr float32 kRangeeSnippets = 22.f;

			/// Le code a montrer : celui en cours d'edition, sinon celui du noeud.
			NkString CodeMontre(const NkEditeurBlueprintEtat &e, const graph::NkNodeGraph &g, const graph::NkNode &n) {
				if (e.codeNoeud == n.id && &e.doc.graphes[e.codeGraphe].graphe == &g) {
					return NkString(e.code);
				}
				return NkBpCodeNoeud(g, n);
			}

			/// Les lignes AFFICHEES (sauts du code, puis coupe a la largeur).
			struct LigneAff {
					uint32 debut = 0u, fin = 0u; ///< octets dans le code
			};
			void Couper(const char *code, nkgui::NkGuiFont *f, float32 largeur, float32 echelle, NkVector<LigneAff> &lignes) {
				lignes.Clear();
				const uint32 n = static_cast<uint32>(std::strlen(code));
				uint32 a = 0;
				while (a <= n) {
					uint32 b = a;
					while (b < n && code[b] != '\n') {
						++b;
					}
					// [a, b) : une ligne du code ; coupee si trop large.
					uint32 x = a;
					while (true) {
						uint32 fin = b;
						if (f != nullptr && f->Face() != nullptr) {
							NkString morceau(code + x, b - x);
							if (f->MeasureWidth(morceau.CStr()) * echelle > largeur && b - x > 1u) {
								// Le plus long prefixe qui tient (aux espaces si possible).
								uint32 bon = x + 1u, espace = 0u;
								for (uint32 k = x + 1u; k <= b; ++k) {
									if ((static_cast<unsigned char>(code[k - 1u]) & 0xC0u) == 0x80u && k < b) {
										continue;
									}
									NkString m2(code + x, k - x);
									if (f->MeasureWidth(m2.CStr()) * echelle > largeur) {
										break;
									}
									bon = k;
									if (code[k - 1u] == ' ') {
										espace = k;
									}
								}
								fin = espace > x ? espace : bon;
							}
						}
						LigneAff l;
						l.debut = x;
						l.fin = fin;
						lignes.PushBack(l);
						if (fin >= b) {
							break;
						}
						x = fin;
					}
					a = b + 1u;
				}
			}

			float32 HauteurBloc(void *d, const graph::NkNodeGraph &g, const graph::NkNode &n, float32 largeur) {
				if (!EstCode(n)) {
					return 0.f;
				}
				NkEditeurBlueprintEtat &e = E(d);
				const NkString code = CodeMontre(e, g, n);
				// MESUREE avec la police de l'editeur, coupee comme `Bloc` la coupera
				// (a z = 1 : largeur - 40, echelle 0,92) ; sans police, une estimation.
				if (e.police != nullptr && e.police->Face() != nullptr) {
					NkVector<LigneAff> aff;
					Couper(code.CStr(), e.police, largeur - 40.f, 0.92f, aff);
					const uint32 nl = static_cast<uint32>(aff.Size()) < 2u ? 2u : static_cast<uint32>(aff.Size());
					NkArbreExpr ar0;
					const bool err0 = !code.Empty() && !NkBpAnalyserExpr(code.CStr(), n.type == NK_BP_CODE, ar0);
					return kRangeeAjout + static_cast<float32>(nl) * kLigneCode + 14.f + (err0 ? 16.f : 0.f) + kRangeeSnippets;
				}
				uint32 lignes = 1u;
				uint32 col = 0u;
				const uint32 maxCol = static_cast<uint32>((largeur - 40.f) / 7.2f);
				for (const char *p = code.CStr(); *p != '\0'; ++p) {
					if (*p == '\n') {
						++lignes;
						col = 0u;
					} else if ((static_cast<unsigned char>(*p) & 0xC0u) != 0x80u && ++col > maxCol) {
						++lignes;
						col = 1u;
					}
				}
				lignes = lignes < 2u ? 2u : lignes;
				NkArbreExpr ar;
				const bool erreur = !code.Empty() && !NkBpAnalyserExpr(code.CStr(), n.type == NK_BP_CODE, ar);
				return kRangeeAjout + static_cast<float32>(lignes) * kLigneCode + 14.f + (erreur ? 16.f : 0.f) + kRangeeSnippets;
			}

			void TexteE(nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *f, float32 x, float32 top, const char *s, const NkColor &c, float32 z,
						float32 maxW = -1.f) {
				if (f == nullptr || f->Face() == nullptr || s == nullptr || s[0] == '\0') {
					return;
				}
				dl.AddTextScaled(f->Face(), f->TexId(), NkVec2{x, top + f->Ascent() * z}, s, c, z, maxW);
			}

			bool Bloc(void *d, nkgui::NkGuiDrawList &dl, nkgui::NkGuiInput &in, nkgui::NkGuiFont *f, graph::NkNodeGraph &g, graph::NkNodeId id,
					  const NkRect &r, float32 z, bool survole) {
				NkEditeurBlueprintEtat &e = E(d);
				const editorkit::NkJetonsNodal &s = editorkit::NkJetonsNodalParDefaut();
				graph::NkNode *n = g.Find(id);
				if (n == nullptr) {
					return false;
				}
				bool change = false;
				const NkVec2 souris = in.mousePos;
				const float32 lh = f != nullptr && f->Face() != nullptr ? f->LineHeight() : 14.f;
				const bool instructions = n->type == NK_BP_CODE;
				// ── « + ajouter une entrée » : une rangee pointillee (planche 01) ──
				const NkRect ra{r.x + 10.f * z, r.y, r.w - 20.f * z, (kRangeeAjout - 4.f) * z};
				const bool surAjout = survole && souris.x >= ra.x && souris.y >= ra.y && souris.x < ra.x + ra.w && souris.y < ra.y + ra.h;
				dl.AddRectFilled(ra, surAjout ? NkColor(38, 38, 46, 255) : NkColor(30, 30, 36, 255), 2.f * z);
				{
					const char *t = "+  ajouter une entrée";
					const float32 tw = f != nullptr ? f->MeasureWidth(t) * z * 0.86f : 0.f;
					TexteE(dl, f, ra.x + (ra.w - tw) * 0.5f, ra.y + (ra.h - lh * 0.86f * z) * 0.5f, t, s.attenue, z * 0.86f);
				}
				if (surAjout && in.mouseClicked[0]) {
					const NkString nom = NkBpCodeNomLibre(*n, graph::NkSocketDir::Input);
					graph::NkNodeId nid = id;
					if (NkBpCodeAjouterPrise(g, nid, nom.CStr(), "reel", graph::NkSocketDir::Input)) {
						if (e.Toile().selection == id) {
							e.Toile().Choisir(nid);
						}
						if (e.codeNoeud == id) {
							e.codeNoeud = nid;
						}
						change = true;
					}
					in.mouseClicked[0] = false;
					return change; // le noeud a ete refait : on redessinera a la trame suivante
				}
				// ── Le CODE, colore (les couleurs de la reference principale) ──
				const NkString code = CodeMontre(e, g, *n);
				const bool enEdition = e.codeNoeud == id && &e.doc.graphes[e.codeGraphe].graphe == &g;
				NkArbreExpr ar;
				const bool syntaxeOk = code.Empty() || NkBpAnalyserExpr(code.CStr(), instructions, ar);
				NkVector<LigneAff> lignes;
				const float32 ech = z * 0.92f;
				Couper(code.CStr(), f, r.w - 40.f * z, ech, lignes);
				const uint32 nl = static_cast<uint32>(lignes.Size()) < 2u ? 2u : static_cast<uint32>(lignes.Size());
				const NkRect rc{r.x + 10.f * z, r.y + kRangeeAjout * z, r.w - 20.f * z, (static_cast<float32>(nl) * kLigneCode + 10.f) * z};
				const bool surCode = survole && souris.x >= rc.x && souris.y >= rc.y && souris.x < rc.x + rc.w && souris.y < rc.y + rc.h;
				dl.AddRectFilled(rc, s.codeFond, 3.f * z);
				dl.AddRect(rc, enEdition ? s.exec : (syntaxeOk ? NkColor(40, 40, 48, 255) : s.erreur), enEdition ? 1.5f : 1.f, 3.f * z);
				NkVector<NkJetonExpr> jetons;
				NkBpLexerExpr(code.CStr(), jetons);
				float32 caretX = -1.f, caretY = 0.f;
				for (uint32 li = 0; li < lignes.Size(); ++li) {
					const LigneAff &l = lignes[li];
					const float32 y = rc.y + 5.f * z + static_cast<float32>(li) * kLigneCode * z;
					float32 x = rc.x + 6.f * z;
					uint32 o = l.debut;
					// Les jetons de cette ligne, et les blancs entre eux.
					for (uint32 k = 0; k < jetons.Size() && o < l.fin; ++k) {
						const NkJetonExpr &j = jetons[k];
						if (j.fin <= o || j.genre == NkGenreJetonExpr::NK_FIN) {
							continue;
						}
						if (j.debut >= l.fin) {
							break;
						}
						// Le blanc avant le jeton.
						if (j.debut > o) {
							const NkString blanc(code.CStr() + o, j.debut - o);
							x += f != nullptr ? f->MeasureWidth(blanc.CStr()) * ech : 0.f;
							o = j.debut;
						}
						const uint32 fin = j.fin < l.fin ? j.fin : l.fin;
						const NkString texte(code.CStr() + o, fin - o);
						NkColor c = s.codeAutre;
						switch (j.genre) {
							case NkGenreJetonExpr::NK_NOMBRE:
								c = s.codeNombre;
								break;
							case NkGenreJetonExpr::NK_TEXTE:
								c = s.codeTexte;
								break;
							case NkGenreJetonExpr::NK_OP:
								c = s.codeOperateur;
								break;
							case NkGenreJetonExpr::NK_ERREUR:
								c = s.codeErreur;
								break;
							case NkGenreJetonExpr::NK_IDENT: {
								const bool appel = k + 1u < jetons.Size() && jetons[k + 1u].genre == NkGenreJetonExpr::NK_OP && jetons[k + 1u].texte == "(";
								const bool mot = j.texte == "vrai" || j.texte == "faux" || j.texte == "soi" || j.texte == "et" || j.texte == "ou" ||
												 j.texte == "non" || j.texte == "true" || j.texte == "false";
								c = appel ? s.codeFonction : (mot ? s.codeNombre : s.codeIdent);
								break;
							}
							default:
								break;
						}
						TexteE(dl, f, x, y, texte.CStr(), c, ech);
						if (enEdition && e.caret >= o && e.caret <= fin && caretX < 0.f) {
							const NkString avant(code.CStr() + o, e.caret - o);
							caretX = x + (f != nullptr ? f->MeasureWidth(avant.CStr()) * ech : 0.f);
							caretY = y;
						}
						x += f != nullptr ? f->MeasureWidth(texte.CStr()) * ech : 0.f;
						o = fin;
					}
					if (enEdition && caretX < 0.f && e.caret >= l.debut && e.caret <= l.fin) {
						const NkString avant(code.CStr() + l.debut, e.caret - l.debut);
						caretX = rc.x + 6.f * z + (f != nullptr ? f->MeasureWidth(avant.CStr()) * ech : 0.f);
						caretY = y;
					}
				}
				if (code.Empty() && !enEdition) {
					TexteE(dl, f, rc.x + 6.f * z, rc.y + 5.f * z, instructions ? "écrire des instructions…" : "écrire une expression…", s.attenue, ech);
				}
				if (enEdition) {
					e.clignote += in.dt > 0.f ? in.dt : 1.f / 60.f;
					if (caretX < 0.f) {
						caretX = rc.x + 6.f * z;
						caretY = rc.y + 5.f * z;
					}
					if (static_cast<int32>(e.clignote * 2.f) % 2 == 0) {
						dl.AddLine(NkVec2{caretX, caretY}, NkVec2{caretX, caretY + lh * ech}, s.texteEntete, 1.f);
					}
				}
				// L'icone « pinceau » de la reference, a droite : ouvrir l'edition.
				if (!enEdition && surCode && in.mouseClicked[0]) {
					e.codeNoeud = id;
					for (uint32 i = 0; i < e.doc.graphes.Size(); ++i) {
						if (&e.doc.graphes[i].graphe == &g) {
							e.codeGraphe = i;
						}
					}
					std::snprintf(e.code, sizeof(e.code), "%s", code.CStr());
					e.caret = static_cast<uint32>(std::strlen(e.code));
					e.clignote = 0.f;
					in.mouseClicked[0] = false;
				}
				float32 y = rc.y + rc.h + 4.f * z;
				// La raison d'une erreur de SYNTAXE, ecrite (la charte : lisible sans survol).
				if (!syntaxeOk) {
					const NkString m = NkString::Format("ligne %u, col. %u : %s", static_cast<unsigned>(ar.ligne), static_cast<unsigned>(ar.colonne), ar.erreur.CStr());
					TexteE(dl, f, r.x + 10.f * z, y, m.CStr(), s.erreur, z * 0.8f, r.w - 20.f * z);
					y += 16.f * z;
				}
				// ── « Snippets ▾ » ──
				const NkRect rs{r.x + 10.f * z, y, 90.f * z, (kRangeeSnippets - 4.f) * z};
				const bool surSnip = survole && souris.x >= rs.x && souris.y >= rs.y && souris.x < rs.x + rs.w && souris.y < rs.y + rs.h;
				TexteE(dl, f, rs.x, rs.y + (rs.h - lh * 0.86f * z) * 0.5f, "Snippets", surSnip ? s.texteEntete : s.attenue, z * 0.86f);
				{
					const float32 cx = rs.x + 62.f * z, cy = rs.y + rs.h * 0.5f;
					dl.AddTriangleFilled(NkVec2{cx - 3.5f * z, cy - 2.f * z}, NkVec2{cx + 3.5f * z, cy - 2.f * z}, NkVec2{cx, cy + 2.5f * z}, s.attenue);
				}
				if (surSnip && in.mouseClicked[0]) {
					// Le menu des snippets : l'editeur l'ouvre (il passe au-dessus de tout).
					e.popup = NkPopupBp();
					e.popup.ouvert = true;
					e.popup.genre = 7u; // snippets
					e.popup.x = rs.x;
					e.popup.y = rs.y + rs.h + 2.f;
					e.popup.cible = static_cast<int32>(id);
					uint32 nb = 0;
					const char *const *sn = NkBpSnippets(nb, instructions);
					for (uint32 i = 0; i < nb; ++i) {
						e.popup.libelles.PushBack(NkString(sn[i]));
						e.popup.ids.PushBack(static_cast<int32>(i));
						e.popup.grises.PushBack(false);
					}
					in.mouseClicked[0] = false;
				}
				return change;
			}

			NkString Erreur(void *d, const graph::NkNodeGraph &g, const graph::NkNode &n) {
				NkEditeurBlueprintEtat &e = E(d);
				// L'erreur de la derniere compilation, sur SON noeud de SON graphe.
				if (e.erreur && e.derniereErreur.noeud == n.id && e.derniereErreur.graphe < e.doc.graphes.Size() &&
					&e.doc.graphes[e.derniereErreur.graphe].graphe == &g) {
					return e.derniereErreur.message;
				}
				return NkString();
			}
		} // namespace

		editorkit::NkDomaineCanevas NkEditeurBlueprintDomaine(NkEditeurBlueprintEtat &e) {
			editorkit::NkDomaineCanevas d;
			d.donnees = &e;
			d.CouleurType = &CouleurType;
			d.Nature = &Nature;
			d.Forme = &Forme;
			d.SousTitre = &SousTitre;
			d.LibellePrise = &LibellePrise;
			d.TexteDefaut = &TexteDefaut;
			d.PoserDefaut = &PoserDefaut;
			d.Champ = &Champ;
			d.Compte = &Compte;
			d.CreerRelais = &CreerRelais;
			d.CreerCommentaire = &CreerCommentaire;
			d.TailleCommentaire = &TailleCommentaire;
			d.PoserTailleCommentaire = &PoserTailleCommentaire;
			d.Supprimable = &Supprimable;
			d.LargeurMin = &LargeurMin;
			d.HauteurBloc = &HauteurBloc;
			d.Bloc = &Bloc;
			d.Erreur = &Erreur;
			return d;
		}

	} // namespace editeur
} // namespace nkentseu
