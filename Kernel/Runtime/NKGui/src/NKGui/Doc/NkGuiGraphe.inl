// -----------------------------------------------------------------------------
// @File    NkGuiGraphe.inl
// @Brief   La compilation d'un Blueprint nodal vers le script.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#pragma once

namespace nkentseu {
	namespace nkgui {

		namespace detail {

			inline bool NkGGEspace(char c) noexcept {
				return c == ' ' || c == '\t' || c == '\r' || c == '\n';
			}

			inline bool NkGGMotEgal(const NkString &a, const char *b) noexcept {
				uint32 i = 0;
				for (; i < (uint32)a.Size(); ++i)
					if (b[i] == '\0' || a.CStr()[i] != b[i])
						return false;
				return b[i] == '\0';
			}

			/// Un nœud lu : son nom, son type, et ses broches écrites.
			struct NkGGBroche {
					NkString nom;
					NkString valeur; ///< telle qu'écrite : `"nom"`, `12`, `n2.value`
			};
			struct NkGGNoeud {
					NkString nom;
					NkString type;
					NkVector<NkGGBroche> broches;
					/// ⚠️ MARQUE D'EXPANSION EN COURS, et elle existe pour les
					///    cycles : un nœud pur qui se lit lui-même bouclerait
					///    indéfiniment. §6.2 nomme ce refus (« cycle de nœuds
					///    purs »).
					bool enCours = false;
			};
			/// Un fil : `de.broche -> vers.broche`.
			struct NkGGFil {
					NkString deNoeud, deBroche, versNoeud, versBroche;
			};

			struct NkGGGraphe {
					NkVector<NkGGNoeud> noeuds;
					NkVector<NkGGFil> fils;

					NkGGNoeud *Trouver(const NkString &n) noexcept {
						for (uint32 i = 0; i < (uint32)noeuds.Size(); ++i)
							if (noeuds[i].nom.Compare(n) == 0)
								return &noeuds[i];
						return nullptr;
					}
					/// La valeur écrite d'une broche d'entrée, ou vide.
					const NkString *Broche(const NkGGNoeud &n, const char *nom) const noexcept {
						for (uint32 i = 0; i < (uint32)n.broches.Size(); ++i)
							if (NkGGMotEgal(n.broches[i].nom, nom))
								return &n.broches[i].valeur;
						return nullptr;
					}
					/// Le nœud branché SUR cette entrée de données, s'il y en a un.
					const NkGGFil *FilVers(const NkString &noeud, const char *broche) const noexcept {
						for (uint32 i = 0; i < (uint32)fils.Size(); ++i)
							if (fils[i].versNoeud.Compare(noeud) == 0
								&& NkGGMotEgal(fils[i].versBroche, broche))
								return &fils[i];
						return nullptr;
					}
					/// La suite d'exécution partant de cette sortie.
					const NkGGFil *FilDe(const NkString &noeud, const char *broche) const noexcept {
						for (uint32 i = 0; i < (uint32)fils.Size(); ++i)
							if (fils[i].deNoeud.Compare(noeud) == 0
								&& NkGGMotEgal(fils[i].deBroche, broche))
								return &fils[i];
						return nullptr;
					}
			};

			/// Le lecteur de la tranche : `behavior "nom" (Evt)? graph { … }`.
			class NkGGLecteur {
				public:
					NkGGLecteur(NkStringView s) noexcept : mS(s), mI(0) {
					}

					void SauterEspaces() noexcept {
						while (mI < mS.Size() && NkGGEspace(mS.Data()[mI]))
							++mI;
					}

					bool Mot(const char *m) noexcept {
						SauterEspaces();
						uint32 k = 0;
						while (m[k] && mI + k < mS.Size() && mS.Data()[mI + k] == m[k])
							++k;
						if (m[k] != '\0')
							return false;
						// Un mot ne doit pas se confondre avec le début d'un autre.
						if (mI + k < mS.Size()) {
							const char c = mS.Data()[mI + k];
							if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
								|| (c >= '0' && c <= '9') || c == '_')
								return false;
						}
						mI += k;
						return true;
					}

