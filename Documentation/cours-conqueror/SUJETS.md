# 60 sujets — concevoir des outils pour ceux qui fabriquent

Chaque sujet part d'un **manque réel** de ConquerorLab. Aucun n'est inventé pour
l'exercice : ce sont les endroits où l'outil, aujourd'hui, laisse son utilisateur
se débrouiller.

## Comment lire une fiche

| Colonne | Sens |
|---|---|
| **Manque** | ce qui ne va pas aujourd'hui, constatable en dix minutes d'usage |
| **Objectif** | ce que l'étudiant doit produire, et ce qu'il doit avoir compris |
| **Nature** | `D` conception pure (maquette, spécification, protocole) · `C` demande du code |
| **↑** | difficulté : ○ abordable · ◐ moyen · ● exigeant |

> **Un sujet marqué `D` n'est pas un sujet au rabais.** Spécifier ce qu'il faut
> faire, le maquetter et le tester sur quelqu'un est le cœur du métier ; écrire
> le code n'en est que la trace.

**Avant de commencer, quel que soit le sujet** : utiliser l'atelier une heure, et
noter les moments d'hésitation. Un sujet traité sans cette heure produit une
solution à un problème imaginé.

---

## A. Les totems — l'outil dont vous êtes l'utilisateur

Le système de totems (`travail/totems/`, chapitre 7 du cours) fonctionne, et il
est **volontairement rudimentaire** : pas de prévisualisation, pas de
vérification, aucun retour quand un fichier est mal nommé. C'est la famille de
sujets où vous êtes exactement la personne que l'outil sert.

**1. Prévisualisation du totem dans le panneau Joueurs** · `C` ↑◐
**Manque** : on choisit un totem dans une liste déroulante, par son nom. On ne
voit rien avant de lancer une partie.
**Objectif** : afficher une vignette du totem sélectionné, ses cinq niveaux côte
à côte. Décider quoi montrer quand un niveau n'a pas d'image — et défendre ce
choix.

**2. Validateur de nommage** · `C` ↑◐
**Manque** : `n0.PNG`, `N0.png`, `n0_1.png`, `n0_01.png` — trois de ces quatre
noms sont ignorés en silence. L'artiste croit avoir livré une image ; elle
n'existe pas pour l'atelier.
**Objectif** : un rapport qui liste les fichiers **ignorés** et dit pourquoi, en
langage d'artiste. « `n0_1.png` : la numérotation attend trois chiffres, essayez
`n0_001.png` ».

**3. Éditeur de cadence d'animation** · `C` ↑◐
**Manque** : l'animation tourne à 12 images/s, en dur, pour tous les totems.
**Objectif** : rendre la cadence réglable par totem. Décider **où vit ce
réglage** — un fichier à côté des images ? un champ dans l'interface ? — et
justifier au regard de qui doit pouvoir le changer.

**4. Planche-contact** · `C` ↑○
**Manque** : pour voir tous ses totems, il faut lancer autant de parties.
**Objectif** : un panneau qui affiche tous les totems, tous les niveaux, sur une
grille. C'est l'outil qu'on ouvre pour juger une cohérence d'ensemble.

**5. Détection de l'alpha manquant** · `C` ↑○
**Manque** : un PNG sans canal alpha donne un carré posé sur la cellule. Rien ne
prévient.
**Objectif** : détecter à la lecture et avertir. Question à trancher : refuser
l'image, ou l'accepter avec un avertissement ? Argumentez.

**6. Recadrage automatique sur le contenu** · `C` ↑●
**Manque** : deux artistes livrent des images de cadrages différents ; leurs
totems n'ont pas la même taille apparente sur le plateau.
**Objectif** : détecter la boîte englobante des pixels opaques et normaliser.
Attention : c'est une décision **destructrice de l'intention** — un totem
volontairement petit devient grand. Prévoyez de pouvoir la désactiver, et dites
pourquoi.

**7. Rechargement à chaud du dossier** · `C` ↑●
**Manque** : il faut cliquer « Recharger les totems » après chaque modification.
**Objectif** : détecter les changements de fichiers. Le piège est connu et
documenté : décoder des PNG soixante fois par seconde fige l'atelier. Concevez le
garde-fou avant d'écrire la surveillance.

**8. Point d'ancrage par totem** · `C` ↑◐
**Manque** : l'image est centrée sur la cellule. Un totem « posé sur ses pieds »
paraît flotter.
**Objectif** : permettre de déclarer un point d'ancrage. Décidez comment
l'exprimer sans demander à l'artiste d'ouvrir un fichier texte.

