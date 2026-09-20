#include "pch.h"
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/MeshSculpt/NkBrushDesc.cpp
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   Lecture d'un descripteur de brosse depuis du texte `.nkbrush`.
//
// FORMAT -- calque sur `.nktheme`, qui est deja celui du produit :
//   commentaires `#`, une ligne de magie `nkbrush 1`, puis `cle = valeur`.
//   Le choisir identique n'est pas de la coquetterie : quelqu'un qui a deja
//   personnalise un theme sait deja ecrire une brosse.
//
// ⚠️ POURQUOI UN PARSEUR DE NOMBRE MAISON, ET NON `atof`
//    `atof` et `strtod` DEPENDENT DE LA LOCALE. En fr-FR, le separateur
//    decimal est la virgule : `atof("0.9")` peut rendre **0.0**. Le depot a
//    deja paye exactement ca -- « PowerShell ecrit la virgule decimale :
//    $env:X = 0.90 donne 0,9 et atof rend 0.0 ». Un fichier de brosse ecrit
//    sur une machine et lu sur une autre donnerait alors une force NULLE, en
//    silence, et le seul symptome serait « la brosse ne fait rien ».
//    Ici : le point est le seul separateur accepte, la virgule aussi est
//    acceptee et vaut la meme chose, et tout autre caractere est un REFUS.
// -----------------------------------------------------------------------------

#include "NKRenderer/Tools/MeshSculpt/NkBrushDesc.h"

#include <cmath>

namespace nkentseu {
	namespace renderer {

		namespace {

			bool IsSpace(char c) noexcept {
				return c == ' ' || c == '\t' || c == '\r' || c == '\n';
			}

			bool StrEqI(const char *a, const char *b) noexcept {
				for (;; ++a, ++b) {
					char ca = *a, cb = *b;
					if (ca >= 'A' && ca <= 'Z')
						ca = (char)(ca - 'A' + 'a');
					if (cb >= 'A' && cb <= 'Z')
						cb = (char)(cb - 'A' + 'a');
					if (ca != cb)
						return false;
					if (ca == 0)
						return true;
				}
			}

			void CopyClamped(char *dst, uint32 cap, const char *src, uint32 len) noexcept {
				uint32 n = (len < cap - 1) ? len : cap - 1;
				for (uint32 i = 0; i < n; ++i)
					dst[i] = src[i];
				dst[n] = 0;
			}

			// Lecture d'un flottant INDEPENDANTE DE LA LOCALE. Rend false si la
			// chaine n'est pas entierement un nombre -- « 0.5abc » est un refus,
			// pas un 0.5. Une lecture partielle acceptee en silence est la porte
			// par laquelle une donnee fautive devient un reglage plausible.
			bool ParseF32(const char *s, uint32 len, float32 &out) noexcept {
				uint32 i = 0;
				while (i < len && IsSpace(s[i]))
					++i;
				if (i >= len)
					return false;
				float64 sign = 1.0;
				if (s[i] == '+' || s[i] == '-') {
					if (s[i] == '-')
						sign = -1.0;
					++i;
				}
				bool anyDigit = false;
				float64 v = 0.0;
				while (i < len && s[i] >= '0' && s[i] <= '9') {
					v = v * 10.0 + (float64)(s[i] - '0');
					++i;
					anyDigit = true;
				}
				// Point OU virgule : le fichier reste lisible quelle que soit la
				// machine qui l'a ecrit, sans dependre d'une locale a l'execution.
				if (i < len && (s[i] == '.' || s[i] == ',')) {
					++i;
					float64 scale = 0.1;
					while (i < len && s[i] >= '0' && s[i] <= '9') {
						v += (float64)(s[i] - '0') * scale;
						scale *= 0.1;
						++i;
						anyDigit = true;
					}
				}
				if (!anyDigit)
					return false;
				while (i < len && IsSpace(s[i]))
					++i;
				if (i != len)
					return false; // reste des caracteres : REFUS, pas de lecture partielle
				out = (float32)(sign * v);
				return true;
			}

