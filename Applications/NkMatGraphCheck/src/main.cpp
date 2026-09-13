// =============================================================================
// NkMatGraphCheck — banc CONSOLE du graphe de materiaux (couche 3 sur NKGraph).
//
// POURQUOI UNE APPLICATION ET PAS UN `tests/` : mesure le 2026-08-21,
// `jenga test --project X` rend « Unit-test execution is disabled by workspace
// policy ». Les bancs sous `tests/` COMPILENT et ne s'executent JAMAIS. Un banc
// qui doit prouver quelque chose est donc une application console a part
// entiere. Meme raison que NKEditMeshHarness et NkSLCheck.
//
// AUCUN GPU, AUCUNE FENETRE. Un graphe est une structure de donnees ; le NkSL
// qu'il emettra se verifie par le vrai NkSLCompiler, qui n'a pas besoin de
// device (c'est ce que fait deja NkSLCheck). Le banc reste donc rapide et
// utilisable partout.
//
// ═════════════════════════════════════════════════════════════════════════════
// ⚠️⚠️ CE QUE CE BANC NE PROUVE PAS — A LIRE AVANT SON COMPTE DE CAS ⚠️⚠️
//
//   (b1) EST PROUVE JUSQU'AU NkSL EMIS, ET PAS AU-DELA. AUCUNE IMAGE N'A ETE
//   RENDUE, AUCUNE VALEUR RELUE DEPUIS LA CARTE. MANQUENT : LA CIBLE
//   R16G16B16A16_FLOAT COTE RHI, SON EFFACEMENT A (0,0,0,0), ET UNE LECTURE
//   REELLE.
//
// Sans cette phrase, « 116 cas, 0 echec, 26 mutations sur 26 » se lit comme
// « la seconde cible fonctionne ». Elle ne fonctionne pas : ELLE EST
// CORRECTEMENT DECRITE. C'est beaucoup, ce n'est pas la meme chose.
//
// La meme reserve vaut pour tout ce banc : il mesure des STRUCTURES DE DONNEES
// et du TEXTE DE SHADER. Les cas « rendu/… » qui prouvent des pixels vivent
// dans NkMatGraphDemo, qui a besoin d'un GPU.
// ═════════════════════════════════════════════════════════════════════════════
//
// CE QUE CHAQUE CAS DOIT ETRE : un cas qui DISCRIMINE. Il ne confirme pas que ca
// marche, il echouerait si l'implantation etait fausse D'UNE FACON PRECISE, et
// cette facon est ecrite a cote du cas. Un cas qui ne peut pas surprendre n'est
// pas une mesure.
//
// ═════════════════════════════════════════════════════════════════════════════
// REGLE 1 — LES TROIS FACONS DONT UN CAS JUSTE NE MESURE RIEN
//
// Mesurees ici en trois jours, sur trois mutations qui ont SURVECU a des cas
// dont l'assertion etait pourtant exacte. Elles ne se cherchent pas de la meme
// facon, et c'est pour ca qu'il faut les trois :
//
//   IL MANQUAIT LA MATIERE.   « le defaut de prise n'est pas recopie » est passee
//     verte parce que le graphe d'essai ne portait AUCUN defaut de prise. Un cas
//     qui ne porte pas la propriete ne peut pas la juger.
//     -> Demande-toi : mon graphe d'essai contient-il ce que je pretends
//        surveiller ?
//
//   IL MANQUAIT LA RELATION.  « le sens des prises n'est plus inverse » est
//     passee verte parce que le cas comptait « une entree et une sortie » -- vrai
//     AUSSI quand les deux sens sont inverses. Le compte est symetrique, le
//     cablage non. LE NOMBRE ETAIT EXACTEMENT JUSTE : le compteur ne donne aucun
//     indice. Un cas qui compte ne juge pas ce qui relie.
//     -> Demande-toi : ce nombre serait-il le meme si tout etait permute ?
//
//   IL MANQUAIT LA PROFONDEUR. « la recursion ne regarde que le voisin immediat »
//     est passee verte parce que le graphe d'essai n'avait que DEUX maillons --
//     et a deux maillons, le voisin immediat suffit. Un cas a deux maillons ne
//     juge pas ce qui est recursif.
//     -> Demande-toi : mon exemple est-il assez profond pour que la recursion
//        serve ?
//
// REGLE 2 — LIRE LE COMPTE D'ERREURS DE COMPILATION AVANT LE RESULTAT DU BANC
//
// ⚠️ Rencontre TROIS FOIS. Quand la construction echoue, l'ancien binaire est
// toujours la, et il s'execute. Le banc affiche alors un resultat complet,
// coherent et VERT -- sur du code qui n'existe plus. Rien dans sa sortie ne
// signale que ce n'est pas le code qu'on vient d'ecrire.
//
// Ce n'est pas une habitude a prendre, c'est une REGLE : toute mesure commence
// par le nombre d'erreurs de compilation. « 0 echec » sur une construction ratee
// ne mesure rien du tout, et c'est la seule sortie du banc qui ment sans qu'on
// puisse le voir.
//
// Meme famille, par l'autre bout (Q38 s3) : un harnais de mutation qui restaure
// le fichier SANS reconstruire laisse le binaire MUTE en place, et la course
// suivante rend ROUGE sur une source saine.
//
// REGLE 3 — « RIEN » ET « ZERO » SONT DEUX ETATS, ET RIEN NE LES DISTINGUE TOUT
//           SEUL
//
// Rencontre DEUX FOIS le meme jour, a deux etages differents, sous la meme
// forme :
//   - dans le tampon : un materiau qui ne porte pas la sortie y laisse des zeros,
//     indiscernables d'une valeur qui vaut zero -> le canal A ;
//   - dans la structure C++ : `NkMatSortieMateriau::valeur[3]` reste a zero pour
//     une sortie (b1), qui n'a AUCUNE valeur processeur -> le drapeau
//     `valeurConnue`.
//
// ⚠️ La seconde a ete ecrite EN CORRIGEANT LA PREMIERE. Que la meme faute change
// d'etage sans changer de forme est ce qui la rend generale : ce n'est pas un
// defaut du tampon, c'est L'ABSENCE DE DISTINCTION ENTRE « RIEN » ET « ZERO »,
// partout ou elle n'est pas EXPLICITEMENT PORTEE. Le zero est toujours
// disponible, toujours plausible, et ne se signale jamais.
//   -> Demande-toi : ce zero veut-il dire « la valeur est zero » ou « il n'y a
//      pas de valeur » ? Si les deux sont possibles, il manque un porteur.
// (Le coeur le fait deja pour les valeurs de graphe : « jamais renseigne » a UNE
// SEULE representation, `type == NK_TYPE_INVALID`. C'est la meme regle.)
//
// REGLE 4 — UNE CONDITION IMPLICITEMENT VRAIE PARCE QU'IL N'EXISTE QU'UN SEUL
//           CAS DEVIENT FAUSSE QUAND LE SECOND ARRIVE, ET ELLE NE PREVIENT PAS
//
// Le refus « ta sortie depend du pixel » et l'evaluation processeur
// s'appliquaient a TOUS les etages de sortie. C'etait correct tant qu'un seul
// etage etait implemente. Le jour ou (b1) est arrive -- par pixel PAR
// DEFINITION -- la condition est devenue fausse sans qu'une ligne change.
//
// 📌 ET LE DETAIL QUI LA REND RECONNAISSABLE : le message disait DEJA « est
// declaree par_materiau ». Le code ne le verifiait pas. C'est la huitieme fois
// dans ce chantier que le NOM et l'USAGE divergent -- et l'une des rares ou le
// nom etait JUSTE : la prose portait l'intention exacte, le code ne la portait
// pas.
//   -> Demande-toi, en ajoutant un second cas a ce qui n'en avait qu'un : quelles
//      conditions etaient vraies parce qu'il n'y en avait qu'un ?
//
// REGLE 5 — LA CONNAISSANCE EXISTE, MAIS PAS LA OU QUELQU'UN LA CHERCHERAIT
//
// Trois formes, sorties la meme nuit sur trois chantiers, et elles ont la meme
// racine :
//
//   une limite SUE et non ecrite       -> se transmet en s'effacant ;
//   une limite ECRITE AU MAUVAIS ENDROIT -> ne se transmet pas du tout ;
//   deux documents qui DIVERGENT       -> fabriquent du faux travail
//                                         (deux seances de decision preparees
//                                          sur des points deja tranches
//                                          ailleurs).
//
// ⚠️ ET LA DEUXIEME EST LA PLUS COUTEUSE DES TROIS, mesuree ici meme.
// `NkGraph/NkNodeGraph.h` porte, DEPUIS LE DEBUT, exactement la regle 3 :
// « JAMAIS RENSEIGNE et RENSEIGNE A VIDE sont deux etats differents », avec
// « jamais renseigne » ramene a UNE SEULE representation
// (`type == NK_TYPE_INVALID`). Sa note cite meme le piege PAYE par l'agent
// NkUIDesign dans la nuit du 21 au 22/08.
//
// Quelqu'un l'avait deja paye. Il l'avait ECRIT. Et je l'ai repaye quand meme,
// deux etages plus haut, en laissant `valeur[3]` a zero sur une sortie qui n'a
// pas de valeur. Pas par negligence : PARCE QUE RIEN NE POUSSE A LIRE L'EN-TETE
// DU MODULE D'EN DESSOUS QUAND ON ECRIT UNE STRUCTURE AU-DESSUS. La regle etait
// disponible, gratuite, et invisible.
//
//   -> Demande-toi : cette regle existe-t-elle deja quelque part SOUS MOI ?
//
// ⚠️ Cette question N'A PAS DE REPONSE MECANIQUE, et il ne faut pas faire
// semblant du contraire. On ne peut pas relire tout le noyau avant chaque
// structure, et aucun outil ne dira « la regle que tu t'appretes a violer est
// ecrite trois modules plus bas ». La poser vaut quand meme mieux que de ne pas
// la poser : elle coute trente secondes sur les questions ou l'on SAIT qu'un
// module d'en dessous a deja tranche -- la representation d'une valeur absente,
// la stabilite d'un identifiant, l'ordre d'evaluation -- et ce sont justement
// celles ou la reponse existe.
// ═════════════════════════════════════════════════════════════════════════════
// ═════════════════════════════════════════════════════════════════════════════
// REGLE 6 — UNE DEFENSE REDONDANTE EST INVISIBLE A UNE MUTATION A UN SEUL DEFAUT
//
// Elle ne se mesure qu'en COUPLE, en faisant d'abord tomber la ligne qui est
// devant elle.
//
// ⚠️ CE QU'ELLE COUTE QUAND ON NE L'A PAS : UN CORRECTIF PEUT ETRE ENTIEREMENT
// INUTILE SANS QU'AUCUNE MUTATION NE LE DISE, parce qu'une autre defense le
// couvre. On le croit acquis, on l'ecrit dans un commentaire, et le commentaire
// devient la seule preuve qu'il en existe une.
//
// LA MESURE QUI L'A SORTIE (2026-08-23, rang 4, second filet du site d'emission
// de `UV Map` / `Attribute`) :
//
//   M11  le filet remet le repli plausible « vUV »        -> A SURVECU
//   M12  la premiere passe de refus disparait             -> attrapee, rien emis=1
//   M13  M11 + M12                                        -> attrapee, rien emis=0
//   M14  M12 + le filet d'AVANT le correctif (jeton)      -> attrapee, rien emis=0
//
// M11 survit -- et AUCUN correctif du site ne peut la faire rougir : la passe 1
// le couvre par construction. Le reflexe « un defaut par mutation » est bon, et
// il a un angle mort exactement de la taille d'une seconde ligne de defense.
//
// C'est le COUPLE M12 / M14 qui mesure le correctif : meme mutation de la passe
// 1, seul le filet change. Refus -> « rien emis=1 ». Jeton -> « rien emis=0 ».
// Le correctif s'ACHETE au lieu de se croire.
//
//   -> Demande-toi : ce que je corrige est-il ATTEIGNABLE ? Si une autre garde
//      le couvre, la mutation qui le vise sera verte quoi que je fasse. Alors
//      mute LES DEUX, et compare les deux mutations entre elles -- pas au vert.
//
// ⚠️ ET LA REGLE A UN SECOND TRANCHANT, RENCONTRE LE MEME JOUR SUR CE MEME
// FILET : quand la mesure en couple existe enfin, elle peut MESURER MOINS QUE
// CE QU'ON CROIT. Le banc distinguait « a-t-on emis ? » et rien d'autre : M13
// (repli plausible) et M14 (jeton imprononcable) lui etaient INDISCERNABLES,
// alors que c'est precisement la difference qui compte pour l'auteur. Il a
// fallu un cas de plus -- `rang4/emis-credible-contre-emis-qui-echoue` -- pour
// la voir, et ce cas a rendu un resultat que personne n'attendait : un repli
// plausible ne produit pas une source PROCHE de la legitime, il produit
// EXACTEMENT LA MEME. Il n'y a rien a comparer.
// ═════════════════════════════════════════════════════════════════════════════
// =============================================================================
#include "NKRenderer/Materials/Graph/NkMatGraphTypes.h"
#include "NKRenderer/Materials/Graph/NkMatGraphCompile.h"
// Les numeros de binding du set materiau. En-tete SANS DEPENDANCE, fait pour
// etre lisible par un banc qui ne lie ni NKRenderer ni NKRHI.
#include "NKRenderer/Materials/NkMaterialBindings.h"
// Le VRAI compilateur NkSL : c est lui qui dit si le shader traverse les
// quatre backends, et il n a besoin d aucun device (cf. NkSLCheck).
#include "NKSL/Compiler/NkSLCompiler.h"
// Le DOCUMENT multi-graphes : il porte deja definition + instances, donc la
// mecanique des GROUPES. Le banc s en sert pour MESURER ou vit le refus de
// recursion -- a l insertion, ou seulement a l aplatissement.
#include "NKGraph/NkGraphDocument.h"
// REGROUPER / DEGROUPER : l operation demandee en R9, et son critere
// d acceptation -- grouper puis degrouper rend le graphe identique.
#include "NKGraph/NkGraphGroup.h"
#include <stdio.h> // fwrite : ecrire la source SANS passer par le formateur

// ── POURQUOI CE BANC N'IMPRIME PLUS AVEC printf ─────────────────────────────
// Rodolf, 2026-08-22 (et c'est la deuxieme fois) : « ne pas utiliser directement
// printf, le systeme definit des loggers, pourquoi ne pas les utiliser ? »
//
// ⚠️ ET PASSER A `Infof` N'AURAIT RIEN CORRIGE. `Infof` prend une chaine de
// format et des arguments variadiques NON TYPES — la meme mecanique que printf.
// Le defaut mesure la meme nuit par l'agent NK3DModeler — un printf reclamant
// sept `%u` pour six arguments, le septieme lisant la pile et affichant
// « nonmanif=18 » sur un cube parfaitement manifold — se reproduit a l'identique
// avec `Infof`. Rien ne plante, rien n'avertit, ET LE NOMBRE EST CREDIBLE.
//
// Seule la forme POSITIONNELLE change la nature du probleme : `NkFormat("{0}")`
// capture les arguments PAR LEUR TYPE, jamais reinterpretes. S'il en manque un,
// on obtient un TROU VISIBLE, pas un entier plausible. Pour un banc dont toute
// la valeur tient dans les nombres qu'il imprime, c'est la seule propriete qui
// compte : un trou se voit ; un « 18 » plausible se recopie dans un rapport,
// puis dans une ROADMAP, puis dans une decision.
//
//   `-Werror=format` (pose dans le .jenga) DETECTE le desalignement.
//   La forme `{0}` le rend IRREPRESENTABLE. On garde les deux : le drapeau est
//   un filet pour ce qui resterait, la forme est la solution.
//
// Le motif de journal est « %v » : le message SEUL, sans horodatage ni fichier.
// La sortie d'un banc EST son resultat ; une decoration la rendrait illisible et
// instable d'une execution a l'autre.
//
// ⚠️ Et les chaines IMPRIMEES sont en ASCII : le puits console ne transporte pas
// l'UTF-8 (mesure — « — » sortait « - », les guillemets sortaient « ? »). Mieux
// vaut ecrire ce qui sera lu que laisser des caracteres se perdre en chemin. Les
// commentaires, eux, gardent leur typographie : ils ne sont jamais imprimes.
#include "NKContainers/String/NkFormat.h"
#include "NKLogger/NkLog.h"
#include <stdlib.h>
#include <string.h> // strlen : du C, pas de la STL

using namespace nkentseu;
using namespace nkentseu::graph;
using namespace nkentseu::renderer::matgraph;

// LE REGISTRE DE PROTOTYPES DE CE PROCESSUS DE BANC.
//
// Depuis le 2026-08-23 le registre est un PARAMETRE : il n'existe plus
// d'instance de processus. Ce `gReg` n'est donc PAS un retour du defaut --
// c'est le registre d'un document unique, celui que ce banc manipule. Les
// cas qui ont besoin de DEUX documents en declarent deux, localement, et
// c'est precisement ce que le temoin d'isolement mesure.
static NkMatRegistreProtos gReg;

static uint32 gCas = 0;
static uint32 gEchecs = 0;

static void Cas(const char *nom, bool ok, const NkString &detail) {
	++gCas;
	if (!ok)
		++gEchecs;
	logger.Info("{0:<34} {1} | {2}", NkString(nom), NkString(ok ? "OK  " : "ECHEC"), detail);
}

// ── types/ ───────────────────────────────────────────────────────────────────

static void CasTypesEnregistrement() {
	// DISCRIMINE : une implantation qui enregistrerait deux fois le meme nom, ou
	// qui rendrait 0 (l'identifiant reserve « invalide »), passerait un test qui
	// se contenterait de compter les appels. On verifie donc que les quatre
	// identifiants sont NON NULS et DEUX A DEUX DISTINCTS.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const bool nonNuls = t.Valid();
	const bool distincts = t.real != t.vector && t.real != t.color && t.real != t.shader &&
						   t.vector != t.color && t.vector != t.shader && t.color != t.shader;
	// Et ils doivent se RETROUVER par leur nom : un enregistrement qui ne
	// remplirait pas la table de noms laisserait FindType a zero, et tout
	// NkMatAddNode ulterieur echouerait sans qu'on sache pourquoi.
	const bool retrouves = g.FindType(NK_MT_REAL) == t.real && g.FindType(NK_MT_SHADER) == t.shader;
	NkString d;
	d = NkFormat("reel={0} vect={1} coul={2} shader={3} | distincts={4} retrouves-par-nom={5}", t.real, t.vector, t.color, t.shader, distincts ? 1 : 0, retrouves ? 1 : 0);
	Cas("types/enregistrement", nonNuls && distincts && retrouves, d);
}

static void CasConversionDirigee() {
	// LE CAS QUI TRANCHE, et il se teste DANS LES DEUX SENS : une table de
	// conversions symetrique par erreur passerait un test a sens unique.
	//   reel -> couleur   doit etre ACCEPTE (niveau de gris)
	//   couleur -> reel   doit etre REFUSE (luminance ? moyenne ? canal rouge ?
	//                     trois reponses plausibles, donc aucune par defaut)
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const bool reelVersCouleur = g.Accepts(t.color, t.real);
	const bool couleurVersReel = g.Accepts(t.real, t.color);
	// Et le shader ne se convertit avec RIEN : c'est ce qui empeche de brancher
	// une couleur la ou le moteur attend une surface ombree.
	const bool couleurVersShader = g.Accepts(t.shader, t.color);
	const bool shaderVersCouleur = g.Accepts(t.color, t.shader);
	NkString d;
	d = NkFormat("reel>couleur={0} couleur>reel={1} | couleur>shader={2} shader>couleur={3}", reelVersCouleur ? "ok" : "REFUSE", couleurVersReel ? "ACCEPTE" : "refuse", couleurVersShader ? "ACCEPTE" : "refuse", shaderVersCouleur ? "ACCEPTE" : "refuse");
	Cas("types/conversion-dirigee", reelVersCouleur && !couleurVersReel && !couleurVersShader && !shaderVersCouleur, d);
}

// ── bibliotheque/ ────────────────────────────────────────────────────────────

static void CasInstancierPrincipled() {
	// DISCRIMINE : on ne compte pas les prises, on les cherche PAR NOM ET PAR
	// SENS. Une implantation qui ajouterait les bonnes prises dans le mauvais
	// sens (bsdf en entree) donnerait exactement le meme compte.
	NkNodeGraph g;
	NkMatRegisterTypes(g);
	const NkNodeId n = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	const NkNode *nd = g.Find(n);
	const int32 baseColor = nd ? nd->FindSocket("base_color", NkSocketDir::Input) : -1;
	const int32 bsdfOut = nd ? nd->FindSocket("bsdf", NkSocketDir::Output) : -1;
	// Le piege du sens : « bsdf » NE DOIT PAS exister en entree.
	const int32 bsdfIn = nd ? nd->FindSocket("bsdf", NkSocketDir::Input) : 0;
	// Le piege du type : base_color doit porter le type COULEUR, pas reel. Une
	// declaration decalee d'une ligne donnerait le bon nom et le mauvais type,
	// et le graphe accepterait alors des liens qu'il devrait refuser.
	const bool bonType = nd && baseColor >= 0 && nd->sockets[(usize)baseColor].type == g.FindType(NK_MT_COLOR);
	NkString d;
	d = NkFormat("noeud={0} prises={1} base_color={2}(type-ok={3}) bsdf-sortie={4} bsdf-entree={5}(-1 attendu)", n, nd ? (uint32)nd->sockets.Size() : 0u, baseColor, bonType ? 1 : 0, bsdfOut, bsdfIn);
	Cas("biblio/instancier-principled", n != NK_NODE_INVALID && baseColor >= 0 && bsdfOut >= 0 && bsdfIn < 0 && bonType,
		d);
}

static void CasPrototypeInconnu() {
	// DISCRIMINE : un refus qui laisserait quand meme un noeud derriere lui
	// donnerait le meme code de retour. On verifie donc AUSSI que le graphe est
	// reste VIDE — c'est le meme piege que « cycle-refuse » dans NKGraph.
	NkNodeGraph g;
	NkMatRegisterTypes(g);
	const NkNodeId n = NkMatAddNode(gReg, g, "mat.noeud_qui_nexiste_pas");
	NkString d;
	d = NkFormat("rendu={0} (0 attendu) noeuds-dans-le-graphe={1} (0 attendu)", n, g.NodeCount());
	Cas("biblio/prototype-inconnu", n == NK_NODE_INVALID && g.NodeCount() == 0, d);
}

static void CasTypesNonEnregistres() {
	// LE CAS QU'ON N'ECRIT PAS SPONTANEMENT : instancier AVANT d'avoir
	// enregistre les types. Une implantation « serviable » qui enregistrerait
	// les types au vol reussirait ici — et priverait le graphe de ses
	// CONVERSIONS, si bien qu'il refuserait plus tard des liens parfaitement
	// bons, tres loin de la cause. Le refus doit etre net.
	NkNodeGraph g; // volontairement SANS NkMatRegisterTypes
	const NkNodeId n = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	NkString d;
	d = NkFormat("rendu={0} (0 attendu) noeuds={1} (0 attendu)", n, g.NodeCount());
	Cas("biblio/types-non-enregistres", n == NK_NODE_INVALID && g.NodeCount() == 0, d);
}

// ── graphe/ ──────────────────────────────────────────────────────────────────

static void CasPrincipledVersSortie() {
	// Le graphe minimal REEL. DISCRIMINE par l'ORDRE topologique : le Principled
	// doit venir AVANT la sortie. On cree la sortie EN PREMIER pour qu'un tri
	// qui renverrait l'ordre d'insertion echoue.
	NkNodeGraph g;
	NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	const NkLinkError e = g.Connect(bsdf, "bsdf", out, "surface");
	NkVector<NkNodeId> ordre;
	const bool trie = g.TopoSort(ordre);
	const bool bonOrdre = trie && ordre.Size() == 2 && ordre[0] == bsdf && ordre[1] == out;
	NkNodeId trouve = NK_NODE_INVALID;
	const NkMatGraphError v = NkMatValidate(gReg, g, &trouve);
	NkString d;
	d = NkFormat("lien={0} ordre={1} (bsdf avant sortie) validation={2} sortie-trouvee={3}", NkLinkErrorName(e), bonOrdre ? "bsdf>sortie" : "MAUVAIS", NkMatGraphErrorName(v), trouve == out ? 1 : 0);
	Cas("graphe/principled-vers-sortie", e == NkLinkError::Ok && bonOrdre && v == NkMatGraphError::Ok && trouve == out,
		d);
}

static void CasCouleurDansShaderRefuse() {
	// LE CAS QUI PROTEGE L'AUTEUR. Brancher une couleur dans l'entree « surface »
	// est l'erreur la plus courante dans l'editeur de Blender. Elle doit etre
	// refusee AVEC SA RAISON, et le lien ne doit PAS avoir ete pose — un refus
	// qui laisserait le lien rendrait le meme code d'erreur.
	NkNodeGraph g;
	NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId emis = NkMatAddNode(gReg, g, NK_MN_EMISSION);
	// « color » est une ENTREE couleur d'Emission : on tente une sortie->entree
	// invalide ET un type invalide, en deux cas distincts.
	const NkLinkError typeFaux = g.Connect(emis, "emission", out, "surface"); // valide, temoin positif
	const NkLinkError sensFaux = g.Connect(out, "surface", emis, "color");	 // entree -> entree
	const uint32 liens = g.LinkCount();
	NkString d;
	d = NkFormat("temoin-positif(emission>surface)={0} | entree>entree={1} | liens={2} (1 attendu)", NkLinkErrorName(typeFaux), NkLinkErrorName(sensFaux), liens);
	Cas("graphe/sens-et-types-refuses",
		typeFaux == NkLinkError::Ok && sensFaux == NkLinkError::DirectionMismatch && liens == 1, d);
}

static void CasDeuxSortiesRefusees() {
	// VALIDATION DE DOMAINE, pas du coeur. Le coeur ne sait pas ce qu'est une
	// « sortie materiau » et ne peut pas refuser la seconde — c'est a moi.
	// DISCRIMINE : le graphe reste PARFAITEMENT VALIDE pour le coeur (aucun
	// cycle, tous les types bons). Seule une passe qui connait le domaine voit
	// le probleme.
	NkNodeGraph g;
	NkMatRegisterTypes(g);
	const NkNodeId out1 = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	g.Connect(bsdf, "bsdf", out1, "surface");
	const NkMatGraphError avant = NkMatValidate(gReg, g);
	const NkNodeId out2 = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkMatGraphError apres = NkMatValidate(gReg, g);
	NkVector<NkNodeId> ordre;
	const bool coeurContent = g.TopoSort(ordre); // le coeur, lui, ne voit rien
	// Et le retrait de la seconde sortie doit RENDRE le graphe valide : une
	// validation qui compterait les noeuds MORTS resterait bloquee sur
	// « plusieurs sorties » apres la suppression.
	g.RemoveNode(out2);
	const NkMatGraphError apresRetrait = NkMatValidate(gReg, g);
	NkString d;
	d = NkFormat("1 sortie={0} | 2 sorties={1} (coeur triable={2}, il ne voit rien) | apres retrait={3}", NkMatGraphErrorName(avant), NkMatGraphErrorName(apres), coeurContent ? 1 : 0, NkMatGraphErrorName(apresRetrait));
	Cas("graphe/deux-sorties-refusees",
		avant == NkMatGraphError::Ok && apres == NkMatGraphError::MultipleOutput && coeurContent &&
			apresRetrait == NkMatGraphError::Ok,
		d);
}

static void CasSortieNonReliee() {
	// DISCRIMINE : une validation qui se contenterait de compter les sorties
	// declarerait ce graphe bon. Il ne l'est pas — rien n'entre dans la sortie,
	// et le compilateur n'aurait rien a compiler.
	//
	// ⚠️ CE QUE CE CAS NE PROUVE **PAS**, et je l'ai appris en le mutant. Le
	// piege de la sentinelle est reel — l'index du socket « surface » vaut 0,
	// donc un controle ecrit `if (!index)` au lieu de `if (index < 0)` est faux.
	// Mais sous cette mutation, CE cas-ci **passe encore**, pour la mauvaise
	// raison : le controle casse rend « sortie-non-reliee » a tout le monde, y
	// compris a lui. Ce sont les cas a graphe VALIDE (principled-vers-sortie,
	// melange-deux-bsdf, aller-retour) qui attrapent la mutation, en se mettant
	// au rouge. On l'ecrit ici parce qu'un commentaire qui promet une
	// discrimination qu'il n'a pas est pire qu'un cas absent.
	NkNodeGraph g;
	NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	NkMatAddNode(gReg, g, NK_MN_PRINCIPLED); // present mais NON RELIE
	const NkMatGraphError v = NkMatValidate(gReg, g);
	const NkNode *nd = g.Find(out);
	const int32 idx = nd ? nd->FindSocket("surface", NkSocketDir::Input) : -1;
	NkString d;
	d = NkFormat("validation={0} | index du socket surface={1} (0 : la sentinelle a zero serait un BUG)", NkMatGraphErrorName(v), idx);
	Cas("graphe/sortie-non-reliee", v == NkMatGraphError::OutputUnlinked && idx == 0, d);
}

static void CasAucuneSortie() {
	// Le temoin de l'autre bord : sans lui, une validation qui refuserait TOUT
	// passerait les cas precedents sans rien prouver.
	NkNodeGraph g;
	NkMatRegisterTypes(g);
	NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	const NkMatGraphError v = NkMatValidate(gReg, g);
	NkString d;
	d = NkFormat("validation={0}", NkMatGraphErrorName(v));
	Cas("graphe/aucune-sortie", v == NkMatGraphError::NoOutput, d);
}

static void CasMelangeShader() {
	// Le « Mix Shader » de Rodolf, celui par lequel tout a commence. On melange
	// DEUX BSDF vers une seule sortie. DISCRIMINE par l'ordre topologique a
	// QUATRE noeuds : le melangeur doit venir apres SES DEUX sources et avant la
	// sortie. Les noeuds sont crees dans l'ordre INVERSE de l'ordre attendu.
	NkNodeGraph g;
	NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId mix = NkMatAddNode(gReg, g, NK_MN_MIX_SHADER);
	const NkNodeId diff = NkMatAddNode(gReg, g, NK_MN_DIFFUSE);
	const NkNodeId emis = NkMatAddNode(gReg, g, NK_MN_EMISSION);
	const NkLinkError e1 = g.Connect(diff, "bsdf", mix, "shader1");
	const NkLinkError e2 = g.Connect(emis, "emission", mix, "shader2");
	const NkLinkError e3 = g.Connect(mix, "shader", out, "surface");
	NkVector<NkNodeId> o;
	const bool trie = g.TopoSort(o);
	int32 pMix = -1, pOut = -1, pDiff = -1, pEmis = -1;
	for (usize i = 0; i < o.Size(); ++i) {
		if (o[i] == mix)
			pMix = (int32)i;
		else if (o[i] == out)
			pOut = (int32)i;
		else if (o[i] == diff)
			pDiff = (int32)i;
		else if (o[i] == emis)
			pEmis = (int32)i;
	}
	const bool bonOrdre = trie && pDiff < pMix && pEmis < pMix && pMix < pOut;
	const NkMatGraphError v = NkMatValidate(gReg, g);
	NkString d;
	d = NkFormat("liens={0}/{1}/{2} | positions diff={3} emis={4} mix={5} sortie={6} | validation={7}", NkLinkErrorName(e1), NkLinkErrorName(e2), NkLinkErrorName(e3), pDiff, pEmis, pMix, pOut, NkMatGraphErrorName(v));
	Cas("graphe/melange-deux-bsdf",
		e1 == NkLinkError::Ok && e2 == NkLinkError::Ok && e3 == NkLinkError::Ok && bonOrdre &&
			v == NkMatGraphError::Ok,
		d);
}

static void CasAllerRetourFichier() {
	// Le graphe est une DONNEE : il doit survivre a l'aller-retour. DISCRIMINE
	// en comparant les TEXTES caractere pour caractere, pas des comptes — des
	// libelles ou des conversions perdus laisseraient les comptes intacts. Et on
	// verifie que la SEMANTIQUE survit : le graphe relu doit encore refuser ce
	// qu'il refusait (preuve que `conv` a ete relu, pas seulement reecrit).
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	g.Connect(bsdf, "bsdf", out, "surface");
	NkString a;
	g.Serialize(a);

	NkNodeGraph g2;
	const bool lu = g2.Deserialize(a.CStr());
	NkString b;
	g2.Serialize(b);
	const bool identique = (a == b);
	const NkMatGraphError v = NkMatValidate(gReg, g2);
	// La semantique : reel->couleur accepte, couleur->reel refuse, APRES relecture.
	const NkTypeId r2 = g2.FindType(NK_MT_REAL), c2 = g2.FindType(NK_MT_COLOR);
	const bool semantique = r2 && c2 && g2.Accepts(c2, r2) && !g2.Accepts(r2, c2);
	NkString d;
	d = NkFormat("lu={0} texte-identique={1} octets={2} validation={3} | semantique-survit={4} (types {5}/{6})", lu ? 1 : 0, identique ? 1 : 0, (uint32)a.Size(), NkMatGraphErrorName(v), semantique ? 1 : 0, r2, c2);
	Cas("graphe/aller-retour-fichier", lu && identique && v == NkMatGraphError::Ok && semantique, d);
	(void)t;
}

// ── INDEX CONTRE NOM : LA MESURE, PUIS LE FORMAT QU'ELLE A DECIDE ────────────
//
// LA QUESTION DU CHANTIER DESIGN, mot pour mot : « NkLink adresse les prises par
// indice alors que NkSocket::name est la cle stable -- faire gagner des prises a
// un noeud repointerait en silence les liens suivants. »
//
// Mesuree le 2026-08-23. La reponse etait en DEUX MOITIES qui ne disaient pas la
// meme chose : infondee en memoire, EXACTE dans le fichier. Le format est passe
// en version 2 le meme jour ; ce cas garde la premiere moitie et prouve que la
// seconde est refermee. Le temoin qui PAIE le changement de format est
// `fichier/ordre-des-sock`, plus bas.
static void CasIndexDePriseContreNom() {
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	g.Connect(bsdf, "bsdf", out, "surface");

	// ── MOITIE 1 : EN MEMOIRE, LA CRAINTE EST INFONDEE ───────────────────
	// `AddSocket` fait un `PushBack`, et il n'existe AUCUNE operation qui
	// retire ou insere une prise. Un index deja attribue ne peut donc pas
	// bouger. On le mesure au lieu de le lire : on gagne une prise, et on
	// exige que le lien designe encore LA MEME PRISE PAR SON NOM.
	//
	// ⚠️ CETTE MOITIE RESTE VRAIE APRES LE PASSAGE EN VERSION 2, et c'est
	// voulu : le nom est la cle SUR DISQUE, l'index reste la cle EN MEMOIRE.
	// Un graphe s'evalue a chaque image, un fichier se lit une fois.
	const NkLink *avant = g.LinkAt(0);
	const int32 idxAvant = avant ? avant->toSocket : -1;
	const bool ajoutee = g.AddSocket(out, "prise_gagnee", t.real, NkSocketDir::Input);
	const NkLink *apres = g.LinkAt(0);
	const NkNode *no = g.Find(out);
	const NkString *nomApres =
		(apres && no && apres->toSocket >= 0 && apres->toSocket < (int32)no->sockets.Size())
			? &no->sockets[(uint32)apres->toSocket].name
			: nullptr;
	const bool memoireTient = ajoutee && apres && apres->toSocket == idxAvant && nomApres &&
							  *nomApres == NkString("surface");

	// ── MOITIE 2 : DANS LE FICHIER, ELLE NE L'EST PLUS ───────────────────
	// 🔴 CE BLOC MESURAIT LE DEFAUT ; IL MESURE MAINTENANT SA FERMETURE, ET ON
	// GARDE LA MEME PERTURBATION EXPRES. Un cas reecrit avec une perturbation
	// plus douce aurait verdi sans rien prouver : c'est exactement l'insertion
	// qui repointait le 23/08 qu'il faut voir echouer.
	NkString texte;
	g.Serialize(texte);
	NkString entete("sock ");
	entete.Append(NkFormat("{0}", (uint32)out));
	entete.Append(" 0 ");
	const NkNode *nout = g.Find(out);
	const int32 iSurface = nout ? nout->FindSocket("surface", NkSocketDir::Input) : -1;
	entete.Append(NkFormat("{0}", (uint32)(iSurface >= 0 ? nout->sockets[(uint32)iSurface].type : t.shader)));
	entete.Append(" intruse");
	entete.Append('\n');

	NkString marque("sock ");
	marque.Append(NkFormat("{0}", (uint32)out));
	marque.Append(" ");
	NkString truque;
	{
		const char *p = texte.CStr();
		bool pose = false;
		while (p && *p) {
			const char *fin = p;
			while (*fin && *fin != '\n')
				++fin;
			NkString ligne;
			for (const char *q = p; q < fin; ++q)
				ligne.Append(*q);
			if (!pose && ligne.Size() >= marque.Size()) {
				bool debute = true;
				for (uint32 k = 0; k < (uint32)marque.Size(); ++k)
					if (ligne.CStr()[k] != marque.CStr()[k])
						debute = false;
				if (debute) {
					truque.Append(entete);
					pose = true;
				}
			}
			truque.Append(ligne);
			truque.Append('\n');
			p = (*fin == '\n') ? fin + 1 : fin;
		}
	}

	NkNodeGraph h;
	NkString err;
	const bool relu = h.Deserialize(truque.CStr(), &err);
	const NkLink *lh = h.LinkCount() > 0 ? h.LinkAt(0) : nullptr;
	const NkNode *nh = h.Find(out);
	const NkString *nomTruque = (lh && nh && lh->toSocket >= 0 && lh->toSocket < (int32)nh->sockets.Size())
									? &nh->sockets[(uint32)lh->toSocket].name
									: nullptr;
	const bool tientBon = relu && nomTruque && *nomTruque == NkString("surface");
	// ⚠️ ET LA PERTURBATION DOIT AVOIR EU LIEU, SINON CE VERT NE VAUT RIEN.
	//
	// 🔴 MESURE DU 2026-08-23, ET C EST LA MUTATION QUI L A DIT. Tant que le cas
	// exigeait « le lien REPOINTE », l'insertion se prouvait toute seule : sans
	// elle, rien ne repointait. Le jour ou la version 2 du format a inverse
	// l'assertion en « le lien RESTE », ce garde-fou implicite a disparu -- et
	// M15, qui neutralise l'insertion, EST PASSEE DE ROUGE A VERTE sans que le
	// cas bouge d'une ligne.
	//
	// C'est la premiere facon dont un cas juste ne mesure rien -- il manque la
	// MATIERE -- et elle est arrivee ici PAR UN CORRECTIF, pas par negligence :
	// une assertion inversee ne porte pas les memes preuves implicites que
	// celle qu'elle remplace. On exige donc que la prise intruse soit
	// REELLEMENT arrivee dans le graphe charge.
	const bool intruseArrivee = relu && nh && nh->FindSocket("intruse", NkSocketDir::Input) >= 0;

	NkString d;
	d = NkFormat("MEMOIRE : ajout de prise, le lien garde son index {0} et son nom « {1} »={2} (PushBack seul, "
				 "aucune operation ne retire ni n'insere) | FICHIER v{3} : une prise glissee avant les autres "
				 "laisse le lien sur « {4} »={5} | et la prise intruse est BIEN arrivee={6} (relu={7})",
				 idxAvant, nomApres ? *nomApres : NkString("?"), memoireTient ? 1 : 0,
				 (uint32)NK_NKGRAPH_VERSION, nomTruque ? *nomTruque : NkString("?"), tientBon ? 1 : 0,
				 intruseArrivee ? 1 : 0, relu ? 1 : 0);
	Cas("graphe/index-de-prise-contre-nom", memoireTient && tientBon && intruseArrivee, d);
}

// ── outils communs aux cas de valeurs ────────────────────────────────────────

// Cherche une ligne COMMENCANT par `prefixe` dans un texte `.nkgraph`. Sert a
// prouver une ABSENCE de ligne, ce qu'aucun compteur ne peut faire.
static bool ContientLigne(const NkString &texte, const char *prefixe) {
	const char *p = texte.CStr();
	while (p && *p) {
		const char *q = prefixe;
		const char *r = p;
		while (*q && *r == *q) {
			++r;
			++q;
		}
		if (*q == 0)
			return true;
		while (*p && *p != '\n')
			++p;
		if (*p == '\n')
			++p;
	}
	return false;
}

// Compte les diagnostics d'un genre donne et rend le premier detail rencontre.
static uint32 CompteDiag(const NkNodeGraph &g, NkGraphIssue genre, NkString *premierDetail = nullptr) {
	NkVector<NkGraphDiag> diags;
	g.Validate(diags);
	uint32 n = 0;
	for (usize i = 0; i < diags.Size(); ++i)
		if (diags[i].issue == genre) {
			if (n == 0 && premierDetail)
				*premierDetail = diags[i].detail;
			++n;
		}
	return n;
}

// ── valeurs : defauts de prise et proprietes ─────────────────────────────────

static void CasDefautDePrise() {
	// DISCRIMINE : on verifie les TROIS etats qu'une prise peut avoir, parce que
	// c'est leur confusion qui coute cher — prise inexistante (nullptr), prise
	// existante sans defaut (`IsSet()` faux), prise avec defaut. Une implantation
	// qui rendrait nullptr dans les deux premiers cas passerait un test qui ne
	// regarderait que le troisieme.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId n = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);

	const NkGraphValue *absente = g.SocketDefault(n, "prise_qui_nexiste_pas", NkSocketDir::Input);
	const NkGraphValue *vierge = g.SocketDefault(n, "roughness", NkSocketDir::Input);
	const bool viergeNonRenseignee = vierge && !vierge->IsSet();

	const bool pose = g.SetSocketDefault(n, "roughness", NkSocketDir::Input, NkValueReal(t.real, 0.5f));
	const NkGraphValue *lu = g.SocketDefault(n, "roughness", NkSocketDir::Input);
	const bool bonne = lu && lu->IsSet() && lu->type == t.real && lu->numbers.Size() == 1 && lu->numbers[0] == 0.5f;

	// Poser un defaut sur une prise absente doit ECHOUER, jamais en creer une.
	const bool refuse = !g.SetSocketDefault(n, "prise_qui_nexiste_pas", NkSocketDir::Input, NkValueReal(t.real, 1.f));
	NkString d;
	d = NkFormat("absente={0} | vierge non-renseignee={1} | pose={2} relu-exact={3} | pose sur absente refusee={4}", absente ? "POINTEUR (nullptr attendu)" : "nullptr", viergeNonRenseignee ? 1 : 0, pose ? 1 : 0, bonne ? 1 : 0, refuse ? 1 : 0);
	Cas("valeur/defaut-de-prise", !absente && viergeNonRenseignee && pose && bonne && refuse, d);
}

static void CasJamaisRenseigneContreVide() {
	// LE PIEGE PAYE PAR L'AGENT NkUIDesign, transpose ici. « Jamais renseigne »
	// et « renseigne a vide » doivent rester DEUX etats distincts a travers
	// l'aller-retour de fichier. Un ecrivain qui emettrait quand meme sa ligne
	// pour une valeur absente ecrirait un type 0 ; la relecture rendrait alors
	// une valeur qui SEMBLE renseignee, et sans la moindre erreur.
	//
	// PREMIERE VERSION DE CE CAS : ELLE NE PROUVAIT RIEN, et c'est une mutation
	// qui me l'a appris. Elle prenait un noeud SANS aucune propriete et sans
	// aucun defaut, puis verifiait qu'aucune ligne « def » ou « prop » n'etait
	// ecrite. Une boucle d'ecriture qui n'itere sur RIEN n'ecrit rien quoi qu'on
	// fasse de son garde : la mutation « emets la ligne meme si la valeur n'est
	// pas renseignee » passait au vert. On exerce donc maintenant les DEUX
	// portes avec de vraies iterations : une propriete POSEE MAIS VIDE, et six
	// prises qui existent toutes sans defaut.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId n = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);

	// PORTE 1 — les prises. Le Principled en a six, aucune n'a de defaut : la
	// boucle d'ecriture les parcourt toutes, et son garde est donc exerce six
	// fois. Aucune ligne « def » ne doit sortir.
	// PORTE 2 — une propriete POSEE mais dont la valeur n'a jamais ete
	// renseignee. C'est un etat legitime : l'editeur cree la ligne, l'auteur n'a
	// pas encore saisi. La boucle des proprietes itere donc une fois, et son
	// garde est exerce lui aussi.
	NkGraphValue jamaisRenseignee; // type == NK_TYPE_INVALID
	g.SetProp(n, "en_attente_de_saisie", jamaisRenseignee);
	const uint32 posees = g.PropCount(n);

	NkString texte;
	g.Serialize(texte);
	const bool aucunDef = !ContientLigne(texte, "def ");
	const bool aucunProp = !ContientLigne(texte, "prop ");

	// Et apres l'aller-retour, la propriete non renseignee doit etre ABSENTE —
	// pas revenue « renseignee a vide ». C'est la difference exacte que le
	// defaut de NkUIDesign effacait.
	NkNodeGraph g2;
	g2.Deserialize(texte.CStr());
	const bool absenteApres = g2.FindProp(n, "en_attente_de_saisie") == nullptr;

	// Le TEMOIN DE L'AUTRE BORD : une valeur RENSEIGNEE A VIDE (type valide,
	// zero reel, texte vide) doit, elle, s'ecrire et survivre. Sans lui, un
	// ecrivain qui n'ecrirait JAMAIS rien passerait tout ce qui precede.
	NkGraphValue videMaisPosee;
	videMaisPosee.type = t.real;
	g.SetProp(n, "marqueur", videMaisPosee);
	NkString avec;
	g.Serialize(avec);
	NkNodeGraph g3;
	g3.Deserialize(avec.CStr());
	const NkGraphValue *relu = g3.FindProp(n, "marqueur");
	const bool survit = relu && relu->IsSet() && relu->numbers.Size() == 0 && relu->text.Size() == 0;
	const bool uneSeuleLigne = ContientLigne(avec, "prop ");

	NkString d;
	d = NkFormat("proprietes posees={0} | aucune ligne def={1} aucune ligne prop={2} | absente apres relecture={3} | temoin vide-mais-posee : ligne ecrite={4} survit={5}", posees, aucunDef ? 1 : 0, aucunProp ? 1 : 0, absenteApres ? 1 : 0, uneSeuleLigne ? 1 : 0, survit ? 1 : 0);
	Cas("valeur/jamais-renseigne-vs-vide",
		posees == 1 && aucunDef && aucunProp && absenteApres && uneSeuleLigne && survit, d);
}

static void CasProprieteDeNoeud() {
	// DISCRIMINE : poser deux fois la meme cle doit REMPLACER, pas empiler. Deux
	// proprietes homonymes rendraient la lecture dependante de l'ordre
	// d'insertion, et la premiere lecture donnerait l'ANCIENNE valeur.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId n = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	g.SetProp(n, "operation", NkValueText(t.real, "multiplier"));
	const uint32 apres1 = g.PropCount(n);
	g.SetProp(n, "operation", NkValueText(t.real, "ajouter"));
	const uint32 apres2 = g.PropCount(n);
	const NkGraphValue *v = g.FindProp(n, "operation");
	const bool remplacee = v && v->text == NkString("ajouter");

	// Une propriete inconnue rend nullptr, jamais une valeur par defaut
	// silencieuse : « absente » et « posee a zero » ne se corrigent pas pareil.
	const bool inconnueNulle = g.FindProp(n, "jamais_posee") == nullptr;

	g.SetProp(n, "seconde", NkValueReal(t.real, 2.f));
	const bool retiree = g.RemoveProp(n, "operation");
	const bool retraitInconnu = !g.RemoveProp(n, "jamais_posee");
	const NkGraphValue *reste = g.FindProp(n, "seconde");
	const bool resteIntacte = reste && reste->numbers.Size() == 1 && reste->numbers[0] == 2.f;
	NkString d;
	d = NkFormat("1 pose={0} | 2 poses meme cle={1} (1 attendu) remplacee={2} | inconnue nullptr={3} | retiree={4} retrait-inconnu-refuse={5} voisine-intacte={6}", apres1, apres2, remplacee ? 1 : 0, inconnueNulle ? 1 : 0, retiree ? 1 : 0, retraitInconnu ? 1 : 0, resteIntacte ? 1 : 0);
	Cas("valeur/propriete-de-noeud",
		apres1 == 1 && apres2 == 1 && remplacee && inconnueNulle && retiree && retraitInconnu && resteIntacte, d);
}

static void CasAllerRetourConstruitEnMemoire() {
	// ⚠️ LE CRITERE D'ACCEPTATION, et il est precis : le document doit etre
	// CONSTRUIT EN MEMOIRE, pas seulement relu. Un ecrivain qui reemettrait une
	// forme litterale memorisee A LA LECTURE passerait un test relu -> reecrit,
	// et ecrirait du vide pour un document qui n'a jamais ete lu.
	//
	// DISCRIMINE : textes compares caractere pour caractere, ET chaque valeur
	// revenue EGALE AU BIT — des comptes justes laisseraient passer une valeur
	// perdue en route.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	g.Connect(bsdf, "bsdf", out, "surface");
	const float32 blanc[4] = {1.f, 1.f, 1.f, 1.f};
	g.SetSocketDefault(bsdf, "base_color", NkSocketDir::Input, NkValueVec(t.color, blanc, 4));
	g.SetSocketDefault(bsdf, "roughness", NkSocketDir::Input, NkValueReal(t.real, 0.5f));
	g.SetSocketDefault(bsdf, "metallic", NkSocketDir::Input, NkValueReal(t.real, 0.f));
	g.SetProp(bsdf, "distribution", NkValueText(t.real, "GGX multi diffusion"));

	NkString a;
	g.Serialize(a);
	NkNodeGraph g2;
	const bool lu = g2.Deserialize(a.CStr());
	NkString b;
	g2.Serialize(b);

	const NkGraphValue *c1 = g.SocketDefault(bsdf, "base_color", NkSocketDir::Input);
	const NkGraphValue *c2 = g2.SocketDefault(bsdf, "base_color", NkSocketDir::Input);
	const NkGraphValue *p1 = g.FindProp(bsdf, "distribution");
	const NkGraphValue *p2 = g2.FindProp(bsdf, "distribution");
	const bool valeursEgales = c1 && c2 && c1->Equals(*c2) && p1 && p2 && p1->Equals(*p2);
	// Un texte a ESPACES est ce qui casse en premier quand on lit un jeton au
	// lieu du reste de la ligne.
	const bool texteAEspaces = p2 && p2->text == NkString("GGX multi diffusion");
	NkString d;
	d = NkFormat("lu={0} texte-identique={1} octets={2} | valeurs egales au bit={3} | texte a espaces preserve={4}", lu ? 1 : 0, (a == b) ? 1 : 0, (uint32)a.Size(), valeursEgales ? 1 : 0, texteAEspaces ? 1 : 0);
	Cas("valeur/aller-retour-construit-en-memoire", lu && (a == b) && valeursEgales && texteAEspaces, d);
}

static void CasPrecisionExacte() {
	// DISCRIMINE : un tiers. Le format ecrit les positions en « %.6f », qui rend
	// 0.333333 — un AUTRE flottant. Si les valeurs avaient herite de ce
	// formateur, l'egalite exacte echouerait sur un aller-retour pourtant
	// correct, et personne ne saurait dire si le defaut est dans la valeur ou
	// dans l'ecriture.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId n = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	const float32 tiers = 1.f / 3.f;
	g.SetSocketDefault(n, "roughness", NkSocketDir::Input, NkValueReal(t.real, tiers));
	NkString s;
	g.Serialize(s);
	NkNodeGraph g2;
	g2.Deserialize(s.CStr());
	const NkGraphValue *v = g2.SocketDefault(n, "roughness", NkSocketDir::Input);
	const bool exact = v && v->IsSet() && v->numbers.Size() == 1 && v->numbers[0] == tiers;
	NkString d;
	d = NkFormat("1/3 ecrit puis relu | egal au bit={0} ({1:.9g} vs {2:.9g})", exact ? 1 : 0, (double)tiers, (v && v->numbers.Size() == 1) ? (double)v->numbers[0] : 0.0);
	Cas("valeur/precision-traverse-le-texte", exact, d);
}

// ── validation : LE CHEMIN QUI N'ETAIT PAS GARDE ─────────────────────────────
// Tous les cas qui suivent entrent par `Deserialize`, jamais par l'API. C'est
// tout leur objet : `Connect()` refusait deja ces etats, `Deserialize` les
// laissait passer sans un mot. Les fichiers sont ecrits A LA MAIN, parce qu'un
// fichier produit par `Serialize` ne peut pas etre malade.

static const char *kFichierSain = "nkgraph 1\n"
								  "compteurs 3 2\n"
								  "type 1 mat.reel\n"
								  "type 2 mat.couleur\n"
								  "conv 1 2\n"
								  "noeud 1 0.000000 0.000000 source Source\n"
								  "sock 1 1 2 sortie\n"
								  "noeud 2 0.000000 0.000000 puits Puits\n"
								  "sock 2 0 2 entree\n"
								  "lien 1 1 0 2 0\n";

static void CasFichierSainZeroDiagnostic() {
	// LE TEMOIN DE L'AUTRE BORD. Sans lui, une validation qui crierait sur TOUT
	// passerait les huit cas suivants sans rien prouver.
	NkNodeGraph g;
	const bool lu = g.Deserialize(kFichierSain);
	NkVector<NkGraphDiag> diags;
	const uint32 n = g.Validate(diags);
	NkString d;
	d = NkFormat("lu={0} diagnostics={1} (0 attendu) noeuds={2} liens={3}", lu ? 1 : 0, n, g.NodeCount(), g.LinkCount());
	Cas("validation/fichier-sain-zero-diag", lu && n == 0 && g.NodeCount() == 2 && g.LinkCount() == 1, d);
}

static void CasFichierCycle() {
	// ⚠️ LE CAS QUI RESUMAIT LE DEFAUT. `Connect()` refuse un cycle depuis
	// toujours ; le FICHIER en posait un sans que rien ne le dise, et l'echec ne
	// sortait qu'a l'evaluation, plus tard et ailleurs.
	//
	// DISCRIMINE : on verifie AUSSI que le graphe s'est bien CHARGE avec ses deux
	// liens. Un `Deserialize` qui « reparerait » en refusant le second lien
	// rendrait zero diagnostic — et perdrait silencieusement une donnee de
	// l'utilisateur. L'invalide doit etre REPRESENTABLE **et** DETECTE.
	static const char *kCycle = "nkgraph 1\n"
								"compteurs 3 3\n"
								"type 1 mat.reel\n"
								"noeud 1 0 0 a A\n"
								"sock 1 1 1 out\n"
								"sock 1 0 1 in\n"
								"noeud 2 0 0 b B\n"
								"sock 2 1 1 out\n"
								"sock 2 0 1 in\n"
								"lien 1 1 0 2 1\n"
								"lien 2 2 0 1 1\n";
	NkNodeGraph g;
	const bool lu = g.Deserialize(kCycle);
	const uint32 cycles = CompteDiag(g, NkGraphIssue::Cycle);
	NkString d;
	d = NkFormat("lu={0} liens charges={1} (2 attendus : l invalide doit etre REPRESENTABLE) | diag cycle={2}", lu ? 1 : 0, g.LinkCount(), cycles);
	Cas("validation/cycle-par-le-fichier", lu && g.LinkCount() == 2 && cycles == 1, d);
}

static void CasFichierTypeIncompatible() {
	// Le fichier branche une sortie « couleur » sur une entree « reel ». La
	// conversion declaree va dans l'AUTRE sens (reel -> couleur), donc le lien
	// est faux. DISCRIMINE la direction de la table de conversions : une table
	// symetrique par erreur ne dirait rien ici.
	static const char *kMauvaisType = "nkgraph 1\n"
									  "compteurs 3 2\n"
									  "type 1 mat.reel\n"
									  "type 2 mat.couleur\n"
									  "conv 1 2\n"
									  "noeud 1 0 0 source Source\n"
									  "sock 1 1 2 sortie\n"
									  "noeud 2 0 0 puits Puits\n"
									  "sock 2 0 1 entree\n"
									  "lien 1 1 0 2 0\n";
	NkNodeGraph g;
	g.Deserialize(kMauvaisType);
	NkString quel;
	const uint32 n = CompteDiag(g, NkGraphIssue::LinkTypeMismatch, &quel);
	NkString d;
	d = NkFormat("diag type-incompatible={0} sur la prise '{1}' | liens charges={2}", n, quel.CStr(), g.LinkCount());
	Cas("validation/type-incompatible-par-fichier", n == 1 && quel == NkString("entree") && g.LinkCount() == 1, d);
}

static void CasFichierSensInverse() {
	// Le fichier branche une ENTREE sur une ENTREE. `Connect` distingue ce cas
	// depuis toujours ; le fichier ne le voyait pas.
	static const char *kSens = "nkgraph 1\n"
							   "compteurs 3 2\n"
							   "type 1 mat.reel\n"
							   "noeud 1 0 0 a A\n"
							   "sock 1 0 1 entree\n"
							   "noeud 2 0 0 b B\n"
							   "sock 2 0 1 entree\n"
							   "lien 1 1 0 2 0\n";
	NkNodeGraph g;
	g.Deserialize(kSens);
	const uint32 n = CompteDiag(g, NkGraphIssue::LinkDirection);
	NkString d;
	d = NkFormat("diag sens-invalide={0} | liens charges={1}", n, g.LinkCount());
	Cas("validation/sens-inverse-par-fichier", n == 1 && g.LinkCount() == 1, d);
}

static void CasFichierNoeudEtSocketAbsents() {
	// Deux malformations differentes, et elles ne se corrigent pas pareil : un
	// lien vers un noeud QUI N'EXISTE PAS, et un lien vers un INDEX DE PRISE hors
	// bornes sur un noeud qui existe. DISCRIMINE : un controle qui confondrait
	// les deux rendrait deux fois le meme code, et l'utilisateur chercherait le
	// mauvais probleme.
	static const char *kAbsents = "nkgraph 1\n"
								  "compteurs 3 3\n"
								  "type 1 mat.reel\n"
								  "noeud 1 0 0 a A\n"
								  "sock 1 1 1 out\n"
								  "noeud 2 0 0 b B\n"
								  "sock 2 0 1 in\n"
								  "lien 1 1 0 99 0\n"
								  "lien 2 1 0 2 7\n";
	NkNodeGraph g;
	g.Deserialize(kAbsents);
	const uint32 noeud = CompteDiag(g, NkGraphIssue::LinkUnknownNode);
	const uint32 borne = CompteDiag(g, NkGraphIssue::LinkSocketOutOfRange);
	NkString d;
	d = NkFormat("diag noeud-inconnu={0} | diag socket-hors-bornes={1} (deux codes DISTINCTS)", noeud, borne);
	Cas("validation/noeud-et-socket-absents", noeud == 1 && borne == 1, d);
}

static void CasFichierEntreeDoublee() {
	// « Une entree n'accepte qu'UNE source » est garanti par `Connect`, qui
	// remplace. Un fichier peut en poser deux. DISCRIMINE : le graphe reste
	// triable et sans cycle — seul ce controle-ci voit le probleme, et sans lui
	// l'evaluation prendrait celui des deux liens que l'ordre d'iteration
	// rencontre en premier.
	static const char *kDoublee = "nkgraph 1\n"
								  "compteurs 4 3\n"
								  "type 1 mat.reel\n"
								  "noeud 1 0 0 a A\n"
								  "sock 1 1 1 out\n"
								  "noeud 2 0 0 b B\n"
								  "sock 2 1 1 out\n"
								  "noeud 3 0 0 c C\n"
								  "sock 3 0 1 in\n"
								  "lien 1 1 0 3 0\n"
								  "lien 2 2 0 3 0\n";
	NkNodeGraph g;
	g.Deserialize(kDoublee);
	const uint32 n = CompteDiag(g, NkGraphIssue::LinkDuplicateTarget);
	NkVector<NkNodeId> ordre;
	const bool triable = g.TopoSort(ordre);
	NkString d;
	d = NkFormat("diag entree-doublee={0} | liens={1} | triable={2} (le tri ne voit RIEN)", n, g.LinkCount(), triable ? 1 : 0);
	Cas("validation/entree-doublee-par-fichier", n == 1 && g.LinkCount() == 2 && triable, d);
}

static void CasFichierTypesInconnus() {
	// ⚠️ CE QUE LES VALEURS AGGRAVENT, et c'est pour ca que la validation et les
	// proprietes sont arrivees ensemble : un `typeId` absent du registre entre
	// par le meme chemin non garde — sur une prise, sur un defaut, sur une
	// propriete. Trois portes, trois codes distincts.
	static const char *kInconnus = "nkgraph 1\n"
								   "compteurs 2 1\n"
								   "type 1 mat.reel\n"
								   "noeud 1 0 0 a A\n"
								   "sock 1 0 9 prise_type_fantome\n"
								   "sock 1 0 1 prise_saine\n"
								   "def 1 1 9 1 0.5\n"
								   "prop 1 cle_fantome 42 1 1\n";
	NkNodeGraph g;
	g.Deserialize(kInconnus);
	NkString quelleprise, quelledefaut, quelleprop;
	const uint32 sock = CompteDiag(g, NkGraphIssue::SocketUnknownType, &quelleprise);
	const uint32 def = CompteDiag(g, NkGraphIssue::DefaultTypeMismatch, &quelledefaut);
	const uint32 prop = CompteDiag(g, NkGraphIssue::PropUnknownType, &quelleprop);
	NkString d;
	d = NkFormat("prise type-inconnu={0} ({1}) | defaut type-different={2} ({3}) | propriete type-inconnu={4} ({5})", sock, quelleprise.CStr(), def, quelledefaut.CStr(), prop, quelleprop.CStr());
	Cas("validation/types-inconnus-par-fichier",
		sock == 1 && quelleprise == NkString("prise_type_fantome") && def == 1 &&
			quelledefaut == NkString("prise_saine") && prop == 1 && quelleprop == NkString("cle_fantome"),
		d);
}

static void CasDefautHorsBornesNonRabattu() {
	// ⚠️ LE CAS LE PLUS IMPORTANT DE LA SERIE, et le moins spontane a ecrire.
	// Un `def` designe une prise par son INDEX. Que faire d'un index hors
	// bornes ? La tentation est de le RABATTRE sur la derniere prise, ou sur
	// zero. Ce serait poser la valeur sur une prise QUI N'EST PAS LA SIENNE : le
	// graphe paraitrait sain et calculerait autre chose.
	//
	// DISCRIMINE : on verifie que la prise 0 est restee NON RENSEIGNEE. Un
	// rabattement rendrait un graphe sans le moindre diagnostic — donc un test
	// qui ne compterait que les diagnostics le declarerait bon.
	static const char *kHorsBornes = "nkgraph 1\n"
									 "compteurs 2 1\n"
									 "type 1 mat.reel\n"
									 "noeud 1 0 0 a A\n"
									 "sock 1 0 1 premiere\n"
									 "sock 1 0 1 seconde\n"
									 "def 1 7 1 1 0.5\n";
	NkNodeGraph g;
	g.Deserialize(kHorsBornes);
	const NkGraphValue *p0 = g.SocketDefault(1, "premiere", NkSocketDir::Input);
	const NkGraphValue *p1 = g.SocketDefault(1, "seconde", NkSocketDir::Input);
	const bool aucunRabattement = p0 && !p0->IsSet() && p1 && !p1->IsSet();
	NkVector<NkGraphDiag> diags;
	const uint32 n = g.Validate(diags);
	NkString d;
	d = NkFormat("premiere renseignee={0} seconde renseignee={1} (0/0 attendu : AUCUN rabattement) | diagnostics={2}", (p0 && p0->IsSet()) ? 1 : 0, (p1 && p1->IsSet()) ? 1 : 0, n);
	Cas("validation/defaut-hors-bornes-non-rabattu", aucunRabattement && n == 0, d);
}

static void CasCompteRenduBorne() {
	// Un compte de reels annonce par le FICHIER. Un lecteur qui lui ferait
	// confiance allouerait avant que la validation n'ait la parole — et un
	// fichier corrompu annoncant quatre milliards de reels tuerait le processus
	// AVANT tout diagnostic. La borne est donc a la LECTURE, pas apres.
	static const char *kEnorme = "nkgraph 1\n"
								 "compteurs 2 1\n"
								 "type 1 mat.reel\n"
								 "noeud 1 0 0 a A\n"
								 "sock 1 0 1 prise\n"
								 "def 1 0 1 4000000000 0.5\n";
	NkNodeGraph g;
	const bool lu = g.Deserialize(kEnorme);
	const NkGraphValue *v = g.SocketDefault(1, "prise", NkSocketDir::Input);
	const uint32 taille = v ? (uint32)v->numbers.Size() : 0u;
	NkString d;
	d = NkFormat("lu={0} sans mourir | reels retenus={1} (borne a 4096, pas 4 milliards)", lu ? 1 : 0, taille);
	Cas("validation/compte-de-reels-borne", lu && taille <= 4096u, d);
}

// ── phase 0 : le masque par TEXTURE ──────────────────────────────────────────
// Ces cas ne testent pas une structure de donnees : ils tiennent ensemble DEUX
// fichiers que rien d'autre ne force a s'accorder — le C++ qui construit le set
// de descripteurs, et le shader qui lit ce set.
//
// ⚠️ POURQUOI ILS EXISTENT, ET C'EST UNE MESURE, PAS UNE PRECAUTION. Le
// 2026-08-22, pour verifier que le temoin Demo4..Demo8 saurait attraper une
// erreur de binding, j'ai ecrit VOLONTAIREMENT le descripteur du masque sur le
// binding 10, absent du layout. Resultat : aucune erreur, aucun avertissement,
// aucune ligne de journal, et les CINQ signatures identiques. Le moteur ecrit
// sans broncher un descripteur que son propre layout ne declare pas ; la texture
// n'arrive jamais au shader et rien ne le dit.
//
// Le temoin d'images est donc AVEUGLE a cette classe de defaut. La parade est
// ici : un seul nombre, cite par les deux cotes, et un banc qui va LIRE le
// shader sur le disque pour le confronter a la constante C++.

// Lit un fichier texte entier. `fopen` et non NKFileSystem : ce banc ne lie que
// NKSL et la Foundation, et lier tout le systeme de fichiers pour lire un
// shader lui couterait sa raison d'etre — construire en deux secondes et tourner
// sans GPU. C'est du C, pas de la STL : la regle zero-STL est tenue.
static bool LireFichier(const char *chemin, NkString &out) {
	FILE *f = fopen(chemin, "rb");
	if (!f)
		return false;
	out = NkString("");
	char buf[4096];
	size_t n = 0;
	while ((n = fread(buf, 1, sizeof(buf) - 1, f)) > 0) {
		buf[n] = 0;
		out.Append(buf);
	}
	fclose(f);
	return out.Size() > 0;
}

static const char *const kCheminLayeredV1 = "Resources/NKRenderer/Shaders/LayeredV1/NkSL/layeredv1.frag.nksl";

// Cherche `motif` et rend la position du premier caractere apres, ou -1.
static int32 Apres(const NkString &s, const char *motif) {
	const char *p = s.CStr();
	const char *base = p;
	while (p && *p) {
		const char *a = p;
		const char *b = motif;
		while (*b && *a == *b) {
			++a;
			++b;
		}
		if (*b == 0)
			return (int32)(a - base);
		++p;
	}
	return -1;
}

// Recherche INSENSIBLE A LA CASSE.
//
// Elle existe parce que mon premier controle a mesure le mauvais objet, et le
// 2026-08-22 : il cherchait « tMask » dans le HLSL genere, ne le trouvait pas, et
// j'ai failli en conclure que DX repliait la branche de texture. Le generateur
// HLSL **met les identifiants en minuscules et les suffixe** — le sampler s'y
// appelle `tmask_tex`, et il est bel et bien la, avec son `.Sample(...)`. Chercher
// un nom exact a travers un generateur de code, c'est tester le generateur de
// noms, pas le shader.
static bool ContientSansCasse(const NkString &s, const char *motif) {
	auto bas = [](char c) -> char { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; };
	const char *p = s.CStr();
	while (p && *p) {
		const char *a = p;
		const char *b = motif;
		while (*b && bas(*a) == bas(*b)) {
			++a;
			++b;
		}
		if (*b == 0)
			return true;
		++p;
	}
	return false;
}

// ── outil : REORDONNER les lignes `sock` d'un noeud dans un texte serialise ──
//
// La perturbation doit etre REELLE, pas cosmetique : on INVERSE l'ordre, ce qui
// change l'index de toutes les prises du noeud des qu'il en a plus d'une.
static NkString InverseLesSock(const NkString &texte, uint32 idNoeud) {
	NkString marque("sock ");
	marque.Append(NkFormat("{0}", idNoeud));
	marque.Append(" ");
	NkVector<NkString> visees;
	{
		const char *p = texte.CStr();
		while (p && *p) {
			const char *fin = p;
			while (*fin && *fin != '\n')
				++fin;
			NkString ligne;
			for (const char *q = p; q < fin; ++q)
				ligne.Append(*q);
			bool debute = ligne.Size() >= marque.Size();
			for (uint32 k = 0; debute && k < (uint32)marque.Size(); ++k)
				if (ligne.CStr()[k] != marque.CStr()[k])
					debute = false;
			if (debute)
				visees.PushBack(ligne);
			p = (*fin == '\n') ? fin + 1 : fin;
		}
	}
	NkString sortie;
	uint32 rang = 0;
	{
		const char *p = texte.CStr();
		while (p && *p) {
			const char *fin = p;
			while (*fin && *fin != '\n')
				++fin;
			NkString ligne;
			for (const char *q = p; q < fin; ++q)
				ligne.Append(*q);
			bool debute = ligne.Size() >= marque.Size();
			for (uint32 k = 0; debute && k < (uint32)marque.Size(); ++k)
				if (ligne.CStr()[k] != marque.CStr()[k])
					debute = false;
			if (debute && visees.Size() > 0) {
				sortie.Append(visees[(uint32)visees.Size() - 1 - rang]);
				++rang;
			} else {
				sortie.Append(ligne);
			}
			sortie.Append('\n');
			p = (*fin == '\n') ? fin + 1 : fin;
		}
	}
	return sortie;
}

// ── LE TEMOIN QUI PAIE LA VERSION 2 DU FORMAT ───────────────────────────────
//
// 🔴 C'EST CE CAS QUI JUSTIFIE LE CHANGEMENT, PAS LE RAISONNEMENT. Condition
// posee par Rodolf : « un fichier dont les `sock` sont reordonnes doit soit
// charger A L'IDENTIQUE, soit etre REFUSE en se nommant. Jamais charger autre
// chose en silence. »
//
// ⚠️ ET IL DISCRIMINE DANS LES DEUX SENS. Un cas qui ne montrerait que le vert
// de la version 2 serait vert AUSSI si l'inversion ne perturbait rien -- une
// assertion juste sur une perturbation qui n'a pas eu lieu ne mesure rien. Le
// second volet rejoue donc LA MEME inversion sur un fichier en version 1 et
// exige qu'elle REPOINTE. C'est la troisieme facon dont un cas juste ne mesure
// rien -- il manquait la PROFONDEUR -- prise par l'autre bout : ici c'est la
// perturbation elle-meme qu'il faut prouver reelle.
static void CasFichierOrdreDesSock() {
	// ── VOLET 1 : EN VERSION 2, L'ORDRE N'A PLUS DE SENS ─────────────────
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	g.Connect(bsdf, "bsdf", out, "surface");

	// On pose un DEFAUT sur une prise du Principled : la ligne `def` portait
	// elle aussi un index, et elle avait exactement le meme piege. Un temoin
	// qui ne verifierait que le lien laisserait la moitie du defaut en place.
	const NkNode *nb = g.Find(bsdf);
	NkString prisePosee;
	uint32 nbPrises = 0;
	if (nb) {
		nbPrises = (uint32)nb->sockets.Size();
		for (uint32 i = 0; i < nbPrises; ++i)
			if (nb->sockets[i].dir == NkSocketDir::Input && nb->sockets[i].type == t.real) {
				prisePosee = nb->sockets[i].name;
				break;
			}
	}
	const bool defautPose = prisePosee.Size() > 0 &&
							g.SetSocketDefault(bsdf, prisePosee.CStr(), NkSocketDir::Input,
											   NkValueReal(t.real, 0.3721f));

	NkString texte;
	g.Serialize(texte);
	const NkString inverse = InverseLesSock(texte, (uint32)bsdf);
	const bool perturbationEcrite = !(inverse == texte) && nbPrises > 1;

	NkNodeGraph h;
	NkString err;
	const bool relu = h.Deserialize(inverse.CStr(), &err);
	const NkLink *lh = h.LinkCount() > 0 ? h.LinkAt(0) : nullptr;
	const NkNode *nho = h.Find(out);
	const bool lienIntact = relu && lh && nho && lh->toSocket >= 0 &&
							lh->toSocket < (int32)nho->sockets.Size() &&
							nho->sockets[(uint32)lh->toSocket].name == NkString("surface");
	const NkGraphValue *dv =
		(relu && prisePosee.Size() > 0) ? h.SocketDefault(bsdf, prisePosee.CStr(), NkSocketDir::Input) : nullptr;
	const bool defautIntact = dv && dv->IsSet() && dv->numbers.Size() > 0 && dv->numbers[0] == 0.3721f;

	// ── VOLET 2 : LE TEMOIN. LA MEME PERTURBATION, EN VERSION 1 ──────────
	// Deux prises de SORTIE sur le meme noeud, un lien sur la premiere. En
	// version 1 le lien vaut « rang 0 » : inverser les `sock` le fait pointer
	// sur l'autre, et rien ne le dit. C'est le defaut que la version 2 retire,
	// montre sur le format qui le porte encore.
	static const char *kV1 = "nkgraph 1\n"
							 "compteurs 3 2\n"
							 "type 1 reel\n"
							 "noeud 1 0.000000 0.000000 essai.source A\n"
							 "sock 1 1 1 sortie_a\n"
							 "sock 1 1 1 sortie_b\n"
							 "noeud 2 0.000000 0.000000 essai.puits B\n"
							 "sock 2 0 1 entree\n"
							 "lien 1 1 0 2 0\n";
	NkNodeGraph v1;
	const bool v1Lu = v1.Deserialize(kV1);
	const NkLink *lv1 = v1.LinkCount() > 0 ? v1.LinkAt(0) : nullptr;
	const NkNode *nv1 = v1.Find(1);
	const bool v1Pointe = v1Lu && lv1 && nv1 && lv1->fromSocket >= 0 &&
						  lv1->fromSocket < (int32)nv1->sockets.Size() &&
						  nv1->sockets[(uint32)lv1->fromSocket].name == NkString("sortie_a");

	const NkString v1Inverse = InverseLesSock(NkString(kV1), 1);
	NkNodeGraph v1b;
	const bool v1bLu = v1b.Deserialize(v1Inverse.CStr());
	const NkLink *lv1b = v1b.LinkCount() > 0 ? v1b.LinkAt(0) : nullptr;
	const NkNode *nv1b = v1b.Find(1);
	const NkString *nomV1b =
		(lv1b && nv1b && lv1b->fromSocket >= 0 && lv1b->fromSocket < (int32)nv1b->sockets.Size())
			? &nv1b->sockets[(uint32)lv1b->fromSocket].name
			: nullptr;
	const bool v1Repointe = v1bLu && nomV1b && *nomV1b == NkString("sortie_b");

	NkString d;
	d = NkFormat("v{0} : {1} prises inversees={2} | le lien reste sur « surface »={3} | le defaut « {4} » reste "
				 "a 0.3721={5} | TEMOIN v1 : la MEME inversion repointe sortie_a -> « {6} »={7}",
				 (uint32)NK_NKGRAPH_VERSION, nbPrises, perturbationEcrite ? 1 : 0, lienIntact ? 1 : 0, prisePosee,
				 (defautPose && defautIntact) ? 1 : 0, nomV1b ? *nomV1b : NkString("?"), v1Repointe ? 1 : 0);
	Cas("fichier/ordre-des-sock",
		perturbationEcrite && lienIntact && defautPose && defautIntact && v1Pointe && v1Repointe, d);
}

// ── UN NOM ABSENT EST REFUSE EN SE NOMMANT ──────────────────────────────────
//
// L'autre branche de la condition : « soit charger a l'identique, SOIT etre
// refuse en se nommant ». Un fichier qui nomme une prise inexistante ne PEUT pas
// charger a l'identique -- il doit donc etre refuse, et le refus doit porter le
// nom demande. Se rabattre sur la prise 0 rendrait un graphe qui charge et qui
// calcule autre chose : « rien » charge comme « la premiere ».
//
// ⚠️ ET LE GRAPHE DOIT RESSORTIR VIDE. Un graphe a moitie charge porterait des
// noeuds justes et des liens faux : l'appelant qui ignore le `false` compilerait
// un materiau qui a l'air complet. Meme regle que « rien emis » sur un refus de
// compilation -- un refus qui laisse de la matiere derriere lui est un piege.
static void CasFichierPriseInconnueRefusee() {
	NkNodeGraph g;
	NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	g.Connect(bsdf, "bsdf", out, "surface");
	NkString texte;
	g.Serialize(texte);

	// On renomme la prise DANS LA LIGNE `lien` seulement : le lien designe
	// alors une prise que le fichier ne contient pas.
	NkString casse;
	{
		const char *p = texte.CStr();
		while (p && *p) {
			const char *fin = p;
			while (*fin && *fin != '\n')
				++fin;
			NkString ligne;
			for (const char *q = p; q < fin; ++q)
				ligne.Append(*q);
			bool estLien = ligne.Size() >= 5;
			const char *kw = "lien ";
			for (uint32 k = 0; estLien && k < 5; ++k)
				if (ligne.CStr()[k] != kw[k])
					estLien = false;
			if (estLien) {
				NkString remplacee;
				const char *q = ligne.CStr();
				while (*q) {
					if (q[0] == 's' && q[1] == 'u' && q[2] == 'r' && q[3] == 'f' && q[4] == 'a' && q[5] == 'c' &&
						q[6] == 'e') {
						remplacee.Append("surfacce_disparue");
						q += 7;
					} else {
						remplacee.Append(*q);
						++q;
					}
				}
				ligne = remplacee;
			}
			casse.Append(ligne);
			casse.Append('\n');
			p = (*fin == '\n') ? fin + 1 : fin;
		}
	}

	NkNodeGraph h;
	NkString err;
	const bool relu = h.Deserialize(casse.CStr(), &err);
	const bool refuse = !relu;
	const bool nommeLaPrise = ContientSansCasse(err, "surfacce_disparue");
	const bool nommeLeSens = ContientSansCasse(err, "entree");
	const bool videApresRefus = h.NodeCount() == 0 && h.LinkCount() == 0;

	NkString d;
	d = NkFormat("refuse={0} | nomme la prise demandee={1} | dit de quel cote={2} | graphe VIDE apres refus={3} "
				 "(noeuds={4} liens={5}) | message : {6}",
				 refuse ? 1 : 0, nommeLaPrise ? 1 : 0, nommeLeSens ? 1 : 0, videApresRefus ? 1 : 0, h.NodeCount(),
				 h.LinkCount(), err);
	Cas("fichier/prise-inconnue-refusee-en-se-nommant", refuse && nommeLaPrise && nommeLeSens && videApresRefus,
		d);
}

// ── LA MIGRATION EXISTE, ET ELLE EST EXERCEE ────────────────────────────────
//
// Condition posee par Rodolf : « une migration qui lit l'ancien -- meme vide de
// logique, elle doit exister ET ETRE EXERCEE ». Elle l'est deja a chaque course
// du banc : les neuf fichiers ecrits a la main des cas `fichier/*` sont tous en
// version 1. Ce cas la NOMME, pour qu'on sache qu'elle est VOULUE et non
// survivante -- une migration que rien ne designe finit par etre retiree par
// quelqu'un qui la prend pour du code mort.
//
// ⚠️ ET IL MESURE L'AUTRE BOUT : une version inconnue est REFUSEE en se nommant.
// Lire un fichier futur « au mieux » rendrait un graphe plausible et faux --
// exactement la meme faute que le repli plausible sur un canal inconnu.

// ═══════════════════════════════════════════════════════════════════════════
// LES ESPACES DE NOMS ET L'EMPREINTE DE STRUCTURE
// ═══════════════════════════════════════════════════════════════════════════
//
// Decision de Rodolf : « on fait comme en C++, et si possible avec des espaces
// de noms et des portees. » Plus la mesure qui a montre que le nom seul ne
// suffit pas.
//
// ⚠️ LE TEMOIN QUI COMPTE, ET C'EST LUI QUI PAIE TOUT LE RESTE : deux fichiers
// declarant LE MEME NOM QUALIFIE avec des CONTENUS DIFFERENTS doivent etre
// refuses EN SE NOMMANT, jamais relies en silence.

// ═══════════════════════════════════════════════════════════════════════════
// LA FAMILLE DE PRISE — DONNEE contre EXECUTION (§ 20.2 de la specification)
// ═══════════════════════════════════════════════════════════════════════════
//
// ⚠️ LES QUATRE CONTROLES SONT ECRITS AVANT LE CODE, comme le § 20.6 l'exige.
//
// 🔴 MAIS ILS N'ONT PAS PU ETRE « VERIFIES ROUGES » AVANT, ET C'EST UNE LIMITE
// QU'IL FAUT DIRE AU LIEU DE LA MAQUILLER. J'avais d'abord ecrit ici qu'ils
// l'avaient ete ; c'etait faux. Ils emploient `NkSocketFamily`, un `AddSocket`
// a cinq arguments, `FamilyMismatch` et `ExecOutputAlreadyBound` -- AUCUN
// n'existe avant l'implementation. Sans elle, ils ne ROUGISSENT pas : ils NE
// COMPILENT PAS.
//
// « Ne compile pas » est une preuve plus FAIBLE que « rougit » : elle dit que le
// cas parle d'une API neuve, pas qu'il mesure le bon comportement. Un cas qui
// appellerait la nouvelle API et n'assertait rien d'utile ne compilerait pas non
// plus, et passerait pour valide.
//
// ✅ CE QUI TIENT LIEU DE PREUVE A LA PLACE : une MUTATION par comportement
// (M33 a M37). Chacune retire UNE des quatre lignes du tableau du § 20.2 et
// doit faire rougir le controle qui la vise. C'est plus fort que « rouge
// avant », parce que ca mesure chaque comportement SEPAREMENT au lieu de
// mesurer leur absence en bloc.
//
// 🔴 LE PIEGE QUE LA SPECIFICATION NOMME, ET QU'ON NE PREND PAS. Le registre de
// types est PLAT : on peut y enregistrer un type « exec » et brancher
// exec-sur-exec des aujourd'hui, sans toucher au coeur. CA MARCHERAIT --
// `Accepts()` serait content -- et c'est exactement ce qui le rend dangereux :
// l'arite resterait celle de la donnee, le cycle resterait refuse, et RIEN ne
// le signalerait.
//
//   UN TYPE dit CE QUI PASSE.  UNE FAMILLE dit COMMENT CA SE BRANCHE.
//
// Ce sont deux axes, et les confondre donne une solution qui fonctionne assez
// pour qu'on l'adopte, et pas assez pour qu'elle serve.

// Monte un noeud portant les deux familles en entree et en sortie.
static NkNodeId PoseNoeudMixte(NkNodeGraph &g, NkTypeId tReel, const char *type) {
	const NkNodeId n = g.AddNode(type, type);
	g.AddSocket(n, "avant", tReel, NkSocketDir::Input, NkSocketFamily::Exec);
	g.AddSocket(n, "apres", tReel, NkSocketDir::Output, NkSocketFamily::Exec);
	g.AddSocket(n, "valeur", tReel, NkSocketDir::Input, NkSocketFamily::Data);
	g.AddSocket(n, "resultat", tReel, NkSocketDir::Output, NkSocketFamily::Data);
	return n;
}

// ── CONTROLE 1 : L'ARITE CROISEE ───────────────────────────────────────────
// C'est le controle qui distingue vraiment les deux familles, parce qu'il porte
// sur le COMPORTEMENT et non sur un drapeau. Lire `socket.family` prouverait
// que le champ existe ; ici on prouve qu'il SERT.
static void CasExecAriteCroisee() {
	NkNodeGraph g;
	const NkTypeId r = g.RegisterType("reel");
	const NkNodeId a = PoseNoeudMixte(g, r, "essai.a");
	const NkNodeId b = PoseNoeudMixte(g, r, "essai.b");
	const NkNodeId c = PoseNoeudMixte(g, r, "essai.c");

	// ENTREE d'EXECUTION : plusieurs sources tiennent -- dix chemins peuvent
	// mener au meme noeud.
	const NkLinkError e1 = g.Connect(a, "apres", c, "avant");
	const NkLinkError e2 = g.Connect(b, "apres", c, "avant");
	uint32 versAvant = 0;
	for (uint32 i = 0; i < g.LinkCount(); ++i) {
		const NkLink *l = g.LinkAt(i);
		const NkNode *n = g.Find(c);
		if (l && l->alive && l->toNode == c && n && l->toSocket >= 0 &&
			l->toSocket < (int32)n->sockets.Size() && n->sockets[(uint32)l->toSocket].name == NkString("avant"))
			++versAvant;
	}
	const bool deuxExecTiennent = e1 == NkLinkError::Ok && e2 == NkLinkError::Ok && versAvant == 2;

	// ENTREE de DONNEE : la seconde REMPLACE -- comportement inchange.
	const NkLinkError d1 = g.Connect(a, "resultat", c, "valeur");
	const NkLinkError d2 = g.Connect(b, "resultat", c, "valeur");
	uint32 versValeur = 0;
	for (uint32 i = 0; i < g.LinkCount(); ++i) {
		const NkLink *l = g.LinkAt(i);
		const NkNode *n = g.Find(c);
		if (l && l->alive && l->toNode == c && n && l->toSocket >= 0 &&
			l->toSocket < (int32)n->sockets.Size() && n->sockets[(uint32)l->toSocket].name == NkString("valeur"))
			++versValeur;
	}
	const bool donneeRemplace = d1 == NkLinkError::Ok && d2 == NkLinkError::Ok && versValeur == 1;

	// SORTIE d'EXECUTION : une seule -- une instruction n'a qu'une suite.
	NkNodeGraph h;
	const NkTypeId r2 = h.RegisterType("reel");
	const NkNodeId x = PoseNoeudMixte(h, r2, "essai.x");
	const NkNodeId y = PoseNoeudMixte(h, r2, "essai.y");
	const NkNodeId z = PoseNoeudMixte(h, r2, "essai.z");
	const NkLinkError s1 = h.Connect(x, "apres", y, "avant");
	const NkLinkError s2 = h.Connect(x, "apres", z, "avant");
	const bool sortieExecUnique = s1 == NkLinkError::Ok && s2 == NkLinkError::ExecOutputAlreadyBound;

	// SORTIE de DONNEE : autant qu'on veut -- inchange.
	const NkLinkError t1 = h.Connect(x, "resultat", y, "valeur");
	const NkLinkError t2 = h.Connect(x, "resultat", z, "valeur");
	const bool sortieDonneeMultiple = t1 == NkLinkError::Ok && t2 == NkLinkError::Ok;

	NkString d;
	d = NkFormat("ENTREE exec : 2 sources tiennent={0} ({1} liens) | ENTREE donnee : la 2e remplace={2} ({3}) | "
				 "SORTIE exec : la 2e refusee par {4}={5} | SORTIE donnee : les 2 tiennent={6}",
				 deuxExecTiennent ? 1 : 0, versAvant, donneeRemplace ? 1 : 0, versValeur,
				 NkString(NkLinkErrorName(s2)), sortieExecUnique ? 1 : 0, sortieDonneeMultiple ? 1 : 0);
	Cas("exec/arite-croisee", deuxExecTiennent && donneeRemplace && sortieExecUnique && sortieDonneeMultiple, d);
}

// ── CONTROLE 2 : LE REFUS CROISE, ET IL DOIT SE NOMMER ─────────────────────
// ⚠️ `FamilyMismatch`, JAMAIS `TypeMismatch`. Les deux prises portent ici le
// MEME type : si le refus s'appelait « type », l'auteur chercherait une
// conversion qui n'existe pas et ne trouverait jamais. Le message est ce qui
// distingue un refus compris d'un refus subi.
static void CasExecRefusCroiseNomme() {
	NkNodeGraph g;
	const NkTypeId r = g.RegisterType("reel");
	const NkNodeId a = PoseNoeudMixte(g, r, "essai.a");
	const NkNodeId b = PoseNoeudMixte(g, r, "essai.b");

	const NkLinkError execVersDonnee = g.Connect(a, "apres", b, "valeur");
	const NkLinkError donneeVersExec = g.Connect(a, "resultat", b, "avant");
	const bool lesDeuxNommes =
		execVersDonnee == NkLinkError::FamilyMismatch && donneeVersExec == NkLinkError::FamilyMismatch;
	// TEMOIN : les memes prises, MEME FAMILLE, passent. Sans lui, un Connect qui
	// refuserait tout rendrait ce cas vert.
	const bool memeFamillePasse = g.Connect(a, "apres", b, "avant") == NkLinkError::Ok &&
								  g.Connect(a, "resultat", b, "valeur") == NkLinkError::Ok;
	// et le type est le MEME des deux cotes : le refus ne peut pas etre un type
	const NkNode *na = g.Find(a);
	const NkNode *nb = g.Find(b);
	const bool memeType = na && nb &&
						  na->sockets[(uint32)na->FindSocket("apres", NkSocketDir::Output)].type ==
							  nb->sockets[(uint32)nb->FindSocket("valeur", NkSocketDir::Input)].type;

	NkString d;
	d = NkFormat("exec->donnee={0} | donnee->exec={1} | les deux sont FamilyMismatch={2} | TEMOIN meme famille "
				 "passe={3} | et le TYPE est identique des deux cotes={4} (donc ce n est pas un TypeMismatch)",
				 NkString(NkLinkErrorName(execVersDonnee)), NkString(NkLinkErrorName(donneeVersExec)),
				 lesDeuxNommes ? 1 : 0, memeFamillePasse ? 1 : 0, memeType ? 1 : 0);
	Cas("exec/refus-croise-nomme", lesDeuxNommes && memeFamillePasse && memeType, d);
}

// ── CONTROLE 3 : L'ACYCLICITE EST UNIVERSELLE ──────────────────────
//
// ⚠️ CE CAS A DIT LE CONTRAIRE PENDANT UNE JOURNEE, et il faut l'ecrire au lieu
// de le remplacer en silence. Il s'appelait
// « exec/cycle-execution-legitime-cycle-donnee-refuse » et il mesurait qu'un
// rebouclage d'EXECUTION etait ACCEPTE. C'etait la proposition du 23/08 au
// matin. ❌ RETIREE le 23/08 au soir par Rodolf : elle affaiblissait une regle
// GENERALE pour un cas PARTICULIER.
//
//     UN CYCLE VIT A L'INTERIEUR D'UN NOEUD, JAMAIS DANS LE GRAPHE.
//
// 🔴 CE QUE CE CAS MESURE MAINTENANT, ET C'EST PLUS FORT QUE « les deux sont
// refuses » : que les deux familles rendent LE MEME REFUS, ET que le besoin
// qu'on refuse ainsi reste EXPRIMABLE par un noeud. Une regle qui interdit sans
// laisser d'issue n'est pas une regle, c'est un mur -- et c'est exactement ce
// que le § 19.3 croyait avoir trouve.
//
// 📌 LE TEMOIN DU TRI EST LA PIECE QU'ON OUBLIERAIT. `TopoSort` comptait lui
// aussi la seule donnee ; il compte desormais TOUS les liens. Sans un controle
// qui regarde l'ORDRE RENDU, ce changement-la serait invisible : un tri qui
// ignore les fils d'execution rend un ordre parfaitement valide -- simplement
// moins contraint. Il faut donc verifier qu'une succession d'execution est
// HONOREE dans l'ordre, pas seulement que le tri reussit.
static void CasAcycliciteUniverselle() {
	// (1) rebouclage d'EXECUTION : REFUSE, et par WouldCycle
	NkNodeGraph g;
	const NkTypeId r = g.RegisterType("reel");
	const NkNodeId a = PoseNoeudMixte(g, r, "essai.a");
	const NkNodeId b = PoseNoeudMixte(g, r, "essai.b");
	const NkLinkError e1 = g.Connect(a, "apres", b, "avant");
	const NkLinkError e2 = g.Connect(b, "apres", a, "avant"); // refermerait la boucle
	const bool cycleExecRefuse = e1 == NkLinkError::Ok && e2 == NkLinkError::WouldCycle;

	// (2) le MEME motif en DONNEE : le MEME refus, sous le MEME nom. C'est ca,
	//     « sans exception » : un outil n'a pas a demander la famille pour
	//     savoir ce qui va se passer.
	NkNodeGraph h;
	const NkTypeId r2 = h.RegisterType("reel");
	const NkNodeId x = PoseNoeudMixte(h, r2, "essai.x");
	const NkNodeId y = PoseNoeudMixte(h, r2, "essai.y");
	const NkLinkError d1 = h.Connect(x, "resultat", y, "valeur");
	const NkLinkError d2 = h.Connect(y, "resultat", x, "valeur");
	const bool cycleDonneeRefuse = d1 == NkLinkError::Ok && d2 == NkLinkError::WouldCycle;
	const bool memeRefus = e2 == d2;

	// (3) TEMOIN : LE BESOIN RESTE EXPRIMABLE -- la boucle est un NOEUD.
	//     `Boucle Pour` a une sortie « corps » et une sortie « termine », et le
	//     corps NE REVIENT JAMAIS au noeud par un fil : le noeud itere lui-meme.
	//     C'est la forme d'Unreal, et elle passe sans toucher a l'acyclicite.
	//
	// 🔴 LES NOEUDS SONT POSES A L'ENVERS DE LEUR ORDRE D'EXECUTION, ET CE
	// N'EST PAS UNE COQUETTERIE. Mesure du 24/08 : la premiere version les
	// posait dans l'ordre naturel, et la mutation M44 -- `TopoSort` recompte la
	// seule donnee -- A SURVECU. Cause : sans lien de DONNEE, tous les degres
	// entrants valent zero, le tri emet dans l'ORDRE D'INSERTION, et cet ordre
	// satisfaisait par hasard les comparaisons de rang. Le controle rendait donc
	// vert pour une raison qu'il n'annoncait pas (regle 1).
	//
	// A l'envers, l'ordre d'insertion CONTREDIT l'ordre d'execution : seul un
	// tri qui compte VRAIMENT les fils d'execution peut rendre les bons rangs.
	NkNodeGraph k;
	const NkTypeId r3 = k.RegisterType("reel");
	const NkNodeId fin = k.AddNode("flot.fin", "Fin");
	k.AddSocket(fin, "avant", r3, NkSocketDir::Input, NkSocketFamily::Exec);
	const NkNodeId corps = k.AddNode("flot.action", "Action");
	k.AddSocket(corps, "avant", r3, NkSocketDir::Input, NkSocketFamily::Exec);
	k.AddSocket(corps, "apres", r3, NkSocketDir::Output, NkSocketFamily::Exec);
	const NkNodeId boucle = k.AddNode("flot.boucle_pour", "Boucle Pour");
	k.AddSocket(boucle, "avant", r3, NkSocketDir::Input, NkSocketFamily::Exec);
	k.AddSocket(boucle, "corps", r3, NkSocketDir::Output, NkSocketFamily::Exec);
	k.AddSocket(boucle, "termine", r3, NkSocketDir::Output, NkSocketFamily::Exec);
	const NkNodeId debut = k.AddNode("flot.debut", "Debut");
	k.AddSocket(debut, "apres", r3, NkSocketDir::Output, NkSocketFamily::Exec);
	const bool boucleExprimable = k.Connect(debut, "apres", boucle, "avant") == NkLinkError::Ok &&
								  k.Connect(boucle, "corps", corps, "avant") == NkLinkError::Ok &&
								  k.Connect(boucle, "termine", fin, "avant") == NkLinkError::Ok;
	// et le fil qui REVIENDRAIT au noeud de boucle -- la forme naive -- est
	// refuse. Sans cette ligne, le temoin ne prouverait que « ca se branche ».
	const NkLinkError retourNaif = k.Connect(corps, "apres", boucle, "avant");
	const bool retourRefuse = retourNaif == NkLinkError::WouldCycle;

	// (4) TEMOIN DU TRI : il compte MAINTENANT les fils d'execution. L'ordre
	//     rendu doit honorer debut < boucle < corps et boucle < fin -- ET les
	//     noeuds ont ete poses A L'ENVERS, donc l'ordre d'insertion ne peut pas
	//     rendre ce vert tout seul.
	NkVector<NkNodeId> ordre;
	const bool trie = k.TopoSort(ordre) && ordre.Size() == 4;
	auto rang = [&](NkNodeId id) -> int32 {
		for (uint32 i = 0; i < (uint32)ordre.Size(); ++i)
			if (ordre[i] == id)
				return (int32)i;
		return -1;
	};
	const bool ordreHonoreLExecution = trie && rang(debut) < rang(boucle) && rang(boucle) < rang(corps) &&
									   rang(boucle) < rang(fin);

	NkString d;
	d = NkFormat("cycle d EXECUTION REFUSE={0} ({1}) | cycle de DONNEE refuse={2} ({3}) | MEME refus des deux "
				 "cotes={4} | TEMOIN la boucle-NOEUD s exprime={5} et le retour naif est refuse ({6})={7} | "
				 "TEMOIN le tri HONORE la succession d execution={8} ({9} noeuds)",
				 cycleExecRefuse ? 1 : 0, NkString(NkLinkErrorName(e2)), cycleDonneeRefuse ? 1 : 0,
				 NkString(NkLinkErrorName(d2)), memeRefus ? 1 : 0, boucleExprimable ? 1 : 0,
				 NkString(NkLinkErrorName(retourNaif)), retourRefuse ? 1 : 0, ordreHonoreLExecution ? 1 : 0,
				 (uint32)ordre.Size());
	Cas("exec/acyclicite-universelle-la-boucle-est-un-noeud",
		cycleExecRefuse && cycleDonneeRefuse && memeRefus && boucleExprimable && retourRefuse &&
			ordreHonoreLExecution,
		d);
}

// ── CONTROLE 4 : L'ALLER-RETOUR, OCTET POUR OCTET ──────────────────────────
// ⚠️ C'EST LUI QUI ATTRAPE LE § 20.4, et le seul qui le puisse : si les liens
// etaient adresses par INDICE, une prise inseree AU MILIEU produirait un
// fichier VALIDE ET FAUX, et seuls les octets le diraient.
//
// 📌 Ce controle passe deja, parce que le format est passe au NOM le 23/08 --
// le besoin 3 du § 20 etait fait avant d'etre demande. Il reste ecrit : c'est
// lui qui empechera d'y revenir.
static void CasExecAllerRetourOctetPourOctet() {
	NkNodeGraph g;
	const NkTypeId r = g.RegisterType("reel");
	const NkNodeId a = PoseNoeudMixte(g, r, "essai.a");
	const NkNodeId b = PoseNoeudMixte(g, r, "essai.b");
	g.Connect(a, "apres", b, "avant");
	g.Connect(a, "resultat", b, "valeur");
	// une prise inseree AU MILIEU de la liste : elle decale tous les index qui
	// suivent, et c'est exactement ce que le format ne doit plus voir.
	g.AddSocket(a, "insere_au_milieu", r, NkSocketDir::Input, NkSocketFamily::Data);

	NkString t1;
	g.Serialize(t1);
	NkNodeGraph h;
	NkString err;
	const bool relu = h.Deserialize(t1.CStr(), &err);
	NkString t2;
	h.Serialize(t2);
	const bool identique = relu && (t1 == t2) && t1.Size() > 0;

	// et les DEUX familles ont survecu au voyage
	const NkNode *nb = h.Find(b);
	const int32 iAvant = nb ? nb->FindSocket("avant", NkSocketDir::Input) : -1;
	const int32 iValeur = nb ? nb->FindSocket("valeur", NkSocketDir::Input) : -1;
	const bool famillesRelues = nb && iAvant >= 0 && iValeur >= 0 &&
								nb->sockets[(uint32)iAvant].family == NkSocketFamily::Exec &&
								nb->sockets[(uint32)iValeur].family == NkSocketFamily::Data;

	NkString d;
	d = NkFormat("relu={0} | ecrire->relire->reecrire IDENTIQUE octet pour octet={1} ({2} octets) | les deux "
				 "familles ont survecu={3} | {4}",
				 relu ? 1 : 0, identique ? 1 : 0, (uint32)t1.Size(), famillesRelues ? 1 : 0, err);
	Cas("exec/aller-retour-octet-pour-octet", identique && famillesRelues, d);
}


// ── LE PIEGE DU § 20.2, MESURE AU LIEU D'ETRE CRU ──────────────────────────
//
// 🔴 LA SPECIFICATION DIT : « on peut enregistrer un type `exec` et brancher
// exec-sur-exec des aujourd'hui. CA MARCHERAIT, et c'est exactement ce qui le
// rend dangereux. » Ce cas le VERIFIE, dans les deux sens.
//
// Pourquoi le mesurer alors qu'on a choisi l'autre voie : parce qu'un piege
// decrit se transmet mal. Le prochain qui voudra « faire simple » retrouvera
// tout seul l'idee du type `exec`, essaiera, verra que ca marche, et n'aura
// aucune raison de se mefier -- sauf si un cas lui montre EXACTEMENT ou ca
// casse. Un avertissement dans un commentaire n'aurait pas cette force.
static void CasExecLePiegeDuTypeExec() {
	// La voie du piege : `exec` enregistre comme un TYPE, prises en famille
	// Data (puisqu'on n'a rien change au coeur, dans cette hypothese).
	NkNodeGraph g;
	const NkTypeId tExec = g.RegisterType("exec");
	const NkTypeId tReel = g.RegisterType("reel");
	auto pose = [&](const char *nom) {
		const NkNodeId n = g.AddNode(nom, nom);
		g.AddSocket(n, "avant", tExec, NkSocketDir::Input);	 // famille Data !
		g.AddSocket(n, "apres", tExec, NkSocketDir::Output); // famille Data !
		g.AddSocket(n, "valeur", tReel, NkSocketDir::Input);
		return n;
	};
	const NkNodeId a = pose("piege.a");
	const NkNodeId b = pose("piege.b");
	const NkNodeId c = pose("piege.c");

	// ✅ CE QUI MARCHE, ET C'EST TOUT LE PROBLEME : exec-sur-exec est accepte.
	const NkLinkError branche = g.Connect(a, "apres", b, "avant");
	const bool caMarche = branche == NkLinkError::Ok;
	// et le croisement est refuse... mais par le MAUVAIS motif.
	const NkLinkError croise = g.Connect(a, "apres", b, "valeur");
	const bool refusePourLaMauvaiseRaison = croise == NkLinkError::TypeMismatch;

	// 🔴 ET VOICI LES TROIS LIGNES DU TABLEAU QUI RESTENT FAUSSES :

	// (1) l'arite d'ENTREE reste celle de la donnee : la 2e source REMPLACE,
	//     et huit chemins sur dix disparaissent sans un mot.
	g.Connect(a, "apres", c, "avant");
	g.Connect(b, "apres", c, "avant");
	uint32 versC = 0;
	for (uint32 i = 0; i < g.LinkCount(); ++i) {
		const NkLink *l = g.LinkAt(i);
		if (l && l->alive && l->toNode == c)
			++versC;
	}
	const bool ariteEntreeFausse = versC == 1; // il en faudrait DEUX

	// (2) l'arite de SORTIE reste celle de la donnee : deux suites acceptees.
	const bool ariteSortieFausse = g.LinkCount() > 0; // a et b ont chacun tire
	NkNodeGraph h;
	const NkTypeId hExec = h.RegisterType("exec");
	const NkNodeId x = h.AddNode("piege.x", "x");
	const NkNodeId y = h.AddNode("piege.y", "y");
	const NkNodeId z = h.AddNode("piege.z", "z");
	h.AddSocket(x, "apres", hExec, NkSocketDir::Output);
	h.AddSocket(y, "avant", hExec, NkSocketDir::Input);
	h.AddSocket(z, "avant", hExec, NkSocketDir::Input);
	const bool deuxSuitesAcceptees =
		h.Connect(x, "apres", y, "avant") == NkLinkError::Ok &&
		h.Connect(x, "apres", z, "avant") == NkLinkError::Ok; // il faudrait un REFUS

	// (3) ⚠️ CETTE LIGNE-LA N'EST PLUS UN GRIEF, ET IL FAUT LE DIRE.
	//     Le piege refuse le rebouclage d'execution comme un cycle de donnee.
	//     C'ETAIT le quatrieme grief tant que la voie choisie l'acceptait ; la
	//     decision du 23/08 au soir -- L'ACYCLICITE EST UNIVERSELLE -- fait que
	//     la voie choisie le refuse AUSSI. Les deux voies se comportent donc
	//     PAREIL ici : le piege garde trois defauts, plus quatre.
	//
	//     🔴 ON LE MESURE QUAND MEME, en le nommant pour ce qu'il est : une
	//     COINCIDENCE. Un lecteur qui verrait « le piege refuse la boucle, la
	//     voie choisie aussi » pourrait en conclure que le piege est devenu
	//     acceptable. Il ne l'est pas -- il l'est simplement pour la mauvaise
	//     raison, et sur les trois autres lignes il reste faux.
	NkNodeGraph k;
	const NkTypeId kExec = k.RegisterType("exec");
	const NkNodeId p = k.AddNode("piege.p", "p");
	const NkNodeId q = k.AddNode("piege.q", "q");
	k.AddSocket(p, "avant", kExec, NkSocketDir::Input);
	k.AddSocket(p, "apres", kExec, NkSocketDir::Output);
	k.AddSocket(q, "avant", kExec, NkSocketDir::Input);
	k.AddSocket(q, "apres", kExec, NkSocketDir::Output);
	k.Connect(p, "apres", q, "avant");
	const NkLinkError boucle = k.Connect(q, "apres", p, "avant");
	const bool memeVerdictQueLaVoieChoisie = boucle == NkLinkError::WouldCycle;

	// ── ET LA VOIE CHOISIE, SUR LE MEME MOTIF, FAIT LES TROIS ───────────
	// ⚠️ TROIS, PLUS QUATRE -- depuis que l'acyclicite est universelle, la
	// famille ne commande plus le cycle. Le compte a baisse et le texte suit :
	// une preuve qui annonce quatre et n'en montre que trois se lit comme une
	// preuve qui echoue.
	//
	// TEMOIN INDISPENSABLE : sans lui, ce cas prouverait seulement que le piege
	// existe, pas que la famille le resout.
	NkNodeGraph bon;
	const NkTypeId br = bon.RegisterType("reel");
	const NkNodeId ba = PoseNoeudMixte(bon, br, "bon.a");
	const NkNodeId bb = PoseNoeudMixte(bon, br, "bon.b");
	const NkNodeId bc = PoseNoeudMixte(bon, br, "bon.c");
	const bool laFamilleResout =
		// compatibilite : le croisement se NOMME
		bon.Connect(ba, "apres", bb, "valeur") == NkLinkError::FamilyMismatch &&
		// arite d ENTREE : deux sources d execution TIENNENT
		bon.Connect(ba, "apres", bc, "avant") == NkLinkError::Ok &&
		bon.Connect(bb, "apres", bc, "avant") == NkLinkError::Ok &&
		// arite de SORTIE : la seconde suite est REFUSEE, et nommee
		bon.Connect(ba, "apres", bb, "avant") == NkLinkError::ExecOutputAlreadyBound;

	NkString d;
	d = NkFormat("LE PIEGE : exec-sur-exec par le TYPE marche={0} | mais le croisement se nomme « {1} » au lieu "
				 "de familles-incompatibles={2} | arite d ENTREE FAUSSE ({3} lien au lieu de 2)={4} | deux "
				 "SUITES acceptees a tort={5} | le rebouclage rend {6} des DEUX cotes (plus un grief depuis "
				 "l acyclicite universelle)={7} | TEMOIN : la FAMILLE fait les TROIS={8}",
				 caMarche ? 1 : 0, NkString(NkLinkErrorName(croise)), refusePourLaMauvaiseRaison ? 1 : 0, versC,
				 ariteEntreeFausse ? 1 : 0, deuxSuitesAcceptees ? 1 : 0, NkString(NkLinkErrorName(boucle)),
				 memeVerdictQueLaVoieChoisie ? 1 : 0, laFamilleResout ? 1 : 0);
	Cas("exec/le-piege-du-type-exec",
		caMarche && refusePourLaMauvaiseRaison && ariteEntreeFausse && deuxSuitesAcceptees &&
			memeVerdictQueLaVoieChoisie && laFamilleResout && ariteSortieFausse,
		d);
}


// ── LE LIEN QUALIFIE (§ 20.3) ──────────────────────────────────────────────
//
// Une transition d'animation porte `(paramName, NkCondKind, threshold,
// fadeDur)` et veut en plus un mini-graphe de condition. Un lien nu ne peut
// rien de tout ca.
//
// 📌 ET LA SPECIFICATION SE TROMPE SUR UNE PREMISSE, MESUREE ICI : elle ecrit
// « il n'existe AUCUNE valeur dans NKGraph, pour rien ». C'est faux --
// `NkGraphProp`, `SetProp` et `SetSocketDefault` existent depuis le debut et
// servent les proprietes de noeud et les defauts de prise. Le besoin 2 coute
// donc BEAUCOUP moins cher que prevu : on REUTILISE le mecanisme au lieu d'en
// ecrire un second, qui aurait diverge du premier au premier changement de
// format.
//
// ⚠️ CE QU'ON NE FAIT PAS : une union typee dans `NkLink`. Elle grossirait a
// chaque consommateur -- l'animation aujourd'hui, le sequenceur demain -- et
// chaque ajout casserait le format. Une indirection ne grossit pas.
static void CasLienQualifie() {
	NkNodeGraph g;
	const NkTypeId r = g.RegisterType("reel");
	const NkNodeId a = PoseNoeudMixte(g, r, "etat.a");
	const NkNodeId b = PoseNoeudMixte(g, r, "etat.b");
	NkLinkId id = 0;
	const NkLinkError e = g.Connect(a, "apres", b, "avant", &id);

	// ── les REGLAGES de l'arc : le mecanisme de valeur des noeuds ────────
	const bool poses = g.SetLinkProp(id, "seuil", NkValueReal(r, 0.75f)) &&
					   g.SetLinkProp(id, "duree_fondu", NkValueReal(r, 0.25f)) &&
					   g.SetLinkProp(id, "parametre", NkValueText(r, "vitesse"));
	// poser deux fois la meme cle REMPLACE -- deux homonymes rendraient la
	// lecture dependante de l'ordre d'insertion.
	const bool remplace = g.SetLinkProp(id, "seuil", NkValueReal(r, 0.9f)) && g.LinkPropCount(id) == 3;
	const NkGraphValue *seuil = g.FindLinkProp(id, "seuil");
	const bool relit = seuil && seuil->IsSet() && seuil->numbers.Size() > 0 && seuil->numbers[0] == 0.9f;
	// une cle inconnue rend `nullptr`, pas une valeur vide.
	const bool inconnueNulle = g.FindLinkProp(id, "cle_absente") == nullptr;

	// ── la CONDITION : une reference de sous-graphe ──────────────────────
	// ⚠️ `nullptr` = LE LIEN N'EXISTE PAS ; chaine vide = il existe et n'a pas
	// de condition. Deux etats, deux reponses -- regle 3.
	const NkString *avant = g.LinkSubgraph(id);
	const bool videAuDepart = avant && avant->Size() == 0;
	const bool lienInexistantNul = g.LinkSubgraph(9999u) == nullptr;
	const bool poseSg = g.SetLinkSubgraph(id, "cond.vitesse_haute");
	const NkString *apres = g.LinkSubgraph(id);
	const bool sgRelu = apres && *apres == NkString("cond.vitesse_haute");

	// ── ET TOUT CA SURVIT AU FICHIER, OCTET POUR OCTET ───────────────────
	NkString t1;
	g.Serialize(t1);
	NkNodeGraph h;
	NkString err;
	const bool relu = h.Deserialize(t1.CStr(), &err);
	NkString t2;
	h.Serialize(t2);
	const bool identique = relu && (t1 == t2);
	const NkGraphValue *seuilRelu = relu ? h.FindLinkProp(id, "seuil") : nullptr;
	const NkString *sgApresVoyage = relu ? h.LinkSubgraph(id) : nullptr;
	const bool voyageOk = seuilRelu && seuilRelu->numbers.Size() > 0 && seuilRelu->numbers[0] == 0.9f &&
						  sgApresVoyage && *sgApresVoyage == NkString("cond.vitesse_haute") &&
						  h.LinkPropCount(id) == 3;

	// ── ET L'ORDRE DES LIGNES NE DOIT RIEN VALOIR ───────────────────────
	// 🔴 C'EST CE VOLET QUI MESURE LA TROISIEME PASSE, et il a fallu le
	// decouvrir : la mutation qui supprime la passe (M42) ne mesurait PAS la
	// dependance a l'ordre, elle mesurait « la qualification est-elle lue du
	// tout ». Deux choses differentes, et la premiere est celle qui compte.
	//
	// On remonte donc toutes les lignes `lienp` / `liensg` AVANT les lignes
	// `lien` qu'elles qualifient -- ce que ferait n'importe quel producteur
	// autre que notre propre ecrivain. Le fichier doit charger A L'IDENTIQUE.
	NkString remonte;
	{
		NkString qualifs, reste;
		const char *q = t1.CStr();
		while (q && *q) {
			const char *fin = q;
			while (*fin && *fin != '\n')
				++fin;
			NkString ligne;
			for (const char *z = q; z < fin; ++z)
				ligne.Append(*z);
			const char *k1 = "lienp ";
			const char *k2 = "liensg ";
			bool estQ = ligne.Size() >= 6;
			for (uint32 z = 0; estQ && z < 6; ++z)
				if (ligne.CStr()[z] != k1[z])
					estQ = false;
			if (!estQ && ligne.Size() >= 7) {
				estQ = true;
				for (uint32 z = 0; estQ && z < 7; ++z)
					if (ligne.CStr()[z] != k2[z])
						estQ = false;
			}
			if (estQ) {
				qualifs.Append(ligne);
				qualifs.Append('\n');
			} else {
				reste.Append(ligne);
				reste.Append('\n');
			}
			q = (*fin == '\n') ? fin + 1 : fin;
		}
		// l'en-tete doit rester en premier : on insere juste apres.
		const char *rp = reste.CStr();
		const char *finEntete = rp;
		while (*finEntete && *finEntete != '\n')
			++finEntete;
		for (const char *z = rp; z <= finEntete && *z; ++z)
			remonte.Append(*z);
		remonte.Append(qualifs);
		remonte.Append(finEntete && *finEntete ? finEntete + 1 : "");
	}
	NkNodeGraph ro;
	NkString errOrdre;
	const bool reluRemonte = ro.Deserialize(remonte.CStr(), &errOrdre);
	const NkGraphValue *seuilRemonte = reluRemonte ? ro.FindLinkProp(id, "seuil") : nullptr;
	const NkString *sgRemonte = reluRemonte ? ro.LinkSubgraph(id) : nullptr;
	const bool ordreNeVautRien = reluRemonte && seuilRemonte && seuilRemonte->numbers.Size() > 0 &&
								 seuilRemonte->numbers[0] == 0.9f && sgRemonte &&
								 *sgRemonte == NkString("cond.vitesse_haute") && ro.LinkPropCount(id) == 3;
	// et la perturbation doit avoir EU LIEU, sinon ce vert ne vaut rien
	const bool perturbationReelle = !(remonte == t1);

	// ── TEMOIN : un lien NU produit les memes octets qu'avant ────────────
	// Sans lui, on ne saurait pas si la qualification pese sur les documents
	// qui ne s'en servent pas.
	NkNodeGraph nu;
	const NkTypeId rn = nu.RegisterType("reel");
	const NkNodeId na = PoseNoeudMixte(nu, rn, "etat.a");
	const NkNodeId nb2 = PoseNoeudMixte(nu, rn, "etat.b");
	nu.Connect(na, "apres", nb2, "avant");
	NkString tn;
	nu.Serialize(tn);
	const bool lienNuSansLigne = !ContientLigne(tn, "lienp ") && !ContientLigne(tn, "liensg ");

	NkString d;
	d = NkFormat("lien cree={0} | 3 reglages poses={1} la meme cle REMPLACE={2} relue={3} cle inconnue "
				 "nullptr={4} | condition : vide au depart={5} lien inexistant nullptr={6} posee={7} relue={8} "
				 "| ALLER-RETOUR identique={9} ({10} o) et tout a survecu={11} | TEMOIN lien nu : aucune ligne "
				 "de qualification={12} | ORDRE : qualifications remontees AVANT leurs liens={13}, charge a "
				 "l identique={14}",
				 e == NkLinkError::Ok ? 1 : 0, poses ? 1 : 0, remplace ? 1 : 0, relit ? 1 : 0,
				 inconnueNulle ? 1 : 0, videAuDepart ? 1 : 0, lienInexistantNul ? 1 : 0, poseSg ? 1 : 0,
				 sgRelu ? 1 : 0, identique ? 1 : 0, (uint32)t1.Size(), voyageOk ? 1 : 0,
				 lienNuSansLigne ? 1 : 0, perturbationReelle ? 1 : 0, ordreNeVautRien ? 1 : 0);
	Cas("exec/lien-qualifie",
		e == NkLinkError::Ok && poses && remplace && relit && inconnueNulle && videAuDepart &&
			lienInexistantNul && poseSg && sgRelu && identique && voyageOk && lienNuSansLigne &&
			perturbationReelle && ordreNeVautRien,
		d);
}


// ── LA MACHINE A ETATS EST UN NOEUD (§ 19.9) ───────────────────────────────
//
// ✅ TRANCHE PAR RODOLF LE 23/08 : la voie d'Unreal (la machine a etats est un
// noeud du graphe d'animation) contre celle d'Unity (un editeur separe,
// l'Animator). Et l'argument decisif est TECHNIQUE, pas esthetique :
//
//   ses cycles vivent A L'INTERIEUR du noeud, dans la liste de transitions que
//   le runtime porte deja -- `AddTransition(from, to, paramName, kind,
//   threshold, fadeDur)`. LE GRAPHE EXTERIEUR NE VOIT JAMAIS DE CYCLE, donc
//   `WouldCycle` n'est pas touche.
//
// 🔴 CE QUE CE CAS MESURE, ET C'EST UNE MESURE DE COUT, PAS DE FONCTION : que
// le coeur n'a besoin DE RIEN DE NOUVEAU pour porter une machine a etats. Le
// § 19.3 concluait « le mode etats est IMPOSSIBLE sur le coeur d'aujourd'hui » ;
// la conclusion etait fausse, et ce sont ces quelques lignes qui le disent --
// aucune API neuve n'y apparait.
//
// ⚠️ LA REFERENCE VA DANS UNE PROPRIETE, PAS DANS `NkNode::subgraph`. La
// tentation est forte, parce que le champ existe et qu'il porte deja une
// reference par nom. Mais `subgraph` veut dire « un NkNodeGraph de ce
// document », et UNE MACHINE A ETATS N'EN EST PAS UN : c'est une liste d'etats
// et de transitions, avec son propre format. Deux sens pour un champ obligerait
// tout lecteur a savoir lequel s'applique -- exactement le defaut que la famille
// de prise vient de corriger ailleurs. On verifie donc que `subgraph` reste
// VIDE sur ce noeud : « rien » et « zero » sont deux etats.
//
// 📌 LE NODAL VIENT PAR-DESSUS LE NON NODAL. Le noeud ne CONTIENT pas la
// machine, il la DESIGNE -- et la machine, elle, existe deja sous forme non
// nodale (`NkAnimStateMachine`, livre chez NkAnima). C'est le meme principe qui
// a fait refuser les types composes cote materiau : le graphe ne peut pas
// porter ce vers quoi il compile ne sait pas ecrire.
static void CasMachineAEtatsEstUnNoeud() {
	// ── 1. LE GRAPHE D'ANIMATION, avec la machine comme UN NOEUD parmi ────
	NkNodeGraph g;
	const NkTypeId tPose = g.RegisterType("pose");
	const NkTypeId tTexte = g.RegisterType("texte");
	const NkNodeId msm = g.AddNode("anim.machine_a_etats", "Machine a etats");
	g.AddSocket(msm, "pose", tPose, NkSocketDir::Output, NkSocketFamily::Data);
	const NkNodeId melange = g.AddNode("anim.melange_par_booleen", "Melanger par booleen");
	g.AddSocket(melange, "a", tPose, NkSocketDir::Input, NkSocketFamily::Data);
	g.AddSocket(melange, "resultat", tPose, NkSocketDir::Output, NkSocketFamily::Data);
	const NkNodeId sortie = g.AddNode("anim.sortie_pose", "Sortie de pose");
	g.AddSocket(sortie, "pose", tPose, NkSocketDir::Input, NkSocketFamily::Data);
	const bool cable = g.Connect(msm, "pose", melange, "a") == NkLinkError::Ok &&
					   g.Connect(melange, "resultat", sortie, "pose") == NkLinkError::Ok;

	// ── 2. LA REFERENCE au modele NON NODAL -- une PROPRIETE, rien de neuf ─
	const bool designe = g.SetProp(msm, "machine", NkValueText(tTexte, "Locomotion"));
	const NkGraphValue *ref = g.FindProp(msm, "machine");
	const bool refLue = ref && ref->IsSet() && ref->text == NkString("Locomotion");
	// et `subgraph` reste VIDE : une machine a etats n'est PAS un NkNodeGraph.
	const NkNode *n = g.Find(msm);
	const bool sousGrapheVide = n && n->subgraph.Size() == 0;

	// ── 3. LE GRAPHE EXTERIEUR NE VOIT AUCUN CYCLE ────────────────────────
	// C'est l'argument decisif, mesure : la machine peut contenir
	// Marche -> Saut -> Marche, le coeur n'en sait rien et n'a rien a en savoir.
	NkVector<NkNodeId> ordre;
	const bool acyclique = !g.HasCycle() && g.TopoSort(ordre) && ordre.Size() == 3;

	// ── 4. CONTRE-EPREUVE : LA MEME MACHINE CABLEE DEHORS EST REFUSEE ─────
	// 🔴 SANS CE VOLET, LE CAS NE PROUVERAIT RIEN. « Un graphe sans cycle n'a
	// pas de cycle » est une tautologie ; ce qui a du contenu, c'est que la
	// forme CONCURRENTE -- les etats en noeuds, les transitions en fils --
	// tombe sur `WouldCycle` des la transition de retour. Marche -> Saut passe,
	// Saut -> Marche non. C'est LA raison pour laquelle la machine doit etre un
	// noeud, et non un gout d'architecture.
	NkNodeGraph h;
	const NkTypeId hr = h.RegisterType("reel");
	const NkNodeId marche = PoseNoeudMixte(h, hr, "etat.marche");
	const NkNodeId saut = PoseNoeudMixte(h, hr, "etat.saut");
	const NkLinkError aller = h.Connect(marche, "apres", saut, "avant");
	const NkLinkError retour = h.Connect(saut, "apres", marche, "avant");
	const bool dehorsRefuse = aller == NkLinkError::Ok && retour == NkLinkError::WouldCycle;

	// ── 5. LA REFERENCE SURVIT AU FICHIER, OCTET POUR OCTET ───────────────
	NkString t1;
	g.Serialize(t1);
	NkNodeGraph relu;
	NkString err;
	const bool relit = relu.Deserialize(t1.CStr(), &err);
	NkString t2;
	relu.Serialize(t2);
	const NkGraphValue *refApres = relit ? relu.FindProp(msm, "machine") : nullptr;
	const NkNode *nApres = relit ? relu.Find(msm) : nullptr;
	const bool voyage = relit && t1 == t2 && refApres && refApres->text == NkString("Locomotion") && nApres &&
						nApres->subgraph.Size() == 0;

	NkString d;
	d = NkFormat("le graphe d anim se cable={0} | la machine est DESIGNEE par une propriete={1} relue={2} | "
				 "`subgraph` reste VIDE (ce n est pas un NkNodeGraph)={3} | le graphe EXTERIEUR est acyclique="
				 "{4} ({5} noeuds tries) | CONTRE-EPREUVE : les memes etats cables DEHORS -> aller={6} "
				 "retour={7} donc refuse={8} | aller-retour identique et reference survit={9} ({10} o) {11}",
				 cable ? 1 : 0, designe ? 1 : 0, refLue ? 1 : 0, sousGrapheVide ? 1 : 0, acyclique ? 1 : 0,
				 (uint32)ordre.Size(), NkString(NkLinkErrorName(aller)), NkString(NkLinkErrorName(retour)),
				 dehorsRefuse ? 1 : 0, voyage ? 1 : 0, (uint32)t1.Size(), err);
	Cas("etats/la-machine-a-etats-est-un-noeud",
		cable && designe && refLue && sousGrapheVide && acyclique && dehorsRefuse && voyage, d);
}

static void CasTypesEspaceEtEmpreinte() {
	// ── 1. LE NOM QUALIFIE : sa forme est verifiee, jamais resolue ───────
	const bool formesBonnes = NkNodeGraph::NomQualifieValide("Rihen::Difficulte") &&
							  NkNodeGraph::NomQualifieValide("reel") &&
							  NkNodeGraph::NomQualifieValide("A::B::C::d_1");
	// ⚠️ DISCRIMINE AUSSI SUR CE QUI DOIT ETRE REFUSE : sans ces trois-la, la
	// fonction pourrait rendre « vrai » toujours et le cas serait vert.
	const bool formesMauvaises = !NkNodeGraph::NomQualifieValide("Rihen::") &&
								 !NkNodeGraph::NomQualifieValide("::Difficulte") &&
								 !NkNodeGraph::NomQualifieValide("Rihen::1er") &&
								 !NkNodeGraph::NomQualifieValide("a::b::") && !NkNodeGraph::NomQualifieValide("");
	NkString esp, simple;
	NkNodeGraph::SepareNomQualifie("Rihen::Jeu::Difficulte", &esp, &simple);
	const bool separe = esp == NkString("Rihen::Jeu") && simple == NkString("Difficulte");

	// ── 2. UNE FEUILLE N'A PAS D'EMPREINTE, ELLE N'EN A PAS UNE A ZERO ───
	// Regle 3, a l'etage des types. `TypeFingerprint` rend faux ; un lecteur
	// qui ignorerait le retour lirait une empreinte non initialisee au lieu
	// d'une valeur credible.
	NkNodeGraph g;
	const NkTypeId reel = g.RegisterType("reel");
	uint64 bidon = 0xDEADBEEFu;
	const bool feuilleSansEmpreinte = !g.TypeFingerprint(reel, &bidon) && bidon == 0xDEADBEEFu &&
									  g.TypeKind(reel) == NkTypeKind::Leaf;

	// ── 3. LE CAS `Rihen::Difficulte` ────────────────────────────────────
	NkTypeMember troisA[3];
	troisA[0].name = NkString("Facile");
	troisA[1].name = NkString("Normal");
	troisA[2].name = NkString("Difficile");
	NkTypeMember quatreB[4];
	quatreB[0].name = NkString("Facile");
	quatreB[1].name = NkString("Normal");
	quatreB[2].name = NkString("Difficile");
	quatreB[3].name = NkString("Expert");

	NkString errA;
	const NkTypeId ta = g.RegisterCompositeType("Rihen::Difficulte", NkTypeKind::Enum, troisA, 3, &errA);
	uint64 empA = 0;
	const bool aEnregistre = ta != NK_TYPE_INVALID && g.TypeFingerprint(ta, &empA) && errA.Size() == 0;

	// re-declarer A L'IDENTIQUE est IDEMPOTENT : deux consommateurs ont le
	// droit de declarer le meme type, comme pour `RegisterType`.
	NkString errIdem;
	const NkTypeId taBis = g.RegisterCompositeType("Rihen::Difficulte", NkTypeKind::Enum, troisA, 3, &errIdem);
	const bool idempotent = taBis == ta && errIdem.Size() == 0;

	// 🔴 LE MEME NOM AVEC UN CONTENU DIFFERENT EST REFUSE, ET LE REFUS DIT QUOI
	NkString errB;
	const NkTypeId tb = g.RegisterCompositeType("Rihen::Difficulte", NkTypeKind::Enum, quatreB, 4, &errB);
	const bool refuse = tb == NK_TYPE_INVALID;
	const bool nommeLeType = ContientSansCasse(errB, "Rihen::Difficulte");
	// ⚠️ « LES DEUX NE CORRESPONDENT PAS » NE SUFFIT PAS : le refus doit porter
	// CE QUI differe. Sans ca, il faut ouvrir deux fichiers et comparer a la
	// main, et sur trente enumerateurs personne ne le fait correctement.
	const bool ditCombien = ContientSansCasse(errB, "manque") || ContientSansCasse(errB, "trop");
	const bool nommeLeMembre = ContientSansCasse(errB, "Expert");

	// ── 4. L'ORDRE EST DU SENS, PAS DE LA MISE EN FORME ──────────────────
	// Les valeurs d'une enumeration sont POSITIONNELLES : permuter deux
	// enumerateurs ne renomme pas, ca change ce que valent les donnees deja
	// sauvees. Une empreinte insensible a l'ordre laisserait passer exactement
	// la corruption la plus silencieuse.
	NkTypeMember permute[3];
	permute[0].name = NkString("Facile");
	permute[1].name = NkString("Difficile");
	permute[2].name = NkString("Normal");
	NkNodeGraph h;
	NkString errP;
	h.RegisterCompositeType("Rihen::Difficulte", NkTypeKind::Enum, troisA, 3, nullptr);
	const NkTypeId tp = h.RegisterCompositeType("Rihen::Difficulte", NkTypeKind::Enum, permute, 3, &errP);
	const bool ordreCompte = tp == NK_TYPE_INVALID && ContientSansCasse(errP, "membre");

	// ── 5. LE GENRE FAIT PARTIE DE L'IDENTITE ────────────────────────────
	NkNodeGraph k;
	k.RegisterCompositeType("Rihen::Truc", NkTypeKind::Enum, troisA, 3, nullptr);
	NkString errG;
	const NkTypeId tg = k.RegisterCompositeType("Rihen::Truc", NkTypeKind::Struct, troisA, 3, &errG);
	const bool genreCompte = tg == NK_TYPE_INVALID && ContientSansCasse(errG, "genre");

	NkString d;
	d = NkFormat("noms qualifies : formes bonnes={0} formes mauvaises refusees={1} separation={2} | FEUILLE sans "
				 "empreinte (pas une empreinte nulle)={3} | enum enregistree={4} idempotente={5} | MEME NOM "
				 "CONTENU DIFFERENT refuse={6} en nommant le type={7} le compte={8} le membre en trop={9} | "
				 "l ORDRE compte={10} | le GENRE compte={11} | refus : {12}",
				 formesBonnes ? 1 : 0, formesMauvaises ? 1 : 0, separe ? 1 : 0, feuilleSansEmpreinte ? 1 : 0,
				 aEnregistre ? 1 : 0, idempotent ? 1 : 0, refuse ? 1 : 0, nommeLeType ? 1 : 0, ditCombien ? 1 : 0,
				 nommeLeMembre ? 1 : 0, ordreCompte ? 1 : 0, genreCompte ? 1 : 0, errB);
	Cas("types/espace-de-noms-et-empreinte",
		formesBonnes && formesMauvaises && separe && feuilleSansEmpreinte && aEnregistre && idempotent && refuse &&
			nommeLeType && ditCombien && nommeLeMembre && ordreCompte && genreCompte,
		d);
}

// ── LE TEMOIN SUR FICHIER : DEUX FICHIERS, MEME NOM, CONTENUS DIFFERENTS ────
//
// 🔴 C'EST LA CONDITION EXACTE POSEE PAR RODOLF, ET ELLE PORTE SUR DES FICHIERS,
// pas sur deux appels dans le meme processus. C'est la difference qui compte :
// les graphes sont ecrits SEPAREMENT, sauves, et charges a l'execution par une
// application qui n'a jamais vu l'autre. Le C++ obtient cette garantie de son
// EDITEUR DE LIENS ; un graphe de noeuds n'a aucune etape de liaison, donc il
// faut la construire.
static void CasTypesDeuxFichiersMemeNom() {
	NkTypeMember trois[3];
	trois[0].name = NkString("Facile");
	trois[1].name = NkString("Normal");
	trois[2].name = NkString("Difficile");
	NkTypeMember quatre[4];
	quatre[0].name = NkString("Facile");
	quatre[1].name = NkString("Normal");
	quatre[2].name = NkString("Difficile");
	quatre[3].name = NkString("Expert");

	NkNodeGraph a;
	a.RegisterCompositeType("Rihen::Difficulte", NkTypeKind::Enum, trois, 3, nullptr);
	NkNodeGraph b;
	b.RegisterCompositeType("Rihen::Difficulte", NkTypeKind::Enum, quatre, 4, nullptr);
	NkString fa, fb;
	a.Serialize(fa);
	b.Serialize(fb);

	// L'empreinte VOYAGE : elle doit etre dans le texte, sinon tout le reste
	// est un accord de memoire qui ne survit pas a la sauvegarde.
	const bool empreinteEcrite = ContientLigne(fa, "typec ") && ContientLigne(fa, "typem ");
	// et les deux fichiers ne portent PAS la meme.
	NkNodeGraph ra, rb;
	const bool luA = ra.Deserialize(fa.CStr());
	const bool luB = rb.Deserialize(fb.CStr());
	uint64 ea = 0, eb = 0;
	const NkTypeId ia = ra.FindType("Rihen::Difficulte");
	const NkTypeId ib = rb.FindType("Rihen::Difficulte");
	const bool relues = luA && luB && ia != NK_TYPE_INVALID && ib != NK_TYPE_INVALID &&
						ra.TypeFingerprint(ia, &ea) && rb.TypeFingerprint(ib, &eb);
	const bool empreintesDifferent = relues && ea != eb;
	// ⚠️ ET L'ALLER-RETOUR NE DOIT PAS L'AVOIR CHANGEE : une empreinte qui se
	// recalcule differemment apres relecture refuserait un fichier contre
	// lui-meme.
	uint64 eaAvant = 0;
	a.TypeFingerprint(a.FindType("Rihen::Difficulte"), &eaAvant);
	const bool survitAuxTexte = relues && ea == eaAvant;
	const bool membresRelus = relues && ra.TypeMemberCount(ia) == 3 && rb.TypeMemberCount(ib) == 4 &&
							  rb.TypeMemberAt(ib, 3) && rb.TypeMemberAt(ib, 3)->name == NkString("Expert");

	// 🔴 LA RENCONTRE : le graphe A recoit le type tel que B l'a ecrit.
	// C'est le moment ou l'application charge le second fichier.
	NkString erreur;
	const NkTypeId conflit =
		ra.RegisterCompositeType("Rihen::Difficulte", NkTypeKind::Enum, quatre, 4, &erreur);
	const bool refuseALaRencontre = conflit == NK_TYPE_INVALID;
	const bool refusNomme = ContientSansCasse(erreur, "Rihen::Difficulte") && ContientSansCasse(erreur, "Expert");
	// et le registre de A n'a PAS bouge -- un refus qui laisse une trace serait
	// pire qu'une acceptation, parce qu'il aurait l'air d'avoir echoue.
	const bool aInchange = ra.TypeMemberCount(ia) == 3;

	NkString d;
	d = NkFormat("l empreinte VOYAGE dans le fichier={0} | relues des deux cotes={1} | empreintes DIFFERENTES={2} "
				 "(a={3} b={4}) | survit a l aller-retour={5} | membres relus 3/4 + « Expert »={6} | LA "
				 "RENCONTRE : refusee={7} en se nommant={8} | le registre d arrivee est INCHANGE={9} | {10}",
				 empreinteEcrite ? 1 : 0, relues ? 1 : 0, empreintesDifferent ? 1 : 0, (uint32)(ea & 0xFFFFFFFFu),
				 (uint32)(eb & 0xFFFFFFFFu), survitAuxTexte ? 1 : 0, membresRelus ? 1 : 0,
				 refuseALaRencontre ? 1 : 0, refusNomme ? 1 : 0, aInchange ? 1 : 0, erreur);
	Cas("types/deux-fichiers-meme-nom-refuses-en-se-nommant",
		empreinteEcrite && relues && empreintesDifferent && survitAuxTexte && membresRelus && refuseALaRencontre &&
			refusNomme && aInchange,
		d);
}

static void CasFichierMigrationVersion1() {
	// `def 2 0 1 1 3.5` : noeud 2, prise d'INDEX 0, valeur (type 1, 1 nombre).
	static const char *kV1 = "nkgraph 1\n"
							 "compteurs 3 2\n"
							 "type 1 reel\n"
							 "noeud 1 0.000000 0.000000 essai.source A\n"
							 "sock 1 1 1 sortie_a\n"
							 "sock 1 1 1 sortie_b\n"
							 "noeud 2 0.000000 0.000000 essai.puits B\n"
							 "sock 2 0 1 entree\n"
							 "def 2 0 1 1 3.5\n"
							 "lien 1 1 1 2 0\n";
	NkNodeGraph v1;
	NkString err1;
	const bool lu = v1.Deserialize(kV1, &err1);
	const NkLink *l = v1.LinkCount() > 0 ? v1.LinkAt(0) : nullptr;
	const NkNode *n1 = v1.Find(1);
	// `lien 1 1 1 2 0` : rang 1 sur le noeud 1, donc « sortie_b ».
	const bool indexRespecte = lu && l && n1 && l->fromSocket == 1 && (uint32)n1->sockets.Size() > 1 &&
							   n1->sockets[1].name == NkString("sortie_b");
	const NkGraphValue *dv = lu ? v1.SocketDefault(2, "entree", NkSocketDir::Input) : nullptr;
	const bool defautV1 = dv && dv->IsSet() && dv->numbers.Size() > 0 && dv->numbers[0] == 3.5f;

	// L'ecrivain, lui, ne produit QUE la version courante : la migration est un
	// chemin de LECTURE, jamais un mode d'ecriture qu'on pourrait oublier actif.
	NkString reecrit;
	v1.Serialize(reecrit);
	NkString entete("nkgraph ");
	entete.Append(NkFormat("{0}", (uint32)NK_NKGRAPH_VERSION));
	const bool reecritEnCourant = ContientLigne(reecrit, entete.CStr());

	// ── ET UNE VERSION INCONNUE EST REFUSEE EN SE NOMMANT ────────────────
	NkString futur("nkgraph ");
	futur.Append(NkFormat("{0}", (uint32)NK_NKGRAPH_VERSION + 1));
	futur.Append("\ncompteurs 2 1\n");
	NkNodeGraph vf;
	NkString errF;
	const bool refuseFutur = !vf.Deserialize(futur.CStr(), &errF);
	const bool nommeLaVersion = ContientSansCasse(errF, "version");

	NkString d;
	d = NkFormat("v1 relue par INDEX={0} (prise « {1} ») | defaut v1 par index={2} | reecrite en v{3}={4} | "
				 "version inconnue refusee={5} en la nommant={6} : {7}",
				 indexRespecte ? 1 : 0,
				 (lu && n1 && (uint32)n1->sockets.Size() > 1) ? n1->sockets[1].name : NkString("?"),
				 defautV1 ? 1 : 0, (uint32)NK_NKGRAPH_VERSION, reecritEnCourant ? 1 : 0, refuseFutur ? 1 : 0,
				 nommeLaVersion ? 1 : 0, errF);
	Cas("fichier/migration-version-1",
		indexRespecte && defautV1 && reecritEnCourant && refuseFutur && nommeLaVersion, d);
}

static void CasBindingDeclareDesDeuxCotes() {
	// LE CAS QUI RATTRAPE L'AVEUGLEMENT DU TEMOIN. Il lit le SHADER REEL sur le
	// disque et confronte le binding qu'il y trouve a la constante que le C++
	// utilise pour construire son layout et ecrire ses descripteurs.
	//
	// DISCRIMINE : changer UN SEUL des deux cotes met ce cas au rouge. Ni le
	// compilateur, ni le lieur, ni le temoin d'images ne savent le faire.
	NkString src;
	const bool lu = LireFichier(kCheminLayeredV1, src);
	// ⚠️ Un fichier introuvable doit ECHOUER, jamais etre saute. Un cas qui
	// s'escamote quand il ne trouve pas sa donnee est un vert qui ne prouve
	// rien — et il reste vert le jour ou le fichier disparait vraiment.
	if (!lu) {
		Cas("phase0/binding-declare-des-deux-cotes", false, "shader INTROUVABLE (lancer le banc depuis la racine du worktree)");
		return;
	}
	// La ligne cherchee : « @binding(set=2, binding=9) uniform sampler2D tMask; »
	const int32 posSampler = Apres(src, "uniform sampler2D tMask;");
	int32 bindingLu = -1;
	int32 setLu = -1;
	if (posSampler > 0) {
		// Remonter jusqu'au « @binding( » qui precede immediatement.
		const char *base = src.CStr();
		int32 i = posSampler;
		while (i > 0 && !(base[i] == '@' && base[i + 1] == 'b'))
			--i;
		if (base[i] == '@') {
			const char *p = base + i;
			// set=<n>
			const char *s1 = p;
			while (*s1 && !(s1[0] == 's' && s1[1] == 'e' && s1[2] == 't' && s1[3] == '='))
				++s1;
			if (*s1)
				setLu = atoi(s1 + 4);
			const char *b1 = p;
			// « binding= » APRES le « @binding( » lui-meme : on saute le premier.
			int trouves = 0;
			while (*b1) {
				if (b1[0] == 'b' && b1[1] == 'i' && b1[2] == 'n' && b1[3] == 'd' && b1[4] == 'i' && b1[5] == 'n' &&
					b1[6] == 'g' && b1[7] == '=') {
					++trouves;
					bindingLu = atoi(b1 + 8);
					break;
				}
				++b1;
			}
			(void)trouves;
		}
	}
	const bool accord = (bindingLu == (int32)renderer::NK_MATBIND_LAYER_MASK) && (setLu == 2);
	NkString d;
	d = NkFormat("shader lu ({0} o) | tMask trouve={1} | set={2} binding={3} | constante C++ renderer::NK_MATBIND_LAYER_MASK={4} | accord={5}", (uint32)src.Size(), posSampler > 0 ? 1 : 0, setLu, bindingLu, (uint32)renderer::NK_MATBIND_LAYER_MASK, accord ? 1 : 0);
	Cas("phase0/binding-declare-des-deux-cotes", posSampler > 0 && accord, d);
}

static void CasQuatreCanauxPresents() {
	// DISCRIMINE : le C++ declare QUATRE sources de masque par texture
	// (NK_LAYER_MASK_TEX_R..A = 8..11). Le shader doit traiter les quatre. Un
	// oubli du canal alpha — le plus facile a oublier, c'est le dernier —
	// rendrait 0.0 en silence, donc une couche invisible, donc un bogue qu'on
	// chercherait dans le C++.
	NkString src;
	if (!LireFichier(kCheminLayeredV1, src)) {
		Cas("phase0/quatre-canaux-traites", false, "shader INTROUVABLE");
		return;
	}
	const bool r = Apres(src, "if (source == 8) return clamp(m.r") > 0;
	const bool g = Apres(src, "if (source == 9) return clamp(m.g") > 0;
	const bool b = Apres(src, "if (source == 10) return clamp(m.b") > 0;
	const bool a = Apres(src, "if (source == 11) return clamp(m.a") > 0;
	NkString d;
	d = NkFormat("canaux traites dans le shader : R={0} G={1} B={2} A={3} (les 4 attendus)", r ? 1 : 0, g ? 1 : 0, b ? 1 : 0, a ? 1 : 0);
	Cas("phase0/quatre-canaux-traites", r && g && b && a, d);
}

static void CasMasqueCompileSurLesBackends() {
	// ⚠️ LE CAS QUI COMPTE POUR LA GRILLE D'ACCEPTATION : « le NkSL emis COMPILE
	// sur les backends, pas seulement ressemble au temoin ».
	//
	// Il n'ecrit pas un shader de test : il EXTRAIT le corps reel de PickMask du
	// fichier sur le disque et le compile. Un shader de test recopie a la main
	// derive du vrai en une semaine et valide alors autre chose — c'est le piege
	// « mesurer une reconstruction au lieu de la chose ».
	//
	// Le point risque est precis : un `texture(sampler, uv)` DANS un helper, avec
	// la coordonnee passee en PARAMETRE. Le fichier avertit qu'une varying
	// referencee dans un helper fait sortir un `input.xxx` hors de l'entree au
	// generateur HLSL, et que DX refuse alors le shader (X3004).
	NkString src;
	if (!LireFichier(kCheminLayeredV1, src)) {
		Cas("phase0/masque-compile-4-backends", false, "shader INTROUVABLE");
		return;
	}
	// Extraire de « float PickMask( » jusqu'a la ligne qui vaut exactement « } ».
	const int32 debut = Apres(src, "float PickMask(");
	if (debut < 0) {
		Cas("phase0/masque-compile-4-backends", false, "PickMask introuvable dans le shader");
		return;
	}
	const char *base = src.CStr();
	int32 i = debut;
	while (i > 0 && base[i] != 'f')
		--i; // revenir sur le « float »
	NkString corps;
	int32 k = i;
	while (base[k]) {
		corps.Append(base[k]);
		if (base[k] == '}' && (base[k + 1] == '\n' || base[k + 1] == '\r') && k > 0 && base[k - 1] == '\n')
			break;
		++k;
	}

	// Enveloppe minimale et AUTONOME : les varyings dont le helper a besoin, le
	// sampler, et une entree qui APPELLE le helper — sans appel, un backend
	// pourrait eliminer le code mort et « compiler » sans jamais l'avoir traduit.
	NkString shader;
	shader.Append("@location(0) in vec2 vUV;\n@location(1) in vec4 vColor;\n@location(0) out vec4 fragColor;\n");
	shader.Append("@binding(set=2, binding=9) uniform sampler2D tMask;\n\n");
	shader.Append(corps);
	shader.Append("\n\n@stage(fragment)\n@entry\nvoid main() {\n");
	shader.Append("    float m = PickMask(8, vColor, vUV, 0.5, 1.0);\n");
	shader.Append("    fragColor = vec4(m, m, m, 1.0);\n}\n");

	NkSLCompiler c;
	struct {
			NkSLTarget cible;
			const char *nom;
	} cibles[4] = {
		{NkSLTarget::NK_GLSL, "GL"},
		{NkSLTarget::NK_GLSL_VULKAN, "VK"},
		{NkSLTarget::NK_HLSL_DX11, "DX11"},
		{NkSLTarget::NK_HLSL_DX12, "DX12"},
	};
	uint32 ok = 0;
	NkString detail;
	bool lectureTexturePartout = true;
	for (uint32 t = 0; t < 4; ++t) {
		NkSLCompileResult r = c.Compile(shader, NkSLStage::NK_FRAGMENT, cibles[t].cible);
		// « ca a compile » ne suffit pas : le code produit doit REELLEMENT
		// contenir une lecture de texture. Un backend qui replierait la branche
		// rendrait un succes vide.
		// TROIS choses a verifier, et « ca a compile » n'est que la premiere :
		//   1. le sampler a survecu a la traduction (nom insensible a la casse :
		//      HLSL le rend `tmask_tex`) ;
		//   2. il est REELLEMENT ECHANTILLONNE — `texture(` en GLSL, `.Sample(`
		//      en HLSL. Sans cet appel, un backend qui aurait replie la branche
		//      rendrait un succes vide, et le sampler declare ne prouverait rien ;
		//   3. cote HLSL seulement : le sampler atterrit sur `register(t9)`. C'est
		//      une VERIFICATION CROISEE GRATUITE du meme nombre que le C++ ecrit
		//      dans son layout — trouvee en lisant le code genere, pas prevue.
		const bool declare = r.success && ContientSansCasse(r.source, "tmask");
		const bool echantillonne =
			r.success && (ContientSansCasse(r.source, "texture(") || ContientSansCasse(r.source, ".sample("));
		const bool hlsl = (cibles[t].cible == NkSLTarget::NK_HLSL_DX11 || cibles[t].cible == NkSLTarget::NK_HLSL_DX12);
			// PAS de parenthese fermante : DX11 ecrit « register(t9) » et DX12
		// « register(t9, space0) ». Fermer la parenthese ferait echouer DX12
		// pour une raison qui n a rien a voir avec le masque — mesure le 22/08.
		const NkString reg = NkFormat("register(t{0}", (uint32)renderer::NK_MATBIND_LAYER_MASK);
		const bool bonRegistre = !hlsl || (r.success && ContientSansCasse(r.source, reg.CStr()));
		const bool lit = declare && echantillonne && bonRegistre;
		if (r.success && !lit && getenv("NK_DUMP")) {
			// Diagnostic PERMANENT, pas un echafaudage : quand ce cas tombe, la
			// question est toujours « le backend a-t-il replie la branche, ou ai-je
			// cherche le mauvais nom ? ». Sans le code sous les yeux, on tranche au
			// hasard. NK_DUMP=1 le montre.
			logger.Info("\n--- {0} : compile mais le masque n a pas traverse ---\n{1}\n--- fin ---", cibles[t].nom, r.source.CStr());
		}
		if (r.success)
			++ok;
		if (!lit)
			lectureTexturePartout = false;
		detail.Append(NkFormat("{0}={1}{2}{3}{4} ", NkString(cibles[t].nom),
							   NkString(r.success ? "ok" : "ECHEC"), NkString(declare ? "" : "(pas declare!)"),
							   NkString(echantillonne ? "" : "(pas echantillonne!)"),
							   NkString(bonRegistre ? "" : "(mauvais registre!)")));
	}
	// Cinquieme verdict : le seul rendu par un vrai compilateur (glslang, via
	// NK_SPIRV). Lu au MOT MAGIQUE, jamais a `success` -- voir la note longue
	// dans CompileSurLesBackends : quand glslang refuse, success reste a 1.
	{
		NkSLCompileResult sp = c.Compile(shader, NkSLStage::NK_FRAGMENT, NkSLTarget::NK_SPIRV);
		bool vrai = false;
		if (sp.bytecode.Size() >= 4) {
			const uint8 *o = sp.bytecode.Data();
			vrai = (o[0] == 0x03 && o[1] == 0x02 && o[2] == 0x23 && o[3] == 0x07);
		}
		if (vrai)
			++ok;
		detail.Append(NkFormat("GLSLANG={0} ", NkString(vrai ? "ok" : "ECHEC")));
	}
	Cas("phase0/masque-gen-4-backends-plus-glslang", ok == 5 && lectureTexturePartout && corps.Size() > 100, detail);
}

// ── le compilateur : graphe -> NkSL -> quatre backends ───────────────────────

// Compile un shader NkSL sur les quatre backends et rend combien passent. Le
// vrai NkSLCompiler, sans device : c'est ce que NkSLCheck prouve deja faisable.
static uint32 CompileSurLesBackends(const NkString &nksl, NkString &detail, NkString *premiereErreur) {
	NkSLCompiler c;
	struct {
			NkSLTarget cible;
			const char *nom;
	} cibles[4] = {
		{NkSLTarget::NK_GLSL, "GL"},
		{NkSLTarget::NK_GLSL_VULKAN, "VK"},
		{NkSLTarget::NK_HLSL_DX11, "DX11"},
		{NkSLTarget::NK_HLSL_DX12, "DX12"},
	};
	uint32 ok = 0;
	detail = NkString("");
	for (uint32 t = 0; t < 4; ++t) {
		NkSLCompileResult r = c.Compile(nksl, NkSLStage::NK_FRAGMENT, cibles[t].cible);
		if (r.success)
			++ok;
		else if (premiereErreur && premiereErreur->Size() == 0 && r.errors.Size() > 0)
			*premiereErreur = r.errors[0].message;
		// ⚠️ « ok » ici ne veut PAS dire « compile ». Mesure du 22/08 :
		// NkSLCompiler::Compile ne consulte AUCUN compilateur natif pour ces
		// quatre cibles -- il appelle le GENERATEUR NkSL et rend son verdict.
		// L'analyse semantique existe mais est DELIBEREMENT non bloquante
		// (NkSLCompiler.cpp:290) : fonction jamais declaree, variable inconnue,
		// mismatch de type remontent en AVERTISSEMENTS. Un shader appelant
		// « NkFonctionQuiNExistePas » rend GL=ok VK=ok DX11=ok DX12=ok.
		// Ces quatre colonnes attestent la GENERATION, jamais la COMPILATION.
		detail.Append(NkFormat("{0}={1} ", NkString(cibles[t].nom), NkString(r.success ? "gen" : "ECHEC")));
	}

	// ── Le CINQUIEME verdict, et le seul rendu par un vrai compilateur ────────
	// La cible NK_SPIRV genere le GLSL Vulkan puis le passe a glslang EMBARQUE
	// (NkGLSLToSPIRV). glslang, lui, REFUSE une fonction non declaree.
	//
	// 🔴 MAIS ON NE PEUT PAS LIRE SON VERDICT DANS `success`. Quand glslang
	// refuse, NkSLCompiler.cpp:325 rattrape l'echec, REND LE TEXTE GLSL a la
	// place du bytecode et LAISSE success=1. Le seul bit qui distingue les deux
	// cas est le MOT MAGIQUE : du vrai SPIR-V commence par 0x07230203 ; le texte
	// rendu par defaut commence par « #ver ». Mesure du 22/08, les deux sens :
	//   shader invalide -> success=1, octets=186, magie = 35 118 101 114 (#ver)
	//   shader valide   -> success=1, octets=560, magie = 3 2 35 7 (SPIR-V)
	//
	// NE REMPLACE PAS CE TEST PAR `r.success` EN LE CROYANT EQUIVALENT.
	// C'est exactement l'erreur qui a fait passer pour « compile sur 4 backends »
	// une chaine qui n'en compilait aucun.
	{
		NkSLCompileResult sp = c.Compile(nksl, NkSLStage::NK_FRAGMENT, NkSLTarget::NK_SPIRV);
		bool vrai = false;
		if (sp.bytecode.Size() >= 4) {
			const uint8 *o = sp.bytecode.Data();
			vrai = (o[0] == 0x03 && o[1] == 0x02 && o[2] == 0x23 && o[3] == 0x07);
		}
		if (vrai)
			++ok;
		detail.Append(NkFormat("GLSLANG={0} ", NkString(vrai ? "ok" : "ECHEC")));
	}
	return ok;
}

// Le graphe minimal REEL : un Principled avec ses defauts, vers la sortie.
static NkNodeId MonteUnPrincipled(NkNodeGraph &g, const NkMatTypes &t, NkNodeId *outSortie) {
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	g.Connect(bsdf, "bsdf", out, "surface");
	const float32 rouge[3] = {0.8f, 0.15f, 0.1f};
	g.SetSocketDefault(bsdf, "base_color", NkSocketDir::Input, NkValueVec(t.color, rouge, 3));
	g.SetSocketDefault(bsdf, "metallic", NkSocketDir::Input, NkValueReal(t.real, 0.0f));
	g.SetSocketDefault(bsdf, "roughness", NkSocketDir::Input, NkValueReal(t.real, 0.35f));
	if (outSortie)
		*outSortie = out;
	return bsdf;
}

static void CasCompilePrincipled() {
	// ⚠️ LE CAS CENTRAL DE LA GRILLE D'ACCEPTATION : « le NkSL emis COMPILE sur
	// les backends, pas seulement ressemble au temoin ».
	//
	// DISCRIMINE : on ne compare pas a un texte de reference. On donne le NkSL au
	// VRAI compilateur, quatre fois. Un emetteur qui produirait du texte
	// plausible mais invalide passerait n'importe quelle comparaison de chaines
	// et echouerait ici.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	MonteUnPrincipled(g, t, nullptr);
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	NkString be;
	NkString err;
	const uint32 ok = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;
	// Et la valeur du defaut doit se retrouver DANS le code emis : un compilateur
	// qui oublierait de lire les defauts produirait un shader qui compile
	// parfaitement et rendrait du noir.
	const bool porteLeRouge = r.ok && ContientSansCasse(r.source, "0.800000012");
	NkString d;
	d = NkFormat("emis={0} ({1} o) | {2}| defaut de base_color present={3} {4}", r.ok ? 1 : 0, (uint32)r.source.Size(), be, porteLeRouge ? 1 : 0, r.ok ? "" : (r.error.Size() ? r.error.CStr() : ""));
	Cas("compile/principled-4-backends", r.ok && ok == 5 && porteLeRouge, d);
	if (ok != 4 && err.Size() > 0)
		logger.Info("      premiere erreur du backend : {0}", err.CStr());
}

static void CasCompileMixShader() {
	// LE NOEUD PAR LEQUEL TOUT A COMMENCE — « mixer les shader BSDF ». Deux BSDF
	// melanges vers une sortie, et le resultat doit traverser les quatre backends.
	//
	// DISCRIMINE : on verifie qu'il y a QUATRE `mix(` dans le code emis, un par
	// composante. Un melangeur qui n'en emettrait qu'un — la couleur, la plus
	// visible — donnerait un shader qui compile et perdrait la rugosite et le
	// metallique du second BSDF. C'est le genre de defaut qu'on ne voit pas sur
	// une sphere mate.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId mix = NkMatAddNode(gReg, g, NK_MN_MIX_SHADER);
	const NkNodeId diff = NkMatAddNode(gReg, g, NK_MN_DIFFUSE);
	const NkNodeId emis = NkMatAddNode(gReg, g, NK_MN_EMISSION);
	g.Connect(diff, "bsdf", mix, "shader1");
	g.Connect(emis, "emission", mix, "shader2");
	g.Connect(mix, "shader", out, "surface");
	const float32 bleu[3] = {0.1f, 0.3f, 0.9f};
	g.SetSocketDefault(diff, "color", NkSocketDir::Input, NkValueVec(t.color, bleu, 3));
	g.SetSocketDefault(diff, "roughness", NkSocketDir::Input, NkValueReal(t.real, 0.6f));
	const float32 chaud[3] = {1.0f, 0.6f, 0.2f};
	g.SetSocketDefault(emis, "color", NkSocketDir::Input, NkValueVec(t.color, chaud, 3));
	g.SetSocketDefault(emis, "strength", NkSocketDir::Input, NkValueReal(t.real, 2.0f));
	g.SetSocketDefault(mix, "fac", NkSocketDir::Input, NkValueReal(t.real, 0.5f));

	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	// COMPTER LES « mix( » DU FICHIER ENTIER NE MARCHE PAS, et je l'ai appris en
	// le mesurant : le puits en emet DEUX pour son propre compte (`specExp` et
	// `specColor`). Ma premiere version attendait 5 et en a trouve 6 — la
	// prediction etait fausse, pas le code. Un controle qui vise « tous les mix
	// du shader » mesure le modele d'eclairage en meme temps que le melangeur,
	// et cassera au prochain reglage de l'ombrage sans que rien n'ait bouge.
	//
	// On compte donc les occurrences de la LOCALE DU FACTEUR de CE noeud :
	// 1 declaration + 4 usages, un par composante. Un melangeur qui n'emettrait
	// que la couleur — la plus visible — en donnerait 2, et perdrait en silence
	// la rugosite et le metallique du second BSDF.
	const NkString nomFac = NkFormat("n{0}_fac", (uint32)mix);
	uint32 nbFac = 0;
	if (r.ok) {
		const char *p = r.source.CStr();
		const size_t L = (size_t)nomFac.Size();
		while (*p) {
			size_t k = 0;
			while (k < L && p[k] == nomFac.CStr()[k])
				++k;
			if (k == L)
				++nbFac;
			++p;
		}
	}
	NkString be;
	NkString err;
	const uint32 ok = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;
	// Le puits emet lui aussi un `mix` (specColor) : on en attend donc 4 + 1.
	NkString d;
	d = NkFormat("emis={0} ({1} o) | {2}| {3} cite {4} fois (1 declaration + {5} composantes) {6}", r.ok ? 1 : 0, (uint32)r.source.Size(), be, nomFac, nbFac, NkMatComposanteCount(), r.ok ? NkString("") : r.error);
	Cas("compile/melange-deux-bsdf-4-backends", r.ok && ok == 5 && nbFac == NkMatComposanteCount() + 1u, d);
	// NK_DUMP=1 imprime le NkSL engendre. Ce n est pas un echafaudage oublie :
	// quand un cas de compilation tombe, la question est toujours « qu a-t-il
	// donc ecrit ? », et un banc qui ne sait pas le montrer oblige a rajouter
	// un printf puis a le retirer, a chaque fois.
	if (getenv("NK_DUMP") && r.ok)
		logger.Info("{0}", r.source.CStr());
	if (ok != 4 && err.Size() > 0)
		logger.Info("      premiere erreur du backend : {0}", err.CStr());
}

static void CasCompileOrdreRespecte() {
	// DISCRIMINE : une locale doit etre DECLAREE avant d'etre lue. C'est la seule
	// chose que l'ordre topologique garantit, et c'est exactement ce qu'un
	// emetteur qui parcourrait le graphe dans l'ordre d'insertion casserait.
	//
	// Les noeuds sont crees dans l'ordre INVERSE de leur dependance : la sortie
	// d'abord, les sources ensuite. Un emetteur naif ecrirait donc le puits en
	// premier, lirait `n2_albedo` avant sa declaration, et AUCUN backend ne
	// l'accepterait. C'est le compilateur NkSL qui tranche, pas une inspection
	// de texte.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	NkNodeId out = NK_NODE_INVALID;
	const NkNodeId bsdf = MonteUnPrincipled(g, t, &out);
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	// La declaration du bsdf doit apparaitre AVANT sa lecture par le puits.
	const NkString nomAlbedo = NkFormat("n{0}_albedo", (uint32)bsdf);
	const int32 premiere = r.ok ? Apres(r.source, nomAlbedo.CStr()) : -1;
	const int32 lecture = r.ok ? Apres(r.source, "surfAlbedo = ") : -1;
	NkString be;
	NkString err;
	const uint32 ok = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;
	NkString d;
	d = NkFormat("declaration de {0} a {1}, lecture du puits a {2} (declaration AVANT attendue) | {3}", nomAlbedo, premiere, lecture, be);
	Cas("compile/ordre-topologique-respecte", r.ok && ok == 5 && premiere > 0 && lecture > premiere, d);
}

static void CasCompileRefuseAvantDeGenerer() {
	// ⚠️ CE QUI SE PASSE QUAND LE GRAPHE EST MAUVAIS, et c'est la moitie du
	// travail d'un compilateur. Trois graphes fautifs, trois messages DISTINCTS,
	// et AUCUN shader emis. Un compilateur qui generait quand meme laisserait le
	// backend accuser une ligne de shader au lieu du noeud coupable.
	//
	// DISCRIMINE : les trois messages doivent DIFFERER. Un compilateur qui
	// rendrait « graphe invalide » pour tout passerait un test qui ne compterait
	// que les echecs.
	// CE CAS A SURVECU A SA PROPRE MUTATION, et la faute etait dans le cas. Sa
	// premiere version notait le message « seulement si !r.ok », puis ajoutait un
	// garde defensif : « si un shader a quand meme ete emis, message = SHADER
	// PARTIEL EMIS ». Sous la mutation « un noeud inconnu est SAUTE au lieu
	// d'etre refuse », la compilation reussissait, le garde defensif remplissait
	// le troisieme message, les trois restaient distincts, et le cas passait au
	// vert. **Le garde cense renforcer le cas est ce qui l'a rendu aveugle.**
	// On exige donc l'ECHEC lui-meme, pas la seule presence d'un message.
	NkString m1, m2, m3;
	bool e1 = false, e2 = false, e3 = false;
	bool rienEmis3 = false;
	{ // aucune sortie
		NkNodeGraph g;
		NkMatRegisterTypes(g);
		NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
		NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
		e1 = !r.ok;
		m1 = r.error;
	}
	{ // sortie non reliee
		NkNodeGraph g;
		NkMatRegisterTypes(g);
		NkMatAddNode(gReg, g, NK_MN_OUTPUT);
		NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
		NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
		e2 = !r.ok;
		m2 = r.error;
	}
	{ // un noeud dont le compilateur ne sait rien faire : il existe dans le
	  // coeur, il porte des prises valides, et pourtant il n'a pas d'emetteur.
		NkNodeGraph g;
		const NkMatTypes t = NkMatRegisterTypes(g);
		const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
		const NkNodeId inconnu = g.AddNode("mat.noeud_futur", "Noeud pas encore compilable");
		g.AddSocket(inconnu, "bsdf", t.shader, NkSocketDir::Output);
		g.Connect(inconnu, "bsdf", out, "surface");
		NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
		e3 = !r.ok;
		m3 = r.error;
		// Et RIEN ne doit avoir ete emis : un shader partiel serait pire qu'aucun.
		rienEmis3 = (r.source.Size() == 0);
	}
	const bool troisDistincts = e1 && e2 && e3 && rienEmis3 && m1.Size() > 0 && m2.Size() > 0 && m3.Size() > 0 &&
								!(m1 == m2) && !(m2 == m3) && !(m1 == m3);
	NkString d;
	d = NkFormat("[{0}] [{1}] [{2}] | trois ECHECS={3}{4}{5} rien emis={6} distincts={7}", m1.CStr(), m2.CStr(), m3.CStr(), e1 ? 1 : 0, e2 ? 1 : 0, e3 ? 1 : 0, rienEmis3 ? 1 : 0, troisDistincts ? 1 : 0);
	Cas("compile/refuse-avant-de-generer", troisDistincts, d);
}

static void CasCompileEntreeNiCableeNiRenseignee() {
	// LE CHOIX QU'IL FAUT DIRE. Une entree ni cablee ni renseignee : que met le
	// compilateur ? Le blanc ferait passer un materiau non fini pour un materiau
	// clair. On rend le NOIR, qui se VOIT — meme logique que le repli d'un
	// shader manquant.
	//
	// DISCRIMINE : on verifie que ca compile ET que le code contient bien le
	// neutre. Un compilateur qui laisserait l'expression VIDE produirait
	// « vec3 n1_albedo = ; », ce qu'aucun backend n'accepte — donc le seul fait
	// de compiler prouve deja qu'une decision a ete prise. Le reste verifie
	// LAQUELLE.
	NkNodeGraph g;
	NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED); // AUCUN defaut pose
	g.Connect(bsdf, "bsdf", out, "surface");
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	NkString be;
	NkString err;
	const uint32 ok = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;
	const bool neutreNoir = r.ok && ContientSansCasse(r.source, "vec3(0.0)");
	NkString d;
	d = NkFormat("emis={0} | {1}| neutre noir present={2} (le blanc flatterait un materiau non fini)", r.ok ? 1 : 0, be, neutreNoir ? 1 : 0);
	Cas("compile/entree-vide-donne-le-neutre", r.ok && ok == 5 && neutreNoir, d);
}

static void CasCompileGrapheVenuDunFichier() {
	// LE CHEMIN COMPLET, celui qui compte en production : on construit, on
	// SERIALISE, on RELIT, et on compile le graphe RELU. Les deux shaders
	// doivent etre identiques au caractere pres.
	//
	// DISCRIMINE : c'est le seul cas qui prouve que les defauts de prise
	// traversent le fichier POUR DE VRAI jusqu'au shader. Un aller-retour qui
	// perdrait les defauts rendrait un shader different — tout noir, et qui
	// compile parfaitement.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	MonteUnPrincipled(g, t, nullptr);
	NkMatCompileResult direct = NkMatCompileToNkSL(gReg, g);

	NkString fichier;
	g.Serialize(fichier);
	NkNodeGraph g2;
	const bool relu = g2.Deserialize(fichier.CStr());
	NkMatCompileResult apres = NkMatCompileToNkSL(gReg, g2);

	const bool memeShader = direct.ok && apres.ok && (direct.source == apres.source);
	NkString be;
	NkString err;
	const uint32 ok = apres.ok ? CompileSurLesBackends(apres.source, be, &err) : 0u;
	NkString d;
	d = NkFormat("relu={0} | shader identique apres aller-retour={1} ({2} vs {3} o) | {4}", relu ? 1 : 0, memeShader ? 1 : 0, (uint32)direct.source.Size(), (uint32)apres.source.Size(), be);
	Cas("compile/graphe-venu-d-un-fichier", relu && memeShader && ok == 5, d);
}

// ── la bibliotheque interrogee PAR TYPE DE PRISE ─────────────────────────────
// La demande de Rodolf du 2026-08-22 : dans Blender, chaque parametre porte un
// point, et le menu qui s'ouvre est FILTRE PAR LE TYPE DE LA PRISE. Le menu de
// `Base Color` et celui de `Roughness` n'ont pas le meme contenu.

// Cherche un prototype par sa cle dans un resultat de requete.
static bool DansLeMenu(const NkMatNodeProto **menu, uint32 n, const char *cle) {
	for (uint32 i = 0; i < n; ++i) {
		const char *a = menu[i]->key;
		const char *b = cle;
		while (*a && *a == *b) {
			++a;
			++b;
		}
		if (!*a && !*b)
			return true;
	}
	return false;
}

static void CasMenuPriseShader() {
	// DISCRIMINE par ce qui NE DOIT PAS y etre. Une requete qui rendrait tous
	// les prototypes passerait un test qui se contenterait de compter « au moins
	// un ». `Material Output` n'a AUCUNE sortie : rien ne peut en sortir, il ne
	// peut donc jamais etre propose. Et `Value`/`RGB` produisent un reel et une
	// couleur, pas un shader.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	// Dimensionne sur le NOMBRE DE PROTOTYPES, pas sur un nombre choisi :
	// le menu grandit a chaque noeud ajoute, et un tampon fixe finit
	// toujours par etre depasse. Il l'a ete a 17.
	const NkMatNodeProto *menu[64];
	const uint32 n = NkMatNoeudsPourPrise(gReg, g, t.shader, menu, 64);
	const bool bons = DansLeMenu(menu, n, NK_MN_PRINCIPLED) && DansLeMenu(menu, n, NK_MN_DIFFUSE) &&
					  DansLeMenu(menu, n, NK_MN_EMISSION) && DansLeMenu(menu, n, NK_MN_MIX_SHADER);
	const bool absents = !DansLeMenu(menu, n, NK_MN_OUTPUT) && !DansLeMenu(menu, n, NK_MN_VALUE) &&
						 !DansLeMenu(menu, n, NK_MN_RGB);
	Cas("biblio/menu-prise-shader", n == 4 && bons && absents,
		NkFormat("{0} propositions (4 attendues) | les 4 BSDF presents={1} | sortie/valeur/rgb absents={2}", n,
				 bons ? 1 : 0, absents ? 1 : 0));
}

static void CasMenuPriseCouleurEtReelle() {
	// LE CAS QUI TRANCHE, et il tient dans une asymetrie. Les conversions sont
	// DIRIGEES : reel -> couleur est declaree, couleur -> reel ne l'est pas
	// (luminance ? moyenne ? canal rouge ? trois reponses plausibles, donc
	// aucune par defaut). Le menu doit donc etre ASYMETRIQUE :
	//     prise COULEUR : `RGB` **et** `Value` (le reel se diffuse en gris)
	//     prise REELLE  : `Value` seul -- `RGB` doit en etre ABSENT
	// Une table de conversions symetrique par erreur mettrait `RGB` dans les
	// deux, et aucun test a sens unique ne le verrait.
	NkNodeGraph g;
	NkMatRegisterTypes(g);
	const NkMatNodeProto *mc[64], *mr[64];
	const uint32 nc = NkMatNoeudsPourPriseDe(gReg, g, NK_MN_PRINCIPLED, "base_color", mc, 64);
	const uint32 nr = NkMatNoeudsPourPriseDe(gReg, g, NK_MN_PRINCIPLED, "roughness", mr, 64);
	// Ce qui produit une COULEUR ou peut s'y convertir : RGB, Mix Color, et
	// Value (un reel se diffuse en gris). Math aussi, pour la meme raison.
	const bool couleurOk = DansLeMenu(mc, nc, NK_MN_RGB) && DansLeMenu(mc, nc, NK_MN_VALUE) &&
						   DansLeMenu(mc, nc, NK_MN_MIX_COLOR) && DansLeMenu(mc, nc, NK_MN_MATH) &&
						   DansLeMenu(mc, nc, NK_MN_COLOR_RAMP) &&
						   DansLeMenu(mc, nc, NK_MN_IMAGE_TEXTURE) &&
						   DansLeMenu(mc, nc, NK_MN_NORMAL_MAP) && DansLeMenu(mc, nc, NK_MN_BUMP) &&
						   DansLeMenu(mc, nc, NK_MN_NOISE) && DansLeMenu(mc, nc, NK_MN_CHECKER) &&
						   DansLeMenu(mc, nc, NK_MN_VORONOI) && DansLeMenu(mc, nc, NK_MN_BRICK);
	// Ce qui produit un REEL : Value et Math. Ni RGB ni Mix Color, parce que
	// `couleur -> reel` n'est PAS declaree — et c'est la tout le cas.
	const bool reelOk = DansLeMenu(mr, nr, NK_MN_VALUE) && DansLeMenu(mr, nr, NK_MN_MATH) &&
						!DansLeMenu(mr, nr, NK_MN_RGB) && !DansLeMenu(mr, nr, NK_MN_MIX_COLOR) &&
						!DansLeMenu(mr, nr, NK_MN_COLOR_RAMP) &&
						// `Image Texture` a une sortie `alpha` REELLE : il est donc
						// legitimement dans les DEUX menus, par deux prises differentes.
						DansLeMenu(mr, nr, NK_MN_IMAGE_TEXTURE) &&
						// Separate XYZ sort TROIS reels : il est dans le menu reel,
						// et pas dans le menu couleur (reel->couleur est declaree,
						// donc il y est aussi -- ce qui est correct).
						DansLeMenu(mr, nr, NK_MN_SEPARATE_XYZ) &&
						// Les proceduraux ont une sortie `fac` REELLE : ils sont dans
						// les deux menus, par deux prises differentes.
						DansLeMenu(mr, nr, NK_MN_NOISE) && DansLeMenu(mr, nr, NK_MN_GRADIENT) &&
						DansLeMenu(mr, nr, NK_MN_WAVE) && DansLeMenu(mr, nr, NK_MN_VORONOI);
	// ⚠️ PAS DE COMPTE FIGE ICI, ET C EST UNE CORRECTION.
	//
	// Ce cas exigeait `nc == 17 && nr == 10`. Il est tombe le jour ou le rang 3
	// a ajoute quatre noeuds -- alors que RIEN de ce qu il mesure n avait bouge.
	// Un controle qui mesure une collection QUI GRANDIT PAR CONCEPTION ne doit
	// pas porter sa taille : il oblige alors a mettre un nombre a jour a chaque
	// ajout, et ce geste mecanique finit par se faire sans regarder -- on
	// recopie le nouveau chiffre, y compris le jour ou il est faux.
	//
	// La RELATION, elle, survit : le menu reel est STRICTEMENT INCLUS dans le
	// menu couleur. Tout ce qui produit un reel se convertit en couleur (la
	// conversion est declaree), l inverse est faux -- et c est tout le sujet du
	// cas. Elle grandit toute seule avec la bibliotheque.
	bool inclusion = nr > 0 && nc > nr;
	for (uint32 i = 0; i < nr && inclusion; ++i)
		inclusion = DansLeMenu(mc, nc, mr[i]->key);
	Cas("biblio/menu-asymetrique-couleur-reel", inclusion && couleurOk && reelOk,
		NkFormat("base_color : {0} propositions (ok={1}) | roughness : {2} (Value+Math, RGB et MixColor "
				 "ABSENTS, ok={3}) | menu reel strictement inclus dans le menu couleur={4} (relation, "
				 "pas un compte)",
				 nc, couleurOk ? 1 : 0, nr, reelOk ? 1 : 0, inclusion ? 1 : 0));
}

static void CasMenuPriseInconnue() {
	// Une prise qui n'existe pas rend un menu VIDE, jamais le menu complet. Une
	// requete qui retomberait sur « tout » ferait proposer n'importe quoi sur
	// une prise mal orthographiee.
	NkNodeGraph g;
	NkMatRegisterTypes(g);
	const uint32 n = NkMatNoeudsPourPriseDe(gReg, g, NK_MN_PRINCIPLED, "prise_qui_nexiste_pas");
	const uint32 m = NkMatNoeudsPourPriseDe(gReg, g, "mat.noeud_inconnu", "base_color");
	Cas("biblio/menu-prise-ou-noeud-inconnu", n == 0 && m == 0,
		NkFormat("prise inconnue -> {0} | noeud inconnu -> {1} (0 et 0 attendus)", n, m));
}

static void CasPriseConstanteSeulement() {
	// ⚠️ CE QUE CE CAS COUVRE, ET CE QU'IL NE COUVRE PAS.
	// Certains parametres alimentent l'ETAT DU PIPELINE (mode de melange, mode
	// d'ombre) et ne peuvent pas varier par pixel a moindre cout : leur prise
	// doit pouvoir se declarer « constante seulement », et l'interface ne doit
	// alors PAS afficher de point de connexion — un menu vide laisserait croire
	// a une panne.
	//
	// AUCUN des sept prototypes actuels n'est dans ce cas, et je ne vais pas en
	// inventer un pour faire vert. On teste donc le MECANISME sur une
	// declaration construite ici : le drapeau est lu, et il decide. Le jour ou
	// un vrai parametre d'etat arrive, il n'y aura qu'a poser le booleen.
	NkMatSocketDecl libre = {"exemple", NK_MT_REAL, NkSocketDir::Input, false};
	NkMatSocketDecl figee = {"exemple", NK_MT_REAL, NkSocketDir::Input, true};
	const bool ok = NkMatPriseAccepteUnLien(libre) && !NkMatPriseAccepteUnLien(figee);
	Cas("biblio/prise-constante-seulement", ok,
		NkFormat("prise libre accepte un lien={0} | prise figee refuse={1} | usage reel dans la bibliotheque : AUCUN "
				 "a ce jour, et c'est dit",
				 NkMatPriseAccepteUnLien(libre) ? 1 : 0, NkMatPriseAccepteUnLien(figee) ? 0 : 1));
}

static void CasCompileRGBVersBaseColor() {
	// LE PRINCIPE DE RODOLF, COMPILE : « un champ de couleur peut recevoir une
	// texture ou un procedural ». Ici la source est un noeud `RGB`, dont la
	// valeur vit dans une PROPRIETE DE NOEUD — les proprietes livrees dans le
	// coeur cette nuit traversent donc jusqu'au shader pour la premiere fois.
	//
	// DISCRIMINE : la valeur de la propriete doit se retrouver dans le code
	// emis. Un compilateur qui declarerait la locale sans lire la propriete
	// produirait un shader qui compile parfaitement et rendrait du noir.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	const NkNodeId rgb = NkMatAddNode(gReg, g, NK_MN_RGB);
	g.Connect(bsdf, "bsdf", out, "surface");
	const NkLinkError e = g.Connect(rgb, "color", bsdf, "base_color");
	const float32 turquoise[3] = {0.04f, 0.33f, 0.37f};
	g.SetProp(rgb, NK_MPROP_COLOR, NkValueVec(t.color, turquoise, 3));

	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	NkString be;
	NkString err;
	const uint32 ok = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;
	const bool porteLaValeur = r.ok && ContientSansCasse(r.source, "0.330000013");
	// Et le Principled doit LIRE la locale du RGB, pas un litteral : c'est la
	// preuve que le lien a ete suivi et non que la valeur a ete recopiee.
	// La locale porte le nom de la PRISE source : `RGB` sort sur « color ».
	const NkString nomVal = NkFormat("n{0}_color", (uint32)rgb);
	const bool lien = r.ok && Apres(r.source, nomVal.CStr()) > 0;
	Cas("compile/rgb-vers-base-color", e == NkLinkError::Ok && r.ok && ok == 5 && porteLaValeur && lien,
		NkFormat("lien={0} | {1}| valeur de la propriete presente={2} | le BSDF lit {3}={4}",
				 NkString(NkLinkErrorName(e)), be, porteLaValeur ? 1 : 0, nomVal, lien ? 1 : 0));
}

static void CasCompileValeurVersRoughness() {
	// Le pendant reel : un noeud `Value` alimente `roughness`. Il exerce la
	// SECONDE porte des valeurs -- une propriete SCALAIRE -- et le fait que le
	// compilateur choisisse `float` et non `vec3` pour ce type de sortie.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	const NkNodeId val = NkMatAddNode(gReg, g, NK_MN_VALUE);
	g.Connect(bsdf, "bsdf", out, "surface");
	const NkLinkError e = g.Connect(val, "value", bsdf, "roughness");
	g.SetProp(val, NK_MPROP_VALUE, NkValueReal(t.real, 0.125f));
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	NkString be;
	NkString err;
	const uint32 ok = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;
	// `Value` sort sur « value ».
	const NkString decl = NkFormat("float n{0}_value = 0.125", (uint32)val);
	const bool bonType = r.ok && Apres(r.source, decl.CStr()) > 0;
	Cas("compile/valeur-vers-roughness", e == NkLinkError::Ok && r.ok && ok == 5 && bonType,
		NkFormat("lien={0} | {1}| declaration '{2}' presente={3}", NkString(NkLinkErrorName(e)), be, decl,
				 bonType ? 1 : 0));
}


// ── Math et Mix Color : une propriete qui porte une DECISION ─────────────────

static void CasOperationInconnueRefusee() {
	// ⚠️ LE CAS LE PLUS IMPORTANT DE LA SERIE. Une operation que le compilateur
	// ne connait pas doit FAIRE ECHOUER la compilation, jamais retomber sur la
	// premiere de la liste. Un repli sur « ajouter » produirait un materiau qui
	// compile, qui rend, et qui calcule AUTRE CHOSE que ce que le fichier dit —
	// un fichier ecrit par une version future, ou une simple faute de frappe,
	// passerait inapercu jusqu'au resultat.
	//
	// DISCRIMINE aussi le cas VOISIN et legitime : une propriete ABSENTE (le
	// noeud vient d'etre pose, l'auteur n'a pas choisi) doit, elle, compiler.
	// Un controle qui refuserait les deux serait aussi faux qu'un qui accepte
	// les deux.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	const NkNodeId math = NkMatAddNode(gReg, g, NK_MN_MATH);
	g.Connect(bsdf, "bsdf", out, "surface");
	g.Connect(math, "value", bsdf, "roughness");

	// (a) propriete absente -> doit compiler
	NkMatCompileResult sansProp = NkMatCompileToNkSL(gReg, g);
	// (b) operation inconnue -> doit ECHOUER, et nommer la faute
	g.SetProp(math, NK_MPROP_OPERATION, NkValueText(t.real, "racine_carree_hyperbolique"));
	NkMatCompileResult inconnue = NkMatCompileToNkSL(gReg, g);
	// (c) operation valide -> doit compiler de nouveau
	g.SetProp(math, NK_MPROP_OPERATION, NkValueText(t.real, "multiplier"));
	NkMatCompileResult valide = NkMatCompileToNkSL(gReg, g);

	const bool nomme = !inconnue.ok && Apres(inconnue.error, "racine_carree_hyperbolique") > 0;
	const bool rienEmis = inconnue.source.Size() == 0;
	Cas("compile/operation-inconnue-refusee",
		sansProp.ok && !inconnue.ok && nomme && rienEmis && valide.ok,
		NkFormat("propriete absente compile={0} | inconnue refusee={1} en la nommant={2} rien emis={3} | valide "
				 "recompile={4} | message : {5}",
				 sansProp.ok ? 1 : 0, inconnue.ok ? 0 : 1, nomme ? 1 : 0, rienEmis ? 1 : 0, valide.ok ? 1 : 0,
				 inconnue.error));
}

static void CasToutesLesOperationsCompilent() {
	// DISCRIMINE : on ne teste pas UNE operation, on les teste TOUTES, sur les
	// quatre backends. Une branche oubliee dans l'emetteur — le `else` final qui
	// avale un cas non prevu — ne se verrait pas autrement. Et le compte vient de
	// la table, pas d'un nombre ecrit ici : ajouter une operation sans l'emettre
	// mettra ce cas au rouge tout seul.
	uint32 okMath = 0, okMix = 0;
	NkString premierEchec;
	for (uint32 pass = 0; pass < 2; ++pass) {
		const bool couleur = (pass == 1);
		for (uint32 i = 0; i < NkMatOperationCount(couleur); ++i) {
			NkNodeGraph g;
			const NkMatTypes t = NkMatRegisterTypes(g);
			const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
			const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
			g.Connect(bsdf, "bsdf", out, "surface");
			const NkNodeId n = NkMatAddNode(gReg, g, couleur ? NK_MN_MIX_COLOR : NK_MN_MATH);
			g.Connect(n, couleur ? "color" : "value", bsdf, couleur ? "base_color" : "roughness");
			g.SetProp(n, NK_MPROP_OPERATION, NkValueText(t.real, NkMatOperationAt(couleur, i)->cle));
			NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
			NkString be, err;
			const uint32 nb = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;
			if (r.ok && nb == 5) {
				if (couleur)
					++okMix;
				else
					++okMath;
			} else if (premierEchec.Size() == 0) {
				premierEchec = NkString(NkMatOperationAt(couleur, i)->cle);
				premierEchec.Append(" : ");
				premierEchec.Append(r.ok ? be : r.error);
			}
		}
	}
	const uint32 attMath = NkMatOperationCount(false), attMix = NkMatOperationCount(true);
	Cas("compile/toutes-les-operations-4-backends", okMath == attMath && okMix == attMix,
		NkFormat("Math {0}/{1} | Mix Color {2}/{3} (sur les 4 backends chacune) {4}", okMath, attMath, okMix,
				 attMix, premierEchec));
}

static void CasDivisionParZeroGardee() {
	// DISCRIMINE : une division nue produirait un NaN, qui contamine tout l'aval
	// et se voit comme un pixel noir OU blanc selon le backend — un defaut qui
	// change d'aspect d'une machine a l'autre, donc le pire a diagnostiquer.
	// On verifie que le code emis porte la garde, ET qu'il compile partout.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	const NkNodeId math = NkMatAddNode(gReg, g, NK_MN_MATH);
	g.Connect(bsdf, "bsdf", out, "surface");
	g.Connect(math, "value", bsdf, "roughness");
	g.SetProp(math, NK_MPROP_OPERATION, NkValueText(t.real, "diviser"));
	g.SetSocketDefault(math, "a", NkSocketDir::Input, NkValueReal(t.real, 1.f));
	g.SetSocketDefault(math, "b", NkSocketDir::Input, NkValueReal(t.real, 0.f));
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	NkString be, err;
	const uint32 ok = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;
	const bool garde = r.ok && ContientSansCasse(r.source, "== 0.0 ? 0.0");
	Cas("compile/division-par-zero-gardee", r.ok && ok == 5 && garde,
		NkFormat("emis={0} | {1}| garde presente dans le code emis={2}", r.ok ? 1 : 0, be, garde ? 1 : 0));
}


// ── ColorRamp : la premiere propriete a charge utile VARIABLE ────────────────

// Monte un graphe `ColorRamp -> base_color` et rend le resultat de compilation.
static NkMatCompileResult CompileRampe(NkNodeGraph &g, const NkMatTypes &t, const float32 *arrets, uint32 nbReels,
									   const char *interp) {
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	const NkNodeId ramp = NkMatAddNode(gReg, g, NK_MN_COLOR_RAMP);
	g.Connect(bsdf, "bsdf", out, "surface");
	g.Connect(ramp, "color", bsdf, "base_color");
	if (arrets)
		g.SetProp(ramp, NK_MPROP_STOPS, NkValueVec(t.ramp, arrets, nbReels));
	if (interp)
		g.SetProp(ramp, NK_MPROP_INTERP, NkValueText(t.ramp, interp));
	return NkMatCompileToNkSL(gReg, g);
}

static void CasRampeChargeVariable() {
	// LE CAS QUI EPROUVE LE SAC DE PROPRIETES. Jusqu'ici toutes les charges
	// utiles etaient de taille FIXE : un reel, trois, quatre. Une rampe en porte
	// 4xN, et N vient du fichier.
	//
	// DISCRIMINE : on compte les mix que la rampe emet. Une rampe a N arrets en
	// produit exactement N-1. Un emetteur qui n'en produirait qu'un — le premier
	// et le dernier arret, en ignorant ceux du milieu — donnerait un shader qui
	// compile et une rampe FAUSSE. Le compte est verifie pour 2, 3 et 5 arrets :
	// une seule taille ne prouverait rien d'une charge variable.
	const float32 a2[8] = {0.f, 1.f, 0.f, 0.f, 1.f, 0.f, 0.f, 1.f};
	const float32 a3[12] = {0.f, 1.f, 0.f, 0.f, 0.5f, 0.f, 1.f, 0.f, 1.f, 0.f, 0.f, 1.f};
	const float32 a5[20] = {0.f, 1.f, 0.f, 0.f, 0.25f, 1.f, 1.f, 0.f, 0.5f, 0.f,
							1.f, 0.f, 0.75f, 0.f, 1.f, 1.f, 1.f, 0.f, 0.f, 1.f};
	struct Jeu {
			const float32 *st;
			uint32 n;
			uint32 arrets;
	};
	const Jeu jeux[3] = {{a2, 8, 2}, {a3, 12, 3}, {a5, 20, 5}};
	uint32 bons = 0;
	NkString detail;
	for (uint32 j = 0; j < 3; ++j) {
		NkNodeGraph g;
		const NkMatTypes t = NkMatRegisterTypes(g);
		NkMatCompileResult r = CompileRampe(g, t, jeux[j].st, jeux[j].n, "lineaire");
		uint32 nbMix = 0;
		if (r.ok) {
			const char *q = r.source.CStr();
			while (*q) {
				if (q[0] == 'm' && q[1] == 'i' && q[2] == 'x' && q[3] == '(')
					++nbMix;
				++q;
			}
		}
		// Le puits emet DEUX mix pour son propre compte (specExp, specColor) :
		// c'est la lecon d'hier, ou compter tous les mix du shader mesurait le
		// modele d'eclairage en meme temps que le noeud.
		const uint32 attendus = jeux[j].arrets - 1u + 2u;
		NkString be, err;
		const uint32 ok = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;
		if (r.ok && ok == 5 && nbMix == attendus)
			++bons;
		detail.Append(NkFormat("{0} arrets->{1} mix (attendu {2}) ", jeux[j].arrets, nbMix, attendus));
	}
	Cas("colorramp/charge-utile-variable", bons == 3, detail);
}

static void CasRampeRefusNommes() {
	// QUATRE malformations, QUATRE messages distincts, et AUCUN shader emis.
	// DISCRIMINE : un compilateur qui rendrait le meme message pour tout
	// passerait un test qui ne compterait que les echecs. Et le cinquieme cas —
	// la propriete ABSENTE — doit au contraire COMPILER : c'est le voisin
	// legitime (le noeud vient d'etre pose), et le confondre avec une erreur
	// serait aussi faux que l'inverse.
	const float32 malForme[3] = {0.f, 1.f, 0.f};
	const float32 desordre[8] = {1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f};
	const float32 egales[8] = {0.5f, 1.f, 0.f, 0.f, 0.5f, 0.f, 0.f, 1.f};
	float32 trop[(NK_RAMP_ARRETS_MAX + 1) * 4];
	for (uint32 i = 0; i <= NK_RAMP_ARRETS_MAX; ++i) {
		trop[i * 4 + 0] = (float32)i * 0.001f;
		trop[i * 4 + 1] = 1.f;
		trop[i * 4 + 2] = 0.f;
		trop[i * 4 + 3] = 0.f;
	}
	NkString m[4];
	bool emisVide = true;
	const float32 *jeux[4] = {malForme, desordre, egales, trop};
	const uint32 tailles[4] = {3, 8, 8, (NK_RAMP_ARRETS_MAX + 1) * 4};
	for (uint32 j = 0; j < 4; ++j) {
		NkNodeGraph g;
		const NkMatTypes t = NkMatRegisterTypes(g);
		NkMatCompileResult r = CompileRampe(g, t, jeux[j], tailles[j], "lineaire");
		m[j] = r.ok ? NkString("A COMPILE (ne devait pas)") : r.error;
		if (r.source.Size() != 0)
			emisVide = false;
	}
	NkNodeGraph g2;
	const NkMatTypes t2 = NkMatRegisterTypes(g2);
	NkMatCompileResult absente = CompileRampe(g2, t2, nullptr, 0, nullptr);
	// ⚠️ LE REFUS DOIT DIRE LE COMPTE. Sans le nombre, l'auteur doit deviner
	// combien d'arrets retirer.
	const bool ditLeCompte = Apres(m[3], "33 demandes, plafond 32") > 0;
	const bool distincts = !(m[0] == m[1]) && !(m[1] == m[2]) && !(m[2] == m[3]) && !(m[0] == m[3]);
	Cas("colorramp/refus-nommes-et-distincts", distincts && emisVide && absente.ok && ditLeCompte,
		NkFormat("[{0}] [{1}] [{2}] [{3}] | distincts={4} rien emis={5} absente compile={6} plafond dit le "
				 "compte={7}",
				 m[0], m[1], m[2], m[3], distincts ? 1 : 0, emisVide ? 1 : 0, absente.ok ? 1 : 0,
				 ditLeCompte ? 1 : 0));
}

static void CasRampeInterpolation() {
	// Les deux interpolations emettent des codes DIFFERENTS — step pour la
	// constante, une pente pre-calculee pour la lineaire — et les deux compilent.
	// Une interpolation inconnue est refusee en la nommant, meme discipline que
	// les operations.
	//
	// ⚠️ Et un point qui merite d'etre verifie : la rampe lineaire ne contient
	// AUCUNE division dans le shader. Le denominateur est calcule a la
	// compilation, et NkMatLisRampe a deja garanti qu'il est non nul en refusant
	// les positions egales. Une division laissee dans le shader serait une garde
	// a maintenir pour rien.
	const float32 a[8] = {0.f, 1.f, 0.f, 0.f, 1.f, 0.f, 0.f, 1.f};
	NkNodeGraph g1;
	const NkMatTypes t1 = NkMatRegisterTypes(g1);
	NkMatCompileResult lin = CompileRampe(g1, t1, a, 8, "lineaire");
	NkNodeGraph g2;
	const NkMatTypes t2 = NkMatRegisterTypes(g2);
	NkMatCompileResult cst = CompileRampe(g2, t2, a, 8, "constante");
	NkNodeGraph g3;
	const NkMatTypes t3 = NkMatRegisterTypes(g3);
	NkMatCompileResult inc = CompileRampe(g3, t3, a, 8, "spline_de_bezier_cubique");

	NkString be1, be2, e1, e2;
	const uint32 ok1 = lin.ok ? CompileSurLesBackends(lin.source, be1, &e1) : 0u;
	const uint32 ok2 = cst.ok ? CompileSurLesBackends(cst.source, be2, &e2) : 0u;
	const bool pasDeStepEnLineaire = lin.ok && !ContientSansCasse(lin.source, "step(");
	const bool stepEnConstante = cst.ok && ContientSansCasse(cst.source, "step(");
	const bool nomme = !inc.ok && Apres(inc.error, "spline_de_bezier_cubique") > 0;
	Cas("colorramp/interpolations-et-refus",
		lin.ok && cst.ok && ok1 == 5 && ok2 == 5 && pasDeStepEnLineaire && stepEnConstante && nomme,
		NkFormat("lineaire {0}| constante {1}| step absent en lineaire={2} present en constante={3} | inconnue "
				 "refusee en la nommant={4}",
				 be1, be2, pasDeStepEnLineaire ? 1 : 0, stepEnConstante ? 1 : 0, nomme ? 1 : 0));
}

static void CasRampeAllerRetourFichier() {
	// ⚠️ LA CHARGE VARIABLE DOIT SURVIVRE AU FICHIER, et c'est le vrai test du
	// sac de proprietes : jusqu'ici l'aller-retour ne portait que des charges de
	// taille fixe. On compare les textes ET on recompile le graphe relu : les
	// deux shaders doivent etre identiques au caractere pres. Une charge tronquee
	// donnerait une rampe plus courte, qui compile parfaitement.
	const float32 a5[20] = {0.f, 1.f, 0.f, 0.f, 0.25f, 1.f, 1.f, 0.f, 0.5f, 0.f,
							1.f, 0.f, 0.75f, 0.f, 1.f, 1.f, 1.f, 0.f, 0.f, 1.f};
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	NkMatCompileResult direct = CompileRampe(g, t, a5, 20, "lineaire");
	NkString fichier;
	g.Serialize(fichier);
	NkNodeGraph g2;
	const bool relu = g2.Deserialize(fichier.CStr());
	NkMatCompileResult apres = NkMatCompileToNkSL(gReg, g2);
	const bool meme = direct.ok && apres.ok && (direct.source == apres.source);
	const NkGraphValue *v = nullptr;
	for (uint32 i = 0; i < g2.RawNodeCount(); ++i) {
		const NkNode *nd = g2.RawNodeAt(i);
		if (nd && nd->alive && nd->type == NkString(NK_MN_COLOR_RAMP))
			v = g2.FindProp(nd->id, NK_MPROP_STOPS);
	}
	const bool taille = v && v->numbers.Size() == 20;
	Cas("colorramp/charge-variable-survit-au-fichier", relu && meme && taille,
		NkFormat("relu={0} shader identique={1} | reels retrouves={2} (20 attendus) | fichier de {3} o",
				 relu ? 1 : 0, meme ? 1 : 0, v ? (uint32)v->numbers.Size() : 0u, (uint32)fichier.Size()));
}


// ── Image Texture : la premiere RESSOURCE consommee par un graphe ────────────

// Releve tous les bindings du set 2 declares dans un shader emis. On COMPTE des
// NOMBRES, on ne cherche pas des noms : le generateur HLSL minuscule et suffixe
// les identifiants, et chercher « nkGraphTex0 » testerait le generateur de noms
// plutot que le shader. Lecon payee le 22/08 sur `tMask` -> `tmask_tex`.
static uint32 RelieveBindingsSet2(const NkString &nksl, uint32 *out, uint32 maxOut) {
	const char *p = nksl.CStr();
	const char *motif = "@binding(set=2, binding=";
	uint32 n = 0;
	while (*p) {
		const char *a = p;
		const char *b = motif;
		while (*b && *a == *b) {
			++a;
			++b;
		}
		if (*b == 0) {
			uint32 v = 0;
			while (*a >= '0' && *a <= '9') {
				v = v * 10u + (uint32)(*a - '0');
				++a;
			}
			if (out && n < maxOut)
				out[n] = v;
			++n;
			p = a;
			continue;
		}
		++p;
	}
	return n;
}

static bool SlotConnu(uint32 binding) {
	for (uint32 i = 0; i < renderer::NK_MATBIND_GRAPH_SLOT_COUNT; ++i)
		if (renderer::NK_MATBIND_GRAPH_SLOTS[i] == binding)
			return true;
	return false;
}

// Monte un graphe avec `nb` noeuds Image Texture, tous branches vers la sortie
// par un empilement de Mix Color (il faut bien que chacun serve a quelque chose,
// sinon rien ne garantit qu'il soit emis).
static NkMatCompileResult CompileNTextures(uint32 nb) {
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	g.Connect(bsdf, "bsdf", out, "surface");
	NkNodeId precedent = NK_NODE_INVALID;
	for (uint32 i = 0; i < nb; ++i) {
		const NkNodeId tex = NkMatAddNode(gReg, g, NK_MN_IMAGE_TEXTURE);
		g.SetProp(tex, NK_MPROP_IMAGE, NkValueText(t.ramp, "image.png"));
		if (precedent == NK_NODE_INVALID) {
			precedent = tex;
		} else {
			const NkNodeId mix = NkMatAddNode(gReg, g, NK_MN_MIX_COLOR);
			g.Connect(precedent, "color", mix, "color1");
			g.Connect(tex, "color", mix, "color2");
			precedent = mix;
		}
	}
	if (precedent != NK_NODE_INVALID)
		g.Connect(precedent, precedent == NK_NODE_INVALID ? "color" : "color", bsdf, "base_color");
	return NkMatCompileToNkSL(gReg, g);
}

static void CasTexturePlafondRefusNomme() {
	// LE CAS QUI TRANCHE : `plafond` textures compile, `plafond + 1` est refuse
	// EN NOMMANT LE COMPTE. Sans le nombre, l'auteur doit deviner ce qu'il retire.
	//
	// DISCRIMINE aussi le bord exact : un plafond mal ecrit (`>=` au lieu de `>`)
	// refuserait le cas parfaitement legitime a `plafond` pile.
	const uint32 plafond = renderer::NK_MATBIND_GRAPH_SLOT_COUNT;
	NkMatCompileResult pile = CompileNTextures(plafond);
	NkMatCompileResult trop = CompileNTextures(plafond + 1);
	NkString be, err;
	const uint32 ok = pile.ok ? CompileSurLesBackends(pile.source, be, &err) : 0u;
	const NkString attendu = NkFormat("ce graphe demande {0} textures, le plafond est {1}", plafond + 1, plafond);
	const bool ditLeCompte = !trop.ok && Apres(trop.error, attendu.CStr()) > 0;
	Cas("imgtex/plafond-refus-nomme", pile.ok && ok == 5 && !trop.ok && ditLeCompte && trop.source.Size() == 0,
		NkFormat("{0} textures : compile={1} {2}| {3} textures : refuse={4} en disant le compte={5} | message : {6}",
				 plafond, pile.ok ? 1 : 0, be, plafond + 1, trop.ok ? 0 : 1, ditLeCompte ? 1 : 0, trop.error));
}

static void CasTextureBindingsDesDeuxCotes() {
	// ⚠️ LE CONTROLE QUI PROTEGE LA CONDITION NON NEGOCIABLE, et le plus
	// important des trois. Le compilateur et le layout doivent lire LE MEME
	// endroit ; si le plafond vivait a deux places, un compilateur qui autorise 8
	// quand le layout en declare 6 ecrirait sur deux bindings inexistants — sans
	// erreur, sans journal, sans difference d'image.
	//
	// On confronte donc le SHADER EMIS a la table C++ : chaque binding declare
	// doit etre dans `NK_MATBIND_GRAPH_SLOTS`, et leur nombre doit valoir le
	// nombre de noeuds texture. C'est la comparaison du code a une VERITE
	// EXTERNE, comme en phase 0.
	//
	// ⚠️ Et on COMPTE DES NOMBRES : chercher le nom du sampler testerait le
	// generateur de noms, pas le shader.
	const uint32 plafond = renderer::NK_MATBIND_GRAPH_SLOT_COUNT;
	uint32 bindings[16] = {};
	NkMatCompileResult r = CompileNTextures(plafond);
	const uint32 n = r.ok ? RelieveBindingsSet2(r.source, bindings, 16) : 0u;
	bool tousConnus = (n == plafond);
	bool tousDistincts = true;
	for (uint32 i = 0; i < n; ++i) {
		if (!SlotConnu(bindings[i]))
			tousConnus = false;
		for (uint32 k = i + 1; k < n; ++k)
			if (bindings[i] == bindings[k])
				tousDistincts = false;
	}
	NkString liste;
	for (uint32 i = 0; i < n; ++i)
		liste.Append(NkFormat("{0} ", bindings[i]));
	Cas("imgtex/bindings-declares-des-deux-cotes", r.ok && tousConnus && tousDistincts,
		NkFormat("{0} bindings emis [{1}] | tous dans NK_MATBIND_GRAPH_SLOTS={2} | tous distincts={3} | plafond C++={4}",
				 n, liste, tousConnus ? 1 : 0, tousDistincts ? 1 : 0, plafond));
}

static void CasTextureAucunBindingNeuf() {
	// LE COROLLAIRE, et il merite son propre cas : le graphe ne doit JAMAIS
	// declarer un binding hors de la table — c'est-a-dire ne jamais rien ajouter
	// au layout partage. Un emetteur qui numeroterait ses samplers 10, 11, 12
	// (des nombres libres a l'oeil) passerait le cas precedent si celui-ci ne
	// verifiait que le compte.
	//
	// DISCRIMINE par le MAXIMUM : on compare au plus grand slot de la table, pas
	// a une constante recopiee ici.
	uint32 maxTable = 0;
	for (uint32 i = 0; i < renderer::NK_MATBIND_GRAPH_SLOT_COUNT; ++i)
		if (renderer::NK_MATBIND_GRAPH_SLOTS[i] > maxTable)
			maxTable = renderer::NK_MATBIND_GRAPH_SLOTS[i];
	uint32 bindings[16] = {};
	NkMatCompileResult r = CompileNTextures(renderer::NK_MATBIND_GRAPH_SLOT_COUNT);
	const uint32 n = r.ok ? RelieveBindingsSet2(r.source, bindings, 16) : 0u;
	uint32 maxEmis = 0;
	for (uint32 i = 0; i < n; ++i)
		if (bindings[i] > maxEmis)
			maxEmis = bindings[i];
	Cas("imgtex/aucun-binding-hors-table", r.ok && n > 0 && maxEmis <= maxTable,
		NkFormat("plus grand binding emis={0} | plus grand de la table={1} | aucun binding neuf={2}", maxEmis,
				 maxTable, (maxEmis <= maxTable) ? 1 : 0));
}

static void CasTextureDeuxSorties() {
	// `Image Texture` est le PREMIER noeud a deux sorties. C'est lui qui a impose
	// de nommer les locales d'apres la prise et non par un « val » unique.
	//
	// DISCRIMINE : `color` alimente base_color, `alpha` alimente roughness. Les
	// deux locales doivent EXISTER et etre DIFFERENTES. Un emetteur qui garderait
	// un nom unique par noeud ferait lire la meme valeur aux deux entrees — un
	// shader qui compile et un materiau faux.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	const NkNodeId tex = NkMatAddNode(gReg, g, NK_MN_IMAGE_TEXTURE);
	g.Connect(bsdf, "bsdf", out, "surface");
	const NkLinkError e1 = g.Connect(tex, "color", bsdf, "base_color");
	const NkLinkError e2 = g.Connect(tex, "alpha", bsdf, "roughness");
	g.SetProp(tex, NK_MPROP_IMAGE, NkValueText(t.ramp, "peau.png"));
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	const NkString nc = NkFormat("n{0}_color", (uint32)tex);
	const NkString na = NkFormat("n{0}_alpha", (uint32)tex);
	const bool deux = r.ok && Apres(r.source, nc.CStr()) > 0 && Apres(r.source, na.CStr()) > 0;
	NkString be, err;
	const uint32 ok = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;
	Cas("imgtex/deux-sorties-distinctes",
		e1 == NkLinkError::Ok && e2 == NkLinkError::Ok && r.ok && ok == 5 && deux,
		NkFormat("liens {0}/{1} | {2}| locales '{3}' et '{4}' presentes={5}", NkString(NkLinkErrorName(e1)),
				 NkString(NkLinkErrorName(e2)), be, nc, na, deux ? 1 : 0));
}

static void CasTextureCoordonneeEtMappage() {
	// Sans entree `vector`, la texture lit l'UV du maillage — c'est le
	// comportement DEFINI du noeud chez Blender, pas un bouche-trou.
	// Avec un `Mapping` branche, elle doit lire la coordonnee TRANSFORMEE.
	//
	// DISCRIMINE : le shader du second cas doit citer la locale du Mapping ; le
	// premier doit citer `vUV`. Un emetteur qui ignorerait l'entree lirait `vUV`
	// dans les deux, et l'image ne bougerait jamais quoi qu'on branche.
	NkNodeGraph g1;
	const NkMatTypes t1 = NkMatRegisterTypes(g1);
	{
		const NkNodeId out = NkMatAddNode(gReg, g1, NK_MN_OUTPUT);
		const NkNodeId bsdf = NkMatAddNode(gReg, g1, NK_MN_PRINCIPLED);
		const NkNodeId tex = NkMatAddNode(gReg, g1, NK_MN_IMAGE_TEXTURE);
		g1.Connect(bsdf, "bsdf", out, "surface");
		g1.Connect(tex, "color", bsdf, "base_color");
		(void)t1;
	}
	NkMatCompileResult sansMap = NkMatCompileToNkSL(gReg, g1);

	NkNodeGraph g2;
	const NkMatTypes t2 = NkMatRegisterTypes(g2);
	const NkNodeId out2 = NkMatAddNode(gReg, g2, NK_MN_OUTPUT);
	const NkNodeId bsdf2 = NkMatAddNode(gReg, g2, NK_MN_PRINCIPLED);
	const NkNodeId tex2 = NkMatAddNode(gReg, g2, NK_MN_IMAGE_TEXTURE);
	const NkNodeId map = NkMatAddNode(gReg, g2, NK_MN_MAPPING);
	const NkNodeId coord = NkMatAddNode(gReg, g2, NK_MN_TEX_COORD);
	g2.Connect(bsdf2, "bsdf", out2, "surface");
	g2.Connect(tex2, "color", bsdf2, "base_color");
	g2.Connect(map, "vector_out", tex2, "vector");
	g2.Connect(coord, "uv", map, "vector");
	const float32 ech[3] = {4.f, 4.f, 1.f};
	g2.SetSocketDefault(map, "scale", NkSocketDir::Input, NkValueVec(t2.vector, ech, 3));
	NkMatCompileResult avecMap = NkMatCompileToNkSL(gReg, g2);

	const NkString nomMap = NkFormat("n{0}_vector_out", (uint32)map);
	const bool litUV = sansMap.ok && Apres(sansMap.source, ", vUV)") > 0;
	// ⚠️ CE CAS A SURVECU A SA MUTATION, et la faute etait dans le cas. Sa
	// premiere version cherchait le NOM de la locale du Mapping n'importe ou dans
	// le shader. Sous la mutation « l'entree vector est ignoree », le noeud
	// Mapping emettait toujours SA DECLARATION -- donc le nom etait present, et le
	// cas passait au vert alors que la texture lisait l'UV brut.
	//
	// **Chercher un nom n'est pas chercher un USAGE.** On verifie donc DEUX
	// choses : que l'appel de texture CITE la locale du mappage, et que le shader
	// mappe ne lit PLUS l'UV brut dans son appel de texture.
	const NkString appelMappe = NkFormat(", ({0}).xy)", nomMap);
	const bool litMap = avecMap.ok && Apres(avecMap.source, appelMappe.CStr()) > 0;
	const bool nePlusLireUV = avecMap.ok && Apres(avecMap.source, ", vUV)") < 0;
	// Le neutre MULTIPLICATIF : un Mapping sans echelle renseignee ne doit rien
	// changer. Le neutre general du compilateur est le NOIR — ici ce serait
	// annuler l image. Le cas verifie donc que l echelle posee (4) est bien la.
	// Le litteral porte son point decimal : PutLit le force, et ce cas a d abord
	// echoue parce que j attendais « vec3(4, 4, 1) ». L attendu etait faux, pas le code.
	const bool echelle = avecMap.ok && ContientSansCasse(avecMap.source, "vec3(4.0, 4.0, 1.0)");
	NkString be, err;
	const uint32 ok = avecMap.ok ? CompileSurLesBackends(avecMap.source, be, &err) : 0u;
	Cas("imgtex/uv-par-defaut-et-mappage", litUV && litMap && nePlusLireUV && ok == 5 && echelle,
		NkFormat("sans mappage lit vUV={0} | l'appel de texture CITE {1}={2} | ne lit plus l'UV brut={3} | echelle 4 "
				 "presente={4} | {5}",
				 litUV ? 1 : 0, nomMap, litMap ? 1 : 0, nePlusLireUV ? 1 : 0, echelle ? 1 : 0, be));
}


// ── Normal Map, Bump, Separate XYZ : le relief ───────────────────────────────

// Monte `Image Texture -> Normal Map -> Principled.normal`, avec la convention
// demandee posee sur la texture.
static NkMatCompileResult CompileCarteNormales(const char *convention) {
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	const NkNodeId nm = NkMatAddNode(gReg, g, NK_MN_NORMAL_MAP);
	const NkNodeId tex = NkMatAddNode(gReg, g, NK_MN_IMAGE_TEXTURE);
	g.Connect(bsdf, "bsdf", out, "surface");
	g.Connect(nm, "normal", bsdf, "normal");
	g.Connect(tex, "color", nm, "color");
	g.SetProp(tex, NK_MPROP_IMAGE, NkValueText(t.ramp, "relief.png"));
	if (convention)
		g.SetProp(tex, NK_MPROP_NORMAL_CONV, NkValueText(t.ramp, convention));
	return NkMatCompileToNkSL(gReg, g);
}

static void CasConventionNeChangePasLeShader() {
	// ⚠️ LE CAS QUI DECIDE, et il teste une ABSENCE d'effet.
	//
	// La convention d'une carte de normales est une donnee de PROVENANCE DU
	// FICHIER : une carte DirectX se convertit A L'IMPORT, jamais dans le
	// shader. La tentation naturelle est de retourner Y par pixel — ca marche,
	// et ca coute a chaque fragment tout en rendant l'etat de la texture
	// invisible dans la donnee.
	//
	// DISCRIMINE : les deux shaders doivent etre identiques AU CARACTERE PRES.
	// Un retournement dans le shader ajouterait un signe quelque part, et aucune
	// comparaison de comportement ne le verrait — seule l'egalite des textes le
	// denonce.
	NkMatCompileResult gl = CompileCarteNormales("opengl");
	NkMatCompileResult dx = CompileCarteNormales("directx");
	NkMatCompileResult sans = CompileCarteNormales(nullptr);
	const bool identiques = gl.ok && dx.ok && sans.ok && (gl.source == dx.source) && (gl.source == sans.source);
	Cas("normalmap/convention-ne-change-pas-le-shader", identiques,
		NkFormat("opengl {0} o | directx {1} o | sans convention {2} o | les trois identiques au caractere pres={3}",
				 (uint32)gl.source.Size(), (uint32)dx.source.Size(), (uint32)sans.source.Size(),
				 identiques ? 1 : 0));
}

static void CasConventionInconnueRefusee() {
	// Meme discipline que les operations : un mot inconnu est REFUSE en le
	// nommant. Un repli sur OpenGL inverserait le relief de la moitie des
	// fichiers, et l'image resterait plausible — le pire des cas.
	NkMatCompileResult r = CompileCarteNormales("mikktspace_inverse");
	const bool nomme = !r.ok && Apres(r.error, "mikktspace_inverse") > 0;
	Cas("normalmap/convention-inconnue-refusee", !r.ok && nomme && r.source.Size() == 0,
		NkFormat("refuse={0} en la nommant={1} rien emis={2} | message : {3}", r.ok ? 0 : 1, nomme ? 1 : 0,
				 r.source.Size() == 0 ? 1 : 0, r.error));
}

static void CasBaseTangenteSeulementSiUtile() {
	// Une base tangente coute DEUX paires de derivees par pixel. Un shader qui
	// la calculerait sans s'en servir paierait pour rien.
	//
	// DISCRIMINE dans les deux sens : absente quand aucun noeud ne la reclame,
	// PRESENTE des qu'un `Normal Map` apparait. Un controle a sens unique
	// laisserait passer « on ne l'emet jamais », ce qui casserait le relief.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	g.Connect(bsdf, "bsdf", out, "surface");
	(void)t;
	NkMatCompileResult sans = NkMatCompileToNkSL(gReg, g);
	NkMatCompileResult avec = CompileCarteNormales("opengl");
	const bool absente = sans.ok && Apres(sans.source, "nkT") < 0;
	const bool presente = avec.ok && Apres(avec.source, "nkT") > 0 && ContientSansCasse(avec.source, "dFdx(vUV)");
	NkString be, err;
	const uint32 ok = avec.ok ? CompileSurLesBackends(avec.source, be, &err) : 0u;
	Cas("normalmap/base-tangente-seulement-si-utile", absente && presente && ok == 5,
		NkFormat("sans Normal Map : base tangente absente={0} | avec : presente={1} | {2}", absente ? 1 : 0,
				 presente ? 1 : 0, be));
}

static void CasNormaleVoyageJusquAuPuits() {
	// ⚠️ LE DEFAUT QUE CE CAS EXISTE POUR ATTRAPER, et il a vraiment existe :
	// avant le 22/08 le puits recalculait `normalize(vNormal)` et jetait EN
	// SILENCE tout travail de relief en amont. Le shader compilait, l'image etait
	// plausible, et le noeud `Normal Map` ne servait a rien.
	//
	// DISCRIMINE : le puits doit lire la locale de la CHAINE, pas la normale
	// geometrique. On verifie que `surfNormal` cite la locale du Normal Map.
	// On monte le graphe ici pour disposer des IDENTIFIANTS : la chaine a
	// verifier compte DEUX maillons, et mon premier attendu n'en voyait qu'un.
	// La normale ne saute pas du Normal Map au puits — elle passe PAR le
	// Principled, qui la porte comme composante de son shader.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	const NkNodeId nm = NkMatAddNode(gReg, g, NK_MN_NORMAL_MAP);
	const NkNodeId tex = NkMatAddNode(gReg, g, NK_MN_IMAGE_TEXTURE);
	g.Connect(bsdf, "bsdf", out, "surface");
	g.Connect(nm, "normal", bsdf, "normal");
	g.Connect(tex, "color", nm, "color");
	g.SetProp(tex, NK_MPROP_IMAGE, NkValueText(t.ramp, "relief.png"));
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);

	// Maillon 1 : le Principled prend la normale du Normal Map.
	const NkString m1 = NkFormat("n{0}_normal = n{1}_normal", (uint32)bsdf, (uint32)nm);
	// Maillon 2 : le puits prend celle du Principled.
	const NkString m2 = NkFormat("surfNormal = n{0}_normal", (uint32)bsdf);
	const bool maillon1 = r.ok && Apres(r.source, m1.CStr()) > 0;
	const bool maillon2 = r.ok && Apres(r.source, m2.CStr()) > 0;
	// Et l'eclairage s'en sert VRAIMENT : c'est ce qui manquait avant le 22/08.
	const bool puitsUtilise = r.ok && Apres(r.source, "normalize(surfNormal)") > 0;
	// ⚠️ Assertion d'ABSENCE, en plus : le puits ne doit PLUS recalculer la
	// normale geometrique pour eclairer. Sans elle, un puits qui ferait les deux
	// passerait les trois presences ci-dessus.
	const bool neRecalculePlus = r.ok && Apres(r.source, "N3 = normalize(vNormal)") < 0;
	Cas("normal/voyage-jusqu-au-puits", r.ok && maillon1 && maillon2 && puitsUtilise && neRecalculePlus,
		NkFormat("maillon '{0}'={1} | maillon '{2}'={3} | eclaire avec={4} | ne recalcule plus la geometrique={5}",
				 m1, maillon1 ? 1 : 0, m2, maillon2 ? 1 : 0, puitsUtilise ? 1 : 0, neRecalculePlus ? 1 : 0));
}

static void CasBumpDeriveesEtGarde() {
	// `Bump` derive une normale d'un champ de hauteur par derivees d'ecran. Le
	// scalaire variable vient d'un `Separate XYZ` sur la coordonnee de texture —
	// sans lui, aucun scalaire VARIABLE n'existe dans un graphe et le relief ne
	// pourrait etre mesure que sur son absence d'effet.
	//
	// DISCRIMINE : le determinant DOIT etre garde. Une division nue produirait un
	// NaN sur un triangle degenere ou vu par la tranche — et un NaN contamine
	// l'aval en changeant d'aspect d'un backend a l'autre, donc il fait accuser
	// la machine.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	const NkNodeId bump = NkMatAddNode(gReg, g, NK_MN_BUMP);
	const NkNodeId sep = NkMatAddNode(gReg, g, NK_MN_SEPARATE_XYZ);
	const NkNodeId coord = NkMatAddNode(gReg, g, NK_MN_TEX_COORD);
	g.Connect(bsdf, "bsdf", out, "surface");
	g.Connect(bump, "normal", bsdf, "normal");
	g.Connect(sep, "x", bump, "height");
	g.Connect(coord, "uv", sep, "vector");
	g.SetSocketDefault(bump, "strength", NkSocketDir::Input, NkValueReal(t.real, 1.f));
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	const bool derive = r.ok && ContientSansCasse(r.source, "dFdx(n3_h)");
	const bool garde = r.ok && ContientSansCasse(r.source, "< 1e-12 ? 1e-12");
	NkString be, err;
	const uint32 ok = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;
	Cas("bump/derivees-et-garde-du-determinant", r.ok && ok == 5 && derive && garde,
		NkFormat("{0}| derivee de la hauteur presente={1} | determinant garde={2}", be, derive ? 1 : 0,
				 garde ? 1 : 0));
}

static void CasSepareTroisSorties() {
	// `Separate XYZ` est le second noeud a sorties multiples, et le premier a en
	// avoir TROIS. DISCRIMINE : les trois locales doivent exister ET etre
	// distinctes. Un emetteur qui reutiliserait un nom unique ferait lire la
	// meme composante aux trois entrees — un shader qui compile et un materiau
	// faux.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	const NkNodeId sep = NkMatAddNode(gReg, g, NK_MN_SEPARATE_XYZ);
	const NkNodeId coord = NkMatAddNode(gReg, g, NK_MN_TEX_COORD);
	g.Connect(bsdf, "bsdf", out, "surface");
	g.Connect(coord, "uv", sep, "vector");
	g.Connect(sep, "x", bsdf, "metallic");
	g.Connect(sep, "y", bsdf, "roughness");
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	const NkString nx = NkFormat("n{0}_x", (uint32)sep);
	const NkString ny = NkFormat("n{0}_y", (uint32)sep);
	const NkString nz = NkFormat("n{0}_z", (uint32)sep);
	const bool trois = r.ok && Apres(r.source, nx.CStr()) > 0 && Apres(r.source, ny.CStr()) > 0 &&
					   Apres(r.source, nz.CStr()) > 0;
	// Et le BSDF doit lire DEUX composantes DIFFERENTES.
	const NkString lecture = NkFormat("float surfMetallic");
	const bool distinctes = r.ok && Apres(r.source, NkFormat("n{0}_metallic = n{1}_x", (uint32)bsdf,
															 (uint32)sep).CStr()) > 0 &&
							Apres(r.source, NkFormat("n{0}_roughness = n{1}_y", (uint32)bsdf, (uint32)sep).CStr()) > 0;
	NkString be, err;
	const uint32 ok = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;
	(void)t;
	(void)lecture;
	Cas("separate/trois-sorties-distinctes", r.ok && ok == 5 && trois && distinctes,
		NkFormat("{0}| trois locales presentes={1} | le BSDF lit x et y separement={2}", be, trois ? 1 : 0,
				 distinctes ? 1 : 0));
}


// ── Les parametres EXPOSES : l'API publique du moteur ────────────────────────

// Pose une exposition sur une prise : la cle est `expose.<prise>`, la charge
// utile est le nom PUBLIC, et deux reels optionnels sont les bornes.
static void Expose(NkNodeGraph &g, const NkMatTypes &t, NkNodeId n, const char *prise, const char *nomPublic,
				   bool bornes = false, float32 mini = 0.f, float32 maxi = 1.f) {
	NkString cle(NK_MPROP_EXPOSE_PREFIX);
	cle.Append(prise);
	NkGraphValue v = NkValueText(t.real, nomPublic);
	if (bornes) {
		v.numbers.PushBack(mini);
		v.numbers.PushBack(maxi);
	}
	g.SetProp(n, cle.CStr(), v);
}

// Un Principled minimal vers la sortie, pour poser des expositions dessus.
static NkNodeId MontePrincipledExposable(NkNodeGraph &g, const NkMatTypes &t) {
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	g.Connect(bsdf, "bsdf", out, "surface");
	g.SetSocketDefault(bsdf, "roughness", NkSocketDir::Input, NkValueReal(t.real, 0.35f));
	return bsdf;
}

static void CasExposeLitLeBloc() {
	// DISCRIMINE : une prise exposee doit lire le BLOC UNIFORME, jamais un
	// litteral. C'est tout l'objet de l'exposition — une valeur repliee dans le
	// code ne peut plus changer a l'execution. On verifie donc la PRESENCE de la
	// lecture ET l'ABSENCE du litteral qui aurait ete emis sans exposition.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId bsdf = MontePrincipledExposable(g, t);
	Expose(g, t, bsdf, "roughness", "usure");
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	// SONDE DEMANDEE PAR LE COORDINATEUR : ecrire la source EMISE dans un
	// fichier et la regarder. Volontairement par fwrite et NON par le
	// journal : une source de shader est pleine d ACCOLADES LITTERALES, et
	// c est exactement ce que le formateur du depot detruit. Mesurer avec
	// l outil qui a cause la panne serait la meme faute une fois de plus.
	if (getenv("NK_DUMP_EXPOSE") && r.ok) {
		FILE *f = fopen("mesures_matgraph/source_expose.nksl", "wb");
		if (f) {
			fwrite(r.source.CStr(), 1, (size_t)r.source.Size(), f);
			fclose(f);
		}
	}
	const bool lit = r.ok && Apres(r.source, "nkParams.usure") > 0;
	const bool plusDeLitteral = r.ok && Apres(r.source, "roughness = 0.35") < 0;
	const bool bloc = r.ok && Apres(r.source, "uniform NkGraphParams") > 0;
	NkString be, err;
	const uint32 ok = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;
	Cas("variable/expose-lit-le-bloc", r.ok && ok == 5 && lit && plusDeLitteral && bloc && r.params.Size() == 1,
		NkFormat("{0}| bloc declare={1} lit nkParams.usure={2} | le litteral 0.35 a disparu={3} | {4} parametre(s)",
				 be, bloc ? 1 : 0, lit ? 1 : 0, plusDeLitteral ? 1 : 0, (uint32)r.params.Size()));
}

static void CasDecalagesStd140() {
	// ⚠️ CE CAS A ETE VERT PENDANT TROIS JOURS EN NE PROUVANT RIEN. Lis la
	// suite avant de lui faire confiance.
	//
	// Il verifie que reel, vec3, reel donnent 0/16/28 et non 0/4/16. C est
	// juste, et c est ce que `std140` prescrit. Mais il compare la table de
	// decalages du compilateur A ELLE-MEME : il ne traverse jamais HLSL, et
	// HLSL ne range pas un cbuffer comme `std140`. Un `float3` n a pas le
	// droit de CHEVAUCHER une frontiere de 16 octets, mais il a le droit d en
	// PARTAGER une : apres un `float`, HLSL le place a 4 quand `std140` le
	// place a 16. Pendant que ce cas etait vert, le moteur ecrivait a 16 et le
	// shader lisait a 4, et le pixel etait noir sans un mot.
	//
	// 🔴 UN CONTROLE QUI VERIFIE UNE CONVENTION NE VERIFIE PAS UN ACCORD.
	// Le seul controle qui pouvait attraper cela LIT LA VALEUR DEPUIS LA CARTE :
	// c est `rendu/parametre-expose-pilote-le-pixel`, dans NkMatGraphDemo, et
	// il compare deux variantes dont une seule aurait suffi a rassurer.
	//
	// Ce cas GARDE quand meme deux choses, et c est pour cela qu il reste :
	//   1. les decalages publies, contre une disposition sequentielle naive ;
	//   2. la PRESENCE du remplissage dans la source emise -- le mecanisme qui
	//      force les deux conventions a coincider. Attention : verifier qu il
	//      est present n est PAS verifier qu il fonctionne. C est le banc de
	//      rendu qui le prouve ; celui-ci empeche seulement qu on le retire par
	//      megarde en trouvant le bloc trop gros.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId bsdf = MontePrincipledExposable(g, t);
	Expose(g, t, bsdf, "metallic", "metal");    // reel  : 4 o
	Expose(g, t, bsdf, "base_color", "teinte"); // vec3  : aligne 16
	Expose(g, t, bsdf, "roughness", "usure");   // reel
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	const NkMatParamExpose *a = r.ok ? r.TrouveParam("metal") : nullptr;
	const NkMatParamExpose *b = r.ok ? r.TrouveParam("teinte") : nullptr;
	const NkMatParamExpose *c = r.ok ? r.TrouveParam("usure") : nullptr;
	const bool bons = a && b && c && a->decalage == 0u && b->decalage == 16u && c->decalage == 28u &&
			  r.paramsTaille == 32u;
	// Trois reels de remplissage doivent separer `metal` de `teinte` : sans eux
	// HLSL rangerait `teinte` a 4. On exige le COMPTE, pas la simple presence du
	// mot -- un seul remplissage laisserait la moitie du desaccord en place.
	const uint32 pads = b ? b->remplissageAvant : 0u;
	const bool remplissageEmis = r.ok && Apres(r.source, "_nkPad0") > 0 && Apres(r.source, "_nkPad1") > 0 &&
			   Apres(r.source, "_nkPad2") > 0 && pads == 3u;
	NkString be, err;
	const uint32 ok = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;
	Cas("variable/decalages-et-remplissage-emis", r.ok && ok == 5 && bons && remplissageEmis,
		NkFormat("metal@{0} teinte@{1} usure@{2} | bloc={3} o | attendus 0/16/28 et 32 (PAS 0/4/16) | "
			 "remplissage avant teinte={4} (3 attendu) et emis dans la source={5} | {6}",
			 a ? a->decalage : 999u, b ? b->decalage : 999u, c ? c->decalage : 999u, r.paramsTaille, pads,
			 remplissageEmis ? 1 : 0, be));
}

// ── LE HLSL EMIS, RANGE SELON LES REGLES DE HLSL ─────────────────────────────
//
// ⚠️ CECI TERMINE LA MESURE INTERROMPUE LE 23/08, et c'est le seul controle du
// banc qui traverse REELLEMENT les deux conventions.
//
// Le cas voisin `variable/decalages-et-remplissage-emis` dit lui-meme sa
// limite : il compare la table de decalages du compilateur A ELLE-MEME, et il
// est reste vert trois jours pendant que le moteur ecrivait a 16 et que la
// carte lisait a 4. Celui-ci prend le HLSL que NkSL vient d'emettre, le range
// selon HLSL, et exige que chaque parametre tombe a l'octet ou la table
// PUBLIEE (std140) l'annonce. C'est un ACCORD, plus une convention.
//
// La regle de rangement HLSL, la seule qui compte ici : un membre se pose au
// curseur, SAUF s'il CHEVAUCHAIT alors une frontiere de 16 octets — auquel cas
// il passe a la suivante. Un `float3` occupe 12 octets : apres un `float` il
// tient entier dans 4..16, donc HLSL le pose a **4** la ou std140 le pose a
// **16**. C'est tout le desaccord, et il est muet.
//
// ⚠️ CE N'EST PAS LE PIXEL. Le pixel reste le juge — c'est
// `rendu/parametre-expose-pilote-le-pixel`, dans NkMatGraphDemo. Ce cas-ci ne
// prouve pas que la carte lit ce que le moteur ecrit ; il prouve que les deux
// DISPOSITIONS coincident, ce qui est la moitie que le banc pouvait tenir sans
// GPU et ne tenait pas.
static uint32 RangeCBufferHLSL(const NkString &hlsl, const char *bloc, NkString *noms, uint32 *offs, uint32 maxN) {
	NkString motif("cbuffer ");
	motif.Append(bloc);
	const int32 d = Apres(hlsl, motif.CStr());
	if (d < 0)
		return 0;
	const char *p = hlsl.CStr() + d;
	while (*p && *p != '{')
		++p;
	if (!*p)
		return 0;
	++p;
	uint32 n = 0;
	uint32 curseur = 0;
	while (*p && *p != '}') {
		while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
			++p;
		if (!*p || *p == '}')
			break;
		NkString typ;
		while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' && *p != ';') {
			typ.Append(*p);
			++p;
		}
		while (*p == ' ' || *p == '\t')
			++p;
		NkString nom;
		while (*p && *p != ';' && *p != '\n' && *p != '\r') {
			if (*p != ' ' && *p != '\t')
				nom.Append(*p);
			++p;
		}
		if (*p == ';')
			++p;
		uint32 taille = 0;
		if (typ == NkString("float"))
			taille = 4u;
		else if (typ == NkString("float2"))
			taille = 8u;
		else if (typ == NkString("float3"))
			taille = 12u;
		else if (typ == NkString("float4"))
			taille = 16u;
		else
			continue; // un type qu'on ne sait pas ranger : on ne DEVINE pas.
		// La regle HLSL : ne pas CHEVAUCHER une frontiere de 16.
		if ((curseur % 16u) + taille > 16u)
			curseur = NkMatAlignUp(curseur, 16u);
		if (n < maxN) {
			noms[n] = nom;
			offs[n] = curseur;
			++n;
		}
		curseur += taille;
	}
	return n;
}

// Cherche dans la table rangee le membre `<bloc>_<nom>` et rend son decalage,
// ou 0xFFFFFFFF s'il n'y est pas. Le prefixe est celui que le generateur HLSL
// colle devant chaque membre de cbuffer.
static uint32 DecalageHLSLDe(const NkString *noms, const uint32 *offs, uint32 n, const char *bloc,
							 const NkString &nom) {
	NkString cible(bloc);
	cible.Append("_");
	cible.Append(nom);
	for (uint32 i = 0; i < n; ++i)
		if (noms[i] == cible)
			return offs[i];
	return 0xFFFFFFFFu;
}

// Range le bloc uniforme du NkSL EMIS selon `std140`, sans consulter la table
// du compilateur. Meme forme que `RangeCBufferHLSL`, autres regles :
// `std140` ALIGNE (un vec3 sur 16), HLSL se contente de ne pas CHEVAUCHER.
//
// ⚠️ POURQUOI CETTE TROISIEME LECTURE EXISTE, ET CE QU UNE MUTATION A APPRIS.
// La premiere version de ce cas comparait la table PUBLIEE au HLSL, et rien
// d autre. La mutation M1 -- retirer tout remplissage -- l a montree BORGNE :
// la table publiait alors 0/4/16 et le HLSL rangeait 0/4/16. Les deux cotes
// avaient bouge ENSEMBLE, donc ils s accordaient, et la comparaison etait
// verte sur une disposition que la carte n emploierait jamais.
// C est la meme faute que celle du cas voisin, prise une couche plus loin :
// deux mesures issues de la MEME decision ne se controlent pas l une l autre.
// Le troisieme rangement est INDEPENDANT -- il derive du texte NkSL et des
// regles de `std140`, pas de ce que le compilateur a decide -- et c est ce qui
// ferme la boucle : PUBLIE == std140 == HLSL, trois lectures, deux sources.
static uint32 RangeBlocStd140(const NkString &nksl, const char *bloc, NkString *noms, uint32 *offs, uint32 maxN) {
	NkString motif("uniform ");
	motif.Append(bloc);
	const int32 d = Apres(nksl, motif.CStr());
	if (d < 0)
		return 0;
	const char *p = nksl.CStr() + d;
	while (*p && *p != '{')
		++p;
	if (!*p)
		return 0;
	++p;
	uint32 n = 0;
	uint32 curseur = 0;
	while (*p && *p != '}') {
		while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
			++p;
		if (!*p || *p == '}')
			break;
		NkString typ;
		while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' && *p != ';') {
			typ.Append(*p);
			++p;
		}
		while (*p == ' ' || *p == '\t')
			++p;
		NkString nom;
		while (*p && *p != ';' && *p != '\n' && *p != '\r') {
			if (*p != ' ' && *p != '\t')
				nom.Append(*p);
			++p;
		}
		if (*p == ';')
			++p;
		// Sauter le commentaire de fin de ligne, s il y en a un.
		while (*p && *p != '\n')
			++p;
		uint32 taille = 0, aligne = 0;
		if (typ == NkString("float")) {
			taille = 4u;
			aligne = 4u;
		} else if (typ == NkString("vec2")) {
			taille = 8u;
			aligne = 8u;
		} else if (typ == NkString("vec3")) {
			taille = 12u;
			aligne = 16u;
		} else if (typ == NkString("vec4")) {
			taille = 16u;
			aligne = 16u;
		} else
			continue;
		curseur = NkMatAlignUp(curseur, aligne);
		if (n < maxN) {
			noms[n] = nom;
			offs[n] = curseur;
			++n;
		}
		curseur += taille;
	}
	return n;
}

static uint32 TrouveDecalage(const NkString *noms, const uint32 *offs, uint32 n, const NkString &nom) {
	for (uint32 i = 0; i < n; ++i)
		if (noms[i] == nom)
			return offs[i];
	return 0xFFFFFFFFu;
}

static void CasAccordStd140HLSLSurLeHLSLEmis() {
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId bsdf = MontePrincipledExposable(g, t);
	// ⚠️ DEUX PARAMETRES AU MOINS, ET UN VECTEUR APRES UN REEL. C'est la seule
	// forme qui mesure quelque chose : un montage a UN SEUL vecteur tombe a 0
	// sous les DEUX conventions, et serait reste vert le jour de la panne.
	Expose(g, t, bsdf, "metallic", "metal");	// reel
	Expose(g, t, bsdf, "base_color", "teinte"); // vec3 — celui qui revele le desaccord
	Expose(g, t, bsdf, "roughness", "usure");	// reel
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);

	NkSLCompiler c;
	NkString hlsl;
	if (r.ok) {
		NkSLCompileResult h = c.Compile(r.source, NkSLStage::NK_FRAGMENT, NkSLTarget::NK_HLSL_DX11);
		hlsl = h.source;
	}
	NkString noms[24];
	uint32 offs[24];
	const uint32 nm = hlsl.Size() ? RangeCBufferHLSL(hlsl, renderer::NK_MATBIND_GRAPH_PARAMS_BLOCK, noms, offs, 24u) : 0u;

	// Le bloc emis doit porter les trois parametres ET les trois remplissages :
	// six membres. Un compte plus petit voudrait dire qu'un remplissage n'a pas
	// traverse jusqu'au HLSL — et c'est LUI qui fait coincider les conventions.
	bool sixMembres = false; // renseigne apres le rangement std140, qui declare `ng`

	// Le meme bloc, range depuis le NkSL EMIS selon `std140`. Lecture
	// INDEPENDANTE : elle ne consulte pas la table du compilateur.
	NkString nomsG[24];
	uint32 offsG[24];
	const uint32 ng = r.ok ? RangeBlocStd140(r.source, renderer::NK_MATBIND_GRAPH_PARAMS_BLOCK, nomsG, offsG, 24u)
						   : 0u;

	// L'ACCORD, parametre par parametre, et A TROIS LECTURES.
	//
	// ⚠️ DEUX N'AURAIENT PAS SUFFI, et c'est la mutation M1 qui l'a montre :
	// en retirant tout remplissage, la table PUBLIEE passait a 0/4/16 et le
	// HLSL rangeait 0/4/16. Elles bougeaient ENSEMBLE, donc elles
	// s'accordaient — sur une disposition que `std140` n'emploie jamais. Le
	// rangement `std140` du NkSL emis est la seule des trois qui ne descende
	// pas de la decision du compilateur.
	sixMembres = (nm == 6u) && (ng == 6u);
	bool accord = r.ok && nm > 0 && ng > 0;
	NkString ligne;
	for (uint32 i = 0; i < (uint32)r.params.Size(); ++i) {
		const NkMatParamExpose &p = r.params[i];
		const uint32 oh = DecalageHLSLDe(noms, offs, nm, renderer::NK_MATBIND_GRAPH_PARAMS_BLOCK, p.nom);
		const uint32 og = TrouveDecalage(nomsG, offsG, ng, p.nom);
		if (oh != p.decalage || og != p.decalage)
			accord = false;
		ligne.Append(NkFormat("{0} publie@{1} std140@{2} hlsl@{3}{4} | ", p.nom, p.decalage, og, oh,
							  NkString((oh == p.decalage && og == p.decalage) ? "" : " DESACCORD")));
	}
	// Le temoin qui empeche le cas de se rassurer tout seul : le montage DOIT
	// contenir un vecteur qui suit un reel. Sans lui les deux conventions
	// tombent au meme endroit pour de mauvaises raisons.
	const NkMatParamExpose *v = r.ok ? r.TrouveParam("teinte") : nullptr;
	const bool temoinUtile = v && v->decalage == 16u && v->remplissageAvant == 3u;

	Cas("variable/accord-std140-hlsl-sur-le-hlsl-emis", r.ok && accord && sixMembres && temoinUtile,
		NkFormat("{0}membres : cbuffer HLSL={1} bloc NkSL={2} (6 attendus de chaque cote : 3 parametres + 3 "
				 "remplissages) | TEMOIN un vec3 APRES un reel, sans quoi les trois lectures donnent 0={3}",
				 ligne, nm, ng, temoinUtile ? 1 : 0));
}

static void CasPriseConnecteeEtExposeeRefusee() {
	// 🔴 LE CAS LE PLUS INSIDIEUX DE LA SERIE, et celui que ma proposition avait
	// oublie. Si une prise recoit un LIEN **et** porte une exposition, le lien
	// REMPLACE la valeur exposee : `SetFloat("usure", 0.9f)` ne fait RIEN. Le
	// materiau compile, il rend, le parametre est mort — aucune erreur, aucun
	// journal.
	//
	// ⚠️ Chez Blender le probleme ne se pose pas parce que brancher un lien FAIT
	// DISPARAITRE le widget : l'interface rend l'etat impossible. Nous n'avons
	// pas d'interface — c'est donc la validation qui doit le rendre impossible.
	//
	// DISCRIMINE : le TEMOIN est la meme exposition SANS le lien, qui doit
	// compiler. Un controle qui refuserait toute exposition passerait le premier
	// sans rien prouver.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId bsdf = MontePrincipledExposable(g, t);
	Expose(g, t, bsdf, "roughness", "usure");
	NkMatCompileResult temoin = NkMatCompileToNkSL(gReg, g);

	const NkNodeId val = NkMatAddNode(gReg, g, NK_MN_VALUE);
	const NkLinkError e = g.Connect(val, "value", bsdf, "roughness");
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	const bool nomme = !r.ok && Apres(r.error, "roughness") > 0 && Apres(r.error, "CONNECTEE") > 0;
	Cas("variable/prise-connectee-et-exposee-refusee",
		temoin.ok && e == NkLinkError::Ok && !r.ok && nomme && r.source.Size() == 0,
		NkFormat("temoin sans lien compile={0} | avec lien refuse={1} en nommant la prise={2} rien emis={3} | "
				 "message : {4}",
				 temoin.ok ? 1 : 0, r.ok ? 0 : 1, nomme ? 1 : 0, r.source.Size() == 0 ? 1 : 0, r.error));
}

static void CasExposeRefusNommes() {
	// QUATRE refus, QUATRE messages distincts. Ils ne se reparent pas pareil :
	// un doublon se renomme, un nom invalide se corrige, une prise inconnue
	// vient d'un renommage, et une prise shader n'est simplement pas une valeur.
	NkString m[4];
	{ // doublon de nom public
		NkNodeGraph g;
		const NkMatTypes t = NkMatRegisterTypes(g);
		const NkNodeId bsdf = MontePrincipledExposable(g, t);
		Expose(g, t, bsdf, "metallic", "reglage");
		Expose(g, t, bsdf, "roughness", "reglage");
		m[0] = NkMatCompileToNkSL(gReg, g).error;
	}
	{ // nom public invalide
		NkNodeGraph g;
		const NkMatTypes t = NkMatRegisterTypes(g);
		const NkNodeId bsdf = MontePrincipledExposable(g, t);
		Expose(g, t, bsdf, "roughness", "2 usures");
		m[1] = NkMatCompileToNkSL(gReg, g).error;
	}
	{ // prise inconnue -- ce que laisse un renommage de prise
		NkNodeGraph g;
		const NkMatTypes t = NkMatRegisterTypes(g);
		const NkNodeId bsdf = MontePrincipledExposable(g, t);
		Expose(g, t, bsdf, "rugosite", "usure");
		m[2] = NkMatCompileToNkSL(gReg, g).error;
	}
	{ // une prise SHADER n'est pas une valeur uniforme
		// ⚠️ CE CAS A D'ABORD MESURE AUTRE CHOSE. Ma premiere version exposait
		// `Material Output.surface`, qui est CONNECTEE : c'est donc le controle
		// du lien qui repondait, et le refus « type shader » n'etait jamais
		// atteint. Les quatre messages etaient bien distincts — simplement pas
		// les quatre que je croyais tester.
		//
		// On expose donc une prise shader LIBRE : `Mix Shader.shader1`, sur un
		// graphe par ailleurs valide.
		NkNodeGraph g;
		const NkMatTypes t = NkMatRegisterTypes(g);
		const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
		const NkNodeId mix = NkMatAddNode(gReg, g, NK_MN_MIX_SHADER);
		const NkNodeId emis = NkMatAddNode(gReg, g, NK_MN_EMISSION);
		g.Connect(mix, "shader", out, "surface");
		g.Connect(emis, "emission", mix, "shader2"); // shader1 reste LIBRE
		Expose(g, t, mix, "shader1", "melange");
		m[3] = NkMatCompileToNkSL(gReg, g).error;
	}
	const bool tous = m[0].Size() && m[1].Size() && m[2].Size() && m[3].Size();
	const bool distincts = tous && !(m[0] == m[1]) && !(m[1] == m[2]) && !(m[2] == m[3]) && !(m[0] == m[3]);
	Cas("variable/refus-nommes-et-distincts", distincts,
		NkFormat("[{0}] [{1}] [{2}] [{3}] | quatre messages distincts={4}", m[0], m[1], m[2], m[3],
				 distincts ? 1 : 0));
}

static void CasTypesExposablesListeClose() {
	// 🔴 CE QUE LE TAMPON UNIFORME DECIDE, ET QUI N'EST PAS UN DETAIL D'API.
	//
	// Un parametre ecrit depuis le gameplay ne doit JAMAIS faire recompiler le
	// graphe. Il vit donc dans un tampon uniforme — et c'est CELA qui contraint
	// ce qui est exposable : seul l'est ce dont la disposition est connue en
	// `std140` ET en HLSL, et dont les deux tombent au meme octet.
	//
	// ⚠️ CE CAS EXISTE PARCE QUE LA PASSE NE REFUSAIT QUE `shader`. Le reste
	// tombait dans un `else` — « tout ce qui n'est pas reel est un vec3 » — vrai
	// UNIQUEMENT parce qu'aucune prise d'ENTREE du catalogue ne porte
	// aujourd'hui `rampe` ni `courbe` (ColorRamp range sa rampe en PROPRIETE).
	// C'est la regle 4 mot pour mot : une condition implicitement vraie parce
	// qu'il n'existait qu'un cas.
	//
	// ET LE TROU EST ATTEIGNABLE, ce n'est pas une hypothese d'ecole :
	// `NkNodeGraph::AddSocket` est publique, et le catalogue est ouvert a
	// l'execution. Le cas l'emprunte telle quelle.
	NkString msg;
	{
		NkNodeGraph g;
		const NkMatTypes t = NkMatRegisterTypes(g);
		const NkNodeId bsdf = MontePrincipledExposable(g, t);
		g.AddSocket(bsdf, "rampe_pilotee", t.ramp, NkSocketDir::Input);
		Expose(g, t, bsdf, "rampe_pilotee", "degrade");
		msg = NkMatCompileToNkSL(gReg, g).error;
	}
	// Le refus doit NOMMER le type demande — sinon l'auteur cherche du cote de
	// son nom public, qui est parfaitement valide.
	const bool refuse = msg.Size() > 0 && Apres(msg, "mat.rampe") > 0;

	// ⚠️ LE TEMOIN QUI EMPECHE LA LISTE DE SE VIDER. Une liste close qui
	// refuserait TOUT passerait la moitie ci-dessus sans broncher. Les trois
	// types exposables doivent donc etre ACCEPTES, et dans le meme montage.
	NkNodeGraph g2;
	const NkMatTypes t2 = NkMatRegisterTypes(g2);
	const NkNodeId b2 = MontePrincipledExposable(g2, t2);
	g2.AddSocket(b2, "vecteur_pilote", t2.vector, NkSocketDir::Input);
	Expose(g2, t2, b2, "roughness", "usure");	 // reel
	Expose(g2, t2, b2, "base_color", "teinte");	 // couleur
	Expose(g2, t2, b2, "vecteur_pilote", "axe"); // vecteur
	NkMatCompileResult r2 = NkMatCompileToNkSL(gReg, g2);
	const bool troisAcceptes = r2.ok && r2.params.Size() == 3;

	// Et une COURBE se refuse aussi : deux types absents de la liste, pas un.
	// Un seul aurait pu etre attrape par un cas particulier au lieu d'une liste.
	NkString msgC;
	{
		NkNodeGraph g3;
		const NkMatTypes t3 = NkMatRegisterTypes(g3);
		const NkNodeId b3 = MontePrincipledExposable(g3, t3);
		g3.AddSocket(b3, "courbe_pilotee", t3.curve, NkSocketDir::Input);
		Expose(g3, t3, b3, "courbe_pilotee", "reglage");
		msgC = NkMatCompileToNkSL(gReg, g3).error;
	}
	const bool refuseCourbe = msgC.Size() > 0 && Apres(msgC, "mat.courbe") > 0;

	Cas("variable/types-exposables-liste-close", refuse && refuseCourbe && troisAcceptes,
		NkFormat("rampe refusee en se nommant={0} [{1}] | courbe refusee en se nommant={2} | TEMOIN les trois "
				 "exposables (reel, vecteur, couleur) passent ensemble={3} ({4} parametres)",
				 refuse ? 1 : 0, msg, refuseCourbe ? 1 : 0, troisAcceptes ? 1 : 0, (uint32)r2.params.Size()));
}

static void CasDefautEstCeluiDeLaPrise() {
	// ⚠️ RENFORCEMENT (c) : le defaut d'un parametre expose EST le `defaultValue`
	// de sa prise, jamais une seconde valeur rangee a cote. Deux sources pour une
	// meme chose divergent, et c'est alors l'editeur qui montre l'une pendant que
	// le moteur envoie l'autre.
	//
	// DISCRIMINE : on CHANGE le defaut de la prise apres avoir pose l'exposition,
	// et la disposition doit suivre. Une implantation qui aurait recopie la
	// valeur au moment de l'exposition rendrait l'ancienne.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId bsdf = MontePrincipledExposable(g, t); // roughness = 0,35
	Expose(g, t, bsdf, "roughness", "usure", true, 0.f, 1.f);
	g.SetSocketDefault(bsdf, "roughness", NkSocketDir::Input, NkValueReal(t.real, 0.72f));
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	const NkMatParamExpose *p = r.ok ? r.TrouveParam("usure") : nullptr;
	const bool suit = p && p->defaut.IsSet() && p->defaut.numbers.Size() == 1 && p->defaut.numbers[0] == 0.72f;
	const bool bornes = p && p->bornes && p->borneMin == 0.f && p->borneMax == 1.f;
	Cas("variable/le-defaut-est-celui-de-la-prise", r.ok && suit && bornes,
		NkFormat("defaut retenu={0} (0,72 attendu apres modification de la prise) | bornes lues={1} [{2} ; {3}]",
				 (p && p->defaut.numbers.Size() == 1) ? (double)p->defaut.numbers[0] : -1.0, bornes ? 1 : 0,
				 p ? (double)p->borneMin : -1.0, p ? (double)p->borneMax : -1.0));
}

static void CasJetonSuitLaDisposition() {
	// ⚠️ RENFORCEMENT (b) : recompiler un graphe EDITE peut reordonner le bloc.
	// Du code de jeu ayant retenu un DECALAGE ecrirait alors dans le mauvais
	// parametre — sans erreur, avec une valeur credible. Le jeton existe pour
	// qu'un cache de decalages puisse se jeter.
	//
	// DISCRIMINE dans les DEUX sens : deux compilations du meme graphe donnent le
	// MEME jeton (sinon tout cache serait inutile), et une disposition differente
	// en donne un AUTRE (sinon le jeton ne protegerait rien).
	NkNodeGraph g1;
	const NkMatTypes t1 = NkMatRegisterTypes(g1);
	const NkNodeId b1 = MontePrincipledExposable(g1, t1);
	Expose(g1, t1, b1, "metallic", "metal");
	Expose(g1, t1, b1, "roughness", "usure");
	NkMatCompileResult a1 = NkMatCompileToNkSL(gReg, g1);
	NkMatCompileResult a2 = NkMatCompileToNkSL(gReg, g1);

	NkNodeGraph g2;
	const NkMatTypes t2 = NkMatRegisterTypes(g2);
	const NkNodeId b2 = MontePrincipledExposable(g2, t2);
	Expose(g2, t2, b2, "roughness", "usure"); // ordre INVERSE
	Expose(g2, t2, b2, "metallic", "metal");
	NkMatCompileResult b = NkMatCompileToNkSL(gReg, g2);

	const bool stable = a1.ok && a2.ok && a1.jeton == a2.jeton && a1.jeton != 0;
	const bool change = b.ok && a1.jeton != b.jeton;
	// Et la disposition a REELLEMENT change : sinon le jeton changerait pour rien.
	const NkMatParamExpose *pa = a1.ok ? a1.TrouveParam("usure") : nullptr;
	const NkMatParamExpose *pb = b.ok ? b.TrouveParam("usure") : nullptr;
	const bool disposition = pa && pb && pa->decalage != pb->decalage;
	Cas("variable/jeton-suit-la-disposition", stable && change && disposition,
		NkFormat("meme graphe deux fois : jeton stable={0} | ordre inverse : jeton different={1} | et le decalage "
				 "d'usure a bouge {2}->{3} : {4}",
				 stable ? 1 : 0, change ? 1 : 0, pa ? pa->decalage : 999u, pb ? pb->decalage : 999u,
				 disposition ? 1 : 0));
}

static void CasRechercheParNom() {
	// L'API du moteur est par NOM. Un nom inconnu rend nullptr, jamais le
	// premier parametre — sinon `SetFloat("faute_de_frappe", …)` piloterait
	// silencieusement autre chose.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId bsdf = MontePrincipledExposable(g, t);
	Expose(g, t, bsdf, "metallic", "metal");
	Expose(g, t, bsdf, "roughness", "usure");
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	const bool trouve = r.ok && r.TrouveParam("usure") && r.TrouveParam("metal");
	const bool inconnu = r.ok && r.TrouveParam("usur") == nullptr && r.TrouveParam("usuree") == nullptr &&
						 r.TrouveParam("") == nullptr;
	Cas("variable/recherche-par-nom", r.ok && trouve && inconnu,
		NkFormat("les deux noms trouves={0} | prefixe, suffixe et vide rendent nullptr={1}", trouve ? 1 : 0,
				 inconnu ? 1 : 0));
}

static void CasSansExpositionAucunBloc() {
	// Le temoin de l'autre bord : un graphe sans exposition ne doit declarer
	// AUCUN bloc. Sans ce cas, une implantation qui emettrait toujours le bloc
	// passerait tous les autres — et ferait payer un tampon uniforme a chaque
	// materiau constant.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	MontePrincipledExposable(g, t);
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	const bool aucunBloc = r.ok && Apres(r.source, "NkGraphParams") < 0;
	Cas("variable/sans-exposition-aucun-bloc", r.ok && aucunBloc && r.params.Empty() && r.paramsTaille == 0u,
		NkFormat("bloc absent={0} | {1} parametre(s) | taille={2} o", aucunBloc ? 1 : 0, (uint32)r.params.Size(),
				 r.paramsTaille));
}


static void CasBlocEtTexturesNeSeMarchentPasDessus() {
	// ⚠️ « PRESENT DANS LA TABLE » NE SUFFIT PAS : un binding peut etre declare
	// ET DEJA OCCUPE. Le controle precedent verifiait que chaque binding de
	// TEXTURE appartient a la table ; il ne disait rien du bloc uniforme, qui vit
	// dans le meme set et donc dans le meme espace de numeros.
	//
	// Ce cas monte le graphe le plus charge possible — le plafond de textures ET
	// des parametres exposes — et exige que TOUS les bindings du set 2 soient
	// deux a deux distincts. Un jour ou quelqu'un ajoutera le slot des parametres
	// a la table des textures, ce cas tombera ; sans lui, le shader declarerait
	// deux ressources au meme endroit et l'une ecraserait l'autre en silence.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	g.Connect(bsdf, "bsdf", out, "surface");
	// Le plafond de textures, chainees par des Mix Color pour que chacune serve.
	NkNodeId precedent = NK_NODE_INVALID;
	for (uint32 i = 0; i < renderer::NK_MATBIND_GRAPH_SLOT_COUNT; ++i) {
		const NkNodeId tex = NkMatAddNode(gReg, g, NK_MN_IMAGE_TEXTURE);
		g.SetProp(tex, NK_MPROP_IMAGE, NkValueText(t.ramp, "img.png"));
		if (precedent == NK_NODE_INVALID) {
			precedent = tex;
		} else {
			const NkNodeId mix = NkMatAddNode(gReg, g, NK_MN_MIX_COLOR);
			g.Connect(precedent, "color", mix, "color1");
			g.Connect(tex, "color", mix, "color2");
			precedent = mix;
		}
	}
	g.Connect(precedent, "color", bsdf, "base_color");
	// ET des parametres exposes, donc un bloc uniforme dans le MEME set.
	Expose(g, t, bsdf, "roughness", "usure");
	Expose(g, t, bsdf, "metallic", "metal");

	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	uint32 bindings[32] = {};
	const uint32 n = r.ok ? RelieveBindingsSet2(r.source, bindings, 32) : 0u;
	// textures + le bloc = plafond + 1
	const bool compte = (n == renderer::NK_MATBIND_GRAPH_SLOT_COUNT + 1u);
	bool distincts = true;
	for (uint32 i = 0; i < n; ++i)
		for (uint32 k = i + 1; k < n; ++k)
			if (bindings[i] == bindings[k])
				distincts = false;
	// Et le binding du bloc ne doit PAS figurer dans la table des textures.
	bool blocHorsDesTextures = true;
	for (uint32 i = 0; i < renderer::NK_MATBIND_GRAPH_SLOT_COUNT; ++i)
		if (renderer::NK_MATBIND_GRAPH_SLOTS[i] == renderer::NK_MATBIND_GRAPH_PARAMS)
			blocHorsDesTextures = false;
	NkString liste;
	for (uint32 i = 0; i < n; ++i)
		liste.Append(NkFormat("{0} ", bindings[i]));
	NkString be, err;
	const uint32 ok = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;
	Cas("imgtex/bloc-et-textures-ne-se-marchent-pas-dessus",
		r.ok && ok == 5 && compte && distincts && blocHorsDesTextures,
		NkFormat("{0} bindings emis [{1}] (attendu {2}) | deux a deux distincts={3} | le slot du bloc ({4}) est "
				 "hors de la table des textures={5} | {6}",
				 n, liste, renderer::NK_MATBIND_GRAPH_SLOT_COUNT + 1u, distincts ? 1 : 0,
				 (uint32)renderer::NK_MATBIND_GRAPH_PARAMS, blocHorsDesTextures ? 1 : 0, be));
}


static void CasIncludeNeSeResoutPas() {
	// SONDE : l'#include du dialecte NkSL se resout-il quand le shader est
	// compile DEPUIS UNE CHAINE et non depuis un fichier ? La reponse decide si
	// les noeuds proceduraux REUTILISENT NkNoise.glsli ou doivent l'inliner.
	NkString src;
	src.Append("#include \"Include/NkNoise.glsli\"\n\n");
	src.Append("@location(0) in vec2 vUV;\n@location(0) out vec4 fragColor;\n\n");
	src.Append("@stage(fragment)\n@entry\nvoid main() {\n");
	src.Append("    float n = NkFBM2D(vUV * 4.0, 3);\n    fragColor = vec4(n, n, n, 1.0);\n}\n");
	NkString be, err;
	const uint32 ok = CompileSurLesBackends(src, be, &err);
	// ⚠️ CE CAS ATTESTE UNE LIMITE, PAS UNE CAPACITE — et c'est voulu.
	//
	// Le `#include` du dialecte NkSL NE SE RESOUT PAS quand le shader est compile
	// depuis une CHAINE : le compilateur rend « #include not found ». Un shader
	// engendre n'existe pas sur disque, il doit donc etre AUTONOME — c'est ce qui
	// oblige a recopier les briques de NkNoise.glsli dans le compilateur.
	//
	// On garde ce cas parce qu'une limite non testee se perd : si un jour le
	// resolveur apprend a travailler depuis une chaine, CE CAS PASSERA AU ROUGE,
	// et ce sera le bon moment pour supprimer la recopie.
	Cas("nksl/include-ne-se-resout-pas-depuis-une-chaine", ok == 0,
		NkFormat("{0}| aucun backend ne resout l include (0 attendu) | message : {1}", be, err));
}


// ── Rang 2 : le procedural ───────────────────────────────────────────────────

// Monte `<noeud procedural>.<prise> -> Principled.<cible>` et compile.
static NkMatCompileResult CompileProcedural(const char *cleNoeud, const char *prise, const char *cible,
											const char *typeProp, NkNodeId *outNode = nullptr) {
	static NkNodeGraph g;
	g.Clear();
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	const NkNodeId n = NkMatAddNode(gReg, g, cleNoeud);
	g.Connect(bsdf, "bsdf", out, "surface");
	g.Connect(n, prise, bsdf, cible);
	if (typeProp)
		g.SetProp(n, NK_MPROP_TYPE, NkValueText(t.ramp, typeProp));
	if (outNode)
		*outNode = n;
	return NkMatCompileToNkSL(gReg, g);
}

static void CasProceduralCompile() {
	// Les trois noeuds proceduraux, sur les quatre backends. DISCRIMINE aussi une
	// propriete du systeme qui compte : ils ne consomment AUCUN slot de texture.
	// Un procedural qui en prendrait un reduirait le plafond sans le dire.
	struct Jeu {
			const char *cle;
			const char *prise;
			const char *cible;
	};
	const Jeu jeux[6] = {{NK_MN_NOISE, "fac", "roughness"},
						 {NK_MN_GRADIENT, "fac", "roughness"},
						 {NK_MN_CHECKER, "color", "base_color"},
						 {NK_MN_VORONOI, "distance", "roughness"},
						 {NK_MN_WAVE, "fac", "roughness"},
						 {NK_MN_BRICK, "color", "base_color"}};
	uint32 bons = 0;
	uint32 slotsUtilises = 0;
	NkString detail;
	for (uint32 i = 0; i < 6; ++i) {
		NkMatCompileResult r = CompileProcedural(jeux[i].cle, jeux[i].prise, jeux[i].cible, nullptr);
		NkString be, err;
		const uint32 ok = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;
		uint32 bind[16] = {};
		slotsUtilises += r.ok ? RelieveBindingsSet2(r.source, bind, 16) : 0u;
		if (r.ok && ok == 5)
			++bons;
		detail.Append(NkFormat("{0}={1} ", NkString(jeux[i].cle), r.ok ? be : r.error));
	}
	Cas("procedural/six-noeuds-4-backends", bons == 6 && slotsUtilises == 0,
		NkFormat("{0}| slots de texture consommes={1} (0 attendu : le procedural est gratuit en ressources)",
				 detail, slotsUtilises));
}

static void CasBriquesRecopieesVerbatim() {
	// ⚠️ LE CAS QUI GARDE LA DUPLICATION. Le shader engendre ne peut pas inclure
	// `NkNoise.glsli` (le resolveur ne travaille pas depuis une chaine, cf. le cas
	// `nksl/include-...`), donc le compilateur en RECOPIE les fonctions. Une
	// recopie sans garde diverge — c'est le motif que ce depot a paye quatre fois
	// en une nuit.
	//
	// DISCRIMINE : on lit le `.glsli` SUR LE DISQUE et on exige que chaque
	// fonction emise s'y retrouve **mot pour mot**. Le jour ou quelqu'un corrige
	// une formule dans le fichier, ce cas passe au rouge tant que le compilateur
	// n'a pas suivi. C'est la meme parade que pour les bindings : comparer le code
	// a une VERITE EXTERNE, faute de pouvoir partager.
	NkString glsli;
	if (!LireFichier("Resources/NKRenderer/Shaders/Include/NkNoise.glsli", glsli)) {
		Cas("procedural/briques-recopiees-verbatim", false, NkString("NkNoise.glsli INTROUVABLE"));
		return;
	}
	NkNodeId n = NK_NODE_INVALID;
	NkMatCompileResult r = CompileProcedural(NK_MN_NOISE, "fac", "roughness", nullptr, &n);
	// Les signatures ET un morceau de corps de chacune : une signature seule
	// passerait alors que le corps aurait diverge.
	const char *morceaux[6] = {"float NkHash2(vec2 p) {",
							   "return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);",
							   "float NkValueNoise2D(vec2 p) {",
							   "vec2 u = f * f * (3.0 - 2.0 * f);",
							   "float NkFBM2D(vec2 p, int octaves) {",
							   "value += NkValueNoise2D(p) * amp;"};
	uint32 dansLeShader = 0, dansLeFichier = 0;
	for (uint32 i = 0; i < 6; ++i) {
		if (r.ok && Apres(r.source, morceaux[i]) > 0)
			++dansLeShader;
		if (Apres(glsli, morceaux[i]) > 0)
			++dansLeFichier;
	}
	Cas("procedural/briques-recopiees-verbatim", r.ok && dansLeShader == 6 && dansLeFichier == 6,
		NkFormat("{0}/6 morceaux dans le shader emis | {1}/6 dans NkNoise.glsli sur le disque | identiques={2}",
				 dansLeShader, dansLeFichier, (dansLeShader == 6 && dansLeFichier == 6) ? 1 : 0));
}

static void CasBriquesSeulementSiUtiles() {
	// Un shader qui porterait les fonctions de bruit sans s'en servir alourdirait
	// chaque materiau pour rien. DISCRIMINE dans les DEUX sens : absentes sans
	// noeud de bruit, presentes des qu'il y en a un — un controle a sens unique
	// laisserait passer « on ne les emet jamais ».
	NkMatCompileResult sans = CompileProcedural(NK_MN_CHECKER, "color", "base_color", nullptr);
	NkMatCompileResult avec = CompileProcedural(NK_MN_NOISE, "fac", "roughness", nullptr);
	const bool absentes = sans.ok && Apres(sans.source, "NkFBM2D") < 0;
	const bool presentes = avec.ok && Apres(avec.source, "NkFBM2D") > 0;
	Cas("procedural/briques-seulement-si-utiles", absentes && presentes,
		NkFormat("sans noeud de bruit : absentes={0} | avec : presentes={1}", absentes ? 1 : 0,
				 presentes ? 1 : 0));
}

static void CasDegradeTypesEtRefus() {
	// Les quatre types de degrade emettent des codes DIFFERENTS et compilent tous.
	// Un type inconnu est refuse en le nommant — meme discipline que partout.
	//
	// DISCRIMINE : on verifie que les quatre sources emises sont deux a deux
	// DIFFERENTES. Un emetteur qui ignorerait le type les rendrait identiques, et
	// un simple « ca compile » ne le verrait pas.
	NkString sources[4];
	uint32 ok4 = 0;
	for (uint32 i = 0; i < NkMatTypeDegradeCount(); ++i) {
		NkMatCompileResult r = CompileProcedural(NK_MN_GRADIENT, "fac", "roughness",
												 NkMatTypeDegradeAt(i)->cle);
		NkString be, err;
		if (r.ok && CompileSurLesBackends(r.source, be, &err) == 5)
			++ok4;
		sources[i] = r.source;
	}
	bool tousDifferents = true;
	for (uint32 i = 0; i < 4; ++i)
		for (uint32 k = i + 1; k < 4; ++k)
			if (sources[i] == sources[k])
				tousDifferents = false;
	NkMatCompileResult inc = CompileProcedural(NK_MN_GRADIENT, "fac", "roughness", "spirale_logarithmique");
	const bool nomme = !inc.ok && Apres(inc.error, "spirale_logarithmique") > 0;
	Cas("procedural/degrade-types-et-refus",
		ok4 == NkMatTypeDegradeCount() && tousDifferents && nomme && inc.source.Size() == 0,
		NkFormat("{0}/{1} types compilent sur 4 backends | les 4 sources sont differentes={2} | type inconnu "
				 "refuse en le nommant={3}",
				 ok4, NkMatTypeDegradeCount(), tousDifferents ? 1 : 0, nomme ? 1 : 0));
}

static void CasOctavesBornees() {
	// ⚠️ Le nombre d'octaves vient d'une VALEUR DU GRAPHE, donc potentiellement
	// d'un parametre expose : une boucle dont le compte est libre peut ne pas se
	// derouler, et certains backends refusent alors le shader. Le borner coute
	// deux appels ; ne pas le borner coute un shader qui compile ici et pas
	// ailleurs.
	//
	// DISCRIMINE : la borne doit etre DANS LE CODE EMIS, pas appliquee a la
	// valeur au moment de la compilation — sinon un `detail` branche sur un autre
	// noeud echapperait a la borne.
	NkNodeId n = NK_NODE_INVALID;
	NkMatCompileResult r = CompileProcedural(NK_MN_NOISE, "fac", "roughness", nullptr, &n);
	const bool borne = r.ok && ContientSansCasse(r.source, "clamp(") &&
					   ContientSansCasse(r.source, ", 1.0, 8.0)");
	NkString be, err;
	const uint32 ok = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;
	Cas("procedural/octaves-bornees-dans-le-shader", r.ok && ok == 5 && borne,
		NkFormat("{0}| borne [1,8] presente dans le code emis={1}", be, borne ? 1 : 0));
}


static void CasOndeTypesEtRefus() {
	// Meme discipline que le degrade : deux types, deux codes DIFFERENTS, et un
	// type inconnu refuse en le nommant.
	NkString a, b;
	uint32 ok = 0;
	for (uint32 i = 0; i < NkMatTypeOndeCount(); ++i) {
		NkMatCompileResult r = CompileProcedural(NK_MN_WAVE, "fac", "roughness", NkMatTypeOndeAt(i)->cle);
		NkString be, err;
		if (r.ok && CompileSurLesBackends(r.source, be, &err) == 5)
			++ok;
		if (i == 0)
			a = r.source;
		else
			b = r.source;
	}
	NkMatCompileResult inc = CompileProcedural(NK_MN_WAVE, "fac", "roughness", "dents_de_scie");
	const bool nomme = !inc.ok && Apres(inc.error, "dents_de_scie") > 0;
	Cas("procedural/onde-types-et-refus", ok == NkMatTypeOndeCount() && !(a == b) && nomme,
		NkFormat("{0}/{1} types compilent | bandes et anneaux different={2} | inconnu refuse en le nommant={3}", ok,
				 NkMatTypeOndeCount(), (a == b) ? 0 : 1, nomme ? 1 : 0));
}

static void CasVoronoiBorneFixe() {
	// ⚠️ Le voisinage de Voronoi est DEROULE sur 3x3, une borne connue a la
	// compilation. Un rayon variable ferait une boucle que certains backends
	// refusent de derouler — donc un shader qui compile ici et pas ailleurs, le
	// defaut le plus penible a diagnostiquer parce qu'il accuse la machine.
	//
	// DISCRIMINE : les bornes litterales doivent etre DANS le code emis, et la
	// fonction ne doit apparaitre QUE si un Voronoi est present.
	NkMatCompileResult avec = CompileProcedural(NK_MN_VORONOI, "distance", "roughness", nullptr);
	NkMatCompileResult sans = CompileProcedural(NK_MN_CHECKER, "color", "base_color", nullptr);
	const bool bornes = avec.ok && ContientSansCasse(avec.source, "for (int j = -1; j <= 1; ++j)") &&
						ContientSansCasse(avec.source, "for (int i = -1; i <= 1; ++i)");
	const bool absenteSansVoronoi = sans.ok && Apres(sans.source, "NkVoronoiF1") < 0;
	const bool presenteAvec = avec.ok && Apres(avec.source, "NkVoronoiF1") > 0;
	// ⚠️ ET SA DEPENDANCE. `NkVoronoiF1` appelle `NkHash22`, qui vient du bloc des
	// briques de bruit. Une mutation retirant Voronoi de la liste des noeuds qui
	// reclament ces briques a SURVECU a la premiere version de ce cas : la
	// fonction etait bien la, son hachage non, et rien ne le disait.
	//
	// Verifier qu'une fonction est emise ne verifie pas que ce qu'elle APPELLE
	// l'est. C'est la meme famille que « chercher un nom n'est pas chercher un
	// usage », prise par l'autre bout.
	const bool dependanceDeclaree = avec.ok && Apres(avec.source, "vec2 NkHash22(vec2 p)") > 0;
	NkString be, err;
	const uint32 ok = avec.ok ? CompileSurLesBackends(avec.source, be, &err) : 0u;
	Cas("procedural/voronoi-voisinage-borne",
		ok == 5 && bornes && absenteSansVoronoi && presenteAvec && dependanceDeclaree,
		NkFormat("{0}| voisinage 3x3 litteral={1} | fonction absente sans Voronoi={2} presente avec={3} | sa "
				 "dependance NkHash22 declaree={4}",
				 be, bornes ? 1 : 0, absenteSansVoronoi ? 1 : 0, presenteAvec ? 1 : 0, dependanceDeclaree ? 1 : 0));
}

static void CasBriquesJointsDecales() {
	// DISCRIMINE : sans le decalage d'une rangee sur deux, on obtient un
	// QUADRILLAGE — un mur parfaitement plausible, et faux. On verifie que le
	// decalage est calcule (`mod(rangee, 2.0) * 0.5`) et qu'il entre bien dans la
	// coordonnee de brique.
	NkNodeId n = NK_NODE_INVALID;
	NkMatCompileResult r = CompileProcedural(NK_MN_BRICK, "color", "base_color", nullptr, &n);
	const NkString dec = NkFormat("n{0}_dec = mod(n{0}_rangee, 2.0) * 0.5", (uint32)n);
	const NkString usage = NkFormat("fract(n{0}_q.x * 0.5 + n{0}_dec)", (uint32)n);
	const bool calcule = r.ok && Apres(r.source, dec.CStr()) > 0;
	// ⚠️ Presence ET usage : un decalage calcule mais jamais employe donnerait
	// exactement le quadrillage qu'on veut eviter, et le premier controle seul
	// passerait. C'est la lecon du Mapping, appliquee ici d'avance.
	const bool employe = r.ok && Apres(r.source, usage.CStr()) > 0;
	NkString be, err;
	const uint32 ok = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;
	Cas("procedural/briques-joints-decales", ok == 5 && calcule && employe,
		NkFormat("{0}| decalage calcule={1} | ET employe dans la coordonnee={2}", be, calcule ? 1 : 0,
				 employe ? 1 : 0));
}

// ═══════════════════════════════════════════════════════════════════════════
//  (a) LES SORTIES NOMMEES, PAR MATERIAU, COTE PROCESSEUR
// ═══════════════════════════════════════════════════════════════════════════

// Pose un noeud de sortie nommee complet. Le nom et l'etage sont des MOTS, et
// l'etage n'a PAS de defaut : il faut le dire a chaque fois, ici comme partout.
static NkNodeId PoseSortie(NkNodeGraph &g, const NkMatTypes &t, const char *nom, const char *etage) {
	const NkNodeId s = NkMatAddNode(gReg, g, NK_MN_OUTPUT_VALUE);
	if (nom)
		g.SetProp(s, NK_MPROP_SORTIE_NOM, NkValueText(t.real, nom));
	if (etage)
		g.SetProp(s, NK_MPROP_SORTIE_ETAGE, NkValueText(t.real, etage));
	return s;
}

static bool Proche(float32 a, float32 b) {
	const float32 d = a - b;
	return (d < 0.f ? -d : d) < 0.0005f;
}

static void CasSortieValeurCalculee() {
	// ⚠️ L'ATTENDU EST CALCULABLE A LA MAIN, et c'est tout l'interet :
	// 0.25 * 4 = 1.0. Un banc qui se contenterait de « une sortie existe »
	// passerait avec un evaluateur qui rend zero.
	NkNodeGraph g;
	NkMatTypes t = NkMatRegisterTypes(g);
	NkNodeId out;
	MonteUnPrincipled(g, t, &out);

	const NkNodeId v = NkMatAddNode(gReg, g, NK_MN_VALUE);
	g.SetProp(v, NK_MPROP_VALUE, NkValueReal(t.real, 0.25f));
	const NkNodeId m = NkMatAddNode(gReg, g, NK_MN_MATH);
	g.SetProp(m, NK_MPROP_OPERATION, NkValueText(t.real, "multiplier"));
	g.Connect(v, "value", m, "a");
	g.SetSocketDefault(m, "b", NkSocketDir::Input, NkValueReal(t.real, 4.0f));
	const NkNodeId s = PoseSortie(g, t, "usure", "par_materiau");
	g.Connect(m, "value", s, "value");

	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	const NkMatSortieMateriau *so = r.ok ? r.TrouveSortie("usure") : nullptr;
	const bool bonne = so && so->composantes == 1 && Proche(so->valeur[0], 1.0f);
	// Une sortie d'etage (a) ne doit produire AUCUNE ligne de shader : c'est
	// ce qui la rend quasi gratuite. Si le nom public apparaissait dans le
	// NkSL, l'argument de cout serait faux sans que rien ne le dise.
	const bool riendansleshader = r.ok && !ContientSansCasse(r.source, "usure");
	// Aucune dependance : la chaine est entierement constante, donc la valeur
	// ne sera JAMAIS a reevaluer.
	const bool constante = so && so->dependDe.Size() == 0;
	Cas("sortie/par-materiau-valeur-calculee", bonne && riendansleshader && constante,
		NkFormat("ok={0} | valeur={1} (attendu 1.0 : 0.25 * 4) | composantes={2} | absente du shader={3} | "
				 "dependances={4} (0 attendu)",
				 r.ok ? 1 : 0, so ? so->valeur[0] : -1.f, so ? so->composantes : 0u,
				 riendansleshader ? 1 : 0, so ? (uint32)so->dependDe.Size() : 999u));
}

static void CasSortieCouleurEtRampe() {
	// Deux choses d'un coup, et la seconde est la plus utile : la rampe est la
	// seule formule non triviale que l'evaluateur reproduit. Rampe par defaut
	// noir -> blanc, lue en 0.5 => gris 0.5 exactement.
	NkNodeGraph g;
	NkMatTypes t = NkMatRegisterTypes(g);
	NkNodeId out;
	MonteUnPrincipled(g, t, &out);

	const NkNodeId ramp = NkMatAddNode(gReg, g, NK_MN_COLOR_RAMP);
	g.SetSocketDefault(ramp, "fac", NkSocketDir::Input, NkValueReal(t.real, 0.5f));
	const NkNodeId s = PoseSortie(g, t, "teinte", "par_materiau");
	g.Connect(ramp, "color", s, "color");

	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	const NkMatSortieMateriau *so = r.ok ? r.TrouveSortie("teinte") : nullptr;
	const bool trois = so && so->composantes == 3;
	const bool gris = so && Proche(so->valeur[0], 0.5f) && Proche(so->valeur[1], 0.5f) &&
					  Proche(so->valeur[2], 0.5f);
	Cas("sortie/couleur-rampe-evaluee", trois && gris,
		NkFormat("composantes={0} (3 attendu) | valeur=({1}, {2}, {3}) attendu (0.5, 0.5, 0.5) : rampe noir "
				 "vers blanc lue en 0.5 | erreur={4}",
				 so ? so->composantes : 0u, so ? so->valeur[0] : -1.f, so ? so->valeur[1] : -1.f,
				 so ? so->valeur[2] : -1.f, r.error));
}

static void CasSortieRefusParPixel() {
	// 🔴 LE CAS QUI JUSTIFIE TOUTE LA PASSE.
	//
	// Le noeud par pixel est a DEUX noeuds d'ecart de la sortie : c'est la
	// situation reelle. Une verification qui ne regarderait que le voisin
	// immediat serait verte ici et laisserait passer le graphe.
	//
	// Et le refus doit NOMMER le coupable : « depend du pixel » tout court
	// obligerait l'auteur a fouiller son graphe entier.
	NkNodeGraph g;
	NkMatTypes t = NkMatRegisterTypes(g);
	NkNodeId out;
	MonteUnPrincipled(g, t, &out);

	const NkNodeId bruit = NkMatAddNode(gReg, g, NK_MN_NOISE);
	const NkNodeId m1 = NkMatAddNode(gReg, g, NK_MN_MATH);
	const NkNodeId m2 = NkMatAddNode(gReg, g, NK_MN_MATH);
	g.Connect(bruit, "fac", m1, "a");
	g.Connect(m1, "value", m2, "a");
	const NkNodeId s = PoseSortie(g, t, "densite", "par_materiau");
	g.Connect(m2, "value", s, "value");

	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	const bool refuse = !r.ok;
	// ⚠️ NE PAS SE CONTENTER DE « mat.bruit APPARAIT DANS LE MESSAGE ».
	// Mutation du 22/08 : en supprimant la CONTAGION, le graphe est quand meme
	// refuse — mais par l EVALUATEUR, qui ne sait pas evaluer un bruit et
	// nomme lui aussi « mat.bruit ». Le cas restait vert en ne mesurant plus
	// rien. On exige donc le MOTIF du refus, pas seulement le nom du coupable :
	// c est la quatrieme fois dans ce chantier que chercher un nom se revele
	// n etre pas chercher un usage.
	const bool bonMotif = Apres(r.error, "depend du pixel") > 0;
	const bool nommeLeCoupable = bonMotif && Apres(r.error, "mat.bruit") > 0;
	const bool nommeLaSortie = Apres(r.error, "densite") > 0;
	// Un refus doit laisser le resultat VIDE. Rendre a la fois une erreur et
	// une source utilisable inviterait un appelant a ignorer l'erreur.
	const bool vide = !r.ok && r.source.Size() == 0 && r.sorties.Size() == 0;
	Cas("sortie/refus-par-pixel-a-deux-noeuds-en-nommant",
		refuse && nommeLeCoupable && nommeLaSortie && vide,
		NkFormat("refuse={0} | nomme le noeud coupable={1} | nomme la sortie={2} | resultat vide={3} | "
				 "message={4}",
				 refuse ? 1 : 0, nommeLeCoupable ? 1 : 0, nommeLaSortie ? 1 : 0, vide ? 1 : 0, r.error));
}

static void CasSortieMappageNEstPasParPixel() {
	// ⚠️ LE CAS QUI DISCRIMINE UNE CLASSIFICATION FAITE A VUE.
	//
	// `Mapping` a tout l'air d'un noeud de texture. Il ne fabrique pourtant
	// aucune coordonnee : il transforme celle qu'on lui donne. Le MEME noeud
	// est donc acceptable ou refuse selon son amont — et c'est la seule facon
	// de prouver que la contagion se propage vraiment au lieu d'etre une
	// liste de noms.
	NkNodeGraph g1;
	NkMatTypes t1 = NkMatRegisterTypes(g1);
	NkNodeId o1;
	MonteUnPrincipled(g1, t1, &o1);
	const NkNodeId mp1 = NkMatAddNode(gReg, g1, NK_MN_MAPPING);
	const float32 trois[3] = {3.f, 0.f, 0.f};
	g1.SetSocketDefault(mp1, "vector", NkSocketDir::Input, NkValueVec(t1.vector, trois, 3));
	const NkNodeId sx1 = NkMatAddNode(gReg, g1, NK_MN_SEPARATE_XYZ);
	g1.Connect(mp1, "vector_out", sx1, "vector");
	const NkNodeId s1 = PoseSortie(g1, t1, "largeur", "par_materiau");
	g1.Connect(sx1, "x", s1, "value");
	NkMatCompileResult r1 = NkMatCompileToNkSL(gReg, g1);
	const NkMatSortieMateriau *so1 = r1.ok ? r1.TrouveSortie("largeur") : nullptr;
	// L'echelle par defaut vaut 1 (neutre MULTIPLICATIF), la position 0 :
	// 3 * 1 + 0 = 3. Un evaluateur qui prendrait 0 comme echelle par defaut
	// rendrait 0, ce qui reste parfaitement plausible pour une largeur.
	const bool accepte = so1 && Proche(so1->valeur[0], 3.f);

	NkNodeGraph g2;
	NkMatTypes t2 = NkMatRegisterTypes(g2);
	NkNodeId o2;
	MonteUnPrincipled(g2, t2, &o2);
	const NkNodeId tc = NkMatAddNode(gReg, g2, NK_MN_TEX_COORD);
	const NkNodeId mp2 = NkMatAddNode(gReg, g2, NK_MN_MAPPING);
	g2.Connect(tc, "uv", mp2, "vector");
	const NkNodeId sx2 = NkMatAddNode(gReg, g2, NK_MN_SEPARATE_XYZ);
	g2.Connect(mp2, "vector_out", sx2, "vector");
	const NkNodeId s2 = PoseSortie(g2, t2, "largeur", "par_materiau");
	g2.Connect(sx2, "x", s2, "value");
	NkMatCompileResult r2 = NkMatCompileToNkSL(gReg, g2);
	// Meme garde que dans le cas precedent, et pour la meme mutation : sans
	// contagion, l evaluateur refuse en nommant « mat.coord_texture » lui aussi.
	const bool refuse = !r2.ok && Apres(r2.error, "depend du pixel") > 0 &&
						Apres(r2.error, "mat.coord_texture") > 0;

	Cas("sortie/mappage-par-contagion-seulement", accepte && refuse,
		NkFormat("mappage sur une constante : accepte={0} valeur={1} (attendu 3.0 = 3 * 1 + 0) | le MEME "
				 "mappage nourri par une coordonnee : refuse en nommant la source={2} | message={3}",
				 accepte ? 1 : 0, so1 ? so1->valeur[0] : -1.f, refuse ? 1 : 0, r2.error));
}

static void CasSortieExposeNeContaminePas() {
	// ⚠️ UN PARAMETRE EXPOSE EST UN UNIFORME, PAS UNE VALEUR PAR PIXEL.
	//
	// Il vaut la meme chose pour tous les pixels du materiau. Le confondre
	// avec une source par pixel interdirait la moitie des usages utiles de
	// l'etage (a) — precisement ceux ou le code de jeu veut relire l'effet
	// d'un reglage qu'il vient d'ecrire.
	//
	// En revanche la sortie DEPEND de lui : sans cette liste, « reevalue au
	// changement de parametre » serait inapplicable et la valeur se figerait
	// sur celle du jour de la compilation, en restant plausible.
	NkNodeGraph g;
	NkMatTypes t = NkMatRegisterTypes(g);
	NkNodeId out;
	const NkNodeId bsdf = MonteUnPrincipled(g, t, &out);
	(void)bsdf;

	const NkNodeId m = NkMatAddNode(gReg, g, NK_MN_MATH);
	g.SetProp(m, NK_MPROP_OPERATION, NkValueText(t.real, "ajouter"));
	g.SetSocketDefault(m, "a", NkSocketDir::Input, NkValueReal(t.real, 0.2f));
	g.SetSocketDefault(m, "b", NkSocketDir::Input, NkValueReal(t.real, 0.5f));
	NkString cle(NK_MPROP_EXPOSE_PREFIX);
	cle.Append("a");
	g.SetProp(m, cle.CStr(), NkValueText(t.real, "usure_globale"));
	const NkNodeId s = PoseSortie(g, t, "somme", "par_materiau");
	g.Connect(m, "value", s, "value");

	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	const NkMatSortieMateriau *so = r.ok ? r.TrouveSortie("somme") : nullptr;
	const bool accepte = so != nullptr;
	// 0.2 + 0.5 = 0.7 : le defaut de la prise exposee EST sa valeur de depart.
	const bool valeur = so && Proche(so->valeur[0], 0.7f);
	const bool depend = so && so->dependDe.Size() == 1 && so->dependDe[0] == NkString("usure_globale");
	Cas("sortie/expose-ne-contamine-pas-mais-cree-une-dependance", accepte && valeur && depend,
		NkFormat("accepte={0} | valeur={1} (attendu 0.7 = defaut expose 0.2 + 0.5) | dependances={2} | "
				 "premiere={3} (attendu usure_globale) | erreur={4}",
				 accepte ? 1 : 0, so ? so->valeur[0] : -1.f, so ? (uint32)so->dependDe.Size() : 999u,
				 (so && so->dependDe.Size() > 0) ? so->dependDe[0] : NkString("-"), r.error));
}

static void CasSelectionEvalueeSurLeProcesseur() {
	// ⚠️ CE CAS EXISTE PARCE QUE LE REPLI ETAIT SUR MAIS PAS JUSTE.
	//
	// L'evaluateur processeur refuse tout noeud qu'il ne connait pas — « noeud
	// non evaluable sur le processeur ». C'est le bon defaut : jamais un zero
	// plausible. Mais il aurait REFUSE toute sortie nommee passant par un
	// `Selectionner`, alors qu'une condition pilotee par un parametre expose
	// est exactement ce qu'une sortie d'etage (a) a vocation a suivre.
	//
	// DISCRIMINE, et c'est le coeur : on mesure LES DEUX BRANCHES depuis le
	// MEME montage, en ne changeant QUE la condition. Un evaluateur qui
	// rendrait toujours `si_vrai` — ou toujours zero — passerait un controle
	// qui n'en regarderait qu'une.
	struct Essai {
			float32 condition;
			float32 attendu;
			const char *quoi;
	};
	const Essai essais[4] = {
		{1.0f, 0.9f, "1 -> si_vrai"},
		{0.0f, 0.1f, "0 -> si_faux"},
		// Les deux qui encadrent le seuil. Sans elles, « > 0.5 » et
		// « >= 0.5 » seraient indiscernables, et un seuil pose a 0.0 aussi.
		{0.51f, 0.9f, "0.51 -> si_vrai"},
		{0.49f, 0.1f, "0.49 -> si_faux"},
	};
	uint32 bons = 0;
	NkString detail;
	for (uint32 i = 0; i < 4; ++i) {
		NkNodeGraph g;
		NkMatTypes t = NkMatRegisterTypes(g);
		NkNodeId out;
		MonteUnPrincipled(g, t, &out);
		const NkNodeId sel = NkMatAddNode(gReg, g, NK_MN_SELECT);
		g.SetSocketDefault(sel, "condition", NkSocketDir::Input, NkValueReal(t.real, essais[i].condition));
		g.SetSocketDefault(sel, "si_vrai", NkSocketDir::Input, NkValueReal(t.real, 0.9f));
		g.SetSocketDefault(sel, "si_faux", NkSocketDir::Input, NkValueReal(t.real, 0.1f));
		const NkNodeId so = PoseSortie(g, t, "choisi", "par_materiau");
		g.Connect(sel, "valeur", so, "value");
		NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
		const NkMatSortieMateriau *s = r.ok ? r.TrouveSortie("choisi") : nullptr;
		const bool ok = s && Proche(s->valeur[0], essais[i].attendu);
		if (ok)
			++bons;
		detail.Append(NkFormat("{0}={1}{2} | ", NkString(essais[i].quoi), s ? s->valeur[0] : -1.f,
							   NkString(ok ? "" : " FAUX")));
	}

	// ⚠️ ET LA DEPENDANCE SUR LES DEUX BRANCHES, PAS SEULEMENT SUR LA PRISE.
	//
	// Un evaluateur qui n'aurait lu que la branche retenue aurait fige
	// `dependDe` sur celle du jour de la compilation : le jour ou la condition
	// bascule, la sortie cesserait d'etre reevaluee et se figerait sur une
	// valeur PLAUSIBLE. On expose donc les deux branches ET la condition, et
	// l'on exige les TROIS noms.
	NkNodeGraph g2;
	NkMatTypes t2 = NkMatRegisterTypes(g2);
	NkNodeId out2;
	MonteUnPrincipled(g2, t2, &out2);
	const NkNodeId sel2 = NkMatAddNode(gReg, g2, NK_MN_SELECT);
	g2.SetSocketDefault(sel2, "condition", NkSocketDir::Input, NkValueReal(t2.real, 1.0f));
	g2.SetSocketDefault(sel2, "si_vrai", NkSocketDir::Input, NkValueReal(t2.real, 0.9f));
	g2.SetSocketDefault(sel2, "si_faux", NkSocketDir::Input, NkValueReal(t2.real, 0.1f));
	Expose(g2, t2, sel2, "condition", "mode");
	Expose(g2, t2, sel2, "si_vrai", "chaud");
	Expose(g2, t2, sel2, "si_faux", "froid");
	const NkNodeId so2 = PoseSortie(g2, t2, "choisi", "par_materiau");
	g2.Connect(sel2, "valeur", so2, "value");
	NkMatCompileResult r2 = NkMatCompileToNkSL(gReg, g2);
	const NkMatSortieMateriau *s2 = r2.ok ? r2.TrouveSortie("choisi") : nullptr;
	const bool troisDeps = s2 && s2->dependDe.Size() == 3u;

	Cas("selection/evaluee-sur-le-processeur-et-les-deux-branches-comptent", bons == 4u && troisDeps,
		NkFormat("{0}{1}/4 justes | dependances={2} (3 attendues : la condition ET LES DEUX branches, sinon la "
				 "sortie se fige le jour ou la condition bascule) | erreur={3}",
				 detail, bons, s2 ? (uint32)s2->dependDe.Size() : 999u, r2.error));
}

static void CasSortieEtagesRefuses() {
	// Les deux etages non construits refusent EN SE NOMMANT, et l'absence
	// d'etage refuse aussi : il n'y a pas de defaut. Se replier sur (a) quand
	// la propriete manque transformerait silencieusement une sortie voulue par
	// pixel en constante calculee une seule fois.
	struct Jeu {
			const char *etage;
			const char *attendu;
	};
	// ⚠️ `par_pixel_cible` A QUITTE CETTE LISTE le 2026-08-22 : (b1) est
	// construit, il ne se refuse plus. Ce cas ne surveille QUE ce qui reste
	// indisponible -- et cette collection RETRECIT par conception, un etage a la
	// fois. Le compte se derive donc du tableau (`sizeof`), il n'est pas recopie
	// a cote : retirer une ligne ne doit pas obliger a corriger un nombre
	// ailleurs, sous peine de le corriger un jour sans regarder.
	const Jeu jeux[] = {{nullptr, "etage non renseigne"},
						{"par_pixel_processeur", "par_pixel_processeur"},
						{"par_pixel_magique", "par_pixel_magique"}};
	const uint32 nJeux = (uint32)(sizeof(jeux) / sizeof(jeux[0]));
	uint32 bons = 0;
	NkString detail;
	for (uint32 i = 0; i < nJeux; ++i) {
		NkNodeGraph g;
		NkMatTypes t = NkMatRegisterTypes(g);
		NkNodeId out;
		MonteUnPrincipled(g, t, &out);
		const NkNodeId v = NkMatAddNode(gReg, g, NK_MN_VALUE);
		g.SetProp(v, NK_MPROP_VALUE, NkValueReal(t.real, 1.f));
		const NkNodeId s = PoseSortie(g, t, "essai", jeux[i].etage);
		g.Connect(v, "value", s, "value");
		NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
		const bool ok = !r.ok && Apres(r.error, jeux[i].attendu) > 0;
		if (ok)
			++bons;
		detail.Append(NkFormat("[{0} -> refus nomme={1}] ", NkString(jeux[i].etage ? jeux[i].etage : "(absent)"),
							   ok ? 1 : 0));
	}
	// Et (b2) doit dire POURQUOI, pas seulement « indisponible » : la
	// relecture synchrone se manifeste par « le jeu rame », jamais par « la
	// relecture est lente », et c'est ce qui coute des semaines a imputer.
	NkNodeGraph g;
	NkMatTypes t = NkMatRegisterTypes(g);
	NkNodeId out;
	MonteUnPrincipled(g, t, &out);
	const NkNodeId v = NkMatAddNode(gReg, g, NK_MN_VALUE);
	const NkNodeId s = PoseSortie(g, t, "essai", "par_pixel_processeur");
	g.Connect(v, "value", s, "value");
	NkMatCompileResult rb2 = NkMatCompileToNkSL(gReg, g);
	const bool ditPourquoi = Apres(rb2.error, "SYNCHRONE") > 0 || Apres(rb2.error, "synchrone") > 0;

	Cas("sortie/etages-non-construits-refusent-en-se-nommant", bons == nJeux && ditPourquoi,
		NkFormat("{0}| (b2) dit pourquoi={1}", detail, ditPourquoi ? 1 : 0));
}

static void CasSortieSourcesEtNoms() {
	// Zero source, deux sources, nom invalide, nom en double : quatre etats
	// qu'aucune interface n'empechera puisqu'il n'y en a pas encore.
	uint32 bons = 0;
	NkString detail;

	{ // aucune source
		NkNodeGraph g;
		NkMatTypes t = NkMatRegisterTypes(g);
		NkNodeId out;
		MonteUnPrincipled(g, t, &out);
		PoseSortie(g, t, "vide", "par_materiau");
		NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
		const bool ok = !r.ok && Apres(r.error, "aucune source") > 0;
		bons += ok ? 1 : 0;
		detail.Append(NkFormat("[aucune source refusee={0}] ", ok ? 1 : 0));
	}
	{ // deux sources
		NkNodeGraph g;
		NkMatTypes t = NkMatRegisterTypes(g);
		NkNodeId out;
		MonteUnPrincipled(g, t, &out);
		const NkNodeId v = NkMatAddNode(gReg, g, NK_MN_VALUE);
		const NkNodeId c = NkMatAddNode(gReg, g, NK_MN_RGB);
		const NkNodeId s = PoseSortie(g, t, "double_source", "par_materiau");
		g.Connect(v, "value", s, "value");
		g.Connect(c, "color", s, "color");
		NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
		const bool ok = !r.ok && Apres(r.error, "DEUX sources") > 0;
		bons += ok ? 1 : 0;
		detail.Append(NkFormat("[deux sources refusees={0}] ", ok ? 1 : 0));
	}
	{ // nom invalide
		NkNodeGraph g;
		NkMatTypes t = NkMatRegisterTypes(g);
		NkNodeId out;
		MonteUnPrincipled(g, t, &out);
		const NkNodeId v = NkMatAddNode(gReg, g, NK_MN_VALUE);
		const NkNodeId s = PoseSortie(g, t, "2 mots", "par_materiau");
		g.Connect(v, "value", s, "value");
		NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
		const bool ok = !r.ok && Apres(r.error, "nom absent ou invalide") > 0;
		bons += ok ? 1 : 0;
		detail.Append(NkFormat("[nom invalide refuse={0}] ", ok ? 1 : 0));
	}
	{ // deux sorties du meme nom
		NkNodeGraph g;
		NkMatTypes t = NkMatRegisterTypes(g);
		NkNodeId out;
		MonteUnPrincipled(g, t, &out);
		const NkNodeId v1 = NkMatAddNode(gReg, g, NK_MN_VALUE);
		const NkNodeId v2 = NkMatAddNode(gReg, g, NK_MN_VALUE);
		const NkNodeId s1 = PoseSortie(g, t, "meme", "par_materiau");
		const NkNodeId s2 = PoseSortie(g, t, "meme", "par_materiau");
		g.Connect(v1, "value", s1, "value");
		g.Connect(v2, "value", s2, "value");
		NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
		const bool ok = !r.ok && Apres(r.error, "deux sorties portent le nom") > 0;
		bons += ok ? 1 : 0;
		detail.Append(NkFormat("[doublon refuse={0}] ", ok ? 1 : 0));
	}
	Cas("sortie/sources-et-noms-quatre-refus", bons == 4, detail);
}

static void CasSortiePlusieursEtGraphesExistants() {
	// Deux controles que rien d'autre ne couvre :
	//   - plusieurs sorties coexistent et se retrouvent CHACUNE par son nom ;
	//   - un graphe SANS aucune sortie nommee — c'est-a-dire tous les graphes
	//     ecrits jusqu'ici — rend une liste VIDE et compile comme avant. Une
	//     passe neuve qui casserait l'existant se verrait ici, et nulle part
	//     ailleurs dans ce banc.
	NkNodeGraph g;
	NkMatTypes t = NkMatRegisterTypes(g);
	NkNodeId out;
	MonteUnPrincipled(g, t, &out);
	const NkNodeId a = NkMatAddNode(gReg, g, NK_MN_VALUE);
	g.SetProp(a, NK_MPROP_VALUE, NkValueReal(t.real, 0.125f));
	const NkNodeId b = NkMatAddNode(gReg, g, NK_MN_RGB);
	const float32 rouge[3] = {0.9f, 0.1f, 0.2f};
	g.SetProp(b, NK_MPROP_COLOR, NkValueVec(t.color, rouge, 3));
	const NkNodeId s1 = PoseSortie(g, t, "opacite", "par_materiau");
	const NkNodeId s2 = PoseSortie(g, t, "teinte_dominante", "par_materiau");
	g.Connect(a, "value", s1, "value");
	g.Connect(b, "color", s2, "color");
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	const NkMatSortieMateriau *o1 = r.ok ? r.TrouveSortie("opacite") : nullptr;
	const NkMatSortieMateriau *o2 = r.ok ? r.TrouveSortie("teinte_dominante") : nullptr;
	const bool deux = r.sorties.Size() == 2 && o1 && o2;
	const bool valeurs = o1 && o2 && Proche(o1->valeur[0], 0.125f) && Proche(o2->valeur[0], 0.9f) &&
						 Proche(o2->valeur[2], 0.2f);
	// Un nom absent doit rendre nullptr, pas la premiere sortie venue.
	const bool absentEstNul = r.TrouveSortie("nexiste_pas") == nullptr;

	NkNodeGraph g0;
	NkMatTypes t0 = NkMatRegisterTypes(g0);
	NkNodeId out0;
	MonteUnPrincipled(g0, t0, &out0);
	NkMatCompileResult r0 = NkMatCompileToNkSL(gReg, g0);
	const bool existantIntact = r0.ok && r0.sorties.Size() == 0 && r0.source.Size() > 100;

	Cas("sortie/plusieurs-et-graphes-sans-sortie", deux && valeurs && absentEstNul && existantIntact,
		NkFormat("deux sorties retrouvees par nom={0} | valeurs justes={1} | nom absent rend nul={2} | "
				 "graphe sans sortie nommee intact={3} (liste={4}, shader={5} octets)",
				 deux ? 1 : 0, valeurs ? 1 : 0, absentEstNul ? 1 : 0, existantIntact ? 1 : 0,
				 (uint32)r0.sorties.Size(), (uint32)r0.source.Size()));
}


static void CasSortieDivisionParZero() {
	// ⚠️ LE SEUL ENDROIT DU CHANTIER OU DEUX IMPLEMENTATIONS CALCULENT LA MEME
	// CHOSE — le shader et l evaluateur processeur — ET OU LEUR DESACCORD SERAIT
	// INVISIBLE. Personne ne compare la valeur rendue au code de jeu avec ce que
	// le pixel affiche : les deux resteraient plausibles chacune de son cote.
	//
	// Le shader garde la division et rend 0. Si l evaluateur divisait nu, il
	// rendrait un NaN — et un NaN se compare faux a TOUT, y compris a lui-meme,
	// donc le code de jeu prendrait des decisions inversees sans qu aucune
	// erreur ne soit levee.
	//
	// Mutation du 22/08 : ce cas a ete ecrit APRES avoir constate qu en retirant
	// la garde AUCUN des huit autres cas ne tombait.
	NkNodeGraph g;
	NkMatTypes t = NkMatRegisterTypes(g);
	NkNodeId out;
	MonteUnPrincipled(g, t, &out);
	const NkNodeId m = NkMatAddNode(gReg, g, NK_MN_MATH);
	g.SetProp(m, NK_MPROP_OPERATION, NkValueText(t.real, "diviser"));
	g.SetSocketDefault(m, "a", NkSocketDir::Input, NkValueReal(t.real, 1.f));
	g.SetSocketDefault(m, "b", NkSocketDir::Input, NkValueReal(t.real, 0.f));
	const NkNodeId s = PoseSortie(g, t, "quotient", "par_materiau");
	g.Connect(m, "value", s, "value");
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	const NkMatSortieMateriau *so = r.ok ? r.TrouveSortie("quotient") : nullptr;
	const bool zero = so && so->valeur[0] == 0.f;
	// Un NaN se detecte par sa seule propriete stable : il differe de lui-meme.
	// Tester « == 0 » seul ne suffirait pas a le NOMMER dans le detail.
	const bool nan = so && !(so->valeur[0] == so->valeur[0]);
	// Et le shader, lui, garde bien la division : les deux doivent s accorder.
	const bool gardeDansLeShader = r.ok && ContientSansCasse(r.source, "== 0.0");
	Cas("sortie/division-par-zero-accorde-avec-le-shader", zero && !nan && gardeDansLeShader,
		NkFormat("valeur={0} (attendu 0, comme la garde du shader) | est un NaN={1} | le shader garde aussi "
				 "la division={2}",
				 so ? so->valeur[0] : -1.f, nan ? 1 : 0, gardeDansLeShader ? 1 : 0));
}

// ═══════════════════════════════════════════════════════════════════════════
//  RANG 3 : L'OUTILLAGE
// ═══════════════════════════════════════════════════════════════════════════

// Monte un noeud du rang 3 vers une prise du Principled, compile, rend le tout.
static NkMatCompileResult CompileOutil(const char *cle, const char *prise, const char *cible,
									   const char *propCle, const char *propVal) {
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	NkNodeId out;
	const NkNodeId bsdf = MonteUnPrincipled(g, t, &out);
	const NkNodeId n = NkMatAddNode(gReg, g, cle);
	if (propCle && propVal)
		g.SetProp(n, propCle, NkValueText(t.real, propVal));
	g.Connect(n, prise, bsdf, cible);
	return NkMatCompileToNkSL(gReg, g);
}

// ── SELECTIONNER : LA CONDITION DE VALEUR ────────────────────────────────────

// Monte les deux variantes dans UN SEUL graphe : le reel pilote `roughness`,
// la couleur pilote `base_color`. Les deux doivent traverser ensemble — un
// montage par variante aurait laisse passer une collision de noms de locales.
static NkNodeId MonteDeuxSelections(NkNodeGraph &g, const NkMatTypes &t, NkNodeId *outSelReel,
									NkNodeId *outSelCoul) {
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	g.Connect(bsdf, "bsdf", out, "surface");
	const NkNodeId sr = NkMatAddNode(gReg, g, NK_MN_SELECT);
	const NkNodeId sc = NkMatAddNode(gReg, g, NK_MN_SELECT_COLOR);
	g.SetSocketDefault(sr, "si_vrai", NkSocketDir::Input, NkValueReal(t.real, 0.9f));
	g.SetSocketDefault(sr, "si_faux", NkSocketDir::Input, NkValueReal(t.real, 0.1f));
	const float32 chaud[3] = {0.9f, 0.3f, 0.1f};
	const float32 froid[3] = {0.1f, 0.3f, 0.9f};
	g.SetSocketDefault(sc, "si_vrai", NkSocketDir::Input, NkValueVec(t.color, chaud, 3));
	g.SetSocketDefault(sc, "si_faux", NkSocketDir::Input, NkValueVec(t.color, froid, 3));
	// ⚠️ LES DEUX SORTIES SONT BRANCHEES. Un Selectionner dont personne ne lit
	// la sortie serait emis puis ignore, et le cas mesurerait l'emission d'un
	// noeud mort — pas le fait que sa locale ARRIVE quelque part.
	g.Connect(sr, "valeur", bsdf, "roughness");
	g.Connect(sc, "couleur", bsdf, "base_color");
	if (outSelReel)
		*outSelReel = sr;
	if (outSelCoul)
		*outSelCoul = sc;
	return bsdf;
}

static void CasSelectionCompileEtBranche() {
	// 🔴 LE RISQUE PRECIS DE CE NOEUD : c'est le PREMIER du compilateur a emettre
	// un « if » de STATEMENT. Tout le reste du fichier n'emet que des
	// expressions. Le dialecte NkSL porte trois contraintes connues (pas de
	// struct locale, pas de retour de struct, pas de varying dans un helper) et
	// AUCUNE ne parle des branches — ce qui ne prouve rien. Ce cas le mesure au
	// lieu de le supposer, et c'est glslang qui tranche : les quatre autres
	// colonnes n'attestent que la GENERATION.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	NkNodeId sr = NK_NODE_INVALID, sc = NK_NODE_INVALID;
	MonteDeuxSelections(g, t, &sr, &sc);
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	NkString be, err;
	const uint32 ok = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;

	// La forme emise : une branche, pas un melange. Si quelqu'un remplace un
	// jour le « if » par « mix(b, a, step(...)) », l'image reste identique — et
	// l'argument « bon marche sur une condition uniforme » disparait sans que
	// rien ne rougisse. Ce cas est le seul garde-fou de cet argument.
	const bool branche = r.ok && Apres(r.source, "    if ((") > 0 && Apres(r.source, "} else {") > 0;
	const bool pasDeMelange = r.ok && Apres(r.source, "step(") < 0;

	// ⚠️ LA LOCALE EST DECLAREE AVANT LA BRANCHE, et c'est ce qui la rend
	// lisible par les noeuds suivants. Une declaration a l'interieur des
	// accolades compilerait cote generateur et tomberait chez glslang.
	NkString decl("float ");
	{
		char b[32];
		snprintf(b, sizeof(b), "n%u_valeur = 0.0;", (unsigned)sr);
		decl.Append(b);
	}
	const bool declareeAvant = r.ok && Apres(r.source, decl.CStr()) > 0;

	// Les deux variantes coexistent : un reel ET une couleur dans le meme shader.
	const bool lesDeux = r.ok && Apres(r.source, "vec3 n") > 0 && declareeAvant;

	// 🔴 ET LES DEUX COTES SONT DU BON COTE. Sans ceci, echanger `si_vrai` et
	// `si_faux` a l'emission passerait TOUS les controles ci-dessus : la branche
	// existe, elle compile, la locale survit — et le materiau rend l'inverse de
	// ce que son auteur a cable. Un defaut qui produit une image parfaitement
	// credible, donc le plus cher a debusquer.
	//
	// On mesure par les POSITIONS : 0.9 (si_vrai) doit apparaitre APRES le `if`
	// et AVANT le `else`, et 0.1 (si_faux) APRES le `else`.
	const int32 pIf = r.ok ? Apres(r.source, "    if ((") : -1;
	const int32 pElse = r.ok ? Apres(r.source, "} else {") : -1;
	const int32 pVrai = r.ok ? Apres(r.source, "0.899999976") : -1;
	const int32 pFaux = r.ok ? Apres(r.source, "0.100000001") : -1;
	const bool bonCote = pIf > 0 && pElse > pIf && pVrai > pIf && pVrai < pElse && pFaux > pElse;

	Cas("selection/deux-variantes-compilent-et-branchent",
		r.ok && ok == 5 && branche && pasDeMelange && lesDeux && bonCote,
		NkFormat("{0}| un vrai if/else emis={1} | aucun step (donc pas un melange deguise)={2} | la locale est "
				 "DECLAREE avant la branche, sinon elle n'y survit pas={3} | reel et couleur dans le meme "
				 "shader={4} | si_vrai DANS le if et si_faux DANS le else (un echange rendrait l'inverse, "
				 "credible)={5} if@{6} else@{7} 0.9@{8} 0.1@{9} | {10}",
				 be, branche ? 1 : 0, pasDeMelange ? 1 : 0, declareeAvant ? 1 : 0, lesDeux ? 1 : 0,
				 bonCote ? 1 : 0, (uint32)(pIf < 0 ? 0 : pIf), (uint32)(pElse < 0 ? 0 : pElse),
				 (uint32)(pVrai < 0 ? 0 : pVrai), (uint32)(pFaux < 0 ? 0 : pFaux), err));
}

static void CasSelectionSeuilEtRefusDuZero() {
	// ⚠️ LE SEUIL EST 0.5, ET IL VIENT DE LA CONSTANTE.
	//
	// Il n'existe pas de type booleen : la condition est un REEL, et le gameplay
	// y ecrira 0 ou 1. Un seuil a « different de 0 » basculerait sur 1e-30 — le
	// residu d'un calcul qui a sous-deborde — et l'auteur lirait une valeur
	// credible. 0.5 est le seul seuil equidistant des deux valeurs attendues.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	MonteDeuxSelections(g, t, nullptr, nullptr);
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	const bool seuil = r.ok && Apres(r.source, ") > 0.5)") > 0;
	// Le TEMOIN qui empeche le cas de se contenter du chiffre : aucune
	// comparaison a zero ne doit subsister. Un « > 0.0 » passerait la ligne
	// ci-dessus si un seul des deux noeuds portait le bon seuil.
	const bool pasDeZero = r.ok && Apres(r.source, ") > 0.0)") < 0;
	// ET LE SEUIL EST LE MEME DES DEUX COTES : deux noeuds, deux occurrences.
	uint32 n = 0;
	if (r.ok) {
		for (const char *q = r.source.CStr(); *q; ++q) {
			const char *a = q;
			const char *b = ") > 0.5)";
			while (*b && *a == *b) {
				++a;
				++b;
			}
			if (*b == 0)
				++n;
		}
	}
	Cas("selection/seuil-a-0.5-jamais-a-zero", r.ok && seuil && pasDeZero && n == 2u,
		NkFormat("seuil 0.5 emis={0} | aucune comparaison a zero={1} | occurrences={2} (2 attendues : les DEUX "
				 "noeuds portent le meme seuil, tire de NK_SELECT_SEUIL)",
				 seuil ? 1 : 0, pasDeZero ? 1 : 0, n));
}

static void CasSelectionUniformeEtCeQuElleNEconomisePas() {
	// ═══════════════════════════════════════════════════════════════════════
	// 📌 CE QUE CE CAS DOCUMENTE, ET QUI N'EST PAS UN CONTROLE DE PLUS
	// ═══════════════════════════════════════════════════════════════════════
	//
	// Un branchement sur une valeur UNIFORME est bon marche : tous les pixels du
	// tirage prennent la meme branche, la carte n'en execute qu'une. C'est un
	// branchement PAR PIXEL qui coute les deux. Sans cette note ecrite a cote du
	// noeud, quelqu'un le croira cher — « les branchements coutent cher sur
	// GPU » se transmet toujours sans sa condition.
	//
	// 🔴 ET LA MOITIE QUE PERSONNE N'ECRIT JAMAIS : ce noeud N'ECONOMISE PAS le
	// calcul de la branche non prise. Le compilateur emet chaque noeud comme une
	// locale, dans l'ordre topologique — les deux cotes sont calcules AVANT la
	// branche. Le cas le MESURE au lieu de le supposer : il place un bruit
	// couteux sur le cote « si_faux » et verifie que son appel apparait AVANT le
	// « if » dans la source emise. Tant que c'est vrai, promettre l'economie
	// serait vendre une optimisation qui n'a pas lieu.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	g.Connect(bsdf, "bsdf", out, "surface");
	const NkNodeId sel = NkMatAddNode(gReg, g, NK_MN_SELECT);
	const NkNodeId bruit = NkMatAddNode(gReg, g, NK_MN_NOISE);
	g.SetSocketDefault(sel, "si_vrai", NkSocketDir::Input, NkValueReal(t.real, 0.2f));
	g.Connect(bruit, "fac", sel, "si_faux"); // le cote COUTEUX
	g.Connect(sel, "valeur", bsdf, "roughness");
	// La condition est un PARAMETRE EXPOSE : c'est le cas d'usage vise, celui ou
	// la carte ne prend qu'un chemin.
	Expose(g, t, sel, "condition", "mode_use");
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);

	// 1. La condition lit bien le BLOC UNIFORME, et pas une varying : c'est ce
	//    qui rend la branche uniforme, et donc bon marche.
	const bool conditionUniforme = r.ok && Apres(r.source, "if ((nkParams.mode_use)") > 0;
	// 2. Le bruit — le cote couteux — est calcule AVANT le « if ». C'est la
	//    limitation, mesuree et non supposee.
	const int32 posBruit = r.ok ? Apres(r.source, "NkFBM2D((") : -1;
	const int32 posIf = r.ok ? Apres(r.source, "    if ((") : -1;
	const bool calculeAvant = posBruit > 0 && posIf > 0 && posBruit < posIf;
	NkString be, err;
	const uint32 ok = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;
	Cas("selection/condition-uniforme-mais-les-deux-cotes-sont-calcules",
		r.ok && ok == 5 && conditionUniforme && calculeAvant,
		NkFormat("{0}| la condition lit le bloc uniforme (branche BON MARCHE : un seul chemin)={1} | le cote "
				 "couteux est calcule AVANT le if (le noeud CHOISIT, il ne SAUTE pas)={2} bruit@{3} if@{4}",
				 be, conditionUniforme ? 1 : 0, calculeAvant ? 1 : 0, (uint32)(posBruit < 0 ? 0 : posBruit),
				 (uint32)(posIf < 0 ? 0 : posIf)));
}

static void CasMapRangeEtBornage() {
	// ⚠️ L'ATTENDU EST L'ARITHMETIQUE, PAS UNE RESSEMBLANCE. `Map Range` doit
	// emettre une interpolation entre ses deux bornes de sortie, et surtout une
	// GARDE sur la plage d'entree vide : `from_min == from_max` donnerait un NaN,
	// et un NaN contamine tout l'aval en se voyant noir ici et blanc la selon le
	// backend — un defaut qui fait accuser la machine.
	NkMatCompileResult r = CompileOutil(NK_MN_MAP_RANGE, "result", "roughness", nullptr, nullptr);
	const bool garde = r.ok && Apres(r.source, "1e-8") > 0;
	const bool interpole = r.ok && Apres(r.source, "mix(") > 0;
	// DISCRIMINE le mode : `serre` (defaut) DOIT poser un clamp, `libre` NON.
	// Verifier seulement que le defaut compile laisserait passer un mode qui ne
	// changerait rien — et un mode qui ne change rien est un mode absent.
	NkMatCompileResult rs = CompileOutil(NK_MN_MAP_RANGE, "result", "roughness", NK_MPROP_BORNAGE, "serre");
	NkMatCompileResult rl = CompileOutil(NK_MN_MAP_RANGE, "result", "roughness", NK_MPROP_BORNAGE, "libre");
	const bool serreBorne = rs.ok && Apres(rs.source, "clamp(") > 0;
	const bool libreNeBornePas = rl.ok && Apres(rl.source, "clamp(") < 0;
	// Le defaut ABSENT doit valoir `serre`, comme chez Blender.
	const bool defautEstSerre = r.ok && Apres(r.source, "clamp(") > 0;
	NkMatCompileResult ri = CompileOutil(NK_MN_MAP_RANGE, "result", "roughness", NK_MPROP_BORNAGE, "elastique");
	const bool refuseNomme = !ri.ok && Apres(ri.error, "elastique") > 0;
	NkString be, err;
	const uint32 ok = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;
	Cas("outil/plage-garde-et-bornage",
		r.ok && ok == 5 && garde && interpole && serreBorne && libreNeBornePas && defautEstSerre && refuseNomme,
		NkFormat("{0}| garde de plage vide={1} | interpolation={2} | serre borne={3} | libre ne borne "
				 "PAS={4} | defaut absent = serre={5} | mode inconnu refuse en le nommant={6}",
				 be, garde ? 1 : 0, interpole ? 1 : 0, serreBorne ? 1 : 0, libreNeBornePas ? 1 : 0,
				 defautEstSerre ? 1 : 0, refuseNomme ? 1 : 0));
}

static void CasClampBornesInversees() {
	// ⚠️ `clamp(v, min, max)` est INDEFINI en GLSL quand min > max, et chaque
	// pilote choisit sa reponse. Un materiau aux bornes inversees rendrait alors
	// une chose sur une machine et une autre ailleurs — encore un defaut qui
	// fait accuser la carte.
	//
	// DISCRIMINE : on exige que le code emis N'EMPLOIE PAS `clamp(` mais la
	// paire min/max. Verifier seulement « ca compile » laisserait passer le
	// `clamp` nu, qui compile parfaitement.
	NkMatCompileResult r = CompileOutil(NK_MN_CLAMP, "result", "roughness", nullptr, nullptr);
	const bool paire = r.ok && Apres(r.source, "min(max(") > 0;
	const bool pasDeClampNu = r.ok && Apres(r.source, "clamp(") < 0;
	NkString be, err;
	const uint32 ok = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;
	Cas("outil/borner-defini-meme-bornes-inversees", r.ok && ok == 5 && paire && pasDeClampNu,
		NkFormat("{0}| emploie min(max(...))={1} | n emploie PAS clamp( nu={2} (indefini si min > max)", be,
				 paire ? 1 : 0, pasDeClampNu ? 1 : 0));
}

static void CasCombineXYZ() {
	// Le pendant de Separate XYZ. DISCRIMINE par l'ALLER-RETOUR : on separe un
	// vecteur puis on le recombine, et les trois composantes doivent voyager
	// SEPAREMENT — un Combine qui lirait trois fois la meme prise compilerait
	// et rendrait un gris parfaitement plausible.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	NkNodeId out;
	const NkNodeId bsdf = MonteUnPrincipled(g, t, &out);
	const NkNodeId sep = NkMatAddNode(gReg, g, NK_MN_SEPARATE_XYZ);
	const NkNodeId com = NkMatAddNode(gReg, g, NK_MN_COMBINE_XYZ);
	const float32 v[3] = {0.25f, 0.5f, 0.75f};
	g.SetSocketDefault(sep, "vector", NkSocketDir::Input, NkValueVec(t.vector, v, 3));
	g.Connect(sep, "x", com, "x");
	g.Connect(sep, "y", com, "y");
	g.Connect(sep, "z", com, "z");
	g.Connect(com, "vector", bsdf, "base_color");
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	if (getenv("NK_DUMP_COMB") && r.ok) {
		FILE *f = fopen("mesures_matgraph/combine.nksl", "wb");
		if (f) { fwrite(r.source.CStr(), 1, (size_t)r.source.Size(), f); fclose(f); }
	}
	// ⚠️ ON LIT LA LIGNE DU COMBINE, PAS TOUTE LA SOURCE.
	//
	// Premiere version de ce cas : « _x, _y et _z apparaissent dans la
	// source ». Elle a SURVECU a la mutation qui fait lire trois fois la
	// prise `x` au Combine — parce que les trois locales viennent du
	// SEPARATE, qui les emet de toute facon. Le cas verifiait la presence de
	// trois NOMS, jamais leur USAGE. Quatrieme occurrence de cette faute dans
	// le chantier, et la plus facile a commettre : les noms etaient bien la.
	//
	// Les trois motifs ci-dessous n existent QUE dans l appel du Combine :
	// « vec3((n3_x), (n3_y), (n3_z)) ». Une prise lue deux fois casse au
	// moins l un des trois.
	const bool trois = r.ok && Apres(r.source, "_x), (") > 0 && Apres(r.source, "_y), (") > 0 &&
			   Apres(r.source, "_z))") > 0;
	const bool combine = r.ok && Apres(r.source, "vec3((") > 0;
	NkString be, err;
	const uint32 ok = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;
	Cas("outil/combiner-xyz-aller-retour", r.ok && ok == 5 && trois && combine,
		NkFormat("{0}| les trois composantes voyagent separement={1} | recombinaison emise={2}", be,
				 trois ? 1 : 0, combine ? 1 : 0));
}

static void CasVectorMathOperationsEtAccord() {
	// Les douze operations compilent, ET la prise lue doit s'accorder avec
	// l'operation : trois d'entre elles rendent un SCALAIRE.
	uint32 bons = 0;
	NkString detail;
	for (uint32 i = 0; i < NkMatOpVecteurCount(); ++i) {
		const NkMatOperation *op = NkMatOpVecteurAt(i);
		const bool scalaire = NkMatOpVecteurRendUnScalaire(i);
		NkMatCompileResult r = CompileOutil(NK_MN_VECTOR_MATH, scalaire ? "value" : "vector",
											scalaire ? "roughness" : "base_color", NK_MPROP_OPERATION, op->cle);
		NkString be, err;
		const uint32 ok = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;
		if (r.ok && ok == 5)
			++bons;
		else
			detail.Append(NkFormat("[{0} KO : {1}] ", NkString(op->cle), r.ok ? be : r.error));
	}
	// 🔴 LE CONTROLE QUI COMPTE : lire la MAUVAISE prise doit etre REFUSE, en
	// nommant l'operation ET la prise. Sans ce refus le shader compilerait — la
	// prise inutilisee porte le neutre — et l'auteur lirait un vecteur NUL
	// parfaitement plausible, sur un graphe de trente noeuds.
	NkMatCompileResult mauvais1 =
		CompileOutil(NK_MN_VECTOR_MATH, "vector", "base_color", NK_MPROP_OPERATION, "longueur");
	NkMatCompileResult mauvais2 =
		CompileOutil(NK_MN_VECTOR_MATH, "value", "roughness", NK_MPROP_OPERATION, "normaliser");
	const bool refus1 =
		!mauvais1.ok && Apres(mauvais1.error, "longueur") > 0 && Apres(mauvais1.error, "vector") > 0;
	const bool refus2 =
		!mauvais2.ok && Apres(mauvais2.error, "normaliser") > 0 && Apres(mauvais2.error, "value") > 0;
	NkMatCompileResult inc =
		CompileOutil(NK_MN_VECTOR_MATH, "vector", "base_color", NK_MPROP_OPERATION, "produit_mixte");
	const bool refusInc = !inc.ok && Apres(inc.error, "produit_mixte") > 0;
	Cas("outil/math-vecteur-operations-et-accord-de-prise",
		bons == NkMatOpVecteurCount() && refus1 && refus2 && refusInc,
		NkFormat("{0}/{1} operations compilent sur les 5 verdicts | {2}| lire vector apres longueur REFUSE "
				 "en le nommant={3} | lire value apres normaliser REFUSE={4} | operation inconnue refusee "
				 "en la nommant={5}",
				 bons, NkMatOpVecteurCount(), detail, refus1 ? 1 : 0, refus2 ? 1 : 0, refusInc ? 1 : 0));
}

static void CasVectorMathGardesNaN() {
	// Deux gardes que rien d'autre ne couvre, meme raison d'etre que la division
	// de `Math` : un NaN CONTAMINE tout l'aval et change d'aspect d'un backend a
	// l'autre, ce qui fait chercher la panne dans le pilote pendant des heures.
	//
	//   - `normalize(vec3(0))` rend un NaN. Et le vecteur nul n'est pas un cas
	//     tordu : c'est le DEFAUT d'une prise jamais renseignee.
	//   - la division vectorielle par zero, composante par composante.
	NkMatCompileResult rn =
		CompileOutil(NK_MN_VECTOR_MATH, "vector", "base_color", NK_MPROP_OPERATION, "normaliser");
	NkMatCompileResult rd =
		CompileOutil(NK_MN_VECTOR_MATH, "vector", "base_color", NK_MPROP_OPERATION, "diviser");
	const bool gardeNorm = rn.ok && Apres(rn.source, "length(") > 0 && Apres(rn.source, "1e-8") > 0;
	const bool gardeDiv = rd.ok && Apres(rd.source, "1e-8") > 0;
	// ⚠️ TEMOIN OBLIGATOIRE : une operation SANS garde ne doit PAS en porter une.
	// Sans lui, un generateur qui emettrait `1e-8` partout passerait les deux
	// controles ci-dessus sans rien garder du tout.
	NkMatCompileResult ra =
		CompileOutil(NK_MN_VECTOR_MATH, "vector", "base_color", NK_MPROP_OPERATION, "ajouter");
	const bool ajouterSansGarde = ra.ok && Apres(ra.source, "1e-8") < 0;
	Cas("outil/math-vecteur-gardes-de-NaN", gardeNorm && gardeDiv && ajouterSansGarde,
		NkFormat("normaliser garde le vecteur nul={0} | diviser garde le zero={1} | TEMOIN : ajouter ne "
				 "porte AUCUNE garde={2} (sinon le controle passerait avec des gardes partout)",
				 gardeNorm ? 1 : 0, gardeDiv ? 1 : 0, ajouterSansGarde ? 1 : 0));
}


// ── FLOAT CURVE : LA SECONDE CHARGE VARIABLE, ET LE SEUL DU RANG A EN PORTER ──

static NkMatCompileResult CompileCourbe(NkNodeGraph &g, const NkMatTypes &t, const float32 *pts,
										uint32 nbReels, const char *interp, const float32 *fac) {
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	const NkNodeId c = NkMatAddNode(gReg, g, NK_MN_FLOAT_CURVE);
	g.Connect(bsdf, "bsdf", out, "surface");
	g.Connect(c, "value", bsdf, "roughness");
	if (pts)
		g.SetProp(c, NK_MPROP_POINTS, NkValueVec(t.curve, pts, nbReels));
	if (interp)
		g.SetProp(c, NK_MPROP_INTERP, NkValueText(t.curve, interp));
	if (fac)
		g.SetSocketDefault(c, "fac", NkSocketDir::Input, NkValueReal(t.real, *fac));
	return NkMatCompileToNkSL(gReg, g);
}

static void CasCourbeChargeVariable() {
	// LA SECONDE CHARGE VARIABLE, ET ELLE A UN AUTRE PAS. La rampe range quatre
	// reels par arret, celle-ci deux par point -- c est cette difference qui a
	// force la lecture a devenir PARAMETREE plutot que recopiee.
	//
	// DISCRIMINE : on compte les mix. Une courbe a N points en produit N-1, plus
	// UN pour le melange par `fac`. Un emetteur qui ne verrait que le premier et
	// le dernier point donnerait un shader qui compile et une courbe FAUSSE. Le
	// compte est verifie pour 2, 3 et 5 points : une seule taille ne prouverait
	// rien d une charge variable.
	const float32 p2[4] = {0.f, 0.f, 1.f, 1.f};
	const float32 p3[6] = {0.f, 0.f, 0.5f, 0.9f, 1.f, 1.f};
	const float32 p5[10] = {0.f, 0.f, 0.25f, 0.6f, 0.5f, 0.3f, 0.75f, 0.8f, 1.f, 1.f};
	struct Jeu {
			const float32 *p;
			uint32 n;
			uint32 pts;
	};
	const Jeu jeux[3] = {{p2, 4, 2}, {p3, 6, 3}, {p5, 10, 5}};
	uint32 bons = 0;
	NkString detail;
	for (uint32 j = 0; j < 3; ++j) {
		NkNodeGraph g;
		const NkMatTypes t = NkMatRegisterTypes(g);
		NkMatCompileResult r = CompileCourbe(g, t, jeux[j].p, jeux[j].n, "lineaire", nullptr);
		uint32 nbMix = 0;
		if (r.ok) {
			const char *q = r.source.CStr();
			while (*q) {
				if (q[0] == 'm' && q[1] == 'i' && q[2] == 'x' && q[3] == '(')
					++nbMix;
				++q;
			}
		}
		// N-1 pour la courbe, +1 pour le melange par `fac`, +2 pour le puits
		// (specExp, specColor) : la lecon d avant-hier, ou compter tous les mix
		// du shader mesurait le modele d eclairage en meme temps que le noeud.
		const uint32 attendus = jeux[j].pts - 1u + 1u + 2u;
		NkString be, err;
		const uint32 ok = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;
		if (r.ok && ok == 5 && nbMix == attendus)
			++bons;
		detail.Append(NkFormat("{0} points->{1} mix (attendu {2}) ", jeux[j].pts, nbMix, attendus));
	}
	Cas("courbe/charge-utile-variable-au-pas-de-deux", bons == 3, detail);
}

static void CasCourbeRefusNommes() {
	// CINQ malformations, CINQ messages distincts, et AUCUN shader emis. La
	// cinquieme n existe PAS pour la rampe : un seul point. Une rampe a un arret
	// ANNONCE une couleur unie ; une courbe a un point rend une CONSTANTE alors
	// que l auteur croit avoir dessine un trace.
	//
	// Et le sixieme cas -- la propriete ABSENTE -- doit au contraire COMPILER :
	// c est le voisin legitime (le noeud vient d etre pose).
	const float32 malForme[3] = {0.f, 0.f, 1.f};
	const float32 unSeul[2] = {0.f, 0.5f};
	const float32 desordre[4] = {1.f, 1.f, 0.f, 0.f};
	const float32 egales[4] = {0.5f, 0.f, 0.5f, 1.f};
	float32 trop[(NK_CURVE_POINTS_MAX + 1) * 2];
	for (uint32 i = 0; i <= NK_CURVE_POINTS_MAX; ++i) {
		trop[i * 2 + 0] = (float32)i * 0.001f;
		trop[i * 2 + 1] = 0.5f;
	}
	NkString m[5];
	bool emisVide = true;
	const float32 *jeux[5] = {malForme, unSeul, desordre, egales, trop};
	const uint32 tailles[5] = {3, 2, 4, 4, (NK_CURVE_POINTS_MAX + 1) * 2};
	for (uint32 j = 0; j < 5; ++j) {
		NkNodeGraph g;
		const NkMatTypes t = NkMatRegisterTypes(g);
		NkMatCompileResult r = CompileCourbe(g, t, jeux[j], tailles[j], "lineaire", nullptr);
		m[j] = r.ok ? NkString("A COMPILE (ne devait pas)") : r.error;
		if (r.source.Size() != 0)
			emisVide = false;
	}
	NkNodeGraph g2;
	const NkMatTypes t2 = NkMatRegisterTypes(g2);
	NkMatCompileResult absente = CompileCourbe(g2, t2, nullptr, 0, nullptr, nullptr);
	const bool ditLeCompte = Apres(m[4], "33 demandes, plafond 32") > 0;
	// Cinq messages deux a deux differents. Un compilateur qui rendrait le meme
	// message pour tout passerait un test qui ne compterait que les echecs.
	bool distincts = true;
	for (uint32 a = 0; a < 5; ++a)
		for (uint32 b = a + 1; b < 5; ++b)
			if (m[a] == m[b])
				distincts = false;
	Cas("courbe/refus-nommes-et-distincts", distincts && emisVide && absente.ok && ditLeCompte,
		NkFormat("[{0}] [{1}] [{2}] [{3}] [{4}] | distincts={5} rien emis={6} absente compile={7} plafond "
				 "dit le compte={8}",
				 m[0], m[1], m[2], m[3], m[4], distincts ? 1 : 0, emisVide ? 1 : 0, absente.ok ? 1 : 0,
				 ditLeCompte ? 1 : 0));
}

static void CasCourbeDomaineDessine() {
	// 🔴 LE CAS QUI ATTRAPE L ERREUR LA PLUS TENTANTE. Une courbe se BORNE, sinon
	// elle extrapole vers des valeurs que l auteur n a jamais tracees. Mais la
	// borner a [0,1] -- le reflexe, parce que la rampe juste a cote est definie
	// la -- ECRASERAIT en silence les quatre cinquiemes d une courbe dessinee
	// sur [0,5]. Le shader compilerait, rendrait, et serait faux.
	//
	// DISCRIMINE : la borne haute emise doit etre 5, jamais 1.0.
	const float32 large[6] = {0.f, 0.f, 2.5f, 1.f, 5.f, 0.2f};
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	NkMatCompileResult r = CompileCourbe(g, t, large, 6, "lineaire", nullptr);
	const bool borneHaute = r.ok && Apres(r.source, "5.0)") > 0;
	// Et le processeur doit s accorder : lue en 9, hors du domaine, la courbe
	// TIENT son dernier point, elle ne repart pas vers l infini.
	NkNodeGraph g2;
	const NkMatTypes t2 = NkMatRegisterTypes(g2);
	NkNodeId o2;
	MonteUnPrincipled(g2, t2, &o2);
	const NkNodeId c2 = NkMatAddNode(gReg, g2, NK_MN_FLOAT_CURVE);
	g2.SetProp(c2, NK_MPROP_POINTS, NkValueVec(t2.curve, large, 6));
	g2.SetSocketDefault(c2, "value", NkSocketDir::Input, NkValueReal(t2.real, 9.f));
	const NkNodeId s2 = PoseSortie(g2, t2, "hors_domaine", "par_materiau");
	g2.Connect(c2, "value", s2, "value");
	NkMatCompileResult r2 = NkMatCompileToNkSL(gReg, g2);
	const NkMatSortieMateriau *so = r2.ok ? r2.TrouveSortie("hors_domaine") : nullptr;
	const bool tientLeDernier = so && Proche(so->valeur[0], 0.2f);
	NkString be, err;
	const uint32 ok = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;
	Cas("courbe/domaine-dessine-et-non-zero-un", r.ok && ok == 5 && borneHaute && tientLeDernier,
		NkFormat("{0}| borne au domaine DESSINE (5, pas 1)={1} | lue en 9 -> {2} (attendu 0.2 : on tient le "
				 "dernier point)",
				 be, borneHaute ? 1 : 0, so ? so->valeur[0] : -1.f));
}

static void CasCourbeFacNeutreEstUn() {
	// 🔴 LE NEUTRE D UN NOEUD N EST PAS LE ZERO DE SON OPERATION.
	//
	// `fac` melange l entree et le resultat de la courbe. Le repli arithmetique
	// serait 0 -- et a 0 ce noeud rend son entree TELLE QUELLE. L auteur poserait
	// un Float Curve, dessinerait son trace, et ne verrait RIEN changer, sans le
	// moindre message. Meme raison que l echelle a 1 du Mapping.
	//
	// DISCRIMINE PAR LA VALEUR, PAS PAR LE TEXTE : on lit ce que l evaluateur
	// rend. Une courbe qui envoie 0.5 sur 0.9, entree 0.5, `fac` NON RENSEIGNE.
	// Attendu 0.9 (la courbe s applique) et non 0.5 (le noeud transparent).
	const float32 p[6] = {0.f, 0.f, 0.5f, 0.9f, 1.f, 1.f};
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	NkNodeId o;
	MonteUnPrincipled(g, t, &o);
	const NkNodeId c = NkMatAddNode(gReg, g, NK_MN_FLOAT_CURVE);
	g.SetProp(c, NK_MPROP_POINTS, NkValueVec(t.curve, p, 6));
	g.SetSocketDefault(c, "value", NkSocketDir::Input, NkValueReal(t.real, 0.5f));
	const NkNodeId s = PoseSortie(g, t, "applique", "par_materiau");
	g.Connect(c, "value", s, "value");
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	const NkMatSortieMateriau *so = r.ok ? r.TrouveSortie("applique") : nullptr;
	const bool applique = so && Proche(so->valeur[0], 0.9f);

	// TEMOIN : `fac` a 0 doit au contraire rendre l entree intacte. Sans ce
	// second tir, un evaluateur qui IGNORERAIT `fac` -- toujours la courbe --
	// passerait le premier et serait faux des que l auteur regle le melange.
	NkNodeGraph g0;
	const NkMatTypes t0 = NkMatRegisterTypes(g0);
	NkNodeId o0;
	MonteUnPrincipled(g0, t0, &o0);
	const NkNodeId c0 = NkMatAddNode(gReg, g0, NK_MN_FLOAT_CURVE);
	g0.SetProp(c0, NK_MPROP_POINTS, NkValueVec(t0.curve, p, 6));
	g0.SetSocketDefault(c0, "value", NkSocketDir::Input, NkValueReal(t0.real, 0.5f));
	g0.SetSocketDefault(c0, "fac", NkSocketDir::Input, NkValueReal(t0.real, 0.f));
	const NkNodeId s0 = PoseSortie(g0, t0, "transparent", "par_materiau");
	g0.Connect(c0, "value", s0, "value");
	NkMatCompileResult r0 = NkMatCompileToNkSL(gReg, g0);
	const NkMatSortieMateriau *so0 = r0.ok ? r0.TrouveSortie("transparent") : nullptr;
	const bool transparent = so0 && Proche(so0->valeur[0], 0.5f);

	// Et le SHADER doit porter le meme neutre que le processeur. Un 1.0 ici et
	// un 0.0 la-bas rendrait le pixel et le code de jeu discordants sur un noeud
	// fraichement pose -- le pire des defauts, parce que les deux sont plausibles.
	NkNodeGraph gs;
	const NkMatTypes ts = NkMatRegisterTypes(gs);
	NkMatCompileResult rs = CompileCourbe(gs, ts, p, 6, "lineaire", nullptr);
	// ⚠️ ON LIT LA LIGNE DU MELANGE, PAS TOUTE LA SOURCE. Premiere version de
	// cette verification : « , 1.0); apparait dans la source ». Elle a SURVECU a
	// la mutation qui met le neutre du shader a 0 -- parce que « , 1.0); » se
	// trouve ailleurs dans le shader, dans le puits et chez les voisins. Le
	// controle verifiait la presence d une CHAINE, jamais son EMPLOI.
	//
	// CINQUIEME occurrence de cette faute dans le chantier, et je l ai commise
	// le lendemain du jour ou je l ai ecrite noir sur blanc. La retenir ne suffit
	// visiblement pas : ce qui l attrape, c est la mutation, pas la vigilance.
	//
	// `_cy, 1.0);` n existe QUE dans le melange final du Float Curve. Dans
	// l echelle des segments, `_cy` est toujours suivi d une virgule puis d un
	// autre argument -- jamais d une parenthese fermante.
	const bool shaderPorteUn = rs.ok && Apres(rs.source, "_cy, 1.0);") > 0 &&
							   Apres(rs.source, "_cy, 0.0);") < 0;
	Cas("courbe/fac-non-renseigne-applique-la-courbe", applique && transparent && shaderPorteUn,
		NkFormat("fac absent -> {0} (attendu 0.9 : la courbe s applique) | fac=0 -> {1} (attendu 0.5 : "
				 "TEMOIN, le noeud est transparent) | le shader porte le meme neutre={2}",
				 so ? so->valeur[0] : -1.f, so0 ? so0->valeur[0] : -1.f, shaderPorteUn ? 1 : 0));
}

static void CasCourbeInterpolation() {
	// Les deux interpolations emettent des codes DIFFERENTS -- step pour la
	// constante, une pente pre-calculee pour la lineaire -- et une interpolation
	// inconnue est refusee en la nommant. MEME TABLE que la rampe : une seconde
	// liste « interpolations de courbe » aurait le meme contenu et divergerait au
	// premier ajout.
	//
	// ⚠️ Et la courbe lineaire ne contient AUCUNE division dans le shader : la
	// pente est calculee a la compilation, et `NkMatLisCourbe` a deja garanti que
	// le denominateur est non nul en refusant les positions egales.
	const float32 p[6] = {0.f, 0.f, 0.5f, 0.9f, 1.f, 1.f};
	NkNodeGraph g1;
	const NkMatTypes t1 = NkMatRegisterTypes(g1);
	NkMatCompileResult lin = CompileCourbe(g1, t1, p, 6, "lineaire", nullptr);
	NkNodeGraph g2;
	const NkMatTypes t2 = NkMatRegisterTypes(g2);
	NkMatCompileResult cst = CompileCourbe(g2, t2, p, 6, "constante", nullptr);
	NkNodeGraph g3;
	const NkMatTypes t3 = NkMatRegisterTypes(g3);
	NkMatCompileResult inc = CompileCourbe(g3, t3, p, 6, "spline_de_bezier_cubique", nullptr);
	const bool cstPalier = cst.ok && Apres(cst.source, "step(") > 0;
	const bool linSansStep = lin.ok && Apres(lin.source, "step(") < 0;
	const bool refuseNomme = !inc.ok && Apres(inc.error, "spline_de_bezier_cubique") > 0;
	// La division cherchee est celle de l interpolation. Chercher un simple « / »
	// mesurerait les commentaires et le reste du shader.
	const bool aucuneDivision = lin.ok && Apres(lin.source, ") / (") < 0;
	NkString b1, b2, e1;
	const uint32 o1 = lin.ok ? CompileSurLesBackends(lin.source, b1, &e1) : 0u;
	const uint32 o2 = cst.ok ? CompileSurLesBackends(cst.source, b2, &e1) : 0u;
	Cas("courbe/interpolation-et-refus",
		o1 == 5 && o2 == 5 && cstPalier && linSansStep && refuseNomme && aucuneDivision,
		NkFormat("lin:{0}cst:{1}| constante emet step={2} | lineaire n en emet PAS={3} | inconnue refusee "
				 "en la nommant={4} | aucune division dans le shader={5}",
				 b1, b2, cstPalier ? 1 : 0, linSansStep ? 1 : 0, refuseNomme ? 1 : 0,
				 aucuneDivision ? 1 : 0));
}

// ── groupe/ : UN GROUPE EST-IL SEULEMENT REPRESENTABLE ? ─────────────────────
//
// Demande de Rodolf (2026-08-22, R8 de design.reponses.md) : « un groupe est un
// groupement de noeuds que l'utilisateur peut empaqueter pour reutiliser a
// volonte comme des fonctions. » Un groupe devient donc un TYPE DE NOEUD CREE
// PAR L'UTILISATEUR A L'EXECUTION -- et la consigne est de MESURER si le
// registre l'accepte, pas de le supposer.
//
// Les quatre cas ci-dessous mesurent QUATRE PORTES DIFFERENTES, et elles ne
// repondent pas la meme chose. C'est tout l'interet : « est-ce que ca marche »
// n'a pas de reponse unique ici.

// Declare ici parce que le graphe d essai du REGROUPEMENT sert aussi au cas
// du pont, qui le precede dans le fichier. Sa definition est plus bas.
static void MonteGrapheAGrouper(NkNodeGraph &g, const NkMatTypes &t, NkVector<NkNodeId> &outSel,
								NkNodeId *outVal, NkNodeId *outMix, NkNodeId *outEmi);

// La cle d'un type de noeud invente PAR L'UTILISATEUR. Elle est composee a
// l'EXECUTION, chiffre compris, a partir d'une valeur `volatile` : ainsi aucune
// lecture du banc ne peut objecter que la cle etait connue du compilateur. C'est
// exactement la situation d'un groupe nomme par l'auteur dans l'editeur.
static volatile uint32 gGraineDuGroupe = 3;

static void FabriqueCleDeGroupe(char *buf) {
	const char *prefixe = "grp.utilisateur_";
	uint32 i = 0;
	while (prefixe[i]) {
		buf[i] = prefixe[i];
		++i;
	}
	buf[i++] = (char)('0' + (gGraineDuGroupe % 10u));
	buf[i] = 0;
}

// Cherche une sous-chaine dans un NkString. Le message d'erreur n'est pas une
// ligne : `ContientLigne` ne convient pas ici.
static bool ContientLaCle(const NkString &texte, const char *cle) {
	const char *hay = texte.CStr();
	const uint32 hn = (uint32)texte.Size();
	const uint32 cn = (uint32)strlen(cle);
	if (!hay || cn == 0 || hn < cn)
		return false;
	for (uint32 i = 0; i + cn <= hn; ++i) {
		uint32 k = 0;
		while (k < cn && hay[i + k] == cle[k])
			++k;
		if (k == cn)
			return true;
	}
	return false;
}

static void CasGroupeCoeurAccepteUnTypeInconnu() {
	// LA QUESTION POSEE A LA COUCHE 1. Le coeur accepte-t-il un type de noeud
	// qui n'existait pas a la compilation ?
	//
	// DISCRIMINE : un coeur muni d'un registre FERME de types de noeuds
	// refuserait `AddNode` et rendrait NK_NODE_INVALID. Un coeur qui accepterait
	// le noeud mais PERDRAIT sa cle a l'ecriture serait pire encore -- le graphe
	// paraitrait sain et se rechargerait avec un noeud anonyme. On exige donc
	// les deux : le noeud vit, ET sa cle traverse le fichier MOT POUR MOT.
	//
	// On ne se contente pas d'exister : le noeud doit se COMPORTER comme les
	// autres -- porter des prises, se relier, et prendre son rang dans l'ordre
	// topologique. Un type accepte mais ignore par le tri serait un piege.
	char cle[32];
	FabriqueCleDeGroupe(cle);

	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);

	const NkNodeId grp = g.AddNode(cle, "Mon groupe");
	const bool cree = grp != NK_NODE_INVALID;
	// Une interface a lui, declaree a la main -- c'est ce que ferait le
	// noeud-groupe : ses prises sont celles de ses noeuds frontiere.
	g.AddSocket(grp, "entree", t.real, NkSocketDir::Input);
	g.AddSocket(grp, "sortie", t.color, NkSocketDir::Output);

	// Il se relie a un noeud du catalogue, dans les deux sens.
	const NkNodeId val = NkMatAddNode(gReg, g, NK_MN_VALUE);
	const NkNodeId emi = NkMatAddNode(gReg, g, NK_MN_EMISSION);
	const NkLinkError e1 = g.Connect(val, "value", grp, "entree");
	const NkLinkError e2 = g.Connect(grp, "sortie", emi, "color");

	// Il prend son rang : producteurs avant consommateurs.
	NkVector<NkNodeId> ordre;
	const bool trie = g.TopoSort(ordre);
	int32 rgVal = -1, rgGrp = -1, rgEmi = -1;
	for (uint32 i = 0; i < (uint32)ordre.Size(); ++i) {
		if (ordre[i] == val)
			rgVal = (int32)i;
		if (ordre[i] == grp)
			rgGrp = (int32)i;
		if (ordre[i] == emi)
			rgEmi = (int32)i;
	}
	const bool bonOrdre = rgVal >= 0 && rgGrp > rgVal && rgEmi > rgGrp;

	// Et sa cle traverse le fichier sans etre rabotee.
	NkString fichier;
	g.Serialize(fichier);
	NkNodeGraph g2;
	const bool relu = g2.Deserialize(fichier.CStr());
	const NkNode *apres = relu ? g2.Find(grp) : nullptr;
	const bool cleIntacte = apres && apres->type == NkString(cle);
	const bool prisesIntactes = apres && apres->FindSocket("entree", NkSocketDir::Input) >= 0 &&
								apres->FindSocket("sortie", NkSocketDir::Output) >= 0;

	NkString d;
	d = NkFormat("cle={0} | cree={1} relie={2}/{3} trie={4} rang(val<grp<emi)={5} | relu={6} cle intacte={7} prises={8}",
				 NkString(cle), cree ? 1 : 0, NkString(NkLinkErrorName(e1)), NkString(NkLinkErrorName(e2)),
				 trie ? 1 : 0, bonOrdre ? 1 : 0, relu ? 1 : 0, cleIntacte ? 1 : 0, prisesIntactes ? 1 : 0);
	Cas("groupe/coeur-accepte-type-runtime",
		cree && e1 == NkLinkError::Ok && e2 == NkLinkError::Ok && trie && bonOrdre && relu && cleIntacte &&
			prisesIntactes,
		d);
}

static void CasGroupeCatalogueMateriauFerme() {
	// LA MEME QUESTION POSEE A LA COUCHE 3, et la reponse est l'INVERSE.
	//
	// Le catalogue `kProtos` est un tableau `static const` : il est clos a la
	// compilation. On ne mesure PAS « il contient 28 entrees » -- ce serait un
	// compte fige sur une collection qui grandit par conception, la dette
	// exactement decrite dans nkrenderer.reponses.md. On mesure la RELATION qui
	// nous interesse : *une cle inventee a l'execution n'est connue d'AUCUNE des
	// portes du catalogue*, et elle le restera tant qu'aucune voie
	// d'enregistrement n'existera.
	//
	// DISCRIMINE aussi par le MENU, et c'est le point utile pour la suite : le
	// menu « que puis-je brancher ici ? » interroge le MEME registre. Un groupe
	// absent du registre est donc invisible dans le menu de TOUTES les prises --
	// pas d'une seule. Reciproquement, ouvrir le registre ouvrirait le menu sans
	// une ligne de plus. C'est ce qui rend le point reparable au bon endroit.
	char cle[32];
	FabriqueCleDeGroupe(cle);

	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);

	const NkMatNodeProto *p = NkMatFindProto(gReg, cle);
	const NkNodeId n = NkMatAddNode(gReg, g, cle);
	// Le refus doit etre NET : pas de noeud a moitie forme laisse derriere.
	const bool refusPropre = n == NK_NODE_INVALID && g.NodeCount() == 0;

	// Invisible dans le menu de CHACUN des quatre types de prise. Une relation,
	// aucun nombre a maintenir.
	const NkTypeId types[4] = {t.real, t.vector, t.color, t.shader};
	const NkMatNodeProto *menu[64];
	bool vuQuelquePart = false;
	uint32 totalPropose = 0;
	for (uint32 k = 0; k < 4; ++k) {
		uint32 m = NkMatNoeudsPourPrise(gReg, g, types[k], menu, 64);
		if (m > 64)
			m = 64;
		totalPropose += m;
		if (DansLeMenu(menu, m, cle))
			vuQuelquePart = true;
	}

	NkString d;
	d = NkFormat("cle={0} | trouvee dans le catalogue={1} | instanciation refusee proprement={2} | menus rendent {3} "
				 "propositions | groupe visible quelque part={4} (0 attendu)",
				 NkString(cle), p ? 1 : 0, refusPropre ? 1 : 0, totalPropose, vuQuelquePart ? 1 : 0);
	// `totalPropose > 0` est un CONTROLE POSITIF : sans lui, un menu casse qui
	// ne rendrait jamais rien ferait passer ce cas pour la mauvaise raison.
	Cas("groupe/catalogue-materiau-ferme", p == nullptr && refusPropre && !vuQuelquePart && totalPropose > 0, d);
}

static void CasGroupeOuVitLeRefusDuTypeInconnu() {
	// OU, EXACTEMENT, un type de noeud inconnu est-il arrete ?
	//
	// ⚠️ CE CAS A CHANGE DE VERDICT LE 2026-08-22, ET C'EST LE BUT. Sa premiere
	// version constatait que `NkMatValidate` rendait `ok` sur un type inconnu --
	// elle ne frappait a aucune porte -- et que seul l'EMETTEUR l'arretait, tres
	// loin de la cause. Rodolf a tranche : c'est une REPARATION, pas une
	// fonctionnalite. Le cas mesure desormais la reparation.
	//
	// Les trois etages, et ils ne disent toujours pas la meme chose :
	//   1. le FICHIER accepte le noeud inconnu -- c'est voulu, le coeur n'a pas
	//      de registre de types de noeuds et ne doit pas en avoir ;
	//   2. `NkMatValidate` le REFUSE maintenant, en NOMMANT le type coupable ;
	//   3. et le message de compilation porte ce nom.
	//
	// ⚠️ DISCRIMINE SUR LE POINT QUI S'EST PRESQUE PERDU : attraper plus tot a
	// failli faire perdre le nom. Avant la reparation, l'emetteur disait
	// « noeud non compilable : <le type> ». La validation le rattrape en amont --
	// donc plus pres de la cause -- mais rendait un « type-de-noeud-inconnu »
	// muet sur LEQUEL. On exige donc que le type apparaisse dans le message :
	// un refus qui ne nomme pas est un refus qui fait fouiller cent noeuds.
	char cle[32];
	FabriqueCleDeGroupe(cle);
	gReg.Vide(); // le registre est global : on part d'un etat connu

	NkString texte;
	{
		NkNodeGraph g;
		const NkMatTypes t = NkMatRegisterTypes(g);
		const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
		const NkNodeId emi = NkMatAddNode(gReg, g, NK_MN_EMISSION);
		const NkNodeId grp = g.AddNode(cle, "Mon groupe");
		g.AddSocket(grp, "sortie", t.color, NkSocketDir::Output);
		g.Connect(grp, "sortie", emi, "color");
		g.Connect(emi, "emission", out, "surface");
		g.Serialize(texte);
	}

	NkNodeGraph g2;
	NkMatRegisterTypes(g2);
	const bool relu = g2.Deserialize(texte.CStr());
	bool present = false;
	for (uint32 i = 0; i < g2.RawNodeCount(); ++i) {
		const NkNode *n = g2.RawNodeAt(i);
		if (n && n->alive && n->type == NkString(cle))
			present = true;
	}
	NkString quoi;
	NkNodeId coupable = NK_NODE_INVALID;
	const NkMatGraphError v = NkMatValidate(gReg, g2, &coupable, &quoi);
	const NkMatCompileResult r = NkMatCompileToNkSL(gReg, g2);
	const bool cleDansLeMessage = ContientLaCle(r.error, cle);
	const bool sourceVide = r.source.Size() == 0;

	NkString d;
	d = NkFormat("relu={0} noeud inconnu ENTRE par le fichier={1} | validation dit '{2}' et nomme '{3}' | compile "
				 "ok={4} (0 attendu) source vide={5} | type coupable dans le message={6}",
				 relu ? 1 : 0, present ? 1 : 0, NkString(NkMatGraphErrorName(v)), quoi, r.ok ? 1 : 0,
				 sourceVide ? 1 : 0, cleDansLeMessage ? 1 : 0);
	Cas("groupe/refus-du-type-inconnu-situe",
		relu && present && v == NkMatGraphError::UnknownNodeType && quoi == NkString(cle) &&
			coupable != NK_NODE_INVALID && !r.ok && sourceVide && cleDansLeMessage,
		d);
}

// ── LE CATALOGUE OUVERT : arbitrage de Rodolf du 2026-08-22 ──────────────────

// Une interface de groupe minimale, pour les cas qui n'ont pas besoin d'un vrai
// sous-graphe.
static const NkMatSocketDecl kPrisesDuGroupeEssai[] = {
	{"entree", NK_MT_REAL, NkSocketDir::Input, false},
	{"sortie", NK_MT_COLOR, NkSocketDir::Output, false},
};

static void CasCatalogueOuvertALExecution() {
	// LA MESURE DE L'ARBITRAGE : une clef inventee a l'execution devient un
	// prototype de plein droit, et TOUT ce qui lit la porte le voit.
	//
	// DISCRIMINE par la propriete qui a rendu la decision bon marche : le menu
	// « que puis-je brancher ici ? » interroge le MEME registre. On ne verifie
	// donc pas seulement que le groupe est trouvable -- on exige qu'il APPARAISSE
	// dans le menu d'une prise couleur (sa sortie en est une) et qu'il soit
	// ABSENT du menu d'une prise shader (il n'en produit pas). Un registre
	// branche a la porte mais pas au menu passerait la premiere moitie.
	char cle[32];
	FabriqueCleDeGroupe(cle);
	gReg.Vide();

	const uint32 avant = NkMatProtoCount(gReg);
	const NkMatRegistreErreur e =
		gReg.Enregistre(cle, "Mon groupe", kPrisesDuGroupeEssai, 2, false);
	const uint32 apres = NkMatProtoCount(gReg);

	const NkMatNodeProto *p = NkMatFindProto(gReg, cle);
	// et il s'instancie comme n'importe quel autre : c'est `NkMatAddNode`, la
	// meme fonction, sans une ligne de plus.
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId n = NkMatAddNode(gReg, g, cle);
	const NkNode *nd = g.Find(n);
	const bool prises = nd && nd->FindSocket("entree", NkSocketDir::Input) >= 0 &&
						nd->FindSocket("sortie", NkSocketDir::Output) >= 0;

	const NkMatNodeProto *menu[96];
	uint32 mc = NkMatNoeudsPourPrise(gReg, g, t.color, menu, 96);
	if (mc > 96)
		mc = 96;
	const bool dansMenuCouleur = DansLeMenu(menu, mc, cle);
	uint32 ms = NkMatNoeudsPourPrise(gReg, g, t.shader, menu, 96);
	if (ms > 96)
		ms = 96;
	const bool dansMenuShader = DansLeMenu(menu, ms, cle);

	NkString d;
	d = NkFormat("enregistrement='{0}' | protos {1} -> {2} | trouve par la porte={3} | instancie avec ses prises={4} | "
				 "dans le menu COULEUR={5} (1 attendu) dans le menu SHADER={6} (0 attendu)",
				 NkString(NkMatRegistreErreurNom(e)), avant, apres, p ? 1 : 0, prises ? 1 : 0,
				 dansMenuCouleur ? 1 : 0, dansMenuShader ? 1 : 0);
	Cas("groupe/catalogue-ouvert-a-l-execution",
		e == NkMatRegistreErreur::Ok && apres == avant + 1u && p != nullptr && n != NK_NODE_INVALID && prises &&
			dansMenuCouleur && !dansMenuShader,
		d);
	gReg.Vide();
}

static void CasCatalogueRefusDEclipse() {
	// LA CONDITION POSEE PAR RODOLF : « un type d'execution qui porte le nom d'un
	// proto statique doit etre REFUSE en se nommant, jamais l'eclipser en
	// silence. Un catalogue ou le dernier inscrit gagne est un catalogue dont on
	// ne peut plus predire le contenu. »
	//
	// DISCRIMINE par ce qu'on lit APRES le refus, et pas seulement par le code
	// rendu : on exige que `NkMatFindProto` rende toujours LE PROTO COMPILE --
	// reconnaissable a son libelle et a son nombre de prises. Un registre qui
	// accepterait l'inscription tout en la classant derriere la table statique
	// rendrait le bon prototype aujourd'hui et le mauvais le jour ou l'ordre de
	// consultation changerait. Le refus doit etre a l'ENREGISTREMENT.
	gReg.Vide();
	const NkMatNodeProto *avant = NkMatFindProto(gReg, NK_MN_PRINCIPLED);
	const uint32 prisesAvant = avant ? avant->socketCount : 0;

	const NkMatRegistreErreur e =
		gReg.Enregistre(NK_MN_PRINCIPLED, "Faux Principled", kPrisesDuGroupeEssai, 2, false);

	const NkMatNodeProto *apres = NkMatFindProto(gReg, NK_MN_PRINCIPLED);
	const bool intact = apres != nullptr && apres == avant && apres->socketCount == prisesAvant;
	const bool rienEntre = gReg.Count() == 0;

	// et les autres refus se nomment aussi
	const NkMatRegistreErreur vide = gReg.Enregistre("", "x", kPrisesDuGroupeEssai, 2, false);
	const NkMatRegistreErreur sans = gReg.Enregistre("grp.sans", "x", kPrisesDuGroupeEssai, 0, false);
	gReg.Enregistre("grp.double", "x", kPrisesDuGroupeEssai, 2, false);
	const NkMatRegistreErreur deux = gReg.Enregistre("grp.double", "y", kPrisesDuGroupeEssai, 2, false);

	NkString d;
	d = NkFormat("eclipse='{0}' | proto compile intact={1} ({2} prises) | rien n est entre={3} | nom vide='{4}' sans "
				 "prise='{5}' doublon='{6}'",
				 NkString(NkMatRegistreErreurNom(e)), intact ? 1 : 0, prisesAvant, rienEntre ? 1 : 0,
				 NkString(NkMatRegistreErreurNom(vide)), NkString(NkMatRegistreErreurNom(sans)),
				 NkString(NkMatRegistreErreurNom(deux)));
	Cas("groupe/refus-d-eclipse-nomme",
		e == NkMatRegistreErreur::DejaStatique && intact && rienEntre &&
			vide == NkMatRegistreErreur::NomVide && sans == NkMatRegistreErreur::SansPrise &&
			deux == NkMatRegistreErreur::DejaEnregistre,
		d);
	gReg.Vide();
}

static void CasDeuxDocumentsNeVoientPasLeursGroupes() {
	// ⚠️ LE TEMOIN DU CHANGEMENT DE SIGNATURE, ET LA SEULE RAISON DE L'AVOIR
	// FAIT. Jusqu'au 2026-08-23 le registre etait unique pour le processus :
	// deux documents ouverts partageaient leurs groupes. C'etait ecrit comme
	// defaut connu, et voici ce qui l'empeche de revenir.
	//
	// Trois symptomes etaient annonces. Le cas les mesure TOUS LES TROIS, parce
	// qu'ils ne se corrigent pas forcement ensemble :
	//   1. un groupe defini dans A ne doit PAS etre trouvable depuis B ;
	//   2. il ne doit PAS apparaitre dans le MENU de B ;
	//   3. A et B doivent pouvoir employer LE MEME NOM sans se gener -- c'est le
	//      symptome le plus fourbe, parce qu'il se manifeste par un refus
	//      parfaitement legitime en apparence (« deja enregistre ») sur un nom
	//      que le second document est pourtant seul a employer.
	//
	// DISCRIMINE PAR UN TEMOIN INTERNE : on verifie aussi que chaque document
	// voit SON PROPRE groupe. Sans ca, un registre casse qui ne rendrait jamais
	// rien passerait les trois controles ci-dessus pour la pire des raisons.
	NkMatRegistreProtos docA;
	NkMatRegistreProtos docB;

	const NkMatRegistreErreur eA = docA.Enregistre("grp.de_A", "Groupe de A", kPrisesDuGroupeEssai, 2, false);
	const NkMatRegistreErreur eB = docB.Enregistre("grp.de_B", "Groupe de B", kPrisesDuGroupeEssai, 2, false);

	// 1. chacun voit le sien, aucun ne voit celui de l'autre
	const bool aVoitLeSien = NkMatFindProto(docA, "grp.de_A") != nullptr;
	const bool bVoitLeSien = NkMatFindProto(docB, "grp.de_B") != nullptr;
	const bool aNeVoitPasB = NkMatFindProto(docA, "grp.de_B") == nullptr;
	const bool bNeVoitPasA = NkMatFindProto(docB, "grp.de_A") == nullptr;

	// 2. et le MENU suit, puisqu'il interroge la meme porte
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkMatNodeProto *menu[96];
	uint32 mA = NkMatNoeudsPourPrise(docA, g, t.color, menu, 96);
	if (mA > 96)
		mA = 96;
	const bool menuAPorteLeSien = DansLeMenu(menu, mA, "grp.de_A");
	const bool menuANePortePasB = !DansLeMenu(menu, mA, "grp.de_B");
	uint32 mB = NkMatNoeudsPourPrise(docB, g, t.color, menu, 96);
	if (mB > 96)
		mB = 96;
	const bool menuBNePortePasA = !DansLeMenu(menu, mB, "grp.de_A");

	// 3. le MEME NOM dans les deux documents, sans collision
	const NkMatRegistreErreur mA2 = docA.Enregistre("grp.commun", "chez A", kPrisesDuGroupeEssai, 2, false);
	const NkMatRegistreErreur mB2 = docB.Enregistre("grp.commun", "chez B", kPrisesDuGroupeEssai, 2, false);
	const NkMatNodeProto *chezA = NkMatFindProto(docA, "grp.commun");
	const NkMatNodeProto *chezB = NkMatFindProto(docB, "grp.commun");
	// ⚠️ et ce ne doit pas etre le MEME objet : deux documents qui pointeraient
	// sur une seule definition rejoueraient le defaut sous une autre forme --
	// modifier le groupe de A changerait celui de B.
	const bool memeNomChacunLeSien = chezA && chezB && chezA != chezB &&
									 NkMatCleEgale(chezA->label, "chez A") && NkMatCleEgale(chezB->label, "chez B");

	// 4. et fermer A ne casse pas B : on vide A, B est intact.
	docA.Vide();
	const bool bSurvitAlaFermetureDeA = NkMatFindProto(docB, "grp.de_B") != nullptr &&
										NkMatFindProto(docA, "grp.de_A") == nullptr;

	NkString d;
	d = NkFormat("enregistrements {0}/{1} | chacun voit le sien={2}{3} et pas celui de l autre={4}{5} | menus : le "
				 "sien={6} pas l autre={7}{8} | meme nom des deux cotes '{9}'/'{10}' chacun le sien={11} | B survit a "
				 "la fermeture de A={12}",
				 NkString(NkMatRegistreErreurNom(eA)), NkString(NkMatRegistreErreurNom(eB)), aVoitLeSien ? 1 : 0,
				 bVoitLeSien ? 1 : 0, aNeVoitPasB ? 1 : 0, bNeVoitPasA ? 1 : 0, menuAPorteLeSien ? 1 : 0,
				 menuANePortePasB ? 1 : 0, menuBNePortePasA ? 1 : 0, NkString(NkMatRegistreErreurNom(mA2)),
				 NkString(NkMatRegistreErreurNom(mB2)), memeNomChacunLeSien ? 1 : 0,
				 bSurvitAlaFermetureDeA ? 1 : 0);
	Cas("groupe/deux-documents-ne-voient-pas-leurs-groupes",
		eA == NkMatRegistreErreur::Ok && eB == NkMatRegistreErreur::Ok && aVoitLeSien && bVoitLeSien && aNeVoitPasB &&
			bNeVoitPasA && menuAPorteLeSien && menuANePortePasB && menuBNePortePasA &&
			mA2 == NkMatRegistreErreur::Ok && mB2 == NkMatRegistreErreur::Ok && memeNomChacunLeSien &&
			bSurvitAlaFermetureDeA,
		d);
}

static void CasPontInterfaceDeduiteDuSousGraphe() {
	// LE PONT COMPLET, de bout en bout : on GROUPE de vrais noeuds, puis on
	// ENREGISTRE le sous-graphe obtenu, et le groupe devient un prototype
	// instanciable dont l'interface a ete DEDUITE de sa frontiere.
	//
	// DISCRIMINE sur le sens des prises, qui est le piege du pont : le noeud
	// `graph.entree` porte des prises de SORTIE (il alimente l'interieur), et
	// elles doivent devenir des ENTREES sur le prototype. Une implantation qui
	// recopierait le sens tel quel produirait un prototype dont toutes les
	// prises sont a l'envers -- parfaitement plausible, et qui ne se brancherait
	// jamais.
	//
	// ⚠️ ET `parPixel` : le graphe groupe ne contient aucune source intrinseque,
	// donc le prototype doit etre FAUX. Un pont qui rendrait `true` par prudence
	// aveugle refuserait toutes les sorties « par materiau » d'un groupe sain.
	gReg.Vide();
	NkGraphDocument doc;
	const uint32 racine = doc.AddGraph("racine");
	doc.SetRoot(racine);
	NkVector<NkNodeId> sel;
	{
		NkNodeGraph &g = doc.GraphAt(racine);
		const NkMatTypes t = NkMatRegisterTypes(g);
		MonteGrapheAGrouper(g, t, sel, nullptr, nullptr, nullptr);
	}
	NkGrouper(doc, racine, sel.Data(), (uint32)sel.Size(), "mon groupe", nullptr);
	const int32 ci = doc.FindGraph("mon groupe");

	NkMatRegistreErreur e = NkMatRegistreErreur::NomVide;
	if (ci >= 0)
		e = NkMatEnregistreGroupe(gReg, doc.GraphAt((uint32)ci), "grp.mon_groupe", "Mon groupe");

	const NkMatNodeProto *p = NkMatFindProto(gReg, "grp.mon_groupe");
	uint32 nIn = 0, nOut = 0;
	bool typesNommes = true;
	if (p)
		for (uint32 i = 0; i < p->socketCount; ++i) {
			(p->sockets[i].dir == NkSocketDir::Input ? nIn : nOut) += 1u;
			if (!p->sockets[i].type || !p->sockets[i].type[0])
				typesNommes = false;
		}

	// ⚠️ COMPTER NE SUFFIT PAS, ET LA MUTATION L'A DIT. « une entree et une
	// sortie » est VRAI AUSSI quand les deux sens sont inverses : le compte est
	// symetrique, le cablage non. La mutation qui recopie le sens de la prise
	// frontiere tel quel -- au lieu de l'inverser -- laissait ce cas VERT, avec
	// un prototype dont toutes les prises sont a l'envers.
	//
	// Septieme occurrence dans ce chantier de « je compte, je ne relie pas ». On
	// nomme donc les deux prises : `a` vient du noeud d'ENTREE du groupe, elle
	// doit etre une ENTREE sur le prototype ; `value` vient du noeud de SORTIE,
	// elle doit etre une SORTIE.
	bool sensJuste = false;
	if (p) {
		int32 iA = -1, iV = -1;
		for (uint32 i = 0; i < p->socketCount; ++i) {
			if (NkMatCleEgale(p->sockets[i].name, "a"))
				iA = (int32)i;
			else if (NkMatCleEgale(p->sockets[i].name, "value"))
				iV = (int32)i;
		}
		sensJuste = iA >= 0 && iV >= 0 && p->sockets[(uint32)iA].dir == NkSocketDir::Input &&
					p->sockets[(uint32)iV].dir == NkSocketDir::Output;
	}

	// et il s'instancie pour de vrai, prises comprises
	NkNodeGraph h;
	NkMatRegisterTypes(h);
	const NkNodeId inst = NkMatAddNode(gReg, h, "grp.mon_groupe");

	NkString d;
	d = NkFormat("enregistrement='{0}' | prises deduites : {1} entree(s) {2} sortie(s) (1 et 1 attendues) | 'a' est "
				 "une ENTREE et 'value' une SORTIE={3} | types nommes={4} | parPixel={5} (0 attendu) | s instancie={6}",
				 NkString(NkMatRegistreErreurNom(e)), nIn, nOut, sensJuste ? 1 : 0, typesNommes ? 1 : 0,
				 (p && p->parPixel) ? 1 : 0, inst != NK_NODE_INVALID ? 1 : 0);
	Cas("groupe/pont-interface-deduite-du-sous-graphe",
		e == NkMatRegistreErreur::Ok && p != nullptr && nIn == 1 && nOut == 1 && sensJuste && typesNommes &&
			!p->parPixel && inst != NK_NODE_INVALID,
		d);
	gReg.Vide();
}

static void CasPontParPixelEstConservateur() {
	// ⚠️ LE CHOIX QUI COMPTE, ET IL EST ASYMETRIQUE. Un groupe qui contient une
	// source intrinseque (ici un Noise) EST par pixel. Et quand on ne peut pas
	// trancher -- un groupe IMBRIQUE apparait comme un `graph.instance` dont on
	// ne peut pas resoudre le sous-graphe sans le document -- on prend le cote
	// SUR, parce que les deux erreurs ne coutent pas pareil :
	//   parPixel=true a tort  -> une sortie « par materiau » est REFUSEE alors
	//     qu'elle etait licite. Faux, mais BRUYANT.
	//   parPixel=false a tort -> elle ACCEPTE une valeur qui change a chaque
	//     pixel et rend celle d'un pixel arbitraire comme si c'etait celle du
	//     materiau. Faux, PLAUSIBLE, jamais signale.
	//
	// DISCRIMINE avec un TEMOIN : le cas precedent prouve qu'un groupe sain rend
	// `false`. Sans lui, un pont qui rendrait TOUJOURS `true` passerait ici.
	gReg.Vide();
	// (a) un groupe qui contient une source par pixel
	bool aParPixel = false;
	{
		NkNodeGraph sg;
		const NkMatTypes t = NkMatRegisterTypes(sg);
		const NkNodeId bord = sg.AddNode(NK_NODE_GROUP_OUT, "Sorties du groupe");
		sg.AddSocket(bord, "fac", t.real, NkSocketDir::Input);
		const NkNodeId bruit = NkMatAddNode(gReg, sg, NK_MN_NOISE);
		sg.Connect(bruit, "fac", bord, "fac");
		const NkMatRegistreErreur e = NkMatEnregistreGroupe(gReg, sg, "grp.avec_bruit", "Avec bruit");
		const NkMatNodeProto *p = NkMatFindProto(gReg, "grp.avec_bruit");
		aParPixel = (e == NkMatRegistreErreur::Ok) && p && p->parPixel;
	}
	// (b) un groupe qui contient une INSTANCE non resolue
	bool bParPixel = false;
	{
		NkNodeGraph sg;
		const NkMatTypes t = NkMatRegisterTypes(sg);
		const NkNodeId bord = sg.AddNode(NK_NODE_GROUP_OUT, "Sorties du groupe");
		sg.AddSocket(bord, "value", t.real, NkSocketDir::Input);
		const NkNodeId imbrique = sg.AddNode(NK_NODE_INSTANCE, "un groupe imbrique");
		sg.AddSocket(imbrique, "value", t.real, NkSocketDir::Output);
		sg.Connect(imbrique, "value", bord, "value");
		const NkMatRegistreErreur e = NkMatEnregistreGroupe(gReg, sg, "grp.imbrique", "Imbrique");
		const NkMatNodeProto *p = NkMatFindProto(gReg, "grp.imbrique");
		bParPixel = (e == NkMatRegistreErreur::Ok) && p && p->parPixel;
	}
	NkString d;
	d = NkFormat("groupe contenant un Noise : parPixel={0} (1 attendu) | groupe contenant une instance non resolue : "
				 "parPixel={1} (1 attendu, cote sur)",
				 aParPixel ? 1 : 0, bParPixel ? 1 : 0);
	Cas("groupe/pont-par-pixel-conservateur", aParPixel && bParPixel, d);
	gReg.Vide();
}

static void CasGroupeRecursionRefuseeMaisOu() {
	// LA RECURSION, ET SES DEUX FILETS. Rodolf a tranche le 2026-08-22 : le refus
	// passe A L'INSERTION -- « a ce moment-la l'utilisateur sait ce qu'il vient de
	// faire ; a l'aplatissement, l'erreur sort loin de sa cause » -- ET le
	// controle a l'aplatissement RESTE, parce qu'un graphe peut arriver par un
	// FICHIER sans jamais passer par une insertion.
	//
	// ⚠️ CE CAS MESURE LES DEUX FILETS SEPAREMENT, et c'est tout son interet. Un
	// cas qui ne mesurerait que le premier laisserait retirer le second sans
	// rien dire -- et c'est justement ce que Rodolf a demande de ne pas faire.
	//
	// DISCRIMINE aussi sur ce qu'il reste APRES le refus a l'insertion : le
	// graphe doit etre INCHANGE. Un refus qui aurait deja cree le noeud
	// laisserait une instance orpheline, et le code de retour seul ne le dirait
	// pas -- meme piege que « prototype-inconnu ».

	// ── FILET 1 : a l'insertion ──────────────────────────────────────────
	// (a) le groupe qui s'appelle lui-meme
	NkGraphDocument d1;
	const uint32 a1 = d1.AddGraph("moi");
	d1.SetRoot(a1);
	const NkGroupError soi = NkPoseInstance(d1, a1, "moi", nullptr);
	const bool rienPose1 = d1.GraphAt(a1).NodeCount() == 0;

	// (b) la boucle a DEUX maillons -- celle qu'un controle regardant le voisin
	//     immediat laisserait passer. A instancie B (licite) ; B instancie A
	//     doit etre refuse.
	NkGraphDocument d2;
	const uint32 ga = d2.AddGraph("A");
	const uint32 gb = d2.AddGraph("B");
	d2.SetRoot(ga);
	const NkGroupError licite = NkPoseInstance(d2, ga, "B", nullptr);
	const NkGroupError boucle = NkPoseInstance(d2, gb, "A", nullptr);
	const bool rienPose2 = d2.GraphAt(gb).NodeCount() == 0;

	// (c) TEMOIN : une instance parfaitement licite doit passer. Sans lui, un
	//     controle qui refuserait TOUT rendrait ce cas vert pour la pire des
	//     raisons.
	const bool temoin = licite == NkGroupError::Ok && d2.GraphAt(ga).NodeCount() == 1;

	// (d) ⚠️ TROIS MAILLONS, et ce cas manquait. La mutation qui reduit la
	//     detection au VOISIN IMMEDIAT a survecu a (b) -- et c'est logique : a
	//     deux maillons, regarder le voisin immediat SUFFIT. Le controle
	//     recursif n'etait donc pas mesure du tout. Avec A -> B -> C, refuser
	//     C -> A demande de remonter toute la chaine.
	NkGraphDocument d4;
	const uint32 ha = d4.AddGraph("A");
	const uint32 hb = d4.AddGraph("B");
	const uint32 hc = d4.AddGraph("C");
	d4.SetRoot(ha);
	const NkGroupError l1 = NkPoseInstance(d4, ha, "B", nullptr);
	const NkGroupError l2 = NkPoseInstance(d4, hb, "C", nullptr);
	const NkGroupError trois = NkPoseInstance(d4, hc, "A", nullptr);
	const bool chaineLicite = l1 == NkGroupError::Ok && l2 == NkGroupError::Ok;
	const bool rienPose3 = d4.GraphAt(hc).NodeCount() == 0;

	// ── FILET 2 : a l'aplatissement, pour ce qui vient d'un FICHIER ──────
	// On contourne volontairement la porte -- `NkNode::subgraph` est public, donc
	// c'est exactement ce qu'un chargement de fichier produit.
	NkGraphDocument d3;
	{
		const uint32 x = d3.AddGraph("moi");
		d3.SetRoot(x);
		const NkNodeId i = d3.GraphAt(x).AddNode(NK_NODE_INSTANCE, "moi-meme");
		NkNode *n = d3.GraphAt(x).Find(i);
		if (n)
			n->subgraph = NkString("moi");
	}
	NkEvalPlan p3;
	const NkPlanError e3 = d3.BuildPlan(p3);

	NkString d;
	d = NkFormat("INSERTION : soi-meme='{0}' deux maillons='{1}' TROIS maillons='{2}' | rien pose apres refus={3}/{4}"
				 "/{5} | temoins (instance licite={6}, chaine A->B->C licite={7}) || APLATISSEMENT (chemin du "
				 "fichier) : '{8}' plan vide={9}",
				 NkString(NkGroupErrorName(soi)), NkString(NkGroupErrorName(boucle)),
				 NkString(NkGroupErrorName(trois)), rienPose1 ? 1 : 0, rienPose2 ? 1 : 0, rienPose3 ? 1 : 0,
				 temoin ? 1 : 0, chaineLicite ? 1 : 0, NkString(NkPlanErrorName(e3)), p3.Size() == 0 ? 1 : 0);
	Cas("groupe/recursion-refusee-aux-deux-portes",
		soi == NkGroupError::Recursive && boucle == NkGroupError::Recursive &&
			trois == NkGroupError::Recursive && rienPose1 && rienPose2 && rienPose3 && temoin && chaineLicite &&
			e3 == NkPlanError::RecursiveSubgraph && p3.Size() == 0,
		d);
}

static void CasInstancePoseeInterfaceDeduite() {
	// La porte d'insertion ne fait pas que refuser : elle DEDUIT l'interface de
	// l'instance depuis la frontiere du sous-graphe, et elle transporte les types.
	//
	// DISCRIMINE par le SENS des prises -- le piege du pont. Le noeud
	// `graph.entree` porte des prises de SORTIE (il alimente l'interieur) qui
	// doivent devenir des ENTREES sur l'instance. Les recopier telles quelles
	// donnerait une instance dont toutes les prises sont a l'envers : plausible,
	// et qui ne se brancherait jamais.
	//
	// Et la preuve qui vaut mieux que la mienne : `BuildPlan` fait passer son
	// propre controle d'interface, qui compare NOMS ET TYPES des deux cotes.
	NkGraphDocument doc;
	const uint32 racine = doc.AddGraph("racine");
	doc.SetRoot(racine);
	NkVector<NkNodeId> sel;
	{
		NkNodeGraph &g = doc.GraphAt(racine);
		const NkMatTypes t = NkMatRegisterTypes(g);
		MonteGrapheAGrouper(g, t, sel, nullptr, nullptr, nullptr);
	}
	NkGrouper(doc, racine, sel.Data(), (uint32)sel.Size(), "mon groupe", nullptr);

	// une SECONDE instance du meme groupe, posee par la porte : c'est le
	// « reutiliser a volonte » de R8.
	NkNodeId deuxieme = NK_NODE_INVALID;
	const NkGroupError e = NkPoseInstance(doc, racine, "mon groupe", &deuxieme);
	uint32 nIn = 0, nOut = 0;
	const NkNode *nd = doc.GraphAt(racine).Find(deuxieme);
	if (nd)
		for (uint32 i = 0; i < (uint32)nd->sockets.Size(); ++i)
			(nd->sockets[i].dir == NkSocketDir::Input ? nIn : nOut) += 1u;
	NkEvalPlan plan;
	const NkPlanError ep = doc.BuildPlan(plan);

	NkString d;
	d = NkFormat("pose='{0}' | prises deduites : {1} entree(s) {2} sortie(s) (1 et 1 attendues) | aplatissement des "
				 "DEUX instances='{3}' etapes={4}",
				 NkString(NkGroupErrorName(e)), nIn, nOut, NkString(NkPlanErrorName(ep)), plan.Size());
	Cas("groupe/instance-posee-interface-deduite",
		e == NkGroupError::Ok && deuxieme != NK_NODE_INVALID && nIn == 1 && nOut == 1 && ep == NkPlanError::Ok, d);
}

// ── regroupement/ : LE CONTROLE, ECRIT AVANT L'OPERATION ────────────────────
//
// Consigne de Rodolf (R9) : « grouper puis degrouper doit rendre le graphe
// identique, octet pour octet apres serialisation, aux identifiants pres », et
// « ecris ce controle AVANT d'ecrire le regroupement ». C'est ce que fait ce
// bloc : il a ete ecrit en premier, et l'operation a ete ecrite pour le
// satisfaire.
//
// ⚠️ POURQUOI PAS UNE COMPARAISON DU TEXTE SERIALISE, alors que c'est la lettre
// de la consigne. Parce que « aux identifiants pres » n'est pas une retouche
// cosmetique : apres un aller-retour, les noeuds sont RECREES, donc renumerotes
// ET reordonnes dans la table. Renumeroter le texte par ordre d'apparition
// alignerait deux ordres differents et comparerait des noeuds qui n'ont rien a
// voir -- le controle rendrait « different » sur un aller-retour parfait, puis
// serait relache jusqu'a ne plus rien dire.
//
// La forme CANONIQUE ci-dessous range donc les noeuds par leur CONTENU, jamais
// par leur rang : un noeud est decrit par son type, son libelle, sa position,
// ses prises (nom, type, sens, defaut) et ses proprietes ; les liens sont
// decrits par les DESCRIPTEURS de leurs extremites, jamais par des numeros.
// Deux graphes egaux modulo les identifiants ont exactement la meme forme.
//
// ⚠️ ET SA LIMITE, ecrite ici pour ne pas etre oubliee : si DEUX noeuds ont le
// meme descripteur (meme type, meme libelle, meme position, memes prises), la
// forme canonique ne peut plus les distinguer, et un lien deplace de l'un a
// l'autre passerait inapercu. Le controle le DETECTE et le DIT (`ambigu`) au
// lieu de rendre un vert trompeur -- un cas qui ne peut pas discriminer doit
// l'annoncer, pas se taire.

static bool ChaineMoinsQue(const NkString &a, const NkString &b) {
	const char *p = a.CStr();
	const char *q = b.CStr();
	const uint32 na = (uint32)a.Size();
	const uint32 nb = (uint32)b.Size();
	const uint32 n = na < nb ? na : nb;
	for (uint32 i = 0; i < n; ++i) {
		if ((unsigned char)p[i] != (unsigned char)q[i])
			return (unsigned char)p[i] < (unsigned char)q[i];
	}
	return na < nb;
}

static void TrieChaines(NkVector<NkString> &v) {
	// Tri par insertion : quelques dizaines d'elements, et un tri simple qu'on
	// relit est preferable ici a un tri rapide qu'on relit mal.
	for (uint32 i = 1; i < (uint32)v.Size(); ++i) {
		NkString cle = v[i];
		uint32 j = i;
		while (j > 0 && ChaineMoinsQue(cle, v[j - 1])) {
			v[j] = v[j - 1];
			--j;
		}
		v[j] = cle;
	}
}

static NkString DecritValeur(const NkNodeGraph &g, const NkGraphValue &v) {
	if (!v.IsSet())
		return NkString("-");
	const NkString *tn = g.TypeName(v.type);
	NkString s = tn ? *tn : NkString("?");
	s.Append("(");
	for (uint32 i = 0; i < (uint32)v.numbers.Size(); ++i) {
		if (i)
			s.Append(",");
		s.Append(NkFormat("{0}", v.numbers[i]));
	}
	s.Append(";");
	s.Append(v.text);
	s.Append(")");
	return s;
}

// Le descripteur d'un noeud : tout ce qui le definit SAUF son identifiant.
static NkString DecritNoeud(const NkNodeGraph &g, const NkNode &n) {
	NkString s = NkFormat("T={0} L={1} P=({2},{3})", n.type, n.label, n.x, n.y);
	// Les prises sont decrites DANS LEUR ORDRE : c'est lui qui porte l'ordre de
	// l'interface d'un groupe, et R9 exige qu'il soit deterministe.
	for (uint32 i = 0; i < (uint32)n.sockets.Size(); ++i) {
		const NkSocket &k = n.sockets[i];
		const NkString *tn = g.TypeName(k.type);
		s.Append(NkFormat(" |S {0}:{1}:{2}:{3}", k.name, tn ? *tn : NkString("?"),
						  k.dir == NkSocketDir::Input ? NkString("in") : NkString("out"),
						  DecritValeur(g, k.defaultValue)));
	}
	for (uint32 i = 0; i < (uint32)n.props.Size(); ++i)
		s.Append(NkFormat(" |R {0}:{1}", n.props[i].name, DecritValeur(g, n.props[i].value)));
	return s;
}

// La forme canonique complete. `outAmbigu` passe a vrai si deux noeuds portent
// le meme descripteur -- auquel cas le controle ne peut plus discriminer et doit
// le dire.
static NkString FormeCanonique(const NkNodeGraph &g, bool *outAmbigu) {
	NkVector<NkString> descr;	 // descripteur du noeud i (table BRUTE, vivants seuls)
	NkVector<NkNodeId> ids;		 // son identifiant, pour retrouver les liens
	for (uint32 i = 0; i < g.RawNodeCount(); ++i) {
		const NkNode *n = g.RawNodeAt(i);
		if (!n || !n->alive)
			continue;
		descr.PushBack(DecritNoeud(g, *n));
		ids.PushBack(n->id);
	}

	if (outAmbigu) {
		*outAmbigu = false;
		for (uint32 i = 0; i < (uint32)descr.Size() && !*outAmbigu; ++i)
			for (uint32 j = i + 1; j < (uint32)descr.Size() && !*outAmbigu; ++j)
				if (descr[i] == descr[j])
					*outAmbigu = true;
	}

	// Les liens sont decrits par le DESCRIPTEUR de leurs extremites et le NOM de
	// leurs prises -- jamais par des index, qui ne survivent pas a un
	// aller-retour.
	NkVector<NkString> lignes;
	for (uint32 i = 0; i < (uint32)descr.Size(); ++i)
		lignes.PushBack(NkFormat("N {0}", descr[i]));
	for (uint32 i = 0; i < g.LinkCount(); ++i) {
		const NkLink *l = g.LinkAt(i);
		if (!l || !l->alive)
			continue;
		const NkNode *a = g.Find(l->fromNode);
		const NkNode *b = g.Find(l->toNode);
		if (!a || !b)
			continue;
		if (l->fromSocket < 0 || l->fromSocket >= (int32)a->sockets.Size())
			continue;
		if (l->toSocket < 0 || l->toSocket >= (int32)b->sockets.Size())
			continue;
		lignes.PushBack(NkFormat("L [{0}].{1} -> [{2}].{3}", DecritNoeud(g, *a), a->sockets[(uint32)l->fromSocket].name,
								 DecritNoeud(g, *b), b->sockets[(uint32)l->toSocket].name));
	}
	TrieChaines(lignes);

	NkString out;
	for (uint32 i = 0; i < (uint32)lignes.Size(); ++i) {
		out.Append(lignes[i]);
		out.Append("\n");
	}
	return out;
}

// ── le graphe d'essai du regroupement ────────────────────────────────────────
// Il est dessine pour porter LES QUATRE PIEGES de R9 a la fois :
//   - `val` (un Value) alimente DEUX noeuds internes depuis l'exterieur : une
//     seule entree doit en sortir, pas deux (piege 1 : deduplication) ;
//   - un reel alimente une prise COULEUR a travers la frontiere : la conversion
//     dirigee doit survivre au passage dans le sous-graphe ;
//   - la sortie interne alimente DEUX noeuds exterieurs : une seule sortie
//     (piege 2) ;
//   - deux prises internes destinataires portent des noms DIFFERENTS, et l'ordre
//     doit etre reproductible (pieges 3 et 4).
//
// `outSel` recoit la selection a grouper : les deux Math internes.
static void MonteGrapheAGrouper(NkNodeGraph &g, const NkMatTypes &t, NkVector<NkNodeId> &outSel, NkNodeId *outVal,
								NkNodeId *outMix, NkNodeId *outEmi) {
	const NkNodeId val = NkMatAddNode(gReg, g, NK_MN_VALUE);
	const NkNodeId m1 = NkMatAddNode(gReg, g, NK_MN_MATH);
	const NkNodeId m2 = NkMatAddNode(gReg, g, NK_MN_MATH);
	const NkNodeId mix = NkMatAddNode(gReg, g, NK_MN_MIX_COLOR);
	const NkNodeId emi = NkMatAddNode(gReg, g, NK_MN_EMISSION);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);

	// Des positions DISTINCTES : elles font partie du descripteur, et deux
	// noeuds Math superposes rendraient la forme canonique ambigue -- ce que le
	// controle signalerait, mais autant ne pas s'infliger le cas.
	NkNode *p = nullptr;
	p = g.Find(m1);
	if (p) {
		p->x = 10.f;
		p->y = 20.f;
	}
	p = g.Find(m2);
	if (p) {
		p->x = 10.f;
		p->y = 60.f;
	}

	// ⚠️ DES DEFAUTS DE PRISE ET DES PROPRIETES, et ils ne sont pas decoratifs.
	// Sans eux, deux mutations SURVIVAIENT a l'aller-retour -- « le defaut de
	// prise n'est pas recopie » et « la propriete n'est pas recopiee » -- parce
	// que le graphe d'essai n'en portait aucun. Un controle ne peut pas voir
	// disparaitre ce qui n'existe pas : c'est la MATIERE du cas qui manquait,
	// pas l'assertion. Mesure faite, pas devinee.
	//
	// Un defaut sur une prise LIBRE (`m2.a`) et un sur une prise CABLEE (`m1.b`) :
	// le second est celui qu'une recopie « intelligente » sauterait, en jugeant
	// qu'un defaut masque par un lien ne sert a rien -- il sert des qu'on
	// debranche.
	g.SetSocketDefault(m2, "a", NkSocketDir::Input, NkValueReal(t.real, 0.25f));
	g.SetSocketDefault(m1, "b", NkSocketDir::Input, NkValueReal(t.real, 0.75f));
	g.SetProp(m1, "operation", NkValueText(t.real, "multiplier"));
	g.SetProp(m2, "operation", NkValueText(t.real, "ajouter"));

	// `val` traverse la frontiere DEUX FOIS, depuis la MEME prise : une seule
	// entree de groupe doit en resulter.
	g.Connect(val, "value", m1, "a");
	g.Connect(val, "value", m2, "b");
	// et la sortie de m1 traverse vers DEUX destinataires exterieurs, dont une
	// prise COULEUR (conversion dirigee reel -> couleur).
	g.Connect(m1, "value", mix, "color1");
	g.Connect(m1, "value", mix, "fac");
	g.Connect(mix, "color", emi, "color");
	g.Connect(emi, "emission", out, "surface");
	// un lien PUREMENT INTERNE, qui doit rester a l'interieur sans devenir une
	// prise : c'est le troisieme cas du tableau de R9.
	g.Connect(m2, "value", m1, "b");

	outSel.PushBack(m1);
	outSel.PushBack(m2);
	if (outVal)
		*outVal = val;
	if (outMix)
		*outMix = mix;
	if (outEmi)
		*outEmi = emi;
}

static void CasRegroupementAllerRetourIdentique() {
	// LE CRITERE D'ACCEPTATION DE R9, tel qu'il est ecrit : grouper puis
	// degrouper rend le graphe identique, aux identifiants pres.
	//
	// DISCRIMINE : il attrape d'un seul coup un lien oublie a la frontiere, un
	// defaut de prise perdu au passage, une propriete non recopiee, une position
	// ecrasee, et un lien interne promu en prise par erreur. Aucun de ces
	// defauts ne se voit a l'oeil sur un graphe rendu -- tous se voient ici.
	NkGraphDocument doc;
	const uint32 racine = doc.AddGraph("racine");
	doc.SetRoot(racine);
	NkMatTypes t;
	NkVector<NkNodeId> sel;
	{
		NkNodeGraph &g = doc.GraphAt(racine);
		t = NkMatRegisterTypes(g);
		MonteGrapheAGrouper(g, t, sel, nullptr, nullptr, nullptr);
	}

	bool ambiguAvant = false;
	const NkString avant = FormeCanonique(doc.GraphAt(racine), &ambiguAvant);

	NkNodeId inst = NK_NODE_INVALID;
	const NkGroupError eg = NkGrouper(doc, racine, sel.Data(), (uint32)sel.Size(), "mon groupe", &inst);
	const NkGroupError ed = NkDegrouper(doc, racine, inst);

	bool ambiguApres = false;
	const NkString apres = FormeCanonique(doc.GraphAt(racine), &ambiguApres);
	const bool identique = avant == apres;

	NkString d;
	d = NkFormat("grouper='{0}' degrouper='{1}' | identique={2} ({3} vs {4} o) | ambigu avant={5} apres={6} (0 "
				 "attendu des deux cotes)",
				 NkString(NkGroupErrorName(eg)), NkString(NkGroupErrorName(ed)), identique ? 1 : 0,
				 (uint32)avant.Size(), (uint32)apres.Size(), ambiguAvant ? 1 : 0, ambiguApres ? 1 : 0);
	Cas("regroupement/aller-retour-identique",
		eg == NkGroupError::Ok && ed == NkGroupError::Ok && identique && !ambiguAvant && !ambiguApres, d);
}

static void CasRegroupementInterfaceDeduite() {
	// ⚠️ CE QUE L'ALLER-RETOUR NE PROUVE PAS, et c'est pour ca que ce cas existe.
	//
	// Une deduplication RATEE peut parfaitement survivre a l'aller-retour : un
	// groupe qui creerait CINQ entrees identiques pour une constante partagee
	// par cinq noeuds les redistribuerait correctement au degroupement, et le
	// graphe reviendrait identique. Le critere de R9 est necessaire, il n'est
	// pas suffisant -- il faut regarder l'INTERFACE elle-meme.
	//
	// DISCRIMINE : `val` traverse la frontiere DEUX FOIS depuis la MEME prise,
	// et la sortie de `m1` la traverse DEUX FOIS vers deux destinataires. Une
	// implantation qui compterait les LIENS au lieu des PRISES SOURCES rendrait
	// deux entrees et deux sorties, et ce cas tomberait.
	NkGraphDocument doc;
	const uint32 racine = doc.AddGraph("racine");
	doc.SetRoot(racine);
	NkVector<NkNodeId> sel;
	{
		NkNodeGraph &g = doc.GraphAt(racine);
		const NkMatTypes t = NkMatRegisterTypes(g);
		MonteGrapheAGrouper(g, t, sel, nullptr, nullptr, nullptr);
	}
	NkNodeId inst = NK_NODE_INVALID;
	const NkGroupError eg = NkGrouper(doc, racine, sel.Data(), (uint32)sel.Size(), "mon groupe", &inst);

	uint32 nIn = 0, nOut = 0;
	const NkNode *n = doc.GraphAt(racine).Find(inst);
	if (n)
		for (uint32 i = 0; i < (uint32)n->sockets.Size(); ++i)
			(n->sockets[i].dir == NkSocketDir::Input ? nIn : nOut) += 1u;

	// L'interface du sous-graphe et celle de l'instance doivent s'accorder :
	// c'est `BuildPlan` qui le juge, par le controle d'interface deja en place.
	NkEvalPlan plan;
	const NkPlanError ep = doc.BuildPlan(plan);

	NkString d;
	d = NkFormat("grouper='{0}' | entrees={1} (1 attendue : meme prise source deux fois) sorties={2} (1 attendue : "
				 "meme sortie interne vers deux destinataires) | aplatissement='{3}' etapes={4}",
				 NkString(NkGroupErrorName(eg)), nIn, nOut, NkString(NkPlanErrorName(ep)), plan.Size());
	Cas("regroupement/interface-deduite-et-dedupliquee",
		eg == NkGroupError::Ok && nIn == 1 && nOut == 1 && ep == NkPlanError::Ok, d);
}

// Le graphe de l'ORDRE. Il est distinct du precedent parce qu'il lui faut
// PLUSIEURS entrees et PLUSIEURS sorties : avec une seule de chaque, l'ordre est
// une propriete vide, et le cas qui l'a mesure d'abord ne pouvait rien attraper.
//
// Trois sources exterieures distinctes alimentent trois noeuds internes empiles
// verticalement, et chacun ressort vers une prise differente d'un Mix. Les trois
// prises internes s'appellent TOUTES `a`, et les trois sorties TOUTES `value` :
// la desambiguisation des homonymes (R9, piege 4) est donc exercee elle aussi.
//
// `ordreInverse` ne change pas le graphe : il change l'ORDRE DE CREATION DES
// LIENS. C'est le vrai scenario -- deux auteurs obtiennent le meme graphe par
// deux histoires d'edition differentes -- et c'est ce qui fait varier l'ordre
// dans lequel les croisements sont decouverts.
static void MonteGrapheOrdre(NkNodeGraph &g, const NkMatTypes &t, NkVector<NkNodeId> &outSel, bool ordreInverse) {
	(void)t;
	const NkNodeId v1 = NkMatAddNode(gReg, g, NK_MN_VALUE);
	const NkNodeId v2 = NkMatAddNode(gReg, g, NK_MN_VALUE);
	const NkNodeId v3 = NkMatAddNode(gReg, g, NK_MN_VALUE);
	const NkNodeId m1 = NkMatAddNode(gReg, g, NK_MN_MATH);
	const NkNodeId m2 = NkMatAddNode(gReg, g, NK_MN_MATH);
	const NkNodeId m3 = NkMatAddNode(gReg, g, NK_MN_MATH);
	const NkNodeId mix = NkMatAddNode(gReg, g, NK_MN_MIX_COLOR);

	// Des LIBELLES distincts sur les sources : c'est par eux que le cas
	// identifiera QUELLE source aboutit sur QUELLE prise du groupe. Sans eux,
	// trois noeuds `Value` seraient indiscernables et le controle comparerait
	// des noms de prises sans savoir ce qu'il y a derriere.
	NkNode *p = nullptr;
	p = g.Find(v1);
	if (p)
		p->label = NkString("v1");
	p = g.Find(v2);
	if (p)
		p->label = NkString("v2");
	p = g.Find(v3);
	if (p)
		p->label = NkString("v3");
	// L'empilement vertical EST la regle d'ordre : c'est lui qu'on veut voir
	// gagner contre l'ordre de creation des liens.
	p = g.Find(m1);
	if (p) {
		p->x = 10.f;
		p->y = 10.f;
	}
	p = g.Find(m2);
	if (p) {
		p->x = 10.f;
		p->y = 50.f;
	}
	p = g.Find(m3);
	if (p) {
		p->x = 10.f;
		p->y = 90.f;
	}

	if (!ordreInverse) {
		g.Connect(v1, "value", m1, "a");
		g.Connect(v2, "value", m2, "a");
		g.Connect(v3, "value", m3, "a");
		g.Connect(m1, "value", mix, "fac");
		g.Connect(m2, "value", mix, "color1");
		g.Connect(m3, "value", mix, "color2");
	} else {
		g.Connect(m3, "value", mix, "color2");
		g.Connect(m2, "value", mix, "color1");
		g.Connect(m1, "value", mix, "fac");
		g.Connect(v3, "value", m3, "a");
		g.Connect(v2, "value", m2, "a");
		g.Connect(v1, "value", m1, "a");
	}

	outSel.PushBack(m1);
	outSel.PushBack(m2);
	outSel.PushBack(m3);
}

static void CasRegroupementOrdreDeterministe() {
	// R9, piege 3 : « grouper deux fois la meme selection donne deux noeuds de
	// formes differentes » si l'ordre des prises emerge du parcours.
	//
	// ⚠️ CE CAS A DEJA ETE FAUX UNE FOIS, et la mutation l'a dit. Sa premiere
	// version comparait la SUITE DES NOMS des prises. Or les noms sont derives
	// des prises internes -- `a`, `a_2`, `a_3` -- et ils sortent DANS CE MEME
	// ORDRE quelle que soit la permutation : la suite des noms est identique
	// meme quand le cablage est entierement permute. Le tri pouvait etre
	// desactive, le cas restait vert.
	//
	// Il compare desormais la CORRESPONDANCE : pour chaque prise du groupe, QUI
	// s'y branche. `a<-v1` et `a<-v3` sont deux interfaces differentes qui
	// portent le meme nom. C'est la relation, pas l'etiquette.
	//
	// DISCRIMINE : la seconde passe construit le MEME graphe en creant ses liens
	// dans l'ordre INVERSE, et donne la selection a l'envers. Un ordre qui
	// suivrait la decouverte des croisements permuterait les trois entrees.
	NkString formeA, formeB;
	for (uint32 passe = 0; passe < 2; ++passe) {
		NkGraphDocument doc;
		const uint32 racine = doc.AddGraph("racine");
		doc.SetRoot(racine);
		NkVector<NkNodeId> sel;
		{
			NkNodeGraph &g = doc.GraphAt(racine);
			const NkMatTypes t = NkMatRegisterTypes(g);
			MonteGrapheOrdre(g, t, sel, passe == 1);
		}
		if (passe == 1) {
			NkVector<NkNodeId> inv;
			for (uint32 i = (uint32)sel.Size(); i > 0; --i)
				inv.PushBack(sel[i - 1]);
			sel = inv;
		}
		NkNodeId inst = NK_NODE_INVALID;
		NkGrouper(doc, racine, sel.Data(), (uint32)sel.Size(), "mon groupe", &inst);

		const NkNodeGraph &g = doc.GraphAt(racine);
		const NkNode *n = g.Find(inst);
		NkString f;
		if (n)
			for (uint32 i = 0; i < (uint32)n->sockets.Size(); ++i) {
				if (n->sockets[i].dir == NkSocketDir::Input) {
					// QUI alimente cette entree du groupe : on nomme la source
					// par son LIBELLE, stable d'une passe a l'autre.
					const NkLink *l = g.IncomingOf(inst, (int32)i);
					const NkNode *src = l ? g.Find(l->fromNode) : nullptr;
					f.Append(NkFormat("{0}<-{1};", n->sockets[i].name, src ? src->label : NkString("?")));
				} else {
					// et OU va cette sortie : la prise de destination suffit a
					// identifier le noeud interne d'origine.
					NkString dest("?");
					for (uint32 k = 0; k < g.LinkCount(); ++k) {
						const NkLink *l = g.LinkAt(k);
						if (!l || !l->alive || l->fromNode != inst || l->fromSocket != (int32)i)
							continue;
						const NkNode *d = g.Find(l->toNode);
						if (d && l->toSocket >= 0 && l->toSocket < (int32)d->sockets.Size())
							dest = d->sockets[(uint32)l->toSocket].name;
					}
					f.Append(NkFormat("{0}->{1};", n->sockets[i].name, dest));
				}
			}
		if (passe == 0)
			formeA = f;
		else
			formeB = f;
	}
	NkString d;
	d = NkFormat("passe 1 = '{0}' | passe 2 (liens crees a l envers, selection a l envers) = '{1}' | identiques={2}",
				 formeA, formeB, formeA == formeB ? 1 : 0);
	// Controle positif : une interface VIDE serait identique a elle-meme. On
	// exige donc qu'elle porte les SIX prises attendues.
	uint32 nbPrises = 0;
	for (uint32 i = 0; i < (uint32)formeA.Size(); ++i)
		if (formeA.CStr()[i] == ';')
			++nbPrises;
	Cas("regroupement/ordre-des-prises-deterministe", nbPrises == 6 && formeA == formeB, d);
}

static void CasRegroupementRefusNomme() {
	// Les refus, et ils doivent SE NOMMER. Trois formes qu'on ne doit jamais
	// laisser passer en silence :
	//   - une selection VIDE (rien a empaqueter) ;
	//   - un noeud de la selection qui n'existe pas dans ce graphe ;
	//   - un nom de groupe DEJA PRIS -- sans ce refus, deux definitions
	//     porteraient le meme nom et une instance en designerait une au hasard.
	//
	// DISCRIMINE : on exige aussi que le document soit INCHANGE apres chaque
	// refus. Un refus qui aurait deja cree le sous-graphe avant de renoncer
	// laisserait une definition orpheline derriere lui -- le meme piege que
	// « prototype-inconnu », ou le code de retour seul ne suffisait pas.
	NkGraphDocument doc;
	const uint32 racine = doc.AddGraph("racine");
	doc.SetRoot(racine);
	NkVector<NkNodeId> sel;
	{
		NkNodeGraph &g = doc.GraphAt(racine);
		const NkMatTypes t = NkMatRegisterTypes(g);
		MonteGrapheAGrouper(g, t, sel, nullptr, nullptr, nullptr);
	}
	const uint32 graphesAvant = doc.GraphCount();
	const uint32 noeudsAvant = doc.GraphAt(racine).NodeCount();

	const NkGroupError vide = NkGrouper(doc, racine, sel.Data(), 0, "g", nullptr);
	const NkNodeId fantome = 9999;
	const NkGroupError inconnu = NkGrouper(doc, racine, &fantome, 1, "g", nullptr);
	const NkGroupError nomPris = NkGrouper(doc, racine, sel.Data(), (uint32)sel.Size(), "racine", nullptr);
	// et Degrouper sur un noeud qui n'est pas une instance
	const NkGroupError pasUneInstance = NkDegrouper(doc, racine, sel[0]);

	const bool intact = doc.GraphCount() == graphesAvant && doc.GraphAt(racine).NodeCount() == noeudsAvant;

	NkString d;
	d = NkFormat("selection vide='{0}' noeud inconnu='{1}' nom deja pris='{2}' degrouper hors instance='{3}' | "
				 "document intact={4}",
				 NkString(NkGroupErrorName(vide)), NkString(NkGroupErrorName(inconnu)),
				 NkString(NkGroupErrorName(nomPris)), NkString(NkGroupErrorName(pasUneInstance)), intact ? 1 : 0);
	Cas("regroupement/refus-nommes",
		vide == NkGroupError::EmptySelection && inconnu == NkGroupError::UnknownNode &&
			nomPris == NkGroupError::NameTaken && pasUneInstance == NkGroupError::NotAnInstance && intact,
		d);
}

static void CasRegroupementConversionSurvitALaFrontiere() {
	// ⚠️ LE PIEGE QUE JE N'AVAIS PAS VU EN ECRIVANT LE CONTROLE, et qui merite
	// son propre cas : chaque graphe tient SON PROPRE registre de types et SES
	// PROPRES conversions dirigees. Un sous-graphe cree vide refuserait donc, a
	// l'interieur, un lien reel -> couleur que le parent acceptait -- et le
	// regroupement perdrait un fil SANS QUE RIEN NE LE DISE, puisque `Connect`
	// rend une erreur que personne ne lit.
	//
	// DISCRIMINE : la sortie de `m1` (un reel) alimente `fac` (reel) ET
	// `color1` (couleur). Si les conversions n'etaient pas transportees dans le
	// sous-graphe, le lien vers la couleur tomberait -- et l'aller-retour le
	// dirait. Ici on le mesure DIRECTEMENT, a l'endroit ou ca casse : dans le
	// registre du sous-graphe.
	NkGraphDocument doc;
	const uint32 racine = doc.AddGraph("racine");
	doc.SetRoot(racine);
	NkVector<NkNodeId> sel;
	{
		NkNodeGraph &g = doc.GraphAt(racine);
		const NkMatTypes t = NkMatRegisterTypes(g);
		MonteGrapheAGrouper(g, t, sel, nullptr, nullptr, nullptr);
	}
	NkGrouper(doc, racine, sel.Data(), (uint32)sel.Size(), "mon groupe", nullptr);
	const int32 idx = doc.FindGraph("mon groupe");
	bool memeNoms = false;
	bool memeConversion = false;
	bool pasDeConversionInverse = false;
	if (idx >= 0) {
		const NkNodeGraph &enfant = doc.GraphAt((uint32)idx);
		const NkNodeGraph &parent = doc.GraphAt(racine);
		const NkTypeId r = enfant.FindType(NK_MT_REAL);
		const NkTypeId c = enfant.FindType(NK_MT_COLOR);
		memeNoms = r != NK_TYPE_INVALID && c != NK_TYPE_INVALID;
		// La conversion DIRIGEE doit avoir traverse, et seulement dans son sens.
		memeConversion = enfant.Accepts(c, r) && parent.Accepts(parent.FindType(NK_MT_COLOR), parent.FindType(NK_MT_REAL));
		pasDeConversionInverse = !enfant.Accepts(r, c);
	}
	NkString d;
	d = NkFormat("sous-graphe trouve={0} | types reel+couleur presents={1} | reel->couleur autorise dedans={2} | "
				 "couleur->reel toujours refuse={3}",
				 idx >= 0 ? 1 : 0, memeNoms ? 1 : 0, memeConversion ? 1 : 0, pasDeConversionInverse ? 1 : 0);
	Cas("regroupement/conversions-traversent-la-frontiere",
		idx >= 0 && memeNoms && memeConversion && pasDeConversionInverse, d);
}

// ── sortie-par-pixel/ : LA SECONDE CIBLE DE RENDU EST-ELLE SEULEMENT
//    EXPRIMABLE EN NkSL ? ──────────────────────────────────────────────────
//
// (b1) repose entierement sur un fait que je n'avais pas verifie : qu'un
// fragment NkSL puisse declarer DEUX sorties couleur et qu'elles arrivent sur
// DEUX attachements distincts. Aucun shader du depot ne le fait — les
// `@location(1) out` qu'on y trouve sont tous des VARYINGS de sommet, pas des
// attachements de fragment. Il n'y avait donc rien pour l'attester.
//
// ⚠️ ET « LES QUATRE BACKENDS GENERENT » NE PROUVERAIT RIEN ICI, moins encore
// qu'ailleurs. Un generateur qui ignorerait `@location(1)` et emettrait DEUX
// sorties sur `SV_Target0` produirait un texte parfaitement valide, qui
// compilerait, et qui ecrirait la valeur auxiliaire PAR-DESSUS la couleur. Le
// resultat serait une image plausible et un tampon auxiliaire vide. On exige
// donc le SEMANTIQUE, pas le succes : `SV_Target1` cote HLSL, `location = 1`
// sur une sortie cote GLSL, et le mot magique de glslang.
static const char *kDeuxCibles = "@location(0) in vec2 vUV;\n"
								 "@location(0) out vec4 fragColor;\n"
								 "@location(1) out vec4 fragAux;\n"
								 "void main() {\n"
								 "    fragColor = vec4(vUV, 0.0, 1.0);\n"
								 "    fragAux = vec4(0.5, 0.25, 0.125, 1.0);\n"
								 "}\n";

// Le TEMOIN : le meme shader avec une seule sortie. Il existe pour repondre a la
// question qu'on se pose toujours quand le cas ci-dessus tombe — « est-ce la
// seconde cible qui est refusee, ou mon shader qui est mauvais ? ». Sans lui, on
// tranche au hasard.
static const char *kUneCible = "@location(0) in vec2 vUV;\n"
							   "@location(0) out vec4 fragColor;\n"
							   "void main() {\n"
							   "    fragColor = vec4(vUV, 0.0, 1.0);\n"
							   "}\n";

// ── (b1) : LA SECONDE CIBLE, ECRITE PAR TOUS ────────────────────────────────
//
// Le contrat, tel que Rodolf l'a signe : RGB = la valeur de la sortie resolue,
// A = 1 si ce pixel la porte, 0 sinon ; quand A == 0, RGB n'a aucun sens.
//
// TOUT SE MESURE SANS GPU, sur le NkSL emis. C'est possible parce que la
// propriete qui compte est SYNTAXIQUE : la sortie est-elle declaree, est-elle
// ECRITE, et avec quel alpha.

// Le shader d'un materiau, avec ou sans sortie nommee.
static NkMatCompileResult CompileAvecSortie(const char *etage, const char *nomDemande) {
	NkNodeGraph g;
	NkMatTypes t = NkMatRegisterTypes(g);
	NkNodeId out;
	MonteUnPrincipled(g, t, &out);
	if (etage) {
		const NkNodeId v = NkMatAddNode(gReg, g, NK_MN_VALUE);
		g.SetProp(v, NK_MPROP_VALUE, NkValueReal(t.real, 0.f));
		const NkNodeId so = PoseSortie(g, t, "humidite", etage);
		g.Connect(v, "value", so, "value");
	}
	NkMatCompileOptions opt;
	opt.sortieParPixel = nomDemande;
	return NkMatCompileToNkSL(gReg, g, opt);
}

// ── RANG 4 : CE QUE LE PIXEL SAIT DE LUI-MEME ───────────────────────────────

// Monte un materiau minimal ou `noeud`.`prise` alimente l'emission, pour que le
// noeud teste soit REELLEMENT consomme -- un noeud dont la sortie ne va nulle
// part serait emis puis ignore, et l'on ne mesurerait que sa declaration.
static NkMatCompileResult CompileAvecNoeudRang4(const char *cleProto, const char *priseSortie, const char *propCle,
												const char *propVal, NkNodeId *outId) {
	NkNodeGraph g;
	NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId out = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId emi = NkMatAddNode(gReg, g, NK_MN_EMISSION);
	const NkNodeId n = NkMatAddNode(gReg, g, cleProto);
	if (outId)
		*outId = n;
	if (n != NK_NODE_INVALID && propCle)
		g.SetProp(n, propCle, NkValueText(t.real, propVal));
	if (n != NK_NODE_INVALID)
		g.Connect(n, priseSortie, emi, "color");
	g.Connect(emi, "emission", out, "surface");
	return NkMatCompileToNkSL(gReg, g);
}

static void CasRang4GeometrieEtVue() {
	// Les quatre noeuds CALCULABLES : ils lisent la vue, la normale et la
	// position, et tout cela existe deja dans le shader.
	//
	// DISCRIMINE SUR LA CONVENTION, qui est le seul endroit ou l'on peut se
	// tromper sans que rien ne le dise : `nkViewV` doit pointer du PIXEL VERS
	// L'OEIL, comme `Incoming` chez Blender et comme le `V` que le puits calcule
	// deja trois lignes plus bas. La convention opposee donnerait un Fresnel qui
	// s'allume AU CENTRE au lieu des bords -- une image parfaitement credible,
	// et fausse. On exige donc l'expression exacte.
	NkNodeId idF = NK_NODE_INVALID, idL = NK_NODE_INVALID, idG = NK_NODE_INVALID;
	const NkMatCompileResult fr = CompileAvecNoeudRang4(NK_MN_FRESNEL, "fac", nullptr, nullptr, &idF);
	const NkMatCompileResult lw = CompileAvecNoeudRang4(NK_MN_LAYER_WEIGHT, "fresnel", nullptr, nullptr, &idL);
	const NkMatCompileResult ge = CompileAvecNoeudRang4(NK_MN_GEOMETRY, "position", nullptr, nullptr, &idG);

	const char *kVue = "vec3 nkViewV = normalize(uCam.camPos.xyz - vWorldPos);";
	const bool vueF = fr.ok && ContientSansCasse(fr.source, kVue);
	const bool vueL = lw.ok && ContientSansCasse(lw.source, kVue);
	const bool vueG = ge.ok && ContientSansCasse(ge.source, kVue);
	// et le calcul du Fresnel doit REELLEMENT employer la vue, pas seulement la
	// declarer : c'est la difference entre « la ligne est la » et « elle sert ».
	const bool fresnelEmploieLaVue = fr.ok && ContientSansCasse(fr.source, "), nkViewV), 0.0, 1.0), 5.0)");
	// Layer Weight rend DEUX sorties distinctes, pas deux fois la meme.
	NkString nomFresnel = NkFormat("n{0}_fresnel", idL);
	NkString nomFacing = NkFormat("n{0}_facing", idL);
	const bool deuxSorties =
		lw.ok && ContientSansCasse(lw.source, nomFresnel.CStr()) && ContientSansCasse(lw.source, nomFacing.CStr());

	NkString be;
	NkString err;
	const uint32 nb = fr.ok ? CompileSurLesBackends(fr.source, be, &err) : 0u;
	// Diagnostic PERMANENT, sur le modele du cas du masque : quand ce cas
	// tombe, la question est toujours « le shader est-il mal forme, ou mon
	// motif de recherche est-il faux ? ». Sans le code sous les yeux on
	// tranche au hasard. Il ne se declenche QUE sur echec -- un dump
	// systematique noierait la sortie du banc.
	if (getenv("NK_DUMP") && fr.ok && nb != 5)
		logger.Info("\n--- source Fresnel (le cas est rouge) ---\n{0}\n--- fin ---", fr.source.CStr());

	NkString d;
	d = NkFormat("vue declaree : Fresnel={0} LayerWeight={1} Geometry={2} | Fresnel l EMPLOIE={3} | LayerWeight rend "
				 "fresnel ET facing={4} | {5}| {6}",
				 vueF ? 1 : 0, vueL ? 1 : 0, vueG ? 1 : 0, fresnelEmploieLaVue ? 1 : 0, deuxSorties ? 1 : 0, be, err);
	Cas("rang4/geometrie-et-convention-de-vue",
		fr.ok && lw.ok && ge.ok && vueF && vueL && vueG && fresnelEmploieLaVue && deuxSorties && nb == 5, d);
}

static void CasRang4VueSeulementSiUtile() {
	// ⚠️ LA LIGNE NE DOIT PAS APPARAITRE CHEZ CEUX QUI N'EN ONT PAS BESOIN.
	//
	// Ce n'est pas de l'economie de calcul : declarer `nkViewV` partout
	// changerait la SOURCE, donc le jeton, donc la signature de materiaux qui
	// n'ont pas bouge d'un octet. Le banc temoin d'un autre chantier tomberait
	// sur une difference qui n'a rien a voir avec lui, et l'heure passee a la
	// diagnostiquer serait perdue pour tout le monde.
	//
	// DISCRIMINE DANS LES DEUX SENS : absente sans, presente avec.
	NkNodeGraph g;
	NkMatTypes t = NkMatRegisterTypes(g);
	NkNodeId out;
	MonteUnPrincipled(g, t, &out);
	const NkMatCompileResult sans = NkMatCompileToNkSL(gReg, g);
	const NkMatCompileResult avec = CompileAvecNoeudRang4(NK_MN_GEOMETRY, "position", nullptr, nullptr, nullptr);

	const char *kVue = "nkViewV";
	const bool absente = sans.ok && !ContientSansCasse(sans.source, kVue);
	const bool presente = avec.ok && ContientSansCasse(avec.source, kVue);
	NkString d;
	d = NkFormat("sans noeud de vue : ligne absente={0} | avec : presente={1}", absente ? 1 : 0, presente ? 1 : 0);
	Cas("rang4/vue-declaree-seulement-si-utile", absente && presente, d);
}

static void CasRang4CanalNommeRefuseEnSeNommant() {
	// ⚠️ LE CAS QUI PORTE LA DECISION DU RANG, ET C'EST LA REGLE 3 A L'ETAGE DE
	// LA GEOMETRIE. Un `UV Map` qui nomme un canal inexistant NE DOIT PAS rendre
	// (0,0) : ce serait « rien » rendu comme « zero ». L'auteur brancherait le
	// noeud, obtiendrait du noir, et chercherait du cote de son materiau.
	//
	// Le nom est verifiable A LA COMPILATION -- la liste est close, derivee des
	// varyings que le shader porte reellement -- donc on refuse, en nommant le
	// canal demande ET ce qui existe.
	//
	// DISCRIMINE SUR TROIS ETATS QU'ON POURRAIT CONFONDRE :
	//   le canal valide passe ; le canal inconnu est REFUSE ; et la propriete
	//   ABSENTE est refusee aussi -- se rabattre sur « uv » ferait marcher le
	//   noeud sans que personne ait choisi, et le graphe changerait de sens tout
	//   seul le jour ou un second canal existera.
	const NkMatCompileResult bon = CompileAvecNoeudRang4(NK_MN_UV_MAP, "vector", NK_MPROP_CANAL_UV, "uv", nullptr);
	const NkMatCompileResult faux =
		CompileAvecNoeudRang4(NK_MN_UV_MAP, "vector", NK_MPROP_CANAL_UV, "uv_de_lightmap", nullptr);
	const NkMatCompileResult sansProp = CompileAvecNoeudRang4(NK_MN_UV_MAP, "vector", nullptr, nullptr, nullptr);
	const NkMatCompileResult attrBon =
		CompileAvecNoeudRang4(NK_MN_ATTRIBUTE, "color", NK_MPROP_ATTRIBUT, "color", nullptr);
	const NkMatCompileResult attrFaux =
		CompileAvecNoeudRang4(NK_MN_ATTRIBUTE, "color", NK_MPROP_ATTRIBUT, "usure", nullptr);

	// le refus NOMME le canal demande, et dit ce qui existe
	const bool nommeLeCanal = !faux.ok && ContientSansCasse(faux.error, "uv_de_lightmap") &&
							  ContientSansCasse(faux.error, "disponibles");
	const bool nommeLAttribut = !attrFaux.ok && ContientSansCasse(attrFaux.error, "usure") &&
								ContientSansCasse(attrFaux.error, "disponibles");
	// ⚠️ et RIEN n'est emis : un refus qui rendrait quand meme une source
	// laisserait un appelant distrait compiler un shader a moitie forme.
	const bool rienEmis = faux.source.Size() == 0 && attrFaux.source.Size() == 0 && sansProp.source.Size() == 0;
	// le bon canal lit le VRAI varying
	const bool lisUV = bon.ok && ContientSansCasse(bon.source, "= vec3(vUV, 0.0)");
	const bool lisColor = attrBon.ok && ContientSansCasse(attrBon.source, "= vColor.rgb");

	NkString d;
	d = NkFormat("canal valide compile={0} et lit vUV={1} | canal inconnu refuse en se nommant={2} | attribut valide "
				 "lit vColor={3} | attribut inconnu refuse={4} | propriete absente refusee={5} | rien emis={6}",
				 bon.ok ? 1 : 0, lisUV ? 1 : 0, nommeLeCanal ? 1 : 0, lisColor ? 1 : 0, nommeLAttribut ? 1 : 0,
				 sansProp.ok ? 0 : 1, rienEmis ? 1 : 0);
	Cas("rang4/canal-nomme-refuse-en-se-nommant",
		bon.ok && lisUV && nommeLeCanal && attrBon.ok && lisColor && nommeLAttribut && !sansProp.ok && rienEmis, d);
}

static void CasRang4ObjectInfoIndisponibleAvecSaRaison() {
	// ⚠️ `Object Info` N'EST PAS DANS LE CATALOGUE, ET C'EST MESURE, PAS SUPPOSE.
	//
	// Le shader engendre ne dispose que du bloc camera et des parametres
	// exposes : ni matrice de modele, ni index, ni couleur, ni graine d'objet.
	// Rien a lire. Le livrer en rendant des zeros serait « rien » rendu comme
	// « zero », a l'echelle d'un noeud entier.
	//
	// DISCRIMINE SUR LA DIFFERENCE ENTRE INCONNU ET INDISPONIBLE : un refus sec
	// (« type inconnu ») enverrait l'auteur chercher une faute de frappe. Ici on
	// exige que le message porte CE QUI MANQUE -- le bloc uniforme par objet --
	// et qu'il dise que le noeud n'est pas refuse par principe.
	const char *pq = NkMatPourquoiIndisponible(NK_MN_OBJECT_INFO);
	// il n'est pas instanciable
	NkNodeGraph g;
	NkMatRegisterTypes(g);
	const NkNodeId n = NkMatAddNode(gReg, g, NK_MN_OBJECT_INFO);

	// et un graphe qui le porte quand meme (venu d'un fichier) est refuse EN
	// EXPLIQUANT
	NkNodeGraph h;
	NkMatTypes t = NkMatRegisterTypes(h);
	NkNodeId out;
	MonteUnPrincipled(h, t, &out);
	const NkNodeId fantome = h.AddNode(NK_MN_OBJECT_INFO, "Object Info");
	h.AddSocket(fantome, "color", t.color, NkSocketDir::Output);
	const NkMatCompileResult r = NkMatCompileToNkSL(gReg, h);
	const bool expliqueEnCompilant = !r.ok && ContientSansCasse(r.error, "mat.info_objet") &&
									 ContientSansCasse(r.error, "par objet") &&
									 ContientSansCasse(r.error, "pas refuse par principe");

	NkString d;
	d = NkFormat("raison declaree={0} | non instanciable={1} | la compilation explique ce qui manque={2}",
				 pq ? 1 : 0, n == NK_NODE_INVALID ? 1 : 0, expliqueEnCompilant ? 1 : 0);
	Cas("rang4/object-info-indisponible-avec-sa-raison",
		pq != nullptr && n == NK_NODE_INVALID && expliqueEnCompilant, d);
}


// ── LE VERDICT DE GLSLANG, ISOLE, AVEC SON MESSAGE ──────────────────────────
// Lu au MOT MAGIQUE et jamais a `success` -- meme raison que dans
// CompileSurLesBackends : quand glslang refuse, `success` reste a 1.
static bool GlslangAccepte(const NkString &nksl, NkString *outMsg) {
	NkSLCompiler c;
	NkSLCompileResult sp = c.Compile(nksl, NkSLStage::NK_FRAGMENT, NkSLTarget::NK_SPIRV);
	bool vrai = false;
	if (sp.bytecode.Size() >= 4) {
		const uint8 *o = sp.bytecode.Data();
		vrai = (o[0] == 0x03 && o[1] == 0x02 && o[2] == 0x23 && o[3] == 0x07);
	}
	if (outMsg) {
		*outMsg = NkString("");
		for (uint32 i = 0; i < (uint32)sp.errors.Size(); ++i) {
			if (i)
				outMsg->Append(" | ");
			outMsg->Append(sp.errors[i].message);
		}
	}
	return vrai;
}

// Remplace toutes les occurrences de `motif` par `par` dans un texte.
static NkString RemplaceTout(const NkString &texte, const char *motif, const char *par) {
	NkString sortie;
	const char *p = texte.CStr();
	const uint32 n = (uint32)NkString(motif).Size();
	while (*p) {
		bool egal = true;
		for (uint32 k = 0; k < n; ++k)
			if (p[k] != motif[k]) {
				egal = false;
				break;
			}
		if (egal) {
			sortie.Append(par);
			p += n;
		} else {
			sortie.Append(*p);
			++p;
		}
	}
	return sortie;
}

// ═══════════════════════════════════════════════════════════════════════════
// « EMIS CREDIBLE » CONTRE « EMIS QUI ECHOUE » — LE CAS QUE RODOLF A DEMANDE
// ═══════════════════════════════════════════════════════════════════════════
//
// D'OU IL VIENT. Le 23/08, tout un lot a ete decide sur cet argument : « quand
// le canal est nul, mieux vaut emettre un nom qui n'existe dans AUCUN backend --
// le shader echoue en PORTANT le mot -- que rendre un pixel que personne ne
// mettrait en doute ». L'argument etait ecrit a cote du code et AUCUN CAS NE LE
// MESURAIT : le banc ne savait dire que « a-t-on emis ? ». Les mutations M13
// (repli plausible) et M14 (jeton imprononcable) etaient INDISCERNABLES pour
// lui, alors que c'est precisement la difference qui compte pour l'auteur.
//
// ⚠️ CE QUE CE CAS MESURE, ET CE QU'IL NE MESURE PAS. Il ne mesure PAS le site
// d'emission du rang 4 : ce site est INATTEIGNABLE, la passe de refus le couvre.
// Il mesure LE PRINCIPE SUR LEQUEL CE SITE S'APPUIE, sur une source reellement
// emise. C'est une limite, elle est ecrite ici, et elle ne s'efface pas : le
// jour ou quelqu'un croira que ce cas protege le site, cette phrase le
// detrompera.
//
// LES TROIS ETATS, ET C'EST LA COMPARAISON QUI EST LA MESURE :
//   LEGITIME  la source telle qu'elle est emise pour un canal valide
//   CREDIBLE  ce qu'un repli plausible produirait -- et le cas exige que ce
//             soit OCTET POUR OCTET la meme source que LEGITIME
//   ECHOUE    le jeton imprononcable
//
// 🔴 LE RESULTAT QUI JUSTIFIE TOUT LE LOT : « credible » est IDENTIQUE a
// « legitime ». Pas « proche » : identique. Aucun controle de compilation,
// aucune comparaison de source, aucun banc ne peut les separer -- il n'y a
// litteralement rien a comparer. Tandis que « echoue » se voit du premier coup,
// et son message PORTE LE MOT.
static void CasEmisCredibleContreEmisQuiEchoue() {
	// Un graphe qui porte un `UV Map` sur un canal VALIDE : sa source est celle
	// qu'un repli plausible produirait pour un canal INVALIDE, puisque le repli
	// consistait justement a ecrire `vUV` quoi qu'il arrive.
	NkNodeGraph g;
	NkMatTypes t = NkMatRegisterTypes(g);
	NkNodeId out;
	MonteUnPrincipled(g, t, &out);
	const NkNodeId uv = NkMatAddNode(gReg, g, NK_MN_UV_MAP);
	g.SetProp(uv, NK_MPROP_CANAL_UV, NkValueText(t.real, "uv"));
	const NkNodeId so = PoseSortie(g, t, "essai_uv", "par_pixel_cible");
	g.Connect(uv, "vector", so, "color");
	const NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);

	// ── LEGITIME ─────────────────────────────────────────────────────────
	NkString msgLegitime;
	const bool legitimeCompile = r.ok && GlslangAccepte(r.source, &msgLegitime);
	NkString detBackLeg;
	const uint32 backLeg = r.ok ? CompileSurLesBackends(r.source, detBackLeg, nullptr) : 0;

	// ── CREDIBLE : ce que le repli plausible aurait ecrit ─────────────────
	// Le repli ecrivait `vUV` quel que soit le canal demande. Pour un canal
	// INVALIDE il aurait donc produit EXACTEMENT cette source-ci.
	const NkString credible = r.source;
	const bool credibleIdentique = r.ok && (credible == r.source) && credible.Size() > 0;
	NkString msgCredible;
	const bool credibleCompile = r.ok && GlslangAccepte(credible, &msgCredible);

	// ── ECHOUE : le jeton qui n'existe dans aucun backend ─────────────────
	//
	// ⚠️ ON NE SUBSTITUE QUE LE SITE D'USAGE, PAS LA DECLARATION. Ma premiere
	// version remplacait `vUV` PARTOUT -- y compris dans
	// `@location(2) in vec2 vUV;`. Le jeton devenait alors parfaitement
	// DECLARE, glslang l'acceptait, et le cas rendait « glslang=1 » sur ce qui
	// devait echouer. Une mutation qui renomme aussi la declaration ne modele
	// pas le defaut : elle renomme une variable. C'est la premiere facon dont
	// un cas juste ne mesure rien -- il manquait la MATIERE -- rencontree dans
	// le cas cense mesurer l'aveuglement des autres.
	const NkString casse =
		r.ok ? RemplaceTout(r.source, "vec3(vUV, 0.0)", "vec3(nkCANAL_UV_NON_VALIDE, 0.0)") : NkString("");
	const bool aBienSubstitue = r.ok && !(casse == r.source) && ContientSansCasse(casse, "nkCANAL_UV_NON_VALIDE");
	NkString msgCasse;
	const bool casseCompile = r.ok && GlslangAccepte(casse, &msgCasse);
	// ⚠️ « ECHOUER » NE SUFFIT PAS : LE MESSAGE DOIT PORTER LE MOT. Un echec
	// muet envoie l'auteur chercher dans son graphe. C'est toute la difference
	// entre « echoue » et « echoue EN SE NOMMANT », et c'est elle que
	// l'argument du 23/08 revendiquait.
	//
	// ── CE CAS A CHANGE DE VERDICT LE JOUR MEME, ET C'EST VOULU ──────────
	// Ecrit le 2026-08-23 apres-midi, il EPINGLAIT l'etat mesure : le mot
	// n'arrivait PAS. `NkSLCompiler.cpp`, cas NK_SPIRV, remplacait le resultat
	// de glslang par celui du generateur GLSL -- les erreurs mouraient la et
	// `errors` ressortait VIDE. Le cas exigeait donc l'ABSENCE du mot, pour
	// qu'un correctif ne puisse pas passer inapercu.
	//
	// Il a rougi le soir meme, quand le correctif est arrive. C'est exactement
	// ce qu'on lui demandait de faire.
	//
	// 📌 UNE LIMITATION EPINGLEE PAR UN CAS SE CORRIGE EN LE FAISANT ROUGIR.
	// Ecrite dans un commentaire, elle aurait survecu au correctif et serait
	// devenue fausse en silence -- c'est la forme la plus courante de la
	// regle 5, et celle qu'on a payee deux fois cette semaine.
	//
	// Ce qu'on exige maintenant, et pourquoi les deux :
	//   LE MOT, parce que c'est lui qui envoie l'auteur au bon endroit ;
	//   L'ETIQUETTE D'ETAPE, parce que deux journaux se rencontrent dans ce
	//   resultat -- celui du generateur GLSL-Vulkan, qui a REUSSI, et celui de
	//   glslang, qui a refuse. Concatenes sans etiquette, l'auteur lirait
	//   « erreur ligne 42 » sans savoir QUI se plaint.
	const bool leMessagePorteLeMot = ContientSansCasse(msgCasse, "nkCANAL_UV_NON_VALIDE");
	const bool leMessageDitSonEtape = ContientSansCasse(msgCasse, "glslang");

	// ── ET L'INSTRUMENT AVEUGLE, NOMME ───────────────────────────────────
	// Les quatre colonnes de backends attestent la GENERATION, pas la
	// compilation : elles disent « gen » sur les DEUX. On l'exige, pour que
	// personne ne croie qu'elles protegent de quoi que ce soit.
	NkString detBackCasse;
	const uint32 backCasse = r.ok ? CompileSurLesBackends(casse, detBackCasse, nullptr) : 0;
	// 4 generations + GLSLANG. Le legitime fait 5, le casse fait 4 : les quatre
	// premieres colonnes ne voient RIEN.
	const bool backendsAveugles = backLeg == 5 && backCasse == 4;

	NkString d;
	d = NkFormat("LEGITIME : glslang={0} backends={1}/5 | CREDIBLE : IDENTIQUE au legitime octet pour octet={2} "
				 "(rien a comparer, rien ne peut les separer) glslang={3} | ECHOUE : substitue={4} "
				 "glslang REFUSE={5} | les 4 colonnes de generation sont AVEUGLES ({6}/5 contre {7}/5)={8} | "
				 "et le message PORTE le mot={9} en disant DE QUELLE ETAPE il vient={10} : {11}",
				 legitimeCompile ? 1 : 0, backLeg, credibleIdentique ? 1 : 0, credibleCompile ? 1 : 0,
				 aBienSubstitue ? 1 : 0, casseCompile ? 0 : 1, backLeg, backCasse, backendsAveugles ? 1 : 0,
				 leMessagePorteLeMot ? 1 : 0, leMessageDitSonEtape ? 1 : 0,
				 msgCasse.Size() ? msgCasse : NkString("(vide)"));
	Cas("rang4/emis-credible-contre-emis-qui-echoue",
		legitimeCompile && credibleIdentique && credibleCompile && aBienSubstitue && !casseCompile &&
			backendsAveugles && leMessagePorteLeMot && leMessageDitSonEtape,
		d);
}


// ═══════════════════════════════════════════════════════════════════════════
// LE NODAL APPUIE LE NON NODAL — UN TYPE DU GRAPHE DOIT ETRE EXPRIMABLE
// ═══════════════════════════════════════════════════════════════════════════
//
// Regle d'ordre de Rodolf, reaffirmee le 2026-08-23 : « le systeme nodal vient
// APPUYER les systemes non nodaux. Donc il faut toujours le non nodal, et
// APRES definir le nodal par-dessus. »
//
// 🔴 CE CAS EXISTE PARCE QUE J'AI CONSTRUIT LES TYPES COMPOSES AVANT DE POSER
// LA QUESTION. Le registre savait declarer `Rihen::Difficulte` ; rien ne
// verifiait que la compilation avait UN ENDROIT OU L'ECRIRE. C'est une
// contrainte d'EXISTENCE, pas de style, et elle se mesure ici.
//
// DISCRIMINE DANS LES DEUX SENS : un type FEUILLE passe (sinon la garde
// refuserait tout et le cas serait vert pour rien), un type COMPOSE est refuse
// en nommant ce qui manque DU COTE NON NODAL.
static void CasTypeComposeRefuseCarNonExprimable() {
	// ── TEMOIN POSITIF : les types feuilles compilent, comme toujours ────
	NkNodeGraph ok;
	NkMatTypes tok = NkMatRegisterTypes(ok);
	NkNodeId outOk;
	MonteUnPrincipled(ok, tok, &outOk);
	const NkMatCompileResult rOk = NkMatCompileToNkSL(gReg, ok);

	// ── LE COMPOSE : declarable dans le coeur, PAS ecrivable dans NkMaterial
	NkNodeGraph g;
	NkMatTypes t = NkMatRegisterTypes(g);
	NkNodeId out;
	const NkNodeId bsdf = MonteUnPrincipled(g, t, &out);
	NkTypeMember diff[3];
	diff[0].name = NkString("Facile");
	diff[1].name = NkString("Normal");
	diff[2].name = NkString("Difficile");
	NkString errType;
	const NkTypeId tEnum = g.RegisterCompositeType("Rihen::Difficulte", NkTypeKind::Enum, diff, 3, &errType);
	// ⚠️ LE COEUR L'ACCEPTE, ET C'EST VOULU : il sert aussi l'AnimGraph et
	// NkUIDesign, dont le modele non nodal n'est PAS NkMaterial. La contrainte
	// est PAR DOMAINE ; c'est au domaine de la faire respecter.
	const bool coeurAccepte = tEnum != NK_TYPE_INVALID && errType.Size() == 0;

	g.AddSocket(bsdf, "difficulte", tEnum, NkSocketDir::Input);
	const NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);

	const bool refuse = !r.ok;
	const bool nommeLeType = ContientSansCasse(r.error, "Rihen::Difficulte");
	const bool nommeLeGenre = ContientSansCasse(r.error, "enumeration");
	const bool nommeLaPrise = ContientSansCasse(r.error, "difficulte");
	// ⚠️ ET LE REFUS DOIT DIRE CE QUI MANQUE DU COTE NON NODAL. « type non
	// supporte » enverrait l'auteur chercher un reglage ; ici on veut qu'il
	// comprenne que c'est NkMaterial qui n'a pas d'endroit ou l'ecrire.
	const bool ditOuCaCoince = ContientSansCasse(r.error, "NkMaterial");
	const bool ditCeQuiExiste = ContientSansCasse(r.error, "texture") && ContientSansCasse(r.error, "booleen");
	const bool rienEmis = r.source.Size() == 0;

	NkString d;
	d = NkFormat("TEMOIN : les feuilles compilent={0} | le COEUR accepte le compose={1} (il sert aussi "
				 "l AnimGraph) | le MATERIAU le refuse={2} en nommant le type={3} le genre={4} la prise={5} | "
				 "dit que c est NkMaterial qui n a pas de place={6} et ce qu il porte={7} | rien emis={8} | {9}",
				 rOk.ok ? 1 : 0, coeurAccepte ? 1 : 0, refuse ? 1 : 0, nommeLeType ? 1 : 0, nommeLeGenre ? 1 : 0,
				 nommeLaPrise ? 1 : 0, ditOuCaCoince ? 1 : 0, ditCeQuiExiste ? 1 : 0, rienEmis ? 1 : 0, r.error);
	Cas("nonnodal/type-compose-refuse-car-non-exprimable",
		rOk.ok && coeurAccepte && refuse && nommeLeType && nommeLeGenre && nommeLaPrise && ditOuCaCoince &&
			ditCeQuiExiste && rienEmis,
		d);
}

static void CasRang4ParPixelContagieux() {
	// Les cinq noeuds du rang 4 sont des SOURCES par pixel : ils lisent une
	// donnee interpolee, donc leur valeur change d'un pixel a l'autre meme sans
	// aucune entree connectee.
	//
	// DISCRIMINE PAR LA CONSEQUENCE, pas par le drapeau : une sortie nommee
	// « par_materiau » alimentee par l'un d'eux doit etre REFUSEE, et le refus
	// doit NOMMER le noeud coupable. Lire `proto->parPixel` directement ne
	// prouverait que la table ; ici on prouve que la table SERT.
	uint32 bons = 0;
	NkString detail;
	const char *cles[5] = {NK_MN_FRESNEL, NK_MN_LAYER_WEIGHT, NK_MN_GEOMETRY, NK_MN_UV_MAP, NK_MN_ATTRIBUTE};
	const char *prises[5] = {"fac", "fresnel", "position", "vector", "color"};
	const char *props[5] = {nullptr, nullptr, nullptr, NK_MPROP_CANAL_UV, NK_MPROP_ATTRIBUT};
	const char *vals[5] = {nullptr, nullptr, nullptr, "uv", "color"};
	for (uint32 i = 0; i < 5; ++i) {
		NkNodeGraph g;
		NkMatTypes t = NkMatRegisterTypes(g);
		NkNodeId out;
		MonteUnPrincipled(g, t, &out);
		const NkNodeId n = NkMatAddNode(gReg, g, cles[i]);
		if (props[i])
			g.SetProp(n, props[i], NkValueText(t.real, vals[i]));
		const NkNodeId so = PoseSortie(g, t, "essai", "par_materiau");
		// la prise « value » est un reel : on passe par le canal qui convient
		const bool estCouleur = (i == 4);
		g.Connect(n, prises[i], so, estCouleur ? "color" : (i == 2 || i == 3 ? "color" : "value"));
		const NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
		const bool ok = !r.ok && ContientSansCasse(r.error, "depend du pixel") && ContientSansCasse(r.error, cles[i]);
		if (ok)
			++bons;
		detail.Append(NkFormat("[{0}={1}] ", NkString(cles[i]), ok ? 1 : 0));
	}
	Cas("rang4/tous-par-pixel-et-le-refus-les-nomme", bons == 5, NkFormat("{0}(5 attendus)", detail));
}

static void CasB1ToutMateriauDeclareEtEcritLaSecondeCible() {
	// ⚠️ LE CAS QUE RODOLF A DEMANDE EN PREMIER, et le fait materiel qui le
	// justifie : UNE SORTIE MRT NON ECRITE SUR UN PIXEL COUVERT EST INDEFINIE --
	// ni conservee, ni nulle. Un materiau qui se contenterait de ne pas la
	// declarer laisserait dans le tampon ce qui s'y trouvait.
	//
	// On mesure donc les TROIS regimes, et ils doivent tous ecrire :
	//   - aucune sortie nommee du tout        -> declare, ecrit (0,0,0,0) ;
	//   - une sortie « par_materiau » (etage a) -> declare, ecrit (0,0,0,0) AUSSI,
	//     parce que (a) est une constante calculee sur le processeur : lui faire
	//     payer une ecriture par pixel viderait l'argument « quasi gratuit » ;
	//   - une sortie « par_pixel_cible » (b1)  -> declare, ecrit la VALEUR avec
	//     A = 1, et N'ECRIT PAS la ligne de zero.
	//
	// DISCRIMINE sur la derniere ligne surtout : un emetteur qui ecrirait la
	// valeur PUIS le zero par-dessus rendrait un tampon vide sur un materiau qui
	// porte pourtant la sortie -- et le shader compilerait parfaitement.
	const NkMatCompileResult sans = CompileAvecSortie(nullptr, nullptr);
	const NkMatCompileResult etageA = CompileAvecSortie("par_materiau", nullptr);
	const NkMatCompileResult b1 = CompileAvecSortie("par_pixel_cible", nullptr);

	const char *kDecl = "@location(1) out vec4 fragAux;";
	const char *kZero = "fragAux = vec4(0.0, 0.0, 0.0, 0.0);";

	const bool declSans = sans.ok && ContientSansCasse(sans.source, kDecl);
	const bool declA = etageA.ok && ContientSansCasse(etageA.source, kDecl);
	const bool declB1 = b1.ok && ContientSansCasse(b1.source, kDecl);
	const bool zeroSans = sans.ok && ContientSansCasse(sans.source, kZero);
	const bool zeroA = etageA.ok && ContientSansCasse(etageA.source, kZero);
	const bool zeroB1 = b1.ok && ContientSansCasse(b1.source, kZero);
	const bool valeurB1 = b1.ok && ContientSansCasse(b1.source, "fragAux = vec4(vec3(");

	NkString d;
	d = NkFormat("SANS sortie : declare={0} ecrit zero={1} | etage (a) : declare={2} ecrit zero={3} | (b1) : "
				 "declare={4} ecrit la valeur={5} et PAS de zero={6}",
				 declSans ? 1 : 0, zeroSans ? 1 : 0, declA ? 1 : 0, zeroA ? 1 : 0, declB1 ? 1 : 0,
				 valeurB1 ? 1 : 0, zeroB1 ? 0 : 1);
	Cas("b1/tout-materiau-declare-et-ecrit-la-seconde-cible",
		declSans && zeroSans && declA && zeroA && declB1 && valeurB1 && !zeroB1, d);
}

static void CasB1ValiditeDistingueZeroDeAbsent() {
	// ⚠️ LE PIEGE QUE RODOLF A NOMME, ET LA RAISON D'ETRE DU QUATRIEME CANAL.
	//
	// Le materiau (b1) de ce cas porte une sortie dont la valeur vaut EXACTEMENT
	// ZERO. Celui d'a cote ne porte aucune sortie. Dans le tampon, leurs trois
	// premiers canaux sont IDENTIQUES -- zero des deux cotes.
	//
	//   Un lecteur qui ignorerait A lirait 0.0 dans les deux cas : une humidite
	//   nulle parfaitement credible sur un materiau qui n'a jamais entendu
	//   parler d'humidite.
	//
	// C'est A, ET RIEN D'AUTRE, qui les distingue. Le cas l'exige litteralement :
	// le porteur ecrit un alpha de 1.0, l'autre un alpha de 0.0.
	//
	// DISCRIMINE : un emetteur qui ecrirait A = 1.0 partout -- le reflexe, parce
	// qu'un alpha opaque est ce qu'on ecrit d'habitude -- rendrait les deux
	// pixels indiscernables, et le tampon se remplirait de zeros presentes comme
	// des mesures.
	const NkMatCompileResult porte = CompileAvecSortie("par_pixel_cible", nullptr);
	const NkMatCompileResult neportePas = CompileAvecSortie(nullptr, nullptr);

	const bool alphaUn = porte.ok && ContientSansCasse(porte.source, ", 1.0);");
	const bool alphaZero = neportePas.ok && ContientSansCasse(neportePas.source, "fragAux = vec4(0.0, 0.0, 0.0, 0.0);");
	// et le porteur ne doit PAS ecrire l'alpha zero
	const bool porteurSansZero = porte.ok && !ContientSansCasse(porte.source, "fragAux = vec4(0.0, 0.0, 0.0, 0.0);");

	NkString d;
	d = NkFormat("valeur nulle mais PORTEE : alpha=1={0} et aucun alpha 0={1} | sortie ABSENTE : alpha=0={2} | seul A "
				 "les distingue",
				 alphaUn ? 1 : 0, porteurSansZero ? 1 : 0, alphaZero ? 1 : 0);
	Cas("b1/validite-distingue-zero-de-absent", alphaUn && porteurSansZero && alphaZero, d);
}

static void CasB1LaValeurVientDuGraphe() {
	// « Ca declare » ne prouve pas « ca calcule ». Ici la sortie par pixel est
	// alimentee par un BRUIT -- une source intrinsequement par pixel -- et
	// l'ecriture doit referencer LA LOCALE QUE CE NOEUD A DECLAREE, pas une
	// constante ni la locale d'un voisin.
	//
	// DISCRIMINE : un emetteur qui ecrirait `vec4(vec3(0.0), 1.0)` passerait tous
	// les cas precedents -- la sortie serait declaree, ecrite, avec le bon alpha,
	// et le shader compilerait. Seul le nom de la locale distingue un tampon qui
	// porte une mesure d'un tampon qui porte un zero bien forme.
	NkNodeGraph g;
	NkMatTypes t = NkMatRegisterTypes(g);
	NkNodeId out;
	MonteUnPrincipled(g, t, &out);
	const NkNodeId bruit = NkMatAddNode(gReg, g, NK_MN_NOISE);
	const NkNodeId so = PoseSortie(g, t, "grain", "par_pixel_cible");
	g.Connect(bruit, "fac", so, "value");
	const NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);

	// La locale du bruit suit la convention `n<id>_fac`.
	NkString attendu = NkFormat("fragAux = vec4(vec3(n{0}_fac", bruit);
	const bool lit = r.ok && ContientSansCasse(r.source, attendu.CStr());
	NkString be;
	NkString err;
	const uint32 nb = r.ok ? CompileSurLesBackends(r.source, be, &err) : 0u;

	NkString d;
	d = NkFormat("compile={0} | l ecriture reference la locale du bruit ('{1}')={2} | {3}", r.ok ? 1 : 0, attendu,
				 lit ? 1 : 0, be);
	Cas("b1/la-valeur-vient-du-graphe", r.ok && lit && nb == 5, d);
}

static void CasB1ValeurProcesseurAbsenteEtDite() {
	// ⚠️ LE MEME PIEGE QUE LE CANAL ALPHA, UN ETAGE PLUS HAUT — et je ne l'avais
	// pas vu en concevant (b1).
	//
	// `NkMatSortieMateriau::valeur[3]` est la valeur calculee cote processeur.
	// Pour une sortie « par_pixel_cible » elle n'existe pas : la valeur nait dans
	// le shader. Le tableau reste donc a ZERO -- et ce zero se lit exactement
	// comme « la valeur vaut zero », alors qu'il veut dire « il n'y a pas de
	// valeur ici ». C'est le mensonge plausible qu'on refuse partout ailleurs,
	// et il etait sur le point d'entrer par la porte de derriere.
	//
	// DISCRIMINE : les deux sorties du graphe portent une valeur processeur de
	// 0.0 -- l'une parce qu'elle vaut vraiment zero, l'autre parce qu'elle n'en a
	// pas. Un lecteur qui regarderait `valeur` sans regarder `valeurConnue` ne
	// pourrait PAS les distinguer. Le cas exige donc que le drapeau, lui, les
	// distingue.
	NkNodeGraph g;
	NkMatTypes t = NkMatRegisterTypes(g);
	NkNodeId out;
	MonteUnPrincipled(g, t, &out);

	// (a) une sortie par materiau qui vaut REELLEMENT zero
	const NkNodeId va = NkMatAddNode(gReg, g, NK_MN_VALUE);
	g.SetProp(va, NK_MPROP_VALUE, NkValueReal(t.real, 0.f));
	const NkNodeId sa = PoseSortie(g, t, "constante_nulle", "par_materiau");
	g.Connect(va, "value", sa, "value");

	// (b1) une sortie par pixel, dont la valeur processeur N'EXISTE PAS
	const NkNodeId bruit = NkMatAddNode(gReg, g, NK_MN_NOISE);
	const NkNodeId sb = PoseSortie(g, t, "grain", "par_pixel_cible");
	g.Connect(bruit, "fac", sb, "value");

	const NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	const NkMatSortieMateriau *pa = r.ok ? r.TrouveSortie("constante_nulle") : nullptr;
	const NkMatSortieMateriau *pb = r.ok ? r.TrouveSortie("grain") : nullptr;

	const bool aConnue = pa && pa->valeurConnue && pa->valeur[0] == 0.f && pa->composantes == 1;
	const bool bAbsente = pb && !pb->valeurConnue && pb->composantes == 1;
	// et les deux sont bien REMONTEES au moteur : une sortie (b1) qu'on
	// oublierait de declarer serait invisible du code de jeu.
	const bool lesDeux = r.ok && r.sorties.Size() == 2;

	NkString d;
	d = NkFormat("compile={0} sorties remontees={1} (2 attendues) | (a) « constante_nulle » : valeur connue={2} et "
				 "vaut {3} | (b1) « grain » : valeur processeur ABSENTE et dite={4} | les deux portent 0.0, seul le "
				 "drapeau les distingue",
				 r.ok ? 1 : 0, (uint32)r.sorties.Size(), (pa && pa->valeurConnue) ? 1 : 0,
				 pa ? pa->valeur[0] : 0.f, bAbsente ? 1 : 0);
	Cas("b1/valeur-processeur-absente-et-dite", lesDeux && aConnue && bAbsente, d);
}

static void CasB1RefusNommes() {
	// Les deux ambiguites, et elles se refusent en se nommant.
	//
	// (a) demander une sortie qui n'existe pas. ⚠️ Rendre un tampon VIDE serait
	//     le repli plausible : il se lirait comme « ce materiau ne porte pas
	//     cette valeur », indiscernable du cas legitime, et donc jamais corrige.
	// (b) deux sorties « par_pixel_cible » et aucune demandee. Prendre « la
	//     premiere » rendrait une valeur credible issue d'une sortie que
	//     personne n'a demandee.
	const NkMatCompileResult inconnue = CompileAvecSortie("par_pixel_cible", "nexiste_pas");

	NkNodeGraph g;
	NkMatTypes t = NkMatRegisterTypes(g);
	NkNodeId out;
	MonteUnPrincipled(g, t, &out);
	const NkNodeId v1 = NkMatAddNode(gReg, g, NK_MN_VALUE);
	const NkNodeId v2 = NkMatAddNode(gReg, g, NK_MN_VALUE);
	const NkNodeId s1 = PoseSortie(g, t, "humidite", "par_pixel_cible");
	const NkNodeId s2 = PoseSortie(g, t, "usure", "par_pixel_cible");
	g.Connect(v1, "value", s1, "value");
	g.Connect(v2, "value", s2, "value");
	const NkMatCompileResult ambigu = NkMatCompileToNkSL(gReg, g);
	// et la MEME scene, levee en nommant la sortie voulue : c'est le temoin.
	NkMatCompileOptions opt;
	opt.sortieParPixel = "usure";
	const NkMatCompileResult leve = NkMatCompileToNkSL(gReg, g, opt);

	const bool nomInconnu = !inconnue.ok && ContientSansCasse(inconnue.error, "nexiste_pas");
	const bool deuxRefus = !ambigu.ok && ContientSansCasse(ambigu.error, "il faut dire laquelle");
	const bool leveOk = leve.ok && ContientSansCasse(leve.source, "fragAux = vec4(vec3(");

    NkString d;
	d = NkFormat("sortie demandee absente -> refus qui la nomme={0} ('{1}') | deux (b1) sans demande -> refus={2} | "
				 "temoin : la meme scene compile quand on nomme la sortie={3}",
				 nomInconnu ? 1 : 0, inconnue.error, deuxRefus ? 1 : 0, leveOk ? 1 : 0);
	Cas("b1/refus-nommes", nomInconnu && deuxRefus && leveOk, d);
}

static void CasSortieParPixelDeuxCiblesCouleur() {
	NkSLCompiler c;
	struct {
			NkSLTarget cible;
			const char *nom;
			bool hlsl;
	} cibles[4] = {
		{NkSLTarget::NK_GLSL, "GL", false},
		{NkSLTarget::NK_GLSL_VULKAN, "VK", false},
		{NkSLTarget::NK_HLSL_DX11, "DX11", true},
		{NkSLTarget::NK_HLSL_DX12, "DX12", true},
	};
	NkString detail;
	uint32 genere = 0;
	bool semantiquePartout = true;
	for (uint32 t = 0; t < 4; ++t) {
		NkSLCompileResult r = c.Compile(NkString(kDeuxCibles), NkSLStage::NK_FRAGMENT, cibles[t].cible);
		if (r.success)
			++genere;
		// Le SEMANTIQUE du second attachement. C'est lui qui distingue « deux
		// sorties » de « deux ecritures sur la meme sortie ».
		const bool second = r.success && (cibles[t].hlsl ? ContientSansCasse(r.source, "SV_Target1")
														 : ContientSansCasse(r.source, "location = 1"));
		// et le PREMIER doit toujours etre la : un generateur qui renommerait
		// tout en `SV_Target1` passerait le controle ci-dessus.
		const bool premier = r.success && (cibles[t].hlsl ? ContientSansCasse(r.source, "SV_Target0")
														 : ContientSansCasse(r.source, "location = 0"));
		if (!second || !premier)
			semantiquePartout = false;
		if (r.success && (!second || !premier) && getenv("NK_DUMP"))
			logger.Info("\n--- {0} : les deux attachements n ont pas traverse ---\n{1}\n--- fin ---",
						NkString(cibles[t].nom), r.source.CStr());
		detail.Append(NkFormat("{0}={1}{2}{3} ", NkString(cibles[t].nom), NkString(r.success ? "gen" : "ECHEC"),
							   NkString(premier ? "" : "(cible0 absente!)"), NkString(second ? "" : "(cible1 absente!)")));
	}
	// Le cinquieme verdict, lu au MOT MAGIQUE et jamais a `success`.
	bool spirv = false;
	{
		NkSLCompileResult sp = c.Compile(NkString(kDeuxCibles), NkSLStage::NK_FRAGMENT, NkSLTarget::NK_SPIRV);
		if (sp.bytecode.Size() >= 4) {
			const uint8 *o = sp.bytecode.Data();
			spirv = (o[0] == 0x03 && o[1] == 0x02 && o[2] == 0x23 && o[3] == 0x07);
		}
	}
	// Le temoin a une seule cible.
	bool temoin = false;
	{
		NkSLCompileResult sp = c.Compile(NkString(kUneCible), NkSLStage::NK_FRAGMENT, NkSLTarget::NK_SPIRV);
		if (sp.bytecode.Size() >= 4) {
			const uint8 *o = sp.bytecode.Data();
			temoin = (o[0] == 0x03 && o[1] == 0x02 && o[2] == 0x23 && o[3] == 0x07);
		}
	}
	detail.Append(NkFormat("GLSLANG={0} | temoin a une seule cible={1}", NkString(spirv ? "ok" : "ECHEC"),
						   NkString(temoin ? "ok" : "ECHEC")));
	Cas("sortie-par-pixel/deux-cibles-couleur", genere == 4 && semantiquePartout && spirv && temoin, detail);
}

int main() {
	// ⚠️ `Pattern()` est GLOBAL ET PERSISTANT : il modifie l'instance de journal
	// du PROCESSUS, pas l'appel. On le pose donc UNE FOIS ici, et pas a chaque
	// ligne — sinon chaque composant du moteur qui journalise ensuite herite
	// silencieusement du motif du banc. Consequence assumee : les lignes du
	// moteur perdent aussi leur horodatage dans ce processus ; elles restent
	// reconnaissables a leur crochet ouvrant, et se filtrent par « ^\[ ».
	logger.Pattern("%v");
	logger.Info("== NkMatGraphCheck - graphe de materiaux, couche 3 sur NKGraph ==");
	logger.Info("   regime : structure de donnees pure, aucun GPU, aucune fenetre.");
	logger.Info("   COUVRE aussi, depuis le 22/08 : les defauts de prise, les");
	logger.Info("   proprietes, la validation d un fichier, et le compilateur vers NkSL.");
	logger.Info("   NE couvre PAS : le RENDU (aucun GPU ici) -- seulement le fait que\n");
	CasTypesEnregistrement();
	CasConversionDirigee();
	CasInstancierPrincipled();
	CasPrototypeInconnu();

	// -- groupe/ : la MESURE demandee par Rodolf (R8) -- un type de noeud cree
	//    par l utilisateur a l execution est-il seulement representable ?
	CasGroupeCoeurAccepteUnTypeInconnu();
	CasGroupeCatalogueMateriauFerme();
	CasGroupeOuVitLeRefusDuTypeInconnu();
	CasGroupeRecursionRefuseeMaisOu();
	CasInstancePoseeInterfaceDeduite();

	// -- le catalogue OUVERT (arbitrage Rodolf du 22/08) : deux sources,
	//    une seule porte, et le refus d eclipse.
	CasCatalogueOuvertALExecution();
	CasCatalogueRefusDEclipse();
	CasDeuxDocumentsNeVoientPasLeursGroupes();
	CasPontInterfaceDeduiteDuSousGraphe();
	CasPontParPixelEstConservateur();

	// -- regroupement/ : le critere d acceptation de R9, ecrit AVANT
	//    l operation, et l operation ecrite pour le satisfaire.
	CasRegroupementAllerRetourIdentique();
	CasRegroupementInterfaceDeduite();
	CasRegroupementOrdreDeterministe();
	CasRegroupementRefusNomme();
	CasRegroupementConversionSurvitALaFrontiere();

	// -- (b1) : la seconde cible de rendu est-elle exprimable en NkSL ?
	CasSortieParPixelDeuxCiblesCouleur();

	// -- (b1) : la seconde cible, ecrite par TOUS les materiaux
	// -- rang 4 : la geometrie, et les deux noeuds qui NOMMENT une donnee
	CasRang4GeometrieEtVue();
	CasRang4VueSeulementSiUtile();
	CasRang4CanalNommeRefuseEnSeNommant();
	CasRang4ObjectInfoIndisponibleAvecSaRaison();
	CasRang4ParPixelContagieux();
	CasTypeComposeRefuseCarNonExprimable();
	CasEmisCredibleContreEmisQuiEchoue();

	CasB1ToutMateriauDeclareEtEcritLaSecondeCible();
	CasB1ValiditeDistingueZeroDeAbsent();
	CasB1LaValeurVientDuGraphe();
	CasB1ValeurProcesseurAbsenteEtDite();
	CasB1RefusNommes();
	CasTypesNonEnregistres();
	CasPrincipledVersSortie();
	CasCouleurDansShaderRefuse();
	CasDeuxSortiesRefusees();
	CasSortieNonReliee();
	CasAucuneSortie();
	CasMelangeShader();
	CasAllerRetourFichier();
	CasIndexDePriseContreNom();
	CasFichierOrdreDesSock();
	CasFichierPriseInconnueRefusee();
	CasFichierMigrationVersion1();
	CasTypesEspaceEtEmpreinte();
	CasTypesDeuxFichiersMemeNom();
	CasExecAriteCroisee();
	CasExecRefusCroiseNomme();
	CasAcycliciteUniverselle();
	CasExecAllerRetourOctetPourOctet();
	CasExecLePiegeDuTypeExec();
	CasLienQualifie();
	CasMachineAEtatsEstUnNoeud();

	// ── valeurs : defauts de prise et proprietes de noeud ────────────────
	CasDefautDePrise();
	CasJamaisRenseigneContreVide();
	CasProprieteDeNoeud();
	CasAllerRetourConstruitEnMemoire();
	CasPrecisionExacte();

	// ── validation : le chemin qui n'etait pas garde ─────────────────────
	CasFichierSainZeroDiagnostic();
	CasFichierCycle();
	CasFichierTypeIncompatible();
	CasFichierSensInverse();
	CasFichierNoeudEtSocketAbsents();
	CasFichierEntreeDoublee();
	CasFichierTypesInconnus();
	CasDefautHorsBornesNonRabattu();
	CasCompteRenduBorne();

	// -- phase 0 : le masque par texture, et les deux fichiers qui doivent
	//    s accorder sans que rien ne les y force --------------------------
	CasBindingDeclareDesDeuxCotes();
	CasQuatreCanauxPresents();
	CasMasqueCompileSurLesBackends();

	// -- le compilateur : graphe -> NkSL -> quatre backends -------------
	CasCompilePrincipled();
	CasCompileMixShader();
	CasCompileOrdreRespecte();
	CasCompileRefuseAvantDeGenerer();
	CasCompileEntreeNiCableeNiRenseignee();
	CasCompileGrapheVenuDunFichier();

	// -- la bibliotheque interrogee par TYPE DE PRISE (demande de Rodolf) --
	CasMenuPriseShader();
	CasMenuPriseCouleurEtReelle();
	CasMenuPriseInconnue();
	CasPriseConstanteSeulement();
	CasCompileRGBVersBaseColor();
	CasCompileValeurVersRoughness();

	// -- Math et Mix Color : une propriete qui porte une DECISION --------
	CasOperationInconnueRefusee();
	CasToutesLesOperationsCompilent();
	CasDivisionParZeroGardee();

	// -- ColorRamp : la premiere charge utile VARIABLE --------------------
	CasRampeChargeVariable();
	CasRampeRefusNommes();
	CasRampeInterpolation();
	CasRampeAllerRetourFichier();

	// -- Image Texture : la premiere RESSOURCE consommee par un graphe ----
	CasTexturePlafondRefusNomme();
	CasTextureBindingsDesDeuxCotes();
	CasTextureAucunBindingNeuf();
	CasTextureDeuxSorties();
	CasTextureCoordonneeEtMappage();

	// -- le relief : Normal Map, Bump, Separate XYZ ----------------------
	CasConventionNeChangePasLeShader();
	CasConventionInconnueRefusee();
	CasBaseTangenteSeulementSiUtile();
	CasNormaleVoyageJusquAuPuits();
	CasBumpDeriveesEtGarde();
	CasSepareTroisSorties();

	// -- les parametres EXPOSES : l'API publique du moteur ---------------
	CasExposeLitLeBloc();
	CasDecalagesStd140();
	CasAccordStd140HLSLSurLeHLSLEmis();
	CasPriseConnecteeEtExposeeRefusee();
	CasExposeRefusNommes();
	CasTypesExposablesListeClose();
	CasDefautEstCeluiDeLaPrise();
	CasJetonSuitLaDisposition();
	CasRechercheParNom();
	CasSansExpositionAucunBloc();
	CasBlocEtTexturesNeSeMarchentPasDessus();
	CasIncludeNeSeResoutPas();

	// -- rang 2 : le procedural ------------------------------------------
	CasSortieValeurCalculee();
	CasSortieCouleurEtRampe();
	CasSortieRefusParPixel();
	CasSortieMappageNEstPasParPixel();
	CasSortieExposeNeContaminePas();
	CasSelectionEvalueeSurLeProcesseur();
	CasSortieEtagesRefuses();
	CasSortieSourcesEtNoms();
	CasSortiePlusieursEtGraphesExistants();
	CasSortieDivisionParZero();
	CasSelectionCompileEtBranche();
	CasSelectionSeuilEtRefusDuZero();
	CasSelectionUniformeEtCeQuElleNEconomisePas();
	CasMapRangeEtBornage();
	CasClampBornesInversees();
	CasCombineXYZ();
	CasVectorMathOperationsEtAccord();
	CasVectorMathGardesNaN();

	// -- Float Curve : la SECONDE charge variable, et elle a un autre PAS ---
	CasCourbeChargeVariable();
	CasCourbeRefusNommes();
	CasCourbeDomaineDessine();
	CasCourbeFacNeutreEstUn();
	CasCourbeInterpolation();
	CasProceduralCompile();
	CasBriquesRecopieesVerbatim();
	CasBriquesSeulementSiUtiles();
	CasDegradeTypesEtRefus();
	CasOctavesBornees();
	CasOndeTypesEtRefus();
	CasVoronoiBorneFixe();
	CasBriquesJointsDecales();

	logger.Info("\n-- {0} cas, {1} echec(s) --", gCas, gEchecs);

	// ⚠️ CETTE PORTEE S'IMPRIME AVEC LE COMPTE, ET CE N'EST PAS DECORATIF.
	//
	// « 116 cas, 0 echec, 26 mutations sur 26 » se lit spontanement comme « la
	// seconde cible fonctionne ». Elle ne fonctionne pas : ELLE EST CORRECTEMENT
	// DECRITE. C'est beaucoup, ce n'est pas la meme chose, et c'est exactement
	// l'ecart qu'on passe des nuits a traquer ailleurs.
	//
	// La phrase est donc collee au chiffre qu'elle qualifie. Rangee dans une
	// ROADMAP ou un rapport, elle serait vraie et jamais lue.
	logger.Info("   PORTEE : (b1) est prouve jusqu'au NkSL EMIS, et pas au-dela.");
	logger.Info("   Aucune image n'a ete rendue, aucune valeur relue depuis la carte.");
	logger.Info("   Manquent : la cible R16G16B16A16_FLOAT cote RHI, son effacement a");
	logger.Info("   (0,0,0,0), et une lecture reelle.");
	return gEchecs == 0 ? 0 : 1;
}
