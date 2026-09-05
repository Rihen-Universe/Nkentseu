#pragma once
// -----------------------------------------------------------------------------
// @File    NkFilePickerNav.h
// @Brief   LE SELECTEUR DE FICHIERS A DEUX VOLETS — la VARIANTE « navigateur »
//          du selecteur du kit : rail de gauche (disques, dossiers usuels,
//          favoris, chemin courant), volet de droite en VIGNETTES, fil
//          d'Ariane, recherche, tri, filtre d'extension, champ de nom.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUI A ETE MESURE AVANT D'ECRIRE UNE LIGNE (2026-09-05)
// =============================================================================
//  Rodolf : « Est-ce qu'on peut avoir un vrai sélecteur de fichiers, comme sur
//  Blender ou Windows ? NKCode en a un je crois : regarde-le d'abord, et s'il
//  est dans l'application, remonte-le dans NKEditorKit. L'ancien reste. »
//
//  MESURE 1 — NKCode N'A PAS DE SELECTEUR A LUI. `NKCode/Shell/Dialogs.h`
//  declare `struct NkCodeDialogs : public NkFilePickerState` : il HERITE du
//  selecteur du kit et n'ajoute que des usages (PK_NewFolder, PK_ExportZip,
//  PK_ExampleCopy) et un panneau supplementaire par les crochets virtuels.
//  Il n'y a donc RIEN a remonter : c'etait deja monte. Le dire est plus utile
//  que de deplacer du code pour faire croire au geste demande.
//
//  MESURE 2 — CE QUE L'ANCIEN NE SAIT PAS FAIRE. `NkDrawFilePicker` est une
//  COLONNE UNIQUE : l'arbre des dossiers, et les fichiers ajoutes A LA SUITE
//  dans la meme liste defilante (`totalRows = pickerTree + pickerFiles + 1`).
//  Pas de second volet, pas de vignette, pas de fil d'Ariane, pas de tri.
//  C'est exactement l'ecart que Rodolf decrit.
//
//  MESURE 3 — LE VOLET DROIT EXISTE DEJA, ET IL EST DEJA DANS LE KIT.
//  `NkDrawContentBrowser` (821 l.) porte le fil d'Ariane, la recherche, les
//  puces de filtre, la bascule grille/liste, le tri, la barre d'etat, et un
//  ARBRE de dossiers a gauche (le `tree_view` du kit, pas une copie). Ecrire
//  ici un second navigateur aurait ete la quatrieme copie que ce composant
//  existe justement pour supprimer. CE FICHIER N'EN ECRIT AUCUN : il pose le
//  cadre modal, remplit le modele depuis NKFileSystem, et laisse le composant
//  dessiner. C'est la porte « la couche du dessous d'abord ».
//
//  ⚠️ CE QUE LA REUTILISATION A COUTE, ET QUI EST PAYE DANS LE MEME LOT :
//    - `NkAssetEntry::thumbnail` etait DECLAREE ET LUE NULLE PART — une entree
//      avec vignette ne montrait NI image NI icone. Corrige dans
//      `NkContentBrowserDraw.cpp` (`DrawThumb`), au contrat de `ImagePolygone`.
//    - le fil d'Ariane ne portait que le LIBELLE d'une miette : deux segments
//      homonymes (`src/nkgui/src`) la rendaient indechiffrable. Le resultat
//      porte desormais `navigatedCrumb` (son index).
//    - la bande « Contenu » et les boutons « Creer / Importer / Tout
//      enregistrer » n'ont aucun sens dans un dialogue d'ouverture : deux
//      parametres (`show_header`, `show_actions`) et un champ (`headerTitle`)
//      les rendent silencieux. Tous les trois valent leur ancien comportement
//      par defaut — les consommateurs existants ne bougent pas.
//
//  L'ANCIEN RESTE. `NkDrawFilePicker` n'est pas touche, et ses huit
//  consommateurs non plus : ce fichier n'ajoute qu'un chemin de plus.
//  `NkFilePickerNavState` DERIVE de `NkFilePickerState` — les modes, le
//  confinement, le filtre d'extension, le nom d'enregistrement et surtout LE
//  RESULTAT (`pickerConfirmed` / `pickerResultPath` / `pickerResultName`) sont
//  les memes : une application passe de l'un a l'autre sans toucher son code de
//  confirmation.
//
//  ⚠️ LE CLAVIER. Le champ de nom et le champ de chemin sont des
//     `NkOverlayTextField` de NKGui — c'est-a-dire du VRAI clavier, pas le
//     contournement `searchFocused` du navigateur (dont le modele n'a pas
//     d'entree clavier). C'est aussi la raison pour laquelle ce fichier vit au
//     niveau NKGui et non au niveau `NkComponentPaint`.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkFilePicker.h"
#include "NKFileSystem/NkFileSystem.h" // ② NkFileSystem::GetDrives : les volumes MONTES
#include "NKEditorKit/NkTheme.h"
#include "NKEditorKit/Components/NkContentBrowserModel.h"
#include "NKEditorKit/Components/NkGuiComponentPaint.h"

namespace nkentseu {
	namespace editorkit {

		// ── LE STYLE ────────────────────────────────────────────────────────────
		// Deux moities, et la separation n'est pas cosmetique : le VOLET DROIT est
		// un composant declare, il ne connait que des ROLES de theme ; le CADRE du
		// dialogue n'en est pas un, et reprend les couleurs de l'ancien selecteur
		// pour que les deux se ressemblent a l'ecran.
		struct NkFilePickerNavStyle {
				NkFilePickerStyle cadre;	 ///< la fenetre : fond, liseres, boutons
				NkContentBrowserStyle volet; ///< le navigateur : des roles, resolus par l'hote
		};

		// ── ⑥ UN FILTRE DE FICHIERS (2026-09-05, nuit) ───────────────────────
		// Rodolf : « est-ce que programmatiquement on peut specifier les formats de
		// fichiers a charger, donc uniquement eux et les dossiers seront visibles, et
		// aussi tout pour tout voir ? »
		//
		// L'ancien `pickerFileExt` portait UNE extension, en mode fichier seulement.
		// Ce n'est pas assez : un selecteur d'images doit accepter cinq extensions, et
		// proposer « Tous les fichiers » a cote.
		struct NkFiltreFichiers {
			NkString nom;				   ///< « Images », « Documents », « Tous les fichiers »
			NkVector<NkString> extensions; ///< « .png »... ; UNE seule valant « * » = tout

			bool Tout() const {
				return extensions.Empty()
					   || (extensions.Size() == 1u && extensions[0].Length() == 1u
						   && extensions[0].CStr()[0] == '*');
			}
		};

		// « VIDE OU PLEIN ? », ET CE QUE CA COUTE (2026-09-05, v5)
		// Rodolf : « il faut aussi distinguer dossier vide de dossier plein ».
		//
		// LA RESERVE ETAIT : un acces disque par entree affichee. Elle est payee en
		// quatre fois, et chaque moitie compte :
		//   1. ON NE DEMANDE PAS « COMBIEN », ON DEMANDE « AU MOINS UN »
		//      (`NkDirectory::Probe`, arret au premier element, aucune allocation) ;
		//   2. UNE SEULE FOIS PAR DOSSIER, retenu ICI, avec l'horodatage du dossier :
		//      si l'horodatage change, la reponse est reprise ; sinon elle est rendue
		//      sans toucher au disque. `accesDisque` COMPTE les mesures reelles --
		//      c'est ce compteur que la sonde interroge, et sans lui « c'est en cache »
		//      serait une affirmation, pas une mesure ;
		//   3. SEULEMENT CE QUI SE VOIT : l'hote ne sonde que la plage rendue par
		//      `NkContentBrowserResult::premierVisible/dernierVisible` et les noeuds du
		//      rail qui sont deplies ;
		//   4. LE CACHE MEURT AVEC LE DIALOGUE : rouvrir, c'est relire.
		//
		// ⚠️ DEUX QUESTIONS VOISINES, UN SEUL CORPS. Le chevron du rail demande « a-t-il
		//    des SOUS-DOSSIERS ? » ; l'icone demande « contient-il QUELQUE CHOSE ? ». Un
		//    dossier plein de fichiers sans aucun sous-dossier repond NON au premier et OUI
		//    au second -- les deux reponses sont donc retenues separement, mais elles
		//    sortent de la MEME fonction parametree. Deux fonctions auraient diverge.
		struct NkCacheDossiers {
				struct Ligne {
						nk_uint64 cle = 0; ///< empreinte NORMALISEE du chemin -- voir `Cle`
						NkString chemin;
						nk_int64 horodatage = 0;
						uint8 tout = 0; ///< 0 = pas encore demande
						uint8 sous = 0;
				};
				NkVector<Ligne> lignes;
				uint32 accesDisque = 0; ///< mesures REELLES, pour la sonde

				/// L'EMPREINTE D'UN CHEMIN, pour ne PAS comparer 124 chaines a chaque
				/// interrogation. MESURE qui l'a rendue necessaire : la sonde 123 en Debug --
				/// relire le cache de 124 dossiers coutait 5,2 ms, soit PLUS que les 4,6 ms
				/// des acces disque qu'il evitait. Un cache plus lent que ce qu'il economise
				/// n'est pas un cache.
				/// ⚠️ ELLE NE REMPLACE PAS `PathSame`, elle le FILTRE : deux empreintes
				///    egales font ensuite l'objet d'une vraie comparaison. Au pire, deux
				///    ecritures differentes du meme chemin donnent deux empreintes et donc
				///    une ligne de trop -- un acces disque de plus, jamais une reponse fausse.
				static nk_uint64 Cle(const char *s) {
					nk_uint64 h = 1469598103934665603ull;
					usize n = 0u;
					while (s && s[n])
						++n;
					while (n > 1u && (s[n - 1u] == '/' || s[n - 1u] == '\\'))
						--n; // une barre finale ne change pas le dossier
					for (usize i = 0; i < n; ++i) {
						char c = s[i];
						if (c == '\\')
							c = '/'; // les deux separateurs designent le meme dossier
						if (c >= 'A' && c <= 'Z')
							c = (char)(c - 'A' + 'a');
						h ^= (nk_uint64)(unsigned char)c;
						h *= 1099511628211ull;
					}
					return h;
				}

				void Vider() {
					lignes.Clear();
					accesDisque = 0;
				}

				/// L'etat de `chemin` (`NkContenuDossier`). `horodatage` est celui du dossier,
				/// tel que l'enumeration du parent l'a donne -- il est donc GRATUIT : on ne
				/// paie pas un acces disque pour savoir s'il faut en payer un.
				uint8 Etat(const char *chemin, nk_int64 horodatage, bool sousDossiersSeulement) {
					if (!chemin || !*chemin)
						return (uint8)NkContenuDossier::Inconnu;
					const nk_uint64 cle = Cle(chemin);
					for (uint32 i = 0; i < (uint32)lignes.Size(); ++i) {
						Ligne &l = lignes[i];
						// L'EMPREINTE D'ABORD : elle ecarte 123 lignes sur 124 en une
						// comparaison d'entiers. `PathSame` reste l'autorite sur celle qui reste.
						if (l.cle != cle || !NkFilePickerState::PathSame(l.chemin.CStr(), chemin))
							continue;
						if (l.horodatage != horodatage) { // le dossier a bouge : on reprend
							l.horodatage = horodatage;
							l.tout = 0;
							l.sous = 0;
						}
						uint8 &v = sousDossiersSeulement ? l.sous : l.tout;
						if (v == 0u)
							v = Mesurer(chemin, sousDossiersSeulement);
						return v;
					}
					Ligne l;
					l.cle = cle;
					l.chemin = NkString(chemin);
					l.horodatage = horodatage;
					const uint8 v = Mesurer(chemin, sousDossiersSeulement);
					if (sousDossiersSeulement)
						l.sous = v;
					else
						l.tout = v;
					lignes.PushBack(l);
					return v;
				}

			private:
				uint8 Mesurer(const char *chemin, bool sousDossiersSeulement) {
					++accesDisque;
					switch (NkDirectory::Probe(chemin, sousDossiersSeulement)) {
						case NkDirectory::NkDirProbe::Vide:
							return (uint8)NkContenuDossier::Vide;
						case NkDirectory::NkDirProbe::Plein:
							return (uint8)NkContenuDossier::Plein;
						default:
							// NI VIDE NI PLEIN : un dossier qu'on ne peut pas lire n'est pas
							// vide, et le compter comme tel serait le mensonge le plus facile.
							return (uint8)NkContenuDossier::Illisible;
					}
				}
		};

		// ── L'ETAT ──────────────────────────────────────────────────────────────
		struct NkFilePickerNavState : public NkFilePickerState {
				NkContentBrowserModel vue;	 ///< le volet droit ET son rail de gauche
				/// « vide ou plein ? », retenu par chemin et par horodatage. Vide a l'ouverture.
				NkCacheDossiers cacheDossiers;
				/// La plage d'entrees VISIBLES rendue par le volet a l'image precedente : c'est
				/// elle, et elle seule, que l'on sonde. -1 tant que rien n'a ete dessine.
				int32 premierVu = -1, dernierVu = -1;
				/// Le dialogue etait-il ouvert a l'image precedente ? Sert a vider le cache a
				/// l'ouverture -- un seul site, celui du dessin.
				bool etaitOuvert = false;
								NkVector<NkString> cheminsCrumb; ///< le chemin COMPLET de chaque miette
				NkVector<NkString> favoris;		 ///< poses par l'hote (chemins absolus)

