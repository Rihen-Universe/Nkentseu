/**
 * @File    NkOGGVorbisCodec.cpp
 * @Brief   Decodeur OGG Vorbis pour Nkentseu (float32 interleaved).
 *
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - Free to use and modify
 *
 * @Description
 *  Decodeur complet du format Ogg Vorbis (.ogg). Pipeline :
 *   1. Parser Ogg pages (sync, segments, multi-page packets)
 *   2. Vorbis identification + comment + setup headers
 *   3. Audio packets : mode -> floor curve -> residue -> IMDCT -> overlap-add
 *  Decode -> AudioSample (float32 interleaved, compatible AudioLoader::Free).
 *
 * @Reference
 *  Spec Vorbis I : https://www.xiph.org/vorbis/doc/Vorbis_I_spec.html
 *
 * @MultiPlatform
 *  100% C++17 standard. Aucune dependance OS, aucune syscall.
 *  Compile identique sur Windows / Linux / macOS / iOS / Android / Web.
 *
 * @ThreadSafety
 *  Une instance de NkVorbisDecoder n'est pas thread-safe. Pour decoder
 *  plusieurs flux en parallele, creer plusieurs instances independantes.
 */

#include "NKAudio/Codecs/OGG/NkOGGVorbisCodec.h"
#include "NKCore/NkTypes.h"
#include "NKCore/NkMacros.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKPlatform/NkArchDetect.h"
#include "NKPlatform/NkPlatformDetect.h"
#include "NKPlatform/NkCompilerDetect.h"
#include "NKPlatform/NkEndianness.h"
#include "NKPlatform/NkPlatformInline.h"
#include "NKMemory/NkAllocator.h"
#include <climits>  // UINT_MAX (non transitivement inclus par OHOS clang)
#include "NKLogger/NkLog.h"
#include <cstring>

// Remappe `assert` du code Vorbis vers NKENTSEU_ASSERT (assert Nkentseu unifie).
#define assert(cond) NKENTSEU_ASSERT(cond)

// Importer les types primitifs Nkentseu au scope global pour stbVorbis (qui
// utilise int8/int16/int32, uint8/uint16/uint32, float32/float64 sans qualifier).
using ::nkentseu::float32;
using ::nkentseu::float64;
using ::nkentseu::int16;
using ::nkentseu::int32;
using ::nkentseu::int64;
using ::nkentseu::int8;
using ::nkentseu::uint16;
using ::nkentseu::uint32;
using ::nkentseu::uint64;
using ::nkentseu::uint8;
using ::nkentseu::usize;

// Configuration interne du decodeur Vorbis.
#define NK_VORBIS_NO_STDIO 1
#define NK_VORBIS_NO_PUSHDATA_API 1

// ═════════════════════════════════════════════════════════════════════════════
//  Decodeur Vorbis Nkentseu - implementation interne (namespace vorbis)
//
//  Limitations connues :
//   - NkFloor 0 (LSP, deprecated 2004) non supporte
//   - Sample positions 32 bits (max seek ~6 heures a 192 kHz)
//   - Concat de plusieurs streams Vorbis non supporte
// ═════════════════════════════════════════════════════════════════════════════

#ifndef NK_VORBIS_DECODER_HEADER_H
#define NK_VORBIS_DECODER_HEADER_H

#if defined(NK_VORBIS_NO_CRT) && !defined(NK_VORBIS_NO_STDIO)
#define NK_VORBIS_NO_STDIO 1
#endif

#ifndef NK_VORBIS_NO_STDIO
#include <stdio.h>
#endif

namespace nkentseu {
	namespace audio {
		namespace vorbis {

			///////////   THREAD SAFETY

			// Individual NkVorbisDecoder* handles are not thread-safe; you cannot decode from
			// them from multiple threads at the same time. However, you can have multiple
			// NkVorbisDecoder* handles and decode from them independently in multiple thrads.

			///////////   MEMORY ALLOCATION

			// normally NkVorbisDecoder uses malloc() to allocate memory at startup,
			// and alloca() to allocate temporary memory during a frame on the
			// stack. (Memory consumption will depend on the amount of setup
			// data in the file and how you set the compile flags for speed
			// vs. size. In my test files the maximal-size usage is ~150KB.)
			//
			// You can modify the wrapper functions in the source (NkSetupMalloc,
			// NkSetupTempMalloc, tempMalloc) to change this behavior, or you
			// can use a simpler allocation model: you pass in a buffer from
			// which NkVorbisDecoder will allocate _all_ its memory (including the
			// temp memory). "open" may fail with a NK_VORBIS_OUTOFMEM if you
			// do not pass in enough data; there is no way to determine how
			// much you do need except to succeed (at which point you can
			// query getInfo to find the exact amount required. yes I know
			// this is lame).
			//
			// If you pass in a non-NULL buffer of the type below, allocation
			// will occur from it as described above. Otherwise just pass NULL
			// to use malloc()/alloca()

			typedef struct {
				char *allocBuffer;
				int32 allocBufferLengthInBytes;
			} NkVorbisAllocator;

			///////////   FUNCTIONS USEABLE WITH ALL INPUT MODES

			typedef struct NkVorbisDecoder NkVorbisDecoder;

			typedef struct {
				uint32 sampleRate;
				int32 channels;

				uint32 setupMemoryRequired;
				uint32 setupTempMemoryRequired;
				uint32 tempMemoryRequired;

				int32 maxFrameSize;
			} NkVorbisInfo;

			typedef struct {
				char *vendor;

				int32 commentListLength;
				char **commentList;
			} NkVorbisComment;

			// get general information about the file
			extern NkVorbisInfo NkVorbisGetInfo(NkVorbisDecoder *f);

			// get ogg comments
			extern NkVorbisComment NkVorbisGetComment(NkVorbisDecoder *f);

			// get the last error detected (clears it, too)
			extern int32 NkVorbisGetError(NkVorbisDecoder *f);

			// close an ogg vorbis file and free all memory in use
			extern void NkVorbisClose(NkVorbisDecoder *f);

			// this function returns the offset (in samples) from the beginning of the
			// file that will be returned by the next decode, if it is known, or -1
			// otherwise. after a flushPushdata() call, this may take a while before
			// it becomes valid again.
			// NOT WORKING YET after a seek with PULLDATA API
			extern int32 NkVorbisGetSampleOffset(NkVorbisDecoder *f);

			// returns the current seek point within the file, or offset from the beginning
			// of the memory buffer. In pushdata mode it returns 0.
			extern uint32 NkVorbisGetFileOffset(NkVorbisDecoder *f);

			///////////   PUSHDATA API

#ifndef NK_VORBIS_NO_PUSHDATA_API

			// this API allows you to get blocks of data from any source and hand
			// them to NkVorbisDecoder. you have to buffer them; NkVorbisDecoder will tell
			// you how much it used, and you have to give it the rest next time;
			// and NkVorbisDecoder may not have enough data to work with and you will
			// need to give it the same data again PLUS more. Note that the Vorbis
			// specification does not bound the size of an individual frame.

			extern NkVorbisDecoder *NkVorbisOpenPushdata(const uint8 *datablock, int32 datablockLengthInBytes,
														 int32 *datablockMemoryConsumedInBytes, int32 *error,
														 const NkVorbisAllocator *allocBuffer);
			// create a vorbis decoder by passing in the initial data block containing
			//    the ogg&vorbis headers (you don't need to do parse them, just provide
			//    the first N bytes of the file--you're told if it's not enough, see below)
			// on success, returns an NkVorbisDecoder *, does not set error, returns the amount of
			//    data parsed/consumed on this call in *datablockMemoryConsumedInBytes;
			// on failure, returns NULL on error and sets *error, does not change *datablockMemoryConsumed
			// if returns NULL and *error is NK_VORBIS_NEED_MORE_DATA, then the input block was
			//       incomplete and you need to pass in a larger block from the start of the file

			extern int32
			NkVorbisDecodeFramePushdata(NkVorbisDecoder *f, const uint8 *datablock, int32 datablockLengthInBytes,
										int32 *channels,   // place to write number of float32 * buffers
										float32 ***output, // place to write float32 ** array of float32 * buffers
										int32 *samples	   // place to write number of output samples
			);
			// decode a frame of audio sample data if possible from the passed-in data block
			//
			// return value: number of bytes we used from datablock
			//
			// possible cases:
			//     0 bytes used, 0 samples output (need more data)
			//     N bytes used, 0 samples output (resynching the stream, keep going)
			//     N bytes used, M samples output (one frame of data)
			// note that after opening a file, you will ALWAYS get one N-bytes,0-sample
			// frame, because Vorbis always "discards" the first frame.
			//
			// Note that on resynch, NkVorbisDecoder will rarely consume all of the buffer,
			// instead only datablockLengthInBytes-3 or less. This is because it wants
			// to avoid missing parts of a page header if they cross a datablock boundary,
			// without writing state-machiney code to record a partial detection.
			//
			// The number of channels returned are stored in *channels (which can be
			// NULL--it is always the same as the number of channels reported by
			// getInfo). *output will contain an array of float32* buffers, one per
			// channel. In other words, (*output)[0][0] contains the first sample from
			// the first channel, and (*output)[1][0] contains the first sample from
			// the second channel.
			//
			// *output points into NkVorbisDecoder's internal output buffer storage; these
			// buffers are owned by NkVorbisDecoder and application code should not free
			// them or modify their contents. They are transient and will be overwritten
			// once you ask for more data to get decoded, so be sure to grab any data
			// you need before then.

			extern void NkVorbisFlushPushdata(NkVorbisDecoder *f);
// inform NkVorbisDecoder that your next datablock will not be contiguous with
// previous ones (e.g. you've seeked in the data); future attempts to decode
// frames will cause NkVorbisDecoder to resynchronize (as noted above), and
// once it sees a valid Ogg page (typically 4-8KB, as large as 64KB), it
// will begin decoding the _next_ frame.
//
// if you want to seek using pushdata, you need to seek in your file, then
// call NkVorbisFlushPushdata(), then start calling decoding, then once
// decoding is returning you data, call NkVorbisGetSampleOffset, and
// if you don't like the result, seek your file again and repeat.
#endif

			//////////   PULLING INPUT API

#ifndef NK_VORBIS_NO_PULLDATA_API
			// This API assumes NkVorbisDecoder is allowed to pull data from a source--
			// either a block of memory containing the _entire_ vorbis stream, or a
			// FILE * that you or it create, or possibly some other reading mechanism
			// if you go modify the source to replace the FILE * case with some kind
			// of callback to your code. (But if you don't support seeking, you may
			// just want to go ahead and use pushdata.)

#if !defined(NK_VORBIS_NO_STDIO) && !defined(NK_VORBIS_NO_INTEGER_CONVERSION)
			extern int32 NkVorbisDecodeFilename(const char *filename, int32 *channels, int32 *sampleRate,
												int16 **output);
#endif
#if !defined(NK_VORBIS_NO_INTEGER_CONVERSION)
			extern int32 NkVorbisDecodeMemory(const uint8 *mem, int32 len, int32 *channels, int32 *sampleRate,
											  int16 **output);
#endif
			// decode an entire file and output the data interleaved into a malloc()ed
			// buffer stored in *output. The return value is the number of samples
			// decoded, or -1 if the file could not be opened or was not an ogg vorbis file.
			// When you're done with it, just free() the pointer returned in *output.

			extern NkVorbisDecoder *NkVorbisOpenMemory(const uint8 *data, int32 len, int32 *error,
													   const NkVorbisAllocator *allocBuffer);
			// create an ogg vorbis decoder from an ogg vorbis stream in memory (note
			// this must be the entire stream!). on failure, returns NULL and sets *error

#ifndef NK_VORBIS_NO_STDIO
			extern NkVorbisDecoder *NkVorbisOpenFilename(const char *filename, int32 *error,
														 const NkVorbisAllocator *allocBuffer);
			// create an ogg vorbis decoder from a filename via fopen(). on failure,
			// returns NULL and sets *error (possibly to NK_VORBIS_FILE_OPEN_FAILURE).

			extern NkVorbisDecoder *NkVorbisOpenFile(FILE *f, int32 closeHandleOnClose, int32 *error,
													 const NkVorbisAllocator *allocBuffer);
			// create an ogg vorbis decoder from an open FILE *, looking for a stream at
			// the _current_ seek point (ftell). on failure, returns NULL and sets *error.
			// note that NkVorbisDecoder must "own" this stream; if you seek it in between
			// calls to NkVorbisDecoder, it will become confused. Moreover, if you attempt to
			// perform NkVorbisSeek*() operations on this file, it will assume it
			// owns the _entire_ rest of the file after the start point. Use the next
			// function, NkVorbisOpenFileSection(), to limit it.

			extern NkVorbisDecoder *NkVorbisOpenFileSection(FILE *f, int32 closeHandleOnClose, int32 *error,
															const NkVorbisAllocator *allocBuffer, uint32 len);
// create an ogg vorbis decoder from an open FILE *, looking for a stream at
// the _current_ seek point (ftell); the stream will be of length 'len' bytes.
// on failure, returns NULL and sets *error. note that NkVorbisDecoder must "own"
// this stream; if you seek it in between calls to NkVorbisDecoder, it will become
// confused.
#endif

			extern int32 NkVorbisSeekFrame(NkVorbisDecoder *f, uint32 sampleNumber);
			extern int32 NkVorbisSeek(NkVorbisDecoder *f, uint32 sampleNumber);
			// these functions seek in the Vorbis file to (approximately) 'sampleNumber'.
			// after calling seekFrame(), the next call to getFrame*() will include
			// the specified sample. after calling NkVorbisSeek(), the next call to
			// NkVorbisGetSamples* will start with the specified sample. If you
			// do not need to seek to EXACTLY the target sample when using getSamples*,
			// you can also use seekFrame().

			extern int32 NkVorbisSeekStart(NkVorbisDecoder *f);
			// this function is equivalent to NkVorbisSeek(f,0)

			extern uint32 NkVorbisStreamLengthInSamples(NkVorbisDecoder *f);
			extern float32 NkVorbisStreamLengthInSeconds(NkVorbisDecoder *f);
			// these functions return the total length of the vorbis stream

			extern int32 NkVorbisGetFrameFloat(NkVorbisDecoder *f, int32 *channels, float32 ***output);
			// decode the next frame and return the number of samples. the number of
			// channels returned are stored in *channels (which can be NULL--it is always
			// the same as the number of channels reported by getInfo). *output will
			// contain an array of float32* buffers, one per channel. These outputs will
			// be overwritten on the next call to NkVorbisGetFrame*.
			//
			// You generally should not intermix calls to NkVorbisGetFrame*()
			// and NkVorbisGetSamples*(), since the latter calls the former.

#ifndef NK_VORBIS_NO_INTEGER_CONVERSION
			extern int32 NkVorbisGetFrameShortInterleaved(NkVorbisDecoder *f, int32 numC, int16 *buffer,
														  int32 numShorts);
			extern int32 NkVorbisGetFrameShort(NkVorbisDecoder *f, int32 numC, int16 **buffer, int32 numSamples);
#endif
			// decode the next frame and return the number of *samples* per channel.
			// Note that for interleaved data, you pass in the number of shorts (the
			// size of your array), but the return value is the number of samples per
			// channel, not the total number of samples.
			//
			// The data is coerced to the number of channels you request according to the
			// channel coercion rules (see below). You must pass in the size of your
			// buffer(s) so that NkVorbisDecoder will not overwrite the end of the buffer.
			// The maximum buffer size needed can be gotten from getInfo(); however,
			// the Vorbis I specification implies an absolute maximum of 4096 samples
			// per channel.

			// Channel coercion rules:
			//    Let M be the number of channels requested, and N the number of channels present,
			//    and Cn be the nth channel; let stereo L be the sum of all L and center channels,
			//    and stereo R be the sum of all R and center channels (channel assignment from the
			//    vorbis spec).
			//        M    N       output
			//        1    k      sum(Ck) for all k
			//        2    *      stereo L, stereo R
			//        k    l      k > l, the first l channels, then 0s
			//        k    l      k <= l, the first k channels
			//    Note that this is not _good_ surround etc. mixing at all! It's just so
			//    you get something useful.

			extern int32 NkVorbisGetSamplesFloatInterleaved(NkVorbisDecoder *f, int32 channels, float32 *buffer,
															int32 numFloats);
			extern int32 NkVorbisGetSamplesFloat(NkVorbisDecoder *f, int32 channels, float32 **buffer,
												 int32 numSamples);
			// gets numSamples samples, not necessarily on a frame boundary--this requires
			// buffering so you have to supply the buffers. DOES NOT APPLY THE COERCION RULES.
			// Returns the number of samples stored per channel; it may be less than requested
			// at the end of the file. If there are no more samples in the file, returns 0.

#ifndef NK_VORBIS_NO_INTEGER_CONVERSION
			extern int32 NkVorbisGetSamplesShortInterleaved(NkVorbisDecoder *f, int32 channels, int16 *buffer,
															int32 numShorts);
			extern int32 NkVorbisGetSamplesShort(NkVorbisDecoder *f, int32 channels, int16 **buffer, int32 numSamples);
#endif
			// gets numSamples samples, not necessarily on a frame boundary--this requires
			// buffering so you have to supply the buffers. Applies the coercion rules above
			// to produce 'channels' channels. Returns the number of samples stored per channel;
			// it may be less than requested at the end of the file. If there are no more
			// samples in the file, returns 0.

#endif

			////////   ERROR CODES

			enum NkVorbisError {
				NK_VORBIS__NO_ERROR,

				NK_VORBIS_NEED_MORE_DATA = 1, // not a real error

				NK_VORBIS_INVALID_API_MIXING,	 // can't mix API modes
				NK_VORBIS_OUTOFMEM,				 // not enough memory
				NK_VORBIS_FEATURE_NOT_SUPPORTED, // uses floor 0
				NK_VORBIS_TOO_MANY_CHANNELS,	 // NK_VORBIS_MAX_CHANNELS is too small
				NK_VORBIS_FILE_OPEN_FAILURE,	 // fopen() failed
				NK_VORBIS_SEEK_WITHOUT_LENGTH,	 // can't seek in unknown-length file

				NK_VORBIS_UNEXPECTED_EOF = 10, // file is truncated?
				NK_VORBIS_SEEK_INVALID,		   // seek past EOF

				// decoding errors (corrupt/invalid stream) -- you probably
				// don't care about the exact details of these

				// vorbis errors:
				NK_VORBIS_INVALID_SETUP = 20,
				NK_VORBIS_INVALID_STREAM,

				// ogg errors:
				NK_VORBIS_MISSING_NKCAPTUREPATTERN = 30,
				NK_VORBIS_INVALID_STREAM_STRUCTURE_VERSION,
				NK_VORBIS_CONTINUED_PACKET_FLAG_INVALID,
				NK_VORBIS_INCORRECT_STREAM_SERIAL_NUMBER,
				NK_VORBIS_INVALID_FIRST_PAGE,
				NK_VORBIS_BAD_PACKET_TYPE,
				NK_VORBIS_CANT_FIND_LAST_PAGE,
				NK_VORBIS_SEEK_FAILED,
				NK_VORBIS_OGG_SKELETON_NOT_SUPPORTED
			};

			// Le namespace vorbis reste ouvert pendant tout l'impl - ferme a la fin
			// (juste avant le bloc nkentseu::audio Decode wrapper).

#endif // NK_VORBIS_DECODER_HEADER_H
			//
			//  HEADER ENDS HERE
			//
			//////////////////////////////////////////////////////////////////////////////

#ifndef NK_VORBIS_HEADER_ONLY

// global configuration settings (e.g. set these in the project/makefile),
// or just set them in this file at the top (although ideally the first few
// should be visible when the header file is compiled too, although it's not
// crucial)

// NK_VORBIS_NO_PUSHDATA_API
//     does not compile the code for the various NkVorbis*Pushdata()
//     functions
// #define NK_VORBIS_NO_PUSHDATA_API

// NK_VORBIS_NO_PULLDATA_API
//     does not compile the code for the non-pushdata APIs
// #define NK_VORBIS_NO_PULLDATA_API

// NK_VORBIS_NO_STDIO
//     does not compile the code for the APIs that use FILE *s internally
//     or externally (implied by NK_VORBIS_NO_PULLDATA_API)
// #define NK_VORBIS_NO_STDIO

// NK_VORBIS_NO_INTEGER_CONVERSION
//     does not compile the code for converting audio sample data from
//     float32 to integer (implied by NK_VORBIS_NO_PULLDATA_API)
// #define NK_VORBIS_NO_INTEGER_CONVERSION

// NK_VORBIS_NO_FAST_SCALED_FLOAT
//      does not use a fast float32-to-int32 trick to accelerate float32-to-int32 on
//      most platforms which requires endianness be defined correctly.
// #define NK_VORBIS_NO_FAST_SCALED_FLOAT

// NK_VORBIS_MAX_CHANNELS [number]
//     globally define this to the maximum number of channels you need.
//     The spec does not put a restriction on channels except that
//     the count is stored in a byte, so 255 is the hard limit.
//     Reducing this saves about 16 bytes per value, so using 16 saves
//     (255-16)*16 or around 4KB. Plus anything other memory usage
//     I forgot to account for. Can probably go as low as 8 (7.1 audio),
//     6 (5.1 audio), or 2 (stereo only).
#ifndef NK_VORBIS_MAX_CHANNELS
#define NK_VORBIS_MAX_CHANNELS 16 // enough for anyone?
#endif

// NK_VORBIS_PUSHDATA_CRC_COUNT [number]
//     after a flushPushdata(), NkVorbisDecoder begins scanning for the
//     next valid page, without backtracking. when it finds something
//     that looks like a page, it streams through it and verifies its
//     CRC32. Should that validation fail, it keeps scanning. But it's
//     possible that _while_ streaming through to check the CRC32 of
//     one candidate page, it sees another candidate page. This #define
//     determines how many "overlapping" candidate pages it can search
//     at once. Note that "real" pages are typically ~4KB to ~8KB, whereas
//     garbage pages could be as big as 64KB, but probably average ~16KB.
//     So don't hose ourselves by scanning an apparent 64KB page and
//     missing a ton of real ones in the interim; so minimum of 2
#ifndef NK_VORBIS_PUSHDATA_CRC_COUNT
#define NK_VORBIS_PUSHDATA_CRC_COUNT 4
#endif

// NK_VORBIS_FAST_HUFFMAN_LENGTH [number]
//     sets the log size of the huffman-acceleration table.  Maximum
//     supported value is 24. with larger numbers, more decodings are O(1),
//     but the table size is larger so worse cache missing, so you'll have
//     to probe (and try multiple ogg vorbis files) to find the sweet spot.
#ifndef NK_VORBIS_FAST_HUFFMAN_LENGTH
#define NK_VORBIS_FAST_HUFFMAN_LENGTH 10
#endif

// NK_VORBIS_FAST_BINARY_LENGTH [number]
//     sets the log size of the binary-search acceleration table. this
//     is used in similar fashion to the fast-huffman size to set initial
//     parameters for the binary search

// NK_VORBIS_FAST_HUFFMAN_INT
//     The fast huffman tables are much more efficient if they can be
//     stored as 16-bit results instead of 32-bit results. This restricts
//     the codebooks to having only 65535 possible outcomes, though.
//     (At least, accelerated by the huffman table.)
#ifndef NK_VORBIS_FAST_HUFFMAN_INT
#define NK_VORBIS_FAST_HUFFMAN_SHORT
#endif

			// NK_VORBIS_NO_HUFFMAN_BINARY_SEARCH
			//     If the 'fast huffman' search doesn't succeed, then NkVorbisDecoder falls
			//     back on binary searching for the correct one. This requires storing
			//     extra tables with the huffman codes in sorted order. Defining this
			//     symbol trades off space for speed by forcing a linear search in the
			//     non-fast case, except for "sparse" codebooks.
			// #define NK_VORBIS_NO_HUFFMAN_BINARY_SEARCH

			// NK_VORBIS_DIVIDES_IN_RESIDUE
			//     NkVorbisDecoder precomputes the result of the scalar residue decoding
			//     that would otherwise require a divide per chunk. you can trade off
			//     space for time by defining this symbol.
			// #define NK_VORBIS_DIVIDES_IN_RESIDUE

			// NK_VORBIS_DIVIDES_IN_CODEBOOK
			//     vorbis VQ codebooks can be encoded two ways: with every case explicitly
			//     stored, or with all elements being chosen from a small range of values,
			//     and all values possible in all elements. By default, NkVorbisDecoder expands
			//     this latter kind out to look like the former kind for ease of decoding,
			//     because otherwise an integer divide-per-vector-element is required to
			//     unpack the index. If you define NK_VORBIS_DIVIDES_IN_CODEBOOK, you can
			//     trade off storage for speed.
			// #define NK_VORBIS_DIVIDES_IN_CODEBOOK

#ifdef NK_VORBIS_CODEBOOK_SHORTS
#error "NK_VORBIS_CODEBOOK_SHORTS is no longer supported as it produced incorrect results for some input formats"
#endif

			// NK_VORBIS_DIVIDE_TABLE
			//     this replaces small integer divides in the floor decode loop with
			//     table lookups. made less than 1% difference, so disabled by default.

			// NK_VORBIS_NO_INLINE_DECODE
			//     disables the inlining of the scalar codebook fast-huffman decode.
			//     might save a little codespace; useful for debugging
			// #define NK_VORBIS_NO_INLINE_DECODE

			// NK_VORBIS_NO_DEFER_FLOOR
			//     Normally we only decode the floor without synthesizing the actual
			//     full curve. We can instead synthesize the curve immediately. This
			//     requires more memory and is very likely slower, so I don't think
			//     you'd ever want to do it except for debugging.
			// #define NK_VORBIS_NO_DEFER_FLOOR

			//////////////////////////////////////////////////////////////////////////////

#ifdef NK_VORBIS_NO_PULLDATA_API
#define NK_VORBIS_NO_INTEGER_CONVERSION
#define NK_VORBIS_NO_STDIO
#endif

#if defined(NK_VORBIS_NO_CRT) && !defined(NK_VORBIS_NO_STDIO)
#define NK_VORBIS_NO_STDIO 1
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Endianness : detection fournie par NKPlatform/NkArchDetect.h.
//  NK_VORBIS_ENDIAN : 0 = little-endian, 1 = big-endian.
// ─────────────────────────────────────────────────────────────────────────────
#if defined(NKENTSEU_ARCH_BIG_ENDIAN)
#define NK_VORBIS_ENDIAN 1
#else
#define NK_VORBIS_ENDIAN 0
#endif

			// Includes systeme : NkTypes/NkMacros/cmath/cstring/cstdio sont fournis via
			// le PCH NKAudio. Aucun besoin de stdio/stdlib/string/assert/math/limits/malloc/alloca
			// car on utilise notre gestionnaire memoire (memory::NkAlloc/NkFree) pour les
			// allocations temporaires et longue duree.

#if NK_VORBIS_MAX_CHANNELS > 256
#error "Value of NK_VORBIS_MAX_CHANNELS outside of allowed range"
#endif

#if NK_VORBIS_FAST_HUFFMAN_LENGTH > 24
#error "Value of NK_VORBIS_FAST_HUFFMAN_LENGTH outside of allowed range"
#endif

#if 0
#include <crtdbg.h>
#define CHECK(f) _CrtIsValidHeapPointer(f->channelBuffers[1])
#else
#define CHECK(f) ((void)0)
#endif

#define MAX_BLOCKSIZE_LOG 13 // from specification
#define MAX_BLOCKSIZE (1 << MAX_BLOCKSIZE_LOG)

			typedef float32 codetype;

			// NKENTSEU_UNUSED est deja fourni par NKCore/NkMacros.h (inclus via PCH).

			// @NOTE
			//
			// Some arrays below are tagged "//varies", which means it's actually
			// a variable-sized piece of data, but rather than malloc I assume it's
			// small enough it's better to just allocate it all together with the
			// main thing
			//
			// Most of the variables are specified with the smallest size I could pack
			// them into. It might give better performance to make them all full-sized
			// integers. It should be safe to freely rearrange the structures or change
			// the sizes larger--nothing relies on silently truncating etc., nor the
			// order of variables.

#define FAST_HUFFMAN_TABLE_SIZE (1 << NK_VORBIS_FAST_HUFFMAN_LENGTH)
#define FAST_HUFFMAN_TABLE_MASK (FAST_HUFFMAN_TABLE_SIZE - 1)

