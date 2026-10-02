#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiHabillage.h
// @Brief   L'HABILLAGE d'un widget de document : ce qu'un bloc `appearance` dit
//          AU-DELA d'une couleur -- degrade, image (et image en neuf tranches),
//          ombre et lueur, coins biseautes, opacite, echelle, son, typographie
//          (police, taille, contour, ombre du texte).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI (Rihen, 02/10) : « on doit pouvoir construire des interfaces
//  STYLISEES avec notre systeme d'UI pour nos jeux video »
// =============================================================================
//  Le format savait DIRE presque tout (document 2 §13) : `fill { gradient from
//  to angle image fit }`, `shadow { offset blur spread color }`, `appearance {
//  opacity font size }`. Le monteur n'en PEIGNAIT que la couleur et le rayon --
//  le reste etait lu, compte, et rendu comme un bouton d'outil. Un menu de jeu
//  ne peut pas ressembler a un panneau d'editeur.
//
//  Ce fichier PEINT ce vocabulaire, et y ajoute ce qu'un jeu demande et que le
//  format n'avait pas (ajouts ecrits aussi dans le validateur et le document 2) :
//    fill  { fit = Slice  slice = (g, h, d, b) }   l'image en NEUF TRANCHES
//    appearance { bevel = n }                      les coins BISEAUTES
//    appearance { scale = 1.06 }                   l'echelle d'un etat
//    appearance { transition = 0.12 }              le fondu d'un etat a l'autre
//    appearance { sound = "Contenu/Sons/x.wav" }   le son d'un etat (l'hote joue)
//    text { outline = #.. shadow = #.. }           le contour et l'ombre du texte
//
//  ⚠️ ADDITIF, SANS EXCEPTION : un document qui n'ecrit rien de tout cela est
//     peint EXACTEMENT comme avant (aucun champ n'est pose, le chemin d'avant
//     garde la main).
//
//  EN-TETE SEUL, comme le monteur : l'application qui l'inclut declare
//  NKSerialization.
// -----------------------------------------------------------------------------

#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKGui/Core/NkGuiDrawList.h"
#include "NKGui/Core/NkGuiFont.h"
#include "NKGui/Doc/NkGuiImages.h"
#include "NKMath/NKMath.h"
#include "NKSerialization/NkGui/NkGuiArchive.h"

namespace nkentseu {
	namespace nkgui {

		// Declares dans NkGuiMonteur.h / NkGuiJetons.h, definis avant usage.
		inline bool NkGuiCouleur(NkStringView lex, NkColor &out) noexcept;

		/// Ce qu'UN etat d'un widget demande au-dela de la couleur et du rayon.
		/// Chaque champ a son « a » : un champ jamais ecrit n'est jamais peint.
		struct NkGuiSurface {
				// ── fill : degrade ou image ──────────────────────────────────
				bool aDegrade = false;
				NkColor de{0, 0, 0, 255};
				NkColor a{0, 0, 0, 255};
				/// Degres : 90 = de haut en bas (le defaut), 0 = de gauche a droite.
				float32 angle = 90.f;
				NkString image;		 ///< un fichier (le registre NkGuiImages le charge)
				bool aImage = false;
				/// « Stretch » (defaut), « Slice » (neuf tranches), « Fit » (proportions).
				NkString cadrage;
				bool aTranches = false;
				float32 tranches[4] = {0.f, 0.f, 0.f, 0.f}; ///< gauche, haut, droite, bas (pixels de l'image)
				// ── shadow : ombre portee, ou LUEUR (decalage nul) ───────────
				bool aOmbre = false;
				NkVec2 ombreDecalage{0.f, 2.f};
				float32 ombreFlou = 6.f;
				float32 ombreEtendue = 0.f;
				NkColor ombreCouleur{0, 0, 0, 90};
				// ── la forme et l'etat ────────────────────────────────────────
				bool aBiseau = false;
				float32 biseau = 0.f;
				bool aOpacite = false;
				float32 opacite = 1.f;
				bool aEchelle = false;
				float32 echelle = 1.f;
				bool aTransition = false;
				float32 transition = 0.f; ///< secondes
				NkString son;			  ///< joue par l'HOTE a l'entree dans l'etat
				// ── la typographie ────────────────────────────────────────────
				NkString police; ///< l'identifiant d'une `font` de la section `fonts`
				bool aTaille = false;
				float32 taille = 0.f;
				bool aContourTexte = false;
				NkColor contourTexte{0, 0, 0, 255};
				bool aOmbreTexte = false;
				NkColor ombreTexte{0, 0, 0, 160};
				// ── stroke (le contour, pour les conteneurs peints ici) ───────
				bool aContour = false;
				NkColor contour{0, 0, 0, 255};
				float32 contourLargeur = 1.f;
				// ── `fill "valeur" { ... }` : la PART remplie d'une jauge ───────
				bool aValeur = false;
				NkColor valeurCouleur{255, 255, 255, 255};
				bool aValeurDegrade = false;
				NkColor valeurDe{0, 0, 0, 255};
				NkColor valeurA{0, 0, 0, 255};
				// ── `fill { color }` relu ici (un conteneur peint par ce fichier) ──
				bool aCouleur = false;
				NkColor couleur{0, 0, 0, 255};

				/// Quelque chose a peindre de PLUS que le chemin d'avant ?
				bool Riche() const noexcept {
					return aDegrade || aImage || aOmbre || aBiseau;
				}
				bool Declaree() const noexcept {
					return Riche() || aOpacite || aEchelle || aTransition || !son.Empty() || !police.Empty() || aTaille ||
						   aContourTexte || aOmbreTexte;
				}
		};