				// ── ⑥ LES FILTRES ───────────────────────────────────────────
				// Vide = tout passe. Le dialogue affiche un combo des `nom` quand il y en a
				// au moins deux.
				// ⚠️ LES DOSSIERS PASSENT TOUJOURS, quel que soit le filtre : un filtre sert a
				//    trouver un FICHIER, pas a s'interdire de naviguer.
				NkVector<NkFiltreFichiers> filtres;
				int32 filtreActif = 0;

				/// Pose un groupe. `exts` : « png;jpg;jpeg » (avec ou sans le point), ou « * ».
				/// ⚠️ « Tous les fichiers » n'est PAS ajoute d'office : c'est l'hote qui decide
				///    s'il veut l'offrir. Mais `NkFiltresUsuels` le met, et c'est ce que les
				///    quatre modes utilisent par defaut -- l'utilisateur doit toujours pouvoir
				///    voir ce qu'il y a.
				void AjouterFiltre(const char *nom, const char *exts) {
					NkFiltreFichiers f;
					f.nom = NkString(nom ? nom : "");
					NkString cour;
					for (const char *p = exts ? exts : "";; ++p) {
						if (*p == ';' || *p == ',' || *p == '\0') {
							if (!cour.Empty()) {
								if (cour.CStr()[0] != '.' && cour.CStr()[0] != '*') {
									NkString avecPoint(".");
									avecPoint.Append(cour.CStr());
									cour = avecPoint;
								}
								f.extensions.PushBack(cour);
								cour = NkString();
							}
							if (*p == '\0')
								break;
						} else
							cour.Append(*p);
					}
					filtres.PushBack(f);
					relire = true;
				}

				/// Ce nom de FICHIER passe-t-il le filtre actif ?
				/// ⚠️ Compatibilite : sans filtre pose, on retombe sur `pickerFileExt`, que les
				///    appelants existants remplissent encore (et que `OuvrirNav` remplit).
				bool PasseLeFiltre(const char *nom) const {
					if (!nom || !*nom)
						return false;
					if (filtres.Empty())
						return pickerFileExt.Empty() || EndsWithI(nom, pickerFileExt.CStr());
					const uint32 i = (filtreActif >= 0 && filtreActif < (int32)filtres.Size())
							  ? (uint32)filtreActif
							  : 0u;
					const NkFiltreFichiers &f = filtres[i];
					if (f.Tout())
						return true;
					for (uint32 k = 0; k < (uint32)f.extensions.Size(); ++k)
						if (EndsWithI(nom, f.extensions[k].CStr()))
							return true;
					return false;
				}

				/// Le libelle du filtre actif, pour le combo.
				const char *NomFiltreActif() const {
					if (filtres.Empty())
						return "Tous les fichiers";
					const uint32 i = (filtreActif >= 0 && filtreActif < (int32)filtres.Size())
							  ? (uint32)filtreActif
							  : 0u;
					return filtres[i].nom.CStr();
				}

				// ── ③ LES DOSSIERS RECEMMENT OUVERTS (05/09, soir) ─────────────────
				// Rodolf : « on doit aussi voir les dossiers recemment ouverts dans cette
				// session ou ce document. » DEUX listes, et la distinction est portee par
				// L'HOTE, pas par le kit : le kit ne sait pas ce qu'est un document.
				//   - la SESSION : memoire vive, oubliee au lancement suivant ;
				//   - le DOCUMENT : range dans le fichier, il voyage avec lui.
				// Ici, une seule liste ORDONNEE (le plus recent d'abord) que l'hote remplit
				// dans l'ordre qu'il veut -- deux vecteurs dans le kit auraient impose sa
				// semantique a NK3DModeler et NKCode, qui n'ont pas la meme notion.
				NkVector<NkString> recents;

				/// ⑥ (05/09, nuit) LE DOSSIER COURANT -- LA TETE DES RECENTS, et rien d'autre.
				/// Rodolf : « un dossier est appele dossier courant si on a reussi a sauvegarder
				/// ou a charger un fichier de ce dossier-la. »
				/// ⚠️ CE N'EST PAS LE DOSSIER OU L'ON NAVIGUE. Une visite, un apercu, un refus,
				///    un « Annuler » ne le changent pas : seule une ecriture ou une lecture
				///    REUSSIE le fait, et c'est deja la regle de `RetenirDossierRecent`.
				/// ⚠️ UNE SEULE SOURCE. Tenir un `dossierCourant` a cote de la liste des recents
				///    aurait fait deux etats qui disent deux choses -- exactement le defaut ① du
				///    matin, ou `dossier` doublait `pickerPath`.
				/// Rend un chemin VIDE tant qu'aucune operation n'a reussi.
				const char *DossierCourant() const {
					return recents.Empty() ? "" : recents[0].CStr();
				}
				
				/// Pose un dossier en tete des recents, sans doublon, borne a `kMaxRecents`.
				/// ⚠️ LE PLUS RECENT EN TETE, et la deduplication est un DEPLACEMENT, pas un
				///    rejet : reouvrir un dossier deja liste doit le faire remonter, sinon la
				///    liste vieillit exactement a l'envers de son nom.
				static void PoserRecent(NkVector<NkString> &liste, const char *chemin) {
					if (!chemin || !*chemin)
						return;
					for (uint32 i = 0; i < (uint32)liste.Size(); ++i)
						if (PathSame(liste[i].CStr(), chemin)) {
							liste.RemoveAt(i);
							break;
						}
					NkVector<NkString> tmp;
					tmp.PushBack(NkString(chemin));
					for (uint32 i = 0; i < (uint32)liste.Size() && i + 1 < kMaxRecents; ++i)
						tmp.PushBack(liste[i]);
					liste = tmp;
				}
				static const uint32 kMaxRecents = 10u;
				// ── ① (05/09, soir) LE CHEMIN N'A QU'UN SEUL ENDROIT ──────────────────
				// ⚠️ DEFAUT MESURE SUR LA CAPTURE DE RODOLF : le selecteur s'ouvrait sur
				//    « 0 element(s) », rail vide, fil d'Ariane absent -- et se remplissait au
				//    PREMIER GESTE. Cause : ce fichier portait un champ `dossier` EN PLUS de
				//    `pickerPath`, et la lecture etait ARMEE par `OuvrirNav`. Or le dialogue
				//    d'export appelle `OpenPickerBase` -- la porte de la classe de base, celle
				//    des huit consommateurs existants : le chemin etait pose, l'armement non.
				//    Meme famille que « la valeur dessinee et la valeur montree viennent de
				//    deux endroits ».
				//
				//    LE REMEDE N'EST PAS D'ARMER AUSSI LE SECOND APPELANT -- c'est de
				//    SUPPRIMER L'ARMEMENT. `pickerPath` est la seule verite ; `listePour`
				//    retient le chemin pour lequel la liste courante a ete batie, et le dessin
				//    relit des que les deux different. N'importe qui peut ecrire `pickerPath`
				//    (le bouton « Aller », `OpenPickerBase`, un appelant qui n'existe pas
				//    encore) : il n'y a plus rien a oublier.
				char listePour[512] = {};
				/// ④ (05/09, nuit) L'EMPREINTE DE CE QUI EST DEPLIE. Le rail SE RECONSTRUIT
				/// quand elle change -- personne n'a a « armer » une reconstruction apres un
				/// clic sur un chevron. Meme regle qu'au point ① du 05/09 : on compare, on
				/// n'arme pas.
				nk_uint64 empreinteDeplie = 0u;

				// ── ② LA LARGEUR DU RAIL, EN PIXELS (05/09, nuit) ───────────────────
				// Elle etait une FRACTION de la largeur du volet (`tree_width` = 0,18) : 155 px
				// ici, et moins encore sur une fenetre plus petite. Un rail dont la largeur
				// depend de la fenetre tronque d'autant plus qu'on a peu de place -- exactement
				// quand on en a le plus besoin.
				// ⚠️ LE MINIMUM EST REEL : en dessous de `kRailMin`, on ne montre pas un rail
				//    etroit, on refuse de le retrecir. Un rail illisible ne rend aucun service.
				float32 largeurRail = 220.f;
				bool railGlisse = false;
				static const float32 kRailMin;
				static const float32 kRailMax;
				bool relire = true; ///< forcer une relecture (creation de dossier, F5...)

				/// LE dossier affiche. Ce n'est PAS un champ : c'est `pickerPath` lui-meme,
				/// pour qu'il soit IMPOSSIBLE que les deux divergent.
				const char *Dossier() const {
					return pickerPath;
				}
				bool DoitRelire() const {
					return relire || !PathSame(listePour, pickerPath);
				}

				/// LA VIGNETTE. Le kit ne sait pas charger une image ni fabriquer une
				/// texture : c'est l'hote qui le sait (NkUIDesign a son cache d'images).
				/// Rend 0 quand il n'y a pas de vignette — l'icone de nature prend alors
				/// le relais, et c'est le comportement historique.
				nk_uint64 (*vignette)(void *user, const char *chemin) = nullptr;
				void *vignetteUser = nullptr;

				/// Le role de theme des dossiers / des fichiers, pose par l'hote (le kit
				/// ne connait l'enumeration d'aucune application — meme regle que
				/// `NkAssetEntry::kindRole`).
				uint16 roleDossier = 0, roleFichier = 0;

				/// ⑤ (05/09, nuit) UNE TEINTE PAR FAMILLE, posee par l'hote (image, texte,
				/// code, archive, executable...). Indexee par `NkAssetIcone`. A zero, on retombe
				/// sur `roleFichier` : un hote qui n'a rien pose garde le rendu d'avant.
				uint16 rolesFamille[(uint32)NkAssetIcone::Count] = {};

				/// Le role d'une famille, avec son repli.
				uint16 RoleDeFamille(NkAssetIcone g) const {
					const uint32 i = (uint32)g;
					if (i < (uint32)NkAssetIcone::Count && rolesFamille[i] != 0u)
						return rolesFamille[i];
					return roleFichier;
				}

				/// ⑤ CE FICHIER EST-IL D'UN FORMAT RECONNU ? La reponse vient de LA TABLE DES
				/// FILTRES -- une extension listee dans un groupe NOMME est reconnue.
				/// ⚠️ UNE SEULE SOURCE, PAS UNE SECONDE TABLE D'EXTENSIONS. Deux tables auraient
				///    diverge, et le badge aurait fini par annoncer une famille que le filtre
				///    ne laisse pas passer.
				/// ⚠️ LE GROUPE « TOUT » NE RECONNAIT RIEN : il laisse passer, il ne qualifie
				///    pas. Sans cette regle, tout serait reconnu des qu'on offre « Tous les
				///    fichiers » -- c'est-a-dire toujours.
				bool EstReconnu(const char *nom) const {
					if (!nom || !*nom)
						return false;
					if (filtres.Empty())
						return !pickerFileExt.Empty() && EndsWithI(nom, pickerFileExt.CStr());
					for (uint32 i = 0; i < (uint32)filtres.Size(); ++i) {
						if (filtres[i].Tout())
							continue;
						for (uint32 k = 0; k < (uint32)filtres[i].extensions.Size(); ++k)
							if (EndsWithI(nom, filtres[i].extensions[k].CStr()))
								return true;
					}
					return false;
				}

				// ── L'OUVERTURE ─────────────────────────────────────────────────
				/// `purpose` : les memes PK_* que l'ancien selecteur. `ext` = filtre
				/// d'extension (« .png »), vide = tout. `nomPropose` = le nom pre-rempli
				/// en mode enregistrer.
				void OuvrirNav(int32 purpose, const char *depart, const char *ext, const char *nomPropose,
							   char *buf, int32 cap) {
					OpenPickerBase(purpose, depart, buf, cap, nullptr, nullptr);
					// ⑥ `ext` reste accepte -- une extension unique est le cas simple, et les
					//    appelants existants l'utilisent. Les FILTRES nommes se posent a cote,
					//    par `AjouterFiltre`, et prennent le pas quand il y en a.
					pickerFileExt = NkString(ext ? ext : "");
					if (nomPropose)
						CopyTo(pickerSaveName, nomPropose, (int32)sizeof(pickerSaveName));
					vue.viewMode = 0; // la GRILLE par defaut : c'est ce que Rodolf a demande
					// ⚠️ AUCUN ARMEMENT ICI. Cette fonction est un CONFORT (l'extension et le nom
					//    propose), pas un passage oblige : `OpenPickerBase` seul suffit a remplir
					//    la vue, et la sonde le mesure PAR CETTE PORTE-LA.
				}

				/// Change de dossier. Ne descend PAS dans un chemin interdit par le
				/// confinement : la garde est celle de l'ancien selecteur, mot pour mot.
				void AllerA(const char *chemin) {
					if (!chemin || !*chemin || !NkDirectory::Exists(chemin))
						return;
					if (!PickerAllowed(chemin))
						return;
					// UN SEUL ECRIT : le dessin verra que `listePour` ne correspond plus.
					CopyTo(pickerPath, chemin, (int32)sizeof(pickerPath));
				}

				// ── ⑥ CREER UN DOSSIER (05/09, soir) ────────────────────────────
				// Rodolf : « que ce soit pour creer un dossier, selectionner un dossier ou un
				// fichier, pour ouvrir ou pour sauvegarder » -- quatre modes, un seul outil.
				// Le troisieme manquait : on pouvait choisir un dossier, pas en faire un.
				char nouveauNom[128] = {};
				bool nouveauFocus = false;
				/// ⑤ (05/09, nuit) LA CREATION EST REPLIEE PAR DEFAUT. Elle occupait une rangee
				/// PLEINE LARGEUR en permanence, qui poussait le nom du fichier vers le bas --
				/// alors qu'on cree un dossier une fois sur vingt. Un bouton discret l'ouvre.
				bool creationOuverte = false;
				/// ⑥⑤ Quel menu deroulant est ouvert : 0 aucun, 1 les filtres, 2 le tri.
				/// UN SEUL a la fois -- deux listes ouvertes se recouvriraient.
				int32 menuOuvert = 0;
				NkString messageCreation; ///< ce qui s'est passe, dit SOUS le champ

