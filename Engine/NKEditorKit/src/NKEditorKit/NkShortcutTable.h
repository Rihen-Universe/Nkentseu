#pragma once
// -----------------------------------------------------------------------------
// @File    NkShortcutTable.h
// @Brief   Table de raccourcis CONFIGURABLE : la liaison touche -> commande vit
//          en donnee, jamais en dur dans le code.
// @Author  Rihen
// @License Proprietary - Free to use and modify
//
// PROBLEME RESOLU
//   Les raccourcis de l'editeur de modelisation sont aujourd'hui ecrits en dur,
//   dans une suite de `if (k == NkKey::NK_G) …` de plusieurs centaines de lignes.
//   Trois consequences, toutes constatees :
//     1. l'interface ne peut PAS afficher le raccourci d'une commande — elle ne
//        le connait pas. NkEditorCommand porte bien un champ `shortcut`, mais il
//        est explicitement COSMETIQUE : une chaine recopiee a la main, qui peut
//        donc mentir sans que rien ne le signale ;
//     2. l'utilisateur ne peut rien reconfigurer ;
//     3. les CONFLITS sont invisibles. Cas reel : `Shift+S` etait deja pris par
//        « ombrage smooth », ce qui a force des combinaisons moins naturelles
//        pour la pile de modificateurs — decouvert a la compilation, par hasard,
//        alors qu'une table l'aurait dit immediatement.
//
// CE QUE CETTE TABLE GARANTIT
//   • une SEULE source de verite : ce qui declenche la commande est ce qui est
//     affiche a cote d'elle ;
//   • la detection des conflits AVANT qu'ils ne se manifestent a l'usage ;
//   • la reconfiguration, sans recompilation ;
//   • des CONTEXTES : la meme touche peut signifier autre chose en mode edition
//     qu'en mode objet — c'est le cas chez Blender, et notre code le fait deja de
//     facon implicite, en imbriquant des `if`.
//
// CE QU'ELLE NE FAIT PAS
//   Elle ne LIT pas le clavier et n'appelle pas les commandes. Elle repond a la
//   question « quelle commande pour cette combinaison, dans ce contexte ? ».
//   L'appelant garde la main sur le moment ou il pose la question — un editeur a
//   des etats (operation modale en cours, champ de saisie actif) ou aucun
//   raccourci ne doit passer.
//
// ZERO-STL, ZERO-ALLOCATION : capacite fixe, comme NkEditorCommand.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkEditorExport.h"

#include "NKEvent/NkKeyboardEvent.h"

namespace nkentseu {
	namespace editorkit {

		// Modificateurs, combinables. Volontairement independant de l'etat clavier
		// du systeme : l'appelant compose ce masque a partir de ce qu'il observe.
		enum NkShortcutMod : uint8 {
			NK_SC_NONE = 0,
			NK_SC_CTRL = 1 << 0,
			NK_SC_SHIFT = 1 << 1,
			NK_SC_ALT = 1 << 2,
		};

		// CONTEXTES. Un raccourci ne vaut que dans le sien ; `NK_SCTX_GLOBAL` vaut
		// partout. Le masque permet d'en viser plusieurs (edition sommet ET arete).
		//
		// Pourquoi un masque et non une simple enumeration : en mode edition,
		// Blender distingue deja sommet / arete / face, et certaines commandes ne
		// valent que pour l'un d'eux. Un entier unique obligerait a dupliquer la
		// meme liaison trois fois — donc a la desynchroniser un jour.
		enum NkShortcutCtx : uint16 {
			NK_SCTX_GLOBAL = 0xFFFF, ///< partout
			NK_SCTX_OBJECT = 1 << 0, ///< mode objet
			NK_SCTX_EDIT = 1 << 1,	 ///< mode edition, tous sous-modes
			NK_SCTX_VERTEX = 1 << 2, ///< sous-mode sommet
			NK_SCTX_EDGE = 1 << 3,	 ///< sous-mode arete
			NK_SCTX_FACE = 1 << 4,	 ///< sous-mode face
			NK_SCTX_MODAL = 1 << 5,	 ///< operation modale en cours
			NK_SCTX_PANEL = 1 << 6,	 ///< focus dans un panneau
		};

