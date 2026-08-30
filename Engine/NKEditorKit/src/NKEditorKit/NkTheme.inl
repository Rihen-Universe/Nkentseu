#pragma once
// -----------------------------------------------------------------------------
// @File    NkTheme.inl
// @Brief   Implantation du systeme de themes. Incluse par NkTheme.h.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include <math.h>
#include <stdio.h> // puits par defaut du repli franc (NkRoleAudit) : stderr

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
					"type_folder",
					"button_bg",
					"tab_bar_bg",
					"canvas_bg",
					"canvas_dot",
					"status_ok",
					"status_err",
					"accent_ai",
					"snap_line",
					"artboard_bg",
					"doc_text",
					"doc_field_bg",
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

		namespace themedetail {

			// LA RECHERCHE BRUTE : octet pour octet, coeur puis extensions. Aucune
			// canonisation, aucun audit, aucun effet de bord.
			//
			// ⚠️ ELLE EXISTE POUR QUE `Register` NE PASSE PAS PAR `Find`. Le detail
			//    a l'air mineur et ne l'est pas : `Register` appelle la recherche
			//    pour son idempotence, et si cette recherche comptait une faute,
			//    enregistrer un role d'application parfaitement legitime
			//    (« nk3d.anneau_brosse », premier appel) inscrirait une faute
			//    IMAGINAIRE dans l'audit. Un audit faux est pire qu'aucun audit :
			//    il fait chercher un defaut qui n'existe pas, et il decredibilise
			//    les vrais. Tenu par l'essai 3g du banc NKEditorKitTest.
			inline uint16 FindExactRole(const char *name) {
				// Le coeur d'abord : une application ne doit pas pouvoir redefinir
				// « accent_ui » en role d'extension et se retrouver avec deux
				// entrees portant le meme nom dans un fichier de theme.
				const NkRole core = NkRoleFromName(name);
				if (core != NkRole::Count)
					return (uint16)core;
				const NkVector<NkString> &n = ExtNames();
				for (uint16 i = 0; i < (uint16)n.Size(); ++i)
					if (StrEqZ(n[i].CStr(), name))
						return (uint16)((uint16)NkRole::Count + i);
				return NK_ROLE_INVALID;
			}

			// Le puits du repli franc. Statique LOCALE A LA FONCTION, meme raison
			// que `ExtNames()` : immunisee contre l'ordre d'initialisation des
			// statiques globales.
			struct AuditSinkSlot {
					NkRoleAuditSink fn;
					void *user;
			};

			// Puits PAR DEFAUT : stderr. Il n'est pas nul, et c'est le coeur de la
			// regle — un repli qui ne se dit pas est un repli silencieux, et un
			// repli silencieux est ce qui a laisse vivre le magenta trois seances.
			inline void DefaultAuditSink(void *user, const char *line) {
				(void)user;
				if (line && *line)
					fprintf(stderr, "[NKEditorKit/theme] %s\n", line);
			}

			inline AuditSinkSlot &AuditSink() {
				static AuditSinkSlot slot = {&DefaultAuditSink, nullptr};
				return slot;
			}

		} // namespace themedetail

		// ── LA CANONISATION ─────────────────────────────────────────────────────
		// PURE ET SANS ETAT : elle ne lit ni le registre, ni le theme, ni rien
		// d'autre que son entree. C'est ce qui la rend testable seule, et ce qui
		// garantit qu'elle ne peut pas dependre de l'ordre des appels.
		inline bool NkCanonicalRoleName(const char *in, char *out, uint32 cap) {
			if (!in || !*in || !out || cap == 0)
				return false;
			uint32 n = 0;
			char prev = 0;
			for (const char *p = in; *p; ++p) {
				const char c = *p;
				const bool upper = (c >= 'A' && c <= 'Z');
				const bool prevLowerOrDigit =
					(prev >= 'a' && prev <= 'z') || (prev >= '0' && prev <= '9');
				if (upper && prevLowerOrDigit) {
					if (n + 1 >= cap)
						return false; // pas de troncature silencieuse
					out[n++] = '_';
				}
				if (n + 1 >= cap)
					return false;
				out[n++] = upper ? (char)(c - 'A' + 'a') : c;
				prev = c;
			}
			out[n] = 0;
			return n > 0;
		}

		// ── LE REPLI FRANC ──────────────────────────────────────────────────────
		inline NkVector<NkRoleAudit::Entry> &NkRoleAudit::Faults() {
			static NkVector<Entry> v;
			return v;
		}

		inline NkVector<NkRoleAudit::Entry> &NkRoleAudit::Rescued() {
			static NkVector<Entry> v;
			return v;
		}

		inline void NkRoleAudit::Reset() {
			Faults().Clear();
			Rescued().Clear();
		}

		inline uint32 NkRoleAudit::FaultCount() {
			return (uint32)Faults().Size();
		}

		inline uint32 NkRoleAudit::RescuedCount() {
			return (uint32)Rescued().Size();
		}

		inline void NkRoleAudit::SetSink(NkRoleAuditSink fn, void *user) {
			themedetail::AuditSinkSlot &slot = themedetail::AuditSink();
			slot.fn = fn;
			slot.user = user;
		}

		inline void NkRoleAudit::Note(NkVector<Entry> &into, const char *name, const char *canon) {
			for (uint32 i = 0; i < (uint32)into.Size(); ++i)
				if (themedetail::StrEqZ(into[i].name.CStr(), name))
					return; // deja vu : une seule entree, quel que soit le nombre d'images
			Entry e;
			e.name = NkString(name ? name : "");
			e.canon = NkString(canon ? canon : "");
			into.PushBack(e);

			// LA VOIX DU REPLI. Une fois par nom DISTINCT, grace a la deduplication
			// ci-dessus : la trace reste lisible meme si le dessin resout le meme
			// nom soixante fois par seconde.
			const themedetail::AuditSinkSlot &slot = themedetail::AuditSink();
			if (!slot.fn)
				return;
			char line[256];
			if (canon && *canon)
				snprintf(line, sizeof(line),
						 "role « %s » rattrape par canonisation -> « %s » ; A CORRIGER A LA SOURCE",
						 name ? name : "(nul)", canon);
			else
				snprintf(line, sizeof(line),
						 "role « %s » NON RESOLU : il sera peint avec la couleur de repli",
						 name ? name : "(nul)");
			slot.fn(slot.user, line);
		}

		inline void NkRoleAudit::Summary(NkString &out, uint32 maxNames) {
			out = NkString("");
			char b[160];
			snprintf(b, sizeof(b), "%u role(s) NON RESOLU(S), %u rattrape(s) par canonisation",
					 FaultCount(), RescuedCount());
			out.Append(b);

			// Deux listes, deux significations : la premiere est ce qui est PERDU,
			// la seconde ce qui est SAUVE mais faux a la source. Les fondre en une
			// seule ferait disparaitre la distinction qui dicte quoi faire.
			struct Local {
					static void Append(NkString &o, const NkVector<Entry> &v, const char *lead,
									   uint32 maxN, bool withCanon) {
						if (v.Size() == 0)
							return;
						o.Append(lead);
						const uint32 n = (uint32)v.Size();
						const uint32 shown = n < maxN ? n : maxN;
						for (uint32 i = 0; i < shown; ++i) {
							if (i)
								o.Append(", ");
							o.Append(v[i].name);
							if (withCanon && !v[i].canon.Empty()) {
								o.Append("->");
								o.Append(v[i].canon);
							}
						}
						if (n > shown) {
							char t[48];
							snprintf(t, sizeof(t), " (+%u)", n - shown);
							o.Append(t);
						}
					}
			};
			Local::Append(out, Faults(), "  |  non resolus : ", maxNames, false);
			Local::Append(out, Rescued(), "  |  a corriger a la source : ", maxNames, true);
		}

		inline uint16 NkRoleRegistry::Register(const char *name) {
			// ⚠️ `FindExactRole`, PAS `Find` : voir le bloc de `FindExactRole`.
			//    Passer par `Find` inscrirait une faute a chaque enregistrement neuf
			//    et ferait resoudre « PanelBg » vers le role du coeur au lieu de
			//    creer l'extension demandee — un changement de comportement que
			//    personne n'a demande. Un refactor se juge sur ce qu'il ne change
			//    pas : `Register` se comporte EXACTEMENT comme avant.
			const uint16 found = themedetail::FindExactRole(name);
			if (found != NK_ROLE_INVALID)
				return found; // idempotent, comme NkNodeGraph::RegisterType
			NkVector<NkString> &n = themedetail::ExtNames();
			n.PushBack(NkString(name ? name : ""));
			return (uint16)((uint16)NkRole::Count + (uint16)n.Size() - 1u);
		}

		inline uint16 NkRoleRegistry::Find(const char *name) {
			// 1. LE NOM BRUT D'ABORD. Il ne peut donc rien arriver a un nom qui
			//    resolvait deja — la canonisation ne peut QUE rattraper.
			const uint16 direct = themedetail::FindExactRole(name);
			if (direct != NK_ROLE_INVALID)
				return direct;

			// 2. LA FORME CANONISEE ENSUITE.
			char canon[96];
			if (NkCanonicalRoleName(name, canon, sizeof(canon))) {
				const uint16 id = themedetail::FindExactRole(canon);
				if (id != NK_ROLE_INVALID) {
					// Rattrape, mais faux a la source : on le dit, sinon la
					// canonisation rend les declarations fausses invisibles.
					NkRoleAudit::Note(NkRoleAudit::Rescued(), name, canon);
					return id;
				}
			}

			// 3. ECHEC FRANC. Compte, nomme, et le dit. Jamais peint en silence.
			NkRoleAudit::Note(NkRoleAudit::Faults(), name, "");
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

		// ── LES SIX ROLES BANANI (doc 11 §3), UNE SEULE TABLE ───────────────────
		// Poses par CHAQUE fabrique livree : les valeurs sont celles de la
		// maquette (reference exacte, Rodolf 30/08) et ne dependent pas du theme
		// de l'EDITEUR — la toile V2 est claire meme en editeur sombre, le vert
		// « Pret » est LE vert, le violet IA est LE violet. Un theme FICHIER qui
		// ne les porte pas garde le repli (GetOuRepli), rien ne casse.
		inline void NkThemePoserRolesBanani(NkTheme &t) {
			t.Set(NkRole::CanvasBg, NkTheme::FromHex("#f5f7fb"));
			t.Set(NkRole::CanvasDot, NkTheme::FromHex("#d4dce8"));
			t.Set(NkRole::StatusOk, NkTheme::FromHex("#3fb950"));
			t.Set(NkRole::StatusErr, NkTheme::FromHex("#f85149"));
			t.Set(NkRole::AccentAI, NkTheme::FromHex("#a371f7"));
			t.Set(NkRole::SnapLine, NkTheme::FromHex("#ff4fd8"));
			t.Set(NkRole::ArtboardBg, NkTheme::FromHex("#ffffff"));
			t.Set(NkRole::DocText, NkTheme::FromHex("#1f2328"));
			t.Set(NkRole::DocFieldBg, NkTheme::FromHex("#f2f4f8"));
		}

		// ── THEMES LIVRES ───────────────────────────────────────────────────────
		inline NkTheme NkTheme::Dark() {
			NkTheme t;
			t.mDark = true;
			t.mName = NkString("Sombre");
			auto S = [&](NkRole r, const char *h) {
				t.Set(r, FromHex(h));
			};

			// ── PALETTE GITHUB DARK ─────────────────────────────────────────────
			// Demandee par Rihen. Elle remplace les trois gris neutres de UI_SPEC
			// 10bis.1 -- la HIERARCHIE A TROIS NIVEAUX, elle, est conservee : c'est
			// elle qui structure la lecture, pas les valeurs exactes.
			//
			// Ce qui change en pratique : les gris sont plus SOMBRES et legerement
			// BLEUTES. Un gris parfaitement neutre sur un grand aplat parait sale ;
			// une pointe de bleu le fait lire comme une surface et non comme une
			// absence de couleur. C'est le choix de GitHub, de VS Code et d'Unreal.
			S(NkRole::WindowBg, "#010409");	  // le plus sombre : il recule derriere tout
			S(NkRole::PanelBg, "#0D1117");	  // le panneau se detache du vide
			S(NkRole::PanelHeader, "#161B22"); // en-tetes et barres : ce qui structure se lit d'abord
			// Bordure OPAQUE et non un blanc translucide : sur un fond aussi sombre,
			// 8 % de blanc donne un trait invisible. GitHub utilise une valeur pleine.
			S(NkRole::Border, "#30363D");
			S(NkRole::InputBg, "#0D1117");
			S(NkRole::LabelCol, "#161B22");
			S(NkRole::Text, "#FFFFFF");
			S(NkRole::TextMuted, "#8B949E"); // le gris de texte secondaire de GitHub
			S(NkRole::TextOnAccent, "#FFFFFF");
			S(NkRole::AccentUi, "#1F6FEB"); // bleu d'etat d'interface
			S(NkRole::AccentSel, "#F2980E"); // l'orange UNIQUE du produit (10bis.2), inchange

			// Etats de selection d'un element de maillage.
			S(NkRole::ElemActive, "#FFFFFF");
			S(NkRole::ElemSelected, "#F2980E");
			S(NkRole::ElemIdle, "#161B22");

			// Axes, eclaircis pour tenir sur un fond plus sombre qu'avant.
			S(NkRole::AxisX, "#F85149"); // rouge GitHub
			S(NkRole::AxisY, "#3FB950"); // vert GitHub
			S(NkRole::AxisZ, "#58A6FF"); // bleu GitHub

			S(NkRole::TypeMesh, "#39C5CF");
			S(NkRole::TypeAnim, "#DB6D28");
			S(NkRole::TypeMat, "#3FB950");
			S(NkRole::TypeTex, "#DB61A2");

			// Les deux sarcelles imposees restent : elles servent deux etats du MEME
			// en-tete de noeud, qui ne coexistent jamais (10bis.3).
			S(NkRole::NodeDataHeader, "#0A545E");
			S(NkRole::NodeDataHeaderHot, "#095461");
			S(NkRole::NodeActionHeader, "#F2980E");
			S(NkRole::NodeBody, "#161B22");
			S(NkRole::NodeWire, "#8B949E");

			// Vue 3D : un cran AU-DESSUS des panneaux et non en dessous. C'est la zone
			// qu'on regarde, elle ne doit pas s'enfoncer davantage que le cadre.
			S(NkRole::ViewportTop, "#1C2128");
			S(NkRole::ViewportBottom, "#22272E");
			S(NkRole::GridLine, "#FFFFFF14");

			S(NkRole::TypeFolder, "#E3B341"); // ambre de dossier, version GitHub
			NkThemePoserRolesBanani(t);
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
			// Assombri : #F0B429 sur fond blanc passe inapercu.
			S(NkRole::TypeFolder, "#A87400");
			NkThemePoserRolesBanani(t);
			return t;
		}

		inline NkTheme::NkTheme() {
			// Base neutre : magenta criard. Un role oublie doit SAUTER AUX YEUX, pas
			// se fondre en noir sur un fond sombre.
			for (uint16 i = 0; i < (uint16)NkRole::Count; ++i)
				mColors[i] = 0xFF00FFFFu;
			// ⚠️ LES ROLES A REPLI NE PRENNENT PAS LE MAGENTA. Le magenta dit
			//    « quelqu'un a oublie de me poser » ; ces deux-la ne sont pas
			//    oublies, ils sont FACULTATIFS et se replient sur leur role source
			//    (cf. `GetOuRepli`). Les laisser en magenta rendrait tous les
			//    boutons de toutes les applications magenta a la seconde ou la
			//    conversion les lirait -- un role neuf doit etre gratuit.
			mColors[(uint16)NkRole::ButtonBg] = NkThemeNonDefini;
			mColors[(uint16)NkRole::TabBarBg] = NkThemeNonDefini;
			// Les six roles Banani du 31/08 : meme regime facultatif-avec-repli.
			mColors[(uint16)NkRole::CanvasBg] = NkThemeNonDefini;
			mColors[(uint16)NkRole::CanvasDot] = NkThemeNonDefini;
			mColors[(uint16)NkRole::StatusOk] = NkThemeNonDefini;
			mColors[(uint16)NkRole::StatusErr] = NkThemeNonDefini;
			mColors[(uint16)NkRole::AccentAI] = NkThemeNonDefini;
			mColors[(uint16)NkRole::SnapLine] = NkThemeNonDefini;
			mColors[(uint16)NkRole::ArtboardBg] = NkThemeNonDefini;
			mColors[(uint16)NkRole::DocText] = NkThemeNonDefini;
			mColors[(uint16)NkRole::DocFieldBg] = NkThemeNonDefini;
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
		// ── LES THEMES GITHUB, ET ILS SONT ICI ─────────────────────────────────
		// ⚠️ DANS LE KIT, PAS DANS L APPLICATION, et c est la regle pour TOUTES les
		//    applications : « Dark Pro / Light Pro pour toutes les applications ».
		//    Definis dans une application, chaque application les redefinirait
		//    differemment -- six « GitHub Dark Pro » qui ne se ressemblent pas, et
		//    aucun moyen de dire lequel fait foi.
		//
		// ⚠️ ILS HERITENT DE LA BASE, ILS NE LA REECRIVENT PAS. Seuls les roles que
		//    GitHub DEFINIT reellement sont poses ; les autres (axes X/Y/Z, en-tetes
		//    de noeuds, degrade de vue 3D) n ont aucun equivalent chez GitHub et
		//    gardent ceux de `Dark()` / `Light()`. C est exactement le mecanisme que
		//    `AddFromText` exploite deja -- « un theme de trois lignes doit heriter
		//    des 26 autres ». Inventer une valeur GitHub pour un role que GitHub
		//    n a pas, ce serait publier un chiffre sans provenance.
		//
		// PROVENANCE DES VALEURS : jetons Primer de GitHub. `#0969da` (accent clair)
		// et `#ffffff` sont en outre LISIBLES SUR LA PLANCHE 091913 de NkUIDesign,
		// qui montre « Fond #0969da / Texte #ffffff » dans l Inspecteur -- deux des
		// valeurs sont donc confirmees par une source interne, pas seulement par ma
		// memoire. Les autres viennent des jetons Primer publies.
		inline NkTheme NkThemeGitHubDarkPro() {
			NkTheme t = NkTheme::Dark();
			t.SetName("GitHub Dark Pro");
			t.Set(NkRole::WindowBg, NkTheme::FromHex("#0d1117"));	// canvas.default
			t.Set(NkRole::PanelBg, NkTheme::FromHex("#161b22"));	// canvas.subtle
			t.Set(NkRole::PanelHeader, NkTheme::FromHex("#21262d")); // canvas.inset+
			t.Set(NkRole::Border, NkTheme::FromHex("#30363d"));		// border.default
			t.Set(NkRole::InputBg, NkTheme::FromHex("#0d1117"));
			t.Set(NkRole::LabelCol, NkTheme::FromHex("#161b22"));
			t.Set(NkRole::Text, NkTheme::FromHex("#c9d1d9"));		// fg.default
			t.Set(NkRole::TextMuted, NkTheme::FromHex("#8b949e"));	// fg.muted
			t.Set(NkRole::TextOnAccent, NkTheme::FromHex("#ffffff"));
			t.Set(NkRole::AccentUi, NkTheme::FromHex("#58a6ff"));	// accent.fg
			// ⚠️ `AccentSel` RESTE L ORANGE DU PRODUIT. C est une regle du depot --
			//    « le BLEU dit l etat de l INTERFACE, l AMBRE dit la selection » --
			//    et elle ne depend pas du theme choisi. La remplacer par un jeton
			//    GitHub ferait disparaitre la distinction dans ce theme-la seulement,
			//    ce qui est pire qu une couleur inhabituelle : c est une regle qui
			//    tient une fois sur deux.
			return t;
		}

		inline NkTheme NkThemeGitHubLightPro() {
			NkTheme t = NkTheme::Light();
			t.SetName("GitHub Light Pro");
			t.Set(NkRole::WindowBg, NkTheme::FromHex("#ffffff"));	// canvas.default
			t.Set(NkRole::PanelBg, NkTheme::FromHex("#f6f8fa"));	// canvas.subtle
			t.Set(NkRole::PanelHeader, NkTheme::FromHex("#eaeef2"));
			t.Set(NkRole::Border, NkTheme::FromHex("#d0d7de"));		// border.default
			t.Set(NkRole::InputBg, NkTheme::FromHex("#ffffff"));
			t.Set(NkRole::LabelCol, NkTheme::FromHex("#f6f8fa"));
			t.Set(NkRole::Text, NkTheme::FromHex("#1f2328"));		// fg.default
			t.Set(NkRole::TextMuted, NkTheme::FromHex("#656d76"));	// fg.muted
			t.Set(NkRole::TextOnAccent, NkTheme::FromHex("#ffffff"));
			t.Set(NkRole::AccentUi, NkTheme::FromHex("#0969da"));	// accent.fg
			return t;
		}

		// ⚠️ LE THEME PAR DEFAUT DE LA COQUILLE (migration du 2026-08-30).
		//    C est la palette « GitHub Dark » que `NkEditorShell::Init` recopiait
		//    a la main depuis des semaines, ENFIN exprimee dans le vocabulaire
		//    des roles -- ce qui n est devenu possible que quand `ButtonBg` et
		//    `TabBarBg` ont ete ajoutes (le fond d un bouton et la barre
		//    d onglets n avaient pas de mot).
		//
		//    ⚠️ PAS DANS `AddBuiltins`, ET C EST VOULU : ce n est pas un choix
		//       offert a l utilisateur, c est le defaut de la coquille. L ajouter
		//       a la liste en ferait un cinquieme theme du menu Affichage, et il
		//       y ferait doublon avec « GitHub Dark Pro » sans lui etre identique
		//       -- deux entrees qui se ressemblent et different est exactement ce
		//       qu un menu ne doit pas offrir.
		inline NkTheme NkThemeCoquilleDefaut() {
			NkTheme t = NkTheme::Dark();
			t.SetName("Coquille (GitHub Dark)");
			t.Set(NkRole::WindowBg, NkTheme::FromHex("#0d1117"));	 // editeur
			t.Set(NkRole::PanelBg, NkTheme::FromHex("#010409"));	 // sidebar, plus sombre
			t.Set(NkRole::PanelHeader, NkTheme::FromHex("#191d23")); // titres/menus
			t.Set(NkRole::Border, NkTheme::FromHex("#212730"));
			t.Set(NkRole::InputBg, NkTheme::FromHex("#0d1117"));
			t.Set(NkRole::Text, NkTheme::FromHex("#dfdfdf"));
			t.Set(NkRole::TextMuted, NkTheme::FromHex("#7d8590"));
			t.Set(NkRole::TextOnAccent, NkTheme::FromHex("#ffffff"));
			t.Set(NkRole::AccentUi, NkTheme::FromHex("#1f6feb"));
			// Les deux roles qui ont rendu cette palette exprimable :
			t.Set(NkRole::ButtonBg, NkTheme::FromHex("#191d23")); // != InputBg, c etait le point
			t.Set(NkRole::TabBarBg, NkTheme::FromHex("#191d23"));
			return t;
		}

		inline void NkThemeLibrary::AddBuiltins() {
			mThemes.PushBack(NkTheme::Dark());
			mThemes.PushBack(NkTheme::Light());
			// ⚠️ AJOUTES APRES, jamais AVANT : `mCurrent = 0` designe le premier, et
			//    inserer devant changerait silencieusement le theme par defaut de
			//    toutes les applications qui appellent `AddBuiltins`.
			mThemes.PushBack(NkThemeGitHubDarkPro());
			mThemes.PushBack(NkThemeGitHubLightPro());
			mCurrent = 0;
		}

		inline int32 NkThemeLibrary::AddOrReplace(const NkTheme &t) {
			// Un theme qui reprend le nom d'un autre le REMPLACE : c'est ce qu'attend
			// un utilisateur qui surcharge « Sombre » depuis son dossier personnel. En
			// ajouter un homonyme donnerait deux entrees indistinguables dans le menu.
			const int32 existing = Find(t.Name().CStr());
			if (existing >= 0) {
				mThemes[(uint32)existing] = t;
				return existing;
			}
			mThemes.PushBack(t);
			return (int32)mThemes.Size() - 1;
		}

		inline int32 NkThemeLibrary::AddFromText(const char *text, bool baseDark) {
			// On part de la BASE demandee, jamais d'un theme vide : c'est ce qui permet
			// a un fichier de trois lignes de donner un theme complet.
			NkTheme t = baseDark ? NkTheme::Dark() : NkTheme::Light();
			if (!t.Load(text))
				return -1;
			return AddOrReplace(t);
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
