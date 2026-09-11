#pragma once
// -----------------------------------------------------------------------------
// @File    ExportSVG.h
// @Brief   L'EXPORT SVG d'une page ou d'une selection : un LECTEUR DE PLUS du
//          document (l'arbre des noeuds), jamais les commandes du peintre.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUE CE FICHIER TRADUIT, ET COMMENT
// =============================================================================
//  Chaque noeud devient un <g> (son etiquette en `data-nom`, sa matrice PROPRE en
//  `transform="matrix(...)"` -- celle du noeud seul, les ancetres portant la leur).
//    - rect          : <rect rx> (rayon uniforme), <path> (rayons par coin, tracé
//                      edite) ; un element PAR REMPLISSAGE visible, dans l'ordre
//                      du document ; un element par bordure ;
//    - degrades      : <linearGradient> / <radialGradient> en `userSpaceOnUse`,
//                      les arrets tries ; « angulaire » et « losange » sont
//                      NOMMES et rendus en lineaire, dit dans un commentaire ;
//    - bordures      : cotes egaux -> `stroke` (interieure / exterieure : le
//                      rectangle est retreci / elargi d'une demi-epaisseur,
//                      SVG ne connaissant que le trait centre) ; cotes inegaux
//                      -> un <line> par cote, la position ignoree (dit) ;
//    - ombres        : <filter><feDropShadow> (flou = ecart-type flou / 2) ;
//                      l'ombre interne n'est pas traduite (dit) ;
//    - texte         : <text> avec `font-family="Inter"`, le corps, la graisse,
//                      l'ancrage ; la ligne de base est celle du peintre (les
//                      metriques d'Inter lues dans la police embarquee) ;
//    - images        : <image href> RELATIF au dossier de sortie, ou `data:`
//                      base64 quand on embarque (le fichier tel quel, PNG / JPEG
//                      / WebP / GIF ; sinon re-encode en PNG) ; Fill / Fit /
//                      Stretch par `preserveAspectRatio` ; Tile, Crop et la
//                      rotation de l'image sont approches et dits ;
//    - opacite       : `fill-opacity`, `stroke-opacity` ; le mode de fusion en
//                      `style="mix-blend-mode:..."`.
//  Ce que l'editeur dessine POUR LUI (l'etiquette au-dessus d'une page, les
//  poignees) n'est pas exporte. Un composant est approche par sa boite et son
//  libelle, et c'est dit dans les notes.
//
//  TEMOIN (sonde 82) : le SVG exporte est RE-RASTERISE par le parseur SVG maison
//  (`NkSVGCodec`, NKImage) et compare aux memes points que le PNG pour les
//  formes et les degrades ; ce parseur ne sait ni <text> ni <image> (mesure :
//  son en-tete le dit, « Pas supporte : <text>, <use>, <image> ») ni `rx` sur
//  <rect> : pour ceux-la, le temoin est STRUCTUREL, et il le dit.
// -----------------------------------------------------------------------------

#include "NKContainers/String/Encoding/NkBase64.h"
#include "NKFont/Core/NkFontParser.h" // Ⓛ le cadratin de la police : `unitsPerEm` vit ici
#include "NKImage/Codecs/PNG/NkPNGCodec.h"

#include "Export.h"
#include "Selecteur.h" // ② le style du selecteur a deux volets

namespace nkuidesign {

	namespace svgdetail {

		using nkentseu::float32;
		using nkentseu::int32;
		using nkentseu::uint32;
		using nkentseu::uint8;

		struct Ecrivain {
				NkString defs, corps, notes;
				uint32 ids = 0, elements = 0, textes = 0, images = 0, degrades = 0, groupes = 0, notesNb = 0;
				DesignState *st = nullptr;
				const NkLayoutResult *lay = nullptr;
				bool embarquer = false;
				NkString dossierSortie; ///< avec separateur final ; "" = le dossier du document
				int32 profondeur = 0;
		};

		inline void Nombre(float32 v, char *b, nkentseu::usize cap) {
			snprintf(b, cap, "%.3f", (double)v);
			// les zeros de queue s'en vont : « 12.000 » -> « 12 », « 0.500 » -> « 0.5 »
			int32 n = 0;
			while (b[n])
				++n;
			bool point = false;
			for (int32 i = 0; i < n; ++i)
				if (b[i] == '.')
					point = true;
			if (!point)
				return;
			while (n > 0 && b[n - 1] == '0')
				b[--n] = 0;
			if (n > 0 && b[n - 1] == '.')
				b[--n] = 0;
			if (n == 2 && b[0] == '-' && b[1] == '0')
				b[0] = '0', b[1] = 0;
		}
		inline void Ajouter(NkString &s, const char *cle, float32 v) {
			char b[48];
			Nombre(v, b, sizeof(b));
			s.Append(" ");
			s.Append(cle);
			s.Append("=\"");
			s.Append(b);
			s.Append("\"");
		}
		inline void Ajouter(NkString &s, const char *cle, const char *v) {
			s.Append(" ");
			s.Append(cle);
			s.Append("=\"");
			s.Append(v ? v : "");
			s.Append("\"");
		}
		inline NkString Echapper(const char *t) {
			NkString o;
			for (const char *p = t ? t : ""; *p; ++p) {
				switch (*p) {
					case '&': o.Append("&amp;"); break;
					case '<': o.Append("&lt;"); break;
					case '>': o.Append("&gt;"); break;
					case '"': o.Append("&quot;"); break;
					default: o.Append(*p); break;
				}
			}
			return o;
		}
		inline void Indenter(Ecrivain &e) {
			for (int32 i = 0; i < e.profondeur; ++i)
				e.corps.Append("  ");
		}
		inline void Note(Ecrivain &e, const char *t) {
			e.notes.Append("  <!-- ");
			e.notes.Append(Echapper(t).Data());
			e.notes.Append(" -->\n");
			++e.notesNb;
		}
		inline void Hex(uint32 rgba, char *out /* 8 */) {
			snprintf(out, 8, "#%02x%02x%02x", (unsigned)((rgba >> 24) & 0xFFu), (unsigned)((rgba >> 16) & 0xFFu),
					 (unsigned)((rgba >> 8) & 0xFFu));
		}
		inline float32 Alpha(uint32 rgba) {
			return (float32)(rgba & 0xFFu) / 255.f;
		}
		inline float32 Pourcent(float32 p) {
			return p < 0.f ? 0.f : (p > 100.f ? 1.f : p * 0.01f);
		}
		/// LE MODE DE FUSION, TEL QUE LE FORMAT LE CONNAIT -- pas tel que notre
		/// peintre le sait faire.
		///
		/// 🔴 CORRIGE LE 07/09 : cette table ne rendait que les CINQ modes exacts
		///    de notre peintre et jetait les treize autres. *L'export s'alignait sur
		///    la limite de l'ECRAN au lieu de la capacite du FORMAT.* Un document qui
		///    porte « overlay » perdait le mot en chemin, alors que tout lecteur SVG
		///    sait le peindre. Ce que NOUS ne savons pas peindre n'est pas une raison
		///    de ne pas l'ECRIRE.
		/// ⚠️ MAIS ON N'INVENTE RIEN : `plus-darker` est un nom de Lunacy, PAS une
		///    valeur de `mix-blend-mode`. Il n'est pas exporte, et l'appelant le note.
		inline const char *Fusion(const NkString &f) {
			const char *c = f.Data();
			if (!c || !*c)
				return nullptr;
			// Les valeurs de `mix-blend-mode` (CSS Compositing 1, plus `plus-lighter`
			// de Compositing 2). Nos cles SONT deja ces noms -- une table de
			// traduction serait une seconde orthographe a tenir.
			static const char *const kCss[16] = {
				"multiply", "screen",      "overlay",   "darken",     "lighten",   "color-dodge",
				"color-burn", "hard-light", "soft-light", "difference", "exclusion", "hue",
				"saturation", "color",     "luminosity", "plus-lighter"};
			for (nkentseu::uint32 i = 0; i < 16u; ++i)
				if (StrEq(c, kCss[i]))
					return kCss[i];
			return nullptr; // hors CSS (`plus-darker`) ou mode inconnu : non exporte, et dit
		}
		inline void Style(NkString &s, const NkRemplissage *f) {
			const char *m = f ? Fusion(f->fusion) : nullptr;
			if (!m)
				return;
			s.Append(" style=\"mix-blend-mode:");
			s.Append(m);
			s.Append("\"");
		}

