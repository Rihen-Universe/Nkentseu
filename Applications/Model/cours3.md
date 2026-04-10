Absolument. Voici un cours complet et structuré sur la création d'applications 2D/3D avec le système que vous avez fourni (le framework Nkentseu).

Ce cours part des bases de l'initialisation du système, passe par la création d'une fenêtre et d'un contexte graphique, puis explore en profondeur les aspects de rendu, de création de ressources (shaders, textures, buffers), d'éclairage, d'ombres, et de techniques avancées comme le rendu hors-écran (offscreen) et l'overlay.

---

### **Cours Complet : Développement d'une Application 2D/3D avec Nkentseu**

#### **Objectifs du cours**

À la fin de ce cours, vous serez capable de :

1. Initialiser et fermer proprement le framework Nkentseu.
2. Créer une fenêtre et gérer ses événements.
3. Initialiser un contexte graphique (OpenGL, Vulkan, etc.) et gérer son cycle de vie.
4. Créer et compiler des shaders (GLSL/HLSL) en utilisant les outils de conversion intégrés.
5. Créer des buffers GPU (vertex, index, uniform) et les remplir de données.
6. Charger et créer des textures 2D et cubemaps.
7. Écrire une boucle de rendu complète, incluant les commandes de dessin.
8. Implémenter un modèle d'éclairage de base (Phong/Blinn-Phong).
9. Gérer des rendus multiples avec un `NkICommandBuffer` et des pipelines.
10. Créer et afficher une scène avec des ombres (shadow mapping).
11. Faire du rendu hors-écran (offscreen) en utilisant des `NkFramebuffer` et `NkRenderPass`.
12. Ajouter un rendu d'interface (overlay) 2D par-dessus la scène 3D.

---

### **Partie 1 : Fondations et Cycle de Vie de l'Application**

Avant toute chose, il faut comprendre le point d'entrée du framework. Il ne s'agit pas d'un `main` classique, mais d'une fonction `nkmain`.

```cpp
// =============================================================================
// 1. Inclusion des en-têtes nécessaires
// =============================================================================
#include "NKWindow/NkWindow.h"
#include "NKWindow/Core/NkMain.h"
#include "NKWindow/Core/NkSystem.h" // Inclut le système d'événements
#include "NKRHI/Core/NkDeviceFactory.h" // Pour créer le device graphique
#include "NKRHI/Core/NkDescs.h" // Pour les descripteurs (Desc)
#include "NKMath/NkMath.h" // Pour les maths (vecteurs, matrices)
#include "NKLogger/NkLog.h" // Pour les logs

using namespace nkentseu;

// =============================================================================
// 2. Point d'entrée principal de l'application
// =============================================================================
int nkmain(const NkEntryState& state) {
    // =========================================
    // Étape 1 : Initialiser le système global
    // =========================================
    NkAppData appData;
    appData.appName = "MonApplication";
    appData.appVersion = "1.0";
    // Vous pouvez activer des options comme le logging des événements.
    // appData.enableEventLogging = true;

    if (!NkInitialise(appData)) {
        logger.Error("Échec de l'initialisation du système Nkentseu.");
        return -1;
    }

    // =========================================
    // Étape 2 : Créer la fenêtre principale
    // =========================================
    NkWindowConfig windowConfig;
    windowConfig.title = "Ma Première Application 2D/3D";
    windowConfig.width = 1280;
    windowConfig.height = 720;
    windowConfig.centered = true;
    windowConfig.resizable = true; // On permettra le redimensionnement

    NkWindow mainWindow;
    if (!mainWindow.Create(windowConfig)) {
        logger.Error("Échec de la création de la fenêtre.");
        NkClose(); // Nettoyer avant de quitter
        return -2;
    }

    logger.Info("Fenêtre créée avec succès !");

    // =========================================
    // Étape 3 : Boucle principale de l'application
    // =========================================
    bool running = true;
    auto& eventSystem = NkEvents(); // Récupère le système d'événements

    while (running) {
        // --- Traitement des événements ---
        // PollEvent() retourne le prochain événement dans la file.
        // On le traite jusqu'à ce qu'il n'y en ait plus.
        while (NkEvent* event = eventSystem.PollEvent()) {
            // On vérifie si c'est un événement de fermeture de fenêtre.
            if (auto* closeEvent = event->As<NkWindowCloseEvent>()) {
                // L'utilisateur a cliqué sur la croix de la fenêtre.
                running = false;
                break; // On sort de la boucle d'événements, la fenêtre va fermer.
            }

            // Ici, on pourrait traiter d'autres événements comme les touches clavier,
            // la souris, le redimensionnement, etc.
        }

        // --- Simulation et Logique du jeu ---
        // (À remplir plus tard)

        // --- Rendu ---
        // (À remplir plus tard)

        // --- Petite pause pour ne pas surcharger le CPU (optionnel) ---
        // NkChrono::Sleep(1);
    }

    // =========================================
    // Étape 4 : Nettoyage et fermeture
    // =========================================
    mainWindow.Close(); // Ferme la fenêtre
    NkClose(); // Nettoie le système global

    logger.Info("Application terminée proprement.");
    return 0;
}
```

