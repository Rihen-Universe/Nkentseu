// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#pragma once
// =============================================================================
// NkXLibPressePapiers.h : LE PRESSE-PAPIERS X11 (sélection CLIPBOARD), Xlib seule (21/10).
//
// Un apprenant (Ubuntu 24.04, GNOME en session Wayland, NKCode 1.4.0) : « Ctrl+C et Ctrl+V ne
// marchent pas ». La cause, lue : hors de Windows, NkWindow::GetClipboardText/SetClipboardText
// (Core/NkWindowClipboard.cpp) n'etaient qu'une chaine INTERNE au processus. Copier dans NKCode
// et coller dans NKCode marchait ; copier ailleurs et coller dans NKCode, ou l'inverse, jamais.
// La 1.5.2 ne change rien a ce fichier : elle ne corrige pas le defaut.
//
// Ce fichier parle le protocole des selections d'X11 (ICCCM 2.6) pour la selection CLIPBOARD,
// celle de Ctrl+C / Ctrl+V. Sous GNOME en session Wayland, NKCode (dorsal XLib, le defaut)
// tourne sous XWayland, qui relie CLIPBOARD au presse-papiers de Wayland : le meme code sert.
//   - POSSEDER : XSetSelectionOwner ; puis repondre a chaque SelectionRequest (TARGETS,
//     UTF8_STRING, STRING, TEXT) en ecrivant la propriete demandee chez le demandeur.
//   - LIRE : XConvertSelection vers UTF8_STRING (repli STRING), attendre SelectionNotify
//     (delai borne, `select` sur la connexion), lire la propriete.
// Limite ecrite : le transfert INCR (texte plus gros que la requete maximale du serveur, des
// Mo) n'est pas lu ; la lecture rend alors faux, comme avant le texte n'est pas colle.
// Aucune dependance hors de Xlib et de <sys/select.h> : un banc l'eprouve seul, sous Xvfb
// (Applications/NKCode/tools/banc_presse_papiers_x11.sh).
// =============================================================================
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <sys/select.h>
#include <sys/time.h>

namespace nkentseu {
	namespace xlibpp {

		// (22/10) LE « None » D'X11, SOUS UN NOM A NOUS. X.h le definit en macro (#define None 0L) ;
		// NKPlatform/NkX11Clean.h la RETIRE (#undef None : collision avec NkGuiInteract::None...). Inclus
		// par NkXLibWindow.h apres lui, ce fichier ne voyait plus None : son second #include <X11/Xlib.h>
		// ne redefinit rien (garde d'inclusion). La construction Linux de NKCode 1.5.3 s'arretait sur
		// 15 erreurs « use of undeclared identifier 'None' » (NKEditorKit, 3 fichiers). Le banc isole
		// (banc_presse_papiers_x11.sh) incluait Xlib seule : il ne pouvait pas le voir.
		// Atom et Window sont des unsigned long (XID) : la constante a leur type, valeur 0L comme None.
		constexpr unsigned long NkPpAucun = 0L;

		struct NkPpAtomes {
				::Atom clipboard = 0, utf8 = 0, targets = 0, text = 0, chaine = 0, propriete = 0, incr = 0;
		};

		inline void NkPpInit(::Display *d, NkPpAtomes &a) {
			a.clipboard = XInternAtom(d, "CLIPBOARD", False);
			a.utf8 = XInternAtom(d, "UTF8_STRING", False);
			a.targets = XInternAtom(d, "TARGETS", False);
			a.text = XInternAtom(d, "TEXT", False);
			a.chaine = XA_STRING;
			a.propriete = XInternAtom(d, "NK_PRESSE_PAPIERS", False);
			a.incr = XInternAtom(d, "INCR", False);
		}

		/// Devenir le proprietaire de CLIPBOARD. Rend vrai si le serveur l'a bien donne.
		inline bool NkPpPrendre(::Display *d, ::Window w, const NkPpAtomes &a) {
			XSetSelectionOwner(d, a.clipboard, w, CurrentTime);
			XFlush(d);
			return XGetSelectionOwner(d, a.clipboard) == w;
		}

