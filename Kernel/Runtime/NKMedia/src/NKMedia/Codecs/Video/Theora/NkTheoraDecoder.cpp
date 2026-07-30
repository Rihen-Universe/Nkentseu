// =============================================================================
// NKMedia/Codecs/Video/Theora/NkTheoraDecoder.cpp
// -----------------------------------------------------------------------------
// Implémentation FROM SCRATCH du décodeur Theora (voir NkTheoraDecoder.h).
// Écrit intégralement depuis la spécification Theora I (Xiph.Org, 2017).
// Aucun code libtheora/ffmpeg copié — références de section indiquées en
// commentaires. Zero-STL : NkVector / NKMemory uniquement.
// =============================================================================
#include "NKMedia/Codecs/Video/Theora/NkTheoraDecoder.h"
#include "NKMedia/Codecs/Video/Theora/NkTheoraTables.h"

#include "NKMemory/NkAllocator.h"

#include <new> // placement new

namespace nkentseu {
	namespace media {

		using namespace theora;

		namespace {

			static inline int32 Abs32(int32 x) { return x < 0 ? -x : x; }

			// Tronque une valeur à sa représentation signée 16 bits (rejet des bits
			// de poids fort, arithmétique non saturée — cf. §7.9.3).
			static inline int32 Trunc16(int32 x) { return (int32)(int16)(uint16)((uint32)x & 0xFFFFu); }

			static inline int32 Clamp255(int32 x) { return x < 0 ? 0 : (x > 255 ? 255 : x); }

			// ilog(a) : nombre de bits pour stocker l'entier positif a (§ notation).
			static inline int32 Ilog(int32 a) {
				int32 n = 0;
				while (a > 0) {
					++n;
					a >>= 1;
				}
				return n;
			}

			// ---- Lecteur de bits BIG-ENDIAN (Theora = oggpackB, MSB en premier) -----
			struct BitReader {
					const uint8 *data = nullptr;
					usize size = 0;
					usize bytePos = 0;
					int32 bitPos = 0; // 0 = MSB de l'octet courant
					bool overrun = false;

					BitReader() = default;
					BitReader(const uint8 *d, usize s) : data(d), size(s) {}

					int32 ReadBit() {
						if (bytePos >= size) {
							overrun = true;
							return 0;
						}
						int32 bit = (data[bytePos] >> (7 - bitPos)) & 1;
						if (++bitPos == 8) {
							bitPos = 0;
							++bytePos;
						}
						return bit;
					}

					uint32 ReadBits(int32 n) {
						uint32 v = 0;
						for (int32 i = 0; i < n; ++i)
							v = (v << 1) | (uint32)ReadBit();
						return v;
					}
				};

			// ---- Arbre de Huffman (tokens DCT + MV) ---------------------------------
			struct HuffNode {
					int32 child0 = -1;
					int32 child1 = -1;
					int32 token = 0;
					bool isLeaf = false; // ⚠️ drapeau explicite : les valeurs de token
										 // peuvent être NÉGATIVES (composantes MV), donc on
										 // ne peut pas utiliser token<0 comme sentinelle.
			};
			struct HuffTree {
					NkVector<HuffNode> nodes;

					int32 NewNode() {
						HuffNode n;
						nodes.PushBack(n);
						return (int32)nodes.Size() - 1;
					}
					// Décode un token depuis le lecteur (bit à bit). Renvoie INT32_MIN
					// (via found=false) si l'arbre est invalide.
					int32 Decode(BitReader &br) const {
						if (nodes.Size() == 0)
							return -0x40000000;
						int32 idx = 0;
						int32 guard = 0;
						while (!nodes[(usize)idx].isLeaf) {
							int32 bit = br.ReadBit();
							idx = bit ? nodes[(usize)idx].child1 : nodes[(usize)idx].child0;
							if (idx < 0 || ++guard > 64)
								return -0x40000000;
						}
						return nodes[(usize)idx].token;
					}
					// Ajoute un code binaire (chaîne "010...") → valeur (pour la table MV).
					// ⚠️ On manipule des INDICES (pas des références) : NewNode() fait un
					// PushBack qui peut RÉALLOUER le vecteur et invalider toute référence.
					void AddCode(const char *bits, int32 value) {
						if (nodes.Size() == 0)
							NewNode();
						int32 idx = 0;
						for (const char *p = bits; *p; ++p) {
							int32 b = (*p == '1') ? 1 : 0;
							int32 child = b ? nodes[(usize)idx].child1 : nodes[(usize)idx].child0;
							if (child < 0) {
								child = NewNode(); // peut réallouer → réindexer ci-dessous
								if (b)
									nodes[(usize)idx].child1 = child;
								else
									nodes[(usize)idx].child0 = child;
							}
							idx = child;
						}
						nodes[(usize)idx].token = value;
						nodes[(usize)idx].isLeaf = true;
					}
			};

			// Construction récursive d'un arbre de tokens DCT depuis le setup header
			// (§6.4.4). Renvoie l'index du nœud créé, -1 si invalide.
			static int32 BuildTokenTree(HuffTree &t, BitReader &br, int32 depth) {
				if (depth > 32 || br.overrun)
					return -1;
				int32 node = t.NewNode();
				int32 isLeaf = br.ReadBit();
				if (isLeaf) {
					t.nodes[(usize)node].token = (int32)br.ReadBits(5);
					t.nodes[(usize)node].isLeaf = true;
					return node;
				}
				int32 c0 = BuildTokenTree(t, br, depth + 1);
				if (c0 < 0)
					return -1;
				int32 c1 = BuildTokenTree(t, br, depth + 1);
				if (c1 < 0)
					return -1;
				t.nodes[(usize)node].child0 = c0;
				t.nodes[(usize)node].child1 = c1;
				return node;
			}

			// ---- Ogg : extraction des paquets du flux logique Theora ----------------
			static inline uint32 U32LE(const uint8 *p) {
				return (uint32)p[0] | ((uint32)p[1] << 8) | ((uint32)p[2] << 16) | ((uint32)p[3] << 24);
			}

			struct OggPageView {
					uint8 headerType = 0;
					uint32 serial = 0;
					usize payloadOffset = 0;
					usize payloadSize = 0;
					uint8 nsegs = 0;
					const uint8 *segTable = nullptr;
					usize next = 0;
			};

			static bool OggParsePage(const uint8 *base, usize size, usize pos, OggPageView &pg) {
				if (pos + 27 > size)
					return false;
				const uint8 *h = base + pos;
				if (h[0] != 'O' || h[1] != 'g' || h[2] != 'g' || h[3] != 'S' || h[4] != 0)
					return false;
				pg.headerType = h[5];
				pg.serial = U32LE(h + 14);
				pg.nsegs = h[26];
				if (pos + 27 + pg.nsegs > size)
					return false;
				pg.segTable = h + 27;
				pg.payloadOffset = pos + 27 + pg.nsegs;
				usize payload = 0;
				for (uint8 i = 0; i < pg.nsegs; ++i)
					payload += pg.segTable[i];
				if (pg.payloadOffset + payload > size)
					return false;
				pg.payloadSize = payload;
				pg.next = pg.payloadOffset + payload;
				return true;
			}

			// Un paquet Theora reconstitué (peut couvrir plusieurs pages).
			struct Packet {
					NkVector<uint8> bytes;
			};

			// Table 7.23 : codes de Huffman des composantes de vecteur de mouvement.
			// {code binaire, valeur}. Transcrit à la main depuis la spec.
			struct MvCode {
					const char *code;
					int32 value;
			};
			static const MvCode kMvCodes[63] = {
				{"000", 0},		 {"001", 1},		{"0110", 2},	  {"1000", 3},
				{"101000", 4},	 {"101010", 5},		{"101100", 6},	  {"101110", 7},
				{"1100000", 8},	 {"1100010", 9},	{"1100100", 10},  {"1100110", 11},
				{"1101000", 12}, {"1101010", 13},	{"1101100", 14},  {"1101110", 15},
				{"11100000", 16},{"11100010", 17},	{"11100100", 18}, {"11100110", 19},
				{"11101000", 20},{"11101010", 21},	{"11101100", 22}, {"11101110", 23},
				{"11110000", 24},{"11110010", 25},	{"11110100", 26}, {"11110110", 27},
				{"11111000", 28},{"11111010", 29},	{"11111100", 30}, {"11111110", 31},
				{"010", -1},	 {"0111", -2},		{"1001", -3},	  {"101001", -4},
				{"101011", -5},	 {"101101", -6},	{"101111", -7},	  {"1100001", -8},
				{"1100011", -9}, {"1100101", -10},	{"1100111", -11}, {"1101001", -12},
				{"1101011", -13},{"1101101", -14},	{"1101111", -15}, {"11100001", -16},
				{"11100011", -17},{"11100101", -18},{"11100111", -19},{"11101001", -20},
				{"11101011", -21},{"11101101", -22},{"11101111", -23},{"11110001", -24},
				{"11110011", -25},{"11110101", -26},{"11110111", -27},{"11111001", -28},
				{"11111011", -29},{"11111101", -30},{"11111111", -31},
			};
			static void BuildMvTree(HuffTree &t) {
				t.nodes.Clear();
				for (int32 i = 0; i < 63; ++i)
					t.AddCode(kMvCodes[i].code, kMvCodes[i].value);
			}

		} // namespace

		// =====================================================================
		//  Impl : état complet du décodeur
		// =====================================================================
		struct NkTheoraDecoder::Impl {
				// Paquets du flux Theora (0 = ident, 1 = comment, 2 = setup, 3.. = data).
				NkVector<Packet> packets;
				usize nextDataPacket = 3;
				int64 displayIndex = 0;

				// --- En-tête d'identification (§6.2) ---
				int32 fmbw = 0, fmbh = 0;
				int32 picW = 0, picH = 0, picX = 0, picY = 0;
				int32 pf = 0; // 0=4:2:0, 2=4:2:2, 3=4:4:4

				// --- Setup header (§6.4) ---
				int32 lflims[64] = {};
				int32 acscale[64] = {};
				int32 dcscale[64] = {};
				int32 nbms = 0;
				NkVector<uint8> bms; // nbms*64, ordre naturel
				int32 nqrs[2][3] = {};
				int32 qrsizes[2][3][64] = {};
				int32 qrbmis[2][3][64] = {};
				HuffTree hts[80];

