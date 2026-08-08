#pragma once
// =============================================================================
// NkModelerProject.h — le PROJET du modeleur : un dossier + un fichier .nk3dm.
//
// La regle vient de CONVENTIONS_FICHIERS.md (decision de Rihen, 5 aout 2026) :
//   - un projet est UN DOSSIER, avec un fichier `.nk3dm` a sa racine ;
//   - AUCUNE arborescence n'est imposee -- le navigateur du modeleur gere deja
//     les dossiers, lui en imposer une ferait deux organisations concurrentes ;
//   - tous les chemins stockes sont RELATIFS au dossier du projet, pour qu'un
//     projet deplace, copie ou envoye continue de fonctionner.
//
// CE QUE LE .nk3dm CONTIENT
//   version du format, nom, dates de creation et de modification, chemin
//   RELATIF de l'image de couverture, et la section `scene`.
//
//   CETTE SECTION EST DESORMAIS REMPLIE (format 2). Ce module n'en connait
//   pourtant PAS le contenu : il la recoit deja composee, sous forme
//   d'archive, et se contente de l'ecrire puis de la relire telle quelle.
//   C'est `Project/NkModelerScene.h` qui la compose -- lui seul connait la
//   scene, ici on ne sait qu'ecrire un fichier.
//
//   Passer un `scene` nul reste legitime et HONNETE : la section s'ecrit alors
//   `"serialisee": false`, exactement comme avant. Un fichier ne doit jamais
//   laisser croire qu'il porte un travail qu'on ne lui a pas donne.
//
// POURQUOI UN .cpp ET PAS UN EN-TETE SEUL, comme le reste du Shell : ce fichier
// lie NKFileSystem, NKSerialization et NKTime. Le laisser en en-tete ferait
// entrer ces trois modules dans chaque unite de compilation qui inclut l'ecran.
// =============================================================================

