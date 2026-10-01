// -----------------------------------------------------------------------------
// FICHIER: UnkenyEditor/Script/NkBpExpression.cpp
// DESCRIPTION: Le lexeur et l'analyseur du langage des noeuds de code
//              (NkBpExpression.h).
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Script/NkBpExpression.h"

#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		namespace {
			bool Lettre(unsigned char c) {
				return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$' || c >= 0x80u;
			}
			bool Chiffre(unsigned char c) {
				return c >= '0' && c <= '9';
			}
		} // namespace

		void NkBpLexerExpr(const char *code, NkVector<NkJetonExpr> &sortie) {
			sortie.Clear();
			const char *p = code != nullptr ? code : "";
			const char *debut = p;
			auto jeton = [&](NkGenreJetonExpr g, const char *a, const char *b) {
				NkJetonExpr j;
				j.genre = g;
				j.texte = NkString(a, static_cast<usize>(b - a));
				j.debut = static_cast<uint32>(a - debut);
				j.fin = static_cast<uint32>(b - debut);
				sortie.PushBack(j);
			};
			while (*p != '\0') {
				const unsigned char c = static_cast<unsigned char>(*p);
				if (c == ' ' || c == '\t' || c == '\r') {
					++p;
					continue;
				}
				if (c == '\n') {
					jeton(NkGenreJetonExpr::NK_OP, p, p + 1);
					++p;
					continue;
				}
				if (Chiffre(c) || (c == '.' && Chiffre(static_cast<unsigned char>(p[1])))) {
					const char *a = p;
					while (Chiffre(static_cast<unsigned char>(*p))) {
						++p;
					}
					if (*p == '.' && Chiffre(static_cast<unsigned char>(p[1]))) {
						++p;
						while (Chiffre(static_cast<unsigned char>(*p))) {
							++p;
						}
					}
					jeton(NkGenreJetonExpr::NK_NOMBRE, a, p);
					continue;
				}
				if (Lettre(c)) {
					const char *a = p;
					while (Lettre(static_cast<unsigned char>(*p)) || Chiffre(static_cast<unsigned char>(*p))) {
						++p;
					}
					jeton(NkGenreJetonExpr::NK_IDENT, a, p);
					continue;
				}
				if (c == '"' || c == '\'') {
					const char q = *p;
					const char *a = p++;
					while (*p != '\0' && *p != q && *p != '\n') {
						++p;
					}
					if (*p == q) {
						++p;
						jeton(NkGenreJetonExpr::NK_TEXTE, a, p);
					} else {
						jeton(NkGenreJetonExpr::NK_ERREUR, a, p);
					}
					continue;
				}
				static const char *const doubles[] = {"&&", "||", "==", "!=", "<=", ">="};
				bool fait = false;
				for (const char *d : doubles) {
					if (p[0] == d[0] && p[1] == d[1]) {
						jeton(NkGenreJetonExpr::NK_OP, p, p + 2);
						p += 2;
						fait = true;
						break;
					}
				}
				if (fait) {
					continue;
				}
				if (std::strchr("+-*/<>!(),.=;", static_cast<int>(c)) != nullptr) {
					jeton(NkGenreJetonExpr::NK_OP, p, p + 1);
					++p;
					continue;
				}
				jeton(NkGenreJetonExpr::NK_ERREUR, p, p + 1);
				++p;
			}
			NkJetonExpr fin;
			fin.genre = NkGenreJetonExpr::NK_FIN;
			fin.debut = fin.fin = static_cast<uint32>(p - debut);
			sortie.PushBack(fin);
		}

		namespace {
			struct Analyseur {
					const char *code;
					NkVector<NkJetonExpr> j;
					uint32 i = 0;
					NkArbreExpr &a;
					bool instructions = false;

					Analyseur(const char *c, NkArbreExpr &arbre) : code(c), a(arbre) {
					}

					/// La colonne (1..) et la ligne (1..) de l'octet `o`.
					void Position(uint32 o, uint32 &ligne, uint32 &colonne) const {
						ligne = 1u;
						colonne = 1u;
						for (uint32 k = 0; k < o && code[k] != '\0'; ++k) {
							if (code[k] == '\n') {
								++ligne;
								colonne = 1u;
							} else if ((static_cast<unsigned char>(code[k]) & 0xC0u) != 0x80u) {
								++colonne;
							}
						}
					}
					bool Echec(const NkString &m) {
						if (a.erreur.Empty()) {
							Position(j[i].debut, a.ligne, a.colonne);
							a.erreur = m;
						}
						return false;
					}
					const NkJetonExpr &Cour() const {
						return j[i];
					}
					bool EstOp(const char *op) const {
						return j[i].genre == NkGenreJetonExpr::NK_OP && j[i].texte == op;
					}
					bool EstMot(const char *m) const {
						return j[i].genre == NkGenreJetonExpr::NK_IDENT && j[i].texte == m;
					}
					void SauterLignes() {
						while (!instructions && EstOp("\n")) {
							++i;
						}
					}
					int32 Nouveau(NkGenreNoeudExpr g, uint32 o) {
						NkNoeudExpr n;
						n.genre = g;
						Position(o, n.ligne, n.colonne);
						a.noeuds.PushBack(n);
						return static_cast<int32>(a.noeuds.Size() - 1u);
					}

					int32 Primaire() {
						SauterLignes();
						const NkJetonExpr t = Cour();
						if (t.genre == NkGenreJetonExpr::NK_NOMBRE) {
							++i;
							const int32 n = Nouveau(NkGenreNoeudExpr::NK_NOMBRE, t.debut);
							a.noeuds[static_cast<uint32>(n)].nombre = std::strtod(t.texte.CStr(), nullptr);
							a.noeuds[static_cast<uint32>(n)].entier = std::strchr(t.texte.CStr(), '.') == nullptr;
							return n;
						}
						if (t.genre == NkGenreJetonExpr::NK_TEXTE) {
							++i;
							const int32 n = Nouveau(NkGenreNoeudExpr::NK_TEXTE, t.debut);
							a.noeuds[static_cast<uint32>(n)].nom = NkString(t.texte.CStr() + 1, t.texte.Length() - 2u);
							return n;
						}
						if (t.genre == NkGenreJetonExpr::NK_IDENT) {
							++i;
							if (t.texte == "vrai" || t.texte == "faux" || t.texte == "true" || t.texte == "false") {
								const int32 n = Nouveau(NkGenreNoeudExpr::NK_BOOLEEN, t.debut);
								a.noeuds[static_cast<uint32>(n)].nombre = (t.texte == "vrai" || t.texte == "true") ? 1.0 : 0.0;
								return n;
							}
							if (t.texte == "soi" || t.texte == "self") {
								return Nouveau(NkGenreNoeudExpr::NK_SOI, t.debut);
							}
							NkString nom = t.texte;
							if (nom.Length() > 1u && nom.CStr()[0] == '$') {
								nom = NkString(nom.CStr() + 1); // « $round » : la reference
							}
							if (EstOp("(")) {
								++i;
								const int32 n = Nouveau(NkGenreNoeudExpr::NK_APPEL, t.debut);
								a.noeuds[static_cast<uint32>(n)].nom = nom;
								SauterLignes();
								if (!EstOp(")")) {
									for (;;) {
										const int32 x = Expr();
										if (x < 0) {
											return -1;
										}
										a.noeuds[static_cast<uint32>(n)].args.PushBack(x);
										SauterLignes();
										if (EstOp(",")) {
											++i;
											continue;
										}
										break;
									}
								}
								if (!EstOp(")")) {
									Echec(NkString("« ) » attendu"));
									return -1;
								}
								++i;
								return n;
							}
							const int32 n = Nouveau(NkGenreNoeudExpr::NK_NOM, t.debut);
							a.noeuds[static_cast<uint32>(n)].nom = nom;
							return n;
						}
						if (EstOp("(")) {
							++i;
							const int32 x = Expr();
							SauterLignes();
							if (x < 0) {
								return -1;
							}
							if (!EstOp(")")) {
								Echec(NkString("« ) » attendu"));
								return -1;
							}
							++i;
							return x;
						}
						if (t.genre == NkGenreJetonExpr::NK_FIN || EstOp("\n") || EstOp(";")) {
							Echec(NkString("une valeur est attendue ici (l'expression s'arrête trop tôt)"));
						} else if (t.genre == NkGenreJetonExpr::NK_ERREUR) {
							Echec(NkString::Format("caractère inattendu « %s »", t.texte.CStr()));
						} else {
							Echec(NkString::Format("« %s » inattendu", t.texte.CStr()));
						}
						return -1;
					}
					int32 Postfixe() {
						int32 x = Primaire();
						while (x >= 0 && EstOp(".")) {
							const uint32 o = Cour().debut;
							++i;
							if (Cour().genre != NkGenreJetonExpr::NK_IDENT) {
								Echec(NkString("un membre est attendu après « . » (x ou y)"));
								return -1;
							}
							const int32 m = Nouveau(NkGenreNoeudExpr::NK_MEMBRE, o);
							a.noeuds[static_cast<uint32>(m)].nom = Cour().texte;
							a.noeuds[static_cast<uint32>(m)].a = x;
							++i;
							x = m;
						}
						return x;
					}
					int32 Unaire() {
						SauterLignes();
						if (EstOp("-") || EstOp("!") || EstMot("non") || EstMot("not")) {
							const uint32 o = Cour().debut;
							const NkString op = EstOp("-") ? NkString("-") : NkString("!");
							++i;
							const int32 x = Unaire();
							if (x < 0) {
								return -1;
							}
							const int32 n = Nouveau(NkGenreNoeudExpr::NK_UNAIRE, o);
							a.noeuds[static_cast<uint32>(n)].nom = op;
							a.noeuds[static_cast<uint32>(n)].a = x;
							return n;
						}
						return Postfixe();
					}
					int32 Binaire(int32 niveau) {
						static const char *const ops[5][6] = {{"||", "ou", "or", nullptr},
															  {"&&", "et", "and", nullptr},
															  {"==", "!=", "<", "<=", ">", ">="},
															  {"+", "-", nullptr},
															  {"*", "/", nullptr}};
						if (niveau >= 5) {
							return Unaire();
						}
						int32 x = Binaire(niveau + 1);
						for (;;) {
							if (x < 0) {
								return -1;
							}
							SauterLignes();
							const char *trouve = nullptr;
							for (int32 k = 0; k < 6 && ops[niveau][k] != nullptr; ++k) {
								if (EstOp(ops[niveau][k]) || EstMot(ops[niveau][k])) {
									trouve = ops[niveau][k];
								}
							}
							if (trouve == nullptr) {
								return x;
							}
							const uint32 o = Cour().debut;
							++i;
							const int32 y = Binaire(niveau + 1);
							if (y < 0) {
								return -1;
							}
							const int32 n = Nouveau(NkGenreNoeudExpr::NK_BINAIRE, o);
							NkString op(trouve);
							op = op == "ou" || op == "or" ? NkString("||") : (op == "et" || op == "and" ? NkString("&&") : op);
							a.noeuds[static_cast<uint32>(n)].nom = op;
							a.noeuds[static_cast<uint32>(n)].a = x;
							a.noeuds[static_cast<uint32>(n)].b = y;
							x = n;
						}
					}
					int32 Expr() {
						return Binaire(0);
					}
			};
		} // namespace

		bool NkBpAnalyserExpr(const char *code, bool instructions, NkArbreExpr &a) {
			a = NkArbreExpr();
			Analyseur an(code != nullptr ? code : "", a);
			an.instructions = instructions;
			NkBpLexerExpr(an.code, an.j);
			if (!instructions) {
				an.SauterLignes();
				if (an.Cour().genre == NkGenreJetonExpr::NK_FIN) {
					return an.Echec(NkString("expression vide : écrire un calcul (« a + b »)"));
				}
				NkInstructionExpr ins;
				ins.valeur = an.Expr();
				if (ins.valeur < 0) {
					return false;
				}
				an.SauterLignes();
				if (an.Cour().genre != NkGenreJetonExpr::NK_FIN) {
					return an.Echec(NkString::Format("« %s » de trop après l'expression", an.Cour().texte.CStr()));
				}
				a.instructions.PushBack(ins);
				return true;
			}
			for (;;) {
				while (an.EstOp("\n") || an.EstOp(";")) {
					++an.i;
				}
				if (an.Cour().genre == NkGenreJetonExpr::NK_FIN) {
					break;
				}
				NkInstructionExpr ins;
				// `nom = valeur` : une affectation (et pas « == »).
				if (an.Cour().genre == NkGenreJetonExpr::NK_IDENT && an.j[an.i + 1u].genre == NkGenreJetonExpr::NK_OP && an.j[an.i + 1u].texte == "=") {
					ins.cible = an.Cour().texte;
					uint32 l = 0;
					an.Position(an.Cour().debut, l, ins.colonneCible);
					an.i += 2u;
				}
				ins.valeur = an.Expr();
				if (ins.valeur < 0) {
					return false;
				}
				if (!an.EstOp("\n") && !an.EstOp(";") && an.Cour().genre != NkGenreJetonExpr::NK_FIN) {
					return an.Echec(NkString::Format("« %s » inattendu : une instruction par ligne (ou « ; »)", an.Cour().texte.CStr()));
				}
				a.instructions.PushBack(ins);
			}
			return true;
		}

		NkString NkBpCodeVersPropriete(const char *code) {
			NkString r;
			for (const char *p = code != nullptr ? code : ""; *p != '\0'; ++p) {
				if (*p == '\\') {
					r.Append("\\\\");
				} else if (*p == '\n') {
					r.Append("\\n");
				} else if (*p != '\r') {
					r.Append(*p);
				}
			}
			return r;
		}

		NkString NkBpCodeDePropriete(const char *propriete) {
			NkString r;
			for (const char *p = propriete != nullptr ? propriete : ""; *p != '\0'; ++p) {
				if (*p == '\\' && p[1] != '\0') {
					r.Append(p[1] == 'n' ? '\n' : p[1]);
					++p;
				} else {
					r.Append(*p);
				}
			}
			return r;
		}

		const char *const *NkBpSnippets(uint32 &nombre, bool instructions) {
			static const char *const expr[] = {"a + b", "arrondi(a / b * 100, 1)", "borner(a, 0, 1)", "interpoler(a, b, 0.5)", "min(a, b)",
											   "max(a, b)", "abs(a)", "racine(a)", "vec2(a, b)", "longueur(position(soi))"};
			static const char *const ins[] = {"afficher(\"Bonjour\")", "total = total + 1", "ouverte = vrai",
											  "teleporter(soi, position(soi) + vec2(0, 1))", "impulsion(soi, vec2(0, 5))",
											  "afficher(\"x = \" + texte(position(soi).x))"};
			if (instructions) {
				nombre = static_cast<uint32>(sizeof(ins) / sizeof(ins[0]));
				return ins;
			}
			nombre = static_cast<uint32>(sizeof(expr) / sizeof(expr[0]));
			return expr;
		}

	} // namespace editeur
} // namespace nkentseu
