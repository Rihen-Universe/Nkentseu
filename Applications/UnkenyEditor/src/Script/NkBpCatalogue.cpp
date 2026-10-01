// -----------------------------------------------------------------------------
// FICHIER: UnkenyEditor/Script/NkBpCatalogue.cpp
// DESCRIPTION: Le catalogue des noeuds Blueprint d'Unkeny et le compilateur
//              graphe (NKGraph) -> module de bytecode.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Script/NkBpCatalogue.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		using unkeny::NkOpBp;
		using unkeny::NkTypeBp;

		namespace {
			const char *const kTypes[] = {"exec", "booleen", "entier", "reel", "vec2", "entite", "texte"};

			NkProtoBp Proto(const char *type, const char *libelle, const char *categorie, NkGenreNoeudBp genre) {
				NkProtoBp p;
				p.type = type;
				p.libelle = libelle;
				p.categorie = categorie;
				p.genre = genre;
				return p;
			}
			void E(NkProtoBp &p, const char *nom, const char *type) {
				p.entrees[p.nbEntrees].nom = nom;
				p.entrees[p.nbEntrees].type = type;
				++p.nbEntrees;
			}
			void S(NkProtoBp &p, const char *nom, const char *type) {
				p.sorties[p.nbSorties].nom = nom;
				p.sorties[p.nbSorties].type = type;
				++p.nbSorties;
			}

			NkVector<NkProtoBp> Construire() {
				NkVector<NkProtoBp> v;
				// ── Les evenements : une sortie d'execution, des valeurs ──
				struct Ev {
						const char *type;
						const char *libelle;
						uint32 genre;
				};
				const Ev evs[] = {{"bp.ev.debut", "Événement Début", NK_UNK_EV_DEBUT},
								  {"bp.ev.tick", "Événement Tick (chaque image)", NK_UNK_EV_TICK},
								  {"bp.ev.pas_fixe", "Événement Pas fixe (forces)", NK_UNK_EV_PAS_FIXE},
								  {"bp.ev.contact_debut", "Événement Collision : début", NK_UNK_EV_CONTACT_DEBUT},
								  {"bp.ev.contact_fin", "Événement Collision : fin", NK_UNK_EV_CONTACT_FIN},
								  {"bp.ev.zone_entree", "Événement Déclencheur : entrée", NK_UNK_EV_ZONE_ENTREE},
								  {"bp.ev.zone_sortie", "Événement Déclencheur : sortie", NK_UNK_EV_ZONE_SORTIE},
								  {"bp.ev.action_pressee", "Événement Action pressée", NK_UNK_EV_ACTION_PRESSEE},
								  {"bp.ev.action_relachee", "Événement Action relâchée", NK_UNK_EV_ACTION_RELACHEE}};
				for (const Ev &e : evs) {
					NkProtoBp p = Proto(e.type, e.libelle, "Événements", NkGenreNoeudBp::NK_EVENEMENT);
					p.evenement = e.genre;
					S(p, "suite", "exec");
					if (e.genre == NK_UNK_EV_TICK || e.genre == NK_UNK_EV_PAS_FIXE) {
						S(p, "dt", "reel");
					} else if (e.genre == NK_UNK_EV_CONTACT_DEBUT || e.genre == NK_UNK_EV_CONTACT_FIN) {
						S(p, "autre", "entite");
					} else if (e.genre == NK_UNK_EV_ZONE_ENTREE || e.genre == NK_UNK_EV_ZONE_SORTIE) {
						S(p, "autre", "entite");
						S(p, "soi est la zone", "booleen");
					} else if (e.genre == NK_UNK_EV_ACTION_PRESSEE || e.genre == NK_UNK_EV_ACTION_RELACHEE) {
						E(p, "action", "texte"); // une constante : le NOM de l'action, jamais une touche
						S(p, "valeur", "reel");
					}
					v.PushBack(p);
				}
				// ── Le flot ──
				{
					NkProtoBp p = Proto("bp.si", "Si", "Flot", NkGenreNoeudBp::NK_SI);
					E(p, "exec", "exec");
					E(p, "condition", "booleen");
					S(p, "vrai", "exec");
					S(p, "faux", "exec");
					v.PushBack(p);
					NkProtoBp q = Proto("bp.sequence", "Séquence", "Flot", NkGenreNoeudBp::NK_SEQUENCE);
					E(q, "exec", "exec");
					S(q, "alors 0", "exec");
					S(q, "alors 1", "exec");
					S(q, "alors 2", "exec");
					v.PushBack(q);
				}
				// ── Les variables (sauvees par nom dans le composant, exposees) ──
				struct Var {
						const char *suffixe;
						const char *type;
						NkTypeBp t;
				};
				const Var vars[] = {{"reel", "reel", NkTypeBp::NK_REEL},
									{"entier", "entier", NkTypeBp::NK_ENTIER},
									{"booleen", "booleen", NkTypeBp::NK_BOOLEEN},
									{"vec2", "vec2", NkTypeBp::NK_VEC2}};
				for (const Var &x : vars) {
					NkString t = NkString("bp.var.lire.") + x.suffixe;
					NkProtoBp p = Proto(t.CStr(), NkString::Format("Lire une variable (%s)", x.suffixe).CStr(), "Variables",
										NkGenreNoeudBp::NK_LIRE_VAR);
					p.typeVar = x.t;
					E(p, "nom", "texte");
					S(p, "valeur", x.type);
					v.PushBack(p);
					t = NkString("bp.var.ecrire.") + x.suffixe;
					NkProtoBp q = Proto(t.CStr(), NkString::Format("Écrire une variable (%s)", x.suffixe).CStr(), "Variables",
										NkGenreNoeudBp::NK_ECRIRE_VAR);
					q.typeVar = x.t;
					E(q, "exec", "exec");
					E(q, "nom", "texte");
					E(q, "valeur", x.type);
					S(q, "suite", "exec");
					v.PushBack(q);
				}
				{
					NkProtoBp p = Proto("bp.soi", "Soi", "Entité", NkGenreNoeudBp::NK_SOI);
					S(p, "entité", "entite");
					v.PushBack(p);
				}
				// ── Le calcul ──
				struct Op {
						const char *type;
						const char *libelle;
						NkOpBp op;
						const char *ta;
						const char *tb; ///< nul : un seul operande
						const char *tr;
				};
				const Op ops[] = {{"bp.math.add_r", "+ (réel)", NkOpBp::NK_ADD_R, "reel", "reel", "reel"},
								  {"bp.math.sub_r", "− (réel)", NkOpBp::NK_SUB_R, "reel", "reel", "reel"},
								  {"bp.math.mul_r", "× (réel)", NkOpBp::NK_MUL_R, "reel", "reel", "reel"},
								  {"bp.math.div_r", "÷ (réel)", NkOpBp::NK_DIV_R, "reel", "reel", "reel"},
								  {"bp.math.add_i", "+ (entier)", NkOpBp::NK_ADD_I, "entier", "entier", "entier"},
								  {"bp.math.sub_i", "− (entier)", NkOpBp::NK_SUB_I, "entier", "entier", "entier"},
								  {"bp.math.add_v", "+ (vec2)", NkOpBp::NK_ADD_V, "vec2", "vec2", "vec2"},
								  {"bp.math.sub_v", "− (vec2)", NkOpBp::NK_SUB_V, "vec2", "vec2", "vec2"},
								  {"bp.math.mul_vr", "vec2 × réel", NkOpBp::NK_MUL_VR, "vec2", "reel", "vec2"},
								  {"bp.math.lt_r", "< (réel)", NkOpBp::NK_LT_R, "reel", "reel", "booleen"},
								  {"bp.math.le_r", "≤ (réel)", NkOpBp::NK_LE_R, "reel", "reel", "booleen"},
								  {"bp.math.eq_r", "= (réel)", NkOpBp::NK_EQ_R, "reel", "reel", "booleen"},
								  {"bp.math.eq_i", "= (entier)", NkOpBp::NK_EQ_I, "entier", "entier", "booleen"},
								  {"bp.math.eq_e", "Même entité", NkOpBp::NK_EQ_E, "entite", "entite", "booleen"},
								  {"bp.math.et", "ET", NkOpBp::NK_ET, "booleen", "booleen", "booleen"},
								  {"bp.math.ou", "OU", NkOpBp::NK_OU, "booleen", "booleen", "booleen"},
								  {"bp.math.non", "NON", NkOpBp::NK_NON, "booleen", nullptr, "booleen"},
								  {"bp.math.vec2", "Construire un vec2", NkOpBp::NK_VEC2, "reel", "reel", "vec2"},
								  {"bp.math.vx", "X d'un vec2", NkOpBp::NK_VX, "vec2", nullptr, "reel"},
								  {"bp.math.vy", "Y d'un vec2", NkOpBp::NK_VY, "vec2", nullptr, "reel"},
								  {"bp.math.longueur", "Longueur", NkOpBp::NK_LONGUEUR, "vec2", nullptr, "reel"},
								  {"bp.math.normaliser", "Normaliser", NkOpBp::NK_NORMALISER, "vec2", nullptr, "vec2"}};
				for (const Op &o : ops) {
					NkProtoBp p = Proto(o.type, o.libelle, "Calcul", NkGenreNoeudBp::NK_MATH);
					p.op = o.op;
					const bool vec = o.op == NkOpBp::NK_VEC2;
					E(p, vec ? "x" : "a", o.ta);
					if (o.tb != nullptr) {
						E(p, vec ? "y" : "b", o.tb);
					}
					S(p, "r", o.tr);
					v.PushBack(p);
				}
				// ── Un noeud par NATIF de la machine : la table est la verite ──
				uint32 n = 0;
				const unkeny::NkNatifBp *natifs = unkeny::NkNatifsBp(n);
				for (uint32 i = 0; i < n; ++i) {
					const unkeny::NkNatifBp &x = natifs[i];
					NkProtoBp p = Proto((NkString("bp.natif:") + x.signature.nom).CStr(), x.libelle, x.categorie, NkGenreNoeudBp::NK_NATIF);
					p.natif = static_cast<int32>(i);
					if (!x.pur) {
						E(p, "exec", "exec");
					}
					for (uint8 k = 0; k < x.signature.nbParams; ++k) {
						E(p, x.nomsParams[k], unkeny::NkNomTypeBp(x.signature.params[k]));
					}
					if (!x.pur) {
						S(p, "suite", "exec");
					}
					for (uint8 k = 0; k < x.signature.nbResultats; ++k) {
						S(p, x.nomsResultats[k], unkeny::NkNomTypeBp(x.signature.resultats[k]));
					}
					if (x.signature.evenements == unkeny::NK_BP_PAS_FIXE_SEUL) {
						p.aide = "Seulement sous « Pas fixe » : une force ne vit qu'un pas.";
					}
					v.PushBack(p);
				}
				return v;
			}
		} // namespace

		const NkVector<NkProtoBp> &NkBpProtos() {
			static const NkVector<NkProtoBp> v = Construire();
			return v;
		}

		const NkProtoBp *NkBpProto(const char *type) {
			const NkVector<NkProtoBp> &v = NkBpProtos();
			for (uint32 i = 0; type != nullptr && i < v.Size(); ++i) {
				if (std::strcmp(v[i].type.CStr(), type) == 0) {
					return &v[i];
				}
			}
			return nullptr;
		}

		NkTypeBp NkBpTypeDeNom(const char *nom) noexcept {
			if (nom == nullptr) {
				return NkTypeBp::NK_RIEN;
			}
			for (uint32 k = 1; k < sizeof(kTypes) / sizeof(kTypes[0]); ++k) {
				if (std::strcmp(nom, kTypes[k]) == 0) {
					return static_cast<NkTypeBp>(k); // meme ordre que NkTypeBp (booleen = 1...)
				}
			}
			return NkTypeBp::NK_RIEN;
		}

		void NkBpEnregistrerTypes(graph::NkNodeGraph &g) {
			for (const char *t : kTypes) {
				if (g.FindType(t) == graph::NK_TYPE_INVALID) {
					g.RegisterType(t);
				}
			}
			// La SEULE conversion implicite : entier -> reel (exacte jusqu'a 2^24).
			// Reel -> entier, non : trois arrondis plausibles, le meme refus que
			// les materiaux.
			g.AllowConversion(g.FindType("entier"), g.FindType("reel"));
		}

		graph::NkNodeId NkBpCreerNoeud(graph::NkNodeGraph &g, const char *type, float32 x, float32 y) {
			const NkProtoBp *p = NkBpProto(type);
			if (p == nullptr) {
				return graph::NK_NODE_INVALID;
			}
			NkBpEnregistrerTypes(g);
			const graph::NkNodeId n = g.AddNode(p->type.CStr(), p->libelle.CStr());
			for (uint8 k = 0; k < p->nbEntrees; ++k) {
				const bool exec = std::strcmp(p->entrees[k].type, "exec") == 0;
				g.AddSocket(n, p->entrees[k].nom, g.FindType(p->entrees[k].type), graph::NkSocketDir::Input,
							exec ? graph::NkSocketFamily::Exec : graph::NkSocketFamily::Data);
			}
			for (uint8 k = 0; k < p->nbSorties; ++k) {
				const bool exec = std::strcmp(p->sorties[k].type, "exec") == 0;
				g.AddSocket(n, p->sorties[k].nom, g.FindType(p->sorties[k].type), graph::NkSocketDir::Output,
							exec ? graph::NkSocketFamily::Exec : graph::NkSocketFamily::Data);
			}
			if (graph::NkNode *node = g.Find(n)) {
				node->x = x;
				node->y = y;
			}
			return n;
		}

		bool NkBpPoserDefaut(graph::NkNodeGraph &g, graph::NkNodeId n, const char *prise, const char *texte) {
			const graph::NkNode *node = g.Find(n);
			if (node == nullptr || texte == nullptr) {
				return false;
			}
			const int32 k = node->FindSocket(prise, graph::NkSocketDir::Input);
			if (k < 0) {
				return false;
			}
			const graph::NkSocket &s = node->sockets[static_cast<uint32>(k)];
			const NkString *nomType = g.TypeName(s.type);
			const NkTypeBp t = NkBpTypeDeNom(nomType != nullptr ? nomType->CStr() : nullptr);
			graph::NkGraphValue v;
			v.type = s.type;
			char *fin = nullptr;
			switch (t) {
				case NkTypeBp::NK_TEXTE:
					v.text = texte;
					break;
				case NkTypeBp::NK_BOOLEEN: {
					const bool vrai = std::strcmp(texte, "vrai") == 0 || std::strcmp(texte, "1") == 0 ||
									  std::strcmp(texte, "true") == 0 || std::strcmp(texte, "oui") == 0;
					const bool faux = std::strcmp(texte, "faux") == 0 || std::strcmp(texte, "0") == 0 ||
									  std::strcmp(texte, "false") == 0 || std::strcmp(texte, "non") == 0;
					if (!vrai && !faux) {
						return false;
					}
					v.numbers.PushBack(vrai ? 1.f : 0.f);
					break;
				}
				case NkTypeBp::NK_ENTIER:
				case NkTypeBp::NK_REEL: {
					const float32 x = std::strtof(texte, &fin);
					if (fin == texte) {
						return false;
					}
					v.numbers.PushBack(t == NkTypeBp::NK_ENTIER ? static_cast<float32>(static_cast<int32>(x)) : x);
					break;
				}
				case NkTypeBp::NK_VEC2: {
					const float32 x = std::strtof(texte, &fin);
					if (fin == texte) {
						return false;
					}
					const char *a = fin;
					while (*a == ' ' || *a == ',' || *a == ';') {
						++a;
					}
					const float32 y = std::strtof(a, &fin);
					v.numbers.PushBack(x);
					v.numbers.PushBack(fin == a ? 0.f : y);
					break;
				}
				default:
					return false; // entite, exec : pas de valeur saisie
			}
			return g.SetSocketDefault(n, prise, graph::NkSocketDir::Input, v);
		}

		NkString NkBpTexteDefaut(const graph::NkNodeGraph &g, const graph::NkNode &n, const graph::NkSocket &s) {
			(void)n;
			const NkString *nomType = g.TypeName(s.type);
			const NkTypeBp t = NkBpTypeDeNom(nomType != nullptr ? nomType->CStr() : nullptr);
			const graph::NkGraphValue &v = s.defaultValue;
			auto num = [&](uint32 i) {
				return v.IsSet() && i < v.numbers.Size() ? v.numbers[i] : 0.f;
			};
			switch (t) {
				case NkTypeBp::NK_TEXTE:
					return v.IsSet() ? v.text : NkString();
				case NkTypeBp::NK_BOOLEEN:
					return NkString(num(0) != 0.f ? "vrai" : "faux");
				case NkTypeBp::NK_ENTIER:
					return NkString::Format("%d", static_cast<int>(num(0)));
				case NkTypeBp::NK_REEL:
					return NkString::Format("%g", static_cast<double>(num(0)));
				case NkTypeBp::NK_VEC2:
					return NkString::Format("%g %g", static_cast<double>(num(0)), static_cast<double>(num(1)));
				case NkTypeBp::NK_ENTITE:
					return NkString("soi");
				default:
					return NkString();
			}
		}

		// =====================================================================
		// Le compilateur
		// =====================================================================
		namespace {
			struct NkCompilateur {
					const graph::NkNodeGraph &g;
					unkeny::NkAssembleurBp a;
					NkErreurBp &err;
					graph::NkNodeId evenement = graph::NK_NODE_INVALID;
					bool ok = true;

					NkCompilateur(const graph::NkNodeGraph &graphe, NkErreurBp &e) : g(graphe), err(e) {
					}

					uint32 Echec(graph::NkNodeId n, const NkString &message) {
						if (ok) {
							err.message = message;
							err.noeud = n;
						}
						ok = false;
						return 0u;
					}
					const NkProtoBp *ProtoDe(const graph::NkNode &n) {
						return NkBpProto(n.type.CStr());
					}
					NkTypeBp TypePrise(const graph::NkSocket &s) {
						const NkString *t = g.TypeName(s.type);
						return NkBpTypeDeNom(t != nullptr ? t->CStr() : nullptr);
					}
					/// Le lien qui PART de la sortie `k` de `n` (une sortie exec n'en a qu'un).
					const graph::NkLink *Sortant(graph::NkNodeId n, int32 k) {
						for (uint32 i = 0; i < g.LinkCount(); ++i) {
							const graph::NkLink *l = g.LinkAt(i);
							if (l != nullptr && l->alive && l->fromNode == n && l->fromSocket == k) {
								return l;
							}
						}
						return nullptr;
					}
					/// Le texte CONSTANT d'une entree (le nom d'une variable, d'une action).
					NkString Constante(const graph::NkNode &n, const char *entree) {
						const int32 k = n.FindSocket(entree, graph::NkSocketDir::Input);
						if (k < 0) {
							return NkString();
						}
						if (g.IncomingOf(n.id, k) != nullptr) {
							Echec(n.id, NkString::Format("« %s » doit être une constante (saisie sur le nœud), pas un fil", entree));
							return NkString();
						}
						const graph::NkGraphValue &v = n.sockets[static_cast<uint32>(k)].defaultValue;
						return v.IsSet() ? v.text : NkString();
					}
					uint32 Variable(const graph::NkNode &n, const NkProtoBp &p) {
						const NkString nom = Constante(n, "nom");
						if (!ok) {
							return 0u;
						}
						if (nom.Empty()) {
							return Echec(n.id, NkString("variable sans nom : saisir « nom » sur le nœud"));
						}
						if (nom.Length() >= unkeny::NK_UNKENY_VAR_NOM_MAX) {
							return Echec(n.id, NkString::Format("nom de variable trop long (%u caractères au plus)",
																 static_cast<unsigned>(unkeny::NK_UNKENY_VAR_NOM_MAX - 1u)));
						}
						for (uint32 i = 0; i < a.module.variables.Size(); ++i) {
							if (a.module.variables[i].nom == nom && a.module.variables[i].type != p.typeVar) {
								return Echec(n.id, NkString::Format("la variable « %s » a déjà un autre type", nom.CStr()));
							}
						}
						if (a.module.variables.Size() >= unkeny::NK_UNKENY_SCRIPT_VARS_MAX) {
							bool connue = false;
							for (uint32 i = 0; i < a.module.variables.Size(); ++i) {
								connue = connue || a.module.variables[i].nom == nom;
							}
							if (!connue) {
								return Echec(n.id, NkString("trop de variables pour un script"));
							}
						}
						return a.Variable(nom.CStr(), p.typeVar, unkeny::NkValeurBp());
					}

					/// La valeur de l'entree `entree` de `n`, convertie en `attendu`.
					uint32 Valeur(const graph::NkNode &n, const char *entree, NkTypeBp attendu, int32 profondeur) {
						const int32 k = n.FindSocket(entree, graph::NkSocketDir::Input);
						if (k < 0) {
							return Echec(n.id, NkString::Format("prise « %s » absente (graphe d'une autre version ?)", entree));
						}
						const graph::NkLink *l = g.IncomingOf(n.id, k);
						if (l != nullptr) {
							const graph::NkNode *src = g.Find(l->fromNode);
							if (src == nullptr || l->fromSocket < 0 || static_cast<uint32>(l->fromSocket) >= src->sockets.Size()) {
								return Echec(n.id, NkString("fil vers un nœud absent"));
							}
							const uint32 r = Sortie(*src, l->fromSocket, profondeur + 1);
							const NkTypeBp t = TypePrise(src->sockets[static_cast<uint32>(l->fromSocket)]);
							if (!ok || t == attendu) {
								return r;
							}
							if (t == NkTypeBp::NK_ENTIER && attendu == NkTypeBp::NK_REEL) {
								const uint32 c = a.Registre(NkTypeBp::NK_REEL);
								a.Emettre(NkOpBp::NK_I2R, c, r);
								return c;
							}
							return Echec(n.id, NkString::Format("« %s » attend un %s", entree, unkeny::NkNomTypeBp(attendu)));
						}
						// Une entree LIBRE : sa valeur saisie ; une entite libre vaut SOI.
						const graph::NkGraphValue &v = n.sockets[static_cast<uint32>(k)].defaultValue;
						auto num = [&](uint32 i) {
							return v.IsSet() && i < v.numbers.Size() ? v.numbers[i] : 0.f;
						};
						const uint32 r = a.Registre(attendu);
						switch (attendu) {
							case NkTypeBp::NK_ENTITE:
								a.Emettre(NkOpBp::NK_SOI, r);
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
							default:
								return Echec(n.id, NkString::Format("« %s » : type sans valeur", entree));
						}
						return r;
					}

					/// La sortie `k` d'un noeud PUR (ou d'un evenement), calculee ici.
					uint32 Sortie(const graph::NkNode &src, int32 k, int32 profondeur) {
						if (profondeur > 256) {
							return Echec(src.id, NkString("graphe trop profond"));
						}
						const NkProtoBp *p = ProtoDe(src);
						if (p == nullptr) {
							return Echec(src.id, NkString::Format("nœud inconnu de ce catalogue : %s", src.type.CStr()));
						}
						const graph::NkSocket &s = src.sockets[static_cast<uint32>(k)];
						const NkTypeBp t = TypePrise(s);
						a.Ligne(src.id);
						switch (p->genre) {
							case NkGenreNoeudBp::NK_EVENEMENT: {
								if (src.id != evenement) {
									return Echec(src.id, NkString("valeur d'un AUTRE événement : chaque événement a les siennes"));
								}
								unkeny::NkArgBp arg = unkeny::NkArgBp::NK_DT;
								if (s.name == "autre") {
									arg = unkeny::NkArgBp::NK_AUTRE;
								} else if (s.name == "soi est la zone") {
									arg = unkeny::NkArgBp::NK_SOI_EST_ZONE;
								} else if (s.name == "valeur") {
									arg = unkeny::NkArgBp::NK_VALEUR;
								}
								const uint32 r = a.Registre(t);
								a.Emettre(NkOpBp::NK_ARG, r, static_cast<uint32>(arg));
								return r;
							}
							case NkGenreNoeudBp::NK_SOI: {
								const uint32 r = a.Registre(NkTypeBp::NK_ENTITE);
								a.Emettre(NkOpBp::NK_SOI, r);
								return r;
							}
							case NkGenreNoeudBp::NK_LIRE_VAR: {
								const uint32 v = Variable(src, *p);
								if (!ok) {
									return 0u;
								}
								const uint32 r = a.Registre(p->typeVar);
								a.Emettre(NkOpBp::NK_LIRE_VAR, r, v);
								return r;
							}
							case NkGenreNoeudBp::NK_MATH: {
								const uint32 ra = Valeur(src, p->entrees[0].nom, NkBpTypeDeNom(p->entrees[0].type), profondeur);
								const uint32 rb = p->nbEntrees > 1 ? Valeur(src, p->entrees[1].nom, NkBpTypeDeNom(p->entrees[1].type), profondeur) : 0u;
								if (!ok) {
									return 0u;
								}
								const uint32 r = a.Registre(NkBpTypeDeNom(p->sorties[0].type));
								a.Ligne(src.id);
								if (p->nbEntrees > 1) {
									a.Emettre(p->op, r, ra, rb);
								} else {
									a.Emettre(p->op, r, ra);
								}
								return r;
							}
							case NkGenreNoeudBp::NK_NATIF: {
								uint32 nb = 0;
								const unkeny::NkNatifBp &x = unkeny::NkNatifsBp(nb)[p->natif];
								if (!x.pur) {
									return Echec(src.id, NkString("valeur d'un nœud d'exécution : non disponible dans ce premier lot"));
								}
								return Natif(src, *p, x, k, profondeur);
							}
							default:
								return Echec(src.id, NkString("ce nœud ne produit pas de valeur"));
						}
					}

					/// Un natif : parametres evalues, l'appel, les resultats ; rend le
					/// registre du resultat `sortie` (-1 : aucun).
					uint32 Natif(const graph::NkNode &n, const NkProtoBp &p, const unkeny::NkNatifBp &x, int32 sortie, int32 profondeur) {
						const int32 imp = a.Import(x.signature.nom);
						if (imp < 0) {
							return Echec(n.id, NkString::Format("natif inconnu : %s", x.signature.nom));
						}
						uint32 regs[6] = {};
						uint32 nr = 0;
						for (uint8 k = 0; k < x.signature.nbParams && ok; ++k) {
							regs[nr++] = Valeur(n, x.nomsParams[k], x.signature.params[k], profondeur);
						}
						uint32 resultat = 0u;
						for (uint8 k = 0; k < x.signature.nbResultats && ok; ++k) {
							const uint32 r = a.Registre(x.signature.resultats[k]);
							regs[nr++] = r;
							// La sortie demandee : par NOM (les sorties d'un natif pur sont
							// ses resultats, dans l'ordre).
							const int32 ks = n.FindSocket(x.nomsResultats[k], graph::NkSocketDir::Output);
							if (ks == sortie) {
								resultat = r;
							}
						}
						if (!ok) {
							return 0u;
						}
						a.Ligne(n.id);
						a.Natif(static_cast<uint32>(imp), regs, nr);
						(void)p;
						return resultat;
					}

					void Chaine(const graph::NkNode &n, const char *sortieExec, int32 profondeur) {
						const int32 k = n.FindSocket(sortieExec, graph::NkSocketDir::Output);
						if (k < 0 || !ok) {
							return;
						}
						const graph::NkLink *l = Sortant(n.id, k);
						if (l == nullptr) {
							return;
						}
						const graph::NkNode *suivant = g.Find(l->toNode);
						if (suivant == nullptr) {
							Echec(n.id, NkString("fil d'exécution vers un nœud absent"));
							return;
						}
						Noeud(*suivant, profondeur + 1);
					}

					void Noeud(const graph::NkNode &n, int32 profondeur) {
						if (profondeur > 256) {
							Echec(n.id, NkString("chaîne d'exécution trop longue"));
							return;
						}
						const NkProtoBp *p = ProtoDe(n);
						if (p == nullptr) {
							Echec(n.id, NkString::Format("nœud inconnu de ce catalogue : %s", n.type.CStr()));
							return;
						}
						a.Ligne(n.id);
						switch (p->genre) {
							case NkGenreNoeudBp::NK_SI: {
								const uint32 c = Valeur(n, "condition", NkTypeBp::NK_BOOLEEN, profondeur);
								if (!ok) {
									return;
								}
								a.Ligne(n.id);
								a.Emettre(NkOpBp::NK_SAUT_SI_FAUX, c, 0u);
								const uint32 versFaux = a.Pc() - 1u;
								Chaine(n, "vrai", profondeur);
								a.Emettre(NkOpBp::NK_SAUT, 0u);
								const uint32 versFin = a.Pc() - 1u;
								a.Patcher(versFaux, a.Pc());
								Chaine(n, "faux", profondeur);
								a.Patcher(versFin, a.Pc());
								return;
							}
							case NkGenreNoeudBp::NK_SEQUENCE:
								for (uint8 k = 0; k < p->nbSorties && ok; ++k) {
									Chaine(n, p->sorties[k].nom, profondeur);
								}
								return;
							case NkGenreNoeudBp::NK_ECRIRE_VAR: {
								const uint32 v = Variable(n, *p);
								const uint32 r = ok ? Valeur(n, "valeur", p->typeVar, profondeur) : 0u;
								if (!ok) {
									return;
								}
								a.Ligne(n.id);
								a.Emettre(NkOpBp::NK_ECRIRE_VAR, v, r);
								Chaine(n, "suite", profondeur);
								return;
							}
							case NkGenreNoeudBp::NK_NATIF: {
								uint32 nb = 0;
								const unkeny::NkNatifBp &x = unkeny::NkNatifsBp(nb)[p->natif];
								Natif(n, *p, x, -1, profondeur);
								Chaine(n, "suite", profondeur);
								return;
							}
							default:
								Echec(n.id, NkString("ce nœud ne s'exécute pas : relier un nœud d'exécution"));
								return;
						}
					}
			};
		} // namespace

		bool NkBpCompiler(const graph::NkNodeGraph &g, unkeny::NkModuleBp &sortie, NkErreurBp &erreur) {
			erreur = NkErreurBp();
			// ── 1. Le graphe est-il SAIN ? (Validate connait la famille : G1) ──
			NkVector<graph::NkGraphDiag> diags;
			if (g.Validate(diags) > 0u) {
				erreur.message = NkString::Format("graphe invalide : %s %s", graph::NkGraphIssueName(diags[0].issue), diags[0].detail.CStr());
				erreur.noeud = diags[0].node;
				return false;
			}
			NkCompilateur c(g, erreur);
			uint32 evenements = 0;
			// ── 2. Une fonction par EVENEMENT, en marchant le long des fils exec ──
			for (uint32 i = 0; i < g.RawNodeCount() && c.ok; ++i) {
				const graph::NkNode *n = g.RawNodeAt(i);
				if (n == nullptr || !n->alive) {
					continue;
				}
				const NkProtoBp *p = NkBpProto(n->type.CStr());
				if (p == nullptr) {
					erreur.message = NkString::Format("nœud inconnu de ce catalogue : %s", n->type.CStr());
					erreur.noeud = n->id;
					return false;
				}
				if (p->genre != NkGenreNoeudBp::NK_EVENEMENT) {
					continue;
				}
				++evenements;
				c.evenement = n->id;
				const uint32 f = c.a.Fonction(p->libelle.CStr());
				NkString parametre;
				if (p->evenement == NK_UNK_EV_ACTION_PRESSEE || p->evenement == NK_UNK_EV_ACTION_RELACHEE) {
					parametre = c.Constante(*n, "action");
					if (c.ok && parametre.Empty()) {
						c.Echec(n->id, NkString("événement d'action : saisir le NOM de l'action (« Sauter »...)"));
					}
				}
				c.a.Entree(p->evenement, f, parametre.CStr());
				c.a.Ligne(n->id);
				c.Chaine(*n, "suite", 0);
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
			g.Serialize(texte);
			c.a.module.empreinte = unkeny::NkEmpreinteBp(texte.CStr(), texte.Length());
			// ── 3. Verifie COMME LE JEU LE VERIFIERA (natif hors evenement...) ──
			unkeny::NkProgrammeBp p;
			p.module = c.a.module;
			unkeny::NkRefusBp refus;
			if (!unkeny::NkPreparerProgrammeBp(p, refus)) {
				erreur.message = refus.raison;
				erreur.noeud = refus.noeud;
				return false;
			}
			sortie = c.a.module;
			return true;
		}

		bool NkBpEnregistrer(const char *chemin, const graph::NkNodeGraph &g, NkErreurBp &erreur, unkeny::NkModuleBp *module) {
			unkeny::NkModuleBp m;
			const bool ok = NkBpCompiler(g, m, erreur);
			NkString texte;
			g.Serialize(texte);
			NkString err;
			if (!unkeny::NkEcrireFichierBp(chemin, texte, ok ? &m : nullptr, &err)) {
				erreur.message = NkString::Format("écriture impossible : %s", err.CStr());
				erreur.noeud = graph::NK_NODE_INVALID;
				return false;
			}
			if (ok && module != nullptr) {
				*module = m;
			}
			return ok;
		}

		bool NkBpOuvrir(const char *chemin, graph::NkNodeGraph &g, NkString *erreur) {
			NkString texte;
			if (!unkeny::NkLireFichierBp(chemin, &texte, nullptr, nullptr, erreur)) {
				return false;
			}
			if (texte.Empty()) {
				g.Clear();
				NkBpEnregistrerTypes(g);
				return true; // un module sans graphe (livraison) : rien a editer
			}
			if (!g.Deserialize(texte.CStr(), erreur)) {
				return false;
			}
			NkBpEnregistrerTypes(g);
			return true;
		}

		// =====================================================================
		// Des graphes construits PAR CODE
		// =====================================================================
		namespace {
			void Fil(graph::NkNodeGraph &g, graph::NkNodeId a, const char *sa, graph::NkNodeId b, const char *sb) {
				g.Connect(a, sa, b, sb);
			}
		} // namespace

		void NkBpGrapheBonjour(graph::NkNodeGraph &g) {
			g.Clear();
			NkBpEnregistrerTypes(g);
			const graph::NkNodeId ev = NkBpCreerNoeud(g, "bp.ev.debut", 0.f, 0.f);
			const graph::NkNodeId aff = NkBpCreerNoeud(g, "bp.natif:unkeny.journal.afficher", 260.f, 0.f);
			NkBpPoserDefaut(g, aff, "texte", "Bonjour depuis un Blueprint");
			Fil(g, ev, "suite", aff, "exec");
		}

		void NkBpGraphePorte(graph::NkNodeGraph &g, const char *porte) {
			g.Clear();
			NkBpEnregistrerTypes(g);
			const graph::NkNodeId ev = NkBpCreerNoeud(g, "bp.ev.zone_entree", 0.f, 0.f);
			const graph::NkNodeId nomEst = NkBpCreerNoeud(g, "bp.natif:unkeny.entite.nom_est", 40.f, 170.f);
			NkBpPoserDefaut(g, nomEst, "nom", "Joueur");
			const graph::NkNodeId et = NkBpCreerNoeud(g, "bp.math.et", 300.f, 120.f);
			// « ouverte » : une variable du Blueprint (montree et sauvee dans les Details).
			const graph::NkNodeId lire = NkBpCreerNoeud(g, "bp.var.lire.booleen", 40.f, 320.f);
			NkBpPoserDefaut(g, lire, "nom", "ouverte");
			const graph::NkNodeId non = NkBpCreerNoeud(g, "bp.math.non", 300.f, 320.f);
			const graph::NkNodeId et2 = NkBpCreerNoeud(g, "bp.math.et", 420.f, 220.f);
			const graph::NkNodeId si = NkBpCreerNoeud(g, "bp.si", 640.f, 0.f);
			const graph::NkNodeId parNom = NkBpCreerNoeud(g, "bp.natif:unkeny.entite.par_nom", 640.f, 260.f);
			NkBpPoserDefaut(g, parNom, "nom", porte != nullptr ? porte : "Porte");
			const graph::NkNodeId pos = NkBpCreerNoeud(g, "bp.natif:unkeny.transform.position", 900.f, 300.f);
			const graph::NkNodeId add = NkBpCreerNoeud(g, "bp.math.add_v", 1140.f, 280.f);
			NkBpPoserDefaut(g, add, "b", "0 2");
			const graph::NkNodeId tel = NkBpCreerNoeud(g, "bp.natif:unkeny.transform.teleporter", 1380.f, 0.f);
			const graph::NkNodeId eff = NkBpCreerNoeud(g, "bp.natif:unkeny.effet.jouer", 1640.f, 0.f);
			const graph::NkNodeId aff = NkBpCreerNoeud(g, "bp.natif:unkeny.journal.afficher", 1900.f, 0.f);
			NkBpPoserDefaut(g, aff, "texte", "La porte s'ouvre (Blueprint)");
			const graph::NkNodeId ecrire = NkBpCreerNoeud(g, "bp.var.ecrire.booleen", 2160.f, 0.f);
			NkBpPoserDefaut(g, ecrire, "nom", "ouverte");
			NkBpPoserDefaut(g, ecrire, "valeur", "vrai");
			Fil(g, ev, "suite", si, "exec");
			Fil(g, ev, "autre", nomEst, "entité");
			Fil(g, ev, "soi est la zone", et, "a");
			Fil(g, nomEst, "égal", et, "b");
			Fil(g, lire, "valeur", non, "a");
			Fil(g, et, "r", et2, "a");
			Fil(g, non, "r", et2, "b");
			Fil(g, et2, "r", si, "condition");
			Fil(g, aff, "suite", ecrire, "exec");
			Fil(g, si, "vrai", tel, "exec");
			Fil(g, parNom, "entité", pos, "entité");
			Fil(g, pos, "position", add, "a");
			Fil(g, parNom, "entité", tel, "entité");
			Fil(g, add, "r", tel, "position");
			Fil(g, tel, "suite", eff, "exec");
			Fil(g, parNom, "entité", eff, "entité");
			Fil(g, eff, "suite", aff, "exec");
		}

	} // namespace editeur
} // namespace nkentseu
