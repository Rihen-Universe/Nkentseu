// -----------------------------------------------------------------------------
// FICHIER: Kernel\Runtime\NKRenderer\src\NKRenderer\Mesh\NkCreatureScene.cpp
// DESCRIPTION: Implementation du lecteur `.nkscene` v2 (voir NkCreatureScene.h).
// AUTEUR: TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// DATE: 2026-09-29
// VERSION: 0.1.0
// -----------------------------------------------------------------------------
//
// ⚠️ AUCUN NOMBRE N'EST LU PAR LA LOCALE (`atof` rend 0,5 en fr-FR pour « 0.5 » ou
//    l'inverse) : lecture faite main, comme dans NkMeshR32.
// -----------------------------------------------------------------------------

// ============================================================
// INCLUDES
// ============================================================

// Header correspondant (TOUJOURS EN PREMIER)
#include "NKRenderer/Mesh/NkCreatureScene.h"

// Standard library
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

// ============================================================
// ANONYMOUS NAMESPACE (helpers internes)
// ============================================================

namespace nkentseu {
	namespace renderer {
		namespace {

			// ========================================
			// CONSTANTS
			// ========================================

			const uint32 NK_SCENE_JETONS_MAX = 96u;
			const uint32 NK_SCENE_JETON_MAX = 64u;
			const float32 NK_SCENE_RAYON_DEFAUT = 0.1f;

			// ========================================
			// TEXTE
			// ========================================

			void Dire(char *out, uint32 cap, const char *fmt, ...) {
				if (!out || cap == 0u) {
					return;
				}
				va_list args;
				va_start(args, fmt);
				std::vsnprintf(out, cap, fmt, args);
				va_end(args);
			}

			void Avertir(char *out, uint32 cap, const char *fmt, ...) {
				if (!out || cap == 0u) {
					return;
				}
				char ligne[256];
				va_list args;
				va_start(args, fmt);
				std::vsnprintf(ligne, sizeof(ligne), fmt, args);
				va_end(args);
				const uint32 n = (uint32)std::strlen(out);
				std::snprintf(out + n, cap - n, "%s\n", ligne);
			}

			bool Egal(const char *a, const char *b) {
				return a && b && std::strcmp(a, b) == 0;
			}

			void Copier(char *dst, const char *src, uint32 cap) {
				std::snprintf(dst, cap, "%s", src ? src : "");
			}

			// Lecture d'un reel, sans locale. Rend faux si le texte n'EST pas un nombre
			// (un reste non lu est une erreur : `0.5x` n'est pas 0,5).
			bool Reel(const char *t, float32 &v, const char **fin = nullptr) {
				const char *p = t;
				bool negatif = false;
				if (*p == '-' || *p == '+') {
					negatif = (*p == '-');
					++p;
				}
				if (!((*p >= '0' && *p <= '9') || *p == '.')) {
					return false;
				}
				float64 r = 0.0;
				while (*p >= '0' && *p <= '9') {
					r = r * 10.0 + (float64)(*p - '0');
					++p;
				}
				if (*p == '.') {
					++p;
					float64 e = 0.1;
					while (*p >= '0' && *p <= '9') {
						r += e * (float64)(*p - '0');
						e *= 0.1;
						++p;
					}
				}
				v = (float32)(negatif ? -r : r);
				if (fin) {
					*fin = p;
					return true;
				}
				return *p == 0;
			}

			bool Entier(const char *t, uint32 &v) {
				float32 f = 0.f;
				if (!Reel(t, f) || f < 0.f || f != std::floor(f)) {
					return false;
				}
				v = (uint32)f;
				return true;
			}

			struct NkSceneLigne {
					char jetons[NK_SCENE_JETONS_MAX][NK_SCENE_JETON_MAX];
					uint32 n = 0u;
					uint32 retrait = 0u;
			};

			// Decoupe une ligne en jetons (les guillemets groupent), sans le commentaire
			// (`#` ou `//`, hors guillemets). Rend le retrait (tabulation = 4).
			void Decouper(const char *debut, const char *fin, NkSceneLigne &l) {
				l.n = 0u;
				l.retrait = 0u;
				const char *p = debut;
				while (p < fin && (*p == ' ' || *p == '\t')) {
					l.retrait += (*p == '\t') ? 4u : 1u;
					++p;
				}
				while (p < fin) {
					while (p < fin && (*p == ' ' || *p == '\t' || *p == '\r')) {
						++p;
					}
					if (p >= fin || *p == '#' || (*p == '/' && p + 1 < fin && p[1] == '/')) {
						return;
					}
					char *j = l.jetons[l.n < NK_SCENE_JETONS_MAX ? l.n : NK_SCENE_JETONS_MAX - 1u];
					uint32 k = 0u;
					if (*p == '"') {
						++p;
						while (p < fin && *p != '"') {
							if (k + 1u < NK_SCENE_JETON_MAX) {
								j[k] = *p;
								++k;
							}
							++p;
						}
						++p;
					} else {
						while (p < fin && *p != ' ' && *p != '\t' && *p != '\r') {
							if (k + 1u < NK_SCENE_JETON_MAX) {
								j[k] = *p;
								++k;
							}
							++p;
						}
					}
					j[k] = 0;
					if (l.n < NK_SCENE_JETONS_MAX) {
						++l.n;
					}
				}
			}