**9. Teinte automatique par joueur** · `C` ↑◐
**Manque** : quatre joueurs demandent quatre jeux d'images, ou tous se
ressemblent.
**Objectif** : proposer une teinte automatique de l'image selon le camp.
**Mesurez** ensuite si les quatre camps restent distincts en niveaux de gris —
c'est l'exigence du projet, et une teinte mal choisie la casse.

**10. Animation pilotée par les événements** · `C` ↑●
**Manque** : l'animation est une boucle d'attente. Un totem qui se fait retourner
ne réagit pas.
**Objectif** : déclencher une animation sur `TotemTransformed`, `Cascade`,
`FusionPerformed`. L'atelier lit déjà ces événements. Le vrai travail est de
concevoir **ce qui se passe quand deux événements se chevauchent**.

**11. Export d'une planche de référence** · `D`+`C` ↑○
**Manque** : rien ne permet de sortir une image montrant l'état des totems, pour
une revue ou un dossier.
**Objectif** : produire un PNG unique, lisible, exploitable dans une présentation.
Décidez de ce qui doit y figurer et de ce qui encombrerait.

**12. Budget mémoire des images** · `C` ↑◐
**Manque** : rien ne signale qu'un totem occupe 40 Mo de texture.
**Objectif** : afficher le coût par totem et un total. Choisissez le **seuil**
d'alerte et défendez-le : trop bas, il devient du bruit qu'on ignore.

---

## B. Les plateaux — la forme du terrain

**13. Éditeur de plateau à la souris** · `C` ↑●
**Manque** : le document de règles parle d'un plateau « éditable à la souris ».
**Cet éditeur n'existe pas** — on écrit du JSON à la main.
**Objectif** : le concevoir. C'est le plus gros sujet de la liste ; ne visez pas
la complétude, visez **la boucle la plus courte** entre une intention et un
plateau jouable.

**14. Vérificateur de symétrie** · `C` ↑◐
**Manque** : le document exige que les départs soient symétriques, et prévient
qu'un plateau asymétrique rend tout écart de winrate ininterprétable.
**L'atelier ne le vérifie pas.**
**Objectif** : détecter et signaler. Difficulté réelle : définir « symétrique »
sur une forme quelconque.

**15. Générateur de formes paramétriques** · `C` ↑◐
**Manque** : les formes viennent d'un script Python que seul l'auteur du projet
lance.
**Objectif** : amener dans l'interface ce que fait `boards/_generer.py` —
rectangle, croix, anneau, parallélogramme, avec leurs paramètres.

**16. Miniature dans la liste des plateaux** · `C` ↑◐
**Manque** : on choisit un plateau par son nom de fichier ; il faut le charger
pour le voir.
**Objectif** : dessiner une miniature. Question de conception : la calculer à
l'ouverture (lenteur au démarrage) ou à la demande (lenteur au survol) ?

**17. Comparateur de deux plateaux** · `C` ↑◐
**Manque** : comparer deux formes demande de les charger l'une après l'autre, de
mémoire.
**Objectif** : les afficher côte à côte, avec ce qui les distingue — nombre de
cases, voisinage moyen, cases bloquées.

**18. Palette de pinceaux** · `D`+`C` ↑◐
**Manque** : (dépend du sujet 13) poser des cases, des blocages et des départs
demande trois gestes différents.
**Objectif** : concevoir la palette d'outils d'un éditeur de plateau. Combien
d'outils ? Comment bascule-t-on ? Maquettez avant de coder.

**19. Import d'une image comme masque** · `C` ↑●
**Manque** : dessiner une forme complexe case par case est décourageant.
**Objectif** : lire une image en noir et blanc et en déduire les cases. Le vrai
sujet est la **conversion pixels → grille** et ce qu'on fait des bords.

**20. Détection des cases inatteignables** · `C` ↑◐
**Manque** : un plateau peut avoir des îlots isolés par des cases bloquées.
Personne ne le voit avant que la partie soit bizarre.
**Objectif** : détecter les composantes non connexes et les signaler visuellement.

---

## C. Lisibilité et langage visuel

**21. Test de lisibilité en niveaux de gris** · `D`+`C` ↑◐
**Manque** : le projet affirme que les quatre camps restent distincts en niveaux
de gris. **Personne ne l'a jamais vérifié.**
**Objectif** : un mode qui désature l'affichage, et un protocole de test avec de
vraies personnes. Rendez le verdict, même s'il contredit l'affirmation.

