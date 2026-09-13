#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkModelerGeom.h — LA GEOMETRIE DANS LE FICHIER : sommets et indices.
//
// LE DEFAUT QUE CE FICHIER FERME. Le `.nkmesh` et le `.nkscene` ecrivaient les
// noeuds, leurs origines, leurs noms, leurs materiaux et les PARAMETRES DE
// CREATION des primitives -- jamais les SOMMETS. Un maillage revenait donc
// regenere depuis ses parametres : une sphere retrouvait sa sphere, mais tout
// ce qui avait ete deplace, extrude ou importe etait perdu a la fermeture.
// C'est la dette nommee dans l'en-tete de NkModelerScene.h (« CE QUI N'EST PAS
// ENCORE SAUVEGARDE », deux premieres lignes).
//
// ON N'INVENTE PAS UN FORMAT DE PLUS. Le depot n'a AUCUN ecrivain de maillage
// (NkOBJIO::Export et NkGLTFExporter le DISENT eux-memes dans leur en-tete :
// non implementes), et NkEditableMesh ne sait pas se serialiser. La geometrie
// entre donc dans le conteneur qui existe deja -- l'archive du `.nkmesh` /
// `.nkscene` -- sous une cle `geometrie` par noeud, a cote de `creation`.
//
// POURQUOI DU BASE64 ET PAS UN TABLEAU DE FLOTTANTS
//   1. EXACTITUDE. Les octets du sommet sont recopies tels quels : l'aller
//      -retour est BIT A BIT, pas « a 1e-4 pres ». Un tableau de flottants
//      passe par une ecriture decimale, et c'est la que les positions
//      derivent -- exactement ce qu'un modeleur ne doit pas faire au travail
//      de quelqu'un.
//   2. TAILLE. Un sommet fait 56 octets ; en base64 il en coute 76. Ecrit en
//      JSON, le meme sommet demanderait entre 150 et 200 caracteres.
//   Le prix est assume : le bloc n'est pas lisible a l'oeil. Les COMPTES, eux,
//   le sont (`sommets`, `indices`, `octetsParSommet`), et c'est ce qu'on lit
//   quand on ouvre un fichier pour comprendre ce qu'il porte.
//
// LE PAS DU SOMMET EST ECRIT DANS LE FICHIER, et relu avant de decoder. Le jour
// ou NkVertex3D gagnera un champ, les fichiers d'avant ne seront pas lus DE
// TRAVERS : ils seront refuses avec leur pas, ce qui se voit. Un bloc binaire
// dont on devine la forme est la pire des relectures.
//
// ORDRE DES OCTETS : celui de la machine. Les huit plateformes du depot sont
// petit-boutistes ; le jour ou l'une ne le sera plus, c'est le pas ET un
// marqueur d'ordre qu'il faudra ecrire. Dit ici pour que ce ne soit pas une
// surprise.
//
// AUCUNE DEPENDANCE AU RENDU NI A L'HOTE 3D : ce fichier ne connait que des
// octets, un pas, et une archive. C'est ce qui permet a la sonde `--probe=geom`
// de l'eprouver SANS FENETRE ET SANS GPU.
// =============================================================================

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/Encoding/NkBase64.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKSerialization/NkArchive.h"
// LE COMPRESSEUR DU DEPOT. Il vit dans NKImage parce que le PNG en avait besoin
// le premier -- ce n'est pas une dependance au rendu, c'est du CPU pur, et la
// sonde `--probe=geom` s'en sert sans fenetre comme du reste.
#include "NKImage/NKImage.h"
#include "NKMemory/NKMemory.h"

#include <cstdio>

#ifdef GetObject
#undef GetObject
#endif

namespace nkentseu {
	namespace nk3d {

		// Version du bloc `geometrie`, distincte de celle du fichier d'asset : la
		// forme d'un sommet bougera a son rythme.
		//
		// 1 -> 2 (13/09, soir) : les octets peuvent etre TRANSPOSES ET COMPRESSES.
		// Un fichier de version 1 reste lu tel quel -- la compatibilite descendante
		// prouvee ce matin ne se perd pas au lot suivant --, et un bloc de version 2
		// DIT son codage (`codage`) au lieu de le faire deviner.
		static const int32 kGeomBlockVersion = 2;

