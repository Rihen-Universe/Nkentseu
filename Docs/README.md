<!-- AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen -->
# Docs : la documentation publique de Rihen

Ce dossier produit le site `docs.rihen-universe.com` : le portail des projets de Rihen, puis, pour chaque projet,
son accueil et ses quatre espaces (Apprendre, Comprendre, Faire, Consulter). La documentation d'une version est
celle de son étiquette git : la référence se lit dans les en-têtes À L'ÉTIQUETTE publiée, jamais dans `main`.

| Dossier | Contenu |
|---|---|
| `site/site.json` | Les projets, leurs familles (Moteur, Outils, Kits, Jeux), leurs versions, les couches du moteur |
| `site/<langue>/<projet>/` | Les pages écrites, en Markdown simple : `apprendre/`, `manuel/`, `guides/` |
| `exemples/<Couche>/<Module>/<nom>.cpp` | Les exemples : de vrais programmes, avec `<nom>.attendu` (leur sortie exacte) |
| `exemples/NkDocExemples.jenga` | Un projet console par exemple (inclus seulement avec `NK_DOC_EXEMPLES=1`) |
| `traductions/en/<Module>.json` | L'anglais des descriptions de la référence (clé : symbole et empreinte du français) |
| `theme/` | La feuille de style, le script, les polices (Poppins, licence SIL Open Font License 1.1) et les logos de Rihen, tous en local. Avengenz n'y est pas : sa version gratuite exige une licence web commerciale |
| `outils/` | Le générateur (`generer_site.py`) et ses deux bancs (`banc_site.py`, `banc_generateur.py`) |
| `publication/` | Le flux GitHub Actions (`docs.yml`, inactif ici) et le fichier `CNAME` |

## Construire en local (depuis la racine du dépôt)

```bash
NK_DOC_EXEMPLES=1 jenga build --target NkDocEx_NKPlatform_ordre_des_octets --config Debug
jenga docs graphe --output build-docs/graphe.json
jenga docs api --project NKPlatform --etiquette v0.1.0-beta.11 --output build-docs/api/0.1
jenga docs api --project NKPlatform --output build-docs/api/dev     # la version de travail (main)
jenga docs api --project NKPlatform --output build-docs/api/main    # la référence de la garde des exemples
jenga docs exemples --config Debug --output build-docs/exemples.json
py -3 -I Docs/outils/generer_site.py --construction build-docs --sortie build-docs/site
py -3 -I Docs/outils/banc_site.py --site build-docs/site --construction build-docs --contre-epreuve
py -3 -I Docs/outils/banc_generateur.py --construction build-docs
```

## Les règles

- Un bloc de code C++ dans une page vient d'un fichier de `exemples/` (compilé et exécuté), sauf s'il est marqué
  `cpp fragment` : il s'affiche alors « fragment, non compilé ».
- Un nom cité par `[[Nom]]` doit exister dans la référence de la version ; un lien `@/chemin` doit mener à une page.
- Un exemple n'emploie que l'API présente à l'étiquette de la version où il s'affiche.
- Au tutoiement en français, sans tiret long ; l'anglais ne montre jamais une traduction périmée.
- Rien de privé n'entre : le générateur ne lit que les chemins de ce tableau, et `banc_site.py` fouille la sortie.
