#pragma once
// =============================================================================
// NkcParamSchema — lecture du schema de parametres expose par un module.
//
// POURQUOI CE FICHIER EXISTE
// --------------------------
// `GetParamsSchemaJson` est le seul canal par lequel l'atelier apprend ce qu'un
// moteur de regles (ou une IA) sait regler. Le panneau « Regles » est ENTIEREMENT
// genere a partir d'ici : pas une ligne d'interface par parametre, donc rien a
// recompiler quand le stagiaire en ajoute un.
//
// Format attendu (ConquerorRulesABI.h) :
//   [{"key":"portee_duplication","label":"Portee","group":"Duplication",
//     "type":"int","min":1,"max":3,"def":1,"val":1}, ...]
//
// Extensions tolerees, toutes optionnelles :
//   "label"   absent  -> derive de la cle ("portee_duplication" -> « Portee duplication »)
//   "group"   absent  -> « General »
//   "type"    "bool"  -> case a cocher ; "enum" + "values":["A","B"] -> liste deroulante
//   "help"    presente en infobulle
//
// Scanner ecrit a la main, sans NKSerialization : le module produit ce JSON dans
// un tampon fixe, il est petit et sa grammaire est connue. Ajouter une dependance
// de serialisation complete pour trois cents octets serait disproportionne — et
// la liste de dependances de l'application est un engagement (HANDOFF §d).
// =============================================================================

#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKCore/NkTypes.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>

namespace nkentseu {
	namespace conqueror {

		enum class NkcParamType : uint8 {
			Int	 = 0,
			Bool = 1,
			Enum = 2
		};

		struct NkcParam {
				NkString		   key;
				NkString		   label;
				NkString		   group;
				NkString		   help;
				NkVector<NkString> values;	 ///< Enum uniquement
				NkcParamType	   type = NkcParamType::Int;
				int32			   mn	= 0;
				int32			   mx	= 0;
				int32			   def	= 0;
				int32			   val	= 0;
		};

		// ---------------------------------------------------------------------
		namespace nkcjson {

			inline const char *SkipWs(const char *p) noexcept {
				while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
				return p;
			}

			/// Fin de l'objet ouvert en `p` (qui pointe sur '{'). Suit la
			/// profondeur ET l'etat « dans une chaine » : sans ce second test, un
			/// libelle contenant une accolade tronquerait l'objet.
			inline const char *ObjectEnd(const char *p) noexcept {
				int32 depth	 = 0;
				bool  inStr	 = false;
				for (; *p; ++p) {
					if (inStr) {
						if (*p == '\\' && p[1]) ++p;
						else if (*p == '"')		inStr = false;
						continue;
					}
					if (*p == '"')					inStr = true;
					else if (*p == '{' || *p == '[') ++depth;
					else if (*p == '}' || *p == ']') {
						--depth;
						if (depth == 0) return p + 1;
					}
				}
				return p;
			}

			/// Valeur associee a "key" DANS [begin, end). Renvoie nullptr si absente.
			/// Recherche naive : un objet de parametre tient en 150 octets.
			inline const char *Find(const char *begin, const char *end, const char *key) noexcept {
				char pat[80];
				std::snprintf(pat, sizeof(pat), "\"%s\"", key);
				const usize plen = std::strlen(pat);
				for (const char *p = begin; p + plen <= end; ++p) {
					if (std::strncmp(p, pat, plen) != 0) continue;
					const char *c = SkipWs(p + plen);
					if (*c != ':') continue;
					return SkipWs(c + 1);
				}
				return nullptr;
			}

			inline bool ReadInt(const char *p, int32 &out) noexcept {
				if (!p) return false;
				if (*p == 't') { out = 1; return true; }   // true
				if (*p == 'f') { out = 0; return true; }   // false
				char	  *e = nullptr;
				const long v = std::strtol(p, &e, 10);
				if (e == p) return false;
				out = static_cast<int32>(v);
				return true;
			}

			inline bool ReadString(const char *p, NkString &out) noexcept {
				if (!p || *p != '"') return false;
				out.Clear();
				for (++p; *p && *p != '"'; ++p) {
					if (*p == '\\' && p[1]) ++p;
					out += *p;
				}
				return true;
			}