		namespace detail {
			inline bool NkGHMot(NkStringView v, const char *lit) noexcept {
				usize i = 0;
				for (; lit[i] != '\0'; ++i) {
					if (i >= v.Size() || v.Data()[i] != lit[i])
						return false;
				}
				return i == v.Size();
			}
			inline const NkArchiveNode *NkGHCorps(const NkArchive &b) noexcept {
				const NkArchiveNode *c = b.FindNode(NkStringView(NkGuiArchive::KeyBody()));
				return (c && c->IsArray()) ? c : nullptr;
			}
			inline uint32 NkGHNombres(NkStringView lex, float32 *out, uint32 max) noexcept {
				uint32 n = 0;
				const char *p = lex.Data();
				const usize len = lex.Size();
				usize i = 0;
				while (i < len && n < max) {
					while (i < len && !((p[i] >= '0' && p[i] <= '9') || p[i] == '-' || p[i] == '.'))
						++i;
					if (i >= len)
						break;
					float32 signe = 1.f;
					if (p[i] == '-') {
						signe = -1.f;
						++i;
					}
					float32 v = 0.f;
					while (i < len && p[i] >= '0' && p[i] <= '9') {
						v = v * 10.f + static_cast<float32>(p[i] - '0');
						++i;
					}
					if (i < len && p[i] == '.') {
						++i;
						float32 f = 0.1f;
						while (i < len && p[i] >= '0' && p[i] <= '9') {
							v += f * static_cast<float32>(p[i] - '0');
							f *= 0.1f;
							++i;
						}
					}
					out[n++] = signe * v;
				}
				return n;
			}
			inline bool NkGHCouleur(const NkArchive &b, const char *cle, NkColor &out) noexcept {
				const NkArchiveNode *n = b.FindNode(NkStringView(cle));
				return n != nullptr && NkGuiCouleur(n->Lexeme(), out);
			}
			inline bool NkGHNombre(const NkArchive &b, const char *cle, float32 &out) noexcept {
				const NkArchiveNode *n = b.FindNode(NkStringView(cle));
				if (n == nullptr)
					return false;
				float32 v[1] = {0.f};
				if (NkGHNombres(n->Lexeme(), v, 1u) != 1u)
					return false;
				out = v[0];
				return true;
			}
			inline bool NkGHTexte(const NkArchive &b, const char *cle, NkString &out) noexcept {
				if (b.GetString(NkStringView(cle), out))
					return true;
				const NkArchiveNode *n = b.FindNode(NkStringView(cle));
				if (n == nullptr)
					return false;
				out = NkString(n->Lexeme());
				return true;
			}
			inline NkColor NkGHMelange(const NkColor &x, const NkColor &y, float32 t) noexcept {
				auto m = [t](uint8 a, uint8 b) -> uint8 {
					const float32 v = static_cast<float32>(a) + (static_cast<float32>(b) - static_cast<float32>(a)) * t;
					return static_cast<uint8>(v < 0.f ? 0.f : (v > 255.f ? 255.f : v + 0.5f));
				};
				return NkColor{m(x.r, y.r), m(x.g, y.g), m(x.b, y.b), m(x.a, y.a)};
			}
		} // namespace detail

