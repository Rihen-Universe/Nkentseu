#pragma once
// -----------------------------------------------------------------------------
// @File    NkTheme.inl
// @Brief   Implantation du systeme de themes. Incluse par NkTheme.h.
// @Author  Rihen
// @License Proprietary - Free to use and modify
// -----------------------------------------------------------------------------

#include <math.h>

namespace nkentseu {
	namespace editorkit {

		namespace themedetail {

			// Table role -> cle. Ecrite EXPLICITEMENT plutot que derivee par macro :
			// la cle est un contrat de FICHIER, elle ne doit pas changer parce qu'on
			// a renomme une valeur d'enum en C++.
			inline const char *const *RoleNames() {
				static const char *const kNames[(uint16)NkRole::Count] = {
					"window_bg",	 "panel_bg",		 "panel_header",  "border",
					"input_bg",		 "label_col",		 "text",		  "text_muted",
					"text_on_accent", "accent_ui",		 "accent_sel",	  "elem_active",
					"elem_selected", "elem_idle",		 "axis_x",		  "axis_y",
					"axis_z",		 "type_mesh",		 "type_anim",	  "type_mat",
					"type_tex",		 "node_data_header", "node_data_hot", "node_action_header",
					"node_body",	 "node_wire",		 "viewport_top",  "viewport_bottom",
					"grid_line",
				};
				return kNames;
			}

			inline bool StrEqZ(const char *a, const char *b) {
				if (!a || !b)
					return a == b;
				while (*a && *b) {
					if (*a != *b)
						return false;
					++a;
					++b;
				}
				return *a == *b;
			}

			inline int32 HexDigit(char c) {
				if (c >= '0' && c <= '9')
					return c - '0';
				if (c >= 'a' && c <= 'f')
					return c - 'a' + 10;
				if (c >= 'A' && c <= 'F')
					return c - 'A' + 10;
				return -1;
			}

			// Luminance relative WCAG : les canaux sont d'abord LINEARISES. Prendre
			// la moyenne des canaux bruts donnerait un contraste faux -- le vert
			// pese six fois le bleu dans la perception.
			inline float64 RelLum(NkThemeColor c) {
				const float64 ch[3] = {(float64)((c >> 24) & 0xFF) / 255.0, (float64)((c >> 16) & 0xFF) / 255.0,
									   (float64)((c >> 8) & 0xFF) / 255.0};
				float64 lin[3];
				for (int32 i = 0; i < 3; ++i)
					lin[i] = (ch[i] <= 0.03928) ? ch[i] / 12.92 : pow((ch[i] + 0.055) / 1.055, 2.4);
				return 0.2126 * lin[0] + 0.7152 * lin[1] + 0.0722 * lin[2];
			}

			// Paires qui se SUPERPOSENT REELLEMENT. Comparer toutes les paires
			// possibles n'aurait aucun sens : l'axe X ne se dessine jamais sur l'axe
			// Y, et le pire contraste du theme serait un chiffre sans consequence.
			// Chaque paire porte SON seuil, parce qu'elles ne servent pas a la meme
			// chose. Du TEXTE demande 4,5 ; un ELEMENT GRAPHIQUE (contour de
			// selection, axe, pastille) demande 3,0. C'est la regle WCAG, et la
			// confondre en un seuil unique donne un validateur inutilisable dans un
			// sens ou dans l'autre.
			struct Pair {
					NkRole fg, bg;
					bool isText;
			};
			inline const Pair *ContrastPairs(uint32 &count) {
				static const Pair kPairs[] = {
					// texte — seuil 4,5
					{NkRole::Text, NkRole::WindowBg, true},
					{NkRole::Text, NkRole::PanelBg, true},
					{NkRole::Text, NkRole::PanelHeader, true},
					{NkRole::Text, NkRole::InputBg, true},
					{NkRole::Text, NkRole::LabelCol, true},
					{NkRole::TextMuted, NkRole::PanelBg, true},
					{NkRole::TextOnAccent, NkRole::AccentUi, true},
					{NkRole::TextOnAccent, NkRole::NodeDataHeader, true},
					// elements graphiques — seuil 3,0
					{NkRole::AxisX, NkRole::PanelBg, false},
					{NkRole::AxisY, NkRole::PanelBg, false},
					{NkRole::AxisZ, NkRole::PanelBg, false},
					{NkRole::ElemSelected, NkRole::ViewportTop, false},
					{NkRole::ElemActive, NkRole::ViewportTop, false},
					{NkRole::AccentSel, NkRole::PanelBg, false},
					// PAS de paire {ElemIdle, ViewportTop}, et c'est deliberé apres
					// mesure : je l'avais ajoutee sans reflechir, elle sortait a 1,10.
					// Un element NON selectionne doit RECULER par construction --
					// Blender lui-meme dessine ses aretes non selectionnees a un
					// contraste tres faible, exprès. La signaler ferait crier au loup,
					// et un validateur qui crie au loup se fait ignorer.
				};
				count = (uint32)(sizeof(kPairs) / sizeof(kPairs[0]));
				return kPairs;
			}

