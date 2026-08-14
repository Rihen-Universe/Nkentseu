//
// NkPdfInfo.cpp — metadonnees du document (/Info) et langue (/Lang).
//
#include "NKMedia/Pdf/NkPdfInfo.h"
#include "NKMedia/Pdf/NkPdfGlyphList.h"

namespace nkentseu {
	namespace media {
		namespace pdf {

			// ── PDFDocEncoding : les positions qui DIFFERENT de Latin-1 ──
			//
			// (ISO 32000-1, annexe D.2.) Ces plages sont donnees ici par NOMS DE
			// GLYPHES et resolues via l'Adobe Glyph List deja embarquee, jamais par
			// valeurs hexadecimales recopiees. Deux raisons :
			//   - une transcription manuelle de table a deja produit un decalage
			//     d'indexation dans ce depot ; un nom mal orthographie, lui, rend
			//     une chaine vide et se voit immediatement ;
			//   - l'AGL est deja validee et attribuee (THIRD_PARTY_LICENSES.md),
			//     donc on ne duplique aucune donnee.
			// Les deux plages sont dans l'ordre exact de la table de la
			// specification, qui est alphabetique par nom de glyphe.
			static const char *const kPdfDoc18[8] = {"breve",	   "caron",	 "circumflex", "dotaccent",
													 "hungarumlaut", "ogonek", "ring",		 "tilde"};

			static const char *const kPdfDoc80[32] = {
				"bullet",		 "dagger",		  "daggerdbl",	  "ellipsis",	   "emdash",
				"endash",		 "florin",		  "fraction",	  "guilsinglleft", "guilsinglright",
				"minus",		 "perthousand",	  "quotedblbase", "quotedblleft",  "quotedblright",
				"quoteleft",	 "quoteright",	  "quotesinglbase", "trademark",   "fi",
				"fl",			 "Lslash",		  "OE",			  "Scaron",		   "Ydieresis",
				"Zcaron",		 "dotlessi",	  "lslash",		  "oe",			   "scaron",
				"zcaron",		 nullptr /*0x9F non defini*/};

			// Longueur d'une chaine C, sans <cstring> (module zero-STL).
			static int32 LongueurC(const char *s) {
				int32 n = 0;
				while (s && s[n])
					++n;
				return n;
			}

			static void AjouterUtf8(NkString &out, uint32 u) {
				if (u < 0x80u) {
					out += static_cast<char>(u);
				} else if (u < 0x800u) {
					out += static_cast<char>(0xC0u | (u >> 6));
					out += static_cast<char>(0x80u | (u & 0x3Fu));
				} else if (u < 0x10000u) {
					out += static_cast<char>(0xE0u | (u >> 12));
					out += static_cast<char>(0x80u | ((u >> 6) & 0x3Fu));
					out += static_cast<char>(0x80u | (u & 0x3Fu));
				} else {
					out += static_cast<char>(0xF0u | (u >> 18));
					out += static_cast<char>(0x80u | ((u >> 12) & 0x3Fu));
					out += static_cast<char>(0x80u | ((u >> 6) & 0x3Fu));
					out += static_cast<char>(0x80u | (u & 0x3Fu));
				}
			}

			NkString NkPdfChaineVersUtf8(const NkPdfDoc &doc, const NkPdfVal &v) {
				NkString out;
				if (v.kind != NK_PDF_STRING)
					return out;
				int32 len = 0;
				const char *p = doc.Text(v, &len);
				if (!p || len <= 0)
					return out;
				const uint8 *b = reinterpret_cast<const uint8 *>(p);

				// ── UTF-16BE, annonce par l'indicateur d'ordre FE FF ──
				if (len >= 2 && b[0] == 0xFEu && b[1] == 0xFFu) {
					for (int32 i = 2; i + 1 < len; i += 2) {
						uint32 u = (static_cast<uint32>(b[i]) << 8) | b[i + 1];
						// Paire de substitution : sans elle, tout caractere hors du
						// plan de base ressort casse en deux morceaux invalides.
						if (u >= 0xD800u && u <= 0xDBFFu && i + 3 < len) {
							const uint32 bas = (static_cast<uint32>(b[i + 2]) << 8) | b[i + 3];
							if (bas >= 0xDC00u && bas <= 0xDFFFu) {
								u = 0x10000u + ((u - 0xD800u) << 10) + (bas - 0xDC00u);
								i += 2;
							}
						}
						AjouterUtf8(out, u);
					}
					return out;
				}

				// ── UTF-16LE : hors specification, mais des producteurs en ecrivent.
				// On le reconnait a son indicateur FF FE plutot que de rendre un
				// texte ou un octet nul separe chaque lettre.
				if (len >= 2 && b[0] == 0xFFu && b[1] == 0xFEu) {
					for (int32 i = 2; i + 1 < len; i += 2)
						AjouterUtf8(out, (static_cast<uint32>(b[i + 1]) << 8) | b[i]);
					return out;
				}

				// ── PDFDocEncoding ──
				for (int32 i = 0; i < len; ++i) {
					const uint8 c = b[i];
					const char *nom = nullptr;
					if (c >= 0x18u && c <= 0x1Fu)
						nom = kPdfDoc18[c - 0x18u];
					else if (c >= 0x80u && c <= 0x9Fu)
						nom = kPdfDoc80[c - 0x80u];
					else if (c == 0xA0u)
						nom = "Euro"; // PDFDocEncoding place l'euro la ou Latin-1
									  // met une espace insecable

					if (nom) {
						const NkString t = NkPdfGlyphNameToText(nom, LongueurC(nom));
						if (!t.Empty())
							out.Append(t);
						continue;
					}
					if (c < 0x18u || (c >= 0x20u && c < 0x80u)) {
						// Zone ASCII : identique dans les deux encodages. Les
						// caracteres de controle sont conserves tels quels (une
						// tabulation ou un saut de ligne peut figurer dans un
						// /Subject multiligne).
						out += static_cast<char>(c);
						continue;
					}
					if (c >= 0xA1u) // 0xA1..0xFF : identique a Latin-1
						AjouterUtf8(out, static_cast<uint32>(c));
					// 0x9F et 0xA0 non traites ci-dessus : non definis, on se tait.
				}
				return out;
			}

