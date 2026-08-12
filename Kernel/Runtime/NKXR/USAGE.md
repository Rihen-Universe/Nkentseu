# NKXR — Guide d'utilisation

> Le runtime VR/AR/XR de Nkentseu, **écrit from scratch**, posé sur le loader
> OpenXR pour parler aux casques. Ce guide est fait pour être lu par quelqu'un
> qui n'a jamais touché à l'XR : il explique d'abord **pourquoi** les choses
> sont ainsi, puis **comment** s'en servir.
>
> État : VR complète (Quest 2 prouvé). L'AR aura son propre chapitre quand
> l'étage 3 sera livré. Feuille de route : [ROADMAP.md](ROADMAP.md).

---

## 1. Les cinq idées à comprendre avant d'écrire une ligne

**1. Une SESSION, pas un « mode VR ».** L'application n'allume pas la VR : elle
demande une session au runtime du casque, qui la fait passer par des états
(prête → synchronisée → visible → focalisée → arrêt). C'est le runtime qui
décide, parce que c'est lui qui gère l'utilisateur : casque posé, menu système
ouvert, batterie vide. Une application qui ignore ces états est tuée sans
sommation par le casque.

**2. Deux yeux = deux caméras, à la même date.** Chaque œil a sa position, son
orientation et son champ de vision — et ce champ est **décentré** (l'œil ne
regarde pas au milieu de son image, la lentille impose un frustum asymétrique).

**3. Le temps est une donnée d'entrée.** On ne demande jamais « où est la tête
maintenant ? » mais « où sera-t-elle à l'instant où l'image sera affichée ? ».
Le runtime donne cet instant (`predictedDisplayTime`) ; s'en servir est la
différence entre une image collée à la tête et une image qui traîne — c'est de
là que vient la nausée.

**4. Les entrées sont des ACTIONS, pas des touches.** L'application déclare
« sélectionner », « attraper », « se déplacer » ; le backend lie ces intentions
au matériel réel (gâchette Touch, stick, ou souris sur le simulateur). Une
application écrite ainsi tourne sans modification sur un casque, sur un autre,
et sur le simulateur.

