// -----------------------------------------------------------------------------
// FICHIER: Kernel\Runtime\NKRenderer\src\NKRenderer\Mesh\NkMeshR32.h
// DESCRIPTION: R32 — adresses stables sur la peau d'une creature, et pile
//              d'operations (C3) rejouee par-dessus une base regeneree (C2).
//              Palier G0 du generateur de creatures de NKCraft.
// AUTEUR: TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// DATE: 2026-09-29
// VERSION: 0.1.0
// -----------------------------------------------------------------------------
//
// SPECIFICATION
//   Applications/NKCraft/design/04-generateur-creatures-nkcraft.md :
//   §3.2 (l'adresse), §3.2bis (ce qui la casse), §10 (humain <-> IA), §14 G0.
//   Tools/Genia/FORMAT_SCENE.md §8 (R32, les designations).
//
//     document -> BASE regeneree (C2) + PILE D'OPERATIONS rejouee (C3)
//
// L'INVARIANT (04 §3.2bis)
//   Une adresse (os, s, a) survit a toute regeneration qui preserve le NOM de
//   l'os, son PARAMETRAGE (s = 0 racine, s = 1 extremite) et sa REFERENCE
//   D'ORIENTATION. Les cinq cas qui la cassent passent par la LIGNEE :
//     1 renomme   -> report propose, a confirmer ; refuse => orpheline
//     2 coupe     -> report CALCULE (s renormalise dans le bon morceau)
//     3 fusionne  -> report calcule ; les collisions sont SIGNALEES
//     4 reference -> 🔴 le seul DANGEREUX : sans garde, le report est faux SANS
//                    RIEN DIRE. Chaque operation GELE (version, vecteur) de la
//                    reference a sa creation ; une reorientation versionnee est
//                    reportee AVEC une alerte ; une reference changee hors
//                    versionnement est REFUSEE (orpheline montree).
//     5 disparu   -> orpheline montree, jamais effacee
//
// ⚠️ CE QUE CE MODULE NE FAIT PAS — ECRIT AVANT QU'ON LE DECOUVRE
//   - La peau n'est PAS celle du §5 : ni boucles d'articulation, ni jonctions
//     (un tube separe par os, ferme par un n-gone a chaque bout), ni volumes
//     C1. C'est G1, G2, G3. Elle existe pour PORTER DES ADRESSES.
//   - La reference d'orientation est un vecteur du repere du DOCUMENT (pose de
//     repos). L'exprimer dans le repere du parent, pour suivre une pose, releve
//     du rig (G4).
//   - `adresse:` ne vise que des faces de tube encore D'ORIGINE ; la geometrie
//     creee par une operation se vise par `groupe:` ou `faces:`.
//   - Pas de symetrie (_g -> _d), pas de sculpt C4 (G3), pas de `zone:` ni de
//     `boucle:` (refus nomme).
//
// NOMMAGE DES VALEURS D'ENUMERATION (Rodolf, 29/09) : `Nk_<Type sans Nk>_<Valeur>`
//   (NkR32Statut::Nk_R32Statut_Orpheline). Elles se distinguent ainsi d'un coup
//   d'oeil des macros et constantes (`NK_...`) et des types (`NkPascal`).
//
// MUTATIONS VIVANTES — le banc NKR32Harness doit rougir sous CHACUNE
//   NK_R32_MUTE=1  ignorer la reference (ni decalage de version, ni refus)
//   NK_R32_MUTE=2  coupe sans renormaliser s
//   NK_R32_MUTE=3  une orpheline ARRETE le rejeu
//   NK_R32_MUTE=4  renommage reporte SANS confirmation
//   (Le rejeu par INDICES n'est pas une mutation : le banc le mesure en
//   rejouant tel quel le journal classique, NkR32Etat::journal.)
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_NKRENDERER_SRC_NKRENDERER_MESH_NKMESHR32_H_INCLUDED
#define NK_NKRENDERER_SRC_NKRENDERER_MESH_NKMESHR32_H_INCLUDED

