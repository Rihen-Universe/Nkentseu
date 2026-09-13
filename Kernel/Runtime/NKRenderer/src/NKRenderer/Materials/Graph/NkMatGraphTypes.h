#pragma once
// -----------------------------------------------------------------------------
// @File    NkMatGraphTypes.h
// @Brief   COUCHE 3 pour les materiaux : types de sockets et bibliotheque de
//          prototypes de noeuds, poses SUR le coeur agnostique NKGraph.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// OU CE FICHIER SE SITUE, ET POURQUOI CA COMPTE
//   Decision d'architecture du 2026-07-09 (cf. Kernel/Runtime/NKGraph/ROADMAP.md) :
//   UN SEUL systeme de graphe pour tout l'ecosysteme, en trois couches.
//     couche 1 — le coeur, Kernel/Runtime/NKGraph : modele pur, AUCUN type metier ;
//     couche 2 — le canevas d'edition, Engine/NKEditorKit, partage ;
//     couche 3 — CE FICHIER : la semantique MATERIAU, et rien d'autre.
//   Le mot « couleur », le mot « shader » et le mot « Principled » n'ont le droit
//   d'exister qu'ICI. Les mettre dans le coeur tuerait l'architecture — c'est le
//   garde-fou n°1 du module, ecrit dans l'en-tete de NkNodeGraph.h.
//
// EN-TETE PUR, SANS CIBLE DE BUILD — meme raison que NkNodeGraph et
//   NkShortcutTable : le banc console peut l'exercer sans lier NKRenderer, donc
//   sans GPU, sans fenetre et sans les trente secondes de liaison qui vont avec.
//   La cible deviendra legitime le jour ou un consommateur reel (le systeme de
//   materiaux, l'editeur) la liera.
//
// CE QUE CE FICHIER NE FAIT PAS ENCORE, ET POURQUOI
//   Il ne porte AUCUNE valeur de parametre (Base Color = blanc, Roughness = 0,5,
//   les arrets d'un ColorRamp, l'operation d'un noeud Math). Ce n'est pas un
//   oubli : `NkNode` du coeur n'a aujourd'hui aucun endroit ou les ranger et le
//   format `.nkgraph` aucune directive pour les ecrire. La question est posee en
//   Q14 dans echanges/nkrenderer.questions.md et se decide au niveau du coeur,
//   pas ici. Tant qu'elle n'est pas tranchee, les prototypes ci-dessous
//   declarent la FORME d'un noeud — ses prises, leurs noms, leurs types — ce qui
//   est deja ce dont le compilateur a besoin pour parcourir un graphe.
//
// ZERO-STL : NkVector / NkString, aucun `std::`.
// -----------------------------------------------------------------------------

#include "NKGraph/NkNodeGraph.h"
// La GRAMMAIRE des groupes (graph.entree / graph.sortie / graph.instance).
// La couche 3 en a legitimement besoin : c'est elle qui derive un
// prototype de la frontiere d'un sous-graphe.
#include "NKGraph/NkGraphDocument.h"

namespace nkentseu {
	namespace renderer {
		namespace matgraph {

			using graph::NkGraphProp;
			using graph::NkGraphValue;
			using graph::NkNode;
			using graph::NkNodeGraph;
			using graph::NkNodeId;
			using graph::NkSocket;
			using graph::NkSocketDir;
			using graph::NkTypeId;
			using graph::NK_NODE_INVALID;
			using graph::NK_TYPE_INVALID;

			// ── LES TYPES DE PRISES D'UN GRAPHE DE MATERIAU ──────────────────
			// Ce sont des NOMS que le coeur enregistre et compare sans jamais
			// savoir ce qu'ils designent. Le decoupage suit celui de Blender, qui
			// est le modele demande — et il n'est pas arbitraire :
			//
			//   reel / vecteur / couleur  se convertissent entre eux dans un SENS
			//                             precis (un reel alimente un vecteur : il
			//                             se diffuse sur les trois composantes).
			//   shader                    ne se convertit avec RIEN. C'est ce qui
			//                             empeche de brancher une couleur la ou le
			//                             moteur attend une surface ombree — et
			//                             c'est la moitie des erreurs qu'un auteur
			//                             commet dans l'editeur de Blender.
			static const char *const NK_MT_REAL = "mat.reel";
			static const char *const NK_MT_VECTOR = "mat.vecteur";
			static const char *const NK_MT_COLOR = "mat.couleur";
			static const char *const NK_MT_SHADER = "mat.shader";
			// ⚠️ UN TYPE DE VALEUR QUI N'EST LE TYPE D'AUCUNE PRISE, et c'est
			// legitime. La condition posee sur les proprietes est que leur
			// `NkTypeId` vienne DU REGISTRE — pas qu'il soit porte par une prise.
			// Une rampe ne se branche pas : elle se REGLE. Elle a pourtant besoin
			// d'un type, sans quoi le coeur ne pourrait ni la comparer ni la
			// valider, et on aurait rouvert la porte de derriere du garde-fou n°1.
			static const char *const NK_MT_RAMP = "mat.rampe";
			// ⚠️ ET UN TYPE DISTINCT POUR LA COURBE, PAS `mat.rampe` REEMPLOYE.
			// Les deux charges sont des reels tries par position, mais elles n ont
			// ni le meme pas ni le meme sens : reemployer le type de la rampe
			// laisserait le coeur accepter une rampe la ou une courbe est
			// attendue -- 4 reels par element lus comme 2, donc une courbe a
			// deux fois trop de points, parfaitement plausible et jamais signalee.
			static const char *const NK_MT_CURVE = "mat.courbe";

			// Quatre reels par arret : c'est ecrit ici parce que le compilateur,
			// le banc et l'interface doivent decouper le tableau de la MEME
			// facon. Un decoupage suppose a trois endroits est un decoupage qui
			// divergera.
			static const uint32 NK_RAMP_REELS_PAR_ARRET = 4;
			// ⚠️ PLAFOND. Une charge variable sans borne est une invitation : un
			// fichier annoncant dix mille arrets deroulerait dix mille `mix` dans
			// le shader, qui ne compilerait plus — et l'erreur accuserait le
			// backend. On borne ici, et le refus dira le compte.
			static const uint32 NK_RAMP_ARRETS_MAX = 32;
			// ⚠️ UN arret suffit a une rampe : elle rend cette couleur partout, et
			// c est un resultat DEFINI, pas un piege. Le minimum n est donc pas le
			// meme pour tout le monde -- voir la courbe juste en dessous.
			static const uint32 NK_RAMP_ARRETS_MIN = 1;

			// ── LA COURBE REELLE : LA MEME FAMILLE DE CHARGE, UN AUTRE PAS ────
			// Deux reels par point -- position, valeur. La rampe en met quatre
			// (position, r, v, b). C est la SEULE difference de forme, et c est
			// exactement pourquoi la lecture ci-dessous est PARAMETREE par le pas
			// au lieu d etre recopiee : deux lectures d un meme genre de tableau
			// finissent par ne plus lire la meme chose.
			static const uint32 NK_CURVE_REELS_PAR_POINT = 2;
			static const uint32 NK_CURVE_POINTS_MAX = 32;
			// ⚠️ DEUX POINTS AU MINIMUM, et c est une VRAIE difference avec la
			// rampe, pas un alignement par gout. Une courbe a un seul point rend
			// sa valeur partout : l auteur a dessine une courbe et obtient une
			// constante. Ca compile, ca rend, et rien ne dit que le noeud n a
			// servi a rien. La rampe a un arret, elle, ANNONCE une couleur unie --
			// on voit tout de suite ce qu on a demande.
			static const uint32 NK_CURVE_POINTS_MIN = 2;

			struct NkMatTypes {
					NkTypeId real = graph::NK_TYPE_INVALID;
					NkTypeId vector = graph::NK_TYPE_INVALID;
					NkTypeId color = graph::NK_TYPE_INVALID;
					NkTypeId shader = graph::NK_TYPE_INVALID;
					NkTypeId ramp = graph::NK_TYPE_INVALID;
					NkTypeId curve = graph::NK_TYPE_INVALID;

					bool Valid() const {
						return real && vector && color && shader && ramp && curve;
					}
			};

			// Enregistre les quatre types et les conversions AUTORISEES.
			//
			// ⚠️ LES CONVERSIONS SONT DIRIGEES, ET C'EST TOUT L'INTERET. Le coeur
			// ne devine jamais : il applique ce qu'on declare. On declare donc
			// exactement ce que fait Blender, et rien de plus :
			//   reel -> vecteur   : la valeur se diffuse en (v, v, v)
			//   reel -> couleur   : niveau de gris (v, v, v)
			//   vecteur <-> couleur : les deux sens, ce sont trois flottants dans
			//                        les deux cas et Blender les echange librement
			// Ce qu'on NE declare PAS, volontairement :
			//   couleur -> reel   : il faudrait choisir entre la luminance, la
			//                       moyenne et le canal rouge. Trois reponses
			//                       plausibles, donc aucune par defaut : l'auteur
			//                       pose un noeud qui DIT laquelle il veut.
			//   shader -> quoi que ce soit, et l'inverse.
			inline NkMatTypes NkMatRegisterTypes(NkNodeGraph &g) {
				NkMatTypes t;
				t.real = g.RegisterType(NK_MT_REAL);
				t.vector = g.RegisterType(NK_MT_VECTOR);
				t.color = g.RegisterType(NK_MT_COLOR);
				t.shader = g.RegisterType(NK_MT_SHADER);
				t.ramp = g.RegisterType(NK_MT_RAMP);
				t.curve = g.RegisterType(NK_MT_CURVE);
				g.AllowConversion(t.real, t.vector);
				g.AllowConversion(t.real, t.color);
				g.AllowConversion(t.vector, t.color);
				g.AllowConversion(t.color, t.vector);
				return t;
			}

			// ── LES OPERATIONS RECONNUES ─────────────────────────────────────
			// UN SEUL endroit. Le compilateur les lit, le banc les enumere, et
			// l'interface les proposera. Trois listes finiraient par diverger, et
			// c'est le compilateur qui aurait raison sans que personne le sache.
			//
			// ⚠️ Ce sont des MOTS, jamais des numeros. Un numero d'enumeration se
			// decale des qu'on insere une valeur au milieu, et les graphes deja
			// enregistres se mettent alors a calculer autre chose EN SILENCE.
			struct NkMatOperation {
					const char *cle;	 ///< ce qui s'ecrit dans le fichier
					const char *libelle; ///< ce qui s'affiche, traduisible
			};

			namespace detail {
				static const NkMatOperation kOpsMath[] = {
					{"ajouter", "Add"},		  {"soustraire", "Subtract"}, {"multiplier", "Multiply"},
					{"diviser", "Divide"},	  {"minimum", "Minimum"},	  {"maximum", "Maximum"},
					{"puissance", "Power"},
				};
				static const uint32 kOpsMathCount = 7;

				// Types de degrade. Ceux de Blender qui ont un sens sans
				// coordonnee d'objet : les autres demanderaient des varyings que
				// le vertex engendre ne fournit pas, et une prise qui rendrait
				// zero serait le repli plausible qu'on refuse partout.
				static const NkMatOperation kTypesDegrade[] = {
					{"lineaire", "Linear"},
					{"quadratique", "Quadratic"},
					{"radial", "Radial"},
					{"spherique", "Spherical"},
				};
				static const uint32 kTypesDegradeCount = 4;

				// Types d'onde. `bandes` suit une coordonnee, `anneaux` la
				// distance a l'origine : c'est la meme difference qu'entre un
				// degrade lineaire et un degrade spherique.
				static const NkMatOperation kTypesOnde[] = {
					{"bandes", "Bands"},
					{"anneaux", "Rings"},
				};
				static const uint32 kTypesOndeCount = 2;

				// Conventions de carte de normales. Des MOTS, une seule table, et
				// une convention inconnue est REFUSEE : un repli sur OpenGL
				// donnerait un relief inverse sur la moitie des fichiers, et
				// l'image resterait plausible.
				static const NkMatOperation kConvNormale[] = {
					{"opengl", "OpenGL (+Y vers le haut)"},
					{"directx", "DirectX (-Y vers le haut)"},
				};
				static const uint32 kConvNormaleCount = 2;

				// Interpolations de rampe. Meme discipline que les operations :
				// des MOTS, une seule table, et un mode inconnu est REFUSE.
				static const NkMatOperation kInterps[] = {
					{"lineaire", "Linear"},
					{"constante", "Constant"},
				};
				static const uint32 kInterpsCount = 2;

				// Modes de bornage. `serre` ramene dans la plage de sortie, `libre`
				// laisse extrapoler. Le defaut de Blender est SERRE pour `Map Range`
				// et on le suit : une valeur qui sort de sa plage sans prevenir donne
				// des couleurs que personne n a choisies.
				static const NkMatOperation kBornages[] = {
					{"serre", "Clamp"},
					{"libre", "Unclamped"},
				};
				static const uint32 kBornagesCount = 2;