#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {

	// DECLAREE, PAS INCLUSE. NKSerialization n'a rien a faire dans chaque unite
	// qui inclut l'ecran d'accueil -- c'est la raison meme du .cpp (cf. le pied
	// de l'en-tete). Les fonctions ci-dessous n'en manipulent que des pointeurs,
	// une declaration anticipee suffit ; qui compose vraiment une archive inclut
	// NkArchive.h de son cote.
	class NkArchive;

	namespace nk3d {

		// Version du FORMAT, pas de l'application. Elle est ecrite dans chaque
		// fichier et relue a l'ouverture : un projet ecrit par une version future
		// se signale au lieu d'etre lu de travers.
		//
		// 1 -> 2 (serialisation de la scene) : la section `scene` porte desormais
		// un contenu reel. Les fichiers de format 1 restent lisibles -- leur
		// section dit `"serialisee": false` et rien n'en est restaure, ce qui est
		// exactement ce qu'ils annoncaient.
		//
		// 2 -> 3 (UN FICHIER PAR ASSET, decision de Rihen du 8 aout) : le .nk3dm
		// ne porte plus AUCUNE donnee d'asset -- seulement la configuration
		// globale, l'arbre du navigateur et le CHEMIN RELATIF du fichier de chaque
		// carte. Le contenu vit dans `.nkscene` / `.nkmesh` / `.nkmat`.
		// Un fichier de format 2 reste lu tel quel, puis reecrit dans la nouvelle
		// disposition au premier enregistrement : c'est une MIGRATION, personne ne
		// perd son projet parce que la disposition a change.
		static const int32 kProjectFormatVersion = 3;

		// Extension du projet du modeleur. Elle vit ICI et nulle part ailleurs --
		// la lecon des tables recopiees (cf. CONVENTIONS_FICHIERS.md §3).
		static const char *const kProjectExt = "nk3dm";

		// ── ETAT DU PROJET OUVERT ───────────────────────────────────────────────
		// `open` a faux = aucun projet : c'est l'etat de l'ecran d'accueil, pas une
		// valeur sentinelle d'erreur.
		struct NkProjectState {
				NkString file;	   ///< chemin ABSOLU du .nk3dm
				NkString root;	   ///< dossier du projet (le dossier du .nk3dm)
				NkString name;	   ///< nom affiche (defaut : nom du fichier)
				NkString coverRel; ///< image de couverture, RELATIVE a `root` ("" = aucune)
				NkString created;  ///< date de creation, AAAA-MM-JJ
				NkString modified; ///< date du dernier enregistrement, AAAA-MM-JJ
				bool open = false;
				// Vrai tant que le fichier n'a jamais ete ECRIT sur disque. Sert a
				// « Enregistrer » : sans chemin, il doit se comporter comme
				// « Enregistrer sous... » au lieu d'echouer en silence.
				bool neverSaved = true;

				void Reset() {
					file.Clear();
					root.Clear();
					name.Clear();
					coverRel.Clear();
					created.Clear();
					modified.Clear();
					open = false;
					neverSaved = true;
				}
		};

		// ── OPERATIONS ──────────────────────────────────────────────────────────
		// Toutes renvoient false et remplissent `err` (s'il est fourni) plutot que
		// de lever : le modeleur affiche le message, il ne meurt pas.

		// LA SCENE VOYAGE EN PARAMETRE FACULTATIF, et non dans NkProjectState :
		// l'etat du projet est ce qu'on garde OUVERT (chemin, nom, dates), la
		// scene est ce qu'on ECRIT a un instant donne. Les confondre obligerait a
		// tenir une copie de toute la scene en permanence, et a se demander a
		// chaque enregistrement si elle est encore a jour.
		// `scene` a nullptr => section `"serialisee": false`, comme avant.

		/// Cree `<parentDir>/<name>/` puis y ecrit `<name>.nk3dm`. Echoue si le
		/// dossier existe deja et n'est pas vide : ecraser un dossier de travail
		/// serait la pire des surprises.
		bool NkProjectCreate(const char *parentDir, const char *name, NkProjectState &out,
							 NkString *err, const NkArchive *scene = nullptr);

		/// Ecrit le .nk3dm a `st.file`. Met a jour la date de modification et
		/// choisit la couverture (cf. NkProjectPickCover).
		bool NkProjectSave(NkProjectState &st, NkString *err, const NkArchive *scene = nullptr);

		/// Racine (dossier) qu'AURAIT le projet s'il etait enregistre sous `file`.
		/// Existe pour « Enregistrer sous... » : la scene doit etre capturee avec
		/// la racine de DESTINATION, sinon ses chemins relatifs resteraient
		/// calcules depuis l'ancien dossier et pointeraient a cote a la
		/// reouverture. Applique exactement la meme normalisation que
		/// NkProjectSaveAs -- c'est le meme point de passage.
		NkString NkProjectRootFor(const char *file);

		/// Ecrit le projet SOUS UN AUTRE CHEMIN et bascule dessus. Le dossier du
		/// nouveau fichier devient la nouvelle racine.
		bool NkProjectSaveAs(NkProjectState &st, const char *file, NkString *err,
							 const NkArchive *scene = nullptr);

		/// Lit un .nk3dm. `out` n'est modifie qu'en cas de succes.
		/// `scene`, s'il est fourni, recoit la section scene TELLE QUELLE : c'est
		/// a l'appelant d'en faire quelque chose (ou de la laisser tomber si elle
		/// annonce ne rien contenir). Il est vide quand la section est absente ou
		/// se declare non serialisee -- la distinction n'a pas lieu d'etre ici.
		bool NkProjectLoad(const char *file, NkProjectState &out, NkString *err,
						   NkArchive *scene = nullptr);

		/// Cherche la capture la PLUS RECENTE sous la racine du projet et la
		/// retient comme couverture (chemin relatif). Ne FABRIQUE aucune image :
		/// le rendu se declenche par un bouton, pas par un enregistrement.
		/// Renvoie true si la couverture a change.
		bool NkProjectPickCover(NkProjectState &st);

		// ── PROJETS RECENTS ─────────────────────────────────────────────────────
		// Meme patron que l'IDE frere NKCode (`~/.nkcode_recent.cfg`) : un fichier
		// texte, UNE LIGNE PAR ENTREE, prefixe « P » = epingle / « R » = recent,
		// les epingles restant en tete. On ajoute des CHAMPS a la ligne, separes
		// par « | » -- le meme separateur que NKCode emploie deja pour ses noms
		// personnalises :
		//
		//     P <chemin .nk3dm>|<nom>|<couverture relative>|<date>
		//
		// La couverture est memorisee DANS L'ENTREE (demande de Rihen) : l'ecran
		// d'accueil affiche la vignette sans avoir a ouvrir chaque projet, et un
		// projet devenu injoignable garde quand meme sa carte lisible.
		struct NkRecentEntry {
				NkString path;	  ///< chemin absolu du .nk3dm
				NkString name;
				NkString cover;	  ///< RELATIF au dossier du projet ("" = aucune)
				NkString date;
				bool pinned = false;
				// Identifiant de texture de la vignette, 0 = pas encore chargee ou
				// image absente. Rempli par la couche d'affichage, pas par ce module :
				// televerser une image demande le renderer, dont le projet n'a que
				// faire.
				uint32 tex = 0;

				/// Chemin ABSOLU de la couverture, ou chaine vide.
				NkString CoverAbs() const;
		};

		struct NkRecentList {
				NkVector<NkRecentEntry> items; ///< epingles d'abord, puis les recents
				// Passe a vrai des que la liste change : la couche d'affichage
				// recharge alors les vignettes. Sans ce drapeau elle rechargerait
				// des images a chaque image de l'ecran d'accueil.
				bool texDirty = true;

				void Load();
				void Save() const;
				/// Remonte le projet en tete des recents (ou met a jour son epingle).
				void Touch(const NkProjectState &st);
				void TogglePin(usize i);
				void Remove(usize i);
		};

		/// `~/.nk3dmodeler_recent.cfg` — repli sur le dossier courant si le profil
		/// utilisateur est introuvable.
		NkString NkRecentFilePath();

	} // namespace nk3d
} // namespace nkentseu
