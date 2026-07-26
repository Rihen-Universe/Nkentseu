// =============================================================================
// NkSLComputeCheck — vérifie que le COMPUTE NkSL se convertit vers TOUS les
// backends (GLSL-OpenGL, GLSL-Vulkan, SPIR-V, HLSL-DX11, HLSL-DX12, MSL Metal,
// MSL via SPIRV-Cross). Sans GPU : on compile et on inspecte le code généré.
//
// Répond à : « NkSL fait-il déjà la conversion du compute, y compris vers Metal ? »
// =============================================================================
#include "NKSL/Compiler/NkSLCompiler.h"

#include <cstdio>
#include <cstdlib>

using namespace nkentseu;

// --- Shader compute NkSL : addition vectorielle C[i] = A[i] + B[i] -----------
// Storage buffers (SSBO), push constant (count), workgroup 64, builtin invocation.
static const char *kVecAdd = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) buffer BufC { float data[]; } C;

@push_constant
uniform Params { uint count; } pc;

layout(local_size_x = 64) in;

@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < pc.count) {
        C.data[i] = A.data[i] + B.data[i];
    }
}
)NKSL";

// --- Shader compute NkSL : matmul naïf (le kernel qui servira à NKTensor) -----
static const char *kMatmul = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) buffer BufC { float data[]; } C;

@push_constant
uniform Params { uint M; uint K; uint N; } pc;

layout(local_size_x = 16, local_size_y = 16) in;

@stage(compute)
@entry
void main() {
    uint row = gl_GlobalInvocationID.y;
    uint col = gl_GlobalInvocationID.x;
    if (row < pc.M && col < pc.N) {
        float acc = 0.0;
        for (uint k = 0u; k < pc.K; k = k + 1u) {
            acc = acc + A.data[row * pc.K + k] * B.data[k * pc.N + col];
        }
        C.data[row * pc.N + col] = acc;
    }
}
)NKSL";

// --- Vertex shader NkSL minimal ---------------------------------------------
static const char *kVS = R"NKSL(
@location(0) in vec3 aPos;
@location(1) in vec3 aNormal;
@location(0) out vec3 vNormal;

@stage(vertex)
@entry
void main() {
    vNormal = aNormal;
    gl_Position = vec4(aPos, 1.0);
}
)NKSL";

// --- Fragment shader NkSL minimal -------------------------------------------
static const char *kFS = R"NKSL(
@location(0) in vec3 vNormal;
@location(0) out vec4 fragColor;

@stage(fragment)
@entry
void main() {
    vec3 N = normalize(vNormal);
    float d = max(dot(N, vec3(0.4, 0.8, 0.5)), 0.0);
    fragColor = vec4(vec3(0.2) + vec3(0.8) * d, 1.0);
}
)NKSL";

// -----------------------------------------------------------------------------
// FP16 (mixed precision, 2026-07-25) — historique : à l'origine NkSL n'avait AUCUN
// type half/float16_t natif ici. C'est désormais livré (cf plus bas, section
// « FP16 NATIF » + `NkSLBaseType::NK_HALF`, additif pur) : mot-clé `half`,
// constructeur explicite `half(x)`/`float(x)`, codegen GLSL/GLSL-Vulkan
// (`float16_t` + extension), SPIR-V (capacité Float16 via glslang), HLSL/MSL
// (`half` natif). Le chemin PACKED ci-dessous reste documenté et testé tel quel
// (stockage seul, calcul en float après dépack) — utile pour la bande passante
// mémoire même quand le calcul natif half est disponible.
//
// Ce qui reste exploitable via le chemin STOCKAGE (indépendant du type half) : le
// stockage PACKED f16 via les builtins `packHalf2x16`/`unpackHalf2x16` (déclarés
// dans NkSLSymbolTable.cpp, typecheck OK — vec2<->uint). Ces deux noms sont de
// VRAIS builtins natifs GLSL 4.20+/GLSL-Vulkan 4.50+ (donc valides pour GLSL,
// GLSL-Vulkan, ET SPIR-V puisque ce dernier passe par le VRAI glslang -> preuve
// de compilation réelle, pas juste texte). En revanche AUCUN mapping n'existe
// vers HLSL (le vrai équivalent s'appelle f32tof16/f16tof32, signature différente)
// ni vers MSL (type half natif, pas de pack/unpack par ce nom) : sur ces cibles,
// NkSLCompiler ÉMET du texte (il ne fait QUE de la génération de texte pour ces
// backends dans ce pipeline offline, aucun compilateur externe fxc/dxc/metal
// n'est invoqué) mais ce texte n'est PAS un HLSL/MSL valide si on le compilait
// réellement. La fonction ci-dessous distingue explicitement les deux garanties.
// -----------------------------------------------------------------------------
static const char *kFp16PackedAdd = R"NKSL(
@binding(set=0, binding=0) buffer BufA { uint data[]; } A;
@binding(set=0, binding=1) buffer BufB { uint data[]; } B;
@binding(set=0, binding=2) buffer BufC { uint data[]; } C;

