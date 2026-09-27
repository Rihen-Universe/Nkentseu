// -----------------------------------------------------------------------------
// @File    NkGuiDispositions.inl
// @Brief   P10 — la lecture de la section `layout`.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#pragma once

namespace nkentseu {
	namespace nkgui {

		namespace detail {

			inline bool NkGDMotEgal(NkStringView a, const char *b) noexcept {
				usize i = 0;
				for (; i < a.Size(); ++i)
					if (b[i] == '\0' || a.Data()[i] != b[i])
						return false;
				return b[i] == '\0';
			}

			inline const NkArchiveNode *NkGDCorps(const NkArchive &bloc) noexcept {
				const NkArchiveNode *b = bloc.FindNode(NkStringView(NkGuiArchive::KeyBody()));
				return (b && b->IsArray()) ? b : nullptr;
			}

			inline bool NkGDEspace(char c) noexcept {
				return c == ' ' || c == '\t' || c == '\r' || c == '\n';
			}

			/// Un nombre décimal simple, sans exposant.
			///
			/// ⚠️ ÉCRIT À LA MAIN, ET PAS `atof`. Ce dépôt a payé un `atof` qui
			///    rendait 0.0 sur « 0,9 » en fr-FR : la bibliothèque C lit le
			///    séparateur décimal de la CULTURE, un document non. Un nombre de
			///    document se lit toujours avec le point, partout sur Terre.
			inline bool NkGDNombre(const char *p, uint32 &k, float32 &out) noexcept {
				const uint32 depart = k;
				bool negatif = false;
				if (p[k] == '-') {
					negatif = true;
					++k;
				}
				float32 v = 0.f;
				bool aChiffre = false;
				while (p[k] >= '0' && p[k] <= '9') {
					v = v * 10.f + (float32)(p[k] - '0');
					aChiffre = true;
					++k;
				}
				if (p[k] == '.') {
					++k;
					float32 ech = 0.1f;
					while (p[k] >= '0' && p[k] <= '9') {
						v += (float32)(p[k] - '0') * ech;
						ech *= 0.1f;
						aChiffre = true;
						++k;
					}
				}
				if (!aChiffre) {
					k = depart;
					return false;
				}
				out = negatif ? -v : v;
				return true;
			}

			/// `dock "nom" cote [fraction]`, lu depuis une tranche brute.
			///
			/// ⚠️ UNE TRANCHE BRUTE, PARCE QUE `dock` N'A PAS D'ACCOLADES. Le
			///    lecteur d'archive ne fait un BLOC que d'un `Type "id" { ... }` ;
			///    `dock "scene" left 0.16` n'en est pas un, et l'archive le garde
			///    en source verbatim (règle T11). C'est la même situation que
			///    `include "x"`, et c'est la même lecture.
			inline bool NkGDLireDock(NkStringView lex, NkGuiAmarrage &out) noexcept {
				const NkString brut(lex);
				const char *p = brut.CStr();
				uint32 k = 0;
				while (NkGDEspace(p[k]))
					++k;
				const char *mot = "dock";
				uint32 m = 0;
				while (mot[m] && p[k + m] == mot[m])
					++m;
				if (mot[m] != '\0')
					return false;
				k += m;
				while (NkGDEspace(p[k]))
					++k;
				if (p[k] != '"')
					return false;
				++k;
				NkString nom;
				while (p[k] && p[k] != '"') {
					const char c[2] = {p[k], '\0'};
					nom += c;
					++k;
				}
				if (p[k] != '"' || nom.Size() == 0u)
					return false;
				++k;
				while (NkGDEspace(p[k]))
					++k;
				NkString cote;
				while (p[k] && !NkGDEspace(p[k])) {
					const char c[2] = {p[k], '\0'};
					cote += c;
					++k;
				}
				out.panneau = nom;
				const NkStringView cv(cote.CStr());
				if (NkGDMotEgal(cv, "center") || NkGDMotEgal(cv, "centre"))
					out.cote = NkGuiCote::Centre;
				else if (NkGDMotEgal(cv, "left") || NkGDMotEgal(cv, "gauche"))
					out.cote = NkGuiCote::Gauche;
				else if (NkGDMotEgal(cv, "right") || NkGDMotEgal(cv, "droite"))
					out.cote = NkGuiCote::Droite;
				else if (NkGDMotEgal(cv, "top") || NkGDMotEgal(cv, "haut"))
					out.cote = NkGuiCote::Haut;
				else if (NkGDMotEgal(cv, "bottom") || NkGDMotEgal(cv, "bas"))
					out.cote = NkGuiCote::Bas;
				else
					return false; // côté inconnu : l'appelant le compte et le nomme

				while (NkGDEspace(p[k]))
					++k;
				float32 f = 0.f;
				if (NkGDNombre(p, k, f)) {
					out.fraction = f;
					out.aFraction = true;
				}
				return true;
			}

		} // namespace detail

