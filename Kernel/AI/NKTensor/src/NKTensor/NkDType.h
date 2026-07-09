// =============================================================================
// NkDType.h — types de données scalaires d'un tenseur (NKAI).
// From-scratch, pédagogique : on part des flottants (ML) + quelques entiers.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace ai {

		// Type scalaire stocké dans un tenseur. On commence volontairement petit ;
		// fp16/bf16/int8 (quantization) viendront plus tard (cf ROADMAP « Plus tard »).
		enum class NkDType : uint8 {
			NK_F32 = 0, // float
			NK_F64,		// double
			NK_I32,		// int32
			NK_I64,		// int64
			NK_U8,		// uint8
			NK_COUNT
		};

		// Taille d'un élément en octets.
		inline nk_size NkDTypeSize(NkDType t) {
			switch (t) {
				case NkDType::NK_F32:
					return 4;
				case NkDType::NK_F64:
					return 8;
				case NkDType::NK_I32:
					return 4;
				case NkDType::NK_I64:
					return 8;
				case NkDType::NK_U8:
					return 1;
				default:
					return 0;
			}
		}

		inline const char *NkDTypeName(NkDType t) {
			switch (t) {
				case NkDType::NK_F32:
					return "f32";
				case NkDType::NK_F64:
					return "f64";
				case NkDType::NK_I32:
					return "i32";
				case NkDType::NK_I64:
					return "i64";
				case NkDType::NK_U8:
					return "u8";
				default:
					return "?";
			}
		}

		inline bool NkDTypeIsFloat(NkDType t) {
			return t == NkDType::NK_F32 || t == NkDType::NK_F64;
		}

// Distribue une action templée sur le type C++ correspondant au NkDType.
// Variadique : le corps peut contenir des virgules (appels templés, etc.).
// Usage : NK_DTYPE_DISPATCH(t, T, { /* T = float/double/... */ });
#define NK_DTYPE_DISPATCH(DT, TVAR, ...)                                                                               \
	do {                                                                                                               \
		switch (DT) {                                                                                                  \
			case ::nkentseu::ai::NkDType::NK_F32: {                                                                    \
				using TVAR = float;                                                                                    \
				__VA_ARGS__                                                                                            \
			} break;                                                                                                   \
			case ::nkentseu::ai::NkDType::NK_F64: {                                                                    \
				using TVAR = double;                                                                                   \
				__VA_ARGS__                                                                                            \
			} break;                                                                                                   \
			case ::nkentseu::ai::NkDType::NK_I32: {                                                                    \
				using TVAR = ::nkentseu::int32;                                                                        \
				__VA_ARGS__                                                                                            \
			} break;                                                                                                   \
			case ::nkentseu::ai::NkDType::NK_I64: {                                                                    \
				using TVAR = ::nkentseu::int64;                                                                        \
				__VA_ARGS__                                                                                            \
			} break;                                                                                                   \
			case ::nkentseu::ai::NkDType::NK_U8: {                                                                     \
				using TVAR = ::nkentseu::uint8;                                                                        \
				__VA_ARGS__                                                                                            \
			} break;                                                                                                   \
			default:                                                                                                   \
				break;                                                                                                 \
		}                                                                                                              \
	} while (0)

// Variante limitée aux flottants (ops mathématiques : exp, sigmoid…).
#define NK_DTYPE_DISPATCH_FLOAT(DT, TVAR, ...)                                                                         \
	do {                                                                                                               \
		switch (DT) {                                                                                                  \
			case ::nkentseu::ai::NkDType::NK_F32: {                                                                    \
				using TVAR = float;                                                                                    \
				__VA_ARGS__                                                                                            \
			} break;                                                                                                   \
			case ::nkentseu::ai::NkDType::NK_F64: {                                                                    \
				using TVAR = double;                                                                                   \
				__VA_ARGS__                                                                                            \
			} break;                                                                                                   \
			default:                                                                                                   \
				break;                                                                                                 \
		}                                                                                                              \
	} while (0)

	} // namespace ai
} // namespace nkentseu