**Explications :**

- `nkmain` : Le point d'entrée standard du framework.
- `NkInitialise` : Initialise le système d'événements, le gestionnaire de fenêtres, etc.
- `NkWindow` : La classe représentant une fenêtre OS.
- `NkEvents()` : Récupère le singleton du système d'événements pour traiter les entrées.
- `NkEvent` : Classe de base pour tous les événements (clavier, souris, fenêtre, etc.).
- `NkClose` : Nettoie toutes les ressources allouées par `NkInitialise`.

---

### **Partie 2 : Initialisation du RHI (Rendering Hardware Interface)**

La partie graphique est gérée par l'interface `NkIDevice`. Nous allons utiliser `NkDeviceFactory` pour créer une instance appropriée pour notre plateforme.

```cpp
// À ajouter dans la fonction nkmain, juste après la création de la fenêtre.

// =========================================
// Étape 3b (après la création de la fenêtre) : Créer le Device RHI
// =========================================

// 1. Obtenir la description de la surface de la fenêtre.
//    C'est ici que le framework nous donne les handles natifs (HWND, NSView, etc.)
//    nécessaires pour créer le contexte graphique.
NkSurfaceDesc surfaceDesc = mainWindow.GetSurfaceDesc();
if (!surfaceDesc.IsValid()) {
    logger.Error("La surface de la fenêtre n'est pas valide.");
    mainWindow.Close();
    NkClose();
    return -3;
}

// 2. Définir la configuration de notre contexte graphique.
//    On choisit Vulkan comme API, mais vous pouvez aussi utiliser OpenGL, DirectX, etc.
NkContextDesc contextDesc = NkContextDesc::MakeVulkan(true); // Avec validation layers pour le debug.
// Optionnel : on peut spécifier des préférences GPU (haute performance, basse consommation).
contextDesc.gpu.preference = NkGpuPreference::NK_HIGH_PERFORMANCE;

// 3. Remplir la structure d'initialisation du device.
NkDeviceInitInfo deviceInitInfo;
deviceInitInfo.api = contextDesc.api; // On utilise l'API de la config.
deviceInitInfo.context = contextDesc; // On passe notre configuration.
deviceInitInfo.surface = surfaceDesc; // On passe la description de la surface.
deviceInitInfo.width = mainWindow.GetSize().x;
deviceInitInfo.height = mainWindow.GetSize().y;

// 4. Créer le device via la factory.
NkIDevice* gpuDevice = NkDeviceFactory::Create(deviceInitInfo);
if (!gpuDevice || !gpuDevice->IsValid()) {
    logger.Error("Échec de la création du périphérique graphique.");
    mainWindow.Close();
    NkClose();
    return -4;
}

logger.Info("Périphérique graphique créé avec succès : {}", NkGraphicsApiName(gpuDevice->GetApi()));

// Nous stockerons ce pointeur pour l'utiliser dans la boucle de rendu.
// Idéalement, nous le mettrons dans une classe d'application ou comme variable globale pour ce tutoriel.
// Pour simplifier, on va le déclarer en dehors de la fonction et l'utiliser ici.
// Dans un vrai projet, il faudrait encapsuler tout ça.

// Après la boucle de rendu, il faudra aussi détruire le device.
// NkDeviceFactory::Destroy(gpuDevice);
```

**Explications :**

- `NkSurfaceDesc` : Contient tous les handles natifs nécessaires pour créer un contexte graphique. C'est le lien entre `NkWindow` et le RHI.
- `NkContextDesc` : Permet de configurer l'API graphique (Vulkan, OpenGL) avec des détails comme la version, l'activation du debug, le nombre de buffers d'échange (swapchain), etc.
- `NkDeviceInitInfo` : Agrège toutes les informations nécessaires pour créer un `NkIDevice`.
- `NkDeviceFactory::Create` : Fonction magique qui va instancier le bon backend (`NkVulkanDevice`, `NkOpenGLDevice`, etc.) en fonction de la configuration et de la plateforme.

