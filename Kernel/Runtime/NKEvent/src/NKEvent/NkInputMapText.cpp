#include "pch.h"
//
// NkInputMapText.cpp
// =============================================================================
// Description :
//   NkInputMap en TEXTE : ce qu'un joueur, un outil ou un fichier de projet
//   lit et ecrit. Une ligne par chose, et toute ligne fautive est REFUSEE avec
//   son numero -- une configuration illisible qui passe en silence donne un
//   jeu ou une touche ne repond pas, sans message.
//
// Le format :
//     # commentaire
//     action   <nom> <bouton|axe1|axe2> [seuil=0.5]
//     contexte <nom> [priorite=<n>] [consomme|traverse] [eteint]
//     lier     <action> <source> [options...]      (dans le dernier contexte)
//
//   Sources :
//     Key:SPACE  Mouse:LEFT  Wheel:V  Gamepad:SOUTH  GamepadAxis:LEFT_X   (NkInputCode)
//     Composite:<plus>,<moins>                     -> axe 1D
//     Composite:<droite>,<gauche>,<haut>,<bas>     -> axe 2D (ZQSD, WASD, fleches)
//     Stick:LEFT | Stick:RIGHT                     MouseDelta
//     Gesture:TAP|DOUBLE_TAP|LONG_PRESS|SWIPE_LEFT|SWIPE_RIGHT|SWIPE_UP|SWIPE_DOWN|PINCH|ROTATE|PAN
//     Zone:<x>,<y>,<l>,<h>                         Joystick:<x>,<y>,<l>,<h>[,<rayon>]
//
//   Options :
//     zm=<min>[:<max>]  (axiale)   zmr=<min>[:<max>]  (radiale)
//     echelle=<x>[,<y>]   inverser=x|y|xy   permuter   courbe=<e>   lissage=<s>
//     quand=enfonce|presse|relache|maintenu:<s>|tape:<s>|double:<s>
//     accord=<code>[+<code>...]
//
//   Les codes sont ceux de NkInputCode::ToString / FromString : le vocabulaire
//   des touches est celui de NkLoadBindings, il n'y en a pas un second.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "NKEvent/NkInputMap.h"

#include <cstdio>

namespace nkentseu {

	namespace {

		/// Un nombre que detail::NkLireNombre relit : jamais d'exposant, et sans
		/// zeros inutiles (« 0.2 », pas « 0.2000 ») -- le texte doit se LIRE.
		NkString Nombre(float32 v) {
			char b[32];
			std::snprintf(b, sizeof(b), "%.4f", static_cast<double>(v));
			int32 n = 0;
			while (b[n] != '\0') {
				++n;
			}
			while (n > 0 && b[n - 1] == '0') {
				--n;
			}
			if (n > 0 && b[n - 1] == '.') {
				--n;
			}
			b[n] = '\0';
			if (b[0] == '-' && b[1] == '0' && b[2] == '\0') {
				return NkString("0");
			}
			return NkString(b);
		}

		bool Prefixe(const char *mot, const char *prefixe, const char *&reste) noexcept {
			if (mot == nullptr) {
				return false;
			}
			int32 k = 0;
			while (prefixe[k] != '\0') {
				// Casse indifferente : « composite: » vaut « Composite: ».
				char a = mot[k];
				char b = prefixe[k];
				if (a >= 'A' && a <= 'Z') {
					a = static_cast<char>(a - 'A' + 'a');
				}
				if (b >= 'A' && b <= 'Z') {
					b = static_cast<char>(b - 'A' + 'a');
				}
				if (a != b) {
					return false;
				}
				++k;
			}
			reste = mot + k;
			return true;
		}

		/// Coupe `texte` sur `sep` ; rend le nombre de morceaux (au plus `capacite`).
		uint32 Couper(const char *texte, char sep, NkString *morceaux, uint32 capacite) {
			uint32 n = 0;
			const char *debut = texte;
			const char *p = texte;
			while (true) {
				if (*p == sep || *p == '\0') {
					if (n < capacite) {
						morceaux[n] = NkString(debut, static_cast<nk_size>(p - debut));
					}
					++n;
					if (*p == '\0') {
						break;
					}
					debut = p + 1;
				}
				++p;
			}
			return n;
		}

		const char *kGestes[] = {"TAP", "DOUBLE_TAP", "LONG_PRESS", "SWIPE_LEFT", "SWIPE_RIGHT",
								 "SWIPE_UP", "SWIPE_DOWN", "PINCH", "ROTATE", "PAN"};

