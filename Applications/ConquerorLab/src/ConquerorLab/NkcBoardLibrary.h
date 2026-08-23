#pragma once
// =============================================================================
// NkcBoardLibrary — les GRILLES, comme des donnees deposees dans un dossier.
//
// POURQUOI UN DOSSIER ET PAS DU CODE
// ----------------------------------
// « Le plateau n'est pas une constante du code : c'est un descripteur
// serialisable » (REGLES §4). Un stagiaire — ou le designer — doit pouvoir
// ajouter une forme sans toucher au moteur ni a l'atelier :
//
//     1. il ecrit   Build/ConquerorLab/boards/diamant_4j.json
//     2. il sauvegarde
//     3. la grille apparait dans la liste du panneau « Regles »
//     4. il la charge, la partie repart dessus
//
// Le format est CELUI DU CONTRAT (ConquerorRulesABI.h, LoadBoardJson) :
//
//   {"topology":"HEX_POINTY",
//    "cells":[[0,0],[1,0], ...],
//    "blocked":[[2,3]],
//    "starts":[{"player":0,"q":0,"r":0,"level":0}, ...],
//    "min_players":2,"max_players":2}
//
// C'est le MODULE qui lit ce JSON, pas l'atelier : celui-ci ne fait que passer
// la chaine a `LoadBoardJson`. Un moteur de stagiaire qui accepte un champ de
// plus le verra donc sans qu'on modifie quoi que ce soit ici.
//
// Un exemple est ECRIT AU PREMIER LANCEMENT (export du plateau par defaut du
// moteur charge) : un dossier vide n'apprend a personne quel format on attend.
// =============================================================================

#include "ConquerorLab/NkcWinClean.h"

#include "Conqueror/ConquerorRulesABI.h"

#include "ConquerorLab/NkcBoardRender.h"

#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkPath.h"
#include "NKLogger/NkLog.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKSerialization/NkArchive.h"

#include <cstdio>

namespace nkentseu {
	namespace conqueror {

		struct NkcBoardFile {
				NkString name;	 ///< « diamant_4j.json » — le nom de FICHIER
				NkString path;	 ///< chemin complet
				/// Ce que l'utilisateur LIT dans la liste. Vaut le champ « nom » du
				/// `.json` s'il en porte un, sinon le nom de fichier.
				///
				/// ⚠️ CE CHAMP EST NE D'UNE TROUVAILLE QUI ETAIT FAUSSE, ET IL
				/// SURVIT POUR UNE AUTRE RAISON — la note vaut plus que le champ.
				///
				/// J'avais rapporte le 15/08 que `rectangle_8x6.json` et
				/// `carre_8x8_*.json` etaient PENCHES a l'ecran, en deduisant leur
				/// forme d'une signature d'etiquetage. **C'etait faux.** Mesure du
				/// 16/08, plateau par plateau : ils sont en topologie SQUARE, ou
				/// `CoordToUnit` rend {2q, 2r} — aucun cisaillement possible. Mon
				/// script appliquait le decalage hexagonal `q + (r>>1)` a des
				/// plateaux carres : l'escalier que j'observais etait un artefact de
				/// l'instrument, pas une propriete du plateau.
				///
				/// Les SEULS plateaux reellement penches sont `parallelogramme_6x7`
				/// et `parallelogramme_8x5` — et ils s'appellent « parallelogramme ».
				/// **Les 15 noms livres sont exacts** ; aucun n'a ete touche.
				///
				/// Le champ reste parce qu'il sert independamment : un stagiaire qui
				/// depose `mon_plateau_v3_final.json` peut lui donner un nom lisible
				/// sans renommer son fichier. Retro-compatible par construction — un
				/// plateau sans le champ retombe sur son nom de fichier, donc sur le
				/// comportement d'avant, et il n'y a rien a migrer.
				NkString libelle;
		};