@push_constant
uniform Params { uint count; } pc; // count = nombre de PAIRES f16 (1 uint = 2 halves)

layout(local_size_x = 64) in;

@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < pc.count) {
        vec2 a = unpackHalf2x16(A.data[i]);
        vec2 b = unpackHalf2x16(B.data[i]);
        C.data[i] = packHalf2x16(a + b);
    }
}
)NKSL";

// Pas d'Adam avec PARAM/GRAD stockés PACKED f16 (bande passante mémoire divisée
// par 2) mais moments m/v (état maître, cf pattern mixed-precision standard) et
// arithmétique EN FLOAT — seul le stockage est demi-précision ici (PAS un calcul
// natif f16 : NkSL n'a pas le type pour ça, cf remarque ci-dessus).
static const char *kFp16PackedAdam = R"NKSL(
@binding(set=0, binding=0) buffer BufP { uint data[]; } P;   // param, packé f16 (2/uint)
@binding(set=0, binding=1) buffer BufG { uint data[]; } G;   // grad,  packé f16 (2/uint)
@binding(set=0, binding=2) buffer BufM { float data[]; } M;  // 1er moment, F32 MAÎTRE
@binding(set=0, binding=3) buffer BufV { float data[]; } V;  // 2e moment,  F32 MAÎTRE
@binding(set=0, binding=4) uniform Params {
    uint pairCount; float lr; float b1; float b2; float eps; float b1t; float b2t;
} pc;

layout(local_size_x = 64) in;

@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < pc.pairCount) {
        vec2 p = unpackHalf2x16(P.data[i]);
        vec2 g = unpackHalf2x16(G.data[i]);
        uint m0 = 2u * i, m1 = 2u * i + 1u;
        float m0v = pc.b1 * M.data[m0] + (1.0 - pc.b1) * g.x;
        float m1v = pc.b1 * M.data[m1] + (1.0 - pc.b1) * g.y;
        float v0v = pc.b2 * V.data[m0] + (1.0 - pc.b2) * g.x * g.x;
        float v1v = pc.b2 * V.data[m1] + (1.0 - pc.b2) * g.y * g.y;
        M.data[m0] = m0v; M.data[m1] = m1v;
        V.data[m0] = v0v; V.data[m1] = v1v;
        float p0 = p.x - pc.lr * (m0v / pc.b1t) / (sqrt(v0v / pc.b2t) + pc.eps);
        float p1 = p.y - pc.lr * (m1v / pc.b1t) / (sqrt(v1v / pc.b2t) + pc.eps);
        P.data[i] = packHalf2x16(vec2(p0, p1));
    }
}
)NKSL";

