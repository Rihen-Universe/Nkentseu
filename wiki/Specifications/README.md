# Les spécifications des trois applications

**AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
**Déposées ici le 26/09/2026.**

> ⚠️ **Elles sont dans le dépôt parce qu'une série précédente a été perdue.**
> Rodolf, le 26/09 : *« même ça je ne trouve plus, donc ce que je vais faire
> c'est que je vais les refaire. »* Un document qui ne vit que dans `Downloads`
> ne survit pas à un nettoyage de disque. Ici, il est versionné, daté, et il
> revient avec le dépôt.

---

## Les neuf documents, trois par application

| | NKScena | Nogee / Noge | NkAnima |
|---|---|---|---|
| **le produit** | `01-produit-nkscena.md` | `04-produit-noge-nogee.md` | `07-produit-nkanima.md` |
| **l'interface, pour un humain** | `02-interface-humaine-nkscena.md` | `05-interface-humaine-nogee.md` | `08-interface-humaine-nkanima.md` |
| **l'interface, pour l'agent** | `03-interface-agent-nkscena.md` | `06-interface-agent-nogee.md` | `09-interface-agent-nkanima.md` |

**Le document « produit » est la cible réelle.** Rodolf, le 26/09 : *« le produit
est ce que je veux réellement. »* Les deux autres le servent — une interface n'a
de valeur que si elle porte le produit décrit dans le premier.

📌 **Les documents « agent » sont écrits pour celui qui écrit les `.nkgui`.** Ils
donnent l'arbre des éléments nommés, ce qui se répète (les composants), ce qui
agit (les actions), ce qui est de la couleur **et pourquoi** — plus ce que la
chaîne ne sait pas encore reproduire, **écrit quand même**. C'est la meilleure
forme de spécification que ce dépôt ait reçue.

---

## ⚠️ La version qui fait foi

Trois documents existaient en double dans `Downloads`, et **les deux copies
différaient**. Les versions retenues ici sont les plus récentes (19h44-19h47
contre 18h01) :

| document | écart |
|---|---|
| `07-produit-nkanima` | 11 lignes |
| `09-interface-agent-nkanima` | 16 lignes |
| `03-interface-agent-nkscena` | 9 lignes |

Et l'écart n'était pas cosmétique. Exemple, dans `07` : la version ancienne
disait que les VFX étaient « la partie la plus neuve du produit » ; la récente
corrige — **le socle VFX vit dans NKRenderer** (`NkVFXSystem`,
`NkParticleStore`/`NkParticleStoreGPU`, `NkSPHSolver`, `NkFluidGrid` +
`NkFluidGridRaymarch`, `NkForceField`, `NkSplashEmitter`, `NkTerrainSable`), et
ce qui manque est surtout **l'édition** et **la destruction**.

📌 *Une livraison par copie se périme en silence.* Travailler sur la version de
18h aurait conduit à reconstruire ce qui existe déjà.

---

## Ce que la chaîne `.nkgui` sait faire de ces documents, au 26/09

Relevé et mesuré le même jour. Voir `../Guides/Vocabulaire-nkgui.md`.

| ce que la spécification écrit | état |
|---|---|
| structure, composants, libellés | ✅ **monté** |
| couleur de fond, couleur de texte, rayon | ✅ **peint, sur tout rôle** |
| un composant nommé, instancié n fois | ✅ **développé avant le montage** |
| une bibliothèque de composants partagée (`include`) | ✅ **résolue** |
| `on = Click` · `on = Changed` · `on = Hover` | ✅ **déclenchés** |
| `on = DoubleClick` `ContextMenu` `Focus` `Blur` `KeyDown` `Wheel` `Submit` `DragStart` `Drop` | 🟡 **reconnus, comptés, sans source** |
| les états (`appearance(Hover)`) | 🟡 **écrits, comptés, non peints** |
| les animations | 🟡 **la section voyage, jamais jouée** |
| `font` `shadow` `stroke` | 🟡 **comptés non peints** |
| rotation, miroir, perspective, opacité, fusion | ❌ **aucun vocabulaire** |

⚠️ **Écrivez quand même ce qui n'est pas encore tenu.** Ces déclarations voyagent
dans le fichier et se comptent : le jour où elles seront branchées, le document
s'anime **sans être réécrit**. Ce qui n'aurait pas été écrit, lui, serait perdu.

---

## Ce que NKUIDesign doit encore recevoir

Mesuré le 26/09, et **les deux manques n'en font qu'un** :

- la **conversation** existe et est atteignable — `DesignChat.h`,
  `DesignChatAsync.h`, `DesignAI.h`, `DesignIABoutEnBout.h`, une pastille
  « Chat IA » sur le rail droit, et un dorsal Claude (`NkConverseClaude.h`) ;
- l'**écriture d'un `.nkgui`** existe — `NkGuiEcrire.h`, *« le maillon qui
  manquait »* ;
- ⚠️ **mais rien ne relie les deux**, et l'écriture n'est atteignable que par un
  drapeau de ligne de commande (`--recette-ecrivain`). Les exports que le menu
  propose sont SVG et PNG — des images, précisément ce dont une application n'a
  pas besoin.

> **Pour les applications qui tournent sur NKGui et NKEditorKit, la source est le
> `.nkgui`, jamais une image.** Une image ne se monte pas, ne porte ni rôle, ni
> comportement, ni identifiant.

📌 **Et la cible dépasse le PC** : ces designs visent applications **et jeux**,
sur **PC, web et mobile** pour commencer — puis la conversion vers HTML/CSS/JS,
Java, C#, Qt. C'est ce qui rend le format central : *le document est la source,
et les cibles sont multiples.* Un design qui vivrait dans du C++ ne pourrait
servir qu'une seule d'entre elles.