				// Operations vectorielles. ⚠️ TROIS D ENTRE ELLES RENDENT UN SCALAIRE
				// et non un vecteur -- produit scalaire, longueur, distance. C est
				// pourquoi `Vector Math` porte DEUX prises de sortie comme chez Blender,
				// et pourquoi le compilateur doit refuser de lire la mauvaise : brancher
				// `vector` apres un produit scalaire rendrait un vecteur invente.
				static const NkMatOperation kOpsVecteur[] = {
					{"ajouter", "Add"},           {"soustraire", "Subtract"},
					{"multiplier", "Multiply"},   {"diviser", "Divide"},
					{"produit_vectoriel", "Cross Product"}, {"normaliser", "Normalize"},
					{"absolu", "Absolute"},       {"minimum", "Minimum"},
					{"maximum", "Maximum"},
					// Les trois qui rendent un SCALAIRE :
					{"produit_scalaire", "Dot Product"}, {"longueur", "Length"},
					{"distance", "Distance"},
				};
				static const uint32 kOpsVecteurCount = 12;
				// Rang, dans la table ci-dessus, a partir duquel la sortie est SCALAIRE.
				// Range ici et non deduit d une liste de noms recopiee ailleurs : deux
				// listes de la meme chose divergent des qu on ajoute une operation.
				static const uint32 kOpsVecteurPremierScalaire = 9;

				static const NkMatOperation kOpsMix[] = {
					{"melanger", "Mix"},		{"multiplier", "Multiply"}, {"ajouter", "Add"},
					{"soustraire", "Subtract"}, {"eclaircir", "Lighten"},	{"assombrir", "Darken"},
				};
				static const uint32 kOpsMixCount = 6;
			} // namespace detail

			inline uint32 NkMatBornageCount() {
				return detail::kBornagesCount;
			}

			inline const NkMatOperation *NkMatBornageAt(uint32 i) {
				return i < detail::kBornagesCount ? &detail::kBornages[i] : nullptr;
			}

			inline int32 NkMatTrouveBornage(const char *cle) {
				if (!cle)
					return -1;
				for (uint32 i = 0; i < detail::kBornagesCount; ++i) {
					const char *a = detail::kBornages[i].cle;
					const char *b = cle;
					while (*a && *a == *b) {
						++a;
						++b;
					}
					if (!*a && !*b)
						return (int32)i;
				}
				return -1;
			}

			inline uint32 NkMatOpVecteurCount() {
				return detail::kOpsVecteurCount;
			}

			inline const NkMatOperation *NkMatOpVecteurAt(uint32 i) {
				return i < detail::kOpsVecteurCount ? &detail::kOpsVecteur[i] : nullptr;
			}

			inline int32 NkMatTrouveOpVecteur(const char *cle) {
				if (!cle)
					return -1;
				for (uint32 i = 0; i < detail::kOpsVecteurCount; ++i) {
					const char *a = detail::kOpsVecteur[i].cle;
					const char *b = cle;
					while (*a && *a == *b) {
						++a;
						++b;
					}
					if (!*a && !*b)
						return (int32)i;
				}
				return -1;
			}

			// ⚠️ LA SEULE SOURCE de « cette operation rend-elle un scalaire ? ».
			// Le compilateur ET le banc l interrogent ici. Une seconde liste de noms
			// recopiee ailleurs divergerait a la premiere operation ajoutee, et le
			// shader lirait alors un vecteur la ou il y a un reel.
			inline bool NkMatOpVecteurRendUnScalaire(uint32 rang) {
				return rang >= detail::kOpsVecteurPremierScalaire && rang < detail::kOpsVecteurCount;
			}

			inline uint32 NkMatTypeOndeCount() {
				return detail::kTypesOndeCount;
			}

			inline const NkMatOperation *NkMatTypeOndeAt(uint32 i) {
				return i < detail::kTypesOndeCount ? &detail::kTypesOnde[i] : nullptr;
			}

			inline int32 NkMatTrouveTypeOnde(const char *cle) {
				if (!cle)
					return -1;
				for (uint32 i = 0; i < detail::kTypesOndeCount; ++i) {
					const char *a = detail::kTypesOnde[i].cle;
					const char *b = cle;
					while (*a && *a == *b) {
						++a;
						++b;
					}
					if (!*a && !*b)
						return (int32)i;
				}
				return -1;
			}

			inline uint32 NkMatTypeDegradeCount() {
				return detail::kTypesDegradeCount;
			}

			inline const NkMatOperation *NkMatTypeDegradeAt(uint32 i) {
				return i < detail::kTypesDegradeCount ? &detail::kTypesDegrade[i] : nullptr;
			}

			inline int32 NkMatTrouveTypeDegrade(const char *cle) {
				if (!cle)
					return -1;
				for (uint32 i = 0; i < detail::kTypesDegradeCount; ++i) {
					const char *a = detail::kTypesDegrade[i].cle;
					const char *b = cle;
					while (*a && *a == *b) {
						++a;
						++b;
					}
					if (!*a && !*b)
						return (int32)i;
				}
				return -1;
			}

			inline uint32 NkMatConvNormaleCount() {
				return detail::kConvNormaleCount;
			}

			inline const NkMatOperation *NkMatConvNormaleAt(uint32 i) {
				return i < detail::kConvNormaleCount ? &detail::kConvNormale[i] : nullptr;
			}

			inline int32 NkMatTrouveConvNormale(const char *cle) {
				if (!cle)
					return -1;
				for (uint32 i = 0; i < detail::kConvNormaleCount; ++i) {
					const char *a = detail::kConvNormale[i].cle;
					const char *b = cle;
					while (*a && *a == *b) {
						++a;
						++b;
					}
					if (!*a && !*b)
						return (int32)i;
				}
				return -1;
			}

			inline uint32 NkMatInterpCount() {
				return detail::kInterpsCount;
			}

			inline const NkMatOperation *NkMatInterpAt(uint32 i) {
				return i < detail::kInterpsCount ? &detail::kInterps[i] : nullptr;
			}

			inline int32 NkMatTrouveInterp(const char *cle) {
				if (!cle)
					return -1;
				for (uint32 i = 0; i < detail::kInterpsCount; ++i) {
					const char *a = detail::kInterps[i].cle;
					const char *b = cle;
					while (*a && *a == *b) {
						++a;
						++b;
					}
					if (!*a && !*b)
						return (int32)i;
				}
				return -1;
			}

			// ── DES POINTS TRIES, LUS ET VALIDES AU MEME ENDROIT ─────────────
			// Le compilateur ET le banc passent par ici, pour la rampe COMME pour
			// la courbe : un decoupage duplique finirait par diverger, et c est le
			// compilateur qui aurait raison sans que personne le sache.
			//
			// ⚠️ POURQUOI UNE SEULE LECTURE POUR DEUX NOEUDS. La rampe range
			// quatre reels par arret, la courbe deux par point ; tout le RESTE est
			// identique -- multiple du pas, compte non nul, plafond, positions
			// strictement croissantes. Recopier la fonction en changeant le 4 en 2
			// aurait marche le premier jour ; le defaut arrive au TROISIEME
			// noeud a charge variable, quand une regle ajoutee d un cote ne l est
			// pas de l autre. Le pas, le plancher et le plafond sont donc des
			// PARAMETRES, et il n existe qu une lecture.
			enum class NkMatPointsErreur : uint8 {
				Ok = 0,
				Absente,	  ///< pas de propriete : cas LEGITIME, valeur par defaut
				MalFormee,	  ///< le compte de reels n est pas un multiple du pas
				Vide,		  ///< zero element
				PasAssez,	  ///< au moins un, mais sous le plancher du noeud
				TropDElements,	  ///< au-dela du plafond
				NonTriee,	  ///< positions dans le DESORDRE : il faut les reordonner
				// ⚠️ DEUX ELEMENTS A LA MEME POSITION EST UN AUTRE DEFAUT, et il se
				// repare autrement : le desordre se corrige en reordonnant, une
				// egalite en DEPLACANT un element. Un message commun obligerait
				// l auteur a comprendre lui-meme lequel des deux il a sous les
				// yeux. Separe apres qu un cas de banc l a exige.
				PositionsEgales,
			};

			inline NkMatPointsErreur NkMatLisPointsTries(const NkGraphValue *v, uint32 pas, uint32 mini,
														 uint32 maxi, uint32 *outCompte) {
				if (outCompte)
					*outCompte = 0;
				if (!v || !v->IsSet())
					return NkMatPointsErreur::Absente;
				if (pas == 0)
					return NkMatPointsErreur::MalFormee;
				const uint32 n = (uint32)v->numbers.Size();
				if (n % pas != 0)
					return NkMatPointsErreur::MalFormee;
				const uint32 compte = n / pas;
				if (compte == 0)
					return NkMatPointsErreur::Vide;
				if (compte < mini)
					return NkMatPointsErreur::PasAssez;
				if (compte > maxi)
					return NkMatPointsErreur::TropDElements;
				// ⚠️ POSITIONS CROISSANTES. Deux elements a la meme position font
				// diviser par zero dans l interpolation ; des positions dans le
				// desordre rendent une rampe (ou une courbe) qui a l air de
				// marcher et qui lit ses valeurs dans le mauvais ordre. On REFUSE
				// plutot que de trier en silence : trier changerait le fichier de
				// l auteur sans le lui dire.
				for (uint32 i = 1; i < compte; ++i) {
					const float32 avant = v->numbers[(i - 1) * pas];
					const float32 ici = v->numbers[i * pas];
					if (ici < avant)
						return NkMatPointsErreur::NonTriee;
					if (ici == avant)
						return NkMatPointsErreur::PositionsEgales;
				}
				if (outCompte)
					*outCompte = compte;
				return NkMatPointsErreur::Ok;
			}

			// Les deux appels nommes. Ils ne REFONT rien : ils fixent le pas, le
			// plancher et le plafond de leur noeud, une seule fois chacun.
			inline NkMatPointsErreur NkMatLisRampe(const NkGraphValue *v, uint32 *outArrets) {
				return NkMatLisPointsTries(v, NK_RAMP_REELS_PAR_ARRET, NK_RAMP_ARRETS_MIN,
										   NK_RAMP_ARRETS_MAX, outArrets);
			}

			inline NkMatPointsErreur NkMatLisCourbe(const NkGraphValue *v, uint32 *outPoints) {
				return NkMatLisPointsTries(v, NK_CURVE_REELS_PAR_POINT, NK_CURVE_POINTS_MIN,
										   NK_CURVE_POINTS_MAX, outPoints);
			}

			// ⚠️ DEUX VOCABULAIRES, UNE SEULE CLASSIFICATION. Ce qui ne doit pas
			// etre duplique, c est la REGLE ; les mots que l auteur lit, eux,
			// doivent parler de SON noeud. « deux arrets a la meme position » ne
			// veut rien dire devant une courbe, et « elements » ne veut rien dire
			// tout court.
			inline const char *NkMatRampeErreurNom(NkMatPointsErreur e) {
				switch (e) {
					case NkMatPointsErreur::Ok:
						return "ok";
					case NkMatPointsErreur::Absente:
						return "absente";
					case NkMatPointsErreur::MalFormee:
						return "mal-formee";
					case NkMatPointsErreur::Vide:
						return "vide";
					case NkMatPointsErreur::PasAssez:
						// Inatteignable pour la rampe : son plancher est 1, et zero
						// arret sort deja en `Vide`. On la nomme quand meme -- un
						// `switch` qui retombe sur « ? » le jour ou le plancher
						// change afficherait un point d interrogation a l auteur.
						return "pas-assez-d-arrets";
					case NkMatPointsErreur::TropDElements:
						return "trop-d-arrets";
					case NkMatPointsErreur::NonTriee:
						return "positions-dans-le-desordre";
					case NkMatPointsErreur::PositionsEgales:
						return "deux-arrets-a-la-meme-position";
				}
				return "?";
			}

			inline const char *NkMatCourbeErreurNom(NkMatPointsErreur e) {
				switch (e) {
					case NkMatPointsErreur::Ok:
						return "ok";
					case NkMatPointsErreur::Absente:
						return "absente";
					case NkMatPointsErreur::MalFormee:
						return "mal-formee";
					case NkMatPointsErreur::Vide:
						return "vide";
					case NkMatPointsErreur::PasAssez:
						return "un-seul-point-donc-une-constante";
					case NkMatPointsErreur::TropDElements:
						return "trop-de-points";
					case NkMatPointsErreur::NonTriee:
						return "positions-dans-le-desordre";
					case NkMatPointsErreur::PositionsEgales:
						return "deux-points-a-la-meme-position";
				}
				return "?";
			}

			inline uint32 NkMatOperationCount(bool pourMelangeCouleur) {
				return pourMelangeCouleur ? detail::kOpsMixCount : detail::kOpsMathCount;
			}

			inline const NkMatOperation *NkMatOperationAt(bool pourMelangeCouleur, uint32 i) {
				const uint32 n = NkMatOperationCount(pourMelangeCouleur);
				if (i >= n)
					return nullptr;
				return pourMelangeCouleur ? &detail::kOpsMix[i] : &detail::kOpsMath[i];
			}

			// Rend -1 si la cle est inconnue. ⚠️ L'appelant doit REFUSER dans ce
			// cas, jamais retomber sur la premiere operation : une operation
			// inconnue traitee comme « ajouter » produit un materiau qui compile,
			// qui rend, et qui calcule autre chose que ce que le fichier disait.
			inline int32 NkMatTrouveOperation(bool pourMelangeCouleur, const char *cle) {
				if (!cle)
					return -1;
				const uint32 n = NkMatOperationCount(pourMelangeCouleur);
				for (uint32 i = 0; i < n; ++i) {
					const char *a = NkMatOperationAt(pourMelangeCouleur, i)->cle;
					const char *b = cle;
					while (*a && *a == *b) {
						++a;
						++b;
					}
					if (!*a && !*b)
						return (int32)i;
				}
				return -1;
			}