**5. On rend chez soi, on REMET au compositeur.** L'application ne dessine pas
à l'écran du casque : elle remplit des images fournies par le runtime et les
lui rend, avec les poses utilisées. Le compositeur fait le reste (distorsion
des lentilles, reprojection si l'on est en retard).

---

## 2. Démarrer sans casque — le simulateur

Le backend simulateur existe pour qu'on **développe et teste l'XR sans
matériel** : fenêtre de bureau, souris = tête, clavier = déplacement. Tout ce
qui suit vaut aussi pour un vrai casque, sans changer une ligne.

```cpp
#include "NKXR/NKXR.h"
namespace nkxr = nkentseu::xr;

nkxr::NkXrSessionDesc desc;
desc.window = &window;                 // fenêtre hôte (simulateur)
// desc.backend = nkxr::NkXrBackendType::NK_XR_BACKEND_OPENXR;  // vrai casque
nkxr::NkXrSession *session = nkxr::NkXrSession::Create(desc);
```

Commandes du simulateur : **souris** = tête · **ZQSD/WASD** = déplacement ·
**Espace/C** = monter/descendre · **Maj** = sprint · **clic gauche** = action
« sélectionner » · **flèches** = stick de locomotion.

---

## 3. La boucle, dans l'ordre exact

L'ordre n'est pas négociable : le runtime cadence l'application, pas l'inverse.

```cpp
// (a) Le cycle de vie : le runtime commande, on obéit.
nkxr::NkXrEvent ev;
while (session->PollEvent(ev)) {
    if (ev.type != nkxr::NkXrEventType::NK_XR_EVENT_STATE_CHANGED) continue;
    switch (ev.state) {
        case nkxr::NkXrSessionState::NK_XR_STATE_READY:    session->Begin(); break;
        case nkxr::NkXrSessionState::NK_XR_STATE_STOPPING: session->End();   break;
        case nkxr::NkXrSessionState::NK_XR_STATE_EXITING:  running = false;  break;
        default: break;
    }
}

// (b) Attendre SON tour : c'est ici que le casque impose sa cadence.
nkxr::NkXrFrameState frame;
if (!session->WaitFrame(frame)) continue;
session->BeginFrame();

// (c) Où seront les yeux QUAND l'image s'affichera.
nkxr::NkXrSpace stage(nkxr::NkXrSpaceType::NK_XR_SPACE_STAGE);
nkxr::NkXrView views[nkxr::NK_XR_EYE_COUNT];
session->LocateViews(stage, frame.predictedDisplayTime, views);

// (d) Rendre un œil à la fois, puis remettre les deux au compositeur.
if (frame.shouldRender) {
    // ... rendre views[0] et views[1] dans deux cibles ...
    session->SubmitEyes(views, imageGauche, imageDroite, largeur, hauteur);
}

// (e) TOUJOURS terminer, même sans rendu : la boucle doit rester synchronisée.
nkxr::NkXrFrameEndInfo endInfo;
endInfo.displayTime = frame.predictedDisplayTime;
session->EndFrame(endInfo);
```

⚠️ **`shouldRender` faux n'autorise pas à sauter `EndFrame`** — casque posé,
application masquée : on soumet une frame vide, sinon le runtime considère
l'application perdue.

### Brancher une caméra du moteur sur un œil

```cpp
NkCamera3DData cd;
cd.position  = views[eye].position;
cd.target    = cd.position + nkxr::NkXrForward(views[eye].orientation);
cd.up        = nkxr::NkXrUp(views[eye].orientation);
cd.nearPlane = 0.05f;  cd.farPlane = 200.f;
// Le frustum DÉCENTRÉ du casque, consommé tel quel (NKRenderer sait le faire) :
cd.useFovAsym = true;
cd.fovLeft = views[eye].fov.angleLeft;   cd.fovRight = views[eye].fov.angleRight;
cd.fovUp   = views[eye].fov.angleUp;     cd.fovDown  = views[eye].fov.angleDown;
```

**Convention spatiale** (identique OpenXR et NKXR, donc aucune conversion) :
main droite, **l'avant regarde −Z**, +Y vers le haut, +X à droite, **unité = le
mètre**. Attention : `NkQuat::Forward()` du moteur rend +Z — utiliser
`NkXrForward/NkXrUp/NkXrRight`, qui fixent la convention une fois pour toutes.

### Les trois espaces, et lequel choisir

| Espace | Origine | Pour quoi |
|---|---|---|
| `NK_XR_SPACE_VIEW` | entre les deux yeux, suit la tête | réticule, HUD collé au visage |
| `NK_XR_SPACE_LOCAL` | la tête au moment du `Begin()` | expérience assise |
| `NK_XR_SPACE_STAGE` | **le SOL**, au centre de la zone de jeu | roomscale — le seul où `y = 0` est le plancher |

Pour poser une scène à l'échelle humaine : **STAGE**. S'accroupir change alors
réellement la hauteur des yeux.

---

## 4. Les entrées : déclarer des intentions

```cpp
nkxr::NkXrActionSet actions;
auto select = actions.CreateAction({ "selectionner",
    nkxr::NkXrActionType::NK_XR_ACTION_BOOL,  nkxr::NkXrActionUsage::NK_XR_USAGE_SELECT });
auto aim    = actions.CreateAction({ "viser",
    nkxr::NkXrActionType::NK_XR_ACTION_POSE,  nkxr::NkXrActionUsage::NK_XR_USAGE_AIM_POSE });
auto move   = actions.CreateAction({ "deplacer",
    nkxr::NkXrActionType::NK_XR_ACTION_VEC2,  nkxr::NkXrActionUsage::NK_XR_USAGE_MOVE });
auto buzz   = actions.CreateAction({ "vibrer",
    nkxr::NkXrActionType::NK_XR_ACTION_HAPTIC, nkxr::NkXrActionUsage::NK_XR_USAGE_HAPTIC });
session->AttachActionSet(actions);   // FIGE le jeu : une seule fois, avant Begin
```

Puis, à chaque frame : `session->SyncActions();` suivi des lectures.

```cpp
nkxr::NkXrActionStateBool s;
session->GetActionStateBool(select, s);
if (s.changed && s.current) {                 // 'changed' = un FRONT, pas un niveau
    session->ApplyHaptic(buzz, 0.7f, 0.06f);  // amplitude [0,1], durée en secondes
}
nkxr::NkXrPose main;
session->LocateActionPose(aim, stage, frame.predictedDisplayTime, main);
```

**Correspondance des usages** — les usages sans suffixe désignent la main
**droite**, ceux en `_LEFT` la **gauche** ; les deux mains sont servies à
égalité (un pupitre, un volant, un levier se manipulent à deux mains).

| Usage | Manette Touch | Simulateur |
|---|---|---|
| `SELECT` / `SELECT_LEFT` | gâchette droite / gauche | clic gauche / clic milieu |
| `GRAB` / `GRAB_LEFT` | grip droit / gauche | clic droit / Maj gauche |
| `AIM_POSE` / `_LEFT` | visée droite / gauche | main simulée, décalée du bon côté |
| `GRIP_POSE` / `_LEFT` | paume droite / gauche | idem, plus près du corps |
| `HAPTIC` / `HAPTIC_LEFT` | vibration droite / gauche | — |
| `MOVE` / `MOVE_RIGHT` | stick gauche / droit | flèches / IJKL |
| `MENU` | bouton menu gauche | Tab |

**Accrocher un objet à la main** : la pose est un simple point d'ancrage 6DoF —
y placer n'importe quel maillage (arme sur `AIM_POSE` dont l'axe est la visée,
gant ou levier sur `GRIP_POSE` qui est la paume) est un handle de maillage à
changer, rien de plus.