			// ========================================
			// GEOMETRIE
			// ========================================

			NkVec3f V3(float32 x, float32 y, float32 z) {
				NkVec3f v;
				v.x = x;
				v.y = y;
				v.z = z;
				return v;
			}

			float32 Scal(const NkVec3f &a, const NkVec3f &b) {
				return a.x * b.x + a.y * b.y + a.z * b.z;
			}

			NkVec3f Vect(const NkVec3f &a, const NkVec3f &b) {
				return V3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
			}

			NkVec3f Unitaire(const NkVec3f &a) {
				const float32 n = std::sqrt(Scal(a, a));
				if (n < 1e-12f) {
					return V3(0.f, 0.f, 0.f);
				}
				return V3(a.x / n, a.y / n, a.z / n);
			}

			NkVec3f ReferenceParDefaut(const NkVec3f &dir) {
				const NkVec3f d = Unitaire(dir);
				if (std::fabs(d.z) < 0.9f) {
					return V3(0.f, 0.f, 1.f);
				}
				return V3(0.f, 1.f, 0.f);
			}

			// La reference du parent, TRANSPORTEE sur l'axe de l'enfant par la rotation
			// minimale (Rodrigues) : le repere ne vrille pas au joint.
			NkVec3f Transporter(const NkR32Os &parent, const NkVec3f &axeEnfant) {
				const NkVec3f ap = Unitaire(parent.direction);
				const NkVec3f ac = Unitaire(axeEnfant);
				const NkVec3f rp = parent.reference;
				NkVec3f e1 = Unitaire(V3(rp.x - ap.x * Scal(rp, ap), rp.y - ap.y * Scal(rp, ap), rp.z - ap.z * Scal(rp, ap)));
				const NkVec3f k = Vect(ap, ac);
				const float32 s = std::sqrt(Scal(k, k));
				const float32 c = Scal(ap, ac);
				if (s > 1e-9f) {
					const NkVec3f kh = V3(k.x / s, k.y / s, k.z / s);
					const NkVec3f kxv = Vect(kh, e1);
					const float32 kv = Scal(kh, e1);
					e1 = V3(e1.x * c + kxv.x * s + kh.x * kv * (1.f - c), e1.y * c + kxv.y * s + kh.y * kv * (1.f - c),
							e1.z * c + kxv.z * s + kh.z * kv * (1.f - c));
				}
				return Unitaire(e1);
			}

			// ========================================
			// LE LECTEUR
			// ========================================

			struct NkSceneChaine {
					char nom[NK_R32_NOM_MAX] = {0};
					NkVector<uint32> os;
			};

			struct NkSceneProfil {
					char motif[NK_R32_NOM_MAX] = {0};
					bool estSection = false;
					float32 largeur = 0.f;
					float32 hauteur = 0.f;
					float32 exposant = 2.f;
					NkVector<NkR32Clef> clefs;
					uint32 ligne = 0u;
			};

			struct NkSceneEtat {
					NkR32Document *doc = nullptr;
					NkVector<NkSceneChaine> chaines;
					NkVector<NkSceneProfil> profils;
					bool estMiroir = false;
					// Le membre en cours : ses os se chainent a partir de `dernier`.
					bool estMembre = false;
					uint32 retraitMembre = 0u;
					char suffixe[8] = {0};
					char dernier[NK_R32_NOM_MAX] = {0};
					NkVec3f directionMembre = {0.f, -1.f, 0.f};
					bool estDirectionMembre = false;
			};

			// REF : un os, une chaine (son dernier os).
			int32 Resoudre(const NkSceneEtat &e, const char *ref) {
				const int32 i = e.doc->Trouver(ref);
				if (i >= 0) {
					return i;
				}
				for (uint32 c = 0u; c < (uint32)e.chaines.Size(); ++c) {
					if (Egal(e.chaines[c].nom, ref) && !e.chaines[c].os.Empty()) {
						return (int32)e.chaines[c].os[(uint32)e.chaines[c].os.Size() - 1u];
					}
				}
				return -1;
			}

			bool AjouterOs(NkSceneEtat &e, const char *nom, int32 parent, const NkVec3f &position, const NkVec3f &dir,
						   float32 longueur, float32 r0, float32 r1, bool estReference, const NkVec3f &reference,
						   uint32 ligne, char *pourquoi, uint32 cap) {
				NkR32Os o;
				Copier(o.nom, nom, NK_R32_NOM_MAX);
				o.racine = position;
				o.direction = Unitaire(dir);
				o.longueur = longueur;
				o.rayon0 = r0;
				o.rayon1 = r1;
				if (parent >= 0) {
					Copier(o.parent, e.doc->os[(uint32)parent].nom, NK_R32_NOM_MAX);
				}
				if (estReference) {
					o.reference = reference;
				} else if (parent >= 0) {
					o.reference = Transporter(e.doc->os[(uint32)parent], o.direction);
				} else {
					o.reference = ReferenceParDefaut(o.direction);
				}
				char raison[256];
				if (!e.doc->AjouterOs(o, raison, 256u)) {
					Dire(pourquoi, cap, "ligne %u : %s", ligne, raison);
					return false;
				}
				return true;
			}

