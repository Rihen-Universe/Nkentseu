# Project — l'intégration Jenga

> ⚠️ Squelette : pas encore de code. Voir l'[architecture](../../../ARCHITECTURE.md) et la
> [roadmap](../../../ROADMAP.md).

## Rôle
`Project` est le **pont vers Jenga**. C'est lui qui fait de NKCode un vrai IDE et pas juste un
éditeur : il sait **créer un projet** (à partir d'un modèle, il génère le `.jenga` et l'arborescence
de départ), l'**ouvrir** et le **gérer** (explorateur de fichiers du workspace), et surtout le
**construire et le lancer** en appelant la **CLI Jenga**, puis afficher le résultat dans un
**panneau de sortie**.

L'astuce clé : NKCode ne réimplémente **aucun** système de build. Il écrit/édite des `.jenga`
(Python + DSL) et délègue tout à Jenga (`jenga build`, `jenga run`…). Le panneau de sortie **parse
les erreurs** du compilateur et les rend cliquables (clic → l'Editor saute à la ligne fautive).

## Responsabilités
- **Modèles de projet** + génération du `.jenga` et de l'arbo de base.
- **Explorateur** de fichiers du workspace ; ouverture dans l'Editor.
- **Build / Run / (debug)** via la CLI Jenga ; **panneau de sortie** + erreurs cliquables.
- Édition assistée du `.jenga` (s'appuie sur l'outillage IDE de Jenga : stubs `useconfig`, etc.).

## Dépend de
**Jenga** (CLI + DSL), NKFileSystem, NKThreading (build asynchrone), le Shell (panneaux), l'Editor.

[Architecture](../../../ARCHITECTURE.md) · [Roadmap](../../../ROADMAP.md)