// ============================================================
// INCLUDES
// ============================================================

// Nkentseu headers
#include "NKRenderer/Mesh/NkEditMesh.h"

// ============================================================
// DECLARATIONS
// ============================================================

namespace nkentseu {
	namespace renderer {

		// ========================================
		// CONSTANTS
		// ========================================

		/** @brief Longueur maximale d'un nom d'os ou de groupe (zero final compris). */
		static const uint32 NK_R32_NOM_MAX = 32u;
		/** @brief Longueur maximale d'une designation ecrite. */
		static const uint32 NK_R32_CIBLE_MAX = 128u;
		/** @brief Longueur maximale d'un message de resultat. */
		static const uint32 NK_R32_MESSAGE_MAX = 256u;
		/** @brief Alerte : une reorientation VERSIONNEE a ete reportee (a verifier). */
		static const uint8 NK_R32_ALERTE_REORIENTATION = 1u;
		/** @brief Alerte : deux retouches visent le meme endroit apres une fusion. */
		static const uint8 NK_R32_ALERTE_COLLISION = 2u;

		// ========================================
		// C0 — LE SQUELETTE MINIMAL
		// ========================================

		/**
		 * @brief Une clef de profil : la section (largeur x hauteur, dimensions
		 *        TOTALES) a la position `s` le long de l'os (04 §4.2, `profil ... clefs`).
		 */
		struct NkR32Clef {
				float32 s = 0.f;
				float32 largeur = 0.2f;
				float32 hauteur = 0.2f;
		};

		/**
		 * @brief Un os nomme du squelette C0.
		 *
		 * L'os va de sa racine a son extremite ; `s` le parametre (0 a la racine,
		 * 1 a l'extremite) et `a` tourne autour de lui a partir de la reference
		 * d'orientation projetee perpendiculairement a l'os.
		 *
		 * @invariant `versionReference` ne change que par NkR32Document::ReorienterOs.
		 */
		struct NkR32Os {
				/** @brief Nom unique : c'est la cle de toute adresse. */
				char nom[NK_R32_NOM_MAX] = {0};
				/** @brief Vide = racine posee a `racine` ; sinon l'os part de l'extremite du parent. */
				char parent[NK_R32_NOM_MAX] = {0};
				NkVec3f racine = {0.f, 0.f, 0.f};
				/** @brief Normalisee a la construction de la peau. */
				NkVec3f direction = {0.f, 1.f, 0.f};
				float32 longueur = 1.f;
				/** @brief Rayon a s = 0. */
				float32 rayon0 = 0.1f;
				/** @brief Rayon a s = 1. */
				float32 rayon1 = 0.1f;
				/**
				 * @brief La reference d'orientation : fixe a = 0 autour de l'os.
				 * @warning GELEE. La changer autrement que par ReorienterOs est le cas 4
				 *          du §3.2bis : le rejeu le DETECTE et refuse le report.
				 */
				NkVec3f reference = {0.f, 0.f, 1.f};
				uint32 versionReference = 1u;
				/**
				 * @brief Le profil (G1). VIDE = cercle de rayon rayon0 -> rayon1 (le cas G0).
				 *        Sinon la section est une superellipse de dimensions interpolees
				 *        entre les clefs, triees par s croissant.
				 */
				NkVector<NkR32Clef> clefs;
				/** @brief Exposant de la superellipse : 2 = ellipse, plus = plus carre. */
				float32 exposant = 2.f;
		};

		/** @brief Nature d'une entree de lignee (ce que le squelette est devenu). */
		enum class NkR32LigneeType : uint8 {
			Nk_R32LigneeType_Renomme = 0,  ///< a -> b
			Nk_R32LigneeType_Coupe,  ///< a -> b (s < t) + c (s >= t)
			Nk_R32LigneeType_Fusion,  ///< a (haut) + b (bas) -> c ; t = part de a
			Nk_R32LigneeType_Reoriente,  ///< reference de a : versionAvant -> versionApres
			Nk_R32LigneeType_Supprime  ///< a disparait
		};

