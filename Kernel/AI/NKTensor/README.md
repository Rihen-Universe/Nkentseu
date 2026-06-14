# NKTensor — tenseurs (CPU & GPU)

> ⚠️ Squelette : pas encore de code. Voir la [ROADMAP](ROADMAP.md) et l'[architecture de la
> couche](../ARCHITECTURE.md).

## Rôle

`NKTensor` est la **brique de base** de tout NKAI. Un *tenseur* est un tableau à n dimensions de
nombres — un vecteur, une matrice, un volume… — et **tout**, en IA, s'exprime avec : les données,
les poids d'un réseau, les activations, les gradients. Sans tenseurs, rien.

Son apport décisif : **une seule API, deux backends**. On choisit, à la création d'un tenseur,
s'il vit sur le **CPU** (via NKMath/SIMD — portable, idéal pour les petits cas et le debug) ou sur
le **GPU** (via **NKRHI compute** — pour la rapidité et le calcul massif en parallèle, donc
l'entraînement et les gros modèles). Tout le reste de NKAI (réseaux, optimiseurs, RL…) est écrit
**une seule fois** au-dessus de cette API et ne sait **pas** sur quel matériel il tourne. C'est le
même principe d'abstraction que le RHI pour le rendu.

## Responsabilités

- Tenseur n-dim : forme, type, *layout* mémoire (via NKMemory), device (CPU/GPU).
- Opérations **élémentaires** (add, mul, activations) avec **broadcasting**.
- **Produit de matrices** et opérations de **réduction** (somme, moyenne, max…).
- Remodelage (reshape, transpose, slicing).
- Backend **CPU** (NKMath/SIMD) et backend **GPU** (NKRHI compute) derrière la même interface.

## Place dans la couche

- **Dépend de** : `NKMath`/SIMD (Foundation), **NKRHI** compute (Runtime), `NKMemory`.
- **Socle de** : tout le reste de NKAI ([NKAutograd](../NKAutograd/README.md), [NKNN](../NKNN/README.md), …).

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
