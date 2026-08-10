//
// NkPdfLoad.cpp — chargement d'un document PDF : index (xref), objets, filtres,
// arbre des pages. L'analyse syntaxique elle-meme vit dans NkPdf.cpp.
//
#include "NKMedia/Pdf/NkPdf.h"

#include "NKFileSystem/NkFile.h"
#include "NKImage/Core/NkImage.h" // NkDeflate

namespace nkentseu {
	namespace media {
		namespace pdf {

			static inline bool IsWsL(uint8 c) {
				return c == 0 || c == 9 || c == 10 || c == 12 || c == 13 || c == 32;
			}
			static bool KwAt(const NkVector<uint8> &b, usize p, const char *kw) {
				usize i = 0;
				for (; kw[i]; ++i)
					if (p + i >= b.Size() || b[p + i] != static_cast<uint8>(kw[i]))
						return false;
				return true;
			}

			// ============================================================
			// Ouverture
			// ============================================================

			const char *NkPdfDoc::StatusText() const {
				switch (mStatus) {
					case NK_PDF_OK: return "";
					case NK_PDF_ERR_OPEN: return "Fichier illisible.";
					case NK_PDF_ERR_SIGNATURE: return "Ce fichier n'est pas un PDF (signature %PDF- absente).";
					case NK_PDF_ERR_XREF: return "Index du document introuvable ou corrompu.";
					case NK_PDF_ERR_ENCRYPTED:
						return "Document chiffre : la lecture des documents proteges n'est pas encore prise en "
							   "charge.";
					case NK_PDF_ERR_STRUCTURE: return "Structure du document inexploitable (catalogue ou pages).";
				}
				return "";
			}

			void NkPdfDoc::Close() {
				mBuf.Clear();
				mPool.Clear();
				mUnsupported.Clear();
				mVals.Clear();
				mKids.Clear();
				mEnts.Clear();
				mXref.Clear();
				mCache.Clear();
				mLoading.Clear();
				mPages.Clear();
				mPageParent.Clear();
				mObjStmDone.Clear();
				mTrailer = -1;
				mRoot = -1;
				mStatus = NK_PDF_ERR_OPEN;
			}

			NkPdfStatus NkPdfDoc::Open(const char *path) {
				Close();
				if (!path || !*path)
					return (mStatus = NK_PDF_ERR_OPEN);
				mBuf = NkFile::ReadAllBytes(path);
				if (mBuf.Size() < 8)
					return (mStatus = NK_PDF_ERR_OPEN);

				// Signature : toleree dans les 1024 premiers octets. Certains
				// generateurs prefixent le fichier (le decalage devient alors l'origine
				// des offsets de l'index, mais en pratique les lecteurs reels se
				// contentent du repli par balayage — c'est ce qu'on fait aussi).
				bool sig = false;
				for (usize i = 0; i < 1024 && i + 5 <= mBuf.Size(); ++i)
					if (KwAt(mBuf, i, "%PDF-")) {
						sig = true;
						break;
					}
				if (!sig)
					return (mStatus = NK_PDF_ERR_SIGNATURE);

				mVals.PushBack(NkPdfVal()); // index 0 = objet nul partage

				if (!LoadXref()) {
					// Index absent ou incoherent : BALAYAGE COMPLET a la recherche des
					// « N G obj ». C'est ce que font les lecteurs reels sur les fichiers
					// abimes, et ca rattrape aussi les PDF a offsets decales.
					mXref.Clear();
					for (usize p = 0; p + 3 < mBuf.Size(); ++p) {
						if (!KwAt(mBuf, p, "obj"))
							continue;
						if (p + 3 < mBuf.Size() && !IsWsL(mBuf[p + 3]) && mBuf[p + 3] != '<' &&
							mBuf[p + 3] != '[' && mBuf[p + 3] != '/')
							continue;
						// Remonte « N G » avant « obj ».
						usize q = p;
						while (q > 0 && IsWsL(mBuf[q - 1]))
							--q;
						const usize genEnd = q;
						while (q > 0 && mBuf[q - 1] >= '0' && mBuf[q - 1] <= '9')
							--q;
						const usize genBeg = q;
						if (genBeg == genEnd)
							continue;
						while (q > 0 && IsWsL(mBuf[q - 1]))
							--q;
						const usize numEnd = q;
						while (q > 0 && mBuf[q - 1] >= '0' && mBuf[q - 1] <= '9')
							--q;
						const usize numBeg = q;
						if (numBeg == numEnd)
							continue;
						int32 num = 0;
						for (usize k = numBeg; k < numEnd; ++k)
							num = num * 10 + (mBuf[k] - '0');
						if (num <= 0 || num > 5000000)
							continue;
						if (static_cast<usize>(num) >= mXref.Size())
							mXref.Resize(static_cast<usize>(num) + 1);
						XEntry &e = mXref[static_cast<usize>(num)];
						e.type = 1;
						e.off = numBeg; // la version la PLUS TARDIVE gagne (mises a jour incrementales)
					}
					if (mXref.Empty())
						return (mStatus = NK_PDF_ERR_XREF);
					// Trailer : cherche le dernier « trailer », sinon on trouvera le
					// catalogue en balayant les objets (voir BuildPageList).
					for (usize p = mBuf.Size(); p-- > 7;)
						if (KwAt(mBuf, p, "trailer")) {
							usize q = p + 7;
							mTrailer = ParseValueAt(q, 0);
							break;
						}
				}

				mCache.Resize(mXref.Size());
				for (usize i = 0; i < mCache.Size(); ++i)
					mCache[i] = -1;
				mLoading.Resize(mXref.Size());
				for (usize i = 0; i < mLoading.Size(); ++i)
					mLoading[i] = 0;

				// Chiffrement : on le DIT au lieu d'afficher des pages vides.
				if (mTrailer >= 0) {
					const NkPdfVal enc = DictGet(mVals[static_cast<usize>(mTrailer)], "Encrypt");
					if (!enc.IsNull())
						return (mStatus = NK_PDF_ERR_ENCRYPTED);
				}

				if (!BuildPageList())
					return (mStatus = NK_PDF_ERR_STRUCTURE);
				return (mStatus = NK_PDF_OK);
			}

