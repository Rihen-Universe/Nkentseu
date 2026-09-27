// -----------------------------------------------------------------------------
// @File    NkGuiLangues.cpp
// @Brief   Le mecanisme multilingue : recherche par cle, a chaud, sans fichier.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// Origine : `NKCode/Shell/NkI18n.h`. Repris le 27/09, moins la lecture de
// fichiers -- que NKGui ne peut pas faire (pas de NKFileSystem). Voir l'en-tete
// de `NkGuiLangues.h`.
// -----------------------------------------------------------------------------

#include "NKGui/Doc/NkGuiLangues.h"

namespace nkentseu {
	namespace nkgui {

		namespace {

			struct Table {
					const NkGuiTraduction *t = nullptr;
					int32 n = 0;
			};

			struct Surcharge {
					NkString cle;
					int32 lang = 0;
					NkString texte;
			};

			struct Etat {
					static const int32 kTablesMax = 8;
					Table tables[kTablesMax];
					int32 nbTables = 0;
					NkVector<Surcharge> surcharges;
					int32 langue = 0;
					NkGuiLanguesRapport rap;
			};

			Etat &E() noexcept {
				static Etat e;
				return e;
			}

			/// Comparaison EXACTE, sensible a la casse.
			///
			/// ⚠️ SENSIBLE A LA CASSE, ET C'EST VOULU. Une cle n'est pas un mot du
			///    vocabulaire du format (`on = Click`, ou l'insensibilite est un
			///    confort) : c'est un identifiant. Deux cles qui ne different que par
			///    la casse doivent rester deux cles -- sinon `design.Enregistrer`
			///    ecraserait `design.enregistrer` en silence, et le mauvais libelle
			///    s'afficherait sans que rien ne le dise.
			bool Eq(const char *a, const char *b) noexcept {
				if (!a || !b)
					return false;
				while (*a && *b) {
					if (*a != *b)
						return false;
					++a;
					++b;
				}
				return *a == '\0' && *b == '\0';
			}

		} // namespace

		const char *NkGuiCodeLangue(int32 lang) noexcept {
			static const char *c[kNkGuiLangues] = {"fr", "en", "es", "pt", "de", "it", "ru", "gom"};
			return (lang >= 0 && lang < kNkGuiLangues) ? c[lang] : "en";
		}

		const char *const *NkGuiNomsLangues() noexcept {
			// Chaque langue dans SA propre langue : c'est ainsi qu'un utilisateur
			// retrouve la sienne dans une liste qu'il ne sait pas lire.
			static const char *n[kNkGuiLangues] = {"Français", "English",  "Español", "Português",
												   "Deutsch",  "Italiano", "Русский", "Ghɔmáláʼ"};
			return n;
		}

		int32 NkGuiLangue() noexcept {
			return E().langue;
		}

		void NkGuiPoserLangue(int32 lang) noexcept {
			E().langue = (lang >= 0 && lang < kNkGuiLangues) ? lang : 0;
		}

		bool NkGuiPoserTableLangues(const NkGuiTraduction *table, int32 nb) noexcept {
			Etat &e = E();
			if (!table || nb <= 0 || e.nbTables >= Etat::kTablesMax)
				return false;
			e.tables[e.nbTables].t = table;
			e.tables[e.nbTables].n = nb;
			++e.nbTables;
			e.rap.tables = e.nbTables;
			e.rap.entrees += nb;
			return true;
		}

		void NkGuiOublierTablesLangues() noexcept {
			Etat &e = E();
			for (int32 i = 0; i < Etat::kTablesMax; ++i)
				e.tables[i] = Table();
			e.nbTables = 0;
			e.rap.tables = 0;
			e.rap.entrees = 0;
		}

		bool NkGuiSurcharger(const char *cle, int32 lang, const char *texte) noexcept {
			if (!cle || !*cle || !texte || lang < 0 || lang >= kNkGuiLangues)
				return false;
			Etat &e = E();
			for (uint32 i = 0; i < (uint32)e.surcharges.Size(); ++i) {
				if (e.surcharges[i].lang == lang && Eq(e.surcharges[i].cle.CStr(), cle)) {
					// REMPLACE : deux fichiers peuvent porter la meme cle, et le
					// dernier lu doit gagner -- c'est ce qui permet a un reglage
					// personnel (`~/.nkcode/lang/`) de battre celui du depot.
					e.surcharges[i].texte = NkString(texte);
					return true;
				}
			}
			Surcharge s;
			s.cle = NkString(cle);
			s.lang = lang;
			s.texte = NkString(texte);
			e.surcharges.PushBack(s);
			e.rap.surcharges = (int32)e.surcharges.Size();
			return true;
		}

		void NkGuiOublierSurcharges() noexcept {
			Etat &e = E();
			e.surcharges.Clear();
			e.rap.surcharges = 0;
		}