		/// Repondre a une demande (on POSSEDE la selection) avec le texte UTF-8 [texte, texte+n).
		/// Rend vrai si la demande visait CLIPBOARD (traitee, acceptee ou refusee).
		inline bool NkPpRepondre(::Display *d, const XSelectionRequestEvent &req, const NkPpAtomes &a, const char *texte,
								 unsigned long n) {
			if (req.selection != a.clipboard)
				return false;
			XSelectionEvent rep = {};
			rep.type = SelectionNotify;
			rep.display = req.display;
			rep.requestor = req.requestor;
			rep.selection = req.selection;
			rep.target = req.target;
			rep.time = req.time;
			// un demandeur d'avant l'ICCCM met None : on ecrit alors dans la propriete du nom de la cible
			const ::Atom prop = req.property != NkPpAucun ? req.property : req.target;
			rep.property = NkPpAucun; // refus, sauf si une cible connue est servie
			if (req.target == a.targets) {
				const ::Atom cibles[4] = {a.targets, a.utf8, a.chaine, a.text};
				XChangeProperty(d, req.requestor, prop, XA_ATOM, 32, PropModeReplace,
								reinterpret_cast<const unsigned char *>(cibles), 4);
				rep.property = prop;
			} else if (req.target == a.utf8 || req.target == a.text || req.target == a.chaine) {
				// STRING est du Latin-1 au sens strict ; les applications actuelles lisent UTF8_STRING.
				// Pour STRING on donne les memes octets (ce que font GLFW et SDL).
				XChangeProperty(d, req.requestor, prop, req.target == a.text ? a.utf8 : req.target, 8, PropModeReplace,
								reinterpret_cast<const unsigned char *>(texte), static_cast<int>(n));
				rep.property = prop;
			}
			XSendEvent(d, req.requestor, False, NoEventMask, reinterpret_cast<XEvent *>(&rep));
			XFlush(d);
			return true;
		}

		/// Attendre (au plus `delaiMs`) l'evenement SelectionNotify de CLIPBOARD destine a `w`.
		inline bool NkPpAttendre(::Display *d, ::Window w, const NkPpAtomes &a, int delaiMs, XSelectionEvent &sortie) {
			const int fd = ConnectionNumber(d);
			int reste = delaiMs;
			for (;;) {
				XEvent ev;
				while (XCheckTypedWindowEvent(d, w, SelectionNotify, &ev)) {
					if (ev.xselection.selection == a.clipboard) {
						sortie = ev.xselection;
						return true;
					}
					// une autre selection (le glisser-deposer : XdndSelection) : on la laisse a la boucle
					XPutBackEvent(d, &ev);
					break;
				}
				if (reste <= 0)
					return false;
				fd_set fds;
				FD_ZERO(&fds);
				FD_SET(fd, &fds);
				timeval tv;
				const int pas = reste < 20 ? reste : 20;
				tv.tv_sec = 0;
				tv.tv_usec = pas * 1000;
				select(fd + 1, &fds, nullptr, nullptr, &tv);
				XFlush(d);
				reste -= pas;
			}
		}

		/// Lire CLIPBOARD (d'une AUTRE application : si `w` la possede, l'appelant rend son propre
		/// texte). Le contenu est rendu a `recevoir(ctx, octets, n)` UNE fois. Rend faux si personne
		/// ne possede la selection, si le proprietaire refuse, ne repond pas dans le delai, ou passe
		/// par INCR (non pris en charge, voir l'en-tete).
		inline bool NkPpLire(::Display *d, ::Window w, const NkPpAtomes &a, int delaiMs,
							 void (*recevoir)(void *ctx, const unsigned char *octets, unsigned long n), void *ctx) {
			if (XGetSelectionOwner(d, a.clipboard) == NkPpAucun)
				return false;
			const ::Atom cibles[2] = {a.utf8, a.chaine};
			for (int k = 0; k < 2; ++k) {
				XDeleteProperty(d, w, a.propriete);
				XConvertSelection(d, a.clipboard, cibles[k], a.propriete, w, CurrentTime);
				XFlush(d);
				XSelectionEvent rep;
				if (!NkPpAttendre(d, w, a, delaiMs, rep))
					return false;
				if (rep.property == NkPpAucun)
					continue; // cible refusee : la suivante
				::Atom type = NkPpAucun;
				int format = 0;
				unsigned long nb = 0, apres = 0;
				unsigned char *donnees = nullptr;
				// longueur en mots de 32 bits : jusqu'a 64 Mo, la limite d'une propriete lue d'un coup
				if (XGetWindowProperty(d, w, a.propriete, 0, 0x1000000L, True, AnyPropertyType, &type, &format, &nb, &apres,
									   &donnees) != Success)
					return false;
				const bool incr = type == a.incr;
				if (!incr && donnees && format == 8)
					recevoir(ctx, donnees, nb);
				if (donnees)
					XFree(donnees);
				return !incr && format == 8;
			}
			return false;
		}

	} // namespace xlibpp
} // namespace nkentseu
