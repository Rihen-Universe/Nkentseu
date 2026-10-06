# NKAnima — la physique d'animation en temps réel (`src/NKAnima/Realtime/`)

> 06/10/2026. Ce que le jeu (Noge), le séquenceur (NKScena) et le simulateur (PV3DE)
> appellent **à chaque image**, sans dépendre d'aucun moteur physique. La **fabrication**
> (corriger toute une animation, l'auto-pose de corps entier, cuire le secondaire) n'est
> pas ici : elle est dans l'atelier privé `Kernel/AnimaAtelier` (NKAnimaAtelier).

## Les briques

| Brique | Ce qu'elle fait |
| --- | --- |
| `NkRtRig` | la vue du squelette (ordre en profondeur, sous-arbres) : chaque solveur ne recalcule que le sous-arbre qu'il tourne |
| `NkSpringBones` | le **secondaire** : chaînes d'os suiveurs (raideur, amortissement relatif, traînée, gravité, angle maximal, sphères, sol) ; intégration **implicite** en sous-pas, en coordonnées relatives à l'ancre |
| `NkLiveBalance` | l'**équilibre** : le centre de masse (Dempster par les noms) ramené au-dessus du polygone d'appui en glissant le bassin, lissé, borné |
| `NkFootIK` | l'**IK des pieds** sur sol irrégulier : lancer de rayon de l'hôte sous la cheville et l'orteil, bassin qui descend (au sol le plus bas, et assez pour que chaque jambe reste à portée), IK à deux os, pied incliné sur la pente au prorata de son appui |
| `NkRagdollBlend` | la **bascule** animation ↔ ragdoll : un ragdoll PBD léger (particules, os rigides, flexions bornées, bassin et poitrine rigides, sol de l'hôte, frottement), démarré **de la pose et de la vitesse** de l'animation ; fondu ; **relèvement** recalé là où le corps est tombé |
| `NkRtCharacter` | les quatre, dans l'ordre : FK (recalée) → équilibre → IK des pieds → ragdoll → ressorts |

## Brancher un personnage

```cpp
#include "NKAnima/Realtime/NkRealtime.h"
using namespace nkentseu::anim::rt;

NkRtCharacter perso;
perso.Setup(parents, bindLocal, noms, n);          // une fois ; jambes, bassin, chaînes trouvés par les NOMS

NkRtGround sol;                                    // le sol de l'HOTE
sol.raycast = &MonLancer;                          // bool (void*, origine, dir, maxDist, NkRtRayHit&)
sol.user = &maScene;

// chaque image : la pose de l'animation (locale ; la racine porte la place dans le monde)
perso.Update(animLocal, solDeLAnimationY, sol, dt);
const NkMat4f *monde = perso.World();              // pour la peau : monde[j] * inverseBind[j]

perso.Hit(poitrine, NkVec3f{0, 0.6f, -3.4f});      // un choc : le ragdoll prend la main
```

Conventions : mètres, +Y en haut, `world[j] = world[parent] * local[j]`. Les noms reconnus :
`foot/ankle` (jambes : parent = genou, grand-parent = hanche, premier enfant = orteil) ;
`ponytail, hair, tail, spring, jiggle, cape, antenna` (chaînes secondaires) ; les masses par
`head/neck/spine/chest/hip/arm/thigh/shin/foot...` (NkPoseMass). Sans noms : `NkFootIK::AddLeg`,
`NkSpringBones::AddChain`, masse uniforme.

Pendant le fondu vers le ragdoll (0,1 s), l'hôte **continue** l'animation en cours ; une fois
le corps à terre, il joue son animation de relèvement (ventre ou dos : `ragdoll.FaceUp()`) ;
le recalage (`ragdoll.Relocation()`) reste appliqué, ou l'hôte le reprend dans sa propre
transformation (`TakeRelocation`).

## Le banc (`NKAnima_Tests`, `tests/test_temps_reel.cpp`)

Chaque critère est mesuré et a sa mutation, dans le même binaire (`NK_RT_MUTATION=nom`) :
ressort qui converge sans exploser (`spring_explicit`), IK des pieds sur une marche et dans un
creux (`footik_off`, `footik_no_pelvis`), bascule ragdoll sans saut de pose et vitesse gardée
(`ragdoll_from_bind`, `ragdoll_no_velocity`), relèvement sans saut (`getup_snap`), équilibre
(`balance_off`), coût par image (`cost`). Coût mesuré (médiane, 23 os, un personnage) :
marche (équilibre + IK des pieds + ressorts) **3,4 µs en Release**, ~27 µs en Debug -O0 ;
ragdoll **15 µs en Release**, ~100 µs en Debug (budget du banc, en Debug : 150 et 300 µs).

## Note de branchement — PV3DE (pour samedi, rien n'est branché)

PV3DE n'a **pas** été touché. Pour l'y brancher :

1. **Le squelette** du patient : `perso.Setup(parents, bindLocal, noms, n)` depuis son actif
   (NkSkeletonDef : `ParentVector()`, `BindLocal(j)`, `bones[j].name`). Si ses os ne portent
   pas les noms anglais courants (foot, thigh, spine...), déclarer les jambes par
   `footIK.AddLeg` et les chaînes par `springs.AddChain`, et poser les masses à la main
   (`balance.Mass()`).
2. **Le sol** : un `NkRtRaycastFn` sur la géométrie de la scène PV3DE (sol, escalier, lit,
   brancard). Sans lancer, un plan `planeY`.
3. **Chaque image**, après le lecteur d'animation : `perso.Update(local, solY, sol, dt)`,
   puis la peau depuis `perso.World()`.
4. **Les cas du simulateur** : la syncope ou la chute (un `Hit` d'impulsion nulle : le corps
   s'effondre avec la vitesse qu'il avait, sans saut de pose) ; le relèvement assisté (l'animation
   de relèvement jouée par PV3DE, recalée par le ragdoll) ; la station debout fragile
   (`balance.settings.targetMargin` plus grand, `maxShift` plus petit) ; l'escalier (l'IK des
   pieds, `maxStepUp`) ; les cheveux, une sangle, un drain (chaînes secondaires nommées).
5. **Le coût** : un personnage coûte quelques dizaines de µs par image ; `useBalance`,
   `useFootIK`, `useSprings` éteignent chaque étage.
6. **Ce qu'il ne faut pas faire** : appeler `Update` deux fois par image (les lissages et les
   ressorts ont un état), ou modifier `animLocal` après l'appel (les sous-arbres sont
   rafraîchis depuis lui).
