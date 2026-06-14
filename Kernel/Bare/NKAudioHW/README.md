# NKAudioHW — audio (DAC)

> ⚠️ Squelette : pas encore de code. Voir la [ROADMAP](ROADMAP.md) et l'[architecture de la
> couche](../ARCHITECTURE.md).

## Rôle

`NKAudioHW` est le pilote de **sortie son** : il pilote le **DAC** (convertisseur numérique→
analogique) ou le contrôleur audio de la console. Son travail est de prendre un flux
d'**échantillons** (les nombres que produit le moteur audio) et de les envoyer au matériel au bon
**rythme**, sans interruption ni craquement.

En pratique, il expose un **tampon** que le système remplit en continu (souvent en double
buffering, réarmé par interruption à chaque fois qu'une moitié a été jouée). Au-dessus, il
alimente **NKAudio** (le moteur audio de Nkentseu — mixage, voix, effets) : NKAudio fait tout le
travail de haut niveau et n'a besoin que d'une chose ici — un endroit où **déverser ses
échantillons** vers le haut-parleur. C'est la dernière brique pour qu'un jeu ait du son.

## Responsabilités

- Initialiser le DAC / contrôleur audio (fréquence d'échantillonnage, format).
- Exposer un **tampon de sortie** alimenté en continu (double buffering, IRQ).
- Gérer le rythme (sous-alimentation = silence propre, pas de craquement).
- Alimenter **NKAudio** (le backend de sortie sur le métal).

## Place dans la couche

- **Dépend de** : [NKDriver](../NKDriver/README.md), [NKInterrupt](../NKInterrupt/README.md).
- **Alimente** : `NKAudio` (Runtime) — la sortie son des jeux.

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
