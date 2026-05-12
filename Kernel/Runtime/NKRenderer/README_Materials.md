# NKRenderer — Système de Matériaux

Guide complet pour créer des matériaux, paramétrer des propriétés PBR/NPR, et écrire des shaders personnalisés.

---

## 1. Utiliser l'API NkMaterial

### 1.1 Inclure les headers

```cpp
#include "NKRenderer/Materials/NkMaterial.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
```

Ou via le header facade si tu utilises Sandbox :

```cpp
#include "NkRenderer.h"   // inclut déjà NkMaterial.h
```

### 1.2 Créer un matériau

```cpp
// Récupère le système de matériaux depuis le renderer
NkMaterialSystem* matSys = renderer->GetMaterials();

// Crée une instance d'un type prédéfini
NkMaterial* mat = NkMaterial::Create(matSys, NkMaterialType::NK_PBR_METALLIC);

// Par nom de template (si tu as enregistré un template custom)
NkMaterial* mat2 = NkMaterial::Create(matSys, "MyCustomMaterial");
```

### 1.3 Paramétrer les propriétés

**PBR Metallic / PBR Specular**

```cpp
mat->SetAlbedo({1.0f, 0.78f, 0.20f})   // couleur de base (RGB)
   ->SetMetallic(1.0f)                   // 0=diélectrique, 1=métal pur
   ->SetRoughness(0.15f)                 // 0=miroir poli, 1=diffus total
   ->SetEmissive({1.f, 0.3f, 0.f}, 2.f) // couleur + intensité émissive
   ->SetClearcoat(0.5f, 0.05f)           // coat intensity + roughness
   ->SetSubsurface(0.3f, {0.9f, 0.5f, 0.3f}); // SSS factor + couleur
```

**Toon / Anime (NPR)**

```cpp
mat->SetAlbedo({0.2f, 0.4f, 0.9f})
   ->SetToonThreshold(0.35f)            // seuil ombre/lumière (0..1)
   ->SetToonSmooth(0.05f)               // lissage de la frontière
   ->SetToonShadow({0.05f, 0.1f, 0.2f}) // couleur de l'ombre
   ->SetOutline(2.0f, {0.f, 0.f, 0.f}) // épaisseur (px) + couleur outline
   ->SetRim(0.6f, {0.9f, 0.95f, 1.f}); // rim intensity + couleur (Anime)
```

**Unlit**

```cpp
mat->SetAlbedo({0.5f, 0.8f, 1.f});   // affiché tel quel, pas d'éclairage
```

**Textures**

```cpp
NkTexHandle albedoTex = texLib->Load("textures/my_diffuse.png", opts);
mat->SetAlbedoMap(albedoTex)
   ->SetNormalMap(normalTex, 1.0f)  // 2ème arg = force du normal map
   ->SetORMMap(ormTex)              // ORM = Occlusion/Roughness/Metallic packed
   ->SetEmissiveMap(emissiveTex)
   ->SetAOMap(aoTex);
```

**Paramètres génériques** (pour shaders custom)

```cpp
mat->SetFloat("my_param", 0.75f)
   ->SetVec3("my_color", {1.f, 0.5f, 0.f})
   ->SetBool("my_flag", true);
```

### 1.4 Attacher le matériau à un draw call

```cpp
NkDrawCall3D dc;
dc.mesh      = meshHandle;
dc.transform = NkMat4f::Identity();
dc.material  = mat->GetInstHandle();  // passe le handle au draw call
r3d->Submit(dc);
```

### 1.5 Modifier les propriétés en temps réel

Toutes les méthodes `Set*` peuvent être appelées n'importe quand, y compris pendant le rendu. Le matériau est marqué `dirty` et le GPU UBO sera mis à jour au prochain `BindInstance` (automatique lors du flush) :

```cpp
// Pendant la boucle principale :
mat->SetRoughness(newRoughness)->SetAlbedo(newColor);
```

### 1.6 Détruire un matériau

```cpp
NkMaterial::Destroy(mat);   // libère l'instance GPU et met mat à nullptr
```

---

## 2. Types de matériaux disponibles

