#pragma once
// -----------------------------------------------------------------------------
// @File    NkComponentInstance.h
// @Brief   L'INSTANCE d'un composant : sa declaration + ce qu'on y a change.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE FICHIER EXISTE — ET POURQUOI IL EST SEPARE DE `NkComponentDecl.h`
// =============================================================================
//  La frontiere ecrite dans `NkComponentDecl.h` affirme que la declaration est
//  une CONSTANTE DE COMPILATION : zero allocation, zero enregistrement,
//  verifiable sans rien lier. Cette affirmation doit rester vraie.
//
//  Or l'editeur `NKUIDesign` a besoin de CHANGER des valeurs et de les
//  SAUVEGARDER. Mettre ces valeurs dans la declaration l'aurait rendue mutable
//  et allouee — c'est-a-dire aurait rendu la frontiere fausse le jour meme ou
//  elle a ete ecrite.
//
//  D'ou la separation, et c'est la seule chose a retenir de ce fichier :
//
//      DECLARATION  = ce que le composant EST        (constante, partagee, figee)
//      INSTANCE     = ce qu'on en a FAIT ICI         (mutable, allouee, sauvee)
//
//  Une instance ne contient QUE les ECARTS. Une instance vide se comporte
//  exactement comme la declaration : c'est ce qui permet a une application de ne
//  jamais en creer une si elle n'a rien a changer.
//
// =============================================================================
//  CE QUE CE FICHIER PROUVE, ET C'EST LE TEMOIN DE LA TRANCHE
// =============================================================================
//  « Changer un parametre dans NkUIDesign change le rendu SANS RECOMPILER. »
//  Ce n'est possible que si le dessin lit ses nombres A L'EXECUTION, depuis une
//  source qu'un fichier peut modifier. C'est exactement ce que fait
//  `NkContentBrowserDraw.cpp` : il appelle `inst.Metric("card_gap")`, jamais
//  `12.f`.
//
//  ⚠️ LE DEFAUT QU'ON EVITE AINSI, et il a un nom dans le corpus : « un
//     parametre qui n'est pas honore est pire qu'un parametre absent ». Une
//     instance qu'on pourrait remplir sans que le dessin la lise donnerait
//     exactement ce defaut — reglages qui ne font rien, et rien qui le signale.
//     Le banc de `NKUIDesign --probe` existe pour l'attraper : il compare les
//     commandes de dessin AVEC et SANS ecrasement.
//
// EN-TETE NEUTRE : aucune dependance d'interface, aucun acces disque. `Save`
//   rend du TEXTE et `Load` en prend — c'est l'appelant qui touche le fichier,
//   exactement comme `NkTheme::Save/Load`, dont ce fichier copie le format et
//   surtout la TOLERANCE (une cle inconnue se COMPTE, elle ne fait pas echouer).
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKEditorKit/Components/NkComponentDecl.h"
// Pour l'adaptateur `NkMetricsOf` en bas de fichier : le resolveur de
// disposition doit pouvoir lire les valeurs EDITEES, pas seulement les defauts
// compiles. La dependance va dans ce sens-la et jamais dans l'autre -- le
// resolveur reste compilable et executable sans `NkVector`/`NkString`.
#include "NKEditorKit/Components/NkLayoutSolve.h"

namespace nkentseu {
	namespace editorkit {