### Les vraies mains, sans manettes

```cpp
nkxr::NkXrHand hand;
if (session->LocateHand(nkxr::NkXrHandSide::NK_XR_HAND_RIGHT, stage,
                        frame.predictedDisplayTime, hand)) {
    for (uint32 j = 0; j < nkxr::NK_XR_HAND_JOINT_COUNT; ++j)   // 26 articulations
        if (hand.joints[j].valid) { /* position, orientation, radius */ }
}
```
Rend `false` si le runtime n'a pas l'extension **ou** si la main n'est pas vue :
retomber sur les manettes est le comportement normal, pas une erreur.
⚠️ Sur Quest via Link, activer « Fonctionnalités du runtime pour développeurs »
(Meta Horizon → Paramètres → Bêta) — sinon l'extension n'est pas offerte.

---

## 5. Le vrai casque : ce qui change

Un seul renversement : **la session vient AVANT le device graphique**, parce
que le runtime impose ses extensions Vulkan et son GPU.

```cpp
nkxr::NkXrVulkanRequirements reqs;
session->GetVulkanRequirements(reqs);        // AVANT de créer le device
// → poser reqs.instanceExtensions / deviceExtensions dans NkVulkanDesc,
//   et le crochet pickPhysicalDevice qui appelle GetVulkanPhysicalDevice()
// ... créer le device NKRHI ...
nkxr::NkXrVulkanBinding binding{ instance, physicalDevice, device, familleQueue, 0 };
session->BindVulkan(binding);                // → la session de casque naît ici
session->CreateHmdSwapchains(largeur, hauteur);
```

Ensuite, `SubmitEyes(...)` par frame : il copie les images vers celles du
runtime et prépare la couche de projection. Le reste du code (poses, actions,
boucle) est **identique au simulateur**.

---

## 6. Réglages — les crochets d'environnement

