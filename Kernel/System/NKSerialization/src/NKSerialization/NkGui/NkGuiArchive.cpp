// =============================================================================
// NKSerialization/NkGui/NkGuiArchive.cpp
// Le lecteur et l'ecrivain `.nkgui`, cote NkArchive.
//
// Trois passes, et la deuxieme est celle qui rend l'aller-retour a l'octet
// possible :
//   1. LEXEUR       -- la suite des jetons, avec l'OFFSET de debut et de fin de
//                      chacun. Sans ces offsets, ni la trivia ni la conservation
//                      verbatim d'une tranche inconnue ne sont possibles : les
//                      deux ont besoin de retrouver la SOURCE, pas le jeton.
//   2. TRIVIA       -- une passe unique sur les INTERVALLES entre jetons. Ce qui
//                      separe deux jetons est du texte que personne ne modelise
//                      et que tout le monde veut retrouver : commentaires,
//                      lignes vides, indentation.
//   3. ANALYSEUR    -- descendant, un seul jeton d'avance, et il ne connait que
//                      trois formes : `cle = valeur`, `Type "id" { ... }`, et
//                      « tout le reste », conserve en tranche de source.
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================

#include "NKSerialization/NkGui/NkGuiArchive.h"

namespace nkentseu {

	namespace {

		// =====================================================================
		//  LE LEXEUR
		// =====================================================================

		enum class Tk : nk_uint8 {
			End = 0,
			Ident,	 ///< [A-Za-z_][A-Za-z0-9_]* , eventuellement pointe
			Str,	 ///< "..."
			Num,	 ///< -?[0-9]+(\.[0-9]+)?
			Punct	 ///< tout le reste, un ou deux caracteres
		};

		struct Tok {
				Tk kind = Tk::End;
				NkString text;	 ///< Ident : le texte. Str : le contenu DECODE.
				NkString raw;	 ///< le lexeme source exact
				char p0 = 0;	 ///< Punct : le premier caractere
				nk_uint32 begin = 0;
				nk_uint32 end = 0;
				nk_uint32 line = 1;		///< ligne du PREMIER octet
				nk_uint32 endLine = 1;	///< ligne du DERNIER octet (une chaine peut sauter des lignes)
				nk_uint32 column = 1;

				/// Les lignes COMPLETES qui precedent le jeton, VERBATIM, terminateurs
				/// compris. Elles se reemettent telles quelles : une ligne de
				/// commentaire n'a pas de « profondeur » dans le modele, la
				/// reindenter serait decider a la place de l'auteur.
				///
				/// ⚠️ Si le reste de ligne juste avant le jeton porte autre chose que
				///    de l'espace (un commentaire de bloc referme sur la meme ligne),
				///    il est GARDE dans `lead` sans terminateur final. L'ecrivain
				///    reconnait ce cas au fait que `lead` ne finit pas par un saut de
				///    ligne, et n'indente alors pas : l'indentation d'origine est
				///    deja dedans.
				NkString lead;

				/// Le reste de la ligne APRES ce jeton, verbatim, sans terminateur.
				NkString trail;
		};

		inline bool IsSpace(char c) {
			return c == ' ' || c == '\t' || c == '\r' || c == '\n';
		}
		inline bool IsDigit(char c) {
			return c >= '0' && c <= '9';
		}
		/// ⚠️ C'EST ICI QUE NAIT LA GARANTIE DES CLES RESERVEES. `$` n'est pas un
		///    caractere d'identifiant, donc aucun fichier `.nkgui` valide ne peut
		///    nommer une propriete `$type`. La couche d'archive EMPRUNTE cette
		///    garantie ; le canari qui la surveille est le controle « $ refuse » du
		///    banc corpus, et il est ici, la ou la garantie est PRODUITE.
		inline bool IsAlpha(char c) {
			return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
		}

		class Lexer {
			public:
				Lexer(const char *src, nk_uint32 len) : mSrc(src), mLen(len) {}

				nk_uint32 FirstOffset() const {
					if (mLen >= 3 && (nk_uint8)mSrc[0] == 0xEF && (nk_uint8)mSrc[1] == 0xBB
						&& (nk_uint8)mSrc[2] == 0xBF) {
						return 3;
					}
					return 0;
				}

				bool Run(NkVector<Tok> &out, NkGuiDiag &err) {
					mPos = FirstOffset();
					while (true) {
						if (!SkipTrivia(err)) {
							return false;
						}
						if (mPos >= mLen) {
							Tok t;
							t.kind = Tk::End;
							t.begin = mLen;
							t.end = mLen;
							t.line = mLine;
							t.endLine = mLine;
							t.column = mCol;
							out.PushBack(t);
							return true;
						}
						Tok tok;
						tok.begin = mPos;
						tok.line = mLine;
						tok.column = mCol;
						if (!Next(tok, err)) {
							return false;
						}
						tok.end = mPos;
						tok.endLine = mLine;
						out.PushBack(tok);
					}
				}

			private:
				const char *mSrc = nullptr;
				nk_uint32 mLen = 0;
				nk_uint32 mPos = 0;
				nk_uint32 mLine = 1;
				nk_uint32 mCol = 1;

				void Advance() {
					if (mSrc[mPos] == '\n') {
						++mLine;
						mCol = 1;
					} else {
						++mCol;
					}
					++mPos;
				}

				bool SkipTrivia(NkGuiDiag &err) {
					while (mPos < mLen) {
						const char c = mSrc[mPos];
						if (IsSpace(c)) {
							Advance();
							continue;
						}
						if (c == '/' && mPos + 1 < mLen && mSrc[mPos + 1] == '/') {
							while (mPos < mLen && mSrc[mPos] != '\n') {
								Advance();
							}
							continue;
						}
						if (c == '/' && mPos + 1 < mLen && mSrc[mPos + 1] == '*') {
							const nk_uint32 l = mLine;
							const nk_uint32 col = mCol;
							Advance();
							Advance();
							bool closed = false;
							while (mPos < mLen) {
								if (mSrc[mPos] == '*' && mPos + 1 < mLen && mSrc[mPos + 1] == '/') {
									Advance();
									Advance();
									closed = true;
									break;
								}
								Advance();
							}
							if (!closed) {
								// Un commentaire de bloc jamais referme avale la fin du
								// fichier EN SILENCE si on ne le dit pas : le document se
								// lirait « valide » avec la moitie de son contenu disparue.
								err.code = NkString("E-PARSE");
								err.message = NkString("commentaire de bloc jamais ferme");
								err.line = l;
								err.column = col;
								return false;
							}
							continue;
						}
						break;
					}
					return true;
				}

				bool Next(Tok &tok, NkGuiDiag &err) {
					const nk_uint32 begin = mPos;
					const char c = mSrc[mPos];

					if (c == '"') {
						return LexString(tok, err);
					}
					if (IsDigit(c)) {
						return LexNumber(tok);
					}
					if (IsAlpha(c)) {
						while (mPos < mLen && (IsAlpha(mSrc[mPos]) || IsDigit(mSrc[mPos]))) {
							Advance();
						}
						// L'IDENTIFIANT POINTE (`n1.value`, `Enum.X`) est UN jeton : le
						// chemin complet est ce qui se reemet.
						while (mPos + 1 < mLen && mSrc[mPos] == '.' && IsAlpha(mSrc[mPos + 1])) {
							Advance();
							while (mPos < mLen && (IsAlpha(mSrc[mPos]) || IsDigit(mSrc[mPos]))) {
								Advance();
							}
						}
						tok.kind = Tk::Ident;
						tok.text = NkString(mSrc + begin, mPos - begin);
						tok.raw = tok.text;
						return true;
					}

					// TOUT LE RESTE EST DE LA PONCTUATION, et c'est delibere. Cette
					// couche ne modelise ni les expressions ni les operateurs : ce qui
					// n'entre pas dans `cle = valeur` ou `Type "id" { }` est conserve en
					// TRANCHE DE SOURCE. Le lexeur n'a donc besoin de distinguer que ce
					// que l'analyseur regarde : `=`, `{`, `}`, et les ouvrants/fermants
					// qui servent a compter la profondeur.
					const char *kTwo[] = {"->", ">=", "<=", "==", "&&", "||"};
					for (nk_uint32 i = 0; i < 6; ++i) {
						if (mPos + 1 < mLen && mSrc[mPos] == kTwo[i][0]
							&& mSrc[mPos + 1] == kTwo[i][1]) {
							Advance();
							Advance();
							tok.kind = Tk::Punct;
							tok.p0 = kTwo[i][0];
							tok.text = NkString(mSrc + begin, 2);
							tok.raw = tok.text;
							return true;
						}
					}

					// Un octet qui n'ouvre rien de connu doit REFUSER le fichier. C'est
					// ce qui tient la garantie des cles reservees : `$foo = 1` ne se lit
					// pas, donc `$type` ne peut pas entrer en collision avec un vrai nom.
					if (!IsPunctStart(c)) {
						err.code = NkString("E-PARSE");
						err.message = NkString("caractere inattendu '");
						err.message.Append(c);
						err.message.Append('\'');
						err.line = mLine;
						err.column = mCol;
						return false;
					}
					Advance();
					tok.kind = Tk::Punct;
					tok.p0 = c;
					tok.text = NkString(mSrc + begin, 1);
					tok.raw = tok.text;
					return true;
				}