				// Table de Huffman des composantes MV (§ Table 7.23), construite à Open.
				HuffTree mvTree;

				// --- Géométrie (calculée à Open) ---
				int32 planeBW[3] = {}, planeBH[3] = {}; // blocs par plan
				int32 planePW[3] = {}, planePH[3] = {}; // pixels par plan
				int32 nbs = 0;							// total blocs
				int32 nmbs = 0;							// total macroblocs
				int32 nlbs = 0;							// blocs luma
				int32 planeBlockBase[3] = {};			// 1er index de bloc (coded) du plan

				// Cartes ordre codé <-> raster (par bloc global).
				NkVector<int32> codedPlane; // plan de chaque bloc codé
				NkVector<int32> codedBx;	// raster bx (dans le plan)
				NkVector<int32> codedBy;	// raster by (dans le plan)
				NkVector<int32> codedMb;	// macrobloc du bloc (index MB codé)
				// rasterToCoded[plane] : bh*bw → index bloc global codé.
				NkVector<int32> rasterToCoded[3];
				// macrobloc : raster (mby*fmbw+mbx) → index MB codé.
				NkVector<int32> mbRasterToCoded;
				NkVector<int32> mbCodedToRaster;

				// --- Trames de référence (stockées BAS-EN-HAUT, sans bordure) ---
				NkVector<uint8> prevY, prevCb, prevCr;
				NkVector<uint8> goldY, goldCb, goldCr;
				bool haveRef = false;

				// --- Buffers de travail par trame ---
				NkVector<uint8> curY, curCb, curCr;
				NkVector<int16> coeffs;	 // nbs*64 (ordre zig-zag)
				NkVector<uint8> tis;	 // token index par bloc
				NkVector<uint8> ncoeffs; // compte de coefficients
				NkVector<uint8> bcoded;	 // drapeau codé par bloc
				NkVector<uint8> qiis;	 // qii par bloc
				NkVector<uint8> mbmodes; // mode par MB (index codé)
				NkVector<int16> mvects;	 // 2*nbs (x,y demi-pel)

				bool BuildGeometry();
				bool ParseIdent(BitReader &br);
				bool ParseSetup(BitReader &br);
				bool ParseQuant(BitReader &br);
				void ComputeQMat(int32 qti, int32 pli, int32 qi, int32 out[64]) const;

				bool DecodeFrame(NkTheoraFrame &out, NkString *err);
				void DecodeCodedFlags(BitReader &br, int32 ftype);
				void DecodeModes(BitReader &br, int32 ftype);
				void DecodeMVs(BitReader &br);
				void DecodeBlockQis(BitReader &br, int32 nqis);
				void DecodeCoefficients(BitReader &br);
				void UndoDcPrediction();
				void Reconstruct(int32 qi0, const int32 *qis, int32 ftype);
				void LoopFilter(int32 qi0);

				// Aides RLE (§7.2).
				void DecodeLongRun(BitReader &br, int32 nbits, NkVector<uint8> &outBits);
				void DecodeShortRun(BitReader &br, int32 nbits, NkVector<uint8> &outBits);

				int32 DecodeMvComp(BitReader &br, int32 mvmode);
		};

		// ------------------------------------------------------------------
		//  Construction de la géométrie (blocs / super blocs / macroblocs)
		// ------------------------------------------------------------------
		bool NkTheoraDecoder::Impl::BuildGeometry() {
			const int32 lumaBW = fmbw * 2;
			const int32 lumaBH = fmbh * 2;
			planeBW[0] = lumaBW;
			planeBH[0] = lumaBH;
			planePW[0] = fmbw * 16;
			planePH[0] = fmbh * 16;

			int32 chromaBW, chromaBH, chromaPW, chromaPH;
			if (pf == 0) { // 4:2:0
				chromaBW = fmbw;
				chromaBH = fmbh;
				chromaPW = fmbw * 8;
				chromaPH = fmbh * 8;
			} else if (pf == 2) { // 4:2:2
				chromaBW = fmbw;
				chromaBH = fmbh * 2;
				chromaPW = fmbw * 8;
				chromaPH = fmbh * 16;
			} else { // 4:4:4
				chromaBW = fmbw * 2;
				chromaBH = fmbh * 2;
				chromaPW = fmbw * 16;
				chromaPH = fmbh * 16;
			}
			for (int32 p = 1; p < 3; ++p) {
				planeBW[p] = chromaBW;
				planeBH[p] = chromaBH;
				planePW[p] = chromaPW;
				planePH[p] = chromaPH;
			}

			nmbs = fmbw * fmbh;
			nlbs = nmbs * 4;
			nbs = 0;
			for (int32 p = 0; p < 3; ++p) {
				planeBlockBase[p] = nbs;
				nbs += planeBW[p] * planeBH[p];
			}

			// Construction de l'ordre codé des macroblocs (Hilbert 2x2 dans chaque
			// super bloc luma).
			mbRasterToCoded.Resize((usize)nmbs);
			mbCodedToRaster.Resize((usize)nmbs);
			{
				const int32 sbCols = (lumaBW + 3) / 4;
				const int32 sbRows = (lumaBH + 3) / 4;
				int32 mbIdx = 0;
				for (int32 sby = 0; sby < sbRows; ++sby) {
					for (int32 sbx = 0; sbx < sbCols; ++sbx) {
						for (int32 h = 0; h < 4; ++h) {
							int32 mbx = sbx * 2 + kMbHilbertLx[h];
							int32 mby = sby * 2 + kMbHilbertLy[h];
							if (mbx < fmbw && mby < fmbh) {
								int32 raster = mby * fmbw + mbx;
								mbRasterToCoded[(usize)raster] = mbIdx;
								mbCodedToRaster[(usize)mbIdx] = raster;
								++mbIdx;
							}
						}
					}
				}
			}

			// Construction de l'ordre codé des blocs (Hilbert 4x4 par super bloc,
			// plans Y→Cb→Cr).
			codedPlane.Resize((usize)nbs);
			codedBx.Resize((usize)nbs);
			codedBy.Resize((usize)nbs);
			codedMb.Resize((usize)nbs);
			for (int32 p = 0; p < 3; ++p)
				rasterToCoded[p].Resize((usize)(planeBW[p] * planeBH[p]));

			int32 bi = 0;
			for (int32 p = 0; p < 3; ++p) {
				const int32 bw = planeBW[p];
				const int32 bh = planeBH[p];
				const int32 sbCols = (bw + 3) / 4;
				const int32 sbRows = (bh + 3) / 4;
				for (int32 sby = 0; sby < sbRows; ++sby) {
					for (int32 sbx = 0; sbx < sbCols; ++sbx) {
						for (int32 h = 0; h < 16; ++h) {
							int32 lx = kHilbertLx[h];
							int32 ly = kHilbertLy[h];
							int32 bx = sbx * 4 + lx;
							int32 by = sby * 4 + ly;
							if (bx < bw && by < bh) {
								codedPlane[(usize)bi] = p;
								codedBx[(usize)bi] = bx;
								codedBy[(usize)bi] = by;
								rasterToCoded[p][(usize)(by * bw + bx)] = bi;
								// Macrobloc de ce bloc.
								int32 mbx, mby;
								if (p == 0) {
									mbx = bx / 2;
									mby = by / 2;
								} else if (pf == 0) {
									mbx = bx;
									mby = by;
								} else if (pf == 2) {
									mbx = bx;
									mby = by / 2;
								} else {
									mbx = bx / 2;
									mby = by / 2;
								}
								codedMb[(usize)bi] = mbRasterToCoded[(usize)(mby * fmbw + mbx)];
								++bi;
							}
						}
					}
				}
			}
			return bi == nbs;
		}

		// ------------------------------------------------------------------
		//  En-tête d'identification (§6.2)
		// ------------------------------------------------------------------
		bool NkTheoraDecoder::Impl::ParseIdent(BitReader &br) {
			// Common header : type + "theora" déjà lus par l'appelant.
			int32 vmaj = (int32)br.ReadBits(8);
			int32 vmin = (int32)br.ReadBits(8);
			(void)br.ReadBits(8); // vrev
			if (vmaj != 3 || vmin != 2)
				return false;
			fmbw = (int32)br.ReadBits(16);
			fmbh = (int32)br.ReadBits(16);
			if (fmbw <= 0 || fmbh <= 0)
				return false;
			picW = (int32)br.ReadBits(24);
			picH = (int32)br.ReadBits(24);
			picX = (int32)br.ReadBits(8);
			picY = (int32)br.ReadBits(8);
			(void)br.ReadBits(32); // FRN
			(void)br.ReadBits(32); // FRD
			(void)br.ReadBits(24); // PARN
			(void)br.ReadBits(24); // PARD
			(void)br.ReadBits(8);  // CS
			(void)br.ReadBits(24); // NOMBR
			(void)br.ReadBits(6);  // QUAL
			(void)br.ReadBits(5);  // KFGSHIFT
			pf = (int32)br.ReadBits(2);
			if (pf == 1)
				return false; // réservé
			(void)br.ReadBits(3); // réservé
			return true;
		}