---

### **Partie 3 : Gestion des Événements de Redimensionnement**

Un aspect crucial pour une application robuste est la gestion du redimensionnement de la fenêtre. Lorsque l'utilisateur redimensionne la fenêtre, la surface de rendu change de taille. Nous devons en informer notre device.

Nous allons écouter l'événement `NkWindowResizeEvent`.

```cpp
// À ajouter dans la fonction nkmain, avant la boucle principale.
// ...

// Ajouter un callback pour l'événement de redimensionnement
// On utilise un lambda pour capturer les variables dont on a besoin.
eventSystem.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* ev) {
    logger.Info("Fenêtre redimensionnée : {}x{}", ev->GetWidth(), ev->GetHeight());

    // On ne fait le resize que si la fenêtre a une taille valide (>0)
    if (ev->GetWidth() > 0 && ev->GetHeight() > 0) {
        // On met à jour le device graphique.
        // Cela va recréer la swapchain et les framebuffers associés.
        gpuDevice->OnResize(ev->GetWidth(), ev->GetHeight());
    }
});

// La boucle principale reste la même.
// ...
```

**Explications :**

- `AddEventCallback` : Enregistre une fonction (ici un lambda) qui sera appelée à chaque fois qu'un événement du type spécifié (`NkWindowResizeEvent`) est reçu.
- `gpuDevice->OnResize` : Cette fonction est critique. Elle informe le RHI que la surface de sortie a changé de taille. Le device se chargera de recréer les ressources dépendantes de la taille, comme la swapchain et les `NkFramebuffer` associés.

---

### **Partie 4 : Chargement et Compilation des Shaders**

Pour dessiner, nous avons besoin de shaders. Le framework fournit `NkShaderConverter` pour nous aider à charger et convertir des shaders depuis des fichiers (GLSL, HLSL, MSL) vers un format consommable par le RHI (SPIR-V pour Vulkan, ou source pour OpenGL).

Créons deux shaders simples : un vertex shader et un fragment shader pour dessiner un triangle.

**Fichier `triangle.vert.glsl` :**

```glsl
#version 450 core

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec3 aColor;

layout(location = 0) out vec3 vColor;

void main() {
    gl_PointSize = 1.0;
    gl_Position = vec4(aPosition, 0.0, 1.0);
    vColor = aColor;
}
```

**Fichier `triangle.frag.glsl` :**

```glsl
#version 450 core

layout(location = 0) in vec3 vColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(vColor, 1.0);
}
```

Maintenant, chargeons et compilons ces shaders.

```cpp
// À ajouter après la création du gpuDevice et avant la boucle principale.

// --- Chargement du Vertex Shader ---
NkShaderConvertResult vertSpirv = NkShaderConverter::LoadAsSpirv("triangle.vert.glsl");
if (!vertSpirv.success) {
    logger.Error("Échec du chargement/compilation du vertex shader : {}", vertSpirv.errors.CStr());
    // Gérer l'erreur (quitter)
}

// --- Chargement du Fragment Shader ---
NkShaderConvertResult fragSpirv = NkShaderConverter::LoadAsSpirv("triangle.frag.glsl");
if (!fragSpirv.success) {
    logger.Error("Échec du chargement/compilation du fragment shader : {}", fragSpirv.errors.CStr());
    // Gérer l'erreur
}

// --- Création du descripteur de shader pour le RHI ---
NkShaderDesc shaderDesc;
// Ajouter le stage Vertex avec le SPIR-V chargé
shaderDesc.AddSPIRV(NkShaderStage::NK_VERTEX, vertSpirv.binary.Data(), vertSpirv.binary.Size());
// Ajouter le stage Fragment avec le SPIR-V chargé
shaderDesc.AddSPIRV(NkShaderStage::NK_FRAGMENT, fragSpirv.binary.Data(), fragSpirv.binary.Size());
shaderDesc.debugName = "TriangleShader";

// Demander au RHI de créer l'objet shader GPU.
NkShaderHandle gpuShader = gpuDevice->CreateShader(shaderDesc);
if (!gpuShader.IsValid()) {
    logger.Error("Échec de la création du shader GPU.");
    // Gérer l'erreur
}
logger.Info("Shaders chargés et créés avec succès.");
```

**Explications :**