		/// GRAVITE D'UN MESSAGE — PORTEE, JAMAIS DEVINEE.
		///
		/// Le panneau Regles colorait le bandeau en cherchant des MOTS dans le
		/// texte : « REFUSE », « impossible », « illisible ». Le message d'erreur
		/// ecrit « Fichier ILLISIBLE » en capitales ; la recherche etait sensible
		/// a la casse ; l'echec le plus grave de la liste s'affichait donc en
		/// VERT, exactement comme un chargement reussi. Deviner la gravite depuis
		/// la prose est un piege : la prose change, la couleur ment, et personne
		/// ne relit le panneau qui colore pour verifier.
		enum class NkcMsgKind : uint8 { Info = 0, Ok = 1, Avertissement = 2, Erreur = 3 };

		class NkcBoardLibrary {
			public:
				/// `dir`  : dossier de travail, ou le stagiaire depose SES grilles.
				/// `seed` : bibliotheque livree avec le depot, recopiee au premier
				///          lancement. Sans elle, l'atelier s'ouvrait sur une liste
				///          d'UNE entree — la liste deroulante existait, mais il n'y
				///          avait rien a choisir. Une bibliotheque vide n'apprend rien
				///          sur un format.
				void Init(const NkString &dir, const NkString &seed = NkString()) noexcept {
					mDir = dir;
					NkDirectory::CreateRecursive(mDir.CStr());
					if (!seed.Empty()) SeedFrom(seed);
					Refresh();
				}

				/// Recopie les grilles livrees qui manquent. On ne REMPLACE jamais un
				/// fichier existant : le stagiaire a pu modifier `hexagone_6x7.json`,
				/// et son travail ne doit pas disparaitre au prochain demarrage.
				///
				/// ⚠️ `NkDirectory::GetFiles` REND DES CHEMINS COMPLETS, pas des noms
				/// de fichiers (NkDirectory.cpp:355 — `NkPath(path) / cFileName`).
				/// Les recoller derriere `mDir` fabriquait
				/// `<travail>/boards/D:/.../amorcage/boards/x.json` : un chemin
				/// impossible sous Windows, donc `Exists` faux, donc `Copy` en echec —
				/// et comme on ne journalisait QUE `copied > 0`, l'amorcage ne recopiait
				/// RIEN, en silence, depuis toujours. Mesure du 2026-08-15 : 15 grilles
				/// dans le dossier livre, 0 dans le dossier de travail.
				void SeedFrom(const NkString &seedDir) noexcept {
					if (seedDir.Empty() || !NkDirectory::Exists(seedDir.CStr())) {
						logger.Warnf("[lab] aucune bibliotheque de grilles livree a : %s",
									 seedDir.Empty() ? "(chemin vide)" : seedDir.CStr());
						return;
					}
					// Chemins COMPLETS — on ne les reconstruit pas, on s'en sert tels quels.
					NkVector<NkString> paths = NkDirectory::GetFiles(seedDir.CStr(), "*.json");
					uint32			   copied = 0, kept = 0, failed = 0;
					for (usize i = 0; i < paths.Size(); ++i) {
						const NkString name = NkPath(paths[i]).GetFileName();
						if (name.Empty()) continue;
						NkString dst = mDir;
						dst += "/";
						dst += name;
						if (NkFile::Exists(dst.CStr())) { ++kept; continue; }
						if (NkFile::Copy(paths[i].CStr(), dst.CStr(), false)) ++copied;
						else {
							++failed;
							logger.Errorf("[lab] copie impossible : %s -> %s",
										  paths[i].CStr(), dst.CStr());
						}
					}
					// On parle MEME quand rien n'a bouge : un dossier qu'on croit
					// installe et qui ne l'est pas est exactement ce qui a coute ce bug.
					logger.Infof("[lab] grilles livrees : %u vue(s), %u installee(s), "
								 "%u deja presente(s), %u en echec  [%s -> %s]",
								 static_cast<uint32>(paths.Size()), copied, kept, failed,
								 seedDir.CStr(), mDir.CStr());
				}

				const NkString				   &Dir() const noexcept { return mDir; }
				const NkVector<NkcBoardFile>   &Files() const noexcept { return mFiles; }
				const NkString				   &Message() const noexcept { return mMsg; }
				/// Gravite du dernier message. Le panneau s'en sert pour la couleur.
				NkcMsgKind					   MessageKind() const noexcept { return mKind; }