		// Une LIAISON. `command` est une CLE STABLE, pas un libelle : elle est
		// serialisee dans le fichier de configuration et citee par l'interface.
		// La renommer casserait les configurations existantes — meme regle que les
		// noms de parametres de modificateurs.
		struct NKEDITORKIT_API NkShortcutBinding {
				char command[64] = {};				 ///< CLE stable, ex. "mesh.extrude"
				char label[64] = {};				 ///< libelle affichable, librement modifiable
				NkKey key = NkKey::NK_UNKNOWN;		 ///< touche principale
				uint8 mods = NK_SC_NONE;			 ///< masque NkShortcutMod
				uint16 context = NK_SCTX_GLOBAL;	 ///< masque NkShortcutCtx
				bool userDefined = false;			 ///< true si l'utilisateur l'a modifiee
		};

		class NKEDITORKIT_API NkShortcutTable {
			public:
				static const uint32 kMax = 256;

				void Clear() {
					mCount = 0;
					mConflicts = 0;
				}

				uint32 Count() const {
					return mCount;
				}

				const NkShortcutBinding *At(uint32 i) const {
					return (i < mCount) ? &mBind[i] : nullptr;
				}

				// Enregistre une liaison. Renvoie false si la table est pleine.
				// N'EMPECHE PAS un conflit : elle le COMPTE. Refuser silencieusement
				// la seconde liaison serait pire — la commande semblerait enregistree
				// et ne repondrait jamais. Mieux vaut qu'elle existe et que l'outil
				// dise qu'il y a un doublon.
				bool Bind(const char *command, const char *label, NkKey key, uint8 mods = NK_SC_NONE,
						  uint16 context = NK_SCTX_GLOBAL) {
					if (mCount >= kMax || !command)
						return false;
					if (FindConflict(key, mods, context, mCount) >= 0)
						mConflicts++;
					NkShortcutBinding &b = mBind[mCount++];
					Copy(b.command, sizeof(b.command), command);
					Copy(b.label, sizeof(b.label), label ? label : command);
					b.key = key;
					b.mods = mods;
					b.context = context;
					b.userDefined = false;
					return true;
				}

				// REAFFECTE la combinaison d'une commande existante (reconfiguration
				// utilisateur). Renvoie false si la commande est inconnue.
				bool Rebind(const char *command, NkKey key, uint8 mods, uint16 context) {
					const int32 i = IndexOf(command);
					if (i < 0)
						return false;
					mBind[(uint32)i].key = key;
					mBind[(uint32)i].mods = mods;
					mBind[(uint32)i].context = context;
					mBind[(uint32)i].userDefined = true;
					RecountConflicts();
					return true;
				}

				// LOOKUP : quelle commande pour cette combinaison, dans ce contexte ?
				// `ctx` est le contexte COURANT (un seul bit, ou plusieurs si l'etat
				// est composite) ; une liaison repond si son masque le recoupe.
				// Renvoie nullptr si aucune.
				//
				// Premiere trouvee gagne. C'est deliberement l'ordre d'ENREGISTREMENT
				// qui tranche : les liaisons les plus specifiques doivent donc etre
				// enregistrees en premier, exactement comme les `if` imbriques qu'elle
				// remplace testaient le mode edition avant le mode objet.
				const NkShortcutBinding *Lookup(NkKey key, uint8 mods, uint16 ctx) const {
					for (uint32 i = 0; i < mCount; ++i) {
						const NkShortcutBinding &b = mBind[i];
						if (b.key != key || b.mods != mods)
							continue;
						if ((b.context & ctx) == 0)
							continue;
						return &b;
					}
					return nullptr;
				}

