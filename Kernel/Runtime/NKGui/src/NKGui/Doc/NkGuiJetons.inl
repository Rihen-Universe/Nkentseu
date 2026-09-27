// -----------------------------------------------------------------------------
// @File    NkGuiJetons.inl
// @Brief   P1 — la lecture d'un thème et le registre de jetons.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#pragma once

namespace nkentseu {
	namespace nkgui {

		namespace detail {
			/// ⚠️ UN POINTEUR, PAS UNE COPIE — et c'est la table de l'hôte.
			///    Copier la table ici ferait deux vérités : changer de thème
			///    n'aurait aucun effet sur le montage, et le défaut serait muet.
			inline NkGuiTableJetons *&NkGJetonsRegistre() noexcept {
				static NkGuiTableJetons *s_table = nullptr;
				return s_table;
			}

			/// Le `$body` d'un bloc, ou nullptr. Même forme que partout ailleurs
			/// dans cette couche : la structure de l'archive ne se redécouvre pas.
			inline const NkArchiveNode *NkGJCorps(const NkArchive &bloc) noexcept {
				const NkArchiveNode *b = bloc.FindNode(NkStringView(NkGuiArchive::KeyBody()));
				return (b && b->IsArray()) ? b : nullptr;
			}

			inline bool NkGJMotEgal(NkStringView a, const char *b) noexcept {
				usize i = 0;
				for (; i < a.Size(); ++i) {
					if (b[i] == '\0' || a.Data()[i] != b[i])
						return false;
				}
				return b[i] == '\0';
			}

			/// Le lexème d'une chaîne, GUILLEMETS RETIRÉS s'il en porte.
			///
			/// ⚠️ IL EN PORTE, ET C'EST LA RAISON DE CETTE FONCTION. `sens` et le
			///    nom du jeton sont écrits entre guillemets dans le document ;
			///    les garder ferait un jeton nommé `"@info.ecrit"` que personne
			///    ne trouverait jamais — un défaut muet, puisque la résolution
			///    échouerait « normalement ».
			inline NkString NkGJTexteNu(NkStringView lex) noexcept {
				const char *d = lex.Data();
				usize n = lex.Size();
				if (n >= 2u && (d[0] == '"' || d[0] == '\'') && d[n - 1u] == d[0]) {
					++d;
					n -= 2u;
				}
				NkString s;
				for (usize i = 0; i < n; ++i)
					s.PushBack(d[i]);
				return s;
			}
		} // namespace detail

		inline NkGuiTableJetons *NkGuiJetonsPoses() noexcept {
			return detail::NkGJetonsRegistre();
		}

		inline void NkGuiPoserJetons(NkGuiTableJetons *table) noexcept {
			detail::NkGJetonsRegistre() = table;
		}

		namespace detail {
			inline const NkGuiIconSet *&NkGIconesRegistre() noexcept {
				static const NkGuiIconSet *s_jeu = nullptr;
				return s_jeu;
			}
		} // namespace detail

		inline const NkGuiIconSet *NkGuiIconesPosees() noexcept {
			return detail::NkGIconesRegistre();
		}

		inline void NkGuiPoserIcones(const NkGuiIconSet *jeu) noexcept {
			detail::NkGIconesRegistre() = jeu;
		}

		// =====================================================================
		//  LIRE UN THÈME
		// =====================================================================
		//  theme "UE5 Rihen" {
		//    jeton "@info.ecrit" {
		//      sombre = #F79A28
		//      clair  = #C97A08
		//      sens   = "ce bouton écrit une clé"
		//    }
		//  }
		//
		//  ⚠️ LA COULEUR SE LIT PAR LA PORTE DU MONTEUR, PAS PAR UNE TROISIÈME.
		//     `NkGuiCouleur` est déclarée plus haut dans la chaîne d'inclusion
		//     (`NkGuiMonteur.h`) ; ce fichier est inclus APRÈS elle, exprès. En
		//     écrire une ici aurait recréé le défaut que P1 vient réduire.
		inline bool NkGuiLireTheme(const NkArchive &doc, NkGuiTableJetons &table,
								   NkGuiRapportTheme &rap) noexcept {
			const NkArchiveNode *racine = detail::NkGJCorps(doc);
			if (!racine)
				return false;
			bool trouve = false;
			for (uint32 i = 0; i < (uint32)racine->array.Size(); ++i) {
				if (!racine->array[i].IsObject() || !racine->array[i].object)
					continue;
				const NkArchive &sec = *racine->array[i].object;
				if (!detail::NkGJMotEgal(NkGuiArchive::TypeOf(sec), "theme"))
					continue;
				trouve = true;
				rap.nomTheme = detail::NkGJTexteNu(NkGuiArchive::IdOf(sec));
				const NkArchiveNode *corps = detail::NkGJCorps(sec);
				if (!corps)
					continue;
				for (uint32 j = 0; j < (uint32)corps->array.Size(); ++j) {
					if (!corps->array[j].IsObject() || !corps->array[j].object)
						continue;
					const NkArchive &bloc = *corps->array[j].object;
					if (!detail::NkGJMotEgal(NkGuiArchive::TypeOf(bloc), "jeton"))
						continue;

					NkGuiJeton jet;
					jet.nom = detail::NkGJTexteNu(NkGuiArchive::IdOf(bloc));

					const NkArchiveNode *ns = bloc.FindNode(NkStringView("sombre"));
					if (ns)
						jet.aSombre = NkGuiCouleur(ns->Lexeme(), jet.sombre);
					const NkArchiveNode *nc = bloc.FindNode(NkStringView("clair"));
					if (nc)
						jet.aClair = NkGuiCouleur(nc->Lexeme(), jet.clair);
					const NkArchiveNode *nse = bloc.FindNode(NkStringView("sens"));
					if (nse)
						jet.sens = detail::NkGJTexteNu(nse->Lexeme());

					// ⚠️ TROIS REFUS DISTINCTS, ET ILS NE SE CONFONDENT PAS.
					//    « pas de phrase » est une faute d'auteur ; « pas de
					//    couleur lisible » est une faute d'écriture ; « table
					//    pleine » est une limite de l'outil. Les mettre dans le
					//    même compteur rendrait le diagnostic inutile.
					if (jet.sens.Size() == 0u) {
						++rap.sansSens;
						NkString r = jet.nom;
						r.Append(" : aucun `sens` — un jeton sans phrase ne dit pas "
								 "QUELLE information il porte");
						rap.refuses.PushBack(r);
						continue;
					}
					if (!jet.aSombre && !jet.aClair) {
						++rap.sansValeur;
						NkString r = jet.nom;
						r.Append(" : ni `sombre` ni `clair` lisible");
						rap.refuses.PushBack(r);
						continue;
					}
					if (!table.Ajouter(jet)) {
						++rap.debordees;
						NkString r = jet.nom;
						r.Append(" : table de jetons pleine");
						rap.refuses.PushBack(r);
						continue;
					}
					++rap.jetons;
				}
			}
			return trouve;
		}

	} // namespace nkgui
} // namespace nkentseu