			// Lit `x y z` a partir du jeton i.
			bool Trois(const NkSceneLigne &l, uint32 i, NkVec3f &v) {
				if (i + 2u >= l.n) {
					return false;
				}
				return Reel(l.jetons[i], v.x) && Reel(l.jetons[i + 1u], v.y) && Reel(l.jetons[i + 2u], v.z);
			}

			bool Vers(const char *t, NkVec3f &v) {
				if (std::strlen(t) != 2u || (t[0] != '+' && t[0] != '-')) {
					return false;
				}
				const float32 s = (t[0] == '-') ? -1.f : 1.f;
				if (t[1] == 'X') {
					v = V3(s, 0.f, 0.f);
				} else if (t[1] == 'Y') {
					v = V3(0.f, s, 0.f);
				} else if (t[1] == 'Z') {
					v = V3(0.f, 0.f, s);
				} else {
					return false;
				}
				return true;
			}

			// `os NOM ...` a partir du jeton `i` (qui vaut « os »). Rend l'indice du
			// jeton suivant la definition (plusieurs os par ligne dans un membre).
			bool LireOs(NkSceneEtat &e, const NkSceneLigne &l, uint32 &i, bool dansMembre, uint32 ligne, char *pourquoi,
						uint32 cap) {
				++i;
				if (i >= l.n) {
					Dire(pourquoi, cap, "ligne %u : nom d'os attendu apres `os`", ligne);
					return false;
				}
				char nom[NK_R32_NOM_MAX];
				if (dansMembre) {
					std::snprintf(nom, sizeof(nom), "%s%s", l.jetons[i], e.suffixe);
				} else {
					Copier(nom, l.jetons[i], NK_R32_NOM_MAX);
				}
				++i;
				int32 parent = -1;
				NkVec3f position = V3(0.f, 0.f, 0.f);
				NkVec3f dir = V3(0.f, 1.f, 0.f);
				bool estDir = false;
				float32 longueur = 0.f;
				float32 r0 = NK_SCENE_RAYON_DEFAUT;
				float32 r1 = -1.f;
				bool estRef = false;
				NkVec3f ref = V3(0.f, 0.f, 1.f);
				bool estRacine = false;
				if (dansMembre) {
					parent = (e.dernier[0] != 0) ? e.doc->Trouver(e.dernier) : -1;
					estRacine = parent < 0;
				}
				while (i < l.n && !Egal(l.jetons[i], "os")) {
					const char *m = l.jetons[i];
					if (Egal(m, "racine")) {
						estRacine = true;
						i += 1u;
					} else if (Egal(m, "depuis") && i + 1u < l.n) {
						parent = Resoudre(e, l.jetons[i + 1u]);
						if (parent < 0) {
							Dire(pourquoi, cap, "ligne %u : `depuis %s` ne designe aucun os ni chaine", ligne,
								 l.jetons[i + 1u]);
							return false;
						}
						i += 2u;
					} else if (Egal(m, "position") && Trois(l, i + 1u, position)) {
						i += 4u;
					} else if (Egal(m, "longueur") && i + 1u < l.n && Reel(l.jetons[i + 1u], longueur)) {
						i += 2u;
					} else if (Egal(m, "direction") && Trois(l, i + 1u, dir)) {
						estDir = true;
						i += 4u;
					} else if (Egal(m, "vers") && i + 1u < l.n && Vers(l.jetons[i + 1u], dir)) {
						estDir = true;
						i += 2u;
					} else if (Egal(m, "reference") && Trois(l, i + 1u, ref)) {
						estRef = true;
						i += 4u;
					} else if (Egal(m, "rayon") && i + 1u < l.n && Reel(l.jetons[i + 1u], r0)) {
						i += 2u;
						if (i < l.n && Reel(l.jetons[i], r1)) {
							i += 1u;
						}
					} else {
						Dire(pourquoi, cap, "ligne %u : mot inconnu `%s` dans la definition de l'os `%s`", ligne, m, nom);
						return false;
					}
				}
				if (!(longueur > 0.f)) {
					Dire(pourquoi, cap, "ligne %u : l'os `%s` doit avoir une `longueur` positive", ligne, nom);
					return false;
				}
				if (!estRacine && parent < 0) {
					Dire(pourquoi, cap, "ligne %u : l'os `%s` doit etre `racine` ou `depuis` un os", ligne, nom);
					return false;
				}
				if (!estDir) {
					if (parent >= 0) {
						dir = e.doc->os[(uint32)parent].direction;
					} else if (dansMembre && e.estDirectionMembre) {
						dir = e.directionMembre;
					}
				}
				if (r1 < 0.f) {
					r1 = r0;
				}
				if (!AjouterOs(e, nom, estRacine ? -1 : parent, position, dir, longueur, r0, r1, estRef, ref, ligne,
							   pourquoi, cap)) {
					return false;
				}
				if (dansMembre) {
					Copier(e.dernier, nom, NK_R32_NOM_MAX);
				}
				return true;
			}