			typedef struct {
				int32 dimensions, entries;
				uint8 *codewordLengths;
				float32 minimumValue;
				float32 deltaValue;
				uint8 valueBits;
				uint8 lookupType;
				uint8 sequenceP;
				uint8 sparse;
				uint32 lookupValues;
				codetype *multiplicands;
				uint32 *codewords;
#ifdef NK_VORBIS_FAST_HUFFMAN_SHORT
				int16 fastHuffman[FAST_HUFFMAN_TABLE_SIZE];
#else
				int32 fastHuffman[FAST_HUFFMAN_TABLE_SIZE];
#endif
				uint32 *sortedCodewords;
				int32 *sortedValues;
				int32 sortedEntries;
			} NkCodebook;

			typedef struct {
				uint8 order;
				uint16 rate;
				uint16 barkMapSize;
				uint8 amplitudeBits;
				uint8 amplitudeOffset;
				uint8 numberOfBooks;
				uint8 bookList[16]; // varies
			} Floor0;

			typedef struct {
				uint8 partitions;
				uint8 partitionClassList[32]; // varies
				uint8 classDimensions[16];	  // varies
				uint8 classSubclasses[16];	  // varies
				uint8 classMasterbooks[16];	  // varies
				int16 subclassBooks[16][8];	  // varies
				uint16 Xlist[31 * 8 + 2];	  // varies
				uint8 sortedOrder[31 * 8 + 2];
				uint8 NkNeighbors[31 * 8 + 2][2];
				uint8 floor1Multiplier;
				uint8 rangebits;
				int32 values;
			} NkFloor1;

			typedef union {
				Floor0 floor0;
				NkFloor1 floor1;
			} NkFloor;

			typedef struct {
				uint32 begin, end;
				uint32 partSize;
				uint8 classifications;
				uint8 classbook;
				uint8 **classdata;
				int16 (*residueBooks)[8];
			} NkResidue;

			typedef struct {
				uint8 magnitude;
				uint8 angle;
				uint8 mux;
			} NkMappingChannel;

			typedef struct {
				uint16 couplingSteps;
				NkMappingChannel *chan;
				uint8 submaps;
				uint8 submapFloor[15];	 // varies
				uint8 submapResidue[15]; // varies
			} NkMapping;

			typedef struct {
				uint8 blockflag;
				uint8 mapping;
				uint16 windowtype;
				uint16 transformtype;
			} NkMode;

			typedef struct {
				uint32 goalCrc;	  // expected crc if match
				int32 bytesLeft;  // bytes left in packet
				uint32 crcSoFar;  // running crc
				int32 bytesDone;  // bytes processed in _current_ chunk
				uint32 sampleLoc; // granule pos encoded in page
			} NkCrcScan;

			typedef struct {
				uint32 pageStart, pageEnd;
				uint32 lastDecodedSample;
			} NkProbedPage;

			struct NkVorbisDecoder {
				// user-accessible info
				uint32 sampleRate;
				int32 channels;

				uint32 setupMemoryRequired;
				uint32 tempMemoryRequired;
				uint32 setupTempMemoryRequired;

				char *vendor;
				int32 commentListLength;
				char **commentList;

				// input config
#ifndef NK_VORBIS_NO_STDIO
				FILE *f;
				uint32 fStart;
				int32 closeOnFree;
#endif

				uint8 *stream;
				uint8 *streamStart;
				uint8 *streamEnd;

				uint32 streamLen;

				uint8 pushMode;

				// the page to seek to when seeking to start, may be zero
				uint32 firstAudioPageOffset;

				// pFirst is the page on which the first audio packet ends
				// (but not necessarily the page on which it starts)
				NkProbedPage pFirst, pLast;

				// memory management
				NkVorbisAllocator alloc;
				int32 setupOffset;
				int32 tempOffset;

				// run-time results
				int32 eof;
				enum NkVorbisError error;

				// user-useful data

				// header info
				int32 blocksize[2];
				int32 blocksize0, blocksize1;
				int32 codebookCount;
				NkCodebook *codebooks;
				int32 floorCount;
				uint16 floorTypes[64]; // varies
				NkFloor *floorConfig;
				int32 residueCount;
				uint16 residueTypes[64]; // varies
				NkResidue *residueConfig;
				int32 mappingCount;
				NkMapping *mapping;
				int32 modeCount;
				NkMode modeConfig[64]; // varies

				uint32 totalSamples;

				// decode buffer
				float32 *channelBuffers[NK_VORBIS_MAX_CHANNELS];
				float32 *outputs[NK_VORBIS_MAX_CHANNELS];

				float32 *previousWindow[NK_VORBIS_MAX_CHANNELS];
				int32 previousLength;

#ifndef NK_VORBIS_NO_DEFER_FLOOR
				int16 *finalY[NK_VORBIS_MAX_CHANNELS];
#else
				float32 *floorBuffers[NK_VORBIS_MAX_CHANNELS];
#endif

				uint32 currentLoc; // sample location of next frame to decode
				int32 currentLocValid;

				// per-blocksize precomputed data

				// twiddle factors
				float32 *A[2], *B[2], *C[2];
				float32 *window[2];
				uint16 *NkBitReverse[2];

				// current page/packet/segment streaming info
				uint32 serial; // stream serial number for verification
				int32 lastPage;
				int32 segmentCount;
				uint8 segments[255];
				uint8 pageFlag;
				uint8 bytesInSeg;
				uint8 firstDecode;
				int32 nextSeg;
				int32 lastSeg;		// flag that we're on the last segment
				int32 lastSegWhich; // what was the segment number of the last seg?
				uint32 acc;
				int32 validBits;
				int32 packetBytes;
				int32 endSegWithKnownLoc;
				uint32 knownLocForPacket;
				int32 discardSamplesDeferred;
				uint32 samplesOutput;

				// push mode scanning
				int32 pageCrcTests; // only in pushMode: number of tests active; -1 if not searching
#ifndef NK_VORBIS_NO_PUSHDATA_API
				NkCrcScan scan[NK_VORBIS_PUSHDATA_CRC_COUNT];
#endif

				// sample-access
				int32 channelBufferStart;
				int32 channelBufferEnd;
			};

#if defined(NK_VORBIS_NO_PUSHDATA_API)
#define IS_PUSH_MODE(f) false
#elif defined(NK_VORBIS_NO_PULLDATA_API)
#define IS_PUSH_MODE(f) true
#else
#define IS_PUSH_MODE(f) ((f)->pushMode)
#endif

			typedef struct NkVorbisDecoder NkVorbisDecoder;

			static int32 error(NkVorbisDecoder *f, enum NkVorbisError e) {
				f->error = e;
				if (!f->eof && e != NK_VORBIS_NEED_MORE_DATA) {
					f->error = e; // breakpoint for debugging
				}
				return 0;
			}

			// these functions are used for allocating temporary memory
			// while decoding. if you can afford the stack space, use
			// alloca(); otherwise, provide a temp buffer and it will
			// allocate out of those.

#define arraySizeRequired(count, size) (count * (sizeof(void *) + (size)))

// tempAlloc/tempFree : utilisent toujours NkSetupTempMalloc/NkSetupTempFree
// qui delegent a memory::NkAlloc/NkFree (notre gestionnaire memoire Nkentseu).
// Plus de besoin de alloca() stack.
#define tempAlloc(f, size) NkSetupTempMalloc(f, size)
#define tempFree(f, p) NkSetupTempFree(f, p, 0)
#define tempAllocSave(f) ((f)->tempOffset)
#define tempAllocRestore(f, p) ((f)->tempOffset = (p))

#define tempBlockArray(f, count, size) makeBlockArray(tempAlloc(f, arraySizeRequired(count, size)), count, size)

			// given a sufficiently large block of memory, make an array of pointers to subblocks of it
			static void *makeBlockArray(void *mem, int32 count, int32 size) {
				int32 i;
				void **p = (void **)mem;
				char *q = (char *)(p + count);
				for (i = 0; i < count; ++i) {
					p[i] = q;
					q += size;
				}
				return p;
			}

			static void *NkSetupMalloc(NkVorbisDecoder *f, int32 sz) {
				sz = (sz + 7) & ~7; // round up to nearest 8 for alignment of future allocs.
				f->setupMemoryRequired += sz;
				if (f->alloc.allocBuffer) {
					void *p = (char *)f->alloc.allocBuffer + f->setupOffset;
					if (f->setupOffset + sz > f->tempOffset)
						return NULL;
					f->setupOffset += sz;
					return p;
				}
				return sz ? ::nkentseu::memory::NkAlloc(::nkentseu::usize(sz), nullptr, 8) : nullptr;
			}

			static void NkSetupFree(NkVorbisDecoder *f, void *p) {
				if (f->alloc.allocBuffer)
					return; // do nothing; setup mem is a stack
				::nkentseu::memory::NkFree(p, nullptr);
			}

			static void *NkSetupTempMalloc(NkVorbisDecoder *f, int32 sz) {
				sz = (sz + 7) & ~7; // round up to nearest 8 for alignment of future allocs.
				if (f->alloc.allocBuffer) {
					if (f->tempOffset - sz < f->setupOffset)
						return NULL;
					f->tempOffset -= sz;
					return (char *)f->alloc.allocBuffer + f->tempOffset;
				}
				return ::nkentseu::memory::NkAlloc(::nkentseu::usize(sz), nullptr, 8);
			}

			static void NkSetupTempFree(NkVorbisDecoder *f, void *p, int32 sz) {
				if (f->alloc.allocBuffer) {
					f->tempOffset += (sz + 7) & ~7;
					return;
				}
				::nkentseu::memory::NkFree(p, nullptr);
			}

#define CRC32_POLY 0x04c11db7 // from spec

			static uint32 crcTable[256];
			static void NkCrc32Init(void) {
				int32 i, j;
				uint32 s;
				for (i = 0; i < 256; i++) {
					for (s = (uint32)i << 24, j = 0; j < 8; ++j)
						s = (s << 1) ^ (s >= (1U << 31) ? CRC32_POLY : 0);
					crcTable[i] = s;
				}
			}

			static NKENTSEU_FORCE_INLINE uint32 crc32Update(uint32 crc, uint8 byte) {
				return (crc << 8) ^ crcTable[byte ^ (crc >> 24)];
			}

			// used in setup, and for huffman that doesn't go fast path
			static uint32 NkBitReverse(uint32 n) {
				n = ((n & 0xAAAAAAAA) >> 1) | ((n & 0x55555555) << 1);
				n = ((n & 0xCCCCCCCC) >> 2) | ((n & 0x33333333) << 2);
				n = ((n & 0xF0F0F0F0) >> 4) | ((n & 0x0F0F0F0F) << 4);
				n = ((n & 0xFF00FF00) >> 8) | ((n & 0x00FF00FF) << 8);
				return (n >> 16) | (n << 16);
			}

			static float32 NkSquare(float32 x) {
				return x * x;
			}

			// this is a weird definition of log2() for which log2(1) = 1, log2(2) = 2, log2(4) = 3
			// as required by the specification. fast(?) implementation
			// @OPTIMIZE: called multiple times per-packet with "constants"; move to setup
			static int32 NkIlog(int32 n) {
				static signed char log24[16] = {0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4};

				if (n < 0)
					return 0; // signed n returns 0

				// 2 compares if n < 16, 3 compares otherwise (4 if signed or n > 1<<29)
				if (n < (1 << 14))
					if (n < (1 << 4))
						return 0 + log24[n];
					else if (n < (1 << 9))
						return 5 + log24[n >> 5];
					else
						return 10 + log24[n >> 10];
				else if (n < (1 << 24))
					if (n < (1 << 19))
						return 15 + log24[n >> 15];
					else
						return 20 + log24[n >> 20];
				else if (n < (1 << 29))
					return 25 + log24[n >> 25];
				else
					return 30 + log24[n >> 30];
			}

#ifndef M_PI
#define M_PI 3.14159265358979323846264f // from CRC
#endif

// code length assigned to a value with no huffman encoding
#define NO_CODE 255

			/////////////////////// LEAF SETUP FUNCTIONS //////////////////////////
			//
			// these functions are only called at setup, and only a few times
			// per file

			static float32 NkFloat32Unpack(uint32 x) {
				// from the specification
				uint32 mantissa = x & 0x1fffff;
				uint32 sign = x & 0x80000000;
				uint32 exp = (x & 0x7fe00000) >> 21;
				float64 res = sign ? -(float64)mantissa : (float64)mantissa;
				return (float32)ldexp((float32)res, (int32)exp - 788);
			}

			// zlib & jpeg huffman tables assume that the output symbols
			// can either be arbitrarily arranged, or have monotonically
			// increasing frequencies--they rely on the lengths being sorted;
			// this makes for a very simple generation algorithm.
			// vorbis allows a huffman table with non-sorted lengths. This
			// requires a more sophisticated construction, since symbols in
			// order do not map to huffman codes "in order".
			static void NkAddEntry(NkCodebook *c, uint32 huffCode, int32 symbol, int32 count, int32 len,
								   uint32 *values) {
				if (!c->sparse) {
					c->codewords[symbol] = huffCode;
				} else {
					c->codewords[count] = huffCode;
					c->codewordLengths[count] = len;
					values[count] = symbol;
				}
			}

			static int32 NkComputeCodewords(NkCodebook *c, uint8 *len, int32 n, uint32 *values) {
				int32 i, k, m = 0;
				uint32 available[32];

				memset(available, 0, sizeof(available));
				// find the first entry
				for (k = 0; k < n; ++k)
					if (len[k] < NO_CODE)
						break;
				if (k == n) {
					assert(c->sortedEntries == 0);
					return true;
				}
				assert(len[k] < 32); // no error return required, code reading lens checks this
				// add to the list
				NkAddEntry(c, 0, k, m++, len[k], values);
				// add all available leaves
				for (i = 1; i <= len[k]; ++i)
					available[i] = 1U << (32 - i);
				// note that the above code treats the first case specially,
				// but it's really the same as the following code, so they
				// could probably be combined (except the initial code is 0,
				// and I use 0 in available[] to mean 'empty')
				for (i = k + 1; i < n; ++i) {
					uint32 res;
					int32 z = len[i], y;
					if (z == NO_CODE)
						continue;
					assert(z < 32); // no error return required, code reading lens checks this
					// find lowest available leaf (should always be earliest,
					// which is what the specification calls for)
					// note that this property, and the fact we can never have
					// more than one free leaf at a given level, isn't totally
					// trivial to prove, but it seems true and the assert never
					// fires, so!
					while (z > 0 && !available[z])
						--z;
					if (z == 0) {
						return false;
					}
					res = available[z];
					available[z] = 0;
					NkAddEntry(c, NkBitReverse(res), i, m++, len[i], values);
					// propagate availability up the tree
					if (z != len[i]) {
						for (y = len[i]; y > z; --y) {
							assert(available[y] == 0);
							available[y] = res + (1 << (32 - y));
						}
					}
				}
				return true;
			}

			// accelerated huffman table allows fast O(1) match of all symbols
			// of length <= NK_VORBIS_FAST_HUFFMAN_LENGTH
			static void NkComputeAcceleratedHuffman(NkCodebook *c) {
				int32 i, len;
				for (i = 0; i < FAST_HUFFMAN_TABLE_SIZE; ++i)
					c->fastHuffman[i] = -1;

				len = c->sparse ? c->sortedEntries : c->entries;
#ifdef NK_VORBIS_FAST_HUFFMAN_SHORT
				if (len > 32767)
					len = 32767; // largest possible value we can encode!
#endif
				for (i = 0; i < len; ++i) {
					if (c->codewordLengths[i] <= NK_VORBIS_FAST_HUFFMAN_LENGTH) {
						uint32 z = c->sparse ? NkBitReverse(c->sortedCodewords[i]) : c->codewords[i];
						// set table entries for all bit combinations in the higher bits
						while (z < FAST_HUFFMAN_TABLE_SIZE) {
							c->fastHuffman[z] = i;
							z += 1 << c->codewordLengths[i];
						}
					}
				}
			}

// Calling convention __cdecl requis par MSVC pour les callbacks qsort/etc.
#ifdef NKENTSEU_COMPILER_MSVC
#define NKV_CDECL __cdecl
#else
#define NKV_CDECL
#endif

			static int32 NKV_CDECL NkUint32Compare(const void *p, const void *q) {
				uint32 x = *(uint32 *)p;
				uint32 y = *(uint32 *)q;
				return x < y ? -1 : x > y;
			}

			static int32 NkIncludeInSort(NkCodebook *c, uint8 len) {
				if (c->sparse) {
					assert(len != NO_CODE);
					return true;
				}
				if (len == NO_CODE)
					return false;
				if (len > NK_VORBIS_FAST_HUFFMAN_LENGTH)
					return true;
				return false;
			}

			// if the fast table above doesn't work, we want to binary
			// search them... need to reverse the bits
			static void NkComputeSortedHuffman(NkCodebook *c, uint8 *lengths, uint32 *values) {
				int32 i, len;
				// build a list of all the entries
				// OPTIMIZATION: don't include the int16 ones, since they'll be caught by FAST_HUFFMAN.
				// this is kind of a frivolous optimization--I don't see any performance improvement,
				// but it's like 4 extra lines of code, so.
				if (!c->sparse) {
					int32 k = 0;
					for (i = 0; i < c->entries; ++i)
						if (NkIncludeInSort(c, lengths[i]))
							c->sortedCodewords[k++] = NkBitReverse(c->codewords[i]);
					assert(k == c->sortedEntries);
				} else {
					for (i = 0; i < c->sortedEntries; ++i)
						c->sortedCodewords[i] = NkBitReverse(c->codewords[i]);
				}

				qsort(c->sortedCodewords, c->sortedEntries, sizeof(c->sortedCodewords[0]), NkUint32Compare);
				c->sortedCodewords[c->sortedEntries] = 0xffffffff;

				len = c->sparse ? c->sortedEntries : c->entries;
				// now we need to indicate how they correspond; we could either
				//   #1: sort a different data structure that says who they correspond to
				//   #2: for each sorted entry, search the original list to find who corresponds
				//   #3: for each original entry, find the sorted entry
				// #1 requires extra storage, #2 is slow, #3 can use binary search!
				for (i = 0; i < len; ++i) {
					int32 huffLen = c->sparse ? lengths[values[i]] : lengths[i];
					if (NkIncludeInSort(c, huffLen)) {
						uint32 code = NkBitReverse(c->codewords[i]);
						int32 x = 0, n = c->sortedEntries;
						while (n > 1) {
							// invariant: sc[x] <= code < sc[x+n]
							int32 m = x + (n >> 1);
							if (c->sortedCodewords[m] <= code) {
								x = m;
								n -= (n >> 1);
							} else {
								n >>= 1;
							}
						}
						assert(c->sortedCodewords[x] == code);
						if (c->sparse) {
							c->sortedValues[x] = values[i];
							c->codewordLengths[x] = huffLen;
						} else {
							c->sortedValues[x] = i;
						}
					}
				}
			}

			// only run while parsing the header (3 times)
			static int32 NkVorbisValidate(uint8 *data) {
				static uint8 vorbis[6] = {'v', 'o', 'r', 'b', 'i', 's'};
				return memcmp(data, vorbis, 6) == 0;
			}

			// called from setup only, once per code book
			// (formula implied by specification)
			static int32 NkLookup1Values(int32 entries, int32 dim) {
				int32 r = (int32)floor(exp((float32)log((float32)entries) / dim));
				if ((int32)floor(pow((float32)r + 1, dim)) <= entries) // (int32) cast for MinGW warning;
					++r;											   // floor() to avoid _ftol() when non-CRT
				if (pow((float32)r + 1, dim) <= entries)
					return -1;
				if ((int32)floor(pow((float32)r, dim)) > entries)
					return -1;
				return r;
			}

			// called twice per file
			static void NkComputeTwiddleFactors(int32 n, float32 *A, float32 *B, float32 *C) {
				int32 n4 = n >> 2, n8 = n >> 3;
				int32 k, k2;

				for (k = k2 = 0; k < n4; ++k, k2 += 2) {
					A[k2] = (float32)cos(4 * k * M_PI / n);
					A[k2 + 1] = (float32)-sin(4 * k * M_PI / n);
					B[k2] = (float32)cos((k2 + 1) * M_PI / n / 2) * 0.5f;
					B[k2 + 1] = (float32)sin((k2 + 1) * M_PI / n / 2) * 0.5f;
				}
				for (k = k2 = 0; k < n8; ++k, k2 += 2) {
					C[k2] = (float32)cos(2 * (k2 + 1) * M_PI / n);
					C[k2 + 1] = (float32)-sin(2 * (k2 + 1) * M_PI / n);
				}
			}

			static void NkComputeWindow(int32 n, float32 *window) {
				int32 n2 = n >> 1, i;
				for (i = 0; i < n2; ++i)
					window[i] = (float32)sin(0.5 * M_PI * NkSquare((float32)sin((i - 0 + 0.5) / n2 * 0.5 * M_PI)));
			}

			static void NkComputeBitreverse(int32 n, uint16 *rev) {
				int32 ld = NkIlog(n) - 1; // NkIlog is off-by-one from normal definitions
				int32 i, n8 = n >> 3;
				for (i = 0; i < n8; ++i)
					rev[i] = (NkBitReverse(i) >> (32 - ld + 3)) << 2;
			}

			static int32 NkInitBlocksize(NkVorbisDecoder *f, int32 b, int32 n) {
				int32 n2 = n >> 1, n4 = n >> 2, n8 = n >> 3;
				f->A[b] = (float32 *)NkSetupMalloc(f, sizeof(float32) * n2);
				f->B[b] = (float32 *)NkSetupMalloc(f, sizeof(float32) * n2);
				f->C[b] = (float32 *)NkSetupMalloc(f, sizeof(float32) * n4);
				if (!f->A[b] || !f->B[b] || !f->C[b])
					return error(f, NK_VORBIS_OUTOFMEM);
				NkComputeTwiddleFactors(n, f->A[b], f->B[b], f->C[b]);
				f->window[b] = (float32 *)NkSetupMalloc(f, sizeof(float32) * n2);
				if (!f->window[b])
					return error(f, NK_VORBIS_OUTOFMEM);
				NkComputeWindow(n, f->window[b]);
				f->NkBitReverse[b] = (uint16 *)NkSetupMalloc(f, sizeof(uint16) * n8);
				if (!f->NkBitReverse[b])
					return error(f, NK_VORBIS_OUTOFMEM);
				NkComputeBitreverse(n, f->NkBitReverse[b]);
				return true;
			}

			static void NkNeighbors(uint16 *x, int32 n, int32 *plow, int32 *phigh) {
				int32 low = -1;
				int32 high = 65536;
				int32 i;
				for (i = 0; i < n; ++i) {
					if (x[i] > low && x[i] < x[n]) {
						*plow = i;
						low = x[i];
					}
					if (x[i] < high && x[i] > x[n]) {
						*phigh = i;
						high = x[i];
					}
				}
			}

			// this has been repurposed so y is now the original index instead of y
			typedef struct {
				uint16 x, id;
			} NkVorbisFloorOrdering;

			static int32 NKV_CDECL NkPointCompare(const void *p, const void *q) {
				NkVorbisFloorOrdering *a = (NkVorbisFloorOrdering *)p;
				NkVorbisFloorOrdering *b = (NkVorbisFloorOrdering *)q;
				return a->x < b->x ? -1 : a->x > b->x;
			}

			//
			/////////////////////// END LEAF SETUP FUNCTIONS //////////////////////////

#if defined(NK_VORBIS_NO_STDIO)
#define USE_MEMORY(z) true
#else
#define USE_MEMORY(z) ((z)->stream)
#endif

			static uint8 get8(NkVorbisDecoder *z) {
				if (USE_MEMORY(z)) {
					if (z->stream >= z->streamEnd) {
						z->eof = true;
						return 0;
					}
					return *z->stream++;
				}

#ifndef NK_VORBIS_NO_STDIO
				{
					int32 c = fgetc(z->f);
					if (c == EOF) {
						z->eof = true;
						return 0;
					}
					return c;
				}
#endif
			}

			static uint32 get32(NkVorbisDecoder *f) {
				uint32 x;
				x = get8(f);
				x += get8(f) << 8;
				x += get8(f) << 16;
				x += (uint32)get8(f) << 24;
				return x;
			}

			static int32 NkGetn(NkVorbisDecoder *z, uint8 *data, int32 n) {
				if (USE_MEMORY(z)) {
					if (z->stream + n > z->streamEnd) {
						z->eof = 1;
						return 0;
					}
					memcpy(data, z->stream, n);
					z->stream += n;
					return 1;
				}

#ifndef NK_VORBIS_NO_STDIO
				if (fread(data, n, 1, z->f) == 1)
					return 1;
				else {
					z->eof = 1;
					return 0;
				}
#endif
			}

			static void NkSkip(NkVorbisDecoder *z, int32 n) {
				if (USE_MEMORY(z)) {
					z->stream += n;
					if (z->stream >= z->streamEnd)
						z->eof = 1;
					return;
				}
#ifndef NK_VORBIS_NO_STDIO
				{
					int64 x = ftell(z->f);
					fseek(z->f, x + n, SEEK_SET);
				}
#endif
			}

			static int32 NkSetFileOffset(NkVorbisDecoder *f, uint32 loc) {
#ifndef NK_VORBIS_NO_PUSHDATA_API
				if (f->pushMode)
					return 0;
#endif
				f->eof = 0;
				if (USE_MEMORY(f)) {
					// Garde anti-overflow de pointeur heritee de stb_vorbis (vendored) :
					// `streamStart + loc < streamStart` est toujours faux en arithmetique
					// de pointeur definie -> on neutralise le warning sans toucher la logique.
#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wtautological-pointer-compare"
#  pragma clang diagnostic ignored "-Wtautological-compare"
#endif
					if (f->streamStart + loc >= f->streamEnd || f->streamStart + loc < f->streamStart) {
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif
						f->stream = f->streamEnd;
						f->eof = 1;
						return 0;
					} else {
						f->stream = f->streamStart + loc;
						return 1;
					}
				}
#ifndef NK_VORBIS_NO_STDIO
				if (loc + f->fStart < loc || loc >= 0x80000000) {
					loc = 0x7fffffff;
					f->eof = 1;
				} else {
					loc += f->fStart;
				}
				if (!fseek(f->f, loc, SEEK_SET))
					return 1;
				f->eof = 1;
				fseek(f->f, f->fStart, SEEK_END);
				return 0;
#endif
			}

			static uint8 oggPageHeader[4] = {0x4f, 0x67, 0x67, 0x53};

			static int32 NkCapturePattern(NkVorbisDecoder *f) {
				if (0x4f != get8(f))
					return false;
				if (0x67 != get8(f))
					return false;
				if (0x67 != get8(f))
					return false;
				if (0x53 != get8(f))
					return false;
				return true;
			}

#define PAGEFLAG_continued_packet 1
#define PAGEFLAG_first_page 2
#define PAGEFLAG_last_page 4

			static int32 NkStartPageNoCapturepattern(NkVorbisDecoder *f) {
				uint32 loc0, loc1, n;
				if (f->firstDecode && !IS_PUSH_MODE(f)) {
					f->pFirst.pageStart = NkVorbisGetFileOffset(f) - 4;
				}
				// stream structure version
				if (0 != get8(f))
					return error(f, NK_VORBIS_INVALID_STREAM_STRUCTURE_VERSION);
				// header flag
				f->pageFlag = get8(f);
				// absolute granule position
				loc0 = get32(f);
				loc1 = get32(f);
				// @TODO: validate loc0,loc1 as valid positions?
				// stream serial number -- vorbis doesn't interleave, so discard
				get32(f);
				// if (f->serial != get32(f)) return error(f, NK_VORBIS_INCORRECT_STREAM_SERIAL_NUMBER);
				//  page sequence number
				n = get32(f);
				f->lastPage = n;
				// CRC32
				get32(f);
				// pageSegments
				f->segmentCount = get8(f);
				if (!NkGetn(f, f->segments, f->segmentCount))
					return error(f, NK_VORBIS_UNEXPECTED_EOF);
				// assume we _don't_ know any the sample position of any segments
				f->endSegWithKnownLoc = -2;
				if (loc0 != ~0U || loc1 != ~0U) {
					int32 i;
					// determine which packet is the last one that will complete
					for (i = f->segmentCount - 1; i >= 0; --i)
						if (f->segments[i] < 255)
							break;
					// 'i' is now the index of the _last_ segment of a packet that ends
					if (i >= 0) {
						f->endSegWithKnownLoc = i;
						f->knownLocForPacket = loc0;
					}
				}
				if (f->firstDecode) {
					int32 i, len;
					len = 0;
					for (i = 0; i < f->segmentCount; ++i)
						len += f->segments[i];
					len += 27 + f->segmentCount;
					f->pFirst.pageEnd = f->pFirst.pageStart + len;
					f->pFirst.lastDecodedSample = loc0;
				}
				f->nextSeg = 0;
				return true;
			}