				static bool IsPunctStart(char c) {
					switch (c) {
						case '{':
						case '}':
						case '(':
						case ')':
						case '[':
						case ']':
						case ',':
						case ':':
						case '=':
						case '|':
						case '&':
						case '+':
						case '-':
						case '*':
						case '/':
						case '>':
						case '<':
						case '#':
						case '!':
						case '.':
						case '%':
						case '@':
							return true;
						default:
							return false;
					}
				}

				bool LexNumber(Tok &tok) {
					const nk_uint32 begin = mPos;
					while (mPos < mLen && IsDigit(mSrc[mPos])) {
						Advance();
					}
					if (mPos + 1 < mLen && mSrc[mPos] == '.' && IsDigit(mSrc[mPos + 1])) {
						Advance();
						while (mPos < mLen && IsDigit(mSrc[mPos])) {
							Advance();
						}
					}
					tok.kind = Tk::Num;
					tok.text = NkString(mSrc + begin, mPos - begin);
					tok.raw = tok.text;
					return true;
				}

				bool LexString(Tok &tok, NkGuiDiag &err) {
					const nk_uint32 begin = mPos;
					const nk_uint32 openLine = mLine;
					const nk_uint32 openCol = mCol;
					Advance();
					NkString decoded;
					while (mPos < mLen && mSrc[mPos] != '"') {
						const char c = mSrc[mPos];
						if (c == '\\') {
							if (mPos + 1 >= mLen) {
								break;
							}
							const char e = mSrc[mPos + 1];
							// TROIS ECHAPPEMENTS, PAS QUATRE. En accepter d'autres en les
							// recopiant tels quels perdrait l'aller-retour : le
							// re-encodage doublerait la contre-oblique. Un echappement
							// inconnu est une ERREUR NOMMEE, pas une tolerance.
							if (e == '"') {
								decoded.Append('"');
							} else if (e == '\\') {
								decoded.Append('\\');
							} else if (e == 'n') {
								decoded.Append('\n');
							} else {
								err.code = NkString("E-PARSE");
								err.message = NkString("echappement inconnu dans une chaine : \\");
								err.message.Append(e);
								err.line = mLine;
								err.column = mCol;
								return false;
							}
							Advance();
							Advance();
							continue;
						}
						decoded.Append(c);
						Advance();
					}
					if (mPos >= mLen || mSrc[mPos] != '"') {
						err.code = NkString("E-PARSE");
						err.message = NkString("chaine jamais fermee");
						err.line = openLine;
						err.column = openCol;
						return false;
					}
					Advance();
					tok.kind = Tk::Str;
					tok.text = decoded;
					tok.raw = NkString(mSrc + begin, mPos - begin);
					tok.line = openLine;
					tok.column = openCol;
					return true;
				}
		};

		// =====================================================================
		//  LA PASSE DE TRIVIA
		// =====================================================================
		// Elle decoupe chaque INTERVALLE entre deux jetons en deux morceaux :
		//   - ce qui reste sur la ligne du jeton PRECEDENT  -> son `trail` ;
		//   - les lignes COMPLETES qui suivent               -> le `lead` du suivant.
		// Le reste de ligne juste avant le jeton (son indentation) est jete :
		// l'ecrivain la regenere -- SAUF s'il y porte autre chose que de l'espace,
		// auquel cas il est garde et l'indentation ne sera pas regeneree.

		void AttachTrivia(const char *src, NkVector<Tok> &toks, nk_uint32 first) {
			for (nk_size i = 0; i < toks.Size(); ++i) {
				const nk_uint32 gapBegin = (i == 0) ? first : toks[i - 1].end;
				const nk_uint32 gapEnd = toks[i].begin;
				if (gapEnd <= gapBegin) {
					continue;
				}
				const nk_uint32 n = gapEnd - gapBegin;
				const char *g = src + gapBegin;

				// 1. Le reste de la ligne du jeton precedent.
				//
				// ⚠️ SAUF POUR LE PREMIER JETON, QUI N'EN A PAS. Le decoupage
				//    « fin de ligne du precedent + lignes completes » suppose un
				//    jeton precedent ; devant le tout premier, la ligne d'ouverture
				//    est deja une ligne COMPLETE. Sans ce cas, un commentaire pose
				//    tout en haut du fichier, avant `nkgui`, disparaissait -- mesure
				//    du 2026-08-22, controle L3.
				nk_uint32 firstNL = n;
				for (nk_uint32 k = 0; k < n; ++k) {
					if (g[k] == '\n') {
						firstNL = k;
						break;
					}
				}
				const bool premier = (i == 0);
				if (!premier) {
					if (firstNL == n) {
						// AUCUN SAUT DE LIGNE : les deux jetons sont sur la MEME ligne.
						// L'intervalle est alors le SEPARATEUR du jeton suivant, et il
						// se garde tel quel.
						//
						// ⚠️ Il etait jete jusqu'au 2026-08-22, et le corpus des
						//    documents l'a montre : `key 1.0 -> 0.96, curve = EaseOut`
						//    (document 9 §4.2) met DEUX membres sur une ligne, dans un
						//    bloc qui, lui, tient sur plusieurs. La virgule n'est donc
						//    pas une propriete du BLOC -- c'est une propriete du
						//    SEPARATEUR entre deux membres, et elle vit la.
						toks[i].lead = NkString(g, n);
						continue;
					}
					nk_uint32 te = firstNL;
					if (te > 0 && g[te - 1] == '\r') {
						--te;
					}
					toks[i - 1].trail = NkString(g, te);
				}

				// 2. Les lignes completes, verbatim, terminateurs compris.
				nk_uint32 lastNL = n;
				bool haveNL = false;
				for (nk_uint32 k = 0; k < n; ++k) {
					if (g[k] == '\n') {
						lastNL = k;
						haveNL = true;
					}
				}
				const nk_uint32 leadBegin = premier ? 0u : (firstNL + 1);
				nk_uint32 leadEnd = leadBegin;
				if (haveNL && lastNL + 1 > leadBegin) {
					leadEnd = lastNL + 1;
				}

				// 3. Le reste de ligne devant le jeton. Espace pur -> jete (l'ecrivain
				//    l'indente). Sinon (commentaire de bloc referme sur la ligne) ->
				//    garde, et l'ecrivain n'indentera pas.
				bool keepPartial = false;
				for (nk_uint32 k = leadEnd; k < n; ++k) {
					if (!IsSpace(g[k])) {
						keepPartial = true;
						break;
					}
				}
				const nk_uint32 stop = keepPartial ? n : leadEnd;
				if (stop > leadBegin) {
					toks[i].lead = NkString(g + leadBegin, stop - leadBegin);
				}
			}
		}

		// =====================================================================
		//  OUTILS PARTAGES
		// =====================================================================

		inline bool IsPunct(const Tok &t, char c) {
			return t.kind == Tk::Punct && t.text.Size() == 1 && t.p0 == c;
		}

		/// Vrai si le jeton `i` commence un NOUVEAU membre : `Ident =`.
		inline bool StartsMember(const NkVector<Tok> &toks, nk_size i) {
			return i + 1 < toks.Size() && toks[i].kind == Tk::Ident && IsPunct(toks[i + 1], '=');
		}

		/// La FIN d'une construction qui commence au jeton `start` : elle court
		/// jusqu'a la fin de sa ligne, sauf si une parenthese, un crochet ou une
		/// accolade s'ouvre -- auquel cas elle court jusqu'a l'equilibre.
		///
		/// C'est la regle unique dont sortent la valeur d'une propriete ET la
		/// tranche brute d'une construction inconnue. Une seule regle, deux usages :
		/// il n'y a pas deux facons de decider ou une chose s'arrete.
		///
		/// ⚠️ DEUX ARRETS QUE « LA FIN DE LA LIGNE » NE DONNE PAS, et il a fallu un
		///    corpus INDEPENDANT pour les voir. Un bloc ecrit sur UNE ligne --
		///    `shadow { offset = (0, 2), blur = 6 }`, document 9 §4.3 -- met
		///    plusieurs membres sur la meme ligne. Sans ces deux arrets, la valeur
		///    d'`offset` avalait `, blur = 6, color = ...` : le fichier ressortait
		///    juste (une tranche verbatim ressort toujours juste) et l'archive
		///    etait FAUSSE. C'est le pire des cas -- vert et faux.
		nk_size SpanEnd(const NkVector<Tok> &toks, nk_size start, bool stopAtMember) {
			nk_int32 depth = 0;
			nk_size k = start;
			while (k < toks.Size() && toks[k].kind != Tk::End) {
				const Tok &t = toks[k];
				if (t.kind == Tk::Punct && t.text.Size() == 1) {
					if (t.p0 == '{' || t.p0 == '[' || t.p0 == '(') {
						++depth;
					} else if (t.p0 == '}' || t.p0 == ']' || t.p0 == ')') {
						--depth;
					}
				}
				if (depth > 0) {
					++k;
					continue;
				}
				const nk_size nx = k + 1;
				if (nx >= toks.Size() || toks[nx].kind == Tk::End) {
					break;
				}
				// A profondeur nulle, une accolade fermante appartient au bloc qui
				// nous contient : elle n'est jamais avalee.
				if (IsPunct(toks[nx], '}')) {
					break;
				}
				// A profondeur nulle, une virgule SEPARE des membres ; elle n'est
				// jamais dans une valeur (dans `(0, 2)` ou `[a, b]` elle est a une
				// profondeur superieure).
				if (IsPunct(toks[nx], ',')) {
					break;
				}
				// Et un `Ident =` a profondeur nulle ouvre le membre SUIVANT --
				// mais SEULEMENT quand on delimite une VALEUR.
				//
				// ⚠️ CE `stopAtMember` N'ETAIT PAS LA, et la mesure l'a impose tout
				//    de suite : une TRANCHE BRUTE contient tres bien un `=` a
				//    profondeur nulle. `set n1.value = a + 2` (document 2 §5) etait
				//    coupe apres `set`, et la suite repartait en tranche separee.
				//    Delimiter une valeur et delimiter une instruction ne sont pas la
				//    meme question ; une seule regle pour les deux etait une regle de
				//    trop.
				if (stopAtMember && StartsMember(toks, nx)) {
					break;
				}
				if (toks[nx].line != toks[k].endLine) {
					break;
				}
				++k;
			}
			return k;
		}

