# NKLogger : où va vraiment une ligne de journal

**AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**

> Couche **System** · NKLogger · La table de vérité **puits × configuration**, et la table des
> **niveaux**.
>
> Elle existe pour la même raison que celle de NKWindow, un cran plus loin : un utilisateur a écrit
> `logger.Debug()` dans sa boucle de jeu, n'a **rien** vu, a mis `consoleapp()` dans son `.jenga`,
> n'a **toujours rien** vu, a essayé `std::cout` — rien non plus — et a cherché l'erreur chez lui
> pendant des heures. **Trois causes étaient vraies en même temps, et aucune ne se disait.**
>
> Tout ce qui suit est **mesuré** le 25/09/2026 sur Windows (clang/UCRT64, `--target=x86_64-w64-windows-gnu`),
> par `Sandbox/System/NKLoggerSilence/` et son banc `banc_silence.py`. Ce qui n'est pas mesuré est
> écrit « non mesuré » et non « ça marche ».

---

## 1. Les trois silences, nommés

| # | Le silence | Ce qui le produit | Mesure |
|---|---|---|---|
| **S1** | `logger.Debug()` ne sort **nulle part**, même en Debug, même avec un puits console réglé sur `debug` | Le **niveau du journal** vaut `info`. `NK_DEBUG = 1 < NK_INFO = 2`, et `NkLogger::LogInternal` rejette **avant** de distribuer aux puits. Le puits n'y peut rien : le filtre est en amont. | Debug : `GetSinkCount=3`, `GetLevel=info`, aucun `[DBG]` nulle part |
| **S2** | En **Release**, rien n'est visible — pas même `logger.Error()` | `NkLogger.jenga` pose `defines(["NDEBUG"])` en Release, et `NkLog.cpp` gardait le puits console derrière `#if !defined(NDEBUG)`. Le puits était **compilé hors du module**. | `llvm-nm src_NKLogger_NkLog.obj` : **0** référence à `NkConsoleSink` ; `GetSinkCount=2` |
| **S3** | `NK_LOG_LEVEL` ne faisait rien | La variable est documentée dans `NkLogLevel.h`… **à l'intérieur d'un bloc de commentaire** (« Exemple 6 »). Aucune ligne de code ne la lisait. | `grep NK_LOG_LEVEL` → 1 occurrence, dans `/* … */` |

⚠️ **Le piège de S2 n'est pas réparable par l'utilisateur.** Le drapeau qui décide de son puits
console est posé dans le `.jenga` **du logger**, pas dans le sien. Mettre `consoleapp()` ou
`defines(["NDEBUG"])` dans son propre projet ne change rien à `NKLogger.lib`.

---

## 2. `consoleapp()` sur Windows : ce qu'il fait vraiment

**Rien.** Mesuré, pas supposé :

| Constat | Mesure |
|---|---|
| Le chemin natif de Jenga n'émet **aucun** `/SUBSYSTEM` ni `-mwindows` sur Windows | ligne de lien relevée en `--verbose` : `clang++ -o …exe …objs … --target=x86_64-w64-windows-gnu -static-libstdc++ -static-libgcc -static -Wl,-Bstatic -lpthread` |
| `CONSOLE_APP` / `WINDOWED_APP` n'apparaissent dans `Builders/Windows.py` que pour l'**extension** du binaire et pour l'**icône** | `grep` : aucune occurrence liée au sous-système |
| Le sous-système n'est traité que par les **générateurs** (`jenga gen` → `.vcxproj`, CMake), jamais par `jenga build` | `Gen.py:1137` et `Gen.py:432` |
| Conséquence : **tous** les exécutables du dépôt sont en sous-système **CONSOLE (3)**, y compris ceux déclarés `windowedapp()` | `NK3DModeler.exe`, `renderdemo.exe`, `Nogee.exe`, `PV3DE.exe`, `NKUIDesign.exe` : `Subsystem=3` lu dans l'en-tête PE |

