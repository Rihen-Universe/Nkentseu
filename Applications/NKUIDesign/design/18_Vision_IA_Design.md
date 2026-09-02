# 18 — Vision : l'IA qui design (Rodolf, 02/09) — quatre étages, rien de lancé

> **Statut : document de VISION.** Rien ici n'est codé ni engagé — c'est
> l'ordre de livraison qui est décidé, pas le calendrier. Consigné pour que la
> vision ne se redécouvre pas de mémoire.

## Les mots de Rodolf

> *« À partir de plusieurs composants, dire à l'IA de designer toute une
> interface — là c'est juste du rangement, du Lego. On décrit notre
> application et son schéma, on peut demander d'implémenter page par page ;
> dès que des informations comme boutons sont spécifiées il peut les
> reconnaître comme composants — soit utiliser des composants existants, soit
> demander si on doit designer ces composants de zéro ; et dans ce cas il
> propose un wireframe et part de ce wireframe pour proposer des designs de
> composants et designer entièrement les interfaces, page après page. »*

## 🔴 Les deux règles charnières — AVANT les étages, parce qu'elles les gouvernent tous

1. **Quand un composant manque, l'IA DEMANDE** (« on le designe de zéro ? ») —
   elle n'invente jamais un composant en douce. C'est le principe §15.11
   (« composant = acte explicite ») appliqué à l'IA.
2. **Toute sortie IA passe par la même porte que la main.** Une page générée
   est un document ordinaire : éditable, annulable, qui obéit à Q51 (§15.6)
   comme n'importe quelle page. Aucune donnée à part, aucun statut spécial.

## Les quatre étages, avec leurs dépendances — c'est l'ordre de livraison

### 1. LEGO — composer depuis l'existant *(le premier à livrer)*

L'IA ne dessine pas : elle **range** — des instances et des agencements, la
même donnée que la main. Dépendances : le registre, la palette, la porte
d'instanciation (`InstancierComposant`) — **tout existe déjà**. Passe par la
place IA existante : prompt, backend remplaçable, sortie par la porte commune.

### 2. PAGE PAR PAGE — la description de l'application

On décrit l'application et son schéma ; l'IA implémente **page après page**,
chaque page étant un point de validation — jamais douze pages d'un coup à
reprendre.

### 3. RECONNAISSANCE — « bouton » → composant du catalogue

Le vocabulaire existe déjà : **étiquettes + filiation + recherche** (les
décisions du jour). ⚠️ L'ambiguïté est un cas de conception, pas un détail :
« bouton » peut matcher douze composants → l'IA **propose les candidats** et
l'utilisateur choisit, ou elle choisit par contexte **en le disant** — jamais
un choix silencieux.

### 4. DEPUIS ZÉRO — wireframe, puis composants, puis pages *(le plus lointain)*

⚠️ **Le wireframe est un DOCUMENT** : des formes basse-fidélité éditables,
corrigées AVANT le raffinement — pas douze pages finies à reprendre. Puis des
designs de composants proposés depuis le wireframe, puis les interfaces, page
après page. Dépend du corpus IA de design (chantier séparé, le plus lointain).

## Ce que chaque étage suppose de vrai — pour ne pas se mentir en le lançant

| étage | dépend de | existe au 02/09 |
|---|---|---|
| 1. Lego | registre, palette, `InstancierComposant`, place IA | ✅ tout |
| 2. Page par page | 1 + le format de description d'application | 🟡 la description reste à concevoir |
| 3. Reconnaissance | étiquettes, filiation, recherche + règle d'ambiguïté | 🟡 vocabulaire oui, règle d'ambiguïté à concevoir |
| 4. Depuis zéro | wireframe-document + corpus IA de design | ❌ les deux |
