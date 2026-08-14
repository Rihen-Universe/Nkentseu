//
// NkPdfInfo.h — ce que le DOCUMENT dit de lui-meme : titre, auteur, dates,
// langue.
//
// COUCHE D'ACCESSEURS, pas une extension du lecteur : tout est lu a travers le
// modele d'objets existant (Trailer/Catalog/DictGet/Text). Aucun etat, aucune
// allocation persistante, rien qui puisse changer le comportement de lecture
// des documents deja traites.
//
// PERIMETRE DECIDE PAR MESURE (sondage de 258 PDF, 13/08/2026) :
//   /Info non vide 100 % · /Lang 15 % · /Metadata (XMP) 16 %
// Le XMP est ECARTE : il est present dans 16 % des documents la ou /Info l'est
// dans 100 %, et n'apporte donc rien qu'on n'ait deja — ajouter un parseur XML
// pour une source moins bien couverte serait du travail a perte.
//
#pragma once

#include "NKMedia/Pdf/NkPdf.h"

namespace nkentseu {
	namespace media {
		namespace pdf {

			// Date PDF decomposee. `valide` dit si l'analyse a abouti ; les champs
			// absents gardent leur valeur par defaut plutot que d'invalider le
			// tout — un « D:2019 » seul est une information, pas une erreur.
			struct NkPdfDate {
					bool valide = false;
					int32 annee = 0, mois = 1, jour = 1;
					int32 heure = 0, minute = 0, seconde = 0;
					// Decalage par rapport a UTC, en minutes (negatif a l'ouest).
					// `aFuseau` distingue « UTC+0 declare » de « rien de declare ».
					bool aFuseau = false;
					int32 fuseauMin = 0;
					NkString brut; // tel qu'ecrit dans le document
			};

			// Ce que /Info et le catalogue declarent. Chaines DEJA converties en
			// UTF-8 : l'appelant n'a jamais a connaitre l'encodage d'origine.
			struct NkPdfDocInfo {
					bool aInfo = false; // le trailer portait un /Info exploitable

					NkString titre;		 // /Title
					NkString auteur;	 // /Author
					NkString sujet;		 // /Subject
					NkString motsCles;	 // /Keywords
					NkString createur;	 // /Creator   (l'application d'origine)
					NkString producteur; // /Producer  (le convertisseur PDF)

					NkPdfDate creation;		// /CreationDate
					NkPdfDate modification; // /ModDate

					// /Lang du CATALOGUE (pas de /Info) : « fr », « en-US »...
					// Utile pour classer un fonds multilingue sans deviner.
					NkString langue;
			};

			// ── Conversion des chaines PDF vers UTF-8 ──
			//
			// Une chaine de document peut etre :
			//   - en UTF-16BE, signalee par un indicateur d'ordre FE FF en tete ;
			//   - sinon en PDFDocEncoding, qui n'est PAS du Latin-1 : les plages
			//     0x18-0x1F et 0x80-0x9F portent des signes typographiques la ou
			//     Latin-1 place des caracteres de controle. Confondre les deux rend
			//     un titre en charabia sans qu'aucune erreur ne soit signalee.
			//
			// Rendue publique parce que /Info n'est pas seule concernee : les
			// signets, les destinations nommees et les champs de formulaire portent
			// des chaines de la meme nature.
			NkString NkPdfChaineVersUtf8(const NkPdfDoc &doc, const NkPdfVal &v);

			// Analyse « D:YYYYMMDDHHmmSSOHH'mm' » (§7.9.4). Tout ce qui suit
			// l'annee est facultatif et souvent tronque : on prend ce qui est la
			// et on s'arrete, plutot que de rejeter la date entiere.
			NkPdfDate NkPdfAnalyserDate(const NkString &s);

			// Remplit `out`. Rend false seulement si le document n'est pas charge :
			// un document sans /Info reste un succes, avec `aInfo` a false.
			bool NkPdfLireInfo(const NkPdfDoc &doc, NkPdfDocInfo &out);

		} // namespace pdf
	} // namespace media
} // namespace nkentseu