			// ── PROTOTYPES DE NOEUDS ─────────────────────────────────────────
			// Un prototype decrit la FORME d'un noeud : sa cle de type, son
			// libelle, et ses prises NOMMEES. Les noms sont des CLES stables :
			// renommer le libelle affiche ne doit jamais casser un graphe
			// enregistre, et c'est pour ca que le lien porte un nom et pas un
			// numero de colonne.
			struct NkMatSocketDecl {
					const char *name;
					const char *type; ///< un des NK_MT_*
					NkSocketDir dir;
					// ⚠️ CERTAINES PRISES NE PEUVENT PAS RECEVOIR D'EXPRESSION.
					// Un parametre qui alimente l'ETAT DU PIPELINE — mode de
					// melange, mode d'ombre — ne peut pas varier par pixel a
					// moindre cout. Blender a la meme limite : certaines entrees
					// refusent un lien.
					//
					// L'interface doit alors NE PAS AFFICHER le point de
					// connexion, plutot qu'ouvrir un menu vide : un menu vide
					// laisse croire a une panne, une prise sans point dit « ce
					// parametre est une constante », ce qui est la verite.
					//
					// C'est declare ICI, en couche 3, et pas dans le coeur : le
					// coeur ne sait pas ce qu'est un etat de pipeline. La
					// consequence est que `Connect` ne le REFUSE pas — c'est la
					// requete de bibliotheque qui rend une liste vide, et une
					// passe de validation qui pourra le signaler. Conforme au
					// principe : la validation est une passe, jamais une
					// precondition de structure.
					bool constanteSeulement = false;
			};

			struct NkMatNodeProto {
					const char *key;   ///< ex. « mat.principled »
					const char *label; ///< libelle par defaut, traduisible
					const NkMatSocketDecl *sockets;
					uint32 socketCount;

					// ⚠️ SOURCE INTRINSEQUEMENT PAR PIXEL.
					//
					// Vrai quand le noeud produit une valeur qui CHANGE d'un pixel
					// a l'autre meme si aucune de ses entrees n'est connectee :
					// il lit une coordonnee interpolee, echantillonne une texture,
					// ou derive (`dFdx`). Faux quand le noeud n'est par pixel que
					// PAR CONTAGION, c'est-a-dire si l'une de ses entrees l'est.
					//
					// A quoi ca sert : une sortie « par materiau » promet une
					// valeur unique pour tout le materiau. Si son calcul descend
					// jusqu'a un de ces noeuds, cette promesse est fausse — on
					// rendrait la valeur d'UN pixel arbitraire en la faisant
					// passer pour celle du materiau. Elle serait parfaitement
					// PLAUSIBLE, et c'est precisement ce qui la rend dangereuse.
					//
					// La distinction n'est PAS « noeud de texture ou non » :
					// `Mapping` transforme une coordonnee sans en fabriquer une,
					// il est donc faux ici et devient par pixel uniquement si on
					// lui branche une source qui l'est. Le classer a vue aurait
					// donne l'inverse.
					bool parPixel = false;
			};

			// Cles des noeuds de cette premiere tranche. On n'en declare que ce
			// qu'on sait deja compiler ou refuser : une bibliotheque de 150
			// noeuds dont trois marchent ne rend service a personne.
			static const char *const NK_MN_PRINCIPLED = "mat.principled";
			static const char *const NK_MN_DIFFUSE = "mat.diffuse";
			static const char *const NK_MN_EMISSION = "mat.emission";
			static const char *const NK_MN_MIX_SHADER = "mat.melange_shader";
			static const char *const NK_MN_OUTPUT = "mat.sortie";
			// Les deux noeuds SOURCES de Blender : `Value` et `RGB`. Ils ne
			// prennent aucune entree et produisent une valeur constante, portee
			// par une PROPRIETE DE NOEUD. Ce sont eux qui donnent une reponse
			// non vide a « que puis-je brancher sur une prise de type couleur ? »
			// — et ce sont les premiers noeuds du depot a exercer `NkNode::props`
			// jusque dans le shader.
			static const char *const NK_MN_VALUE = "mat.valeur";
			static const char *const NK_MN_RGB = "mat.rgb";
			// Cles des proprietes qu'ils portent. Ce sont des CLES stables : le
			// compilateur les lit par ce nom, l'interface les ecrit par ce nom.
			static const char *const NK_MPROP_VALUE = "valeur";
			static const char *const NK_MPROP_COLOR = "couleur";

			// `Math` et `Mix Color` : les deux premiers noeuds dont le CALCUL lui
			// meme est choisi par une propriete. Jusqu'ici une propriete portait
			// une valeur ; celles-ci portent une DECISION.
			static const char *const NK_MN_MATH = "mat.math";
			static const char *const NK_MN_MIX_COLOR = "mat.melange_couleur";
			// La cle de l'operation, commune aux deux : le consommateur lit un
			// mot, jamais un numero. Un numero d'enumeration se decale des qu'on
			// insere une valeur au milieu, et les graphes enregistres se mettent
			// alors a calculer autre chose SANS que rien ne le dise.
			static const char *const NK_MPROP_OPERATION = "operation";

			// ColorRamp : la PREMIERE propriete a charge utile VARIABLE. Ses
			// arrets sont N groupes de quatre reels — position, r, v, b — dans un
			// seul `NkGraphValue`. Jusqu'ici toutes les charges etaient de taille
			// fixe (un reel, trois, quatre) ; celle-ci est ce qui eprouve
			// vraiment le sac de proprietes.
			static const char *const NK_MN_COLOR_RAMP = "mat.rampe_couleur";

			// `Image Texture` et ses deux compagnons de rang 1. `Texture
			// Coordinate` et `Mapping` existent pour que l'auteur puisse dire OU
			// echantillonner ; sans eux une texture ne saurait que lire l'UV brut.
			static const char *const NK_MN_IMAGE_TEXTURE = "mat.texture_image";
			static const char *const NK_MN_TEX_COORD = "mat.coord_texture";
			static const char *const NK_MN_MAPPING = "mat.mappage";
			// Le chemin de l'image : une charge utile VARIABLE elle aussi, mais
			// textuelle cette fois. C'est le second usage du sac de proprietes
			// apres les arrets de rampe, et il valide l'autre moitie de
			// `NkGraphValue` — le texte, la ou la rampe validait les reels.
			static const char *const NK_MPROP_IMAGE = "image";

			// ⚠️ LA CONVENTION D'UNE CARTE DE NORMALES EST UNE DONNEE DE
			// PROVENANCE DU FICHIER, PAS UN CALCUL DE SHADER.
			//
			// Convention interne : **OpenGL, +Y vers le haut** (« vert vers le
			// haut »). Une carte DirectX (-Y) se convertit A L'IMPORT, jamais
			// dans le shader. Trois raisons, tranchees le 2026-08-22 :
			//   - Blender emploie cette convention, et Rodolf construit sur les
			//     siennes : diverger produirait des reliefs INVERSES en important
			//     son propre travail, avec un symptome notoirement difficile a
			//     diagnostiquer (l'image reste plausible, elle est juste creuse
			//     la ou elle devrait etre bombee) ;
			//   - Vulkan et OpenGL sont les deux backends valides ;
			//   - convertir a l'import evite de payer par pixel ET rend l'etat de
			//     la texture visible dans la donnee.
			//
			// Le drapeau existe DES MAINTENANT, meme si rien ne le consomme
			// encore : l'ajouter apres coup obligerait a DEVINER la convention
			// des textures deja importees — et les deux hypotheses donnent une
			// image plausible, donc le doute serait indecidable.
			static const char *const NK_MPROP_NORMAL_CONV = "convention_normale";

			// ─────────────────────────────────────────────────────────────────
			// LES PARAMETRES EXPOSES — l'endroit ou ce chantier devient une API
			// publique du moteur.
			// ─────────────────────────────────────────────────────────────────
			// Une propriete `expose.<prise>` porte le NOM PUBLIC du parametre.
			// Presente = expose ; absente = constante.
			//
			// ⚠️ POURQUOI LE NOM EST LA DONNEE, ET PAS UN BOOLEEN A COTE : c'est
			// ce nom que le code du jeu emploiera — `SetFloat("usure", 0.7f)`. Un
			// booleen obligerait a inventer le nom ailleurs, donc a le maintenir
			// a deux endroits. Et quinze proprietes `expose.*` sont verbeuses
			// mais SE REGROUPENT le jour ou l'on voudra un objet dedie ; quinze
			// booleens ne se regroupent pas.
			//
			// ⚠️ ET « EXPOSE » N'EST PAS LE DEFAUT. Une constante se REPLIE dans
			// le code emis — `x * 0` disparait, les branches mortes s'effacent —
			// tandis qu'une variable exposee vit dans un bloc uniforme et
			// qu'AUCUNE optimisation n'est plus possible sur elle. Tout exposer
			// donnerait un materiau pilotable et lent. C'est donc un choix par
			// parametre, pose par l'AUTEUR sur SON noeud : `roughness` est
			// exposable dans un materiau et pas dans un autre.
			static const char *const NK_MPROP_EXPOSE_PREFIX = "expose.";

			// La charge utile de cette propriete : le nom public dans `text`, et
			// DEUX reels optionnels dans `numbers` — les bornes. Elles ne sont pas
			// de la coquetterie : elles disent a un editeur quel curseur afficher,
			// et elles empechent un script d'ecrire une valeur absurde.
			static const uint32 NK_EXPOSE_BORNES_REELS = 2;

			// ⚠️ LE DEFAUT D'UN PARAMETRE EXPOSE **EST** LE `defaultValue` DE SA
			// PRISE. Jamais une seconde valeur rangee a cote : deux sources pour
			// une meme chose divergent, et c'est alors l'editeur qui montre l'une
			// pendant que le moteur envoie l'autre.

			// Un nom public doit etre un IDENTIFIANT : le shader en fait un membre
			// de bloc uniforme, et le code du jeu une cle. Un nom a espaces ou
			// commencant par un chiffre produirait un shader invalide, et l'erreur
			// accuserait le generateur au lieu du nom.
			inline bool NkMatNomPublicValide(const char *n) {
				if (!n || !*n)
					return false;
				const char *p = n;
				if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_'))
					return false;
				for (; *p; ++p) {
					const bool ok = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
									(*p >= '0' && *p <= '9') || *p == '_';
					if (!ok)
						return false;
				}
				return true;
			}

			// ── DISPOSITION std140 ───────────────────────────────────────────
			// ⚠️ LA DISPOSITION PORTE DES DECALAGES, JAMAIS UN ORDRE. En `std140`
			// un `vec3` s'aligne sur 16 octets et en occupe 12 : trois reels a la
			// suite n'occupent PAS trois emplacements contigus. Un moteur qui
			// deduirait les positions de l'ordre de declaration ecrirait a cote
			// des le premier `vec3` — sans erreur, avec une valeur credible.
			inline uint32 NkMatStd140Align(NkTypeId t, const NkMatTypes &types) {
				if (t == types.real)
					return 4u;
				return 16u; // vecteur et couleur : alignes sur 16 en std140
			}

			inline uint32 NkMatStd140Size(NkTypeId t, const NkMatTypes &types) {
				if (t == types.real)
					return 4u;
				return 12u; // vec3 : occupe 12, mais le SUIVANT s'aligne sur 16
			}

			inline uint32 NkMatAlignUp(uint32 v, uint32 a) {
				return (v + a - 1u) / a * a;
			}

			// Les noeuds de relief. `Normal Map` decode une carte tangente ;
			// `Bump` derive une normale d'un champ de hauteur. Et `Separate XYZ`
			// vient avec eux parce que sans lui aucun scalaire VARIABLE n'est
			// disponible pour alimenter une hauteur — un banc ne pourrait alors
			// mesurer que l'absence d'effet.
			static const char *const NK_MN_NORMAL_MAP = "mat.carte_normales";
			static const char *const NK_MN_BUMP = "mat.relief";
			static const char *const NK_MN_SEPARATE_XYZ = "mat.separer_xyz";

			// ── RANG 2 : LE PROCEDURAL ───────────────────────────────────────
			// Des motifs calcules, sans aucune texture — donc sans consommer un
			// seul slot du plafond. C'est ce qui les rend precieux : un materiau
			// entierement procedural ne coute aucune ressource.
			static const char *const NK_MN_NOISE = "mat.bruit";
			static const char *const NK_MN_GRADIENT = "mat.degrade";
			static const char *const NK_MN_CHECKER = "mat.damier";
			static const char *const NK_MN_VORONOI = "mat.voronoi";
			static const char *const NK_MN_WAVE = "mat.onde";
			static const char *const NK_MN_BRICK = "mat.briques";
			// ── LES SORTIES NOMMEES : (a) PAR MATERIAU, COTE PROCESSEUR ──────
			//
			// Un graphe ne produit pas qu'une apparence. Il peut aussi repondre a
			// une question que le code de jeu se pose : « ce materiau est-il
			// transparent ? », « quelle est sa teinte dominante ? ». C'est une
			// SORTIE NOMMEE, et l'auteur choisit son ETAGE sur le noeud, comme il
			// choisit `expose` sur une prise.
			//
			// ⚠️ TROIS ETAGES, ET LE SECOND EST CELUI QU'ON OUBLIE (22/08/2026) :
			//
			//   (a)  par materiau, cote processeur — la valeur ne depend pas du
			//        pixel. Cout quasi nul : evaluee une fois, a la compilation
			//        ou au changement de parametre. **SEUL ETAGE IMPLEMENTE.**
			//
			//   (b1) par pixel, mais qui RESTE sur la carte — une cible de rendu
			//        de plus, consommee par une autre passe (masque de bloom,
			//        post-traitement). Cout MODERE : memoire et bande passante.
			//        Ce n'est PAS l'etage cher, et c'est l'erreur a ne pas
			//        refaire : tout moteur fait cela pour ses masques.
			//
			//   (b2) par pixel, RELU par le processeur — la meme, rapatriee en
			//        memoire centrale. 🔴 C'est CELUI-LA qui coute, et il ne coute
			//        pas en calcul : la relecture SYNCHRONISE, le processeur
			//        attend la carte, et tout le pipelinage s'effondre. Le
			//        symptome est « le jeu rame », jamais « la relecture est
			//        lente » — on met des semaines a l'imputer. S'il existe un
			//        jour, ce sera une relecture DIFFEREE de plusieurs images,
			//        jamais synchrone.
			//
			// Ce qui coute n'est donc pas le calcul par pixel, c'est le RETOUR
			// vers le processeur.
			static const char *const NK_MN_OUTPUT_VALUE = "mat.sortie_valeur";

