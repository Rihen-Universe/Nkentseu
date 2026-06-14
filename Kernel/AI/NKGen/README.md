# NKGen — modèles génératifs (2D / 3D / animation)

> ⚠️ Squelette : pas encore de code. Voir la [ROADMAP](ROADMAP.md) et l'[architecture de la
> couche](../ARCHITECTURE.md).

## Rôle

`NKGen` regroupe les IA qui **créent du contenu** plutôt que de le classer ou de décider : générer
une **image 2D**, esquisser une **forme 3D**, produire une **animation**. Ce sont des modèles de
NKNN entraînés à *produire* — typiquement des modèles de **diffusion** pour l'image — exécutés via
NKInfer.

L'intérêt, dans un moteur, est immédiat : générer des **assets directement dans Nkentseu**.
Imaginer une texture, une silhouette de personnage, un cycle de marche, une variation
d'environnement — assistés par l'IA, au sein de l'éditeur, sans quitter le moteur. C'est aussi ce
qui pourra peupler la civilisation d'apparences variées. On reste lucide (cf.
[architecture §1](../ARCHITECTURE.md#1-pourquoi-cette-couche-existe)) : reproduire les modèles
génératifs **état de l'art** from-scratch est colossal ; la cible réaliste est d'**héberger** de
tels modèles et d'en écrire **quelques-uns**, petits, pour comprendre et pour les cas utiles au
moteur.

## Responsabilités

- Modèles **génératifs d'images 2D** (diffusion : entraînement de petits modèles + inférence).
- Génération de **formes 3D** (procédural guidé par l'apprentissage, puis modèles dédiés).
- **Animation / motion** (génération de mouvements).
- Intégration **moteur** : produire des assets exploitables (textures, maillages, anims).

## Place dans la couche

- **Dépend de** : [NKNN](../NKNN/README.md), [NKInfer](../NKInfer/README.md), [NKTensor](../NKTensor/README.md).
- **Alimente** : l'éditeur / le pipeline d'assets, et [NKCivilization](../NKCivilization/README.md) (apparences).

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
