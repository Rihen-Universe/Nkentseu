//
// NkX11Clean.h — retire les macros X11 qui collisionnent avec du C++ ordinaire.
//
// POURQUOI CE FICHIER EXISTE
//
// Les en-tetes X11 definissent des MACROS portant des noms de mots courants :
// None, Bool, Status, Success, Always... Ce ne sont pas des identifiants mais
// des `#define`, donc ils remplacent aveuglement le texte partout ou il
// apparait ENSUITE — y compris dans des enumerations parfaitement legitimes.
//
// Symptome rencontre : `NkGuiTypes.h` declare `None = 0` dans plusieurs
// enumerations. Des qu'un fichier incluait X11 avant NKGui, le preprocesseur
// transformait `None = 0` en `0L = 0`, et le compilateur repondait « expected
// identifier » sur des lignes parfaitement correctes. Des dizaines d'erreurs,
// toutes a cote de la vraie cause.
//
// CE QU'ON NE FAIT PAS, ET POURQUOI
//
// On ne retire PAS les constantes d'EVENEMENTS (KeyPress, Expose,
// ConfigureNotify...). Le code de fenetrage X11 en a besoin : les supprimer
// deplacerait simplement la casse vers NkXLibWindow.cpp et consorts. Seuls
// sont retires les noms qui entrent en conflit avec du C++ ordinaire.
//
// USAGE : inclure ce fichier APRES les en-tetes X11, dans les fichiers dont le
// contenu est ensuite mele a du code moteur (contexte graphique, RHI). Les
// fichiers purement X11 ne doivent PAS l'inclure.
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
// Cette liste est VOLONTAIREMENT courte : chaque ajout retire un outil au code
// X11 legitime. On n'y met un nom que lorsqu'une collision reelle est constatee.
#ifdef None
#undef None // collisionne avec NkGuiInteract::None, NkGuiEdge::None, etc.
#endif
#ifdef Bool
#undef Bool
#endif
#ifdef Status
#undef Status
#endif
#ifdef Success
#undef Success
#endif
#ifdef Always
#undef Always
#endif
#ifdef Complex
#undef Complex
#endif
#ifdef Convex
#undef Convex
#endif

#endif // Linux / BSD