			bool ParseU32(const char *s, uint32 len, uint32 &out) noexcept {
				float32 f = 0.f;
				if (!ParseF32(s, len, f))
					return false;
				if (f < 0.f)
					return false;
				out = (uint32)(f + 0.5f);
				return true;
			}

			struct NamedOp {
					const char *name;
					NkSculptOp op;
			};

			// ⚠️ CETTE TABLE NE CONTIENT QUE CE QUI AGIT. Y ajouter un nom avant
			//    d'avoir ecrit la primitive ferait apparaitre dans l'interface une
			//    brosse qui ne deforme rien -- et « elle existe » se lirait comme
			//    « elle marche ». « lisser » y entre AUJOURD'HUI parce que
			//    `NkSculptApplyStroke` la traite, pas parce qu'on la prevoit.
			const NamedOp kOps[] = {
				{"normale", NkSculptOp::NK_SCULPT_OP_NORMAL},
				{"normal", NkSculptOp::NK_SCULPT_OP_NORMAL},
				{"lisser", NkSculptOp::NK_SCULPT_OP_SMOOTH},
				{"lissage", NkSculptOp::NK_SCULPT_OP_SMOOTH},
				{"smooth", NkSculptOp::NK_SCULPT_OP_SMOOTH},
			};

			struct NamedFalloff {
					const char *name;
					NkSculptFalloffKind k;
			};

			const NamedFalloff kFalloffs[] = {
				{"doux", NkSculptFalloffKind::NK_FALLOFF_SMOOTH},
				{"smooth", NkSculptFalloffKind::NK_FALLOFF_SMOOTH},
				{"lineaire", NkSculptFalloffKind::NK_FALLOFF_LINEAR},
				{"linear", NkSculptFalloffKind::NK_FALLOFF_LINEAR},
				{"constant", NkSculptFalloffKind::NK_FALLOFF_CONSTANT},
				{"net", NkSculptFalloffKind::NK_FALLOFF_SHARP},
				{"sharp", NkSculptFalloffKind::NK_FALLOFF_SHARP},
				{"sphere", NkSculptFalloffKind::NK_FALLOFF_SPHERE},
			};

		} // namespace

		const char *NkBrushParseText(NkBrushParse r) noexcept {
			switch (r) {
				case NkBrushParse::NK_BRUSH_OK:
					return "ok";
				case NkBrushParse::NK_BRUSH_ERR_MAGIC:
					return "ligne de magie 'nkbrush <version>' absente";
				case NkBrushParse::NK_BRUSH_ERR_VERSION:
					return "version de format inconnue";
				case NkBrushParse::NK_BRUSH_ERR_NO_NAME:
					return "aucun champ 'nom' : la brosse n'a pas d'identite";
				case NkBrushParse::NK_BRUSH_ERR_UNKNOWN_OP:
					return "operation inconnue (primitive non implementee)";
				case NkBrushParse::NK_BRUSH_ERR_UNKNOWN_FALLOFF:
					return "profil d'attenuation inconnu";
				case NkBrushParse::NK_BRUSH_ERR_BAD_NUMBER:
					return "champ numerique illisible";
				case NkBrushParse::NK_BRUSH_ERR_OUT_OF_RANGE:
					return "valeur hors des bornes declarees";
			}
			return "motif inconnu";
		}

