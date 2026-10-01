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
			// L'ORDRE est celui de NkTypeBp (booleen = 1...) jusqu'a « couleur » ;
			// « asset » (une reference d'asset) est un TEXTE pour la machine.
			const char *const kTypes[] = {"exec", "booleen", "entier", "reel", "vec2", "entite", "texte", "couleur", "asset"};

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
						S(p, "soiEstLaZone", "booleen");
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
					S(q, "alors0", "exec");
					S(q, "alors1", "exec");
					S(q, "alors2", "exec");
					v.PushBack(q);
					// (2026-10-01) Les boucles sont des NOEUDS (jamais un fil qui
					// revient) ; le budget de la machine coupe une boucle sans fin.
					NkProtoBp b = Proto("bp.pour", "Boucle Pour", "Flot", NkGenreNoeudBp::NK_POUR);
					E(b, "exec", "exec");
					E(b, "premier", "entier");
					E(b, "dernier", "entier");
					S(b, "corps", "exec");
					S(b, "indice", "entier");
					S(b, "fini", "exec");
					b.aide = "Exécute « corps » pour chaque indice de premier à dernier (compris), puis « fini ».";
					v.PushBack(b);
					NkProtoBp t = Proto("bp.tant_que", "Tant que", "Flot", NkGenreNoeudBp::NK_TANT_QUE);
					E(t, "exec", "exec");
					E(t, "condition", "booleen");
					S(t, "corps", "exec");
					S(t, "fini", "exec");
					t.aide = "Exécute « corps » tant que la condition est vraie (relue à chaque tour).";
					v.PushBack(t);
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
								  {"bp.math.mul_i", "× (entier)", NkOpBp::NK_MUL_I, "entier", "entier", "entier"},
								  {"bp.math.div_i", "÷ (entier)", NkOpBp::NK_DIV_I, "entier", "entier", "entier"},
								  {"bp.math.lt_i", "< (entier)", NkOpBp::NK_LT_I, "entier", "entier", "booleen"},
								  {"bp.math.le_i", "≤ (entier)", NkOpBp::NK_LE_I, "entier", "entier", "booleen"},
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
			if (std::strcmp(nom, "asset") == 0) {
				return NkTypeBp::NK_TEXTE; // une reference d'asset : son chemin
			}
			for (uint32 k = 1; k <= static_cast<uint32>(unkeny::NK_BP_TYPE_DERNIER); ++k) {
				if (std::strcmp(nom, kTypes[k]) == 0) {
					return static_cast<NkTypeBp>(k); // meme ordre que NkTypeBp (booleen = 1...)
				}
			}
			return NkTypeBp::NK_RIEN;
		}

		NkGenreNoeudBp NkBpGenre(const graph::NkNode &n) noexcept {
			struct G {
					const char *cle;
					NkGenreNoeudBp genre;
			};
			static const G k[] = {{NK_BP_VAR_LIRE, NkGenreNoeudBp::NK_VAR_GET},
								  {NK_BP_VAR_ECRIRE, NkGenreNoeudBp::NK_VAR_SET},
								  {NK_BP_FN_ENTREE, NkGenreNoeudBp::NK_FN_ENTREE},
								  {NK_BP_FN_RETOUR, NkGenreNoeudBp::NK_FN_RETOUR},
								  {NK_BP_APPEL, NkGenreNoeudBp::NK_APPEL_FN},
								  {NK_BP_MACRO, NkGenreNoeudBp::NK_MACRO},
								  {NK_BP_MACRO_ENTREE, NkGenreNoeudBp::NK_MACRO_ENTREE},
								  {NK_BP_MACRO_SORTIE, NkGenreNoeudBp::NK_MACRO_SORTIE},
								  {NK_BP_REP_APPELER, NkGenreNoeudBp::NK_REP_APPELER},
								  {NK_BP_REP_EVENEMENT, NkGenreNoeudBp::NK_REP_EVENEMENT},
								  {NK_BP_RELAIS, NkGenreNoeudBp::NK_RELAIS},
								  {NK_BP_COMMENTAIRE, NkGenreNoeudBp::NK_COMMENTAIRE}};
			for (const G &g : k) {
				if (n.type == g.cle) {
					return g.genre;
				}
			}
			const NkProtoBp *p = NkBpProto(n.type.CStr());
			return p != nullptr ? p->genre : NkGenreNoeudBp::NK_INCONNU;
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
			// Une reference d'asset EST un texte (son chemin), dans les deux sens.
			g.AllowConversion(g.FindType("asset"), g.FindType("texte"));
			g.AllowConversion(g.FindType("texte"), g.FindType("asset"));
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
				case NkTypeBp::NK_COULEUR: {
					uint32 rvba = 0u;
					if (!NkBpLireCouleur(texte, rvba)) {
						return false;
					}
					v.text = NkBpTexteCouleur(rvba);
					break;
				}
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
				case NkTypeBp::NK_COULEUR:
					return v.IsSet() && !v.text.Empty() ? v.text : NkString("#FFFFFFFF");
				default:
					return NkString();
			}
		}

		// =====================================================================
		// Le compilateur : NkBpCompilateur.cpp (le DOCUMENT). Un graphe seul est
		// un document d'un seul graphe, sans declaration.
		// =====================================================================
		bool NkBpCompiler(const graph::NkNodeGraph &g, unkeny::NkModuleBp &sortie, NkErreurBp &erreur) {
			NkDocumentBp d;
			d.graphes[0].graphe = g;
			NkBpEnregistrerTypes(d.graphes[0].graphe);
			return NkBpCompilerDocument(d, sortie, erreur);
		}

		bool NkBpEnregistrer(const char *chemin, const graph::NkNodeGraph &g, NkErreurBp &erreur, unkeny::NkModuleBp *module) {
			NkDocumentBp d;
			d.graphes[0].graphe = g;
			NkBpEnregistrerTypes(d.graphes[0].graphe);
			return NkBpEnregistrerDocument(chemin, d, erreur, module);
		}

		bool NkBpEnregistrerDocument(const char *chemin, const NkDocumentBp &d, NkErreurBp &erreur, unkeny::NkModuleBp *module) {
			unkeny::NkModuleBp m;
			const bool ok = NkBpCompilerDocument(d, m, erreur);
			NkString texte;
			d.graphes[0].graphe.Serialize(texte);
			// DOCU seulement s'il y a quelque chose a dire : un Blueprint sans
			// declaration reste un fichier de la forme d'avant le 01/10.
			NkString doc;
			const bool aDoc = !d.variables.Empty() || d.graphes.Size() > 1u || !d.repartiteurs.Empty() || !d.description.Empty() ||
							  !d.categorie.Empty();
			if (aDoc) {
				NkBpEcrireDocument(d, doc);
			}
			NkString err;
			if (!unkeny::NkEcrireFichierBp(chemin, texte, ok ? &m : nullptr, &err, aDoc ? &doc : nullptr)) {
				erreur.message = NkString::Format("écriture impossible : %s", err.CStr());
				erreur.noeud = graph::NK_NODE_INVALID;
				erreur.graphe = 0u;
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
			// Quatre colonnes : l'evenement et ses valeurs, la condition, le Si et
			// la porte, les gestes (de haut en bas, dans l'ordre d'execution).
			const graph::NkNodeId ev = NkBpCreerNoeud(g, "bp.ev.zone_entree", 0.f, 0.f);
			const graph::NkNodeId nomEst = NkBpCreerNoeud(g, "bp.natif:unkeny.entite.nom_est", 0.f, 130.f);
			NkBpPoserDefaut(g, nomEst, "nom", "Joueur");
			// « ouverte » : une variable du Blueprint (montree et sauvee dans les Details).
			const graph::NkNodeId lire = NkBpCreerNoeud(g, "bp.var.lire.booleen", 0.f, 260.f);
			NkBpPoserDefaut(g, lire, "nom", "ouverte");
			const graph::NkNodeId et = NkBpCreerNoeud(g, "bp.math.et", 250.f, 110.f);
			const graph::NkNodeId non = NkBpCreerNoeud(g, "bp.math.non", 250.f, 260.f);
			const graph::NkNodeId et2 = NkBpCreerNoeud(g, "bp.math.et", 250.f, 360.f);
			const graph::NkNodeId si = NkBpCreerNoeud(g, "bp.si", 500.f, 0.f);
			const graph::NkNodeId parNom = NkBpCreerNoeud(g, "bp.natif:unkeny.entite.par_nom", 500.f, 150.f);
			NkBpPoserDefaut(g, parNom, "nom", porte != nullptr ? porte : "Porte");
			const graph::NkNodeId pos = NkBpCreerNoeud(g, "bp.natif:unkeny.transform.position", 500.f, 250.f);
			const graph::NkNodeId add = NkBpCreerNoeud(g, "bp.math.add_v", 500.f, 350.f);
			NkBpPoserDefaut(g, add, "b", "0 2");
			const graph::NkNodeId tel = NkBpCreerNoeud(g, "bp.natif:unkeny.transform.teleporter", 750.f, 0.f);
			const graph::NkNodeId eff = NkBpCreerNoeud(g, "bp.natif:unkeny.effet.jouer", 750.f, 130.f);
			const graph::NkNodeId aff = NkBpCreerNoeud(g, "bp.natif:unkeny.journal.afficher", 750.f, 240.f);
			NkBpPoserDefaut(g, aff, "texte", "La porte s'ouvre (Blueprint)");
			const graph::NkNodeId ecrire = NkBpCreerNoeud(g, "bp.var.ecrire.booleen", 750.f, 350.f);
			NkBpPoserDefaut(g, ecrire, "nom", "ouverte");
			NkBpPoserDefaut(g, ecrire, "valeur", "vrai");
			Fil(g, ev, "suite", si, "exec");
			Fil(g, ev, "autre", nomEst, "entité");
			Fil(g, ev, "soiEstLaZone", et, "a");
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