			/// Tableau de chaines : ["A","B"]. Les listes deroulantes en vivent.
			inline bool ReadStringArray(const char *p, NkVector<NkString> &out) noexcept {
				if (!p || *p != '[') return false;
				out.Clear();
				for (++p; *p && *p != ']'; ++p) {
					if (*p != '"') continue;
					NkString s;
					ReadString(p, s);
					out.PushBack(s);
					// avance jusqu'au guillemet fermant de CETTE chaine
					for (++p; *p && *p != '"'; ++p)
						if (*p == '\\' && p[1]) ++p;
					if (!*p) break;
				}
				return true;
			}

		} // namespace nkcjson

		/// « portee_duplication » -> « Portee duplication ». Le module N'EST PAS
		/// tenu de fournir un libelle : exiger l'un ou l'autre ferait echouer le
		/// premier module d'un stagiaire pour une raison cosmetique.
		inline NkString NkcPrettyLabel(const NkString &key) noexcept {
			NkString out;
			bool	 first = true;
			for (usize i = 0; i < key.Size(); ++i) {
				char c = key[i];
				if (c == '_') { out += ' '; continue; }
				if (first && c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
				first = false;
				out += c;
			}
			return out;
		}

		// ---------------------------------------------------------------------
		/// Analyse le schema complet. Renvoie false si le JSON est inexploitable —
		/// le panneau affiche alors le message brut du module plutot que rien.
		inline bool NkcParseParamsSchema(const char *json, NkVector<NkcParam> &out) noexcept {
			out.Clear();
			if (!json || !*json) return false;

			const char *p = nkcjson::SkipWs(json);
			if (*p != '[') return false;
			++p;

			while (*p) {
				p = nkcjson::SkipWs(p);
				if (*p == ',') { ++p; continue; }
				if (*p == ']' || *p == '\0') break;
				if (*p != '{') break;

				const char *end = nkcjson::ObjectEnd(p);
				NkcParam	prm;

				if (!nkcjson::ReadString(nkcjson::Find(p, end, "key"), prm.key) || prm.key.Empty()) {
					p = end;
					continue;
				}
				if (!nkcjson::ReadString(nkcjson::Find(p, end, "label"), prm.label) || prm.label.Empty())
					prm.label = NkcPrettyLabel(prm.key);
				if (!nkcjson::ReadString(nkcjson::Find(p, end, "group"), prm.group) || prm.group.Empty())
					prm.group = "General";
				nkcjson::ReadString(nkcjson::Find(p, end, "help"), prm.help);

				NkString ty;
				nkcjson::ReadString(nkcjson::Find(p, end, "type"), ty);
				if (ty == "bool")		prm.type = NkcParamType::Bool;
				else if (ty == "enum")	prm.type = NkcParamType::Enum;
				else					prm.type = NkcParamType::Int;

				nkcjson::ReadInt(nkcjson::Find(p, end, "min"), prm.mn);
				nkcjson::ReadInt(nkcjson::Find(p, end, "max"), prm.mx);
				nkcjson::ReadInt(nkcjson::Find(p, end, "def"), prm.def);
				prm.val = prm.def;
				nkcjson::ReadInt(nkcjson::Find(p, end, "val"), prm.val);

				if (prm.type == NkcParamType::Enum) {
					nkcjson::ReadStringArray(nkcjson::Find(p, end, "values"), prm.values);
					// Enum sans libelles : on retombe sur un entier plutot que de
					// presenter une liste deroulante vide, injouable.
					if (prm.values.Empty()) prm.type = NkcParamType::Int;
					else {
						prm.mn = 0;
						prm.mx = static_cast<int32>(prm.values.Size()) - 1;
					}
				}
				if (prm.type == NkcParamType::Bool) { prm.mn = 0; prm.mx = 1; }
				if (prm.mx < prm.mn) prm.mx = prm.mn;

				out.PushBack(prm);
				p = end;
			}
			return !out.Empty();
		}

		/// Liste ORDONNEE des groupes, dans l'ordre d'apparition du schema. Le
		/// module decide donc de l'ordre des sections du panneau : c'est lui qui
		/// sait ce qui compte le plus.
		inline void NkcCollectGroups(const NkVector<NkcParam> &params,
									 NkVector<NkString>		 &groups) noexcept {
			groups.Clear();
			for (usize i = 0; i < params.Size(); ++i) {
				bool seen = false;
				for (usize g = 0; g < groups.Size(); ++g)
					if (groups[g] == params[i].group) { seen = true; break; }
				if (!seen) groups.PushBack(params[i].group);
			}
		}

	} // namespace conqueror
} // namespace nkentseu
