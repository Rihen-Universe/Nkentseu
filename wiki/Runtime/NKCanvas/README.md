# NKCanvas — documentation détaillée

Le module **NKCanvas**, partie par partie. Pour une vue d'ensemble et un guide « par où
commencer », voir le récap : [../NKCanvas.md](../NKCanvas.md).

Chaque page suit la même structure : un **tutoriel** narratif, un **aperçu** tabulaire de toute
l'API, puis une **référence complète** où chaque élément est expliqué avec ses cas d'usage concrets
(rendu, gameplay, animation, UI/2D, physique, outils/éditeur).

| Page | Ce qu'on y apprend | Headers |
|------|--------------------|---------|
| [Context.md](Context.md) | Ouvrir la session GPU : choisir une API (`NkGraphicsApi`), décrire (`NkContextDesc`), fabriquer (`NkContextFactory` → `NkIGraphicsContext`), accès natif, et calcul GPU pur (`NkIComputeContext`). | `Core/NkGraphicsApi.h`, `NkContextDesc.h`, `NkIGraphicsContext.h`, `NkContextInfo.h`, `NkOpenGLDesc.h`, `NkWGLPixelFormat.h`, `NkGpuPolicy.h`, `NkNativeContextAccess.h`, `NkSurfaceDesc.h`, `NKRenderer2D.h`, `Factory/NkContextFactory.h`, `Compute/NkIComputeContext.h` |
| [Drawing.md](Drawing.md) | Dessiner en 2D façon SFML : la façade `NkRenderer2D` (frame, vue, clip, blend), `NkTransform`/`NkTransformable`, `NkVertexArray`, `NkDrawable`/`NkRenderStates`, et le batcher `NkBatchRenderer2D`. | `Renderer/Core/*` (`NkRenderer2DTypes.h`, `NkRenderer2D.h`, `NkRenderer2DFactory.h`, `NkTransform.h`, `NkTransformable.h`, `NkVertexArray.h`, `NkDrawable.h`, `NkRenderStates.h`), `Renderer/Batch/NkBatchRenderer2D.h` |
| [Shapes.md](Shapes.md) | Poser des formes prêtes à l'emploi : la base `NkShape` (style, texture, triangulation fan) et le rectangle, le cercle, le polygone convexe, le segment épais. | `Renderer/Shapes/NkShape.h`, `NkRectangleShape.h`, `NkCircleShape.h`, `NkConvexShape.h`, `NkLineShape.h` |
| [Resources.md](Resources.md) | Les objets GPU : texture, sprite, texte, shader, matériau, fonte, et les tables d'aiguillage backend texture/shader. | `Renderer/Resources/NkTexture.h`, `NkSprite.h`, `NkFont.h`, `NkShader.h`, `NkMaterial.h` |
| [Targets.md](Targets.md) | Choisir *où* rendre : la surface abstraite `NkRenderTarget`, la fenêtre `NkRenderWindow`, la texture offscreen `NkRenderTexture`, son dispatch GPU, et le pont NKUI → Canvas. | `Renderer/Targets/NkRenderTarget.h`, `NkRenderWindow.h`, `NkRenderTexture.h`, `NkRenderTextureBackend.h`, `UI/NkUICanvasBackend.h` |

[← Récap NKCanvas](../NKCanvas.md) · [← Couche Runtime](../README.md)
