# CONTINUATION — travail parallèle sur Noge / modélisation (2026-07-30)

Document de passation écrit pour qu'un AUTRE agent puisse avancer SANS entrer en
collision avec les deux chantiers en cours. Lis-le en entier avant d'éditer quoi
que ce soit. Complète HANDOFF.md (vision et conventions) — ne le remplace pas.

## 1. Règles non négociables (résumé opérationnel)

- **Français** partout : réponses, commentaires de code, messages de commit.
- **Pas de STL** : conteneurs maison (`NkVector`, `NkString`, `NkHashMap`,
  `NkPair` → membres `First`/`Second`).
- **Compiler Debug ET Release** avant de considérer une étape finie, rapporter
  les deux statuts. `Permission denied` au link = l'exe est ouvert (souvent par
  Rihen qui teste) : compilation valide, le signaler.
- **Preuve avant annonce** : capture, log cité ou chiffre mesuré. Jamais « ça
  marche » sans exécution. Les hypothèses plausibles non mesurées ont coûté des
  heures dans cette session (voir HANDOFF.md).
- **Un seul build à la fois** (contrainte thermique), et PAS de charge GPU
  inutile : la machine s'éteint brutalement sous pic GPU (Kernel-Power 41,
  3 occurrences) ; un générateur de corpus tourne en GPU en permanence.
- Builds : `jenga build --target <X> --config Debug|Release --platform Windows`.
- Outil de déploiement multi-plateforme : `python Tools/nkdeploy.py <plateforme>
  --demo 2` (encapsule adb/hdc/WSL/serveur web — lire son en-tête).

## 2. FICHIERS VERROUILLÉS — ne pas éditer (chantiers en cours)

**Chantier modélisation (agent principal, en cours) :**
- `Applications/Sandbox/src/Demo/Demo3D.cpp`
- `Kernel/Runtime/NKRenderer/src/NKRenderer/Mesh/NkEditMesh.{h,cpp}`
- `Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/Render3D/*`
- `Kernel/Runtime/NKRenderer/src/NKRenderer/Core/NkGizmo.h`
- `Kernel/Runtime/NKRenderer/src/NKRenderer/Materials/NkMatcapLibrary.*`
- `Resources/NKRenderer/Shaders/{Sel*,PBR}/**`
- `Applications/NKEditMeshHarness/**`

**Chantier Web/écran noir (agent en cours, ne pas doubler) :**
- `Kernel/Runtime/NKRHI/src/NKRHI/Opengl/**`
- `Kernel/Runtime/NKRenderer/src/NKRenderer/Passes/**` (environnement)
- `Kernel/System/NKLogger/**`, `Kernel/System/NKThreading/**` (modifs non
  commitées en vol)
- `PLATEFORMES_ETAT.md` (journal de ce chantier — lecture libre)

Si ton diagnostic te mène DANS un fichier verrouillé : arrête-toi et note-le
dans ton rapport plutôt que d'éditer.

## 3. État acquis (commits poussés, réutilisables)

| quoi | commit |
|---|---|
| Harnais de non-régression NkEditMesh — 42 cas, `--check` en CI possible | `89eb9cab`, `94d3c8e8` |
| BMesh étape 1 : arête entité, F sur 2 sommets crée un segment filaire | `94d3c8e8` |
| Aller-retour édition = identité exacte (6 attributs) ; `NK_EDIT_IDENTITY=1` | `ed80d5b8` |
| Liseré sélection multi + actif distinct + instances | `81e6e692`, `175dc6d3` |
| Zone select (B / Ctrl+glisser / C) en mode objet | `dce7caaa` |
| 34 matcaps (30 Blender + 4 historiques), atlas 6×6, anti-ligne-sombre | `7a8dfbac`, `ecabc216` |
| Plateformes : Android scène complète, HarmonyOS image moteur, Web RHI init | `50ff3b5b`, `814be498`, `7c90c457` |
| `Tools/nkdeploy.py` (déploiement + capture par plateforme) | `d8669ba1` |

Leviers de test headless dans Demo3D (`NK_*`) : `NK_FIX_CAM`, `NK_CAPTURE`,
`NK_CAPTURE_PATH`, `NK_MAXFRAMES`, `NK_SHADING`, `NK_MATCAP`, `NK_GIZMO_MULTI`,
`NK_OBJ_ZONE`, `NK_EDIT_MODE`, `NK_EDIT_SELIDX`, `NK_EDIT_OP`, `NK_EDIT_EXIT`,
`NK_EDIT_IDENTITY`, `NK_WIRE_COLOR`. Grep `getenv("NK_` dans Demo3D.cpp pour la
liste complète.