		const char *NomType(NkInputValueType t) noexcept {
			switch (t) {
				case NkInputValueType::NK_INPUT_BUTTON: return "bouton";
				case NkInputValueType::NK_INPUT_AXIS1D: return "axe1";
				case NkInputValueType::NK_INPUT_AXIS2D: return "axe2";
			}
			return "bouton";
		}

		bool LireType(const char *mot, NkInputValueType &t) noexcept {
			if (NkInputNameEquals(mot, "bouton") || NkInputNameEquals(mot, "button")) {
				t = NkInputValueType::NK_INPUT_BUTTON;
				return true;
			}
			if (NkInputNameEquals(mot, "axe1") || NkInputNameEquals(mot, "axis1d")) {
				t = NkInputValueType::NK_INPUT_AXIS1D;
				return true;
			}
			if (NkInputNameEquals(mot, "axe2") || NkInputNameEquals(mot, "axis2d")) {
				t = NkInputValueType::NK_INPUT_AXIS2D;
				return true;
			}
			return false;
		}

		/// La source d'une liaison en texte.
		NkString EcrireSource(const NkInputSource &s) {
			switch (s.kind) {
				case NkInputSourceKind::NK_INPUT_SRC_CODE:
					return s.code.ToString();
				case NkInputSourceKind::NK_INPUT_SRC_COMPOSITE: {
					NkString t = "Composite:";
					for (uint32 p = 0; p < s.partCount; ++p) {
						if (p > 0) {
							t += ",";
						}
						t += s.parts[p].ToString();
					}
					return t;
				}
				case NkInputSourceKind::NK_INPUT_SRC_STICK:
					return NkString(s.rightStick ? "Stick:RIGHT" : "Stick:LEFT");
				case NkInputSourceKind::NK_INPUT_SRC_MOUSE_DELTA:
					return NkString("MouseDelta");
				case NkInputSourceKind::NK_INPUT_SRC_GESTURE:
					return NkString("Gesture:") + kGestes[static_cast<uint32>(s.gesture)];
				case NkInputSourceKind::NK_INPUT_SRC_SCREEN_BUTTON:
					return "Zone:" + Nombre(s.zoneX) + "," + Nombre(s.zoneY) + "," + Nombre(s.zoneW) + "," +
						   Nombre(s.zoneH);
				case NkInputSourceKind::NK_INPUT_SRC_SCREEN_STICK:
					return "Joystick:" + Nombre(s.zoneX) + "," + Nombre(s.zoneY) + "," + Nombre(s.zoneW) + "," +
						   Nombre(s.zoneH) + "," + Nombre(s.radius);
				case NkInputSourceKind::NK_INPUT_SRC_NONE:
					break;
			}
			return NkString("?");
		}

