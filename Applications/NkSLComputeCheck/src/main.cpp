// =============================================================================
// NkSLComputeCheck — vérifie que le COMPUTE NkSL se convertit vers TOUS les
// backends (GLSL-OpenGL, GLSL-Vulkan, SPIR-V, HLSL-DX11, HLSL-DX12, MSL Metal,
// MSL via SPIRV-Cross). Sans GPU : on compile et on inspecte le code généré.
//
// Répond à : « NkSL fait-il déjà la conversion du compute, y compris vers Metal ? »
// =============================================================================
#include "NKSL/Compiler/NkSLCompiler.h"
#include "NKSL/Frontend/NkSLLexer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring> // memcmp : comparaison du SPIR-V hexa vs décimal

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

static void Chk(bool cond, const char *what) {
	printf("  [%s] %s\n", cond ? " OK " : "FAIL", what);
	if (cond)
		++g_ok;
	else
		++g_fail;
}

// =============================================================================
// LITTÉRAUX HEXADÉCIMAUX ET BINAIRES (ajout 2026-08-05)
//
// POURQUOI CE TEST EXISTE : les noyaux qui décodent un format quantifié par
// blocs (Q4_K = 144 octets, Q6_K = 210 octets) ne sont qu'une suite de masques
// et de décalages. Tant que le lexer ne garantissait pas `0xFF`, ils devaient
// s'écrire en décimal (255u, 1023u, 2139095040u) : invérifiables à l'œil contre
// la spec ggml. Le jalon 4 QLoRA a payé ce détour, et l'échec correspondant
// remontait tout au bout de la chaîne en « erreur de pipeline » — un message
// qui ne désigne PAS la cause.
//
// Ce que le test prouve, en trois temps :
//   1. VALEURS : le lexer rend bien 0xFF == 255, 0x0F == 15, 0b1010 == 10…
//      (contrôle direct sur les tokens, sans passer par un backend).
//   2. ÉQUIVALENCE : un noyau écrit en hexadécimal/binaire et son JUMEAU écrit
//      en décimal produisent, sur chacune des 7 cibles, un code généré
//      STRICTEMENT IDENTIQUE octet pour octet. C'est la vérification du
//      RÉSULTAT NUMÉRIQUE : si un seul masque différait d'une unité, les deux
//      textes divergeraient. Et le passage par SPIR-V est compilé par le vrai
//      glslang embarqué — donc réellement exécutable, pas seulement du texte.
//   3. DIAGNOSTIC : un littéral fautif doit produire une erreur qui dit QUOI et
//      OÙ (ligne ET colonne), et non un silence qui se paye plus loin.
// =============================================================================

// Le noyau de contrôle : masques (0xFF, 0x0F, 0xF0), décalages (>> 4, << 16),
// littéral binaire, et une constante 32 bits pleine (0x7F800000u = exposant d'un
// float, exactement le genre de motif qui rend le décimal illisible).
static const char *kHexMasks = R"NKSL(
@binding(set=0, binding=0) buffer BufIn  { uint data[]; } I;
@binding(set=0, binding=1) buffer BufOut { uint data[]; } O;
@binding(set=0, binding=2) uniform Params { uint count; } pc;

layout(local_size_x = 64) in;

@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < pc.count) {
        uint w   = I.data[i];
        uint lo  = w & 0xFFu;
        uint nib = (w >> 8u) & 0x0Fu;
        uint hi  = (w & 0xF0u) >> 4u;
        uint bin = w & 0b1010u;
        uint exp = w & 0x7F800000u;
        O.data[i] = lo | (nib << 8u) | (hi << 16u) | bin | exp;
    }
}
)NKSL";

// Le MÊME noyau, masques en décimal : 0xFF=255, 0x0F=15, 0xF0=240, 0b1010=10,
// 0x7F800000=2139095040. Toute divergence de code généré = divergence de valeur.
static const char *kDecMasks = R"NKSL(
@binding(set=0, binding=0) buffer BufIn  { uint data[]; } I;
@binding(set=0, binding=1) buffer BufOut { uint data[]; } O;
@binding(set=0, binding=2) uniform Params { uint count; } pc;