// -----------------------------------------------------------------------------
// FP16 NATIF (2026-07-25, suite) — le préalable ci-dessus est maintenant livré :
// NkSL a un vrai type `half` (NkSLBaseType::NK_HALF, additif pur, cf NkSLTypes.h)
// reconnu par le lexer/parser (mot-clé `half`), typé par la sémantique (constructeur
// explicite `half(x)`/`float(x)`, cf kConstructors dans NkSLSemantic.cpp — PAS de
// conversion implicite float<->half, comme le vrai GLSL float16_t), et généré par
// les 5 backends texte : GLSL/GLSL-Vulkan -> `float16_t` (+ #extension
// GL_EXT_shader_explicit_arithmetic_types_float16 injectée seulement si utilisée),
// HLSL-DX11/DX12 -> `half` (mot-clé HLSL natif), MSL -> `half` (mot-clé MSL natif).
// Le kernel ci-dessous calcule RÉELLEMENT en demi-précision (contrairement au
// pack/unpack ci-dessus qui ne fait QUE du stockage f16, calcul en float) :
// lit deux floats, les convertit en half, additionne EN half, reconvertit en float.
// -----------------------------------------------------------------------------
static const char *kHalfNativeAdd = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) buffer BufC { float data[]; } C;

@push_constant
uniform Params { uint count; } pc;

layout(local_size_x = 64) in;

@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < pc.count) {
        half ha = half(A.data[i]);
        half hb = half(B.data[i]);
        half hc = ha + hb;
        C.data[i] = float(hc);
    }
}
)NKSL";

// Vérifie le type `half` natif sur les 5 backends visés par la mission (GLSL,
// GLSL-Vulkan, SPIR-V, HLSL, MSL). SPIR-V passe par le VRAI glslang embarqué
// (NkGLSLToSPIRV -> NKGLSlang) : succès = preuve de compilation réelle, pas
// seulement de génération de texte. GLSL/GLSL-Vulkan/HLSL/MSL : validation
// textuelle honnête (présence des bons tokens, absence de résidu croisé) —
// aucun compilateur HLSL/MSL n'est embarqué dans ce pipeline offline.
static void CheckNativeHalf(NkSLCompiler &c, const char *kernelName, const char *src) {
	printf("\n[FP16-NATIF] %s\n", kernelName);
	const NkSLTarget targets3[] = {NkSLTarget::NK_GLSL, NkSLTarget::NK_GLSL_VULKAN, NkSLTarget::NK_SPIRV,
									NkSLTarget::NK_HLSL_DX11, NkSLTarget::NK_HLSL_DX12, NkSLTarget::NK_MSL};
	for (NkSLTarget t : targets3) {
		const char *tn = NkSLTargetName(t);
		printf("  ... %-16s : ", tn);
		fflush(stdout);
		NkSLCompileResult r = c.Compile(NkString(src), NkSLStage::NK_COMPUTE, t);
		if (!r.success) {
			printf("FAIL\n");
			for (uint32 i = 0; i < r.errors.Size() && i < 5; i++)
				printf("         ligne %u: %s\n", r.errors[i].line, r.errors[i].message.CStr());
			continue;
		}
		if (t == NkSLTarget::NK_SPIRV) {
			printf("OK, %u mots SPIR-V (VALIDÉ par le vrai glslang embarqué -> `half` natif réellement "
				   "compilable, capacité Float16 émise par glslang lui-même)\n",
				   (unsigned)(r.bytecode.Size() / 4));
		} else if (t == NkSLTarget::NK_GLSL || t == NkSLTarget::NK_GLSL_VULKAN) {
			bool hasType = r.source.Contains("float16_t");
			bool hasExt = r.source.Contains("GL_EXT_shader_explicit_arithmetic_types_float16");
			printf("OK, %u octets (TEXTE : float16_t present=%s, #extension presente=%s)\n",
				   (unsigned)r.source.Size(), hasType ? "oui" : "NON", hasExt ? "oui" : "NON");
		} else {
			bool hasHalf = r.source.Contains("half ha") || r.source.Contains("half hb") || r.source.Contains("half hc");
			bool residualF16 = r.source.Contains("float16_t");
			printf("OK, %u octets (TEXTE SEUL, pas de compilateur %s embarqué ici : `half` present=%s, "
				   "residu float16_t=%s)\n",
				   (unsigned)r.source.Size(), tn, hasHalf ? "oui" : "NON", residualF16 ? "OUI(bug)" : "non");
		}
	}
}