		namespace instdetail {
			inline bool StrEqZ(const char *a, const char *b) {
				if (!a || !b)
					return false;
				for (; *a && *b; ++a, ++b)
					if (*a != *b)
						return false;
				return *a == *b;
			}
			/// Conversion decimale minimale — pas de `<cstdlib>` (zero-STL) et
			/// surtout pas de dependance de locale : une virgule decimale selon la
			/// machine rendrait un fichier illisible sur une autre.
			inline float32 ParseFloat(const char *s) {
				if (!s)
					return 0.f;
				while (*s == ' ' || *s == '\t')
					++s;
				bool neg = false;
				if (*s == '-') {
					neg = true;
					++s;
				} else if (*s == '+')
					++s;
				float64 v = 0.0;
				while (*s >= '0' && *s <= '9')
					v = v * 10.0 + (float64)(*s++ - '0');
				if (*s == '.') {
					++s;
					float64 f = 0.1;
					while (*s >= '0' && *s <= '9') {
						v += (float64)(*s++ - '0') * f;
						f *= 0.1;
					}
				}
				return (float32)(neg ? -v : v);
			}
			/// Ecriture decimale a 4 chiffres apres la virgule, sans `snprintf` :
			/// le meme motif que `NkTheme::ToHex`, pour la meme raison (un seul
			/// point de verite sur le format du fichier).
			inline void WriteFloat(float32 x, NkString &out) {
				if (x < 0.f) {
					out.Append('-');
					x = -x;
				}
				uint64 whole = (uint64)x;
				float32 frac = x - (float32)whole;
				char buf[24];
				int32 n = 0;
				if (whole == 0)
					buf[n++] = '0';
				while (whole > 0 && n < 20) {
					buf[n++] = (char)('0' + (whole % 10));
					whole /= 10;
				}
				for (int32 i = n - 1; i >= 0; --i)
					out.Append(buf[i]);
				// Partie decimale : on n'ecrit que ce qui est significatif, sinon
				// tous les entiers du fichier trainent « .0000 » derriere eux.
				uint32 f4 = (uint32)(frac * 10000.f + 0.5f);
				if (f4 >= 10000u)
					f4 = 9999u;
				if (f4 == 0)
					return;
				out.Append('.');
				char fb[4];
				for (int32 i = 3; i >= 0; --i) {
					fb[i] = (char)('0' + (f4 % 10));
					f4 /= 10;
				}
				int32 last = 3;
				while (last > 0 && fb[last] == '0')
					--last;
				for (int32 i = 0; i <= last; ++i)
					out.Append(fb[i]);
			}
		} // namespace instdetail

		/// Un ecart nomme. `name` est une COPIE : l'instance survit a la
		/// declaration dans un seul cas — un fichier charge nommant une cle que la
		/// declaration ne connait pas — et ce cas doit se compter, pas planter.
		struct NkValueOverride {
				NkString name;
				float32 value = 0.f;
		};

		/// Un jeton reaffecte a un autre role de theme. C'est la deuxieme exigence
		/// de Rodolf (« en changeant le theme ») rendue EDITABLE : l'editeur ne
		/// change pas une couleur, il change le ROLE dont le jeton herite.
		struct NkTokenBinding {
				NkString token;
				NkString role;
		};

		// ── L'INSTANCE ──────────────────────────────────────────────────────────
		class NkComponentInstance {
			public:
				NkComponentInstance() = default;
				explicit NkComponentInstance(const NkComponentDecl &d) : mDecl(&d) {}

				void Bind(const NkComponentDecl &d) {
					mDecl = &d;
				}

				// ── LA PROVENANCE (ajout 4 de Rodolf, 2026-08-19) ────────────────
				// ⚠️ ELLE EST PORTEE PAR L'INSTANCE AUTANT QUE PAR LA DECLARATION, et
				//    c'est ici qu'elle sert VRAIMENT aujourd'hui : la declaration est
				//    ecrite en C++ par une main, tandis que l'instance est le seul
				//    objet que NkUIDesign produit, sauve et rouvre. C'est donc elle
				//    qui devient de la donnee d'entrainement, et elle qui doit dire
				//    d'ou elle vient.
				//
				// LE CAS QUI JUSTIFIE `corrected` A LUI SEUL : l'IA produit une
				// instance, Rodolf la rouvre et deplace trois valeurs. `author`
				// reste `AI` -- c'est la verite de l'origine -- et `corrected`
				// passe a vrai. Ecraser `author` en `Human` perdrait justement
				// l'information la plus chere du corpus : ce que la main a change
				// APRES la machine.
				const NkProvenance &Provenance() const {
					return mProv;
				}
				void SetProvenance(const NkProvenance &p) {
					mProv = p;
				}
				/// A appeler quand une main modifie une instance : n'ecrase pas
				/// l'auteur d'origine, marque la reprise, et invalide la
				/// verification -- une paire corrigee n'est plus la paire qui avait
				/// ete rejouee.
				void MarkCorrectedByHuman() {
					if (mProv.author != NkAuthorKind::Human)
						mProv.corrected = true;
					mProv.verified = false;
				}