**22. Mode capture** · `C` ↑○
**Manque** : pour illustrer une règle, on capture l'écran entier, avec les
panneaux.
**Objectif** : un rendu propre du plateau seul, à une résolution choisie.
Décidez ce qui reste — le score ? le dernier coup ? — et pourquoi.

**23. Zoom minimal garantissant la lisibilité** · `D` ↑◐
**Manque** : sur un grand plateau, on peut dézoomer jusqu'à l'illisible sans
qu'aucun signal ne l'indique.
**Objectif** : **mesurer** à quel facteur chaque information devient illisible —
camp, niveau, dernier coup — et proposer un garde-fou. C'est un sujet
d'observation, pas de code.

**24. Trois signalisations du dernier coup** · `D` ↑◐
**Manque** : le dernier coup est un anneau orange qui pulse. C'est un choix, pas
un résultat de test.
**Objectif** : proposer trois alternatives, les maquetter, les faire départager
par cinq personnes sur une tâche précise (« quel a été le dernier coup ? »).

**25. Hiérarchie du bandeau de score** · `D` ↑○
**Manque** : le bandeau affiche totems, énergie et points de conquête avec un
poids visuel proche.
**Objectif** : déterminer par observation ce qu'on regarde vraiment, et
re-hiérarchiser. Justifiez par ce que vous avez vu, pas par le goût.

**26. Autre chose que l'anneau pour le niveau** · `D`+`C` ↑◐
**Manque** : le niveau se lit à la taille du disque et à un anneau. Au-delà du
niveau 2, la différence est ténue.
**Objectif** : proposer et tester d'autres codages. Contrainte non négociable :
**jamais la couleur seule**, elle porte déjà le propriétaire.

**27. Thème présentation** · `C` ↑○
**Manque** : les thèmes clair et sombre visent l'écran de bureau. Sur
vidéoprojecteur, tout est trop fin.
**Objectif** : un troisième thème, contrastes forts et éléments épaissis. Testez
en salle, pas sur votre écran.

**28. Repères de coordonnées** · `C` ↑○
**Manque** : pour parler d'une case à quelqu'un, il faut dire « celle-là, en
haut à gauche ».
**Objectif** : un affichage optionnel des coordonnées. Attention à la densité :
900 étiquettes ne servent à personne.

---

## D. Retour, erreurs, diagnostic

**29. Refonte du panneau Modules** · `D`+`C` ↑●
**Manque** : la sortie du compilateur s'affiche brute. Pour un débutant, une
erreur de template C++ fait quarante lignes illisibles.
**Objectif** : hiérarchiser — la première erreur en gros, le reste repliable.
C'est le seul retour qu'un stagiaire a ; c'est probablement le sujet le plus
utile de la liste.

**30. Traduction des erreurs fréquentes** · `D`+`C` ↑◐
**Manque** : `undefined reference to nkc_rules_get_factory` ne dit rien à qui a
oublié la macro d'export.
**Objectif** : constituer un dictionnaire des dix erreurs les plus fréquentes et
leur traduction en langage humain. Le travail est d'**observer** ce sur quoi les
gens butent réellement.

**31. Repenser la barre d'état** · `D` ↑◐
**Manque** : elle existe déjà, et elle a été ajoutée après qu'un utilisateur a
cru l'application en panne. Elle n'a jamais été testée.
**Objectif** : vérifier qu'elle dit la bonne chose dans les six situations, sur
de vraies personnes. Corrigez ce qui ne passe pas.

**32. Journal filtrable** · `C` ↑○
**Manque** : le journal liste tous les coups. Sur 300 coups, chercher les
cascades demande de tout lire.
**Objectif** : filtrer par joueur, par type de coup, par présence de
retournement.

**33. Notification non bloquante** · `C` ↑◐
**Manque** : quand une campagne se termine, rien ne le signale si le panneau
n'est pas visible.
**Objectif** : concevoir la notification. Le vrai sujet : **ce qui mérite
d'interrompre** et ce qui ne le mérite pas.

**34. Panneau Santé** · `D`+`C` ↑◐
**Manque** : les problèmes sont dispersés — incohérence dans *Modules*, réglage
modifié dans *Règles*, coups illégaux dans *Métriques*.
**Objectif** : un endroit unique qui dit ce qui ne va pas. Décidez ce qui y
entre : un panneau qui liste tout devient un panneau qu'on ferme.

**35. Différence entre deux positions** · `C` ↑◐
**Manque** : en rejeu, passer d'un coup au suivant demande de repérer le
changement à l'œil.
**Objectif** : mettre en évidence ce qui a changé entre deux positions.