		/**
		 * @brief Une entree de la lignee du squelette.
		 *
		 * Ecrite par les methodes de NkR32Document, dans l'ordre. Une operation
		 * de la pile retient la longueur de la lignee a sa creation (`epoque`) :
		 * au rejeu, seules les entrees POSTERIEURES la concernent.
		 */
		struct NkR32Lignee {
				NkR32LigneeType type = NkR32LigneeType::Nk_R32LigneeType_Renomme;
				char a[NK_R32_NOM_MAX] = {0};
				char b[NK_R32_NOM_MAX] = {0};
				char c[NK_R32_NOM_MAX] = {0};
				float32 t = 0.5f;
				NkVec3f refAvant = {0.f, 0.f, 1.f};
				NkVec3f refApres = {0.f, 0.f, 1.f};
				/** @brief Axe de l'os au moment de la reorientation (l'angle se mesure autour). */
				NkVec3f axe = {0.f, 1.f, 0.f};
				uint32 versionAvant = 0u;
				uint32 versionApres = 0u;
		};

		/**
		 * @brief Le document C0 dont G0 a besoin (la partie squelette du `.nkscene` v2).
		 *
		 * @warning Les operations de squelette passent par les methodes : elles
		 *          ECRIVENT LA LIGNEE. Modifier `os` directement reste possible (c'est
		 *          ce que fait un editeur de texte) — et c'est precisement le cas que
		 *          le rejeu doit DETECTER au lieu de reporter faux.
		 */
		struct NkR32Document {
				NkVector<NkR32Os> os;
				NkVector<NkR32Lignee> lignee;
				/** @brief Sommets par anneau (M). PAIR, au moins 4 (04 §5.4). */
				uint32 anneaux = 12u;
				/**
				 * @brief Anneaux le long de l'os, par unite de longueur. 0 = AUTOMATIQUE
				 *        (G1) : l'espacement suit le pas autour de l'os, pour des quads
				 *        proches du carre (04 §5.9 « la resolution suit la forme »).
				 */
				float32 densite = 8.f;
				/**
				 * @brief La peau a construire : 1 = un tube par os (G0, porte les adresses) ;
				 *        2 = un tube continu par CHAINE, boucles d'articulation, capuchons en
				 *        quads (G1, 04 §5.1-5.4).
				 */
				uint32 generateur = 1u;
				/** @brief Boucles par articulation (G1). 3 seulement en G1. */
				uint32 boucles = 3u;
				/**
				 * @brief Demi-largeur de la bande de pli (G1), en RAYONS de la section au
				 *        joint : les boucles se posent a +-largeurPli x rayon du joint. 0 =
				 *        un demi-pas (serre au maximum).
				 */
				float32 largeurPli = 0.f;
				/** @brief Nom de la creature (`creature "..."`). */
				char nom[64] = {0};

