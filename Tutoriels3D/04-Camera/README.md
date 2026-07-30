# Étape 4 — Caméra interactive : souris, clavier, tactile

> **Cible jenga** : `Tuto04Camera` · **Source** : [main.cpp](main.cpp) + [camera_input.h](camera_input.h)
> **Nouveaux systèmes** : `NkOrbitCameraController3D` · pont événements → caméra
> **Prérequis** : [Étape 3](../03-Scene/README.md) — scène, caméra, ombres

La scène de l'étape 3 est belle mais figée : impossible de tourner autour du
cube pour l'admirer sous un autre angle. Ce tutoriel branche une **caméra
orbitale** pilotée à la souris (bureau), au tactile (mobile) et au clavier —
et surtout, il montre **où** ranger ce genre de code pour qu'il reste propre
et réutilisable.

## Ce que vous saurez faire à la fin

- comprendre pourquoi le contrôleur caméra du moteur ignore volontairement
  NKEvent, et comment faire le pont vous-même ;
- lire des deltas souris et des événements multi-touch (avec centroïde et
  pincement) ;
- distinguer un « tap » (sélection) d'un « drag » (caméra) sans qu'ils se
  gênent ;
- écrire un contrôleur clavier à état continu (WASD) ;
- organiser ce code dans un **fichier séparé** que d'autres tutoriels
  réutilisent tel quel.

---

## 1. Pourquoi un fichier séparé ?

`NkOrbitCameraController3D` (dans `NKRenderer/Core/NkCameraController.h`) sait
faire tourner une caméra autour d'un point — mais il ne sait **rien** de
NKEvent, de la souris ou du tactile. C'est un choix de conception délibéré :
un contrôleur caméra ne doit pas imposer un backend d'entrée. Le module
NKRenderer reste utilisable dans un moteur qui ne branche pas NKEvent de la
même façon (un plugin, un outil headless…).

**C'est donc l'application qui fait le pont**, une fois, dans
[camera_input.h](camera_input.h). Regroupez toujours ce genre de traduction
« événements → logique métier » dans un fichier dédié : c'est ce qui permet à
l'étape 5 de le réutiliser **sans en copier une ligne** :

```cpp
#include "04-Camera/camera_input.h"   // dans 05-Meshes/main.cpp
```

## 2. Le contrôleur : une API sans notion d'input

```cpp
tuto::TutoCameraInput camInput;
camInput.orbit.SetCenter({0.f, 0.5f, 0.f}, /*distance*/ 6.f, /*yaw*/ 0.9f, /*pitch*/ -0.35f);
camInput.Install();     // branche les callbacks NKEvent — une seule fois
```

Puis, chaque frame :

```cpp
camInput.Update(dt);        // WASD : état clavier continu
camInput.orbit.Apply(cam);  // écrit position + cible dans la NkCamera3D
```

Le contrôleur expose des verbes génériques — `Rotate(dx, dy)`, `Pan(dx, dy)`,
`Zoom(delta)`, `MoveCameraRelative(right, up, forward)` — sans savoir d'où
viennent ces deltas. C'est `camera_input.h` qui les nourrit.

## 3. La souris : traduire drag et molette

```cpp
events.AddEventCallback<NkMouseMoveEvent>([this](NkMouseMoveEvent *e) {
    const bool left = e->IsButtonDown(NkMouseButton::NK_MB_LEFT);
    const bool middle = e->IsButtonDown(NkMouseButton::NK_MB_MIDDLE);
    if (!left && !middle) return;               // pas de bouton = pas de caméra

    const float32 dx = (float32)e->GetDeltaX();
    const float32 dy = (float32)e->GetDeltaY();
    if (e->GetModifiers().shift)
        orbit.Pan(dx, dy);
    else
        orbit.Rotate(dx * -1.f, dy);             // glisser à droite = tourner à droite
});
events.AddEventCallback<NkMouseWheelVerticalEvent>(
    [this](NkMouseWheelVerticalEvent *e) { orbit.Zoom((float32)e->GetDeltaY()); });
```

Le `GetModifiers().shift` bascule Rotate/Pan sur le MÊME geste (drag) : c'est
une convention (Blender, Maya…) que vos utilisateurs connaissent déjà.

## 4. Le tactile : deltas par doigt + centroïde

Le multi-touch demande un peu plus de logique, car un « doigt » n'est pas une
souris : on reçoit une **liste** de points de contact.

```cpp
events.AddEventCallback<NkTouchMoveEvent>([this](NkTouchMoveEvent *e) {
    const uint32 n = e->GetNumTouches();
    if (n == 1) {
        // 1 doigt : orbite — chaque NkTouchPoint porte déjà son delta.
        const NkTouchPoint &p = e->GetTouch(0);
        orbit.Rotate(p.deltaX * -1.f, p.deltaY);
        return;
    }
    // 2 doigts : la DISTANCE entre eux = zoom (pincement), le CENTROÏDE = pan.
    const NkTouchPoint &a = e->GetTouch(0);
    const NkTouchPoint &b = e->GetTouch(1);
    const float32 ddx = a.clientX - b.clientX, ddy = a.clientY - b.clientY;
    const float32 dist = math::NkSqrt(ddx * ddx + ddy * ddy);
    if (mPinchDist > 0.f)
        orbit.Zoom((dist - mPinchDist) * pinchZoomScale);  // écarter les doigts = zoomer
    mPinchDist = dist;

    const float32 cx = e->GetCentroidX(), cy = e->GetCentroidY();
    orbit.Pan(cx - mLastCX, cy - mLastCY);
    mLastCX = cx; mLastCY = cy;
});
```