			static int32 NkStartPage(NkVorbisDecoder *f) {
				if (!NkCapturePattern(f))
					return error(f, NK_VORBIS_MISSING_NKCAPTUREPATTERN);
				return NkStartPageNoCapturepattern(f);
			}

			static int32 NkStartPacket(NkVorbisDecoder *f) {
				while (f->nextSeg == -1) {
					if (!NkStartPage(f))
						return false;
					if (f->pageFlag & PAGEFLAG_continued_packet)
						return error(f, NK_VORBIS_CONTINUED_PACKET_FLAG_INVALID);
				}
				f->lastSeg = false;
				f->validBits = 0;
				f->packetBytes = 0;
				f->bytesInSeg = 0;
				// f->nextSeg is now valid
				return true;
			}

			static int32 NkMaybeStartPacket(NkVorbisDecoder *f) {
				if (f->nextSeg == -1) {
					int32 x = get8(f);
					if (f->eof)
						return false; // EOF at page boundary is not an error!
					if (0x4f != x)
						return error(f, NK_VORBIS_MISSING_NKCAPTUREPATTERN);
					if (0x67 != get8(f))
						return error(f, NK_VORBIS_MISSING_NKCAPTUREPATTERN);
					if (0x67 != get8(f))
						return error(f, NK_VORBIS_MISSING_NKCAPTUREPATTERN);
					if (0x53 != get8(f))
						return error(f, NK_VORBIS_MISSING_NKCAPTUREPATTERN);
					if (!NkStartPageNoCapturepattern(f))
						return false;
					if (f->pageFlag & PAGEFLAG_continued_packet) {
						// set up enough state that we can read this packet if we want,
						// e.g. during recovery
						f->lastSeg = false;
						f->bytesInSeg = 0;
						return error(f, NK_VORBIS_CONTINUED_PACKET_FLAG_INVALID);
					}
				}
				return NkStartPacket(f);
			}

			static int32 NkNextSegment(NkVorbisDecoder *f) {
				int32 len;
				if (f->lastSeg)
					return 0;
				if (f->nextSeg == -1) {
					f->lastSegWhich = f->segmentCount - 1; // in case NkStartPage fails
					if (!NkStartPage(f)) {
						f->lastSeg = 1;
						return 0;
					}
					if (!(f->pageFlag & PAGEFLAG_continued_packet))
						return error(f, NK_VORBIS_CONTINUED_PACKET_FLAG_INVALID);
				}
				len = f->segments[f->nextSeg++];
				if (len < 255) {
					f->lastSeg = true;
					f->lastSegWhich = f->nextSeg - 1;
				}
				if (f->nextSeg >= f->segmentCount)
					f->nextSeg = -1;
				assert(f->bytesInSeg == 0);
				f->bytesInSeg = len;
				return len;
			}

#define EOP (-1)
#define INVALID_BITS (-1)

			static int32 NkGet8PacketRaw(NkVorbisDecoder *f) {
				if (!f->bytesInSeg) { // CLANG!
					if (f->lastSeg)
						return EOP;
					else if (!NkNextSegment(f))
						return EOP;
				}
				assert(f->bytesInSeg > 0);
				--f->bytesInSeg;
				++f->packetBytes;
				return get8(f);
			}

			static int32 NkGet8Packet(NkVorbisDecoder *f) {
				int32 x = NkGet8PacketRaw(f);
				f->validBits = 0;
				return x;
			}

			static int32 NkGet32Packet(NkVorbisDecoder *f) {
				uint32 x;
				x = NkGet8Packet(f);
				x += NkGet8Packet(f) << 8;
				x += NkGet8Packet(f) << 16;
				x += (uint32)NkGet8Packet(f) << 24;
				return x;
			}

			static void NkFlushPacket(NkVorbisDecoder *f) {
				while (NkGet8PacketRaw(f) != EOP)
					;
			}

			// @OPTIMIZE: this is the secondary bit decoder, so it's probably not as important
			// as the huffman decoder?
			static uint32 getBits(NkVorbisDecoder *f, int32 n) {
				uint32 z;

				if (f->validBits < 0)
					return 0;
				if (f->validBits < n) {
					if (n > 24) {
						// the accumulator technique below would not work correctly in this case
						z = getBits(f, 24);
						z += getBits(f, n - 24) << 24;
						return z;
					}
					if (f->validBits == 0)
						f->acc = 0;
					while (f->validBits < n) {
						int32 z = NkGet8PacketRaw(f);
						if (z == EOP) {
							f->validBits = INVALID_BITS;
							return 0;
						}
						f->acc += z << f->validBits;
						f->validBits += 8;
					}
				}

				assert(f->validBits >= n);
				z = f->acc & ((1 << n) - 1);
				f->acc >>= n;
				f->validBits -= n;
				return z;
			}

			// @OPTIMIZE: primary accumulator for huffman
			// expand the buffer to as many bits as possible without reading off end of packet
			// it might be nice to allow f->validBits and f->acc to be stored in registers,
			// e.g. cache them locally and decode locally
			static NKENTSEU_FORCE_INLINE void NkPrepHuffman(NkVorbisDecoder *f) {
				if (f->validBits <= 24) {
					if (f->validBits == 0)
						f->acc = 0;
					do {
						int32 z;
						if (f->lastSeg && !f->bytesInSeg)
							return;
						z = NkGet8PacketRaw(f);
						if (z == EOP)
							return;
						f->acc += (uint32)z << f->validBits;
						f->validBits += 8;
					} while (f->validBits <= 24);
				}
			}

			enum { NK_VORBIS_PACKET_ID = 1, NK_VORBIS_PACKET_COMMENT = 3, NK_VORBIS_PACKET_SETUP = 5 };

			static int32 NkCodebookDecodeScalarRaw(NkVorbisDecoder *f, NkCodebook *c) {
				int32 i;
				NkPrepHuffman(f);

				if (c->codewords == NULL && c->sortedCodewords == NULL)
					return -1;

				// cases to use binary search: sortedCodewords && !c->codewords
				//                             sortedCodewords && c->entries > 8
				if (c->entries > 8 ? c->sortedCodewords != NULL : !c->codewords) {
					// binary search
					uint32 code = NkBitReverse(f->acc);
					int32 x = 0, n = c->sortedEntries, len;

					while (n > 1) {
						// invariant: sc[x] <= code < sc[x+n]
						int32 m = x + (n >> 1);
						if (c->sortedCodewords[m] <= code) {
							x = m;
							n -= (n >> 1);
						} else {
							n >>= 1;
						}
					}
					// x is now the sorted index
					if (!c->sparse)
						x = c->sortedValues[x];
					// x is now sorted index if sparse, or symbol otherwise
					len = c->codewordLengths[x];
					if (f->validBits >= len) {
						f->acc >>= len;
						f->validBits -= len;
						return x;
					}

					f->validBits = 0;
					return -1;
				}

				// if small, linear search
				assert(!c->sparse);
				for (i = 0; i < c->entries; ++i) {
					if (c->codewordLengths[i] == NO_CODE)
						continue;
					if (c->codewords[i] == (f->acc & ((1 << c->codewordLengths[i]) - 1))) {
						if (f->validBits >= c->codewordLengths[i]) {
							f->acc >>= c->codewordLengths[i];
							f->validBits -= c->codewordLengths[i];
							return i;
						}
						f->validBits = 0;
						return -1;
					}
				}

				error(f, NK_VORBIS_INVALID_STREAM);
				f->validBits = 0;
				return -1;
			}

#ifndef NK_VORBIS_NO_INLINE_DECODE

#define DECODE_RAW(var, f, c)                                                                                          \
	if (f->validBits < NK_VORBIS_FAST_HUFFMAN_LENGTH)                                                                  \
		NkPrepHuffman(f);                                                                                              \
	var = f->acc & FAST_HUFFMAN_TABLE_MASK;                                                                            \
	var = c->fastHuffman[var];                                                                                         \
	if (var >= 0) {                                                                                                    \
		int32 n = c->codewordLengths[var];                                                                             \
		f->acc >>= n;                                                                                                  \
		f->validBits -= n;                                                                                             \
		if (f->validBits < 0) {                                                                                        \
			f->validBits = 0;                                                                                          \
			var = -1;                                                                                                  \
		}                                                                                                              \
	} else {                                                                                                           \
		var = NkCodebookDecodeScalarRaw(f, c);                                                                         \
	}

#else

			static int32 NkCodebookDecodeScalar(NkVorbisDecoder *f, NkCodebook *c) {
				int32 i;
				if (f->validBits < NK_VORBIS_FAST_HUFFMAN_LENGTH)
					NkPrepHuffman(f);
				// fast huffman table lookup
				i = f->acc & FAST_HUFFMAN_TABLE_MASK;
				i = c->fastHuffman[i];
				if (i >= 0) {
					f->acc >>= c->codewordLengths[i];
					f->validBits -= c->codewordLengths[i];
					if (f->validBits < 0) {
						f->validBits = 0;
						return -1;
					}
					return i;
				}
				return NkCodebookDecodeScalarRaw(f, c);
			}

#define DECODE_RAW(var, f, c) var = NkCodebookDecodeScalar(f, c);

#endif

#define DECODE(var, f, c)                                                                                              \
	DECODE_RAW(var, f, c)                                                                                              \
	if (c->sparse)                                                                                                     \
		var = c->sortedValues[var];

#ifndef NK_VORBIS_DIVIDES_IN_CODEBOOK
#define DECODE_VQ(var, f, c) DECODE_RAW(var, f, c)
#else
#define DECODE_VQ(var, f, c) DECODE(var, f, c)
#endif

// CODEBOOK_ELEMENT_FAST is an optimization for the CODEBOOK_FLOATS case
// where we avoid one addition
#define CODEBOOK_ELEMENT(c, off) (c->multiplicands[off])
#define CODEBOOK_ELEMENT_FAST(c, off) (c->multiplicands[off])
#define CODEBOOK_ELEMENT_BASE(c) (0)

			static int32 NkCodebookDecodeStart(NkVorbisDecoder *f, NkCodebook *c) {
				int32 z = -1;

				// type 0 is only legal in a scalar context
				if (c->lookupType == 0)
					error(f, NK_VORBIS_INVALID_STREAM);
				else {
					DECODE_VQ(z, f, c);
					if (c->sparse)
						assert(z < c->sortedEntries);
					if (z < 0) { // check for EOP
						if (!f->bytesInSeg)
							if (f->lastSeg)
								return z;
						error(f, NK_VORBIS_INVALID_STREAM);
					}
				}
				return z;
			}

			static int32 NkCodebookDecode(NkVorbisDecoder *f, NkCodebook *c, float32 *output, int32 len) {
				int32 i, z = NkCodebookDecodeStart(f, c);
				if (z < 0)
					return false;
				if (len > c->dimensions)
					len = c->dimensions;

#ifdef NK_VORBIS_DIVIDES_IN_CODEBOOK
				if (c->lookupType == 1) {
					float32 last = CODEBOOK_ELEMENT_BASE(c);
					int32 div = 1;
					for (i = 0; i < len; ++i) {
						int32 off = (z / div) % c->lookupValues;
						float32 val = CODEBOOK_ELEMENT_FAST(c, off) + last;
						output[i] += val;
						if (c->sequenceP)
							last = val + c->minimumValue;
						div *= c->lookupValues;
					}
					return true;
				}
#endif

				z *= c->dimensions;
				if (c->sequenceP) {
					float32 last = CODEBOOK_ELEMENT_BASE(c);
					for (i = 0; i < len; ++i) {
						float32 val = CODEBOOK_ELEMENT_FAST(c, z + i) + last;
						output[i] += val;
						last = val + c->minimumValue;
					}
				} else {
					float32 last = CODEBOOK_ELEMENT_BASE(c);
					for (i = 0; i < len; ++i) {
						output[i] += CODEBOOK_ELEMENT_FAST(c, z + i) + last;
					}
				}

				return true;
			}

			static int32 NkCodebookDecodeStep(NkVorbisDecoder *f, NkCodebook *c, float32 *output, int32 len,
											  int32 step) {
				int32 i, z = NkCodebookDecodeStart(f, c);
				float32 last = CODEBOOK_ELEMENT_BASE(c);
				if (z < 0)
					return false;
				if (len > c->dimensions)
					len = c->dimensions;

#ifdef NK_VORBIS_DIVIDES_IN_CODEBOOK
				if (c->lookupType == 1) {
					int32 div = 1;
					for (i = 0; i < len; ++i) {
						int32 off = (z / div) % c->lookupValues;
						float32 val = CODEBOOK_ELEMENT_FAST(c, off) + last;
						output[i * step] += val;
						if (c->sequenceP)
							last = val;
						div *= c->lookupValues;
					}
					return true;
				}
#endif

				z *= c->dimensions;
				for (i = 0; i < len; ++i) {
					float32 val = CODEBOOK_ELEMENT_FAST(c, z + i) + last;
					output[i * step] += val;
					if (c->sequenceP)
						last = val;
				}

				return true;
			}

			static int32 NkCodebookDecodeDeinterleaveRepeat(NkVorbisDecoder *f, NkCodebook *c, float32 **outputs,
															int32 ch, int32 *cInterP, int32 *pInterP, int32 len,
															int32 totalDecode) {
				int32 cInter = *cInterP;
				int32 pInter = *pInterP;
				int32 i, z, effective = c->dimensions;

				// type 0 is only legal in a scalar context
				if (c->lookupType == 0)
					return error(f, NK_VORBIS_INVALID_STREAM);

				while (totalDecode > 0) {
					float32 last = CODEBOOK_ELEMENT_BASE(c);
					DECODE_VQ(z, f, c);
#ifndef NK_VORBIS_DIVIDES_IN_CODEBOOK
					assert(!c->sparse || z < c->sortedEntries);
#endif
					if (z < 0) {
						if (!f->bytesInSeg)
							if (f->lastSeg)
								return false;
						return error(f, NK_VORBIS_INVALID_STREAM);
					}

					// if this will take us off the end of the buffers, stop int16!
					// we check by computing the length of the virtual interleaved
					// buffer (len*ch), our current offset within it (pInter*ch)+(cInter),
					// and the length we'll be using (effective)
					if (cInter + pInter * ch + effective > len * ch) {
						effective = len * ch - (pInter * ch - cInter);
					}

#ifdef NK_VORBIS_DIVIDES_IN_CODEBOOK
					if (c->lookupType == 1) {
						int32 div = 1;
						for (i = 0; i < effective; ++i) {
							int32 off = (z / div) % c->lookupValues;
							float32 val = CODEBOOK_ELEMENT_FAST(c, off) + last;
							if (outputs[cInter])
								outputs[cInter][pInter] += val;
							if (++cInter == ch) {
								cInter = 0;
								++pInter;
							}
							if (c->sequenceP)
								last = val;
							div *= c->lookupValues;
						}
					} else
#endif
					{
						z *= c->dimensions;
						if (c->sequenceP) {
							for (i = 0; i < effective; ++i) {
								float32 val = CODEBOOK_ELEMENT_FAST(c, z + i) + last;
								if (outputs[cInter])
									outputs[cInter][pInter] += val;
								if (++cInter == ch) {
									cInter = 0;
									++pInter;
								}
								last = val;
							}
						} else {
							for (i = 0; i < effective; ++i) {
								float32 val = CODEBOOK_ELEMENT_FAST(c, z + i) + last;
								if (outputs[cInter])
									outputs[cInter][pInter] += val;
								if (++cInter == ch) {
									cInter = 0;
									++pInter;
								}
							}
						}
					}

					totalDecode -= effective;
				}
				*cInterP = cInter;
				*pInterP = pInter;
				return true;
			}

			static int32 NkPredictPoint(int32 x, int32 x0, int32 x1, int32 y0, int32 y1) {
				int32 dy = y1 - y0;
				int32 adx = x1 - x0;
				// @OPTIMIZE: force int32 division to round in the right direction... is this necessary on x86?
				int32 err = abs(dy) * (x - x0);
				int32 off = err / adx;
				return dy < 0 ? y0 - off : y0 + off;
			}

			// the following table is block-copied from the specification
			static float32 inverseDbTable[256] = {1.0649863e-07f, 1.1341951e-07f, 1.2079015e-07f, 1.2863978e-07f,
												  1.3699951e-07f, 1.4590251e-07f, 1.5538408e-07f, 1.6548181e-07f,
												  1.7623575e-07f, 1.8768855e-07f, 1.9988561e-07f, 2.1287530e-07f,
												  2.2670913e-07f, 2.4144197e-07f, 2.5713223e-07f, 2.7384213e-07f,
												  2.9163793e-07f, 3.1059021e-07f, 3.3077411e-07f, 3.5226968e-07f,
												  3.7516214e-07f, 3.9954229e-07f, 4.2550680e-07f, 4.5315863e-07f,
												  4.8260743e-07f, 5.1396998e-07f, 5.4737065e-07f, 5.8294187e-07f,
												  6.2082472e-07f, 6.6116941e-07f, 7.0413592e-07f, 7.4989464e-07f,
												  7.9862701e-07f, 8.5052630e-07f, 9.0579828e-07f, 9.6466216e-07f,
												  1.0273513e-06f, 1.0941144e-06f, 1.1652161e-06f, 1.2409384e-06f,
												  1.3215816e-06f, 1.4074654e-06f, 1.4989305e-06f, 1.5963394e-06f,
												  1.7000785e-06f, 1.8105592e-06f, 1.9282195e-06f, 2.0535261e-06f,
												  2.1869758e-06f, 2.3290978e-06f, 2.4804557e-06f, 2.6416497e-06f,
												  2.8133190e-06f, 2.9961443e-06f, 3.1908506e-06f, 3.3982101e-06f,
												  3.6190449e-06f, 3.8542308e-06f, 4.1047004e-06f, 4.3714470e-06f,
												  4.6555282e-06f, 4.9580707e-06f, 5.2802740e-06f, 5.6234160e-06f,
												  5.9888572e-06f, 6.3780469e-06f, 6.7925283e-06f, 7.2339451e-06f,
												  7.7040476e-06f, 8.2047000e-06f, 8.7378876e-06f, 9.3057248e-06f,
												  9.9104632e-06f, 1.0554501e-05f, 1.1240392e-05f, 1.1970856e-05f,
												  1.2748789e-05f, 1.3577278e-05f, 1.4459606e-05f, 1.5399272e-05f,
												  1.6400004e-05f, 1.7465768e-05f, 1.8600792e-05f, 1.9809576e-05f,
												  2.1096914e-05f, 2.2467911e-05f, 2.3928002e-05f, 2.5482978e-05f,
												  2.7139006e-05f, 2.8902651e-05f, 3.0780908e-05f, 3.2781225e-05f,
												  3.4911534e-05f, 3.7180282e-05f, 3.9596466e-05f, 4.2169667e-05f,
												  4.4910090e-05f, 4.7828601e-05f, 5.0936773e-05f, 5.4246931e-05f,
												  5.7772202e-05f, 6.1526565e-05f, 6.5524908e-05f, 6.9783085e-05f,
												  7.4317983e-05f, 7.9147585e-05f, 8.4291040e-05f, 8.9768747e-05f,
												  9.5602426e-05f, 0.00010181521f, 0.00010843174f, 0.00011547824f,
												  0.00012298267f, 0.00013097477f, 0.00013948625f, 0.00014855085f,
												  0.00015820453f, 0.00016848555f, 0.00017943469f, 0.00019109536f,
												  0.00020351382f, 0.00021673929f, 0.00023082423f, 0.00024582449f,
												  0.00026179955f, 0.00027881276f, 0.00029693158f, 0.00031622787f,
												  0.00033677814f, 0.00035866388f, 0.00038197188f, 0.00040679456f,
												  0.00043323036f, 0.00046138411f, 0.00049136745f, 0.00052329927f,
												  0.00055730621f, 0.00059352311f, 0.00063209358f, 0.00067317058f,
												  0.00071691700f, 0.00076350630f, 0.00081312324f, 0.00086596457f,
												  0.00092223983f, 0.00098217216f, 0.0010459992f,  0.0011139742f,
												  0.0011863665f,  0.0012634633f,  0.0013455702f,  0.0014330129f,
												  0.0015261382f,  0.0016253153f,  0.0017309374f,  0.0018434235f,
												  0.0019632195f,  0.0020908006f,  0.0022266726f,  0.0023713743f,
												  0.0025254795f,  0.0026895994f,  0.0028643847f,  0.0030505286f,
												  0.0032487691f,  0.0034598925f,  0.0036847358f,  0.0039241906f,
												  0.0041792066f,  0.0044507950f,  0.0047400328f,  0.0050480668f,
												  0.0053761186f,  0.0057254891f,  0.0060975636f,  0.0064938176f,
												  0.0069158225f,  0.0073652516f,  0.0078438871f,  0.0083536271f,
												  0.0088964928f,  0.009474637f,	  0.010090352f,	  0.010746080f,
												  0.011444421f,	  0.012188144f,	  0.012980198f,	  0.013823725f,
												  0.014722068f,	  0.015678791f,	  0.016697687f,	  0.017782797f,
												  0.018938423f,	  0.020169149f,	  0.021479854f,	  0.022875735f,
												  0.024362330f,	  0.025945531f,	  0.027631618f,	  0.029427276f,
												  0.031339626f,	  0.033376252f,	  0.035545228f,	  0.037855157f,
												  0.040315199f,	  0.042935108f,	  0.045725273f,	  0.048696758f,
												  0.051861348f,	  0.055231591f,	  0.058820850f,	  0.062643361f,
												  0.066714279f,	  0.071049749f,	  0.075666962f,	  0.080584227f,
												  0.085821044f,	  0.091398179f,	  0.097337747f,	  0.10366330f,
												  0.11039993f,	  0.11757434f,	  0.12521498f,	  0.13335215f,
												  0.14201813f,	  0.15124727f,	  0.16107617f,	  0.17154380f,
												  0.18269168f,	  0.19456402f,	  0.20720788f,	  0.22067342f,
												  0.23501402f,	  0.25028656f,	  0.26655159f,	  0.28387361f,
												  0.30232132f,	  0.32196786f,	  0.34289114f,	  0.36517414f,
												  0.38890521f,	  0.41417847f,	  0.44109412f,	  0.46975890f,
												  0.50028648f,	  0.53279791f,	  0.56742212f,	  0.60429640f,
												  0.64356699f,	  0.68538959f,	  0.72993007f,	  0.77736504f,
												  0.82788260f,	  0.88168307f,	  0.9389798f,	  1.0f};

// @OPTIMIZE: if you want to replace this bresenham line-drawing routine,
// note that you must produce bit-identical output to decode correctly;
// this specific sequence of operations is specified in the spec (it's
// drawing integer-quantized frequency-space lines that the encoder
// expects to be exactly the same)
//     ... also, isn't the whole point of Bresenham's algorithm to NOT
// have to divide in the setup? sigh.
#ifndef NK_VORBIS_NO_DEFER_FLOOR
#define LINE_OP(a, b) a *= b
#else
#define LINE_OP(a, b) a = b
#endif

#ifdef NK_VORBIS_DIVIDE_TABLE
#define DIVTAB_NUMER 32
#define DIVTAB_DENOM 64
			int8 integerDivideTable[DIVTAB_NUMER][DIVTAB_DENOM]; // 2KB
#endif

			static NKENTSEU_FORCE_INLINE void NkDrawLine(float32 *output, int32 x0, int32 y0, int32 x1, int32 y1,
														 int32 n) {
				int32 dy = y1 - y0;
				int32 adx = x1 - x0;
				int32 ady = abs(dy);
				int32 base;
				int32 x = x0, y = y0;
				int32 err = 0;
				int32 sy;

#ifdef NK_VORBIS_DIVIDE_TABLE
				if (adx < DIVTAB_DENOM && ady < DIVTAB_NUMER) {
					if (dy < 0) {
						base = -integerDivideTable[ady][adx];
						sy = base - 1;
					} else {
						base = integerDivideTable[ady][adx];
						sy = base + 1;
					}
				} else {
					base = dy / adx;
					if (dy < 0)
						sy = base - 1;
					else
						sy = base + 1;
				}
#else
				base = dy / adx;
				if (dy < 0)
					sy = base - 1;
				else
					sy = base + 1;
#endif
				ady -= abs(base) * adx;
				if (x1 > n)
					x1 = n;
				if (x < x1) {
					LINE_OP(output[x], inverseDbTable[y & 255]);
					for (++x; x < x1; ++x) {
						err += ady;
						if (err >= adx) {
							err -= adx;
							y += sy;
						} else
							y += base;
						LINE_OP(output[x], inverseDbTable[y & 255]);
					}
				}
			}

			static int32 NkResidueDecode(NkVorbisDecoder *f, NkCodebook *book, float32 *target, int32 offset, int32 n,
										 int32 rtype) {
				int32 k;
				if (rtype == 0) {
					int32 step = n / book->dimensions;
					for (k = 0; k < step; ++k)
						if (!NkCodebookDecodeStep(f, book, target + offset + k, n - offset - k, step))
							return false;
				} else {
					for (k = 0; k < n;) {
						if (!NkCodebookDecode(f, book, target + offset, n - k))
							return false;
						k += book->dimensions;
						offset += book->dimensions;
					}
				}
				return true;
			}