				/** @brief Indice de l'os `nom`, ou -1. */
				int32 Trouver(const char *nom) const;
				/** @brief Ajoute un os. Refus nomme : nom vide ou en double, parent inconnu, longueur ou rayon nuls. */
				bool AjouterOs(const NkR32Os &o, char *pourquoi, uint32 cap);
				/** @brief Cas 1. Ecrit la lignee ; les enfants suivent le nouveau nom. */
				bool RenommerOs(const char *ancien, const char *nouveau, char *pourquoi, uint32 cap);
				/**
				 * @brief Cas 2 : insere une articulation a `t` ; `nom` devient `haut` [0, t] + `bas` [t, 1].
				 * @pre 0 < t < 1 ; `haut` et `bas` n'existent pas.
				 */
				bool CouperOs(const char *nom, float32 t, const char *haut, const char *bas, char *pourquoi,
							  uint32 cap);
				/**
				 * @brief Cas 3 : `haut` + `bas` -> `nouveau`.
				 * @pre `bas` a `haut` pour parent, `haut` n'a pas d'autre enfant, et les deux
				 *      ont la MEME reference (sinon refus nomme : reorienter d'abord).
				 */
				bool FusionnerOs(const char *haut, const char *bas, const char *nouveau, char *pourquoi, uint32 cap);
				/**
				 * @brief Cas 4, la seule voie legitime : versionnee, ecrite dans la lignee ;
				 *        le rejeu reporte en decalant `a` et PREVIENT (alerte).
				 */
				bool ReorienterOs(const char *nom, const NkVec3f &nouvelleReference, char *pourquoi, uint32 cap);
				/** @brief Cas 5. Refus nomme si l'os a des enfants. */
				bool SupprimerOs(const char *nom, char *pourquoi, uint32 cap);
		};

		// ========================================
		// C2 — LA PEAU ADRESSEE
		// ========================================

		/** @brief Ou se trouve une face de base sur son os. */
		enum class NkR32Lieu : uint8 {
			Nk_R32Lieu_Tube = 0,
			Nk_R32Lieu_BoutDebut,  ///< le n-gone qui ferme s = 0
			Nk_R32Lieu_BoutFin  ///< le n-gone qui ferme s = 1
		};

		/**
		 * @brief Adresse d'une face de la base : son os et son etendue en (s, a).
		 *
		 * `a` est dans [0, 1[, mesure depuis la reference, dans le sens
		 * reference -> (direction x reference).
		 */
		struct NkR32AdresseFace {
				uint16 os = 0;
				NkR32Lieu lieu = NkR32Lieu::Nk_R32Lieu_Tube;
				float32 s0 = 0.f;
				float32 s1 = 0.f;
				float32 a0 = 0.f;
				float32 a1 = 0.f;
		};

		/**
		 * @brief Adresse d'un SOMMET de la peau (G1) : sert aux mesures du §6 (boucles
		 *        d'articulation, distance des poles aux bandes de pli, poids par
		 *        construction du banc de pose).
		 */
		struct NkR32AdresseSommet {
				uint16 os = 0;
				NkR32Lieu lieu = NkR32Lieu::Nk_R32Lieu_Tube;
				float32 s = 0.f;
				float32 a = 0.f;
				/** @brief Indice de l'anneau LE LONG DE LA CHAINE (capuchon : au-dela des bouts). */
				int32 anneau = 0;
				/** @brief Indice de la chaine. */
				uint16 chaine = 0;
				/** @brief 1 si l'anneau est une boucle d'articulation (bande de pli). */
				uint8 estBoucle = 0u;
		};

		/**
		 * @brief La peau C2 : le maillage de base et l'adresse de chacune de ses faces.
		 * @invariant maillage.faces[f].origine == f + 1 a la sortie de NkR32ConstruirePeau.
		 */
		struct NkR32Peau {
				NkEditMesh maillage;
				NkVector<NkR32AdresseFace> adresses;
				/** @brief Copie des os au moment de la construction. */
				NkVector<NkR32Os> os;
				/** @brief Adresse de chaque sommet (G1 ; vide en G0). */
				NkVector<NkR32AdresseSommet> sommets;
		};

		/** @brief Le repere d'un os dans le document (rest pose). */
		struct NkR32Repere {
				NkVec3f racine;
				NkVec3f axe;
				/** @brief a = 0 */
				NkVec3f e1;
				/** @brief a = 1/4 */
				NkVec3f e2;
				float32 longueur = 1.f;
		};

		/** @brief Reperes de tous les os (dans l'ordre du document). Refus nomme si un repere n'est pas defini. */
		bool NkR32CalculerReperes(const NkR32Document &doc, NkVector<NkR32Repere> &reps, char *pourquoi, uint32 cap);