layout(local_size_x = 64) in;

@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < pc.count) {
        uint w   = I.data[i];
        uint lo  = w & 255u;
        uint nib = (w >> 8u) & 15u;
        uint hi  = (w & 240u) >> 4u;
        uint bin = w & 10u;
        uint exp = w & 2139095040u;
        O.data[i] = lo | (nib << 8u) | (hi << 16u) | bin | exp;
    }
}
)NKSL";

// --- 1. Valeurs rendues par le lexer -----------------------------------------
static void CheckLiteralValues() {
	printf("\n[LITTÉRAUX] valeurs rendues par le lexer\n");
	struct Cas {
			const char *src;
			bool isUint;
			uint64 value;
			const char *what;
	};
	const Cas cas[] = {
		{"0xFF", false, 255ull, "0xFF == 255"},
		{"0x0F", false, 15ull, "0x0F == 15"},
		{"0xff", false, 255ull, "0xff (minuscules) == 255"},
		{"0X1F", false, 31ull, "0X1F (préfixe majuscule) == 31"},
		{"0xFFu", true, 255ull, "0xFFu est UNSIGNED et vaut 255"},
		{"0x7F800000u", true, 2139095040ull, "0x7F800000u == 2139095040"},
		{"0b1010", false, 10ull, "0b1010 == 10"},
		{"0b11111111u", true, 255ull, "0b11111111u == 255"},
		{"255u", true, 255ull, "255u (décimal, non régressé) == 255"},
		{"42", false, 42ull, "42 (décimal, non régressé) == 42"},
		// Sans suffixe et au-delà de la plage int : classé UINT, sinon le
		// générateur écrirait « 4294967295 » comme int et GLSL le refuserait.
		{"0xFFFFFFFF", true, 4294967295ull, "0xFFFFFFFF (sans suffixe) devient uint 4294967295"},
	};
	for (const Cas &k : cas) {
		NkSLLexer lex(NkString(k.src));
		NkSLToken t = lex.Next();
		const bool kindOk = k.isUint ? (t.kind == NkSLTokenKind::NK_LIT_UINT) : (t.kind == NkSLTokenKind::NK_LIT_INT);
		const uint64 got = (t.kind == NkSLTokenKind::NK_LIT_UINT) ? t.uintVal : (uint64)t.intVal;
		const bool ok = kindOk && got == k.value && lex.GetErrors().Empty();
		if (!ok)
			printf("       (lu : kind=%u valeur=%llu erreurs=%u)\n", (unsigned)t.kind, (unsigned long long)got,
				   (unsigned)lex.GetErrors().Size());
		Chk(ok, k.what);
	}

	// Le décalage doit rester un opérateur, pas être avalé par le littéral.
	NkSLLexer lex(NkString("0xF0u >> 4u"));
	NkSLToken a = lex.Next(), op = lex.Next(), b = lex.Next();
	Chk(a.kind == NkSLTokenKind::NK_LIT_UINT && a.uintVal == 240ull && op.kind == NkSLTokenKind::NK_OP_RSHIFT &&
			b.kind == NkSLTokenKind::NK_LIT_UINT && b.uintVal == 4ull,
		"« 0xF0u >> 4u » se découpe en 240u, >>, 4u");
}

