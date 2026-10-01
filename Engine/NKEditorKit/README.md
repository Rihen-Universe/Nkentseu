# NKEditorKit

**Socle d'éditeur partagé de Nkentseu** — la coquille d'application dockable
réutilisée par les éditeurs maison (NKCode = IDE aujourd'hui, Nogee = éditeur de
moteur plus tard). NKEditorKit **n'invente rien** : il **assemble** le docking
complet déjà présent dans **NKUI** (`NkUIDockManager`, fenêtres, widgets) dans un
cadre « application d'édition » prêt à l'emploi.

> 2D pur (**NKCanvas / NKUI**). **Aucune dépendance** à NKRenderer (3D) ni à
> NKReflection. Un éditeur de code n'a pas besoin du moteur de jeu.

---

## Ce que le kit fournit

| Élément | Rôle |
|---|---|
| `NkEditorShell` | Fenêtre + cible de rendu + contexte NKUI + docking + **boucle principale**. L'app n'écrit pas une ligne de plomberie fenêtre/événements/rendu. |
| `NkEditorPanel` | Classe de base d'un panneau dockable (Explorateur, Inspecteur, Console, Viewport…). On dérive et on implémente `OnUI()`. |
| `NkEditorFrameContext` | Contexte passé à chaque `OnUI()` + **helpers ergonomiques** (`ec.Text`, `ec.Button`, `ec.Checkbox`, `ec.SliderFloat`…) au lieu des appels NKUI à 5 arguments. |
| `NkEditorCommand` + palette | Toute action = commande nommée, invocable via la **palette (Ctrl+P)**, un menu ou un raccourci. Point d'ancrage des futures extensions. |
| Barre de menus | **Fichier / Affichage / Fenêtre** générés automatiquement (+ menu applicatif optionnel). « Affichage » liste les panneaux, « Fenêtre » gère la disposition. |
| Layout | `SaveLayout` / `LoadLayout` (via `NkUIDockManager`), `ResetLayout`. |

---

## Exemple minimal

```cpp
#include "NKEditorKit/NkEditorKit.h"
#include "NKMemory/NkUniquePtr.h"
using namespace nkentseu;
using namespace nkentseu::editorkit;

class MonPanneau : public NkEditorPanel {
public:
    MonPanneau() : NkEditorPanel("Mon Panneau", NkEditorDockSide::NK_LEFT) {}
    void OnUI(NkEditorFrameContext& ec) override {
        ec.Text("Bonjour depuis NKEditorKit !");
        if (ec.Button("Cliquez-moi")) { /* ... */ }
        ec.Checkbox("Option", mOption);
    }
private:
    bool mOption = true;
};

int nkmain(const NkEntryState&) {
    // ⚠️ Le shell possède de gros états NKUI (gestionnaires fenêtres/dock) :
    //     l'allouer sur le TAS (NKMemory), JAMAIS sur la pile (> 1 Mo => stack overflow).
    auto shell = memory::NkMakeUnique<NkEditorShell>();
    if (!shell->Init({ "Mon Éditeur", 1280, 720 })) return -1;

    static MonPanneau panneau;
    shell->AddPanel(&panneau);
    shell->RegisterCommand("Fichier: Nouveau", &OnNew, nullptr, "Ctrl+N");

    return shell->Run();
}
```

> Le shell **ne possède pas** les panneaux : l'appelant garantit leur durée de vie
> (ici `static`). Allocation mémoire = **NKMemory** uniquement (`NkMakeUnique`,
> jamais `new`/`delete`).

---

## Navigateur de contenu : la variante « Unreal 5 » (2026-10-01)

`Components/NkContentBrowserModel.h` porte une cinquième variante,
`NkBrowserVariant::Unreal` (`SetVariantByName("unreal")`) : les **sept zones** du
Content Browser d'Unreal 5 (barre `+ Ajouter / Importer / Tout enregistrer`,
précédent / suivant, fil d'Ariane, verrou, Réglages ; sources repliables
*Favoris*, le projet, *Collections* ; puces de type ; recherche et tri ; cartes
d'Unreal avec bande de couleur du type et nom sur deux lignes ; cartes de dossier
à couleur choisie ; état vide ; « N éléments (M sélectionnés) »). Dessin :
`NkContentBrowserUnreal.cpp`. Le mixte (`grid`), la liste et la variante minimale
ne changent pas d'un pixel.

- **Gestes** (dans la variante) : sélection simple / Ctrl / Maj / cadre, dossiers
  compris ; glisser de toute la sélection ; clic droit qui choisit ;
  Ctrl+molette = taille des vignettes ; `focus` clavier rapporté à l'hôte.