		/// Lit, dans un bloc `appearance` (ou `appearance(Etat)`), ce que
		/// NkGuiSurface porte. Le reste (couleur, contour, encre, rayon) est lu
		/// ailleurs, par les lecteurs d'avant : rien n'est lu deux fois.
		inline void NkGuiLireSurface(const NkArchive &app, NkGuiSurface &s) noexcept {
			using namespace detail;
			float32 v = 0.f;
			if (NkGHNombre(app, "opacity", v)) {
				s.aOpacite = true;
				s.opacite = v > 1.f ? v / 100.f : v; // 0..1 ou 0..100
			}
			if (NkGHNombre(app, "scale", v)) {
				s.aEchelle = true;
				s.echelle = v;
			}
			if (NkGHNombre(app, "bevel", v)) {
				s.aBiseau = true;
				s.biseau = v;
			}
			if (NkGHNombre(app, "transition", v)) {
				s.aTransition = true;
				s.transition = v;
			}
			if (NkGHNombre(app, "size", v)) {
				s.aTaille = true;
				s.taille = v;
			}
			(void)NkGHTexte(app, "font", s.police);
			(void)NkGHTexte(app, "sound", s.son);
			const NkArchiveNode *c = NkGHCorps(app);
			if (c == nullptr)
				return;
			for (uint32 k = 0; k < static_cast<uint32>(c->array.Size()); ++k) {
				if (!c->array[k].IsObject() || !c->array[k].object)
					continue;
				const NkArchive &e = *c->array[k].object;
				const NkStringView t = NkGuiArchive::TypeOf(e);
				// ⚠️ « valeur », OU « <instance>.valeur » : dans un composant, le
				//    developpement PREFIXE les identifiants des blocs (le nom d'un
				//    effet compris) par celui de l'instance.
				const NkStringView idE = NkGuiArchive::IdOf(e);
				const bool nommeValeur = NkGHMot(idE, "valeur") ||
										 (idE.Size() > 7u && NkGHMot(NkStringView(idE.Data() + idE.Size() - 7u, 7u), ".valeur"));
				if (NkGHMot(t, "fill") && nommeValeur) {
					// La part REMPLIE d'une jauge : sa couleur, ou son degrade.
					s.aValeur = true;
					(void)NkGHCouleur(e, "color", s.valeurCouleur);
					if (NkGHCouleur(e, "from", s.valeurDe) && NkGHCouleur(e, "to", s.valeurA))
						s.aValeurDegrade = true;
				} else if (NkGHMot(t, "stroke")) {
					if (NkGHCouleur(e, "color", s.contour))
						s.aContour = true;
					(void)NkGHNombre(e, "width", s.contourLargeur);
				} else if (NkGHMot(t, "fill")) {
					if (NkGHCouleur(e, "color", s.couleur))
						s.aCouleur = true;
					NkColor de{0, 0, 0, 255}, a{0, 0, 0, 255};
					if (NkGHCouleur(e, "from", de) && NkGHCouleur(e, "to", a)) {
						s.aDegrade = true;
						s.de = de;
						s.a = a;
						if (NkGHNombre(e, "angle", v))
							s.angle = v;
					}
					NkString img;
					if (NkGHTexte(e, "image", img) && !img.Empty()) {
						s.aImage = true;
						s.image = img;
					}
					(void)NkGHTexte(e, "fit", s.cadrage);
					const NkArchiveNode *sl = e.FindNode(NkStringView("slice"));
					if (sl != nullptr) {
						float32 q[4] = {0.f, 0.f, 0.f, 0.f};
						const uint32 n = NkGHNombres(sl->Lexeme(), q, 4u);
						if (n == 1u)
							q[1] = q[2] = q[3] = q[0];
						if (n >= 1u) {
							s.aTranches = true;
							for (uint32 i = 0; i < 4u; ++i)
								s.tranches[i] = q[i];
						}
					}
				} else if (NkGHMot(t, "shadow")) {
					s.aOmbre = true;
					const NkArchiveNode *o = e.FindNode(NkStringView("offset"));
					if (o != nullptr) {
						float32 q[2] = {0.f, 0.f};
						if (NkGHNombres(o->Lexeme(), q, 2u) == 2u)
							s.ombreDecalage = NkVec2{q[0], q[1]};
					}
					(void)NkGHNombre(e, "blur", s.ombreFlou);
					(void)NkGHNombre(e, "spread", s.ombreEtendue);
					(void)NkGHCouleur(e, "color", s.ombreCouleur);
				} else if (NkGHMot(t, "text")) {
					if (NkGHCouleur(e, "outline", s.contourTexte))
						s.aContourTexte = true;
					if (NkGHCouleur(e, "shadow", s.ombreTexte))
						s.aOmbreTexte = true;
				}
			}
		}

		/// L'etat par-dessus le repos, champ par champ (la regle de
		/// NkGuiPeintureEffective : le repos est le SOCLE).
		inline NkGuiSurface NkGuiFusionnerSurface(const NkGuiSurface &repos, const NkGuiSurface &etat) noexcept {
			NkGuiSurface r = repos;
			if (etat.aDegrade) {
				r.aDegrade = true;
				r.de = etat.de;
				r.a = etat.a;
				r.angle = etat.angle;
			}
			if (etat.aImage) {
				r.aImage = true;
				r.image = etat.image;
			}
			if (!etat.cadrage.Empty())
				r.cadrage = etat.cadrage;
			if (etat.aTranches) {
				r.aTranches = true;
				for (uint32 i = 0; i < 4u; ++i)
					r.tranches[i] = etat.tranches[i];
			}
			if (etat.aOmbre) {
				r.aOmbre = true;
				r.ombreDecalage = etat.ombreDecalage;
				r.ombreFlou = etat.ombreFlou;
				r.ombreEtendue = etat.ombreEtendue;
				r.ombreCouleur = etat.ombreCouleur;
			}
			if (etat.aBiseau) {
				r.aBiseau = true;
				r.biseau = etat.biseau;
			}
			if (etat.aOpacite) {
				r.aOpacite = true;
				r.opacite = etat.opacite;
			}
			if (etat.aEchelle) {
				r.aEchelle = true;
				r.echelle = etat.echelle;
			}
			if (etat.aTransition) {
				r.aTransition = true;
				r.transition = etat.transition;
			}
			if (!etat.son.Empty())
				r.son = etat.son;
			if (!etat.police.Empty())
				r.police = etat.police;
			if (etat.aTaille) {
				r.aTaille = true;
				r.taille = etat.taille;
			}
			if (etat.aContourTexte) {
				r.aContourTexte = true;
				r.contourTexte = etat.contourTexte;
			}
			if (etat.aOmbreTexte) {
				r.aOmbreTexte = true;
				r.ombreTexte = etat.ombreTexte;
			}
			if (etat.aContour) {
				r.aContour = true;
				r.contour = etat.contour;
				r.contourLargeur = etat.contourLargeur;
			}
			if (etat.aCouleur) {
				r.aCouleur = true;
				r.couleur = etat.couleur;
			}
			if (etat.aValeur) {
				r.aValeur = true;
				r.valeurCouleur = etat.valeurCouleur;
				r.aValeurDegrade = etat.aValeurDegrade;
				r.valeurDe = etat.valeurDe;
				r.valeurA = etat.valeurA;
			}
			return r;
		}