		// ── LES ROLES DE THEME, lus COMME LE PEINTRE les lit ─────────────────
		// `theme.Get(role)` : la meme valeur que `ColorOf(host.Role(nom))` sur la toile,
		// alpha compris (un role transparent donne `fill-opacity="0"`, pas un noir).
		inline uint32 RoleRGBA(const Ecrivain &e, const char *nom) {
			return e.st->theme.Get(NkDesignResolveRole(nom));
		}
		inline void AjouterRole(Ecrivain &e, const char *attr, const char *nom) {
			const uint32 c = RoleRGBA(e, nom);
			char hex[8];
			Hex(c, hex);
			Ajouter(e.corps, attr, hex);
			const float32 a = Alpha(c);
			if (a < 0.999f)
				Ajouter(e.corps, StrEq(attr, "fill") ? "fill-opacity" : "stroke-opacity", a);
		}

		// ── LES METRIQUES D'INTER, lues dans la police embarquee ─────────────
		// La ligne de base du peintre : y = r.y + (r.h - lineHeight) / 2 + ascent.
		/// Ⓛ LE CORPS SVG N'EST PAS NOTRE CORPS EN PIXELS (2026-09-05, temoin croise de
		///    l'agent codec SVG : rapport ~1,25 mesure entre son rendu et le notre).
		///    En SVG, `font-size` est le CADRATIN (l'em) : un lecteur echelonne les contours
		///    par `font-size / unitsPerEm`. `NkFontAtlas`, lui, echelonne par
		///    `px / (ascender - descender)` (`NkScaleForPixelHeight`). Pour un meme nombre,
		///    le lecteur SVG dessine donc plus grand dans le rapport
		///    `(ascender - descender) / unitsPerEm` -- 3408 / 2816 = 1,21 pour Inter.
		///    Le corps ECRIT dans le SVG est donc `px * unitsPerEm / (ascender - descender)`.
		/// ⚠️ CALCULE DEPUIS LA POLICE CHARGEE, jamais en dur : une autre police a un autre
		///    rapport (et l'ecart passerait inapercu, comme celui-ci l'a fait).
		inline bool MetriquesEx(float32 px, float32 &ascent, float32 &ligne, float32 &corpsSvg);
		inline bool Metriques(float32 px, float32 &ascent, float32 &ligne) {
			float32 c = 0.f;
			return MetriquesEx(px, ascent, ligne, c);
		}
		inline bool MetriquesEx(float32 px, float32 &ascent, float32 &ligne, float32 &corpsSvg) {
			struct Entree {
					float32 px = 0.f;
					float32 ascent = 0.f, ligne = 0.f, corpsSvg = 0.f;
					bool ok = false;
			};
			static Entree cache[12];
			static int32 nb = 0;
			for (int32 i = 0; i < nb; ++i)
				if (cache[i].px == px) {
					ascent = cache[i].ascent;
					ligne = cache[i].ligne;
					corpsSvg = cache[i].corpsSvg; // ⚠️ la sortie par le CACHE renseigne les TROIS
													   //    valeurs : elle en oubliait une, et la sonde
													   //    l'a vue avant l'oeil (corps 0 au lieu de 11,57)
					return cache[i].ok;
				}
			Entree en;
			en.px = px;
			nkentseu::nkgui::NkGuiFont *f = nkentseu::memory::NkGetDefaultAllocator().New<nkentseu::nkgui::NkGuiFont>();
			if (f) {
				if (f->LoadEmbedded(nkentseu::NkEmbeddedFontId::Inter, px, false)) {
					en.ascent = f->Ascent();
					en.ligne = f->LineHeight();
					// Ⓛ le corps SVG : `px * unitsPerEm / (ascender - descender)`, lu dans la face
					const nkentseu::NkFont *face = f->Face();
					const nkentseu::nkfont::NkFontFaceInfo *info = face ? face->m_FaceInfo : nullptr;
					if (info && info->unitsPerEm > 0 && (info->ascent - info->descent) != 0)
						en.corpsSvg = px * (float32)info->unitsPerEm / (float32)(info->ascent - info->descent);
					else
						en.corpsSvg = px; // metriques de face absentes : le corps nu, et la note le dira
					en.ok = true;
				}
				nkentseu::memory::NkGetDefaultAllocator().Delete(f);
			}
			if (!en.ok) { // sans police : les rapports d'Inter (hhea 2728 / 3408 em), dits
				en.ascent = px * 0.8005f;
				en.ligne = px;
				en.corpsSvg = px * 2816.f / 3408.f; // Inter : upm / (asc - desc)
			}
			if (nb < 12)
				cache[nb++] = en;
			ascent = en.ascent;
			ligne = en.ligne;
			corpsSvg = en.corpsSvg;
			return en.ok;
		}

		// ── LA GEOMETRIE D'UN RECT : rect, path arrondi par coin, ou trace ──
		struct Forme {
				NkString ouverture; ///< « <rect x=.. » ou « <path d=.. » sans fermeture
				bool estRect = false;
		};
		inline void ChemainPolygone(NkString &d, const float32 *xy, uint32 nb) {
			char b[48], c[48];
			for (uint32 i = 0; i < nb; ++i) {
				Nombre(xy[i * 2], b, sizeof(b));
				Nombre(xy[i * 2 + 1], c, sizeof(c));
				d.Append(i == 0 ? "M" : "L");
				d.Append(b);
				d.Append(" ");
				d.Append(c);
			}
			d.Append("Z");
		}
		inline Forme FormeRect(const NkUINode &n, const NkPaintRect &r, float32 retrait) {
			Forme f;
			float32 R[4], c[4];
			renderdetail::NkGRayons(n, R);
			const NkPaintRect q = {r.x + retrait, r.y + retrait, r.w - 2.f * retrait, r.h - 2.f * retrait};
			for (int32 i = 0; i < 4; ++i)
				R[i] = R[i] - retrait;
			renderdetail::NkGBornerRayons(q.w > 0.f ? q.w : 0.f, q.h > 0.f ? q.h : 0.f, R, c);
			if (!n.sommets.Empty()) {
				float32 xy[256];
				const uint32 nb = NkContourDe(n, r, xy, 128);
				if (nb >= 3u) {
					NkString d;
					ChemainPolygone(d, xy, nb);
					f.ouverture.Append("<path");
					Ajouter(f.ouverture, "d", d.Data());
					return f;
				}
			}
			if (c[0] == c[1] && c[1] == c[2] && c[2] == c[3]) {
				f.estRect = true;
				f.ouverture.Append("<rect");
				Ajouter(f.ouverture, "x", q.x);
				Ajouter(f.ouverture, "y", q.y);
				Ajouter(f.ouverture, "width", q.w > 0.f ? q.w : 0.f);
				Ajouter(f.ouverture, "height", q.h > 0.f ? q.h : 0.f);
				if (c[0] > 0.f)
					Ajouter(f.ouverture, "rx", c[0]);
				return f;
			}
			// quatre coins distincts : un chemin avec un arc par coin (haut-gauche,
			// haut-droite, bas-droite, bas-gauche)
			char b[48], b2[48], b3[48];
			NkString d;
			auto M = [&](const char *op, float32 x, float32 y) {
				Nombre(x, b, sizeof(b));
				Nombre(y, b2, sizeof(b2));
				d.Append(op);
				d.Append(b);
				d.Append(" ");
				d.Append(b2);
			};
			auto A = [&](float32 rr, float32 x, float32 y) {
				if (rr <= 0.f) {
					M("L", x, y);
					return;
				}
				Nombre(rr, b3, sizeof(b3));
				d.Append("A");
				d.Append(b3);
				d.Append(" ");
				d.Append(b3);
				d.Append(" 0 0 1 ");
				Nombre(x, b, sizeof(b));
				Nombre(y, b2, sizeof(b2));
				d.Append(b);
				d.Append(" ");
				d.Append(b2);
			};
			M("M", q.x + c[0], q.y);
			M("L", q.x + q.w - c[1], q.y);
			A(c[1], q.x + q.w, q.y + c[1]);
			M("L", q.x + q.w, q.y + q.h - c[2]);
			A(c[2], q.x + q.w - c[2], q.y + q.h);
			M("L", q.x + c[3], q.y + q.h);
			A(c[3], q.x, q.y + q.h - c[3]);
			M("L", q.x, q.y + c[0]);
			A(c[0], q.x + c[0], q.y);
			d.Append("Z");
			f.ouverture.Append("<path");
			Ajouter(f.ouverture, "d", d.Data());
			return f;
		}