		// ------------------------------------------------------------------
		//  Paramètres de quantification (§6.4.2)
		// ------------------------------------------------------------------
		bool NkTheoraDecoder::Impl::ParseQuant(BitReader &br) {
			int32 nbits = (int32)br.ReadBits(4) + 1;
			for (int32 qi = 0; qi < 64; ++qi)
				acscale[qi] = (int32)br.ReadBits(nbits);
			nbits = (int32)br.ReadBits(4) + 1;
			for (int32 qi = 0; qi < 64; ++qi)
				dcscale[qi] = (int32)br.ReadBits(nbits);
			nbms = (int32)br.ReadBits(9) + 1;
			if (nbms > 384)
				return false;
			bms.Resize((usize)(nbms * 64));
			for (int32 bmi = 0; bmi < nbms; ++bmi)
				for (int32 ci = 0; ci < 64; ++ci)
					bms[(usize)(bmi * 64 + ci)] = (uint8)br.ReadBits(8);

			for (int32 qti = 0; qti < 2; ++qti) {
				for (int32 pli = 0; pli < 3; ++pli) {
					int32 newqr = 1;
					if (qti > 0 || pli > 0)
						newqr = (int32)br.ReadBits(1);
					if (newqr == 0) {
						// Copie d'un ensemble précédent.
						int32 rpqr = 0;
						if (qti > 0)
							rpqr = (int32)br.ReadBits(1);
						int32 qtj, plj;
						if (rpqr == 1) {
							qtj = qti - 1;
							plj = pli;
						} else {
							int32 lin = 3 * qti + pli - 1;
							qtj = lin / 3;
							plj = (pli + 2) % 3;
						}
						nqrs[qti][pli] = nqrs[qtj][plj];
						for (int32 k = 0; k < 64; ++k) {
							qrsizes[qti][pli][k] = qrsizes[qtj][plj][k];
							qrbmis[qti][pli][k] = qrbmis[qtj][plj][k];
						}
					} else {
						int32 qri = 0;
						int32 qi = 0;
						qrbmis[qti][pli][qri] = (int32)br.ReadBits(Ilog(nbms - 1));
						if (qrbmis[qti][pli][qri] >= nbms)
							return false;
						for (;;) {
							qrsizes[qti][pli][qri] = (int32)br.ReadBits(Ilog(62 - qi)) + 1;
							qi += qrsizes[qti][pli][qri];
							++qri;
							qrbmis[qti][pli][qri] = (int32)br.ReadBits(Ilog(nbms - 1));
							if (qi >= 63)
								break;
						}
						if (qi > 63)
							return false;
						nqrs[qti][pli] = qri;
					}
				}
			}
			return true;
		}

		// ------------------------------------------------------------------
		//  Setup header complet (§6.4.5)
		// ------------------------------------------------------------------
		bool NkTheoraDecoder::Impl::ParseSetup(BitReader &br) {
			// §6.4.1 : table des limites de filtre de boucle.
			int32 nbits = (int32)br.ReadBits(3);
			for (int32 qi = 0; qi < 64; ++qi)
				lflims[qi] = (int32)br.ReadBits(nbits);
			// §6.4.2 : quantification.
			if (!ParseQuant(br))
				return false;
			// §6.4.4 : 80 arbres de Huffman de tokens DCT.
			for (int32 hti = 0; hti < 80; ++hti) {
				hts[hti].nodes.Clear();
				if (BuildTokenTree(hts[hti], br, 0) < 0)
					return false;
			}
			return !br.overrun;
		}

		// ------------------------------------------------------------------
		//  Calcul d'une matrice de quantification (§6.4.3)
		// ------------------------------------------------------------------
		void NkTheoraDecoder::Impl::ComputeQMat(int32 qti, int32 pli, int32 qi, int32 out[64]) const {
			int32 qiStart = 0;
			int32 qri = 0;
			const int32 n = nqrs[qti][pli];
			for (qri = 0; qri < n - 1; ++qri) {
				if (qiStart + qrsizes[qti][pli][qri] >= qi)
					break;
				qiStart += qrsizes[qti][pli][qri];
			}
			int32 qsz = qrsizes[qti][pli][qri];
			int32 qiEnd = qiStart + qsz;
			int32 bmi = qrbmis[qti][pli][qri];
			int32 bmj = qrbmis[qti][pli][qri + 1];
			for (int32 ci = 0; ci < 64; ++ci) {
				int32 bmiVal = (int32)bms[(usize)(bmi * 64 + ci)];
				int32 bmjVal = (int32)bms[(usize)(bmj * 64 + ci)];
				int32 bm = (2 * (qiEnd - qi) * bmiVal + 2 * (qi - qiStart) * bmjVal + qsz) / (2 * qsz);
				int32 qmin = QMin(qti, ci);
				int32 qscale = (ci == 0) ? dcscale[qi] : acscale[qi];
				int32 v = (qscale * bm / 100) * 4;
				if (v > 4096)
					v = 4096;
				if (v < qmin)
					v = qmin;
				out[ci] = v;
			}
		}

		// ------------------------------------------------------------------
		//  RLE longue portée (§7.2.1)
		// ------------------------------------------------------------------
		void NkTheoraDecoder::Impl::DecodeLongRun(BitReader &br, int32 nbits, NkVector<uint8> &outBits) {
			outBits.Clear();
			if (nbits <= 0)
				return;
			int32 len = 0;
			int32 bit = br.ReadBit();
			for (;;) {
				// Préfixe de Huffman (table 7.7) : compte des '1' menants (max 6).
				int32 c = 0;
				while (c < 6) {
					if (br.ReadBit() == 0)
						break;
					++c;
				}
				int32 rstart, rbits;
				switch (c) {
					case 0: rstart = 1; rbits = 0; break;
					case 1: rstart = 2; rbits = 1; break;
					case 2: rstart = 4; rbits = 1; break;
					case 3: rstart = 6; rbits = 2; break;
					case 4: rstart = 10; rbits = 3; break;
					case 5: rstart = 18; rbits = 4; break;
					default: rstart = 34; rbits = 12; break;
				}
				int32 roffs = (int32)br.ReadBits(rbits);
				int32 rlen = rstart + roffs;
				for (int32 i = 0; i < rlen && len < nbits; ++i) {
					outBits.PushBack((uint8)bit);
					++len;
				}
				if (len >= nbits)
					return;
				if (rlen == 4129)
					bit = br.ReadBit();
				else
					bit = 1 - bit;
				if (br.overrun)
					return;
			}
		}

		// ------------------------------------------------------------------
		//  RLE courte portée (§7.2.2)
		// ------------------------------------------------------------------
		void NkTheoraDecoder::Impl::DecodeShortRun(BitReader &br, int32 nbits, NkVector<uint8> &outBits) {
			outBits.Clear();
			if (nbits <= 0)
				return;
			int32 len = 0;
			int32 bit = br.ReadBit();
			for (;;) {
				int32 c = 0;
				while (c < 5) {
					if (br.ReadBit() == 0)
						break;
					++c;
				}
				int32 rstart, rbits;
				switch (c) {
					case 0: rstart = 1; rbits = 1; break;
					case 1: rstart = 3; rbits = 1; break;
					case 2: rstart = 5; rbits = 1; break;
					case 3: rstart = 7; rbits = 2; break;
					case 4: rstart = 11; rbits = 2; break;
					default: rstart = 15; rbits = 4; break;
				}
				int32 roffs = (int32)br.ReadBits(rbits);
				int32 rlen = rstart + roffs;
				for (int32 i = 0; i < rlen && len < nbits; ++i) {
					outBits.PushBack((uint8)bit);
					++len;
				}
				if (len >= nbits)
					return;
				bit = 1 - bit;
				if (br.overrun)
					return;
			}
		}

		// ------------------------------------------------------------------
		//  Drapeaux de blocs codés (§7.3)
		// ------------------------------------------------------------------
		void NkTheoraDecoder::Impl::DecodeCodedFlags(BitReader &br, int32 ftype) {
			bcoded.Resize((usize)nbs);
			if (ftype == 0) {
				for (int32 i = 0; i < nbs; ++i)
					bcoded[(usize)i] = 1;
				return;
			}
			// Nombre de super blocs par plan + total.
			int32 nsbs = 0;
			int32 sbPlaneBase[3];
			int32 sbCols[3], sbRows[3];
			for (int32 p = 0; p < 3; ++p) {
				sbCols[p] = (planeBW[p] + 3) / 4;
				sbRows[p] = (planeBH[p] + 3) / 4;
				sbPlaneBase[p] = nsbs;
				nsbs += sbCols[p] * sbRows[p];
			}
			NkVector<uint8> sbpcoded, sbfcoded;
			NkVector<uint8> bits;
			DecodeLongRun(br, nsbs, bits);
			sbpcoded.Resize((usize)nsbs);
			for (int32 i = 0; i < nsbs; ++i)
				sbpcoded[(usize)i] = (i < (int32)bits.Size()) ? bits[(usize)i] : 0;

			int32 nFully = 0;
			for (int32 i = 0; i < nsbs; ++i)
				if (sbpcoded[(usize)i] == 0)
					++nFully;
			DecodeLongRun(br, nFully, bits);
			sbfcoded.Resize((usize)nsbs);
			{
				int32 k = 0;
				for (int32 i = 0; i < nsbs; ++i) {
					if (sbpcoded[(usize)i] == 0)
						sbfcoded[(usize)i] = (k < (int32)bits.Size()) ? bits[(usize)k++] : 0;
					else
						sbfcoded[(usize)i] = 0;
				}
			}

			// Nombre de blocs dans les super blocs partiellement codés.
			// On associe chaque bloc codé à son super bloc.
			// Calcul du super bloc de chaque bloc (par plan, en coordonnées raster).
			auto superBlockOfBlock = [&](int32 pl, int32 bx, int32 by) -> int32 {
				int32 sbx = bx / 4;
				int32 sby = by / 4;
				return sbPlaneBase[pl] + sby * sbCols[pl] + sbx;
			};

			int32 nPartialBlocks = 0;
			for (int32 b = 0; b < nbs; ++b) {
				int32 pl = codedPlane[(usize)b];
				int32 sbi = superBlockOfBlock(pl, codedBx[(usize)b], codedBy[(usize)b]);
				if (sbpcoded[(usize)sbi] != 0)
					++nPartialBlocks;
			}
			DecodeShortRun(br, nPartialBlocks, bits);

			int32 k = 0;
			for (int32 b = 0; b < nbs; ++b) {
				int32 pl = codedPlane[(usize)b];
				int32 sbi = superBlockOfBlock(pl, codedBx[(usize)b], codedBy[(usize)b]);
				if (sbpcoded[(usize)sbi] == 0)
					bcoded[(usize)b] = sbfcoded[(usize)sbi];
				else
					bcoded[(usize)b] = (k < (int32)bits.Size()) ? bits[(usize)k++] : 0;
			}
		}