		/// Lit une source ; `erreur` dit pourquoi en cas d'echec.
		bool LireSource(const NkString &mot, NkInputSource &s, NkString &erreur) {
			const char *m = mot.CStr();
			const char *reste = nullptr;
			if (Prefixe(m, "Composite:", reste)) {
				NkString parts[5];
				const uint32 n = Couper(reste, ',', parts, 5);
				if (n != 2 && n != 4) {
					erreur = "un composite a 2 parties (plus, moins) ou 4 (droite, gauche, haut, bas)";
					return false;
				}
				for (uint32 p = 0; p < n; ++p) {
					bool ok = false;
					s.parts[p] = NkInputCode::FromString(parts[p], &ok);
					if (!ok) {
						erreur = NkString::Fmt("partie inconnue « {0} »", parts[p]);
						return false;
					}
				}
				s.kind = NkInputSourceKind::NK_INPUT_SRC_COMPOSITE;
				s.partCount = static_cast<uint8>(n);
				return true;
			}
			if (Prefixe(m, "Stick:", reste)) {
				if (NkInputNameEquals(reste, "LEFT") || NkInputNameEquals(reste, "GAUCHE")) {
					s.rightStick = false;
				} else if (NkInputNameEquals(reste, "RIGHT") || NkInputNameEquals(reste, "DROIT")) {
					s.rightStick = true;
				} else {
					erreur = NkString::Fmt("stick inconnu « {0} » (LEFT ou RIGHT)", NkString(reste));
					return false;
				}
				s.kind = NkInputSourceKind::NK_INPUT_SRC_STICK;
				return true;
			}
			if (NkInputNameEquals(m, "MouseDelta")) {
				s.kind = NkInputSourceKind::NK_INPUT_SRC_MOUSE_DELTA;
				return true;
			}
			if (Prefixe(m, "Gesture:", reste)) {
				for (uint32 g = 0; g < static_cast<uint32>(NkInputGesture::NK_INPUT_GESTURE_COUNT); ++g) {
					if (NkInputNameEquals(reste, kGestes[g])) {
						s.kind = NkInputSourceKind::NK_INPUT_SRC_GESTURE;
						s.gesture = static_cast<NkInputGesture>(g);
						return true;
					}
				}
				erreur = NkString::Fmt("geste inconnu « {0} »", NkString(reste));
				return false;
			}
			const bool zone = Prefixe(m, "Zone:", reste);
			const bool joy = !zone && Prefixe(m, "Joystick:", reste);
			if (zone || joy) {
				NkString v[6];
				const uint32 n = Couper(reste, ',', v, 6);
				const bool compteOk = zone ? n == 4 : (n == 4 || n == 5);
				if (!compteOk) {
					erreur = zone ? "une zone demande x,y,l,h" : "un joystick demande x,y,l,h[,rayon]";
					return false;
				}
				float32 f[5] = {0.f, 0.f, 0.f, 0.f, 0.08f};
				for (uint32 k = 0; k < n; ++k) {
					if (!detail::NkLireNombre(v[k], f[k])) {
						erreur = NkString::Fmt("nombre illisible « {0} »", v[k]);
						return false;
					}
				}
				s.kind = zone ? NkInputSourceKind::NK_INPUT_SRC_SCREEN_BUTTON : NkInputSourceKind::NK_INPUT_SRC_SCREEN_STICK;
				s.zoneX = f[0];
				s.zoneY = f[1];
				s.zoneW = f[2];
				s.zoneH = f[3];
				s.radius = f[4] > 0.f ? f[4] : 0.08f;
				return true;
			}
			bool ok = false;
			s.code = NkInputCode::FromString(mot, &ok);
			if (!ok) {
				erreur = NkString::Fmt("entree inconnue « {0} »", mot);
				return false;
			}
			s.kind = NkInputSourceKind::NK_INPUT_SRC_CODE;
			return true;
		}

		/// « 0.2 » ou « 0.2:0.9 ».
		bool LireMinMax(const char *texte, float32 &lo, float32 &hi) {
			NkString v[3];
			const uint32 n = Couper(texte, ':', v, 3);
			if (n < 1 || n > 2 || !detail::NkLireNombre(v[0], lo)) {
				return false;
			}
			hi = 1.f;
			return n == 1 || detail::NkLireNombre(v[1], hi);
		}

