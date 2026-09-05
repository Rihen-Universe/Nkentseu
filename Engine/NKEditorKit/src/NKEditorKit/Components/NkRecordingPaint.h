#pragma once
// -----------------------------------------------------------------------------
// @File    NkRecordingPaint.h
// @Brief   Un peintre qui n'affiche rien et ENREGISTRE tout — le banc headless.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  A QUOI IL SERT, ET A QUOI IL NE SERT PAS — a lire avant de conclure quoi que
//  ce soit d'une de ses sorties.
// =============================================================================
//  IL REPOND A UNE SEULE QUESTION : *les commandes de dessin produites
//  changent-elles quand la declaration change ?* C'est le temoin de la tranche
//  verticale du 18/08 — « changer un parametre dans NkUIDesign change le rendu
//  sans recompiler » — dans sa forme NON VISUELLE, la seule disponible pendant
//  que le GPU est occupe.
//
//  ⚠️ IL NE REPOND PAS A « EST-CE QUE C'EST BEAU », NI MEME A « EST-CE QUE C'EST
//     JUSTE ». Deux flux de commandes peuvent differer comme prevu et donner un
//     rendu faux ; ils peuvent etre identiques et l'ecran etre bon. Ce banc
//     mesure la CHAINE (declaration -> fichier -> dessin), pas le pixel. La
//     conformite aux planches se mesure a l'ecran, et cette mesure-la est
//     DIFFEREE et nommee comme telle dans `ROADMAP.md`.
//
//  ⚠️ ET IL NE MESURE PAS UNE RECONSTRUCTION. C'est le piege n.4 de la grille du
//     corpus : un banc monte sur les memes primitives les recable correctement
//     par construction et masque le defaut cherche. Ici, ce qui est exerce est
//     `NkDrawContentBrowser` LUI-MEME, la fonction que l'application appellera —
//     pas une reecriture du dessin dans le banc. Seul le PEINTRE est remplace,
//     et le peintre est precisement ce qu'on ne cherche pas a mesurer.
//
//  LE TEMOIN OBLIGATOIRE, et il est dans la sonde, pas ici : la meme mesure
//  repetee SANS RIEN CHANGER. Sans lui, on ne saurait pas si une difference
//  observee est l'effet de l'ecrasement ou du bruit (piege n.6). Ce peintre est
//  entierement deterministe — aucune horloge, aucun aleatoire, aucune ressource
//  externe — donc le plancher de bruit attendu est EXACTEMENT zero, et la sonde
//  le verifie au lieu de le supposer.
//
// LES METRIQUES DE TEXTE SONT FICTIVES ET LE DISENT : `LineHeight` et
//   `TextWidth` rendent des valeurs fixes. C'est volontaire — charger une police
//   ferait dependre le banc d'un fichier et d'un rasteriseur, donc de tout ce
//   qu'il essaie d'eviter. Consequence a assumer : ce banc ne peut RIEN dire de
//   ce qui depend de la largeur reelle d'un texte (troncature, ellipse,
//   centrage). Ces trois-la se verront a l'ecran, pas ici.
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKEditorKit/Components/NkComponentPaint.h"

namespace nkentseu {
	namespace editorkit {

		enum class NkPaintOp : uint8 {
			Fill = 0,
			FillColor,
			Outline,
			OutlineSharp,
			HLine,
			VLine,
			Text,
			Icon,
			PushClip,
			PopClip,
			// Ajoutees le 2026-08-30 (chaine du designer) — EN FIN, avant Count :
			// l'enumeration est append-only comme toutes celles de la forme.
			Ellipse,
			Line,
			// 2026-09-03 : la transformee du peintre. EN FIN, avant Count.
			PushTransform,
			PopTransform,
			// 2026-09-04 : le mode de melange du peintre. EN FIN, avant Count.
			PushBlend,
			PopBlend,
			// 2026-09-05 : l'image du peintre (polygone texture). EN FIN, avant Count.
			Image,
			Count
		};