			static const float32 kTextRatio = 4.5f;
			static const float32 kGfxRatio = 3.0f;

		} // namespace themedetail

		inline const char *NkRoleName(NkRole r) {
			return (uint16)r < (uint16)NkRole::Count ? themedetail::RoleNames()[(uint16)r] : "?";
		}

		inline NkRole NkRoleFromName(const char *name) {
			const char *const *n = themedetail::RoleNames();
			for (uint16 i = 0; i < (uint16)NkRole::Count; ++i)
				if (themedetail::StrEqZ(n[i], name))
					return (NkRole)i;
			return NkRole::Count;
		}

		// ── REGISTRE DES ROLES D'APPLICATION ────────────────────────────────────
		namespace themedetail {
			// Statique LOCALE A LA FONCTION : construite au premier appel, donc
			// immunisee contre l'ordre d'initialisation des statiques globales. Une
			// application qui enregistre ses roles depuis un constructeur global
			// planterait autrement.
			inline NkVector<NkString> &ExtNames() {
				static NkVector<NkString> names;
				return names;
			}
		} // namespace themedetail

		inline uint16 NkRoleRegistry::Register(const char *name) {
			const uint16 found = Find(name);
			if (found != NK_ROLE_INVALID)
				return found; // idempotent, comme NkNodeGraph::RegisterType
			NkVector<NkString> &n = themedetail::ExtNames();
			n.PushBack(NkString(name ? name : ""));
			return (uint16)((uint16)NkRole::Count + (uint16)n.Size() - 1u);
		}

		inline uint16 NkRoleRegistry::Find(const char *name) {
			// Le coeur d'abord : une application ne doit pas pouvoir redefinir
			// « accent_ui » en role d'extension et se retrouver avec deux entrees
			// portant le meme nom dans un fichier de theme.
			const NkRole core = NkRoleFromName(name);
			if (core != NkRole::Count)
				return (uint16)core;
			const NkVector<NkString> &n = themedetail::ExtNames();
			for (uint16 i = 0; i < (uint16)n.Size(); ++i)
				if (themedetail::StrEqZ(n[i].CStr(), name))
					return (uint16)((uint16)NkRole::Count + i);
			return NK_ROLE_INVALID;
		}

		inline const char *NkRoleRegistry::Name(uint16 id) {
			if (id < (uint16)NkRole::Count)
				return NkRoleName((NkRole)id);
			const NkVector<NkString> &n = themedetail::ExtNames();
			const uint16 k = (uint16)(id - (uint16)NkRole::Count);
			return k < (uint16)n.Size() ? n[k].CStr() : "?";
		}

		inline uint16 NkRoleRegistry::Total() {
			return (uint16)((uint16)NkRole::Count + (uint16)themedetail::ExtNames().Size());
		}

		inline uint16 NkResolveRole(const char *name) {
			return NkRoleRegistry::Find(name);
		}