			// Comparaison de cles, definie ICI parce que le rang 4 en a besoin
			// plus haut que le registre, qui l'employait seul jusque-la.
			inline bool NkMatCleEgale(const char *a, const char *b) {
				if (!a || !b)
					return false;
				while (*a && *a == *b) {
					++a;
					++b;
				}
				return *a == 0 && *b == 0;
			}

			// ── RANG 4 : CE QUE LE PIXEL SAIT DE LUI-MEME ────────────────────
			// Quatre noeuds qui lisent la geometrie, et deux qui nomment une
			// donnee du maillage. La difference entre les deux familles n'est pas
			// cosmetique -- voir la note sur les canaux plus bas.
			static const char *const NK_MN_FRESNEL = "mat.fresnel";
			static const char *const NK_MN_LAYER_WEIGHT = "mat.poids_couche";
			static const char *const NK_MN_GEOMETRY = "mat.geometrie";
			static const char *const NK_MN_UV_MAP = "mat.carte_uv";
			static const char *const NK_MN_ATTRIBUTE = "mat.attribut";
			// Declare pour etre REFUSE en se nommant -- il n'a rien a lire.
			static const char *const NK_MN_OBJECT_INFO = "mat.info_objet";

			// ── SELECTIONNER : LA CONDITION DE VALEUR ────────────────────────
			//
			// Demande de Rodolf, validee le 2026-08-23 apres explication.
			//
			// ⚠️ CE N'EST PAS UN `Si/Sinon` D'EXECUTION, ET LA DIFFERENCE EST LE
			// NOEUD ENTIER. Un materiau n'a jamais besoin de decider s'il
			// EXECUTE quelque chose ; il a besoin de decider QUELLE VALEUR il
			// prend. Deux entrees, une condition, une sortie. L'execution reste
			// hors du graphe -- c'est la premiere ligne de ce fichier.
			//
			// ═══════════════════════════════════════════════════════════════
			// 📌 CE QU'UN BRANCHEMENT COUTE, ET POURQUOI IL FAUT L'ECRIRE ICI
			// ═══════════════════════════════════════════════════════════════
			// Sans cette note quelqu'un croira le noeud cher, parce que « les
			// branchements coutent cher sur GPU » se transmet sans sa condition.
			//
			//   - Condition UNIFORME (un parametre expose, pilote par le
			//     gameplay) : BON MARCHE. Tous les pixels du tirage prennent la
			//     MEME branche, la carte n'en execute qu'une. C'est le cas
			//     d'usage vise, et c'est celui qui rend le noeud interessant.
			//   - Condition PAR PIXEL (issue d'une texture, d'un bruit, de la
			//     geometrie) : les pixels d'un meme groupe divergent, et le
			//     materiel execute alors LES DEUX branches en masquant. C'est
			//     LA le cout, et il ne vient pas du branchement en soi.
			//
			// ⚠️ ET CE QUE CE NOEUD N'ECONOMISE PAS AUJOURD'HUI — mesure, pas
			// suppose. Ce compilateur emet chaque noeud comme une locale, dans
			// l'ordre topologique : les DEUX cotes sont calcules AVANT la
			// selection, quel que soit le resultat. Poser un `Selectionner`
			// entre deux chaines de textures couteuses ne fait donc PAS
			// l'economie de la chaine inutilisee. Il choisit, il ne saute pas.
			// L'economie viendrait d'un emetteur qui enfoncerait le calcul DANS
			// la branche ; il n'existe pas, et le pretendre serait vendre une
			// optimisation qui n'a pas lieu.
			//
			// ⚠️ CE QUE `Selectionner` NE FERA JAMAIS : changer de TEXTURE ou de
			// modele d'eclairage. Cela n'est pas un parametre -- c'est un
			// changement STRUCTUREL, qui se traite par deux chemins dans un
			// shader ou par des variantes compilees a l'avance. C'est une
			// affaire de COMPILATEUR, pas de graphe. On le nomme pour que
			// personne ne vienne le demander a ce noeud.
			//
			// ── DEUX NOEUDS ET NON UN, ET C'EST UNE REGLE DE LA MAISON ───────
			// `Math` est reel, `Mix Color` est couleur : le depot a deja tranche
			// que la valeur et la couleur ne se melangent pas dans un seul
			// noeud. On suit. Un noeud unique a deux sorties aurait oblige
			// l'auteur qui n'en branche qu'une a lire ZERO sur l'autre -- « rien »
			// rendu comme « zero », la regle 3, sur un noeud tout neuf.
			static const char *const NK_MN_SELECT = "mat.selectionner";
			static const char *const NK_MN_SELECT_COLOR = "mat.selectionner_couleur";

			// ⚠️ LE SEUIL EST 0.5, ET CE N'EST PAS ARBITRAIRE. Il n'existe pas de
			// type booleen : la condition est un REEL, et le gameplay y ecrira 0
			// ou 1. Un seuil a `!= 0.0` ferait basculer sur `1e-30` -- le residu
			// d'un calcul qui a sous-deborde -- et l'auteur lirait une valeur
			// parfaitement credible. 0.5 est le seul seuil equidistant des deux
			// valeurs attendues : aucune imprecision de flottant ne le franchit.
			static const float32 NK_SELECT_SEUIL = 0.5f;

			static const char *const NK_MPROP_CANAL_UV = "canal";
			static const char *const NK_MPROP_ATTRIBUT = "nom";

			// ═════════════════════════════════════════════════════════════════
			// ⚠️ « RIEN » CONTRE « ZERO », A L'ETAGE DE LA GEOMETRIE
			//
			// `UV Map` et `Attribute` nomment une donnee qui peut ne pas exister.
			// La pente naturelle est de rendre (0,0) quand le nom ne correspond a
			// rien -- et c'est EXACTEMENT « rien » rendu comme « zero ». Un
			// auteur brancherait le noeud, obtiendrait une image noire, et
			// chercherait une heure du cote de son materiau.
			//
			// DECISION, prise AVANT d'implanter et apres avoir mesure ce qui
			// existe REELLEMENT en dessous :
			//
			//   Le shader engendre ne dispose que de QUATRE varyings :
			//   `vWorldPos`, `vNormal`, `vUV`, `vColor`. C'est tout. Le nom d'un
			//   canal est donc verifiable A LA COMPILATION, contre une liste
			//   CLOSE derivee de ces varyings -- et un nom absent est REFUSE EN
			//   SE NOMMANT, jamais rabattu sur zero.
			//
			// ⚠️ ET LE PIEGE QUE LA MESURE A ATTRAPE : `NkVertex3D` porte bien
			// `uv2`, et `NkVertexLayout::Default3D()` declare `NK_TEXCOORD1`. La
			// tentation etait donc d'ouvrir un canal « uv1 ». MAIS AUCUN VARYING
			// NE LE TRANSPORTE jusqu'au fragment : le declarer aurait ete une
			// promesse que le compilateur ne peut pas tenir. La liste se derive
			// de CE QUE LE SHADER LIT, jamais de ce que le sommet contient --
			// c'est la meme faute que « le nom est la, l'usage non », prise du
			// bon cote pour une fois.
			//
			// Le jour ou un second canal traverse, il s'ajoute ICI et nulle part
			// ailleurs.
			struct NkMatCanal {
					const char *cle;
					const char *varying; ///< l'expression NkSL qui le lit
					const char *libelle;
			};

			namespace detail {
				static const NkMatCanal kCanauxUV[] = {
					{"uv", "vUV", "UV du maillage"},
				};
				static const uint32 kCanauxUVCount = 1;

				// `Attribute` ne rend QU'UNE couleur, et c'est deliberé. Blender
				// lui donne aussi un `Fac`, moyenne des composantes -- mais ce
				// module REFUSE deja la conversion couleur -> reel, parce que
				// « luminance ? moyenne ? canal rouge ? » sont trois reponses
				// plausibles et qu'aucune ne s'impose. Offrir un `Fac` ici
				// reintroduirait par la porte de derriere la conversion qu'on
				// refuse par la porte principale.
				static const NkMatCanal kAttributs[] = {
					{"color", "vColor", "couleur de sommet"},
				};
				static const uint32 kAttributsCount = 1;

				// Un noeud DECLARE mais INDISPONIBLE, avec sa raison. Meme
				// dispositif que `NkMatEtageSortie::pourquoiPas` : un refus sec
				// laisserait croire a un oubli ; ici l'auteur apprend ce qui
				// manque et pourquoi.
				struct NkMatNoeudIndisponible {
						const char *cle;
						const char *pourquoiPas;
				};
				static const NkMatNoeudIndisponible kIndisponibles[] = {
					{"mat.info_objet",
					 "aucune donnee PAR OBJET n'atteint le fragment : le shader engendre ne dispose que du bloc "
					 "camera et des parametres exposes -- ni matrice de modele, ni index, ni couleur, ni graine "
					 "d'objet. Le noeud attend un bloc uniforme par objet ; il n'est pas refuse par principe"},
				};
				static const uint32 kIndisponiblesCount = 1;
			} // namespace detail

			inline uint32 NkMatCanalUVCount() {
				return detail::kCanauxUVCount;
			}
			inline const NkMatCanal *NkMatCanalUVAt(uint32 i) {
				return i < detail::kCanauxUVCount ? &detail::kCanauxUV[i] : nullptr;
			}
			inline const NkMatCanal *NkMatTrouveCanalUV(const char *cle) {
				for (uint32 i = 0; i < detail::kCanauxUVCount; ++i)
					if (NkMatCleEgale(detail::kCanauxUV[i].cle, cle))
						return &detail::kCanauxUV[i];
				return nullptr;
			}
			inline uint32 NkMatAttributCount() {
				return detail::kAttributsCount;
			}
			inline const NkMatCanal *NkMatAttributAt(uint32 i) {
				return i < detail::kAttributsCount ? &detail::kAttributs[i] : nullptr;
			}
			inline const NkMatCanal *NkMatTrouveAttribut(const char *cle) {
				for (uint32 i = 0; i < detail::kAttributsCount; ++i)
					if (NkMatCleEgale(detail::kAttributs[i].cle, cle))
						return &detail::kAttributs[i];
				return nullptr;
			}
			// Rend la RAISON de l'indisponibilite, ou nullptr si la cle n'est pas
			// dans la liste. Sert a distinguer « je ne connais pas ce noeud » de
			// « je le connais et voici ce qui manque ».
			inline const char *NkMatPourquoiIndisponible(const char *cle) {
				for (uint32 i = 0; i < detail::kIndisponiblesCount; ++i)
					if (NkMatCleEgale(detail::kIndisponibles[i].cle, cle))
						return detail::kIndisponibles[i].pourquoiPas;
				return nullptr;
			}
			// Le nom public de la sortie : meme grammaire qu'un parametre expose,
			// et pour la meme raison — c'est une cle que du code de jeu ecrira.
			static const char *const NK_MPROP_SORTIE_NOM = "nom";
			// L'etage, porte par un MOT. Meme discipline que les operations : un
			// numero d'enumeration se decale des qu'on insere une valeur au
			// milieu, et les graphes enregistres se mettent alors a promettre un
			// autre etage SANS que rien ne le dise.
			static const char *const NK_MPROP_SORTIE_ETAGE = "etage";

			namespace detail {
				struct NkMatEtageSortie {
						const char *cle;
						const char *libelle;
						bool implemente;
						const char *pourquoiPas; ///< non nul quand implemente == false
				};
				static const NkMatEtageSortie kEtages[] = {
					{"par_materiau", "par materiau, cote processeur", true, nullptr},
					// (b1), construit le 2026-08-22. Format signe par Rodolf :
					// R16G16B16A16_FLOAT, 8 octets/pixel -- RGB porte la valeur,
					// A porte la VALIDITE. Voir la note longue sur l'ecriture de
					// `fragAux` dans NkMatGraphCompile.h : tout materiau declare
					// et ecrit cette sortie, et celui qui n'a rien a y mettre y
					// ecrit (0,0,0,0).
					{"par_pixel_cible", "par pixel, vers une cible de rendu", true, nullptr},
					{"par_pixel_processeur", "par pixel, relu par le processeur", false,
					 "etage (b2) volontairement absent : une relecture SYNCHRONE ferait attendre le processeur "
					 "et s afficherait comme « le jeu rame », jamais comme « la relecture est lente ». S il "
					 "existe un jour ce sera une relecture DIFFEREE de plusieurs images, sur demande explicite"},
				};
				static const uint32 kEtagesCount = 3;
			} // namespace detail

