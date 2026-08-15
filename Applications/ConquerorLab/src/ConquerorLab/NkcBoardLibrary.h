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

namespace nkentseu {
	namespace conqueror {

		struct NkcBoardFile {
				NkString name;	 ///< « diamant_4j.json » — ce que voit l'utilisateur
				NkString path;	 ///< chemin complet
		};

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
					if (mDir.Empty()) {
						logger.Warnf("[lab] grilles de travail : aucun dossier defini");
						return;
					}
					NkVector<NkString> paths = NkDirectory::GetFiles(mDir.CStr(), "*.json");
					uint32			   ignores = 0;
					for (usize i = 0; i < paths.Size(); ++i) {
						NkcBoardFile f;
						f.name = NkPath(paths[i]).GetFileName();
						f.path = paths[i];
						if (f.name.Empty()) { ++ignores; continue; }
						mFiles.PushBack(f);
					}
					logger.Infof("[lab] grilles de travail : %u retenue(s) sur %u fichier(s) "
								 ".json vu(s)%s  [%s]",
								 static_cast<uint32>(mFiles.Size()),
								 static_cast<uint32>(paths.Size()),
								 ignores ? " (des noms illisibles ont ete ignores)" : "",
								 mDir.CStr());
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
					if (!inst || !vt.LoadBoardJson) { mMsg = "Le moteur n'accepte pas de plateau."; return false; }
					if (idx >= mFiles.Size()) { mMsg = "Fichier introuvable."; return false; }

					const NkString text = NkFile::ReadAllText(mFiles[idx].path.CStr());
					if (text.Empty()) {
						mMsg = "Fichier vide ou illisible : ";
						mMsg += mFiles[idx].name;
						return false;
					}
					if (!vt.LoadBoardJson(inst, text.CStr())) {
						mMsg = "Le moteur a REFUSE ce plateau : ";
						mMsg += mFiles[idx].name;
						mMsg += "  (cellules manquantes ou JSON invalide)";
						return false;
					}
					// La FORME DES CELLULES est de la presentation : le contrat ne la
					// transporte pas, et le moteur n'a aucune raison de la connaitre.
					// C'est donc l'atelier qui la lit, ici, ou il a le texte en main.
					mShape = ReadCellShape(text);

					mMsg = "Plateau charge : ";
					mMsg += mFiles[idx].name;
					return true;
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
						mMsg = "Ecriture impossible : ";
						mMsg += path;
						return false;
					}
					mMsg = "Plateau exporte : ";
					mMsg += name;
					Refresh();
					return true;
				}

			private:
				NkString			   mDir;
				NkVector<NkcBoardFile> mFiles;
				NkString			   mMsg;
				/// Forme declaree par le dernier plateau charge (presentation).
				NkcCellShape		   mShape = NkcCellShape::Auto;
		};

	} // namespace conqueror
} // namespace nkentseu
