#pragma once
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Applications/NK3DModeler/src/NK3DModeler/Shell/NkModelerCreation.h
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
//
// =============================================================================
//  CREER, ET NON PLUS SEULEMENT EDITER — une phrase devient un objet en parties
// =============================================================================
//  Ce que Rodolf a vu le 21/09 a 04 h : « modelise une chaise en banbou » ->
//  « "aucune" n'est pas un verbe du contrat ». Le refus etait JUSTE : les 26
//  verbes du contrat sont tous des verbes d'edition ou de vue, aucun ne cree.
//  Le contrat etait trop pauvre pour la demande. Ce fichier lui donne la moitie
//  qui manquait.
//
//  LE DOCUMENT, PAS UN NOUVEAU LANGAGE. Le format `.nkscene`
//  (Tools/Genia/FORMAT_SCENE.md) existe depuis le 18/09 : des lignes
//  `partie <nom> forme <f> taille <sx> <sy> <sz> pose_sur <autre> ...`, lisibles
//  en dix secondes, corrigeables dans n'importe quel editeur, et deja mesurees
//  sur ce modele local. On le LIT ici, dans le modeleur, au lieu d'en inventer un
//  second : deux grammaires pour la meme chose auraient diverge au premier mot.
//  Ce que le modeleur y ajoute, et seulement ca : `matiere`, `couleur`, les
//  unites en METRES, et une vraie primitive par partie (au lieu d'un champ
//  implicite fondu) -- donc un objet EDITABLE, partie par partie.
//
//  CE QUE LE MODELE FAIT, ET CE QUE L'OUTIL FAIT. Le modele PLANIFIE : il nomme
//  les parties, choisit leur forme, leur taille et QUI repose sur QUI. L'outil
//  POSE : il lit l'etendue REELLE de chaque primitive (Demo3DHostNodeBounds), en
//  tire l'echelle, resout `pose_sur` / `pose_sous` sur les boites MONDE, puis
//  pose l'ensemble au sol, sous le curseur. C'est la consigne de Rodolf -- « les
//  outils qu'on developpe doivent faire qu'il soit performant » -- appliquee a
//  la lettre : un modele 7B sait qu'une assise est sur quatre pieds, il ne sait
//  pas calculer 0,42 + 0,025.
//  ⚠️ ET LA PART DE CHACUN SE MESURE : `NK_CREA_SANS_POSE=1` retire la pose au
//     sol (la MUTATION), dans le meme binaire. Un objet qui ne tient au sol que
//     grace a l'outil doit se voir comme tel.
//
//  LA BOUCLE. Planifier -> poser -> VERIFIER -> corriger, et la condition
//  d'arret appartient a l'application, pas au modele : si des lignes sont
//  refusees ou si des parties flottent, on renvoie au modele SON document et les
//  motifs NOMMES, au plus `NK_CREA_TOURS` fois (defaut 2). Apres, on pose ce qui
//  est valide et on DIT ce qui ne l'est pas.
//
//  L'ANNULATION, EN MODE OBJET AUSSI. Tout ce que l'IA cree forme UN LOT ; un
//  geste « annuler » (Ctrl+Z, le bouton, le verbe `undo`) retire le lot entier,
//  « refaire » le repose depuis son document. La pile d'edition du maillage n'est
//  pas touchee : hors du mode Edition elle etait muette, c'est ici qu'on parle.
// -----------------------------------------------------------------------------