				const NkComponentDecl *Decl() const {
					return mDecl;
				}

				// ── LECTURE : ecart s'il existe, sinon defaut declare ────────────
				// C'EST LA FONCTION QUE LE DESSIN APPELLE. Elle est la raison d'etre
				// de tout ce fichier : un seul appel, et le nombre vient soit du
				// fichier edite, soit de la declaration — le dessin ne sait pas
				// lequel, et n'a pas a le savoir.
				float32 Metric(const char *n, float32 fallback = 0.f) const {
					for (uint32 i = 0; i < (uint32)mMetrics.Size(); ++i)
						if (instdetail::StrEqZ(mMetrics[i].name.Data(), n))
							return mMetrics[i].value;
					return mDecl ? mDecl->Metric(n, fallback) : fallback;
				}
				float32 Param(const char *n, float32 fallback = 0.f) const {
					for (uint32 i = 0; i < (uint32)mParams.Size(); ++i)
						if (instdetail::StrEqZ(mParams[i].name.Data(), n))
							return mParams[i].value;
					return mDecl ? mDecl->Param(n, fallback) : fallback;
				}
				/// UN NOMBRE NOMME, ecart d'abord, declaration ensuite -- metrique
				/// puis parametre des deux cotes. C'est ce que lit la disposition
				/// (cf. `NkComponentDecl::Number`).
				float32 Number(const char *n, float32 fallback = 0.f) const {
					for (uint32 i = 0; i < (uint32)mMetrics.Size(); ++i)
						if (instdetail::StrEqZ(mMetrics[i].name.Data(), n))
							return mMetrics[i].value;
					for (uint32 i = 0; i < (uint32)mParams.Size(); ++i)
						if (instdetail::StrEqZ(mParams[i].name.Data(), n))
							return mParams[i].value;
					return mDecl ? mDecl->Number(n, fallback) : fallback;
				}

				/// Le NOM du role auquel un jeton est lie. La resolution en
				/// identifiant appartient a l'application (`NkResolveRole`) : ce
				/// fichier ne connait pas le theme, et c'est voulu.
				const char *TokenRole(const char *token) const {
					for (uint32 i = 0; i < (uint32)mTokens.Size(); ++i)
						if (instdetail::StrEqZ(mTokens[i].token.Data(), token))
							return mTokens[i].role.Data();
					if (mDecl) {
						const NkTokenDecl *t = mDecl->FindToken(token);
						if (t)
							return t->defaultRole;
					}
					return "";
				}
				/// ⚠️ REND -1 QUAND L'INSTANCE N'IMPOSE RIEN, et c'est deliberé.
				///    La premiere ecriture rendait 0 dans ce cas — ce qui aurait
				///    silencieusement force la variante 0 a toute application qui
				///    branche une instance sans toucher a la variante, en ecrasant
				///    le choix qu'elle avait pose dans son style. Un defaut
				///    invisible : la vue s'afficherait, simplement pas la bonne.
				///    C'est la meme famille que « un parametre qui n'est pas
				///    honore » — ici, un parametre honore par la MAUVAISE source.
				int32 Variant() const {
					return mVariant;
				}
				bool HasVariant() const {
					return mVariant >= 0;
				}