		// ------------------------------------------------------------------
		//  Modes de macrobloc (§7.4)
		// ------------------------------------------------------------------
		void NkTheoraDecoder::Impl::DecodeModes(BitReader &br, int32 ftype) {
			mbmodes.Resize((usize)nmbs);
			if (ftype == 0) {
				for (int32 i = 0; i < nmbs; ++i)
					mbmodes[(usize)i] = 1; // INTRA
				return;
			}
			int32 mscheme = (int32)br.ReadBits(3);
			int32 malphabet[8] = {};
			if (mscheme == 0) {
				for (int32 mode = 0; mode < 8; ++mode) {
					int32 mi = (int32)br.ReadBits(3);
					malphabet[mi] = mode;
				}
			} else if (mscheme != 7) {
				for (int32 mi = 0; mi < 8; ++mi)
					malphabet[mi] = kModeScheme[mscheme - 1][mi];
			}

			for (int32 mbc = 0; mbc < nmbs; ++mbc) {
				// Un mode est stocké seulement si au moins un bloc luma du MB est codé.
				int32 mbRaster = mbCodedToRaster[(usize)mbc];
				int32 mbx = mbRaster % fmbw;
				int32 mby = mbRaster / fmbw;
				bool anyLuma = false;
				for (int32 dy = 0; dy < 2 && !anyLuma; ++dy)
					for (int32 dx = 0; dx < 2; ++dx) {
						int32 bx = mbx * 2 + dx;
						int32 by = mby * 2 + dy;
						int32 b = rasterToCoded[0][(usize)(by * planeBW[0] + bx)];
						if (bcoded[(usize)b]) {
							anyLuma = true;
							break;
						}
					}
				if (!anyLuma) {
					mbmodes[(usize)mbc] = 0; // INTER NOMV
					continue;
				}
				if (mscheme == 7) {
					mbmodes[(usize)mbc] = (uint8)br.ReadBits(3);
				} else {
					// Huffman table 7.19 : compte des '1' menants (max 7).
					int32 c = 0;
					while (c < 7) {
						if (br.ReadBit() == 0)
							break;
						++c;
					}
					mbmodes[(usize)mbc] = (uint8)malphabet[c];
				}
			}
		}

		// ------------------------------------------------------------------
		//  Composante MV (§7.5.1) + décodage MV par MB (§7.5.2)
		// ------------------------------------------------------------------
		int32 NkTheoraDecoder::Impl::DecodeMvComp(BitReader &br, int32 mvmode) {
			if (mvmode == 0)
				return mvTree.Decode(br); // §7.5.1 : code de Huffman (table 7.23)
			// Sinon : magnitude 5 bits + bit de signe (deux représentations de zéro).
			int32 mag = (int32)br.ReadBits(5);
			int32 sign = (int32)br.ReadBits(1);
			return sign ? -mag : mag;
		}

		void NkTheoraDecoder::Impl::DecodeMVs(BitReader &br) {
			mvects.Resize((usize)(nbs * 2));
			for (int32 i = 0; i < nbs * 2; ++i)
				mvects[(usize)i] = 0;

			int32 last1x = 0, last1y = 0, last2x = 0, last2y = 0;
			int32 mvmode = (int32)br.ReadBits(1);

			// Blocs luma d'un MB en raster (A=LL,B=LR,C=UL,D=UR).
			for (int32 mbc = 0; mbc < nmbs; ++mbc) {
				int32 mode = mbmodes[(usize)mbc];
				int32 mbRaster = mbCodedToRaster[(usize)mbc];
				int32 mbx = mbRaster % fmbw;
				int32 mby = mbRaster / fmbw;
				int32 lumaA = rasterToCoded[0][(usize)((mby * 2 + 0) * planeBW[0] + (mbx * 2 + 0))];
				int32 lumaB = rasterToCoded[0][(usize)((mby * 2 + 0) * planeBW[0] + (mbx * 2 + 1))];
				int32 lumaC = rasterToCoded[0][(usize)((mby * 2 + 1) * planeBW[0] + (mbx * 2 + 0))];
				int32 lumaD = rasterToCoded[0][(usize)((mby * 2 + 1) * planeBW[0] + (mbx * 2 + 1))];

				int32 mvx = 0, mvy = 0;
				if (mode == 7) { // INTER MV FOUR
					int32 lb[4] = {lumaA, lumaB, lumaC, lumaD};
					int32 vx[4] = {0, 0, 0, 0}, vy[4] = {0, 0, 0, 0};
					for (int32 j = 0; j < 4; ++j) {
						if (bcoded[(usize)lb[j]]) {
							vx[j] = DecodeMvComp(br, mvmode);
							vy[j] = DecodeMvComp(br, mvmode);
							mvx = vx[j];
							mvy = vy[j]; // dernier bloc luma codé (raster)
						}
						mvects[(usize)(lb[j] * 2 + 0)] = (int16)vx[j];
						mvects[(usize)(lb[j] * 2 + 1)] = (int16)vy[j];
					}
					// Chroma dérivé.
					auto roundDiv = [](int32 num, int32 den) -> int32 {
						if (num >= 0)
							return (num + den / 2) / den;
						return -(((-num) + den / 2) / den);
					};
					if (pf == 0) { // 4:2:0 : 1 bloc Cb + 1 Cr
						// §7.5.2 : MV chroma = moyenne arrondie des 4 MV luma. Le
						// sous-échantillonnage (÷2) est appliqué en reconstruction.
						// ⚠️ NOTE : petit écart résiduel vs le décodeur ffmpeg sur ce
						// mode (INTER_MV_FOUR) — arrondi exact de la MV chroma à revoir.
						int32 cx = roundDiv(vx[0] + vx[1] + vx[2] + vx[3], 4);
						int32 cy = roundDiv(vy[0] + vy[1] + vy[2] + vy[3], 4);
						int32 cbB = rasterToCoded[1][(usize)(mby * planeBW[1] + mbx)];
						int32 crB = rasterToCoded[2][(usize)(mby * planeBW[2] + mbx)];
						mvects[(usize)(cbB * 2 + 0)] = (int16)cx;
						mvects[(usize)(cbB * 2 + 1)] = (int16)cy;
						mvects[(usize)(crB * 2 + 0)] = (int16)cx;
						mvects[(usize)(crB * 2 + 1)] = (int16)cy;
					} else if (pf == 2) { // 4:2:2
						int32 bx0 = roundDiv(vx[0] + vx[1], 2), by0 = roundDiv(vy[0] + vy[1], 2);
						int32 bx1 = roundDiv(vx[2] + vx[3], 2), by1 = roundDiv(vy[2] + vy[3], 2);
						int32 cbBot = rasterToCoded[1][(usize)((mby * 2 + 0) * planeBW[1] + mbx)];
						int32 cbTop = rasterToCoded[1][(usize)((mby * 2 + 1) * planeBW[1] + mbx)];
						int32 crBot = rasterToCoded[2][(usize)((mby * 2 + 0) * planeBW[2] + mbx)];
						int32 crTop = rasterToCoded[2][(usize)((mby * 2 + 1) * planeBW[2] + mbx)];
						mvects[(usize)(cbBot * 2)] = (int16)bx0;
						mvects[(usize)(cbBot * 2 + 1)] = (int16)by0;
						mvects[(usize)(crBot * 2)] = (int16)bx0;
						mvects[(usize)(crBot * 2 + 1)] = (int16)by0;
						mvects[(usize)(cbTop * 2)] = (int16)bx1;
						mvects[(usize)(cbTop * 2 + 1)] = (int16)by1;
						mvects[(usize)(crTop * 2)] = (int16)bx1;
						mvects[(usize)(crTop * 2 + 1)] = (int16)by1;
					} else { // 4:4:4 : recopie A,B,C,D
						int32 planeMap[2] = {1, 2};
						for (int32 pp = 0; pp < 2; ++pp) {
							int32 pl = planeMap[pp];
							int32 e = rasterToCoded[pl][(usize)((mby * 2 + 0) * planeBW[pl] + (mbx * 2 + 0))];
							int32 f = rasterToCoded[pl][(usize)((mby * 2 + 0) * planeBW[pl] + (mbx * 2 + 1))];
							int32 g = rasterToCoded[pl][(usize)((mby * 2 + 1) * planeBW[pl] + (mbx * 2 + 0))];
							int32 hh = rasterToCoded[pl][(usize)((mby * 2 + 1) * planeBW[pl] + (mbx * 2 + 1))];
							mvects[(usize)(e * 2)] = (int16)vx[0];
							mvects[(usize)(e * 2 + 1)] = (int16)vy[0];
							mvects[(usize)(f * 2)] = (int16)vx[1];
							mvects[(usize)(f * 2 + 1)] = (int16)vy[1];
							mvects[(usize)(g * 2)] = (int16)vx[2];
							mvects[(usize)(g * 2 + 1)] = (int16)vy[2];
							mvects[(usize)(hh * 2)] = (int16)vx[3];
							mvects[(usize)(hh * 2 + 1)] = (int16)vy[3];
						}
					}
					last2x = last1x;
					last2y = last1y;
					last1x = mvx;
					last1y = mvy;
					continue;
				} else if (mode == 6) { // INTER GOLDEN MV
					mvx = DecodeMvComp(br, mvmode);
					mvy = DecodeMvComp(br, mvmode);
				} else if (mode == 4) { // INTER MV LAST2
					mvx = last2x;
					mvy = last2y;
					last2x = last1x;
					last2y = last1y;
					last1x = mvx;
					last1y = mvy;
				} else if (mode == 3) { // INTER MV LAST
					mvx = last1x;
					mvy = last1y;
				} else if (mode == 2) { // INTER MV
					mvx = DecodeMvComp(br, mvmode);
					mvy = DecodeMvComp(br, mvmode);
					last2x = last1x;
					last2y = last1y;
					last1x = mvx;
					last1y = mvy;
				} else {
					mvx = 0;
					mvy = 0;
				}
				// Modes non-FOUR : tous les blocs codés du MB reçoivent (mvx,mvy).
				// Luma :
				int32 lb[4] = {lumaA, lumaB, lumaC, lumaD};
				for (int32 j = 0; j < 4; ++j) {
					mvects[(usize)(lb[j] * 2)] = (int16)mvx;
					mvects[(usize)(lb[j] * 2 + 1)] = (int16)mvy;
				}
				// Chroma : tous les blocs chroma du MB.
				for (int32 pl = 1; pl < 3; ++pl) {
					int32 cbw = planeBW[pl];
					int32 bx0, by0, bx1, by1;
					if (pf == 0) {
						bx0 = mbx;
						by0 = mby;
						bx1 = mbx;
						by1 = mby;
					} else if (pf == 2) {
						bx0 = mbx;
						by0 = mby * 2;
						bx1 = mbx;
						by1 = mby * 2 + 1;
					} else {
						bx0 = mbx * 2;
						by0 = mby * 2;
						bx1 = mbx * 2 + 1;
						by1 = mby * 2 + 1;
					}
					int32 blist[4];
					int32 cnt = 0;
					if (pf == 0) {
						blist[cnt++] = rasterToCoded[pl][(usize)(by0 * cbw + bx0)];
					} else if (pf == 2) {
						blist[cnt++] = rasterToCoded[pl][(usize)(by0 * cbw + bx0)];
						blist[cnt++] = rasterToCoded[pl][(usize)(by1 * cbw + bx0)];
					} else {
						blist[cnt++] = rasterToCoded[pl][(usize)(by0 * cbw + bx0)];
						blist[cnt++] = rasterToCoded[pl][(usize)(by0 * cbw + bx1)];
						blist[cnt++] = rasterToCoded[pl][(usize)(by1 * cbw + bx0)];
						blist[cnt++] = rasterToCoded[pl][(usize)(by1 * cbw + bx1)];
					}
					for (int32 j = 0; j < cnt; ++j) {
						mvects[(usize)(blist[j] * 2)] = (int16)mvx;
						mvects[(usize)(blist[j] * 2 + 1)] = (int16)mvy;
					}
				}
			}
		}