		inline const char *NkPaintOpName(NkPaintOp o) {
			switch (o) {
				case NkPaintOp::Fill:
					return "Fill";
				case NkPaintOp::FillColor:
					return "FillColor";
				case NkPaintOp::Outline:
					return "Outline";
				case NkPaintOp::OutlineSharp:
					return "OutlineSharp";
				case NkPaintOp::HLine:
					return "HLine";
				case NkPaintOp::VLine:
					return "VLine";
				case NkPaintOp::Text:
					return "Text";
				case NkPaintOp::Icon:
					return "Icon";
				case NkPaintOp::PushClip:
					return "PushClip";
				case NkPaintOp::PopClip:
					return "PopClip";
				case NkPaintOp::Ellipse:
					return "Ellipse";
				case NkPaintOp::Line:
					return "Line";
				case NkPaintOp::PushTransform:
					return "PushTransform";
				case NkPaintOp::PopTransform:
					return "PopTransform";
				case NkPaintOp::PushBlend:
					return "PushBlend";
				case NkPaintOp::PopBlend:
					return "PopBlend";
				case NkPaintOp::Image:
					return "Image";
				default:
					return "?";
			}
		}

		struct NkPaintCmd {
				NkPaintOp op = NkPaintOp::Fill;
				float32 x = 0.f, y = 0.f, w = 0.f, h = 0.f;
				uint16 role = 0, role2 = 0;
				uint32 rgba = 0;
				float32 rounding = 0.f;
				/// Pour PushTransform : a,b,c,d dans x,y,w,h ; e dans rounding ; f ICI.
				float32 tf = 0.f;
				uint16 icon = 0;
				uint8 align = 0;
				NkString text;
				/// Pour Image : le handle, et les sommets / uv (count paires) -- x,y,w,h
				/// portent l'englobant, rgba l'opacite (0..255) dans l'octet bas.
				uint32 image = 0;
				NkVector<float32> xy, uv;

				/// Egalite STRUCTURELLE, geometrie comprise. C'est ce qui rend la
				/// comparaison de deux flux significative : si seule l'operation
				/// etait comparee, deplacer une carte de 40 px passerait pour
				/// « aucun changement ».
				bool SameAs(const NkPaintCmd &o) const {
					if (op != o.op || role != o.role || role2 != o.role2 || rgba != o.rgba ||
						icon != o.icon || align != o.align || image != o.image || xy.Size() != o.xy.Size())
						return false;
					if (!Near(x, o.x) || !Near(y, o.y) || !Near(w, o.w) || !Near(h, o.h) ||
						!Near(rounding, o.rounding) || !Near(tf, o.tf))
						return false;
					const char *a = text.Data(), *b = o.text.Data();
					if (!a || !b)
						return a == b;
					for (; *a && *b; ++a, ++b)
						if (*a != *b)
							return false;
					return *a == *b;
				}

			private:
				static bool Near(float32 a, float32 b) {
					const float32 d = a - b;
					return (d < 0.001f) && (d > -0.001f);
				}
		};

		// ── LE PEINTRE ENREGISTREUR ─────────────────────────────────────────────
		class NkRecordingPaint : public NkComponentPaint {
			public:
				NkVector<NkPaintCmd> cmds;

				void Reset() {
					cmds.Clear();
					mClipDepth = 0;
					mMaxClipDepth = 0;
					mUnbalanced = false;
				}

				// ── Theme fictif, mais DETERMINISTE ET INJECTIF ──────────────────
				// Une couleur par role, distincte pour chaque role. L'injectivite
				// compte : si deux roles rendaient la meme couleur, une erreur de
				// role serait invisible dans le flux enregistre.
				uint32 ColorOf(uint16 role) const override {
					return 0xFF000000u | ((uint32)role * 0x00010307u);
				}
				// Metriques de POLICE, fixes et sans echelle : l'echelle est
				// celle de la SURFACE et vit dans `NkComponentInput` (arbitrage du
				// 18/08). Un peintre qui la remultiplierait ici l'appliquerait deux
				// fois.
				float32 LineHeight() const override {
					return 16.f;
				}
				float32 TextWidth(const char *s) const override {
					uint32 n = 0;
					if (s)
						while (s[n])
							++n;
					return (float32)n * 7.f;
				}

