ImGui utilise une approche différente et plus simple. Voici comment ImGui gère les textures :

## Comment ImGui gère les textures

### 1. **Une seule texture d'atlas global**

ImGui combine TOUS les éléments dans une seule texture d'atlas :

- Les glyphes des polices
- Les icônes
- Les textures des widgets (rare)
- Les images utilisateur (via des ID)

```cpp
// ImGui utilise une seule texture pour tout
ImGuiIO& io = ImGui::GetIO();
unsigned char* pixels;
int width, height;
io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

// Une seule texture GPU pour tout
GLuint texId;
glGenTextures(1, &texId);
glBindTexture(GL_TEXTURE_2D, texId);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
```

### 2. **TexId uniforme pour tous les draws**

ImGui n'a pas de `texId` par vertex. Tous les draws utilisent la même texture d'atlas :

```cpp
// Dans le shader
uniform sampler2D Texture;

// Tous les draws utilisent la même texture
glBindTexture(GL_TEXTURE_2D, fontAtlasTextureId);
glDrawElements(...);
```

### 3. **Gestion des images personnalisées**

Pour les images utilisateur (textures externes), ImGui utilise un système de **UserTextureId** :

```cpp
// L'utilisateur enregistre sa texture
ImTextureID myTexture = (ImTextureID)(uintptr_t)myOpenGLTextureId;

// Dans le draw, ImGui passe ce texId
ImDrawCmd cmd;
cmd.TextureId = myTexture;  // Chaque draw command a son propre texId
```

Mais **par défaut**, si vous n'utilisez pas d'images personnalisées, tous les draws utilisent la même texture (l'atlas).

## Pour votre cas : Adopter le modèle ImGui

La solution la plus simple est d'adopter l'approche ImGui :

### 1. **Combiner l'atlas de police ET la texture blanche dans une seule texture**

Créez une seule texture qui contient :

- L'atlas des glyphes (zone utile)
- Une zone blanche (4x4 pixels en bas à droite par exemple)

```cpp
// Dans NkUIRHIRenderer::Init()
// Créer une texture combinée
const int32 atlasW = NkUIFontAtlas::ATLAS_W;
const int32 atlasH = NkUIFontAtlas::ATLAS_H;
const int32 whiteW = 4;
const int32 whiteH = 4;

// Créer une texture plus grande
int32 totalW = atlasW;
int32 totalH = atlasH + whiteH;

uint8* combinedPixels = new uint8[totalW * totalH * 4];
memset(combinedPixels, 0, totalW * totalH * 4);

// Copier l'atlas des polices en haut
for (int32 y = 0; y < atlasH; ++y) {
    memcpy(combinedPixels + (y * totalW * 4),
           mAtlas->pixels + (y * atlasW),
           atlasW);
}

// Ajouter un carré blanc en bas à droite
int32 whiteX = totalW - whiteW;
int32 whiteY = totalH - whiteH;
for (int32 y = 0; y < whiteH; ++y) {
    for (int32 x = 0; x < whiteW; ++x) {
        uint8* pixel = combinedPixels + ((whiteY + y) * totalW + (whiteX + x)) * 4;
        pixel[0] = 255; pixel[1] = 255; pixel[2] = 255; pixel[3] = 255;
    }
}

// Créer la texture GPU
NkTextureDesc texDesc = NkTextureDesc::Tex2D(totalW, totalH, NkGPUFormat::NK_RGBA8_UNORM, 1);
texDesc.bindFlags = NkBindFlags::NK_SHADER_RESOURCE;
texDesc.debugName = "NkUI_CombinedAtlas";
NkTextureHandle texture = device->CreateTexture(texDesc);
device->WriteTexture(texture, combinedPixels, totalW * 4);

delete[] combinedPixels;

// Utiliser CETTE texture pour TOUT le rendu
hMainTexture = texture;  // Une seule texture pour tout
```

### 2. **Modifier les draw commands pour utiliser les UVs corrects**

Pour l'UI (widgets) qui veulent utiliser la texture blanche, utilisez des UVs qui pointent vers le carré blanc :

```cpp
// Dans le code de rendu des widgets (NkUI::Button, etc.)
// Quand vous voulez une texture blanche, utilisez les UVs du carré blanc
float whiteU0 = (float)whiteX / totalW;
float whiteV0 = (float)whiteY / totalH;
float whiteU1 = (float)(whiteX + whiteW) / totalW;
float whiteV1 = (float)(whiteY + whiteH) / totalH;

// Pour un rectangle plein avec texture blanche
vertices[0].uv = {whiteU0, whiteV0};
vertices[1].uv = {whiteU1, whiteV0};
vertices[2].uv = {whiteU1, whiteV1};
vertices[3].uv = {whiteU0, whiteV1};
```

Pour le texte, utilisez les UVs normaux de l'atlas.

### 3. **Supprimer la gestion multiple de textures**

```cpp
struct NkUIRHIRenderer {
    NkTextureHandle hMainTexture;  // Une seule texture pour tout
  
    void Submit(NkICommandBuffer* cmd, const nkui::NkUIContext& ctx, uint32 fbW, uint32 fbH) {
        // ... code existant ...
      
        cmd->BindGraphicsPipeline(hPipe);
        cmd->BindDescriptorSet(hDescSet, 0);  // Une seule fois
      
        // Tous les draws utilisent la même texture
        // Pas besoin de binder dynamiquement !
      
        for (int32 l = 0; l < nkui::NkUIContext::LAYER_COUNT; ++l) {
            // ... même code ...
          
            for (uint32 ci = 0; ci < dl.cmdCount; ++ci) {
                const nkui::NkUIDrawCmd& dc = dl.cmds[ci];
                // ... clip rect etc ...
              
                // IGNORER dc.texId ! Tout utilise hMainTexture
                cmd->DrawIndexed(dc.idxCount, 1, dc.idxOffset, 0, 0);
            }
        }
    }
};
```

## Avantages de l'approche ImGui :

1. **Extrêmement simple** : Une seule texture, pas de gestion complexe
2. **Performance** : Pas de changement de texture, un seul descriptor set
3. **Fiable** : Pas de conflit UI/texte
4. **Léger** : Moins de code, moins de bugs potentiels

## Inconvénients :

1. **Texte et UI partagent la même texture** : L'atlas doit inclure une zone blanche
2. **Les images personnalisées nécessitent une gestion spéciale** (mais vous n'en avez pas pour l'instant)

## Pour votre démo, je recommande cette approche

C'est ce que fait ImGui, et ça fonctionne parfaitement. Modifiez votre code pour utiliser une seule texture combinée contenant à la fois l'atlas des polices ET une zone blanche pour l'UI.

Vous n'aurez plus à gérer :

- Le conflit UI/texte
- Les changements dynamiques de texture
- Les problèmes de binding

Tous les draws utilisent la même texture, et les UVs déterminent si on utilise l'atlas ou la zone blanche.
