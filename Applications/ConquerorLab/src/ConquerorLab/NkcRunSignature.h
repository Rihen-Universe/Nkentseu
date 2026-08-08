#pragma once
// =============================================================================
// NkcRunSignature — CE QUI A PRODUIT CE CHIFFRE.
//
// LE PROBLEME QU'ON NE PEUT PAS RESOUDRE PAR LA DISCIPLINE
// --------------------------------------------------------
// Deux stagiaires travaillent en parallele : A1 les regles, A2 l'IA. Le code les
// isole tres bien — dossiers separes, compilations separees, aucun etat partage.
// La MESURE, elle, ne les isole pas :
//
//     campagne du matin  : 62 % pour le camp 0
//     campagne du soir   : 51 % pour le camp 0
//
// Que s'est-il passe ? A1 a change un parametre ? A2 a change son evaluation ?
// Les deux ? Sans reponse, ces deux nombres ne veulent RIEN dire — et on ne peut
// meme pas savoir qu'ils ne veulent rien dire.
//
// « Une variable a la fois » est la bonne discipline. Mais une discipline qu'on
// ne peut pas VERIFIER apres coup n'en est pas une : personne ne se souvient, une
// semaine plus tard, de ce qui avait bouge.
//
// CE QUE FAIT CE FICHIER
// ----------------------
// Il resume en quelques octets TOUT ce qui influence un resultat : moteur, IA de
// chaque siege, paliers, budget, plateau, valeurs de TOUS les parametres. Le
// panneau Metriques l'affiche avec le resultat, garde celle de la campagne
// precedente, et DIT CE QUI A CHANGE.
//
// On n'empeche personne de changer deux choses a la fois. On rend seulement
// impossible de ne pas s'en apercevoir — c'est la seule forme de garde-fou qui
// tienne entre deux personnes.
// =============================================================================

#include "Conqueror/ConquerorRulesABI.h"
#include "Conqueror/ConquerorAIABI.h"
#include "ConquerorLab/NkcParamSchema.h"

#include "NKContainers/String/NkString.h"

#include <cstdio>

namespace nkentseu {
	namespace conqueror {

		/// Ce qui identifie une campagne. `hash` sert a comparer, `lines` a lire :
		/// un identifiant seul dit QU'IL y a eu un changement, pas LEQUEL.
		struct NkcRunSignature {
				uint64	 hash = 0;
				NkString rules;			 ///< moteur + version
				NkString board;
				NkString seats;			 ///< « IA (1.0) Normal 40ms  vs  ... »
				NkString params;		 ///< « portee=1 max_tours=200 ... »

				bool Empty() const noexcept { return hash == 0; }
		};

		/// FNV-1a. Le choix compte peu — ce qui compte est qu'il soit STABLE d'une
		/// execution a l'autre. Un hachage de pointeurs ou d'adresses ne le serait
		/// pas, et deux campagnes identiques paraitraient differentes.
		inline uint64 NkcHashText(uint64 h, const char *s) noexcept {
			if (!s) return h;
			for (; *s; ++s) {
				h ^= static_cast<uint64>(static_cast<unsigned char>(*s));
				h *= 1099511628211ull;
			}
			return h;
		}

		/// Construit la signature a partir de l'etat courant de l'atelier.
		/// `seatLabel(i)` doit rendre le nom de l'IA du siege i (ou « Humain »).
		template <typename FSeatLabel>
		inline NkcRunSignature NkcMakeSignature(const NkcRulesVTable &vt, NkcRules inst,
												const char *rulesLabel, const char *boardLabel,
												uint8 playerCount, const NkcDifficulty *diff,
												const uint32 *budgetMs,
												FSeatLabel &&seatLabel) noexcept {
			NkcRunSignature sig;
			uint64			h = 1469598103934665603ull;

			sig.rules = rulesLabel ? rulesLabel : "?";
			sig.board = boardLabel ? boardLabel : "?";
			h = NkcHashText(h, sig.rules.CStr());
			h = NkcHashText(h, sig.board.CStr());

			char buf[192];
			for (uint8 p = 0; p < playerCount; ++p) {
				std::snprintf(buf, sizeof(buf), "%s%s [%s, %u ms]", p ? "  vs  " : "",
							  seatLabel(p), NkcDifficultyName(diff[p]),
							  static_cast<unsigned>(budgetMs[p]));
				sig.seats += buf;
				h = NkcHashText(h, buf);
			}

			// TOUS les parametres, pas seulement ceux qu'on croit importants : le
			// but est justement de rattraper celui auquel on n'avait pas pense.
			if (vt.GetParamsSchemaJson && vt.GetParam && inst) {
				NkVector<NkcParam> schema;
				if (NkcParseParamsSchema(vt.GetParamsSchemaJson(inst), schema)) {
					for (usize i = 0; i < schema.Size(); ++i) {
						const float64 v = vt.GetParam(inst, schema[i].key.CStr());
						std::snprintf(buf, sizeof(buf), "%s=%g ", schema[i].key.CStr(), v);
						sig.params += buf;
						h = NkcHashText(h, buf);
					}
				}
			}

			sig.hash = h ? h : 1ull;   // 0 est reserve a « pas de signature »
			return sig;
		}

		/// Ce qui a change entre deux campagnes, en clair. Chaine vide si rien.
		/// Volontairement grossier — trois categories suffisent a savoir OU
		/// regarder, et un diff champ par champ serait illisible.
		inline NkString NkcSignatureDiff(const NkcRunSignature &a, const NkcRunSignature &b) noexcept {
			NkString out;
			if (a.Empty() || b.Empty()) return out;
			if (a.hash == b.hash) return out;
			if (a.rules != b.rules)	 { out += "le MOTEUR"; }
			if (a.params != b.params) { if (!out.Empty()) out += ", "; out += "les PARAMETRES"; }
			if (a.seats != b.seats)	 { if (!out.Empty()) out += ", "; out += "les IA"; }
			if (a.board != b.board)	 { if (!out.Empty()) out += ", "; out += "le PLATEAU"; }
			if (out.Empty()) out = "quelque chose";
			return out;
		}

	} // namespace conqueror
} // namespace nkentseu
