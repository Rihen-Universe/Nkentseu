// =============================================================================
// NkUnkenySauvegarde.cpp
//
// ⚠️ LE CHARGEMENT PASSE PAR LA PHOTO
//   Relire un fichier, c'est fabriquer une NkScene::NkPhoto puis appeler
//   Restaurer — le chemin qu'emprunte deja Jouer / Arreter, et que les bancs
//   eprouvent. Ecrire un second chemin de reconstruction de scene ferait deux
//   facons de refaire un corps rigide, et elles finiraient par diverger.
//
// ⚠️ LA VERSION 1 SE RELIT PAR LE MEME CODE QU'AVANT (2026-09-29)
//   Ce qui a change pour la version 2 s'AJOUTE a cote : les cles "uid",
//   "parent", "local" et "prochainUid" sont lues si elles existent ; un
//   composant "jeu" est lu champ par champ si c'est un OBJET, en octets si
//   c'est une CHAINE — exactement le chemin v1. Le banc (v1) relit un fichier
//   ecrit par le code d'avant, fige dans le banc.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Unkeny/Scene/NkUnkenySauvegarde.h"

#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/NkArchive.h"
#include "Unkeny/Rendu/NkUnkenyTextures.h"
#include "Unkeny/Scene/NkUnkenyPrefab.h"
#include "Unkeny/Scene/NkUnkenySauvegardeInterne.h"
#include "Unkeny/Son/NkUnkenySon.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace unkeny {

		namespace {
			using physics::NkParticules2D;

			NkStringView V(const char *s) {
				return NkStringView(s);
			}

			// ---- Nombres en chaine : "%.9g", la precision exacte d'un float32 ----
			class NkNombres {
				public:
					NkNombres &F(float32 v) {
						char b[32];
						std::snprintf(b, sizeof(b), mTexte.Empty() ? "%.9g" : " %.9g", static_cast<double>(v));
						mTexte.Append(b);
						return *this;
					}
					NkNombres &U(uint32 v) {
						char b[16];
						std::snprintf(b, sizeof(b), mTexte.Empty() ? "%u" : " %u", static_cast<unsigned>(v));
						mTexte.Append(b);
						return *this;
					}
					NkNombres &V2(const NkVec2f &v) {
						return F(v.x).F(v.y);
					}
					const NkString &Texte() const {
						return mTexte;
					}

				private:
					NkString mTexte;
			};

			class NkLecteur {
				public:
					explicit NkLecteur(const NkString &s) : mP(s.CStr()) {
					}
					bool F(float32 &v) {
						char *fin = nullptr;
						const float v2 = std::strtof(mP, &fin);
						if (fin == mP) {
							mOk = false;
							return false;
						}
						v = v2;
						mP = fin;
						return true;
					}
					bool U(uint32 &v) {
						char *fin = nullptr;
						const unsigned long v2 = std::strtoul(mP, &fin, 10);
						if (fin == mP) {
							mOk = false;
							return false;
						}
						v = static_cast<uint32>(v2);
						mP = fin;
						return true;
					}
					bool V2(NkVec2f &v) {
						return F(v.x) && F(v.y);
					}
					bool Ok() const {
						return mOk;
					}

				private:
					const char *mP;
					bool mOk = true;
			};

			NkString Hex(const uint8 *d, uint32 n) {
				static const char k[] = "0123456789abcdef";
				NkString s;
				char paire[3] = {0, 0, 0};
				for (uint32 i = 0; i < n; ++i) {
					paire[0] = k[d[i] >> 4];
					paire[1] = k[d[i] & 0xF];
					s.Append(paire);
				}
				return s;
			}
			bool DeHex(const NkString &s, NkVector<uint8> &out) {
				const char *p = s.CStr();
				const usize n = static_cast<usize>(s.Length());
				if (n % 2u != 0u) {
					return false;
				}
				out.Resize(n / 2u);
				auto val = [](char c) -> int32 {
					return (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
				};
				for (usize i = 0; i < n / 2u; ++i) {
					const int32 h = val(p[2 * i]), l = val(p[2 * i + 1]);
					if (h < 0 || l < 0) {
						return false;
					}
					out[i] = static_cast<uint8>(h * 16 + l);
				}
				return true;
			}

			// Lectures tolerantes : un champ absent garde la valeur par defaut.
			// C'est ce qui permet a une version 1 de relire un fichier ecrit
			// avant l'ajout d'un champ.
			void LireF(const NkArchive &a, const char *k, float32 &v) {
				nk_float32 x;
				if (a.GetFloat32(V(k), x)) {
					v = x;
				}
			}
			void LireU(const NkArchive &a, const char *k, uint32 &v) {
				nk_uint32 x;
				if (a.GetUInt32(V(k), x)) {
					v = x;
				}
			}
			void LireI(const NkArchive &a, const char *k, int32 &v) {
				nk_int32 x;
				if (a.GetInt32(V(k), x)) {
					v = x;
				}
			}
			void LireB(const NkArchive &a, const char *k, bool &v) {
				nk_bool x;
				if (a.GetBool(V(k), x)) {
					v = x;
				}
			}
			void LireV2(const NkArchive &a, const char *k, NkVec2f &v) {
				NkString s;
				if (a.GetString(V(k), s)) {
					NkVec2f t;
					NkLecteur l(s);
					if (l.V2(t)) {
						v = t;
					}
				}
			}

			// =================================================================
			// Eclairage et effets (2026-09-30) : ECRITS SEULEMENT S'ILS SONT LA.
			// Une scene sans lumiere ni emetteur ressort a l'octet pres comme
			// avant eux : aucune cle de plus, pas meme vide.
			// =================================================================
			NkArchive EcrireLumiere(const NkLumiere2D &l) {
				NkArchive o;
				o.SetUInt32(V("type"), static_cast<uint32>(l.type));
				o.SetUInt32(V("couleur"), l.couleur);
				o.SetFloat32(V("intensite"), l.intensite);
				o.SetFloat32(V("portee"), l.portee);
				o.SetFloat32(V("attenuation"), l.attenuation);
				o.SetFloat32(V("direction"), l.direction);
				o.SetFloat32(V("ouverture"), l.ouverture);
				o.SetFloat32(V("douceur"), l.douceur);
				o.SetString(V("decalage"), NkNombres().V2(l.decalage).Texte().View());
				o.SetFloat32(V("halo"), l.halo);
				o.SetBool(V("ombres"), l.ombres);
				o.SetBool(V("actif"), l.actif);
				return o;
			}

			bool LireLumiere(const NkArchive &o, NkLumiere2D &l, NkString &erreur) {
				uint32 type = 0;
				LireU(o, "type", type);
				if (type > static_cast<uint32>(NkTypeLumiere2D::NK_DIRECTIONNELLE)) {
					erreur = "type de lumiere inconnu";
					return false;
				}
				l.type = static_cast<NkTypeLumiere2D>(type);
				LireU(o, "couleur", l.couleur);
				LireF(o, "intensite", l.intensite);
				LireF(o, "portee", l.portee);
				LireF(o, "attenuation", l.attenuation);
				LireF(o, "direction", l.direction);
				LireF(o, "ouverture", l.ouverture);
				LireF(o, "douceur", l.douceur);
				LireV2(o, "decalage", l.decalage);
				LireF(o, "halo", l.halo);
				LireB(o, "ombres", l.ombres);
				LireB(o, "actif", l.actif);
				return true;
			}

			NkArchive EcrireEmetteur(const NkEmetteur2D &e) {
				NkArchive o;
				o.SetUInt32(V("preset"), static_cast<uint32>(e.preset));
				o.SetBool(V("actif"), e.actif);
				o.SetBool(V("boucle"), e.boucle);
				o.SetFloat32(V("duree"), e.duree);
				o.SetUInt32(V("rafale"), e.rafale);
				o.SetFloat32(V("debit"), e.debit);
				o.SetString(V("vie"), NkNombres().F(e.vieMin).F(e.vieMax).Texte().View());
				o.SetString(V("vitesse"), NkNombres().F(e.vitesseMin).F(e.vitesseMax).Texte().View());
				o.SetFloat32(V("direction"), e.direction);
				o.SetFloat32(V("dispersion"), e.dispersion);
				o.SetString(V("gravite"), NkNombres().V2(e.gravite).Texte().View());
				o.SetFloat32(V("frein"), e.frein);
				o.SetString(V("taille"), NkNombres().F(e.tailleDebut).F(e.tailleFin).Texte().View());
				o.SetString(V("couleurs"), NkNombres().U(e.couleurDebut).U(e.couleurMilieu).U(e.couleurFin).Texte().View());
				o.SetBool(V("additif"), e.additif);
				o.SetUInt32(V("forme"), static_cast<uint32>(e.forme));
				o.SetUInt32(V("zone"), static_cast<uint32>(e.zone));
				o.SetFloat32(V("rayonZone"), e.rayonZone);
				o.SetFloat32(V("largeurZone"), e.largeurZone);
				o.SetString(V("decalage"), NkNombres().V2(e.decalage).Texte().View());
				o.SetUInt32(V("graine"), e.graine);
				o.SetUInt32(V("max"), e.maxParticules);
				o.SetBool(V("eclaire"), e.eclaire);
				o.SetUInt32(V("couleurLumiere"), e.couleurLumiere);
				o.SetFloat32(V("intensiteLumiere"), e.intensiteLumiere);
				o.SetFloat32(V("porteeLumiere"), e.porteeLumiere);
				o.SetFloat32(V("scintillement"), e.scintillement);
				o.SetBool(V("ombresLumiere"), e.ombresLumiere);
				return o;
			}

			bool LireEmetteur(const NkArchive &o, NkEmetteur2D &e, NkString &erreur) {
				uint32 preset = 0;
				uint32 forme = 0;
				uint32 zone = 0;
				LireU(o, "preset", preset);
				LireU(o, "forme", forme);
				LireU(o, "zone", zone);
				if (preset >= static_cast<uint32>(NkPresetEffet2D::NK_COUNT) ||
					forme > static_cast<uint32>(NkFormeParticule2D::NK_PLEINE) ||
					zone > static_cast<uint32>(NkZoneEmission2D::NK_LIGNE)) {
					erreur = "emetteur de particules illisible (preset, forme ou zone inconnus)";
					return false;
				}
				e.preset = static_cast<NkPresetEffet2D>(preset);
				e.forme = static_cast<NkFormeParticule2D>(forme);
				e.zone = static_cast<NkZoneEmission2D>(zone);
				LireB(o, "actif", e.actif);
				LireB(o, "boucle", e.boucle);
				LireF(o, "duree", e.duree);
				LireU(o, "rafale", e.rafale);
				LireF(o, "debit", e.debit);
				NkString s;
				if (o.GetString(V("vie"), s)) {
					NkLecteur l(s);
					l.F(e.vieMin);
					l.F(e.vieMax);
				}
				if (o.GetString(V("vitesse"), s)) {
					NkLecteur l(s);
					l.F(e.vitesseMin);
					l.F(e.vitesseMax);
				}
				LireF(o, "direction", e.direction);
				LireF(o, "dispersion", e.dispersion);
				LireV2(o, "gravite", e.gravite);
				LireF(o, "frein", e.frein);
				if (o.GetString(V("taille"), s)) {
					NkLecteur l(s);
					l.F(e.tailleDebut);
					l.F(e.tailleFin);
				}
				if (o.GetString(V("couleurs"), s)) {
					NkLecteur l(s);
					l.U(e.couleurDebut);
					l.U(e.couleurMilieu);
					l.U(e.couleurFin);
				}
				LireB(o, "additif", e.additif);
				LireF(o, "rayonZone", e.rayonZone);
				LireF(o, "largeurZone", e.largeurZone);
				LireV2(o, "decalage", e.decalage);
				LireU(o, "graine", e.graine);
				LireU(o, "max", e.maxParticules);
				LireB(o, "eclaire", e.eclaire);
				LireU(o, "couleurLumiere", e.couleurLumiere);
				LireF(o, "intensiteLumiere", e.intensiteLumiere);
				LireF(o, "porteeLumiere", e.porteeLumiere);
				LireF(o, "scintillement", e.scintillement);
				LireB(o, "ombresLumiere", e.ombresLumiere);
				return true;
			}

			// =================================================================
			// Les noms des ressources (2026-09-29)
			// =================================================================
			/// Les registres, en LECTURE : ecrire ne charge rien.
			struct NkNoms {
					const NkTextures2D *textures = nullptr;
					const NkSons2D *sons = nullptr;
					const NkPrefabs2D *prefabs = nullptr;
			};
			NkNoms Noms(const NkRessourcesScene &r) {
				NkNoms n;
				n.textures = r.textures;
				n.sons = r.sons;
				n.prefabs = r.prefabs;
				return n;
			}

			const char *NomRessource(const NkNoms &r, NkTypeChamp t, uint32 id) {
				if (id == 0u) {
					return "";
				}
				switch (t) {
					case NkTypeChamp::NK_SON:
						return r.sons != nullptr ? r.sons->Nom(id) : "";
					case NkTypeChamp::NK_TEXTURE:
						return r.textures != nullptr ? r.textures->Nom(id) : "";
					case NkTypeChamp::NK_PREFAB:
						return r.prefabs != nullptr ? r.prefabs->Nom(id) : "";
					default:
						return "";
				}
			}

			const char *NomGenre(NkTypeChamp t) {
				switch (t) {
					case NkTypeChamp::NK_SON:
						return "son";
					case NkTypeChamp::NK_TEXTURE:
						return "texture";
					default:
						return "prefab";
				}
			}

			/// Le nom -> l'identifiant de CETTE session. Un son ou une texture absents
			/// sont charges depuis le disque (leur nom est leur chemin) ; un prefab
			/// absent est lu depuis son .nkprefab.
			uint32 IdRessource(NkScene &scene, const NkRessourcesScene &r, NkTypeChamp t, const NkString &nom) {
				if (nom.Empty()) {
					return 0u;
				}
				uint32 id = 0u;
				bool registre = false;
				switch (t) {
					case NkTypeChamp::NK_SON:
						registre = r.sons != nullptr;
						id = registre ? r.sons->Charger(nom.CStr()) : 0u;
						break;
					case NkTypeChamp::NK_TEXTURE:
						registre = r.textures != nullptr;
						id = registre ? r.textures->Charger(nom.CStr()) : 0u;
						break;
					case NkTypeChamp::NK_PREFAB:
						registre = r.prefabs != nullptr;
						if (registre) {
							id = r.prefabs->Trouver(nom.CStr());
							if (id == 0u) {
								id = r.prefabs->Charger(scene, nom.CStr(), r);
							}
						}
						break;
					default:
						break;
				}
				if (id == 0u) {
					// On le DIT : une source muette apres reouverture fait chercher
					// le defaut dans NKAudio.
					logger.Warn("[unkeny] {0} « {1} » {2} a la relecture : le champ revient vide", NomGenre(t),
								nom.CStr(), registre ? "introuvable" : "sans registre (NkRessourcesScene)");
				}
				return id;
			}

			bool EstReference(NkTypeChamp t) {
				return t == NkTypeChamp::NK_SON || t == NkTypeChamp::NK_TEXTURE || t == NkTypeChamp::NK_PREFAB;
			}

			template <typename T> T Lu(const uint8 *p) {
				T v;
				std::memcpy(&v, p, sizeof(T));
				return v;
			}
			template <typename T> void Ecrit(uint8 *p, T v) {
				std::memcpy(p, &v, sizeof(T));
			}

			/// Un element d'un tableau, en texte (sans les references ni le texte).
			void AjouterElement(NkString &s, NkTypeChamp t, const uint8 *p) {
				char b[80];
				switch (t) {
					case NkTypeChamp::NK_BOOL:
						std::snprintf(b, sizeof(b), "%u", Lu<bool>(p) ? 1u : 0u);
						break;
					case NkTypeChamp::NK_I8:
						std::snprintf(b, sizeof(b), "%d", static_cast<int>(Lu<int8>(p)));
						break;
					case NkTypeChamp::NK_U8:
						std::snprintf(b, sizeof(b), "%u", static_cast<unsigned>(Lu<uint8>(p)));
						break;
					case NkTypeChamp::NK_I16:
						std::snprintf(b, sizeof(b), "%d", static_cast<int>(Lu<int16>(p)));
						break;
					case NkTypeChamp::NK_U16:
						std::snprintf(b, sizeof(b), "%u", static_cast<unsigned>(Lu<uint16>(p)));
						break;
					case NkTypeChamp::NK_I32:
						std::snprintf(b, sizeof(b), "%ld", static_cast<long>(Lu<int32>(p)));
						break;
					case NkTypeChamp::NK_U32:
						std::snprintf(b, sizeof(b), "%lu", static_cast<unsigned long>(Lu<uint32>(p)));
						break;
					case NkTypeChamp::NK_I64:
						std::snprintf(b, sizeof(b), "%lld", static_cast<long long>(Lu<int64>(p)));
						break;
					case NkTypeChamp::NK_U64:
					case NkTypeChamp::NK_ENTITE:
						std::snprintf(b, sizeof(b), "%llu", static_cast<unsigned long long>(Lu<uint64>(p)));
						break;
					case NkTypeChamp::NK_F32:
						std::snprintf(b, sizeof(b), "%.9g", static_cast<double>(Lu<float32>(p)));
						break;
					case NkTypeChamp::NK_F64:
						std::snprintf(b, sizeof(b), "%.17g", Lu<float64>(p));
						break;
					case NkTypeChamp::NK_VEC2: {
						const NkVec2f v = Lu<NkVec2f>(p);
						std::snprintf(b, sizeof(b), "%.9g %.9g", static_cast<double>(v.x), static_cast<double>(v.y));
						break;
					}
					default:
						b[0] = '\0';
						break;
				}
				if (!s.Empty()) {
					s.Append(" ");
				}
				s.Append(b);
			}

			/// Le composant decrit `d` -> objet, champ par champ. Les champs
			/// transitoires n'y sont pas. Les photos portent deja l'IDENTITE des
			/// entites referencees (NkScene::Photographier).
			void EcrireChamps(NkArchive &o, const NkChampSauve *champs, uint32 n, const uint8 *d, const NkNoms &r) {
				for (uint32 c = 0; c < n; ++c) {
					const NkChampSauve &ch = champs[c];
					if (ch.transitoire) {
						continue;
					}
					const uint8 *p = d + ch.decalage;
					const NkStringView k(ch.nom);
					if (ch.nombre > 1u) {
						NkString s;
						for (uint32 i = 0; i < ch.nombre; ++i) {
							const uint8 *q = d + ch.decalage + i * ch.Pas();
							if (EstReference(ch.type)) {
								// Une ligne par element : un nom de fichier peut porter des
								// espaces, pas de saut de ligne. « #n » : sans nom connu.
								const uint32 id = Lu<uint32>(q);
								const char *nom = NomRessource(r, ch.type, id);
								if (i > 0u) {
									s.Append("\n");
								}
								if (id != 0u && nom[0] != '\0') {
									s.Append(nom);
								} else {
									char b[16];
									std::snprintf(b, sizeof(b), "#%lu", static_cast<unsigned long>(id));
									s.Append(b);
								}
							} else {
								AjouterElement(s, ch.type, q);
							}
						}
						o.SetString(k, s.View());
						continue;
					}
					switch (ch.type) {
						case NkTypeChamp::NK_BOOL:
							o.SetBool(k, Lu<bool>(p));
							break;
						case NkTypeChamp::NK_I8:
							o.SetInt32(k, Lu<int8>(p));
							break;
						case NkTypeChamp::NK_U8:
							o.SetUInt32(k, Lu<uint8>(p));
							break;
						case NkTypeChamp::NK_I16:
							o.SetInt32(k, Lu<int16>(p));
							break;
						case NkTypeChamp::NK_U16:
							o.SetUInt32(k, Lu<uint16>(p));
							break;
						case NkTypeChamp::NK_I32:
							o.SetInt32(k, Lu<int32>(p));
							break;
						case NkTypeChamp::NK_U32:
							o.SetUInt32(k, Lu<uint32>(p));
							break;
						case NkTypeChamp::NK_I64:
							o.SetInt64(k, Lu<int64>(p));
							break;
						case NkTypeChamp::NK_U64:
						case NkTypeChamp::NK_ENTITE:
							o.SetUInt64(k, Lu<uint64>(p));
							break;
						case NkTypeChamp::NK_F32:
							o.SetFloat32(k, Lu<float32>(p));
							break;
						case NkTypeChamp::NK_F64:
							o.SetFloat64(k, Lu<float64>(p));
							break;
						case NkTypeChamp::NK_VEC2:
							o.SetString(k, NkNombres().V2(Lu<NkVec2f>(p)).Texte().View());
							break;
						case NkTypeChamp::NK_TEXTE: {
							// Borne a la taille du champ : un texte sans zero final ne
							// fait pas lire le composant suivant.
							usize l = 0;
							while (l < ch.taille && p[l] != 0u) {
								++l;
							}
							o.SetString(k, NkStringView(reinterpret_cast<const char *>(p), l));
							break;
						}
						case NkTypeChamp::NK_SON:
						case NkTypeChamp::NK_TEXTURE:
						case NkTypeChamp::NK_PREFAB: {
							const uint32 id = Lu<uint32>(p);
							const char *nom = NomRessource(r, ch.type, id);
							if (id != 0u && nom[0] != '\0') {
								o.SetString(k, NkStringView(nom));
							} else {
								o.SetUInt32(k, id); // pas de registre : le numero, comme avant
							}
							break;
						}
					}
				}
			}

			/// L'inverse, dans `d` qui porte DEJA la valeur par defaut : un champ
			/// absent la garde (c'est l'ajout d'un champ qui ne casse rien), un champ
			/// inconnu du composant n'est pas lu (c'est le retrait qui ne casse rien).
			void LireChamps(const NkArchive &o, const NkChampSauve *champs, uint32 n, uint8 *d, NkScene &scene,
							const NkRessourcesScene &r) {
				for (uint32 c = 0; c < n; ++c) {
					const NkChampSauve &ch = champs[c];
					const NkStringView k(ch.nom);
					if (ch.transitoire || !o.Has(k)) {
						continue;
					}
					uint8 *p = d + ch.decalage;
					if (ch.nombre > 1u) {
						NkString s;
						if (!o.GetString(k, s)) {
							continue;
						}
						if (EstReference(ch.type)) {
							const char *a = s.CStr();
							for (uint32 i = 0; i < ch.nombre && a != nullptr; ++i) {
								const char *fin = std::strchr(a, '\n');
								const NkString morceau = fin != nullptr ? NkString(a, static_cast<usize>(fin - a)) : NkString(a);
								uint32 id = 0u;
								if (!morceau.Empty() && morceau.CStr()[0] == '#') {
									id = static_cast<uint32>(std::strtoul(morceau.CStr() + 1, nullptr, 10));
								} else {
									id = IdRessource(scene, r, ch.type, morceau);
								}
								Ecrit<uint32>(d + ch.decalage + i * ch.Pas(), id);
								a = fin != nullptr ? fin + 1 : nullptr;
							}
							continue;
						}
						const char *a = s.CStr();
						for (uint32 i = 0; i < ch.nombre; ++i) {
							uint8 *q = d + ch.decalage + i * ch.Pas();
							char *fin = nullptr;
							switch (ch.type) {
								case NkTypeChamp::NK_F32:
									Ecrit<float32>(q, std::strtof(a, &fin));
									break;
								case NkTypeChamp::NK_F64:
									Ecrit<float64>(q, std::strtod(a, &fin));
									break;
								case NkTypeChamp::NK_VEC2: {
									NkVec2f v;
									v.x = std::strtof(a, &fin);
									v.y = std::strtof(fin, &fin);
									Ecrit<NkVec2f>(q, v);
									break;
								}
								case NkTypeChamp::NK_I8:
								case NkTypeChamp::NK_I16:
								case NkTypeChamp::NK_I32:
								case NkTypeChamp::NK_I64: {
									const long long v = std::strtoll(a, &fin, 10);
									if (ch.type == NkTypeChamp::NK_I8) {
										Ecrit<int8>(q, static_cast<int8>(v));
									} else if (ch.type == NkTypeChamp::NK_I16) {
										Ecrit<int16>(q, static_cast<int16>(v));
									} else if (ch.type == NkTypeChamp::NK_I32) {
										Ecrit<int32>(q, static_cast<int32>(v));
									} else {
										Ecrit<int64>(q, static_cast<int64>(v));
									}
									break;
								}
								default: {
									const unsigned long long v = std::strtoull(a, &fin, 10);
									if (ch.type == NkTypeChamp::NK_BOOL) {
										Ecrit<bool>(q, v != 0u);
									} else if (ch.type == NkTypeChamp::NK_U8) {
										Ecrit<uint8>(q, static_cast<uint8>(v));
									} else if (ch.type == NkTypeChamp::NK_U16) {
										Ecrit<uint16>(q, static_cast<uint16>(v));
									} else if (ch.type == NkTypeChamp::NK_U32) {
										Ecrit<uint32>(q, static_cast<uint32>(v));
									} else {
										Ecrit<uint64>(q, static_cast<uint64>(v));
									}
									break;
								}
							}
							if (fin == a || fin == nullptr) {
								break; // tableau plus court dans le fichier : le reste garde son defaut
							}
							a = fin;
						}
						continue;
					}
					switch (ch.type) {
						case NkTypeChamp::NK_BOOL: {
							nk_bool v = false;
							if (o.GetBool(k, v)) {
								Ecrit<bool>(p, v);
							}
							break;
						}
						case NkTypeChamp::NK_I8:
						case NkTypeChamp::NK_I16:
						case NkTypeChamp::NK_I32:
						case NkTypeChamp::NK_I64: {
							nk_int64 v = 0;
							if (!o.GetInt64(k, v)) {
								break;
							}
							if (ch.type == NkTypeChamp::NK_I8) {
								Ecrit<int8>(p, static_cast<int8>(v));
							} else if (ch.type == NkTypeChamp::NK_I16) {
								Ecrit<int16>(p, static_cast<int16>(v));
							} else if (ch.type == NkTypeChamp::NK_I32) {
								Ecrit<int32>(p, static_cast<int32>(v));
							} else {
								Ecrit<int64>(p, static_cast<int64>(v));
							}
							break;
						}
						case NkTypeChamp::NK_U8:
						case NkTypeChamp::NK_U16:
						case NkTypeChamp::NK_U32:
						case NkTypeChamp::NK_U64:
						case NkTypeChamp::NK_ENTITE: {
							nk_uint64 v = 0;
							if (!o.GetUInt64(k, v)) {
								break;
							}
							if (ch.type == NkTypeChamp::NK_U8) {
								Ecrit<uint8>(p, static_cast<uint8>(v));
							} else if (ch.type == NkTypeChamp::NK_U16) {
								Ecrit<uint16>(p, static_cast<uint16>(v));
							} else if (ch.type == NkTypeChamp::NK_U32) {
								Ecrit<uint32>(p, static_cast<uint32>(v));
							} else {
								// NK_ENTITE : l'IDENTITE, que Restaurer rebranche.
								Ecrit<uint64>(p, static_cast<uint64>(v));
							}
							break;
						}
						case NkTypeChamp::NK_F32: {
							nk_float32 v = 0.f;
							if (o.GetFloat32(k, v)) {
								Ecrit<float32>(p, v);
							}
							break;
						}
						case NkTypeChamp::NK_F64: {
							nk_float64 v = 0.0;
							if (o.GetFloat64(k, v)) {
								Ecrit<float64>(p, v);
							}
							break;
						}
						case NkTypeChamp::NK_VEC2: {
							NkVec2f v = Lu<NkVec2f>(p);
							LireV2(o, ch.nom, v);
							Ecrit<NkVec2f>(p, v);
							break;
						}
						case NkTypeChamp::NK_TEXTE: {
							NkString s;
							if (o.GetString(k, s)) {
								std::memset(p, 0, ch.taille);
								const usize l = static_cast<usize>(s.Length()) < ch.taille ? static_cast<usize>(s.Length()) : ch.taille - 1u;
								std::memcpy(p, s.CStr(), l);
							}
							break;
						}
						case NkTypeChamp::NK_SON:
						case NkTypeChamp::NK_TEXTURE:
						case NkTypeChamp::NK_PREFAB: {
							// Un NOM (v2 avec registre) se resout dans CETTE session ; un
							// NOMBRE (ecrit sans registre) reste un numero, comme avant.
							NkArchiveValue v;
							if (!o.GetValue(k, v)) {
								break;
							}
							if (v.type == NkArchiveValueType::NK_VALUE_STRING) {
								Ecrit<uint32>(p, IdRessource(scene, r, ch.type, v.text));
							} else {
								nk_uint32 id = 0;
								if (o.GetUInt32(k, id)) {
									Ecrit<uint32>(p, id);
								}
							}
							break;
						}
					}
				}
			}

			/// Des OCTETS (v1) d'un composant aujourd'hui decrit : ses champs NK_ENTITE
			/// y portent la poignee d'une autre session, qui ne designe rien. Ils
			/// reviennent vides plutot que de se faire prendre pour une identite.
			void ViderEntites(const NkChampSauve *champs, uint32 n, uint8 *d) {
				for (uint32 c = 0; c < n; ++c) {
					if (champs[c].type != NkTypeChamp::NK_ENTITE) {
						continue;
					}
					for (uint32 i = 0; i < champs[c].nombre; ++i) {
						Ecrit<uint64>(d + champs[c].decalage + i * champs[c].Pas(), 0u);
					}
				}
			}

			// =================================================================
			// Controleurs de personnage (2026-09-29) : champ par champ, par NOM.
			// Un fichier ancien n'a pas ces objets : l'entite n'a pas de
			// controleur, rien d'autre ne change. Un champ absent garde sa valeur
			// par defaut (Lire* tolerants, plus haut).
			// =================================================================
			NkArchive EcrireReglagesControle(const NkReglagesControle2D &r) {
				NkArchive o;
				o.SetInt32(V("actionX"), r.actionX);
				o.SetInt32(V("actionSauter"), r.actionSauter);
				o.SetFloat32(V("vitesseMax"), r.vitesseMax);
				o.SetFloat32(V("acceleration"), r.acceleration);
				o.SetFloat32(V("freinage"), r.freinage);
				o.SetFloat32(V("controleAir"), r.controleAir);
				o.SetFloat32(V("vitesseSaut"), r.vitesseSaut);
				o.SetFloat32(V("coyote"), r.coyote);
				o.SetFloat32(V("tamponSaut"), r.tamponSaut);
				o.SetFloat32(V("penteMax"), r.penteMax);
				o.SetBool(V("actif"), r.actif);
				return o;
			}

			void LireReglagesControle(const NkArchive &o, NkReglagesControle2D &r) {
				LireI(o, "actionX", r.actionX);
				LireI(o, "actionSauter", r.actionSauter);
				LireF(o, "vitesseMax", r.vitesseMax);
				LireF(o, "acceleration", r.acceleration);
				LireF(o, "freinage", r.freinage);
				LireF(o, "controleAir", r.controleAir);
				LireF(o, "vitesseSaut", r.vitesseSaut);
				LireF(o, "coyote", r.coyote);
				LireF(o, "tamponSaut", r.tamponSaut);
				LireF(o, "penteMax", r.penteMax);
				LireB(o, "actif", r.actif);
			}

			void LireNomCourt(const NkArchive &o, const char *cle, char *dst, uint32 taille) {
				NkString s;
				if (o.GetString(V(cle), s)) {
					std::snprintf(dst, taille, "%s", s.CStr());
				}
			}

			// =================================================================
			// Entites
			// =================================================================
			struct NkEntiteLue {
					NkScene::NkPhotoEntite e;
					NkString texture;
					bool aJeu = false; ///< l'objet "jeu" est relu APRES Init (voir NkChargerScene)
			};

			NkArchive EcrireEntite(const NkScene::NkPhotoEntite &e, NkScene &scene, const NkNoms &r) {
				NkArchive a;
				// L'identite et la hierarchie (v2). Absentes d'un fichier v1.
				if (e.uid != 0u) {
					a.SetUInt64(V("uid"), e.uid);
				}
				if (e.parentUid != 0u) {
					a.SetUInt64(V("parent"), e.parentUid);
					a.SetString(V("local"),
								NkNombres().V2(e.local.position).F(e.local.rotation).V2(e.local.echelle).Texte().View());
				}
				if (e.aEtiquette) {
					a.SetString(V("nom"), V(e.etiquette.nom));
				}
				a.SetString(V("transform"),
							NkNombres().V2(e.transform.position).F(e.transform.rotation).V2(e.transform.echelle).Texte().View());
				if (e.aSprite) {
					const NkSprite2D &s = e.sprite;
					NkArchive o;
					o.SetString(V("taille"), NkNombres().V2(s.taille).Texte().View());
					o.SetString(V("pivot"), NkNombres().V2(s.pivot).Texte().View());
					o.SetUInt32(V("couleur"), s.couleur);
					o.SetString(V("uv"), NkNombres().V2(s.uv0).V2(s.uv1).Texte().View());
					o.SetInt32(V("couche"), s.couche);
					o.SetBool(V("visible"), s.visible);
					if (s.texId != 0u && r.textures != nullptr && r.textures->Nom(s.texId)[0] != '\0') {
						o.SetString(V("texture"), V(r.textures->Nom(s.texId)));
					}
					a.SetObject(V("sprite"), o);
				}
				if (e.aCollisionneur) {
					const NkCollisionneur2D &c = e.collisionneur;
					NkArchive o;
					o.SetUInt32(V("forme"), static_cast<uint32>(c.forme));
					o.SetString(V("demi"), NkNombres().V2(c.demiTaille).Texte().View());
					o.SetFloat32(V("rayon"), c.rayon);
					o.SetString(V("decalage"), NkNombres().V2(c.decalage).Texte().View());
					o.SetUInt32(V("couche"), c.couche);
					o.SetUInt32(V("masque"), c.masque);
					o.SetBool(V("declencheur"), c.declencheur);
					a.SetObject(V("collisionneur"), o);
				}
				if (e.aCorps) {
					const NkCorps2D &c = e.corps;
					NkArchive o;
					o.SetUInt32(V("type"), static_cast<uint32>(c.type));
					o.SetFloat32(V("masse"), c.masse);
					o.SetFloat32(V("amortLineaire"), c.amortissementLineaire);
					o.SetFloat32(V("amortAngulaire"), c.amortissementAngulaire);
					o.SetFloat32(V("echelleGravite"), c.echelleGravite);
					o.SetBool(V("rotationBloquee"), c.rotationBloquee);
					o.SetFloat32(V("friction"), c.friction);
					o.SetFloat32(V("rebond"), c.rebond);
					// L'id du corps AU MOMENT de l'ecriture (2026-09-29) : ce n'est pas
					// une identite (le corps renait sous un id neuf), c'est la cle qui
					// permet de rebrancher les attaches des particules sur lui.
					o.SetUInt32(V("id"), c.corpsId);
					const physics::NkRigidBody &b = e.etatRigide;
					o.SetString(V("etat"), NkNombres()
											   .F(b.position.x).F(b.position.y).F(b.position.z)
											   .F(b.orientation.x).F(b.orientation.y).F(b.orientation.z).F(b.orientation.w)
											   .F(b.linearVelocity.x).F(b.linearVelocity.y).F(b.linearVelocity.z)
											   .F(b.angularVelocity.x).F(b.angularVelocity.y).F(b.angularVelocity.z)
											   .Texte().View());
					a.SetObject(V("corps"), o);
				}
				if (e.aMou) {
					NkArchive o;
					o.SetUInt32(V("corpsId"), e.mou.corpsId);
					o.SetUInt32(V("couleur"), e.mou.couleur);
					o.SetBool(V("visible"), e.mou.visible);
					a.SetObject(V("mou"), o);
				}
				if (e.aLumiere) {
					a.SetObject(V("lumiere"), EcrireLumiere(e.lumiere));
				}
				if (e.aEmetteur) {
					a.SetObject(V("emetteur"), EcrireEmetteur(e.emetteur));
				}
				if (e.aControleRigide) {
					a.SetObject(V("controleRigide"), EcrireReglagesControle(e.controleRigide.reglages));
				}
				if (e.aControleMou) {
					NkArchive o = EcrireReglagesControle(e.controleMou.reglages);
					o.SetString(V("partiePoussee"), V(e.controleMou.partiePoussee));
					o.SetString(V("partieSol"), V(e.controleMou.partieSol));
					o.SetFloat32(V("redressement"), e.controleMou.redressement);
					a.SetObject(V("controleMou"), o);
				}
				// Les composants declares, sous LEUR nom. Sans nom : memoire seulement.
				// Decrits : champ par champ (v2). Sinon : les octets, comme en v1.
				NkArchive jeu;
				bool aJeu = false;
				uint32 decalage = 0;
				for (uint32 k = 0; k < scene.NbComposantsPhoto(); ++k) {
					const uint32 t = scene.TailleComposantPhoto(k);
					const char *nom = scene.NomComposantPhoto(k);
					if (nom != nullptr && (e.extraPresents & (1u << k)) != 0u && decalage + t <= e.extra.Size()) {
						uint32 nb = 0;
						const NkChampSauve *champs = scene.ChampsComposantPhoto(k, nb);
						if (champs != nullptr) {
							NkArchive o;
							EcrireChamps(o, champs, nb, e.extra.Data() + decalage, r);
							jeu.SetObject(V(nom), o);
						} else {
							jeu.SetString(V(nom), Hex(e.extra.Data() + decalage, t).View());
						}
						aJeu = true;
					}
					decalage += t;
				}
				if (aJeu) {
					a.SetObject(V("jeu"), jeu);
				}
				return a;
			}

			bool LireEntite(const NkArchive &a, NkEntiteLue &out, NkString &erreur) {
				NkScene::NkPhotoEntite &e = out.e;
				NkString s;
				// v2 : identite et hierarchie. Un fichier v1 n'en a pas : uid 0, et
				// Restaurer donne 1..N dans l'ordre du fichier.
				{
					nk_uint64 u = 0;
					if (a.GetUInt64(V("uid"), u)) {
						e.uid = u;
					}
					if (a.GetUInt64(V("parent"), u)) {
						e.parentUid = u;
					}
					if (e.parentUid != 0u && a.GetString(V("local"), s)) {
						NkLecteur l(s);
						l.V2(e.local.position);
						l.F(e.local.rotation);
						l.V2(e.local.echelle);
						if (!l.Ok()) {
							erreur = "transform local illisible";
							return false;
						}
					}
				}
				if (a.GetString(V("nom"), s)) {
					e.aEtiquette = true;
					std::snprintf(e.etiquette.nom, sizeof(e.etiquette.nom), "%s", s.CStr());
				}
				if (!a.GetString(V("transform"), s)) {
					erreur = "entite sans transform";
					return false;
				}
				{
					NkLecteur l(s);
					l.V2(e.transform.position);
					l.F(e.transform.rotation);
					l.V2(e.transform.echelle);
					if (!l.Ok()) {
						erreur = "transform illisible";
						return false;
					}
				}
				NkArchive o;
				if (a.GetObject(V("sprite"), o)) {
					e.aSprite = true;
					NkSprite2D &sp = e.sprite;
					LireV2(o, "taille", sp.taille);
					LireV2(o, "pivot", sp.pivot);
					LireU(o, "couleur", sp.couleur);
					if (o.GetString(V("uv"), s)) {
						NkLecteur l(s);
						l.V2(sp.uv0);
						l.V2(sp.uv1);
					}
					LireI(o, "couche", sp.couche);
					LireB(o, "visible", sp.visible);
					sp.texId = 0u; // resolu apres, par le nom
					(void)o.GetString(V("texture"), out.texture);
				}
				if (a.GetObject(V("collisionneur"), o)) {
					e.aCollisionneur = true;
					NkCollisionneur2D &c = e.collisionneur;
					uint32 forme = 0;
					LireU(o, "forme", forme);
					if (forme > static_cast<uint32>(NkForme2D::NK_CAPSULE)) {
						erreur = "forme de collisionneur inconnue";
						return false;
					}
					c.forme = static_cast<NkForme2D>(forme);
					LireV2(o, "demi", c.demiTaille);
					LireF(o, "rayon", c.rayon);
					LireV2(o, "decalage", c.decalage);
					LireU(o, "couche", c.couche);
					LireU(o, "masque", c.masque);
					LireB(o, "declencheur", c.declencheur);
				}
				if (a.GetObject(V("corps"), o)) {
					e.aCorps = true;
					NkCorps2D &c = e.corps;
					uint32 type = 0;
					LireU(o, "type", type);
					if (type > static_cast<uint32>(NkTypeCorps::NK_DYNAMIQUE)) {
						erreur = "type de corps inconnu";
						return false;
					}
					c.type = static_cast<NkTypeCorps>(type);
					LireF(o, "masse", c.masse);
					LireF(o, "amortLineaire", c.amortissementLineaire);
					LireF(o, "amortAngulaire", c.amortissementAngulaire);
					LireF(o, "echelleGravite", c.echelleGravite);
					LireB(o, "rotationBloquee", c.rotationBloquee);
					LireF(o, "friction", c.friction);
					LireF(o, "rebond", c.rebond);
					c.corpsId = 0;
					// L'ancien id (absent d'un fichier d'avant le 2026-09-29 : reste 0) :
					// Restaurer s'en sert pour rebrancher les attaches, jamais comme id.
					LireU(o, "id", c.corpsId);
					physics::NkRigidBody &b = e.etatRigide;
					// Par defaut, l'etat suit le transform : un fichier ecrit a la
					// main peut omettre "etat".
					b.position = math::NkVec3f(e.transform.position.x, e.transform.position.y, 0.f);
					b.orientation.x = 0.f;
					b.orientation.y = 0.f;
					b.orientation.z = math::NkSin(e.transform.rotation * 0.5f);
					b.orientation.w = math::NkCos(e.transform.rotation * 0.5f);
					if (o.GetString(V("etat"), s)) {
						NkLecteur l(s);
						l.F(b.position.x), l.F(b.position.y), l.F(b.position.z);
						l.F(b.orientation.x), l.F(b.orientation.y), l.F(b.orientation.z), l.F(b.orientation.w);
						l.F(b.linearVelocity.x), l.F(b.linearVelocity.y), l.F(b.linearVelocity.z);
						l.F(b.angularVelocity.x), l.F(b.angularVelocity.y), l.F(b.angularVelocity.z);
						if (!l.Ok()) {
							erreur = "etat de corps rigide illisible";
							return false;
						}
					}
				}
				if (a.GetObject(V("mou"), o)) {
					e.aMou = true;
					LireU(o, "corpsId", e.mou.corpsId);
					LireU(o, "couleur", e.mou.couleur);
					LireB(o, "visible", e.mou.visible);
				}
				if (a.GetObject(V("lumiere"), o)) {
					e.aLumiere = true;
					if (!LireLumiere(o, e.lumiere, erreur)) {
						return false;
					}
				}
				if (a.GetObject(V("emetteur"), o)) {
					e.aEmetteur = true;
					if (!LireEmetteur(o, e.emetteur, erreur)) {
						return false;
					}
				}
				if (a.GetObject(V("controleRigide"), o)) {
					e.aControleRigide = true;
					LireReglagesControle(o, e.controleRigide.reglages);
				}
				if (a.GetObject(V("controleMou"), o)) {
					e.aControleMou = true;
					LireReglagesControle(o, e.controleMou.reglages);
					LireNomCourt(o, "partiePoussee", e.controleMou.partiePoussee, sizeof(e.controleMou.partiePoussee));
					LireNomCourt(o, "partieSol", e.controleMou.partieSol, sizeof(e.controleMou.partieSol));
					LireF(o, "redressement", e.controleMou.redressement);
				}
				// "jeu" : relu apres Init, par les noms que la scene connait alors
				// (un nom inconnu est ignore — c'est un composant d'un autre jeu,
				// ou d'une version qui n'existe plus).
				out.aJeu = a.GetObject(V("jeu"), o);
				return true;
			}

			/// Les composants "jeu" de l'entite `brute`, poses dans la disposition des
			/// copieurs de `scene` (qui doivent etre declares : apres Init).
			void DecoderJeu(const NkArchive &brute, NkEntiteLue &lue, NkScene &scene, const NkRessourcesScene &r) {
				NkScene::NkPhotoEntite &e = lue.e;
				uint32 total = 0;
				for (uint32 k = 0; k < scene.NbComposantsPhoto(); ++k) {
					total += scene.TailleComposantPhoto(k);
				}
				e.extra.Resize(total);
				e.extraPresents = 0u;
				NkArchive jeu;
				if (!lue.aJeu || !brute.GetObject(V("jeu"), jeu)) {
					return;
				}
				uint32 decalage = 0;
				NkVector<uint8> octets;
				NkString s;
				for (uint32 k = 0; k < scene.NbComposantsPhoto(); ++k) {
					const uint32 t = scene.TailleComposantPhoto(k);
					const char *nom = scene.NomComposantPhoto(k);
					uint8 *dst = e.extra.Data() + decalage;
					decalage += t;
					if (nom == nullptr) {
						continue;
					}
					uint32 nb = 0;
					const NkChampSauve *champs = scene.ChampsComposantPhoto(k, nb);
					NkArchive o;
					if (champs != nullptr && jeu.GetObject(V(nom), o)) {
						// v2 : champ par champ, sur la valeur par defaut. Dans une photo,
						// un champ NK_ENTITE porte une IDENTITE : la poignee Invalid() du
						// defaut y deviendrait l'identite 0xFFFFFFFF ; elle devient 0.
						scene.DefautComposantPhoto(k, dst);
						ViderEntites(champs, nb, dst);
						LireChamps(o, champs, nb, dst, scene, r);
						e.extraPresents |= 1u << k;
					} else if (jeu.GetString(V(nom), s) && DeHex(s, octets) && octets.Size() == t) {
						// v1 (ou composant non decrit) : les octets, EXACTEMENT comme avant.
						std::memcpy(dst, octets.Data(), t);
						if (champs != nullptr) {
							ViderEntites(champs, nb, dst);
						}
						e.extraPresents |= 1u << k;
					}
				}
			}

			void ResoudreTexture(NkEntiteLue &lue, const NkRessourcesScene &r) {
				if (lue.e.aSprite && !lue.texture.Empty() && r.textures != nullptr) {
					lue.e.sprite.texId = r.textures->Charger(lue.texture.CStr());
				}
			}

			// =================================================================
			// Particules
			// =================================================================
			NkArchive EcrireParticules(const NkParticules2D &p) {
				NkArchive a;
				const physics::NkReglagesP2D &r = p.reglages;
				NkArchive g;
				g.SetString(V("gravite"), NkNombres().V2(r.gravite).Texte().View());
				g.SetInt32(V("sousPas"), r.sousPas);
				g.SetFloat32(V("echelleTemps"), r.echelleTemps);
				g.SetFloat32(V("temperature"), r.temperature);
				g.SetFloat32(V("vent"), r.vent);
				g.SetFloat32(V("amortAir"), r.amortAir);
				g.SetBool(V("collisions"), r.collisions);
				g.SetBool(V("soudure"), r.soudure);
				NkArchive lim;
				lim.SetBool(V("actif"), r.limites.actif);
				lim.SetString(V("boite"), NkNombres().F(r.limites.gauche).F(r.limites.droite).F(r.limites.bas).F(r.limites.haut).Texte().View());
				lim.SetBool(V("murs"), r.limites.murs);
				lim.SetBool(V("plafond"), r.limites.plafond);
				g.SetObject(V("limites"), lim);
				a.SetObject(V("reglages"), g);
				a.SetUInt32(V("prochainId"), p.prochainId);
				a.SetUInt32(V("rupturesTotal"), p.rupturesTotal);

				NkVector<NkArchive> corps;
				for (uint32 i = 0; i < p.corps.Size(); ++i) {
					const physics::NkCorpsP2D &c = p.corps[i];
					NkArchive o;
					o.SetUInt32(V("mat"), static_cast<uint32>(c.mat));
					o.SetUInt32(V("id"), c.id);
					o.SetString(V("plages"), NkNombres().U(c.debut).U(c.nombre).U(c.lienDebut).U(c.lienNombre).Texte().View());
					o.SetInt32(V("nx"), c.nx);
					o.SetInt32(V("ny"), c.ny);
					o.SetString(V("parametres"), NkNombres()
													 .F(c.espacement).F(c.aireRepos).F(c.raideur).F(c.pression).F(c.friction)
													 .F(c.rebond).F(c.viscosite).F(c.cohesion).F(c.plasticite).F(c.resistance)
													 .F(c.masseParticule).F(c.formeRaideur).F(c.amortissement)
													 .Texte().View());
					o.SetBool(V("autoCollision"), c.autoCollision);
					o.SetBool(V("couplageRigide"), c.couplageRigide);
					corps.PushBack(o);
				}
				a.SetObjectArray(V("corps"), corps);

				NkNombres pos, prec, vit, repos, masse, rayon, idx, vois, drap;
				for (uint32 i = 0; i < p.particules.Size(); ++i) {
					const physics::NkParticule2D &q = p.particules[i];
					pos.V2(q.pos);
					prec.V2(q.prec);
					vit.V2(q.vit);
					repos.V2(q.repos);
					masse.F(q.masse).F(q.invMasse);
					rayon.F(q.rayon);
					idx.U(q.corps);
					vois.U(q.nbVoisins);
					for (uint32 k = 0; k < q.nbVoisins; ++k) {
						vois.U(q.voisins[k]);
					}
					drap.U(q.epingle ? 1u : 0u);
				}
				NkArchive pa;
				pa.SetUInt32(V("n"), static_cast<uint32>(p.particules.Size()));
				pa.SetString(V("pos"), pos.Texte().View());
				pa.SetString(V("prec"), prec.Texte().View());
				pa.SetString(V("vit"), vit.Texte().View());
				pa.SetString(V("repos"), repos.Texte().View());
				pa.SetString(V("masse"), masse.Texte().View());
				pa.SetString(V("rayon"), rayon.Texte().View());
				pa.SetString(V("corps"), idx.Texte().View());
				pa.SetString(V("voisins"), vois.Texte().View());
				pa.SetString(V("epingle"), drap.Texte().View());
				a.SetObject(V("p"), pa);

				NkNombres la, lb, lc, lr, lg;
				for (uint32 i = 0; i < p.liens.Size(); ++i) {
					const physics::NkLien2D &l = p.liens[i];
					la.U(l.a).U(l.b);
					lc.U(l.corps);
					lr.F(l.repos).F(l.reposInitial);
					lg.U(static_cast<uint32>(l.genre)).U(l.casse ? 1u : 0u);
				}
				NkArchive li;
				li.SetUInt32(V("n"), static_cast<uint32>(p.liens.Size()));
				li.SetString(V("ab"), la.Texte().View());
				li.SetString(V("corps"), lc.Texte().View());
				li.SetString(V("repos"), lr.Texte().View());
				li.SetString(V("genre"), lg.Texte().View());
				a.SetObject(V("l"), li);

				// Parties nommees et attaches (2026-09-29) : ecrites seulement s'il y
				// en a -- un monde qui n'en a pas s'ecrit exactement comme avant.
				if (p.parties.Size() > 0u) {
					NkVector<NkArchive> parties;
					for (uint32 i = 0; i < p.parties.Size(); ++i) {
						const physics::NkPartieP2D &q = p.parties[i];
						NkArchive o;
						o.SetUInt32(V("id"), q.id);
						o.SetUInt32(V("corps"), q.corps);
						o.SetString(V("nom"), V(q.nom));
						NkNombres idx;
						for (uint32 k = q.debut; k < q.debut + q.nombre; ++k) {
							idx.U(p.partiesParticules[k]);
						}
						o.SetUInt32(V("n"), q.nombre);
						o.SetString(V("particules"), idx.Texte().View());
						parties.PushBack(o);
					}
					a.SetObjectArray(V("parties"), parties);
					a.SetUInt32(V("prochainIdPartie"), p.prochainIdPartie);
				}
				if (p.attaches.Size() > 0u) {
					NkVector<NkArchive> attaches;
					for (uint32 i = 0; i < p.attaches.Size(); ++i) {
						const physics::NkAttacheP2D &t = p.attaches[i];
						NkArchive o;
						o.SetUInt32(V("corps"), t.corps);
						o.SetUInt32(V("particule"), t.particule);
						o.SetUInt32(V("rigide"), t.rigide);
						o.SetString(V("ancre"), NkNombres().V2(t.ancre).Texte().View());
						o.SetFloat32(V("raideur"), t.raideur);
						o.SetFloat32(V("rupture"), t.rupture);
						o.SetBool(V("casse"), t.casse);
						attaches.PushBack(o);
					}
					a.SetObjectArray(V("attaches"), attaches);
				}
				return a;
			}

			bool LireParticules(const NkArchive &a, NkParticules2D &p, NkString &erreur) {
				p.Vider();
				NkArchive g;
				if (a.GetObject(V("reglages"), g)) {
					physics::NkReglagesP2D &r = p.reglages;
					LireV2(g, "gravite", r.gravite);
					LireI(g, "sousPas", r.sousPas);
					LireF(g, "echelleTemps", r.echelleTemps);
					LireF(g, "temperature", r.temperature);
					LireF(g, "vent", r.vent);
					LireF(g, "amortAir", r.amortAir);
					LireB(g, "collisions", r.collisions);
					LireB(g, "soudure", r.soudure);
					NkArchive lim;
					if (g.GetObject(V("limites"), lim)) {
						LireB(lim, "actif", r.limites.actif);
						NkString s;
						if (lim.GetString(V("boite"), s)) {
							NkLecteur l(s);
							l.F(r.limites.gauche), l.F(r.limites.droite), l.F(r.limites.bas), l.F(r.limites.haut);
						}
						LireB(lim, "murs", r.limites.murs);
						LireB(lim, "plafond", r.limites.plafond);
					}
				}
				LireU(a, "prochainId", p.prochainId);
				LireU(a, "rupturesTotal", p.rupturesTotal);

				NkVector<NkArchive> corps;
				(void)a.GetObjectArray(V("corps"), corps);
				for (uint32 i = 0; i < corps.Size(); ++i) {
					physics::NkCorpsP2D c;
					uint32 mat = 0;
					LireU(corps[i], "mat", mat);
					if (mat >= static_cast<uint32>(physics::NkMateriauP2D::NK_COUNT)) {
						erreur = "materiau de corps mou inconnu";
						return false;
					}
					c.mat = static_cast<physics::NkMateriauP2D>(mat);
					LireU(corps[i], "id", c.id);
					NkString s;
					if (corps[i].GetString(V("plages"), s)) {
						NkLecteur l(s);
						l.U(c.debut), l.U(c.nombre), l.U(c.lienDebut), l.U(c.lienNombre);
					}
					LireI(corps[i], "nx", c.nx);
					LireI(corps[i], "ny", c.ny);
					if (corps[i].GetString(V("parametres"), s)) {
						NkLecteur l(s);
						l.F(c.espacement), l.F(c.aireRepos), l.F(c.raideur), l.F(c.pression), l.F(c.friction);
						l.F(c.rebond), l.F(c.viscosite), l.F(c.cohesion), l.F(c.plasticite), l.F(c.resistance);
						l.F(c.masseParticule), l.F(c.formeRaideur), l.F(c.amortissement);
					}
					LireB(corps[i], "autoCollision", c.autoCollision);
					LireB(corps[i], "couplageRigide", c.couplageRigide);
					p.corps.PushBack(c);
				}

				NkArchive pa;
				uint32 n = 0;
				if (a.GetObject(V("p"), pa)) {
					LireU(pa, "n", n);
				}
				p.particules.Resize(n);
				if (n > 0u) {
					NkString sp, sq, sv, sr, sm, sy, sc, sn, se;
					(void)pa.GetString(V("pos"), sp);
					(void)pa.GetString(V("prec"), sq);
					(void)pa.GetString(V("vit"), sv);
					(void)pa.GetString(V("repos"), sr);
					(void)pa.GetString(V("masse"), sm);
					(void)pa.GetString(V("rayon"), sy);
					(void)pa.GetString(V("corps"), sc);
					(void)pa.GetString(V("voisins"), sn);
					(void)pa.GetString(V("epingle"), se);
					NkLecteur lp(sp), lq(sq), lv(sv), lr(sr), lm(sm), ly(sy), lc(sc), ln(sn), le(se);
					for (uint32 i = 0; i < n; ++i) {
						physics::NkParticule2D &q = p.particules[i];
						lp.V2(q.pos), lq.V2(q.prec), lv.V2(q.vit), lr.V2(q.repos);
						lm.F(q.masse), lm.F(q.invMasse);
						ly.F(q.rayon);
						lc.U(q.corps);
						uint32 nb = 0, e = 0;
						ln.U(nb);
						if (nb > static_cast<uint32>(physics::NK_P2D_MAX_VOISINS)) {
							erreur = "trop de voisins de soudure";
							return false;
						}
						q.nbVoisins = static_cast<uint8>(nb);
						for (uint32 k = 0; k < nb; ++k) {
							ln.U(q.voisins[k]);
						}
						le.U(e);
						q.epingle = e != 0u;
						if (q.corps >= p.corps.Size()) {
							erreur = "particule rattachee a un corps absent";
							return false;
						}
					}
					if (!lp.Ok() || !lq.Ok() || !lv.Ok() || !lr.Ok() || !lm.Ok() || !ly.Ok() || !lc.Ok() || !ln.Ok() || !le.Ok()) {
						erreur = "tableau de particules tronque";
						return false;
					}
				}

				NkArchive li;
				uint32 m = 0;
				if (a.GetObject(V("l"), li)) {
					LireU(li, "n", m);
				}
				p.liens.Resize(m);
				if (m > 0u) {
					NkString sab, sc, sr, sg;
					(void)li.GetString(V("ab"), sab);
					(void)li.GetString(V("corps"), sc);
					(void)li.GetString(V("repos"), sr);
					(void)li.GetString(V("genre"), sg);
					NkLecteur lab(sab), lc(sc), lr(sr), lg(sg);
					for (uint32 i = 0; i < m; ++i) {
						physics::NkLien2D &l = p.liens[i];
						uint32 genre = 0, casse = 0;
						lab.U(l.a), lab.U(l.b), lc.U(l.corps), lr.F(l.repos), lr.F(l.reposInitial), lg.U(genre), lg.U(casse);
						l.genre = static_cast<physics::NkGenreLien2D>(genre);
						l.casse = casse != 0u;
						if (l.a >= n || l.b >= n) {
							erreur = "lien vers une particule absente";
							return false;
						}
					}
					if (!lab.Ok() || !lc.Ok() || !lr.Ok() || !lg.Ok()) {
						erreur = "tableau de liens tronque";
						return false;
					}
				}

				// Parties et attaches (2026-09-29). Absentes d'un fichier ancien : rien.
				NkVector<NkArchive> parties;
				if (a.GetObjectArray(V("parties"), parties)) {
					for (uint32 i = 0; i < parties.Size(); ++i) {
						physics::NkPartieP2D q;
						LireU(parties[i], "id", q.id);
						LireU(parties[i], "corps", q.corps);
						LireNomCourt(parties[i], "nom", q.nom, sizeof(q.nom));
						uint32 nb = 0;
						LireU(parties[i], "n", nb);
						NkString s;
						(void)parties[i].GetString(V("particules"), s);
						NkLecteur l(s);
						q.debut = static_cast<uint32>(p.partiesParticules.Size());
						for (uint32 k = 0; k < nb; ++k) {
							uint32 idx = 0;
							l.U(idx);
							if (idx >= n) {
								erreur = "partie vers une particule absente";
								return false;
							}
							p.partiesParticules.PushBack(idx);
						}
						if (!l.Ok()) {
							erreur = "partie tronquee";
							return false;
						}
						q.nombre = nb;
						p.parties.PushBack(q);
					}
					LireU(a, "prochainIdPartie", p.prochainIdPartie);
				}
				NkVector<NkArchive> attaches;
				if (a.GetObjectArray(V("attaches"), attaches)) {
					for (uint32 i = 0; i < attaches.Size(); ++i) {
						physics::NkAttacheP2D t;
						LireU(attaches[i], "corps", t.corps);
						LireU(attaches[i], "particule", t.particule);
						LireU(attaches[i], "rigide", t.rigide);
						LireV2(attaches[i], "ancre", t.ancre);
						LireF(attaches[i], "raideur", t.raideur);
						LireF(attaches[i], "rupture", t.rupture);
						LireB(attaches[i], "casse", t.casse);
						if (t.particule >= n) {
							erreur = "attache vers une particule absente";
							return false;
						}
						p.attaches.PushBack(t);
					}
				}
				return true;
			}
		} // namespace

		// =====================================================================
		// Pour les prefabs (NkUnkenySauvegardeInterne.h)
		// =====================================================================
		namespace interne {
			NkArchive EcrireEntite(const NkScene::NkPhotoEntite &e, NkScene &scene, const NkRessourcesScene &r) {
				return ::nkentseu::unkeny::EcrireEntite(e, scene, Noms(r));
			}

			bool LireEntite(const NkArchive &a, NkScene &scene, const NkRessourcesScene &r, NkScene::NkPhotoEntite &e,
							NkString &erreur) {
				NkEntiteLue lue;
				if (!::nkentseu::unkeny::LireEntite(a, lue, erreur)) {
					return false;
				}
				DecoderJeu(a, lue, scene, r);
				ResoudreTexture(lue, r);
				e = lue.e;
				return true;
			}
		} // namespace interne

		// =====================================================================
		bool NkSauverScene(NkScene &scene, NkArchive &sortie, const NkTextures2D *textures) {
			NkRessourcesScene r;
			// Ecrire ne fait que LIRE les registres (Nom) : la constance de
			// l'appelant est tenue, le cast ne sert qu'a partager une structure.
			r.textures = const_cast<NkTextures2D *>(textures);
			return NkSauverScene(scene, sortie, r);
		}

		bool NkSauverScene(NkScene &scene, NkArchive &sortie, const NkRessourcesScene &ressources) {
			sortie.Clear();
			NkScene::NkPhoto photo;
			scene.Photographier(photo);
			const NkNoms noms = Noms(ressources);

			sortie.SetString(V("format"), V("unkeny.scene"));
			sortie.SetInt32(V("version"), NK_UNKENY_SCENE_VERSION);
			// Le compteur d'identites : une scene rouverte ne redonne pas le numero
			// d'une entite detruite avant l'enregistrement.
			sortie.SetUInt64(V("prochainUid"), photo.prochainUid);
			const NkSceneConfig &cfg = scene.Config();
			NkArchive c;
			c.SetBool(V("physique"), cfg.physique);
			c.SetBool(V("particules"), cfg.particules);
			c.SetString(V("gravite"), NkNombres().V2(cfg.gravite).Texte().View());
			c.SetFloat32(V("pasFixe"), cfg.pasFixe);
			c.SetInt32(V("pasMaxParTrame"), cfg.pasMaxParTrame);
			sortie.SetObject(V("config"), c);
			const NkVue2D &cam = scene.Camera();
			sortie.SetString(V("camera"), NkNombres().V2(cam.Centre()).F(cam.Zoom()).F(cam.Rotation()).Texte().View());

			NkVector<NkArchive> entites;
			for (uint32 i = 0; i < photo.entites.Size(); ++i) {
				entites.PushBack(EcrireEntite(photo.entites[i], scene, noms));
			}
			sortie.SetObjectArray(V("entites"), entites);
			if (scene.Particules() != nullptr) {
				sortie.SetObject(V("particules"), EcrireParticules(*scene.Particules()));
			}
			// L'eclairage de la scene, seulement s'il n'est pas celui par defaut :
			// un fichier ecrit avant lui se reecrit a l'identique.
			const NkEclairage2D &ec = scene.Eclairage();
			if (!NkEclairageParDefaut(ec)) {
				NkArchive o;
				o.SetBool(V("actif"), ec.actif);
				o.SetUInt32(V("ambiante"), ec.ambiante);
				o.SetBool(V("ombres"), ec.ombres);
				o.SetUInt32(V("masqueOcculteurs"), ec.masqueOcculteurs);
				o.SetUInt32(V("mode"), static_cast<uint32>(ec.mode));
				o.SetFloat32(V("maille"), ec.maille);
				sortie.SetObject(V("eclairage"), o);
			}
			return true;
		}

		bool NkChargerScene(NkScene &scene, const NkArchive &entree, NkTextures2D *textures, NkString *erreur) {
			NkRessourcesScene r;
			r.textures = textures;
			return NkChargerScene(scene, entree, r, erreur);
		}

		bool NkChargerScene(NkScene &scene, const NkArchive &entree, const NkRessourcesScene &ressources,
							NkString *erreur) {
			NkString err;
			auto echec = [&](const char *pourquoi) {
				if (erreur != nullptr) {
					*erreur = pourquoi;
				}
				logger.Warn("[unkeny] scene non chargee : {0}", pourquoi);
				return false;
			};

			// --- 1. TOUT lire et valider AVANT de toucher la scene -------------
			NkString format;
			if (!entree.GetString(V("format"), format) || !(format == "unkeny.scene")) {
				return echec("ce n'est pas une scene Unkeny (champ \"format\")");
			}
			int32 version = 0;
			LireI(entree, "version", version);
			if (version < 1 || version > NK_UNKENY_SCENE_VERSION) {
				return echec("version de scene inconnue (fichier plus recent que ce moteur ?)");
			}
			NkSceneConfig cfg;
			NkArchive c;
			if (entree.GetObject(V("config"), c)) {
				LireB(c, "physique", cfg.physique);
				LireB(c, "particules", cfg.particules);
				LireV2(c, "gravite", cfg.gravite);
				LireF(c, "pasFixe", cfg.pasFixe);
				LireI(c, "pasMaxParTrame", cfg.pasMaxParTrame);
			}

			NkVector<NkArchive> brutes;
			(void)entree.GetObjectArray(V("entites"), brutes);
			NkVector<NkEntiteLue> lues;
			lues.Resize(brutes.Size());
			uint64 plusGrand = 0;
			for (uint32 i = 0; i < brutes.Size(); ++i) {
				if (!LireEntite(brutes[i], lues[i], err)) {
					return echec(err.CStr());
				}
				if (lues[i].e.uid > plusGrand) {
					plusGrand = lues[i].e.uid;
				}
			}

			NkScene::NkPhoto photo;
			NkArchive ea;
			if (entree.GetObject(V("eclairage"), ea)) {
				NkEclairage2D &ec = photo.eclairage;
				uint32 mode = 0;
				LireB(ea, "actif", ec.actif);
				LireU(ea, "ambiante", ec.ambiante);
				LireB(ea, "ombres", ec.ombres);
				LireU(ea, "masqueOcculteurs", ec.masqueOcculteurs);
				LireU(ea, "mode", mode);
				LireF(ea, "maille", ec.maille);
				if (mode > static_cast<uint32>(NkModeEclairage2D::NK_VOILE)) {
					return echec("mode d'eclairage inconnu");
				}
				ec.mode = static_cast<NkModeEclairage2D>(mode);
			}
			NkArchive pa;
			const bool aParticules = entree.GetObject(V("particules"), pa);
			if (aParticules && !LireParticules(pa, photo.particules, err)) {
				return echec(err.CStr());
			}
			{
				nk_uint64 prochain = 0;
				if (entree.GetUInt64(V("prochainUid"), prochain)) {
					photo.prochainUid = prochain;
				}
				// Un fichier retouche a la main peut mentir sur son compteur : il ne
				// descend jamais sous la plus grande identite qu'il contient.
				if (photo.prochainUid <= plusGrand) {
					photo.prochainUid = plusGrand + 1u;
				}
			}

			// --- 2. Refaire la scene ---------------------------------------
			if (!scene.Init(cfg)) {
				return echec("initialisation de la scene impossible");
			}
			NkString s;
			if (entree.GetString(V("camera"), s)) {
				NkLecteur l(s);
				NkVec2f centre;
				float32 zoom = 32.f, rot = 0.f;
				if (l.V2(centre) && l.F(zoom) && l.F(rot)) {
					scene.Camera().PoserCentre(centre);
					scene.Camera().PoserZoom(zoom);
					scene.Camera().PoserRotation(rot);
				}
			}

			// Les composants "jeu", maintenant que la scene connait ses copieurs
			// (ceux d'Unkeny sont declares par Init).
			for (uint32 i = 0; i < lues.Size(); ++i) {
				DecoderJeu(brutes[i], lues[i], scene, ressources);
				ResoudreTexture(lues[i], ressources);
				photo.entites.PushBack(lues[i].e);
			}
			photo.valide = true;
			if (!aParticules && scene.Particules() != nullptr) {
				photo.particules = *scene.Particules(); // vide, reglages par defaut
			}
			scene.Restaurer(photo);
			return true;
		}

		bool NkSauverSceneJSON(NkScene &scene, NkString &json, const NkTextures2D *textures) {
			NkArchive a;
			return NkSauverScene(scene, a, textures) && NkJSONWriter::WriteArchive(a, json, true, 1);
		}

		bool NkSauverSceneJSON(NkScene &scene, NkString &json, const NkRessourcesScene &ressources) {
			NkArchive a;
			return NkSauverScene(scene, a, ressources) && NkJSONWriter::WriteArchive(a, json, true, 1);
		}

		bool NkChargerSceneJSON(NkScene &scene, NkStringView json, NkTextures2D *textures, NkString *erreur) {
			NkRessourcesScene r;
			r.textures = textures;
			return NkChargerSceneJSON(scene, json, r, erreur);
		}

		bool NkChargerSceneJSON(NkScene &scene, NkStringView json, const NkRessourcesScene &ressources,
								NkString *erreur) {
			NkArchive a;
			NkString err;
			if (!NkJSONReader::ReadArchive(json, a, &err)) {
				if (erreur != nullptr) {
					*erreur = NkString("JSON illisible : ") + err;
				}
				return false;
			}
			return NkChargerScene(scene, a, ressources, erreur);
		}

		bool NkSauverSceneFichier(NkScene &scene, const char *chemin, const NkTextures2D *textures) {
			NkRessourcesScene r;
			r.textures = const_cast<NkTextures2D *>(textures);
			return NkSauverSceneFichier(scene, chemin, r);
		}

		bool NkSauverSceneFichier(NkScene &scene, const char *chemin, const NkRessourcesScene &ressources) {
			NkString json;
			if (chemin == nullptr || !NkSauverSceneJSON(scene, json, ressources)) {
				return false;
			}
			if (!NkFile::WriteAllText(chemin, json.CStr())) {
				logger.Warn("[unkeny] ecriture de la scene impossible : {0}", chemin);
				return false;
			}
			return true;
		}

		bool NkChargerSceneFichier(NkScene &scene, const char *chemin, NkTextures2D *textures, NkString *erreur) {
			NkRessourcesScene r;
			r.textures = textures;
			return NkChargerSceneFichier(scene, chemin, r, erreur);
		}

		bool NkChargerSceneFichier(NkScene &scene, const char *chemin, const NkRessourcesScene &ressources,
								   NkString *erreur) {
			if (chemin == nullptr) {
				return false;
			}
			const NkString json = NkFile::ReadAllText(chemin);
			if (json.Empty()) {
				if (erreur != nullptr) {
					*erreur = NkString("fichier absent ou vide : ") + chemin;
				}
				return false;
			}
			return NkChargerSceneJSON(scene, json.View(), ressources, erreur);
		}

	} // namespace unkeny
} // namespace nkentseu
