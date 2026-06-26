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