			inline uint32 NkMatEtageSortieCount() {
				return detail::kEtagesCount;
			}

			inline const detail::NkMatEtageSortie *NkMatEtageSortieAt(uint32 i) {
				return i < detail::kEtagesCount ? &detail::kEtages[i] : nullptr;
			}

			inline const detail::NkMatEtageSortie *NkMatTrouveEtageSortie(const char *cle) {
				if (!cle)
					return nullptr;
				for (uint32 i = 0; i < detail::kEtagesCount; ++i) {
					const char *a = detail::kEtages[i].cle;
					const char *b = cle;
					while (*a && *a == *b) {
						++a;
						++b;
					}
					if (!*a && !*b)
						return &detail::kEtages[i];
				}
				return nullptr;
			}

			// ── RANG 3 : L OUTILLAGE ─────────────────────────────────────────
			//
			// Ces noeuds ne fabriquent aucune apparence : ils REMODELENT une valeur
			// qui existe deja. C est ce qui rend un graphe reellement utilisable --
			// sans eux, la sortie d un bruit ou d une texture arrive telle quelle et
			// l auteur n a aucun moyen de la recadrer sans passer par trois `Math`.
			static const char *const NK_MN_MAP_RANGE = "mat.plage";
			static const char *const NK_MN_CLAMP = "mat.borner";
			static const char *const NK_MN_COMBINE_XYZ = "mat.combiner_xyz";
			static const char *const NK_MN_VECTOR_MATH = "mat.math_vecteur";
			// ⚠️ LE SEUL DU RANG A PORTER UNE CHARGE VARIABLE, comme ColorRamp.
			// Les quatre autres se reglent avec un mot ou rien ; celui-ci porte N
			// points venus du fichier. C est pour ca qu il ferme le rang plutot
			// que de l ouvrir : il eprouve le sac de proprietes une seconde fois,
			// avec un PAS different -- et c est cette difference de pas qui a
			// force la lecture a devenir parametree au lieu d etre recopiee.
			static const char *const NK_MN_FLOAT_CURVE = "mat.courbe_reelle";
			// Les points : N groupes de deux reels -- position, valeur.
			static const char *const NK_MPROP_POINTS = "points";
			// Le mode de bornage de `Map Range` et de `Clamp`. Un MOT, comme partout :
			// un booleen aurait suffi pour deux etats, mais Blender en a quatre pour
			// `Clamp` (min/max, plage) et le jour ou on en ajoutera un, un booleen
			// devrait etre remplace -- pas etendu.
			static const char *const NK_MPROP_BORNAGE = "bornage";

			// Le type de degrade, meme discipline que les operations : un MOT.
			static const char *const NK_MPROP_TYPE = "type";
			static const char *const NK_MPROP_STOPS = "arrets";
			static const char *const NK_MPROP_INTERP = "interpolation";
			namespace detail {

				// Principled REDUIT a ce qui a un sens dans un rasteriseur, et
				// nomme comme chez Blender. Le Principled complet en a une
				// vingtaine ; les entrees absentes ici (subsurface, sheen,
				// clearcoat, transmission) ne sont pas oubliees, elles attendent
				// d'avoir une reponse cote rendu — une prise qui existe et que
				// personne n'honore est pire qu'une prise absente.
				static const NkMatSocketDecl kPrincipled[] = {
					{"base_color", NK_MT_COLOR, NkSocketDir::Input},
					{"metallic", NK_MT_REAL, NkSocketDir::Input},
					{"roughness", NK_MT_REAL, NkSocketDir::Input},
					{"emission", NK_MT_COLOR, NkSocketDir::Input},
					{"normal", NK_MT_VECTOR, NkSocketDir::Input},
					{"bsdf", NK_MT_SHADER, NkSocketDir::Output},
				};

				static const NkMatSocketDecl kDiffuse[] = {
					{"color", NK_MT_COLOR, NkSocketDir::Input},
					{"roughness", NK_MT_REAL, NkSocketDir::Input},
					{"normal", NK_MT_VECTOR, NkSocketDir::Input},
					{"bsdf", NK_MT_SHADER, NkSocketDir::Output},
				};

				static const NkMatSocketDecl kEmission[] = {
					{"color", NK_MT_COLOR, NkSocketDir::Input},
					{"strength", NK_MT_REAL, NkSocketDir::Input},
					{"emission", NK_MT_SHADER, NkSocketDir::Output},
				};

				// Mix Shader : DEUX entrees shader et un facteur. Sur un
				// rasteriseur on melange les resultats d'ombrage, pas des
				// closures a la Cycles — c'est ce que fait EEVEE, et Unreal, et
				// Unity. La FORME du noeud est la meme ; seule son execution
				// differe, et l'execution n'est pas l'affaire du graphe.
				static const NkMatSocketDecl kMixShader[] = {
					{"fac", NK_MT_REAL, NkSocketDir::Input},
					{"shader1", NK_MT_SHADER, NkSocketDir::Input},
					{"shader2", NK_MT_SHADER, NkSocketDir::Input},
					{"shader", NK_MT_SHADER, NkSocketDir::Output},
				};

				// Material Output : le puits. AUCUNE sortie — c'est ce qui le rend
				// reconnaissable sans un `if` sur son nom au moment du parcours.
				static const NkMatSocketDecl kOutput[] = {
					{"surface", NK_MT_SHADER, NkSocketDir::Input},
				};

				// `Value` : un reel constant. `RGB` : une couleur constante. Leur
				// valeur vit dans une propriete de noeud, pas dans une prise : il
				// n'y a rien a y brancher, c'est le point de depart d'une chaine.
				static const NkMatSocketDecl kValue[] = {
					{"value", NK_MT_REAL, NkSocketDir::Output, false},
				};
				static const NkMatSocketDecl kRGB[] = {
					{"color", NK_MT_COLOR, NkSocketDir::Output, false},
				};

				// Image Texture : DEUX sorties. C'est le premier noeud du depot
				// dans ce cas, et c'est ce qui a impose de nommer les locales
				// engendrees d'apres le NOM DE LA PRISE et non par un « val »
				// unique — un noeud a deux sorties n'a pas « une » valeur.
				static const NkMatSocketDecl kImageTexture[] = {
					{"vector", NK_MT_VECTOR, NkSocketDir::Input, false},
					{"color", NK_MT_COLOR, NkSocketDir::Output, false},
					{"alpha", NK_MT_REAL, NkSocketDir::Output, false},
				};

				// Texture Coordinate : les coordonnees disponibles sans calcul.
				// On n'expose que `uv` pour l'instant — `generated`, `object` et
				// `camera` demandent des varyings que le vertex engendre ne
				// fournit pas encore, et une prise qui rendrait zero serait
				// exactement le repli plausible qu'on refuse partout ailleurs.
				static const NkMatSocketDecl kTexCoord[] = {
					{"uv", NK_MT_VECTOR, NkSocketDir::Output, false},
				};

				// Mapping : deplace, tourne, met a l'echelle une coordonnee.
				// La rotation est volontairement absente de cette premiere
				// tranche : elle demande une convention d'axes qu'il vaut mieux
				// poser avec `Normal Map` et `Bump`, qui en dependent aussi.
				static const NkMatSocketDecl kMapping[] = {
					{"vector", NK_MT_VECTOR, NkSocketDir::Input, false},
					{"location", NK_MT_VECTOR, NkSocketDir::Input, false},
					{"scale", NK_MT_VECTOR, NkSocketDir::Input, false},
					{"vector_out", NK_MT_VECTOR, NkSocketDir::Output, false},
				};

				// Normal Map : decode une carte tangente. `strength` melange entre
				// la normale geometrique et la normale decodee, comme Blender.
				static const NkMatSocketDecl kNormalMap[] = {
					{"color", NK_MT_COLOR, NkSocketDir::Input, false},
					{"strength", NK_MT_REAL, NkSocketDir::Input, false},
					{"normal", NK_MT_VECTOR, NkSocketDir::Output, false},
				};

				// Bump : derive une normale d'un CHAMP DE HAUTEUR par derivees
				// d'ecran. `distance` est l'amplitude du relief en unites monde.
				static const NkMatSocketDecl kBump[] = {
					{"height", NK_MT_REAL, NkSocketDir::Input, false},
					{"strength", NK_MT_REAL, NkSocketDir::Input, false},
					{"distance", NK_MT_REAL, NkSocketDir::Input, false},
					{"normal", NK_MT_VECTOR, NkSocketDir::Output, false},
				};

				// Separate XYZ : trois sorties reelles. Sans lui, aucun scalaire
				// VARIABLE n'existe dans un graphe, et `Bump` ne pourrait etre
				// mesure que sur son absence d'effet.
				static const NkMatSocketDecl kSeparateXYZ[] = {
					{"vector", NK_MT_VECTOR, NkSocketDir::Input, false},
					{"x", NK_MT_REAL, NkSocketDir::Output, false},
					{"y", NK_MT_REAL, NkSocketDir::Output, false},
					{"z", NK_MT_REAL, NkSocketDir::Output, false},
				};

				// ── RANG 3 ────────────────────────────────────────────────────
				// Map Range : recadre une valeur d une plage vers une autre.
				static const NkMatSocketDecl kMapRange[] = {
					{"value", NK_MT_REAL, NkSocketDir::Input, false},
					{"from_min", NK_MT_REAL, NkSocketDir::Input, false},
					{"from_max", NK_MT_REAL, NkSocketDir::Input, false},
					{"to_min", NK_MT_REAL, NkSocketDir::Input, false},
					{"to_max", NK_MT_REAL, NkSocketDir::Input, false},
					{"result", NK_MT_REAL, NkSocketDir::Output, false},
				};

				// Clamp : borne une valeur. Trivial, et c est justement pour ca qu il
				// existe -- l ecrire a la main demande deux `Math` imbriques.
				static const NkMatSocketDecl kClamp[] = {
					{"value", NK_MT_REAL, NkSocketDir::Input, false},
					{"min", NK_MT_REAL, NkSocketDir::Input, false},
					{"max", NK_MT_REAL, NkSocketDir::Input, false},
					{"result", NK_MT_REAL, NkSocketDir::Output, false},
				};

				// Combine XYZ : le pendant de Separate XYZ. Sans lui, trois scalaires
				// calcules separement ne peuvent JAMAIS redevenir un vecteur, et la
				// moitie des montages de Blender sont impossibles.
				static const NkMatSocketDecl kCombineXYZ[] = {
					{"x", NK_MT_REAL, NkSocketDir::Input, false},
					{"y", NK_MT_REAL, NkSocketDir::Input, false},
					{"z", NK_MT_REAL, NkSocketDir::Input, false},
					{"vector", NK_MT_VECTOR, NkSocketDir::Output, false},
				};

				// Float Curve : remodele une valeur en la faisant passer par une
				// courbe DESSINEE. `fac` melange l entree et le resultat, comme
				// chez Blender -- a 0 le noeud est transparent, a 1 la courbe
				// s applique entierement.
				//
				// Deux prises nommees `value`, une par direction : c est le nom de
				// Blender des deux cotes, et les rendre differentes obligerait un
				// futur import a traduire un nom sur deux.
				static const NkMatSocketDecl kFloatCurve[] = {
					{"fac", NK_MT_REAL, NkSocketDir::Input, false},
					{"value", NK_MT_REAL, NkSocketDir::Input, false},
					{"value", NK_MT_REAL, NkSocketDir::Output, false},
				};

				// ⚠️ Vector Math a DEUX sorties, et ce n est pas un confort : trois de
				// ses douze operations rendent un SCALAIRE (produit scalaire, longueur,
				// distance). Une seule prise vectorielle obligerait a inventer un
				// vecteur pour ces trois-la, et l auteur lirait une valeur credible.
				static const NkMatSocketDecl kVectorMath[] = {
					{"a", NK_MT_VECTOR, NkSocketDir::Input, false},
					{"b", NK_MT_VECTOR, NkSocketDir::Input, false},
					{"vector", NK_MT_VECTOR, NkSocketDir::Output, false},
					{"value", NK_MT_REAL, NkSocketDir::Output, false},
				};

				// Noise : `detail` pilote le nombre d'octaves. DEUX sorties, comme
				// chez Blender — la valeur scalaire et sa version en gris.
				static const NkMatSocketDecl kNoise[] = {
					{"vector", NK_MT_VECTOR, NkSocketDir::Input, false},
					{"scale", NK_MT_REAL, NkSocketDir::Input, false},
					{"detail", NK_MT_REAL, NkSocketDir::Input, false},
					{"fac", NK_MT_REAL, NkSocketDir::Output, false},
					{"color", NK_MT_COLOR, NkSocketDir::Output, false},
				};

				static const NkMatSocketDecl kGradient[] = {
					{"vector", NK_MT_VECTOR, NkSocketDir::Input, false},
					{"fac", NK_MT_REAL, NkSocketDir::Output, false},
					{"color", NK_MT_COLOR, NkSocketDir::Output, false},
				};

				static const NkMatSocketDecl kChecker[] = {
					{"vector", NK_MT_VECTOR, NkSocketDir::Input, false},
					{"color1", NK_MT_COLOR, NkSocketDir::Input, false},
					{"color2", NK_MT_COLOR, NkSocketDir::Input, false},
					{"scale", NK_MT_REAL, NkSocketDir::Input, false},
					{"color", NK_MT_COLOR, NkSocketDir::Output, false},
					{"fac", NK_MT_REAL, NkSocketDir::Output, false},
				};