				/// Cree `nouveauNom` DANS le dossier courant, y descend, et vide le champ.
				/// ⚠️ ON N'ECRASE PAS UN DOSSIER EXISTANT EN SILENCE : s'il est deja la, on y
				///    descend et on le DIT. C'est la meme regle que le ` (2)` de l'export --
				///    un geste de creation qui tombe sur un homonyme doit se voir.
				bool CreerDossier() {
					messageCreation = NkString();
					if (!nouveauNom[0]) {
						messageCreation = NkString("Donnez un nom au dossier.");
						return false;
					}
					// les caracteres que le systeme refuse : on les dit, on ne les remplace pas
					// en douce (l'utilisateur doit reconnaitre le nom qu'il a tape).
					for (const char *p = nouveauNom; *p; ++p)
						if (*p == '/' || *p == '\\' || *p == ':' || *p == '*' || *p == '?' || *p == '"'
							|| *p == '<' || *p == '>' || *p == '|') {
							messageCreation = NkString("Un nom de dossier ne peut pas contenir \\ / : * ? \" < > |");
							return false;
						}
					const NkString cible = (NkPath(pickerPath) / nouveauNom).ToString();
					if (NkDirectory::Exists(cible.CStr())) {
						messageCreation = NkString("Ce dossier existe d\u00e9j\u00e0 \u2014 on y entre.");
						AllerA(cible.CStr());
						nouveauNom[0] = '\0';
						return true;
					}
					if (!NkDirectory::CreateRecursive(cible.CStr())) {
						messageCreation = NkString("Cr\u00e9ation refus\u00e9e par le syst\u00e8me (droits ? disque plein ?).");
						return false;
					}
					AllerA(cible.CStr());
					nouveauNom[0] = '\0';
					return true;
				}

				// ── LE CONTENU DU DOSSIER ───────────────────────────────────────
				static bool AvantParNom(const NkString &a, const NkString &b) {
					const char *x = a.CStr(), *y = b.CStr();
					for (; *x && *y; ++x, ++y) {
						char p = *x, q = *y;
						if (p >= 'A' && p <= 'Z')
							p = (char)(p + 32);
						if (q >= 'A' && q <= 'Z')
							q = (char)(q + 32);
						if (p != q)
							return p < q;
					}
					return *y != '\0';
				}

				/// L'extension EN MAJUSCULES, sans le point (« PNG ») — le libelle de
				/// nature du navigateur. Un dossier n'en a pas : il EST sa couleur.
				static void ExtDe(const char *nom, char *out, usize cap) {
					out[0] = '\0';
					const char *pt = nullptr;
					for (const char *p = nom; p && *p; ++p)
						if (*p == '.')
							pt = p;
					if (!pt || !pt[1])
						return;
					usize k = 0;
					for (const char *p = pt + 1; *p && k + 1 < cap; ++p, ++k)
						out[k] = (*p >= 'a' && *p <= 'z') ? (char)(*p - 32) : *p;
					out[k] = '\0';
				}

				// ── ③ LA SILHOUETTE D'UNE ENTREE (05/09, nuit) ──────────────────────
				// ⚠️ LES DOSSIERS CONNUS SE RECONNAISSENT A LEUR CHEMIN, pas a leur nom : sur
				//    un Windows francais « Images » s'appelle « Images » mais sur un anglais
				//    « Pictures », et un dossier redirige vers OneDrive n'est ni l'un ni l'autre.
				//    On compare au chemin que le SYSTEME rend (`GetUserFolder`).
				static NkAssetIcone IconeDossier(const char *chemin) {
					struct Paire {
						NkDirectory::NkUserFolder f;
						NkAssetIcone i;
					};
					static const Paire kP[] = {
						{NkDirectory::NkUserFolder::Pictures, NkAssetIcone::DossierImages},
						{NkDirectory::NkUserFolder::Documents, NkAssetIcone::DossierDocuments},
						{NkDirectory::NkUserFolder::Downloads, NkAssetIcone::DossierTelechargements},
						{NkDirectory::NkUserFolder::Desktop, NkAssetIcone::DossierBureau}};
					for (usize k = 0; k < sizeof(kP) / sizeof(kP[0]); ++k) {
						const NkString p = NkDirectory::GetUserFolder(kP[k].f).ToString();
						if (!p.Empty() && PathSame(p.CStr(), chemin))
							return kP[k].i;
					}
					return NkAssetIcone::Dossier;
				}

				/// ② (05/09, nuit) L'ICONE D'UN CHEMIN -- LA SEULE PORTE, appelee par LES DEUX
				/// volets : la grille de droite et le rail de gauche. Rodolf : « le panneau de
				/// gauche ne montre pas les icones associees aux differents dossiers ».
				/// ⚠️ DEUX TABLES AURAIENT DIVERGE des le premier ajout de nature -- et c'est
				///    exactement ce qui venait d'arriver : la grille avait onze formes, le rail
				///    n'avait rien.
				static NkAssetIcone IconePour(const char *chemin, bool estDossier) {
					if (!chemin || !*chemin)
						return NkAssetIcone::Inconnu;
					if (!estDossier)
						return IconeFichier(chemin);
					// UN VOLUME n'est pas un dossier : `D:/`, `C:/`, `/`. On le reconnait a sa
					// forme (deux caracteres et un separateur, ou la racine seule).
					const NkString nom = NomDeDossier(chemin);
					if (nom.Length() <= 2u && (nom.Empty() || nom.CStr()[nom.Length() - 1u] == ':'))
						return NkAssetIcone::Volume;
					return IconeDossier(chemin);
				}
				
				/// La silhouette d'un FICHIER, d'apres son extension. Une table fermee : ce
				/// sont les natures que tout systeme de fichiers connait.
				static NkAssetIcone IconeFichier(const char *nom) {
					struct Ext {
						const char *e;
						NkAssetIcone i;
					};
					static const Ext kE[] = {
						{".png", NkAssetIcone::Image},	 {".jpg", NkAssetIcone::Image},
						{".jpeg", NkAssetIcone::Image},	 {".bmp", NkAssetIcone::Image},
						{".gif", NkAssetIcone::Image},	 {".tga", NkAssetIcone::Image},
						{".webp", NkAssetIcone::Image},	 {".svg", NkAssetIcone::Image},
						{".psd", NkAssetIcone::Image},	 {".hdr", NkAssetIcone::Image},
						{".txt", NkAssetIcone::Texte},	 {".md", NkAssetIcone::Texte},
						{".log", NkAssetIcone::Texte},	 {".csv", NkAssetIcone::Texte},
						{".json", NkAssetIcone::Code},	 {".xml", NkAssetIcone::Code},
						{".h", NkAssetIcone::Code},		 {".hpp", NkAssetIcone::Code},
						{".c", NkAssetIcone::Code},		 {".cpp", NkAssetIcone::Code},
						{".py", NkAssetIcone::Code},	 {".js", NkAssetIcone::Code},
						{".jenga", NkAssetIcone::Code},	 {".nkgui", NkAssetIcone::Code},
						{".nkuidoc", NkAssetIcone::Code}, {".zip", NkAssetIcone::Archive},
						{".7z", NkAssetIcone::Archive},	 {".tar", NkAssetIcone::Archive},
						{".gz", NkAssetIcone::Archive},	 {".rar", NkAssetIcone::Archive},
						{".exe", NkAssetIcone::Executable}, {".dll", NkAssetIcone::Executable},
						{".bat", NkAssetIcone::Executable}, {".sh", NkAssetIcone::Executable}};
					for (usize k = 0; k < sizeof(kE) / sizeof(kE[0]); ++k)
						if (EndsWithI(nom, kE[k].e))
							return kE[k].i;
					return NkAssetIcone::Inconnu;
				}

				/// ⑤ La comparaison par CLE de tri, avec le sens. Le nom departage toujours :
				/// sans ca, deux fichiers de meme taille changeraient d'ordre a chaque lecture.
				bool AvantApres(const NkString &na, nk_int64 ta, nk_int64 da, const NkString &nb,
								nk_int64 tb, nk_int64 db) const {
					bool avant;
					switch ((NkBrowserTri)vue.sortCle) {
						case NkBrowserTri::Taille:
							if (ta != tb) { avant = ta > tb; break; }
							avant = AvantParNom(na, nb);
							break;
						case NkBrowserTri::Date:
							if (da != db) { avant = da > db; break; }
							avant = AvantParNom(na, nb);
							break;
						case NkBrowserTri::Type: {
							char ea[16], eb[16];
							ExtDe(na.CStr(), ea, sizeof(ea));
							ExtDe(nb.CStr(), eb, sizeof(eb));
							const int32 c = NkComponentDecl::StrEq(ea, eb) ? 0 : 1;
							if (c != 0) { avant = AvantParNom(NkString(ea), NkString(eb)); break; }
							avant = AvantParNom(na, nb);
							break;
						}
						default:
							avant = AvantParNom(na, nb);
							break;
					}
					return vue.sortAsc ? avant : !avant;
				}
				
