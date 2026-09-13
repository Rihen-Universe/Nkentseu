# Captures du 16 mai 2026 — un bug de rendu de Pong

**Ces images ont failli être supprimées avec du code mort.** Elles vivaient dans
`Applications/Pong copy/`, un dossier prouvé mort et supprimé le 27/08/2026. La
preuve qui l'a condamné portait sur les **sources** ; elle ne disait rien de ces
fichiers-ci, et c'est pour ça qu'ils sont ici et pas dans l'historique.

## Ce qu'on sait, et c'est peu

| | |
|---|---|
| **date** | 16 mai 2026, entre 10 h 38 et 10 h 45 |
| **sujet** | un défaut de rendu de Pong |
| **documentation** | ⚠️ **les noms des deux dossiers, et rien d'autre** |

```
ce_que_l_on_a_qui_est_faux/   8 captures  10:43:41 -> 10:45:10
ce_que_l_on_dois_avoir/      11 captures  10:38:40 -> 10:40:13
```

> **Cette paire de noms est leur seule documentation.** Elle dit ce qu'aucun nom
> de fichier ne dit : lesquelles montrent le défaut, lesquelles montrent
> l'attendu. C'est pour la préserver que les deux sous-dossiers ont été gardés
> tels quels au lieu d'aplatir les dix-neuf images dans un seul répertoire.

📌 **Et l'ordre des horodatages est une information** : l'attendu (10:38) précède
le défaut (10:43). Ces captures ont donc vraisemblablement été prises **dans cet
ordre** — d'abord ce qui marchait, ensuite ce qui a cassé.
*C'est une lecture des dates, pas un fait établi.*

## Ce qu'on ne sait pas, et qu'il ne faut pas inventer

- **Quel défaut exactement.** Aucune note, aucun rapport, aucun ticket associé.
- **Quel backend.** OpenGL, DX11, Vulkan — rien ne le dit.
- **⚠️ Quelle commande les reproduit.** Aucune.

## Pourquoi elles ne sont pas dans `Documentation/RenduTemoins/`

C'est l'emplacement établi des captures de référence du dépôt, et j'ai commencé
par là. **Son README pose une règle que ces images ne peuvent pas tenir :**

> *« Une capture sans la commande qui la reproduit ne vaut rien — la commande est
> donc écrite à côté de chaque fichier. »*

Ses témoins sont des **preuves d'état**, reproductibles, nommées par ce qu'elles
montrent (`demo23_dx11_solbleu.png`). Les dix-neuf ci-jointes sont des
**souvenirs** : horodatées par Windows, sans commande, sans backend.

> **Les y ranger aurait rendu la phrase de son README fausse pour dix-neuf de ses
> fichiers.** C'est la même faute que d'élargir la population d'un outil sans
> réécrire sa preuve : le dossier aurait gardé son titre en cessant de le mériter.

Elles sont donc chez **Pong**, à côté de ses propres captures — l'emplacement
établi du projet auquel elles appartiennent.

## Ce qui a été supprimé avec le dossier, et pourquoi

`Applications/Pong copy/docs/` contenait cinq documents — le GDD
`GDD_PONG_ULTRA_ARENA_v1.1.docx` et quatre pages HTML. **Vérifié par empreinte :
les cinq sont byte-identiques à ceux de `Applications/Pong/docs/`.** De vraies
copies, sans rien d'unique. Supprimées, pas déplacées.

*Les captures, elles, ne sont pas des doublons : `Applications/Pong/Captures/`
porte sept images du **20** mai, à plat, sans la distinction faux/attendu.*
