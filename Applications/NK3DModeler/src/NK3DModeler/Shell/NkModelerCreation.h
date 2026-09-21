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
#include "NK3DModeler/Genia/NkGeniaImport.h"	 // voie (b) : la vue rendue devient l'entree de TripoSR
#include "NK3DModeler/Viewport/NkDemo3DHost.h"
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
				bool tailleDonnee = false;
				// ── COHERENCE D'ECHELLE (Q7) ──
				bool exclue = false; ///< refusee avant la pose ; le motif est dans d.refus
		};

		struct NkCreaDoc {
				char scene[24] = {0};
				NkCreaPartie p[kCreaMaxParties];
				int32 n = 0;
				char refus[kCreaMaxRefus][176] = {};
				int32 nRefus = 0;
				bool impossible = false;
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
				if (ok && pc.forme >= 0 && strcmp(NkCreaFormes(nfTmp)[pc.forme].nom, "revolution") == 0 && pc.nNombres < 4) {
					NkCreaRefuser(d, "partie « %s » : une revolution demande un « profil » (couples rayon hauteur)%s", pc.nom);
					ok = false;
				}
				if (ok && !aTaille && (pc.aDim[0] || pc.aDim[1] || pc.aDim[2])) {
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
			ajout("de forme revolution, decrite par son PROFIL exterieur : des couples rayon hauteur, en metres, du bas\n");
			ajout("vers le haut. paroi <e> le rend CREUX (un verre, un bol) ; sans paroi il est PLEIN (une colonne).\n");
			// ⚠️ L'EXEMPLE EST UNE BOUTEILLE, PAS UN VERRE : le verre est dans le jeu
			//    d'epreuve (epreuve_revolution.txt). Le montrer mesurerait la recopie.
			ajout("Pas de largeur/hauteur/profondeur : le profil donne la taille. Exemple, une bouteille de 30 cm :\n");
			ajout("partie bouteille forme revolution profil 0.040 0 0.040 0.200 0.015 0.250 0.013 0.300 paroi 0.004 matiere verre\n\n");
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
				// ── les vues (rendu par l'application) ──
				int32 vuesEtape = -1;
				int32 vuesAttente = 0;
				char vuesPrefixe[200] = {0};
		};
		inline NkCreaEtat &NkCrea() {
			static NkCreaEtat s;
			return s;
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

		/// POSE LE DOCUMENT. Rend le numero du lot, ou -1 (et `b.motif` dit pourquoi).
		inline int32 NkCreaPoser(NkModelerState &st, const NkCreaDoc &d, const char *docTexte, const char *demande,
								 NkCreaBilan &b) {
			NkCreaEtat &E = NkCrea();
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
						for (int32 k = nc - 1; k >= 0; --k) {
							const float32 h = pc.profil[2 * k + 1];
							if (h < h0 + pc.paroi)
								break;
							pt(pc.profil[2 * k] - pc.paroi, h);
						}
						pt(0.f, h0 + pc.paroi);
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
		inline bool NkCreaVoieImage() {
			const char *v = std::getenv("NK_CREA_VOIE");
			return v && strcmp(v, "image") == 0;
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
			{
				// le dossier Genia/ du projet peut ne pas exister encore
				char dos[400];
				snprintf(dos, sizeof(dos), "%s", G.dorsal.sortie.CStr());
				if (char *b = strrchr(dos, '/'))
					*b = 0;
				NkDirectory::CreateRecursive(dos);
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
				snprintf(m, sizeof(m), "Voie (%s) : rien n'a ete genere -- %s", G.voie,
						 err.CStr() ? err.CStr() : "raison inconnue");
				(void)NkAiPousser(st, NkModelerState::AiType::Refus, m);
				return;
			}
			const char *un[1] = {rep.CStr()};
			const int32 avant = st.BrowserCount();
			const int32 importes = NkImportFiles(st, un, 1);
			snprintf(m, sizeof(m),
					 importes > 0
						 ? "Voie (%s) : maillage genere en %.0f s (la fenetre est restee vivante : %u images), carte "
						   "ajoutee au navigateur (%d). Glissez-la dans la scene."
						 : "Voie (%s) : le fichier genere en %.0f s (%u images) n'a pas pu etre importe (%d carte).",
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
		inline bool NkCreaLancerPlan(NkModelerState &st, converse::NkIConverseBackend *dorsal);

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
			if (E.descriptionImage[0]) {
				snprintf(avecImage, sizeof(avecImage),
						 "%s\n(L'utilisateur a joint une IMAGE de l'objet. Un modele de vision y voit : %s -- "
						 "respecte les parties, les formes et les proportions de cette image.)",
						 E.demande, E.descriptionImage);
				demande = avecImage;
			}
			static char invite[22000];
			NkCreaEcrireInvite(invite, sizeof(invite), demande);
			NkString pourquoi;
			if (!E.envoi.Lancer(dorsal, NkString(invite), pourquoi)) {
				char m[192];
				snprintf(m, sizeof(m), "L'assistant n'a pas pu etre appele : %s",
						 pourquoi.Data() ? pourquoi.Data() : "raison inconnue");
				(void)NkAiPousser(st, NkModelerState::AiType::Refus, m);
				return false;
			}
			(void)NkAiPousser(st, NkModelerState::AiType::Note,
							  "Voie : assemblage de parties nommees. Je demande au modele local un plan "
							  "(quelles parties, quelle forme, qui repose sur qui)...");
			std::printf("[crea] ENVOI : « %s » (dorsal %s)\n", demande, dorsal->Name());
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
			NkCreaLire(brut, d);
			if (d.impossible && d.n == 0) {
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
			if (d.n == 0 && nMot == 0) {
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
			if (d.n == 0) {
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
		inline void NkCreaDemarrerVues() {
			NkCreaEtat &E = NkCrea();
			const char *v = std::getenv("NK_CREA_VUES");
			char defaut[64];
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
					bool duLot = false;
					if (E.nLots > 0) {
						const NkCreaLot &l = E.lots[E.nLots - 1];
						for (int32 i = 0; i < l.nNoeuds && !duLot; ++i)
							duLot = (l.noeuds[i] == q);
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
			const bool boite = NkCreaBoiteDernierLot(mn, mx);
			const int32 e = E.vuesEtape;
			if (e == 0) {
				sHudAvant = demo::Demo3DHostHud();
				sCurseurAvant = demo::Demo3DHostCursorShown();
				demo::Demo3DHostSetHud(false);
				demo::Demo3DHostSetCursorShown(false);
				NkCreaIsoler(true);
				demo::Demo3DHostSelectEmptyNode(-1);
			}
			if (e == 2 || e == 4 || e == 6) {
				snprintf(chemin, sizeof(chemin), "%s_%s.png", E.vuesPrefixe, kNom[e / 2 - 1]);
				std::printf("[crea] VUE %s -> %s : %s\n", kNom[e / 2 - 1], chemin,
							demo::Demo3DHostCaptureView(chemin) ? "ecrite" : "ECHEC");
				std::fflush(stdout);
			}
			if (e == 6) {
				NkCreaIsoler(false);
				demo::Demo3DHostSetHud(sHudAvant);
				demo::Demo3DHostSetCursorShown(sCurseurAvant);
				E.vuesEtape = -1;
				if (NkCreaVoieTripoSR()) {
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
				NkCreaDemarrerVues();
				if (const char *a = std::getenv("NK_CREA_ANNULE"))
					E.annuleDans = (int32)std::atoi(a);
			}
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
				if (const char *q = std::getenv("NK_CREA_QUITTE"))
					if (*q && *q != '0')
						E.quitteDans = 5;
			}
			if (E.quitteDans > 0 && !NkGenia().envoi.EnCours() && --E.quitteDans == 0)
				st.running = false;
		}

	} // namespace nk3d
} // namespace nkentseu
