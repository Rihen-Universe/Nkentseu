#pragma once
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Kernel/System/NKConverse/src/NKConverse/NkConverseJson.h
// @Brief   UN ARBRE JSON PETIT ET COMPLET : lire une reponse de modele, la
//          parcourir, la modifier, la reecrire.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POURQUOI UN ARBRE ICI (2026-10-01, panneau IA d'Unkeny)
//   Les dorsaux d'avant n'avaient besoin que d'UN champ d'un objet plat
//   (`ExtraireChamp` d'Ollama, `"response"`). Les fournisseurs a outils parlent
//   en objets IMBRIQUES : `message.tool_calls[0].function.arguments` (Ollama),
//   `choices[0].delta.tool_calls[i].function.arguments` (OpenAI), des
//   evenements `content_block_delta` (Anthropic). Chercher une sous-chaine dans
//   ces corps, c'est confondre le texte d'un message avec une cle -- un modele
//   qui ECRIT `"name":` dans sa reponse ferait appeler un outil.
//
//   ⚠️ LE DEPOT EN AVAIT DEJA DEUX, ET AUCUN NE CONVENAIT A UN MODULE PARTAGE :
//     - `NKSerialization/JSON` lit dans une NkArchive (des cles typees, pas un
//       arbre qu'on parcourt) ;
//     - `NKCode/Project/NkLsp.h` (NkJsonDoc) est un arbre a pool -- le bon
//       patron, repris ici -- mais il vit dans une APPLICATION : NKCraft et
//       Unkeny ne peuvent pas inclure NKCode. Il ne decode pas \uXXXX (les
//       accents d'un modele qui echappe), et ne sait pas reecrire.
//     Le jour ou NkLsp.h le veut, il inclut celui-ci : c'est la direction qui
//     ne casse personne (un module ne monte jamais vers une application).
//
// CE QU'IL FAIT
//   - Un POOL d'indices (pas de recursion de types, pas d'allocation par noeud).
//   - Les NOMBRES gardent leur LEXEME : un objet relu puis reecrit sans
//     modification ressort a l'octet pres (un uid 64 bits, un 0.1f d'une scene
//     ne passent pas par un double qui les arrondirait).
//   - \uXXXX decode en UTF-8, paires de substitution comprises.
//   - Reecriture compacte, fusion profonde d'un objet dans un autre (le
//     « patch » qu'un modele envoie pour changer trois champs d'une entite).
//   - Profondeur bornee (128) : un corps malveillant ne fait pas deborder la pile.
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu::converse {

	/// Ajoute `s` a `out` echappe pour une chaine JSON (sans les guillemets).
	inline void NkJsonEchapper(const char *s, NkString &out) {
		if (!s)
			return;
		char b[8];
		for (const unsigned char *p = reinterpret_cast<const unsigned char *>(s); *p; ++p) {
			const unsigned char c = *p;
			if (c == '"')
				out.Append("\\\"");
			else if (c == '\\')
				out.Append("\\\\");
			else if (c == '\n')
				out.Append("\\n");
			else if (c == '\r')
				out.Append("\\r");
			else if (c == '\t')
				out.Append("\\t");
			else if (c < 0x20u) {
				std::snprintf(b, sizeof(b), "\\u%04X", static_cast<unsigned>(c));
				out.Append(b);
			} else
				out.Append(static_cast<char>(c));
		}
	}
	inline void NkJsonEchapper(const NkString &s, NkString &out) {
		NkJsonEchapper(s.CStr(), out);
	}
	/// La chaine JSON complete : `"..."`.
	inline void NkJsonChaine(const char *s, NkString &out) {
		out.Append('"');
		NkJsonEchapper(s, out);
		out.Append('"');
	}
	inline void NkJsonChaine(const NkString &s, NkString &out) {
		NkJsonChaine(s.CStr(), out);
	}

	class NkJsonDoc {
		public:
			enum Genre : uint8 { NUL = 0, BOOLEEN, NOMBRE, TEXTE, TABLEAU, OBJET };
			struct Noeud {
					uint8 genre = NUL;
					bool b = false;
					NkString texte;			   ///< TEXTE : la chaine decodee ; NOMBRE : le lexeme
					NkVector<NkString> cles;   ///< OBJET : la cle de chaque enfant
					NkVector<int32> enfants;   ///< OBJET / TABLEAU : indices dans le pool
			};

			// ── LIRE ────────────────────────────────────────────────────────
			/// Rend false ET NOMME la raison (avec l'octet) si le texte n'est pas
			/// un JSON complet. Un document refuse est vide : on ne lit pas a moitie.
			bool Lire(const char *t, usize n, NkString *erreur = nullptr) {
				mPool.Clear();
				mRacine = -1;
				mT = t ? t : "";
				mN = t ? n : 0u;
				mI = 0u;
				mErreur = NkString();
				Blancs();
				const int32 r = Valeur(0);
				Blancs();
				if (r >= 0 && mI != mN && mErreur.Empty())
					Echouer("du texte apres la valeur");
				if (r < 0 || !mErreur.Empty()) {
					if (erreur)
						*erreur = mErreur;
					mPool.Clear();
					return false;
				}
				mRacine = r;
				return true;
			}
			bool Lire(const NkString &s, NkString *erreur = nullptr) {
				return Lire(s.CStr(), static_cast<usize>(s.Length()), erreur);
			}

			int32 Racine() const {
				return mRacine;
			}
			bool Valide(int32 i) const {
				return i >= 0 && i < static_cast<int32>(mPool.Size());
			}
			uint8 GenreDe(int32 i) const {
				return Valide(i) ? mPool[static_cast<usize>(i)].genre : static_cast<uint8>(NUL);
			}
			bool EstObjet(int32 i) const {
				return GenreDe(i) == OBJET && Valide(i);
			}
			bool EstTableau(int32 i) const {
				return GenreDe(i) == TABLEAU && Valide(i);
			}
			bool EstTexte(int32 i) const {
				return GenreDe(i) == TEXTE && Valide(i);
			}
			const Noeud *N(int32 i) const {
				return Valide(i) ? &mPool[static_cast<usize>(i)] : nullptr;
			}
			/// Le nombre d'enfants (objet, tableau), 0 sinon.
			uint32 Taille(int32 i) const {
				return Valide(i) ? static_cast<uint32>(mPool[static_cast<usize>(i)].enfants.Size()) : 0u;
			}
			int32 Element(int32 tab, uint32 k) const {
				if (!Valide(tab) || k >= Taille(tab))
					return -1;
				return mPool[static_cast<usize>(tab)].enfants[k];
			}
			const char *Cle(int32 obj, uint32 k) const {
				if (!EstObjet(obj) || k >= Taille(obj))
					return "";
				return mPool[static_cast<usize>(obj)].cles[k].CStr();
			}
			/// Le membre `cle` d'un objet, -1 s'il manque (le DERNIER s'il est repete).
			int32 Membre(int32 obj, const char *cle) const {
				if (!EstObjet(obj) || !cle)
					return -1;
				const Noeud &o = mPool[static_cast<usize>(obj)];
				for (usize k = o.cles.Size(); k > 0; --k)
					if (o.cles[k - 1] == cle)
						return o.enfants[k - 1];
				return -1;
			}
			/// Un CHEMIN pointe : « message.tool_calls.0.function.name ». Un segment
			/// tout en chiffres indexe un tableau. -1 au premier pas manque.
			int32 Chemin(int32 i, const char *chemin) const {
				const char *p = chemin;
				while (p && *p && i >= 0) {
					const char *f = p;
					while (*f && *f != '.')
						++f;
					NkString seg(p, static_cast<NkString::SizeType>(f - p));
					bool chiffres = seg.Length() > 0;
					for (NkString::SizeType k = 0; k < seg.Length(); ++k)
						chiffres = chiffres && seg[k] >= '0' && seg[k] <= '9';
					if (EstTableau(i) && chiffres)
						i = Element(i, static_cast<uint32>(std::atoi(seg.CStr())));
					else
						i = Membre(i, seg.CStr());
					p = *f ? f + 1 : f;
				}
				return i;
			}
			/// Le texte d'une chaine ; un nombre rend son lexeme ; sinon `defaut`.
			NkString Texte(int32 i, const char *defaut = "") const {
				if (Valide(i) && (GenreDe(i) == TEXTE || GenreDe(i) == NOMBRE))
					return mPool[static_cast<usize>(i)].texte;
				return NkString(defaut ? defaut : "");
			}
			NkString TexteDe(int32 obj, const char *chemin, const char *defaut = "") const {
				return Texte(Chemin(obj, chemin), defaut);
			}
			double Nombre(int32 i, double defaut = 0.0) const {
				if (Valide(i) && GenreDe(i) == NOMBRE)
					return std::atof(mPool[static_cast<usize>(i)].texte.CStr());
				if (Valide(i) && GenreDe(i) == TEXTE) {
					// Un modele local ecrit souvent « "x": "2.5" » : on l'accepte, c'est
					// une valeur, pas une faute de forme qui vaille un refus.
					const char *s = mPool[static_cast<usize>(i)].texte.CStr();
					char *fin = nullptr;
					const double v = std::strtod(s, &fin);
					if (fin && fin != s)
						return v;
				}
				return defaut;
			}
			bool Booleen(int32 i, bool defaut = false) const {
				if (Valide(i) && GenreDe(i) == BOOLEEN)
					return mPool[static_cast<usize>(i)].b;
				if (Valide(i) && GenreDe(i) == TEXTE) {
					const NkString &s = mPool[static_cast<usize>(i)].texte;
					if (s == "true" || s == "vrai" || s == "oui")
						return true;
					if (s == "false" || s == "faux" || s == "non")
						return false;
				}
				return defaut;
			}

			// ── ECRIRE ──────────────────────────────────────────────────────
			/// La forme COMPACTE du noeud `i` (le document entier : Racine()).
			void Ecrire(int32 i, NkString &out) const {
				if (!Valide(i)) {
					out.Append("null");
					return;
				}
				const Noeud &n = mPool[static_cast<usize>(i)];
				switch (n.genre) {
					case NUL:
						out.Append("null");
						break;
					case BOOLEEN:
						out.Append(n.b ? "true" : "false");
						break;
					case NOMBRE:
						out.Append(n.texte);
						break;
					case TEXTE:
						NkJsonChaine(n.texte, out);
						break;
					case TABLEAU:
						out.Append('[');
						for (usize k = 0; k < n.enfants.Size(); ++k) {
							if (k)
								out.Append(',');
							Ecrire(n.enfants[k], out);
						}
						out.Append(']');
						break;
					case OBJET:
						out.Append('{');
						for (usize k = 0; k < n.enfants.Size(); ++k) {
							if (k)
								out.Append(',');
							NkJsonChaine(n.cles[k], out);
							out.Append(':');
							Ecrire(n.enfants[k], out);
						}
						out.Append('}');
						break;
					default:
						out.Append("null");
				}
			}
			NkString Ecrire(int32 i) const {
				NkString s;
				Ecrire(i, s);
				return s;
			}

			// ── MODIFIER ────────────────────────────────────────────────────
			int32 NouvelObjet() {
				return Nouveau(OBJET);
			}
			int32 NouveauTableau() {
				return Nouveau(TABLEAU);
			}
			int32 NouveauTexte(const char *t) {
				const int32 i = Nouveau(TEXTE);
				mPool[static_cast<usize>(i)].texte = NkString(t ? t : "");
				return i;
			}
			int32 NouveauNombre(double v) {
				char b[48];
				std::snprintf(b, sizeof(b), "%.9g", v);
				const int32 i = Nouveau(NOMBRE);
				mPool[static_cast<usize>(i)].texte = NkString(b);
				return i;
			}
			int32 NouveauBooleen(bool v) {
				const int32 i = Nouveau(BOOLEEN);
				mPool[static_cast<usize>(i)].b = v;
				return i;
			}
			/// Pose (ou REMPLACE) le membre `cle` de l'objet `obj`.
			void Poser(int32 obj, const char *cle, int32 val) {
				if (!EstObjet(obj) || !Valide(val))
					return;
				Noeud &o = mPool[static_cast<usize>(obj)];
				for (usize k = 0; k < o.cles.Size(); ++k)
					if (o.cles[k] == cle) {
						o.enfants[k] = val;
						return;
					}
				o.cles.PushBack(NkString(cle ? cle : ""));
				o.enfants.PushBack(val);
			}
			void Ajouter(int32 tab, int32 val) {
				if (EstTableau(tab) && Valide(val))
					mPool[static_cast<usize>(tab)].enfants.PushBack(val);
			}
			/// Copie PROFONDE du noeud `i` d'un autre document dans celui-ci.
			int32 Copier(const NkJsonDoc &src, int32 i, uint32 profondeur = 0u) {
				if (!src.Valide(i) || profondeur > 128u)
					return Nouveau(NUL);
				const Noeud n = src.mPool[static_cast<usize>(i)]; // copie : le pool peut grandir
				const int32 d = Nouveau(n.genre);
				mPool[static_cast<usize>(d)].b = n.b;
				mPool[static_cast<usize>(d)].texte = n.texte;
				for (usize k = 0; k < n.enfants.Size(); ++k) {
					const int32 e = Copier(src, n.enfants[k], profondeur + 1u);
					mPool[static_cast<usize>(d)].enfants.PushBack(e);
					if (n.genre == OBJET)
						mPool[static_cast<usize>(d)].cles.PushBack(n.cles[k]);
				}
				return d;
			}
			/// FUSION PROFONDE : chaque membre de `patch` (un objet de `src`) entre
			/// dans l'objet `cible` ; deux objets se fusionnent, tout le reste
			/// REMPLACE. C'est la forme du « change ces trois champs, garde le reste ».
			void Fusionner(int32 cible, const NkJsonDoc &src, int32 patch, uint32 profondeur = 0u) {
				if (!EstObjet(cible) || !src.EstObjet(patch) || profondeur > 128u)
					return;
				for (uint32 k = 0; k < src.Taille(patch); ++k) {
					const char *cle = src.Cle(patch, k);
					const int32 v = src.Element(patch, k);
					const int32 ici = Membre(cible, cle);
					if (EstObjet(ici) && src.EstObjet(v))
						Fusionner(ici, src, v, profondeur + 1u);
					else {
						const NkString cleCopie(cle);
						const int32 c = Copier(src, v, profondeur + 1u);
						Poser(cible, cleCopie.CStr(), c);
					}
				}
			}

		private:
			NkVector<Noeud> mPool;
			int32 mRacine = -1;
			const char *mT = "";
			usize mN = 0u;
			usize mI = 0u;
			NkString mErreur;

			int32 Nouveau(uint8 g) {
				Noeud n;
				n.genre = g;
				mPool.PushBack(n);
				return static_cast<int32>(mPool.Size()) - 1;
			}
			void Echouer(const char *quoi) {
				if (!mErreur.Empty())
					return;
				char b[160];
				std::snprintf(b, sizeof(b), "JSON illisible a l'octet %u : %s", static_cast<unsigned>(mI), quoi);
				mErreur = NkString(b);
			}
			void Blancs() {
				while (mI < mN && (mT[mI] == ' ' || mT[mI] == '\t' || mT[mI] == '\n' || mT[mI] == '\r'))
					++mI;
			}
			bool Mot(const char *m) {
				usize k = 0;
				while (m[k]) {
					if (mI + k >= mN || mT[mI + k] != m[k])
						return false;
					++k;
				}
				mI += k;
				return true;
			}
			static void Utf8(uint32 cp, NkString &out) {
				if (cp < 0x80u)
					out.Append(static_cast<char>(cp));
				else if (cp < 0x800u) {
					out.Append(static_cast<char>(0xC0u | (cp >> 6)));
					out.Append(static_cast<char>(0x80u | (cp & 0x3Fu)));
				} else if (cp < 0x10000u) {
					out.Append(static_cast<char>(0xE0u | (cp >> 12)));
					out.Append(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
					out.Append(static_cast<char>(0x80u | (cp & 0x3Fu)));
				} else {
					out.Append(static_cast<char>(0xF0u | (cp >> 18)));
					out.Append(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
					out.Append(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
					out.Append(static_cast<char>(0x80u | (cp & 0x3Fu)));
				}
			}
			bool Hex4(uint32 &v) {
				v = 0u;
				for (int k = 0; k < 4; ++k) {
					if (mI >= mN)
						return false;
					const char c = mT[mI++];
					v <<= 4;
					if (c >= '0' && c <= '9')
						v |= static_cast<uint32>(c - '0');
					else if (c >= 'a' && c <= 'f')
						v |= static_cast<uint32>(c - 'a' + 10);
					else if (c >= 'A' && c <= 'F')
						v |= static_cast<uint32>(c - 'A' + 10);
					else
						return false;
				}
				return true;
			}
			bool Chaine(NkString &out) {
				// mT[mI] == '"'
				++mI;
				usize debut = mI;
				while (mI < mN) {
					const char c = mT[mI];
					if (c == '"') {
						out.Append(mT + debut, static_cast<NkString::SizeType>(mI - debut));
						++mI;
						return true;
					}
					if (c != '\\') {
						++mI;
						continue;
					}
					out.Append(mT + debut, static_cast<NkString::SizeType>(mI - debut));
					++mI;
					if (mI >= mN)
						break;
					const char e = mT[mI++];
					switch (e) {
						case 'n': out.Append('\n'); break;
						case 't': out.Append('\t'); break;
						case 'r': out.Append('\r'); break;
						case 'b': out.Append('\b'); break;
						case 'f': out.Append('\f'); break;
						case 'u': {
							uint32 cp = 0u;
							if (!Hex4(cp)) {
								Echouer("\\u sans quatre chiffres hexadecimaux");
								return false;
							}
							if (cp >= 0xD800u && cp <= 0xDBFFu && mI + 6u <= mN && mT[mI] == '\\' && mT[mI + 1] == 'u') {
								mI += 2u;
								uint32 bas = 0u;
								if (Hex4(bas) && bas >= 0xDC00u && bas <= 0xDFFFu)
									cp = 0x10000u + ((cp - 0xD800u) << 10) + (bas - 0xDC00u);
								else
									cp = 0xFFFDu;
							}
							Utf8(cp, out);
							break;
						}
						default: out.Append(e); break; // \" \\ \/
					}
					debut = mI;
				}
				Echouer("chaine jamais fermee");
				return false;
			}
			int32 Valeur(uint32 profondeur) {
				if (profondeur > 128u) {
					Echouer("imbrication trop profonde");
					return -1;
				}
				Blancs();
				if (mI >= mN) {
					Echouer("valeur attendue, fin du texte");
					return -1;
				}
				const char c = mT[mI];
				if (c == '{') {
					++mI;
					const int32 o = Nouveau(OBJET);
					Blancs();
					if (mI < mN && mT[mI] == '}') {
						++mI;
						return o;
					}
					for (;;) {
						Blancs();
						if (mI >= mN || mT[mI] != '"') {
							Echouer("cle d'objet attendue");
							return -1;
						}
						NkString cle;
						if (!Chaine(cle))
							return -1;
						Blancs();
						if (mI >= mN || mT[mI] != ':') {
							Echouer("« : » attendu apres une cle");
							return -1;
						}
						++mI;
						const int32 v = Valeur(profondeur + 1u);
						if (v < 0)
							return -1;
						mPool[static_cast<usize>(o)].cles.PushBack(cle);
						mPool[static_cast<usize>(o)].enfants.PushBack(v);
						Blancs();
						if (mI < mN && mT[mI] == ',') {
							++mI;
							continue;
						}
						if (mI < mN && mT[mI] == '}') {
							++mI;
							return o;
						}
						Echouer("« , » ou « } » attendu dans un objet");
						return -1;
					}
				}
				if (c == '[') {
					++mI;
					const int32 t = Nouveau(TABLEAU);
					Blancs();
					if (mI < mN && mT[mI] == ']') {
						++mI;
						return t;
					}
					for (;;) {
						const int32 v = Valeur(profondeur + 1u);
						if (v < 0)
							return -1;
						mPool[static_cast<usize>(t)].enfants.PushBack(v);
						Blancs();
						if (mI < mN && mT[mI] == ',') {
							++mI;
							continue;
						}
						if (mI < mN && mT[mI] == ']') {
							++mI;
							return t;
						}
						Echouer("« , » ou « ] » attendu dans un tableau");
						return -1;
					}
				}
				if (c == '"') {
					const int32 s = Nouveau(TEXTE);
					NkString v;
					if (!Chaine(v))
						return -1;
					mPool[static_cast<usize>(s)].texte = v;
					return s;
				}
				if (Mot("true")) {
					const int32 b = Nouveau(BOOLEEN);
					mPool[static_cast<usize>(b)].b = true;
					return b;
				}
				if (Mot("false"))
					return Nouveau(BOOLEEN);
				if (Mot("null"))
					return Nouveau(NUL);
				if (c == '-' || (c >= '0' && c <= '9')) {
					const usize debut = mI;
					++mI;
					while (mI < mN) {
						const char d = mT[mI];
						if ((d >= '0' && d <= '9') || d == '.' || d == 'e' || d == 'E' || d == '+' || d == '-')
							++mI;
						else
							break;
					}
					const int32 n = Nouveau(NOMBRE);
					mPool[static_cast<usize>(n)].texte = NkString(mT + debut, static_cast<NkString::SizeType>(mI - debut));
					return n;
				}
				Echouer("caractere inattendu");
				return -1;
			}
	};

} // namespace nkentseu::converse
