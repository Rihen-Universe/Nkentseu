// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#pragma once
// =============================================================================
// NkGuiComposants.h — UN COMPOSANT : un nom, un sous-arbre, son design, son
//                     comportement. Et on peut l'instancier plusieurs fois.
// =============================================================================
//  Rodolf, le 26/09 : « un composant, c'est un élément graphique ou un groupe
//  d'éléments graphiques ayant son design ou ses designs avec leurs propres
//  comportements, animations, événements, etc. »
//
//  Le format portait déjà les morceaux — `widgets`, `appearance`, `behavior`,
//  `animation`, `callback`, `controller`. Il manquait le NOM : aucune section
//  ne permettait de définir « NavigateurContenu » une fois et de l'employer
//  trois fois. Sans nom ni instanciation, il n'y a rien à partager — et c'est
//  le partage qui est le but.
//
// =============================================================================
//  LE CHOIX CENTRAL : ON DÉVELOPPE AVANT DE MONTER, PAS PENDANT
// =============================================================================
//  L'instanciation se fait sur l'ARCHIVE, avant que le monteur ne voie quoi que
//  ce soit. Trois conséquences, et chacune est une raison :
//
//   1. LE MONTEUR NE CHANGE PAS D'UNE LIGNE. Il monte un document où les
//      composants ont déjà disparu, remplacés par ce qu'ils contiennent. Aucun
//      risque d'écart entre « ce qui se monte » et « ce qui s'instancie ».
//
//   2. ÇA SE MESURE SANS FENÊTRE NI GPU. Le développement est une
//      transformation d'archive : on peut la comparer au texte attendu.
//
//   3. L'ALLER-RETOUR DU FORMAT RESTE VRAI. On ne développe pas le document
//      qu'on réécrit sur disque : on développe une COPIE destinée au montage.
//      Le fichier de l'auteur garde ses composants.
//
// =============================================================================
//  LE CONTRAT, ET IL EST ÉTROIT EXPRÈS
// =============================================================================
//      component "BoutonOutil" {
//        widgets {
//          Button "bouton" {            <-- UNE SEULE racine, obligatoire
//            label = "?"
//            appearance { radius = 4 }
//          }
//        }
//        behavior "bouton" { ... }      <-- 0..n, ses références sont réécrites
//      }
//
//      widgets {
//        HBox "barre" {
//          BoutonOutil "anim.jouer" { label = "Jouer" }
//        }
//      }
//
//  ⚠️ UNE SEULE RACINE, ET C'EST REFUSÉ SINON. Avec plusieurs racines, on ne
//     saurait pas à laquelle appliquer l'identifiant de l'instance ni ses
//     attributs. Deviner produirait un document qui se monte en montrant autre
//     chose que ce qui a été demandé — *la pire des trois issues, parce qu'elle
//     est VERTE*. Un composant à plusieurs racines se déclare avec un conteneur.
//
//  ⚠️ LES IDENTIFIANTS SONT PRÉFIXÉS, SINON DEUX INSTANCES SE MARCHENT DESSUS.
//     La racine prend l'identifiant de l'instance ; ses descendants deviennent
//     `<instance>.<original>`. Sans ça, deux `BoutonOutil` partageraient la clé
//     d'état du monteur — et la seconde case à cocher basculerait la première.
//
//  ⚠️ LES ATTRIBUTS DE L'INSTANCE L'EMPORTENT sur ceux de la racine. C'est ce
//     qui rend un composant utile : sans surcharge, toutes les instances
//     seraient identiques et on aurait renommé la copie-colle.
//
//  ⚠️ ET LA RÉCURSION EST REFUSÉE, PAS LIMITÉE. Un composant qui s'instancie
//     lui-même, directement ou en boucle, est une faute d'écriture — pas un
//     motif. Une profondeur maximale la transformerait en document à moitié
//     développé, donc en défaut silencieux.
// =============================================================================

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKSerialization/NkGui/NkGuiArchive.h"

namespace nkentseu {
	namespace nkgui {