				/// Relit le dossier de travail. Meme piege que `SeedFrom` : `GetFiles`
				/// rend des chemins COMPLETS. On garde le chemin tel quel, et on
				/// n'affiche que le nom du fichier — c'est ce que l'utilisateur nomme.
				///
				/// ⚠️ CETTE FONCTION EST CELLE QUI DECIDE DE CE QUE LE STAGIAIRE VOIT,
				/// et elle se taisait. `SeedFrom` disait ce qu'il avait installe, mais
				/// la question posee — « j'ai ajoute un fichier, pourquoi n'apparait-il
				/// pas ? » — se repond ICI et nulle part ailleurs. Un « Rafraichir » qui
				/// ne laisse aucune trace ne se distingue pas d'un « Rafraichir » qui
				/// regarde le mauvais dossier : c'est le meme silence qui a coute le
				/// defaut d'amorcage. Appelee sur EVENEMENT seulement (demarrage,
				/// bouton, apres ecriture d'un exemple) — jamais par image, donc
				/// journaliser ici ne noie rien.
				void Refresh() noexcept {
					mFiles.Clear();
					mMsg.Clear();
					mKind = NkcMsgKind::Info;
					if (mDir.Empty()) {
						logger.Warnf("[lab] grilles de travail : aucun dossier defini");
						mMsg  = "Aucun dossier de grilles defini.";
						mKind = NkcMsgKind::Erreur;
						return;
					}
					NkVector<NkString> paths = NkDirectory::GetFiles(mDir.CStr(), "*.json");
					uint32			   ignores = 0;
					for (usize i = 0; i < paths.Size(); ++i) {
						NkcBoardFile f;
						f.name = NkPath(paths[i]).GetFileName();
						f.path = paths[i];
						if (f.name.Empty()) { ++ignores; continue; }
						f.libelle = LibelleDe(f.path, f.name);
						mFiles.PushBack(f);
					}
					logger.Infof("[lab] grilles de travail : %u retenue(s) sur %u fichier(s) "
								 ".json vu(s)%s  [%s]",
								 static_cast<uint32>(mFiles.Size()),
								 static_cast<uint32>(paths.Size()),
								 ignores ? " (des noms illisibles ont ete ignores)" : "",
								 mDir.CStr());
					SignalerCeQuiEstIgnore(static_cast<uint32>(paths.Size()));
				}

				/// CE QU'ON NE LIT PAS DOIT SE DIRE, SINON C'EST LE MEME SILENCE.
				///
				/// `Refresh` ne balaie que `*.json`. Un fichier depose sous un autre
				/// nom -- `mon_plateau` sans extension, `.txt`, ou `.json.txt` que
				/// l'Explorateur affiche comme `.json` -- est donc INVISIBLE, et rien
				/// ne le disait : la liste restait la meme, et le stagiaire concluait
				/// que « ca ne marche pas ». Un fichier ignore en silence est un defaut
				/// de l'atelier, pas une erreur de l'utilisateur.
				///
				/// Journalise ET affiche : le panneau « Regles » est le seul endroit
				/// que le stagiaire regarde, le journal le seul qu'on relira apres coup.
				void SignalerCeQuiEstIgnore(uint32 vusJson) noexcept {
					NkVector<NkString> tous = NkDirectory::GetFiles(mDir.CStr(), "*");
					NkString		   noms;
					uint32			   autres = 0;
					for (usize i = 0; i < tous.Size(); ++i) {
						const NkString nom = NkPath(tous[i]).GetFileName();
						if (nom.Empty() || FinitParJson(nom)) continue;
						++autres;
						if (autres <= 3) {
							if (!noms.Empty()) noms += ", ";
							noms += nom;
						}
					}
					if (autres > 0) {
						logger.Warnf("[lab] %u fichier(s) IGNORE(S) dans %s : %s%s "
									 "-- une grille doit se terminer par .json",
									 autres, mDir.CStr(), noms.CStr(),
									 autres > 3 ? ", ..." : "");
						char tmp[96];
						std::snprintf(tmp, sizeof(tmp),
									  "%u fichier(s) ignore(s), pas en .json : ", autres);
						mKind = NkcMsgKind::Avertissement;
						mMsg  = tmp;
						mMsg += noms;
						if (autres > 3) mMsg += ", ...";
					}
					SignalerDossierVoisin(vusJson + autres);
				}