				bool IsMetricOverridden(const char *n) const {
					for (uint32 i = 0; i < (uint32)mMetrics.Size(); ++i)
						if (instdetail::StrEqZ(mMetrics[i].name.Data(), n))
							return true;
					return false;
				}
				bool IsParamOverridden(const char *n) const {
					for (uint32 i = 0; i < (uint32)mParams.Size(); ++i)
						if (instdetail::StrEqZ(mParams[i].name.Data(), n))
							return true;
					return false;
				}
				bool IsTokenOverridden(const char *n) const {
					for (uint32 i = 0; i < (uint32)mTokens.Size(); ++i)
						if (instdetail::StrEqZ(mTokens[i].token.Data(), n))
							return true;
					return false;
				}

				// ── ECRITURE ────────────────────────────────────────────────────
				// ⚠️ LES BORNES VIENNENT DE LA DECLARATION, et c'est le second
				//    endroit ou la declaration est reellement LUE (le premier etant
				//    le dessin). Un editeur qui bornerait ses curseurs lui-meme
				//    reintroduirait deux verites : la sienne et celle du composant.
				void SetParam(const char *n, float32 v) {
					if (mDecl) {
						const NkParamDecl *p = mDecl->FindParam(n);
						if (!p)
							return; // cle inconnue de la declaration : on n'invente pas
						if (p->maxVal > p->minVal) {
							if (v < p->minVal)
								v = p->minVal;
							if (v > p->maxVal)
								v = p->maxVal;
						}
					}
					Assign(mParams, n, v);
				}
				void SetMetric(const char *n, float32 v) {
					if (mDecl && !mDecl->FindMetric(n))
						return;
					if (v < 0.f)
						v = 0.f; // une metrique est une longueur
					Assign(mMetrics, n, v);
				}
				void SetTokenRole(const char *token, const char *role) {
					if (mDecl && !mDecl->FindToken(token))
						return;
					for (uint32 i = 0; i < (uint32)mTokens.Size(); ++i)
						if (instdetail::StrEqZ(mTokens[i].token.Data(), token)) {
							mTokens[i].role = NkString(role);
							return;
						}
					NkTokenBinding b;
					b.token = NkString(token);
					b.role = NkString(role);
					mTokens.PushBack(b);
				}
				void SetVariant(int32 v) {
					mVariant = v;
				}
				void SetVariantByName(const char *n) {
					if (!mDecl)
						return;
					const int32 i = mDecl->VariantIndex(n);
					if (i >= 0)
						mVariant = i;
				}

				/// Revenir au defaut declare — et le distinguer de « poser la valeur
				/// par defaut a la main » : apres `Reset`, le jour ou la declaration
				/// change, l'instance suit. Apres une pose manuelle, non.
				void ResetParam(const char *n) {
					Remove(mParams, n);
				}
				void ResetMetric(const char *n) {
					Remove(mMetrics, n);
				}
				void ResetToken(const char *token) {
					for (uint32 i = 0; i < (uint32)mTokens.Size(); ++i)
						if (instdetail::StrEqZ(mTokens[i].token.Data(), token)) {
							mTokens.RemoveAt(i);
							return;
						}
				}
				void ResetAll() {
					mParams.Clear();
					mMetrics.Clear();
					mTokens.Clear();
					mVariant = -1;
				}
				bool IsPristine() const {
					return mParams.Size() == 0 && mMetrics.Size() == 0 && mTokens.Size() == 0 &&
						   mVariant < 0;
				}
				uint32 OverrideCount() const {
					return (uint32)(mParams.Size() + mMetrics.Size() + mTokens.Size()) +
						   (mVariant >= 0 ? 1u : 0u);
				}