		// =====================================================================
		// LA COMPRESSION, ET POURQUOI CELLE-LA
		// =====================================================================
		// MESURE D'ABORD, CHOIX ENSUITE. Trois maillages REELS du depot, rapport sur
		// les octets bruts du sommet (plus c'est grand, mieux c'est) :
		//
		//                                 cube .ply   rock .obj   canard .gltf
		//                                   8 som.    165 som.     5 676 som.
		//   base64 seul (ce qu'on faisait)   x0,75      x0,75        x0,75  (+33 %)
		//   NkDeflate::Compress              x3,86      x2,02        x1,22
		//   transposition + RLE              x2,52      x1,47        x1,48
		//   transposition + NkDeflate        x3,96      x1,90        x1,63
		//   (reference : zlib -6 = x3,86 / x2,25 / x1,37)
		//
		// ⚠️ CE QUE J'AI FAILLI ECRIRE, ET POURQUOI JE NE L'AI PAS ECRIT.
		// L'en-tete de `NkDeflate` (NKImage.h) annonce « stored blocks (BTYPE=00)
		// [...] sans compression reelle ». Sur cette foi, j'ai d'abord conclu que le
		// depot n'avait aucun compresseur -- et j'allais en ecrire un de plus a cote
		// d'un compresseur qui marche. Le `.cpp` dit l'inverse de son en-tete
		// (NkImage.cpp l. 1152 : « UN bloc Huffman FIXE (BTYPE=01) + LZ77 ») et la
		// MESURE tranche : x3,86 sur le petit cube. LA DOCUMENTATION EST UN
		// INSTRUMENT, et celui-la mentait. Corrige dans le meme lot.
		//
		// POURQUOI TRANSPOSER AVANT DE COMPRESSER. Sur un sommet de 56 octets, 12 a
		// 38 COLONNES D'OCTETS sont CONSTANTES sur tout le maillage (uv2 a zero,
		// couleur identique, exposants de flottants voisins). Groupees par colonne
		// elles deviennent de longues series ; melangees dans l'ordre des sommets,
		// elles ne le sont jamais. Le gain est net sur le gros maillage (x1,22 ->
		// x1,63) et nul a legerement negatif sur les petits -- d'ou le choix
		// ci-dessous.
		//
		// TROIS CODAGES, ET ON ECRIT LE PLUS PETIT. `brut`, `transpose-rle`,
		// `transpose-deflate` : les trois sont calcules, le plus court gagne, et le
		// fichier DIT lequel. Aucun n'est retenu s'il ne bat pas le brut : un codage
		// qui grossit le fichier serait une complication payee pour rien. Le RLE
		// reste parce qu'il n'a AUCUN en-tete la ou zlib en coute six -- sur un
		// maillage de quelques dizaines d'octets, six octets se voient.
		//
		// UN OCTET ABIME EST DETECTE, JAMAIS RELU EN SILENCE. Deux gardes, et il
		// faut passer les DEUX : la taille decodee doit retomber exactement sur
		// `octetsBruts`, et l'empreinte FNV-1a 64 bits des octets BRUTS doit
		// retomber sur `somme`. La premiere attrape une troncature ou une longueur
		// de serie abimee ; la seconde attrape un octet retourne qui laisserait la
		// longueur intacte.
		// =====================================================================

		/// Empreinte FNV-1a 64 bits. Choisie parce qu'elle tient en six lignes et
		/// n'a besoin d'aucune table : ce n'est pas une signature, c'est un
		/// detecteur d'abimage.
		inline uint64 NkGeomFnv1a(const uint8 *p, usize n) {
			uint64 h = 14695981039346656037ull;
			for (usize i = 0; i < n; ++i) {
				h ^= (uint64)p[i];
				h *= 1099511628211ull;
			}
			return h;
		}

