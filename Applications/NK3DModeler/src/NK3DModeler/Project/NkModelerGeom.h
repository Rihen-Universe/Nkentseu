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

#ifdef GetObject
#undef GetObject
#endif

namespace nkentseu {
	namespace nk3d {

		// Version du bloc `geometrie`, distincte de celle du fichier d'asset : la
		// forme d'un sommet bougera a son rythme.
		static const int32 kGeomBlockVersion = 1;

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
			g.SetString("v", encoding::base64::NkEncode((const uint8 *)verts,
														(usize)vcount * (usize)vstride)
								 .CStr());
			if (indices && icount > 0u)
				g.SetString("i", encoding::base64::NkEncode((const uint8 *)indices,
														   (usize)icount * sizeof(uint32))
									 .CStr());
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
			NkString v64;
			if (!g.GetString("v", v64) || v64.Empty()) {
				if (why)
					*why = "bloc geometrie sans sommets encodes";
				return false;
			}
			// DEUX PASSES : la premiere MESURE (le decodeur n'ecrit rien quand on lui
			// donne un pointeur nul), la seconde ecrit dans un tampon exactement
			// dimensionne. NkDecode ne borne pas son ecriture -- lui donner un tampon
			// devine serait un depassement.
			usize n = 0;
			const NkStringView vv(v64.CStr(), v64.Size());
			if (!encoding::base64::NkDecode(vv, nullptr, &n) || n == 0u) {
				if (why)
					*why = "sommets encodes illisibles";
				return false;
			}
			if (n != (usize)nv * (usize)stride) {
				if (why)
					*why = "sommets : le compte annonce ne correspond pas aux octets";
				return false;
			}
			verts.Resize(n);
			if (!encoding::base64::NkDecode(vv, verts.Data(), &n)) {
				verts.Clear();
				if (why)
					*why = "sommets : decodage refuse";
				return false;
			}
			if (ni > 0) {
				NkString i64;
				if (!g.GetString("i", i64) || i64.Empty()) {
					verts.Clear();
					if (why)
						*why = "indices annonces mais absents";
					return false;
				}
				usize m = 0;
				const NkStringView iv(i64.CStr(), i64.Size());
				if (!encoding::base64::NkDecode(iv, nullptr, &m) ||
					m != (usize)ni * sizeof(uint32)) {
					verts.Clear();
					if (why)
						*why = "indices : le compte annonce ne correspond pas aux octets";
					return false;
				}
				NkVector<uint8> raw;
				raw.Resize(m);
				if (!encoding::base64::NkDecode(iv, raw.Data(), &m)) {
					verts.Clear();
					if (why)
						*why = "indices : decodage refuse";
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