				// ── FICHIER ─────────────────────────────────────────────────────
				// Le format est celui de `NkTheme` : une cle, un `=`, une valeur, un
				// en-tete de version, des commentaires `#`. Ce n'est PAS un choix de
				// format engageant — cf. l'avertissement en bas de ce fichier.
				void Save(NkString &out) const {
					out = NkString("nkuicomp 1\n");
					out.Append("# Ecarts par rapport a la declaration compilee.\n");
					out.Append("# Les valeurs absentes suivent la declaration : ce fichier\n");
					out.Append("# ne contient QUE ce qui a ete change.\n");
					out.Append("composant ");
					out.Append(mDecl && mDecl->name ? mDecl->name : "");
					out.Append('\n');
					// ── LA PROVENANCE, TOUJOURS ECRITE ──────────────────────────
					// Meme quand elle vaut le defaut. Une etiquette absente serait
					// relue comme « humain, non verifie » -- ce qui est le defaut,
					// donc indiscernable d'une etiquette perdue. Trois lignes de
					// texte contre une ambiguite permanente sur tout le corpus.
					out.Append("provenance auteur = ");
					out.Append(NkAuthorName(mProv.author));
					out.Append('\n');
					out.Append("provenance verifie = ");
					out.Append(mProv.verified ? "1" : "0");
					out.Append('\n');
					out.Append("provenance corrige = ");
					out.Append(mProv.corrected ? "1" : "0");
					out.Append('\n');
					if (mVariant >= 0 && mDecl && mVariant < (int32)mDecl->variantCount) {
						out.Append("variante ");
						out.Append(mDecl->variants[mVariant].name);
						out.Append('\n');
					}
					for (uint32 i = 0; i < (uint32)mParams.Size(); ++i) {
						out.Append("param ");
						out.Append(mParams[i].name);
						out.Append(" = ");
						instdetail::WriteFloat(mParams[i].value, out);
						out.Append('\n');
					}
					for (uint32 i = 0; i < (uint32)mMetrics.Size(); ++i) {
						out.Append("metrique ");
						out.Append(mMetrics[i].name);
						out.Append(" = ");
						instdetail::WriteFloat(mMetrics[i].value, out);
						out.Append('\n');
					}
					for (uint32 i = 0; i < (uint32)mTokens.Size(); ++i) {
						out.Append("jeton ");
						out.Append(mTokens[i].token);
						out.Append(" = ");
						out.Append(mTokens[i].role);
						out.Append('\n');
					}
				}