		/// Le passage d'un etat a l'autre, a `t` (0 = `x`, 1 = `y`). Les couleurs
		/// et les nombres glissent ; ce qui ne se melange pas (une image, un son)
		/// bascule a mi-chemin.
		inline NkGuiSurface NkGuiInterpolerSurface(const NkGuiSurface &x, const NkGuiSurface &y, float32 t) noexcept {
			using namespace detail;
			if (t >= 1.f)
				return y;
			if (t <= 0.f)
				return x;
			NkGuiSurface r = t < 0.5f ? x : y;
			if (x.aDegrade && y.aDegrade) {
				r.de = NkGHMelange(x.de, y.de, t);
				r.a = NkGHMelange(x.a, y.a, t);
			}
			if (x.aOmbre || y.aOmbre) {
				NkColor cx = x.aOmbre ? x.ombreCouleur : NkColor{y.ombreCouleur.r, y.ombreCouleur.g, y.ombreCouleur.b, 0};
				NkColor cy = y.aOmbre ? y.ombreCouleur : NkColor{x.ombreCouleur.r, x.ombreCouleur.g, x.ombreCouleur.b, 0};
				r.aOmbre = true;
				r.ombreCouleur = NkGHMelange(cx, cy, t);
				const float32 fx = x.aOmbre ? x.ombreFlou : y.ombreFlou;
				const float32 fy = y.aOmbre ? y.ombreFlou : x.ombreFlou;
				r.ombreFlou = fx + (fy - fx) * t;
				const NkVec2 dx = x.aOmbre ? x.ombreDecalage : y.ombreDecalage;
				const NkVec2 dy = y.aOmbre ? y.ombreDecalage : x.ombreDecalage;
				r.ombreDecalage = NkVec2{dx.x + (dy.x - dx.x) * t, dx.y + (dy.y - dx.y) * t};
			}
			const float32 ex = x.aEchelle ? x.echelle : 1.f;
			const float32 ey = y.aEchelle ? y.echelle : 1.f;
			r.aEchelle = x.aEchelle || y.aEchelle;
			r.echelle = ex + (ey - ex) * t;
			const float32 ox = x.aOpacite ? x.opacite : 1.f;
			const float32 oy = y.aOpacite ? y.opacite : 1.f;
			r.aOpacite = x.aOpacite || y.aOpacite;
			r.opacite = ox + (oy - ox) * t;
			return r;
		}

		/// Le CONTOUR d'une forme, dans `pts` : un rectangle, ses coins arrondis
		/// (`rayon`) ou BISEAUTES (`biseau`, qui gagne). Rend le nombre de points.
		inline int32 NkGuiContourForme(const NkRect &r, float32 rayon, float32 biseau, NkVec2 *pts, int32 max) noexcept {
			const float32 m = (r.w < r.h ? r.w : r.h) * 0.5f;
			if (biseau > 0.f && max >= 8) {
				const float32 b = biseau < m ? biseau : m;
				pts[0] = NkVec2{r.x + b, r.y};
				pts[1] = NkVec2{r.x + r.w - b, r.y};
				pts[2] = NkVec2{r.x + r.w, r.y + b};
				pts[3] = NkVec2{r.x + r.w, r.y + r.h - b};
				pts[4] = NkVec2{r.x + r.w - b, r.y + r.h};
				pts[5] = NkVec2{r.x + b, r.y + r.h};
				pts[6] = NkVec2{r.x, r.y + r.h - b};
				pts[7] = NkVec2{r.x, r.y + b};
				return 8;
			}
			if (rayon <= 0.5f || max < 28) {
				pts[0] = NkVec2{r.x, r.y};
				pts[1] = NkVec2{r.x + r.w, r.y};
				pts[2] = NkVec2{r.x + r.w, r.y + r.h};
				pts[3] = NkVec2{r.x, r.y + r.h};
				return 4;
			}
			const float32 R = rayon < m ? rayon : m;
			const NkVec2 centres[4] = {{r.x + r.w - R, r.y + R},
									   {r.x + r.w - R, r.y + r.h - R},
									   {r.x + R, r.y + r.h - R},
									   {r.x + R, r.y + R}};
			const float32 depart[4] = {-90.f, 0.f, 90.f, 180.f};
			int32 n = 0;
			for (int32 c = 0; c < 4; ++c) {
				for (int32 k = 0; k <= 6 && n < max; ++k) {
					const float32 a = (depart[c] + 15.f * static_cast<float32>(k)) * 0.0174532925f;
					pts[n++] = NkVec2{centres[c].x + R * math::NkCos(a), centres[c].y + R * math::NkSin(a)};
				}
			}
			return n;
		}

		/// Remplit le contour : couleur unie ou DEGRADE (une couleur par sommet,
		/// projetee sur l'axe de `angle`), en eventail depuis le centre.
		inline void NkGuiRemplirContour(NkGuiDrawList &dl, const NkRect &r, const NkVec2 *pts, int32 n, const NkColor &unie,
										const NkGuiSurface *degrade) noexcept {
			if (n < 3)
				return;
			NkVec2 sommets[40];
			NkColor couleurs[40];
			uint32 indices[120];
			const int32 m = n < 39 ? n : 39;
			const NkVec2 c{r.x + r.w * 0.5f, r.y + r.h * 0.5f};
			const float32 a = (degrade != nullptr ? degrade->angle : 90.f) * 0.0174532925f;
			const NkVec2 d{math::NkCos(a), math::NkSin(a)};
			const float32 ext = (r.w * (d.x < 0.f ? -d.x : d.x) + r.h * (d.y < 0.f ? -d.y : d.y));
			auto Teinte = [&](const NkVec2 &p) -> NkColor {
				if (degrade == nullptr || !degrade->aDegrade || ext <= 0.f)
					return unie;
				float32 t = ((p.x - c.x) * d.x + (p.y - c.y) * d.y) / ext + 0.5f;
				t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
				return detail::NkGHMelange(degrade->de, degrade->a, t);
			};
			sommets[0] = c;
			couleurs[0] = Teinte(c);
			for (int32 i = 0; i < m; ++i) {
				sommets[i + 1] = pts[i];
				couleurs[i + 1] = Teinte(pts[i]);
			}
			int32 k = 0;
			for (int32 i = 0; i < m; ++i) {
				indices[k++] = 0u;
				indices[k++] = static_cast<uint32>(i + 1);
				indices[k++] = static_cast<uint32>((i + 1) % m + 1);
			}
			dl.AddMesh(0u, sommets, nullptr, couleurs, m + 1, indices, k);
		}