		/** @brief Demi-axes de la section de `o` a `s` (profil, ou cercle rayon0 -> rayon1). */
		void NkR32Section(const NkR32Os &o, float32 s, float32 &demiLargeur, float32 &demiHauteur);

		/**
		 * @brief Point de la peau analytique de l'os `o` a (s, a), et sa normale sortante.
		 * @note Superellipse : x = A sgn(c)|c|^(2/n), y = B sgn(s)|s|^(2/n), theta = 2 pi a.
		 */
		NkVec3f NkR32PointSection(const NkR32Repere &r, const NkR32Os &o, float32 s, float32 a, NkVec3f *normale);

		/**
		 * @brief Construit la peau C2 minimale depuis le document.
		 *
		 * @param[in]  doc Le document.
		 * @param[out] out La peau.
		 * @param[out] pourquoi Refus nomme (anneaux impair, reference parallele a l'os,
		 *                      parent declare apres son enfant...).
		 * @return false si la peau ne se construit pas.
		 * @post Deterministe : meme document -> meme maillage AU BIT.
		 */
		bool NkR32ConstruirePeau(const NkR32Document &doc, NkR32Peau &out, char *pourquoi, uint32 cap);

		/**
		 * @brief Point et normale ANALYTIQUES de la peau a (os, s, a).
		 *
		 * @note Sert aux bancs a juger un report SANS passer par la resolution de
		 *       R32 : sinon le juge et l'accuse seraient la meme fonction.
		 */
		bool NkR32PointAnalytique(const NkR32Document &doc, const char *os, float32 s, float32 a, NkVec3f &point,
								  NkVec3f &normale, float32 *rayon = nullptr);

		/** @brief Coordonnees (s, a, rho) d'un point autour d'un os (rho = distance a l'axe). */
		bool NkR32Projeter(const NkR32Document &doc, const char *os, const NkVec3f &p, float32 &s, float32 &a,
						   float32 &rho);

		// ========================================
		// C3 — LA PILE D'OPERATIONS
		// ========================================

		/** @brief Qui a ecrit l'operation (04 §10.2). */
		enum class NkR32Auteur : uint8 {
			Nk_R32Auteur_Humain = 0,
			Nk_R32Auteur_Auto,
			Nk_R32Auteur_Ia
		};

		/** @brief Les verbes de G0. */
		enum class NkR32Verbe : uint8 {
			Nk_R32Verbe_Extruder = 0,  ///< valeur = distance le long de la normale moyenne
			Nk_R32Verbe_Deplacer,  ///< local = (normale, tangente de l'os, bitangente)
			Nk_R32Verbe_Echelle,  ///< valeur = facteur, autour du centre de la cible
			Nk_R32Verbe_Inserer,  ///< valeur = epaisseur, valeur2 = profondeur
			Nk_R32Verbe_Supprimer
		};

		/**
		 * @brief Une operation C3, ciblee SANS INDICE.
		 *
		 * Designations : `partie:<os>`, `adresse:(<os>, s0..s1, a0..a1)`,
		 * `faces:normale>+Y[:seuil][@partie:<os>]`, `groupe:<nom>`.
		 * `zone:` et `boucle:` sont reconnues et REFUSEES NOMMEMENT (pas en G0).
		 */
		struct NkR32Operation {
				NkR32Auteur auteur = NkR32Auteur::Nk_R32Auteur_Humain;
				NkR32Verbe verbe = NkR32Verbe::Nk_R32Verbe_Extruder;
				/** @brief La designation, LISIBLE (04 §3.2 : une adresse s'ecrit et se lit). */
				char cible[NK_R32_CIBLE_MAX] = {0};
				/** @brief Nom du resultat, reutilisable par `groupe:` ("" = aucun). */
				char groupe[NK_R32_NOM_MAX] = {0};
				float32 valeur = 0.f;
				float32 valeur2 = 0.f;
				NkVec3f local = {0.f, 0.f, 0.f};
				/** @brief GELE : longueur de la lignee a la creation. */
				uint32 epoque = 0u;
				/** @brief GELE : version de la reference de l'os vise (0 = aucun os vise). */
				uint32 refVersion = 0u;
				/** @brief GELE : le vecteur de reference lui-meme. */
				NkVec3f refInstantane = {0.f, 0.f, 0.f};
		};