		inline NkThemeColor NkTheme::Get(uint16 id) const {
			if (id < (uint16)NkRole::Count)
				return mColors[id];
			const uint16 k = (uint16)(id - (uint16)NkRole::Count);
			// Magenta de « role oublie » si le theme est plus ancien que le registre :
			// ca doit sauter aux yeux, pas se fondre en noir.
			return k < (uint16)mExt.Size() ? mExt[k] : 0xFF00FFFFu;
		}

		inline void NkTheme::Set(uint16 id, NkThemeColor c) {
			if (id < (uint16)NkRole::Count) {
				mColors[id] = c;
				return;
			}
			const uint16 k = (uint16)(id - (uint16)NkRole::Count);
			while ((uint16)mExt.Size() <= k)
				mExt.PushBack(0xFF00FFFFu);
			mExt[k] = c;
		}

		inline NkThemeColor NkTheme::FromHex(const char *hex) {
			if (!hex)
				return 0x000000FFu;
			if (*hex == '#')
				++hex;
			uint32 v = 0;
			int32 n = 0;
			for (; n < 8 && hex[n]; ++n) {
				const int32 d = themedetail::HexDigit(hex[n]);
				if (d < 0)
					break;
				v = (v << 4) | (uint32)d;
			}
			if (n == 6)
				return (v << 8) | 0xFFu; // alpha implicite opaque
			if (n == 8)
				return v;
			return 0x000000FFu;
		}

		inline void NkTheme::ToHex(NkThemeColor c, char out[10]) {
			static const char *kHex = "0123456789ABCDEF";
			out[0] = '#';
			// L'alpha n'est ecrit QUE s'il n'est pas opaque : un fichier de theme
			// ecrit a la main est plus lisible avec « #212121 » qu'avec « #212121FF ».
			const uint32 a = c & 0xFFu;
			const uint32 rgb = (c >> 8) & 0xFFFFFFu;
			for (int32 i = 0; i < 6; ++i)
				out[1 + i] = kHex[(rgb >> (20 - i * 4)) & 0xF];
			if (a == 0xFFu) {
				out[7] = 0;
				return;
			}
			out[7] = kHex[(a >> 4) & 0xF];
			out[8] = kHex[a & 0xF];
			out[9] = 0;
		}

		inline float32 NkTheme::Contrast(NkThemeColor a, NkThemeColor b) {
			const float64 la = themedetail::RelLum(a), lb = themedetail::RelLum(b);
			const float64 hi = la > lb ? la : lb, lo = la > lb ? lb : la;
			return (float32)((hi + 0.05) / (lo + 0.05));
		}

		// ── THEMES LIVRES ───────────────────────────────────────────────────────
		inline NkTheme NkTheme::Dark() {
			NkTheme t;
			t.mDark = true;
			t.mName = NkString("Sombre");
			auto S = [&](NkRole r, const char *h) {
				t.Set(r, FromHex(h));
			};
			// Les trois gris de UI_SPEC 10bis.1
			S(NkRole::WindowBg, "#141414");
			S(NkRole::PanelBg, "#212121");
			S(NkRole::PanelHeader, "#2B2B2B");
			S(NkRole::Border, "#FFFFFF14"); // blanc a 8 %
			S(NkRole::InputBg, "#1A1A1A");
			S(NkRole::LabelCol, "#1A1A1A");
			S(NkRole::Text, "#E6E6E6");
			S(NkRole::TextMuted, "#FFFFFF8C"); // blanc a 55 %
			S(NkRole::TextOnAccent, "#FFFFFF");
			S(NkRole::AccentUi, "#1177D1");
			S(NkRole::AccentSel, "#F2980E"); // l'orange UNIQUE (10bis.2)
			S(NkRole::ElemActive, "#FFFFFF");
			S(NkRole::ElemSelected, "#F2980E");
			S(NkRole::ElemIdle, "#0F0F0F");
			S(NkRole::AxisX, "#C7404A");
			S(NkRole::AxisY, "#5A9E3C");
			S(NkRole::AxisZ, "#3A6FB0");
			S(NkRole::TypeMesh, "#22B8CF");
			S(NkRole::TypeAnim, "#F08C00");
			S(NkRole::TypeMat, "#37B24D");
			S(NkRole::TypeTex, "#E64980");
			S(NkRole::NodeDataHeader, "#0A545E"); // repos      (10bis.3)
			S(NkRole::NodeDataHeaderHot, "#095461"); // survole (10bis.3)
			S(NkRole::NodeActionHeader, "#F2980E");
			S(NkRole::NodeBody, "#2B2B2B");
			S(NkRole::NodeWire, "#B4B4B4");
			S(NkRole::ViewportTop, "#1A1A1A");
			S(NkRole::ViewportBottom, "#252525");
			S(NkRole::GridLine, "#FFFFFF12");
			return t;
		}

