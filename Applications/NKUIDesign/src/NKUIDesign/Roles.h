#pragma once
// -----------------------------------------------------------------------------
// @File    Roles.h
// @Brief   D'UN NOM DE ROLE DECLARE VERS UN IDENTIFIANT DE THEME -- et ce qui se
//          passe quand ca rate.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  LE DEFAUT QUI A DONNE CE FICHIER, ET POURQUOI ON NE RENOMME PAS DIX JETONS
// =============================================================================
//  Premier temoin visuel de NkUIDesign, 18/08 : le navigateur de contenu s'est
//  peint en MAGENTA FRANC. Cause lue dans le code : les noms canoniques de
//  `themedetail::RoleNames()` sont en snake_case (« panel_bg »), `NkResolveRole`
//  compare OCTET POUR OCTET, et les `defaultRole` declares sont en PascalCase
//  (« PanelBg »). Aucun ne tombe juste -> NK_ROLE_INVALID -> `NkTheme::Get`
//  rend son magenta de repli.
//
//  ⚠️ LA MESURE DIT QUE CE N'EST PAS UN OUBLI ISOLE : 10 jetons sur 10 dans
//     `NkContentBrowserModel.h`, 13 sur 13 dans `NkTreeViewModel.h`. **23 sur
//     23.** Deux fichiers, deux auteurs, zero exception. Renommer 23 chaines
//     laisserait le vingt-quatrieme jeton refaire exactement la meme erreur --
//     et c'est la lecon deja payee par ce depot : une convention que l'auteur
//     doit CONNAITRE pour l'appliquer sera enfreinte par le prochain auteur.
//
//  DEUX REPONSES, ET IL FAUT LES DEUX. Elles ne se remplacent pas :
//
//   (a) **LA RESOLUTION CANONISE** -- elle accepte les deux ecritures et les
//       ramene a la forme canonique. L'auteur d'un composant n'a plus a
//       connaitre la convention : la classe de defaut disparait.
//   (b) **LE REPLI DEVIENT FRANC** -- un role qui ne resout toujours pas n'est
//       plus peint en magenta *en silence* : il est COMPTE, NOMME, journalise au
//       demarrage, et l'apercu affiche un bandeau qui donne le nom fautif.
//       (a) supprime la classe de defaut ; (b) garantit qu'on verra la
//       prochaine, celle que (a) ne couvre pas.
//
//  ⚠️ OU CE CODE DEVRAIT VIVRE, ET IL NE VIT PAS ICI PAR CHOIX. Le bon foyer de
//     (a) est `NkRoleRegistry::Find` (`NKEditorKit/NkTheme.inl`) : la canoniser
//     LA la rendrait vraie pour Nogee, NK3DModeler, NkAnimaEditor et PV3DE, qui
//     vont tous rencontrer ce magenta. `NkTheme` n'est pas le perimetre de cet
//     agent -- la fonction est donc ecrite ici, PURE et EPROUVEE (famille 33 de
//     la sonde), et proposee telle quelle au canal pour etre deplacee. Tant
//     qu'elle est ici, elle ne protege QUE NkUIDesign, et c'est exactement le
//     calcul de cout que ce depot a deja mesure : ecrire chez soi coute moins
//     cher qu'adopter. Il est nomme, pas resolu.
//
//  ⚠️ ORDRE DES DEUX TENTATIVES, ET IL N'EST PAS INDIFFERENT : le nom BRUT est
//     essaye EN PREMIER, la forme canonisee ensuite. Une application peut
//     enregistrer ses propres roles (`NkRoleRegistry::Register`) sous n'importe
//     quelle graphie ; canoniser d'abord ferait manquer un role d'extension
//     legitimement nomme « MonRole ». Dans cet ordre, la canonisation ne peut
//     QUE rattraper -- elle ne peut casser aucun nom qui resolvait deja.
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKEditorKit/NkTheme.h"

#include <cstdio>

namespace nkuidesign {

	using nkentseu::int32;
	using nkentseu::NkString;
	using nkentseu::NkVector;
	using nkentseu::uint16;
	using nkentseu::uint32;
	using nkentseu::uint8;
	using nkentseu::editorkit::NK_ROLE_INVALID;
	using nkentseu::editorkit::NkResolveRole;

	// ── (a) LA CANONISATION ─────────────────────────────────────────────────
	// PascalCase / camelCase -> snake_case. PURE : aucune allocation, aucun
	// etat, aucune dependance au theme. C'est ce qui la rend deplacable telle
	// quelle dans `NkTheme.inl`.
	//
	// REGLE, en une phrase : on insere un « _ » devant toute MAJUSCULE qui suit
	// une minuscule ou un chiffre, puis on met tout en minuscules.
	//
	//   PanelBg      -> panel_bg          TextOnAccent -> text_on_accent
	//   PanelHeader  -> panel_header      AccentUi     -> accent_ui
	//   TypeFolder   -> type_folder       panel_bg     -> panel_bg   (inchange)
	//   nk3d.AnneauBrosse -> nk3d.anneau_brosse
	//
	// ⚠️ SA LIMITE, ECRITE AVEC ELLE PLUTOT QUE DECOUVERTE PLUS TARD : un
	//    ACRONYME colle ne se coupe pas. « NKThing » donne « nkthing », pas
	//    « nk_thing » -- la regle ne peut pas savoir ou finit l'acronyme. Aucun
	//    des 30 roles du coeur n'est dans ce cas ; le jour ou l'un le sera, le
	//    repli franc (b) le dira au lieu de le peindre en magenta.
	//
	// Rend `false` si l'entree est nulle, vide, ou si le resultat ne tient pas
	// dans `cap` -- une troncature silencieuse fabriquerait un nom qui ne resout
	// pas et deplacerait le defaut au lieu de le signaler.
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
					return false;
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

