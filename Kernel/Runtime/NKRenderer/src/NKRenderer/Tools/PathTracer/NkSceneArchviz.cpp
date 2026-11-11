// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/PathTracer/NkSceneArchviz.cpp
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// La description de scene commune (voir l'en-tete). Aucune bibliotheque standard.

#include "NKRenderer/Tools/PathTracer/NkSceneArchviz.h"
#include "NKRenderer/Tools/PathTracer/NkMatieresProcedurales.h"
#include "NKRenderer/Materials/NkPeintureCarrosserie.h"
#include "NKMemory/NkAllocator.h"
#include "NKPlatform/NkEnv.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/Text/NkSnprintf.h"
#include "NKCore/Text/NkSortie.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu {
	namespace renderer {

		namespace {
			// ── petits outils de texte ──────────────────────────────────────────
			bool Egal(const char *a, const char *b) {
				while (*a && *a == *b) {
					++a;
					++b;
				}
				return *a == *b;
			}
			bool Commence(const char *a, const char *pref) {
				while (*pref) {
					if (*a != *pref)
						return false;
					++a;
					++pref;
				}
				return true;
			}
			bool Contient(const char *a, const char *m) {
				for (const char *p = a; *p; ++p) {
					const char *x = p, *y = m;
					while (*y && *x == *y) {
						++x;
						++y;
					}
					if (!*y)
						return true;
				}
				return false;
			}
			void Copier(char *d, int32 cap, const char *s, int32 n = -1) {
				int32 i = 0;
				while (s[i] && i < cap - 1 && (n < 0 || i < n)) {
					d[i] = s[i];
					++i;
				}
				d[i] = 0;
			}
			const char *Blancs(const char *c) {
				while (*c == ' ' || *c == '\t')
					++c;
				return c;
			}
			const char *Mot(const char *c, char *out, int32 cap) {
				c = Blancs(c);
				int32 n = 0;
				while (*c && *c != ' ' && *c != '\t' && *c != '\r' && *c != '\n') {
					if (n < cap - 1)
						out[n++] = *c;
					++c;
				}
				out[n] = 0;
				return c;
			}
			const char *Ligne(const char *c, char *out, int32 cap) {
				int32 n = 0;
				while (*c && *c != '\n') {
					if (n < cap - 1 && *c != '\r')
						out[n++] = *c;
					++c;
				}
				out[n] = 0;
				return *c ? c + 1 : c;
			}
			const char *Reel(const char *c, float64 &out) {
				c = Blancs(c);
				float64 s = 1.0, v = 0.0;
				if (*c == '-') {
					s = -1.0;
					++c;
				} else if (*c == '+')
					++c;
				while (*c >= '0' && *c <= '9')
					v = v * 10.0 + (float64)(*c++ - '0');
				if (*c == '.') {
					++c;
					float64 f = 0.1;
					while (*c >= '0' && *c <= '9') {
						v += f * (float64)(*c++ - '0');
						f *= 0.1;
					}
				}
				if (*c == 'e' || *c == 'E') {
					++c;
					int32 es = 1, e = 0;
					if (*c == '-') {
						es = -1;
						++c;
					} else if (*c == '+')
						++c;
					while (*c >= '0' && *c <= '9')
						e = e * 10 + (*c++ - '0');
					v *= math::NkPow(10.0, (float64)(es * e));
				}
				out = s * v;
				return c;
			}
			const char *Reel32(const char *c, float32 &out) {
				float64 d = 0.0;
				c = Reel(c, d);
				out = (float32)d;
				return c;
			}
			const char *Entier(const char *c, int32 &out) {
				c = Blancs(c);
				int32 s = 1, v = 0;
				if (*c == '-') {
					s = -1;
					++c;
				}
				while (*c >= '0' && *c <= '9')
					v = v * 10 + (*c++ - '0');
				out = s * v;
				return c;
			}

			// =====================================================================
			//  JSON : un arbre dans un tableau ; chaines decodees dans un reservoir
			// =====================================================================
			struct JNoeud {
					int32 t = 0; // 0 nul, 1 booleen, 2 nombre, 3 chaine, 4 tableau, 5 objet
					float64 n = 0.0;
					int32 so = 0; // chaine : debut dans le reservoir
					int32 premier = 0, nb = 0; // enfants (objet : cle, valeur, cle, valeur...)
			};
			struct Json {
					NkVector<JNoeud> v;
					NkVector<int32> liens;
					NkVector<char> res;
					const char *c = nullptr;
					bool ok = true;

					void Espaces() {
						while (*c == ' ' || *c == '\t' || *c == '\n' || *c == '\r')
							++c;
					}
					int32 Chaine() {
						JNoeud j;
						j.t = 3;
						j.so = (int32)res.Size();
						++c; // "
						while (*c && *c != '"') {
							char x = *c++;
							if (x == '\\' && *c) {
								const char e = *c++;
								x = e == 'n' ? '\n' : (e == 't' ? '\t' : e);
								if (e == 'u') { // \uXXXX : remplace par '?'
									for (int32 k = 0; k < 4 && *c; ++k)
										++c;
									x = '?';
								}
							}
							res.PushBack(x);
						}
						res.PushBack(0);
						if (*c == '"')
							++c;
						else
							ok = false;
						v.PushBack(j);
						return (int32)v.Size() - 1;
					}
					int32 Valeur() {
						Espaces();
						if (*c == '"')
							return Chaine();
						if (*c == '{' || *c == '[') {
							const bool obj = *c == '{';
							const char fin = obj ? '}' : ']';
							++c;
							NkVector<int32> enf;
							Espaces();
							while (*c && *c != fin && ok) {
								if (obj) {
									Espaces();
									if (*c != '"') {
										ok = false;
										break;
									}
									enf.PushBack(Chaine());
									Espaces();
									if (*c == ':')
										++c;
								}
								enf.PushBack(Valeur());
								Espaces();
								if (*c == ',')
									++c;
								Espaces();
							}
							if (*c == fin)
								++c;
							else
								ok = false;
							JNoeud j;
							j.t = obj ? 5 : 4;
							j.premier = (int32)liens.Size();
							j.nb = (int32)enf.Size();
							for (usize k = 0; k < enf.Size(); ++k)
								liens.PushBack(enf[k]);
							v.PushBack(j);
							return (int32)v.Size() - 1;
						}
						JNoeud j;
						if (*c == 't' || *c == 'f') {
							j.t = 1;
							j.n = *c == 't' ? 1.0 : 0.0;
							while (*c >= 'a' && *c <= 'z')
								++c;
						} else if (*c == 'n') {
							j.t = 0;
							while (*c >= 'a' && *c <= 'z')
								++c;
						} else {
							j.t = 2;
							const char *a = c;
							c = Reel(c, j.n);
							if (c == a) {
								ok = false;
								++c;
							}
						}
						v.PushBack(j);
						return (int32)v.Size() - 1;
					}
					int32 Lire(const char *texte) {
						c = texte;
						return Valeur();
					}
					const char *Txt(int32 i) const {
						return (i >= 0 && v[(usize)i].t == 3) ? &res[(usize)v[(usize)i].so] : "";
					}
					float64 Num(int32 i, float64 d = 0.0) const {
						return (i >= 0 && (v[(usize)i].t == 2 || v[(usize)i].t == 1)) ? v[(usize)i].n : d;
					}
					int32 Nb(int32 i) const {
						if (i < 0)
							return 0;
						const JNoeud &j = v[(usize)i];
						return j.t == 5 ? j.nb / 2 : (j.t == 4 ? j.nb : 0);
					}
					int32 Elem(int32 i, int32 k) const { // k-ieme element d'un tableau
						return (i >= 0 && v[(usize)i].t == 4 && k < v[(usize)i].nb) ? liens[(usize)(v[(usize)i].premier + k)] : -1;
					}
					int32 Cle(int32 i, int32 k) const { // k-ieme cle d'un objet
						return (i >= 0 && v[(usize)i].t == 5) ? liens[(usize)(v[(usize)i].premier + 2 * k)] : -1;
					}
					int32 ValK(int32 i, int32 k) const {
						return (i >= 0 && v[(usize)i].t == 5) ? liens[(usize)(v[(usize)i].premier + 2 * k + 1)] : -1;
					}
					int32 Get(int32 i, const char *cle) const {
						for (int32 k = 0; k < Nb(i); ++k)
							if (Egal(Txt(Cle(i, k)), cle))
								return ValK(i, k);
						return -1;
					}
					void Vec3(int32 i, float32 *o) const {
						for (int32 k = 0; k < 3; ++k)
							o[k] = (float32)Num(Elem(i, k), o[k]);
					}
			};

			// =====================================================================
			//  REGEX : ^ $ | ( ) . [..] \d, sans quantificateur, recherche
			// =====================================================================
			struct RNoeud {
					int32 t = 0;		// 0 car, 1 point, 2 classe, 3 chiffre, 4 debut, 5 fin, 6 groupe
					char ch = 0;
					uint8 cl[32] = {0}; // classe : 256 bits
					int32 alt = -1;		// groupe : l'alternative
			};
			struct Regex {
					NkVector<RNoeud> n;
					NkVector<NkVector<int32>> seqs; // une sequence = des noeuds
					NkVector<NkVector<int32>> alts; // une alternative = des sequences
					int32 racine = -1;
					const char *p = nullptr;
					const char *texte = nullptr;
					int32 lg = 0;

					int32 Alt() {
						NkVector<int32> a;
						a.PushBack(Seq());
						while (*p == '|') {
							++p;
							a.PushBack(Seq());
						}
						alts.PushBack(a);
						return (int32)alts.Size() - 1;
					}
					int32 Seq() {
						NkVector<int32> s;
						while (*p && *p != '|' && *p != ')') {
							RNoeud r;
							const char x = *p++;
							if (x == '(') {
								r.t = 6;
								r.alt = Alt();
								if (*p == ')')
									++p;
							} else if (x == '.')
								r.t = 1;
							else if (x == '^')
								r.t = 4;
							else if (x == '$')
								r.t = 5;
							else if (x == '\\' && *p) {
								const char e = *p++;
								if (e == 'd')
									r.t = 3;
								else {
									r.t = 0;
									r.ch = e;
								}
							} else if (x == '[') {
								r.t = 2;
								while (*p && *p != ']') {
									const uint8 a = (uint8)*p++;
									uint8 b = a;
									if (*p == '-' && p[1] && p[1] != ']') {
										b = (uint8)p[1];
										p += 2;
									}
									for (int32 k = a; k <= b; ++k)
										r.cl[k >> 3] |= (uint8)(1u << (k & 7));
								}
								if (*p == ']')
									++p;
							} else {
								r.t = 0;
								r.ch = x;
							}
							n.PushBack(r);
							s.PushBack((int32)n.Size() - 1);
						}
						seqs.PushBack(s);
						return (int32)seqs.Size() - 1;
					}
					void Compiler(const char *motif) {
						p = motif;
						racine = Alt();
					}
					struct Suite {
							int32 seq, idx;
							const Suite *apres;
					};
					bool Seq(int32 s, int32 idx, int32 pos, const Suite *k) const {
						const NkVector<int32> &S = seqs[(usize)s];
						if (idx >= (int32)S.Size())
							return k ? Seq(k->seq, k->idx, pos, k->apres) : true;
						const RNoeud &r = n[(usize)S[(usize)idx]];
						switch (r.t) {
							case 4:
								return pos == 0 && Seq(s, idx + 1, pos, k);
							case 5:
								return pos == lg && Seq(s, idx + 1, pos, k);
							case 6: {
								const Suite c{s, idx + 1, k};
								const NkVector<int32> &A = alts[(usize)r.alt];
								for (usize a = 0; a < A.Size(); ++a)
									if (Seq(A[a], 0, pos, &c))
										return true;
								return false;
							}
							default: {
								if (pos >= lg)
									return false;
								const uint8 x = (uint8)texte[pos];
								bool ok = false;
								if (r.t == 0)
									ok = x == (uint8)r.ch;
								else if (r.t == 1)
									ok = true;
								else if (r.t == 3)
									ok = x >= '0' && x <= '9';
								else
									ok = (r.cl[x >> 3] >> (x & 7)) & 1u;
								return ok && Seq(s, idx + 1, pos + 1, k);
							}
						}
					}
					bool Cherche(const char *t) {
						texte = t;
						lg = 0;
						while (t[lg])
							++lg;
						const NkVector<int32> &A = alts[(usize)racine];
						for (int32 d = 0; d <= lg; ++d)
							for (usize a = 0; a < A.Size(); ++a)
								if (Seq(A[a], 0, d, nullptr))
									return true;
						return false;
					}
			};

			// la classe de carte procedurale d'une matiere nommee de l'archviz
			int32 ClasseDe(const char *nom, float32 &tuile) {
				struct C {
						const char *mot;
						int32 classe;
				};
				static const C k[] = {{"enduit", kMatEnduit},	   {"laque", kMatPeinture},	   {"peinture", kMatPeinture},
									  {"beton", kMatBeton},		   {"mortier", kMatBeton},	   {"pierre", kMatPierre},
									  {"brique", kMatPierre},	   {"parquet", kMatBois},	   {"bois", kMatBois},
									  {"carrelage", kMatCarrelage}, {"carreau", kMatCarrelage}, {"ceramique", kMatCeramique},
									  {"marbre", kMatCeramique},   {"quartz", kMatCeramique},  {"metal", kMatMetal},
									  {"laiton", kMatMetal},	   {"plante", kMatFeuillage},  {"gazon", kMatHerbe},
									  {"terre_vegetale", kMatHerbe}, {"cuir", kMatCuir},
									  // (10/11/2026, pour le film de l'archviz) les ETOFFES et les PLACAGES ont
									  // leurs classes : velours AVANT textile (« textile_velours_vert » contient
									  // les deux) ; toile, laine, voilage, abat-jour : la trame ; placage : le fil
									  // continu des grands panneaux (le bois, lui, reste en lames)
									  {"velours", kMatVelours},	   {"textile", kMatTissu},	   {"tissu", kMatTissu},
									  {"laine", kMatTissu},		   {"voilage", kMatTissu},	   {"abat_jour", kMatTissu},
									  {"placage", kMatPlacage},	   {"terre_cuite", kMatPierre}};
				for (usize i = 0; i < sizeof(k) / sizeof(k[0]); ++i)
					if (Contient(nom, k[i].mot)) {
						tuile = NkMatiereDe(k[i].classe).tuileM;
						return k[i].classe;
					}
				tuile = 1.f;
				return -1;
			}

			struct Doc {
					char scene[96];
					char doc[96];
			};
			struct Piece {
					char fichier[520];
					float32 o[3];
					char doc[96];
					// (10/11/2026) LA ROTATION de la partie (`rotation rx ry rz`, degres) : R = Rz Ry Rx, autour
					// de l'origine de la piece, puis la translation `origine` -- la convention de l'atelier
					// (HostSetNodeEuler, Z*Y*X ; l'export d'essai_rotation : y 90 donne x' = z, z' = -x)
					float32 R[9];
					bool tourne;
			};
		} // namespace

		// =====================================================================
		//  (10/11/2026) L'ATTRIBUTION DES MATIERES, partagee par le traceur et le temps reel
		//  (le code est celui de NkScArchvizCharger, deplace ici tel quel)
		// =====================================================================
		struct NkScAttribution::Interne {
				Json jm;
				int32 jpieces = -1;
				struct Regle {
						Regex doc, piece;
						int32 mat;
				};
				NkVector<Regle> regles;
				NkVector<Doc> docs;
		};

		NkScAttribution::~NkScAttribution() {
			Fermer();
		}

		void NkScAttribution::Fermer() {
			if (mI) {
				mI->~Interne();
				memory::NkFree(mI);
				mI = nullptr;
			}
			mats.Clear();
			nBib = parNom = parRegle = parMtl = 0;
			jsonLu = false;
		}

		bool NkScAttribution::Ouvrir(const char *assemblage, const char *matieres) {
			Fermer();
			void *place = memory::NkAlloc((nk_size)sizeof(Interne));
			if (!place)
				return false;
			mI = new (place) Interne();
			// ── les documents : `scene X` de <dossier>/<document>.nkscene ──────
			char dossier[520];
			Copier(dossier, 520, assemblage ? assemblage : "");
			{
				int32 n = 0, der = -1;
				while (dossier[n]) {
					if (dossier[n] == '/' || dossier[n] == '\\')
						der = n;
					++n;
				}
				if (der >= 0)
					dossier[der] = 0;
				else
					Copier(dossier, 520, ".");
			}
			{
				const NkVector<NkString> fs = NkDirectory::GetFiles(dossier, "*.nkscene");
				for (usize i = 0; i < fs.Size(); ++i) {
					const char *chemin = fs[i].CStr();
					char cheminComplet[600];
					// GetFiles peut rendre un nom seul ou un chemin : on accepte les deux
					if (Contient(chemin, "/") || Contient(chemin, "\\"))
						Copier(cheminComplet, 600, chemin);
					else
						NkSnprintf(cheminComplet, sizeof(cheminComplet), "%s/%s", dossier, chemin);
					const NkString t = NkFile::ReadAllText(cheminComplet);
					const char *c = t.CStr();
					char l[512], mot[64];
					for (int32 k = 0; k < 12 && c && *c; ++k) {
						c = Ligne(c, l, 512);
						const char *q = Mot(l, mot, 64);
						if (Egal(mot, "scene")) {
							Doc d;
							(void)Mot(q, d.scene, 96);
							// le document : le nom du fichier sans dossier ni extension
							const char *b = cheminComplet;
							for (const char *x = cheminComplet; *x; ++x)
								if (*x == '/' || *x == '\\')
									b = x + 1;
							int32 n = 0;
							while (b[n] && !(b[n] == '.' && Egal(b + n, ".nkscene")))
								++n;
							Copier(d.doc, 96, b, n);
							mI->docs.PushBack(d);
							break;
						}
					}
				}
			}
			// ── les matieres ───────────────────────────────────────────────────
			Json &jm = mI->jm;
			const NkString tm = NkFile::ReadAllText(matieres ? matieres : "");
			const int32 rm = tm.Empty() ? -1 : jm.Lire(tm.CStr());
			const int32 bib = jm.Get(rm, "bibliotheque"), jregles = jm.Get(rm, "regles");
			mI->jpieces = jm.Get(rm, "pieces");
			for (int32 k = 0; k < jm.Nb(bib); ++k) {
				NkScMateriau m;
				Copier(m.nom, 48, jm.Txt(jm.Cle(bib, k)));
				const int32 v = jm.ValK(bib, k);
				jm.Vec3(jm.Get(v, "couleur"), m.couleur);
				m.rugosite = (float32)jm.Num(jm.Get(v, "rugosite"), 0.8);
				m.metal = (float32)jm.Num(jm.Get(v, "metal"), 0.0);
				m.transmission = (float32)jm.Num(jm.Get(v, "transmission"), 0.0);
				m.emission = (float32)jm.Num(jm.Get(v, "emission"), 0.0);
				m.classe = (m.transmission > 0.5f || m.emission > 0.f) ? -1 : ClasseDe(m.nom, m.tuile);
				// (10/11) le LUSTRE des fibres : le velours le porte pleinement (reflet rasant serre), une
				// toile un peu ; « lustre » dans la bibliotheque l'emporte s'il est ecrit
				if (m.classe == kMatVelours) {
					m.lustre = 1.f;
					m.lustreRug = 0.3f;
				} else if (m.classe == kMatTissu) {
					m.lustre = 0.35f;
					m.lustreRug = 0.5f;
				}
				m.lustre = (float32)jm.Num(jm.Get(v, "lustre"), (float64)m.lustre);
				// (10/11) la peinture de carrosserie (agent vehicules : « vernis », « paillettes » ; la
				// rugosite du vernis, la densite ou la taille des paillettes s'il les ecrit) : les
				// parametres du type NK_CAR_PAINT (NkPeintureCarrosserie.h). Densite par defaut 1 : les
				// paillettes les plus fines (0,5 mm).
				m.vernis = (float32)jm.Num(jm.Get(v, "vernis"), 0.0);
				m.vernisRug = (float32)jm.Num(jm.Get(v, "vernis_rugosite"), 0.03);
				m.paillettes = (float32)jm.Num(jm.Get(v, "paillettes"), 0.0);
				m.paillettesDensite = (float32)jm.Num(jm.Get(v, "paillettes_densite"), 1.0);
				if (jm.Get(v, "paillettes_taille_mm") >= 0)
					m.paillettesDensite = NkCpDensiteDepuisTaille((float32)(jm.Num(jm.Get(v, "paillettes_taille_mm"), 0.5) * 1e-3));
				m.vernis = m.vernis < 0.f ? 0.f : (m.vernis > 1.f ? 1.f : m.vernis);
				m.paillettes = m.paillettes < 0.f ? 0.f : (m.paillettes > 1.f ? 1.f : m.paillettes);
				m.paillettesDensite = m.paillettesDensite < 0.f ? 0.f : (m.paillettesDensite > 1.f ? 1.f : m.paillettesDensite);
				mats.PushBack(m);
			}
			nBib = (int32)mats.Size();
			// regles compilees une fois
			mI->regles.Resize((usize)jm.Nb(jregles));
			for (int32 k = 0; k < jm.Nb(jregles); ++k) {
				const int32 r = jm.Elem(jregles, k);
				mI->regles[(usize)k].doc.Compiler(jm.Txt(jm.Elem(r, 0)));
				mI->regles[(usize)k].piece.Compiler(jm.Txt(jm.Elem(r, 1)));
				mI->regles[(usize)k].mat = -1;
				const char *nm = jm.Txt(jm.Elem(r, 2));
				for (int32 i = 0; i < nBib; ++i)
					if (Egal(mats[(usize)i].nom, nm)) {
						mI->regles[(usize)k].mat = i;
						break;
					}
			}
			jsonLu = rm >= 0 && jm.ok;
			return rm >= 0 || !mI->docs.Empty();
		}

		void NkScAttribution::Document(const char *scene, char *doc, int32 cap) const {
			if (cap <= 0)
				return;
			// `doc` entre avec le document courant de la piece (vide au debut) : un objet inconnu le garde
			// (la regle de NkScArchvizCharger depuis le 09/11)
			if (mI)
				for (usize d = 0; d < mI->docs.Size(); ++d)
					if (Egal(mI->docs[d].scene, scene))
						Copier(doc, cap, mI->docs[d].doc);
			if (!doc[0])
				Copier(doc, cap, scene);
		}

		int32 NkScAttribution::Index(const char *nom) const {
			for (int32 i = 0; i < nBib; ++i)
				if (Egal(mats[(usize)i].nom, nom))
					return i;
			return -1;
		}

		int32 NkScAttribution::Repli(const float32 *kd) {
			// matieres de repli (teinte du .mtl), une par teinte
			for (usize i = (usize)nBib; i < mats.Size(); ++i)
				if (mats[i].couleur[0] == kd[0] && mats[i].couleur[1] == kd[1] && mats[i].couleur[2] == kd[2])
					return (int32)i;
			NkScMateriau m;
			NkSnprintf(m.nom, sizeof(m.nom), "repli_%d", (int)(mats.Size() - (usize)nBib));
			m.couleur[0] = kd[0];
			m.couleur[1] = kd[1];
			m.couleur[2] = kd[2];
			m.classe = kMatEnduit;
			m.tuile = NkMatiereDe(kMatEnduit).tuileM;
			mats.PushBack(m);
			return (int32)mats.Size() - 1;
		}

		int32 NkScAttribution::Matiere(const char *doc, const char *piece, const float32 *kd) {
			if (mI) {
				Json &jm = mI->jm;
				const int32 dp = jm.Get(mI->jpieces, doc);
				if (dp >= 0) {
					const int32 v = jm.Get(dp, piece);
					if (v >= 0) {
						const int32 i = Index(jm.Txt(v));
						if (i >= 0) {
							++parNom;
							return i;
						}
					}
				}
				for (usize k = 0; k < mI->regles.Size(); ++k)
					if (mI->regles[k].mat >= 0 && mI->regles[k].doc.Cherche(doc) && mI->regles[k].piece.Cherche(piece)) {
						++parRegle;
						return mI->regles[k].mat;
					}
			}
			++parMtl;
			return Repli(kd);
		}

		void NkScRotationEuler(const float32 deg[3], float32 R[9]) {
			const float32 k = 0.017453292519943295f;
			const float32 cx = math::NkCos(deg[0] * k), sx = math::NkSin(deg[0] * k);
			const float32 cy = math::NkCos(deg[1] * k), sy = math::NkSin(deg[1] * k);
			const float32 cz = math::NkCos(deg[2] * k), sz = math::NkSin(deg[2] * k);
			// Rz Ry Rx (lignes)
			R[0] = cz * cy;
			R[1] = cz * sy * sx - sz * cx;
			R[2] = cz * sy * cx + sz * sx;
			R[3] = sz * cy;
			R[4] = sz * sy * sx + cz * cx;
			R[5] = sz * sy * cx - cz * sx;
			R[6] = -sy;
			R[7] = cy * sx;
			R[8] = cy * cx;
			const char *mu = env::GetEnvVar("NK_ROTATION_MUTATION");
			if (mu && mu[0] == '1') {
				// contre-epreuve : la transposee (la rotation inverse)
				float32 t;
				t = R[1]; R[1] = R[3]; R[3] = t;
				t = R[2]; R[2] = R[6]; R[6] = t;
				t = R[5]; R[5] = R[7]; R[7] = t;
			}
		}

		bool NkScRegexCherche(const char *motif, const char *texte) {
			Regex r;
			r.Compiler(motif);
			return r.Cherche(texte);
		}

		int32 NkScArchvizBanc() {
			struct Cas {
					const char *m, *t;
					bool attendu;
			};
			static const Cas k[] = {
				{"(cadre|meneau|dormant|chassis)", "sam_dormant_bas", true},
				{"^(v2_rdc_e_menuiseries|e3_etage_d_vitrages)$", "v2_rdc_e_menuiseries", true},
				{"^(v2_rdc_e_menuiseries|e3_etage_d_vitrages)$", "v2_rdc_e_menuiseries_x", false},
				{"(^v_|vitre|_vitre$)", "v_salon", true},
				{"(^v_|vitre|_vitre$)", "salon_v_", false},
				{"(lum$|lampe$)", "plafonnier_lum", true},
				{"(lum$|lampe$)", "lumiere", false},
				{"marche_\\d\\d$", "marche_07", true},
				{"marche_\\d\\d$", "marche_7", false},
				{"^(v2_rdc_[abcd]|e4_toit_e)", "v2_rdc_c_cloisons", true},
				{"^(v2_rdc_[abcd]|e4_toit_e)", "v2_rdc_e_menuiseries", false},
				{".", "x", true},
				{"(_pot$)", "sej_plante_pot", true},
			};
			int32 echecs = 0;
			for (usize i = 0; i < sizeof(k) / sizeof(k[0]); ++i) {
				const bool r = NkScRegexCherche(k[i].m, k[i].t);
				if (r != k[i].attendu) {
					++echecs;
					NkPrintfSortie("[scene] BANC regex ROUGE : '%s' dans '%s' -> %d (attendu %d)\n", k[i].m, k[i].t, (int)r,
								   (int)k[i].attendu);
				}
			}
			Json j;
			const int32 r = j.Lire("{\"a\": [1, 2.5e1, -3], \"b\": {\"c\": \"x\\\\dy\"}, \"d\": true, \"e\": null}");
			if (!j.ok || j.Num(j.Elem(j.Get(r, "a"), 1)) != 25.0 || !Egal(j.Txt(j.Get(j.Get(r, "b"), "c")), "x\\dy") ||
				j.Num(j.Get(r, "d")) != 1.0 || j.Nb(j.Get(r, "a")) != 3) {
				++echecs;
				NkPrintfSortie("[scene] BANC json ROUGE\n");
			}
			NkPrintfSortie("[scene] BANC regex + json : %d echec(s) sur %d cas\n", (int)echecs,
						   (int)(sizeof(k) / sizeof(k[0]) + 1));
			return echecs;
		}

		bool NkScArchvizCharger(NkScArchviz &s, const char *assemblage, const char *matieres, const char *lumieres,
								const char *ambiance, float32 terrain) {
			// ── l'assemblage : pieces, origines ───────────────────────────────
			// (le dossier de l'assemblage, ou vivent les documents, est lu par NkScAttribution::Ouvrir)
			NkVector<Piece> pieces;
			{
				const NkString t = NkFile::ReadAllText(assemblage);
				if (t.Empty())
					return false;
				const char *c = t.CStr();
				char l[1024], mot[128];
				while (*c) {
					c = Ligne(c, l, 1024);
					const char *q = Mot(l, mot, 128);
					if (!Egal(mot, "partie") || !Contient(q, "forme maillage"))
						continue;
					const char *f = q;
					while (*f && !Commence(f, "fichier "))
						++f;
					if (!*f)
						continue;
					Piece p;
					(void)Mot(f + 8, p.fichier, 520);
					p.o[0] = p.o[1] = p.o[2] = 0.f;
					p.doc[0] = 0;
					const char *o = f;
					while (*o && !Commence(o, "origine "))
						++o;
					if (*o) {
						o += 8;
						o = Reel32(o, p.o[0]);
						o = Reel32(o, p.o[1]);
						(void)Reel32(o, p.o[2]);
					}
					// (10/11) la rotation de la partie (ignoree jusqu'ici : l'archviz tournait ses motos a
					// la main avant l'export)
					{
						float32 deg[3] = {0.f, 0.f, 0.f};
						const char *r = f;
						while (*r && !Commence(r, "rotation "))
							++r;
						if (*r) {
							r += 9;
							r = Reel32(r, deg[0]);
							r = Reel32(r, deg[1]);
							(void)Reel32(r, deg[2]);
						}
						NkScRotationEuler(deg, p.R);
						p.tourne = deg[0] != 0.f || deg[1] != 0.f || deg[2] != 0.f;
					}
					int32 n = 0;
					while (p.fichier[n])
						++n;
					if (n > 6 && Egal(p.fichier + n - 6, ".nkgeo")) {
						p.fichier[n - 5] = 'o';
						p.fichier[n - 4] = 'b';
						p.fichier[n - 3] = 'j';
						p.fichier[n - 2] = 0;
					}
					pieces.PushBack(p);
				}
			}
			// ── les documents et les matieres : L'ATTRIBUTION partagee avec le temps reel (10/11) ──
			NkScAttribution at;
			(void)at.Ouvrir(assemblage, matieres);
			auto matDe = [&](const char *doc, const char *piece, const float32 *kd) -> int32 {
				const int32 m = at.Matiere(doc, piece, kd);
				// la piece attribuee, une fois par (document, groupe, matiere) : le banc des matieres exactes
				bool vu = false;
				for (usize k = s.attribues.Size(); k > 0 && !vu; --k) {
					const NkScAttribue &x = s.attribues[k - 1];
					vu = x.mat == m && Egal(x.doc, doc) && Egal(x.piece, piece);
				}
				if (!vu) {
					NkScAttribue x;
					Copier(x.doc, 96, doc);
					Copier(x.piece, 128, piece);
					x.mat = m;
					s.attribues.PushBack(x);
				}
				return m;
			};
			// (11/11/2026, nkcraft-arbres) LA MATIERE A CARTE : la matiere attribuee (bibliotheque, regle ou
			// teinte du .mtl) copiee avec la carte de l'objet, son masque et sa translucidite -- une copie par
			// (matiere, carte, masque, translucide), ajoutee a la bibliotheque de la scene.
			struct Variante {
					int32 base, idx;
					char carte[260];
					float32 masque, trans;
			};
			NkVector<Variante> variantes;
			auto variante = [&](int32 base, const char *carte, float32 masque, float32 trans) -> int32 {
				if (base < 0 || (usize)base >= at.mats.Size())
					return base;
				for (usize k = 0; k < variantes.Size(); ++k)
					if (variantes[k].base == base && Egal(variantes[k].carte, carte) && variantes[k].masque == masque &&
						variantes[k].trans == trans)
						return variantes[k].idx;
				NkScMateriau m = at.mats[(usize)base];
				Copier(m.carte, 260, carte);
				m.masque = masque;
				m.translucide = trans;
				at.mats.PushBack(m);
				Variante w;
				w.base = base;
				w.idx = (int32)at.mats.Size() - 1;
				Copier(w.carte, 260, carte);
				w.masque = masque;
				w.trans = trans;
				variantes.PushBack(w);
				return w.idx;
			};
			// ── la geometrie, piece par piece ─────────────────────────────────
			int32 triangles = 0;
			for (usize ip = 0; ip < pieces.Size(); ++ip) {
				Piece &pc = pieces[ip];
				const NkString t = NkFile::ReadAllText(pc.fichier);
				if (t.Empty()) {
					NkPrintfSortie("[scene] piece illisible : %s\n", pc.fichier);
					continue;
				}
				// le .mtl (teintes de repli ; (11/11) et les cartes d'objet : map_Kd, map_d, nk_translucide)
				struct Kd {
						char nom[96];
						float32 c[3];
						char carte[260];
						float32 masque;
						float32 trans;
				};
				NkVector<Kd> kds;
				{
					char cm[520];
					Copier(cm, 520, pc.fichier);
					int32 n = 0;
					while (cm[n])
						++n;
					if (n > 4) {
						cm[n - 3] = 'm';
						cm[n - 2] = 't';
						cm[n - 1] = 'l';
					}
					const NkString tt = NkFile::ReadAllText(cm);
					const char *c = tt.CStr();
					char l[512], mot[96];
					while (c && *c) {
						c = Ligne(c, l, 512);
						const char *q = Mot(l, mot, 96);
						if (Egal(mot, "newmtl")) {
							Kd k;
							(void)Mot(q, k.nom, 96);
							k.c[0] = k.c[1] = k.c[2] = 0.8f;
							k.carte[0] = 0;
							k.masque = 0.f;
							k.trans = 0.f;
							kds.PushBack(k);
						} else if (Egal(mot, "Kd") && !kds.Empty()) {
							Kd &k = kds[kds.Size() - 1];
							q = Reel32(q, k.c[0]);
							q = Reel32(q, k.c[1]);
							(void)Reel32(q, k.c[2]);
						} else if (Egal(mot, "map_Kd") && !kds.Empty()) {
							// (11/11/2026) LA CARTE DE L'OBJET : le reste de la ligne ; relative, elle se lit a cote du .mtl
							Kd &k = kds[kds.Size() - 1];
							q = Blancs(q);
							char ch[260];
							int32 n2 = 0;
							while (q[n2] && q[n2] != '\r' && q[n2] != '\n' && n2 < 259) {
								ch[n2] = q[n2];
								++n2;
							}
							while (n2 > 0 && (ch[n2 - 1] == ' ' || ch[n2 - 1] == '\t'))
								--n2;
							ch[n2] = 0;
							const bool absolu = ch[0] == '/' || ch[0] == '\\' || (ch[0] && ch[1] == ':');
							if (absolu)
								Copier(k.carte, 260, ch);
							else {
								char dossier[520];
								Copier(dossier, 520, cm);
								int32 fin = -1;
								for (int32 i = 0; dossier[i]; ++i)
									if (dossier[i] == '/' || dossier[i] == '\\')
										fin = i;
								dossier[fin >= 0 ? fin : 0] = 0;
								NkSnprintf(k.carte, 260, "%s%s%s", dossier, dossier[0] ? "/" : "", ch);
							}
						} else if (Egal(mot, "map_d") && !kds.Empty()) {
							kds[kds.Size() - 1].masque = 0.5f; // le detourage : l'alpha de la carte, coupe a mi-hauteur
						} else if (Egal(mot, "nk_translucide") && !kds.Empty()) {
							(void)Reel32(q, kds[kds.Size() - 1].trans);
						}
					}
				}
				NkVector<float32> V, VN, VT;
				const char *c = t.CStr();
				char l[1024], mot[128], groupe[128] = {0}, mtl[96] = {0};
				float32 kd[3] = {0.8f, 0.8f, 0.8f};
				int32 mat = -1;
				int32 ikd = -1; // (11/11) l'entree du .mtl courante (sa carte d'objet)
				while (*c) {
					c = Ligne(c, l, 1024);
					const char *q = Mot(l, mot, 128);
					if (Egal(mot, "vt")) {
						// (11/11/2026) UV du maillage : l'OBJ les ecrit v = 0 en BAS ; la scene les garde v = 0 en HAUT
						float32 u = 0.f, v = 0.f;
						q = Reel32(q, u);
						(void)Reel32(q, v);
						VT.PushBack(u);
						VT.PushBack(1.f - v);
					} else if (Egal(mot, "o")) {
						char sc[96];
						(void)Mot(q, sc, 96);
						at.Document(sc, pc.doc, 96);
					} else if (Egal(mot, "v") || Egal(mot, "vn")) {
						float32 x, y, z;
						q = Reel32(q, x);
						q = Reel32(q, y);
						(void)Reel32(q, z);
						if (pc.tourne) {
							const float32 *R = pc.R;
							const float32 rx = R[0] * x + R[1] * y + R[2] * z, ry = R[3] * x + R[4] * y + R[5] * z,
										  rz = R[6] * x + R[7] * y + R[8] * z;
							x = rx;
							y = ry;
							z = rz;
						}
						if (mot[1] == 'n') {
							VN.PushBack(x);
							VN.PushBack(y);
							VN.PushBack(z);
						} else {
							V.PushBack(x + pc.o[0]);
							V.PushBack(y + pc.o[1]);
							V.PushBack(z + pc.o[2]);
						}
					} else if (Egal(mot, "g")) {
						(void)Mot(q, groupe, 128);
						mat = -1;
					} else if (Egal(mot, "usemtl")) {
						(void)Mot(q, mtl, 96);
						ikd = -1;
						for (usize k = 0; k < kds.Size(); ++k)
							if (Egal(kds[k].nom, mtl)) {
								kd[0] = kds[k].c[0];
								kd[1] = kds[k].c[1];
								kd[2] = kds[k].c[2];
								ikd = (int32)k;
							}
						mat = -1;
					} else if (Egal(mot, "f")) {
						if (mat < 0) {
							mat = matDe(pc.doc, groupe, kd);
							// (11/11/2026) une entree du .mtl a carte : la matiere attribuee la RECOIT (copie
							// marquee : carte, masque, translucide) -- la feuille detouree garde son contour
							if (ikd >= 0 && kds[(usize)ikd].carte[0])
								mat = variante(mat, kds[(usize)ikd].carte, kds[(usize)ikd].masque, kds[(usize)ikd].trans);
						}
						const bool cartee = ikd >= 0 && kds[(usize)ikd].carte[0] != 0;
						int32 iv[32], in[32], it[32], nr = 0;
						while (*q && nr < 32) {
							q = Blancs(q);
							if (!*q)
								break;
							int32 a = 0, nn = 0, tt = 0;
							q = Entier(q, a);
							if (*q == '/') {
								++q;
								if (*q != '/')
									q = Entier(q, tt);
								if (*q == '/') {
									++q;
									q = Entier(q, nn);
								}
							}
							while (*q && *q != ' ' && *q != '\t')
								++q;
							iv[nr] = a > 0 ? a - 1 : (int32)(V.Size() / 3) + a;
							in[nr] = nn > 0 ? nn - 1 : -1;
							it[nr] = tt > 0 ? tt - 1 : (tt < 0 ? (int32)(VT.Size() / 2) + tt : -1);
							++nr;
						}
						for (int32 k = 1; k + 1 < nr; ++k) {
							const int32 ids[3] = {iv[0], iv[k], iv[k + 1]};
							const int32 nds[3] = {in[0], in[k], in[k + 1]};
							const int32 tds[3] = {it[0], it[k], it[k + 1]};
							bool ok = true;
							for (int32 j = 0; j < 3; ++j)
								if (ids[j] < 0 || ids[j] >= (int32)(V.Size() / 3))
									ok = false;
							if (!ok)
								continue;
							const float32 *a = &V[(usize)ids[0] * 3], *b = &V[(usize)ids[1] * 3], *cc = &V[(usize)ids[2] * 3];
							const float32 e0[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
							const float32 e1[3] = {cc[0] - a[0], cc[1] - a[1], cc[2] - a[2]};
							// ordre du moteur (horaire autour de la normale sortante) : on retourne
							float32 g[3] = {-(e0[1] * e1[2] - e0[2] * e1[1]), -(e0[2] * e1[0] - e0[0] * e1[2]),
											-(e0[0] * e1[1] - e0[1] * e1[0])};
							const float32 gl = math::NkSqrt(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]);
							if (gl < 1e-12f)
								continue;
							g[0] /= gl;
							g[1] /= gl;
							g[2] /= gl;
							const uint32 base = (uint32)(s.P.Size() / 3);
							for (int32 j = 0; j < 3; ++j) {
								const float32 *p = &V[(usize)ids[j] * 3];
								float32 nv[3] = {g[0], g[1], g[2]};
								if (nds[j] >= 0 && nds[j] < (int32)(VN.Size() / 3))
									for (int32 m = 0; m < 3; ++m)
										nv[m] = VN[(usize)nds[j] * 3 + (usize)m];
								// projection en boite, en metres (axe dominant de la normale geometrique)
								const float32 ax = g[0] < 0.f ? -g[0] : g[0], ay = g[1] < 0.f ? -g[1] : g[1],
											  az = g[2] < 0.f ? -g[2] : g[2];
								float32 u, v;
								if (ay >= ax && ay >= az) {
									u = p[0];
									v = -p[2] * (g[1] >= 0.f ? 1.f : -1.f);
								} else if (ax >= az) {
									u = -p[2] * (g[0] >= 0.f ? 1.f : -1.f);
									v = p[1];
								} else {
									u = p[0] * (g[2] >= 0.f ? 1.f : -1.f);
									v = p[1];
								}
								// (11/11/2026) une carte d'objet se lit aux UV DU MAILLAGE, pas en metres
								if (cartee && tds[j] >= 0 && tds[j] < (int32)(VT.Size() / 2)) {
									u = VT[(usize)tds[j] * 2];
									v = VT[(usize)tds[j] * 2 + 1];
								}
								for (int32 m = 0; m < 3; ++m) {
									s.P.PushBack(p[m]);
									s.N.PushBack(nv[m]);
									if (p[m] < s.mn[m])
										s.mn[m] = p[m];
									if (p[m] > s.mx[m])
										s.mx[m] = p[m];
								}
								s.UV.PushBack(u);
								s.UV.PushBack(v);
							}
							s.tri.PushBack(base);
							s.tri.PushBack(base + 1u);
							s.tri.PushBack(base + 2u);
							s.triMat.PushBack(mat);
							++triangles;
						}
					}
				}
			}
			// ── le terrain ─────────────────────────────────────────────────────
			if (terrain > 0.f && triangles > 0) {
				int32 mt = at.Index("gazon");
				if (mt < 0) {
					const float32 vert[3] = {0.12f, 0.20f, 0.06f};
					mt = at.Repli(vert);
				}
				const float32 cx = 0.5f * (s.mn[0] + s.mx[0]), cz = 0.5f * (s.mn[2] + s.mx[2]), y = s.mn[1] - 0.02f;
				// EN DALLES de 20 x 20 : deux triangles de 300 m avaient une boite qui contenait
				// toute la maison -- le BVH ne separait plus rien (0,05 Mrayon/s par fil, mesure).
				const int32 nd = 20;
				const float32 pas = 2.f * terrain / (float32)nd;
				for (int32 iz = 0; iz < nd; ++iz)
					for (int32 ix = 0; ix < nd; ++ix) {
						const float32 x0 = cx - terrain + pas * (float32)ix, z0 = cz - terrain + pas * (float32)iz;
						const float32 c4[4][3] = {{x0, y, z0}, {x0 + pas, y, z0}, {x0 + pas, y, z0 + pas}, {x0, y, z0 + pas}};
						const int32 ordre[6] = {0, 2, 1, 0, 3, 2};
						for (int32 t2 = 0; t2 < 2; ++t2) {
							const uint32 base = (uint32)(s.P.Size() / 3);
							for (int32 j = 0; j < 3; ++j) {
								const float32 *p = c4[ordre[t2 * 3 + j]];
								for (int32 m = 0; m < 3; ++m)
									s.P.PushBack(p[m]);
								s.N.PushBack(0.f);
								s.N.PushBack(1.f);
								s.N.PushBack(0.f);
								s.UV.PushBack(p[0]);
								s.UV.PushBack(-p[2]);
							}
							s.tri.PushBack(base);
							s.tri.PushBack(base + 1u);
							s.tri.PushBack(base + 2u);
							s.triMat.PushBack(mt);
						}
					}
			}
			// ── les lumieres et l'ambiance ────────────────────────────────────
			Json jl;
			const NkString tl = NkFile::ReadAllText(lumieres);
			const int32 rl = tl.Empty() ? -1 : jl.Lire(tl.CStr());
			int32 horsScene = 0;
			const int32 jsrc = jl.Get(rl, "sources");
			auto origineDe = [&](const char *doc, float32 *o) -> bool {
				for (usize i = 0; i < pieces.Size(); ++i)
					if (Egal(pieces[i].doc, doc)) {
						o[0] = pieces[i].o[0];
						o[1] = pieces[i].o[1];
						o[2] = pieces[i].o[2];
						return true;
					}
				return false;
			};
			const float32 kPi = 3.14159265f;
			for (int32 k = 0; k < jl.Nb(jsrc); ++k) {
				const int32 src = jl.Elem(jsrc, k);
				const float32 f = (float32)jl.Num(jl.Get(jl.Get(src, "ambiances"), ambiance), 1.0);
				if (f <= 0.f)
					continue;
				const char *objet = jl.Txt(jl.Get(src, "objet"));
				char doc[96];
				int32 nd = 0;
				while (objet[nd] && objet[nd] != ':')
					++nd;
				Copier(doc, 96, objet, nd);
				float32 o[3];
				if (!origineDe(doc, o)) {
					++horsScene;
					continue;
				}
				NkScSource S;
				Copier(S.objet, 64, objet);
				jl.Vec3(jl.Get(src, "couleur"), S.couleur);
				S.rayon = (float32)jl.Num(jl.Get(src, "rayon"), 0.02);
				const char *forme = jl.Txt(jl.Get(src, "forme"));
				const float32 P = (float32)jl.Num(jl.Get(src, "flux_lm"), 0.0) * 1e-3f * f;
				if (Egal(forme, "ruban")) {
					const int32 ch = jl.Get(src, "chemin");
					const float32 parM = (float32)jl.Num(jl.Get(src, "flux_lm_par_m"), 0.0) * 1e-3f * f;
					for (int32 seg = 0; seg + 1 < jl.Nb(ch); ++seg) {
						float32 a[3] = {0.f, 0.f, 0.f}, b[3] = {0.f, 0.f, 0.f};
						jl.Vec3(jl.Elem(ch, seg), a);
						jl.Vec3(jl.Elem(ch, seg + 1), b);
						const float32 d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
						const float32 lg = math::NkSqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
						const int32 n = (int32)(lg / 0.25f) + 1;
						for (int32 i = 0; i < n; ++i) {
							NkScSource R = S;
							const float32 tt = ((float32)i + 0.5f) / (float32)n;
							for (int32 m = 0; m < 3; ++m)
								R.p[m] = a[m] + d[m] * tt + o[m];
							R.type = 0;
							R.rayon = 0.01f;
							R.puissance = parM * lg / (float32)n;
							R.intensite = R.puissance / (4.f * kPi);
							s.sources.PushBack(R);
						}
					}
					continue;
				}
				float32 pos[3] = {0.f, 0.f, 0.f};
				jl.Vec3(jl.Get(src, "position"), pos);
				for (int32 m = 0; m < 3; ++m)
					S.p[m] = pos[m] + o[m];
				if (Egal(forme, "spot")) {
					float32 dir[3] = {0.f, -1.f, 0.f};
					jl.Vec3(jl.Get(src, "direction"), dir);
					const float32 dl = math::NkSqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
					const float32 demi = (float32)jl.Num(jl.Get(src, "angle_deg"), 60.0) * 0.5f * kPi / 180.f;
					S.type = 1;
					S.cosExt = math::NkCos(demi);
					const bool deux = jl.Num(jl.Get(src, "haut_et_bas"), 0.0) > 0.5;
					S.puissance = deux ? P * 0.5f : P;
					S.intensite = S.puissance / (2.f * kPi * (1.f - S.cosExt) + 1e-4f);
					for (int32 m = 0; m < 3; ++m)
						S.dir[m] = dl > 1e-6f ? dir[m] / dl : (m == 1 ? -1.f : 0.f);
					s.sources.PushBack(S);
					if (deux) {
						for (int32 m = 0; m < 3; ++m)
							S.dir[m] = -S.dir[m];
						s.sources.PushBack(S);
					}
				} else {
					S.type = 0;
					S.puissance = P;
					S.intensite = P / (4.f * kPi);
					s.sources.PushBack(S);
				}
			}
			const int32 amb = jl.Get(jl.Get(rl, "ambiances"), ambiance);
			if (amb >= 0) {
				const int32 so = jl.Get(amb, "soleil"), ci = jl.Get(amb, "ciel");
				const float32 az = (float32)jl.Num(jl.Get(so, "azimut"), 0.0) * kPi / 180.f;
				const float32 el = (float32)jl.Num(jl.Get(so, "elevation"), 45.0) * kPi / 180.f;
				// convention de NK_SOLEIL : azimut 0 = soleil au +Z, 90 = au +X
				s.soleilDir[0] = math::NkCos(el) * math::NkSin(az);
				s.soleilDir[1] = math::NkSin(el);
				s.soleilDir[2] = math::NkCos(el) * math::NkCos(az);
				jl.Vec3(jl.Get(so, "couleur"), s.soleilCouleur);
				s.soleilE = (float32)jl.Num(jl.Get(so, "eclairement_lux"), 0.0) * 1e-3f;
				jl.Vec3(jl.Get(ci, "zenith"), s.cielZenith);
				jl.Vec3(jl.Get(ci, "horizon"), s.cielHorizon);
				s.cielIntensite = (float32)jl.Num(jl.Get(ci, "intensite"), 1.0);
				s.expositionEV = (float32)jl.Num(jl.Get(amb, "exposition_EV"), 10.0);
			}
			// (10/11) les matieres de l'attribution partagee, et son compte rendu
			s.mats = at.mats;
			s.piecesParNom = at.parNom;
			s.piecesParRegle = at.parRegle;
			s.piecesParMtl = at.parMtl;
			const int32 nBib = at.nBib;
			NkPrintfSortie("[scene] %d piece(s), %d triangle(s) ; matieres : %d nommees, %d par nom d'objet, %d par regle, %d par "
						   "repli .mtl ; %d source(s) (ambiance %s, %d hors scene) ; json %s/%s\n",
						   (int)pieces.Size(), (int)triangles, (int)nBib, (int)s.piecesParNom, (int)s.piecesParRegle,
						   (int)s.piecesParMtl, (int)s.sources.Size(), ambiance, (int)horsScene, at.jsonLu ? "ok" : "ERREUR",
						   jl.ok ? "ok" : "ERREUR");
			return triangles > 0;
		}

	} // namespace renderer
} // namespace nkentseu