		// ------------------------------------------------------------------
		//  qi par bloc (§7.6)
		// ------------------------------------------------------------------
		void NkTheoraDecoder::Impl::DecodeBlockQis(BitReader &br, int32 nqis) {
			qiis.Resize((usize)nbs);
			for (int32 i = 0; i < nbs; ++i)
				qiis[(usize)i] = 0;
			NkVector<uint8> bits;
			for (int32 qii = 0; qii < nqis - 1; ++qii) {
				int32 nb = 0;
				for (int32 b = 0; b < nbs; ++b)
					if (bcoded[(usize)b] && qiis[(usize)b] == qii)
						++nb;
				DecodeLongRun(br, nb, bits);
				int32 k = 0;
				for (int32 b = 0; b < nbs; ++b) {
					if (bcoded[(usize)b] && qiis[(usize)b] == qii) {
						uint8 v = (k < (int32)bits.Size()) ? bits[(usize)k] : 0;
						++k;
						qiis[(usize)b] = (uint8)(qiis[(usize)b] + v);
					}
				}
			}
		}

		// ------------------------------------------------------------------
		//  Coefficients DCT (§7.7)
		// ------------------------------------------------------------------
		void NkTheoraDecoder::Impl::DecodeCoefficients(BitReader &br) {
			coeffs.Resize((usize)(nbs * 64));
			for (usize i = 0; i < coeffs.Size(); ++i)
				coeffs[i] = 0;
			tis.Resize((usize)nbs);
			ncoeffs.Resize((usize)nbs);
			for (int32 i = 0; i < nbs; ++i) {
				tis[(usize)i] = 0;
				ncoeffs[(usize)i] = 0;
			}
			int32 eobs = 0;
			int32 htiL = 0, htiC = 0;

			for (int32 ti = 0; ti < 64; ++ti) {
				if (ti == 0 || ti == 1) {
					htiL = (int32)br.ReadBits(4);
					htiC = (int32)br.ReadBits(4);
				}
				int32 hg = HuffGroup(ti);
				for (int32 bi = 0; bi < nbs; ++bi) {
					if (!bcoded[(usize)bi] || tis[(usize)bi] != ti)
						continue;
					ncoeffs[(usize)bi] = (uint8)ti;
					if (eobs > 0) {
						// Bloc dans une plage EOB.
						for (int32 tj = ti; tj < 64; ++tj)
							coeffs[(usize)(bi * 64 + tj)] = 0;
						tis[(usize)bi] = 64;
						--eobs;
						continue;
					}
					int32 hti = (bi < nlbs) ? (16 * hg + htiL) : (16 * hg + htiC);
					int32 token = hts[hti].Decode(br);
					if (token < 0)
						return;
					int32 t = tis[(usize)bi];
					if (token < 7) {
						// Token EOB (§7.7.1).
						int32 run = 0;
						if (token == 0)
							run = 1;
						else if (token == 1)
							run = 2;
						else if (token == 2)
							run = 3;
						else if (token == 3)
							run = (int32)br.ReadBits(2) + 4;
						else if (token == 4)
							run = (int32)br.ReadBits(3) + 8;
						else if (token == 5)
							run = (int32)br.ReadBits(4) + 16;
						else { // token == 6
							run = (int32)br.ReadBits(12);
							if (run == 0) {
								int32 cnt = 0;
								for (int32 bj = 0; bj < nbs; ++bj)
									if (bcoded[(usize)bj] && tis[(usize)bj] < 64)
										++cnt;
								run = cnt;
							}
						}
						for (int32 tj = t; tj < 64; ++tj)
							coeffs[(usize)(bi * 64 + tj)] = 0;
						tis[(usize)bi] = 64;
						eobs = run - 1;
					} else {
						// Token de coefficient (§7.7.2).
						int32 base = bi * 64;
						int32 sign, mag, rlen;
						switch (token) {
							case 7:
								rlen = (int32)br.ReadBits(3) + 1;
								for (int32 j = 0; j < rlen; ++j)
									coeffs[(usize)(base + t + j)] = 0;
								tis[(usize)bi] = (uint8)(t + rlen);
								break;
							case 8:
								rlen = (int32)br.ReadBits(6) + 1;
								for (int32 j = 0; j < rlen; ++j)
									coeffs[(usize)(base + t + j)] = 0;
								tis[(usize)bi] = (uint8)(t + rlen);
								break;
							case 9:
								coeffs[(usize)(base + t)] = 1;
								tis[(usize)bi] = (uint8)(t + 1);
								ncoeffs[(usize)bi] = tis[(usize)bi];
								break;
							case 10:
								coeffs[(usize)(base + t)] = -1;
								tis[(usize)bi] = (uint8)(t + 1);
								ncoeffs[(usize)bi] = tis[(usize)bi];
								break;
							case 11:
								coeffs[(usize)(base + t)] = 2;
								tis[(usize)bi] = (uint8)(t + 1);
								ncoeffs[(usize)bi] = tis[(usize)bi];
								break;
							case 12:
								coeffs[(usize)(base + t)] = -2;
								tis[(usize)bi] = (uint8)(t + 1);
								ncoeffs[(usize)bi] = tis[(usize)bi];
								break;
							case 13:
							case 14:
							case 15:
							case 16: {
								int32 val = token - 10; // 3,4,5,6
								sign = (int32)br.ReadBits(1);
								coeffs[(usize)(base + t)] = (int16)(sign ? -val : val);
								tis[(usize)bi] = (uint8)(t + 1);
								ncoeffs[(usize)bi] = tis[(usize)bi];
								break;
							}
							case 17:
								sign = (int32)br.ReadBits(1);
								mag = (int32)br.ReadBits(1) + 7;
								coeffs[(usize)(base + t)] = (int16)(sign ? -mag : mag);
								tis[(usize)bi] = (uint8)(t + 1);
								ncoeffs[(usize)bi] = tis[(usize)bi];
								break;
							case 18:
								sign = (int32)br.ReadBits(1);
								mag = (int32)br.ReadBits(2) + 9;
								coeffs[(usize)(base + t)] = (int16)(sign ? -mag : mag);
								tis[(usize)bi] = (uint8)(t + 1);
								ncoeffs[(usize)bi] = tis[(usize)bi];
								break;
							case 19:
								sign = (int32)br.ReadBits(1);
								mag = (int32)br.ReadBits(3) + 13;
								coeffs[(usize)(base + t)] = (int16)(sign ? -mag : mag);
								tis[(usize)bi] = (uint8)(t + 1);
								ncoeffs[(usize)bi] = tis[(usize)bi];
								break;
							case 20:
								sign = (int32)br.ReadBits(1);
								mag = (int32)br.ReadBits(4) + 21;
								coeffs[(usize)(base + t)] = (int16)(sign ? -mag : mag);
								tis[(usize)bi] = (uint8)(t + 1);
								ncoeffs[(usize)bi] = tis[(usize)bi];
								break;
							case 21:
								sign = (int32)br.ReadBits(1);
								mag = (int32)br.ReadBits(5) + 37;
								coeffs[(usize)(base + t)] = (int16)(sign ? -mag : mag);
								tis[(usize)bi] = (uint8)(t + 1);
								ncoeffs[(usize)bi] = tis[(usize)bi];
								break;
							case 22:
								sign = (int32)br.ReadBits(1);
								mag = (int32)br.ReadBits(9) + 69;
								coeffs[(usize)(base + t)] = (int16)(sign ? -mag : mag);
								tis[(usize)bi] = (uint8)(t + 1);
								ncoeffs[(usize)bi] = tis[(usize)bi];
								break;
							case 23:
							case 24:
							case 25:
							case 26:
							case 27: {
								int32 zeros = token - 23 + 1; // 1..5
								for (int32 j = 0; j < zeros; ++j)
									coeffs[(usize)(base + t + j)] = 0;
								sign = (int32)br.ReadBits(1);
								coeffs[(usize)(base + t + zeros)] = (int16)(sign ? -1 : 1);
								tis[(usize)bi] = (uint8)(t + zeros + 1);
								ncoeffs[(usize)bi] = tis[(usize)bi];
								break;
							}
							case 28:
								sign = (int32)br.ReadBits(1);
								rlen = (int32)br.ReadBits(2) + 6;
								for (int32 j = 0; j < rlen; ++j)
									coeffs[(usize)(base + t + j)] = 0;
								coeffs[(usize)(base + t + rlen)] = (int16)(sign ? -1 : 1);
								tis[(usize)bi] = (uint8)(t + rlen + 1);
								ncoeffs[(usize)bi] = tis[(usize)bi];
								break;
							case 29:
								sign = (int32)br.ReadBits(1);
								rlen = (int32)br.ReadBits(3) + 10;
								for (int32 j = 0; j < rlen; ++j)
									coeffs[(usize)(base + t + j)] = 0;
								coeffs[(usize)(base + t + rlen)] = (int16)(sign ? -1 : 1);
								tis[(usize)bi] = (uint8)(t + rlen + 1);
								ncoeffs[(usize)bi] = tis[(usize)bi];
								break;
							case 30:
								coeffs[(usize)(base + t)] = 0;
								sign = (int32)br.ReadBits(1);
								mag = (int32)br.ReadBits(1) + 2;
								coeffs[(usize)(base + t + 1)] = (int16)(sign ? -mag : mag);
								tis[(usize)bi] = (uint8)(t + 2);
								ncoeffs[(usize)bi] = tis[(usize)bi];
								break;
							case 31:
								sign = (int32)br.ReadBits(1);
								mag = (int32)br.ReadBits(1) + 2;
								rlen = (int32)br.ReadBits(1) + 2;
								for (int32 j = 0; j < rlen; ++j)
									coeffs[(usize)(base + t + j)] = 0;
								coeffs[(usize)(base + t + rlen)] = (int16)(sign ? -mag : mag);
								tis[(usize)bi] = (uint8)(t + rlen + 1);
								ncoeffs[(usize)bi] = tis[(usize)bi];
								break;
							default:
								break;
						}
					}
					if (br.overrun)
						return;
				}
			}
		}

