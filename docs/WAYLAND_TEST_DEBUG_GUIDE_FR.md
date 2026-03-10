# Guide Test Rapide Wayland (FR)

Ce document explique:
- comment tester rapidement `SandboxGamepadPS3` sous Wayland,
- comment valider les événements (croix, clavier, souris),
- pourquoi les logs peuvent ne pas apparaître,
- comment diagnostiquer un problème clavier (`ESC`).

## 1. Ce que je peux tester automatiquement vs manuellement

- Automatique (depuis terminal): build, lancement binaire, vérification sortie console, vérification fichiers de logs, instrumentation code.
- Manuel (nécessite interaction GUI): cliquer la croix de fenêtre, appuyer `ESC`, vérifier focus clavier visuel.

Important:
- Dans cet environnement, je peux compiler et lancer, mais je ne peux pas garantir un clic souris réel sur la fenêtre comme un humain.
- Pour les tests d’input GUI, j’utilise surtout:
  - instrumentation dans le code,
  - logs console/fichier,
  - puis validation par toi sur la fenêtre affichée.

## 2. Boucle de test rapide (Wayland)

Depuis WSL2:

```bash
cd /mnt/e/Projets/MacShared/Projets/Jenga/Jenga/Exemples/Nkentseu
python3 ../../jenga.py build --target SandboxGamepadPS3 --platform Linux --linux-backend wayland --config Debug --jobs 4
./Build/Bin/Debug-Linux-wayland/SandboxGamepadPS3/SandboxGamepadPS3
```

## 3. Vérifier les événements essentiels

### 3.1 Clic sur la croix (fermeture fenêtre)
- Côté Wayland, l’événement provient de `xdg_toplevel::close`.
- Backend: `NkWaylandWindow.cpp` met `mWantsClose = true`.
- `NkWaylandEventSystem.cpp` convertit ensuite cela en `NkWindowCloseEvent`.

Test:
1. Lancer l’app.
2. Cliquer la croix.
3. Vérifier que la boucle sort rapidement.

### 3.2 Touche `ESC`
- L’app ferme sur `NkKeyPressEvent` avec `NK_ESCAPE`.
- J’ai ajouté un fallback: fermeture aussi si `scancode == NK_SC_ESCAPE`.

Test:
1. Cliquer dans la fenêtre pour forcer le focus.
2. Appuyer `ESC`.
3. Vérifier sortie immédiate.

## 4. Pourquoi les logs ne s’affichent parfois pas

Causes fréquentes:
- Niveau de log trop strict dans le code (ex: `NK_FATAL` masque `Info/Debug`).
- App lancée hors terminal (pas de console visible).
- Logs redirigés majoritairement vers fichier.

Dans ce projet:
- `NkLog` utilise console + fichier `logs/app.log`.
- Le niveau de log dans `main8.cpp` est maintenant:
  - `DEBUG` en build debug,
  - `INFO` sinon.

Commandes utiles:

```bash
tail -f logs/app.log
```

ou

```bash
./Build/Bin/Debug-Linux-wayland/SandboxGamepadPS3/SandboxGamepadPS3 2>&1 | tee /tmp/sandbox_wayland.log
```

## 5. Diagnostic clavier Wayland (méthode)

Approche recommandée:
1. Vérifier focus: clic dans la fenêtre.
2. Vérifier émission `NkKeyPressEvent` (console/fichier).
3. Vérifier mapping:
   - keysym (`xkb`) -> `NkKey`,
   - fallback scancode.
4. Vérifier `windowId` associé (peut être invalide selon compositeur/surface), mais l’app lit aussi les events sans filtre de fenêtre.

## 6. Résumé des correctifs appliqués

- Backend Wayland durci:
  - résolution robuste de fenêtre/focus,
  - fallback de mapping clavier (`keysym + scancode`),
  - normalisation `Escape`,
  - meilleure gestion des transitions `seat`.
- App `main8.cpp`:
  - niveau de log restauré (`DEBUG/INFO` au lieu de `FATAL`),
  - fermeture `ESC` avec fallback scancode.

