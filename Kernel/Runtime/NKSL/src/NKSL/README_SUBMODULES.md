# NkSL — Ajout des sous-modules git

## Dépendances embarquées

NkSL embarque deux bibliothèques en sous-modules git.
**Aucune dépendance système n'est requise** — tout est compilé depuis les sources.

---

## glslang — Compilateur GLSL → SPIR-V

**Multiplatform** : Windows, Linux, macOS, Android, iOS
**SPIR-V** : format binaire portable, même résultat sur tous les OS
**Standalone** : aucune dépendance Vulkan SDK

```bash
# Depuis la racine de ton projet (là où est ton .gitmodules)
git submodule add https://github.com/KhronosGroup/glslang third_party/glslang
git submodule update --init --recursive
```

---

## SPIRV-Cross — Convertisseur SPIR-V → MSL / HLSL / GLSL

**Multiplatform** : Windows, Linux, macOS, Android, iOS
**Standalone** : aucune dépendance système
**Utilisé pour** : génération MSL robuste (Metal), cas edge

```bash
git submodule add https://github.com/KhronosGroup/SPIRV-Cross third_party/SPIRV-Cross
git submodule update --init --recursive
```

---

## Configuration CMake

```cmake
# Dans ton CMakeLists.txt racine :
set(NKSL_USE_GLSLANG     ON)   # SPIR-V compiler (recommandé)
set(NKSL_USE_SPIRV_CROSS ON)   # MSL via SPIRV-Cross (recommandé)
set(NKSL_USE_SHADERC     OFF)  # Optionnel: Vulkan SDK shaderc

add_subdirectory(Modules/Runtime/NKRHI/src/NKRHI/SL)
```

---

## Exemple de .gitmodules

```ini
[submodule "third_party/glslang"]
    path = third_party/glslang
    url = https://github.com/KhronosGroup/glslang

[submodule "third_party/SPIRV-Cross"]
    path = third_party/SPIRV-Cross
    url = https://github.com/KhronosGroup/SPIRV-Cross
```

---

## Vérification multiplatform de SPIR-V

SPIR-V est **100% multiplatform** :

- C'est un format binaire (comme LLVM IR), pas du code machine
- glslang le compile depuis GLSL sur **tout OS sans GPU**
- Vulkan le consomme sur Windows/Linux/macOS/Android/iOS
- SPIRV-Cross peut le convertir en MSL/HLSL/GLSL sur n'importe quelle machine

```
GLSL (texte)
    │
glslang (tourne sur tout OS)
    │
SPIR-V (binaire portable)
    ├── Vulkan (consomme directement)
    ├── SPIRV-Cross → MSL (Metal/macOS/iOS)
    └── SPIRV-Cross → HLSL (DX12 optionnel)
```

---

## Résumé des corrections v3.0

### Bug corrigé : `static int sAutoIdx`

**Avant (bugué)** :

```cpp
// Dans SemanticFor() — NkSLCodeGenAdvanced.cpp
static int sAutoIdx = 0;  // jamais remis à zéro !
char buf[32]; snprintf(buf, sizeof(buf), "TEXCOORD%d", sAutoIdx++);
```

**Après (corrigé)** :

```cpp
// SemanticFor() prend maintenant autoIndex en paramètre
NkString SemanticFor(NkSLVarDeclNode* v, NkSLStage stage, bool isInput, int autoIndex);
// Dans GenInputOutputStructs() :
int autoIdx = 0;  // variable locale, réinitialisée à chaque appel
for (auto* v : mInputVars) {
    NkString sem = GetHLSLSemantic(v, mStage, true, false, autoIdx++);
    ...
}
```

Le même bug existait dans `NkSLCodeGenHLSL.cpp` — corrigé partout.

### Nouveautés

1. **SPIRV-Cross MSL** — `NkSLCodeGenMSL_SpirvCross.cpp`
2. **Reflection automatique** — `NkSLReflector.cpp` + `NkSLTypes.h`
3. **`CompileWithReflection()`** — compile + reflection en une passe
4. **`GenerateLayoutCPP()`** — génère le code C++ du descriptor set layout
5. **`GenerateLayoutJSON()`** — génère la description JSON des bindings
6. Jenga — embed glslang + SPIRV-Cross comme sous-modules

### Utilisation de la reflection

```cpp
// Compiler + obtenir la reflection
auto result = NkSL::CreateShaderWithReflection(
    device, kPhongShader,
    { NkSLStage::VERTEX, NkSLStage::FRAGMENT },
    "Phong"
);

if (result.success) {
    // Inspecter les bindings
    for (auto& r : result.reflection.resources) {
        printf("Binding %u: %s (%s)\n",
               r.binding, r.name.CStr(),
               r.kind == NkSLResourceKind::UNIFORM_BUFFER ? "UBO" : "Texture");
    }

    // Inspecter les vertex inputs
    for (auto& vi : result.reflection.vertexInputs) {
        printf("Input location=%u: %s (%u components)\n",
               vi.location, vi.name.CStr(), vi.components);
    }

    // Générer le layout C++ automatiquement
    NkString layoutCode = NkSL::GenerateLayoutCPP(kPhongShader, NkSLStage::VERTEX);
    printf("Generated layout:\n%s\n", layoutCode.CStr());
}
```