				// Voronoi : la distance a la cellule la plus proche. Reutilise
				// `NkHash22`, deja recopiee et deja gardee par le banc.
				static const NkMatSocketDecl kVoronoi[] = {
					{"vector", NK_MT_VECTOR, NkSocketDir::Input, false},
					{"scale", NK_MT_REAL, NkSocketDir::Input, false},
					{"distance", NK_MT_REAL, NkSocketDir::Output, false},
					{"color", NK_MT_COLOR, NkSocketDir::Output, false},
				};

				static const NkMatSocketDecl kWave[] = {
					{"vector", NK_MT_VECTOR, NkSocketDir::Input, false},
					{"scale", NK_MT_REAL, NkSocketDir::Input, false},
					{"fac", NK_MT_REAL, NkSocketDir::Output, false},
					{"color", NK_MT_COLOR, NkSocketDir::Output, false},
				};

				// Brick : un appareillage a joints decales une rangee sur deux.
				static const NkMatSocketDecl kBrick[] = {
					{"vector", NK_MT_VECTOR, NkSocketDir::Input, false},
					{"color1", NK_MT_COLOR, NkSocketDir::Input, false},
					{"color2", NK_MT_COLOR, NkSocketDir::Input, false},
					{"mortar", NK_MT_COLOR, NkSocketDir::Input, false},
					{"scale", NK_MT_REAL, NkSocketDir::Input, false},
					{"color", NK_MT_COLOR, NkSocketDir::Output, false},
					{"fac", NK_MT_REAL, NkSocketDir::Output, false},
				};

				// La sortie nommee, sur la forme du noeud AOV Output de Blender :
				// DEUX prises d'entree, un reel et une couleur. Une seule prise
				// typee reel aurait refuse une source couleur, car les conversions
				// sont DIRIGEES : reel -> couleur est permis, couleur -> reel ne
				// l'est pas (elle perdrait deux composantes en silence).
				//
				// Exactement une des deux doit etre alimentee. Aucune : la sortie
				// ne promet rien. Les deux : on ne saurait pas laquelle rendre, et
				// choisir la premiere donnerait une valeur PLAUSIBLE.
				//
				// Aucune prise de SORTIE : ce noeud est un puits. Il ne se branche
				// pas dans l'apparence, il repond a une question posee par le code
				// de jeu.
				static const NkMatSocketDecl kOutputValue[] = {
					{"value", NK_MT_REAL, NkSocketDir::Input, false},
					{"color", NK_MT_COLOR, NkSocketDir::Input, false},
				};

				static const NkMatSocketDecl kColorRamp[] = {
					{"fac", NK_MT_REAL, NkSocketDir::Input, false},
					{"color", NK_MT_COLOR, NkSocketDir::Output, false},
				};

				// `condition` D'ABORD, comme `fac` chez Mix Color : l'ordre des
				// prises EST leur index, et les liens s'y referent.
				static const NkMatSocketDecl kSelect[] = {
					{"condition", NK_MT_REAL, NkSocketDir::Input, false},
					{"si_vrai", NK_MT_REAL, NkSocketDir::Input, false},
					{"si_faux", NK_MT_REAL, NkSocketDir::Input, false},
					{"valeur", NK_MT_REAL, NkSocketDir::Output, false},
				};

				static const NkMatSocketDecl kSelectColor[] = {
					{"condition", NK_MT_REAL, NkSocketDir::Input, false},
					{"si_vrai", NK_MT_COLOR, NkSocketDir::Input, false},
					{"si_faux", NK_MT_COLOR, NkSocketDir::Input, false},
					{"couleur", NK_MT_COLOR, NkSocketDir::Output, false},
				};

				static const NkMatSocketDecl kMath[] = {
					{"a", NK_MT_REAL, NkSocketDir::Input, false},
					{"b", NK_MT_REAL, NkSocketDir::Input, false},
					{"value", NK_MT_REAL, NkSocketDir::Output, false},
				};

				// Mix Color : le `fac` d'abord, comme chez Blender, puis les deux
				// couleurs. L'ordre des prises EST leur index, et les liens s'y
				// referent : le changer casserait les graphes enregistres.
				static const NkMatSocketDecl kMixColor[] = {
					{"fac", NK_MT_REAL, NkSocketDir::Input, false},
					{"color1", NK_MT_COLOR, NkSocketDir::Input, false},
					{"color2", NK_MT_COLOR, NkSocketDir::Input, false},
					{"color", NK_MT_COLOR, NkSocketDir::Output, false},
				};

				// ── RANG 4 ───────────────────────────────────────────────
				// Fresnel : le reflet rasant. Une seule entree, l'indice de
				// refraction ; la vue et la normale viennent du pixel.
				static const NkMatSocketDecl kFresnel[] = {
					{"ior", NK_MT_REAL, NkSocketDir::Input, false},
					{"normal", NK_MT_VECTOR, NkSocketDir::Input, false},
					{"fac", NK_MT_REAL, NkSocketDir::Output, false},
				};

				// Layer Weight : DEUX sorties, et elles ne disent pas la meme
				// chose. `fresnel` suit l'angle rasant, `facing` mesure
				// simplement a quel point la surface fait face. Les confondre
				// donne un resultat credible et faux -- c'est pour ca qu'elles
				// sont separees chez Blender aussi.
				static const NkMatSocketDecl kLayerWeight[] = {
					{"blend", NK_MT_REAL, NkSocketDir::Input, false},
					{"normal", NK_MT_VECTOR, NkSocketDir::Input, false},
					{"fresnel", NK_MT_REAL, NkSocketDir::Output, false},
					{"facing", NK_MT_REAL, NkSocketDir::Output, false},
				};

				// Geometry : ce que le pixel sait de sa place. On ne declare QUE
				// ce que les varyings portent -- `backfacing` et `pointiness`
				// demanderaient respectivement gl_FrontFacing et une analyse de
				// courbure, et les declarer sans pouvoir les calculer serait une
				// promesse non tenue.
				static const NkMatSocketDecl kGeometry[] = {
					{"position", NK_MT_VECTOR, NkSocketDir::Output, false},
					{"normal", NK_MT_VECTOR, NkSocketDir::Output, false},
					{"incoming", NK_MT_VECTOR, NkSocketDir::Output, false},
				};

				// UV Map : le canal se NOMME (propriete), il ne se branche pas.
				static const NkMatSocketDecl kUVMap[] = {
					{"vector", NK_MT_VECTOR, NkSocketDir::Output, false},
				};

				// Attribute : une seule sortie, couleur. Pas de `Fac` -- voir la
				// note sur kAttributs : ce serait la conversion couleur -> reel
				// que ce module refuse par ailleurs.
				static const NkMatSocketDecl kAttribute[] = {
					{"color", NK_MT_COLOR, NkSocketDir::Output, false},
				};

				static const NkMatNodeProto kProtos[] = {
					// Le cinquieme champ est `parPixel` — voir la note longue sur
					// NkMatNodeProto. Il n'est PAS deductible du nom : `Mapping`
					// transforme une coordonnee sans en fabriquer une.
					{NK_MN_PRINCIPLED, "Principled BSDF", kPrincipled, 6, false},
					{NK_MN_DIFFUSE, "Diffuse BSDF", kDiffuse, 4, false},
					{NK_MN_EMISSION, "Emission", kEmission, 3, false},
					{NK_MN_MIX_SHADER, "Mix Shader", kMixShader, 4, false},
					{NK_MN_OUTPUT, "Material Output", kOutput, 1, false},
					{NK_MN_VALUE, "Value", kValue, 1, false},
					{NK_MN_RGB, "RGB", kRGB, 1, false},
					{NK_MN_MATH, "Math", kMath, 3, false},
					{NK_MN_MIX_COLOR, "Mix Color", kMixColor, 4, false},
					// ⚠️ `false` : une condition de valeur n'est PAS une source par
					// pixel. Elle ne fabrique aucune coordonnee -- elle propage par
					// contagion ce que ses entrees portent deja. Le declarer `true`
					// forcerait par pixel des materiaux entierement uniformes.
					{NK_MN_SELECT, "Selectionner", kSelect, 4, false},
					{NK_MN_SELECT_COLOR, "Selectionner (couleur)", kSelectColor, 4, false},
					{NK_MN_COLOR_RAMP, "ColorRamp", kColorRamp, 2, false},
					// echantillonne a une coordonnee interpolee
					{NK_MN_IMAGE_TEXTURE, "Image Texture", kImageTexture, 3, true},
					// REND une coordonnee interpolee — c'est la source meme
					{NK_MN_TEX_COORD, "Texture Coordinate", kTexCoord, 1, true},
					// TRANSFORME une coordonnee sans en fabriquer : par contagion seulement
					{NK_MN_MAPPING, "Mapping", kMapping, 4, false},
					// lit la base tangente, qui n'existe que par pixel
					{NK_MN_NORMAL_MAP, "Normal Map", kNormalMap, 3, true},
					// derive : dFdx/dFdy n'ont aucun sens hors d'un quad de pixels
					{NK_MN_BUMP, "Bump", kBump, 4, true},
					{NK_MN_SEPARATE_XYZ, "Separate XYZ", kSeparateXYZ, 4, false},
					// Les six proceduraux : coordonnee d'objet PAR DEFAUT, donc par
					// pixel meme sans aucune entree connectee.
					{NK_MN_NOISE, "Noise Texture", kNoise, 5, true},
					{NK_MN_GRADIENT, "Gradient Texture", kGradient, 3, true},
					{NK_MN_CHECKER, "Checker Texture", kChecker, 6, true},
					{NK_MN_VORONOI, "Voronoi Texture", kVoronoi, 4, true},
					{NK_MN_WAVE, "Wave Texture", kWave, 4, true},
					{NK_MN_BRICK, "Brick Texture", kBrick, 7, true},
					// Rang 3 : de l outillage pur. Aucun n est une source par pixel --
					// ils remodelent ce qu on leur donne.
					{NK_MN_MAP_RANGE, "Map Range", kMapRange, 6, false},
					{NK_MN_CLAMP, "Clamp", kClamp, 4, false},
					{NK_MN_COMBINE_XYZ, "Combine XYZ", kCombineXYZ, 4, false},
					{NK_MN_VECTOR_MATH, "Vector Math", kVectorMath, 4, false},
					// Charge variable, mais pas une source : il remodele: `false`.
					{NK_MN_FLOAT_CURVE, "Float Curve", kFloatCurve, 3, false},
					// Rang 4 : TOUS par pixel, et par NATURE. Ils lisent la
					// normale, la position ou une donnee interpolee : leur valeur
					// change d'un pixel a l'autre meme sans aucune entree
					// connectee. C'est precisement la definition de `parPixel`.
					{NK_MN_FRESNEL, "Fresnel", kFresnel, 3, true},
					{NK_MN_LAYER_WEIGHT, "Layer Weight", kLayerWeight, 4, true},
					{NK_MN_GEOMETRY, "Geometry", kGeometry, 3, true},
					{NK_MN_UV_MAP, "UV Map", kUVMap, 1, true},
					{NK_MN_ATTRIBUTE, "Attribute", kAttribute, 1, true},
					// Un puits, jamais une source : il ne fabrique aucune valeur.
					{NK_MN_OUTPUT_VALUE, "Named Output", kOutputValue, 2, false},
				};
				// ⚠️ COMPTE DERIVE DU TABLEAU, jamais recopie a cote. Cette
				// collection GRANDIT par conception : un compte tenu a la main
				// serait corrige un jour sans regarder, et le dernier prototype
				// ajoute deviendrait invisible en silence.
				static const uint32 kProtoCount = (uint32)(sizeof(kProtos) / sizeof(kProtos[0]));

			} // namespace detail

			// ── LE CATALOGUE A DEUX SOURCES, DERRIERE UNE SEULE PORTE ────────
			//
			// Arbitrage de Rodolf (2026-08-22) : « kProtos s'ouvre. Une seule
			// porte (NkMatFindProto), deux sources : la table statique et un
			// registre d'execution. »
			//
			// POURQUOI CA MARCHE SANS TOUCHER AU RESTE : `NkMatAddNode` et
			// `NkMatNoeudsPourPrise` lisent DEJA cette porte (et `NkMatProtoCount`
			// / `NkMatProtoAt`, que le menu parcourt). Ouvrir la porte ouvre donc
			// le menu SANS UNE LIGNE DE PLUS. C'est la propriete qui a ete mesuree
			// avant de demander l'arbitrage, et c'est elle qui rend la decision
			// bon marche.
			//
			// ⚠️ LA CONDITION POSEE PAR RODOLF, ET ELLE EST STRUCTURELLE :
			// « un type d'execution qui porte le nom d'un proto statique doit
			// etre REFUSE en se nommant, jamais l'eclipser en silence. Un
			// catalogue ou le dernier inscrit gagne est un catalogue dont on ne
			// peut plus predire le contenu. » Le refus est donc pose a
			// l'ENREGISTREMENT, pas a la lecture : une fois la collision
			// impossible, l'ordre dans lequel la porte consulte ses deux sources
			// n'a plus d'importance pour la correction. On consulte quand meme la
			// table statique d'abord, pour que cet invariant se lise dans le code.
			//
			// ⚠️ POURQUOI UN STOCKAGE A CAPACITE FIXE, ET PAS UN NkVector.
			// Un prototype est lu a travers `const NkMatNodeProto*`, et ce
			// prototype pointe lui-meme sur un tableau de `NkMatSocketDecl`, qui
			// pointent eux-memes sur des chaines. Range dans un NkVector, tout ce
			// petit monde CHANGE D'ADRESSE a la premiere reallocation -- et le
			// pointeur deja rendu a un appelant continue de pointer sur de la
			// memoire liberee. Il ne planterait pas : il lirait des noms de prises
			// plausibles. C'est precisement la forme de defaut la plus couteuse de
			// ce depot. Un tableau de capacite fixe rend les adresses STABLES par
			// construction, et le plafond se refuse EN SE NOMMANT.
			static const uint32 NK_MAT_GROUPES_MAX = 32;	   ///< groupes vivants simultanement
			static const uint32 NK_MAT_GROUPE_PRISES_MAX = 16; ///< prises par groupe
			static const uint32 NK_MAT_NOM_MAX = 48;		   ///< octets d'un nom, zero final compris

