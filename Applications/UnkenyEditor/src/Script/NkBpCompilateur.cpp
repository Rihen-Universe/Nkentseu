// -----------------------------------------------------------------------------
// FICHIER: UnkenyEditor/Script/NkBpCompilateur.cpp
// DESCRIPTION: Le COMPILATEUR d'un document Blueprint (NkBpDocument.h) vers un
//              module de bytecode verifie : variables declarees, fonctions
//              appelees (NK_APPEL), macros inserees, repartiteurs, boucles ;
//              le menu de la toile ; la porte reecrite avec de vraies
//              declarations.
//
// LES REGLES, celles d'UE5 la ou elles ont un sens ici :
//   - On MARCHE EN AVANT depuis chaque evenement (et l'entree de chaque
//     fonction) le long des fils d'execution ; un noeud PUR est evalue a la
//     demande, a CHAQUE usage (une Position lue apres un Teleporter est la
//     nouvelle ; dans une boucle, a chaque tour).
//   - La sortie d'un noeud d'EXECUTION (un appel de fonction, un natif) se lit
//     apres qu'il a ete execute : son registre est garde pour la suite de la
//     fonction. Lue avant : erreur sur le noeud qui la lit.
//   - Une MACRO est inseree a chaque usage (ses noeuds comptent dans la trace
//     de SON graphe) ; une macro qui s'insere elle-meme est refusee.
//   - Toute erreur designe son NOEUD et son GRAPHE.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Script/NkBpCatalogue.h"
#include "Script/NkBpExpression.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		using unkeny::NkOpBp;
		using unkeny::NkTypeBp;

		namespace {
			constexpr uint32 NK_AUCUN = 0xFFFFFFFFu;

			/// Le contexte d'un graphe en cours de compilation : le graphe, son
			/// evenement, et -- pour une macro -- l'instance qui l'a inseree.
			struct Ctx {
					uint32 graphe = 0u;
					const graph::NkNodeGraph *g = nullptr;
					graph::NkNodeId evenement = graph::NK_NODE_INVALID;
					/// Le graphe de FONCTION dont les parametres sont dans les
					/// registres 0..P-1 (NK_AUCUN : un evenement).
					uint32 fonction = NK_AUCUN;
					const Ctx *parent = nullptr;
					graph::NkNodeId instance = graph::NK_NODE_INVALID;
					uint32 expansion = 0u; ///< unique par insertion de macro
			};

			struct NkCompilo {
					const NkDocumentBp &d;
					NkErreurBp &err;
					unkeny::NkAssembleurBp a;
					bool ok = true;
					NkVector<int32> fonctions; ///< graphe du document -> fonction du module
					uint32 expansions = 0u;
					uint32 profondeurMacro = 0u;
					/// Les sorties des noeuds d'EXECUTION deja executes dans la
					/// fonction courante : (code, expansion) -> registres.
					struct Cache {
							uint32 code = 0u;
							uint32 expansion = 0u;
							NkVector<NkString> noms;
							NkVector<uint32> regs;
					};
					NkVector<Cache> caches;

					NkCompilo(const NkDocumentBp &doc, NkErreurBp &e) : d(doc), err(e) {
					}

					uint32 Code(const Ctx &c, graph::NkNodeId id) const {
						return NkBpCodeNoeud(c.graphe, id);
					}
					void Ligne(const Ctx &c, graph::NkNodeId id) {
						a.Ligne(Code(c, id));
					}
					uint32 Echec(const Ctx &c, graph::NkNodeId n, const NkString &message) {
						if (ok) {
							err.message = message;
							err.noeud = n;
							err.graphe = c.graphe;
						}
						ok = false;
						return 0u;
					}
					NkTypeBp TypePrise(const Ctx &c, const graph::NkSocket &s) const {
						const NkString *t = c.g->TypeName(s.type);
						return NkBpTypeDeNom(t != nullptr ? t->CStr() : nullptr);
					}
					const graph::NkLink *Sortant(const Ctx &c, graph::NkNodeId n, int32 k) const {
						for (uint32 i = 0; i < c.g->LinkCount(); ++i) {
							const graph::NkLink *l = c.g->LinkAt(i);
							if (l != nullptr && l->alive && l->fromNode == n && l->fromSocket == k) {
								return l;
							}
						}
						return nullptr;
					}
					/// L'indice, PARMI les prises de donnees du sens `dir`, de la prise k.
					int32 RangDonnee(const graph::NkNode &n, int32 k) const {
						int32 r = 0;
						for (int32 i = 0; i < k; ++i) {
							const graph::NkSocket &s = n.sockets[static_cast<uint32>(i)];
							if (s.dir == n.sockets[static_cast<uint32>(k)].dir && s.family == graph::NkSocketFamily::Data) {
								++r;
							}
						}
						return r;
					}
					const graph::NkNode *TrouverType(const graph::NkNodeGraph &g, const char *type) const {
						const graph::NkNode *t = nullptr;
						for (uint32 i = 0; i < g.RawNodeCount(); ++i) {
							const graph::NkNode *n = g.RawNodeAt(i);
							if (n != nullptr && n->alive && n->type == type) {
								if (t != nullptr) {
									return nullptr; // deux : ambigu (l'appelant le dit)
								}
								t = n;
							}
						}
						return t;
					}
					int32 Variable(const Ctx &c, const graph::NkNode &n) {
						const NkString nom = NkBpPropTexte(*c.g, n, "var");
						const int32 v = d.TrouverVariable(nom.CStr());
						if (v < 0) {
							Echec(c, n.id, NkString::Format("variable inconnue : « %s » (supprimée ?)", nom.CStr()));
							return -1;
						}
						// Les variables declarees sont les PREMIERES du module, dans l'ordre.
						return v;
					}
					Cache *TrouverCache(const Ctx &c, graph::NkNodeId id) {
						const uint32 code = Code(c, id);
						for (uint32 i = 0; i < caches.Size(); ++i) {
							if (caches[i].code == code && caches[i].expansion == c.expansion) {
								return &caches[i];
							}
						}
						return nullptr;
					}
					void Garder(const Ctx &c, graph::NkNodeId id, const char *nom, uint32 r) {
						Cache *k = TrouverCache(c, id);
						if (k == nullptr) {
							Cache n;
							n.code = Code(c, id);
							n.expansion = c.expansion;
							caches.PushBack(n);
							k = &caches.Back();
						}
						for (uint32 i = 0; i < k->noms.Size(); ++i) {
							if (k->noms[i] == nom) {
								k->regs[i] = r;
								return;
							}
						}
						k->noms.PushBack(NkString(nom));
						k->regs.PushBack(r);
					}
					/// Le registre d'une sortie d'un noeud d'EXECUTION deja execute.
					uint32 Garde(const Ctx &c, const graph::NkNode &src, const graph::NkSocket &s, const graph::NkNode &lecteur) {
						if (Cache *k = TrouverCache(c, src.id)) {
							for (uint32 i = 0; i < k->noms.Size(); ++i) {
								if (k->noms[i] == s.name) {
									return k->regs[i];
								}
							}
						}
						return Echec(c, lecteur.id,
									 NkString::Format("« %s » de « %s » se lit APRÈS l'exécution de ce nœud (relier son fil d'exécution avant)",
													  s.name.CStr(), src.label.CStr()));
					}

					/// Une constante de la valeur saisie sur une entree libre.
					uint32 Constante(const Ctx &c, const graph::NkNode &n, const graph::NkSocket &s, NkTypeBp attendu) {
						const graph::NkGraphValue &v = s.defaultValue;
						auto num = [&](uint32 i) {
							return v.IsSet() && i < v.numbers.Size() ? v.numbers[i] : 0.f;
						};
						const uint32 r = a.Registre(attendu);
						switch (attendu) {
							case NkTypeBp::NK_ENTITE:
								a.Emettre(NkOpBp::NK_SOI, r); // une entite libre vaut SOI
								break;
							case NkTypeBp::NK_TEXTE:
								a.Emettre(NkOpBp::NK_CONST, r, a.ConstTexte(v.IsSet() ? v.text.CStr() : ""));
								break;
							case NkTypeBp::NK_REEL:
								a.Emettre(NkOpBp::NK_CONST, r, a.ConstReel(num(0)));
								break;
							case NkTypeBp::NK_ENTIER:
								a.Emettre(NkOpBp::NK_CONST, r, a.ConstEntier(static_cast<int32>(num(0))));
								break;
							case NkTypeBp::NK_BOOLEEN:
								a.Emettre(NkOpBp::NK_CONST, r, a.ConstBooleen(num(0) != 0.f));
								break;
							case NkTypeBp::NK_VEC2:
								a.Emettre(NkOpBp::NK_CONST, r, a.ConstVec2(num(0), num(1)));
								break;
							case NkTypeBp::NK_COULEUR: {
								uint32 rvba = 0xFFFFFFFFu;
								if (v.IsSet() && !v.text.Empty()) {
									NkBpLireCouleur(v.text.CStr(), rvba);
								}
								a.Emettre(NkOpBp::NK_CONST, r, a.ConstCouleur(rvba));
								break;
							}
							default:
								return Echec(c, n.id, NkString::Format("« %s » : type sans valeur", s.name.CStr()));
						}
						return r;
					}

					/// La valeur de l'entree `entree` de `n`, convertie en `attendu`.
					uint32 Valeur(const Ctx &c, const graph::NkNode &n, const char *entree, NkTypeBp attendu, int32 prof) {
						const int32 k = n.FindSocket(entree, graph::NkSocketDir::Input);
						if (k < 0) {
							return Echec(c, n.id, NkString::Format("prise « %s » absente (graphe d'une autre version ?)", entree));
						}
						const graph::NkLink *l = c.g->IncomingOf(n.id, k);
						if (l == nullptr) {
							return Constante(c, n, n.sockets[static_cast<uint32>(k)], attendu);
						}
						const graph::NkNode *src = c.g->Find(l->fromNode);
						if (src == nullptr || l->fromSocket < 0 || static_cast<uint32>(l->fromSocket) >= src->sockets.Size()) {
							return Echec(c, n.id, NkString("fil vers un nœud absent"));
						}
						const NkTypeBp t = TypePrise(c, src->sockets[static_cast<uint32>(l->fromSocket)]);
						const uint32 r = Sortie(c, *src, l->fromSocket, n, prof + 1);
						if (!ok || t == attendu) {
							return r;
						}
						if (t == NkTypeBp::NK_ENTIER && attendu == NkTypeBp::NK_REEL) {
							const uint32 cv = a.Registre(NkTypeBp::NK_REEL);
							a.Emettre(NkOpBp::NK_I2R, cv, r);
							return cv;
						}
						return Echec(c, n.id, NkString::Format("« %s » attend un %s", entree, unkeny::NkNomTypeBp(attendu)));
					}

					/// La sortie `k` de `src`, lue par `lecteur`.
					uint32 Sortie(const Ctx &c, const graph::NkNode &src, int32 k, const graph::NkNode &lecteur, int32 prof) {
						if (prof > 256) {
							return Echec(c, src.id, NkString("graphe trop profond"));
						}
						const graph::NkSocket &s = src.sockets[static_cast<uint32>(k)];
						const NkTypeBp t = TypePrise(c, s);
						Ligne(c, src.id);
						switch (NkBpGenre(src)) {
							case NkGenreNoeudBp::NK_EVENEMENT: {
								if (src.id != c.evenement) {
									return Echec(c, src.id, NkString("valeur d'un AUTRE événement : chaque événement a les siennes"));
								}
								unkeny::NkArgBp arg = unkeny::NkArgBp::NK_DT;
								if (s.name == "autre") {
									arg = unkeny::NkArgBp::NK_AUTRE;
								} else if (s.name == "soiEstLaZone") {
									arg = unkeny::NkArgBp::NK_SOI_EST_ZONE;
								} else if (s.name == "valeur") {
									arg = unkeny::NkArgBp::NK_VALEUR;
								}
								const uint32 r = a.Registre(t);
								a.Emettre(NkOpBp::NK_ARG, r, static_cast<uint32>(arg));
								return r;
							}
							case NkGenreNoeudBp::NK_REP_EVENEMENT:
								if (src.id != c.evenement) {
									return Echec(c, src.id, NkString("valeur d'un AUTRE événement : chaque événement a les siennes"));
								}
								return static_cast<uint32>(RangDonnee(src, k)); // ses parametres : les premiers registres
							case NkGenreNoeudBp::NK_FN_ENTREE:
								if (c.fonction != c.graphe || c.parent != nullptr) {
									return Echec(c, src.id, NkString("l'entrée d'une fonction ne se lit que dans sa fonction"));
								}
								return static_cast<uint32>(RangDonnee(src, k));
							case NkGenreNoeudBp::NK_MACRO_ENTREE: {
								// Une entree de la macro : la valeur branchee sur l'INSTANCE.
								if (c.parent == nullptr) {
									return Echec(c, src.id, NkString("entrée de macro hors d'une macro insérée"));
								}
								const graph::NkNode *inst = c.parent->g->Find(c.instance);
								if (inst == nullptr) {
									return Echec(c, src.id, NkString("instance de macro absente"));
								}
								return Valeur(*c.parent, *inst, s.name.CStr(), t, prof + 1);
							}
							case NkGenreNoeudBp::NK_MACRO: {
								// Une sortie de DONNEE d'une macro : ce que sa frontiere de
								// sortie recoit, calcule dans la macro.
								Ctx m;
								const graph::NkNode *sortie = nullptr;
								if (!Inserer(c, src, m) || (sortie = TrouverType(*m.g, NK_BP_MACRO_SORTIE)) == nullptr) {
									return ok ? Echec(c, src.id, NkString("macro sans (ou avec deux) nœud « Sorties »")) : 0u;
								}
								++profondeurMacro;
								const uint32 r = Valeur(m, *sortie, s.name.CStr(), t, prof + 1);
								--profondeurMacro;
								return r;
							}
							case NkGenreNoeudBp::NK_VAR_GET:
							case NkGenreNoeudBp::NK_VAR_SET: {
								const int32 v = Variable(c, src);
								if (v < 0) {
									return 0u;
								}
								const uint32 r = a.Registre(t);
								a.Emettre(NkOpBp::NK_LIRE_VAR, r, static_cast<uint32>(v));
								return r;
							}
							case NkGenreNoeudBp::NK_APPEL_FN: {
								const int32 g = d.TrouverGraphe(NkBpPropTexte(*c.g, src, "fonction").CStr(), NkGenreGrapheBp::NK_FONCTION);
								if (g < 0) {
									return Echec(c, src.id, NkString("fonction inconnue (supprimée ?)"));
								}
								if (!d.graphes[static_cast<uint32>(g)].pure) {
									return Garde(c, src, s, lecteur);
								}
								return Appel(c, src, static_cast<uint32>(g), s.name.CStr(), prof);
							}
							case NkGenreNoeudBp::NK_POUR:
							case NkGenreNoeudBp::NK_CODE:
								return Garde(c, src, s, lecteur);
							case NkGenreNoeudBp::NK_EXPRESSION:
								return SortieExpression(c, src, k, prof);
							case NkGenreNoeudBp::NK_RELAIS:
								return Valeur(c, src, "in", t, prof + 1);
							case NkGenreNoeudBp::NK_SOI: {
								const uint32 r = a.Registre(NkTypeBp::NK_ENTITE);
								a.Emettre(NkOpBp::NK_SOI, r);
								return r;
							}
							case NkGenreNoeudBp::NK_LIRE_VAR: {
								const NkProtoBp *p = NkBpProto(src.type.CStr());
								const uint32 v = VariableAncienne(c, src, *p);
								if (!ok) {
									return 0u;
								}
								const uint32 r = a.Registre(p->typeVar);
								a.Emettre(NkOpBp::NK_LIRE_VAR, r, v);
								return r;
							}
							case NkGenreNoeudBp::NK_MATH: {
								const NkProtoBp *p = NkBpProto(src.type.CStr());
								const uint32 ra = Valeur(c, src, p->entrees[0].nom, NkBpTypeDeNom(p->entrees[0].type), prof);
								const uint32 rb = p->nbEntrees > 1 ? Valeur(c, src, p->entrees[1].nom, NkBpTypeDeNom(p->entrees[1].type), prof) : 0u;
								if (!ok) {
									return 0u;
								}
								const uint32 r = a.Registre(NkBpTypeDeNom(p->sorties[0].type));
								Ligne(c, src.id);
								if (p->nbEntrees > 1) {
									a.Emettre(p->op, r, ra, rb);
								} else {
									a.Emettre(p->op, r, ra);
								}
								return r;
							}
							case NkGenreNoeudBp::NK_NATIF: {
								const NkProtoBp *p = NkBpProto(src.type.CStr());
								uint32 nb = 0;
								const unkeny::NkNatifBp &x = unkeny::NkNatifsBp(nb)[p->natif];
								if (!x.pur) {
									return Garde(c, src, s, lecteur);
								}
								return Natif(c, src, x, k, prof);
							}
							default:
								return Echec(c, src.id, NkString("ce nœud ne produit pas de valeur"));
						}
					}

					/// L'ANCIENNE variable (nom saisi sur le noeud) : lue telle quelle.
					uint32 VariableAncienne(const Ctx &c, const graph::NkNode &n, const NkProtoBp &p) {
						const int32 k = n.FindSocket("nom", graph::NkSocketDir::Input);
						if (k < 0 || c.g->IncomingOf(n.id, k) != nullptr) {
							return Echec(c, n.id, NkString("« nom » doit être une constante (saisie sur le nœud), pas un fil"));
						}
						const graph::NkGraphValue &v = n.sockets[static_cast<uint32>(k)].defaultValue;
						const NkString nom = v.IsSet() ? v.text : NkString();
						if (nom.Empty()) {
							return Echec(c, n.id, NkString("variable sans nom : saisir « nom » sur le nœud"));
						}
						if (nom.Length() >= unkeny::NK_UNKENY_VAR_NOM_MAX) {
							return Echec(c, n.id, NkString::Format("nom de variable trop long (%u caractères au plus)",
																   static_cast<unsigned>(unkeny::NK_UNKENY_VAR_NOM_MAX - 1u)));
						}
						for (uint32 i = 0; i < a.module.variables.Size(); ++i) {
							if (a.module.variables[i].nom == nom) {
								if (a.module.variables[i].type != p.typeVar) {
									return Echec(c, n.id, NkString::Format("la variable « %s » a déjà un autre type", nom.CStr()));
								}
								return i;
							}
						}
						if (a.module.variables.Size() >= unkeny::NK_UNKENY_SCRIPT_VARS_MAX) {
							return Echec(c, n.id, NkString("trop de variables pour un script"));
						}
						return a.Variable(nom.CStr(), p.typeVar, unkeny::NkValeurBp());
					}

					/// Un natif : parametres evalues, l'appel, les resultats ; rend le
					/// registre du resultat `sortie` (-1 : aucun). Un natif d'EXECUTION
					/// garde ses resultats pour la suite.
					uint32 Natif(const Ctx &c, const graph::NkNode &n, const unkeny::NkNatifBp &x, int32 sortie, int32 prof) {
						const int32 imp = a.Import(x.signature.nom);
						if (imp < 0) {
							return Echec(c, n.id, NkString::Format("natif inconnu : %s", x.signature.nom));
						}
						uint32 regs[6] = {};
						uint32 nr = 0;
						for (uint8 k = 0; k < x.signature.nbParams && ok; ++k) {
							regs[nr++] = Valeur(c, n, x.nomsParams[k], x.signature.params[k], prof);
						}
						uint32 resultat = 0u;
						for (uint8 k = 0; k < x.signature.nbResultats && ok; ++k) {
							const uint32 r = a.Registre(x.signature.resultats[k]);
							regs[nr++] = r;
							const int32 ks = n.FindSocket(x.nomsResultats[k], graph::NkSocketDir::Output);
							if (ks == sortie) {
								resultat = r;
							}
							if (!x.pur) {
								Garder(c, n.id, x.nomsResultats[k], r);
							}
						}
						if (!ok) {
							return 0u;
						}
						Ligne(c, n.id);
						a.Natif(static_cast<uint32>(imp), regs, nr);
						return resultat;
					}

					/// L'appel de la fonction du graphe `g` par le noeud `n` ; rend le
					/// registre de la sortie `sortie` (nul : aucune).
					uint32 Appel(const Ctx &c, const graph::NkNode &n, uint32 g, const char *sortie, int32 prof) {
						const NkGrapheBp &f = d.graphes[g];
						if (fonctions[g] < 0) {
							return Echec(c, n.id, NkString("fonction non compilée"));
						}
						NkVector<uint32> regs;
						for (uint32 i = 0; i < f.entrees.Size() && ok; ++i) {
							regs.PushBack(Valeur(c, n, f.entrees[i].nom.CStr(), NkBpTypeDeNom(f.entrees[i].type.CStr()), prof));
						}
						uint32 resultat = 0u;
						for (uint32 i = 0; i < f.sorties.Size() && ok; ++i) {
							const uint32 r = a.Registre(NkBpTypeDeNom(f.sorties[i].type.CStr()));
							regs.PushBack(r);
							if (sortie != nullptr && f.sorties[i].nom == sortie) {
								resultat = r;
							}
							if (!f.pure) {
								Garder(c, n.id, f.sorties[i].nom.CStr(), r);
							}
						}
						if (!ok) {
							return 0u;
						}
						Ligne(c, n.id);
						a.Appel(static_cast<uint32>(fonctions[g]), regs.Data(), static_cast<uint32>(regs.Size()));
						return resultat;
					}

					/// Le contexte d'une macro inseree par l'instance `inst`.
					bool Inserer(const Ctx &c, const graph::NkNode &inst, Ctx &m) {
						const int32 g = d.TrouverGraphe(NkBpPropTexte(*c.g, inst, "macro").CStr(), NkGenreGrapheBp::NK_MACRO);
						if (g < 0) {
							Echec(c, inst.id, NkString("macro inconnue (supprimée ?)"));
							return false;
						}
						if (profondeurMacro > 16u) {
							Echec(c, inst.id, NkString("macro insérée dans elle-même (ou trop profond)"));
							return false;
						}
						m.graphe = static_cast<uint32>(g);
						m.g = &d.graphes[static_cast<uint32>(g)].graphe;
						m.evenement = graph::NK_NODE_INVALID;
						m.fonction = c.fonction;
						m.parent = &c;
						m.instance = inst.id;
						m.expansion = ++expansions;
						return true;
					}

					void Chaine(const Ctx &c, const graph::NkNode &n, const char *sortieExec, int32 prof) {
						const int32 k = n.FindSocket(sortieExec, graph::NkSocketDir::Output);
						if (k < 0 || !ok) {
							return;
						}
						const graph::NkLink *l = Sortant(c, n.id, k);
						if (l == nullptr) {
							return;
						}
						const graph::NkNode *suivant = c.g->Find(l->toNode);
						if (suivant == nullptr) {
							Echec(c, n.id, NkString("fil d'exécution vers un nœud absent"));
							return;
						}
						Noeud(c, *suivant, l->toSocket, prof + 1);
					}

					void Noeud(const Ctx &c, const graph::NkNode &n, int32 entree, int32 prof) {
						if (prof > 512) {
							Echec(c, n.id, NkString("chaîne d'exécution trop longue"));
							return;
						}
						Ligne(c, n.id);
						switch (NkBpGenre(n)) {
							case NkGenreNoeudBp::NK_SI: {
								const uint32 cond = Valeur(c, n, "condition", NkTypeBp::NK_BOOLEEN, prof);
								if (!ok) {
									return;
								}
								Ligne(c, n.id);
								a.Emettre(NkOpBp::NK_SAUT_SI_FAUX, cond, 0u);
								const uint32 versFaux = a.Pc() - 1u;
								Chaine(c, n, "vrai", prof);
								a.Emettre(NkOpBp::NK_SAUT, 0u);
								const uint32 versFin = a.Pc() - 1u;
								a.Patcher(versFaux, a.Pc());
								Chaine(c, n, "faux", prof);
								a.Patcher(versFin, a.Pc());
								return;
							}
							case NkGenreNoeudBp::NK_SEQUENCE: {
								const NkProtoBp *p = NkBpProto(n.type.CStr());
								for (uint8 k = 0; k < p->nbSorties && ok; ++k) {
									Chaine(c, n, p->sorties[k].nom, prof);
								}
								return;
							}
							case NkGenreNoeudBp::NK_POUR: {
								const uint32 r0 = Valeur(c, n, "premier", NkTypeBp::NK_ENTIER, prof);
								const uint32 r1 = Valeur(c, n, "dernier", NkTypeBp::NK_ENTIER, prof);
								if (!ok) {
									return;
								}
								const uint32 i = a.Registre(NkTypeBp::NK_ENTIER);
								const uint32 fin = a.Registre(NkTypeBp::NK_ENTIER);
								const uint32 un = a.Registre(NkTypeBp::NK_ENTIER);
								const uint32 cond = a.Registre(NkTypeBp::NK_BOOLEEN);
								Ligne(c, n.id);
								a.Emettre(NkOpBp::NK_COPIER, i, r0);
								a.Emettre(NkOpBp::NK_COPIER, fin, r1); // « dernier » lu une fois
								a.Emettre(NkOpBp::NK_CONST, un, a.ConstEntier(1));
								Garder(c, n.id, "indice", i);
								const uint32 haut = a.Pc();
								Ligne(c, n.id);
								a.Emettre(NkOpBp::NK_LE_I, cond, i, fin);
								a.Emettre(NkOpBp::NK_SAUT_SI_FAUX, cond, 0u);
								const uint32 versFin = a.Pc() - 1u;
								Chaine(c, n, "corps", prof);
								Ligne(c, n.id);
								a.Emettre(NkOpBp::NK_ADD_I, i, i, un);
								a.Emettre(NkOpBp::NK_SAUT, haut);
								a.Patcher(versFin, a.Pc());
								Chaine(c, n, "fini", prof);
								return;
							}
							case NkGenreNoeudBp::NK_TANT_QUE: {
								const uint32 haut = a.Pc();
								const uint32 cond = Valeur(c, n, "condition", NkTypeBp::NK_BOOLEEN, prof);
								if (!ok) {
									return;
								}
								Ligne(c, n.id);
								a.Emettre(NkOpBp::NK_SAUT_SI_FAUX, cond, 0u);
								const uint32 versFin = a.Pc() - 1u;
								Chaine(c, n, "corps", prof);
								Ligne(c, n.id);
								a.Emettre(NkOpBp::NK_SAUT, haut);
								a.Patcher(versFin, a.Pc());
								Chaine(c, n, "fini", prof);
								return;
							}
							case NkGenreNoeudBp::NK_ECRIRE_VAR: {
								const NkProtoBp *p = NkBpProto(n.type.CStr());
								const uint32 v = VariableAncienne(c, n, *p);
								const uint32 r = ok ? Valeur(c, n, "valeur", p->typeVar, prof) : 0u;
								if (!ok) {
									return;
								}
								Ligne(c, n.id);
								a.Emettre(NkOpBp::NK_ECRIRE_VAR, v, r);
								Chaine(c, n, "suite", prof);
								return;
							}
							case NkGenreNoeudBp::NK_VAR_SET: {
								const int32 v = Variable(c, n);
								if (v < 0) {
									return;
								}
								const uint32 r = Valeur(c, n, "valeur", NkBpTypeDeNom(d.variables[static_cast<uint32>(v)].type.CStr()), prof);
								if (!ok) {
									return;
								}
								Ligne(c, n.id);
								a.Emettre(NkOpBp::NK_ECRIRE_VAR, static_cast<uint32>(v), r);
								Chaine(c, n, "suite", prof);
								return;
							}
							case NkGenreNoeudBp::NK_NATIF: {
								const NkProtoBp *p = NkBpProto(n.type.CStr());
								uint32 nb = 0;
								const unkeny::NkNatifBp &x = unkeny::NkNatifsBp(nb)[p->natif];
								if (x.pur) {
									Echec(c, n.id, NkString("ce nœud ne s'exécute pas : il calcule une valeur"));
									return;
								}
								Natif(c, n, x, -1, prof);
								Chaine(c, n, "suite", prof);
								return;
							}
							case NkGenreNoeudBp::NK_APPEL_FN: {
								const int32 g = d.TrouverGraphe(NkBpPropTexte(*c.g, n, "fonction").CStr(), NkGenreGrapheBp::NK_FONCTION);
								if (g < 0) {
									Echec(c, n.id, NkString("fonction inconnue (supprimée ?)"));
									return;
								}
								Appel(c, n, static_cast<uint32>(g), nullptr, prof);
								Chaine(c, n, "suite", prof);
								return;
							}
							case NkGenreNoeudBp::NK_FN_RETOUR: {
								if (c.fonction != c.graphe || c.parent != nullptr) {
									Echec(c, n.id, NkString("« Retour » n'a de sens que dans le graphe de sa fonction"));
									return;
								}
								const NkGrapheBp &f = d.graphes[c.graphe];
								const uint32 np = static_cast<uint32>(f.entrees.Size());
								for (uint32 i = 0; i < f.sorties.Size() && ok; ++i) {
									const uint32 r = Valeur(c, n, f.sorties[i].nom.CStr(), NkBpTypeDeNom(f.sorties[i].type.CStr()), prof);
									if (ok && r != np + i) {
										Ligne(c, n.id);
										a.Emettre(NkOpBp::NK_COPIER, np + i, r);
									}
								}
								Ligne(c, n.id);
								a.Emettre(NkOpBp::NK_FIN);
								return;
							}
							case NkGenreNoeudBp::NK_MACRO: {
								Ctx m;
								if (!Inserer(c, n, m)) {
									return;
								}
								const graph::NkNode *e = TrouverType(*m.g, NK_BP_MACRO_ENTREE);
								if (e == nullptr) {
									Echec(c, n.id, NkString("macro sans (ou avec deux) nœud « Entrées »"));
									return;
								}
								// L'entree d'execution par laquelle on arrive : sa jumelle sur
								// la frontiere d'entree de la macro.
								const NkString par = n.sockets[static_cast<uint32>(entree)].name;
								++profondeurMacro;
								Ligne(m, e->id);
								Chaine(m, *e, par.CStr(), prof);
								--profondeurMacro;
								return;
							}
							case NkGenreNoeudBp::NK_MACRO_SORTIE: {
								// La sortie d'execution de la macro : on ressort par la
								// jumelle de l'INSTANCE, dans le graphe qui l'a inseree.
								if (c.parent == nullptr) {
									Echec(c, n.id, NkString("sortie de macro hors d'une macro insérée"));
									return;
								}
								const graph::NkNode *inst = c.parent->g->Find(c.instance);
								if (inst == nullptr) {
									Echec(c, n.id, NkString("instance de macro absente"));
									return;
								}
								const NkString par = n.sockets[static_cast<uint32>(entree)].name;
								--profondeurMacro;
								Chaine(*c.parent, *inst, par.CStr(), prof);
								++profondeurMacro;
								return;
							}
							case NkGenreNoeudBp::NK_REP_APPELER: {
								const int32 r = d.TrouverRepartiteur(NkBpPropTexte(*c.g, n, "rep").CStr());
								if (r < 0) {
									Echec(c, n.id, NkString("répartiteur inconnu (supprimé ?)"));
									return;
								}
								const NkRepartiteurBp &rep = d.repartiteurs[static_cast<uint32>(r)];
								const uint32 cible = Valeur(c, n, "cible", NkTypeBp::NK_ENTITE, prof);
								NkVector<uint32> args;
								for (uint32 i = 0; i < rep.params.Size() && ok; ++i) {
									args.PushBack(Valeur(c, n, rep.params[i].nom.CStr(), NkBpTypeDeNom(rep.params[i].type.CStr()), prof));
								}
								if (!ok) {
									return;
								}
								if (args.Size() > 8u) {
									Echec(c, n.id, NkString("un répartiteur porte 8 paramètres au plus"));
									return;
								}
								Ligne(c, n.id);
								a.Emettre(NkOpBp::NK_DIFFUSER, cible, a.ConstTexte(rep.nom.CStr()), static_cast<uint32>(args.Size()));
								for (uint32 i = 0; i < args.Size(); ++i) {
									a.module.fonctions[a.Courante()].code.PushBack(args[i]);
								}
								Chaine(c, n, "suite", prof);
								return;
							}
							case NkGenreNoeudBp::NK_RELAIS:
								Chaine(c, n, "out", prof);
								return;
							case NkGenreNoeudBp::NK_SI_EXPRESSION: {
								NkArbreExpr ar;
								if (!Analyser(c, n, false, ar)) {
									return;
								}
								CtxCode cc;
								cc.n = &n;
								cc.pur = true;
								ValExpr v;
								if (!Gen(c, cc, ar, ar.instructions[0].valeur, v, prof)) {
									return;
								}
								if (v.t != NkTypeBp::NK_BOOLEEN) {
									EchecCode(c, cc, 1u, NkString::Format("la condition doit être un booléen (« a > 0 »), elle donne un %s", NomType(v.t)));
									return;
								}
								Ligne(c, n.id);
								a.Emettre(NkOpBp::NK_SAUT_SI_FAUX, v.r, 0u);
								const uint32 versFaux = a.Pc() - 1u;
								Chaine(c, n, "vrai", prof);
								a.Emettre(NkOpBp::NK_SAUT, 0u);
								const uint32 versFin = a.Pc() - 1u;
								a.Patcher(versFaux, a.Pc());
								Chaine(c, n, "faux", prof);
								a.Patcher(versFin, a.Pc());
								return;
							}
							case NkGenreNoeudBp::NK_CODE:
								ExecuterCode(c, n, prof);
								Chaine(c, n, "suite", prof);
								return;
							default:
								Echec(c, n.id, NkString("ce nœud ne s'exécute pas : relier un nœud d'exécution"));
								return;
						}
					}

					// ═════════════════════════════════════════════════════════════
					// LES NOEUDS DE CODE (NkBpExpression.h) : l'arbre -> la machine
					// ═════════════════════════════════════════════════════════════
					struct ValExpr {
							uint32 r = 0u;
							NkTypeBp t = NkTypeBp::NK_RIEN;
					};
					/// Le contexte d'un noeud de code : ses SORTIES (un noeud Code les
					/// ecrit), et s'il est PUR (une Expression n'agit pas).
					struct CtxCode {
							const graph::NkNode *n = nullptr;
							bool pur = true;
							NkVector<NkString> sorties;
							NkVector<uint32> regs;
							NkVector<NkTypeBp> types;
							uint32 ligne = 1u;
					};
					uint32 EchecCode(const Ctx &c, const CtxCode &cc, uint32 colonne, const NkString &m) {
						return Echec(c, cc.n->id, NkString::Format("ligne %u, colonne %u : %s", static_cast<unsigned>(cc.ligne),
																   static_cast<unsigned>(colonne), m.CStr()));
					}
					static const char *NomType(NkTypeBp t) {
						return unkeny::NkNomTypeBp(t);
					}
					/// `v` en `vers` (entier -> reel seulement). false : erreur posee.
					bool Convertir(const Ctx &c, const CtxCode &cc, ValExpr &v, NkTypeBp vers, uint32 colonne) {
						if (v.t == vers) {
							return true;
						}
						if (v.t == NkTypeBp::NK_ENTIER && vers == NkTypeBp::NK_REEL) {
							const uint32 r = a.Registre(NkTypeBp::NK_REEL);
							a.Emettre(NkOpBp::NK_I2R, r, v.r);
							v.r = r;
							v.t = NkTypeBp::NK_REEL;
							return true;
						}
						EchecCode(c, cc, colonne, NkString::Format("un %s est attendu, l'expression donne un %s", NomType(vers), NomType(v.t)));
						return false;
					}
					uint32 ConstanteNombre(NkTypeBp t, float64 x) {
						const uint32 r = a.Registre(t);
						a.Emettre(NkOpBp::NK_CONST, r, t == NkTypeBp::NK_ENTIER ? a.ConstEntier(static_cast<int32>(x)) : a.ConstReel(static_cast<float32>(x)));
						return r;
					}
					/// Le natif `nom` (qualifie) appele avec `args` (deja calcules).
					bool AppelNatif(const Ctx &c, const CtxCode &cc, int32 k, NkVector<ValExpr> &args, ValExpr &sortie, uint32 colonne) {
						uint32 nb = 0;
						const unkeny::NkNatifBp &x = unkeny::NkNatifsBp(nb)[k];
						if (!x.pur && cc.pur) {
							EchecCode(c, cc, colonne,
									  NkString::Format("« %s » AGIT : il n'a sa place que dans un nœud Code (pas dans une expression)", x.libelle));
							return false;
						}
						// Une entite omise en tete : SOI (« position() », « impulsion(vec2(0, 5)) »).
						if (args.Size() + 1u == x.signature.nbParams && x.signature.params[0] == NkTypeBp::NK_ENTITE) {
							ValExpr soi;
							soi.r = a.Registre(NkTypeBp::NK_ENTITE);
							soi.t = NkTypeBp::NK_ENTITE;
							a.Emettre(NkOpBp::NK_SOI, soi.r);
							args.Insert(args.Begin(), soi);
						}
						if (args.Size() != x.signature.nbParams) {
							EchecCode(c, cc, colonne,
									  NkString::Format("« %s » attend %u paramètre(s), %u donné(s)", x.libelle, static_cast<unsigned>(x.signature.nbParams),
													   static_cast<unsigned>(args.Size())));
							return false;
						}
						uint32 regs[6] = {};
						uint32 nr = 0;
						for (uint32 i = 0; i < args.Size(); ++i) {
							if (!Convertir(c, cc, args[i], x.signature.params[i], colonne)) {
								return false;
							}
							regs[nr++] = args[i].r;
						}
						sortie = ValExpr();
						for (uint8 i = 0; i < x.signature.nbResultats; ++i) {
							const uint32 r = a.Registre(x.signature.resultats[i]);
							regs[nr++] = r;
							if (i == 0u) {
								sortie.r = r;
								sortie.t = x.signature.resultats[i];
							}
						}
						const int32 imp = a.Import(x.signature.nom);
						if (imp < 0) {
							EchecCode(c, cc, colonne, NkString::Format("natif inconnu : %s", x.signature.nom));
							return false;
						}
						Ligne(c, cc.n->id);
						a.Natif(static_cast<uint32>(imp), regs, nr);
						return true;
					}
					/// Le natif d'un NOM du langage (alias francais, puis nom court).
					int32 NatifDeNom(const NkString &nom) {
						struct A {
								const char *alias;
								const char *natif;
						};
						static const A k[] = {{"abs", "unkeny.math.abs"},
											  {"min", "unkeny.math.min"},
											  {"max", "unkeny.math.max"},
											  {"plancher", "unkeny.math.plancher"},
											  {"floor", "unkeny.math.plancher"},
											  {"racine", "unkeny.math.racine"},
											  {"sqrt", "unkeny.math.racine"},
											  {"sin", "unkeny.math.sin"},
											  {"cos", "unkeny.math.cos"},
											  {"puissance", "unkeny.math.puissance"},
											  {"pow", "unkeny.math.puissance"},
											  {"borner", "unkeny.math.borner"},
											  {"clamp", "unkeny.math.borner"},
											  {"interpoler", "unkeny.math.interpoler"},
											  {"lerp", "unkeny.math.interpoler"},
											  {"jouer_effet", "unkeny.effet.jouer"},
											  {"arreter_effet", "unkeny.effet.arreter"},
											  {"jouer_son", "unkeny.son.jouer"},
											  {"couleur", "unkeny.couleur.rvba"},
											  {"poser_couleur", "unkeny.sprite.poser_couleur"},
											  {"valeur_action", "unkeny.entree.valeur"},
											  {"action_enfoncee", "unkeny.entree.enfoncee"},
											  {"afficher_valeur", "unkeny.journal.afficher_reel"},
											  {"parametre_anim", "unkeny.anim.parametre"},
											  {"vivante", "unkeny.entite.vivante"},
											  {"egaux", "unkeny.texte.egaux"}};
						for (const A &x : k) {
							if (nom == x.alias) {
								return unkeny::NkTrouverNatifBp(x.natif);
							}
						}
						uint32 nb = 0;
						const unkeny::NkNatifBp *t = unkeny::NkNatifsBp(nb);
						for (uint32 i = 0; i < nb; ++i) {
							const char *court = std::strrchr(t[i].signature.nom, '.');
							if ((court != nullptr && nom == court + 1) || nom == t[i].signature.nom) {
								return static_cast<int32>(i);
							}
						}
						return -1;
					}

					bool Gen(const Ctx &c, CtxCode &cc, const NkArbreExpr &ar, int32 i, ValExpr &v, int32 prof) {
						if (!ok) {
							return false;
						}
						if (i < 0 || prof > 128) {
							EchecCode(c, cc, 0u, NkString("expression trop profonde"));
							return false;
						}
						const NkNoeudExpr &x = ar.noeuds[static_cast<uint32>(i)];
						cc.ligne = x.ligne;
						switch (x.genre) {
							case NkGenreNoeudExpr::NK_NOMBRE:
								v.t = x.entier ? NkTypeBp::NK_ENTIER : NkTypeBp::NK_REEL;
								v.r = ConstanteNombre(v.t, x.nombre);
								return true;
							case NkGenreNoeudExpr::NK_BOOLEEN:
								v.t = NkTypeBp::NK_BOOLEEN;
								v.r = a.Registre(v.t);
								a.Emettre(NkOpBp::NK_CONST, v.r, a.ConstBooleen(x.nombre != 0.0));
								return true;
							case NkGenreNoeudExpr::NK_TEXTE:
								v.t = NkTypeBp::NK_TEXTE;
								v.r = a.Registre(v.t);
								a.Emettre(NkOpBp::NK_CONST, v.r, a.ConstTexte(x.nom.CStr()));
								return true;
							case NkGenreNoeudExpr::NK_SOI:
								v.t = NkTypeBp::NK_ENTITE;
								v.r = a.Registre(v.t);
								a.Emettre(NkOpBp::NK_SOI, v.r);
								return true;
							case NkGenreNoeudExpr::NK_NOM: {
								// 1. une SORTIE deja ecrite par ce Code ; 2. une ENTREE du noeud ;
								// 3. une VARIABLE du Blueprint.
								for (uint32 k = 0; k < cc.sorties.Size(); ++k) {
									if (cc.sorties[k] == x.nom) {
										v.r = cc.regs[k];
										v.t = cc.types[k];
										return true;
									}
								}
								const int32 k = cc.n->FindSocket(x.nom.CStr(), graph::NkSocketDir::Input);
								if (k >= 0 && cc.n->sockets[static_cast<uint32>(k)].family == graph::NkSocketFamily::Data) {
									v.t = TypePrise(c, cc.n->sockets[static_cast<uint32>(k)]);
									v.r = Valeur(c, *cc.n, x.nom.CStr(), v.t, prof + 1);
									return ok;
								}
								const int32 var = d.TrouverVariable(x.nom.CStr());
								if (var >= 0) {
									v.t = NkBpTypeDeNom(d.variables[static_cast<uint32>(var)].type.CStr());
									v.r = a.Registre(v.t);
									a.Emettre(NkOpBp::NK_LIRE_VAR, v.r, static_cast<uint32>(var));
									return true;
								}
								EchecCode(c, cc, x.colonne,
										  NkString::Format("« %s » est inconnu (ni une entrée du nœud, ni une variable du Blueprint)", x.nom.CStr()));
								return false;
							}
							case NkGenreNoeudExpr::NK_MEMBRE: {
								ValExpr b;
								if (!Gen(c, cc, ar, x.a, b, prof + 1)) {
									return false;
								}
								if (b.t != NkTypeBp::NK_VEC2 || (!(x.nom == "x") && !(x.nom == "y"))) {
									EchecCode(c, cc, x.colonne, NkString::Format("« .%s » : seul un vec2 a un .x et un .y", x.nom.CStr()));
									return false;
								}
								v.t = NkTypeBp::NK_REEL;
								v.r = a.Registre(v.t);
								a.Emettre(x.nom == "x" ? NkOpBp::NK_VX : NkOpBp::NK_VY, v.r, b.r);
								return true;
							}
							case NkGenreNoeudExpr::NK_UNAIRE: {
								ValExpr b;
								if (!Gen(c, cc, ar, x.a, b, prof + 1)) {
									return false;
								}
								if (x.nom == "!") {
									if (b.t != NkTypeBp::NK_BOOLEEN) {
										EchecCode(c, cc, x.colonne, NkString::Format("« ! » attend un booléen, pas un %s", NomType(b.t)));
										return false;
									}
									v.t = b.t;
									v.r = a.Registre(v.t);
									a.Emettre(NkOpBp::NK_NON, v.r, b.r);
									return true;
								}
								if (b.t == NkTypeBp::NK_ENTIER || b.t == NkTypeBp::NK_REEL) {
									const uint32 z = ConstanteNombre(b.t, 0.0);
									v.t = b.t;
									v.r = a.Registre(v.t);
									a.Emettre(b.t == NkTypeBp::NK_ENTIER ? NkOpBp::NK_SUB_I : NkOpBp::NK_SUB_R, v.r, z, b.r);
									return true;
								}
								if (b.t == NkTypeBp::NK_VEC2) {
									const uint32 m = ConstanteNombre(NkTypeBp::NK_REEL, -1.0);
									v.t = b.t;
									v.r = a.Registre(v.t);
									a.Emettre(NkOpBp::NK_MUL_VR, v.r, b.r, m);
									return true;
								}
								EchecCode(c, cc, x.colonne, NkString::Format("« - » ne s'applique pas à un %s", NomType(b.t)));
								return false;
							}
							case NkGenreNoeudExpr::NK_BINAIRE:
								return Binaire(c, cc, ar, x, v, prof);
							case NkGenreNoeudExpr::NK_APPEL:
								return Appel(c, cc, ar, x, v, prof);
						}
						return false;
					}

					bool Binaire(const Ctx &c, CtxCode &cc, const NkArbreExpr &ar, const NkNoeudExpr &x, ValExpr &v, int32 prof) {
						ValExpr l, r;
						if (!Gen(c, cc, ar, x.a, l, prof + 1) || !Gen(c, cc, ar, x.b, r, prof + 1)) {
							return false;
						}
						cc.ligne = x.ligne;
						const NkString &op = x.nom;
						const bool num = (l.t == NkTypeBp::NK_ENTIER || l.t == NkTypeBp::NK_REEL) && (r.t == NkTypeBp::NK_ENTIER || r.t == NkTypeBp::NK_REEL);
						auto unifier = [&]() {
							// entier op entier reste entier ; un reel promeut l'autre.
							if (l.t != r.t) {
								Convertir(c, cc, l, NkTypeBp::NK_REEL, x.colonne);
								Convertir(c, cc, r, NkTypeBp::NK_REEL, x.colonne);
							}
						};
						if (op == "&&" || op == "||") {
							if (l.t != NkTypeBp::NK_BOOLEEN || r.t != NkTypeBp::NK_BOOLEEN) {
								EchecCode(c, cc, x.colonne, NkString::Format("« %s » relie deux booléens (%s, %s)", op.CStr(), NomType(l.t), NomType(r.t)));
								return false;
							}
							v.t = NkTypeBp::NK_BOOLEEN;
							v.r = a.Registre(v.t);
							a.Emettre(op == "&&" ? NkOpBp::NK_ET : NkOpBp::NK_OU, v.r, l.r, r.r);
							return true;
						}
						if (op == "+" && (l.t == NkTypeBp::NK_TEXTE || r.t == NkTypeBp::NK_TEXTE)) {
							// Texte + valeur : la valeur devient un texte, puis on concatene.
							auto enTexte = [&](ValExpr &w) -> bool {
								if (w.t == NkTypeBp::NK_TEXTE) {
									return true;
								}
								const char *conv = w.t == NkTypeBp::NK_ENTIER ? "unkeny.texte.de_entier" : (w.t == NkTypeBp::NK_REEL ? "unkeny.texte.de_reel" : nullptr);
								if (conv == nullptr) {
									EchecCode(c, cc, x.colonne, NkString::Format("un %s ne se met pas en texte ici", NomType(w.t)));
									return false;
								}
								NkVector<ValExpr> args;
								args.PushBack(w);
								return AppelNatif(c, cc, unkeny::NkTrouverNatifBp(conv), args, w, x.colonne);
							};
							if (!enTexte(l) || !enTexte(r)) {
								return false;
							}
							NkVector<ValExpr> args;
							args.PushBack(l);
							args.PushBack(r);
							return AppelNatif(c, cc, unkeny::NkTrouverNatifBp("unkeny.texte.concatener"), args, v, x.colonne);
						}
						if (op == "+" || op == "-" || op == "*" || op == "/") {
							if (num) {
								if (op == "/") {
									// « / » rend toujours un REEL (5 / 7 = 0,714, pas 0) ; la
									// division entiere s'ecrit entier(a / b).
									Convertir(c, cc, l, NkTypeBp::NK_REEL, x.colonne);
									Convertir(c, cc, r, NkTypeBp::NK_REEL, x.colonne);
								}
								unifier();
								v.t = l.t;
								v.r = a.Registre(v.t);
								const bool ent = v.t == NkTypeBp::NK_ENTIER;
								const NkOpBp o = op == "+" ? (ent ? NkOpBp::NK_ADD_I : NkOpBp::NK_ADD_R)
												 : op == "-" ? (ent ? NkOpBp::NK_SUB_I : NkOpBp::NK_SUB_R)
												 : op == "*" ? (ent ? NkOpBp::NK_MUL_I : NkOpBp::NK_MUL_R)
															 : (ent ? NkOpBp::NK_DIV_I : NkOpBp::NK_DIV_R);
								a.Emettre(o, v.r, l.r, r.r);
								return true;
							}
							if (l.t == NkTypeBp::NK_VEC2 && r.t == NkTypeBp::NK_VEC2 && (op == "+" || op == "-")) {
								v.t = NkTypeBp::NK_VEC2;
								v.r = a.Registre(v.t);
								a.Emettre(op == "+" ? NkOpBp::NK_ADD_V : NkOpBp::NK_SUB_V, v.r, l.r, r.r);
								return true;
							}
							if ((op == "*" || op == "/") && (l.t == NkTypeBp::NK_VEC2 || r.t == NkTypeBp::NK_VEC2)) {
								ValExpr vec = l.t == NkTypeBp::NK_VEC2 ? l : r;
								ValExpr k = l.t == NkTypeBp::NK_VEC2 ? r : l;
								if (op == "/" && l.t != NkTypeBp::NK_VEC2) {
									EchecCode(c, cc, x.colonne, NkString("un nombre ne se divise pas par un vec2"));
									return false;
								}
								if (!Convertir(c, cc, k, NkTypeBp::NK_REEL, x.colonne)) {
									return false;
								}
								if (op == "/") {
									const uint32 un = ConstanteNombre(NkTypeBp::NK_REEL, 1.0);
									const uint32 inv = a.Registre(NkTypeBp::NK_REEL);
									a.Emettre(NkOpBp::NK_DIV_R, inv, un, k.r);
									k.r = inv;
								}
								v.t = NkTypeBp::NK_VEC2;
								v.r = a.Registre(v.t);
								a.Emettre(NkOpBp::NK_MUL_VR, v.r, vec.r, k.r);
								return true;
							}
							EchecCode(c, cc, x.colonne, NkString::Format("« %s » entre un %s et un %s : impossible", op.CStr(), NomType(l.t), NomType(r.t)));
							return false;
						}
						// Les comparaisons.
						v.t = NkTypeBp::NK_BOOLEEN;
						v.r = a.Registre(v.t);
						if (op == "==" || op == "!=") {
							if (num) {
								unifier();
								a.Emettre(l.t == NkTypeBp::NK_ENTIER ? NkOpBp::NK_EQ_I : NkOpBp::NK_EQ_R, v.r, l.r, r.r);
							} else if (l.t == NkTypeBp::NK_ENTITE && r.t == NkTypeBp::NK_ENTITE) {
								a.Emettre(NkOpBp::NK_EQ_E, v.r, l.r, r.r);
							} else if (l.t == NkTypeBp::NK_TEXTE && r.t == NkTypeBp::NK_TEXTE) {
								NkVector<ValExpr> args;
								args.PushBack(l);
								args.PushBack(r);
								ValExpr e;
								if (!AppelNatif(c, cc, unkeny::NkTrouverNatifBp("unkeny.texte.egaux"), args, e, x.colonne)) {
									return false;
								}
								v.r = e.r;
							} else if (l.t == NkTypeBp::NK_BOOLEEN && r.t == NkTypeBp::NK_BOOLEEN) {
								// a == b  <=>  (a et b) ou (non a et non b)
								const uint32 ab = a.Registre(v.t), na = a.Registre(v.t), nb = a.Registre(v.t), nn = a.Registre(v.t);
								a.Emettre(NkOpBp::NK_ET, ab, l.r, r.r);
								a.Emettre(NkOpBp::NK_NON, na, l.r);
								a.Emettre(NkOpBp::NK_NON, nb, r.r);
								a.Emettre(NkOpBp::NK_ET, nn, na, nb);
								a.Emettre(NkOpBp::NK_OU, v.r, ab, nn);
							} else {
								EchecCode(c, cc, x.colonne, NkString::Format("« %s » entre un %s et un %s : impossible", op.CStr(), NomType(l.t), NomType(r.t)));
								return false;
							}
							if (op == "!=") {
								const uint32 n = a.Registre(v.t);
								a.Emettre(NkOpBp::NK_NON, n, v.r);
								v.r = n;
							}
							return true;
						}
						if (!num) {
							EchecCode(c, cc, x.colonne, NkString::Format("« %s » compare des nombres, pas un %s et un %s", op.CStr(), NomType(l.t), NomType(r.t)));
							return false;
						}
						unifier();
						const bool ent = l.t == NkTypeBp::NK_ENTIER;
						// a > b  <=>  b < a ; a >= b  <=>  b <= a
						const bool inverse = op == ">" || op == ">=";
						const bool strict = op == "<" || op == ">";
						const NkOpBp o = strict ? (ent ? NkOpBp::NK_LT_I : NkOpBp::NK_LT_R) : (ent ? NkOpBp::NK_LE_I : NkOpBp::NK_LE_R);
						a.Emettre(o, v.r, inverse ? r.r : l.r, inverse ? l.r : r.r);
						return true;
					}

					bool Appel(const Ctx &c, CtxCode &cc, const NkArbreExpr &ar, const NkNoeudExpr &x, ValExpr &v, int32 prof) {
						NkVector<ValExpr> args;
						for (uint32 i = 0; i < x.args.Size(); ++i) {
							ValExpr w;
							if (!Gen(c, cc, ar, x.args[i], w, prof + 1)) {
								return false;
							}
							args.PushBack(w);
						}
						cc.ligne = x.ligne;
						const NkString &nom = x.nom;
						auto nbArgs = [&](uint32 n) -> bool {
							if (args.Size() != n) {
								EchecCode(c, cc, x.colonne, NkString::Format("« %s » attend %u paramètre(s), %u donné(s)", nom.CStr(), static_cast<unsigned>(n),
																			 static_cast<unsigned>(args.Size())));
								return false;
							}
							return true;
						};
						// Les fonctions du LANGAGE, qui sont des instructions de la machine.
						if (nom == "vec2") {
							if (!nbArgs(2u) || !Convertir(c, cc, args[0], NkTypeBp::NK_REEL, x.colonne) || !Convertir(c, cc, args[1], NkTypeBp::NK_REEL, x.colonne)) {
								return false;
							}
							v.t = NkTypeBp::NK_VEC2;
							v.r = a.Registre(v.t);
							a.Emettre(NkOpBp::NK_VEC2, v.r, args[0].r, args[1].r);
							return true;
						}
						if (nom == "longueur" || nom == "length" || nom == "normaliser" || nom == "normalize") {
							if (!nbArgs(1u) || !Convertir(c, cc, args[0], NkTypeBp::NK_VEC2, x.colonne)) {
								return false;
							}
							const bool lg = nom == "longueur" || nom == "length";
							v.t = lg ? NkTypeBp::NK_REEL : NkTypeBp::NK_VEC2;
							v.r = a.Registre(v.t);
							a.Emettre(lg ? NkOpBp::NK_LONGUEUR : NkOpBp::NK_NORMALISER, v.r, args[0].r);
							return true;
						}
						if (nom == "reel" || nom == "float") {
							if (!nbArgs(1u) || !Convertir(c, cc, args[0], NkTypeBp::NK_REEL, x.colonne)) {
								return false;
							}
							v = args[0];
							return true;
						}
						if (nom == "entier" || nom == "int") {
							if (!nbArgs(1u)) {
								return false;
							}
							if (args[0].t == NkTypeBp::NK_ENTIER) {
								v = args[0];
								return true;
							}
							return AppelNatif(c, cc, unkeny::NkTrouverNatifBp("unkeny.math.entier"), args, v, x.colonne);
						}
						if (nom == "texte" || nom == "str" || nom == "string") {
							if (!nbArgs(1u)) {
								return false;
							}
							if (args[0].t == NkTypeBp::NK_TEXTE) {
								v = args[0];
								return true;
							}
							const char *conv = args[0].t == NkTypeBp::NK_ENTIER ? "unkeny.texte.de_entier" : "unkeny.texte.de_reel";
							return AppelNatif(c, cc, unkeny::NkTrouverNatifBp(conv), args, v, x.colonne);
						}
						if (nom == "arrondi" || nom == "round") {
							// arrondi(x) ou arrondi(x, decimales) -- « $round(..., 1) ».
							if (args.Size() == 1u) {
								ValExpr z;
								z.t = NkTypeBp::NK_ENTIER;
								z.r = ConstanteNombre(z.t, 0.0);
								args.PushBack(z);
							}
							if (!nbArgs(2u)) {
								return false;
							}
							return AppelNatif(c, cc, unkeny::NkTrouverNatifBp("unkeny.math.arrondi"), args, v, x.colonne);
						}
						// Une FONCTION du Blueprint.
						const int32 g = d.TrouverGraphe(nom.CStr(), NkGenreGrapheBp::NK_FONCTION);
						if (g >= 0) {
							const NkGrapheBp &f = d.graphes[static_cast<uint32>(g)];
							if (!f.pure && cc.pur) {
								EchecCode(c, cc, x.colonne, NkString::Format("la fonction « %s » AGIT : appelez-la dans un nœud Code", nom.CStr()));
								return false;
							}
							if (!nbArgs(static_cast<uint32>(f.entrees.Size()))) {
								return false;
							}
							NkVector<uint32> regs;
							for (uint32 i = 0; i < args.Size(); ++i) {
								if (!Convertir(c, cc, args[i], NkBpTypeDeNom(f.entrees[i].type.CStr()), x.colonne)) {
									return false;
								}
								regs.PushBack(args[i].r);
							}
							v = ValExpr();
							for (uint32 i = 0; i < f.sorties.Size(); ++i) {
								const NkTypeBp t = NkBpTypeDeNom(f.sorties[i].type.CStr());
								const uint32 r = a.Registre(t);
								regs.PushBack(r);
								if (i == 0u) {
									v.r = r;
									v.t = t;
								}
							}
							Ligne(c, cc.n->id);
							a.Appel(static_cast<uint32>(fonctions[static_cast<uint32>(g)]), regs.Data(), static_cast<uint32>(regs.Size()));
							return true;
						}
						// Un NATIF (alias francais ou nom court).
						const int32 k = NatifDeNom(nom);
						if (k >= 0) {
							return AppelNatif(c, cc, k, args, v, x.colonne);
						}
						EchecCode(c, cc, x.colonne, NkString::Format("fonction inconnue : « %s »", nom.CStr()));
						return false;
					}

					/// Analyse le code du noeud ; false : erreur posee (ligne, colonne).
					bool Analyser(const Ctx &c, const graph::NkNode &n, bool instructions, NkArbreExpr &ar) {
						const NkString code = NkBpCodeNoeud(*c.g, n);
						if (!NkBpAnalyserExpr(code.CStr(), instructions, ar)) {
							Echec(c, n.id, NkString::Format("ligne %u, colonne %u : %s", static_cast<unsigned>(ar.ligne), static_cast<unsigned>(ar.colonne),
															ar.erreur.CStr()));
							return false;
						}
						return true;
					}

					/// La valeur d'une EXPRESSION (noeud pur), dans le type de sa sortie.
					uint32 SortieExpression(const Ctx &c, const graph::NkNode &n, int32 k, int32 prof) {
						NkArbreExpr ar;
						if (!Analyser(c, n, false, ar)) {
							return 0u;
						}
						CtxCode cc;
						cc.n = &n;
						cc.pur = true;
						ValExpr v;
						if (!Gen(c, cc, ar, ar.instructions[0].valeur, v, prof)) {
							return 0u;
						}
						const NkTypeBp t = TypePrise(c, n.sockets[static_cast<uint32>(k)]);
						if (!Convertir(c, cc, v, t, 1u)) {
							return 0u;
						}
						return v.r;
					}

					/// Les INSTRUCTIONS d'un noeud Code.
					void ExecuterCode(const Ctx &c, const graph::NkNode &n, int32 prof) {
						NkArbreExpr ar;
						if (!Analyser(c, n, true, ar)) {
							return;
						}
						CtxCode cc;
						cc.n = &n;
						cc.pur = false;
						// Ses sorties : des registres (zero au depart), gardes pour la suite.
						for (uint32 k = 0; k < n.sockets.Size(); ++k) {
							const graph::NkSocket &s = n.sockets[k];
							if (s.dir == graph::NkSocketDir::Output && s.family == graph::NkSocketFamily::Data) {
								const NkTypeBp t = TypePrise(c, s);
								const uint32 r = a.Registre(t);
								cc.sorties.PushBack(s.name);
								cc.regs.PushBack(r);
								cc.types.PushBack(t);
								Garder(c, n.id, s.name.CStr(), r);
							}
						}
						const NkString code = NkBpCodeNoeud(*c.g, n);
						for (uint32 i = 0; i < ar.instructions.Size() && ok; ++i) {
							const NkInstructionExpr &ins = ar.instructions[i];
							cc.ligne = 1u;
							// La ligne de l'instruction (pour les messages).
							const NkNoeudExpr &x0 = ar.noeuds[static_cast<uint32>(ins.valeur)];
							(void)x0;
							ValExpr v;
							if (ins.cible.Empty()) {
								const NkNoeudExpr &x = ar.noeuds[static_cast<uint32>(ins.valeur)];
								if (x.genre != NkGenreNoeudExpr::NK_APPEL) {
									EchecCode(c, cc, x.colonne, NkString("une instruction AGIT : une affectation (« x = ... ») ou un appel (« afficher(...) »)"));
									return;
								}
								Gen(c, cc, ar, ins.valeur, v, prof);
								continue;
							}
							if (!Gen(c, cc, ar, ins.valeur, v, prof)) {
								return;
							}
							// La cible : une SORTIE du noeud, sinon une VARIABLE.
							bool fait = false;
							for (uint32 k = 0; k < cc.sorties.Size() && !fait; ++k) {
								if (cc.sorties[k] == ins.cible) {
									if (!Convertir(c, cc, v, cc.types[k], ins.colonneCible)) {
										return;
									}
									Ligne(c, n.id);
									a.Emettre(NkOpBp::NK_COPIER, cc.regs[k], v.r);
									fait = true;
								}
							}
							if (!fait) {
								const int32 var = d.TrouverVariable(ins.cible.CStr());
								if (var < 0) {
									EchecCode(c, cc, ins.colonneCible,
											  NkString::Format("« %s » n'est ni une sortie du nœud, ni une variable du Blueprint", ins.cible.CStr()));
									return;
								}
								if (!Convertir(c, cc, v, NkBpTypeDeNom(d.variables[static_cast<uint32>(var)].type.CStr()), ins.colonneCible)) {
									return;
								}
								Ligne(c, n.id);
								a.Emettre(NkOpBp::NK_ECRIRE_VAR, static_cast<uint32>(var), v.r);
							}
						}
						(void)code;
					}

					/// Les types d'une liste de parametres. false : un type que la
					/// machine ne porte pas (l'erreur designe le graphe).
					bool Types(const NkVector<NkParamBp> &p, NkVector<NkTypeBp> &t, const char *quoi, const Ctx &c) {
						for (uint32 i = 0; i < p.Size(); ++i) {
							const NkTypeBp x = NkBpTypeDeNom(p[i].type.CStr());
							if (x == NkTypeBp::NK_RIEN || p[i].tableau) {
								Echec(c, graph::NK_NODE_INVALID,
									  NkString::Format("%s « %s » : type « %s » non pris en charge%s", quoi, p[i].nom.CStr(),
													   p[i].type.CStr(), p[i].tableau ? " (les tableaux viennent au prochain lot)" : ""));
								return false;
							}
							t.PushBack(x);
						}
						return true;
					}
			};
		} // namespace

		bool NkBpCompilerDocument(const NkDocumentBp &d, unkeny::NkModuleBp &sortie, NkErreurBp &erreur) {
			erreur = NkErreurBp();
			NkCompilo c(d, erreur);
			// ── 1. Chaque graphe est-il SAIN ? (Validate connait la famille : G1) ──
			for (uint32 gi = 0; gi < d.graphes.Size(); ++gi) {
				NkVector<graph::NkGraphDiag> diags;
				if (d.graphes[gi].graphe.Validate(diags) > 0u) {
					erreur.message = NkString::Format("graphe invalide : %s %s", graph::NkGraphIssueName(diags[0].issue), diags[0].detail.CStr());
					erreur.noeud = diags[0].node;
					erreur.graphe = gi;
					return false;
				}
				for (uint32 i = 0; i < d.graphes[gi].graphe.RawNodeCount(); ++i) {
					const graph::NkNode *n = d.graphes[gi].graphe.RawNodeAt(i);
					if (n != nullptr && n->alive && NkBpGenre(*n) == NkGenreNoeudBp::NK_INCONNU) {
						erreur.message = NkString::Format("nœud inconnu de ce catalogue : %s", n->type.CStr());
						erreur.noeud = n->id;
						erreur.graphe = gi;
						return false;
					}
				}
			}
			Ctx racine;
			racine.g = &d.graphes[0].graphe;
			// ── 2. Les VARIABLES declarees : les premieres du module, dans l'ordre ──
			if (d.variables.Size() > unkeny::NK_UNKENY_SCRIPT_VARS_MAX) {
				erreur.message = NkString::Format("trop de variables (%u au plus par script)", static_cast<unsigned>(unkeny::NK_UNKENY_SCRIPT_VARS_MAX));
				return false;
			}
			for (uint32 i = 0; i < d.variables.Size(); ++i) {
				const NkVariableDeclBp &v = d.variables[i];
				const NkTypeBp t = NkBpTypeDeNom(v.type.CStr());
				if (v.nom.Empty() || v.nom.Length() >= unkeny::NK_UNKENY_VAR_NOM_MAX) {
					erreur.message = NkString::Format("variable « %s » : nom vide ou trop long (%u caractères au plus)", v.nom.CStr(),
													  static_cast<unsigned>(unkeny::NK_UNKENY_VAR_NOM_MAX - 1u));
					return false;
				}
				if (t == NkTypeBp::NK_RIEN || v.tableau) {
					erreur.message = NkString::Format("variable « %s » : type « %s » non pris en charge%s", v.nom.CStr(), v.type.CStr(),
													  v.tableau ? " (les tableaux viennent au prochain lot)" : "");
					return false;
				}
				unkeny::NkValeurBp defaut;
				NkString texte;
				if (!NkBpValeurDeTexte(v.type.CStr(), v.defaut.CStr(), defaut, &texte)) {
					erreur.message = NkString::Format("variable « %s » : valeur par défaut « %s » illisible pour un %s", v.nom.CStr(),
													  v.defaut.CStr(), NkBpLibelleType(v.type.CStr()));
					return false;
				}
				if (t == NkTypeBp::NK_TEXTE) {
					defaut.i = static_cast<int32>(c.a.ConstTexte(texte.CStr()));
				}
				c.a.Variable(v.nom.CStr(), t, defaut, v.instance);
			}
			// ── 3. Les FONCTIONS : declarees d'abord (leurs appels se resolvent) ──
			c.fonctions.Resize(d.graphes.Size(), -1);
			for (uint32 gi = 1; gi < d.graphes.Size() && c.ok; ++gi) {
				const NkGrapheBp &g = d.graphes[gi];
				if (g.genre != NkGenreGrapheBp::NK_FONCTION) {
					continue;
				}
				Ctx cg;
				cg.graphe = gi;
				cg.g = &g.graphe;
				NkVector<NkTypeBp> pe, ps;
				if (!c.Types(g.entrees, pe, "entrée", cg) || !c.Types(g.sorties, ps, "sortie", cg)) {
					return false;
				}
				const uint32 f = c.a.Fonction(g.nom.CStr());
				c.a.Signature(pe.Data(), static_cast<uint32>(pe.Size()), ps.Data(), static_cast<uint32>(ps.Size()));
				c.fonctions[gi] = static_cast<int32>(f);
			}
			// ── 4. Leurs CORPS : depuis « Entree », le long des fils d'execution ──
			for (uint32 gi = 1; gi < d.graphes.Size() && c.ok; ++gi) {
				const NkGrapheBp &g = d.graphes[gi];
				if (g.genre != NkGenreGrapheBp::NK_FONCTION) {
					continue;
				}
				Ctx cg;
				cg.graphe = gi;
				cg.g = &g.graphe;
				cg.fonction = gi;
				const graph::NkNode *e = c.TrouverType(g.graphe, NK_BP_FN_ENTREE);
				if (e == nullptr) {
					c.Echec(cg, graph::NK_NODE_INVALID, NkString::Format("la fonction « %s » n'a pas (ou a deux) nœud d'entrée", g.nom.CStr()));
					break;
				}
				for (uint32 i = 0; i < g.graphe.RawNodeCount(); ++i) {
					const graph::NkNode *n = g.graphe.RawNodeAt(i);
					if (n != nullptr && n->alive && (NkBpGenre(*n) == NkGenreNoeudBp::NK_EVENEMENT || NkBpGenre(*n) == NkGenreNoeudBp::NK_REP_EVENEMENT)) {
						c.Echec(cg, n->id, NkString("un événement vit dans le graphe d'événements, pas dans une fonction"));
					}
				}
				c.a.Courante(static_cast<uint32>(c.fonctions[gi]));
				c.caches.Clear();
				c.Ligne(cg, e->id);
				c.Chaine(cg, *e, "suite", 0);
				c.a.Emettre(NkOpBp::NK_FIN);
			}
			if (!c.ok) {
				return false;
			}
			// ── 5. Le GRAPHE D'EVENEMENTS : une fonction par evenement ──
			const NkGrapheBp &ev = d.graphes[0];
			uint32 evenements = 0;
			for (uint32 i = 0; i < ev.graphe.RawNodeCount() && c.ok; ++i) {
				const graph::NkNode *n = ev.graphe.RawNodeAt(i);
				if (n == nullptr || !n->alive) {
					continue;
				}
				const NkGenreNoeudBp genre = NkBpGenre(*n);
				if (genre == NkGenreNoeudBp::NK_FN_ENTREE || genre == NkGenreNoeudBp::NK_FN_RETOUR || genre == NkGenreNoeudBp::NK_MACRO_ENTREE ||
					genre == NkGenreNoeudBp::NK_MACRO_SORTIE) {
					c.Echec(racine, n->id, NkString("ce nœud n'a de sens que dans une fonction ou une macro"));
					break;
				}
				Ctx ce = racine;
				ce.evenement = n->id;
				if (genre == NkGenreNoeudBp::NK_EVENEMENT) {
					const NkProtoBp *p = NkBpProto(n->type.CStr());
					++evenements;
					const uint32 f = c.a.Fonction(p->libelle.CStr());
					NkString parametre;
					if (p->evenement == NK_UNK_EV_ACTION_PRESSEE || p->evenement == NK_UNK_EV_ACTION_RELACHEE) {
						const int32 k = n->FindSocket("action", graph::NkSocketDir::Input);
						if (k >= 0 && ev.graphe.IncomingOf(n->id, k) != nullptr) {
							c.Echec(ce, n->id, NkString("« action » doit être une constante (saisie sur le nœud), pas un fil"));
							break;
						}
						const graph::NkGraphValue &v = n->sockets[static_cast<uint32>(k)].defaultValue;
						parametre = v.IsSet() ? v.text : NkString();
						if (parametre.Empty()) {
							c.Echec(ce, n->id, NkString("événement d'action : saisir le NOM de l'action (« Sauter »...)"));
							break;
						}
					}
					c.a.Entree(p->evenement, f, parametre.CStr());
				} else if (genre == NkGenreNoeudBp::NK_REP_EVENEMENT) {
					const int32 r = d.TrouverRepartiteur(NkBpPropTexte(ev.graphe, *n, "rep").CStr());
					if (r < 0) {
						c.Echec(ce, n->id, NkString("répartiteur inconnu (supprimé ?)"));
						break;
					}
					const NkRepartiteurBp &rep = d.repartiteurs[static_cast<uint32>(r)];
					NkVector<NkTypeBp> pt;
					if (!c.Types(rep.params, pt, "paramètre", ce)) {
						break;
					}
					++evenements;
					const uint32 f = c.a.Fonction((NkString("Événement ") + rep.nom).CStr());
					c.a.Signature(pt.Data(), static_cast<uint32>(pt.Size()), nullptr, 0u);
					c.a.Entree(NK_UNK_EV_PERSONNALISE, f, rep.nom.CStr());
				} else {
					continue;
				}
				c.caches.Clear();
				c.Ligne(ce, n->id);
				c.Chaine(ce, *n, "suite", 0);
				c.a.Emettre(NkOpBp::NK_FIN);
			}
			if (!c.ok) {
				return false;
			}
			if (evenements == 0u) {
				erreur.message = "aucun événement : un Blueprint commence par un nœud « Événement »";
				return false;
			}
			NkString texte;
			NkBpInstantane(d, texte);
			c.a.module.empreinte = unkeny::NkEmpreinteBp(texte.CStr(), texte.Length());
			// ── 6. Verifie COMME LE JEU LE VERIFIERA (natif hors evenement...) ──
			unkeny::NkProgrammeBp p;
			p.module = c.a.module;
			unkeny::NkRefusBp refus;
			if (!unkeny::NkPreparerProgrammeBp(p, refus)) {
				erreur.message = refus.raison;
				erreur.noeud = NkBpNoeudDuCode(refus.noeud);
				erreur.graphe = NkBpGrapheDuCode(refus.noeud);
				return false;
			}
			sortie = c.a.module;
			return true;
		}

		// =====================================================================
		// Le menu de la toile
		// =====================================================================
		namespace {
			void Proto(const NkProtoBp &p, NkEntreeMenuBp &e) {
				e.cle = p.type;
				e.libelle = p.libelle;
				e.categorie = p.categorie;
				e.aide = p.aide;
				for (uint8 k = 0; k < p.nbEntrees; ++k) {
					e.entrees.PushBack(NkString(p.entrees[k].type));
				}
				for (uint8 k = 0; k < p.nbSorties; ++k) {
					e.sorties.PushBack(NkString(p.sorties[k].type));
				}
			}
			void Types(const NkVector<NkParamBp> &p, NkVector<NkString> &t) {
				for (uint32 i = 0; i < p.Size(); ++i) {
					t.PushBack(p[i].type);
				}
			}
		} // namespace

		void NkBpEntreesMenu(const NkDocumentBp &d, uint32 graphe, NkVector<NkEntreeMenuBp> &sortie) {
			sortie.Clear();
			const bool evenements = graphe == 0u;
			const NkGenreGrapheBp genre = graphe < d.graphes.Size() ? d.graphes[graphe].genre : NkGenreGrapheBp::NK_EVENEMENTS;
			// Les variables declarees, d'abord (ce qu'on cherche le plus).
			for (uint32 i = 0; i < d.variables.Size(); ++i) {
				const NkVariableDeclBp &v = d.variables[i];
				NkEntreeMenuBp a;
				a.cle = NkString("bp.var.get:") + v.nom;
				a.libelle = NkString("Lire ") + v.nom;
				a.categorie = v.categorie.Empty() ? NkString("Variables") : NkString("Variables|") + v.categorie;
				a.aide = v.infobulle;
				a.sorties.PushBack(v.type);
				sortie.PushBack(a);
				NkEntreeMenuBp b;
				b.cle = NkString("bp.var.set:") + v.nom;
				b.libelle = NkString("Écrire ") + v.nom;
				b.categorie = a.categorie;
				b.aide = v.infobulle;
				b.entrees.PushBack(NkString("exec"));
				b.entrees.PushBack(v.type);
				b.sorties.PushBack(NkString("exec"));
				b.sorties.PushBack(v.type);
				sortie.PushBack(b);
			}
			for (uint32 i = 1; i < d.graphes.Size(); ++i) {
				const NkGrapheBp &g = d.graphes[i];
				NkEntreeMenuBp e;
				if (g.genre == NkGenreGrapheBp::NK_FONCTION) {
					e.cle = NkString("bp.appel:") + g.nom;
					e.libelle = g.nom;
					e.categorie = g.categorie.Empty() ? NkString("Fonctions") : NkString("Fonctions|") + g.categorie;
					if (!g.pure) {
						e.entrees.PushBack(NkString("exec"));
						e.sorties.PushBack(NkString("exec"));
					}
				} else {
					if (i == graphe) {
						continue; // une macro ne s'insere pas dans elle-meme
					}
					e.cle = NkString("bp.macro:") + g.nom;
					e.libelle = g.nom;
					e.categorie = g.categorie.Empty() ? NkString("Macros") : NkString("Macros|") + g.categorie;
				}
				e.aide = g.infobulle;
				Types(g.entrees, e.entrees);
				Types(g.sorties, e.sorties);
				sortie.PushBack(e);
			}
			for (uint32 i = 0; i < d.repartiteurs.Size(); ++i) {
				const NkRepartiteurBp &r = d.repartiteurs[i];
				NkEntreeMenuBp a;
				a.cle = NkString("bp.rep.appeler:") + r.nom;
				a.libelle = NkString("Appeler ") + r.nom;
				a.categorie = "Répartiteurs d'événements";
				a.aide = r.infobulle;
				a.entrees.PushBack(NkString("exec"));
				a.entrees.PushBack(NkString("entite"));
				Types(r.params, a.entrees);
				a.sorties.PushBack(NkString("exec"));
				sortie.PushBack(a);
				if (evenements) {
					NkEntreeMenuBp b;
					b.cle = NkString("bp.rep.evenement:") + r.nom;
					b.libelle = NkString("Événement ") + r.nom;
					b.categorie = "Répartiteurs d'événements";
					b.aide = r.infobulle;
					b.sorties.PushBack(NkString("exec"));
					Types(r.params, b.sorties);
					sortie.PushBack(b);
				}
			}
			if (genre == NkGenreGrapheBp::NK_FONCTION) {
				NkEntreeMenuBp e;
				e.cle = NK_BP_FN_RETOUR;
				e.libelle = "Retour";
				e.categorie = "Fonction";
				e.aide = "Rend les sorties de la fonction et la termine.";
				e.entrees.PushBack(NkString("exec"));
				Types(d.graphes[graphe].sorties, e.entrees);
				sortie.PushBack(e);
			}
			// Le catalogue fixe : les evenements seulement dans le graphe
			// d'evenements ; les ANCIENS noeuds de variable (nom saisi) restent
			// lisibles mais ne se proposent plus.
			const NkVector<NkProtoBp> &protos = NkBpProtos();
			for (uint32 i = 0; i < protos.Size(); ++i) {
				const NkProtoBp &p = protos[i];
				if (p.genre == NkGenreNoeudBp::NK_LIRE_VAR || p.genre == NkGenreNoeudBp::NK_ECRIRE_VAR) {
					continue;
				}
				if (p.genre == NkGenreNoeudBp::NK_EVENEMENT && !evenements) {
					continue;
				}
				NkEntreeMenuBp e;
				Proto(p, e);
				sortie.PushBack(e);
			}
			// Les noeuds de CODE (NkBpExpression.h) : la reference principale.
			{
				NkEntreeMenuBp x;
				x.cle = NK_BP_EXPRESSION;
				x.libelle = "Expression";
				x.categorie = "Code";
				x.aide = "Un calcul ÉCRIT (« arrondi((a - b) / a * 100, 1) ») : ses entrées, ses variables, un résultat.";
				x.entrees.PushBack(NkString("reel"));
				x.entrees.PushBack(NkString("reel"));
				x.sorties.PushBack(NkString("reel"));
				sortie.PushBack(x);
				NkEntreeMenuBp si;
				si.cle = NK_BP_SI_EXPRESSION;
				si.libelle = "Si (expression)";
				si.categorie = "Code";
				si.aide = "Une condition ÉCRITE (« runtime > -1 ») : Vrai ou Faux.";
				si.entrees.PushBack(NkString("exec"));
				si.entrees.PushBack(NkString("reel"));
				si.sorties.PushBack(NkString("exec"));
				sortie.PushBack(si);
				NkEntreeMenuBp co;
				co.cle = NK_BP_CODE;
				co.libelle = "Code";
				co.categorie = "Code";
				co.aide = "Des INSTRUCTIONS, une par ligne : « ouverte = vrai », « afficher(\"...\") », « OuvrirPorte(2) ».";
				co.entrees.PushBack(NkString("exec"));
				co.sorties.PushBack(NkString("exec"));
				sortie.PushBack(co);
			}
			{
				NkEntreeMenuBp e;
				e.cle = NK_BP_COMMENTAIRE;
				e.libelle = "Ajouter un commentaire";
				e.categorie = "Utilitaires";
				e.aide = "Un cadre titré qui entoure des nœuds (touche C sur une sélection).";
				sortie.PushBack(e);
				NkEntreeMenuBp r;
				r.cle = "bp.relais:exec";
				r.libelle = "Nœud de réacheminement";
				r.categorie = "Utilitaires";
				r.aide = "Un point de passage pour ranger les fils (double-clic sur un fil).";
				r.entrees.PushBack(NkString("exec"));
				r.sorties.PushBack(NkString("exec"));
				sortie.PushBack(r);
			}
		}

		// =====================================================================
		// La porte, reecrite avec de VRAIES declarations
		// =====================================================================
		void NkBpDocumentPorte(NkDocumentBp &d, const char *porte) {
			d.Vider();
			d.description = "Ouvre une porte quand le Joueur entre dans la zone qui porte ce script.";
			d.categorie = "Exemples";
			// Les variables : « ouverte » (l'etat), « porte » et « hauteur » (a
			// regler PAR INSTANCE dans les Details : un seul Blueprint, deux portes).
			{
				NkVariableDeclBp v;
				v.nom = "ouverte";
				v.type = "booleen";
				v.defaut = "faux";
				v.instance = false;
				v.infobulle = "Vraie une fois la porte ouverte : elle ne remonte pas deux fois.";
				v.categorie = "État";
				d.variables.PushBack(v);
				NkVariableDeclBp p;
				p.nom = "porte";
				p.type = "texte";
				p.defaut = porte != nullptr ? porte : "Porte";
				p.instance = true;
				p.infobulle = "Le NOM de l'entité porte à ouvrir.";
				p.categorie = "Réglages";
				d.variables.PushBack(p);
				NkVariableDeclBp h;
				h.nom = "hauteur";
				h.type = "reel";
				h.defaut = "2";
				h.instance = true;
				h.infobulle = "De combien la porte monte (mètres).";
				h.categorie = "Réglages";
				d.variables.PushBack(h);
			}
			// La fonction OuvrirPorte(hauteur) : Entree -> Afficher -> Teleporter(porte,
			// Position + (0, hauteur)) -> Jouer l'effet -> ouverte = vrai -> Retour.
			const uint32 gf = NkBpAjouterFonction(d, "OuvrirPorte");
			NkGrapheBp &f = d.graphes[gf];
			{
				NkParamBp e;
				e.nom = "hauteur";
				e.type = "reel";
				f.entrees.PushBack(e);
				f.categorie = "Porte";
				f.infobulle = "Monte la porte de « hauteur » mètres, joue son effet, et retient qu'elle est ouverte.";
				NkBpRafraichirNoeuds(d); // l'entree gagne sa prise « hauteur »
			}
			graph::NkNodeGraph &g = d.graphes[gf].graphe;
			graph::NkNodeId entree = graph::NK_NODE_INVALID, retour = graph::NK_NODE_INVALID;
			for (uint32 i = 0; i < g.RawNodeCount(); ++i) {
				const graph::NkNode *n = g.RawNodeAt(i);
				if (n != nullptr && n->alive) {
					if (n->type == NK_BP_FN_ENTREE) {
						entree = n->id;
					} else if (n->type == NK_BP_FN_RETOUR) {
						retour = n->id;
					}
				}
			}
			if (graph::NkNode *r = g.Find(retour)) {
				r->x = 1510.f;
				r->y = 0.f;
			}
			// La mise en page : le FLOT d'execution sur une ligne (en haut), les
			// valeurs dessous, de gauche a droite dans l'ordre ou elles se calculent.
			const graph::NkNodeId nomPorte = NkBpCreerLireVariable(d, g, "porte", 0.f, 170.f);
			const graph::NkNodeId parNom = NkBpCreerNoeud(g, "bp.natif:unkeny.entite.par_nom", 130.f, 150.f);
			const graph::NkNodeId pos = NkBpCreerNoeud(g, "bp.natif:unkeny.transform.position", 370.f, 150.f);
			const graph::NkNodeId vec = NkBpCreerNoeud(g, "bp.math.vec2", 370.f, 290.f);
			const graph::NkNodeId add = NkBpCreerNoeud(g, "bp.math.add_v", 600.f, 220.f);
			const graph::NkNodeId tel = NkBpCreerNoeud(g, "bp.natif:unkeny.transform.teleporter", 830.f, 0.f);
			const graph::NkNodeId eff = NkBpCreerNoeud(g, "bp.natif:unkeny.effet.jouer", 1070.f, 0.f);
			const graph::NkNodeId aff = NkBpCreerNoeud(g, "bp.natif:unkeny.journal.afficher", 240.f, 0.f);
			NkBpPoserDefaut(g, aff, "texte", "La porte s'ouvre (Blueprint)");
			const graph::NkNodeId ecr = NkBpCreerEcrireVariable(d, g, "ouverte", 1290.f, 0.f);
			NkBpPoserDefaut(g, ecr, "valeur", "vrai");
			// La fonction neuve va d'Entree a Retour : on libere « suite » (une
			// sortie d'execution n'a qu'UNE suite).
			for (uint32 i = 0; i < g.LinkCount(); ++i) {
				const graph::NkLink *l = g.LinkAt(i);
				if (l != nullptr && l->alive && l->fromNode == entree) {
					g.Disconnect(l->id);
				}
			}
			g.Connect(entree, "suite", aff, "exec");
			g.Connect(aff, "suite", tel, "exec");
			g.Connect(nomPorte, "valeur", parNom, "nom");
			g.Connect(parNom, "entité", tel, "entité");
			g.Connect(parNom, "entité", pos, "entité");
			g.Connect(entree, "hauteur", vec, "y");
			g.Connect(pos, "position", add, "a");
			g.Connect(vec, "r", add, "b");
			g.Connect(add, "r", tel, "position");
			g.Connect(tel, "suite", eff, "exec");
			g.Connect(parNom, "entité", eff, "entité");
			g.Connect(eff, "suite", ecr, "exec");
			g.Connect(ecr, "suite", retour, "exec");

			// Le graphe d'evenements : Zone entree -> Si (soi est la zone ET l'autre
			// est le Joueur) -> Vrai -> Si (expression) « non ouverte » -> Vrai :
			// OuvrirPorte(hauteur) ; Faux : un noeud Code ecrit « deja ouverte ».
			// Ainsi la branche Faux du code ne parle QUE du Joueur devant une porte
			// deja ouverte (un autre corps qui touche la zone ne dit rien).
			graph::NkNodeGraph &e = d.graphes[0].graphe;
			// La mise en page : le flot en haut, la condition dessous.
			const graph::NkNodeId ev = NkBpCreerNoeud(e, "bp.ev.zone_entree", 0.f, 0.f);
			const graph::NkNodeId nomEst = NkBpCreerNoeud(e, "bp.natif:unkeny.entite.nom_est", 250.f, 150.f);
			NkBpPoserDefaut(e, nomEst, "nom", "Joueur");
			const graph::NkNodeId et = NkBpCreerNoeud(e, "bp.math.et", 500.f, 110.f);
			const graph::NkNodeId si = NkBpCreerNoeud(e, "bp.si", 740.f, 0.f);
			graph::NkNodeId pasOuverte = NkBpCreerNoeudCode(e, NK_BP_SI_EXPRESSION, 970.f, 0.f);
			NkBpCodeRetirerPrise(e, pasOuverte, "a", graph::NkSocketDir::Input);
			NkBpPoserCodeNoeud(e, pasOuverte, "non ouverte");
			const graph::NkNodeId h = NkBpCreerLireVariable(d, e, "hauteur", 1150.f, 250.f);
			const graph::NkNodeId appel = NkBpCreerAppel(d, e, "OuvrirPorte", 1340.f, 0.f);
			e.Connect(ev, "suite", si, "exec");
			e.Connect(ev, "autre", nomEst, "entité");
			e.Connect(ev, "soiEstLaZone", et, "a");
			e.Connect(nomEst, "égal", et, "b");
			e.Connect(et, "r", si, "condition");
			e.Connect(si, "vrai", pasOuverte, "exec");
			e.Connect(pasOuverte, "vrai", appel, "exec");
			e.Connect(h, "valeur", appel, "hauteur");
			// Le cas « deja ouverte », ecrit en code (le noeud « Code » de la reference).
			const graph::NkNodeId rien = NkBpCreerNoeudCode(e, NK_BP_CODE, 1340.f, 140.f);
			NkBpPoserCodeNoeud(e, rien, "afficher(\"Rien à ouvrir : \" + porte + \" est déjà ouverte\")");
			e.Connect(pasOuverte, "faux", rien, "exec");
			// Un cadre, comme on en pose dans UE5 (la charte : option A).
			NkBpCreerCommentaire(e, "Le Joueur entre dans la zone : la porte s'ouvre une fois", -40.f, -70.f, 1750.f, 420.f);
		}

	} // namespace editeur
} // namespace nkentseu