| Variable | Effet | Défaut |
|---|---|---|
| `NK_XR_BACKEND=openxr` | tenter un vrai casque (sinon simulateur) | simulateur |
| `NK_XR_RENDER_SCALE` | résolution de rendu × recommandé du casque | `0.85` |
| `NK_XR_HZ` | cadence d'affichage souhaitée (la plus proche des proposées) | runtime |
| `NK_XR_HALF_RATE=0/1` | verrou demi-cadence (régularité si le moteur ne tient pas) | `1` |
| `NK_XR_SPECTATOR=0/1` | fenêtre PC : vue du porteur / stéréo côte à côte | vue du porteur |
| `NK_XR_TAA` | anti-aliasing temporel (un historique par œil) | `1` |
| `NK_XR_DEPTH_LAYER` | soumettre la profondeur (reprojection positionnelle) | `1` |
| `NK_XR_SSAO`, `NK_XR_BLOOM` | effets écran (coupés en VR par défaut) | `0` |
| `NK_XR_SHADOW`, `NK_XR_SHADOW_RES`, `NK_XR_SHADOW_CASCADES` | coût des ombres | `1`, `1024`, `2` |
| `NK_XR_LIST_EXT=1` | lister les extensions offertes par le runtime | — |
| `NK_XR_SYNC_COPY=1` | attente immédiate de la copie (diagnostic) | — |
| **Simulateur** | | |
| `NK_XR_SIM_POSE="yaw,pitch,x,y,z"` | fige la tête (captures reproductibles) | libre |
| `NK_XR_SIM_FOV="l,r,u,d"` | FOV asymétrique de test (degrés signés) | symétrique |
| `NK_XR_SIM_IPD`, `NK_XR_SIM_EYE_HEIGHT`, `NK_XR_SIM_SPEED`, `NK_XR_SIM_HZ`, `NK_XR_SIM_LATENCY_MS` | réglages du casque simulé | 0,063 · 1,70 · 2,5 · 72 · 0 |

---

## 7. Mesurer — et ne jamais deviner

```cpp
nkxr::NkXrPerfMetrics m;
if (session->GetPerfMetrics(m) && m.available) { /* m.appCpuMs, m.appGpuMs, ... */ }
```

C'est cette mesure qui a fait passer la démo de **40 à 70 images/seconde** : les
compteurs montraient `app CPU 10-24 ms` pour `app GPU 0,01 ms`, donc un CPU
bloqué et non un GPU saturé. **Toujours mesurer avant d'optimiser.**

Repères mesurés (Quest 2 via Link + RTX 3070 Laptop bridée à 1100 MHz) :

| Échelle | Par œil | Cadence | app CPU / GPU |
|---|---|---|---|
| 0,8 | 1664×1676 | 65-70 i/s | 5-7 ms / 4 ms |
| **0,9** | **1872×1886** | **68-71 i/s** | 9-11 ms / 11 ms |
| 1,0 | 2080×2096 | 32-42 i/s | 11-12 ms / 7-18 ms |

### Anti-aliasing : ce qui marche, ce qui ne marche pas (mesuré)

**Réglage retenu : TAA + échelle 0,85** (les deux par défaut). Chemin parcouru,
tout mesuré sur Quest 2 via Link + RTX 3070 Laptop bridée à 1100 MHz :

| Configuration | Par œil | Cadence | GPU app | Verdict |
|---|---|---|---|---|
| TAA + 0,60 | 1248×1257 | 66-68 i/s | 3,1 ms | lisse mais **flou** |
| TAA + 0,75 | 1560×1572 | 68-69 i/s | 4,7 ms | bon compromis |
| **TAA + 0,85** | **1768×1781** | **66-70 i/s** | **6,1 ms** | ✅ **retenu** — net ET lissé |
| TAA + 0,90 | 1872×1886 | 21-25 i/s | 27,7 ms | ⛔ **falaise** (voir ci-dessous) |
| sans TAA + 0,90 | 1872×1886 | 68-71 i/s | 11 ms | net mais **crénelé** |
| sans TAA + 1,00 | 2080×2096 | 32-42 i/s | 18 ms | le GPU décroche |

