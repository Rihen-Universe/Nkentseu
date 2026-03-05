# Cours pratique: valider une bibliothèque mémoire pour la production

Ce document explique, étape par étape, comment valider `NKMemory` comme un ingénieur qualité/performance.  
Le but est que **n'importe quel développeur de l'équipe** puisse répéter la méthode et créer d'autres campagnes de test.

---

## 1) Pourquoi faire une campagne "Release + endurance" ?

En `Debug`, on vérifie surtout la logique.
En `Release`, on valide le comportement réel:

- performances (latence moyenne, p99),
- stabilité dans le temps (pas de crash après des heures),
- absence de régression (p99 qui dérive),
- robustesse (multi-thread, fragmentation, coalescing).

Sans cette campagne, un module peut "passer les tests" mais échouer en prod.

---

## 2) Ce qu'on veut mesurer

### 2.1 Mesures fonctionnelles

- Tests passés / total
- Assertions passées / total
- Exit code des runs

### 2.2 Mesures performance

- `avg` (latence moyenne)
- `p99` (latence de queue, plus proche de la réalité utilisateur)
- `failures` (allocations échouées pendant benchmark)

### 2.3 Mesures stabilité

- Nombre de runs consécutifs sans erreur (endurance)
- Dérive du p99 run après run

---

## 3) Scripts fournis

Deux scripts sont prêts:

- Windows: [validate_release.ps1](e:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\Nkentseu\scripts\validate_release.ps1)
- Linux/WSL: [validate_release.sh](e:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\Nkentseu\scripts\validate_release.sh)

Ils font automatiquement:

1. build `Release`,
2. exécution en boucle de `NKMemory_Tests`,
3. collecte logs + CSV,
4. calcul des régressions p99,
5. verdict `GO` / `NO-GO`,
6. sortie d'un rapport JSON + Markdown.

---

## 4) Comment lancer (usage simple)

## 4.1 Windows (PowerShell)

Depuis `Jenga/Exemples/Nkentseu`:

```powershell
.\scripts\validate_release.ps1 -DurationHours 12
```

Mode rapide (1 run):

```powershell
.\scripts\validate_release.ps1 -SkipBuild -MaxRuns 1 -DurationHours 0.01
```

Option utile:

- `-CleanConfigArtifacts $true` (défaut): nettoie `Build/Obj|Lib|Tests/<Config>-Windows` avant build.

## 4.2 Linux/WSL

```bash
cd /mnt/e/Projets/MacShared/Projets/Jenga/Jenga/Exemples/Nkentseu
chmod +x scripts/validate_release.sh
./scripts/validate_release.sh --duration-hours 12 --linux-backend headless
```

Mode rapide:

```bash
./scripts/validate_release.sh --skip-build --max-runs 1 --duration-hours 0.01
```

Option utile:

- `--no-clean`: désactive le nettoyage auto des artefacts de config avant build.

---

## 5) Où trouver les résultats

Les scripts écrivent dans:

`Build/Reports/NKMemory/Validation-<Platform>-Release-<timestamp>/`

Fichiers principaux:

- `summary.json` (machine-readable, CI/CD)
- `summary.md` (lecture humaine)
- `runs.csv` (historique des runs)
- `benchmark.csv` (avg/p99/failures par allocateur)
- `regressions.csv` (dérives p99)
- `logs/` (sortie complète de chaque run)

---

## 6) Règle de décision GO / NO-GO

Le script applique:

- `GO` si:
  - aucun run en échec,
  - aucune failure benchmark,
  - aucune régression p99 au-dessus du seuil.
- `NO-GO` sinon.

Le seuil p99 par défaut est `10%` (`0.10`).

Tu peux le changer:

- PowerShell: `-P99RegressionThreshold 0.05`
- Bash: `--p99-threshold 0.05`

---

## 7) Comprendre la dérive p99 (important)

Exemple:

- p99 baseline: `500 ns`
- p99 max observé: `620 ns`
- dérive = `(620 - 500) / 500 = 0.24 = 24%`

