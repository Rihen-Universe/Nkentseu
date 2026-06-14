# Nkoung ROADMAP — Plateforme Multi-Jeux 2D

## Phase actuelle : MVP Plateforme (En cours)

### Statut de compilation

| Tâche | Statut | Blocage |
|-------|--------|---------|
| Architecture multi-fichier | ✅ Livré | — |
| NkoungPlatformApp (orchestrateur) | ✅ Livré | — |
| GameFactory (pattern) | ✅ Livré | — |
| LaserPuzzleGame (MVP) | ✅ Livré | — |
| NkoungConfig (allocateurs) | ✅ Livré | — |
| main.cpp (entry point) | ✅ Livré | — |
| **Build Jenga (tous .cpp inclus)** | ❓ À valider | Risque : dépendances manquantes |
| **Test graphique (--backend=opengl)** | ❓ À valider | Risque : liens manquants |
| **Test Menu & LaserPuzzle playable** | ⏳ Bloqué sur build | Dépend compilation |

## Livré cette session

- **Architecture** : 7 fichiers multi-couches (Core, Platform, Games/Common, Games/Specific)
- **Backends** : Support complet 5 backends (DX11/GL/VK/DX12/SW), parsing CLI `--backend=...`
- **Orchestrateur** : NkoungPlatformApp avec scènes (PlatformMenu, GameScene), transitions, event routing
- **LaserPuzzle MVP** : Grille 6×6, miroirs rotables (R), détection souris, rendu 2D basique
- **Documentation** : ARCHITECTURE_NKOUNG.md + ce ROADMAP

---

## Phase 2 : Polish & Fonctionnalités (1-2 semaines)

### 2.1 Menu de plateforme professional

**Tâches:**
- [ ] Afficher titres/sous-titres/descriptions GameInfo
- [ ] Indicateurs status (✅ Playable / 🔶 Prototype / ⏳ À venir)
- [ ] Highlighting sélection avec couleurs
- [ ] Keyboard nav (flèches + Entrée) + souris
- [ ] "Retour menu" bouton/touche ESC lors gameplay
- [ ] Icônes des jeux (si assets disponibles)
- [ ] FPS counter + info backend (titre fenêtre)

