// -----------------------------------------------------------------------------
// FICHIER: Unkeny/Script/NkUnkenyScript.cpp
// DESCRIPTION: Les gestes sur le composant de script (NkUnkenyScript.h) et sa
//              description champ par champ pour la sauvegarde.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Unkeny/Script/NkUnkenyScript.h"

#include <cstddef>
#include <cstring>
#include <type_traits>

namespace nkentseu {
	namespace unkeny {

		static_assert(std::is_trivially_copyable<NkScript2D>::value, "NkScript2D doit etre copiable bit a bit");

		namespace {
			bool Copier(char *dst, uint32 taille, const char *src) noexcept {
				if (src == nullptr) {
					return false;
				}
				const usize n = std::strlen(src);
				if (n == 0u || n >= taille) {
					return false;
				}
				std::memset(dst, 0, taille);
				std::memcpy(dst, src, n);
				return true;
			}
		} // namespace

		int32 NkScriptAjouter(NkScript2D &s, const char *ref) noexcept {
			if (s.nombre >= NK_UNKENY_SCRIPTS_MAX) {
				return -1;
			}
			if (!Copier(s.refs[s.nombre], NK_UNKENY_SCRIPT_NOM_MAX, ref)) {
				return -1;
			}
			s.actifs[s.nombre] = true;
			return static_cast<int32>(s.nombre++);
		}

		bool NkScriptRetirer(NkScript2D &s, uint32 i) noexcept {
			if (i >= s.nombre) {
				return false;
			}
			// Les variables du script retire s'effacent ; celles des suivants
			// descendent d'un emplacement avec lui.
			for (uint32 v = 0; v < NK_UNKENY_SCRIPT_VARS_MAX; ++v) {
				NkVarScript &x = s.vars[v];
				if (x.nom[0] == '\0') {
					continue;
				}
				if (x.script == i) {
					x = NkVarScript();
				} else if (x.script > i) {
					--x.script;
				}
			}
			for (uint32 k = i; k + 1u < s.nombre; ++k) {
				std::memcpy(s.refs[k], s.refs[k + 1u], NK_UNKENY_SCRIPT_NOM_MAX);
				s.actifs[k] = s.actifs[k + 1u];
			}
			--s.nombre;
			std::memset(s.refs[s.nombre], 0, NK_UNKENY_SCRIPT_NOM_MAX);
			s.actifs[s.nombre] = true;
			return true;
		}

		bool NkScriptEchanger(NkScript2D &s, uint32 i, uint32 j) noexcept {
			if (i >= s.nombre || j >= s.nombre) {
				return false;
			}
			if (i == j) {
				return true;
			}
			char tampon[NK_UNKENY_SCRIPT_NOM_MAX];
			std::memcpy(tampon, s.refs[i], NK_UNKENY_SCRIPT_NOM_MAX);
			std::memcpy(s.refs[i], s.refs[j], NK_UNKENY_SCRIPT_NOM_MAX);
			std::memcpy(s.refs[j], tampon, NK_UNKENY_SCRIPT_NOM_MAX);
			const bool a = s.actifs[i];
			s.actifs[i] = s.actifs[j];
			s.actifs[j] = a;
			for (uint32 v = 0; v < NK_UNKENY_SCRIPT_VARS_MAX; ++v) {
				NkVarScript &x = s.vars[v];
				if (x.nom[0] == '\0') {
					continue;
				}
				if (x.script == i) {
					x.script = static_cast<uint8>(j);
				} else if (x.script == j) {
					x.script = static_cast<uint8>(i);
				}
			}
			return true;
		}

		int32 NkScriptTrouver(const NkScript2D &s, const char *ref) noexcept {
			for (uint32 i = 0; ref != nullptr && i < s.nombre; ++i) {
				if (std::strcmp(s.refs[i], ref) == 0) {
					return static_cast<int32>(i);
				}
			}
			return -1;
		}

		const NkVarScript *NkScriptVariable(const NkScript2D &s, uint32 emplacement, const char *nom) noexcept {
			for (uint32 v = 0; nom != nullptr && v < NK_UNKENY_SCRIPT_VARS_MAX; ++v) {
				const NkVarScript &x = s.vars[v];
				if (x.nom[0] != '\0' && x.script == emplacement && std::strcmp(x.nom, nom) == 0) {
					return &x;
				}
			}
			return nullptr;
		}

		NkVarScript *NkScriptVariable(NkScript2D &s, uint32 emplacement, const char *nom) noexcept {
			return const_cast<NkVarScript *>(NkScriptVariable(static_cast<const NkScript2D &>(s), emplacement, nom));
		}