			// ============================================================
			// Index (xref)
			// ============================================================

			bool NkPdfDoc::LoadXref() {
				// « startxref » en fin de fichier.
				const usize tail = mBuf.Size() > 2048 ? mBuf.Size() - 2048 : 0;
				usize sx = 0;
				bool found = false;
				for (usize p = mBuf.Size(); p-- > tail;)
					if (KwAt(mBuf, p, "startxref")) {
						sx = p + 9;
						found = true;
						break;
					}
				if (!found)
					return false;
				while (sx < mBuf.Size() && IsWsL(mBuf[sx]))
					++sx;
				usize pos = 0;
				bool any = false;
				while (sx < mBuf.Size() && mBuf[sx] >= '0' && mBuf[sx] <= '9') {
					pos = pos * 10 + static_cast<usize>(mBuf[sx++] - '0');
					any = true;
				}
				if (!any || pos >= mBuf.Size())
					return false;
				return LoadXrefAt(pos, 0);
			}

			bool NkPdfDoc::LoadXrefAt(usize pos, int32 depth) {
				if (depth > 32 || pos >= mBuf.Size()) // chaine /Prev bornee (cycles)
					return false;
				usize p = pos;
				while (p < mBuf.Size() && IsWsL(mBuf[p]))
					++p;
				if (KwAt(mBuf, p, "xref")) {
					p += 4;
					return LoadXrefTable(p, depth);
				}
				return LoadXrefStream(p, depth);
			}

			bool NkPdfDoc::LoadXrefTable(usize &p, int32 depth) {
				for (;;) {
					while (p < mBuf.Size() && IsWsL(mBuf[p]))
						++p;
					if (KwAt(mBuf, p, "trailer")) {
						p += 7;
						const int32 t = ParseValueAt(p, 0);
						if (mTrailer < 0)
							mTrailer = t;
						if (t >= 0) {
							// /XRefStm : index hybride (table + flux) — les objets
							// compresses ne sont QUE dans le flux.
							const NkPdfVal hyb = DictGet(mVals[static_cast<usize>(t)], "XRefStm");
							if (hyb.IsNum())
								LoadXrefAt(static_cast<usize>(hyb.num), depth + 1);
							const NkPdfVal prev = DictGet(mVals[static_cast<usize>(t)], "Prev");
							if (prev.IsNum())
								LoadXrefAt(static_cast<usize>(prev.num), depth + 1);
						}
						return true;
					}
					// Sous-section « premier nombre » puis « nombre d'entrees ».
					if (p >= mBuf.Size() || mBuf[p] < '0' || mBuf[p] > '9')
						return true;
					int32 first = 0, count = 0;
					while (p < mBuf.Size() && mBuf[p] >= '0' && mBuf[p] <= '9')
						first = first * 10 + (mBuf[p++] - '0');
					while (p < mBuf.Size() && IsWsL(mBuf[p]))
						++p;
					while (p < mBuf.Size() && mBuf[p] >= '0' && mBuf[p] <= '9')
						count = count * 10 + (mBuf[p++] - '0');
					if (count < 0 || count > 5000000)
						return false;
					for (int32 i = 0; i < count; ++i) {
						while (p < mBuf.Size() && IsWsL(mBuf[p]))
							++p;
						if (p + 18 > mBuf.Size())
							return true;
						usize off = 0;
						for (int32 k = 0; k < 10 && p < mBuf.Size(); ++k, ++p)
							if (mBuf[p] >= '0' && mBuf[p] <= '9')
								off = off * 10 + static_cast<usize>(mBuf[p] - '0');
						while (p < mBuf.Size() && IsWsL(mBuf[p]))
							++p;
						for (int32 k = 0; k < 5 && p < mBuf.Size(); ++k, ++p)
							; // generation : ignoree (on ne gere pas les objets multi-generations)
						while (p < mBuf.Size() && IsWsL(mBuf[p]))
							++p;
						const uint8 kind = p < mBuf.Size() ? mBuf[p++] : 'f';
						const int32 num = first + i;
						if (kind != 'n' || num <= 0)
							continue;
						if (static_cast<usize>(num) >= mXref.Size())
							mXref.Resize(static_cast<usize>(num) + 1);
						XEntry &e = mXref[static_cast<usize>(num)];
						if (e.type == 0) { // premiere vue = la plus recente
							e.type = 1;
							e.off = off;
						}
					}
				}
			}