					bool Car(char c) noexcept {
						SauterEspaces();
						if (mI < mS.Size() && mS.Data()[mI] == c) {
							++mI;
							return true;
						}
						return false;
					}

					char Regarder() noexcept {
						SauterEspaces();
						return mI < mS.Size() ? mS.Data()[mI] : '\0';
					}

					/// Un identifiant nu.
					bool Identifiant(NkString &out) noexcept {
						SauterEspaces();
						out = NkString();
						while (mI < mS.Size()) {
							const char c = mS.Data()[mI];
							const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
											|| (c >= '0' && c <= '9') || c == '_' || c == '.';
							if (!ok)
								break;
							const char d[2] = {c, '\0'};
							out += d;
							++mI;
						}
						return out.Size() > 0u;
					}

					/// Une chaîne entre guillemets, GUILLEMETS COMPRIS — c'est ce que
					/// le script attend, et les retirer ici obligerait à les remettre.
					bool Chaine(NkString &out) noexcept {
						SauterEspaces();
						if (mI >= mS.Size() || mS.Data()[mI] != '"')
							return false;
						out = NkString("\"");
						++mI;
						while (mI < mS.Size() && mS.Data()[mI] != '"') {
							const char d[2] = {mS.Data()[mI], '\0'};
							out += d;
							++mI;
						}
						if (mI < mS.Size())
							++mI;
						out += "\"";
						return true;
					}

					/// La valeur d'une broche : chaîne, nombre, identifiant ou
					/// référence `n2.value`. Rendue TELLE QU'ÉCRITE.
					bool Valeur(NkString &out) noexcept {
						SauterEspaces();
						if (Regarder() == '"')
							return Chaine(out);
						out = NkString();
						while (mI < mS.Size()) {
							const char c = mS.Data()[mI];
							if (c == ',' || c == '}' || NkGGEspace(c))
								break;
							const char d[2] = {c, '\0'};
							out += d;
							++mI;
						}
						return out.Size() > 0u;
					}

					bool Fini() noexcept {
						SauterEspaces();
						return mI >= mS.Size();
					}

				private:
					NkStringView mS;
					usize mI;
			};

			/// `n2.value` coupé en `n2` et `value`.
			///
			/// ⚠️ AU PREMIER POINT, ET NON AU DERNIER. Un nom de nœud est un
			///    identifiant simple (`n1`, `n2`) ; la broche est ce qui suit.
			///    C'est l'inverse du résolveur de chemins du script, qui coupe au
			///    DERNIER point parce qu'un chemin de modèle en contient
			///    plusieurs. Deux règles opposées pour deux grammaires — et les
			///    confondre ferait chercher un nœud nommé `n2.value`.
			inline bool NkGGCouper(const NkString &ref, NkString &noeud, NkString &broche) noexcept {
				const char *p = ref.CStr();
				uint32 i = 0;
				while (p[i] && p[i] != '.')
					++i;
				if (!p[i])
					return false;
				noeud = NkString();
				for (uint32 k = 0; k < i; ++k) {
					const char d[2] = {p[k], '\0'};
					noeud += d;
				}
				broche = NkString();
				for (uint32 k = i + 1u; p[k]; ++k) {
					const char d[2] = {p[k], '\0'};
					broche += d;
				}
				return noeud.Size() > 0u && broche.Size() > 0u;
			}

			/// La liste FERMÉE des nœuds purs que ce compilateur sait traduire.
			///
			/// ⚠️ ELLE EST FERMÉE, ET C'EST LE POINT. Traduire « ce qui ressemble
			///    à » produirait un script que personne n'a écrit. Un type absent
			///    d'ici fait refuser le graphe ENTIER, avec son nom.
			inline bool NkGGEstPur(const NkString &t) noexcept {
				static const char *kPurs[] = {
					"Literal",	"GetWidgetValue", "GetVariable", "IsEmpty",	 "Length",
					"Contains", "Matches",		  "Not",		 "And",		 "Or",
					"Compare",	"Add",			  "Subtract",	 "Multiply", "Divide"};
				for (uint32 i = 0; i < (uint32)(sizeof(kPurs) / sizeof(char *)); ++i)
					if (NkGGMotEgal(t, kPurs[i]))
						return true;
				return false;
			}