			bool LireChaine(NkSceneEtat &e, const NkSceneLigne &l, uint32 ligne, char *pourquoi, uint32 cap) {
				if (l.n < 2u) {
					Dire(pourquoi, cap, "ligne %u : nom attendu apres `chaine`", ligne);
					return false;
				}
				NkSceneChaine ch;
				Copier(ch.nom, l.jetons[1], NK_R32_NOM_MAX);
				int32 parent = -1;
				bool estRacine = false;
				uint32 segments = 0u;
				float32 longueur = 0.f;
				NkVec3f dir = V3(0.f, 1.f, 0.f);
				bool estDir = false;
				NkVec3f courbure = V3(0.f, 0.f, 0.f);
				float32 effilement = 1.f;
				float32 rayon = NK_SCENE_RAYON_DEFAUT;
				NkVec3f position = V3(0.f, 0.f, 0.f);
				uint32 i = 2u;
				while (i < l.n) {
					const char *m = l.jetons[i];
					if (Egal(m, "racine")) {
						estRacine = true;
						i += 1u;
					} else if (Egal(m, "depuis") && i + 1u < l.n) {
						parent = Resoudre(e, l.jetons[i + 1u]);
						if (parent < 0) {
							Dire(pourquoi, cap, "ligne %u : `depuis %s` ne designe aucun os ni chaine", ligne,
								 l.jetons[i + 1u]);
							return false;
						}
						i += 2u;
					} else if (Egal(m, "segments") && i + 1u < l.n && Entier(l.jetons[i + 1u], segments)) {
						i += 2u;
					} else if (Egal(m, "longueur") && i + 1u < l.n && Reel(l.jetons[i + 1u], longueur)) {
						i += 2u;
					} else if (Egal(m, "direction") && Trois(l, i + 1u, dir)) {
						estDir = true;
						i += 4u;
					} else if (Egal(m, "vers") && i + 1u < l.n && Vers(l.jetons[i + 1u], dir)) {
						estDir = true;
						i += 2u;
					} else if (Egal(m, "courbure") && Trois(l, i + 1u, courbure)) {
						i += 4u;
					} else if (Egal(m, "effilement") && i + 1u < l.n && Reel(l.jetons[i + 1u], effilement)) {
						i += 2u;
					} else if (Egal(m, "rayon") && i + 1u < l.n && Reel(l.jetons[i + 1u], rayon)) {
						i += 2u;
					} else if (Egal(m, "position") && Trois(l, i + 1u, position)) {
						i += 4u;
					} else {
						Dire(pourquoi, cap, "ligne %u : mot inconnu `%s` dans la chaine `%s`", ligne, m, ch.nom);
						return false;
					}
				}
				if (segments < 1u || !(longueur > 0.f)) {
					Dire(pourquoi, cap, "ligne %u : la chaine `%s` demande `segments` >= 1 et une `longueur` positive",
						 ligne, ch.nom);
					return false;
				}
				if (!estRacine && parent < 0) {
					Dire(pourquoi, cap, "ligne %u : la chaine `%s` doit etre `racine` ou `depuis` un os", ligne, ch.nom);
					return false;
				}
				if (!estDir && parent >= 0) {
					dir = e.doc->os[(uint32)parent].direction;
				}
				const NkVec3f d0 = Unitaire(dir);
				int32 precedent = estRacine ? -1 : parent;
				for (uint32 k = 1u; k <= segments; ++k) {
					char nom[NK_R32_NOM_MAX];
					std::snprintf(nom, sizeof(nom), "%s.%u", ch.nom, k);
					// La courbure est le changement TOTAL de direction, reparti le long
					// de la chaine : l'os k prend la direction a son milieu.
					const float32 f = ((float32)k - 0.5f) / (float32)segments;
					const NkVec3f dk = V3(d0.x + courbure.x * f, d0.y + courbure.y * f, d0.z + courbure.z * f);
					const float32 ra = rayon * (1.f + (effilement - 1.f) * (float32)(k - 1u) / (float32)segments);
					const float32 rb = rayon * (1.f + (effilement - 1.f) * (float32)k / (float32)segments);
					if (!AjouterOs(e, nom, precedent, position, dk, longueur / (float32)segments, ra, rb, false,
								   V3(0.f, 0.f, 1.f), ligne, pourquoi, cap)) {
						return false;
					}
					precedent = e.doc->Trouver(nom);
					ch.os.PushBack((uint32)precedent);
				}
				e.chaines.PushBack(ch);
				return true;
			}