		/// L'image d'un remplissage : etiree, aux proportions (Fit), ou en NEUF
		/// TRANCHES (Slice : les coins gardent leur taille, les bords s'etirent
		/// dans un sens, le centre dans les deux -- le cadre peint d'un jeu).
		/// Rend faux si l'image ne se resout pas (le registre le compte).
		inline bool NkGuiPeindreImage(NkGuiDrawList &dl, const NkRect &r, const NkGuiSurface &s, const NkColor &teinte) noexcept {
			uint32 tex = 0;
			NkVec2 taille{0.f, 0.f};
			if (!s.aImage || !NkGuiResoudreImage(s.image.CStr(), tex, taille) || taille.x <= 0.f || taille.y <= 0.f)
				return false;
			const bool tranches = s.aTranches && (s.cadrage.Empty() || detail::NkGHMot(NkStringView(s.cadrage.CStr()), "Slice"));
			if (!tranches) {
				if (detail::NkGHMot(NkStringView(s.cadrage.CStr()), "Fit")) {
					const float32 f = (r.w / taille.x < r.h / taille.y) ? r.w / taille.x : r.h / taille.y;
					const NkRect q{r.x + (r.w - taille.x * f) * 0.5f, r.y + (r.h - taille.y * f) * 0.5f, taille.x * f, taille.y * f};
					dl.AddImage(tex, q, NkVec2{0.f, 0.f}, NkVec2{1.f, 1.f}, teinte);
				} else {
					dl.AddImage(tex, r, NkVec2{0.f, 0.f}, NkVec2{1.f, 1.f}, teinte);
				}
				return true;
			}
			// Les quatre bords, en pixels de l'image ET a l'ecran (bornes : un
			// widget plus petit que ses coins les reduit en proportion).
			float32 g = s.tranches[0], h = s.tranches[1], d = s.tranches[2], b = s.tranches[3];
			const float32 kx = (g + d) > r.w && (g + d) > 0.f ? r.w / (g + d) : 1.f;
			const float32 ky = (h + b) > r.h && (h + b) > 0.f ? r.h / (h + b) : 1.f;
			const float32 xs[4] = {r.x, r.x + g * kx, r.x + r.w - d * kx, r.x + r.w};
			const float32 ys[4] = {r.y, r.y + h * ky, r.y + r.h - b * ky, r.y + r.h};
			const float32 us[4] = {0.f, g / taille.x, 1.f - d / taille.x, 1.f};
			const float32 vs[4] = {0.f, h / taille.y, 1.f - b / taille.y, 1.f};
			for (int32 j = 0; j < 3; ++j) {
				for (int32 i = 0; i < 3; ++i) {
					const NkRect q{xs[i], ys[j], xs[i + 1] - xs[i], ys[j + 1] - ys[j]};
					if (q.w <= 0.f || q.h <= 0.f)
						continue;
					dl.AddImage(tex, q, NkVec2{us[i], vs[j]}, NkVec2{us[i + 1], vs[j + 1]}, teinte);
				}
			}
			return true;
		}

		/// L'OMBRE (ou la LUEUR, decalage nul) : des couches elargies dont
		/// l'opacite decroit -- un flou sans flou, que le rasteriseur logiciel et
		/// tous les dorsaux peignent de la meme facon.
		inline void NkGuiPeindreOmbre(NkGuiDrawList &dl, const NkRect &r, float32 rayon, float32 biseau,
									  const NkGuiSurface &s) noexcept {
			if (!s.aOmbre || s.ombreCouleur.a == 0u)
				return;
			const int32 couches = s.ombreFlou <= 1.f ? 1 : (s.ombreFlou > 16.f ? 8 : static_cast<int32>(s.ombreFlou * 0.5f) + 1);
			NkVec2 pts[40];
			for (int32 i = couches - 1; i >= 0; --i) {
				const float32 f = couches == 1 ? 0.f : static_cast<float32>(i) / static_cast<float32>(couches - 1);
				const float32 e = s.ombreEtendue + s.ombreFlou * f;
				const NkRect q{r.x + s.ombreDecalage.x - e, r.y + s.ombreDecalage.y - e, r.w + 2.f * e, r.h + 2.f * e};
				const float32 alpha = static_cast<float32>(s.ombreCouleur.a) * (1.f - f) * (1.f - f) * 1.6f /
									  static_cast<float32>(couches);
				const NkColor col{s.ombreCouleur.r, s.ombreCouleur.g, s.ombreCouleur.b,
								  static_cast<uint8>(alpha > 255.f ? 255.f : alpha)};
				const int32 n = NkGuiContourForme(q, rayon > 0.f ? rayon + e : 0.f, biseau > 0.f ? biseau + e * 0.4f : 0.f, pts, 40);
				NkGuiRemplirContour(dl, q, pts, n, col, nullptr);
			}
		}

