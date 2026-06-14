# BugReports — Catalogue des erreurs & bugs Nkentseu

Dossier central des **erreurs rencontrées** pendant le développement de Nkentseu et
leurs **solutions vérifiées**. But : ne jamais re-déboguer deux fois le même
problème, et donner à n'importe qui (toi dans 6 mois, un contributeur) une fiche
actionnable par symptôme.

## Comment utiliser

1. Identifie la **catégorie** du problème (voir ci-dessous).
2. Ouvre le sous-dossier, cherche par **symptôme** (code d'erreur, message, comportement).
3. Chaque fiche suit le même format : Symptôme → Contexte → Cause racine → Solution → Vérification → Liens.

## Comment ajouter une fiche

Crée un `.md` dans le bon sous-dossier (ou crée un nouveau sous-dossier de catégorie).
Nomme-le par le symptôme principal (kebab-case). Réutilise le **gabarit** ci-dessous.
Ajoute une ligne dans la table de la catégorie concernée ici.

```markdown
# <Titre court du problème>

- **Catégorie** : <DirectX12 | NkSL-Generateurs | Backends-Rendu | Environnement-Windows | Memoire-Heap | ...>
- **Sévérité** : bloquant | majeur | mineur | cosmétique
- **Date** : AAAA-MM-JJ
- **Statut** : résolu | contourné | ouvert

## Symptôme
<Ce qu'on voit : message exact, code hr, crash, écran noir, etc.>

## Contexte
<Quand ça arrive : backend, toolchain, OS, étape.>

## Cause racine
<Le vrai pourquoi.>

## Solution
<Le fix exact, avec fichiers:lignes et extraits de code.>

## Vérification
<Comment confirmer que c'est réglé.>

## Liens
<Fichiers sources, autres fiches, mémoire.>
```

## Catégories

### [DirectX12/](DirectX12/) — device DX12, PSO, root signature, dxc, debug layer
| Fiche | Sévérité | Symptôme |
|-------|----------|----------|
| [pso-signature-linkage-E_INVALIDARG.md](DirectX12/pso-signature-linkage-E_INVALIDARG.md) | bloquant | `CreateGraphicsPipelineState` échoue `0x80070057` |
| [debug-layer-graphics-tools.md](DirectX12/debug-layer-graphics-tools.md) | outil | Pas de message de validation DX12 (E_INVALIDARG opaque) |
| [dxc-integration-clang-mingw.md](DirectX12/dxc-integration-clang-mingw.md) | majeur | Intégrer dxc (DXIL/SM6) sous clang-mingw |
| [root-signature-register-base.md](DirectX12/root-signature-register-base.md) | bloquant | Shader utilise t0/s0 hors root signature |
| [root-signature-table-par-registre.md](DirectX12/root-signature-table-par-registre.md) | bloquant | ≥2 textures invisibles (sol noir) — table SRV offsetée |
| [dxc-entry-point-hardcode-main.md](DirectX12/dxc-entry-point-hardcode-main.md) | majeur | dxc « missing entry point » (-E main codé en dur) |
| [depth-state-tracking-clear-barrier.md](DirectX12/depth-state-tracking-clear-barrier.md) | majeur (ouvert) | Spam `DEPTH_READ vs DEPTH_WRITE` (clear/barrier) |

### [DirectX11/](DirectX11/) — device DX11, input layout, samplers
| Fiche | Sévérité | Symptôme |
|-------|----------|----------|
| [input-layout-format-enum-index.md](DirectX11/input-layout-format-enum-index.md) | majeur | Dernier triangle d'un mesh avec UV=(0,0) (format d'attribut faux : table indexée par enum → lit 16 o → dépasse le VB) |

### [NkSL-Generateurs/](NkSL-Generateurs/) — générateurs de code NkSL→GLSL/HLSL/MSL
| Fiche | Sévérité | Symptôme |
|-------|----------|----------|
| [hlsl-dx12-bugs-generateur.md](NkSL-Generateurs/hlsl-dx12-bugs-generateur.md) | bloquant | HLSL DX12 invalide (casse, matrices, littéraux, entry) |
| [hlsl-dx-binding-shared-counter.md](NkSL-Generateurs/hlsl-dx-binding-shared-counter.md) | majeur | Textures invisibles DX (compteur binding séparé vs partagé) |
| [glsl-locations-bindings.md](NkSL-Generateurs/glsl-locations-bindings.md) | bloquant | OpenGL écran noir (location/binding manquants) |

### [Backends-Rendu/](Backends-Rendu/) — rendu par backend (GL/VK/DX/SW)
| Fiche | Sévérité | Symptôme |
|-------|----------|----------|
| [opengl-vulkan-rendu.md](Backends-Rendu/opengl-vulkan-rendu.md) | majeur | Écran noir / crash / teinte selon backend |

### [Environnement-Windows/](Environnement-Windows/) — toolchain, runtimes, features OS
| Fiche | Sévérité | Symptôme |
|-------|----------|----------|
| [runtime-vc-dxcompiler-graphics-tools.md](Environnement-Windows/runtime-vc-dxcompiler-graphics-tools.md) | bloquant | dxcompiler.dll ne charge pas / debug layer absent |

### [Memoire-Heap/](Memoire-Heap/) — corruption, use-after-free, allocateurs
| Fiche | Sévérité | Symptôme |
|-------|----------|----------|
| [nkunorderedmap-define-find-rehash.md](Memoire-Heap/nkunorderedmap-define-find-rehash.md) | majeur | SIGSEGV intermittent (deref null après insert+find) |

---

> Les sessions de travail détaillées sont aussi historisées dans la mémoire de Claude
> (`~/.claude/projects/.../memory/`), mais ces fiches-ci sont **dans le repo** et
> orientées « par symptôme » pour une recherche rapide.
