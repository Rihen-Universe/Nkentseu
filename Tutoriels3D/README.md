# Tutoriels3D — concevoir une application 3D avec NKRenderer, pas à pas

Cette série montre **à quel point il est simple** de construire une application 3D
complète avec le moteur Nkentseu : chaque sous-dossier est un **projet autonome**
qui ajoute UNE notion par rapport au précédent. Le code est court, commenté en
français, et chaque étape se compile et se lance seule.

| Étape | Guide | Notion ajoutée | Cible jenga |
|-------|-------|----------------|-------------|
| 1 | [01-Fenetre](01-Fenetre/README.md) | Ouvrir une fenêtre native + réagir aux événements (NKWindow + NKEvent) | `Tuto01Fenetre` |
| 2 | [02-Renderer](02-Renderer/README.md) | Initialiser le GPU (NKRHI auto-détecté) + NKRenderer + texte à l'écran + **safe area** mobile | `Tuto02Renderer` |
| 3 | [03-Scene](03-Scene/README.md) | Une vraie scène : cube, sphère, sol, soleil, **ombres**, panneau de texte | `Tuto03Scene` |
| 4 | [04-Camera](04-Camera/README.md) | Caméra interactive **souris + clavier + tactile** (orbite/pan/zoom, pincement 2 doigts) — logique d'entrées dans un **fichier séparé** [camera_input.h](04-Camera/camera_input.h) | `Tuto04Camera` |
| 5 | [05-Meshes](05-Meshes/README.md) | Mesh **défini par ses sommets dans le code**, mesh **éditable NkEditMesh** (extrusion/subdivision), **sélection au tap/clic** | `Tuto05Meshes` |

Chaque dossier contient un **README complet** : les systèmes utilisés, la lecture
guidée du code, et des pistes concrètes pour enrichir l'application à ce niveau.

## Plateformes

Le même code source tourne sur **Windows, Linux, Web (Emscripten), Android et
HarmonyOS** (zéro `#ifdef` dans les tutos) :

```
jenga build --target Tuto03Scene --config Release                      # Windows
jenga build --target Tuto03Scene --config Release --platform linux    # Linux (ou depuis WSL2)
jenga build --target Tuto03Scene --config Release --platform web      # Web (wasm)
jenga build --target Tuto03Scene --config Release --platform android  # APK Android
jenga build --target Tuto03Scene --config Release --platform harmonyos
```

Sur mobile : orientation paysage, contrôles tactiles (étapes 4-5) et panneaux
décalés par la **safe area** (`NkWindow::GetSafeAreaInsets`) pour éviter
l'encoche et les barres système.

## Compiler et lancer une étape

```
cd D:\Projets\2026\Nkentseu\Nkentseu
jenga build --target Tuto03Scene --config Release
.\Build\Bin\Release-Windows\Tuto03Scene\Tuto03Scene.exe
```

(Remplacer `Tuto03Scene` par la cible de l'étape voulue.)

## Ce qu'il faut retenir

- **Une application = une boucle** : `PollEvents()` → mettre à jour → `BeginFrame()`
  → soumettre la scène → `Present()`/`EndFrame()`.
- **Le renderer fait le gros du travail** : PBR, ombres, texte… un objet 3D =
  un `NkDrawCall3D` (mesh + transform + matériau) soumis à `NkRender3D`.
- **Les entrées sont découplées** : les contrôleurs caméra du moteur
  (`NkOrbitCameraController3D`) ne connaissent pas NKEvent ; l'application traduit
  ses événements en appels `Rotate/Pan/Zoom` — c'est le rôle de `camera_input.h`.
- **Le CPU reste maître de la géométrie** : `NkMeshDesc::Simple` transforme un
  simple tableau de sommets écrit à la main en mesh GPU ; `NkEditMesh` (demi-arête,
  façon Blender) permet d'éditer la topologie puis de re-trianguler vers le GPU.