		// ── LES DEGRADES ─────────────────────────────────────────────────────
		inline NkString Degrade(Ecrivain &e, const NkDegrade &g, const NkPaintRect &r) {
			char id[24];
			snprintf(id, sizeof(id), "deg%u", ++e.ids);
			++e.degrades;
			const int32 genre = renderdetail::NkGenreDegrade(g);
			NkString &d = e.defs;
			if (genre == 2 || genre == 3) {
				d.Append("    <!-- dégradé « ");
				d.Append(g.type.Data());
				d.Append(" » : SVG n'a pas ce genre, rendu en linéaire -->\n");
			}
			if (genre == 1) {
				const renderdetail::NkGeomDegrade gm = renderdetail::NkGeomDegradeDe(r, g);
				d.Append("    <radialGradient");
				Ajouter(d, "id", id);
				Ajouter(d, "gradientUnits", "userSpaceOnUse");
				Ajouter(d, "cx", gm.ox);
				Ajouter(d, "cy", gm.oy);
				Ajouter(d, "r", gm.rx);
				if (gm.rx > 0.f && (gm.ry < gm.rx - 0.01f || gm.ry > gm.rx + 0.01f)) {
					// une ellipse : l'axe y est etire autour de l'origine
					char t[160], a[48], b[48], c[48];
					Nombre(gm.ox, a, sizeof(a));
					Nombre(gm.oy, b, sizeof(b));
					Nombre(gm.ry / gm.rx, c, sizeof(c));
					snprintf(t, sizeof(t), "translate(%s %s) scale(1 %s) translate(-%s -%s)", a, b, c, a, b);
					Ajouter(d, "gradientTransform", t);
				}
			} else {
				const renderdetail::NkAxeDegrade ax = renderdetail::NkAxeDegradeDe(r, g);
				d.Append("    <linearGradient");
				Ajouter(d, "id", id);
				Ajouter(d, "gradientUnits", "userSpaceOnUse");
				Ajouter(d, "x1", ax.ax);
				Ajouter(d, "y1", ax.ay);
				Ajouter(d, "x2", ax.bx);
				Ajouter(d, "y2", ax.by);
			}
			d.Append(">\n");
			const renderdetail::NkArretsTries tri(g);
			for (uint32 k = 0; k < tri.nb; ++k) {
				const NkArretDegrade &a = g.arrets[tri.ordre[k]];
				const uint32 c = renderdetail::NkGCouleur(a.couleur.Data());
				char hex[8];
				Hex(c, hex);
				d.Append("      <stop");
				Ajouter(d, "offset", a.position);
				Ajouter(d, "stop-color", hex);
				const float32 op = Alpha(c) * Pourcent(a.opacite);
				if (op < 0.999f)
					Ajouter(d, "stop-opacity", op);
				d.Append("/>\n");
			}
			d.Append(genre == 1 ? "    </radialGradient>\n" : "    </linearGradient>\n");
			NkString url("url(#");
			url.Append(id);
			url.Append(")");
			return url;
		}

		// ── L'IMAGE D'UN REMPLISSAGE ─────────────────────────────────────────
		inline const char *Mime(const char *chemin) {
			int32 n = 0;
			while (chemin && chemin[n])
				++n;
			auto fin = [&](const char *ext) {
				int32 m = 0;
				while (ext[m])
					++m;
				if (m > n)
					return false;
				for (int32 i = 0; i < m; ++i) {
					char a = chemin[n - m + i], b = ext[i];
					if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
					if (a != b)
						return false;
				}
				return true;
			};
			if (fin(".png")) return "image/png";
			if (fin(".jpg") || fin(".jpeg")) return "image/jpeg";
			if (fin(".webp")) return "image/webp";
			if (fin(".gif")) return "image/gif";
			return nullptr;
		}
		inline void Image(Ecrivain &e, const NkUINode &n, const NkRemplissage &f, const NkPaintRect &r) {
			using namespace nkentseu;
			DesignState &st = *e.st;
			st.images.base = NkCacheImages::Dossier(st.cheminActif.Data());
			NkCacheImages::Entree &en = st.images.Charger(f.image.Data());
			if (en.absente) {
				char t[600];
				snprintf(t, sizeof(t), "image introuvable, non exportée : %s", f.image.Data());
				Note(e, t);
				return;
			}
			NkString href;
			if (e.embarquer) {
				const char *mime = Mime(en.absolu.Data());
				NkVector<nk_uint8> octets;
				if (mime)
					octets = NkFile::ReadAllBytes(en.absolu.Data());
				if (mime && !octets.Empty()) {
					href.Append("data:");
					href.Append(mime);
					href.Append(";base64,");
					href.Append(encoding::base64::NkEncode(octets.Data(), (usize)octets.Size()).Data());
				} else {
					// un format que le navigateur ne lit pas tel quel (TGA, QOI...) : re-encode en PNG
					NkImage img;
					uint8 *out = nullptr;
					usize taille = 0;
					if (img.Create((uint32)en.w, (uint32)en.h, math::NkColor(), 4) && img.Pixels()) {
						uint8 *px = img.Pixels();
						for (uint32 i = 0; i < (uint32)en.pixels.Size(); ++i)
							px[i] = en.pixels[i];
						if (NkPNGCodec::Encode(img, out, taille) && out && taille > 0) {
							href.Append("data:image/png;base64,");
							href.Append(encoding::base64::NkEncode(out, taille).Data());
							memory::NkFree(out);
						}
					}
					if (href.Empty()) {
						Note(e, "image non embarquée : ni lisible telle quelle, ni ré-encodable");
						return;
					}
					Note(e, "image ré-encodée en PNG pour l'embarquer (format d'origine non lu par les navigateurs)");
				}
			} else {
				href = e.dossierSortie.Empty() ? f.image : NkCacheImages::Relatif(e.dossierSortie.Data(), en.absolu.Data());
				if (NkCacheImages::EstAbsolu(href.Data()))
					Note(e, "image hors du dossier de sortie : chemin absolu écrit tel quel");
			}
			++e.images;
			++e.elements;
			Indenter(e);
			e.corps.Append("<image");
			Ajouter(e.corps, "x", r.x);
			Ajouter(e.corps, "y", r.y);
			Ajouter(e.corps, "width", r.w);
			Ajouter(e.corps, "height", r.h);
			const char *cad = f.cadrage.Data();
			const char *par = "xMidYMid slice"; // fill (couvre) : le defaut du peintre
			if (cad && StrEq(cad, "fit"))
				par = "xMidYMid meet";
			else if (cad && StrEq(cad, "stretch"))
				par = "none";
			else if (cad && (StrEq(cad, "tile") || StrEq(cad, "crop"))) {
				par = "none";
				char t[200];
				snprintf(t, sizeof(t), "cadrage « %s » approché par un étirement (SVG : pattern / clip non écrits)", cad);
				Note(e, t);
			}
			Ajouter(e.corps, "preserveAspectRatio", par);
			if (f.rotationImage != 0.f)
				Note(e, "rotation de l'image dans sa forme non traduite");
			const float32 op = Pourcent(f.opacite);
			if (op < 0.999f)
				Ajouter(e.corps, "opacity", op);
			Style(e.corps, &f);
			Ajouter(e.corps, "href", href.Data());
			e.corps.Append("/>\n");
			(void)n;
		}

