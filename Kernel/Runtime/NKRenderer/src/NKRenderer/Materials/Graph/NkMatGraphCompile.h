#pragma once
// -----------------------------------------------------------------------------
// @File    NkMatGraphCompile.h
// @Brief   Le compilateur : un graphe de materiau -> du NkSL.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POURQUOI IL EMET DU NkSL ET PAS DU GLSL
//   `Kernel/Runtime/NKSL` porte Frontend, Compiler, CodeGen, Reflection,
//   ShaderConvert et VM, et tourne sur cinq backends sur six. Un graphe qui emet
//   du NkSL herite donc GRATUITEMENT d'OpenGL, Vulkan, DX11 et DX12. Emettre du
//   GLSL a la main creerait un second generateur de shaders a maintenir en
//   parallele, et il divergerait. **Le graphe est un frontend de plus pour NkSL.**
//
// ⚠️ CE QU'UN RASTERISEUR PEUT ET NE PEUT PAS FAIRE — la question de Rodolf
//   « mixer les shader BSDF » n'a pas le meme sens selon le moteur :
//     Cycles (tracage de chemins) melange des CLOSURES — il tire une lobe au
//       hasard par rayon, ponderee par le facteur ;
//     EEVEE (rasteriseur)        APPROXIME — il melange les parametres avant
//       ombrage, ou les resultats apres.
//   NKRenderer est un rasteriseur. `Mix Shader` melange donc ici les PARAMETRES,
//   composante par composante. Ce n'est pas un renoncement : c'est ce que font
//   EEVEE, Unreal et Unity. La FORME du graphe est celle de Blender ; seule son
//   execution differe, et l'execution n'est pas l'affaire du graphe.
//
// ⚠️ TROIS CONTRAINTES DU DIALECTE NkSL, relevees dans `layeredv1.frag.nksl` et
//   qui cadrent tout ce fichier. Aucune n'est devinable :
//     1. PAS de variable locale de type struct utilisateur. Un « shader » ne peut
//        donc pas etre une struct : il est emis en QUATRE LOCALES SCALAIRES ou
//        vectorielles (albedo, metallic, roughness, emission). C'est exactement
//        le motif de l'accumulateur de LayeredV1.
//     2. PAS de retour de struct depuis un helper.
//     3. JAMAIS de varying referencee dans un helper — elle se passe en
//        parametre, sinon le generateur HLSL sort un `input.xxx` hors de l'entree
//        et DX refuse le shader (X3004). Ce compilateur n'emet aucun helper : il
//        met tout dans l'entree, ce qui rend la contrainte 3 sans objet.
//
// EN-TETE PUR, sans cible de build — comme NkNodeGraph et NkMatGraphTypes.h. Le
//   banc l'exerce sans lier NKRenderer.
//
// ZERO-STL : NkString / NkVector, aucun `std::`.
// -----------------------------------------------------------------------------

#include "NKRenderer/Materials/Graph/NkMatGraphTypes.h"
// ⚠️ LE PLAFOND DE TEXTURES ET LES SLOTS VIENNENT D'ICI, ET DE NULLE PART
// AILLEURS. Un compilateur qui autoriserait 8 slots quand le layout en
// declare 6 ecrirait sur deux bindings inexistants — sans erreur, sans
// journal, sans difference d'image. C'est la panne mesuree le 22/08.
#include "NKRenderer/Materials/NkMaterialBindings.h"

#include <math.h>  // powf : la seule fonction transcendante de l evaluateur processeur
#include <stdio.h> // snprintf : formatage des litteraux, pas de flux

namespace nkentseu {
	namespace renderer {
		namespace matgraph {

			// Un parametre expose, tel que le MOTEUR doit le voir.
			//
			// ⚠️ IL PORTE SON DECALAGE, PAS SON RANG. En `std140` un `vec3`
			// s'aligne sur 16 octets et en occupe 12 : trois reels a la suite
			// n'occupent PAS trois emplacements contigus. Un moteur qui deduirait
			// les positions de l'ordre de declaration ecrirait a cote des le
			// premier `vec3` — sans erreur, avec une valeur credible.
			struct NkMatParamExpose {
					NkString nom;	 ///< le nom PUBLIC, celui qu'emploie le code du jeu
					NkTypeId type = NK_TYPE_INVALID;
					uint32 decalage = 0; ///< en octets, depuis le debut du bloc
					// ⚠️ REMPLISSAGE A EMETTRE AVANT CE MEMBRE, en nombre de reels.
					//
					// Il est RANGE ici et non recalcule a l emission. Deux calculs d une
					// meme disposition finissent toujours par diverger, et celle-ci est
					// precisement celle dont le desaccord ne se voit pas : le shader lirait
					// a un endroit, le moteur ecrirait a un autre, et le pixel resterait
					// parfaitement plausible.
					uint32 remplissageAvant = 0;
					uint32 taille = 0;	 ///< en octets
					// ⚠️ LE DEFAUT EST CELUI DE LA PRISE, recopie ici pour que le
					// moteur n'ait pas a retraverser le graphe. Il n'existe PAS de
					// seconde valeur : deux sources pour une meme chose divergent,
					// et c'est alors l'editeur qui montre l'une pendant que le
					// moteur envoie l'autre.
					NkGraphValue defaut;
					bool bornes = false;
					float32 borneMin = 0.f, borneMax = 0.f;
					// D'ou il vient, pour qu'un message d'erreur puisse le designer.
					NkNodeId noeud = NK_NODE_INVALID;
					NkString prise;
			};

			// ── (a) UNE SORTIE NOMMEE, PAR MATERIAU, COTE PROCESSEUR ─────────
			//
			// Ce que le code de jeu recupere : un nom, un nombre de composantes,
			// une valeur. Rien d'autre — pas de handle, pas de pointeur dans le
			// graphe, parce que le graphe peut etre recompile et cette valeur doit
			// survivre.
			struct NkMatSortieMateriau {
					NkString nom;	///< le nom PUBLIC, celui qu'emploie le code de jeu
					NkString etage; ///< le MOT choisi par l'auteur, recopie tel quel
					uint32 composantes = 0;			  ///< 1 (un reel) ou 3 (une couleur)
					float32 valeur[3] = {0.f, 0.f, 0.f};

					// ⚠️ LA VALEUR CI-DESSUS N'EXISTE QUE POUR L'ETAGE (a).
					//
					// Une sortie « par_pixel_cible » n'a AUCUNE valeur cote
					// processeur : la sienne est calculee dans le shader, pixel
					// par pixel, et se lit dans la seconde cible de rendu. Le
					// tableau reste alors a zero -- et ce zero est exactement le
					// mensonge qu'on refuse partout ailleurs : il se lit comme
					// « la valeur vaut zero » alors qu'il veut dire « il n'y a pas
					// de valeur ici ».
					//
					// C'est le MEME piege que le canal alpha de la seconde cible,
					// un etage plus haut, et il se repare de la meme facon : par
					// un drapeau qui dit si la valeur veut dire quelque chose.
					// `etage` permettrait de le deduire ; ce drapeau evite d'avoir
					// a le savoir, et un lecteur qui l'ignore ne peut pas se
					// tromper en silence -- il lira `false` et devra s'en occuper.
					bool valeurConnue = false;

					// ⚠️ CE DONT LA VALEUR DEPEND, ET POURQUOI CE CHAMP EXISTE.
					//
					// L'etage (a) est « evalue une fois a la compilation OU AU
					// CHANGEMENT DE PARAMETRE ». Sans cette liste, la seconde
					// moitie de la phrase est inapplicable : le moteur ne saurait
					// pas QUAND reevaluer, et devrait soit tout recalculer a
					// chaque image — ce qui detruit l'argument « cout quasi nul »
					// — soit ne jamais recalculer, et la sortie se figerait sur la
					// valeur du jour de la compilation pendant que le parametre
					// bouge sous elle. Le second defaut est silencieux : la valeur
					// reste PLAUSIBLE, simplement perimee.
					//
					// Vide = constante pour toujours, aucune reevaluation.
					NkVector<NkString> dependDe;

					NkNodeId noeud = NK_NODE_INVALID; ///< pour qu'un message puisse la designer
			};

			struct NkMatCompileResult {
					bool ok = false;
					NkString source; ///< le NkSL emis
					NkString error;	 ///< renseigne SEULEMENT si ok == false

					// ── L'API PUBLIQUE ───────────────────────────────────────
					// La disposition du bloc uniforme, decrite ICI et NULLE PART
					// AILLEURS. A deux endroits, le moteur ecrirait a un decalage
					// et le shader lirait a un autre : ni erreur, ni journal, et
					// une valeur credible.
					NkVector<NkMatParamExpose> params;
					uint32 paramsTaille = 0; ///< taille totale du bloc, en octets

					// ⚠️ JETON DE COMPILATION. Recompiler un graphe EDITE peut
					// reordonner le bloc ; du code de jeu ayant retenu un decalage
					// ecrirait alors dans le mauvais parametre — sans erreur, avec
					// une valeur credible. **L'API du moteur est par NOM**, et tout
					// cache de decalage doit porter ce jeton et se jeter quand il
					// change.
					uint64 jeton = 0;

					// Les sorties nommees d'etage (a). Vide quand le graphe n'en
					// declare aucune — le cas de tous les graphes existants.
					NkVector<NkMatSortieMateriau> sorties;

					const NkMatParamExpose *TrouveParam(const char *nom) const;
					const NkMatSortieMateriau *TrouveSortie(const char *nom) const;
			};

			// ⚠️ L'API DU MOTEUR EST PAR NOM. Un decalage retenu par du code de jeu
			// devient faux des que le graphe est reedite et recompile ; le nom, lui,
			// ne ment pas. Tout cache de decalage doit porter le `jeton` et se
			// jeter quand il change.
			inline const NkMatParamExpose *NkMatCompileResult::TrouveParam(const char *nom) const {
				if (!nom)
					return nullptr;
				for (uint32 i = 0; i < (uint32)params.Size(); ++i) {
					const char *a = params[i].nom.CStr();
					const char *b = nom;
					while (*a && *a == *b) {
						++a;
						++b;
					}
					if (!*a && !*b)
						return &params[i];
				}
				return nullptr;
			}

			// Meme discipline que pour les parametres : PAR NOM. Un rang dans le
			// tableau se decale des qu'on ajoute une sortie au graphe.
			inline const NkMatSortieMateriau *NkMatCompileResult::TrouveSortie(const char *nom) const {
				if (!nom)
					return nullptr;
				for (uint32 i = 0; i < (uint32)sorties.Size(); ++i) {
					const char *a = sorties[i].nom.CStr();
					const char *b = nom;
					while (*a && *a == *b) {
						++a;
						++b;
					}
					if (!*a && !*b)
						return &sorties[i];
				}
				return nullptr;
			}

			namespace detail {

				// Litteral flottant pour un shader. `%.9g` traverse le texte sans
				// perdre un bit — mais il peut rendre « 1 », et « float x = 1; »
				// n'est pas « float x = 1.0; » dans tous les dialectes. On force
				// donc le point quand il manque.
				//
				// ⚠️ PRECAUTION NON PROUVEE, et je le dis plutot que de laisser
				// croire le contraire. Le 2026-08-22, la mutation qui RETIRE ce
				// forcage a SURVECU : les quatre backends exerces (GL, Vulkan,
				// DX11, DX12) acceptent « 2 » la ou l'on attend « 2.0 », et aucun
				// cas du banc n'est passe au rouge. Le forcage reste parce qu'il ne
				// coute rien et couvre les dialectes qu'on n'exerce PAS encore
				// (Metal, le backend logiciel) — mais il n'est aujourd'hui appuye
				// par aucune mesure, et il ne faut pas s'y fier comme a une regle
				// verifiee.
				inline void PutLit(NkString &out, float32 v) {
					char b[40];
					snprintf(b, sizeof(b), "%.9g", (double)v);
					bool aPoint = false;
					for (const char *p = b; *p; ++p)
						if (*p == '.' || *p == 'e' || *p == 'E' || *p == 'n' || *p == 'i')
							aPoint = true; // nan/inf compris : ne pas leur coller « .0 »
					out.Append(b);
					if (!aPoint)
						out.Append(".0");
				}

				// Comparaison de cles d'operation. Une fonction plutot qu'un
				// `strcmp` inline : elle est appelee une douzaine de fois par
				// noeud et une inversion d'argument y passerait inapercue.
				inline bool OpEst(const char *a, const char *b) {
					while (*a && *a == *b) {
						++a;
						++b;
					}
					return *a == 0 && *b == 0;
				}

				inline void PutU(NkString &out, uint32 v) {
					char b[16];
					snprintf(b, sizeof(b), "%u", v);
					out.Append(b);
				}

				// Nom de locale d'un noeud. On prend l'IDENTIFIANT, jamais le
				// libelle : un libelle est traduisible, peut contenir des espaces
				// et change sans prevenir. Un identifiant est stable par contrat.
				inline void PutNom(NkString &out, NkNodeId id, const char *suffixe) {
					out.Append("n");
					PutU(out, id);
					out.Append("_");
					out.Append(suffixe);
				}

				// Les quatre composantes d'un « shader » approxime a la EEVEE.
				// L'ordre est fixe et cite partout : le changer ici et pas la-bas
				// donnerait un shader qui compile et rend faux.
				// ⚠️ CINQ composantes depuis le 2026-08-22 : la NORMALE a rejoint le
				// lot. Sans elle, `Normal Map` et `Bump` n'avaient nulle part ou
				// aller — le puits recalculait `normalize(vNormal)` et jetait tout
				// travail de relief. Un noeud dont la sortie n'est lue par
				// personne est pire qu'un noeud absent : il donne l'illusion que
				// la capacite existe.
				static const uint32 kCompCount = 5;
				static const char *const kComp[5] = {"albedo", "metallic", "roughness", "emission", "normal"};
				static const char *const kCompType[5] = {"vec3", "float", "float", "vec3", "vec3"};

				// Litteral d'une valeur, adapte au TYPE ATTENDU par la prise. Une
				// couleur a 4 reels alimentant un vec3 perd son alpha — c'est
				// voulu et dit, pas un accident de conversion.
				inline void PutValeur(NkString &out, const NkGraphValue &v, const char *typeAttendu) {
					const uint32 n = (uint32)v.numbers.Size();
					const bool scalaire = typeAttendu[0] == 'f'; // "float"
					if (scalaire) {
						PutLit(out, n > 0 ? v.numbers[0] : 0.f);
						return;
					}
					out.Append("vec3(");
					for (uint32 i = 0; i < 3; ++i) {
						if (i)
							out.Append(", ");
						// Un seul reel se DIFFUSE sur les trois composantes : c'est
						// la conversion reel -> couleur declaree au registre, et
						// elle doit se comporter ici comme elle se declare la-bas.
						const float32 f = (n == 1) ? v.numbers[0] : (i < n ? v.numbers[i] : 0.f);
						PutLit(out, f);
					}
					out.Append(")");
				}


				// ── LES BRIQUES PROCEDURALES, RECOPIEES VERBATIM ─────────────
				//
				// ⚠️ POURQUOI RECOPIEES ET NON INCLUSES. Mesure le 2026-08-22 : le
				// `#include` du dialecte NkSL **ne se resout pas** quand le shader est
				// compile DEPUIS UNE CHAINE — le compilateur rend « #include not
				// found: Include/NkNoise.glsli » sur les quatre backends. Un shader
				// engendre n'existe pas sur disque : il doit donc etre AUTONOME.
				//
				// ⚠️ ET LA DUPLICATION EST GARDEE. Ce texte est copie MOT POUR MOT de
				// `Resources/NKRenderer/Shaders/Include/NkNoise.glsli`, et un cas de
				// banc **lit ce fichier sur le disque** pour verifier que chaque
				// fonction s'y retrouve a l'identique. Le jour ou quelqu'un corrige
				// une formule dans le `.glsli`, le banc passe au rouge tant que ce
				// texte n'a pas suivi. C'est la meme parade que pour les bindings :
				// comparer le code a une VERITE EXTERNE, faute de pouvoir partager.
				inline void PutBriquesBruit(NkString &out) {
					out.Append("float NkHash2(vec2 p) {\n");
					out.Append("    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);\n");
					out.Append("}\n");
					out.Append("\n");
					out.Append("vec2 NkHash22(vec2 p) {\n");
					out.Append("    vec2 q = vec2(dot(p, vec2(127.1, 311.7)),\n");
					out.Append("                  dot(p, vec2(269.5, 183.3)));\n");
					out.Append("    return fract(sin(q) * 43758.5453);\n");
					out.Append("}\n");
					out.Append("\n");
					out.Append("float NkValueNoise2D(vec2 p) {\n");
					out.Append("    vec2 i = floor(p);\n");
					out.Append("    vec2 f = fract(p);\n");
					out.Append("    vec2 u = f * f * (3.0 - 2.0 * f);  // smoothstep\n");
					out.Append("    float a = NkHash2(i);\n");
					out.Append("    float b = NkHash2(i + vec2(1.0, 0.0));\n");
					out.Append("    float c = NkHash2(i + vec2(0.0, 1.0));\n");
					out.Append("    float d = NkHash2(i + vec2(1.0, 1.0));\n");
					out.Append("    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);\n");
					out.Append("}\n");
					out.Append("\n");
					out.Append("float NkFBM2D(vec2 p, int octaves) {\n");
					out.Append("    float value = 0.0;\n");
					out.Append("    float amp = 0.5;\n");
					out.Append("    for (int i = 0; i < octaves; ++i) {\n");
					out.Append("        value += NkValueNoise2D(p) * amp;\n");
					out.Append("        p *= 2.0;\n");
					out.Append("        amp *= 0.5;\n");
					out.Append("    }\n");
					out.Append("    return value;\n");
					out.Append("}\n");
					out.Append("\n");
				}

				// La cellule la plus proche, sur le voisinage 3x3. Ecrite ICI et
				// non recopiee : aucun `.glsli` du depot ne porte de Voronoi —
				// verifie avant d'ecrire. Elle s'appuie en revanche sur
				// `NkHash22`, qui vient du fichier et reste gardee par le banc.
				//
				// ⚠️ La boucle est DEROULEE sur un voisinage FIXE de 3x3. Une
				// borne connue a la compilation est ce qui permet a tous les
				// backends de la derouler ; un rayon variable ferait de cette
				// fonction un shader qui compile ici et pas ailleurs.
				inline void PutBriqueVoronoi(NkString &out) {
					out.Append("float NkVoronoiF1(vec2 p) {\n");
					out.Append("    vec2 cell = floor(p);\n");
					out.Append("    vec2 f = fract(p);\n");
					out.Append("    float best = 8.0;\n");
					out.Append("    for (int j = -1; j <= 1; ++j) {\n");
					out.Append("        for (int i = -1; i <= 1; ++i) {\n");
					out.Append("            vec2 g2 = vec2(float(i), float(j));\n");
					out.Append("            vec2 o = NkHash22(cell + g2);\n");
					out.Append("            vec2 d = g2 + o - f;\n");
					out.Append("            best = min(best, dot(d, d));\n");
					out.Append("        }\n");
					out.Append("    }\n");
					out.Append("    return sqrt(best);\n");
					out.Append("}\n");
					out.Append("\n");
				}
			} // namespace detail

			// Combien de composantes porte un « shader » approxime. Public,
			// parce que le banc doit lire LA MEME verite que le compilateur :
			// un nombre recopie dans le banc cesserait de suivre le jour ou une
			// composante s ajoute, et le cas passerait au vert sans rien prouver.
			inline uint32 NkMatComposanteCount() {
				return detail::kCompCount;
			}


			// ── LE COMPILATEUR ───────────────────────────────────────────────
			// Parcours topologique, une locale par sortie, les entrees resolues
			// soit vers la locale qui les alimente, soit vers le DEFAUT de la
			// prise. C'est precisement pour cette derniere branche que les
			// defauts de prise ont ete ajoutes au coeur : sans eux, un
			// `Principled` non cable n'a aucune couleur a emettre.
			// ═════════════════════════════════════════════════════════════════
			//  (a) LES SORTIES NOMMEES EVALUEES SUR LE PROCESSEUR
			// ═════════════════════════════════════════════════════════════════

			namespace detail {

				// ── LA CONTAGION « PAR PIXEL » ───────────────────────────────
				//
				// Un noeud est par pixel s'il l'est INTRINSEQUEMENT (le registre
				// le dit) OU si l'une de ses entrees CONNECTEES vient d'un noeud
				// qui l'est. C'est une contagion, pas une propriete locale — et
				// c'est tout l'interet : personne ne branche `Texture Coordinate`
				// directement sur une sortie, on branche trois `Math` d'ecart.
				//
				// ⚠️ UNE PRISE EXPOSEE NE CONTAMINE PAS. Un parametre expose est
				// un UNIFORME : il vaut la meme chose pour tous les pixels du
				// materiau. Il rend la sortie DEPENDANTE — il faudra la reevaluer
				// quand il change — jamais par pixel. Confondre les deux
				// interdirait la moitie des usages utiles de l'etage (a).
				struct NkMatContagion {
						NkVector<NkNodeId> ids;
						NkVector<uint8> parPixel;

						bool Lit(NkNodeId id) const {
							for (uint32 i = 0; i < (uint32)ids.Size(); ++i)
								if (ids[i] == id)
									return parPixel[i] != 0;
							return false;
						}
						void Ecris(NkNodeId id, bool v) {
							ids.PushBack(id);
							parPixel.PushBack(v ? (uint8)1 : (uint8)0);
						}
				};