		const char *NkGuiTexteLangue(const char *cle) noexcept {
			Etat &e = E();
			++e.rap.demandes;
			if (!cle || !*cle)
				return "";
			const int32 L = e.langue;

			// 1. LA SURCHARGE, d'abord : c'est ce qui permet de corriger un libelle
			//    sans recompiler, et donc a quelqu'un qui n'est pas developpeur de
			//    corriger sa propre langue.
			for (uint32 i = 0; i < (uint32)e.surcharges.Size(); ++i) {
				if (e.surcharges[i].lang == L && Eq(e.surcharges[i].cle.CStr(), cle)) {
					++e.rap.serviesParSurcharge;
					return e.surcharges[i].texte.CStr();
				}
			}

			// 2. LES TABLES POSEES, dans l'ordre de pose.
			for (int32 t = 0; t < e.nbTables; ++t) {
				const Table &tb = e.tables[t];
				for (int32 i = 0; i < tb.n; ++i) {
					if (!Eq(tb.t[i].cle, cle))
						continue;
					const char *s = tb.t[i].s[L];
					if (s && *s) {
						++e.rap.serviesParTable;
						return s;
					}
					// 3. L'ANGLAIS, quand la colonne demandee est vide. Une colonne
					//    vide veut dire « pas encore traduit », pas « rien a
					//    afficher » : rendre l'anglais laisse l'interface UTILISABLE
					//    pendant qu'on traduit.
					const char *en = tb.t[i].s[1];
					if (en && *en) {
						++e.rap.repliAnglais;
						return en;
					}
					break;
				}
			}

			// 4. LA CLE. Voir l'en-tete : laid, visible, corrigeable -- tout ce
			//    qu'une chaine vide n'est pas.
			++e.rap.repliCle;
			e.rap.derniereSansTraduction = NkString(cle);
			return cle;
		}

		uint32 NkGuiChargerSurcharges(const char *texte, uint32 taille, int32 lang) noexcept {
			if (!texte || taille == 0u || lang < 0 || lang >= kNkGuiLangues)
				return 0u;
			uint32 retenues = 0u;
			uint32 i = 0u;
			while (i < taille) {
				// La ligne : de `i` au prochain saut. On accepte LF et CRLF sans le
				// dire -- un fichier de traduction voyage entre machines, et le faire
				// echouer sur un retour chariot serait une cruaute gratuite.
				uint32 fin = i;
				while (fin < taille && texte[fin] != '\n')
					++fin;
				uint32 d = i, f = fin;
				while (f > d && (texte[f - 1] == '\r' || texte[f - 1] == ' ' || texte[f - 1] == '\t'))
					--f;
				while (d < f && (texte[d] == ' ' || texte[d] == '\t'))
					++d;
				i = fin + 1u;
				if (d >= f)
					continue; // ligne vide
				if (texte[d] == '#' || (texte[d] == '/' && d + 1 < f && texte[d + 1] == '/'))
					continue; // commentaire
				// LE PREMIER `=` SEULEMENT. Une valeur a parfaitement le droit d'en
				// contenir (`raccourci=Ctrl+= pour agrandir`) : couper au dernier, ou
				// refuser la ligne, perdrait des libelles legitimes en silence.
				uint32 eq = d;
				while (eq < f && texte[eq] != '=')
					++eq;
				if (eq >= f)
					continue; // pas de `=` : ce n'est pas une paire
				uint32 kf = eq;
				while (kf > d && (texte[kf - 1] == ' ' || texte[kf - 1] == '\t'))
					--kf;
				uint32 vd = eq + 1u;
				while (vd < f && (texte[vd] == ' ' || texte[vd] == '\t'))
					++vd;
				if (kf <= d)
					continue; // cle vide
				NkString cle(texte + d, (NkString::SizeType)(kf - d));
				NkString val(texte + vd, (NkString::SizeType)(f - vd));
				if (NkGuiSurcharger(cle.CStr(), lang, val.CStr()))
					++retenues;
			}
			return retenues;
		}

		NkString NkGuiFormeStable(const NkString &texte) noexcept {
			const char *s = texte.CStr();
			if (s && s[0] == '@' && s[1] == 't' && s[2] == ':' && s[3] != '\0')
				return NkString(s + 3);
			return texte;
		}

		const NkGuiLanguesRapport &NkGuiLanguesReleve() noexcept {
			return E().rap;
		}

		void NkGuiLanguesRemiseAZero() noexcept {
			Etat &e = E();
			e.rap.demandes = 0;
			e.rap.serviesParSurcharge = 0;
			e.rap.serviesParTable = 0;
			e.rap.repliAnglais = 0;
			e.rap.repliCle = 0;
			e.rap.derniereSansTraduction = NkString();
		}

	} // namespace nkgui
} // namespace nkentseu

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