				// ── Primitives ──────────────────────────────────────────────────
				void Fill(const NkPaintRect &r, uint16 role, float32 rounding) override {
					Push(NkPaintOp::Fill, r, role, 0, 0, rounding, 0, 0, nullptr);
				}
				void FillColor(const NkPaintRect &r, uint32 rgba, float32 rounding) override {
					Push(NkPaintOp::FillColor, r, 0, 0, rgba, rounding, 0, 0, nullptr);
				}
				void Outline(const NkPaintRect &r, uint16 border, uint16 inner,
							 float32 rounding) override {
					Push(NkPaintOp::Outline, r, border, inner, 0, rounding, 0, 0, nullptr);
				}
				void OutlineSharp(const NkPaintRect &r, uint16 role) override {
					Push(NkPaintOp::OutlineSharp, r, role, 0, 0, 0.f, 0, 0, nullptr);
				}
				void HLine(float32 x, float32 y, float32 w, uint16 role) override {
					Push(NkPaintOp::HLine, {x, y, w, 1.f}, role, 0, 0, 0.f, 0, 0, nullptr);
				}
				void VLine(float32 x, float32 y, float32 h, uint16 role) override {
					Push(NkPaintOp::VLine, {x, y, 1.f, h}, role, 0, 0, 0.f, 0, 0, nullptr);
				}
				void Text(const NkPaintRect &r, const char *s, uint16 role, NkTextAlign a) override {
					Push(NkPaintOp::Text, r, role, 0, 0, 0.f, 0, (uint8)a, s);
				}
				void Icon(const NkPaintRect &r, uint16 iconHandle, uint16 role) override {
					Push(NkPaintOp::Icon, r, role, 0, 0, 0.f, iconHandle, 0, nullptr);
				}
				bool Ellipse(const NkPaintRect &r, uint16 role) override {
					Push(NkPaintOp::Ellipse, r, role, 0, 0, 0.f, 0, 0, nullptr);
					return true;
				}
				bool Line(float32 x1, float32 y1, float32 x2, float32 y2, uint16 role,
						  float32 thickness) override {
					// Le rect porte les DEUX EXTREMITES (x,y)-(w,h relatifs) : la
					// geometrie complete du segment, pas seulement sa boite.
					Push(NkPaintOp::Line, {x1, y1, x2 - x1, y2 - y1}, role, 0, 0, thickness, 0, 0,
						 nullptr);
					return true;
				}
				/// LA TRANSFORMEE S'ENREGISTRE -- c'est ce qui rend le texte tourne
				/// PROUVABLE sans ecran : le banc lit `PushTransform` avec ses six
				/// coefficients, puis les commandes qu'elle couvre.
				void PushTransform(const NkPaintTransform &t) override {
					Push(NkPaintOp::PushTransform, {t.a, t.b, t.c, t.d}, 0, 0, 0, t.e, 0, 0, nullptr);
					if (!cmds.Empty())
						cmds[cmds.Size() - 1].tf = t.f;
				}
				bool ImagePolygone(const float32 *xy, const float32 *uv, int32 count, uint32 image,
								   float32 opacite) override {
					if (!xy || !uv || count < 3)
						return false;
					float32 x0 = xy[0], y0 = xy[1], x1 = xy[0], y1 = xy[1];
					for (int32 i = 1; i < count; ++i) {
						if (xy[i * 2] < x0) x0 = xy[i * 2];
						if (xy[i * 2] > x1) x1 = xy[i * 2];
						if (xy[i * 2 + 1] < y0) y0 = xy[i * 2 + 1];
						if (xy[i * 2 + 1] > y1) y1 = xy[i * 2 + 1];
					}
					const float32 k = opacite < 0.f ? 0.f : (opacite > 100.f ? 1.f : opacite * 0.01f);
					Push(NkPaintOp::Image, {x0, y0, x1 - x0, y1 - y0}, 0, 0, (uint32)(255.f * k + 0.5f) & 0xFFu, 0.f, 0, 0,
						 nullptr);
					NkPaintCmd &c = cmds[cmds.Size() - 1];
					c.image = image;
					for (int32 i = 0; i < count * 2; ++i) {
						c.xy.PushBack(xy[i]);
						c.uv.PushBack(uv[i]);
					}
					return true;
				}
				void PopTransform() override {
					Push(NkPaintOp::PopTransform, {0.f, 0.f, 0.f, 0.f}, 0, 0, 0, 0.f, 0, 0, nullptr);
				}
				/// 2026-09-04 : le mode dans `icon` (sa valeur NkPaintBlend), rien d'autre
				void PushBlend(NkPaintBlend b) override {
					Push(NkPaintOp::PushBlend, {0.f, 0.f, 0.f, 0.f}, 0, 0, 0, 0.f, (uint16)b, 0, nullptr);
				}
				void PopBlend() override {
					Push(NkPaintOp::PopBlend, {0.f, 0.f, 0.f, 0.f}, 0, 0, 0, 0.f, 0, 0, nullptr);
				}
				void PushClip(const NkPaintRect &r) override {
					++mClipDepth;
					if (mClipDepth > mMaxClipDepth)
						mMaxClipDepth = mClipDepth;
					Push(NkPaintOp::PushClip, r, 0, 0, 0, 0.f, 0, 0, nullptr);
				}
				void PopClip() override {
					// ⚠️ ON N'IGNORE PAS UN DEPILEMENT DE TROP, ON LE NOTE. Un
					//    `PopClip` en trop passe inapercu a l'ecran (la pile de
					//    NKGui a un fond) et corrompt le decoupage du panneau
					//    VOISIN — un defaut qui se manifeste loin de sa cause.
					if (mClipDepth == 0)
						mUnbalanced = true;
					else
						--mClipDepth;
					Push(NkPaintOp::PopClip, {}, 0, 0, 0, 0.f, 0, 0, nullptr);
				}