			namespace detail {

				inline void MatCopieNom(char *dst, const char *src) {
					uint32 i = 0;
					if (src)
						while (src[i] && i + 1u < NK_MAT_NOM_MAX) {
							dst[i] = src[i];
							++i;
						}
					dst[i] = 0;
				}

				inline uint32 MatLongueur(const char *s) {
					uint32 n = 0;
					if (s)
						while (s[n])
							++n;
					return n;
				}

				// Un prototype ne du regroupement. Il POSSEDE toutes ses chaines :
				// rien ici ne pointe vers l'exterieur, donc rien ne peut se
				// perimer sous lui quand le graphe d'origine est modifie ou detruit.
				struct MatProtoDyn {
						char key[NK_MAT_NOM_MAX];
						char label[NK_MAT_NOM_MAX];
						char noms[NK_MAT_GROUPE_PRISES_MAX][NK_MAT_NOM_MAX];
						char types[NK_MAT_GROUPE_PRISES_MAX][NK_MAT_NOM_MAX];
						NkMatSocketDecl decls[NK_MAT_GROUPE_PRISES_MAX];
						NkMatNodeProto proto;
				};

			} // namespace detail

			// Les refus se NOMMENT, comme partout ailleurs dans ce module.
			enum class NkMatRegistreErreur : uint8 {
				Ok = 0,
				NomVide,
				NomTropLong,	 ///< la cle, le libelle ou un nom de prise depasse NK_MAT_NOM_MAX
				DejaStatique,	 ///< la condition de Rodolf : ne JAMAIS eclipser un proto compile
				DejaEnregistre,	 ///< deux groupes du meme nom
				Plein,			 ///< NK_MAT_GROUPES_MAX atteint
				TropDePrises,	 ///< NK_MAT_GROUPE_PRISES_MAX atteint
				SansPrise,		 ///< un prototype sans aucune prise ne sert a rien
			};

			inline const char *NkMatRegistreErreurNom(NkMatRegistreErreur e) {
				switch (e) {
					case NkMatRegistreErreur::Ok:
						return "ok";
					case NkMatRegistreErreur::NomVide:
						return "nom-vide";
					case NkMatRegistreErreur::NomTropLong:
						return "nom-trop-long";
					case NkMatRegistreErreur::DejaStatique:
						return "eclipserait-un-proto-compile";
					case NkMatRegistreErreur::DejaEnregistre:
						return "deja-enregistre";
					case NkMatRegistreErreur::Plein:
						return "registre-plein";
					case NkMatRegistreErreur::TropDePrises:
						return "trop-de-prises";
					case NkMatRegistreErreur::SansPrise:
						return "aucune-prise";
				}
				return "?";
			}


			// ═════════════════════════════════════════════════════════════════
			// ✅ DEFAUT CORRIGE LE 2026-08-23 — il etait declare ici depuis la
			//    veille, et voici ce qu'il disait :
			//
			//   « LE REGISTRE DE GROUPES EST UNIQUE POUR LE PROCESSUS.
			//     DEUX DOCUMENTS OUVERTS PARTAGENT LEURS GROUPES. »
			//
			// Ce n'est PAS une simplification acceptable, c'est un DEFAUT. Dans
			// une application qui ouvre plusieurs documents -- NK3DModeler,
			// NKScena, Nogee -- il produit trois symptomes, et aucun ne se voit
			// dans un banc :
			//   1. un groupe defini dans le document A apparait dans le menu de B ;
			//   2. deux documents independants entrent en collision de noms, et
			//      le second se voit refuser un nom qu'il est seul a employer ;
			//   3. B casse quand A se ferme.
			// Ca se decouvrira chez un utilisateur, pas ici.
			//
			// CE QUI A ETE CORRIGE, ET CE N'EST PAS CE STOCKAGE : c'est la
			// SIGNATURE de la porte. `NkMatFindProto(cle)` ne transportait aucun
			// contexte ; tant qu'elle n'en transportait pas, aucun rangement ne
			// pouvait separer deux documents. Le stockage a capacite fixe, lui,
			// etait un choix DELIBERE et il reste inchange (voir la note plus
			// haut : un NkVector deplacerait les prototypes et le pointeur deja
			// rendu lirait de la memoire liberee, sans planter, en rendant des
			// noms de prises plausibles).
			//
			// Le registre est desormais un PARAMETRE SANS DEFAUT de toute la
			// couche 3, et le banc porte son temoin :
			// **deux documents ne voient pas les groupes l'un de l'autre**.
			//
			// 📌 Le changement a ete fait SEUL, apres (b1) et non pendant :
			// ouvrir la signature touche tous les appelants, et meler cette
			// mesure a celle de la seconde cible en aurait fait une mesure qui
			// porte sur deux choses a la fois.
			// ═════════════════════════════════════════════════════════════════
			//
			// La source d'execution. Voir plus haut pourquoi elle est a capacite
			// fixe et pourquoi la collision de noms est refusee ICI.
			class NkMatRegistreProtos {
				public:
					NkMatRegistreErreur Enregistre(const char *key, const char *label, const NkMatSocketDecl *sockets,
												   uint32 n, bool parPixel);

					const NkMatNodeProto *Trouve(const char *key) const {
						for (uint32 i = 0; i < mN; ++i)
							if (NkMatCleEgale(mEntrees[i].key, key))
								return &mEntrees[i].proto;
						return nullptr;
					}

					uint32 Count() const {
						return mN;
					}

					const NkMatNodeProto *At(uint32 i) const {
						return i < mN ? &mEntrees[i].proto : nullptr;
					}

					// Remet le registre a vide. Il n'y a plus rien a avouer ici :
					// depuis que le registre est un parametre, un registre local a
					// une portee suffit a isoler ce qu'on y met, et deux
					// registres distincts s'ignorent par construction.
					void Vide() {
						mN = 0;
					}

				private:
					detail::MatProtoDyn mEntrees[NK_MAT_GROUPES_MAX];
					uint32 mN = 0;
			};

			// ⚠️ IL N'Y A PLUS D'INSTANCE DE PROCESSUS, ET C'EST LE POINT.
			//
			// Jusqu'au 2026-08-23 un `NkMatRegistre()` statique servait tout le
			// processus. C'etait un DEFAUT declare : deux documents ouverts
			// partageaient leurs groupes. La correction n'etait pas dans le
			// rangement -- elle etait dans LA SIGNATURE, parce qu'une porte qui
			// ne transporte aucun contexte ne peut separer personne, quel que
			// soit son stockage.
			//
			// Le registre est donc devenu un PARAMETRE, et il n'a AUCUNE valeur
			// par defaut. Un defaut qui serait retombe sur un registre global
			// aurait laisse le defaut atteignable en silence : tout appelant qui
			// n'y aurait pas pense aurait continue de partager. Ici le
			// compilateur oblige chaque appelant a dire DE QUEL document il
			// parle -- c'est desagreable une fois, et definitif.
			//
			// Chaque document en possede un ; deux documents ne voient donc pas
			// les groupes l'un de l'autre, et le banc l'exige.

			inline uint32 NkMatProtoCount(const NkMatRegistreProtos &reg) {
				return detail::kProtoCount + reg.Count();
			}

			inline const NkMatNodeProto *NkMatProtoAt(const NkMatRegistreProtos &reg, uint32 i) {
				if (i < detail::kProtoCount)
					return &detail::kProtos[i];
				return reg.At(i - detail::kProtoCount);
			}

			inline const NkMatNodeProto *NkMatFindProto(const NkMatRegistreProtos &reg, const char *key) {
				if (!key)
					return nullptr;
				// SOURCE 1 : la table compilee. Consultee d'abord pour que
				// l'invariant « rien ne peut l'eclipser » se lise ici meme ;
				// la collision etant refusee a l'enregistrement, l'ordre
				// n'a de toute facon aucune consequence.
				for (uint32 i = 0; i < detail::kProtoCount; ++i) {
					const char *a = detail::kProtos[i].key;
					const char *b = key;
					while (*a && *a == *b) {
						++a;
						++b;
					}
					if (*a == 0 && *b == 0)
						return &detail::kProtos[i];
				}
				// SOURCE 2 : les prototypes nes du regroupement, a l'execution.
				return reg.Trouve(key);
			}

			// ⚠️ DEFINIE ICI, APRES LA PORTE, et pas dans la classe : c'est
			// `NkMatFindProto` qui sait ce que contient la table statique, et
			// c'est par elle que passe le refus d'eclipse. Enregistrer sans
			// frapper a la porte laisserait entrer exactement ce que Rodolf a
			// demande de refuser.
			inline NkMatRegistreErreur NkMatRegistreProtos::Enregistre(const char *key, const char *label,
																	   const NkMatSocketDecl *sockets, uint32 n,
																	   bool parPixel) {
				if (!key || !key[0])
					return NkMatRegistreErreur::NomVide;
				if (detail::MatLongueur(key) + 1u > NK_MAT_NOM_MAX || detail::MatLongueur(label) + 1u > NK_MAT_NOM_MAX)
					return NkMatRegistreErreur::NomTropLong;
				if (!sockets || n == 0)
					return NkMatRegistreErreur::SansPrise;
				if (n > NK_MAT_GROUPE_PRISES_MAX)
					return NkMatRegistreErreur::TropDePrises;
				// LA CONDITION DE RODOLF. On distingue les deux collisions : un
				// proto compile et un groupe deja enregistre ne se corrigent pas
				// de la meme facon, donc ils ne portent pas le meme nom.
				for (uint32 i = 0; i < detail::kProtoCount; ++i)
					if (NkMatCleEgale(detail::kProtos[i].key, key))
						return NkMatRegistreErreur::DejaStatique;
				if (Trouve(key))
					return NkMatRegistreErreur::DejaEnregistre;
				for (uint32 k = 0; k < n; ++k)
					if (detail::MatLongueur(sockets[k].name) + 1u > NK_MAT_NOM_MAX ||
						detail::MatLongueur(sockets[k].type) + 1u > NK_MAT_NOM_MAX)
						return NkMatRegistreErreur::NomTropLong;
				if (mN >= NK_MAT_GROUPES_MAX)
					return NkMatRegistreErreur::Plein;

				detail::MatProtoDyn &e = mEntrees[mN];
				detail::MatCopieNom(e.key, key);
				detail::MatCopieNom(e.label, label && label[0] ? label : key);
				for (uint32 k = 0; k < n; ++k) {
					detail::MatCopieNom(e.noms[k], sockets[k].name);
					detail::MatCopieNom(e.types[k], sockets[k].type);
					// Les declarations pointent sur les COPIES, jamais sur ce que
					// l'appelant nous a passe : son tableau peut disparaitre a la
					// ligne suivante.
					e.decls[k].name = e.noms[k];
					e.decls[k].type = e.types[k];
					e.decls[k].dir = sockets[k].dir;
					e.decls[k].constanteSeulement = sockets[k].constanteSeulement;
				}
				e.proto.key = e.key;
				e.proto.label = e.label;
				e.proto.sockets = e.decls;
				e.proto.socketCount = n;
				// ⚠️ `parPixel` NE SE DEDUIT PAS du contenu du groupe ici : le
				// registre ne voit que l'interface. C'est l'appelant qui le sait,
				// parce qu'il a le sous-graphe sous les yeux -- et un groupe qui
				// contient un Noise EST par pixel. Le lui faire deviner ici
				// rendrait « faux » sur tous les groupes, silencieusement.
				e.proto.parPixel = parPixel;
				++mN;
				return NkMatRegistreErreur::Ok;
			}

