# Nkoung — Spécification Design UI

## 🎨 Palette de couleurs (Flat Modern Gaming)

### Couleurs principales
```
Background primaire:   #0F1419 (Noir profond)
Background secondaire: #1A1F2E (Gris très foncé)
Accent primaire:       #00D9FF (Cyan électrique) — Actions, highlights
Accent secondaire:     #FF006E (Rose électrique) — Erreurs, warnings
Success:               #00D084 (Vert menthe) — Statut OK
Warning:               #FFA500 (Orange) — Attention
```

### Couleurs texte
```
Texte primaire:        #FFFFFF (Blanc pur)
Texte secondaire:      #A0A8C0 (Gris clair)
Texte tertiaire:       #5A6478 (Gris sombre)
```

### Dégradés (subtils)
```
Gradient Premium:      #00D9FF → #0099CC (Cyan dégradé)
Gradient Danger:       #FF006E → #FF3366 (Rose dégradé)
Gradient Success:      #00D084 → #009966 (Vert dégradé)
```

---

## 🎮 Thème Platform Menu

### Layout global
```
┌─────────────────────────────────────────────────────┐
│ HEADER (60 px)                                      │
│ Nkoung v0.2.0 | FPS: 60                            │
├─────────────────────────────────────────────────────┤
│                                                     │
│  GAME CARDS (270 px height)                         │
│  ┌──────────────────┐  ┌──────────────────┐        │
│  │ Laser Puzzle     │  │ Territoires      │        │
│  │ [icon]           │  │ [icon]           │        │
│  │ Prototype ▶      │  │ À concevoir ❌   │        │
│  │ ► Jouer          │  │ (Verrouillé)     │        │
│  └──────────────────┘  └──────────────────┘        │
│                                                     │
│  [2×3 grille de jeux]                              │
│                                                     │
│ FOOTER (30 px)                                      │
│ Entrée: Jouer | ESC: Quitter                       │
└─────────────────────────────────────────────────────┘
```

### Couleurs
- Background: `#0F1419`
- Card background: `#1A1F2E`
- Card border (normal): `#2A3142` (2 px)
- Card border (hover): `#00D9FF` (2 px, glow)
- Card border (selected): `#FF006E` (3 px, glow)

### Typo
- Titre app: 32 px, **Bold**, Cyan `#00D9FF`
- Titre jeu: 18 px, **Bold**, Blanc
- Statut jeu: 12 px, Regular, Gris clair `#A0A8C0`
- Footer: 12 px, Regular, Gris clair

### Animations (future)
- Hover card: border color fade 200ms, shadow glow
- Click: scale 0.95 → 1.0 (100ms)
- Selection: glow pulse 1s infinite

---

## 🎮 Thème Laser Puzzle

### Layout
```
┌─────────────────────────────────────────────────────┐
│ HEADER (50 px)                                      │
│ Laser Puzzle | Level 1 | Moves: 5 | ← Menu         │
├─────────────────────────────────────────────────────┤
│                                                     │
│  [GAME BOARD]                                       │
│  Grille 6×6 avec padding                           │
│  Cellules: 60 px × 60 px                           │
│  Gap: 2 px                                          │
│                                                     │
│  [HUD overlay en bas]                               │
│  Level Complete! | Next → | Restart ↻              │
│                                                     │
└─────────────────────────────────────────────────────┘
```

### Couleurs cellules
```
Cellule vide:         #1A1F2E (gris sombre) + border #2A3142
Cellule sélectionnée: #00D9FF (cyan) + glow
Cellule hover:        #2A3142 (gris plus clair)

Source laser:         Cercle #00D084 (vert menthe)
Target laser:         Cercle #FFD700 (or)
Mirror tile:          Diagonale #A0A8C0 (gris clair, 3 px)
Wall tile:            Remplissage #5A6478 (gris sombre)
Laser ray:            Ligne #FF006E (rose) ou #00D9FF (cyan), 2.5 px
```

### Typo in-game
- Titre niveau: 24 px, Bold, Cyan
- Infos HUD: 14 px, Regular, Blanc
- Messages victoire: 28 px, Bold, Vert menthe `#00D084`

---

## 🎨 Composants réutilisables

### Button style
```
Normal:     bg=#1A1F2E, border=#2A3142 (2px), fg=#FFFFFF
Hover:      bg=#2A3142, border=#00D9FF (2px)
Active:     bg=#00D9FF, border=#00D9FF (2px), fg=#0F1419
Disabled:   bg=#0F1419, border=#5A6478 (2px, dashed), fg=#5A6478
```

### Panel/Card style
```
Background:    #1A1F2E
Border:        #2A3142 (1-2 px)
Corner radius: 8 px (soft)
Padding:       16 px
Shadow:        Subtle (optional, future)
```

### Progress bar
```
Background:    #0F1419
Fill:          Gradient cyan-to-green (#00D9FF → #00D084)
Height:        6 px
```

---

## 📐 Spacing & Sizing

### Padding standards
```
Small:    8 px
Medium:   16 px
Large:    24 px
XL:       32 px
```

### Border radius
```
Buttons:   4 px
Cards:     8 px
Large UI:  12 px
```

### Shadows (future)
```
Light:     rgba(0, 217, 255, 0.1) offset 2px 2px
Medium:    rgba(0, 217, 255, 0.2) offset 4px 4px
Heavy:     rgba(0, 217, 255, 0.3) offset 8px 8px
```

---

## 🎯 Interactions visuelles

### Hover effects
- **Cards**: Border devient Cyan `#00D9FF`, subtle glow
- **Buttons**: Background s'éclaircit, border Cyan

### Select effects
- **Cards**: Border devient Rose `#FF006E`, glow plus prononcé
- **Menu items**: Highlight Cyan, underline

### Disabled state
- **Elements**: Opacity 50%, texte gris `#5A6478`

### Feedback
- **Click**: Micro-vibration visuelle (scale animation)
- **Success**: Texte vert `#00D084`, aura brève
- **Error**: Texte rose `#FF006E`, shake léger

---

## 🚀 Implementation roadmap

### Phase 1 (MVP) — Flat colors
- [x] Palette définie
- [ ] Platform menu : grid de jeux + cards flat
- [ ] Laser Puzzle : grille colorée, UI basique

### Phase 2 (Polish)
- [ ] Glows subtils (Cyan/Rose)
- [ ] Gradients dégradés
- [ ] Hover effects
- [ ] Animations transitions

### Phase 3 (Advanced)
- [ ] Particle effects
- [ ] Shader glows
- [ ] Sound design matching
- [ ] Theme selector (Dark/Light/Custom)

---

## 📝 Notes design

- **Flat first**: Pas de ombres complexes, couleurs pures
- **Accessibility**: Contraste WCAG AA minimum
- **Performance**: Pas d'effet GPU-heavy (rendu CPU 2D)
- **Cohérence**: Mêmes couleurs partout, typo consistante
- **Évolutivité**: Design scale bien 800×600 → 1920×1080

---

## 🎮 Références visuelles

- **Style**: Hyper Light Drifter meets Tron (flat + neon)
- **Palette**: Cyberpunk minimal (noir profond + cyan/rose)
- **Typo**: Futuristique sobre (sans serif, simple)
- **Layout**: Game UI moderne (card-based, aéré)

---

Créé: 2026-06-09
Statut: 🟢 Prêt pour implémentation
