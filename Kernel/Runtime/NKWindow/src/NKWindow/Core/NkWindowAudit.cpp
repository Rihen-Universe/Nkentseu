// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkWindowAudit.cpp
//
// Voir NkWindowAudit.h pour le pourquoi. Ici, le comment.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#include "NKWindow/Core/NkWindowAudit.h"
#include "NKWindow/Core/NkWindowConfig.h"

#include "NKLogger/NkLog.h"

namespace nkentseu {

	namespace {

		// Les noms sont ceux des CHAMPS de NkWindowConfig, pas des paraphrases :
		// l'utilisateur doit pouvoir chercher la chaîne du journal dans son code
		// et tomber sur la ligne qu'il a écrite.
		const char *const kNoms[static_cast<uint32>(NkWindowProp::Count)] = {
			"resizable",   "movable",	   "closable",	  "minimizable",	   "maximizable",
			"canFullscreen", "fullscreen", "modal",		  "centered",		   "dropEnabled",
			"frame",	   "hasShadow",	   "transparent", "visible",		   "bgColor",
			"alwaysOnTop", "clickThrough", "opacity",	  "noActivate",		   "minWidth/minHeight",
			"maxWidth/maxHeight", "screenOrientation", "respectSafeArea", "hideSystemUI", "lockOrientation",
		};

		// « Déjà dit » : un seul dorsal est compilé par processus, donc un
		// tableau plat par propriété suffit — le couple (propriété, plateforme)
		// est déterminé par la propriété seule.
		bool gDejaDit[static_cast<uint32>(NkWindowProp::Count)] = {};

		void Dire(NkWindowProp p, const char *plateforme, const char *raison) {
			const uint32 i = static_cast<uint32>(p);
			if (i >= static_cast<uint32>(NkWindowProp::Count) || gDejaDit[i])
				return;
			gDejaDit[i] = true;
			NkLog::Instance().Warnf("[NkWindow] REFUS : NkWindowConfig::%s n'est pas tenu par le dorsal %s — %s. "
									"Le reglage a ete accepte, il n'aura AUCUN effet. "
									"Table complete : wiki/Runtime/NKWindow/Proprietes-par-dorsal.md",
									kNoms[i], plateforme ? plateforme : "?", raison);
		}

		// Demandé ≠ défaut. On compare au `NkWindowConfig` par défaut plutôt qu'à
		// des constantes recopiées ici : une constante recopiée se périme le jour
		// où le défaut du struct change, et le refus se mettrait à crier sur des
		// applications qui n'ont rien demandé.
		bool EstDemandee(NkWindowProp p, const NkWindowConfig &c, const NkWindowConfig &d) {
			switch (p) {
				case NkWindowProp::Resizable:	 return c.resizable != d.resizable;
				case NkWindowProp::Movable:		 return c.movable != d.movable;
				case NkWindowProp::Closable:	 return c.closable != d.closable;
				case NkWindowProp::Minimizable:	 return c.minimizable != d.minimizable;
				case NkWindowProp::Maximizable:	 return c.maximizable != d.maximizable;
				case NkWindowProp::CanFullscreen:return c.canFullscreen != d.canFullscreen;
				case NkWindowProp::Fullscreen:	 return c.fullscreen != d.fullscreen;
				case NkWindowProp::Modal:		 return c.modal != d.modal;
				case NkWindowProp::Centered:	 return c.centered != d.centered;
				case NkWindowProp::DropEnabled:	 return c.dropEnabled != d.dropEnabled;
				case NkWindowProp::Frame:		 return c.frame != d.frame;
				case NkWindowProp::HasShadow:	 return c.hasShadow != d.hasShadow;
				case NkWindowProp::Transparent:	 return c.transparent != d.transparent;
				case NkWindowProp::Visible:		 return c.visible != d.visible;
				case NkWindowProp::BgColor:		 return c.bgColor != d.bgColor;
				case NkWindowProp::AlwaysOnTop:	 return c.alwaysOnTop != d.alwaysOnTop;
				case NkWindowProp::ClickThrough: return c.clickThrough != d.clickThrough;
				case NkWindowProp::Opacity:		 return c.opacity < 1.0f;
				case NkWindowProp::NoActivate:	 return c.noActivate != d.noActivate;
				case NkWindowProp::MinSize:		 return c.minWidth != d.minWidth || c.minHeight != d.minHeight;
				case NkWindowProp::MaxSize:		 return c.maxWidth != d.maxWidth || c.maxHeight != d.maxHeight;
				case NkWindowProp::ScreenOrientation: return c.screenOrientation != d.screenOrientation;
				case NkWindowProp::RespectSafeArea:	  return c.respectSafeArea != d.respectSafeArea;
				case NkWindowProp::HideSystemUI:	  return c.hideSystemUI != d.hideSystemUI;
				case NkWindowProp::LockOrientation:	  return c.lockOrientation != d.lockOrientation;
				default: return false;
			}
		}

	} // namespace