			bool NkPdfDoc::LoadXrefStream(usize &p, int32 depth) {
				// « N G obj << ... >> stream ... » : saute l'en-tete d'objet.
				while (p < mBuf.Size() && IsWsL(mBuf[p]))
					++p;
				while (p < mBuf.Size() && mBuf[p] >= '0' && mBuf[p] <= '9')
					++p;
				while (p < mBuf.Size() && IsWsL(mBuf[p]))
					++p;
				while (p < mBuf.Size() && mBuf[p] >= '0' && mBuf[p] <= '9')
					++p;
				while (p < mBuf.Size() && IsWsL(mBuf[p]))
					++p;
				if (!KwAt(mBuf, p, "obj"))
					return false;
				p += 3;
				const int32 idx = ParseValueAt(p, 0);
				if (idx < 0 || mVals[static_cast<usize>(idx)].kind != NK_PDF_STREAM)
					return false;
				const NkPdfVal stm = mVals[static_cast<usize>(idx)];
				if (mTrailer < 0)
					mTrailer = idx; // le dictionnaire du flux d'index EST le trailer

				NkVector<uint8> data;
				if (!DecodeStream(stm, data))
					return false;

				// /W : largeur en octets de chacun des 3 champs.
				const NkPdfVal w = DictGet(stm, "W");
				if (w.kind != NK_PDF_ARRAY || w.b < 3)
					return false;
				int32 wid[3] = {0, 0, 0};
				for (int32 i = 0; i < 3; ++i)
					wid[i] = static_cast<int32>(Num(ArrayAt(w, i)));
				const int32 rowSz = wid[0] + wid[1] + wid[2];
				if (rowSz <= 0)
					return false;

				// /Index : couples (premier, nombre). Absent => [0 /Size].
				NkVector<int32> ranges;
				const NkPdfVal index = DictGet(stm, "Index");
				if (index.kind == NK_PDF_ARRAY) {
					for (int32 i = 0; i + 1 < index.b; i += 2) {
						ranges.PushBack(static_cast<int32>(Num(ArrayAt(index, i))));
						ranges.PushBack(static_cast<int32>(Num(ArrayAt(index, i + 1))));
					}
				} else {
					ranges.PushBack(0);
					ranges.PushBack(static_cast<int32>(Num(DictGet(stm, "Size"))));
				}

				usize row = 0;
				for (usize r = 0; r + 1 < ranges.Size(); r += 2) {
					const int32 first = ranges[r], count = ranges[r + 1];
					for (int32 i = 0; i < count; ++i) {
						if ((row + 1) * static_cast<usize>(rowSz) > data.Size())
							break;
						const uint8 *rec = data.Data() + row * static_cast<usize>(rowSz);
						++row;
						// Champ 1 absent => type 1 par defaut (specification).
						uint64 f[3] = {1, 0, 0};
						int32 o = 0;
						for (int32 k = 0; k < 3; ++k) {
							if (wid[k] == 0)
								continue;
							uint64 v = 0;
							for (int32 b = 0; b < wid[k]; ++b)
								v = (v << 8) | rec[o++];
							f[k] = v;
						}
						const int32 num = first + i;
						if (num <= 0 || f[0] == 0) // 0 = objet libre
							continue;
						if (static_cast<usize>(num) >= mXref.Size())
							mXref.Resize(static_cast<usize>(num) + 1);
						XEntry &e = mXref[static_cast<usize>(num)];
						if (e.type != 0)
							continue; // deja connu par un index plus recent
						if (f[0] == 1) {
							e.type = 1;
							e.off = static_cast<usize>(f[1]);
						} else if (f[0] == 2) {
							e.type = 2;
							e.stmNum = static_cast<int32>(f[1]);
							e.stmIdx = static_cast<int32>(f[2]);
						}
					}
				}

				const NkPdfVal prev = DictGet(stm, "Prev");
				if (prev.IsNum())
					LoadXrefAt(static_cast<usize>(prev.num), depth + 1);
				return true;
			}