		float32 NkBrushFalloff(float32 t, NkSculptFalloffKind kind, float32 hardness) noexcept {
			if (t < 0.f)
				t = 0.f;
			if (t > 1.f)
				t = 1.f;
			switch (kind) {
				case NkSculptFalloffKind::NK_FALLOFF_CONSTANT:
					return 1.f;
				case NkSculptFalloffKind::NK_FALLOFF_LINEAR:
					return 1.f - t;
				case NkSculptFalloffKind::NK_FALLOFF_SHARP: {
					const float32 u = 1.f - t;
					return u * u * u;
				}
				case NkSculptFalloffKind::NK_FALLOFF_SPHERE: {
					const float32 q = 1.f - t * t;
					return (q <= 0.f) ? 0.f : sqrtf(q);
				}
				case NkSculptFalloffKind::NK_FALLOFF_SMOOTH:
				default: {
					// smoothstep inverse, puis exposant module par la durete :
					// durete 0 -> tres doux, durete 1 -> bord franc.
					const float32 s = 1.f - (t * t * (3.f - 2.f * t));
					float32 e = 1.f + 3.f * ((hardness < 0.f) ? 0.f : (hardness > 1.f ? 1.f : hardness));
					float32 r = s;
					// Puissance par multiplications : evite d'appeler pow pour un
					// exposant qui vit dans [1..4].
					float32 acc = 1.f;
					while (e >= 1.f) {
						acc *= r;
						e -= 1.f;
					}
					if (e > 0.f)
						acc *= (1.f - e) + e * r; // interpolation lineaire du reste
					return acc;
				}
			}
		}