⚠️ **La falaise entre 0,85 et 0,90 avec TAA n'est pas une pente** : le coût GPU
passe de 6 à 27 ms pour 12 % de pixels en plus. Un seuil est franchi (mémoire
vidéo ou cache) — à ne pas confondre avec un coût qui monterait doucement. Sur
une machine à plus de VRAM, ce seuil sera ailleurs : **remesurer plutôt que
recopier ces chiffres**.

| Autre technique | Verdict |
|---|---|
| **MSAA** | ⛔ n'existe pas dans le moteur : `NkRendererConfig::msaaSamples` est un champ **jamais lu**. Le brancher est un chantier (cibles multi-échantillonnées + résolution dans les passes, 4 backends) — note de coordination posée dans la ROADMAP de NKRenderer. |
| **FXAA** | 🔶 actif, utile en complément, insuffisant seul contre le scintillement d'un casque. |

### Peut-on monter à « 2K ou 4K » par œil ?

Oui — mais c'est une question de MACHINE, pas de moteur. `NK_XR_RENDER_SCALE`
n'a pas de plafond logiciel (borné à 2,0 par prudence) : le Quest 2 recommande
déjà 2080×2096 par œil, soit **8,7 mégapixels par image** pour les deux yeux —
davantage qu'un écran 4K (8,3 Mpx), et **72 fois par seconde**. La limite
mesurée ici est celle d'une RTX 3070 Laptop **bridée à 1100 MHz** (bridage de
sécurité contre les extinctions machine). Trois leviers, dans l'ordre :
1. **lever le bridage GPU** — le plus direct, si la machine le supporte ;
2. **rendu deux-vues en une passe** (ombres et culling calculés une fois au
   lieu de deux) : aujourd'hui presque tout est payé deux fois ;
3. **masque de visibilité** : 15-25 % des pixels ne sont jamais vus à travers
   la lentille — les récupérer finance directement la résolution.
Les points 2 et 3 sont des chantiers NKRenderer, déjà notés dans sa ROADMAP.

---

## 8. Symptômes et causes (tous vécus, tous mesurés)

| Ce qu'on voit | Cause réelle | Remède |
|---|---|---|
| Fenêtre blanche qui ne s'ouvre jamais | la boucle attend un état que le backend n'émettra pas | vérifier le journal `[NKXR]` — la sonde dit toujours où elle s'arrête |
| Déchirures/saccades dans le casque, **miroir PC propre** | cadence irrégulière : le compositeur alterne ses modes de reprojection | mesurer (§7) ; verrou demi-cadence en attendant |
| Taches noires scintillantes sur les modèles | SSAO (effet écran) sous les micro-mouvements de tête | `NK_XR_SSAO=0` (défaut en VR) |
| Bords d'ombre qui clignotent en se déplaçant | cascades recalées sur la caméra à chaque frame | `autoFitDirectional = true` (cascade ancrée au monde) |
| Image délavée après une copie vers le casque | double encodage sRGB | copie **brute**, jamais de conversion de format |
| La tête dérive alors que la souris ne bouge pas | delta souris brut jamais remis à zéro par l'état global | NKXR accumule ses propres événements bruts |
| « Source inconnue » au casque | l'application n'est pas du magasin | Meta Horizon → Paramètres → activer les sources inconnues |
| Le correctif « ne marche pas » | l'exécutable était **verrouillé** par la démo ouverte pendant la compilation | fermer la démo, vérifier la date du binaire |

---

## 9. Ce qui existe, et ce qui vient

**Aujourd'hui** : session complète, espaces, poses prédites, stéréo (frustums
décentrés), swapchains et couche de projection, manettes (états, poses 6DoF,
haptique), vraies mains (26 articulations), métriques, choix de cadence, masque
de visibilité, vue moniteur, et le simulateur pour tout faire sans casque.

**Ensuite** : anti-aliasing (MSAA par œil), rendu deux-vues en une passe,
intégration à Noge (l'XR comme capacité du framework), main gauche symétrique,
postes distants (état répliqué NKNetwork ou flux H.264 NKMedia), APK Quest
natif — puis **l'AR** (caméra plein écran, marqueurs from scratch, IMU),
qui recevra son propre chapitre dans ce guide.