	// ── (b) LE REPLI FRANC ──────────────────────────────────────────────────
	// Ce que le magenta ne disait pas : QUEL role. Une couleur criarde dit qu'il
	// y a un probleme ; elle ne dit ni lequel, ni combien, ni ou. Ce registre le
	// dit, et il le dit A TROIS ENDROITS -- journal au demarrage, bandeau dans
	// l'apercu, rapport de sonde -- parce qu'aucun des trois n'est toujours lu.
	//
	// ⚠️ IL RETIENT AUSSI LES RATTRAPAGES (les noms que la canonisation a
	//    sauves), et ce n'est pas de la decoration : c'est la LISTE DE TRAVAIL
	//    de la correction a la source. Sans elle, (a) rendrait le defaut
	//    invisible et les declarations resteraient fausses pour toujours --
	//    « une protection qui empeche d'aller verifier ».
	class NkRoleAudit {
		public:
			struct Entry {
					NkString name;  ///< le nom tel qu'il est DECLARE
					NkString canon; ///< la forme qui a resolu, vide si aucune
			};

			/// Un nom qui n'a resolu sous AUCUNE forme. C'est le magenta.
			static NkVector<Entry> &Faults() {
				static NkVector<Entry> v;
				return v;
			}
			/// Un nom qui n'a resolu qu'APRES canonisation : la declaration est a
			/// corriger a la source, mais l'ecran est juste.
			static NkVector<Entry> &Rescued() {
				static NkVector<Entry> v;
				return v;
			}

			static void Reset() {
				Faults().Clear();
				Rescued().Clear();
			}
			static uint32 FaultCount() {
				return (uint32)Faults().Size();
			}
			static uint32 RescuedCount() {
				return (uint32)Rescued().Size();
			}

			/// Une ligne lisible par un humain, pour le journal et le bandeau.
			/// Bornee : un bandeau qui deborde ne se lit pas, et une liste de 23
			/// noms n'apprend rien de plus que les cinq premiers plus le compte.
			static void Summary(NkString &out, uint32 maxNames = 5) {
				out = NkString("");
				char b[128];
				snprintf(b, sizeof(b), "%u role(s) NON RESOLU(S), %u rattrape(s) par canonisation",
						 FaultCount(), RescuedCount());
				out.Append(b);
				AppendList(out, Faults(), "  |  non resolus : ", maxNames, false);
				AppendList(out, Rescued(), "  |  a corriger a la source : ", maxNames, true);
			}

			// ⚠️ AJOUT DEDUPLIQUE. Le dessin passe par ici a CHAQUE image : sans
			//    deduplication, le registre grossirait de 23 entrees par image et
			//    la fuite se presenterait comme un ralentissement, jamais comme un
			//    defaut de roles.
			static void Note(NkVector<Entry> &into, const char *name, const char *canon) {
				for (uint32 i = 0; i < (uint32)into.Size(); ++i)
					if (Same(into[i].name.Data(), name))
						return;
				Entry e;
				e.name = NkString(name ? name : "");
				e.canon = NkString(canon ? canon : "");
				into.PushBack(e);
			}

		private:
			static bool Same(const char *a, const char *b) {
				if (!a || !b)
					return a == b;
				for (; *a && *b; ++a, ++b)
					if (*a != *b)
						return false;
				return *a == *b;
			}
			static void AppendList(NkString &out, const NkVector<Entry> &v, const char *lead,
								   uint32 maxNames, bool withCanon) {
				if (v.Size() == 0)
					return;
				out.Append(lead);
				const uint32 n = (uint32)v.Size();
				const uint32 shown = n < maxNames ? n : maxNames;
				for (uint32 i = 0; i < shown; ++i) {
					if (i)
						out.Append(", ");
					out.Append(v[i].name);
					if (withCanon && !v[i].canon.Empty()) {
						out.Append("->");
						out.Append(v[i].canon);
					}
				}
				if (n > shown) {
					char b[48];
					snprintf(b, sizeof(b), " (+%u)", n - shown);
					out.Append(b);
				}
			}
	};

	// ── LA RESOLUTION DE L'APPLICATION ──────────────────────────────────────
	// ⚠️ C'EST LA SEULE RESOLUTION DU PROGRAMME. L'editeur fenetre ET la sonde
	//    headless passent tous les deux par cette fonction, et c'est la
	//    correction du defaut le plus cher du 18/08 : la sonde avait SA propre
	//    resolution (un hachage vers 1..250 qui ne rendait jamais
	//    NK_ROLE_INVALID), donc un resolveur qui disait oui a tout. 68 essais
	//    verts n'ont pas pu voir un magenta plein ecran. Deux resolveurs, c'est
	//    une sonde qui mesure autre chose que ce que l'ecran montre.
	inline uint16 NkDesignResolveRole(const char *roleName) {
		if (!roleName || !*roleName)
			return NK_ROLE_INVALID;
		const uint16 direct = NkResolveRole(roleName);
		if (direct != NK_ROLE_INVALID)
			return direct;
		char canon[96];
		if (NkCanonicalRoleName(roleName, canon, sizeof(canon))) {
			const uint16 id = NkResolveRole(canon);
			if (id != NK_ROLE_INVALID) {
				NkRoleAudit::Note(NkRoleAudit::Rescued(), roleName, canon);
				return id;
			}
		}
		NkRoleAudit::Note(NkRoleAudit::Faults(), roleName, "");
		return NK_ROLE_INVALID;
	}

} // namespace nkuidesign