Si ton seuil est `10%`, c'est une régression => `NO-GO`.

Pourquoi on s'en sert:

- la moyenne (`avg`) peut rester bonne,
- mais les pires cas (p99) peuvent se dégrader fortement.

---

## 8) Comment créer d'autres tests similaires

Tu peux reproduire exactement la méthode pour un autre module.

### 8.1 Injecter des tests via Jenga

Dans `<Module>.jenga`:

```python
with filter("system:Linux || system:macOS || system:Windows && !options:windows-runtime=uwp && !system:XboxSeries && !system:XboxOne"):
    with test():
        testfiles(["tests/**.cpp"])
```

### 8.2 Structurer les tests C++

Dans `tests/*.cpp`, faire 3 blocs:

1. **Unit tests** (API correcte)
2. **Stress tests** (beaucoup d'itérations, multi-thread)
3. **Benchmark tests** (imprimer avg/p99/failures dans un format stable)

Format benchmark recommandé:

```text
- Name avg=123.45 ns p99=678.90 ns failures=0/2048
```

Le script sait parser cette ligne.

### 8.3 Ajouter un nouveau KPI

Exemple: `p999`.

1. Ajouter impression côté test.
2. Ajouter parsing dans script.
3. Ajouter règle de décision dans `summary`.

---

## 9) Conseils pédagogiques pour étudiants/jeunes devs

Quand tu construis une campagne de validation, pense en couches:

1. Correctness: "le code marche ?"
2. Performance: "ça marche vite ?"
3. Stability: "ça marche longtemps ?"
4. Regression: "ça reste stable au fil des versions ?"
5. Automation: "est-ce exécutable sans intervention humaine ?"

Si une couche manque, le verdict production est fragile.

---

## 10) Plan d'amélioration avancé (niveau supérieur)

Pour industrialiser davantage:

- ajouter exécution CI quotidienne (nightly),
- conserver l'historique des `summary.json`,
- comparer version N vs N-1 automatiquement,
- lancer aussi sur Linux natif + Android + Web,
- ajouter tests mémoire externes (ASan/Valgrind/Dr. Memory selon cible).

---

## 11) Résumé en 5 lignes

- `Debug` valide la logique, `Release` valide la réalité.
- Les scripts fournis automatisent build + endurance + benchmark + verdict.
- Le critère principal perf est `p99`, pas seulement `avg`.
- Le verdict `GO/NO-GO` est basé sur des seuils explicites.
- La méthode est réutilisable pour tout module futur.

---

## 12) Dépannage (cas réel rencontré)

Erreur possible en build Release:

```text
error: __OPTIMIZE__ predefined macro was disabled in precompiled file ...NKPlatform.pch but is currently enabled
```

Cause:

- mismatch de configuration autour du PCH (artefacts mélangés entre modes/flags).

Actions:

1. Relancer avec nettoyage activé (défaut dans les scripts fournis).
2. Si ça persiste, nettoyer tout `Build/` puis rebuild Release.
3. Si le problème reste, désactiver temporairement le PCH sur le module fautif pour confirmer l'origine.

Le script est prêt pour ce cas:

- PowerShell: `-CleanConfigArtifacts $true` (défaut)
- Bash: `--no-clean` pour désactiver, sinon nettoyage actif par défaut

## 13) Troubleshooting CSV locale issue

Observed real case: status `NO-GO` with very large `benchmark_failures` (for example `4000`) and `runs_executed` null.

Root cause:

- Windows decimal separator from locale can produce malformed CSV rows.
- Example malformed row: `1,NkMallocAllocator,188,11,300,0,2035` (7 columns instead of 6).

Effect:

- `p99` and `failures` columns are shifted.
- Script sums wrong values and can output false `NO-GO`.

Fix applied in `validate_release.ps1`:

- invariant number formatting (`.`) when writing CSV,
- robust parsing for both decimal formats (`.` and `,`),
- run counting forced as array (`@(...)`) to avoid null count on single-run reports.
