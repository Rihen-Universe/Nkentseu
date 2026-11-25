---
titre: Les exemples de NKPlatform
resume: Des programmes complets, compilés et exécutés à chaque construction de la documentation, avec ce qu'ils affichent.
ordre: 10
cles: exemples programmes complets octets réseau environnement
---
<!-- AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen -->

Chaque exemple est un fichier de `Docs/exemples/Foundation/NKPlatform/`. Jenga le construit comme un programme console, puis `jenga docs exemples` le lance et compare sa sortie à ce qui est attendu. Un exemple qui ne compile plus, ou qui n'affiche plus la même chose, bloque la construction du site.

## Savoir pour quoi le programme a été compilé

{{exemple:Foundation/NKPlatform/detection}}

## Interroger le processeur à l'exécution

{{exemple:Foundation/NKPlatform/processeur}}

## Rediriger le journal de fondation

{{exemple:Foundation/NKPlatform/journal_fondation}}

## Lire et convertir l'ordre des octets

{{exemple:Foundation/NKPlatform/ordre_des_octets}}

## Lire une variable d'environnement, reconnaître une variable absente

{{exemple:Foundation/NKPlatform/variables_environnement}}

{{si-version:dev}}
## Écrire, lire et retirer une variable (version de travail)

{{exemple:Foundation/NKPlatform/environnement_ecrire}}
{{fin-si}}