		// ── LE RECT ──────────────────────────────────────────────────────────
		inline void Rect(Ecrivain &e, const NkUINode &n, const NkPaintRect &r, const char *fondRole) {
			using namespace nkentseu;
			const Forme forme = FormeRect(n, r, 0.f);
			auto plein = [&](const char *fill, float32 opacite, const NkRemplissage *f) {
				++e.elements;
				Indenter(e);
				e.corps.Append(forme.ouverture.Data());
				Ajouter(e.corps, "fill", fill);
				if (opacite < 0.999f)
					Ajouter(e.corps, "fill-opacity", opacite);
				Style(e.corps, f);
				e.corps.Append("/>\n");
			};
			bool peint = false;
			if (!n.fills.Empty()) {
				for (uint32 i = 0; i < (uint32)n.fills.Size(); ++i) {
					const NkRemplissage &f = n.fills[i];
					if (!f.visible)
						continue;
					if (f.EstImage()) {
						Image(e, n, f, r);
						peint = true;
						continue;
					}
					if (f.degrade.Actif()) {
						const NkString url = Degrade(e, f.degrade, r);
						plein(url.Data(), 1.f, &f);
						peint = true;
						continue;
					}
					if (f.couleur.Empty())
						continue;
					const uint32 c = renderdetail::NkGCouleur(f.couleur.Data());
					char hex[8];
					Hex(c, hex);
					plein(hex, Alpha(c) * Pourcent(f.opacite), &f);
					peint = true;
				}
			} else if (!n.fill.Empty()) {
				const uint32 c = renderdetail::NkGCouleur(n.fill.Data());
				char hex[8];
				Hex(c, hex);
				plein(hex, Alpha(c), nullptr);
				peint = true;
			} else {
				const uint32 cr = RoleRGBA(e, fondRole);
				char hr[8];
				Hex(cr, hr);
				plein(hr, Alpha(cr), nullptr);
				peint = true;
			}
			// les bordures
			auto trait = [&](const NkBordure &b) {
				if (!b.visible || b.couleur.Empty() || b.epaisseur <= 0.f)
					return false;
				const uint32 c = renderdetail::NkGCouleur(b.couleur.Data());
				char hex[8];
				Hex(c, hex);
				const float32 op = Alpha(c) * Pourcent(b.opacite);
				if (b.CotesEgaux() || !forme.estRect) {
					if (!b.CotesEgaux())
						Note(e, "bordure par côté sur une forme non rectangulaire : épaisseur uniforme");
					const float32 ep = b.epaisseur;
					const float32 retrait = b.position == NkBordurePos::Interieur ? ep * 0.5f
											: b.position == NkBordurePos::Exterieur ? -ep * 0.5f
																					 : 0.f;
					const Forme ft = FormeRect(n, r, retrait);
					++e.elements;
					Indenter(e);
					e.corps.Append(ft.ouverture.Data());
					Ajouter(e.corps, "fill", "none");
					Ajouter(e.corps, "stroke", hex);
					Ajouter(e.corps, "stroke-width", ep);
					if (op < 0.999f)
						Ajouter(e.corps, "stroke-opacity", op);
					const char *j = b.jointure.Data();
					if (j && StrEq(j, "rond"))
						Ajouter(e.corps, "stroke-linejoin", "round");
					else if (j && StrEq(j, "biseau"))
						Ajouter(e.corps, "stroke-linejoin", "bevel");
					e.corps.Append("/>\n");
					return true;
				}
				// cotes inegaux : un segment par cote, centre sur le bord (la position est ignoree, dit)
				Note(e, "bordure par côté : quatre segments centrés sur le bord (intérieur / extérieur non traduits)");
				const float32 xs[4][4] = {{r.x, r.y, r.x + r.w, r.y},
										  {r.x + r.w, r.y, r.x + r.w, r.y + r.h},
										  {r.x + r.w, r.y + r.h, r.x, r.y + r.h},
										  {r.x, r.y + r.h, r.x, r.y}};
				for (uint32 k = 0; k < 4u; ++k) {
					const float32 ep = b.Cote(k);
					if (ep <= 0.f)
						continue;
					++e.elements;
					Indenter(e);
					e.corps.Append("<line");
					Ajouter(e.corps, "x1", xs[k][0]);
					Ajouter(e.corps, "y1", xs[k][1]);
					Ajouter(e.corps, "x2", xs[k][2]);
					Ajouter(e.corps, "y2", xs[k][3]);
					Ajouter(e.corps, "stroke", hex);
					Ajouter(e.corps, "stroke-width", ep);
					if (op < 0.999f)
						Ajouter(e.corps, "stroke-opacity", op);
					e.corps.Append("/>\n");
				}
				return true;
			};
			// LA MEME PORTE QUE LE PEINTRE (11/09) : l'export ne recopie plus la cle
			// historique, il lit ce qui se peint.
			bool unTrait = false;
			const NkBordure *bordures[NkUINode::kMaxBorduresPeintes];
			NkBordure bordureHistorique;
			const uint32 nbBordures =
				n.BorduresEffectives(bordures, NkUINode::kMaxBorduresPeintes, bordureHistorique);
			for (uint32 i = 0; i < nbBordures; ++i)
				unTrait = trait(*bordures[i]) || unTrait;
			(void)peint;
			(void)unTrait;
		}

		// ── LE TEXTE ─────────────────────────────────────────────────────────
		inline void Texte(Ecrivain &e, const NkUINode &n, const NkPaintRect &r) {
			using namespace nkentseu;
			const char *t = n.text.Data();
			const bool vide = !t || !*t;
			const char *contenu = vide ? (n.label.Empty() ? "Texte" : n.label.Data()) : t;
			const float32 px = n.fontPx > 0.f ? n.fontPx : 12.f;
			float32 ascent = 0.f, ligne = 0.f, corpsSvg = px;
			if (!MetriquesEx(px, ascent, ligne, corpsSvg))
				Note(e, "police embarquée indisponible : ligne de base et corps approchés par les rapports d'Inter");
			const float32 yBase = r.y + (r.h - ligne) * 0.5f + ascent;
			const char *al = n.alignText.Data();
			float32 x = r.x;
			const char *anchor = "start";
			if (al && StrEq(al, "centre")) {
				x = r.x + r.w * 0.5f;
				anchor = "middle";
			} else if (al && StrEq(al, "droite")) {
				x = r.x + r.w;
				anchor = "end";
			}
			char hex[8];
			float32 op = 1.f;
			if (!vide && !n.textColor.Empty()) {
				const uint32 c = renderdetail::NkGCouleur(n.textColor.Data());
				Hex(c, hex);
				op = Alpha(c);
			} else {
				const uint32 cr = RoleRGBA(e, vide ? "text_muted" : "doc_text");
				Hex(cr, hex);
				op = Alpha(cr);
			}
			++e.elements;
			++e.textes;
			Indenter(e);
			e.corps.Append("<text");
			Ajouter(e.corps, "x", x);
			Ajouter(e.corps, "y", yBase);
			Ajouter(e.corps, "font-family", "Inter, sans-serif");
			Ajouter(e.corps, "font-size", corpsSvg); // Ⓛ le CADRATIN, pas notre corps en pixels
			if (n.fontWeight > 0.f)
				Ajouter(e.corps, "font-weight", n.fontWeight);
			Ajouter(e.corps, "fill", hex);
			if (op < 0.999f)
				Ajouter(e.corps, "fill-opacity", op);
			Ajouter(e.corps, "text-anchor", anchor);
			Ajouter(e.corps, "xml:space", "preserve");
			e.corps.Append(">");
			e.corps.Append(Echapper(contenu).Data());
			e.corps.Append("</text>\n");
		}

		// ── UN POLYGONE PLEIN ────────────────────────────────────────────────
		inline void Polygone(Ecrivain &e, const float32 *xy, uint32 nb, const char *fill, float32 op) {
			++e.elements;
			Indenter(e);
			e.corps.Append("<polygon points=\"");
			char b[48], c[48];
			for (uint32 i = 0; i < nb; ++i) {
				Nombre(xy[i * 2], b, sizeof(b));
				Nombre(xy[i * 2 + 1], c, sizeof(c));
				if (i)
					e.corps.Append(" ");
				e.corps.Append(b);
				e.corps.Append(",");
				e.corps.Append(c);
			}
			e.corps.Append("\"");
			Ajouter(e.corps, "fill", fill);
			if (op < 0.999f)
				Ajouter(e.corps, "fill-opacity", op);
			e.corps.Append("/>\n");
		}