		/// Vrai si le jeton `i` ouvre un bloc : `Ident {` ou `Ident "id" {`.
		bool LooksLikeBlock(const NkVector<Tok> &toks, nk_size i, bool &hasId) {
			hasId = false;
			if (i >= toks.Size() || toks[i].kind != Tk::Ident) {
				return false;
			}
			nk_size j = i + 1;
			if (j < toks.Size() && toks[j].kind == Tk::Str) {
				hasId = true;
				++j;
			}
			return j < toks.Size() && IsPunct(toks[j], '{');
		}

		void AppendView(NkString &dst, NkStringView v) {
			if (v.Data() && v.Size() > 0) {
				dst.Append(v.Data(), (NkString::SizeType)v.Size());
			}
		}

		// =====================================================================
		//  L'ANALYSEUR
		// =====================================================================

		class Parser {
			public:
				Parser(const char *src, const NkVector<Tok> &toks) : mSrc(src), mToks(&toks) {}

				bool Members(nk_size &i, NkArchive &ar, NkGuiDiag &err, bool *sawComma = nullptr) {
					NkVector<NkArchiveNode> body;
					nk_int32 rank = 2;	// 0 et 1 sont pris par $type et $id
					const NkVector<Tok> &T = *mToks;

					while (i < T.Size() && T[i].kind != Tk::End && !IsPunct(T[i], '}')) {
						// La virgule SEPARE des membres sur une meme ligne. Elle n'est
						// pas un membre : on la note et on passe.
						//
						// ⚠️ ET ON RETIENT LE SEPARATEUR ENTIER. La virgule est un JETON,
						//    donc l'intervalle qui precede le membre suivant ne contient
						//    que l'espace d'apres la virgule -- pas la virgule. Le
						//    separateur va de la fin du membre PRECEDENT au debut du
						//    suivant ; c'est la seule facon de le reemettre tel quel.
						if (IsPunct(T[i], ',')) {
							if (sawComma) {
								*sawComma = true;
							}
							if (mSepStart == 0 && i > 0) {
								mSepStart = T[i - 1].end;
							}
							++i;
							continue;
						}
						bool hasId = false;
						if (T[i].kind == Tk::Ident && i + 1 < T.Size() && IsPunct(T[i + 1], '=')
							&& !ar.Has(NkStringView(T[i].text))) {
							if (!Property(i, ar, rank, err)) {
								return false;
							}
						} else if (LooksLikeBlock(T, i, hasId)) {
							if (!Block(i, body, rank, err)) {
								return false;
							}
						} else {
							// TOUT LE RESTE : une tranche de source, gardee telle quelle.
							// C'est la regle (d) devenue le regime normal -- et une
							// propriete en DOUBLE passe aussi par ici, parce qu'une
							// archive n'a qu'une entree par cle : la seconde serait
							// PERDUE si on la posait, elle est donc conservee verbatim.
							Raw(i, body, rank);
						}
					}
					if (!body.Empty()) {
						ar.SetNodeArray(NkStringView(NkGuiArchive::KeyBody()), body);
					}
					return true;
				}

			private:
				const char *mSrc = nullptr;
				const NkVector<Tok> *mToks = nullptr;
				/// Offset de debut du separateur en attente (0 = aucun). Voir la note
				/// sur la virgule dans `Members`.
				nk_uint32 mSepStart = 0;

				/// La trivia de tete du membre qui commence au jeton `i` : le separateur
				/// retenu s'il y en a un, sinon l'intervalle du jeton. CONSOMMANT : un
				/// separateur ne sert qu'une fois.
				NkString TakeLead(nk_size i) {
					const NkVector<Tok> &T = *mToks;
					if (mSepStart != 0 && T[i].begin > mSepStart) {
						const NkString sep(mSrc + mSepStart, T[i].begin - mSepStart);
						mSepStart = 0;
						return sep;
					}
					mSepStart = 0;
					return T[i].lead;
				}

				NkString Slice(nk_size a, nk_size b) const {
					const NkVector<Tok> &T = *mToks;
					return NkString(mSrc + T[a].begin, T[b].end - T[a].begin);
				}

				bool Property(nk_size &i, NkArchive &ar, nk_int32 &rank, NkGuiDiag &err) {
					const NkVector<Tok> &T = *mToks;
					const NkString key = T[i].text;
					const nk_size v0 = i + 2;
					if (v0 >= T.Size() || T[v0].kind == Tk::End || IsPunct(T[v0], '}')) {
						err.code = NkString("E-PARSE");
						err.message = NkString("propriete sans valeur : ");
						err.message.Append(key);
						err.line = T[i].line;
						err.column = T[i].column;
						return false;
					}
					const nk_size v1 = SpanEnd(T, v0, true);
					const NkString raw = Slice(v0, v1);

					const NkString lead = TakeLead(i);
					SetTypedValue(ar, key, T, v0, v1, raw);
					ar.SetSourceOrder(NkStringView(key), rank++);
					// LA LIGNE DU FICHIER, portee jusqu'au diagnostic. Elle vient du
					// jeton qui ouvre le membre, pas de sa valeur : c'est le nom de la
					// propriete que l'utilisateur cherche des yeux.
					ar.SetSourceLine(NkStringView(key), (nk_int32)T[i].line);
					ar.SetLeadingTrivia(NkStringView(key), NkStringView(lead));
					ar.SetTrailingTrivia(NkStringView(key), NkStringView(T[v1].trail));
					i = v1 + 1;
					return true;
				}

				/// LE TYPAGE, ET LA SEULE CHOSE QU'IL PROMET. Une valeur n'est typee
				/// que si elle tient en UN jeton dont le sens ne se discute pas : une
				/// chaine, un nombre, `true`/`false`. Tout le reste -- couleur, vecteur,
				/// identifiant, drapeaux, liste, dictionnaire -- est un JETON NU :
				/// une chaine dont le litteral est la tranche de source, reemise telle
				/// quelle.
				///
				/// ⚠️ CE QUE CA NE FAIT PAS, et il vaut mieux l'ecrire que de le laisser
				///    croire : `items = [a, b]` ne devient PAS un tableau d'archive. Un
				///    outil qui veut iterer dessus doit encore l'analyser lui-meme. Le
				///    faire ici demanderait de poser un litteral sur un noeud TABLEAU --
				///    or `HasUsableLiteral()` rend false pour un non-scalaire, donc le
				///    litteral ne protegerait plus rien et une modification du tableau
				///    se reemettrait avec l'ancien texte. C'est la prochaine tranche,
				///    pas un oubli.
				static void SetTypedValue(NkArchive &ar, const NkString &key,
										  const NkVector<Tok> &T, nk_size v0, nk_size v1,
										  const NkString &raw) {
					const NkStringView k(key);
					if (v0 == v1) {
						const Tok &t = T[v0];
						if (t.kind == Tk::Str) {
							ar.SetString(k, NkStringView(t.text));
							ar.SetLiteral(k, NkStringView(raw));
							return;
						}
						if (t.kind == Tk::Num) {
							bool isInt = true;
							for (nk_size c = 0; c < t.text.Size(); ++c) {
								if (t.text.Data()[c] == '.') {
									isInt = false;
									break;
								}
							}
							if (isInt && t.text.Size() <= 18) {
								nk_int64 v = 0;
								for (nk_size c = 0; c < t.text.Size(); ++c) {
									v = v * 10 + (nk_int64)(t.text.Data()[c] - '0');
								}
								ar.SetInt64(k, v);
								ar.SetLiteral(k, NkStringView(raw));
								return;
							}
							if (!isInt) {
								ar.SetFloat64(k, ParseFloat(t.text));
								ar.SetLiteral(k, NkStringView(raw));
								return;
							}
						}
						if (t.kind == Tk::Ident
							&& (t.text.Compare("true") == 0 || t.text.Compare("false") == 0)) {
							ar.SetBool(k, t.text.Compare("true") == 0);
							ar.SetLiteral(k, NkStringView(raw));
							return;
						}
					}
					NkGuiArchive::SetToken(ar, k, NkStringView(raw));
				}