- `NkShaderConverter::LoadAsSpirv` : Charge un fichier `.glsl` et le compile en SPIR-V. C'est idéal pour Vulkan. Pour OpenGL, on utiliserait `NkShaderConverter::LoadFile` pour obtenir la source GLSL.
- `NkShaderDesc` : Un descripteur qui contient tous les stages du shader (vertex, fragment, etc.) et leurs données (SPIR-V binaire ou source).
- `gpuDevice->CreateShader` : Demande au GPU de créer et de compiler les shaders. Le résultat est un `NkShaderHandle` opaque qui sera utilisé plus tard pour créer les pipelines.

---

### **Partie 5 : Création des Buffers GPU (Vertex et Index)**

Nous allons maintenant créer les buffers contenant la géométrie de notre triangle.

```cpp
// Définition de la structure d'un vertex.
struct Vertex {
    math::NkVec2 position;
    math::NkVec3 color;
};

// Les données de notre triangle.
Vertex vertices[] = {
    { { 0.0f,  0.5f }, { 1.0f, 0.0f, 0.0f } }, // Sommet du haut (rouge)
    { { 0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f } }, // Sommet en bas à droite (vert)
    { {-0.5f, -0.5f }, { 0.0f, 0.0f, 1.0f } }  // Sommet en bas à gauche (bleu)
};

// Pas d'index buffer pour un simple triangle.

// 1. Créer un descripteur pour le vertex buffer.
NkBufferDesc vertexBufferDesc = NkBufferDesc::Vertex(
    sizeof(vertices),        // Taille en octets
    vertices                 // Données initiales (copiées)
);
vertexBufferDesc.debugName = "TriangleVertexBuffer";

// 2. Demander au RHI de créer le buffer GPU.
NkBufferHandle vertexBuffer = gpuDevice->CreateBuffer(vertexBufferDesc);
if (!vertexBuffer.IsValid()) {
    logger.Error("Échec de la création du vertex buffer.");
}
logger.Info("Vertex buffer créé.");

// Note : Si nous avions un Index Buffer, nous ferions de même avec NkBufferDesc::Index.
```

**Explications :**

- `NkBufferDesc` : Un descripteur pour créer un buffer. La méthode `Vertex()` est une méthode utilitaire qui définit le type, l'usage et les flags de bind (`NK_VERTEX_BUFFER`) correctement.
- `gpuDevice->CreateBuffer` : Crée le buffer sur le GPU. Les données initiales sont copiées car nous avons passé `initialData`.

---

### **Partie 6 : Création du Vertex Layout et du Pipeline**

Avant de dessiner, nous devons dire au GPU comment interpréter les données dans le vertex buffer. Cela se fait via un `NkVertexLayout` et un `NkGraphicsPipelineDesc`.

```cpp
// 1. Définir la disposition des attributs du vertex.
NkVertexLayout vertexLayout;
vertexLayout.AddAttribute(0,        // location dans le shader (correspond à layout(location = 0))
                           0,        // binding index (0 pour ce buffer)
                           NkGPUFormat::NK_RG32_FLOAT, // format: 2 floats (vec2)
                           offsetof(Vertex, position)); // décalage dans la structure
vertexLayout.AddAttribute(1,        // location (correspond à layout(location = 1))
                           0,        // binding index
                           NkGPUFormat::NK_RGB32_FLOAT, // format: 3 floats (vec3)
                           offsetof(Vertex, color));
vertexLayout.AddBinding(0, // binding index
                        sizeof(Vertex), // stride
                        false); // perInstance = false

// 2. Créer le descripteur du pipeline graphique.
NkGraphicsPipelineDesc pipelineDesc;
pipelineDesc.shader = gpuShader;          // Les shaders créés précédemment
pipelineDesc.vertexLayout = vertexLayout; // La disposition des vertices
pipelineDesc.topology = NkPrimitiveTopology::NK_TRIANGLE_LIST; // On dessine des triangles
pipelineDesc.renderPass = gpuDevice->GetSwapchainRenderPass(); // Le render pass par défaut de la fenêtre
pipelineDesc.debugName = "TrianglePipeline";

// 3. Créer le pipeline GPU.
NkPipelineHandle graphicsPipeline = gpuDevice->CreateGraphicsPipeline(pipelineDesc);
if (!graphicsPipeline.IsValid()) {
    logger.Error("Échec de la création du pipeline graphique.");
}
logger.Info("Pipeline graphique créé.");
```

**Explications :**