		// ── LE NOEUD ─────────────────────────────────────────────────────────
		inline void Noeud(Ecrivain &e, int32 i, const NkMat2D &parentEff) {
			using namespace nkentseu;
			const NkUIDocument &doc = e.st->doc;
			if (!doc.IsValidIndex(i) || !e.lay->Has(i))
				return;
			const NkUINode &n = doc.nodes[(uint32)i];
			if (n.masque)
				return;
			const NkPaintRect r = e.lay->At(i);
			// ⚠️ LA MATRICE ORTHOGONALE, ET L'EXPORT LE DIT (11/09) : SVG n'a pas de
			//    perspective -- sa `matrix(...)` est affine par definition. On ecrit donc
			//    la silhouette SANS fuite, et on pose `data-projection` / `data-focale`
			//    sur le groupe pour que le fichier ne fasse pas croire au contraire.
			//    *Un fichier qui a l'air juste et ne l'est pas est ce qu'on refuse.* Le
			//    dialogue a deux modes (garder orthogonal / aplatir en polygone), tranche
			//    par Rodolf, vient au palier C.
			const NkMat2D eff = NkMatEffective(doc, *e.lay, i, false);
			const NkMat2D propre = NkMatComposer(NkMatInverse(parentEff), eff);
			++e.groupes;
			Indenter(e);
			e.corps.Append("<g");
			{
				char id[32];
				snprintf(id, sizeof(id), "n%d", i);
				Ajouter(e.corps, "id", id);
			}
			if (!n.label.Empty())
				Ajouter(e.corps, "data-nom", Echapper(n.label.Data()).Data());
			if (n.perspective && (n.inclinaisonX != 0.f || n.inclinaisonY != 0.f)) {
				Ajouter(e.corps, "data-projection", "perspective");
				Ajouter(e.corps, "data-focale", n.focale);
			}
			// ① (07/09) L'OPACITE ET LA FUSION DU NŒUD, SUR SON GROUPE.
			// 🔴 Elles etaient PERDUES EN SILENCE : `Style()` prend un
			//    `NkRemplissage*`, jamais un nœud, donc l'export portait celles des
			//    REMPLISSAGES et jetait celles du CALQUE. A l'ecran elles agissent, au
			//    fichier elles disparaissaient -- un SVG coherent avec lui-meme et
			//    incomplet, la meme famille que l'arrondi par coin.
			// ⚠️ SUR LE `<g>`, ET C'EST EXACTEMENT LA BONNE PLACE : SVG applique
			//    `opacity` au GROUPE -- donc au nœud ET a ses enfants, composite UNE
			//    SEULE FOIS. C'est le vrai calque que notre peintre ne sait pas encore
			//    faire (il multiplie element par element) : sur ce point l'export est
			//    PLUS JUSTE que l'ecran. C'est un fait, pas un defaut -- et il faut le
			//    savoir avant de comparer les deux.
			// ⚠️ ADDITIVES : rien tant qu'elles valent leur defaut.
			if (n.opacite != 100.f)
				Ajouter(e.corps, "opacity", Pourcent(n.opacite));
			if (!n.fusion.Empty()) {
				if (const char *mf = Fusion(n.fusion)) {
					e.corps.Append(" style=\"mix-blend-mode:");
					e.corps.Append(mf);
					e.corps.Append("\"");
				} else {
					char t[160];
					snprintf(t, sizeof(t),
							 "mode de fusion « %s » sans equivalent CSS : non exporte",
							 n.fusion.Data());
					Note(e, t);
				}
			}
			if (!propre.Identite()) {
				char t[200], a[48], b[48], c[48], d[48], f[48], g[48];
				Nombre(propre.a, a, sizeof(a));
				Nombre(propre.b, b, sizeof(b));
				Nombre(propre.c, c, sizeof(c));
				Nombre(propre.d, d, sizeof(d));
				Nombre(propre.e, f, sizeof(f));
				Nombre(propre.f, g, sizeof(g));
				snprintf(t, sizeof(t), "matrix(%s %s %s %s %s %s)", a, b, c, d, f, g);
				Ajouter(e.corps, "transform", t);
			}
			// les ombres portees : un filtre par noeud (feDropShadow par effet)
			bool ombre = false;
			for (uint32 k = 0; k < (uint32)n.effets.Size(); ++k)
				if (n.effets[k].visible && !n.effets[k].couleur.Empty() && n.effets[k].opacite > 0.f) {
					if (n.effets[k].type == NkEffetType::OmbrePortee)
						ombre = true;
					else
						Note(e, "ombre interne non traduite (SVG : un filtre composite à écrire)");
				}
			if (ombre) {
				char id[24];
				snprintf(id, sizeof(id), "ombre%u", ++e.ids);
				e.defs.Append("    <filter");
				Ajouter(e.defs, "id", id);
				Ajouter(e.defs, "x", "-50%");
				Ajouter(e.defs, "y", "-50%");
				Ajouter(e.defs, "width", "200%");
				Ajouter(e.defs, "height", "200%");
				e.defs.Append(">\n");
				for (uint32 k = 0; k < (uint32)n.effets.Size(); ++k) {
					const NkEffet &ef = n.effets[k];
					if (!ef.visible || ef.couleur.Empty() || ef.type != NkEffetType::OmbrePortee || ef.opacite <= 0.f)
						continue;
					const uint32 c = renderdetail::NkGCouleur(ef.couleur.Data());
					char hex[8];
					Hex(c, hex);
					e.defs.Append("      <feDropShadow");
					Ajouter(e.defs, "dx", ef.x);
					Ajouter(e.defs, "dy", ef.y);
					Ajouter(e.defs, "stdDeviation", ef.flou * 0.5f);
					Ajouter(e.defs, "flood-color", hex);
					Ajouter(e.defs, "flood-opacity", Alpha(c) * Pourcent(ef.opacite));
					e.defs.Append("/>\n");
					if (ef.etendue != 0.f)
						Note(e, "étendue d'ombre non traduite (feMorphology à écrire)");
				}
				e.defs.Append("    </filter>\n");
				char url[40];
				snprintf(url, sizeof(url), "url(#%s)", id);
				Ajouter(e.corps, "filter", url);
			}
			e.corps.Append(">\n");
			++e.profondeur;

			const char *shape = n.shape.Data();
			const bool groupeDeclare = n.shape.Empty() && n.component.Empty() && !n.genre.Empty();
			const bool posee = n.parent >= 0
							   && (doc.nodes[(uint32)n.parent].layout.kind == NkLayoutKind::Free
								   || doc.nodes[(uint32)n.parent].layout.kind == NkLayoutKind::Anchor);
			if (groupeDeclare) {
				// un groupe ne peint rien
			} else if (n.IsFrame() && posee && r.w > 0.f && r.h > 0.f) {
				if (shape && StrEq(shape, "frame")) {
					// la page : son fond de theme, son cadre ; l'etiquette de l'editeur n'est pas exportee
					NkUINode page;
					page.radius = n.radius;
					page.rayonsDelies = n.rayonsDelies;
					for (int32 k = 0; k < 4; ++k)
						page.rayonsCoins[k] = n.rayonsCoins[k];
					const Forme f = FormeRect(page, r, 0.f);
					++e.elements;
					Indenter(e);
					e.corps.Append(f.ouverture.Data());
					AjouterRole(e, "fill", "artboard_bg");
					AjouterRole(e, "stroke", "border");
					Ajouter(e.corps, "stroke-width", 1.f);
					e.corps.Append("/>\n");
				} else if (shape && StrEq(shape, "rect")) {
					Rect(e, n, r, "doc_field_bg");
				} else if (shape && StrEq(shape, "ellipse")) {
					++e.elements;
					Indenter(e);
					e.corps.Append("<ellipse");
					Ajouter(e.corps, "cx", r.x + r.w * 0.5f);
					Ajouter(e.corps, "cy", r.y + r.h * 0.5f);
					Ajouter(e.corps, "rx", r.w * 0.5f);
					Ajouter(e.corps, "ry", r.h * 0.5f);
					AjouterRole(e, "fill", "doc_field_bg");
					e.corps.Append("/>\n");
				} else if (shape && (StrEq(shape, "line") || StrEq(shape, "line_up"))) {
					const bool monte = StrEq(shape, "line_up");
					const NkBordure *bd = nullptr;
					for (uint32 bi = 0; bi < (uint32)n.borders.Size() && !bd; ++bi)
						if (n.borders[bi].visible && !n.borders[bi].couleur.Empty() && n.borders[bi].epaisseur > 0.f)
							bd = &n.borders[bi];
					char hex[8];
					float32 op = 1.f;
					if (bd) {
						const uint32 c = renderdetail::NkGCouleur(bd->couleur.Data());
						Hex(c, hex);
						op = Alpha(c) * Pourcent(bd->opacite);
					} else
						{ const uint32 cr = RoleRGBA(e, "doc_text"); Hex(cr, hex); op = Alpha(cr); }
					++e.elements;
					Indenter(e);
					e.corps.Append("<line");
					Ajouter(e.corps, "x1", r.x);
					Ajouter(e.corps, "y1", monte ? r.y + r.h : r.y);
					Ajouter(e.corps, "x2", r.x + r.w);
					Ajouter(e.corps, "y2", monte ? r.y : r.y + r.h);
					Ajouter(e.corps, "stroke", hex);
					Ajouter(e.corps, "stroke-width", bd ? bd->epaisseur : 2.f);
					if (op < 0.999f)
						Ajouter(e.corps, "stroke-opacity", op);
					const char *ext = bd ? bd->extremite.Data() : "";
					if (ext && StrEq(ext, "ronde"))
						Ajouter(e.corps, "stroke-linecap", "round");
					else if (ext && StrEq(ext, "carree"))
						Ajouter(e.corps, "stroke-linecap", "square");
					e.corps.Append("/>\n");
				} else if (shape && (StrEq(shape, "triangle") || StrEq(shape, "pentagone") || StrEq(shape, "etoile"))) {
					float32 xy[256];
					const uint32 nb = NkContourDe(n, r, xy, 128);
					char hex[8];
					float32 op = 1.f;
					if (n.FondEffectif()) {
						const uint32 c = renderdetail::NkGFondRGBA(n);
						Hex(c, hex);
						op = Alpha(c);
					} else
						{ const uint32 cr = RoleRGBA(e, "doc_field_bg"); Hex(cr, hex); op = Alpha(cr); }
					if (nb >= 3u)
						Polygone(e, xy, nb, hex, op);
				} else if (shape && StrEq(shape, "fleche")) {
					const float32 ym = r.y + r.h * 0.5f;
					const float32 tete = r.w * 0.25f < 16.f ? (r.w * 0.25f) : 16.f;
					char hex[8];
					float32 op = 1.f;
					if (n.FondEffectif()) {
						const uint32 c = renderdetail::NkGFondRGBA(n);
						Hex(c, hex);
						op = Alpha(c);
					} else
						{ const uint32 cr = RoleRGBA(e, "doc_text"); Hex(cr, hex); op = Alpha(cr); }
					++e.elements;
					Indenter(e);
					e.corps.Append("<line");
					Ajouter(e.corps, "x1", r.x);
					Ajouter(e.corps, "y1", ym);
					Ajouter(e.corps, "x2", r.x + r.w - tete * 0.6f);
					Ajouter(e.corps, "y2", ym);
					AjouterRole(e, "stroke", "doc_text");
					Ajouter(e.corps, "stroke-width", 2.f);
					e.corps.Append("/>\n");
					const float32 xyT[6] = {r.x + r.w, ym, r.x + r.w - tete, ym - tete * 0.55f, r.x + r.w - tete, ym + tete * 0.55f};
					Polygone(e, xyT, 3u, hex, op);
				} else if (shape && StrEq(shape, "text")) {
					Texte(e, n, r);
				} else if (shape && (StrEq(shape, "image") || StrEq(shape, "avatar"))) {
					// les gabarits de l'editeur : leur boite, et c'est dit
					++e.elements;
					Indenter(e);
					e.corps.Append("<rect");
					Ajouter(e.corps, "x", r.x);
					Ajouter(e.corps, "y", r.y);
					Ajouter(e.corps, "width", r.w);
					Ajouter(e.corps, "height", r.h);
					AjouterRole(e, "fill", "doc_field_bg");
					AjouterRole(e, "stroke", "border");
					e.corps.Append("/>\n");
					char t[120];
					snprintf(t, sizeof(t), "gabarit « %s » : sa boîte seulement", shape);
					Note(e, t);
				} else {
					// une forme sans dessin propre : le contour de theme, comme la toile
					++e.elements;
					Indenter(e);
					e.corps.Append("<rect");
					Ajouter(e.corps, "x", r.x);
					Ajouter(e.corps, "y", r.y);
					Ajouter(e.corps, "width", r.w);
					Ajouter(e.corps, "height", r.h);
					Ajouter(e.corps, "fill", "none");
					AjouterRole(e, "stroke", "border");
					e.corps.Append("/>\n");
				}
			} else if (!n.IsFrame() && r.w > 0.f && r.h > 0.f) {
				// un composant : sa boite et son libelle, et c'est dit (HTML / CSS le liront un jour)
				++e.elements;
				Indenter(e);
				e.corps.Append("<rect");
				Ajouter(e.corps, "x", r.x);
				Ajouter(e.corps, "y", r.y);
				Ajouter(e.corps, "width", r.w);
				Ajouter(e.corps, "height", r.h);
				AjouterRole(e, "fill", "doc_field_bg");
				AjouterRole(e, "stroke", "border");
				Ajouter(e.corps, "rx", n.RayonCoin(0));
				e.corps.Append("/>\n");
				if (!n.text.Empty() || !n.label.Empty()) {
					NkUINode t;
					t.text = n.text.Empty() ? n.label : n.text;
					t.fontPx = 12.f;
					t.alignText = NkString("centre");
					Texte(e, t, r);
				}
				char t[200];
				snprintf(t, sizeof(t), "composant « %s » approché par sa boîte et son libellé", n.component.Data());
				Note(e, t);
			}
			for (uint32 k = 0; k < (uint32)n.children.Size(); ++k)
				Noeud(e, n.children[k], eff);
			--e.profondeur;
			Indenter(e);
			e.corps.Append("</g>\n");
		}

	} // namespace svgdetail