		/// LA SURFACE COMPLETE d'un widget : l'ombre, le fond (couleur, degrade,
		/// image), le contour. `fond` / `contour` : ce que le chemin d'avant aurait
		/// peint (couleur du document ou du theme).
		inline void NkGuiPeindreSurface(NkGuiDrawList &dl, const NkRect &r, const NkGuiSurface &s, const NkColor &fond,
										bool peindreFond, const NkColor &contour, float32 contourLargeur, bool peindreContour,
										float32 rayon) noexcept {
			const float32 biseau = s.aBiseau ? s.biseau : 0.f;
			NkGuiPeindreOmbre(dl, r, rayon, biseau, s);
			NkVec2 pts[40];
			const int32 n = NkGuiContourForme(r, rayon, biseau, pts, 40);
			if (s.aImage) {
				if (!NkGuiPeindreImage(dl, r, s, NkColor{255, 255, 255, 255}) && peindreFond)
					NkGuiRemplirContour(dl, r, pts, n, fond, &s);
			} else if (s.aDegrade || peindreFond) {
				NkGuiRemplirContour(dl, r, pts, n, fond, &s);
			}
			if (peindreContour && contourLargeur > 0.f && contour.a > 0u)
				dl.AddPolyline(pts, n, contour, contourLargeur, true);
		}

		/// L'opacite et l'echelle d'un etat, appliquees APRES COUP aux sommets que
		/// le widget vient d'ecrire (`depuis` .. fin) : l'echelle autour du centre
		/// de `r`, l'alpha multiplie. Les decoupes suivent l'echelle.
		inline void NkGuiTransformerSommets(NkGuiDrawList &dl, uint32 depuis, uint32 cmdDepuis, const NkRect &r, float32 echelle,
											float32 opacite, const NkVec2 &decalage) noexcept {
			const NkVec2 c{r.x + r.w * 0.5f, r.y + r.h * 0.5f};
			const bool bouge = echelle != 1.f || decalage.x != 0.f || decalage.y != 0.f;
			for (uint32 i = depuis; i < static_cast<uint32>(dl.vtx.Size()); ++i) {
				NkGuiVertex &v = dl.vtx[i];
				if (bouge) {
					v.pos.x = c.x + (v.pos.x - c.x) * echelle + decalage.x;
					v.pos.y = c.y + (v.pos.y - c.y) * echelle + decalage.y;
				}
				if (opacite < 1.f) {
					const uint32 a = (v.col >> 24) & 0xFFu;
					const float32 na = static_cast<float32>(a) * (opacite < 0.f ? 0.f : opacite);
					v.col = (v.col & 0x00FFFFFFu) | (static_cast<uint32>(na + 0.5f) << 24);
				}
			}
			if (bouge && echelle > 1.f) {
				// L'agrandi ne doit pas etre coupe par la decoupe d'avant.
				for (uint32 k = cmdDepuis; k < static_cast<uint32>(dl.cmds.Size()); ++k) {
					NkRect &q = dl.cmds[k].clipRect;
					if (q.w >= 1.0e8f)
						continue;
					const float32 gx = q.w * (echelle - 1.f) * 0.5f + 2.f;
					const float32 gy = q.h * (echelle - 1.f) * 0.5f + 2.f;
					q = NkRect{q.x - gx + decalage.x, q.y - gy + decalage.y, q.w + 2.f * gx, q.h + 2.f * gy};
				}
			}
		}