			/// Compile une SORTIE DE DONNÉE en expression de script.
			///
			/// ⚠️ ELLE EST RÉCURSIVE, ET LE CYCLE EST REFUSÉ, PAS BORNÉ. Une
			///    profondeur maximale donnerait un graphe à moitié compilé — un
			///    défaut silencieux, exactement ce que §6.2 interdit.
			inline bool NkGGExpression(NkGGGraphe &g, const NkString &noeud, const char *broche,
									   NkString &out, NkGuiRapportGraphe &rap) noexcept;

			/// La valeur d'une ENTRÉE de données : le fil s'il existe, sinon la
			/// valeur écrite, sinon le défaut demandé.
			inline bool NkGGEntree(NkGGGraphe &g, const NkGGNoeud &n, const char *broche,
								   const char *defaut, NkString &out,
								   NkGuiRapportGraphe &rap) noexcept {
				const NkGGFil *f = g.FilVers(n.nom, broche);
				if (f)
					return NkGGExpression(g, f->deNoeud, f->deBroche.CStr(), out, rap);
				const NkString *v = g.Broche(n, broche);
				if (v) {
					// 🔴 UNE VALEUR ÉCRITE `n3.result` EST UNE LIAISON, PAS DU TEXTE.
					//    C'est la graphie du §6.5 — `node n4 Branch { cond =
					//    n3.result }` — et la traiter littéralement produisait le
					//    script `if n3.result { … }`. L'évaluateur n'y trouvait aucun
					//    widget nommé `n3`, en faisait un JETON non vide, donc VRAI :
					//    la branche vraie était prise **par accident**.
					//
					//    Trouvé parce que le cycle de nœuds purs n'était pas détecté
					//    — et en le cherchant, j'ai vu que mon témoin d'équivalence
					//    passait pour la mauvaise raison : le graphe et le script
					//    donnaient le même état par coïncidence. *Un vert obtenu pour
					//    une raison fausse est plus coûteux qu'un rouge.*
					NkString ref, brocheRef;
					if (NkGGCouper(*v, ref, brocheRef) && g.Trouver(ref))
						return NkGGExpression(g, ref, brocheRef.CStr(), out, rap);
					out = *v;
					return true;
				}
				if (defaut) {
					out = NkString(defaut);
					return true;
				}
				NkString c("E-GRAPHE ");
				c += n.nom;
				c += " (";
				c += n.type;
				c += ") : l'entree `";
				c += broche;
				c += "` n'est ni reliee ni ecrite";
				rap.causes.PushBack(c);
				return false;
			}