				// ── Ce que la sonde interroge apres coup ────────────────────────
				bool ClipBalanced() const {
					return !mUnbalanced && mClipDepth == 0;
				}
				uint16 MaxClipDepth() const {
					return mMaxClipDepth;
				}
				uint32 CountOf(NkPaintOp o) const {
					uint32 n = 0;
					for (uint32 i = 0; i < (uint32)cmds.Size(); ++i)
						if (cmds[i].op == o)
							++n;
					return n;
				}
				/// Nombre de commandes qui different de celles d'un autre flux —
				/// les longueurs inegales comptent comme differences.
				uint32 DiffCount(const NkRecordingPaint &o) const {
					const uint32 a = (uint32)cmds.Size(), b = (uint32)o.cmds.Size();
					const uint32 lo = a < b ? a : b;
					uint32 d = (a > b ? a - b : b - a);
					for (uint32 i = 0; i < lo; ++i)
						if (!cmds[i].SameAs(o.cmds[i]))
							++d;
					return d;
				}

			private:
				void Push(NkPaintOp op, const NkPaintRect &r, uint16 role, uint16 role2, uint32 rgba,
						  float32 rounding, uint16 icon, uint8 align, const char *s) {
					NkPaintCmd c;
					c.op = op;
					c.x = r.x;
					c.y = r.y;
					c.w = r.w;
					c.h = r.h;
					c.role = role;
					c.role2 = role2;
					c.rgba = rgba;
					c.rounding = rounding;
					c.icon = icon;
					c.align = align;
					if (s)
						c.text = NkString(s);
					cmds.PushBack(c);
				}

				uint16 mClipDepth = 0;
				uint16 mMaxClipDepth = 0;
				bool mUnbalanced = false;
		};

	} // namespace editorkit
} // namespace nkentseu