#include "NK3DModeler/Shell/NkModelerIA.h"		// NKConverse, contrat, NkModelerState
#include "NK3DModeler/Shell/NkModelerAiPanel.h" // NkAiPousser, NkAiCopie
#include "NK3DModeler/Shell/NkModelerCommon.h"	// NkMatUniqueName
#include "NK3DModeler/Shell/NkModelerScreens.h" // NkMarkDirty
#include "NK3DModeler/Shell/NkModelerVertexColor.h" // (Q15) les couleurs de sommet de TripoSR
#include "NK3DModeler/Genia/NkGeniaImport.h"	 // voie (b) : la vue rendue devient l'entree de TripoSR
#include "NK3DModeler/Viewport/NkDemo3DHost.h"
#include "NK3DModeler/Viewport/NkCreaFamilles.h" // (Q8) les constructeurs par famille
#include "NKRenderer/Mesh/NkMeshFamilles.h"   // (25/09) les dimensions plausibles
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace nk3d {

		// =====================================================================
		//  1. LES DONNEES DU CONTRAT DE CREATION — une seule autorite
		// =====================================================================
		// Lue par le lecteur de documents, par l'invite donnee au modele et par
		// l'imprimeur du contrat. Une forme ajoutee ici est reconnue, enseignee
		// et documentee du meme geste.
		struct NkCreaForme {
				const char *nom;
				int32 kind; ///< nature de Demo3DHostAddNode
				int32 sub;	///< variante
				const char *effet;
		};
		inline const NkCreaForme *NkCreaFormes(int32 &n) {
			static const NkCreaForme kF[] = {
				{"cube", 2, 0, "pave droit : planche, plateau, mur, boite"},
				{"cylindre", 2, 1, "cylindre debout : pied, tronc, barreau, disque plat"},
				{"cone", 2, 2, "cone pointe en haut : toit rond, abat-jour, sapin"},
				{"sphere", 1, 0, "sphere ou ellipsoide : tete, boule, feuillage"},
				{"tore", 1, 2, "anneau couche : roue, bouee, anse"},
				{"capsule", 1, 3, "cylindre aux bouts arrondis : bras, jambe, doigt"},
				{"plan", 3, 0, "plan horizontal sans epaisseur : sol, tapis"},
				// (21/09, Q7) kind -1 : pas une primitive du catalogue, un PROFIL
				// tourne par NkEditMesh::SpinSelected (Demo3DHostCreateRevolution).
				{"revolution", -1, 0,
				 "objet TOURNE autour de l'axe vertical : verre, bouteille, vase, bol, tasse, colonne, pied "
				 "tourne (profil + paroi, voir plus bas)"},
			};
			n = (int32)(sizeof(kF) / sizeof(kF[0]));
			return kF;
		}

		/// `NK_CREA_SANS_REVOLUTION=1` : la MUTATION du Q7 -- la forme revolution
		/// disparait de l'invite et le lecteur la refuse. C'est l'« avant » mesure
		/// dans le meme binaire.
		inline bool NkCreaSansRevolution() {
			const char *v = std::getenv("NK_CREA_SANS_REVOLUTION");
			return v && v[0] && v[0] != '0';
		}
		/// LES SILHOUETTES DE REVOLUTION (21/09, Q7, seconde course). La premiere
		/// course a montre le modele RECOPIER les nombres de l'exemple (la bouteille)
		/// pour le verre, le vase ET le bol : trois objets, un seul profil. Un 7B
		/// sait donner une largeur et une hauteur credibles -- il l'a fait pour
		/// toutes les autres parties --, il ne sait pas dessiner un profil point par
		/// point. L'outil lui offre donc des SILHOUETTES NORMALISEES (u = rayon / demi-
		/// largeur, v = hauteur / hauteur totale, du bas vers le haut), mises a
		/// l'echelle par largeur et hauteur.
		/// ⚠️ NOMMEES PAR LEUR FORME, JAMAIS PAR UN OBJET : « evase », pas « verre ».
		///    Des silhouettes nommees « verre » et « bol » donneraient la reponse du
		///    jeu d'epreuve ; ici le modele doit encore CHOISIR la forme.
		struct NkCreaSilhouette {
				const char *nom;
				const char *effet;
				float32 uv[16]; // jusqu'a 8 couples (u, v)
				int32 n;
		};
		inline const NkCreaSilhouette *NkCreaSilhouettes(int32 &n) {
			static const NkCreaSilhouette kS[] = {
				{"droit", "flancs verticaux", {1.f, 0.f, 1.f, 1.f}, 2},
				{"evase", "s'elargit vers le haut", {0.72f, 0.f, 0.78f, 0.08f, 1.f, 1.f}, 3},
				{"ventru", "renfle au milieu, col plus etroit", {0.6f, 0.f, 0.9f, 0.2f, 1.f, 0.45f, 0.85f, 0.72f, 0.5f, 0.9f, 0.58f, 1.f}, 6},
				{"goulot", "corps droit puis long col etroit", {1.f, 0.f, 1.f, 0.62f, 0.75f, 0.73f, 0.35f, 0.83f, 0.3f, 1.f}, 5},
				{"coupe", "petit pied puis flancs arrondis, large ouverture", {0.45f, 0.f, 0.72f, 0.15f, 0.9f, 0.45f, 1.f, 1.f}, 4},
				{"dome", "arrondi, se referme en haut", {1.f, 0.f, 0.96f, 0.3f, 0.82f, 0.6f, 0.5f, 0.86f, 0.05f, 1.f}, 5},
				{"conique", "se retrecit en pointe", {1.f, 0.f, 0.05f, 1.f}, 2},
				{"colonne", "base et chapiteau plus larges qu'un fut droit", {1.f, 0.f, 1.f, 0.06f, 0.78f, 0.1f, 0.78f, 0.9f, 1.f, 0.94f, 1.f, 1.f}, 6},
			};
			n = (int32)(sizeof(kS) / sizeof(kS[0]));
			return kS;
		}

		/// ── UN RECIPIENT EST OUVERT EN HAUT, ET C'EST NOTRE CODE QUI LE SAIT ──
		/// Mesure du 22/09 : une fois l'effet de chaque silhouette donne au modele, il
		/// a cesse d'ecrire `silhouette vase` -- mais il a repondu `dome` pour un vase
		/// a fleurs, et l'application a rendu un OBUS ferme en haut. Deux tentatives
		/// de le dire dans l'invite ont ete MESUREES ET REFUTEES (voir
		/// `Tools/Genia/mesure_formulaire.py`, variantes APRES2 et APRES3 : nommer les
		/// objets a fait revenir le mot `vase`, 9/9 -> 6/9 sur les valeurs permises).
		/// La regle appartient donc a l'outil : un objet CREUX (`paroi > 0`) ne peut
		/// pas porter une silhouette qui se referme. Le rayon du haut vient de la
		/// TABLE elle-meme -- rien n'est recopie, une silhouette ajoutee est jugee
		/// sans qu'on touche a ce code.
		inline float32 NkCreaSilhouetteRayonHaut(const NkCreaSilhouette &S) {
			return S.n > 0 ? S.uv[2 * (S.n - 1)] : 1.f;
		}
		inline bool NkCreaSilhouetteOuverte(const NkCreaSilhouette &S) {
			return NkCreaSilhouetteRayonHaut(S) >= 0.25f;
		}
		/// Le rayon de `S` a la hauteur `v`, par interpolation sur son profil.
		inline float32 NkCreaSilhouetteRayonA(const NkCreaSilhouette &S, float32 v) {
			if (S.n <= 0)
				return 1.f;
			for (int32 k = 1; k < S.n; ++k) {
				const float32 v0 = S.uv[2 * (k - 1) + 1], v1 = S.uv[2 * k + 1];
				if (v <= v1 + 1e-6f) {
					const float32 d = v1 - v0;
					const float32 t = d > 1e-6f ? (v - v0) / d : 0.f;
					return S.uv[2 * (k - 1)] + t * (S.uv[2 * k] - S.uv[2 * (k - 1)]);
				}
			}
			return S.uv[2 * (S.n - 1)];
		}
		/// L'OUVERTE LA PLUS PROCHE de `k`, au sens de l'ecart de profil (L2 sur 16
		/// hauteurs). ⚠️ PAS le defaut nomme du champ : retomber sur `droit` rendrait
		/// un tube pour toute forme fermee, alors qu'une forme fermee RESSEMBLE a
		/// quelque chose, et c'est cette ressemblance qu'on garde.
		inline int32 NkCreaSilhouetteOuvrir(int32 k) {
			int32 n = 0;
			const NkCreaSilhouette *S = NkCreaSilhouettes(n);
			if (k < 0 || k >= n)
				return k;
			int32 meilleur = -1;
			float32 best = 0.f;
			for (int32 j = 0; j < n; ++j) {
				if (j == k || !NkCreaSilhouetteOuverte(S[j]))
					continue;
				float32 e = 0.f;
				for (int32 t = 0; t < 16; ++t) {
					const float32 v = (float32)t / 15.f;
					const float32 d = NkCreaSilhouetteRayonA(S[j], v) - NkCreaSilhouetteRayonA(S[k], v);
					e += d * d;
				}
				if (meilleur < 0 || e < best) {
					best = e;
					meilleur = j;
				}
			}
			return meilleur < 0 ? k : meilleur;
		}

		/// ── LE REGISTRE DES LIAISONS (25/09) ──────────────────────────────────
		/// ⚠️ LA LIAISON EST UNE DONNEE, PAS UN PARENTAGE DE SCENE, et deux mesures
		///    l'imposent. (1) `Demo3DHostSetNodeOrigin` RECULE LES ENFANTS : la porte
		///    passait de 2,40 a 4,46 m avec quatre pieces flottantes. (2) Toutes les
		///    pieces d'une famille sont ensuite reparentees au GROUPE du lot, qui
		///    porte l'annulation -- un parentage battant->montant y serait ecrase
		///    dix lignes plus bas. Garder la liaison a cote satisfait aussi ce que
		///    Rodolf demande : « une partie reste selectionnable, cassable, avec sa
		///    propre matiere ».
		struct NkArticulation {
				int32 noeud = -1;
				int32 parent = -1;
				int32 liaison = 0;
				float32 pivot[3] = {0.f, 0.f, 0.f}; ///< point de l'axe, en MONDE
				float32 axe[3] = {0.f, 1.f, 0.f};
				float32 butee[2] = {0.f, 0.f};
		};
		inline NkVector<NkArticulation> &NkArticTable() {
			static NkVector<NkArticulation> t;
			return t;
		}
		inline const NkArticulation *NkArticDe(int32 noeud) {
			NkVector<NkArticulation> &t = NkArticTable();
			for (usize i = 0; i < t.Size(); ++i)
				if (t[i].noeud == noeud)
					return &t[i];
			return nullptr;
		}

		/// TOURNER UNE PIECE AUTOUR DE SON AXE, SANS DEPLACER SON PIVOT.
		/// ⚠️ C'EST LA COMPOSITION QUI FAIT LE PIVOT, PAS UNE ORIGINE. Les
		///    transformations de ce systeme sont ABSOLUES et `SetModelTransform`
		///    emmene deja les descendants : pour tourner autour de P, il suffit de
		///    poser `pos' = P + R*(pos - P)`. Rien n'est recule, rien n'est a
		///    compenser, et aucun mecanisme neuf n'est ecrit.
		inline bool NkArticTourner(int32 noeud, float32 degres) {
			const NkArticulation *a = NkArticDe(noeud);
			if (!a || a->liaison != 1)
				return false;
			float32 pos[3], rot[3], scl[3];
			if (!demo::Demo3DHostEmptyTransform(noeud, pos, rot, scl))
				return false;
			// L'axe des familles est vertical (charniere) ou normal a la face
			// (poignee) : on ne traite ici que la rotation autour de Y, et on le DIT
			// plutot que de faire semblant de gerer un axe quelconque.
			if (fabsf(a->axe[1]) < 0.9f)
				return false;
			const float32 t = degres * 0.017453292f;
			const float32 c = cosf(t), sn = sinf(t);
			const float32 dx = pos[0] - a->pivot[0], dz = pos[2] - a->pivot[2];
			const float32 np3[3] = {a->pivot[0] + dx * c + dz * sn, pos[1], a->pivot[2] - dx * sn + dz * c};
			const float32 nr3[3] = {rot[0], rot[1] + degres, rot[2]};
			demo::Demo3DHostSetModelTransform(noeud, np3, nr3, scl);
			demo::Demo3DHostHierarchyResync();
			return true;
		}

		/// `NK_CREA_SANS_FAMILLES=1` : la MUTATION du Q8 (l'« avant »).
		inline bool NkCreaSansFamilles() {
			const char *v = std::getenv("NK_CREA_SANS_FAMILLES");
			return v && v[0] && v[0] != '0';
		}

		/// LES MATIERES. ⚠️ CE NE SONT PAS LES PREREGLAGES DE NKRENDERER, et il faut
		/// le dire : `Materials/` porte 30 prereglages de MATCAP (un eclairage
		/// d'apercu, pas un materiau physique), et `NkVpMatTypeDefaults.h` a etabli
		/// le 22/08 que le moteur n'expose AUCUN jeu de valeurs PBR par archetype.
		/// Cette table est donc la premiere source de verite, pas une seconde ; elle
		/// se remplace le jour ou le graphe de materiaux portera ses defauts.
		/// Couleur de base (0..1), rugosite, metal -- les trois champs que
		/// `Demo3DHostProjMatSetParams` pose.
		struct NkCreaMatiere {
				const char *nom;
				float32 albedo[3];
				float32 rugosite;
				float32 metal;
		};
		inline const NkCreaMatiere *NkCreaMatieres(int32 &n) {
			static const NkCreaMatiere kM[] = {
				{"bois", {0.55f, 0.36f, 0.20f}, 0.60f, 0.f},
				{"bois_clair", {0.76f, 0.60f, 0.40f}, 0.55f, 0.f},
				{"bois_sombre", {0.30f, 0.18f, 0.10f}, 0.55f, 0.f},
				{"bambou", {0.80f, 0.68f, 0.42f}, 0.45f, 0.f},
				{"metal", {0.75f, 0.75f, 0.77f}, 0.30f, 1.f},
				{"acier_sombre", {0.35f, 0.36f, 0.38f}, 0.40f, 1.f},
				{"laiton", {0.90f, 0.70f, 0.35f}, 0.30f, 1.f},
				{"or", {1.00f, 0.78f, 0.34f}, 0.25f, 1.f},
				{"cuivre", {0.95f, 0.60f, 0.45f}, 0.30f, 1.f},
				{"pierre", {0.55f, 0.54f, 0.50f}, 0.90f, 0.f},
				{"brique", {0.62f, 0.28f, 0.20f}, 0.85f, 0.f},
				{"tuile", {0.60f, 0.25f, 0.15f}, 0.70f, 0.f},
				{"beton", {0.60f, 0.60f, 0.58f}, 0.90f, 0.f},
				{"platre", {0.90f, 0.88f, 0.84f}, 0.85f, 0.f},
				{"tissu", {0.60f, 0.55f, 0.50f}, 0.95f, 0.f},
				{"cuir", {0.40f, 0.22f, 0.12f}, 0.60f, 0.f},
				{"plastique", {0.80f, 0.80f, 0.80f}, 0.40f, 0.f},
				{"ceramique", {0.92f, 0.92f, 0.90f}, 0.20f, 0.f},
				{"verre", {0.90f, 0.95f, 1.00f}, 0.05f, 0.f},
				{"ecorce", {0.35f, 0.25f, 0.18f}, 0.90f, 0.f},
				{"feuillage", {0.25f, 0.45f, 0.18f}, 0.80f, 0.f},
				{"herbe", {0.30f, 0.55f, 0.20f}, 0.85f, 0.f},
				{"peau_claire", {0.85f, 0.66f, 0.55f}, 0.60f, 0.f},
				{"peau_moyenne", {0.62f, 0.43f, 0.31f}, 0.60f, 0.f},
				{"peau_foncee", {0.36f, 0.23f, 0.16f}, 0.60f, 0.f},
				{"cheveux_noirs", {0.06f, 0.05f, 0.05f}, 0.50f, 0.f},
				{"blanc", {0.92f, 0.92f, 0.92f}, 0.60f, 0.f},
				{"noir", {0.05f, 0.05f, 0.05f}, 0.60f, 0.f},
				{"rouge", {0.75f, 0.12f, 0.10f}, 0.50f, 0.f},
				{"bleu", {0.15f, 0.30f, 0.70f}, 0.50f, 0.f},
				{"vert", {0.20f, 0.55f, 0.25f}, 0.50f, 0.f},
				{"jaune", {0.90f, 0.78f, 0.20f}, 0.50f, 0.f},
				{"orange", {0.97f, 0.60f, 0.16f}, 0.50f, 0.f},
				// (Q8) la laque des portes et pavillons chinois
				{"laque_rouge", {0.62f, 0.08f, 0.06f}, 0.25f, 0.f},
				{"laque_noire", {0.04f, 0.04f, 0.04f}, 0.20f, 0.f},
			};
			n = (int32)(sizeof(kM) / sizeof(kM[0]));
			return kM;
		}

		// =====================================================================
		//  2. LE DOCUMENT
		// =====================================================================
		static const int32 kCreaMaxParties = 48; // 64 emplacements utilisateur, dont le groupe
		static const int32 kCreaMaxRefus = 12;

		struct NkCreaPartie {
				char nom[24] = {0};
				int32 forme = -1;
				float32 taille[3] = {0.f, 0.f, 0.f};
				float32 rot[3] = {0.f, 0.f, 0.f};
				/// 0 aucune, 1 pose_sur, 2 pose_sous, 3 aligne_sur,
				/// 4 a_gauche_de, 5 a_droite_de, 6 devant, 7 derriere
				int32 rel = 0;
				int32 relIdx = -1; ///< -2 : le SOL (`pose_sur sol`)
				/// TOUTES les relations de la ligne, dans l'ordre (course 3) : le
				/// modele ecrit naturellement « pose_sur tronc a_droite_de tronc ».
				int32 rels[4] = {0, 0, 0, 0};
				int32 relsIdx[4] = {-1, -1, -1, -1};
				int32 nRels = 0;
				bool aDim[3] = {false, false, false}; ///< largeur / hauteur / profondeur nommees
				bool aCentre = false;
				float32 centre[3] = {0.f, 0.f, 0.f};
				float32 decale[3] = {0.f, 0.f, 0.f};
				int32 matiere = -1;
				bool aCouleur = false;
				float32 couleur[3] = {0.f, 0.f, 0.f};
				// ── REVOLUTION (Q7) ──
				float32 profil[64] = {}; ///< nombres lus : rayon, hauteur, rayon, hauteur...
				int32 nNombres = 0;
				float32 paroi = 0.f; ///< > 0 : objet CREUX (verre, bol) ; le fond a la meme epaisseur
				int32 silhouette = -1; ///< indice dans NkCreaSilhouettes, ou -1 (profil explicite)
				bool tailleDonnee = false;
				// ── COHERENCE D'ECHELLE (Q7) ──
				bool exclue = false; ///< refusee avant la pose ; le motif est dans d.refus
				bool detailRevolution = false; ///< (Q8) bord arrondi et fond epais
		};

		struct NkCreaDoc {
				char scene[24] = {0};
				NkCreaPartie p[kCreaMaxParties];
				int32 n = 0;
				char refus[kCreaMaxRefus][176] = {};
				int32 nRefus = 0;
				bool impossible = false;
				/// (Q8) UNE FAMILLE CONSTRUITE PAR NOTRE CODE : le modele n'a ecrit que
				/// `famille <nom> <parametres>`. Vide = assemblage libre.
				NkFamParams famille;
				bool aFamille = false;
		};

		inline void NkCreaRefuser(NkCreaDoc &d, const char *fmt, const char *a, const char *b = "") {
			if (d.nRefus >= kCreaMaxRefus)
				return;
			snprintf(d.refus[d.nRefus++], sizeof(d.refus[0]), fmt, a, b);
		}

		/// Copie un nom en ne coupant JAMAIS un caractere UTF-8 en deux : 23 octets
		/// au plus, parce que c'est la largeur des noms de la hierarchie.
		inline void NkCreaCopieNom(char *dst, uint32 cap, const char *src) {
			uint32 n = 0;
			while (src && src[n] && n + 1u < cap)
				++n;
			while (n > 0 && ((unsigned char)src[n] & 0xC0u) == 0x80u)
				--n; // on recule au debut d'un caractere
			for (uint32 i = 0; i < n; ++i)
				dst[i] = src[i];
			dst[n] = 0;
		}

		/// Un mot : jusqu'au prochain blanc. Rend faux en fin de ligne.
		inline bool NkCreaMot(const char *&c, char *out, uint32 cap) {
			while (*c == ' ' || *c == '\t')
				++c;
			if (!*c || *c == '\n' || *c == '\r')
				return false;
			uint32 n = 0;
			while (*c && *c != ' ' && *c != '\t' && *c != '\n' && *c != '\r') {
				if (n + 1u < cap)
					out[n++] = *c;
				++c;
			}
			out[n] = 0;
			return true;
		}

		/// Un nombre, SANS locale : « 0.45 » et « 0,45 » donnent la meme valeur. Un
		/// modele francophone ecrit la virgule, et `atof` sous une locale francaise
		/// ferait l'inverse -- le piege que PowerShell nous a deja fait payer.
		inline bool NkCreaNombre(const char *s, float32 &v) {
			if (!s || !*s)
				return false;
			double signe = 1.0, ent = 0.0, frac = 0.0, div = 1.0;
			const char *c = s;
			if (*c == '-') {
				signe = -1.0;
				++c;
			} else if (*c == '+')
				++c;
			bool chiffre = false, point = false;
			for (; *c; ++c) {
				if (*c >= '0' && *c <= '9') {
					chiffre = true;
					if (point) {
						div *= 10.0;
						frac += (double)(*c - '0') / div;
					} else
						ent = ent * 10.0 + (double)(*c - '0');
				} else if ((*c == '.' || *c == ',') && !point)
					point = true;
				else
					return false; // « 0.4m », « abc » : pas un nombre, et on le dit
			}
			if (!chiffre)
				return false;
			v = (float32)(signe * (ent + frac));
			return true;
		}

		inline bool NkCreaTrois(const char *&c, float32 *v) {
			char m[32];
			for (int32 a = 0; a < 3; ++a) {
				if (!NkCreaMot(c, m, sizeof(m)) || !NkCreaNombre(m, v[a]))
					return false;
			}
			return true;
		}

		inline int32 NkCreaFormeDuNom(const char *nom) {
			int32 nf = 0;
			const NkCreaForme *F = NkCreaFormes(nf);
			for (int32 i = 0; i < nf; ++i)
				if (strcmp(F[i].nom, nom) == 0)
					return i;
			// Les synonymes que le modele ecrit reellement. Ce ne sont pas des
			// formes de plus : ils designent les MEMES primitives.
			if (strcmp(nom, "boite") == 0 || strcmp(nom, "pave") == 0)
				return 0;
			if (strcmp(nom, "cylinder") == 0)
				return 1;
			if (strcmp(nom, "sphère") == 0)
				return 3;
			return -1;
		}

		inline int32 NkCreaMatiereDuNom(const char *nom) {
			int32 nm = 0;
			const NkCreaMatiere *M = NkCreaMatieres(nm);
			for (int32 i = 0; i < nm; ++i)
				if (strcmp(M[i].nom, nom) == 0)
					return i;
			return -1;
		}

		/// LA RELATION, ET SES SYNONYMES. ⚠️ Chaque synonyme de cette table a ete
		/// ECRIT PAR LE MODELE lors des courses 1 et 2 (« pose_droite_de »,
		/// « pose_derriere »...), refuse, puis redemande deux fois sans succes. Ce
		/// ne sont pas des relations de plus : ils designent les MEMES, et la table
		/// les rend lisibles au lieu de laisser un mur flotter pour un prefixe.
		/// 0 = pas une relation.
		inline int32 NkCreaRelationDuMot(const char *m) {
			struct R {
					const char *mot;
					int32 rel;
			};
			static const R kR[] = {
				{"pose_sur", 1},	  {"sur", 1},			  {"pose_sous", 2},		 {"sous", 2},
				{"aligne_sur", 3},	  {"a_gauche_de", 4},	  {"pose_gauche_de", 4}, {"pose_a_gauche_de", 4},
				{"gauche_de", 4},	  {"a_gauche", 4},		  {"a_droite_de", 5},	 {"pose_droite_de", 5},
				{"pose_a_droite_de", 5}, {"droite_de", 5},	  {"a_droite", 5},		 {"devant", 6},
				{"devant_de", 6},	  {"pose_devant", 6},	  {"pose_devant_de", 6}, {"derriere", 7},
				{"derriere_de", 7},	  {"pose_derriere", 7},	  {"pose_derriere_de", 7},
			};
			for (const R &r : kR)
				if (strcmp(r.mot, m) == 0)
					return r.rel;
			return 0;
		}

		inline int32 NkCreaPartieDuNom(const NkCreaDoc &d, const char *nom) {
			for (int32 i = 0; i < d.n; ++i)
				if (strcmp(d.p[i].nom, nom) == 0)
					return i;
			return -1;
		}

		/// LIT LE DOCUMENT. Chaque ligne refusee l'est AVEC SON MOTIF, et la partie
		/// n'entre pas : on ne devine jamais une forme ou une taille absente.
		/// ⚠️ ON NE REPARE PAS LA SORTIE DU MODELE EN SILENCE. On tolere ce qui
		///    n'est pas du contenu (puces, blocs de code, numerotation) ; tout le
		///    reste est soit compris, soit refuse en le disant.
		inline void NkCreaLire(const char *texte, NkCreaDoc &d) {
			d = NkCreaDoc();
			if (!texte)
				return;
			const char *c = texte;
			int32 ligne = 0;
			while (*c) {
				++ligne;
				const char *l = c;
				while (*c && *c != '\n')
					++c;
				const char *finLigne = c;
				if (*c == '\n')
					++c;
				// ornements de debut de ligne
				while (l < finLigne && (*l == ' ' || *l == '\t' || *l == '-' || *l == '*' || *l == '`' ||
										*l == '>' || (*l >= '0' && *l <= '9' && (l[1] == '.' || l[1] == ')'))))
					l += (*l >= '0' && *l <= '9') ? 2 : 1;
				char buf[512];
				uint32 lg = (uint32)(finLigne - l);
				if (lg >= sizeof(buf))
					lg = (uint32)sizeof(buf) - 1u;
				memcpy(buf, l, lg);
				buf[lg] = 0;
				for (uint32 k = 0; k < lg; ++k)
					if (buf[k] == '\r' || buf[k] == '`')
						buf[k] = ' ';
				const char *q = buf;
				char mot[64];
				if (!NkCreaMot(q, mot, sizeof(mot)) || mot[0] == '#')
					continue;
				for (char *m = mot; *m; ++m)
					if (*m >= 'A' && *m <= 'Z')
						*m = (char)(*m - 'A' + 'a');
				if (strcmp(mot, "impossible") == 0) {
					d.impossible = true;
					continue;
				}
				if (strcmp(mot, "scene") == 0) {
					char v[64];
					if (NkCreaMot(q, v, sizeof(v)))
						NkCreaCopieNom(d.scene, sizeof(d.scene), v);
					continue;
				}
				if (strcmp(mot, "demande") == 0 || strcmp(mot, "lissage") == 0 || strcmp(mot, "assemblage") == 0)
					continue; // metadonnees du format : sans effet ici, avalees EXPLICITEMENT
				if (strcmp(mot, "famille") == 0 && !NkCreaSansFamilles()) {
					// ── (Q8) LA FAMILLE : un nom et quelques parametres, rien d'autre ──
					char nf[32] = {0}, cle[32], val[40];
					NkCreaMot(q, nf, sizeof(nf));
					for (char *m = nf; *m; ++m)
						if (*m >= 'A' && *m <= 'Z')
							*m = (char)(*m - 'A' + 'a');
					NkFamParams fp;
					snprintf(fp.famille, sizeof(fp.famille), "%s", nf);
					bool detaille = true;
					char silh[32] = {0};
					float32 paroi = 0.f;
					while (NkCreaMot(q, cle, sizeof(cle))) {
						if (!NkCreaMot(q, val, sizeof(val)))
							break;
						float32 x = 0.f;
						const bool num = NkCreaNombre(val, x);
						if (strcmp(cle, "largeur") == 0 && num)
							fp.largeur = x;
						else if (strcmp(cle, "hauteur") == 0 && num)
							fp.hauteur = x;
						else if (strcmp(cle, "profondeur") == 0 && num)
							fp.profondeur = x;
						else if ((strcmp(cle, "battants") == 0 || strcmp(cle, "etages") == 0 || strcmp(cle, "nombre") == 0 || strcmp(cle, "pieces") == 0) && num)
							fp.nombre = (int32)(x + 0.5f);
						else if (strcmp(cle, "fenetres") == 0 && num)
							fp.fenetres = (int32)(x + 0.5f);
						else if (strcmp(cle, "style") == 0 || strcmp(cle, "toit") == 0) {
							char rempl[24] = {0};
							if (!NkFamValeurAutorisee(nf, cle, val, rempl, sizeof(rempl))) {
								char av[120];
								snprintf(av, sizeof(av), "%s « %s » inconnu pour la famille %s", cle, val, nf);
								NkCreaRefuser(d, "%s : je prends « %s »", av, rempl);
								snprintf(fp.style, sizeof(fp.style), "%s", rempl);
							} else
								snprintf(fp.style, sizeof(fp.style), "%s", val);
						}
						else if (strcmp(cle, "detail") == 0)
							detaille = strcmp(val, "simple") != 0;
						else if (strcmp(cle, "silhouette") == 0) {
							// ⚠️ ON CORRIGE, ON NE REJETTE PAS (Q10.1). Le 21/09 le modele a
							//    ecrit `silhouette vase` -- un mot pris dans la demande, absent
							//    de la table -- et le DOCUMENT ENTIER a ete refuse : l'utilisateur
							//    n'a rien eu pour un mot. La valeur inconnue retombe maintenant
							//    sur le defaut NOMME du champ, et le refus devient un AVIS.
							char rempl[24] = {0};
							if (!NkFamValeurAutorisee(nf, "silhouette", val, rempl, sizeof(rempl))) {
								NkCreaRefuser(d, "silhouette « %s » inconnue : je prends « %s »", val, rempl);
								snprintf(silh, sizeof(silh), "%s", rempl);
							} else
								snprintf(silh, sizeof(silh), "%s", val);
						}
						else if (strcmp(cle, "paroi") == 0 && num)
							paroi = x;
						else if (strcmp(cle, "matiere") == 0)
							; // la famille choisit ses matieres
						else
							NkCreaRefuser(d, "famille « %s » : parametre inconnu « %s », ignore", nf, cle);
					}
					detaille = true; // le modele ne decide pas du niveau : voir NkCreaPoserFamille
					if (const char *dv = std::getenv("NK_CREA_DETAIL"))
						detaille = strcmp(dv, "simple") != 0; // la mesure « simple contre detaille »
					fp.detaille = detaille;
					if (strcmp(nf, "revolution") == 0 || strcmp(nf, "objet_de_revolution") == 0) {
						// LA REVOLUTION reste une PARTIE : elle a deja son chemin (profil,
						// silhouette, paroi) ; la famille ne fait que la declarer.
						// ── UN CREUX NE SE REFERME PAS (22/09) ──────────────────────────
						// `paroi > 0` dit « recipient » ; une silhouette qui se referme en
						// haut donnerait un obus etanche, pas un vase. On CORRIGE vers
						// l'ouverte la plus proche et on le DIT : un silence ici rendrait
						// l'objet inexplicable, exactement ce que Rodolf a vu.
						if (paroi > 0.f && silh[0]) {
							int32 nsv = 0;
							const NkCreaSilhouette *SV = NkCreaSilhouettes(nsv);
							for (int32 k = 0; k < nsv; ++k) {
								if (strcmp(SV[k].nom, silh) != 0 || NkCreaSilhouetteOuverte(SV[k]))
									continue;
								const int32 o = NkCreaSilhouetteOuvrir(k);
								NkCreaRefuser(d, "silhouette « %s » se referme en haut : un objet creux ne peut pas "
												 "l'etre, je prends « %s »",
											  silh, SV[o].nom);
								snprintf(silh, sizeof(silh), "%s", SV[o].nom);
								break;
							}
						}
						static char ligne[256];
						snprintf(ligne, sizeof(ligne),
								 "partie objet forme revolution silhouette %s largeur %g hauteur %g%s%g matiere %s",
								 silh[0] ? silh : "evase", (double)(fp.largeur > 0.f ? fp.largeur : 0.08f),
								 (double)(fp.hauteur > 0.f ? fp.hauteur : 0.12f), paroi > 0.f ? " paroi " : " paroi ",
								 (double)(paroi > 0.f ? paroi : 0.f), "verre");
						NkCreaDoc sous;
						NkCreaLire(ligne, sous);
						if (sous.n == 1 && d.n < kCreaMaxParties) {
							d.p[d.n] = sous.p[0];
							d.p[d.n].detailRevolution = detaille;
							NkCreaCopieNom(d.p[d.n].nom, 24, d.scene[0] ? d.scene : "objet");
							++d.n;
						} else
							for (int32 k = 0; k < sous.nRefus; ++k)
								NkCreaRefuser(d, "famille revolution : %s%s", sous.refus[k]);
						continue;
					}
					if (!NkFamConnue(nf)) {
						// LA LISTE VIENT DE LA BIBLIOTHEQUE, elle n'est plus recopiee ici :
						// une liste recopiee se perime au premier ajout de famille.
						char connues[192];
						connues[0] = 0;
						for (int32 fi = 0; const char *fn = NkFamNom(fi); ++fi) {
							if (connues[0])
								strncat(connues, ", ", sizeof(connues) - strlen(connues) - 1u);
							strncat(connues, fn, sizeof(connues) - strlen(connues) - 1u);
						}
						strncat(connues, ", revolution", sizeof(connues) - strlen(connues) - 1u);
						char av2[224];
						snprintf(av2, sizeof(av2), "famille inconnue « %s » (connues : %s)", nf, connues);
						NkCreaRefuser(d, "%s : ecris des parties%s", av2);
						continue;
					}
					d.famille = fp;
					d.aFamille = true;
					continue;
				}
				if (strcmp(mot, "partie") != 0)
					continue; // une phrase du modele : ni une partie, ni une erreur de partie
				char nom[64];
				if (!NkCreaMot(q, nom, sizeof(nom))) {
					char lb[16];
					snprintf(lb, sizeof(lb), "%d", (int)ligne);
					NkCreaRefuser(d, "ligne %s : « partie » sans nom%s", lb);
					continue;
				}
				if (d.n >= kCreaMaxParties) {
					NkCreaRefuser(d, "partie « %s » : plus de 48 parties, elle n'est pas posee%s", nom);
					continue;
				}
				NkCreaPartie pc;
				NkCreaCopieNom(pc.nom, sizeof(pc.nom), nom);
				if (NkCreaPartieDuNom(d, pc.nom) >= 0) {
					NkCreaRefuser(d, "partie « %s » : deux parties portent ce nom%s", pc.nom);
					continue;
				}
				bool ok = true, aTaille = false;
				int32 nfTmp = 0;
				char clef[64], v[64];
				v[0] = 0;
				while (ok && NkCreaMot(q, clef, sizeof(clef))) {
					if (clef[0] == '#')
						break;
					for (char *m = clef; *m; ++m)
						if (*m >= 'A' && *m <= 'Z')
							*m = (char)(*m - 'A' + 'a');
					if (strcmp(clef, "forme") == 0) {
						if (!NkCreaMot(q, v, sizeof(v)) || (pc.forme = NkCreaFormeDuNom(v)) < 0) {
							NkCreaRefuser(d, "partie « %s » : forme inconnue « %s » (cube, cylindre, cone, sphere, tore, capsule, plan)", pc.nom, v);
							ok = false;
						}
					} else if (strcmp(clef, "taille") == 0) {
						if (!NkCreaTrois(q, pc.taille) || pc.taille[0] < 0.f || pc.taille[1] < 0.f ||
							pc.taille[2] < 0.f || pc.taille[0] > 60.f || pc.taille[1] > 60.f || pc.taille[2] > 60.f) {
							NkCreaRefuser(d, "partie « %s » : « taille » attend trois nombres en metres, entre 0 et 60%s", pc.nom);
							ok = false;
						}
						aTaille = true;
					} else if (strcmp(clef, "profil") == 0) {
						// Des NOMBRES jusqu'au premier mot qui n'en est pas un, relu ensuite
						// comme la clef suivante.
						pc.nNombres = 0;
						for (;;) {
							const char *avant = q;
							char mv[32];
							float32 x = 0.f;
							if (!NkCreaMot(q, mv, sizeof(mv)) || !NkCreaNombre(mv, x)) {
								q = avant;
								break;
							}
							if (pc.nNombres < 64)
								pc.profil[pc.nNombres++] = x;
						}
						if (pc.nNombres < 4 || (pc.nNombres & 1)) {
							NkCreaRefuser(d, "partie « %s » : « profil » attend des couples rayon hauteur (au moins deux)%s", pc.nom);
							ok = false;
						} else
							aTaille = true; // la revolution tire sa taille de son profil
					} else if (strcmp(clef, "silhouette") == 0) {
						int32 ns = 0;
						const NkCreaSilhouette *S = NkCreaSilhouettes(ns);
						pc.silhouette = -1;
						if (NkCreaMot(q, v, sizeof(v))) {
							for (int32 k = 0; k < ns; ++k)
								if (strcmp(S[k].nom, v) == 0)
									pc.silhouette = k;
							// UN PREFIXE UNIQUE D'AU MOINS 4 LETTRES SUFFIT : la seconde
							// course a vu « coup » pour « coupe », repete trois tours de
							// suite malgre le motif -- et le verre n'a jamais ete pose.
							// Un prefixe AMBIGU reste refuse.
							if (pc.silhouette < 0 && strlen(v) >= 4) {
								int32 trouve = -1, nb = 0;
								for (int32 k = 0; k < ns; ++k)
									if (strncmp(S[k].nom, v, strlen(v)) == 0) {
										trouve = k;
										++nb;
									}
								if (nb == 1)
									pc.silhouette = trouve;
							}
						}
						if (pc.silhouette < 0) {
							NkCreaRefuser(d, "partie « %s » : silhouette inconnue « %s » (droit, evase, ventru, goulot, coupe, dome, conique, colonne)", pc.nom, v);
							ok = false;
						}
					} else if (strcmp(clef, "paroi") == 0) {
						if (!NkCreaMot(q, v, sizeof(v)) || !NkCreaNombre(v, pc.paroi) || pc.paroi < 0.f) {
							NkCreaRefuser(d, "partie « %s » : « paroi » attend une epaisseur en metres%s", pc.nom);
							ok = false;
						}
					} else if (strcmp(clef, "rotation") == 0) {
						if (!NkCreaTrois(q, pc.rot)) {
							NkCreaRefuser(d, "partie « %s » : « rotation » attend trois angles en degres%s", pc.nom);
							ok = false;
						}
					} else if (strcmp(clef, "centre") == 0 || strcmp(clef, "position") == 0) {
						if (!NkCreaTrois(q, pc.centre)) {
							NkCreaRefuser(d, "partie « %s » : « centre » attend trois nombres%s", pc.nom);
							ok = false;
						}
						pc.aCentre = true;
					} else if (strcmp(clef, "decale") == 0) {
						if (!NkCreaTrois(q, pc.decale)) {
							NkCreaRefuser(d, "partie « %s » : « decale » attend trois nombres%s", pc.nom);
							ok = false;
						}
					} else if (strcmp(clef, "largeur") == 0 || strcmp(clef, "hauteur") == 0 ||
							   strcmp(clef, "profondeur") == 0) {
						// ── LES DIMENSIONS NOMMEES (course 2, 21/09) ──────────────
						// La course 1 a montre le modele ecrire `taille 1.20 0.80 0.03`
						// pour un plateau de 3 cm d'EPAISSEUR : il pensait (largeur,
						// profondeur, hauteur). Trois nombres sans nom laissent l'ordre
						// a deviner ; trois nombres NOMMES ne laissent rien.
						const int32 ax = clef[0] == 'l' ? 0 : (clef[0] == 'h' ? 1 : 2);
						float32 val = 0.f;
						if (!NkCreaMot(q, v, sizeof(v)) || !NkCreaNombre(v, val) || val < 0.f || val > 60.f) {
							NkCreaRefuser(d, "partie « %s » : « %s » attend un nombre en metres, entre 0 et 60", pc.nom, clef);
							ok = false;
						} else {
							pc.taille[ax] = val;
							pc.aDim[ax] = true;
							if (pc.aDim[0] && pc.aDim[1] && pc.aDim[2]) {
								aTaille = true;
								pc.tailleDonnee = true;
							}
						}
					} else if (NkCreaRelationDuMot(clef) != 0) {
						const int32 r = NkCreaRelationDuMot(clef);
						char cible[64];
						if (!NkCreaMot(q, cible, sizeof(cible))) {
							NkCreaRefuser(d, "partie « %s » : « %s » sans nom de partie", pc.nom, clef);
							ok = false;
						} else {
							char cn[24];
							NkCreaCopieNom(cn, sizeof(cn), cible);
							const int32 idx = (r == 1 && strcmp(cn, "sol") == 0) ? -2 : NkCreaPartieDuNom(d, cn);
							float32 bidon = 0.f;
							if (idx == -1 && NkCreaNombre(cn, bidon)) {
								// « a_gauche_de 0.10 0 0 » : la relation prise pour un
								// decalage (course 2). Le motif le dit TEL QUEL, sinon le
								// modele relit « 0.10 n'est pas une partie » sans comprendre.
								NkCreaRefuser(d, "partie « %s » : « %s » attend le NOM d'une partie, pas un nombre (pour deplacer, c'est decale)", pc.nom, clef);
								ok = false;
							} else if (idx == -1) {
								NkCreaRefuser(d, "partie « %s » : elle se pose sur « %s », qui n'est pas une partie ecrite AVANT elle", pc.nom, cn);
								ok = false;
							} else {
								if (pc.nRels == 0) {
									pc.rel = r;
									pc.relIdx = idx;
								}
								if (pc.nRels < 4) {
									pc.rels[pc.nRels] = r;
									pc.relsIdx[pc.nRels] = idx;
									++pc.nRels;
								}
							}
						}
					} else if (strcmp(clef, "matiere") == 0 || strcmp(clef, "materiau") == 0 ||
							   strcmp(clef, "matière") == 0 || strcmp(clef, "matériau") == 0) {
						if (NkCreaMot(q, v, sizeof(v))) {
							for (char *m = v; *m; ++m)
								if (*m >= 'A' && *m <= 'Z')
									*m = (char)(*m - 'A' + 'a');
							pc.matiere = NkCreaMatiereDuNom(v);
							// UNE MATIERE INCONNUE N'ANNULE PAS LA PARTIE. La forme et
							// la place sont justes ; on la pose GRISE et on le dit, au
							// lieu de perdre un pied de chaise pour un mot.
							if (pc.matiere < 0)
								NkCreaRefuser(d, "partie « %s » : matiere inconnue « %s », posee sans matiere", pc.nom, v);
						}
					} else if (strcmp(clef, "couleur") == 0) {
						if (!NkCreaTrois(q, pc.couleur)) {
							NkCreaRefuser(d, "partie « %s » : « couleur » attend trois nombres entre 0 et 1%s", pc.nom);
							ok = false;
						} else {
							for (int32 a = 0; a < 3; ++a)
								if (pc.couleur[a] > 1.f)
									pc.couleur[a] /= 255.f; // 0..255 ecrit par habitude
							pc.aCouleur = true;
						}
					} else if (strcmp(clef, "op") == 0) {
						v[0] = 0;
						NkCreaMot(q, v, sizeof(v));
						if (strcmp(v, "union") != 0) {
							NkCreaRefuser(d, "partie « %s » : « op %s » n'existe pas dans le modeleur (des primitives separees ne se soustraient pas) : partie non posee", pc.nom, v);
							ok = false;
						}
					} else {
						NkCreaRefuser(d, "partie « %s » : directive inconnue « %s »", pc.nom, clef);
						ok = false;
					}
				}
				if (ok && pc.forme < 0) {
					NkCreaRefuser(d, "partie « %s » : pas de forme%s", pc.nom);
					ok = false;
				}
				if (ok && pc.forme >= 0 && NkCreaFormes(nfTmp)[pc.forme].kind == -1 && NkCreaSansRevolution()) {
					NkCreaRefuser(d, "partie « %s » : forme revolution desactivee (NK_CREA_SANS_REVOLUTION)%s", pc.nom);
					ok = false;
				}
				if (ok && pc.silhouette >= 0) {
					int32 ns = 0;
					const NkCreaSilhouette &S = NkCreaSilhouettes(ns)[pc.silhouette];
					if (!pc.aDim[0] || !pc.aDim[1]) {
						NkCreaRefuser(d, "partie « %s » : une silhouette demande largeur et hauteur%s", pc.nom);
						ok = false;
					} else {
						// Le profil EN METRES, tire de la silhouette : la suite de la
						// chaine (pose, echelle, garde) ne voit qu'un profil ordinaire.
						pc.nNombres = 0;
						for (int32 k = 0; k < S.n; ++k) {
							pc.profil[pc.nNombres++] = S.uv[2 * k] * 0.5f * pc.taille[0];
							pc.profil[pc.nNombres++] = S.uv[2 * k + 1] * pc.taille[1];
						}
						pc.tailleDonnee = false;
						aTaille = true;
					}
				}
				if (ok && pc.forme >= 0 && strcmp(NkCreaFormes(nfTmp)[pc.forme].nom, "revolution") == 0 && pc.nNombres < 4) {
					NkCreaRefuser(d, "partie « %s » : une revolution demande un « profil » (couples rayon hauteur)%s", pc.nom);
					ok = false;
				}
				if (ok && !aTaille && pc.silhouette < 0 && (pc.aDim[0] || pc.aDim[1] || pc.aDim[2])) {
					NkCreaRefuser(d, "partie « %s » : il faut les TROIS dimensions (largeur, hauteur, profondeur)%s", pc.nom);
					ok = false;
				}
				if (ok && !aTaille) {
					NkCreaRefuser(d, "partie « %s » : pas de taille%s", pc.nom);
					ok = false;
				}
				if (ok)
					d.p[d.n++] = pc;
			}
		}

		// =====================================================================
		//  3. L'INVITE DE CREATION — ecrite depuis les tables, jamais recopiee
		// =====================================================================
		/// LES GABARITS PAR FAMILLE (21/09, Q6) : des documents ECRITS A LA MAIN,
		/// lus sur le disque (Tools/Genia/gabarits/<famille>.nkscene) -- des
		/// DONNEES, que Rodolf corrige dans un editeur sans recompiler. Le modele
		/// les ADAPTE au lieu d'inventer la structure.
		/// ⚠️ ILS SONT TOUS DONNES, pas choisis par mot-cle : un choix par mot-cle
		///    ne servirait a rien sur un objet sans gabarit (le jeu neuf), et c'est
		///    justement la qu'il faut savoir s'ils aident.
		/// `NK_CREA_GABARITS=0` les retire (la MUTATION de la mesure).
		inline const char *const *NkCreaFamilles(int32 &n) {
			static const char *const kF[] = {"chaise", "table",	   "tabouret", "etagere",  "lampe",
											 "maison", "arbre",	   "personnage", "vehicule", "creature", "revolution"};
			n = (int32)(sizeof(kF) / sizeof(kF[0]));
			return kF;
		}
		inline bool NkCreaGabaritsActifs() {
			const char *v = std::getenv("NK_CREA_GABARITS");
			return !(v && v[0] == '0');
		}

		inline void NkCreaEcrireInvite(char *dst, uint32 cap, const char *demande) {
			if (!dst || cap == 0)
				return;
			dst[0] = 0;
			uint32 n = 0;
			auto ajout = [&](const char *s) {
				while (s && *s && n + 1u < cap)
					dst[n++] = *s++;
				dst[n] = 0;
			};
			ajout("Tu construis un objet 3D en PARTIES NOMMEES, avec des formes simples. Tu n'ecris QUE\n");
			ajout("les lignes du document, rien d'autre : pas d'explication, pas de texte avant ou apres.\n\n");
			if (!NkCreaSansFamilles()) {
				// (Q8) LES FAMILLES D'ABORD : pour elles, notre code construit ; le
				// modele ne donne que des parametres. C'est la reponse a la porte en 22
				// planches et a la villa en 21 boites.
				ajout("FAMILLES. Si l'objet est l'une de ces familles, ecris seulement DEUX lignes :\n");
				ajout("scene <nom_de_l_objet>\n");
				ajout("famille <famille> <parametres>\n");
				ajout("et rien d'autre : l'outil construit l'objet complet, avec ses details.\n");
				ajout("- famille porte style simple|chinois largeur <m> hauteur <m> battants 1|2 detail simple|detaille\n");
				ajout("  (porte, portail, porte d'entree, porte chinoise ou pagode)\n");
				ajout("- famille table largeur <m> profondeur <m> hauteur <m> detail simple|detaille\n");
				ajout("- famille maison style deux_pans|plat largeur <m> profondeur <m> etages <n> fenetres <n> detail simple|detaille\n");
				ajout("  (maison, villa, pavillon ; fenetres = par facade et par etage)\n");
				ajout("- famille revolution silhouette <forme> largeur <m> hauteur <m> paroi <m> detail simple|detaille\n");
				ajout("  (verre, vase, bol, bouteille, tasse ; silhouettes plus bas)\n");
				ajout("Exemple, pour « un grand portail de jardin » :\n");
				ajout("scene portail\nfamille porte style simple largeur 3.0 hauteur 2.0 battants 2 detail detaille\n\n");
				ajout("SINON, pour tout autre objet, decris-le en parties, comme suit.\n\n");
			}
			ajout("Premiere ligne : scene <nom_de_l_objet>\n");
			ajout("Puis UNE ligne par partie :\n");
			ajout("partie <nom> forme <forme> largeur <x> hauteur <y> profondeur <z> [placement] [rotation <rx> <ry> <rz>] [matiere <matiere>]\n\n");
			ajout("LES UNITES SONT DES METRES. Une porte mesure 2.0 de haut, une tasse 0.1, un immeuble 20.\n");
			ajout("Les axes : x = largeur (gauche-droite), y = hauteur (vers le haut), z = profondeur (avant-arriere).\n\n");
			ajout("LES FORMES, et il n'y en a pas d'autres :\n");
			{
				int32 nf = 0;
				const NkCreaForme *F = NkCreaFormes(nf);
				char l[160];
				for (int32 i = 0; i < nf; ++i) {
					if (F[i].kind == -1 && NkCreaSansRevolution())
						continue;
					snprintf(l, sizeof(l), "- %s : %s\n", F[i].nom, F[i].effet);
					ajout(l);
				}
			}
			ajout("\nLARGEUR, HAUTEUR, PROFONDEUR sont l'encombrement TOTAL de la partie, en metres.\n");
			ajout("La HAUTEUR est TOUJOURS la dimension verticale :\n");
			ajout("- cylindre largeur 0.05 hauteur 0.40 profondeur 0.05 : un barreau debout de 40 cm\n");
			ajout("- cube largeur 1.20 hauteur 0.03 profondeur 0.40 : une planche posee a plat\n");
			ajout("- cube largeur 1.20 hauteur 0.80 profondeur 0.02 : un panneau debout\n");
			ajout("- cylindre largeur 0.30 hauteur 0.02 profondeur 0.30 : un disque plat\n\n");
			ajout("LE PLACEMENT dit ou va la partie PAR RAPPORT A UNE PARTIE ECRITE AU-DESSUS :\n");
			ajout("- pose_sur <autre> : elle est posee SUR l'autre (son dessous touche le dessus de l'autre), centree sur elle\n");
			ajout("- pose_sous <autre> : elle est SOUS l'autre (son dessus touche le dessous de l'autre), centree sous elle\n");
			ajout("- a_gauche_de <autre>, a_droite_de <autre> : elle touche le cote gauche (ou droit) de l'autre, centree sur sa hauteur\n");
			ajout("- devant <autre>, derriere <autre> : elle touche l'avant (ou l'arriere) de l'autre, centree sur sa hauteur\n");
			ajout("- pose_sur sol : elle est posee par terre (pour une seconde partie qui touche le sol)\n");
			ajout("On peut COMBINER une relation verticale et une horizontale : pose_sur X a_droite_de X.\n");
			ajout("- decale <dx> <dy> <dz> : deplacement en metres APRES le placement (pour ecarter des pieds, par exemple).\n");
			ajout("  Avec pose_sur ou pose_sous, garde dy = 0 : sinon les deux parties ne se touchent plus.\n");
			ajout("- centre <x> <y> <z> : position absolue du centre, seulement si aucune relation ne convient\n");
			ajout("La premiere partie n'a pas de placement. Chaque autre partie DOIT avoir une relation,\n");
			ajout("sinon elle flotte. <autre> est le NOM d'une partie deja ecrite.\n\n");
			if (!NkCreaSansRevolution()) {
			ajout("LA REVOLUTION : un objet TOURNE (verre, bouteille, vase, bol, tasse, colonne) est UNE seule partie\n");
			ajout("de forme revolution. Donne sa SILHOUETTE, sa largeur et sa hauteur REELLES en metres :\n");
			{
				int32 ns = 0;
				const NkCreaSilhouette *S = NkCreaSilhouettes(ns);
				char l[160];
				for (int32 k = 0; k < ns; ++k) {
					snprintf(l, sizeof(l), "- silhouette %s : %s\n", S[k].nom, S[k].effet);
					ajout(l);
				}
			}
			ajout("paroi <e> le rend CREUX (ce qui contient : un recipient) ; sans paroi il est PLEIN (une colonne).\n");
			ajout("Pour un profil sur mesure : profil <rayon hauteur> <rayon hauteur>... au lieu de silhouette.\n");
			// ⚠️ L'EXEMPLE EST UNE BOUTEILLE, PAS UN VERRE : le verre est dans le jeu
			//    d'epreuve (epreuve_revolution.txt). Le montrer mesurerait la recopie.
			// ⚠️ LES EXEMPLES NE SONT AUCUN OBJET DU JEU D'EPREUVE (verre, vase, bol) :
			//    la premiere course a montre que le modele recopie les nombres montres.
			ajout("Exemples, un seau et une colonne :\n");
			ajout("partie seau forme revolution silhouette evase largeur 0.30 hauteur 0.28 paroi 0.005 matiere metal\n");
			ajout("partie colonne forme revolution silhouette colonne largeur 0.40 hauteur 2.20 matiere pierre\n\n");
			}
			ajout("L'ECHELLE : toutes les parties d'un objet ont des tailles du MEME ordre que l'objet. Une partie dix fois\n");
			ajout("plus grande que les autres est refusee (un mur a cote d'un verre).\n\n");
			ajout("LA ROTATION, en degres, tourne la partie autour de son centre (rotation 0 0 30 l'incline de 30 degres).\n\n");
			ajout("LA MATIERE, facultative, parmi :");
			{
				int32 nm = 0;
				const NkCreaMatiere *M = NkCreaMatieres(nm);
				for (int32 i = 0; i < nm; ++i) {
					ajout(i ? ", " : " ");
					ajout(M[i].nom);
				}
				ajout("\n\n");
			}
			ajout("REGLES :\n");
			ajout("- chaque partie porte un nom qui dit CE QU'ELLE EST (assise, pied_avant_gauche, toit...), jamais partie1 ;\n");
			ajout("- des dimensions REALISTES, en metres ;\n");
			ajout("- toutes les parties forment UN SEUL objet d'un seul tenant ;\n");
			ajout("- si la demande ne designe pas un objet precis, ecris exactement : IMPOSSIBLE\n\n");
			// ⚠️ L'EXEMPLE N'EST AUCUN DES OBJETS DU JEU D'EPREUVE. Un banc montre
			//    le seul geste difficile -- des pieds SOUS une planche, ecartes par
			//    `decale` -- sans donner la reponse d'un cas mesure. L'exemple de
			//    l'ancien pont etait une TABLE : la mesurer apres l'avoir montree
			//    aurait mesure la recopie.
			if (NkCreaGabaritsActifs()) {
				int32 nf = 0;
				const char *const *F = NkCreaFamilles(nf);
				int32 lus = 0;
				ajout("BIBLIOTHEQUE DE GABARITS. Ce sont des objets JUSTES, ecrits a la main, en metres.\n");
				ajout("Si l'objet demande appartient a l'une de ces familles, RECOPIE son gabarit et ADAPTE-le\n");
				ajout("(tailles, matieres, pieces en plus ou en moins). Sinon, construis l'objet DE LA MEME FACON :\n");
				ajout("memes relations, memes ordres de grandeur, une partie nommee par piece reelle.\n\n");
				for (int32 i = 0; i < nf; ++i) {
					char chemin[160];
					if (NkCreaSansRevolution() && strcmp(F[i], "revolution") == 0)
						continue;
					// (Q8) UNE FAMILLE CONSTRUITE REMPLACE SON GABARIT : la course Q8 a
					// montre le modele RECOPIER le gabarit « table » ou « maison » au lieu
					// d'ecrire la famille -- deux chemins pour la meme chose, et il prenait
					// l'ancien. Le gabarit ne reste que si les familles sont coupees.
					if (!NkCreaSansFamilles() && (strcmp(F[i], "table") == 0 || strcmp(F[i], "maison") == 0 ||
												  strcmp(F[i], "revolution") == 0))
						continue;
					snprintf(chemin, sizeof(chemin), "Tools/Genia/gabarits/%s.nkscene", F[i]);
					const NkString t = NkFile::ReadAllText(chemin);
					if (!t.Data() || !t.Data()[0])
						continue;
					char entete[96];
					snprintf(entete, sizeof(entete), "--- gabarit %s ---\n", F[i]);
					ajout(entete);
					// Les commentaires du fichier ne partent pas : ils parlent a Rodolf.
					const char *c = t.Data();
					while (*c) {
						const char *l = c;
						while (*c && *c != '\n')
							++c;
						if (*l != '#' && *l != '\r' && l != c) {
							char ligne[400];
							uint32 lg = (uint32)(c - l);
							if (lg >= sizeof(ligne) - 2)
								lg = sizeof(ligne) - 2;
							memcpy(ligne, l, lg);
							ligne[lg] = '\n';
							ligne[lg + 1] = 0;
							if (lg > 0 && ligne[lg - 1] == '\r') {
								ligne[lg - 1] = '\n';
								ligne[lg] = 0;
							}
							ajout(ligne);
						}
						if (*c == '\n')
							++c;
					}
					++lus;
				}
				ajout("\n");
				(void)lus;
			}
			ajout("EXEMPLE, pour « un banc de jardin en bois » :\n");
			ajout("scene banc\n");
			ajout("partie planche forme cube largeur 1.50 hauteur 0.05 profondeur 0.40 matiere bois\n");
			ajout("partie pied_gauche forme cube largeur 0.06 hauteur 0.40 profondeur 0.36 pose_sous planche decale -0.65 0 0 matiere bois\n");
			ajout("partie pied_droit forme cube largeur 0.06 hauteur 0.40 profondeur 0.36 pose_sous planche decale 0.65 0 0 matiere bois\n\n");
			ajout("Maintenant, decris : ");
			ajout(demande ? demande : "");
			ajout("\n");
		}

		/// LA SECTION « CREATION » DU CONTRAT IMPRIME, ajoutee a la suite de celle des
		/// verbes. Ecrite depuis les MEMES tables que l'invite et le lecteur.
		inline bool NkCreaAjouterAuContrat(const char *chemin) {
			FILE *f = fopen(chemin, "ab");
			if (!f)
				return false;
			int32 nf = 0, nm = 0;
			const NkCreaForme *F = NkCreaFormes(nf);
			const NkCreaMatiere *M = NkCreaMatieres(nm);
			fprintf(f, "\n## Creation : un document de parties nommees\n\n");
			fprintf(f, "Une demande de creation (« modelise une chaise ») produit un DOCUMENT au format\n");
			fprintf(f, "`.nkscene` (Tools/Genia/FORMAT_SCENE.md), une ligne par partie :\n\n");
			fprintf(f, "    scene <nom>\n    partie <nom> forme <forme> taille <sx> <sy> <sz> [pose_sur|pose_sous <autre>]\n");
			fprintf(f, "           [decale <dx> <dy> <dz>] [centre <x> <y> <z>] [rotation <rx> <ry> <rz>]\n");
			fprintf(f, "           [matiere <m>] [couleur <r> <g> <b>]\n\n");
			fprintf(f, "Unites : metres, degres. La taille est l'encombrement TOTAL par axe. L'outil lit\n");
			fprintf(f, "l'etendue reelle de chaque primitive, resout les relations sur les boites MONDE et\n");
			fprintf(f, "pose l'objet au sol sous le curseur 3D. Chaque partie devient un objet NOMME, enfant\n");
			fprintf(f, "d'un groupe au nom de la scene ; un geste « annuler » en mode Objet retire le lot.\n\n");
			fprintf(f, "### Les %d formes\n\n| forme | effet |\n|---|---|\n", (int)nf);
			for (int32 i = 0; i < nf; ++i)
				fprintf(f, "| `%s` | %s |\n", F[i].nom, F[i].effet);
			fprintf(f, "\n### Les %d matieres (couleur de base, rugosite, metal)\n\n", (int)nm);
			fprintf(f, "| matiere | couleur | rugosite | metal |\n|---|---|---|---|\n");
			for (int32 i = 0; i < nm; ++i)
				fprintf(f, "| `%s` | %.2f %.2f %.2f | %.2f | %.0f |\n", M[i].nom, (double)M[i].albedo[0],
						(double)M[i].albedo[1], (double)M[i].albedo[2], (double)M[i].rugosite, (double)M[i].metal);
			fprintf(f, "\n⚠️ Ce ne sont pas des prereglages de NKRenderer : `Materials/` n'en porte que de\n");
			fprintf(f, "MATCAP (eclairage d'apercu). Cette table est la premiere source de valeurs PBR.\n\n");
			fprintf(f, "### Ce que la creation refuse, NOMMEMENT\n\n");
			fprintf(f, "- une forme hors de la table, une taille absente ou hors de 0..60 m ;\n");
			fprintf(f, "- une relation vers une partie inconnue ou ecrite APRES ;\n");
			fprintf(f, "- `op difference` : des primitives separees ne se soustraient pas ;\n");
			fprintf(f, "- une demande trop vague (« quelque chose de joli ») : le modele repond IMPOSSIBLE.\n");
			fclose(f);
			return true;
		}

		// =====================================================================
		//  (Q9) RECONNAITRE LA FAMILLE DANS LA DEMANDE — avant le modele
		// =====================================================================
		//  Mesure Q8 : le 7B ne prenait la famille qu'une fois sur deux ; il a
		//  construit une table de 45 cm a la main alors que la famille existait.
		//  La chaine decide donc elle-meme : un LEXIQUE (francais et anglais) lu sur
		//  le NOM DE TETE de la demande -- le premier mot qui n'est ni une formule,
		//  ni un verbe, ni un determinant, ni un adjectif courant.
		//  ⚠️ LE NOM DE TETE, PAS « UN MOT QUELQUE PART » : « un verre de table » est
		//     un verre, « une lampe de table » n'est pas une table, « un tabouret de
		//     bar » n'est pas une table. Un simple « contient table » se tromperait
		//     sur les trois.
		/// Minuscules, accents retires (UTF-8 latin courant), ponctuation -> espace.
		inline void NkCreaNormaliser(const char *src, char *dst, uint32 cap) {
			uint32 n = 0;
			for (const unsigned char *c = (const unsigned char *)src; c && *c && n + 1u < cap; ++c) {
				unsigned char x = *c;
				if (x == 0xC3 && c[1]) {
					const unsigned char y = c[1];
					char r = 0;
					if ((y >= 0xA0 && y <= 0xA5) || (y >= 0x80 && y <= 0x85))
						r = 'a';
					else if (y == 0xA7 || y == 0x87)
						r = 'c';
					else if ((y >= 0xA8 && y <= 0xAB) || (y >= 0x88 && y <= 0x8B))
						r = 'e';
					else if ((y >= 0xAC && y <= 0xAF) || (y >= 0x8C && y <= 0x8F))
						r = 'i';
					else if ((y >= 0xB2 && y <= 0xB6) || (y >= 0x92 && y <= 0x96))
						r = 'o';
					else if ((y >= 0xB9 && y <= 0xBC) || (y >= 0x99 && y <= 0x9C))
						r = 'u';
					if (r) {
						dst[n++] = r;
						++c;
						continue;
					}
				}
				if (x >= 'A' && x <= 'Z')
					x = (unsigned char)(x - 'A' + 'a');
				if (!((x >= 'a' && x <= 'z') || (x >= '0' && x <= '9') || x >= 0x80))
					x = ' ';
				dst[n++] = (char)x;
			}
			dst[n] = 0;
		}

		/// L'INVITE DE CORRECTION : le document du modele, et ce qu'on lui reproche
		/// NOMMEMENT. Un « recommence » sans motif ferait tirer au hasard.
		inline void NkCreaEcrireCorrection(char *dst, uint32 cap, const char *demande, const char *docPrecedent,
										   const char *const *motifs, int32 nMotifs) {
			NkCreaEcrireInvite(dst, cap, demande);
			uint32 n = (uint32)strlen(dst);
			auto ajout = [&](const char *s) {
				while (s && *s && n + 1u < cap)
					dst[n++] = *s++;
				dst[n] = 0;
			};
			ajout("\nTu as deja ecrit ce document :\n");
			ajout(docPrecedent);
			ajout("\n\nL'outil l'a verifie et a trouve ces defauts :\n");
			for (int32 i = 0; i < nMotifs; ++i) {
				ajout("- ");
				ajout(motifs[i]);
				ajout("\n");
			}
			ajout("\nRecris le document COMPLET, corrige, dans le meme format, et rien d'autre.\n");
		}

		// =====================================================================
		//  4. LES LOTS — ce que l'IA a cree, pour l'annuler d'un geste
		// =====================================================================
		static const int32 kCreaMaxLots = 8;
		struct NkCreaLot {
				int32 noeuds[kCreaMaxParties + 1] = {};
				char noms[kCreaMaxParties + 1][24] = {};
				int32 nNoeuds = 0;
				int32 groupe = -1;
				int32 mats[16] = {};
				int32 nMats = 0;
				int32 objetsAvant = 0;
				char scene[24] = {0};
				char doc[6144] = {0}; ///< pour REFAIRE : le document, pas les noeuds
				char demande[256] = {0};
		};

		struct NkCreaEtat {
				// ── la conversation en vol ──
				converse::NkConverseBackendProcessus dorsal;
				converse::NkEnvoiAsync envoi;
				bool prepare = false;
				char demande[256] = {0};
				char docPrecedent[6144] = {0};
				int32 tour = 0;
				int32 toursMax = 2;
				// ── les lots ──
				NkCreaLot lots[kCreaMaxLots];
				int32 nLots = 0;
				NkCreaLot refaire; ///< le dernier lot annule (un seul niveau)
				bool aRefaire = false;
				// ── la mesure ──
				int32 dernierLot = -1;	 ///< numero croissant, pour le journal
				int32 compteurLots = 0;
				int32 annuleDans = -1;	 ///< NK_CREA_ANNULE : images restantes avant le geste
				int32 mesureAnnul = -1;	 ///< images restantes avant la mesure d'apres
				int32 quitteDans = -1;
				// ── L'IMAGE JOINTE (Q7) ──
				// Posee par `NkCreaJoindreImage` (le panneau, piece jointe) ; consommee
				// par la PROCHAINE demande de creation, puis videe.
				char imageJointe[400] = {0};
				char descriptionImage[700] = {0};
				converse::NkConverseBackendProcessus vision;
				converse::NkEnvoiAsync envoiVision;
				int32 ongletAttente = 0;
				converse::NkIConverseBackend *dorsalAttente = nullptr;
				bool tripoApresPose = false;
				// ── (Q9) LA FAMILLE RECONNUE PAR NOTRE CODE, avant le modele ──
				char familleReconnue[24] = {0};
				char nomTete[24] = {0}; ///< le mot de la demande (« verre », « villa ») : le nom de la scene
				char styleReconnu[24] = {0};
				// ── (Q8) CE QUI EST REELLEMENT APPELE, dit dans le fil ──
				char qui[200] = {0};
				char modeleAppele[64] = {0};
				// ── les vues (rendu par l'application) ──
				int32 vuesEtape = -1;
				int32 vuesAttente = 0;
				/// (Q9.4) LA SERIE QUI MONTRE TOUT. Les vues d'une creation ISOLENT le
				/// lot : c'est ce qu'il faut pour photographier l'objet qu'on vient de
				/// poser. Pour la serie qui suit la generation, c'est exactement
				/// l'inverse -- il faut voir l'assemblage ET l'objet genere, cote a
				/// cote, sinon l'image ne dit rien de leur echelle relative.
				/// ⚠️ ET L'ISOLATION AURAIT MONTRE UNE SCENE VIDE : le noeud inscrit au
				///    lot est le CONTENEUR du modele importe ; la geometrie vit dans ses
				///    enfants, qui ne sont pas dans le lot et que l'isolation masquait
				///    donc. Trois series d'images identiques et vides l'ont dit.
				bool vuesToutVoir = false;
				/// (Q10.0) La voie par l'image a ete prise PAR DEFAUT : si elle refuse,
				/// l'assemblage libre reprend la main, et le fil le dit.
				bool replisurAssemblage = false;
				/// Le dorsal a rappeler pour le repli (celui de l'onglet en cours).
				converse::NkIConverseBackend *dorsalRepli = nullptr;
				/// La boite a CADRER quand `vuesToutVoir` : l'assemblage reuni a l'objet
				/// genere. ⚠️ PAS « tout cadrer » : Demo3DHostFrameAll embrasse aussi
				/// les noeuds SOURCES des cartes du navigateur, qui vivent loin de la
				/// scene ; la camera partait si haut que les deux objets tenaient dans
				/// trente pixels. Mesure : trois series d'images ou l'on ne distinguait
				/// rien. On cadre donc une boite qu'on a CALCULEE, pas la scene entiere.
				float32 vuesBoite[6] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
				bool vuesBoiteValide = false;
				char vuesPrefixe[200] = {0};
		};
		inline NkCreaEtat &NkCrea() {
			static NkCreaEtat s;
			return s;
		}

		/// Le nom de l'objet courant : le mot de la demande, sinon la famille.
		/// ⚠️ IL SERT A DEUX CHOSES QUI DOIVENT S'ACCORDER : le dossier du modele et
		///    le nom du noeud dans la hierarchie. Deux derivations se
		///    decorreleraient au premier changement.
		inline const char *NkCreaNomObjet() {
			NkCreaEtat &E = NkCrea();
			if (E.nomTete[0])
				return E.nomTete;
			if (E.nLots > 0 && E.lots[E.nLots - 1].scene[0])
				return E.lots[E.nLots - 1].scene;
			if (E.familleReconnue[0])
				return E.familleReconnue;
			return "genere";
		}


		inline int32 NkCreaCompterObjets() {
			int32 c = 0;
			const int32 nT = demo::Demo3DHostNodeCount();
			for (int32 q = 0; q < nT; ++q)
				if (demo::Demo3DHostUserKind(q) != 0 && !demo::Demo3DHostNodeDeleted(q))
					++c;
			return c;
		}

		// =====================================================================
		//  5. POSER — de la liste des parties a des noeuds de la scene
		// =====================================================================
		/// Un materiau du projet portant ce nom, sinon -1.
		inline int32 NkCreaMatDuProjet(const char *nom) {
			const int32 mx = demo::Demo3DHostProjMatMax();
			char nm[80];
			float32 a[3], r = 0.f, m = 0.f;
			for (int32 i = 0; i < mx; ++i)
				if (demo::Demo3DHostProjMatInfo(i, nm, sizeof(nm), a, &r, &m) && strcmp(nm, nom) == 0)
					return i;
			return -1;
		}

		struct NkCreaBilan {
				int32 poses = 0;
				int32 flottantes = 0;
				float32 mn[3] = {0.f, 0.f, 0.f}, mx[3] = {0.f, 0.f, 0.f};
				bool ok = false;
				char motif[192] = {0};
				char flottanteNoms[160] = {0};
		};

		/// Deux boites se touchent-elles (a `tol` pres) ?
		inline bool NkCreaTouche(const float32 *amn, const float32 *amx, const float32 *bmn, const float32 *bmx,
								 float32 tol) {
			for (int32 a = 0; a < 3; ++a)
				if (amn[a] > bmx[a] + tol || bmn[a] > amx[a] + tol)
					return false;
			return true;
		}

		/// MESURE ce qui est dans la scene, sur les boites MONDE relues a l'hote --
		/// jamais sur ce que le document annoncait. Ecrit le journal de mesure.
		inline void NkCreaMesurer(const NkCreaLot &lot, int32 numero, int32 nRefus, NkCreaBilan &b) {
			static float32 mn[kCreaMaxParties][3], mx[kCreaMaxParties][3];
			int32 nb = 0;
			int32 idx[kCreaMaxParties];
			for (int32 i = 0; i < lot.nNoeuds && nb < kCreaMaxParties; ++i) {
				if (lot.noeuds[i] == lot.groupe)
					continue;
				if (demo::Demo3DHostNodeBounds(lot.noeuds[i], true, mn[nb], mx[nb])) {
					idx[nb] = i;
					++nb;
				}
			}
			b.poses = nb;
			for (int32 a = 0; a < 3; ++a) {
				b.mn[a] = 1e30f;
				b.mx[a] = -1e30f;
			}
			for (int32 i = 0; i < nb; ++i)
				for (int32 a = 0; a < 3; ++a) {
					if (mn[i][a] < b.mn[a])
						b.mn[a] = mn[i][a];
					if (mx[i][a] > b.mx[a])
						b.mx[a] = mx[i][a];
				}
			// RIEN NE FLOTTE : chaque partie rejoint le sol par une chaine de
			// contacts. Parcours en largeur depuis les parties qui touchent y=0.
			bool atteint[kCreaMaxParties] = {};
			const float32 tol = 0.01f;
			int32 file[kCreaMaxParties], tete = 0, queue = 0;
			for (int32 i = 0; i < nb; ++i)
				if (mn[i][1] <= tol && mn[i][1] >= -tol - 1e-3f) {
					atteint[i] = true;
					file[queue++] = i;
				}
			while (tete < queue) {
				const int32 i = file[tete++];
				for (int32 j = 0; j < nb; ++j)
					if (!atteint[j] && NkCreaTouche(mn[i], mx[i], mn[j], mx[j], tol)) {
						atteint[j] = true;
						file[queue++] = j;
					}
			}
			b.flottantes = 0;
			b.flottanteNoms[0] = 0;
			for (int32 i = 0; i < nb; ++i)
				if (!atteint[i]) {
					++b.flottantes;
					const size_t l = strlen(b.flottanteNoms);
					snprintf(b.flottanteNoms + l, sizeof(b.flottanteNoms) - l, "%s%s", l ? ", " : "",
							 lot.noms[idx[i]]);
				}
			b.ok = nb > 0;
			// ── LE JOURNAL DE MESURE : ce que le banc lit, et lui seul ──
			// ⚠️ Des BOITES MONDE, pas des verdicts : le banc recalcule ses
			//    criteres lui-meme. Un banc qui lirait le verdict de l'application
			//    ne mesurerait que sa propre confiance.
			NkDirectory::CreateRecursive("logs");
			if (FILE *f = fopen("logs/crea_mesure.txt", "ab")) {
				fprintf(f, "LOT %d scene=%s parties=%d refusees=%d objets_avant=%d demande=%s\n", numero,
						lot.scene, nb, nRefus, lot.objetsAvant, lot.demande);
				for (int32 i = 0; i < nb; ++i)
					fprintf(f, "PARTIE %d %s %.4f %.4f %.4f %.4f %.4f %.4f\n", numero, lot.noms[idx[i]],
							(double)mn[i][0], (double)mn[i][1], (double)mn[i][2], (double)mx[i][0], (double)mx[i][1],
							(double)mx[i][2]);
				fprintf(f, "FIN %d\n", numero);
				fclose(f);
			}
			// L'ASSEMBLAGE TEL QU'IL EST RENDU, en .obj, un groupe par partie : c'est
			// l'entree de la comparaison avec une reconstruction (TripoSR), et le
			// seul moyen de le sortir du modeleur sans « Exporter ».
			char chemin[96];
			snprintf(chemin, sizeof(chemin), "logs/crea_lot_%03d.obj", (int)numero);
			if (FILE *f = fopen(chemin, "wb")) {
				fprintf(f, "# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen\n");
				fprintf(f, "# assemblage « %s », %d partie(s), coordonnees MONDE en metres\n", lot.scene, nb);
				uint32 base = 1;
				for (int32 i = 0; i < nb; ++i)
					(void)demo::Demo3DHostNodeAppendObj(lot.noeuds[idx[i]], f, &base, lot.noms[idx[i]]);
				fclose(f);
			}
		}

		/// Le materiau du projet portant ce nom de matiere, cree au besoin (table des
		/// matieres), et range dans le lot pour l'annulation.
		inline int32 NkCreaMatiereNommee(const char *nom, NkCreaLot &lot) {
			const int32 idx = NkCreaMatiereDuNom(nom);
			if (idx < 0)
				return -1;
			int32 nm = 0;
			const NkCreaMatiere *M = NkCreaMatieres(nm);
			int32 slot = NkCreaMatDuProjet(nom);
			if (slot < 0) {
				slot = demo::Demo3DHostProjMatCreate();
				if (slot >= 0) {
					demo::Demo3DHostProjMatSetName(slot, nom);
					demo::Demo3DHostProjMatSetParams(slot, M[idx].albedo, M[idx].rugosite, M[idx].metal);
					if (lot.nMats < 16)
						lot.mats[lot.nMats++] = slot;
				}
			}
			return slot;
		}

		/// LA SURFACE DEMANDEE, en m² : un nombre suivi, a trois mots au plus, de « m2 »,
		/// « m² », « metres carres » (et « mettre carrer », ecrit par Rodolf le
		/// 21/09), « sqm », « square ». 0 si la demande n'en donne pas.
		inline float32 NkCreaSurfaceDemandee(const char *demande) {
			if (!demande)
				return 0.f;
			char t[400];
			NkCreaNormaliser(demande, t, sizeof(t));
			// « m² » : l'exposant deux (C2 B2) est devenu un octet >= 0x80 conserve
			char mots[40][24];
			int32 n = 0;
			for (const char *c = t; *c && n < 40;) {
				while (*c == ' ')
					++c;
				if (!*c)
					break;
				uint32 k = 0;
				while (*c && *c != ' ') {
					if (k + 1u < 24u)
						mots[n][k++] = *c;
					++c;
				}
				mots[n][k] = 0;
				++n;
			}
			for (int32 i = 0; i < n; ++i) {
				float32 v = 0.f;
				char nb[24];
				snprintf(nb, sizeof(nb), "%s", mots[i]);
				// « 200m2 » colle
				uint32 j = 0;
				while (nb[j] >= '0' && nb[j] <= '9')
					++j;
				const char *suite = nb + j;
				nb[j] = 0;
				if (j == 0 || !NkCreaNombre(nb, v))
					continue;
				auto surf = [](const char *m) {
					return strncmp(m, "m2", 2) == 0 || (m[0] == 'm' && (unsigned char)m[1] >= 0x80) ||
						   strncmp(m, "carr", 4) == 0 || strncmp(m, "sqm", 3) == 0 || strncmp(m, "square", 6) == 0 ||
						   strcmp(m, "mc") == 0;
				};
				if (*suite && surf(suite))
					return v;
				for (int32 k = i + 1; k < n && k <= i + 3; ++k)
					if (surf(mots[k]))
						return v;
			}
			return 0.f;
		}

		/// (Q8) POSE UNE FAMILLE : notre code construit (NkFamConstruire), la creation
		/// fait le reste comme pour un assemblage -- groupe nomme, curseur 3D, lot
		/// annulable d'un geste, matieres, mesure, vues.
		inline int32 NkCreaPoserFamille(NkModelerState &st, const NkCreaDoc &d, const char *docTexte,
										const char *demande, NkCreaBilan &b) {
			NkCreaEtat &E = NkCrea();
			b = NkCreaBilan();
			NkCreaLot lot;
			lot.objetsAvant = NkCreaCompterObjets();
			char nomScene[24];
			NkCreaCopieNom(nomScene, sizeof(nomScene), d.scene[0] ? d.scene : d.famille.famille);
			NkCreaCopieNom(lot.scene, sizeof(lot.scene), nomScene);
			NkCreaCopieNom(lot.demande, sizeof(lot.demande), demande ? demande : "");
			snprintf(lot.doc, sizeof(lot.doc), "%s", docTexte ? docTexte : "");
			const int32 g = demo::Demo3DHostAddNode(4, 0);
			if (g < 0) {
				snprintf(b.motif, sizeof(b.motif), "Plus d'emplacement libre dans la scene : rien n'est pose.");
				return -1;
			}
			float32 cible[3] = {0.f, 0.f, 0.f};
			{
				float32 cr[3], cs[3];
				demo::Demo3DHostEmptyTransform(g, cible, cr, cs);
			}
			cible[1] = 0.f;
			snprintf(st.customNames[g], 24, "%s", lot.scene);
			lot.groupe = g;
			lot.noeuds[lot.nNoeuds] = g;
			NkCreaCopieNom(lot.noms[lot.nNoeuds], 24, lot.scene);
			++lot.nNoeuds;
			static NkFamPiece pieces[kCreaMaxParties];
			char pourquoi[200] = {0};
			// ── LE NIVEAU DE DETAIL APPARTIENT A L'UTILISATEUR, PAS AU MODELE ──
			// Mesure du 21/09 (course Q8) : le 7B ecrit `detail simple` de lui-meme,
			// et la porte chinoise sortait sans tuiles ni panneaux -- exactement ce
			// que Rodolf reproche (« je ne vois pas de details, des creux »). Le
			// simple n'est donc retenu que si LA DEMANDE le dit (« simple »,
			// « basique », « low poly »). `NK_CREA_DETAIL` reste la porte de mesure.
			NkFamParams fp = d.famille;
			if (strcmp(fp.famille, "maison") == 0 && !std::getenv("NK_CREA_SANS_SURFACE")) {
				// ── (Q9) LA MAISON : NOTRE CODE TIRE LES DIMENSIONS DE LA SURFACE ──
				// « une villa sur 200 m² » avait donne 30 x 20 m sur un niveau (600 m²
				// au sol) : les dimensions venaient du modele. Elles viennent
				// desormais de la SURFACE demandee (habitable, tous niveaux) :
				//   niveaux : jusqu'a 120 m² -> 1, jusqu'a 320 -> 2, au-dela -> 3 ;
				//   emprise = surface / niveaux ; plan en rectangle 1,4 : 1 ;
				//   hauteur d'etage 2,9 m (2,6 sous plafond + plancher) ;
				//   fenetres par facade : une par 3,2 m de facade, de 2 a 6.
				// Sans surface, les pieces la donnent (22 m² par piece, 60 au moins),
				// et sans pieces, 120 m². Le modele ne fournit que le style.
				float32 S = NkCreaSurfaceDemandee(demande);
				const char *origine = "demandee";
				if (S <= 0.f && d.famille.nombre > 0) {
					S = 22.f * (float32)d.famille.nombre;
					if (S < 60.f)
						S = 60.f;
					origine = "tiree du nombre de pieces";
				}
				if (S <= 0.f) {
					S = 120.f;
					origine = "par defaut";
				}
				const int32 niv = S <= 120.f ? 1 : (S <= 320.f ? 2 : 3);
				const float32 emprise = S / (float32)niv;
				fp.largeur = sqrtf(emprise * 1.4f);
				fp.profondeur = emprise / fp.largeur;
				fp.nombre = niv;
				int32 nf = (int32)(fp.largeur / 3.2f + 0.5f);
				fp.fenetres = nf < 2 ? 2 : (nf > 6 ? 6 : nf);
				if (!fp.style[0])
					snprintf(fp.style, sizeof(fp.style), "deux_pans");
				std::printf("[crea] MAISON : surface %.0f m² (%s) -> %d niveau(x), emprise %.1f x %.1f m, %d fenetres/facade, toit %s\n",
							(double)S, origine, (int)niv, (double)fp.largeur, (double)fp.profondeur, (int)fp.fenetres,
							fp.style);
				std::fflush(stdout);
				if (FILE *f = fopen("logs/crea_mesure.txt", "ab")) {
					fprintf(f, "MAISON surface=%.1f niveaux=%d hauteur_etage=2.9 largeur=%.2f profondeur=%.2f toit=%s\n",
							(double)S, (int)niv, (double)fp.largeur, (double)fp.profondeur, fp.style);
					fclose(f);
				}
			}
			if (!std::getenv("NK_CREA_DETAIL")) {
				bool demandeSimple = false;
				static const char *const kS[] = {"simple", "basique", "low poly", "lowpoly", "sans detail"};
				for (const char *k : kS)
					if (demande && strstr(demande, k))
						demandeSimple = true;
				fp.detaille = !demandeSimple;
			}
			const int32 np = NkFamConstruire(fp, pieces, kCreaMaxParties, pourquoi, sizeof(pourquoi));
			if (np <= 0) {
				demo::Demo3DHostDeleteNode(g, false);
				st.customNames[g][0] = 0;
				snprintf(b.motif, sizeof(b.motif), "Famille « %s » : %s", d.famille.famille, pourquoi);
				return -1;
			}
			uint32 faces = 0;
			for (int32 i = 0; i < np; ++i) {
				const int32 n = pieces[i].noeud;
				float32 pp[3], rr[3], ss[3];
				demo::Demo3DHostEmptyTransform(n, pp, rr, ss);
				const float32 p2[3] = {pp[0] + cible[0], pp[1], pp[2] + cible[2]};
				demo::Demo3DHostSetEmptyTransform(n, p2, rr, ss);
				snprintf(st.customNames[n], 24, "%s", pieces[i].nom);
				if (lot.nNoeuds < kCreaMaxParties + 1) {
					lot.noeuds[lot.nNoeuds] = n;
					NkCreaCopieNom(lot.noms[lot.nNoeuds], 24, pieces[i].nom);
					++lot.nNoeuds;
				}
				const int32 slot = NkCreaMatiereNommee(pieces[i].matiere, lot);
				if (slot >= 0)
					demo::Demo3DHostProjMatAssign(n, slot);
				faces += pieces[i].faces;
				// ── LA LIAISON ENTRE DANS LE REGISTRE, AVEC SON PIVOT EN MONDE ────
				// Les pieces viennent d'etre translatees de `cible` : le pivot suit la
				// MEME translation, sinon l'axe resterait a l'origine du monde pendant
				// que la porte est ailleurs -- et la mesure des 30 degres le dirait.
				if (pieces[i].liaison != 0) {
					NkArticulation art;
					art.noeud = n;
					art.parent = pieces[i].parent;
					art.liaison = pieces[i].liaison;
					art.pivot[0] = pieces[i].pivot[0] + cible[0];
					art.pivot[1] = pieces[i].pivot[1];
					art.pivot[2] = pieces[i].pivot[2] + cible[2];
					for (int32 k = 0; k < 3; ++k)
						art.axe[k] = pieces[i].axe[k];
					art.butee[0] = pieces[i].butee[0];
					art.butee[1] = pieces[i].butee[1];
					NkArticTable().PushBack(art);
				}
			}
			const float32 gp[3] = {cible[0], 0.f, cible[2]}, r0[3] = {0.f, 0.f, 0.f}, s1[3] = {1.f, 1.f, 1.f};
			demo::Demo3DHostSetEmptyTransform(g, gp, r0, s1);
			for (int32 i = 0; i < np; ++i)
				demo::Demo3DHostSetNodeParent(pieces[i].noeud, g);
			demo::Demo3DHostHierarchyResync();
			demo::Demo3DHostSelectEmptyNode(g);
			NkMarkDirty(st);
			if (E.nLots == kCreaMaxLots) {
				for (int32 i = 1; i < kCreaMaxLots; ++i)
					E.lots[i - 1] = E.lots[i];
				--E.nLots;
			}
			E.lots[E.nLots++] = lot;
			E.aRefaire = false;
			const int32 numero = ++E.compteurLots;
			E.dernierLot = numero;
			NkCreaMesurer(lot, numero, d.nRefus, b);
			if (FILE *f = fopen("logs/crea_mesure.txt", "ab")) {
				fprintf(f, "FAMILLE %d %s style=%s detail=%s pieces=%d faces=%u\n", numero, d.famille.famille,
						d.famille.style, fp.detaille ? "detaille" : "simple", (int)np, (unsigned)faces);
				fclose(f);
			}
			std::printf("[crea] FAMILLE %s (style %s, %s) : %d pieces, %u faces\n", d.famille.famille,
						d.famille.style[0] ? d.famille.style : "-", fp.detaille ? "detaille" : "simple", (int)np,
						(unsigned)faces);
			std::fflush(stdout);
			return numero;
		}

		/// POSE LE DOCUMENT. Rend le numero du lot, ou -1 (et `b.motif` dit pourquoi).
		inline int32 NkCreaPoser(NkModelerState &st, const NkCreaDoc &d, const char *docTexte, const char *demande,
								 NkCreaBilan &b) {
			NkCreaEtat &E = NkCrea();
			if (d.aFamille)
				return NkCreaPoserFamille(st, d, docTexte, demande, b); // (Q8) notre code construit
			b = NkCreaBilan();
			if (d.n == 0) {
				snprintf(b.motif, sizeof(b.motif), "Aucune partie valide : rien n'est pose.");
				return -1;
			}
			static const bool sSansPose = []() {
				const char *v = std::getenv("NK_CREA_SANS_POSE");
				return v && v[0] && v[0] != '0';
			}();
			NkCreaLot lot;
			lot.objetsAvant = NkCreaCompterObjets();
			NkCreaCopieNom(lot.scene, sizeof(lot.scene), d.scene[0] ? d.scene : "objet");
			NkCreaCopieNom(lot.demande, sizeof(lot.demande), demande ? demande : "");
			snprintf(lot.doc, sizeof(lot.doc), "%s", docTexte ? docTexte : "");
			// Le groupe d'abord : un EMPTY nomme d'apres la scene. Il nait au curseur
			// 3D, et c'est la que l'objet sera pose.
			const int32 g = demo::Demo3DHostAddNode(4, 0);
			if (g < 0) {
				snprintf(b.motif, sizeof(b.motif),
						 "Plus d'emplacement libre dans la scene (64 objets) : rien n'est pose.");
				return -1;
			}
			float32 cible[3] = {0.f, 0.f, 0.f}, r0[3] = {0.f, 0.f, 0.f}, s1[3] = {1.f, 1.f, 1.f};
			{
				float32 cr[3], cs[3];
				demo::Demo3DHostEmptyTransform(g, cible, cr, cs);
			}
			lot.groupe = g;
			lot.noeuds[lot.nNoeuds] = g;
			NkCreaCopieNom(lot.noms[lot.nNoeuds], 24, lot.scene);
			++lot.nNoeuds;
			snprintf(st.customNames[g], 24, "%s", lot.scene);

			int32 nf = 0, nm = 0;
			const NkCreaForme *F = NkCreaFormes(nf);
			const NkCreaMatiere *M = NkCreaMatieres(nm);
			static float32 pos[kCreaMaxParties][3], scl[kCreaMaxParties][3], rmn[kCreaMaxParties][3],
				rmx[kCreaMaxParties][3];
			int32 noeud[kCreaMaxParties];
			for (int32 i = 0; i < d.n; ++i) {
				const NkCreaPartie &pc = d.p[i];
				noeud[i] = -1;
				if (pc.exclue)
					continue; // refusee avant la pose (echelle) : le motif est dans le fil
				int32 n = -1;
				char pourquoiRev[160] = {0};
				if (F[pc.forme].kind == -1) {
					// ── LE PROFIL FERME, TIRE DES POINTS DU MODELE ──────────────
					// Plein : axe en bas -> contour exterieur -> axe en haut. Creux
					// (paroi e) : axe en bas -> exterieur montant -> interieur
					// descendant (rayon - e) jusqu'au fond, a h0 + e -> axe.
					float32 pr[140];
					uint32 np = 0;
					const int32 nc = pc.nNombres / 2;
					const float32 h0 = pc.profil[1];
					auto pt = [&](float32 r, float32 h) {
						if (np < 70) {
							pr[2 * np] = r < 0.f ? 0.f : r;
							pr[2 * np + 1] = h;
							++np;
						}
					};
					if (pc.profil[0] > 1e-4f)
						pt(0.f, h0);
					for (int32 k = 0; k < nc; ++k)
						pt(pc.profil[2 * k], pc.profil[2 * k + 1]);
					if (pc.paroi > 1e-5f) {
						// (Q8) DETAILLE : le BORD est arrondi (un demi-cercle de diametre
						// la paroi, par-dessus -- le chanfrein arrondi d'un vrai verre), et
						// le FOND est trois fois plus epais que la paroi.
						const float32 fond = pc.detailRevolution ? 3.f * pc.paroi : pc.paroi;
						if (pc.detailRevolution && nc >= 1) {
							const float32 rt = pc.profil[2 * (nc - 1)], ht = pc.profil[2 * (nc - 1) + 1];
							const float32 rc = rt - pc.paroi * 0.5f, ra = pc.paroi * 0.5f;
							for (int32 k = 1; k < 6; ++k) {
								const float32 a = 3.14159265f * (float32)k / 6.f;
								pt(rc + ra * cosf(a), ht + ra * sinf(a));
							}
						}
						for (int32 k = nc - 1; k >= 0; --k) {
							const float32 h = pc.profil[2 * k + 1];
							if (h < h0 + fond)
								break;
							pt(pc.profil[2 * k] - pc.paroi, h);
						}
						pt(0.f, h0 + fond);
					} else if (pc.profil[2 * (nc - 1)] > 1e-4f)
						pt(0.f, pc.profil[2 * (nc - 1) + 1]);
					const float32 zero3[3] = {0.f, 0.f, 0.f};
					n = demo::Demo3DHostCreateRevolution(pr, np, 48, zero3, pc.nom, pourquoiRev,
														 (uint32)sizeof(pourquoiRev));
				} else
					n = demo::Demo3DHostAddNode(F[pc.forme].kind, F[pc.forme].sub);
				if (n < 0 && pourquoiRev[0]) {
					char m[240];
					snprintf(m, sizeof(m), "partie « %s » non posee : %s", pc.nom, pourquoiRev);
					(void)NkAiPousser(st, NkModelerState::AiType::Refus, m);
					continue;
				}
				if (n < 0) {
					snprintf(b.motif, sizeof(b.motif),
							 "Plus d'emplacement libre apres %d partie(s) : le reste n'est pas pose.", (int)i);
					break;
				}
				noeud[i] = n;
				lot.noeuds[lot.nNoeuds] = n;
				NkCreaCopieNom(lot.noms[lot.nNoeuds], 24, pc.nom);
				++lot.nNoeuds;
				snprintf(st.customNames[n], 24, "%s", pc.nom);
				// ── L'ECHELLE, TIREE DE L'ETENDUE REELLE DE LA PRIMITIVE ──────
				float32 lmn[3], lmx[3];
				for (int32 a = 0; a < 3; ++a)
					scl[i][a] = 1.f;
				if (demo::Demo3DHostNodeBounds(n, false, lmn, lmx)) {
					for (int32 a = 0; a < 3; ++a) {
						const float32 ext = lmx[a] - lmn[a];
						// Un plan n'a pas d'epaisseur : son echelle en y n'a aucun
						// sens, on la laisse a 1 au lieu de diviser par zero.
						scl[i][a] = ext > 1e-4f ? (pc.taille[a] > 1e-4f ? pc.taille[a] : 1e-4f) / ext : 1.f;
						// une revolution sans taille explicite garde les metres de son profil
						if (F[pc.forme].kind == -1 && !pc.tailleDonnee)
							scl[i][a] = 1.f;
					}
				}
				const float32 zero[3] = {0.f, 0.f, 0.f};
				demo::Demo3DHostSetEmptyTransform(n, zero, pc.rot, scl[i]);
				// La boite RELATIVE (noeud a l'origine), rotation et echelle comprises.
				if (!demo::Demo3DHostNodeBounds(n, true, rmn[i], rmx[i]))
					for (int32 a = 0; a < 3; ++a) {
						rmn[i][a] = -0.5f * pc.taille[a];
						rmx[i][a] = 0.5f * pc.taille[a];
					}
				// ── LE PLACEMENT, RESOLU SUR LES BOITES ─────────────────────
				float32 cB[3];
				for (int32 a = 0; a < 3; ++a)
					cB[a] = 0.5f * (rmn[i][a] + rmx[i][a]);
				for (int32 a = 0; a < 3; ++a)
					pos[i][a] = pc.aCentre ? pc.centre[a] - cB[a] : -cB[a];
				if (pc.rel == 1 && pc.relIdx == -2) {
					// `pose_sur sol` : le dessous a y = 0, centre en x et z sur
					// l'origine de l'objet (puis `decale`). Le sol n'est pas une
					// partie ; c'est le plan que la pose au sol vise de toute facon.
					pos[i][0] = -cB[0];
					pos[i][1] = -rmn[i][1];
					pos[i][2] = -cB[2];
				} else if (pc.rel != 0 && pc.relIdx >= 0 && noeud[pc.relIdx] >= 0) {
					{
						const int32 k = pc.relIdx;
						for (int32 a = 0; a < 3; ++a)
							pos[i][a] = pos[k][a] + 0.5f * (rmn[k][a] + rmx[k][a]) - cB[a];
					}
					if (pc.rel == 3)
						pos[i][1] = pc.aCentre ? pc.centre[1] - cB[1] : -cB[1];
				}
				// ── UNE RELATION PAR AXE (course 3) ─────────────────────────────
				// Chaque relation pose le CONTACT sur son axe ; les axes qu'aucune
				// relation ne nomme restent centres sur la premiere cible. « pose_sur
				// tronc a_droite_de tronc » donne donc une branche posee a l'angle du
				// tronc, au lieu que la seconde relation efface la premiere.
				for (int32 r = 0; r < pc.nRels; ++r) {
					const int32 k = pc.relsIdx[r];
					if (k < 0 || noeud[k] < 0)
						continue;
					const float32 aMin[3] = {pos[k][0] + rmn[k][0], pos[k][1] + rmn[k][1], pos[k][2] + rmn[k][2]};
					const float32 aMax[3] = {pos[k][0] + rmx[k][0], pos[k][1] + rmx[k][1], pos[k][2] + rmx[k][2]};
					// ── LE CONTACT, SUR L'AXE DE LA RELATION ──────────────────
					// pose_sur/sous : vertical. a_gauche_de/a_droite_de : x.
					// devant/derriere : z (devant = +z, cote de la vue de face).
					// Les deux autres axes restent CENTRES sur l'autre partie.
					// ⚠️ CES QUATRE RELATIONS HORIZONTALES ONT ETE AJOUTEES APRES
					//    LA COURSE 1 : un bras « pose_sous corps » pendait SOUS le
					//    torse. FORMAT_SCENE §7 l'annoncait : « pose_sur est
					//    vertical... se rouvre au premier document qui en aurait
					//    besoin ». Le voici.
					switch (pc.rels[r]) {
						case 1: pos[i][1] = aMax[1] - rmn[i][1]; break;
						case 2: pos[i][1] = aMin[1] - rmx[i][1]; break;
						case 4: pos[i][0] = aMin[0] - rmx[i][0]; break;
						case 5: pos[i][0] = aMax[0] - rmn[i][0]; break;
						case 6: pos[i][2] = aMax[2] - rmn[i][2]; break;
						case 7: pos[i][2] = aMin[2] - rmx[i][2]; break;
						default: break;
					}
				}
				for (int32 a = 0; a < 3; ++a)
					pos[i][a] += pc.decale[a];
				// ── LA MATIERE ──────────────────────────────────────────────
				if (pc.matiere >= 0 || pc.aCouleur) {
					char mnom[40];
					float32 alb[3] = {0.7f, 0.7f, 0.7f}, rg = 0.8f, mt = 0.f;
					if (pc.matiere >= 0) {
						snprintf(mnom, sizeof(mnom), "%s", M[pc.matiere].nom);
						for (int32 a = 0; a < 3; ++a)
							alb[a] = M[pc.matiere].albedo[a];
						rg = M[pc.matiere].rugosite;
						mt = M[pc.matiere].metal;
					}
					if (pc.aCouleur) {
						snprintf(mnom, sizeof(mnom), "%s_%02X%02X%02X", pc.matiere >= 0 ? M[pc.matiere].nom : "teinte",
								 (unsigned)(pc.couleur[0] * 255.f), (unsigned)(pc.couleur[1] * 255.f),
								 (unsigned)(pc.couleur[2] * 255.f));
						for (int32 a = 0; a < 3; ++a)
							alb[a] = pc.couleur[a];
					}
					int32 slot = NkCreaMatDuProjet(mnom);
					if (slot < 0) {
						slot = demo::Demo3DHostProjMatCreate();
						if (slot >= 0) {
							demo::Demo3DHostProjMatSetName(slot, mnom);
							demo::Demo3DHostProjMatSetParams(slot, alb, rg, mt);
							if (lot.nMats < 16)
								lot.mats[lot.nMats++] = slot;
						}
					}
					if (slot >= 0)
						demo::Demo3DHostProjMatAssign(n, slot);
				}
			}
			// ── LA POSE AU SOL, ET SOUS LE CURSEUR ──────────────────────────────
			// ⚠️ C'EST L'OUTIL, PAS LE MODELE. Le document dit qui repose sur qui ;
			//    ou se trouve le sol, c'est l'application qui le sait. La mutation
			//    `NK_CREA_SANS_POSE=1` laisse l'objet la ou le document le met.
			float32 gmn[3] = {1e30f, 1e30f, 1e30f}, gmx[3] = {-1e30f, -1e30f, -1e30f};
			for (int32 i = 0; i < d.n; ++i) {
				if (noeud[i] < 0)
					continue;
				for (int32 a = 0; a < 3; ++a) {
					if (pos[i][a] + rmn[i][a] < gmn[a])
						gmn[a] = pos[i][a] + rmn[i][a];
					if (pos[i][a] + rmx[i][a] > gmx[a])
						gmx[a] = pos[i][a] + rmx[i][a];
				}
			}
			float32 dep[3] = {0.f, 0.f, 0.f};
			if (!sSansPose && gmn[0] < 1e29f) {
				dep[0] = cible[0] - 0.5f * (gmn[0] + gmx[0]);
				dep[1] = -gmn[1];
				dep[2] = cible[2] - 0.5f * (gmn[2] + gmx[2]);
			}
			for (int32 i = 0; i < d.n; ++i) {
				if (noeud[i] < 0)
					continue;
				float32 p[3] = {pos[i][0] + dep[0], pos[i][1] + dep[1], pos[i][2] + dep[2]};
				demo::Demo3DHostSetEmptyTransform(noeud[i], p, d.p[i].rot, scl[i]);
			}
			const float32 gp[3] = {cible[0], 0.f, cible[2]};
			demo::Demo3DHostSetEmptyTransform(g, gp, r0, s1);
			// La parente APRES les transformations, puis le recalage du detecteur :
			// sinon la premiere image lirait « le groupe a bouge » et trainerait
			// ses enfants deja a leur place.
			for (int32 i = 0; i < d.n; ++i)
				if (noeud[i] >= 0)
					demo::Demo3DHostSetNodeParent(noeud[i], g);
			demo::Demo3DHostHierarchyResync();
			demo::Demo3DHostSelectEmptyNode(g);
			NkMarkDirty(st);

			// ── LE LOT ENTRE DANS LA PILE ───────────────────────────────────────
			if (E.nLots == kCreaMaxLots) {
				for (int32 i = 1; i < kCreaMaxLots; ++i)
					E.lots[i - 1] = E.lots[i];
				--E.nLots;
			}
			E.lots[E.nLots++] = lot;
			E.aRefaire = false; // une creation neuve efface le « refaire », comme partout
			const int32 numero = ++E.compteurLots;
			E.dernierLot = numero;
			NkCreaMesurer(lot, numero, d.nRefus, b);
			// Le document, lisible par Rodolf, a cote des autres traces.
			{
				char chemin[96];
				snprintf(chemin, sizeof(chemin), "logs/creation_%03d.nkscene", (int)numero);
				if (FILE *f = fopen(chemin, "wb")) {
					fprintf(f, "# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen\n");
					fprintf(f, "# Document ECRIT PAR LE MODELE, pose tel quel par NK3DModeler.\n");
					fprintf(f, "# demande %s\n%s\n", lot.demande, lot.doc);
					fclose(f);
				}
			}
			return numero;
		}

		// =====================================================================
		//  6. ANNULER / REFAIRE, EN MODE OBJET
		// =====================================================================
		/// Retire le dernier lot. Rend faux s'il n'y a rien a annuler -- et
		/// l'appelant ne pretend alors rien avoir fait.
		inline bool NkCreaAnnuler(NkModelerState &st) {
			NkCreaEtat &E = NkCrea();
			if (E.nLots == 0)
				return false;
			NkCreaLot &lot = E.lots[E.nLots - 1];
			int32 retires = 0, absents = 0;
			// Les parties d'abord, le groupe ensuite : jamais un parent retire avant
			// ses enfants.
			for (int32 i = lot.nNoeuds - 1; i >= 0; --i) {
				const int32 n = lot.noeuds[i];
				// ⚠️ ON NE RETIRE QUE CE QUI EST ENCORE A NOUS. Un emplacement libere
				//    a la main puis reutilise par un autre objet porte un autre nom :
				//    le supprimer effacerait le travail de Rodolf.
				if (demo::Demo3DHostNodeDeleted(n) || demo::Demo3DHostUserKind(n) == 0 ||
					strcmp(st.customNames[n], lot.noms[i]) != 0) {
					++absents;
					continue;
				}
				demo::Demo3DHostDeleteNode(n, false);
				st.customNames[n][0] = 0;
				++retires;
			}
			for (int32 m = 0; m < lot.nMats; ++m)
				demo::Demo3DHostProjMatDelete(lot.mats[m]);
			demo::Demo3DHostHierarchyResync();
			E.refaire = lot;
			E.aRefaire = true;
			--E.nLots;
			char m[192];
			if (absents)
				snprintf(m, sizeof(m), "Annule : « %s » -- %d objet(s) retire(s), %d n'existai(en)t plus.", lot.scene,
						 (int)retires, (int)absents);
			else
				snprintf(m, sizeof(m), "Annule : « %s » -- %d objet(s) retire(s). Refaire le repose.", lot.scene,
						 (int)retires);
			(void)NkAiPousser(st, NkModelerState::AiType::Note, m);
			std::printf("[crea] ANNULE lot « %s » : %d retire(s), %d absent(s)\n", lot.scene, (int)retires,
						(int)absents);
			std::fflush(stdout);
			NkMarkDirty(st);
			return true;
		}

		inline bool NkCreaRefaire(NkModelerState &st) {
			NkCreaEtat &E = NkCrea();
			if (!E.aRefaire)
				return false;
			E.aRefaire = false;
			static NkCreaDoc d;
			NkCreaLire(E.refaire.doc, d);
			NkCreaBilan b;
			static char doc[6144], dem[256];
			snprintf(doc, sizeof(doc), "%s", E.refaire.doc);
			snprintf(dem, sizeof(dem), "%s", E.refaire.demande);
			return NkCreaPoser(st, d, doc, dem, b) > 0;
		}

		/// VOIE (c) : texte -> image (modele de diffusion local) -> detourage ->
		/// TripoSR, par la PORTE TEXTE du generateur. `NK_CREA_VOIE=image`.
		// ── (Q10.0) LA REGLE DES VOIES, ECRITE UNE FOIS ────────────────────────
		//   famille reconnue          -> LA FAMILLE (notre code construit)
		//   sinon                     -> LA VOIE PAR L'IMAGE (texte -> diffusion
		//                                locale -> TripoSR), et c'est LE DEFAUT
		//   la voie par l'image refuse -> l'assemblage libre, en REPLI NOMME
		//
		// ⚠️ PLUS DE VARIABLE D'ENVIRONNEMENT POUR L'UTILISATEUR. Jusqu'au 22/09 il
		//    fallait `NK_CREA_VOIE=image` pour obtenir autre chose que des pavés ; un
		//    reglage qu'il faut connaitre pour avoir le bon resultat est un reglage
		//    qui ne sera jamais mis. `NK_CREA_VOIE` ne sert plus qu'a FORCER une voie
		//    pour la mesure -- c'est un instrument, pas une porte d'entree.
		//
		// ⚠️ LA PRESENCE DES POIDS N'EST PAS TESTEE ICI, ET C'EST VOULU.
		//    `genia_texte_3d.py` la verifie deja, etage par etage (un telechargement
		//    interrompu laisse `model_index.json` sans poids). La reecrire en C++
		//    ferait DEUX verites sur le meme fait, qui se decorreleraient au premier
		//    changement de format. On lance, et le REFUS NOMME du script declenche le
		//    repli -- une seule autorite.
		inline bool NkCreaVoieImage() {
			const char *v = std::getenv("NK_CREA_VOIE");
			return v && strcmp(v, "image") == 0;
		}
		/// `NK_CREA_VOIE=assemblage` : force l'ancien comportement (plan libre), pour
		/// mesurer « avant » dans le MEME binaire.
		inline bool NkCreaVoieAssemblageForcee() {
			const char *v = std::getenv("NK_CREA_VOIE");
			return v && strcmp(v, "assemblage") == 0;
		}

		/// ── LE GENERATEUR, VU COMME UN DORSAL DE CONVERSATION ────────────────
		/// ⚠️ POURQUOI CE DEGUISEMENT : `NkEnvoiAsync` (NKConverse) sait deja faire
		///    tourner un appel long HORS du fil d'affichage, le recolter une fois,
		///    compter les images pendant l'attente, et lacher sa tache sans course.
		///    Ecrire un second mecanisme de fil pour le generateur serait la dette
		///    des trois exemplaires. On lui donne donc la forme d'un dorsal :
		///    `Complete` lance la generation, `rep.text` rend le chemin produit.
		/// Les chemins sont poses AVANT le lancement et jamais retouches ensuite
		/// (la regle de NkTacheIA : un seul ecrivain par champ).
		class NkGeniaDorsal final : public converse::NkIConverseBackend {
			public:
				NkIGenerateur *gen = nullptr;
				bool depuisTexte = false;
				NkString entree; ///< l'image, ou l'invite
				NkString sortie; ///< le .glb attendu
				bool Complete(const converse::NkConverseRequest &, converse::NkConverseReply &out) override {
					NkString why;
					const bool ok = gen && (depuisTexte ? gen->GenererDepuisTexte(entree.CStr(), sortie.CStr(), why)
														: gen->Generer(entree.CStr(), sortie.CStr(), why));
					out.success = ok;
					out.text = ok ? sortie : NkString("");
					out.error = ok ? NkString("") : why;
					return ok;
				}
				bool IsAvailable() const override {
					return gen != nullptr;
				}
				const char *Name() const override {
					return depuisTexte ? "generateur (texte)" : "generateur (image)";
				}
		};
		struct NkGeniaVol {
				converse::NkEnvoiAsync envoi;
				NkGeniaDorsal dorsal;
				char voie[8] = {0};
				int32 relances = 0;
		};
		inline NkGeniaVol &NkGenia() {
			static NkGeniaVol s;
			return s;
		}

		/// LANCE une generation hors du fil. Rend faux (et le dit) si une autre vole.
		inline bool NkGeniaLancer(NkModelerState &st, bool depuisTexte, const char *entree, const char *voie) {
			NkGeniaVol &G = NkGenia();
			if (G.envoi.EnCours()) {
				(void)NkAiPousser(st, NkModelerState::AiType::Refus,
								  "Une generation est deja en cours : attendez qu'elle rende son objet.");
				return false;
			}
			G.dorsal.gen = &NkGeniaGenerateurParDefaut();
			G.dorsal.depuisTexte = depuisTexte;
			G.dorsal.entree = NkString(entree);
			if (depuisTexte) {
				// Le nom du fichier vient de la demande, pas d'un compteur : on doit
				// pouvoir le retrouver dans le dossier Genia/ du projet.
				char nom[80];
				uint32 n = 0;
				for (const char *c = entree; *c && n + 1u < 40u; ++c) {
					const char x = *c;
					nom[n++] = ((x >= 'a' && x <= 'z') || (x >= '0' && x <= '9')) ? x : '_';
				}
				nom[n] = 0;
				snprintf(nom + n, sizeof(nom) - n, ".png");
				G.dorsal.sortie = NkGeniaSortiePour(st, nom);
			} else
				G.dorsal.sortie = NkGeniaSortiePour(st, entree);
			snprintf(G.voie, sizeof(G.voie), "%s", voie);
			// ── (Q18.4) SANS PROJET, ON REFUSE AVANT DE GENERER ────────────────────
			// ⚠️ MESURE DU 24/09 : sans projet ouvert, la generation ecrivait dans le
			//    dossier temporaire un fichier parfaitement valide -- que l'import
			//    refusait ensuite (« Import impossible : aucun PROJET ouvert »). 72
			//    secondes de carte graphique pour un fichier que personne ne pouvait
			//    ouvrir, et le fil ne disait « 0 carte » qu'a la fin. Le refus remonte
			//    donc AVANT le calcul, et il dit quoi faire.
			if (G.dorsal.sortie.Empty()) {
				(void)NkAiPousser(st, NkModelerState::AiType::Refus,
								  "Aucun projet n'est ouvert : le modele n'aurait nulle part ou vivre, et rien "
								  "ne pourrait etre importe. Fichier > Nouveau projet, puis redemandez.");
				std::printf("[crea] REFUS voie (%s) : aucun projet ouvert\n", voie);
				std::fflush(stdout);
				return false;
			}
			{
				// le dossier du modele peut ne pas exister encore
				char dos[400];
				snprintf(dos, sizeof(dos), "%s", G.dorsal.sortie.CStr());
				if (char *b = strrchr(dos, '/'))
					*b = 0;
				NkDirectory::CreateRecursive(dos);
			}
			// ── (Q18.4) LE DOSSIER DU MODELE : l'entree y est RECOPIEE, et la fiche
			//    ecrite MAINTENANT. Une fiche qu'on remplirait apres coup manquerait
			//    pour les objets qui auront echoue -- c'est-a-dire ceux qu'on voudra
			//    relire.
			{
				char nomObj[64];
				NkImpStem(G.dorsal.sortie.CStr(), nomObj, (uint32)sizeof(nomObj));
				const NkString dm = NkGeniaDossierModele(st, nomObj);
				if (!dm.Empty()) {
					if (!depuisTexte && entree && entree[0]) {
						char stem2[48];
						NkImpStem(entree, stem2, (uint32)sizeof(stem2));
						const char *ext = strrchr(entree, '.');
						NkString dst = dm;
						dst.Append("vues/");
						dst.Append(stem2);
						dst.Append(ext ? ext : ".png");
						// L'IMAGE FOURNIE EST RECOPIEE, PAS REFERENCEE : elle peut vivre
						// n'importe ou sur la machine de l'utilisateur, et le dossier du
						// modele doit rester lisible seul, plus tard, ailleurs.
						(void)NkFile::Copy(entree, dst.CStr(), true);
					}
					NkCreaEtat &EC = NkCrea();
					NkGeniaEcrireFiche(dm, nomObj, EC.demande, voie,
									   depuisTexte ? "diffusion locale + TripoSR" : "TripoSR",
									   depuisTexte ? "" : entree);
				}
			}
			NkString pourquoi;
			if (!G.envoi.Lancer(&G.dorsal, NkString("genia"), pourquoi)) {
				char m[200];
				snprintf(m, sizeof(m), "La generation n'a pas pu partir : %s", pourquoi.CStr());
				(void)NkAiPousser(st, NkModelerState::AiType::Refus, m);
				return false;
			}
			std::printf("[crea] VOIE (%s) : generation lancee HORS du fil d'affichage -> %s\n", voie,
						G.dorsal.sortie.CStr());
			std::fflush(stdout);
			return true;
		}

		/// A CHAQUE IMAGE : recolte, puis IMPORT sur le fil d'affichage (l'hote et
		/// le navigateur ne se touchent que d'ici).
		/// La boite MONDE du dernier lot, relue a l'hote.
		inline bool NkCreaBoiteDernierLot(float32 *mn, float32 *mx) {
			NkCreaEtat &E = NkCrea();
			if (E.nLots == 0)
				return false;
			const NkCreaLot &l = E.lots[E.nLots - 1];
			bool trouve = false;
			for (int32 a = 0; a < 3; ++a) {
				mn[a] = 1e30f;
				mx[a] = -1e30f;
			}
			for (int32 i = 0; i < l.nNoeuds; ++i) {
				float32 a0[3], a1[3];
				if (!demo::Demo3DHostNodeBounds(l.noeuds[i], true, a0, a1))
					continue;
				trouve = true;
				for (int32 a = 0; a < 3; ++a) {
					if (a0[a] < mn[a])
						mn[a] = a0[a];
					if (a1[a] > mx[a])
						mx[a] = a1[a];
				}
			}
			return trouve;
		}

		inline void NkGeniaRecolter(NkModelerState &st) {
			NkGeniaVol &G = NkGenia();
			if (!G.envoi.EnCours())
				return;
			NkString rep, err;
			bool ok = false;
			if (!G.envoi.Recolter(rep, err, ok))
				return;
			char m[300];
			std::printf("[crea] VOIE (%s) : generation finie en %.1f s, %u image(s) affichee(s) pendant l'attente -> %s\n",
						G.voie, (double)G.envoi.Secondes(), (unsigned)G.envoi.Images(), ok ? "fichier ecrit" : err.CStr());
			std::fflush(stdout);
			if (!ok) {
				NkCreaEtat &EE = NkCrea();
				// ── (Q10.0) LE REPLI NOMME ────────────────────────────────────────
				// La voie par l'image a ete prise PAR DEFAUT et elle a refuse (poids
				// absents, carte prise, script casse). On ne laisse pas l'utilisateur
				// les mains vides : l'assemblage libre reprend, ET ON LE DIT. Un repli
				// muet ferait croire que c'est le resultat normal.
				if (EE.replisurAssemblage) {
					EE.replisurAssemblage = false;
					snprintf(m, sizeof(m),
							 "La voie par l'image n'a rien rendu (%s). Je retombe sur un assemblage de volumes "
							 "nommes -- c'est une EBAUCHE, pas le resultat vise.",
							 err.CStr() ? err.CStr() : "raison inconnue");
					(void)NkAiPousser(st, NkModelerState::AiType::Refus, m);
					std::printf("[crea] VOIE PAR DEFAUT : refus de la voie (c) -> REPLI sur l'assemblage libre (%s)\n",
								err.CStr() ? err.CStr() : "?");
					std::fflush(stdout);
					static char inv[22000];
					NkCreaEcrireInvite(inv, sizeof(inv), EE.demande);
					NkString pq;
					if (EE.envoi.Lancer(EE.dorsalRepli ? EE.dorsalRepli : (converse::NkIConverseBackend *)&EE.dorsal,
										NkString(inv), pq))
						return;
					snprintf(m, sizeof(m), "Le repli sur l'assemblage a echoue lui aussi : %s",
							 pq.Data() ? pq.Data() : "raison inconnue");
					(void)NkAiPousser(st, NkModelerState::AiType::Refus, m);
					return;
				}
				snprintf(m, sizeof(m), "Voie (%s) : rien n'a ete genere -- %s", G.voie,
						 err.CStr() ? err.CStr() : "raison inconnue");
				(void)NkAiPousser(st, NkModelerState::AiType::Refus, m);
				return;
			}
			const char *un[1] = {rep.CStr()};
			const int32 avant = st.BrowserCount();
			NkVector<int32> cartes;
			const int32 importes = NkImportFiles(st, un, 1, &cartes);
			// ── (Q9.4) L'OBJET VA DANS LA SCENE, PAS SEULEMENT AU NAVIGATEUR ────
			// « Glissez-la dans la scene » demandait un geste de plus pour un objet
			// qu'on vient de demander. La carte reste (elle sert a le reposer), mais
			// le noeud est pose tout de suite, a cote de l'assemblage, mis a
			// l'echelle de sa hauteur, et inscrit dans un lot : Ctrl+Z le retire
			// comme il retire une creation.
			int32 poses = 0;
			float32 hAv = 0.f, hAp = 0.f, kEch = 1.f;
			float32 basApres = 0.f; ///< le y minimal APRES la pose, relu a l'hote
			bool debout = false;
			if (importes > 0 && cartes.Size() > 0) {
				NkCreaEtat &E = NkCrea();
				NkVector<int32> nes;
				poses = NkImportInstantiate(st, cartes, nullptr, &nes);
				if (poses > 0) {
					// LA BOITE DU RESULTAT, MESUREE (monde), et celle de l'assemblage
					// en place -- c'est elle qui donne l'echelle et le cote ou poser.
					float32 mn[3] = {0.f, 0.f, 0.f}, mx[3] = {0.f, 0.f, 0.f};
					bool aBoite = false;
					for (usize i = 0; i < nes.Size(); ++i) {
						float32 a[3], b[3];
						if (!demo::Demo3DHostNodeBounds(nes[i], true, a, b))
							continue;
						for (int32 k = 0; k < 3; ++k) {
							if (!aBoite || a[k] < mn[k])
								mn[k] = a[k];
							if (!aBoite || b[k] > mx[k])
								mx[k] = b[k];
						}
						aBoite = true;
					}
					// La cible : la hauteur du dernier lot cree, sinon 1 m.
					// ⚠️ LA BOITE DE L'ASSEMBLAGE VIENT DE LA PORTE QUI EXISTE DEJA
					//    (NkCreaBoiteDernierLot), et pas d'une seconde boucle ecrite
					//    ici : deux derivations du meme chiffre se decorrelent au
					//    premier changement, et c'est alors la copie qui ment.
					float32 cibleH = 1.f, bordX = 0.f;
					const char *origineCible = "1 m par defaut";
					float32 lm[3] = {0.f, 0.f, 0.f}, lM[3] = {0.f, 0.f, 0.f};
					const bool aAssemblage = NkCreaBoiteDernierLot(lm, lM);
					if (aAssemblage && lM[1] - lm[1] > 1e-4f) {
						cibleH = lM[1] - lm[1];
						bordX = lM[0];
						origineCible = "la boite de l'assemblage";
					}
					// ── LA TAILLE PLAUSIBLE PRIME SUR CELLE DU MODELE (25/09) ────────
					// Rodolf : « la cible de 5,30 m vient d'un nombre invente par le
					// 7B ». Quand la famille est RECONNUE, la bibliotheque sait ce
					// qu'est une hauteur defendable pour cet objet, et c'est elle qui
					// donne la cible. ⚠️ ON NE CORRIGE QUE CE QUI EST HORS DU MONDE :
					// si la hauteur de l'assemblage tombe deja dans l'intervalle
					// plausible, on n'y touche pas -- l'ecraser jetterait une
					// information juste (« une table BASSE ») au nom d'une moyenne.
					{
						NkCreaEtat &EH = NkCrea();
						float32 lo = 0.f, hi = 0.f, def = 0.f;
						if (EH.familleReconnue[0] &&
							renderer::NkFamilleDimensionPlausible(EH.familleReconnue, 1, &lo, &hi, &def)) {
							if (cibleH < lo || cibleH > hi) {
								std::printf("[crea] TAILLE : hauteur %.2f m hors de l'intervalle plausible de "
											"« %s » (%.2f a %.2f) -> je prends %.2f m\n",
											(double)cibleH, EH.familleReconnue, (double)lo, (double)hi, (double)def);
								std::fflush(stdout);
								cibleH = def;
								origineCible = "la table de dimensions plausibles";
							}
						}
					}
					if (aBoite) {
						hAv = mx[1] - mn[1];
						const float32 lx = mx[0] - mn[0], lz = mx[2] - mn[2];
						// ⚠️ « DEBOUT » EST UNE OBSERVATION, PAS UNE CORRECTION. La
						//    conversion d'axes est deja faite a la source
						//    (genia_triposr.py, Z-up -> Y-up, NkFBXLoader.cpp:1717) ;
						//    remettre une rotation ici serait une derivation EN DOUBLE,
						//    et deux redressements se compensent ou s'ajoutent sans que
						//    rien ne le dise. On MESURE donc, on ECRIT ce qu'on voit, et
						//    on ne tourne pas : si ce compte dit un jour « couche », le
						//    defaut est a la source et c'est la qu'il faut le corriger.
						debout = hAv >= 0.6f * (lx > lz ? lx : lz);
						if (hAv > 1e-4f) {
							kEch = cibleH / hAv;
							hAp = cibleH;
						}
						const float32 marge = 0.35f * cibleH;
						for (usize i = 0; i < nes.Size(); ++i) {
							float32 sp[3], sr[3], ss[3];
							if (!demo::Demo3DHostEmptyTransform(nes[i], sp, sr, ss))
								continue;
							// ⚠️ L'ECHELLE PART DE L'ORIGINE DU NOEUD, PAS DE SA BOITE, et
							//    la premiere version melangeait les deux : l'objet est
							//    parti hors du champ, et l'image rendue par l'application
							//    l'a montre tout de suite. Le calcul juste : apres
							//    SetModelTransform(np3, rot, k*ss), un point du monde
							//    devient np3 + k * (monde - sp). On resout donc np3 pour
							//    que le BAS touche le sol, que le cote GAUCHE tombe apres
							//    l'assemblage, et que la profondeur reste centree.
							const float32 np3[3] = {bordX + marge - kEch * (mn[0] - sp[0]),
													-kEch * (mn[1] - sp[1]),
													-kEch * (0.5f * (mn[2] + mx[2]) - sp[2])};
							const float32 ns3[3] = {ss[0] * kEch, ss[1] * kEch, ss[2] * kEch};
							demo::Demo3DHostSetModelTransform(nes[i], np3, sr, ns3);
						}
						demo::Demo3DHostHierarchyResync();
						// LA BOITE D'APRES, RELUE A L'HOTE. Sans elle, « pose a
						// l'echelle » serait une intention, pas une mesure -- et la
						// premiere version de ce calcul a justement envoye l'objet
						// hors du champ sans qu'aucun chiffre ne le dise.
						{
							float32 a2[3] = {0.f, 0.f, 0.f}, b2[3] = {0.f, 0.f, 0.f};
							bool ab2 = false;
							for (usize i = 0; i < nes.Size(); ++i) {
								float32 u[3], w[3];
								if (!demo::Demo3DHostNodeBounds(nes[i], true, u, w))
									continue;
								for (int32 k = 0; k < 3; ++k) {
									if (!ab2 || u[k] < a2[k])
										a2[k] = u[k];
									if (!ab2 || w[k] > b2[k])
										b2[k] = w[k];
								}
								ab2 = true;
							}
							if (ab2)
								basApres = a2[1]; // « au sol » se MESURE, il ne se decrete pas
							if (FILE *f = fopen("logs/crea_mesure.txt", "ab")) {
								fprintf(f,
										"TRIPOSR_BOITE avant=(%.3f,%.3f,%.3f)-(%.3f,%.3f,%.3f) "
										"apres=(%.3f,%.3f,%.3f)-(%.3f,%.3f,%.3f) bordX=%.3f cibleH=%.3f\n",
										(double)mn[0], (double)mn[1], (double)mn[2], (double)mx[0], (double)mx[1],
										(double)mx[2], (double)a2[0], (double)a2[1], (double)a2[2], (double)b2[0],
										(double)b2[1], (double)b2[2], (double)bordX, (double)cibleH);
								fclose(f);
							}
							std::printf("[crea] TRIPOSR boite avant (%.3f,%.3f,%.3f)-(%.3f,%.3f,%.3f) -> apres "
										"(%.3f,%.3f,%.3f)-(%.3f,%.3f,%.3f), bordX=%.3f\n",
										(double)mn[0], (double)mn[1], (double)mn[2], (double)mx[0], (double)mx[1],
										(double)mx[2], (double)a2[0], (double)a2[1], (double)a2[2], (double)b2[0],
										(double)b2[1], (double)b2[2], (double)bordX);
							std::fflush(stdout);
						}
					}
					// ── (Q15) LES COULEURS DE SOMMET SONT RECENSEES ET DITES ──────────
					// Un objet TripoSR arrive avec COLOR_0 et sans UV. Le nuanceur fait
					// `vColor = aColor * uObj.tint` : la couleur du materiau y est
					// MULTIPLIEE par celle des sommets, jamais posee a sa place. On le
					// constate a l'import, on le NOMME dans le fil, et l'interrupteur
					// existe -- plutot que de laisser l'utilisateur devant un objet qui
					// ne repond pas comme les autres sans que rien ne le dise.
					// ⚠️ LA GEOMETRIE EST DANS LES ENFANTS, PAS DANS LE NOEUD POSE. Un
					//    modele importe est un CONTENEUR sans sommets ; ses maillages sont
					//    ses descendants. La premiere version ne regardait que `nes` et
					//    n'a rien trouve sur un objet qui portait pourtant 15 200 sommets
					//    colores -- un compte a zero qui disait « rien a signaler ».
					//    Le parcours est celui que le LOT fait dix lignes plus bas, pour la
					//    meme raison et par les memes portes.
					{
						int32 colores = 0, avecCouleurs = 0;
						const int32 nT2 = demo::Demo3DHostNodeCount();
						for (usize i = 0; i < nes.Size(); ++i) {
							const int32 c = NkVcDetecter(nes[i]);
							if (c > 0) {
								colores += c;
								++avecCouleurs;
							}
							for (int32 q = 0; q < nT2; ++q) {
								if (q == nes[i] || demo::Demo3DHostNodeDeleted(q))
									continue;
								int32 pere = demo::Demo3DHostNodeParent(q);
								bool descend = false;
								for (int32 garde = 0; pere >= 0 && garde < 16 && !descend; ++garde) {
									descend = (pere == nes[i]);
									pere = demo::Demo3DHostNodeParent(pere);
								}
								if (!descend)
									continue;
								const int32 ce = NkVcDetecter(q);
								if (ce > 0) {
									colores += ce;
									++avecCouleurs;
								}
							}
						}
						if (avecCouleurs > 0) {
							// ── LA TEINTE DOIT ETRE BLANCHE (25/09) ──────────────────────
							// MESURE : les couleurs de sommet de TripoSR valent deja 0,61 a
							// 0,85 de la luminance de l'image source (le modele assombrit).
							// Le nuanceur fait ensuite `vColor = aColor * uObj.tint` : une
							// teinte de materiau qui n'est pas blanche les assombrit une
							// SECONDE fois, et l'objet arrive « tres sombre » dans la scene.
							// Tant que l'interrupteur « utiliser les couleurs du maillage »
							// est allume, la teinte est donc posee a blanc -- par la MEME
							// porte que le panneau, pour que le reglage soit celui qu'on voit.
							const float32 blanc[3] = {1.f, 1.f, 1.f};
							for (usize i = 0; i < nes.Size(); ++i)
								demo::Demo3DHostSetMeshTint(nes[i], blanc);
							for (usize i = 0; i < NkVcTable().Size(); ++i)
								demo::Demo3DHostSetMeshTint(NkVcTable()[i].noeud, blanc);
							char mv[300];
							snprintf(mv, sizeof(mv),
									 "Ce maillage porte des COULEURS PAR SOMMET (%d sommets sur %d piece(s)) et "
									 "aucune UV. Une couleur de materiau posee dessus serait multipliee par "
									 "elles : l'interrupteur « utiliser les couleurs du maillage » les eteint.",
									 (int)colores, (int)avecCouleurs);
							(void)NkAiPousser(st, NkModelerState::AiType::Note, mv);
							std::printf("[crea] COULEURS DE SOMMET : %d sommet(s) colore(s) sur %d piece(s) ; "
										"interrupteur allume\n",
										(int)colores, (int)avecCouleurs);
							std::fflush(stdout);
						}
					}
					// ── (Q19.1) CHAQUE PARTIE POSEE PORTE UNE FENTE DE MATERIAU ──────
					// Rodolf, capture 060654 : « le personnage importe n'a AUCUN materiau
					// dans sa configuration » -- le panneau des proprietes n'a alors rien
					// a montrer, et il ne peut RIEN changer. Les pieces de famille en ont
					// une depuis toujours (`Demo3DHostProjMatAssign`) ; l'objet de la voie
					// image n'en avait aucune, parce que son glTF n'en declare aucun.
					// ⚠️ LE MATERIAU EST BLANC, ET C'EST LE SEUL CHOIX JUSTE ICI : le
					//    maillage porte ses propres couleurs de sommet, que le nuanceur
					//    MULTIPLIE par la teinte. Toute autre couleur les assombrirait ou
					//    les teinterait -- exactement le defaut de Q15. Blanc = « la fente
					//    existe et ne change rien tant que tu n'y touches pas ».
					{
						const char *nomMat = NkCreaNomObjet();
						int32 slotObj = NkCreaMatDuProjet(nomMat);
						if (slotObj < 0) {
							slotObj = demo::Demo3DHostProjMatCreate();
							if (slotObj >= 0) {
								const float32 blanc[3] = {1.f, 1.f, 1.f};
								demo::Demo3DHostProjMatSetName(slotObj, nomMat);
								demo::Demo3DHostProjMatSetParams(slotObj, blanc, 0.6f, 0.f);
							}
						}
						int32 nAff = 0;
						if (slotObj >= 0) {
							const int32 nT4 = demo::Demo3DHostNodeCount();
							for (usize i = 0; i < nes.Size(); ++i) {
								demo::Demo3DHostProjMatAssign(nes[i], slotObj);
								++nAff;
								for (int32 q = 0; q < nT4; ++q) {
									if (q == nes[i] || demo::Demo3DHostNodeDeleted(q))
										continue;
									if (demo::Demo3DHostNodeParent(q) != nes[i])
										continue;
									demo::Demo3DHostProjMatAssign(q, slotObj);
									++nAff;
								}
							}
						}
						std::printf("[crea] MATERIAU « %s » (blanc) pose sur %d noeud(s) : le panneau peut "
									"desormais en changer\n",
									nomMat, (int)nAff);
						std::fflush(stdout);
					}
					// ── (25/09) L'OBJET PORTE LE NOM DE LA DEMANDE, PAS « geometry_0 » ──
					// Rodolf a photographie un noeud nomme `geometry_0`, sans parent ni
					// enfant : le nom venait du glTF de TripoSR, qui ne sait rien de ce
					// qu'on lui a demande. Un objet qu'on ne retrouve pas dans la
					// hierarchie n'est pas livre. Le nom est celui que la chaine a deja
					// reconnu dans la demande (`nomTete`, le mot de Rodolf), sinon le nom
					// de scene, sinon « genere ».
					{
						NkCreaEtat &EN = NkCrea();
						// ⚠️ TROIS SOURCES, DANS CET ORDRE, ET LA TROISIEME COMPTE. `nomTete`
						//    n'est rempli que si le LEXIQUE a reconnu une famille : pour « cet
						//    arbre », il est vide, et le repli tombait sur « genere » --
						//    c'est-a-dire le defaut que je venais de corriger. Le nom de SCENE
						//    du dernier lot pose est, lui, toujours le mot de la demande.
						const char *nomLot = (EN.nLots > 0) ? EN.lots[EN.nLots - 1].scene : "";
						const char *nomObjet = EN.nomTete[0] ? EN.nomTete
											  : (nomLot && nomLot[0] ? nomLot
												 : (EN.familleReconnue[0] ? EN.familleReconnue : "genere"));
						for (usize i = 0; i < nes.Size(); ++i) {
							if (nes[i] < NkModelerState::kMaxNodeNames)
								snprintf(st.customNames[nes[i]], 24, "%s", nomObjet);
						}
						// ET SES MORCEAUX LUI APPARTIENNENT : un descendant garde son nom
						// mais prend le nom de l'objet en prefixe, pour qu'on lise a qui il
						// est. ⚠️ On ne REPARENTE rien : l'import a deja construit la
						// hierarchie, et la refaire ici serait une seconde mecanique.
						const int32 nT3 = demo::Demo3DHostNodeCount();
						int32 nEnfants = 0;
						for (usize i = 0; i < nes.Size(); ++i)
							for (int32 q = 0; q < nT3; ++q) {
								if (q == nes[i] || demo::Demo3DHostNodeDeleted(q))
									continue;
								if (demo::Demo3DHostNodeParent(q) != nes[i])
									continue;
								if (q < NkModelerState::kMaxNodeNames) {
									char sousNom[24];
									snprintf(sousNom, sizeof(sousNom), "%s_%d", nomObjet, (int)nEnfants);
									snprintf(st.customNames[q], 24, "%s", sousNom);
								}
								++nEnfants;
							}
						demo::Demo3DHostHierarchyResync();
						std::printf("[crea] TRIPOSR nomme « %s » (%d morceau(x) sous lui)\n",
									nomObjet, (int)nEnfants);
						std::fflush(stdout);
					}
					// LE LOT : sans lui, l'objet pose ne serait pas annulable, et le
					// contrat « Ctrl+Z retire ce que la conversation a mis » serait faux
					// pour la moitie des voies.
					NkCreaLot lot;
					lot.objetsAvant = NkCreaCompterObjets() - poses;
					snprintf(lot.scene, sizeof(lot.scene), "%s", "genere");
					snprintf(lot.demande, sizeof(lot.demande), "voie (%s)", G.voie);
					// ⚠️ LES ENFANTS ENTRENT DANS LE LOT, PAS SEULEMENT LE CONTENEUR.
					//    Un modele importe est un conteneur SANS geometrie ; ses
					//    maillages sont ses enfants. N'inscrire que le conteneur
					//    retirait le porte-manteau et laissait les manteaux : l'objet
					//    aurait survecu au Ctrl+Z, alors que le compteur d'objets, lui,
					//    serait revenu a son chiffre d'avant -- un vert par omission.
					//    Les enfants d'abord, le conteneur ensuite : NkCreaAnnuler
					//    parcourt le lot a l'envers, donc jamais un parent avant ses
					//    enfants.
					{
						const int32 nT = demo::Demo3DHostNodeCount();
						for (usize i = 0; i < nes.Size(); ++i) {
							for (int32 q = 0; q < nT && lot.nNoeuds < kCreaMaxParties; ++q) {
								if (q == nes[i] || demo::Demo3DHostNodeDeleted(q))
									continue;
								int32 pere = demo::Demo3DHostNodeParent(q);
								bool descend = false;
								for (int32 garde = 0; pere >= 0 && garde < 16 && !descend; ++garde) {
									descend = (pere == nes[i]);
									pere = demo::Demo3DHostNodeParent(pere);
								}
								if (!descend)
									continue;
								lot.noeuds[lot.nNoeuds] = q;
								NkCreaCopieNom(lot.noms[lot.nNoeuds], 24,
											   (q < NkModelerState::kMaxNodeNames) ? st.customNames[q] : "genere");
								++lot.nNoeuds;
							}
							if (lot.nNoeuds < kCreaMaxParties) {
								lot.noeuds[lot.nNoeuds] = nes[i];
								NkCreaCopieNom(lot.noms[lot.nNoeuds], 24,
											   (nes[i] < NkModelerState::kMaxNodeNames) ? st.customNames[nes[i]]
																					   : "genere");
								++lot.nNoeuds;
							}
						}
					}
					if (E.nLots == kCreaMaxLots) {
						for (int32 i = 1; i < kCreaMaxLots; ++i)
							E.lots[i - 1] = E.lots[i];
						--E.nLots;
					}
					E.lots[E.nLots++] = lot;
					E.aRefaire = false;
					E.dernierLot = ++E.compteurLots;
					if (FILE *f = fopen("logs/crea_mesure.txt", "ab")) {
						fprintf(f,
								"TRIPOSR %d voie=%s noeuds=%d hauteur_avant=%.4f hauteur_apres=%.4f echelle=%.4f "
								"debout=%d\n",
								(int)E.dernierLot, G.voie, (int)poses, (double)hAv, (double)hAp, (double)kEch,
								debout ? 1 : 0);
						fclose(f);
					}
					std::printf("[crea] TRIPOSR pose : %d noeud(s), hauteur %.3f -> %.3f m (echelle %.3f, "
								"cible prise sur %s), bas a y=%.4f, debout=%d ; Ctrl+Z le retire.\n",
								(int)poses, (double)hAv, (double)hAp, (double)kEch,
								origineCible,
								(double)basApres, debout ? 1 : 0);
					std::fflush(stdout);
					// LES VUES REPARTENT : les premieres ont photographie l'assemblage
					// SEUL, puisque la generation dure une minute. Sans cette seconde
					// serie, aucune image de l'application ne montrerait ce que Q9.4
					// ajoute -- et le rapport parlerait d'un resultat que personne n'a
					// vu. Le prefixe change, donc rien n'est ecrase.
					if (const char *v = std::getenv("NK_CREA_VUES"))
						if (*v) {
							char p2[200];
							snprintf(p2, sizeof(p2), "%s_avec", v);
							snprintf(E.vuesPrefixe, sizeof(E.vuesPrefixe), "%s", p2);
							E.vuesToutVoir = true;
							// La boite a cadrer : l'objet POSE -- donc le dernier lot, deja
							// a sa place -- reuni a l'assemblage. Les deux ensemble, c'est
							// le sujet de la photo, et leur echelle relative le propos.
							// ⚠️ PAS `mn`/`mx` : ce sont les bornes D'AVANT la pose, et
							//    cadrer dessus montrerait l'endroit ou l'objet N'EST PLUS.
							float32 u[3], w[3];
							if (NkCreaBoiteDernierLot(u, w)) {
								for (int32 k = 0; k < 3; ++k) {
									E.vuesBoite[k] = (aAssemblage && lm[k] < u[k]) ? lm[k] : u[k];
									E.vuesBoite[3 + k] = (aAssemblage && lM[k] > w[k]) ? lM[k] : w[k];
								}
								E.vuesBoiteValide = true;
							}
							E.vuesEtape = 0;
							E.vuesAttente = 3;
						}
				}
			}
			snprintf(m, sizeof(m),
					 importes <= 0
						 ? "Voie (%s) : le fichier genere en %.0f s (%u images) n'a pas pu etre importe (%d carte)."
						 : (poses > 0 ? "Voie (%s) : maillage genere en %.0f s (la fenetre est restee vivante : %u "
										"images), POSE DANS LA SCENE a cote de l'assemblage, mis a son echelle "
										"(%d carte gardee au navigateur). Ctrl+Z le retire."
									  : "Voie (%s) : maillage genere en %.0f s (%u images), carte ajoutee au "
										"navigateur (%d) -- il n'a PAS pu etre pose dans la scene. Glissez-la."),
					 G.voie, (double)G.envoi.Secondes(), (unsigned)G.envoi.Images(), (int)(st.BrowserCount() - avant));
			(void)NkAiPousser(st, importes > 0 ? NkModelerState::AiType::Note : NkModelerState::AiType::Refus, m);
			std::printf("[crea] %s\n", m);
			std::fflush(stdout);
		}

		// =====================================================================
		//  7. LA CONVERSATION : lancer, recolter, corriger
		// =====================================================================
		inline void NkCreaPreparer() {
			NkCreaEtat &E = NkCrea();
			if (E.prepare)
				return;
			E.prepare = true;
			E.dorsal.nom = NkString("assistant-creation");
			E.dorsal.invitePath = NkString("logs/nk3dmodeler_crea_invite.txt");
			E.dorsal.sortiePath = NkString("logs/nk3dmodeler_crea_reponse.txt");
			// ⚠️ UN BUDGET DE JETONS A PART, ET C'EST LA RAISON DE CE SECOND DORSAL.
			//    Le dorsal des verbes borne la reponse a 64 jetons -- une ligne. Un
			//    document de dix parties en demande ~400. Le meme gabarit, avec le
			//    budget en troisieme argument ; `NK_IA_CREA_CMD` le remplace.
			if (const char *g = std::getenv("NK_IA_CREA_CMD"))
				if (*g)
					E.dorsal.gabarit = NkString(g);
			if (E.dorsal.gabarit.Length() == 0) {
				const char *py = std::getenv("NK_IA_PYTHON");
				// LE MODELE DE CREATION EST UN REGLAGE, ET SON DEFAUT VIENT D'UNE
				// MESURE : sur le jeu d'epreuve, meme invite, qwen2.5:7b-instruct a
				// rendu 0/8 objets verts sur les cinq criteres, qwen2.5-coder:7b 3/8
				// (course 3, 21/09). n = 8 : c'est un indice, pas une loi.
				// `NK_IA_CREA_MODELE` le remplace ; les verbes d'edition gardent le leur.
				const char *mo = std::getenv("NK_IA_CREA_MODELE");
				char buf[512];
				snprintf(buf, sizeof(buf), "%s \"Tools/Genia/ia_verbe.py\" \"{invite}\" \"{sortie}\" 1200 \"%s\"",
						 (py && *py) ? py : "python", (mo && *mo) ? mo : "qwen2.5-coder:7b");
				E.dorsal.gabarit = NkString(buf);
			}
			if (const char *t = std::getenv("NK_CREA_TOURS"))
				E.toursMax = (int32)std::atoi(t);
			if (E.toursMax < 0)
				E.toursMax = 0;
			NkDirectory::CreateRecursive("logs");
		}

		/// L'INTENTION DE CREER, lue sur le premier mot. ⚠️ Une regle ECRITE, pas un
		/// classifieur : les verbes qui ne peuvent QUE creer. « fais » et « ajoute »
		/// n'y sont pas -- « fais un biseau », « ajoute une boucle » sont des
		/// editions -- ils passent par le contrat d'edition, dont le « aucune »
		/// renvoie ensuite ici.
		inline bool NkCreaIntention(const char *t) {
			if (!t)
				return false;
			while (*t == ' ')
				++t;
			static const char *const kV[] = {"modelise", "modélise", "modeliser", "modéliser", "cree ", "crée ",
											 "creer ", "créer ", "construis", "construire", "genere", "génère",
											 "generer", "générer", "fabrique", "sculpte un", "sculpte une"};
			for (const char *v : kV) {
				const size_t l = strlen(v);
				bool egal = true;
				for (size_t i = 0; i < l && egal; ++i) {
					char a = t[i];
					if (a >= 'A' && a <= 'Z')
						a = (char)(a - 'A' + 'a');
					if (a != v[i])
						egal = false;
				}
				if (egal)
					return true;
			}
			return false;
		}

		/// Le texte tape EST-il deja un document (« partie ... ») ? On le pose alors
		/// sans deranger le modele, comme un verbe tape se passe de traduction.
		inline bool NkCreaEstDocument(const char *t) {
			if (!t)
				return false;
			while (*t == ' ')
				++t;
			return strncmp(t, "partie ", 7) == 0 || strncmp(t, "scene ", 6) == 0;
		}

		/// LANCE la demande de creation. `dorsalOnglet` sert pour un onglet distant ;
		/// l'onglet local prend le dorsal de creation (budget de jetons).
		/// Rend la famille (porte, table, maison, revolution) ou vide ; `style` recoit
		/// « chinois » si la demande le dit.
		inline bool NkCreaReconnaitreFamille(const char *demande, char *famille, uint32 capF, char *style, uint32 capS) {
			famille[0] = 0;
			if (style)
				style[0] = 0;
			char t[400];
			NkCreaNormaliser(demande, t, sizeof(t));
			static const char *const kVides[] = {
				"bonjour", "salut", "hello", "je", "j", "veux", "voudrais", "aimerais", "souhaite", "peux", "tu", "me",
				"moi", "nous", "vous", "modelise", "modeliser", "modelisez", "fais", "fait", "faire", "cree", "creer",
				"creez", "construis", "construire", "genere", "generer", "dessine", "dessiner", "donne", "ajoute",
				"un", "une", "des", "le", "la", "les", "l", "d", "du", "de", "mon", "ma", "mes", "s", "il", "te",
				"plait", "stp", "svp", "petit", "petite", "grand", "grande", "gros", "grosse", "beau", "belle",
				"joli", "jolie", "vieux", "vieille", "ancien", "ancienne", "vieil", "nouveau", "nouvelle", "simple",
				"a", "an", "the", "make", "create", "build", "model", "please", "i", "want", "me", "some", "big",
				"small", "old", "new", "nice", "tres", "super", "vraie", "vrai",
				// ⚠️ LES DEMONSTRATIFS MANQUAIENT (25/09). « modelise moi CETTE table »
				//    s'arretait sur « cette » : le nom de tete etait un determinant, la
				//    famille n'etait pas reconnue, et la table partait en assemblage
				//    libre -- 5 boites sans nom la ou la famille en donne 6 nommees.
				//    C'est aussi ce que Q12 demande : une demande qui DESIGNE l'image
				//    (« ceci », « cet objet », « ce qui est en image jointe ») ne doit
				//    pas perdre son nom de tete en chemin.
				"ce", "cet", "cette", "ces", "ceci", "cela", "ca", "celui", "celle",
				"this", "that", "these", "those", "it", "here"};
			static const struct {
					const char *mot;
					const char *famille;
			} kLex[] = {
				{"porte", "porte"},		{"portail", "porte"},	   {"portique", "porte"},	  {"torii", "porte"},
				{"paifang", "porte"},	{"gate", "porte"},		   {"door", "porte"},		  {"doorway", "porte"},
				{"table", "table"},		{"bureau", "table"},	   {"desk", "table"},		  {"guéridon", "table"},
				{"gueridon", "table"},	{"maison", "maison"},	   {"villa", "maison"},		  {"pavillon", "maison"},
				{"house", "maison"},	{"home", "maison"},		   {"cottage", "maison"},	  {"chalet", "maison"},
				{"bungalow", "maison"}, {"demeure", "maison"},	   {"batisse", "maison"},	  {"verre", "revolution"},
				{"gobelet", "revolution"}, {"vase", "revolution"}, {"bol", "revolution"},	  {"bouteille", "revolution"},
				{"tasse", "revolution"}, {"coupe", "revolution"},  {"carafe", "revolution"},  {"cruche", "revolution"},
				{"jarre", "revolution"}, {"amphore", "revolution"}, {"flacon", "revolution"}, {"pot", "revolution"},
				{"glass", "revolution"}, {"cup", "revolution"},	   {"bowl", "revolution"},	  {"bottle", "revolution"},
				{"mug", "revolution"},	{"jar", "revolution"},	   {"goblet", "revolution"},  {"verres", "revolution"},
			};
			// le premier mot « plein »
			const char *c = t;
			char mot[48];
			while (*c) {
				while (*c == ' ')
					++c;
				if (!*c)
					break;
				uint32 n = 0;
				while (*c && *c != ' ') {
					if (n + 1u < sizeof(mot))
						mot[n++] = *c;
					++c;
				}
				mot[n] = 0;
				bool vide = false;
				for (const char *v : kVides)
					if (strcmp(v, mot) == 0)
						vide = true;
				if (vide)
					continue;
				for (const auto &L : kLex)
					if (strcmp(L.mot, mot) == 0) {
						snprintf(famille, capF, "%s", L.famille);
						snprintf(NkCrea().nomTete, sizeof(NkCrea().nomTete), "%s", mot);
						break;
					}
				break; // le nom de tete est trouve : reconnu ou pas, on s'arrete
			}
			if (!famille[0])
				return false;
			if (style && strcmp(famille, "porte") == 0) {
				static const char *const kChinois[] = {"chinois", "chinoise", "torii", "pagode", "japonais", "japonaise",
													   "asiatique", "paifang", "temple", "chinese", "japanese"};
				for (const char *k : kChinois)
					if (strstr(t, k))
						snprintf(style, capS, "chinois");
			}
			return true;
		}

		/// Retire de `ligne` les champs que L'OUTIL remplit lui-meme, avec leur
		/// valeur `<...>` (les valeurs des formulaires ne portent jamais d'espace).
		/// ⚠️ POURQUOI PLUTOT QUE RECOPIER LA LIGNE DANS L'APPLICATION : la
		///    bibliotheque est seule a dire quels champs existent et quelles valeurs
		///    sont permises ; l'application, elle, sait lesquels elle s'approprie -- le
		///    niveau de detail vient de la demande (Q8), les dimensions de la maison de
		///    sa surface (Q9.2). Une ligne recopiee a la main se perime au premier
		///    champ ajoute : c'est exactement le defaut paye le 21/09.
		inline void NkCreaRetirerChamps(char *ligne, const char *const *champs, int32 n) {
			char out[400];
			uint32 o = 0;
			const char *c = ligne;
			while (*c) {
				while (*c == ' ')
					++c;
				const char *mot = c;
				while (*c && *c != ' ')
					++c;
				const usize lm = (usize)(c - mot);
				if (!lm)
					break;
				bool aRetirer = false;
				for (int32 k = 0; k < n; ++k)
					if (strlen(champs[k]) == lm && strncmp(mot, champs[k], lm) == 0)
						aRetirer = true;
				if (aRetirer) {
					while (*c == ' ')
						++c;
					while (*c && *c != ' ')
						++c; // la valeur <...> qui suit part avec le champ
					continue;
				}
				if (o && o + 1u < sizeof(out))
					out[o++] = ' ';
				for (usize k = 0; k < lm && o + 1u < sizeof(out); ++k)
					out[o++] = mot[k];
			}
			out[o] = 0;
			snprintf(ligne, 400, "%s", out);
		}

		/// LA LIGNE DU FORMULAIRE, SEULE. Elle est extraite parce que DEUX endroits en
		/// ont besoin : l'invite (ci-dessous) et la LECTURE de la reponse, qui doit
		/// savoir quels mots sont des champs de cette famille.
		inline void NkCreaLigneFormulaire(char *ligne, uint32 cap, const char *famille) {
			ligne[0] = 0;
			if (!NkFamFormulaire(famille, ligne, cap)) {
				// `revolution` : la liste des silhouettes est CONSTRUITE depuis la table,
				// jamais recopiee -- une valeur ajoutee a la table apparait ici seule.
				int32 ns = 0;
				const NkCreaSilhouette *S = NkCreaSilhouettes(ns);
				snprintf(ligne, cap, "famille revolution silhouette <");
				for (int32 k = 0; k < ns; ++k) {
					strncat(ligne, S[k].nom, cap - strlen(ligne) - 2u);
					if (k + 1 < ns)
						strncat(ligne, "|", cap - strlen(ligne) - 2u);
				}
				strncat(ligne, "> largeur <metres> hauteur <metres> paroi <metres, 0 si plein>",
						cap - strlen(ligne) - 2u);
			}
			// LE DETAIL N'EST JAMAIS DEMANDE AU MODELE : il vient de la demande (Q8).
			static const char *const kDetail[] = {"detail"};
			NkCreaRetirerChamps(ligne, kDetail, 1);
			if (strcmp(famille, "maison") == 0) {
				// LES DIMENSIONS DE LA MAISON VIENNENT DE LA SURFACE (Q9.2) : les demander
				// au modele serait lui faire ecrire des nombres qu'on jette.
				// ⚠️ ET `etages` PORTAIT UNE COLLISION : le lecteur range `etages`,
				//    `battants`, `nombre` et `pieces` dans le MEME champ `nombre`, dont la
				//    maison se sert comme d'un nombre de PIECES (22 m² chacune). Un modele
				//    qui ecrivait `etages 2` faisait donc calculer une surface de 44 m².
				//    Le champ que la maison veut vraiment s'appelle `pieces`, et il est a
				//    l'application : on le declare ici, une fois.
				static const char *const kSurface[] = {"largeur", "profondeur", "etages", "fenetres"};
				NkCreaRetirerChamps(ligne, kSurface, 4);
				strncat(ligne, " pieces <nombre de pieces si la demande le dit, sinon 0>",
						cap - strlen(ligne) - 2u);
			}
		}

		/// LE FORMULAIRE D'UNE FAMILLE : c'est TOUT ce que le modele recoit quand la
		/// famille est reconnue. Une ligne a remplir ; aucune grammaire de parties.
		/// ⚠️ LA LIGNE VIENT DE LA BIBLIOTHEQUE (22/09, Q10.1), plus d'un gabarit
		///    recopie ici. Seule `revolution` reste ecrite dans l'application : la
		///    bibliotheque ne la construit pas (elle passe par une partie `forme
		///    revolution`), et ses silhouettes vivent dans `NkCreaSilhouettes`.
		/// ⚠️ ET L'AIDE DIT L'EFFET DE CHAQUE VALEUR, pas seulement son nom. MESURE du
		///    22/09 sur le jeu revolution (3 objets x 3 courses, qwen2.5-coder:7b,
		///    `Tools/Genia/mesure_formulaire.py`) : avec la seule liste des noms, la
		///    silhouette PLAUSIBLE sortait **0 fois sur 9** -- un verre et un bol en
		///    « conique », un vase en « vase » (mot pris dans la demande, hors table).
		///    En donnant l'effet de chaque forme : permise 6/9 -> **9/9**, plausible
		///    0/9 -> **6/9**. Nommer les valeurs ne suffit pas ; il faut les DECRIRE.
		inline void NkCreaEcrireFormulaire(char *dst, uint32 cap, const char *famille, const char *demande) {
			char ligne[400] = {0};
			char aide[1200] = {0};
			NkCreaLigneFormulaire(ligne, sizeof(ligne), famille);
			if (strcmp(famille, "porte") == 0 || strcmp(famille, "portail") == 0) {
				snprintf(aide, sizeof(aide),
						 "style chinois pour une porte chinoise, un torii, une pagode ; largeur et hauteur de "
						 "l'OUVERTURE entiere (une porte d'entree : 0.9 x 2.1 ; un portail chinois : 2.4 x 3.2).");
			} else if (strcmp(famille, "table") == 0) {
				snprintf(aide, sizeof(aide),
						 "une table a manger : 1.6 x 0.9 x 0.75 ; une table basse : 1.1 x 0.6 x 0.45 ; "
						 "un bureau : 1.4 x 0.7 x 0.75.");
			} else if (strcmp(famille, "maison") == 0) {
				snprintf(aide, sizeof(aide),
						 "toit plat pour une villa moderne ou un toit terrasse ; deux_pans sinon. Les DIMENSIONS ne "
						 "sont PAS a donner : l'outil les tire de la surface demandee.");
			} else {
				// ⚠️ L'AIDE DE LA REVOLUTION DECRIT CHAQUE SILHOUETTE. C'est le correctif
				//    mesure : sans ces effets, le modele choisit par le NOM DE L'OBJET.
				snprintf(aide, sizeof(aide),
						 "largeur et hauteur REELLES de l'objet (un verre : 0.08 x 0.12, paroi 0.003 ; un bol : "
						 "0.16 x 0.08, paroi 0.004 ; un vase : 0.2 x 0.35, paroi 0.006).\n"
						 "CHOISIS la silhouette par sa FORME, jamais par le nom de l'objet :");
				int32 ns = 0;
				const NkCreaSilhouette *S = NkCreaSilhouettes(ns);
				for (int32 k = 0; k < ns; ++k) {
					char l[160];
					snprintf(l, sizeof(l), "\n- %s : %s", S[k].nom, S[k].effet);
					strncat(aide, l, sizeof(aide) - strlen(aide) - 2u);
				}
			}
			snprintf(dst, cap,
					 "Tu remplis UN formulaire. La demande est reconnue comme un objet de la famille « %s ».\n"
					 "Reponds par UNE SEULE ligne, exactement de cette forme, en remplacant chaque <...> par une "
					 "valeur :\n%s\n%s\nRien d'autre : pas de phrase, pas de partie.\n\nDemande : %s\nLigne :",
					 famille, ligne, aide, demande ? demande : "");
		}

		/// LE DOCUMENT D'UNE FAMILLE RECONNUE, tire de la reponse : on n'y prend QUE la
		/// ligne `famille` ; la famille est FORCEE a celle reconnue ; un formulaire
		/// non rempli retombe sur les valeurs de la famille, et on le DIT.
		inline void NkCreaDocFamille(const char *reponse, NkCreaDoc &d, bool &rempli) {
			NkCreaEtat &E = NkCrea();
			rempli = false;
			const char *ligneModele = nullptr;
			char doc2[500] = {0}; ///< la ligne de repli, sans le mot `famille`
			for (const char *c = reponse; c && *c;) {
				while (*c == ' ' || *c == '\t' || *c == '`' || *c == '-' || *c == '*' || *c == '\n' || *c == '\r')
					++c;
				if (strncmp(c, "famille ", 8) == 0 || strncmp(c, "Famille ", 8) == 0) {
					ligneModele = c;
					break;
				}
				while (*c && *c != '\n')
					++c;
			}
			// ── LE PREFIXE `famille ` N'EST PAS LA REPONSE, IL EST LA CEREMONIE ────
			// MESURE du 22/09 (`Tools/Genia/mesure_formulaire.py`, critere C_lu) : sur
			// « un bol », le 7B repond deux fois sur trois
			// `silhouette evase largeur 0.16 hauteur 0.08 paroi 0.004` -- juste, complet,
			// mais SANS le mot `famille`. Tout etait jete, et l'objet retombait sur les
			// valeurs par defaut, c'est-a-dire un VERRE a la place d'un bol.
			// ⚠️ ET LE MOT NE SERVAIT DEJA A RIEN : la famille ecrite par le modele est
			//    REMPLACEE par celle que la chaine a reconnue, juste en dessous. On
			//    exigeait donc un mot qu'on s'appretait a effacer.
			// La ligne de repli est acceptee sur PREUVE : son premier mot doit etre un
			// CHAMP du formulaire de cette famille -- et les champs sont lus dans le
			// formulaire lui-meme, jamais recopies ici.
			if (!ligneModele) {
				char form[400] = {0};
				NkCreaLigneFormulaire(form, sizeof(form), E.familleReconnue);
				for (const char *c = reponse; c && *c;) {
					while (*c == ' ' || *c == '\t' || *c == '`' || *c == '-' || *c == '*' || *c == '\n' || *c == '\r')
						++c;
					const char *mot = c;
					while (*c && *c != ' ' && *c != '\n' && *c != '\r')
						++c;
					const usize lm = (usize)(c - mot);
					// ce premier mot est-il un champ du formulaire ?
					bool champ = false;
					for (const char *f = form; *f && !champ;) {
						while (*f == ' ')
							++f;
						const char *fm = f;
						while (*f && *f != ' ')
							++f;
						const usize lf = (usize)(f - fm);
						if (lf == lm && lm > 0u && fm[0] != '<' && strncmp(fm, mot, lm) == 0)
							champ = true;
					}
					if (champ) {
						ligneModele = mot;
						break;
					}
					while (*c && *c != '\n')
						++c;
				}
				if (ligneModele) {
					// ici il n'y a PAS de nom de famille a sauter : la ligne commence
					// deja par un champ. On la copie telle quelle.
					uint32 n = 0;
					const char *q = ligneModele;
					while (*q && *q != '\n' && *q != '\r' && n + 1u < 500u)
						doc2[n++] = *q++;
					doc2[n] = 0;
					rempli = true;
					ligneModele = nullptr; // deja consommee
				}
			}
			static char doc[700];
			char ligne[500] = {0};
			if (doc2[0])
				snprintf(ligne, sizeof(ligne), "%s", doc2);
			if (ligneModele) {
				// la famille ecrite par le modele est REMPLACEE par celle reconnue
				const char *q = ligneModele + 8;
				while (*q == ' ')
					++q;
				while (*q && *q != ' ' && *q != '\n')
					++q; // saute le nom de famille du modele
				uint32 n = 0;
				while (*q && *q != '\n' && *q != '\r' && n + 1u < sizeof(ligne))
					ligne[n++] = *q++;
				ligne[n] = 0;
				rempli = true;
			}
			// ── UN STYLE EXOTIQUE DOIT ETRE DEMANDE (25/09) ───────────────────────
			// MESURE : « modelise moi cette porte » a rendu un PORTAIL CHINOIS de
			// 4,66 m (21 pieces). Le modele ecrit `style chinois` parce que l'aide du
			// formulaire nomme trois fois le chinois (« porte chinoise, torii,
			// pagode ») : le mot le plus saillant de l'aide l'emporte sur le defaut.
			// La chaine sait deja lire ce style DANS LA DEMANDE (`styleReconnu`).
			// Sans trace dans la demande, le style du modele ne peut pas introduire
			// l'exotique : on le retire de sa ligne, et le defaut de la famille joue.
			// ⚠️ LA REGLE NE VAUT QUE POUR PORTE/PORTAIL, et c'est ecrit : la maison
			//    n'a AUCUN lecteur de style dans la demande, donc lui appliquer la
			//    meme regle interdirait « toit plat » sans qu'aucun texte ne puisse
			//    l'autoriser. Elle s'etendra le jour ou le lexique lira « moderne »
			//    ou « terrasse ».
			if (!E.styleReconnu[0] &&
				(strcmp(E.familleReconnue, "porte") == 0 || strcmp(E.familleReconnue, "portail") == 0)) {
				if (char *q2 = strstr(ligne, "style chinois")) {
					memmove(q2, q2 + 13, strlen(q2 + 13) + 1u);
					NkCreaRefuser(d, "style « chinois » non demande : je prends « simple »%s", "");
				}
			}
			// le style reconnu PRIME s'il a ete trouve dans la demande
			// LE NOM DE LA SCENE EST LE MOT DE RODOLF (« verre », « villa »), pas le nom
			// interne de la famille : c'est ce qu'il lit dans la hierarchie.
			snprintf(doc, sizeof(doc), "scene %s\nfamille %s %s%s%s\n", E.nomTete[0] ? E.nomTete : E.familleReconnue,
					 E.familleReconnue, ligne,
					 E.styleReconnu[0] ? " style " : "", E.styleReconnu);
			NkCreaLire(doc, d);
		}

		inline bool NkCreaLancerPlan(NkModelerState &st, converse::NkIConverseBackend *dorsal);

		/// LA DESCRIPTION DE L'IMAGE GUIDE-T-ELLE LE PLAN ? NON PAR DEFAUT, et c'est
		/// une MESURE (21/09, Q7) : moondream a vu une « urne » dans la chaise de
		/// TripoSR et dans la theiere ; guide par lui, le plan a rendu une urne a la
		/// place d'une chaise. La description reste AFFICHEE dans le fil (on voit ce
		/// que la machine voit) ; l'image, elle, part a TripoSR, qui la reconstruit
		/// bien. `NK_CREA_IMAGE_GUIDE=1` rebranche la description dans le plan, le
		/// jour ou un modele de vision plus fort sera la (qwen2.5vl).
		inline bool NkCreaImageGuidePlan() {
			const char *v = std::getenv("NK_CREA_IMAGE_GUIDE");
			return v && v[0] && v[0] != '0';
		}

		/// ── LA PIECE JOINTE, COTE MODELEUR (Q7) ──────────────────────────────────
		/// LA porte que le panneau appelle quand l'utilisateur joint une image (`+`,
		/// coller, glisser-deposer) : le panneau fournit le fichier, le modeleur le
		/// consomme a la prochaine demande de creation. Chaine vide = la retirer.
		/// ⚠️ Le crochet de mesure `NK_CREA_IMAGE` entre ICI, pas plus loin.
		inline void NkCreaJoindreImage(const char *chemin) {
			NkCreaEtat &E = NkCrea();
			snprintf(E.imageJointe, sizeof(E.imageJointe), "%s", chemin ? chemin : "");
			E.descriptionImage[0] = 0;
		}

		inline bool NkCreaLancer(NkModelerState &st, const char *demande, int32 onglet,
								 converse::NkIConverseBackend *dorsalOnglet) {
			NkCreaPreparer();
			NkCreaEtat &E = NkCrea();
			if (E.envoi.EnCours()) {
				(void)NkAiPousser(st, NkModelerState::AiType::Refus,
								  "Une creation est deja en cours : attendez sa fin.");
				return false;
			}
			converse::NkIConverseBackend *dorsal = (onglet == 0) ? (converse::NkIConverseBackend *)&E.dorsal
																 : dorsalOnglet;
			if (!dorsal) {
				(void)NkAiPousser(st, NkModelerState::AiType::Refus,
								  "Aucun dorsal pour creer : choisissez l'onglet Local.");
				return false;
			}
			NkCreaCopieNom(E.demande, sizeof(E.demande), demande);
			E.tour = 0;
			E.docPrecedent[0] = 0;
			// ── (Q8) LA CREATION SUIT LE FOURNISSEUR, LE MODELE ET L'EFFORT DU CHAT ──
			// Capture 173536 : la puce disait « opus Max », le fil « Je demande au
			// modele LOCAL ». Deux defauts : (1) cote local, le gabarit de creation
			// portait SON modele en 4e argument (qwen2.5-coder:7b), qui PRIME sur celui
			// du chat -- le choix de la puce etait ignore ; (2) la note du fil etait un
			// texte fixe, qui disait « local » meme quand l'onglet etait Claude.
			// Desormais : le modele local est celui de la puce (`st.aiLocalModele`) ;
			// Claude recoit le modele et l'Effort de la puce (poses par main.cpp sur
			// le dorsal) ; et le fil nomme ce qui est REELLEMENT appele.
			if (onglet == 0) {
				const char *py = std::getenv("NK_IA_PYTHON");
				const char *forceCrea = std::getenv("NK_IA_CREA_MODELE");
				const char *mod = (forceCrea && *forceCrea) ? forceCrea
								  : (st.aiLocalModele[0] ? st.aiLocalModele : "qwen2.5-coder:7b");
				snprintf(E.modeleAppele, sizeof(E.modeleAppele), "%s", mod);
				if (!std::getenv("NK_IA_CREA_CMD")) {
					char buf[512];
					snprintf(buf, sizeof(buf), "%s \"Tools/Genia/ia_verbe.py\" \"{invite}\" \"{sortie}\" 1200 \"%s\"",
							 (py && *py) ? py : "python", mod);
					E.dorsal.gabarit = NkString(buf);
				}
				snprintf(E.qui, sizeof(E.qui), "au modele local %s (rien ne quitte cette machine)", mod);
			} else if (onglet == 1) {
				// LU SUR LE DORSAL QUI PARTIRA, pas sur l'etat du panneau : c'est ce que
				// le CLI recevra (`--model`, `--effort`), donc ce que le fil doit dire.
				const converse::NkConverseBackendClaude *cl = static_cast<const converse::NkConverseBackendClaude *>(dorsal);
				snprintf(E.modeleAppele, sizeof(E.modeleAppele), "%s", cl->modele.Data() ? cl->modele.Data() : "?");
				snprintf(E.qui, sizeof(E.qui), "a Claude, modele %s, effort %s (l'invite QUITTE cette machine)",
						 E.modeleAppele, cl->effort.Length() ? cl->effort.Data() : "(defaut du CLI)");
			} else
				snprintf(E.qui, sizeof(E.qui), "au dorsal %s", dorsal->Name());
			if (NkCreaVoieImage()) {
				// VOIE (c) : pas d'assemblage. Texte -> image -> TripoSR, hors du fil.
				if (NkGeniaLancer(st, true, demande, "c"))
					(void)NkAiPousser(st, NkModelerState::AiType::Note,
									  "Voie (c) : texte -> image (modele de diffusion local) -> detourage -> TripoSR. "
									  "La fenetre reste vivante.");
				return true;
			}
			if (E.imageJointe[0]) {
				// ── UNE IMAGE EST JOINTE : D'ABORD LA REGARDER ─────────────────────
				// Le modele de VISION local (moondream, deja present : aucun `pull`)
				// decrit l'objet ; sa description entre dans le plan. L'image part
				// AUSSI a TripoSR, une fois l'assemblage pose.
				// ⚠️ DEUX APPELS EN SERIE, PAS EN PARALLELE : la carte de 8 Go est
				//    disputee (Ilyana) ; deux modeles charges ensemble debordent.
				const char *py = std::getenv("NK_IA_PYTHON");
				const char *mv = std::getenv("NK_IA_VISION_MODELE");
				char g[900];
				snprintf(g, sizeof(g), "%s \"Tools/Genia/ia_verbe.py\" \"{invite}\" \"{sortie}\" 220 \"%s\" \"%s\"",
						 (py && *py) ? py : "python", (mv && *mv) ? mv : "moondream", E.imageJointe);
				E.vision.gabarit = NkString(g);
				E.vision.nom = NkString("vision");
				E.vision.invitePath = NkString("logs/nk3dmodeler_vision_invite.txt");
				E.vision.sortiePath = NkString("logs/nk3dmodeler_vision_reponse.txt");
				E.ongletAttente = onglet;
				E.dorsalAttente = dorsal;
				NkString pv;
				if (E.envoiVision.Lancer(&E.vision,
										 NkString("Describe this object in one short English paragraph: what it is, its "
												  "parts and their shapes, its material and colours, and its proportions "
												  "(height compared to width)."),
										 pv)) {
					(void)NkAiPousser(st, NkModelerState::AiType::Note,
									  "Image jointe : je la fais decrire par le modele de vision local, puis elle "
									  "guidera le plan ; elle partira ensuite a TripoSR.");
					std::printf("[crea] VISION : image '%s' -> %s\n", E.imageJointe, (mv && *mv) ? mv : "moondream");
					std::fflush(stdout);
					return true;
				}
				char m[240];
				snprintf(m, sizeof(m), "L'image jointe n'a pas pu etre regardee (%s) : le plan part sans elle.",
						 pv.CStr());
				(void)NkAiPousser(st, NkModelerState::AiType::Refus, m);
			}
			return NkCreaLancerPlan(st, dorsal);
		}

		/// LE PLAN, avec la description de l'image jointe s'il y en a une.
		inline bool NkCreaLancerPlan(NkModelerState &st, converse::NkIConverseBackend *dorsal) {
			NkCreaEtat &E = NkCrea();
			const char *demande = E.demande;
			static char avecImage[1100];
			if (E.descriptionImage[0] && NkCreaImageGuidePlan()) {
				snprintf(avecImage, sizeof(avecImage),
						 "%s\n(L'utilisateur a joint une IMAGE de l'objet. Un modele de vision y voit : %s -- "
						 "respecte les parties, les formes et les proportions de cette image.)",
						 E.demande, E.descriptionImage);
				demande = avecImage;
			}
			static char invite[22000];
			E.familleReconnue[0] = 0;
			E.styleReconnu[0] = 0;
			const bool sansReco = std::getenv("NK_CREA_SANS_RECONNAISSANCE") != nullptr;
			if (!sansReco && !NkCreaSansFamilles() &&
				NkCreaReconnaitreFamille(E.demande, E.familleReconnue, sizeof(E.familleReconnue), E.styleReconnu,
										 sizeof(E.styleReconnu))) {
				NkCreaEcrireFormulaire(invite, sizeof(invite), E.familleReconnue, demande);
				E.tour = E.toursMax; // un formulaire ne se « corrige » pas : il se remplit ou prend ses defauts
				std::printf("[crea] FAMILLE RECONNUE par la chaine : %s%s%s -> formulaire\n", E.familleReconnue,
							E.styleReconnu[0] ? " style " : "", E.styleReconnu);
				char m[240];
				snprintf(m, sizeof(m), "Famille reconnue : %s%s%s. Je demande seulement ses parametres.",
						 E.familleReconnue, E.styleReconnu[0] ? ", style " : "", E.styleReconnu);
				(void)NkAiPousser(st, NkModelerState::AiType::Note, m);
			} else if (!E.imageJointe[0] && !NkCreaVoieAssemblageForcee() && !NkCreaSansFamilles()) {
				// ── (Q10.0) AUCUNE FAMILLE : LA VOIE PAR L'IMAGE, PAR DEFAUT ──────
				// Pas d'assemblage de pavés : la demande part au generateur local
				// (texte -> image -> TripoSR), hors du fil d'affichage. Si le script
				// refuse (poids absents), `NkGeniaRecolter` retombe sur le plan libre
				// et le DIT.
				E.replisurAssemblage = true;
				E.dorsalRepli = dorsal;
				if (NkGeniaLancer(st, true, demande, "c")) {
					(void)NkAiPousser(st, NkModelerState::AiType::Note,
									  "Aucune famille reconnue : je passe par l'image (texte -> modele de diffusion "
									  "local -> TripoSR). La fenetre reste vivante ; si les poids manquent, je "
									  "retombe sur un assemblage de volumes et je vous le dirai.");
					std::printf("[crea] VOIE PAR DEFAUT : aucune famille reconnue -> voie (c) texte->image->TripoSR\n");
					std::fflush(stdout);
					return true;
				}
				E.replisurAssemblage = false;
				NkCreaEcrireInvite(invite, sizeof(invite), demande);
			} else
				NkCreaEcrireInvite(invite, sizeof(invite), demande);
			NkString pourquoi;
			if (!E.envoi.Lancer(dorsal, NkString(invite), pourquoi)) {
				char m[192];
				snprintf(m, sizeof(m), "L'assistant n'a pas pu etre appele : %s",
						 pourquoi.Data() ? pourquoi.Data() : "raison inconnue");
				(void)NkAiPousser(st, NkModelerState::AiType::Refus, m);
				return false;
			}
			{
				char m[300];
				snprintf(m, sizeof(m), "Je demande %s une famille ou un plan en parties nommees...", E.qui);
				(void)NkAiPousser(st, NkModelerState::AiType::Note, m);
			}
			std::printf("[crea] ENVOI : « %s » -> %s\n", demande, E.qui);
			std::fflush(stdout);
			return true;
		}

		/// Pose un document et raconte le resultat dans le fil. Rend le numero du lot.
		/// L'ENCOMBREMENT d'une partie avant la pose, en metres : sa taille, ou pour
		/// une revolution l'etendue de son profil.
		inline float32 NkCreaEtendue(const NkCreaPartie &pc) {
			int32 nf = 0;
			const NkCreaForme *F = NkCreaFormes(nf);
			if (pc.forme >= 0 && F[pc.forme].kind == -1 && !pc.tailleDonnee) {
				float32 rmax = 0.f, hmin = 1e30f, hmax = -1e30f;
				for (int32 k = 0; k + 1 < pc.nNombres; k += 2) {
					if (pc.profil[k] > rmax)
						rmax = pc.profil[k];
					if (pc.profil[k + 1] < hmin)
						hmin = pc.profil[k + 1];
					if (pc.profil[k + 1] > hmax)
						hmax = pc.profil[k + 1];
				}
				const float32 h = hmax - hmin;
				return 2.f * rmax > h ? 2.f * rmax : h;
			}
			float32 e = pc.taille[0];
			if (pc.taille[1] > e)
				e = pc.taille[1];
			if (pc.taille[2] > e)
				e = pc.taille[2];
			return e;
		}

		/// LA COHERENCE D'ECHELLE (Q7) : une partie dont le plus grand cote depasse
		/// 10 fois la MEDIANE des plus grands cotes des AUTRES parties est hors
		/// d'echelle -- le « mur geant » a cote du verre de 10 cm (capture du 21/09,
		/// 13 h 45). Rend le nombre de parties signalees ; `exclure` les retire de la
		/// pose, avec un refus NOMME dans d.refus.
		/// ⚠️ LA MEDIANE DES AUTRES, PAS LA MOYENNE DE TOUTES : avec deux parties, la
		///    moyenne contient deja le mur et le ratio s'ecrase ; et une maison (murs
		///    6 m, fenetre 1 m) reste sous le seuil (6 / ~1,7).
		static const float32 kCreaEchelleMax = 10.f;
		inline int32 NkCreaVerifierEchelle(NkCreaDoc &d, bool exclure, char (*motifs)[176], int32 capMotifs,
										   int32 &nMot) {
			int32 signalees = 0;
			// `NK_CREA_SANS_ECHELLE=1` : la MUTATION -- la garde se tait, et le juge
			// (epreuve_creation.py, C6) doit alors voir le mur que l'application
			// aurait refuse. Sans ce negatif, un C6 vert ne prouverait rien.
			if (const char *mu = std::getenv("NK_CREA_SANS_ECHELLE"))
				if (mu[0] && mu[0] != '0')
					return 0;
			if (d.n < 2)
				return 0;
			float32 e[kCreaMaxParties];
			for (int32 i = 0; i < d.n; ++i)
				e[i] = NkCreaEtendue(d.p[i]);
			for (int32 i = 0; i < d.n; ++i) {
				float32 autres[kCreaMaxParties];
				int32 na = 0;
				for (int32 j = 0; j < d.n; ++j)
					if (j != i && !d.p[j].exclue)
						autres[na++] = e[j];
				if (na == 0)
					continue;
				for (int32 a = 1; a < na; ++a) // tri par insertion : 48 parties au plus
					for (int32 b = a; b > 0 && autres[b - 1] > autres[b]; --b) {
						const float32 t = autres[b];
						autres[b] = autres[b - 1];
						autres[b - 1] = t;
					}
				const float32 med = (na & 1) ? autres[na / 2] : 0.5f * (autres[na / 2 - 1] + autres[na / 2]);
				if (med <= 1e-5f || e[i] <= kCreaEchelleMax * med * 1.001f)
					continue;
				// ⚠️ QUI EST FAUTIF ? Entre deux parties, le rapport ne le dit pas : un
				//    bouchon de 3 cm sur une bouteille de 30 cm donne 10, comme un verre
				//    et un mur. Ce qui les separe est l'ATTACHE : le bouchon est pose SUR
				//    la bouteille, le mur ne touche le verre par aucune relation. On
				//    n'EXCLUT donc qu'une partie sans attache (aucune relation vers une
				//    autre partie, et aucune partie ne s'appuie sur elle) ; une partie
				//    attachee est seulement SIGNALEE au modele. Le gabarit bouteille
				//    etait refuse par la premiere version -- c'est lui qui l'a montre.
				bool attachee = false;
				for (int32 r = 0; r < d.p[i].nRels && !attachee; ++r)
					attachee = d.p[i].relsIdx[r] >= 0;
				for (int32 j = 0; j < d.n && !attachee; ++j)
					for (int32 r = 0; r < d.p[j].nRels && !attachee; ++r)
						attachee = (j != i && d.p[j].relsIdx[r] == i);
				++signalees;
				char m[176];
				snprintf(m, sizeof(m),
						 "partie « %s » HORS D'ECHELLE : %.2f m, %.0f fois la mediane des autres parties (%.3f m) -- "
						 "elle n'appartient pas a cet objet",
						 d.p[i].nom, (double)e[i], (double)(e[i] / med), (double)med);
				if (motifs && nMot < capMotifs)
					snprintf(motifs[nMot++], 176, "%s", m);
				if (exclure && !attachee) {
					d.p[i].exclue = true;
					if (d.nRefus < kCreaMaxRefus)
						snprintf(d.refus[d.nRefus++], sizeof(d.refus[0]), "%s : non posee", m);
					std::printf("[crea] REFUS ECHELLE : %s\n", m);
					std::fflush(stdout);
				}
			}
			return signalees;
		}

		inline int32 NkCreaDire(NkModelerState &st, const NkCreaDoc &d, const NkCreaBilan &b, int32 num,
								const char *demande) {
			if (num < 0) {
				(void)NkAiPousser(st, NkModelerState::AiType::Refus, b.motif);
				return -1;
			}
			char titre[96];
			snprintf(titre, sizeof(titre), "creer : %s (%d parties)", d.scene[0] ? d.scene : "objet", (int)b.poses);
			const uint32 id = NkAiPousser(st, NkModelerState::AiType::Operation, titre);
			char effet[256];
			snprintf(effet, sizeof(effet),
					 "%d parties posees%s, %d flottante(s)%s%s ; boite %.2f x %.2f x %.2f m ; %d ligne(s) "
					 "refusee(s). Ctrl+Z annule le tout.",
					 (int)b.poses, std::getenv("NK_CREA_SANS_POSE") ? " SANS pose au sol" : " au sol",
					 (int)b.flottantes, b.flottantes ? " : " : "", b.flottantes ? b.flottanteNoms : "",
					 (double)(b.mx[0] - b.mn[0]), (double)(b.mx[1] - b.mn[1]), (double)(b.mx[2] - b.mn[2]),
					 (int)d.nRefus);
			if (editorkit::NkAiBlocDonnees *bl = st.aiFil.MutableParId(id)) {
				bl->effet = NkString(effet);
				bl->entree = NkString(demande ? demande : "");
			}
			for (int32 i = 0; i < d.nRefus; ++i)
				(void)NkAiPousser(st, NkModelerState::AiType::Refus, d.refus[i]);
			std::printf("[crea] POSE lot %d « %s » : %s\n", (int)num, d.scene, effet);
			for (int32 i = 0; i < d.nRefus; ++i)
				std::printf("[crea]   refus : %s\n", d.refus[i]);
			std::fflush(stdout);
			return num;
		}
		inline int32 NkCreaPoserEtDire(NkModelerState &st, const NkCreaDoc &d0, const char *texte,
									   const char *demande) {
			static NkCreaDoc d;
			d = d0;
			int32 nmBidon = 0;
			NkCreaVerifierEchelle(d, true, nullptr, 0, nmBidon);
			NkCreaBilan b;
			const int32 num = NkCreaPoser(st, d, texte, demande, b);
			return NkCreaDire(st, d, b, num, demande);
		}

		/// RETIRE le dernier lot SANS rien dire : c'est un essai intermediaire de la
		/// boucle, pas un geste de Rodolf. Il ne devient pas « refaisable ».
		inline void NkCreaRetirerEssai(NkModelerState &st) {
			NkCreaEtat &E = NkCrea();
			if (E.nLots == 0)
				return;
			NkCreaLot &lot = E.lots[E.nLots - 1];
			for (int32 i = lot.nNoeuds - 1; i >= 0; --i) {
				const int32 n = lot.noeuds[i];
				if (demo::Demo3DHostNodeDeleted(n) || strcmp(st.customNames[n], lot.noms[i]) != 0)
					continue;
				demo::Demo3DHostDeleteNode(n, false);
				st.customNames[n][0] = 0;
			}
			for (int32 m = 0; m < lot.nMats; ++m)
				demo::Demo3DHostProjMatDelete(lot.mats[m]);
			demo::Demo3DHostHierarchyResync();
			--E.nLots;
		}

		/// A CHAQUE IMAGE : recolte, corrige ou pose. Rend vrai quand un lot est pose.
		inline bool NkCreaRecolter(NkModelerState &st, int32 onglet, converse::NkIConverseBackend *dorsalOnglet) {
			NkCreaEtat &E = NkCrea();
			if (!E.envoi.EnCours())
				return false;
			NkString rep, err;
			bool reussi = false;
			if (!E.envoi.Recolter(rep, err, reussi))
				return false;
			std::printf("[crea] REPONSE en %.2f s (%u images pendant l'attente), tour %d\n",
						(double)E.envoi.Secondes(), (unsigned)E.envoi.Images(), (int)E.tour);
			std::fflush(stdout);
			const char *brut = rep.Data() ? rep.Data() : "";
			if (!reussi || strncmp(brut, "REFUS:", 6) == 0) {
				char m[192];
				snprintf(m, sizeof(m), "L'assistant n'a pas repondu : %s",
						 !reussi ? (err.Data() ? err.Data() : "raison inconnue") : brut + 6);
				(void)NkAiPousser(st, NkModelerState::AiType::Refus, m);
				return false;
			}
			static NkCreaDoc d;
			if (E.familleReconnue[0]) {
				bool rempli = false;
				NkCreaDocFamille(brut, d, rempli);
				std::printf("[crea] FORMULAIRE %s : %s\n", E.familleReconnue,
							rempli ? "rempli par le modele" : "NON rempli : valeurs de la famille");
				std::fflush(stdout);
				if (!rempli)
					(void)NkAiPousser(st, NkModelerState::AiType::Note,
									  "Le modele n'a pas rempli le formulaire : l'objet prend les valeurs de sa famille.");
			} else
				NkCreaLire(brut, d);
			if (d.impossible && d.n == 0 && !d.aFamille) {
				(void)NkAiPousser(st, NkModelerState::AiType::Refus,
								  "Le modele juge la demande trop vague pour un objet precis : rien n'est cree. "
								  "Nommez l'objet (« une chaise », « un arbre »).");
				return false;
			}
			// ── LA VERIFICATION, PUIS AU PLUS `toursMax` CORRECTIONS ────────────
			// Les defauts qu'on sait NOMMER avant de poser : les lignes refusees,
			// et les parties sans relation qui ne sont pas la premiere (elles
			// flotteront a coup sur, sauf `centre` explicite).
			static char motifs[kCreaMaxRefus + 8][176];
			const char *pm[kCreaMaxRefus + 8];
			int32 nMot = 0;
			for (int32 i = 0; i < d.nRefus && nMot < kCreaMaxRefus + 8; ++i) {
				snprintf(motifs[nMot], sizeof(motifs[0]), "%s", d.refus[i]);
				pm[nMot] = motifs[nMot];
				++nMot;
			}
			for (int32 i = 1; i < d.n && nMot < kCreaMaxRefus + 8; ++i)
				for (int32 r = 0; r < d.p[i].nRels && nMot < kCreaMaxRefus + 8; ++r) {
					// ⚠️ DEFAUT VU AUX COURSES 1 ET 2 : « pose_sur X decale 0 0.10 0 »
					//    dit a la fois « touche X » et « 10 cm au-dessus de X ». La
					//    relation est tenue, le decalage la defait, la partie flotte.
					//    Meme chose sur x pour a_gauche_de/a_droite_de, sur z pour
					//    devant/derriere : le decalage sur l'axe d'une relation la ROMPT.
					const int32 rr = d.p[i].rels[r];
					const int32 ax = (rr == 1 || rr == 2) ? 1 : ((rr == 4 || rr == 5) ? 0 : ((rr == 6 || rr == 7) ? 2 : -1));
					if (ax < 0)
						continue;
					// ⚠️ SEUL UN DECALAGE QUI ELOIGNE ROMPT LE CONTACT (21/09, Q6). Un
					//    decalage qui ENFONCE garde les deux boites en contact : c'est
					//    ainsi qu'un toit s'encastre dans les murs, une roue dans la
					//    caisse. La premiere version refusait les deux, et aurait
					//    refuse les gabarits ecrits a la main.
					// sens qui eloigne : pose_sur +y, pose_sous -y, a_gauche_de -x,
					// a_droite_de +x, devant +z, derriere -z.
					const float32 sens = (rr == 1 || rr == 5 || rr == 6) ? 1.f : -1.f;
					if (d.p[i].decale[ax] * sens <= 1e-4f)
						continue;
					static const char *const kAxe[3] = {"dx", "dy", "dz"};
					snprintf(motifs[nMot], sizeof(motifs[0]),
							 "partie « %s » : sa relation colle la partie sur cet axe, et decale %s = %.2f la decolle -- elle flottera ; mets %s a 0",
							 d.p[i].nom, kAxe[ax], (double)d.p[i].decale[ax], kAxe[ax]);
					pm[nMot] = motifs[nMot];
					++nMot;
				}
			for (int32 i = 1; i < d.n && nMot < kCreaMaxRefus + 8; ++i)
				if (d.p[i].rel == 0 && !d.p[i].aCentre) {
					snprintf(motifs[nMot], sizeof(motifs[0]),
							 "partie « %s » : ni pose_sur ni pose_sous, elle flottera -- dis sur quelle partie elle repose",
							 d.p[i].nom);
					pm[nMot] = motifs[nMot];
					++nMot;
				}
			{
				// L'ECHELLE : signalee au modele tant qu'il reste des tours ; au dernier,
				// la partie fautive est EXCLUE de la pose, refus nomme.
				const bool dernier = !(E.tour < E.toursMax);
				int32 nm2 = nMot;
				NkCreaVerifierEchelle(d, dernier, motifs, kCreaMaxRefus + 8, nm2);
				for (int32 k = nMot; k < nm2; ++k)
					pm[k] = motifs[k];
				nMot = nm2;
			}
			if (d.n == 0 && !d.aFamille && nMot == 0) {
				snprintf(motifs[0], sizeof(motifs[0]), "aucune ligne « partie » dans la reponse");
				pm[0] = motifs[0];
				nMot = 1;
			}
			if (nMot > 0 && E.tour < E.toursMax) {
				++E.tour;
				static char invite[28000];
				snprintf(E.docPrecedent, sizeof(E.docPrecedent), "%s", brut);
				NkCreaEcrireCorrection(invite, sizeof(invite), E.demande, E.docPrecedent, pm, nMot);
				converse::NkIConverseBackend *dorsal = (onglet == 0) ? (converse::NkIConverseBackend *)&E.dorsal
																	 : dorsalOnglet;
				NkString pourquoi;
				char m[192];
				snprintf(m, sizeof(m), "Je renvoie le plan au modele avec %d defaut(s) nomme(s) (tour %d sur %d).",
						 (int)nMot, (int)E.tour, (int)E.toursMax);
				std::printf("[crea] CORRECTION tour %d : %d defaut(s)\n", (int)E.tour, (int)nMot);
				for (int32 i = 0; i < nMot; ++i)
					std::printf("[crea]   defaut : %s\n", pm[i]);
				std::fflush(stdout);
				if (dorsal && E.envoi.Lancer(dorsal, NkString(invite), pourquoi)) {
					(void)NkAiPousser(st, NkModelerState::AiType::Note, m);
					return false;
				}
			}
			if (d.n == 0 && !d.aFamille) {
				char m[192];
				snprintf(m, sizeof(m), "Aucune partie lisible dans la reponse du modele : rien n'est cree.");
				(void)NkAiPousser(st, NkModelerState::AiType::Refus, m);
				std::printf("[crea] REFUS : %s (reponse brute : « %.300s »)\n", m, brut);
				std::fflush(stdout);
				return false;
			}
			NkCreaBilan b;
			const int32 num = NkCreaPoser(st, d, brut, E.demande, b);
			// ── POSER, MESURER, ET CORRIGER CE QUE LA MESURE CONTREDIT ──────────
			// Les defauts qu'on ne voit qu'une fois pose : une partie dont la boite
			// ne touche ni le sol ni une autre partie. On RETIRE l'essai (il n'a
			// jamais ete un geste de Rodolf) et on renvoie au modele les noms que
			// la MESURE designe -- pas une supposition sur son document.
			if (num > 0 && b.flottantes > 0 && E.tour < E.toursMax) {
				NkCreaRetirerEssai(st);
				++E.tour;
				static char mot0[400];
				snprintf(mot0, sizeof(mot0),
						 "mesure apres pose : %d partie(s) ne touchent ni le sol ni une autre partie : %s -- "
						 "donne-leur une relation qui les colle a une partie existante",
						 (int)b.flottantes, b.flottanteNoms);
				const char *pm2[1] = {mot0};
				static char invite2[28000];
				snprintf(E.docPrecedent, sizeof(E.docPrecedent), "%s", brut);
				NkCreaEcrireCorrection(invite2, sizeof(invite2), E.demande, E.docPrecedent, pm2, 1);
				converse::NkIConverseBackend *dorsal = (onglet == 0) ? (converse::NkIConverseBackend *)&E.dorsal
																	 : dorsalOnglet;
				NkString pourquoi;
				std::printf("[crea] CORRECTION tour %d (apres pose) : %s\n", (int)E.tour, mot0);
				std::fflush(stdout);
				if (dorsal && E.envoi.Lancer(dorsal, NkString(invite2), pourquoi)) {
					char m[240];
					snprintf(m, sizeof(m), "Pose d'essai : %d partie(s) flottent (%s). Je renvoie le plan au modele "
										   "(tour %d sur %d).",
							 (int)b.flottantes, b.flottanteNoms, (int)E.tour, (int)E.toursMax);
					(void)NkAiPousser(st, NkModelerState::AiType::Note, m);
					return false;
				}
				// L'appel n'a pas pu partir : on repose l'essai tel quel, et on le dit.
				const int32 num2 = NkCreaPoser(st, d, brut, E.demande, b);
				return NkCreaDire(st, d, b, num2, E.demande) > 0;
			}
			return NkCreaDire(st, d, b, num, E.demande) > 0;
		}

		// =====================================================================
		//  8. LES CROCHETS DE MESURE — entrent aux memes portes que la main
		// =====================================================================
		/// NK_CREA_VUES=<prefixe> : apres chaque lot pose, l'application REND
		/// trois vues (3/4, face, profil) par sa propre cible hors ecran -- jamais
		/// une capture d'ecran. `Demo3DHostCaptureView` fige la derniere image rendue
		/// de la vue 3D ; on attend donc quelques images apres chaque geste de camera.
		/// LA VOIE (b) EST-ELLE DEMANDEE ? `NK_CREA_VOIE=triposr` : apres la pose,
		/// la vue 3/4 RENDUE PAR L'APPLICATION devient l'entree de TripoSR. Le texte
		/// seul est le cas limite de la vue manquante -- toutes manquent -- et c'est
		/// l'assemblage qui les cree. ⚠️ PAS LE DEFAUT : la mesure du 21/09 dit ce
		/// que cette voie rend (voir modelisation-ia.reponses.md), et l'attente est
		/// SYNCHRONE (~30 s de fenetre figee), la dette deja declaree du generateur.
		inline bool NkCreaVoieTripoSR() {
			const char *v = std::getenv("NK_CREA_VOIE");
			return v && strcmp(v, "triposr") == 0;
		}
		inline void NkCreaDemarrerVues(const NkModelerState &st) {
			NkCreaEtat &E = NkCrea();
			const char *v = std::getenv("NK_CREA_VUES");
			char defaut[400];
			// ── (Q19.2) TOUTE GENERATION ECRIT SES VUES, QUELLE QUE SOIT LA VOIE ──
			// Rodolf : « qu'on passe en texte ou pas, on doit pouvoir voir la
			// reconstitution de vue, si c'est le cas, dans son dossier ».
			// Avant, seule la voie TripoSR rendait des vues, et elle les ecrivait
			// dans `logs/` -- c'est-a-dire nulle part pour l'utilisateur. Le dossier
			// `vues/` du modele existait et restait VIDE, ce qui est pire que pas de
			// dossier : il promet quelque chose qu'il ne contient pas.
			// Desormais : des qu'un projet est ouvert, les vues vont dans
			// `<projet>/Modeles/<objet>/vues/`, pour la voie famille comme pour la
			// voie image. Elles servent a la mesure de fidelite, a Q17 et a
			// l'entrainement -- c'est-a-dire qu'elles ne sont pas une decoration.
			if (!v || !*v) {
				const NkString dm = NkGeniaDossierModele(st, NkCreaNomObjet());
				if (!dm.Empty()) {
					snprintf(defaut, sizeof(defaut), "%svues/%s", dm.CStr(), NkCreaNomObjet());
					v = defaut;
				}
			}
			if ((!v || !*v) && NkCreaVoieTripoSR()) {
				NkDirectory::CreateRecursive("logs");
				snprintf(defaut, sizeof(defaut), "logs/crea_vue_%03d", (int)E.dernierLot);
				v = defaut;
			}
			if (!v || !*v)
				return;
			snprintf(E.vuesPrefixe, sizeof(E.vuesPrefixe), "%s", v);
			E.vuesEtape = 0;
			E.vuesAttente = 3;
		}

		/// Rend vrai tant qu'une sequence de vues est en cours.
		/// La boite MONDE du dernier lot, relue a l'hote.

		/// Masque (ou rend) tout objet utilisateur qui n'est pas du dernier lot. ⚠️
		/// L'ETAT D'AVANT EST GARDE ET RENDU : une photo ne doit pas laisser la
		/// scene de Rodolf autrement qu'elle l'a trouvee.
		inline void NkCreaIsoler(bool isoler) {
			NkCreaEtat &E = NkCrea();
			static bool sAvant[512];
			static bool sTouche[512];
			const int32 nT = demo::Demo3DHostNodeCount();
			if (isoler) {
				for (int32 q = 0; q < nT && q < 512; ++q) {
					sTouche[q] = false;
					// ⚠️ LES MAILLAGES SEULEMENT. La premiere version masquait tout
					//    objet hors du lot -- y compris la LUMIERE du projet, et la
					//    chaise sortait brun sombre sur fond sombre. Une photo qui
					//    change l'eclairage ne montre plus l'objet tel qu'il est.
					const int32 uk = demo::Demo3DHostUserKind(q);
					if (!((uk >= 1 && uk <= 3) || uk == 10) || demo::Demo3DHostNodeDeleted(q))
						continue;
					// ⚠️ LE LOT, SES ANCETRES, ET LE LOT D'AVANT QUAND ON MONTRE TOUT.
					//    Deux elargissements, tous deux payes par une image :
					//    (1) un noeud IMPORTE est un CONTENEUR dont la geometrie vit
					//        dans des enfants, qui ne sont pas inscrits au lot ; s'en
					//        tenir aux noeuds du lot donnait une scene vide ;
					//    (2) la serie qui suit une generation doit montrer l'assemblage
					//        A COTE de l'objet genere, donc DEUX lots -- sinon il ne
					//        reste rien a quoi comparer l'echelle.
					bool duLot = false;
					const int32 premier = E.vuesToutVoir && E.nLots > 1 ? E.nLots - 2 : E.nLots - 1;
					for (int32 li = premier; li < E.nLots && !duLot; ++li) {
						const NkCreaLot &l = E.lots[li];
						for (int32 i = 0; i < l.nNoeuds && !duLot; ++i) {
							if (l.noeuds[i] == q) {
								duLot = true;
								break;
							}
							// remonte : `q` descend-il d'un noeud du lot ?
							int32 pere = demo::Demo3DHostNodeParent(q);
							for (int32 garde = 0; pere >= 0 && garde < 16; ++garde) {
								if (pere == l.noeuds[i]) {
									duLot = true;
									break;
								}
								pere = demo::Demo3DHostNodeParent(pere);
							}
						}
					}
					if (duLot)
						continue;
					sAvant[q] = demo::Demo3DHostObjectHidden(q);
					sTouche[q] = true;
					demo::Demo3DHostSetObjectHidden(q, true);
				}
			} else {
				for (int32 q = 0; q < nT && q < 512; ++q)
					if (sTouche[q]) {
						demo::Demo3DHostSetObjectHidden(q, sAvant[q]);
						sTouche[q] = false;
					}
			}
		}

		/// Rend vrai tant qu'une sequence de vues est en cours.
		/// ⚠️ PENDANT LES VUES, les autres objets sont masques et les surimpressions
		///    de mise au point eteintes : l'image doit montrer l'OBJET CREE, pas le
		///    cube de depart ni le texte de la vue. Tout est rendu a la fin.
		inline bool NkCreaVuesTick(NkModelerState &st) {
			NkCreaEtat &E = NkCrea();
			static bool sHudAvant = true, sCurseurAvant = true;
			if (E.vuesEtape < 0)
				return false;
			if (E.vuesAttente > 0) {
				--E.vuesAttente;
				return true;
			}
			// DEUX TEMPS PAR VUE : le geste de camera (qui peut s'animer sur
			// quelques images), PUIS le cadrage sur la boite du lot, PUIS la photo.
			// Cadrer dans la meme image que le geste lisait les angles d'AVANT : la
			// premiere « vue 3/4 » etait une vue de face.
			// etapes : 0 prepare+face | 1 cadre | 2 photo face+profil | 3 cadre |
			//          4 photo profil+3/4 | 5 cadre | 6 photo 3/4 + fin
			static const char *const kNom[3] = {"face", "profil", "34"};
			char chemin[256];
			float32 mn[3], mx[3];
			bool boite;
			if (E.vuesToutVoir) {
				boite = E.vuesBoiteValide;
				for (int32 k = 0; k < 3; ++k) {
					mn[k] = E.vuesBoite[k];
					mx[k] = E.vuesBoite[3 + k];
				}
			} else
				boite = NkCreaBoiteDernierLot(mn, mx);
			const int32 e = E.vuesEtape;
			if (e == 0) {
				sHudAvant = demo::Demo3DHostHud();
				sCurseurAvant = demo::Demo3DHostCursorShown();
				demo::Demo3DHostSetHud(false);
				demo::Demo3DHostSetCursorShown(false);
				NkCreaIsoler(true); // deux lots quand E.vuesToutVoir : voir NkCreaIsoler
				demo::Demo3DHostSelectEmptyNode(-1);
			}
			if (e == 2 || e == 4 || e == 6) {
				snprintf(chemin, sizeof(chemin), "%s_%s.png", E.vuesPrefixe, kNom[e / 2 - 1]);
				std::printf("[crea] VUE %s -> %s : %s\n", kNom[e / 2 - 1], chemin,
							demo::Demo3DHostCaptureView(chemin) ? "ecrite" : "ECHEC");
				std::fflush(stdout);
			}
			// ── (Q8) LE GROS PLAN : « des gros plans, pas seulement des vues
			//    d'ensemble ». `NK_CREA_GROSPLAN=x,y,z,taille` (fractions de la boite
			//    de l'objet, 0..1) : apres la vue 3/4, la camera garde l'angle et
			//    cadre une BOITE de cette taille centree sur ce point.
			const char *gp = std::getenv("NK_CREA_GROSPLAN");
			if (e == 6 && gp && *gp && boite) {
				float32 f[4] = {0.5f, 0.6f, 1.f, 0.3f};
				const char *c = gp;
				for (int32 k = 0; k < 4 && *c; ++k) {
					f[k] = (float32)std::atof(c);
					while (*c && *c != ',')
						++c;
					if (*c == ',')
						++c;
				}
				float32 cc[3], ext = 0.f;
				for (int32 a = 0; a < 3; ++a) {
					cc[a] = mn[a] + f[a] * (mx[a] - mn[a]);
					if (mx[a] - mn[a] > ext)
						ext = mx[a] - mn[a];
				}
				const float32 h = 0.5f * f[3] * ext;
				const float32 bmn[3] = {cc[0] - h, cc[1] - h, cc[2] - h}, bmx[3] = {cc[0] + h, cc[1] + h, cc[2] + h};
				demo::Demo3DHostFrameBox(bmn, bmx);
				E.vuesAttente = 8;
				E.vuesEtape = 7;
				return true;
			}
			if (e == 7) {
				E.vuesAttente = 2;
				E.vuesEtape = 8;
				return true;
			}
			if (e == 8) {
				snprintf(chemin, sizeof(chemin), "%s_gros.png", E.vuesPrefixe);
				std::printf("[crea] VUE gros plan -> %s : %s\n", chemin,
							demo::Demo3DHostCaptureView(chemin) ? "ecrite" : "ECHEC");
				std::fflush(stdout);
			}
			if (e == 6 || e == 8) {
				NkCreaIsoler(false);
				demo::Demo3DHostSetHud(sHudAvant);
				demo::Demo3DHostSetCursorShown(sCurseurAvant);
				E.vuesEtape = -1;
				const bool serieDApres = E.vuesToutVoir;
				E.vuesToutVoir = false; // JAMAIS laisse arme : la creation suivante isole a nouveau
				// ⚠️ LA SERIE D'APRES NE RELANCE PAS LA VOIE (b), ET C'EST UNE BOUCLE
				//    INFINIE QUI L'A APPRIS. La voie (b) part de la vue 3/4 en fin de
				//    serie ; la serie ajoutee apres la generation produit une vue 3/4
				//    de plus, donc une generation de plus, donc une serie de plus. Mesure
				//    du 22/09 : onze generations sur un seul « un banc de parc », chacune
				//    a 60 s, jusqu'au delai de garde. Le declencheur appartient a la
				//    PREMIERE serie, celle qui photographie une creation.
				if (NkCreaVoieTripoSR() && !serieDApres) {
					// ── VOIE (b) : LA VUE RENDUE -> TRIPOSR -> UNE CARTE ─────────
					// Par la MEME porte que le bouton « Generer » : le refus est nomme
					// a l'ecran s'il y en a un. La carte nait dans le navigateur (un
					// import n'ajoute pas a la scene, contrat du 17/08).
					// ⚠️ PLUS D'ATTENTE SUR LE FIL D'AFFICHAGE (Q6) : la mesure du
					//    21/09 donnait 84 s de fenetre figee. La generation part dans
					//    un fil ; l'import se fait a la recolte.
					snprintf(chemin, sizeof(chemin), "%s_34.png", E.vuesPrefixe);
					if (NkGeniaLancer(st, false, chemin, "b"))
						(void)NkAiPousser(st, NkModelerState::AiType::Note,
										  "Voie (b) : la vue 3/4 de l'assemblage part a TripoSR. La fenetre reste "
										  "vivante ; l'assemblage en parties reste dans la scene.");
				}
				return false;
			}
			if (e % 2 == 0) {
				// le geste de camera de la vue suivante
				if (e == 0)
					demo::Demo3DHostAxisView(0, false); // face
				else if (e == 2)
					demo::Demo3DHostAxisView(1, false); // profil
				else
					demo::Demo3DHostResetView(); // la vue de depart du modeleur : 3/4 en perspective
			} else {
				if (boite)
					demo::Demo3DHostFrameBox(mn, mx);
				else
					demo::Demo3DHostFrameAll();
			}
			E.vuesAttente = 8;
			++E.vuesEtape;
			return true;
		}

		/// ── NK_VC_PREUVE=<prefixe> : LA PREUVE DEMANDEE PAR Q15, EN QUATRE IMAGES ──
		/// Le meme objet TripoSR, materiau GRIS puis ROUGE, avec les couleurs du
		/// maillage puis sans elles. Quatre fichiers, meme camera, meme objet : la
		/// seule chose qui bouge d'une image a l'autre est ce qu'on veut montrer.
		/// ⚠️ ELLE POSE LA TEINTE PAR LA MEME PORTE QUE LE PANNEAU
		///    (`Demo3DHostSetMeshTint`) : une preuve qui passerait par un chemin a
		///    elle ne dirait rien de ce que l'utilisateur obtient.
		inline bool NkVcPreuveTick(NkModelerState &st) {
			const char *pref = std::getenv("NK_VC_PREUVE");
			if (!pref || !*pref)
				return false;
			static int32 etape = 0;
			static int32 attente = 0;
			if (etape > 8)
				return false;
			NkVector<NkVcEntree> &T = NkVcTable();
			if (T.Size() == 0)
				return false; // rien de colore dans la scene : il n'y a rien a montrer
			if (attente > 0) {
				--attente;
				return true;
			}
			const float32 kGris[3] = {0.50f, 0.50f, 0.50f};
			const float32 kRouge[3] = {0.80f, 0.10f, 0.10f};
			auto teinter = [&](const float32 *c) {
				for (usize i = 0; i < T.Size(); ++i)
					demo::Demo3DHostSetMeshTint(T[i].noeud, c);
			};
			auto capturer = [&](const char *suffixe) {
				char chemin[260];
				snprintf(chemin, sizeof(chemin), "%s_%s.png", pref, suffixe);
				const bool ok = demo::Demo3DHostCaptureView(chemin);
				std::printf("[vc] PREUVE %s -> %s : %s\n", suffixe, chemin, ok ? "ecrite" : "ECHEC");
				std::fflush(stdout);
			};
			switch (etape) {
				case 0: {
					// CADRER SUR L'OBJET COLORE, et sur lui seul : cadrer la scene
					// entiere le montrerait a cote de l'assemblage, trop petit pour
					// qu'on voie quoi que ce soit de sa matiere.
					float32 mn[3] = {0.f, 0.f, 0.f}, mx[3] = {0.f, 0.f, 0.f};
					bool aB = false;
					for (usize i = 0; i < T.Size(); ++i) {
						float32 a[3], b[3];
						if (!demo::Demo3DHostNodeBounds(T[i].noeud, true, a, b))
							continue;
						for (int32 k = 0; k < 3; ++k) {
							if (!aB || a[k] < mn[k])
								mn[k] = a[k];
							if (!aB || b[k] > mx[k])
								mx[k] = b[k];
						}
						aB = true;
					}
					if (aB)
						demo::Demo3DHostFrameBox(mn, mx);
					attente = 8;
					break;
				}
				case 1: teinter(kGris); attente = 6; break;
				case 2: capturer("gris_avec_couleurs"); attente = 2; break;
				case 3: teinter(kRouge); attente = 6; break;
				case 4: capturer("rouge_avec_couleurs"); attente = 2; break;
				case 5: {
					int32 n = 0;
					for (usize i = 0; i < T.Size(); ++i)
						if (NkVcRegler(T[i].noeud, false))
							++n;
					teinter(kGris);
					std::printf("[vc] INTERRUPTEUR eteint sur %d noeud(s)\n", (int)n);
					std::fflush(stdout);
					attente = 10;
					break;
				}
				case 6: capturer("gris_sans_couleurs"); attente = 2; break;
				case 7: teinter(kRouge); attente = 6; break;
				case 8: capturer("rouge_sans_couleurs"); attente = 2; break;
				default: break;
			}
			++etape;
			return etape <= 8;
		}

		/// ── NK_ARTIC_MESURE=1 : LA MESURE DES 30 DEGRES, REECRITE POUR ECHOUER ──
		/// ⚠️ LA PREMIERE VERSION ETAIT UNE TAUTOLOGIE, et c'est la lecon de ce
		///    crochet. Elle comparait `sp + o` avant et apres une rotation qui ne
		///    change ni `sp` ni `o` : son zero ne pouvait pas etre autre chose que
		///    zero. Elle a imprime « ecart 0,0000 mm » sur une porte dont la boite
		///    venait de passer de 2,40 a 4,46 m -- verte pendant que l'objet cassait.
		/// CELLE-CI PEUT ECHOUER, et c'est tout ce qui la distingue. Elle relit la
		/// GEOMETRIE MONDE (`Demo3DHostNodeBounds`), et elle exige DEUX choses
		/// opposees, donc impossibles a satisfaire par accident :
		///   (a) le cote de l'axe ne bouge pas -- sinon la charniere n'est pas sur les
		///       gonds ;
		///   (b) le cote OPPOSE bouge, d'a peu pres ce que la geometrie impose
		///       (2 L sin(15 degres) pour 30 degres) -- sinon rien n'a tourne, et un
		///       (a) vert tout seul serait vert precisement parce que rien n'a bouge.
		inline void NkArticMesureTick(NkModelerState &st, int32 image) {
			const char *v = std::getenv("NK_ARTIC_MESURE");
			if (!v || !*v || v[0] == '0')
				return;
			static int32 etape = 0;
			static int32 cible = -1;
			static float32 mnA[3] = {0.f, 0.f, 0.f}, mxA[3] = {0.f, 0.f, 0.f};
			static float32 pv[3] = {0.f, 0.f, 0.f};
			if (etape > 2 || image < 60 || !demo::Demo3DHostReady())
				return;
			if (etape == 0) {
				for (usize i = 0; i < NkArticTable().Size(); ++i) {
					const NkArticulation &a = NkArticTable()[i];
					if (a.liaison != 1 || a.noeud < 0 || demo::Demo3DHostNodeDeleted(a.noeud))
						continue;
					if (!demo::Demo3DHostNodeBounds(a.noeud, true, mnA, mxA))
						continue;
					cible = a.noeud;
					pv[0] = a.pivot[0];
					pv[1] = a.pivot[1];
					pv[2] = a.pivot[2];
					std::printf("[artic] CIBLE noeud %d « %s » : pivot monde (%.4f, %.4f, %.4f), "
								"boite x [%.4f, %.4f]\n",
								(int)cible, (cible < NkModelerState::kMaxNodeNames) ? st.customNames[cible] : "?",
								(double)pv[0], (double)pv[1], (double)pv[2], (double)mnA[0], (double)mxA[0]);
					std::fflush(stdout);
					break;
				}
				if (cible < 0) {
					// ⚠️ ON ATTEND LA POSE AU LIEU DE CONCLURE, ET L'ECHEANCE NE SUFFIT
					//    PAS. Premiere version : conclure a l'image 60 mesurait une scene
					//    vide. Deuxieme : une echeance de 2000 images marchait SANS image
					//    jointe et ratait la pose AVEC -- la vision et le plan la
					//    repoussent de plusieurs secondes, et « aucune charniere » est
					//    retombe alors qu'il y en avait quatre. C'est deux fois la meme
					//    faute : conclure avant que la chose existe.
					//    On ne conclut donc que lorsque PLUS RIEN N'EST EN VOL, et
					//    l'echeance n'est qu'un garde-fou tres large.
					NkCreaEtat &EA = NkCrea();
					const bool enVol = EA.envoi.EnCours() || EA.envoiVision.EnCours() || NkGenia().envoi.EnCours();
					if (enVol || image < 12000)
						return;
					std::printf("[artic] AUCUNE charniere dans la scene apres 2000 images : rien a mesurer\n");
					std::fflush(stdout);
					etape = 3;
					return;
				}
				++etape;
				return;
			}
			if (etape == 1) {
				if (!NkArticTourner(cible, 30.f)) {
					std::printf("[artic] ROUGE : la rotation a ete REFUSEE (axe non vertical ou noeud sans "
								"liaison)\n");
					std::fflush(stdout);
					etape = 3;
					return;
				}
				++etape;
				return;
			}
			float32 mnB[3], mxB[3];
			if (demo::Demo3DHostNodeBounds(cible, true, mnB, mxB)) {
				// Le cote de l'axe : celui des deux bords en x le plus proche du pivot.
				const bool axeAGauche = fabsf(mnA[0] - pv[0]) < fabsf(mxA[0] - pv[0]);
				const float32 bordAxeA = axeAGauche ? mnA[0] : mxA[0];
				const float32 bordAxeB = axeAGauche ? mnB[0] : mxB[0];
				const float32 bordLibreA = axeAGauche ? mxA[0] : mnA[0];
				const float32 bordLibreB = axeAGauche ? mxB[0] : mnB[0];
				const float32 dAxe = fabsf(bordAxeB - bordAxeA) * 1000.f;
				const float32 dLibre = fabsf(bordLibreB - bordLibreA) * 1000.f;
				// CE QU'IMPOSE LA GEOMETRIE : le bord libre est a L du pivot ; apres 30
				// degres il s'est deplace de 2 L sin(15 degres), dont L(1 - cos 30) en x.
				const float32 L = fabsf(bordLibreA - pv[0]);
				const float32 attenduX = L * (1.f - cosf(30.f * 0.017453292f)) * 1000.f;
				// ── LA TOLERANCE EST DERIVEE, PAS CHOISIE (25/09) ───────────────
				// La premiere version exigeait 0 mm et rougissait a 12,500 mm. Ce
				// n'etait pas le pivot : c'est la BOITE ALIGNEE d'un battant EPAIS
				// qui tourne. Un panneau d'epaisseur e pivotant de theta sur son
				// arete voit le bord de sa boite reculer de (e/2) sin(theta) --
				// (0,050/2) x sin(30) = 12,5 mm, exactement le chiffre mesure.
				// On borne donc par ce que la geometrie impose, plus un demi-
				// millimetre de rasterisation. ⚠️ Ce n'est PAS relacher le critere
				// pour le faire passer : la borne se calcule sur l'epaisseur RELUE
				// avant la rotation, et un vrai decalage de pivot la depasserait.
				const float32 ep = mxA[2] - mnA[2];
				const float32 tolAxe = 0.5f + 0.5f * ep * sinf(30.f * 0.017453292f) * 1000.f;
				const bool okAxe = dAxe <= tolAxe;
				const bool okLibre = dLibre >= 0.5f * attenduX; // il a VRAIMENT tourne
				std::printf("[artic] MESURE 30 deg : cote de l'axe bouge de %.3f mm (attendu 0) ; "
							"cote libre bouge de %.3f mm (la geometrie impose ~%.3f) [tolerance de l'axe "
							"%.3f mm, tiree d'une epaisseur de %.3f m] -> %s\n",
							(double)dAxe, (double)dLibre, (double)attenduX, (double)tolAxe, (double)ep,
							(okAxe && okLibre) ? "VERT"
											   : (!okAxe ? "ROUGE : l'axe s'est deplace"
													 : "ROUGE : rien n'a tourne, le vert de l'axe ne prouve rien"));
				std::fflush(stdout);
				if (FILE *f = fopen("logs/crea_mesure.txt", "ab")) {
					fprintf(f, "ARTICULATION noeud=%d axe_mm=%.4f libre_mm=%.4f attendu_mm=%.4f vert=%d\n",
							(int)cible, (double)dAxe, (double)dLibre, (double)attenduX, (okAxe && okLibre) ? 1 : 0);
					fclose(f);
				}
			}
			etape = 3;
		}

		/// A appeler une fois par image. `poserUndo` pose le geste « annuler » par la
		/// porte commune (NkVpPoserAction) -- c'est l'appelant qui la connait.
		inline void NkCreaTick(NkModelerState &st, int32 onglet, converse::NkIConverseBackend *dorsalOnglet,
							   void (*poserUndo)(NkModelerState &), int32 image) {
			NkCreaEtat &E = NkCrea();
			NkGeniaRecolter(st); // voies (b) et (c) : la generation qui vole hors du fil
			{
				static bool sImgHook = false;
				if (!sImgHook) {
					sImgHook = true;
					if (const char *im = std::getenv("NK_CREA_IMAGE"))
						if (*im)
							NkCreaJoindreImage(im); // la MEME porte que le panneau
				}
			}
			if (E.envoiVision.EnCours()) {
				NkString rep, err;
				bool okv = false;
				if (E.envoiVision.Recolter(rep, err, okv)) {
					const char *t = rep.CStr() ? rep.CStr() : "";
					if (okv && strncmp(t, "REFUS:", 6) != 0) {
						snprintf(E.descriptionImage, sizeof(E.descriptionImage), "%s", t);
						for (char *c = E.descriptionImage; *c; ++c)
							if (*c == '\n' || *c == '\r')
								*c = ' ';
					}
					char m[800];
					snprintf(m, sizeof(m), okv ? "Ce que le modele de vision voit sur l'image (%.1f s) : %s"
											   : "Le modele de vision n'a pas repondu (%.1f s) : %s -- le plan part sans lui.",
							 (double)E.envoiVision.Secondes(), okv ? E.descriptionImage : err.CStr());
					(void)NkAiPousser(st, NkModelerState::AiType::Note, m);
					std::printf("[crea] VISION : %.1f s -> %s\n", (double)E.envoiVision.Secondes(),
								okv ? E.descriptionImage : "ECHEC");
					std::fflush(stdout);
					E.tripoApresPose = true;
					(void)NkCreaLancerPlan(st, E.dorsalAttente);
				}
			}
			// ── NK_CREA_DEMANDE=<phrase> : LA PORTE DE MESURE DE LA CREATION ──
			// ⚠️ ELLE EXISTE PARCE QUE LA FRAPPE REJOUEE NE SUFFIT PLUS (22/09). Le
			//    clic + `t:` + Entree de `NK_EVENEMENTS` a tape « modelise moi un vase
			//    a fleurs » et l'application a envoye « Je veux un canar ninja » : le
			//    composeur portait un brouillon RESTAURE, et c'est lui qui est parti.
			//    Mesurer la creation a travers le composeur, c'est donc mesurer aussi
			//    l'etat du panneau -- et se tromper de coupable quand il change. Cette
			//    porte entre au MEME endroit que le panneau (`NkCreaLancer`), avec le
			//    meme onglet et le meme dorsal : elle court-circuite la saisie, rien
			//    d'autre. Ce n'est pas une entree d'utilisateur, c'est un instrument.
			{
				static bool sDemFait = false;
				if (!sDemFait) {
					const char *v = std::getenv("NK_CREA_DEMANDE");
					if (!v || !*v)
						sDemFait = true;
					else if (image >= 30 && demo::Demo3DHostReady() && !E.envoi.EnCours()) {
						sDemFait = true;
						std::printf("[crea] NK_CREA_DEMANDE : « %s »\n", v);
						std::fflush(stdout);
						(void)NkCreaLancer(st, v, onglet, dorsalOnglet);
					}
				}
			}
			bool pose = NkCreaRecolter(st, onglet, dorsalOnglet);
			// NK_CREA_DOC=<fichier>[,image] : pose un document ECRIT A LA MAIN, sans
			// modele. C'est le temoin de l'OUTIL seul : si la chaise d'un document
			// juste sort de travers, le defaut est ici et pas dans le modele.
			{
				static bool sDocFait = false;
				if (!sDocFait) {
					const char *v = std::getenv("NK_CREA_DOC");
					if (!v || !*v)
						sDocFait = true;
					else {
						char chemin[400];
						snprintf(chemin, sizeof(chemin), "%s", v);
						int32 quand = 20;
						if (char *virg = strrchr(chemin, ',')) {
							*virg = 0;
							quand = (int32)std::atoi(virg + 1);
						}
						if (image >= quand && demo::Demo3DHostReady()) {
							sDocFait = true;
							const NkString texte = NkFile::ReadAllText(chemin);
							static NkCreaDoc d;
							NkCreaLire(texte.Data() ? texte.Data() : "", d);
							std::printf("[crea] NK_CREA_DOC '%s' : %d partie(s), %d refus\n", chemin, (int)d.n,
										(int)d.nRefus);
							pose = NkCreaPoserEtDire(st, d, texte.Data() ? texte.Data() : "", chemin) > 0;
						}
					}
				}
			}
			if (pose && E.tripoApresPose && E.imageJointe[0]) {
				// L'IMAGE JOINTE PART AUSSI A TRIPOSR, apres la pose (carte au
				// navigateur, hors du fil) ; puis la piece jointe est CONSOMMEE.
				E.tripoApresPose = false;
				if (NkGeniaLancer(st, false, E.imageJointe, "image"))
					(void)NkAiPousser(st, NkModelerState::AiType::Note,
									  "L'image jointe part a TripoSR : sa reconstruction arrivera en carte au navigateur, "
									  "a cote de l'assemblage.");
				E.imageJointe[0] = 0;
				E.descriptionImage[0] = 0;
			}
			if (pose) {
				NkCreaDemarrerVues(st);
				if (const char *a = std::getenv("NK_CREA_ANNULE"))
					E.annuleDans = (int32)std::atoi(a);
			}
			NkArticMesureTick(st, image);
			if (NkVcPreuveTick(st))
				return; // (Q15) la preuve des couleurs de sommet passe avant l'annulation
			if (NkCreaVuesTick(st))
				return; // les vues d'abord : annuler avant la photo effacerait le sujet
			if (E.annuleDans > 0 && --E.annuleDans == 0) {
				E.annuleDans = -1;
				E.mesureAnnul = 4;
				if (poserUndo)
					poserUndo(st);
			}
			if (E.mesureAnnul > 0 && --E.mesureAnnul == 0) {
				E.mesureAnnul = -1;
				// LA MESURE D'APRES : combien d'objets, et combien des NOMS du lot
				// annule survivent dans la scene.
				const int32 apres = NkCreaCompterObjets();
				int32 restants = 0;
				const NkCreaLot &l = E.refaire;
				for (int32 i = 0; i < l.nNoeuds; ++i) {
					const int32 n = l.noeuds[i];
					if (!demo::Demo3DHostNodeDeleted(n) && demo::Demo3DHostUserKind(n) != 0 &&
						strcmp(st.customNames[n], l.noms[i]) == 0)
						++restants;
				}
				const bool geste = E.aRefaire; // vrai seulement si l'annulation a eu lieu
				if (FILE *f = fopen("logs/crea_mesure.txt", "ab")) {
					fprintf(f, "ANNULATION %d geste=%d objets_avant=%d objets_apres=%d restants=%d\n", E.dernierLot,
							geste ? 1 : 0, l.objetsAvant, apres, restants);
					fclose(f);
				}
				std::printf("[crea] MESURE annulation : geste=%d objets %d -> %d, %d partie(s) restante(s)\n",
							geste ? 1 : 0, (int)l.objetsAvant, (int)apres, (int)restants);
				std::fflush(stdout);
				// UNE IMAGE D'APRES L'ANNULATION, quand on la demande. Un compteur qui
				// revient a son chiffre d'avant n'est pas une scene vide : il peut etre
				// juste pendant que la geometrie reste a l'ecran. Le cadrage est celui
				// deja calcule pour la serie d'apres generation, donc les deux images se
				// comparent directement, meme camera, meme boite.
				bool photoApres = false;
				if (const char *v = std::getenv("NK_CREA_VUES"))
					if (*v && std::getenv("NK_CREA_VUE_APRES_ANNULE") && E.vuesBoiteValide) {
						char p3[200];
						snprintf(p3, sizeof(p3), "%s_annule", v);
						snprintf(E.vuesPrefixe, sizeof(E.vuesPrefixe), "%s", p3);
						E.vuesToutVoir = true;
						E.vuesEtape = 0;
						E.vuesAttente = 3;
						photoApres = true;
					}
				if (const char *q = std::getenv("NK_CREA_QUITTE"))
					if (*q && *q != '0')
						E.quitteDans = photoApres ? 200 : 5;
			}
			if (E.quitteDans > 0 && !NkGenia().envoi.EnCours() && --E.quitteDans == 0)
				st.running = false;
		}

	} // namespace nk3d
} // namespace nkentseu