				// `ordre` est TOPOLOGIQUE : quand on traite un noeud, tous ses
				// amonts sont deja decides. C'est ce qui rend la passe lineaire au
				// lieu de recursive, et surtout ce qui la rend SURE — pas de pile
				// a borner, pas de cycle a craindre, TopoSort ayant deja echoue si
				// le graphe en avait un.
				inline void CalculeContagion(const NkMatRegistreProtos &reg, const NkNodeGraph &g, const NkVector<NkNodeId> &ordre,
											 NkMatContagion &out) {
					for (uint32 i = 0; i < (uint32)ordre.Size(); ++i) {
						const NkNode *n = g.Find(ordre[i]);
						if (!n)
							continue;
						bool pp = false;
						const NkMatNodeProto *pr = NkMatFindProto(reg, n->type.CStr());
						if (pr && pr->parPixel)
							pp = true;
						if (!pp) {
							for (uint32 k = 0; k < (uint32)n->sockets.Size() && !pp; ++k) {
								if (n->sockets[k].dir != NkSocketDir::Input)
									continue;
								const graph::NkLink *l = g.IncomingOf(n->id, (int32)k);
								if (l && out.Lit(l->fromNode))
									pp = true;
							}
						}
						out.Ecris(n->id, pp);
					}
				}

				// ── LE PREMIER NOEUD PAR PIXEL EN AMONT, POUR LE NOMMER ──────
				//
				// Refuser ne suffit pas : « cette sortie depend du pixel » laisse
				// l'auteur chercher lequel de ses quinze noeuds est coupable. On
				// remonte donc jusqu'a la SOURCE intrinseque et on la nomme.
				inline NkNodeId TrouveSourceParPixel(const NkMatRegistreProtos &reg, const NkNodeGraph &g, NkNodeId depart,
													 const NkMatContagion &c, NkVector<NkNodeId> &vus) {
					for (uint32 i = 0; i < (uint32)vus.Size(); ++i)
						if (vus[i] == depart)
							return NK_NODE_INVALID;
					vus.PushBack(depart);
					const NkNode *n = g.Find(depart);
					if (!n)
						return NK_NODE_INVALID;
					const NkMatNodeProto *pr = NkMatFindProto(reg, n->type.CStr());
					// On remonte D'ABORD : la source la plus profonde est la vraie
					// cause. Rendre le noeud courant des qu'il est intrinseque
					// nommerait le dernier maillon plutot que le premier, et
					// l'auteur corrigerait au mauvais endroit.
					for (uint32 k = 0; k < (uint32)n->sockets.Size(); ++k) {
						if (n->sockets[k].dir != NkSocketDir::Input)
							continue;
						const graph::NkLink *l = g.IncomingOf(n->id, (int32)k);
						if (!l || !c.Lit(l->fromNode))
							continue;
						const NkNodeId r = TrouveSourceParPixel(reg, g, l->fromNode, c, vus);
						if (r != NK_NODE_INVALID)
							return r;
					}
					return (pr && pr->parPixel) ? depart : NK_NODE_INVALID;
				}

				// ── LA VALEUR, SUR LE PROCESSEUR ─────────────────────────────
				struct NkMatValeurCPU {
						uint32 n = 0; ///< 1 = reel, 3 = vecteur ou couleur
						float32 v[3] = {0.f, 0.f, 0.f};
				};

				// ⚠️ CE QUE CET EVALUATEUR DOIT A LA LETTRE : DONNER LE MEME
				// RESULTAT QUE LE SHADER.
				//
				// Deux implementations d'une meme formule finissent toujours par
				// diverger. Ici la divergence serait INVISIBLE : personne ne
				// compare la valeur rendue au code de jeu avec ce que le pixel a
				// affiche, et les deux resteraient plausibles chacune de son cote.
				// La regle de survie est donc simple — toute formule ci-dessous
				// reproduit celle du shader, division par zero comprise, qui rend
				// 0 et non un NaN.
				struct NkMatEvalCPU {
						const NkNodeGraph *g = nullptr;
						NkMatTypes t;
						NkVector<NkString> *dependDe = nullptr;
						NkString erreur;

						bool Echoue(const char *quoi, const NkNode *n) {
							if (erreur.Size() == 0) {
								erreur = NkString(quoi);
								if (n) {
									erreur.Append(" : ");
									erreur.Append(n->type);
								}
							}
							return false;
						}

						// reel -> vecteur : on REPLIQUE, comme le shader ecrit
						// `vec3(x)`. Vecteur -> reel n'arrive jamais ici : la
						// conversion n'est pas permise et le graphe l'a refusee.
						static void Adapte(const NkMatValeurCPU &in, uint32 comp, NkMatValeurCPU &out) {
							out.n = comp;
							for (uint32 i = 0; i < 3; ++i)
								out.v[i] = (in.n == 1u) ? in.v[0] : (i < in.n ? in.v[i] : 0.f);
						}

						void NoteDependance(const NkNode &n, const char *prise) {
							if (!dependDe)
								return;
							NkString cle(NK_MPROP_EXPOSE_PREFIX);
							cle.Append(prise);
							for (uint32 i = 0; i < (uint32)n.props.Size(); ++i) {
								if (!(n.props[i].name == cle))
									continue;
								const NkString &nom = n.props[i].value.text;
								if (nom.Size() == 0)
									return;
								for (uint32 k = 0; k < (uint32)dependDe->Size(); ++k)
									if ((*dependDe)[k] == nom)
										return;
								dependDe->PushBack(nom);
								return;
							}
						}

						bool Prop(const NkNode &n, const char *cle, const NkGraphValue **out) const {
							for (uint32 i = 0; i < (uint32)n.props.Size(); ++i)
								if (n.props[i].name == NkString(cle)) {
									*out = &n.props[i].value;
									return true;
								}
							return false;
						}

						bool Entree(const NkNode &n, const char *prise, uint32 comp, NkMatValeurCPU &out);
						bool Sortie(NkNodeId id, int32 socket, NkMatValeurCPU &out);
				};

				inline bool NkMatEvalCPU::Entree(const NkNode &n, const char *prise, uint32 comp,
												 NkMatValeurCPU &out) {
					const int32 idx = n.FindSocket(prise, NkSocketDir::Input);
					if (idx < 0)
						return Echoue("prise inconnue a l evaluation", &n);
					const graph::NkLink *l = g->IncomingOf(n.id, idx);
					if (l) {
						NkMatValeurCPU amont;
						if (!Sortie(l->fromNode, l->fromSocket, amont))
							return false;
						Adapte(amont, comp, out);
						return true;
					}
					// Prise non connectee : sa valeur COURANTE est son defaut. Si
					// elle est exposee, ce defaut est aussi la valeur de depart du
					// parametre — et la sortie devient dependante de lui.
					NoteDependance(n, prise);
					const NkGraphValue &d = n.sockets[(uint32)idx].defaultValue;
					NkMatValeurCPU brut;
					if (d.IsSet() && d.numbers.Size() > 0) {
						brut.n = (uint32)d.numbers.Size() > 3u ? 3u : (uint32)d.numbers.Size();
						for (uint32 i = 0; i < brut.n; ++i)
							brut.v[i] = d.numbers[i];
					} else {
						brut.n = comp;
						for (uint32 i = 0; i < 3; ++i)
							brut.v[i] = 0.f;
					}
					Adapte(brut, comp, out);
					return true;
				}

			} // namespace detail

			namespace detail {

				inline float32 NkMatMixF(float32 a, float32 b, float32 f) {
					return a + (b - a) * f;
				}
				inline float32 NkMatClampF(float32 v, float32 a, float32 b) {
					return v < a ? a : (v > b ? b : v);
				}

				// La valeur d'une prise de SORTIE. Chaque branche reproduit la
				// ligne de shader citee en commentaire — c'est la seule protection
				// contre une divergence que personne ne verrait.
				inline bool NkMatEvalCPU::Sortie(NkNodeId id, int32 socket, NkMatValeurCPU &out) {
					const NkNode *n = g->Find(id);
					if (!n)
						return Echoue("noeud absent a l evaluation", nullptr);
					const NkString ty = n->type;

					if (ty == NkString(NK_MN_VALUE)) {
						const NkGraphValue *p = nullptr;
						out.n = 1;
						out.v[0] = (Prop(*n, NK_MPROP_VALUE, &p) && p->IsSet() && p->numbers.Size() > 0)
									   ? p->numbers[0]
									   : 0.f;
						return true;
					}
					if (ty == NkString(NK_MN_RGB)) {
						const NkGraphValue *p = nullptr;
						out.n = 3;
						const bool ok = Prop(*n, NK_MPROP_COLOR, &p) && p->IsSet() && p->numbers.Size() >= 3;
						for (uint32 i = 0; i < 3; ++i)
							out.v[i] = ok ? p->numbers[i] : 0.f;
						return true;
					}
					if (ty == NkString(NK_MN_MATH)) {
						NkMatValeurCPU a, b;
						if (!Entree(*n, "a", 1, a) || !Entree(*n, "b", 1, b))
							return false;
						const NkGraphValue *p = nullptr;
						const char *op = (Prop(*n, NK_MPROP_OPERATION, &p) && p->IsSet()) ? p->text.CStr() : "ajouter";
						if (NkMatTrouveOperation(false, op) < 0)
							return Echoue("operation inconnue a l evaluation", n);
						out.n = 1;
						if (OpEst(op, "ajouter"))
							out.v[0] = a.v[0] + b.v[0];
						else if (OpEst(op, "soustraire"))
							out.v[0] = a.v[0] - b.v[0];
						else if (OpEst(op, "multiplier"))
							out.v[0] = a.v[0] * b.v[0];
						else if (OpEst(op, "diviser"))
							// ⚠️ REND 0, exactement comme la garde du shader, et
							// pour la meme raison : un NaN contamine tout l'aval.
							// Si cette ligne divisait nue, la valeur rendue au code
							// de jeu serait un NaN la ou le pixel affiche 0 — deux
							// verites differentes pour un seul graphe.
							out.v[0] = (b.v[0] == 0.f) ? 0.f : a.v[0] / b.v[0];
						else if (OpEst(op, "minimum"))
							out.v[0] = a.v[0] < b.v[0] ? a.v[0] : b.v[0];
						else if (OpEst(op, "maximum"))
							out.v[0] = a.v[0] > b.v[0] ? a.v[0] : b.v[0];
						else if (OpEst(op, "puissance")) {
							// `pow` d'une base negative est indefini en GLSL comme
							// en C. Le shader emet `pow` nu ; on rend 0 plutot
							// qu'un NaN, et on le DIT ici pour que la difference
							// soit connue au lieu d'etre subie.
							out.v[0] = (a.v[0] < 0.f) ? 0.f : (float32)powf(a.v[0], b.v[0]);
						} else
							return Echoue("operation non evaluable", n);
						return true;
					}
					if (ty == NkString(NK_MN_MIX_COLOR)) {
						NkMatValeurCPU f, c1, c2;
						if (!Entree(*n, "fac", 1, f) || !Entree(*n, "color1", 3, c1) ||
							!Entree(*n, "color2", 3, c2))
							return false;
						const NkGraphValue *p = nullptr;
						const char *op = (Prop(*n, NK_MPROP_OPERATION, &p) && p->IsSet()) ? p->text.CStr() : "melanger";
						if (NkMatTrouveOperation(true, op) < 0)
							return Echoue("operation de melange inconnue a l evaluation", n);
						// `mix(color1, OP(color1, color2), clamp(fac, 0, 1))` — le
						// facteur melange color1 avec le RESULTAT, pas les deux
						// couleurs. Meme choix que Blender, meme ligne que le
						// shader ; l'inverser donnerait un resultat credible et faux.
						const float32 fc = NkMatClampF(f.v[0], 0.f, 1.f);
						out.n = 3;
						for (uint32 i = 0; i < 3; ++i) {
							float32 r = c2.v[i];
							if (OpEst(op, "melanger"))
								r = c2.v[i];
							else if (OpEst(op, "multiplier"))
								r = c1.v[i] * c2.v[i];
							else if (OpEst(op, "ajouter"))
								r = c1.v[i] + c2.v[i];
							else if (OpEst(op, "soustraire"))
								r = c1.v[i] - c2.v[i];
							else if (OpEst(op, "eclaircir"))
								r = c1.v[i] > c2.v[i] ? c1.v[i] : c2.v[i];
							else if (OpEst(op, "assombrir"))
								r = c1.v[i] < c2.v[i] ? c1.v[i] : c2.v[i];
							else
								return Echoue("operation de melange non evaluable", n);
							out.v[i] = NkMatMixF(c1.v[i], r, fc);
						}
						return true;
					}
					if (ty == NkString(NK_MN_MAPPING)) {
						NkMatValeurCPU v, sc, lo;
						if (!Entree(*n, "vector", 3, v) || !Entree(*n, "location", 3, lo))
							return false;
						// L'echelle vaut 1 par defaut, PAS 0 : un Mapping fraichement
						// pose ne doit rien changer. Meme neutre multiplicatif que
						// le shader, et c'est le genre de detail dont l'oubli rend
						// une sortie nulle sans qu'aucune erreur ne le dise.
						const int32 iv = n->FindSocket("scale", NkSocketDir::Input);
						const bool cable = iv >= 0 && g->IncomingOf(n->id, iv) != nullptr;
						const NkGraphValue *d = iv >= 0 ? &n->sockets[(uint32)iv].defaultValue : nullptr;
						if (cable || (d && d->IsSet())) {
							if (!Entree(*n, "scale", 3, sc))
								return false;
						} else {
							sc.n = 3;
							sc.v[0] = sc.v[1] = sc.v[2] = 1.f;
						}
						out.n = 3;
						for (uint32 i = 0; i < 3; ++i)
							out.v[i] = v.v[i] * sc.v[i] + lo.v[i];
						return true;
					}
					if (ty == NkString(NK_MN_SELECT) || ty == NkString(NK_MN_SELECT_COLOR)) {
						// Reproduit EXACTEMENT la ligne du shader :
						//     if ((condition) > NK_SELECT_SEUIL) { r = a; } else { r = b; }
						//
						// ⚠️ LE SEUIL VIENT DE LA MEME CONSTANTE DES DEUX COTES, et
						// ce n'est pas de la coquetterie : deux seuils qui
						// divergeraient donneraient une sortie d'etage (a) qui
						// bascule a un endroit et un pixel qui bascule a un autre.
						// Aucune erreur, aucun journal, et l'ecart n'apparait que
						// dans la bande etroite entre les deux seuils.
						//
						// ⚠️ ET IL FALLAIT L'ECRIRE, sinon le repli generique
						// (« noeud non evaluable sur le processeur ») aurait REFUSE
						// toute sortie nommee passant par un `Selectionner`. Le
						// refus etait sur, il n'etait pas juste : une condition
						// pilotee par un parametre expose est precisement ce qu'une
						// sortie d'etage (a) a vocation a suivre.
						const uint32 comp = (ty == NkString(NK_MN_SELECT_COLOR)) ? 3u : 1u;
						NkMatValeurCPU c, a, b;
						if (!Entree(*n, "condition", 1, c) || !Entree(*n, "si_vrai", comp, a) ||
							!Entree(*n, "si_faux", comp, b))
							return false;
						// ⚠️ LES DEUX ENTREES SONT LUES DANS TOUS LES CAS, et c'est
						// VOLONTAIRE : `Entree` note les dependances de parametre au
						// passage. N'en lire qu'une figerait `dependDe` sur la
						// branche du jour de la compilation, et la sortie cesserait
						// d'etre reevaluee des que la condition bascule.
						out.n = comp;
						const NkMatValeurCPU &pris = (c.v[0] > NK_SELECT_SEUIL) ? a : b;
						for (uint32 i = 0; i < comp; ++i)
							out.v[i] = pris.v[i];
						return true;
					}
					if (ty == NkString(NK_MN_SEPARATE_XYZ)) {
						NkMatValeurCPU v;
						if (!Entree(*n, "vector", 3, v))
							return false;
						// La composante vient du NUMERO DE PRISE DE SORTIE, pas
						// d'une propriete : x, y, z sont trois prises distinctes.
						const NkSocket *sk = (socket >= 0 && socket < (int32)n->sockets.Size())
												 ? &n->sockets[(uint32)socket]
												 : nullptr;
						if (!sk || sk->dir != NkSocketDir::Output)
							return Echoue("prise de sortie invalide sur separer_xyz", n);
						uint32 c = 0;
						if (sk->name == NkString("y"))
							c = 1;
						else if (sk->name == NkString("z"))
							c = 2;
						else if (!(sk->name == NkString("x")))
							return Echoue("prise inattendue sur separer_xyz", n);
						out.n = 1;
						out.v[0] = v.v[c];
						return true;
					}
					if (ty == NkString(NK_MN_COLOR_RAMP)) {
						NkMatValeurCPU f;
						if (!Entree(*n, "fac", 1, f))
							return false;
						// ⚠️ MEME DECOUPAGE QUE LE SHADER, par le MEME appel.
						// Redecouper les arrets a la main ici serait la divergence
						// annoncee : deux lectures d'un meme tableau finissent par
						// ne plus lire la meme chose, et c'est le compilateur qui
						// aurait raison sans que personne le sache.
						const NkGraphValue *ps = g->FindProp(n->id, NK_MPROP_STOPS);
						uint32 arrets = 0;
						const NkMatPointsErreur re = NkMatLisRampe(ps, &arrets);
						static const float32 kDefaut[8] = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
						const float32 *st = kDefaut;
						if (re == NkMatPointsErreur::Absente)
							arrets = 2;
						else if (re != NkMatPointsErreur::Ok)
							return Echoue("rampe invalide a l evaluation", n);
						else
							st = ps->numbers.Data();

						const NkGraphValue *pi = g->FindProp(n->id, NK_MPROP_INTERP);
						int32 ii = 0;
						if (pi && pi->IsSet()) {
							ii = NkMatTrouveInterp(pi->text.CStr());
							if (ii < 0)
								return Echoue("interpolation inconnue a l evaluation", n);
						}
						const NkMatOperation *idd = NkMatInterpAt((uint32)ii);
						if (!idd)
							return Echoue("interpolation hors table a l evaluation", n);
						const bool constante = OpEst(idd->cle, "constante");

						const float32 fc = NkMatClampF(f.v[0], 0.f, 1.f);
						out.n = 3;
						for (uint32 c = 0; c < 3; ++c)
							out.v[c] = st[1 + c];
						for (uint32 k = 1; k < arrets; ++k) {
							const float32 p0 = st[(k - 1) * NK_RAMP_REELS_PAR_ARRET];
							const float32 p1 = st[k * NK_RAMP_REELS_PAR_ARRET];
							const float32 w = constante ? (fc >= p1 ? 1.f : 0.f)
														: NkMatClampF((fc - p0) / (p1 - p0), 0.f, 1.f);
							for (uint32 c = 0; c < 3; ++c)
								out.v[c] = NkMatMixF(out.v[c], st[k * NK_RAMP_REELS_PAR_ARRET + 1 + c], w);
						}
						return true;
					}

					if (ty == NkString(NK_MN_FLOAT_CURVE)) {
						NkMatValeurCPU vin;
						if (!Entree(*n, "value", 1, vin))
							return false;
						// ⚠️ MEME DECOUPAGE QUE LE SHADER, PAR LE MEME APPEL.
						// Redecouper les points a la main ici serait la divergence
						// annoncee -- et cette fois elle serait DOUBLE, puisque la
						// rampe et la courbe partagent deja la lecture.
						const NkGraphValue *pp = g->FindProp(n->id, NK_MPROP_POINTS);
						uint32 pts = 0;
						const NkMatPointsErreur ce = NkMatLisCourbe(pp, &pts);
						static const float32 kDiag[4] = {0.f, 0.f, 1.f, 1.f};
						const float32 *cp = kDiag;
						if (ce == NkMatPointsErreur::Absente)
							pts = 2;
						else if (ce != NkMatPointsErreur::Ok)
							return Echoue("courbe invalide a l evaluation", n);
						else
							cp = pp->numbers.Data();

						const NkGraphValue *pi = g->FindProp(n->id, NK_MPROP_INTERP);
						int32 ii = 0;
						if (pi && pi->IsSet()) {
							ii = NkMatTrouveInterp(pi->text.CStr());
							if (ii < 0)
								return Echoue("interpolation inconnue a l evaluation", n);
						}
						const NkMatOperation *idc = NkMatInterpAt((uint32)ii);
						if (!idc)
							return Echoue("interpolation hors table a l evaluation", n);
						const bool constanteC = OpEst(idc->cle, "constante");

						const float32 cx = NkMatClampF(vin.v[0], cp[0],
													   cp[(pts - 1) * NK_CURVE_REELS_PAR_POINT]);
						float32 cy = cp[1];
						for (uint32 k = 1; k < pts; ++k) {
							const float32 x0 = cp[(k - 1) * NK_CURVE_REELS_PAR_POINT];
							const float32 x1 = cp[k * NK_CURVE_REELS_PAR_POINT];
							const float32 w = constanteC ? (cx >= x1 ? 1.f : 0.f)
														 : NkMatClampF((cx - x0) / (x1 - x0), 0.f, 1.f);
							cy = NkMatMixF(cy, cp[k * NK_CURVE_REELS_PAR_POINT + 1], w);
						}

						// Le meme defaut de `fac` qu au shader, pour la meme
						// raison -- et surtout LU AU MEME ENDROIT : un neutre a 1
						// ici et a 0 la-bas rendrait le processeur et le pixel
						// discordants sur un noeud fraichement pose.
						float32 f = 1.f;
						{
							const int32 iv = n->FindSocket("fac", NkSocketDir::Input);
							const bool cable = iv >= 0 && g->IncomingOf(n->id, iv) != nullptr;
							const NkGraphValue *d =
								iv >= 0 ? &n->sockets[(uint32)iv].defaultValue : nullptr;
							if (cable || (d && d->IsSet())) {
								NkMatValeurCPU fv;
								if (!Entree(*n, "fac", 1, fv))
									return false;
								f = NkMatClampF(fv.v[0], 0.f, 1.f);
							}
						}
						out.n = 1;
						out.v[0] = NkMatMixF(vin.v[0], cy, f);
						return true;
					}

					// ⚠️ TOUT LE RESTE REFUSE EN SE NOMMANT.
					//
					// Il n'y a PAS de repli ici, et c'est deliberé : rendre zero
					// pour un noeud qu'on ne sait pas evaluer donnerait une sortie
					// parfaitement formee, d'une valeur inventee, que le code de
					// jeu utiliserait sans defiance. Un refus qui nomme le type du
					// noeud coute une minute a l'auteur ; une valeur inventee coute
					// la confiance dans toutes les autres.
					return Echoue("noeud non evaluable sur le processeur", n);
				}

			} // namespace detail

