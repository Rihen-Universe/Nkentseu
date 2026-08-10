# MISSION IA — NKXR : le système VR/AR/XR de Nkentseu, from scratch

> Document de passation destiné à **une IA quelconque**. Décidé par Rihen le
> 10 août 2026. Dépôt principal : `D:\Projets\2026\Nkentseu\Nkentseu` — mais
> CET AGENT TRAVAILLE DANS UN WORKTREE (voir §2, règle absolue).
> Objectifs de Rihen : **AR** = s'adapter au téléphone (Android d'abord,
> iPhone et HarmonyOS ensuite) ; **VR** = casques type Quest/Pico ;
> **XR** = les deux. Matériel disponible : un **Meta Quest 2**, un téléphone
> **Android**. (iPhone et Harmony : achat prévu, pas encore là. Un Pico
> viendra comme preuve de portabilité.)

---

## 1. LA PHILOSOPHIE — « from scratch » qui tient debout

Nkentseu écrit tout lui-même (zéro STL, codecs maison, RHI maison). Pour
l'XR, le « from scratch » RÉALISTE se définit ainsi :

- **NOTRE runtime NKXR** : sessions, espaces, poses, prédiction, entrées,
  couches de composition, swapchains par œil — notre code, notre style,
  zéro middleware (pas d'Unity XR, pas d'OpenVR, pas de plugin).
- **MAIS le matériel VR ne parle qu'OpenXR** : Quest et Pico n'exposent QUE
  ce protocole. Réinventer le protocole HMD n'est pas un chantier, c'est
  une impasse. NKXR se pose donc DIRECTEMENT sur le **loader OpenXR**
  (Khronos, Apache 2.0 — licence compatible, à consigner dans
  `THIRD_PARTY_LICENSES.md` selon `docs/SOURCES_TIERCES.md`), comme nos
  codecs se posent sur les specs des formats.
- **AR téléphone** : le tracking SLAM/VIO complet est un projet de RECHERCHE
  (des mois). Le chemin honnête, étagé : d'abord la caméra (NKCamera sait
  déjà capturer multi-OS) + **AR à marqueurs from scratch** (détection de
  motifs — vision classique, faisable) + fusion IMU ; le SLAM viendra plus
  tard, et c'est un sujet en or pour NKAI.
- **Ne jamais surévaluer** : chaque étage est annoncé pour ce qu'il est.

## 2. RÈGLES ABSOLUES (le dépôt les a payées cher)

1. **WORKTREE OBLIGATOIRE** — tu ne travailles JAMAIS dans le dossier
   principal :
   ```
   git worktree add ../Nkentseu-xr -b feat/nkxr origin/main
   ```
   Deux `jenga build` simultanés dans le MÊME arbre corrompent `Build/Obj`
   (binaires qui crashent absurdement — incident vécu, une heure perdue).
   Ton worktree a SON Build/, tu ne touches jamais à celui du principal.
2. Cycle TEST → VALIDATION → INTÉGRATION du `CLAUDE.md` racine : tu livres
   un bloc de test depuis TON worktree ; l'intégration dans main ne se fait
   qu'après validation de Rihen.
3. Français partout ; **zéro STL** (NkString, NkVector, allocateurs
   NKMemory) ; commentaires qui expliquent le POURQUOI ; commits via
   `./gitcommit.sh "msg" <chemins explicites>` (il commite par pathspec).
4. **Carnet de bord** : `Applications/NKXRDemo/CARNET.private.md` (ou à la
   racine de ton module), au fil de l'eau, jamais poussé.
5. Lire avant de commencer : `ARCHITECTURE.md`, `CLAUDE.md` (racine),
   `docs/SOURCES_TIERCES.md`, `Kernel/Runtime/NKRHI/ROADMAP.md`,
   `Kernel/Runtime/NKCamera/ROADMAP.md`.

## 3. CE QUE LE MOTEUR T'OFFRE DÉJÀ

| Brique | État | Usage XR |
|---|---|---|
| NKRHI | 6 backends, Vulkan validé | Vulkan = la voie des casques ; le rendu par œil passera par lui |
| NKWindow | Android RÉEL (APK signé fonctionne), Harmony compilé (stubs) | l'hôte AR téléphone |
| NKCamera | capture multi-OS livrée (P1+P2) | le flux vidéo AR |
| NKMath | quaternions, matrices propres | poses, espaces, prédiction |
| NKEvent | gamepad, capteurs à étendre | entrées, IMU |
| NKRenderer | PBR, ombres, post | la scène rendue en stéréo (chantier multiview À COORDONNER) |

## 4. LE PLAN PAR ÉTAGES — chaque étage est un livrable seul