			// ── LE PONT : UN SOUS-GRAPHE DEVIENT UN PROTOTYPE ───────────────
			//
			// L'interface du groupe SE DEDUIT de ses noeuds frontiere -- elle ne
			// se declare pas (R9). Le noeud `graph.entree` porte des prises de
			// SORTIE (il alimente l'interieur) : ce sont les ENTREES du groupe vu
			// du dehors. Symetriquement pour `graph.sortie`.
			//
			// LES TYPES SONT PRIS PAR LEUR NOM, jamais par leur identifiant :
			// chaque graphe tient son propre registre, et le numero 3 peut
			// designer « couleur » ici et « vecteur » la. C'est la meme regle que
			// le controle d'interface de NkGraphDocument, et pour la meme raison.
			inline NkMatRegistreErreur NkMatEnregistreGroupe(NkMatRegistreProtos &reg, const graph::NkNodeGraph &sousGraphe,
													 const char *key,
															 const char *label = nullptr) {
				NkMatSocketDecl decls[NK_MAT_GROUPE_PRISES_MAX];
				uint32 n = 0;
				bool tropDePrises = false;

				// ⚠️ `parPixel` D'UN GROUPE : vrai des qu'UN SEUL de ses noeuds
				// internes est une source intrinseque. Un groupe qui contient un
				// Noise EST par pixel, quoi qu'on branche dessus.
				//
				// ET LE CAS QU'ON NE SAIT PAS TRANCHER : un groupe IMBRIQUE
				// apparait ici comme un `graph.instance`, dont on ne peut pas
				// resoudre le sous-graphe sans le document. On ne devine pas -- on
				// prend le cote SUR. Les deux erreurs ne coutent pas pareil :
				//   parPixel=true a tort  -> une sortie « par materiau » est
				//     REFUSEE alors qu'elle etait licite. Faux, mais BRUYANT.
				//   parPixel=false a tort -> une sortie « par materiau » ACCEPTE
				//     une valeur qui change a chaque pixel, et rend celle d'un
				//     pixel arbitraire en la faisant passer pour celle du
				//     materiau. Faux, PLAUSIBLE, et jamais signale.
				// Le refus bruyant est toujours preferable a la valeur plausible.
				bool parPixel = false;

				for (uint32 i = 0; i < sousGraphe.RawNodeCount(); ++i) {
					const graph::NkNode *nd = sousGraphe.RawNodeAt(i);
					if (!nd || !nd->alive)
						continue;
					const bool estEntree = nd->type == NkString(graph::NK_NODE_GROUP_IN);
					const bool estSortie = nd->type == NkString(graph::NK_NODE_GROUP_OUT);
					if (!estEntree && !estSortie) {
						if (nd->type == NkString(graph::NK_NODE_INSTANCE)) {
							parPixel = true; // indecidable ici : on prend le cote sur
							continue;
						}
						const NkMatNodeProto *pr = NkMatFindProto(reg, nd->type.CStr());
						if (pr && pr->parPixel)
							parPixel = true;
						continue;
					}
					for (uint32 k = 0; k < (uint32)nd->sockets.Size(); ++k) {
						const graph::NkSocket &sk = nd->sockets[k];
						// Le noeud d'entree ne contribue que par ses SORTIES, et
						// reciproquement. Une prise du mauvais sens sur un noeud
						// frontiere n'est pas de l'interface.
						if (estEntree && sk.dir != NkSocketDir::Output)
							continue;
						if (estSortie && sk.dir != NkSocketDir::Input)
							continue;
						if (n >= NK_MAT_GROUPE_PRISES_MAX) {
							tropDePrises = true;
							break;
						}
						const NkString *tn = sousGraphe.TypeName(sk.type);
						decls[n].name = sk.name.CStr();
						decls[n].type = tn ? tn->CStr() : "";
						decls[n].dir = estEntree ? NkSocketDir::Input : NkSocketDir::Output;
						decls[n].constanteSeulement = false;
						++n;
					}
				}
				if (tropDePrises)
					return NkMatRegistreErreur::TropDePrises;
				// `Enregistre` RECOPIE tout : les `CStr()` ci-dessus pointent dans
				// le sous-graphe, qui peut disparaitre juste apres.
				return reg.Enregistre(key, label && label[0] ? label : key, decls, n, parPixel);
			}

			// Instancie un prototype DANS le graphe : cree le noeud puis ajoute
			// ses prises, dans l'ordre declare.
			//
			// Rend NK_NODE_INVALID si la cle est inconnue ou si les types n'ont
			// pas ete enregistres. On ne cree PAS un noeud a moitie forme : un
			// noeud sans ses prises se relierait mal et l'erreur se verrait trois
			// manipulations plus loin, loin de sa cause.
			inline NkNodeId NkMatAddNode(const NkMatRegistreProtos &reg, NkNodeGraph &g, const char *protoKey) {
				const NkMatNodeProto *p = NkMatFindProto(reg, protoKey);
				if (!p)
					return graph::NK_NODE_INVALID;
				// Les types doivent DEJA etre enregistres : les chercher, jamais
				// les creer ici. Un RegisterType() implicite masquerait un appel
				// oublie a NkMatRegisterTypes et priverait le graphe de ses
				// conversions — il refuserait alors des liens parfaitement bons.
				for (uint32 i = 0; i < p->socketCount; ++i)
					if (g.FindType(p->sockets[i].type) == graph::NK_TYPE_INVALID)
						return graph::NK_NODE_INVALID;

				const NkNodeId n = g.AddNode(p->key, p->label);
				if (n == graph::NK_NODE_INVALID)
					return n;
				for (uint32 i = 0; i < p->socketCount; ++i) {
					const NkMatSocketDecl &s = p->sockets[i];
					g.AddSocket(n, s.name, g.FindType(s.type), s.dir);
				}
				return n;
			}

			// Une prise accepte-t-elle un lien ? Isole en une fonction pour que la
			// requete ET le banc jugent par le MEME chemin : un drapeau lu a deux
			// endroits differents finit par etre lu differemment.
			inline bool NkMatPriseAccepteUnLien(const NkMatSocketDecl &sd) {
				return !sd.constanteSeulement;
			}

			// ── « QUE PUIS-JE BRANCHER ICI ? » ───────────────────────────────
			// La demande de Rodolf (2026-08-22) : dans Blender, chaque parametre
			// porte un point, et cliquer ce point ouvre un menu de sources
			// FILTRE PAR LE TYPE DE LA PRISE — le menu de `Base Color` et celui
			// de `Roughness` n'ont pas le meme contenu.
			//
			// ⚠️ LA REPONSE SE CALCULE, ELLE NE S'ECRIT PAS. Une liste tenue a la
			// main par famille se perimerait au premier noeud ajoute, et
			// divergerait de ce que le graphe accepte VRAIMENT — on aurait alors
			// un menu qui propose ce que `Connect` refuse. Ici la question est
			// posee au registre : un prototype est proposable si l'une de ses
			// SORTIES est acceptee par la prise, conversions dirigees comprises.
			//
			// Consequence directe, et c'est le principe que Rodolf a formule :
			// **un parametre n'est pas une valeur, c'est une EXPRESSION d'un type
			// donne.** Une constante en est une forme, un echantillonnage une
			// autre. Ce qui les rend interchangeables est le TYPE.
			//
			// Rend le nombre de prototypes proposables ; remplit `out` jusqu'a
			// `maxOut`. `out` peut etre nul pour ne compter que.
			inline uint32 NkMatNoeudsPourPrise(const NkMatRegistreProtos &reg, const NkNodeGraph &g, NkTypeId typePrise,
											   const NkMatNodeProto **out = nullptr, uint32 maxOut = 0) {
				if (typePrise == NK_TYPE_INVALID)
					return 0;
				uint32 n = 0;
				for (uint32 i = 0; i < NkMatProtoCount(reg); ++i) {
					const NkMatNodeProto *p = NkMatProtoAt(reg, i);
					bool proposable = false;
					for (uint32 k = 0; k < p->socketCount; ++k) {
						const NkMatSocketDecl &sd = p->sockets[k];
						if (sd.dir != NkSocketDir::Output)
							continue;
						// `Accepts` porte les conversions DIRIGEES : un reel
						// alimente une couleur, l'inverse est refuse. C'est ce qui
						// fait que le menu d'une prise couleur contient `Value`,
						// et que celui d'une prise reelle ne contient pas `RGB`.
						if (g.Accepts(typePrise, g.FindType(sd.type))) {
							proposable = true;
							break;
						}
					}
					// Un noeud SANS AUCUNE SORTIE — le Material Output — n'est
					// jamais proposable. Rien ne peut sortir de lui : c'est un
					// puits. Il tombe naturellement, sans cas particulier.
					if (!proposable)
						continue;
					if (out && n < maxOut)
						out[n] = p;
					++n;
				}
				return n;
			}

			// La meme question, posee comme l'interface la pose : « sur CETTE
			// prise de CE noeud ». Rend 0 si la prise est declaree constante.
			inline uint32 NkMatNoeudsPourPriseDe(const NkMatRegistreProtos &reg, const NkNodeGraph &g, const char *protoKey,
												 const char *prise,
												 const NkMatNodeProto **out = nullptr, uint32 maxOut = 0) {
				const NkMatNodeProto *p = NkMatFindProto(reg, protoKey);
				if (!p || !prise)
					return 0;
				for (uint32 k = 0; k < p->socketCount; ++k) {
					const NkMatSocketDecl &sd = p->sockets[k];
					if (sd.dir != NkSocketDir::Input)
						continue;
					const char *a = sd.name;
					const char *b = prise;
					while (*a && *a == *b) {
						++a;
						++b;
					}
					if (*a || *b)
						continue;
					if (!NkMatPriseAccepteUnLien(sd))
						return 0; // pas de point de connexion sur cette prise
					return NkMatNoeudsPourPrise(reg, g, g.FindType(sd.type), out, maxOut);
				}
				return 0;
			}

			// ── VALIDATION PROPRE AU DOMAINE ─────────────────────────────────
			// Elle est une PASSE, jamais une precondition : un graphe en cours
			// d'edition passe son temps invalide — zero sortie pendant qu'on la
			// pose, deux sorties pendant qu'on remplace la premiere. Le refuser a
			// la construction rendrait le graphe chargeable mais pas editable.
			enum class NkMatGraphError : uint8 {
				Ok = 0,
				NoOutput,	   ///< aucun Material Output
				MultipleOutput, ///< plusieurs Material Output
				OutputUnlinked, ///< la sortie existe mais rien n'y entre
				Cycle,
				// ⚠️ CORRECTION du 2026-08-22, demandee par Rodolf, et c'est une
				// REPARATION et non une fonctionnalite. La validation de domaine
				// rendait `ok` sur un graphe portant un type de noeud absent du
				// catalogue : elle n'y frappait pas. Seul l'emetteur l'arretait,
				// tres loin de la cause. Maintenant qu'il existe UNE PORTE
				// (NkMatFindProto, deux sources), elle y frappe comme les autres.
				UnknownNodeType,
			};

			inline const char *NkMatGraphErrorName(NkMatGraphError e) {
				switch (e) {
					case NkMatGraphError::Ok:
						return "ok";
					case NkMatGraphError::NoOutput:
						return "aucune-sortie";
					case NkMatGraphError::MultipleOutput:
						return "plusieurs-sorties";
					case NkMatGraphError::OutputUnlinked:
						return "sortie-non-reliee";
					case NkMatGraphError::Cycle:
						return "cycle";
					case NkMatGraphError::UnknownNodeType:
						return "type-de-noeud-inconnu";
				}
				return "?";
			}

			// `outOutput` recoit le noeud de sortie quand il y en a exactement un.
			// `outDetail`, quand il est fourni, recoit le TYPE coupable pour les
			// diagnostics qui en designent un -- « type-de-noeud-inconnu » sans
			// dire lequel obligerait a fouiller un graphe de cent noeuds.
			inline NkMatGraphError NkMatValidate(const NkMatRegistreProtos &reg, const NkNodeGraph &g,
										 NkNodeId *outOutput = nullptr,
												 NkString *outDetail = nullptr) {
				NkNodeId found = graph::NK_NODE_INVALID;
				uint32 count = 0;

				// ⚠️ EN PREMIER, ET DELIBEREMENT. Un graphe dont on ne connait pas
				// les noeuds ne se valide pas « par ailleurs » : compter ses
				// sorties ou chercher son cycle donnerait un verdict sur une
				// structure qu'on ne comprend pas. Et le diagnostic serait pire
				// qu'inutile -- il nommerait un defaut secondaire pendant que la
				// vraie cause passe.
				//
				// ⚠️ Consequence assumee : les noeuds de GRAMMAIRE de groupe
				// (graph.instance, graph.entree, graph.sortie) sont refuses ici.
				// C'est correct AUJOURD'HUI -- l'emetteur ne sait pas les
				// compiler non plus -- et ce n'est pas un obstacle demain : ce
				// qu'on compile est le PLAN APLATI, d'ou les instances et les
				// frontieres ont deja disparu.
				for (uint32 i = 0; i < g.RawNodeCount(); ++i) {
					const graph::NkNode *n = g.RawNodeAt(i);
					if (!n || !n->alive)
						continue;
					if (!NkMatFindProto(reg, n->type.CStr())) {
						if (outDetail)
							*outDetail = n->type;
						if (outOutput)
							*outOutput = n->id;
						return NkMatGraphError::UnknownNodeType;
					}
				}
				// Parcours BRUT : il faut voir toute la table. Un noeud supprime
				// reste dans le tableau avec alive=false, et le compter ferait
				// dire « plusieurs sorties » a un graphe qui n'en a qu'une.
				for (uint32 i = 0; i < g.RawNodeCount(); ++i) {
					const graph::NkNode *n = g.RawNodeAt(i);
					if (!n || !n->alive)
						continue;
					const char *a = n->type.CStr();
					const char *b = NK_MN_OUTPUT;
					while (*a && *a == *b) {
						++a;
						++b;
					}
					if (*a == 0 && *b == 0) {
						++count;
						if (count == 1)
							found = n->id;
					}
				}
				if (outOutput)
					*outOutput = found;
				if (count == 0)
					return NkMatGraphError::NoOutput;
				if (count > 1)
					return NkMatGraphError::MultipleOutput;
				if (g.HasCycle())
					return NkMatGraphError::Cycle;

				const graph::NkNode *out = g.Find(found);
				const int32 s = out ? out->FindSocket("surface", NkSocketDir::Input) : -1;
				if (s < 0 || !g.IncomingOf(found, s))
					return NkMatGraphError::OutputUnlinked;
				return NkMatGraphError::Ok;
			}

		} // namespace matgraph
	} // namespace renderer
} // namespace nkentseu