				static bool FinitParJson(const NkString &nom) noexcept {
					const usize n = nom.Size();
					if (n < 5) return false;
					const char *c = nom.CStr() + (n - 5);
					return c[0] == '.' &&
						   (c[1] == 'j' || c[1] == 'J') && (c[2] == 's' || c[2] == 'S') &&
						   (c[3] == 'o' || c[3] == 'O') && (c[4] == 'n' || c[4] == 'N');
				}

				/// Le dossier d'a cote qui ressemble au bon. On ne le lit PAS -- on le
				/// NOMME. Deviner ce que l'utilisateur voulait est le debut des
				/// comportements qu'on ne sait plus expliquer ; le lui dire lui laisse
				/// la main, et lui coute dix secondes au lieu d'une journee. Un
				/// stagiaire ecrit `travail/board` (singulier) ou `travail/grilles`,
				/// l'atelier lit `travail/boards`, et les deux ont raison de leur point
				/// de vue. Seul l'atelier peut lever le malentendu.
				void SignalerDossierVoisin(uint32 vus) noexcept {
					if (vus > 0) return;   // le bon dossier n'est pas vide : rien a chercher
					const usize cut = DerniereBarre(mDir);
					if (cut == NkString::npos) return;
					const NkString parent = mDir.SubStr(0, cut);
					static const char *const kProches[] = {"board", "grille", "grilles",
															   "plateau", "plateaux", "Boards"};
					for (usize k = 0; k < sizeof(kProches) / sizeof(kProches[0]); ++k) {
						NkString cand = parent;
						cand += "/";
						cand += kProches[k];
						if (!NkDirectory::Exists(cand.CStr())) continue;
						if (NkDirectory::GetFiles(cand.CStr(), "*").Empty()) continue;
						logger.Warnf("[lab] le dossier %s contient des fichiers, mais "
									 "l'atelier lit %s -- renommez-le en \"boards\"",
									 cand.CStr(), mDir.CStr());
						mKind = NkcMsgKind::Avertissement;
						mMsg  = "Vos fichiers sont dans le dossier \"";
						mMsg += kProches[k];
						mMsg += "\" ; l'atelier lit \"boards\". Renommez le dossier.";
						return;
					}
				}

				static usize DerniereBarre(const NkString &p) noexcept {
					const usize a = p.RFind('/');
					const usize b = p.RFind('\\');
					if (a == NkString::npos) return b;
					if (b == NkString::npos) return a;
					return (a > b) ? a : b;
				}

				/// Ecrit un exemple si le dossier est vide, a partir du plateau que le
				/// moteur charge expose. Le format documente vaut mieux qu'un format
				/// decrit : celui-ci est forcement valide, puisqu'il vient du module.
				void EnsureExample(const NkcRulesVTable &vt, NkcRules inst) noexcept {
					if (!mFiles.Empty() || mDir.Empty() || !inst || !vt.GetBoardJson) return;
					const char *json = vt.GetBoardJson(inst);
					if (!json || !*json) return;
					NkString path = mDir;
					path += "/exemple_plateau_par_defaut.json";
					if (NkFile::WriteAllText(path.CStr(), json)) {
						logger.Infof("[lab] exemple de plateau ecrit : %s", path.CStr());
						Refresh();
					}
				}