		/// Un texte HABILLE : son ombre, son contour (huit passages a un pixel),
		/// puis lui. `police` : la face a employer (celle du contexte sinon).
		template <typename NkGuiFontT>
		inline void NkGuiTexteHabille(NkGuiDrawList &dl, const NkGuiFontT *police, const NkVec2 &base, const char *texte,
									  const char *fin, const NkColor &encre, const NkGuiSurface &s, float32 largeurMax) noexcept {
			if (police == nullptr || !police->Valid() || texte == nullptr)
				return;
			if (s.aOmbreTexte)
				dl.AddText(police->Face(), police->TexId(), NkVec2{base.x + 2.f, base.y + 2.f}, texte, s.ombreTexte, largeurMax, 0.f, fin);
			if (s.aContourTexte) {
				static const float32 kD[8][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
				for (int32 i = 0; i < 8; ++i)
					dl.AddText(police->Face(), police->TexId(), NkVec2{base.x + kD[i][0], base.y + kD[i][1]}, texte, s.contourTexte,
							   largeurMax, 0.f, fin);
			}
			dl.AddText(police->Face(), police->TexId(), base, texte, encre, largeurMax, 0.f, fin);
		}


		// =====================================================================
		//  LES POLICES DU DOCUMENT (section `fonts`, document 2 §17)
		// =====================================================================
		//  fonts {
		//    font "titre" { source { path = "Polices/Titre.ttf" } }   un FICHIER, relatif au document
		//    font "corps" { source { embedded = "Karla" } }            une police EMBARQUEE de NKFont
		//  }
		//  appearance { font = "titre"  size = 34 }
		//
		//  ⚠️ L'HOTE CHARGE, NKGui RETIENT. Une police doit etre TELEVERSEE au
		//     dorsal (son atlas) : seul l'hote sait le faire. Il pose un chargeur ;
		//     le registre garde chaque (source, taille) une fois.
		struct NkGuiPoliceDoc {
				NkString id;
				NkString chemin;	 ///< absolu (resolu depuis le dossier du document)
				NkString embarquee;  ///< « DroidSerif », « Karla »... (NKFont)
		};

		/// Charge une police : `chemin` (fichier) ou `embarquee` (nom NKFont), a
		/// `taille` pixels. Rend nul si elle ne se charge pas. L'hote la televerse.
		using NkGuiChargeurPolice = NkGuiFont *(*)(const char *chemin, const char *embarquee, float32 taille, void *user);

		namespace detail {
			struct NkGHPolice {
					NkString chemin, embarquee;
					float32 taille = 0.f;
					NkGuiFont *police = nullptr;
			};
			struct NkGHPolices {
					NkGuiChargeurPolice chargeur = nullptr;
					void *user = nullptr;
					NkVector<NkGHPolice> chargees;
					uint32 refusees = 0;
			};
			inline NkGHPolices &NkGHRegistrePolices() noexcept {
				static NkGHPolices r;
				return r;
			}
		} // namespace detail

		inline void NkGuiPoserChargeurPolice(NkGuiChargeurPolice fn, void *user) noexcept {
			detail::NkGHRegistrePolices().chargeur = fn;
			detail::NkGHRegistrePolices().user = user;
		}

		/// La police (source, taille), chargee une fois. Nul : pas de chargeur,
		/// ou elle ne se charge pas (compte dans NkGuiPolicesRefusees).
		inline NkGuiFont *NkGuiPoliceDuDocument(const char *chemin, const char *embarquee, float32 taille) noexcept {
			detail::NkGHPolices &r = detail::NkGHRegistrePolices();
			const NkString c(chemin != nullptr ? chemin : "");
			const NkString e(embarquee != nullptr ? embarquee : "");
			const float32 t = taille > 0.f ? taille : 16.f;
			for (uint32 i = 0; i < static_cast<uint32>(r.chargees.Size()); ++i) {
				const detail::NkGHPolice &p = r.chargees[i];
				if (p.chemin == c && p.embarquee == e && p.taille == t)
					return p.police;
			}
			if (r.chargeur == nullptr)
				return nullptr;
			detail::NkGHPolice p;
			p.chemin = c;
			p.embarquee = e;
			p.taille = t;
			p.police = r.chargeur(c.CStr(), e.CStr(), t, r.user);
			if (p.police == nullptr)
				++r.refusees;
			r.chargees.PushBack(p); // un echec aussi : on ne retente pas a chaque image
			return p.police;
		}

		inline uint32 NkGuiNbPolicesDuDocument() noexcept {
			return static_cast<uint32>(detail::NkGHRegistrePolices().chargees.Size());
		}
		/// La i-eme police chargee (nul pour un echec) : l'hote televerse, un
		/// rasteriseur de banc pose sa texture.
		inline NkGuiFont *NkGuiPoliceDuDocumentA(uint32 i) noexcept {
			detail::NkGHPolices &r = detail::NkGHRegistrePolices();
			return i < r.chargees.Size() ? r.chargees[i].police : nullptr;
		}
		inline uint32 NkGuiPolicesRefusees() noexcept {
			return detail::NkGHRegistrePolices().refusees;
		}
		/// Oublie le registre (l'hote a libere ses polices, le dorsal a change).
		inline void NkGuiOublierPolicesDuDocument() noexcept {
			detail::NkGHRegistrePolices().chargees.Clear();
			detail::NkGHRegistrePolices().refusees = 0;
		}

		/// Le dossier d'un chemin, barre finale comprise (« » s'il n'en a pas).
		inline NkString NkGuiDossierDe(const char *chemin) noexcept {
			NkString c(chemin != nullptr ? chemin : "");
			usize fin = 0;
			for (usize i = 0; i < c.Size(); ++i)
				if (c.CStr()[i] == '/' || c.CStr()[i] == '\\')
					fin = i + 1;
			return NkString(c.SubStr(0, fin));
		}

		/// Lit la section `fonts` du document. `dossier` : celui du document (les
		/// chemins y sont relatifs).
		inline void NkGuiLirePolices(const NkArchive &doc, const char *dossier, NkVector<NkGuiPoliceDoc> &out) noexcept {
			out.Clear();
			const NkArchiveNode *corps = detail::NkGHCorps(doc);
			if (corps == nullptr)
				return;
			for (uint32 i = 0; i < static_cast<uint32>(corps->array.Size()); ++i) {
				if (!corps->array[i].IsObject() || !corps->array[i].object)
					continue;
				const NkArchive &sec = *corps->array[i].object;
				if (!detail::NkGHMot(NkGuiArchive::TypeOf(sec), "fonts"))
					continue;
				const NkArchiveNode *c = detail::NkGHCorps(sec);
				for (uint32 k = 0; c != nullptr && k < static_cast<uint32>(c->array.Size()); ++k) {
					if (!c->array[k].IsObject() || !c->array[k].object)
						continue;
					const NkArchive &f = *c->array[k].object;
					if (!detail::NkGHMot(NkGuiArchive::TypeOf(f), "font"))
						continue;
					NkGuiPoliceDoc p;
					p.id = NkString(NkGuiArchive::IdOf(f));
					const NkArchiveNode *cs = detail::NkGHCorps(f);
					for (uint32 j = 0; cs != nullptr && j < static_cast<uint32>(cs->array.Size()); ++j) {
						if (!cs->array[j].IsObject() || !cs->array[j].object)
							continue;
						const NkArchive &src = *cs->array[j].object;
						if (!detail::NkGHMot(NkGuiArchive::TypeOf(src), "source"))
							continue;
						NkString chemin;
						if (detail::NkGHTexte(src, "path", chemin) && !chemin.Empty()) {
							const bool absolu = chemin.CStr()[0] == '/' || (chemin.Size() > 1u && chemin.CStr()[1] == ':');
							p.chemin = absolu ? chemin : NkString(dossier != nullptr ? dossier : "") + chemin;
						}
						(void)detail::NkGHTexte(src, "embedded", p.embarquee);
					}
					if (!p.id.Empty())
						out.PushBack(p);
				}
			}
		}

		// =====================================================================
		//  LES AMBIANCES (section `animation`, document 2 §16) -- un sous-ensemble
		// =====================================================================
		//  animation "titre" {
		//    ambience "lanterne" {
		//      target = "titre.lanterne"  state = Normal  duration = 1.6
		//      repeat = 0  direction = alternate  delay = 0.2
		//      track "opacity" { from = 0.7  to = 1.0 }
		//      track "scale"   { from = 1.0  to = 1.05 }
		//      track "offset.y" { from = 0  to = -4 }
		//    }
		//  }
		//  ⚠️ JOUEES : `ambience`, avec `state = Normal` (toujours) ou `Hover`, et
		//     les pistes `opacity`, `scale`, `offset.x`, `offset.y` a deux cles
		//     (`from` / `to`). `transition` et `continuous` restent LUES et
		//     comptees, comme avant -- l'etat d'un widget glisse deja par
		//     `appearance { transition = ... }`.
		struct NkGuiPisteAmbiance {
				uint8 propriete = 0; ///< 0 opacity, 1 scale, 2 offset.x, 3 offset.y
				float32 de = 0.f, a = 1.f;
		};
		struct NkGuiAmbiance {
				NkString cible;
				bool surSurvol = false;
				float32 duree = 1.f;
				float32 delai = 0.f;
				int32 repetitions = 0; ///< 0 = sans fin
				bool alterne = true;
				NkGuiPisteAmbiance pistes[4];
				uint32 nbPistes = 0;
		};

		inline void NkGuiLireAmbiances(const NkArchive &doc, NkVector<NkGuiAmbiance> &out, uint32 *ignorees = nullptr) noexcept {
			out.Clear();
			const NkArchiveNode *corps = detail::NkGHCorps(doc);
			if (corps == nullptr)
				return;
			for (uint32 i = 0; i < static_cast<uint32>(corps->array.Size()); ++i) {
				if (!corps->array[i].IsObject() || !corps->array[i].object)
					continue;
				const NkArchive &sec = *corps->array[i].object;
				if (!detail::NkGHMot(NkGuiArchive::TypeOf(sec), "animation"))
					continue;
				const NkArchiveNode *c = detail::NkGHCorps(sec);
				for (uint32 k = 0; c != nullptr && k < static_cast<uint32>(c->array.Size()); ++k) {
					if (!c->array[k].IsObject() || !c->array[k].object)
						continue;
					const NkArchive &a = *c->array[k].object;
					if (!detail::NkGHMot(NkGuiArchive::TypeOf(a), "ambience")) {
						if (ignorees != nullptr)
							++*ignorees;
						continue;
					}
					NkGuiAmbiance am;
					(void)detail::NkGHTexte(a, "target", am.cible);
					NkString etat;
					(void)detail::NkGHTexte(a, "state", etat);
					am.surSurvol = detail::NkGHMot(NkStringView(etat.CStr()), "Hover");
					(void)detail::NkGHNombre(a, "duration", am.duree);
					(void)detail::NkGHNombre(a, "delay", am.delai);
					float32 rep = 0.f;
					if (detail::NkGHNombre(a, "repeat", rep))
						am.repetitions = static_cast<int32>(rep);
					NkString dir;
					if (detail::NkGHTexte(a, "direction", dir))
						am.alterne = detail::NkGHMot(NkStringView(dir.CStr()), "alternate");
					const NkArchiveNode *ps = detail::NkGHCorps(a);
					for (uint32 j = 0; ps != nullptr && j < static_cast<uint32>(ps->array.Size()) && am.nbPistes < 4u; ++j) {
						if (!ps->array[j].IsObject() || !ps->array[j].object)
							continue;
						const NkArchive &t = *ps->array[j].object;
						if (!detail::NkGHMot(NkGuiArchive::TypeOf(t), "track"))
							continue;
						const NkStringView nom = NkGuiArchive::IdOf(t);
						NkGuiPisteAmbiance p;
						if (detail::NkGHMot(nom, "opacity"))
							p.propriete = 0;
						else if (detail::NkGHMot(nom, "scale"))
							p.propriete = 1;
						else if (detail::NkGHMot(nom, "offset.x"))
							p.propriete = 2;
						else if (detail::NkGHMot(nom, "offset.y"))
							p.propriete = 3;
						else
							continue;
						if (!detail::NkGHNombre(t, "from", p.de) || !detail::NkGHNombre(t, "to", p.a))
							continue;
						am.pistes[am.nbPistes++] = p;
					}
					if (!am.cible.Empty() && am.nbPistes > 0u && am.duree > 0.f)
						out.PushBack(am);
				}
			}
		}

		/// La valeur d'une ambiance a l'instant `temps` (secondes) : 0..1 le long
		/// de ses cles, adoucie (entree et sortie lentes), aller-retour si alterne.
		inline float32 NkGuiAvancementAmbiance(const NkGuiAmbiance &a, float32 temps) noexcept {
			float32 t = temps - a.delai;
			if (t <= 0.f)
				return 0.f;
			const float32 cycles = t / a.duree;
			if (a.repetitions > 0 && cycles >= static_cast<float32>(a.repetitions))
				return a.alterne && (a.repetitions % 2) == 0 ? 0.f : 1.f;
			float32 f = cycles - math::NkFloor(cycles);
			if (a.alterne && (static_cast<int32>(math::NkFloor(cycles)) % 2) == 1)
				f = 1.f - f;
			return f * f * (3.f - 2.f * f);
		}
	} // namespace nkgui
} // namespace nkentseu