**Donc, sur Windows : une console est attachée dès qu'on lance depuis un terminal, et
`consoleapp()` n'y est pour rien.** L'hypothèse « aucune console attachée » est **réfutée** pour ce
couple plateforme/chaîne d'outils. Elle reste vraie ailleurs (voir §5).

---

## 3. La table des puits

`existe` = la classe est dans le module · `par défaut` = branché par `NkLog::NkLog()` sans que
personne ne le demande.

| Puits | Existe | Branché par défaut | Où il écrit | Ce que l'utilisateur voit |
|---|---|---|---|---|
| **NkConsoleSink** | oui | **oui, désormais dans toutes les configurations** (Debug : niveau `debug` · Release : niveau `error` · Android/HarmonyOS : `debug`) | `stdout` pour `trace…warn` ; `stderr` pour `error`, `critical`, `fatal` (`m_UseStderrForErrors` vaut vrai par défaut) ; **logcat** sur Android ; **hilog** sur HarmonyOS | En Release : **uniquement les erreurs**, et sur **stderr**. C'est délibéré : `stdout` est ce que lisent les bancs du dépôt, on n'y touche pas. |
| **NkFileSink** (a) « journal de la course ») | oui | oui | `logs/app_<AAAA-MM-JJ>_<HHMMSS>_<pid>.log`, **relatif au répertoire de travail** | un fichier par lancement, jamais écrasé ; les 20 derniers sont gardés (`NKENTSEU_LOG_KEEP`) |
| **NkFileSink** (b) « journal courant » | oui | oui | `logs/app.log`, **relatif au répertoire de travail**, tronqué à chaque lancement | le nom historique ; ne contient plus que la **dernière** course |
| **NkDailyFileSink** | oui | **non** | fichier + rotation à minuit | rien, sauf `AddSink()` explicite |
| **NkRotatingFileSink** | oui | **non** | fichier + rotation à la taille | rien, sauf `AddSink()` explicite |
| **NkAsyncSink** | oui | **non** | enveloppe un autre puits derrière une file | rien, sauf `AddSink()` explicite |
| **NkDistributingSink** | oui | **non** | composite : redistribue à N sous-puits | rien, sauf `AddSink()` explicite |
| **NkNullSink** | oui | **non** | nulle part, exprès | rien, par construction |

### ⚠️ Deux pièges de chemin, mesurés

1. **`logs/` est relatif au RÉPERTOIRE DE TRAVAIL, pas à l'exécutable.** Le même binaire lancé
   depuis deux dossiers écrit dans deux fichiers différents. C'est pourquoi la ligne de démarrage
   imprime désormais le chemin **absolu** résolu, jamais `logs/app.log`.
2. **`CreateLogger("mon-module")` rend un logger qui n'a AUCUN puits.** `NkRegistry::GetOrCreate`
   crée le logger, lui pose le niveau global… et s'arrête là. Mesure : `GetSinkCount=0`. Il accepte
   tous les appels et n'écrit nulle part. Seul `CreateDefaultLogger()` (le logger nommé `default`)
   reçoit un puits console. **Ce point n'est pas corrigé** — il est constaté, et le banc a un critère
   (`C7`) pour que la table cesse d'être vraie si quelqu'un y touche.

---

## 4. La table des niveaux

`NkLogLevel` est **ordonné**, et le filtre est un `>=` :

| Niveau | Valeur | Passe le filtre par défaut (`info`) ? | Où se règle le filtre |
|---|---|---|---|
| `NK_TRACE` | 0 | **non** | `NkLogger::SetLevel()` · `NK_LOG_LEVEL=trace` |
| `NK_DEBUG` | 1 | **non** ← *le défaut signalé par l'utilisateur* | `NkLogger::SetLevel()` · `NK_LOG_LEVEL=debug` |
| `NK_INFO` | 2 | oui | défaut |
| `NK_WARN` | 3 | oui | |
| `NK_ERROR` | 4 | oui | |
| `NK_CRITICAL` | 5 | oui | |
| `NK_FATAL` | 6 | oui | |
| `NK_OFF` | 7 | — | sentinelle : `NK_LOG_LEVEL=off` éteint tout |