- **Le composant signale, l'hôte agit** : menus (`ajouterDemande`,
  `reglagesDemandes`, `triDemande`, `deposeSources`…), champs de saisie (recherche,
  recherche d'une section, renommage en place : rectangle + tampon rapportés).
- **La touche de chaque application** : la greffe `vignetteApp` (ses icônes de
  types, ses vrais aperçus), ses natures (`kinds`), ses jetons.
- **Le disque** : `Components/NkContentBrowserDisque.h` — nouveau dossier, copier,
  déplacer, dupliquer, renommer, supprimer (corbeille), importer fichiers **et
  dossiers** ; tout est **confiné** à la racine de contenu, **jamais
  d'écrasement** (`_2`, `_3`), pas de cycle ; la mémoire `.nknavigateur`
  (couleurs de dossiers, favoris, collections) suit renommages et suppressions.

Premier consommateur : le tiroir « Contenu » d'UnkenyEditor (témoins e51 à e56 de
son banc). NKCraft et NKUIDesign gardent leurs variantes.

---

## Le lanceur de projets partagé (2026-10-01)

L'écran qu'on voit avant d'avoir un projet ouvert, façon Unreal Engine 5 /
Unity Hub, **un seul composant pour toute la famille** ; chaque application n'y
met que **sa touche, par la donnée** (nom, logo, couleur, modèles, extensions,
pages). Il servira aussi au futur Launcher du moteur (R23).

| fichier | rôle |
|---|---|
| `Components/NkProjectLauncherModel.h` | le modèle (projets, modèles, pages, identité), le style (rôles), les crochets, la demande rendue |
| `Components/NkProjectLauncherDraw.cpp` | la mise en page et les gestes ; 38 glyphes vectoriels (`NkLanceurPeindreGlyphe`) |
| `NkProjectLauncherHost.h` | monde NKGui : polices du lanceur, peinture + vrai champ de recherche, **capture sans fenêtre** (`NkLanceurCapturer`), recents sur disque (`NkLanceurRecents`) |
| `NkProjectLauncherShell.h` | coquille `NkEditorShell` : `NkLanceurCoquille::Brancher(shell)` (écran de démarrage plein corps) |
| `NkEditorRendererMemoire.h` | un `NkIEditorRenderer` sans fenêtre ni GPU : garde les textures, rasterise, écrit un PNG |
| `Components/NkProjectLauncherProbe.h` | la sonde (NKEditorKitTest, famille 31) |

Options : colonne de navigation facultative (`colonne`, incrustation dans une
application qui a la sienne), en-tête facultatif (`enTete`), disposition
`Cartes` (modèles illustrés en haut) ou `ColonneDroite` (actions et exemples à
droite, NKCode), liste détaillée (`details`, `groupe`), bouton propre à
l'application (`actionHote`), barre de titre dessinée (`chromeFenetre`),
bascule de thème, bande de version.

**Qui l'héberge :** NKCraft (`Shell/NkModelerWelcome.h`), NkAnimaEditor
(`NkAnimaLanceur.h`), NKUIDesign (`Lanceur.h`), PV3DE (`UI/PV3DELanceur.h`,
`MedicalUILayer`), NKCode (`Shell/NkHomeLanceur.h`, page Accueil, incrusté).
**Prêts à brancher** (leur interface est en travaux ailleurs) : Nogee
(`Shell/NogeeLanceur.h` : `NogeeBrancherLanceur(*shell, portes)`) et
UnkenyEditor (`Editeur/NkEditeurLanceur.h` : `NkEditeurLanceurHote::Peindre`
dans l'image) — chaque en-tête donne les lignes à ajouter.

**Photos sans fenêtre :** `<App>.exe --capture-lanceur=FICHIER.png
[--theme-lanceur=clair]` (NkAnimaEditor, NKUIDesign, PV3DE, NKCode),
`NK_CAPTURE_ACCUEIL=FICHIER.png NKCraft.exe`, `NKEditorKitTest.exe
--capture-lanceurs=DOSSIER` (Nogee, UnkenyEditor). État de la prise :
`NK_LANCEUR_PAGE`, `NK_LANCEUR_VUE=liste`, `NK_LANCEUR_FILTRE`, `NK_LANCEUR_TRI`.
Coquille : `NK_LANCEUR_CHOIX=modele:i|recent:i|nouveau` joue une demande,
`NK_FENETRE_CACHEE=1` crée la fenêtre sans la montrer.

---

## Démo

`Applications/NKEditorKitDemo/` — coquille à 4 panneaux (Explorateur à gauche,
Viewport + Inspecteur en onglets au centre, Console en bas) + palette de commandes.

```sh
jenga build --target NKEditorKitDemo --config Debug
```

---

## Limites connues / à faire

- **Disposition par défaut** : les splits **gauche** et **bas** sont propres ; le
  split **droite** du `NkUIDockManager` reste à affiner, donc le centre regroupe
  Viewport + Inspecteur en **onglets** (réarrangeables par glisser-déposer). La
  disposition L/C/R/B « parfaite » viendra avec un constructeur d'arbre de dock
  dédié (ou le chargement d'un layout JSON par défaut).
- **Palette de commandes** : exécution par **clic** et **flèches + Entrée** ; le
  **filtrage par frappe** arrivera avec l'intégration clavier texte complète.
- **Panneaux pilotés par réflexion** (inspecteur générique via NKReflection,
  éditeur de nœuds sérialisable) : différés jusqu'à maturité de
  **NKReflection ↔ NKSerialization** (cf. `ECOSYSTEM.md`).

Voir [`ARCHITECTURE.md`](ARCHITECTURE.md) pour la conception détaillée.
