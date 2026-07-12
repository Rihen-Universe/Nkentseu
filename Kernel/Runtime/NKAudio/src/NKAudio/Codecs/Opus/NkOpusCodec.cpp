/**
 * @File    NkOpusCodec.cpp
 * @Brief   Decodeur Opus (.opus / Ogg-Opus) pour NKAudio — voir NkOpusCodec.h.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - Free to use and modify
 */

#include "NkOpusCodec.h"

#include "NKMedia/Codecs/Opus/NkOpusFile.h"
#include "NKMemory/NkFunction.h"

namespace nkentseu {
	namespace audio {

		AudioSample NkOpusCodec::Decode(const uint8 *data, usize size, memory::NkAllocator *allocator) noexcept {
			AudioSample result{};
			if (data == nullptr || size == 0)
				return result;

			NkVector<int16> pcm;
			int32 channels = 0;
			int32 sampleRate = 0;
			if (!media::NkOpusFile::Decode(data, size, pcm, channels, sampleRate) || pcm.IsEmpty())
				return result;

			// Conversion int16 -> float32 normalise (pipeline NKAudio standard).
			const usize sampleCount = pcm.Size();
			float32 *outData = (float32 *)memory::NkAlloc(sampleCount * sizeof(float32), allocator, sizeof(float32));
			if (outData == nullptr)
				return result;
			for (usize i = 0; i < sampleCount; ++i)
				outData[i] = (float32)pcm[i] / 32768.0f;

			result.data = outData;
			result.frameCount = sampleCount / (usize)channels;
			result.sampleRate = sampleRate;
			result.channels = channels;
			result.format = AudioFormat::OPUS;
			result.mAllocator = allocator;
			return result;
		}

	} // namespace audio
} // namespace nkentseu
