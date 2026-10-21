// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#pragma once
// =============================================================================
// NkXLibSaisie.h : LE TEXTE TAPE SOUS X11 : accents, touches mortes, œ (21/10).
//
// Rodolf : « pourquoi on ne peut pas ecrire dans NKCode en utilisant des accents ? ». Sous
// Linux, le dorsal XLib (le defaut) lisait la frappe par XLookupString : du Latin-1 seulement
// (pas de « œ », pas de « € »), AUCUNE touche morte (« ^ » puis « e » donnait « e »), et seul le
// premier octet partait. Ici : la methode de saisie d'Xlib (XOpenIM, un XIC par fenetre),
// XFilterEvent pour les touches mortes et la touche Compose, et Xutf8LookupString, qui rend de
// l'UTF-8 ; chaque point de code devient un NkTextInputEvent. Sans methode de saisie (serveur
// sans locale prise en charge), le repli est XLookupString, comme avant, mais tous ses octets.
// Aucune dependance hors de Xlib : un banc l'eprouve seul, sous Xvfb, disposition francaise
// (Applications/NKCode/tools/banc_saisie_x11.sh).
// =============================================================================
#include <X11/Xlib.h>
#include <X11/Xutil.h>

namespace nkentseu {
	namespace xlibsaisie {

		/// La methode de saisie LOCALE d'Xlib (touches mortes, Compose), une fois par connexion.
		/// Rend nullptr si le serveur et la locale n'en offrent pas : la frappe passe alors par le repli.
		inline XIM NkXsOuvrir(::Display *d) {
			if (!d)
				return nullptr;
			XSetLocaleModifiers("");
			XIM im = XOpenIM(d, nullptr, nullptr, nullptr);
			if (!im) { // aucune methode nommee par XMODIFIERS : la methode interne d'Xlib
				XSetLocaleModifiers("@im=none");
				im = XOpenIM(d, nullptr, nullptr, nullptr);
			}
			return im;
		}

		/// Un contexte de saisie pour la fenetre `w` (sans pre-edition dessinee : NKGui l'ignore).
		inline XIC NkXsContexte(XIM im, ::Window w) {
			if (!im || !w)
				return nullptr;
			return XCreateIC(im, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, w, XNFocusWindow, w,
							 nullptr);
		}

		/// Decode `n` octets UTF-8 et rend chaque point de code IMPRIMABLE a `emettre` (les commandes
		/// C0, DEL et C1 sont laissees aux evenements de touche). Une sequence invalide est sautee.
		inline int NkXsEmettreUtf8(const char *s, int n, void (*emettre)(void *, unsigned int), void *ctx) {
			int nb = 0;
			for (int i = 0; i < n;) {
				const unsigned char c = static_cast<unsigned char>(s[i]);
				unsigned int cp = 0;
				int len = 0;
				if (c < 0x80u) {
					cp = c;
					len = 1;
				} else if ((c & 0xE0u) == 0xC0u) {
					cp = c & 0x1Fu;
					len = 2;
				} else if ((c & 0xF0u) == 0xE0u) {
					cp = c & 0x0Fu;
					len = 3;
				} else if ((c & 0xF8u) == 0xF0u) {
					cp = c & 0x07u;
					len = 4;
				} else {
					++i;
					continue;
				}
				if (i + len > n)
					break;
				bool ok = true;
				for (int k = 1; k < len; ++k) {
					const unsigned char t = static_cast<unsigned char>(s[i + k]);
					if ((t & 0xC0u) != 0x80u) {
						ok = false;
						break;
					}
					cp = (cp << 6) | (t & 0x3Fu);
				}
				i += ok ? len : 1;
				if (!ok || cp < 0x20u || cp == 0x7Fu || (cp >= 0x80u && cp < 0xA0u))
					continue;
				emettre(ctx, cp);
				++nb;
			}
			return nb;
		}

		/// Le texte d'une frappe (KeyPress DEJA passe par XFilterEvent) : rend le nombre de points de
		/// code emis. Avec un XIC : Xutf8LookupString. Sans : XLookupString (Latin-1, octet = point de code).
		inline int NkXsTexte(XIC ic, XKeyEvent *k, void (*emettre)(void *, unsigned int), void *ctx) {
			char buf[64];
			KeySym ks = 0;
			if (ic) {
				Status st = 0;
				const int n = Xutf8LookupString(ic, k, buf, static_cast<int>(sizeof(buf)) - 1, &ks, &st);
				if (st != XLookupChars && st != XLookupBoth)
					return 0; // rien a ecrire (touche seule), ou trop long pour une frappe (XBufferOverflow)
				return NkXsEmettreUtf8(buf, n, emettre, ctx);
			}
			const int n = XLookupString(k, buf, static_cast<int>(sizeof(buf)) - 1, &ks, nullptr);
			int nb = 0;
			for (int i = 0; i < n; ++i) {
				const unsigned int cp = static_cast<unsigned char>(buf[i]);
				if (cp < 0x20u || cp == 0x7Fu || (cp >= 0x80u && cp < 0xA0u))
					continue;
				emettre(ctx, cp);
				++nb;
			}
			return nb;
		}

	} // namespace xlibsaisie
} // namespace nkentseu