		/** @brief La pile C3 : rejouee dans l'ordre apres chaque regeneration. */
		struct NkR32Pile {
				NkVector<NkR32Operation> ops;

				/**
				 * @brief Ajoute une operation en GELANT son epoque et la reference de l'os vise.
				 * @return false (refus nomme) si la designation ne se lit pas, ou si l'os vise n'existe pas.
				 */
				bool Ajouter(const NkR32Document &doc, const NkR32Operation &op, char *pourquoi, uint32 cap);
		};

		/**
		 * @brief Ecrit la pile en texte, une operation par ligne (04 §4.1 : `retouches fichier "x.nkr"`).
		 * @note Nombres ecrits SANS dependre de la locale (pas de virgule decimale en fr-FR).
		 */
		void NkR32PileEcrire(const NkR32Pile &p, char *out, uint32 cap);
		/** @brief Relit une pile ecrite par NkR32PileEcrire (ou a la main). */
		bool NkR32PileLire(const char *texte, NkR32Pile &p, char *pourquoi, uint32 cap);

		/** @brief Reponse humaine a un renommage propose (cas 1). */
		struct NkR32Confirmation {
				char ancien[NK_R32_NOM_MAX] = {0};
				char nouveau[NK_R32_NOM_MAX] = {0};
				bool estAccepte = false;
		};

		/** @brief Ce qu'il est advenu d'une operation au rejeu. */
		enum class NkR32Statut : uint8 {
			Nk_R32Statut_Appliquee = 0,  ///< cible trouvee telle quelle
			Nk_R32Statut_Reportee,  ///< cible trouvee APRES un report de lignee
			Nk_R32Statut_Orpheline,  ///< gardee, montree, NON appliquee
			Nk_R32Statut_AConfirmer  ///< renommage propose, en attente (non appliquee)
		};

		/** @brief Pourquoi une operation n'a pas ete appliquee. */
		enum class NkR32Cause : uint8 {
			Nk_R32Cause_Aucune = 0,
			Nk_R32Cause_OsDisparu,
			Nk_R32Cause_RenommageAConfirmer,
			Nk_R32Cause_RenommageRefuse,
			Nk_R32Cause_ReferenceChangeeHorsVersion,
			Nk_R32Cause_CibleVide,
			Nk_R32Cause_DesignationInconnue,
			Nk_R32Cause_DesignationPasEnG0,
			Nk_R32Cause_GroupeInconnu,
			Nk_R32Cause_OperationEchouee,
			Nk_R32Cause_RejeuInterrompu  ///< seulement sous NK_R32_MUTE=3
		};

		/** @brief Le compte rendu d'une operation : ce qui se MONTRE a l'artiste. */
		struct NkR32Resultat {
				NkR32Statut statut = NkR32Statut::Nk_R32Statut_Orpheline;
				NkR32Cause cause = NkR32Cause::Nk_R32Cause_Aucune;
				/** @brief Combinaison de NK_R32_ALERTE_* : prevenir n'empeche pas d'appliquer. */
				uint8 alertes = 0u;
				uint32 facesVisees = 0u;
				char message[NK_R32_MESSAGE_MAX] = {0};
		};