**36. Aide contextuelle sur les paramètres** · `D`+`C` ↑○
**Manque** : le panneau *Règles* affiche `portee_duplication` sans dire ce que
ça fait.
**Objectif** : afficher une explication au survol. Où vit ce texte ? Le contrat
prévoit un champ `label` dans le schéma — suffit-il ?

---

## E. Mesure et présentation des données

**37. Export d'une campagne** · `C` ↑○
**Manque** : les résultats vivent à l'écran. Pour les analyser ailleurs, on
recopie à la main.
**Objectif** : exporter en CSV ou JSON. Décidez **ce qu'on exporte** : les
agrégats, ou chaque partie ?

**38. Comparateur de deux campagnes** · `C` ↑●
**Manque** : l'atelier signale quand la configuration a changé, mais ne montre
pas les deux résultats ensemble.
**Objectif** : afficher deux campagnes côte à côte, avec le diff de leur
signature. Le piège : rendre visible qu'elles **ne sont pas comparables** quand
elles ne le sont pas.

**39. Courbe de convergence du winrate** · `C` ↑◐
**Manque** : on lit un winrate final. On ne voit pas s'il s'est stabilisé.
**Objectif** : tracer son évolution au fil des parties. C'est ce qui répond à la
question « ai-je lancé assez de parties ? » — mieux qu'une règle apprise.

**40. Heatmap des cases jouées** · `C` ↑◐
**Manque** : rien ne dit quelles zones du plateau sont disputées.
**Objectif** : accumuler sur une campagne et afficher. Question sérieuse :
quelle échelle de couleur, et pourquoi pas l'arc-en-ciel ?

**41. Visualiser la réflexion d'une IA** · `C` ↑●
**Manque** : le contrat prévoit `GetDebugJson` — visites et valeur par coup.
**Rien ne l'affiche.**
**Objectif** : montrer ce que l'IA a envisagé. C'est l'outil qui permet à un
étudiant en IA de comprendre pourquoi la sienne joue mal.

**42. Rapport imprimable** · `D`+`C` ↑◐
**Manque** : pour rendre compte d'une mesure, on fait des captures d'écran.
**Objectif** : un rapport d'une page, auto-descriptif — la signature de
configuration en fait partie, sans quoi le chiffre ne vaut rien.

**43. Indicateur de significativité** · `D`+`C` ↑●
**Manque** : le cours dit « 200 parties pour une tendance, 1000 pour une
conclusion ». Ce sont des ordres de grandeur, pas une mesure.
**Objectif** : afficher un intervalle de confiance, et surtout **le rendre
compréhensible** par quelqu'un qui ne fait pas de statistiques.

---

## F. Flux de travail et vitesse d'itération

**44. Recharger un plateau sans perdre la partie** · `C` ↑◐
**Manque** : charger un plateau relance la partie. Corriger une case oblige à
tout recommencer.
**Objectif** : décider ce qui est conservable et ce qui ne l'est pas. Le sujet
est de **définir la règle**, pas de la contourner.

**45. Configurations enregistrées** · `C` ↑◐
**Manque** : régler moteur, IA, paliers, budget et plateau prend deux minutes, à
refaire à chaque lancement.
**Objectif** : enregistrer et rappeler des configurations nommées.

**46. Historique des essais** · `D`+`C` ↑◐
**Manque** : une semaine plus tard, personne ne se souvient de ce qui avait été
essayé.
**Objectif** : garder trace des configurations passées et de leur résultat.
C'est le remède à ce que la signature de campagne ne fait que **diagnostiquer**.

**47. Comparaison A/B** · `C` ↑●
**Manque** : comparer deux jeux de paramètres demande deux campagnes séparées et
une mémoire fidèle.
**Objectif** : lancer les deux dans la même campagne, sur les mêmes graines, et
présenter l'écart.

**48. Raccourcis clavier configurables** · `C` ↑○
**Manque** : quelques raccourcis existent, figés dans le code.
**Objectif** : les rendre visibles et modifiables. Commencez par les **rendre
découvrables** — un raccourci qu'on ignore n'existe pas.

**49. Mode kiosque** · `C` ↑○
**Manque** : pour montrer le jeu, on montre un atelier avec sept panneaux.
**Objectif** : un mode plein écran, plateau seul. Décidez ce qui reste
accessible et comment on en sort.

**50. Disposition des panneaux mémorisée** · `C` ↑◐
**Manque** : le shell sait sauvegarder une disposition ; **l'atelier ne l'utilise
pas**. Tout est à replacer à chaque lancement.
**Objectif** : la brancher. Piège de conception : que se passe-t-il quand un
panneau nouveau apparaît dans une disposition ancienne ?