		inline NkTheme NkTheme::Light() {
			// Le theme clair n'INVERSE pas les gris, il les REMPLACE (10bis.4) :
			// inverser donnerait des gris moyens sales. Et les couleurs porteuses de
			// sens sont ASSOMBRIES pour rester lisibles sur fond clair -- c'est tout
			// l'interet de les avoir mises dans le theme.
			NkTheme t = Dark();
			t.mDark = false;
			t.mName = NkString("Clair");
			auto S = [&](NkRole r, const char *h) {
				t.Set(r, FromHex(h));
			};
			S(NkRole::WindowBg, "#F5F5F5");
			S(NkRole::PanelBg, "#FFFFFF");
			S(NkRole::PanelHeader, "#EAEAEA");
			S(NkRole::Border, "#0000001F");
			S(NkRole::InputBg, "#FFFFFF");
			S(NkRole::LabelCol, "#F0F0F0");
			S(NkRole::Text, "#1A1A1A");
			S(NkRole::TextMuted, "#0000008C");
			S(NkRole::AccentUi, "#0E5FA6");	 // bleu assombri
			S(NkRole::AccentSel, "#C97A08"); // ambre assombri
			S(NkRole::ElemActive, "#101010");
			// CORRECTIF impose par la mesure : #C97A08 sur la vue claire ne donnait
			// que 2,35 de contraste, sous le seuil graphique de 3,0 -- le contour de
			// selection se serait perdu dans le fond. Assombri, il remonte au-dessus.
			// Assombrir le FOND aurait empire les choses : la vue se serait
			// rapprochee de la luminance de l'ambre au lieu de s'en eloigner.
			S(NkRole::ElemSelected, "#8A4F00");
			// #6E6E6E etait trop proche EN LUMINANCE de l'ambre assombri : sur fond
			// clair, selectionne et non selectionne se seraient confondus. Eclairci,
			// il recule vers le fond -- ce qu'on attend d'un element non selectionne
			// -- tout en laissant l'ambre ressortir.
			S(NkRole::ElemIdle, "#C4C4C4");
			// Axes assombris : les teintes du sombre passent inapercues sur #F5F5F5.
			S(NkRole::AxisX, "#A32B34");
			S(NkRole::AxisY, "#3C7526");
			S(NkRole::AxisZ, "#26518A");
			S(NkRole::TypeMesh, "#0B7C8C");
			S(NkRole::TypeAnim, "#B36800");
			S(NkRole::TypeMat, "#237A33");
			S(NkRole::TypeTex, "#B02A5B");
			// Les sarcelles restent : elles sont deja sombres (10bis.4).
			S(NkRole::NodeBody, "#EAEAEA");
			S(NkRole::NodeWire, "#5A5A5A");
			S(NkRole::ViewportTop, "#D8D8D8");
			S(NkRole::ViewportBottom, "#BFBFBF");
			S(NkRole::GridLine, "#00000014");
			return t;
		}

		inline NkTheme::NkTheme() {
			// Base neutre : magenta criard. Un role oublie doit SAUTER AUX YEUX, pas
			// se fondre en noir sur un fond sombre.
			for (uint16 i = 0; i < (uint16)NkRole::Count; ++i)
				mColors[i] = 0xFF00FFFFu;
			mName = NkString("Sombre");
		}