		/**
		 * @brief Role d'une face derivee par rapport a sa mere.
		 *
		 * Une face d'extrusion a pour adresse « la face (os, s, a) de la base,
		 * prolongee par l'operation k, en bout (ou en flanc) ». Stable d'une
		 * regeneration a l'autre, puisque l'operation est rejouee.
		 */
		enum class NkR32Role : uint8 {
			Nk_R32Role_Base = 0,
			Nk_R32Role_Bout,
			Nk_R32Role_Flanc
		};

		/** @brief Identite d'une face : ce que porte `Face::origine` (identifiant = indice + 1). */
		struct NkR32Origine {
				/** @brief Identifiant d'origine de la mere (0 = aucune). */
				uint32 parent = 0u;
				/** @brief Indice de la face de base ancetre. */
				uint32 faceBase = 0u;
				int32 op = -1;
				NkR32Role role = NkR32Role::Nk_R32Role_Base;
		};

		/** @brief Un resultat nomme, que `groupe:` retrouve par ses origines (jamais par indices). */
		struct NkR32Groupe {
				char nom[NK_R32_NOM_MAX] = {0};
				NkVector<uint32> origines;
		};

		/** @brief L'etat apres un rejeu. */
		struct NkR32Etat {
				/** @brief La base C2 regeneree. */
				NkR32Peau peau;
				/** @brief Base + pile rejouee. */
				NkEditMesh maillage;
				/** @brief origines[id - 1]. */
				NkVector<NkR32Origine> origines;
				NkVector<NkR32Groupe> groupes;
				/** @brief Un par operation, dans l'ordre de la pile. */
				NkVector<NkR32Resultat> resultats;
				/**
				 * @brief Les commandes EFFECTIVEMENT appliquees, cibles resolues en INDICES.
				 *
				 * C'est le journal classique de l'editeur. Le rejouer tel quel sur une
				 * autre base est le temoin « rejeu par indices » du banc.
				 */
				NkMeshEditRecorder journal;
		};

		/**
		 * @brief Regenere la base depuis `doc` et rejoue `pile` par-dessus.
		 *
		 * @return false seulement si la BASE ne se construit pas. Une operation qui
		 *         ne trouve pas sa cible devient une orpheline (resultats), et les
		 *         autres continuent (R32.7).
		 * @post Pile vide => `maillage` identique AU BIT a `peau.maillage`.
		 */
		bool NkR32Rejouer(const NkR32Document &doc, const NkR32Pile &pile, const NkR32Confirmation *confirmations,
						  uint32 nbConfirmations, NkR32Etat &out, char *pourquoi, uint32 cap);

		/**
		 * @brief Selection libre -> adresse (04 §10.3).
		 *
		 * Des faces choisies a la main deviennent une designation `adresse:(...)`
		 * qui survit a la regeneration.
		 *
		 * @return false (refus nomme) si une face n'est pas une face de tube d'origine,
		 *         si la selection couvre plusieurs os, ou si elle n'est pas une boite
		 *         en (s, a) : G0 ne sait pas la convertir sans en changer le sens.
		 */
		bool NkR32ConvertirSelection(const NkR32Etat &etat, const uint8 *faceSel, uint32 nbFaces, char *outCible,
									 uint32 capCible, char *pourquoi, uint32 cap);

		/** @brief Les faces du maillage COURANT que designe `cible` (sans lignee). */
		bool NkR32Designer(const NkR32Etat &etat, const char *cible, NkVector<uint32> &faces, char *pourquoi,
						   uint32 cap);

		/**
		 * @brief Empreinte 64 bits du maillage : positions AU BIT et boucles des faces.
		 * @note A imprimer en hexadecimal (identique sur toutes les plateformes).
		 */
		uint64 NkR32Empreinte(const NkEditMesh &m);

		const char *NkR32StatutNom(NkR32Statut s);
		const char *NkR32CauseNom(NkR32Cause c);

	}  // namespace renderer
}  // namespace nkentseu

#endif  // NK_NKRENDERER_SRC_NKRENDERER_MESH_NKMESHR32_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Creation Date: 2026-09-29
// ============================================================