		/// Regroupe les octets PAR COLONNE : tous les octets 0 des sommets, puis
		/// tous les octets 1... C'est le « shuffle » des formats scientifiques, et
		/// c'est ce qui transforme une colonne constante en une longue serie.
		/// La queue (si `n` n'est pas un multiple du pas) est recopiee telle quelle
		/// a la fin -- elle ne devrait pas exister, mais un codage qui perd des
		/// octets dans un cas qu'il croit impossible est un codage qui ment.
		inline void NkGeomTranspose(const uint8 *in, usize n, uint32 stride, NkVector<uint8> &out) {
			out.Clear();
			out.Resize(n);
			if (stride == 0u) {
				for (usize i = 0; i < n; ++i)
					out[i] = in[i];
				return;
			}
			const usize rows = n / stride;
			usize k = 0;
			for (uint32 c = 0; c < stride; ++c)
				for (usize r = 0; r < rows; ++r)
					out[k++] = in[r * stride + c];
			for (usize i = rows * stride; i < n; ++i)
				out[k++] = in[i];
		}

		/// L'inverse exact.
		inline void NkGeomUntranspose(const uint8 *in, usize n, uint32 stride, NkVector<uint8> &out) {
			out.Clear();
			out.Resize(n);
			if (stride == 0u) {
				for (usize i = 0; i < n; ++i)
					out[i] = in[i];
				return;
			}
			const usize rows = n / stride;
			usize k = 0;
			for (uint32 c = 0; c < stride; ++c)
				for (usize r = 0; r < rows; ++r)
					out[r * stride + c] = in[k++];
			for (usize i = rows * stride; i < n; ++i)
				out[i] = in[k++];
		}

		/// RLE a deux formes, sans ambiguite possible :
		///   0x00, compte(1..255), valeur   -> une SERIE
		///   L(1..255), L octets            -> des LITTERAUX
		/// Une serie n'est ecrite qu'a partir de trois octets identiques : en
		/// dessous, elle couterait plus cher que les litteraux.
		inline void NkGeomRle(const uint8 *in, usize n, NkVector<uint8> &out) {
			out.Clear();
			usize i = 0;
			while (i < n) {
				usize j = i;
				while (j + 1u < n && in[j + 1u] == in[i] && (j - i) < 254u)
					++j;
				const usize run = j - i + 1u;
				if (run >= 3u) {
					out.PushBack(0u);
					out.PushBack((uint8)run);
					out.PushBack(in[i]);
					i = j + 1u;
					continue;
				}
				usize k = i;
				usize lit = 0;
				while (k < n && lit < 254u) {
					if (k + 2u < n && in[k] == in[k + 1u] && in[k] == in[k + 2u])
						break;
					++k;
					++lit;
				}
				out.PushBack((uint8)lit);
				for (usize m = i; m < k; ++m)
					out.PushBack(in[m]);
				i = k;
			}
		}

		/// Decodage BORNE. Rend faux des que le flux demande plus que `expect` ou
		/// s'arrete trop tot : un decodeur qui ecrit au-dela de ce qu'on lui a
		/// promis est la faille, pas la performance.
		inline bool NkGeomUnrle(const uint8 *in, usize n, usize expect, NkVector<uint8> &out) {
			out.Clear();
			out.Reserve(expect);
			usize i = 0;
			usize ecrits = 0;
			while (i < n) {
				const uint8 tag = in[i++];
				if (tag == 0u) {
					if (i + 1u >= n)
						return false;
					const usize run = (usize)in[i++];
					const uint8 v = in[i++];
					if (run == 0u || ecrits + run > expect)
						return false;
					for (usize k = 0; k < run; ++k)
						out.PushBack(v);
					ecrits += run;
				} else {
					const usize lit = (usize)tag;
					if (i + lit > n || ecrits + lit > expect)
						return false;
					for (usize k = 0; k < lit; ++k)
						out.PushBack(in[i + k]);
					i += lit;
					ecrits += lit;
				}
			}
			return ecrits == expect;
		}