- `NkVertexLayout` : Décrit la structure d'un vertex. `AddAttribute` décrit un attribut shader (sa location, son binding, son type GPU, son offset). `AddBinding` décrit un flux de données (binding index, stride).
- `NkGraphicsPipelineDesc` : C'est le descripteur le plus important. Il contient le shader, la disposition des vertices, la topologie, le render pass (lié à la swapchain), etc.
- `gpuDevice->GetSwapchainRenderPass()` : Le framework nous donne le render pass par défaut de la swapchain, qui décrit comment les images de la fenêtre sont présentées. C'est essentiel pour créer un pipeline compatible.
- `gpuDevice->CreateGraphicsPipeline` : Compile et crée le pipeline graphique final sur le GPU. C'est une opération lourde.

---

### **Partie 7 : Boucle de Rendu - Drawing Commands**

Nous avons maintenant tout le nécessaire pour dessiner ! Nous allons utiliser un `NkICommandBuffer` pour enregistrer les commandes de rendu et les soumettre au GPU.

```cpp
// Dans la boucle principale de rendu (while(running)), on remplace la section "--- Rendu ---"
// par le code suivant.

// 1. Créer un command buffer (on le recrée à chaque frame pour l'exemple).
NkICommandBuffer* cmdBuffer = gpuDevice->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);
if (!cmdBuffer) {
    logger.Error("Impossible de créer un command buffer.");
    continue;
}

// 2. Commencer l'enregistrement des commandes.
if (!cmdBuffer->Begin()) {
    logger.Error("Impossible de démarrer l'enregistrement du command buffer.");
    gpuDevice->DestroyCommandBuffer(cmdBuffer);
    continue;
}

// 3. Démarrer le render pass.
//    On utilise le framebuffer courant de la swapchain.
NkFramebufferHandle currentFb = gpuDevice->GetSwapchainFramebuffer();
NkRenderPassHandle renderPass = gpuDevice->GetSwapchainRenderPass();

// La zone de rendu est toute la fenêtre.
NkRect2D renderArea = { 0, 0, (int32)gpuDevice->GetSwapchainWidth(), (int32)gpuDevice->GetSwapchainHeight() };
if (!cmdBuffer->BeginRenderPass(renderPass, currentFb, renderArea)) {
    logger.Error("Impossible de démarrer le render pass.");
    cmdBuffer->End();
    gpuDevice->DestroyCommandBuffer(cmdBuffer);
    continue;
}

// 4. Définir le viewport et le scissor.
NkViewport viewport;
viewport.width = (float32)gpuDevice->GetSwapchainWidth();
viewport.height = (float32)gpuDevice->GetSwapchainHeight();
viewport.minDepth = 0.0f;
viewport.maxDepth = 1.0f;
// On ajuste l'origine Y pour Vulkan (généralement à l'envers).
viewport.flipY = (gpuDevice->GetApi() == NkGraphicsApi::NK_API_VULKAN);
cmdBuffer->SetViewport(viewport);

NkRect2D scissor = renderArea;
cmdBuffer->SetScissor(scissor);

// 5. Binder le pipeline.
cmdBuffer->BindGraphicsPipeline(graphicsPipeline);

// 6. Binder les vertex buffers.
cmdBuffer->BindVertexBuffer(0, vertexBuffer, 0);

// 7. Dessiner !
cmdBuffer->Draw(3, 1, 0, 0); // 3 vertices, 1 instance

// 8. Terminer le render pass.
cmdBuffer->EndRenderPass();

// 9. Terminer l'enregistrement.
cmdBuffer->End();

// 10. Soumettre le command buffer au GPU et présenter l'image.
gpuDevice->SubmitAndPresent(cmdBuffer);

// 11. Le command buffer est consommé par le GPU. On le détruit immédiatement.
gpuDevice->DestroyCommandBuffer(cmdBuffer);
```

**Explications :**

- `NkICommandBuffer` : L'interface pour enregistrer toutes les commandes GPU. On le crée, on l'utilise, on le détruit.
- `BeginRenderPass` : Indique qu'on commence à dessiner dans le framebuffer donné.
- `SetViewport` / `SetScissor` : Configurent la région de rendu.
- `BindGraphicsPipeline` : Active le pipeline créé plus tôt.
- `BindVertexBuffer` : Lie notre buffer de vertices au pipeline.
- `Draw` : Lance le dessin de 3 vertices (notre triangle).
- `SubmitAndPresent` : C'est une méthode pratique qui soumet le command buffer à la queue graphique et demande à la swapchain de présenter l'image.

À ce stade, si tout s'est bien passé, vous devriez voir un triangle coloré s'afficher à l'écran !

---

