// =============================================================================
// NkAspirateur — parcourir un site public et en rapporter le texte.
// -----------------------------------------------------------------------------
// CE QUE FAIT CE FICHIER : partir d'une adresse, télécharger la page, en extraire
// l'article, relever les liens qui restent sur le même site, recommencer — jusqu'à
// une limite qu'on s'impose.
//
// LES QUATRE REGLES QUI NE SONT PAS DES SCRUPULES MAIS DES CONDITIONS DE SUCCES.
// Un aspirateur qui les ignore se fait bloquer en quelques minutes, et tout est à
// refaire depuis une adresse désormais bannie :
//
//  1. RESTER SUR LE DOMAINE demandé. Suivre les liens sortants transforme la
//     collecte d'un site en parcours sans fin de l'internet entier.
//  2. RESPECTER robots.txt. C'est la façon dont un site déclare ce qu'il accepte
//     de voir parcourir. Passer outre est le moyen le plus sûr d'être bloqué —
//     et de mériter de l'être.
//  3. ESPACER LES REQUETES. Un site personnel tourne souvent sur une machine
//     modeste ; enchaîner les demandes sans pause revient à l'attaquer.
//  4. PLAFONNER. Un site peut avoir des pages infinies (calendriers, filtres de
//     recherche). Sans plafond, la collecte ne s'arrête jamais.
//
// SE DECLARER. La requête annonce qui elle est. Un administrateur qui voit passer
// un visiteur identifié peut le contacter ou l'autoriser ; un visiteur anonyme et
// insistant est bloqué sans discussion.
//
// CE QUE CE FICHIER NE FAIT PAS : il ne contourne rien. Ni authentification, ni
// blocage, ni page dont le contenu est construit par du script une fois affichée
// — ces dernières reviennent vides, et il le dit plutôt que de rendre une page
// blanche silencieuse.
// AUTEUR : Rihen — LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#ifndef NK_MEDIA_ASPIRATEUR_H
#define NK_MEDIA_ASPIRATEUR_H

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKMedia/Document/NkPageWeb.h"

namespace nkentseu {
	namespace media {

		struct ReglesAspiration {
			int64 maxPages = 200;	// plafond dur
			int64 delaiMs = 1000;	// pause entre deux requetes
			int32 profondeurMax = 3; // liens suivis depuis la page de depart
			nk_size longueurMini = 180;
		};

		struct PageAspiree {
			NkString url;
			NkString texte;
			int64 blocsGardes = 0;
			int64 blocsJetes = 0;
		};

		// ── Adresses ─────────────────────────────────────────────────────────
		// Découpage minimal : schéma, hôte, chemin. Suffisant pour rester sur un
		// domaine et pour résoudre les liens relatifs, qui sont l'immense majorité.
		inline NkString HoteDe(const NkString &url) {
			nk_size deb = url.Find("://");
			deb = (deb == NkString::npos) ? 0 : deb + 3;
			nk_size fin = deb;
			while (fin < url.Size() && url.Data()[fin] != '/' && url.Data()[fin] != '?' &&
				   url.Data()[fin] != '#')
				++fin;
			return url.SubStr(deb, fin - deb);
		}

		inline NkString RacineDe(const NkString &url) {
			const nk_size deb = url.Find("://");
			const NkString schema = (deb == NkString::npos) ? NkString("https://") : url.SubStr(0, deb + 3);
			NkString r = schema;
			r.Append(HoteDe(url));
			return r;
		}

		// Résout un lien éventuellement relatif contre la page qui le porte.
		// Rend une chaîne vide pour ce qu'on ne suit pas : ancres, courriels,
		// scripts, et fichiers qui ne sont pas des pages.
		inline NkString ResoudreLien(const NkString &base, const NkString &lien) {
			if (lien.Size() == 0)
				return NkString();
			if (lien.Data()[0] == '#')
				return NkString();
			const char *rejets[] = {"mailto:", "javascript:", "tel:", "data:"};
			for (nk_size i = 0; i < 4; ++i)
				if (lien.Size() > 4 && lien.Find(rejets[i]) == 0)
					return NkString();
			// Extensions qui ne sont pas des pages. Le PDF est exclu ici parce que
			// la collecte vise le texte des pages ; un PDF se depose a la main,
			// ou l'on sait ce qu'on prend.
			const char *bin[] = {".jpg", ".jpeg", ".png", ".gif",  ".svg", ".webp", ".zip",
								 ".mp4", ".mp3",  ".css", ".js",   ".ico", ".woff", ".pdf"};
			for (nk_size i = 0; i < 14; ++i) {
				const nk_size l = NkString(bin[i]).Size();
				if (lien.Size() > l) {
					NkString ext = lien.SubStr(lien.Size() - l);
					for (nk_size k = 0; k < ext.Size(); ++k) {
						char *d = (char *)ext.Data();
						if (d[k] >= 'A' && d[k] <= 'Z')
							d[k] = (char)(d[k] - 'A' + 'a');
					}
					if (ext == NkString(bin[i]))
						return NkString();
				}
			}

			NkString abs;
			if (lien.Find("http://") == 0 || lien.Find("https://") == 0)
				abs = lien;
			else if (lien.Size() > 1 && lien.Data()[0] == '/')
				abs = RacineDe(base) + lien;
			else {
				// Relatif au dossier de la page courante.
				NkString b = base;
				const nk_size q = b.Find("?");
				if (q != NkString::npos)
					b = b.SubStr(0, q);
				const nk_size slash = b.RFind("/");
				const nk_size schema = b.Find("://");
				if (slash != NkString::npos && (schema == NkString::npos || slash > schema + 2))
					b = b.SubStr(0, slash + 1);
				else
					b.Append('/');
				abs = b + lien;
			}
			// L'ancre ne designe pas une autre page : deux adresses qui n'en
			// different que par elle seraient telechargees deux fois.
			const nk_size diese = abs.Find("#");
			if (diese != NkString::npos)
				abs = abs.SubStr(0, diese);
			return abs;
		}