static void CheckFp16(NkSLCompiler &c, const char *kernelName, const char *src) {
	printf("\n[FP16-PACKED] %s\n", kernelName);
	const NkSLTarget targets2[] = {
		NkSLTarget::NK_GLSL, NkSLTarget::NK_GLSL_VULKAN, NkSLTarget::NK_SPIRV, NkSLTarget::NK_HLSL_DX11,
		NkSLTarget::NK_HLSL_DX12, NkSLTarget::NK_MSL, NkSLTarget::NK_MSL_SPIRV_CROSS};
	for (NkSLTarget t : targets2) {
		const char *tn = NkSLTargetName(t);
		printf("  ... %-16s : ", tn);
		fflush(stdout);
		NkSLCompileResult r = c.Compile(NkString(src), NkSLStage::NK_COMPUTE, t);
		const bool realNative = (t == NkSLTarget::NK_GLSL || t == NkSLTarget::NK_GLSL_VULKAN ||
								 t == NkSLTarget::NK_SPIRV); // packHalf2x16/unpackHalf2x16 EXISTENT vraiment ici
		if (r.success) {
			if (t == NkSLTarget::NK_SPIRV)
				printf("OK, %u mots SPIR-V (VALIDÉ par glslang -> half packing réellement compilable)\n",
					   (unsigned)(r.bytecode.Size() / 4));
			else if (realNative)
				printf("OK, %u octets (builtin natif de CETTE cible -> texte réellement valide)\n",
					   (unsigned)r.source.Size());
			else
				printf("OK (TEXTE SEUL, %u octets) -- ATTENTION : packHalf2x16 n'existe PAS en %s "
					   "(vrai équivalent : f32tof16/HLSL ou half/MSL) ; ce texte ne compilerait PAS "
					   "avec un vrai compilateur %s\n",
					   (unsigned)r.source.Size(), tn, tn);
		} else {
			printf("FAIL\n");
			for (uint32 i = 0; i < r.errors.Size() && i < 3; i++)
				printf("         ligne %u: %s\n", r.errors[i].line, r.errors[i].message.CStr());
		}
	}
}

static int g_ok = 0, g_fail = 0;

static void Convert(NkSLCompiler &c, const char *shaderName, const char *src, NkSLStage stage, NkSLTarget t,
					bool showSnippet) {
	(void)showSnippet;
	const char *tn = NkSLTargetName(t);
	printf("  ... %-14s %-16s : ", shaderName, tn);
	fflush(stdout);
	NkSLCompileResult r = c.Compile(NkString(src), stage, t);
	if (r.success) {
		++g_ok;
		if (t == NkSLTarget::NK_SPIRV) {
			const unsigned bin = (unsigned)r.bytecode.Size();
			printf("OK (SPIR-V binaire : %u mots)\n", bin / 4);
		} else {
			printf("OK (%u octets de code généré)\n", (unsigned)r.source.Size());
		}
		fflush(stdout);
	} else {
		++g_fail;
		printf("FAIL\n");
		for (uint32 i = 0; i < r.errors.Size() && i < 3; i++)
			printf("         ligne %u: %s\n", r.errors[i].line, r.errors[i].message.CStr());
		fflush(stdout);
	}
}