	const char *NkWindowPropNom(NkWindowProp p) {
		const uint32 i = static_cast<uint32>(p);
		return i < static_cast<uint32>(NkWindowProp::Count) ? kNoms[i] : "?";
	}

	void NkWindowAuditerConfig(const NkWindowConfig &config, const char *plateforme, uint64 tenues, uint64 sansObjet) {
		// ⚠️ La référence est un défaut CONSTRUIT, pas une copie de `config` :
		// tirer la référence de l'objet mesuré rendrait « rien n'a été demandé »
		// quelle que soit la demande.
		const NkWindowConfig defauts{};

		for (uint32 i = 0; i < static_cast<uint32>(NkWindowProp::Count); ++i) {
			const NkWindowProp p = static_cast<NkWindowProp>(i);
			const uint64 bit = NkWindowPropBit(p);
			if (tenues & bit)
				continue; // le dorsal l'honore : rien à dire
			if (!EstDemandee(p, config, defauts))
				continue; // valeur par défaut : l'appelant n'a rien demandé
			Dire(p, plateforme,
				 (sansObjet & bit) ? "cette plateforme n'a pas la notion correspondante"
								   : "ce dorsal ne l'implemente pas encore");
		}
	}

	// =========================================================================
	// NkWindowAuditerDorsalCourant — LA TABLE DE VERITE, EN CODE
	//
	// Un SEUL endroit declare ce que chaque dorsal tient. La table lisible
	// (wiki/Runtime/NKWindow/Proprietes-par-dorsal.md) et ce bloc doivent dire
	// la meme chose ; si l'un des deux change, l'autre est faux.
	//
	// Appele depuis NkWESystem::RegisterWindow, que TOUS les dorsaux traversent
	// dans leur `Create`. Un appel par dorsal aurait voulu douze fichiers edites
	// dont onze qu'aucune construction d'ici ne compile — et un refus qui ne
	// compile pas est un silence de plus.
	//
	// ⚠️ Ecrire une propriete dans `tenues` sans l'implementer remet le silence
	//    en place par la porte de service. Ce masque est une PROMESSE.
	// =========================================================================
	void NkWindowAuditerDorsalCourant(const NkWindowConfig &config) {
		// Communs a tous les dorsaux qui ont une fenetre : le plein ecran et la
		// visibilite sont honores partout.
		constexpr uint64 kBase = NK_WPROP(Fullscreen) | NK_WPROP(Visible);

		// Ce qu'un appareil sans bureau n'a pas : cadre, barre de titre, bords,
		// empilement, curseur qui traverse. Une application plein ecran unique.
		constexpr uint64 kSansBureau = NK_WPROP(Resizable) | NK_WPROP(Movable) | NK_WPROP(Closable) |
									   NK_WPROP(Minimizable) | NK_WPROP(Maximizable) | NK_WPROP(CanFullscreen) |
									   NK_WPROP(Modal) | NK_WPROP(Centered) | NK_WPROP(Frame) |
									   NK_WPROP(HasShadow) | NK_WPROP(DropEnabled) | NK_WPROP(MinSize) |
									   NK_WPROP(MaxSize) | NK_WPROP(AlwaysOnTop) | NK_WPROP(ClickThrough) |
									   NK_WPROP(Opacity) | NK_WPROP(NoActivate);

		// Ce qu'un bureau n'a pas : rotation d'ecran, barre systeme, encoche.
		constexpr uint64 kSansMobile = NK_WPROP(ScreenOrientation) | NK_WPROP(RespectSafeArea) |
									   NK_WPROP(HideSystemUI) | NK_WPROP(LockOrientation);

#if defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY)
		NkWindowAuditerConfig(config, "Noop", kBase, kSansBureau | kSansMobile | NK_WPROP(Transparent) | NK_WPROP(BgColor));

#elif defined(NKENTSEU_PLATFORM_UWP)
		NkWindowAuditerConfig(config, "UWP", kBase, kSansBureau | kSansMobile);

#elif defined(NKENTSEU_PLATFORM_XBOX)
		NkWindowAuditerConfig(config, "Xbox", kBase | NK_WPROP(Transparent) | NK_WPROP(BgColor),
							  kSansBureau | kSansMobile);

#elif defined(NKENTSEU_PLATFORM_WINDOWS)
		// Win32 apres le correctif du 25/09 : le style DESCEND de la config, et
		// `bgColor` alimente la brosse de la classe.
		// `modal` et `canFullscreen` sont tenus depuis le 25/09 (consigne ecrite) :
		// modal desactive la fenetre parent designee et la reactive a la fermeture
		// (refus nomme s'il n'y a pas de parent) ; canFullscreen=false refuse le
		// passage plein ecran par l'UTILISATEUR sans brider SetFullscreen.
		// Plus rien n'est en silence sur ce dorsal.
		// ⚠️ `bgColor` est TENU, mais il porte une reserve que l'audit global ne
		//    sait pas exprimer : la brosse appartient a la CLASSE de fenetre. Le
		//    cas « deja enregistree avec une autre couleur » est donc refuse a son
		//    propre site, dans NkWin32Window.cpp, la ou il se constate.
		NkWindowAuditerConfig(config, "Win32",
							  kBase | NK_WPROP(Resizable) | NK_WPROP(Movable) | NK_WPROP(Closable) |
								  NK_WPROP(Minimizable) | NK_WPROP(Maximizable) | NK_WPROP(Centered) |
								  NK_WPROP(DropEnabled) | NK_WPROP(Frame) | NK_WPROP(HasShadow) |
								  NK_WPROP(Transparent) | NK_WPROP(AlwaysOnTop) | NK_WPROP(ClickThrough) |
								  NK_WPROP(Opacity) | NK_WPROP(NoActivate) | NK_WPROP(MinSize) |
							  NK_WPROP(MaxSize) | NK_WPROP(BgColor) | NK_WPROP(Modal) |
							  NK_WPROP(CanFullscreen),
							  kSansMobile);

#elif defined(NKENTSEU_PLATFORM_MACOS)
		// Cocoa lit resizable et minimizable ; closable est pose SANS CONDITION
		// avec le cadre (le champ n'est jamais relu), donc il n'est pas promis.
		NkWindowAuditerConfig(config, "Cocoa",
							  kBase | NK_WPROP(Resizable) | NK_WPROP(Minimizable) | NK_WPROP(Centered) |
								  NK_WPROP(Frame) | NK_WPROP(HasShadow) | NK_WPROP(Transparent) |
								  NK_WPROP(AlwaysOnTop) | NK_WPROP(ClickThrough) | NK_WPROP(Opacity),
							  kSansMobile);

#elif defined(NKENTSEU_PLATFORM_IOS)
		NkWindowAuditerConfig(config, "UIKit",
							  kBase | NK_WPROP(Transparent) | NK_WPROP(ScreenOrientation),
							  kSansBureau);

#elif defined(NKENTSEU_PLATFORM_ANDROID)
		NkWindowAuditerConfig(config, "Android",
							  kBase | NK_WPROP(ScreenOrientation) | NK_WPROP(RespectSafeArea) |
								  NK_WPROP(HideSystemUI) | NK_WPROP(LockOrientation),
							  kSansBureau);

#elif defined(NKENTSEU_PLATFORM_HARMONYOS)
		NkWindowAuditerConfig(config, "HarmonyOS",
							  kBase | NK_WPROP(ScreenOrientation) | NK_WPROP(HideSystemUI) |
								  NK_WPROP(LockOrientation),
							  kSansBureau);

#elif defined(NKENTSEU_WINDOWING_WAYLAND)
		// Wayland est le seul a honorer maxWidth/maxHeight. En revanche
		// xdg-shell n'a ni positionnement client (centered), ni « toujours
		// au-dessus », ni opacite par fenetre : ce sont des protocoles
		// d'extension que nous n'implementons pas.
		NkWindowAuditerConfig(config, "Wayland",
							  kBase | NK_WPROP(Resizable) | NK_WPROP(Frame) | NK_WPROP(Transparent) |
								  NK_WPROP(DropEnabled) | NK_WPROP(ClickThrough) | NK_WPROP(MinSize) |
								  NK_WPROP(MaxSize),
							  kSansMobile | NK_WPROP(Centered));

#elif defined(NKENTSEU_WINDOWING_XCB)
		NkWindowAuditerConfig(config, "XCB",
							  kBase | NK_WPROP(Resizable) | NK_WPROP(Centered) | NK_WPROP(Frame) |
								  NK_WPROP(Transparent) | NK_WPROP(BgColor) | NK_WPROP(AlwaysOnTop) |
								  NK_WPROP(ClickThrough) | NK_WPROP(Opacity) | NK_WPROP(MinSize),
							  kSansMobile);

#elif defined(NKENTSEU_WINDOWING_XLIB)
		NkWindowAuditerConfig(config, "XLib",
							  kBase | NK_WPROP(Resizable) | NK_WPROP(Centered) | NK_WPROP(Frame) |
								  NK_WPROP(Transparent) | NK_WPROP(BgColor) | NK_WPROP(AlwaysOnTop) |
								  NK_WPROP(ClickThrough) | NK_WPROP(Opacity) | NK_WPROP(MinSize),
							  kSansMobile);

#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
		NkWindowAuditerConfig(config, "Emscripten",
							  kBase | NK_WPROP(Transparent) | NK_WPROP(ScreenOrientation), kSansBureau);

#else
		NkWindowAuditerConfig(config, "Noop", kBase, kSansBureau | kSansMobile | NK_WPROP(Transparent) | NK_WPROP(BgColor));
#endif
	}

	void NkWindowRefuserUneFois(NkWindowProp p, const char *plateforme, const char *raison) {
		Dire(p, plateforme, raison ? raison : "non implemente");
	}

	void NkWindowAuditReinitialiser() {
		for (uint32 i = 0; i < static_cast<uint32>(NkWindowProp::Count); ++i)
			gDejaDit[i] = false;
	}

} // namespace nkentseu