// --- 2. Équivalence hexadécimal / décimal sur les 7 cibles --------------------
static void CheckHexEquivalence(NkSLCompiler &c, const NkSLTarget *targets, int nt) {
	printf("\n[LITTÉRAUX] noyau hexa/binaire == noyau décimal, code généré identique\n");
	for (int i = 0; i < nt; ++i) {
		const NkSLTarget t = targets[i];
		const char *tn = NkSLTargetName(t);
		NkSLCompileResult h = c.Compile(NkString(kHexMasks), NkSLStage::NK_COMPUTE, t);
		NkSLCompileResult d = c.Compile(NkString(kDecMasks), NkSLStage::NK_COMPUTE, t);
		if (!h.success) {
			printf("  ... %-16s : le noyau HEXA a été REFUSÉ\n", tn);
			for (uint32 e = 0; e < h.errors.Size() && e < 3; ++e)
				printf("         ligne %u col %u : %s\n", h.errors[e].line, h.errors[e].column,
					   h.errors[e].message.CStr());
			Chk(false, "compilation du noyau à masques hexadécimaux");
			continue;
		}
		bool same;
		if (t == NkSLTarget::NK_SPIRV)
			same = d.success && h.bytecode.Size() == d.bytecode.Size() && h.bytecode.Size() > 0 &&
				   memcmp(h.bytecode.Data(), d.bytecode.Data(), h.bytecode.Size()) == 0;
		else
			same = d.success && h.source == d.source;
		printf("  ... %-16s : %s (%u %s)\n", tn, same ? "identique au décimal" : "DIVERGENT",
			   (unsigned)(t == NkSLTarget::NK_SPIRV ? h.bytecode.Size() / 4 : h.source.Size()),
			   t == NkSLTarget::NK_SPIRV ? "mots SPIR-V" : "octets");
		Chk(same, tn);
	}
}

// --- 3. Un littéral fautif doit dire QUOI et OÙ -------------------------------
static void CheckLiteralDiagnostics(NkSLCompiler &c) {
	printf("\n[LITTÉRAUX] messages d'erreur : quoi, et où (ligne/colonne)\n");
	struct Mauvais {
			const char *src;
			const char *what;
	};
	const Mauvais mauvais[] = {
		{"0x", "0x sans chiffre"},
		{"0b", "0b sans chiffre"},
		{"0b12", "chiffre 2 dans un littéral binaire"},
		{"12abc", "identifiant collé à un littéral décimal"},
		{"0xFFFFFFFFFFFFFFFFF", "littéral hexadécimal de plus de 64 bits"},
	};
	for (const Mauvais &m : mauvais) {
		NkSLLexer lex(NkString(m.src));
		(void)lex.Next();
		const bool ok = !lex.GetErrors().Empty() && lex.GetErrors()[0].line > 0 && lex.GetErrors()[0].column > 0;
		if (ok)
			printf("       « %s » -> ligne %u, colonne %u : %s\n", m.src, lex.GetErrors()[0].line,
				   lex.GetErrors()[0].column, lex.GetErrors()[0].message.CStr());
		Chk(ok, m.what);
	}

	// Et de bout en bout : la faute doit remonter jusqu'au RÉSULTAT de
	// compilation, pas mourir dans le lexer. C'est précisément ce qui manquait :
	// les erreurs lexicales n'étaient lues par personne.
	static const char *kBad = R"NKSL(
@binding(set=0, binding=0) buffer BufOut { uint data[]; } O;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    O.data[0] = 0xZZu;
}
)NKSL";
	NkSLCompileResult r = c.Compile(NkString(kBad), NkSLStage::NK_COMPUTE, NkSLTarget::NK_GLSL_VULKAN);
	bool located = false;
	for (uint32 i = 0; i < r.errors.Size(); ++i) {
		printf("       ligne %u col %u : %s\n", r.errors[i].line, r.errors[i].column, r.errors[i].message.CStr());
		if (r.errors[i].line > 0 && r.errors[i].column > 0)
			located = true;
	}
	Chk(!r.success && located, "un littéral fautif fait ÉCHOUER la compilation avec ligne+colonne");
}

// =============================================================================
// MÉMOIRE PARTAGÉE (`shared`) — ajout 2026-08-05
//
// POURQUOI CE TEST EXISTE : le mot-clé `shared`, son token, son qualificateur de
// stockage et son émission dans les trois générateurs existaient DEPUIS TOUJOURS
// dans NkSL — mais le dispatch de haut niveau du parser ne le routait pas, si
// bien que toute déclaration `shared` au niveau global échouait sur « Unexpected
// token at top level ». Écrire un noyau à mémoire partagée était donc
// impossible, et personne ne l'avait vu parce que personne n'en avait eu besoin
// avant le GEMM tuilé du chantier QLoRA.
//
// La leçon justifie le test : une fonctionnalité présente à 95 % dans le code
// est une fonctionnalité ABSENTE. Seul un passage bout-en-bout le prouve — d'où
// une vérification sur chaque cible, et non « le mot-clé est dans le lexer ».
// =============================================================================
static const char *kSharedReduce = R"NKSL(
@binding(set=0, binding=0) buffer BufIn  { float data[]; } I;
@binding(set=0, binding=1) buffer BufOut { float data[]; } O;
@binding(set=0, binding=2) uniform Params { uint count; } pc;