				/// ⚠️ TOLERANT PAR CONCEPTION, comme `NkTheme::Load` : une cle que la
				///    declaration ne connait pas se COMPTE dans `outUnknown` au lieu
				///    de faire echouer le chargement. Sans ca, retirer un parametre
				///    d'un composant rendrait illisibles tous les fichiers deja
				///    enregistres — et le premier a en souffrir serait Rodolf.
				bool Load(const char *text, uint32 *outUnknown = nullptr, uint32 *outApplied = nullptr) {
					if (outUnknown)
						*outUnknown = 0;
					if (outApplied)
						*outApplied = 0;
					if (!text)
						return false;
					ResetAll();
					// La provenance se relit du fichier ou retombe au defaut. La
					// remettre a zero ICI et pas dans `ResetAll` : `ResetAll` est
					// aussi le bouton « Reinitialiser » de l'editeur, et remettre a
					// zero les ecarts n'efface pas qui a produit le fichier.
					mProv = NkProvenance{};

					const char *p = text;
					bool sawHeader = false;
					char kind[24], key[64], val[64];
					while (*p) {
						while (*p == ' ' || *p == '\t')
							++p;
						if (*p == '#' || *p == '\n' || *p == '\r') {
							while (*p && *p != '\n')
								++p;
							if (*p)
								++p;
							continue;
						}
						uint32 k = 0;
						while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' && k < 23)
							kind[k++] = *p++;
						kind[k] = 0;
						while (*p == ' ' || *p == '\t')
							++p;

						if (instdetail::StrEqZ(kind, "nkuicomp")) {
							sawHeader = true;
							while (*p && *p != '\n')
								++p;
						} else if (instdetail::StrEqZ(kind, "composant")) {
							// Informatif. On NE refuse PAS un fichier dont le nom de
							// composant differe : l'appelant a choisi la declaration a
							// laquelle il relie l'instance, et c'est son droit.
							while (*p && *p != '\n')
								++p;
						} else if (instdetail::StrEqZ(kind, "variante")) {
							uint32 v = 0;
							while (*p && *p != '\n' && *p != '\r' && v < 63)
								val[v++] = *p++;
							val[v] = 0;
							TrimEnd(val);
							const int32 idx = mDecl ? mDecl->VariantIndex(val) : -1;
							if (idx >= 0) {
								mVariant = idx;
								if (outApplied)
									(*outApplied)++;
							} else if (outUnknown)
								(*outUnknown)++;
						} else if (instdetail::StrEqZ(kind, "provenance")) {
							// Meme grammaire que les autres : `cle = valeur`. Une cle
							// de provenance inconnue se COMPTE comme inconnue, elle ne
							// fait pas echouer -- meme tolerance que partout ailleurs
							// dans ce chargeur.
							uint32 n = 0;
							while (*p && *p != ' ' && *p != '\t' && *p != '=' && *p != '\n' &&
								   *p != '\r' && n < 63)
								key[n++] = *p++;
							key[n] = 0;
							while (*p == ' ' || *p == '\t')
								++p;
							if (*p == '=') {
								++p;
								while (*p == ' ' || *p == '\t')
									++p;
							}
							uint32 v = 0;
							while (*p && *p != '\n' && *p != '\r' && v < 63)
								val[v++] = *p++;
							val[v] = 0;
							TrimEnd(val);

							bool ok = true;
							if (instdetail::StrEqZ(key, "auteur")) {
								if (instdetail::StrEqZ(val, "humain"))
									mProv.author = NkAuthorKind::Human;
								else if (instdetail::StrEqZ(val, "ia"))
									mProv.author = NkAuthorKind::AI;
								else if (instdetail::StrEqZ(val, "importe"))
									mProv.author = NkAuthorKind::Imported;
								else
									ok = false; // auteur inconnu : on n'invente pas une origine
							} else if (instdetail::StrEqZ(key, "verifie"))
								mProv.verified = (val[0] == '1');
							else if (instdetail::StrEqZ(key, "corrige"))
								mProv.corrected = (val[0] == '1');
							else
								ok = false;

							if (ok) {
								if (outApplied)
									(*outApplied)++;
							} else if (outUnknown)
								(*outUnknown)++;
						} else if (instdetail::StrEqZ(kind, "param") ||
								   instdetail::StrEqZ(kind, "metrique") ||
								   instdetail::StrEqZ(kind, "jeton")) {
							uint32 n = 0;
							while (*p && *p != ' ' && *p != '\t' && *p != '=' && *p != '\n' &&
								   *p != '\r' && n < 63)
								key[n++] = *p++;
							key[n] = 0;
							while (*p == ' ' || *p == '\t')
								++p;
							if (*p == '=') {
								++p;
								while (*p == ' ' || *p == '\t')
									++p;
							}
							uint32 v = 0;
							while (*p && *p != '\n' && *p != '\r' && v < 63)
								val[v++] = *p++;
							val[v] = 0;
							TrimEnd(val);

							const uint32 before = OverrideCount();
							if (instdetail::StrEqZ(kind, "param"))
								SetParam(key, instdetail::ParseFloat(val));
							else if (instdetail::StrEqZ(kind, "metrique"))
								SetMetric(key, instdetail::ParseFloat(val));
							else
								SetTokenRole(key, val);
							// `Set*` refuse en silence une cle inconnue de la
							// declaration : c'est cette difference de compte qui le
							// detecte. Sans elle, un fichier entierement perime se
							// chargerait « avec succes » en n'appliquant rien.
							if (OverrideCount() > before) {
								if (outApplied)
									(*outApplied)++;
							} else if (outUnknown)
								(*outUnknown)++;
						} else {
							if (outUnknown)
								(*outUnknown)++;
							while (*p && *p != '\n')
								++p;
						}
						while (*p && *p != '\n')
							++p;
						if (*p)
							++p;
					}
					return sawHeader;
				}

