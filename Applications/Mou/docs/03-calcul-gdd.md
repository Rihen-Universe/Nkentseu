# Mú — GDD Jeu « Petits calculs »

> Additions et soustractions jusqu'à 10, 100 % visuelles. Public cible : 5 ans+.
> Système de niveaux global : [00-plateforme-gdd.md](00-plateforme-gdd.md) §4.

---

## 1. Boucle de jeu

1. Nana (voix) : « **Deux** pommes **et encore une** pomme, ça fait combien ? »
2. Affichage **visuel** : 🍎🍎 **+** 🍎 (les objets sont montrés, pas juste des chiffres).
3. L'enfant choisit la réponse parmi **3 grosses cartes** (chiffres + points/objets).
4. Bon → les objets fusionnent en un groupe, carillon, étoile. Mauvais → Nana
   montre en regroupant réellement les objets (aide concrète), puis on réessaie.

> Règle clé maternelle : **toujours un support concret** (objets visibles) sous le
> calcul symbolique. On n'enseigne pas l'abstraction nue à 5 ans.

## 2. Modes (progressifs)

| Mode | Description |
|------|-------------|
| `add` | Addition : N objets + M objets = ? |
| `sub` | Soustraction : N objets, on en enlève M (animation), il en reste ? |
| `mixed` | Addition et soustraction mélangées. |
| `missing` | Terme manquant : 2 + ? = 3 (choisir ce qui complète). |

## 3. Niveaux (≈12, sur 3 mondes)

### Monde 1 — Le jardin 🌱 (addition ≤ 5)
| Niv | Opération | Réponses |
|-----|-----------|----------|
| 1 | add, somme ≤ 3 | 2 cartes |
| 2 | add, somme ≤ 3 | 3 cartes |
| 3 | add, somme ≤ 5 | 3 cartes |
| 4 | add, somme ≤ 5 (objets variés) | 3 cartes |

### Monde 2 — La cuisine 🍪 (soustraction ≤ 5)
| Niv | Opération | Réponses |
|-----|-----------|----------|
| 5 | sub, depuis ≤ 5 | 3 cartes |
| 6 | sub, depuis ≤ 5 | 3 cartes |
| 7 | mixed ≤ 5 | 3 cartes |
| 8 | missing (terme manquant) ≤ 5 | 3 cartes |

### Monde 3 — Le marché 🧺 (jusqu'à 10)
| Niv | Opération | Réponses |
|-----|-----------|----------|
| 9 | add, somme ≤ 10 | 3 cartes |
| 10 | sub, depuis ≤ 10 | 3 cartes |
| 11 | mixed ≤ 10 | 4 cartes |
| 12 | missing ≤ 10 | 4 cartes (timer doux pour 3⭐) |

## 4. Étoiles
- 3⭐ : bon du premier coup. 2⭐ : 1 erreur. 1⭐ : terminé avec aide.

## 5. Détails
- L'**animation** porte le sens : pour `add`, les deux groupes glissent et se
  réunissent ; pour `sub`, les objets retirés s'envolent. L'enfant **voit** l'opération.
- Cartes-réponses : gros chiffre (NKFont) **+** rappel visuel (points) → lisible
  même sans connaître encore les chiffres.

## 6. Assets requis
- Existants : `apple.svg`, `card_frame.svg`, `mascot_nana.svg`, `star_reward.svg`.
- À produire : 2-3 objets « comptables » supplémentaires (cookie, carotte, fleur),
  animation de retrait (objet qui s'envole), décors jardin/cuisine/marché.