		NkBrushParse ParseBrushDesc(const char *text, uint32 len, NkBrushDesc &out, char *errBuf,
									uint32 errCap) noexcept {
			if (errBuf && errCap)
				errBuf[0] = 0;
			out = NkBrushDesc{};
			if (!text || len == 0)
				return NkBrushParse::NK_BRUSH_ERR_MAGIC;

			bool sawMagic = false;
			bool sawName = false;
			uint32 i = 0;

			while (i < len) {
				// Decoupe d'une ligne.
				uint32 b = i;
				while (i < len && text[i] != '\n')
					++i;
				uint32 e = i;
				if (i < len)
					++i; // saute le \n
				while (e > b && (text[e - 1] == '\r' || text[e - 1] == ' ' || text[e - 1] == '\t'))
					--e;
				while (b < e && IsSpace(text[b]))
					++b;
				if (b >= e || text[b] == '#')
					continue; // ligne vide ou commentaire

				// La ligne de magie doit etre la PREMIERE ligne utile.
				if (!sawMagic) {
					const char *kMagic = "nkbrush";
					uint32 m = 0;
					while (m < 7 && b + m < e && text[b + m] == kMagic[m])
						++m;
					if (m != 7)
						return NkBrushParse::NK_BRUSH_ERR_MAGIC;
					uint32 p = b + 7;
					while (p < e && IsSpace(text[p]))
						++p;
					uint32 ver = 0;
					if (!ParseU32(text + p, e - p, ver))
						return NkBrushParse::NK_BRUSH_ERR_MAGIC;
					if (ver != kNkBrushFormatVersion)
						return NkBrushParse::NK_BRUSH_ERR_VERSION;
					sawMagic = true;
					continue;
				}

				// `cle = valeur`
				uint32 eq = b;
				while (eq < e && text[eq] != '=')
					++eq;
				if (eq >= e)
					continue; // ligne sans '=' : ignoree (tolerance de format)
				uint32 ke = eq;
				while (ke > b && IsSpace(text[ke - 1]))
					--ke;
				uint32 vb = eq + 1;
				while (vb < e && IsSpace(text[vb]))
					++vb;

				char key[32] = {};
				CopyClamped(key, 32, text + b, ke - b);
				const char *vs = text + vb;
				const uint32 vl = e - vb;

				if (StrEqI(key, "nom")) {
					if (vl == 0)
						return NkBrushParse::NK_BRUSH_ERR_NO_NAME;
					CopyClamped(out.name, NkBrushDesc::kNameCap, vs, vl);
					sawName = true;
				} else if (StrEqI(key, "libelle")) {
					CopyClamped(out.label, NkBrushDesc::kLabelCap, vs, vl);
				} else if (StrEqI(key, "icone")) {
					CopyClamped(out.icon, NkBrushDesc::kIconCap, vs, vl);
				} else if (StrEqI(key, "operation")) {
					char v[32] = {};
					CopyClamped(v, 32, vs, vl);
					bool found = false;
					for (uint32 k = 0; k < sizeof(kOps) / sizeof(kOps[0]); ++k)
						if (StrEqI(v, kOps[k].name)) {
							out.op = kOps[k].op;
							found = true;
							break;
						}
					// ⚠️ REFUS NOMME, jamais un repli sur la primitive par defaut.
					//    Se replier ferait agir une brosse « glaise » comme un
					//    simple « dessiner » : elle marcherait, mal, sans rien dire.
					if (!found) {
						if (errBuf && errCap)
							CopyClamped(errBuf, errCap, vs, vl);
						return NkBrushParse::NK_BRUSH_ERR_UNKNOWN_OP;
					}
				} else if (StrEqI(key, "profil")) {
					char v[32] = {};
					CopyClamped(v, 32, vs, vl);
					bool found = false;
					for (uint32 k = 0; k < sizeof(kFalloffs) / sizeof(kFalloffs[0]); ++k)
						if (StrEqI(v, kFalloffs[k].name)) {
							out.falloff = kFalloffs[k].k;
							found = true;
							break;
						}
					if (!found) {
						if (errBuf && errCap)
							CopyClamped(errBuf, errCap, vs, vl);
						return NkBrushParse::NK_BRUSH_ERR_UNKNOWN_FALLOFF;
					}
				} else {
					float32 f = 0.f;
					bool numeric = true;
					if (StrEqI(key, "rayon"))
						numeric = ParseF32(vs, vl, out.radius);
					else if (StrEqI(key, "force"))
						numeric = ParseF32(vs, vl, out.strength);
					else if (StrEqI(key, "durete"))
						numeric = ParseF32(vs, vl, out.hardness);
					else if (StrEqI(key, "sens"))
						numeric = ParseF32(vs, vl, out.dir);
					else if (StrEqI(key, "espacement"))
						numeric = ParseF32(vs, vl, out.spacing);
					else if (StrEqI(key, "rayon_min"))
						numeric = ParseF32(vs, vl, out.radiusMin);
					else if (StrEqI(key, "rayon_max"))
						numeric = ParseF32(vs, vl, out.radiusMax);
					else if (StrEqI(key, "force_min"))
						numeric = ParseF32(vs, vl, out.strengthMin);
					else if (StrEqI(key, "force_max"))
						numeric = ParseF32(vs, vl, out.strengthMax);
					else {
						(void)f; // cle inconnue : ignoree, le format reste extensible
						continue;
					}
					if (!numeric) {
						if (errBuf && errCap)
							CopyClamped(errBuf, errCap, text + b, e - b);
						return NkBrushParse::NK_BRUSH_ERR_BAD_NUMBER;
					}
				}
			}

			if (!sawMagic)
				return NkBrushParse::NK_BRUSH_ERR_MAGIC;
			if (!sawName)
				return NkBrushParse::NK_BRUSH_ERR_NO_NAME;

			// COHERENCE DES BORNES. Une brosse dont le maximum est sous le minimum
			// donnerait une glissiere vide dans l'interface : le defaut serait
			// visible, mais seulement a l'ecran, donc tard.
			if (out.radiusMax < out.radiusMin || out.strengthMax < out.strengthMin)
				return NkBrushParse::NK_BRUSH_ERR_OUT_OF_RANGE;
			// `sens` n'a que deux valeurs qui aient un sens physique.
			if (out.dir >= 0.f)
				out.dir = 1.f;
			else
				out.dir = -1.f;

			out.valid = true;
			return NkBrushParse::NK_BRUSH_OK;
		}

	} // namespace renderer
} // namespace nkentseu