			// ============================================================
			// Chargement paresseux des objets
			// ============================================================

			// mCache/mLoading doivent suivre mXref. L'analyse d'un flux d'index resout
			// deja des references indirectes (un /Length) alors que l'index est encore
			// en construction : sans cet alignement, on indexait des tableaux vides.
			void NkPdfDoc::EnsureTables() const {
				NkPdfDoc *self = const_cast<NkPdfDoc *>(this);
				const usize n = mXref.Size();
				if (mCache.Size() < n) {
					const usize old = mCache.Size();
					self->mCache.Resize(n);
					for (usize i = old; i < n; ++i)
						self->mCache[i] = -1;
				}
				if (mLoading.Size() < n) {
					const usize old = mLoading.Size();
					self->mLoading.Resize(n);
					for (usize i = old; i < n; ++i)
						self->mLoading[i] = 0;
				}
			}

			int32 NkPdfDoc::LoadObject(int32 num) const {
				if (num <= 0 || static_cast<usize>(num) >= mXref.Size())
					return -1;
				EnsureTables();
				const usize n = static_cast<usize>(num);
				if (mCache[n] >= 0)
					return mCache[n];
				if (mLoading[n]) // reference circulaire : coupe net
					return -1;

				NkPdfDoc *self = const_cast<NkPdfDoc *>(this);
				const XEntry e = mXref[n];
				if (e.type == 1) {
					if (e.off >= mBuf.Size())
						return -1;
					self->mLoading[n] = 1;
					// « N G obj » puis la valeur.
					usize p = e.off;
					while (p < mBuf.Size() && IsWsL(mBuf[p]))
						++p;
					while (p < mBuf.Size() && mBuf[p] >= '0' && mBuf[p] <= '9')
						++p;
					while (p < mBuf.Size() && IsWsL(mBuf[p]))
						++p;
					while (p < mBuf.Size() && mBuf[p] >= '0' && mBuf[p] <= '9')
						++p;
					while (p < mBuf.Size() && IsWsL(mBuf[p]))
						++p;
					if (!KwAt(mBuf, p, "obj")) {
						self->mLoading[n] = 0;
						return -1;
					}
					p += 3;
					const int32 idx = self->ParseValueAt(p, 0);
					self->mLoading[n] = 0;
					self->mCache[n] = idx;
					return idx;
				}
				if (e.type == 2) {
					self->mLoading[n] = 1;
					const bool ok = self->LoadObjStm(e.stmNum);
					self->mLoading[n] = 0;
					if (ok && mCache[n] >= 0)
						return mCache[n];
					return -1;
				}
				return -1;
			}

			// Depaquette un flux d'objets (/Type /ObjStm) : en-tete de couples
			// « numero decalage », puis les objets concatenes.
			bool NkPdfDoc::LoadObjStm(int32 objNum) {
				for (usize i = 0; i < mObjStmDone.Size(); ++i)
					if (mObjStmDone[i] == objNum)
						return true; // deja depaquete
				const int32 sidx = LoadObject(objNum);
				if (sidx < 0 || mVals[static_cast<usize>(sidx)].kind != NK_PDF_STREAM)
					return false;
				const NkPdfVal stm = mVals[static_cast<usize>(sidx)];
				NkVector<uint8> data;
				if (!DecodeStream(stm, data))
					return false;
				const int32 n = static_cast<int32>(Num(DictGet(stm, "N")));
				const usize first = static_cast<usize>(Num(DictGet(stm, "First")));
				if (n <= 0 || first > data.Size())
					return false;
				mObjStmDone.PushBack(objNum);

				// L'analyseur travaille sur mBuf : on y annexe temporairement les
				// donnees depaquetees plutot que de dupliquer tout l'analyseur pour un
				// tampon different. L'annexe reste en memoire (les objets y pointent).
				const usize base = mBuf.Size();
				for (usize i = 0; i < data.Size(); ++i)
					mBuf.PushBack(data[i]);
				mBuf.PushBack(0); // separateur : evite qu'un jeton deborde sur la suite

				usize p = base;
				NkVector<int32> nums;
				NkVector<usize> offs;
				for (int32 i = 0; i < n; ++i) {
					while (p < mBuf.Size() && IsWsL(mBuf[p]))
						++p;
					int32 num = 0;
					bool got = false;
					while (p < mBuf.Size() && mBuf[p] >= '0' && mBuf[p] <= '9') {
						num = num * 10 + (mBuf[p++] - '0');
						got = true;
					}
					while (p < mBuf.Size() && IsWsL(mBuf[p]))
						++p;
					usize off = 0;
					while (p < mBuf.Size() && mBuf[p] >= '0' && mBuf[p] <= '9')
						off = off * 10 + static_cast<usize>(mBuf[p++] - '0');
					if (!got)
						break;
					nums.PushBack(num);
					offs.PushBack(off);
				}
				for (usize i = 0; i < nums.Size(); ++i) {
					usize q = base + first + offs[i];
					if (q >= mBuf.Size())
						continue;
					const int32 idx = ParseValueAt(q, 0);
					const int32 num = nums[i];
					if (idx >= 0 && num > 0 && static_cast<usize>(num) < mCache.Size() &&
						mCache[static_cast<usize>(num)] < 0)
						mCache[static_cast<usize>(num)] = idx;
				}
				return true;
			}

