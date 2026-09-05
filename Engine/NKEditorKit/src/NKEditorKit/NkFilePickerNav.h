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

		// ── L'ETAT ──────────────────────────────────────────────────────────────
		struct NkFilePickerNavState : public NkFilePickerState {
				NkContentBrowserModel vue;	 ///< le volet droit ET son rail de gauche
								NkVector<NkString> cheminsCrumb; ///< le chemin COMPLET de chaque miette
				NkVector<NkString> favoris;		 ///< poses par l'hote (chemins absolus)

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

				// ── L'OUVERTURE ─────────────────────────────────────────────────
				/// `purpose` : les memes PK_* que l'ancien selecteur. `ext` = filtre
				/// d'extension (« .png »), vide = tout. `nomPropose` = le nom pre-rempli
				/// en mode enregistrer.
				void OuvrirNav(int32 purpose, const char *depart, const char *ext, const char *nomPropose,
							   char *buf, int32 cap) {
					OpenPickerBase(purpose, depart, buf, cap, nullptr, nullptr);
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
					for (usize i = 0; i < e.Size(); ++i) {
						const char *nm = e[i].Name.CStr();
						if (!nm || !nm[0] || nm[0] == '.' || e[i].IsHidden)
							continue;
						if (e[i].IsDirectory)
							dirs.PushBack(e[i].Name);
						else if (pickerFileExt.Empty() || EndsWithI(nm, pickerFileExt.CStr()))
							files.PushBack(e[i].Name);
					}
					// Tri par insertion : les listes d'un dossier sont courtes, et une
					// dependance de tri de plus ne se justifierait pas ici.
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
					trier(files);
					// ⚠️ LES DOSSIERS D'ABORD, TOUJOURS — et meme en tri decroissant.
					//    C'est la regle du navigateur historique, et la seule qui rende
					//    un dossier atteignable sans defiler toute une liste d'images.
					for (uint32 i = 0; i < (uint32)dirs.Size(); ++i) {
						NkAssetEntry a;
						a.name = dirs[i];
						a.path = (NkPath(pickerPath) / dirs[i].CStr()).ToString();
						a.isFolder = true;
						a.kindRole = roleDossier;
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
						char ext[16];
						ExtDe(files[i].CStr(), ext, sizeof(ext));
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
				void ConstruireRail() {
					vue.folders.nodes.Clear();
					auto ajouter = [&](const char *chemin, const char *libelle, int32 parent) -> int32 {
						if (!chemin || !*chemin || !NkDirectory::Exists(chemin))
							return -1;
						NkTreeNode n;
						n.id = IdDe(chemin);
						n.parent = parent;
						n.label = NkString(libelle && *libelle ? libelle : chemin);
						n.path = NkString(chemin);
						n.kindRole = roleDossier;
						vue.folders.nodes.PushBack(n);
						return (int32)vue.folders.nodes.Size() - 1;
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
					if (!recents.Empty()) {
						const int32 sec = section("R\u00e9cents");
						uint32 poses = 0u;
						for (uint32 i = 0; i < (uint32)recents.Size(); ++i) {
							const NkString nm = NkPath(recents[i]).GetFileName();
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
							const NkString nm = NkPath(favoris[i]).GetFileName();
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
					if (pickerPath[0]) {
						const int32 sec = section("Dossier courant");
						NkString nom = NkPath(pickerPath).GetFileName();
						if (nom.Empty())
							nom = NkString(pickerPath);
						const int32 ici = ajouter(pickerPath, nom.CStr(), sec);
						if (ici >= 0) {
							vue.folders.SetOpen(vue.folders.nodes[(uint32)ici].id, true, true);
							NkVector<NkDirectoryEntry> e = NkDirectory::GetEntries(
								NkPath(pickerPath), "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
							for (usize i = 0; i < e.Size(); ++i) {
								const char *nm = e[i].Name.CStr();
								if (!e[i].IsDirectory || !nm || !nm[0] || nm[0] == '.' || e[i].IsHidden)
									continue;
								ajouter((NkPath(pickerPath) / e[i].Name.CStr()).ToString().CStr(), nm, ici);
							}
							vue.folders.active = vue.folders.nodes[(uint32)ici].id;
						}
					}
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
		inline const NkComponentInstance &NkInstanceVoletSelecteur(bool selectionMultiple) {
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
			return inst;
		}

		// ── LE RENDU ────────────────────────────────────────────────────────────
		/// Rend VRAI tant que le selecteur est ouvert. La confirmation depose son
		/// resultat dans `fp` (les MEMES champs que l'ancien selecteur).
		inline bool NkDrawFilePickerNav(nkgui::NkGuiContext &ctx, NkFilePickerNavState &fp,
										const NkFilePickerNavStyle &sty, const NkTheme &theme) {
			using namespace nkentseu::nkgui;
			if (!fp.pickerOpen)
				return false;
			const NkGuiFont *f = ctx.font;
			if (!f || !f->Valid())
				return true;
			// ① LA LISTE SUIT LE CHEMIN. Aucun appelant n'a a « armer » quoi que ce soit : si
			//    `pickerPath` n'est pas celui pour lequel la liste a ete batie, on relit.
			//    C'est ce qui rend le defaut de la capture (ouvert = vide) IMPOSSIBLE.
			if (fp.DoitRelire())
				fp.RelireDossier();

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
			const float32 pw = 900.f * S, ph = 620.f * S;
			const float32 px = (W - pw) * 0.5f + fp.pickerWinOffX, py = (H - ph) * 0.5f + fp.pickerWinOffY;

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
			float32 y = py + 50.f * S;
			bool fieldClicked = false;
			// ── LA LIGNE DE CHEMIN : « Remonter », le chemin editable, « Aller » ──
			{
				const float32 hb = 30.f * S;
				if (sbtn({cx, y, 40.f * S, hb}, "▲"))
					fp.AllerA(NkPath(fp.pickerPath).GetParent().ToString().CStr());
				const NkRect r = {cx + 48.f * S, y, cwid - 48.f * S - 84.f * S - 8.f * S, hb};
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
			}
			y += 40.f * S;

			// ── LE VOLET : le navigateur de contenu du kit, tel quel ────────────
			// ⑥ La rangee « Nouveau dossier » prend sa place quand le mode la demande.
			const bool peutCreer = (saveMode || dossierMode);
			const float32 basH = ((saveMode ? 92.f : 52.f) + (peutCreer ? 40.f : 0.f)) * S;
			const NkRect zone = {cx, y, cwid, ph - (y - py) - basH - 16.f * S};
			int32 aOuvrir = -1;	  // un dossier a suivre APRES le dessin
			NkString cible;		  // le chemin a suivre
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
				NkComponentInstance &inst = const_cast<NkComponentInstance &>(
					NkInstanceVoletSelecteur(!saveMode && !dossierMode));
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
			float32 by = py + ph - basH;
			// ⑥ NOUVEAU DOSSIER : un champ et un bouton, la ou creer a un sens (enregistrer
			//    quelque part, ou choisir un dossier). En OUVERTURE DE FICHIER, creer un
			//    dossier vide ne menerait a rien -- la rangee ne s'affiche pas.
			if (peutCreer) {
				const NkRect rc = {cx, by + 4.f * S, cwid - 150.f * S - 8.f * S, 28.f * S};
				if (hit(rc) && click) {
					fp.nouveauFocus = true;
					fp.pickerEditing = false;
					fp.pickerSaveFocus = false;
					fieldClicked = true;
				}
				NkOverlayTextField(ctx, dl, f, rc, fp.nouveauNom, (int32)sizeof(fp.nouveauNom),
								   fp.nouveauFocus);
				if (!fp.nouveauNom[0] && !fp.nouveauFocus)
					text(rc.x + 8.f * S, rc.y + (rc.h - lh) * 0.5f, "Nom du nouveau dossier", sty.cadre.sub);
				if (sbtn({cx + cwid - 150.f * S, by + 4.f * S, 150.f * S, 28.f * S}, "Nouveau dossier"))
					fp.CreerDossier();
				if (!fp.messageCreation.Empty())
					text(cx, by + 34.f * S, fp.messageCreation.Data(), sty.cadre.sub);
				by += 40.f * S;
			}

			if (saveMode) {
				text(cx, by + 6.f * S, "Nom du fichier", sty.cadre.sub);
				const NkRect r = {cx, by + 24.f * S, cwid, 30.f * S};
				if (hit(r) && click) {
					fp.pickerSaveFocus = true;
					fp.pickerEditing = false;
					fieldClicked = true;
				}
				NkOverlayTextField(ctx, dl, f, r, fp.pickerSaveName, (int32)sizeof(fp.pickerSaveName),
								   fp.pickerSaveFocus);
				by += 62.f * S;
			}
			{
				const float32 bw = 120.f * S, bh = 34.f * S;
				const bool pret = saveMode ? (fp.pickerSaveName[0] != '\0')
										   : (dossierMode ? fp.pickerPath[0] != '\0'
														  : (fp.vue.active >= 0
															 && fp.vue.active < (int32)fp.vue.entries.Size()
															 && !fp.vue.entries[(uint32)fp.vue.active].isFolder));
				if (sbtn({px + pw - 20.f * S - bw * 2.f - 10.f * S, by, bw, bh}, "Annuler")) {
					fp.PickerCancel();
					--ctx.modalDepth;
					return false;
				}
				if (pbtn({px + pw - 20.f * S - bw, by, bw, bh}, fp.PickerConfirmLabel(), pret)) {
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