| Enum | Description | Shader fichier |
|------|-------------|----------------|
| `NK_PBR_METALLIC` | PBR métal/diélectrique (GGX, IBL, shadows) | `PBR/VK/pbr.{vert,frag}.vk.glsl` |
| `NK_PBR_SPECULAR` | Idem, workflow specular/glossiness | même shader |
| `NK_TOON` | Cel-shading, outline, shadow threshold | `Toon/VK/toon.{vert,frag}.vk.glsl` |
| `NK_ANIME` | Toon + rim light + outline coloré | `Anime/VK/anime.{vert,frag}.vk.glsl` |
| `NK_UNLIT` | Couleur pure, pas d'éclairage | `Unlit/VK/unlit.{vert,frag}.vk.glsl` |
| `NK_SKIN` | Subsurface scattering pour la peau | `Skin/VK/skin.{vert,frag}.vk.glsl` |
| `NK_HAIR` | Alpha test + anisotropie | `Hair/VK/hair.{vert,frag}.vk.glsl` |
| `NK_WIREFRAME_MAT` | Wireframe | réutilise PBR, fillMode=WIREFRAME |
| `NK_ARCHIVIZ` | Variante PBR pour l'architecture | réutilise PBR |

---

## 3. Créer un shader personnalisé

### 3.1 Convention de nommage et emplacement

Tous les shaders sources sont en **GLSL Vulkan** (fichiers `.vk.glsl`). Le renderer les convertit automatiquement pour OpenGL, DX11, DX12 et MSL.

#### Option A — Fichier disque (chemin relatif standard)

