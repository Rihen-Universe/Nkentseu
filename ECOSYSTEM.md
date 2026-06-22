# L'écosystème Rihen — vision unifiée & feuille de route priorisée

> Document de consolidation. Il relie toutes les briques (moteur, build, IDE, IA, OS, jeux) en
> **une seule vision** et fixe **un ordre de priorité clair**. Objectif : garder le cap et éviter la
> dispersion (le risque n°1 quand on construit autant de choses en parallèle).

---

## 1. La thèse

Rihen construit un **écosystème créatif et logiciel intégré verticalement, et possédé de bout en
bout** : **du métal jusqu'à l'IDE et l'IA**, chaque couche est écrite from scratch, sans dépendre
d'un tiers. C'est rare et exigeant — mais c'est le différenciateur : une cohérence et une maîtrise
totales, de la console à l'outil de création.

> **Si tu possèdes chaque couche, tu n'es bloqué par personne.**

---

## 2. Les briques

| Brique | Rôle | État | S'appuie sur |
|--------|------|------|--------------|
| **Nkentseu** | Le **moteur** (Foundation/System/Runtime) : mémoire, conteneurs, maths, rendu (NKCanvas/NKRHI), UI, audio, fenêtre, événements… | ✅ Production (Win/Linux/Android/Web) | — |
| **Jenga** | Le **système de build** (DSL Python, cross-plateforme, sans CMake) : compile, package, déploie. | ✅ v2.0.7 publiée | — |
| **Editor Kit** *(partagé)* | Le **framework d'éditeur commun** : shell/docking, panneaux, **inspecteur (NKReflection)**, **éditeur de nœuds (Blueprint/Blocks)**, **UI builder**, palette de commandes, thème. **Réutilisé par NKCode ET Nogee** (construit une fois). | 🎯 À factoriser | Nkentseu (NKUI/NKCanvas) + NKReflection |
| **NKCode** | L'**IDE / éditeur de code** (≈ VSCode/Rider) : texte + langages visuels + extensions + **agents IA**. App **mince sur l'Editor Kit**. | 📐 Scaffold (10 sous-systèmes) | Editor Kit + Jenga |
| **Noge** | Le **moteur de jeu 2D/3D** (gameplay/scène/ECS, **utilise NKRenderer**) ≈ framework gameplay d'Unity/Unreal. | 🟡 Spécifié (peu de code) | Nkentseu + NKRenderer |
| **Nogee** | L'**éditeur du moteur de jeu** (scènes, niveaux, assets, inspecteur) ≈ **Unity Editor / UnrealEd**. App **mince sur l'Editor Kit**. | 📐 À démarrer | Editor Kit + Noge |
| **NKAI** (`Kernel/AI`) | Le **framework IA** from scratch : tenseurs/NN/RL/LLM + agents + sim de civilisation. Fournira les **modèles locaux** de NKCode. | 📐 Scaffold | Nkentseu (NKRHI compute) |
| **Kernel/Bare** | L'**OS bare-metal** de la console maison (boot, mémoire, pilotes, runtime). | 📐 Scaffold | Nkentseu (freestanding) |
| **NKReflection ↔ NKSerialization** | La **clé de voûte** : réflexion + sérialisation (inspecteur, assets, nœuds Blueprint, sauvegarde IA). | 🟡 À mûrir | Nkentseu |
| **Nkoung / Mou** | Des **produits-jeux** qui prouvent le moteur (plateforme de jeux 2D ; éveil maternelle). | ⏸️ **EN PAUSE** | Nkentseu |

---

## 3. Comment tout s'imbrique

```
                       ┌─────────────────────────────────────────┐
                       │  NKCode  (IDE-plateforme)               │
                       │   texte · Blueprint/Blocks · Extensions │
                       │   · Agents IA ───────┐                  │
                       └───────────┬──────────┼──────────────────┘
        construit/build │          │ héberge  │ modèles locaux
                        ▼          ▼          ▼
                   ┌────────┐  ┌────────┐  ┌────────┐
                   │ Jenga  │  │Nkentseu│  │  NKAI  │
                   │ build  │  │ moteur │  │  IA    │
                   └────────┘  └───┬────┘  └────────┘
                                   │ tourne sur
                                   ▼
                              ┌──────────┐        (NKReflection/NKSerialization
                              │Kernel/Bare│         irriguent éditeur, assets,
                              │  console  │         nœuds Blueprint, sauvegarde IA)
                              └──────────┘
```

