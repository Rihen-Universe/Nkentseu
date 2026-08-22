# Générateurs des planches de la spécification visuelle

> ## ⚠️ LA RÈGLE
>
> **Une planche n'est pas « faite » quand le script se termine sans erreur —
> elle l'est quand le PNG a été REGARDÉ.**
>
> Toutes les pannes listées plus bas passent l'intégralité des contrôles
> automatiques : XML bien formé, poids plausible, code de retour nul. Aucune ne
> survit à un coup d'œil.
>
> Et le coup d'œil attrape ce qu'aucun contrôle ne peut voir : dans la première
> version de la planche 05, le nœud légendé « hors cadre » était dessiné
> **dedans**. **La planche contredisait en image la règle qu'elle enseignait en
> texte.** Le SVG était valide, le PNG au bon poids, le script sans erreur.

Les planches `../planche_0*.svg` **ne sont pas dessinées à la main** : elles sont
produites par ces scripts. Ils vivaient dans un dossier temporaire de session —
c'est-à-dire nulle part. Ils sont ici pour que les planches restent
**modifiables**, et pas seulement consultables.

## Usage

```sh
cd generateurs/
python p05.py          # écrit ../planche_05_matieres.svg
```

Puis le rendu PNG, **avec une URL absolue** :

```sh
msedge --headless=new --disable-gpu \
  --screenshot=../planche_05_matieres.png \
  --window-size=1800,1580 \
  --allow-file-access-from-files \
  "file:///D:/.../references/planche_05_matieres.svg"
```

## Trois pièges, tous silencieux

Chacun a réellement produit un fichier **valide et faux**. Aucun n'a levé
d'erreur. Ils sont notés ici parce qu'ils se reproduiront.

1. ⚠️ **Chemin relatif passé à Edge.** Edge le prend pour un **nom d'hôte**,
   échoue en DNS, et **capture sa propre page d'erreur** : le PNG a la bonne
   taille et le bon nom. Seul son **poids** trahit (quelques dizaines de Ko au
   lieu de quelques centaines). Toujours `file:///` + chemin absolu.

2. ⚠️ **Demi-caractères jetés au lieu d'être recombinés.** Un caractère hors du
   plan de base écrit en deux moitiés dans un script était supprimé à
   l'écriture. La planche 02 a ainsi perdu deux marqueurs sans qu'aucun contrôle
   ne bronche. `gen.ecrire` recombine désormais avant de filtrer — **ne pas
   remettre le filtre seul**.

3. ⚠️ **Un script qui ne tourne plus ne dit pas qu'il est périmé.**
   Voir `p04.py` ci-dessous.

**Règle : une planche n'est pas « faite » quand le script se termine sans
erreur — elle l'est quand le PNG a été REGARDÉ.** Les trois pannes ci-dessus
passent tous les contrôles automatiques : XML bien formé, poids plausible, code
de retour nul.

## État des scripts

| script | état |
|---|---|
| `gen.py` | la bibliothèque commune : palettes, `noeud`, `prise`, `fil`, `cartouche`, `ecrire` |
| `p02.py` | ✅ reproduit `planche_02_types.svg` à l'octet près |
| `p03.py` | ✅ reproduit `planche_03_formes.svg` à l'octet près |
| `p04.py` | ⛔ **PÉRIMÉ** — voir ci-dessous |
| `p05.py` | ✅ reproduit `planche_05_matieres.svg` à l'octet près |

⛔ **`p04.py` ne reproduit PAS la planche 04 commitée.** Il produit une planche
de 1060 px de haut là où la planche committée en fait 1230, avec des décalages
et un tracé de cycle différents : la version qui a réellement produit la planche
n'a pas été sauvegardée avant la coupure de courant du 22/08. **Le SVG committé
fait foi** ; le script est gardé comme point de départ. Pour le remettre à
niveau, comparer avec `../planche_04_etats.svg` — le SVG contient toutes les
coordonnées finales. `planche_01_noeuds.svg` n'a, lui, aucun script du tout.

**Leçon générale : le script est la source, le SVG n'en est que la sortie —
alors on commite les deux, dans le même geste.**