Structure attendue par défaut (relatif au répertoire de travail à l'exécution) :

```
Resources/NKRenderer/Shaders/
    <NomDuShader>/
        VK/
            <nomdushader>.vert.vk.glsl   ← vertex shader
            <nomdushader>.frag.vk.glsl   ← fragment shader
```

Le nom du dossier doit correspondre exactement au nom que tu passeras à `RegisterTemplate`. La casse est préservée pour le chemin disque, mais le fichier est cherché en **minuscules** (ex: dossier `MyShader`, fichiers `myshader.vert.vk.glsl`).

#### Option B — Source inline (n'importe quel emplacement)

Si tes shaders sont dans un autre dossier (absolu ou relatif quelconque), lis le fichier toi-même et passe le source via `vertSrcVK` / `fragSrcVK` :

```cpp
#include <fstream>
#include <sstream>

auto readFile = [](const std::string& path) -> std::string {
    std::ifstream f(path); std::stringstream ss; ss << f.rdbuf(); return ss.str();
};

NkMaterialTemplateDesc desc;
desc.type      = NkMaterialType::NK_CUSTOM;
desc.name      = "MonShaderPerso";
// Chemin absolu, relatif, ou empacketé — peu importe.
desc.vertSrcVK = readFile("C:/MonProjet/Shaders/monshader.vert.vk.glsl").c_str();
desc.fragSrcVK = readFile("C:/MonProjet/Shaders/monshader.frag.vk.glsl").c_str();

NkMatHandle tmpl = matSys->RegisterTemplate(desc);
```

C'est la méthode recommandée pour les shaders hors du dossier `Resources/` standard.

### 3.2 Structure d'un vertex shader

```glsl
#version 450

// === Vertex inputs (NkVertex3D) ===
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec2 inUV;
layout(location = 4) in vec2 inUV2;
layout(location = 5) in vec4 inColor;

// === Set 0 : Global (caméra, lumières) ===
layout(set = 0, binding = 0) uniform CameraUBO {
    mat4  view;
    mat4  proj;
    vec4  camPos;
    vec4  camDir;
    float viewportX, viewportY;
    float time, deltaTime;
    float iblStrength;
} uCam;

// === Set 1 : Per-objet (transform) ===
layout(set = 1, binding = 0) uniform ObjectUBO {
    mat4  model;
    mat4  normalMatrix;
    vec4  tint;
    float metallic;
    float roughness;
    float aoStrength;
    float emissiveStrength;
    float normalStrength;
    float clearcoat;
    float clearcoatRough;
    float subsurface;
    vec4  subsurfaceColor;
} uObj;

// === Outputs vers le fragment ===
layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec2 vUV;
layout(location = 3) out vec4 vColor;

void main() {
    vec4 worldPos = uObj.model * vec4(inPos, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal   = normalize(mat3(uObj.normalMatrix) * inNormal);
    vUV       = inUV;
    vColor    = inColor;
    gl_Position = uCam.proj * uCam.view * worldPos;
}
```

### 3.3 Structure d'un fragment shader (matériau simple)

```glsl
#version 450

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vUV;
layout(location = 3) in vec4 vColor;

// === Set 2 : Per-instance (paramètres matériau) ===
layout(set = 2, binding = 1) uniform MaterialUBO {
    vec4  albedo;       // RGB + alpha
    vec4  emissive;     // RGB + unused
    float metallic;
    float roughness;
    float ao;
    float emissiveStrength;
    float normalStrength;
    float clearcoat;
    float clearcoatRough;
    float subsurface;
    vec4  subsurfaceColor;
    float anisotropy;
    float sheen;
    float _pad[2];
} uMat;

// Textures (set 2, bindings 3-6)
layout(set = 2, binding = 3) uniform sampler2D texAlbedo;
layout(set = 2, binding = 4) uniform sampler2D texNormal;
layout(set = 2, binding = 5) uniform sampler2D texORM;
layout(set = 2, binding = 6) uniform sampler2D texEmissive;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 albedoColor = uMat.albedo * texture(texAlbedo, vUV);
    outColor = albedoColor;
}
```

### 3.4 Enregistrer un template custom

```cpp
// Dans ton code C++ d'initialisation
NkMaterialSystem* matSys = renderer->GetMaterials();

NkMaterialTemplateDesc desc;
desc.type     = NkMaterialType::NK_CUSTOM;
desc.name     = "MyShader";
desc.queue    = NkRenderQueue::NK_OPAQUE;
desc.cullMode = NkCullMode::NK_BACK;
// Le fichier sera cherché dans :
// Resources/NKRenderer/Shaders/MyShader/VK/myshader.{vert,frag}.vk.glsl
desc.vertSrcGL = "MyShader";  // hint chemin disque

NkMatHandle tmpl = matSys->RegisterTemplate(desc);

// Créer une instance du template custom
NkMaterial* mat = NkMaterial::Create(matSys, "MyShader");
```

### 3.5 Source inline (sans fichier disque)

Si tu veux embarquer le shader directement dans le code :

```cpp
desc.type      = NkMaterialType::NK_CUSTOM;
desc.name      = "MyInlineShader";
desc.vertSrcVK = R"(
    #version 450
    layout(location=0) in vec3 inPos;
    // ... vertex shader GLSL VK source ...
    void main() { gl_Position = vec4(inPos, 1.0); }
)";
desc.fragSrcVK = R"(
    #version 450
    layout(location=0) out vec4 outColor;
    void main() { outColor = vec4(1.0, 0.5, 0.0, 1.0); }
)";
```

---

## 4. Système de cache des shaders

Les shaders compilés sont mis en cache dans `cache/shaders/` (relatif à l'exécutable) :

- **Clé** : hash FNV-1a de la source GLSL
- **Format** : `.nksc` (NKShader Compiled)
- **Invalidation** : automatique si la source change (nouveau hash = nouvelle entrée)

Au **premier démarrage** : compilation depuis disque (~0.5s pour tous les shaders PBR+NPR).  
Au **démarrage suivant** : chargement cache (~0.05s).

Si tu modifies un fichier `.vk.glsl`, le cache de ce shader est invalidé automatiquement (le hash change).

---

## 5. Paramètres set=2 — Layout du MaterialUBO

Le MaterialUBO (binding=1 du set=2) suit le layout `std140` :

| Offset | Type | Champ | Fonction |
|--------|------|-------|----------|
| 0 | vec4 | albedo | Couleur de base (RGBA) |
| 16 | vec4 | emissive | Couleur émissive (RGB) |
| 32 | float | metallic | 0=plastique, 1=métal |
| 36 | float | roughness | 0=poli, 1=mat |
| 40 | float | ao | Occlusion ambiante (1=normal) |
| 44 | float | emissiveStrength | Intensité émissive |
| 48 | float | normalStrength | Force du normal map |
| 52 | float | clearcoat | Coat de vernis (0..1) |
| 56 | float | clearcoatRough | Rugosité du coat |
| 60 | float | subsurface | SSS factor (0..1) |
| 64 | vec4 | subsurfaceColor | Couleur SSS |
| 80 | float | anisotropy | Anisotropie (0..1) |
| 84 | float | sheen | Sheen (tissus) |
| 88 | float[2] | _pad | padding std140 |

Total : 96 bytes (aligné 16).

---

## 6. Exemple complet — Demo4_Materials

Voir `Applications/Sandbox/src/Demo/Demo4_Materials.cpp` pour un exemple fonctionnel avec :
- 5 sphères, chacune avec un type de matériau différent
- Modification en temps réel (roughness, metallic, outline, couleur)
- Contrôles clavier : `1-5` (sélection), `+/-` (roughness/threshold), `M` (metallic), `C` (couleur), `O` (outline)

Lancer avec :
```
renderdemo.exe --demo=3 --backend=opengl
renderdemo.exe --demo=3 --backend=vulkan
```
