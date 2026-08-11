//
// NkPdf.h — Lecture de documents PDF (structure + acces aux pages).
//
// PERIMETRE (decide par MESURE sur un corpus de 95 PDF reels, 30/07/2026 —
// voir ROADMAP « Lecteur PDF ») : on implemente ce que le corpus contient
// vraiment, pas la specification entiere.
//   FlateDecode 98,9 % · flux d'index + flux d'objets 35 % · TrueType 59 % ·
//   Type0/CID 50 % · JPEG 24 % · CFF 3 %.
// Hors v1, mesure a l'appui : Type 1 brut (2,1 %), chiffrement (2,1 %),
// LZW/CCITT/JBIG2/JPX (0 %).
//
// LECTURE SEULE. Aucune ecriture, aucune modification de document.
//
// Modele d'objets en ARENE plutot qu'arborescence de pointeurs : un objet PDF
// est recursif (un tableau contient des dictionnaires qui contiennent des
// tableaux...), et un `NkVector<NkPdfVal>` membre de `NkPdfVal` exigerait un
// type incomplet. Les enfants vivent donc dans des tableaux plats du document
// et les objets ne portent que des INDEX. Effet de bord utile : aucune
// allocation par noeud, et la duree de vie est celle du document.
//
#pragma once

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {
		namespace pdf {

			// ── Nature d'un objet PDF (§7.3 de la specification) ──
			enum NkPdfKind : uint8 {
				NK_PDF_NULL = 0,
				NK_PDF_BOOL,
				NK_PDF_INT,
				NK_PDF_REAL,
				NK_PDF_STRING, // chaine litterale ou hexadecimale, deja decodee
				NK_PDF_NAME,   // /Nom, sans la barre oblique, sequences #xx resolues
				NK_PDF_ARRAY,
				NK_PDF_DICT,
				NK_PDF_STREAM, // dictionnaire + donnees brutes
				NK_PDF_REF	   // « 12 0 R » : reference indirecte, a resoudre
			};

			// Valeur PDF. Compacte et copiable : tout le contenu variable vit dans
			// les tableaux du document, referencé par index.
			struct NkPdfVal {
					uint8 kind = NK_PDF_NULL;
					double num = 0.0; // INT / REAL / BOOL

					// STRING, NAME  : a = position dans le pool, b = longueur
					// ARRAY         : a = 1er index dans mKids, b = nombre d'elements
					// DICT, STREAM  : a = 1re entree dans mEnts, b = nombre d'entrees
					// REF           : a = numero d'objet, b = numero de generation
					int32 a = 0, b = 0;

					// STREAM : emplacement des octets BRUTS (encore filtres) dans le
					// tampon source. Le decodage est explicite (`DecodeStream`) pour ne
					// jamais decompresser un flux dont on n'a pas besoin.
					usize rawOff = 0, rawLen = 0;

					bool IsNull() const { return kind == NK_PDF_NULL; }
					bool IsNum() const { return kind == NK_PDF_INT || kind == NK_PDF_REAL; }
					bool IsDictLike() const { return kind == NK_PDF_DICT || kind == NK_PDF_STREAM; }
			};

			// Profondeur maximale d'imbrication et de chainage. Un PDF malforme peut
			// contenir des cycles (/Parent, /Prev, references croisees) : toutes les
			// descentes recursives sont bornees par cette valeur.
			static constexpr int32 kMaxDepth = 64;

			// Etat de chargement — distingue les echecs pour pouvoir le DIRE a
			// l'utilisateur plutot que d'afficher une page blanche.
			enum NkPdfStatus : uint8 {
				NK_PDF_OK = 0,
				NK_PDF_ERR_OPEN,	  // fichier illisible
				NK_PDF_ERR_SIGNATURE, // pas un PDF (%PDF- absent)
				NK_PDF_ERR_XREF,	  // index introuvable ou incoherent
				NK_PDF_ERR_ENCRYPTED, // /Encrypt : hors perimetre v1, dit franchement
				NK_PDF_ERR_STRUCTURE  // catalogue ou arbre des pages inexploitable
			};

			// Document charge. Possede le tampon du fichier et l'arene d'objets.
			class NkPdfDoc {
				public:
					NkPdfDoc() = default;
					~NkPdfDoc() { Close(); }

					NkPdfDoc(const NkPdfDoc &) = delete;
					NkPdfDoc &operator=(const NkPdfDoc &) = delete;

					// Charge un document. Renvoie NK_PDF_OK ou la raison de l'echec.
					NkPdfStatus Open(const char *path);
					void Close();

					// Nom : surtout PAS `Status()`. X11 definit `#define Status int`, ce
					// qui transformait cette ligne en `NkPdfStatus int() const`. Retirer
					// la macro n'est pas une option : les en-tetes X11 s'en servent comme
					// type (details dans NkX11Clean.h). C'est a notre nom de ceder.
					NkPdfStatus StatusCode() const { return mStatus; }
					// Message pret a afficher, en francais, pour l'etat courant.
					const char *StatusText() const;

					int32 PageCount() const { return static_cast<int32>(mPages.Size()); }

					// Dictionnaire de la page `i` (0-based). Objet nul si hors bornes.
					NkPdfVal Page(int32 i) const;

					// Boite d'affichage de la page, en points PDF (1/72 pouce), attribut
					// HERITE de l'arbre des pages si absent de la page elle-meme.
					// Renvoie false si introuvable (l'appelant doit alors se rabattre sur
					// un format par defaut plutot que de rendre une page de taille nulle).
					bool PageMediaBox(int32 i, double *x0, double *y0, double *x1, double *y1) const;

					// Rotation d'affichage (0/90/180/270), heritee elle aussi.
					int32 PageRotate(int32 i) const;

					// ── Navigation dans les objets ──

					// Suit une reference indirecte jusqu'a l'objet reel. Les objets
					// directs sont renvoyes tels quels. Borne la profondeur : un PDF
					// malforme peut contenir un cycle de references.
					NkPdfVal Resolve(const NkPdfVal &v) const;

					// Valeur d'une cle de dictionnaire, DEJA resolue. Objet nul si absente.
					NkPdfVal DictGet(const NkPdfVal &dict, const char *key) const;

					// Element `i` d'un tableau, DEJA resolu.
					NkPdfVal ArrayAt(const NkPdfVal &arr, int32 i) const;
					int32 ArraySize(const NkPdfVal &arr) const { return arr.kind == NK_PDF_ARRAY ? arr.b : 0; }

					// Contenu texte d'un NAME ou d'un STRING (pointeur dans le pool,
					// valide tant que le document vit). Jamais nullptr.
					const char *Text(const NkPdfVal &v, int32 *len = nullptr) const;
					bool NameIs(const NkPdfVal &v, const char *name) const;

					double Num(const NkPdfVal &v, double def = 0.0) const {
						return v.IsNum() ? v.num : def;
					}

					// Decode les octets d'un flux en appliquant sa chaine /Filter et ses
					// /DecodeParms. Renvoie false si un filtre n'est pas supporte —
					// l'appelant doit alors le signaler, jamais faire semblant.
					// `out` est vide au depart et rempli en cas de succes.
					bool DecodeStream(const NkPdfVal &stream, NkVector<uint8> &out) const;

					// Nom du 1er filtre non supporte rencontre (diagnostic), ou "".
					const NkString &UnsupportedFilter() const { return mUnsupported; }

				private:
					// ── Analyse lexicale/syntaxique ──
					struct Lexer;
					int32 ParseValueAt(usize &p, int32 depth); // renvoie un index dans mVals
					void SkipWs(usize &p) const;

					// ── Index (xref) ──
					bool LoadXref();
					bool LoadXrefAt(usize pos, int32 depth);
					bool LoadXrefTable(usize &p, int32 depth);	// « xref » classique
					bool LoadXrefStream(usize &p, int32 depth); // flux /Type /XRef (PDF 1.5+)
					bool LoadObjStm(int32 objNum);				// flux d'objets compresses

					// Charge (paresseusement) l'objet `num` et renvoie son index dans mVals.
					int32 LoadObject(int32 num) const;

					// Aligne mCache/mLoading sur la taille de mXref. INDISPENSABLE :
					// l'analyse d'un flux d'index resout deja des references (un /Length
					// indirect) AVANT la fin de la construction de l'index — sans cet
					// alignement, LoadObject indexait des tableaux encore vides.
					void EnsureTables() const;

					// Variantes « par index » : renvoient un index dans mVals au lieu
					// d'une copie de valeur. Necessaires pour parcourir l'arbre des pages,
					// ou l'on doit MEMORISER quel objet est une page (une copie de valeur
					// ne permet pas de retrouver son index sans une recherche fragile).
					int32 ResolveIdx(int32 rawIdx) const;
					int32 DictGetIdx(const NkPdfVal &dict, const char *key) const;
					int32 ArrayRawAt(const NkPdfVal &arr, int32 i) const;

					// ── Arbre des pages ──
					bool BuildPageList();
					void WalkPages(int32 nodeIdx, int32 depth);

					// Attribut herite : remonte la chaine /Parent depuis la page.
					NkPdfVal Inherited(int32 pageIdx, const char *key) const;

					// ── Filtres ──
					bool ApplyFilter(const char *name, int32 nameLen, const NkPdfVal &parms,
									 const NkVector<uint8> &in, NkVector<uint8> &out) const;
					static bool Inflate(const uint8 *in, usize inSz, NkVector<uint8> &out);
					static bool Ascii85(const uint8 *in, usize inSz, NkVector<uint8> &out);
					static bool AsciiHex(const uint8 *in, usize inSz, NkVector<uint8> &out);
					static bool RunLength(const uint8 *in, usize inSz, NkVector<uint8> &out);
					// Predicteurs PNG/TIFF de /DecodeParms — INDISPENSABLE : les flux
					// d'index les utilisent presque toujours, sans quoi la table est du
					// bruit et le document parait corrompu.
					bool Unpredict(const NkPdfVal &parms, NkVector<uint8> &data) const;

					int32 PoolPush(const char *s, int32 n);

					// ── Donnees ──
					NkVector<uint8> mBuf;	// contenu integral du fichier
					NkString mPool;			// noms et chaines, concatenes
					NkString mUnsupported;	// 1er filtre non supporte rencontre
					NkPdfStatus mStatus = NK_PDF_ERR_OPEN;

					// Arene. `mutable` : le chargement d'un objet est PARESSEUX, donc
					// declenche par des accesseurs const (Resolve/DictGet). L'etat
					// logique du document ne change pas, seul le cache se remplit.
					mutable NkVector<NkPdfVal> mVals;
					mutable NkVector<int32> mKids; // elements des tableaux
					struct Ent {
							int32 keyOff = 0, keyLen = 0, val = 0;
					};
					mutable NkVector<Ent> mEnts; // entrees des dictionnaires

					// Table des objets : offset dans le fichier, ou (flux d'objets, rang).
					struct XEntry {
							usize off = 0;	  // type 1 : position de « N G obj »
							int32 stmNum = 0; // type 2 : numero du flux d'objets porteur
							int32 stmIdx = 0; // type 2 : rang dans ce flux
							uint8 type = 0;	  // 0 libre, 1 direct, 2 dans un flux d'objets
					};
					mutable NkVector<XEntry> mXref;
					mutable NkVector<int32> mCache;	 // numero d'objet -> index mVals (-1 = pas encore lu)
					mutable NkVector<uint8> mLoading; // garde anti-recursion par objet

					NkVector<usize> mXrefSeen;		  // offsets xref deja charges (anti-cycle /Prev)
				int32 mTrailer = -1;			  // index du dictionnaire trailer
					int32 mRoot = -1;				  // index du catalogue
					NkVector<int32> mPages;			  // index des dictionnaires de page, dans l'ordre
					NkVector<int32> mPageParent;	  // parent de chaque page (attributs herites)
					mutable NkVector<int32> mObjStmDone; // flux d'objets deja depaquetes
			};

		} // namespace pdf
	} // namespace media
} // namespace nkentseu
