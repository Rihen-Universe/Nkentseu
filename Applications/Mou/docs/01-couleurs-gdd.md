# Mú — GDD Jeu « Les Couleurs »

> Reconnaître et trier les couleurs. Public cible : 3 ans+.
> Voir le système de niveaux global : [00-plateforme-gdd.md](00-plateforme-gdd.md) §4.

---

## 1. Boucle de jeu

1. Nana (voix) : « Donne-moi le **rouge** ! » (+ le panier rouge brille).
2. Des **ballons** colorés flottent en bas de l'écran.
3. L'enfant **glisse** le bon ballon dans le **panier** correspondant.
4. Bon → le ballon « plop » dans le panier, carillon, +1. Mauvais → le ballon
   revient doucement, Nana : « Essaie encore ! » (jamais punitif).
5. Tous les objets triés → étoiles + confettis → niveau suivant.

**Accessibilité daltonien** : chaque panier ET chaque ballon portent un **symbole**
(rouge=cœur, bleu=rond, jaune=étoile, vert=feuille) — réussite possible par la forme.
Assets : `bin_*.svg`, `balloon_*.svg`.

## 2. Modes (introduits progressivement)

| Mode | Description |
|------|-------------|
| `sort` | Trier chaque objet dans le panier de sa couleur. |
| `pick` | Nana nomme UNE couleur, taper l'objet de cette couleur (sans drag). |
| `odd` | Trouver l'intrus (la couleur différente des autres). |
| `shade` | Distinguer clair / foncé d'une même couleur (4-5 ans). |

## 3. Niveaux (≈15, sur 3 mondes)

> Difficulté = nombre de couleurs × nombre d'objets × mode. Aucun timer avant le
> monde 3 (et même là, doux, pour l'étoile bonus seulement).

### Monde 1 — La forêt 🍃 (découverte, 3 ans)
| Niv | Couleurs | Objets | Mode |
|-----|----------|--------|------|
| 1 | rouge, bleu | 3 | pick |
| 2 | rouge, bleu | 4 | sort |
| 3 | rouge, bleu, jaune | 4 | sort |
| 4 | rouge, bleu, jaune | 5 | sort |
| 5 | + vert | 6 | sort |

### Monde 2 — La plage 🏖️ (consolidation, 3-4 ans)
| Niv | Couleurs | Objets | Mode |
|-----|----------|--------|------|
| 6 | 4 couleurs | 6 | sort |
| 7 | + orange | 6 | pick |
| 8 | 5 couleurs | 7 | sort |
| 9 | 5 couleurs | 5 | odd (intrus) |
| 10 | + violet (6) | 8 | sort |

### Monde 3 — L'espace 🚀 (défi, 4-5 ans)
| Niv | Couleurs | Objets | Mode |
|-----|----------|--------|------|
| 11 | 6 couleurs | 8 | sort |
| 12 | 6 couleurs | 6 | odd |
| 13 | rouge clair/foncé | 4 | shade |
| 14 | 3 paires de nuances | 6 | shade |
| 15 | 6 couleurs + nuances | 8 | sort (timer doux pour 3⭐) |

## 4. Étoiles
- 3⭐ : tout trié sans erreur (et sous le temps cible en M3).
- 2⭐ : ≤ 2 erreurs. 1⭐ : terminé (toujours débloquant).

## 4 bis. Mode « vocabulaire » (nom des éléments) — demandé 2026-06-15

En plus de la couleur, le jeu peut **nommer l'élément** (voix + mot écrit gros) :
« le **ballon rouge** », « la **banane jaune** ». Double apprentissage : couleur +
vocabulaire. Implémentation : un flag `naming` par niveau (off au monde 1 pour ne
pas surcharger les 3 ans, on au monde 2-3). La voix dit le nom ; le mot s'affiche
sous l'objet (gros, via NKFont). **Décision** : on garde ça comme **mode du jeu
Couleurs** (pas un jeu séparé) — c'est la même mécanique enrichie.

## 4 ter. Éléments culturels camerounais / africains — à investiguer

Au lieu d'objets génériques, utiliser des **éléments typiquement camerounais /
africains** (cohérent avec le nom ghomálá' « Mú ») : fruits (banane plantain,
mangue, papaye, goyave, safou, ananas), animaux (panthère, perroquet gris du Gabon,
margouillat, antilope), objets (tam-tam/djembe, calebasse, pagne/wax coloré, case).
Ça ancre culturellement + enseigne le vocabulaire local. **À faire : recherche
approfondie** (liste validée + cohérence visuelle + noms FR et éventuellement
ghomálá') AVANT de produire les assets — pilote le pack d'illustrations.

## 5. Assets requis
- Existants : `bin_{red,blue,yellow,green}.svg`, `balloon_{red,blue,yellow,green}.svg`,
  `mascot_nana.svg`, `star_reward.svg`.
- À produire : paniers/ballons **orange** + **violet**, variantes **nuances**
  (clair/foncé), décors des 3 mondes (forêt/plage/espace), confettis.
