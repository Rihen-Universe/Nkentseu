# -*- coding: utf-8 -*-
"""Classe les 747 markdown de main. Regles explicites, chacune motivee."""
import csv, io, os, re, collections

INV = 'cartographie_markdown/inventaire.csv'
OUT = 'cartographie_markdown/carte_consolidation.csv'

rows = list(csv.DictReader(io.open(INV, encoding='utf-8')))
rows = [r for r in rows if not r['fichier'].startswith('cartographie_markdown/')]

# ---- overrides a la main (fichiers relus un par un) -------------------------
OVERRIDE = {
 'CONSOLIDATION_TODO.md': ('supprimer', '', 'agent qui reprend',
   'non - le contenu a atterri (NKCollision present, SetFinalColorTarget utilise)',
   'consigne perimee au niveau le plus lu du depot : elle dit "NE PAS faire un merge brut" alors que la fusion est faite'),
 'README.md': ('garder', '', 'visiteur, etudiant, recruteur', 'oui - point d entree unique',
   'cible; a completer : ignore les trois applications (voir divergences 1.4)'),
 'ARCHITECTURE.md': ('garder', '', 'celui qui maintient', 'oui - les 5 couches et leurs regles',
   'cible; a completer : ignore les trois applications'),
 'ECOSYSTEM.md': ('fusionner', 'ARCHITECTURE.md', 'visiteur', 'non - redit ARCHITECTURE.md',
   'trois documents de presentation racontent la meme chose a trois dates'),
 'ECOSYSTEM_OVERVIEW.md': ('fusionner', 'README.md', 'visiteur', 'non', 'doublon de presentation'),
 'EXPLICATION_SIMPLE.md': ('garder', '', 'non-technicien, famille, presse', 'oui - le seul registre grand public',
   'garder BIEN QUE redondant : son public n est pas celui du README'),
 'PRESENTATION_TECHNIQUE.md': ('fusionner', 'README.md', 'partenaire, salon', 'non',
   'se recouvre avec README + ARCHITECTURE'),
 'promote.md': ('supprimer', '', 'personne', 'non - 24 lignes, evenement de juin 2026 passe',
   'note de salon perimee'),
 'DETTE_LISIBILITE.md': ('garder', '', 'celui qui maintient', 'oui - liste de dette nommee', 'a verifier avec Rodolf'),
 'CONVENTIONS_CODE.md': ('garder', '', 'tout contributeur', 'oui', 'contrat de style'),
 'CONVENTIONS_FICHIERS.md': ('garder', '', 'tout contributeur', 'oui', 'contrat de nommage'),
 'THIRD_PARTY_LICENSES.md': ('garder', '', 'juridique', 'oui - obligation legale', 'ne se touche pas'),
 'IMPORT_EXPORT_VISION.md': ('fusionner', 'ARCHITECTURE.md', 'celui qui decide', 'partiellement', 'vision transverse isolee a la racine'),
 'XR_MISSION_IA.md': ('fusionner', 'Kernel/Runtime/NKXR/ROADMAP.md', 'agent', 'partiellement - cadrage de mission', 'brief de mission a la racine'),
 'NKREF_MISSION_IA.md': ('fusionner', 'Applications/NkRef/ROADMAP.md', 'agent', 'partiellement', 'brief de mission a la racine'),
 'LUMIERE_OMBRE_MISSION_IA.md': ('fusionner', 'Kernel/Runtime/NKRenderer/ROADMAP.md', 'agent', 'partiellement', 'brief de mission a la racine'),
}