### Étage 0 — le module NKXR + SIMULATEUR desktop (AUCUN matériel requis)
`Kernel/Runtime/NKXR/` : l'abstraction complète —
`NkXrSession` (cycle de vie), `NkXrSpace` (stage/local/view),
`NkXrPose` (position+quaternion, horodatée, prédite), `NkXrInput`
(actions abstraites, pas des touches), `NkXrSwapchain` (une par œil),
`NkXrLayer` (projection, quad). **Backend n°1 : le SIMULATEUR** — une
fenêtre desktop en stéréo côte à côte, souris = orientation de la tête,
WASD = déplacement, la latence simulée. TOUT le développement et TOUS les
tests d'agent se font là : c'est ce qui force la bonne abstraction, et ça
se vérifie par captures comme le modeleur (crochets NK_*).
**Livrable : une démo `NKXRDemo` qui affiche une scène NKRenderer en stéréo
simulée, tête pilotée à la souris.**

### Étage 1 — le rendu STÉRÉO dans NKRenderer (COORDINATION REQUISE)
Deux yeux = deux matrices vue/projection, un render graph qui rend deux
fois (ou multiview Vulkan). C'est un chantier DANS NKRenderer — à
coordonner avec l'agent du modeleur (note de coordination dans
`Kernel/Runtime/NKRenderer/ROADMAP.md`, ne pas modifier ses passes sans
prévenir). V1 acceptable : deux passes séparées vers deux cibles.

### Étage 2 — VR réelle : backend OpenXR → Quest 2
Le loader OpenXR (Khronos) entre dans `Externals/` avec sa licence au
registre. Backend `NkXrBackendOpenXR` : instance, session, espaces,
swapchains Vulkan, frame timing (xrWaitFrame/xrBeginFrame/xrEndFrame),
entrées par actions. Cible : le **Quest 2 de Rihen** (Android + OpenXR,
l'APK passe par la chaîne jenga Android existante). Le Pico viendra
ensuite : même code, second runtime = preuve de portabilité.

### Étage 3 — AR téléphone (Android d'abord)
1. Caméra plein écran via NKCamera + rendu 3D par-dessus (composition).
2. **Marqueurs from scratch** : détection d'un motif imprimé (seuillage,
   quads, homographie — vision classique, NKImage décode déjà tout),
   pose 6DoF depuis les coins (PnP). C'est LE from-scratch réaliste.
3. IMU (gyroscope/accéléromètre via NKEvent à étendre côté Android) pour
   stabiliser entre deux détections.
4. SLAM/VIO : PLUS TARD, chantier recherche, en lien avec NKAI.
iPhone/Harmony : quand le matériel arrive — l'abstraction doit être prête.

### Étage 4 — XR : la composition
Passthrough caméra + scène (AR sur casque quand le matériel le permettra),
et l'API unifiée NKXR qui sert les deux mondes.

## 5. TESTS — l'agent se vérifie SEUL aux étages 0-1
- Simulateur : captures + diff pixel (le rendu du moteur est déterministe,
  bruit mesuré = 0 — cf. la boucle d'agent du modeleur, à imiter).
- Poses : tests numériques (aller-retour quaternion/matrice, prédiction).
- Étage 2+ : blocs de test pour Rihen (APK à installer sur le Quest 2 —
  mode développeur requis sur le casque).

## 6. LE PROMPT — à copier tel quel pour l'agent

```
Tu travailles sur le moteur Nkentseu (C++ maison, zéro STL). Ta mission :
NKXR, le système VR/AR/XR from scratch du moteur. LIS D'ABORD
XR_MISSION_IA.md à la racine du dépôt principal — il contient la
philosophie (notre runtime, posé sur le loader OpenXR — seul accès réaliste
aux casques), le plan par étages, les règles absolues et l'état du moteur.

RÈGLE N°1 : crée ton WORKTREE (git worktree add ../Nkentseu-xr -b feat/nkxr
origin/main) et n'en sors JAMAIS — deux builds dans le même arbre
corrompent les objets (incident vécu). Travaille et réponds en FRANÇAIS,
zéro STL, commentaires qui disent le POURQUOI, carnet de bord au fil de
l'eau.

COMMENCE PAR L'ÉTAGE 0 : le module Kernel/Runtime/NKXR (session, espaces,
poses, entrées par actions, swapchains par œil) avec son backend SIMULATEUR
desktop (stéréo côte à côte, souris = tête) et la démo NKXRDemo qui rend
une scène NKRenderer en stéréo simulée. Teste par captures et diffs comme
la boucle d'agent du modeleur. Ne passe à l'étage suivant qu'après
validation de Rihen. L'étage 1 (stéréo dans NKRenderer) exige une NOTE DE
COORDINATION dans la ROADMAP de NKRenderer avant de toucher à ses passes.
Matériel disponible pour l'étage 2 : un Meta Quest 2 (Android + OpenXR).
Ne jamais surévaluer : chaque étage est annoncé pour ce qu'il est.
```
