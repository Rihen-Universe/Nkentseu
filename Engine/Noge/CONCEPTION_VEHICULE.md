# Noge — physique de véhicule : la conception, écrite avant le code

> Mandat de Rodolf : *« le jeu de voiture, si et seulement si la physique du
> véhicule est déjà prête. »* La physique d'abord, le jeu après.
> **Rien n'est codé à ce stade.** Ce document est ce qui se discute avant.
> Mesures du 2026-09-03, arbre `Nkentseu-noge`.

---

## 1. 📏 CE QUE LE SOCLE PORTE DÉJÀ — mesuré, pas supposé

`NKPhysics` : **1 571 lignes**, et j'ai cherché **où vit le corps** plutôt que de
compter des suffixes — 2 `.cpp` (958 l.), 9 `.h` (613 l.), **zéro `.inl`**. Les
en-têtes sont bien des déclarations : seuls `NkPhysicsWorld.h` (11 corps),
`NkRagdoll.h` (11) et deux autres portent de l'inline.

| ce dont une voiture a besoin | état | où |
|---|---|---|
| **pas de temps fixe** | ✅ **déjà là, et correct** | `NkPhysicsWorld::Advance(realDt)` accumule et exécute des `Step(h)` à `fixedTimeStep`, plafonné par `maxSubSteps` |
| **raycast avec point + normale** | ✅ | `Raycast(ray, outBody, hit, layerMask)` → `NkRayHit3D{hit, t, point, normal, bodyId}` |
| corps rigides, intégrateur, contacts | ✅ | `NkIntegrator`, `SolveContacts` (impulsions séquentielles + warm-start) |
| articulations, moteurs, limites | ✅ | `CreateRevoluteJoint`, `SetRevoluteMotor` (PD), `SetRevoluteLimit` |
| matériaux, `OverlapShape`, sommeil | ✅ | — |
| **force au centre de masse** | ✅ | `NkRigidBody::ApplyForce(f)` |
| **force EN UN POINT** | 🔴 **absente** | — |
| **couple** | 🔴 **le champ existe, il n'est JAMAIS intégré** | voir §2 |
| véhicule, roue, suspension | ❌ **rien** | le seul « roue » du module est un commentaire de `NkJoint.h:25` |

> ✅ **Le premier piège est déjà désamorcé par le socle.** *« Une suspension
> explose sous un pas variable »* — c'est vrai, et `Advance()` fait déjà la
> sous-cadence fixe. **Il n'y a rien à écrire ici, seulement à s'y brancher
> correctement** : les forces de roue doivent être calculées **dans** le pas
> fixe, jamais dans la boucle de jeu à cadence variable. C'est une contrainte
> d'architecture, pas du code en plus.
> *La porte de la maison s'applique : avant d'écrire un mécanisme, chercher qui
> le porte déjà — et il le portait.*

---

## 2. 🔴 LE BLOCAGE RÉEL — `torque` est écrit, remis à zéro, et jamais intégré

`NkRigidBody` porte un champ `torque`. `NkIntegrator.cpp:20` le **remet à zéro**
à chaque pas. Et entre les deux, **personne ne l'intègre** :

```cpp
NkVec3f accel = b.force * b.invMass;              // ← la force agit
...
b.linearVelocity = b.linearVelocity + accel * dt;
b.angularVelocity = b.angularVelocity * (1/(1+damping*dt));   // ← QUE de l'amortissement
b.force  = {0,0,0};
b.torque = {0,0,0};                               // ← effacé sans avoir servi
```

**C'est la famille du déclaré-inerte**, et c'est bloquant pour un véhicule : une
roue applique sa force **en un point**, à distance du centre de masse. Sans
couple, la caisse ne peut ni **plonger au freinage**, ni **rouler en virage**,
ni **lacet** sous la poussée des roues. La voiture glisserait comme un patin
rigide.

⚠️ **Et le contact solver, lui, fait bien de l'angulaire** — il travaille en
impulsions directes sur `angularVelocity`, sans passer par `torque`. **Donc le
défaut ne se voit nulle part aujourd'hui** : les contacts font tourner les
objets, personne n'a jamais eu besoin d'appliquer un couple continu. *Un champ
inerte entouré de code qui marche est invisible jusqu'au premier usage.*

**Ce qu'il faut, et c'est petit** :

1. **intégrer le couple** dans `NkIntegrateVelocity` :
   `angularVelocity += invInertiaWorld(b) * torque * dt`, **avant** l'amortissement ;
2. **faire remonter `NkInvInertiaApply`** de `NkPhysicsWorld.cpp:402` (où elle est
   `static`, donc invisible ailleurs) vers `NkRigidBody.h`. *Le manque est dans
   le socle, on le comble dans le socle* — pas de copie dans le véhicule ;
3. **ajouter `ApplyForceAtPoint(f, pWorld)`** :
   `force += f; torque += (pWorld - position) × f;`

