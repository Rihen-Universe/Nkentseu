#pragma once
// -----------------------------------------------------------------------------
// @File    NkContentBrowserDisque.h
// @Brief   LES GESTES DU NAVIGATEUR DE CONTENU SUR LE DISQUE : nouveau dossier,
//          copier, deplacer, dupliquer, renommer, supprimer, importer (fichiers
//          ET dossiers) -- tous CONFINES a la racine de contenu du projet, et
//          sans jamais ecraser. Plus la petite memoire du navigateur (couleurs
//          de dossiers, favoris, collections), rangee a cote du contenu.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI DANS LE KIT (2026-10-01, document 02 §3.1 d'UnkenyEditor)
// =============================================================================
//  Rihen, 01/10 : « on ne peut pas selectionner les dossiers ; pas de copier /
//  coller / couper ; pas de deplacer par glisser ; pas de poser des fichiers ou
//  des dossiers ». Ces gestes sont ceux de TOUT editeur de la famille (Nogee,
//  NkAnimaEditor, PV3DE, NKCraft) : les ecrire dans UnkenyEditor aurait fait la
//  premiere de cinq copies. Le composant de dessin (`NkContentBrowserModel.h`)
//  ne touche toujours pas au disque -- il SIGNALE ; ce fichier-ci est ce que
//  l'hote APPELLE quand il decide d'agir. Deux couches, deux responsabilites.
//
// =============================================================================
//  LES TROIS REGLES, ET ELLES SONT VERIFIEES PAR LE BANC D'UNKENYEDITOR (e51)
// =============================================================================
//  1. CONFINEMENT. Toute source et toute cible d'un geste DOIT etre sous la
//     racine (la racine elle-meme n'est jamais supprimee, renommee ni deplacee).
//     La verification est LEXICALE (separateurs, « . », « .. », casse ASCII) :
//     « Contenu/../../Windows » est refuse avant d'atteindre le systeme.
//     Seul l'IMPORT lit hors de la racine -- c'est sa definition -- et il n'y
//     ECRIT jamais ailleurs que dedans.
//  2. JAMAIS D'ECRASEMENT. Un nom pris devient « nom_2.ext », « nom_3.ext »
//     (la regle de l'import d'UnkenyEditor, montee ici). Renommer vers un nom
//     pris est REFUSE plutot que suffixe : l'utilisateur a choisi ce nom-la.
//  3. JAMAIS DE SUPPRESSION SANS CONFIRMATION. Ce fichier ne pose pas la
//     question (il n'a pas d'ecran) ; il offre la CORBEILLE de l'OS par defaut,
//     et l'hote ne l'appelle qu'apres sa boite de confirmation.
//
//  ⚠️ CE FICHIER NE CONNAIT NI NKGui NI LE COMPOSANT : il prend des chemins et
//     rend des chemins. Le banc l'exerce sur des dossiers temporaires.
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace editorkit {

		// ── LES CHEMINS ─────────────────────────────────────────────────────────
		/// Forme canonique LEXICALE : « / » partout, ni « . » ni segment vide, les
		/// « .. » resolus, pas de « / » final. Un chemin relatif reste relatif.
		NkString NkDisqueNormaliser(const char *chemin);
		/// `chemin` est-il `racine` ou en dessous ? Casse ASCII ignoree (Windows),
		/// les deux rendus ABSOLUS (repertoire courant) avant comparaison.
		bool NkDisqueSous(const char *racine, const char *chemin);
		/// Le dernier segment (« a/b/c.png » -> « c.png »).
		NkString NkDisqueNom(const char *chemin);
		/// Tout sauf le dernier segment (« a/b/c.png » -> « a/b »).
		NkString NkDisqueParent(const char *chemin);
		/// Un NOM (pas un chemin) acceptable : non vide, ni « . » ni « .. », aucun
		/// de `/ \ : * ? " < > |`, ni espace ni point final (Windows les retire).
		bool NkDisqueNomValide(const char *nom);
		/// `dossier/nom` s'il est libre, sinon `dossier/pied_2.ext`, `_3`... Vide si
		/// rien n'est libre avant 10 000 (on ne boucle pas sans fin).
		NkString NkDisqueCheminLibre(const char *dossier, const char *nom);

		// ── LES GESTES ──────────────────────────────────────────────────────────
		// Tous rendent le NOUVEAU chemin (dans la meme forme que leurs entrees),
		// ou une chaine vide si le geste est refuse ou a echoue. `raison`, si
		// fourni, recoit une phrase lisible (« hors du contenu », « nom deja
		// pris »...) -- l'hote l'annonce telle quelle.

		/// Cree `dossier/nomVoulu` (suffixe s'il est pris).
		NkString NkDisqueNouveauDossier(const char *racine, const char *dossier, const char *nomVoulu,
										NkString *raison = nullptr);
		/// Copie le fichier ou le dossier `source` DANS `dossierCible` (recursif,
		/// suffixe si le nom est pris). Refuse de copier un dossier dans lui-meme
		/// ou dans l'un de ses descendants.
		NkString NkDisqueCopier(const char *racine, const char *source, const char *dossierCible,
								NkString *raison = nullptr);
		/// Deplace `source` DANS `dossierCible`. Meme refus que la copie ; deplacer
		/// la ou il est deja ne fait rien et rend le chemin inchange.
		NkString NkDisqueDeplacer(const char *racine, const char *source, const char *dossierCible,
								  NkString *raison = nullptr);
		/// Une copie a cote de l'original (« caisse_2.png »).
		NkString NkDisqueDupliquer(const char *racine, const char *source, NkString *raison = nullptr);
		/// Renomme `source` en `nouveauNom` (un NOM). Refuse un nom pris.
		NkString NkDisqueRenommer(const char *racine, const char *source, const char *nouveauNom,
								  NkString *raison = nullptr);
		/// Supprime `chemin` (fichier ou dossier, recursif). `corbeille` : vers la
		/// corbeille de l'OS (recuperable) ; faux : definitif (le banc, sur ses
		/// dossiers temporaires). Jamais la racine elle-meme.
		bool NkDisqueSupprimer(const char *racine, const char *chemin, bool corbeille, NkString *raison = nullptr);

		/// Ce qu'un import a fait.
		struct NkDisqueRapport {
				uint32 fichiers = 0; ///< fichiers copies
				uint32 dossiers = 0; ///< dossiers crees
				uint32 refuses = 0;	 ///< fichiers que `accepte` a refuses
				uint32 echecs = 0;	 ///< copies impossibles (droits, disque)
				NkString premierRefus;
				/// Les chemins CREES au premier niveau (un par source importee).
				NkVector<NkString> crees;
		};
		/// Importe `sources` (chemins de l'OS, fichiers OU dossiers) dans
		/// `dossierCible` (sous `racine`). Un dossier est recopie avec son
		/// arborescence ; `accepte(user, fichier)` filtre les FICHIERS (nul = tout).
		/// Un dossier qui ne garde aucun fichier accepte est quand meme cree : il
		/// a ete depose, l'utilisateur doit le voir.
		NkDisqueRapport NkDisqueImporter(const char *racine, const char *dossierCible, const NkVector<NkString> &sources,
										 bool (*accepte)(void *user, const char *fichier), void *user);

		// ── LA MEMOIRE DU NAVIGATEUR ────────────────────────────────────────────
		// Unreal garde la couleur d'un dossier, les favoris et les collections dans
		// la configuration du projet. Ici : un fichier CACHE a la racine du contenu
		// (« .nknavigateur », ignore par les listes parce que son nom commence par
		// un point). Il voyage avec le projet. Il n'est ecrit QUE sur un geste de
		// l'utilisateur -- regarder le navigateur n'ecrit rien sur le disque.
		// Les cles sont des chemins RELATIFS a la racine (« » = la racine).
		struct NkDisqueCollection {
				NkString nom;
				uint32 couleur = 0; ///< 0xRRGGBBAA
				NkVector<NkString> elements;
		};
		struct NkDisqueMeta {
				NkVector<NkString> couleursCles;
				NkVector<uint32> couleurs; ///< parallele a `couleursCles`
				NkVector<NkString> favoris;
				NkVector<NkDisqueCollection> collections;

				/// 0 = la teinte du style.
				uint32 Couleur(const NkString &rel) const;
				void PoserCouleur(const NkString &rel, uint32 rgba); ///< 0 = retirer
				bool EstFavori(const NkString &rel) const;
				void BasculerFavori(const NkString &rel);
				/// `ancien` (et tout ce qui est dessous) s'appelle desormais `nouveau`.
				void Renommer(const NkString &ancien, const NkString &nouveau);
				/// `rel` (et tout ce qui est dessous) n'existe plus.
				void Oublier(const NkString &rel);
		};
		/// Le nom du fichier de memoire, a la racine.
		constexpr const char *NK_DISQUE_META = ".nknavigateur";
		/// Lit la memoire (absente = vide, sans erreur).
		bool NkDisqueMetaLire(const char *racine, NkDisqueMeta &meta);
		/// Ecrit la memoire. Cree la racine au besoin.
		bool NkDisqueMetaEcrire(const char *racine, const NkDisqueMeta &meta);

	} // namespace editorkit
} // namespace nkentseu