			inline bool NkGGExpression(NkGGGraphe &g, const NkString &noeud, const char *broche,
									   NkString &out, NkGuiRapportGraphe &rap) noexcept {
				(void)broche;
				NkGGNoeud *n = g.Trouver(noeud);
				if (!n) {
					NkString c("E-GRAPHE : un fil part du noeud inconnu `");
					c += noeud;
					c += "`";
					rap.causes.PushBack(c);
					return false;
				}
				if (!NkGGEstPur(n->type)) {
					NkString c("E-GRAPHE ");
					c += n->nom;
					c += " (";
					c += n->type;
					c += ") : ce type n'a pas d'equivalent d'EXPRESSION dans le script";
					rap.causes.PushBack(c);
					return false;
				}
				if (n->enCours) {
					NkString c("E-GRAPHE ");
					c += n->nom;
					c += " : cycle de noeuds purs";
					rap.causes.PushBack(c);
					return false;
				}
				n->enCours = true;
				bool ok = true;
				NkString a, b;

				if (NkGGMotEgal(n->type, "Literal")) {
					ok = NkGGEntree(g, *n, "value", nullptr, out, rap);
				} else if (NkGGMotEgal(n->type, "GetVariable")) {
					ok = NkGGEntree(g, *n, "name", nullptr, out, rap);
					// Le nom d'une variable s'ecrit nu dans le script : on retire
					// les guillemets s'il en porte.
					if (ok && out.Size() >= 2u && out.CStr()[0] == '"') {
						NkString nu;
						for (uint32 k = 1u; k + 1u < (uint32)out.Size(); ++k) {
							const char d[2] = {out.CStr()[k], '\0'};
							nu += d;
						}
						out = nu;
					}
				} else if (NkGGMotEgal(n->type, "GetWidgetValue")) {
					// `GetWidgetValue { target = "nom", field = text }` devient
					// `"nom".text` — la graphie EXACTE du script, guillemets
					// compris, parce qu'un identifiant peut contenir un point.
					ok = NkGGEntree(g, *n, "target", nullptr, a, rap)
						 && NkGGEntree(g, *n, "field", "value", b, rap);
					if (ok) {
						out = a;
						out += ".";
						// `field` peut être écrit `text` ou `"text"`.
						if (b.Size() >= 2u && b.CStr()[0] == '"') {
							for (uint32 k = 1u; k + 1u < (uint32)b.Size(); ++k) {
								const char d[2] = {b.CStr()[k], '\0'};
								out += d;
							}
						} else {
							out += b;
						}
					}
				} else if (NkGGMotEgal(n->type, "IsEmpty") || NkGGMotEgal(n->type, "Length")) {
					ok = NkGGEntree(g, *n, "value", nullptr, a, rap);
					if (ok) {
						out = NkString(NkGGMotEgal(n->type, "IsEmpty") ? "empty(" : "length(");
						out += a;
						out += ")";
					}
				} else if (NkGGMotEgal(n->type, "Contains") || NkGGMotEgal(n->type, "Matches")) {
					ok = NkGGEntree(g, *n, "a", nullptr, a, rap)
						 && NkGGEntree(g, *n, "b", nullptr, b, rap);
					if (ok) {
						out = NkString(NkGGMotEgal(n->type, "Contains") ? "contains(" : "matches(");
						out += a;
						out += ", ";
						out += b;
						out += ")";
					}
				} else if (NkGGMotEgal(n->type, "Not")) {
					ok = NkGGEntree(g, *n, "a", nullptr, a, rap);
					if (ok) {
						out = NkString("not (");
						out += a;
						out += ")";
					}
				} else if (NkGGMotEgal(n->type, "Compare")) {
					NkString op;
					ok = NkGGEntree(g, *n, "a", nullptr, a, rap)
						 && NkGGEntree(g, *n, "op", "\"==\"", op, rap)
						 && NkGGEntree(g, *n, "b", nullptr, b, rap);
					if (ok) {
						NkString nu;
						if (op.Size() >= 2u && op.CStr()[0] == '"') {
							for (uint32 k = 1u; k + 1u < (uint32)op.Size(); ++k) {
								const char d[2] = {op.CStr()[k], '\0'};
								nu += d;
							}
						} else {
							nu = op;
						}
						out = NkString("(");
						out += a;
						out += " ";
						out += nu;
						out += " ";
						out += b;
						out += ")";
					}
				} else {
					// And / Or / Add / Subtract / Multiply / Divide : deux entrées,
					// un opérateur, et rien d'autre.
					const char *sym = NkGGMotEgal(n->type, "And")		? "&&"
									  : NkGGMotEgal(n->type, "Or")		? "||"
									  : NkGGMotEgal(n->type, "Add")		? "+"
									  : NkGGMotEgal(n->type, "Subtract") ? "-"
									  : NkGGMotEgal(n->type, "Multiply") ? "*"
																		 : "/";
					ok = NkGGEntree(g, *n, "a", nullptr, a, rap)
						 && NkGGEntree(g, *n, "b", nullptr, b, rap);
					if (ok) {
						out = NkString("(");
						out += a;
						out += " ";
						out += sym;
						out += " ";
						out += b;
						out += ")";
					}
				}
				n->enCours = false;
				return ok;
			}