int main(int argc, char **argv) {
	const NkSLTarget targets[] = {
		NkSLTarget::NK_GLSL,		   // 0 OpenGL compute
		NkSLTarget::NK_GLSL_VULKAN,	   // 1 Vulkan compute (GLSL)
		NkSLTarget::NK_SPIRV,		   // 2 Vulkan compute (SPIR-V binaire)
		NkSLTarget::NK_HLSL_DX11,	   // 3 DX11 compute (CS 5.0)
		NkSLTarget::NK_HLSL_DX12,	   // 4 DX12 compute (SM6+)
		NkSLTarget::NK_MSL,			   // 5 Metal compute (kernel, natif)
		NkSLTarget::NK_MSL_SPIRV_CROSS // 6 Metal compute (via SPIRV-Cross)
	};
	const int NT = (int)(sizeof(targets) / sizeof(targets[0]));

	NkSLCompiler c;

	// Mode dump : argv[1]="dump" -> affiche le MSL généré du VS et du FS (preuve).
	if (argc > 1 && NkString(argv[1]) == NkString("dump")) {
		NkSLCompileResult vs = c.Compile(NkString(kVS), NkSLStage::NK_VERTEX, NkSLTarget::NK_MSL);
		NkSLCompileResult fs = c.Compile(NkString(kFS), NkSLStage::NK_FRAGMENT, NkSLTarget::NK_MSL);
		NkSLCompileResult cs = c.Compile(NkString(kMatmul), NkSLStage::NK_COMPUTE, NkSLTarget::NK_MSL);
		printf("===== VS -> MSL (Metal) =====\n%s\n", vs.success ? vs.source.CStr() : "ECHEC");
		printf("\n===== FS -> MSL (Metal) =====\n%s\n", fs.success ? fs.source.CStr() : "ECHEC");
		printf("\n===== Matmul (compute) -> MSL (Metal) =====\n%s\n", cs.success ? cs.source.CStr() : "ECHEC");
		return (vs.success && fs.success && cs.success) ? 0 : 1;
	}

	// Mode isolé : argv[1] = index de cible (un process par cible).
	if (argc > 1) {
		int idx = atoi(argv[1]);
		if (idx < 0 || idx >= NT) {
			printf("index invalide\n");
			return 2;
		}
		NkSLTarget t = targets[idx];
		printf("[cible %d = %s]\n", idx, NkSLTargetName(t));
		Convert(c, "VecAdd(cs)", kVecAdd, NkSLStage::NK_COMPUTE, t, false);
		Convert(c, "Matmul(cs)", kMatmul, NkSLStage::NK_COMPUTE, t, false);
		Convert(c, "VS(vertex)", kVS, NkSLStage::NK_VERTEX, t, false);
		Convert(c, "FS(frag)", kFS, NkSLStage::NK_FRAGMENT, t, false);
		return g_fail;
	}

	printf("=== NkSLComputeCheck — conversion NkSL (compute + VS + FS) vers tous backends ===\n\n");
	printf("[COMPUTE] VecAdd\n");
	for (NkSLTarget t : targets)
		Convert(c, "VecAdd(cs)", kVecAdd, NkSLStage::NK_COMPUTE, t, false);
	printf("\n[COMPUTE] Matmul\n");
	for (NkSLTarget t : targets)
		Convert(c, "Matmul(cs)", kMatmul, NkSLStage::NK_COMPUTE, t, false);
	printf("\n[VERTEX] VS\n");
	for (NkSLTarget t : targets)
		Convert(c, "VS(vertex)", kVS, NkSLStage::NK_VERTEX, t, false);
	printf("\n[FRAGMENT] FS\n");
	for (NkSLTarget t : targets)
		Convert(c, "FS(frag)", kFS, NkSLStage::NK_FRAGMENT, t, false);
	printf("\n=== Résultat : %d OK, %d échec(s) ===\n", g_ok, g_fail);

	printf("\n\n=== FP16 (mixed precision) — stockage PACKED via packHalf2x16/unpackHalf2x16 ===\n");
	printf("(Chemin STOCKAGE historique (packing manuel 2xf16 par uint, calcul en float après\n");
	printf(" dépack) — conservé tel quel, non régressé. Depuis le type `half` natif livré plus\n");
	printf(" bas, il existe désormais AUSSI un chemin CALCUL réel en demi-précision (cf section\n");
	printf(" FP16 NATIF ci-dessous) ; celui-ci reste utile pour la bande passante mémoire seule.\n");
	printf(" Packing réellement natif/validé sur GLSL, GLSL-Vulkan, SPIR-V ; texte SEULEMENT --\n");
	printf(" non compilable tel quel -- sur HLSL-DX11/DX12 et MSL, cf détail ci-dessous.)\n");
	CheckFp16(c, "fp16_packed_add (C = A+B, storage packé)", kFp16PackedAdd);
	CheckFp16(c, "fp16_packed_adam (param/grad packés f16, moments F32 maîtres)", kFp16PackedAdam);
	printf("\n=== FIN FP16 (packé) ===\n");

	printf("\n\n=== FP16 NATIF — type `half` (nouveau, additif) sur les 5 backends ===\n");
	CheckNativeHalf(c, "half_native_add (C = float(half(A)+half(B)), calcul REEL en half)", kHalfNativeAdd);
	printf("\n=== FIN FP16 NATIF ===\n");

	return g_fail;
}
