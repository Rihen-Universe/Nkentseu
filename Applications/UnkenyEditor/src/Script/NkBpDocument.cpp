// -----------------------------------------------------------------------------
// FICHIER: UnkenyEditor/Script/NkBpDocument.cpp
// DESCRIPTION: Le document d'un Blueprint a la UE5 : declarations, graphes,
//              lecture / ecriture (section DOCU), gestes et noeuds du document.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Script/NkBpDocument.h"

#include "Script/NkBpCatalogue.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		namespace {
			const char *const kDeclarables[] = {"booleen", "entier", "reel", "texte", "vec2", "couleur", "entite", "asset"};
			const char *const kLibelles[] = {"Booléen", "Entier", "Réel", "Texte", "Vec2", "Couleur", "Entité", "Référence d'asset"};

			bool Egal(const char *a, const char *b) {
				return a != nullptr && b != nullptr && std::strcmp(a, b) == 0;
			}

			/// Un texte sur UNE ligne : « \ » -> « \\ », saut -> « \n ».
			NkString Echapper(const NkString &s) {
				NkString r;
				for (usize i = 0; i < s.Length(); ++i) {
					const char c = s.CStr()[i];
					if (c == '\\') {
						r.Append("\\\\");
					} else if (c == '\n') {
						r.Append("\\n");
					} else if (c != '\r') {
						r.Append(c);
					}
				}
				return r;
			}
			NkString Desechapper(const char *s, usize n) {
				NkString r;
				for (usize i = 0; i < n; ++i) {
					if (s[i] == '\\' && i + 1u < n) {
						r.Append(s[i + 1u] == 'n' ? '\n' : s[i + 1u]);
						++i;
					} else {
						r.Append(s[i]);
					}
				}
				return r;
			}

			// ── Un lecteur de lignes ─────────────────────────────────────────
			struct Lecteur {
					const char *p;
					bool Fin() const {
						return *p == '\0';
					}
					/// La ligne courante [debut, fin), puis avance.
					void Ligne(const char *&debut, usize &n) {
						debut = p;
						while (*p != '\0' && *p != '\n') {
							++p;
						}
						n = static_cast<usize>(p - debut);
						if (n > 0u && debut[n - 1u] == '\r') {
							--n;
						}
						if (*p == '\n') {
							++p;
						}
					}
			};
			/// Le jeton `i` (separe par des espaces) de [l, l+n), ou vide.
			NkString Jeton(const char *l, usize n, uint32 i, const char **reste = nullptr) {
				usize a = 0;
				for (uint32 k = 0;; ++k) {
					while (a < n && l[a] == ' ') {
						++a;
					}
					usize b = a;
					while (b < n && l[b] != ' ') {
						++b;
					}
					if (k == i) {
						if (reste != nullptr) {
							usize c = b;
							while (c < n && l[c] == ' ') {
								++c;
							}
							*reste = l + c;
						}
						return NkString(l + a, b - a);
					}
					if (b >= n) {
						if (reste != nullptr) {
							*reste = l + n;
						}
						return NkString();
					}
					a = b;
				}
			}
			/// Le reste de la ligne apres le mot-cle (desechappe).
			NkString Reste(const char *l, usize n) {
				usize a = 0;
				while (a < n && l[a] != ' ') {
					++a;
				}
				if (a < n) {
					++a;
				}
				return Desechapper(l + a, n - a);
			}

			graph::NkTypeId TypeTexte(graph::NkNodeGraph &g) {
				NkBpEnregistrerTypes(g);
				return g.FindType("texte");
			}

			void EcrireParams(NkString &s, const char *mot, const NkVector<NkParamBp> &p) {
				for (uint32 i = 0; i < p.Size(); ++i) {
					s += NkString::Format("%s %s %s %d\n", mot, p[i].nom.CStr(), p[i].type.CStr(), p[i].tableau ? 1 : 0);
				}
			}
			bool LireParam(const char *l, usize n, NkParamBp &p) {
				p.nom = Jeton(l, n, 1);
				p.type = Jeton(l, n, 2);
				p.tableau = Jeton(l, n, 3) == "1";
				return !p.nom.Empty() && !p.type.Empty();
			}
		} // namespace

		const char *const *NkBpTypesDeclarables(uint32 &nombre) noexcept {
			nombre = static_cast<uint32>(sizeof(kDeclarables) / sizeof(kDeclarables[0]));
			return kDeclarables;
		}

		const char *NkBpLibelleType(const char *type) noexcept {
			for (uint32 i = 0; i < sizeof(kDeclarables) / sizeof(kDeclarables[0]); ++i) {
				if (Egal(type, kDeclarables[i])) {
					return kLibelles[i];
				}
			}
			if (Egal(type, "exec")) {
				return "Exécution";
			}
			return type != nullptr ? type : "?";
		}

		NkString NkBpCleDeNom(const char *texte, uint32 max) {
			NkString r;
			if (texte == nullptr) {
				return r;
			}
			// Les blancs de tete et de queue tombent ; ceux du milieu -> « _ ».
			const char *a = texte;
			while (*a == ' ' || *a == '\t') {
				++a;
			}
			usize n = std::strlen(a);
			while (n > 0u && (a[n - 1u] == ' ' || a[n - 1u] == '\t')) {
				--n;
			}
			for (usize i = 0; i < n && r.Length() + 1u < max; ++i) {
				const char c = a[i];
				if (static_cast<uint8>(c) < 32u) {
					continue;
				}
				r.Append(c == ' ' || c == '\t' ? '_' : c);
			}
			// Ne pas finir au milieu d'un caractere UTF-8.
			while (!r.Empty() && (static_cast<uint8>(r.CStr()[r.Length() - 1u]) & 0xC0u) == 0x80u) {
				usize k = r.Length() - 1u;
				while (k > 0u && (static_cast<uint8>(r.CStr()[k]) & 0xC0u) == 0x80u) {
					--k;
				}
				// k = l'octet de tete : garde si la sequence est complete.
				const uint8 tete = static_cast<uint8>(r.CStr()[k]);
				const usize attendu = tete >= 0xF0u ? 4u : (tete >= 0xE0u ? 3u : (tete >= 0xC0u ? 2u : 1u));
				if (r.Length() - k == attendu) {
					break;
				}
				r = NkString(r.CStr(), k);
			}
			return r;
		}

		// =====================================================================
		// Le document
		// =====================================================================
		NkDocumentBp::NkDocumentBp() {
			Vider();
		}

		void NkDocumentBp::Vider() {
			variables.Clear();
			graphes.Clear();
			repartiteurs.Clear();
			description.Clear();
			categorie.Clear();
			NkGrapheBp ev;
			ev.genre = NkGenreGrapheBp::NK_EVENEMENTS;
			ev.nom = "Graphe d'événements";
			NkBpEnregistrerTypes(ev.graphe);
			graphes.PushBack(ev);
		}

		int32 NkDocumentBp::TrouverVariable(const char *nom) const noexcept {
			for (uint32 i = 0; i < variables.Size(); ++i) {
				if (Egal(variables[i].nom.CStr(), nom)) {
					return static_cast<int32>(i);
				}
			}
			return -1;
		}

		int32 NkDocumentBp::TrouverGraphe(const char *nom, NkGenreGrapheBp genre) const noexcept {
			for (uint32 i = 1; i < graphes.Size(); ++i) {
				if (graphes[i].genre == genre && Egal(graphes[i].nom.CStr(), nom)) {
					return static_cast<int32>(i);
				}
			}
			return -1;
		}

		int32 NkDocumentBp::TrouverRepartiteur(const char *nom) const noexcept {
			for (uint32 i = 0; i < repartiteurs.Size(); ++i) {
				if (Egal(repartiteurs[i].nom.CStr(), nom)) {
					return static_cast<int32>(i);
				}
			}
			return -1;
		}

		bool NkDocumentBp::NomPris(const char *nom) const noexcept {
			if (TrouverVariable(nom) >= 0 || TrouverRepartiteur(nom) >= 0) {
				return true;
			}
			for (uint32 i = 1; i < graphes.Size(); ++i) {
				if (Egal(graphes[i].nom.CStr(), nom)) {
					return true;
				}
			}
			return false;
		}

		NkString NkDocumentBp::NomLibre(const char *base) const {
			const NkString b = NkBpCleDeNom(base, unkeny::NK_UNKENY_VAR_NOM_MAX - 3u);
			if (!NomPris(b.CStr())) {
				return b;
			}
			for (uint32 k = 2; k < 1000u; ++k) {
				const NkString n = NkString::Format("%s_%u", b.CStr(), static_cast<unsigned>(k));
				if (!NomPris(n.CStr())) {
					return n;
				}
			}
			return b;
		}

		// =====================================================================
		// Ecrire / lire DOCU
		// =====================================================================
		void NkBpEcrireDocument(const NkDocumentBp &d, NkString &s) {
			s = "nkbpdoc 1\n";
			if (!d.description.Empty()) {
				s += NkString("classe ") + Echapper(d.description) + "\n";
			}
			if (!d.categorie.Empty()) {
				s += NkString("categorie ") + Echapper(d.categorie) + "\n";
			}
			for (uint32 i = 0; i < d.variables.Size(); ++i) {
				const NkVariableDeclBp &v = d.variables[i];
				s += NkString::Format("var %s %s %d %d\n", v.nom.CStr(), v.type.CStr(), v.tableau ? 1 : 0, v.instance ? 1 : 0);
				if (!v.defaut.Empty()) {
					s += NkString("vdef ") + Echapper(v.defaut) + "\n";
				}
				if (!v.infobulle.Empty()) {
					s += NkString("vinfo ") + Echapper(v.infobulle) + "\n";
				}
				if (!v.categorie.Empty()) {
					s += NkString("vcat ") + Echapper(v.categorie) + "\n";
				}
			}
			for (uint32 i = 1; i < d.graphes.Size(); ++i) {
				const NkGrapheBp &g = d.graphes[i];
				s += NkString::Format("graphe %d %d ", static_cast<int>(g.genre), g.pure ? 1 : 0) + Echapper(g.nom) + "\n";
				if (!g.categorie.Empty()) {
					s += NkString("gcat ") + Echapper(g.categorie) + "\n";
				}
				if (!g.infobulle.Empty()) {
					s += NkString("ginfo ") + Echapper(g.infobulle) + "\n";
				}
				EcrireParams(s, "entree", g.entrees);
				EcrireParams(s, "sortie", g.sorties);
				NkString texte;
				g.graphe.Serialize(texte);
				s += NkString::Format("texte %u\n", static_cast<unsigned>(texte.Length()));
				s += texte;
				s += "\n";
			}
			for (uint32 i = 0; i < d.repartiteurs.Size(); ++i) {
				const NkRepartiteurBp &r = d.repartiteurs[i];
				s += NkString("rep ") + r.nom + "\n";
				if (!r.categorie.Empty()) {
					s += NkString("rcat ") + Echapper(r.categorie) + "\n";
				}
				if (!r.infobulle.Empty()) {
					s += NkString("rinfo ") + Echapper(r.infobulle) + "\n";
				}
				EcrireParams(s, "param", r.params);
			}
			s += "fin\n";
		}

		bool NkBpLireDocument(const char *texte, NkDocumentBp &d, NkString *erreur) {
			auto echec = [&](const NkString &pourquoi) {
				if (erreur != nullptr) {
					*erreur = pourquoi;
				}
				return false;
			};
			if (texte == nullptr || std::strncmp(texte, "nkbpdoc", 7) != 0) {
				return echec(NkString("DOCU : en-tete « nkbpdoc » absent"));
			}
			// Garde le graphe d'evenements deja lu ; le reste repart de zero.
			NkGrapheBp ev = d.graphes.Empty() ? NkGrapheBp() : d.graphes[0];
			d.Vider();
			d.graphes[0].graphe = ev.graphe;
			Lecteur L{texte};
			const char *l = nullptr;
			usize n = 0;
			L.Ligne(l, n); // en-tete
			NkVariableDeclBp *var = nullptr;
			NkGrapheBp *gr = nullptr;
			NkRepartiteurBp *rep = nullptr;
			while (!L.Fin()) {
				L.Ligne(l, n);
				const NkString mot = Jeton(l, n, 0);
				if (mot.Empty()) {
					continue;
				}
				if (mot == "fin") {
					break;
				} else if (mot == "classe") {
					d.description = Reste(l, n);
				} else if (mot == "categorie") {
					d.categorie = Reste(l, n);
				} else if (mot == "var") {
					NkVariableDeclBp v;
					v.nom = Jeton(l, n, 1);
					v.type = Jeton(l, n, 2);
					v.tableau = Jeton(l, n, 3) == "1";
					v.instance = Jeton(l, n, 4) != "0";
					d.variables.PushBack(v);
					var = &d.variables.Back();
					gr = nullptr;
					rep = nullptr;
				} else if (mot == "vdef" && var != nullptr) {
					var->defaut = Reste(l, n);
				} else if (mot == "vinfo" && var != nullptr) {
					var->infobulle = Reste(l, n);
				} else if (mot == "vcat" && var != nullptr) {
					var->categorie = Reste(l, n);
				} else if (mot == "graphe") {
					NkGrapheBp g;
					const int genre = std::atoi(Jeton(l, n, 1).CStr());
					g.genre = genre == 2 ? NkGenreGrapheBp::NK_MACRO : NkGenreGrapheBp::NK_FONCTION;
					g.pure = Jeton(l, n, 2) == "1";
					const char *reste = nullptr;
					Jeton(l, n, 2, &reste);
					g.nom = Desechapper(reste, static_cast<usize>(l + n - reste));
					NkBpEnregistrerTypes(g.graphe);
					d.graphes.PushBack(g);
					gr = &d.graphes.Back();
					var = nullptr;
					rep = nullptr;
				} else if (mot == "gcat" && gr != nullptr) {
					gr->categorie = Reste(l, n);
				} else if (mot == "ginfo" && gr != nullptr) {
					gr->infobulle = Reste(l, n);
				} else if ((mot == "entree" || mot == "sortie") && gr != nullptr) {
					NkParamBp p;
					if (LireParam(l, n, p)) {
						(mot == "entree" ? gr->entrees : gr->sorties).PushBack(p);
					}
				} else if (mot == "texte" && gr != nullptr) {
					const usize taille = static_cast<usize>(std::strtoul(Jeton(l, n, 1).CStr(), nullptr, 10));
					const usize dispo = std::strlen(L.p);
					if (taille > dispo) {
						return echec(NkString::Format("DOCU : graphe « %s » tronque", gr->nom.CStr()));
					}
					const NkString g(L.p, taille);
					L.p += taille;
					if (*L.p == '\n') {
						++L.p;
					}
					NkString err;
					if (!gr->graphe.Deserialize(g.CStr(), &err)) {
						return echec(NkString::Format("DOCU : graphe « %s » illisible : %s", gr->nom.CStr(), err.CStr()));
					}
					NkBpEnregistrerTypes(gr->graphe);
				} else if (mot == "rep") {
					NkRepartiteurBp r;
					r.nom = Jeton(l, n, 1);
					d.repartiteurs.PushBack(r);
					rep = &d.repartiteurs.Back();
					var = nullptr;
					gr = nullptr;
				} else if (mot == "rcat" && rep != nullptr) {
					rep->categorie = Reste(l, n);
				} else if (mot == "rinfo" && rep != nullptr) {
					rep->infobulle = Reste(l, n);
				} else if (mot == "param" && rep != nullptr) {
					NkParamBp p;
					if (LireParam(l, n, p)) {
						rep->params.PushBack(p);
					}
				}
				// Un mot inconnu (une version plus recente) : ignore.
			}
			return true;
		}

		void NkBpInstantane(const NkDocumentBp &d, NkString &s) {
			NkString ev;
			d.graphes[0].graphe.Serialize(ev);
			NkString doc;
			NkBpEcrireDocument(d, doc);
			s = NkString::Format("GRAF %u\n", static_cast<unsigned>(ev.Length())) + ev + doc;
		}

		bool NkBpDepuisInstantane(const char *texte, NkDocumentBp &d) {
			if (texte == nullptr || std::strncmp(texte, "GRAF ", 5) != 0) {
				return false;
			}
			const char *p = texte + 5;
			const usize n = static_cast<usize>(std::strtoul(p, nullptr, 10));
			while (*p != '\0' && *p != '\n') {
				++p;
			}
			if (*p == '\n') {
				++p;
			}
			if (std::strlen(p) < n) {
				return false;
			}
			const NkString ev(p, n);
			d.Vider();
			if (!d.graphes[0].graphe.Deserialize(ev.CStr())) {
				return false;
			}
			NkBpEnregistrerTypes(d.graphes[0].graphe);
			return NkBpLireDocument(p + n, d);
		}

		bool NkBpOuvrirDocument(const char *chemin, NkDocumentBp &d, NkString *erreur) {
			NkString graf, docu;
			if (!unkeny::NkLireFichierBp(chemin, &graf, nullptr, nullptr, erreur, &docu)) {
				return false;
			}
			d.Vider();
			if (!graf.Empty() && !d.graphes[0].graphe.Deserialize(graf.CStr(), erreur)) {
				return false;
			}
			NkBpEnregistrerTypes(d.graphes[0].graphe);
			if (!docu.Empty()) {
				return NkBpLireDocument(docu.CStr(), d, erreur);
			}
			return true;
		}

		// =====================================================================
		// Les proprietes des noeuds
		// =====================================================================
		NkString NkBpPropTexte(const graph::NkNodeGraph &g, const graph::NkNode &n, const char *nom) {
			const graph::NkGraphValue *v = g.FindProp(n.id, nom);
			return v != nullptr && v->IsSet() ? v->text : NkString();
		}

		void NkBpPoserPropTexte(graph::NkNodeGraph &g, graph::NkNodeId n, const char *nom, const char *valeur) {
			graph::NkGraphValue v;
			v.type = TypeTexte(g);
			v.text = valeur != nullptr ? valeur : "";
			g.SetProp(n, nom, v);
		}

		float32 NkBpPropReel(const graph::NkNodeGraph &g, const graph::NkNode &n, const char *nom, float32 defaut) {
			const graph::NkGraphValue *v = g.FindProp(n.id, nom);
			return v != nullptr && v->IsSet() && !v->numbers.Empty() ? v->numbers[0] : defaut;
		}

		void NkBpPoserPropReel(graph::NkNodeGraph &g, graph::NkNodeId n, const char *nom, float32 valeur) {
			NkBpEnregistrerTypes(g);
			g.SetProp(n, nom, graph::NkValueReal(g.FindType("reel"), valeur));
		}

		// =====================================================================
		// Les noeuds du document
		// =====================================================================
		namespace {
			/// Une prise voulue : nom, type, sens.
			struct PriseVoulue {
					NkString nom;
					NkString type;
					graph::NkSocketDir dir = graph::NkSocketDir::Input;
			};
			void Voulue(NkVector<PriseVoulue> &v, const char *nom, const char *type, graph::NkSocketDir dir) {
				PriseVoulue p;
				p.nom = nom;
				p.type = type;
				p.dir = dir;
				v.PushBack(p);
			}
			void Params(NkVector<PriseVoulue> &v, const NkVector<NkParamBp> &p, graph::NkSocketDir dir) {
				for (uint32 i = 0; i < p.Size(); ++i) {
					Voulue(v, p[i].nom.CStr(), p[i].type.CStr(), dir);
				}
			}

			/// Les prises qu'un noeud du document DOIT avoir (selon les
			/// declarations). false : la declaration n'existe plus.
			bool PrisesVoulues(const NkDocumentBp &d, const graph::NkNodeGraph &g, const graph::NkNode &n, NkVector<PriseVoulue> &v,
							   NkString &libelle) {
				const graph::NkSocketDir E = graph::NkSocketDir::Input, S = graph::NkSocketDir::Output;
				if (n.type == NK_BP_VAR_LIRE || n.type == NK_BP_VAR_ECRIRE) {
					const NkString nom = NkBpPropTexte(g, n, "var");
					const int32 k = d.TrouverVariable(nom.CStr());
					if (k < 0) {
						return false;
					}
					const NkVariableDeclBp &x = d.variables[static_cast<uint32>(k)];
					if (n.type == NK_BP_VAR_LIRE) {
						Voulue(v, "valeur", x.type.CStr(), S);
						libelle = x.nom;
					} else {
						Voulue(v, "exec", "exec", E);
						Voulue(v, "valeur", x.type.CStr(), E);
						Voulue(v, "suite", "exec", S);
						Voulue(v, "valeur", x.type.CStr(), S);
						libelle = NkString("Écrire ") + x.nom;
					}
					return true;
				}
				if (n.type == NK_BP_APPEL) {
					const int32 k = d.TrouverGraphe(NkBpPropTexte(g, n, "fonction").CStr(), NkGenreGrapheBp::NK_FONCTION);
					if (k < 0) {
						return false;
					}
					const NkGrapheBp &f = d.graphes[static_cast<uint32>(k)];
					if (!f.pure) {
						Voulue(v, "exec", "exec", E);
					}
					Params(v, f.entrees, E);
					if (!f.pure) {
						Voulue(v, "suite", "exec", S);
					}
					Params(v, f.sorties, S);
					libelle = f.nom;
					return true;
				}
				if (n.type == NK_BP_MACRO) {
					const int32 k = d.TrouverGraphe(NkBpPropTexte(g, n, "macro").CStr(), NkGenreGrapheBp::NK_MACRO);
					if (k < 0) {
						return false;
					}
					const NkGrapheBp &f = d.graphes[static_cast<uint32>(k)];
					Params(v, f.entrees, E);
					Params(v, f.sorties, S);
					libelle = f.nom;
					return true;
				}
				if (n.type == NK_BP_REP_APPELER || n.type == NK_BP_REP_EVENEMENT) {
					const int32 k = d.TrouverRepartiteur(NkBpPropTexte(g, n, "rep").CStr());
					if (k < 0) {
						return false;
					}
					const NkRepartiteurBp &r = d.repartiteurs[static_cast<uint32>(k)];
					if (n.type == NK_BP_REP_APPELER) {
						Voulue(v, "exec", "exec", E);
						Voulue(v, "cible", "entite", E);
						Params(v, r.params, E);
						Voulue(v, "suite", "exec", S);
						libelle = NkString("Appeler ") + r.nom;
					} else {
						Voulue(v, "suite", "exec", S);
						Params(v, r.params, S);
						libelle = NkString("Événement ") + r.nom;
					}
					return true;
				}
				return true; // pas un noeud de declaration : rien a refaire
			}

			/// Les noeuds d'ENTREE et de RETOUR d'une fonction, d'entree et de
			/// sortie d'une macro : leurs prises viennent du graphe qui les porte.
			bool PrisesFrontiere(const NkGrapheBp &gr, const graph::NkNode &n, NkVector<PriseVoulue> &v, NkString &libelle) {
				const graph::NkSocketDir E = graph::NkSocketDir::Input, S = graph::NkSocketDir::Output;
				if (n.type == NK_BP_FN_ENTREE) {
					Voulue(v, "suite", "exec", S);
					Params(v, gr.entrees, S);
					libelle = gr.nom;
					return true;
				}
				if (n.type == NK_BP_FN_RETOUR) {
					Voulue(v, "exec", "exec", E);
					Params(v, gr.sorties, E);
					libelle = "Retour";
					return true;
				}
				if (n.type == NK_BP_MACRO_ENTREE) {
					Params(v, gr.entrees, S);
					libelle = "Entrées";
					return true;
				}
				if (n.type == NK_BP_MACRO_SORTIE) {
					Params(v, gr.sorties, E);
					libelle = "Sorties";
					return true;
				}
				return false;
			}

			graph::NkNodeId Fabriquer(graph::NkNodeGraph &g, const char *type, const char *libelle, const NkVector<PriseVoulue> &v,
									  float32 x, float32 y) {
				NkBpEnregistrerTypes(g);
				const graph::NkNodeId n = g.AddNode(type, libelle);
				for (uint32 i = 0; i < v.Size(); ++i) {
					const bool exec = v[i].type == "exec";
					graph::NkTypeId t = g.FindType(v[i].type.CStr());
					if (t == graph::NK_TYPE_INVALID) {
						t = g.RegisterType(v[i].type.CStr());
					}
					g.AddSocket(n, v[i].nom.CStr(), t, v[i].dir, exec ? graph::NkSocketFamily::Exec : graph::NkSocketFamily::Data);
				}
				if (graph::NkNode *p = g.Find(n)) {
					p->x = x;
					p->y = y;
				}
				return n;
			}

			bool MemesPrises(const graph::NkNodeGraph &g, const graph::NkNode &n, const NkVector<PriseVoulue> &v) {
				if (n.sockets.Size() != v.Size()) {
					return false;
				}
				for (uint32 i = 0; i < v.Size(); ++i) {
					const graph::NkSocket &s = n.sockets[i];
					const NkString *t = g.TypeName(s.type);
					if (!(s.name == v[i].nom) || s.dir != v[i].dir || t == nullptr || !(*t == v[i].type)) {
						return false;
					}
				}
				return true;
			}

			/// Refait le noeud `id` avec les prises `v` : position, proprietes,
			/// valeurs saisies et FILS gardes par nom de prise. Rend le nouveau.
			graph::NkNodeId Refaire(graph::NkNodeGraph &g, graph::NkNodeId id, const NkVector<PriseVoulue> &v, const NkString &libelle) {
				const graph::NkNode *ancien = g.Find(id);
				if (ancien == nullptr) {
					return graph::NK_NODE_INVALID;
				}
				const graph::NkNode copie = *ancien;
				struct Fil {
						graph::NkNodeId de, vers;
						NkString priseDe, priseVers;
						bool sortant; ///< le fil part de CE noeud
				};
				NkVector<Fil> fils;
				for (uint32 i = 0; i < g.LinkCount(); ++i) {
					const graph::NkLink *l = g.LinkAt(i);
					if (l == nullptr || !l->alive || (l->fromNode != id && l->toNode != id)) {
						continue;
					}
					const graph::NkNode *a = g.Find(l->fromNode);
					const graph::NkNode *b = g.Find(l->toNode);
					if (a == nullptr || b == nullptr) {
						continue;
					}
					Fil f;
					f.de = l->fromNode;
					f.vers = l->toNode;
					f.priseDe = a->sockets[static_cast<uint32>(l->fromSocket)].name;
					f.priseVers = b->sockets[static_cast<uint32>(l->toSocket)].name;
					f.sortant = l->fromNode == id;
					fils.PushBack(f);
				}
				g.RemoveNode(id);
				const graph::NkNodeId n = Fabriquer(g, copie.type.CStr(), libelle.Empty() ? copie.label.CStr() : libelle.CStr(), v, copie.x, copie.y);
				for (uint32 i = 0; i < copie.props.Size(); ++i) {
					g.SetProp(n, copie.props[i].name.CStr(), copie.props[i].value);
				}
				// Les valeurs saisies : meme nom, meme sens, meme type.
				if (graph::NkNode *p = g.Find(n)) {
					for (uint32 k = 0; k < copie.sockets.Size(); ++k) {
						const graph::NkSocket &s = copie.sockets[k];
						if (!s.defaultValue.IsSet()) {
							continue;
						}
						const int32 j = p->FindSocket(s.name.CStr(), s.dir);
						if (j >= 0 && p->sockets[static_cast<uint32>(j)].type == s.type) {
							g.SetSocketDefault(n, s.name.CStr(), s.dir, s.defaultValue);
						}
					}
				}
				for (uint32 i = 0; i < fils.Size(); ++i) {
					const Fil &f = fils[i];
					// Un fil dont la prise a disparu, ou dont le type ne colle plus,
					// est refuse par Connect : il tombe (comme « Rafraichir » d'UE5).
					g.Connect(f.sortant ? n : f.de, f.priseDe.CStr(), f.sortant ? f.vers : n, f.priseVers.CStr());
				}
				return n;
			}
		} // namespace

		uint32 NkBpRafraichirNoeuds(NkDocumentBp &d) {
			uint32 refaits = 0;
			for (uint32 gi = 0; gi < d.graphes.Size(); ++gi) {
				graph::NkNodeGraph &g = d.graphes[gi].graphe;
				NkVector<graph::NkNodeId> ids;
				for (uint32 i = 0; i < g.RawNodeCount(); ++i) {
					const graph::NkNode *n = g.RawNodeAt(i);
					if (n != nullptr && n->alive) {
						ids.PushBack(n->id);
					}
				}
				for (uint32 i = 0; i < ids.Size(); ++i) {
					const graph::NkNode *n = g.Find(ids[i]);
					if (n == nullptr) {
						continue;
					}
					NkVector<PriseVoulue> v;
					NkString libelle;
					bool frontiere = PrisesFrontiere(d.graphes[gi], *n, v, libelle);
					if (!frontiere) {
						v.Clear();
						if (!PrisesVoulues(d, g, *n, v, libelle)) {
							g.RemoveNode(ids[i]); // sa declaration n'existe plus
							++refaits;
							continue;
						}
						if (v.Empty() && libelle.Empty()) {
							continue; // un noeud du catalogue fixe
						}
					}
					if (!MemesPrises(g, *n, v)) {
						Refaire(g, ids[i], v, libelle);
						++refaits;
					} else if (!libelle.Empty() && !(n->label == libelle)) {
						if (graph::NkNode *m = g.Find(ids[i])) {
							m->label = libelle;
						}
					}
				}
			}
			return refaits;
		}

		namespace {
			graph::NkNodeId CreerDeclare(NkDocumentBp &d, graph::NkNodeGraph &g, const char *type, const char *prop, const char *nom,
										 float32 x, float32 y) {
				NkBpEnregistrerTypes(g);
				const graph::NkNodeId n = g.AddNode(type, nom);
				NkBpPoserPropTexte(g, n, prop, nom);
				const graph::NkNode *p = g.Find(n);
				NkVector<PriseVoulue> v;
				NkString libelle;
				if (p == nullptr || !PrisesVoulues(d, g, *p, v, libelle)) {
					g.RemoveNode(n);
					return graph::NK_NODE_INVALID;
				}
				const graph::NkNodeId r = Refaire(g, n, v, libelle);
				if (graph::NkNode *q = g.Find(r)) {
					q->x = x;
					q->y = y;
				}
				return r;
			}
		} // namespace

		graph::NkNodeId NkBpCreerLireVariable(NkDocumentBp &d, graph::NkNodeGraph &g, const char *nom, float32 x, float32 y) {
			return CreerDeclare(d, g, NK_BP_VAR_LIRE, "var", nom, x, y);
		}
		graph::NkNodeId NkBpCreerEcrireVariable(NkDocumentBp &d, graph::NkNodeGraph &g, const char *nom, float32 x, float32 y) {
			return CreerDeclare(d, g, NK_BP_VAR_ECRIRE, "var", nom, x, y);
		}
		graph::NkNodeId NkBpCreerAppel(NkDocumentBp &d, graph::NkNodeGraph &g, const char *nom, float32 x, float32 y) {
			return CreerDeclare(d, g, NK_BP_APPEL, "fonction", nom, x, y);
		}
		graph::NkNodeId NkBpCreerMacro(NkDocumentBp &d, graph::NkNodeGraph &g, const char *nom, float32 x, float32 y) {
			return CreerDeclare(d, g, NK_BP_MACRO, "macro", nom, x, y);
		}
		graph::NkNodeId NkBpCreerAppelerRepartiteur(NkDocumentBp &d, graph::NkNodeGraph &g, const char *nom, float32 x, float32 y) {
			return CreerDeclare(d, g, NK_BP_REP_APPELER, "rep", nom, x, y);
		}
		graph::NkNodeId NkBpCreerEvenementRepartiteur(NkDocumentBp &d, graph::NkNodeGraph &g, const char *nom, float32 x, float32 y) {
			return CreerDeclare(d, g, NK_BP_REP_EVENEMENT, "rep", nom, x, y);
		}

		graph::NkNodeId NkBpCreerRelais(graph::NkNodeGraph &g, const char *type, float32 x, float32 y) {
			NkVector<PriseVoulue> v;
			Voulue(v, "in", type, graph::NkSocketDir::Input);
			Voulue(v, "out", type, graph::NkSocketDir::Output);
			const graph::NkNodeId n = Fabriquer(g, NK_BP_RELAIS, "", v, x, y);
			NkBpPoserPropTexte(g, n, "type", type);
			return n;
		}

		graph::NkNodeId NkBpCreerCommentaire(graph::NkNodeGraph &g, const char *texte, float32 x, float32 y, float32 w, float32 h) {
			NkVector<PriseVoulue> v;
			// Une ligne : le libelle est le reste d'une ligne du .nkgraph.
			NkString t;
			for (const char *p = texte != nullptr ? texte : ""; *p != '\0'; ++p) {
				t.Append(*p == '\n' || *p == '\r' ? ' ' : *p);
			}
			const graph::NkNodeId n = Fabriquer(g, NK_BP_COMMENTAIRE, t.CStr(), v, x, y);
			NkBpPoserPropReel(g, n, "w", w);
			NkBpPoserPropReel(g, n, "h", h);
			return n;
		}

		graph::NkNodeId NkBpCreerParCle(NkDocumentBp &d, graph::NkNodeGraph &g, const char *cle, float32 x, float32 y) {
			if (cle == nullptr) {
				return graph::NK_NODE_INVALID;
			}
			struct Pref {
					const char *prefixe;
					graph::NkNodeId (*f)(NkDocumentBp &, graph::NkNodeGraph &, const char *, float32, float32);
			};
			const Pref prefs[] = {{"bp.var.get:", &NkBpCreerLireVariable},
								  {"bp.var.set:", &NkBpCreerEcrireVariable},
								  {"bp.appel:", &NkBpCreerAppel},
								  {"bp.macro:", &NkBpCreerMacro},
								  {"bp.rep.appeler:", &NkBpCreerAppelerRepartiteur},
								  {"bp.rep.evenement:", &NkBpCreerEvenementRepartiteur}};
			for (const Pref &p : prefs) {
				const usize n = std::strlen(p.prefixe);
				if (std::strncmp(cle, p.prefixe, n) == 0) {
					return p.f(d, g, cle + n, x, y);
				}
			}
			if (std::strncmp(cle, "bp.relais:", 10) == 0) {
				return NkBpCreerRelais(g, cle + 10, x, y);
			}
			if (Egal(cle, NK_BP_COMMENTAIRE)) {
				return NkBpCreerCommentaire(g, "Commentaire", x, y, 320.f, 180.f);
			}
			if (Egal(cle, NK_BP_FN_RETOUR)) {
				// Le RETOUR d'une fonction : ses prises viennent de la fonction qui
				// porte ce graphe.
				for (uint32 i = 1; i < d.graphes.Size(); ++i) {
					if (&d.graphes[i].graphe == &g) {
						NkVector<PriseVoulue> v;
						NkString libelle;
						graph::NkNode tmp;
						tmp.type = NK_BP_FN_RETOUR;
						PrisesFrontiere(d.graphes[i], tmp, v, libelle);
						return Fabriquer(g, NK_BP_FN_RETOUR, libelle.CStr(), v, x, y);
					}
				}
				return graph::NK_NODE_INVALID;
			}
			return NkBpCreerNoeud(g, cle, x, y);
		}

		// =====================================================================
		// Les gestes sur le document
		// =====================================================================
		namespace {
			/// Pose les noeuds de frontiere d'un graphe de fonction ou de macro.
			void Frontieres(NkGrapheBp &gr) {
				NkBpEnregistrerTypes(gr.graphe);
				NkVector<PriseVoulue> v;
				NkString libelle;
				graph::NkNode tmp;
				if (gr.genre == NkGenreGrapheBp::NK_FONCTION) {
					tmp.type = NK_BP_FN_ENTREE;
					PrisesFrontiere(gr, tmp, v, libelle);
					Fabriquer(gr.graphe, NK_BP_FN_ENTREE, libelle.CStr(), v, 0.f, 0.f);
					v.Clear();
					tmp.type = NK_BP_FN_RETOUR;
					PrisesFrontiere(gr, tmp, v, libelle);
					const graph::NkNodeId r = Fabriquer(gr.graphe, NK_BP_FN_RETOUR, libelle.CStr(), v, 420.f, 0.f);
					// Entree -> Retour : une fonction neuve s'execute deja jusqu'au bout.
					const graph::NkNode *e = nullptr;
					for (uint32 i = 0; i < gr.graphe.RawNodeCount(); ++i) {
						const graph::NkNode *n = gr.graphe.RawNodeAt(i);
						if (n != nullptr && n->alive && n->type == NK_BP_FN_ENTREE) {
							e = n;
						}
					}
					if (e != nullptr) {
						gr.graphe.Connect(e->id, "suite", r, "exec");
					}
				} else if (gr.genre == NkGenreGrapheBp::NK_MACRO) {
					tmp.type = NK_BP_MACRO_ENTREE;
					PrisesFrontiere(gr, tmp, v, libelle);
					const graph::NkNodeId a = Fabriquer(gr.graphe, NK_BP_MACRO_ENTREE, libelle.CStr(), v, 0.f, 0.f);
					v.Clear();
					tmp.type = NK_BP_MACRO_SORTIE;
					PrisesFrontiere(gr, tmp, v, libelle);
					const graph::NkNodeId b = Fabriquer(gr.graphe, NK_BP_MACRO_SORTIE, libelle.CStr(), v, 420.f, 0.f);
					gr.graphe.Connect(a, "exec", b, "suite");
				}
			}
			void RenommerProp(NkDocumentBp &d, const char *prop, const char *ancien, const char *nouveau) {
				for (uint32 gi = 0; gi < d.graphes.Size(); ++gi) {
					graph::NkNodeGraph &g = d.graphes[gi].graphe;
					for (uint32 i = 0; i < g.RawNodeCount(); ++i) {
						const graph::NkNode *n = g.RawNodeAt(i);
						if (n != nullptr && n->alive && NkBpPropTexte(g, *n, prop) == ancien) {
							NkBpPoserPropTexte(g, n->id, prop, nouveau);
						}
					}
				}
			}
		} // namespace

		uint32 NkBpAjouterVariable(NkDocumentBp &d, const char *nom, const char *type) {
			NkVariableDeclBp v;
			v.nom = d.NomLibre(nom != nullptr ? nom : "NouvelleVar");
			v.type = type != nullptr ? type : "booleen";
			d.variables.PushBack(v);
			return static_cast<uint32>(d.variables.Size() - 1u);
		}

		uint32 NkBpAjouterFonction(NkDocumentBp &d, const char *nom) {
			NkGrapheBp g;
			g.genre = NkGenreGrapheBp::NK_FONCTION;
			g.nom = d.NomLibre(nom != nullptr ? nom : "NouvelleFonction");
			Frontieres(g);
			d.graphes.PushBack(g);
			return static_cast<uint32>(d.graphes.Size() - 1u);
		}

		uint32 NkBpAjouterMacro(NkDocumentBp &d, const char *nom) {
			NkGrapheBp g;
			g.genre = NkGenreGrapheBp::NK_MACRO;
			g.nom = d.NomLibre(nom != nullptr ? nom : "NouvelleMacro");
			NkParamBp e;
			e.nom = "exec";
			e.type = "exec";
			g.entrees.PushBack(e);
			NkParamBp s;
			s.nom = "suite";
			s.type = "exec";
			g.sorties.PushBack(s);
			Frontieres(g);
			d.graphes.PushBack(g);
			return static_cast<uint32>(d.graphes.Size() - 1u);
		}

		uint32 NkBpAjouterRepartiteur(NkDocumentBp &d, const char *nom) {
			NkRepartiteurBp r;
			r.nom = d.NomLibre(nom != nullptr ? nom : "NouveauRepartiteur");
			d.repartiteurs.PushBack(r);
			return static_cast<uint32>(d.repartiteurs.Size() - 1u);
		}

		bool NkBpRenommerVariable(NkDocumentBp &d, uint32 v, const char *nom) {
			const NkString cle = NkBpCleDeNom(nom, unkeny::NK_UNKENY_VAR_NOM_MAX);
			if (v >= d.variables.Size() || cle.Empty()) {
				return false;
			}
			if (cle == d.variables[v].nom) {
				return true;
			}
			if (d.NomPris(cle.CStr())) {
				return false;
			}
			const NkString ancien = d.variables[v].nom;
			d.variables[v].nom = cle;
			RenommerProp(d, "var", ancien.CStr(), cle.CStr());
			NkBpRafraichirNoeuds(d); // les libelles
			return true;
		}

		bool NkBpRenommerGraphe(NkDocumentBp &d, uint32 g, const char *nom) {
			const NkString cle = NkBpCleDeNom(nom);
			if (g == 0u || g >= d.graphes.Size() || cle.Empty()) {
				return false;
			}
			if (cle == d.graphes[g].nom) {
				return true;
			}
			if (d.NomPris(cle.CStr())) {
				return false;
			}
			const NkString ancien = d.graphes[g].nom;
			d.graphes[g].nom = cle;
			RenommerProp(d, d.graphes[g].genre == NkGenreGrapheBp::NK_MACRO ? "macro" : "fonction", ancien.CStr(), cle.CStr());
			NkBpRafraichirNoeuds(d);
			return true;
		}

		bool NkBpRenommerRepartiteur(NkDocumentBp &d, uint32 r, const char *nom) {
			const NkString cle = NkBpCleDeNom(nom);
			if (r >= d.repartiteurs.Size() || cle.Empty()) {
				return false;
			}
			if (cle == d.repartiteurs[r].nom) {
				return true;
			}
			if (d.NomPris(cle.CStr())) {
				return false;
			}
			const NkString ancien = d.repartiteurs[r].nom;
			d.repartiteurs[r].nom = cle;
			RenommerProp(d, "rep", ancien.CStr(), cle.CStr());
			NkBpRafraichirNoeuds(d);
			return true;
		}

		void NkBpSupprimerVariable(NkDocumentBp &d, uint32 v) {
			if (v < d.variables.Size()) {
				d.variables.Erase(d.variables.Begin() + v);
				NkBpRafraichirNoeuds(d); // ses noeuds partent
			}
		}

		void NkBpSupprimerGraphe(NkDocumentBp &d, uint32 g) {
			if (g > 0u && g < d.graphes.Size()) {
				d.graphes.Erase(d.graphes.Begin() + g);
				NkBpRafraichirNoeuds(d);
			}
		}

		void NkBpSupprimerRepartiteur(NkDocumentBp &d, uint32 r) {
			if (r < d.repartiteurs.Size()) {
				d.repartiteurs.Erase(d.repartiteurs.Begin() + r);
				NkBpRafraichirNoeuds(d);
			}
		}

		// =====================================================================
		// Les valeurs
		// =====================================================================
		bool NkBpLireCouleur(const char *texte, uint32 &rvba) noexcept {
			if (texte == nullptr) {
				return false;
			}
			const char *p = texte;
			while (*p == ' ') {
				++p;
			}
			if (*p == '#') {
				++p;
			}
			uint32 v = 0;
			uint32 n = 0;
			for (; p[n] != '\0' && n < 8u; ++n) {
				const char c = p[n];
				uint32 x;
				if (c >= '0' && c <= '9') {
					x = static_cast<uint32>(c - '0');
				} else if (c >= 'a' && c <= 'f') {
					x = static_cast<uint32>(c - 'a' + 10);
				} else if (c >= 'A' && c <= 'F') {
					x = static_cast<uint32>(c - 'A' + 10);
				} else {
					break;
				}
				v = (v << 4) | x;
			}
			if (n == 6u) {
				rvba = (v << 8) | 0xFFu; // opaque
				return true;
			}
			if (n == 8u) {
				rvba = v;
				return true;
			}
			return false;
		}

		NkString NkBpTexteCouleur(uint32 rvba) {
			return NkString::Format("#%08X", static_cast<unsigned>(rvba));
		}

		bool NkBpValeurDeTexte(const char *type, const char *texte, unkeny::NkValeurBp &v, NkString *texteSortie) {
			v = unkeny::NkValeurBp();
			const char *t = texte != nullptr ? texte : "";
			char *fin = nullptr;
			if (Egal(type, "booleen")) {
				const bool vrai = Egal(t, "vrai") || Egal(t, "1") || Egal(t, "true") || Egal(t, "oui");
				const bool faux = t[0] == '\0' || Egal(t, "faux") || Egal(t, "0") || Egal(t, "false") || Egal(t, "non");
				v.i = vrai ? 1 : 0;
				return vrai || faux;
			}
			if (Egal(type, "entier")) {
				if (t[0] == '\0') {
					return true;
				}
				const long x = std::strtol(t, &fin, 10);
				v.i = static_cast<int32>(x);
				v.x = static_cast<float32>(x);
				return fin != t;
			}
			if (Egal(type, "reel")) {
				if (t[0] == '\0') {
					return true;
				}
				v.x = std::strtof(t, &fin);
				return fin != t;
			}
			if (Egal(type, "vec2")) {
				if (t[0] == '\0') {
					return true;
				}
				v.x = std::strtof(t, &fin);
				if (fin == t) {
					return false;
				}
				const char *a = fin;
				while (*a == ' ' || *a == ',' || *a == ';') {
					++a;
				}
				v.y = std::strtof(a, &fin);
				if (fin == a) {
					v.y = 0.f;
				}
				return true;
			}
			if (Egal(type, "couleur")) {
				uint32 c = 0xFFFFFFFFu;
				if (t[0] != '\0' && !NkBpLireCouleur(t, c)) {
					return false;
				}
				v.i = static_cast<int32>(c);
				return true;
			}
			if (Egal(type, "texte") || Egal(type, "asset")) {
				if (texteSortie != nullptr) {
					*texteSortie = t;
				}
				return true;
			}
			if (Egal(type, "entite")) {
				return t[0] == '\0'; // « aucune » : une entite se choisit par instance
			}
			return false;
		}

		// =====================================================================
		// Annuler / refaire
		// =====================================================================
		void NkBpHistorique::Reset(const NkDocumentBp &d) {
			mPile.Clear();
			NkString s;
			NkBpInstantane(d, s);
			mPile.PushBack(s);
			mCurseur = 0u;
		}

		void NkBpHistorique::Commit(const NkDocumentBp &d) {
			NkString s;
			NkBpInstantane(d, s);
			if (!mPile.Empty() && mPile[mCurseur] == s) {
				return; // rien n'a change
			}
			while (mPile.Size() > mCurseur + 1u) {
				mPile.PopBack();
			}
			mPile.PushBack(s);
			if (mPile.Size() > 128u) {
				mPile.Erase(mPile.Begin());
			}
			mCurseur = static_cast<uint32>(mPile.Size() - 1u);
		}

		bool NkBpHistorique::Undo(NkDocumentBp &d) {
			if (mCurseur == 0u || mPile.Empty()) {
				return false;
			}
			--mCurseur;
			return NkBpDepuisInstantane(mPile[mCurseur].CStr(), d);
		}

		bool NkBpHistorique::Redo(NkDocumentBp &d) {
			if (mCurseur + 1u >= mPile.Size()) {
				return false;
			}
			++mCurseur;
			return NkBpDepuisInstantane(mPile[mCurseur].CStr(), d);
		}

	} // namespace editeur
} // namespace nkentseu