layout(local_size_x = 64) in;

shared float nkTile[64];

@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    uint t = gl_LocalInvocationID.x;
    float v = 0.0;
    if (i < pc.count) {
        v = I.data[i];
    }
    nkTile[t] = v;
    barrier();
    if (i < pc.count) {
        float s = 0.0;
        for (uint k = 0u; k < 64u; k = k + 1u) {
            s = s + nkTile[k];
        }
        O.data[i] = s;
    }
}
)NKSL";

static void CheckSharedMemory(NkSLCompiler &c, const NkSLTarget *targets, int nt) {
	printf("\n[PARTAGÉE] déclaration `shared` + barrier() sur chaque cible\n");
	for (int i = 0; i < nt; ++i) {
		const NkSLTarget t = targets[i];
		const char *tn = NkSLTargetName(t);
		NkSLCompileResult r = c.Compile(NkString(kSharedReduce), NkSLStage::NK_COMPUTE, t);
		if (!r.success) {
			for (uint32 e = 0; e < r.errors.Size() && e < 3; ++e)
				printf("         ligne %u col %u : %s\n", r.errors[e].line, r.errors[e].column,
					   r.errors[e].message.CStr());
			Chk(false, tn);
			continue;
		}
		if (t == NkSLTarget::NK_SPIRV) {
			// Le SPIR-V est produit par le vrai glslang embarqué : s'il sort, la
			// déclaration `shared` et la barrière sont réellement COMPILABLES, pas
			// seulement bien orthographiées.
			printf("  ... %-16s : OK, %u mots SPIR-V (validé par glslang)\n", tn, (unsigned)(r.bytecode.Size() / 4));
			Chk(r.bytecode.Size() > 0, tn);
		} else {
			// Chaque langage a SON mot pour la même chose. Vérifier qu'il est présent
			// écarte le faux positif d'une source qui compile en ayant simplement
			// PERDU le qualificateur : la variable redeviendrait locale à chaque
			// thread et le noyau donnerait un résultat faux sans rien signaler.
			const char *needle = "shared";
			if (t == NkSLTarget::NK_MSL || t == NkSLTarget::NK_MSL_SPIRV_CROSS)
				needle = "threadgroup";
			// HLSL : on exige le mot ENTIER `groupshared`. Chercher « shared » seul
			// laisserait passer un générateur qui écrirait `shared` tel quel (mot-clé
			// inconnu de HLSL) — le sous-mot ne prouve rien, le mot exact si.
			else if (t == NkSLTarget::NK_HLSL_DX11 || t == NkSLTarget::NK_HLSL_DX12)
				needle = "groupshared";
			const bool found = r.source.Find(needle) != NkString::npos;
			printf("  ... %-16s : OK, %u octets (« %s » présent = %s)\n", tn, (unsigned)r.source.Size(), needle,
				   found ? "oui" : "NON");
			Chk(found, tn);
		}
	}
}

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

	printf("\n\n=== LITTÉRAUX HEXADÉCIMAUX / BINAIRES (0x, 0b) ===\n");
	CheckLiteralValues();
	CheckHexEquivalence(c, targets, NT);
	CheckLiteralDiagnostics(c);
	printf("\n=== FIN LITTÉRAUX — total cumulé : %d OK, %d échec(s) ===\n", g_ok, g_fail);

	printf("\n\n=== MÉMOIRE PARTAGÉE (shared + barrier) ===\n");
	CheckSharedMemory(c, targets, NT);
	printf("\n=== FIN MÉMOIRE PARTAGÉE — total cumulé : %d OK, %d échec(s) ===\n", g_ok, g_fail);

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