## 4. TÂCHES OUVERTES ET SANS COLLISION (choisir ici)

### T1 — Morph targets / shape keys, côté Noge (LOT 6)
Périmètre : `Engine/Noge/src/Noge/Modeling/` (NkEditableMesh) — AUCUN fichier
verrouillé. Objectif : authoring de shape keys façon Blender (base + N cibles,
poids 0..1, mélange additif relatif à la base, évaluation → mesh de rendu).
S'appuyer sur `NkEditableMesh` (wrapper Noge au-dessus du maillage), stocker les
cibles comme deltas de positions sur l'identité SOUDÉE (voir la notion de
`BuildVertexMerge` dans HANDOFF.md — ne pas dupliquer la soudure, la consommer).
Livrable : API + tests console (modèle : `Engine/Noge/tests/test_editable_mesh.cpp`)
avec chiffres imprimés et vérifiés.

### T2 — Inventaire retopologie (LOT 7, phase ANALYSE seulement)
La retopologie existe en DOUBLE (côté Noge et côté NkEditMesh). Phase confiée :
INVENTAIRE PRÉCIS (qui fait quoi, qui appelle quoi, qualité relative), et un
plan de résorption en fonctions libres sur `renderer::NkEditMesh` (convention
actée en tête de NkEditMesh.h). INTERDIT d'éditer NkEditMesh : livrable = un
rapport `Engine/Noge/RETOPO_INVENTAIRE.md`, chiffré (lignes, appelants, cas
couverts), avec le plan de migration. L'implémentation viendra après BMesh.

### T3 — Squelette NKEditorKit (viewport réutilisable)
Objectif long terme : extraire de Demo3D le viewport (caméra orbit/fly, grille,
gizmo, sélection) vers un kit réutilisable par NK3DModeler/NkAnima/PV3DE — voir
HANDOFF.md (« viewport/multi-camera direction »). Phase confiée : créer le
MODULE NEUF `Engine/NKEditorKit/` (structure jenga + interfaces + implémentation
de la caméra éditeur en RÉUTILISANT `NkOrbitCameraController3D` /
`NkFlyCameraController3D` de NKRenderer/Core — ne pas les réécrire), SANS
toucher à Demo3D. Demo3D est une RÉFÉRENCE DE LECTURE, pas un fichier à éditer.
Livrable : le module compile (Debug+Release) + une démo console/fenêtre minimale
`NKEditorKitDemo` mise à jour qui affiche grille + caméra orbit.

### T4 — Cadrage jenga : helper nkentseuapp()
Chaque application répète ~340 lignes de plomberie plateforme (21 apps
concernées ; voir `Applications/Sandbox/RendererSandbox.jenga`, le plus
complet : Android assets, HarmonyOS ets/links, Web preload). Objectif : un
helper `nkentseuapp(...)` dans `jengaconfig` (ou module partagé du dépôt) qui
prend nom/dépendances/assets et génère les filtres standards ; migrer UNE app
pilote (PAS RendererSandbox — en cours de modification) et prouver que son build
Windows+Android reste identique (comparer la ligne de commande de compilation ou
l'APK produit). Source vivante de jenga : `D:\Projets\MacShared\Projets\Jenga\Jenga`
(2.0.9) — modifs possibles mais À SIGNALER dans le rapport (hors dépôt Nkentseu).

### Tâches à NE PAS prendre (réservées, collision certaine)
- Édition multi-objets, gizmos de lumières, marqueur actif en édition, merge/
  extrude/proportional/symétrie, bloom/modes viewport → tout passe par les
  fichiers verrouillés du chantier modélisation.
- Linux xcb/wayland, logs hilog, tactile HarmonyOS → chantier plateformes
  (NkOpenglDevice / NKWindow, en vol).

## 5. Méthode attendue dans les rapports

État initial mesuré → cause prouvée (sorties citées) → correctif → preuve
d'exécution → limites restantes → fichiers modifiés. Pas de commit sans accord
explicite de Rihen dans la conversation qui te pilote ; sinon laisser l'arbre
propre et lister les fichiers.