			// n is 1/2 of the blocksize --
			// specification: "Correct per-vector decode length is [n]/2"
			static void NkDecodeResidue(NkVorbisDecoder *f, float32 *residueBuffers[], int32 ch, int32 n, int32 rn,
										uint8 *doNotDecode) {
				int32 i, j, pass;
				NkResidue *r = f->residueConfig + rn;
				int32 rtype = f->residueTypes[rn];
				int32 c = r->classbook;
				int32 classwords = f->codebooks[c].dimensions;
				uint32 actualSize = rtype == 2 ? n * 2 : n;
				uint32 limitRBegin = (r->begin < actualSize ? r->begin : actualSize);
				uint32 limitREnd = (r->end < actualSize ? r->end : actualSize);
				int32 nRead = limitREnd - limitRBegin;
				int32 partRead = nRead / r->partSize;
				int32 tempAllocPoint = tempAllocSave(f);
#ifndef NK_VORBIS_DIVIDES_IN_RESIDUE
				uint8 ***partClassdata = (uint8 ***)tempBlockArray(f, f->channels, partRead * sizeof(**partClassdata));
#else
				int32 *b *classifications =
					(int32 * b *)tempBlockArray(f, f->channels, partRead * sizeof(**classifications));
#endif

				CHECK(f);

				for (i = 0; i < ch; ++i)
					if (!doNotDecode[i])
						memset(residueBuffers[i], 0, sizeof(float32) * n);

				if (rtype == 2 && ch != 1) {
					for (j = 0; j < ch; ++j)
						if (!doNotDecode[j])
							break;
					if (j == ch)
						goto done;

					for (pass = 0; pass < 8; ++pass) {
						int32 pcount = 0, classSet = 0;
						if (ch == 2) {
							while (pcount < partRead) {
								int32 z = r->begin + pcount * r->partSize;
								int32 cInter = (z & 1), pInter = z >> 1;
								if (pass == 0) {
									NkCodebook *c = f->codebooks + r->classbook;
									int32 q;
									DECODE(q, f, c);
									if (q == EOP)
										goto done;
#ifndef NK_VORBIS_DIVIDES_IN_RESIDUE
									partClassdata[0][classSet] = r->classdata[q];
#else
									for (i = classwords - 1; i >= 0; --i) {
										classifications[0][i + pcount] = q % r->classifications;
										q /= r->classifications;
									}
#endif
								}
								for (i = 0; i < classwords && pcount < partRead; ++i, ++pcount) {
									int32 z = r->begin + pcount * r->partSize;
#ifndef NK_VORBIS_DIVIDES_IN_RESIDUE
									int32 c = partClassdata[0][classSet][i];
#else
									int32 c = classifications[0][pcount];
#endif
									int32 b = r->residueBooks[c][pass];
									if (b >= 0) {
										NkCodebook *book = f->codebooks + b;
#ifdef NK_VORBIS_DIVIDES_IN_CODEBOOK
										if (!NkCodebookDecodeDeinterleaveRepeat(f, book, residueBuffers, ch, &cInter,
																				&pInter, n, r->partSize))
											goto done;
#else
										// saves 1%
										if (!NkCodebookDecodeDeinterleaveRepeat(f, book, residueBuffers, ch, &cInter,
																				&pInter, n, r->partSize))
											goto done;
#endif
									} else {
										z += r->partSize;
										cInter = z & 1;
										pInter = z >> 1;
									}
								}
#ifndef NK_VORBIS_DIVIDES_IN_RESIDUE
								++classSet;
#endif
							}
						} else if (ch > 2) {
							while (pcount < partRead) {
								int32 z = r->begin + pcount * r->partSize;
								int32 cInter = z % ch, pInter = z / ch;
								if (pass == 0) {
									NkCodebook *c = f->codebooks + r->classbook;
									int32 q;
									DECODE(q, f, c);
									if (q == EOP)
										goto done;
#ifndef NK_VORBIS_DIVIDES_IN_RESIDUE
									partClassdata[0][classSet] = r->classdata[q];
#else
									for (i = classwords - 1; i >= 0; --i) {
										classifications[0][i + pcount] = q % r->classifications;
										q /= r->classifications;
									}
#endif
								}
								for (i = 0; i < classwords && pcount < partRead; ++i, ++pcount) {
									int32 z = r->begin + pcount * r->partSize;
#ifndef NK_VORBIS_DIVIDES_IN_RESIDUE
									int32 c = partClassdata[0][classSet][i];
#else
									int32 c = classifications[0][pcount];
#endif
									int32 b = r->residueBooks[c][pass];
									if (b >= 0) {
										NkCodebook *book = f->codebooks + b;
										if (!NkCodebookDecodeDeinterleaveRepeat(f, book, residueBuffers, ch, &cInter,
																				&pInter, n, r->partSize))
											goto done;
									} else {
										z += r->partSize;
										cInter = z % ch;
										pInter = z / ch;
									}
								}
#ifndef NK_VORBIS_DIVIDES_IN_RESIDUE
								++classSet;
#endif
							}
						}
					}
					goto done;
				}
				CHECK(f);

				for (pass = 0; pass < 8; ++pass) {
					int32 pcount = 0, classSet = 0;
					while (pcount < partRead) {
						if (pass == 0) {
							for (j = 0; j < ch; ++j) {
								if (!doNotDecode[j]) {
									NkCodebook *c = f->codebooks + r->classbook;
									int32 temp;
									DECODE(temp, f, c);
									if (temp == EOP)
										goto done;
#ifndef NK_VORBIS_DIVIDES_IN_RESIDUE
									partClassdata[j][classSet] = r->classdata[temp];
#else
									for (i = classwords - 1; i >= 0; --i) {
										classifications[j][i + pcount] = temp % r->classifications;
										temp /= r->classifications;
									}
#endif
								}
							}
						}
						for (i = 0; i < classwords && pcount < partRead; ++i, ++pcount) {
							for (j = 0; j < ch; ++j) {
								if (!doNotDecode[j]) {
#ifndef NK_VORBIS_DIVIDES_IN_RESIDUE
									int32 c = partClassdata[j][classSet][i];
#else
									int32 c = classifications[j][pcount];
#endif
									int32 b = r->residueBooks[c][pass];
									if (b >= 0) {
										float32 *target = residueBuffers[j];
										int32 offset = r->begin + pcount * r->partSize;
										int32 n = r->partSize;
										NkCodebook *book = f->codebooks + b;
										if (!NkResidueDecode(f, book, target, offset, n, rtype))
											goto done;
									}
								}
							}
						}
#ifndef NK_VORBIS_DIVIDES_IN_RESIDUE
						++classSet;
#endif
					}
				}
			done:
				CHECK(f);
#ifndef NK_VORBIS_DIVIDES_IN_RESIDUE
				tempFree(f, partClassdata);
#else
				tempFree(f, classifications);
#endif
				tempAllocRestore(f, tempAllocPoint);
			}

#if 0
// slow way for debugging
void NkInverseMdctSlow(float32 *buffer, int32 n)
{
   int32 i,j;
   int32 n2 = n >> 1;
   float32 *x = (float32 *) malloc(sizeof(*x) * n2);
   memcpy(x, buffer, sizeof(*x) * n2);
   for (i=0; i < n; ++i) {
      float32 acc = 0;
      for (j=0; j < n2; ++j)
         // formula from paper:
         //acc += n/4.0f * x[j] * (float32) cos(M_PI / 2 / n * (2 * i + 1 + n/2.0)*(2*j+1));
         // formula from wikipedia
         //acc += 2.0f / n2 * x[j] * (float32) cos(M_PI/n2 * (i + 0.5 + n2/2)*(j + 0.5));
         // these are equivalent, except the formula from the paper inverts the multiplier!
         // however, what actually works is NO MULTIPLIER!?!
         //acc += 64 * 2.0f / n2 * x[j] * (float32) cos(M_PI/n2 * (i + 0.5 + n2/2)*(j + 0.5));
         acc += x[j] * (float32) cos(M_PI / 2 / n * (2 * i + 1 + n/2.0)*(2*j+1));
      buffer[i] = acc;
   }
   free(x);
}
#elif 0
			// same as above, but just barely able to run in real time on modern machines
			void NkInverseMdctSlow(float32 *buffer, int32 n, NkVorbisDecoder *f, int32 blocktype) {
				float32 mcos[16384];
				int32 i, j;
				int32 n2 = n >> 1, nmask = (n << 2) - 1;
				float32 *x = (float32 *)malloc(sizeof(*x) * n2);
				memcpy(x, buffer, sizeof(*x) * n2);
				for (i = 0; i < 4 * n; ++i)
					mcos[i] = (float32)cos(M_PI / 2 * i / n);

				for (i = 0; i < n; ++i) {
					float32 acc = 0;
					for (j = 0; j < n2; ++j)
						acc += x[j] * mcos[(2 * i + 1 + n2) * (2 * j + 1) & nmask];
					buffer[i] = acc;
				}
				free(x);
			}
#elif 0
			// transform to use a slow dct-iv; this is STILL basically trivial,
			// but only requires half as many ops
			void NkDctIvSlow(float32 *buffer, int32 n) {
				float32 mcos[16384];
				float32 x[2048];
				int32 i, j;
				int32 n2 = n >> 1, nmask = (n << 3) - 1;
				memcpy(x, buffer, sizeof(*x) * n);
				for (i = 0; i < 8 * n; ++i)
					mcos[i] = (float32)cos(M_PI / 4 * i / n);
				for (i = 0; i < n; ++i) {
					float32 acc = 0;
					for (j = 0; j < n; ++j)
						acc += x[j] * mcos[((2 * i + 1) * (2 * j + 1)) & nmask];
					buffer[i] = acc;
				}
			}

			void NkInverseMdctSlow(float32 *buffer, int32 n, NkVorbisDecoder *f, int32 blocktype) {
				int32 i, n4 = n >> 2, n2 = n >> 1, n34 = n - n4;
				float32 temp[4096];

				memcpy(temp, buffer, n2 * sizeof(float32));
				NkDctIvSlow(temp, n2); // returns -c'-d, a-b'

				for (i = 0; i < n4; ++i)
					buffer[i] = temp[i + n4]; // a-b'
				for (; i < n34; ++i)
					buffer[i] = -temp[n34 - i - 1]; // b-a', c+d'
				for (; i < n; ++i)
					buffer[i] = -temp[i - n34]; // c'+d
			}
#endif

#ifndef LIBVORBIS_MDCT
#define LIBVORBIS_MDCT 0
#endif

#if LIBVORBIS_MDCT
			// directly call the vorbis MDCT using an interface documented
			// by Jeff Roberts... useful for performance comparison
			typedef struct {
				int32 n;
				int32 log2n;

				float32 *trig;
				int32 *bitrev;

				float32 scale;
			} mdctLookup;

			extern void NkMdctInit(mdctLookup *lookup, int32 n);
			extern void NkMdctClear(mdctLookup *l);
			extern void NkMdctBackward(mdctLookup *init, float32 *in, float32 *out);

			mdctLookup M1, M2;

			void NkInverseMdct(float32 *buffer, int32 n, NkVorbisDecoder *f, int32 blocktype) {
				mdctLookup *M;
				if (M1.n == n)
					M = &M1;
				else if (M2.n == n)
					M = &M2;
				else if (M1.n == 0) {
					NkMdctInit(&M1, n);
					M = &M1;
				} else {
					// Si les deux cache slots sont pris, c'est un bug logique : abort.
					NKENTSEU_ASSERT(M2.n == 0);
					NkMdctInit(&M2, n);
					M = &M2;
				}

				NkMdctBackward(M, buffer, buffer);
			}
#endif

			// the following were split out into separate functions while optimizing;
			// they could be pushed back up but eh. NKENTSEU_FORCE_INLINE showed no change;
			// they're probably already being inlined.
			static void NkImdctStep3Iter0Loop(int32 n, float32 *e, int32 iOff, int32 kOff, float32 *A) {
				float32 *ee0 = e + iOff;
				float32 *ee2 = ee0 + kOff;
				int32 i;

				assert((n & 3) == 0);
				for (i = (n >> 2); i > 0; --i) {
					float32 k0020, k0121;
					k0020 = ee0[0] - ee2[0];
					k0121 = ee0[-1] - ee2[-1];
					ee0[0] += ee2[0];	// ee0[ 0] = ee0[ 0] + ee2[ 0];
					ee0[-1] += ee2[-1]; // ee0[-1] = ee0[-1] + ee2[-1];
					ee2[0] = k0020 * A[0] - k0121 * A[1];
					ee2[-1] = k0121 * A[0] + k0020 * A[1];
					A += 8;

					k0020 = ee0[-2] - ee2[-2];
					k0121 = ee0[-3] - ee2[-3];
					ee0[-2] += ee2[-2]; // ee0[-2] = ee0[-2] + ee2[-2];
					ee0[-3] += ee2[-3]; // ee0[-3] = ee0[-3] + ee2[-3];
					ee2[-2] = k0020 * A[0] - k0121 * A[1];
					ee2[-3] = k0121 * A[0] + k0020 * A[1];
					A += 8;

					k0020 = ee0[-4] - ee2[-4];
					k0121 = ee0[-5] - ee2[-5];
					ee0[-4] += ee2[-4]; // ee0[-4] = ee0[-4] + ee2[-4];
					ee0[-5] += ee2[-5]; // ee0[-5] = ee0[-5] + ee2[-5];
					ee2[-4] = k0020 * A[0] - k0121 * A[1];
					ee2[-5] = k0121 * A[0] + k0020 * A[1];
					A += 8;

					k0020 = ee0[-6] - ee2[-6];
					k0121 = ee0[-7] - ee2[-7];
					ee0[-6] += ee2[-6]; // ee0[-6] = ee0[-6] + ee2[-6];
					ee0[-7] += ee2[-7]; // ee0[-7] = ee0[-7] + ee2[-7];
					ee2[-6] = k0020 * A[0] - k0121 * A[1];
					ee2[-7] = k0121 * A[0] + k0020 * A[1];
					A += 8;
					ee0 -= 8;
					ee2 -= 8;
				}
			}

			static void NkImdctStep3InnerRLoop(int32 lim, float32 *e, int32 d0, int32 kOff, float32 *A, int32 k1) {
				int32 i;
				float32 k0020, k0121;

				float32 *e0 = e + d0;
				float32 *e2 = e0 + kOff;

				for (i = lim >> 2; i > 0; --i) {
					k0020 = e0[-0] - e2[-0];
					k0121 = e0[-1] - e2[-1];
					e0[-0] += e2[-0]; // e0[-0] = e0[-0] + e2[-0];
					e0[-1] += e2[-1]; // e0[-1] = e0[-1] + e2[-1];
					e2[-0] = (k0020)*A[0] - (k0121)*A[1];
					e2[-1] = (k0121)*A[0] + (k0020)*A[1];

					A += k1;

					k0020 = e0[-2] - e2[-2];
					k0121 = e0[-3] - e2[-3];
					e0[-2] += e2[-2]; // e0[-2] = e0[-2] + e2[-2];
					e0[-3] += e2[-3]; // e0[-3] = e0[-3] + e2[-3];
					e2[-2] = (k0020)*A[0] - (k0121)*A[1];
					e2[-3] = (k0121)*A[0] + (k0020)*A[1];

					A += k1;

					k0020 = e0[-4] - e2[-4];
					k0121 = e0[-5] - e2[-5];
					e0[-4] += e2[-4]; // e0[-4] = e0[-4] + e2[-4];
					e0[-5] += e2[-5]; // e0[-5] = e0[-5] + e2[-5];
					e2[-4] = (k0020)*A[0] - (k0121)*A[1];
					e2[-5] = (k0121)*A[0] + (k0020)*A[1];

					A += k1;

					k0020 = e0[-6] - e2[-6];
					k0121 = e0[-7] - e2[-7];
					e0[-6] += e2[-6]; // e0[-6] = e0[-6] + e2[-6];
					e0[-7] += e2[-7]; // e0[-7] = e0[-7] + e2[-7];
					e2[-6] = (k0020)*A[0] - (k0121)*A[1];
					e2[-7] = (k0121)*A[0] + (k0020)*A[1];

					e0 -= 8;
					e2 -= 8;

					A += k1;
				}
			}

			static void NkImdctStep3InnerSLoop(int32 n, float32 *e, int32 iOff, int32 kOff, float32 *A, int32 aOff,
											   int32 k0) {
				int32 i;
				float32 A0 = A[0];
				float32 A1 = A[0 + 1];
				float32 A2 = A[0 + aOff];
				float32 A3 = A[0 + aOff + 1];
				float32 A4 = A[0 + aOff * 2 + 0];
				float32 A5 = A[0 + aOff * 2 + 1];
				float32 A6 = A[0 + aOff * 3 + 0];
				float32 A7 = A[0 + aOff * 3 + 1];

				float32 k00, k11;

				float32 *ee0 = e + iOff;
				float32 *ee2 = ee0 + kOff;

				for (i = n; i > 0; --i) {
					k00 = ee0[0] - ee2[0];
					k11 = ee0[-1] - ee2[-1];
					ee0[0] = ee0[0] + ee2[0];
					ee0[-1] = ee0[-1] + ee2[-1];
					ee2[0] = (k00)*A0 - (k11)*A1;
					ee2[-1] = (k11)*A0 + (k00)*A1;

					k00 = ee0[-2] - ee2[-2];
					k11 = ee0[-3] - ee2[-3];
					ee0[-2] = ee0[-2] + ee2[-2];
					ee0[-3] = ee0[-3] + ee2[-3];
					ee2[-2] = (k00)*A2 - (k11)*A3;
					ee2[-3] = (k11)*A2 + (k00)*A3;

					k00 = ee0[-4] - ee2[-4];
					k11 = ee0[-5] - ee2[-5];
					ee0[-4] = ee0[-4] + ee2[-4];
					ee0[-5] = ee0[-5] + ee2[-5];
					ee2[-4] = (k00)*A4 - (k11)*A5;
					ee2[-5] = (k11)*A4 + (k00)*A5;

					k00 = ee0[-6] - ee2[-6];
					k11 = ee0[-7] - ee2[-7];
					ee0[-6] = ee0[-6] + ee2[-6];
					ee0[-7] = ee0[-7] + ee2[-7];
					ee2[-6] = (k00)*A6 - (k11)*A7;
					ee2[-7] = (k11)*A6 + (k00)*A7;

					ee0 -= k0;
					ee2 -= k0;
				}
			}

			static NKENTSEU_FORCE_INLINE void NkIter54(float32 *z) {
				float32 k00, k11, k22, k33;
				float32 y0, y1, y2, y3;

				k00 = z[0] - z[-4];
				y0 = z[0] + z[-4];
				y2 = z[-2] + z[-6];
				k22 = z[-2] - z[-6];

				z[-0] = y0 + y2; // z0 + z4 + z2 + z6
				z[-2] = y0 - y2; // z0 + z4 - z2 - z6

				// done with y0,y2

				k33 = z[-3] - z[-7];

				z[-4] = k00 + k33; // z0 - z4 + z3 - z7
				z[-6] = k00 - k33; // z0 - z4 - z3 + z7

				// done with k33

				k11 = z[-1] - z[-5];
				y1 = z[-1] + z[-5];
				y3 = z[-3] + z[-7];

				z[-1] = y1 + y3;   // z1 + z5 + z3 + z7
				z[-3] = y1 - y3;   // z1 + z5 - z3 - z7
				z[-5] = k11 - k22; // z1 - z5 + z2 - z6
				z[-7] = k11 + k22; // z1 - z5 - z2 + z6
			}

			static void NkImdctStep3InnerSLoopLd654(int32 n, float32 *e, int32 iOff, float32 *A, int32 baseN) {
				int32 aOff = baseN >> 3;
				float32 A2 = A[0 + aOff];
				float32 *z = e + iOff;
				float32 *base = z - 16 * n;

				while (z > base) {
					float32 k00, k11;
					float32 l00, l11;

					k00 = z[-0] - z[-8];
					k11 = z[-1] - z[-9];
					l00 = z[-2] - z[-10];
					l11 = z[-3] - z[-11];
					z[-0] = z[-0] + z[-8];
					z[-1] = z[-1] + z[-9];
					z[-2] = z[-2] + z[-10];
					z[-3] = z[-3] + z[-11];
					z[-8] = k00;
					z[-9] = k11;
					z[-10] = (l00 + l11) * A2;
					z[-11] = (l11 - l00) * A2;

					k00 = z[-4] - z[-12];
					k11 = z[-5] - z[-13];
					l00 = z[-6] - z[-14];
					l11 = z[-7] - z[-15];
					z[-4] = z[-4] + z[-12];
					z[-5] = z[-5] + z[-13];
					z[-6] = z[-6] + z[-14];
					z[-7] = z[-7] + z[-15];
					z[-12] = k11;
					z[-13] = -k00;
					z[-14] = (l11 - l00) * A2;
					z[-15] = (l00 + l11) * -A2;

					NkIter54(z);
					NkIter54(z - 8);
					z -= 16;
				}
			}

			static void NkInverseMdct(float32 *buffer, int32 n, NkVorbisDecoder *f, int32 blocktype) {
				int32 n2 = n >> 1, n4 = n >> 2, n8 = n >> 3, l;
				int32 ld;
				// @OPTIMIZE: reduce register pressure by using fewer variables?
				int32 savePoint = tempAllocSave(f);
				float32 *buf2 = (float32 *)tempAlloc(f, n2 * sizeof(*buf2));
				float32 *u = NULL, *v = NULL;
				// twiddle factors
				float32 *A = f->A[blocktype];

				// IMDCT algorithm from "The use of multirate filter banks for coding of high quality digital audio"
				// See notes about bugs in that paper in less-optimal implementation 'NkInverseMdct_old' after this function.

				// kernel from paper

				// merged:
				//   copy and reflect spectral data
				//   step 0

				// note that it turns out that the items added together during
				// this step are, in fact, being added to themselves (as reflected
				// by step 0). inexplicable inefficiency! this became obvious
				// once I combined the passes.

				// so there's a missing 'times 2' here (for adding X to itself).
				// this propagates through linearly to the end, where the numbers
				// are 1/2 too small, and need to be compensated for.

				{
					float32 *d, *e, *AA, *eStop;
					d = &buf2[n2 - 2];
					AA = A;
					e = &buffer[0];
					eStop = &buffer[n2];
					while (e != eStop) {
						d[1] = (e[0] * AA[0] - e[2] * AA[1]);
						d[0] = (e[0] * AA[1] + e[2] * AA[0]);
						d -= 2;
						AA += 2;
						e += 4;
					}

					e = &buffer[n2 - 3];
					while (d >= buf2) {
						d[1] = (-e[2] * AA[0] - -e[0] * AA[1]);
						d[0] = (-e[2] * AA[1] + -e[0] * AA[0]);
						d -= 2;
						AA += 2;
						e -= 4;
					}
				}

				// now we use symbolic names for these, so that we can
				// possibly swap their meaning as we change which operations
				// are in place

				u = buffer;
				v = buf2;

				// step 2    (paper output is w, now u)
				// this could be in place, but the data ends up in the wrong
				// place... _somebody_'s got to swap it, so this is nominated
				{
					float32 *AA = &A[n2 - 8];
					float32 *d0, *d1, *e0, *e1;

					e0 = &v[n4];
					e1 = &v[0];

					d0 = &u[n4];
					d1 = &u[0];

					while (AA >= A) {
						float32 v4020, v4121;

						v4121 = e0[1] - e1[1];
						v4020 = e0[0] - e1[0];
						d0[1] = e0[1] + e1[1];
						d0[0] = e0[0] + e1[0];
						d1[1] = v4121 * AA[4] - v4020 * AA[5];
						d1[0] = v4020 * AA[4] + v4121 * AA[5];

						v4121 = e0[3] - e1[3];
						v4020 = e0[2] - e1[2];
						d0[3] = e0[3] + e1[3];
						d0[2] = e0[2] + e1[2];
						d1[3] = v4121 * AA[0] - v4020 * AA[1];
						d1[2] = v4020 * AA[0] + v4121 * AA[1];

						AA -= 8;

						d0 += 4;
						d1 += 4;
						e0 += 4;
						e1 += 4;
					}
				}

				// step 3
				ld = NkIlog(n) - 1; // NkIlog is off-by-one from normal definitions

				// optimized step 3:

				// the original step3 loop can be nested r inside s or s inside r;
				// it's written originally as s inside r, but this is dumb when r
				// iterates many times, and s few. So I have two copies of it and
				// switch between them halfway.

				// this is iteration 0 of step 3
				NkImdctStep3Iter0Loop(n >> 4, u, n2 - 1 - n4 * 0, -(n >> 3), A);
				NkImdctStep3Iter0Loop(n >> 4, u, n2 - 1 - n4 * 1, -(n >> 3), A);

				// this is iteration 1 of step 3
				NkImdctStep3InnerRLoop(n >> 5, u, n2 - 1 - n8 * 0, -(n >> 4), A, 16);
				NkImdctStep3InnerRLoop(n >> 5, u, n2 - 1 - n8 * 1, -(n >> 4), A, 16);
				NkImdctStep3InnerRLoop(n >> 5, u, n2 - 1 - n8 * 2, -(n >> 4), A, 16);
				NkImdctStep3InnerRLoop(n >> 5, u, n2 - 1 - n8 * 3, -(n >> 4), A, 16);

				l = 2;
				for (; l < (ld - 3) >> 1; ++l) {
					int32 k0 = n >> (l + 2), k02 = k0 >> 1;
					int32 lim = 1 << (l + 1);
					int32 i;
					for (i = 0; i < lim; ++i)
						NkImdctStep3InnerRLoop(n >> (l + 4), u, n2 - 1 - k0 * i, -k02, A, 1 << (l + 3));
				}

				for (; l < ld - 6; ++l) {
					int32 k0 = n >> (l + 2), k1 = 1 << (l + 3), k02 = k0 >> 1;
					int32 rlim = n >> (l + 6), r;
					int32 lim = 1 << (l + 1);
					int32 iOff;
					float32 *A0 = A;
					iOff = n2 - 1;
					for (r = rlim; r > 0; --r) {
						NkImdctStep3InnerSLoop(lim, u, iOff, -k02, A0, k1, k0);
						A0 += k1 * 4;
						iOff -= 8;
					}
				}

				// iterations with count:
				//   ld-6,-5,-4 all interleaved together
				//       the big win comes from getting rid of needless flops
				//         due to the constants on pass 5 & 4 being all 1 and 0;
				//       combining them to be simultaneous to improve cache made little difference
				NkImdctStep3InnerSLoopLd654(n >> 5, u, n2 - 1, A, n);

				// output is u

				// step 4, 5, and 6
				// cannot be in-place because of step 5
				{
					uint16 *bitrev = f->NkBitReverse[blocktype];
					// weirdly, I'd have thought reading sequentially and writing
					// erratically would have been better than vice-versa, but in
					// fact that's not what my testing showed. (That is, with
					// j = bitreverse(i), do you read i and write j, or read j and write i.)

					float32 *d0 = &v[n4 - 4];
					float32 *d1 = &v[n2 - 4];
					while (d0 >= v) {
						int32 k4;

						k4 = bitrev[0];
						d1[3] = u[k4 + 0];
						d1[2] = u[k4 + 1];
						d0[3] = u[k4 + 2];
						d0[2] = u[k4 + 3];

						k4 = bitrev[1];
						d1[1] = u[k4 + 0];
						d1[0] = u[k4 + 1];
						d0[1] = u[k4 + 2];
						d0[0] = u[k4 + 3];

						d0 -= 4;
						d1 -= 4;
						bitrev += 2;
					}
				}
				// (paper output is u, now v)

				// data must be in buf2
				assert(v == buf2);

				// step 7   (paper output is v, now v)
				// this is now in place
				{
					float32 *C = f->C[blocktype];
					float32 *d, *e;

					d = v;
					e = v + n2 - 4;

					while (d < e) {
						float32 a02, a11, b0, b1, b2, b3;

						a02 = d[0] - e[2];
						a11 = d[1] + e[3];

						b0 = C[1] * a02 + C[0] * a11;
						b1 = C[1] * a11 - C[0] * a02;

						b2 = d[0] + e[2];
						b3 = d[1] - e[3];

						d[0] = b2 + b0;
						d[1] = b3 + b1;
						e[2] = b2 - b0;
						e[3] = b1 - b3;

						a02 = d[2] - e[0];
						a11 = d[3] + e[1];

						b0 = C[3] * a02 + C[2] * a11;
						b1 = C[3] * a11 - C[2] * a02;

						b2 = d[2] + e[0];
						b3 = d[3] - e[1];

						d[2] = b2 + b0;
						d[3] = b3 + b1;
						e[0] = b2 - b0;
						e[1] = b1 - b3;

						C += 4;
						d += 4;
						e -= 4;
					}
				}

				// data must be in buf2

				// step 8+decode   (paper output is X, now buffer)
				// this generates pairs of data a la 8 and pushes them directly through
				// the decode kernel (pushing rather than pulling) to avoid having
				// to make another pass later

				// this cannot POSSIBLY be in place, so we refer to the buffers directly

				{
					float32 *d0, *d1, *d2, *d3;

					float32 *B = f->B[blocktype] + n2 - 8;
					float32 *e = buf2 + n2 - 8;
					d0 = &buffer[0];
					d1 = &buffer[n2 - 4];
					d2 = &buffer[n2];
					d3 = &buffer[n - 4];
					while (e >= v) {
						float32 p0, p1, p2, p3;

						p3 = e[6] * B[7] - e[7] * B[6];
						p2 = -e[6] * B[6] - e[7] * B[7];

						d0[0] = p3;
						d1[3] = -p3;
						d2[0] = p2;
						d3[3] = p2;

						p1 = e[4] * B[5] - e[5] * B[4];
						p0 = -e[4] * B[4] - e[5] * B[5];

						d0[1] = p1;
						d1[2] = -p1;
						d2[1] = p0;
						d3[2] = p0;

						p3 = e[2] * B[3] - e[3] * B[2];
						p2 = -e[2] * B[2] - e[3] * B[3];

						d0[2] = p3;
						d1[1] = -p3;
						d2[2] = p2;
						d3[1] = p2;

						p1 = e[0] * B[1] - e[1] * B[0];
						p0 = -e[0] * B[0] - e[1] * B[1];

						d0[3] = p1;
						d1[0] = -p1;
						d2[3] = p0;
						d3[0] = p0;

						B -= 8;
						e -= 8;
						d0 += 4;
						d2 += 4;
						d1 -= 4;
						d3 -= 4;
					}
				}

				tempFree(f, buf2);
				tempAllocRestore(f, savePoint);
			}

#if 0
// this is the original version of the above code, if you want to optimize it from scratch
void NkInverseMdctNaive(float32 *buffer, int32 n)
{
   float32 s;
   float32 A[1 << 12], B[1 << 12], C[1 << 11];
   int32 i,k,k2,k4, n2 = n >> 1, n4 = n >> 2, n8 = n >> 3, l;
   int32 n34 = n - n4, ld;
   // how can they claim this only uses N words?!
   // oh, because they're only used sparsely, whoops
   float32 u[1 << 13], X[1 << 13], v[1 << 13], w[1 << 13];
   // set up twiddle factors

   for (k=k2=0; k < n4; ++k,k2+=2) {
      A[k2  ] = (float32)  cos(4*k*M_PI/n);
      A[k2+1] = (float32) -sin(4*k*M_PI/n);
      B[k2  ] = (float32)  cos((k2+1)*M_PI/n/2);
      B[k2+1] = (float32)  sin((k2+1)*M_PI/n/2);
   }
   for (k=k2=0; k < n8; ++k,k2+=2) {
      C[k2  ] = (float32)  cos(2*(k2+1)*M_PI/n);
      C[k2+1] = (float32) -sin(2*(k2+1)*M_PI/n);
   }

   // IMDCT algorithm from "The use of multirate filter banks for coding of high quality digital audio"
   // Note there are bugs in that pseudocode, presumably due to them attempting
   // to rename the arrays nicely rather than representing the way their actual
   // implementation bounces buffers back and forth. As a result, even in the
   // "some formulars corrected" version, a direct implementation fails. These
   // are noted below as "paper bug".

   // copy and reflect spectral data
   for (k=0; k < n2; ++k) u[k] = buffer[k];
   for (   ; k < n ; ++k) u[k] = -buffer[n - k - 1];
   // kernel from paper
   // step 1
   for (k=k2=k4=0; k < n4; k+=1, k2+=2, k4+=4) {
      v[n-k4-1] = (u[k4] - u[n-k4-1]) * A[k2]   - (u[k4+2] - u[n-k4-3])*A[k2+1];
      v[n-k4-3] = (u[k4] - u[n-k4-1]) * A[k2+1] + (u[k4+2] - u[n-k4-3])*A[k2];
   }
   // step 2
   for (k=k4=0; k < n8; k+=1, k4+=4) {
      w[n2+3+k4] = v[n2+3+k4] + v[k4+3];
      w[n2+1+k4] = v[n2+1+k4] + v[k4+1];
      w[k4+3]    = (v[n2+3+k4] - v[k4+3])*A[n2-4-k4] - (v[n2+1+k4]-v[k4+1])*A[n2-3-k4];
      w[k4+1]    = (v[n2+1+k4] - v[k4+1])*A[n2-4-k4] + (v[n2+3+k4]-v[k4+3])*A[n2-3-k4];
   }
   // step 3
   ld = NkIlog(n) - 1; // NkIlog is off-by-one from normal definitions
   for (l=0; l < ld-3; ++l) {
      int32 k0 = n >> (l+2), k1 = 1 << (l+3);
      int32 rlim = n >> (l+4), r4, r;
      int32 s2lim = 1 << (l+2), s2;
      for (r=r4=0; r < rlim; r4+=4,++r) {
         for (s2=0; s2 < s2lim; s2+=2) {
            u[n-1-k0*s2-r4] = w[n-1-k0*s2-r4] + w[n-1-k0*(s2+1)-r4];
            u[n-3-k0*s2-r4] = w[n-3-k0*s2-r4] + w[n-3-k0*(s2+1)-r4];
            u[n-1-k0*(s2+1)-r4] = (w[n-1-k0*s2-r4] - w[n-1-k0*(s2+1)-r4]) * A[r*k1]
                                - (w[n-3-k0*s2-r4] - w[n-3-k0*(s2+1)-r4]) * A[r*k1+1];
            u[n-3-k0*(s2+1)-r4] = (w[n-3-k0*s2-r4] - w[n-3-k0*(s2+1)-r4]) * A[r*k1]
                                + (w[n-1-k0*s2-r4] - w[n-1-k0*(s2+1)-r4]) * A[r*k1+1];
         }
      }
      if (l+1 < ld-3) {
         // paper bug: ping-ponging of u&w here is omitted
         memcpy(w, u, sizeof(u));
      }
   }

   // step 4
   for (i=0; i < n8; ++i) {
      int32 j = NkBitReverse(i) >> (32-ld+3);
      assert(j < n8);
      if (i == j) {
         // paper bug: original code probably swapped in place; if copying,
         //            need to directly copy in this case
         int32 i8 = i << 3;
         v[i8+1] = u[i8+1];
         v[i8+3] = u[i8+3];
         v[i8+5] = u[i8+5];
         v[i8+7] = u[i8+7];
      } else if (i < j) {
         int32 i8 = i << 3, j8 = j << 3;
         v[j8+1] = u[i8+1], v[i8+1] = u[j8 + 1];
         v[j8+3] = u[i8+3], v[i8+3] = u[j8 + 3];
         v[j8+5] = u[i8+5], v[i8+5] = u[j8 + 5];
         v[j8+7] = u[i8+7], v[i8+7] = u[j8 + 7];
      }
   }
   // step 5
   for (k=0; k < n2; ++k) {
      w[k] = v[k*2+1];
   }
   // step 6
   for (k=k2=k4=0; k < n8; ++k, k2 += 2, k4 += 4) {
      u[n-1-k2] = w[k4];
      u[n-2-k2] = w[k4+1];
      u[n34 - 1 - k2] = w[k4+2];
      u[n34 - 2 - k2] = w[k4+3];
   }
   // step 7
   for (k=k2=0; k < n8; ++k, k2 += 2) {
      v[n2 + k2 ] = ( u[n2 + k2] + u[n-2-k2] + C[k2+1]*(u[n2+k2]-u[n-2-k2]) + C[k2]*(u[n2+k2+1]+u[n-2-k2+1]))/2;
      v[n-2 - k2] = ( u[n2 + k2] + u[n-2-k2] - C[k2+1]*(u[n2+k2]-u[n-2-k2]) - C[k2]*(u[n2+k2+1]+u[n-2-k2+1]))/2;
      v[n2+1+ k2] = ( u[n2+1+k2] - u[n-1-k2] + C[k2+1]*(u[n2+1+k2]+u[n-1-k2]) - C[k2]*(u[n2+k2]-u[n-2-k2]))/2;
      v[n-1 - k2] = (-u[n2+1+k2] + u[n-1-k2] + C[k2+1]*(u[n2+1+k2]+u[n-1-k2]) - C[k2]*(u[n2+k2]-u[n-2-k2]))/2;
   }
   // step 8
   for (k=k2=0; k < n4; ++k,k2 += 2) {
      X[k]      = v[k2+n2]*B[k2  ] + v[k2+1+n2]*B[k2+1];
      X[n2-1-k] = v[k2+n2]*B[k2+1] - v[k2+1+n2]*B[k2  ];
   }

   // decode kernel to output
   // determined the following value experimentally
   // (by first figuring out what made NkInverseMdctSlow work); then matching that here
   // (probably vorbis encoder premultiplies by n or n/2, to save it on the decoder?)
   s = 0.5; // theoretically would be n4

   // [[[ note! the s value of 0.5 is compensated for by the B[] in the current code,
   //     so it needs to use the "old" B values to behave correctly, or else
   //     set s to 1.0 ]]]
   for (i=0; i < n4  ; ++i) buffer[i] = s * X[i+n4];
   for (   ; i < n34; ++i) buffer[i] = -s * X[n34 - i - 1];
   for (   ; i < n   ; ++i) buffer[i] = -s * X[i - n34];
}
#endif

