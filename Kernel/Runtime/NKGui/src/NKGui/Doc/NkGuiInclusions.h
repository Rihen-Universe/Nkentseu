// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#pragma once
// =============================================================================
// NkGuiInclusions.h — `include` : un document en appelle un autre.
// =============================================================================
//  `include` était au vocabulaire du format **depuis toujours** — l'une des huit
//  sections connues — et **rien ne le lisait**. Le validateur l'acceptait, le
//  monteur le sautait avec `geometry`, `controller`, `callback` et `fonts`. Un
//  auteur pouvait donc l'écrire, voir son document validé, et ne jamais
//  comprendre pourquoi il ne se passait rien.
//
//  Rodolf, le 26/09 : « faut le lier ».
//
// =============================================================================
//  À QUOI ÇA SERT, ET CE N'EST PAS DU CONFORT
// =============================================================================
//  Un composant n'a de valeur que s'il se PARTAGE. Tant qu'il faut le recopier
//  dans chaque document, on a renommé la copie-colle : la première retouche
//  désynchronise les copies. `include` est ce qui transforme un composant en
//  **bibliothèque** — un fichier de composants, inclus par tous ceux qui s'en
//  servent, corrigé à un seul endroit.
//
//      // composants/boutons.nkgui
//      component "BoutonOutil" { widgets { Button "b" { ... } } }
//
//      // panneau.nkgui
//      include "composants/boutons.nkgui"
//      widgets { HBox "barre" { BoutonOutil "outil.deplacer" { ... } } }
//
// =============================================================================
//  LA SYNTAXE N'EST PAS NEUVE — c'est celle de tous les blocs
// =============================================================================
//      include "chemin/relatif.nkgui"
//
//  L'archive lit déjà `Type "identifiant"` : le type est `include`, et le chemin
//  EST l'identifiant. Aucune grammaire à ajouter, aucun lecteur à retoucher.
//
// =============================================================================
//  CE QUI EST INCLUS, ET CE QUI NE L'EST PAS
// =============================================================================
//  Toutes les sections du document inclus sont ajoutées à celui qui l'inclut :
//  ses `component`, ses `widgets`, ses `behavior`. **Sauf sa ligne de version**,
//  qui appartient au fichier, pas à son contenu.
//
//  ⚠️ L'ORDRE COMPTE, ET IL EST CELUI DU TEXTE. Le contenu inclus prend la place
//     exacte du `include`. Un `include` écrit avant `widgets` fournit donc ses
//     composants à temps ; écrit après, ses composants arriveraient trop tard --
//     et le développement les compterait comme des rôles inconnus. On ne réordonne
//     pas pour rattraper l'auteur : *un outil qui devine masque la faute au lieu
//     de la montrer*.
//
// =============================================================================
//  LES QUATRE REFUS, ET CHACUN SE COMPTE
// =============================================================================
//   1. FICHIER INTROUVABLE -> refus nommé, avec le chemin. Jamais un document
//      silencieusement amputé.
//   2. FICHIER ILLISIBLE (erreur de syntaxe) -> refus nommé, avec la raison du
//      lecteur.
//   3. CYCLE (a inclut b qui inclut a) -> refusé, pas limité. Une profondeur
//      maximale en ferait un document à moitié inclus, donc un défaut silencieux.
//   4. AUCUN DOSSIER DE BASE -> `Adopter` prend un ARBRE, pas un chemin : il ne
//      peut résoudre aucune inclusion. Les `include` restants sont alors COMPTÉS
//      comme non résolus. C'est la conséquence assumée de la façade, pas un
//      oubli : *le monteur reste la seule implémentation de la sémantique*, et
//      un arbre sans provenance n'a pas de « à côté de moi ».
// =============================================================================

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKFileSystem/NkFile.h"
#include "NKSerialization/NkGui/NkGuiArchive.h"

namespace nkentseu {
	namespace nkgui {