		bool LireOption(const NkString &mot, NkInputBinding &b, NkString &erreur) {
			const char *m = mot.CStr();
			const char *r = nullptr;
			float32 v = 0.f;
			if (Prefixe(m, "zmr=", r) || Prefixe(m, "zm=", r)) {
				const bool radial = m[2] == 'r' || m[2] == 'R';
				float32 lo = 0.f;
				float32 hi = 1.f;
				if (!LireMinMax(r, lo, hi)) {
					erreur = NkString::Fmt("zone morte illisible « {0} »", mot);
					return false;
				}
				b = b.WithDeadZone(radial ? NkInputDeadZone::NK_INPUT_DEADZONE_RADIAL : NkInputDeadZone::NK_INPUT_DEADZONE_AXIAL,
								   lo, hi);
				return true;
			}
			if (Prefixe(m, "echelle=", r)) {
				NkString e[3];
				const uint32 n = Couper(r, ',', e, 3);
				float32 x = 1.f;
				float32 y = 1.f;
				if (n < 1 || n > 2 || !detail::NkLireNombre(e[0], x) || (n == 2 && !detail::NkLireNombre(e[1], y))) {
					erreur = NkString::Fmt("echelle illisible « {0} »", mot);
					return false;
				}
				b.modifiers.scaleX = x;
				b.modifiers.scaleY = n == 2 ? y : 1.f;
				return true;
			}
			if (Prefixe(m, "inverser=", r)) {
				const bool x = NkInputNameEquals(r, "x") || NkInputNameEquals(r, "xy");
				const bool y = NkInputNameEquals(r, "y") || NkInputNameEquals(r, "xy");
				if (!x && !y) {
					erreur = NkString::Fmt("attendu inverser=x, y ou xy, lu « {0} »", mot);
					return false;
				}
				b.modifiers.invertX = x;
				b.modifiers.invertY = y;
				return true;
			}
			if (NkInputNameEquals(m, "permuter")) {
				b.modifiers.swapXY = true;
				return true;
			}
			if (Prefixe(m, "courbe=", r)) {
				if (!detail::NkLireNombre(NkString(r), v) || v <= 0.f) {
					erreur = NkString::Fmt("courbe illisible « {0} »", mot);
					return false;
				}
				b.modifiers.curve = v;
				return true;
			}
			if (Prefixe(m, "lissage=", r)) {
				if (!detail::NkLireNombre(NkString(r), v) || v < 0.f) {
					erreur = NkString::Fmt("lissage illisible « {0} »", mot);
					return false;
				}
				b.modifiers.smoothing = v;
				return true;
			}
			if (Prefixe(m, "quand=", r)) {
				NkString q[3];
				const uint32 n = Couper(r, ':', q, 3);
				float32 t = 0.f;
				if (n == 2 && !detail::NkLireNombre(q[1], t)) {
					erreur = NkString::Fmt("duree illisible « {0} »", mot);
					return false;
				}
				const char *k = q[0].CStr();
				NkInputTriggerKind kind = NkInputTriggerKind::NK_INPUT_TRIGGER_DOWN;
				if (NkInputNameEquals(k, "enfonce")) {
					kind = NkInputTriggerKind::NK_INPUT_TRIGGER_DOWN;
				} else if (NkInputNameEquals(k, "presse")) {
					kind = NkInputTriggerKind::NK_INPUT_TRIGGER_PRESSED;
				} else if (NkInputNameEquals(k, "relache")) {
					kind = NkInputTriggerKind::NK_INPUT_TRIGGER_RELEASED;
				} else if (NkInputNameEquals(k, "maintenu")) {
					kind = NkInputTriggerKind::NK_INPUT_TRIGGER_HOLD;
				} else if (NkInputNameEquals(k, "tape")) {
					kind = NkInputTriggerKind::NK_INPUT_TRIGGER_TAP;
				} else if (NkInputNameEquals(k, "double")) {
					kind = NkInputTriggerKind::NK_INPUT_TRIGGER_DOUBLE_TAP;
				} else {
					erreur = NkString::Fmt("declencheur inconnu « {0} »", mot);
					return false;
				}
				const bool demandeDuree = kind == NkInputTriggerKind::NK_INPUT_TRIGGER_HOLD ||
										  kind == NkInputTriggerKind::NK_INPUT_TRIGGER_TAP ||
										  kind == NkInputTriggerKind::NK_INPUT_TRIGGER_DOUBLE_TAP;
				if (demandeDuree && n != 2) {
					erreur = NkString::Fmt("« {0} » demande une duree (ex. maintenu:0.5)", mot);
					return false;
				}
				b = b.WithTrigger(kind, t);
				return true;
			}
			if (Prefixe(m, "accord=", r)) {
				NkString c[NK_INPUT_CHORD_MAX + 1];
				const uint32 n = Couper(r, '+', c, NK_INPUT_CHORD_MAX + 1);
				if (n > static_cast<uint32>(NK_INPUT_CHORD_MAX)) {
					erreur = NkString::Fmt("un accord a {0} touches au plus", NK_INPUT_CHORD_MAX);
					return false;
				}
				for (uint32 k = 0; k < n; ++k) {
					bool ok = false;
					const NkInputCode code = NkInputCode::FromString(c[k], &ok);
					if (!ok) {
						erreur = NkString::Fmt("touche d'accord inconnue « {0} »", c[k]);
						return false;
					}
					b = b.WithChord(code);
				}
				return true;
			}
			erreur = NkString::Fmt("option inconnue « {0} »", mot);
			return false;
		}