Remarquez la symétrie avec la souris : **1 doigt ≈ drag gauche** (orbite),
**2 doigts ≈ Shift+drag + molette à la fois** (pan + zoom). Un utilisateur qui
connaît l'un devine l'autre.

## 5. Le « tap » : séparer sélection et caméra sans conflit

Un problème classique : comment cliquer/toucher un objet pour le
**sélectionner** sans que ce geste fasse aussi tourner la caméra ? Réponse du
tutoriel : on mesure le déplacement cumulé pendant la pression.

```cpp
events.AddEventCallback<NkMouseButtonPressEvent>([this](NkMouseButtonPressEvent *e) {
    if (e->IsLeft()) { mLeftDown = true; mDragAccum = 0.f; /* mémorise x,y */ }
});
events.AddEventCallback<NkMouseButtonReleaseEvent>([this](NkMouseButtonReleaseEvent *e) {
    if (e->IsLeft()) {
        if (mLeftDown && mDragAccum < tapThreshold)   // quasi pas bougé -> un TAP
            mTapPending = true;                        // mémorisé, pas encore consommé
        mLeftDown = false;
    }
});
```

`mDragAccum` cumule la distance parcourue par la souris pendant que le
bouton est enfoncé (incrémentée dans `NkMouseMoveEvent`, non montré ici pour
la brièveté — voir le fichier). Sous le seuil (`tapThreshold`, 10 px) →
c'est un clic de sélection, pas un drag de caméra. Cette étape **n'utilise
pas** encore le tap ; l'étape 5 le consommera avec `ConsumeTap(x, y)`.

## 6. Le clavier : état continu, pas un événement ponctuel

WASD doit bouger la caméra **tant que la touche est maintenue**, pas une
seule fois à l'appui. On n'utilise donc pas de callback d'événement ici, mais
l'état persistant du clavier :

```cpp
void Update(float32 dt) {
    const float32 step = moveSpeed * dt;   // dt : la vitesse ne dépend pas du framerate
    float32 fwd = 0.f, right = 0.f;
    if (NkInput.IsKeyDown(NkKey::NK_W)) fwd += step;
    if (NkInput.IsKeyDown(NkKey::NK_S)) fwd -= step;
    if (NkInput.IsKeyDown(NkKey::NK_D)) right += step;
    if (NkInput.IsKeyDown(NkKey::NK_A)) right -= step;
    if (fwd != 0.f || right != 0.f)
        orbit.MoveCameraRelative(right, 0.f, fwd);
}
```

`NkInput` est rempli par `events.PollEvents()` — c'est un **état** interrogé
(`IsKeyDown`), différent des callbacks événementiels utilisés pour la souris
et le tactile. Deux mécanismes NKEvent, deux besoins différents : ponctuel
(clic, relâchement) vs continu (touche maintenue).

## 7. Dans `main.cpp` : quatre lignes

```cpp
tuto::TutoCameraInput camInput;
camInput.orbit.SetCenter({0.f, 0.5f, 0.f}, 6.f, 0.9f, -0.35f);
camInput.Install();
// … dans la boucle …
camInput.Update(dt);
camInput.orbit.Apply(cam);   // remplace cam.SetPosition/SetTarget de l'étape 3
```

C'est tout le changement par rapport à l'étape 3 : le reste (scène, ombres,
overlay) est identique.

---

## Compiler et lancer

```bat
cd D:\Projets\2026\Nkentseu\Nkentseu
jenga build --target Tuto04Camera --config Release
.\Build\Bin\Release-Windows\Tuto04Camera\Tuto04Camera.exe
```

Testez : drag gauche/milieu pour orbiter, Shift+drag pour panoramiquer,
molette pour zoomer, WASD pour déplacer la cible. Sur mobile : 1 doigt, 2
doigts, pincement.

## Pour aller plus loin (exercices)

1. **Recentrage** : ajoutez une touche (`Home`) qui rappelle
   `orbit.SetCenter(...)` avec les valeurs d'origine.
2. **Inertie** : gardez la dernière vitesse de `Rotate` et amortissez-la dans
   `Update(dt)` même après le relâchement du bouton (10 lignes, aucun
   changement moteur).
3. **Caméra libre** : essayez `NkFlyCameraController3D` (même en-tête) à la
   place de l'orbite, et basculez entre les deux avec une touche.
4. **Auto-orbite** : `orbit.SetAutoOrbit(true)` pour une caméra qui tourne
   toute seule — utile pour une vitrine produit.

## Dépannage

| Symptôme | Cause probable |
|---|---|
| La caméra ne bouge pas à la souris | Vérifiez qu'un bouton est bien détecté (`IsButtonDown`) dans `NkMouseMoveEvent` |
| WASD ne fait rien | `Update(dt)` non appelé chaque frame, ou appelé avec un `dt` figé |
| Le pincement zoome dans le mauvais sens | Inversez le signe de `pinchZoomScale` |
| Un tap déclenche aussi un léger mouvement de caméra | `tapThreshold` trop bas pour la sensibilité de l'écran/souris |

**Étape suivante → [05-Meshes](../05-Meshes/README.md)** : créer sa propre
géométrie (sommets écrits dans le code + mesh éditable) et sélectionner des
objets par rayon précis.