			// Lit `n` chiffres a la position `i` si tous sont presents. Rend false
			// des qu'il en manque un : la chaine est alors simplement plus courte
			// que le format complet, ce qui est le cas le plus frequent.
			static bool LireEntier(const char *s, int32 len, int32 &i, int32 n, int32 &out) {
				if (i + n > len)
					return false;
				int32 v = 0;
				for (int32 k = 0; k < n; ++k) {
					const char c = s[i + k];
					if (c < '0' || c > '9')
						return false;
					v = v * 10 + (c - '0');
				}
				i += n;
				out = v;
				return true;
			}

			NkPdfDate NkPdfAnalyserDate(const NkString &s) {
				NkPdfDate d;
				d.brut = s;
				const char *p = s.CStr();
				int32 len = static_cast<int32>(s.Size());
				int32 i = 0;

				// Le prefixe « D: » est OBLIGATOIRE dans la specification et
				// pourtant souvent absent. On l'accepte dans les deux cas.
				if (len >= 2 && p[0] == 'D' && p[1] == ':')
					i = 2;

				if (!LireEntier(p, len, i, 4, d.annee))
					return d; // sans annee, il n'y a rien a garder
				d.valide = true;

				// Tout le reste est facultatif, et chaque etape s'arrete net des
				// qu'un champ manque : « D:2019 » et « D:20190312 » sont l'un et
				// l'autre des dates exploitables.
				if (!LireEntier(p, len, i, 2, d.mois))
					return d;
				if (!LireEntier(p, len, i, 2, d.jour))
					return d;
				if (!LireEntier(p, len, i, 2, d.heure))
					return d;
				if (!LireEntier(p, len, i, 2, d.minute))
					return d;
				if (!LireEntier(p, len, i, 2, d.seconde))
					return d;

				if (i < len) {
					const char signe = p[i];
					if (signe == 'Z') {
						d.aFuseau = true;
						d.fuseauMin = 0;
					} else if (signe == '+' || signe == '-') {
						++i;
						int32 hh = 0, mm = 0;
						if (LireEntier(p, len, i, 2, hh)) {
							d.aFuseau = true;
							// L'apostrophe qui separe heures et minutes est parfois
							// absente, parfois doublee en fin de chaine.
							if (i < len && p[i] == '\'')
								++i;
							LireEntier(p, len, i, 2, mm);
							d.fuseauMin = hh * 60 + mm;
							if (signe == '-')
								d.fuseauMin = -d.fuseauMin;
						}
					}
				}
				return d;
			}

			bool NkPdfLireInfo(const NkPdfDoc &doc, NkPdfDocInfo &out) {
				out = NkPdfDocInfo();
				if (doc.PageCount() <= 0 && doc.Trailer().IsNull())
					return false;

				// /Info vit dans le TRAILER, pas dans le catalogue — confondre les
				// deux rend toujours un document sans metadonnees.
				const NkPdfVal info = doc.DictGet(doc.Trailer(), "Info");
				if (info.IsDictLike()) {
					out.titre = NkPdfChaineVersUtf8(doc, doc.DictGet(info, "Title"));
					out.auteur = NkPdfChaineVersUtf8(doc, doc.DictGet(info, "Author"));
					out.sujet = NkPdfChaineVersUtf8(doc, doc.DictGet(info, "Subject"));
					out.motsCles = NkPdfChaineVersUtf8(doc, doc.DictGet(info, "Keywords"));
					out.createur = NkPdfChaineVersUtf8(doc, doc.DictGet(info, "Creator"));
					out.producteur = NkPdfChaineVersUtf8(doc, doc.DictGet(info, "Producer"));

					const NkString c = NkPdfChaineVersUtf8(doc, doc.DictGet(info, "CreationDate"));
					const NkString m = NkPdfChaineVersUtf8(doc, doc.DictGet(info, "ModDate"));
					if (!c.Empty())
						out.creation = NkPdfAnalyserDate(c);
					if (!m.Empty())
						out.modification = NkPdfAnalyserDate(m);

					out.aInfo = !out.titre.Empty() || !out.auteur.Empty() || !out.sujet.Empty() ||
								!out.motsCles.Empty() || !out.createur.Empty() ||
								!out.producteur.Empty() || out.creation.valide ||
								out.modification.valide;
				}

				// /Lang est dans le CATALOGUE. C'est un nom ou une chaine selon les
				// producteurs : on accepte les deux plutot que de rendre vide sur
				// une forme legitime.
				const NkPdfVal lg = doc.DictGet(doc.Catalog(), "Lang");
				if (lg.kind == NK_PDF_STRING) {
					out.langue = NkPdfChaineVersUtf8(doc, lg);
				} else if (lg.kind == NK_PDF_NAME) {
					int32 len = 0;
					const char *p = doc.Text(lg, &len);
					for (int32 i = 0; i < len; ++i)
						out.langue += p[i];
				}
				return true;
			}

		} // namespace pdf
	} // namespace media
} // namespace nkentseu
