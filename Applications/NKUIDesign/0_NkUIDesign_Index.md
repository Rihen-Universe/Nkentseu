# NKUIDesign — index des documents et ordre d'autorité

### Document 0

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen** — 27/09/2026.
> Écrit lors de la réécriture des documents 1 à 21 (Rodolf : *« réécris tous les
> documents si tu le juges nécessaire, de 1 à 21 »*). Il dit **quoi lire, dans quel
> ordre, et qui gagne** quand deux documents se contredisent.

---

## 1. Les documents

| # | document | nature | état au 27/09 | fait foi pour |
|---|---|---|---|---|
| 0 | `0_NkUIDesign_Index.md` | index | nouveau | l'ordre d'autorité |
| 1 | `1_NkUIDesign_Specification_Application.md` | **normatif** | **réécrit** (absorbe l'ancien 19) | **le produit** : positionnement, principes, modules, phasage, décisions |
| 2 | `2_NkUIDesign_Langage_Description_NodeBlueprint.md` | **normatif** | **réécrit** — format **0.4** | **le langage** : grammaire complète, sens, rôles, événements, script, **Blueprint**, instructions, conditions, formes, carte, tests, **thèmes**, diagnostics |
| 3 | `design/3_NkUIDesign_Interface_Humaine.md` | **normatif** | **réécrit** (absorbe l'ancien 20), numérotation gardée | **ce qui apparaît à l'écran** |
| 4 | `design/4_NkUIDesign_Brief_Banani.md` | archive | en-tête de statut | rien (leçons de méthode, planches validées) |
| 5 | `design/5_NkUIDesign_Specification_Claude.md` | **normatif** | **réécrit** (absorbe l'ancien 21) | **le modèle** d'édition et **l'interface de NKUIDesign écrite en `.nkgui`** (arbres, actions) |
| 6 | `design/6_IA_de_design_cadrage.md` | historique | en-tête de statut | rien (cadrage du corpus de l'IA) |
| 7 | `design/7_Vocabulaire_NkUI.md` | **normatif** | **réécrit**, réconcilié avec le validateur | **les rôles** (avec `NkGuiValidate.h`) et les **rôles proposés** |
| 8 | `design/8_Inventaire_de_l_existant.md` | instantané | en-tête de statut | la règle « chercher avant de spécifier » |
| 9 | `design/9_Grammaire_complete.md` | **justification** | mis à jour, §9 ajouté | **pourquoi** chaque construction est ainsi ; l'état d'implémentation de la 0.3 |
| 10 | `design/10_IA_entree_pipeline.md` | état daté | en-tête de statut | rien (le pipeline livré est au doc 1 §6.2) |
| 11 | `design/11_Banani_depouille.md` | référence géométrique | en-tête de statut | la **géométrie** des écrans V2 |
| 12 | `design/12_Dessiner_vers_Composant.md` | note de conception | en-tête de statut | les pièges du §12.4, la table du §12.3(b) |
| 13 | `design/13_Lunacy_reference_interaction.md` | **mesure** | **non touché** (lu par `compte_etat.py`) | l'état de la parité Lunacy, ligne par ligne |
| 14 | `design/14_Etat_conformite.md` | **mesure** | **non touché** (généré) | le compte de la parité |
| 15 | `design/15_Modele_Composants.md` | **normatif** | §15.16 **ajouté** (citations protégées) | **le modèle des composants**, et son passage au `.nkgui` |
| 16 | `design/16_Etat_apparence.md` | **mesure** | **non touché** (généré) | l'échelle d'espacement `2/4/8/12/16/24` |
| 17 | `design/17_Cote_a_cote.md` | mesure | non touché | les écarts avec les planches |
| 18 | `design/18_Vision_IA_Design.md` | vision | non touché (citations protégées) | les quatre étages de l'IA |
| 19, 20, 21 | *(refonte, 27/09)* | — | **retirés** : fondus dans 1, 3 et 5 | — |

---

## 2. Qui gagne

1. **Le langage** : doc 2 > doc 9 > doc 7 pour la syntaxe et le sens ; **doc 7 +
   `NkGuiValidate.h`** pour la liste des rôles (un écart entre eux est un défaut à
   corriger des deux côtés).
2. **L'écran** : doc 3 > doc 5 > docs 4, 11 (planches). Si le doc 5 contredit le doc 3,
   le doc 5 est à corriger.
3. **Le produit** : doc 1 > tout document sur les priorités, le phasage et les décisions.
4. **Les composants** : doc 15 > doc 12.
5. **Les mesures** (13, 14, 16, 17) disent **ce qui est** ; elles ne décident de rien et
   ne sont jamais contredites par un document normatif sur un fait mesuré — un
   document normatif qui écrit « livré » doit s'appuyer sur elles.
6. **Le code gagne sur tous les documents pour ce qu'il fait réellement** ; un document
   qui le contredit est faux sur ce point et se corrige (principe 14 du doc 1 :
   chercher avant de spécifier).

---

## 3. Ordre de lecture

| tu es | lis |
|---|---|
| **Rodolf, pour décider** | doc 1 §10 (décisions), puis doc 1 §9 (phasage) |
| **l'agent qui écrit l'interface de NKUIDesign** | doc 5 (en entier), doc 3 pour chaque écran, doc 2 pour le langage, doc 7 pour les rôles |
| **l'agent qui implémente l'éditeur** | doc 1, doc 5 §1 à §3, doc 2, doc 15, puis doc 3 section par section |
| **un designer** | doc 3, puis doc 2 §13 à §20 |
| **quelqu'un qui découvre** | doc 1 §0 à §4, puis doc 3 §4 |

---

## 4. Les décisions ouvertes (à trancher par Rodolf)

| # | question | recommandation | où |
|---|---|---|---|
| D3 | couleur de la sélection sur la toile : ambre (règle de la famille) ou bleu (habitude Figma) | ambre, réglable par un jeton | doc 1 §10, doc 3 §2.2 |
| D12 | action portée par un enfant de composant | (a) racine pour une action, (b) `id` qui échappe au préfixe pour plusieurs | doc 2 §14.3 |
| D13 | Ctrl+A ou A pour tout sélectionner dans la famille | Ctrl+A ici | doc 1 §10 |
| — | paramètres d'événement, emplacements de composant (P9), raccourci propre à un widget (P8), `Dragged` | avec le premier besoin réel | doc 9 §9.7 |
| — | ordre d'entrée des rôles proposés au vocabulaire | P2, P3, P4, P6, P5, P14, P22, puis P23 | doc 7 §3.10 |
| — | `Host` : garder ce nom | le garder (un alias suffirait) | doc 7 §7 |
| — | provenance d'un composant tiers : section du format ou `.meta` | `.meta` | doc 15 §15.16 |
| — | orange unique de la famille (`#F2980E`) ou `#F79A28` de NkAnima | `#F2980E` | doc 3 §2.2, spécifications NkAnima |

**Décidé le 27/09** : format 0.4 (D8), thème Rihen UE5 (D2), thème par application
(D10), exécution des Blueprints (D11).

**Proposées** : D1, D4, D5, D6, D7, D9 (doc 1 §10) — proposées, appliquées dans ces
documents en attendant ta confirmation.