				static nk_float64 ParseFloat(const NkString &s) {
					const char *p = s.Data();
					const nk_size n = s.Size();
					nk_float64 whole = 0.0;
					nk_size i = 0;
					for (; i < n && p[i] != '.'; ++i) {
						whole = whole * 10.0 + (nk_float64)(p[i] - '0');
					}
					if (i < n && p[i] == '.') {
						++i;
						nk_float64 scale = 0.1;
						for (; i < n; ++i) {
							whole += (nk_float64)(p[i] - '0') * scale;
							scale *= 0.1;
						}
					}
					return whole;
				}

				bool Block(nk_size &i, NkVector<NkArchiveNode> &body, nk_int32 &rank,
						   NkGuiDiag &err) {
					const NkVector<Tok> &T = *mToks;
					const nk_size head = i;
					const NkString lead = TakeLead(head);
					nk_size j = i + 1;
					bool hasId = false;
					if (T[j].kind == Tk::Str) {
						hasId = true;
					}

					NkArchive child;
					NkGuiArchive::SetToken(child, NkStringView(NkGuiArchive::KeyType()),
										   NkStringView(T[head].text));
					child.SetSourceOrder(NkStringView(NkGuiArchive::KeyType()), 0);
					if (hasId) {
						child.SetString(NkStringView(NkGuiArchive::KeyId()),
										NkStringView(T[j].text));
						child.SetLiteral(NkStringView(NkGuiArchive::KeyId()),
										 NkStringView(T[j].raw));
						child.SetSourceOrder(NkStringView(NkGuiArchive::KeyId()), 1);
						++j;
					}
					const nk_size open = j;	 // le `{`
					nk_size k = open + 1;
					mSepStart = 0;	// il a ete consomme par l'en-tete du bloc
					bool commas = false;
					if (!Members(k, child, err, &commas)) {
						return false;
					}
					if (k >= T.Size() || !IsPunct(T[k], '}')) {
						err.code = NkString("E-PARSE");
						err.message = NkString("bloc jamais ferme : ");
						err.message.Append(T[head].text);
						err.line = T[head].line;
						err.column = T[head].column;
						return false;
					}

					// L'accolade OUVRANTE et la FERMANTE bordent l'objet : sa trivia
					// d'en-tete est ce qui suit `{` sur sa ligne, son pied les lignes
					// qui precedent `}`. Le noeud, lui, porte ce qui borde le BLOC
					// ENTIER : les lignes avant, et le reste de ligne apres `}`.
					const bool sameLine = (T[open].endLine == T[k].line);
					const bool vide = (k == open + 1);
					if (!T[open].trail.Empty()) {
						child.SetHeaderTrivia(NkStringView(T[open].trail));
					}
					// Le pied est ce que le fichier avait avant `}` : des lignes
					// completes, ou -- quand `}` est pose sur la ligne du dernier
					// membre -- le simple espace qui l'en separe. L'ecrivain distingue
					// les deux avec la MEME regle que pour ouvrir un membre.
					if (!sameLine && !T[k].lead.Empty()) {
						child.SetFooterTrivia(NkStringView(T[k].lead));
					}
					// LA DISPOSITION -- et seulement quand elle n'est pas celle par
					// defaut, pour qu'un document fabrique par le code n'ait a poser
					// aucune de ces cles.
					if (sameLine && !vide) {
						NkGuiArchive::SetToken(child, NkStringView(NkGuiArchive::KeyLayout()),
											   NkStringView(commas ? "inline," : "inline"));
					} else if (!sameLine && vide) {
						NkGuiArchive::SetToken(child, NkStringView(NkGuiArchive::KeyLayout()),
											   NkStringView("block"));
					}

					NkArchiveNode node;
					node.SetObject(child);
					node.SetSourceOrder(rank++);
					node.SetSourceLine((nk_int32)T[head].line);
					node.SetLeadingTrivia(NkStringView(lead));
					node.SetTrailingTrivia(NkStringView(T[k].trail));
					body.PushBack(node);
					i = k + 1;
					return true;
				}

				void Raw(nk_size &i, NkVector<NkArchiveNode> &body, nk_int32 &rank) {
					const NkVector<Tok> &T = *mToks;
					const NkString lead = TakeLead(i);
					const nk_size e = SpanEnd(T, i, false);
					// T11 : une tranche brute est un simple noeud CHAINE. La forme
					// canonique d'une chaine EST la chaine -- aucun litteral, aucune
					// trivia de valeur, aucun mecanisme.
					NkArchiveNode node(NkArchiveValue::FromString(NkStringView(Slice(i, e))));
					node.SetSourceOrder(rank++);
					node.SetSourceLine((nk_int32)T[i].line);
					node.SetLeadingTrivia(NkStringView(lead));
					node.SetTrailingTrivia(NkStringView(T[e].trail));
					body.PushBack(node);
					i = e + 1;
				}
		};

		// =====================================================================
		//  L'ECRIVAIN
		// =====================================================================

		class Writer {
			public:
				Writer(const NkGuiStyle &style) : mStyle(style) {}

				NkString Run(const NkArchive &doc) {
					mOut.Clear();
					mOut.Reserve(4096);
					if (mStyle.bom) {
						mOut.Append((char)(nk_uint8)0xEF);
						mOut.Append((char)(nk_uint8)0xBB);
						mOut.Append((char)(nk_uint8)0xBF);
					}
					AppendView(mOut, doc.HeaderTrivia());
					mOut.Append("nkgui ");
					const NkArchiveNode *ver =
						doc.FindNode(NkStringView(NkGuiArchive::KeyVersion()));
					if (ver && ver->IsScalar()) {
						AppendView(mOut, ver->Lexeme());
						AppendView(mOut, ver->TrailingTrivia());
					} else {
						mOut.Append("0.3");
					}
					EndLine();
					Members(doc, 0);
					CloseLine(doc.FooterTrivia(), false, 0);
					if (!mStyle.finalNewline) {
						StripFinalNewline();
					}
					return mOut;
				}

			private:
				NkGuiStyle mStyle;
				NkString mOut;
				bool mPending = false;	 ///< une ligne est finie mais pas encore fermee

				void NewLine() {
					if (mStyle.crlf) {
						mOut.Append('\r');
					}
					mOut.Append('\n');
				}

				/// LE SAUT DE LIGNE EST PARESSEUX, et c'est ce qui permet a un membre de
				/// POURSUIVRE la ligne du precedent. On note qu'une ligne est finie ; on
				/// ne la ferme que lorsqu'on sait que la suite en ouvre une nouvelle.
				void EndLine() {
					mPending = true;
				}

				/// Ferme la ligne en attente, s'il y en a une.
				void FlushLine() {
					if (mPending) {
						mPending = false;
						NewLine();
					}
				}

				static bool HasNewLine(NkStringView v) {
					for (nk_size i = 0; i < v.Size(); ++i) {
						if (v.Data()[i] == '\n') {
							return true;
						}
					}
					return false;
				}

				/// Ouvre le membre : ferme la ligne precedente s'il en faut une, reemet
				/// la trivia de tete verbatim, et indente si le membre commence bien une
				/// ligne.
				///
				/// Trois cas, et un seul endroit qui les distingue :
				///  - trivia VIDE                  -> nouvelle ligne + indentation ;
				///  - trivia SANS saut de ligne    -> le membre POURSUIT la ligne du
				///    precedent : la trivia EST le separateur (`, `), ni saut ni
				///    indentation ;
				///  - trivia AVEC saut de ligne    -> nouvelle ligne, trivia verbatim,
				///    puis indentation seulement si elle finit par un saut (sinon sa
				///    derniere ligne est partielle et porte deja son indentation).
				/// LA MEME REGLE, POUR FERMER. `foot` est ce que le fichier avait avant
				/// l'accolade fermante (ou avant la fin du fichier) :
				///  - sans saut de ligne et non vide -> la fermeture POURSUIT la ligne
				///    du dernier membre (`b = 2 }`) ;
				///  - sinon -> on ferme la ligne, on reemet `foot` verbatim, et on
				///    indente si on nous le demande.
				void CloseLine(NkStringView foot, bool indent, nk_uint32 depth) {
					const bool suite = (foot.Size() > 0) && !HasNewLine(foot);
					if (!suite) {
						FlushLine();
					}
					AppendView(mOut, foot);
					if (!suite && indent) {
						Indent(depth);
					}
				}

				void OpenMember(NkStringView lead, nk_uint32 depth) {
					const bool suite = (lead.Size() > 0) && !HasNewLine(lead);
					if (!suite) {
						FlushLine();
					}
					AppendView(mOut, lead);
					if (suite) {
						return;
					}
					if (lead.Size() == 0 || lead.Data()[lead.Size() - 1] == '\n') {
						Indent(depth);
					}
				}