			static float32 *getWindow(NkVorbisDecoder *f, int32 len) {
				len <<= 1;
				if (len == f->blocksize0)
					return f->window[0];
				if (len == f->blocksize1)
					return f->window[1];
				return NULL;
			}

#ifndef NK_VORBIS_NO_DEFER_FLOOR
			typedef int16 YTYPE;
#else
			typedef int32 YTYPE;
#endif
			static int32 NkDoFloor(NkVorbisDecoder *f, NkMapping *map, int32 i, int32 n, float32 *target, YTYPE *finalY,
								   uint8 *step2Flag) {
				int32 n2 = n >> 1;
				int32 s = map->chan[i].mux, floor;
				floor = map->submapFloor[s];
				if (f->floorTypes[floor] == 0) {
					return error(f, NK_VORBIS_INVALID_STREAM);
				} else {
					NkFloor1 *g = &f->floorConfig[floor].floor1;
					int32 j, q;
					int32 lx = 0, ly = finalY[0] * g->floor1Multiplier;
					for (q = 1; q < g->values; ++q) {
						j = g->sortedOrder[q];
#ifndef NK_VORBIS_NO_DEFER_FLOOR
						NKENTSEU_UNUSED(step2Flag);
						if (finalY[j] >= 0)
#else
						if (step2Flag[j])
#endif
						{
							int32 hy = finalY[j] * g->floor1Multiplier;
							int32 hx = g->Xlist[j];
							if (lx != hx)
								NkDrawLine(target, lx, ly, hx, hy, n2);
							CHECK(f);
							lx = hx, ly = hy;
						}
					}
					if (lx < n2) {
						// optimization of: NkDrawLine(target, lx,ly, n,ly, n2);
						for (j = lx; j < n2; ++j)
							LINE_OP(target[j], inverseDbTable[ly]);
						CHECK(f);
					}
				}
				return true;
			}

			// The meaning of "left" and "right"
			//
			// For a given frame:
			//     we compute samples from 0..n
			//     windowCenter is n/2
			//     we'll window and mix the samples from leftStart to leftEnd with data from the previous frame
			//     all of the samples from leftEnd to rightStart can be output without mixing; however,
			//        this interval is 0-length except when transitioning between int16 and int64 frames
			//     all of the samples from rightStart to rightEnd need to be mixed with the next frame,
			//        which we don't have, so those get saved in a buffer
			//     frame N's rightEnd-rightStart, the number of samples to mix with the next frame,
			//        has to be the same as frame N+1's leftEnd-leftStart (which they are by
			//        construction)

			static int32 NkVorbisDecodeInitial(NkVorbisDecoder *f, int32 *pLeftStart, int32 *pLeftEnd,
											   int32 *pRightStart, int32 *pRightEnd, int32 *mode) {
				NkMode *m;
				int32 i, n, prev, next, windowCenter;
				f->channelBufferStart = f->channelBufferEnd = 0;

			retry:
				if (f->eof)
					return false;
				if (!NkMaybeStartPacket(f))
					return false;
				// check packet type
				if (getBits(f, 1) != 0) {
					if (IS_PUSH_MODE(f))
						return error(f, NK_VORBIS_BAD_PACKET_TYPE);
					while (EOP != NkGet8Packet(f))
						;
					goto retry;
				}

				if (f->alloc.allocBuffer)
					assert(f->alloc.allocBufferLengthInBytes == f->tempOffset);

				i = getBits(f, NkIlog(f->modeCount - 1));
				if (i == EOP)
					return false;
				if (i >= f->modeCount)
					return false;
				*mode = i;
				m = f->modeConfig + i;
				if (m->blockflag) {
					n = f->blocksize1;
					prev = getBits(f, 1);
					next = getBits(f, 1);
				} else {
					prev = next = 0;
					n = f->blocksize0;
				}

				// WINDOWING

				windowCenter = n >> 1;
				if (m->blockflag && !prev) {
					*pLeftStart = (n - f->blocksize0) >> 2;
					*pLeftEnd = (n + f->blocksize0) >> 2;
				} else {
					*pLeftStart = 0;
					*pLeftEnd = windowCenter;
				}
				if (m->blockflag && !next) {
					*pRightStart = (n * 3 - f->blocksize0) >> 2;
					*pRightEnd = (n * 3 + f->blocksize0) >> 2;
				} else {
					*pRightStart = windowCenter;
					*pRightEnd = n;
				}

				return true;
			}

			static int32 NkVorbisDecodePacketRest(NkVorbisDecoder *f, int32 *len, NkMode *m, int32 leftStart,
												  int32 leftEnd, int32 rightStart, int32 rightEnd, int32 *pLeft) {
				NkMapping *map;
				int32 i, j, k, n, n2;
				int32 zeroChannel[256];
				int32 reallyZeroChannel[256];

				// WINDOWING

				NKENTSEU_UNUSED(leftEnd);
				n = f->blocksize[m->blockflag];
				map = &f->mapping[m->mapping];

				// FLOORS
				n2 = n >> 1;

				CHECK(f);

				for (i = 0; i < f->channels; ++i) {
					int32 s = map->chan[i].mux, floor;
					zeroChannel[i] = false;
					floor = map->submapFloor[s];
					if (f->floorTypes[floor] == 0) {
						return error(f, NK_VORBIS_INVALID_STREAM);
					} else {
						NkFloor1 *g = &f->floorConfig[floor].floor1;
						if (getBits(f, 1)) {
							int16 *finalY;
							uint8 step2Flag[256];
							static int32 rangeList[4] = {256, 128, 86, 64};
							int32 range = rangeList[g->floor1Multiplier - 1];
							int32 offset = 2;
							finalY = f->finalY[i];
							finalY[0] = getBits(f, NkIlog(range) - 1);
							finalY[1] = getBits(f, NkIlog(range) - 1);
							for (j = 0; j < g->partitions; ++j) {
								int32 pclass = g->partitionClassList[j];
								int32 cdim = g->classDimensions[pclass];
								int32 cbits = g->classSubclasses[pclass];
								int32 csub = (1 << cbits) - 1;
								int32 cval = 0;
								if (cbits) {
									NkCodebook *c = f->codebooks + g->classMasterbooks[pclass];
									DECODE(cval, f, c);
								}
								for (k = 0; k < cdim; ++k) {
									int32 book = g->subclassBooks[pclass][cval & csub];
									cval = cval >> cbits;
									if (book >= 0) {
										int32 temp;
										NkCodebook *c = f->codebooks + book;
										DECODE(temp, f, c);
										finalY[offset++] = temp;
									} else
										finalY[offset++] = 0;
								}
							}
							if (f->validBits == INVALID_BITS)
								goto error; // behavior according to spec
							step2Flag[0] = step2Flag[1] = 1;
							for (j = 2; j < g->values; ++j) {
								int32 low, high, pred, highroom, lowroom, room, val;
								low = g->NkNeighbors[j][0];
								high = g->NkNeighbors[j][1];
								// NkNeighbors(g->Xlist, j, &low, &high);
								pred = NkPredictPoint(g->Xlist[j], g->Xlist[low], g->Xlist[high], finalY[low],
													  finalY[high]);
								val = finalY[j];
								highroom = range - pred;
								lowroom = pred;
								if (highroom < lowroom)
									room = highroom * 2;
								else
									room = lowroom * 2;
								if (val) {
									step2Flag[low] = step2Flag[high] = 1;
									step2Flag[j] = 1;
									if (val >= room)
										if (highroom > lowroom)
											finalY[j] = val - lowroom + pred;
										else
											finalY[j] = pred - val + highroom - 1;
									else if (val & 1)
										finalY[j] = pred - ((val + 1) >> 1);
									else
										finalY[j] = pred + (val >> 1);
								} else {
									step2Flag[j] = 0;
									finalY[j] = pred;
								}
							}

#ifdef NK_VORBIS_NO_DEFER_FLOOR
							NkDoFloor(f, map, i, n, f->floorBuffers[i], finalY, step2Flag);
#else
							// defer final floor computation until _after_ residue
							for (j = 0; j < g->values; ++j) {
								if (!step2Flag[j])
									finalY[j] = -1;
							}
#endif
						} else {
						error:
							zeroChannel[i] = true;
						}
						// So we just defer everything else to later

						// at this point we've decoded the floor into buffer
					}
				}
				CHECK(f);
				// at this point we've decoded all floors

				if (f->alloc.allocBuffer)
					assert(f->alloc.allocBufferLengthInBytes == f->tempOffset);

				// re-enable coupled channels if necessary
				memcpy(reallyZeroChannel, zeroChannel, sizeof(reallyZeroChannel[0]) * f->channels);
				for (i = 0; i < map->couplingSteps; ++i)
					if (!zeroChannel[map->chan[i].magnitude] || !zeroChannel[map->chan[i].angle]) {
						zeroChannel[map->chan[i].magnitude] = zeroChannel[map->chan[i].angle] = false;
					}

				CHECK(f);
				// RESIDUE DECODE
				for (i = 0; i < map->submaps; ++i) {
					float32 *residueBuffers[NK_VORBIS_MAX_CHANNELS];
					int32 r;
					uint8 doNotDecode[256];
					int32 ch = 0;
					for (j = 0; j < f->channels; ++j) {
						if (map->chan[j].mux == i) {
							if (zeroChannel[j]) {
								doNotDecode[ch] = true;
								residueBuffers[ch] = NULL;
							} else {
								doNotDecode[ch] = false;
								residueBuffers[ch] = f->channelBuffers[j];
							}
							++ch;
						}
					}
					r = map->submapResidue[i];
					NkDecodeResidue(f, residueBuffers, ch, n2, r, doNotDecode);
				}

				if (f->alloc.allocBuffer)
					assert(f->alloc.allocBufferLengthInBytes == f->tempOffset);
				CHECK(f);

				// INVERSE COUPLING
				for (i = map->couplingSteps - 1; i >= 0; --i) {
					int32 n2 = n >> 1;
					float32 *m = f->channelBuffers[map->chan[i].magnitude];
					float32 *a = f->channelBuffers[map->chan[i].angle];
					for (j = 0; j < n2; ++j) {
						float32 a2, m2;
						if (m[j] > 0)
							if (a[j] > 0)
								m2 = m[j], a2 = m[j] - a[j];
							else
								a2 = m[j], m2 = m[j] + a[j];
						else if (a[j] > 0)
							m2 = m[j], a2 = m[j] + a[j];
						else
							a2 = m[j], m2 = m[j] - a[j];
						m[j] = m2;
						a[j] = a2;
					}
				}
				CHECK(f);

				// finish decoding the floors
#ifndef NK_VORBIS_NO_DEFER_FLOOR
				for (i = 0; i < f->channels; ++i) {
					if (reallyZeroChannel[i]) {
						memset(f->channelBuffers[i], 0, sizeof(*f->channelBuffers[i]) * n2);
					} else {
						NkDoFloor(f, map, i, n, f->channelBuffers[i], f->finalY[i], NULL);
					}
				}
#else
				for (i = 0; i < f->channels; ++i) {
					if (reallyZeroChannel[i]) {
						memset(f->channelBuffers[i], 0, sizeof(*f->channelBuffers[i]) * n2);
					} else {
						for (j = 0; j < n2; ++j)
							f->channelBuffers[i][j] *= f->floorBuffers[i][j];
					}
				}
#endif

				// INVERSE MDCT
				CHECK(f);
				for (i = 0; i < f->channels; ++i)
					NkInverseMdct(f->channelBuffers[i], n, f, m->blockflag);
				CHECK(f);

				// this shouldn't be necessary, unless we exited on an error
				// and want to flush to get to the next packet
				NkFlushPacket(f);

				if (f->firstDecode) {
					// assume we start so first non-discarded sample is sample 0
					// this isn't to spec, but spec would require us to read ahead
					// and decode the size of all current frames--could be done,
					// but presumably it's not a commonly used feature
					f->currentLoc =
						0u - n2; // start of first frame is positioned for discard (NB this is an intentional uint32
								 // overflow/wrap-around)
					// we might have to discard samples "from" the next frame too,
					// if we're lapping a large block then a small at the start?
					f->discardSamplesDeferred = n - rightEnd;
					f->currentLocValid = true;
					f->firstDecode = false;
				} else if (f->discardSamplesDeferred) {
					if (f->discardSamplesDeferred >= rightStart - leftStart) {
						f->discardSamplesDeferred -= (rightStart - leftStart);
						leftStart = rightStart;
						*pLeft = leftStart;
					} else {
						leftStart += f->discardSamplesDeferred;
						*pLeft = leftStart;
						f->discardSamplesDeferred = 0;
					}
				} else if (f->previousLength == 0 && f->currentLocValid) {
					// we're recovering from a seek... that means we're going to discard
					// the samples from this packet even though we know our position from
					// the last page header, so we need to update the position based on
					// the discarded samples here
					// but wait, the code below is going to add this in itself even
					// on a discard, so we don't need to do it here...
				}

				// check if we have ogg information about the sample # for this packet
				if (f->lastSegWhich == f->endSegWithKnownLoc) {
					// if we have a valid current loc, and this is final:
					if (f->currentLocValid && (f->pageFlag & PAGEFLAG_last_page)) {
						uint32 currentEnd = f->knownLocForPacket;
						// then let's infer the size of the (probably) int16 final frame
						if (currentEnd < f->currentLoc + (rightEnd - leftStart)) {
							if (currentEnd < f->currentLoc) {
								// negative truncation, that's impossible!
								*len = 0;
							} else {
								*len = currentEnd - f->currentLoc;
							}
							*len += leftStart; // this doesn't seem right, but has no ill effect on my test files
							if (*len > rightEnd)
								*len = rightEnd; // this should never happen
							f->currentLoc += *len;
							return true;
						}
					}
					// otherwise, just set our sample loc
					// guess that the ogg granule pos refers to the _middle_ of the
					// last frame?
					// set f->currentLoc to the position of leftStart
					f->currentLoc = f->knownLocForPacket - (n2 - leftStart);
					f->currentLocValid = true;
				}
				if (f->currentLocValid)
					f->currentLoc += (rightStart - leftStart);

				if (f->alloc.allocBuffer)
					assert(f->alloc.allocBufferLengthInBytes == f->tempOffset);
				*len = rightEnd; // ignore samples after the window goes to 0
				CHECK(f);

				return true;
			}

			static int32 NkVorbisDecodePacket(NkVorbisDecoder *f, int32 *len, int32 *pLeft, int32 *pRight) {
				int32 mode, leftEnd, rightEnd;
				if (!NkVorbisDecodeInitial(f, pLeft, &leftEnd, pRight, &rightEnd, &mode))
					return 0;
				return NkVorbisDecodePacketRest(f, len, f->modeConfig + mode, *pLeft, leftEnd, *pRight, rightEnd,
												pLeft);
			}

			static int32 NkVorbisFinishFrame(NkVorbisDecoder *f, int32 len, int32 left, int32 right) {
				int32 prev, i, j;
				// we use right&left (the start of the right- and left-window sin()-regions)
				// to determine how much to return, rather than inferring from the rules
				// (same result, clearer code); 'left' indicates where our sin() window
				// starts, therefore where the previous window's right edge starts, and
				// therefore where to start mixing from the previous buffer. 'right'
				// indicates where our sin() ending-window starts, therefore that's where
				// we start saving, and where our returned-data ends.

				// mixin from previous window
				if (f->previousLength) {
					int32 i, j, n = f->previousLength;
					float32 *w = getWindow(f, n);
					if (w == NULL)
						return 0;
					for (i = 0; i < f->channels; ++i) {
						for (j = 0; j < n; ++j)
							f->channelBuffers[i][left + j] =
								f->channelBuffers[i][left + j] * w[j] + f->previousWindow[i][j] * w[n - 1 - j];
					}
				}

				prev = f->previousLength;

				// last half of this data becomes previous window
				f->previousLength = len - right;

				// @OPTIMIZE: could avoid this copy by float64-buffering the
				// output (flipping previousWindow with channelBuffers), but
				// then previousWindow would have to be 2x as large, and
				// channelBuffers couldn't be temp mem (although they're NOT
				// currently temp mem, they could be (unless we want to level
				// performance by spreading out the computation))
				for (i = 0; i < f->channels; ++i)
					for (j = 0; right + j < len; ++j)
						f->previousWindow[i][j] = f->channelBuffers[i][right + j];

				if (!prev)
					// there was no previous packet, so this data isn't valid...
					// this isn't entirely true, only the would-have-overlapped data
					// isn't valid, but this seems to be what the spec requires
					return 0;

				// truncate a int16 frame
				if (len < right)
					right = len;

				f->samplesOutput += right - left;

				return right - left;
			}

			static int32 NkVorbisPumpFirstFrame(NkVorbisDecoder *f) {
				int32 len, right, left, res;
				res = NkVorbisDecodePacket(f, &len, &left, &right);
				if (res)
					NkVorbisFinishFrame(f, len, left, right);
				return res;
			}

#ifndef NK_VORBIS_NO_PUSHDATA_API
			static int32 NkIsWholePacketPresent(NkVorbisDecoder *f) {
				// make sure that we have the packet available before continuing...
				// this requires a full ogg parse, but we know we can fetch from f->stream

				// instead of coding this out explicitly, we could save the current read state,
				// read the next packet with get8() until end-of-packet, check f->eof, then
				// reset the state? but that would be slower, esp. since we'd have over 256 bytes
				// of state to restore (primarily the page segment table)

				int32 s = f->nextSeg, first = true;
				uint8 *p = f->stream;

				if (s != -1) { // if we're not starting the packet with a 'continue on next page' flag
					for (; s < f->segmentCount; ++s) {
						p += f->segments[s];
						if (f->segments[s] < 255) // stop at first int16 segment
							break;
					}
					// either this continues, or it ends it...
					if (s == f->segmentCount)
						s = -1; // set 'crosses page' flag
					if (p > f->streamEnd)
						return error(f, NK_VORBIS_NEED_MORE_DATA);
					first = false;
				}
				for (; s == -1;) {
					uint8 *q;
					int32 n;

					// check that we have the page header ready
					if (p + 26 >= f->streamEnd)
						return error(f, NK_VORBIS_NEED_MORE_DATA);
					// validate the page
					if (memcmp(p, oggPageHeader, 4))
						return error(f, NK_VORBIS_INVALID_STREAM);
					if (p[4] != 0)
						return error(f, NK_VORBIS_INVALID_STREAM);
					if (first) { // the first segment must NOT have 'continuedPacket', later ones MUST
						if (f->previousLength)
							if ((p[5] & PAGEFLAG_continued_packet))
								return error(f, NK_VORBIS_INVALID_STREAM);
						// if no previous length, we're resynching, so we can come in on a continued-packet,
						// which we'll just drop
					} else {
						if (!(p[5] & PAGEFLAG_continued_packet))
							return error(f, NK_VORBIS_INVALID_STREAM);
					}
					n = p[26];	// segment counts
					q = p + 27; // q points to segment table
					p = q + n;	// advance past header
					// make sure we've read the segment table
					if (p > f->streamEnd)
						return error(f, NK_VORBIS_NEED_MORE_DATA);
					for (s = 0; s < n; ++s) {
						p += q[s];
						if (q[s] < 255)
							break;
					}
					if (s == n)
						s = -1; // set 'crosses page' flag
					if (p > f->streamEnd)
						return error(f, NK_VORBIS_NEED_MORE_DATA);
					first = false;
				}
				return true;
			}
#endif // !NK_VORBIS_NO_PUSHDATA_API