				/// Charge le fichier `idx` dans l'instance de regles. Renvoie false et
				/// pose un message lisible si le module refuse — c'est le seul retour
				/// que l'auteur du fichier aura.
				bool LoadInto(const NkcRulesVTable &vt, NkcRules inst, usize idx) noexcept {
					mMsg.Clear();
					mKind = NkcMsgKind::Info;
					if (!inst || !vt.LoadBoardJson) { mMsg = "Le moteur n'accepte pas de plateau."; mKind = NkcMsgKind::Erreur; return false; }
					if (idx >= mFiles.Size()) { mMsg = "Fichier introuvable."; mKind = NkcMsgKind::Erreur; return false; }

					// « ILLISIBLE » SANS RAISON EST LE MEME SILENCE QU'AILLEURS.
					// Le message d'avant disait « vide ou illisible » suivi du seul NOM
					// de fichier : le lecteur ne pouvait savoir ni LEQUEL des deux, ni
					// OU l'atelier avait regarde. Les trois causes se distinguent en
					// trois appels, et chacune appelle un geste DIFFERENT -- c'est
					// exactement pour cela qu'il faut les separer. Le chemin COMPLET,
					// pas le nom : un nom ne dit pas dans quel dossier on a cherche.
					const char *chemin = mFiles[idx].path.CStr();
					if (!NkFile::Exists(chemin)) {
						mKind = NkcMsgKind::Erreur;
						mMsg  = "Fichier disparu depuis le dernier Rafraichir : ";
						mMsg += mFiles[idx].path;
						return false;
					}
					const nk_int64 taille = NkFile::GetFileSize(chemin);
					if (taille == 0) {
						mKind = NkcMsgKind::Erreur;
						mMsg  = "Fichier VIDE (0 octet) : ";
						mMsg += mFiles[idx].path;
						return false;
					}
					const NkString text = NkFile::ReadAllText(chemin);
					if (text.Empty()) {
						char tmp[112];
						std::snprintf(tmp, sizeof(tmp),
									  "Fichier ILLISIBLE : %lld octets sur le disque, 0 lu "
									  "(droits, ou fichier ouvert ailleurs) : ",
									  static_cast<long long>(taille));
						mKind = NkcMsgKind::Erreur;
						mMsg  = tmp;
						mMsg += mFiles[idx].path;
						return false;
					}
					if (!vt.LoadBoardJson(inst, text.CStr())) {
						// « CELLULES MANQUANTES OU JSON INVALIDE » EST ENCORE UN OU.
						// Le module renvoie un simple faux : il ne dit pas pourquoi, et
						// il n'a pas a le dire — c'est le contrat. L'atelier, lui, a le
						// texte en main : il peut RELIRE le fichier avec l'analyseur de
						// la pile et rapporter la cause exacte, en distinguant les deux
						// gestes qui n'ont rien a voir — corriger une virgule, ou
						// ajouter une clef.
						NkString erreur;
						NkArchive archive;
						if (!NkJSONReader::ReadArchive(text.View(), archive, &erreur)) {
							mMsg = "JSON INVALIDE : ";
							mMsg += erreur.Empty() ? NkString("syntaxe incorrecte") : erreur;
							mMsg += "  --  ";
							mMsg += mFiles[idx].path;
						} else {
							const char *manque = ClefManquante(text);
							if (manque) {
								mMsg = "JSON correct, mais la clef \"";
								mMsg += manque;
								mMsg += "\" manque : ";
								mMsg += mFiles[idx].path;
							} else {
								mMsg = "Le moteur a REFUSE ce plateau (JSON correct, clefs "
									   "presentes : valeurs hors bornes ?) : ";
								mMsg += mFiles[idx].path;
							}
						}
						mKind = NkcMsgKind::Erreur;
						return false;
					}
					// La FORME DES CELLULES est de la presentation : le contrat ne la
					// transporte pas, et le moteur n'a aucune raison de la connaitre.
					// C'est donc l'atelier qui la lit, ici, ou il a le texte en main.
					mShape = ReadCellShape(text);

					mKind = NkcMsgKind::Ok;
					mMsg  = "Plateau charge : ";
					mMsg += mFiles[idx].name;
					return true;
				}