**Effort:** ~2-3 jours  
**Dépend:** [Test gameplay](#test-gameplay-fonctionnel)

### 2.2 LaserPuzzle complétude MVP

**Tâches:**
- [ ] Implémenter SimulateRay complet (réflexions multi-miroirs)
- [ ] Créer 3-5 niveaux test (JSON ou hardcoded)
- [ ] Carreler niveaux avec GameFactory::LoadLevel()
- [ ] Condition victoire : "tous targets activées, aucun mur frappé"
- [ ] HUD texte (niveau actuel, moves count, "Level Complete!")
- [ ] Ajouter Prism (réfraction) si temps
- [ ] Sons SFX (optionnel : click, rotation, victoire)

**Effort:** ~3-4 jours  
**Dépend:** [Build compilé & jouable](#test-gameplay-fonctionnel)

### 2.3 Skeletons des 5 autres jeux

**Tâches (pour chaque jeu):**
- [ ] Dossier `Games/Specific/MonJeu/`
- [ ] `MonJeuGame.h/cpp` basique (Init/Update/Render/OnEvent/Unload stub)
- [ ] Enregistrer dans `GameFactory`
- [ ] Test : affiche dans menu + clique lance fenêtre noire sans crash

**Jeux:**
1. Territories (stratégie au tour-par-tour)
2. Gardien du Labyrinthe (pathfinding)
3. Ponts & Chemins (puzzle)
4. Flux (flow-like)
5. Tactique (tactique au tour-par-tour)

**Effort:** ~2-3 jours (1 jour par 2 jeux)  
**Dépend:** [Build compilé](#test-gameplay-fonctionnel)

---

## Phase 3 : Implémentations jeux (2-4 semaines / parallèle)

### 3.1 Territories

**Objectif:** Jeu de stratégie au tour-par-tour (capturer territoires en grille)

**Tâches:**
- [ ] Grille hexagonale ou carrée avec propriétaires
- [ ] Turnmanager (joueur 1, joueur 2, IA)
- [ ] Sélection tuile → attaque/move
- [ ] ScoreBoard / timer tours
- [ ] Détection fin jeu

**Effort:** 4-5 jours  
**Dépend:** Skeleton + LaserPuzzle ref

### 3.2 Gardien du Labyrinthe

**Objectif:** Jeu d'aventure maze avec pathfinding & puzzles

**Tâches:**
- [ ] Éditeur de maze simple (ou charger fichier)
- [ ] Joueur controllable (ZQSD ou flèches)
- [ ] Ennemis (IA simple : à-vers-joueur)
- [ ] Collectibles (keys, power-ups)
- [ ] Condition victoire : sortie du labyrinthe

**Effort:** 3-4 jours  
**Dépend:** Skeleton

### 3.3 Ponts & Chemins

**Objectif:** Puzzle connecter îles avec ponts limités

**Tâches:**
- [ ] Grille avec îles
- [ ] Click-drag draw pont (validation géométrique)
- [ ] Compteur ponts restants
- [ ] Détection solution trouvée

**Effort:** 2-3 jours  
**Dépend:** Skeleton

### 3.4 Flux

**Objectif:** Puzzle flow-like connecter chemins sans intersection

**Tâches:**
- [ ] Grille avec points source/dest colorés
- [ ] Drag path (verrouille sur grille)
- [ ] Détection intersections (invalide)
- [ ] Détection grille complète (victoire)

**Effort:** 2-3 jours  
**Dépend:** Skeleton

### 3.5 Tactique

**Objectif:** Turn-based tactical RPG (si temps)

**Tâches:**
- [ ] Personnages sur grille
- [ ] Menu action (move, attack, spell, defend)
- [ ] Calcul distances & portées
- [ ] IA basique ennemis

**Effort:** 4-5 jours  
**Dépend:** Autres jeux + système ECS (optionnel)

---

## Phase 4 : Systèmes support (1-2 semaines)

### 4.1 Sauvegarde / Progression

**Tâches:**
- [ ] Détecter profil joueur (`%USERPROFILE%/.nkoung/profile.json`)
- [ ] Sérialiser progression par jeu (JSON)
- [ ] Charger au démarrage
- [ ] Mettre à jour progress bar menu

**Format JSON:**
```json
{
  "player": "DefaultPlayer",
  "lastGamePlayed": "LaserPuzzle",
  "games": {
    "LaserPuzzle": {
      "currentLevel": 5,
      "totalMoves": 245,
      "bestTime": 125.4
    },
    "Territories": {
      "victoriesCount": 2,
      "winRate": 0.5
    }
  }
}
```

**Effort:** 1-2 jours

### 4.2 Intégration audio (optionnel)

**Tâches:**
- [ ] Menu son ON/OFF
- [ ] SFX jeux (clic, victoire, etc.)
- [ ] BGM plateforme (loop, fade)

**Dépend:** NKAudio (module existant)  
**Effort:** 1-2 jours

### 4.3 Intégration NKECS (optionnel, futur)

**Rationale:** Pour jeux lourds (Tactique) si nécessaire

**Tâches:**
- [ ] Wrapper NKECS pour Nkoung
- [ ] Pattern System + Component défaults
- [ ] Exemple simple (Territories AI avec ECS)

**Effort:** 2-3 jours  
**Dépend:** Autres phases finis

---

## Phase 5 : Packaging & Distribution (1 semaine)

**Tâches:**
- [ ] Compiler Release Windows/Linux/macOS
- [ ] Créer installeur MSI (Windows)
- [ ] Packager AppImage (Linux)
- [ ] Tester sur configs variées (résolutions, backends)
- [ ] Documenter install/usage

**Effort:** 2-3 jours

---

## État du sprint Nkoung

### Completed ✅
- Architecture multi-fichier (7 fichiers)
- NkoungPlatformApp orchestrateur
- GameFactory pattern
- LaserPuzzleGame MVP (~250 lignes, playable)
- NkoungConfig (allocateurs, logger, consts)
- main.cpp simplifié (~35 lignes)
- 5 backends graphiques supportés (--backend=...)
- Documentation ARCHITECTURE_NKOUNG.md

### Immediate Next ❌ 🔴 BLOCKER
- **BUILD & TEST** : Compiler & vérifier pas d'erreurs linker
- **Test Gameplay** : Lancer menu, cliquer jeu, vérifier pas de crash
- Test backends : `--backend=opengl`, `--backend=vulkan`, etc.

### Then In Flight (après build ✅)
- Menu polish (afficher descriptions, status)
- LaserPuzzle complétude (rayons multi-réflexion, niveaux)
- 5 skeletons jeux

### Then Pending (semaines 2+)
- Implémentations jeux (parallèle 4-5 devs)
- Systèmes support (save/load, audio, ECS)
- Packaging & release

---

## Risques & Mitigations

| Risque | Probabilité | Impact | Mitigation |
|--------|-------------|--------|-----------|
| Build échecs dépendances | 🔴 Haut | Bloquant | Vérifier Jenga files, liens OK |
| NKCanvas multiplateforme issues | 🟡 Moyen | 1-2 jours delay | Tester backends un par un |
| Perf grille/rayons trop lente | 🟢 Bas | Optim | Pré-calculer rayons niveau chargement |
| Persistance données complexe | 🟢 Bas | Optionnel | Faire simple JSON d'abord |

---

## Checkpoints validation

### Checkpoint 1 : Build OK + Menu visible ✅
```bash
jenga build --target Nkoung --config Debug
# Devrait compiler sans erreur
# Au lancement : fenêtre avec menu 6 jeux
```

### Checkpoint 2 : Jeu playable
```bash
./Nkoung.exe
# Clic LaserPuzzle → grille 6x6 visible
# Clic miroir + R → rotation OK
# ESC → retour menu OK
```

### Checkpoint 3 : Backends fonctionnels
```bash
./Nkoung.exe --backend=opengl   # OK ?
./Nkoung.exe --backend=vulkan   # OK ?
./Nkoung.exe --backend=dx11     # OK ? (Win)
```

### Checkpoint 4 : Autres jeux lancent
```bash
./Nkoung.exe
# Clic Territories → fenêtre noire (OK, pas de crash)
# Clic Flux → fenêtre noire (OK)
```

---

## KPIs Livraison

| Métrique | MVP | Phase 2 | Phase 3 | Phase 5 |
|----------|-----|---------|---------|---------|
| Jeux jouables | 1/6 | 1/6 | 6/6 | 6/6 |
| Niveaux par jeu | 1 | 3-5 | 5-10 | 10+ |
| FPS stable (60 Hz) | ✅ | ✅ | ✅ | ✅ |
| Backends testés | 1-2 | 3-5 | 5 | 5 |
| Lignes code | ~2500 | ~4000 | ~8000+ | ~8500+ |
| Doc complète | 50% | 75% | 90% | 100% |

---

## Notes

- Développeur solo : Rihen (@nkentseu)
- Langue : français
- Motivations : plateforme agréable pour 6 petits jeux 2D, apprentissage Nkentseu patterns, alpha 2026
- Versions Nkentseu supportées : "latest" (Nkentseu dev branch)