			static int32 NkStartDecoder(NkVorbisDecoder *f) {
				uint8 header[6], x, y;
				int32 len, i, j, k, maxSubmaps = 0;
				int32 longestFloorlist = 0;

				// first page, first packet
				f->firstDecode = true;

				if (!NkStartPage(f))
					return false;
				// validate page flag
				if (!(f->pageFlag & PAGEFLAG_first_page))
					return error(f, NK_VORBIS_INVALID_FIRST_PAGE);
				if (f->pageFlag & PAGEFLAG_last_page)
					return error(f, NK_VORBIS_INVALID_FIRST_PAGE);
				if (f->pageFlag & PAGEFLAG_continued_packet)
					return error(f, NK_VORBIS_INVALID_FIRST_PAGE);
				// check for expected packet length
				if (f->segmentCount != 1)
					return error(f, NK_VORBIS_INVALID_FIRST_PAGE);
				if (f->segments[0] != 30) {
					// check for the Ogg skeleton fishead identifying header to refine our error
					if (f->segments[0] == 64 && NkGetn(f, header, 6) && header[0] == 'f' && header[1] == 'i' &&
						header[2] == 's' && header[3] == 'h' && header[4] == 'e' && header[5] == 'a' &&
						get8(f) == 'd' && get8(f) == '\0')
						return error(f, NK_VORBIS_OGG_SKELETON_NOT_SUPPORTED);
					else
						return error(f, NK_VORBIS_INVALID_FIRST_PAGE);
				}

				// read packet
				// check packet header
				if (get8(f) != NK_VORBIS_PACKET_ID)
					return error(f, NK_VORBIS_INVALID_FIRST_PAGE);
				if (!NkGetn(f, header, 6))
					return error(f, NK_VORBIS_UNEXPECTED_EOF);
				if (!NkVorbisValidate(header))
					return error(f, NK_VORBIS_INVALID_FIRST_PAGE);
				// vorbisVersion
				if (get32(f) != 0)
					return error(f, NK_VORBIS_INVALID_FIRST_PAGE);
				f->channels = get8(f);
				if (!f->channels)
					return error(f, NK_VORBIS_INVALID_FIRST_PAGE);
				if (f->channels > NK_VORBIS_MAX_CHANNELS)
					return error(f, NK_VORBIS_TOO_MANY_CHANNELS);
				f->sampleRate = get32(f);
				if (!f->sampleRate)
					return error(f, NK_VORBIS_INVALID_FIRST_PAGE);
				get32(f); // bitrateMaximum
				get32(f); // bitrateNominal
				get32(f); // bitrateMinimum
				x = get8(f);
				{
					int32 log0, log1;
					log0 = x & 15;
					log1 = x >> 4;
					f->blocksize0 = 1 << log0;
					f->blocksize1 = 1 << log1;
					if (log0 < 6 || log0 > 13)
						return error(f, NK_VORBIS_INVALID_SETUP);
					if (log1 < 6 || log1 > 13)
						return error(f, NK_VORBIS_INVALID_SETUP);
					if (log0 > log1)
						return error(f, NK_VORBIS_INVALID_SETUP);
				}

				// framingFlag
				x = get8(f);
				if (!(x & 1))
					return error(f, NK_VORBIS_INVALID_FIRST_PAGE);

				// second packet!
				if (!NkStartPage(f))
					return false;

				if (!NkStartPacket(f))
					return false;

				if (!NkNextSegment(f))
					return false;

				if (NkGet8Packet(f) != NK_VORBIS_PACKET_COMMENT)
					return error(f, NK_VORBIS_INVALID_SETUP);
				for (i = 0; i < 6; ++i)
					header[i] = NkGet8Packet(f);
				if (!NkVorbisValidate(header))
					return error(f, NK_VORBIS_INVALID_SETUP);
				// file vendor
				len = NkGet32Packet(f);
				f->vendor = (char *)NkSetupMalloc(f, sizeof(char) * (len + 1));
				if (f->vendor == NULL)
					return error(f, NK_VORBIS_OUTOFMEM);
				for (i = 0; i < len; ++i) {
					f->vendor[i] = NkGet8Packet(f);
				}
				f->vendor[len] = (char)'\0';
				// user comments
				f->commentListLength = NkGet32Packet(f);
				f->commentList = NULL;
				if (f->commentListLength > 0) {
					f->commentList = (char **)NkSetupMalloc(f, sizeof(char *) * (f->commentListLength));
					if (f->commentList == NULL)
						return error(f, NK_VORBIS_OUTOFMEM);
				}

				for (i = 0; i < f->commentListLength; ++i) {
					len = NkGet32Packet(f);
					f->commentList[i] = (char *)NkSetupMalloc(f, sizeof(char) * (len + 1));
					if (f->commentList[i] == NULL)
						return error(f, NK_VORBIS_OUTOFMEM);

					for (j = 0; j < len; ++j) {
						f->commentList[i][j] = NkGet8Packet(f);
					}
					f->commentList[i][len] = (char)'\0';
				}

				// framingFlag
				x = NkGet8Packet(f);
				if (!(x & 1))
					return error(f, NK_VORBIS_INVALID_SETUP);

				NkSkip(f, f->bytesInSeg);
				f->bytesInSeg = 0;

				do {
					len = NkNextSegment(f);
					NkSkip(f, len);
					f->bytesInSeg = 0;
				} while (len);

				// third packet!
				if (!NkStartPacket(f))
					return false;

#ifndef NK_VORBIS_NO_PUSHDATA_API
				if (IS_PUSH_MODE(f)) {
					if (!NkIsWholePacketPresent(f)) {
						// convert error in ogg header to write type
						if (f->error == NK_VORBIS_INVALID_STREAM)
							f->error = NK_VORBIS_INVALID_SETUP;
						return false;
					}
				}
#endif

				NkCrc32Init(); // always init it, to avoid multithread race conditions

				if (NkGet8Packet(f) != NK_VORBIS_PACKET_SETUP)
					return error(f, NK_VORBIS_INVALID_SETUP);
				for (i = 0; i < 6; ++i)
					header[i] = NkGet8Packet(f);
				if (!NkVorbisValidate(header))
					return error(f, NK_VORBIS_INVALID_SETUP);

				// codebooks

				f->codebookCount = getBits(f, 8) + 1;
				f->codebooks = (NkCodebook *)NkSetupMalloc(f, sizeof(*f->codebooks) * f->codebookCount);
				if (f->codebooks == NULL)
					return error(f, NK_VORBIS_OUTOFMEM);
				memset(f->codebooks, 0, sizeof(*f->codebooks) * f->codebookCount);
				for (i = 0; i < f->codebookCount; ++i) {
					uint32 *values;
					int32 ordered, sortedCount;
					int32 total = 0;
					uint8 *lengths;
					NkCodebook *c = f->codebooks + i;
					CHECK(f);
					x = getBits(f, 8);
					if (x != 0x42)
						return error(f, NK_VORBIS_INVALID_SETUP);
					x = getBits(f, 8);
					if (x != 0x43)
						return error(f, NK_VORBIS_INVALID_SETUP);
					x = getBits(f, 8);
					if (x != 0x56)
						return error(f, NK_VORBIS_INVALID_SETUP);
					x = getBits(f, 8);
					c->dimensions = (getBits(f, 8) << 8) + x;
					x = getBits(f, 8);
					y = getBits(f, 8);
					c->entries = (getBits(f, 8) << 16) + (y << 8) + x;
					ordered = getBits(f, 1);
					c->sparse = ordered ? 0 : getBits(f, 1);

					if (c->dimensions == 0 && c->entries != 0)
						return error(f, NK_VORBIS_INVALID_SETUP);

					if (c->sparse)
						lengths = (uint8 *)NkSetupTempMalloc(f, c->entries);
					else
						lengths = c->codewordLengths = (uint8 *)NkSetupMalloc(f, c->entries);

					if (!lengths)
						return error(f, NK_VORBIS_OUTOFMEM);

					if (ordered) {
						int32 currentEntry = 0;
						int32 currentLength = getBits(f, 5) + 1;
						while (currentEntry < c->entries) {
							int32 limit = c->entries - currentEntry;
							int32 n = getBits(f, NkIlog(limit));
							if (currentLength >= 32)
								return error(f, NK_VORBIS_INVALID_SETUP);
							if (currentEntry + n > (int32)c->entries) {
								return error(f, NK_VORBIS_INVALID_SETUP);
							}
							memset(lengths + currentEntry, currentLength, n);
							currentEntry += n;
							++currentLength;
						}
					} else {
						for (j = 0; j < c->entries; ++j) {
							int32 present = c->sparse ? getBits(f, 1) : 1;
							if (present) {
								lengths[j] = getBits(f, 5) + 1;
								++total;
								if (lengths[j] == 32)
									return error(f, NK_VORBIS_INVALID_SETUP);
							} else {
								lengths[j] = NO_CODE;
							}
						}
					}

					if (c->sparse && total >= c->entries >> 2) {
						// convert sparse items to non-sparse!
						if (c->entries > (int32)f->setupTempMemoryRequired)
							f->setupTempMemoryRequired = c->entries;

						c->codewordLengths = (uint8 *)NkSetupMalloc(f, c->entries);
						if (c->codewordLengths == NULL)
							return error(f, NK_VORBIS_OUTOFMEM);
						memcpy(c->codewordLengths, lengths, c->entries);
						NkSetupTempFree(
							f, lengths,
							c->entries); // note this is only safe if there have been no intervening temp mallocs!
						lengths = c->codewordLengths;
						c->sparse = 0;
					}

					// compute the size of the sorted tables
					if (c->sparse) {
						sortedCount = total;
					} else {
						sortedCount = 0;
#ifndef NK_VORBIS_NO_HUFFMAN_BINARY_SEARCH
						for (j = 0; j < c->entries; ++j)
							if (lengths[j] > NK_VORBIS_FAST_HUFFMAN_LENGTH && lengths[j] != NO_CODE)
								++sortedCount;
#endif
					}

					c->sortedEntries = sortedCount;
					values = NULL;

					CHECK(f);
					if (!c->sparse) {
						c->codewords = (uint32 *)NkSetupMalloc(f, sizeof(c->codewords[0]) * c->entries);
						if (!c->codewords)
							return error(f, NK_VORBIS_OUTOFMEM);
					} else {
						uint32 size;
						if (c->sortedEntries) {
							c->codewordLengths = (uint8 *)NkSetupMalloc(f, c->sortedEntries);
							if (!c->codewordLengths)
								return error(f, NK_VORBIS_OUTOFMEM);
							c->codewords = (uint32 *)NkSetupTempMalloc(f, sizeof(*c->codewords) * c->sortedEntries);
							if (!c->codewords)
								return error(f, NK_VORBIS_OUTOFMEM);
							values = (uint32 *)NkSetupTempMalloc(f, sizeof(*values) * c->sortedEntries);
							if (!values)
								return error(f, NK_VORBIS_OUTOFMEM);
						}
						size = c->entries + (sizeof(*c->codewords) + sizeof(*values)) * c->sortedEntries;
						if (size > f->setupTempMemoryRequired)
							f->setupTempMemoryRequired = size;
					}

					if (!NkComputeCodewords(c, lengths, c->entries, values)) {
						if (c->sparse)
							NkSetupTempFree(f, values, 0);
						return error(f, NK_VORBIS_INVALID_SETUP);
					}

					if (c->sortedEntries) {
						// allocate an extra slot for sentinels
						c->sortedCodewords =
							(uint32 *)NkSetupMalloc(f, sizeof(*c->sortedCodewords) * (c->sortedEntries + 1));
						if (c->sortedCodewords == NULL)
							return error(f, NK_VORBIS_OUTOFMEM);
						// allocate an extra slot at the front so that c->sortedValues[-1] is defined
						// so that we can catch that case without an extra if
						c->sortedValues = (int32 *)NkSetupMalloc(f, sizeof(*c->sortedValues) * (c->sortedEntries + 1));
						if (c->sortedValues == NULL)
							return error(f, NK_VORBIS_OUTOFMEM);
						++c->sortedValues;
						c->sortedValues[-1] = -1;
						NkComputeSortedHuffman(c, lengths, values);
					}

					if (c->sparse) {
						NkSetupTempFree(f, values, sizeof(*values) * c->sortedEntries);
						NkSetupTempFree(f, c->codewords, sizeof(*c->codewords) * c->sortedEntries);
						NkSetupTempFree(f, lengths, c->entries);
						c->codewords = NULL;
					}

					NkComputeAcceleratedHuffman(c);

					CHECK(f);
					c->lookupType = getBits(f, 4);
					if (c->lookupType > 2)
						return error(f, NK_VORBIS_INVALID_SETUP);
					if (c->lookupType > 0) {
						uint16 *mults;
						c->minimumValue = NkFloat32Unpack(getBits(f, 32));
						c->deltaValue = NkFloat32Unpack(getBits(f, 32));
						c->valueBits = getBits(f, 4) + 1;
						c->sequenceP = getBits(f, 1);
						if (c->lookupType == 1) {
							int32 values = NkLookup1Values(c->entries, c->dimensions);
							if (values < 0)
								return error(f, NK_VORBIS_INVALID_SETUP);
							c->lookupValues = (uint32)values;
						} else {
							c->lookupValues = c->entries * c->dimensions;
						}
						if (c->lookupValues == 0)
							return error(f, NK_VORBIS_INVALID_SETUP);
						mults = (uint16 *)NkSetupTempMalloc(f, sizeof(mults[0]) * c->lookupValues);
						if (mults == NULL)
							return error(f, NK_VORBIS_OUTOFMEM);
						for (j = 0; j < (int32)c->lookupValues; ++j) {
							int32 q = getBits(f, c->valueBits);
							if (q == EOP) {
								NkSetupTempFree(f, mults, sizeof(mults[0]) * c->lookupValues);
								return error(f, NK_VORBIS_INVALID_SETUP);
							}
							mults[j] = q;
						}

#ifndef NK_VORBIS_DIVIDES_IN_CODEBOOK
						if (c->lookupType == 1) {
							int32 len, sparse = c->sparse;
							float32 last = 0;
							// pre-expand the lookup1-style multiplicands, to avoid a divide in the inner loop
							if (sparse) {
								if (c->sortedEntries == 0)
									goto NkSkip;
								c->multiplicands = (codetype *)NkSetupMalloc(f, sizeof(c->multiplicands[0]) *
																					c->sortedEntries * c->dimensions);
							} else
								c->multiplicands = (codetype *)NkSetupMalloc(f, sizeof(c->multiplicands[0]) *
																					c->entries * c->dimensions);
							if (c->multiplicands == NULL) {
								NkSetupTempFree(f, mults, sizeof(mults[0]) * c->lookupValues);
								return error(f, NK_VORBIS_OUTOFMEM);
							}
							len = sparse ? c->sortedEntries : c->entries;
							for (j = 0; j < len; ++j) {
								uint32 z = sparse ? c->sortedValues[j] : j;
								uint32 div = 1;
								for (k = 0; k < c->dimensions; ++k) {
									int32 off = (z / div) % c->lookupValues;
									float32 val = mults[off] * c->deltaValue + c->minimumValue + last;
									c->multiplicands[j * c->dimensions + k] = val;
									if (c->sequenceP)
										last = val;
									if (k + 1 < c->dimensions) {
										if (div > UINT_MAX / (uint32)c->lookupValues) {
											NkSetupTempFree(f, mults, sizeof(mults[0]) * c->lookupValues);
											return error(f, NK_VORBIS_INVALID_SETUP);
										}
										div *= c->lookupValues;
									}
								}
							}
							c->lookupType = 2;
						} else
#endif
						{
							float32 last = 0;
							CHECK(f);
							c->multiplicands =
								(codetype *)NkSetupMalloc(f, sizeof(c->multiplicands[0]) * c->lookupValues);
							if (c->multiplicands == NULL) {
								NkSetupTempFree(f, mults, sizeof(mults[0]) * c->lookupValues);
								return error(f, NK_VORBIS_OUTOFMEM);
							}
							for (j = 0; j < (int32)c->lookupValues; ++j) {
								float32 val = mults[j] * c->deltaValue + c->minimumValue + last;
								c->multiplicands[j] = val;
								if (c->sequenceP)
									last = val;
							}
						}
#ifndef NK_VORBIS_DIVIDES_IN_CODEBOOK
					NkSkip:;
#endif
						NkSetupTempFree(f, mults, sizeof(mults[0]) * c->lookupValues);

						CHECK(f);
					}
					CHECK(f);
				}

				// time domain transfers (notused)

				x = getBits(f, 6) + 1;
				for (i = 0; i < x; ++i) {
					uint32 z = getBits(f, 16);
					if (z != 0)
						return error(f, NK_VORBIS_INVALID_SETUP);
				}

				// Floors
				f->floorCount = getBits(f, 6) + 1;
				f->floorConfig = (NkFloor *)NkSetupMalloc(f, f->floorCount * sizeof(*f->floorConfig));
				if (f->floorConfig == NULL)
					return error(f, NK_VORBIS_OUTOFMEM);
				for (i = 0; i < f->floorCount; ++i) {
					f->floorTypes[i] = getBits(f, 16);
					if (f->floorTypes[i] > 1)
						return error(f, NK_VORBIS_INVALID_SETUP);
					if (f->floorTypes[i] == 0) {
						Floor0 *g = &f->floorConfig[i].floor0;
						g->order = getBits(f, 8);
						g->rate = getBits(f, 16);
						g->barkMapSize = getBits(f, 16);
						g->amplitudeBits = getBits(f, 6);
						g->amplitudeOffset = getBits(f, 8);
						g->numberOfBooks = getBits(f, 4) + 1;
						for (j = 0; j < g->numberOfBooks; ++j)
							g->bookList[j] = getBits(f, 8);
						return error(f, NK_VORBIS_FEATURE_NOT_SUPPORTED);
					} else {
						NkVorbisFloorOrdering p[31 * 8 + 2];
						NkFloor1 *g = &f->floorConfig[i].floor1;
						int32 maxClass = -1;
						g->partitions = getBits(f, 5);
						for (j = 0; j < g->partitions; ++j) {
							g->partitionClassList[j] = getBits(f, 4);
							if (g->partitionClassList[j] > maxClass)
								maxClass = g->partitionClassList[j];
						}
						for (j = 0; j <= maxClass; ++j) {
							g->classDimensions[j] = getBits(f, 3) + 1;
							g->classSubclasses[j] = getBits(f, 2);
							if (g->classSubclasses[j]) {
								g->classMasterbooks[j] = getBits(f, 8);
								if (g->classMasterbooks[j] >= f->codebookCount)
									return error(f, NK_VORBIS_INVALID_SETUP);
							}
							for (k = 0; k < 1 << g->classSubclasses[j]; ++k) {
								g->subclassBooks[j][k] = (int16)getBits(f, 8) - 1;
								if (g->subclassBooks[j][k] >= f->codebookCount)
									return error(f, NK_VORBIS_INVALID_SETUP);
							}
						}
						g->floor1Multiplier = getBits(f, 2) + 1;
						g->rangebits = getBits(f, 4);
						g->Xlist[0] = 0;
						g->Xlist[1] = 1 << g->rangebits;
						g->values = 2;
						for (j = 0; j < g->partitions; ++j) {
							int32 c = g->partitionClassList[j];
							for (k = 0; k < g->classDimensions[c]; ++k) {
								g->Xlist[g->values] = getBits(f, g->rangebits);
								++g->values;
							}
						}
						// precompute the sorting
						for (j = 0; j < g->values; ++j) {
							p[j].x = g->Xlist[j];
							p[j].id = j;
						}
						qsort(p, g->values, sizeof(p[0]), NkPointCompare);
						for (j = 0; j < g->values - 1; ++j)
							if (p[j].x == p[j + 1].x)
								return error(f, NK_VORBIS_INVALID_SETUP);
						for (j = 0; j < g->values; ++j)
							g->sortedOrder[j] = (uint8)p[j].id;
						// precompute the NkNeighbors
						for (j = 2; j < g->values; ++j) {
							int32 low = 0, hi = 0;
							NkNeighbors(g->Xlist, j, &low, &hi);
							g->NkNeighbors[j][0] = low;
							g->NkNeighbors[j][1] = hi;
						}

						if (g->values > longestFloorlist)
							longestFloorlist = g->values;
					}
				}

				// NkResidue
				f->residueCount = getBits(f, 6) + 1;
				f->residueConfig = (NkResidue *)NkSetupMalloc(f, f->residueCount * sizeof(f->residueConfig[0]));
				if (f->residueConfig == NULL)
					return error(f, NK_VORBIS_OUTOFMEM);
				memset(f->residueConfig, 0, f->residueCount * sizeof(f->residueConfig[0]));
				for (i = 0; i < f->residueCount; ++i) {
					uint8 residueCascade[64];
					NkResidue *r = f->residueConfig + i;
					f->residueTypes[i] = getBits(f, 16);
					if (f->residueTypes[i] > 2)
						return error(f, NK_VORBIS_INVALID_SETUP);
					r->begin = getBits(f, 24);
					r->end = getBits(f, 24);
					if (r->end < r->begin)
						return error(f, NK_VORBIS_INVALID_SETUP);
					r->partSize = getBits(f, 24) + 1;
					r->classifications = getBits(f, 6) + 1;
					r->classbook = getBits(f, 8);
					if (r->classbook >= f->codebookCount)
						return error(f, NK_VORBIS_INVALID_SETUP);
					for (j = 0; j < r->classifications; ++j) {
						uint8 highBits = 0;
						uint8 lowBits = getBits(f, 3);
						if (getBits(f, 1))
							highBits = getBits(f, 5);
						residueCascade[j] = highBits * 8 + lowBits;
					}
					r->residueBooks = (int16(*)[8])NkSetupMalloc(f, sizeof(r->residueBooks[0]) * r->classifications);
					if (r->residueBooks == NULL)
						return error(f, NK_VORBIS_OUTOFMEM);
					for (j = 0; j < r->classifications; ++j) {
						for (k = 0; k < 8; ++k) {
							if (residueCascade[j] & (1 << k)) {
								r->residueBooks[j][k] = getBits(f, 8);
								if (r->residueBooks[j][k] >= f->codebookCount)
									return error(f, NK_VORBIS_INVALID_SETUP);
							} else {
								r->residueBooks[j][k] = -1;
							}
						}
					}
					// precompute the classifications[] array to avoid inner-loop mod/divide
					// call it 'classdata' since we already have r->classifications
					r->classdata =
						(uint8 **)NkSetupMalloc(f, sizeof(*r->classdata) * f->codebooks[r->classbook].entries);
					if (!r->classdata)
						return error(f, NK_VORBIS_OUTOFMEM);
					memset(r->classdata, 0, sizeof(*r->classdata) * f->codebooks[r->classbook].entries);
					for (j = 0; j < f->codebooks[r->classbook].entries; ++j) {
						int32 classwords = f->codebooks[r->classbook].dimensions;
						int32 temp = j;
						r->classdata[j] = (uint8 *)NkSetupMalloc(f, sizeof(r->classdata[j][0]) * classwords);
						if (r->classdata[j] == NULL)
							return error(f, NK_VORBIS_OUTOFMEM);
						for (k = classwords - 1; k >= 0; --k) {
							r->classdata[j][k] = temp % r->classifications;
							temp /= r->classifications;
						}
					}
				}

				f->mappingCount = getBits(f, 6) + 1;
				f->mapping = (NkMapping *)NkSetupMalloc(f, f->mappingCount * sizeof(*f->mapping));
				if (f->mapping == NULL)
					return error(f, NK_VORBIS_OUTOFMEM);
				memset(f->mapping, 0, f->mappingCount * sizeof(*f->mapping));
				for (i = 0; i < f->mappingCount; ++i) {
					NkMapping *m = f->mapping + i;
					int32 mappingType = getBits(f, 16);
					if (mappingType != 0)
						return error(f, NK_VORBIS_INVALID_SETUP);
					m->chan = (NkMappingChannel *)NkSetupMalloc(f, f->channels * sizeof(*m->chan));
					if (m->chan == NULL)
						return error(f, NK_VORBIS_OUTOFMEM);
					if (getBits(f, 1))
						m->submaps = getBits(f, 4) + 1;
					else
						m->submaps = 1;
					if (m->submaps > maxSubmaps)
						maxSubmaps = m->submaps;
					if (getBits(f, 1)) {
						m->couplingSteps = getBits(f, 8) + 1;
						if (m->couplingSteps > f->channels)
							return error(f, NK_VORBIS_INVALID_SETUP);
						for (k = 0; k < m->couplingSteps; ++k) {
							m->chan[k].magnitude = getBits(f, NkIlog(f->channels - 1));
							m->chan[k].angle = getBits(f, NkIlog(f->channels - 1));
							if (m->chan[k].magnitude >= f->channels)
								return error(f, NK_VORBIS_INVALID_SETUP);
							if (m->chan[k].angle >= f->channels)
								return error(f, NK_VORBIS_INVALID_SETUP);
							if (m->chan[k].magnitude == m->chan[k].angle)
								return error(f, NK_VORBIS_INVALID_SETUP);
						}
					} else
						m->couplingSteps = 0;

					// reserved field
					if (getBits(f, 2))
						return error(f, NK_VORBIS_INVALID_SETUP);
					if (m->submaps > 1) {
						for (j = 0; j < f->channels; ++j) {
							m->chan[j].mux = getBits(f, 4);
							if (m->chan[j].mux >= m->submaps)
								return error(f, NK_VORBIS_INVALID_SETUP);
						}
					} else
						// @SPECIFICATION: this case is missing from the spec
						for (j = 0; j < f->channels; ++j)
							m->chan[j].mux = 0;

					for (j = 0; j < m->submaps; ++j) {
						getBits(f, 8); // discard
						m->submapFloor[j] = getBits(f, 8);
						m->submapResidue[j] = getBits(f, 8);
						if (m->submapFloor[j] >= f->floorCount)
							return error(f, NK_VORBIS_INVALID_SETUP);
						if (m->submapResidue[j] >= f->residueCount)
							return error(f, NK_VORBIS_INVALID_SETUP);
					}
				}

				// Modes
				f->modeCount = getBits(f, 6) + 1;
				for (i = 0; i < f->modeCount; ++i) {
					NkMode *m = f->modeConfig + i;
					m->blockflag = getBits(f, 1);
					m->windowtype = getBits(f, 16);
					m->transformtype = getBits(f, 16);
					m->mapping = getBits(f, 8);
					if (m->windowtype != 0)
						return error(f, NK_VORBIS_INVALID_SETUP);
					if (m->transformtype != 0)
						return error(f, NK_VORBIS_INVALID_SETUP);
					if (m->mapping >= f->mappingCount)
						return error(f, NK_VORBIS_INVALID_SETUP);
				}

				NkFlushPacket(f);

				f->previousLength = 0;

				for (i = 0; i < f->channels; ++i) {
					f->channelBuffers[i] = (float32 *)NkSetupMalloc(f, sizeof(float32) * f->blocksize1);
					f->previousWindow[i] = (float32 *)NkSetupMalloc(f, sizeof(float32) * f->blocksize1 / 2);
					f->finalY[i] = (int16 *)NkSetupMalloc(f, sizeof(int16) * longestFloorlist);
					if (f->channelBuffers[i] == NULL || f->previousWindow[i] == NULL || f->finalY[i] == NULL)
						return error(f, NK_VORBIS_OUTOFMEM);
					memset(f->channelBuffers[i], 0, sizeof(float32) * f->blocksize1);
#ifdef NK_VORBIS_NO_DEFER_FLOOR
					f->floorBuffers[i] = (float32 *)NkSetupMalloc(f, sizeof(float32) * f->blocksize1 / 2);
					if (f->floorBuffers[i] == NULL)
						return error(f, NK_VORBIS_OUTOFMEM);
#endif
				}

				if (!NkInitBlocksize(f, 0, f->blocksize0))
					return false;
				if (!NkInitBlocksize(f, 1, f->blocksize1))
					return false;
				f->blocksize[0] = f->blocksize0;
				f->blocksize[1] = f->blocksize1;

#ifdef NK_VORBIS_DIVIDE_TABLE
				if (integerDivideTable[1][1] == 0)
					for (i = 0; i < DIVTAB_NUMER; ++i)
						for (j = 1; j < DIVTAB_DENOM; ++j)
							integerDivideTable[i][j] = i / j;
#endif

				// compute how much temporary memory is needed

				// 1.
				{
					uint32 imdctMem = (f->blocksize1 * sizeof(float32) >> 1);
					uint32 classifyMem;
					int32 i, maxPartRead = 0;
					for (i = 0; i < f->residueCount; ++i) {
						NkResidue *r = f->residueConfig + i;
						uint32 actualSize = f->blocksize1 / 2;
						uint32 limitRBegin = r->begin < actualSize ? r->begin : actualSize;
						uint32 limitREnd = r->end < actualSize ? r->end : actualSize;
						int32 nRead = limitREnd - limitRBegin;
						int32 partRead = nRead / r->partSize;
						if (partRead > maxPartRead)
							maxPartRead = partRead;
					}
#ifndef NK_VORBIS_DIVIDES_IN_RESIDUE
					classifyMem = f->channels * (sizeof(void *) + maxPartRead * sizeof(uint8 *));
#else
					classifyMem = f->channels * (sizeof(void *) + maxPartRead * sizeof(int32 * b));
#endif

					// maximum reasonable partition size is f->blocksize1

					f->tempMemoryRequired = classifyMem;
					if (imdctMem > f->tempMemoryRequired)
						f->tempMemoryRequired = imdctMem;
				}

				if (f->alloc.allocBuffer) {
					assert(f->tempOffset == f->alloc.allocBufferLengthInBytes);
					// check if there's enough temp memory so we don't error later
					if (f->setupOffset + sizeof(*f) + f->tempMemoryRequired > (uint32)f->tempOffset)
						return error(f, NK_VORBIS_OUTOFMEM);
				}

				// @TODO: NkVorbisSeekStart expects firstAudioPageOffset to point to a page
				// without PAGEFLAG_continued_packet, so this either points to the first page, or
				// the page after the end of the headers. It might be cleaner to point to a page
				// in the middle of the headers, when that's the page where the first audio packet
				// starts, but we'd have to also correctly NkSkip the end of any continued packet in
				// NkVorbisSeekStart.
				if (f->nextSeg == -1) {
					f->firstAudioPageOffset = NkVorbisGetFileOffset(f);
				} else {
					f->firstAudioPageOffset = 0;
				}

				return true;
			}