		bool NkScriptPoserVariable(NkScript2D &s, uint32 emplacement, const char *nom, NkTypeVarScript type,
								   const math::NkVec2f &valeur) noexcept {
			if (emplacement >= NK_UNKENY_SCRIPTS_MAX) {
				return false;
			}
			if (NkVarScript *x = NkScriptVariable(s, emplacement, nom)) {
				x->valeur = valeur;
				x->type = static_cast<uint8>(type);
				return true;
			}
			for (uint32 v = 0; v < NK_UNKENY_SCRIPT_VARS_MAX; ++v) {
				NkVarScript &x = s.vars[v];
				if (x.nom[0] != '\0') {
					continue;
				}
				if (!Copier(x.nom, NK_UNKENY_VAR_NOM_MAX, nom)) {
					return false;
				}
				x.script = static_cast<uint8>(emplacement);
				x.type = static_cast<uint8>(type);
				x.valeur = valeur;
				return true;
			}
			return false;
		}

		bool NkScriptPoserTexte(NkScript2D &s, uint32 emplacement, const char *nom, NkTypeVarScript type,
								const char *texte) noexcept {
			if (!NkScriptPoserVariable(s, emplacement, nom, type, math::NkVec2f(0.f, 0.f))) {
				return false;
			}
			NkVarScript *x = NkScriptVariable(s, emplacement, nom);
			if (x == nullptr) {
				return false;
			}
			std::memset(x->texte, 0, NK_UNKENY_VAR_TEXTE_MAX);
			if (texte != nullptr) {
				usize n = std::strlen(texte);
				if (n >= NK_UNKENY_VAR_TEXTE_MAX) {
					n = NK_UNKENY_VAR_TEXTE_MAX - 1u;
					// Ne pas couper un caractere UTF-8 en deux.
					while (n > 0u && (static_cast<uint8>(texte[n]) & 0xC0u) == 0x80u) {
						--n;
					}
				}
				std::memcpy(x->texte, texte, n);
			}
			return true;
		}

		uint32 NkScriptNbVariables(const NkScript2D &s, uint32 emplacement) noexcept {
			uint32 n = 0;
			for (uint32 v = 0; v < NK_UNKENY_SCRIPT_VARS_MAX; ++v) {
				n += (s.vars[v].nom[0] != '\0' && s.vars[v].script == emplacement) ? 1u : 0u;
			}
			return n;
		}

		const NkChampSauve *NkChampsScript2D(uint32 &nombre) noexcept {
			// Les tableaux de TEXTES (refs, vars.nom) s'ecrivent une ligne par
			// element (NkUnkenySauvegarde.cpp, 2026-10-01).
			static const NkChampSauve k[] = {
				NkChampSauve{"refs", NkTypeChamp::NK_TEXTE, static_cast<uint32>(offsetof(NkScript2D, refs)),
							 NK_UNKENY_SCRIPT_NOM_MAX, NK_UNKENY_SCRIPTS_MAX, NK_UNKENY_SCRIPT_NOM_MAX, false},
				NkChampSauve{"actifs", NkTypeChamp::NK_BOOL, static_cast<uint32>(offsetof(NkScript2D, actifs)), 1u,
							 NK_UNKENY_SCRIPTS_MAX, 1u, false},
				NK_UNKENY_CHAMP(NkScript2D, nombre, NkTypeChamp::NK_U8),
				NK_UNKENY_CHAMP_TABLEAU(NkScript2D, vars, NkVarScript, nom, NkTypeChamp::NK_TEXTE, "vars.nom"),
				NK_UNKENY_CHAMP_TABLEAU(NkScript2D, vars, NkVarScript, script, NkTypeChamp::NK_U8, "vars.script"),
				NK_UNKENY_CHAMP_TABLEAU(NkScript2D, vars, NkVarScript, type, NkTypeChamp::NK_U8, "vars.type"),
				NK_UNKENY_CHAMP_TABLEAU(NkScript2D, vars, NkVarScript, valeur, NkTypeChamp::NK_VEC2, "vars.valeur"),
				// (2026-10-01) Le texte d'une variable texte, le NOM d'une entite.
				NK_UNKENY_CHAMP_TABLEAU(NkScript2D, vars, NkVarScript, texte, NkTypeChamp::NK_TEXTE, "vars.texte"),
			};
			nombre = static_cast<uint32>(sizeof(k) / sizeof(k[0]));
			return k;
		}

	} // namespace unkeny
} // namespace nkentseu
