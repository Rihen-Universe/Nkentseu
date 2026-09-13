#pragma once
// -----------------------------------------------------------------------------
// @File    NkNodeGraph.h
// @Brief   Coeur du substrat de graphe de noeuds — modele de donnees PUR.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// DECISION D'ARCHITECTURE (cf. la roadmap NKGraph)
//   UN SEUL systeme de graphe pour tout l'ecosysteme : materiaux, VFX,
//   blueprint/scratch, modelisation procedurale, graphes d'animation, futur rig.
//   Trois couches, et la separation EST la regle :
//     3 — semantique metier : chez chaque consommateur (bibliotheques de noeuds,
//         backends d'execution) ;
//     2 — edition : le canevas, dans NKEditorKit ;
//     1 — CE MODULE : le modele pur.
//
//   « On unifie l'AUTORAT — modele, serialisation, interface — JAMAIS
//   l'EXECUTION. » Les materiaux se COMPILENT vers NkSL a l'edition ; la
//   modelisation PRODUIT DES COMMANDES rejouables ; le VFX EVALUE un plan aplati
//   par frame. Ces besoins sont incompatibles : le coeur fournit l'ordre
//   d'evaluation et les types, chaque domaine fournit ses noeuds et ce qu'il en
//   fait.
//
// GARDE-FOU N°1, ecrit dans la roadmap et tenu ici : AUCUN TYPE METIER dans le
//   coeur. Pas de « texture », pas d'« os », pas de « son ». Les types de sockets
//   sont des IDENTIFIANTS enregistres par les consommateurs. Un `if (type == …)`
//   metier dans ce fichier signerait la mort de l'architecture.
//
// POURQUOI « NkNodeGraph » ET NON « NkGraph » : `nkentseu::NkGraph<V, Alloc>`
//   EXISTE DEJA — c'est le graphe pondere generique de NKContainers (sommets,
//   aretes, DFS/BFS, 877 lignes). Deux choses differentes ne peuvent pas porter le
//   meme nom dans le meme espace de noms. Ce module-ci est un graphe de NOEUDS a
//   SOCKETS TYPES ; le conteneur ne l'est pas et ne doit pas etre detourne.
//
// EN-TETE PUR, sans cible de build, pour la meme raison que NkShortcutTable :
//   le harnais peut le tester sans lier quoi que ce soit. La cible deviendra
//   legitime quand un consommateur reel la liera.
//
// ZERO-STL : NkVector/NkString.
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace graph {

		// Identifiant de TYPE de socket. Le coeur ne sait pas ce qu'il designe : il
		// sait seulement que deux sockets de meme identifiant sont compatibles.
		// C'est ce qui lui permet de servir les materiaux et la modelisation sans
		// connaitre ni les textures ni les maillages.
		// ═════════════════════════════════════════════════════════════════════
		// L'IDENTITE D'UN TYPE : LE NOM QUALIFIE, ET L'EMPREINTE DE STRUCTURE
		// ═════════════════════════════════════════════════════════════════════
		// Decision de Rodolf, 2026-08-23 : « on fait comme en C++, et si possible
		// avec des espaces de noms et des portees. » Adoptee -- et completee,
		// parce que les espaces de noms SEULS ne suffisent pas.
		//
		// 🔴 LE CAS QUI LE PROUVE :
		//
		//     graphe A :  Rihen::Difficulte { Facile, Normal, Difficile }
		//     graphe B :  Rihen::Difficulte { Facile, Normal, Difficile, Expert }
		//
		// Meme espace de noms, meme nom, CONTENU DIFFERENT. Le nom les declare
		// identiques ; ils ne le sont pas. On brancherait un fil de l'un a
		// l'autre SANS RIEN DIRE -- exactement le defaut du lien par indice que
		// la version 2 du format vient de retirer, transpose sur les types.
		//
		// ⚠️ POURQUOI LE C++ N'A PAS CE PROBLEME ET NOUS SI : le C++ a un
		// EDITEUR DE LIENS, qui refuse deux definitions differentes du meme nom.
		// UN GRAPHE DE NOEUDS N'A AUCUNE ETAPE DE LIAISON -- les graphes sont
		// ecrits separement, sauves, et charges a l'execution par une
		// application qui n'a jamais vu l'autre. La garantie que le C++ obtient
		// gratuitement, il faut ici la CONSTRUIRE.
		//
		//   le NOM QUALIFIE (`espace::nom`) repond « LEQUEL »
		//                                    -> il regle les collisions
		//   l'EMPREINTE DE STRUCTURE repond  « EST-CE ENCORE LE MEME
		//                                      QU'A LA SAUVEGARDE »
		//
		// ⚠️ ET L'EMPREINTE NE CONCERNE QUE LES TYPES QUI PORTENT UNE CHARGE
		// UTILE. Un type feuille (`reel`, `booleen`) n'en a pas besoin : SON NOM
		// EST SA DEFINITION. Lui en donner une obligerait a inventer un contenu
		// a quelque chose qui n'en a pas -- et c'est la regle 3 : « rien » ne se
		// represente pas par « zero ». Une feuille n'a PAS d'empreinte ;
		// `TypeFingerprint` rend `false`, il ne rend pas 0.
		enum class NkTypeKind : uint8 {
			Leaf = 0,  ///< son nom EST sa definition : ni membres, ni empreinte
			Enum,	   ///< enumerateurs ORDONNES -- l'ordre est le sens
			Struct,	   ///< champs nommes et types
			Union,	   ///< memes membres qu'une structure, un seul vivant
		};

		const char *NkTypeKindName(NkTypeKind k);

		// Un membre d'un type composite.
		struct NkTypeMember {
				NkString name; ///< nom du membre ou de l'enumerateur
				// ⚠️ VIDE POUR UN ENUMERATEUR, et ce n'est pas un oubli : un
				// enumerateur ne porte pas de type, il EST une valeur. Le vide
				// se lit ici comme « sans objet », et le `kind` du type dit
				// lequel des deux sens s'applique -- sans lui, on ne saurait
				// pas distinguer « pas de type » de « type oublie ».
				NkString type;
		};

		using NkTypeId = uint32;
		static const NkTypeId NK_TYPE_INVALID = 0;

		// Identifiants STABLES. Ils survivent aux suppressions et aux
		// reordonnancements — c'est ce qui permet a une sauvegarde, a une courbe
		// d'animation ou a une annulation de designer un noeud sans se casser au
		// premier remaniement. Meme regle que les modificateurs et les raccourcis.
		using NkNodeId = uint32;
		using NkLinkId = uint32;
		static const NkNodeId NK_NODE_INVALID = 0;

		enum class NkSocketDir : uint8 { Input = 0, Output = 1 };

		// ── UNE VALEUR PORTEE PAR LE GRAPHE ──────────────────────────────────
		// Le coeur doit pouvoir la LIRE, l'ECRIRE, la COMPARER et la SERIALISER
		// sans jamais savoir ce qu'elle SIGNIFIE. C'est ce qui protege le
		// garde-fou n°1 par la porte de derriere : le type d'une valeur est un
		// `NkTypeId` du MEME registre que les prises, enregistre par le
		// consommateur. Le coeur ne fait que le transporter et le comparer — il
		// n'a nulle part ou ecrire `if (type == couleur)`.
		//
		// La charge utile est volontairement PAUVRE : N reels et un texte. Tout
		// ce qu'un editeur de noeuds manipule y entre — un scalaire, un vecteur
		// (3 reels), une couleur (4), une matrice (16), un chemin de fichier,
		// une cle d'enumeration. Un variant riche obligerait le coeur a
		// connaitre des cas, donc a en oublier un.
		//
		// ⚠️ « JAMAIS RENSEIGNE » ET « RENSEIGNE A VIDE » SONT DEUX ETATS
		// DIFFERENTS. C'est le piege paye par l'agent NkUIDesign dans la nuit du
		// 21 au 22/08 : un ecrivain qui reemet une forme memorisee ecrit du VIDE
		// quand la valeur n'a jamais ete renseignee, **et sans erreur**. Ici
		// l'etat « jamais renseigne » a UNE SEULE representation,
		// `type == NK_TYPE_INVALID`, et il ne s'ecrit pas du tout dans le
		// fichier : ce qui n'existe pas ne produit aucune ligne, donc ne peut
		// pas revenir en valeur par defaut silencieuse.
		struct NkGraphValue {
				NkTypeId type = NK_TYPE_INVALID;
				NkVector<float32> numbers;
				NkString text;

				bool IsSet() const {
					return type != NK_TYPE_INVALID;
				}
				void Clear() {
					type = NK_TYPE_INVALID;
					numbers.Clear();
					text = NkString("");
				}
				// Comparaison EXACTE, bit a bit sur les reels. Ce n'est pas de la
				// negligence numerique : cette egalite sert a l'aller-retour de
				// fichier et a l'annulation, ou une tolerance ferait passer pour
				// identiques deux etats que l'utilisateur distingue.
				bool Equals(const NkGraphValue &o) const;
		};

		// Fabriques courtes. Elles existent pour que l'appelant n'ait jamais a
		// remplir `numbers` a la main : un tableau rempli de travers produit une
		// valeur plausible, et c'est le genre d'erreur qui se voit au rendu.
		inline NkGraphValue NkValueReal(NkTypeId t, float32 v);
		inline NkGraphValue NkValueVec(NkTypeId t, const float32 *v, uint32 n);
		inline NkGraphValue NkValueText(NkTypeId t, const char *s);

		// Une PROPRIETE de noeud : ce qui n'est pas une entree. L'operation d'un
		// noeud Math, les arrets d'un ColorRamp, le chemin d'une image, l'espace
		// colorimetrique. Le nom est une CLE stable, jamais un libelle.
		struct NkGraphProp {
				NkString name;
				NkGraphValue value;
		};

		// ═════════════════════════════════════════════════════════════════════
		// LA FAMILLE D'UNE PRISE — UN AXE SEPARE DU TYPE
		// ═════════════════════════════════════════════════════════════════════
		// § 20.2 de la specification du chantier design. Trois chantiers y sont
		// arrives par des chemins qui ne se connaissaient pas : les types
		// composes, le mode etats, et la planche d'execution.
		//
		//   UN TYPE dit CE QUI PASSE.   UNE FAMILLE dit COMMENT CA SE BRANCHE.
		//
		// 🔴 LE PIEGE, ET IL RESSEMBLE A UNE BONNE IDEE. Le registre de types est
		// PLAT : on peut y enregistrer un type « exec » et brancher exec-sur-exec
		// des aujourd'hui, sans toucher au coeur. CA MARCHERAIT -- `Accepts()`
		// serait content -- et c'est exactement ce qui le rend dangereux :
		//   l'arite d'une entree resterait « une seule source » ;
		//   l'arite d'une sortie resterait « autant qu'on veut » ;
		//   le refus d'un croisement s'appellerait « type » et non « famille » ;
		// et RIEN ne le signalerait. Une solution qui fonctionne assez pour qu'on
		// l'adopte, et pas assez pour qu'elle serve.
		//
		// 📌 `Engine/Noge/.../NkBlueprint.h` porte deja
		// `NkPinPrimitiveType { Exec, Float, Int, ... }` : le besoin y etait vu,
		// mais `Exec` y est MELANGE aux types -- c'est le piege ci-dessus, ecrit.
		// Il doit etre realigne sur cet axe, pas recopie.
		//
		// ⚠️ `Data = 0` EXPRES : toute prise existante garde son sens sans etre
		// touchee, et un fichier ancien se relit inchange.
		enum class NkSocketFamily : uint8 {
			Data = 0, ///< une valeur circule
			Exec = 1, ///< un ORDRE de passage circule : rien n'est transporte
		};

		const char *NkSocketFamilyName(NkSocketFamily f);

		struct NkSocket {
				NkString name;					  ///< CLE stable, jamais un libelle
				NkTypeId type = NK_TYPE_INVALID;
				NkSocketDir dir = NkSocketDir::Input;
				// ⚠️ LA FAMILLE N'EST PAS UN TYPE. Elle ne dit pas ce qui passe,
				// elle dit comment ca se branche -- et elle commande TROIS
				// comportements : compatibilite, arite d'entree, arite de sortie.
				//
				// 🔴 ELLE EN COMMANDAIT QUATRE PENDANT UNE JOURNEE. L'acyclicite
				// etait le quatrieme, le 23/08 au matin ; Rodolf l'a retire le
				// soir meme : L'ACYCLICITE EST UNIVERSELLE, elle ne consulte plus
				// la famille. Un cycle vit A L'INTERIEUR d'un noeud -- boucle et
				// machine a etats sont des NOEUDS. Voir NkSocketFamily, et le pave
				// devant `LinkFamily` dans le .inl pour les trois raisons.
				NkSocketFamily family = NkSocketFamily::Data;
				// VALEUR D'UNE ENTREE NON CONNECTEE — Base Color, Roughness. Le
				// compilateur en a besoin exactement quand un lien manque.
				//
				// ⚠️ POURQUOI ELLE N'EST PAS DANS `NkNode::props`, alors qu'un seul
				// sac aurait suffi : un defaut de prise range dans un sac de noeud
				// ne se retrouve plus que par CONVENTION DE NOMMAGE
				// (« defaut_base_color »), et une convention de nommage finit
				// toujours par etre violee — par un consommateur, par un import,
				// par un renommage. Ici le lien entre la prise et sa valeur est
				// STRUCTUREL : il ne peut pas se defaire. Blender fait exactement
				// cette separation, et pas par hasard.
				NkGraphValue defaultValue;
		};

		struct NkNode {
				NkNodeId id = NK_NODE_INVALID;
				NkString type;			  ///< CLE du type de noeud, ex. « mesh.extrude »
				NkString label;			  ///< libelle affichable, traduisible
				// Renseigne UNIQUEMENT sur un noeud d'instance (type
				// NK_NODE_INSTANCE) : nom du graphe instancie dans le document.
				// Vide partout ailleurs.
				NkString subgraph;
				float32 x = 0.f, y = 0.f; ///< position dans le canevas (couche 2)
				NkVector<NkSocket> sockets;
				// CE QUI N'EST PAS UNE ENTREE : operation d'un Math, arrets d'un
				// ColorRamp, chemin d'une image. Meme raison d'etre ici que `x, y` —
				// « si le modele ne la porte pas, elle finira dans un fichier a
				// cote, donc desynchronisee ». Le coeur ne lit jamais le CONTENU
				// d'une propriete ; il la transporte.
				NkVector<NkGraphProp> props;
				bool alive = true;

				int32 FindSocket(const char *name, NkSocketDir dir) const;
		};

		// ⚠️ UN LIEN DESIGNE SA PRISE PAR SON INDEX, ALORS QUE `NkSocket::name` EST
		// LA CLE STABLE. La question a ete posee par le chantier design le
		// 2026-08-23 — « faire gagner des prises a un noeud repointerait en
		// silence les liens suivants » — et elle a ete MESUREE, pas raisonnee.
		// Le cas est `graphe/index-de-prise-contre-nom` dans NkMatGraphCheck.
		//
		// LA REPONSE EST EN DEUX MOITIES QUI NE DISENT PAS LA MEME CHOSE.
		//
		// EN MEMOIRE, LA CRAINTE EST INFONDEE. `AddSocket` fait un `PushBack`, et
		// il n'existe AUCUNE operation qui retire ou insere une prise : un index
		// deja attribue ne peut pas bouger. Le cas le mesure — on gagne une prise,
		// le lien garde son index ET son nom.
		//
		// 🔴 DANS LE FICHIER, ELLE EST EXACTE. Le format ecrit les prises dans
		// l'ordre — « cet ordre EST leur index » — puis des lignes `lien` et `def`
		// qui ne portent QUE des nombres. Une prise glissee AVANT une autre
		// decale tout ce qui suit ; le cas le fait, et le lien pointe alors sur
		// une AUTRE prise. `Deserialize` rend `true`, `Validate` rend ZERO
		// diagnostic, et le fichier reste parfaitement bien forme. Il n'y a
		// aucun nom du cote du lien a confronter : rien ne PEUT s'en apercevoir.
		//
		// ⚠️ CE QUI NOUS PROTEGE AUJOURD'HUI N'EST PAS LE FORMAT, C'EST LE FAIT
		// QUE NOUS SOMMES LE SEUL A L'ECRIRE. Notre ecrivain emet toujours les
		// prises et les liens dans un etat coherent, donc l'aller-retour est sur.
		// Trois choses feraient tomber cette protection, et aucune n'est
		// farfelue : un producteur tiers, une edition a la main, ou une migration
		// qui regenererait les prises d'un noeud depuis un catalogue ou le type a
		// gagne une prise. Le jour ou l'une arrive, il faut un NOM dans le
		// `lien` — sinon le graphe s'evalue faux en silence.
		//
		// 📌 ET LA CONVENTION EXISTE DEJA, UN CRAN PLUS BAS DANS CE MEME FICHIER :
		// `SetSocketDefault` porte « la prise se designe par son NOM et son SENS,
		// jamais par son index : un index se decale au premier remaniement, un nom
		// non ». La regle est ECRITE, l'API la respecte, et LE FORMAT NE LA SUIT
		// PAS — les lignes `def` designent la prise par son index elles aussi.
		// Regle 5 dans sa forme la plus couteuse : la regle et sa violation
		// cohabitent dans le meme module.
		// ── LA VERSION DU FORMAT `.nkgraph` ──────────────────────────────────
		// 1 : les liens et les valeurs par defaut designent leur prise par son
		//     INDEX — l'ordre des lignes `sock`. Encore LU, jamais plus ecrit.
		// 2 : ils la designent par son NOM. Voir NkNodeGraphIO.inl pour la mesure
		//     qui a decide, et pour ce que la version 2 garantit.
		// 5 : les LIENS peuvent etre QUALIFIES -- `liensg` (graphe de condition)
		//     et `lienp` (reglages de l'arc), ecrits seulement s'ils existent.
		// 4 : les prises portent leur FAMILLE (donnee / execution), sur une
		//     ligne `sockf` ecrite SEULEMENT quand la famille n'est pas `Data`.
		// 3 : les types COMPOSITES portent leur definition -- genre, membres
		//     ordonnes, et une EMPREINTE DE STRUCTURE. Un fichier sans type
		//     composite est identique a un fichier de version 2 : la ligne `type`
		//     n'a pas bouge, seules s'ajoutent `typec` et `typem`.
		//
		// ⚠️ ELLE VIT ICI ET PAS DANS LE .inl : c'est la version du MODELE, pas
		// un detail de l'ecrivain. Un lecteur qui veut savoir ce qu'il sait lire
		// ne devrait pas avoir a ouvrir le fichier de serialisation.
		static const uint32 NK_NKGRAPH_VERSION = 5;

		struct NkLink {
				NkLinkId id = 0;
				NkNodeId fromNode = NK_NODE_INVALID;
				int32 fromSocket = -1;
				NkNodeId toNode = NK_NODE_INVALID;
				int32 toSocket = -1;
				bool alive = true;

				// ── LE LIEN QUALIFIE (§ 20.3 de la specification design) ─────
				// Une transition d'animation porte `(paramName, NkCondKind,
				// threshold, fadeDur)` et veut en plus un mini-graphe de
				// condition. Un lien nu ne peut rien de tout ca.
				//
				// ⚠️ DEUX BESOINS DISTINCTS, ET LES CONFONDRE COUTERAIT CHER :
				//   la CONDITION est un CALCUL   -> une reference de sous-graphe
				//   les REGLAGES sont des VALEURS -> un stockage de valeur
				//
				// 🔴 ET SURTOUT : PAS D'UNION TYPEE ICI. Elle grossirait a chaque
				// consommateur -- l'animation aujourd'hui, le sequenceur demain --
				// et chaque ajout casserait le format de fichier. Une INDIRECTION
				// ne grossit pas. C'est le meme raisonnement que pour le nom de
				// prise dans le lien : on stocke une cle, pas une forme.
				//
				// 📌 LE MECANISME DE VALEUR EXISTE DEJA : `NkGraphProp`, celui des
				// noeuds et des defauts de prise. On le REUTILISE tel quel. La
				// specification supposait qu'il fallait l'inventer ; il etait la.
				// Le meme sac sert donc le cas choisi d'une enumeration, les
				// reglages d'une transition, et tout ce qui viendra.

				// Nom du graphe de CONDITION dans le document, vide s'il n'y en a
				// pas. Meme champ, meme role et meme nom que `NkNode::subgraph` --
				// deux noms differents pour la meme chose obligeraient a savoir
				// lequel s'applique ou.
				NkString subgraph;
				// Les REGLAGES de l'arc. Le coeur ne lit jamais leur CONTENU ; il
				// les transporte, exactement comme ceux d'un noeud.
				NkVector<NkGraphProp> props;
		};

		// Raison d'un refus de connexion. On REND une raison plutot qu'un simple
		// booleen : l'interface doit pouvoir DIRE pourquoi elle refuse au lieu de
		// laisser l'utilisateur deviner — meme regle que partout ailleurs dans le
		// produit.
		enum class NkLinkError : uint8 {
			Ok = 0,
			UnknownNode,
			UnknownSocket,
			SameNode,		   ///< un noeud ne se connecte pas a lui-meme
			DirectionMismatch, ///< sortie -> entree, jamais autre chose
			TypeMismatch,
			// ⚠️ TOUTES FAMILLES CONFONDUES. Un rebouclage d'EXECUTION est refuse
			// exactement comme un rebouclage de donnee : ce qui boucle, c'est un
			// NOEUD de boucle, pas un fil qui revient.
			WouldCycle, ///< la connexion fermerait une boucle, DONNEE OU EXECUTION
			// ⚠️ JAMAIS `TypeMismatch` POUR UN CROISEMENT DE FAMILLES. Les deux
			// prises peuvent porter le MEME type : appeler ca un desaccord de
			// type enverrait l'auteur chercher une conversion qui n'existe pas.
			FamilyMismatch,			 ///< exec branche sur donnee, ou l'inverse
			ExecOutputAlreadyBound,	 ///< une instruction n'a qu'UNE suite
		};

		const char *NkLinkErrorName(NkLinkError e);

		// ── CE QU'UNE PASSE DE VALIDATION PEUT TROUVER ───────────────────────
		// ⚠️ POURQUOI CETTE PASSE EXISTE, ecrit ici parce que c'est le point ou on
		// se trompe. Jusqu'au 2026-08-22 le module avait DEUX portes qui ne
		// disaient pas la meme chose :
		//   `Connect()`      refusait cycle, type et sens — l'invalide etait
		//                    IMPOSSIBLE A CONSTRUIRE par l'API ;
		//   `Deserialize()`  faisait `mLinks.PushBack` sans le moindre controle —
		//                    l'invalide entrait librement par le FICHIER.
		// C'est l'inverse exact de ce qu'il faut. Un editeur passe son temps dans
		// des etats intermediaires : un modele qui rend l'invalide impossible ne
		// peut pas etre edite, seulement charge. Et un fichier qui entre sans
		// controle produit un graphe d'apparence saine qui echoue plus tard,
		// ailleurs, sans rien designer.
		//
		// Le sens vise est donc : **l'invalide REPRESENTABLE et DETECTE**. La
		// validation rend un DIAGNOSTIC, jamais un refus de structure.
		enum class NkGraphIssue : uint8 {
			Ok = 0,
			LinkUnknownNode,	 ///< un lien designe un noeud absent ou mort
			LinkSocketOutOfRange, ///< index de socket hors des bornes du noeud
			LinkDirection,		 ///< la source n'est pas une sortie, ou la cible pas une entree
			LinkTypeMismatch,	 ///< les types ne s'accordent pas, conversions comprises
			LinkDuplicateTarget, ///< deux liens vivants sur la MEME entree
			Cycle,
			SocketUnknownType,	 ///< le type d'une prise n'est pas dans le registre
			DefaultTypeMismatch, ///< un defaut de prise n'a pas le type de sa prise
			PropUnknownType,	 ///< une propriete porte un type absent du registre
		};

		const char *NkGraphIssueName(NkGraphIssue i);

		// Un diagnostic DESIGNE. Un code d'erreur seul obligerait a chercher dans
		// un graphe qui peut compter des centaines de noeuds — meme regle que le
		// chemin d'instanciation de NkGraphDocument.
		struct NkGraphDiag {
				NkGraphIssue issue = NkGraphIssue::Ok;
				NkNodeId node = NK_NODE_INVALID;
				NkLinkId link = 0;
				NkString detail;
		};

		class NkNodeGraph {
			public:
				// ── TYPES ────────────────────────────────────────────────────────
				// Enregistres par les CONSOMMATEURS. Le coeur ne fait que comparer.
				NkTypeId RegisterType(const char *name);
				NkTypeId FindType(const char *name) const;
				const NkString *TypeName(NkTypeId t) const;

				// ── TYPES COMPOSITES : CEUX QUI PORTENT UNE CHARGE UTILE ─────
				// Enregistre un type dont la STRUCTURE fait partie de l'identite.
				// Idempotent SI la structure est identique ; en cas de conflit,
				// rend NK_TYPE_INVALID et renseigne `outErreur` en NOMMANT ce qui
				// differe -- jamais un ecrasement silencieux, qui ferait dependre
				// le sens du graphe de l'ordre d'enregistrement.
				NkTypeId RegisterCompositeType(const char *name, NkTypeKind kind, const NkTypeMember *members,
											   uint32 count, NkString *outErreur = nullptr);

				NkTypeKind TypeKind(NkTypeId t) const;
				// `false` pour une FEUILLE -- elle n'a pas d'empreinte, elle n'en
				// a pas une qui vaut zero. Regle 3.
				bool TypeFingerprint(NkTypeId t, uint64 *out) const;
				uint32 TypeMemberCount(NkTypeId t) const;
				const NkTypeMember *TypeMemberAt(NkTypeId t, uint32 i) const;

				// ── LES ESPACES DE NOMS ──────────────────────────────────────
				// Un nom qualifie s'ecrit `espace::sous_espace::nom`. Le coeur ne
				// fait que VERIFIER la forme : il ne resout rien, ne cherche pas
				// dans un espace englobant, et n'a pas de `using`. Une resolution
				// implicite ferait qu'un meme fichier changerait de sens selon
				// l'espace ouvert au moment du chargement.
				static bool NomQualifieValide(const char *n);
				// Rend la partie « espace » (vide si le nom n'est pas qualifie) et
				// la partie simple. Utilitaire de PRESENTATION -- le coeur compare
				// toujours le nom ENTIER.
				static void SepareNomQualifie(const char *n, NkString *outEspace, NkString *outSimple);

				// Conversion IMPLICITE autorisee : `from` peut alimenter `to`.
				// Declaree par le consommateur (ex. un flottant alimente un vecteur).
				// Le coeur ne l'invente jamais — deviner une conversion produirait des
				// graphes qui « marchent » et calculent autre chose.
				void AllowConversion(NkTypeId from, NkTypeId to);
				bool Accepts(NkTypeId socketType, NkTypeId valueType) const;

				// ENUMERATION des conversions. Elle existe pour une raison precise :
				// un sous-graphe cree pour accueillir un GROUPE tient son PROPRE
				// registre. Sans pouvoir relire les conversions du parent, il
				// refuserait a l interieur un lien que le parent acceptait -- et le
				// fil serait perdu en silence, Connect() rendant une erreur que
				// personne ne lit. Lecture seule : on ne rend jamais la table.
				uint32 ConversionCount() const {
					return (uint32)mConversions.Size();
				}
				bool ConversionAt(uint32 i, NkTypeId *outFrom, NkTypeId *outTo) const {
					if (i >= (uint32)mConversions.Size())
						return false;
					if (outFrom)
						*outFrom = (NkTypeId)(mConversions[i] >> 32);
					if (outTo)
						*outTo = (NkTypeId)(mConversions[i] & 0xFFFFFFFFull);
					return true;
				}

				// ── NOEUDS ───────────────────────────────────────────────────────
				NkNodeId AddNode(const char *type, const char *label = nullptr);
				// `family` par defaut a `Data` : tous les appelants existants
				// gardent leur sens sans etre touches.
				bool AddSocket(NkNodeId n, const char *name, NkTypeId type, NkSocketDir dir,
							   NkSocketFamily family = NkSocketFamily::Data);
				// Supprime le noeud ET toutes ses connexions. Laisser des liens
				// pendants serait pire qu'une suppression refusee : le graphe
				// paraitrait valide et s'evaluerait faux.
				bool RemoveNode(NkNodeId n);
				NkNode *Find(NkNodeId n);
				const NkNode *Find(NkNodeId n) const;
				uint32 NodeCount() const; ///< noeuds VIVANTS

				// ── VALEURS : DEFAUT DE PRISE ────────────────────────────────
				// La prise se designe par son NOM et son SENS, jamais par son
				// index : un index se decale au premier remaniement, un nom non.
				bool SetSocketDefault(NkNodeId n, const char *socket, NkSocketDir dir, const NkGraphValue &v);
				// nullptr si le noeud ou la prise n'existe pas ; une valeur dont
				// `IsSet()` est faux si la prise existe sans defaut. Les deux cas
				// sont distincts et l'appelant doit pouvoir les distinguer.
				const NkGraphValue *SocketDefault(NkNodeId n, const char *socket, NkSocketDir dir) const;

				// ── VALEURS : PROPRIETE DE NOEUD ─────────────────────────────
				// Poser deux fois la meme cle REMPLACE — comme une entree qui
				// n'accepte qu'une source. Deux proprietes homonymes rendraient
				// la lecture dependante de l'ordre d'insertion.
				bool SetProp(NkNodeId n, const char *name, const NkGraphValue &v);
				const NkGraphValue *FindProp(NkNodeId n, const char *name) const;
				bool RemoveProp(NkNodeId n, const char *name);
				uint32 PropCount(NkNodeId n) const;

				// Parcours BRUT, noeuds morts compris. Reserve aux traitements qui
				// doivent voir toute la table (validation, outillage). Le nom dit
				// « brut » pour qu'on ne l'utilise pas par megarde a la place de
				// NodeCount(), qui ne compte que les vivants.
				uint32 RawNodeCount() const {
					return (uint32)mNodes.Size();
				}
				const NkNode *RawNodeAt(uint32 i) const {
					return i < (uint32)mNodes.Size() ? &mNodes[i] : nullptr;
				}

				// ── CONNEXIONS ───────────────────────────────────────────────────
				// Une ENTREE n'accepte qu'UNE source : brancher une seconde REMPLACE
				// la premiere (comportement de Blender et d'Unreal). Refuser
				// obligerait a debrancher avant de rebrancher, pour aucun gain.
				NkLinkError Connect(NkNodeId from, const char *fromSocket, NkNodeId to, const char *toSocket,
									NkLinkId *outId = nullptr);
				bool Disconnect(NkLinkId link);
				uint32 LinkCount() const;
				const NkLink *LinkAt(uint32 i) const;
				const NkLink *IncomingOf(NkNodeId n, int32 socketIndex) const;

				// La famille d'un LIEN est celle de ses prises -- un lien qui
				// croiserait les familles n'existe pas, `Connect` le refuse.
				//
				// ⚠️ ELLE N'A PLUS AUCUN APPELANT DANS LE COEUR, et c'est le signe
				// que l'acyclicite est bien redevenue universelle : ni
				// `WouldCreateCycle` ni `TopoSort` ne consultent la famille. Ce
				// qui en a besoin est DEHORS -- le canevas, qui ne dessine pas un
				// fil d'execution comme un fil de donnee.
				NkSocketFamily LinkFamily(const NkLink &l) const;

				// ── QUALIFIER UN LIEN ────────────────────────────────────────
				// Meme grammaire que `SetProp` sur un noeud, et ce n'est pas une
				// coincidence : c'est le MEME mecanisme de valeur. Poser deux fois
				// la meme cle REMPLACE.
				NkLink *TrouveLien(NkLinkId l);
				const NkLink *TrouveLien(NkLinkId l) const;
				bool SetLinkProp(NkLinkId l, const char *name, const NkGraphValue &v);
				const NkGraphValue *FindLinkProp(NkLinkId l, const char *name) const;
				bool RemoveLinkProp(NkLinkId l, const char *name);
				uint32 LinkPropCount(NkLinkId l) const;
				// Le graphe de CONDITION porte par l'arc. Vide = pas de condition,
				// et c'est distinct d'une condition vide : `nullptr` si le lien
				// n'existe pas, chaine vide s'il n'a pas de condition.
				bool SetLinkSubgraph(NkLinkId l, const char *nomGraphe);
				const NkString *LinkSubgraph(NkLinkId l) const; ///< nullptr si l'entree est libre

				// ── ORDRE D'EVALUATION ───────────────────────────────────────────
				// Tri topologique : les producteurs avant les consommateurs. Renvoie
				// false s'il existe un CYCLE — et c'est un REFUS, pas un ordre
				// approximatif : evaluer un graphe cyclique boucle, ou produit un
				// resultat qui depend de l'ordre d'insertion.
				bool TopoSort(NkVector<NkNodeId> &out) const;
				bool HasCycle() const;

				// ── VALIDATION : UNE PASSE, JAMAIS UNE PRECONDITION ──────────
				// Rend le NOMBRE de problemes trouves (0 = sain) et les decrit
				// tous — pas seulement le premier : reparer un fichier en le
				// rechargeant dix fois pour decouvrir dix defauts est un supplice
				// qu'aucun format ne merite.
				uint32 Validate(NkVector<NkGraphDiag> &out) const;

				// ── SERIALISATION `.nkgraph` ─────────────────────────────────────
				// Format TEXTE, une directive par ligne (cf. NkNodeGraphIO.inl). Un
				// graphe se relit, se compare avec `git diff` et se repare a la main ;
				// un format binaire ferait gagner des octets sur des fichiers qui
				// pesent quelques kilo-octets.
				void Serialize(NkString &out) const;
				// `outErreur` est renseignee UNIQUEMENT en cas de refus, et elle
				// NOMME ce qui manque : la prise demandee et son noeud. Un `false`
				// muet enverrait l'appelant chercher dans son propre code.
				//
				// ⚠️ UN REFUS VIDE LE GRAPHE. Un graphe a moitie charge porterait
				// des noeuds justes et des liens faux, et l'appelant qui ignore le
				// `false` compilerait un materiau qui a l'air complet.
				bool Deserialize(const char *text, NkString *outErreur = nullptr);

				void Clear();

			private:
				bool WouldCreateCycle(NkNodeId from, NkNodeId to) const;

				NkVector<NkNode> mNodes;
				NkVector<NkLink> mLinks;
				NkVector<NkString> mTypeNames; ///< index 0 reserve = invalide
				// Definition des types COMPOSITES, indexee comme `mTypeNames`.
				// Une feuille y porte `kind = Leaf`, aucun membre, `aEmpreinte`
				// faux. C'est ce drapeau -- et pas une empreinte a zero -- qui
				// distingue « ce type n'a pas de structure » de « sa structure se
				// resume a rien ».
				struct TypeDef {
						NkTypeKind kind = NkTypeKind::Leaf;
						NkVector<NkTypeMember> members;
						uint64 empreinte = 0;
						bool aEmpreinte = false;
				};
				NkVector<TypeDef> mTypeDefs;
				NkVector<uint64> mConversions; ///< (from << 32) | to, DIRIGEE
				NkNodeId mNextNode = 1;
				NkLinkId mNextLink = 1;
		};

		// ── ANNULER / REFAIRE ────────────────────────────────────────────────
		// ECART ASSUME PAR RAPPORT A LA ROADMAP, qui disait « commandes
		// inversibles ». On garde des INSTANTANES serialises, pas des inverses.
		//
		// Pourquoi : l'inverse de « supprimer un noeud » doit restaurer le noeud,
		// TOUS ses liens, ET leurs identifiants d'origine. C'est precisement le
		// genre d'inverse qu'on ecrit presque juste, et dont l'erreur ne se voit
		// que trois manipulations plus tard. L'instantane est correct par
		// construction, puisqu'il reutilise une serialisation deja prouvee.
		//
		// Ce que ca coute, honnetement : memoire proportionnelle a
		// (taille du graphe x profondeur). Pour des graphes de quelques centaines
		// de noeuds c'est negligeable. Si un graphe reel devient assez gros pour
		// que ca compte, on passera aux inverses A CE MOMENT-LA — l'interface
		// publique ci-dessous ne changera pas.
		class NkGraphHistory {
			public:
				void Reset(const NkNodeGraph &g);  ///< etat initial, vide l'historique
				void Commit(const NkNodeGraph &g); ///< APRES chaque modification
				bool Undo(NkNodeGraph &g);
				bool Redo(NkNodeGraph &g);
				uint32 UndoDepth() const; ///< nombre d'annulations encore possibles
				uint32 RedoDepth() const;
				void SetLimit(uint32 n) {
					mLimit = n < 2u ? 2u : n;
				}

			private:
				NkVector<NkString> mStack;
				uint32 mCursor = 0;
				uint32 mLimit = 128;
		};

	} // namespace graph
} // namespace nkentseu

#include "NKGraph/NkNodeGraph.inl"
#include "NKGraph/NkNodeGraphIO.inl"