		// ── FICHIER ─────────────────────────────────────────────────────────────
		inline void NkTheme::Save(NkString &out) const {
			out = NkString("nktheme 1\n");
			out.Append("nom ");
			out.Append(mName);
			out.Append('\n');
			out.Append(mDark ? "base sombre\n" : "base clair\n");
			char hex[10];
			for (uint16 i = 0; i < (uint16)NkRole::Count; ++i) {
				out.Append(NkRoleName((NkRole)i));
				out.Append(" = ");
				ToHex(mColors[i], hex);
				out.Append(hex);
				out.Append('\n');
			}
			// Les roles d'APPLICATION aussi : une sauvegarde qui ne parcourrait que
			// l'enumeration du coeur les perdrait en silence, et le theme paraitrait
			// pourtant enregistre.
			for (uint16 k = 0; k < (uint16)mExt.Size(); ++k) {
				const uint16 id = (uint16)((uint16)NkRole::Count + k);
				out.Append(NkRoleRegistry::Name(id));
				out.Append(" = ");
				ToHex(mExt[k], hex);
				out.Append(hex);
				out.Append('\n');
			}
		}

		inline bool NkTheme::Load(const char *text, uint32 *outUnknown, uint32 *outApplied) {
			if (outUnknown)
				*outUnknown = 0;
			if (outApplied)
				*outApplied = 0;
			if (!text)
				return false;

			const char *p = text;
			bool sawHeader = false;
			char key[64];
			char val[32];
			while (*p) {
				// Debut de ligne : on saute les blancs.
				while (*p == ' ' || *p == '\t')
					++p;
				if (*p == '#' || *p == '\n' || *p == '\r') { // commentaire ou ligne vide
					while (*p && *p != '\n')
						++p;
					if (*p)
						++p;
					continue;
				}
				// Cle
				uint32 k = 0;
				while (*p && *p != ' ' && *p != '\t' && *p != '=' && *p != '\n' && *p != '\r' && k < 63)
					key[k++] = *p++;
				key[k] = 0;
				// Reste de la ligne
				while (*p == ' ' || *p == '\t')
					++p;

				if (themedetail::StrEqZ(key, "nktheme")) {
					sawHeader = true;
				} else if (themedetail::StrEqZ(key, "nom")) {
					NkString nm;
					while (*p && *p != '\n' && *p != '\r')
						nm.Append(*p++);
					if (nm.Size() > 0)
						mName = nm;
				} else if (themedetail::StrEqZ(key, "base")) {
					// Ligne informative : la base est choisie par l'APPELANT avant
					// l'appel. La lire ici et recharger Dark()/Light() ecraserait ce
					// qu'il a deja pose.
				} else if (k > 0) {
					if (*p == '=') {
						++p;
						while (*p == ' ' || *p == '\t')
							++p;
					}
					uint32 v = 0;
					while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' && v < 31)
						val[v++] = *p++;
					val[v] = 0;
					const uint16 id = NkResolveRole(key);
					if (id == NK_ROLE_INVALID) {
						// Role inconnu : soit le fichier vient d'une version plus recente,
						// soit d'une AUTRE APPLICATION (« nkanima.cle » lu par Nogee). Dans
						// les deux cas on COMPTE au lieu d'echouer -- sinon chaque ajout de
						// role rendrait les themes existants illisibles.
						if (outUnknown)
							(*outUnknown)++;
					} else if (v > 0) {
						Set(id, FromHex(val));
						if (outApplied)
							(*outApplied)++;
					}
				}
				while (*p && *p != '\n')
					++p;
				if (*p)
					++p;
			}
			return sawHeader;
		}

		namespace themedetail {
			// Une couleur TRANSPARENTE se compose avec ce qu'il y a dessous : la
			// comparer telle quelle donnerait un contraste faux.
			inline NkThemeColor Composite(NkThemeColor fg, NkThemeColor bg) {
				const uint32 a = fg & 0xFFu;
				if (a >= 0xFFu)
					return fg;
				const float32 af = (float32)a / 255.f;
				uint32 comp = 0;
				for (int32 ch = 0; ch < 3; ++ch) {
					const uint32 sh = 24u - (uint32)ch * 8u;
					const float32 f = (float32)((fg >> sh) & 0xFF);
					const float32 b = (float32)((bg >> sh) & 0xFF);
					const uint32 m = (uint32)(f * af + b * (1.f - af) + 0.5f);
					comp |= (m & 0xFFu) << sh;
				}
				return comp | 0xFFu;
			}
		} // namespace themedetail