### **Partie 8 : Ajout de l'Éclairage (Shader de lumière diffuse)**

Passons à la 3D. Nous allons ajouter un modèle d'éclairage simple (type lambertien). Pour cela, nous avons besoin de nouveaux shaders, d'une matrice de transformation, et d'une lumière.

**Nouveau Vertex Shader (`model.vert.glsl`) :**

```glsl
#version 450 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;

layout(location = 0) out vec3 vWorldPosition;
layout(location = 1) out vec3 vWorldNormal;

uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main() {
    vWorldPosition = vec3(ubo.model * vec4(aPosition, 1.0));
    vWorldNormal = mat3(ubo.model) * aNormal; // Pas de correction pour la non-uniformité pour simplifier
    gl_Position = ubo.proj * ubo.view * vec4(vWorldPosition, 1.0);
}
```

**Nouveau Fragment Shader (`model.frag.glsl`) :**

```glsl
#version 450 core

layout(location = 0) in vec3 vWorldPosition;
layout(location = 1) in vec3 vWorldNormal;

layout(location = 0) out vec4 outColor;

uniform UniformBufferObject {
    vec3 lightPos;
    vec3 lightColor;
    vec3 objectColor;
} ubo;

void main() {
    vec3 N = normalize(vWorldNormal);
    vec3 L = normalize(ubo.lightPos - vWorldPosition);
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * ubo.lightColor;

    vec3 ambient = 0.1 * ubo.lightColor;
    vec3 result = (ambient + diffuse) * ubo.objectColor;
    outColor = vec4(result, 1.0);
}
```

**Étapes supplémentaires :**

1. **Créer un Uniform Buffer Object (UBO)** : Pour passer les matrices et les paramètres de lumière au shader.
2. **Créer un nouveau pipeline** avec ces shaders et la nouvelle disposition de vertex (position + normal).
3. **Mettre à jour l'UBO à chaque frame** pour les matrices de vue/projection et la position de l'objet (cube).

**Création de l'UBO :**

```cpp
// Créer un buffer uniforme (dynamique, mis à jour chaque frame)
NkBufferDesc uboDesc = NkBufferDesc::Uniform(sizeof(UniformBufferObject));
uboDesc.debugName = "ModelUBO";
NkBufferHandle uniformBuffer = gpuDevice->CreateBuffer(uboDesc);
```

**Mise à jour de l'UBO (dans la boucle de rendu, avant de dessiner) :**

```cpp
UniformBufferObject uboData;
// uboData.model = ... ; // Matrice du modèle (rotation, translation)
// uboData.view = ... ; // Matrice de vue (caméra)
// uboData.proj = ... ; // Matrice de projection (perspective)
uboData.lightPos = {5.0f, 5.0f, 5.0f};
uboData.lightColor = {1.0f, 1.0f, 1.0f};
uboData.objectColor = {0.6f, 0.4f, 0.2f}; // Orange

// On écrit les données dans le buffer.
// C'est une opération synchrone simple pour l'exemple.
gpuDevice->WriteBuffer(uniformBuffer, &uboData, sizeof(uboData), 0);
```

**Liaison de l'UBO dans le command buffer :**

```cpp
// Après avoir bindé le pipeline, on doit binder le descriptor set contenant l'UBO.
// Pour simplifier, on crée un descriptor set et on le lie.
// (Voir partie 9 pour une explication plus détaillée)
NkDescSetHandle descSet; // À créer et à remplir
// ...
cmdBuffer->BindDescriptorSet(descSet, 0, nullptr, 0);
```

---

### **Partie 9 : Gestion avancée des Descriptors (UBO, Textures)**

Pour passer l'UBO au shader, nous devons utiliser un `Descriptor Set`. C'est une abstraction qui regroupe un ensemble de ressources (buffers, textures) liées à un shader.

**1. Créer un Descriptor Set Layout :**

```cpp
NkDescriptorSetLayoutDesc layoutDesc;
layoutDesc.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_VERTEX); // Binding 0 pour l'UBO
// Si on avait des textures, on ajouterait un autre binding : layoutDesc.Add(1, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
layoutDesc.debugName = "SceneDescriptorLayout";

NkDescSetHandle descSetLayout = gpuDevice->CreateDescriptorSetLayout(layoutDesc);
```

**2. Allouer un Descriptor Set :**

```cpp
NkDescSetHandle descriptorSet = gpuDevice->AllocateDescriptorSet(descSetLayout);
```

**3. Écrire l'UBO dans le Descriptor Set :**