			static void NkVorbisDeinit(NkVorbisDecoder *p) {
				int32 i, j;

				NkSetupFree(p, p->vendor);
				for (i = 0; i < p->commentListLength; ++i) {
					NkSetupFree(p, p->commentList[i]);
				}
				NkSetupFree(p, p->commentList);

				if (p->residueConfig) {
					for (i = 0; i < p->residueCount; ++i) {
						NkResidue *r = p->residueConfig + i;
						if (r->classdata) {
							for (j = 0; j < p->codebooks[r->classbook].entries; ++j)
								NkSetupFree(p, r->classdata[j]);
							NkSetupFree(p, r->classdata);
						}
						NkSetupFree(p, r->residueBooks);
					}
				}

				if (p->codebooks) {
					CHECK(p);
					for (i = 0; i < p->codebookCount; ++i) {
						NkCodebook *c = p->codebooks + i;
						NkSetupFree(p, c->codewordLengths);
						NkSetupFree(p, c->multiplicands);
						NkSetupFree(p, c->codewords);
						NkSetupFree(p, c->sortedCodewords);
						// c->sortedValues[-1] is the first entry in the array
						NkSetupFree(p, c->sortedValues ? c->sortedValues - 1 : NULL);
					}
					NkSetupFree(p, p->codebooks);
				}
				NkSetupFree(p, p->floorConfig);
				NkSetupFree(p, p->residueConfig);
				if (p->mapping) {
					for (i = 0; i < p->mappingCount; ++i)
						NkSetupFree(p, p->mapping[i].chan);
					NkSetupFree(p, p->mapping);
				}
				CHECK(p);
				for (i = 0; i < p->channels && i < NK_VORBIS_MAX_CHANNELS; ++i) {
					NkSetupFree(p, p->channelBuffers[i]);
					NkSetupFree(p, p->previousWindow[i]);
#ifdef NK_VORBIS_NO_DEFER_FLOOR
					NkSetupFree(p, p->floorBuffers[i]);
#endif
					NkSetupFree(p, p->finalY[i]);
				}
				for (i = 0; i < 2; ++i) {
					NkSetupFree(p, p->A[i]);
					NkSetupFree(p, p->B[i]);
					NkSetupFree(p, p->C[i]);
					NkSetupFree(p, p->window[i]);
					NkSetupFree(p, p->NkBitReverse[i]);
				}
#ifndef NK_VORBIS_NO_STDIO
				if (p->closeOnFree)
					fclose(p->f);
#endif
			}

			void NkVorbisClose(NkVorbisDecoder *p) {
				if (p == NULL)
					return;
				NkVorbisDeinit(p);
				NkSetupFree(p, p);
			}

			static void NkVorbisInit(NkVorbisDecoder *p, const NkVorbisAllocator *z) {
				memset(p, 0, sizeof(*p)); // NULL out all malloc'd pointers to start
				if (z) {
					p->alloc = *z;
					p->alloc.allocBufferLengthInBytes &= ~7;
					p->tempOffset = p->alloc.allocBufferLengthInBytes;
				}
				p->eof = 0;
				p->error = NK_VORBIS__NO_ERROR;
				p->stream = NULL;
				p->codebooks = NULL;
				p->pageCrcTests = -1;
#ifndef NK_VORBIS_NO_STDIO
				p->closeOnFree = false;
				p->f = NULL;
#endif
			}

			int32 NkVorbisGetSampleOffset(NkVorbisDecoder *f) {
				if (f->currentLocValid)
					return f->currentLoc;
				else
					return -1;
			}

			NkVorbisInfo NkVorbisGetInfo(NkVorbisDecoder *f) {
				NkVorbisInfo d;
				d.channels = f->channels;
				d.sampleRate = f->sampleRate;
				d.setupMemoryRequired = f->setupMemoryRequired;
				d.setupTempMemoryRequired = f->setupTempMemoryRequired;
				d.tempMemoryRequired = f->tempMemoryRequired;
				d.maxFrameSize = f->blocksize1 >> 1;
				return d;
			}

			NkVorbisComment NkVorbisGetComment(NkVorbisDecoder *f) {
				NkVorbisComment d;
				d.vendor = f->vendor;
				d.commentListLength = f->commentListLength;
				d.commentList = f->commentList;
				return d;
			}

			int32 NkVorbisGetError(NkVorbisDecoder *f) {
				int32 e = f->error;
				f->error = NK_VORBIS__NO_ERROR;
				return e;
			}

			static NkVorbisDecoder *NkVorbisAlloc(NkVorbisDecoder *f) {
				NkVorbisDecoder *p = (NkVorbisDecoder *)NkSetupMalloc(f, sizeof(*p));
				return p;
			}

#ifndef NK_VORBIS_NO_PUSHDATA_API

			void NkVorbisFlushPushdata(NkVorbisDecoder *f) {
				f->previousLength = 0;
				f->pageCrcTests = 0;
				f->discardSamplesDeferred = 0;
				f->currentLocValid = false;
				f->firstDecode = false;
				f->samplesOutput = 0;
				f->channelBufferStart = 0;
				f->channelBufferEnd = 0;
			}

			static int32 NkVorbisSearchForPagePushdata(NkVorbisDecoder *f, uint8 *data, int32 dataLen) {
				int32 i, n;
				for (i = 0; i < f->pageCrcTests; ++i)
					f->scan[i].bytesDone = 0;

				// if we have room for more scans, search for them first, because
				// they may cause us to stop early if their header is incomplete
				if (f->pageCrcTests < NK_VORBIS_PUSHDATA_CRC_COUNT) {
					if (dataLen < 4)
						return 0;
					dataLen -= 3; // need to look for 4-byte sequence, so don't miss
								  // one that straddles a boundary
					for (i = 0; i < dataLen; ++i) {
						if (data[i] == 0x4f) {
							if (0 == memcmp(data + i, oggPageHeader, 4)) {
								int32 j, len;
								uint32 crc;
								// make sure we have the whole page header
								if (i + 26 >= dataLen || i + 27 + data[i + 26] >= dataLen) {
									// only read up to this page start, so hopefully we'll
									// have the whole page header start next time
									dataLen = i;
									break;
								}
								// ok, we have it all; compute the length of the page
								len = 27 + data[i + 26];
								for (j = 0; j < data[i + 26]; ++j)
									len += data[i + 27 + j];
								// scan everything up to the embedded crc (which we must 0)
								crc = 0;
								for (j = 0; j < 22; ++j)
									crc = crc32Update(crc, data[i + j]);
								// now process 4 0-bytes
								for (; j < 26; ++j)
									crc = crc32Update(crc, 0);
								// len is the total number of bytes we need to scan
								n = f->pageCrcTests++;
								f->scan[n].bytesLeft = len - j;
								f->scan[n].crcSoFar = crc;
								f->scan[n].goalCrc =
									data[i + 22] + (data[i + 23] << 8) + (data[i + 24] << 16) + (data[i + 25] << 24);
								// if the last frame on a page is continued to the next, then
								// we can't recover the sampleLoc immediately
								if (data[i + 27 + data[i + 26] - 1] == 255)
									f->scan[n].sampleLoc = ~0;
								else
									f->scan[n].sampleLoc =
										data[i + 6] + (data[i + 7] << 8) + (data[i + 8] << 16) + (data[i + 9] << 24);
								f->scan[n].bytesDone = i + j;
								if (f->pageCrcTests == NK_VORBIS_PUSHDATA_CRC_COUNT)
									break;
								// keep going if we still have room for more
							}
						}
					}
				}

				for (i = 0; i < f->pageCrcTests;) {
					uint32 crc;
					int32 j;
					int32 n = f->scan[i].bytesDone;
					int32 m = f->scan[i].bytesLeft;
					if (m > dataLen - n)
						m = dataLen - n;
					// m is the bytes to scan in the current chunk
					crc = f->scan[i].crcSoFar;
					for (j = 0; j < m; ++j)
						crc = crc32Update(crc, data[n + j]);
					f->scan[i].bytesLeft -= m;
					f->scan[i].crcSoFar = crc;
					if (f->scan[i].bytesLeft == 0) {
						// does it match?
						if (f->scan[i].crcSoFar == f->scan[i].goalCrc) {
							// Houston, we have page
							dataLen = n + m;					  // consumption amount is wherever that scan ended
							f->pageCrcTests = -1;				  // drop out of page scan mode
							f->previousLength = 0;				  // decode-but-don't-output one frame
							f->nextSeg = -1;					  // start a new page
							f->currentLoc = f->scan[i].sampleLoc; // set the current sample location
							// to the amount we'd have decoded had we decoded this page
							f->currentLocValid = f->currentLoc != ~0U;
							return dataLen;
						}
						// delete entry
						f->scan[i] = f->scan[--f->pageCrcTests];
					} else {
						++i;
					}
				}

				return dataLen;
			}

			// return value: number of bytes we used
			int32 NkVorbisDecodeFramePushdata(NkVorbisDecoder *f,				// the file we're decoding
											  const uint8 *data, int32 dataLen, // the memory available for decoding
											  int32 *channels,	 // place to write number of float32 * buffers
											  float32 ***output, // place to write float32 ** array of float32 * buffers
											  int32 *samples	 // place to write number of output samples
			) {
				int32 i;
				int32 len, right, left;

				if (!IS_PUSH_MODE(f))
					return error(f, NK_VORBIS_INVALID_API_MIXING);

				if (f->pageCrcTests >= 0) {
					*samples = 0;
					return NkVorbisSearchForPagePushdata(f, (uint8 *)data, dataLen);
				}

				f->stream = (uint8 *)data;
				f->streamEnd = (uint8 *)data + dataLen;
				f->error = NK_VORBIS__NO_ERROR;

				// check that we have the entire packet in memory
				if (!NkIsWholePacketPresent(f)) {
					*samples = 0;
					return 0;
				}

				if (!NkVorbisDecodePacket(f, &len, &left, &right)) {
					// save the actual error we encountered
					enum NkVorbisError error = f->error;
					if (error == NK_VORBIS_BAD_PACKET_TYPE) {
						// flush and resynch
						f->error = NK_VORBIS__NO_ERROR;
						while (NkGet8Packet(f) != EOP)
							if (f->eof)
								break;
						*samples = 0;
						return (int32)(f->stream - data);
					}
					if (error == NK_VORBIS_CONTINUED_PACKET_FLAG_INVALID) {
						if (f->previousLength == 0) {
							// we may be resynching, in which case it's ok to hit one
							// of these; just discard the packet
							f->error = NK_VORBIS__NO_ERROR;
							while (NkGet8Packet(f) != EOP)
								if (f->eof)
									break;
							*samples = 0;
							return (int32)(f->stream - data);
						}
					}
					// if we get an error while parsing, what to do?
					// well, it DEFINITELY won't work to continue from where we are!
					NkVorbisFlushPushdata(f);
					// restore the error that actually made us bail
					f->error = error;
					*samples = 0;
					return 1;
				}

				// success!
				len = NkVorbisFinishFrame(f, len, left, right);
				for (i = 0; i < f->channels; ++i)
					f->outputs[i] = f->channelBuffers[i] + left;

				if (channels)
					*channels = f->channels;
				*samples = len;
				*output = f->outputs;
				return (int32)(f->stream - data);
			}

			NkVorbisDecoder *NkVorbisOpenPushdata(const uint8 *data, int32 dataLen, // the memory available for decoding
												  int32 *dataUsed, // only defined if result is not NULL
												  int32 *error, const NkVorbisAllocator *alloc) {
				NkVorbisDecoder *f, p;
				NkVorbisInit(&p, alloc);
				p.stream = (uint8 *)data;
				p.streamEnd = (uint8 *)data + dataLen;
				p.pushMode = true;
				if (!NkStartDecoder(&p)) {
					if (p.eof)
						*error = NK_VORBIS_NEED_MORE_DATA;
					else
						*error = p.error;
					NkVorbisDeinit(&p);
					return NULL;
				}
				f = NkVorbisAlloc(&p);
				if (f) {
					*f = p;
					*dataUsed = (int32)(f->stream - data);
					*error = 0;
					return f;
				} else {
					NkVorbisDeinit(&p);
					return NULL;
				}
			}
#endif // NK_VORBIS_NO_PUSHDATA_API

			uint32 NkVorbisGetFileOffset(NkVorbisDecoder *f) {
#ifndef NK_VORBIS_NO_PUSHDATA_API
				if (f->pushMode)
					return 0;
#endif
				if (USE_MEMORY(f))
					return (uint32)(f->stream - f->streamStart);
#ifndef NK_VORBIS_NO_STDIO
				return (uint32)(ftell(f->f) - f->fStart);
#endif
			}

#ifndef NK_VORBIS_NO_PULLDATA_API
			//
			// DATA-PULLING API
			//

			static uint32 vorbisFindPage(NkVorbisDecoder *f, uint32 *end, uint32 *last) {
				for (;;) {
					int32 n;
					if (f->eof)
						return 0;
					n = get8(f);
					if (n == 0x4f) { // page header candidate
						uint32 retryLoc = NkVorbisGetFileOffset(f);
						int32 i;
						// check if we're off the end of a fileSection stream
						if (retryLoc - 25 > f->streamLen)
							return 0;
						// check the rest of the header
						for (i = 1; i < 4; ++i)
							if (get8(f) != oggPageHeader[i])
								break;
						if (f->eof)
							return 0;
						if (i == 4) {
							uint8 header[27];
							uint32 i, crc, goal, len;
							for (i = 0; i < 4; ++i)
								header[i] = oggPageHeader[i];
							for (; i < 27; ++i)
								header[i] = get8(f);
							if (f->eof)
								return 0;
							if (header[4] != 0)
								goto invalid;
							goal = header[22] + (header[23] << 8) + (header[24] << 16) + ((uint32)header[25] << 24);
							for (i = 22; i < 26; ++i)
								header[i] = 0;
							crc = 0;
							for (i = 0; i < 27; ++i)
								crc = crc32Update(crc, header[i]);
							len = 0;
							for (i = 0; i < header[26]; ++i) {
								int32 s = get8(f);
								crc = crc32Update(crc, s);
								len += s;
							}
							if (len && f->eof)
								return 0;
							for (i = 0; i < len; ++i)
								crc = crc32Update(crc, get8(f));
							// finished parsing probable page
							if (crc == goal) {
								// we could now check that it's either got the last
								// page flag set, OR it's followed by the capture
								// pattern, but I guess TECHNICALLY you could have
								// a file with garbage between each ogg page and recover
								// from it automatically? So even though that paranoia
								// might decrease the chance of an invalid decode by
								// another 2^32, not worth it since it would hose those
								// invalid-but-useful files?
								if (end)
									*end = NkVorbisGetFileOffset(f);
								if (last) {
									if (header[5] & 0x04)
										*last = 1;
									else
										*last = 0;
								}
								NkSetFileOffset(f, retryLoc - 1);
								return 1;
							}
						}
					invalid:
						// not a valid page, so rewind and look for next one
						NkSetFileOffset(f, retryLoc);
					}
				}
			}

#define SAMPLE_unknown 0xffffffff

			// seeking is implemented with a binary search, which narrows down the range to
			// 64K, before using a linear search (because finding the synchronization
			// pattern can be expensive, and the chance we'd find the end page again is
			// relatively high for small ranges)
			//
			// two initial interpolation-style probes are used at the start of the search
			// to try to bound either side of the binary search sensibly, while still
			// working in O(log n) time if they fail.

			static int32 NkGetSeekPageInfo(NkVorbisDecoder *f, NkProbedPage *z) {
				uint8 header[27], lacing[255];
				int32 i, len;

				// record where the page starts
				z->pageStart = NkVorbisGetFileOffset(f);

				// parse the header
				NkGetn(f, header, 27);
				if (header[0] != 'O' || header[1] != 'g' || header[2] != 'g' || header[3] != 'S')
					return 0;
				NkGetn(f, lacing, header[26]);

				// determine the length of the payload
				len = 0;
				for (i = 0; i < header[26]; ++i)
					len += lacing[i];

				// this implies where the page ends
				z->pageEnd = z->pageStart + 27 + header[26] + len;

				// read the last-decoded sample out of the data
				z->lastDecodedSample = header[6] + (header[7] << 8) + (header[8] << 16) + (header[9] << 24);

				// restore file state to where we were
				NkSetFileOffset(f, z->pageStart);
				return 1;
			}

			// rarely used function to seek back to the preceding page while finding the
			// start of a packet
			static int32 NkGoToPageBefore(NkVorbisDecoder *f, uint32 limitOffset) {
				uint32 previousSafe, end;

				// now we want to seek back 64K from the limit
				if (limitOffset >= 65536 && limitOffset - 65536 >= f->firstAudioPageOffset)
					previousSafe = limitOffset - 65536;
				else
					previousSafe = f->firstAudioPageOffset;

				NkSetFileOffset(f, previousSafe);

				while (vorbisFindPage(f, &end, NULL)) {
					if (end >= limitOffset && NkVorbisGetFileOffset(f) < limitOffset)
						return 1;
					NkSetFileOffset(f, end);
				}

				return 0;
			}

			// implements the search logic for finding a page and starting decoding. if
			// the function succeeds, currentLocValid will be true and currentLoc will
			// be less than or equal to the provided sample number (the closer the
			// better).
			static int32 NkSeekToSampleCoarse(NkVorbisDecoder *f, uint32 sampleNumber) {
				NkProbedPage left, right, mid;
				int32 i, startSegWithKnownLoc, endPos, pageStart;
				uint32 delta, streamLength, padding, lastSampleLimit;
				float64 offset = 0.0, bytesPerSample = 0.0;
				int32 probe = 0;

				// find the last page and validate the target sample
				streamLength = NkVorbisStreamLengthInSamples(f);
				if (streamLength == 0)
					return error(f, NK_VORBIS_SEEK_WITHOUT_LENGTH);
				if (sampleNumber > streamLength)
					return error(f, NK_VORBIS_SEEK_INVALID);

				// this is the maximum difference between the window-center (which is the
				// actual granule position value), and the right-start (which the spec
				// indicates should be the granule position (give or take one)).
				padding = ((f->blocksize1 - f->blocksize0) >> 2);
				if (sampleNumber < padding)
					lastSampleLimit = 0;
				else
					lastSampleLimit = sampleNumber - padding;

				left = f->pFirst;
				while (left.lastDecodedSample == ~0U) {
					// (untested) the first page does not have a 'lastDecodedSample'
					NkSetFileOffset(f, left.pageEnd);
					if (!NkGetSeekPageInfo(f, &left))
						goto error;
				}

				right = f->pLast;
				assert(right.lastDecodedSample != ~0U);

				// starting from the start is handled differently
				if (lastSampleLimit <= left.lastDecodedSample) {
					if (NkVorbisSeekStart(f)) {
						if (f->currentLoc > sampleNumber)
							return error(f, NK_VORBIS_SEEK_FAILED);
						return 1;
					}
					return 0;
				}

				while (left.pageEnd != right.pageStart) {
					assert(left.pageEnd < right.pageStart);
					// search range in bytes
					delta = right.pageStart - left.pageEnd;
					if (delta <= 65536) {
						// there's only 64K left to search - handle it linearly
						NkSetFileOffset(f, left.pageEnd);
					} else {
						if (probe < 2) {
							if (probe == 0) {
								// first probe (interpolate)
								float64 dataBytes = right.pageEnd - left.pageStart;
								bytesPerSample = dataBytes / right.lastDecodedSample;
								offset = left.pageStart + bytesPerSample * (lastSampleLimit - left.lastDecodedSample);
							} else {
								// second probe (try to bound the other side)
								float64 error = ((float64)lastSampleLimit - mid.lastDecodedSample) * bytesPerSample;
								if (error >= 0 && error < 8000)
									error = 8000;
								if (error < 0 && error > -8000)
									error = -8000;
								offset += error * 2;
							}

							// ensure the offset is valid
							if (offset < left.pageEnd)
								offset = left.pageEnd;
							if (offset > right.pageStart - 65536)
								offset = right.pageStart - 65536;

							NkSetFileOffset(f, (uint32)offset);
						} else {
							// binary search for large ranges (offset by 32K to ensure
							// we don't hit the right page)
							NkSetFileOffset(f, left.pageEnd + (delta / 2) - 32768);
						}

						if (!vorbisFindPage(f, NULL, NULL))
							goto error;
					}

					for (;;) {
						if (!NkGetSeekPageInfo(f, &mid))
							goto error;
						if (mid.lastDecodedSample != ~0U)
							break;
						// (untested) no frames end on this page
						NkSetFileOffset(f, mid.pageEnd);
						assert(mid.pageStart < right.pageStart);
					}

					// if we've just found the last page again then we're in a tricky file,
					// and we're close enough (if it wasn't an interpolation probe).
					if (mid.pageStart == right.pageStart) {
						if (probe >= 2 || delta <= 65536)
							break;
					} else {
						if (lastSampleLimit < mid.lastDecodedSample)
							right = mid;
						else
							left = mid;
					}

					++probe;
				}

				// seek back to start of the last packet
				pageStart = left.pageStart;
				NkSetFileOffset(f, pageStart);
				if (!NkStartPage(f))
					return error(f, NK_VORBIS_SEEK_FAILED);
				endPos = f->endSegWithKnownLoc;
				assert(endPos >= 0);

				for (;;) {
					for (i = endPos; i > 0; --i)
						if (f->segments[i - 1] != 255)
							break;

					startSegWithKnownLoc = i;

					if (startSegWithKnownLoc > 0 || !(f->pageFlag & PAGEFLAG_continued_packet))
						break;

					// (untested) the final packet begins on an earlier page
					if (!NkGoToPageBefore(f, pageStart))
						goto error;

					pageStart = NkVorbisGetFileOffset(f);
					if (!NkStartPage(f))
						goto error;
					endPos = f->segmentCount - 1;
				}

				// prepare to start decoding
				f->currentLocValid = false;
				f->lastSeg = false;
				f->validBits = 0;
				f->packetBytes = 0;
				f->bytesInSeg = 0;
				f->previousLength = 0;
				f->nextSeg = startSegWithKnownLoc;

				for (i = 0; i < startSegWithKnownLoc; i++)
					NkSkip(f, f->segments[i]);

				// start decoding (optimizable - this frame is generally discarded)
				if (!NkVorbisPumpFirstFrame(f))
					return 0;
				if (f->currentLoc > sampleNumber)
					return error(f, NK_VORBIS_SEEK_FAILED);
				return 1;

			error:
				// try to restore the file to a valid state
				NkVorbisSeekStart(f);
				return error(f, NK_VORBIS_SEEK_FAILED);
			}

			// the same as NkVorbisDecodeInitial, but without advancing
			static int32 NkPeekDecodeInitial(NkVorbisDecoder *f, int32 *pLeftStart, int32 *pLeftEnd, int32 *pRightStart,
											 int32 *pRightEnd, int32 *mode) {
				int32 bitsRead, bytesRead;

				if (!NkVorbisDecodeInitial(f, pLeftStart, pLeftEnd, pRightStart, pRightEnd, mode))
					return 0;

				// either 1 or 2 bytes were read, figure out which so we can rewind
				bitsRead = 1 + NkIlog(f->modeCount - 1);
				if (f->modeConfig[*mode].blockflag)
					bitsRead += 2;
				bytesRead = (bitsRead + 7) / 8;

				f->bytesInSeg += bytesRead;
				f->packetBytes -= bytesRead;
				NkSkip(f, -bytesRead);
				if (f->nextSeg == -1)
					f->nextSeg = f->segmentCount - 1;
				else
					f->nextSeg--;
				f->validBits = 0;

				return 1;
			}

			int32 NkVorbisSeekFrame(NkVorbisDecoder *f, uint32 sampleNumber) {
				uint32 maxFrameSamples;

				if (IS_PUSH_MODE(f))
					return error(f, NK_VORBIS_INVALID_API_MIXING);

				// fast page-level search
				if (!NkSeekToSampleCoarse(f, sampleNumber))
					return 0;

				assert(f->currentLocValid);
				assert(f->currentLoc <= sampleNumber);

				// linear search for the relevant packet
				maxFrameSamples = (f->blocksize1 * 3 - f->blocksize0) >> 2;
				while (f->currentLoc < sampleNumber) {
					int32 leftStart, leftEnd, rightStart, rightEnd, mode, frameSamples;
					if (!NkPeekDecodeInitial(f, &leftStart, &leftEnd, &rightStart, &rightEnd, &mode))
						return error(f, NK_VORBIS_SEEK_FAILED);
					// calculate the number of samples returned by the next frame
					frameSamples = rightStart - leftStart;
					if (f->currentLoc + frameSamples > sampleNumber) {
						return 1; // the next frame will contain the sample
					} else if (f->currentLoc + frameSamples + maxFrameSamples > sampleNumber) {
						// there's a chance the frame after this could contain the sample
						NkVorbisPumpFirstFrame(f);
					} else {
						// this frame is too early to be relevant
						f->currentLoc += frameSamples;
						f->previousLength = 0;
						NkMaybeStartPacket(f);
						NkFlushPacket(f);
					}
				}
				// the next frame should start with the sample
				if (f->currentLoc != sampleNumber)
					return error(f, NK_VORBIS_SEEK_FAILED);
				return 1;
			}

			int32 NkVorbisSeek(NkVorbisDecoder *f, uint32 sampleNumber) {
				if (!NkVorbisSeekFrame(f, sampleNumber))
					return 0;

				if (sampleNumber != f->currentLoc) {
					int32 n;
					uint32 frameStart = f->currentLoc;
					NkVorbisGetFrameFloat(f, &n, NULL);
					assert(sampleNumber > frameStart);
					assert(f->channelBufferStart + (int32)(sampleNumber - frameStart) <= f->channelBufferEnd);
					f->channelBufferStart += (sampleNumber - frameStart);
				}

				return 1;
			}

			int32 NkVorbisSeekStart(NkVorbisDecoder *f) {
				if (IS_PUSH_MODE(f)) {
					return error(f, NK_VORBIS_INVALID_API_MIXING);
				}
				NkSetFileOffset(f, f->firstAudioPageOffset);
				f->previousLength = 0;
				f->firstDecode = true;
				f->nextSeg = -1;
				return NkVorbisPumpFirstFrame(f);
			}