		NkString EcrireOptions(const NkInputBinding &b) {
			NkString t;
			const NkInputModifiers &m = b.modifiers;
			if (m.deadZone != NkInputDeadZone::NK_INPUT_DEADZONE_NONE) {
				t += m.deadZone == NkInputDeadZone::NK_INPUT_DEADZONE_RADIAL ? " zmr=" : " zm=";
				t += Nombre(m.deadZoneMin);
				if (m.deadZoneMax != 1.f) {
					t += ":" + Nombre(m.deadZoneMax);
				}
			}
			if (m.scaleX != 1.f || m.scaleY != 1.f) {
				t += " echelle=" + Nombre(m.scaleX);
				if (m.scaleY != 1.f) {
					t += "," + Nombre(m.scaleY);
				}
			}
			if (m.invertX || m.invertY) {
				t += (m.invertX && m.invertY) ? " inverser=xy" : (m.invertX ? " inverser=x" : " inverser=y");
			}
			if (m.swapXY) {
				t += " permuter";
			}
			if (m.curve != 1.f) {
				t += " courbe=" + Nombre(m.curve);
			}
			if (m.smoothing > 0.f) {
				t += " lissage=" + Nombre(m.smoothing);
			}
			switch (b.trigger.kind) {
				case NkInputTriggerKind::NK_INPUT_TRIGGER_DOWN:       break;
				case NkInputTriggerKind::NK_INPUT_TRIGGER_PRESSED:    t += " quand=presse"; break;
				case NkInputTriggerKind::NK_INPUT_TRIGGER_RELEASED:   t += " quand=relache"; break;
				case NkInputTriggerKind::NK_INPUT_TRIGGER_HOLD:       t += " quand=maintenu:" + Nombre(b.trigger.time); break;
				case NkInputTriggerKind::NK_INPUT_TRIGGER_TAP:        t += " quand=tape:" + Nombre(b.trigger.time); break;
				case NkInputTriggerKind::NK_INPUT_TRIGGER_DOUBLE_TAP: t += " quand=double:" + Nombre(b.trigger.time); break;
			}
			if (b.chordCount > 0) {
				t += " accord=";
				for (uint32 k = 0; k < b.chordCount; ++k) {
					if (k > 0) {
						t += "+";
					}
					t += b.chord[k].ToString();
				}
			}
			return t;
		}

	} // namespace

	NkString NkInputMap::Save() const {
		NkString s = "# NkInputMap -- actions, contextes, liaisons (NkInputMap::Save)\n";
		for (uint32 a = 0; a < mDefs.Size(); ++a) {
			s += NkString("action ") + mDefs[a].name + " " + NomType(mDefs[a].type);
			if (mStates[a].threshold != 0.5f) {
				s += " seuil=" + Nombre(mStates[a].threshold);
			}
			s += "\n";
		}
		for (uint32 c = 0; c < mContexts.Size(); ++c) {
			const NkInputContext &ctx = mContexts[c];
			s += "contexte " + ctx.name + " priorite=" + NkString::Fmt("{0}", ctx.priority);
			s += ctx.consume ? " consomme" : " traverse";
			if (!ctx.enabled) {
				s += " eteint";
			}
			s += "\n";
			for (uint32 i = 0; i < ctx.bindings.Size(); ++i) {
				const NkInputBinding &b = ctx.bindings[i];
				s += NkString("lier ") + ActionName(b.action) + " " + EcrireSource(b.source) + EcrireOptions(b) + "\n";
			}
		}
		return s;
	}

