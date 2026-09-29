// -----------------------------------------------------------------------------
// FICHIER: Kernel\Runtime\NKRenderer\src\NKRenderer\Mesh\NkMeshR32.cpp
// DESCRIPTION: Implementation de R32 (voir NkMeshR32.h pour le pourquoi).
// AUTEUR: TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// DATE: 2026-09-29
// VERSION: 0.1.0
// -----------------------------------------------------------------------------
//
// LE CHEMIN D'UNE OPERATION AU REJEU
//   1. lire la designation (texte) ;
//   2. si elle vise un os : la faire passer par la LIGNEE posterieure a son
//      epoque (renomme, coupe, fusion, reorientation, suppression) ;
//   3. verifier la reference d'orientation GELEE (cas 4, le dangereux) ;
//   4. resoudre en faces du maillage COURANT, par `Face::origine` — jamais par
//      indice ;
//   5. appliquer par `NkMeshEditCommand::Apply`, le chemin de l'editeur, et
//      pousser la commande resolue dans le journal classique ;
//   6. donner une identite aux faces nees de l'operation (bout, flanc).
//
// ⚠️ AUCUNE EXTRUSION « A ZERO PUIS DEPLACEE ». L'adjacence de NkEditMesh est
//    POSITIONNELLE (LinkTwins sur l'identite soudee) : des sommets laisses a
//    la meme place que les anciens seraient re-soudes (cf. NkEdgeSplitParams).
//    On extrude donc directement a la distance voulue ; un CREUX passe par
//    `inserer` a profondeur negative, comme les panneaux de NkMeshFamilles.
// ⚠️ AUCUN NOMBRE N'EST ECRIT OU LU PAR LA LOCALE. `atof`/`printf("%f")`
//    donnent une virgule en fr-FR : le piege est deja paye dans ce depot
//    (ETAT_REPRISE_GENIA3D, NK_UV_ALPHA). Lecture et ecriture sont faites main.
// -----------------------------------------------------------------------------

// ============================================================
// INCLUDES
// ============================================================

// Header correspondant (TOUJOURS EN PREMIER)
#include "NKRenderer/Mesh/NkMeshR32.h"
#include "NKRenderer/Mesh/NkCreaturePeau.h"