			// ── (b1) : CE QUE LA PASSE RESOUT ───────────────────────────────
			// Le moteur resout UNE sortie nommee par passe, parce que l'API du
			// jeu est PAR NOM (cf. NkMatSortieMateriau). C'est ce qui evite de
			// devoir ranger un identifiant dans le tampon : il n'y a rien a
			// identifier, seulement a dire SI le pixel porte la valeur.
			struct NkMatCompileOptions {
					// Nom de la sortie « par_pixel_cible » a ecrire dans la
					// seconde cible. nullptr = laisser le compilateur choisir
					// quand il n'y a aucune ambiguite.
					const char *sortieParPixel = nullptr;
			};

			inline NkMatCompileResult NkMatCompileToNkSL(const NkMatRegistreProtos &reg, const NkNodeGraph &g,
														 const NkMatCompileOptions &opt = NkMatCompileOptions()) {
				NkMatCompileResult r;

				// 1. La validation de domaine d'abord. On ne compile pas un graphe
				// dont on sait qu'il n'a pas de sortie : le message doit venir
				// d'ici, pas d'une erreur de shader trois etages plus bas.
				NkNodeId sortie = NK_NODE_INVALID;
				NkString quoi;
				const NkMatGraphError ve = NkMatValidate(reg, g, &sortie, &quoi);
				if (ve != NkMatGraphError::Ok) {
					r.error = NkString("graphe invalide : ");
					r.error.Append(NkMatGraphErrorName(ve));
					// ⚠️ LE COUPABLE, QUAND IL Y EN A UN. Sans lui, la reparation
					// de `NkMatValidate` aurait DEGRADE le message : avant, un
					// type inconnu remontait « noeud non compilable : <le type> »
					// depuis l'emetteur ; la validation le rattrape desormais plus
					// tot -- donc plus pres de la cause -- et rendrait un
					// « type-de-noeud-inconnu » qui ne dit pas LEQUEL. Attraper
					// plus tot ne doit jamais faire perdre le nom.
					if (quoi.Size() > 0) {
						r.error.Append(" : ");
						r.error.Append(quoi);
						// ⚠️ ET S'IL EST INDISPONIBLE PLUTOT QU'INCONNU, ON LE DIT.
						// « type-de-noeud-inconnu : mat.info_objet » laisserait
						// croire a une faute de frappe ou a un oubli du
						// catalogue, alors que le noeud est parfaitement connu et
						// qu'il lui manque une donnee precise. Meme dispositif que
						// `pourquoiPas` sur les etages de sortie : un refus sec
						// envoie chercher au mauvais endroit.
						if (const char *pq = NkMatPourquoiIndisponible(quoi.CStr())) {
							r.error.Append(" -- ");
							r.error.Append(pq);
						}
					}
					return r;
				}
				// 2. Puis la validation STRUCTURELLE du coeur. Un graphe charge
				// depuis un fichier peut porter un lien de type incompatible que
				// l'API n'aurait jamais laisse poser.
				NkVector<graph::NkGraphDiag> diags;
				if (g.Validate(diags) != 0) {
					r.error = NkString("graphe malforme : ");
					r.error.Append(graph::NkGraphIssueName(diags[0].issue));
					return r;
				}

				// Les identifiants de type, retrouves depuis le registre du graphe.
				// On ne les REENREGISTRE pas : un `RegisterType` ici masquerait un
				// graphe dont les types n'ont jamais ete declares.
				NkMatTypes t;
				t.real = g.FindType(NK_MT_REAL);
				t.vector = g.FindType(NK_MT_VECTOR);
				t.color = g.FindType(NK_MT_COLOR);
				t.shader = g.FindType(NK_MT_SHADER);
				t.ramp = g.FindType(NK_MT_RAMP);

				NkVector<NkNodeId> ordre;
				if (!g.TopoSort(ordre)) {
					r.error = NkString("cycle");
					return r;
				}

				// ── PRE-PASSE : LES PARAMETRES EXPOSES ──────────────────────
				// C'est ici que le chantier devient une API publique : ce qui sort
				// de cette passe est ce que le code d'un jeu manipulera.
				for (uint32 i = 0; i < (uint32)ordre.Size(); ++i) {
					const NkNode *n = g.Find(ordre[i]);
					if (!n)
						continue;
					for (uint32 k = 0; k < (uint32)n->props.Size(); ++k) {
						const NkGraphProp &pr = n->props[k];
						// La cle est `expose.<prise>` : on isole le suffixe.
						const char *cle = pr.name.CStr();
						const char *pref = NK_MPROP_EXPOSE_PREFIX;
						const char *c = cle;
						while (*pref && *c == *pref) {
							++c;
							++pref;
						}
						if (*pref != 0)
							continue; // pas une propriete d'exposition
						const char *nomPrise = c;

						auto refuse = [&](const char *quoi) {
							r.error = NkString(quoi);
							r.error.Append(" : ");
							r.error.Append(n->type);
							r.error.Append(".");
							r.error.Append(nomPrise);
							r.source = NkString("");
						};

						const int32 idx = n->FindSocket(nomPrise, NkSocketDir::Input);
						if (idx < 0) {
							// Une exposition sur une prise inexistante est morte :
							// elle ne pilotera jamais rien, et personne ne le
							// remarquera. C'est typiquement ce que laisse un
							// renommage de prise.
							refuse("expose sur une prise inconnue");
							return r;
						}
						const NkSocket &sk = n->sockets[(uint32)idx];

						// 🔴 UNE PRISE CONNECTEE **ET** EXPOSEE EST REFUSEE.
						// C'est le cas le plus insidieux de tous : le lien
						// REMPLACE la valeur exposee, donc `SetFloat("usure", …)`
						// ne fait RIEN. Le materiau compile, il rend, et le
						// parametre est mort — aucune erreur, aucun journal.
						//
						// ⚠️ Chez Blender le probleme ne se pose pas parce que
						// brancher un lien FAIT DISPARAITRE le widget : l'interface
						// rend l'etat impossible. Nous n'avons pas d'interface —
						// c'est donc la validation qui doit le rendre impossible.
						if (g.IncomingOf(n->id, idx)) {
							refuse("prise CONNECTEE et exposee (le lien remplacerait la valeur exposee, "
								   "qui ne piloterait plus rien)");
							return r;
						}

						if (!pr.value.IsSet() || !NkMatNomPublicValide(pr.value.text.CStr())) {
							// Le nom devient un membre de bloc uniforme ET une cle
							// pour le code du jeu : un nom a espaces produirait un
							// shader invalide, et l'erreur accuserait le
							// generateur au lieu du nom.
							refuse("nom public absent ou invalide (lettres, chiffres et souligne, ne commencant "
								   "pas par un chiffre)");
							return r;
						}
						// ═══════════════════════════════════════════════════════
						// 🔴 CE QU'UN TAMPON UNIFORME ACCEPTE — LA LISTE EST CLOSE
						// ═══════════════════════════════════════════════════════
						//
						// ⚠️ LA CONTRAINTE VIENT DU CHOIX D'EN DESSOUS, PAS D'ICI.
						//
						// 🔴 ET ELLE N'EST PAS UNE PREFERENCE DE PERFORMANCE : ELLE
						// TOMBE DE LA REGLE D'ORDRE. Le nodal APPUIE le non nodal,
						// donc le graphe COMPILE VERS `NkMaterial` et disparait.
						// UN JEU LIVRE CHARGE DES MATERIAUX, PAS DES GRAPHES : il
						// n'y a rien a recompiler a l'execution PARCE QUE LE GRAPHE
						// N'EST PLUS LA. C'est ce qui rend « ne jamais recompiler a
						// chaque image » naturellement vrai, au lieu d'une
						// discipline qu'il faudrait tenir.
						//
						// ⚠️ COROLLAIRE, ET C'EST UN CRITERE D'ARCHITECTURE : RIEN A
						// L'EXECUTION NE DOIT DEPENDRE DE L'EXISTENCE D'UN GRAPHE.
						// Si un consommateur de `NkMaterial` a besoin d'interroger
						// le graphe qui l'a produit, ce n'est pas un manque d'API a
						// combler -- c'est un defaut a signaler. L'accommoder
						// ferait du graphe une dependance d'execution, et le jeu
						// livre embarquerait un compilateur.
						//
						// Un parametre ecrit depuis le gameplay ne doit JAMAIS
						// faire recompiler le graphe — reconstruire un shader a
						// chaque image est hors de question tant qu'on n'a pas de
						// quoi l'accelerer. Il vit donc dans un TAMPON UNIFORME,
						// et cela DECIDE de ce qui est exposable : seul est
						// exposable ce dont on connait la disposition en `std140`
						// ET en HLSL, et dont les deux tombent au meme octet.
						//
						// Trois types la remplissent, et ce sont les seuls :
						//   reel     -> `float`, 4 o
						//   vecteur  -> `vec3`, aligne 16, occupe 12
						//   couleur  -> `vec3`, idem
						//
						// ⚠️ AVANT, CETTE PASSE NE REFUSAIT QUE `shader`. C'etait
						// la REGLE 4 en pleine forme — « une condition
						// implicitement vraie parce qu'il n'existait qu'un cas ».
						// « tout ce qui n'est pas reel est un vec3 » n'etait vrai
						// que parce que `rampe` et `courbe` ne figurent
						// aujourd'hui sur AUCUNE prise d'ENTREE du catalogue :
						// `ColorRamp` porte sa rampe en PROPRIETE, pas en prise.
						// Le jour ou une prise de type `rampe` apparait — et la
						// porte du catalogue est ouverte a l'execution, donc ce
						// jour peut etre demain — l'emission ecrivait
						// `vec3 maRampe;` sans un mot, le moteur y versait douze
						// octets, et la rampe restait celle de la compilation.
						//
						// 📌 REGLE 5 : la convention existait deja UN ETAGE PLUS
						// BAS. Le rang 4 verifie ses canaux contre une liste
						// CLOSE derivee de ce que le shader LIT, et refuse un nom
						// absent EN LE NOMMANT. On applique ici la meme forme, et
						// pour la meme raison : une liste ouverte par defaut
						// rabat le cas inconnu sur un voisin plausible.
						//
						// ⚠️ CE QUE CETTE LISTE NE COUVRE PAS, ET N'A PAS A
						// COUVRIR : un changement STRUCTUREL — brancher une AUTRE
						// texture, changer de modele d'eclairage — n'est pas un
						// parametre. Cela se traite par deux chemins dans un
						// shader ou par des variantes compilees a l'avance.
						// C'est une affaire de COMPILATEUR, pas de graphe, et
						// aucune entree de ce bloc ne le rendra possible.
						if (!(sk.type == t.real || sk.type == t.vector || sk.type == t.color)) {
							const NkString *nt = g.TypeName(sk.type);
							NkString quoi("type non exposable dans un tampon uniforme '");
							quoi.Append(nt ? *nt : NkString("?"));
							quoi.Append("' (exposables : reel, vecteur, couleur — un tampon uniforme n'a de "
										"disposition connue que pour ceux-la)");
							refuse(quoi.CStr());
							return r;
						}

						NkMatParamExpose p;
						p.nom = pr.value.text;
						p.type = sk.type;
						p.defaut = sk.defaultValue;
						p.noeud = n->id;
						p.prise = NkString(nomPrise);
						if ((uint32)pr.value.numbers.Size() >= NK_EXPOSE_BORNES_REELS) {
							p.bornes = true;
							p.borneMin = pr.value.numbers[0];
							p.borneMax = pr.value.numbers[1];
						}

						// Unicite du nom PUBLIC. Deux parametres homonymes
						// rendraient `SetFloat` dependant de l'ordre de
						// declaration — donc l'un des deux inaccessible.
						for (uint32 q = 0; q < (uint32)r.params.Size(); ++q)
							if (r.params[q].nom == p.nom) {
								r.error = NkString("deux parametres exposes portent le meme nom public : ");
								r.error.Append(p.nom);
								r.source = NkString("");
								return r;
							}
						r.params.PushBack(p);
					}
				}

				// ═══════════════════════════════════════════════════════════════════
				//  LA DISPOSITION, ET POURQUOI ELLE PORTE DU REMPLISSAGE
				// ═══════════════════════════════════════════════════════════════════
				//
				// 🔴 `std140` ET HLSL NE RANGENT PAS UN BLOC DE LA MEME FACON, ET LEUR
				// DESACCORD EST MUET.
				//
				// Mesure du 22/08/2026, dans les deux sens, en lisant le PIXEL :
				//
				//     uniform NkGraphParams { float usure; vec3 teinte; };
				//
				//   - `std140` ALIGNE tout `vec3` sur 16 : teinte est a 16.
				//   - HLSL interdit seulement a un membre de CHEVAUCHER une frontiere
				//     de 16 octets ; un `float3` a besoin de 12 octets et tient donc
				//     ENTIER dans 4..16 : teinte est a **4**.
				//
				// Le moteur ecrivait a 16, le shader lisait a 4. Pixel mesure `(0,0,0)`,
				// aucune erreur, aucun journal. En forcant l ecriture a 4 : `(128,0,0)`
				// exact. Ces deux mesures ont clos trois jours de recherche dans la
				// couche RHI, qui n avait aucun defaut.
				//
				// ⚠️ AVEC UN SEUL `vec3` LES DEUX CONVENTIONS DONNENT ZERO. C est pour
				// cela que la panne n apparait qu au SECOND parametre, qu une matrice de
				// sept montages differents etait verte, et qu un banc verifiant
				// « 0/16/28 » restait vert en ne prouvant rien : il verifiait une
				// CONVENTION contre elle-meme, jamais un ACCORD avec ce que la carte lit.
				//
				// ── LE CHOIX : SUPPRIMER LE DESACCORD, PAS LE GERER ─────────────────
				// Deux autres voies existaient -- publier un decalage par famille de
				// backend, ou ecrire par nom a travers une couche qui connait le
				// backend. Toutes deux CONSERVENT deux dispositions et les reconcilient
				// a chaque ecriture. Celle-ci fait qu il n y en a plus qu une : on
				// REMPLIT jusqu a la prochaine frontiere de 16 avant chaque vecteur, et
				// les deux conventions tombent alors forcement au meme endroit.
				//
				//   float usure;
				//   float _nkPad0, _nkPad1, _nkPad2;   <- emis, vus des DEUX cotes
				//   vec3  teinte;                      <- 16 en std140 ET en HLSL
				//
				// C est aussi, deja, la convention manuelle des archetypes du depot :
				// `NkPBRParams` n a AUCUN membre `NkVec3f`, ses vecteurs sont tous des
				// `NkVec4f` et ses reels vont par groupes de quatre. On systematise une
				// regle de la maison, on n en invente pas une.
				//
				// Cout : quelques dizaines d octets par materiau. Le desaccord, lui,
				// coutait trois jours.
				{
					uint32 curseur = 0;
					for (uint32 i = 0; i < (uint32)r.params.Size(); ++i) {
						NkMatParamExpose &p = r.params[i];
						p.remplissageAvant = 0;
						if (p.type != t.real) {
							// Le curseur est toujours un multiple de 4 (un reel fait 4,
							// un vec3 en fait 12), donc le remplissage tombe juste en
							// nombre entier de reels.
							const uint32 manque = (16u - (curseur % 16u)) % 16u;
							p.remplissageAvant = manque / 4u;
							curseur += manque;
						}
						p.decalage = curseur;
						p.taille = NkMatStd140Size(p.type, t);
						curseur = p.decalage + p.taille;
					}
					// Un bloc uniforme se termine sur un multiple de 16.
					r.paramsTaille = NkMatAlignUp(curseur, 16u);
				}

				// ── PRE-PASSE : LES TEXTURES ────────────────────────────────
				// Il faut les compter AVANT d'ecrire le prologue, puisque c'est la
				// que leurs samplers se declarent. On leur attribue un slot dans
				// l'ordre topologique : deterministe, donc reproductible d'une
				// compilation a l'autre — un ordre qui varierait ferait changer le
				// shader emis sans qu'aucune donnee n'ait bouge.
				// Une base tangente coute deux paires de derivees : on ne l'emet que
				// si un noeud la reclame. Un shader qui la calculerait sans
				// l'utiliser paierait a chaque pixel pour rien.
				// Les briques de bruit ne sont emises que si un noeud les reclame :
				// un shader qui les porterait sans s'en servir alourdirait chaque
				// materiau pour rien.
				bool besoinBruit = false;
				bool besoinVoronoi = false;
				bool besoinTBN = false;
				// ⚠️ CONDITIONNEL, ET PAS PAR ECONOMIE DE CALCUL. Declarer
				// `nkViewV` dans TOUS les shaders changerait la source -- donc le
				// jeton, donc la signature -- de materiaux qui n'ont pas bouge.
				// Le banc temoin d'un autre chantier tomberait alors sur une
				// difference qui n'a rien a voir avec lui.
				bool besoinVue = false;
				for (uint32 i = 0; i < (uint32)ordre.Size(); ++i) {
					const NkNode *n = g.Find(ordre[i]);
					if (n && n->type == NkString(NK_MN_NORMAL_MAP))
						besoinTBN = true;
					if (n && (n->type == NkString(NK_MN_FRESNEL) || n->type == NkString(NK_MN_LAYER_WEIGHT) ||
							  n->type == NkString(NK_MN_GEOMETRY)))
						besoinVue = true;
					// ⚠️ Voronoi et Brick emploient les HACHAGES du meme fichier :
					// oublier l'un des trois donnerait un shader qui appelle une
					// fonction non declaree, et l'erreur accuserait le backend.
					if (n && (n->type == NkString(NK_MN_NOISE) || n->type == NkString(NK_MN_VORONOI) ||
							  n->type == NkString(NK_MN_BRICK)))
						besoinBruit = true;
					if (n && n->type == NkString(NK_MN_VORONOI))
						besoinVoronoi = true;
				}

				NkVector<NkNodeId> texNodes;
				for (uint32 i = 0; i < (uint32)ordre.Size(); ++i) {
					const NkNode *n = g.Find(ordre[i]);
					if (n && n->type == NkString(NK_MN_IMAGE_TEXTURE))
						texNodes.PushBack(n->id);
				}
				if ((uint32)texNodes.Size() > NK_MATBIND_GRAPH_SLOT_COUNT) {
					// ⚠️ LE REFUS DIT COMBIEN. « Trop de textures » seul obligerait
					// l'auteur a deviner ce qu'il doit retirer de son graphe.
					r.error = NkString("ce graphe demande ");
					detail::PutU(r.error, (uint32)texNodes.Size());
					r.error.Append(" textures, le plafond est ");
					detail::PutU(r.error, NK_MATBIND_GRAPH_SLOT_COUNT);
					return r;
				}

				// ── PROLOGUE ────────────────────────────────────────────────
				// Les varyings et l'UBO camera sont ceux du moteur, recopies a
				// l'identique de `layeredv1.frag.nksl` : un materiau genere doit
				// entrer dans le meme pipeline que les archetypes ecrits a la
				// main, sinon il faudrait un second chemin de rendu.
				// ⚠️ LES DEUX PASSES DE REFUS SONT ICI, ET PAS PLUS BAS.
				//
				// `s` est une REFERENCE sur `r.source` : tout refus pose apres son
				// premier `Append` rend une erreur ACCOMPAGNEE D'UNE SOURCE PARTIELLE.
				// Un appelant qui ne lit que `error` s'en apercevrait ; un appelant
				// distrait compilerait un shader a moitie forme et chercherait la
				// cause dans le backend.
				//
				// Le banc l'a attrape sur la passe des canaux (« rien emis=0 »), et le
				// meme defaut etait deja present sur celle de (b1) sans que personne
				// le voie : son cas verifiait le message, jamais l'absence de source.
				// ── (b1) : QUELLE sortie par pixel cette passe ecrit-elle ? ──
				// Choisi AVANT d'emettre : un refus doit sortir avant qu'une
				// seule ligne de shader soit ecrite, sinon on rend une source
				// partielle accompagnee d'une erreur.
				NkNodeId sortiePixel = NK_NODE_INVALID;
				{
					NkNodeId premiere = NK_NODE_INVALID;
					uint32 combien = 0;
					for (uint32 i = 0; i < g.RawNodeCount(); ++i) {
						const graph::NkNode *n = g.RawNodeAt(i);
						if (!n || !n->alive || !(n->type == NkString(NK_MN_OUTPUT_VALUE)))
							continue;
						const NkGraphValue *pe = g.FindProp(n->id, NK_MPROP_SORTIE_ETAGE);
						if (!pe || !pe->IsSet() || !(pe->text == NkString("par_pixel_cible")))
							continue;
						++combien;
						if (premiere == NK_NODE_INVALID)
							premiere = n->id;
						if (opt.sortieParPixel) {
							const NkGraphValue *pn = g.FindProp(n->id, NK_MPROP_SORTIE_NOM);
							if (pn && pn->IsSet() && pn->text == NkString(opt.sortieParPixel))
								sortiePixel = n->id;
						}
					}
					if (opt.sortieParPixel && sortiePixel == NK_NODE_INVALID) {
						// Demander une sortie qui n'existe pas est une ERREUR, pas
						// un tampon vide. Un tampon vide se lirait comme « ce
						// materiau ne porte pas cette valeur » -- indiscernable du
						// cas legitime, et donc jamais corrige.
						r.error = NkString("sortie par pixel demandee mais absente du graphe : ");
						r.error.Append(opt.sortieParPixel);
						return r;
					}
					if (!opt.sortieParPixel) {
						if (combien > 1) {
							// ⚠️ ON REFUSE, ON NE CHOISIT PAS. Prendre « la
							// premiere » rendrait une valeur parfaitement
							// plausible, issue d'une sortie que personne n'a
							// demandee -- et l'auteur n'aurait aucun moyen de
							// savoir laquelle il lit.
							r.error = NkString("plusieurs sorties « par_pixel_cible » et aucune demandee : "
											   "la passe n'en ecrit qu'une, il faut dire laquelle");
							return r;
						}
						sortiePixel = premiere;
					}
				}

				// ═══════════════════════════════════════════════════════════════
				// 🔴 UN TYPE DU GRAPHE DOIT ETRE EXPRIMABLE DANS LE MODELE NON NODAL
				// ═══════════════════════════════════════════════════════════════
				// Regle d'ordre de Rodolf, reaffirmee le 2026-08-23 : « le systeme
				// nodal vient APPUYER les systemes non nodaux. Donc il faut
				// toujours le non nodal, et APRES definir le nodal par-dessus. »
				//
				// Consequence directe, et c'est une contrainte D'EXISTENCE, pas de
				// style : LE GRAPHE COMPILE VERS `NkMaterial`, il ne le remplace
				// pas. Un materiau engendre doit etre le MEME objet qu'un materiau
				// ecrit a la main -- le graphe n'est qu'un producteur parmi
				// d'autres. Un type que la compilation n'a nulle part ou ecrire ne
				// peut donc pas exister dans le graphe, quelle que soit l'elegance
				// du registre qui sait le declarer.
				//
				// ⚠️ CE QUE `NkMaterial` SAIT PORTER, MESURE ET NON SUPPOSE :
				// `SetFloat`, `SetVec2/3/4`, `SetColor`, `SetInt`, `SetBool`,
				// `SetTexture`. NI ENUMERATION, NI STRUCTURE, NI UNION.
				//
				// La question a poser pour chaque type compose est donc : SOUS
				// QUELLE FORME SORT-IL DE LA COMPILATION ? Reponses d'aujourd'hui :
				//
				//   ENUMERATION -> un entier (`SetInt`) plus une table de noms
				//                  COTE EDITEUR SEULEMENT. Exprimable EN PRINCIPE,
				//                  pas construit : le bloc de parametres exposes
				//                  n'accepte que reel/vecteur/couleur.
				//   STRUCTURE   -> N parametres a plat (`truc.a`, `truc.b`).
				//                  Exprimable en principe, pas construit.
				//   UNION       -> RIEN. Quel membre est vivant est une notion
				//                  d'execution, et `NkMaterial` n'a aucun endroit
				//                  ou l'inscrire. Ce type n'a pas sa place ici, et
				//                  ce n'est pas une question de calendrier.
				//
				// On REFUSE donc tout type compose sur une prise, en nommant le
				// type ET ce qu'il faudrait pour l'accepter. Le refus n'est pas un
				// « pas encore » vague : il dit ce qui manque du cote NON NODAL.
				//
				// 📌 Le registre de NkNodeGraph garde la machinerie composite : le
				// coeur sert AUSSI l'AnimGraph et NkUIDesign, dont le modele non
				// nodal n'est pas `NkMaterial`. La contrainte est PAR DOMAINE, et
				// c'est au domaine de la faire respecter -- le coeur ne peut pas
				// savoir ou chacun ecrit.
				{
					for (uint32 i = 0; i < g.RawNodeCount(); ++i) {
						const graph::NkNode *n = g.RawNodeAt(i);
						if (!n || !n->alive)
							continue;
						for (uint32 k = 0; k < (uint32)n->sockets.Size(); ++k) {
							const graph::NkTypeKind genre = g.TypeKind(n->sockets[k].type);
							if (genre == graph::NkTypeKind::Leaf)
								continue;
							const NkString *nt = g.TypeName(n->sockets[k].type);
							r.error = NkString("type « ");
							r.error.Append(nt ? *nt : NkString("?"));
							r.error.Append(" » (");
							r.error.Append(graph::NkTypeKindName(genre));
							r.error.Append(") sur la prise « ");
							r.error.Append(n->sockets[k].name);
							r.error.Append(" » du noeud ");
							r.error.Append(n->type);
							r.error.Append(" : NkMaterial n'a aucune forme pour ce type. Le graphe COMPILE VERS "
										   "NkMaterial, il ne le remplace pas -- un type que la compilation n'a "
										   "nulle part ou ecrire ne peut pas exister dans le graphe. NkMaterial "
										   "porte float, vec2/3/4, couleur, entier, booleen, texture.");
							r.source = NkString("");
							return r;
						}
					}
				}

				// ── RANG 4 : LES NOMS DE CANAUX, VERIFIES AVANT D'EMETTRE ────
				//
				// ⚠️ C'EST ICI QUE « RIEN » NE DOIT PAS DEVENIR « ZERO ». Un
				// `UV Map` qui nomme un canal inexistant, un `Attribute` qui
				// nomme une donnee absente : la pente naturelle est de rendre
				// (0,0) et de laisser l'auteur devant une image noire. Il
				// chercherait une heure du cote de son materiau.
				//
				// Le nom est verifiable A LA COMPILATION -- la liste des canaux
				// est CLOSE, derivee des varyings que le shader porte reellement
				// -- donc on REFUSE, et on nomme a la fois le canal demande et ce
				// qui existe. Aucun pixel n'est jamais faux.
				{
					for (uint32 i = 0; i < g.RawNodeCount(); ++i) {
						const graph::NkNode *n = g.RawNodeAt(i);
						if (!n || !n->alive)
							continue;
						const bool estUV = n->type == NkString(NK_MN_UV_MAP);
						const bool estAttr = n->type == NkString(NK_MN_ATTRIBUTE);
						if (!estUV && !estAttr)
							continue;
						const char *cleProp = estUV ? NK_MPROP_CANAL_UV : NK_MPROP_ATTRIBUT;
						const NkGraphValue *pv = g.FindProp(n->id, cleProp);
						// ⚠️ AUCUN DEFAUT. Se rabattre sur « uv » quand la
						// propriete manque ferait marcher le noeud sans que
						// personne ait choisi -- et le jour ou un second canal
						// existera, le graphe changerait de sens tout seul.
						if (!pv || !pv->IsSet() || pv->text.Size() == 0) {
							r.error = NkString(estUV ? "UV Map sans canal : la propriete « canal » n'est pas "
													   "renseignee, et il n'y a pas de defaut"
													 : "Attribute sans nom : la propriete « nom » n'est pas "
													   "renseignee, et il n'y a pas de defaut");
							return r;
						}
						const NkMatCanal *c =
							estUV ? NkMatTrouveCanalUV(pv->text.CStr()) : NkMatTrouveAttribut(pv->text.CStr());
						if (!c) {
							r.error = NkString(estUV ? "canal UV inconnu « " : "attribut inconnu « ");
							r.error.Append(pv->text);
							r.error.Append(" » -- disponibles : ");
							const uint32 nn = estUV ? NkMatCanalUVCount() : NkMatAttributCount();
							for (uint32 k = 0; k < nn; ++k) {
								const NkMatCanal *d = estUV ? NkMatCanalUVAt(k) : NkMatAttributAt(k);
								if (k)
									r.error.Append(", ");
								r.error.Append(d->cle);
							}
							return r;
						}
					}
				}


				NkString &s = r.source;
				s.Append("// ENGENDRE par NkMatCompileToNkSL -- ne pas editer a la main.\n");
				s.Append("// Le graphe est la source ; ce fichier en est la trace lisible.\n");
				s.Append("//\n");
				s.Append("// Un « shader » est approxime a la EEVEE : quatre locales scalaires ou\n");
				s.Append("// vectorielles (albedo, metallic, roughness, emission), et non une closure.\n");
				s.Append("// NkSL n'accepte pas de variable locale de type struct utilisateur -- c'est\n");
				s.Append("// pour cette raison, pas par gout, que les composantes sont separees.\n\n");
				s.Append("@location(0) in vec3 vWorldPos;\n");
				s.Append("@location(1) in vec3 vNormal;\n");
				s.Append("@location(2) in vec2 vUV;\n");
				s.Append("@location(3) in vec4 vColor;\n\n");
				s.Append("@location(0) out vec4 fragColor;\n");
				// ⚠️ (b1) — LA SECONDE CIBLE EST DECLAREE PAR TOUS LES MATERIAUX,
				// Y COMPRIS CEUX QUI N'ONT AUCUNE SORTIE NOMMEE. Ce n'est pas de
				// l'uniformite pour l'uniformite : une sortie MRT NON ECRITE sur
				// un pixel COUVERT est INDEFINIE -- ni conservee, ni nulle. Un
				// materiau qui se contenterait de ne pas la declarer laisserait
				// donc dans le tampon ce qui s'y trouvait, ou n'importe quoi.
				//
				// LE CONTRAT, et il tient en une ligne :
				//   RGB = la valeur de la sortie resolue ; A = 1 si ce pixel la
				//   porte, 0 sinon. QUAND A == 0, RGB N'A AUCUN SENS et le
				//   lecteur n'a pas le droit de le lire.
				//
				// ⚠️ C'est A, et RIEN D'AUTRE, qui distingue « la valeur vaut
				// zero » de « ce materiau ne porte pas cette sortie ». Un lecteur
				// qui ignorerait A lirait 0.0 -- une humidite nulle parfaitement
				// credible sur un materiau qui n'a jamais entendu parler
				// d'humidite. Rien ne pousserait jamais a y revenir : c'est la
				// forme de mensonge la plus couteuse a debusquer.
				s.Append("@location(1) out vec4 fragAux;\n\n");
				s.Append("@binding(set=0, binding=0)\n");
				s.Append("uniform CameraUBO {\n");
				s.Append("    mat4  view;\n    mat4  proj;\n    mat4  viewProj;\n    mat4  invViewProj;\n");
				s.Append("    vec4  camPos;\n    vec4  camDir;\n    vec2  viewport;\n");
				s.Append("    float time;\n    float deltaTime;\n    float iblStrength;\n} uCam;\n\n");
				// Le bloc des parametres exposes. Les membres sont declares dans
				// l'ordre des decalages calcules — c'est ce qui fait que `std140`
				// place chacun la ou la disposition l'annonce.
				if (!r.params.Empty()) {
					s.Append("@binding(set=2, binding=");
					detail::PutU(s, NK_MATBIND_GRAPH_PARAMS);
					s.Append(")\nuniform ");
					s.Append(NK_MATBIND_GRAPH_PARAMS_BLOCK);
					s.Append(" {\n");
					uint32 nPad = 0;
					for (uint32 i = 0; i < (uint32)r.params.Size(); ++i) {
						// Le remplissage vient de la passe de disposition, jamais d un calcul
						// refait ici : c est la meme donnee, lue une fois. Il est NOMME et
						// VISIBLE dans la source engendree -- un remplissage invisible se
						// ferait supprimer par le premier qui trouverait le bloc trop gros.
						for (uint32 k = 0; k < r.params[i].remplissageAvant; ++k) {
							s.Append("    float _nkPad");
							detail::PutU(s, nPad++);
							s.Append("; // accord std140 / HLSL -- ne pas retirer\n");
						}
						s.Append("    ");
						s.Append(r.params[i].type == t.real ? "float " : "vec3  ");
						s.Append(r.params[i].nom);
						s.Append(";\n");
					}
					s.Append("} ");
					s.Append(NK_MATBIND_GRAPH_PARAMS_VAR);
					s.Append(";\n\n");
				}

				// Les samplers du graphe. AUCUN BINDING NEUF : on reutilise les
				// emplacements que le layout materiau declare deja, dans l'ordre.
				// Un materiau engendre n'a que faire de `tAlbedo` ou de `tHeight` —
				// ces slots sont morts pour lui, et le depot reutilise deja le meme
				// binding pour des usages differents selon l'archetype.
				for (uint32 i = 0; i < (uint32)texNodes.Size(); ++i) {
					s.Append("@binding(set=2, binding=");
					detail::PutU(s, NK_MATBIND_GRAPH_SLOTS[i]);
					s.Append(") uniform sampler2D ");
					s.Append(NK_MATBIND_GRAPH_SAMPLER_PREFIX);
					detail::PutU(s, i);
					s.Append(";\n");
				}
				if (!texNodes.Empty())
					s.Append("\n");

				if (besoinBruit)
					detail::PutBriquesBruit(s);
				if (besoinVoronoi)
					detail::PutBriqueVoronoi(s);

				s.Append("@stage(fragment)\n@entry\nvoid main() {\n");
				// La normale geometrique sert de defaut a toute prise `normal` non
				// cablee, et de base au relief. On la nomme une fois.
				s.Append("    vec3 nkGeomN = normalize(vNormal);\n");
				if (besoinVue) {
					// La direction PIXEL -> OEIL. C'est la convention de Blender
					// (`Incoming` pointe vers l'observateur) et celle du puits,
					// qui calcule deja son `V` de la meme facon. Deux conventions
					// opposees dans un meme shader donneraient un Fresnel qui
					// s'allume au centre au lieu des bords -- credible, et faux.
					s.Append("    vec3 nkViewV = normalize(uCam.camPos.xyz - vWorldPos);\n");
				}
				// Suit si la seconde cible a recu sa valeur. Voir plus bas : si
				// elle ne l'a pas recue, elle est ecrite a zero AVANT la fin de
				// main -- jamais laissee indefinie.
				bool auxEcrit = false;
				if (besoinTBN) {
					// ⚠️ BASE TANGENTE PAR DERIVEES D'ECRAN (cadre cotangent de
					// Schuler). Ce n'est pas un pis-aller : c'est deja ce que fait
					// `pbr.frag.nksl`, et pour une raison ecrite la-bas — les
					// tangentes de sommet peuvent etre nulles ou desalignees des
					// UV, et le relief part alors dans une direction ARBITRAIRE
					// par face. Ici T et B suivent exactement le sens des UV du
					// pixel, miroirs et rotations compris.
					//
					// Le vertex engendre ne fournit d'ailleurs AUCUNE tangente :
					// s'en passer n'est pas un choix, c'est la seule voie honnete.
					s.Append("    vec3 nkDpx = dFdx(vWorldPos);\n");
					s.Append("    vec3 nkDpy = dFdy(vWorldPos);\n");
					s.Append("    vec2 nkDux = dFdx(vUV);\n");
					s.Append("    vec2 nkDuy = dFdy(vUV);\n");
					s.Append("    vec3 nkPerpY = cross(nkDpy, nkGeomN);\n");
					s.Append("    vec3 nkPerpX = cross(nkGeomN, nkDpx);\n");
					s.Append("    vec3 nkT = nkPerpY * nkDux.x + nkPerpX * nkDuy.x;\n");
					s.Append("    vec3 nkB = nkPerpY * nkDux.y + nkPerpX * nkDuy.y;\n");
					// Normalisation commune : garde le rapport T/B, donc l'anisotropie
					// reelle des UV, la ou deux normalisations separees l'effacent.
					s.Append("    float nkInvMax = 1.0 / sqrt(max(max(dot(nkT, nkT), dot(nkB, nkB)), 1e-20));\n");
					s.Append("    nkT *= nkInvMax;\n    nkB *= nkInvMax;\n");
				}

				// Resout une entree : soit la locale du producteur, soit le defaut
				// de la prise, soit le neutre documente.
				auto ecrisEntree = [&](const NkNode &n, const char *prise, const char *typeAttendu,
									   const char *composante) {
					const int32 idx = n.FindSocket(prise, NkSocketDir::Input);
					if (idx >= 0) {
						const graph::NkLink *l = g.IncomingOf(n.id, idx);
						if (l) {
							// Une entree « shader » lit LA composante demandee du
							// producteur. Une entree ordinaire lit la locale nommee
							// d'apres LA PRISE SOURCE.
							//
							// ⚠️ POURQUOI PAS UN « val » UNIQUE, comme avant :
							// `Image Texture` a DEUX sorties — `color` et `alpha`.
							// Un nom unique par noeud ne peut pas les distinguer, et
							// l'entree qui lirait « la » valeur en prendrait une au
							// hasard. Le nom de la prise est deja la cle stable du
							// modele ; la locale en herite.
							if (composante) {
								detail::PutNom(s, l->fromNode, composante);
							} else {
								const NkNode *src = g.Find(l->fromNode);
								const char *nomPrise = "val";
								NkTypeId typeSource = graph::NK_TYPE_INVALID;
								if (src && l->fromSocket >= 0 &&
									(uint32)l->fromSocket < (uint32)src->sockets.Size()) {
									nomPrise = src->sockets[(uint32)l->fromSocket].name.CStr();
									typeSource = src->sockets[(uint32)l->fromSocket].type;
								}
								// ⚠️ ELARGISSEMENT reel -> vecteur, ET IL MANQUAIT.
								//
								// Le graphe AUTORISE reel -> couleur (conversion
								// dirigee), mais l'emetteur ecrivait la locale
								// telle quelle : `vec3 x = unFloat;` -- que GLSL
								// REFUSE. Le defaut est reste invisible parce que
								// les quatre GENERATEURS l'acceptent : ils
								// produisent du texte, ils ne verifient pas les
								// types. Seul glslang l'a dit, et il a fallu qu'un
								// cas branche pour de vrai une sortie REELLE sur
								// une entree COULEUR pour l'atteindre.
								//
								// C'est le garde-fou du depot pris en flagrant
								// delit : « ca compile sur les 4 backends » etait
								// vrai, et le shader ne compilait pas.
								const bool elargit =
									typeAttendu && typeAttendu[0] == 'v' && typeSource != graph::NK_TYPE_INVALID &&
									typeSource == t.real;
								if (elargit)
									s.Append("vec3(");
								detail::PutNom(s, l->fromNode, nomPrise);
								if (elargit)
									s.Append(")");
							}
							return;
						}
						// ⚠️ UNE PRISE EXPOSEE LIT LE BLOC UNIFORME, jamais un
						// litteral. C'est tout l'objet de l'exposition : la valeur
						// doit pouvoir changer a l'execution, donc elle ne peut pas
						// etre repliee dans le code. Le defaut, lui, part dans la
						// disposition — c'est la meme valeur, a un autre endroit.
						for (uint32 q = 0; q < (uint32)r.params.Size(); ++q)
							if (r.params[q].noeud == n.id && r.params[q].prise == NkString(prise)) {
								s.Append(NK_MATBIND_GRAPH_PARAMS_VAR);
								s.Append(".");
								s.Append(r.params[q].nom);
								return;
							}
						const NkGraphValue &d = n.sockets[(uint32)idx].defaultValue;
						if (d.IsSet()) {
							detail::PutValeur(s, d, typeAttendu);
							return;
						}
					}
					// ⚠️ CAS PARTICULIER DE LA PRISE `normal` : son neutre n'est pas
					// le noir mais la NORMALE GEOMETRIQUE. Un vecteur nul serait
					// une normale nulle, donc un eclairage indefini — et le noir
					// n'a aucun sens pour une direction. Ce n'est pas un repli
					// plausible : c'est le comportement DEFINI de la prise, celui
					// de Blender.
					{
						const char *a = prise;
						const char *b = "normal";
						while (*a && *a == *b) {
							++a;
							++b;
						}
						if (*a == 0 && *b == 0) {
							s.Append("nkGeomN");
							return;
						}
					}
					// ⚠️ NEUTRE DOCUMENTE, et il n'est pas anodin. Une entree ni
					// cablee ni renseignee ne doit pas produire une valeur
					// flatteuse : le blanc ferait passer un materiau non fini pour
					// un materiau clair. On rend donc le NOIR (ou zero), qui se
					// VOIT, exactement comme le repli d'un shader manquant.
					if (typeAttendu[0] == 'f')
						s.Append("0.0");
					else
						s.Append("vec3(0.0)");
				};

				// ── CORPS : un bloc par noeud, dans l'ordre topologique ─────
				for (uint32 i = 0; i < (uint32)ordre.Size(); ++i) {
					const NkNode *n = g.Find(ordre[i]);
					if (!n)
						continue;
					const NkString &t = n->type;

					s.Append("\n    // ");
					s.Append(n->type);
					s.Append(" #");
					detail::PutU(s, n->id);
					if (n->label.Size() > 0) {
						s.Append(" -- ");
						s.Append(n->label);
					}
					s.Append("\n");

					auto declare = [&](uint32 c, const char *expr) {
						s.Append("    ");
						s.Append(detail::kCompType[c]);
						s.Append(" ");
						detail::PutNom(s, n->id, detail::kComp[c]);
						s.Append(" = ");
						s.Append(expr);
						s.Append(";\n");
					};
					auto declareDebut = [&](uint32 c) {
						s.Append("    ");
						s.Append(detail::kCompType[c]);
						s.Append(" ");
						detail::PutNom(s, n->id, detail::kComp[c]);
						s.Append(" = ");
					};

					if (t == NkString(NK_MN_PRINCIPLED)) {
						declareDebut(0);
						ecrisEntree(*n, "base_color", "vec3", nullptr);
						s.Append(";\n");
						declareDebut(1);
						ecrisEntree(*n, "metallic", "float", nullptr);
						s.Append(";\n");
						declareDebut(2);
						ecrisEntree(*n, "roughness", "float", nullptr);
						s.Append(";\n");
						declareDebut(3);
						ecrisEntree(*n, "emission", "vec3", nullptr);
						s.Append(";\n");
						declareDebut(4);
						ecrisEntree(*n, "normal", "vec3", nullptr);
						s.Append(";\n");
					} else if (t == NkString(NK_MN_DIFFUSE)) {
						declareDebut(0);
						ecrisEntree(*n, "color", "vec3", nullptr);
						s.Append(";\n");
						// Un diffus n'est pas metallique : c'est la definition du
						// noeud, pas un defaut manquant.
						declare(1, "0.0");
						declareDebut(2);
						ecrisEntree(*n, "roughness", "float", nullptr);
						s.Append(";\n");
						declare(3, "vec3(0.0)");
						declareDebut(4);
						ecrisEntree(*n, "normal", "vec3", nullptr);
						s.Append(";\n");
					} else if (t == NkString(NK_MN_EMISSION)) {
						// Une emission n'a pas d'albedo diffus : toute son energie
						// part dans le canal emissif, multiplie par sa force.
						declare(0, "vec3(0.0)");
						declare(1, "0.0");
						declare(2, "1.0");
						declareDebut(3);
						ecrisEntree(*n, "color", "vec3", nullptr);
						s.Append(" * ");
						ecrisEntree(*n, "strength", "float", nullptr);
						s.Append(";\n");
						// Une emission n'a pas de relief : elle prend la normale
						// geometrique, et le dire vaut mieux que de l'omettre.
						declare(4, "nkGeomN");
					} else if (t == NkString(NK_MN_MIX_SHADER)) {
						// LE NOEUD QUE RODOLF A DEMANDE. Melange COMPOSANTE PAR
						// COMPOSANTE, borne a [0,1] : un facteur hors bornes
						// extrapolerait, ce que Blender ne fait pas non plus.
						s.Append("    float ");
						detail::PutNom(s, n->id, "fac");
						s.Append(" = clamp(");
						ecrisEntree(*n, "fac", "float", nullptr);
						s.Append(", 0.0, 1.0);\n");
						for (uint32 c = 0; c < detail::kCompCount; ++c) {
							declareDebut(c);
							s.Append("mix(");
							ecrisEntree(*n, "shader1", detail::kCompType[c], detail::kComp[c]);
							s.Append(", ");
							ecrisEntree(*n, "shader2", detail::kCompType[c], detail::kComp[c]);
							s.Append(", ");
							detail::PutNom(s, n->id, "fac");
							s.Append(");\n");
						}
						// ⚠️ Melanger deux directions ne rend pas une direction :
						// la somme ponderee de deux vecteurs unitaires ne l'est
						// plus. On renormalise, sinon l'eclairage s'assombrit la
						// ou les deux normales divergent — un defaut graduel, donc
						// qu'on attribue a autre chose.
						s.Append("    ");
						detail::PutNom(s, n->id, "normal");
						s.Append(" = normalize(");
						detail::PutNom(s, n->id, "normal");
						s.Append(");\n");
					} else if (t == NkString(NK_MN_VALUE) || t == NkString(NK_MN_RGB)) {
						// LES DEUX NOEUDS SOURCES, et les PREMIERS a lire une
						// PROPRIETE DE NOEUD jusque dans le shader. Leur valeur
						// n'est pas une prise : il n'y a rien a y brancher, c'est
						// le depart d'une chaine. Elle vit donc dans `props`.
						const bool estCouleur = (t == NkString(NK_MN_RGB));
						const NkGraphValue *pv =
							g.FindProp(n->id, estCouleur ? NK_MPROP_COLOR : NK_MPROP_VALUE);
						s.Append("    ");
						s.Append(estCouleur ? "vec3 " : "float ");
						// `RGB` sort sur « color », `Value` sur « value » : la locale
						// porte le nom de la prise, pas un « val » generique.
						detail::PutNom(s, n->id, estCouleur ? "color" : "value");
						s.Append(" = ");
						if (pv && pv->IsSet())
							detail::PutValeur(s, *pv, estCouleur ? "vec3" : "float");
						else
							s.Append(estCouleur ? "vec3(0.0)" : "0.0");
						s.Append(";\n");
					} else if (t == NkString(NK_MN_MATH) || t == NkString(NK_MN_MIX_COLOR)) {
						// ── LES DEUX NOEUDS DONT LE CALCUL EST CHOISI PAR UNE
						//    PROPRIETE ────────────────────────────────────────
						// Jusqu'ici une propriete portait une VALEUR ; celles-ci
						// portent une DECISION. C'est le premier endroit du
						// compilateur ou une propriete pilote le CODE EMIS.
						const bool couleur = (t == NkString(NK_MN_MIX_COLOR));
						const NkGraphValue *pop = g.FindProp(n->id, NK_MPROP_OPERATION);

						// ⚠️ UNE OPERATION INCONNUE FAIT ECHOUER LA COMPILATION.
						// La tentation est de retomber sur la premiere de la
						// liste — « ajouter » — parce que ca « marche ». Ce serait
						// le pire des choix : le materiau compilerait, rendrait,
						// et calculerait AUTRE CHOSE que ce que le fichier dit.
						// Un fichier ecrit par une version future, ou une faute de
						// frappe, passerait inapercu jusqu'au resultat.
						//
						// Une propriete ABSENTE, elle, est un cas different et
						// legitime : le noeud vient d'etre pose et l'auteur n'a
						// pas encore choisi. On prend alors le premier, et c'est
						// ecrit ici pour que ce ne soit pas confondu avec le cas
						// precedent.
						int32 iop = 0;
						if (pop && pop->IsSet()) {
							iop = NkMatTrouveOperation(couleur, pop->text.CStr());
							if (iop < 0) {
								r.error = NkString("operation inconnue sur ");
								r.error.Append(n->type);
								r.error.Append(" : ");
								r.error.Append(pop->text);
								r.source = NkString("");
								return r;
							}
						}
						// ⚠️ LE POINTEUR EST VERIFIE, PAS SUPPOSE.
						//
						//     UN CODE QUI NE PEUT SE TROMPER QUE PAR UN PLANTAGE
						//     N'EST PAS ROBUSTE, IL EST CHANCEUX.
						//
						// Trouve par une mutation, le 2026-08-22 : en retirant le
						// refus ci-dessus, `iop` restait a -1 et ce `->cle`
						// dereferencait un pointeur NUL. Le banc MOURAIT au lieu
						// d'echouer — donc la mutation etait bien tuee, mais par
						// un segfault et non par le cas prevu. La difference
						// compte : un plantage ne dit pas CE QUI est faux.
						const NkMatOperation *opd =
							(iop >= 0) ? NkMatOperationAt(couleur, (uint32)iop) : nullptr;
						if (!opd) {
							r.error = NkString("operation hors table sur ");
							r.error.Append(n->type);
							r.source = NkString("");
							return r;
						}
						const char *op = opd->cle;

						s.Append("    ");
						s.Append(couleur ? "vec3 " : "float ");
						detail::PutNom(s, n->id, couleur ? "color" : "value");
						s.Append(" = ");

						if (!couleur) {
							// Math : deux reels, une operation.
							const NkString a = NkString("a"), b = NkString("b");
							auto ecrisA = [&]() { ecrisEntree(*n, "a", "float", nullptr); };
							auto ecrisB = [&]() { ecrisEntree(*n, "b", "float", nullptr); };
							if (detail::OpEst(op, "ajouter")) {
								ecrisA();
								s.Append(" + ");
								ecrisB();
							} else if (detail::OpEst(op, "soustraire")) {
								ecrisA();
								s.Append(" - ");
								ecrisB();
							} else if (detail::OpEst(op, "multiplier")) {
								ecrisA();
								s.Append(" * ");
								ecrisB();
							} else if (detail::OpEst(op, "diviser")) {
								// ⚠️ DIVISION PAR ZERO : on rend 0, comme Blender.
								//
								// NE RETIRE PAS CETTE GARDE EN LA PRENANT POUR DE
								// LA TIMIDITE. Une division nue produit un NaN ;
								// le NaN CONTAMINE tout l'aval du graphe, et il se
								// voit comme un pixel noir ICI et blanc LA selon
								// le backend. Un defaut qui change d'aspect d'une
								// machine a l'autre FAIT ACCUSER LA MACHINE — on
								// cherche alors le pilote, la carte, le systeme,
								// et jamais la ligne de shader qui divise par
								// zero. C'est le cout reel de cette garde
								// absente : des heures passees au mauvais etage.
								s.Append("((");
								ecrisB();
								s.Append(") == 0.0 ? 0.0 : (");
								ecrisA();
								s.Append(") / (");
								ecrisB();
								s.Append("))");
							} else if (detail::OpEst(op, "minimum")) {
								s.Append("min(");
								ecrisA();
								s.Append(", ");
								ecrisB();
								s.Append(")");
							} else if (detail::OpEst(op, "maximum")) {
								s.Append("max(");
								ecrisA();
								s.Append(", ");
								ecrisB();
								s.Append(")");
							} else { // puissance
								// `pow` d'une base negative n'est pas defini : on
								// borne, comme Blender, plutot que de laisser le
								// resultat dependre du backend.
								s.Append("pow(max(");
								ecrisA();
								s.Append(", 0.0), ");
								ecrisB();
								s.Append(")");
							}
						} else {
							// Mix Color : le facteur melange color1 avec le
							// RESULTAT de l'operation, exactement comme Blender —
							// et non les deux couleurs directement.
							s.Append("mix(");
							ecrisEntree(*n, "color1", "vec3", nullptr);
							s.Append(", ");
							if (detail::OpEst(op, "melanger")) {
								ecrisEntree(*n, "color2", "vec3", nullptr);
							} else if (detail::OpEst(op, "multiplier")) {
								ecrisEntree(*n, "color1", "vec3", nullptr);
								s.Append(" * ");
								ecrisEntree(*n, "color2", "vec3", nullptr);
							} else if (detail::OpEst(op, "ajouter")) {
								ecrisEntree(*n, "color1", "vec3", nullptr);
								s.Append(" + ");
								ecrisEntree(*n, "color2", "vec3", nullptr);
							} else if (detail::OpEst(op, "soustraire")) {
								ecrisEntree(*n, "color1", "vec3", nullptr);
								s.Append(" - ");
								ecrisEntree(*n, "color2", "vec3", nullptr);
							} else if (detail::OpEst(op, "eclaircir")) {
								s.Append("max(");
								ecrisEntree(*n, "color1", "vec3", nullptr);
								s.Append(", ");
								ecrisEntree(*n, "color2", "vec3", nullptr);
								s.Append(")");
							} else { // assombrir
								s.Append("min(");
								ecrisEntree(*n, "color1", "vec3", nullptr);
								s.Append(", ");
								ecrisEntree(*n, "color2", "vec3", nullptr);
								s.Append(")");
							}
							s.Append(", clamp(");
							ecrisEntree(*n, "fac", "float", nullptr);
							s.Append(", 0.0, 1.0))");
						}
						s.Append(";\n");
					} else if (t == NkString(NK_MN_COLOR_RAMP)) {
						// ── LA PREMIERE PROPRIETE A CHARGE UTILE VARIABLE ─────
						// N arrets, quatre reels chacun. Le decoupage passe par
						// `NkMatLisRampe`, partage avec le banc : un decoupage
						// duplique finirait par diverger, et c'est le compilateur
						// qui aurait raison sans que personne le sache.
						const NkGraphValue *ps = g.FindProp(n->id, NK_MPROP_STOPS);
						uint32 arrets = 0;
						const NkMatPointsErreur re = NkMatLisRampe(ps, &arrets);

						// La rampe par defaut de Blender : noir -> blanc. C'est le
						// cas LEGITIME d'une propriete absente — le noeud vient
						// d'etre pose. Toute AUTRE erreur est un refus.
						static const float32 kDefaut[8] = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
						const float32 *st = kDefaut;
						if (re == NkMatPointsErreur::Absente) {
							arrets = 2;
						} else if (re != NkMatPointsErreur::Ok) {
							r.error = NkString("rampe invalide sur ");
							r.error.Append(n->type);
							r.error.Append(" : ");
							r.error.Append(NkMatRampeErreurNom(re));
							if (re == NkMatPointsErreur::TropDElements) {
								// ⚠️ LE REFUS DIT COMBIEN. « trop d'arrets » seul
								// oblige l'auteur a deviner ce qu'il doit retirer.
								r.error.Append(" (");
								detail::PutU(r.error, (uint32)ps->numbers.Size() / NK_RAMP_REELS_PAR_ARRET);
								r.error.Append(" demandes, plafond ");
								detail::PutU(r.error, NK_RAMP_ARRETS_MAX);
								r.error.Append(")");
							}
							r.source = NkString("");
							return r;
						} else {
							st = ps->numbers.Data();
						}

						// Le mode d'interpolation, meme discipline que les
						// operations : un mot, et un mot inconnu est refuse.
						const NkGraphValue *pi = g.FindProp(n->id, NK_MPROP_INTERP);
						int32 ii = 0;
						if (pi && pi->IsSet()) {
							ii = NkMatTrouveInterp(pi->text.CStr());
							if (ii < 0) {
								r.error = NkString("interpolation inconnue sur ");
								r.error.Append(n->type);
								r.error.Append(" : ");
								r.error.Append(pi->text);
								r.source = NkString("");
								return r;
							}
						}
						const NkMatOperation *id = NkMatInterpAt((uint32)ii);
						if (!id) {
							// Meme raison qu'ailleurs dans ce fichier : un code qui
							// ne peut se tromper que par un plantage n'est pas
							// robuste, il est chanceux.
							r.error = NkString("interpolation hors table");
							r.source = NkString("");
							return r;
						}
						const bool constante = detail::OpEst(id->cle, "constante");

						// Le facteur, borne : une rampe est definie sur [0,1], et
						// extrapoler au-dela donnerait des couleurs que l'auteur
						// n'a jamais choisies.
						s.Append("    float ");
						detail::PutNom(s, n->id, "f");
						s.Append(" = clamp(");
						ecrisEntree(*n, "fac", "float", nullptr);
						s.Append(", 0.0, 1.0);\n");

						// On part du PREMIER arret, puis on empile les suivants.
						// Deroule a la compilation : pas de boucle, pas d'index
						// dynamique — le shader reste lisible et aucun backend
						// n'a besoin d'indexation dynamique de tableau.
						s.Append("    vec3 ");
						detail::PutNom(s, n->id, "color");
						s.Append(" = vec3(");
						for (uint32 c = 0; c < 3; ++c) {
							if (c)
								s.Append(", ");
							detail::PutLit(s, st[1 + c]);
						}
						s.Append(");\n");

						for (uint32 k = 1; k < arrets; ++k) {
							const float32 p0 = st[(k - 1) * NK_RAMP_REELS_PAR_ARRET];
							const float32 p1 = st[k * NK_RAMP_REELS_PAR_ARRET];
							s.Append("    ");
							detail::PutNom(s, n->id, "color");
							s.Append(" = mix(");
							detail::PutNom(s, n->id, "color");
							s.Append(", vec3(");
							for (uint32 c = 0; c < 3; ++c) {
								if (c)
									s.Append(", ");
								detail::PutLit(s, st[k * NK_RAMP_REELS_PAR_ARRET + 1 + c]);
							}
							s.Append("), ");
							if (constante) {
								// Palier : la couleur bascule d'un coup a la
								// position de l'arret.
								s.Append("step(");
								detail::PutLit(s, p1);
								s.Append(", ");
								detail::PutNom(s, n->id, "f");
								s.Append(")");
							} else {
								// Lineaire. Le denominateur est calcule ICI, a la
								// compilation, et `NkMatLisRampe` a deja garanti
								// qu'il est strictement positif — les positions
								// egales sont refusees en amont. Il n'y a donc
								// aucune division a garder dans le shader.
								s.Append("clamp((");
								detail::PutNom(s, n->id, "f");
								s.Append(" - ");
								detail::PutLit(s, p0);
								s.Append(") * ");
								detail::PutLit(s, 1.f / (p1 - p0));
								s.Append(", 0.0, 1.0)");
							}
							s.Append(");\n");
						}
					} else if (t == NkString(NK_MN_FLOAT_CURVE)) {
						// ── LA SECONDE CHARGE VARIABLE, ET ELLE A UN AUTRE PAS ─
						// N points, DEUX reels chacun. Le decoupage passe par
						// `NkMatLisCourbe`, qui appelle la MEME lecture que la
						// rampe avec un pas different : c est la raison d etre du
						// parametre. Recopier la lecture ici aurait marche
						// aujourd hui et diverge au prochain durcissement.
						const NkGraphValue *pp = g.FindProp(n->id, NK_MPROP_POINTS);
						uint32 pts = 0;
						const NkMatPointsErreur ce = NkMatLisCourbe(pp, &pts);

						// La courbe par defaut de Blender : la DIAGONALE, (0,0) ->
						// (1,1). C est le cas LEGITIME d une propriete absente --
						// le noeud vient d etre pose et ne doit rien changer.
						static const float32 kDiag[4] = {0.f, 0.f, 1.f, 1.f};
						const float32 *cp = kDiag;
						if (ce == NkMatPointsErreur::Absente) {
							pts = 2;
						} else if (ce != NkMatPointsErreur::Ok) {
							r.error = NkString("courbe invalide sur ");
							r.error.Append(n->type);
							r.error.Append(" : ");
							r.error.Append(NkMatCourbeErreurNom(ce));
							if (ce == NkMatPointsErreur::TropDElements) {
								// ⚠️ LE REFUS DIT COMBIEN, meme raison que la
								// rampe : « trop de points » seul oblige l auteur
								// a deviner ce qu il doit retirer.
								r.error.Append(" (");
								detail::PutU(r.error, (uint32)pp->numbers.Size() / NK_CURVE_REELS_PAR_POINT);
								r.error.Append(" demandes, plafond ");
								detail::PutU(r.error, NK_CURVE_POINTS_MAX);
								r.error.Append(")");
							}
							r.source = NkString("");
							return r;
						} else {
							cp = pp->numbers.Data();
						}

						// Le mode d interpolation : la MEME table que la rampe.
						// Une seconde table « interpolations de courbe » aurait le
						// meme contenu et divergerait au premier ajout.
						const NkGraphValue *pi = g.FindProp(n->id, NK_MPROP_INTERP);
						int32 ii = 0;
						if (pi && pi->IsSet()) {
							ii = NkMatTrouveInterp(pi->text.CStr());
							if (ii < 0) {
								r.error = NkString("interpolation inconnue sur ");
								r.error.Append(n->type);
								r.error.Append(" : ");
								r.error.Append(pi->text);
								r.source = NkString("");
								return r;
							}
						}
						const NkMatOperation *idc = NkMatInterpAt((uint32)ii);
						if (!idc) {
							r.error = NkString("interpolation hors table");
							r.source = NkString("");
							return r;
						}
						const bool constanteC = detail::OpEst(idc->cle, "constante");

						// ⚠️ L ABSCISSE EST BORNEE AU DOMAINE DESSINE, pas a [0,1].
						// Une courbe peut tres bien avoir ete dessinee sur [0, 5] ;
						// borner a [0,1] ecraserait les quatre cinquiemes de son
						// trace en silence. Extrapoler au-dela du dernier point
						// donnerait, lui, des valeurs que l auteur n a jamais
						// tracees : on tient la derniere.
						s.Append("    float ");
						detail::PutNom(s, n->id, "cx");
						s.Append(" = clamp(");
						ecrisEntree(*n, "value", "float", nullptr);
						s.Append(", ");
						detail::PutLit(s, cp[0]);
						s.Append(", ");
						detail::PutLit(s, cp[(pts - 1) * NK_CURVE_REELS_PAR_POINT]);
						s.Append(");\n");

						// Deroule a la compilation, comme la rampe : pas de boucle,
						// pas d index dynamique, aucun backend n en a besoin.
						s.Append("    float ");
						detail::PutNom(s, n->id, "cy");
						s.Append(" = ");
						detail::PutLit(s, cp[1]);
						s.Append(";\n");

						for (uint32 k = 1; k < pts; ++k) {
							const float32 x0 = cp[(k - 1) * NK_CURVE_REELS_PAR_POINT];
							const float32 x1 = cp[k * NK_CURVE_REELS_PAR_POINT];
							s.Append("    ");
							detail::PutNom(s, n->id, "cy");
							s.Append(" = mix(");
							detail::PutNom(s, n->id, "cy");
							s.Append(", ");
							detail::PutLit(s, cp[k * NK_CURVE_REELS_PAR_POINT + 1]);
							s.Append(", ");
							if (constanteC) {
								s.Append("step(");
								detail::PutLit(s, x1);
								s.Append(", ");
								detail::PutNom(s, n->id, "cx");
								s.Append(")");
							} else {
								// La pente est calculee ICI, a la compilation, et
								// `NkMatLisCourbe` a deja garanti que le
								// denominateur est strictement positif -- les
								// positions egales sont refusees en amont. Aucune
								// division ne reste dans le shader.
								s.Append("clamp((");
								detail::PutNom(s, n->id, "cx");
								s.Append(" - ");
								detail::PutLit(s, x0);
								s.Append(") * ");
								detail::PutLit(s, 1.f / (x1 - x0));
								s.Append(", 0.0, 1.0)");
							}
							s.Append(");\n");
						}

						// ⚠️ `fac` VAUT 1 QUAND PERSONNE NE LE RENSEIGNE, et c est
						// le contraire du repli habituel. Le neutre arithmetique
						// serait 0 -- et a 0 ce noeud rend son entree telle quelle.
						// L auteur poserait un Float Curve, dessinerait sa courbe,
						// et ne verrait RIEN changer, sans le moindre message.
						// Meme raison que l echelle a 1 du Mapping : le neutre
						// d un noeud n est pas le zero de son operation.
						s.Append("    float ");
						detail::PutNom(s, n->id, "value");
						s.Append(" = mix(");
						ecrisEntree(*n, "value", "float", nullptr);
						s.Append(", ");
						detail::PutNom(s, n->id, "cy");
						s.Append(", ");
						{
							const int32 iv = n->FindSocket("fac", NkSocketDir::Input);
							const bool cable = iv >= 0 && g.IncomingOf(n->id, iv);
							const NkGraphValue *d =
								iv >= 0 ? &n->sockets[(uint32)iv].defaultValue : nullptr;
							if (cable || (d && d->IsSet())) {
								s.Append("clamp(");
								ecrisEntree(*n, "fac", "float", nullptr);
								s.Append(", 0.0, 1.0)");
							} else {
								s.Append("1.0"); // la courbe s applique
							}
						}
						s.Append(");\n");
					} else if (t == NkString(NK_MN_TEX_COORD)) {
						// L'UV du maillage, sans calcul. `vUV` est une varying, et
						// on est dans l'ENTREE : la contrainte du dialecte (jamais
						// de varying dans un helper) ne s'applique pas ici.
						s.Append("    vec3 ");
						detail::PutNom(s, n->id, "uv");
						s.Append(" = vec3(vUV, 0.0);\n");
					} else if (t == NkString(NK_MN_MAPPING)) {
						// vecteur * echelle + position. L'echelle vaut 1 et la
						// position 0 par defaut : un Mapping fraichement pose ne
						// doit RIEN changer, sinon l'auteur voit son image bouger
						// en branchant un noeud cense etre neutre.
						s.Append("    vec3 ");
						detail::PutNom(s, n->id, "vector_out");
						s.Append(" = (");
						ecrisEntree(*n, "vector", "vec3", nullptr);
						s.Append(") * (");
						{
							const int32 iv = n->FindSocket("scale", NkSocketDir::Input);
							const bool cable = iv >= 0 && g.IncomingOf(n->id, iv);
							const NkGraphValue *d =
								iv >= 0 ? &n->sockets[(uint32)iv].defaultValue : nullptr;
							if (cable || (d && d->IsSet()))
								ecrisEntree(*n, "scale", "vec3", nullptr);
							else
								s.Append("vec3(1.0)"); // neutre MULTIPLICATIF
						}
						s.Append(") + (");
						ecrisEntree(*n, "location", "vec3", nullptr);
						s.Append(");\n");
					} else if (t == NkString(NK_MN_IMAGE_TEXTURE)) {
						// La convention de normale, si elle est declaree, est
						// VALIDEE ici et n'influence RIEN dans le code emis : la
						// conversion se fait a l'import. Un mot inconnu est refuse
						// — un repli sur OpenGL inverserait le relief de la moitie
						// des fichiers, et l'image resterait plausible.
						{
							const NkGraphValue *pc = g.FindProp(n->id, NK_MPROP_NORMAL_CONV);
							if (pc && pc->IsSet() && NkMatTrouveConvNormale(pc->text.CStr()) < 0) {
								r.error = NkString("convention de normale inconnue sur ");
								r.error.Append(n->type);
								r.error.Append(" : ");
								r.error.Append(pc->text);
								r.source = NkString("");
								return r;
							}
						}
						// ── LE PREMIER NOEUD QUI CONSOMME UNE RESSOURCE ───────
						uint32 slot = 0;
						for (uint32 i = 0; i < (uint32)texNodes.Size(); ++i)
							if (texNodes[i] == n->id)
								slot = i;
						// La coordonnee : celle qu'on lui donne, ou l'UV du
						// maillage. Blender fait exactement ce repli, et il n'est
						// pas « plausible » — c'est le comportement DEFINI du
						// noeud, pas un bouche-trou.
						const int32 iv = n->FindSocket("vector", NkSocketDir::Input);
						const bool cable = iv >= 0 && g.IncomingOf(n->id, iv);
						s.Append("    vec4 ");
						detail::PutNom(s, n->id, "texel");
						s.Append(" = texture(");
						s.Append(NK_MATBIND_GRAPH_SAMPLER_PREFIX);
						detail::PutU(s, slot);
						s.Append(", ");
						if (cable) {
							s.Append("(");
							ecrisEntree(*n, "vector", "vec3", nullptr);
							s.Append(").xy");
						} else {
							s.Append("vUV");
						}
						s.Append(");\n");
						// DEUX sorties, deux locales nommees d'apres leurs prises.
						s.Append("    vec3 ");
						detail::PutNom(s, n->id, "color");
						s.Append(" = ");
						detail::PutNom(s, n->id, "texel");
						s.Append(".rgb;\n    float ");
						detail::PutNom(s, n->id, "alpha");
						s.Append(" = ");
						detail::PutNom(s, n->id, "texel");
						s.Append(".a;\n");
					} else if (t == NkString(NK_MN_SEPARATE_XYZ)) {
						// Trois sorties reelles. Le seul moyen, aujourd'hui, de
						// tirer un scalaire VARIABLE d'un graphe.
						static const char *const kAxes[3] = {"x", "y", "z"};
						for (uint32 a = 0; a < 3; ++a) {
							s.Append("    float ");
							detail::PutNom(s, n->id, kAxes[a]);
							s.Append(" = (");
							ecrisEntree(*n, "vector", "vec3", nullptr);
							s.Append(").");
							s.Append(kAxes[a]);
							s.Append(";\n");
						}
					} else if (t == NkString(NK_MN_MAP_RANGE)) {
						// Le mode de bornage, un MOT. Absent = `serre`, le defaut de Blender :
						// une valeur qui sort de sa plage sans prevenir donne des couleurs que
						// personne n a choisies. Un mot INCONNU est refuse en le nommant.
						const NkGraphValue *pb = g.FindProp(n->id, NK_MPROP_BORNAGE);
						int32 ib = 0;
						if (pb && pb->IsSet()) {
							ib = NkMatTrouveBornage(pb->text.CStr());
							if (ib < 0) {
								r.error = NkString("bornage inconnu sur ");
								r.error.Append(n->type);
								r.error.Append(" : ");
								r.error.Append(pb->text);
								r.source = NkString("");
								return r;
							}
						}
						const NkMatOperation *ob = NkMatBornageAt((uint32)ib);
						if (!ob) {
							r.error = NkString("bornage hors table");
							r.source = NkString("");
							return r;
						}
						const bool serre = detail::OpEst(ob->cle, "serre");
						// ⚠️ LA DIVISION EST GARDEE, et ce n'est pas de la timidite.
						// Si `from_min == from_max` la plage d'entree est vide : une
						// division nue rendrait un NaN, et un NaN CONTAMINE tout
						// l'aval en se voyant noir ici et blanc la selon le backend.
						// Blender rend `to_min` dans ce cas ; on fait pareil, et le
						// resultat reste une valeur que l'auteur a choisie.
						s.Append("    float ");
						detail::PutNom(s, n->id, "den");
						s.Append(" = (");
						ecrisEntree(*n, "from_max", "float", nullptr);
						s.Append(") - (");
						ecrisEntree(*n, "from_min", "float", nullptr);
						s.Append(");\n");
						s.Append("    float ");
						detail::PutNom(s, n->id, "t");
						s.Append(" = (abs(");
						detail::PutNom(s, n->id, "den");
						s.Append(") < 1e-8) ? 0.0 : ((");
						ecrisEntree(*n, "value", "float", nullptr);
						s.Append(") - (");
						ecrisEntree(*n, "from_min", "float", nullptr);
						s.Append(")) / ");
						detail::PutNom(s, n->id, "den");
						s.Append(";\n");
						if (serre) {
							s.Append("    ");
							detail::PutNom(s, n->id, "t");
							s.Append(" = clamp(");
							detail::PutNom(s, n->id, "t");
							s.Append(", 0.0, 1.0);\n");
						}
						s.Append("    float ");
						detail::PutNom(s, n->id, "result");
						s.Append(" = mix(");
						ecrisEntree(*n, "to_min", "float", nullptr);
						s.Append(", ");
						ecrisEntree(*n, "to_max", "float", nullptr);
						s.Append(", ");
						detail::PutNom(s, n->id, "t");
						s.Append(");\n");
					} else if (t == NkString(NK_MN_CLAMP)) {
						// ⚠️ `clamp(v, min, max)` est INDEFINI en GLSL quand
						// min > max, et chaque pilote choisit sa reponse. On passe
						// donc par min/max explicites : le resultat est le meme
						// quand les bornes sont dans l'ordre, et il est DEFINI
						// quand elles ne le sont pas.
						s.Append("    float ");
						detail::PutNom(s, n->id, "result");
						s.Append(" = min(max((");
						ecrisEntree(*n, "value", "float", nullptr);
						s.Append("), (");
						ecrisEntree(*n, "min", "float", nullptr);
						s.Append(")), (");
						ecrisEntree(*n, "max", "float", nullptr);
						s.Append("));\n");
					} else if (t == NkString(NK_MN_SELECT) || t == NkString(NK_MN_SELECT_COLOR)) {
						// ── LA CONDITION DE VALEUR ───────────────────────────
						// Deux entrees, une condition, une sortie. Voir la note
						// longue de NkMatGraphTypes.h pour ce qu'un branchement
						// coute VRAIMENT, et pour ce que ce noeud n'economise pas.
						//
						// ⚠️ ON EMET UN VRAI `if/else`, ET PAS `mix(b, a, step(...))`.
						// Les deux donnent le meme pixel. Le `mix` calcule TOUJOURS
						// les deux cotes ; l'`if` laisse au pilote la possibilite de
						// n'en prendre qu'un quand la condition est UNIFORME -- ce
						// qui est precisement le cas d'usage vise. Ecrire le `mix`
						// fermerait cette porte pour toujours, et personne ne
						// reviendrait dessus : ca « marche ».
						//
						// ⚠️ ET LA LOCALE EST DECLAREE PUIS AFFECTEE, en deux temps.
						// Une declaration a l'interieur des accolades ne survivrait
						// pas a la branche, et les noeuds suivants liraient un nom
						// qui n'existe plus -- une erreur du backend qui accuserait
						// le generateur.
						const bool selCouleur = (t == NkString(NK_MN_SELECT_COLOR));
						const char *ty = selCouleur ? "vec3" : "float";
						const char *sortie = selCouleur ? "couleur" : "valeur";
						s.Append("    ");
						s.Append(ty);
						s.Append(" ");
						detail::PutNom(s, n->id, sortie);
						s.Append(" = ");
						s.Append(selCouleur ? "vec3(0.0)" : "0.0");
						s.Append(";\n");
						// Le seuil vient de la constante, jamais d'un litteral
						// recopie : deux seuils qui divergeraient donneraient un
						// noeud qui bascule ailleurs que ce que sa note annonce.
						s.Append("    if ((");
						ecrisEntree(*n, "condition", "float", nullptr);
						s.Append(") > ");
						detail::PutLit(s, NK_SELECT_SEUIL);
						s.Append(") {\n        ");
						detail::PutNom(s, n->id, sortie);
						s.Append(" = (");
						ecrisEntree(*n, "si_vrai", ty, nullptr);
						s.Append(");\n    } else {\n        ");
						detail::PutNom(s, n->id, sortie);
						s.Append(" = (");
						ecrisEntree(*n, "si_faux", ty, nullptr);
						s.Append(");\n    }\n");
					} else if (t == NkString(NK_MN_COMBINE_XYZ)) {
						s.Append("    vec3 ");
						detail::PutNom(s, n->id, "vector");
						s.Append(" = vec3((");
						ecrisEntree(*n, "x", "float", nullptr);
						s.Append("), (");
						ecrisEntree(*n, "y", "float", nullptr);
						s.Append("), (");
						ecrisEntree(*n, "z", "float", nullptr);
						s.Append("));\n");
					} else if (t == NkString(NK_MN_VECTOR_MATH)) {
						// L operation, un MOT. Absente = `ajouter`. Inconnue = REFUS qui la
						// nomme, jamais un repli sur la premiere de la table : un graphe relu
						// d un fichier ecrit par une version plus recente calculerait alors
						// autre chose que ce que son auteur a vu, sans un mot.
						const NkGraphValue *pv = g.FindProp(n->id, NK_MPROP_OPERATION);
						int32 iv2 = 0;
						if (pv && pv->IsSet()) {
							iv2 = NkMatTrouveOpVecteur(pv->text.CStr());
							if (iv2 < 0) {
								r.error = NkString("operation vectorielle inconnue sur ");
								r.error.Append(n->type);
								r.error.Append(" : ");
								r.error.Append(pv->text);
								r.source = NkString("");
								return r;
							}
						}
						const NkMatOperation *ov = NkMatOpVecteurAt((uint32)iv2);
						if (!ov) {
							r.error = NkString("operation vectorielle hors table");
							r.source = NkString("");
							return r;
						}
						const char *opv = ov->cle;
						// 🔴 LA PRISE LUE DOIT S ACCORDER AVEC L OPERATION.
						//
						// Trois des douze operations rendent un SCALAIRE. La bibliotheque, elle,
						// propose les deux prises sans connaitre l operation -- elle ne peut pas
						// la connaitre, le menu se calcule avant que l auteur choisisse. Rien
						// n empeche donc de brancher `vector` apres un `longueur`.
						//
						// Le shader, lui, compilerait : la prise inutilisee porte le neutre. Et
						// c est exactement ce qui rend le cas dangereux -- l auteur lirait un
						// vecteur NUL parfaitement plausible au lieu d une erreur. On refuse, en
						// nommant l operation ET la prise, parce que « ca ne marche pas » sur un
						// graphe de trente noeuds ne se debogue pas.
						{
							const bool scalaire = NkMatOpVecteurRendUnScalaire((uint32)iv2);
							for (uint32 li = 0; li < g.LinkCount(); ++li) {
								const graph::NkLink *lk = g.LinkAt(li);
								if (!lk || lk->fromNode != n->id)
									continue;
								const int32 fs = lk->fromSocket;
								if (fs < 0 || fs >= (int32)n->sockets.Size())
									continue;
								const NkString &nomPrise = n->sockets[(uint32)fs].name;
								const bool litVecteur = (nomPrise == NkString("vector"));
								const bool litValeur = (nomPrise == NkString("value"));
								if ((scalaire && litVecteur) || (!scalaire && litValeur)) {
									r.error = NkString("prise mal accordee sur mat.math_vecteur : l operation « ");
									r.error.Append(opv);
									r.error.Append(scalaire ? " » rend un REEL, la prise « " : " » rend un VECTEUR, la prise « ");
									r.error.Append(nomPrise);
									r.error.Append(" » ne peut pas etre lue");
									r.source = NkString("");
									return r;
								}
							}
						}
						// ⚠️ LES DEUX PRISES SONT TOUJOURS DECLAREES, meme celle
						// que l'operation ne remplit pas.
						//
						// Le graphe autorise un lien vers `value` apres un
						// `normaliser`, et vers `vector` apres un `longueur` : la
						// bibliotheque ne connait pas l'operation quand elle
						// propose ses prises. Emettre seulement la prise « utile »
						// laisserait le consommateur lire une locale JAMAIS
						// DECLAREE, et l'erreur accuserait le generateur.
						//
						// La prise inutilisee recoit donc le NEUTRE de son type, et
						// la validation refuse le montage plus haut en le nommant.
						// Deux gardes valent mieux qu'une quand la premiere est une
						// erreur d'auteur : elle arrivera.
						s.Append("    vec3 ");
						detail::PutNom(s, n->id, "vector");
						s.Append(" = ");
						if (detail::OpEst(opv, "ajouter")) {
							s.Append("(");
							ecrisEntree(*n, "a", "vec3", nullptr);
							s.Append(") + (");
							ecrisEntree(*n, "b", "vec3", nullptr);
							s.Append(")");
						} else if (detail::OpEst(opv, "soustraire")) {
							s.Append("(");
							ecrisEntree(*n, "a", "vec3", nullptr);
							s.Append(") - (");
							ecrisEntree(*n, "b", "vec3", nullptr);
							s.Append(")");
						} else if (detail::OpEst(opv, "multiplier")) {
							s.Append("(");
							ecrisEntree(*n, "a", "vec3", nullptr);
							s.Append(") * (");
							ecrisEntree(*n, "b", "vec3", nullptr);
							s.Append(")");
						} else if (detail::OpEst(opv, "diviser")) {
							// Meme garde que `Math` : composante par composante, un
							// zero rend zero et non un NaN.
							s.Append("vec3(0.0)");
							s.Append(";\n");
							static const char *const kC[3] = {"x", "y", "z"};
							for (uint32 c = 0; c < 3; ++c) {
								s.Append("    ");
								detail::PutNom(s, n->id, "vector");
								s.Append(".");
								s.Append(kC[c]);
								s.Append(" = (abs((");
								ecrisEntree(*n, "b", "vec3", nullptr);
								s.Append(").");
								s.Append(kC[c]);
								s.Append(") < 1e-8) ? 0.0 : (");
								ecrisEntree(*n, "a", "vec3", nullptr);
								s.Append(").");
								s.Append(kC[c]);
								s.Append(" / (");
								ecrisEntree(*n, "b", "vec3", nullptr);
								s.Append(").");
								s.Append(kC[c]);
								s.Append(";\n");
							}
						} else if (detail::OpEst(opv, "produit_vectoriel")) {
							s.Append("cross(");
							ecrisEntree(*n, "a", "vec3", nullptr);
							s.Append(", ");
							ecrisEntree(*n, "b", "vec3", nullptr);
							s.Append(")");
						} else if (detail::OpEst(opv, "normaliser")) {
							// ⚠️ `normalize(vec3(0))` rend un NaN. La garde vaut
							// pour la meme raison que la division : le vecteur nul
							// n'est pas un cas tordu, c'est le DEFAUT d'une prise
							// jamais renseignee.
							s.Append("(length(");
							ecrisEntree(*n, "a", "vec3", nullptr);
							s.Append(") < 1e-8) ? vec3(0.0) : normalize(");
							ecrisEntree(*n, "a", "vec3", nullptr);
							s.Append(")");
						} else if (detail::OpEst(opv, "absolu")) {
							s.Append("abs(");
							ecrisEntree(*n, "a", "vec3", nullptr);
							s.Append(")");
						} else if (detail::OpEst(opv, "minimum")) {
							s.Append("min(");
							ecrisEntree(*n, "a", "vec3", nullptr);
							s.Append(", ");
							ecrisEntree(*n, "b", "vec3", nullptr);
							s.Append(")");
						} else if (detail::OpEst(opv, "maximum")) {
							s.Append("max(");
							ecrisEntree(*n, "a", "vec3", nullptr);
							s.Append(", ");
							ecrisEntree(*n, "b", "vec3", nullptr);
							s.Append(")");
						} else {
							// Les trois operations scalaires : la prise vectorielle
							// recoit le neutre, JAMAIS une valeur inventee.
							s.Append("vec3(0.0)");
						}
						if (!detail::OpEst(opv, "diviser"))
							s.Append(";\n");

						s.Append("    float ");
						detail::PutNom(s, n->id, "value");
						s.Append(" = ");
						if (detail::OpEst(opv, "produit_scalaire")) {
							s.Append("dot(");
							ecrisEntree(*n, "a", "vec3", nullptr);
							s.Append(", ");
							ecrisEntree(*n, "b", "vec3", nullptr);
							s.Append(")");
						} else if (detail::OpEst(opv, "longueur")) {
							s.Append("length(");
							ecrisEntree(*n, "a", "vec3", nullptr);
							s.Append(")");
						} else if (detail::OpEst(opv, "distance")) {
							s.Append("distance(");
							ecrisEntree(*n, "a", "vec3", nullptr);
							s.Append(", ");
							ecrisEntree(*n, "b", "vec3", nullptr);
							s.Append(")");
						} else {
							s.Append("0.0");
						}
						s.Append(";\n");
					} else if (t == NkString(NK_MN_NORMAL_MAP)) {
						// ⚠️ AUCUNE CONVERSION DE CONVENTION ICI, ET C'EST LE
						// POINT. Une carte DirectX se convertit A L'IMPORT. Le
						// shader ne sait pas d'ou vient la texture et n'a pas a le
						// savoir : le faire par pixel couterait a chaque fragment
						// et rendrait l'etat de la donnee invisible. La convention
						// est validee (plus haut) et RECOPIEE nulle part.
						s.Append("    vec3 ");
						detail::PutNom(s, n->id, "nts");
						s.Append(" = (");
						ecrisEntree(*n, "color", "vec3", nullptr);
						s.Append(") * 2.0 - 1.0;\n");
						// `strength` melange entre la normale geometrique et la
						// normale decodee, comme Blender — pas une multiplication
						// de la normale, qui la denormaliserait.
						s.Append("    ");
						detail::PutNom(s, n->id, "nts");
						s.Append(" = normalize(mix(vec3(0.0, 0.0, 1.0), ");
						detail::PutNom(s, n->id, "nts");
						s.Append(", ");
						{
							const int32 iv = n->FindSocket("strength", NkSocketDir::Input);
							const bool cable = iv >= 0 && g.IncomingOf(n->id, iv);
							const NkGraphValue *d = iv >= 0 ? &n->sockets[(uint32)iv].defaultValue : nullptr;
							if (cable || (d && d->IsSet()))
								ecrisEntree(*n, "strength", "float", nullptr);
							else
								s.Append("1.0"); // neutre : la carte s'applique en entier
						}
						s.Append("));\n");
						s.Append("    vec3 ");
						detail::PutNom(s, n->id, "normal");
						s.Append(" = normalize(nkT * ");
						detail::PutNom(s, n->id, "nts");
						s.Append(".x + nkB * ");
						detail::PutNom(s, n->id, "nts");
						s.Append(".y + nkGeomN * ");
						detail::PutNom(s, n->id, "nts");
						s.Append(".z);\n");
					} else if (t == NkString(NK_MN_BUMP)) {
						// Relief par derivees d'ecran du champ de hauteur. La
						// hauteur est un SCALAIRE quelconque du graphe ; sa pente
						// a l'ecran donne l'inclinaison de la surface.
						s.Append("    float ");
						detail::PutNom(s, n->id, "h");
						s.Append(" = ");
						ecrisEntree(*n, "height", "float", nullptr);
						s.Append(";\n");
						s.Append("    vec3 nkBdpx = dFdx(vWorldPos);\n");
						s.Append("    vec3 nkBdpy = dFdy(vWorldPos);\n");
						s.Append("    float ");
						detail::PutNom(s, n->id, "dhx");
						s.Append(" = dFdx(");
						detail::PutNom(s, n->id, "h");
						s.Append(");\n    float ");
						detail::PutNom(s, n->id, "dhy");
						s.Append(" = dFdy(");
						detail::PutNom(s, n->id, "h");
						s.Append(");\n");
						s.Append("    vec3 nkBr1 = cross(nkBdpy, nkGeomN);\n");
						s.Append("    vec3 nkBr2 = cross(nkGeomN, nkBdpx);\n");
						// ⚠️ Le determinant peut etre nul sur un triangle degenere
						// ou vu par la tranche. Une division nue produirait un NaN
						// qui contamine l'aval et change d'aspect d'un backend a
						// l'autre — meme raison que la division du noeud Math.
						s.Append("    float nkBdet = dot(nkBdpx, nkBr1);\n");
						s.Append("    vec3 ");
						detail::PutNom(s, n->id, "grad");
						s.Append(" = (nkBr1 * ");
						detail::PutNom(s, n->id, "dhx");
						s.Append(" + nkBr2 * ");
						detail::PutNom(s, n->id, "dhy");
						s.Append(") / (abs(nkBdet) < 1e-12 ? 1e-12 : abs(nkBdet));\n");
						s.Append("    vec3 ");
						detail::PutNom(s, n->id, "normal");
						s.Append(" = normalize(");
						ecrisEntree(*n, "normal", "vec3", nullptr);
						s.Append(" - (");
						{
							const int32 iv = n->FindSocket("strength", NkSocketDir::Input);
							const bool cable = iv >= 0 && g.IncomingOf(n->id, iv);
							const NkGraphValue *d = iv >= 0 ? &n->sockets[(uint32)iv].defaultValue : nullptr;
							if (cable || (d && d->IsSet()))
								ecrisEntree(*n, "strength", "float", nullptr);
							else
								s.Append("1.0");
						}
						s.Append(") * (");
						{
							const int32 iv = n->FindSocket("distance", NkSocketDir::Input);
							const bool cable = iv >= 0 && g.IncomingOf(n->id, iv);
							const NkGraphValue *d = iv >= 0 ? &n->sockets[(uint32)iv].defaultValue : nullptr;
							if (cable || (d && d->IsSet()))
								ecrisEntree(*n, "distance", "float", nullptr);
							else
								s.Append("1.0");
						}
						s.Append(") * ");
						detail::PutNom(s, n->id, "grad");
						s.Append(");\n");
					} else if (t == NkString(NK_MN_NOISE)) {
						// Le bruit de valeur a octaves, recopie de NkNoise.glsli. La
						// coordonnee par defaut est l'UV du maillage, comme pour une
						// texture : c'est le comportement DEFINI du noeud chez
						// Blender, pas un bouche-trou.
						s.Append("    float ");
						detail::PutNom(s, n->id, "fac");
						s.Append(" = NkFBM2D((");
						{
							const int32 iv = n->FindSocket("vector", NkSocketDir::Input);
							if (iv >= 0 && g.IncomingOf(n->id, iv))
								ecrisEntree(*n, "vector", "vec3", nullptr);
							else
								s.Append("vec3(vUV, 0.0)");
						}
						s.Append(").xy * ");
						{
							const int32 iv = n->FindSocket("scale", NkSocketDir::Input);
							const bool cable = iv >= 0 && g.IncomingOf(n->id, iv);
							const NkGraphValue *d = iv >= 0 ? &n->sockets[(uint32)iv].defaultValue : nullptr;
							if (cable || (d && d->IsSet()))
								ecrisEntree(*n, "scale", "float", nullptr);
							else
								s.Append("5.0");
						}
						s.Append(", int(clamp(");
						{
							const int32 iv = n->FindSocket("detail", NkSocketDir::Input);
							const bool cable = iv >= 0 && g.IncomingOf(n->id, iv);
							const NkGraphValue *d = iv >= 0 ? &n->sockets[(uint32)iv].defaultValue : nullptr;
							if (cable || (d && d->IsSet()))
								ecrisEntree(*n, "detail", "float", nullptr);
							else
								s.Append("2.0");
						}
						// LE NOMBRE D'OCTAVES EST BORNE. Il vient d'une valeur du
						// graphe, donc potentiellement d'un parametre expose : une
						// boucle dont le compte est libre peut ne pas se derouler, et
						// certains backends refusent alors le shader. Borner coute
						// deux appels ; ne pas borner coute un shader qui compile ici
						// et pas ailleurs.
						s.Append(", 1.0, 8.0)));\n");
						s.Append("    vec3 ");
						detail::PutNom(s, n->id, "color");
						s.Append(" = vec3(");
						detail::PutNom(s, n->id, "fac");
						s.Append(");\n");
					} else if (t == NkString(NK_MN_GRADIENT)) {
						const NkGraphValue *ptp = g.FindProp(n->id, NK_MPROP_TYPE);
						int32 itd = 0;
						if (ptp && ptp->IsSet()) {
							itd = NkMatTrouveTypeDegrade(ptp->text.CStr());
							if (itd < 0) {
								r.error = NkString("type de degrade inconnu sur ");
								r.error.Append(n->type);
								r.error.Append(" : ");
								r.error.Append(ptp->text);
								r.source = NkString("");
								return r;
							}
						}
						const NkMatOperation *tdg = NkMatTypeDegradeAt((uint32)itd);
						if (!tdg) {
							r.error = NkString("type de degrade hors table");
							r.source = NkString("");
							return r;
						}
						s.Append("    vec3 ");
						detail::PutNom(s, n->id, "co");
						s.Append(" = ");
						{
							const int32 iv = n->FindSocket("vector", NkSocketDir::Input);
							if (iv >= 0 && g.IncomingOf(n->id, iv))
								ecrisEntree(*n, "vector", "vec3", nullptr);
							else
								s.Append("vec3(vUV, 0.0)");
						}
						s.Append(";\n");
						s.Append("    float ");
						detail::PutNom(s, n->id, "fac");
						s.Append(" = ");
						if (detail::OpEst(tdg->cle, "lineaire")) {
							detail::PutNom(s, n->id, "co");
							s.Append(".x");
						} else if (detail::OpEst(tdg->cle, "quadratique")) {
							s.Append("max(");
							detail::PutNom(s, n->id, "co");
							s.Append(".x, 0.0) * max(");
							detail::PutNom(s, n->id, "co");
							s.Append(".x, 0.0)");
						} else if (detail::OpEst(tdg->cle, "radial")) {
							s.Append("atan(");
							detail::PutNom(s, n->id, "co");
							s.Append(".y, ");
							detail::PutNom(s, n->id, "co");
							s.Append(".x) * 0.15915494 + 0.5");
						} else {
							s.Append("max(1.0 - length(");
							detail::PutNom(s, n->id, "co");
							s.Append("), 0.0)");
						}
						s.Append(";\n");
						s.Append("    vec3 ");
						detail::PutNom(s, n->id, "color");
						s.Append(" = vec3(");
						detail::PutNom(s, n->id, "fac");
						s.Append(");\n");
					} else if (t == NkString(NK_MN_CHECKER)) {
						s.Append("    vec3 ");
						detail::PutNom(s, n->id, "co");
						s.Append(" = (");
						{
							const int32 iv = n->FindSocket("vector", NkSocketDir::Input);
							if (iv >= 0 && g.IncomingOf(n->id, iv))
								ecrisEntree(*n, "vector", "vec3", nullptr);
							else
								s.Append("vec3(vUV, 0.0)");
						}
						s.Append(") * ");
						{
							const int32 iv = n->FindSocket("scale", NkSocketDir::Input);
							const bool cable = iv >= 0 && g.IncomingOf(n->id, iv);
							const NkGraphValue *d = iv >= 0 ? &n->sockets[(uint32)iv].defaultValue : nullptr;
							if (cable || (d && d->IsSet()))
								ecrisEntree(*n, "scale", "float", nullptr);
							else
								s.Append("5.0");
						}
						s.Append(";\n");
						// Damier en TROIS dimensions, comme Blender : la somme des
						// trois parties entieres, modulo 2. En 2D la composante z vaut
						// zero et n'y change rien -- mais l'ecrire en 3D evite d'avoir
						// a le reecrire le jour ou une coordonnee d'objet arrivera.
						s.Append("    float ");
						detail::PutNom(s, n->id, "fac");
						s.Append(" = mod(floor(");
						detail::PutNom(s, n->id, "co");
						s.Append(".x) + floor(");
						detail::PutNom(s, n->id, "co");
						s.Append(".y) + floor(");
						detail::PutNom(s, n->id, "co");
						s.Append(".z), 2.0);\n");
						s.Append("    vec3 ");
						detail::PutNom(s, n->id, "color");
						s.Append(" = mix(");
						ecrisEntree(*n, "color2", "vec3", nullptr);
						s.Append(", ");
						ecrisEntree(*n, "color1", "vec3", nullptr);
						s.Append(", ");
						detail::PutNom(s, n->id, "fac");
						s.Append(");\n");
					} else if (t == NkString(NK_MN_VORONOI)) {
						s.Append("    float ");
						detail::PutNom(s, n->id, "distance");
						s.Append(" = NkVoronoiF1((");
						{
							const int32 iv = n->FindSocket("vector", NkSocketDir::Input);
							if (iv >= 0 && g.IncomingOf(n->id, iv))
								ecrisEntree(*n, "vector", "vec3", nullptr);
							else
								s.Append("vec3(vUV, 0.0)");
						}
						s.Append(").xy * ");
						{
							const int32 iv = n->FindSocket("scale", NkSocketDir::Input);
							const bool cable = iv >= 0 && g.IncomingOf(n->id, iv);
							const NkGraphValue *d = iv >= 0 ? &n->sockets[(uint32)iv].defaultValue : nullptr;
							if (cable || (d && d->IsSet()))
								ecrisEntree(*n, "scale", "float", nullptr);
							else
								s.Append("5.0");
						}
						s.Append(");\n");
						s.Append("    vec3 ");
						detail::PutNom(s, n->id, "color");
						s.Append(" = vec3(");
						detail::PutNom(s, n->id, "distance");
						s.Append(");\n");
					} else if (t == NkString(NK_MN_WAVE)) {
						const NkGraphValue *pto = g.FindProp(n->id, NK_MPROP_TYPE);
						int32 ito = 0;
						if (pto && pto->IsSet()) {
							ito = NkMatTrouveTypeOnde(pto->text.CStr());
							if (ito < 0) {
								r.error = NkString("type d onde inconnu sur ");
								r.error.Append(n->type);
								r.error.Append(" : ");
								r.error.Append(pto->text);
								r.source = NkString("");
								return r;
							}
						}
						const NkMatOperation *tod = NkMatTypeOndeAt((uint32)ito);
						if (!tod) {
							r.error = NkString("type d onde hors table");
							r.source = NkString("");
							return r;
						}
						s.Append("    vec3 ");
						detail::PutNom(s, n->id, "co");
						s.Append(" = ");
						{
							const int32 iv = n->FindSocket("vector", NkSocketDir::Input);
							if (iv >= 0 && g.IncomingOf(n->id, iv))
								ecrisEntree(*n, "vector", "vec3", nullptr);
							else
								s.Append("vec3(vUV, 0.0)");
						}
						s.Append(";\n");
						// L'onde vaut 0,5 + 0,5 sin(x . echelle . 2pi) : periode 1/echelle,
						// bornee dans [0,1] sans clamp. Un sinus brut sortirait de [0,1]
						// et un `clamp` ecraserait les creux au lieu de les rendre.
						s.Append("    float ");
						detail::PutNom(s, n->id, "fac");
						s.Append(" = 0.5 + 0.5 * sin((");
						if (detail::OpEst(tod->cle, "bandes")) {
							detail::PutNom(s, n->id, "co");
							s.Append(".x");
						} else {
							s.Append("length(");
							detail::PutNom(s, n->id, "co");
							s.Append(".xy)");
						}
						s.Append(") * ");
						{
							const int32 iv = n->FindSocket("scale", NkSocketDir::Input);
							const bool cable = iv >= 0 && g.IncomingOf(n->id, iv);
							const NkGraphValue *d = iv >= 0 ? &n->sockets[(uint32)iv].defaultValue : nullptr;
							if (cable || (d && d->IsSet()))
								ecrisEntree(*n, "scale", "float", nullptr);
							else
								s.Append("5.0");
						}
						s.Append(" * 6.28318531);\n");
						s.Append("    vec3 ");
						detail::PutNom(s, n->id, "color");
						s.Append(" = vec3(");
						detail::PutNom(s, n->id, "fac");
						s.Append(");\n");
					} else if (t == NkString(NK_MN_BRICK)) {
						s.Append("    vec2 ");
						detail::PutNom(s, n->id, "q");
						s.Append(" = ((");
						{
							const int32 iv = n->FindSocket("vector", NkSocketDir::Input);
							if (iv >= 0 && g.IncomingOf(n->id, iv))
								ecrisEntree(*n, "vector", "vec3", nullptr);
							else
								s.Append("vec3(vUV, 0.0)");
						}
						s.Append(").xy) * ");
						{
							const int32 iv = n->FindSocket("scale", NkSocketDir::Input);
							const bool cable = iv >= 0 && g.IncomingOf(n->id, iv);
							const NkGraphValue *d = iv >= 0 ? &n->sockets[(uint32)iv].defaultValue : nullptr;
							if (cable || (d && d->IsSet()))
								ecrisEntree(*n, "scale", "float", nullptr);
							else
								s.Append("5.0");
						}
						s.Append(";\n");
						// Appareillage a joints decales : une rangee sur deux glisse
						// d'une demi-brique. Sans ce decalage on obtient un quadrillage,
						// qui est un mur parfaitement plausible et faux.
						s.Append("    float ");
						detail::PutNom(s, n->id, "rangee");
						s.Append(" = floor(");
						detail::PutNom(s, n->id, "q");
						s.Append(".y);\n");
						s.Append("    float ");
						detail::PutNom(s, n->id, "dec");
						s.Append(" = mod(");
						detail::PutNom(s, n->id, "rangee");
						s.Append(", 2.0) * 0.5;\n");
						s.Append("    vec2 ");
						detail::PutNom(s, n->id, "uvb");
						s.Append(" = vec2(fract(");
						detail::PutNom(s, n->id, "q");
						s.Append(".x * 0.5 + ");
						detail::PutNom(s, n->id, "dec");
						s.Append("), fract(");
						detail::PutNom(s, n->id, "q");
						s.Append(".y));\n");
						// Le joint : une bande de 6 % sur chaque bord de la brique.
						s.Append("    float ");
						detail::PutNom(s, n->id, "brique");
						s.Append(" = step(0.06, ");
						detail::PutNom(s, n->id, "uvb");
						s.Append(".x) * step(0.06, ");
						detail::PutNom(s, n->id, "uvb");
						s.Append(".y) * step(");
						detail::PutNom(s, n->id, "uvb");
						s.Append(".x, 0.94) * step(");
						detail::PutNom(s, n->id, "uvb");
						s.Append(".y, 0.94);\n");
						// Chaque brique tire sa nuance de sa position : deux briques
						// voisines ne doivent pas avoir la meme couleur.
						s.Append("    float ");
						detail::PutNom(s, n->id, "h");
						s.Append(" = NkHash2(vec2(floor(");
						detail::PutNom(s, n->id, "q");
						s.Append(".x * 0.5 + ");
						detail::PutNom(s, n->id, "dec");
						s.Append("), ");
						detail::PutNom(s, n->id, "rangee");
						s.Append("));\n");
						s.Append("    vec3 ");
						detail::PutNom(s, n->id, "color");
						s.Append(" = mix(");
						ecrisEntree(*n, "mortar", "vec3", nullptr);
						s.Append(", mix(");
						ecrisEntree(*n, "color1", "vec3", nullptr);
						s.Append(", ");
						ecrisEntree(*n, "color2", "vec3", nullptr);
						s.Append(", ");
						detail::PutNom(s, n->id, "h");
						s.Append("), ");
						detail::PutNom(s, n->id, "brique");
						s.Append(");\n");
						s.Append("    float ");
						detail::PutNom(s, n->id, "fac");
						s.Append(" = ");
						detail::PutNom(s, n->id, "brique");
						s.Append(";\n");
					} else if (t == NkString(NK_MN_FRESNEL)) {
						// Schlick. La normale vient de la prise si elle est
						// cablee -- sinon de la geometrie : un Fresnel pose sur un
						// materiau a relief doit suivre le relief, sinon il
						// contredit visiblement la lumiere du meme pixel.
						s.Append("    vec3 ");
						detail::PutNom(s, n->id, "N");
						s.Append(" = ");
						{
							const int32 iv = n->FindSocket("normal", NkSocketDir::Input);
							if (iv >= 0 && g.IncomingOf(n->id, iv))
								ecrisEntree(*n, "normal", "vec3", nullptr);
							else
								s.Append("nkGeomN");
						}
						s.Append(";\n    float ");
						detail::PutNom(s, n->id, "f0");
						s.Append(" = pow((1.0 - ");
						ecrisEntree(*n, "ior", "float", nullptr);
						s.Append(") / max(1.0 + ");
						ecrisEntree(*n, "ior", "float", nullptr);
						s.Append(", 1e-4), 2.0);\n    float ");
						detail::PutNom(s, n->id, "fac");
						s.Append(" = ");
						detail::PutNom(s, n->id, "f0");
						s.Append(" + (1.0 - ");
						detail::PutNom(s, n->id, "f0");
						s.Append(") * pow(1.0 - clamp(dot(normalize(");
						detail::PutNom(s, n->id, "N");
						s.Append("), nkViewV), 0.0, 1.0), 5.0);\n");
					} else if (t == NkString(NK_MN_LAYER_WEIGHT)) {
						s.Append("    vec3 ");
						detail::PutNom(s, n->id, "N");
						s.Append(" = ");
						{
							const int32 iv = n->FindSocket("normal", NkSocketDir::Input);
							if (iv >= 0 && g.IncomingOf(n->id, iv))
								ecrisEntree(*n, "normal", "vec3", nullptr);
							else
								s.Append("nkGeomN");
						}
						s.Append(";\n    float ");
						detail::PutNom(s, n->id, "ndv");
						s.Append(" = clamp(dot(normalize(");
						detail::PutNom(s, n->id, "N");
						s.Append("), nkViewV), 0.0, 1.0);\n    float ");
						detail::PutNom(s, n->id, "b");
						s.Append(" = clamp(");
						ecrisEntree(*n, "blend", "float", nullptr);
						// ⚠️ BORNE A 0.9999, PAS A 1. A 1 exactement le rapport
						// 1/(1-b) diverge : le shader compilerait et rendrait un
						// inf qui se propagerait en NaN a travers tout l'aval.
						// Blender exclut la meme borne, et pour la meme raison.
						s.Append(", 0.0, 0.9999);\n    float ");
						detail::PutNom(s, n->id, "fresnel");
						s.Append(" = clamp(1.0 - pow(");
						detail::PutNom(s, n->id, "ndv");
						s.Append(", max(1.0 / max(1.0 - ");
						detail::PutNom(s, n->id, "b");
						s.Append(", 1e-4), 1e-4)), 0.0, 1.0);\n    float ");
						detail::PutNom(s, n->id, "facing");
						s.Append(" = clamp(1.0 - ");
						detail::PutNom(s, n->id, "ndv");
						s.Append(", 0.0, 1.0);\n");
					} else if (t == NkString(NK_MN_GEOMETRY)) {
						s.Append("    vec3 ");
						detail::PutNom(s, n->id, "position");
						s.Append(" = vWorldPos;\n    vec3 ");
						detail::PutNom(s, n->id, "normal");
						s.Append(" = nkGeomN;\n    vec3 ");
						detail::PutNom(s, n->id, "incoming");
						s.Append(" = nkViewV;\n");
					} else if (t == NkString(NK_MN_UV_MAP)) {
						// Le canal a DEJA ete valide par la passe de refus. Ici on
						// ne fait qu'ecrire le varying qu'il designe : deux
						// endroits qui decideraient du meme nom finiraient par ne
						// plus s'accorder.
						const NkGraphValue *pc = g.FindProp(n->id, NK_MPROP_CANAL_UV);
						const NkMatCanal *c = pc && pc->IsSet() ? NkMatTrouveCanalUV(pc->text.CStr()) : nullptr;
						s.Append("    vec3 ");
						detail::PutNom(s, n->id, "vector");
						s.Append(" = vec3(");
						// ⚠️ AUCUN REPLI PLAUSIBLE ICI -- LE SECOND FILET REFUSE.
						//
						// Le canal a DEJA ete valide par la passe de refus (~l.1322) :
						// ce point est INATTEIGNABLE tant que cette passe tient. Ce
						// qui suit est donc une SECONDE LIGNE DE DEFENSE, et sa valeur
						// a ete MESUREE au lieu d'etre supposee. Campagne rejouable :
						// `python scripts/matgraph_mutation.py M11 M12 M13 M14`.
						//
						//   etat du code             | inconnu | prop.absente | rien emis
						//   -------------------------|---------|--------------|-----------
						//   HEAD (les deux passes)   |    1    |      1       |     1
						//   M11 filet = « vUV »      |    1    |      1       |     1  <- SURVIT
						//   M12 sans passe 1         |    0    |      1       |  ** 1 **
						//   M13 sans passe 1 + vUV   |    0    |      0       |  ** 0 **
						//   M14 sans passe 1 + jeton |    0    |      0       |  ** 0 **
						//
						// CE QUE LA TABLE DIT, ET C'EST PLUS NUANCE QUE CE QUI ETAIT
						// ECRIT ICI LE 23/08 :
						//
						// 1. M11 SEULE SURVIT ENCORE. Le correctif n'a pas rendu ce
						//    site atteignable -- il ne pouvait pas : la passe 1 le
						//    couvre par construction. UNE DEFENSE REDONDANTE EST
						//    INVISIBLE A UNE MUTATION A UN SEUL DEFAUT. Elle ne se
						//    mesure qu'en COUPLE, en faisant d'abord tomber la ligne
						//    qui est devant. C'est la generalisation a retenir, et
						//    elle vaut pour tout filet pose derriere un autre.
						// 2. LE COUPLE QUI MESURE LE CORRECTIF EST M12 CONTRE M14 :
						//    meme mutation de la passe 1, seul le filet change. Refus
						//    -> « rien emis=1 » ; jeton imprononcable -> « rien emis=0 ».
						//    Le correctif du 23/08 achete donc quelque chose de reel,
						//    et c'est cette colonne qui le prouve.
						// 3. ⚠️ L'ARGUMENT QUI A MOTIVE CE LOT EST VRAI A MOITIE, ET
						//    C'EST MAINTENANT MESURE — cas
						//    `rang4/emis-credible-contre-emis-qui-echoue`, ajoute le
						//    2026-08-23 parce que le banc ne savait dire que « a-t-on
						//    emis ? » et que M13/M14 lui etaient indiscernables.
						//
						//    ✅ CE QUI EST VRAI, ET PIRE QU'ON NE LE DISAIT : un repli
						//       plausible ne produit pas une source « proche » de la
						//       legitime, il produit LA MEME, octet pour octet. Il n'y
						//       a rien a comparer — aucun controle ne peut les separer.
						//       Et le jeton imprononcable fait bien REFUSER glslang,
						//       pendant que les quatre colonnes de generation disent
						//       « gen » sur les deux : elles attestent la generation,
						//       jamais la compilation.
						//
						//    ✅ ET « EN PORTANT LE MOT » EST VRAI DEPUIS LE 2026-08-23 AU
						//       SOIR. Ca ne l'etait pas l'apres-midi meme : `NkSLCompiler.cpp`
						//       remplacait le resultat de glslang par celui du generateur GLSL,
						//       les erreurs mouraient la, et l'auteur recevait un echec MUET.
						//       Le cas EPINGLAIT cette absence ; il a rougi quand le correctif
						//       est arrive, et il exige maintenant le contraire :
						//
						//         [glslang/SPIR-V] ERROR: 0:31: 'nkCANAL_UV_NON_VALIDE'
						//                                     : undeclared identifier
						//
						//       Le mot Y EST, et l'ETAPE aussi — deux journaux se rencontrent
						//       dans ce resultat (le generateur, qui a REUSSI, et glslang, qui
						//       a refuse), et sans etiquette l'auteur ne saurait pas lequel se
						//       plaint.
						//
						//    📌 UNE LIMITATION EPINGLEE PAR UN CAS SE CORRIGE EN LE FAISANT
						//       ROUGIR. Ecrite dans un commentaire, elle aurait survecu au
						//       correctif et serait devenue fausse en silence.
						if (!c) {
							r.error = NkString("canal UV non valide a l emission (le filet du rang 4 a repris "
											   "la main : la passe de refus ne l a pas attrape)");
							r.source = NkString("");
							return r;
						}
						s.Append(c->varying);
						s.Append(", 0.0);\n");
					} else if (t == NkString(NK_MN_ATTRIBUTE)) {
						const NkGraphValue *pa = g.FindProp(n->id, NK_MPROP_ATTRIBUT);
						const NkMatCanal *c = pa && pa->IsSet() ? NkMatTrouveAttribut(pa->text.CStr()) : nullptr;
						s.Append("    vec3 ");
						detail::PutNom(s, n->id, "color");
						s.Append(" = ");
						// Meme raison qu'au-dessus : pas de repli plausible.
							// Meme raison qu au-dessus, et meme mesure : un filet
						// inatteignable ne protege rien. Il refuse.
						if (!c) {
							r.error = NkString("attribut non valide a l emission (le filet du rang 4 a repris "
											   "la main : la passe de refus ne l a pas attrape)");
							r.source = NkString("");
							return r;
						}
						s.Append(c->varying);
						s.Append(".rgb;\n");
					} else if (t == NkString(NK_MN_OUTPUT)) {
						// ── LE PUITS : ombrage puis ecriture ────────────────
						// Le modele d'eclairage est celui de LayeredV1, a
						// l'identique : un materiau engendre doit s'eclairer comme
						// les archetypes ecrits a la main, sinon deux materiaux
						// cotes a cote dans la meme scene ne se ressembleraient pas.
						s.Append("    vec3 surfAlbedo = ");
						ecrisEntree(*n, "surface", "vec3", "albedo");
						s.Append(";\n    float surfMetallic = ");
						ecrisEntree(*n, "surface", "float", "metallic");
						s.Append(";\n    float surfRoughness = ");
						ecrisEntree(*n, "surface", "float", "roughness");
						s.Append(";\n    vec3 surfEmission = ");
						ecrisEntree(*n, "surface", "vec3", "emission");
						s.Append(";\n    vec3 surfNormal = ");
						ecrisEntree(*n, "surface", "vec3", "normal");
						s.Append(";\n\n");
						// La normale vient du GRAPHE. Avant le 22/08 le puits
						// recalculait `normalize(vNormal)` ici, ce qui jetait en
						// silence tout travail de relief en amont.
						s.Append("    vec3 N3 = normalize(surfNormal);\n");
						s.Append("    vec3 L  = normalize(vec3(-0.3, 1.0, 0.4));\n");
						s.Append("    vec3 V  = normalize(uCam.camPos.xyz - vWorldPos);\n");
						s.Append("    vec3 H  = normalize(L + V);\n");
						s.Append("    float NdotL = max(dot(N3, L), 0.0);\n");
						s.Append("    float NdotH = max(dot(N3, H), 0.0);\n");
						s.Append("    float specExp = mix(128.0, 2.0, surfRoughness);\n");
						s.Append("    float spec = pow(NdotH, specExp);\n");
						s.Append("    vec3 specColor = mix(vec3(1.0), surfAlbedo, surfMetallic);\n");
						s.Append("    vec3 diffuse = surfAlbedo * (NdotL + 0.18);\n");
						s.Append("    vec3 specular = specColor * spec * (surfMetallic * 0.7 + 0.3);\n");
						s.Append("    fragColor = vec4(diffuse + specular + surfEmission, 1.0);\n");
					} else if (t == NkString(NK_MN_OUTPUT_VALUE)) {
						// ⚠️ LE SEUL NOEUD QUE L'EMETTEUR SAUTE LEGITIMEMENT, et
						// il faut dire pourquoi sous peine de voir la regle
						// ci-dessous se faire elargir « par symetrie ».
						//
						// La regle generale — REFUSER un noeud inconnu plutot que
						// le sauter — protege contre un consommateur qui lirait
						// une locale jamais declaree. Ici il n'y a AUCUN
						// consommateur possible : ce noeud est un PUITS, il n'a
						// pas une seule prise de sortie, donc rien en aval ne peut
						// attendre une locale de sa part.
						//
						// Et il ne produit deliberement aucune ligne pour (a) :
						// c'est ce qui rend cet etage quasi gratuit.
						//
						// (b1) EST ARRIVE, et la ligne annoncee est ci-dessous —
						// pour lui SEULEMENT. Une sortie « par_materiau » qui
						// ecrirait ici couterait une ecriture par pixel pour une
						// valeur constante.
						if (n->id == sortiePixel) {
							const int32 ivp = n->FindSocket("value", NkSocketDir::Input);
							const bool parValeur = ivp >= 0 && g.IncomingOf(n->id, ivp) != nullptr;
							s.Append("    fragAux = vec4(");
							if (parValeur) {
								// Un reel est REPLIQUE sur les trois canaux. Le
								// lecteur en prend un ; les trois sont d'accord.
								s.Append("vec3(");
								ecrisEntree(*n, "value", "float", nullptr);
								s.Append(")");
							} else {
								ecrisEntree(*n, "color", "vec3", nullptr);
							}
							// A = 1 : CE PIXEL PORTE LA VALEUR.
							s.Append(", 1.0);\n");
							auxEcrit = true;
						}
					} else {
						// ⚠️ ON REFUSE, on n'ignore pas. Un noeud inconnu qu'on
						// sauterait laisserait son consommateur lire une locale
						// jamais declaree — le shader ne compilerait pas, et
						// l'erreur accuserait le generateur au lieu du graphe.
						r.error = NkString("noeud non compilable : ");
						r.error.Append(n->type);
						r.source = NkString("");
						return r;
					}
				}

				// ⚠️ CE QU'ECRIT UN MATERIAU QUI NE PORTE PAS LA SORTIE, et c'est
				// la moitie du contrat de (b1). `(0,0,0,0)` : A = 0 dit « je ne
				// porte pas cette sortie », et RGB n'a alors aucun sens.
				//
				// Ce n'est PAS une precaution decorative : sans cette ligne, la
				// seconde cible resterait INDEFINIE sur tous les pixels couverts
				// par ce materiau. Elle contiendrait des valeurs stables,
				// credibles, et fausses.
				if (!auxEcrit)
					s.Append("    fragAux = vec4(0.0, 0.0, 0.0, 0.0);\n");

				s.Append("}\n");

				// ⚠️ LE JETON. Il est calcule sur la SOURCE EMISE, donc il change
				// des que la disposition change — et aussi quand seul le code
				// change, ce qui est le bon sens de l'erreur : un cache jete pour
				// rien coute une ecriture, un cache garde a tort ecrit dans le
				// mauvais parametre. FNV-1a, parce qu'on veut un identifiant
				// stable et bon marche, pas une garantie cryptographique.
				{
					uint64 h = 1469598103934665603ull;
					const char *p = r.source.CStr();
					while (p && *p) {
						h ^= (uint64)(unsigned char)*p++;
						h *= 1099511628211ull;
					}
					r.jeton = h;
				}
				// ── (a) LA PASSE DES SORTIES NOMMEES ────────────────────────
				//
				// Placee APRES l'emission : elle ne touche pas au shader. Une
				// sortie d'etage (a) ne produit AUCUNE ligne de NkSL — c'est
				// exactement ce qui la rend quasi gratuite. Si un jour une ligne
				// apparaissait ici, l'argument de cout serait perdu et il faudrait
				// le redire, pas le supposer.
				{
					detail::NkMatContagion contagion;
					detail::CalculeContagion(reg, g, ordre, contagion);

					for (uint32 i = 0; i < (uint32)ordre.Size(); ++i) {
						const NkNode *n = g.Find(ordre[i]);
						if (!n || !(n->type == NkString(NK_MN_OUTPUT_VALUE)))
							continue;

						auto refuse = [&](const char *quoi) {
							r.error = NkString("sortie nommee : ");
							r.error.Append(quoi);
							r.source = NkString("");
							r.sorties.Clear();
						};

						NkMatSortieMateriau so;
						so.noeud = n->id;

						// 1. Le nom. Meme grammaire qu'un parametre expose, et
						// pour la meme raison : c'est une cle que du code de jeu
						// ecrira, pas un libelle d'interface.
						const NkGraphValue *pn = g.FindProp(n->id, NK_MPROP_SORTIE_NOM);
						if (!pn || !pn->IsSet() || !NkMatNomPublicValide(pn->text.CStr())) {
							refuse("nom absent ou invalide (lettres, chiffres et souligne, ne commencant pas "
								   "par un chiffre)");
							return r;
						}
						so.nom = pn->text;
						for (uint32 k = 0; k < (uint32)r.sorties.Size(); ++k) {
							if (r.sorties[k].nom == so.nom) {
								// Deux sorties du meme nom : la seconde masquerait
								// la premiere dans toute recherche par nom, et le
								// code de jeu lirait une valeur sans savoir
								// laquelle des deux il a obtenue.
								NkString m("deux sorties portent le nom « ");
								m.Append(so.nom);
								m.Append(" »");
								refuse(m.CStr());
								return r;
							}
						}

						// 2. L'etage. AUCUN DEFAUT.
						//
						// ⚠️ Se replier sur (a) quand la propriete manque serait le
						// repli plausible qu'on refuse partout ailleurs : une
						// sortie voulue par pixel deviendrait silencieusement une
						// constante calculee une fois, et elle rendrait une valeur
						// parfaitement credible pour toujours.
						const NkGraphValue *pe = g.FindProp(n->id, NK_MPROP_SORTIE_ETAGE);
						if (!pe || !pe->IsSet() || pe->text.Size() == 0) {
							refuse("etage non renseigne (choisir « par_materiau », « par_pixel_cible » ou "
								   "« par_pixel_processeur » — il n'y a pas de defaut)");
							return r;
						}
						const detail::NkMatEtageSortie *et = NkMatTrouveEtageSortie(pe->text.CStr());
						if (!et) {
							NkString m("etage inconnu « ");
							m.Append(pe->text);
							m.Append(" »");
							refuse(m.CStr());
							return r;
						}
						if (!et->implemente) {
							// Refuse EN SE NOMMANT, avec la raison. Un « non
							// supporte » sec laisserait croire a un oubli ; ici
							// l'auteur apprend ce qui manque et pourquoi.
							NkString m("etage « ");
							m.Append(et->cle);
							m.Append(" » indisponible — ");
							m.Append(et->pourquoiPas);
							refuse(m.CStr());
							return r;
						}
						so.etage = pe->text;

						// 3. Exactement une prise alimentee.
						const int32 iv = n->FindSocket("value", NkSocketDir::Input);
						const int32 ic = n->FindSocket("color", NkSocketDir::Input);
						const graph::NkLink *lv = iv >= 0 ? g.IncomingOf(n->id, iv) : nullptr;
						const graph::NkLink *lc = ic >= 0 ? g.IncomingOf(n->id, ic) : nullptr;
						if (!lv && !lc) {
							NkString m("« ");
							m.Append(so.nom);
							m.Append(" » n'a aucune source : ni « value » ni « color » n'est connectee");
							refuse(m.CStr());
							return r;
						}
						if (lv && lc) {
							NkString m("« ");
							m.Append(so.nom);
							m.Append(" » a DEUX sources : « value » et « color » sont connectees toutes les "
									 "deux, et rien ne dit laquelle rendre");
							refuse(m.CStr());
							return r;
						}

						// 4. 🔴 LE REFUS QUI JUSTIFIE TOUTE LA PASSE.
						//
						// Une sortie « par materiau » promet UNE valeur pour tout
						// le materiau. Si son calcul descend jusqu'a une source
						// par pixel, cette promesse est fausse : on rendrait la
						// valeur d'un pixel arbitraire. Elle serait plausible —
						// un reel entre 0 et 1, une couleur credible — et
						// personne ne verrait jamais qu'elle ne veut rien dire.
						//
						// On NOMME le noeud coupable. « depend du pixel » tout
						// court laisserait l'auteur fouiller son graphe entier.
						// ⚠️ ET CE REFUS NE VAUT QUE POUR « par_materiau ». Avant
						// (b1) il n'y avait qu'un etage implemente, si bien que la
						// condition etait implicite -- et elle serait devenue
						// FAUSSE en silence : une sortie « par_pixel_cible » est
						// par pixel PAR DEFINITION, la refuser pour cette raison
						// reviendrait a refuser l'etage entier. Le message le
						// disait d'ailleurs deja (« est declaree par_materiau »),
						// mais le code ne le verifiait pas.
						const bool parMateriau = so.etage == NkString("par_materiau");
						const graph::NkLink *l = lv ? lv : lc;
						if (parMateriau && contagion.Lit(l->fromNode)) {
							NkVector<NkNodeId> vus;
							const NkNodeId src = detail::TrouveSourceParPixel(reg, g, l->fromNode, contagion, vus);
							const NkNode *sn = src != NK_NODE_INVALID ? g.Find(src) : nullptr;
							NkString m("« ");
							m.Append(so.nom);
							m.Append(" » est declaree « par_materiau » mais son calcul depend du pixel");
							if (sn) {
								m.Append(" : il remonte jusqu'a ");
								m.Append(sn->type);
							}
							m.Append(". Une valeur par materiau ne peut pas dependre d'une coordonnee, d'une "
									 "texture ni d'une derivee");
							refuse(m.CStr());
							return r;
						}

						// 5. L'evaluation -- COTE PROCESSEUR, donc pour (a) SEUL.
						//
						// Une sortie (b1) n'a rien a evaluer ici : sa valeur naitra
						// dans le shader. On enregistre donc ce qu'on sait d'elle
						// -- son nom, son etage, son nombre de composantes -- et on
						// dit explicitement que la valeur processeur est ABSENTE,
						// plutot que de laisser trainer un zero qui se lirait comme
						// une mesure.
						if (!parMateriau) {
							so.composantes = lv ? 1u : 3u;
							so.valeurConnue = false;
							r.sorties.PushBack(so);
							continue;
						}

						detail::NkMatEvalCPU ev;
						ev.g = &g;
						ev.t = t;
						ev.dependDe = &so.dependDe;
						detail::NkMatValeurCPU val;
						const uint32 comp = lv ? 1u : 3u;
						if (!ev.Sortie(l->fromNode, l->fromSocket, val)) {
							NkString m("« ");
							m.Append(so.nom);
							m.Append(" » : ");
							m.Append(ev.erreur);
							refuse(m.CStr());
							return r;
						}
						detail::NkMatEvalCPU::Adapte(val, comp, val);
						so.composantes = comp;
						for (uint32 k = 0; k < 3; ++k)
							so.valeur[k] = val.v[k];
						so.valeurConnue = true; // (a) : elle vient d'etre calculee
						r.sorties.PushBack(so);
					}
				}

				r.ok = true;

				return r;
			}

		} // namespace matgraph
	} // namespace renderer
} // namespace nkentseu