		/// Ce que le développement a fait, et ce qu'il a REFUSÉ de faire.
		/// ⚠️ Tout refus se compte. Un composant qu'on ne sait pas développer ne
		///    doit pas disparaître en silence : le document deviendrait plus
		///    pauvre sans se présenter comme une erreur, et personne n'irait
		///    chercher ce qui n'est plus là.
		struct NkGuiRapportComposants {
				uint32 definitions = 0;	 ///< sections `component` lues
				uint32 instances = 0;	 ///< instances développées
				uint32 racinesMultiples = 0; ///< composants à 0 ou 2+ racines : REFUSÉS
				uint32 recursions = 0;	 ///< un composant qui s'instancie lui-même : REFUSÉ
				uint32 attributsSurcharges = 0; ///< attributs de l'instance appliqués
				uint32 referencesReecrites = 0; ///< identifiants réécrits dans les `behavior`
				uint32 comportementsCopies = 0; ///< blocs `behavior` copiés par instance
				NkVector<NkString> noms;		///< les composants définis, dans l'ordre
				NkVector<NkString> refuses;		///< les noms refusés, avec leur raison

				bool Propre() const noexcept {
					return racinesMultiples == 0u && recursions == 0u;
				}
		};

		namespace detail {

			inline const NkArchiveNode *NkGCCorps(const NkArchive &bloc) noexcept {
				const NkArchiveNode *b = bloc.FindNode(NkStringView(NkGuiArchive::KeyBody()));
				return (b && b->IsArray()) ? b : nullptr;
			}

			inline NkArchiveNode *NkGCCorpsMut(NkArchive &bloc) noexcept {
				NkArchiveNode *b = bloc.FindNode(NkStringView(NkGuiArchive::KeyBody()));
				return (b && b->IsArray()) ? b : nullptr;
			}

			inline bool NkGCMotEgal(NkStringView a, const char *b) noexcept {
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

			/// Une définition de composant : son nom, et son corps tel qu'écrit.
			struct NkGCDefinition {
					NkString nom;
					NkArchive corps; ///< le bloc `component` complet (widgets + behavior)
			};

			/// La correspondance « identifiant dans le composant » -> « identifiant
			/// dans le document développé ». Elle sert deux fois : pour renommer les
			/// blocs, et pour réécrire les références des `behavior`.
			struct NkGCRenommage {
					NkVector<NkString> avant;
					NkVector<NkString> apres;

					void Ajouter(const NkString &a, const NkString &b) {
						avant.PushBack(a);
						apres.PushBack(b);
					}
			};

			/// Renomme récursivement les identifiants d'un sous-arbre et note
			/// chaque renommage. La racine a déjà reçu son identifiant : on ne
			/// descend que dans ses enfants.
			inline void NkGCPrefixerIds(NkArchiveNode &noeud, const NkString &prefixe,
										NkGCRenommage &ren) noexcept {
				if (!noeud.IsObject() || !noeud.object)
					return;
				NkArchive &bloc = *noeud.object;
				NkArchiveNode *corps = NkGCCorpsMut(bloc);
				if (!corps)
					return;
				for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
					NkArchiveNode &enfant = corps->array[i];
					if (!enfant.IsObject() || !enfant.object)
						continue; // tranche brute : rien à renommer
					NkArchive &sous = *enfant.object;
					const NkStringView id = NkGuiArchive::IdOf(sous);
					if (id.Size() > 0u) {
						NkString ancien(id);
						NkString neuf = prefixe;
						neuf += ".";
						neuf += ancien;
						NkGuiArchive::SetToken(sous, NkStringView(NkGuiArchive::KeyId()),
											   NkStringView(neuf.CStr()));
						ren.Ajouter(ancien, neuf);
					}
					NkGCPrefixerIds(enfant, prefixe, ren);
				}
			}