				void StripFinalNewline() {
					nk_size n = mOut.Size();
					if (n > 0 && mOut.Data()[n - 1] == '\n') {
						--n;
						if (n > 0 && mOut.Data()[n - 1] == '\r') {
							--n;
						}
						mOut = NkString(mOut.Data(), n);
					}
				}

				void Indent(nk_uint32 depth) {
					const nk_uint32 n = depth * mStyle.indent;
					for (nk_uint32 i = 0; i < n; ++i) {
						mOut.Append(' ');
					}
				}

				void Quoted(const NkString &s) {
					mOut.Append('"');
					const char *p = s.Data();
					const nk_size n = s.Size();
					for (nk_size i = 0; i < n; ++i) {
						const char c = p[i];
						if (c == '"') {
							mOut.Append("\\\"");
						} else if (c == '\\') {
							mOut.Append("\\\\");
						} else if (c == '\n') {
							mOut.Append("\\n");
						} else {
							mOut.Append(c);
						}
					}
					mOut.Append('"');
				}

				/// LA REGLE D'IMPRESSION D'UNE VALEUR, et elle tient en une phrase :
				/// un litteral encore valable s'imprime tel quel, sinon une chaine se
				/// remet entre guillemets et tout le reste s'imprime canoniquement.
				///
				/// Le seul cas ou elle se trompe est celui d'un JETON NU dont on a
				/// change la valeur sans repasser par `NkGuiArchive::SetToken` : il
				/// repart entre guillemets. C'est VISIBLE (une couleur devient une
				/// chaine, la validation par role le dit) -- au contraire d'une sortie
				/// qui aurait l'air correcte.
				void Value(const NkArchiveNode &node) {
					if (node.IsArray()) {
						mOut.Append('[');
						for (nk_size i = 0; i < node.array.Size(); ++i) {
							if (i > 0) {
								mOut.Append(", ");
							}
							Value(node.array[i]);
						}
						mOut.Append(']');
						return;
					}
					if (node.IsObject()) {
						if (!node.object || node.object->Empty()) {
							mOut.Append("{ }");
							return;
						}
						mOut.Append("{ ");
						const NkVector<NkArchiveEntry> &e = node.object->Entries();
						for (nk_size i = 0; i < e.Size(); ++i) {
							if (i > 0) {
								mOut.Append(", ");
							}
							mOut.Append(e[i].key);
							mOut.Append(" = ");
							Value(e[i].node);
						}
						mOut.Append(" }");
						return;
					}
					if (node.value.type == NkArchiveValueType::NK_VALUE_STRING
						&& !node.HasUsableLiteral()) {
						Quoted(node.value.text);
						return;
					}
					AppendView(mOut, node.Lexeme());
				}

				static nk_int32 RankOf(const NkArchiveNode &n) {
					const nk_int32 r = n.SourceOrder();
					// Sans rang = « ajoute apres la lecture » : ca s'ecrit A LA FIN, pas
					// a un endroit arbitraire.
					return (r < 0) ? 0x7FFFFFFF : r;
				}

				/// Une place dans l'ordre du fichier : soit une entree de l'archive
				/// (une propriete), soit un element du `$body` (un bloc ou une tranche).
				struct Ref {
						bool entry = true;
						nk_size idx = 0;
				};

				/// L'ECRIVAIN FUSIONNE DEUX SUITES DEJA TRIEES : les proprietes (les
				/// entrees) et les membres non-scalaires (le `$body`). C'est la boucle
				/// de T9 en vrai -- et c'est elle qui rend l'ENTRELACEMENT possible :
				/// `a = 1 / un enfant / b = 2` se reecrit dans cet ordre parce que les
				/// deux suites sont fusionnees par rang, pas concatenees.
				///
				/// L'ordre est calcule UNE fois et rendu tel quel : la disposition
				/// (plusieurs lignes ou une seule) ne doit pas pouvoir changer l'ordre
				/// des membres, donc les deux ecrivains lisent la meme liste.
				static void MergeOrder(const NkArchive &ar, NkVector<Ref> &out) {
					const NkArchiveNode *body =
						ar.FindNode(NkStringView(NkGuiArchive::KeyBody()));
					const nk_size bn = (body && body->IsArray()) ? body->array.Size() : 0;
					const NkVector<NkArchiveEntry> &ents = ar.Entries();
					nk_size ei = 0;
					nk_size bi = 0;
					while (true) {
						while (ei < ents.Size()
							   && NkGuiArchive::IsReservedKey(NkStringView(ents[ei].key))) {
							++ei;
						}
						const bool haveE = ei < ents.Size();
						const bool haveB = bi < bn;
						if (!haveE && !haveB) {
							return;
						}
						bool takeEntry;
						if (!haveB) {
							takeEntry = true;
						} else if (!haveE) {
							takeEntry = false;
						} else {
							takeEntry = RankOf(ents[ei].node) <= RankOf(body->array[bi]);
						}
						Ref r;
						r.entry = takeEntry;
						r.idx = takeEntry ? ei : bi;
						out.PushBack(r);
						if (takeEntry) {
							++ei;
						} else {
							++bi;
						}
					}
				}

				void Members(const NkArchive &ar, nk_uint32 depth) {
					NkVector<Ref> ordre;
					MergeOrder(ar, ordre);
					const NkArchiveNode *body =
						ar.FindNode(NkStringView(NkGuiArchive::KeyBody()));
					for (nk_size i = 0; i < ordre.Size(); ++i) {
						if (ordre[i].entry) {
							const NkArchiveEntry &e = ar.Entries()[ordre[i].idx];
							OpenMember(e.node.LeadingTrivia(), depth);
							mOut.Append(e.key);
							mOut.Append(" = ");
							Value(e.node);
							AppendView(mOut, e.node.TrailingTrivia());
							EndLine();
							continue;
						}
						const NkArchiveNode &n = body->array[ordre[i].idx];
						if (n.IsObject()) {
							Block(n, depth);
						} else {
							// Une tranche brute : elle NE PASSE PAS par le modele.
							// Elle sort telle qu'elle est entree (T11).
							OpenMember(n.LeadingTrivia(), depth);
							AppendView(mOut, n.Lexeme());
							AppendView(mOut, n.TrailingTrivia());
							EndLine();
						}
					}
				}

				/// Vrai si le bloc n'a AUCUN membre (les cles reservees ne comptent pas :
				/// elles portent la syntaxe, pas le contenu).
				static bool IsEmptyBlock(const NkArchive &block) {
					const NkArchiveNode *body =
						block.FindNode(NkStringView(NkGuiArchive::KeyBody()));
					if (body && body->IsArray() && !body->array.Empty()) {
						return false;
					}
					const NkVector<NkArchiveEntry> &e = block.Entries();
					for (nk_size i = 0; i < e.Size(); ++i) {
						if (!NkGuiArchive::IsReservedKey(NkStringView(e[i].key))) {
							return false;
						}
					}
					return true;
				}

				/// LA DISPOSITION, LUE A UN SEUL ENDROIT. `$layout` absente = la forme
				/// par defaut : `{ }` si le bloc est vide, plusieurs lignes sinon.
				enum class Layout : nk_uint8 { Defaut = 0, Bloc, Ligne, LigneVirgules };

				static Layout LayoutOf(const NkArchive &block) {
					const NkArchiveNode *n =
						block.FindNode(NkStringView(NkGuiArchive::KeyLayout()));
					if (!n || !n->IsScalar()) {
						return Layout::Defaut;
					}
					const NkString &t = n->value.text;
					if (t.Compare("block") == 0) {
						return Layout::Bloc;
					}
					if (t.Compare("inline,") == 0) {
						return Layout::LigneVirgules;
					}
					if (t.Compare("inline") == 0) {
						return Layout::Ligne;
					}
					return Layout::Defaut;
				}

				void Block(const NkArchiveNode &node, nk_uint32 depth) {
					OpenMember(node.LeadingTrivia(), depth);
					BlockHead(node);
					const NkArchive &blk = *node.object;
					const Layout lay = LayoutOf(blk);
					const bool vide = IsEmptyBlock(blk);

					if (lay == Layout::Ligne || lay == Layout::LigneVirgules
						|| (lay == Layout::Defaut && vide && mStyle.inlineEmptyBlock)) {
						BlockOnOneLine(blk, lay == Layout::LigneVirgules);
						AppendView(mOut, node.TrailingTrivia());
						EndLine();
						return;
					}
					mOut.Append(" {");
					AppendView(mOut, blk.HeaderTrivia());
					EndLine();
					Members(blk, depth + 1);
					CloseLine(blk.FooterTrivia(), true, depth);
					mOut.Append('}');
					AppendView(mOut, node.TrailingTrivia());
					EndLine();
				}

				/// `Type "id"` -- l'en-tete d'un bloc, sans son corps. `$type` s'imprime
				/// NU sans condition : ce n'est pas une valeur, c'est le mot qui ouvre
				/// le bloc.
				void BlockHead(const NkArchiveNode &node) {
					const NkArchive &blk = *node.object;
					AppendView(mOut, NkGuiArchive::TypeOf(blk));
					const NkArchiveNode *id = blk.FindNode(NkStringView(NkGuiArchive::KeyId()));
					if (id && id->IsScalar()) {
						mOut.Append(' ');
						Value(*id);
					}
				}