	/// Le SVG d'une page ou d'une selection, dans `svg`. `dossierSortie` sert aux
	/// chemins d'image relatifs (vide = tels qu'ecrits dans le document).
	inline bool NkExporterSVG(DesignState &st, const NkExportOptions &o, const char *dossierSortie, NkString &svg,
							  NkExportResultat &res) {
		using namespace nkentseu;
		res = NkExportResultat();
		svg = NkString();
		NkLayoutResult layLocal;
		const NkLayoutResult *lay = &st.layout;
		{
			const int32 page = o.selection ? -1 : (o.page > 0 ? o.page : NkPageParDefaut(st));
			const bool aJour = st.layout.valid.Size() == st.doc.nodes.Size()
							   && (o.selection ? !st.sel.Empty() || st.selected > 0 : page > 0 && st.layout.Has(page));
			if (!aJour) {
				NkComputeLayout(st.doc, NkPaintRect{0.f, 0.f, 1400.f, 900.f}, layLocal);
				lay = &layLocal;
			}
		}
		NkVector<int32> noeuds;
		char pourquoi[160];
		pourquoi[0] = 0;
		if (!NkZoneExport(st, *lay, o, noeuds, res.zone, pourquoi, sizeof(pourquoi))) {
			snprintf(res.message, sizeof(res.message), "ÉCHEC d'export SVG : %s — rien n'a été écrit.", pourquoi);
			return false;
		}
		renderdetail::NkPoserResolveur(&st.doc);
		svgdetail::Ecrivain e;
		e.st = &st;
		e.lay = lay;
		e.embarquer = o.embarquer;
		e.dossierSortie = NkString(dossierSortie ? dossierSortie : "");
		e.profondeur = 2;
		for (uint32 i = 0; i < (uint32)noeuds.Size(); ++i) {
			const int32 n = noeuds[i];
			const int32 parent = st.doc.nodes[(uint32)n].parent;
			const NkMat2D parentEff = parent > 0 ? NkMatEffective(st.doc, *lay, parent) : NkMat2D{};
			svgdetail::Noeud(e, n, parentEff);
		}
		char b[48], c[48], d[48], f[48];
		svgdetail::Nombre(res.zone.w, b, sizeof(b));
		svgdetail::Nombre(res.zone.h, c, sizeof(c));
		svgdetail::Nombre(-res.zone.x, d, sizeof(d));
		svgdetail::Nombre(-res.zone.y, f, sizeof(f));
		svg.Append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
		svg.Append("<!-- NkUIDesign : export SVG, un lecteur du document .nkuidoc (pas les commandes du peintre) -->\n");
		svg.Append("<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\"");
		svgdetail::Ajouter(svg, "width", b);
		svgdetail::Ajouter(svg, "height", c);
		{
			char vb[120];
			snprintf(vb, sizeof(vb), "0 0 %s %s", b, c);
			svgdetail::Ajouter(svg, "viewBox", vb);
		}
		svg.Append(">\n");
		if (!e.defs.Empty()) {
			svg.Append("  <defs>\n");
			svg.Append(e.defs.Data());
			svg.Append("  </defs>\n");
		}
		if (!e.notes.Empty())
			svg.Append(e.notes.Data());
		{
			char t[120];
			snprintf(t, sizeof(t), "translate(%s %s)", d, f);
			svg.Append("  <g");
			svgdetail::Ajouter(svg, "transform", t);
			svg.Append(">\n");
		}
		svg.Append(e.corps.Data());
		svg.Append("  </g>\n</svg>\n");
		res.ok = true;
		res.noeuds = (uint32)noeuds.Size();
		res.images = e.images;
		res.largeur = (int32)(res.zone.w + 0.5f);
		res.hauteur = (int32)(res.zone.h + 0.5f);
		snprintf(res.message, sizeof(res.message), "SVG : %u élément(s), %u texte(s), %u dégradé(s), %u image(s), %u note(s)",
				 e.elements, e.textes, e.degrades, e.images, e.notesNb);
		return true;
	}

