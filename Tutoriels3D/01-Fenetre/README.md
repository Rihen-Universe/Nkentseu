# Étape 1 — Ouvrir une fenêtre et écouter les événements

> **Cible jenga** : `Tuto01Fenetre` · **Source** : [main.cpp](main.cpp) (~60 lignes)
> **Modules découverts** : NKWindow · NKEvent · NKTime · NKLogger
> **Plateformes** : Windows, Linux, Web, Android, HarmonyOS

Toute application graphique commence pareil : une fenêtre, et une boucle qui
réagit à ce que fait l'utilisateur. Dans ce premier tutoriel, on n'affiche
encore **rien** — on apprend le squelette que TOUTES les étapes suivantes
réutiliseront tel quel.

## Ce que vous saurez faire à la fin

- créer une fenêtre native avec `NkWindow` ;
- réagir à la fermeture, au clavier et au redimensionnement avec `NkEvents()` ;
- écrire une boucle principale propre qui ne brûle pas le CPU ;
- fermer proprement l'application.

---

## 1. Le point d'entrée : `nkmain`, pas `main`

Nkentseu est multi-plateforme : sur Windows le vrai point d'entrée est
`WinMain`, sur Android c'est une `NativeActivity`, sur le Web c'est le runtime
Emscripten… Pour ne pas écrire cinq `main` différents, le moteur fournit le
sien (`NKWindow/NKMain.h`) et appelle **votre** fonction :

```cpp
#include "NKWindow/NKMain.h"

int nkmain(const NkEntryState &state) {
    // votre application ici
    return 0;
}
```

Juste au-dessus, ce petit bloc donne un nom à l'application (dossiers de
sauvegarde, logs, nom de process…) :

```cpp
static void ConfigureAppData(NkAppData &d) {
    d.appName = "Tuto01Fenetre";
}
NK_REGISTER_ENTRY_APPDATA_UPDATER(ConfigureAppData)
```

> 💡 **À retenir** : on n'écrit jamais `int main()` dans une app Nkentseu.
> C'est ce qui permet au MÊME fichier de compiler pour Windows, Linux, Android,
> le Web et HarmonyOS sans une seule ligne spécifique à la plateforme.

## 2. Décrire puis créer la fenêtre

La création se fait en deux temps : on remplit une **description**
(`NkWindowConfig`), puis on construit l'objet :

```cpp
NkWindowConfig cfg;
cfg.title     = "Tuto 01 — Une fenetre Nkentseu";
cfg.width     = 1280;
cfg.height    = 720;
cfg.centered  = true;   // centrée sur l'écran
cfg.resizable = true;   // l'utilisateur peut la redimensionner

NkWindow window(cfg);
if (!window.IsValid()) {          // TOUJOURS vérifier
    logger.Error("[Tuto01] Creation fenetre KO");
    return 1;
}
```

Ce pattern « struct de config → objet → `IsValid()` » revient partout dans le
moteur (device GPU, renderer, textures…). Prenez l'habitude de vérifier chaque
création : un moteur qui continue avec un objet invalide crashe plus loin, là
où c'est incompréhensible.

> 📱 Sur mobile et Web, `width`/`height` sont ignorés : la « fenêtre » occupe
> tout l'écran (ou le canvas HTML). Le même code reste correct.

## 3. Les événements : un abonnement, pas une interrogation

NKEvent fonctionne par **callbacks typés** : on s'abonne une fois à un type
d'événement précis, le système nous rappelle quand il survient.

```cpp
bool running = true;
NkEventSystem &events = NkEvents();   // singleton du module NKEvent

// L'utilisateur clique la croix de la fenêtre :
events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) {
    running = false;
});

// Une touche est pressée :
events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent *e) {
    if (e->GetKey() == NkKey::NK_ESCAPE)
        running = false;
});

// La fenêtre change de taille :
events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent *e) {
    logger.Info("[Tuto01] Nouvelle taille : {0}x{1}", e->GetWidth(), e->GetHeight());
});
```

Trois choses importantes :

- **Le type dans les chevrons filtre tout seul** : le callback
  `NkKeyPressEvent` ne verra jamais un événement souris.
- **Les lambdas capturent par référence** (`[&]`) : elles peuvent modifier
  `running` directement. Attention à la durée de vie : ici tout vit dans
  `nkmain`, pas de piège.
- À l'étape 4, on déplacera ces abonnements dans une **classe dédiée**
  (`camera_input.h`) — même mécanisme, mieux rangé.

## 4. La boucle principale

```cpp
while (running && window.IsOpen()) {
    events.PollEvents();          // 1. dépiler les événements de l'OS
    NkClock::Sleep((int64)10);    // 2. pas encore de rendu -> on dort 10 ms
}
```

`PollEvents()` vide la file d'événements de l'OS et **déclenche vos
callbacks**. C'est LE battement de cœur de l'application : sans cet appel, la
fenêtre « ne répond plus » aux yeux du système.

Le `Sleep(10)` n'est là que parce qu'on ne dessine rien : une boucle vide
tournerait à 100 % d'un cœur CPU pour rien. Dès l'étape 2, le rendu (et la
synchronisation verticale) remplacera ce sommeil.

## 5. Fermer proprement

```cpp
window.Close();
logger.Info("[Tuto01] Termine proprement.");
return 0;
```

Règle générale du moteur : **tout ce qui est créé est détruit, dans l'ordre
inverse de la création**. Ici il n'y a que la fenêtre ; à partir de l'étape 2,
l'ordre deviendra important (device GPU → renderer → fenêtre).

---

## Compiler et lancer

```bat
cd D:\Projets\2026\Nkentseu\Nkentseu
jenga build --target Tuto01Fenetre --config Release
.\Build\Bin\Release-Windows\Tuto01Fenetre\Tuto01Fenetre.exe
```

Vous devez voir : une fenêtre 1280×720 centrée. Appuyez sur des touches en
regardant la console : chaque touche est loggée. Redimensionnez : la nouvelle
taille s'affiche. `Échap` ou la croix ferme l'application.

Autres plateformes :

```bash
# Linux (depuis WSL2/Ubuntu)
jenga build --target Tuto01Fenetre --config Release --platform linux
./Build/Bin/Release-Linux/Tuto01Fenetre/Tuto01Fenetre

# Web (produit un .html + .wasm à servir par un petit serveur HTTP)
jenga build --target Tuto01Fenetre --config Release --platform web

# Android (produit un APK universel signé debug)
jenga build --target Tuto01Fenetre --config Release --platform android
```

## Pour aller plus loin (exercices)

1. Loggez la position de la souris (`NkMouseMoveEvent`, header
   `NKEvent/NkMouseEvent.h`).
2. Basculez `cfg.resizable` à `false` et observez la différence.
3. Ajoutez un compteur : au bout de 5 pressions d'`Espace`, fermez l'app.

## Dépannage

| Symptôme | Cause probable |
|---|---|
| La fenêtre s'ouvre puis se ferme aussitôt | Un `return` prématuré — vérifiez le log (`IsValid()` ?) |
| Les touches ne réagissent pas | `PollEvents()` absent de la boucle |
| 100 % CPU | Le `Sleep` a été retiré sans mettre de rendu à la place |

**Étape suivante → [02-Renderer](../02-Renderer/README.md)** : on branche le
GPU et on affiche nos premiers pixels.