		inline const char *NkGuiNomCote(NkGuiCote c) noexcept {
			switch (c) {
				case NkGuiCote::Gauche: return "gauche";
				case NkGuiCote::Droite: return "droite";
				case NkGuiCote::Haut: return "haut";
				case NkGuiCote::Bas: return "bas";
				default: return "centre";
			}
		}

		inline bool NkGuiLireDispositions(const NkArchive &doc, NkVector<NkGuiDisposition> &out,
										  NkGuiRapportDispositions &rap) noexcept {
			const NkArchiveNode *racine = detail::NkGDCorps(doc);
			if (!racine)
				return false;
			bool trouve = false;
			for (uint32 i = 0; i < (uint32)racine->array.Size(); ++i) {
				if (!racine->array[i].IsObject() || !racine->array[i].object)
					continue;
				const NkArchive &sec = *racine->array[i].object;
				if (!detail::NkGDMotEgal(NkGuiArchive::TypeOf(sec), "layout"))
					continue;
				trouve = true;

				NkGuiDisposition d;
				d.nom = NkString(NkGuiArchive::IdOf(sec));
				// Le nom arrive entre guillemets : on les retire, sinon aucun hôte
				// ne reconnaîtra jamais `"animation"` comme `animation`.
				if (d.nom.Size() >= 2u && d.nom.CStr()[0] == '"') {
					NkString nu;
					const char *p = d.nom.CStr();
					for (uint32 k = 1u; k + 1u < (uint32)d.nom.Size(); ++k) {
						const char c[2] = {p[k], '\0'};
						nu += c;
					}
					d.nom = nu;
				}

				const NkArchiveNode *corps = detail::NkGDCorps(sec);
				uint32 centres = 0u;
				if (corps) {
					for (uint32 j = 0; j < (uint32)corps->array.Size(); ++j) {
						const NkArchiveNode &n = corps->array[j];
						if (n.IsObject())
							continue; // un bloc dans un `layout` : pas un amarrage
						NkGuiAmarrage a;
						if (!detail::NkGDLireDock(n.Lexeme(), a)) {
							// ⚠️ ON NE DEVINE PAS LE CÔTÉ. Une ligne `dock` dont le
							//    côté est mal orthographié placerait le panneau
							//    ailleurs que demandé — et personne ne saurait
							//    pourquoi. Elle se compte et se nomme.
							++rap.cotesInconnus;
							NkString r(d.nom);
							r += " : ligne `dock` illisible ou côté inconnu — ";
							r += NkString(n.Lexeme());
							rap.refuses.PushBack(r);
							continue;
						}
						// ⚠️ UNE FRACTION HORS DE [0,1] N'EST PAS RAMENÉE DANS LES
						//    BORNES. La corriger en silence donnerait une
						//    disposition que personne n'a dessinée ; le document
						//    doit être repris.
						if (a.aFraction && (a.fraction <= 0.f || a.fraction >= 1.f)) {
							++rap.fractionsHorsBornes;
							NkString r(d.nom);
							r += " / ";
							r += a.panneau;
							r += " : fraction hors de ]0,1[";
							rap.refuses.PushBack(r);
							continue;
						}
						if (a.cote == NkGuiCote::Centre) {
							++centres;
							// Le centre ne se dimensionne pas : il prend ce qui
							// reste. Une fraction sur lui dirait deux choses.
							if (a.aFraction) {
								a.aFraction = false;
								a.fraction = 0.f;
							}
						}
						d.amarrages.PushBack(a);
						++rap.amarrages;
					}
				}
				if (centres > 1u) {
					// ⚠️ DEUX CENTRES SONT UNE FAUTE DE CONCEPTION, pas d'écriture :
					//    la zone restante est UNE. On garde la disposition — elle
					//    reste utilisable — et on le dit.
					rap.centresMultiples += centres - 1u;
					NkString r(d.nom);
					r += " : plusieurs panneaux au centre — la zone restante est UNE";
					rap.refuses.PushBack(r);
				}
				out.PushBack(d);
				++rap.dispositions;
			}
			return trouve;
		}

	} // namespace nkgui
} // namespace nkentseu