			// ============================================================
			// Filtres
			// ============================================================

			bool NkPdfDoc::Inflate(const uint8 *in, usize inSz, NkVector<uint8> &out) {
				// NkDeflate exige une capacite CONNUE, or un flux PDF ne declare pas sa
				// taille decompressee. On tente donc des capacites croissantes. Le
				// facteur 4 puis x4 couvre le cas courant en une passe, sans exploser la
				// memoire sur un flux minuscule.
				usize cap = inSz * 4 + 4096;
				for (int32 essai = 0; essai < 8; ++essai) {
					out.Resize(cap);
					usize written = 0;
					if (NkDeflate::Decompress(in, inSz, out.Data(), cap, written)) {
						out.Resize(written);
						return true;
					}
					if (cap > (64u << 20)) // 64 Mio : au-dela, le flux est aberrant
						break;
					cap *= 4;
				}
				out.Clear();
				return false;
			}

			bool NkPdfDoc::AsciiHex(const uint8 *in, usize inSz, NkVector<uint8> &out) {
				int32 hi = -1;
				for (usize i = 0; i < inSz; ++i) {
					const uint8 c = in[i];
					if (c == '>')
						break;
					int32 h = -1;
					if (c >= '0' && c <= '9')
						h = c - '0';
					else if (c >= 'a' && c <= 'f')
						h = c - 'a' + 10;
					else if (c >= 'A' && c <= 'F')
						h = c - 'A' + 10;
					else
						continue;
					if (hi < 0)
						hi = h;
					else {
						out.PushBack(static_cast<uint8>((hi << 4) | h));
						hi = -1;
					}
				}
				if (hi >= 0)
					out.PushBack(static_cast<uint8>(hi << 4));
				return true;
			}

			bool NkPdfDoc::Ascii85(const uint8 *in, usize inSz, NkVector<uint8> &out) {
				uint32 tuple = 0;
				int32 count = 0;
				usize i = 0;
				if (inSz >= 2 && in[0] == '<' && in[1] == '~')
					i = 2;
				for (; i < inSz; ++i) {
					const uint8 c = in[i];
					if (c == '~')
						break;
					if (c == 'z' && count == 0) { // raccourci : 4 octets nuls
						for (int32 k = 0; k < 4; ++k)
							out.PushBack(0);
						continue;
					}
					if (c < '!' || c > 'u')
						continue; // espaces et fins de ligne
					tuple = tuple * 85u + static_cast<uint32>(c - '!');
					if (++count == 5) {
						for (int32 k = 3; k >= 0; --k)
							out.PushBack(static_cast<uint8>((tuple >> (k * 8)) & 0xFF));
						tuple = 0;
						count = 0;
					}
				}
				if (count > 1) { // groupe partiel : complete par 'u' puis tronque
					for (int32 k = count; k < 5; ++k)
						tuple = tuple * 85u + 84u;
					for (int32 k = 3; k >= 5 - count; --k)
						out.PushBack(static_cast<uint8>((tuple >> (k * 8)) & 0xFF));
				}
				return true;
			}

			bool NkPdfDoc::RunLength(const uint8 *in, usize inSz, NkVector<uint8> &out) {
				usize i = 0;
				while (i < inSz) {
					const uint8 l = in[i++];
					if (l == 128)
						break; // marqueur de fin
					if (l < 128) {
						for (int32 k = 0; k <= l && i < inSz; ++k)
							out.PushBack(in[i++]);
					} else {
						if (i >= inSz)
							break;
						const uint8 v = in[i++];
						for (int32 k = 0; k < 257 - l; ++k)
							out.PushBack(v);
					}
				}
				return true;
			}