				// Raccourci d'une commande, sous forme affichable : « Ctrl+Shift+B ».
				// C'est CE texte que l'interface montre — plus de chaine recopiee a la
				// main qui puisse mentir.
				bool FormatFor(const char *command, char *out, uint32 outSize) const {
					const int32 i = IndexOf(command);
					if (i < 0 || !out || outSize == 0)
						return false;
					return Format(mBind[(uint32)i], out, outSize);
				}

				static bool Format(const NkShortcutBinding &b, char *out, uint32 outSize) {
					if (!out || outSize == 0)
						return false;
					uint32 n = 0;
					auto put = [&](const char *s) {
						while (*s && n + 1 < outSize)
							out[n++] = *s++;
					};
					if (b.mods & NK_SC_CTRL)
						put("Ctrl+");
					if (b.mods & NK_SC_SHIFT)
						put("Shift+");
					if (b.mods & NK_SC_ALT)
						put("Alt+");
					put(KeyName(b.key));
					out[n < outSize ? n : outSize - 1] = 0;
					return true;
				}

				int32 IndexOf(const char *command) const {
					if (!command)
						return -1;
					for (uint32 i = 0; i < mCount; ++i)
						if (Equal(mBind[i].command, command))
							return (int32)i;
					return -1;
				}

				// Nombre de CONFLITS : deux liaisons repondant a la meme combinaison
				// dans des contextes qui se recoupent. Un editeur doit pouvoir
				// l'afficher — c'est precisement ce que personne ne voyait avant.
				uint32 ConflictCount() const {
					return mConflicts;
				}

				// Detaille le i-eme conflit (indices des deux liaisons en cause).
				bool ConflictAt(uint32 nth, uint32 &outA, uint32 &outB) const {
					uint32 seen = 0;
					for (uint32 i = 0; i < mCount; ++i) {
						const int32 j = FindConflict(mBind[i].key, mBind[i].mods, mBind[i].context, i);
						if (j < 0)
							continue;
						if (seen++ == nth) {
							outA = (uint32)j;
							outB = i;
							return true;
						}
					}
					return false;
				}