⚠️ **Un banc de non-régression AVANT** : le couple intégré change la dynamique
angulaire de **tous** les corps. Les bancs existants (ragdoll, `NkAnimPhysTest`,
`NkSystemsRevivalTest`) doivent revenir **au même compte**. *Un socle qu'on
corrige pour un client casse les autres en silence si personne ne les recompte.*

---

## 3. 🛞 LE MODÈLE DE ROUE — un raycast, trois forces

Modèle **raycast** (celui d'Unity `WheelCollider`, de PhysX et de Rocket League),
pas une roue en corps rigide. **Pourquoi** : une roue rigide sur joint revolute
demande un solveur à petit pas pour ne pas vibrer, et rend le comportement
difficile à régler. Le raycast donne un contrôle direct et **stable**.

Chaque roue, à chaque sous-pas fixe :

### a. Contact — un rayon vers le bas

```
origine  = position monde de l'ancrage de suspension
direction= -up(chassis)
longueur = restLength + rayonRoue
```
Pas de contact → la roue est en l'air : **aucune force**, et on le dit
(`grounded = false`), au lieu de laisser une force fantôme.

### b. Suspension — ressort + amortisseur, force le long de la normale

```
compression = (longueurMax - distanceTouche) / longueurMax     ∈ [0,1]
vitesseComp = (compressionPrec - compression) / h              (dérivée mesurée)
Fsusp       = raideur * compression  -  amortissement * vitesseComp
```

⚠️ **Trois gardes, et chacune répare une panne connue** :
- **`Fsusp` jamais négative** — un ressort ne **tire** pas la voiture vers le sol ;
- **plafonnée** à un multiple du poids porté — sinon un enfoncement d'une image
  éjecte la voiture au ciel ;
- **la vitesse de compression se mesure par différence entre sous-pas**, pas par
  la vitesse du corps projetée : c'est ce qui rend l'amortisseur stable quand la
  caisse tourne.

### c. Adhérence — c'est ici que tout se joue

**Le deuxième piège de Rodolf, mot pour mot** : *trop simple, la voiture glisse
comme sur du verre ; trop raide, elle vibre.* Les deux échecs ont la **même**
cause : une force de frottement calculée **en force** au lieu d'être calculée
**en impulsion bornée**.

> 🔑 **La parade : raisonner en VITESSE À ANNULER, puis borner par le cercle de
> friction.** On ne demande pas « quelle force ? » mais « quelle impulsion
> annulerait exactement le glissement pendant ce sous-pas ? », puis on la
> plafonne. Une impulsion qui vise l'annulation exacte ne peut pas dépasser sa
> cible : **elle ne peut donc pas osciller.**

```
vGlissLat = vitesse du point de contact, projetée sur l'axe latéral de la roue
Jlat      = -vGlissLat * masseEffective          (impulsion qui annule le glissement)
Jlong     = coupleMoteur/rayon * h   -  freinage
|J|       ≤ µ * Fsusp * h                        ← LE CERCLE DE FRICTION
```

- **la borne unique sur `|(Jlong, Jlat)|`** est ce qui donne le survirage
  gratuitement : sous forte accélération, le budget longitudinal mange le
  latéral, l'arrière décroche. *Le comportement émerge de la contrainte, il ne
  se scripte pas.*
- **`µ` vient du matériau** (`NkPhysicsMaterial`), déjà là ;
- **masse effective au point de contact** : `1 / (invMass + (r×axe)·invI·(r×axe))`
  — c'est la même formule que le contact solver, **on la partage, on ne la
  recopie pas**.

⚠️ **Et sous une vitesse plancher, on gèle latéralement** : à l'arrêt, un
glissement résiduel de 1 mm/s fait vibrer la voiture indéfiniment. En dessous de
~0,05 m/s, on annule sec.

---

## 4. 🚗 L'ASSIETTE ET LE BRAQUAGE — rien de plus, et c'est voulu

- **assiette** : aucune barre antiroulis, aucun transfert de charge explicite.
  **Ils sortent tout seuls** des quatre forces de suspension appliquées en quatre
  points **une fois le couple intégré** (§2). *Écrire un transfert de charge
  par-dessus, ce serait simuler deux fois la même chose.*
- **braquage** : on tourne l'**axe latéral** des roues avant, pas le corps. Angle
  limité, et **lissé** vers la consigne (une entrée clavier est un créneau ; un
  créneau sur l'axe de frottement fait sauter la voiture).
- **Ackermann** : la roue intérieure braque plus que l'extérieure. **Une ligne**,
  et sans elle la voiture « racle » en manœuvre serrée.

---

## 5. ⭐ LA SURFACE — conçue MAINTENANT, avec les signatures

> **L'étalon de Rodolf** : *combien de lignes pour faire rouler une voiture ?
> Au-delà de cinquante, la surface est ratée.*
> *Une physique dont l'usage demande deux cents lignes est une physique ratée,
> même si elle simule bien.*

```cpp
// ── faire rouler une voiture, en entier ──────────────────────────────────
NkPhysicsWorld world;
world.SetGravity({0.f, -9.81f, 0.f});
world.CreateStaticBox({0,-0.5f,0}, {200.f, 0.5f, 200.f});         // le sol

NkVehicle car(world);
car.SetChassisBox({0.f, 1.f, 0.f}, {0.9f, 0.5f, 2.2f}, 1200.f);   // demi-tailles, kg
car.AddWheel({-0.8f, 0.f,  1.3f}, NkWheel::kSteered);             // avant gauche
car.AddWheel({ 0.8f, 0.f,  1.3f}, NkWheel::kSteered);             // avant droit
car.AddWheel({-0.8f, 0.f, -1.3f}, NkWheel::kPowered);             // arrière gauche
car.AddWheel({ 0.8f, 0.f, -1.3f}, NkWheel::kPowered);             // arrière droit

while (running) {
    car.SetInput(steerAxis, throttleAxis, brakeAxis);             // ∈ [-1,1] / [0,1]
    world.Advance(dt);                                            // la voiture avance dedans
    Draw(car.ChassisTransform(), car.WheelTransform(0), ...);
}
```

**16 lignes.** Et les défauts se règlent **sans toucher à l'appel** :
`car.Tuning().suspensionStiffness = …` existe, mais **rien n'oblige à l'écrire**.

📌 **Trois choix de surface, et chacun a une raison** :
1. **`AddWheel` prend une POSITION, pas un objet roue.** L'auteur décrit une
   voiture, pas une simulation ;
2. **`SetInput` prend trois axes normalisés**, jamais un couple en N·m. *Le jeu
   déclare une intention — « j'accélère » — le moteur décide du couple.* C'est
   la règle gravée, appliquée à la surface du véhicule ;
3. **`world.Advance(dt)` fait avancer la voiture.** Le jeu n'appelle **jamais**
   `car.Update()` : le véhicule s'enregistre auprès du monde et est mis à jour
   **dans le sous-pas fixe**. ⚠️ *C'est ce qui rend le piège du pas de temps
   impossible à retomber dedans : l'appelant n'a pas de bouton pour se tromper.*

---

## 6. 🧪 CE QUI PROUVERA QUE ÇA MARCHE

**Les trois conditions de la maison** : un corps, **un appelant réel**, et **un
banc qui échoue si on vide l'`Execute()`**.

**Plus le banc de comportement**, headless, sans GPU :

| épreuve | attendu | ce qu'elle attrape |
|---|---|---|
| **elle tient** — voiture posée sur un plan, 5 s | hauteur stable **± 2 cm**, ni enfoncement ni décollage | ressort trop mou / trop raide, amortissement faux |
| **elle avance dans l'axe** — plein gaz, 3 s, sans braquage | avance **> 5 m**, dérive latérale **< 0,2 m** | signe d'axe faux, adhérence latérale absente |
| **elle s'arrête** — freinage plein | s'arrête et **reste immobile** | le gel sous vitesse plancher |
| **contre-épreuve** — `µ` mis à 0,01 | elle **doit patiner** et le banc **doit rougir** | *un banc qui reste vert avec du frottement nul ne mesure pas le frottement* |

⚠️ **Assertions en RELATION, pas en borne** : « hauteur ≈ restLength − poids/raideur
à 2 cm près », pas « hauteur < 10 ». *Une inégalité large est un contrôle de
plantage déguisé en contrôle de justesse.*

---

## 7. 📋 L'ORDRE, ET CE QUI ATTEND RODOLF

| | | |
|---|---|---|
| **0** | intégrer le couple + `ApplyForceAtPoint` + remonter `NkInvInertiaApply` | ½ j, **socle** — et re-compter les bancs existants |
| **1** | `NkWheel` + `NkVehicle`, suspension et adhérence, dans le sous-pas | 1 à 1,5 j |
| **2** | le banc de comportement + sa contre-épreuve | ½ j |
| **3** | réglage sur les deux voitures du dépôt (LowPolyCars, la futuriste et ses points `Wheel_Force`) | ½ j |

**Total ≈ 2,5 à 3 jours.** 🚫 **Rien n'est commencé.**

🔴 **La seule vraie question pour toi, Rodolf** : l'étape 0 modifie
`NkIntegrator`, **le cœur du socle physique**, pour un client qui n'existe pas
encore. Deux façons de la prendre :
- **(a)** on corrige le socle — le couple devient intégré **pour tout le monde**,
  ragdoll compris, avec re-comptage des bancs. *Ma recommandation* : le champ est
  déjà là, son absence d'intégration est un **défaut**, pas un choix ;
- **(b)** le véhicule calcule son propre couple dans son coin et l'applique en
  impulsions angulaires — aucun risque pour l'existant, mais on recopie chez soi
  ce qui manque au socle, et le prochain qui aura besoin d'un couple le recopiera
  encore.