			/// Compile une CHAÎNE D'EXÉCUTION en instructions de script.
			inline bool NkGGChaine(NkGGGraphe &g, const NkString &depart, NkString &out,
								   NkGuiRapportGraphe &rap, uint32 profondeur) noexcept {
				// ⚠️ UNE PROFONDEUR MAXIMALE ICI N'EST PAS UNE APPROXIMATION : un
				//    fil d'exécution qui REMONTE est une boucle hors des nœuds de
				//    boucle, et §6.6 la nomme comme non traduisible en script. La
				//    borne l'attrape ; le refus la nomme.
				if (profondeur > 256u) {
					rap.causes.PushBack(
						NkString("E-GRAPHE : un fil d'execution remonte (boucle hors "
								 "des noeuds de boucle) — intraduisible en script"));
					return false;
				}
				NkString courant = depart;
				while (courant.Size() > 0u) {
					NkGGNoeud *n = g.Trouver(courant);
					if (!n) {
						NkString c("E-GRAPHE : un fil d'execution arrive au noeud inconnu `");
						c += courant;
						c += "`";
						rap.causes.PushBack(c);
						return false;
					}
					NkString a, b, v;
					const NkString type = n->type;

					if (NkGGMotEgal(type, "Comment") || NkGGMotEgal(type, "Reroute")) {
						// Sans effet à l'exécution (§6.3, « Organisation »).
					} else if (NkGGMotEgal(type, "Branch")) {
						if (!NkGGEntree(g, *n, "cond", nullptr, a, rap))
							return false;
						out += "if ";
						out += a;
						out += " {\n";
						const NkGGFil *vrai = g.FilDe(n->nom, "true");
						if (vrai && !NkGGChaine(g, vrai->versNoeud, out, rap, profondeur + 1u))
							return false;
						out += "} else {\n";
						const NkGGFil *faux = g.FilDe(n->nom, "false");
						if (faux && !NkGGChaine(g, faux->versNoeud, out, rap, profondeur + 1u))
							return false;
						out += "}\n";
						// ⚠️ UN `Branch` NE CONTINUE PAS APRÈS LUI. Ses deux sorties
						//    SONT sa suite ; chercher un `exec` de plus produirait
						//    des instructions exécutées dans les deux branches.
						return true;
					} else if (NkGGMotEgal(type, "Sequence")) {
						// `then1…thenN`, dans l'ordre écrit.
						for (uint32 k = 1; k <= 9u; ++k) {
							char nom[8] = {'t', 'h', 'e', 'n', (char)('0' + (char)k), '\0'};
							const NkGGFil *f = g.FilDe(n->nom, nom);
							if (f && !NkGGChaine(g, f->versNoeud, out, rap, profondeur + 1u))
								return false;
						}
						return true;
					} else if (NkGGMotEgal(type, "SetVariable")) {
						if (!NkGGEntree(g, *n, "name", nullptr, a, rap)
							|| !NkGGEntree(g, *n, "value", nullptr, b, rap))
							return false;
						NkString nu;
						if (a.Size() >= 2u && a.CStr()[0] == '"') {
							for (uint32 k = 1u; k + 1u < (uint32)a.Size(); ++k) {
								const char d[2] = {a.CStr()[k], '\0'};
								nu += d;
							}
						} else {
							nu = a;
						}
						out += "set ";
						out += nu;
						out += " = ";
						out += b;
						out += "\n";
					} else if (NkGGMotEgal(type, "SetWidgetProperty")) {
						if (!NkGGEntree(g, *n, "target", nullptr, a, rap)
							|| !NkGGEntree(g, *n, "prop", nullptr, b, rap)
							|| !NkGGEntree(g, *n, "value", nullptr, v, rap))
							return false;
						NkString prop;
						if (b.Size() >= 2u && b.CStr()[0] == '"') {
							for (uint32 k = 1u; k + 1u < (uint32)b.Size(); ++k) {
								const char d[2] = {b.CStr()[k], '\0'};
								prop += d;
							}
						} else {
							prop = b;
						}
						out += "set ";
						out += a;
						out += ".";
						out += prop;
						out += " = ";
						out += v;
						out += "\n";
					} else if (NkGGMotEgal(type, "Show") || NkGGMotEgal(type, "Hide")
							   || NkGGMotEgal(type, "Toggle") || NkGGMotEgal(type, "Enable")
							   || NkGGMotEgal(type, "Focus")) {
						if (!NkGGEntree(g, *n, "target", nullptr, a, rap))
							return false;
						out += NkGGMotEgal(type, "Show")	 ? "show "
							   : NkGGMotEgal(type, "Hide")	 ? "hide "
							   : NkGGMotEgal(type, "Toggle") ? "toggle "
							   : NkGGMotEgal(type, "Enable") ? "enable "
															 : "focus ";
						out += a;
						out += "\n";
					} else if (NkGGMotEgal(type, "Disable")) {
						if (!NkGGEntree(g, *n, "target", nullptr, a, rap))
							return false;
						out += "disable ";
						out += a;
						if (NkGGEntree(g, *n, "because", "\"\"", b, rap) && b.Size() > 2u) {
							out += " because ";
							out += b;
						}
						out += "\n";
					} else if (NkGGMotEgal(type, "Toast") || NkGGMotEgal(type, "SetTheme")) {
						if (!NkGGEntree(g, *n, NkGGMotEgal(type, "Toast") ? "text" : "name",
										nullptr, a, rap))
							return false;
						out += NkGGMotEgal(type, "Toast") ? "toast " : "theme ";
						out += a;
						out += "\n";
					} else if (NkGGMotEgal(type, "OpenScreen")) {
						if (!NkGGEntree(g, *n, "screen", nullptr, a, rap))
							return false;
						out += "open ";
						out += a;
						if (NkGGEntree(g, *n, "modal", "false", b, rap)
							&& NkGGMotEgal(b, "true"))
							out += " as modal";
						out += "\n";
					} else if (NkGGMotEgal(type, "CloseScreen")) {
						out += "close";
						if (NkGGEntree(g, *n, "screen", "", a, rap) && a.Size() > 0u) {
							out += " ";
							out += a;
						}
						out += "\n";
					} else if (NkGGMotEgal(type, "Back")) {
						out += "back\n";
					} else if (NkGGMotEgal(type, "CallCallback") || NkGGMotEgal(type, "Emit")) {
						if (!NkGGEntree(g, *n, "name", nullptr, a, rap))
							return false;
						out += NkGGMotEgal(type, "Emit") ? "emit " : "Callback ";
						out += a;
						out += "(";
						// ⚠️ LES ARGUMENTS SONT LES BROCHES ÉCRITES AUTRES QUE `name`,
						//    DANS L'ORDRE DU FICHIER. Le graphe ne porte pas la
						//    signature ; inventer un ordre ferait passer le mauvais
						//    argument à la bonne fonction.
						bool premier = true;
						for (uint32 k = 0; k < (uint32)n->broches.Size(); ++k) {
							if (NkGGMotEgal(n->broches[k].nom, "name"))
								continue;
							if (!premier)
								out += ", ";
							out += n->broches[k].valeur;
							premier = false;
						}
						out += ")\n";
					} else if (NkGGMotEgal(type, "Log")) {
						if (!NkGGEntree(g, *n, "message", nullptr, a, rap))
							return false;
						out += "toast ";
						out += a;
						out += "\n";
					} else {
						NkString c("E-GRAPHE ");
						c += n->nom;
						c += " (";
						c += type;
						c += ") : ce type n'est pas compile vers le script";
						rap.causes.PushBack(c);
						return false;
					}

					const NkGGFil *suite = g.FilDe(n->nom, "exec");
					if (!suite)
						break;
					courant = suite->versNoeud;
					++profondeur;
					if (profondeur > 256u) {
						rap.causes.PushBack(
							NkString("E-GRAPHE : un fil d'execution remonte (boucle hors "
									 "des noeuds de boucle) — intraduisible en script"));
						return false;
					}
				}
				return true;
			}

		} // namespace detail

