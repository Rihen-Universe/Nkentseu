// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Mesh/NkMeshFamilleFichier.cpp
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// L'INTERPRETE DES FAMILLES (Q16). Voir l'en-tete pour le format et pourquoi.
//
// ⚠️ IL N'A PAS SA PROPRE GEOMETRIE. Chaque operation appelle la MEME primitive
//    que le C++ appelait (`NkFamOpPave`, `NkFamOpChanfrein`, `NkFamOpTournee`,
//    `NkFamOpEmettre`, exposees par NkMeshFamilles.cpp). C'est ce qui rend
//    l'identite au bit possible : si l'interprete refaisait les cubes lui-meme,
//    « identique » ne voudrait rien dire -- on comparerait deux geometries qui
//    n'ont aucune raison de coincider.
//
// ⚠️ ET LE REFUS EST NOMME : fichier, ligne, et le mot fautif. Un interprete qui
//    dit « erreur de syntaxe » oblige a relire cinquante lignes.
// -----------------------------------------------------------------------------
#include "NKRenderer/Mesh/NkMeshFamilleFichier.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKRenderer/Mesh/NkMeshFamilles.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace renderer {

		// Les primitives, exposees par NkMeshFamilles.cpp (elles y vivent deja).
		void *NkFamOpNouvelleSoupe();
		void NkFamOpLibererSoupe(void *s);
		void NkFamOpPave(void *s, float32 x0, float32 y0, float32 z0, float32 x1, float32 y1, float32 z1);
		void NkFamOpChanfrein(void *s, float32 dx, float32 dy, float32 dz, float32 largeur, int32 segments);
		void NkFamOpPanneaux(void *s, float32 dx, float32 dy, float32 dz, int32 n, float32 bord, float32 creux);
		bool NkFamOpEmettre(void *sortie, void *s, const char *nom, const char *matiere);
		bool NkFamOpTournee(void *sortie, const float32 *rh, int32 n, const char *nom, const char *matiere, float32 tx,
							float32 ty, float32 tz, float32 rxDeg);
		void *NkFamOpSortie(NkVector<NkFamillePiece> *out);
		void NkFamOpLibererSortie(void *s);
		bool NkFamOpSortieOk(void *s);
		void NkFamOpLiaison(void *sortie, const char *parent, int32 liaison, float32 px, float32 py, float32 pz,
							float32 ax, float32 ay, float32 az, float32 bmin, float32 bmax);

		namespace {

			// ── LE TEXTE D'UNE FAMILLE, GARDE TEL QUEL ────────────────────────
			// On garde la SOURCE et on la rejoue a chaque construction. Garder un
			// arbre pre-analyse serait plus rapide et rendrait le rechargement a
			// chaud plus difficile a croire : ici, ce qui construit est exactement
			// ce que le fichier dit, a la ligne pres.
			struct Borne {
					char nom[24] = {0};
					float32 lo = 0.f, hi = 0.f, def = 0.f;
			};
			struct Fam {
					Borne bornes[8];
					int32 nBornes = 0;
					char nom[32] = {0};
					char fichier[260] = {0};
					char synonymes[256] = {0};
					char formulaire[400] = {0};
					char source[16000] = {0};
					bool cpp = false;            ///< `implementation cpp` : l'interprete ne la construit pas
					char pourquoiCpp[200] = {0}; ///< et le fichier DIT pourquoi
			};
			NkVector<Fam> &Table() {
				static NkVector<Fam> t;
				return t;
			}
			char gDossier[260] = {0};

			// ── UN EVALUATEUR D'EXPRESSIONS, ET RIEN DE PLUS ──────────────────
			// + - * / parentheses, unaire -, nombres, variables. Pas de fonctions,
			// pas d'appels : une recette de famille n'en a pas besoin, et chaque
			// construction du langage qu'on ajoute est une construction a prouver.
			struct Env {
					char noms[48][24];
					float32 vals[48];
					int32 n = 0;
					void Poser(const char *nom, float32 v) {
						for (int32 i = 0; i < n; ++i)
							if (strcmp(noms[i], nom) == 0) {
								vals[i] = v;
								return;
							}
						if (n < 48) {
							snprintf(noms[n], sizeof(noms[0]), "%s", nom);
							vals[n++] = v;
						}
					}
					bool Lire(const char *nom, float32 &v) const {
						for (int32 i = 0; i < n; ++i)
							if (strcmp(noms[i], nom) == 0) {
								v = vals[i];
								return true;
							}
						return false;
					}
			};

			struct Lex {
					const char *c;
					const Env *env;
					bool ok = true;
					char faute[64] = {0};
			};

			void Sauter(Lex &L) {
				while (*L.c == ' ' || *L.c == '\t')
					++L.c;
			}
			float32 Expr(Lex &L);

			float32 Primaire(Lex &L) {
				Sauter(L);
				if (*L.c == '(') {
					++L.c;
					const float32 v = Expr(L);
					Sauter(L);
					if (*L.c == ')')
						++L.c;
					return v;
				}
				if (*L.c == '-') {
					++L.c;
					return -Primaire(L);
				}
				if ((*L.c >= '0' && *L.c <= '9') || *L.c == '.') {
					char *fin = nullptr;
					const double d = strtod(L.c, &fin);
					L.c = fin;
					return (float32)d;
				}
				char nom[24];
				uint32 k = 0;
				while (((*L.c >= 'a' && *L.c <= 'z') || (*L.c >= 'A' && *L.c <= 'Z') || *L.c == '_' ||
						(k && *L.c >= '0' && *L.c <= '9')) &&
					   k + 1u < sizeof(nom))
					nom[k++] = *L.c++;
				nom[k] = 0;
				float32 v = 0.f;
				if (!k || !L.env->Lire(nom, v)) {
					L.ok = false;
					snprintf(L.faute, sizeof(L.faute), "%s", k ? nom : "(vide)");
				}
				return v;
			}

			float32 Terme(Lex &L) {
				float32 v = Primaire(L);
				for (;;) {
					Sauter(L);
					if (*L.c == '*') {
						++L.c;
						v *= Primaire(L);
					} else if (*L.c == '/') {
						++L.c;
						const float32 d = Primaire(L);
						v = (d != 0.f) ? v / d : 0.f;
					} else
						return v;
				}
			}

			float32 Expr(Lex &L) {
				float32 v = Terme(L);
				for (;;) {
					Sauter(L);
					if (*L.c == '+') {
						++L.c;
						v += Terme(L);
					} else if (*L.c == '-') {
						++L.c;
						v -= Terme(L);
					} else
						return v;
				}
			}

			/// Lit N expressions separees par des VIRGULES.
			/// ⚠️ LA VIRGULE EST OBLIGATOIRE, ET CE N'EST PAS UN GOUT. Separes par des
			///    espaces, `en -lx 0 -lz` se lit « -lx » puis « 0 - lz » : le moins du
			///    troisieme argument devient la soustraction du deuxieme, l'objet sort
			///    faux, et rien ne le signale. Une ambiguite qu'on contourne revient ;
			///    une virgule la supprime.
			bool LireN(const char *&c, const Env &env, float32 *out, int32 n, char *faute, uint32 capF) {
				for (int32 i = 0; i < n; ++i) {
					while (*c == ' ' || *c == 0x09 || *c == ',')
						++c;
					char seg[160];
					uint32 k = 0;
					while (*c && *c != ',' && k + 1u < sizeof(seg))
						seg[k++] = *c++;
					while (k > 0 && (seg[k - 1] == ' ' || seg[k - 1] == 0x09))
						--k;
					seg[k] = 0;
					if (!k) {
						snprintf(faute, capF, "argument %d manquant", (int)i + 1);
						return false;
					}
					Lex L{seg, &env};
					out[i] = Expr(L);
					Sauter(L);
					if (!L.ok || *L.c) {
						snprintf(faute, capF, "%s", L.ok ? seg : L.faute);
						return false;
					}
				}
				return true;
			}

			/// Le mot suivant (sans espaces ni virgules).
			bool Mot(const char *&c, char *out, uint32 cap) {
				while (*c == ' ' || *c == '\t' || *c == ',')
					++c;
				uint32 k = 0;
				while (*c && *c != ' ' && *c != '\t' && *c != ',' && *c != '\n' && *c != '\r' && k + 1u < cap)
					out[k++] = *c++;
				out[k] = 0;
				return k > 0;
			}

		} // namespace

		const char *NkFamilleFichierDossier() {
			return gDossier;
		}

		const char *NkFamilleFichierNom(int32 i) {
			NkVector<Fam> &t = Table();
			return (i >= 0 && i < (int32)t.Size()) ? t[(usize)i].nom : nullptr;
		}

		bool NkFamilleFichierConnue(const char *famille) {
			if (!famille)
				return false;
			NkVector<Fam> &t = Table();
			for (usize i = 0; i < t.Size(); ++i)
				if (strcmp(t[i].nom, famille) == 0 && !t[i].cpp)
					return true;
			return false;
		}

		bool NkFamilleFichierFormulaire(const char *famille, char *out, uint32 cap) {
			if (!famille || !out)
				return false;
			NkVector<Fam> &t = Table();
			for (usize i = 0; i < t.Size(); ++i)
				if (strcmp(t[i].nom, famille) == 0 && t[i].formulaire[0]) {
					snprintf(out, cap, "%s", t[i].formulaire);
					return true;
				}
			return false;
		}

		int32 NkFamilleFichierCharger(const char *dossier, char *pourquoi, uint32 capPourquoi) {
			if (pourquoi && capPourquoi)
				pourquoi[0] = 0;
			if (!dossier || !dossier[0])
				dossier = gDossier;
			if (!dossier || !dossier[0]) {
				if (pourquoi)
					snprintf(pourquoi, capPourquoi, "aucun dossier de familles n'a ete donne");
				return 0;
			}
			NkVector<NkString> fichiers = NkDirectory::GetFiles(dossier, "*.nkfam");
			if (fichiers.Size() == 0) {
				if (pourquoi)
					snprintf(pourquoi, capPourquoi, "aucun fichier .nkfam dans « %s »", dossier);
				return 0;
			}
			// ⚠️ ON CONSTRUIT A COTE, ET ON NE REMPLACE QU'A LA FIN. Un fichier
			//    invalide ne doit pas faire perdre les familles qui marchaient : ce
			//    serait le pire des deux mondes pour un rechargement a chaud.
			NkVector<Fam> neuve;
			int32 lus = 0;
			for (usize i = 0; i < fichiers.Size(); ++i) {
				const NkString txt = NkFile::ReadAllText(fichiers[i].CStr());
				if (!txt.Data() || !txt.Data()[0])
					continue;
				Fam f;
				snprintf(f.fichier, sizeof(f.fichier), "%s", fichiers[i].CStr());
				snprintf(f.source, sizeof(f.source), "%s", txt.Data());
				int32 ligne = 0;
				for (const char *c = f.source; *c;) {
					++ligne;
					char cle[24];
					const char *deb = c;
					while (*c && *c != '\n')
						++c;
					const char *fin = c;
					if (*c)
						++c;
					char l[600];
					const usize n = (usize)(fin - deb) < sizeof(l) - 1u ? (usize)(fin - deb) : sizeof(l) - 1u;
					memcpy(l, deb, n);
					l[n] = 0;
					const char *q = l;
					if (!Mot(q, cle, sizeof(cle)) || cle[0] == '#')
						continue;
					if (strcmp(cle, "famille") == 0)
						Mot(q, f.nom, sizeof(f.nom));
					else if (strcmp(cle, "synonymes") == 0)
						snprintf(f.synonymes, sizeof(f.synonymes), "%s", q);
					else if (strcmp(cle, "formulaire") == 0)
						snprintf(f.formulaire, sizeof(f.formulaire), "famille %s %s", f.nom, q);
					else if (strcmp(cle, "implementation") == 0) {
						char v[16];
						Mot(q, v, sizeof(v));
						f.cpp = (strcmp(v, "cpp") == 0);
					} else if (strcmp(cle, "pourquoi") == 0)
						snprintf(f.pourquoiCpp, sizeof(f.pourquoiCpp), "%s", q);
					else if (strcmp(cle, "borne") == 0 && f.nBornes < 8) {
						// ⚠️ LES BORNES SONT LUES *ET* APPLIQUEES. Les declarer sans les
						//    appliquer aurait donne un fichier qui a l'air juste et une
						//    geometrie differente du C++ : l'identite au bit aurait
						//    echoue sans qu'on sache pourquoi.
						Borne &b = f.bornes[f.nBornes];
						char v[32];
						Mot(q, b.nom, sizeof(b.nom));
						Mot(q, v, sizeof(v));
						b.lo = (float32)atof(v);
						Mot(q, v, sizeof(v));
						b.hi = (float32)atof(v);
						Mot(q, v, sizeof(v));
						b.def = (float32)atof(v);
						if (b.nom[0])
							++f.nBornes;
					}
				}
				if (!f.nom[0]) {
					if (pourquoi && !pourquoi[0])
						snprintf(pourquoi, capPourquoi, "%s : aucune ligne « famille <nom> »", f.fichier);
					continue;
				}
				neuve.PushBack(f);
				++lus;
			}
			if (lus == 0)
				return 0; // on garde la table precedente, et `pourquoi` dit quoi
			Table() = neuve;
			snprintf(gDossier, sizeof(gDossier), "%s", dossier);
			return lus;
		}

		// ── LA CONSTRUCTION ───────────────────────────────────────────────────
		int32 NkFamilleFichierConstruire(const NkFamilleParams &p, NkVector<NkFamillePiece> &out, char *pourquoi,
										 uint32 capPourquoi) {
			const Fam *f = nullptr;
			NkVector<Fam> &t = Table();
			for (usize i = 0; i < t.Size(); ++i)
				if (strcmp(t[i].nom, p.famille) == 0)
					f = &t[i];
			if (!f) {
				if (pourquoi)
					snprintf(pourquoi, capPourquoi, "famille « %s » : aucun fichier .nkfam", p.famille);
				return -1;
			}
			if (f->cpp) {
				if (pourquoi)
					snprintf(pourquoi, capPourquoi, "%s : construite en C++ (%s)", f->nom,
							 f->pourquoiCpp[0] ? f->pourquoiCpp : "raison non ecrite dans le fichier");
				return -1;
			}

			// LES BORNES, APPLIQUEES COMME LE C++ : une valeur absente ou negative
			// prend le defaut ; une valeur hors bornes est ramenee.
			auto borner = [&](const char *nom, float32 v) {
				for (int32 i = 0; i < f->nBornes; ++i) {
					if (strcmp(f->bornes[i].nom, nom) != 0)
						continue;
					if (!(v > 0.f))
						return f->bornes[i].def;
					return v < f->bornes[i].lo ? f->bornes[i].lo : (v > f->bornes[i].hi ? f->bornes[i].hi : v);
				}
				return v;
			};
			Env env;
			env.Poser("largeur", borner("largeur", p.largeur));
			env.Poser("hauteur", borner("hauteur", p.hauteur));
			env.Poser("profondeur", borner("profondeur", p.profondeur));
			env.Poser("nombre", (float32)p.nombre);
			env.Poser("fenetres", (float32)p.fenetres);
			env.Poser("detaille", p.detaille ? 1.f : 0.f);
			env.Poser("cote", 1.f); // hors d'un bloc miroir, il n'y a qu'un cote

			void *S = NkFamOpSortie(&out);
			void *soupe = nullptr;
			char nomPiece[24] = {0}, matPiece[24] = {0};
			bool dansPiece = false;
			// ── LE MIROIR : LA SYMETRIE EST UNE CONSTRUCTION (25/09) ──────────
			// ⚠️ ET PAS UNE VERIFICATION. Verifier la symetrie apres coup laisse
			//    passer les asymetries d'arrondi, et oblige a ecrire deux fois le
			//    meme membre -- deux ecritures qui divergeront. Ici, le bloc
			//    `miroir ... fin` est EXECUTE DEUX FOIS : `cote` vaut +1 puis -1, et
			//    les pieces recoivent le suffixe `_g` puis `_d`. L'auteur ecrit un
			//    seul bras.
			const char *miroirDebut = nullptr;
			int32 miroirPasse = 0; // 0 hors bloc, 1 premiere passe, 2 seconde
			int32 miroirLigne = 0;
			int32 nPieces = 0, ligne = 0;
			// La pile de conditions : `si` / `sinon` / `fin`. Trois niveaux suffisent
			// pour ces recettes, et une pile plus profonde serait une recette a
			// relire plutot qu'un interprete a agrandir.
			bool actif[4] = {true, true, true, true};
			int32 prof = 0;
			char faute[64] = {0};
			auto refus = [&](const char *quoi) {
				if (pourquoi)
					snprintf(pourquoi, capPourquoi, "%s, ligne %d : %s", f->fichier, (int)ligne, quoi);
			};

			for (const char *c = f->source; *c;) {
				++ligne;
				const char *deb = c;
				while (*c && *c != '\n')
					++c;
				const char *fin = c;
				if (*c)
					++c;
				char l[600];
				const usize n = (usize)(fin - deb) < sizeof(l) - 1u ? (usize)(fin - deb) : sizeof(l) - 1u;
				memcpy(l, deb, n);
				l[n] = 0;
				const char *q = l;
				char cle[24];
				if (!Mot(q, cle, sizeof(cle)) || cle[0] == '#')
					continue;

				// ── LES MOTS DECLARATIFS SONT LUS AU CHARGEMENT, PAS ICI ──────
				// `famille`, `borne`, `formulaire`... decrivent la famille ; la boucle
				// de construction les rencontre et doit les SAUTER. Sans cela, le
				// premier `borne` faisait « mot-cle inconnu, ligne 10 » -- et comme le
				// repli C++ rendait quand meme le bon objet, le refus ne se voyait pas.
				static const char *const kDeclaratifs[] = {"famille",  "synonymes",      "formulaire", "borne",
														   "plausible", "valeurs",       "implementation",
														   "pourquoi",  "manque"};
				{
					bool decl = false;
					for (const char *d : kDeclaratifs)
						if (strcmp(d, cle) == 0)
							decl = true;
					if (decl)
						continue;
				}

				if (strcmp(cle, "miroir") == 0) {
					miroirDebut = c; // juste apres cette ligne
					miroirLigne = ligne;
					miroirPasse = 1;
					env.Poser("cote", 1.f);
					continue;
				}

				// ── le controle de flot d'abord : il vaut meme quand on saute ──
				if (strcmp(cle, "si") == 0) {
					float32 v = 0.f;
					if (!LireN(q, env, &v, 1, faute, sizeof(faute))) {
						refus(faute);
						NkFamOpLibererSortie(S);
						return -1;
					}
					if (prof + 1 < 4) {
						++prof;
						actif[prof] = actif[prof - 1] && (v != 0.f);
					}
					continue;
				}
				if (strcmp(cle, "sinon") == 0) {
					if (prof > 0)
						actif[prof] = actif[prof - 1] && !actif[prof];
					continue;
				}
				// ── `piece` EST LU MEME QUAND ON SAUTE, ET C'EST LE CORRECTIF ──
				// Sinon son `fin` est pris pour celui d'un `si`, et la pile des
				// conditions se decale : les quatre pieds de la branche `sinon`
				// refermaient le `si detaille`, et les deux branches sortaient. Mesure
				// avant correctif : 9 pieces et 5 464 sommets contre 6 et 5 392.
				if (strcmp(cle, "piece") == 0) {
					dansPiece = true;
					Mot(q, nomPiece, sizeof(nomPiece));
					if (miroirPasse) {
						const usize ln = strlen(nomPiece);
						if (ln + 2u < sizeof(nomPiece)) {
							nomPiece[ln] = '_';
							nomPiece[ln + 1] = (miroirPasse == 1) ? 'g' : 'd';
							nomPiece[ln + 2] = 0;
						}
					}
					char mot[16];
					Mot(q, mot, sizeof(mot)); // « matiere »
					Mot(q, matPiece, sizeof(matPiece));
					soupe = actif[prof] ? NkFamOpNouvelleSoupe() : nullptr;
					continue;
				}
				if (strcmp(cle, "fin") == 0) {
					if (dansPiece) { // `fin` d'une piece
						dansPiece = false;
						if (soupe) {
							if (!NkFamOpEmettre(S, soupe, nomPiece, matPiece)) {
								refus("la piece n'a pas pu etre emise");
								NkFamOpLibererSoupe(soupe);
								NkFamOpLibererSortie(S);
								return -1;
							}
							NkFamOpLibererSoupe(soupe);
							soupe = nullptr;
							++nPieces;
						}
					} else if (miroirPasse == 1 && prof == 0) {
						// fin du bloc miroir, premiere passe : on rembobine
						miroirPasse = 2;
						env.Poser("cote", -1.f);
						c = miroirDebut;
						ligne = miroirLigne;
						continue;
					} else if (miroirPasse == 2 && prof == 0) {
						miroirPasse = 0;
						miroirDebut = nullptr;
						continue;
					} else if (prof > 0)
						--prof;
					continue;
				}
				if (!actif[prof])
					continue;

				if (strcmp(cle, "var") == 0) {
					char nom[24];
					if (!Mot(q, nom, sizeof(nom))) {
						refus("« var » attend un nom");
						NkFamOpLibererSortie(S);
						return -1;
					}
					while (*q == ' ' || *q == '=')
						++q;
					float32 v = 0.f;
					if (!LireN(q, env, &v, 1, faute, sizeof(faute))) {
						refus(faute);
						NkFamOpLibererSortie(S);
						return -1;
					}
					env.Poser(nom, v);
				} else if (strcmp(cle, "pave") == 0) {
					float32 a[6];
					if (!soupe || !LireN(q, env, a, 6, faute, sizeof(faute))) {
						refus(soupe ? faute : "« pave » hors d'une piece");
						NkFamOpLibererSortie(S);
						return -1;
					}
					NkFamOpPave(soupe, a[0], a[1], a[2], a[3], a[4], a[5]);
				} else if (strcmp(cle, "chanfrein") == 0) {
					float32 a[5];
					if (!soupe || !LireN(q, env, a, 5, faute, sizeof(faute))) {
						refus(soupe ? faute : "« chanfrein » hors d'une piece");
						NkFamOpLibererSortie(S);
						return -1;
					}
					NkFamOpChanfrein(soupe, a[0], a[1], a[2], a[3], (int32)(a[4] + 0.5f));
				} else if (strcmp(cle, "panneaux") == 0) {
					float32 a[6];
					if (!soupe || !LireN(q, env, a, 6, faute, sizeof(faute))) {
						refus(soupe ? faute : "« panneaux » hors d'une piece");
						NkFamOpLibererSortie(S);
						return -1;
					}
					NkFamOpPanneaux(soupe, a[0], a[1], a[2], (int32)(a[3] + 0.5f), a[4], a[5]);
				} else if (strcmp(cle, "tournee") == 0) {
					// tournee <nom> matiere <mat> en <x> <y> <z> profil <r h>...
					char nom[24], mot[16], mat[24];
					Mot(q, nom, sizeof(nom));
					if (miroirPasse) {
						const usize ln = strlen(nom);
						if (ln + 2u < sizeof(nom)) {
							nom[ln] = '_';
							nom[ln + 1] = (miroirPasse == 1) ? 'g' : 'd';
							nom[ln + 2] = 0;
						}
					}
					Mot(q, mot, sizeof(mot));
					Mot(q, mat, sizeof(mat));
					Mot(q, mot, sizeof(mot)); // « en »
					float32 tr[3];
					if (!LireN(q, env, tr, 3, faute, sizeof(faute))) {
						refus(faute);
						NkFamOpLibererSortie(S);
						return -1;
					}
					Mot(q, mot, sizeof(mot)); // « profil »
					float32 rh[32];
					int32 np = 0;
					while (np < 16) {
						while (*q == ' ' || *q == '\t' || *q == ',')
							++q;
						if (!*q)
							break;
						if (!LireN(q, env, rh + 2 * np, 2, faute, sizeof(faute))) {
							refus(faute);
							NkFamOpLibererSortie(S);
							return -1;
						}
						++np;
					}
					if (np < 2) {
						refus("« tournee » demande au moins deux couples rayon/hauteur");
						NkFamOpLibererSortie(S);
						return -1;
					}
					if (!NkFamOpTournee(S, rh, np, nom, mat, tr[0], tr[1], tr[2], 0.f)) {
						refus("la revolution a refuse ce profil");
						NkFamOpLibererSortie(S);
						return -1;
					}
					++nPieces;
				} else if (strcmp(cle, "liaison") == 0) {
					// liaison <parent> charniere pivot x y z axe x y z butees a b
					char parent[24], mot[16];
					Mot(q, parent, sizeof(parent));
					Mot(q, mot, sizeof(mot)); // charniere | glissiere | fixe
					const int32 typ = (strcmp(mot, "charniere") == 0) ? 1 : ((strcmp(mot, "glissiere") == 0) ? 2 : 0);
					float32 a[8] = {0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f};
					Mot(q, mot, sizeof(mot)); // « pivot »
					if (!LireN(q, env, a, 3, faute, sizeof(faute))) {
						refus(faute);
						NkFamOpLibererSortie(S);
						return -1;
					}
					Mot(q, mot, sizeof(mot)); // « axe »
					if (!LireN(q, env, a + 3, 3, faute, sizeof(faute))) {
						refus(faute);
						NkFamOpLibererSortie(S);
						return -1;
					}
					Mot(q, mot, sizeof(mot)); // « butees »
					(void)LireN(q, env, a + 6, 2, faute, sizeof(faute));
					NkFamOpLiaison(S, parent, typ, a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
				} else {
					refus("mot-cle inconnu");
					if (soupe)
						NkFamOpLibererSoupe(soupe);
					NkFamOpLibererSortie(S);
					return -1;
				}
			}
			if (soupe)
				NkFamOpLibererSoupe(soupe);
			const bool ok = NkFamOpSortieOk(S);
			NkFamOpLibererSortie(S);
			if (!ok) {
				refus("la construction a echoue");
				return -1;
			}
			return nPieces;
		}

	} // namespace renderer
} // namespace nkentseu
