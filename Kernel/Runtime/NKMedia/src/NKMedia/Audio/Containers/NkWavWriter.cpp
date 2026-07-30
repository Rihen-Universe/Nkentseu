// =============================================================================
// NKMedia/Audio/Containers/NkWavWriter.cpp — muxer WAV/RIFF from-scratch.
// =============================================================================
#include "NKMedia/Audio/Containers/NkWavWriter.h"
// NkFourCC (package FourCC RIFF en little-endian) : générique, déjà défini pour
// NkAviWriter (même famille de conteneur RIFF) — réutilisé tel quel plutôt que
// dupliqué.
#include "NKMedia/Video/Containers/NkAviWriter.h"

namespace nkentseu {
	namespace media {

		void NkWavWriter::PutU32(uint32 v) {
			uint8 b[4] = {(uint8)(v & 0xFF), (uint8)((v >> 8) & 0xFF), (uint8)((v >> 16) & 0xFF),
						  (uint8)((v >> 24) & 0xFF)};
			mFile.Write(b, 4);
		}

		void NkWavWriter::PutU16(uint16 v) {
			uint8 b[2] = {(uint8)(v & 0xFF), (uint8)((v >> 8) & 0xFF)};
			mFile.Write(b, 2);
		}

		void NkWavWriter::PutBytes(const void *p, usize n) {
			mFile.Write(p, n);
		}

		void NkWavWriter::PatchU32(nk_int64 pos, uint32 v) {
			const nk_int64 cur = mFile.Tell();
			mFile.Seek(pos, NkSeekOrigin::NK_BEGIN);
			PutU32(v);
			mFile.Seek(cur, NkSeekOrigin::NK_BEGIN);
		}

		bool NkWavWriter::Open(const char *path, int32 sampleRate, int32 channels, int32 bitsPerSample,
							   bool isFloat) {
			if (sampleRate <= 0 || channels <= 0 || bitsPerSample <= 0 || (bitsPerSample % 8) != 0)
				return false;
			const uint32 mode =
				(uint32)NkFileMode::NK_WRITE | (uint32)NkFileMode::NK_BINARY | (uint32)NkFileMode::NK_TRUNCATE;
			if (!mFile.Open(path, (NkFileMode)mode))
				return false;

			mDataSize = 0;
			const uint16 audioFormat = isFloat ? 3 : 1; // WAVE_FORMAT_PCM=1, WAVE_FORMAT_IEEE_FLOAT=3
			const uint16 blockAlign = (uint16)(channels * (bitsPerSample / 8));
			const uint32 byteRate = (uint32)sampleRate * blockAlign;

			// ---- RIFF 'WAVE' ----
			PutU32(NkFourCC('R', 'I', 'F', 'F'));
			mRiffSizePos = mFile.Tell();
			PutU32(0); // taille RIFF (rapiécée à Close)
			PutU32(NkFourCC('W', 'A', 'V', 'E'));

			// ---- 'fmt ' (16 octets, PCM/IEEE float canonique — pas d'extension) ----
			PutU32(NkFourCC('f', 'm', 't', ' '));
			PutU32(16);
			PutU16(audioFormat);
			PutU16((uint16)channels);
			PutU32((uint32)sampleRate);
			PutU32(byteRate);
			PutU16(blockAlign);
			PutU16((uint16)bitsPerSample);

			// ---- 'data' (taille rapiécée à Close, échantillons écrits par WriteSamples) ----
			PutU32(NkFourCC('d', 'a', 't', 'a'));
			mDataSizePos = mFile.Tell();
			PutU32(0);

			return mFile.IsOpen();
		}

		bool NkWavWriter::WriteSamples(const void *data, usize size) {
			if (!mFile.IsOpen() || data == nullptr || size == 0)
				return false;
			PutBytes(data, size);
			mDataSize += size;
			return true;
		}

		bool NkWavWriter::Close() {
			if (!mFile.IsOpen())
				return false;

			// RIFF 'data' doit être aligné pair — un octet de bourrage si la taille totale
			// écrite est impaire (§spec RIFF, comme dans NkAviWriter).
			if (mDataSize & 1) {
				const uint8 pad = 0;
				mFile.Write(&pad, 1);
			}
			const nk_int64 fileEndPadded = mFile.Tell();

			PatchU32(mDataSizePos, (uint32)mDataSize);
			PatchU32(mRiffSizePos, (uint32)(fileEndPadded - (mRiffSizePos + 4)));

			mFile.Close();
			return true;
		}

		namespace {
			inline uint32 RdU32LE(const uint8 *p) {
				return (uint32)p[0] | ((uint32)p[1] << 8) | ((uint32)p[2] << 16) | ((uint32)p[3] << 24);
			}
			inline uint16 RdU16LE(const uint8 *p) {
				return (uint16)((uint32)p[0] | ((uint32)p[1] << 8));
			}
		} // namespace

		bool NkWavWriter::SelfTest() {
			const char *path = "nkwavwriter_selftest.wav";
			const int32 sampleRate = 8000, channels = 1, bitsPerSample = 16;
			const int32 n = 8;
			int16 samples[n] = {0, 1000, 2000, 3000, -1000, -2000, -3000, 32767};

			NkWavWriter wr;
			if (!wr.Open(path, sampleRate, channels, bitsPerSample))
				return false;
			if (!wr.WriteSamples(samples, sizeof(samples)))
				return false;
			if (!wr.Close())
				return false;

			NkVector<uint8> bytes = NkFile::ReadAllBytes(path);
			const usize expectedDataSize = sizeof(samples);
			const usize expectedFileSize = 44 + expectedDataSize; // pas de bourrage (taille paire)
			if (bytes.Size() != expectedFileSize)
				return false;
			const uint8 *b = bytes.Data();

			if (b[0] != 'R' || b[1] != 'I' || b[2] != 'F' || b[3] != 'F')
				return false;
			if (RdU32LE(b + 4) != (uint32)(expectedFileSize - 8))
				return false;
			if (b[8] != 'W' || b[9] != 'A' || b[10] != 'V' || b[11] != 'E')
				return false;
			if (b[12] != 'f' || b[13] != 'm' || b[14] != 't' || b[15] != ' ')
				return false;
			if (RdU32LE(b + 16) != 16) // taille chunk fmt (canonique, sans extension)
				return false;
			if (RdU16LE(b + 20) != 1) // WAVE_FORMAT_PCM
				return false;
			if (RdU16LE(b + 22) != (uint16)channels)
				return false;
			if (RdU32LE(b + 24) != (uint32)sampleRate)
				return false;
			const uint32 expectedBlockAlign = (uint32)(channels * (bitsPerSample / 8));
			if (RdU32LE(b + 28) != (uint32)sampleRate * expectedBlockAlign) // byteRate
				return false;
			if (RdU16LE(b + 32) != (uint16)expectedBlockAlign)
				return false;
			if (RdU16LE(b + 34) != (uint16)bitsPerSample)
				return false;
			if (b[36] != 'd' || b[37] != 'a' || b[38] != 't' || b[39] != 'a')
				return false;
			if (RdU32LE(b + 40) != (uint32)expectedDataSize)
				return false;

			// Échantillons bit-exacts (round-trip, écrits en little-endian par la CPU x86/x64
			// hôte — cohérent avec le format WAV qui est nativement little-endian).
			for (int32 i = 0; i < n; ++i) {
				const int16 got = (int16)RdU16LE(b + 44 + i * 2);
				if (got != samples[i])
					return false;
			}
			return true;
		}

	} // namespace media
} // namespace nkentseu