			bool LireProfil(NkSceneEtat &e, const NkSceneLigne &l, uint32 ligne, char *pourquoi, uint32 cap,
							char *avert, uint32 capAvert) {
				if (l.n < 3u) {
					Dire(pourquoi, cap, "ligne %u : `profil MOTIF section ...` ou `profil MOTIF clefs ...` attendu", ligne);
					return false;
				}
				NkSceneProfil p;
				Copier(p.motif, l.jetons[1], NK_R32_NOM_MAX);
				p.ligne = ligne;
				uint32 i = 2u;
				if (Egal(l.jetons[i], "section")) {
					p.estSection = true;
					++i;
					if (i < l.n && Egal(l.jetons[i], "superellipse")) {
						++i;
					}
					while (i < l.n) {
						const char *m = l.jetons[i];
						if (Egal(m, "largeur") && i + 1u < l.n && Reel(l.jetons[i + 1u], p.largeur)) {
							i += 2u;
						} else if (Egal(m, "hauteur") && i + 1u < l.n && Reel(l.jetons[i + 1u], p.hauteur)) {
							i += 2u;
						} else if (Egal(m, "exposant") && i + 1u < l.n && Reel(l.jetons[i + 1u], p.exposant)) {
							i += 2u;
						} else if (Egal(m, "muscle")) {
							Avertir(avert, capAvert, "ligne %u : `muscle` ignore (bosses de profil : G3)", ligne);
							break;
						} else {
							Dire(pourquoi, cap, "ligne %u : mot inconnu `%s` dans le profil `%s`", ligne, m, p.motif);
							return false;
						}
					}
					if (!(p.largeur > 0.f) || !(p.hauteur > 0.f) || !(p.exposant >= 1.f)) {
						Dire(pourquoi, cap, "ligne %u : la section de `%s` demande largeur, hauteur > 0 et exposant >= 1",
							 ligne, p.motif);
						return false;
					}
				} else if (Egal(l.jetons[i], "clefs")) {
					++i;
					while (i < l.n) {
						if (Egal(l.jetons[i], "muscle")) {
							Avertir(avert, capAvert, "ligne %u : `muscle` ignore (bosses de profil : G3)", ligne);
							break;
						}
						NkR32Clef c;
						const char *q = l.jetons[i];
						const char *f = nullptr;
						const bool lu = Reel(q, c.s, &f) && *f == ':' && Reel(f + 1, c.largeur, &f) && *f == 'x' &&
										Reel(f + 1, c.hauteur, &f) && *f == 0;
						if (!lu || c.s < 0.f || c.s > 1.f || !(c.largeur > 0.f) || !(c.hauteur > 0.f)) {
							Dire(pourquoi, cap, "ligne %u : clef `%s` : forme attendue `s:LARGEURxHAUTEUR`, s dans [0, 1]",
								 ligne, q);
							return false;
						}
						if (!p.clefs.Empty() && c.s <= p.clefs[(uint32)p.clefs.Size() - 1u].s) {
							Dire(pourquoi, cap, "ligne %u : les clefs de `%s` doivent etre en s CROISSANT", ligne, p.motif);
							return false;
						}
						p.clefs.PushBack(c);
						++i;
					}
					if (p.clefs.Empty()) {
						Dire(pourquoi, cap, "ligne %u : `clefs` sans aucune clef", ligne);
						return false;
					}
				} else if (Egal(l.jetons[i], "membrane")) {
					Dire(pourquoi, cap, "ligne %u : profil `membrane` (aile, nageoire) : G2c", ligne);
					return false;
				} else {
					Dire(pourquoi, cap, "ligne %u : profil `%s` : `section` ou `clefs` attendu, `%s` trouve", ligne, p.motif,
						 l.jetons[i]);
					return false;
				}
				e.profils.PushBack(p);
				return true;
			}

			bool Correspond(const char *motif, const char *nom) {
				const uint32 n = (uint32)std::strlen(motif);
				if (n > 0u && motif[n - 1u] == '*') {
					return std::strncmp(motif, nom, n - 1u) == 0;
				}
				return Egal(motif, nom);
			}

			NkR32Clef ClefA(const NkVector<NkR32Clef> &clefs, float32 t) {
				NkR32Clef r = clefs[0];
				r.s = t;
				if (t <= clefs[0].s) {
					return r;
				}
				for (uint32 i = 1u; i < (uint32)clefs.Size(); ++i) {
					if (t <= clefs[i].s) {
						const float32 d = clefs[i].s - clefs[i - 1u].s;
						const float32 u = d > 1e-9f ? (t - clefs[i - 1u].s) / d : 0.f;
						r.largeur = clefs[i - 1u].largeur + (clefs[i].largeur - clefs[i - 1u].largeur) * u;
						r.hauteur = clefs[i - 1u].hauteur + (clefs[i].hauteur - clefs[i - 1u].hauteur) * u;
						return r;
					}
				}
				r = clefs[(uint32)clefs.Size() - 1u];
				r.s = t;
				return r;
			}

			// Les clefs d'une CHAINE, redistribuees sur son os k (de N) : la tranche
			// [(k-1)/N, k/N] ramenee a [0, 1], bornes interpolees.
			void ClefsDeTranche(const NkVector<NkR32Clef> &clefs, float32 t0, float32 t1, NkVector<NkR32Clef> &out) {
				out.Clear();
				NkR32Clef a = ClefA(clefs, t0);
				a.s = 0.f;
				out.PushBack(a);
				for (uint32 i = 0u; i < (uint32)clefs.Size(); ++i) {
					if (clefs[i].s > t0 + 1e-6f && clefs[i].s < t1 - 1e-6f) {
						NkR32Clef c = clefs[i];
						c.s = (c.s - t0) / (t1 - t0);
						out.PushBack(c);
					}
				}
				NkR32Clef b = ClefA(clefs, t1);
				b.s = 1.f;
				out.PushBack(b);
			}