		// ── LE VRAI DEFLATE, ET LA DOCUMENTATION QUI DISAIT LE CONTRAIRE ─────
		// ⚠️ `NkImage.h` annonce que `NkDeflate::Compress` ecrit des « stored blocks
		// (BTYPE=00) [...] sans compression reelle ». C'EST FAUX depuis longtemps :
		// le `.cpp` (NkImage.cpp l. 1152) ecrit « UN bloc Huffman FIXE (BTYPE=01) +
		// LZ77 », et la mesure le confirme. J'ai failli conclure « le depot n'a pas
		// de compresseur » sur la foi de cet en-tete, ce qui aurait fait ecrire un
		// compresseur de plus a cote d'un compresseur qui marche. LA DOC EST UN
		// INSTRUMENT, et celui-la mentait -- corrige dans le meme lot.
		//
		// LES TROIS CODAGES, ET POURQUOI ON LES GARDE TOUS
		//   brut               : les octets tels quels (fichiers d'hier, et le repli)
		//   transpose-rle      : sans dependance, sans en-tete -- il gagne sur les
		//                        tout petits maillages ou l'en-tete zlib (6 octets)
		//                        pese autant que le gain
		//   transpose-deflate  : transposition PUIS NkDeflate -- le meilleur des
		//                        trois sur la donnee reelle
		// On les calcule tous et ON ECRIT LE PLUS PETIT. Le fichier DIT lequel ; un
		// lecteur ne devine jamais.
		inline bool NkGeomDeflate(const uint8 *in, usize n, NkVector<uint8> &out) {
			out.Clear();
			uint8 *comp = nullptr;
			usize compSz = 0;
			if (!NkDeflate::Compress(in, n, comp, compSz, 6) || !comp || compSz == 0u) {
				if (comp)
					memory::NkFree(comp);
				return false;
			}
			out.Resize(compSz);
			for (usize i = 0; i < compSz; ++i)
				out[i] = comp[i];
			memory::NkFree(comp);
			return true;
		}

		inline bool NkGeomInflate(const uint8 *in, usize n, usize expect, NkVector<uint8> &out) {
			out.Clear();
			if (expect == 0u)
				return false;
			out.Resize(expect);
			usize written = 0;
			if (!NkDeflate::Decompress(in, n, out.Data(), expect, written) || written != expect) {
				out.Clear();
				return false;
			}
			return true;
		}

		/// Ecrit un tampon d'octets sous `key`, compresse SI ET SEULEMENT SI ca gagne.
		/// Pose a cote : `<key>Brut` (taille avant codage) et `<key>Somme` (empreinte
		/// des octets BRUTS, en hexadecimal). Rend vrai si le codage compresse a ete
		/// retenu -- l'appelant en a besoin pour ecrire `codage`.
		inline const char *NkGeomPack(NkArchive &g, const char *key, const uint8 *raw, usize n,
									  uint32 stride) {
			char kb[48], ks[48];
			snprintf(kb, sizeof(kb), "%sBrut", key);
			snprintf(ks, sizeof(ks), "%sSomme", key);
			g.SetInt64(kb, (nk_int64)n);
			{
				char hex[24];
				snprintf(hex, sizeof(hex), "%016llx",
						 (unsigned long long)NkGeomFnv1a(raw, n));
				g.SetString(ks, hex);
			}
			NkVector<uint8> tr, rl, df;
			NkGeomTranspose(raw, n, stride, tr);
			NkGeomRle(tr.Data(), tr.Size(), rl);
			const bool okDf = NkGeomDeflate(tr.Data(), tr.Size(), df);
			// LE PLUS PETIT GAGNE, et le codage DOIT gagner sur le brut : un codage
			// qui grossit le fichier serait une complication payee pour rien.
			// Comparaison sur les octets AVANT base64 -- les trois chemins paient
			// ensuite le meme +33 %.
			const usize szRl = rl.Size();
			const usize szDf = okDf ? df.Size() : n + 1u;
			if (szDf < n && szDf <= szRl) {
				g.SetString(key, encoding::base64::NkEncode(df.Data(), df.Size()).CStr());
				return "transpose-deflate";
			}
			if (szRl < n) {
				g.SetString(key, encoding::base64::NkEncode(rl.Data(), rl.Size()).CStr());
				return "transpose-rle";
			}
			g.SetString(key, encoding::base64::NkEncode(raw, n).CStr());
			return "brut";
		}

		/// L'inverse. `compresse` vient du `codage` du bloc, jamais d'une devinette.
		/// Trois refus possibles, chacun avec sa raison : base64 illisible, longueur
		/// qui ne retombe pas, empreinte qui ne retombe pas.
		enum NkGeomCodage { NK_GEOM_BRUT = 0, NK_GEOM_RLE = 1, NK_GEOM_DEFLATE = 2 };