				// Nom affichable d'une touche. Table EXPLICITE plutot qu'un cast ou une
				// soustraction d'enum, pour deux raisons :
				//   • l'utilisateur cherche « [ », pas « NK_LBRACKET » ;
				//   • une correspondance calculee supposerait que NkKey est contigue et
				//     ordonnee comme l'ASCII, ce qui n'est garanti nulle part et casserait
				//     en silence a la premiere insertion de valeur.
				//
				// EN-TETE PUR (inline) et non dans un .cpp : la table doit etre utilisable
				// par un harnais de test sans lier NKEditorKit — donc sans trainer NKUI,
				// NKCanvas et NKFont pour verifier une structure de donnees.
				static const char *KeyName(NkKey k) {
			switch (k) {
				case NkKey::NK_A: return "A";
				case NkKey::NK_B: return "B";
				case NkKey::NK_C: return "C";
				case NkKey::NK_D: return "D";
				case NkKey::NK_E: return "E";
				case NkKey::NK_F: return "F";
				case NkKey::NK_G: return "G";
				case NkKey::NK_H: return "H";
				case NkKey::NK_I: return "I";
				case NkKey::NK_J: return "J";
				case NkKey::NK_K: return "K";
				case NkKey::NK_L: return "L";
				case NkKey::NK_M: return "M";
				case NkKey::NK_N: return "N";
				case NkKey::NK_O: return "O";
				case NkKey::NK_P: return "P";
				case NkKey::NK_Q: return "Q";
				case NkKey::NK_R: return "R";
				case NkKey::NK_S: return "S";
				case NkKey::NK_T: return "T";
				case NkKey::NK_U: return "U";
				case NkKey::NK_V: return "V";
				case NkKey::NK_W: return "W";
				case NkKey::NK_X: return "X";
				case NkKey::NK_Y: return "Y";
				case NkKey::NK_Z: return "Z";
				case NkKey::NK_NUM0: return "0";
				case NkKey::NK_NUM1: return "1";
				case NkKey::NK_NUM2: return "2";
				case NkKey::NK_NUM3: return "3";
				case NkKey::NK_NUM4: return "4";
				case NkKey::NK_NUM5: return "5";
				case NkKey::NK_NUM6: return "6";
				case NkKey::NK_NUM7: return "7";
				case NkKey::NK_NUM8: return "8";
				case NkKey::NK_NUM9: return "9";
				case NkKey::NK_NUMPAD_0: return "Pav.0";
				case NkKey::NK_NUMPAD_1: return "Pav.1";
				case NkKey::NK_NUMPAD_2: return "Pav.2";
				case NkKey::NK_NUMPAD_3: return "Pav.3";
				case NkKey::NK_NUMPAD_4: return "Pav.4";
				case NkKey::NK_NUMPAD_5: return "Pav.5";
				case NkKey::NK_NUMPAD_6: return "Pav.6";
				case NkKey::NK_NUMPAD_7: return "Pav.7";
				case NkKey::NK_NUMPAD_8: return "Pav.8";
				case NkKey::NK_NUMPAD_9: return "Pav.9";
				case NkKey::NK_F1: return "F1";
				case NkKey::NK_F2: return "F2";
				case NkKey::NK_F3: return "F3";
				case NkKey::NK_F4: return "F4";
				case NkKey::NK_F5: return "F5";
				case NkKey::NK_F6: return "F6";
				case NkKey::NK_F7: return "F7";
				case NkKey::NK_F8: return "F8";
				case NkKey::NK_F9: return "F9";
				case NkKey::NK_F10: return "F10";
				case NkKey::NK_F11: return "F11";
				case NkKey::NK_F12: return "F12";
				case NkKey::NK_TAB: return "Tab";
				case NkKey::NK_ESCAPE: return "Echap";
				case NkKey::NK_ENTER: return "Entree";
				case NkKey::NK_SPACE: return "Espace";
				case NkKey::NK_DELETE: return "Suppr";
				case NkKey::NK_UP: return "Haut";
				case NkKey::NK_DOWN: return "Bas";
				case NkKey::NK_LEFT: return "Gauche";
				case NkKey::NK_RIGHT: return "Droite";
				case NkKey::NK_PERIOD: return ".";
				case NkKey::NK_COMMA: return ",";
				case NkKey::NK_LBRACKET: return "[";
				case NkKey::NK_RBRACKET: return "]";
				case NkKey::NK_BACKSLASH: return "\\";
				case NkKey::NK_SEMICOLON: return ";";
				case NkKey::NK_SLASH: return "/";
				case NkKey::NK_MINUS: return "-";
				case NkKey::NK_EQUALS: return "=";
				default: return "?";
			}
				}

			private:
				static void Copy(char *dst, uint32 cap, const char *src) {
					uint32 i = 0;
					if (src)
						for (; src[i] && i + 1 < cap; ++i)
							dst[i] = src[i];
					dst[i] = 0;
				}

				static bool Equal(const char *a, const char *b) {
					while (*a && *b) {
						if (*a != *b)
							return false;
						++a;
						++b;
					}
					return *a == *b;
				}

				// Cherche une liaison ANTERIEURE (indice < upTo) qui repondrait a la
				// meme combinaison dans un contexte qui se recoupe.
				int32 FindConflict(NkKey key, uint8 mods, uint16 context, uint32 upTo) const {
					for (uint32 i = 0; i < upTo && i < mCount; ++i) {
						const NkShortcutBinding &b = mBind[i];
						if (b.key == key && b.mods == mods && (b.context & context) != 0)
							return (int32)i;
					}
					return -1;
				}

				void RecountConflicts() {
					mConflicts = 0;
					for (uint32 i = 0; i < mCount; ++i)
						if (FindConflict(mBind[i].key, mBind[i].mods, mBind[i].context, i) >= 0)
							mConflicts++;
				}

				NkShortcutBinding mBind[kMax];
				uint32 mCount = 0;
				uint32 mConflicts = 0;
		};

	} // namespace editorkit
} // namespace nkentseu