			bool AppliquerProfils(NkSceneEtat &e, char *pourquoi, uint32 cap) {
				for (uint32 k = 0u; k < (uint32)e.profils.Size(); ++k) {
					const NkSceneProfil &p = e.profils[k];
					NkVector<NkR32Clef> clefs;
					if (p.estSection) {
						NkR32Clef c;
						c.s = 0.f;
						c.largeur = p.largeur;
						c.hauteur = p.hauteur;
						clefs.PushBack(c);
						c.s = 1.f;
						clefs.PushBack(c);
					} else {
						clefs = p.clefs;
					}
					uint32 touches = 0u;
					for (uint32 c = 0u; c < (uint32)e.chaines.Size(); ++c) {
						const NkSceneChaine &ch = e.chaines[c];
						if (!Correspond(p.motif, ch.nom)) {
							continue;
						}
						const uint32 n = (uint32)ch.os.Size();
						for (uint32 b = 0u; b < n; ++b) {
							NkR32Os &o = e.doc->os[ch.os[b]];
							ClefsDeTranche(clefs, (float32)b / (float32)n, (float32)(b + 1u) / (float32)n, o.clefs);
							o.exposant = p.exposant;
							++touches;
						}
					}
					for (uint32 i = 0u; i < (uint32)e.doc->os.Size(); ++i) {
						NkR32Os &o = e.doc->os[i];
						if (!Correspond(p.motif, o.nom)) {
							continue;
						}
						o.clefs = clefs;
						o.exposant = p.exposant;
						++touches;
					}
					if (touches == 0u) {
						Dire(pourquoi, cap, "ligne %u : le profil `%s` ne designe aucun os ni aucune chaine", p.ligne,
							 p.motif);
						return false;
					}
				}
				return true;
			}

			// `_g` -> `_d` (en fin de nom, ou avant `.k` pour un os de chaine).
			bool NomMiroir(const char *nom, char *out, uint32 cap) {
				const uint32 n = (uint32)std::strlen(nom);
				Copier(out, nom, cap);
				if (n >= 2u && nom[n - 2u] == '_' && nom[n - 1u] == 'g') {
					out[n - 1u] = 'd';
					return true;
				}
				const char *p = std::strstr(nom, "_g.");
				if (p) {
					out[(uint32)(p - nom) + 1u] = 'd';
					return true;
				}
				return false;
			}

			bool AppliquerMiroir(NkSceneEtat &e, char *pourquoi, uint32 cap) {
				const uint32 n = (uint32)e.doc->os.Size();
				for (uint32 i = 0u; i < n; ++i) {
					NkR32Os o = e.doc->os[i];
					char nom[NK_R32_NOM_MAX];
					if (!NomMiroir(o.nom, nom, NK_R32_NOM_MAX)) {
						continue;
					}
					char parent[NK_R32_NOM_MAX];
					if (o.parent[0] != 0 && NomMiroir(o.parent, parent, NK_R32_NOM_MAX)) {
						Copier(o.parent, parent, NK_R32_NOM_MAX);
					}
					Copier(o.nom, nom, NK_R32_NOM_MAX);
					// Le plan de symetrie est x = 0 : x change de signe. Le repere miroir
					// renverse le sens de a -- l'adresse miroir est (os_d, s, 1 - a).
					o.racine.x = -o.racine.x;
					o.direction.x = -o.direction.x;
					o.reference.x = -o.reference.x;
					char raison[256];
					if (!e.doc->AjouterOs(o, raison, 256u)) {
						Dire(pourquoi, cap, "miroir : %s", raison);
						return false;
					}
				}
				const uint32 nc = (uint32)e.chaines.Size();
				for (uint32 c = 0u; c < nc; ++c) {
					char nom[NK_R32_NOM_MAX];
					if (!NomMiroir(e.chaines[c].nom, nom, NK_R32_NOM_MAX)) {
						continue;
					}
					NkSceneChaine ch;
					Copier(ch.nom, nom, NK_R32_NOM_MAX);
					for (uint32 b = 0u; b < (uint32)e.chaines[c].os.Size(); ++b) {
						char nb[NK_R32_NOM_MAX];
						NomMiroir(e.doc->os[e.chaines[c].os[b]].nom, nb, NK_R32_NOM_MAX);
						ch.os.PushBack((uint32)e.doc->Trouver(nb));
					}
					e.chaines.PushBack(ch);
				}
				return true;
			}

			struct NkSceneRefus {
					const char *mot;
					const char *palier;
			};

			const NkSceneRefus NK_SCENE_REFUS[] = {
				{"extremite", "une extremite (main, pied, aile, pince) est une JONCTION preparee : G2"},
				{"appendice", "un appendice part d'une adresse de surface : G2c"},
				{"repeter", "la repetition de membres : G2c"},
				{"boucle", "une chaine qui rejoint un os (g = 1) : G2c"},
				{"corps", "les plans de corps (fuseau, disque, radial, cloche, segmente) : G2b"},
				{"nageoire", "les nageoires a rayons : G2c"},
				{"aile", "les ailes : G2c"},
				{"tentacule", "les tentacules souples : G2c"},
				{"segment", "les segments rigides : G2b"},
				{"semis", "les ornements en semis : G2c"},
				{"greffe", "la greffe de gabarits : G2c"},
				{"volumes", "les volumes du blockout (C1) : G3"},
				{"volume", "les volumes du blockout (C1) : G3"},
				{"tete", "le gabarit de tete et le visage : G4"},
			};