- **Jenga** construit **tout** ; **Nkentseu** rend/anime **tout**.
- **NKCode** est bâti sur Nkentseu + Jenga, **héberge les agents** (NKAI) et **les extensions**, et
  son **éditeur de nœuds unique** sert au gameplay *et* à l'orchestration d'agents.
- **NKAI** donnera à NKCode des **agents 100 % locaux** (hors cloud).
- **Kernel/Bare** fait tourner Nkentseu sur la **console maison**.
- **NKReflection ↔ NKSerialization** est le tissu transverse (inspecteur, assets, nœuds, sauvegarde).

**La famille d'éditeurs (3 produits distincts, 1 socle commun).** Comme dans l'industrie :
- **NKCode** = l'**IDE de code** (≈ VSCode/Rider) ; **Nogee** = l'**éditeur de jeu** (≈ Unity Editor/
  UnrealEd) ; **Noge** = le **moteur de jeu** (≈ gameplay Unity/Unreal, sur NKRenderer).
- **Nogee et NKCode partagent ~70 % de leur plomberie** → on construit **un `Editor Kit` une seule
  fois** (docking, inspecteur, node-graph, UI builder) ; chacun devient une **app mince** dessus.
  **Anti-dispersion + bonne archi.** → **NKReflection** a alors **5+ consommateurs** (inspecteurs
  Nogee+NKCode, sérialisation scènes+graphes, assets) : à mûrir en priorité.
- **Le scripting visuel (Blueprint/Blocks) vit dans l'`Editor Kit`** (substrat Graph commun), exposé
  **à la fois** à Nogee (gameplay, façon Blueprint d'UnrealEd) et à NKCode.

---

## 4. Feuille de route PRIORISÉE (le cap actuel)

**Focus : la FAMILLE D'ÉDITEURS (NKCode + Nogee) sur un socle commun. Nkoung/Mou en pause.**
Séquence pensée pour assainir avant de bâtir, et ne construire le socle qu'une fois.

### 1️⃣ D'abord — FINIR NKRenderer
- Fermer la **saga shaders** (NkSL → vrai backend **HLSL** DX). C'est le **seul chantier proche de la
  fin**, et la **base 3D de Noge**. On **assainit le moteur** avant d'y poser les éditeurs. Victoire
  de moral + chapitre fermé.

### 2️⃣ Clé de voûte — NKReflection ↔ NKSerialization
- **5+ consommateurs** : inspecteurs **Nogee + NKCode**, sérialisation **scènes + graphes**, assets.
  **Le plus haut levier — à mûrir tôt.**

### 3️⃣ Le socle commun — **Editor Kit** (construit une fois)
- Shell/docking + panneaux + **inspecteur** + **node-graph (Blueprint/Blocks)** + **UI builder**,
  sur NKUI/NKCanvas/NKReflection. **Réutilisé par NKCode ET Nogee.** Évite de tout coder deux fois.

### 4️⃣ Les apps minces sur le Kit
- **NKCode** (IDE code) : coquille + éditeur texte + intégration Jenga → Codegen → **Agents** (LLM
  externe → **local via NKAI/NKInfer**) → Extensions.
- **Nogee** (éditeur de jeu) : scènes/niveaux/assets/inspecteur, sur **Noge** (gameplay, sur NKRenderer).

### 🔧 En fond — santé du moteur (au besoin)
- Modules cassés (NKNetwork/NKECS), **CI + tests**.

### ⏸️ Plus tard / en pause
- **Nkoung / Mou** (jeux), **Kernel/Bare** (console), le **NKAI complet** (civilisation, gros LLM).
  Repris quand l'éditeur-famille aura une tranche qui tourne.

---

## 5. Discipline (anti-dispersion)

- **Une tranche qui marche > un scaffold de plus.** On résiste à créer de nouvelles briques tant
  que NKCode n'a pas un noyau fonctionnel.
- **NKReflection est le goulot partagé** : l'avancer sert **NKCode, Nogee, l'Editor Kit, NKAI et les
  assets** à la fois. C'est LE point à débloquer en premier.
- **Construire l'`Editor Kit` une seule fois** (réutilisé par NKCode + Nogee) plutôt que deux éditeurs.
- Les visions (Bare, NKAI complet, civilisation) **restent documentées** (scaffolds) et **attendent**
  leur tour — elles ne sont pas perdues, juste séquencées.

---

*Briques détaillées : `Applications/NKCode/`, `Kernel/AI/`, `Kernel/Bare/` (ARCHITECTURE + README +
ROADMAP chacun) · moteur : `wiki/` · build : dépôt Jenga.*