				/// La premiere clef ATTENDUE qui manque, ou nullptr si elles sont la.
				/// La liste vit ici et nulle part ailleurs : c'est l'atelier qui lit
				/// les plateaux du stagiaire, et un fichier a qui il manque `cells`
				/// n'est pas « invalide », il est INCOMPLET — le geste n'est pas le
				/// meme, et le message doit le dire.
				static const char *ClefManquante(const NkString &texte) noexcept {
					static const char *const kClefs[] = {"topology", "cells"};
					for (usize k = 0; k < sizeof(kClefs) / sizeof(kClefs[0]); ++k) {
						NkString motif = "\"";
						motif += kClefs[k];
						motif += "\"";
						if (texte.Find(motif.CStr()) == NkString::npos) return kClefs[k];
					}
					return nullptr;
				}

				/// Forme declaree par le dernier plateau charge. `Auto` si le fichier
				/// n'en dit rien — donc pour tous les plateaux ecrits avant ce champ.
				NkcCellShape CellShape() const noexcept { return mShape; }

				/// Lit `"cell_shape": "..."`. Analyse volontairement minuscule : on
				/// cherche la cle, on saute jusqu'aux guillemets de la valeur, on lit
				/// la premiere lettre. Ecrire un analyseur JSON complet ici pour UN
				/// champ optionnel serait payer cher une souplesse que personne ne
				/// demande — le moteur, lui, analyse deja le fichier pour de bon.
				static NkcCellShape ReadCellShape(const NkString &text) noexcept {
					const usize k = text.Find("\"cell_shape\"");
					if (k == NkString::npos) return NkcCellShape::Auto;
					usize i = k + 12;
					while (i < text.Size() && text[i] != '"' && text[i] != ',' && text[i] != '}') ++i;
					if (i >= text.Size() || text[i] != '"') return NkcCellShape::Auto;
					++i;
					if (i >= text.Size()) return NkcCellShape::Auto;
					const char first[2] = {text[i], '\0'};
					return NkcCellShapeFromName(first);
				}

				/// Exporte le plateau courant sous `name` — le point de depart naturel
				/// quand on veut fabriquer une variante.
				bool Export(const NkcRulesVTable &vt, NkcRules inst, const char *name) noexcept {
					mMsg.Clear();
					if (!inst || !vt.GetBoardJson || !name || !*name) return false;
					NkString path = mDir;
					path += "/";
					path += name;
					if (!NkFile::WriteAllText(path.CStr(), vt.GetBoardJson(inst))) {
						mKind = NkcMsgKind::Erreur;
						mMsg  = "Ecriture impossible : ";
						mMsg += path;
						return false;
					}
					mKind = NkcMsgKind::Ok;
					mMsg  = "Plateau exporte : ";
					mMsg += name;
					Refresh();
					return true;
				}

			private:
				/// Le nom LU par l'utilisateur : le champ « nom » du `.json` s'il
				/// existe, sinon le nom de fichier.
				///
				/// RETRO-COMPATIBLE PAR CONSTRUCTION : un plateau sans le champ —
				/// c'est-a-dire tous ceux qu'un stagiaire a deja ecrits — retombe
				/// exactement sur le comportement d'avant. Rien a migrer.
				///
				/// On passe par `NkJSONReader` plutot que par une recherche de
				/// chaine : le lecteur existe, il est deja lie a cette application,
				/// et un scanner maison se serait trompe le jour ou un nom contient
				/// une accolade.
				static NkString LibelleDe(const NkString &chemin, const NkString &parDefaut) noexcept {
					const NkString texte = NkFile::ReadAllText(chemin.CStr());
					if (texte.Empty()) return parDefaut;
					NkArchive archive;
					if (!NkJSONReader::ReadArchive(texte.View(), archive)) return parDefaut;
					NkString nom;
					if (!archive.GetString("nom", nom) || nom.Empty()) return parDefaut;
					return nom;
				}

				NkString			   mDir;
				NkVector<NkcBoardFile> mFiles;
				NkString			   mMsg;
				NkcMsgKind			   mKind = NkcMsgKind::Info;
				/// Forme declaree par le dernier plateau charge (presentation).
				NkcCellShape		   mShape = NkcCellShape::Auto;
		};

	} // namespace conqueror
} // namespace nkentseu
