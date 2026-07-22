// =============================================================================
// NKMedia/NkMediaDemux.h
// -----------------------------------------------------------------------------
// Brique 2 de NKMedia : EXTRACTION DES PAQUETS ENCODÉS d'une piste audio.
// MP4/ISOBMFF : reconstruit les échantillons via les tables stbl (stsz/stco/co64/
// stsc/stts + mdhd). WebM/Matroska : SimpleBlock/Block des Clusters. Chaque paquet
// pointe dans le buffer source (offset+size) + timestamp approx. C'est l'entrée des
// décodeurs (AAC/Opus, briques 3-4). Zero-STL, namespace nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMedia/NkMediaProbe.h"

namespace nkentseu {
	namespace media {

		// Un paquet encodé (pointe dans le buffer d'origine ; ne copie pas).
		struct NkMediaPacket {
				usize offset = 0;		// position dans le buffer source
				usize size = 0;			// taille en octets
				int64 timestampMs = 0;	// horodatage approx (ms depuis le début)
				int64 granule = -1;		// OGG : granulepos de la page où le paquet se termine
										// (dernier paquet complet de la page), -1 sinon
				int64 discardPaddingNs = 0; // WebM : DiscardPadding (0x75A2) du BlockGroup —
											// nanosecondes à JETER en FIN de bloc décodé
											// (padding d'encodeur du dernier paquet Opus)
		};

		struct NkMediaDemux {
			public:
				// Extrait les paquets de la PREMIÈRE piste audio. `data/size` = fichier complet en mémoire,
				// `info` = résultat de NkMediaProbe::Probe sur le même buffer. Les paquets référencent `data`
				// (rester valide tant qu'on les lit). Renvoie false si conteneur non supporté / rien trouvé.
				static bool ExtractAudioPackets(const uint8 *data, usize size, const NkMediaInfo &info,
												NkVector<NkMediaPacket> &out);

				// Variante fichier : lit le fichier, probe, extrait. `outBytes` reçoit le buffer (à garder
				// vivant car les paquets pointent dedans), `outInfo` l'info conteneur.
				static bool ExtractAudioPacketsFile(const char *path, NkVector<nk_uint8> &outBytes,
													NkMediaInfo &outInfo, NkVector<NkMediaPacket> &out);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
