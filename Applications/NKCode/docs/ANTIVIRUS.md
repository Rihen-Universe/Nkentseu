# NKCode et les antivirus

Des utilisateurs signalent que Windows Defender, ou leur antivirus, met NKCode
en quarantaine à l'installation ou au premier lancement. Cette page dit
pourquoi, ce que ça vaut, et ce qu'on fait pour que ça cesse.

---

## 1. Ce que c'est : un faux positif, et il est explicable

**Rien n'est infecté.** Le fichier signalé est le compilateur embarqué,
`clang.exe` de llvm-mingw, ou l'installeur qui le contient.

Trois raisons se cumulent, et aucune n'est un malware.

**Un. Rien n'est signé.** Vérifié le 10 septembre 2026 sur les livrables de la
bêta : `NKCode.exe` et `NKCode-<version>-win64-setup.exe` n'ont **aucune
signature Authenticode**. Pour Windows, ce sont des exécutables sans auteur
connu. SmartScreen affiche « Éditeur inconnu », et les moteurs heuristiques de
Defender considèrent l'absence de signature comme un facteur aggravant.

**Deux. NKCode installe un compilateur, et un compilateur ressemble à un
malware.** Il écrit des exécutables sur le disque, il édite des liens, il lance
des processus enfants, il modifie le `PATH`. Ce sont exactement les
comportements que les moteurs comportementaux surveillent. Toutes les
distributions de chaînes de compilation connaissent ce problème, MinGW et
MSYS2 les premiers.

**Trois. La réputation se construit avec le temps.** Un installeur de 90 Mo
téléchargé quelques dizaines de fois n'a aucun historique. Les moteurs
modernes, Defender en tête, pondèrent lourdement la rareté d'un fichier.

**Comment reconnaître ce faux positif** : le nom de la détection finit
généralement par `!ml`, par exemple `Trojan:Win32/Wacatac.B!ml` ou
`Program:Win32/Wacapew.C!ml`. Le suffixe `!ml` signifie *machine learning* :
c'est une **présomption statistique**, pas une signature de malware connu. Une
vraie détection nomme une famille précise, sans `!ml`.

**Ce qui est vérifié aussi** : aucun binaire livré n'est compressé par UPX ou
un autre empaqueteur. Les sections des exécutables sont ordinaires
(`.text .data .rdata .pdata .xdata .bss .edata .idata`). L'empaquetage est la
première cause de faux positifs, et NKCode n'en fait pas.

---

## 2. Ce qu'on fait pour que ça cesse

Par ordre d'efficacité réelle, pas de facilité.

### a. Signer les binaires (le vrai correctif)

Un certificat Authenticode au nom de **Rihen** résout le problème à la racine :
SmartScreen nomme l'éditeur au lieu de dire « inconnu », et l'absence de
signature cesse d'alourdir le score heuristique.

| Type de certificat | Effet | Ordre de grandeur |
|---|---|---|
| **OV** (validation d'organisation) | la réputation se construit sur quelques semaines de téléchargements | 200 à 400 € / an |
| **EV** (validation étendue) | réputation SmartScreen **immédiate**, clé sur jeton matériel | 350 à 600 € / an |

À signer : `NKCode.exe`, l'installeur, et idéalement les binaires de la chaîne
d'outils embarquée. La commande est `signtool sign /fd SHA256 /tr <serveur
d'horodatage> /td SHA256 <fichier>`. **L'horodatage n'est pas optionnel** :
sans lui, les binaires deviennent non fiables à l'expiration du certificat.

### b. Déclarer le faux positif à Microsoft (gratuit, quelques jours)

Portail : <https://www.microsoft.com/en-us/wdsi/filesubmission>, catégorie
« Software developer », en cochant *Incorrectly detected as malware*. Réponse
en 24 à 72 heures d'ordinaire, et la correction se propage par mise à jour de
définitions à tout le parc Defender.

À refaire **à chaque version**, tant que les binaires ne sont pas signés : la
détection porte sur des empreintes de fichiers, et une nouvelle version est un
nouveau fichier.

Pour les autres éditeurs, VirusTotal donne la liste de ceux qui détectent et
leurs formulaires de contact.

### c. Publier les empreintes, à chaque version

Un `SHA256SUMS.txt` dans la release, et le lien VirusTotal de l'installeur.
L'utilisateur peut alors vérifier lui-même que le fichier qu'il a téléchargé
est bien celui qu'on a publié :

```powershell
Get-FileHash .\NKCode-<version>-win64-setup.exe -Algorithm SHA256
```

C'est ce qui distingue un projet sérieux d'un exécutable trouvé sur un forum,
et cela ne coûte rien.

### d. Envisager de télécharger la chaîne d'outils au lieu de l'embarquer

Le compilateur est la partie signalée, et c'est aussi la plus lourde. Le
récupérer au premier lancement depuis la release officielle de llvm-mingw, au
lieu de le mettre dans l'installeur, allège l'installeur de plusieurs dizaines
de mégaoctets et déplace les octets suspects vers une origine que les moteurs
connaissent déjà.

Ce n'est pas gratuit : il faut alors une connexion à la première ouverture.
C'est un arbitrage, pas une évidence, et l'installeur « complet » existe
justement pour ceux qui n'ont pas de bonne connexion.

---

## 3. Ce qu'on ne fera pas

**On ne demandera jamais à l'utilisateur de désactiver son antivirus.** C'est
le conseil que donnent les logiciels douteux, et il est mauvais pour lui comme
pour nous. Au mieux, on lui indique comment ajouter **une exclusion sur le
dossier d'installation**, ce qui est ciblé et réversible :

> Sécurité Windows → Protection contre les virus et menaces → Gérer les
> paramètres → Exclusions → Ajouter une exclusion → Dossier →
> `C:\Program Files\NKCode`

Et on lui dit d'abord de vérifier l'empreinte du fichier qu'il a téléchargé.

**On ne cherchera pas à contourner la détection.** Modifier un binaire pour
qu'il échappe à un moteur d'analyse est exactement ce que fait un malware, et
cela se retournerait contre le projet à la première analyse sérieuse. La
signature et la déclaration de faux positif sont les deux voies légitimes, et
elles suffisent.

---

## 4. Texte à mettre dans les notes de version

> **Avertissement antivirus.** NKCode embarque un compilateur C++. Certains
> antivirus, dont Windows Defender, signalent les compilateurs par heuristique :
> ils écrivent des exécutables, éditent des liens et lancent des processus, ce
> qui ressemble de loin à un malware. Une détection dont le nom finit par `!ml`
> est une présomption statistique, pas une signature connue.
>
> Les empreintes SHA-256 de tous les fichiers de cette version sont dans
> `SHA256SUMS.txt`. Vérifiez la vôtre avant d'installer. Si votre antivirus
> bloque quand même, ajoutez une exclusion sur le dossier d'installation
> plutôt que de désactiver la protection.

---

*Établi le 10 septembre 2026, après vérification des binaires de la bêta :
aucune signature Authenticode, aucun empaquetage UPX.*
