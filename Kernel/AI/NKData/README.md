# NKData — données

> ⚠️ Squelette : pas encore de code. Voir la [ROADMAP](ROADMAP.md) et l'[architecture de la
> couche](../ARCHITECTURE.md).

## Rôle

`NKData` **nourrit** l'apprentissage. Un modèle n'apprend rien sans données, et la façon de les
préparer et de les présenter pèse autant que l'architecture du réseau. Ce module transforme des
sources brutes (fichiers d'images, de texte, journaux d'une simulation) en **lots (batchs) de
tenseurs** prêts à être consommés par NKTrain.

Il couvre quatre besoins : décrire un **jeu de données** (où sont les exemples, comment lire l'un
d'eux), les regrouper en **lots** mélangés efficacement, **tokeniser** le texte (le découper en
unités numériques, indispensable pour les LLM), et **augmenter** les données (variations
synthétiques pour mieux généraliser). Sur la console et la civilisation, c'est aussi NKData qui
fournira les **observations** des agents comme données d'apprentissage.

## Responsabilités

- Interface de **jeu de données** + lecture d'un exemple.
- **Chargeur** : mélange, regroupement en lots, conversion en tenseurs (NKTensor).
- **Tokenizer** (texte → identifiants) et détokenizer.
- **Augmentation** de données.

## Place dans la couche

- **Dépend de** : [NKTensor](../NKTensor/README.md) ; `NKFileSystem`/`NKImage` (Runtime) pour lire les sources.
- **Utilisé par** : [NKTrain](../NKTrain/README.md), et plus tard les LLM (via [NKInfer](../NKInfer/README.md)).

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