		// ------------------------------------------------------------------
		//  Dé-prédiction DC (§7.8)
		// ------------------------------------------------------------------
		void NkTheoraDecoder::Impl::UndoDcPrediction() {
			for (int32 pli = 0; pli < 3; ++pli) {
				int32 lastdc[3] = {0, 0, 0};
				const int32 bw = planeBW[pli];
				const int32 bh = planeBH[pli];
				for (int32 by = 0; by < bh; ++by) {
					for (int32 bx = 0; bx < bw; ++bx) {
						int32 bi = rasterToCoded[pli][(usize)(by * bw + bx)];
						if (!bcoded[(usize)bi])
							continue;
						int32 mbi = codedMb[(usize)bi];
						int32 rfi = kModeRefFrame[mbmodes[(usize)mbi]];

						int32 P[4] = {0, 0, 0, 0};
						int32 PBI[4] = {0, 0, 0, 0};
						// P0 = gauche (bx-1,by)
						if (bx > 0) {
							int32 bj = rasterToCoded[pli][(usize)(by * bw + (bx - 1))];
							if (bcoded[(usize)bj] && kModeRefFrame[mbmodes[(usize)codedMb[(usize)bj]]] == rfi) {
								P[0] = 1;
								PBI[0] = bj;
							}
						}
						// P1 = bas-gauche (bx-1,by-1)
						if (bx > 0 && by > 0) {
							int32 bj = rasterToCoded[pli][(usize)((by - 1) * bw + (bx - 1))];
							if (bcoded[(usize)bj] && kModeRefFrame[mbmodes[(usize)codedMb[(usize)bj]]] == rfi) {
								P[1] = 1;
								PBI[1] = bj;
							}
						}
						// P2 = bas (bx,by-1)
						if (by > 0) {
							int32 bj = rasterToCoded[pli][(usize)((by - 1) * bw + bx)];
							if (bcoded[(usize)bj] && kModeRefFrame[mbmodes[(usize)codedMb[(usize)bj]]] == rfi) {
								P[2] = 1;
								PBI[2] = bj;
							}
						}
						// P3 = bas-droite (bx+1,by-1)
						if (by > 0 && bx < bw - 1) {
							int32 bj = rasterToCoded[pli][(usize)((by - 1) * bw + (bx + 1))];
							if (bcoded[(usize)bj] && kModeRefFrame[mbmodes[(usize)codedMb[(usize)bj]]] == rfi) {
								P[3] = 1;
								PBI[3] = bj;
							}
						}

						int32 dcpred;
						if (!P[0] && !P[1] && !P[2] && !P[3]) {
							dcpred = lastdc[rfi];
						} else {
							int32 mask = P[0] | (P[1] << 1) | (P[2] << 2) | (P[3] << 3);
							const DcWeights &w = kDcPredWeights[mask];
							int64 sum = 0;
							for (int32 k = 0; k < 4; ++k)
								if (P[k])
									sum += (int64)w.w[k] * (int64)coeffs[(usize)(PBI[k] * 64 + 0)];
							// Division tronquée vers zéro.
							int32 pdiv = w.pdiv;
							int32 dp;
							if (sum >= 0)
								dp = (int32)(sum / pdiv);
							else
								dp = -(int32)((-sum) / pdiv);
							dcpred = dp;
							if (P[0] && P[1] && P[2]) {
								int32 c2 = coeffs[(usize)(PBI[2] * 64 + 0)];
								int32 c0 = coeffs[(usize)(PBI[0] * 64 + 0)];
								int32 c1 = coeffs[(usize)(PBI[1] * 64 + 0)];
								if (Abs32(dcpred - c2) > 128)
									dcpred = c2;
								else if (Abs32(dcpred - c0) > 128)
									dcpred = c0;
								else if (Abs32(dcpred - c1) > 128)
									dcpred = c1;
							}
						}
						int32 rawdc = (int32)coeffs[(usize)(bi * 64 + 0)];
						int32 dc = Trunc16(rawdc + dcpred);
						coeffs[(usize)(bi * 64 + 0)] = (int16)dc;
						lastdc[rfi] = dc;
					}
				}
			}
		}

		// ------------------------------------------------------------------
		//  iDCT 1D (§7.9.3.1) et 2D (§7.9.3.2)
		// ------------------------------------------------------------------
		static void Idct1D(const int32 *Y, int32 *X) {
			int32 T[8];
			int32 R;
			T[0] = Trunc16(Y[0] + Y[4]);
			T[0] = (kC4 * T[0]) >> 16;
			T[1] = Trunc16(Y[0] - Y[4]);
			T[1] = (kC4 * T[1]) >> 16;
			T[2] = ((kC6 * Y[2]) >> 16) - ((kC2 * Y[6]) >> 16);
			T[3] = ((kC2 * Y[2]) >> 16) + ((kC6 * Y[6]) >> 16);
			T[4] = ((kC7 * Y[1]) >> 16) - ((kC1 * Y[7]) >> 16);
			T[5] = ((kC3 * Y[5]) >> 16) - ((kC5 * Y[3]) >> 16);
			T[6] = ((kC5 * Y[5]) >> 16) + ((kC3 * Y[3]) >> 16);
			T[7] = ((kC1 * Y[1]) >> 16) + ((kC7 * Y[7]) >> 16);
			R = T[4] + T[5];
			T[5] = T[4] - T[5];
			T[5] = Trunc16(T[5]);
			T[5] = (kC4 * T[5]) >> 16;
			T[4] = R;
			R = T[7] + T[6];
			T[6] = T[7] - T[6];
			T[6] = Trunc16(T[6]);
			T[6] = (kC4 * T[6]) >> 16;
			T[7] = R;
			R = T[0] + T[3];
			T[3] = T[0] - T[3];
			T[0] = R;
			R = T[1] + T[2];
			T[2] = T[1] - T[2];
			T[1] = R;
			R = T[6] + T[5];
			T[5] = T[6] - T[5];
			T[6] = R;
			X[0] = Trunc16(T[0] + T[7]);
			X[1] = Trunc16(T[1] + T[6]);
			X[2] = Trunc16(T[2] + T[5]);
			X[3] = Trunc16(T[3] + T[4]);
			X[4] = Trunc16(T[3] - T[4]);
			X[5] = Trunc16(T[2] - T[5]);
			X[6] = Trunc16(T[1] - T[6]);
			X[7] = Trunc16(T[0] - T[7]);
		}

		static void Idct2D(const int32 *dqc, int32 *res) {
			int32 tmp[64];
			int32 Y[8], X[8];
			for (int32 ri = 0; ri < 8; ++ri) {
				for (int32 ci = 0; ci < 8; ++ci)
					Y[ci] = dqc[ri * 8 + ci];
				Idct1D(Y, X);
				for (int32 ci = 0; ci < 8; ++ci)
					tmp[ri * 8 + ci] = X[ci];
			}
			for (int32 ci = 0; ci < 8; ++ci) {
				for (int32 ri = 0; ri < 8; ++ri)
					Y[ri] = tmp[ri * 8 + ci];
				Idct1D(Y, X);
				for (int32 ri = 0; ri < 8; ++ri)
					res[ri * 8 + ci] = (X[ri] + 8) >> 4;
			}
		}