# --- overrides docs/ (racine du depot) : 32 fichiers hors API generee -------
_D = 'zero lien entrant depuis un markdown hors docs/ (mesure)'
OVERRIDE.update({
 'docs/dd2.md': ('supprimer','','personne','non - "ARCHITECTURE COMPLETE v1.0, Fevrier 2026"',
   'supplante par ARCHITECTURE.md (juin) et wiki/Architecture.md ; ' + _D),
 'docs/dd3.md': ('supprimer','','personne','non - "CONCEPTION DETAILLEE v2.0"',
   'deuxieme version du meme document ; ' + _D),
 'docs/dd4.md': ('supprimer','','personne','non - troisieme version du meme document', _D),
 'docs/README.md': ('supprimer','','personne','non - vieille copie du README racine (badge "80% Complete")',
   'deux README concurrents ; celui de la racine est a jour (2026-08-12), celui-ci date du 2026-03-05'),
 'docs/ARCHITECTURE.md': ('supprimer','','personne','non','supplante par ARCHITECTURE.md a la racine ; ' + _D),
 'docs/COMPLETION_STATUS.md': ('supprimer','','personne','non - statut fige au 2026-03-05',
   'rapport de session ; un statut sans date de peremption devient un mensonge'),
 'docs/FINAL_INTEGRATION_REPORT_2026.md': ('supprimer','','personne','non - rapport de session mars', _D),
 'docs/SESSION_FINAL_REPORT_2026.md': ('supprimer','','personne','non - rapport de session mars',
   'porte aussi le handle GitHub perime (l. 572) ; ' + _D),
 'docs/NKMATH_SESSION_COMPLETE_2026.md': ('supprimer','','personne','non - rapport de session mars', _D),
 'docs/NKENTSEU_PRODUCTION_ANALYSIS_2026.md': ('fusionner','Kernel/*/ROADMAP.md concernes','personne aujourd hui',
   'a relire - peut contenir des mesures de perf de mars','447 lignes non relues par moi : a trier avant suppression'),
 'docs/OPTIMIZATION_GUIDE_2026.md': ('fusionner','ROADMAP du module concerne','a verifier',
   'a relire - guide de perf','non relu par moi'),
 'docs/OPTIMIZATION_ROADMAP.md': ('fusionner','ROADMAP du module concerne','a verifier','a relire','non relu par moi'),
 'docs/NKMemory_Production_Guide.md': ('fusionner','Kernel/Foundation/NKMemory/Readme.md','utilisateur de NKMemory',
   'a relire','NKMemory/Readme.md fait deja 2 919 lignes et couvre le sujet'),
 'docs/NKMemory_Validation_Course.md': ('fusionner','Documentation/cours/','etudiant','a relire - support de cours','serie pedagogique orpheline'),
 'docs/NKMemory_Validation_Report.md': ('fusionner','Kernel/Foundation/NKMemory/Readme.md','personne',
   'OUI si le rapport porte des chiffres de validation','a relire AVANT toute suppression'),
 'docs/NKWindow_OpenGL_GLAD_Quickstart.md': ('fusionner','Kernel/System/NKWindow/ + Guides/','debutant','a relire','tutoriel orphelin de mars'),
 'docs/NKWindow_Renderer_Student_Procedure.md': ('fusionner','Documentation/cours/','etudiant','a relire','procedure etudiante orpheline'),
 'docs/NKSurface_Backend_Initialization_Guide.md': ('fusionner','Kernel/Runtime/NKRHI/ROADMAP.md','agent du RHI','a relire','guide technique orphelin'),
 'docs/NKRenderer_Simple_Context_Init.md': ('fusionner','Kernel/Runtime/NKRenderer/ROADMAP.md','agent du renderer','a relire','68 lignes orphelines'),
 'docs/PLATFORM_ISSUES_ANDROID_WEB_WAYLAND_2026.md': ('fusionner','Kernel/Foundation/NKPlatform/Readme.md','agent portage',
   'OUI probablement - problemes de plateforme constates','a relire AVANT suppression : ce type de fichier est du vecu'),
 'docs/WAYLAND_TEST_DEBUG_GUIDE_FR.md': ('fusionner','Kernel/System/NKWindow/','agent Linux',
   'OUI probablement - procedure de debogage Wayland','a relire AVANT suppression'),
 'docs/Audit-Portabilite-Apple.md': ('fusionner','Kernel/Foundation/NKPlatform/Readme.md','agent portage Apple',
   'OUI - audit date du 2026-07-04','le mot AUDIT signale une mesure : a fusionner, jamais supprimer'),
 'docs/TEST_FRAMEWORK_README.md': ('fusionner','le README du cadre de test','agent qui ecrit un test','a relire','370 lignes orphelines'),
 'docs/LANGUES_LOCALES_CAMEROUN.md': ('garder','','celui qui decide / NKFont / Ilyana',
   'OUI - travail de documentation linguistique non reconstructible','sujet unique dans tout le depot'),
 'docs/IDEES_ARCHITECTURE.md': ('fusionner','ARCHITECTURE.md','celui qui decide','oui - idees datees du 2026-08-12','recent : a lire avant de fusionner'),
 'docs/SOURCES_TIERCES.md': ('fusionner','THIRD_PARTY_LICENSES.md','juridique','oui - inventaire de sources','doublon partiel du fichier racine'),
 'docs/ssh.md': ('fusionner','CONVENTIONS_FICHIERS.md ou un GUIDE_CONTRIBUTION','contributeur qui installe',
   'oui - procedure de cles SSH','porte aussi le handle GitHub a confirmer (l. 118)'),
})

