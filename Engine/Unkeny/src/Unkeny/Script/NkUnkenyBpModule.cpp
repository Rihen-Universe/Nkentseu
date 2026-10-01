// -----------------------------------------------------------------------------
// FICHIER: Unkeny/Script/NkUnkenyBpModule.cpp
// DESCRIPTION: Le module de bytecode d'un Blueprint : octets (petit-boutiste,
//              champ par champ), verification, et le fichier .nkbp.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Unkeny/Script/NkUnkenyBpModule.h"
#include "Unkeny/Script/NkUnkenyScriptABI.h"

#include "NKSerialization/Asset/NkAssetMetadata.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace unkeny {

		const char *NkNomTypeBp(NkTypeBp t) noexcept {
			switch (t) {
				case NkTypeBp::NK_BOOLEEN:
					return "booleen";
				case NkTypeBp::NK_ENTIER:
					return "entier";
				case NkTypeBp::NK_REEL:
					return "reel";
				case NkTypeBp::NK_VEC2:
					return "vec2";
				case NkTypeBp::NK_ENTITE:
					return "entite";
				case NkTypeBp::NK_TEXTE:
					return "texte";
				case NkTypeBp::NK_COULEUR:
					return "couleur";
				default:
					return "rien";
			}
		}

		const char *NkNomOpBp(NkOpBp op) noexcept {
			static const char *const k[] = {"FIN",	 "CONST", "COPIER", "LIRE_VAR", "ECRIRE_VAR", "SOI",	  "ARG",
											"ADD_R", "SUB_R", "MUL_R",	"DIV_R",	"ADD_I",	  "SUB_I",	  "MUL_I",
											"DIV_I", "ADD_V", "SUB_V",	"MUL_VR",	"LT_R",		  "LE_R",	  "EQ_R",
											"LT_I",	 "LE_I",  "EQ_I",	"EQ_E",		"ET",		  "OU",		  "NON",
											"I2R",	 "VEC2",  "VX",		"VY",		"LONGUEUR",	  "NORMALISER", "SAUT",
											"SAUT_SI_FAUX",	  "NATIF", "APPEL", "DIFFUSER"};
			const uint32 i = static_cast<uint32>(op);
			return i < sizeof(k) / sizeof(k[0]) ? k[i] : "?";
		}

		NkTypeBp NkTypeArgBp(uint32 n) noexcept {
			switch (static_cast<NkArgBp>(n)) {
				case NkArgBp::NK_DT:
				case NkArgBp::NK_VALEUR:
					return NkTypeBp::NK_REEL;
				case NkArgBp::NK_AUTRE:
					return NkTypeBp::NK_ENTITE;
				case NkArgBp::NK_SOI_EST_ZONE:
					return NkTypeBp::NK_BOOLEEN;
				default:
					return NkTypeBp::NK_RIEN;
			}
		}

		uint32 NkModuleBp::NoeudDe(uint32 f, uint32 pc) const noexcept {
			if (f >= fonctions.Size()) {
				return 0u;
			}
			const NkVector<NkLigneBp> &l = fonctions[f].lignes;
			uint32 noeud = 0u, meilleur = 0u;
			for (uint32 i = 0; i < l.Size(); ++i) {
				if (l[i].pc <= pc && l[i].pc >= meilleur) {
					meilleur = l[i].pc;
					noeud = l[i].noeud;
				}
			}
			return noeud;
		}

		uint64 NkEmpreinteBp(const char *texte, usize longueur) noexcept {
			uint64 h = 1469598103934665603ull;
			for (usize i = 0; texte != nullptr && i < longueur; ++i) {
				h ^= static_cast<uint8>(texte[i]);
				h *= 1099511628211ull;
			}
			return h;
		}

		// =====================================================================
		// Les octets, petit-boutiste, champ par champ
		// =====================================================================
		namespace {
			struct NkEcrivain {
					NkVector<uint8> &o;
					void U8(uint8 v) {
						o.PushBack(v);
					}
					void U16(uint16 v) {
						U8(static_cast<uint8>(v & 0xFFu));
						U8(static_cast<uint8>(v >> 8));
					}
					void U32(uint32 v) {
						for (int32 k = 0; k < 4; ++k) {
							U8(static_cast<uint8>((v >> (8 * k)) & 0xFFu));
						}
					}
					void U64(uint64 v) {
						U32(static_cast<uint32>(v & 0xFFFFFFFFull));
						U32(static_cast<uint32>(v >> 32));
					}
					void F32(float32 f) {
						uint32 v;
						std::memcpy(&v, &f, 4);
						U32(v);
					}
					void Texte(const NkString &s) {
						U32(static_cast<uint32>(s.Length()));
						for (usize i = 0; i < s.Length(); ++i) {
							U8(static_cast<uint8>(s.CStr()[i]));
						}
					}
					void Magie(const char *m) {
						for (int32 i = 0; i < 4; ++i) {
							U8(static_cast<uint8>(m[i]));
						}
					}
					void Valeur(const NkValeurBp &v) {
						F32(v.x);
						F32(v.y);
						U32(static_cast<uint32>(v.i));
						U64(v.e);
					}
			};

			struct NkLecteur {
					const uint8 *p;
					usize n;
					usize at = 0;
					bool ok = true;
					bool Prendre(usize k) {
						if (!ok || at + k > n) {
							ok = false;
							return false;
						}
						return true;
					}
					uint8 U8() {
						if (!Prendre(1)) {
							return 0u;
						}
						return p[at++];
					}
					uint16 U16() {
						const uint16 a = U8();
						const uint16 b = U8();
						return static_cast<uint16>(a | (b << 8));
					}
					uint32 U32() {
						uint32 v = 0u;
						for (int32 k = 0; k < 4; ++k) {
							v |= static_cast<uint32>(U8()) << (8 * k);
						}
						return v;
					}
					uint64 U64() {
						const uint64 a = U32();
						const uint64 b = U32();
						return a | (b << 32);
					}
					float32 F32() {
						const uint32 v = U32();
						float32 f;
						std::memcpy(&f, &v, 4);
						return f;
					}
					NkString Texte() {
						const uint32 l = U32();
						if (!Prendre(l)) {
							return NkString();
						}
						NkString s(reinterpret_cast<const char *>(p + at), static_cast<usize>(l));
						at += l;
						return s;
					}
					bool Magie(const char *m) {
						if (!Prendre(4)) {
							return false;
						}
						const bool egal = std::memcmp(p + at, m, 4) == 0;
						at += 4;
						return egal;
					}
					NkValeurBp Valeur() {
						NkValeurBp v;
						v.x = F32();
						v.y = F32();
						v.i = static_cast<int32>(U32());
						v.e = U64();
						return v;
					}
					/// Un compte plausible : chaque element prend au moins un octet.
					uint32 Compte() {
						const uint32 c = U32();
						if (ok && c > n - at) {
							ok = false;
							return 0u;
						}
						return c;
					}
			};

			NkTypeBp Type(uint8 v) {
				return v <= NK_BP_TYPE_DERNIER ? static_cast<NkTypeBp>(v) : NkTypeBp::NK_RIEN;
			}
		} // namespace

		void NkEcrireModuleBp(const NkModuleBp &m, NkVector<uint8> &sortie) {
			sortie.Clear();
			NkEcrivain w{sortie};
			w.Magie("NKBC");
			w.U16(m.format);
			w.U16(m.abiMajeure);
			w.U16(m.abiMineure);
			w.U16(0u);
			w.U64(m.empreinte);
			w.U32(static_cast<uint32>(m.imports.Size()));
			for (uint32 i = 0; i < m.imports.Size(); ++i) {
				const NkImportBp &x = m.imports[i];
				w.Texte(x.nom);
				w.U8(static_cast<uint8>(x.params.Size()));
				for (uint32 k = 0; k < x.params.Size(); ++k) {
					w.U8(static_cast<uint8>(x.params[k]));
				}
				w.U8(static_cast<uint8>(x.resultats.Size()));
				for (uint32 k = 0; k < x.resultats.Size(); ++k) {
					w.U8(static_cast<uint8>(x.resultats[k]));
				}
			}
			w.U32(static_cast<uint32>(m.variables.Size()));
			for (uint32 i = 0; i < m.variables.Size(); ++i) {
				const NkVariableBp &v = m.variables[i];
				w.Texte(v.nom);
				w.U8(static_cast<uint8>(v.type));
				w.Valeur(v.defaut);
				w.U8(v.exposee ? 1u : 0u);
			}
			w.U32(static_cast<uint32>(m.constantes.Size()));
			for (uint32 i = 0; i < m.constantes.Size(); ++i) {
				const NkConstanteBp &c = m.constantes[i];
				w.U8(static_cast<uint8>(c.type));
				w.Valeur(c.valeur);
				w.Texte(c.texte);
			}
			w.U32(static_cast<uint32>(m.entrees.Size()));
			for (uint32 i = 0; i < m.entrees.Size(); ++i) {
				w.U32(m.entrees[i].genre);
				w.Texte(m.entrees[i].parametre);
				w.U32(m.entrees[i].fonction);
			}
			w.U32(static_cast<uint32>(m.fonctions.Size()));
			for (uint32 i = 0; i < m.fonctions.Size(); ++i) {
				const NkFonctionBp &f = m.fonctions[i];
				w.Texte(f.nom);
				// Format 2 : la signature, avant les registres.
				w.U8(static_cast<uint8>(f.params.Size()));
				for (uint32 k = 0; k < f.params.Size(); ++k) {
					w.U8(static_cast<uint8>(f.params[k]));
				}
				w.U8(static_cast<uint8>(f.resultats.Size()));
				for (uint32 k = 0; k < f.resultats.Size(); ++k) {
					w.U8(static_cast<uint8>(f.resultats[k]));
				}
				w.U32(static_cast<uint32>(f.registres.Size()));
				for (uint32 k = 0; k < f.registres.Size(); ++k) {
					w.U8(static_cast<uint8>(f.registres[k]));
				}
				w.U32(static_cast<uint32>(f.code.Size()));
				for (uint32 k = 0; k < f.code.Size(); ++k) {
					w.U32(f.code[k]);
				}
				w.U32(static_cast<uint32>(f.lignes.Size()));
				for (uint32 k = 0; k < f.lignes.Size(); ++k) {
					w.U32(f.lignes[k].pc);
					w.U32(f.lignes[k].noeud);
				}
			}
		}

		bool NkLireModuleBp(const uint8 *octets, usize taille, NkModuleBp &m, NkString *erreur) {
			auto echec = [&](const char *pourquoi) {
				if (erreur != nullptr) {
					*erreur = NkString(pourquoi);
				}
				return false;
			};
			NkLecteur r{octets, taille};
			if (octets == nullptr || !r.Magie("NKBC")) {
				return echec("module : magie « NKBC » absente");
			}
			m = NkModuleBp();
			m.format = r.U16();
			m.abiMajeure = r.U16();
			m.abiMineure = r.U16();
			(void)r.U16();
			m.empreinte = r.U64();
			if (r.ok && (m.format < NK_BP_FORMAT_MIN || m.format > NK_BP_FORMAT)) {
				return echec("module : format inconnu (plus recent que ce moteur ?)");
			}
			uint32 n = r.Compte();
			for (uint32 i = 0; r.ok && i < n; ++i) {
				NkImportBp x;
				x.nom = r.Texte();
				const uint8 np = r.U8();
				for (uint8 k = 0; r.ok && k < np; ++k) {
					x.params.PushBack(Type(r.U8()));
				}
				const uint8 nr = r.U8();
				for (uint8 k = 0; r.ok && k < nr; ++k) {
					x.resultats.PushBack(Type(r.U8()));
				}
				m.imports.PushBack(x);
			}
			n = r.Compte();
			for (uint32 i = 0; r.ok && i < n; ++i) {
				NkVariableBp v;
				v.nom = r.Texte();
				v.type = Type(r.U8());
				v.defaut = r.Valeur();
				v.exposee = r.U8() != 0u;
				m.variables.PushBack(v);
			}
			n = r.Compte();
			for (uint32 i = 0; r.ok && i < n; ++i) {
				NkConstanteBp c;
				c.type = Type(r.U8());
				c.valeur = r.Valeur();
				c.texte = r.Texte();
				m.constantes.PushBack(c);
			}
			n = r.Compte();
			for (uint32 i = 0; r.ok && i < n; ++i) {
				NkEntreeBp e;
				e.genre = r.U32();
				e.parametre = r.Texte();
				e.fonction = r.U32();
				m.entrees.PushBack(e);
			}
			n = r.Compte();
			for (uint32 i = 0; r.ok && i < n; ++i) {
				NkFonctionBp f;
				f.nom = r.Texte();
				if (m.format >= 2u) {
					const uint8 np = r.U8();
					for (uint8 k = 0; r.ok && k < np; ++k) {
						f.params.PushBack(Type(r.U8()));
					}
					const uint8 ns = r.U8();
					for (uint8 k = 0; r.ok && k < ns; ++k) {
						f.resultats.PushBack(Type(r.U8()));
					}
				}
				const uint32 nr = r.Compte();
				for (uint32 k = 0; r.ok && k < nr; ++k) {
					f.registres.PushBack(Type(r.U8()));
				}
				const uint32 nc = r.Compte();
				for (uint32 k = 0; r.ok && k < nc; ++k) {
					f.code.PushBack(r.U32());
				}
				const uint32 nl = r.Compte();
				for (uint32 k = 0; r.ok && k < nl; ++k) {
					NkLigneBp l;
					l.pc = r.U32();
					l.noeud = r.U32();
					f.lignes.PushBack(l);
				}
				m.fonctions.PushBack(f);
			}
			if (!r.ok) {
				return echec("module : octets tronques");
			}
			return true;
		}

		// =====================================================================
		// La verification
		// =====================================================================
		namespace {
			/// Le genre d'un operande.
			enum NkGenreOp : uint8 { R = 0, K, V, P, A };
			struct NkFormeOp {
					uint8 n;
					uint8 genre[3];
					NkTypeBp type[3]; ///< pour R : NK_RIEN = « le type du premier registre »
			};
			constexpr NkTypeBp RIEN = NkTypeBp::NK_RIEN, B_ = NkTypeBp::NK_BOOLEEN, I_ = NkTypeBp::NK_ENTIER,
							   R_ = NkTypeBp::NK_REEL, V_ = NkTypeBp::NK_VEC2, E_ = NkTypeBp::NK_ENTITE;

			NkFormeOp Forme(NkOpBp op) {
				switch (op) {
					case NkOpBp::NK_FIN:
						return {0, {}, {}};
					case NkOpBp::NK_CONST:
						return {2, {R, K, 0}, {RIEN, RIEN, RIEN}};
					case NkOpBp::NK_COPIER:
						return {2, {R, R, 0}, {RIEN, RIEN, RIEN}};
					case NkOpBp::NK_LIRE_VAR:
						return {2, {R, V, 0}, {RIEN, RIEN, RIEN}};
					case NkOpBp::NK_ECRIRE_VAR:
						return {2, {V, R, 0}, {RIEN, RIEN, RIEN}};
					case NkOpBp::NK_SOI:
						return {1, {R, 0, 0}, {E_, RIEN, RIEN}};
					case NkOpBp::NK_ARG:
						return {2, {R, A, 0}, {RIEN, RIEN, RIEN}};
					case NkOpBp::NK_ADD_R:
					case NkOpBp::NK_SUB_R:
					case NkOpBp::NK_MUL_R:
					case NkOpBp::NK_DIV_R:
						return {3, {R, R, R}, {R_, R_, R_}};
					case NkOpBp::NK_ADD_I:
					case NkOpBp::NK_SUB_I:
					case NkOpBp::NK_MUL_I:
					case NkOpBp::NK_DIV_I:
						return {3, {R, R, R}, {I_, I_, I_}};
					case NkOpBp::NK_ADD_V:
					case NkOpBp::NK_SUB_V:
						return {3, {R, R, R}, {V_, V_, V_}};
					case NkOpBp::NK_MUL_VR:
						return {3, {R, R, R}, {V_, V_, R_}};
					case NkOpBp::NK_LT_R:
					case NkOpBp::NK_LE_R:
					case NkOpBp::NK_EQ_R:
						return {3, {R, R, R}, {B_, R_, R_}};
					case NkOpBp::NK_LT_I:
					case NkOpBp::NK_LE_I:
					case NkOpBp::NK_EQ_I:
						return {3, {R, R, R}, {B_, I_, I_}};
					case NkOpBp::NK_EQ_E:
						return {3, {R, R, R}, {B_, E_, E_}};
					case NkOpBp::NK_ET:
					case NkOpBp::NK_OU:
						return {3, {R, R, R}, {B_, B_, B_}};
					case NkOpBp::NK_NON:
						return {2, {R, R, 0}, {B_, B_, RIEN}};
					case NkOpBp::NK_I2R:
						return {2, {R, R, 0}, {R_, I_, RIEN}};
					case NkOpBp::NK_VEC2:
						return {3, {R, R, R}, {V_, R_, R_}};
					case NkOpBp::NK_VX:
					case NkOpBp::NK_VY:
					case NkOpBp::NK_LONGUEUR:
						return {2, {R, R, 0}, {R_, V_, RIEN}};
					case NkOpBp::NK_NORMALISER:
						return {2, {R, R, 0}, {V_, V_, RIEN}};
					case NkOpBp::NK_SAUT:
						return {1, {P, 0, 0}, {RIEN, RIEN, RIEN}};
					case NkOpBp::NK_SAUT_SI_FAUX:
						return {2, {R, P, 0}, {B_, RIEN, RIEN}};
					default:
						return {0xFF, {}, {}};
				}
			}

			bool VarTypeValide(NkTypeBp t) {
				// (2026-10-01) texte, entite et couleur : des variables declarees
				// dans l'editeur de Blueprint (une entite ou un texte par instance).
				return t == B_ || t == I_ || t == R_ || t == V_ || t == E_ || t == NkTypeBp::NK_TEXTE ||
					   t == NkTypeBp::NK_COULEUR;
			}
		} // namespace

		bool NkVerifierModuleBp(const NkModuleBp &m, const NkSignatureNatifBp *table, uint32 nbTable,
								NkVector<int32> &natifs, NkRefusBp &refus) {
			refus = NkRefusBp();
			natifs.Clear();
			auto refuser = [&](int32 f, int32 pc, const NkString &raison) {
				refus.raison = raison;
				refus.fonction = f;
				refus.pc = pc;
				refus.noeud = (f >= 0 && pc >= 0) ? m.NoeudDe(static_cast<uint32>(f), static_cast<uint32>(pc)) : 0u;
				return false;
			};
			if (m.format < NK_BP_FORMAT_MIN || m.format > NK_BP_FORMAT) {
				return refuser(-1, -1, NkString("format de module inconnu"));
			}
			if (m.abiMajeure != NK_UNK_ABI_MAJEURE) {
				return refuser(-1, -1,
							   NkString::Format("ABI majeure %u, l'hote parle la %u", static_cast<unsigned>(m.abiMajeure),
												static_cast<unsigned>(NK_UNK_ABI_MAJEURE)));
			}
			// ── Les imports, resolus PAR NOM, signature comprise ──
			for (uint32 i = 0; i < m.imports.Size(); ++i) {
				const NkImportBp &x = m.imports[i];
				int32 trouve = -1;
				for (uint32 k = 0; k < nbTable; ++k) {
					if (table[k].nom != nullptr && std::strcmp(x.nom.CStr(), table[k].nom) == 0) {
						trouve = static_cast<int32>(k);
						break;
					}
				}
				if (trouve < 0) {
					return refuser(-1, -1, NkString::Format("natif inconnu de cet hote : %s", x.nom.CStr()));
				}
				const NkSignatureNatifBp &s = table[trouve];
				bool egal = x.params.Size() == s.nbParams && x.resultats.Size() == s.nbResultats;
				for (uint32 k = 0; egal && k < s.nbParams; ++k) {
					egal = x.params[k] == s.params[k];
				}
				for (uint32 k = 0; egal && k < s.nbResultats; ++k) {
					egal = x.resultats[k] == s.resultats[k];
				}
				if (!egal) {
					return refuser(-1, -1, NkString::Format("signature discordante pour le natif %s", x.nom.CStr()));
				}
				natifs.PushBack(trouve);
			}
			for (uint32 i = 0; i < m.variables.Size(); ++i) {
				if (!VarTypeValide(m.variables[i].type) || m.variables[i].nom.Empty()) {
					return refuser(-1, -1, NkString::Format("variable %u : type ou nom invalide", static_cast<unsigned>(i)));
				}
				// Le defaut d'un texte est une CONSTANTE du module (son indice).
				if (m.variables[i].type == NkTypeBp::NK_TEXTE) {
					const int32 k = m.variables[i].defaut.i;
					if (k < 0 || static_cast<uint32>(k) >= m.constantes.Size() ||
						m.constantes[static_cast<uint32>(k)].type != NkTypeBp::NK_TEXTE) {
						return refuser(-1, -1, NkString::Format("variable %s : defaut texte hors table", m.variables[i].nom.CStr()));
					}
				}
			}
			for (uint32 i = 0; i < m.constantes.Size(); ++i) {
				if (m.constantes[i].type == RIEN) {
					return refuser(-1, -1, NkString::Format("constante %u sans type", static_cast<unsigned>(i)));
				}
			}
			// ── Les fonctions ──
			NkVector<uint32> masques; // evenements autorises par fonction (natifs DIRECTS, puis fermes)
			NkVector<uint32> appels;  // (appelant, appele, pc) : la fermeture des masques
			for (uint32 f = 0; f < m.fonctions.Size(); ++f) {
				const NkFonctionBp &fn = m.fonctions[f];
				const uint32 nreg = static_cast<uint32>(fn.registres.Size());
				if (nreg > NK_BP_REGISTRES_MAX) {
					return refuser(static_cast<int32>(f), -1, NkString("trop de registres"));
				}
				// La signature : les premiers registres SONT les parametres, puis
				// les resultats, de memes types.
				const uint32 nsig = static_cast<uint32>(fn.params.Size() + fn.resultats.Size());
				if (nsig > nreg) {
					return refuser(static_cast<int32>(f), -1, NkString("signature plus large que le cadre"));
				}
				for (uint32 k = 0; k < nsig; ++k) {
					const NkTypeBp t = k < fn.params.Size() ? fn.params[k] : fn.resultats[k - static_cast<uint32>(fn.params.Size())];
					if (t == RIEN || fn.registres[k] != t) {
						return refuser(static_cast<int32>(f), -1,
									   NkString::Format("signature : le registre %u n'a pas son type", static_cast<unsigned>(k)));
					}
				}
				for (uint32 k = 0; k < nreg; ++k) {
					if (fn.registres[k] == RIEN) {
						return refuser(static_cast<int32>(f), -1, NkString("registre sans type"));
					}
				}
				const NkVector<uint32> &c = fn.code;
				if (c.Empty()) {
					return refuser(static_cast<int32>(f), -1, NkString("fonction vide"));
				}
				NkVector<uint8> debut;
				debut.Resize(c.Size() + 1u, 0u);
				NkVector<uint32> sauts; // (pc de l'instruction, cible)
				uint32 masque = 0xFFFFFFFFu;
				uint32 pc = 0;
				uint32 dernier = 0;
				while (pc < c.Size()) {
					const uint32 at = pc;
					dernier = c[pc];
					debut[pc] = 1u;
					const NkOpBp op = static_cast<NkOpBp>(c[pc++]);
					auto reg = [&](uint32 r) -> bool {
						return r < nreg;
					};
					if (op == NkOpBp::NK_NATIF) {
						if (pc >= c.Size()) {
							return refuser(static_cast<int32>(f), static_cast<int32>(at), NkString("instruction tronquee"));
						}
						const uint32 k = c[pc++];
						if (k >= m.imports.Size()) {
							return refuser(static_cast<int32>(f), static_cast<int32>(at), NkString("import hors table"));
						}
						const NkImportBp &x = m.imports[k];
						const uint32 nb = static_cast<uint32>(x.params.Size() + x.resultats.Size());
						if (pc + nb > c.Size()) {
							return refuser(static_cast<int32>(f), static_cast<int32>(at), NkString("instruction tronquee"));
						}
						for (uint32 a = 0; a < nb; ++a) {
							const uint32 r = c[pc++];
							const NkTypeBp attendu =
								a < x.params.Size() ? x.params[a] : x.resultats[a - static_cast<uint32>(x.params.Size())];
							if (!reg(r)) {
								return refuser(static_cast<int32>(f), static_cast<int32>(at), NkString("registre hors cadre"));
							}
							if (fn.registres[r] != attendu) {
								return refuser(static_cast<int32>(f), static_cast<int32>(at),
											   NkString::Format("type discordant : %s attend un %s", x.nom.CStr(),
																NkNomTypeBp(attendu)));
							}
						}
						masque &= table[natifs[k]].evenements;
						continue;
					}
					if (op == NkOpBp::NK_DIFFUSER) {
						if (pc + 3u > c.Size()) {
							return refuser(static_cast<int32>(f), static_cast<int32>(at), NkString("instruction tronquee"));
						}
						const uint32 cible = c[pc++], k = c[pc++], n = c[pc++];
						if (!reg(cible) || fn.registres[cible] != E_) {
							return refuser(static_cast<int32>(f), static_cast<int32>(at), NkString("repartiteur : la cible n'est pas une entite"));
						}
						if (k >= m.constantes.Size() || m.constantes[k].type != NkTypeBp::NK_TEXTE) {
							return refuser(static_cast<int32>(f), static_cast<int32>(at), NkString("repartiteur : nom hors table"));
						}
						if (n > 8u || pc + n > c.Size()) {
							return refuser(static_cast<int32>(f), static_cast<int32>(at), NkString("repartiteur : trop de parametres"));
						}
						for (uint32 a = 0; a < n; ++a) {
							if (!reg(c[pc++])) {
								return refuser(static_cast<int32>(f), static_cast<int32>(at), NkString("registre hors cadre"));
							}
						}
						continue;
					}
					if (op == NkOpBp::NK_APPEL) {
						if (pc >= c.Size()) {
							return refuser(static_cast<int32>(f), static_cast<int32>(at), NkString("instruction tronquee"));
						}
						const uint32 g = c[pc++];
						if (g >= m.fonctions.Size()) {
							return refuser(static_cast<int32>(f), static_cast<int32>(at), NkString("appel d'une fonction absente"));
						}
						const NkFonctionBp &cible = m.fonctions[g];
						const uint32 np = static_cast<uint32>(cible.params.Size());
						const uint32 nb = np + static_cast<uint32>(cible.resultats.Size());
						if (pc + nb > c.Size()) {
							return refuser(static_cast<int32>(f), static_cast<int32>(at), NkString("instruction tronquee"));
						}
						for (uint32 a = 0; a < nb; ++a) {
							const uint32 r = c[pc++];
							const NkTypeBp attendu = a < np ? cible.params[a] : cible.resultats[a - np];
							if (!reg(r)) {
								return refuser(static_cast<int32>(f), static_cast<int32>(at), NkString("registre hors cadre"));
							}
							if (fn.registres[r] != attendu) {
								return refuser(static_cast<int32>(f), static_cast<int32>(at),
											   NkString::Format("type discordant : %s attend un %s", cible.nom.CStr(),
																NkNomTypeBp(attendu)));
							}
						}
						appels.PushBack(f);
						appels.PushBack(g);
						appels.PushBack(at);
						continue;
					}
					const NkFormeOp forme = Forme(op);
					if (forme.n == 0xFF) {
						return refuser(static_cast<int32>(f), static_cast<int32>(at),
									   NkString::Format("opcode inconnu %u", static_cast<unsigned>(c[at])));
					}
					if (pc + forme.n > c.Size()) {
						return refuser(static_cast<int32>(f), static_cast<int32>(at), NkString("instruction tronquee"));
					}
					NkTypeBp premier = RIEN;
					uint32 operandes[3] = {0, 0, 0};
					for (uint32 a = 0; a < forme.n; ++a) {
						operandes[a] = c[pc++];
					}
					for (uint32 a = 0; a < forme.n; ++a) {
						const uint32 v = operandes[a];
						switch (forme.genre[a]) {
							case R: {
								if (!reg(v)) {
									return refuser(static_cast<int32>(f), static_cast<int32>(at), NkString("registre hors cadre"));
								}
								const NkTypeBp t = fn.registres[v];
								const NkTypeBp attendu = forme.type[a] != RIEN ? forme.type[a] : premier;
								if (attendu != RIEN && t != attendu) {
									return refuser(static_cast<int32>(f), static_cast<int32>(at),
												   NkString::Format("type discordant : %s attend un %s, le registre %u est un %s",
																	NkNomOpBp(op), NkNomTypeBp(attendu), static_cast<unsigned>(v),
																	NkNomTypeBp(t)));
								}
								if (premier == RIEN) {
									premier = t;
								}
								break;
							}
							case K:
								if (v >= m.constantes.Size()) {
									return refuser(static_cast<int32>(f), static_cast<int32>(at), NkString("constante hors table"));
								}
								if (m.constantes[v].type != premier) {
									return refuser(static_cast<int32>(f), static_cast<int32>(at),
												   NkString("type discordant : constante et registre"));
								}
								break;
							case V:
								if (v >= m.variables.Size()) {
									return refuser(static_cast<int32>(f), static_cast<int32>(at), NkString("variable hors table"));
								}
								break;
							case A:
								if (NkTypeArgBp(v) == RIEN || NkTypeArgBp(v) != premier) {
									return refuser(static_cast<int32>(f), static_cast<int32>(at),
												   NkString("argument d'evenement inconnu ou de mauvais type"));
								}
								break;
							case P:
								sauts.PushBack(at);
								sauts.PushBack(v);
								break;
							default:
								break;
						}
					}
					// La variable et son registre : meme type (dans les deux sens).
					if (op == NkOpBp::NK_ECRIRE_VAR &&
						(operandes[1] >= nreg || fn.registres[operandes[1]] != m.variables[operandes[0]].type)) {
						return refuser(static_cast<int32>(f), static_cast<int32>(at), NkString("type discordant : variable"));
					}
					if (op == NkOpBp::NK_LIRE_VAR && fn.registres[operandes[0]] != m.variables[operandes[1]].type) {
						return refuser(static_cast<int32>(f), static_cast<int32>(at), NkString("type discordant : variable"));
					}
				}
				(void)dernier;
				for (uint32 s = 0; s + 1u < sauts.Size(); s += 2u) {
					const uint32 cible = sauts[s + 1u];
					if (cible >= c.Size() || debut[cible] == 0u) {
						return refuser(static_cast<int32>(f), static_cast<int32>(sauts[s]),
									   NkString("saut hors fonction ou au milieu d'une instruction"));
					}
				}
				// La derniere instruction : FIN ou SAUT, sinon l'execution tomberait
				// hors du code. On la retrouve en reparcourant les debuts.
				uint32 derniere = 0;
				for (uint32 k = 0; k < c.Size(); ++k) {
					if (debut[k] != 0u) {
						derniere = k;
					}
				}
				const NkOpBp opFin = static_cast<NkOpBp>(c[derniere]);
				if (opFin != NkOpBp::NK_FIN && opFin != NkOpBp::NK_SAUT) {
					return refuser(static_cast<int32>(f), static_cast<int32>(derniere),
								   NkString("la fonction ne finit pas par FIN"));
				}
				for (uint32 l = 0; l < fn.lignes.Size(); ++l) {
					if (fn.lignes[l].pc >= c.Size()) {
						return refuser(static_cast<int32>(f), -1, NkString("table de lignes hors code"));
					}
				}
				masques.PushBack(masque);
			}
			// ── La FERMETURE des masques par les appels : une fonction appelee sous
			//    « Tick » ne peut pas y appliquer une force. Point fixe : les masques
			//    ne font que DECROITRE (ET binaire), il converge -- recursion comprise.
			for (bool change = true; change;) {
				change = false;
				for (uint32 a = 0; a + 2u < appels.Size(); a += 3u) {
					const uint32 appelant = appels[a], appele = appels[a + 1u];
					const uint32 nm = masques[appelant] & masques[appele];
					if (nm != masques[appelant]) {
						masques[appelant] = nm;
						change = true;
					}
				}
			}
			// ── Une fonction d'evenement n'a ni parametre ni resultat (sauf un
			//    evenement PERSONNALISE, qui porte les parametres de son repartiteur) ──
			for (uint32 i = 0; i < m.entrees.Size(); ++i) {
				const NkEntreeBp &e = m.entrees[i];
				if (e.fonction < m.fonctions.Size() && e.genre != NK_UNK_EV_PERSONNALISE &&
					(!m.fonctions[e.fonction].params.Empty() || !m.fonctions[e.fonction].resultats.Empty())) {
					return refuser(static_cast<int32>(e.fonction), -1, NkString("une fonction d'evenement n'a pas de parametre"));
				}
			}
			// ── Les entrees : fonction existante, natifs permis pour l'evenement ──
			for (uint32 i = 0; i < m.entrees.Size(); ++i) {
				const NkEntreeBp &e = m.entrees[i];
				if (e.fonction >= m.fonctions.Size()) {
					return refuser(-1, -1, NkString("entree vers une fonction absente"));
				}
				if (e.genre >= NK_UNK_EV_NOMBRE) {
					return refuser(static_cast<int32>(e.fonction), -1, NkString("evenement inconnu de cet hote"));
				}
				if ((e.genre == NK_UNK_EV_ACTION_PRESSEE || e.genre == NK_UNK_EV_ACTION_RELACHEE) && e.parametre.Empty()) {
					return refuser(static_cast<int32>(e.fonction), -1, NkString("evenement d'action sans action"));
				}
				if ((masques[e.fonction] & (1u << e.genre)) == 0u) {
					// Retrouver le natif fautif : le premier dont le masque refuse --
					// ou l'APPEL qui y mene (la faute est designee la ou on la voit).
					const NkFonctionBp &fn = m.fonctions[e.fonction];
					uint32 pc = 0;
					while (pc < fn.code.Size()) {
						const NkOpBp op = static_cast<NkOpBp>(fn.code[pc]);
						if (op == NkOpBp::NK_NATIF) {
							const uint32 k = fn.code[pc + 1u];
							if ((table[natifs[k]].evenements & (1u << e.genre)) == 0u) {
								return refuser(static_cast<int32>(e.fonction), static_cast<int32>(pc),
											   NkString::Format("%s n'est pas permis sous cet evenement", m.imports[k].nom.CStr()));
							}
							pc += 2u + static_cast<uint32>(m.imports[k].params.Size() + m.imports[k].resultats.Size());
						} else if (op == NkOpBp::NK_DIFFUSER) {
							pc += 4u + fn.code[pc + 3u];
						} else if (op == NkOpBp::NK_APPEL) {
							const uint32 g = fn.code[pc + 1u];
							if ((masques[g] & (1u << e.genre)) == 0u) {
								return refuser(static_cast<int32>(e.fonction), static_cast<int32>(pc),
											   NkString::Format("la fonction %s fait un geste qui n'est pas permis sous cet evenement",
																m.fonctions[g].nom.CStr()));
							}
							pc += 2u + static_cast<uint32>(m.fonctions[g].params.Size() + m.fonctions[g].resultats.Size());
						} else {
							pc += 1u + Forme(op).n;
						}
					}
					return refuser(static_cast<int32>(e.fonction), -1, NkString("natif non permis sous cet evenement"));
				}
			}
			return true;
		}

		// =====================================================================
		// La charge utile et le fichier .nkbp
		// =====================================================================
		void NkEcrireChargeBp(const NkString &graphe, const NkModuleBp *module, NkVector<uint8> &sortie,
							  const NkString *document) {
			sortie.Clear();
			NkEcrivain w{sortie};
			w.Magie("NKBP");
			w.U32(NK_BP_FICHIER_VERSION);
			const bool doc = document != nullptr && !document->Empty();
			w.U32((module != nullptr ? 2u : 1u) + (doc ? 1u : 0u));
			w.Magie("GRAF");
			w.Texte(graphe);
			if (module != nullptr) {
				NkVector<uint8> octets;
				NkEcrireModuleBp(*module, octets);
				w.Magie("MODL");
				w.U32(static_cast<uint32>(octets.Size()));
				for (uint32 i = 0; i < octets.Size(); ++i) {
					w.U8(octets[i]);
				}
			}
			// EN DERNIER : un lecteur d'avant le 01/10 s'arrete a la premiere
			// section inconnue -- il aura deja lu GRAF et MODL.
			if (doc) {
				w.Magie("DOCU");
				w.Texte(*document);
			}
		}

		bool NkLireChargeBp(const uint8 *octets, usize taille, NkString *graphe, NkModuleBp *module, bool *aModule,
							NkString *erreur, NkString *document) {
			if (aModule != nullptr) {
				*aModule = false;
			}
			if (document != nullptr) {
				document->Clear();
			}
			NkLecteur r{octets, taille};
			if (octets == nullptr || !r.Magie("NKBP")) {
				if (erreur != nullptr) {
					*erreur = "charge .nkbp : magie « NKBP » absente";
				}
				return false;
			}
			const uint32 version = r.U32();
			if (r.ok && version > NK_BP_FICHIER_VERSION) {
				if (erreur != nullptr) {
					*erreur = "charge .nkbp plus recente que ce moteur";
				}
				return false;
			}
			const uint32 sections = r.U32();
			for (uint32 s = 0; r.ok && s < sections; ++s) {
				if (!r.Prendre(4)) {
					break;
				}
				char magie[5] = {};
				std::memcpy(magie, r.p + r.at, 4);
				r.at += 4;
				if (std::strcmp(magie, "GRAF") == 0) {
					NkString g = r.Texte();
					if (graphe != nullptr) {
						*graphe = g;
					}
				} else if (std::strcmp(magie, "MODL") == 0) {
					const uint32 n = r.U32();
					if (!r.Prendre(n)) {
						break;
					}
					if (module != nullptr) {
						if (!NkLireModuleBp(r.p + r.at, n, *module, erreur)) {
							return false;
						}
						if (aModule != nullptr) {
							*aModule = true;
						}
					}
					r.at += n;
				} else if (std::strcmp(magie, "DOCU") == 0) {
					NkString d = r.Texte();
					if (document != nullptr) {
						*document = d;
					}
				} else {
					// Une section d'une version plus recente : TOUTES portent leur
					// longueur (u32), on la saute.
					const uint32 n = r.U32();
					if (!r.Prendre(n)) {
						break;
					}
					r.at += n;
				}
			}
			if (!r.ok) {
				if (erreur != nullptr) {
					*erreur = "charge .nkbp tronquee";
				}
				return false;
			}
			return true;
		}

		bool NkEcrireFichierBp(const char *chemin, const NkString &graphe, const NkModuleBp *module, NkString *erreur,
							   const NkString *document) {
			NkVector<uint8> charge;
			NkEcrireChargeBp(graphe, module, charge, document);
			NkAssetMetadata meta;
			meta.type = NkAssetType::Blueprint;
			meta.typeName = "unkeny.Blueprint";
			meta.assetPath.path = chemin;
			meta.assetPath.name = chemin;
			NkString err;
			if (!NkAssetIO::Write(chemin, meta, charge.Data(), charge.Size(), &err)) {
				if (erreur != nullptr) {
					*erreur = err;
				}
				return false;
			}
			return true;
		}

		bool NkLireFichierBp(const char *chemin, NkString *graphe, NkModuleBp *module, bool *aModule, NkString *erreur,
							 NkString *document) {
			NkAssetMetadata meta;
			NkVector<nk_uint8> charge;
			NkString err;
			if (!NkAssetIO::ReadFull(chemin, meta, charge, &err)) {
				if (erreur != nullptr) {
					*erreur = NkString::Format("%s : %s", chemin, err.CStr());
				}
				return false;
			}
			return NkLireChargeBp(charge.Data(), charge.Size(), graphe, module, aModule, erreur, document);
		}

	} // namespace unkeny
} // namespace nkentseu
