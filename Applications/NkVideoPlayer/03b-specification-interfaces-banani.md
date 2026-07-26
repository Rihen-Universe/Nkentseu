# Cahier de Spécification des Interfaces — Version Progressive pour Banani

**Version :** 1.0
**Date :** 21 juillet 2026
**Public visé :** outil de génération de design IA (Banani), à utiliser comme suite de prompts à fournir un par un, dans l'ordre
**Document source :** `03a-specification-interfaces-humain.md`

---

## 0. Mode d'emploi de ce document

Ce document n'est **pas** destiné à être lu d'un bloc par un humain, mais à être **copié-collé progressivement, prompt par prompt, dans Banani**, dans l'ordre indiqué. Chaque bloc correspond à un prompt autonome, formulé pour que l'outil génère une brique de design cohérente avec les précédentes.

**Règle d'usage :** ne pas envoyer deux prompts à la fois. Envoyer le Prompt 1, valider/ajuster le résultat, puis envoyer le Prompt 2 en faisant référence au résultat précédent si l'outil le permet ("en cohérence avec le design system précédent"), et ainsi de suite.

**Convention de style commune à rappeler dans chaque prompt si besoin :**
> Style moderne, sombre par défaut (dark mode), épuré, orienté streaming vidéo premium, inspiré des plateformes de SVOD, avec des accents de couleur vive pour les actions principales et les éléments interactifs (lecture, boutons d'action, indicateurs de segmentation Drama).

---

## PROMPT 1 — Design System / Fondations

```
Crée un design system pour une application de streaming audio/vidéo multiplateforme (web, mobile, desktop), style moderne et épuré, dark mode par défaut, inspiré des plateformes de streaming premium.

Définis :
- Palette de couleurs (fond sombre, couleur d'accent principale pour les CTA, couleur secondaire, couleurs d'état : succès, erreur, avertissement, information)
- Typographie (police principale, hiérarchie de tailles : titres, sous-titres, corps de texte, légendes)
- Système d'espacement (grille de base 4px ou 8px)
- Style des boutons (primaire, secondaire, tertiaire, désactivé) pour PC (souris) et mobile (tactile, cibles plus grandes)
- Style des cartes/vignettes de contenu (avec image, titre, badge de durée, badge "Drama" pour segment gratuit)
- Icônes de base : lecture, pause, avance rapide, retour rapide, volume, plein écran, sous-titres, langue, paramètres, chat, utilisateur, recherche, notification
```

---

## PROMPT 2 — Écran Splash / Onboarding

```
En cohérence avec le design system précédent, crée l'écran d'onboarding d'une app de streaming audio/vidéo.

Contenu :
- Logo centré
- 3 slides de présentation avec illustration + titre court + description courte, sur les thèmes : "Streaming adaptatif toute résolution", "Multilingue : audio et sous-titres", "Regardez ensemble, où que vous soyez"
- Points de pagination en bas
- Deux boutons en bas : "Créer un compte" (primaire) et "Se connecter" (secondaire)

Génère la version mobile (portrait) et la version PC (format large paysage).
```

---

## PROMPT 3 — Écrans d'Authentification

```
En cohérence avec le design system, crée trois écrans d'authentification :
1. Connexion : champ email, champ mot de passe, bouton "Se connecter" (primaire), lien "Mot de passe oublié ?", lien "Créer un compte"
2. Inscription : champ nom, champ email, champ mot de passe, champ confirmation, case à cocher CGU, bouton "S'inscrire"
3. Mot de passe oublié : champ email, bouton "Envoyer le lien", message de confirmation

Formulaires centrés, fond sombre, champs avec bordure fine qui s'illumine en couleur d'accent au focus. Version mobile et version PC (formulaire centré dans un panneau plus étroit sur PC).
```

---

## PROMPT 4 — Écran Accueil (Home)

```
En cohérence avec le design system, crée l'écran d'accueil principal de l'app.

Structure :
- Barre de navigation (Accueil, Recherche, Bibliothèque, Watch Party, Profil) — en haut pour PC, en bas (tab bar) pour mobile
- Bandeau "Continuer à regarder" avec vignettes horizontales scrollables montrant une barre de progression sur chaque vignette
- Section "Recommandé pour vous" avec grille de vignettes de contenu
- Section par catégories (ex. "Films récents", "Podcasts vidéo")
- Sur certaines vignettes : badge "12 min gratuites" (mode Drama) en overlay coin supérieur

Génère la version PC (grille large multi-colonnes) et la version mobile (défilement vertical, vignettes plus grandes, 1 à 2 colonnes).
```

---

## PROMPT 5 — Écran Recherche

```
En cohérence avec le design system, crée l'écran de recherche.

Éléments :
- Champ de recherche en haut avec icône loupe
- Chips de filtres sous le champ (Genre, Langue, Durée, Type de contenu)
- Résultats sous forme de grille de vignettes
- État vide : affichage de "Recherches récentes" et "Tendances"

Version PC et mobile.
```

---

## PROMPT 6 — Fiche Contenu

```
En cohérence avec le design system, crée l'écran "Fiche Contenu" (page de détail d'un film/vidéo avant lecture).

Éléments :
- Grande image de couverture en haut (bannière)
- Titre, métadonnées (durée, année, genre) sous l'image
- Description du contenu
- Rangée d'icônes/badges : langues audio disponibles, langues de sous-titres disponibles, qualité maximale disponible
- Si contenu en mode Drama : bandeau informatif "12 premières minutes gratuites, puis paiement à la minute ou déblocage complet"
- Bouton principal "Regarder" (grand, couleur d'accent, avec icône lecture)
- Bouton secondaire "Ajouter à ma liste"
- Bouton secondaire "Créer une Watch Party"
- Section "Contenus similaires" en bas (grille horizontale)

Version PC (bannière large, boutons alignés horizontalement) et mobile (bannière pleine largeur, boutons empilés).
```

---

## PROMPT 7 — Lecteur vidéo : vue de base

```
En cohérence avec le design system, crée l'interface du lecteur vidéo en plein écran.

Éléments :
- Zone vidéo en plein écran (placeholder image de film)
- Barre de progression en bas, fine, avec un curseur au survol, et une portion de la barre visuellement distincte (couleur différente ou hachurée) pour représenter un segment payant du mode Drama, par opposition à la portion gratuite
- Rangée de contrôles sous la barre de progression : bouton lecture/pause (central, grand), retour 10s, avance 10s, volume avec slider, temps écoulé / temps restant, bouton plein écran
- Rangée d'icônes secondaires en haut à droite : Qualité, Langue/Sous-titres, Vitesse, Chapitres, IA (icône étincelle), Watch Party (icône personnes), Paramètres

Style : contrôles semi-transparents sur fond sombre dégradé, qui s'estompent après quelques secondes d'inactivité (mobile) ou restent visibles au survol (PC).

Génère la version PC et la version mobile (contrôles tactiles plus grands, disposition adaptée en portrait ou paysage).
```

---

## PROMPT 8 — Menus du Lecteur (Qualité, Langue, Vitesse, Chapitres)

```
En cohérence avec le lecteur vidéo précédent, crée 4 menus contextuels qui s'ouvrent en overlay ou en panneau latéral depuis les icônes de la barre de contrôles :

1. Menu Qualité : liste verticale (Auto, 360p, 720p, 1080p, 4K) avec coche sur l'option active
2. Menu Langue & Sous-titres : deux colonnes ou deux sections — "Audio" (liste de langues) et "Sous-titres" (liste de langues + option Désactivé), avec un lien "Personnaliser l'affichage" menant à des réglages de taille/police/couleur
3. Menu Vitesse : liste de vitesses (0.5x à 2x) avec coche sur l'option active
4. Menu Chapitres : liste de vignettes miniatures avec titre de chapitre et timestamp, scrollable verticalement

Style cohérent avec le lecteur (fond sombre semi-transparent, texte clair, option active en couleur d'accent). Version PC (panneau latéral droit) et mobile (tiroir remontant du bas, "bottom sheet").
```

---

## PROMPT 9 — Menu IA du Lecteur

```
En cohérence avec le lecteur vidéo, crée le panneau "Fonctionnalités IA" accessible depuis l'icône étincelle.

Éléments :
- Toggle "Sous-titres automatiques" avec sélecteur de langue cible qui apparaît si activé
- Bouton "Voir le résumé du contenu" qui ouvre une carte avec un texte de résumé généré
- Champ de recherche "Rechercher un moment dans la vidéo" (placeholder : "ex. la scène de la voiture") avec résultats sous forme de vignettes horodatées
- Toggle "Amélioration de la qualité (IA)"
- Toggle "Doublage automatique" avec sélecteur de langue cible

Ajoute un état "traitement en cours" avec une icône de chargement discrète à côté de chaque fonctionnalité en cours de génération.

Version PC (panneau latéral) et mobile (plein écran ou bottom sheet).
```

---

## PROMPT 10 — Overlay Mode Drama (fin de segment gratuit)

```
En cohérence avec le lecteur vidéo, crée deux états d'un overlay lié au mode de monétisation "Drama" :

1. Avertissement (non bloquant) : petite bannière discrète en haut de l'écran vidéo indiquant "Votre segment gratuit se termine dans 00:30", avec une icône d'horloge, qui disparaît après quelques secondes.

2. Blocage (à la fin du segment gratuit) : overlay plein écran semi-opaque sur la vidéo figée, avec :
   - Titre "Votre segment gratuit est terminé"
   - Deux cartes/options côte à côte (ou empilées sur mobile) :
     a) "Continuer à la minute" avec le tarif affiché (ex. "0,05€/min") et bouton "Continuer"
     b) "Débloquer le film entier" avec le prix affiché (ex. "2,99€") et bouton "Débloquer", mise en avant visuelle (badge "Meilleure offre" par exemple)
   - Lien discret "Retour à la fiche du film"

Style : cartes contrastées sur fond sombre, bouton principal en couleur d'accent, hiérarchie visuelle claire entre les deux options.
```

---

## PROMPT 11 — Écran Watch Party (création / salle d'attente)

```
En cohérence avec le design system, crée l'écran de création et d'attente d'une Watch Party.

Éléments :
- Titre "Créer une Watch Party"
- Aperçu du contenu sélectionné (vignette + titre)
- Bloc "Inviter des participants" avec un lien/code partageable et une icône de copie
- Liste des participants ayant rejoint (avatar + statut "prêt" ou "en attente")
- Bouton "Démarrer la session" (visible uniquement pour l'hôte, désactivé tant qu'aucun participant n'a rejoint si on le souhaite)

Version PC et mobile.
```

---

## PROMPT 12 — Panneau Watch Party intégré au lecteur

```
En cohérence avec le lecteur vidéo, crée le panneau Watch Party qui s'affiche en superposition pendant la lecture.

Éléments :
- Liste verticale des participants avec avatar, nom, badge "Hôte" pour celui qui contrôle la lecture
- Zone de chat avec messages entrants (bulle avec avatar et nom), champ de saisie en bas avec bouton d'envoi et accès rapide à des réactions emoji
- Bouton "Inviter" et bouton "Quitter la session" en haut du panneau

Style cohérent avec le lecteur, panneau semi-transparent sur fond sombre. Version PC (panneau latéral droit fixe, coexistant avec la vidéo) et mobile (tiroir qui remonte du bas, réductible).
```

---

## PROMPT 13 — Écran Bibliothèque personnelle

```
En cohérence avec le design system, crée l'écran "Ma Bibliothèque" (contenus personnels de l'utilisateur, distincts du catalogue).

Éléments :
- Bouton "Importer un fichier" en haut (icône +)
- Grille de vignettes des fichiers importés (avec durée affichée, pas de badge "Drama" — ces contenus sont toujours libres d'accès)
- Barre de recherche interne
- État vide : illustration + message "Votre bibliothèque est vide, importez votre premier fichier"

Version PC et mobile.
```

---

## PROMPT 14 — Écran Profil & Paramètres

```
En cohérence avec le design system, crée l'écran "Profil & Paramètres" avec une navigation par sections :

Sections (menu latéral sur PC, liste verticale cliquable sur mobile) :
- Compte (avatar, nom, email, bouton modifier, bouton supprimer le compte en rouge)
- Préférences de lecture (sélecteurs de langue audio/sous-titres par défaut, qualité par défaut, toggles des fonctionnalités IA par défaut)
- Accessibilité (taille des sous-titres avec slider, contraste élevé toggle)
- Paiement (liste de cartes/moyens de paiement enregistrés, bouton "Ajouter un moyen de paiement", historique des transactions sous forme de liste avec date/montant/contenu)
- Notifications (liste de toggles par type de notification)

Style formulaire propre, aligné avec le design system, cohérence visuelle avec les autres écrans.
```

---

## PROMPT 15 — Dialogues transverses

```
En cohérence avec le design system, crée une série de petites modales/dialogues réutilisables :

1. "Reprendre où vous étiez ?" — avec vignette du contenu, boutons "Reprendre" et "Recommencer"
2. "Connexion perdue" — icône d'alerte, message, indicateur de reconnexion en cours
3. "Confirmation de paiement" — récapitulatif (contenu, montant, moyen de paiement), bouton "Confirmer"
4. "Paiement échoué" — icône d'erreur, message, boutons "Réessayer" et "Changer de moyen de paiement"
5. "Consentement fonctionnalités IA" — texte court explicatif, boutons "Accepter" et "Refuser"

Toutes les modales doivent suivre un même gabarit : coins arrondis, fond légèrement plus clair que l'arrière-plan général, bouton principal en couleur d'accent, bouton secondaire discret.
```

---

## PROMPT 16 — Assemblage final / cohérence multiplateforme

```
Sur la base de tous les écrans générés précédemment (onboarding, authentification, accueil, recherche, fiche contenu, lecteur et ses menus, mode Drama, Watch Party, bibliothèque, profil, dialogues), vérifie et harmonise :
- La cohérence des couleurs, typographies et espacements sur l'ensemble des écrans
- La cohérence des composants réutilisés (boutons, cartes, badges, icônes) entre les versions PC et mobile
- La cohérence du badge "Drama" et des indicateurs de segment gratuit/payant partout où ils apparaissent
- La cohérence de la navigation principale (barre haute sur PC, tab bar en bas sur mobile) sur tous les écrans

Produis une planche de synthèse ("style guide" visuel) regroupant les composants clés côte à côte.
```

---

*Fin de la séquence de prompts. Chaque prompt peut être ajusté après retour de Banani avant de passer au suivant, afin de garantir la cohérence progressive du design.*