---

## G. Accessibilité

**51. Lisibilité sur petit écran** · `D` ↑◐
**Manque** : l'interface s'adapte sous 900 px, mais personne n'a vérifié qu'elle
reste utilisable.
**Objectif** : auditer, sur un vrai petit écran, et corriger ce qui casse.

**52. Palettes pour daltoniens** · `D`+`C` ↑◐
**Manque** : la palette actuelle a été *pensée* pour rester distinguable ; elle
n'a pas été *validée*.
**Objectif** : proposer des palettes alternatives et les faire valider par des
personnes concernées. Si aucune n'est disponible, un simulateur — en disant que
c'en est un.

**53. Navigation entièrement au clavier** · `C` ↑●
**Manque** : jouer un coup demande la souris.
**Objectif** : rendre le plateau jouable au clavier. Le vrai sujet est la
**désignation d'une case** sans pointeur.

**54. Audit des cibles tactiles** · `D` ↑○
**Manque** : le code impose un rayon minimal « pour Android ». Personne n'a
mesuré.
**Objectif** : mesurer sur du matériel réel et confronter aux recommandations
d'accessibilité. Rendez les chiffres.

**55. Lire l'état sans couleur** · `D`+`C` ↑●
**Manque** : le camp d'un totem est porté par la couleur. Sans elle, plus rien.
**Objectif** : proposer un codage complémentaire — motif, forme, symbole — sans
alourdir l'affichage. Sujet difficile, et c'est le bon genre de difficulté.

---

## H. Documentation et transmission

**56. Guide de démarrage illustré** · `D` ↑○
**Manque** : `LISEZMOI.txt` est du texte. Le cours fait 50 pages. Entre les deux,
rien.
**Objectif** : une page illustrée, du premier lancement au premier module chargé.
Testez-la sur quelqu'un qui n'a jamais vu l'atelier.

**57. Fiche de retour d'expérience** · `D` ↑○
**Manque** : « signalez ce qui casse » ne dit pas comment.
**Objectif** : concevoir la fiche qui obtient une information exploitable —
version, configuration, ce qui était attendu, ce qui s'est produit — **sans
décourager** celui qui la remplit. La tension entre les deux est le sujet.

**58. Vidéo de prise en main** · `D` ↑◐
**Manque** : aucune démonstration animée.
**Objectif** : script, captures, montage. Trois minutes maximum. L'exercice
oblige à choisir ce qui compte vraiment.

**59. Glossaire visuel des signes** · `D` ↑○
**Manque** : anneau vert, halo rouge, anneau orange qui pulse, « CASCADE ×N ».
Leur signification est dans le cours, pas sous les yeux.
**Objectif** : une planche qui montre chaque signe et ce qu'il veut dire.
Où doit-elle vivre pour être vue au bon moment ?

**60. Arbre de décision « je ne sais pas quoi faire »** · `D` ↑◐
**Manque** : le cours a une section « quand ça ne marche pas », mais il faut
savoir qu'elle existe et l'ouvrir.
**Objectif** : concevoir le parcours qui mène quelqu'un de « ça ne marche pas » à
la bonne page — dans l'interface, pas dans un document. C'est le sujet le plus
proche du métier réel : **rendre l'aide trouvable au moment du besoin**.

---

## Répartition

| Famille | Sujets | Dont conception pure |
|---|---|---|
| A — Totems | 1–12 | 0 |
| B — Plateaux | 13–20 | 0 |
| C — Lisibilité | 21–28 | 3 |
| D — Retour et erreurs | 29–36 | 1 |
| E — Mesure | 37–43 | 0 |
| F — Flux de travail | 44–50 | 0 |
| G — Accessibilité | 51–55 | 2 |
| H — Transmission | 56–60 | 5 |

Onze sujets ne demandent **aucun code**. Ils conviennent à qui ne programme pas —
et il serait bon que ceux qui programment en prennent un aussi.

## Trois sujets à recommander en priorité

- **29 (refonte du panneau Modules)** — c'est le seul retour qu'un débutant
  reçoit. L'améliorer aide immédiatement de vraies personnes.
- **21 (lisibilité en niveaux de gris)** — le projet affirme quelque chose que
  personne n'a vérifié. Un étudiant peut clore la question.
- **60 (arbre de décision)** — le plus proche du métier, et celui qui demande le
  plus d'observation pour le moins de technique.