```cpp
NkDescriptorWrite write;
write.set = descriptorSet;
write.binding = 0; // Correspond au binding 0 du layout
write.type = NkDescriptorType::NK_UNIFORM_BUFFER;
write.buffer = uniformBuffer;
write.bufferOffset = 0;
write.bufferRange = sizeof(UniformBufferObject); // Toute la taille

gpuDevice->UpdateDescriptorSets(&write, 1);
```

**4. Lier le Descriptor Set dans le command buffer :**

```cpp
// Dans la boucle de rendu, après avoir bindé le pipeline :
cmdBuffer->BindDescriptorSet(descriptorSet, 0, nullptr, 0);
```

---

### **Partie 10 : Textures**

Ajouter une texture est similaire. Vous devez :

1. Charger une image (vous devrez implémenter ou utiliser une librairie comme stb_image).
2. Créer une texture GPU avec `NkTextureDesc`.
3. Écrire les pixels dans la texture avec `gpuDevice->WriteTexture`.
4. Créer un sampler avec `NkSamplerDesc`.
5. Modifier votre Descriptor Set Layout pour inclure un `NK_COMBINED_IMAGE_SAMPLER`.
6. Mettre à jour le Descriptor Set avec la texture et le sampler.
7. Utiliser la texture dans votre shader (sample it).

---

### **Partie 11 : Ombres (Shadow Mapping)**

Le shadow mapping nécessite deux passes de rendu :