				/// Le corps d'un bloc SUR UNE LIGNE : `{ }`, `{ color = #2F6F7A }`,
				/// `{ offset = (0, 2), blur = 6 }`. Aucune trivia n'y est reemise --
				/// il ne peut pas y en avoir, il n'y a pas de saut de ligne.
				void BlockOnOneLine(const NkArchive &blk, bool virgules) {
					mOut.Append(" {");
					NkVector<Ref> ordre;
					MergeOrder(blk, ordre);
					const NkArchiveNode *body =
						blk.FindNode(NkStringView(NkGuiArchive::KeyBody()));
					for (nk_size i = 0; i < ordre.Size(); ++i) {
						mOut.Append((i > 0 && virgules) ? ", " : " ");
						if (ordre[i].entry) {
							const NkArchiveEntry &e = blk.Entries()[ordre[i].idx];
							mOut.Append(e.key);
							mOut.Append(" = ");
							Value(e.node);
						} else {
							const NkArchiveNode &n = body->array[ordre[i].idx];
							if (n.IsObject()) {
								BlockHead(n);
								BlockOnOneLine(*n.object, LayoutOf(*n.object)
															  == Layout::LigneVirgules);
							} else {
								AppendView(mOut, n.Lexeme());
							}
						}
					}
					mOut.Append(" }");
				}
		};

	} // namespace

	// =========================================================================
	//  NkGuiArchive
	// =========================================================================

	// =========================================================================
	//  COMPARER DEUX DOCUMENTS
	// =========================================================================

	namespace {

		bool EqNode(const NkArchiveNode &a, const NkArchiveNode &b, bool tv);

		bool EqView(NkStringView a, NkStringView b) {
			if (a.Size() != b.Size()) {
				return false;
			}
			for (nk_size i = 0; i < a.Size(); ++i) {
				if (a.Data()[i] != b.Data()[i]) {
					return false;
				}
			}
			return true;
		}

		bool EqArchive(const NkArchive &a, const NkArchive &b, bool tv) {
			if (a.Entries().Size() != b.Entries().Size()) {
				return false;
			}
			if (tv && (!EqView(a.HeaderTrivia(), b.HeaderTrivia())
					   || !EqView(a.FooterTrivia(), b.FooterTrivia()))) {
				return false;
			}
			for (nk_size i = 0; i < a.Entries().Size(); ++i) {
				// L'ORDRE DES ENTREES FAIT PARTIE DE L'EGALITE. Deux documents qui
				// n'ont pas les memes proprietes dans le meme ordre ne sont pas le
				// meme document : le banc `.nkgui` traite deja l'ordre inverse comme
				// une difference (controle 2c).
				if (a.Entries()[i].key.Compare(b.Entries()[i].key) != 0) {
					return false;
				}
				if (!EqNode(a.Entries()[i].node, b.Entries()[i].node, tv)) {
					return false;
				}
			}
			return true;
		}

		bool EqNode(const NkArchiveNode &a, const NkArchiveNode &b, bool tv) {
			if (a.kind != b.kind) {
				return false;
			}
			if (a.SourceOrder() != b.SourceOrder()) {
				return false;
			}
			if (tv && (!EqView(a.LeadingTrivia(), b.LeadingTrivia())
					   || !EqView(a.TrailingTrivia(), b.TrailingTrivia()))) {
				return false;
			}
			if (a.IsArray()) {
				if (a.array.Size() != b.array.Size()) {
					return false;
				}
				for (nk_size i = 0; i < a.array.Size(); ++i) {
					if (!EqNode(a.array[i], b.array[i], tv)) {
						return false;
					}
				}
				return true;
			}
			if (a.IsObject()) {
				if (!a.object || !b.object) {
					return a.object == b.object;
				}
				return EqArchive(*a.object, *b.object, tv);
			}
			// ⚠️ ON COMPARE LE LEXEME, PAS LA VALEUR. `0.20` et `0.2` denotent le
			//    meme nombre et NE SONT PAS le meme document -- reecrire l'un a la
			//    place de l'autre modifie une ligne que l'auteur n'a pas touchee.
			//    Le controle 2d du banc repose exactement la-dessus.
			if (a.value.type != b.value.type) {
				return false;
			}
			return EqView(a.Lexeme(), b.Lexeme());
		}

	} // namespace

	bool NkGuiArchive::Equal(const NkArchive &a, const NkArchive &b, bool withTrivia) noexcept {
		return EqArchive(a, b, withTrivia);
	}

	// =========================================================================
	//  LE CLASSIFICATEUR DE VALEUR
	// =========================================================================
	//
	// Un scanner de VALEURS, separe du lexeur de FICHIERS -- et c'est voulu. Les
	// deux repondent a des questions differentes : le lexeur decoupe un fichier en
	// jetons SANS JUGER, celui-ci demande « ce texte est-il une valeur bien formee,
	// et laquelle ». Les melanger obligerait le lexeur a connaitre la forme d'une
	// couleur, c'est-a-dire a juger -- exactement ce qu'on lui interdit.

	namespace {

		inline bool VHex(char c) {
			return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
		}
		inline bool VDigit(char c) {
			return c >= '0' && c <= '9';
		}
		inline bool VAlpha(char c) {
			return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
		}
		inline void VSkip(const char *s, nk_size n, nk_size &i) {
			while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) {
				++i;
			}
		}

		const nk_size kVBad = (nk_size)-1;

		nk_size VScanValue(const char *s, nk_size n, nk_size i, NkGuiValueKind *kind);

		/// Une chaine, avec les TROIS echappements du format et pas un de plus.
		nk_size VScanString(const char *s, nk_size n, nk_size i) {
			++i;  // le guillemet ouvrant
			while (i < n && s[i] != '"') {
				if (s[i] != '\\') {
					++i;
					continue;
				}
				if (i + 1 >= n) {
					return kVBad;
				}
				const char e = s[i + 1];
				if (e != '"' && e != '\\' && e != 'n') {
					return kVBad;
				}
				i += 2;
			}
			return (i < n && s[i] == '"') ? (i + 1) : kVBad;
		}

		nk_size VScanNumber(const char *s, nk_size n, nk_size i) {
			if (i < n && s[i] == '-') {
				++i;
			}
			if (i >= n || !VDigit(s[i])) {
				return kVBad;
			}
			while (i < n && VDigit(s[i])) {
				++i;
			}
			if (i + 1 < n && s[i] == '.' && VDigit(s[i + 1])) {
				++i;
				while (i < n && VDigit(s[i])) {
					++i;
				}
			}
			return i;
		}

		/// UNE COULEUR A SIX OU HUIT CHIFFRES, ET RIEN D'AUTRE. `#12345` etait
		/// refuse par l'ancien lecteur ; il est redevenu une faute ici.
		nk_size VScanColor(const char *s, nk_size n, nk_size i) {
			const nk_size begin = ++i;
			while (i < n && VHex(s[i])) {
				++i;
			}
			const nk_size len = i - begin;
			return (len == 6 || len == 8) ? i : kVBad;
		}

		/// Un identifiant, eventuellement POINTE (`n1.value`, `Enum.X`, `a.b.c`).
		nk_size VScanIdent(const char *s, nk_size n, nk_size i) {
			if (i >= n || !VAlpha(s[i])) {
				return kVBad;
			}
			while (i < n && (VAlpha(s[i]) || VDigit(s[i]))) {
				++i;
			}
			while (i + 1 < n && s[i] == '.' && VAlpha(s[i + 1])) {
				++i;
				while (i < n && (VAlpha(s[i]) || VDigit(s[i]))) {
					++i;
				}
			}
			return i;
		}

		nk_size VScanVec2(const char *s, nk_size n, nk_size i) {
			++i;  // (
			VSkip(s, n, i);
			i = VScanNumber(s, n, i);
			if (i == kVBad) {
				return kVBad;
			}
			VSkip(s, n, i);
			if (i >= n || s[i] != ',') {
				return kVBad;
			}
			++i;
			VSkip(s, n, i);
			i = VScanNumber(s, n, i);
			if (i == kVBad) {
				return kVBad;
			}
			VSkip(s, n, i);
			return (i < n && s[i] == ')') ? (i + 1) : kVBad;
		}

		/// PAS DE VIRGULE FINALE. `["a",]` etait refuse par l'ancien lecteur, et il
		/// le reste ici : une virgule finale se lit comme un element vide, et
		/// « element vide » n'existe pas dans le format.
		nk_size VScanList(const char *s, nk_size n, nk_size i) {
			++i;  // [
			VSkip(s, n, i);
			if (i < n && s[i] == ']') {
				return i + 1;
			}
			while (true) {
				i = VScanValue(s, n, i, nullptr);
				if (i == kVBad) {
					return kVBad;
				}
				VSkip(s, n, i);
				if (i < n && s[i] == ',') {
					++i;
					VSkip(s, n, i);
					continue;
				}
				return (i < n && s[i] == ']') ? (i + 1) : kVBad;
			}
		}