			// Predicteurs de /DecodeParms. Sans eux, un flux d'index Flate ressort en
			// bruit et le document parait corrompu : c'est le piege classique.
			bool NkPdfDoc::Unpredict(const NkPdfVal &parms, NkVector<uint8> &data) const {
				if (!parms.IsDictLike())
					return true;
				const int32 pred = static_cast<int32>(Num(DictGet(parms, "Predictor"), 1));
				if (pred <= 1)
					return true;
				const int32 colors = static_cast<int32>(Num(DictGet(parms, "Colors"), 1));
				const int32 bpc = static_cast<int32>(Num(DictGet(parms, "BitsPerComponent"), 8));
				const int32 columns = static_cast<int32>(Num(DictGet(parms, "Columns"), 1));
				if (colors <= 0 || bpc <= 0 || columns <= 0)
					return false;
				const int32 bpp = (colors * bpc + 7) / 8;			   // octets par pixel (>=1)
				const int32 rowLen = (columns * colors * bpc + 7) / 8; // octets par ligne
				if (rowLen <= 0)
					return false;

				if (pred == 2) { // predicteur TIFF : seul le cas 8 bits est courant
					if (bpc != 8)
						return true;
					for (usize r = 0; r + static_cast<usize>(rowLen) <= data.Size();
						 r += static_cast<usize>(rowLen))
						for (int32 i = bpp; i < rowLen; ++i)
							data[r + static_cast<usize>(i)] =
								static_cast<uint8>(data[r + static_cast<usize>(i)] +
												   data[r + static_cast<usize>(i - bpp)]);
					return true;
				}

				// Predicteurs PNG : chaque ligne est prefixee par son type de filtre.
				const usize stride = static_cast<usize>(rowLen) + 1;
				const usize rows = data.Size() / stride;
				NkVector<uint8> out;
				out.Resize(rows * static_cast<usize>(rowLen));
				for (usize r = 0; r < rows; ++r) {
					const uint8 ft = data[r * stride];
					const uint8 *src = data.Data() + r * stride + 1;
					uint8 *dst = out.Data() + r * static_cast<usize>(rowLen);
					const uint8 *up = (r > 0) ? (out.Data() + (r - 1) * static_cast<usize>(rowLen)) : nullptr;
					for (int32 i = 0; i < rowLen; ++i) {
						const int32 a = (i >= bpp) ? dst[i - bpp] : 0;			 // gauche
						const int32 b = up ? up[i] : 0;							 // haut
						const int32 c = (up && i >= bpp) ? up[i - bpp] : 0;		 // haut-gauche
						int32 v = src[i];
						switch (ft) {
							case 0: break;					  // None
							case 1: v += a; break;			  // Sub
							case 2: v += b; break;			  // Up
							case 3: v += (a + b) / 2; break; // Average
							case 4: {						  // Paeth
								const int32 p = a + b - c;
								const int32 pa = p > a ? p - a : a - p;
								const int32 pb = p > b ? p - b : b - p;
								const int32 pc = p > c ? p - c : c - p;
								v += (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
								break;
							}
							default: break; // type inconnu : laisse tel quel
						}
						dst[i] = static_cast<uint8>(v & 0xFF);
					}
				}
				data = out;
				return true;
			}

			bool NkPdfDoc::ApplyFilter(const char *name, int32 nameLen, const NkPdfVal &parms,
									   const NkVector<uint8> &in, NkVector<uint8> &out) const {
				auto is = [&](const char *s) {
					int32 i = 0;
					for (; s[i]; ++i)
						if (i >= nameLen || name[i] != s[i])
							return false;
					return i == nameLen;
				};
				bool ok = false;
				if (is("FlateDecode") || is("Fl"))
					ok = Inflate(in.Data(), in.Size(), out);
				else if (is("ASCII85Decode") || is("A85"))
					ok = Ascii85(in.Data(), in.Size(), out);
				else if (is("ASCIIHexDecode") || is("AHx"))
					ok = AsciiHex(in.Data(), in.Size(), out);
				else if (is("RunLengthDecode") || is("RL"))
					ok = RunLength(in.Data(), in.Size(), out);
				else if (is("Crypt")) { // filtre d'identite en pratique
					out = in;
					ok = true;
				} else {
					// DCTDecode / JPXDecode / CCITTFaxDecode / JBIG2Decode : ce sont des
					// filtres d'IMAGE. Ils ne se decodent pas ici : les octets sont
					// remis tels quels au decodeur d'image, qui sait les lire.
					if (is("DCTDecode") || is("DCT") || is("JPXDecode") || is("CCITTFaxDecode") ||
						is("JBIG2Decode")) {
						out = in;
						return true;
					}
					NkPdfDoc *self = const_cast<NkPdfDoc *>(this);
					if (self->mUnsupported.Empty())
						for (int32 i = 0; i < nameLen; ++i)
							self->mUnsupported += name[i];
					return false;
				}
				if (ok)
					const_cast<NkPdfDoc *>(this)->Unpredict(parms, out);
				return ok;
			}

			bool NkPdfDoc::DecodeStream(const NkPdfVal &stream, NkVector<uint8> &out) const {
				out.Clear();
				if (stream.kind != NK_PDF_STREAM)
					return false;
				if (stream.rawOff + stream.rawLen > mBuf.Size())
					return false;

				NkVector<uint8> cur;
				cur.Resize(stream.rawLen);
				for (usize i = 0; i < stream.rawLen; ++i)
					cur[i] = mBuf[stream.rawOff + i];

				NkPdfVal filt = DictGet(stream, "Filter");
				if (filt.IsNull())
					filt = DictGet(stream, "F"); // abreviation (flux en ligne)
				NkPdfVal parms = DictGet(stream, "DecodeParms");
				if (parms.IsNull())
					parms = DictGet(stream, "DP");

				if (filt.IsNull()) { // aucun filtre : donnees brutes
					out = cur;
					return true;
				}
				if (filt.kind == NK_PDF_NAME) {
					int32 n = 0;
					const char *s = Text(filt, &n);
					return ApplyFilter(s, n, parms, cur, out);
				}
				if (filt.kind == NK_PDF_ARRAY) {
					// Chaine de filtres : appliques dans l'ordre du tableau.
					for (int32 i = 0; i < filt.b; ++i) {
						const NkPdfVal f = ArrayAt(filt, i);
						if (f.kind != NK_PDF_NAME)
							return false;
						const NkPdfVal pp = (parms.kind == NK_PDF_ARRAY) ? ArrayAt(parms, i) : parms;
						int32 n = 0;
						const char *s = Text(f, &n);
						NkVector<uint8> tmp;
						if (!ApplyFilter(s, n, pp, cur, tmp))
							return false;
						cur = tmp;
					}
					out = cur;
					return true;
				}
				return false;
			}

			// ============================================================
			// Arbre des pages
			// ============================================================

			int32 NkPdfDoc::ResolveIdx(int32 rawIdx) const {
				if (rawIdx < 0 || static_cast<usize>(rawIdx) >= mVals.Size())
					return -1;
				int32 cur = rawIdx;
				for (int32 hop = 0; hop < 32; ++hop) { // borne : cycle de references
					const NkPdfVal &v = mVals[static_cast<usize>(cur)];
					if (v.kind != NK_PDF_REF)
						return cur;
					const int32 nxt = LoadObject(v.a);
					if (nxt < 0)
						return -1;
					cur = nxt;
				}
				return -1;
			}

			int32 NkPdfDoc::DictGetIdx(const NkPdfVal &dict, const char *key) const {
				if (!dict.IsDictLike())
					return -1;
				int32 klen = 0;
				while (key[klen])
					++klen;
				for (int32 i = 0; i < dict.b; ++i) {
					const usize ei = static_cast<usize>(dict.a + i);
					if (ei >= mEnts.Size())
						break;
					const Ent &e = mEnts[ei];
					if (e.keyLen != klen)
						continue;
					bool same = true;
					for (int32 k = 0; k < klen && same; ++k)
						same = mPool.CStr()[e.keyOff + k] == key[k];
					if (same)
						return ResolveIdx(e.val);
				}
				return -1;
			}

			int32 NkPdfDoc::ArrayRawAt(const NkPdfVal &arr, int32 i) const {
				if (arr.kind != NK_PDF_ARRAY || i < 0 || i >= arr.b)
					return -1;
				const usize ki = static_cast<usize>(arr.a + i);
				if (ki >= mKids.Size())
					return -1;
				return ResolveIdx(mKids[ki]);
			}

			void NkPdfDoc::WalkPages(int32 nodeIdx, int32 depth) {
				if (depth > kMaxDepth || nodeIdx < 0 || static_cast<usize>(nodeIdx) >= mVals.Size())
					return;
				const NkPdfVal node = mVals[static_cast<usize>(nodeIdx)];
				if (!node.IsDictLike())
					return;
				// Garde anti-cycle : un /Kids qui pointe vers un ancetre boucherait
				// jusqu'a kMaxDepth a chaque branche. On refuse de revisiter un noeud.
				for (usize i = 0; i < mPages.Size(); ++i)
					if (mPages[i] == nodeIdx)
						return;
				const NkPdfVal kids = DictGet(node, "Kids");
				// Un noeud SANS /Kids est une feuille, meme si son /Type manque : des
				// generateurs l'omettent. Se fier au seul /Type perdrait ces pages.
				if (kids.kind != NK_PDF_ARRAY) {
					const NkPdfVal type = DictGet(node, "Type");
					if (NameIs(type, "Page") || type.IsNull())
						mPages.PushBack(nodeIdx);
					return;
				}
				for (int32 i = 0; i < kids.b; ++i)
					WalkPages(ArrayRawAt(kids, i), depth + 1);
			}

			bool NkPdfDoc::BuildPageList() {
				int32 rootIdx = -1;
				if (mTrailer >= 0)
					rootIdx = DictGetIdx(mVals[static_cast<usize>(mTrailer)], "Root");
				if (rootIdx < 0 || !mVals[static_cast<usize>(rootIdx)].IsDictLike()) {
					// Pas de trailer exploitable : cherche un objet /Type /Catalog.
					rootIdx = -1;
					for (usize n = 1; n < mXref.Size(); ++n) {
						const int32 idx = LoadObject(static_cast<int32>(n));
						if (idx < 0)
							continue;
						const NkPdfVal v = mVals[static_cast<usize>(idx)];
						if (!v.IsDictLike())
							continue;
						if (NameIs(DictGet(v, "Type"), "Catalog")) {
							rootIdx = idx;
							break;
						}
					}
				}
				if (rootIdx >= 0) {
					const int32 pagesIdx = DictGetIdx(mVals[static_cast<usize>(rootIdx)], "Pages");
					if (pagesIdx >= 0)
						WalkPages(pagesIdx, 0);
				}
				mRoot = rootIdx;
				if (mPages.Empty()) {
					// Dernier recours : tout objet /Type /Page. L'ordre suit alors les
					// numeros d'objet, ce qui n'est pas garanti — mieux que rien.
					for (usize n = 1; n < mXref.Size(); ++n) {
						const int32 idx = LoadObject(static_cast<int32>(n));
						if (idx < 0)
							continue;
						const NkPdfVal v = mVals[static_cast<usize>(idx)];
						if (v.IsDictLike() && NameIs(DictGet(v, "Type"), "Page"))
							mPages.PushBack(idx);
					}
				}
				return !mPages.Empty();
			}

			NkPdfVal NkPdfDoc::Page(int32 i) const {
				if (i < 0 || static_cast<usize>(i) >= mPages.Size())
					return NkPdfVal();
				return mVals[static_cast<usize>(mPages[static_cast<usize>(i)])];
			}

			// Attribut herite : remonte la chaine /Parent, bornee.
			NkPdfVal NkPdfDoc::Inherited(int32 pageIdx, const char *key) const {
				NkPdfVal node = Page(pageIdx);
				for (int32 hop = 0; hop < kMaxDepth && node.IsDictLike(); ++hop) {
					const NkPdfVal v = DictGet(node, key);
					if (!v.IsNull())
						return v;
					node = DictGet(node, "Parent");
				}
				return NkPdfVal();
			}

			bool NkPdfDoc::PageMediaBox(int32 i, double *x0, double *y0, double *x1, double *y1) const {
				const NkPdfVal mb = Inherited(i, "MediaBox");
				if (mb.kind != NK_PDF_ARRAY || mb.b < 4)
					return false;
				double v[4];
				for (int32 k = 0; k < 4; ++k)
					v[k] = Num(ArrayAt(mb, k));
				// Les coins peuvent etre donnes dans n'importe quel ordre.
				const double ax = v[0] < v[2] ? v[0] : v[2], bx = v[0] < v[2] ? v[2] : v[0];
				const double ay = v[1] < v[3] ? v[1] : v[3], by = v[1] < v[3] ? v[3] : v[1];
				if (bx - ax <= 0.0 || by - ay <= 0.0)
					return false;
				if (x0)
					*x0 = ax;
				if (y0)
					*y0 = ay;
				if (x1)
					*x1 = bx;
				if (y1)
					*y1 = by;
				return true;
			}

			int32 NkPdfDoc::PageRotate(int32 i) const {
				const NkPdfVal r = Inherited(i, "Rotate");
				if (!r.IsNum())
					return 0;
				int32 d = static_cast<int32>(r.num) % 360;
				if (d < 0)
					d += 360;
				return (d / 90) * 90; // normalise sur 0/90/180/270
			}

		} // namespace pdf
	} // namespace media
} // namespace nkentseu