			/// Remplace, dans une tranche de source, chaque `avant.` par `apres.`.
			/// ⚠️ ON N'AGIT QUE SUR `identifiant.` — jamais sur un identifiant nu.
			///    Une référence de `behavior` s'écrit `case.checked` : c'est le
			///    point qui la distingue d'un mot quelconque. Sans cette condition,
			///    un composant dont un enfant s'appelle `if` réécrirait le mot-clé.
			inline uint32 NkGCReecrireReferences(NkString &texte, const NkGCRenommage &ren) noexcept {
				uint32 faites = 0;
				for (uint32 k = 0; k < (uint32)ren.avant.Size(); ++k) {
					NkString motif = ren.avant[k];
					motif += ".";
					NkString remplacement = ren.apres[k];
					remplacement += ".";
					const NkString source = texte;
					NkString sortie;
					const char *p = source.CStr();
					const uint32 n = (uint32)source.Size();
					const uint32 m = (uint32)motif.Size();
					uint32 i = 0;
					while (i < n) {
						bool egal = (i + m <= n);
						for (uint32 j = 0; egal && j < m; ++j)
							if (p[i + j] != motif.CStr()[j])
								egal = false;
						// ⚠️ LE CARACTÈRE D'AVANT DOIT ÊTRE UN SÉPARATEUR. Sinon
						//    `macase.checked` verrait `case.` au milieu d'un mot.
						if (egal && i > 0u) {
							const char c = p[i - 1u];
							const bool motSuite = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
												  || (c >= '0' && c <= '9') || c == '_' || c == '.';
							if (motSuite)
								egal = false;
						}
						if (egal) {
							sortie += remplacement;
							i += m;
							++faites;
						} else {
							char un[2] = {p[i], '\0'};
							sortie += un;
							++i;
						}
					}
					texte = sortie;
				}
				return faites;
			}

			/// Applique la réécriture à toutes les tranches brutes d'un bloc.
			inline uint32 NkGCReecrireCorps(NkArchive &bloc, const NkGCRenommage &ren) noexcept {
				uint32 faites = 0;
				NkArchiveNode *corps = NkGCCorpsMut(bloc);
				if (!corps)
					return 0u;
				for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
					NkArchiveNode &n = corps->array[i];
					if (n.IsObject() && n.object) {
						faites += NkGCReecrireCorps(*n.object, ren);
						continue;
					}
					// Une tranche brute : c'est une chaîne, et la chaîne EST la
					// tranche de source (règle T11 du format).
					NkString t = NkString(n.Lexeme());
					const uint32 f = NkGCReecrireReferences(t, ren);
					if (f > 0u) {
						NkGuiArchive::SetTokenNode(n, NkStringView(t.CStr()));
						faites += f;
					}
				}
				return faites;
			}

			/// Copie les attributs SCALAIRES de `source` sur `cible`, la source
			/// l'emportant. Les blocs (dont `appearance`) ne sont pas fusionnés :
			/// l'instance qui veut un autre design le dit avec son propre
			/// `appearance`, qui s'ajoute au corps.
			inline uint32 NkGCSurcharger(NkArchive &cible, const NkArchive &source) noexcept {
				uint32 faits = 0;
				const NkVector<NkArchiveEntry> &ents = source.Entries();
				for (uint32 i = 0; i < (uint32)ents.Size(); ++i) {
					const NkStringView cle = NkStringView(ents[i].key);
					// $type / $id / $body : l'identité ne se surcharge pas. Une
					// instance qui changerait le `$type` de la racine ferait d'un
					// bouton autre chose que ce que le composant promet.
					if (NkGuiArchive::IsReservedKey(cle))
						continue;
					const NkArchiveNode &v = ents[i].node;
					if (v.IsArray() || v.IsObject())
						continue;
					NkGuiArchive::SetToken(cible, cle, v.Lexeme());
					++faits;
				}
				return faits;
			}

		} // namespace detail

		// =====================================================================
		//  LE DÉVELOPPEMENT
		// =====================================================================
		/**
		 * @brief Remplace chaque instance de composant par ce que le composant
		 *        contient, identifiants préfixés et attributs surchargés.
		 *
		 * @param doc  l'archive à développer, MODIFIÉE EN PLACE. Passez-lui une
		 *             copie si le document d'origine doit garder ses composants.
		 * @param rap  ce qui a été fait, et surtout ce qui a été refusé.
		 * @return     `true` si rien n'a été refusé. `false` n'empêche pas le
		 *             document de se monter : il dit qu'il se montera INCOMPLET,
		 *             et le rapport dit où.
		 */
		inline bool NkGuiDevelopperComposants(NkArchive &doc, NkGuiRapportComposants &rap) noexcept;

	} // namespace nkgui
} // namespace nkentseu

#include "NkGuiComposants.inl"