1. **Pass 1 (Profondeur) :** On rend la scène du point de vue de la lumière dans un framebuffer dédié, en écrivant uniquement dans un `NkTextureDesc` de type `Depth`. On utilisera un pipeline simplifié (sans sortie de couleur).
2. **Pass 2 (Final) :** On rend la scène du point de vue de la caméra, et pour chaque fragment, on compare sa profondeur (dans l'espace de la lumière) avec celle stockée dans la texture de profondeur de la lumière. Si elle est plus profonde (ou plus proche selon la configuration), le fragment est dans l'ombre.

**Création du Shadow Map (Framebuffer et Render Pass) :**

```cpp
// 1. Créer la texture de profondeur
NkTextureDesc shadowMapDesc = NkTextureDesc::Depth(1024, 1024, NkGPUFormat::NK_D32_FLOAT);
shadowMapDesc.bindFlags = NkBindFlags::NK_DEPTH_STENCIL | NkBindFlags::NK_SHADER_RESOURCE;
shadowMapDesc.debugName = "ShadowMap";
NkTextureHandle shadowMap = gpuDevice->CreateTexture(shadowMapDesc);

// 2. Créer un Render Pass simple pour l'écriture de la profondeur
NkRenderPassDesc shadowPassDesc;
shadowPassDesc.SetDepth(NkAttachmentDesc::Depth(NkGPUFormat::NK_D32_FLOAT)); // Seul l'attachement depth est utilisé
shadowPassDesc.debugName = "ShadowMapPass";
NkRenderPassHandle shadowRenderPass = gpuDevice->CreateRenderPass(shadowPassDesc);

// 3. Créer un Framebuffer pour ce render pass
NkFramebufferDesc shadowFbDesc;
shadowFbDesc.renderPass = shadowRenderPass;
shadowFbDesc.depthAttachment = shadowMap;
shadowFbDesc.width = 1024;
shadowFbDesc.height = 1024;
NkFramebufferHandle shadowFramebuffer = gpuDevice->CreateFramebuffer(shadowFbDesc);
```

**Rendu de la passe d'ombre (dans un command buffer séparé) :**

```cpp
// Commencer le render pass sur le shadowFramebuffer
cmdBuffer->BeginRenderPass(shadowRenderPass, shadowFramebuffer, {0,0,1024,1024});
// Bind le pipeline d'ombre (un pipeline qui écrit seulement la profondeur)
cmdBuffer->BindGraphicsPipeline(shadowPipeline);
// ... (bind vertex buffers, matrices de projection/vue de la lumière)
// Dessiner tous les objets qui doivent projeter une ombre
cmdBuffer->Draw(...);
cmdBuffer->EndRenderPass();
```

**Utilisation dans la passe finale :**
Dans le shader final, vous aurez un sampler lié à `shadowMap` :

```glsl
uniform sampler2D uShadowMap;
uniform mat4 uLightSpaceMatrix;

// Dans le fragment shader, après avoir calculé la position dans l'espace de la lumière :
vec4 fragPosLightSpace = uLightSpaceMatrix * vec4(vWorldPosition, 1.0);
float shadow = ShadowCalculation(fragPosLightSpace);
vec3 lighting = (ambient + (1.0 - shadow) * diffuse) * lightColor;
// etc.
```

---

### **Partie 12 : Rendu Hors-Écran (Offscreen) et Overlay**

Le rendu hors-écran est exactement ce que nous avons fait pour le shadow map : on crée un `NkRenderPass` et un `NkFramebuffer` avec une ou plusieurs textures de rendu (couleur, profondeur). Ensuite, on dessine dedans. Pour l'overlay, on dessine la scène principale, puis on change de pipeline et on dessine l'interface 2D par-dessus.

**Exemple d'un framebuffer pour rendu 2D d'interface :**

```cpp
// 1. Créer une texture de couleur pour l'interface (même taille que la fenêtre)
NkTextureDesc uiTexDesc = NkTextureDesc::RenderTarget(windowWidth, windowHeight, NkGPUFormat::NK_RGBA8_SRGB);
uiTexDesc.bindFlags = NkBindFlags::NK_RENDER_TARGET | NkBindFlags::NK_SHADER_RESOURCE;
NkTextureHandle uiColor = gpuDevice->CreateTexture(uiTexDesc);

// 2. Créer un render pass pour le UI
NkRenderPassDesc uiPassDesc;
uiPassDesc.AddColor(NkAttachmentDesc::Color(NkGPUFormat::NK_RGBA8_SRGB, NkLoadOp::NK_CLEAR));
// On peut ne pas avoir de depth/stencil pour un overlay simple.
NkRenderPassHandle uiRenderPass = gpuDevice->CreateRenderPass(uiPassDesc);

// 3. Créer le framebuffer
NkFramebufferDesc uiFbDesc;
uiFbDesc.renderPass = uiRenderPass;
uiFbDesc.colorAttachments.PushBack(uiColor);
uiFbDesc.width = windowWidth;
uiFbDesc.height = windowHeight;
NkFramebufferHandle uiFramebuffer = gpuDevice->CreateFramebuffer(uiFbDesc);
```

**Processus de rendu complet :**

```cpp
// 1. On dessine la scène 3D dans le framebuffer de la swapchain (fenêtre).
//    (Comme on le faisait avant)

// 2. On change de render pass pour dessiner l'interface sur notre framebuffer offscreen.
cmdBuffer->BeginRenderPass(uiRenderPass, uiFramebuffer, renderArea);
    // Bind pipeline 2D (sans projection 3D, juste des rectangles, sprites, etc.)
    cmdBuffer->BindGraphicsPipeline(uiPipeline);
    // Dessiner les éléments d'interface (boutons, texte, etc.)
    cmdBuffer->Draw(...);
cmdBuffer->EndRenderPass();

// 3. Pour finalement afficher l'overlay sur l'écran, on doit copier ou blitter la texture de l'UI
//    sur le framebuffer de la swapchain, ou bien on peut dessiner un quad plein écran avec cette texture.
//    L'approche la plus simple est de dessiner un quad texturé sur le framebuffer principal.
cmdBuffer->BeginRenderPass(swapchainRenderPass, swapchainFramebuffer, renderArea);
    // ... dessiner la scène 3D (si on ne l'a pas déjà fait)
    // Puis dessiner un quad plein écran avec la texture uiColor
    cmdBuffer->BindGraphicsPipeline(fullscreenQuadPipeline);
    cmdBuffer->BindTextureSampler(/*descriptor set avec uiColor*/);
    cmdBuffer->Draw(6, 1, 0, 0);
cmdBuffer->EndRenderPass();

gpuDevice->SubmitAndPresent(cmdBuffer);
```

---

### **Conclusion**

Vous avez maintenant une base solide pour développer des applications 2D et 3D avec le framework Nkentseu. Ce cours a couvert :

- L'initialisation et la gestion du cycle de vie.
- La création d'une fenêtre et la gestion des événements.
- L'initialisation d'un périphérique graphique.
- Le chargement et la compilation de shaders.
- La création de buffers et de pipelines.
- Une boucle de rendu fonctionnelle.
- L'ajout d'éclairage via des UBOs et des descriptors.
- L'implémentation d'ombres via le shadow mapping.
- Le rendu hors-écran et la création d'overlays.

Avec ces concepts, vous êtes prêt à explorer des techniques plus avancées comme le PBR (Physically Based Rendering), le post-processing, le compute shader, et bien d'autres, en utilisant la puissance et la flexibilité du RHI de Nkentseu.