		/// UNE CLE EST UN IDENTIFIANT OU UNE CHAINE. `{ 1 = 2 }` etait refuse par
		/// l'ancien lecteur, et il le reste.
		nk_size VScanDict(const char *s, nk_size n, nk_size i) {
			++i;  // {
			VSkip(s, n, i);
			if (i < n && s[i] == '}') {
				return i + 1;
			}
			while (true) {
				nk_size k = (i < n && s[i] == '"') ? VScanString(s, n, i) : VScanIdent(s, n, i);
				if (k == kVBad) {
					return kVBad;
				}
				i = k;
				VSkip(s, n, i);
				if (i >= n || s[i] != '=') {
					return kVBad;
				}
				++i;
				VSkip(s, n, i);
				i = VScanValue(s, n, i, nullptr);
				if (i == kVBad) {
					return kVBad;
				}
				VSkip(s, n, i);
				if (i < n && s[i] == ',') {
					++i;
					VSkip(s, n, i);
					continue;
				}
				return (i < n && s[i] == '}') ? (i + 1) : kVBad;
			}
		}

		nk_size VScanValue(const char *s, nk_size n, nk_size i, NkGuiValueKind *kind) {
			VSkip(s, n, i);
			if (i >= n) {
				return kVBad;
			}
			const char c = s[i];
			NkGuiValueKind k = NkGuiValueKind::Invalid;
			nk_size e = kVBad;
			if (c == '"') {
				k = NkGuiValueKind::String;
				e = VScanString(s, n, i);
			} else if (c == '#') {
				k = NkGuiValueKind::Color;
				e = VScanColor(s, n, i);
			} else if (c == '(') {
				k = NkGuiValueKind::Vec2;
				e = VScanVec2(s, n, i);
			} else if (c == '[') {
				k = NkGuiValueKind::List;
				e = VScanList(s, n, i);
			} else if (c == '{') {
				k = NkGuiValueKind::Dict;
				e = VScanDict(s, n, i);
			} else if (c == '-' || VDigit(c)) {
				k = NkGuiValueKind::Number;
				e = VScanNumber(s, n, i);
			} else if (VAlpha(c)) {
				k = NkGuiValueKind::Ident;
				e = VScanIdent(s, n, i);
				// Les DRAPEAUX : `A | B | C`. Un seul identifiant reste un
				// identifiant -- c'est la barre qui fait le drapeau.
				while (e != kVBad) {
					nk_size j = e;
					VSkip(s, n, j);
					if (j >= n || s[j] != '|') {
						break;
					}
					++j;
					VSkip(s, n, j);
					const nk_size nx = VScanIdent(s, n, j);
					if (nx == kVBad) {
						return kVBad;
					}
					k = NkGuiValueKind::Flags;
					e = nx;
				}
			}
			if (e == kVBad) {
				return kVBad;
			}
			if (kind) {
				*kind = k;
			}
			return e;
		}

	} // namespace

	NkGuiValueKind NkGuiArchive::KindOf(const NkArchiveNode &node) noexcept {
		// Un noeud TABLEAU ou OBJET ne vient jamais d'un fichier -- il vient du
		// code. Sa forme syntaxique est celle qu'il prendra a l'ecriture.
		if (node.IsArray()) {
			return NkGuiValueKind::List;
		}
		if (node.IsObject()) {
			return NkGuiValueKind::Dict;
		}
		if (node.value.type == NkArchiveValueType::NK_VALUE_NULL) {
			return NkGuiValueKind::Null;
		}
		if (node.value.type == NkArchiveValueType::NK_VALUE_BOOL) {
			// LE BOOLEEN N'EST PAS UN TYPE DU LEXIQUE : c'est un identifiant qui
			// vaut `true` ou `false`. On rend donc Ident, et c'est a l'appelant de
			// regarder le TEXTE -- sinon `wrap = Vrai` passerait pour un booleen.
			return NkGuiValueKind::Ident;
		}
		if (node.value.type != NkArchiveValueType::NK_VALUE_STRING) {
			return NkGuiValueKind::Number;
		}
		// Une chaine SANS litteral utilisable s'ecrira entre guillemets : c'est
		// donc une chaine, quel que soit son contenu.
		if (!node.HasUsableLiteral()) {
			return NkGuiValueKind::String;
		}
		const NkStringView lx = node.Lexeme();
		const char *s = lx.Data();
		const nk_size n = lx.Size();
		if (!s || n == 0) {
			return NkGuiValueKind::Invalid;
		}
		NkGuiValueKind k = NkGuiValueKind::Invalid;
		nk_size e = VScanValue(s, n, 0, &k);
		if (e == kVBad) {
			return NkGuiValueKind::Invalid;
		}
		VSkip(s, n, e);
		// TOUT le lexeme doit avoir ete consomme : `#12345 zut` n'est pas une
		// couleur suivie de bruit, c'est une valeur invalide.
		return (e == n) ? k : NkGuiValueKind::Invalid;
	}

	const char *NkGuiArchive::KindName(NkGuiValueKind k) noexcept {
		switch (k) {
			case NkGuiValueKind::Null:
				return "null";
			case NkGuiValueKind::Number:
				return "un nombre";
			case NkGuiValueKind::String:
				return "une chaine";
			case NkGuiValueKind::Color:
				return "une couleur";
			case NkGuiValueKind::Vec2:
				return "un Vec2";
			case NkGuiValueKind::Ident:
				return "un identifiant";
			case NkGuiValueKind::Flags:
				return "des drapeaux";
			case NkGuiValueKind::List:
				return "une liste";
			case NkGuiValueKind::Dict:
				return "un dictionnaire";
			default:
				return "une valeur mal formee";
		}
	}

	// =========================================================================
	//  LES VERSIONS ET LES MIGRATIONS  (etape 5)
	// =========================================================================

	namespace {

		/// LA MIGRATION 0.2 -> 0.3, ET ELLE NE TOUCHE A RIEN.
		///
		/// ⚠️ CE N'EST PAS UN OUBLI, ET IL VAUT MIEUX L'ECRIRE QUE DE LAISSER
		///    CROIRE QU'IL RESTE DU TRAVAIL ICI. Tout ce que la v0.3 a ajoute au
		///    socle v0.2 est ADDITIF : listes et dictionnaires comme valeurs,
		///    identifiant pointe, blocs `appearance`, sections `animation` et
		///    `fonts`, conservation des commentaires. Rien n'a ete retire, rien n'a
		///    ete resserre, aucune cle n'a change de nom. **Un document 0.2 EST un
		///    document 0.3 valide**, et le corpus des 10 -- qui est en 0.2 -- le
		///    montre : il se lit, se represente et se reemet a l'octet sans qu'une
		///    seule valeur soit transformee.
		///
		///    Elle est enregistree quand meme, et c'est le point : sans elle, le
		///    registre repondrait « No migration path from 0.2.0 to 0.3.0 » et
		///    REFUSERAIT les dix fichiers. Une migration vide n'est pas du vide --
		///    c'est la declaration explicite que la compatibilite est totale.
		nk_bool NkGMigrate_0_2_vers_0_3(NkArchive &, NkSchemaVersion, NkSchemaVersion) noexcept {
			return true;
		}

	} // namespace

	NkTypeId NkGuiArchive::DocumentType() noexcept {
		return NkTypeOf<NkGuiDocumentTag>();
	}

	NkSchemaVersion NkGuiArchive::VersionOf(const NkArchive &doc) noexcept {
		const NkArchiveNode *n = doc.FindNode(NkStringView(KeyVersion()));
		if (!n || !n->IsScalar()) {
			return NkSchemaVersion((nk_uint16)kMajor, (nk_uint16)kMinor, 0);
		}
		const NkString &t = n->value.text;
		nk_uint16 maj = 0;
		nk_uint16 min = 0;
		nk_size i = 0;
		for (; i < t.Size() && t.Data()[i] >= '0' && t.Data()[i] <= '9'; ++i) {
			maj = (nk_uint16)(maj * 10 + (t.Data()[i] - '0'));
		}
		if (i < t.Size() && t.Data()[i] == '.') {
			++i;
			for (; i < t.Size() && t.Data()[i] >= '0' && t.Data()[i] <= '9'; ++i) {
				min = (nk_uint16)(min * 10 + (t.Data()[i] - '0'));
			}
		}
		return NkSchemaVersion(maj, min, 0);
	}

	bool NkGuiArchive::SetVersion(NkArchive &doc, NkSchemaVersion v) noexcept {
		if (v.patch != 0) {
			return false;
		}
		NkString t;
		t.Append(NkString::Fmtf("%u.%u", (unsigned)v.major, (unsigned)v.minor));
		const nk_int32 rang = doc.GetSourceOrder(NkStringView(KeyVersion()));
		if (!SetToken(doc, NkStringView(KeyVersion()), NkStringView(t))) {
			return false;
		}
		doc.SetSourceOrder(NkStringView(KeyVersion()), rang < 0 ? 0 : rang);
		return true;
	}

	void NkGuiArchive::RegisterFormat() noexcept {
		static bool s_fait = false;
		if (s_fait) {
			return;
		}
		s_fait = true;
		NkSchemaRegistry::SetCurrentVersion(DocumentType(),
											NkSchemaVersion((nk_uint16)kMajor, (nk_uint16)kMinor, 0));
		NkSchemaRegistry::RegisterMigration(DocumentType(), NkSchemaVersion(0, 2, 0),
											NkSchemaVersion(0, 3, 0), NkGMigrate_0_2_vers_0_3);
	}

	bool NkGuiArchive::Migrate(NkArchive &doc, NkGuiDiag &err) noexcept {
		RegisterFormat();
		const NkSchemaVersion stored = VersionOf(doc);
		NkString motif;
		if (!NkSchemaRegistry::MigrateArchive(DocumentType(), doc, stored, &motif)) {
			err.code = NkString("E-MIGRATION");
			err.message = motif;
			return false;
		}
		// La version atteinte, telle que le registre la connait -- et pas une
		// constante recopiee ici, qui divergerait le jour ou elle changerait.
		const NkSchemaVersion atteinte = NkSchemaRegistry::GetCurrentVersion(DocumentType());
		doc.Remove(NkStringView("__meta__"));
		if (!SetVersion(doc, atteinte)) {
			err.code = NkString("E-MIGRATION");
			err.message = NkString("version cible inexprimable en `.nkgui` : ");
			err.message.Append(atteinte.ToString());
			return false;
		}
		return true;
	}

	bool NkGuiArchive::IsReservedKey(NkStringView key) noexcept {
		return key.Size() > 0 && key.Data() && key.Data()[0] == '$';
	}

	bool NkGuiArchive::IsToken(const NkArchiveNode &node) noexcept {
		return node.IsScalar() && node.value.type == NkArchiveValueType::NK_VALUE_STRING
			   && node.HasUsableLiteral();
	}

	void NkGuiArchive::SetTokenNode(NkArchiveNode &node, NkStringView token) noexcept {
		node.MakeScalar();
		node.value = NkArchiveValue::FromString(token);
		node.SetLiteral(token);
	}

	bool NkGuiArchive::SetToken(NkArchive &ar, NkStringView key, NkStringView token) noexcept {
		if (!ar.SetString(key, token)) {
			return false;
		}
		return ar.SetLiteral(key, token) != 0;
	}

	NkStringView NkGuiArchive::TypeOf(const NkArchive &block) noexcept {
		const NkArchiveNode *n = block.FindNode(NkStringView(KeyType()));
		if (!n || !n->IsScalar()) {
			return NkStringView();
		}
		return n->value.text.View();
	}

	NkStringView NkGuiArchive::IdOf(const NkArchive &block) noexcept {
		const NkArchiveNode *n = block.FindNode(NkStringView(KeyId()));
		if (!n || !n->IsScalar()) {
			return NkStringView();
		}
		return n->value.text.View();
	}

	NkArchiveNode &NkGuiArchive::EnsureBody(NkArchive &ar) noexcept {
		NkArchiveNode *b = ar.FindNode(NkStringView(KeyBody()));
		if (!b || !b->IsArray()) {
			NkVector<NkArchiveNode> empty;
			ar.SetNodeArray(NkStringView(KeyBody()), empty);
			b = ar.FindNode(NkStringView(KeyBody()));
		}
		return *b;
	}

	NkArchiveNode *NkGuiArchive::AddBlock(NkArchive &parent, NkStringView type,
										  NkStringView id) noexcept {
		NkArchive child;
		SetToken(child, NkStringView(KeyType()), type);
		child.SetSourceOrder(NkStringView(KeyType()), 0);
		if (id.Size() > 0) {
			child.SetString(NkStringView(KeyId()), id);
			child.SetSourceOrder(NkStringView(KeyId()), 1);
		}
		NkArchiveNode node;
		node.SetObject(child);
		NkArchiveNode &body = EnsureBody(parent);
		body.array.PushBack(node);
		return &body.array[body.array.Size() - 1];
	}

	NkGuiStyle NkGuiArchive::DetectStyle(const char *src, nk_uint32 length) noexcept {
		NkGuiStyle opt;
		if (!src || length == 0) {
			return opt;
		}
		opt.bom = (length >= 3 && (nk_uint8)src[0] == 0xEF && (nk_uint8)src[1] == 0xBB
				   && (nk_uint8)src[2] == 0xBF);
		opt.crlf = false;
		for (nk_uint32 i = 0; i + 1 < length; ++i) {
			if (src[i] == '\r' && src[i + 1] == '\n') {
				opt.crlf = true;
				break;
			}
			if (src[i] == '\n') {
				break;
			}
		}
		// La premiere ligne qui commence par des espaces donne la largeur d'un
		// cran : c'est forcement UN cran, jamais deux, parce qu'un fichier commence
		// par une section au niveau zero.
		opt.indent = 2;
		for (nk_uint32 i = 0; i < length; ++i) {
			if (src[i] != '\n') {
				continue;
			}
			nk_uint32 j = i + 1;
			nk_uint32 spaces = 0;
			while (j < length && src[j] == ' ') {
				++spaces;
				++j;
			}
			if (spaces > 0 && j < length && src[j] != '\r' && src[j] != '\n') {
				opt.indent = spaces;
				break;
			}
		}
		opt.finalNewline = (src[length - 1] == '\n');
		return opt;
	}

	bool NkGuiArchive::Read(const char *src, nk_uint32 length, NkArchive &out,
							NkGuiDiag &err) noexcept {
		out.Clear();
		out.ClearTrivia();
		if (!src) {
			err.code = NkString("E-PARSE");
			err.message = NkString("source nulle");
			return false;
		}

		NkVector<Tok> toks;
		Lexer lex(src, length);
		if (!lex.Run(toks, err)) {
			return false;
		}
		AttachTrivia(src, toks, lex.FirstOffset());

		if (toks.Size() < 2 || toks[0].kind != Tk::Ident || toks[0].text.Compare("nkgui") != 0) {
			err.code = NkString("E-PARSE");
			err.message = NkString("le fichier doit commencer par `nkgui <majeure>.<mineure>`");
			err.line = toks.Empty() ? 1 : toks[0].line;
			err.column = toks.Empty() ? 1 : toks[0].column;
			return false;
		}
		if (!toks[0].lead.Empty()) {
			out.SetHeaderTrivia(NkStringView(toks[0].lead));
		}

		// La version : la tranche de source qui suit `nkgui` sur sa ligne. Elle est
		// reemise TELLE QUELLE (regle (a)) -- un outil qui reestampille en silence
		// les fichiers qu'il touche rend tout diagnostic de version impossible.
		const nk_size v1 = SpanEnd(toks, 1, false);
		if (toks[1].kind == Tk::End) {
			err.code = NkString("E-PARSE");
			err.message = NkString("version absente apres `nkgui`");
			err.line = toks[0].line;
			return false;
		}
		const NkString version(src + toks[1].begin, toks[v1].end - toks[1].begin);

		// Regle (c) : une MAJEURE superieure est une rupture. « ce fichier est trop
		// recent pour moi » vaut mieux que l'ouvrir en en perdant la moitie.
		nk_int32 major = 0;
		{
			const char *p = version.Data();
			for (nk_size c = 0; c < version.Size() && p[c] >= '0' && p[c] <= '9'; ++c) {
				major = major * 10 + (nk_int32)(p[c] - '0');
			}
		}
		if (major > kMajor) {
			// LE MESSAGE EST LA MOITIE DE LA REGLE (c). « ce fichier est trop recent
			// pour moi » vaut mieux que l'ouvrir en en perdant la moitie -- et il
			// faut que le message le DISE, sinon celui qui le lit ne sait pas s'il
			// doit mettre son outil a jour ou reparer son fichier.
			err.code = NkString("E-VERSION-INCOMPATIBLE");
			err.message = NkString("ce fichier est trop recent pour moi : version ");
			err.message.Append(version);
			err.message.Append(", je ne comprends que la majeure 0");
			err.line = toks[1].line;
			err.column = toks[1].column;
			return false;
		}

		SetToken(out, NkStringView(KeyVersion()), NkStringView(version));
		out.SetSourceOrder(NkStringView(KeyVersion()), 0);
		out.SetTrailingTrivia(NkStringView(KeyVersion()), NkStringView(toks[v1].trail));

		nk_size i = v1 + 1;
		Parser parser(src, toks);
		if (!parser.Members(i, out, err)) {
			return false;
		}
		if (i < toks.Size() && IsPunct(toks[i], '}')) {
			err.code = NkString("E-PARSE");
			err.message = NkString("accolade fermante sans bloc ouvert");
			err.line = toks[i].line;
			err.column = toks[i].column;
			return false;
		}
		// Les lignes de la fin du fichier. Sans elles, chaque enregistrement
		// rognerait un peu plus la fin du document.
		if (!toks.Empty() && !toks[toks.Size() - 1].lead.Empty()) {
			out.SetFooterTrivia(NkStringView(toks[toks.Size() - 1].lead));
		}
		return true;
	}

	NkString NkGuiArchive::Write(const NkArchive &doc, const NkGuiStyle &style) {
		Writer w(style);
		return w.Run(doc);
	}

} // namespace nkentseu

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