		inline uint32 NkTheme::Validate(NkThemeIssue *outWorst) const {
			uint32 n = 0;
			const themedetail::Pair *pairs = themedetail::ContrastPairs(n);
			uint32 fails = 0;
			float32 worstDeficit = 0.f;
			for (uint32 i = 0; i < n; ++i) {
				const NkThemeColor bg = Get(pairs[i].bg);
				const NkThemeColor fg = themedetail::Composite(Get(pairs[i].fg), bg);
				const float32 c = Contrast(fg, bg);
				const float32 need = pairs[i].isText ? themedetail::kTextRatio : themedetail::kGfxRatio;
				if (c >= need)
					continue;
				fails++;
				// On retient le pire ECART A SON SEUIL, pas le plus petit rapport :
				// un axe a 2,9 (seuil 3,0) est presque bon, du texte a 3,2 (seuil
				// 4,5) est bien plus fautif malgre un rapport superieur.
				const float32 deficit = need - c;
				if (deficit > worstDeficit) {
					worstDeficit = deficit;
					if (outWorst) {
						outWorst->fg = pairs[i].fg;
						outWorst->bg = pairs[i].bg;
						outWorst->ratio = c;
						outWorst->required = need;
						outWorst->isText = pairs[i].isText;
					}
				}
			}
			return fails;
		}

		inline float32 NkTheme::WorstContrast(NkRole *outA, NkRole *outB) const {
			uint32 n = 0;
			const themedetail::Pair *pairs = themedetail::ContrastPairs(n);
			float32 worst = 1e9f;
			for (uint32 i = 0; i < n; ++i) {
				// Une couleur TRANSPARENTE se compose avec ce qu'il y a dessous : la
				// comparer telle quelle donnerait un contraste faux. On la compose sur
				// son fond avant de mesurer.
				const NkThemeColor bg = Get(pairs[i].bg);
				const NkThemeColor fg = themedetail::Composite(Get(pairs[i].fg), bg);
				const float32 c = Contrast(fg, bg);
				if (c < worst) {
					worst = c;
					if (outA)
						*outA = pairs[i].fg;
					if (outB)
						*outB = pairs[i].bg;
				}
			}
			return worst;
		}

		// ── BIBLIOTHEQUE ────────────────────────────────────────────────────────
		inline void NkThemeLibrary::AddBuiltins() {
			mThemes.PushBack(NkTheme::Dark());
			mThemes.PushBack(NkTheme::Light());
			mCurrent = 0;
		}

		inline int32 NkThemeLibrary::AddFromText(const char *text, bool baseDark) {
			// On part de la BASE demandee, jamais d'un theme vide : c'est ce qui permet
			// a un fichier de trois lignes de donner un theme complet.
			NkTheme t = baseDark ? NkTheme::Dark() : NkTheme::Light();
			if (!t.Load(text))
				return -1;
			// Un theme qui reprend le nom d'un autre le REMPLACE : c'est ce qu'attend
			// un utilisateur qui surcharge « Sombre » depuis son dossier personnel. En
			// ajouter un second homonyme donnerait deux entrees indistinguables.
			const int32 existing = Find(t.Name().CStr());
			if (existing >= 0) {
				mThemes[(uint32)existing] = t;
				return existing;
			}
			mThemes.PushBack(t);
			return (int32)mThemes.Size() - 1;
		}

		inline int32 NkThemeLibrary::Find(const char *name) const {
			for (uint32 i = 0; i < (uint32)mThemes.Size(); ++i)
				if (themedetail::StrEqZ(mThemes[i].Name().CStr(), name))
					return (int32)i;
			return -1;
		}

		inline bool NkThemeLibrary::SetCurrent(const char *name) {
			const int32 i = Find(name);
			if (i < 0)
				return false;
			mCurrent = (uint32)i;
			return true;
		}

	} // namespace editorkit
} // namespace nkentseu