		// ------------------------------------------------------------------
		//  Reconstruction (§7.9.4)
		// ------------------------------------------------------------------
		void NkTheoraDecoder::Impl::Reconstruct(int32 qi0, const int32 *qis, int32 ftype) {
			(void)ftype;
			// Références par plan.
			const uint8 *prevPlanes[3] = {prevY.Data(), prevCb.Data(), prevCr.Data()};
			const uint8 *goldPlanes[3] = {goldY.Data(), goldCb.Data(), goldCr.Data()};
			uint8 *curPlanes[3] = {curY.Data(), curCb.Data(), curCr.Data()};

			int32 qmatDcCache[2][3][64];
			bool qmatDcValid[2][3] = {};

			for (int32 bi = 0; bi < nbs; ++bi) {
				int32 pli = codedPlane[(usize)bi];
				const int32 rpw = planePW[pli];
				const int32 rph = planePH[pli];
				int32 bx = codedBx[(usize)bi];
				int32 by = codedBy[(usize)bi];
				int32 BX = bx * 8;
				int32 BY = by * 8;
				uint8 *cur = curPlanes[pli];

				int32 pred[64];
				int32 res[64];

				if (bcoded[(usize)bi]) {
					int32 mbi = codedMb[(usize)bi];
					int32 mode = mbmodes[(usize)mbi];
					int32 qti = (mode == 1) ? 0 : 1;
					int32 rfi = kModeRefFrame[mode];

					if (rfi == 0) {
						// Prédicteur intra = 128.
						for (int32 k = 0; k < 64; ++k)
							pred[k] = 128;
					} else {
						const uint8 *ref = (rfi == 1) ? prevPlanes[pli] : goldPlanes[pli];
						int32 mvx = mvects[(usize)(bi * 2)];
						int32 mvy = mvects[(usize)(bi * 2 + 1)];
						// Sur un axe sous-échantillonné du chroma, la composante MV est
						// interprétée au quart de pel (§3394-3395) : on la divise par
						// deux (troncature vers zéro) avant la dérivation pleine/demi-pel.
						if (pli > 0) {
							if (pf == 0 || pf == 2)
								mvx = mvx / 2;
							if (pf == 0)
								mvy = mvy / 2;
						}
						int32 amx = mvx < 0 ? -mvx : mvx;
						int32 amy = mvy < 0 ? -mvy : mvy;
						int32 sgx = mvx < 0 ? -1 : 1;
						int32 sgy = mvy < 0 ? -1 : 1;
						int32 mvx1 = (amx >> 1) * sgx;
						int32 mvy1 = (amy >> 1) * sgy;
						int32 mvx2 = ((amx + 1) >> 1) * sgx;
						int32 mvy2 = ((amy + 1) >> 1) * sgy;
						if (mvx1 == mvx2 && mvy1 == mvy2) {
							for (int32 py = 0; py < 8; ++py) {
								int32 ry = BY + mvy1 + py;
								if (ry > rph - 1)
									ry = rph - 1;
								if (ry < 0)
									ry = 0;
								for (int32 px = 0; px < 8; ++px) {
									int32 rx = BX + mvx1 + px;
									if (rx > rpw - 1)
										rx = rpw - 1;
									if (rx < 0)
										rx = 0;
									pred[py * 8 + px] = ref[(usize)(ry * rpw + rx)];
								}
							}
						} else {
							for (int32 py = 0; py < 8; ++py) {
								int32 ry1 = BY + mvy1 + py;
								if (ry1 > rph - 1)
									ry1 = rph - 1;
								if (ry1 < 0)
									ry1 = 0;
								int32 ry2 = BY + mvy2 + py;
								if (ry2 > rph - 1)
									ry2 = rph - 1;
								if (ry2 < 0)
									ry2 = 0;
								for (int32 px = 0; px < 8; ++px) {
									int32 rx1 = BX + mvx1 + px;
									if (rx1 > rpw - 1)
										rx1 = rpw - 1;
									if (rx1 < 0)
										rx1 = 0;
									int32 rx2 = BX + mvx2 + px;
									if (rx2 > rpw - 1)
										rx2 = rpw - 1;
									if (rx2 < 0)
										rx2 = 0;
									pred[py * 8 + px] =
										(ref[(usize)(ry1 * rpw + rx1)] + ref[(usize)(ry2 * rpw + rx2)]) >> 1;
								}
							}
						}
					}

					if (ncoeffs[(usize)bi] < 2) {
						// Cas DC seul.
						if (!qmatDcValid[qti][pli]) {
							ComputeQMat(qti, pli, qi0, qmatDcCache[qti][pli]);
							qmatDcValid[qti][pli] = true;
						}
						int32 dc = Trunc16(((int32)coeffs[(usize)(bi * 64 + 0)] * qmatDcCache[qti][pli][0] + 15) >> 5);
						for (int32 k = 0; k < 64; ++k)
							res[k] = dc;
					} else {
						int32 qi = qis[qiis[(usize)bi]];
						int32 qmatDc[64], qmatAc[64];
						if (!qmatDcValid[qti][pli]) {
							ComputeQMat(qti, pli, qi0, qmatDcCache[qti][pli]);
							qmatDcValid[qti][pli] = true;
						}
						for (int32 k = 0; k < 64; ++k)
							qmatDc[k] = qmatDcCache[qti][pli][k];
						ComputeQMat(qti, pli, qi, qmatAc);
						// Déquantification (§7.9.2), ordre naturel.
						int32 dqc[64];
						dqc[0] = Trunc16((int32)coeffs[(usize)(bi * 64 + 0)] * qmatDc[0]);
						for (int32 ci = 1; ci < 64; ++ci) {
							int32 zzi = kNatToZz[ci];
							dqc[ci] = Trunc16((int32)coeffs[(usize)(bi * 64 + zzi)] * qmatAc[ci]);
						}
						Idct2D(dqc, res);
					}
				} else {
					// Bloc non codé : copie du bloc co-localisé de la trame précédente.
					const uint8 *ref = prevPlanes[pli];
					for (int32 py = 0; py < 8; ++py) {
						int32 ry = BY + py;
						if (ry > rph - 1)
							ry = rph - 1;
						for (int32 px = 0; px < 8; ++px) {
							int32 rx = BX + px;
							if (rx > rpw - 1)
								rx = rpw - 1;
							pred[py * 8 + px] = ref[(usize)(ry * rpw + rx)];
						}
					}
					for (int32 k = 0; k < 64; ++k)
						res[k] = 0;
				}

				for (int32 py = 0; py < 8; ++py)
					for (int32 px = 0; px < 8; ++px) {
						int32 v = Clamp255(pred[py * 8 + px] + res[py * 8 + px]);
						cur[(usize)((BY + py) * rpw + (BX + px))] = (uint8)v;
					}
			}
		}

		// ------------------------------------------------------------------
		//  Filtre de boucle (§7.10)
		// ------------------------------------------------------------------
		static inline int32 LfLim(int32 r, int32 l) {
			// Fonction non-linéaire lflim (§7.10).
			if (r <= -2 * l)
				return 0;
			if (r <= -l)
				return -r - 2 * l;
			if (r < l)
				return r;
			if (r < 2 * l)
				return -r + 2 * l;
			return 0;
		}

		void NkTheoraDecoder::Impl::LoopFilter(int32 qi0) {
			int32 L = lflims[qi0];
			if (L <= 0)
				return;
			uint8 *curPlanes[3] = {curY.Data(), curCb.Data(), curCr.Data()};

			// Filtre horizontal (bord vertical) : colonnes FX..FX+3, lignes FY..FY+7.
			auto hfilter = [&](uint8 *rp, int32 rpw, int32 rph, int32 FX, int32 FY) {
				(void)rph;
				for (int32 by = 0; by < 8; ++by) {
					int32 row = (FY + by) * rpw;
					int32 a = rp[(usize)(row + FX)];
					int32 b = rp[(usize)(row + FX + 1)];
					int32 c = rp[(usize)(row + FX + 2)];
					int32 d = rp[(usize)(row + FX + 3)];
					int32 R = (a - 3 * b + 3 * c - d + 4) >> 3;
					int32 f = LfLim(R, L);
					int32 p = b + f;
					rp[(usize)(row + FX + 1)] = (uint8)Clamp255(p);
					p = c - f;
					rp[(usize)(row + FX + 2)] = (uint8)Clamp255(p);
				}
			};
			// Filtre vertical (bord horizontal) : lignes FY..FY+3, colonnes FX..FX+7.
			auto vfilter = [&](uint8 *rp, int32 rpw, int32 rph, int32 FX, int32 FY) {
				(void)rph;
				for (int32 bx = 0; bx < 8; ++bx) {
					int32 a = rp[(usize)((FY + 0) * rpw + FX + bx)];
					int32 b = rp[(usize)((FY + 1) * rpw + FX + bx)];
					int32 c = rp[(usize)((FY + 2) * rpw + FX + bx)];
					int32 d = rp[(usize)((FY + 3) * rpw + FX + bx)];
					int32 R = (a - 3 * b + 3 * c - d + 4) >> 3;
					int32 f = LfLim(R, L);
					int32 p = b + f;
					rp[(usize)((FY + 1) * rpw + FX + bx)] = (uint8)Clamp255(p);
					p = c - f;
					rp[(usize)((FY + 2) * rpw + FX + bx)] = (uint8)Clamp255(p);
				}
			};

			for (int32 pli = 0; pli < 3; ++pli) {
				uint8 *rp = curPlanes[pli];
				int32 rpw = planePW[pli];
				int32 rph = planePH[pli];
				int32 bw = planeBW[pli];
				int32 bh = planeBH[pli];
				for (int32 by = 0; by < bh; ++by) {
					for (int32 bx = 0; bx < bw; ++bx) {
						int32 bi = rasterToCoded[pli][(usize)(by * bw + bx)];
						if (!bcoded[(usize)bi])
							continue;
						int32 BX = bx * 8;
						int32 BY = by * 8;
						if (BX > 0)
							hfilter(rp, rpw, rph, BX - 2, BY);
						if (BY > 0)
							vfilter(rp, rpw, rph, BX, BY - 2);
						if (BX + 8 < rpw) {
							int32 bj = rasterToCoded[pli][(usize)(by * bw + (bx + 1))];
							if (!bcoded[(usize)bj])
								hfilter(rp, rpw, rph, BX + 6, BY);
						}
						if (BY + 8 < rph) {
							int32 bj = rasterToCoded[pli][(usize)((by + 1) * bw + bx)];
							if (!bcoded[(usize)bj])
								vfilter(rp, rpw, rph, BX, BY + 6);
						}
					}
				}
			}
		}

