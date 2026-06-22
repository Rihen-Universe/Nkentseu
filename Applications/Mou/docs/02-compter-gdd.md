# Mú — GDD Jeu « Compte avec moi »

> Dénombrer de 1 à 10. Public cible : 4 ans+.
> Système de niveaux global : [00-plateforme-gdd.md](00-plateforme-gdd.md) §4.

---

## 1. Boucle de jeu

1. Nana (voix) : « Combien de **canards** ? »
2. Des objets (canards, pommes…) apparaissent à l'écran.
3. L'enfant **tape chaque objet** : il rebondit et fait un son, et un **compteur
   vocal** dit « un… deux… trois… » à chaque tap (apprentissage du dénombrement 1-1).
4. Puis il choisit le bon **gros chiffre** parmi des cartes-réponses.
5. Bon → carillon + étoile. Mauvais → Nana recompte avec lui (aide), pas de sanction.

## 2. Modes (progressifs)

| Mode | Description |
|------|-------------|
| `count` | Compter les objets puis taper le bon chiffre. |
| `give` | « Donne-moi **N** canards » : l'enfant tape N objets (produire une quantité). |
| `compare` | Deux groupes : taper celui qui a **plus** (ou **moins**). |
| `order` | Remettre des chiffres/quantités dans l'ordre croissant. |

## 3. Niveaux (≈15, sur 3 mondes)

### Monde 1 — La ferme 🐤 (1 à 3, gros objets)
| Niv | Plage | Mode |
|-----|-------|------|
| 1 | 1–3 | count |
| 2 | 1–3 | count (objets éparpillés) |
| 3 | 1–3 | give |
| 4 | 1–4 | count |
| 5 | 1–5 | count |

### Monde 2 — Le verger 🍎 (1 à 5-7)
| Niv | Plage | Mode |
|-----|-------|------|
| 6 | 1–5 | give |
| 7 | 1–5 | compare (plus) |
| 8 | 1–6 | count |
| 9 | 1–7 | count |
| 10 | 1–5 | order |

### Monde 3 — La ville 🏙️ (1 à 10)
| Niv | Plage | Mode |
|-----|-------|------|
| 11 | 1–8 | count |
| 12 | 1–10 | count |
| 13 | 1–10 | give |
| 14 | 1–10 | compare (plus / moins) |
| 15 | 1–10 | order (5 quantités) |

## 4. Étoiles
- 3⭐ : bonne réponse du premier coup, dénombrement complet.
- 2⭐ : 1 erreur. 1⭐ : terminé avec aide.

## 5. Détails pédagogiques
- **Compteur vocal au tap** = le pilier (correspondance 1 objet ↔ 1 nombre).
- Objet déjà compté = marqué (petit ✓ ou changement de teinte) pour éviter de
  recompter — soutien visuel du dénombrement.
- Cartes-chiffres : très gros, rendues via **NKFont** (pas figées en SVG).

## 6. Assets requis
- Existants : `duck.svg`, `apple.svg`, `mascot_nana.svg`, `star_reward.svg`,
  `card_frame.svg` (support des cartes-chiffres).
- À produire : 2-3 objets à compter de plus (poisson, fleur, ballon…), marqueur
  « compté », décors ferme/verger/ville.