			uint32 NkVorbisStreamLengthInSamples(NkVorbisDecoder *f) {
				uint32 restoreOffset, previousSafe;
				uint32 end, lastPageLoc;

				if (IS_PUSH_MODE(f))
					return error(f, NK_VORBIS_INVALID_API_MIXING);
				if (!f->totalSamples) {
					uint32 last;
					uint32 lo, hi;
					char header[6];

					// first, store the current decode position so we can restore it
					restoreOffset = NkVorbisGetFileOffset(f);

					// now we want to seek back 64K from the end (the last page must
					// be at most a little less than 64K, but let's allow a little slop)
					if (f->streamLen >= 65536 && f->streamLen - 65536 >= f->firstAudioPageOffset)
						previousSafe = f->streamLen - 65536;
					else
						previousSafe = f->firstAudioPageOffset;

					NkSetFileOffset(f, previousSafe);
					// previousSafe is now our candidate 'earliest known place that seeking
					// to will lead to the final page'

					if (!vorbisFindPage(f, &end, &last)) {
						// if we can't find a page, we're hosed!
						f->error = NK_VORBIS_CANT_FIND_LAST_PAGE;
						f->totalSamples = 0xffffffff;
						goto done;
					}

					// check if there are more pages
					lastPageLoc = NkVorbisGetFileOffset(f);

					// stop when the lastPage flag is set, not when we reach eof;
					// this allows us to stop int16 of a 'fileSection' end without
					// explicitly checking the length of the section
					while (!last) {
						NkSetFileOffset(f, end);
						if (!vorbisFindPage(f, &end, &last)) {
							// the last page we found didn't have the 'last page' flag
							// set. whoops!
							break;
						}
						// previousSafe = lastPageLoc+1; // NOTE: not used after this point, but note for debugging
						lastPageLoc = NkVorbisGetFileOffset(f);
					}

					NkSetFileOffset(f, lastPageLoc);

					// parse the header
					NkGetn(f, (uint8 *)header, 6);
					// extract the absolute granule position
					lo = get32(f);
					hi = get32(f);
					if (lo == 0xffffffff && hi == 0xffffffff) {
						f->error = NK_VORBIS_CANT_FIND_LAST_PAGE;
						f->totalSamples = SAMPLE_unknown;
						goto done;
					}
					if (hi)
						lo = 0xfffffffe; // saturate
					f->totalSamples = lo;

					f->pLast.pageStart = lastPageLoc;
					f->pLast.pageEnd = end;
					f->pLast.lastDecodedSample = lo;

				done:
					NkSetFileOffset(f, restoreOffset);
				}
				return f->totalSamples == SAMPLE_unknown ? 0 : f->totalSamples;
			}

			float32 NkVorbisStreamLengthInSeconds(NkVorbisDecoder *f) {
				return NkVorbisStreamLengthInSamples(f) / (float32)f->sampleRate;
			}

			int32 NkVorbisGetFrameFloat(NkVorbisDecoder *f, int32 *channels, float32 ***output) {
				int32 len, right, left, i;
				if (IS_PUSH_MODE(f))
					return error(f, NK_VORBIS_INVALID_API_MIXING);

				if (!NkVorbisDecodePacket(f, &len, &left, &right)) {
					f->channelBufferStart = f->channelBufferEnd = 0;
					return 0;
				}

				len = NkVorbisFinishFrame(f, len, left, right);
				for (i = 0; i < f->channels; ++i)
					f->outputs[i] = f->channelBuffers[i] + left;

				f->channelBufferStart = left;
				f->channelBufferEnd = left + len;

				if (channels)
					*channels = f->channels;
				if (output)
					*output = f->outputs;
				return len;
			}

#ifndef NK_VORBIS_NO_STDIO

			NkVorbisDecoder *NkVorbisOpenFileSection(FILE *file, int32 closeOnFree, int32 *error,
													 const NkVorbisAllocator *alloc, uint32 length) {
				NkVorbisDecoder *f, p;
				NkVorbisInit(&p, alloc);
				p.f = file;
				p.fStart = (uint32)ftell(file);
				p.streamLen = length;
				p.closeOnFree = closeOnFree;
				if (NkStartDecoder(&p)) {
					f = NkVorbisAlloc(&p);
					if (f) {
						*f = p;
						NkVorbisPumpFirstFrame(f);
						return f;
					}
				}
				if (error)
					*error = p.error;
				NkVorbisDeinit(&p);
				return NULL;
			}

			NkVorbisDecoder *NkVorbisOpenFile(FILE *file, int32 closeOnFree, int32 *error,
											  const NkVorbisAllocator *alloc) {
				uint32 len, start;
				start = (uint32)ftell(file);
				fseek(file, 0, SEEK_END);
				len = (uint32)(ftell(file) - start);
				fseek(file, start, SEEK_SET);
				return NkVorbisOpenFileSection(file, closeOnFree, error, alloc, len);
			}

			NkVorbisDecoder *NkVorbisOpenFilename(const char *filename, int32 *error, const NkVorbisAllocator *alloc) {
				FILE *f;
#if defined(NKENTSEU_PLATFORM_WINDOWS) && defined(__STDC_WANT_SECURE_LIB__)
				if (0 != fopenS(&f, filename, "rb"))
					f = NULL;
#else
				f = fopen(filename, "rb");
#endif
				if (f)
					return NkVorbisOpenFile(f, true, error, alloc);
				if (error)
					*error = NK_VORBIS_FILE_OPEN_FAILURE;
				return NULL;
			}
#endif // NK_VORBIS_NO_STDIO

			NkVorbisDecoder *NkVorbisOpenMemory(const uint8 *data, int32 len, int32 *error,
												const NkVorbisAllocator *alloc) {
				NkVorbisDecoder *f, p;
				if (!data) {
					if (error)
						*error = NK_VORBIS_UNEXPECTED_EOF;
					return NULL;
				}
				NkVorbisInit(&p, alloc);
				p.stream = (uint8 *)data;
				p.streamEnd = (uint8 *)data + len;
				p.streamStart = (uint8 *)p.stream;
				p.streamLen = len;
				p.pushMode = false;
				if (NkStartDecoder(&p)) {
					f = NkVorbisAlloc(&p);
					if (f) {
						*f = p;
						NkVorbisPumpFirstFrame(f);
						if (error)
							*error = NK_VORBIS__NO_ERROR;
						return f;
					}
				}
				if (error)
					*error = p.error;
				NkVorbisDeinit(&p);
				return NULL;
			}

#ifndef NK_VORBIS_NO_INTEGER_CONVERSION
#define PLAYBACK_MONO 1
#define PLAYBACK_LEFT 2
#define PLAYBACK_RIGHT 4

#define L (PLAYBACK_LEFT | PLAYBACK_MONO)
#define C (PLAYBACK_LEFT | PLAYBACK_RIGHT | PLAYBACK_MONO)
#define R (PLAYBACK_RIGHT | PLAYBACK_MONO)

			static int8 channelPosition[7][6] = {
				{0}, {C}, {L, R}, {L, C, R}, {L, R, L, R}, {L, C, R, L, R}, {L, C, R, L, R, C},
			};

#ifndef NK_VORBIS_NO_FAST_SCALED_FLOAT
			typedef union {
				float32 f;
				int32 i;
			} floatConv;
			typedef char NkVorbisFloatSizeTest[sizeof(float32) == 4 && sizeof(int32) == 4];
#define FASTDEF(x) floatConv x
// add (1<<23) to convert to int32, then divide by 2^SHIFT, then add 0.5/2^SHIFT to round
#define MAGIC(SHIFT) (1.5f * (1 << (23 - SHIFT)) + 0.5f / (1 << SHIFT))
#define ADDEND(SHIFT) (((150 - SHIFT) << 23) + (1 << 22))
#define FAST_SCALED_FLOAT_TO_INT(temp, x, s) (temp.f = (x) + MAGIC(s), temp.i - ADDEND(s))
#define checkEndianness()
#else
#define FAST_SCALED_FLOAT_TO_INT(temp, x, s) ((int32)((x) * (1 << (s))))
#define checkEndianness()
#define FASTDEF(x)
#endif

			static void NkCopySamples(int16 *dest, float32 *src, int32 len) {
				int32 i;
				checkEndianness();
				for (i = 0; i < len; ++i) {
					FASTDEF(temp);
					int32 v = FAST_SCALED_FLOAT_TO_INT(temp, src[i], 15);
					if ((uint32)(v + 32768) > 65535)
						v = v < 0 ? -32768 : 32767;
					dest[i] = v;
				}
			}

			static void NkComputeSamples(int32 mask, int16 *output, int32 numC, float32 **data, int32 dOffset,
										 int32 len) {
#define NK_VORBIS_BUFFER_SIZE 32
				float32 buffer[NK_VORBIS_BUFFER_SIZE];
				int32 i, j, o, n = NK_VORBIS_BUFFER_SIZE;
				checkEndianness();
				for (o = 0; o < len; o += NK_VORBIS_BUFFER_SIZE) {
					memset(buffer, 0, sizeof(buffer));
					if (o + n > len)
						n = len - o;
					for (j = 0; j < numC; ++j) {
						if (channelPosition[numC][j] & mask) {
							for (i = 0; i < n; ++i)
								buffer[i] += data[j][dOffset + o + i];
						}
					}
					for (i = 0; i < n; ++i) {
						FASTDEF(temp);
						int32 v = FAST_SCALED_FLOAT_TO_INT(temp, buffer[i], 15);
						if ((uint32)(v + 32768) > 65535)
							v = v < 0 ? -32768 : 32767;
						output[o + i] = v;
					}
				}
#undef NK_VORBIS_BUFFER_SIZE
			}

			static void NkComputeStereoSamples(int16 *output, int32 numC, float32 **data, int32 dOffset, int32 len) {
#define NK_VORBIS_BUFFER_SIZE 32
				float32 buffer[NK_VORBIS_BUFFER_SIZE];
				int32 i, j, o, n = NK_VORBIS_BUFFER_SIZE >> 1;
				// o is the offset in the source data
				checkEndianness();
				for (o = 0; o < len; o += NK_VORBIS_BUFFER_SIZE >> 1) {
					// o2 is the offset in the output data
					int32 o2 = o << 1;
					memset(buffer, 0, sizeof(buffer));
					if (o + n > len)
						n = len - o;
					for (j = 0; j < numC; ++j) {
						int32 m = channelPosition[numC][j] & (PLAYBACK_LEFT | PLAYBACK_RIGHT);
						if (m == (PLAYBACK_LEFT | PLAYBACK_RIGHT)) {
							for (i = 0; i < n; ++i) {
								buffer[i * 2 + 0] += data[j][dOffset + o + i];
								buffer[i * 2 + 1] += data[j][dOffset + o + i];
							}
						} else if (m == PLAYBACK_LEFT) {
							for (i = 0; i < n; ++i) {
								buffer[i * 2 + 0] += data[j][dOffset + o + i];
							}
						} else if (m == PLAYBACK_RIGHT) {
							for (i = 0; i < n; ++i) {
								buffer[i * 2 + 1] += data[j][dOffset + o + i];
							}
						}
					}
					for (i = 0; i < (n << 1); ++i) {
						FASTDEF(temp);
						int32 v = FAST_SCALED_FLOAT_TO_INT(temp, buffer[i], 15);
						if ((uint32)(v + 32768) > 65535)
							v = v < 0 ? -32768 : 32767;
						output[o2 + i] = v;
					}
				}
#undef NK_VORBIS_BUFFER_SIZE
			}

			static void NkConvertSamplesShort(int32 bufC, int16 **buffer, int32 bOffset, int32 dataC, float32 **data,
											  int32 dOffset, int32 samples) {
				int32 i;
				if (bufC != dataC && bufC <= 2 && dataC <= 6) {
					static int32 channelSelector[3][2] = {{0}, {PLAYBACK_MONO}, {PLAYBACK_LEFT, PLAYBACK_RIGHT}};
					for (i = 0; i < bufC; ++i)
						NkComputeSamples(channelSelector[bufC][i], buffer[i] + bOffset, dataC, data, dOffset, samples);
				} else {
					int32 limit = bufC < dataC ? bufC : dataC;
					for (i = 0; i < limit; ++i)
						NkCopySamples(buffer[i] + bOffset, data[i] + dOffset, samples);
					for (; i < bufC; ++i)
						memset(buffer[i] + bOffset, 0, sizeof(int16) * samples);
				}
			}

			int32 NkVorbisGetFrameShort(NkVorbisDecoder *f, int32 numC, int16 **buffer, int32 numSamples) {
				float32 **output = NULL;
				int32 len = NkVorbisGetFrameFloat(f, NULL, &output);
				if (len > numSamples)
					len = numSamples;
				if (len)
					NkConvertSamplesShort(numC, buffer, 0, f->channels, output, 0, len);
				return len;
			}

			static void NkConvertChannelsShortInterleaved(int32 bufC, int16 *buffer, int32 dataC, float32 **data,
														  int32 dOffset, int32 len) {
				int32 i;
				checkEndianness();
				if (bufC != dataC && bufC <= 2 && dataC <= 6) {
					assert(bufC == 2);
					for (i = 0; i < bufC; ++i)
						NkComputeStereoSamples(buffer, dataC, data, dOffset, len);
				} else {
					int32 limit = bufC < dataC ? bufC : dataC;
					int32 j;
					for (j = 0; j < len; ++j) {
						for (i = 0; i < limit; ++i) {
							FASTDEF(temp);
							float32 f = data[i][dOffset + j];
							int32 v = FAST_SCALED_FLOAT_TO_INT(temp, f, 15); // data[i][dOffset+j],15);
							if ((uint32)(v + 32768) > 65535)
								v = v < 0 ? -32768 : 32767;
							*buffer++ = v;
						}
						for (; i < bufC; ++i)
							*buffer++ = 0;
					}
				}
			}

			int32 NkVorbisGetFrameShortInterleaved(NkVorbisDecoder *f, int32 numC, int16 *buffer, int32 numShorts) {
				float32 **output;
				int32 len;
				if (numC == 1)
					return NkVorbisGetFrameShort(f, numC, &buffer, numShorts);
				len = NkVorbisGetFrameFloat(f, NULL, &output);
				if (len) {
					if (len * numC > numShorts)
						len = numShorts / numC;
					NkConvertChannelsShortInterleaved(numC, buffer, f->channels, output, 0, len);
				}
				return len;
			}

			int32 NkVorbisGetSamplesShortInterleaved(NkVorbisDecoder *f, int32 channels, int16 *buffer,
													 int32 numShorts) {
				float32 **outputs;
				int32 len = numShorts / channels;
				int32 n = 0;
				while (n < len) {
					int32 k = f->channelBufferEnd - f->channelBufferStart;
					if (n + k >= len)
						k = len - n;
					if (k)
						NkConvertChannelsShortInterleaved(channels, buffer, f->channels, f->channelBuffers,
														  f->channelBufferStart, k);
					buffer += k * channels;
					n += k;
					f->channelBufferStart += k;
					if (n == len)
						break;
					if (!NkVorbisGetFrameFloat(f, NULL, &outputs))
						break;
				}
				return n;
			}

			int32 NkVorbisGetSamplesShort(NkVorbisDecoder *f, int32 channels, int16 **buffer, int32 len) {
				float32 **outputs;
				int32 n = 0;
				while (n < len) {
					int32 k = f->channelBufferEnd - f->channelBufferStart;
					if (n + k >= len)
						k = len - n;
					if (k)
						NkConvertSamplesShort(channels, buffer, n, f->channels, f->channelBuffers,
											  f->channelBufferStart, k);
					n += k;
					f->channelBufferStart += k;
					if (n == len)
						break;
					if (!NkVorbisGetFrameFloat(f, NULL, &outputs))
						break;
				}
				return n;
			}

#ifndef NK_VORBIS_NO_STDIO
			int32 NkVorbisDecodeFilename(const char *filename, int32 *channels, int32 *sampleRate, int16 **output) {
				int32 dataLen, offset, total, limit, error;
				int16 *data;
				NkVorbisDecoder *v = NkVorbisOpenFilename(filename, &error, NULL);
				if (v == NULL)
					return -1;
				limit = v->channels * 4096;
				*channels = v->channels;
				if (sampleRate)
					*sampleRate = v->sampleRate;
				offset = dataLen = 0;
				total = limit;
				data = (int16 *)malloc(total * sizeof(*data));
				if (data == NULL) {
					NkVorbisClose(v);
					return -2;
				}
				for (;;) {
					int32 n = NkVorbisGetFrameShortInterleaved(v, v->channels, data + offset, total - offset);
					if (n == 0)
						break;
					dataLen += n;
					offset += n * v->channels;
					if (offset + limit > total) {
						int16 *data2;
						total *= 2;
						data2 = (int16 *)realloc(data, total * sizeof(*data));
						if (data2 == NULL) {
							free(data);
							NkVorbisClose(v);
							return -2;
						}
						data = data2;
					}
				}
				*output = data;
				NkVorbisClose(v);
				return dataLen;
			}
#endif // NO_STDIO

			int32 NkVorbisDecodeMemory(const uint8 *mem, int32 len, int32 *channels, int32 *sampleRate,
									   int16 **output) {
				int32 dataLen, offset, total, limit, error;
				int16 *data;
				NkVorbisDecoder *v = NkVorbisOpenMemory(mem, len, &error, NULL);
				if (v == NULL)
					return -1;
				limit = v->channels * 4096;
				*channels = v->channels;
				if (sampleRate)
					*sampleRate = v->sampleRate;
				offset = dataLen = 0;
				total = limit;
				data = (int16 *)malloc(total * sizeof(*data));
				if (data == NULL) {
					NkVorbisClose(v);
					return -2;
				}
				for (;;) {
					int32 n = NkVorbisGetFrameShortInterleaved(v, v->channels, data + offset, total - offset);
					if (n == 0)
						break;
					dataLen += n;
					offset += n * v->channels;
					if (offset + limit > total) {
						int16 *data2;
						total *= 2;
						data2 = (int16 *)realloc(data, total * sizeof(*data));
						if (data2 == NULL) {
							free(data);
							NkVorbisClose(v);
							return -2;
						}
						data = data2;
					}
				}
				*output = data;
				NkVorbisClose(v);
				return dataLen;
			}
#endif // NK_VORBIS_NO_INTEGER_CONVERSION

			int32 NkVorbisGetSamplesFloatInterleaved(NkVorbisDecoder *f, int32 channels, float32 *buffer,
													 int32 numFloats) {
				float32 **outputs;
				int32 len = numFloats / channels;
				int32 n = 0;
				int32 z = f->channels;
				if (z > channels)
					z = channels;
				while (n < len) {
					int32 i, j;
					int32 k = f->channelBufferEnd - f->channelBufferStart;
					if (n + k >= len)
						k = len - n;
					for (j = 0; j < k; ++j) {
						for (i = 0; i < z; ++i)
							*buffer++ = f->channelBuffers[i][f->channelBufferStart + j];
						for (; i < channels; ++i)
							*buffer++ = 0;
					}
					n += k;
					f->channelBufferStart += k;
					if (n == len)
						break;
					if (!NkVorbisGetFrameFloat(f, NULL, &outputs))
						break;
				}
				return n;
			}

			int32 NkVorbisGetSamplesFloat(NkVorbisDecoder *f, int32 channels, float32 **buffer, int32 numSamples) {
				float32 **outputs;
				int32 n = 0;
				int32 z = f->channels;
				if (z > channels)
					z = channels;
				while (n < numSamples) {
					int32 i;
					int32 k = f->channelBufferEnd - f->channelBufferStart;
					if (n + k >= numSamples)
						k = numSamples - n;
					if (k) {
						for (i = 0; i < z; ++i)
							memcpy(buffer[i] + n, f->channelBuffers[i] + f->channelBufferStart, sizeof(float32) * k);
						for (; i < channels; ++i)
							memset(buffer[i] + n, 0, sizeof(float32) * k);
					}
					n += k;
					f->channelBufferStart += k;
					if (n == numSamples)
						break;
					if (!NkVorbisGetFrameFloat(f, NULL, &outputs))
						break;
				}
				return n;
			}
#endif // NK_VORBIS_NO_PULLDATA_API

			/* Version history
	1.17    - 2019-07-08 - fix CVE-2019-13217, -13218, -13219, -13220, -13221, -13222, -13223
						   found with Mayhem by ForAllSecure
	1.16    - 2019-03-04 - fix warnings
	1.15    - 2019-02-07 - explicit failure if Ogg Skeleton data is found
	1.14    - 2018-02-11 - delete bogus dealloca usage
	1.13    - 2018-01-29 - fix truncation of last frame (hopefully)
	1.12    - 2017-11-21 - limit residue begin/end to blocksize/2 to avoid large temp allocs in bad/corrupt files
	1.11    - 2017-07-23 - fix MinGW compilation
	1.10    - 2017-03-03 - more robust seeking; fix negative NkIlog(); clear error in openMemory
	1.09    - 2016-04-04 - back out 'avoid discarding last frame' fix from previous version
	1.08    - 2016-04-02 - fixed multiple warnings; fix setup memory leaks;
						   avoid discarding last frame of audio data
	1.07    - 2015-01-16 - fixed some warnings, fix mingw, const-correct API
						   some more crash fixes when out of memory or with corrupt files
	1.06    - 2015-08-31 - full, correct support for seeking API (Dougall Johnson)
						   some crash fixes when out of memory or with corrupt files
	1.05    - 2015-04-19 - don't define NKENTSEU_FORCE_INLINE if it's redundant
	1.04    - 2014-08-27 - fix missing const-correct case in API
	1.03    - 2014-08-07 - Warning fixes
	1.02    - 2014-07-09 - Declare qsort compare function _cdecl on windows
	1.01    - 2014-06-18 - fix NkVorbisGetSamplesFloat
	1.0     - 2014-05-26 - fix memory leaks; fix warnings; fix bugs in multichannel
						   (API change) report sample rate for decode-full-file funcs
	0.99996 - bracket #include <malloc.h> for macintosh compilation by Laurent Gomila
	0.99995 - use union instead of pointer-cast for fast-float32-to-int32 to avoid alias-optimization problem
	0.99994 - change fast-float32-to-int32 to work in single-precision FPU mode, remove endian-dependence
	0.99993 - remove assert that fired on legal files with empty tables
	0.99992 - rewind-to-start
	0.99991 - bugfix to NkVorbisGetSamplesShort by Bernhard Wodo
	0.9999 - (should have been 0.99990) fix no-CRT support, compiling as C++
	0.9998 - add a full-decode function with a memory source
	0.9997 - fix a bug in the read-from-FILE case in 0.9996 addition
	0.9996 - query length of vorbis stream in samples/seconds
	0.9995 - bugfix to another optimization that only happened in certain files
	0.9994 - bugfix to one of the optimizations that caused significant (but inaudible?) errors
	0.9993 - performance improvements; runs in 99% to 104% of time of reference implementation
	0.9992 - performance improvement of IMDCT; now performs close to reference implementation
	0.9991 - performance improvement of IMDCT
	0.999 - (should have been 0.9990) performance improvement of IMDCT
	0.998 - no-CRT support from Casey Muratori
	0.997 - bugfixes for bugs found by Terje Mathisen
	0.996 - bugfix: fast-huffman decode initialized incorrectly for sparse codebooks; fixing gives 10% speedup - found
   by Terje Mathisen 0.995 - bugfix: fix to 'effective' overrun detection - found by Terje Mathisen 0.994 - bugfix:
   garbage decode on final VQ symbol of a non-multiple - found by Terje Mathisen 0.993 - bugfix: pushdata API required 1
   extra byte for empty page (failed to consume final page if empty) - found by Terje Mathisen 0.992 - fixes for MinGW
   warning 0.991 - turn fast-float32-conversion on by default 0.990 - fix push-mode seek recovery if you seek into the
   headers 0.98b - fix to bad release of 0.98 0.98 - fix push-mode seek recovery; robustify float32-to-int32 and support
   non-fast mode 0.97 - builds under c++ (typecasting, don't use 'class' keyword) 0.96 - somehow MY 0.95 was right, but
   the web one was wrong, so here's my 0.95 rereleased as 0.96, fixes a typo in the clamping code 0.95 - clamping code
   for 16-bit functions 0.94 - not publically released 0.93 - fixed all-zero-floor case (was decoding garbage) 0.92 -
   fixed a memory leak 0.91 - conditional compiles to omit parts of the API and the infrastructure to support them:
   NK_VORBIS_NO_PULLDATA_API, NK_VORBIS_NO_PUSHDATA_API, NK_VORBIS_NO_STDIO, NK_VORBIS_NO_INTEGER_CONVERSION 0.90 -
   first public release
*/

#endif // NK_VORBIS_HEADER_ONLY

		} // namespace vorbis
	} // namespace audio
} // namespace nkentseu

// ═════════════════════════════════════════════════════════════════════════════
//  Fin de l'implementation interne. Le wrapper public NkOGGVorbisCodec::Decode
//  ci-dessous est l'API consommee par NKAudio (cf. NkOGGVorbisCodec.h).
// ═════════════════════════════════════════════════════════════════════════════

namespace nkentseu {
	namespace audio {

		// ════════════════════════════════════════════════════════════════════
		//  Decode : wrapper vorbis -> AudioSample Nkentseu.
		//
		//  Flow :
		//   1. NkVorbisOpenMemory(data, size) -> handle
		//   2. NkVorbisGetInfo(handle) -> channels, sampleRate
		//   3. Allouer PCM dynamic (croit a la volee si pas assez)
		//   4. Boucle NkVorbisGetSamplesFloatInterleaved jusqu'a 0 frames
		//   5. NkVorbisClose(handle)
		//   6. Retourner AudioSample avec data pointe sur le buffer alloue
		//
		//  L'AudioSample retourne possede la memoire (sera libere via
		//  AudioLoader::Free qui appelle memory::NkFree).
		// ════════════════════════════════════════════════════════════════════

		AudioSample NkOGGVorbisCodec::Decode(const uint8 *data, usize size, memory::NkAllocator *allocator) noexcept {
			AudioSample empty{};
			if (!data || size < 27) {
				logger.Error("[OGG] Decode : donnees invalides (taille {0}).", (int32)size);
				return empty;
			}

			int32 err = 0;
			vorbis::NkVorbisDecoder *vb = vorbis::NkVorbisOpenMemory(data, int32(size), &err, nullptr);
			if (!vb) {
				logger.Error("[OGG] NkVorbisOpenMemory echoue (err={0}).", err);
				return empty;
			}

			vorbis::NkVorbisInfo info = vorbis::NkVorbisGetInfo(vb);
			int32 channels = info.channels;
			int32 sampleRate = int32(info.sampleRate);
			if (channels <= 0 || channels > 8 || sampleRate <= 0) {
				vorbis::NkVorbisClose(vb);
				logger.Error("[OGG] Info invalide : ch={0}, sr={1}.", channels, sampleRate);
				return empty;
			}

			// Allocation initiale : 256K floats (1 MB pour stereo = ~5.9s)
			usize cap = 256 * 1024;
			usize len = 0;
			float32 *pcm = static_cast<float32 *>(memory::NkAlloc(cap * sizeof(float32), allocator, sizeof(float32)));
			if (!pcm) {
				vorbis::NkVorbisClose(vb);
				logger.Error("[OGG] Alloc PCM initiale echoue.");
				return empty;
			}

			const int32 chunkFloats = 4096 * channels;
			for (;;) {
				if (len + usize(chunkFloats) > cap) {
					usize newCap = cap * 2;
					while (len + usize(chunkFloats) > newCap)
						newCap *= 2;
					float32 *newPcm =
						static_cast<float32 *>(memory::NkAlloc(newCap * sizeof(float32), allocator, sizeof(float32)));
					if (!newPcm) {
						memory::NkFree(pcm, allocator);
						vorbis::NkVorbisClose(vb);
						logger.Error("[OGG] Realloc PCM echoue.");
						return empty;
					}
					::memcpy(newPcm, pcm, len * sizeof(float32));
					memory::NkFree(pcm, allocator);
					pcm = newPcm;
					cap = newCap;
				}
				int32 n = vorbis::NkVorbisGetSamplesFloatInterleaved(vb, channels, pcm + len, chunkFloats);
				if (n == 0)
					break;
				len += usize(n) * usize(channels);
			}

			vorbis::NkVorbisClose(vb);

			AudioSample s{};
			s.data = pcm;
			s.frameCount = len / usize(channels);
			s.channels = channels;
			s.sampleRate = sampleRate;
			logger.Info("[OGG] Decode OK : {0} frames, {1} ch, {2} Hz "
						"(duree {3:.2}s).",
						(int32)s.frameCount, s.channels, s.sampleRate, float64(s.frameCount) / float64(s.sampleRate));
			return s;
		}

	} // namespace audio
} // namespace nkentseu