		// ------------------------------------------------------------------
		//  Décodage d'une trame complète (§7.11)
		// ------------------------------------------------------------------
		bool NkTheoraDecoder::Impl::DecodeFrame(NkTheoraFrame &out, NkString *err) {
			if (nextDataPacket >= packets.Size())
				return false;
			const Packet &pk = packets[nextDataPacket];
			++nextDataPacket;

			// Allocation des buffers courants.
			curY.Resize((usize)(planePW[0] * planePH[0]));
			curCb.Resize((usize)(planePW[1] * planePH[1]));
			curCr.Resize((usize)(planePW[2] * planePH[2]));

			int32 ftype = 0;
			int32 nqis = 1;
			int32 qis[3] = {0, 0, 0};

			if (pk.bytes.Size() > 0) {
				BitReader br(pk.bytes.Data(), (usize)pk.bytes.Size());
				// §7.1 : en-tête de trame.
				if (br.ReadBit() != 0) {
					if (err)
						*err = "Theora : paquet de donnees invalide (bit 0 != 0)";
					return false;
				}
				ftype = (int32)br.ReadBit();
				if (ftype == 0 && !haveRef) {
					// première trame : doit être intra (OK).
				}
				if (ftype != 0 && !haveRef) {
					if (err)
						*err = "Theora : trame inter avant toute trame cle";
					return false;
				}
				qis[0] = (int32)br.ReadBits(6);
				int32 moreqis = (int32)br.ReadBit();
				if (moreqis) {
					qis[1] = (int32)br.ReadBits(6);
					moreqis = (int32)br.ReadBit();
					if (moreqis) {
						qis[2] = (int32)br.ReadBits(6);
						nqis = 3;
					} else
						nqis = 2;
				} else
					nqis = 1;
				if (ftype == 0)
					(void)br.ReadBits(3); // réservé

				DecodeCodedFlags(br, ftype);
				DecodeModes(br, ftype);
				if (ftype != 0)
					DecodeMVs(br);
				else {
					mvects.Resize((usize)(nbs * 2));
					for (int32 i = 0; i < nbs * 2; ++i)
						mvects[(usize)i] = 0;
				}
				DecodeBlockQis(br, nqis);
				DecodeCoefficients(br);
				UndoDcPrediction();
			} else {
				// Paquet 0 octet = trame inter sans bloc codé.
				ftype = 1;
				nqis = 1;
				qis[0] = 63;
				bcoded.Resize((usize)nbs);
				for (int32 i = 0; i < nbs; ++i)
					bcoded[(usize)i] = 0;
				mbmodes.Resize((usize)nmbs);
				for (int32 i = 0; i < nmbs; ++i)
					mbmodes[(usize)i] = 0;
				mvects.Resize((usize)(nbs * 2));
				for (int32 i = 0; i < nbs * 2; ++i)
					mvects[(usize)i] = 0;
				coeffs.Resize((usize)(nbs * 64));
				for (usize i = 0; i < coeffs.Size(); ++i)
					coeffs[i] = 0;
				ncoeffs.Resize((usize)nbs);
				for (int32 i = 0; i < nbs; ++i)
					ncoeffs[(usize)i] = 0;
				if (!haveRef) {
					if (err)
						*err = "Theora : premier paquet vide (pas de reference)";
					return false;
				}
			}

			Reconstruct(qis[0], qis, ftype);
			LoopFilter(qis[0]);

			// Mise à jour des trames de référence.
			prevY = curY;
			prevCb = curCb;
			prevCr = curCr;
			if (ftype == 0) {
				goldY = curY;
				goldCb = curCb;
				goldCr = curCr;
			}
			haveRef = true;

			// Sortie : rognage à la région d'affichage + retournement TOP-DOWN.
			int32 lumaW = picW > 0 ? picW : planePW[0];
			int32 lumaH = picH > 0 ? picH : planePH[0];
			int32 subX = (pf == 0 || pf == 2) ? 1 : 0;
			int32 subY = (pf == 0) ? 1 : 0;
			int32 chW = lumaW >> subX;
			int32 chH = lumaH >> subY;
			int32 cPicX = picX >> subX;
			int32 cPicY = picY >> subY;

			out.width = lumaW;
			out.height = lumaH;
			out.chromaWidth = chW;
			out.chromaHeight = chH;
			out.isKeyFrame = (ftype == 0);
			out.frameIndex = displayIndex++;
			out.y.Resize((usize)(lumaW * lumaH));
			out.cb.Resize((usize)(chW * chH));
			out.cr.Resize((usize)(chW * chH));

			// Luma.
			for (int32 d = 0; d < lumaH; ++d) {
				int32 srcRow = picY + (lumaH - 1 - d); // bas-en-haut → top-down
				for (int32 c = 0; c < lumaW; ++c)
					out.y[(usize)(d * lumaW + c)] = curY[(usize)(srcRow * planePW[0] + (picX + c))];
			}
			// Chroma.
			for (int32 d = 0; d < chH; ++d) {
				int32 srcRow = cPicY + (chH - 1 - d);
				for (int32 c = 0; c < chW; ++c) {
					out.cb[(usize)(d * chW + c)] = curCb[(usize)(srcRow * planePW[1] + (cPicX + c))];
					out.cr[(usize)(d * chW + c)] = curCr[(usize)(srcRow * planePW[2] + (cPicX + c))];
				}
			}
			return true;
		}

		// =====================================================================
		//  API publique
		// =====================================================================
		NkTheoraDecoder::NkTheoraDecoder() {
			void *mem = nkentseu::memory::NkAlloc(sizeof(Impl));
			mImpl = new (mem) Impl();
		}

		NkTheoraDecoder::~NkTheoraDecoder() {
			if (mImpl) {
				mImpl->~Impl();
				nkentseu::memory::NkFree(mImpl);
				mImpl = nullptr;
			}
		}

		bool NkTheoraDecoder::Probe(const uint8 *data, usize size) {
			if (!data || size < 35)
				return false;
			usize pos = 0;
			OggPageView pg;
			while (pos < size && OggParsePage(data, size, pos, pg)) {
				if ((pg.headerType & 0x02) != 0 && pg.payloadSize >= 7) {
					const uint8 *pl = data + pg.payloadOffset;
					if (pl[0] == 0x80 && pl[1] == 't' && pl[2] == 'h' && pl[3] == 'e' && pl[4] == 'o' &&
						pl[5] == 'r' && pl[6] == 'a')
						return true;
				}
				pos = pg.next;
			}
			return false;
		}

		bool NkTheoraDecoder::Open(const uint8 *data, usize size, NkString *outError) {
			if (!data || size == 0) {
				if (outError)
					*outError = "Theora : buffer vide";
				return false;
			}
			// 1) Trouver le serial du flux Theora (BOS commençant par 0x80 'theora').
			uint32 serial = 0;
			bool haveSerial = false;
			{
				usize pos = 0;
				OggPageView pg;
				while (pos < size && OggParsePage(data, size, pos, pg)) {
					if ((pg.headerType & 0x02) != 0 && pg.payloadSize >= 7) {
						const uint8 *pl = data + pg.payloadOffset;
						if (pl[0] == 0x80 && pl[1] == 't' && pl[2] == 'h' && pl[3] == 'e' && pl[4] == 'o' &&
							pl[5] == 'r' && pl[6] == 'a') {
							serial = pg.serial;
							haveSerial = true;
							break;
						}
					}
					pos = pg.next;
				}
			}
			if (!haveSerial) {
				if (outError)
					*outError = "Theora : aucun flux logique Theora dans le conteneur Ogg";
				return false;
			}

			// 2) Reconstituer les paquets du flux (gère les paquets multi-pages).
			mImpl->packets.Clear();
			{
				usize pos = 0;
				OggPageView pg;
				NkVector<uint8> cur;
				bool open = false;
				while (pos < size && OggParsePage(data, size, pos, pg)) {
					if (pg.serial != serial) {
						pos = pg.next;
						continue;
					}
					const uint8 *payload = data + pg.payloadOffset;
					usize segOff = 0;
					for (uint8 i = 0; i < pg.nsegs; ++i) {
						uint8 lace = pg.segTable[i];
						if (!open) {
							cur.Clear();
							open = true;
						}
						for (uint8 k = 0; k < lace; ++k)
							cur.PushBack(payload[segOff + k]);
						segOff += lace;
						if (lace < 255) {
							Packet packet;
							packet.bytes = cur;
							mImpl->packets.PushBack(packet);
							open = false;
						}
					}
					pos = pg.next;
				}
			}

			if (mImpl->packets.Size() < 3) {
				if (outError)
					*outError = "Theora : en-tetes incomplets (< 3 paquets)";
				return false;
			}

			// 3) Décoder les 3 en-têtes.
			// Ident (0x80).
			{
				const Packet &p0 = mImpl->packets[0];
				if (p0.bytes.Size() < 7 || p0.bytes[0] != 0x80) {
					if (outError)
						*outError = "Theora : en-tete d'identification manquant";
					return false;
				}
				BitReader br(p0.bytes.Data(), (usize)p0.bytes.Size());
				br.ReadBits(8); // type
				for (int32 i = 0; i < 6; ++i)
					br.ReadBits(8); // "theora"
				if (!mImpl->ParseIdent(br)) {
					if (outError)
						*outError = "Theora : en-tete d'identification invalide";
					return false;
				}
			}
			// Setup (0x82).
			{
				const Packet &p2 = mImpl->packets[2];
				if (p2.bytes.Size() < 7 || p2.bytes[0] != 0x82) {
					if (outError)
						*outError = "Theora : en-tete setup manquant";
					return false;
				}
				BitReader br(p2.bytes.Data(), (usize)p2.bytes.Size());
				br.ReadBits(8);
				for (int32 i = 0; i < 6; ++i)
					br.ReadBits(8);
				if (!mImpl->ParseSetup(br)) {
					if (outError)
						*outError = "Theora : en-tete setup invalide";
					return false;
				}
			}

			// 4) Construire la table MV (Table 7.23) + la géométrie.
			BuildMvTree(mImpl->mvTree);
			if (!mImpl->BuildGeometry()) {
				if (outError)
					*outError = "Theora : geometrie invalide";
				return false;
			}
			mImpl->nextDataPacket = 3;
			mImpl->haveRef = false;
			mImpl->displayIndex = 0;

			// Références vides (au cas où).
			mImpl->prevY.Resize((usize)(mImpl->planePW[0] * mImpl->planePH[0]));
			mImpl->prevCb.Resize((usize)(mImpl->planePW[1] * mImpl->planePH[1]));
			mImpl->prevCr.Resize((usize)(mImpl->planePW[2] * mImpl->planePH[2]));
			mImpl->goldY = mImpl->prevY;
			mImpl->goldCb = mImpl->prevCb;
			mImpl->goldCr = mImpl->prevCr;

			mFmbw = mImpl->fmbw;
			mFmbh = mImpl->fmbh;
			mPicW = mImpl->picW;
			mPicH = mImpl->picH;
			mPicX = mImpl->picX;
			mPicY = mImpl->picY;
			mPf = mImpl->pf;
			return true;
		}

		bool NkTheoraDecoder::DecodeNextFrame(NkTheoraFrame &out, NkString *outError) {
			return mImpl->DecodeFrame(out, outError);
		}

		bool NkTheoraDecoder::HasMoreFrames() const {
			return mImpl->nextDataPacket < mImpl->packets.Size();
		}

	} // namespace media
} // namespace nkentseu