			private:
				static void TrimEnd(char *s) {
					int32 n = 0;
					while (s[n])
						++n;
					while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r'))
						s[--n] = 0;
				}
				static void Assign(NkVector<NkValueOverride> &v, const char *n, float32 x) {
					for (uint32 i = 0; i < (uint32)v.Size(); ++i)
						if (instdetail::StrEqZ(v[i].name.Data(), n)) {
							v[i].value = x;
							return;
						}
					NkValueOverride o;
					o.name = NkString(n);
					o.value = x;
					v.PushBack(o);
				}
				static void Remove(NkVector<NkValueOverride> &v, const char *n) {
					for (uint32 i = 0; i < (uint32)v.Size(); ++i)
						if (instdetail::StrEqZ(v[i].name.Data(), n)) {
							v.RemoveAt(i);
							return;
						}
				}

				const NkComponentDecl *mDecl = nullptr;
				/// ⚠️ HORS de `IsPristine()` et de `OverrideCount()` a dessein : la
				///    provenance n'est pas un ECART, c'est une etiquette sur le
				///    fichier. Une instance qui n'a rien change reste vierge meme si
				///    elle sait qui l'a produite -- sinon toute instance deviendrait
				///    « modifiee » du seul fait d'etre tracee, et le test 5 de la
				///    sonde (« une instance vierge se comporte comme la
				///    declaration ») mesurerait autre chose que ce qu'il annonce.
				NkProvenance mProv;
				NkVector<NkValueOverride> mParams;
				NkVector<NkValueOverride> mMetrics;
				NkVector<NkTokenBinding> mTokens;
				int32 mVariant = -1; ///< -1 = celle de la declaration
		};

		// ── LA DISPOSITION SUIT LES VALEURS EDITEES ─────────────────────────────
		// Sans cet adaptateur, `NkSolveLayout` lirait les defauts COMPILES et une
		// gouttiere changee dans NkUIDesign ne deplacerait rien. Ce serait « un
		// parametre qui n'est pas honore » sous sa forme la plus trompeuse : les
		// couleurs suivraient l'edition, les positions non, et on chercherait le
		// defaut dans le dessin.
		inline NkMetricSource NkMetricsOf(const NkComponentInstance &inst) {
			NkMetricSource s;
			s.user = &inst;
			s.get = [](const void *u, const char *n, float32 f) -> float32 {
				return ((const NkComponentInstance *)u)->Number(n, f);
			};
			return s;
		}

		// ⚠️ CE QUE CE FORMAT N'ENGAGE PAS, et il faut le dire avant qu'on le
		//    relaie autrement. Le §9 du devis pose « le format de fichier des
		//    declarations » comme un arbitrage de RODOLF, et il le reste. Ce qui
		//    est ecrit ici n'est PAS le format d'une declaration : c'est le format
		//    d'un fichier d'ECARTS, qui ne decrit aucun composant et ne peut pas
		//    en decrire un — il ne contient ni type, ni borne, ni evenement, ni
		//    variante nouvelle, rien qu'un nom deja connu et un nombre.
		//
		//    Il est donc jetable : le jour ou le format des declarations est
		//    choisi, ce fichier d'ecarts disparait dedans en une passe, et rien
		//    d'autre ne bouge. C'etait la condition pour livrer la tranche sans
		//    prendre a Rodolf une decision qui lui appartient.

	} // namespace editorkit
} // namespace nkentseu