		inline bool NkGeomUnpack(const NkArchive &g, const char *key, int32 codage, uint32 stride,
								 NkVector<uint8> &out, NkString *why) {
			out.Clear();
			char kb[48], ks[48];
			snprintf(kb, sizeof(kb), "%sBrut", key);
			snprintf(ks, sizeof(ks), "%sSomme", key);
			nk_int64 brut = 0;
			(void)g.GetInt64(kb, brut);
			NkString b64;
			if (!g.GetString(key, b64) || b64.Empty()) {
				if (why)
					*why = "bloc sans donnees encodees";
				return false;
			}
			usize n = 0;
			const NkStringView vv(b64.CStr(), b64.Size());
			if (!encoding::base64::NkDecode(vv, nullptr, &n) || n == 0u) {
				if (why)
					*why = "donnees encodees illisibles";
				return false;
			}
			NkVector<uint8> flux;
			flux.Resize(n);
			if (!encoding::base64::NkDecode(vv, flux.Data(), &n)) {
				if (why)
					*why = "decodage base64 refuse";
				return false;
			}
			if (codage != NK_GEOM_BRUT) {
				if (brut <= 0) {
					if (why)
						*why = "codage compresse sans taille brute";
					return false;
				}
				NkVector<uint8> tr;
				if (codage == NK_GEOM_RLE) {
					if (!NkGeomUnrle(flux.Data(), flux.Size(), (usize)brut, tr)) {
						if (why)
							*why = "flux RLE abime : la longueur ne retombe pas";
						return false;
					}
				} else {
					if (!NkGeomInflate(flux.Data(), flux.Size(), (usize)brut, tr)) {
						if (why)
							*why = "flux deflate abime : decompression refusee";
						return false;
					}
				}
				NkGeomUntranspose(tr.Data(), tr.Size(), stride, out);
			} else {
				out = flux;
			}
			if (brut > 0 && out.Size() != (usize)brut) {
				out.Clear();
				if (why)
					*why = "taille decodee differente de la taille annoncee";
				return false;
			}
			// L'EMPREINTE EN DERNIER, et elle est la garde qui attrape ce que la
			// longueur laisse passer : un octet retourne ne change aucune taille.
			NkString sum;
			if (g.GetString(ks, sum) && !sum.Empty()) {
				char hex[24];
				snprintf(hex, sizeof(hex), "%016llx",
						 (unsigned long long)NkGeomFnv1a(out.Data(), out.Size()));
				if (!(sum == NkString(hex))) {
					out.Clear();
					if (why)
						*why = "empreinte des octets differente de celle du fichier";
					return false;
				}
			}
			return true;
		}

		/// Ecrit la geometrie d'un noeud dans `nd` sous la cle « geometrie ».
		/// Rend faux -- et n'ecrit RIEN -- si les donnees sont vides ou le pas nul :
		/// une cle `geometrie` presente doit toujours porter des sommets, sinon la
		/// relecture ne saurait pas distinguer « pas de geometrie » de « geometrie
		/// vide », et l'une des deux est un travail perdu.
		inline bool NkGeomWrite(NkArchive &nd, const void *verts, uint32 vcount, uint32 vstride,
								const uint32 *indices, uint32 icount) {
			if (!verts || vcount == 0u || vstride == 0u)
				return false;
			NkArchive g;
			g.SetInt32("version", kGeomBlockVersion);
			g.SetInt32("octetsParSommet", (int32)vstride);
			g.SetInt32("sommets", (int32)vcount);
			g.SetInt32("indices", (int32)icount);
			const char *cv = NkGeomPack(g, "v", (const uint8 *)verts,
										(usize)vcount * (usize)vstride, vstride);
			const char *ci = "brut";
			if (indices && icount > 0u)
				ci = NkGeomPack(g, "i", (const uint8 *)indices,
								(usize)icount * sizeof(uint32), (uint32)sizeof(uint32));
			// LE CODAGE EST ECRIT, PAR TAMPON, EN TOUTES LETTRES. Un lecteur ne doit
			// jamais avoir a deduire d'une taille si les octets sont compresses :
			// c'est ainsi qu'on lit un flux de travers en croyant l'avoir compris.
			g.SetString("codage", cv);
			g.SetString("codageIndices", ci);
			nd.SetObject("geometrie", g);
			return true;
		}