	/// Le SVG, ecrit dans un fichier ; les images relatives au dossier du fichier.
	inline bool NkExporterSVGFichier(DesignState &st, const NkExportOptions &o, const char *chemin,
									 NkExportResultat &res) {
		using namespace nkentseu;
		if (!chemin || !*chemin) {
			snprintf(res.message, sizeof(res.message), "ÉCHEC d'export SVG : aucun chemin de destination.");
			return false;
		}
		const NkString dossier = NkCacheImages::Dossier(chemin);
		NkString svg;
		if (!NkExporterSVG(st, o, dossier.Data(), svg, res))
			return false;
		if (!NkFile::WriteAllText(chemin, svg.Data()) || !NkFile::Exists(chemin) || NkFile::GetFileSize(chemin) <= 0) {
			snprintf(res.message, sizeof(res.message), "ÉCHEC d'export : le SVG n'a pas pu être écrit dans %s.", chemin);
			res.ok = false;
			return false;
		}
		char detail[200];
		snprintf(detail, sizeof(detail), "%s", res.message);
		snprintf(res.message, sizeof(res.message), "Exporté : %s (%s, %s%s)", chemin, detail,
				 o.selection ? "sélection" : "page", o.embarquer ? ", images embarquées" : "");
		return true;
	}

	// ── ④ UN FICHIER PAR OBJET (05/09) ───────────────────────────────
	//  Rodolf : « on peut tout exporter, pas seulement les pages : meme les graphiques, les
	//  groupes et leurs enfants qui vont suivre le meme principe. » Plusieurs objets choisis,
	//  deux sorties ont un sens : UNE image (la boite englobante) ou UN FICHIER CHACUN.
	//  ⚠️ LE NOM DE CHAQUE FICHIER EST CELUI DE SON OBJET, et deux objets peuvent porter le
	//     meme nom : le second recoit un suffixe ` (2)`. Ecraser en silence aurait fait
	//     disparaitre un export sans un mot.
	/// Rend le nombre de fichiers ecrits ; `message` porte la phrase du pied.
	inline nkentseu::uint32 NkExporterParObjet(DesignState &st, const NkExportOptions &o, const char *dossier,
												   NkString &message) {
		using namespace nkentseu;
		NkVector<int32> cibles;
		for (uint32 k = 0; k < st.sel.Count(); ++k) {
			const int32 i = st.sel.items[k];
			if (!st.doc.IsValidIndex(i) || i == 0)
				continue;
			bool couvert = false; // un descendant d'un autre choisi part avec lui
			for (int32 p = st.doc.nodes[(uint32)i].parent; p > 0 && !couvert; p = st.doc.nodes[(uint32)p].parent)
				for (uint32 j = 0; j < st.sel.Count(); ++j)
					if (st.sel.items[j] == p)
						couvert = true;
			if (!couvert)
				cibles.PushBack(i);
		}
		if (cibles.Empty()) {
			message = NkString("ÉCHEC d'export : rien n'est sélectionné — rien n'a été écrit.");
			return 0u;
		}
		const NkSelection selAvant = st.sel;
		const int32 selectedAvant = st.selected;
		NkVector<NkString> ecrits;
		uint32 faits = 0u, rates = 0u;
		char dernier[300];
		dernier[0] = 0;
		for (uint32 k = 0; k < (uint32)cibles.Size(); ++k) {
			st.sel.Set(cibles[k]);
			st.selected = cibles[k];
			NkExportOptions oi = o;
			oi.selection = true;
			oi.unFichierParObjet = false;
			char nom[220];
			NkNomExportPropose(st, oi, nom, sizeof(nom));
			NkString chemin;
			{ // le meme nom deux fois : ` (2)`, ` (3)`... jamais un ecrasement silencieux
				uint32 rang = 1u;
				char essai[240];
				snprintf(essai, sizeof(essai), "%s", nom);
				bool prise = true;
				while (prise) {
					prise = false;
					for (uint32 e = 0; e < (uint32)ecrits.Size(); ++e)
						if (NkComponentDecl::StrEq(ecrits[e].Data(), essai))
							prise = true;
					if (prise) {
						++rang;
						const char *pt = nullptr;
						for (const char *q = nom; *q; ++q)
							if (*q == '.')
								pt = q;
						char base[220];
						const usize n = pt ? (usize)(pt - nom) : 0u;
						for (usize q = 0; q < n && q + 1 < sizeof(base); ++q)
							base[q] = nom[q];
						base[n] = '\0';
						snprintf(essai, sizeof(essai), "%s (%u)%s", base, rang, pt ? pt : "");
					}
				}
				ecrits.PushBack(NkString(essai));
				chemin = NkString(dossier ? dossier : "");
				if (!chemin.Empty()) {
					const char last = chemin.Data()[chemin.Length() - 1];
					if (last != '/' && last != '\\')
						chemin.Append('/');
				}
				chemin.Append(essai);
			}
			NkExportResultat r;
			const bool ok = (o.format == NkExportFormat::PNG) ? NkExporterPNG(st, oi, chemin.Data(), r)
															  : NkExporterSVGFichier(st, oi, chemin.Data(), r);
			if (ok)
				++faits;
			else {
				++rates;
				snprintf(dernier, sizeof(dernier), "%s", r.message);
			}
		}
		st.sel = selAvant;
		st.selected = selectedAvant;
		st.Recompute(NkPaintRect{0.f, 0.f, 1400.f, 900.f});
		char m[420];
		if (rates == 0u)
			snprintf(m, sizeof(m), "Exporté : %u fichier(s), un par objet, dans %s", faits, dossier ? dossier : "?");
		else
			snprintf(m, sizeof(m), "%u fichier(s) écrit(s), %u ÉCHEC(S) — dernier : %s", faits, rates, dernier);
		message = NkString(m);
		return faits;
	}

	/// ② LA VIGNETTE D'UN FICHIER pour le volet droit du selecteur. Elle passe par
	/// LE CACHE d'images de l'application (celui des remplissages) : un dossier
	/// reouvert ne recharge rien, et le televerseur GPU est deja branche dessus.
	/// Rend 0 pour tout ce qui n'est pas une image -- le navigateur peint alors son
	/// icone de nature, comportement historique, rien n'est simule.
	inline nkentseu::nk_uint64 NkVignetteFichierExport(void *user, const char *chemin) {
		if (!user || !NkEstUneImage(chemin))
			return 0u;
		DesignState &st = *static_cast<DesignState *>(user);
		st.images.base = NkString(); // le chemin donne est ABSOLU : aucun dossier de base
		NkCacheImages::Entree &e = st.images.Charger(chemin);
		return e.absente ? 0u : (nkentseu::nk_uint64)e.handle;
	}

