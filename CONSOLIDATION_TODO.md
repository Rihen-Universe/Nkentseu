# ⚠️ CONSOLIDATION À FAIRE — fusionner le travail moteur dans la ligne NKCode

> Fichier-rappel versionné (ne pas supprimer tant que la consolidation n'est pas faite).
> Créé 2026-06-29. Worktree `Nkentseu-anima` (branche `chore/externals-submodules`)
> vs repo principal `Nkentseu` (branche `feature/nkcode-launcher`).

## Situation
Deux branches divergées depuis une base ANCIENNE (`f5bd8409`) :
- `feature/nkcode-launcher` = NKCode canonique (PRs propres #7→#10 + launcher).
- `chore/externals-submodules` = vieux NKCode WIP **+ tout mon travail moteur**.

Un `git merge` brut dupliquerait NKCode → conflits massifs. **NE PAS faire un merge brut.**

## Méthode propre = cherry-pick de MES commits moteur sur la ligne NKCode
Commits à rejouer (dans cet ordre) sur `feature/nkcode-launcher` :

```
93e1c38b  feat(nkrenderer): SetFinalColorTarget (graph -> offscreen externe)
eb6877ee  feat(nkeditorkit): backend pluggable (NkIEditorRenderer) + NkGuiRHIBackend
f7497e2d  feat(nkanima): viewport 3D eclaire (NKRHI + device partage + graph->offscreen)
8435019b  feat(foundation): NkVector::Find/IndexOf/Contains + NkMat4::TRS
5ab8c7e4  fix(sandbox): DemoAnimIK garde-fou glTF manquant
24638bba  build(workspace): enregistrer NKECS dans Nkentseu.jenga
04306869  fix(noge): includes ECS obsoletes corriges [WIP api drift restante]
d191caec  feat(nkcollision): module collision 2D+3D zero-STL (self-test 22/22)
5f1a4e0d  docs(nkcollision): ROADMAP a jour
```

## Procédure (quand NKCode est à un jalon stable + l'autre agent en pause)
```
# dans le worktree (ne touche pas le repo principal) :
git checkout -b consolidation feature/nkcode-launcher
git cherry-pick 93e1c38b eb6877ee f7497e2d 8435019b 5ab8c7e4 24638bba 04306869 d191caec 5f1a4e0d
# resoudre conflits attendus : NKRenderer (vs PR #8), NKEditorKit (vs #10 NKEditorKitDemo),
#   Nkentseu.jenga / config/modules.jenga (includes), NKContainers/NKMath (vs #8 lot)
# build de verif : jenga build --target renderdemo ; --target NKCollision ; --target NkAnimaEditor
# puis : merge 'consolidation' -> main (ou feature/nkcode-launcher fast-forward dessus)
```

## Conflits attendus (où regarder)
- **NKEditorKit** (`NkEditorShell.h/.cpp`) : l'autre agent a peut-être DÉJÀ intégré le
  pluggable (NkIEditorRenderer) côté #10 -> garder UNE version cohérente.
- **NKRenderer** (`NkRenderGraph.*`, `NkRendererImpl.*`) : mon `SetFinalColorTarget` vs #8.
- **Nkentseu.jenga / config/modules.jenga** : additions d'includes des deux côtés.
- **Integrations/NKGui** : `NkGuiRHIBackend` est à moi (probablement absent côté NKCode).

Plus on attend, plus la divergence grossit. **Consolider dès que NKCode atteint son jalon.**