			bool Refuse(const char *mot, uint32 ligne, char *pourquoi, uint32 cap) {
				for (uint32 k = 0u; k < (uint32)(sizeof(NK_SCENE_REFUS) / sizeof(NK_SCENE_REFUS[0])); ++k) {
					if (Egal(mot, NK_SCENE_REFUS[k].mot)) {
						Dire(pourquoi, cap, "ligne %u : `%s` n'est pas lu en G1 -- %s", ligne, mot, NK_SCENE_REFUS[k].palier);
						return true;
					}
				}
				return false;
			}

		}  // namespace

		// ============================================================
		// IMPLEMENTATIONS
		// ============================================================

		bool NkCreatureLireScene(const char *texte, NkR32Document &doc, char *pourquoi, uint32 cap, char *avertissements,
								 uint32 capAvertissements) {
			doc = NkR32Document{};
			doc.generateur = 2u;
			doc.densite = 0.f;
			// LA BANDE DE PLI PAR DEFAUT : 1,1 RAYON de part et d'autre du joint.
			// Plier a 90 degres (quaternions duaux) fait tourner l'interieur du coude
			// autour du joint : il ne se replie pas si la bande depasse r tan(45) = r.
			// Mesure du 29/09 : a 1,0 le coude du bras garde 1 pliure, a 1,1 aucune
			// (serpent, bras, jambe) ; au-dela de ~1,25 les quads de la bande
			// s'allongent (regularite < 95 % sur la jambe). 1,1 = r + 10 % de marge.
			doc.largeurPli = 1.1f;
			if (avertissements && capAvertissements > 0u) {
				avertissements[0] = 0;
			}
			if (!texte) {
				Dire(pourquoi, cap, "document vide");
				return false;
			}
			NkSceneEtat e;
			e.doc = &doc;
			// Une ligne decoupee (~6 Ko) : une seule, reutilisee. Pas de `static` ni de
			// `thread_local` : le lecteur doit rester reentrant sur toutes les cibles.
			NkSceneLigne l;
			enum class NkSceneSection : uint8 {
				Nk_SceneSection_Entete = 0,
				Nk_SceneSection_Squelette,
				Nk_SceneSection_Forme
			};
			NkSceneSection section = NkSceneSection::Nk_SceneSection_Entete;
			bool estVersion = false;
			uint32 ligne = 0u;
			const char *p = texte;
			while (*p) {
				const char *fin = p;
				while (*fin && *fin != '\n') {
					++fin;
				}
				++ligne;
				Decouper(p, fin, l);
				p = (*fin == '\n') ? fin + 1 : fin;
				if (l.n == 0u) {
					continue;
				}
				const char *m = l.jetons[0];
				if (!estVersion) {
					uint32 v = 0u;
					if (!Egal(m, "nkscene") || l.n < 2u || !Entier(l.jetons[1], v) || v != 2u) {
						Dire(pourquoi, cap, "ligne %u : le document doit commencer par `nkscene 2`", ligne);
						return false;
					}
					estVersion = true;
					continue;
				}
				// Un membre se termine a la premiere ligne qui n'est pas plus indentee que lui.
				if (e.estMembre && !(Egal(m, "os") && l.retrait > e.retraitMembre)) {
					if (Egal(m, "extremite") && l.retrait > e.retraitMembre) {
						Refuse(m, ligne, pourquoi, cap);
						return false;
					}
					e.estMembre = false;
				}
				if (Refuse(m, ligne, pourquoi, cap)) {
					return false;
				}
				if (Egal(m, "creature")) {
					if (l.n >= 2u) {
						Copier(doc.nom, l.jetons[1], sizeof(doc.nom));
					}
					for (uint32 i = 2u; i + 1u < l.n; i += 2u) {
						if (Egal(l.jetons[i], "style")) {
							Avertir(avertissements, capAvertissements, "ligne %u : `style %s` ignore (prereglages : G3)",
									ligne, l.jetons[i + 1u]);
						} else if (Egal(l.jetons[i], "symetrie")) {
							if (!Egal(l.jetons[i + 1u], "X")) {
								Dire(pourquoi, cap, "ligne %u : symetrie `%s` : seule la symetrie X est lue", ligne,
									 l.jetons[i + 1u]);
								return false;
							}
						} else {
							Dire(pourquoi, cap, "ligne %u : mot inconnu `%s` apres `creature`", ligne, l.jetons[i]);
							return false;
						}
					}
				} else if (Egal(m, "squelette")) {
					section = NkSceneSection::Nk_SceneSection_Squelette;
				} else if (Egal(m, "forme")) {
					section = NkSceneSection::Nk_SceneSection_Forme;
				} else if (Egal(m, "retouches") || Egal(m, "sculpt")) {
					Avertir(avertissements, capAvertissements,
							"ligne %u : `%s` ignore ici (la pile C3 et le sculpt se chargent a part)", ligne, m);
				} else if (section == NkSceneSection::Nk_SceneSection_Squelette && Egal(m, "os")) {
					for (uint32 i = 0u; i < l.n;) {
						if (!Egal(l.jetons[i], "os")) {
							Dire(pourquoi, cap, "ligne %u : `os` attendu, `%s` trouve", ligne, l.jetons[i]);
							return false;
						}
						if (!LireOs(e, l, i, e.estMembre, ligne, pourquoi, cap)) {
							return false;
						}
					}
				} else if (section == NkSceneSection::Nk_SceneSection_Squelette && Egal(m, "chaine")) {
					if (!LireChaine(e, l, ligne, pourquoi, cap)) {
						return false;
					}
				} else if (section == NkSceneSection::Nk_SceneSection_Squelette && Egal(m, "membre")) {
					if (l.n < 2u) {
						Dire(pourquoi, cap, "ligne %u : nom attendu apres `membre`", ligne);
						return false;
					}
					const char *nom = l.jetons[1];
					const uint32 n = (uint32)std::strlen(nom);
					e.suffixe[0] = 0;
					if (n >= 2u && nom[n - 2u] == '_' && (nom[n - 1u] == 'g' || nom[n - 1u] == 'd')) {
						Copier(e.suffixe, nom + n - 2u, sizeof(e.suffixe));
					}
					e.dernier[0] = 0;
					e.estDirectionMembre = false;
					for (uint32 i = 2u; i < l.n;) {
						if (Egal(l.jetons[i], "depuis") && i + 1u < l.n) {
							const int32 r = Resoudre(e, l.jetons[i + 1u]);
							if (r < 0) {
								Dire(pourquoi, cap, "ligne %u : `depuis %s` ne designe aucun os ni chaine", ligne,
									 l.jetons[i + 1u]);
								return false;
							}
							Copier(e.dernier, doc.os[(uint32)r].nom, NK_R32_NOM_MAX);
							i += 2u;
						} else if (Egal(l.jetons[i], "racine")) {
							i += 1u;
						} else if (Egal(l.jetons[i], "direction") && Trois(l, i + 1u, e.directionMembre)) {
							e.estDirectionMembre = true;
							i += 4u;
						} else {
							Dire(pourquoi, cap, "ligne %u : mot inconnu `%s` apres `membre`", ligne, l.jetons[i]);
							return false;
						}
					}
					e.estMembre = true;
					e.retraitMembre = l.retrait;
				} else if (section == NkSceneSection::Nk_SceneSection_Squelette && Egal(m, "miroir")) {
					if (l.n != 4u || !Egal(l.jetons[1], "*_g") || !Egal(l.jetons[2], "->") || !Egal(l.jetons[3], "*_d")) {
						Dire(pourquoi, cap, "ligne %u : seule la regle `miroir *_g -> *_d` est lue", ligne);
						return false;
					}
					e.estMiroir = true;
				} else if (section == NkSceneSection::Nk_SceneSection_Forme && Egal(m, "profil")) {
					if (!LireProfil(e, l, ligne, pourquoi, cap, avertissements, capAvertissements)) {
						return false;
					}
				} else if (section == NkSceneSection::Nk_SceneSection_Forme && Egal(m, "resolution")) {
					for (uint32 i = 1u; i < l.n; i += 2u) {
						uint32 v = 0u;
						if (Egal(l.jetons[i], "bande_pli")) {
							if (i + 1u >= l.n || !Reel(l.jetons[i + 1u], doc.largeurPli) || doc.largeurPli < 0.f) {
								Dire(pourquoi, cap, "ligne %u : `bande_pli` attend un nombre de rayons >= 0", ligne);
								return false;
							}
							continue;
						}
						if (i + 1u >= l.n || !Entier(l.jetons[i + 1u], v)) {
							Dire(pourquoi, cap, "ligne %u : `resolution anneaux M boucles_articulation K` attendu", ligne);
							return false;
						}
						if (Egal(l.jetons[i], "anneaux")) {
							doc.anneaux = v;
						} else if (Egal(l.jetons[i], "boucles_articulation")) {
							doc.boucles = v;
						} else {
							Dire(pourquoi, cap, "ligne %u : mot inconnu `%s` dans `resolution`", ligne, l.jetons[i]);
							return false;
						}
					}
				} else {
					Dire(pourquoi, cap, "ligne %u : directive inconnue `%s`%s", ligne, m,
						 section == NkSceneSection::Nk_SceneSection_Entete ? " (hors section squelette / forme)" : "");
					return false;
				}
			}
			if (!estVersion) {
				Dire(pourquoi, cap, "document vide (il doit commencer par `nkscene 2`)");
				return false;
			}
			if (e.estMiroir && !AppliquerMiroir(e, pourquoi, cap)) {
				return false;
			}
			if (!AppliquerProfils(e, pourquoi, cap)) {
				return false;
			}
			if (doc.os.Empty()) {
				Dire(pourquoi, cap, "aucun os dans le squelette");
				return false;
			}
			return true;
		}

	}  // namespace renderer
}  // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Creation Date: 2026-09-29
// ============================================================