// Standard library
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
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

			const float32 NK_R32_PI = 3.14159265358979323846f;
			const float32 NK_R32_EPS_ADRESSE = 1e-5f;
			const float32 NK_R32_EPS_REFERENCE = 1e-6f;

			// ========================================
			// CHAINES
			// ========================================

			void Copier(char *dst, const char *src, uint32 cap) {
				if (!dst || cap == 0u) {
					return;
				}
				uint32 i = 0u;
				if (src) {
					while (src[i] != 0 && i + 1u < cap) {
						dst[i] = src[i];
						++i;
					}
				}
				dst[i] = 0;
			}

			bool Egal(const char *a, const char *b) {
				return a && b && std::strcmp(a, b) == 0;
			}

			void Dire(char *out, uint32 cap, const char *fmt, ...) {
				if (!out || cap == 0u) {
					return;
				}
				va_list args;
				va_start(args, fmt);
				std::vsnprintf(out, cap, fmt, args);
				va_end(args);
			}

			// Ajoute a la fin de `out` (sans jamais deborder).
			void Ajouter(char *out, uint32 cap, const char *texte) {
				const uint32 n = (uint32)std::strlen(out);
				if (n + 1u >= cap) {
					return;
				}
				Copier(out + n, texte, cap - n);
			}

			// ========================================
			// NOMBRES SANS LOCALE
			// ========================================

			// Six decimales : assez pour une adresse (une face fait au plus 1/4 de
			// tour), et l'ecriture est la meme sur toutes les plateformes.
			void EcrireReel(char *out, uint32 cap, float32 v) {
				const bool negatif = v < 0.f;
				const float64 a = negatif ? -(float64)v : (float64)v;
				const uint64 millioniemes = (uint64)(a * 1000000.0 + 0.5);
				const uint64 entier = millioniemes / 1000000u;
				const uint64 fraction = millioniemes % 1000000u;
				std::snprintf(out, cap, "%s%llu.%06llu", negatif ? "-" : "", (unsigned long long)entier,
							  (unsigned long long)fraction);
			}

			void SauterEspaces(const char *&p) {
				while (*p == ' ' || *p == '\t') {
					++p;
				}
			}

			bool LireReel(const char *&p, float32 &v) {
				SauterEspaces(p);
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
				// ⚠️ « 0..1 » : le premier point d'une PLAGE n'est pas une virgule
				//    decimale. On ne mange le point que s'il est suivi d'un chiffre.
				if (*p == '.' && p[1] >= '0' && p[1] <= '9') {
					++p;
					float64 echelle = 0.1;
					while (*p >= '0' && *p <= '9') {
						r += echelle * (float64)(*p - '0');
						echelle *= 0.1;
						++p;
					}
				}
				v = (float32)(negatif ? -r : r);
				return true;
			}

			bool LireEntier(const char *&p, uint32 &v) {
				SauterEspaces(p);
				if (!(*p >= '0' && *p <= '9')) {
					return false;
				}
				uint64 r = 0u;
				while (*p >= '0' && *p <= '9') {
					r = r * 10u + (uint64)(*p - '0');
					++p;
				}
				v = (uint32)r;
				return true;
			}

			bool LireNom(const char *&p, char *nom, uint32 cap) {
				SauterEspaces(p);
				uint32 n = 0u;
				while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
					   *p == '_' || *p == '.') {
					if (n + 1u < cap) {
						nom[n] = *p;
						++n;
					}
					++p;
				}
				nom[n] = 0;
				return n > 0u;
			}

			bool Attendre(const char *&p, const char *mot) {
				SauterEspaces(p);
				const uint32 n = (uint32)std::strlen(mot);
				if (std::strncmp(p, mot, n) != 0) {
					return false;
				}
				p += n;
				return true;
			}

			// ========================================
			// MUTATIONS
			// ========================================

			int32 Mutation() {
				const char *e = std::getenv("NK_R32_MUTE");
				if (!e) {
					return 0;
				}
				return (int32)std::atoi(e);
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

			NkVec3f Plus(const NkVec3f &a, const NkVec3f &b) {
				return V3(a.x + b.x, a.y + b.y, a.z + b.z);
			}

			NkVec3f Moins(const NkVec3f &a, const NkVec3f &b) {
				return V3(a.x - b.x, a.y - b.y, a.z - b.z);
			}

			NkVec3f Fois(const NkVec3f &a, float32 k) {
				return V3(a.x * k, a.y * k, a.z * k);
			}

			float32 Scal(const NkVec3f &a, const NkVec3f &b) {
				return a.x * b.x + a.y * b.y + a.z * b.z;
			}

			NkVec3f Vect(const NkVec3f &a, const NkVec3f &b) {
				return V3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
			}

			float32 Norme(const NkVec3f &a) {
				return std::sqrt(Scal(a, a));
			}

			NkVec3f Unitaire(const NkVec3f &a) {
				const float32 n = Norme(a);
				if (n < 1e-12f) {
					return V3(0.f, 0.f, 0.f);
				}
				return Fois(a, 1.f / n);
			}

			bool ProcheV3(const NkVec3f &a, const NkVec3f &b, float32 eps) {
				return std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps && std::fabs(a.z - b.z) <= eps;
			}

			// a ramene dans [0, 1[.
			float32 Tour(float32 a) {
				float32 r = a - std::floor(a);
				if (r >= 1.f) {
					r = 0.f;
				}
				return r;
			}

			// ========================================
			// DESIGNATIONS
			// ========================================

			enum class NkR32Genre : uint8 {
				Nk_R32Genre_Partie = 0,
				Nk_R32Genre_Adresse,
				Nk_R32Genre_Critere,
				Nk_R32Genre_Groupe
			};

			struct NkR32Designation {
					NkR32Genre genre = NkR32Genre::Nk_R32Genre_Partie;
					char os[NK_R32_NOM_MAX] = {0};
					float32 s0 = 0.f;
					float32 s1 = 1.f;
					float32 a0 = 0.f;
					float32 a1 = 1.f;
					/** 0..5 = +X -X +Y -Y +Z -Z */
					int32 axe = 2;
					float32 seuil = 0.70710678f;
					char groupe[NK_R32_NOM_MAX] = {0};
			};

			bool LireDesignation(const char *texte, NkR32Designation &d, NkR32Cause &cause, char *pourquoi,
								 uint32 cap) {
				d = NkR32Designation{};
				const char *p = texte;
				SauterEspaces(p);
				if (Attendre(p, "zone:") || Attendre(p, "boucle:")) {
					cause = NkR32Cause::Nk_R32Cause_DesignationPasEnG0;
					Dire(pourquoi, cap,
						 "`%s` : les designations par zone de visage et par boucle arrivent en G1/G4, pas en G0",
						 texte);
					return false;
				}
				cause = NkR32Cause::Nk_R32Cause_DesignationInconnue;
				if (Attendre(p, "partie:")) {
					d.genre = NkR32Genre::Nk_R32Genre_Partie;
					if (!LireNom(p, d.os, NK_R32_NOM_MAX)) {
						Dire(pourquoi, cap, "`%s` : nom d'os attendu apres `partie:`", texte);
						return false;
					}
				} else if (Attendre(p, "groupe:")) {
					d.genre = NkR32Genre::Nk_R32Genre_Groupe;
					if (!LireNom(p, d.groupe, NK_R32_NOM_MAX)) {
						Dire(pourquoi, cap, "`%s` : nom de groupe attendu apres `groupe:`", texte);
						return false;
					}
				} else if (Attendre(p, "adresse:")) {
					d.genre = NkR32Genre::Nk_R32Genre_Adresse;
					const bool lu = Attendre(p, "(") && LireNom(p, d.os, NK_R32_NOM_MAX) && Attendre(p, ",") &&
									LireReel(p, d.s0) && Attendre(p, "..") && LireReel(p, d.s1) &&
									Attendre(p, ",") && LireReel(p, d.a0) && Attendre(p, "..") && LireReel(p, d.a1) &&
									Attendre(p, ")");
					if (!lu) {
						Dire(pourquoi, cap, "`%s` : forme attendue `adresse:(os, s0..s1, a0..a1)`", texte);
						return false;
					}
					if (d.s0 > d.s1 || d.s0 < 0.f || d.s1 > 1.f) {
						Dire(pourquoi, cap, "`%s` : la plage de s doit etre croissante et dans [0, 1]", texte);
						return false;
					}
				} else if (Attendre(p, "faces:normale>")) {
					d.genre = NkR32Genre::Nk_R32Genre_Critere;
					SauterEspaces(p);
					const char signe = *p;
					const char axe = p[1];
					if ((signe != '+' && signe != '-') || (axe != 'X' && axe != 'Y' && axe != 'Z')) {
						Dire(pourquoi, cap, "`%s` : axe attendu (+X, -X, +Y, -Y, +Z ou -Z)", texte);
						return false;
					}
					p += 2;
					d.axe = (axe == 'X' ? 0 : (axe == 'Y' ? 2 : 4)) + (signe == '-' ? 1 : 0);
					if (Attendre(p, ":")) {
						if (!LireReel(p, d.seuil)) {
							Dire(pourquoi, cap, "`%s` : seuil numerique attendu apres `:`", texte);
							return false;
						}
					}
					if (Attendre(p, "@partie:")) {
						if (!LireNom(p, d.os, NK_R32_NOM_MAX)) {
							Dire(pourquoi, cap, "`%s` : nom d'os attendu apres `@partie:`", texte);
							return false;
						}
					}
				} else {
					Dire(pourquoi, cap,
						 "`%s` : designation inconnue (partie:, adresse:, faces:normale>, groupe:)", texte);
					return false;
				}
				SauterEspaces(p);
				if (*p != 0) {
					Dire(pourquoi, cap, "`%s` : texte en trop apres la designation", texte);
					return false;
				}
				cause = NkR32Cause::Nk_R32Cause_Aucune;
				return true;
			}

			// ========================================
			// BOITES D'ADRESSE ET LIGNEE
			// ========================================

			struct NkR32Boite {
					char os[NK_R32_NOM_MAX] = {0};
					char osOrigine[NK_R32_NOM_MAX] = {0};
					float32 s0 = 0.f;
					float32 s1 = 1.f;
					float32 a0 = 0.f;
					float32 a1 = 1.f;
					bool estTourComplet = true;
					bool estViaFusion = false;
					// ⚠️ LA REFERENCE GELEE VOYAGE AVEC LA BOITE, PAS AVEC L'OPERATION.
					//    La relecture du 29/09 l'a prouve : une boite coupee en deux, puis
					//    deux reorientations (une par morceau) -- suivie par operation, la
					//    version passait a 2 au premier morceau et le second ne tournait
					//    PLUS ; la moitie de la bosse partait d'un quart de tour, statut
					//    « reportee ». Chaque morceau a sa propre histoire de reference.
					uint32 refVersion = 0u;
					NkVec3f refInstantane;
			};

			bool DansArc(float32 a, const NkR32Boite &b) {
				if (b.estTourComplet) {
					return true;
				}
				if (b.a0 <= b.a1) {
					return a >= b.a0 - NK_R32_EPS_ADRESSE && a <= b.a1 + NK_R32_EPS_ADRESSE;
				}
				return a >= b.a0 - NK_R32_EPS_ADRESSE || a <= b.a1 + NK_R32_EPS_ADRESSE;
			}

			// Chevauchement d'aire positive entre deux arcs (eventuellement a cheval sur 0).
			bool ArcsSeChevauchent(const NkR32Boite &x, const NkR32Boite &y) {
				if (x.estTourComplet || y.estTourComplet) {
					return true;
				}
				float32 xi[4];
				float32 yi[4];
				uint32 nx = 0u;
				uint32 ny = 0u;
				if (x.a0 <= x.a1) {
					xi[0] = x.a0;
					xi[1] = x.a1;
					nx = 1u;
				} else {
					xi[0] = x.a0;
					xi[1] = 1.f;
					xi[2] = 0.f;
					xi[3] = x.a1;
					nx = 2u;
				}
				if (y.a0 <= y.a1) {
					yi[0] = y.a0;
					yi[1] = y.a1;
					ny = 1u;
				} else {
					yi[0] = y.a0;
					yi[1] = 1.f;
					yi[2] = 0.f;
					yi[3] = y.a1;
					ny = 2u;
				}
				for (uint32 i = 0u; i < nx; ++i) {
					for (uint32 j = 0u; j < ny; ++j) {
						const float32 lo = xi[2u * i] > yi[2u * j] ? xi[2u * i] : yi[2u * j];
						const float32 hi = xi[2u * i + 1u] < yi[2u * j + 1u] ? xi[2u * i + 1u] : yi[2u * j + 1u];
						if (hi - lo > 1e-6f) {
							return true;
						}
					}
				}
				return false;
			}

			float32 LongueurArc(const NkR32Boite &b) {
				if (b.estTourComplet) {
					return 1.f;
				}
				if (b.a0 <= b.a1) {
					return b.a1 - b.a0;
				}
				return 1.f - b.a0 + b.a1;
			}

			float32 Recouvrement(float32 x0, float32 x1, float32 y0, float32 y1) {
				const float32 lo = x0 > y0 ? x0 : y0;
				const float32 hi = x1 < y1 ? x1 : y1;
				return hi > lo ? hi - lo : 0.f;
			}

			// UNE FACE EST VISEE SI ELLE RECOUVRE LA BOITE D'AU MOINS LA MOITIE DE
			// LA PLUS PETITE DES DEUX AIRES (en (s, a)).
			// ⚠️ « le centre de la face est dans la boite » etait la premiere regle,
			//    et elle casse des que la resolution GROSSIT : une boite plus etroite
			//    qu'une face ne contient plus aucun centre, et la retouche devenait
			//    orpheline A TORT. A resolution egale, les deux regles designent
			//    exactement les memes faces (le banc le verifie par la conversion).
			// L'aire (en (s, a)) commune a une face et a une boite.
			float32 AireCommune(const NkR32AdresseFace &adr, const NkR32Boite &b) {
				const float32 rs = Recouvrement(adr.s0, adr.s1, b.s0, b.s1);
				float32 ra = 0.f;
				if (b.estTourComplet) {
					ra = adr.a1 - adr.a0;
				} else if (b.a0 <= b.a1) {
					ra = Recouvrement(adr.a0, adr.a1, b.a0, b.a1);
				} else {
					ra = Recouvrement(adr.a0, adr.a1, b.a0, 1.f) + Recouvrement(adr.a0, adr.a1, 0.f, b.a1);
				}
				return rs * ra;
			}

			bool FaceDansBoite(const NkR32AdresseFace &adr, const NkR32Boite &b) {
				const float32 rs = Recouvrement(adr.s0, adr.s1, b.s0, b.s1);
				float32 ra = 0.f;
				if (b.estTourComplet) {
					ra = adr.a1 - adr.a0;
				} else if (b.a0 <= b.a1) {
					ra = Recouvrement(adr.a0, adr.a1, b.a0, b.a1);
				} else {
					ra = Recouvrement(adr.a0, adr.a1, b.a0, 1.f) + Recouvrement(adr.a0, adr.a1, 0.f, b.a1);
				}
				const float32 aireFace = (adr.s1 - adr.s0) * (adr.a1 - adr.a0);
				const float32 aireBoite = (b.s1 - b.s0) * LongueurArc(b);
				const float32 plusPetite = aireFace < aireBoite ? aireFace : aireBoite;
				if (plusPetite <= 1e-12f) {
					// Boite degeneree (s0 == s1) : on retombe sur le centre de la face.
					const float32 sm = 0.5f * (adr.s0 + adr.s1);
					const float32 am = 0.5f * (adr.a0 + adr.a1);
					const bool dansS = sm >= b.s0 - NK_R32_EPS_ADRESSE && sm <= b.s1 + NK_R32_EPS_ADRESSE;
					return dansS && DansArc(am, b);
				}
				return rs * ra >= 0.5f * plusPetite - 1e-9f;
			}

			void DecalerArc(NkR32Boite &b, float32 da) {
				if (b.estTourComplet) {
					return;
				}
				b.a0 = Tour(b.a0 + da);
				b.a1 = Tour(b.a1 + da);
			}

			const NkR32Confirmation *ChercherConfirmation(const NkR32Confirmation *c, uint32 n, const char *ancien,
														  const char *nouveau) {
				for (uint32 i = 0u; i < n; ++i) {
					if (Egal(c[i].ancien, ancien) && Egal(c[i].nouveau, nouveau)) {
						return &c[i];
					}
				}
				return nullptr;
			}

			// Fait traverser la lignee posterieure a l'epoque de l'operation.
			// Rend faux quand l'operation ne doit PAS s'appliquer (orpheline ou a
			// confirmer) ; `res` dit alors pourquoi, en toutes lettres.
			bool TraverserLignee(const NkR32Document &doc, const NkR32Operation &op, uint32 indiceOp,
								 const NkR32Confirmation *conf, uint32 nConf, bool verifierReference,
								 NkVector<NkR32Boite> &boites, bool &estReporte, NkR32Resultat &res) {
				const int32 mute = Mutation();
				for (uint32 k = 0u; k < (uint32)boites.Size(); ++k) {
					boites[k].refVersion = op.refVersion;
					boites[k].refInstantane = op.refInstantane;
				}
				for (uint32 i = op.epoque; i < (uint32)doc.lignee.Size(); ++i) {
					const NkR32Lignee &l = doc.lignee[i];
					NkVector<NkR32Boite> suivantes;
					for (uint32 k = 0u; k < (uint32)boites.Size(); ++k) {
						NkR32Boite b = boites[k];
						if (!Egal(b.os, l.a) && !(l.type == NkR32LigneeType::Nk_R32LigneeType_Fusion && Egal(b.os, l.b))) {
							suivantes.PushBack(b);
							continue;
						}
						switch (l.type) {
							case NkR32LigneeType::Nk_R32LigneeType_Renomme: {
								const NkR32Confirmation *c = ChercherConfirmation(conf, nConf, l.a, l.b);
								if (!c && mute != 4) {
									res.statut = NkR32Statut::Nk_R32Statut_AConfirmer;
									res.cause = NkR32Cause::Nk_R32Cause_RenommageAConfirmer;
									Dire(res.message, NK_R32_MESSAGE_MAX,
										 "la retouche %u vise `%s`, renomme en `%s` : report PROPOSE, a confirmer",
										 indiceOp, l.a, l.b);
									return false;
								}
								if (c && !c->estAccepte) {
									res.statut = NkR32Statut::Nk_R32Statut_Orpheline;
									res.cause = NkR32Cause::Nk_R32Cause_RenommageRefuse;
									Dire(res.message, NK_R32_MESSAGE_MAX,
										 "la retouche %u vise `%s` ; le report vers `%s` a ete REFUSE : orpheline",
										 indiceOp, l.a, l.b);
									return false;
								}
								Copier(b.os, l.b, NK_R32_NOM_MAX);
								estReporte = true;
								suivantes.PushBack(b);
								break;
							}
							case NkR32LigneeType::Nk_R32LigneeType_Coupe: {
								estReporte = true;
								if (mute == 2) {
									// MUTATION : le morceau du haut, s inchange.
									Copier(b.os, l.b, NK_R32_NOM_MAX);
									suivantes.PushBack(b);
									break;
								}
								const float32 t = l.t;
								if (b.s0 < t) {
									NkR32Boite h = b;
									Copier(h.os, l.b, NK_R32_NOM_MAX);
									h.s0 = b.s0 / t;
									h.s1 = ((b.s1 < t) ? b.s1 : t) / t;
									suivantes.PushBack(h);
								}
								if (b.s1 > t) {
									NkR32Boite g = b;
									Copier(g.os, l.c, NK_R32_NOM_MAX);
									g.s0 = (((b.s0 > t) ? b.s0 : t) - t) / (1.f - t);
									g.s1 = (b.s1 - t) / (1.f - t);
									suivantes.PushBack(g);
								}
								break;
							}
							case NkR32LigneeType::Nk_R32LigneeType_Fusion: {
								const float32 t = l.t;
								if (Egal(b.os, l.a)) {
									b.s0 = b.s0 * t;
									b.s1 = b.s1 * t;
								} else {
									b.s0 = t + b.s0 * (1.f - t);
									b.s1 = t + b.s1 * (1.f - t);
								}
								Copier(b.os, l.c, NK_R32_NOM_MAX);
								b.estViaFusion = true;
								estReporte = true;
								suivantes.PushBack(b);
								break;
							}
							case NkR32LigneeType::Nk_R32LigneeType_Reoriente: {
								if (b.refVersion == l.versionAvant) {
									if (mute != 1) {
										// L'angle dont la reference a tourne autour de l'os. Un point a
										// l'angle phi de l'ancienne reference est a phi - theta de la
										// nouvelle : a' = a - theta / 2pi.
										const NkVec3f ax = Unitaire(l.axe);
										const NkVec3f ra = Unitaire(Moins(l.refAvant, Fois(ax, Scal(l.refAvant, ax))));
										const NkVec3f rb = Unitaire(Moins(l.refApres, Fois(ax, Scal(l.refApres, ax))));
										const float32 theta = std::atan2(Scal(Vect(ra, rb), ax), Scal(ra, rb));
										DecalerArc(b, -theta / (2.f * NK_R32_PI));
									}
									res.alertes = (uint8)(res.alertes | NK_R32_ALERTE_REORIENTATION);
									estReporte = true;
									b.refVersion = l.versionApres;
									b.refInstantane = l.refApres;
								}
								suivantes.PushBack(b);
								break;
							}
							case NkR32LigneeType::Nk_R32LigneeType_Supprime: {
								res.statut = NkR32Statut::Nk_R32Statut_Orpheline;
								res.cause = NkR32Cause::Nk_R32Cause_OsDisparu;
								Dire(res.message, NK_R32_MESSAGE_MAX,
									 "la retouche %u vise `%s`, qui a ete supprime : orpheline GARDEE", indiceOp,
									 l.a);
								return false;
							}
						}
					}
					boites = suivantes;
				}
				for (uint32 k = 0u; k < (uint32)boites.Size(); ++k) {
					const int32 io = doc.Trouver(boites[k].os);
					if (io < 0) {
						res.statut = NkR32Statut::Nk_R32Statut_Orpheline;
						res.cause = NkR32Cause::Nk_R32Cause_OsDisparu;
						Dire(res.message, NK_R32_MESSAGE_MAX,
							 "la retouche %u vise `%s`, qui n'existe plus : orpheline GARDEE", indiceOp,
							 boites[k].os);
						return false;
					}
					if (verifierReference && mute != 1) {
						const NkR32Os &o = doc.os[(uint32)io];
						const bool memeVersion = (o.versionReference == boites[k].refVersion);
						const bool memeVecteur = ProcheV3(o.reference, boites[k].refInstantane, NK_R32_EPS_REFERENCE);
						if (!memeVersion || !memeVecteur) {
							res.statut = NkR32Statut::Nk_R32Statut_Orpheline;
							res.cause = NkR32Cause::Nk_R32Cause_ReferenceChangeeHorsVersion;
							Dire(res.message, NK_R32_MESSAGE_MAX,
								 "la retouche %u : la reference d'orientation de `%s` a change HORS "
								 "versionnement (gelee v%u, trouvee v%u, vecteur %s) ; reporter la placerait "
								 "au mauvais endroit sans le dire -- REFUSE. Passer par ReorienterOs.",
								 indiceOp, boites[k].os, boites[k].refVersion, o.versionReference,
								 memeVecteur ? "inchange" : "MODIFIE");
							return false;
						}
					}
				}
				return true;
			}

			// ========================================
			// RESOLUTION SUR LE MAILLAGE COURANT
			// ========================================

			const NkR32Origine *OrigineDe(const NkR32Etat &e, uint32 f) {
				const uint32 id = e.maillage.faces[f].origine;
				if (id == 0u || id > (uint32)e.origines.Size()) {
					return nullptr;
				}
				return &e.origines[id - 1u];
			}

			const char *OsDeBase(const NkR32Etat &e, uint32 faceBase) {
				const NkR32AdresseFace &adr = e.peau.adresses[faceBase];
				return e.peau.os[adr.os].nom;
			}

			NkVec3f AxeCritere(int32 axe) {
				const float32 s = (axe % 2 == 0) ? 1.f : -1.f;
				if (axe < 2) {
					return V3(s, 0.f, 0.f);
				}
				if (axe < 4) {
					return V3(0.f, s, 0.f);
				}
				return V3(0.f, 0.f, s);
			}

			bool OsDansBoites(const char *os, const NkVector<NkR32Boite> &boites) {
				for (uint32 k = 0u; k < (uint32)boites.Size(); ++k) {
					if (Egal(boites[k].os, os)) {
						return true;
					}
				}
				return false;
			}

			// Les faces vivantes du maillage courant que designe `d` (boites deja
			// passees par la lignee quand elles s'appliquent).
			bool Resoudre(const NkR32Etat &e, const NkR32Designation &d, const NkVector<NkR32Boite> &boites,
						  NkVector<uint32> &faces, NkR32Cause &cause) {
				faces.Clear();
				const NkEditMesh &m = e.maillage;
				if (d.genre == NkR32Genre::Nk_R32Genre_Groupe) {
					int32 g = -1;
					for (uint32 i = 0u; i < (uint32)e.groupes.Size(); ++i) {
						if (Egal(e.groupes[i].nom, d.groupe)) {
							g = (int32)i;
						}
					}
					if (g < 0) {
						cause = NkR32Cause::Nk_R32Cause_GroupeInconnu;
						return false;
					}
					NkVector<uint8> dedans;
					dedans.Resize((uint32)e.origines.Size() + 1u);
					for (uint32 i = 0u; i < (uint32)dedans.Size(); ++i) {
						dedans[i] = 0u;
					}
					const NkR32Groupe &gr = e.groupes[(uint32)g];
					for (uint32 i = 0u; i < (uint32)gr.origines.Size(); ++i) {
						if (gr.origines[i] < (uint32)dedans.Size()) {
							dedans[gr.origines[i]] = 1u;
						}
					}
					for (uint32 f = 0u; f < m.FaceCount(); ++f) {
						const uint32 id = m.faces[f].origine;
						if (m.faces[f].alive && id != 0u && id < (uint32)dedans.Size() && dedans[id]) {
							faces.PushBack(f);
						}
					}
				} else {
					const NkVec3f axe = AxeCritere(d.axe);
					for (uint32 f = 0u; f < m.FaceCount(); ++f) {
						if (!m.faces[f].alive) {
							continue;
						}
						const NkR32Origine *o = OrigineDe(e, f);
						if (!o) {
							continue;
						}
						const char *osBase = OsDeBase(e, o->faceBase);
						if (d.genre == NkR32Genre::Nk_R32Genre_Partie) {
							if (OsDansBoites(osBase, boites)) {
								faces.PushBack(f);
							}
						} else if (d.genre == NkR32Genre::Nk_R32Genre_Critere) {
							const bool dansPortee = (d.os[0] == 0) || OsDansBoites(osBase, boites);
							if (dansPortee && Scal(m.faces[f].normal, axe) > d.seuil) {
								faces.PushBack(f);
							}
						} else {
							if (o->role != NkR32Role::Nk_R32Role_Base) {
								continue;
							}
							const NkR32AdresseFace &adr = e.peau.adresses[o->faceBase];
							if (adr.lieu != NkR32Lieu::Nk_R32Lieu_Tube) {
								continue;
							}
							for (uint32 k = 0u; k < (uint32)boites.Size(); ++k) {
								const NkR32Boite &b = boites[k];
								if (Egal(osBase, b.os) && FaceDansBoite(adr, b)) {
									faces.PushBack(f);
									break;
								}
							}
						}
					}
				}
				// ── LA BOITE A CHEVAL SUR DEUX FACES (04 §10.7, reechantillonnage) ──
				// Quand la grille de la nouvelle peau est DECALEE par rapport a la
				// boite, aucune face n'en couvre la moitie : une bosse sur 0,4..0,5
				// tombe sur deux faces 0,22..0,45 et 0,45..0,67 qui en ont chacune
				// ~50 %. Mesure du 29/09 (banc R32 sur la peau G1) : la retouche
				// devenait ORPHELINE. On retient alors les faces qui recouvrent au
				// moins la moitie du MEILLEUR recouvrement, si ENSEMBLE elles couvrent
				// la moitie de la boite ; sinon la boite ne vise vraiment rien.
				// Mutation 5 : sans ce repli (le banc G1 doit rougir, cas 3).
				if (faces.Empty() && d.genre == NkR32Genre::Nk_R32Genre_Adresse && Mutation() != 5) {
					NkVector<uint32> candidats;
					NkVector<float32> aires;
					float32 meilleure = 0.f;
					float32 aireBoites = 0.f;
					for (uint32 k = 0u; k < (uint32)boites.Size(); ++k) {
						aireBoites += (boites[k].s1 - boites[k].s0) * LongueurArc(boites[k]);
					}
					for (uint32 f = 0u; f < m.FaceCount(); ++f) {
						if (!m.faces[f].alive) {
							continue;
						}
						const NkR32Origine *o = OrigineDe(e, f);
						if (!o || o->role != NkR32Role::Nk_R32Role_Base) {
							continue;
						}
						const NkR32AdresseFace &adr = e.peau.adresses[o->faceBase];
						if (adr.lieu != NkR32Lieu::Nk_R32Lieu_Tube) {
							continue;
						}
						const char *osBase = OsDeBase(e, o->faceBase);
						float32 aire = 0.f;
						for (uint32 k = 0u; k < (uint32)boites.Size(); ++k) {
							if (Egal(osBase, boites[k].os)) {
								aire += AireCommune(adr, boites[k]);
							}
						}
						if (aire > 1e-12f) {
							candidats.PushBack(f);
							aires.PushBack(aire);
							meilleure = aire > meilleure ? aire : meilleure;
						}
					}
					float32 couverte = 0.f;
					for (uint32 i = 0u; i < (uint32)candidats.Size(); ++i) {
						if (aires[i] >= 0.5f * meilleure) {
							faces.PushBack(candidats[i]);
							couverte += aires[i];
						}
					}
					if (couverte < 0.5f * aireBoites - 1e-9f) {
						faces.Clear();
					}
				}
				if (faces.Empty()) {
					cause = NkR32Cause::Nk_R32Cause_CibleVide;
					return false;
				}
				return true;
			}

			// Boites de depart d'une designation (avant lignee).
			void BoitesInitiales(const NkR32Designation &d, NkVector<NkR32Boite> &boites) {
				boites.Clear();
				if (d.genre == NkR32Genre::Nk_R32Genre_Groupe) {
					return;
				}
				if (d.genre == NkR32Genre::Nk_R32Genre_Critere && d.os[0] == 0) {
					return;
				}
				NkR32Boite b;
				Copier(b.os, d.os, NK_R32_NOM_MAX);
				Copier(b.osOrigine, d.os, NK_R32_NOM_MAX);
				if (d.genre == NkR32Genre::Nk_R32Genre_Adresse) {
					b.s0 = d.s0;
					b.s1 = d.s1;
					// ⚠️ `0.917..1.083` et `-0.083..0.083` sont le MEME arc que `0.917..0.083` :
					//    on ramene chaque borne dans [0, 1[ (une fin qui tombe pile sur un
					//    tour complet vaut 1). La premiere version TRONQUAIT a1 > 1 a 1, et
					//    l'arc perdait sa moitie en passant par 0 (relecture du 29/09).
					b.estTourComplet = (d.a1 - d.a0) >= 1.f - NK_R32_EPS_ADRESSE;
					if (b.estTourComplet) {
						b.a0 = 0.f;
						b.a1 = 1.f;
					} else {
						b.a0 = Tour(d.a0);
						b.a1 = Tour(d.a1);
						if (b.a1 <= NK_R32_EPS_ADRESSE && d.a1 > d.a0) {
							b.a1 = 1.f;
						}
					}
				}
				boites.PushBack(b);
			}

			// ========================================
			// APPLICATION
			// ========================================

			void Commande(const NkEditMesh &m, const NkVector<uint32> &faces, NkMeshEditCommand &cmd) {
				NkVector<uint8> marque;
				marque.Resize(m.VertCount());
				for (uint32 i = 0u; i < m.VertCount(); ++i) {
					marque[i] = 0u;
				}
				NkVector<NkEmId> fv;
				for (uint32 k = 0u; k < (uint32)faces.Size(); ++k) {
					m.GetFaceVerts((NkEmId)faces[k], fv);
					for (uint32 j = 0u; j < (uint32)fv.Size(); ++j) {
						if (fv[j] < m.VertCount()) {
							marque[fv[j]] = 1u;
						}
					}
				}
				cmd.selection.Clear();
				for (uint32 i = 0u; i < m.VertCount(); ++i) {
					if (marque[i]) {
						cmd.selection.PushBack(i);
					}
				}
				cmd.faceSel.Clear();
				cmd.faceSel.Resize(m.FaceCount());
				for (uint32 f = 0u; f < m.FaceCount(); ++f) {
					cmd.faceSel[f] = 0u;
				}
				for (uint32 k = 0u; k < (uint32)faces.Size(); ++k) {
					cmd.faceSel[faces[k]] = 1u;
				}
			}

			NkVec3f NormaleMoyenne(const NkEditMesh &m, const NkVector<uint32> &faces) {
				NkVec3f n = V3(0.f, 0.f, 0.f);
				for (uint32 k = 0u; k < (uint32)faces.Size(); ++k) {
					const float32 aire = m.FaceArea((NkEmId)faces[k]);
					n = Plus(n, Fois(m.faces[faces[k]].normal, aire));
				}
				return Unitaire(n);
			}

			uint32 NouvelleOrigine(NkR32Etat &e, uint32 parent, int32 op, NkR32Role role) {
				NkR32Origine o;
				o.parent = parent;
				o.faceBase = (parent != 0u) ? e.origines[parent - 1u].faceBase : 0u;
				o.op = op;
				o.role = role;
				e.origines.PushBack(o);
				return (uint32)e.origines.Size();
			}

			// Apres une operation qui fait naitre des faces (extruder, inserer) :
			// les faces de la cible dont TOUS les sommets sont selectionnes sont le
			// BOUT (les sommets neufs sont ceux que l'operation laisse selectionnes) ;
			// les autres faces de meme origine sont les FLANCS.
			void Identifier(NkR32Etat &e, const NkVector<uint8> &cibleOrig, int32 op, NkVector<uint32> &bouts) {
				NkEditMesh &m = e.maillage;
				NkVector<uint32> flancDe;
				flancDe.Resize((uint32)cibleOrig.Size());
				for (uint32 i = 0u; i < (uint32)flancDe.Size(); ++i) {
					flancDe[i] = 0u;
				}
				NkVector<NkEmId> fv;
				bouts.Clear();
				for (uint32 f = 0u; f < m.FaceCount(); ++f) {
					const uint32 id = m.faces[f].origine;
					if (!m.faces[f].alive || id == 0u || id >= (uint32)cibleOrig.Size() || !cibleOrig[id]) {
						continue;
					}
					m.GetFaceVerts((NkEmId)f, fv);
					bool tousChoisis = !fv.Empty();
					for (uint32 j = 0u; j < (uint32)fv.Size(); ++j) {
						if (fv[j] >= m.VertCount() || !m.verts[fv[j]].sel) {
							tousChoisis = false;
						}
					}
					if (tousChoisis) {
						const uint32 nid = NouvelleOrigine(e, id, op, NkR32Role::Nk_R32Role_Bout);
						m.faces[f].origine = nid;
						bouts.PushBack(nid);
						// UN GROUPE SUIT LE BOUT DE SES MEMBRES. Sans cela, extruder
						// `groupe:g` sans renommer le resultat vidait g : la mere prend
						// une nouvelle identite, et g designait un identifiant que plus
						// aucune face ne porte (relecture du 29/09).
						for (uint32 g = 0u; g < (uint32)e.groupes.Size(); ++g) {
							NkVector<uint32> &membres = e.groupes[g].origines;
							for (uint32 q = 0u; q < (uint32)membres.Size(); ++q) {
								if (membres[q] == id) {
									membres[q] = nid;
								}
							}
						}
					} else {
						if (flancDe[id] == 0u) {
							flancDe[id] = NouvelleOrigine(e, id, op, NkR32Role::Nk_R32Role_Flanc);
						}
						m.faces[f].origine = flancDe[id];
					}
				}
			}

			void PoserGroupe(NkR32Etat &e, const char *nom, const NkVector<uint32> &origines) {
				if (!nom || nom[0] == 0) {
					return;
				}
				for (uint32 i = 0u; i < (uint32)e.groupes.Size(); ++i) {
					if (Egal(e.groupes[i].nom, nom)) {
						e.groupes[i].origines = origines;
						return;
					}
				}
				NkR32Groupe g;
				Copier(g.nom, nom, NK_R32_NOM_MAX);
				g.origines = origines;
				e.groupes.PushBack(g);
			}

			bool Appliquer(NkR32Etat &e, const NkR32Operation &op, int32 indiceOp, const NkVector<uint32> &faces,
						   const NkVector<NkR32Repere> &reps, const NkR32Document &doc, char *pourquoi, uint32 cap) {
				NkEditMesh &m = e.maillage;
				NkVector<uint8> cibleOrig;
				cibleOrig.Resize((uint32)e.origines.Size() + 1u);
				for (uint32 i = 0u; i < (uint32)cibleOrig.Size(); ++i) {
					cibleOrig[i] = 0u;
				}
				NkVector<uint32> origCible;
				for (uint32 k = 0u; k < (uint32)faces.Size(); ++k) {
					const uint32 id = m.faces[faces[k]].origine;
					if (id != 0u && id < (uint32)cibleOrig.Size() && !cibleOrig[id]) {
						cibleOrig[id] = 1u;
						origCible.PushBack(id);
					}
				}
				NkMeshEditCommand cmd;
				Commande(m, faces, cmd);
				switch (op.verbe) {
					case NkR32Verbe::Nk_R32Verbe_Extruder: {
						if (!(op.valeur > 0.f)) {
							Dire(pourquoi, cap,
								 "extruder demande une distance POSITIVE (un creux passe par `inserer` a "
								 "profondeur negative)");
							return false;
						}
						cmd.op = NkMeshEditOp::Extrude;
						cmd.extrude.offset = op.valeur;
						cmd.extrude.direction = NkExtrudeParams::Region;
						cmd.extrude.individual = false;
						if (!cmd.Apply(m)) {
							Dire(pourquoi, cap, "l'extrusion a echoue sur %u faces", (uint32)faces.Size());
							return false;
						}
						e.journal.Push(cmd);
						NkVector<uint32> bouts;
						Identifier(e, cibleOrig, indiceOp, bouts);
						if (bouts.Empty()) {
							Dire(pourquoi, cap, "l'extrusion n'a laisse aucun bout identifiable");
							return false;
						}
						PoserGroupe(e, op.groupe, bouts);
						return true;
					}
					case NkR32Verbe::Nk_R32Verbe_Inserer: {
						cmd.op = NkMeshEditOp::Inset;
						cmd.inset.thickness = op.valeur;
						cmd.inset.depth = op.valeur2;
						cmd.inset.individual = false;
						if (!cmd.Apply(m)) {
							Dire(pourquoi, cap, "l'insertion a echoue sur %u faces", (uint32)faces.Size());
							return false;
						}
						e.journal.Push(cmd);
						NkVector<uint32> bouts;
						Identifier(e, cibleOrig, indiceOp, bouts);
						if (bouts.Empty()) {
							Dire(pourquoi, cap, "l'insertion n'a laisse aucune face interieure identifiable");
							return false;
						}
						PoserGroupe(e, op.groupe, bouts);
						return true;
					}
					case NkR32Verbe::Nk_R32Verbe_Deplacer:
					case NkR32Verbe::Nk_R32Verbe_Echelle: {
						NkVector<NkVec3f> deltas;
						deltas.Resize((uint32)cmd.selection.Size());
						if (op.verbe == NkR32Verbe::Nk_R32Verbe_Deplacer) {
							// LE REPERE LOCAL : la normale de la cible, la tangente de l'os,
							// la bitangente. Il suit l'os quand l'os tourne ou s'allonge.
							const NkVec3f n = NormaleMoyenne(m, faces);
							NkVec3f t = V3(0.f, 1.f, 0.f);
							const NkR32Origine *o = OrigineDe(e, faces[0]);
							if (o) {
								const int32 io = doc.Trouver(OsDeBase(e, o->faceBase));
								if (io >= 0) {
									t = reps[(uint32)io].axe;
								}
							}
							NkVec3f b = Unitaire(Vect(n, t));
							if (Norme(b) < 0.5f) {
								b = Unitaire(Vect(n, V3(1.f, 0.f, 0.f)));
							}
							t = Vect(b, n);
							const NkVec3f d = Plus(Plus(Fois(n, op.local.x), Fois(t, op.local.y)), Fois(b, op.local.z));
							for (uint32 k = 0u; k < (uint32)deltas.Size(); ++k) {
								deltas[k] = d;
							}
						} else {
							NkVec3f c = V3(0.f, 0.f, 0.f);
							for (uint32 k = 0u; k < (uint32)cmd.selection.Size(); ++k) {
								c = Plus(c, m.verts[cmd.selection[k]].pos);
							}
							c = Fois(c, 1.f / (float32)cmd.selection.Size());
							for (uint32 k = 0u; k < (uint32)deltas.Size(); ++k) {
								const NkVec3f p = m.verts[cmd.selection[k]].pos;
								deltas[k] = Moins(Plus(c, Fois(Moins(p, c), op.valeur)), p);
							}
						}
						cmd.op = NkMeshEditOp::Move;
						cmd.moveDeltas = deltas;
						if (!cmd.Apply(m)) {
							Dire(pourquoi, cap, "le deplacement n'a rien change");
							return false;
						}
						m.RecomputeNormals();
						e.journal.Push(cmd);
						PoserGroupe(e, op.groupe, origCible);
						return true;
					}
					case NkR32Verbe::Nk_R32Verbe_Supprimer: {
						cmd.op = NkMeshEditOp::Delete;
						if (!cmd.Apply(m)) {
							Dire(pourquoi, cap, "la suppression a echoue");
							return false;
						}
						e.journal.Push(cmd);
						return true;
					}
				}
				return false;
			}

			// ========================================
			// NOMS DES VALEURS (texte de la pile)
			// ========================================

			const char *NomAuteur(NkR32Auteur a) {
				switch (a) {
					case NkR32Auteur::Nk_R32Auteur_Humain: return "humain";
					case NkR32Auteur::Nk_R32Auteur_Auto:   return "auto";
					case NkR32Auteur::Nk_R32Auteur_Ia:     return "ia";
				}
				return "humain";
			}

			const char *NomVerbe(NkR32Verbe v) {
				switch (v) {
					case NkR32Verbe::Nk_R32Verbe_Extruder:  return "extruder";
					case NkR32Verbe::Nk_R32Verbe_Deplacer:  return "deplacer";
					case NkR32Verbe::Nk_R32Verbe_Echelle:   return "echelle";
					case NkR32Verbe::Nk_R32Verbe_Inserer:   return "inserer";
					case NkR32Verbe::Nk_R32Verbe_Supprimer: return "supprimer";
				}
				return "extruder";
			}

		}  // namespace

		// ============================================================
		// IMPLEMENTATIONS
		// ============================================================

		// ========================================
		// REPERES ET SECTIONS (publics : partages par la peau G1 et les bancs)
		// ========================================

		bool NkR32CalculerReperes(const NkR32Document &doc, NkVector<NkR32Repere> &reps, char *pourquoi, uint32 cap) {
			reps.Clear();
			reps.Resize((uint32)doc.os.Size());
			for (uint32 i = 0u; i < (uint32)doc.os.Size(); ++i) {
				const NkR32Os &o = doc.os[i];
				NkR32Repere r;
				if (o.parent[0] == 0) {
					r.racine = o.racine;
				} else {
					int32 p = -1;
					for (uint32 j = 0u; j < i; ++j) {
						if (Egal(doc.os[j].nom, o.parent)) {
							p = (int32)j;
						}
					}
					if (p < 0) {
						Dire(pourquoi, cap, "l'os `%s` a pour parent `%s`, inconnu ou declare APRES lui", o.nom,
							 o.parent);
						return false;
					}
					const NkR32Repere &rp = reps[(uint32)p];
					r.racine = Plus(rp.racine, Fois(rp.axe, rp.longueur));
				}
				r.axe = Unitaire(o.direction);
				if (Norme(r.axe) < 0.5f) {
					Dire(pourquoi, cap, "l'os `%s` a une direction nulle", o.nom);
					return false;
				}
				const NkVec3f ref = Unitaire(o.reference);
				if (Norme(ref) < 0.5f || std::fabs(Scal(ref, r.axe)) > 0.99f) {
					Dire(pourquoi, cap,
						 "la reference d'orientation de `%s` est nulle ou presque parallele a l'os : a = 0 n'y "
						 "est pas defini",
						 o.nom);
					return false;
				}
				r.e1 = Unitaire(Moins(ref, Fois(r.axe, Scal(ref, r.axe))));
				r.e2 = Vect(r.axe, r.e1);
				r.longueur = o.longueur;
				reps[i] = r;
			}
			return true;
		}

		void NkR32Section(const NkR32Os &o, float32 s, float32 &demiLargeur, float32 &demiHauteur) {
			if (o.clefs.Empty()) {
				const float32 r = o.rayon0 + (o.rayon1 - o.rayon0) * s;
				demiLargeur = r;
				demiHauteur = r;
				return;
			}
			const uint32 n = (uint32)o.clefs.Size();
			if (s <= o.clefs[0].s || n == 1u) {
				demiLargeur = 0.5f * o.clefs[0].largeur;
				demiHauteur = 0.5f * o.clefs[0].hauteur;
				return;
			}
			for (uint32 i = 1u; i < n; ++i) {
				if (s <= o.clefs[i].s) {
					const NkR32Clef &c0 = o.clefs[i - 1u];
					const NkR32Clef &c1 = o.clefs[i];
					const float32 d = c1.s - c0.s;
					const float32 t = d > 1e-9f ? (s - c0.s) / d : 0.f;
					demiLargeur = 0.5f * (c0.largeur + (c1.largeur - c0.largeur) * t);
					demiHauteur = 0.5f * (c0.hauteur + (c1.hauteur - c0.hauteur) * t);
					return;
				}
			}
			demiLargeur = 0.5f * o.clefs[n - 1u].largeur;
			demiHauteur = 0.5f * o.clefs[n - 1u].hauteur;
		}

		// ── a EST UNE FRACTION DE PERIMETRE, PAS UN ANGLE DE PARAMETRE ─────────
		// Sur un cercle, les deux coincident (et G0 reste identique au bit : voir
		// le chemin du cercle dans NkR32PointSection). Sur une ellipse ou une superellipse, un parametre theta
		// uniforme ENTASSE les sommets pres des flancs : mesure du 29/09, un pied en
		// superellipse aplatie (0,09 x 0,05, exposant 3) montrait des quads de
		// rapport 6 a 8. En prenant a = fraction de perimetre depuis la reference,
		// les sommets sont equidistants autour de l'os, et a garde un sens stable.
		// Le perimetre est integre en NK_R32_ARC_ECHANTILLONS cordes : deterministe.
		namespace {
			const uint32 NK_R32_ARC_ECHANTILLONS = 512u;

			bool EstCercle(float32 A, float32 B, float32 n) {
				return n == 2.f && A == B;
			}

			void PointParametre(float32 A, float32 B, float32 n, float32 th, float32 &x, float32 &y) {
				const float32 c = std::cos(th);
				const float32 sn = std::sin(th);
				if (n == 2.f) {
					x = A * c;
					y = B * sn;
					return;
				}
				const float32 e = 2.f / n;
				const float32 ac = std::pow(std::fabs(c), e);
				const float32 as = std::pow(std::fabs(sn), e);
				x = A * (c < 0.f ? -ac : ac);
				y = B * (sn < 0.f ? -as : as);
			}

			// theta tel que l'arc [0, theta] soit la fraction `a` du perimetre.
			float32 ThetaDeA(float32 A, float32 B, float32 n, float32 a) {
				if (EstCercle(A, B, n)) {
					return 2.f * NK_R32_PI * a;
				}
				float32 cumul[NK_R32_ARC_ECHANTILLONS + 1u];
				cumul[0] = 0.f;
				float32 px = 0.f;
				float32 py = 0.f;
				PointParametre(A, B, n, 0.f, px, py);
				for (uint32 k = 1u; k <= NK_R32_ARC_ECHANTILLONS; ++k) {
					float32 x = 0.f;
					float32 y = 0.f;
					PointParametre(A, B, n, 2.f * NK_R32_PI * (float32)k / (float32)NK_R32_ARC_ECHANTILLONS, x, y);
					cumul[k] = cumul[k - 1u] + std::sqrt((x - px) * (x - px) + (y - py) * (y - py));
					px = x;
					py = y;
				}
				const float32 cible = (a - std::floor(a)) * cumul[NK_R32_ARC_ECHANTILLONS];
				uint32 k = 1u;
				while (k < NK_R32_ARC_ECHANTILLONS && cumul[k] < cible) {
					++k;
				}
				const float32 d = cumul[k] - cumul[k - 1u];
				const float32 t = d > 1e-20f ? (cible - cumul[k - 1u]) / d : 0.f;
				return 2.f * NK_R32_PI * ((float32)(k - 1u) + t) / (float32)NK_R32_ARC_ECHANTILLONS;
			}

			// L'inverse : la fraction de perimetre de [0, theta].
			float32 ADeTheta(float32 A, float32 B, float32 n, float32 th) {
				float32 t = th / (2.f * NK_R32_PI);
				t = t - std::floor(t);
				if (EstCercle(A, B, n)) {
					return t;
				}
				float32 total = 0.f;
				float32 jusque = 0.f;
				float32 px = 0.f;
				float32 py = 0.f;
				PointParametre(A, B, n, 0.f, px, py);
				const float32 kt = t * (float32)NK_R32_ARC_ECHANTILLONS;
				for (uint32 k = 1u; k <= NK_R32_ARC_ECHANTILLONS; ++k) {
					float32 x = 0.f;
					float32 y = 0.f;
					PointParametre(A, B, n, 2.f * NK_R32_PI * (float32)k / (float32)NK_R32_ARC_ECHANTILLONS, x, y);
					const float32 l = std::sqrt((x - px) * (x - px) + (y - py) * (y - py));
					total += l;
					if ((float32)k <= kt) {
						jusque += l;
					} else if ((float32)(k - 1u) < kt) {
						jusque += l * (kt - (float32)(k - 1u));
					}
					px = x;
					py = y;
				}
				return total > 1e-20f ? jusque / total : t;
			}
		}  // namespace

		NkVec3f NkR32PointSection(const NkR32Repere &r, const NkR32Os &o, float32 s, float32 a, NkVec3f *normale) {
			float32 A = 0.f;
			float32 B = 0.f;
			NkR32Section(o, s, A, B);
			const float32 th = ThetaDeA(A, B, o.exposant, a);
			if (EstCercle(A, B, o.exposant)) {
				// Le cercle est calcule EXACTEMENT comme en G0 (meme ordre des
				// operations) : l'empreinte des peaux G0 livrees ne bouge pas d'un bit.
				// Mesure : l'ordre e1 (A cos) au lieu de (e1 cos) A changeait
				// l'empreinte de base (cff553b84f65b9f8 -> 9721d9b589ed66d7).
				const NkVec3f u = Plus(Fois(r.e1, std::cos(th)), Fois(r.e2, std::sin(th)));
				if (normale) {
					*normale = u;
				}
				return Plus(Plus(r.racine, Fois(r.axe, s * r.longueur)), Fois(u, A));
			}
			const float32 c = std::cos(th);
			const float32 sn = std::sin(th);
			float32 x = A * c;
			float32 y = B * sn;
			float32 nx = c / (A > 1e-12f ? A : 1e-12f);
			float32 ny = sn / (B > 1e-12f ? B : 1e-12f);
			if (o.exposant != 2.f) {
				const float32 e = 2.f / o.exposant;
				const float32 ac = std::pow(std::fabs(c), e);
				const float32 as = std::pow(std::fabs(sn), e);
				x = A * (c < 0.f ? -ac : ac);
				y = B * (sn < 0.f ? -as : as);
				// Gradient de |x/A|^n + |y/B|^n : la normale de la superellipse.
				const float32 gx = std::pow(std::fabs(x / A), o.exposant - 1.f) / A;
				const float32 gy = std::pow(std::fabs(y / B), o.exposant - 1.f) / B;
				nx = x < 0.f ? -gx : gx;
				ny = y < 0.f ? -gy : gy;
			}
			if (normale) {
				*normale = Unitaire(Plus(Fois(r.e1, nx), Fois(r.e2, ny)));
			}
			return Plus(Plus(r.racine, Fois(r.axe, s * r.longueur)), Plus(Fois(r.e1, x), Fois(r.e2, y)));
		}


		// ========================================
		// NkR32Document
		// ========================================

		int32 NkR32Document::Trouver(const char *nom) const {
			for (uint32 i = 0u; i < (uint32)os.Size(); ++i) {
				if (Egal(os[i].nom, nom)) {
					return (int32)i;
				}
			}
			return -1;
		}

		bool NkR32Document::AjouterOs(const NkR32Os &o, char *pourquoi, uint32 cap) {
			if (o.nom[0] == 0) {
				Dire(pourquoi, cap, "un os doit avoir un nom (c'est la cle de toute adresse)");
				return false;
			}
			if (Trouver(o.nom) >= 0) {
				Dire(pourquoi, cap, "l'os `%s` existe deja : un nom en double est refuse", o.nom);
				return false;
			}
			if (o.parent[0] != 0 && Trouver(o.parent) < 0) {
				Dire(pourquoi, cap, "l'os `%s` a pour parent `%s`, inconnu", o.nom, o.parent);
				return false;
			}
			if (!(o.longueur > 0.f) || !(o.rayon0 > 0.f) || !(o.rayon1 > 0.f)) {
				Dire(pourquoi, cap, "l'os `%s` doit avoir une longueur et des rayons positifs", o.nom);
				return false;
			}
			os.PushBack(o);
			return true;
		}

		bool NkR32Document::RenommerOs(const char *ancien, const char *nouveau, char *pourquoi, uint32 cap) {
			const int32 i = Trouver(ancien);
			if (i < 0) {
				Dire(pourquoi, cap, "renommer : l'os `%s` n'existe pas", ancien);
				return false;
			}
			if (!nouveau || nouveau[0] == 0 || Trouver(nouveau) >= 0) {
				Dire(pourquoi, cap, "renommer : le nom `%s` est vide ou deja pris", nouveau ? nouveau : "");
				return false;
			}
			Copier(os[(uint32)i].nom, nouveau, NK_R32_NOM_MAX);
			for (uint32 k = 0u; k < (uint32)os.Size(); ++k) {
				if (Egal(os[k].parent, ancien)) {
					Copier(os[k].parent, nouveau, NK_R32_NOM_MAX);
				}
			}
			NkR32Lignee l;
			l.type = NkR32LigneeType::Nk_R32LigneeType_Renomme;
			Copier(l.a, ancien, NK_R32_NOM_MAX);
			Copier(l.b, nouveau, NK_R32_NOM_MAX);
			lignee.PushBack(l);
			return true;
		}

		bool NkR32Document::CouperOs(const char *nom, float32 t, const char *haut, const char *bas, char *pourquoi,
									 uint32 cap) {
			const int32 i = Trouver(nom);
			if (i < 0) {
				Dire(pourquoi, cap, "couper : l'os `%s` n'existe pas", nom);
				return false;
			}
			if (!(t > 0.f && t < 1.f)) {
				Dire(pourquoi, cap, "couper `%s` : t doit etre strictement entre 0 et 1", nom);
				return false;
			}
			if (Trouver(haut) >= 0 || Trouver(bas) >= 0 || Egal(haut, bas)) {
				Dire(pourquoi, cap, "couper `%s` : `%s` ou `%s` existe deja", nom, haut, bas);
				return false;
			}
			const NkR32Os o = os[(uint32)i];
			const float32 rt = o.rayon0 + (o.rayon1 - o.rayon0) * t;
			NkR32Os h = o;
			Copier(h.nom, haut, NK_R32_NOM_MAX);
			h.longueur = o.longueur * t;
			h.rayon1 = rt;
			NkR32Os g = o;
			Copier(g.nom, bas, NK_R32_NOM_MAX);
			Copier(g.parent, haut, NK_R32_NOM_MAX);
			g.longueur = o.longueur * (1.f - t);
			g.rayon0 = rt;
			// LE PROFIL SE COUPE AVEC L'OS : chaque morceau garde ses clefs, ramenees
			// a son propre s, et une clef interpolee a la coupure. Sans cela la peau
			// des deux morceaux ne serait pas celle de l'os entier.
			if (!o.clefs.Empty()) {
				float32 wt = 0.f;
				float32 ht = 0.f;
				NkR32Section(o, t, wt, ht);
				NkR32Clef ct;
				ct.largeur = 2.f * wt;
				ct.hauteur = 2.f * ht;
				h.clefs.Clear();
				g.clefs.Clear();
				for (uint32 k = 0u; k < (uint32)o.clefs.Size(); ++k) {
					NkR32Clef c = o.clefs[k];
					if (c.s < t) {
						c.s = c.s / t;
						h.clefs.PushBack(c);
					}
				}
				ct.s = 1.f;
				h.clefs.PushBack(ct);
				ct.s = 0.f;
				g.clefs.PushBack(ct);
				for (uint32 k = 0u; k < (uint32)o.clefs.Size(); ++k) {
					NkR32Clef c = o.clefs[k];
					if (c.s > t) {
						c.s = (c.s - t) / (1.f - t);
						g.clefs.PushBack(c);
					}
				}
			}
			NkVector<NkR32Os> nouveaux;
			for (uint32 k = 0u; k < (uint32)os.Size(); ++k) {
				if (k == (uint32)i) {
					nouveaux.PushBack(h);
					nouveaux.PushBack(g);
					continue;
				}
				NkR32Os c = os[k];
				if (Egal(c.parent, nom)) {
					Copier(c.parent, bas, NK_R32_NOM_MAX);
				}
				nouveaux.PushBack(c);
			}
			os = nouveaux;
			NkR32Lignee l;
			l.type = NkR32LigneeType::Nk_R32LigneeType_Coupe;
			Copier(l.a, nom, NK_R32_NOM_MAX);
			Copier(l.b, haut, NK_R32_NOM_MAX);
			Copier(l.c, bas, NK_R32_NOM_MAX);
			l.t = t;
			lignee.PushBack(l);
			return true;
		}

		bool NkR32Document::FusionnerOs(const char *haut, const char *bas, const char *nouveau, char *pourquoi,
										uint32 cap) {
			const int32 ih = Trouver(haut);
			const int32 ib = Trouver(bas);
			if (ih < 0 || ib < 0) {
				Dire(pourquoi, cap, "fusionner : `%s` ou `%s` n'existe pas", haut, bas);
				return false;
			}
			const NkR32Os h = os[(uint32)ih];
			const NkR32Os g = os[(uint32)ib];
			if (!Egal(g.parent, haut)) {
				Dire(pourquoi, cap, "fusionner : `%s` n'a pas `%s` pour parent", bas, haut);
				return false;
			}
			uint32 enfantsHaut = 0u;
			for (uint32 k = 0u; k < (uint32)os.Size(); ++k) {
				if (Egal(os[k].parent, haut)) {
					++enfantsHaut;
				}
			}
			if (enfantsHaut != 1u) {
				Dire(pourquoi, cap, "fusionner : `%s` porte d'autres os que `%s` ; ils perdraient leur point d'attache",
					 haut, bas);
				return false;
			}
			if (h.versionReference != g.versionReference ||
				!ProcheV3(h.reference, g.reference, NK_R32_EPS_REFERENCE)) {
				Dire(pourquoi, cap,
					 "fusionner : `%s` et `%s` n'ont pas la meme reference d'orientation ; reorienter d'abord, "
					 "EXPLICITEMENT",
					 haut, bas);
				return false;
			}
			// ⚠️ G0 NE FUSIONNE QUE DES OS COLINEAIRES. L'os fusionne a pour axe la
			//    corde des deux ; si les deux n'etaient pas alignes, la reference y
			//    serait projetee autrement et a = 0 TOURNERAIT sans lignee ni alerte
			//    (relecture du 29/09). Une fusion coudee est une jonction : G2.
			if (Scal(Unitaire(h.direction), Unitaire(g.direction)) < 1.f - 1e-6f) {
				Dire(pourquoi, cap,
					 "fusionner : `%s` et `%s` ne sont pas alignes ; a = 0 tournerait sans rien dire -- refuse en G0",
					 haut, bas);
				return false;
			}
			if (h.exposant != g.exposant) {
				Dire(pourquoi, cap, "fusionner : `%s` et `%s` n'ont pas le meme exposant de section", haut, bas);
				return false;
			}
			if (Trouver(nouveau) >= 0 && !Egal(nouveau, haut) && !Egal(nouveau, bas)) {
				Dire(pourquoi, cap, "fusionner : le nom `%s` est deja pris", nouveau);
				return false;
			}
			NkVector<NkR32Repere> reps;
			if (!NkR32CalculerReperes(*this, reps, pourquoi, cap)) {
				return false;
			}
			const NkR32Repere &rh = reps[(uint32)ih];
			const NkR32Repere &rb = reps[(uint32)ib];
			const NkVec3f fin = Plus(rb.racine, Fois(rb.axe, rb.longueur));
			NkR32Os n = h;
			Copier(n.nom, nouveau, NK_R32_NOM_MAX);
			n.direction = Unitaire(Moins(fin, rh.racine));
			n.longueur = Norme(Moins(fin, rh.racine));
			n.rayon1 = g.rayon1;
			// Le profil fusionne : les clefs des deux morceaux, ramenees au s du tout.
			if (!h.clefs.Empty() || !g.clefs.Empty()) {
				const float32 tf = h.longueur / (h.longueur + g.longueur);
				NkR32Os hc = h;
				NkR32Os gc = g;
				n.clefs.Clear();
				if (hc.clefs.Empty()) {
					NkR32Clef c;
					c.s = 0.f;
					c.largeur = 2.f * h.rayon0;
					c.hauteur = 2.f * h.rayon0;
					hc.clefs.PushBack(c);
				}
				if (gc.clefs.Empty()) {
					NkR32Clef c;
					c.s = 1.f;
					c.largeur = 2.f * g.rayon1;
					c.hauteur = 2.f * g.rayon1;
					gc.clefs.PushBack(c);
				}
				for (uint32 k = 0u; k < (uint32)hc.clefs.Size(); ++k) {
					NkR32Clef c = hc.clefs[k];
					c.s = c.s * tf;
					n.clefs.PushBack(c);
				}
				for (uint32 k = 0u; k < (uint32)gc.clefs.Size(); ++k) {
					NkR32Clef c = gc.clefs[k];
					c.s = tf + c.s * (1.f - tf);
					if (!n.clefs.Empty() && c.s <= n.clefs[(uint32)n.clefs.Size() - 1u].s + 1e-6f) {
						continue;
					}
					n.clefs.PushBack(c);
				}
			}
			NkVector<NkR32Os> nouveaux;
			for (uint32 k = 0u; k < (uint32)os.Size(); ++k) {
				if (k == (uint32)ib) {
					continue;
				}
				if (k == (uint32)ih) {
					nouveaux.PushBack(n);
					continue;
				}
				NkR32Os c = os[k];
				if (Egal(c.parent, bas)) {
					Copier(c.parent, nouveau, NK_R32_NOM_MAX);
				}
				nouveaux.PushBack(c);
			}
			os = nouveaux;
			NkR32Lignee l;
			l.type = NkR32LigneeType::Nk_R32LigneeType_Fusion;
			Copier(l.a, haut, NK_R32_NOM_MAX);
			Copier(l.b, bas, NK_R32_NOM_MAX);
			Copier(l.c, nouveau, NK_R32_NOM_MAX);
			l.t = h.longueur / (h.longueur + g.longueur);
			lignee.PushBack(l);
			return true;
		}

		bool NkR32Document::ReorienterOs(const char *nom, const NkVec3f &nouvelleReference, char *pourquoi,
										 uint32 cap) {
			const int32 i = Trouver(nom);
			if (i < 0) {
				Dire(pourquoi, cap, "reorienter : l'os `%s` n'existe pas", nom);
				return false;
			}
			NkR32Os &o = os[(uint32)i];
			NkR32Lignee l;
			l.type = NkR32LigneeType::Nk_R32LigneeType_Reoriente;
			Copier(l.a, nom, NK_R32_NOM_MAX);
			l.refAvant = o.reference;
			l.refApres = nouvelleReference;
			l.axe = Unitaire(o.direction);
			l.versionAvant = o.versionReference;
			l.versionApres = o.versionReference + 1u;
			o.reference = nouvelleReference;
			o.versionReference = l.versionApres;
			lignee.PushBack(l);
			return true;
		}

		bool NkR32Document::SupprimerOs(const char *nom, char *pourquoi, uint32 cap) {
			const int32 i = Trouver(nom);
			if (i < 0) {
				Dire(pourquoi, cap, "supprimer : l'os `%s` n'existe pas", nom);
				return false;
			}
			for (uint32 k = 0u; k < (uint32)os.Size(); ++k) {
				if (Egal(os[k].parent, nom)) {
					Dire(pourquoi, cap, "supprimer : `%s` porte encore `%s`", nom, os[k].nom);
					return false;
				}
			}
			NkVector<NkR32Os> nouveaux;
			for (uint32 k = 0u; k < (uint32)os.Size(); ++k) {
				if (k != (uint32)i) {
					nouveaux.PushBack(os[k]);
				}
			}
			os = nouveaux;
			NkR32Lignee l;
			l.type = NkR32LigneeType::Nk_R32LigneeType_Supprime;
			Copier(l.a, nom, NK_R32_NOM_MAX);
			lignee.PushBack(l);
			return true;
		}

		// ========================================
		// C2 — LA PEAU
		// ========================================

		bool NkR32ConstruirePeau(const NkR32Document &doc, NkR32Peau &out, char *pourquoi, uint32 cap) {
			if (doc.generateur == 2u) {
				return NkCreatureConstruirePeau(doc, out, pourquoi, cap);
			}
			if (doc.generateur != 1u) {
				Dire(pourquoi, cap, "generateur %u inconnu (1 = tubes G0, 2 = chaines G1)", doc.generateur);
				return false;
			}
			out.maillage.Clear();
			out.adresses.Clear();
			out.sommets.Clear();
			out.os = doc.os;
			if (doc.anneaux < 4u || (doc.anneaux % 2u) != 0u) {
				Dire(pourquoi, cap, "anneaux = %u : il faut un nombre PAIR et au moins 4 (04 §5.4)", doc.anneaux);
				return false;
			}
			if (!(doc.densite > 0.f)) {
				Dire(pourquoi, cap, "densite nulle : aucun anneau le long des os");
				return false;
			}
			if (doc.os.Empty()) {
				Dire(pourquoi, cap, "aucun os : pas de peau");
				return false;
			}
			NkVector<NkR32Repere> reps;
			if (!NkR32CalculerReperes(doc, reps, pourquoi, cap)) {
				return false;
			}
			const uint32 M = doc.anneaux;
			// ── LES OS QUI PROLONGENT EXACTEMENT LEUR PARENT PARTAGENT SON ANNEAU ──
			// ⚠️ SANS CELA, COUPER UN OS FABRIQUAIT DEUX ANNEAUX CONFONDUS (la fin du
			//    morceau haut, le debut du morceau bas). L'adjacence de NkEditMesh est
			//    POSITIONNELLE : ils auraient ete soudes, avec des aretes portees par
			//    quatre faces. La regle est etroite a dessein -- meme axe, meme
			//    reference, meme rayon au joint, enfant UNIQUE : c'est le cas d'une
			//    coupe, pas celui d'une articulation (qui est une jonction, G2).
			NkVector<int32> prolonge;
			NkVector<uint32> enfants;
			prolonge.Resize((uint32)doc.os.Size());
			enfants.Resize((uint32)doc.os.Size());
			for (uint32 io = 0u; io < (uint32)doc.os.Size(); ++io) {
				prolonge[io] = -1;
				enfants[io] = 0u;
			}
			for (uint32 io = 0u; io < (uint32)doc.os.Size(); ++io) {
				const int32 p = doc.Trouver(doc.os[io].parent);
				if (p >= 0) {
					enfants[(uint32)p] += 1u;
				}
			}
			for (uint32 io = 0u; io < (uint32)doc.os.Size(); ++io) {
				const int32 p = doc.Trouver(doc.os[io].parent);
				if (p < 0 || enfants[(uint32)p] != 1u) {
					continue;
				}
				const NkR32Repere &rp = reps[(uint32)p];
				const NkR32Repere &rc = reps[io];
				const bool memeAxe = Scal(rp.axe, rc.axe) > 1.f - 1e-6f;
				const bool memeRef = ProcheV3(rp.e1, rc.e1, 1e-6f);
				float32 ap = 0.f;
				float32 bp = 0.f;
				float32 ac = 0.f;
				float32 bc = 0.f;
				NkR32Section(doc.os[(uint32)p], 1.f, ap, bp);
				NkR32Section(doc.os[io], 0.f, ac, bc);
				const bool memeRayon = std::fabs(ap - ac) < 1e-6f && std::fabs(bp - bc) < 1e-6f &&
									   doc.os[(uint32)p].exposant == doc.os[io].exposant;
				if (memeAxe && memeRef && memeRayon) {
					prolonge[io] = p;
				}
			}
			NkVector<uint8> estProlonge;
			estProlonge.Resize((uint32)doc.os.Size());
			for (uint32 io = 0u; io < (uint32)doc.os.Size(); ++io) {
				estProlonge[io] = 0u;
			}
			for (uint32 io = 0u; io < (uint32)doc.os.Size(); ++io) {
				if (prolonge[io] >= 0) {
					estProlonge[(uint32)prolonge[io]] = 1u;
				}
			}
			NkVector<uint32> dernierAnneau;
			dernierAnneau.Resize((uint32)doc.os.Size());
			NkVector<NkVertex3D> sommets;
			NkVector<uint32> debut;
			NkVector<uint32> boucles;
			NkVector<NkEditMesh::FaceAttrib> attribs;
			debut.PushBack(0u);
			for (uint32 io = 0u; io < (uint32)doc.os.Size(); ++io) {
				const NkR32Repere &r = reps[io];
				uint32 pas = (uint32)(r.longueur * doc.densite + 0.5f);
				if (pas < 1u) {
					pas = 1u;
				}
				const uint32 nAnneaux = pas + 1u;
				const bool partage = prolonge[io] >= 0;
				// Indice du sommet (i, j) : l'anneau 0 d'un os qui prolonge son parent
				// EST le dernier anneau du parent.
				const uint32 premier = (uint32)sommets.Size();
				const uint32 anneau0 = partage ? dernierAnneau[(uint32)prolonge[io]] : premier;
				auto indice = [&](uint32 i, uint32 j) -> uint32 {
					if (i == 0u) {
						return anneau0 + j;
					}
					return premier + (partage ? (i - 1u) : i) * M + j;
				};
				for (uint32 i = partage ? 1u : 0u; i < nAnneaux; ++i) {
					const float32 s = (float32)i / (float32)pas;
					for (uint32 j = 0u; j < M; ++j) {
						const float32 a = (float32)j / (float32)M;
						NkVec3f radial;
						NkVertex3D v{};
						v.pos = NkR32PointSection(r, doc.os[io], s, a, &radial);
						v.normal = radial;
						v.tangent = r.axe;
						v.uv = NkVec2f{a, s};
						v.uv2 = NkVec2f{a, s};
						v.color = 0xFFFFFFFFu;
						sommets.PushBack(v);
					}
				}
				// Tube : l'ordre (i,j) (i+1,j) (i+1,j+1) (i,j+1) donne une normale
				// SORTANTE dans la convention de NkEditMesh.
				// ⚠️ LA PREMIERE VERSION AVAIT L'ORDRE INVERSE, deduit « sur le papier ».
				//    Le banc (base/normales-sortantes) a mesure 0/276 : toute la peau
				//    etait retournee, et chaque extrusion partait VERS L'INTERIEUR --
				//    alors que les six operations se disaient « appliquees ».
				for (uint32 i = 0u; i < pas; ++i) {
					for (uint32 j = 0u; j < M; ++j) {
						const uint32 j1 = (j + 1u) % M;
						boucles.PushBack(indice(i, j));
						boucles.PushBack(indice(i + 1u, j));
						boucles.PushBack(indice(i + 1u, j1));
						boucles.PushBack(indice(i, j1));
						debut.PushBack((uint32)boucles.Size());
						NkR32AdresseFace adr;
						adr.os = (uint16)io;
						adr.lieu = NkR32Lieu::Nk_R32Lieu_Tube;
						adr.s0 = (float32)i / (float32)pas;
						adr.s1 = (float32)(i + 1u) / (float32)pas;
						adr.a0 = (float32)j / (float32)M;
						adr.a1 = (float32)(j + 1u) / (float32)M;
						out.adresses.PushBack(adr);
					}
				}
				dernierAnneau[io] = indice(pas, 0u);
				// Les deux bouts : un n-gone chacun (G1 les remplacera par des capuchons
				// en quads) -- sauf au joint d'une chaine droite, ou il n'y a pas de bout.
				if (!partage) {
					for (uint32 j = 0u; j < M; ++j) {
						boucles.PushBack(indice(0u, j));
					}
					debut.PushBack((uint32)boucles.Size());
					NkR32AdresseFace adr;
					adr.os = (uint16)io;
					adr.lieu = NkR32Lieu::Nk_R32Lieu_BoutDebut;
					adr.a1 = 1.f;
					out.adresses.PushBack(adr);
				}
				if (!estProlonge[io]) {
					for (uint32 j = 0u; j < M; ++j) {
						boucles.PushBack(indice(pas, M - 1u - j));
					}
					debut.PushBack((uint32)boucles.Size());
					NkR32AdresseFace adr;
					adr.os = (uint16)io;
					adr.lieu = NkR32Lieu::Nk_R32Lieu_BoutFin;
					adr.s0 = 1.f;
					adr.s1 = 1.f;
					adr.a1 = 1.f;
					out.adresses.PushBack(adr);
				}
			}
			const uint32 nFaces = (uint32)debut.Size() - 1u;
			attribs.Resize(nFaces);
			for (uint32 f = 0u; f < nFaces; ++f) {
				attribs[f] = NkEditMesh::FaceAttrib{};
				attribs[f].origine = f + 1u;
			}
			out.maillage.BuildFromPolygons(sommets.Data(), (uint32)sommets.Size(), debut.Data(), nFaces,
										   boucles.Data(), attribs.Data());
			out.maillage.RecomputeNormals();
			if (out.maillage.FaceCount() != nFaces) {
				Dire(pourquoi, cap, "BuildFromPolygons a rendu %u faces pour %u : l'adresse f -> f+1 ne tient plus",
					 out.maillage.FaceCount(), nFaces);
				return false;
			}
			for (uint32 f = 0u; f < nFaces; ++f) {
				if (out.maillage.faces[f].origine != f + 1u) {
					Dire(pourquoi, cap, "la face %u ne porte pas son origine : l'ordre des faces a change", f);
					return false;
				}
			}
			return true;
		}

		bool NkR32PointAnalytique(const NkR32Document &doc, const char *os, float32 s, float32 a, NkVec3f &point,
								  NkVec3f &normale, float32 *rayon) {
			NkVector<NkR32Repere> reps;
			char pourquoi[NK_R32_MESSAGE_MAX];
			if (!NkR32CalculerReperes(doc, reps, pourquoi, NK_R32_MESSAGE_MAX)) {
				return false;
			}
			const int32 i = doc.Trouver(os);
			if (i < 0) {
				return false;
			}
			const NkR32Repere &r = reps[(uint32)i];
			point = NkR32PointSection(r, doc.os[(uint32)i], s, a, &normale);
			if (rayon) {
				// Distance a l'axe DANS LA DIRECTION a : c'est le rayon qu'un point de
				// peau a en (s, a), cercle ou superellipse.
				const NkVec3f v = Moins(point, Plus(r.racine, Fois(r.axe, s * r.longueur)));
				*rayon = Norme(v);
			}
			return true;
		}

		bool NkR32Projeter(const NkR32Document &doc, const char *os, const NkVec3f &p, float32 &s, float32 &a,
						   float32 &rho) {
			NkVector<NkR32Repere> reps;
			char pourquoi[NK_R32_MESSAGE_MAX];
			if (!NkR32CalculerReperes(doc, reps, pourquoi, NK_R32_MESSAGE_MAX)) {
				return false;
			}
			const int32 i = doc.Trouver(os);
			if (i < 0) {
				return false;
			}
			const NkR32Repere &r = reps[(uint32)i];
			const NkVec3f v = Moins(p, r.racine);
			const float32 le = Scal(v, r.axe);
			s = le / r.longueur;
			const NkVec3f radial = Moins(v, Fois(r.axe, le));
			rho = Norme(radial);
			// Le parametre theta de la superellipse sur ce rayon (invariant par
			// homothetie), puis la fraction de perimetre -- l'inverse exact de
			// NkR32PointSection.
			const NkR32Os &o = doc.os[(uint32)i];
			float32 A = 0.f;
			float32 B = 0.f;
			const float32 sc = s < 0.f ? 0.f : (s > 1.f ? 1.f : s);
			NkR32Section(o, sc, A, B);
			const float32 X = Scal(radial, r.e1) / (A > 1e-12f ? A : 1e-12f);
			const float32 Y = Scal(radial, r.e2) / (B > 1e-12f ? B : 1e-12f);
			const float32 e = 0.5f * o.exposant;
			const float32 cx = (X < 0.f ? -1.f : 1.f) * std::pow(std::fabs(X), e);
			const float32 cy = (Y < 0.f ? -1.f : 1.f) * std::pow(std::fabs(Y), e);
			a = Tour(ADeTheta(A, B, o.exposant, std::atan2(cy, cx)));
			return true;
		}

		// ========================================
		// C3 — LA PILE
		// ========================================

		bool NkR32Pile::Ajouter(const NkR32Document &doc, const NkR32Operation &op, char *pourquoi, uint32 cap) {
			NkR32Designation d;
			NkR32Cause cause = NkR32Cause::Nk_R32Cause_Aucune;
			if (!LireDesignation(op.cible, d, cause, pourquoi, cap)) {
				if (cause != NkR32Cause::Nk_R32Cause_DesignationPasEnG0) {
					return false;
				}
			}
			NkR32Operation o = op;
			o.epoque = (uint32)doc.lignee.Size();
			o.refVersion = 0u;
			o.refInstantane = V3(0.f, 0.f, 0.f);
			if (cause == NkR32Cause::Nk_R32Cause_Aucune && d.genre == NkR32Genre::Nk_R32Genre_Adresse) {
				const int32 i = doc.Trouver(d.os);
				if (i < 0) {
					Dire(pourquoi, cap, "`%s` : l'os `%s` n'existe pas dans le document", op.cible, d.os);
					return false;
				}
				o.refVersion = doc.os[(uint32)i].versionReference;
				o.refInstantane = doc.os[(uint32)i].reference;
			}
			ops.PushBack(o);
			return true;
		}

		void NkR32PileEcrire(const NkR32Pile &p, char *out, uint32 cap) {
			if (!out || cap == 0u) {
				return;
			}
			out[0] = 0;
			char ligne[512];
			char r[8][40];
			for (uint32 i = 0u; i < (uint32)p.ops.Size(); ++i) {
				const NkR32Operation &o = p.ops[i];
				EcrireReel(r[0], 40u, o.valeur);
				EcrireReel(r[1], 40u, o.valeur2);
				EcrireReel(r[2], 40u, o.local.x);
				EcrireReel(r[3], 40u, o.local.y);
				EcrireReel(r[4], 40u, o.local.z);
				EcrireReel(r[5], 40u, o.refInstantane.x);
				EcrireReel(r[6], 40u, o.refInstantane.y);
				EcrireReel(r[7], 40u, o.refInstantane.z);
				std::snprintf(ligne, sizeof(ligne),
							  "op auteur=%s verbe=%s valeur=%s valeur2=%s local=%s;%s;%s epoque=%u ref=%u;%s;%s;%s "
							  "groupe=%s cible=%s\n",
							  NomAuteur(o.auteur), NomVerbe(o.verbe), r[0], r[1], r[2], r[3], r[4], o.epoque,
							  o.refVersion, r[5], r[6], r[7], o.groupe[0] ? o.groupe : "-", o.cible);
				Ajouter(out, cap, ligne);
			}
		}

		bool NkR32PileLire(const char *texte, NkR32Pile &p, char *pourquoi, uint32 cap) {
			p.ops.Clear();
			const char *c = texte;
			uint32 numero = 0u;
			while (c && *c) {
				++numero;
				SauterEspaces(c);
				if (*c == '\n') {
					++c;
					continue;
				}
				NkR32Operation o;
				char mot[32];
				bool lu = Attendre(c, "op") && Attendre(c, "auteur=") && LireNom(c, mot, 32u);
				if (lu) {
					if (Egal(mot, "humain")) {
						o.auteur = NkR32Auteur::Nk_R32Auteur_Humain;
					} else if (Egal(mot, "auto")) {
						o.auteur = NkR32Auteur::Nk_R32Auteur_Auto;
					} else if (Egal(mot, "ia")) {
						o.auteur = NkR32Auteur::Nk_R32Auteur_Ia;
					} else {
						lu = false;
					}
				}
				lu = lu && Attendre(c, "verbe=") && LireNom(c, mot, 32u);
				if (lu) {
					if (Egal(mot, "extruder")) {
						o.verbe = NkR32Verbe::Nk_R32Verbe_Extruder;
					} else if (Egal(mot, "deplacer")) {
						o.verbe = NkR32Verbe::Nk_R32Verbe_Deplacer;
					} else if (Egal(mot, "echelle")) {
						o.verbe = NkR32Verbe::Nk_R32Verbe_Echelle;
					} else if (Egal(mot, "inserer")) {
						o.verbe = NkR32Verbe::Nk_R32Verbe_Inserer;
					} else if (Egal(mot, "supprimer")) {
						o.verbe = NkR32Verbe::Nk_R32Verbe_Supprimer;
					} else {
						lu = false;
					}
				}
				lu = lu && Attendre(c, "valeur=") && LireReel(c, o.valeur);
				lu = lu && Attendre(c, "valeur2=") && LireReel(c, o.valeur2);
				lu = lu && Attendre(c, "local=") && LireReel(c, o.local.x) && Attendre(c, ";") &&
					 LireReel(c, o.local.y) && Attendre(c, ";") && LireReel(c, o.local.z);
				lu = lu && Attendre(c, "epoque=") && LireEntier(c, o.epoque);
				lu = lu && Attendre(c, "ref=") && LireEntier(c, o.refVersion) && Attendre(c, ";") &&
					 LireReel(c, o.refInstantane.x) && Attendre(c, ";") && LireReel(c, o.refInstantane.y) &&
					 Attendre(c, ";") && LireReel(c, o.refInstantane.z);
				lu = lu && Attendre(c, "groupe=");
				if (lu) {
					if (Attendre(c, "-")) {
						o.groupe[0] = 0;
					} else {
						lu = LireNom(c, o.groupe, NK_R32_NOM_MAX);
					}
				}
				lu = lu && Attendre(c, "cible=");
				if (!lu) {
					Dire(pourquoi, cap, "pile, ligne %u : forme non reconnue", numero);
					return false;
				}
				uint32 n = 0u;
				while (*c && *c != '\n' && *c != '\r') {
					if (n + 1u < NK_R32_CIBLE_MAX) {
						o.cible[n] = *c;
						++n;
					}
					++c;
				}
				while (n > 0u && o.cible[n - 1u] == ' ') {
					--n;
				}
				o.cible[n] = 0;
				while (*c == '\r' || *c == '\n') {
					++c;
				}
				p.ops.PushBack(o);
			}
			return true;
		}

		// ========================================
		// LE REJEU
		// ========================================

		bool NkR32Rejouer(const NkR32Document &doc, const NkR32Pile &pile, const NkR32Confirmation *confirmations,
						  uint32 nbConfirmations, NkR32Etat &out, char *pourquoi, uint32 cap) {
			out.origines.Clear();
			out.groupes.Clear();
			out.resultats.Clear();
			out.journal.Clear();
			if (!NkR32ConstruirePeau(doc, out.peau, pourquoi, cap)) {
				return false;
			}
			NkVector<NkR32Repere> reps;
			if (!NkR32CalculerReperes(doc, reps, pourquoi, cap)) {
				return false;
			}
			out.maillage = out.peau.maillage;
			for (uint32 f = 0u; f < (uint32)out.peau.adresses.Size(); ++f) {
				NkR32Origine o;
				o.faceBase = f;
				out.origines.PushBack(o);
			}
			const int32 mute = Mutation();
			bool estInterrompu = false;
			// ── LES COLLISIONS SE DETECTENT SUR LES FACES, PAS SUR LES BOITES ──
			// ⚠️ LA PREMIERE VERSION COMPARAIT LES BOITES, et le banc l'a prise en
			//    defaut : apres une fusion, deux retouches ADJACENTES (0,4..0,5 et
			//    0,5..0,6) retombaient, a la resolution de l'os fusionne, sur LA MEME
			//    face -- deplacee deux fois, 0,0765 au lieu de 0,04, et rien ne le
			//    disait. Deux boites qui se touchent ne se recouvrent pas ; deux
			//    retouches qui visent la meme face, si. C'est ce qui compte.
			NkVector<uint32> dejaFaceBase;  // faces de base visees, a plat
			NkVector<uint32> dejaOp;  // l'operation qui visait chacune
			NkVector<uint8> dejaFusion;
			NkVector<uint32> dejaOrigineNom;  // indice dans nomsOrigine
			NkVector<NkR32Boite> nomsOrigine;
			for (uint32 k = 0u; k < (uint32)pile.ops.Size(); ++k) {
				const NkR32Operation &op = pile.ops[k];
				NkR32Resultat res;
				if (estInterrompu) {
					res.statut = NkR32Statut::Nk_R32Statut_Orpheline;
					res.cause = NkR32Cause::Nk_R32Cause_RejeuInterrompu;
					Dire(res.message, NK_R32_MESSAGE_MAX, "la retouche %u n'a pas ete rejouee (rejeu interrompu)", k);
					out.resultats.PushBack(res);
					continue;
				}
				out.maillage.RecomputeNormals();
				NkR32Designation d;
				NkR32Cause cause = NkR32Cause::Nk_R32Cause_Aucune;
				char raison[NK_R32_MESSAGE_MAX];
				raison[0] = 0;
				bool estAppliquable = LireDesignation(op.cible, d, cause, raison, NK_R32_MESSAGE_MAX);
				if (!estAppliquable) {
					res.statut = NkR32Statut::Nk_R32Statut_Orpheline;
					res.cause = cause;
					Dire(res.message, NK_R32_MESSAGE_MAX, "la retouche %u : %s", k, raison);
				}
				NkVector<NkR32Boite> boites;
				bool estReporte = false;
				if (estAppliquable) {
					BoitesInitiales(d, boites);
					const bool verifierRef = (d.genre == NkR32Genre::Nk_R32Genre_Adresse);
					estAppliquable = TraverserLignee(doc, op, k, confirmations, nbConfirmations, verifierRef, boites,
													 estReporte, res);
				}
				NkVector<uint32> faces;
				if (estAppliquable) {
					NkR32Cause c2 = NkR32Cause::Nk_R32Cause_Aucune;
					if (!Resoudre(out, d, boites, faces, c2)) {
						estAppliquable = false;
						res.statut = NkR32Statut::Nk_R32Statut_Orpheline;
						res.cause = c2;
						if (c2 == NkR32Cause::Nk_R32Cause_GroupeInconnu) {
							Dire(res.message, NK_R32_MESSAGE_MAX,
								 "la retouche %u vise le groupe `%s`, qu'aucune operation n'a produit", k, d.groupe);
						} else {
							Dire(res.message, NK_R32_MESSAGE_MAX,
								 "la retouche %u : `%s` ne designe AUCUNE face sur ce maillage", k, op.cible);
						}
					}
				}
				NkVector<uint32> baseVisees;
				if (estAppliquable) {
					for (uint32 f = 0u; f < (uint32)faces.Size(); ++f) {
						const NkR32Origine *o = OrigineDe(out, faces[f]);
						if (o && o->role == NkR32Role::Nk_R32Role_Base) {
							baseVisees.PushBack(o->faceBase);
						}
					}
				}
				if (estAppliquable) {
					if (!Appliquer(out, op, (int32)k, faces, reps, doc, raison, NK_R32_MESSAGE_MAX)) {
						estAppliquable = false;
						res.statut = NkR32Statut::Nk_R32Statut_Orpheline;
						res.cause = NkR32Cause::Nk_R32Cause_OperationEchouee;
						Dire(res.message, NK_R32_MESSAGE_MAX, "la retouche %u : %s", k, raison);
					}
				}
				if (estAppliquable) {
					res.statut = estReporte ? NkR32Statut::Nk_R32Statut_Reportee : NkR32Statut::Nk_R32Statut_Appliquee;
					res.cause = NkR32Cause::Nk_R32Cause_Aucune;
					res.facesVisees = (uint32)faces.Size();
					Dire(res.message, NK_R32_MESSAGE_MAX, "la retouche %u : %s sur %u faces", k,
						 estReporte ? "REPORTEE" : "appliquee", res.facesVisees);
					if (res.alertes & NK_R32_ALERTE_REORIENTATION) {
						Ajouter(res.message, NK_R32_MESSAGE_MAX,
								" -- ATTENTION : la reference d'orientation a ete reorientee (versionnee), a verifier");
					}
					if (d.genre == NkR32Genre::Nk_R32Genre_Adresse && !boites.Empty()) {
						bool viaFusion = false;
						for (uint32 b = 0u; b < (uint32)boites.Size(); ++b) {
							viaFusion = viaFusion || boites[b].estViaFusion;
						}
						NkR32Boite nom;
						Copier(nom.osOrigine, boites[0].osOrigine, NK_R32_NOM_MAX);
						nomsOrigine.PushBack(nom);
						const uint32 iNom = (uint32)nomsOrigine.Size() - 1u;
						NkVector<uint32> deja;  // operations deja signalees pour celle-ci
						for (uint32 q = 0u; q < (uint32)dejaFaceBase.Size(); ++q) {
							const bool fusion = viaFusion || dejaFusion[q] != 0u;
							const bool autreOs = !Egal(nomsOrigine[dejaOrigineNom[q]].osOrigine, nom.osOrigine);
							if (!fusion || !autreOs) {
								continue;
							}
							bool commune = false;
							for (uint32 f = 0u; f < (uint32)baseVisees.Size() && !commune; ++f) {
								commune = (baseVisees[f] == dejaFaceBase[q]);
							}
							bool dejaDit = false;
							for (uint32 w = 0u; w < (uint32)deja.Size(); ++w) {
								dejaDit = dejaDit || deja[w] == dejaOp[q];
							}
							if (!commune || dejaDit) {
								continue;
							}
							deja.PushBack(dejaOp[q]);
							char note[96];
							res.alertes = (uint8)(res.alertes | NK_R32_ALERTE_COLLISION);
							Dire(note, 96u, " -- COLLISION avec la retouche %u apres fusion", dejaOp[q]);
							Ajouter(res.message, NK_R32_MESSAGE_MAX, note);
							NkR32Resultat &autre = out.resultats[dejaOp[q]];
							if (!(autre.alertes & NK_R32_ALERTE_COLLISION)) {
								autre.alertes = (uint8)(autre.alertes | NK_R32_ALERTE_COLLISION);
								Dire(note, 96u, " -- COLLISION avec la retouche %u apres fusion", k);
								Ajouter(autre.message, NK_R32_MESSAGE_MAX, note);
							}
						}
						for (uint32 f = 0u; f < (uint32)baseVisees.Size(); ++f) {
							dejaFaceBase.PushBack(baseVisees[f]);
							dejaOp.PushBack(k);
							dejaFusion.PushBack(viaFusion ? 1u : 0u);
							dejaOrigineNom.PushBack(iNom);
						}
					}
				} else if (mute == 3) {
					estInterrompu = true;
				}
				out.resultats.PushBack(res);
			}
			out.maillage.RecomputeNormals();
			return true;
		}

		// ========================================
		// SELECTION LIBRE -> ADRESSE
		// ========================================

		bool NkR32ConvertirSelection(const NkR32Etat &etat, const uint8 *faceSel, uint32 nbFaces, char *outCible,
									 uint32 capCible, char *pourquoi, uint32 cap) {
			if (!faceSel || !outCible) {
				Dire(pourquoi, cap, "selection absente");
				return false;
			}
			const NkEditMesh &m = etat.maillage;
			int32 os = -1;
			float32 s0 = 1.f;
			float32 s1 = 0.f;
			uint32 choisies = 0u;
			// Les intervalles d'arc des faces choisies, EXACTS (bornes des faces).
			// ⚠️ La premiere version les rangeait sur une grille de 4096 : l'adresse
			//    ecrite devenait 0.166748 au lieu de 1/6 -- une adresse approchee,
			//    donc un report approche.
			NkVector<float32> arcDebut;
			NkVector<float32> arcFin;
			for (uint32 f = 0u; f < nbFaces && f < m.FaceCount(); ++f) {
				if (!faceSel[f] || !m.faces[f].alive) {
					continue;
				}
				const NkR32Origine *o = OrigineDe(etat, f);
				if (!o || o->role != NkR32Role::Nk_R32Role_Base) {
					Dire(pourquoi, cap,
						 "la face %u n'est pas une face d'origine de la peau : la viser par `groupe:` ou `faces:`", f);
					return false;
				}
				const NkR32AdresseFace &adr = etat.peau.adresses[o->faceBase];
				if (adr.lieu != NkR32Lieu::Nk_R32Lieu_Tube) {
					Dire(pourquoi, cap, "la face %u ferme un bout d'os : G0 ne sait pas l'adresser", f);
					return false;
				}
				if (os >= 0 && (int32)adr.os != os) {
					Dire(pourquoi, cap, "la selection couvre plusieurs os : une adresse par os (en faire deux retouches)");
					return false;
				}
				os = (int32)adr.os;
				s0 = adr.s0 < s0 ? adr.s0 : s0;
				s1 = adr.s1 > s1 ? adr.s1 : s1;
				arcDebut.PushBack(adr.a0);
				arcFin.PushBack(adr.a1);
				++choisies;
			}
			if (choisies == 0u) {
				Dire(pourquoi, cap, "selection vide");
				return false;
			}
			// L'arc le plus court qui couvre la selection : le complement du plus
			// grand trou entre intervalles (tries par debut, trou circulaire compris).
			const uint32 n = (uint32)arcDebut.Size();
			for (uint32 i = 1u; i < n; ++i) {
				for (uint32 j = i; j > 0u && arcDebut[j] < arcDebut[j - 1u]; --j) {
					const float32 td = arcDebut[j];
					const float32 tf = arcFin[j];
					arcDebut[j] = arcDebut[j - 1u];
					arcFin[j] = arcFin[j - 1u];
					arcDebut[j - 1u] = td;
					arcFin[j - 1u] = tf;
				}
			}
			float32 finCourante = arcFin[0];
			float32 meilleurTrou = 0.f;
			float32 a0 = 0.f;
			float32 a1 = 1.f;
			for (uint32 i = 1u; i < n; ++i) {
				const float32 trou = arcDebut[i] - finCourante;
				if (trou > meilleurTrou + NK_R32_EPS_ADRESSE) {
					meilleurTrou = trou;
					a0 = arcDebut[i];
					a1 = finCourante;
				}
				if (arcFin[i] > finCourante) {
					finCourante = arcFin[i];
				}
			}
			const float32 trouCirculaire = 1.f - finCourante + arcDebut[0];
			if (trouCirculaire > meilleurTrou + NK_R32_EPS_ADRESSE) {
				meilleurTrou = trouCirculaire;
				a0 = arcDebut[0];
				a1 = finCourante;
			}
			if (meilleurTrou <= NK_R32_EPS_ADRESSE) {
				a0 = 0.f;
				a1 = 1.f;
			}
			char e[4][40];
			EcrireReel(e[0], 40u, s0);
			EcrireReel(e[1], 40u, s1);
			EcrireReel(e[2], 40u, a0);
			EcrireReel(e[3], 40u, a1);
			Dire(outCible, capCible, "adresse:(%s, %s..%s, %s..%s)", etat.peau.os[(uint32)os].nom, e[0], e[1], e[2],
				 e[3]);
			// LA BOITE DOIT ETRE LA SELECTION, ni plus ni moins : sinon la retouche
			// rejouee viserait AUTRE CHOSE que ce que la main a choisi.
			NkVector<uint32> faces;
			char raison[NK_R32_MESSAGE_MAX];
			if (!NkR32Designer(etat, outCible, faces, raison, NK_R32_MESSAGE_MAX)) {
				Dire(pourquoi, cap, "conversion : %s", raison);
				return false;
			}
			bool identique = ((uint32)faces.Size() == choisies);
			for (uint32 k = 0u; k < (uint32)faces.Size() && identique; ++k) {
				if (faces[k] >= nbFaces || !faceSel[faces[k]]) {
					identique = false;
				}
			}
			if (!identique) {
				Dire(pourquoi, cap,
					 "la selection (%u faces) n'est pas une boite en (s, a) : sa boite en couvre %u. G0 ne la "
					 "convertit pas sans en changer le sens",
					 choisies, (uint32)faces.Size());
				return false;
			}
			return true;
		}

		bool NkR32Designer(const NkR32Etat &etat, const char *cible, NkVector<uint32> &faces, char *pourquoi,
						   uint32 cap) {
			NkR32Designation d;
			NkR32Cause cause = NkR32Cause::Nk_R32Cause_Aucune;
			if (!LireDesignation(cible, d, cause, pourquoi, cap)) {
				return false;
			}
			NkVector<NkR32Boite> boites;
			BoitesInitiales(d, boites);
			if (!Resoudre(etat, d, boites, faces, cause)) {
				Dire(pourquoi, cap, "`%s` : %s", cible, NkR32CauseNom(cause));
				return false;
			}
			return true;
		}

		uint64 NkR32Empreinte(const NkEditMesh &m) {
			NkVector<NkVertex3D> v;
			NkVector<uint32> debut;
			NkVector<uint32> boucles;
			m.ToPolygons(v, debut, boucles);
			uint64 h = 1469598103934665603ull;
			auto melanger = [&h](uint32 x) {
				for (uint32 i = 0u; i < 4u; ++i) {
					h ^= (uint64)((x >> (8u * i)) & 0xFFu);
					h *= 1099511628211ull;
				}
			};
			melanger((uint32)v.Size());
			for (uint32 i = 0u; i < (uint32)v.Size(); ++i) {
				uint32 b[3];
				std::memcpy(&b[0], &v[i].pos.x, 4u);
				std::memcpy(&b[1], &v[i].pos.y, 4u);
				std::memcpy(&b[2], &v[i].pos.z, 4u);
				melanger(b[0]);
				melanger(b[1]);
				melanger(b[2]);
			}
			melanger((uint32)debut.Size());
			for (uint32 i = 0u; i < (uint32)debut.Size(); ++i) {
				melanger(debut[i]);
			}
			for (uint32 i = 0u; i < (uint32)boucles.Size(); ++i) {
				melanger(boucles[i]);
			}
			return h;
		}

		const char *NkR32StatutNom(NkR32Statut s) {
			switch (s) {
				case NkR32Statut::Nk_R32Statut_Appliquee:  return "appliquee";
				case NkR32Statut::Nk_R32Statut_Reportee:   return "reportee";
				case NkR32Statut::Nk_R32Statut_Orpheline:  return "orpheline";
				case NkR32Statut::Nk_R32Statut_AConfirmer: return "a-confirmer";
			}
			return "?";
		}

		const char *NkR32CauseNom(NkR32Cause c) {
			switch (c) {
				case NkR32Cause::Nk_R32Cause_Aucune:                      return "aucune";
				case NkR32Cause::Nk_R32Cause_OsDisparu:                   return "os-disparu";
				case NkR32Cause::Nk_R32Cause_RenommageAConfirmer:         return "renommage-a-confirmer";
				case NkR32Cause::Nk_R32Cause_RenommageRefuse:             return "renommage-refuse";
				case NkR32Cause::Nk_R32Cause_ReferenceChangeeHorsVersion: return "reference-changee-hors-version";
				case NkR32Cause::Nk_R32Cause_CibleVide:                   return "cible-vide";
				case NkR32Cause::Nk_R32Cause_DesignationInconnue:         return "designation-inconnue";
				case NkR32Cause::Nk_R32Cause_DesignationPasEnG0:          return "designation-pas-en-g0";
				case NkR32Cause::Nk_R32Cause_GroupeInconnu:               return "groupe-inconnu";
				case NkR32Cause::Nk_R32Cause_OperationEchouee:            return "operation-echouee";
				case NkR32Cause::Nk_R32Cause_RejeuInterrompu:             return "rejeu-interrompu";
			}
			return "?";
		}

	}  // namespace renderer
}  // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Creation Date: 2026-09-29
// ============================================================