		/// Relit la geometrie d'un noeud. Rend faux si la cle est absente (le cas
		/// NORMAL d'un fichier ecrit avant ce bloc, et celui d'une primitive qui se
		/// regenere de ses parametres), ou si ce qu'elle porte ne se recoupe pas :
		/// pas different, comptes qui ne retombent pas sur les octets decodes.
		/// `why`, s'il est fourni, recoit la raison -- un refus silencieux ferait
		/// disparaitre un maillage sans un mot.
		inline bool NkGeomRead(const NkArchive &nd, uint32 vstride, NkVector<uint8> &verts,
							   uint32 *vcount, NkVector<uint32> &indices, NkString *why = nullptr) {
			verts.Clear();
			indices.Clear();
			if (vcount)
				*vcount = 0u;
			NkArchive g;
			if (!nd.GetObject("geometrie", g))
				return false; // pas de geometrie dans ce noeud : ce n'est pas une erreur
			nk_int32 stride = 0, nv = 0, ni = 0;
			(void)g.GetInt32("octetsParSommet", stride);
			(void)g.GetInt32("sommets", nv);
			(void)g.GetInt32("indices", ni);
			if (stride <= 0 || nv <= 0) {
				if (why)
					*why = "bloc geometrie sans pas ni sommets";
				return false;
			}
			if ((uint32)stride != vstride) {
				// REFUS NET, jamais une lecture approchee : un sommet decode avec le
				// mauvais pas donne une geometrie plausible et fausse.
				if (why) {
					char b[96];
					snprintf(b, sizeof(b), "sommet de %d octets, cette version en attend %u",
							 (int)stride, (unsigned)vstride);
					*why = b;
				}
				return false;
			}
			// ── LE CODAGE VIENT DU FICHIER, JAMAIS D'UNE DEVINETTE ──────────────
			// Un bloc de version 1 (ecrit ce matin) ne porte PAS la cle `codage` :
			// son absence vaut « brut », et c'est exactement ce qu'il est. La
			// compatibilite descendante ne tient pas a une intention, elle tient a
			// ce defaut-la, ecrit ici.
			NkString cod, codI;
			(void)g.GetString("codage", cod);
			(void)g.GetString("codageIndices", codI);
			auto codeDe = [](const NkString &s) -> int32 {
				if (s.Empty() || s == NkString("brut"))
					return NK_GEOM_BRUT;
				if (s == NkString("transpose-rle"))
					return NK_GEOM_RLE;
				if (s == NkString("transpose-deflate"))
					return NK_GEOM_DEFLATE;
				return -1; // inconnu : on REFUSE, on ne devine pas
			};
			const int32 cv = codeDe(cod);
			const int32 ci = codeDe(codI);
			if (cv < 0 || ci < 0) {
				if (why)
					*why = NkString("codage inconnu : ") + (cv < 0 ? cod : codI);
				return false;
			}
			if (!NkGeomUnpack(g, "v", cv, (uint32)stride, verts, why)) {
				verts.Clear();
				return false;
			}
			if (verts.Size() != (usize)nv * (usize)stride) {
				verts.Clear();
				if (why)
					*why = "sommets : le compte annonce ne correspond pas aux octets";
				return false;
			}
			if (ni > 0) {
				NkVector<uint8> raw;
				if (!NkGeomUnpack(g, "i", ci, (uint32)sizeof(uint32), raw, why)) {
					verts.Clear();
					return false;
				}
				if (raw.Size() != (usize)ni * sizeof(uint32)) {
					verts.Clear();
					if (why)
						*why = "indices : le compte annonce ne correspond pas aux octets";
					return false;
				}
				indices.Resize((usize)ni);
				for (usize k = 0; k < (usize)ni; ++k) {
					uint32 val = 0u;
					// Recopie OCTET PAR OCTET : le tampon decode n'a aucune garantie
					// d'alignement, et un `reinterpret_cast<uint32*>` dessus serait un
					// acces non aligne -- defini nulle part, et lent la ou il marche.
					for (usize b = 0; b < sizeof(uint32); ++b)
						val |= (uint32)raw[k * sizeof(uint32) + b] << (8u * (uint32)b);
					indices[k] = val;
				}
			}
			if (vcount)
				*vcount = (uint32)nv;
			return true;
		}

	} // namespace nk3d
} // namespace nkentseu