# --- Tutoriels3D : quatrieme serie pedagogique -----------------------------
for _i in ('README.md','01-Fenetre/README.md','02-Renderer/README.md','03-Scene/README.md',
           '04-Camera/README.md','05-Meshes/README.md'):
    OVERRIDE['Tutoriels3D/'+_i] = ('garder','','debutant 3D',
      'oui - chaque README documente un projet jenga compilable a cote de lui',
      'colocalise avec du code qui tourne ; mais c est la 4e serie pedagogique du depot (voir rapport)')

def classe(f, octets, lignes, date):
    base = os.path.basename(f)
    low = base.lower()

    if f in OVERRIDE:
        a, c, l, i, m = OVERRIDE[f]
        return a, c, l, i, m

    # --- API generee ---------------------------------------------------------
    if f.startswith('docs/') and '/markdown/' in f:
        mod = f.split('/')[1]
        return ('supprimer', '', 'personne',
                'non - listing d API produit par outil',
                'API generee de %s, figee 2026-03, ZERO lien entrant mesure, module deja couvert par wiki/' % mod)

    # --- wiki ----------------------------------------------------------------
    if f.startswith('wiki/'):
        return ('garder', '', 'wiki publie - lecteur externe',
                'oui pour les pages de module (prose ecrite a la main)',
                'reference API tenue a la main; 180/187 figees depuis juin 2026 - a dater, pas a supprimer')

    # --- juridique -----------------------------------------------------------
    if f.startswith('Externals/'):
        return ('garder', '', 'juridique / audit de licence', 'oui - obligation legale',
                'NOTICE de dependance tierce, ne se consolide pas')

    # --- cours ---------------------------------------------------------------
    if f.startswith('Documentation/cours'):
        return ('garder', '', 'etudiants (rentree septembre 2026)',
                'oui - se lit comme un livre',
                'hors du modele README/SPECIFICATION/ROADMAP : autre animal, autre rythme')
    if f.startswith('Documentation/notes_'):
        return ('fusionner', 'Documentation/cours/ ou le ROADMAP du module concerne',
                'auteur du cours', 'oui - releves d integration, souvent chiffres',
                'notes de travail de %d lignes a la racine de Documentation/' % int(lignes))
    if f.startswith('Documentation/'):
        return ('garder', '', 'etudiants', 'a verifier', 'reste de Documentation/')

    # --- guides --------------------------------------------------------------
    if f.startswith('Guides/'):
        return ('fusionner', 'Documentation/cours/',
                'debutant qui utilise le moteur', 'partiellement - code compilable',
                'meme public et 6 sujets sur 9 communs avec Documentation/cours/md/')

    # --- bug reports ---------------------------------------------------------
    if f.startswith('BugReports/'):
        if base in ('README.md', 'PROMPT_WIKI.md') or base.startswith('SESSION_'):
            return ('fusionner', 'BugReports/README.md', 'agent qui debogue', 'non',
                    'meta-fichier du dossier')
        return ('garder', '', 'agent qui debogue le meme backend',
                'OUI - symptome + cause + lecon, introuvable ailleurs',
                'un rapport de bug est la trace d un jour perdu : le supprimer le fait repayer')

    # --- fichiers vides ------------------------------------------------------
    if int(octets) < 200:
        return ('supprimer', '', 'personne',
                'non - %s octets' % octets,
                'fichier vide ou quasi vide : repond "deja fait" a qui cherche')

    # --- Kernel / Engine / Spark : modules -----------------------------------
    if f.split('/')[0] in ('Kernel', 'Engine', 'Spark', 'Resources'):
        mod = '/'.join(f.split('/')[:3]) if f.startswith('Kernel/') else os.path.dirname(f)
        if low in ('readme.md',):
            return ('garder', '', 'celui qui utilise le module', 'oui - point d entree',
                    'cible README')
        if low == 'roadmap.md':
            return ('garder', '', 'celui qui decide', 'OUI - etat reel, mesures, decisions datees',
                    'cible ROADMAP; porte aussi le POURQUOI faute de SPECIFICATION - a scinder')
        if low in ('specification.md', 'specification_visuelle.md'):
            return ('garder', '', 'celui qui maintient', 'oui', 'cible SPECIFICATION')
        return ('fusionner', mod + '/ROADMAP.md ou SPECIFICATION.md a creer',
                'agent du module', 'a verifier au cas par cas',
                'quatrieme fichier du module, hors des trois cibles')

    # --- Applications --------------------------------------------------------
    if f.startswith('Applications/'):
        app = 'Applications/' + f.split('/')[2] if len(f.split('/')) > 2 else f
        app = 'Applications/' + f.split('/')[1]
        if low == 'readme.md' and f.count('/') == 2:
            return ('garder', '', 'celui qui utilise l application', 'oui', 'cible README')
        if low == 'roadmap.md' and f.count('/') == 2:
            return ('garder', '', 'celui qui decide', 'OUI - etat reel et decisions datees', 'cible ROADMAP')
        if low.startswith('specification') or low == 'specification.md':
            return ('garder', '', 'celui qui maintient', 'oui - dit le POURQUOI', 'cible SPECIFICATION')
        if '/src/' in f:
            return ('fusionner', app + '/SPECIFICATION.md a creer', 'agent du sous-systeme',
                    'partiellement - decrit un dossier de code', 'README enfoui dans src/')
        if low.startswith('passation') or 'gdd' in low or low.startswith('lisezmoi'):
            return ('garder', '', 'agent qui reprend / testeur', 'OUI - etat exact a une date',
                    'passation, GDD ou notice de temoin : contenu non reconstructible')
        return ('fusionner', app + '/SPECIFICATION.md', 'agent de l application',
                'a verifier au cas par cas', 'quatrieme fichier de l application')

    # --- docs/ hors API generee ---------------------------------------------
    if f.startswith('docs/'):
        if f.startswith('docs/recherche/'):
            return ('garder', '', 'celui qui decide', 'oui - sources bibliographiques',
                    'notes de recherche sourcees')
        return ('fusionner', 'le ROADMAP du module concerne ou Documentation/',
                'a verifier - probablement personne', 'a verifier au cas par cas',
                'rapport de session ou guide isole a la racine de docs/, fige depuis mars 2026')

    return ('fusionner', 'a decider', 'a verifier', 'a verifier', 'non classe par les regles')

out = []
for r in rows:
    a, c, l, i, m = classe(r['fichier'], r['octets'], r['lignes'], r['derniere_modif'])
    out.append([r['fichier'], a, c, l, i, r['derniere_modif'], r['lignes'], m])

w = csv.writer(io.open(OUT, 'w', newline='', encoding='utf-8'))
w.writerow(['fichier', 'action', 'cible_de_fusion', 'qui_le_lit',
            'ce_qu_il_contient_d_irremplacable', 'derniere_modif', 'lignes', 'motif'])
for o in out:
    w.writerow(o)

c = collections.Counter(o[1] for o in out)
print('total classe :', len(out))
for k, v in c.most_common():
    print('  %-12s %d' % (k, v))
irr = [o for o in out if o[4].startswith('OUI')]
print('irremplacable (OUI) :', len(irr))
print('non classe :', sum(1 for o in out if o[7].startswith('non classe')))
