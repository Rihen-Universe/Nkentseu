//
// NkX11Clean.h — retire les macros X11 qui collisionnent avec du C++ ordinaire.
//
// X11 definit des macros aux noms courants (None, Bool, Status, Success...) qui remplacent le texte des
// enumerations qui suivent. Inclure ce fichier APRES X11, dans les fichiers meles au code moteur ; les
// constantes d'evenements (KeyPress, Expose...) restent. Le pourquoi complet : manuel de NKPlatform, et
// Docs/archives/NKPlatform/exemples_des_entetes.md (section NkX11Clean.h).
//
#pragma once

#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)

// Valeurs conservees sous un autre nom, pour le code qui en a reellement besoin
// apres la neutralisation.
namespace nkentseu {
	namespace platform {
		// `None` de X11 : « aucune ressource ». Utiliser NkX11None a la place.
		static const unsigned long NkX11None = 0UL;
		// `ParentRelative` de X11.
		static const unsigned long NkX11ParentRelative = 1UL;
	} // namespace platform
} // namespace nkentseu

// ── Noms en conflit avec du C++ ordinaire ──
//
// Cette liste ne contient QUE des noms dont la collision a ete CONSTATEE. Elle
// se limite aujourd'hui a `None`. Ne rien y ajouter « par precaution » :
// l'erreur a deja ete commise, et voici ce qu'elle a coute.
//
// `Bool`, `Status` et `Success` avaient ete ajoutes sans qu'aucune collision
// n'ait ete observee. Or ce ne sont pas des macros decoratives : les en-tetes
// X11 s'en servent comme TYPES. Dans la variante XCB, `Xutil.h` est inclus
// APRES NKGui, donc apres cette neutralisation — et il ne trouvait plus ses
// propres types. Des dizaines d'erreurs « unknown type name 'Status' », toutes
// provoquees par le correctif lui-meme.
//
// REGLE : on n'ajoute un nom ici qu'apres avoir vu l'erreur qu'il provoque, et
// apres avoir verifie qu'aucun en-tete X11 ne s'en sert comme type.
#ifdef None
#undef None // collisionne avec NkGuiInteract::None, NkGuiEdge::None, etc.
#endif

#endif // Linux / BSD