	NkInputMapReport NkInputMap::Load(const NkString &text, bool clearBindings) {
		NkInputMapReport rapport;
		const char *p = text.Data();
		if (p == nullptr) {
			return rapport;
		}
		int32 courant = -1;
		NkVector<int32> vides; // les contextes deja vides par CE texte
		auto Vider = [&](int32 c) {
			if (!clearBindings || c < 0) {
				return;
			}
			for (uint32 k = 0; k < vides.Size(); ++k) {
				if (vides[k] == c) {
					return;
				}
			}
			vides.PushBack(c);
			mContexts[static_cast<uint32>(c)].bindings.Clear();
		};

		uint32 numero = 0;
		while (*p != '\0') {
			const char *debut = p;
			while (*p != '\0' && *p != '\n') {
				++p;
			}
			const NkString ligne(debut, static_cast<nk_size>(p - debut));
			if (*p == '\n') {
				++p;
			}
			++numero;
			NkString mots[16];
			const uint32 n = detail::NkDecouperMots(ligne, mots, 16);
			if (n == 0 || (mots[0].Data() != nullptr && mots[0].Data()[0] == '#')) {
				continue;
			}
			const char *verbe = mots[0].CStr();

			if (NkInputNameEquals(verbe, "action")) {
				NkInputValueType type = NkInputValueType::NK_INPUT_BUTTON;
				if (n < 3 || !LireType(mots[2].CStr(), type)) {
					rapport.errors.PushBack(
						NkString::Fmt("ligne {0} : attendu « action <nom> <bouton|axe1|axe2> »", numero));
					continue;
				}
				float32 seuil = 0.5f;
				const char *r = nullptr;
				if (n >= 4 && (!Prefixe(mots[3].CStr(), "seuil=", r) || !detail::NkLireNombre(NkString(r), seuil))) {
					rapport.errors.PushBack(NkString::Fmt("ligne {0} : option d'action illisible « {1} »", numero, mots[3]));
					continue;
				}
				const NkInputActionId existante = FindAction(mots[1].CStr());
				if (existante != NK_INPUT_ACTION_INVALID && ActionType(existante) != type) {
					rapport.errors.PushBack(NkString::Fmt(
						"ligne {0} : l'action « {1} » existe deja avec un autre type", numero, mots[1]));
					continue;
				}
				const NkInputActionId id = DeclareAction(mots[1].CStr(), type, seuil);
				mStates[static_cast<uint32>(id)].threshold = seuil;
				++rapport.applied;
				continue;
			}

			if (NkInputNameEquals(verbe, "contexte")) {
				if (n < 2) {
					rapport.errors.PushBack(NkString::Fmt("ligne {0} : un contexte demande un nom", numero));
					continue;
				}
				int32 priorite = 0;
				bool consomme = true;
				bool allume = true;
				bool ok = true;
				for (uint32 k = 2; k < n && ok; ++k) {
					const char *r = nullptr;
					float32 v = 0.f;
					if (Prefixe(mots[k].CStr(), "priorite=", r) && detail::NkLireNombre(NkString(r), v)) {
						priorite = static_cast<int32>(v);
					} else if (NkInputNameEquals(mots[k].CStr(), "consomme")) {
						consomme = true;
					} else if (NkInputNameEquals(mots[k].CStr(), "traverse")) {
						consomme = false;
					} else if (NkInputNameEquals(mots[k].CStr(), "eteint")) {
						allume = false;
					} else if (NkInputNameEquals(mots[k].CStr(), "allume")) {
						allume = true;
					} else {
						rapport.errors.PushBack(
							NkString::Fmt("ligne {0} : option de contexte inconnue « {1} »", numero, mots[k]));
						ok = false;
					}
				}
				if (!ok) {
					continue;
				}
				courant = AddContext(mots[1].CStr(), priorite, consomme);
				NkInputContext &c = mContexts[static_cast<uint32>(courant)];
				c.priority = priorite;
				c.consume = consomme;
				c.enabled = allume;
				Vider(courant);
				++rapport.applied;
				continue;
			}

			if (NkInputNameEquals(verbe, "lier")) {
				if (n < 3) {
					rapport.errors.PushBack(NkString::Fmt("ligne {0} : attendu « lier <action> <source> »", numero));
					continue;
				}
				const NkInputActionId action = FindAction(mots[1].CStr());
				if (action == NK_INPUT_ACTION_INVALID) {
					rapport.errors.PushBack(NkString::Fmt(
						"ligne {0} : action « {1} » inconnue, declarez-la avant (ligne « action » ou code)", numero,
						mots[1]));
					continue;
				}
				NkInputBinding b;
				b.action = action;
				NkString erreur;
				if (!LireSource(mots[2], b.source, erreur)) {
					rapport.errors.PushBack(NkString::Fmt("ligne {0} : {1}", numero, erreur));
					continue;
				}
				bool ok = true;
				for (uint32 k = 3; k < n && ok; ++k) {
					ok = LireOption(mots[k], b, erreur);
				}
				if (!ok) {
					rapport.errors.PushBack(NkString::Fmt("ligne {0} : {1}", numero, erreur));
					continue;
				}
				if (courant < 0) {
					courant = AddContext("Defaut", 0, true);
					Vider(courant);
				}
				mContexts[static_cast<uint32>(courant)].Add(b);
				++rapport.applied;
				continue;
			}

			rapport.errors.PushBack(NkString::Fmt(
				"ligne {0} : attendu « action », « contexte » ou « lier », lu « {1} »", numero, mots[0]));
		}
		ResetBindingStates();
		return rapport;
	}

} // namespace nkentseu