	// ── LE MENU « EXPORTER... » : le selecteur de fichier du kit, puis l'export ──
	// ── LA SEULE PORTE QUI OUVRE LE SELECTEUR D'EXPORT ───────────────────────
	// ⚠️ DEFAUT DE LA MEME FAMILLE QUE ①, TROUVE EN RELISANT LE CHEMIN REEL : le
	//    dialogue d'export appelait `OpenPickerBase` directement. Depuis ① il
	//    heritait bien du CONTENU du dossier -- mais pas du filtre d'extension, ni
	//    des vignettes, ni des roles de theme, ni des recents : tout cela etait pose
	//    par `NkOuvrirChoixExport`, que seul l'ANCIEN menu appelait. Deux portes,
	//    une seule preparee ; et c'est la porte du dialogue que Rodolf emprunte.
	//
	//    Le remede est le meme qu'a ① : UNE seule fonction ouvre, et les deux
	//    appelants passent par elle.
	inline void NkOuvrirSelecteurExport(DesignState &st, const char *nomPropose) {
		DesignState::NkChoixExport &c = st.choixExport;
		c.picker.vignette = &NkVignetteFichierExport;
		c.picker.vignetteUser = &st;
		c.picker.roleDossier = NkDesignResolveRole("type_folder");
		c.picker.roleFichier = NkDesignResolveRole("type_tex");
		// ⑤ UNE TEINTE PAR FAMILLE. Les cinq roles `type_*` du coeur existent pour ca ;
		//    les prendre au hasard aurait fait mentir la capture.
		c.picker.rolesFamille[(uint32)nkentseu::editorkit::NkAssetIcone::Image] =
			NkDesignResolveRole("type_tex");
		c.picker.rolesFamille[(uint32)nkentseu::editorkit::NkAssetIcone::Texte] =
			NkDesignResolveRole("text_muted");
		c.picker.rolesFamille[(uint32)nkentseu::editorkit::NkAssetIcone::Code] =
			NkDesignResolveRole("type_mat");
		c.picker.rolesFamille[(uint32)nkentseu::editorkit::NkAssetIcone::Archive] =
			NkDesignResolveRole("type_anim");
		c.picker.rolesFamille[(uint32)nkentseu::editorkit::NkAssetIcone::Executable] =
			NkDesignResolveRole("accent_ui");
		// ③ les recents, session d'abord puis document : le rail les met EN TETE.
		c.picker.recents = st.RecentsPourLeRail();
		// ⑥ LES FILTRES : le format choisi d'abord, l'autre ensuite, puis TOUT.
		//    « Tous les fichiers » est toujours propose -- voir ce qu'il y a dans le
		//    dossier aide a choisir ou l'on ecrit, meme quand on enregistre.
		c.picker.filtres.Clear();
		c.picker.filtreActif = 0;
		const bool png = c.format == (nkentseu::int32)NkExportFormat::PNG;
		c.picker.AjouterFiltre(png ? "Images PNG" : "Images SVG", png ? "png" : "svg");
		c.picker.AjouterFiltre(png ? "Images SVG" : "Images PNG", png ? "svg" : "png");
		c.picker.AjouterFiltre("Tous les fichiers", "*");
		c.buf[0] = '\0';
		// ⑥ ON REPART DU DOSSIER COURANT -- le dernier ou un export a REUSSI (Rodolf :
		//    « un dossier est appele dossier courant si on a reussi a sauvegarder ou a
		//    charger un fichier de ce dossier-la »). C'est la que la notion sert : au
		//    prochain export on ne repart pas de la racine du projet.
		//    Le dossier du DOCUMENT reste le repli tant qu'aucun export n'a reussi.
		NkString dep = st.DossierImages();
		{
			const char *courant = c.picker.DossierCourant();
			if (courant && *courant && nkentseu::NkDirectory::Exists(courant))
				dep = NkString(courant);
		}
		// Le filtre suit le format choisi : chercher son SVG parmi trois cents PNG
		// etait le vrai cout de la colonne unique.
		c.picker.OuvrirNav(nkentseu::editorkit::NkSelecteurEnregistrer, dep.Data(),
				   c.format == (nkentseu::int32)NkExportFormat::PNG ? ".png" : ".svg",
				   nomPropose, c.buf, (nkentseu::int32)sizeof(c.buf));
	}

	// ── L'ANCIEN MENU « EXPORTER... » : il choisit, puis passe par LA porte ───────
	inline void NkOuvrirChoixExport(DesignState &st, NkExportFormat format, nkentseu::float32 echelle,
									 bool selection, bool embarquer) {
		DesignState::NkChoixExport &c = st.choixExport;
		c.format = (nkentseu::int32)format;
		c.echelle = echelle;
		c.selection = selection;
		c.embarquer = embarquer;
		NkExportOptions o;
		o.format = format;
		o.echelle = echelle;
		o.selection = selection;
		o.embarquer = embarquer;
		char nomPropose[220];
		NkNomExportPropose(st, o, nomPropose, sizeof(nomPropose));
		NkOuvrirSelecteurExport(st, nomPropose);
	}

	/// Le selecteur (modal, entree reelle), puis l'export au choix confirme : le pied
	/// dit le chemin ecrit ou l'echec, la Console le garde.
	inline void NkDessinerPickerExport(nkentseu::nkgui::NkGuiContext &ctx, DesignState &st) {
		using namespace nkentseu;
		DesignState::NkChoixExport &c = st.choixExport;
		if (c.picker.pickerOpen) {
			// ② LE NOUVEAU SELECTEUR. `NkDrawFilePicker` (l'ancien) n'a pas bouge et
			//    reste celui de « Choisir une image... » : deux chemins, aucun retire.
			// ⑥ LE MEME POINT D'ENTREE que « Choisir une image... » : le style vient du
			//    kit, resolu depuis la declaration du composant. Un style construit ici
			//    aurait ete le second endroit ou vivent les memes treize roles.
			editorkit::NkDrawSelecteur(ctx, c.picker, st.theme);
		}
		if (c.picker.pickerCancelled)
			c.picker.pickerCancelled = false;
		if (!c.picker.pickerConfirmed)
			return;
		c.picker.pickerConfirmed = false;
		const char *nom = c.picker.pickerResultName[0] ? c.picker.pickerResultName : c.picker.pickerSaveName;
		if (!nom || !*nom) {
			st.DireAuPied("Export annulé : aucun nom de fichier.");
			return;
		}
		NkString chemin = (NkPath(c.picker.pickerResultPath) / nom).ToString();
		const char *ext = c.format == (int32)NkExportFormat::PNG ? ".png" : ".svg";
		{
			// l'extension du format, si elle manque
			const char *p = chemin.Data();
			int32 n = 0;
			while (p[n])
				++n;
			bool a = n >= 4;
			for (int32 i = 0; a && i < 4; ++i) {
				char x = p[n - 4 + i], y = ext[i];
				if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
				if (x != y)
					a = false;
			}
			if (!a)
				chemin.Append(ext);
		}
		NkExportOptions o;
		o.format = (NkExportFormat)c.format;
		o.echelle = c.echelle;
		o.selection = c.selection;
		o.embarquer = c.embarquer;
		o.unFichierParObjet = c.parObjet;
		// ④ UN FICHIER PAR OBJET : c'est le DOSSIER choisi qui compte, pas le nom saisi
		if (o.unFichierParObjet && o.selection && st.sel.Count() >= 2u) {
			NkString msg;
			const uint32 faits = NkExporterParObjet(st, o, c.picker.pickerResultPath, msg);
			// ⑤ Plusieurs fichiers : le bandeau montre LE DOSSIER (il n'y a pas UN
			//    resultat a ouvrir), sans vignette -- laquelle des N choisir serait un
			//    arbitraire, et en montrer une ferait croire qu'il n'y en a qu'une.
			if (faits > 0u) {
				st.RetenirDossierRecent(c.picker.pickerResultPath);
				st.avisExport = DesignState::NkAvisExport();
				st.avisExport.actif = true;
				st.avisExport.chemin = NkString(c.picker.pickerResultPath);
				snprintf(st.avisExport.titre, sizeof(st.avisExport.titre),
					 "Export\u00e9 : %u fichier(s)", faits);
				snprintf(st.avisExport.detail, sizeof(st.avisExport.detail), "%s",
					 c.picker.pickerResultPath);
			}
			st.DireAuPied(msg.Data());
			st.Consigner(msg.Data());
			return;
		}
		NkExportResultat res;
		if (o.format == NkExportFormat::PNG)
			NkExporterPNG(st, o, chemin.Data(), res);
		else
			NkExporterSVGFichier(st, o, chemin.Data(), res);
		// ③ LE DOSSIER CHOISI DEVIENT UN RECENT -- session ET document. On le retient
		//    APRES l'ecriture : un export qui echoue ne doit pas laisser de trace.
		if (res.ok) {
			st.RetenirDossierRecent(c.picker.pickerResultPath);
			// ⑤ LE RESULTAT SE VOIT (Rodolf : « je pourrais vraiment avoir le resultat
			//    exporte une fois le dialogue traite »). Le bandeau porte la VIGNETTE DU
			//    FICHIER RELU DU DISQUE -- pas un rendu de plus : si le codec avait mal
			//    ecrit, la vignette le montrerait.
			st.PoserAvisExport(chemin.Data(), res.largeur, res.hauteur,
							   o.format == NkExportFormat::PNG ? "PNG" : "SVG");
		}
		st.DireAuPied(res.message);
		st.Consigner(res.message);
	}

} // namespace nkuidesign