**Il y a DEUX filtres en série, et c'est la source de la confusion :**

```
logger.Debug(…)
   │
   ├─ filtre 1 : le NIVEAU DU JOURNAL      (NkLogger::m_Level, défaut = info)   ← rejette ici
   │
   └─ pour chaque puits :
        filtre 2 : le NIVEAU DU PUITS      (NkISink::SetLevel)
```

Un puits réglé sur `debug` ne verra jamais un `Debug` si le **journal** est resté sur `info`.
C'est exactement ce qui se passait en Debug : trois puits, l'un d'eux réglé sur `debug`, et zéro
ligne. **Régler le puits sans régler le journal ne sert à rien.**

---

## 5. Par plateforme

| Plateforme | Console attachée ? | Puits console → où | Journal fichier utilisable ? |
|---|---|---|---|
| **Windows** (clang/UCRT64, chemin natif Jenga) | **oui**, sous-système CONSOLE pour *tous* les binaires du dépôt — mesuré dans l'en-tête PE | `stdout` / `stderr` | oui |
| **Windows**, lancé depuis un IDE ou un script | `stdout` devient un **tube** ou un **fichier** ; `GetConsoleWindow()` rend 0 | idem, mais **entièrement tamponné** (voir §6) | oui |
| **Linux / macOS** | oui depuis un terminal | `stdout` / `stderr` | oui — *non mesuré ce jour* |
| **Android** | non (pas de console) | **logcat**, via `__android_log_print` | **non** : `logs/` est relatif et le répertoire courant n'est pas inscriptible. C'est pourquoi le puits console y reste actif même en Release. |
| **HarmonyOS** | non | **hilog**, domaine `0x3200`, `%{public}s` | non — même raison |
| **Emscripten / Web** | non | `stdout` vers la console du navigateur, **vidé à chaque message** (sinon rien ne sort avant la fin de l'image) | non |

---

## 6. `std::cout` qui ne sort pas — un défaut À PART

Il a sa propre cause, même quand il accompagne le silence du logger. **Mesuré :**

| Condition de lancement | `GetFileType(stdout)` | `std::cout << "…"` sans `flush`, dans une boucle qui ne finit pas |
|---|---|---|
| **vraie console** (terminal, double-clic) | `2` = `FILE_TYPE_CHAR` | **VISIBLE immédiatement.** Relecture du tampon d'écran de la console *avant* la sortie du programme : les 15 lignes de la boucle y sont. La libc ne tamponne pas vers une console. |
| **redirigé** (`> fichier`, tube, IDE) | `1` = `DISK` ou `3` = `PIPE` | **INVISIBLE.** Preuve dans l'ordre des lignes : les écritures faites par `WriteFile` (sans libc) arrivent **en tête** du fichier, et tout ce qui est passé par `cout`/`printf` arrive **à la fin**, vidé d'un bloc à la sortie du programme. Une boucle de jeu qui ne se termine jamais ne vide jamais. |

**Conclusions, séparées :**

1. Depuis un **terminal**, `std::cout` fonctionne. Si l'utilisateur ne voit rien là, la cause n'est
   ni le tampon ni le sous-système : c'est qu'il a écrit `std::cout << ""` — **une chaîne vide
   n'imprime aucun octet** — ou que son `cout` n'est jamais atteint.
2. Depuis un **IDE, un script, ou avec une redirection**, `std::cout` est entièrement tamponné :
   il faut `<< std::endl` (ou `std::flush`) à chaque ligne, sinon rien ne paraît avant la fin.
3. **La chaîne d'outils et le mode de lancement font partie des hypothèses.** Le même binaire ne
   se comporte pas pareil selon qu'il est lancé depuis un terminal ou depuis un outil qui capture
   sa sortie — c'est la même leçon que l'audit NKWindow de cette semaine (clang 22/libstdc++ dans
   un terminal contre clang 18/libc++ depuis NKCode).

⚠️ **Un piège voisin, déjà payé, à ne pas réintroduire** : `NkWindowsDesktop.h` appelait
`AllocConsole()` inconditionnellement en Debug, ce qui **réinitialisait les descripteurs standard**
et détournait toute la sortie vers une console jetable — une redirection `> fichier` était
court-circuitée. Corrigé le 21/08/2026 : la console n'est allouée que si l'appelant n'en fournit
aucune.

---

## 7. La règle par défaut, et ce qu'elle change

| | Avant (mesuré) | Après (mesuré) |
|---|---|---|
| Puits console, **Debug** | oui, niveau `debug` | **inchangé** |
| Puits console, **Release** | **aucun** | oui, niveau `error` → part sur **stderr** |
| Niveau du journal | `info` | `info` — **délibérément inchangé** |
| Niveau des puits fichier | `info`, figé | suit le journal **quand on le baisse** (sinon `info`) |
| `NK_LOG_LEVEL` | ignorée | **lue**. Une valeur illisible est **nommée**, pas silencieusement remplacée par `info`. |
| `NK_LOG_CONSOLE` | n'existait pas | `0` = aucun puits console · `1` = puits sans filtre propre · un nom de niveau |
| `NK_LOG_QUIET` / `NK_LOG_BANNER` | n'existaient pas | taire / forcer la ligne de démarrage |
| Ligne de démarrage | aucune | deux lignes sur **stderr**, **seulement si stderr est une vraie console** |

### Pourquoi le niveau par défaut reste `info`

Parce que le piège n'était **pas** la valeur, c'était le **silence sur la valeur**. Passer le
défaut à `debug` rendrait bavards tous les bancs Debug du dépôt d'un coup, pour régler un problème
qu'une ligne de texte règle mieux : la bannière dit maintenant, à l'écran, que `trace` et `debug`
sont sous le niveau et comment les remonter.

### Pourquoi la bannière n'est écrite que sur une console

Pour que **la garde « aucun banc ne devient bavard » soit structurelle et non promise**. Un banc
redirige toujours sa sortie ; il ne remplit donc jamais la condition. Un humain dans son terminal
la remplit toujours. Le banc a un critère dédié (`C5`) et un autre (`C3`) qui vérifie que **rien**
n'a été ajouté sur `stdout`.

### Pourquoi une seule variable suffit

Première version mesurée : avec `NK_LOG_LEVEL=debug` seul, en Release, le journal laissait passer
`debug` mais le puits console gardait son filtre `error` — **l'écran restait muet, et il fallait
DEUX variables**. C'était le même piège, un cran plus loin. Corrigé : poser `NK_LOG_LEVEL`
explicitement fait suivre le puits console, sauf si `NK_LOG_CONSOLE` dit autre chose.

---

## 8. Le banc, et son négatif

`Sandbox/System/NKLoggerSilence/` — un projet `consoleapp()` qui **mesure** au lieu de supposer :
type du descripteur `stdout`, présence d'une console, valeur de `NDEBUG` vue par le préprocesseur,
nombre de puits, niveau du journal, et **relecture du tampon d'écran de la console** (pour répondre
à « est-ce que l'utilisateur VOIT la ligne ? » sans photographier un écran).

```
python Sandbox/System/NKLoggerSilence/banc_silence.py
    → VERDICT : VERT (9 criteres)

python Sandbox/System/NKLoggerSilence/banc_silence.py --exe <binaire d'avant> --negatif
    → NEGATIF CONFORME : C1, C2 et C4 rougissent sur le binaire d'avant.
```

Le négatif est la moitié de la valeur du banc : sans lui, neuf critères verts ne prouvent rien.

⚠️ **Ordre des inclusions.** `NkLog.h` définit la **macro** `logger`. `NkRegistry.h` nomme `logger`
un **paramètre** (`Register(memory::NkSharedPtr<NkLogger> logger)`). Inclure `NkRegistry.h` **après**
`NkLog.h` ne compile pas. Le registre passe en premier.