		/// Ce que la résolution a fait, et ce qu'elle a REFUSÉ de faire.
		struct NkGuiRapportInclusions {
				uint32 resolues = 0;	 ///< documents inclus avec succès
				uint32 introuvables = 0; ///< le fichier n'existe pas ou est vide
				uint32 illisibles = 0;	 ///< le fichier existe et ne s'analyse pas
				uint32 cycles = 0;		 ///< a inclut b qui inclut a
				uint32 nonResolues = 0;	 ///< `include` rencontré sans dossier de base
				uint32 sectionsApportees = 0; ///< sections ajoutées au document
				NkVector<NkString> refuses;	  ///< chaque refus, nommé

				bool Propre() const noexcept {
					return introuvables == 0u && illisibles == 0u && cycles == 0u
						   && nonResolues == 0u;
				}
		};

		namespace detail {

			inline bool NkGIMotEgal(NkStringView a, const char *b) noexcept {
				const uint32 n = (uint32)a.Size();
				uint32 i = 0;
				for (; i < n && b[i]; ++i) {
					char x = a.Data()[i], y = b[i];
					if (x >= 'A' && x <= 'Z')
						x = (char)(x - 'A' + 'a');
					if (y >= 'A' && y <= 'Z')
						y = (char)(y - 'A' + 'a');
					if (x != y)
						return false;
				}
				return i == n && b[i] == '\0';
			}

			inline NkArchiveNode *NkGICorpsMut(NkArchive &bloc) noexcept {
				NkArchiveNode *b = bloc.FindNode(NkStringView(NkGuiArchive::KeyBody()));
				return (b && b->IsArray()) ? b : nullptr;
			}

			/// Le dossier d'un chemin, séparateur compris. Vide s'il n'y en a pas.
			/// ⚠️ LES DEUX SÉPARATEURS SONT ACCEPTÉS. Un document écrit sous Windows
			///    et lu sous Linux -- ou l'inverse -- porte l'un ou l'autre, et ce
			///    dépôt a déjà payé un aller-retour masqué par une normalisation
			///    silencieuse. Ici on ne normalise pas : on coupe au dernier des deux.
			inline NkString NkGIDossierDe(const char *chemin) noexcept {
				NkString s(chemin ? chemin : "");
				int32 coupe = -1;
				for (int32 i = 0; i < (int32)s.Size(); ++i)
					if (s.CStr()[i] == '/' || s.CStr()[i] == '\\')
						coupe = i;
				if (coupe < 0)
					return NkString("");
				NkString out;
				for (int32 i = 0; i <= coupe; ++i) {
					const char c[2] = {s.CStr()[i], '\0'};
					out += c;
				}
				return out;
			}

			/// Joint un dossier et un chemin relatif. Un chemin déjà absolu
			/// (commençant par `/`, `\` ou `X:`) est rendu tel quel.
			inline NkString NkGIJoindre(const NkString &dossier, const NkString &relatif) noexcept {
				const char *r = relatif.CStr();
				const bool absolu = (r[0] == '/' || r[0] == '\\')
									|| (relatif.Size() > 1u && r[1] == ':');
				if (absolu || dossier.Size() == 0u)
					return relatif;
				NkString out = dossier;
				out += relatif;
				return out;
			}

		} // namespace detail

		/**
		 * @brief Remplace chaque `include "..."` par le contenu du document visé.
		 *
		 * @param doc          l'archive, MODIFIÉE EN PLACE.
		 * @param dossierBase  le dossier du document qui inclut. Vide = aucune
		 *                     provenance : les `include` sont alors comptés
		 *                     `nonResolues`, jamais devinés.
		 * @param rap          ce qui a été fait, et ce qui a été refusé.
		 * @return `true` si rien n'a été refusé.
		 */
		inline bool NkGuiResoudreInclusions(NkArchive &doc, const char *dossierBase,
											NkGuiRapportInclusions &rap) noexcept;

	} // namespace nkgui
} // namespace nkentseu

#include "NkGuiInclusions.inl"