		inline bool NkGuiCompilerGraphe(NkStringView slice, NkString &nomOut, NkString &evenementOut,
										NkString &scriptOut, NkGuiRapportGraphe &rap) noexcept {
			using namespace detail;
			NkGGLecteur l(slice);
			if (!l.Mot("behavior"))
				return false;
			if (!l.Chaine(nomOut))
				return false;
			// Les guillemets du nom s'en vont : l'appelant compare a des noms nus.
			if (nomOut.Size() >= 2u) {
				NkString nu;
				for (uint32 k = 1u; k + 1u < (uint32)nomOut.Size(); ++k) {
					const char d[2] = {nomOut.CStr()[k], '\0'};
					nu += d;
				}
				nomOut = nu;
			}
			evenementOut = NkString();
			if (l.Car('(')) {
				(void)l.Identifiant(evenementOut);
				(void)l.Car(')');
			}
			if (!l.Mot("graph"))
				return false; // ce n'est pas un graphe : l'appelant fera comme avant
			++rap.graphes;
			if (!l.Car('{')) {
				rap.causes.PushBack(NkString("E-GRAPHE : `graph` sans corps"));
				++rap.refuses;
				return false;
			}

			NkGGGraphe g;
			for (;;) {
				if (l.Car('}') || l.Fini())
					break;
				if (l.Mot("node")) {
					NkGGNoeud n;
					if (!l.Identifiant(n.nom) || !l.Identifiant(n.type)) {
						rap.causes.PushBack(NkString("E-GRAPHE : `node` sans nom ou sans type"));
						++rap.refuses;
						return false;
					}
					if (l.Car('{')) {
						for (;;) {
							if (l.Car('}'))
								break;
							NkGGBroche b;
							if (!l.Identifiant(b.nom))
								break;
							if (!l.Car('='))
								break;
							if (!l.Valeur(b.valeur))
								break;
							n.broches.PushBack(b);
							(void)l.Car(',');
						}
					}
					++rap.noeuds;
					g.noeuds.PushBack(n);
					continue;
				}
				if (l.Mot("wire")) {
					// `wire a.x -> b.y -> c.z` : une CHAÎNE, donc N-1 fils.
					NkString precedent;
					for (;;) {
						NkString ref;
						if (!l.Identifiant(ref))
							break;
						if (precedent.Size() > 0u) {
							NkGGFil f;
							if (!NkGGCouper(precedent, f.deNoeud, f.deBroche)
								|| !NkGGCouper(ref, f.versNoeud, f.versBroche)) {
								rap.causes.PushBack(
									NkString("E-GRAPHE : `wire` attend `noeud.broche`"));
								++rap.refuses;
								return false;
							}
							g.fils.PushBack(f);
							++rap.fils;
						}
						precedent = ref;
						// Le `->` suivant, s'il y en a un.
						if (!(l.Car('-') && l.Car('>')))
							break;
					}
					continue;
				}
				// Ni `node` ni `wire` : on ne devine pas.
				rap.causes.PushBack(
					NkString("E-GRAPHE : une ligne du graphe n'est ni `node` ni `wire`"));
				++rap.refuses;
				return false;
			}

			// ── LE POINT D'ENTRÉE : un nœud d'événement ─────────────────────
			//  ⚠️ IL EN FAUT UN ET UN SEUL. Zéro : le graphe ne démarre jamais et
			//     le dire vaut mieux que de le compiler en un script vide. Deux :
			//     l'ordre d'exécution ne serait pas écrit, il serait choisi par
			//     l'outil — et il changerait sans que le fichier change.
			const NkGGNoeud *entree = nullptr;
			uint32 entrees = 0u;
			for (uint32 i = 0; i < (uint32)g.noeuds.Size(); ++i) {
				const NkString &t = g.noeuds[i].type;
				if (NkGGMotEgal(t, "EventClick") || NkGGMotEgal(t, "EventChanged")
					|| NkGGMotEgal(t, "EventHover") || NkGGMotEgal(t, "EventTick")
					|| NkGGMotEgal(t, "EventEmitted")) {
					entree = &g.noeuds[i];
					++entrees;
				}
			}
			if (entrees != 1u) {
				rap.causes.PushBack(NkString(entrees == 0u
												 ? "E-GRAPHE : aucun noeud d'evenement — le "
												   "graphe ne demarre jamais"
												 : "E-GRAPHE : plusieurs noeuds d'evenement — "
												   "l'ordre d'execution ne serait pas ECRIT"));
				++rap.refuses;
				return false;
			}

			const NkGGFil *premier = g.FilDe(entree->nom, "exec");
			scriptOut = NkString();
			if (premier && !NkGGChaine(g, premier->versNoeud, scriptOut, rap, 0u)) {
				++rap.refuses;
				return false;
			}
			++rap.compiles;
			return true;
		}

	} // namespace nkgui
} // namespace nkentseu