		// Relève les href d'une page. Volontairement littéral : on ne cherche pas
		// à comprendre le HTML, seulement à trouver les liens.
		inline void ReleverLiens(const NkString &html, const NkString &base, NkVector<NkString> &out) {
			out.Clear();
			nk_size i = 0;
			while (i < html.Size()) {
				const nk_size h = html.Find("href", i);
				if (h == NkString::npos)
					break;
				nk_size p = h + 4;
				while (p < html.Size() && (html.Data()[p] == ' ' || html.Data()[p] == '='))
					++p;
				if (p >= html.Size()) {
					break;
				}
				const char guillemet = html.Data()[p];
				if (guillemet != '"' && guillemet != '\'') {
					i = h + 4;
					continue;
				}
				const nk_size deb = p + 1;
				nk_size fin = deb;
				while (fin < html.Size() && html.Data()[fin] != guillemet)
					++fin;
				if (fin >= html.Size())
					break;
				const NkString lien = ResoudreLien(base, html.SubStr(deb, fin - deb));
				if (lien.Size() > 0)
					out.PushBack(lien);
				i = fin + 1;
			}
		}

		// ── robots.txt ───────────────────────────────────────────────────────
		// Lecture volontairement stricte et simple : on ne retient que les
		// « Disallow: » qui s'appliquent a tout le monde (User-agent: *). En cas
		// de doute sur la portee d'une regle, on la prend pour soi — se croire
		// autorise a tort coute plus cher que s'interdire un chemin de trop.
		inline void LireRobots(const NkString &robots, NkVector<NkString> &interdits) {
			interdits.Clear();
			bool pourTous = false;
			nk_size i = 0;
			while (i < robots.Size()) {
				nk_size fin = robots.Find("\n", i);
				if (fin == NkString::npos)
					fin = robots.Size();
				NkString ligne = robots.SubStr(i, fin - i);
				i = fin + 1;
				while (ligne.Size() > 0 && (ligne.Data()[ligne.Size() - 1] == '\r' ||
											ligne.Data()[ligne.Size() - 1] == ' '))
					ligne = ligne.SubStr(0, ligne.Size() - 1);
				if (ligne.Size() == 0 || ligne.Data()[0] == '#')
					continue;

				if (ligne.Find("User-agent:") == 0 || ligne.Find("User-Agent:") == 0) {
					NkString v = ligne.SubStr(11);
					while (v.Size() > 0 && v.Data()[0] == ' ')
						v = v.SubStr(1);
					pourTous = (v == NkString("*"));
					continue;
				}
				if (pourTous && ligne.Find("Disallow:") == 0) {
					NkString v = ligne.SubStr(9);
					while (v.Size() > 0 && v.Data()[0] == ' ')
						v = v.SubStr(1);
					if (v.Size() > 0)
						interdits.PushBack(v);
				}
			}
		}

		inline bool CheminInterdit(const NkString &url, const NkVector<NkString> &interdits) {
			// On compare sur le chemin, pas sur l'adresse entiere.
			nk_size deb = url.Find("://");
			deb = (deb == NkString::npos) ? 0 : deb + 3;
			nk_size slash = deb;
			while (slash < url.Size() && url.Data()[slash] != '/')
				++slash;
			const NkString chemin = (slash < url.Size()) ? url.SubStr(slash) : NkString("/");
			for (nk_size i = 0; i < interdits.Size(); ++i)
				if (chemin.Find(interdits[i].CStr()) == 0)
					return true;
			return false;
		}

	} // namespace media
} // namespace nkentseu

#endif // NK_MEDIA_ASPIRATEUR_H