				void RelireDossier() {
					relire = false;
					// ⚠️ RETENU AVANT DE POUVOIR ECHOUER : un chemin illisible ne doit pas faire
					//    relire a CHAQUE image (un listage en boucle sur un lecteur absent gele la
					//    fenetre, et le defaut parait alors venir du dessin).
					CopyTo(listePour, pickerPath, (int32)sizeof(listePour));
					vue.entries.Clear();
					vue.ClearSelection();
					vue.scroll = 0.f;
					if (!pickerPath[0] || !NkDirectory::Exists(pickerPath))
						return;
					NkVector<NkDirectoryEntry> e = NkDirectory::GetEntries(
						NkPath(pickerPath), "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
					NkVector<NkString> dirs, files;
					// ⑤ LA TAILLE ET LA DATE viennent du systeme (`NkDirectoryEntry`), et on les
					//    retient PAR NOM : la liste est ensuite triee, donc l'index change.
					NkVector<nk_int64> tailles, dates;
					for (usize i = 0; i < e.Size(); ++i) {
						const char *nm = e[i].Name.CStr();
						if (!nm || !nm[0] || nm[0] == '.' || e[i].IsHidden)
							continue;
						if (e[i].IsDirectory)
							dirs.PushBack(e[i].Name);
						// ⑥ LE FILTRE NE PORTE QUE SUR LES FICHIERS -- les dossiers sont passes
						//    au-dessus, sans condition : un filtre sert a trouver un fichier, pas a
						//    s'interdire de naviguer.
						// ⚠️ EN MODE DOSSIER, ON LISTE TOUT : voir ce qu'il y a aide a choisir ou
						//    l'on va. Ces fichiers sont peints en teinte ATTENUEE (`roleFichier` que
						//    l'hote pose plus sourd) et la confirmation continue de les refuser.
						else if (pickerFor == PK_PickFolder || pickerFor == PK_Open || PasseLeFiltre(nm)) {
							files.PushBack(e[i].Name);
							tailles.PushBack(e[i].Size);
							dates.PushBack(e[i].ModificationTime);
						}
					}
					// Tri par insertion : les listes d'un dossier sont courtes, et une
					// dependance de tri de plus ne se justifierait pas ici.
					// ⚠️ LE TRI DEPLACE TROIS TABLEAUX EN MEME TEMPS (nom, taille, date) : les
					//    trier separement les desynchroniserait, et un fichier porterait la taille
					//    d'un autre. C'est le genre de defaut qu'on ne voit qu'a l'usage.
					auto trierFichiers = [&]() {
						for (uint32 i = 1; i < (uint32)files.Size(); ++i) {
							NkString nomI = files[i];
							const nk_int64 tI = i < (uint32)tailles.Size() ? tailles[i] : 0;
							const nk_int64 dI = i < (uint32)dates.Size() ? dates[i] : 0;
							int32 j = (int32)i - 1;
							while (j >= 0 && AvantApres(nomI, tI, dI, files[(uint32)j],
														   tailles[(uint32)j], dates[(uint32)j])) {
								files[(uint32)j + 1] = files[(uint32)j];
								tailles[(uint32)j + 1] = tailles[(uint32)j];
								dates[(uint32)j + 1] = dates[(uint32)j];
								--j;
							}
							files[(uint32)j + 1] = nomI;
							tailles[(uint32)j + 1] = tI;
							dates[(uint32)j + 1] = dI;
						}
					};
					auto trier = [&](NkVector<NkString> &v) {
						for (uint32 i = 1; i < (uint32)v.Size(); ++i) {
							NkString cle = v[i];
							int32 j = (int32)i - 1;
							while (j >= 0 && (vue.sortAsc ? AvantParNom(cle, v[(uint32)j])
															  : AvantParNom(v[(uint32)j], cle))) {
								v[(uint32)j + 1] = v[(uint32)j];
								--j;
							}
							v[(uint32)j + 1] = cle;
						}
					};
					trier(dirs);
					trierFichiers();
					// ⚠️ LES DOSSIERS D'ABORD, TOUJOURS — et meme en tri decroissant.
					//    C'est la regle du navigateur historique, et la seule qui rende
					//    un dossier atteignable sans defiler toute une liste d'images.
					for (uint32 i = 0; i < (uint32)dirs.Size(); ++i) {
						NkAssetEntry a;
						a.name = dirs[i];
						a.path = (NkPath(pickerPath) / dirs[i].CStr()).ToString();
						a.isFolder = true;
						a.kindRole = roleDossier;
						a.icone = (uint8)IconePour(a.path.CStr(), true);
						a.kindLabel = "";
						vue.entries.PushBack(a);
					}
					mExts.Clear();
					for (uint32 i = 0; i < (uint32)files.Size(); ++i) {
						NkAssetEntry a;
						a.name = files[i];
						a.path = (NkPath(pickerPath) / files[i].CStr()).ToString();
						a.isFolder = false;
						a.kindRole = roleFichier;
						// ⑤ RECONNU OU NON : deux designs, et la reponse vient des FILTRES.
						const bool reconnu = EstReconnu(files[i].CStr());
						const NkAssetIcone fam = reconnu ? IconePour(files[i].CStr(), false)
													 : NkAssetIcone::Inconnu;
						a.icone = (uint8)fam;
						a.kindRole = reconnu ? RoleDeFamille(fam) : roleFichier;
						// ⑤ LA TAILLE ET LA DATE, POSEES SUR L'ENTREE. Elles etaient lues du systeme
						//    et rangees a cote, mais jamais recopiees ici : le tri par taille tombait
						//    en repli sur le nom et PASSAIT PAR HASARD. La sonde l'a vu parce qu'elle
						//    imprime les octets au lieu de croire l'ordre.
						a.taille = i < (uint32)tailles.Size() ? tailles[i] : 0;
						a.dateModif = i < (uint32)dates.Size() ? dates[i] : 0;
						char ext[16];
						ExtDe(files[i].CStr(), ext, sizeof(ext));
						// ⑤ LE BADGE N'EXISTE QUE POUR UN FORMAT RECONNU. Un inconnu garde son
						//    extension EN GRIS s'il en a une, et rien s'il n'en a pas -- la pastille
						//    coloree annonce une famille, elle ne doit pas annoncer l'ignorance.
						mExts.PushBack(NkString(ext));
						if (vignette)
							a.thumbnail = vignette(vignetteUser, a.path.CStr());
						vue.entries.PushBack(a);
					}
					// `kindLabel` est un `const char*` : il doit pointer sur une chaine
					// qui SURVIT a la fonction. `mExts` est ce proprietaire, et il est
					// rempli AVANT d'etre adresse — un `PushBack` peut realloger.
					for (uint32 i = 0; i < (uint32)mExts.Size(); ++i)
						vue.entries[(uint32)dirs.Size() + i].kindLabel = mExts[i].CStr();
					ConstruireFil();
					ConstruireRail();
				}

				/// LE FIL D'ARIANE et le chemin complet de chaque miette. Les deux
				/// listes ont la MEME longueur, et c'est ce qui rend `navigatedCrumb`
				/// exploitable.
				void ConstruireFil() {
					vue.breadcrumb.Clear();
					cheminsCrumb.Clear();
					const char *p = pickerPath;
					NkString cour;
					NkString seg;
					for (const char *q = p;; ++q) {
						if (*q == '/' || *q == '\\' || *q == '\0') {
							if (!seg.Empty()) {
								if (cour.Empty())
									cour = seg;
								else {
									cour.Append('/');
									cour.Append(seg.CStr());
								}
								vue.breadcrumb.PushBack(seg);
								cheminsCrumb.PushBack(cour);
								seg = NkString();
							}
							if (*q == '\0')
								break;
						} else
							seg.Append(*q);
					}
				}

				/// ② TRONQUER AU MILIEU : `Nkentseu-…-noge` plutot que `Nken…`.
				/// ⚠️ POURQUOI PAS A DROITE : deux dossiers freres partagent presque toujours
				///    leur debut (`Nkentseu`, `Nkentseu-noge`, `Nkentseu-actifs`) et se
				///    distinguent par leur FIN. Couper a droite efface justement ce qui les
				///    separe -- c'est ce que montrait la capture : `Nken…` et `Nk…`.
				/// ⚠️ LES FRONTIERES UTF-8 SONT TENUES DES DEUX COTES (on recule sur les octets
				///    de suite `10xxxxxx`) : la lecon de ⑥, payee le meme soir.
				static void TronquerMilieu(const char *nom, char *out, usize cap, const NkGuiFont &f,
										   float32 dispo) {
					if (!out || cap == 0)
						return;
					out[0] = '\0';
					if (!nom || !*nom)
						return;
					usize n = 0;
					while (nom[n] && n + 1 < cap)
						++n;
					for (usize k = 0; k < n; ++k)
						out[k] = nom[k];
					out[n] = '\0';
					if (!f.Valid() || dispo <= 0.f || f.MeasureWidth(out) <= dispo)
						return;
					static const char kSuite[] = "\xE2\x80\xA6"; // … U+2026, en octets explicites
					usize g = n / 2u, d = n / 2u; // g = fin du debut garde, d = debut de la fin gardee
					while (g > 0u && ((unsigned char)nom[g] & 0xC0u) == 0x80u)
						--g;
					while (d < n && ((unsigned char)nom[d] & 0xC0u) == 0x80u)
						++d;
					while (g > 0u || d < n) {
						usize k = 0;
						for (usize i = 0; i < g && k + 5 < cap; ++i)
							out[k++] = nom[i];
						for (usize i = 0; i < 3u && k + 2 < cap; ++i)
							out[k++] = kSuite[i];
						for (usize i = d; i < n && k + 1 < cap; ++i)
							out[k++] = nom[i];
						out[k] = '\0';
						if (f.MeasureWidth(out) <= dispo)
							return;
						// on rogne alternativement des deux cotes, en sautant les octets de suite
						if (g > 0u) {
							--g;
							while (g > 0u && ((unsigned char)nom[g] & 0xC0u) == 0x80u)
								--g;
						}
						if (d < n) {
							++d;
							while (d < n && ((unsigned char)nom[d] & 0xC0u) == 0x80u)
								++d;
						}
					}
					snprintf(out, cap, "%s", kSuite);
				}

				/// ⑥ LE NOM D'UN DOSSIER, sans jamais rendre son chemin.
				/// ⚠️ `NkPath::GetFileName()` rend VIDE quand le chemin se termine par un
				///    separateur (`D:/Projets/Nkentseu-noge/`), et le repli prenait alors le
				///    CHEMIN ENTIER : c'est ce que Rodolf a vu, `D:/Projets/…entseu-no…` a la
				///    place d'un nom. Un repli qui remplace un nom par un chemin n'est pas un
				///    repli. On enleve les separateurs de fin AVANT de decouper.
				static NkString NomDeDossier(const char *chemin) {
					if (!chemin || !*chemin)
						return NkString();
					NkString c(chemin);
					while (c.Length() > 1u) {
						const char d = c.CStr()[c.Length() - 1];
						if (d != '/' && d != '\\')
							break;
						c = NkString(c.CStr(), c.Length() - 1);
					}
					const NkString nom = NkPath(c).GetFileName();
					if (!nom.Empty())
						return nom;
					// une RACINE (`D:/`, `/`) : son nom EST sa lettre, pas son chemin
					return c;
				}
				
				static nk_uint64 IdDe(const char *s) { // FNV-1a : l'identite d'un noeud du rail
					nk_uint64 h = 1469598103934665603ull;
					for (const char *p = s; p && *p; ++p) {
						h ^= (nk_uint64)(unsigned char)*p;
						h *= 1099511628211ull;
					}
					return h ? h : 1ull;
				}

				/// LE RAIL DE GAUCHE : les disques, les dossiers usuels, les favoris,
				/// puis LA CHAINE du dossier courant (ses ancetres) et ses sous-dossiers.
				/// ⚠️ Il est RECONSTRUIT a chaque navigation plutot que deplie a la
				///    demande : un rail qui ne montre pas ou l'on est ne sert a rien, et
				///    une expansion paresseuse aurait demande un cache d'etat de plus
				///    pour le meme resultat visible.
				// ── ② LE RAIL A LA FORME DE L'EXPLORATEUR (05/09, soir) ──────────────────
				// Rodolf, sur sa capture : « le rail ne montre ni les disques ni les dossiers
				// principaux de l'explorateur » -- ils y etaient, mais MELANGES a l'arborescence
				// du dossier courant, sans titre ni separation. Une liste plate de quatorze
				// entrees heterogenes n'est pas un rail : c'est un tas.
				//
				// TROIS SECTIONS TITREES, dans l'ordre d'utilite decroissante. Ce sont des
				// noeuds `locked` du `tree_view` : le composant les rend inselectionnables sans
				// une ligne de code dediee (`NkTreeViewDraw.cpp` : `if (mousePressed && !locked)`),
				// et leur chevron les replie comme n'importe quel noeud. AUCUNE notion de
				// « section » n'est ajoutee au kit : la donnee suffit.
				//
				// ⚠️ LES DOSSIERS USUELS SE LISENT DU SYSTEME (`NkDirectory::GetUserFolder`,
				//    ajoute a NKFileSystem dans le meme lot). Ecrire `home + "/Desktop"` est faux
				//    sur un Windows francais (« Bureau »), faux sur un dossier redirige vers
				//    OneDrive, et faux sur un Linux qui suit XDG. Le LIBELLE affiche est le nom
				//    reel du dossier, pas une traduction de notre cru.
				//
				// ⚠️ LES VOLUMES SE LISENT A L'EXECUTION (`NkFileSystem::GetDrives`), avec leur
				//    etiquette (« D: Projets »). Boucler de A a Z en testant `Exists` -- ce que
				//    faisait la version precedente -- ne donne ni l'etiquette ni l'etat monte.
				/// ④ (05/09, nuit) AJOUTE UN NOEUD AU RAIL. C'etait une lambda locale a
				/// `ConstruireRail` ; `PoserSousDossiers` en a besoin aussi, et une lambda ne se
				/// partage pas. Un seul site pose une entree du rail -- son icone, son infobulle
				/// et son chemin viennent donc toujours du meme endroit.
				/// @param horodatage Date de modification du dossier, quand l'appelant la tient
				///        deja (l'enumeration du parent la donne). Sert de cle de fraicheur au
				///        cache ; 0 quand on ne la connait pas.
				int32 AjouterNoeud(const char *chemin, const char *libelle, int32 parent,
								   nk_int64 horodatage = 0) {
					if (!chemin || !*chemin || !NkDirectory::Exists(chemin))
						return -1;
					NkTreeNode n;
					n.id = IdDe(chemin);
					n.parent = parent;
					n.label = NkString(libelle && *libelle ? libelle : chemin);
					n.path = NkString(chemin);
					n.kindRole = roleDossier;
					// L'INFOBULLE PORTE LE CHEMIN COMPLET : le libelle est tronque au milieu,
					// et c'est le seul moyen de lire ce qu'on survole.
					n.infobulle = NkString(chemin);
					// LA MEME ICONE QUE LA GRILLE, par la MEME fonction.
					n.silhouette = (uint8)IconePour(chemin, true);
					// ③ (05/09, v5) LE CHEVRON POUR TOUT LE MONDE, ET IL EST POSE ICI.
					// Rodolf : « les chevrons ne sont pas presents sur la plupart des entrees ».
					// C'etait exact, et la cause etait structurelle : `enfantsPossibles` n'etait
					// renseigne QUE par `PoserSousDossiers`, qui ne tourne que sous le dossier
					// courant. « Recents », « Acces rapide » et « Ce PC » passaient par ce
					// site-ci, qui ne le renseignait pas -- une entree sur cinq avait son
					// chevron. Une regle appliquee a un seul endroit sur deux n'est pas une
					// regle. Elle est desormais posee LA OU LE NŒUD NAIT.
					//
					// LA PROFONDEUR se remonte par les parents (au plus quatre sauts), et les
					// TITRES de section ne comptent pas -- ils n'ont pas de chemin, ils ne sont
					// pas un niveau de l'arborescence.
					int32 prof = 0;
					for (int32 a = parent; a >= 0; a = vue.folders.nodes[(uint32)a].parent)
						if (!vue.folders.nodes[(uint32)a].path.Empty())
							++prof;
					n.enfantsPossibles =
						prof < kProfondeurRail
						&& cacheDossiers.Etat(chemin, horodatage, true)
							   == (uint8)NkContenuDossier::Plein;
					// et son etat de remplissage, par la MEME fonction, l'autre question.
					n.contenu = cacheDossiers.Etat(chemin, horodatage, false);
					if (n.contenu == (uint8)NkContenuDossier::Illisible)
						n.infobulle.Append(" — lecture refusée");
					vue.folders.nodes.PushBack(n);
					return (int32)vue.folders.nodes.Size() - 1;
				}
				
				/// L'empreinte de l'ensemble des noeuds deplies.
				nk_uint64 EmpreinteDeplie() const {
					nk_uint64 h = 1469598103934665603ull;
					for (uint32 i = 0; i < (uint32)vue.folders.toggled.Size(); ++i) {
						h ^= vue.folders.toggled[i];
						h *= 1099511628211ull;
					}
					return h;
				}
				
				/// ④ Les sous-dossiers de `chemin`, ajoutes sous `parent`. Rend le nombre pose.
				/// ⚠️ LE COUT EST BORNE, ET IL EST DIT : un `GetEntries` par dossier DEPLIE, et
				///    un test « a-t-il des sous-dossiers » par enfant pose -- soit un acces
				///    disque par entree visible du rail. Sur un dossier de trente sous-dossiers,
				///    c'est trente-et-un acces a la RECONSTRUCTION du rail, pas a chaque image :
				///    le rail ne se rebatit que sur navigation ou sur depliage.
				///    Au-dela de `kProfondeurRail`, on n'annonce plus rien : un rail deplie sur
				///    huit niveaux n'est plus un rail.
				static const int32 kProfondeurRail = 4;
				uint32 PoserSousDossiers(const char *chemin, int32 parent, int32 profondeur) {
					if (!chemin || !*chemin || profondeur > kProfondeurRail)
						return 0u;
					NkVector<NkDirectoryEntry> e =
						NkDirectory::GetEntries(NkPath(chemin), "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
					uint32 n = 0u;
					for (usize i = 0; i < e.Size(); ++i) {
						const char *nm = e[i].Name.CStr();
						if (!e[i].IsDirectory || !nm || !nm[0] || nm[0] == '.' || e[i].IsHidden)
							continue;
						const NkString sous = (NkPath(chemin) / nm).ToString();
						// L'HORODATAGE VIENT DE L'ENUMERATION QU'ON TIENT DEJA : il est gratuit,
						// et il sert de cle de fraicheur au cache. Le chevron et l'icone sont
						// poses par `AjouterNoeud`, seul site -- ils l'etaient ici, c'est-a-dire
						// pour les sous-dossiers SEULEMENT (defaut ③ de Rodolf).
						const int32 j = AjouterNoeud(sous.CStr(), nm, parent,
													 (nk_int64)e[i].ModificationTime);
						if (j < 0)
							continue;
						++n;
						// et s'il est DEJA deplie, on descend
						if (vue.folders.IsOpen(vue.folders.nodes[(uint32)j].id, false))
							n += PoserSousDossiers(sous.CStr(), j, profondeur + 1);
					}
					return n;
				}
				
				void ConstruireRail() {
					vue.folders.nodes.Clear();
					// ⑥ ET IL REVIENT EN HAUT. Le defilement du rail survivait a sa
					//    reconstruction : apres une navigation il pointait sur des rangees qui
					//    n'existaient plus -- et les sections du haut disparaissaient du champ
					//    sans que rien ne l'explique. C'est l'hypothese la plus probable pour
					//    la capture ou Rodolf ne voyait aucune section.
					vue.folders.scroll = 0.f;
					auto ajouter = [&](const char *chemin, const char *libelle, int32 parent) {
						return AjouterNoeud(chemin, libelle, parent);
					};
					// Un TITRE de section : pas de chemin, donc rien a suivre ; `locked`, donc rien
					// a selectionner. Deux barrieres pour un seul role -- la seconde tient meme si
					// un jour le rail apprend a naviguer sur un chemin vide.
					auto section = [&](const char *titre) -> int32 {
						NkTreeNode n;
						n.id = IdDe(titre);
						n.parent = -1;
						n.label = NkString(titre);
						n.locked = true;
						// ② UN TITRE A SA PROPRE FORME : trois traits. Il n'est pas un objet du
						//    systeme de fichiers et ne doit pas en avoir l'air.
						n.silhouette = (uint8)NkAssetIcone::Section;
						n.bandeau = true; // ③ sa bande plus sombre, sur toute la largeur
						vue.folders.nodes.PushBack(n);
						return (int32)vue.folders.nodes.Size() - 1;
					};
					if (!pickerConfine.Empty()) {
						// Parcours LIMITE : une seule racine, aucune section -- montrer « Ce PC »
						// dans un dialogue confine proposerait ce que le confinement interdit.
						NkString nm = NkPath(pickerConfine).GetFileName();
						ajouter(pickerConfine.CStr(), nm.Empty() ? pickerConfine.CStr() : nm.CStr(), -1);
						return;
					}
					// ── ③ RECENTS ─────────────────────────────────────────────
					// EN TETE : la plus utile est la plus haute. Rien du tout quand la liste est
					// vide -- une section vide occupe une ligne et n'apprend rien.
					// ⚠️ UN DOSSIER DISPARU NE S'AFFICHE PAS (`ajouter` teste `Exists`) mais RESTE
					//    dans la liste : une cle USB debranchee ne doit pas effacer l'historique.
					if ((uint32)recents.Size() > 1u) {
						const int32 sec = section("R\u00e9cents");
						uint32 poses = 0u;
						// ⑥ LA TETE EST LE DOSSIER COURANT : elle a sa propre section, plus bas.
						//    L'afficher deux fois dirait deux fois la meme chose.
						for (uint32 i = 1u; i < (uint32)recents.Size(); ++i) {
							const NkString nm = NomDeDossier(recents[i].CStr());
							if (ajouter(recents[i].CStr(), nm.Empty() ? recents[i].CStr() : nm.CStr(), sec) >= 0)
								++poses;
						}
						if (poses == 0u)
							vue.folders.nodes.RemoveAt((uint32)sec); // aucun n'existe encore : pas de titre orphelin
					}
					// ── ACCES RAPIDE ───────────────────────────────────────
					{
						const int32 sec = section("Acc\u00e8s rapide");
						const NkString home = NkDirectory::GetHomeDirectory().ToString();
						ajouter(home.CStr(), "Accueil", sec);
						static const NkDirectory::NkUserFolder kUsuels[] = {
							NkDirectory::NkUserFolder::Desktop, NkDirectory::NkUserFolder::Documents,
							NkDirectory::NkUserFolder::Downloads,
							NkDirectory::NkUserFolder::Pictures, NkDirectory::NkUserFolder::Music,
							NkDirectory::NkUserFolder::Videos};
						for (usize k = 0; k < sizeof(kUsuels) / sizeof(kUsuels[0]); ++k) {
							const NkString p = NkDirectory::GetUserFolder(kUsuels[k]).ToString();
							if (p.Empty())
								continue; // le systeme ne le connait pas : on n'invente pas
							const NkString nm = NkPath(p).GetFileName();
							ajouter(p.CStr(), nm.Empty() ? p.CStr() : nm.CStr(), sec);
						}
						for (uint32 i = 0; i < (uint32)favoris.Size(); ++i) {
							const NkString nm = NomDeDossier(favoris[i].CStr());
							ajouter(favoris[i].CStr(), nm.Empty() ? favoris[i].CStr() : nm.CStr(), sec);
						}
					}
					// ── CE PC : les volumes MONTES, avec leur etiquette ──────────────────
					{
						const int32 sec = section("Ce PC");
						NkVector<NkDriveInfo> volumes = NkFileSystem::GetDrives();
						uint32 poses = 0u;
						for (usize k = 0; k < volumes.Size(); ++k) {
							if (!volumes[k].IsReady)
								continue; // un lecteur vide fige le listage : on ne le propose pas
							// « D: Projets » -- la lettre SEULE ne dit pas ou l'on va.
							NkString lettre = volumes[k].Name;
							while (!lettre.Empty()) {
								const char d = lettre.CStr()[lettre.Length() - 1];
								if (d != '/' && d != '\\')
									break;
								lettre = NkString(lettre.CStr(), lettre.Length() - 1);
							}
							NkString lib = lettre;
							if (!volumes[k].Label.Empty()) {
								lib.Append(' ');
								lib.Append(volumes[k].Label.CStr());
							}
							if (ajouter(volumes[k].Name.CStr(), lib.CStr(), sec) >= 0)
								++poses;
						}
						if (poses == 0u)
							ajouter("/", "/", sec); // les systemes qui n'enumerent pas leurs montages
					}
					// ── LE DOSSIER COURANT : LUI, puis ses sous-dossiers ─────────────────
					// ⚠️ LA CHAINE D'ANCETRES A ETE RETIREE (05/09, nuit), et c'est une MESURE qui
					//    l'a decidee, pas un gout : elle ajoutait CINQ rangees et CINQ niveaux
					//    d'indentation pour `D: > Projets > 2026 > Nkentseu > Nkentseu-noge`. Deux
					//    consequences visibles sur la capture de Rodolf :
					//      - les sous-dossiers tombaient a la profondeur 6, ou il reste DIX PIXELS
					//        pour un nom -> ils s'affichaient tous « ... » ;
					//      - les trois sections, pourtant PEINTES (sonde 105), etaient repoussees
					//        au-dela des onze rangees visibles -- Rodolf ne voyait qu'une
					//        arborescence nue et concluait qu'elles n'existaient pas.
					//    Le FIL D'ARIANE, juste au-dessus, porte deja ces ancetres ET il est
					//    cliquable : les repeter ici coutait cinq rangees pour rien.
					//    Profondeur maximale desormais : 2 (le dossier, ses enfants).
					{
						// ⑤ (05/09, v5) LE DOSSIER COURANT EST CELUI DE RODOLF, OU IL N'Y EN A PAS.
						// Sa definition : « un dossier est appele dossier courant si on a REUSSI a
						// sauvegarder ou a charger un fichier de ce dossier-la. »
						//
						// LE REPLI `courant = pickerPath` ETAIT LE DEFAUT. Il faisait exister un
						// TROISIEME etat -- ni « le dossier d'une reussite », ni « aucun », mais « le
						// dossier ou l'on navigue » -- et c'est celui que Rodolf voyait : la section
						// suivait ses pas, avec toute son arborescence depliee. Un repli qui invente
						// une valeur ment plus surement qu'une absence.
						//
						// Donc : pas de reussite, pas de dossier courant. La section reste, avec UNE
						// entree grise qui DIT la regle -- la supprimer laisserait croire a une panne,
						// et l'ecrire ailleurs ferait une deuxieme source. Cette entree n'a PAS de
						// chemin : elle ne navigue nulle part, et le tronqueur de libelles la laisse
						// intacte (il ne touche qu'aux noeuds qui portent un chemin).
						const int32 sec = section("Dossier courant");
						const char *courant = DossierCourant();
						const bool aucun = !courant || !*courant || !NkDirectory::Exists(courant);
						if (aucun) {
							// PAS par `AjouterNoeud` : celui-la refuse un chemin qui n'existe pas,
							// et c'est bien ainsi -- une entree du rail SANS chemin n'est pas un
							// dossier, c'est une phrase. Verrouillee : elle ne se selectionne pas.
							NkTreeNode note;
							note.id = IdDe("::aucun-dossier-courant");
							note.parent = sec;
							note.label = NkString("(aucun — après un enregistrement ou une "
												  "ouverture réussie)");
							note.locked = true;
							vue.folders.nodes.PushBack(note);
						}
						const NkString nom = aucun ? NkString("") : NomDeDossier(courant);
						const int32 ici = aucun ? -1 : ajouter(courant, nom.CStr(), sec);
						if (ici >= 0) {
							vue.folders.SetOpen(vue.folders.nodes[(uint32)ici].id, true, true);
							// ④ UN SEUL SITE POSE LES SOUS-DOSSIERS, et il descend dans ceux qui sont
							//    DEJA deplies : le rail retrouve son etat apres une navigation.
							PoserSousDossiers(courant, ici, 1);
							vue.folders.active = vue.folders.nodes[(uint32)ici].id;
						}
					}
					// ⑤ OU L'ON EST : le rail le marque, MEME QUAND IL N'Y A PAS DE DOSSIER
					// COURANT. Cette boucle vivait DANS le bloc precedent, donc elle ne tournait
					// que si la section « Dossier courant » avait pose une entree -- le repli la
					// posait toujours, et l'avoir retire aurait eteint le marquage. Elle est
					// remontee d'un cran : elle cherche `pickerPath` dans TOUT le rail (recents,
					// acces rapide, volumes, sous-dossiers) et n'appartient a aucune section.
					for (uint32 k = 0; k < (uint32)vue.folders.nodes.Size(); ++k)
						if (!vue.folders.nodes[k].path.Empty()
							&& PathSame(vue.folders.nodes[k].path.CStr(), pickerPath))
							vue.folders.active = vue.folders.nodes[k].id;
				}

			private:
				NkVector<NkString> mExts; ///< proprietaire des `kindLabel` du volet droit
		};

		// ② Les bornes du rail. 180 px : la largeur en dessous de laquelle un nom de
		// dossier usuel (« Telechargements ») ne tient plus, mesuree au costume.
		// 45 % : au-dela, le volet des vignettes -- la raison d'etre du dialogue --
		// devient plus etroit que son rail.
		inline const float32 NkFilePickerNavState::kRailMin = 180.f;
		inline const float32 NkFilePickerNavState::kRailMax = 0.45f;
		
		// ── ④ LA GEOMETRIE DU DIALOGUE, EN UN SEUL ENDROIT (05/09, nuit) ──────────
		// Rodolf : « les boutons Annuler et Enregistrer ici debordent du dialogue. »
		// MESURE sur le code d'alors : `basH = 92`, puis `by += 62`, puis des boutons de
		// 34 -- somme 96 pour une reserve de 92. Les boutons etaient peints QUATRE
		// PIXELS SOUS le cadre, sans marge. Trois nombres poses a la main qui ne
		// s'additionnaient pas.
		//
		// ⚠️ ET ILS VIVAIENT DANS LE DESSIN : le temoin ne pouvait pas les lire, donc il
		//    ne pouvait pas constater qu'ils depassaient. C'est la troisieme fois de ce
		//    chantier qu'une valeur non lisible echappe a sa sonde.
		struct NkGeomSelecteur {
			NkRect cadre, ligneChemin, zone, labelNom, champNom, annuler, confirmer;
		};

		/// La geometrie complete, calculee UNE FOIS. Le dessin l'appelle, la sonde aussi.
		inline NkGeomSelecteur NkGeometrieSelecteur(float32 W, float32 H, float32 S, bool saveMode,
												 bool decalX, float32 offX, float32 offY) {
			NkGeomSelecteur g;
			const float32 pw = 900.f * S, ph = 620.f * S;
			const float32 px = (W - pw) * 0.5f + (decalX ? offX : 0.f);
			const float32 py = (H - ph) * 0.5f + (decalX ? offY : 0.f);
			g.cadre = {px, py, pw, ph};
			const float32 cx = px + 20.f * S, cwid = pw - 40.f * S;
			g.ligneChemin = {cx, py + 50.f * S, cwid, 30.f * S};
			// Les hauteurs du bas, UNE constante par element -- et `basH` est leur SOMME.
			const float32 hMarge = 14.f * S, hLabel = 18.f * S, hChamp = 30.f * S;
			const float32 hBouton = 34.f * S, hEcart = 10.f * S;
			const float32 basH = hMarge + (saveMode ? hLabel + hChamp + hEcart : 0.f) + hBouton + hMarge;
			const float32 yVolet = py + 90.f * S;
			g.zone = {cx, yVolet, cwid, ph - (yVolet - py) - basH};
			float32 by = py + ph - basH + hMarge;
			g.labelNom = saveMode ? NkRect{cx, by, cwid, hLabel} : NkRect{0.f, 0.f, 0.f, 0.f};
			g.champNom = saveMode ? NkRect{cx, by + hLabel, cwid, hChamp} : NkRect{0.f, 0.f, 0.f, 0.f};
			if (saveMode)
				by += hLabel + hChamp + hEcart;
			const float32 bw = 120.f * S;
			g.annuler = {px + pw - 20.f * S - bw * 2.f - 10.f * S, by, bw, hBouton};
			g.confirmer = {px + pw - 20.f * S - bw, by, bw, hBouton};
			return g;
		}

		// ── ④ CE QUE LE VOLET DOIT TAIRE DANS UN DIALOGUE (05/09, soir) ───────────
		// Sur la capture de Rodolf, le selecteur « choisir le dossier » affichait la bande
		// « Contenu » et les boutons « Creer / Importer / Tout enregistrer » : ce sont ceux
		// du NAVIGATEUR D'ASSETS, herites parce qu'on reutilise son dessin. Un dialogue
		// d'enregistrement n'importe rien et n'enregistre pas « tout » -- il proposait une
		// action qu'il ne ferait pas.
		//
		// ⚠️ ON UTILISE LE MOYEN DEJA POSE plutot que d'en ajouter un : les parametres du
		//    composant, portes par UNE INSTANCE. Aucun dessin n'est duplique, et le
		//    navigateur d'assets garde ses boutons (leurs defauts valent 1).
		//
		// UN SEUL SITE, ET IL PORTE UN NOM : le dessin appelle ceci, la sonde appelle ceci.
		// Une instance construite en ligne dans le dessin aurait ete inaccessible au temoin.
		inline NkComponentInstance &NkInstanceVoletSelecteur(bool selectionMultiple) {
			// STATIQUE : la reconstruire a chaque image allouerait un tableau de parametres
			// soixante fois par seconde pour trois valeurs.
			static NkComponentInstance inst(NkContentBrowserDecl());
			static bool posee = false;
			if (!posee) {
				inst.SetParam("show_header", 0.f);	// la bande « Contenu » : le titre est celui de la fenetre
				inst.SetParam("show_actions", 0.f); // Creer / Importer / Tout enregistrer
				posee = true;
			}
			// « Tout selectionner » n'a de sens que la ou plusieurs objets peuvent l'etre.
			inst.SetParam("show_select_all", selectionMultiple ? 1.f : 0.f);
			// ⑤ (05/09, nuit) LE DIALOGUE OFFRE SON PROPRE COMBO DE TRI (nom, date,
			//    taille, type, avec le sens) : celui du navigateur -- un simple texte qui
			//    bascule a-z / z-a sur le seul nom -- se tait. Deux commandes pour un
			//    reglage, c'est une de trop, et la moins capable gagnerait au clic.
			inst.SetParam("show_sort", 0.f);
			return inst;
		}

		// ── LE RENDU ────────────────────────────────────────────────────────────
		/// Rend VRAI tant que le selecteur est ouvert. La confirmation depose son
		/// resultat dans `fp` (les MEMES champs que l'ancien selecteur).
		// ── ① OU SE POSE LE CARTOUCHE D'INFOBULLE (2026-09-05, v5) ───────────────
		// Une FONCTION PURE, hors du dessin. C'est la quatrieme fois de ce chantier
		// qu'une valeur enfermee dans le dessin echappe a sa sonde (les hauteurs du
		// bas, la largeur du rail, la teinte de l'onglet) : ici le temoin peut LIRE le
		// rectangle et constater qu'il ne mord pas sur le rail.
		//
		// Les quatre proprietes, dans l'ordre ou elles sont tenues :
		//   1. `railDroite` est un MUR : la bulle commence a sa droite et n'y revient
		//      jamais — donc elle ne recouvre AUCUNE entree du rail, ni celle qu'on
		//      survole ni sa voisine (c'etait le defaut ②, l'entree sans libelle) ;
		//   2. elle est EN FACE de la rangee, centree sur elle, pas sous le curseur ;
		//   3. elle reste DANS LA FENETRE, bornee des quatre cotes ;
		//   4. si la fenetre est trop etroite pour les deux, le mur gagne : mieux vaut
		//      une bulle qui deborde a droite qu'une bulle qui mange le rail.
		inline NkRect NkPlacerInfobulle(float32 railDroite, float32 rangeeY, float32 rangeeH,
										float32 largeurTexte, float32 hauteurLigne, float32 W,
										float32 H, float32 S) {
			const float32 pad = 8.f * S, marge = 4.f * S;
			const float32 w = largeurTexte + pad * 2.f;
			const float32 h = hauteurLigne + pad;
			float32 x = railDroite + 8.f * S;
			float32 y = rangeeY + (rangeeH - h) * 0.5f;
			if (x + w > W - marge)
				x = W - marge - w;
			if (x < railDroite)
				x = railDroite; // le mur gagne
			if (y < marge)
				y = marge;
			if (y + h > H - marge)
				y = H - marge - h;
			return {x, y, w, h};
		}

		inline bool NkDrawFilePickerNav(nkgui::NkGuiContext &ctx, NkFilePickerNavState &fp,
										const NkFilePickerNavStyle &sty, const NkTheme &theme) {
			using namespace nkentseu::nkgui;
			if (!fp.pickerOpen) {
				// LE CACHE MEURT AVEC LE DIALOGUE. Rouvrir, c'est relire : entre deux
				// ouvertures, le disque a pu changer sans que personne nous previenne.
				fp.etaitOuvert = false;
				return false;
			}
			if (!fp.etaitOuvert) {
				fp.etaitOuvert = true;
				fp.cacheDossiers.Vider();
				fp.premierVu = -1;
				fp.dernierVu = -1;
			}
			const NkGuiFont *f = ctx.font;
			if (!f || !f->Valid())
				return true;
			// ① LA LISTE SUIT LE CHEMIN. Aucun appelant n'a a « armer » quoi que ce soit : si
			//    `pickerPath` n'est pas celui pour lequel la liste a ete batie, on relit.
			//    C'est ce qui rend le defaut de la capture (ouvert = vide) IMPOSSIBLE.
			if (fp.DoitRelire())
				fp.RelireDossier();
			// ④ LE RAIL SUIT LE DEPLIAGE. Un clic sur un chevron change l'ensemble des
			//    noeuds deplies ; on le CONSTATE (une empreinte) au lieu de demander a
			//    l'arbre de nous prevenir. Meme regle qu'au point ① du 05/09 : ce qui doit
			//    etre arme finit par ne pas l'etre.
			{
				const nk_uint64 emp = fp.EmpreinteDeplie();
				if (emp != fp.empreinteDeplie) {
					fp.empreinteDeplie = emp;
					fp.ConstruireRail();
				}
			}

			auto &dl = ctx.dlOverlay;
			const float32 W = (float32)ctx.viewW, H = (float32)ctx.viewH, S = ctx.S(1.f);
			const float32 asc = f->Ascent(), lh = f->LineHeight();
			const NkVec2 mp = ctx.input.mousePos;
			const bool click = ctx.input.mouseClicked[0];
			auto hit = [&](const NkRect &r) { return NkGuiRectContains(r, mp); };
			auto text = [&](float32 x, float32 y, const char *s, const NkColor &c) {
				dl.AddText(f->Face(), f->TexId(), {x, y + asc}, s, c);
			};
			auto sbtn = [&](const NkRect &r, const char *s) -> bool {
				const bool hov = hit(r);
				dl.AddRectFilled(r, hov ? sty.cadre.btnHover : sty.cadre.btn, 6.f * S);
				dl.AddRect(r, sty.cadre.border, 1.f);
				text(r.x + (r.w - f->MeasureWidth(s)) * 0.5f, r.y + (r.h - lh) * 0.5f, s, sty.cadre.text);
				return hov && click;
			};
			// ── ⑥⑤ UN PETIT MENU DEROULANT, SUR PLACE ─────────────────────────
			// ⚠️ POURQUOI PAS `NkComboButton` : il delegue sa liste au systeme de popups de
			//    la coquille (`openId`, `anchorOut`), qui dessine DANS une couche au-dessus
			//    de l'application -- or nous SOMMES deja cette couche, et nous avons
			//    reserve l'entree. Sa liste s'ouvrirait sous le dialogue. Ici, la liste est
			//    peinte au meme endroit que le reste, juste apres : quinze lignes, et
			//    aucune dependance a l'ordre des couches.
			// Rend l'indice choisi, ou -1. `ouvert` porte l'identite du menu ouvert.
			auto deroulant = [&](const NkRect &r, const char *libelle, const char *const *items,
								 int32 n, int32 courant, int32 id, int32 &ouvert) -> int32 {
				const bool hov = hit(r);
				dl.AddRectFilled(r, hov ? sty.cadre.btnHover : sty.cadre.btn, 5.f * S);
				dl.AddRect(r, sty.cadre.border, 1.f, 5.f * S);
				char t[160];
				snprintf(t, sizeof(t), "%s%s", libelle ? libelle : "",
					 (courant >= 0 && courant < n) ? items[courant] : "");
				text(r.x + 10.f * S, r.y + (r.h - lh) * 0.5f, t, sty.cadre.text);
				// le chevron : deux traits, jamais un caractere de police
				{
					const float32 mx2 = r.x + r.w - 16.f * S, my2 = r.y + r.h * 0.5f;
					dl.AddLine({mx2 - 4.f * S, my2 - 2.f * S}, {mx2, my2 + 2.f * S}, sty.cadre.sub, 1.4f);
					dl.AddLine({mx2, my2 + 2.f * S}, {mx2 + 4.f * S, my2 - 2.f * S}, sty.cadre.sub, 1.4f);
				}
				if (hov && click)
					ouvert = (ouvert == id) ? 0 : id;
				int32 choisi = -1;
				if (ouvert == id) {
					const float32 hl = 26.f * S;
					// LA LISTE MONTE quand elle deborderait du dialogue vers le bas.
					const float32 total = hl * (float32)n;
					// Le bas du dialogue : on le relit de la geometrie, qui est calculee
					// plus bas -- d'ou la hauteur du cadre passee en dur ici, la MEME
					// constante que `NkGeometrieSelecteur`.
					const float32 basDuCadre = (H - 620.f * S) * 0.5f + fp.pickerWinOffY + 620.f * S;
					const bool versLeHaut = r.y + r.h + total > basDuCadre - 8.f * S;
					const float32 y0 = versLeHaut ? r.y - total - 2.f * S : r.y + r.h + 2.f * S;
					const NkRect boite = {r.x, y0, r.w, total};
					dl.AddRectFilled(boite, sty.cadre.menuBg, 5.f * S);
					dl.AddRect(boite, sty.cadre.border, 1.f, 5.f * S);
					for (int32 k = 0; k < n; ++k) {
						const NkRect li = {boite.x, y0 + hl * (float32)k, boite.w, hl};
						const bool hl2 = hit(li);
						if (hl2)
							dl.AddRectFilled(li, sty.cadre.rowHover, 3.f * S);
						text(li.x + 10.f * S, li.y + (hl - lh) * 0.5f, items[k],
							 k == courant ? sty.cadre.textStrong : sty.cadre.text);
						if (hl2 && click) {
							choisi = k;
							ouvert = 0;
						}
					}
					// un clic AILLEURS referme, et ne traverse pas
					if (click && !hit(boite) && !hov)
						ouvert = 0;
				}
				return choisi;
			};
			auto pbtn = [&](const NkRect &r, const char *s, bool en) -> bool {
				const bool hov = en && hit(r);
				dl.AddRectFilled(r, !en ? sty.cadre.btn : hov ? sty.cadre.confirmHover : sty.cadre.accent,
								 6.f * S);
				text(r.x + (r.w - f->MeasureWidth(s)) * 0.5f, r.y + (r.h - lh) * 0.5f, s,
					 en ? sty.cadre.textStrong : sty.cadre.sub);
				return en && hov && click;
			};

			const bool saveMode = (fp.pickerFor == NkFilePickerState::PK_SaveFile);
			const bool dossierMode = (fp.pickerFor == NkFilePickerState::PK_PickFolder
									  || fp.pickerFor == NkFilePickerState::PK_Open);
			// ④ LA GEOMETRIE VIENT D'UNE SEULE FONCTION, celle que la sonde appelle aussi.
			const NkGeomSelecteur G =
				NkGeometrieSelecteur(W, H, S, saveMode, true, fp.pickerWinOffX, fp.pickerWinOffY);
			const float32 pw = G.cadre.w, ph = G.cadre.h;
			const float32 px = G.cadre.x, py = G.cadre.y;

			// ② CE DIALOGUE POSSEDE L'ENTREE — l'occlusion protege les widgets du kit,
			//    la reserve protege le code propre de l'application (une toile qui lit
			//    `ctx.input` sans passer par un widget). A re-armer a chaque image :
			//    c'est ce qui la rend caduque toute seule a la fermeture.
			ctx.PushOcclusion({0.f, 0.f, W, H}, 100);
			NkGuiContext::NkInputLayerScope _couche(ctx, 100);
			ctx.input.ReserverSaisie();
			const bool premiereDeLaPile = (ctx.modalDepth == 0);
			++ctx.modalDepth;
			if (premiereDeLaPile)
				dl.AddRectFilled({0.f, 0.f, W, H}, sty.cadre.backdrop);
			dl.AddRectFilled({px, py, pw, ph}, sty.cadre.card, 10.f * S);
			dl.AddRect({px, py, pw, ph}, sty.cadre.border, 1.5f);
			dl.AddRectFilled({px, py, pw, 3.f * S}, sty.cadre.accent, 10.f * S);
			text(px + 20.f * S, py + 16.f * S, fp.PickerTitle(), sty.cadre.text);
			{ // la barre de titre deplace la fenetre
				const NkRect titleBar = {px, py, pw - 44.f * S, 40.f * S};
				if (click && hit(titleBar)) {
					fp.pickerWinDrag = true;
					fp.pickerWinDragX = mp.x - px;
					fp.pickerWinDragY = mp.y - py;
				}
				if (fp.pickerWinDrag && ctx.input.mouseDown[0]) {
					fp.pickerWinOffX = (mp.x - fp.pickerWinDragX) - (W - pw) * 0.5f;
					fp.pickerWinOffY = (mp.y - fp.pickerWinDragY) - (H - ph) * 0.5f;
				}
				if (!ctx.input.mouseDown[0])
					fp.pickerWinDrag = false;
			}

			const float32 cx = px + 20.f * S, cwid = pw - 40.f * S;
			// ⑤ declare ICI : la ligne du haut en a besoin pour son bouton « + ».
			const bool peutCreer = (saveMode || dossierMode);
			float32 y = G.ligneChemin.y;
			bool fieldClicked = false;
			// ── LA LIGNE DE CHEMIN : « Remonter », le chemin editable, « Aller » ──
			{
				const float32 hb = 30.f * S;
				if (sbtn({cx, y, 40.f * S, hb}, "▲"))
					fp.AllerA(NkPath(fp.pickerPath).GetParent().ToString().CStr());
				const NkRect r = {cx + 48.f * S, y, cwid - 48.f * S - 84.f * S - 8.f * S
												   - (peutCreer ? hb + 8.f * S : 0.f),
								   hb};
				if (hit(r) && click) {
					fp.pickerEditing = true;
					fp.pickerSaveFocus = false;
					fieldClicked = true;
				}
				NkOverlayTextField(ctx, dl, f, r, fp.pickerPath, (int32)sizeof(fp.pickerPath),
								   fp.pickerEditing);
				if (sbtn({cx + cwid - 84.f * S, y, 84.f * S, hb}, "Aller")) {
					fp.pickerEditing = false;
					fp.AllerA(fp.pickerPath);
				}
				// ⑤ LE BOUTON DISCRET : un carre avec un « + », a cote d'« Aller ». Il
				//    n'apparait que la ou creer a un sens ; en ouverture de fichier, creer un
				//    dossier vide ne menerait a rien.
				if (peutCreer && sbtn({cx + cwid - 84.f * S - 8.f * S - hb, y, hb, hb}, "+")) {
					fp.creationOuverte = !fp.creationOuverte;
					fp.nouveauFocus = fp.creationOuverte;
					fp.messageCreation = NkString();
				}
			}
			// ⑤ LA CREATION DE DOSSIER, EN HAUT ET DISCRETE (05/09, nuit). Elle prend la
			//    place de la ligne de chemin le temps qu'on la remplisse, puis disparait.
			//    Une rangee permanente pleine largeur pour un geste qu'on fait une fois sur
			//    vingt poussait le nom du fichier -- le geste PRINCIPAL -- vers le bas.
			if (peutCreer && fp.creationOuverte) {
				const float32 hb = 30.f * S;
				const NkRect rc = {cx, y, cwid - 200.f * S, hb};
				if (hit(rc) && click) {
					fp.nouveauFocus = true;
					fp.pickerEditing = false;
					fp.pickerSaveFocus = false;
					fieldClicked = true;
				}
				NkOverlayTextField(ctx, dl, f, rc, fp.nouveauNom, (int32)sizeof(fp.nouveauNom),
								   fp.nouveauFocus);
				if (!fp.nouveauNom[0] && !fp.nouveauFocus)
					text(rc.x + 8.f * S, rc.y + (hb - lh) * 0.5f, "Nom du nouveau dossier", sty.cadre.sub);
				if (sbtn({cx + cwid - 196.f * S, y, 96.f * S, hb}, "Cr\u00e9er")) {
					if (fp.CreerDossier())
						fp.creationOuverte = false;
				}
				if (sbtn({cx + cwid - 96.f * S, y, 96.f * S, hb}, "Annuler")) {
					fp.creationOuverte = false;
					fp.nouveauNom[0] = '\0';
					fp.messageCreation = NkString();
				}
				if (!fp.messageCreation.Empty())
					text(cx, y + 32.f * S, fp.messageCreation.Data(), sty.cadre.sub);
			}
			y += 40.f * S;

			// ── LE VOLET : le navigateur de contenu du kit, tel quel ────────────
			// ⑤ LE BAS NE PORTE PLUS LA CREATION (05/09, nuit) : elle est passee en haut,
			//    derriere un bouton. Le nom du fichier remonte d'autant.
			// ④ LES HAUTEURS DU BAS S'ADDITIONNENT (05/09, nuit). Elles etaient trois
			//    nombres poses a la main (92, +62, 34) dont la somme depassait le cadre de
			//    QUATRE PIXELS -- les boutons etaient peints SOUS le dialogue. Ici, une
			//    constante par element, et `basH` est LEUR SOMME : le dessin plus bas relit
			//    exactement les memes. Deux endroits pour une meme hauteur, c'est deux
			//    endroits pour se tromper.
			const NkRect zone = G.zone;
			int32 aOuvrir = -1;	  // un dossier a suivre APRES le dessin
			NkString cible;		  // le chemin a suivre
			// ① L'INFOBULLE DU RAIL : relevee ici, PEINTE TOUT EN BAS DE CETTE FONCTION.
			//    Voir le bloc « L'INCRUSTATION SE PEINT EN DERNIER » a la fin.
			NkString bulle;
			float32 bulleX = 0.f, bulleY = 0.f, bulleH = 0.f;
			{
				// ④ Le volet se tait sur ce que ce mode n'exige pas -- un seul site, nomme.
				NkContentBrowserStyle volet = sty.volet;
				// ② LA LARGEUR DU RAIL EST IMPOSEE EN PIXELS. Le composant l'exprime en
				//    FRACTION (`tree_width`) : on lui donne donc la fraction qui correspond a
				//    la largeur voulue, bornee. Sans ca, le rail retrecit quand la fenetre
				//    retrecit -- et tronque le plus quand on a le moins de place.
				if (fp.largeurRail < NkFilePickerNavState::kRailMin)
					fp.largeurRail = NkFilePickerNavState::kRailMin;
				if (fp.largeurRail > zone.w * NkFilePickerNavState::kRailMax)
					fp.largeurRail = zone.w * NkFilePickerNavState::kRailMax;
				// L'instance est MODIFIEE a chaque image (le mode, puis la largeur du rail) :
				// la fonction rend donc une reference NON constante. Un `const_cast` ici
				// aurait ete l'aveu que la signature ment.
				NkComponentInstance &inst = NkInstanceVoletSelecteur(!saveMode && !dossierMode);
				inst.SetParam("tree_width", zone.w > 0.f ? fp.largeurRail / zone.w : 0.18f);
				volet.values = &inst;
				// ② RE-TRONQUER LES LIBELLES AU MILIEU, ici et pas a la construction : c'est
				//    ici qu'on connait la police ET la largeur. Le nom complet reste dans
				//    `path`, d'ou on le relit a chaque image -- le libelle n'est qu'un
				//    affichage, jamais une donnee.
				for (uint32 i = 0; i < (uint32)fp.vue.folders.nodes.Size(); ++i) {
					NkTreeNode &n = fp.vue.folders.nodes[i];
					if (n.path.Empty())
						continue; // un TITRE de section : il tient, et il ne se tronque pas
					uint32 prof = 0u;
					for (int32 a = n.parent; a >= 0; a = fp.vue.folders.nodes[(uint32)a].parent)
						++prof;
					const NkString complet = NkPath(n.path).GetFileName();
					char court[160];
					NkFilePickerNavState::TronquerMilieu(complet.Empty() ? n.path.CStr() : complet.CStr(),
														 court, sizeof(court), *f,
														 fp.largeurRail - 34.f * S - (float32)prof * 16.f * S);
					n.label = NkString(court);
				}
				nkgui::PushOverlay(ctx); // le composant doit peindre dans la couche modale
				NkGuiComponentPaint peintre(ctx, theme);
				NkComponentInput in;
				in.surfaceScale = S;
				in.mouseX = mp.x;
				in.mouseY = mp.y;
				in.wheel = hit({zone.x, zone.y, zone.w, zone.h}) ? ctx.input.wheel : 0.f;
				in.mouseDown = ctx.input.mouseDown[0];
				in.mousePressed = click && !fieldClicked;
				in.mouseReleased = ctx.input.mouseReleased[0];
				in.doubleClick = ctx.input.mouseDoubleClicked[0];
				in.rightPressed = ctx.input.mouseClicked[1];
				in.ctrl = ctx.input.ctrlDown;
				in.shift = ctx.input.shiftDown;
				in.alt = ctx.input.altDown;
				if (in.wheel != 0.f)
					ctx.input.wheel = 0.f;
				// Les greffes : deux evenements suffisent — suivre un dossier, choisir
				// un fichier. Le composant SIGNALE, ce fichier decide.
				struct Pont {
						NkFilePickerNavState *fp;
						NkString *cible;
				} pont{&fp, &cible};
				NkContentBrowserHooks h;
				h.user = &pont;
				h.onNavigate = [](void *u, const char *chemin) {
					Pont *p = (Pont *)u;
					// L'ARBRE porte un chemin complet ; LE FIL D'ARIANE porte un
					// libelle — on le traduit par son index, jamais par son texte.
					if (chemin && NkDirectory::Exists(chemin))
						*p->cible = NkString(chemin);
				};
				const NkContentBrowserResult res =
					NkDrawContentBrowser(peintre, in, {zone.x, zone.y, zone.w, zone.h}, fp.vue,
										 volet, h);
				nkgui::PopOverlay(ctx);
				// ── VIDE OU PLEIN : SEULEMENT CE QUI EST A L'ECRAN (05/09, v5) ─────
				// Le volet vient de dire quelles entrees il a REELLEMENT dessinees. On ne
				// sonde que celles-la : sur un dossier de 124 entrees, c'est la quinzaine
				// visible, pas les 124. Le cache fait le reste -- une entree deja sondee ne
				// retourne pas au disque tant que son horodatage n'a pas bouge.
				// ⚠️ APRES LE DESSIN, PAS PENDANT : on ecrit dans `vue.entries`, que le
				//    composant tenait encore par reference une ligne plus haut.
				fp.premierVu = res.premierVisible;
				fp.dernierVu = res.dernierVisible;
				if (res.premierVisible >= 0) {
					const uint32 fin = (uint32)res.dernierVisible < (uint32)fp.vue.entries.Size()
										   ? (uint32)res.dernierVisible
										   : (uint32)fp.vue.entries.Size() - 1u;
					for (uint32 i = (uint32)res.premierVisible; i <= fin; ++i) {
						NkAssetEntry &e = fp.vue.entries[i];
						if (!e.isFolder || e.contenu != (uint8)NkContenuDossier::Inconnu)
							continue;
						e.contenu = fp.cacheDossiers.Etat(e.path.CStr(), e.dateModif, false);
					}
				}
				if (!res.infobulle.Empty()) {
					bulle = res.infobulle;
					bulleX = res.infobulleX;
					bulleY = res.infobulleY;
					bulleH = res.infobulleH;
				}
				if (res.navigatedCrumb >= 0
					&& res.navigatedCrumb < (int32)fp.cheminsCrumb.Size())
					cible = fp.cheminsCrumb[(uint32)res.navigatedCrumb];
				if (res.activatedIndex >= 0 && res.activatedIndex < (int32)fp.vue.entries.Size())
					aOuvrir = res.activatedIndex;
				else if (res.selectionChanged && fp.vue.active >= 0
						 && fp.vue.active < (int32)fp.vue.entries.Size()) {
					const NkAssetEntry &e = fp.vue.entries[(uint32)fp.vue.active];
					if (!e.isFolder && saveMode)
						NkFilePickerState::CopyTo(fp.pickerSaveName, e.name.CStr(),
												  (int32)sizeof(fp.pickerSaveName));
				}
			}
			dl.AddRect(zone, sty.cadre.border, 1.f);
			// ② LA POIGNEE : une bande verticale entre les deux volets. Elle est plus large
			//    que le trait qu'elle deplace (8 px contre 1) -- une poignee qu'il faut
			//    viser au pixel n'est pas une poignee.
			{
				const NkRect p = {zone.x + fp.largeurRail - 4.f * S, zone.y, 8.f * S, zone.h};
				const bool surP = hit(p);
				if (surP || fp.railGlisse)
					dl.AddRectFilled({p.x + 3.f * S, p.y, 2.f * S, p.h}, sty.cadre.accent);
				if (surP && click)
					fp.railGlisse = true;
				if (fp.railGlisse && ctx.input.mouseDown[0])
					fp.largeurRail = mp.x - zone.x;
				if (!ctx.input.mouseDown[0])
					fp.railGlisse = false;
			}

			// ── LE BAS : le nom (mode enregistrer), puis Annuler / Confirmer ────

			if (saveMode) {
				text(G.labelNom.x, G.labelNom.y, "Nom du fichier", sty.cadre.sub);
				const NkRect r = G.champNom;
				if (hit(r) && click) {
					fp.pickerSaveFocus = true;
					fp.pickerEditing = false;
					fieldClicked = true;
				}
				NkOverlayTextField(ctx, dl, f, r, fp.pickerSaveName, (int32)sizeof(fp.pickerSaveName),
								   fp.pickerSaveFocus);
			}
			// ⑤ LE COMBO DE TRI : la cle a gauche, le SENS a droite. Deux commandes, parce
			//    que « Date (recent) » et « Date (ancien) » dans une meme liste ferait huit
			//    entrees pour quatre choix -- et obligerait a relire le libelle pour savoir
			//    dans quel sens on est.
			{
				static const char *const kCles[4] = {"Nom", "Date", "Taille", "Type"};
				const NkRect rt = {cx, G.annuler.y, 190.f * S, G.annuler.h};
				const int32 ct = deroulant(rt, "Trier par : ", kCles, 4, (int32)fp.vue.sortCle, 2,
											  fp.menuOuvert);
				if (ct >= 0 && (uint8)ct != fp.vue.sortCle) {
					fp.vue.sortCle = (uint8)ct;
					fp.relire = true;
				}
				// LE SENS : une fleche dessinee, jamais un caractere de police.
				const NkRect rs = {cx + 196.f * S, G.annuler.y, G.annuler.h, G.annuler.h};
				if (sbtn(rs, "")) {
					fp.vue.sortAsc = !fp.vue.sortAsc;
					fp.relire = true;
				}
				{
					const float32 mx3 = rs.x + rs.w * 0.5f, my3 = rs.y + rs.h * 0.5f;
					const float32 d = fp.vue.sortAsc ? 1.f : -1.f;
					dl.AddLine({mx3, my3 - 5.f * S * d}, {mx3, my3 + 5.f * S * d}, sty.cadre.text, 1.4f);
					dl.AddLine({mx3 - 4.f * S, my3 + 1.f * S * d}, {mx3, my3 + 5.f * S * d},
							   sty.cadre.text, 1.4f);
					dl.AddLine({mx3 + 4.f * S, my3 + 1.f * S * d}, {mx3, my3 + 5.f * S * d},
							   sty.cadre.text, 1.4f);
				}
			}
			// ⑥ LE COMBO DE FILTRE, a gauche de la barre d'action -- la ou Windows le met.
			//    Affiche des qu'il y a DEUX groupes : un seul choix n'est pas un choix.
			if ((int32)fp.filtres.Size() >= 2) {
				const NkRect rf = {G.annuler.x - 280.f * S, G.annuler.y, 270.f * S, G.annuler.h};
				const char *noms[16];
				const int32 nf = (int32)fp.filtres.Size() < 16 ? (int32)fp.filtres.Size() : 16;
				for (int32 k = 0; k < nf; ++k)
					noms[k] = fp.filtres[(uint32)k].nom.CStr();
				const int32 ch = deroulant(rf, "Filtre : ", noms, nf, fp.filtreActif, 1, fp.menuOuvert);
				if (ch >= 0 && ch != fp.filtreActif) {
					fp.filtreActif = ch;
					fp.relire = true; // le listage suit immediatement
				}
			}
			{
				const bool pret = saveMode ? (fp.pickerSaveName[0] != '\0')
										   : (dossierMode ? fp.pickerPath[0] != '\0'
														  : (fp.vue.active >= 0
															 && fp.vue.active < (int32)fp.vue.entries.Size()
															 && !fp.vue.entries[(uint32)fp.vue.active].isFolder));
				if (sbtn(G.annuler, "Annuler")) {
					fp.PickerCancel();
					--ctx.modalDepth;
					return false;
				}
				if (pbtn(G.confirmer, fp.PickerConfirmLabel(), pret)) {
					fp.pickerConfirmed = true;
					fp.pickerResultFor = fp.pickerFor;
					NkFilePickerState::CopyTo(fp.pickerResultPath, fp.pickerPath,
											  (int32)sizeof(fp.pickerResultPath));
					if (saveMode)
						NkFilePickerState::CopyTo(fp.pickerResultName, fp.pickerSaveName,
												  (int32)sizeof(fp.pickerResultName));
					else if (!dossierMode && fp.vue.active >= 0
							 && fp.vue.active < (int32)fp.vue.entries.Size()) {
						const NkAssetEntry &e = fp.vue.entries[(uint32)fp.vue.active];
						NkFilePickerState::CopyTo(fp.pickerResultPath, e.path.CStr(),
												  (int32)sizeof(fp.pickerResultPath));
						NkFilePickerState::CopyTo(fp.pickerResultName, e.name.CStr(),
												  (int32)sizeof(fp.pickerResultName));
					}
					fp.pickerOpen = false;
					fp.pickerFor = NkFilePickerState::PK_None;
					--ctx.modalDepth;
					return false;
				}
			}

			// ── LES SUITES, APRES LE DESSIN ─────────────────────────────────────
			// ⚠️ JAMAIS PENDANT : changer `vue.entries` au milieu du dessin invaliderait
			//    la reference que le composant tient encore.
			if (aOuvrir >= 0) {
				const NkAssetEntry &e = fp.vue.entries[(uint32)aOuvrir];
				if (e.isFolder)
					fp.AllerA(e.path.CStr());
				else if (!saveMode) { // double-clic sur un fichier = confirmer
					fp.pickerConfirmed = true;
					fp.pickerResultFor = fp.pickerFor;
					NkFilePickerState::CopyTo(fp.pickerResultPath, e.path.CStr(),
											  (int32)sizeof(fp.pickerResultPath));
					NkFilePickerState::CopyTo(fp.pickerResultName, e.name.CStr(),
											  (int32)sizeof(fp.pickerResultName));
					fp.pickerOpen = false;
					fp.pickerFor = NkFilePickerState::PK_None;
					--ctx.modalDepth;
					return false;
				} else
					NkFilePickerState::CopyTo(fp.pickerSaveName, e.name.CStr(),
											  (int32)sizeof(fp.pickerSaveName));
			}
			// ── ① L'INCRUSTATION SE PEINT EN DERNIER (2026-09-05, v5) ───────────
			// Rodolf, sur deux captures : « l'infobulle se peint au milieu du rail et
			// masque les entrees », et « une entree du rail n'a pas de libelle ».
			// UNE SEULE CAUSE : le cartouche etait peint par l'arbre, dans SA liste de
			// dessin, a « souris + 12/+16 » — donc dans le flux du rail, sur la rangee
			// du dessous, dont il volait le libelle.
			//
			// Ici, quatre proprietes tenues ensemble, et c'est ce qui fait la reponse :
			//   1. EN DERNIER — apres le volet, apres le champ, apres les boutons : plus
			//      rien ne peut passer par-dessus (regle du 03/09 sur les incrustations) ;
			//   2. HORS DU RAIL — adossee a son bord DROIT (`bulleX`), donc elle ne
			//      recouvre AUCUNE entree, ni celle qu'on survole ni sa voisine ;
			//   3. EN FACE de la rangee survolee, pas sous le curseur : on lit le nom et
			//      le chemin d'un seul regard ;
			//   4. DANS LA FENETRE — bornee des quatre cotes, fond OPAQUE et liseré.
			if (!bulle.Empty()) {
				const float32 padB = 8.f * S;
				const NkRect rB = NkPlacerInfobulle(bulleX, bulleY, bulleH,
													f->MeasureWidth(bulle.CStr()), lh, W, H, S);
				dl.AddRectFilled(rB, sty.cadre.menuBg, 4.f * S);
				dl.AddRect(rB, sty.cadre.border, 1.f);
				text(rB.x + padB, rB.y + (rB.h - lh) * 0.5f, bulle.CStr(), sty.cadre.text);
			}
			if (!cible.Empty())
				fp.AllerA(cible.CStr());
			--ctx.modalDepth;
			return true;
		}

		// ════════════════════════════════════════════════════════════════════════
		//  ⑥ LE POINT D'ENTREE PAR DEFAUT DU KIT (2026-09-05, soir)
		// ════════════════════════════════════════════════════════════════════════
		//  Rodolf : « le selecteur de fichier qu'on ecrit dans NkUIDesign -- que ce soit
		//  pour creer un dossier, selectionner un dossier ou un fichier, pour ouvrir ou
		//  pour sauvegarder -- doit etre l'outil par defaut. Meme dans NK3DModeler il
		//  doit l'utiliser, pareil pour les autres applications. »
		//
		//  CE QUE CES DEUX FONCTIONS RENDENT POSSIBLE : une application n'a plus a
		//  connaitre ni le style, ni les roles de theme, ni les parametres du volet.
		//  Ouvrir = une ligne. Dessiner = une ligne. C'est la condition pour que la
		//  bascule d'une application tienne en UNE LIGNE et pas en un chantier.
		//
		//  ⚠️ L'ANCIEN RESTE APPELABLE. `NkDrawFilePicker` n'est pas touche : une
		//     application qui veut la colonne unique l'appelle encore, et les huit
		//     consommateurs recompilent sans une ligne changee. Ce qui change est LE
		//     DEFAUT -- c'est-a-dire ce que prend celui qui ne choisit pas.

		/// LE STYLE PAR DEFAUT, resolu depuis la DECLARATION du navigateur de contenu.
		/// Aucune couleur ecrite ici : chaque jeton prend le role que sa declaration
		/// annonce, et le theme de l'hote lui donne sa valeur. Un jeton ajoute au
		/// composant demain sera resolu sans toucher cette fonction.
		inline NkFilePickerNavStyle NkStyleSelecteurDefaut() {
			const NkComponentDecl &d = NkContentBrowserDecl();
			auto role = [&](const char *jeton) -> uint16 {
				for (uint16 i = 0; i < d.tokenCount; ++i)
					if (NkComponentDecl::StrEq(d.tokens[i].name, jeton))
						return NkResolveRole(d.tokens[i].defaultRole);
				return NkResolveRole("TextMuted");
			};
			NkFilePickerNavStyle s;
			s.volet.panelBg = role("panel_bg");
			s.volet.headerBg = role("header_bg");
			s.volet.border = role("border");
			s.volet.text = role("text");
			s.volet.textMuted = role("text_muted");
			s.volet.cardBg = role("card_bg");
			s.volet.cardFooterBg = role("card_footer_bg");
			s.volet.activeMark = role("active_mark");
			s.volet.chosenMark = role("chosen_mark");
			s.volet.folderTint = role("folder_tint");
			s.volet.chipBg = role("chip_bg");
			s.volet.badgeText = role("badge_text");
			s.volet.statusBg = role("status_bg");
			s.volet.variant = NkBrowserVariant::Grid;
			return s;
		}

		/// LE SELECTEUR DE FICHIERS PAR DEFAUT. Une ligne dans la boucle d'une
		/// application : `editorkit::NkDrawSelecteur(ctx, monEtat, monTheme);`
		/// Rend VRAI tant qu'il est ouvert. Le resultat se lit dans les MEMES champs
		/// que l'ancien (`pickerConfirmed`, `pickerResultPath`, `pickerResultName`).
		inline bool NkDrawSelecteur(nkgui::NkGuiContext &ctx, NkFilePickerNavState &fp,
							const NkTheme &theme) {
			// Le style est reconstruit a chaque image et c'est VOULU : le theme peut
			// changer a chaud (clair / sombre), et un style mis en cache garderait les
			// couleurs de l'ancien -- c'est le defaut « une valeur figee au demarrage ».
			// Treize resolutions de role par image : mesure, pas supposition -- une
			// recherche lineaire dans une table de treize noms.
			return NkDrawFilePickerNav(ctx, fp, NkStyleSelecteurDefaut(), theme);
		}

		/// LES QUATRE MODES, nommes. Ce sont les PK_* de la classe de base, rappeles ici
		/// pour qu'une application n'ait pas a savoir lequel choisir :
		///   - `NkSelecteurOuvrirFichier`   : choisir un fichier existant ;
		///   - `NkSelecteurOuvrirDossier`   : choisir un dossier existant ;
		///   - `NkSelecteurCreerDossier`    : choisir un dossier, avec la rangee de
		///                                    creation (c'est le meme mode : creer PUIS
		///                                    choisir est un seul geste, pas deux) ;
		///   - `NkSelecteurEnregistrer`     : un dossier et un nom de fichier.
		enum : nkentseu::int32 {
			NkSelecteurOuvrirFichier = NkFilePickerState::PK_File,
			NkSelecteurOuvrirDossier = NkFilePickerState::PK_PickFolder,
			NkSelecteurCreerDossier = NkFilePickerState::PK_PickFolder,
			NkSelecteurEnregistrer = NkFilePickerState::PK_SaveFile,
		};

	} // namespace editorkit
} // namespace nkentseu
