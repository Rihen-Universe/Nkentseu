#pragma once
// =============================================================================
// NkCodeEditor.h — Widget code-editeur facon VSCode (immediate mode, sur NKGui).
//   Modele par LIGNES (source de verite pour l'edition) + etat curseur/selection/
//   scroll EMBARQUE dans le document -> survit au changement d'onglet ET au
//   re-dock (etat hors-frame). Fonctions : gouttiere + numeros de ligne, scroll
//   vertical/horizontal (molette + barres), caret, selection souris (clic-glisser)
//   et clavier (Shift+fleches), navigation (fleches/Home/End), edition (saisie,
//   Entree, Backspace, Delete), auto-scroll pour garder le caret visible.
//
//   Limites v1 : pas de presse-papiers (NKWindow n'expose pas encore de clipboard),
//   pas de PageUp/Down (touches absentes de NkGuiKey), largeur H approximee sur les
//   lignes visibles. Coloration syntaxique = phase suivante.
// =============================================================================
#include "NKGui/NKGui.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCode/Editor/NkSyntax.h"
#include "NKCode/Editor/NkTextDraw.h"
#include "NKCode/Project/NkText.h"	  // NkPPEvalActive / NkDefineValue (régions préproc inactives)
#include "NKCode/Project/NkLogSink.h" // GlobalLogBuffer : traces [ac] (panneau OUTPUT)

#include <cstdio> // snprintf (numeros de ligne)

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu;
		using namespace nkentseu::nkgui;

		// Une ligne = caracteres bruts (PAS de '\0'). Mesure/dessin via plage [begin,end).
		using NkCodeLine = NkVector<char>;

		// Document editable : modele par lignes + etat d'edition. Place dans OpenFile
		// (persistant) afin que curseur/selection/scroll ne soient jamais perdus.
		struct NkCodeDoc {
				NkVector<NkCodeLine> lines;
				int32 curLine = 0, curCol = 0; // curseur (col = index caractere)
				int32 selLine = 0, selCol = 0; // ancre de selection (== curseur si vide)
				int32 lineSelAnchor = -1;	   // ancre de la sélection-ligne Ctrl+L (pour étendre au répété)
				// ── Multi-clic (double = MOT, triple et + = LIGNE) ──
				int32 clkN = 0, clkTick = -1000, clkL = -1, clkC = -1;
				int32 tick = 0; // compteur de frames (fenêtre de détection du multi-clic)
				float32 scrollX = 0.f, scrollY = 0.f;
				bool dirty = false;
				int64 savedSig = 0;		 // hash du texte au dernier enregistrement : undo/redo eteignent le point
				bool wantReveal = false; // demande de révélation du curseur (clic Outline -> défile vers la ligne)
				// ── Ctrl+clic (navigation façon VSCode) : rempli par l'éditeur, consommé par NkCodeState ──
				NkString linkTarget;		 // symbole (go-to-def) ou chemin d'include à ouvrir ; vidé après traitement
				bool linkIsInclude = false;	 // true = chemin d'#include ; false = symbole (définition)
				NkVector<int32> breakpoints; // lignes (0-based) avec point d'arrêt (gouttière)

				bool HasBreakpoint(int32 l) const {
					for (usize i = 0; i < breakpoints.Size(); ++i)
						if (breakpoints[i] == l)
							return true;
					return false;
				}

				void ToggleBreakpoint(int32 l) {
					for (usize i = 0; i < breakpoints.Size(); ++i)
						if (breakpoints[i] == l) {
							breakpoints.Erase(breakpoints.Begin() + i);
							return;
						}
					breakpoints.PushBack(l);
				}

				// Indicateurs Git par ligne (bande gouttière) : 0 aucun, 1 ajoutée, 2 modifiée.
				// Rempli par NkCodeState::RefreshGit (à l'ouverture + à la sauvegarde). `gitDeleted`
				// = index de ligne AU-DESSUS de laquelle des lignes ont été supprimées.
				NkVector<uint8> gitStatus;
				NkVector<int32> gitDeleted;

				uint8 GitAt(int32 l) const {
					return (l >= 0 && l < static_cast<int32>(gitStatus.Size())) ? gitStatus[l] : 0;
				}

				bool GitDelAt(int32 l) const {
					for (usize i = 0; i < gitDeleted.Size(); ++i)
						if (gitDeleted[i] == l)
							return true;
					return false;
				}

				// ── Undo / Redo PAR FICHIER (snapshots texte + curseur, coalescés par type) ──
				struct UndoState {
						NkString text;
						int32 cl;
						int32 cc;
				};

				NkVector<UndoState> undoU, redoU;
				int32 lastEdit = 0; // 0 aucun, 1 saisie, 2 suppression, 3 autre (pour coalescer)
				// ── Recherche / Remplacement (Ctrl+F / Ctrl+H) ──
				bool findOpen = false;	  // barre de recherche affichée
				bool findReplace = false; // mode remplacement (champ « Remplacer » en plus)
				bool findCase = false;	  // sensible à la casse
				int32 findFocus = 0;	  // champ actif : 0 = Rechercher, 1 = Remplacer
				char findQuery[256] = {},
					 findRepl[256] = {};		   // termes recherché / remplacement (buffers -> NkOverlayTextField)
				NkVector<int32> findLine, findCol; // occurrences (arrays parallèles ligne/colonne)
				int32 findCur = -1;				   // occurrence courante (surlignée)
				int64 findSig = -1;				   // signature (contenu+query+casse) -> invalide le cache
				// ── Index de symboles du fichier (coloration SÉMANTIQUE fiable) ──
				NkVector<NkString> symTypes, symFuncs; // triés -> recherche binaire (NkSymHas)
				NkVector<NkString> symWords; // TOUS les identifiants du document (complétion par mots façon VSCode)
				int64 symSig = -1;			 // signature du contenu (rebuild seulement si changé)
				// ── Autocomplétion (popup façon VSCode) ──
				bool acOpen = false;				  // popup affiché
				NkVector<NkString> acItems;			  // candidats filtrés (préfixe courant)
				int32 acSel = 0;					  // item sélectionné
				int32 acWordCol = 0;				  // colonne de début du mot en cours (pour le remplacer)
				int32 acTop = 0;					  // 1er element visible (defilement du popup)
				float32 acXOff = 0.f;				  // defilement horizontal du popup
				NkRect acRect = {0.f, 0.f, 0.f, 0.f}; // rect du popup (frame precedente) -> routage des clics
				// ── Hover documentation (survol ~0,5 s d un symbole -> carte signature + doc) ──
				int32 hovWordL = -1, hovWordS = -1, hovWordE = -1; // mot sous la souris (detection du survol)
				float32 hovDwell = 0.f;							   // duree d immobilite sur ce mot
				bool hovReq = false;					  // requete posee (consommee par NkCodeState::ProcessHover)
				bool hovDone = false;					  // deja resolu pour CE mot (anti re-requete chaque frame)
				bool hovKb = false;						  // requete posee au CLAVIER (Ctrl+K I) : resets souris ignores
				int32 hovKind = 0;						  // pastille : 1 fonction/membre, 2 type, 3 variable, 4 macro
				NkRect hovActRect = {0.f, 0.f, 0.f, 0.f}; // action [Aller a la definition]
				NkRect hovCopyRect = {0.f, 0.f, 0.f, 0.f};	 // bouton [Copier]
				NkRect hovRefsRect = {0.f, 0.f, 0.f, 0.f};	 // bouton [References]
				float32 hovXOff = 0.f;						 // defilement horizontal du PROTOTYPE (long)
				NkRect hovTitleRect = {0.f, 0.f, 0.f, 0.f};	 // zone du prototype (glisser = defiler)
				float32 hovDragX = 0.f, hovDragOff = 0.f;	 // ancrage du glisser horizontal
				float32 hovBodyOff = 0.f, hovBodyXOff = 0.f; // defilement V/H de la DOC (commentaires)
				NkRect hovBodyRect = {0.f, 0.f, 0.f, 0.f};	 // zone doc (molette / glisser 2 axes)
				int32 hovDragMode = 0;						 // 1 = prototype (X), 2 = doc (X+Y)
				float32 hovDragY = 0.f, hovDragOffY = 0.f;
				// ── QoL : barre « aller a la ligne » (Ctrl+G), chord Ctrl+K, references ──
				bool gotoOpen = false;
				char gotoBuf[12] = {};
				int32 blinkTick = 0; // clignotement du caret : rallume a chaque frappe/deplacement
				int32 blinkL = -1, blinkC = -1;
				// Quick fix (Ctrl+.) : actions deduites des diagnostics de la ligne du caret.
				NkVector<NkString> qfLabels; // libelles du menu
				NkVector<int32> qfL, qfC;	 // position d'application
				NkVector<uint8> qfKind;		 // 0 = inserer ';' ; 1 = remplacer le mot par qfPay
				NkVector<NkString> qfPay;
				int32 chordK = -100000;				   // tick du dernier Ctrl+K (chords Ctrl+K 0/J/I)
				NkString refsTarget;				   // symbole dont on veut les REFERENCES (consomme par l etat)
				NkString hovSym;					   // symbole demande
				int32 hovLine = -1, hovCol = -1;	   // position du mot (resolution contextuelle)
				bool hovShow = false;				   // carte visible
				NkString hovTitle;					   // signature inferee
				NkVector<NkString> hovBody;			   // lignes de documentation (commentaires au-dessus de la def)
				float32 hovX = 0.f, hovY = 0.f;		   // ancre px de la carte (coin bas-gauche du mot)
				NkRect hovRect = {0.f, 0.f, 0.f, 0.f}; // rect de la carte (la souris peut venir DESSUS)
				// ── Autocomplétion CONTEXTUELLE (membres après '.', '->', '::') — compile-first :
				//    l'éditeur POSE la requête (position du point), NkCodeState lance le compilateur
				//    (`-Xclang -code-completion-at`, flags .jcdb) et remplit acCtxAll ; la frappe
				//    re-filtre EN LIVE depuis cette liste (pas de nouvel appel compilo par lettre). ──
				bool acCtxReq = false; // demande posée (consommée par NkCodeState::ProcessCompletionRequest)
				int32 acCtxLine = -1;  // position du point de complétion (juste après le déclencheur)
				int32 acCtxCol = -1;
				NkVector<NkString> acCtxAll; // résultats COMPLETS du compilateur (re-filtrés à la frappe)

				// Remplace le mot [acWordCol, curCol) par l'item choisi (réutilise Backspace/InsertChar).
				void AcceptAutocomplete() {
					if (!acOpen || acItems.Empty())
						return;
					const int32 sel = (acSel >= 0 && acSel < static_cast<int32>(acItems.Size())) ? acSel : 0;
					const NkString item = acItems[static_cast<usize>(sel)];
					Checkpoint(3);
					for (int32 del = curCol - acWordCol; del > 0; --del)
						Backspace();
					for (int32 k = 0; k < static_cast<int32>(item.Size()); ++k)
						InsertChar(item.CStr()[k]);
					acOpen = false;
					acCtxAll.Clear(); // fin du mode contextuel (membre inséré)
					acTop = 0;
					acXOff = 0.f;
				}

				// ── Diagnostics (erreurs/avertissements du compilateur) ──
				struct Diag {
						int32 line;
						int32 col;
						int32 endCol;
						uint8 sev;
						NkString msg;
				}; // sev: 0 warning, 1 error

				NkVector<Diag> diags;

				bool HasDiagOn(int32 l) const {
					for (usize i = 0; i < diags.Size(); ++i)
						if (diags[i].line == l)
							return true;
					return false;
				}

				int32 DiagSevOn(int32 l) const {
					int32 s = -1;
					for (usize i = 0; i < diags.Size(); ++i)
						if (diags[i].line == l) {
							if (diags[i].sev > (uint8)(s < 0 ? 0 : s))
								s = diags[i].sev;
							else if (s < 0)
								s = diags[i].sev;
						}
					return s;
				} // -1 aucun, 0 warning, 1 erreur

				// ── Régions préprocesseur inactives (#if/#ifdef non satisfait) -> atténuées ──
				NkVector<uint8> inactive; // par ligne : 1 = branche morte (grisée façon VSCode)
				int64 ppSig = -1;		  // signature (contenu ^ defines) -> recalcul paresseux

				bool InactiveAt(int32 l) const {
					return l >= 0 && l < static_cast<int32>(inactive.Size()) && inactive[l] != 0;
				}

				// Évalue toutes les branches #if/#ifdef/#ifndef/#elif/#else/#endif du fichier avec
				// les defines du PROJET (issus du .jcdb). Une ligne de code dans une branche non
				// prise est marquée inactive ; les directives ne sont grisées que si leur contexte
				// ENGLOBANT est déjà inactif (comme VSCode). `symSig` doit être à jour (EnsureSymbols).
				void EnsurePreproc(NkLang lang, const NkVector<NkString> *defs) {
					int64 h = static_cast<int64>(static_cast<int32>(lang) + 1) * 1099511628211LL;
					if (defs)
						for (usize i = 0; i < defs->Size(); ++i) {
							const char *s = (*defs)[i].CStr();
							while (*s) {
								h = (h ^ (unsigned char)*s) * 1099511628211LL;
								++s;
							}
						}
					const int64 sig = symSig ^ h;
					if (sig == ppSig)
						return;
					ppSig = sig;
					inactive.Clear();
					inactive.Resize(LineCount(), 0);
					if (lang != NkLang::C || !defs)
						return; // .c/.cpp/.h -> NkLang::C ; sinon rien
					// ── 1) Macros DÉFINIES PAR CE FICHIER (include-guard `#ifndef X/#define X` inclus).
					//    Elles ne comptent PAS avant leur `#define` -> on les retire de l'ensemble effectif
					//    (issu du compilateur = état de FIN) et on les (re)définit à leur position. ──
					NkVector<NkString> fileDef;
					auto readName = [](const char *s, NkString &out) {
						int32 L2 = 0;
						while (NkPPIsIdc(s[L2]))
							++L2;
						out = NkString();
						for (int32 t = 0; t < L2; ++t)
							out += s[t];
						return L2;
					};
					for (int32 i = 0; i < LineCount(); ++i) {
						const NkCodeLine &L = lines[i];
						const char *d = L.Data();
						const int32 n = static_cast<int32>(L.Size());
						int32 j = 0;
						while (j < n && (d[j] == ' ' || d[j] == '\t'))
							++j;
						if (j >= n || d[j] != '#')
							continue;
						++j;
						while (j < n && (d[j] == ' ' || d[j] == '\t'))
							++j;
						if (n - j >= 6 && d[j] == 'd' && d[j + 1] == 'e' && d[j + 2] == 'f' && d[j + 3] == 'i' &&
							d[j + 4] == 'n' && d[j + 5] == 'e') {
							int32 k = j + 6;
							while (k < n && (d[k] == ' ' || d[k] == '\t'))
								++k;
							NkString nm;
							if (readName(d + k, nm) > 0 && (int32)nm.Size() <= n - k)
								fileDef.PushBack(nm);
						}
					}
					auto nameOf = [](const NkString &e) {
						NkString o;
						for (const char *p = e.CStr(); *p && *p != '='; ++p)
							o += *p;
						return o;
					};
					auto streq = [](const char *a, const char *b) {
						while (*a && *a == *b) {
							++a;
							++b;
						}
						return *a == *b;
					};
					auto inFileDef = [&](const NkString &nm) {
						for (usize x = 0; x < fileDef.Size(); ++x)
							if (streq(fileDef[x].CStr(), nm.CStr()))
								return true;
						return false;
					};
					// ── 2) Base = effective SANS les macros locales ──
					NkVector<NkString> adj;
					for (usize x = 0; x < defs->Size(); ++x) {
						if (!inFileDef(nameOf((*defs)[x])))
							adj.PushBack((*defs)[x]);
					}
					auto adjHas = [&](const char *name, int32 len) {
						for (usize x = 0; x < adj.Size(); ++x) {
							const char *e = adj[x].CStr();
							int32 t = 0;
							while (t < len && e[t] && e[t] != '=' && e[t] == name[t])
								++t;
							if (t == len && (e[t] == '\0' || e[t] == '='))
								return (int32)x;
						}
						return -1;
					};
					auto adjAdd = [&](const char *name, int32 len) {
						if (adjHas(name, len) < 0) {
							NkString nm;
							for (int32 t = 0; t < len; ++t)
								nm += name[t];
							adj.PushBack(nm);
						}
					};
					auto adjDel = [&](const char *name, int32 len) {
						const int32 x = adjHas(name, len);
						if (x >= 0)
							adj.Erase(adj.Begin() + x);
					};

					// ── 3) Scan des directives (mutations #define/#undef à leur position, si branche active) ──
					struct PF {
							bool outer;
							bool taken;
							bool active;
					};

					NkVector<PF> st;
					char buf[4096];
					for (int32 i = 0; i < LineCount(); ++i) {
						const bool emit = st.Empty() ? true : st.Back().active;
						const NkCodeLine &L = lines[i];
						const char *d = L.Data();
						const int32 n = static_cast<int32>(L.Size());
						int32 j = 0;
						while (j < n && (d[j] == ' ' || d[j] == '\t'))
							++j;
						if (j >= n || d[j] != '#') {
							inactive[i] = emit ? 0 : 1;
							continue;
						}
						++j;
						while (j < n && (d[j] == ' ' || d[j] == '\t'))
							++j;
						const int32 w0 = j;
						while (j < n && d[j] >= 'a' && d[j] <= 'z')
							++j;
						const int32 wl = j - w0;
						auto isW = [&](const char *lit) {
							int32 k = 0;
							while (lit[k]) {
								if (k >= wl || d[w0 + k] != lit[k])
									return false;
								++k;
							}
							return k == wl;
						};
						while (j < n && (d[j] == ' ' || d[j] == '\t'))
							++j;
						int32 a = 0;
						for (int32 k = j; k < n && a < 4094; ++k) {
							if (d[k] == '/' && k + 1 < n && d[k + 1] == '/')
								break;
							buf[a++] = d[k];
						}
						buf[a] = 0;
						if (isW("if") || isW("ifdef") || isW("ifndef")) {
							bool cond;
							if (isW("if"))
								cond = NkPPEvalActive(buf, adj);
							else {
								const char *s = buf;
								int32 L2 = 0;
								while (NkPPIsIdc(s[L2]))
									++L2;
								bool f = false;
								NkDefineValue(adj, s, L2, &f);
								cond = isW("ifdef") ? f : !f;
							}
							inactive[i] = emit ? 0 : 1;
							st.PushBack(PF{emit, emit && cond, emit && cond});
						} else if (isW("elif")) {
							if (!st.Empty()) {
								PF &f = st.Back();
								const bool wasTaken = f.taken;
								const bool cond = wasTaken ? false : NkPPEvalActive(buf, adj);
								f.active = f.outer && !wasTaken && cond;
								if (f.active)
									f.taken = true;
								inactive[i] = f.outer ? 0 : 1;
							} else
								inactive[i] = emit ? 0 : 1;
						} else if (isW("else")) {
							if (!st.Empty()) {
								PF &f = st.Back();
								f.active = f.outer && !f.taken;
								f.taken = true;
								inactive[i] = f.outer ? 0 : 1;
							} else
								inactive[i] = emit ? 0 : 1;
						} else if (isW("endif")) {
							if (!st.Empty()) {
								const bool outer = st.Back().outer;
								inactive[i] = outer ? 0 : 1;
								st.Erase(st.End() - 1);
							} else
								inactive[i] = 0;
						} else {
							if (emit) { // #define/#undef ACTIFS -> mettent à jour l'ensemble à leur position
								if (isW("define")) {
									const char *s = buf;
									int32 L2 = 0;
									while (NkPPIsIdc(s[L2]))
										++L2;
									if (L2 > 0)
										adjAdd(s, L2);
								} else if (isW("undef")) {
									const char *s = buf;
									int32 L2 = 0;
									while (NkPPIsIdc(s[L2]))
										++L2;
									if (L2 > 0)
										adjDel(s, L2);
								}
							}
							inactive[i] = emit ? 0 : 1; // #include/#define/#pragma... suit l'état courant
						}
					}
				}

				// A appeler AVANT une mutation. Coalesce les runs de saisie/suppression.
				void Checkpoint(int32 kind) {
					if (kind != 0 && kind == lastEdit && (kind == 1 || kind == 2))
						return; // même run -> pas de nouveau snapshot
					if (undoU.Size() > 400)
						undoU.Erase(undoU.Begin());
					undoU.PushBack({GetText(), curLine, curCol});
					redoU.Clear();
					lastEdit = kind;
				}

				void ResetEditRun() {
					lastEdit = 0;
					lineSelAnchor = -1;
					acOpen = false;
					extraCarets.Clear();
				} // déplacement curseur : nouveau groupe undo + reset sélection-ligne + ferme l'autocomplétion + réduit
				  // à 1 curseur

				void Undo() {
					if (undoU.Empty())
						return;
					redoU.PushBack({GetText(), curLine, curCol});
					const UndoState s = undoU[undoU.Size() - 1];
					undoU.Erase(undoU.End() - 1);
					const float32 sx = scrollX, sy = scrollY; // SetText remet le scroll à 0 -> on le garde
					SetText(s.text.CStr());
					scrollX = sx;
					scrollY = sy;
					curLine = s.cl;
					curCol = s.cc;
					ClampCursor();
					Collapse();
					lastEdit = 0;
					dirty = (SymSig() != savedSig); // revenu a l'etat sauvegarde -> le point s'eteint
					widthDirty = true;
				}

				void Redo() {
					if (redoU.Empty())
						return;
					undoU.PushBack({GetText(), curLine, curCol});
					const UndoState s = redoU[redoU.Size() - 1];
					redoU.Erase(redoU.End() - 1);
					const float32 sx = scrollX, sy = scrollY; // SetText remet le scroll à 0 -> on le garde
					SetText(s.text.CStr());
					scrollX = sx;
					scrollY = sy;
					curLine = s.cl;
					curCol = s.cc;
					ClampCursor();
					Collapse();
					lastEdit = 0;
					dirty = (SymSig() != savedSig); // revenu a l'etat sauvegarde -> le point s'eteint
					widthDirty = true;
				}

				// ── Recherche / Remplacement ──────────────────────────────────────────────
				static char FLow(char c) {
					return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
				}

				static int32 FLen(const char *s) {
					int32 n = 0;
					while (s[n])
						++n;
					return n;
				}

				int64 FindSigOf() const {
					int64 h = SymSig(); // contenu du document
					for (const char *p = findQuery; *p; ++p)
						h = (h ^ static_cast<unsigned char>(*p)) * 1099511628211LL;
					return (h ^ (findCase ? 1 : 2)) * 1099511628211LL;
				}

				int32 FindCount() const {
					return static_cast<int32>(findLine.Size());
				}

				void FindRecompute() {
					findLine.Clear();
					findCol.Clear();
					const int32 qn = FLen(findQuery);
					if (qn > 0)
						for (int32 l = 0; l < LineCount(); ++l) {
							const NkCodeLine &L = lines[l];
							const int32 n = static_cast<int32>(L.Size());
							for (int32 c = 0; c + qn <= n; ++c) {
								bool ok = true;
								for (int32 k = 0; k < qn; ++k) {
									char a = L[c + k], b = findQuery[k];
									if (!findCase) {
										a = FLow(a);
										b = FLow(b);
									}
									if (a != b) {
										ok = false;
										break;
									}
								}
								if (ok) {
									findLine.PushBack(l);
									findCol.PushBack(c);
								}
							}
						}
					findSig = FindSigOf();
					if (findCur >= FindCount())
						findCur = FindCount() > 0 ? 0 : -1;
				}

				void FindEnsure() {
					if (findSig != FindSigOf())
						FindRecompute();
				}

				void
				FindGoto(int32 i) { // place curseur + sélection sur l'occurrence (surlignage + révélation réutilisés)
					if (i < 0 || i >= FindCount())
						return;
					findCur = i;
					const int32 qn = FLen(findQuery);
					selLine = findLine[i];
					selCol = findCol[i];
					curLine = findLine[i];
					curCol = findCol[i] + qn;
					ClampCursor();
					wantReveal = true;
				}

				void FindNext(bool fwd) {
					FindEnsure();
					const int32 n = FindCount();
					if (n == 0) {
						findCur = -1;
						return;
					}
					int32 i = findCur;
					if (i < 0) { // pas d'occ courante -> la 1ère >= curseur (ou la dernière < curseur si recul)
						i = 0;
						for (int32 k = 0; k < n; ++k)
							if (findLine[k] > curLine || (findLine[k] == curLine && findCol[k] >= curCol)) {
								i = k;
								break;
							}
						if (!fwd)
							i = (i - 1 + n) % n;
					} else
						i = fwd ? (i + 1) % n : (i - 1 + n) % n;
					FindGoto(i);
				}

				void ReplaceCurrent() {
					FindEnsure();
					if (findCur < 0 || findCur >= FindCount()) {
						FindNext(true);
						return;
					}
					Checkpoint(3);
					const int32 l = findLine[findCur], c = findCol[findCur], qn = FLen(findQuery);
					selLine = l;
					selCol = c;
					curLine = l;
					curCol = c + qn;
					EraseSelection();
					InsertText(findRepl);
					FindRecompute();
					FindNext(true);
				}

				void ReplaceAll() {
					FindEnsure();
					if (FindCount() == 0)
						return;
					Checkpoint(3);
					for (int32 i = FindCount() - 1; i >= 0; --i) { // de la FIN vers le DÉBUT : préserve les positions
						const int32 l = findLine[i], c = findCol[i], qn = FLen(findQuery);
						selLine = l;
						selCol = c;
						curLine = l;
						curCol = c + qn;
						EraseSelection();
						InsertText(findRepl);
					}
					FindRecompute();
					findCur = -1;
					Collapse();
				}

				void FindClose() {
					findOpen = false;
					findCur = -1;
					findLine.Clear();
					findCol.Clear();
					findSig = -1;
				}

				// ── Multi-curseur (Ctrl+D façon VSCode) ───────────────────────────────────
				// Curseurs SECONDAIRES (le primaire reste curLine/curCol/sel). Rangés du haut
				// vers le bas ; les éditions s'appliquent à TOUS, du BAS vers le HAUT (préserve
				// les positions). Un mouvement de curseur / clic / Échap réduit à un seul curseur.
				struct Caret {
						int32 sl, sc, cl, cc;
				};

				NkVector<Caret> extraCarets;

				bool McActive() const {
					return !extraCarets.Empty();
				}

				void McClear() {
					extraCarets.Clear();
				}

				static bool McIsW(char c) {
					return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
				}

				// Applique `edit` (opère sur le primaire) à chaque curseur, bas -> haut.
				template <class F> void McForEach(F edit) {
					const Caret prim = {selLine, selCol, curLine, curCol};
					for (int32 i = static_cast<int32>(extraCarets.Size()) - 1; i >= 0; --i) {
						Caret &c = extraCarets[i];
						selLine = c.sl;
						selCol = c.sc;
						curLine = c.cl;
						curCol = c.cc;
						ClampCursor();
						edit();
						c.sl = selLine;
						c.sc = selCol;
						c.cl = curLine;
						c.cc = curCol;
					}
					selLine = prim.sl;
					selCol = prim.sc;
					curLine = prim.cl;
					curCol = prim.cc;
					ClampCursor();
					edit();
				}

				// Ctrl+D : 1er appui = sélectionne le mot sous le curseur ; suivants = ajoute la
				// prochaine occurrence du texte sélectionné (MONO-ligne) comme curseur additionnel.
				// Ctrl+Maj+L : selectionne TOUTES les occurrences du mot sous le caret
				// (multi-curseur) — le caret principal reste sur l occurrence courante.
				void SelectAllOccurrences() {
					if (curLine >= LineCount())
						return;
					const NkCodeLine &L0 = lines[curLine];
					int32 ws = curCol, we = curCol;
					while (ws > 0 && IsWChar(L0[ws - 1]))
						--ws;
					while (we < static_cast<int32>(L0.Size()) && IsWChar(L0[we]))
						++we;
					if (we <= ws)
						return;
					char w[96];
					int32 wn = 0;
					for (int32 k = ws; k < we && wn < 95; ++k)
						w[wn++] = L0[k];
					w[wn] = 0;
					extraCarets.Clear();
					selLine = curLine;
					selCol = ws;
					curCol = we;
					for (int32 l = 0; l < LineCount(); ++l) {
						const NkCodeLine &L = lines[l];
						const int32 n = static_cast<int32>(L.Size());
						for (int32 i = 0; i + wn <= n; ++i) {
							if (i > 0 && IsWChar(L[i - 1]))
								continue;
							int32 k = 0;
							while (k < wn && L[i + k] == w[k])
								++k;
							if (k != wn || (i + wn < n && IsWChar(L[i + wn])))
								continue;
							if (l == curLine && i == ws) {
								i += wn - 1;
								continue; // le principal
							}
							if (extraCarets.Size() < 256)
								extraCarets.PushBack({l, i, l, i + wn});
							i += wn - 1;
						}
					}
				}

				void SelectWordOrAddNext() {
					if (!HasSel()) {
						const NkCodeLine &L = lines[curLine];
						int32 a = curCol, b = curCol;
						while (a > 0 && McIsW(L[a - 1]))
							--a;
						while (b < static_cast<int32>(L.Size()) && McIsW(L[b]))
							++b;
						if (b > a) {
							selLine = curLine;
							selCol = a;
							curCol = b;
						} // curLine inchangé
						return;
					}
					int32 aL, aC, bL, bC;
					SelRange(aL, aC, bL, bC);
					if (aL != bL)
						return; // sélection multi-ligne -> pas d'ajout
					const int32 wn = bC - aC;
					if (wn <= 0)
						return;
					char needle[256];
					if (wn >= 256)
						return;
					for (int32 k = 0; k < wn; ++k)
						needle[k] = lines[aL][aC + k];
					// point de départ = APRÈS le dernier curseur (le plus bas)
					int32 fromL = bL, fromC = bC;
					if (!extraCarets.Empty()) {
						fromL = extraCarets.Back().cl;
						fromC = extraCarets.Back().cc;
					}
					auto matchAt = [&](int32 l, int32 c) -> bool {
						const NkCodeLine &L = lines[l];
						if (c + wn > static_cast<int32>(L.Size()))
							return false;
						for (int32 k = 0; k < wn; ++k)
							if (L[c + k] != needle[k])
								return false;
						return true;
					};
					const int32 nLines = LineCount();
					for (int32 step = 0; step <= nLines; ++step) { // balayage avec bouclage
						int32 l = fromL, cStart = (step == 0) ? fromC : 0;
						if (step > 0)
							l = (fromL + step) % nLines;
						const int32 n = static_cast<int32>(lines[l].Size());
						for (int32 c = cStart; c + wn <= n; ++c) {
							if (!matchAt(l, c))
								continue;
							if (l == aL && c == aC)
								continue; // saute la sélection primaire
							bool dup = false;
							for (usize e = 0; e < extraCarets.Size(); ++e)
								if (extraCarets[e].cl == l && extraCarets[e].sc == c) {
									dup = true;
									break;
								}
							if (dup)
								continue;
							extraCarets.PushBack({l, c, l, c + wn});
							wantReveal = true;
							return;
						}
					}
				}

				// Éditions « multi » : appliquent à tous les curseurs si actif, sinon au seul primaire.
				void McType(char c) {
					if (McActive())
						McForEach([&] { InsertChar(c); });
					else
						InsertChar(c);
				}

				void McBackspace() {
					if (McActive())
						McForEach([&] { Backspace(); });
					else
						Backspace();
				}

				void McDeleteFwd() {
					if (McActive())
						McForEach([&] { DeleteFwd(); });
					else
						DeleteFwd();
				}

				void McNewline() {
					if (McActive())
						McForEach([&] { InsertNewline(); });
					else
						InsertNewline();
				}

				void McInsertText(const char *s) {
					if (McActive())
						McForEach([&] { InsertText(s); });
					else
						InsertText(s);
				}

				// ── Index de symboles (coloration sémantique) : rebuild paresseux si le contenu change ──
				static int32 SymCmp(const char *a, const char *b) {
					while (*a && *a == *b) {
						++a;
						++b;
					}
					return (int32)(unsigned char)*a - (int32)(unsigned char)*b;
				}

				static bool IsWChar(char c) {
					return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
				}

				int64 SymSig() const {
					int64 h = static_cast<int64>(1469598103934665603ULL);
					for (int32 l = 0; l < LineCount(); ++l) {
						const NkCodeLine &L = lines[l];
						for (usize k = 0; k < L.Size(); ++k)
							h = (h ^ (unsigned char)L[k]) * 1099511628211LL;
						h = (h ^ 10) * 1099511628211LL;
					}
					return h;
				}

				void EnsureSymbols(NkLang lang) {
					const int64 s = SymSig();
					if (s == symSig)
						return;
					symSig = s;
					RebuildSymbols(lang);
				}

				// ── Repli de code (folding) : régions { } multi-lignes ──
				// Modèle : régions pliables détectées depuis les accolades (foldHdr/foldEnd, parallèles).
				// `foldOn` = en-têtes actuellement REPLIÉS. Le rendu ne mappe plus ligne->y linéairement :
				// `rowOfLine[i]` = row visuel de la ligne i (-1 si masquée), `visRows[r]` = ligne au row r.
				NkVector<int32> foldHdr, foldEnd; // régions pliables [hdr, end] (hdr < end)
				NkVector<int32> foldOn;			  // en-têtes repliés (sous-ensemble de foldHdr)
				int64 foldSig = -1;				  // signature contenu -> recompute des régions
				bool foldDirty = true;			  // recompute du mapping lignes visibles
				NkVector<int32> visRows;		  // row visuel -> ligne doc
				NkVector<int32> rowOfLine;		  // ligne doc -> row visuel (-1 masquée)

				int32 FoldEndOf(int32 line) const {
					for (usize k = 0; k < foldHdr.Size(); ++k)
						if (foldHdr[k] == line)
							return foldEnd[k];
					return -1;
				}

				bool IsFoldHeader(int32 line) const {
					return FoldEndOf(line) >= 0;
				}

				bool FoldedAt(int32 line) const {
					for (usize k = 0; k < foldOn.Size(); ++k)
						if (foldOn[k] == line)
							return true;
					return false;
				}

				void SetFold(int32 header, bool on) {
					if (FoldEndOf(header) < 0)
						return; // pas une en-tête pliable
					if (on == FoldedAt(header))
						return;
					if (on)
						foldOn.PushBack(header);
					else
						for (usize k = 0; k < foldOn.Size(); ++k)
							if (foldOn[k] == header) {
								foldOn.Erase(foldOn.Begin() + k);
								break;
							}
					foldDirty = true;
				}

				void FoldAll() { // replie TOUTES les regions (Ctrl+K Ctrl+0)
					foldOn.Clear();
					for (usize k = 0; k < foldHdr.Size(); ++k)
						foldOn.PushBack(foldHdr[k]);
					foldDirty = true;
				}

				void UnfoldAll() { // deplie tout (Ctrl+K Ctrl+J)
					foldOn.Clear();
					foldDirty = true;
				}

				void ToggleFold(int32 header) {
					if (FoldEndOf(header) >= 0)
						SetFold(header, !FoldedAt(header));
				}

				// En-tête de la région pliable la plus INTÉRIEURE contenant `line` (hdr <= line <= end).
				int32 EnclosingFoldHeader(int32 line) const {
					int32 best = -1;
					for (usize k = 0; k < foldHdr.Size(); ++k)
						if (foldHdr[k] <= line && line <= foldEnd[k] && foldHdr[k] > best)
							best = foldHdr[k];
					return best;
				}

				bool LineHidden(int32 line) const {
					return (line >= 0 && line < static_cast<int32>(rowOfLine.Size())) ? rowOfLine[line] < 0 : false;
				}

				int32 RowOf(int32 line) const {
					return (line >= 0 && line < static_cast<int32>(rowOfLine.Size())) ? rowOfLine[line] : line;
				}

				int32 VisRowCount() const {
					return static_cast<int32>(visRows.Size());
				}

				// Ligne doc au row visuel `r` (bornée). Identité si aucun repli.
				int32 LineAtRow(int32 r) const {
					if (visRows.Empty())
						return r < 0 ? 0 : (r >= LineCount() ? LineCount() - 1 : r);
					if (r < 0)
						r = 0;
					if (r >= static_cast<int32>(visRows.Size()))
						r = static_cast<int32>(visRows.Size()) - 1;
					return visRows[r];
				}

				// Première ligne VISIBLE à `line` ou au-dessus (pour ne jamais laisser le caret masqué).
				int32 NearestVisibleUp(int32 line) const {
					if (line < 0)
						return 0;
					if (line >= LineCount())
						line = LineCount() - 1;
					while (line > 0 && LineHidden(line))
						--line;
					return line;
				}

				// Détecte les régions pliables via les accolades (en ignorant chaînes/commentaires).
				void ComputeFoldRegions(NkLang lang) {
					foldHdr.Clear();
					foldEnd.Clear();
					if (lang != NkLang::C && lang != NkLang::NKSL) { // accolades uniquement (pas Python/MD)
						for (int32 k = static_cast<int32>(foldOn.Size()) - 1; k >= 0; --k)
							foldOn.Erase(foldOn.Begin() + k);
						return;
					}
					NkVector<int32> stack;
					int32 block = 0; // 1 = dans un /* ... */
					const int32 n = LineCount();
					for (int32 i = 0; i < n; ++i) {
						const NkCodeLine &L = lines[i];
						const char *s = L.Data();
						const int32 m = static_cast<int32>(L.Size());
						int32 j = 0;
						while (j < m) {
							const char c = s[j];
							if (block) {
								if (c == '*' && j + 1 < m && s[j + 1] == '/') {
									block = 0;
									j += 2;
									continue;
								}
								++j;
								continue;
							}
							if (c == '/' && j + 1 < m && s[j + 1] == '/')
								break; // commentaire ligne -> fin de ligne
							if (c == '/' && j + 1 < m && s[j + 1] == '*') {
								block = 1;
								j += 2;
								continue;
							}
							if (c == '"' || c == '\'') { // chaîne / caractère (échappements \)
								const char q = c;
								++j;
								while (j < m) {
									if (s[j] == '\\') {
										j += 2;
										continue;
									}
									if (s[j] == q) {
										++j;
										break;
									}
									++j;
								}
								continue;
							}
							if (c == '{')
								stack.PushBack(i);
							else if (c == '}') {
								if (!stack.Empty()) {
									const int32 h = stack.Back();
									stack.PopBack();
									if (h < i) { // région multi-lignes uniquement
										foldHdr.PushBack(h);
										foldEnd.PushBack(i);
									}
								}
							}
							++j;
						}
					}
					// Purge les replis dont l'en-tête n'est plus une région valide (édition).
					for (int32 k = static_cast<int32>(foldOn.Size()) - 1; k >= 0; --k)
						if (FoldEndOf(foldOn[k]) < 0)
							foldOn.Erase(foldOn.Begin() + k);
				}

				// Reconstruit rowOfLine / visRows depuis l'état de repli courant.
				void RebuildVisRows() {
					const int32 n = LineCount();
					rowOfLine.Resize(n, 0);
					for (int32 i = 0; i < n; ++i)
						rowOfLine[i] = 0;
					for (usize k = 0; k < foldOn.Size(); ++k) { // masque (hdr, end] de chaque région repliée
						const int32 e = FoldEndOf(foldOn[k]);
						if (e < 0)
							continue;
						for (int32 j = foldOn[k] + 1; j <= e && j < n; ++j)
							rowOfLine[j] = -1;
					}
					visRows.Clear();
					int32 row = 0;
					for (int32 i = 0; i < n; ++i) {
						if (rowOfLine[i] < 0)
							continue; // masquée
						rowOfLine[i] = row++;
						visRows.PushBack(i);
					}
				}

				// Recalcul paresseux : régions si le contenu a changé, mapping si un repli a bougé.
				void EnsureFolds(NkLang lang) {
					const int64 s = SymSig();
					if (s != foldSig) {
						foldSig = s;
						ComputeFoldRegions(lang);
						foldDirty = true;
					}
					if (foldDirty || static_cast<int32>(rowOfLine.Size()) != LineCount()) {
						RebuildVisRows();
						foldDirty = false;
					}
				}

				static void SortDedup(NkVector<NkString> &v) {
					for (int32 i = 1; i < (int32)v.Size(); ++i) {
						NkString key = v[i];
						int32 j = i - 1;
						while (j >= 0 && SymCmp(v[j].CStr(), key.CStr()) > 0) {
							v[j + 1] = v[j];
							--j;
						}
						v[j + 1] = key;
					}
					for (int32 i = (int32)v.Size() - 1; i > 0; --i)
						if (SymCmp(v[i].CStr(), v[i - 1].CStr()) == 0)
							v.Erase(v.Begin() + i);
				}

				void RebuildSymbols(NkLang lang) {
					symTypes.Clear();
					symFuncs.Clear();
					symWords.Clear();
					if (lang == NkLang::None || lang == NkLang::Markdown)
						return;
					const bool isC = (lang == NkLang::C || lang == NkLang::NKSL);
					NkScanTextSymbols(GetText().CStr(), isC, symTypes, symFuncs);
					NkSymSortDedup(symTypes);
					NkSymSortDedup(symFuncs);
					// ── TOUS les identifiants du document (variables locales, paramètres, membres…) :
					//    la complétion par mots doit proposer ce qui existe DANS le fichier, même sans
					//    définition reconnue (façon « word-based suggestions » de VSCode). ──
					for (int32 l = 0; l < LineCount() && symWords.Size() < 2500; ++l) {
						const NkCodeLine &L = lines[l];
						const int32 n = static_cast<int32>(L.Size());
						int32 i = 0;
						while (i < n) {
							const char c = L[i];
							if (!IsWChar(c) || (c >= '0' && c <= '9')) { // un mot commence par lettre/_
								++i;
								continue;
							}
							int32 e = i;
							while (e < n && IsWChar(L[e]))
								++e;
							if (e - i >= 3) { // ignore les mots trop courts (bruit)
								char nm[128];
								int32 k = 0;
								for (int32 t = i; t < e && k < 127; ++t)
									nm[k++] = L[t];
								nm[k] = 0;
								symWords.PushBack(NkString(nm));
							}
							i = e;
						}
					}
					NkSymSortDedup(symWords);
				}

				void ExtractSyms(const char *p, int32 n, bool isC, NkLang lang) {
					(void)lang;
					int32 i = 0;
					while (i < n && (p[i] == ' ' || p[i] == '\t'))
						++i;
					if (i >= n || p[i] == '#')
						return; // ligne vide / préproc-commentaire
					auto starts = [&](const char *w) {
						int32 k = 0;
						for (; w[k]; ++k)
							if (i + k >= n || p[i + k] != w[k])
								return false;
						return true;
					};
					auto pushRange = [&](int32 s, int32 e, NkVector<NkString> &out) {
						if (e > s && (p[s] < '0' || p[s] > '9')) {
							char nm[128];
							int32 k = 0;
							for (int32 t = s; t < e && k < 127; ++t)
								nm[k++] = p[t];
							nm[k] = 0;
							out.PushBack(NkString(nm));
						}
					};
					auto addFrom = [&](int32 s, NkVector<NkString> &out) {
						while (s < n && (p[s] == ' ' || p[s] == '\t'))
							++s;
						int32 e = s;
						while (e < n && IsWChar(p[e]))
							++e;
						pushRange(s, e, out);
					};
					// ── Types ──
					if (starts("struct "))
						addFrom(i + 7, symTypes);
					else if (starts("class "))
						addFrom(i + 6, symTypes);
					else if (starts("enum class "))
						addFrom(i + 11, symTypes);
					else if (starts("enum "))
						addFrom(i + 5, symTypes);
					else if (starts("union "))
						addFrom(i + 6, symTypes);
					else if (starts("namespace "))
						addFrom(i + 10, symTypes);
					else if (isC && starts("using "))
						addFrom(i + 6, symTypes);		  // using Alias = ...
					else if (isC && starts("typedef ")) { // alias = dernier identifiant avant ';'
						int32 e = n;
						while (e > 0 && (p[e - 1] == ' ' || p[e - 1] == '\t' || p[e - 1] == ';'))
							--e;
						int32 s = e;
						while (s > 0 && IsWChar(p[s - 1]))
							--s;
						pushRange(s, e, symTypes);
					}
					// ── Fonctions ──
					if (!isC && starts("def "))
						addFrom(i + 4, symFuncs); // Python
					if (isC) {					  // C : définition (ligne finissant par '{' ou ')' sans ';')
						int32 e = n;
						while (e > 0 && (p[e - 1] == ' ' || p[e - 1] == '\t'))
							--e;
						const bool endsBrace = (e > 0 && p[e - 1] == '{'), endsParen = (e > 0 && p[e - 1] == ')');
						bool hasSemi = false;
						for (int32 t = 0; t < n; ++t)
							if (p[t] == ';') {
								hasSemi = true;
								break;
							}
						if (endsBrace || (endsParen && !hasSemi)) {
							int32 par = -1;
							for (int32 t = i; t < n; ++t) {
								if (p[t] == '(') {
									par = t;
									break;
								}
								if (p[t] == ';' || p[t] == '=')
									break;
							}
							if (par > i) {
								int32 ee = par;
								while (ee > i && p[ee - 1] == ' ')
									--ee;
								int32 ss = ee;
								while (ss > i && IsWChar(p[ss - 1]))
									--ss;
								char nm[128];
								int32 k = 0;
								for (int32 t = ss; t < ee && k < 127; ++t)
									nm[k++] = p[t];
								nm[k] = 0;
								if (ee > ss && SymCmp(nm, "if") && SymCmp(nm, "for") && SymCmp(nm, "while") &&
									SymCmp(nm, "switch") && SymCmp(nm, "catch") && SymCmp(nm, "return") &&
									SymCmp(nm, "sizeof"))
									symFuncs.PushBack(NkString(nm));
							}
						}
					}
				}

				bool mojibake = false; // contenu vraisemblablement double-encodé UTF-8 (bannière « Réparer »)
				// Cache de la largeur de la PLUS LONGUE ligne (px) -> barre H stable
				// (recalcule seulement a l'edition, pas a chaque frame / scroll).
				bool widthDirty = true;
				float32 maxLineWCache = 0.f;

				int32 LineCount() const {
					return static_cast<int32>(lines.Size());
				}

				int32 LineLen(int32 l) const {
					return (l >= 0 && l < LineCount()) ? static_cast<int32>(lines[l].Size()) : 0;
				}

				bool HasSel() const {
					return curLine != selLine || curCol != selCol;
				}

				void Collapse() {
					selLine = curLine;
					selCol = curCol;
				}

				void EnsureNonEmpty() {
					if (lines.Empty())
						lines.PushBack(NkCodeLine());
				}

				void ClampCursor() {
					if (curLine < 0)
						curLine = 0;
					if (curLine >= LineCount())
						curLine = LineCount() - 1;
					if (curLine < 0) {
						curLine = 0;
					}
					if (curCol < 0)
						curCol = 0;
					const int32 m = LineLen(curLine);
					if (curCol > m)
						curCol = m;
				}

				// Selection normalisee : (aL,aC) <= (bL,bC).
				void SelRange(int32 &aL, int32 &aC, int32 &bL, int32 &bC) const {
					aL = selLine;
					aC = selCol;
					bL = curLine;
					bC = curCol;
					if (aL > bL || (aL == bL && aC > bC)) {
						int32 tL = aL, tC = aC;
						aL = bL;
						aC = bC;
						bL = tL;
						bC = tC;
					}
				}

				// ── Construction / serialisation ──────────────────────────────────────
				void Clear() {
					lines.Clear();
					curLine = curCol = selLine = selCol = 0;
					scrollX = scrollY = 0.f;
					widthDirty = true;
				}

				void SetText(const char *s) {
					Clear();
					lines.PushBack(NkCodeLine());
					for (const char *p = s; p && *p; ++p) {
						if (*p == '\n')
							lines.PushBack(NkCodeLine());
						else if (*p == '\r') {
						} // ignore CR (CRLF -> LF)
						else if (*p == '\t') {
							for (int k = 0; k < 4; ++k)
								lines[lines.Size() - 1].PushBack(' ');
						} else
							lines[lines.Size() - 1].PushBack(*p);
					}
					EnsureNonEmpty();
					mojibake = NkMojibakeDetect(s); // détecte le double-encodage à l'ouverture
				}

				// Répare le double-encodage UTF-8 du document (bouton « Réparer l'encodage »).
				// Reconstruit le texte correct puis le recharge ; marque le doc modifié (à enregistrer).
				void RepairEncoding() {
					const NkString fixed = NkMojibakeRepair(GetText().CStr());
					const int32 sl = curLine, sc = curCol; // préserve grossièrement le curseur
					SetText(fixed.CStr());				   // recharge (mojibake re-détecté -> false)
					curLine = sl;
					curCol = sc;
					ClampCursor();
					Collapse();
					dirty = true;
					widthDirty = true;
				}

				NkString GetText() const {
					NkVector<char> buf;
					for (usize i = 0; i < lines.Size(); ++i) {
						const NkCodeLine &ln = lines[i];
						for (usize j = 0; j < ln.Size(); ++j)
							buf.PushBack(ln[j]);
						if (i + 1 < lines.Size())
							buf.PushBack('\n');
					}
					buf.PushBack('\0');
					return NkString(buf.Data());
				}

				// ── Edition ───────────────────────────────────────────────────────────
				void EraseSelection() {
					if (!HasSel())
						return;
					int32 aL, aC, bL, bC;
					SelRange(aL, aC, bL, bC);
					if (aL == bL) {
						lines[aL].Erase(lines[aL].Begin() + aC, lines[aL].Begin() + bC);
					} else {
						NkCodeLine &A = lines[aL];
						A.Erase(A.Begin() + aC, A.End()); // garde [0, aC)
						NkCodeLine &B = lines[bL];
						for (usize j = static_cast<usize>(bC); j < B.Size(); ++j)
							A.PushBack(B[j]); // + queue de B
						lines.Erase(lines.Begin() + (aL + 1), lines.Begin() + (bL + 1));
					}
					curLine = aL;
					curCol = aC;
					Collapse();
					dirty = true;
					widthDirty = true;
				}

				void InsertChar(char c) {
					EraseSelection();
					NkCodeLine &L = lines[curLine];
					L.Insert(L.Begin() + curCol, c);
					++curCol;
					Collapse();
					dirty = true;
					widthDirty = true;
				}

				void InsertNewline() {
					EraseSelection();
					NkCodeLine &C = lines[curLine];
					// Auto-indentation (façon VSCode) : la nouvelle ligne reprend les blancs de tête
					// de la ligne courante, + un niveau si elle se termine par '{' avant le caret.
					NkCodeLine tail;
					const int32 lim = curCol < static_cast<int32>(C.Size()) ? curCol : static_cast<int32>(C.Size());
					int32 ind = 0;
					while (ind < lim && (C[ind] == ' ' || C[ind] == '\t'))
						++ind;
					int32 last = lim;
					while (last > 0 && (C[last - 1] == ' ' || C[last - 1] == '\t'))
						--last;
					const bool open = last > 0 && C[last - 1] == '{';
					// ── Commentaires de bloc : continuation automatique de la documentation. ──
					// `/*|*/` (ou `/**|*/`) -> ligne du milieu ` * ` (caret) + fermante ` */` alignée ;
					// à l'intérieur d'un bloc (ligne `/*…` non fermée ou ligne `* …`) -> la nouvelle
					// ligne est préfixée `* ` automatiquement (style doc).
					{
						int32 t0 = 0;
						while (t0 < lim && (C[t0] == ' ' || C[t0] == '\t'))
							++t0;
						const bool begBlock = (t0 + 1 < lim && C[t0] == '/' && C[t0 + 1] == '*');
						const bool contStar =
							(t0 < lim && C[t0] == '*' && !(t0 + 1 < static_cast<int32>(C.Size()) && C[t0 + 1] == '/'));
						bool closedBefore = false;
						for (int32 k2 = t0; k2 + 1 < lim; ++k2)
							if (C[k2] == '*' && C[k2 + 1] == '/') {
								closedBefore = true;
								break;
							}
						if ((begBlock || contStar) && !closedBefore) {
							const bool closeAfter =
								(curCol + 1 < static_cast<int32>(C.Size()) && C[curCol] == '*' && C[curCol + 1] == '/');
							NkCodeLine mid;
							for (int32 k2 = 0; k2 < t0; ++k2)
								mid.PushBack(C[k2]);
							if (begBlock)
								mid.PushBack(' ');
							mid.PushBack('*');
							mid.PushBack(' ');
							const int32 caretCol = static_cast<int32>(mid.Size());
							if (closeAfter) { // `/*|*/` -> ligne du milieu + fermante alignée dessous
								NkCodeLine closeL;
								for (int32 k2 = 0; k2 < t0; ++k2)
									closeL.PushBack(C[k2]);
								if (begBlock)
									closeL.PushBack(' ');
								for (usize j = static_cast<usize>(curCol); j < C.Size(); ++j)
									closeL.PushBack(C[j]);
								C.Erase(C.Begin() + curCol, C.End());
								lines.Insert(lines.Begin() + (curLine + 1), mid);
								lines.Insert(lines.Begin() + (curLine + 2), closeL);
							} else { // à l'intérieur du bloc : nouvelle ligne `* ` (+ texte déplacé)
								for (usize j = static_cast<usize>(curCol); j < C.Size(); ++j)
									mid.PushBack(C[j]);
								C.Erase(C.Begin() + curCol, C.End());
								lines.Insert(lines.Begin() + (curLine + 1), mid);
							}
							++curLine;
							curCol = caretCol;
							Collapse();
							dirty = true;
							widthDirty = true;
							return;
						}
					}
					// `{|}` : Entrée crée une ligne VIDE indentée (+1 niveau) pour le caret et
					// renvoie la fermante sur sa PROPRE ligne, alignée sur l'indentation d'origine.
					if (open && curCol < static_cast<int32>(C.Size()) && C[curCol] == '}') {
						NkCodeLine mid, closeL;
						for (int32 k = 0; k < ind; ++k) {
							mid.PushBack(C[k]);
							closeL.PushBack(C[k]);
						}
						for (int32 k = 0; k < 4; ++k)
							mid.PushBack(' ');
						for (usize j = static_cast<usize>(curCol); j < C.Size(); ++j)
							closeL.PushBack(C[j]);
						C.Erase(C.Begin() + curCol, C.End());
						lines.Insert(lines.Begin() + (curLine + 1), mid);
						lines.Insert(lines.Begin() + (curLine + 2), closeL);
						++curLine;
						curCol = static_cast<int32>(mid.Size());
						Collapse();
						dirty = true;
						widthDirty = true;
						return;
					}
					for (int32 k = 0; k < ind; ++k)
						tail.PushBack(C[k]);
					if (open)
						for (int32 k = 0; k < 4; ++k)
							tail.PushBack(' ');
					const int32 newCol = static_cast<int32>(tail.Size());
					for (usize j = static_cast<usize>(curCol); j < C.Size(); ++j)
						tail.PushBack(C[j]);
					C.Erase(C.Begin() + curCol, C.End());
					lines.Insert(lines.Begin() + (curLine + 1), tail);
					++curLine;
					curCol = newCol;
					Collapse();
					dirty = true;
					widthDirty = true;
				}

				void Backspace() {
					if (HasSel()) {
						EraseSelection();
						return;
					}
					if (curCol > 0) {
						// Paire vide `(|)` `[|]` `{|}` : Backspace supprime l'ouvrante ET la fermante.
						{
							const char pb = lines[curLine][curCol - 1];
							const char pa = curCol < LineLen(curLine) ? lines[curLine][curCol] : 0;
							if ((pb == '(' && pa == ')') || (pb == '[' && pa == ']') || (pb == '{' && pa == '}'))
								lines[curLine].Erase(lines[curLine].Begin() + curCol); // la fermante d'abord
						}
						// Indentation intelligente : dans les BLANCS DE TÊTE, efface d'un coup
						// jusqu'au multiple de 4 précédent (une « tabulation » de 4 espaces).
						const NkCodeLine &L0 = lines[curLine];
						bool leading = true;
						for (int32 k = 0; k < curCol && leading; ++k)
							leading = (L0[k] == ' ' || L0[k] == '\t');
						if (leading && L0[curCol - 1] == ' ') {
							int32 del = ((curCol - 1) % 4) + 1;
							while (del-- > 0 && curCol > 0 && lines[curLine][curCol - 1] == ' ') {
								lines[curLine].Erase(lines[curLine].Begin() + (curCol - 1));
								--curCol;
							}
							Collapse();
							dirty = true;
							widthDirty = true;
							return;
						}
						lines[curLine].Erase(lines[curLine].Begin() + (curCol - 1));
						--curCol;
					} else if (curLine > 0) {
						const int32 prev = curLine - 1, plen = LineLen(prev);
						NkCodeLine &P = lines[prev];
						NkCodeLine &C = lines[curLine];
						for (usize j = 0; j < C.Size(); ++j)
							P.PushBack(C[j]);
						lines.Erase(lines.Begin() + curLine);
						curLine = prev;
						curCol = plen;
					}
					Collapse();
					dirty = true;
					widthDirty = true;
				}

				void DeleteFwd() {
					if (HasSel()) {
						EraseSelection();
						return;
					}
					if (curCol < LineLen(curLine)) {
						lines[curLine].Erase(lines[curLine].Begin() + curCol);
					} else if (curLine < LineCount() - 1) {
						NkCodeLine &C = lines[curLine];
						NkCodeLine &N = lines[curLine + 1];
						for (usize j = 0; j < N.Size(); ++j)
							C.PushBack(N[j]);
						lines.Erase(lines.Begin() + (curLine + 1));
					}
					Collapse();
					dirty = true;
					widthDirty = true;
				}

				// ── Opérations LIGNE (raccourcis éditeur, Phase 4) ──────────────────────
				void SelLineRange(int32 &a, int32 &b) const {
					a = selLine < curLine ? selLine : curLine;
					b = selLine < curLine ? curLine : selLine;
					if (a < 0)
						a = 0;
					if (b >= LineCount())
						b = LineCount() - 1;
				}

				// Ctrl+L : 1er appui = sélectionne la ligne courante (ancre col 0, curseur en
				// fin de ligne — pas de débordement) ; appuis suivants = ÉTEND vers le bas
				// ligne par ligne (accumulation), façon VS Code.
				void SelectCurrentLine() {
					// « Continue » = notre sélection-ligne précédente est INTACTE (ancre valide,
					// sélection pleine-ligne du bas). Sinon on (re)part de la ligne courante.
					const bool cont = (lineSelAnchor >= 0 && lineSelAnchor < LineCount() && selLine == lineSelAnchor &&
									   selCol == 0 && curLine >= selLine && curCol == LineLen(curLine));
					int32 anchor, bottom;
					if (cont) {
						anchor = lineSelAnchor;
						bottom = (curLine < LineCount() - 1) ? curLine + 1 : curLine;
					} // étend d'une ligne
					else {
						anchor = bottom = curLine;
						lineSelAnchor = curLine;
					} // nouvelle sélection
					if (bottom >= LineCount())
						bottom = LineCount() - 1;
					selLine = anchor;
					selCol = 0;
					curLine = bottom;
					curCol = LineLen(bottom);
				}

				void DeleteLines() {
					int32 a, b;
					SelLineRange(a, b);
					for (int32 l = b; l >= a; --l)
						if (LineCount() > 1)
							lines.Erase(lines.Begin() + l);
					if (lines.Empty())
						lines.PushBack(NkCodeLine());
					curLine = a;
					if (curLine >= LineCount())
						curLine = LineCount() - 1;
					curCol = 0;
					Collapse();
					dirty = true;
					widthDirty = true;
				}

				void MoveLines(bool up) {
					int32 a, b;
					SelLineRange(a, b);
					if (up) {
						if (a == 0)
							return;
						NkCodeLine t = lines[a - 1];
						lines.Erase(lines.Begin() + (a - 1));
						lines.Insert(lines.Begin() + b, t);
						--a;
						--b;
					} else {
						if (b >= LineCount() - 1)
							return;
						NkCodeLine t = lines[b + 1];
						lines.Erase(lines.Begin() + (b + 1));
						lines.Insert(lines.Begin() + a, t);
						++a;
						++b;
					}
					selLine = a;
					curLine = b;
					ClampCursor();
					dirty = true;
					widthDirty = true;
				}

				void DuplicateLines(bool up) {
					int32 a, b;
					SelLineRange(a, b);
					NkVector<NkCodeLine> copy;
					for (int32 l = a; l <= b; ++l)
						copy.PushBack(lines[l]);
					const int32 at = b + 1;
					for (int32 i = 0; i < static_cast<int32>(copy.Size()); ++i)
						lines.Insert(lines.Begin() + at + i, copy[i]);
					if (!up) {
						selLine = at;
						curLine = at + (b - a);
					} // curseur sur la copie (bas)
					else {
						selLine = a;
						curLine = b;
					} // reste sur l'original (haut)
					curCol = LineLen(curLine);
					Collapse();
					dirty = true;
					widthDirty = true;
				}

				void InsertLineBelow() {
					lines.Insert(lines.Begin() + curLine + 1, NkCodeLine());
					++curLine;
					curCol = 0;
					Collapse();
					dirty = true;
					widthDirty = true;
				}

				void InsertLineAbove() {
					lines.Insert(lines.Begin() + curLine, NkCodeLine());
					curCol = 0;
					Collapse();
					dirty = true;
					widthDirty = true;
				}

				void IndentSelection(bool out) {
					int32 a, b;
					SelLineRange(a, b);
					for (int32 l = a; l <= b; ++l) {
						if (!out) {
							for (int32 k = 0; k < 4; ++k)
								lines[l].Insert(lines[l].Begin(), ' ');
						} else {
							int32 rm = 0;
							while (rm < 4 && rm < LineLen(l) && lines[l][rm] == ' ')
								++rm;
							for (int32 k = 0; k < rm; ++k)
								lines[l].Erase(lines[l].Begin());
						}
					}
					ClampCursor();
					dirty = true;
					widthDirty = true;
				}

				// Commente/décommente les lignes sélectionnées avec `prefix` (ex "//", "#").
				void ToggleComment(const char *prefix) {
					if (!prefix || !*prefix)
						return;
					int32 a, b;
					SelLineRange(a, b);
					int32 pl = 0;
					while (prefix[pl])
						++pl;
					bool allComment = true;
					for (int32 l = a; l <= b; ++l) {
						int32 i = 0;
						while (i < LineLen(l) && lines[l][i] == ' ')
							++i;
						if (i >= LineLen(l))
							continue; // ligne vide -> ignore
						bool m = (i + pl <= LineLen(l));
						for (int32 k = 0; m && k < pl; ++k)
							if (lines[l][i + k] != prefix[k])
								m = false;
						if (!m) {
							allComment = false;
							break;
						}
					}
					for (int32 l = a; l <= b; ++l) {
						int32 i = 0;
						while (i < LineLen(l) && lines[l][i] == ' ')
							++i;
						if (i >= LineLen(l))
							continue;
						if (allComment) {
							for (int32 k = 0; k < pl; ++k)
								lines[l].Erase(lines[l].Begin() + i);
							if (i < LineLen(l) && lines[l][i] == ' ')
								lines[l].Erase(lines[l].Begin() + i);
						} else {
							lines[l].Insert(lines[l].Begin() + i, ' ');
							for (int32 k = pl - 1; k >= 0; --k)
								lines[l].Insert(lines[l].Begin() + i, prefix[k]);
						}
					}
					ClampCursor();
					dirty = true;
					widthDirty = true;
				}

				static bool MatchAt(const char *s, int32 i, const char *pat, int32 plen, int32 n) {
					if (i < 0 || i + plen > n)
						return false;
					for (int32 k = 0; k < plen; ++k)
						if (s[i + k] != pat[k])
							return false;
					return true;
				}

				int32 LineColToOffset(int32 line, int32 col) const {
					int32 off = 0;
					for (int32 i = 0; i < line && i < LineCount(); ++i)
						off += LineLen(i) + 1;
					return off + col;
				}

				// Commentaire BLOC INTELLIGENT (Ctrl+Maj+/) — délimiteurs DISTINCTS (ex "/* "…" */").
				// TOGGLE : si la sélection est DÉJÀ dans un commentaire englobant `open…close`, on
				// le RETIRE (dé-commente) au lieu d'imbriquer (ce qui casserait le commentaire).
				void BlockComment(const char *open, const char *close) {
					if (!open || !*open || !close || !*close)
						return;
					int32 aL, aC, bL, bC;
					if (HasSel())
						SelRange(aL, aC, bL, bC);
					else {
						aL = bL = curLine;
						aC = 0;
						bC = LineLen(curLine);
					}
					const NkString full = GetText();
					const char *s = full.CStr();
					const int32 n = static_cast<int32>(full.Size());
					const int32 ol = static_cast<int32>(NkString(open).Size()),
								cl = static_cast<int32>(NkString(close).Size());
					const int32 aOff = LineColToOffset(aL, aC), bOff = LineColToOffset(bL, bC);
					// Commentaire ENGLOBANT la sélection : `open` à/avant le début (sans `close`
					// entre) + son `close` apparié à/après la fin. Couvre BOTH : sélection = le
					// commentaire exact (délimiteurs aux bords) ET sélection À L'INTÉRIEUR d'un
					// commentaire (délimiteurs dehors). Si trouvé -> on RETIRE (dé-commente).
					int32 encOpen = -1;
					for (int32 i = aOff; i >= 0; --i) {
						if (MatchAt(s, i, close, cl, n))
							break;
						if (MatchAt(s, i, open, ol, n)) {
							encOpen = i;
							break;
						}
					}
					int32 encClose = -1;
					if (encOpen >= 0)
						for (int32 i = encOpen + ol; i + cl <= n; ++i) {
							if (MatchAt(s, i, open, ol, n))
								break;
							if (MatchAt(s, i, close, cl, n)) {
								encClose = i;
								break;
							}
						}
					const bool found = (encOpen >= 0 && encClose >= 0 && encOpen <= aOff && encClose + cl >= bOff);
					const bool exact = found && encOpen >= aOff &&
									   encClose + cl <= bOff; // les DEUX délimiteurs sont DANS la sélection
					// Sélection À L'INTÉRIEUR d'un commentaire (délimiteurs dehors) -> NE PAS
					// retirer le bloc : commentaire de LIGNE sur la sélection (comme demandé).
					if (found && !exact) {
						ToggleComment("//");
						return;
					}
					NkVector<char> out;
					if (exact) {
						// dé-commente le bloc EXACT : copie tout SAUF [encOpen,+ol) et [encClose,+cl)
						for (int32 i = 0; i < n; ++i) {
							if ((i >= encOpen && i < encOpen + ol) || (i >= encClose && i < encClose + cl))
								continue;
							out.PushBack(s[i]);
						}
					} else {
						// commente : insère open à aOff et close à bOff
						for (int32 i = 0; i < n; ++i) {
							if (i == aOff)
								for (int32 k = 0; k < ol; ++k)
									out.PushBack(open[k]);
							if (i == bOff)
								for (int32 k = 0; k < cl; ++k)
									out.PushBack(close[k]);
							out.PushBack(s[i]);
						}
						if (bOff == n)
							for (int32 k = 0; k < cl; ++k)
								out.PushBack(close[k]); // close en toute fin
					}
					out.PushBack('\0');
					SetText(out.Data());
					curLine = aL;
					curCol = aC;
					ClampCursor();
					Collapse();
					dirty = true;
					widthDirty = true;
				}

				// Texte selectionne (multi-ligne, lignes jointes par '\n'). Vide si pas de selection.
				NkString GetSelectedText() const {
					if (!HasSel())
						return NkString();
					int32 aL, aC, bL, bC;
					SelRange(aL, aC, bL, bC);
					NkVector<char> buf;
					for (int32 l = aL; l <= bL; ++l) {
						const int32 c0 = (l == aL) ? aC : 0, c1 = (l == bL) ? bC : LineLen(l);
						for (int32 c = c0; c < c1; ++c)
							buf.PushBack(lines[l][c]);
						if (l < bL)
							buf.PushBack('\n');
					}
					buf.PushBack('\0');
					return NkString(buf.Data());
				}

				// Insere `s` au curseur (remplace la selection). Gere '\n' / '\t'.
				void InsertText(const char *s) {
					if (HasSel())
						EraseSelection();
					for (const char *p = s; *p; ++p) {
						if (*p == '\n')
							InsertNewline();
						else if (*p == '\r') { /* ignore */
						} else if (*p == '\t') {
							for (int32 k = 0; k < 4; ++k)
								InsertChar(' ');
						} else
							InsertChar(*p);
					}
				}

				void SelectAll() {
					selLine = 0;
					selCol = 0;
					curLine = LineCount() - 1;
					if (curLine < 0)
						curLine = 0;
					curCol = LineLen(curLine);
				}

				// Re-indente le document a partir de la profondeur d'accolades (style VS
				// "Format Document"). 4 espaces / niveau ; les lignes commencant par } ) ]
				// se desindentent. v1 : naif (ignore accolades dans chaines/commentaires).
				void FormatCpp() {
					int32 depth = 0;
					for (usize li = 0; li < lines.Size(); ++li) {
						NkCodeLine &ln = lines[li];
						usize s = 0;
						while (s < ln.Size() && (ln[s] == ' ' || ln[s] == '\t'))
							++s;
						int32 lineDepth = depth;
						if (s < ln.Size() && (ln[s] == '}' || ln[s] == ')' || ln[s] == ']'))
							lineDepth = depth > 0 ? depth - 1 : 0;
						NkCodeLine nl;
						if (s < ln.Size()) { // ligne non vide -> indente
							for (int32 d = 0; d < lineDepth; ++d)
								for (int32 k = 0; k < 4; ++k)
									nl.PushBack(' ');
							for (usize j = s; j < ln.Size(); ++j)
								nl.PushBack(ln[j]);
						}
						for (usize j = s; j < ln.Size(); ++j) { // MAJ profondeur
							const char c = ln[j];
							if (c == '{')
								++depth;
							else if (c == '}') {
								if (depth > 0)
									--depth;
							}
						}
						ln = nl;
					}
					dirty = true;
					widthDirty = true;
					ClampCursor();
					Collapse();
				}
		};

		// ── Focus clavier global (un seul editeur actif a la fois) ────────────────
		// Minimap ON/OFF globale (raccourci + commande de palette).
		inline bool &NkCodeMinimapOn() {
			static bool v = true;
			return v;
		}

		inline NkGuiId &NkCodeFocusId() {
			static NkGuiId id = NKGUI_ID_NONE;
			return id;
		}

		inline NkCtxMenu &NkCodeCtxMenu() {
			static NkCtxMenu mn;
			return mn;
		} // menu clic droit (partage)

		namespace detail {
			inline bool InRect(const NkRect &r, const NkVec2 &p) {
				return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
			}

			// Menu du QUICK FIX (Ctrl+.) — un seul ouvert a la fois, position = caret.
			inline NkCtxMenu &NkCodeQfMenu() {
				static NkCtxMenu m;
				return m;
			}

			// Largeur en px du prefixe [0, col) de la ligne `l`.
			inline float32 PrefixW(NkGuiContext &ctx, const NkCodeDoc &d, int32 l, int32 col) {
				if (col <= 0 || l < 0 || l >= d.LineCount())
					return 0.f;
				const NkCodeLine &ln = d.lines[l];
				if (ln.Size() == 0)
					return 0.f;
				const int32 c = col > static_cast<int32>(ln.Size()) ? static_cast<int32>(ln.Size()) : col;
				return ctx.font->Face()->CalcTextSizeX(ln.Data(), ln.Data() + c);
			}

			// Colonne dont la position pixel est la plus proche de targetX.
			inline int32 ColAtX(NkGuiContext &ctx, const NkCodeDoc &d, int32 l, float32 targetX) {
				const int32 n = d.LineLen(l);
				if (n <= 0 || targetX <= 0.f)
					return 0;
				const NkCodeLine &ln = d.lines[l];
				float32 prev = 0.f;
				for (int32 i = 1; i <= n; ++i) {
					const float32 w = ctx.font->Face()->CalcTextSizeX(ln.Data(), ln.Data() + i);
					if (w >= targetX)
						return (targetX - prev < w - targetX) ? (i - 1) : i;
					prev = w;
				}
				return n;
			}
		} // namespace detail

		// Préfixe de commentaire de ligne selon le langage (Ctrl+/). "" = pas de commentaire.
		inline const char *CommentPrefix(NkLang lang) {
			switch (lang) {
				case NkLang::C:
				case NkLang::NKSL:
					return "//";
				case NkLang::Python:
					return "#";
				default:
					return ""; // None / Markdown
			}
		}

		// ── Autocomplétion : mots-clés par langage + filtrage par préfixe ────────────
		inline const char *const *NkKeywordsFor(NkLang lang, int32 &count) {
			static const char *kCpp[] = {
				"alignas", "alignof",  "auto",		"bool",		"break",	   "case",		"catch",   "char",
				"class",   "const",	   "constexpr", "continue", "decltype",	   "default",	"delete",  "do",
				"double",  "else",	   "enum",		"explicit", "extern",	   "false",		"float",   "for",
				"friend",  "goto",	   "if",		"inline",	"int",		   "long",		"mutable", "namespace",
				"new",	   "noexcept", "nullptr",	"operator", "private",	   "protected", "public",  "return",
				"short",   "signed",   "sizeof",	"static",	"static_cast", "struct",	"switch",  "template",
				"this",	   "throw",	   "true",		"try",		"typedef",	   "typename",	"union",   "unsigned",
				"using",   "virtual",  "void",		"volatile", "while",	   "override",	"final",   "uint8",
				"uint16",  "uint32",   "uint64",	"int8",		"int16",	   "int32",		"int64",   "usize",
				"float32", "float64",  nullptr};
			static const char *kPy[] = {
				"and",	"as",	 "assert", "async", "await",	"break", "class", "continue", "def",   "del",
				"elif", "else",	 "except", "False", "finally",	"for",	 "from",  "global",	  "if",	   "import",
				"in",	"is",	 "lambda", "None",	"nonlocal", "not",	 "or",	  "pass",	  "raise", "return",
				"True", "try",	 "while",  "with",	"yield",	"self",	 "range", "len",	  "print", "str",
				"int",	"float", "list",   "dict",	"set",		"tuple", nullptr};
			static const char *kNone[] = {nullptr};
			const char *const *p = (lang == NkLang::C) ? kCpp : (lang == NkLang::Python) ? kPy : kNone;
			count = 0;
			for (const char *const *q = p; *q; ++q)
				++count;
			return p;
		}

		inline bool NkStartsWithI(const char *s, const char *pre) {
			for (; *pre; ++s, ++pre) {
				char a = *s, b = *pre;
				if (a >= 'A' && a <= 'Z')
					a += 32;
				if (b >= 'A' && b <= 'Z')
					b += 32;
				if (a != b)
					return false;
			}
			return true;
		}

		// Candidats commençant par `prefix` (symboles fichier + projet + mots-clés), dédup, cap 40.
		inline void NkBuildCompletions(const char *prefix, NkLang lang, const NkVector<NkString> *fileT,
									   const NkVector<NkString> *fileF, const NkVector<NkString> *fileW,
									   const NkVector<NkString> *projT, const NkVector<NkString> *projF,
									   NkVector<NkString> &out) {
			out.Clear();
			if (!prefix)
				return;
			auto add = [&](const char *s) {
				if (*prefix && !NkStartsWithI(s, prefix))
					return;
				if (*prefix && NkCodeDoc::SymCmp(s, prefix) == 0)
					return; // pas le mot exact
				for (usize i = 0; i < out.Size(); ++i)
					if (NkCodeDoc::SymCmp(out[i].CStr(), s) == 0)
						return; // dédup
				if (out.Size() < 40)
					out.PushBack(NkString(s));
			};
			// Ordre de priorité : symboles du fichier, puis TOUS les mots du fichier (variables
			// locales, paramètres…), puis l'index projet, puis les mots-clés du langage.
			const NkVector<NkString> *lists[] = {fileT, fileF, fileW, projT, projF};
			for (int32 li = 0; li < 5; ++li)
				if (lists[li])
					for (usize i = 0; i < lists[li]->Size(); ++i)
						add((*lists[li])[i].CStr());
			int32 kn = 0;
			const char *const *kw = NkKeywordsFor(lang, kn);
			for (int32 i = 0; i < kn; ++i)
				add(kw[i]);
		}

		// Dessine + pilote un editeur de code sur `d` dans `area`. Retourne true si le
		// document a ete modifie cette frame.
		inline bool CodeEditor(NkGuiContext &ctx, const char *idStr, NkCodeDoc &d, const NkRect &area,
							   NkLang lang = NkLang::None, const NkVector<NkString> *projTypes = nullptr,
							   const NkVector<NkString> *projFuncs = nullptr,
							   const NkVector<NkString> *projDefines = nullptr) {
			using namespace detail;
			NkCodeFontScope _cfs(ctx); // tout l'editeur dessine avec la police monospace (code)
			if (!ctx.font || !ctx.font->Face())
				return false;
			d.EnsureNonEmpty();
			++d.tick; // horloge en frames (fenêtre du multi-clic)
			// ── Repli de code : mapping lignes visibles À JOUR avant souris/scroll/rendu. ──
			d.EnsureFolds(lang);
			if (d.LineHidden(d.curLine)) { // ne jamais laisser le caret sur une ligne masquée
				d.curLine = d.NearestVisibleUp(d.curLine);
				d.ClampCursor();
				d.selLine = d.curLine;
				d.selCol = d.curCol;
			}

			// Palette THEME-AWARE (suit le thème NKCode via ctx.theme : Dark Pro/Dark/Midnight/Light).
			const NkColor kBg = ctx.theme.bgPrimary;
			const NkColor kGutterBg = ctx.theme.bgPrimary;
			const NkColor kLineNo = ctx.theme.textDisabled;
			const NkColor kLineNoCur = ctx.theme.text;
			const NkColor kText = ctx.theme.text;
			const NkColor kSel = {ctx.theme.accent.r, ctx.theme.accent.g, ctx.theme.accent.b,
								  100}; // sélection semi-transparente
			const NkColor kCurLine = {ctx.theme.textDisabled.r, ctx.theme.textDisabled.g, ctx.theme.textDisabled.b, 24};
			const NkColor kCaret = ctx.theme.text;
			const NkColor kScrollTk = ctx.theme.button;
			const NkColor kBorder = ctx.theme.accent;

			auto &dl = ctx.DL();
			const NkGuiId id = ctx.GetId(idStr);
			const NkGuiId dragId = ctx.GetId((NkString(idStr) + "#drag").CStr());
			const NkGuiId vbarId = ctx.GetId((NkString(idStr) + "#vbar").CStr());
			const NkGuiId hbarId = ctx.GetId((NkString(idStr) + "#hbar").CStr());
			const NkGuiId mmId = ctx.GetId((NkString(idStr) + "#mm").CStr());			// drag minimap
			const NkGuiId hovDragId = ctx.GetId((NkString(idStr) + "#hovdrag").CStr()); // glisser du prototype
			const bool focused = (NkCodeFocusId() == id);

			const float32 lineGap = ctx.S(5.f);						 // espace entre les lignes (interligne)
			const float32 lineH = ctx.font->LineHeight() + lineGap;	 // hauteur d'une ligne
			const float32 asc = ctx.font->Ascent() + lineGap * 0.5f; // baseline centree dans la ligne
			const float32 pad = 4.f;

			// Gouttiere : [numeros right-alignes | colonne breakpoints | bande Git 3px].
			char numbuf[16];
			std::snprintf(numbuf, sizeof(numbuf), "%d", d.LineCount());
			const float32 numAreaW = ctx.font->MeasureWidth(numbuf) + pad * 2.f;
			const float32 foldW = ctx.S(13.f); // colonne des chevrons de repli (folding)
			const float32 bpW = lineH;		   // colonne breakpoints (carree)
			const float32 gitW = 3.f;		   // bande Git au bord droit de la gouttiere
			const float32 gutterW = numAreaW + foldW + bpW + gitW;
			// Cadre regle : on RESERVE en permanence les gouttieres de scroll (V a droite,
			// H en bas) -> zone texte bornee, barres toujours visibles (facon VSCode/VS).
			const float32 sbW = 14.f;
			// Minimap (aperçu miniature à droite, façon VSCode) : TOUJOURS présente.
			const bool showMinimap = NkCodeMinimapOn(); // Ctrl+Maj+\ / palette : afficher/masquer
			const float32 mmW = ctx.S(140.f);			// largeur minimap (un peu plus large que VSCode)
			const NkRect textArea = {area.x + gutterW, area.y, area.w - gutterW - sbW - mmW, area.h - sbW};
			const float32 textLeft = textArea.x + pad;
			const float32 topPad = lineH, botPad = lineH; // ligne vierge haut + bas (non editable)
			const float32 textTop = textArea.y + topPad;  // 1re ligne decalee d'une ligne vierge
			const float32 viewH = textArea.h;
			const float32 viewW = textArea.w - pad * 2.f;

			// Fond.
			dl.AddRectFilled(area, kBg);
			dl.AddRectFilled({area.x, area.y, gutterW, area.h}, kGutterBg);

			// ── Entrees ───────────────────────────────────────────────────────────
			const NkVec2 mouse = ctx.input.mousePos;
			const bool hover = InRect(area, mouse);
			const int32 oldL = d.curLine, oldC = d.curCol; // pour detecter un mouvement du curseur

			// Molette (consommee pour ne pas scroller la fenetre dessous).
			if (hover) {
				if (ctx.input.wheel != 0.f) {
					if (ctx.input.shiftDown)
						d.scrollX -= ctx.input.wheel * 40.f;
					else
						d.scrollY -= ctx.input.wheel * lineH * 3.f;
					ctx.input.wheel = 0.f;
				}
				if (ctx.input.wheelH != 0.f) {
					d.scrollX -= ctx.input.wheelH * 40.f;
					ctx.input.wheelH = 0.f;
				}
			}

			// Clic dans la zone texte : focus + place le curseur (+ selection si Shift) + drag.
			// Ignore si un popup (ex. combo ouvert vers le haut) recouvre l'editeur -> sinon
			// le clic sur le popup volerait le focus a l'editeur.
			bool overText = InRect(textArea, mouse) && ctx.popupDepth == 0;
			if (d.findOpen &&
				ctx.font) { // la barre de recherche « mange » la souris -> pas de déplacement du caret dessous
				const float32 _rH = ctx.font->LineHeight() + ctx.S(12.f);
				const NkRect _br = {area.x + area.w - ctx.S(380.f) - ctx.S(18.f), area.y + ctx.S(8.f), ctx.S(380.f),
									_rH * (d.findReplace ? 2.f : 1.f) + ctx.S(10.f)};
				if (InRect(_br, mouse))
					overText = false;
			}
			// ── Ctrl+survol : détecte un lien navigable (chemin d'#include OU identifiant) sous
			//    la souris -> souligné + Ctrl+clic = navigation (item traité par NkCodeState). ──
			int32 linkL = -1, linkC0 = -1, linkC1 = -1;
			bool linkInc = false;
			if (ctx.input.ctrlDown && overText) {
				int32 l = d.LineAtRow(static_cast<int32>((mouse.y - textTop + d.scrollY) / lineH));
				if (l >= 0 && l < d.LineCount()) {
					const int32 c = ColAtX(ctx, d, l, mouse.x - textLeft + d.scrollX);
					const NkCodeLine &L = d.lines[l];
					const char *dd = L.Data();
					const int32 n = static_cast<int32>(L.Size());
					int32 t = 0;
					while (t < n && (dd[t] == ' ' || dd[t] == '\t'))
						++t;
					const bool isInc = (t < n && dd[t] == '#') && [&] {
						int32 k = t + 1;
						while (k < n && (dd[k] == ' ' || dd[k] == '\t'))
							++k;
						const char *w = "include";
						int32 m = 0;
						while (w[m] && k + m < n && dd[k + m] == w[m])
							++m;
						return w[m] == '\0';
					}();
					if (isInc) { // span = intérieur des guillemets ou chevrons
						int32 s = t;
						while (s < n && dd[s] != '"' && dd[s] != '<')
							++s;
						if (s < n) {
							const char close = (dd[s] == '<') ? '>' : '"';
							int32 e = s + 1;
							while (e < n && dd[e] != close)
								++e;
							if (c >= s && c <= e) {
								linkL = l;
								linkC0 = s + 1;
								linkC1 = e;
								linkInc = true;
							}
						}
					} else if (c >= 0 && c < n && NkCodeDoc::IsWChar(dd[c])) { // identifiant sous la souris
						int32 s = c;
						while (s > 0 && NkCodeDoc::IsWChar(dd[s - 1]))
							--s;
						int32 e = c;
						while (e < n && NkCodeDoc::IsWChar(dd[e]))
							++e;
						if (e > s && !(dd[s] >= '0' && dd[s] <= '9')) {
							linkL = l;
							linkC0 = s;
							linkC1 = e;
							linkInc = false;
						}
					}
				}
			}
			const bool ctrlLink = (linkL >= 0);
			if (ctrlLink && ctx.input.mouseClicked[0] &&
				overText) { // Ctrl+clic : arme la navigation, ne bouge pas le caret
				const NkCodeLine &L = d.lines[linkL];
				NkString tgt;
				for (int32 k = linkC0; k < linkC1; ++k)
					tgt += L[k];
				d.linkTarget = tgt;
				d.linkIsInclude = linkInc;
				NkCodeFocusId() = id;
			}
			// ── Hover documentation : survol IMMOBILE (~0,5 s) d'un mot -> requête (résolue par
			//    NkCodeState::ProcessHover), carte rendue plus bas. Bouge/clic/molette -> reset. ──
			// Zone-PONT mot->carte : la carte reste tant que la souris est dans la bbox
			// (mot + carte) gonflee — on peut la rejoindre sans quelle disparaisse.
			if (!d.hovShow && !d.hovReq)
				d.hovKb = false; // fin de vie de la requete clavier -> les resets souris reprennent
			bool overHovCard = false;
			// molette sur la carte : defile le prototype horizontalement (consommee)
			if (d.hovShow) {
				NkRect br = d.hovRect;
				const float32 wx0 = d.hovX, wy0 = d.hovY, wy1 = d.hovY + lineH;
				if (wx0 < br.x) {
					br.w += br.x - wx0;
					br.x = wx0;
				}
				if (wy0 < br.y) {
					br.h += br.y - wy0;
					br.y = wy0;
				}
				if (wy1 > br.y + br.h)
					br.h = wy1 - br.y;
				br.x -= 8.f;
				br.y -= 8.f;
				br.w += 16.f;
				br.h += 16.f;
				overHovCard = InRect(br, mouse);
			}
			// Glisser en cours sur le prototype -> suit la souris (relache = fin).
			if (ctx.activeId == hovDragId) {
				if (ctx.input.mouseDown[0]) {
					if (d.hovDragMode == 2) { // doc : pan X+Y
						d.hovBodyXOff = d.hovDragOff - (mouse.x - d.hovDragX);
						d.hovBodyOff = d.hovDragOffY - (mouse.y - d.hovDragY);
						if (d.hovBodyXOff < 0.f)
							d.hovBodyXOff = 0.f;
						if (d.hovBodyOff < 0.f)
							d.hovBodyOff = 0.f;
					} else { // prototype : X seul
						d.hovXOff = d.hovDragOff - (mouse.x - d.hovDragX);
						if (d.hovXOff < 0.f)
							d.hovXOff = 0.f;
					}
				} else
					ctx.activeId = NKGUI_ID_NONE;
			}
			if (overHovCard && (ctx.input.wheel != 0.f || ctx.input.wheelH != 0.f)) {
				if (InRect(d.hovBodyRect, mouse)) { // sur la DOC : molette = V, molette H/Ctrl = H
					if (ctx.input.wheelH != 0.f || ctx.input.ctrlDown)
						d.hovBodyXOff += (ctx.input.wheelH != 0.f ? -ctx.input.wheelH : -ctx.input.wheel) * 28.f;
					else
						d.hovBodyOff += -ctx.input.wheel * lineH;
					if (d.hovBodyXOff < 0.f)
						d.hovBodyXOff = 0.f;
					if (d.hovBodyOff < 0.f)
						d.hovBodyOff = 0.f;
				} else { // ailleurs sur la carte : defile le PROTOTYPE
					d.hovXOff += (ctx.input.wheelH != 0.f ? -ctx.input.wheelH : -ctx.input.wheel) * 28.f;
					if (d.hovXOff < 0.f)
						d.hovXOff = 0.f;
				}
				ctx.input.wheel = 0.f;
				ctx.input.wheelH = 0.f;
			}
			if (d.hovShow && ctx.input.KeyPressed(NkGuiKey::Escape)) { // Echap ferme la carte
				d.hovShow = false;
				d.hovDwell = 0.f;
				d.hovRect = {0.f, 0.f, 0.f, 0.f};
			}
			if (overHovCard) {
				// souris SUR la carte : on la garde (lecture / capture d ecran)
			} else if (overText && !ctx.input.mouseDown[0] && !d.acOpen && !ctx.input.ctrlDown) {
				const int32 hl = d.LineAtRow(static_cast<int32>((mouse.y - textTop + d.scrollY) / lineH));
				int32 hs = -1, he = -1;
				int32 dgi = -1; // diagnostic sous la souris ? -> carte ERREUR/AVERTISSEMENT
				if (hl >= 0 && hl < d.LineCount()) {
					const int32 hc = ColAtX(ctx, d, hl, mouse.x - textLeft + d.scrollX);
					const NkCodeLine &HL = d.lines[hl];
					const int32 hn = static_cast<int32>(HL.Size());
					for (usize di2 = 0; di2 < d.diags.Size() && dgi < 0; ++di2) {
						const NkCodeDoc::Diag &dg = d.diags[di2];
						if (dg.line != hl)
							continue;
						int32 c0 = dg.col < 0 ? 0 : dg.col;
						if (c0 > hn)
							c0 = hn;
						int32 c1 = c0;
						while (c1 < hn && NkCodeDoc::IsWChar(HL[c1]))
							++c1;
						if (c1 <= c0)
							c1 = c0 + 1;
						if (hc >= c0 - 1 && hc <= c1) { // sur la zone soulignée
							dgi = static_cast<int32>(di2);
							hs = c0;
							he = c1;
						}
					}
					if (dgi < 0 && hc >= 0 && hc < hn && NkCodeDoc::IsWChar(HL[hc])) {
						hs = hc;
						he = hc;
						while (hs > 0 && NkCodeDoc::IsWChar(HL[hs - 1]))
							--hs;
						while (he < hn && NkCodeDoc::IsWChar(HL[he]))
							++he;
						if (HL[hs] >= '0' && HL[hs] <= '9')
							hs = -1; // littéral numérique : pas de carte
					}
				}
				if (hs >= 0 && hl == d.hovWordL && hs == d.hovWordS && he == d.hovWordE) {
					d.hovDwell += ctx.input.dt;
					if (d.hovDwell >= 0.5f && !d.hovShow && !d.hovReq && !d.hovDone) {
						d.hovX = textLeft + PrefixW(ctx, d, hl, hs) - d.scrollX;
						d.hovY = textTop + d.RowOf(hl) * lineH - d.scrollY; // haut de la ligne du mot
						d.hovXOff = 0.f;
						d.hovBodyOff = 0.f;
						d.hovBodyXOff = 0.f;
						if (dgi >= 0) { // carte DIAGNOSTIC : remplie directement (pas de requête)
							const NkCodeDoc::Diag &dg = d.diags[static_cast<usize>(dgi)];
							d.hovSym = NkString();
							d.hovTitle = NkString(dg.sev ? "Erreur" : "Avertissement");
							d.hovKind = dg.sev ? 5 : 6;
							d.hovBody.Clear();
							const char *ms = dg.msg.CStr(); // message découpé (~72 colonnes)
							while (*ms && d.hovBody.Size() < 12) {
								int32 k2 = 0;
								while (ms[k2] && k2 < 72)
									++k2;
								int32 cut = k2;
								if (ms[k2]) {
									while (cut > 40 && ms[cut] != ' ')
										--cut;
									if (cut <= 40)
										cut = k2;
								}
								NkString l2;
								for (int32 t2 = 0; t2 < cut; ++t2)
									l2 += ms[t2];
								d.hovBody.PushBack(l2);
								ms += cut;
								while (*ms == ' ')
									++ms;
							}
							d.hovShow = true;
							d.hovDone = true;
						} else {
							NkString sym;
							const NkCodeLine &HL = d.lines[hl];
							for (int32 k = hs; k < he; ++k)
								sym += HL[k];
							d.hovSym = sym;
							d.hovLine = hl;
							d.hovCol = hs;
							d.hovReq = true;
						}
					}
				} else if (!d.hovKb || ctx.input.mouseClicked[0] || ctx.input.charCount > 0) {
					// mot change / plus de mot sous la souris -> reset (mode CLAVIER : seuls clic/frappe ferment)
					d.hovWordL = hs >= 0 ? hl : -1;
					d.hovWordS = hs;
					d.hovWordE = he;
					d.hovDwell = 0.f;
					d.hovShow = false;
					d.hovReq = false;
					d.hovDone = false;
					d.hovRect = {0.f, 0.f, 0.f, 0.f};
				}
			} else if ((!d.hovKb || ctx.input.mouseClicked[0] || ctx.input.charCount > 0) &&
					   (d.hovShow || d.hovDwell > 0.f || d.hovReq)) { // hors zone / clic (mode clavier : clic/frappe)
				d.hovShow = false;
				d.hovDwell = 0.f;
				d.hovWordL = -1;
				d.hovReq = false;
				d.hovRect = {0.f, 0.f, 0.f, 0.f};
			}
			// Clic sur la CARTE hover : action [Aller a la definition] (reutilise le pipeline Ctrl+clic).
			if (overHovCard && ctx.input.mouseClicked[0]) {
				if (InRect(d.hovTitleRect, mouse)) { // prise du PROTOTYPE : glisser gauche/droite = defiler
					ctx.activeId = hovDragId;
					d.hovDragX = mouse.x;
					d.hovDragOff = d.hovXOff;
					d.hovDragMode = 1;
				}
				if (InRect(d.hovBodyRect, mouse)) { // prise de la DOC : pan libre X+Y
					ctx.activeId = hovDragId;
					d.hovDragMode = 2;
					d.hovDragX = mouse.x;
					d.hovDragY = mouse.y;
					d.hovDragOff = d.hovBodyXOff;
					d.hovDragOffY = d.hovBodyOff;
				}
				if (InRect(d.hovActRect, mouse) && !d.hovSym.Empty()) {
					d.linkTarget = d.hovSym;
					d.linkIsInclude = false;
					d.hovShow = false;
					d.hovRect = {0.f, 0.f, 0.f, 0.f};
					NkCodeFocusId() = id;
				}
				if (InRect(d.hovRefsRect, mouse) && !d.hovSym.Empty()) { // [References] -> liste workspace
					d.refsTarget = d.hovSym;
					d.hovShow = false;
					d.hovRect = {0.f, 0.f, 0.f, 0.f};
					NkCodeFocusId() = id;
				}
				if (InRect(d.hovCopyRect, mouse)) { // [Copier] : titre + doc dans le presse-papiers
					NkString all = d.hovTitle;
					for (usize bi = 0; bi < d.hovBody.Size(); ++bi) {
						all += "\n";
						all += d.hovBody[bi].CStr();
					}
					ctx.SetClipboard(all.CStr());
				}
			}
			// Clic DANS le popup d autocompletion (rect de la frame precedente) : selection + insertion.
			const bool overAc = d.acOpen && InRect(d.acRect, mouse);
			if (overAc && ctx.input.mouseClicked[0]) {
				const float32 rowsY0 = d.acRect.y + ctx.S(4.f);
				const float32 rowH0 = lineH + ctx.S(2.f);
				const int32 vi = static_cast<int32>((mouse.y - rowsY0) / rowH0);
				const int32 n0 = static_cast<int32>(d.acItems.Size());
				const int32 shown0 = n0 < 9 ? n0 : 9;
				if (vi >= 0 && vi < shown0 && d.acTop + vi < n0 &&
					mouse.x < d.acRect.x + d.acRect.w - ctx.S(10.f)) { // hors barre verticale
					d.acSel = d.acTop + vi;
					d.Checkpoint(3);
					d.AcceptAutocomplete();
				}
			}
			// Un clic (n'importe où : vide, autre ligne, autre caractère) FERME l'autocomplétion.
			if (!overAc &&
				(ctx.input.mouseClicked[0] || ctx.input.mouseClicked[1] || ctx.input.mouseDoubleClicked[0]) &&
				d.acOpen) {
				d.acOpen = false;
				d.acCtxAll.Clear();
			}
			// Double-clic OS : sous Windows le 2e clic arrive en WM_LBUTTONDBLCLK (PAS un press),
			// donc jamais dans mouseClicked -> on écoute mouseDoubleClicked. Mot sous la souris ;
			// un double-clic enchaîné (3e clic rapide) escalade à la LIGNE.
			if (!ctrlLink && !overAc && !overHovCard && overText && ctx.input.mouseDoubleClicked[0] &&
				!ctx.input.altDown) {
				NkCodeFocusId() = id;
				int32 l = d.LineAtRow(static_cast<int32>((mouse.y - textTop + d.scrollY) / lineH));
				if (l < 0)
					l = 0;
				if (l >= d.LineCount())
					l = d.LineCount() - 1;
				const int32 c = ColAtX(ctx, d, l, mouse.x - textLeft + d.scrollX);
				d.extraCarets.Clear();
				d.curLine = l;
				d.lineSelAnchor = -1;
				ctx.activeId = NKGUI_ID_NONE; // pas de glisser pendant la sélection mot/ligne
				const bool chain3 =
					(d.tick - d.clkTick <= 30) && l == d.clkL && (c - d.clkC <= 1) && (d.clkC - c <= 1) && d.clkN >= 2;
				if (chain3) { // double-clic répété très vite -> ligne
					d.curCol = c;
					d.SelectCurrentLine();
					d.clkN = 3;
				} else {
					const NkCodeLine &L = d.lines[l];
					int32 ws = c, we = c;
					while (ws > 0 && NkCodeDoc::IsWChar(L[ws - 1]))
						--ws;
					while (we < static_cast<int32>(L.Size()) && NkCodeDoc::IsWChar(L[we]))
						++we;
					if (we > ws) { // sélectionne le MOT
						d.selLine = l;
						d.selCol = ws;
						d.curCol = we;
					} else {
						d.curCol = c;
						d.Collapse();
					}
					d.clkN = 2;
				}
				d.clkTick = d.tick;
				d.clkL = l;
				d.clkC = c;
			}
			if (!ctrlLink && !overAc && !overHovCard && ctx.input.mouseClicked[0] && overText &&
				!ctx.input.mouseDoubleClicked[0]) {
				NkCodeFocusId() = id;
				int32 l = d.LineAtRow(static_cast<int32>((mouse.y - textTop + d.scrollY) / lineH));
				if (l < 0)
					l = 0;
				if (l >= d.LineCount())
					l = d.LineCount() - 1;
				const int32 c = ColAtX(ctx, d, l, mouse.x - textLeft + d.scrollX);
				if (ctx.input.altDown) {
					// Alt+clic : AJOUTE un curseur à la position cliquée (multi-curseur).
					d.extraCarets.PushBack(
						{d.selLine, d.selCol, d.curLine, d.curCol}); // l'ancien primaire devient secondaire
					d.curLine = l;
					d.curCol = c;
					d.Collapse();
					d.lineSelAnchor = -1;
				} else {
					d.extraCarets.Clear(); // clic simple -> un seul curseur
					d.curLine = l;
					d.curCol = c;
					d.lineSelAnchor = -1; // clic -> réinitialise la sélection-ligne Ctrl+L
					// Multi-clic (façon VSCode) : double-clic = MOT sous le curseur, triple = LIGNE.
					const bool chain =
						(d.tick - d.clkTick <= 24) && l == d.clkL && (c - d.clkC <= 1) && (d.clkC - c <= 1);
					d.clkN = chain ? d.clkN + 1 : 1;
					d.clkTick = d.tick;
					d.clkL = l;
					d.clkC = c;
					if (d.clkN == 2 && l < d.LineCount()) { // double : sélectionne le mot
						const NkCodeLine &L = d.lines[l];
						int32 ws = c, we = c;
						while (ws > 0 && NkCodeDoc::IsWChar(L[ws - 1]))
							--ws;
						while (we < static_cast<int32>(L.Size()) && NkCodeDoc::IsWChar(L[we]))
							++we;
						if (we > ws) {
							d.selLine = l;
							d.selCol = ws;
							d.curCol = we;
						} else if (!ctx.input.shiftDown)
							d.Collapse();
					} else if (d.clkN >= 3) { // triple (et +) : la ligne entière
						d.SelectCurrentLine();
					} else {
						ctx.activeId = dragId; // simple clic : amorce le glisser-sélection
						if (!ctx.input.shiftDown)
							d.Collapse();
					}
				}
			}
			// Glisser : etend la selection.
			if (ctx.activeId == dragId && ctx.input.mouseDown[0]) {
				int32 l = d.LineAtRow(static_cast<int32>((mouse.y - textTop + d.scrollY) / lineH));
				if (l < 0)
					l = 0;
				if (l >= d.LineCount())
					l = d.LineCount() - 1;
				d.curLine = l;
				d.curCol = ColAtX(ctx, d, l, mouse.x - textLeft + d.scrollX);
			}
			// ── Clic sur un chevron de repli (colonne foldW de la gouttière) : plie/déplie. ──
			{
				const float32 foldX = area.x + numAreaW;
				const bool overFold = ctx.popupDepth == 0 && mouse.x >= foldX && mouse.x < foldX + foldW &&
									  mouse.y >= textArea.y && mouse.y < textArea.y + viewH;
				if (overFold && ctx.input.mouseClicked[0]) {
					const int32 fl = d.LineAtRow(static_cast<int32>((mouse.y - textTop + d.scrollY) / lineH));
					if (d.IsFoldHeader(fl)) {
						d.ToggleFold(fl);
						NkCodeFocusId() = id;
					}
				}
			}
			// Clic DROIT dans la zone texte : focus + menu contextuel Copier/Couper/Coller.
			if (ctx.input.mouseClicked[1] && overText) { // clic DROIT (convention [1]=droit)
				NkCodeFocusId() = id;
				NkCodeCtxMenu().open = true;
				NkCodeCtxMenu().pos = mouse;
			}
			// Clic hors zone texte mais hors editeur : perd le focus.
			if (ctx.input.mouseClicked[0] && !hover && focused)
				NkCodeFocusId() = NKGUI_ID_NONE;

			bool changed = false;
			bool noAutoReveal = false; // Ctrl+A : selection totale SANS defiler jusqu au caret
			if (focused) {
				const bool shift = ctx.input.shiftDown;
				const bool ctrl = ctx.input.ctrlDown, alt = ctx.input.altDown;
				// ── Barre de recherche/remplacement (Ctrl+F / Ctrl+H) ── ouverture + capture d'input.
				if (ctrl && !alt) {
					if (ctx.input.KeyPressed(NkGuiKey::F)) {
						d.findReplace = false;
						d.findFocus = 0;
						if (!d.findOpen && d.HasSel()) {
							int32 aL, aC, bL, bC;
							d.SelRange(aL, aC, bL, bC);
							if (aL == bL) {
								const NkCodeLine &L = d.lines[aL];
								int32 j = 0;
								for (int32 c = aC; c < bC && j < 255; ++c)
									d.findQuery[j++] = L[c];
								d.findQuery[j] = '\0';
							}
						}
						d.findOpen = true;
						d.FindRecompute();
					}
					if (ctx.input.KeyPressed(NkGuiKey::H)) {
						d.findOpen = true;
						d.findReplace = true;
						d.findFocus = 0;
						d.FindRecompute();
					}
				}
				// Capture AVANT traitement : l'Entree/Echap qui FERME une barre ne doit pas fuir
				// vers l'edition la meme frame (inserait une ligne apres Ctrl+G -> caret +1).
				const bool barWasOpen = d.findOpen || d.gotoOpen;
				if (d.gotoOpen) { // barre « Aller a la ligne » (Ctrl+G) : chiffres + Entree/Echap
					int32 gl = 0;
					while (d.gotoBuf[gl])
						++gl;
					for (int32 i = 0; i < ctx.input.charCount; ++i) {
						const uint32 cp = ctx.input.chars[i];
						if (cp >= '0' && cp <= '9' && gl < 9) {
							d.gotoBuf[gl++] = static_cast<char>(cp);
							d.gotoBuf[gl] = 0;
						}
					}
					if (ctx.input.KeyPressedRepeat(NkGuiKey::Backspace) && gl > 0)
						d.gotoBuf[--gl] = 0;
					if (ctx.input.KeyPressed(NkGuiKey::Escape))
						d.gotoOpen = false;
					if (ctx.input.KeyPressed(NkGuiKey::Enter)) {
						int32 tgt = 0;
						for (int32 i = 0; d.gotoBuf[i]; ++i)
							tgt = tgt * 10 + (d.gotoBuf[i] - '0');
						if (tgt > 0) {
							d.ResetEditRun();
							d.curLine = tgt - 1;
							d.ClampCursor();
							d.curCol = 0;
							d.Collapse();
							d.wantReveal = true;
						}
						d.gotoOpen = false;
					}
				}
				if (d.findOpen) { // la barre gèle le document ; l'ÉDITION des champs est faite par NkOverlayTextField
								  // (rendu, plus bas)
					if (ctx.input.KeyPressed(NkGuiKey::Escape))
						d.FindClose();
					if (d.findReplace && ctx.input.KeyPressedRepeat(NkGuiKey::Tab))
						d.findFocus ^= 1;
					// ↑ / ↓ (ou Entrée / Maj+Entrée) = SAUTER à l'occurrence suivante/précédente.
					if (ctx.input.KeyPressedRepeat(NkGuiKey::Up)) {
						d.FindEnsure();
						d.FindNext(false);
						changed = true;
					}
					if (ctx.input.KeyPressedRepeat(NkGuiKey::Down)) {
						d.FindEnsure();
						d.FindNext(true);
						changed = true;
					}
					if (ctx.input.KeyPressed(NkGuiKey::Enter)) {
						d.FindEnsure();
						if (d.findReplace && d.findFocus == 1)
							d.ReplaceCurrent();
						else
							d.FindNext(!shift);
						changed = true;
					}
				}
				// ── Autocomplétion ouverte : capte ↑↓ (navigue), Tab/Entrée (accepte), Échap
				//    (ferme) AVANT l'édition normale. `acEat` = touche consommée cette frame. ──
				// ── Fenetre de chord Ctrl+K (~1,5 s) : la 2e touche agit, MODIFICATEURS LIBRES
				//    (AZERTY : « 0 » = Maj+À ; Ctrl maintenu ou pas). Toute autre frappe desarme
				//    la fenetre SANS s'inserer (facon VSCode).
				bool chordEat = false;
				if (d.tick - d.chordK <= 90) {
					if (ctx.input.KeyPressed(NkGuiKey::Num0)) { // Ctrl+K (Ctrl+)0 = TOUT replier
						d.FoldAll();
						d.chordK = -100000;
						chordEat = true;
					} else if (ctx.input.KeyPressed(NkGuiKey::J)) { // Ctrl+K (Ctrl+)J = TOUT deplier
						d.UnfoldAll();
						d.chordK = -100000;
						chordEat = true;
					} else if (ctx.input.KeyPressed(NkGuiKey::I) && d.curLine < d.LineCount()) {
						// Ctrl+K (Ctrl+)I = carte hover au caret
						const NkCodeLine &L = d.lines[d.curLine];
						int32 ws = d.curCol, we = d.curCol;
						while (ws > 0 && NkCodeDoc::IsWChar(L[ws - 1]))
							--ws;
						while (we < static_cast<int32>(L.Size()) && NkCodeDoc::IsWChar(L[we]))
							++we;
						if (we > ws) {
							NkString sym;
							for (int32 k = ws; k < we; ++k)
								sym += L[k];
							d.hovSym = sym;
							d.hovLine = d.curLine;
							d.hovCol = ws;
							d.hovX = textLeft + PrefixW(ctx, d, d.curLine, ws) - d.scrollX;
							d.hovY = textTop + d.RowOf(d.curLine) * lineH - d.scrollY;
							d.hovXOff = 0.f;
							d.hovBodyOff = 0.f;
							d.hovBodyXOff = 0.f;
							d.hovShow = false;
							d.hovDone = false;
							d.hovKb = true;
							d.hovReq = true;
						}
						d.chordK = -100000;
						chordEat = true;
					} else if (ctx.input.charCount > 0 && !ctrl) { // touche inconnue -> desarme, n'insere pas
						d.chordK = -100000;
						chordEat = true;
					}
				}
				bool acEat = false, acTyped = false;
				if (!barWasOpen) { // saisie du DOCUMENT gelée tant qu'une barre est ouverte (etat de DEBUT de frame)
					if (d.acOpen && !d.acItems.Empty()) {
						const int32 acN = static_cast<int32>(d.acItems.Size());
						if (ctx.input.KeyPressedRepeat(NkGuiKey::Up)) {
							d.acSel = (d.acSel - 1 + acN) % acN;
							acEat = true;
						}
						if (ctx.input.KeyPressedRepeat(NkGuiKey::Down)) {
							d.acSel = (d.acSel + 1) % acN;
							acEat = true;
						}
						if (ctx.input.KeyPressed(NkGuiKey::Escape)) {
							d.acOpen = false;
							acEat = true;
						}
						if (!ctrl && !alt &&
							(ctx.input.KeyPressed(NkGuiKey::Enter) || ctx.input.KeyPressedRepeat(NkGuiKey::Tab))) {
							d.AcceptAutocomplete();
							acEat = true;
							changed = true;
						}
					}
					// Échap (hors autocomplétion) : réduit un multi-curseur à un seul.
					if (!acEat && d.McActive() && ctx.input.KeyPressed(NkGuiKey::Escape)) {
						d.McClear();
						acEat = true;
					}
					// Saisie texte.
					if (!ctx.input.ctrlDown && !chordEat) {
						bool typed = false;
						for (int32 i = 0; i < ctx.input.charCount; ++i) {
							const uint32 cp = ctx.input.chars[i];
							if (cp >= 32 && cp < 127) {
								const char tc0 = static_cast<char>(cp);
								// Type-over : taper ')' ']' '}' quand le MÊME caractère suit -> on
								// passe dessus au lieu de doubler (complément de l'auto-fermeture).
								if (!d.McActive() && !d.HasSel() && (tc0 == ')' || tc0 == ']' || tc0 == '}') &&
									d.curCol < d.LineLen(d.curLine) && d.lines[d.curLine][d.curCol] == tc0) {
									++d.curCol;
									d.Collapse();
									continue;
								}
								if (!typed) {
									d.Checkpoint(1);
									typed = true;
								}
								d.McType(tc0);
								changed = true;
								acTyped = true;
								// Auto-fermeture des paires : '(' '[' '{' insèrent leur fermante si le
								// caractère suivant ne s'y oppose pas (fin de ligne, blanc, fermante, , ;).
								if (!d.McActive() && (tc0 == '(' || tc0 == '[' || tc0 == '{')) {
									const int32 ll = d.LineLen(d.curLine);
									const char af = d.curCol < ll ? d.lines[d.curLine][d.curCol] : 0;
									const bool okc = !af || af == ' ' || af == '\t' || af == ')' || af == ']' ||
													 af == '}' || af == ',' || af == ';';
									if (okc) {
										d.InsertChar(tc0 == '(' ? ')' : (tc0 == '[' ? ']' : '}'));
										--d.curCol;
										d.Collapse();
									}
								}
								// Auto-fermeture des blocs commentaire : taper `/*` insere `*/`.
								if (!d.McActive() && tc0 == 0x2A && d.curCol >= 2 && d.curLine < d.LineCount() &&
									d.lines[d.curLine][d.curCol - 2] == 0x2F) {
									const int32 ll2 = d.LineLen(d.curLine);
									const char af2 = d.curCol < ll2 ? d.lines[d.curLine][d.curCol] : 0;
									if (!af2 || af2 == 0x20 || af2 == 0x09) { // rien de significatif apres
										d.InsertChar(0x2A);
										d.InsertChar(0x2F);
										d.curCol -= 2;
										d.Collapse();
									}
								}
								// Déclencheur de complétion CONTEXTUELLE : '.', '->' ou '::' (C/NKSL,
								// mono-curseur). La requête est consommée par NkCodeState (compile-first).
								if ((lang == NkLang::C || lang == NkLang::NKSL) && !d.McActive() &&
									d.curLine < d.LineCount()) {
									const char tc = static_cast<char>(cp);
									const NkCodeLine &CL = d.lines[d.curLine];
									const char pv = (d.curCol >= 2) ? CL[d.curCol - 2] : 0;
									bool trig = (tc == '>' && pv == '-') || (tc == ':' && pv == ':');
									if (tc == '.') { // '.' sauf littéral numérique (3.14) : que des chiffres avant
										int32 q = d.curCol - 2;
										while (q >= 0 && CL[q] >= '0' && CL[q] <= '9')
											--q;
										const bool numLit = (q < d.curCol - 2) && (q < 0 || !NkCodeDoc::IsWChar(CL[q]));
										trig = !numLit;
									}
									if (trig) {
										d.acCtxReq = true;
										d.acCtxLine = d.curLine;
										d.acCtxCol = d.curCol;
										d.acCtxAll.Clear();
										d.acOpen = false;
									}
								}
							}
						}
					}
					// Tab arrive par ÉVÉNEMENT TOUCHE (0x09 est filtré au niveau WM_CHAR) :
					// Tab sur sélection = indente ; Maj+Tab = désindente ; sinon 4 espaces.
					if (!acEat && !ctx.input.ctrlDown && !ctx.input.altDown &&
						ctx.input.KeyPressedRepeat(NkGuiKey::Tab)) {
						d.Checkpoint(3);
						if (ctx.input.shiftDown)
							d.IndentSelection(true);
						else if (d.HasSel())
							d.IndentSelection(false);
						else {
							for (int k = 0; k < 4; ++k)
								d.InsertChar(' ');
						}
						changed = true;
					}
					// Touches d'edition (avec repetition au maintien).
					auto K = [&](NkGuiKey k) { return ctx.input.KeyPressedRepeat(k); };
					if (!acEat && !ctrl && K(NkGuiKey::Enter)) {
						d.Checkpoint(3);
						d.McNewline();
						changed = true;
					}
					if (K(NkGuiKey::Backspace)) {
						d.Checkpoint(2);
						d.McBackspace();
						changed = true;
						acTyped = true;
					}
					if (K(NkGuiKey::Delete)) {
						d.Checkpoint(2);
						d.McDeleteFwd();
						changed = true;
					}
					if (K(NkGuiKey::Left)) {
						d.ResetEditRun();
						if (d.curCol > 0)
							--d.curCol;
						else if (d.curLine > 0) {
							--d.curLine;
							d.curCol = d.LineLen(d.curLine);
						}
						if (!shift)
							d.Collapse();
					}
					if (K(NkGuiKey::Right)) {
						d.ResetEditRun();
						if (d.curCol < d.LineLen(d.curLine))
							++d.curCol;
						else if (d.curLine < d.LineCount() - 1) {
							++d.curLine;
							d.curCol = 0;
						}
						if (!shift)
							d.Collapse();
					}
					if (!acEat && !alt && K(NkGuiKey::Up)) {
						d.ResetEditRun();
						if (d.curLine > 0) {
							int32 t = d.curLine - 1;
							while (t > 0 && d.LineHidden(t)) // saute un corps replié
								--t;
							d.curLine = t;
							d.ClampCursor();
						}
						if (!shift)
							d.Collapse();
					}
					if (!acEat && !alt && K(NkGuiKey::Down)) {
						d.ResetEditRun();
						if (d.curLine < d.LineCount() - 1) {
							int32 t = d.curLine + 1;
							while (t < d.LineCount() - 1 && d.LineHidden(t)) // saute un corps replié
								++t;
							d.curLine = t;
							d.ClampCursor();
						}
						if (!shift)
							d.Collapse();
					}
					if (K(NkGuiKey::Home)) {
						d.ResetEditRun();
						if (ctrl)
							d.curLine = 0;
						d.curCol = 0;
						if (!shift)
							d.Collapse();
					} // Ctrl+Home = début du fichier
					if (K(NkGuiKey::End)) {
						d.ResetEditRun();
						if (ctrl)
							d.curLine = d.LineCount() - 1;
						d.curCol = d.LineLen(d.curLine);
						if (!shift)
							d.Collapse();
					} // Ctrl+End = fin du fichier
					// F8 / Maj+F8 : erreur/avertissement SUIVANT / PRECEDENT (avec bouclage).
					if (!ctrl && K(NkGuiKey::F8) && !d.diags.Empty()) {
						int32 bestL = -1, bestC = -1;
						if (!shift) { // suivant : plus petit (l,c) strictement apres le caret, sinon le 1er
							for (usize i = 0; i < d.diags.Size(); ++i) {
								const int32 l2 = d.diags[i].line, c2 = d.diags[i].col < 0 ? 0 : d.diags[i].col;
								const bool after = l2 > d.curLine || (l2 == d.curLine && c2 > d.curCol);
								if (!after)
									continue;
								if (bestL < 0 || l2 < bestL || (l2 == bestL && c2 < bestC)) {
									bestL = l2;
									bestC = c2;
								}
							}
							if (bestL < 0)
								for (usize i = 0; i < d.diags.Size(); ++i) {
									const int32 l2 = d.diags[i].line, c2 = d.diags[i].col < 0 ? 0 : d.diags[i].col;
									if (bestL < 0 || l2 < bestL || (l2 == bestL && c2 < bestC)) {
										bestL = l2;
										bestC = c2;
									}
								}
						} else { // precedent : plus grand (l,c) strictement avant, sinon le dernier
							for (usize i = 0; i < d.diags.Size(); ++i) {
								const int32 l2 = d.diags[i].line, c2 = d.diags[i].col < 0 ? 0 : d.diags[i].col;
								const bool before = l2 < d.curLine || (l2 == d.curLine && c2 < d.curCol);
								if (!before)
									continue;
								if (bestL < 0 || l2 > bestL || (l2 == bestL && c2 > bestC)) {
									bestL = l2;
									bestC = c2;
								}
							}
							if (bestL < 0)
								for (usize i = 0; i < d.diags.Size(); ++i) {
									const int32 l2 = d.diags[i].line, c2 = d.diags[i].col < 0 ? 0 : d.diags[i].col;
									if (bestL < 0 || l2 > bestL || (l2 == bestL && c2 > bestC)) {
										bestL = l2;
										bestC = c2;
									}
								}
						}
						if (bestL >= 0) {
							d.ResetEditRun();
							d.curLine = bestL;
							d.ClampCursor();
							d.curCol = bestC < d.LineLen(d.curLine) ? bestC : d.LineLen(d.curLine);
							d.Collapse();
							d.wantReveal = true;
						}
					}
					// F12 : aller a la definition du mot sous le caret ; Maj+F12 : REFERENCES.
					if (!ctrl && K(NkGuiKey::F12) && d.curLine < d.LineCount()) {
						const NkCodeLine &L = d.lines[d.curLine];
						int32 ws = d.curCol, we = d.curCol;
						while (ws > 0 && NkCodeDoc::IsWChar(L[ws - 1]))
							--ws;
						while (we < static_cast<int32>(L.Size()) && NkCodeDoc::IsWChar(L[we]))
							++we;
						if (we > ws) {
							NkString sym;
							for (int32 k = ws; k < we; ++k)
								sym += L[k];
							if (shift)
								d.refsTarget = sym; // consomme par l etat -> liste des references
							else {
								d.linkTarget = sym; // pipeline go-to-def existant (Ctrl+clic)
								d.linkIsInclude = false;
							}
						}
					}
					// ── Raccourcis d'édition (Phase 4) ── (ctrl/alt définis en haut du bloc)
					if (alt && !ctrl) {
						if (K(NkGuiKey::Up)) {
							d.Checkpoint(3);
							if (shift)
								d.DuplicateLines(true);
							else
								d.MoveLines(true);
							changed = true;
						} // Alt+↑ / Maj+Alt+↑
						if (K(NkGuiKey::Down)) {
							d.Checkpoint(3);
							if (shift)
								d.DuplicateLines(false);
							else
								d.MoveLines(false);
							changed = true;
						} // Alt+↓ / Maj+Alt+↓
					}
					if (ctrl && !alt) {
						if (K(NkGuiKey::Z)) {
							if (shift)
								d.Redo();
							else
								d.Undo();
							changed = true;
						} // Ctrl+Z undo / Ctrl+Maj+Z redo
						if (K(NkGuiKey::Y)) {
							d.Redo();
							changed = true;
						} // Ctrl+Y redo
						if (!shift && K(NkGuiKey::D)) {
							d.SelectWordOrAddNext();
							changed = true;
						} // Ctrl+D = mot puis occurrence suivante (multi-curseur)
						if (shift && K(NkGuiKey::K)) {
							d.Checkpoint(3);
							d.DeleteLines();
							changed = true;
						} // Ctrl+Maj+K = supprimer ligne
						if (!shift && K(NkGuiKey::L)) {
							d.SelectCurrentLine();
						} // Ctrl+L = sélectionner ligne (étend au répété)
						if (shift && K(NkGuiKey::L)) {
							d.SelectAllOccurrences();
							changed = true;
						} // Ctrl+Maj+L = toutes les occurrences (multi-curseur)
						if (K(NkGuiKey::G)) { // Ctrl+G = aller a la ligne
							d.gotoOpen = true;
							d.gotoBuf[0] = 0;
						}
						if (K(NkGuiKey::Period)) { // Ctrl+. = QUICK FIX : actions sur les diags de la ligne
							d.qfLabels.Clear();
							d.qfL.Clear();
							d.qfC.Clear();
							d.qfKind.Clear();
							d.qfPay.Clear();
							for (usize i = 0; i < d.diags.Size() && d.qfLabels.Size() < 6; ++i) {
								if (d.diags[i].line != d.curLine)
									continue;
								const char *m = d.diags[i].msg.CStr();
								const char *dy = NkFindSub(m, "did you mean '");
								if (NkFindSub(m, "expected ';'")) {
									d.qfLabels.PushBack(NkString("Inserer « ; »"));
									d.qfL.PushBack(d.diags[i].line);
									d.qfC.PushBack(d.diags[i].col);
									d.qfKind.PushBack(0);
									d.qfPay.PushBack(NkString());
								} else if (dy) { // clang propose une correction : « did you mean 'Y' »
									NkString y;
									for (const char *q2 = dy + 14; *q2 && *q2 != 0x27; ++q2)
										y += *q2;
									if (!y.Empty()) {
										d.qfLabels.PushBack(NkString("Remplacer par « ") + y.CStr() + " »");
										d.qfL.PushBack(d.diags[i].line);
										d.qfC.PushBack(d.diags[i].col);
										d.qfKind.PushBack(1);
										d.qfPay.PushBack(y);
									}
								}
							}
							if (!d.qfLabels.Empty()) {
								NkCodeQfMenu().open = true;
								NkCodeQfMenu().pos = {textLeft + PrefixW(ctx, d, d.curLine, d.curCol) - d.scrollX,
													  textTop + (d.RowOf(d.curLine) + 1) * lineH - d.scrollY};
							}
						}
						if (!shift && K(NkGuiKey::K))
							d.chordK = d.tick; // amorce des chords Ctrl+K (fenetre traitee AVANT la saisie, plus haut)
						if (shift && K(NkGuiKey::Backslash)) { // Ctrl+Maj+\ = minimap on/off
							NkCodeMinimapOn() = !NkCodeMinimapOn();
						}
						if (shift && K(NkGuiKey::Space) && d.curLine < d.LineCount()) {
							// Ctrl+Maj+Espace : AIDE AUX PARAMETRES — prototype de l appel englobant.
							const NkCodeLine &L = d.lines[d.curLine];
							int32 q2 = d.curCol, dep = 0;
							int32 callee = -1;
							while (q2 > 0) { // '(' non appariee la plus proche a gauche
								const char c2 = L[q2 - 1];
								if (c2 == ')')
									++dep;
								else if (c2 == '(') {
									if (dep == 0) {
										callee = q2 - 1;
										break;
									}
									--dep;
								}
								--q2;
							}
							if (callee > 0) {
								int32 we2 = callee, ws2 = callee;
								while (ws2 > 0 && NkCodeDoc::IsWChar(L[ws2 - 1]))
									--ws2;
								if (we2 > ws2) {
									NkString sym;
									for (int32 k = ws2; k < we2; ++k)
										sym += L[k];
									d.hovSym = sym;
									d.hovLine = d.curLine;
									d.hovCol = ws2;
									d.hovX = textLeft + PrefixW(ctx, d, d.curLine, d.curCol) - d.scrollX;
									d.hovY = textTop + d.RowOf(d.curLine) * lineH - d.scrollY;
									d.hovXOff = 0.f;
									d.hovBodyOff = 0.f;
									d.hovBodyXOff = 0.f;
									d.hovShow = false;
									d.hovDone = false;
									d.hovKb = true;	 // requete CLAVIER : ne pas fermer sur mouvement souris
									d.hovReq = true; // ProcessHover affiche le prototype (carte)
								}
							}
						}
						if (K(NkGuiKey::Space) && d.curLine < d.LineCount()) { // Ctrl+Espace = autocomplétion
							const NkCodeLine &L = d.lines[d.curLine];
							int32 s = d.curCol;
							while (s > 0 && NkCodeDoc::IsWChar(L[s - 1]))
								--s;
							// Contexte MEMBRE ? ('.', '->' ou '::' juste avant le mot en cours)
							bool ctxTrig = false;
							if (s >= 1 && L[s - 1] == '.') {
								int32 q2 = s - 2; // pas un littéral numérique (3.14)
								while (q2 >= 0 && L[q2] >= '0' && L[q2] <= '9')
									--q2;
								ctxTrig = !((q2 < s - 2) && (q2 < 0 || !NkCodeDoc::IsWChar(L[q2])));
							} else if (s >= 2 &&
									   ((L[s - 2] == '-' && L[s - 1] == '>') || (L[s - 2] == ':' && L[s - 1] == ':')))
								ctxTrig = true;
							if (ctxTrig && (lang == NkLang::C || lang == NkLang::NKSL) && !d.McActive()) {
								d.acCtxReq = true; // heuristique + compilateur (frame suivante)
								d.acCtxLine = d.curLine;
								d.acCtxCol = s;
								d.acCtxAll.Clear();
								d.acOpen = false;
							} else { // complétion par MOTS : préfixe courant, ou TOUT si préfixe vide
								char pre[128];
								int32 pn = 0;
								for (int32 k2 = s; k2 < d.curCol && pn < 127; ++k2)
									pre[pn++] = L[k2];
								pre[pn] = 0;
								d.EnsureSymbols(lang);
								NkBuildCompletions(pre, lang, &d.symTypes, &d.symFuncs, &d.symWords, projTypes,
												   projFuncs, d.acItems);
								d.acOpen = !d.acItems.Empty();
								d.acSel = 0;
								d.acTop = 0;
								d.acXOff = 0.f;
								d.acWordCol = s;
							}
							GlobalLogBuffer().Push(
								NkString(ctxTrig ? "[ac] Ctrl+Espace: contexte membre" : "[ac] Ctrl+Espace: mots"));
						}
						if (K(NkGuiKey::Enter)) {
							d.Checkpoint(3);
							if (shift)
								d.InsertLineAbove();
							else
								d.InsertLineBelow();
							changed = true;
						} // Ctrl+Entrée / Ctrl+Maj+Entrée
						if (K(NkGuiKey::Slash)) { // Ctrl+/ = commenter la ligne ; Ctrl+Maj+/ = commentaire BLOC
							d.Checkpoint(3);
							if (shift) {
								if (lang == NkLang::C || lang == NkLang::NKSL)
									d.BlockComment("/* ", " */"); // toggle bloc intelligent
								else
									d.ToggleComment(
										CommentPrefix(lang)); // Python etc. : pas de bloc natif -> lignes (toggle sûr)
							} else
								d.ToggleComment(CommentPrefix(lang));
							changed = true;
						}
						if (!shift && K(NkGuiKey::RBracket)) {
							d.Checkpoint(3);
							d.IndentSelection(false);
							changed = true;
						} // Ctrl+] = indenter
						if (!shift && K(NkGuiKey::LBracket)) {
							d.Checkpoint(3);
							d.IndentSelection(true);
							changed = true;
						} // Ctrl+[ = désindenter
						if (shift && K(NkGuiKey::LBracket)) { // Ctrl+Maj+[ = replier la région englobant le caret
							const int32 h = d.EnclosingFoldHeader(d.curLine);
							if (h >= 0)
								d.SetFold(h, true);
						}
						if (shift &&
							K(NkGuiKey::RBracket)) { // Ctrl+Maj+] = déplier la région repliée englobant le caret
							int32 best = -1;
							for (usize k = 0; k < d.foldOn.Size(); ++k) {
								const int32 h = d.foldOn[k], e = d.FoldEndOf(h);
								if (h <= d.curLine && d.curLine <= e && h > best)
									best = h;
							}
							if (best >= 0)
								d.SetFold(best, false);
						}
					}
					// Copier / couper / coller / tout-selectionner (presse-papiers).
					if (ctx.input.wantSelectAll) {
						d.SelectAll();
						noAutoReveal = true;
					}
					if ((ctx.input.wantCopy || ctx.input.wantCut) && d.HasSel()) {
						ctx.SetClipboard(d.GetSelectedText().CStr());
						if (ctx.input.wantCut) {
							d.Checkpoint(3);
							d.EraseSelection();
							changed = true;
						}
					}
					if (ctx.input.wantPaste) {
						const NkString clip = ctx.GetClipboard();
						if (!clip.Empty()) {
							d.Checkpoint(3);
							d.McInsertText(clip.CStr());
							changed = true;
						}
					}
					// ── Recalcule l'autocomplétion après une frappe/effacement réel ──
					// Mode CONTEXTUEL d'abord : si le compilateur a fourni des membres pour ce point
					// ('.', '->', '::'), on re-filtre CETTE liste avec le préfixe tapé (aucun nouvel
					// appel compilo par lettre) ; sinon complétion par mots (types/fonctions connus).
					if (acTyped && !d.acCtxAll.Empty() && d.curLine == d.acCtxLine && d.curCol >= d.acCtxCol) {
						const NkCodeLine &L = d.lines[d.curLine];
						bool word = true;
						for (int32 k = d.acCtxCol; k < d.curCol && word; ++k)
							word = NkCodeDoc::IsWChar(L[k]);
						if (word) {
							char pre[128];
							int32 pn = 0;
							for (int32 k = d.acCtxCol; k < d.curCol && pn < 127; ++k)
								pre[pn++] = L[k];
							pre[pn] = 0;
							d.acItems.Clear();
							for (usize ii = 0; ii < d.acCtxAll.Size(); ++ii) {
								const char *nm = d.acCtxAll[ii].CStr();
								int32 m = 0;
								while (pre[m] && nm[m] && (nm[m] | 32) == (pre[m] | 32))
									++m;
								if (!pre[m])
									d.acItems.PushBack(d.acCtxAll[ii]);
							}
							d.acOpen = !d.acItems.Empty();
							d.acSel = 0;
							d.acWordCol = d.acCtxCol;
						} else {
							d.acCtxAll.Clear(); // le préfixe n'est plus un identifiant -> fin du mode membre
							d.acOpen = false;
						}
					} else if (acTyped && lang != NkLang::None && d.curLine < d.LineCount()) {
						if (!d.acCtxAll.Empty())
							d.acCtxAll.Clear(); // caret sorti du contexte membre
						const NkCodeLine &L = d.lines[d.curLine];
						int32 s = d.curCol;
						while (s > 0 && NkCodeDoc::IsWChar(L[s - 1]))
							--s;
						const bool startsId = s < static_cast<int32>(L.Size()) &&
											  (((L[s] | 32) >= 'a' && (L[s] | 32) <= 'z') || L[s] == '_');
						const int32 wl = d.curCol - s;
						if (wl >= 1 && startsId) {
							char pre[128];
							int32 pn = 0;
							for (int32 k = s; k < d.curCol && pn < 127; ++k)
								pre[pn++] = L[k];
							pre[pn] = 0;
							d.EnsureSymbols(lang); // symboles du fichier à jour
							NkBuildCompletions(pre, lang, &d.symTypes, &d.symFuncs, &d.symWords, projTypes, projFuncs,
											   d.acItems);
							d.acOpen = !d.acItems.Empty();
							d.acSel = 0;
							d.acWordCol = s;
						} else
							d.acOpen = false;
					}
				} // ferme if (!d.findOpen) : saisie document gelée quand la barre de recherche est ouverte
			}
			d.ClampCursor();

			// Repli : ré-applique les bascules de repli faites CETTE frame (clic gouttière / Ctrl+Maj+[/]).
			d.EnsureFolds(lang);
			if (d.LineHidden(d.curLine)) {
				d.curLine = d.NearestVisibleUp(d.curLine);
				d.ClampCursor();
				d.selLine = d.curLine;
				d.selCol = d.curCol;
			}
			// ── Largeur max GLOBALE (cache) -> barre H stable, independante du scroll ──
			const float32 contentH = d.VisRowCount() * lineH + topPad + botPad;
			if (d.widthDirty) {
				float32 mw = 0.f;
				for (usize i = 0; i < d.lines.Size(); ++i) {
					const NkCodeLine &ln = d.lines[i];
					if (ln.Size() == 0)
						continue;
					const float32 w = ctx.font->Face()->CalcTextSizeX(ln.Data(), ln.Data() + ln.Size());
					if (w > mw)
						mw = w;
				}
				d.maxLineWCache = mw;
				d.widthDirty = false;
			}
			const float32 maxLineW = d.maxLineWCache;

			// Auto-scroll : garde le caret dans la vue UNIQUEMENT s'il vient de bouger
			// (clic/clavier/edition) -> ne combat pas le scroll molette/barre.
			const bool ensureCaret =
				!noAutoReveal && (d.curLine != oldL || d.curCol != oldC || changed || d.wantReveal);
			if (ensureCaret) {
				const float32 cX = PrefixW(ctx, d, d.curLine, d.curCol);
				const float32 cY = topPad + d.RowOf(d.curLine) * lineH; // repli : position VISUELLE du caret
				if (d.wantReveal) {
					d.scrollY = cY - viewH * 0.4f;
					if (d.scrollY < 0.f)
						d.scrollY = 0.f;
					d.wantReveal = false;
				} // clic Outline -> centre
				if (cY < d.scrollY)
					d.scrollY = cY;
				if (cY + lineH > d.scrollY + viewH)
					d.scrollY = cY + lineH - viewH;
				if (cX < d.scrollX)
					d.scrollX = cX;
				if (cX + 2.f > d.scrollX + viewW)
					d.scrollX = cX + 2.f - viewW;
			}
			const float32 maxScrollY = contentH > viewH ? contentH - viewH : 0.f;
			const float32 maxScrollX = maxLineW > viewW ? maxLineW - viewW : 0.f;
			if (d.scrollY < 0.f)
				d.scrollY = 0.f;
			if (d.scrollY > maxScrollY)
				d.scrollY = maxScrollY;
			if (d.scrollX < 0.f)
				d.scrollX = 0.f;
			if (d.scrollX > maxScrollX)
				d.scrollX = maxScrollX;

			// ── Rendu des lignes visibles ─────────────────────────────────────────
			int32 aL, aC, bL, bC;
			d.SelRange(aL, aC, bL, bC);
			int32 firstVis = static_cast<int32>((d.scrollY - topPad) / lineH);
			if (firstVis < 0)
				firstVis = 0;
			const int32 lastVis = firstVis + static_cast<int32>(viewH / lineH) + 2;
			// Repli : 1re ligne DOC affichée au row visuel `firstVis` (mapping lignes visibles).
			const int32 startDoc = d.LineAtRow(firstVis);

			// Surlignage de la ligne courante (toute la zone texte).
			if (focused) {
				const float32 y = textTop + d.RowOf(d.curLine) * lineH - d.scrollY;
				if (y + lineH > textArea.y && y < textArea.y + textArea.h)
					dl.AddRectFilled({textArea.x, y, textArea.w, lineH}, kCurLine);
			}

			// Etat bloc-commentaire (/* .. */) AU DEBUT de la 1re ligne visible :
			// scan prefixe [0, firstVis) (C uniquement ; sinon jamais en bloc).
			const NkSynColors &syn = ctx.syntax; // couleurs editables via Preferences > Langages
			const NkFont *face = ctx.font->Face();
			const uint32 tex = ctx.font->TexId();
			d.EnsureSymbols(lang); // index sémantique (types/fonctions du fichier) — rebuild paresseux
			d.EnsurePreproc(lang,
							projDefines); // régions préproc inactives (grisées) — après EnsureSymbols (symSig à jour)
			int32 inBlock = 0;			  // état de bloc multi-lignes (0 aucun, 1 /*..*/ ou ```, 2 py """, 3 py ''')
			if (lang != NkLang::None)
				for (int32 i = 0; i < startDoc && i < d.LineCount(); ++i)
					inBlock = TokenizeLine(lang, d.lines[i].Data(), static_cast<int32>(d.lines[i].Size()), inBlock, syn,
										   [](int32, int32, const NkColor &) {});

			// ── Guides d'indentation + bracket matching (surcouches subtiles) ──
			const float32 chW = ctx.font->MeasureWidth("0");
			const int32 tabSize = 4;
			const bool edLight = ((int32)ctx.theme.bgPrimary.r + ctx.theme.bgPrimary.g + ctx.theme.bgPrimary.b) > 384;
			const int32 bgR = ctx.theme.bgPrimary.r, bgG = ctx.theme.bgPrimary.g,
						bgB = ctx.theme.bgPrimary.b; // pour atténuer les branches préproc inactives
			const NkColor kGuide = edLight ? NkColor{0, 0, 0, 28} : NkColor{255, 255, 255, 24};
			const NkColor kBracket = {ctx.theme.accent.r, ctx.theme.accent.g, ctx.theme.accent.b, 70};
			// Paire d'accolades/parenthèses correspondante quand le curseur en est adjacent.
			int32 bl1 = -1, bc1 = -1, bl2 = -1, bc2 = -1;
			if (focused) {
				auto chAt = [&](int32 l, int32 c) -> char {
					if (l < 0 || l >= d.LineCount())
						return 0;
					const NkCodeLine &L = d.lines[l];
					return (c >= 0 && c < (int32)L.Size()) ? L[c] : 0;
				};
				auto isOpen = [](char c) { return c == '(' || c == '[' || c == '{'; };
				auto isClose = [](char c) { return c == ')' || c == ']' || c == '}'; };
				int32 pc = -1;
				char bc = 0;
				const char lc = chAt(d.curLine, d.curCol - 1), rc = chAt(d.curLine, d.curCol);
				if (isOpen(lc) || isClose(lc)) {
					pc = d.curCol - 1;
					bc = lc;
				} else if (isOpen(rc) || isClose(rc)) {
					pc = d.curCol;
					bc = rc;
				}
				if (pc >= 0) {
					bl1 = d.curLine;
					bc1 = pc;
					if (isOpen(bc)) {
						int32 l = d.curLine, c = pc + 1, depth = 1, g = 0;
						while (l < d.LineCount() && g++ < 300000) {
							const int32 sz = (int32)d.lines[l].Size();
							if (c >= sz) {
								++l;
								c = 0;
								continue;
							}
							const char cc = d.lines[l][c];
							if (isOpen(cc))
								++depth;
							else if (isClose(cc)) {
								if (--depth == 0) {
									bl2 = l;
									bc2 = c;
									break;
								}
							}
							++c;
						}
					} else {
						int32 l = d.curLine, c = pc - 1, depth = 1, g = 0;
						while (l >= 0 && g++ < 300000) {
							if (c < 0) {
								if (--l < 0)
									break;
								c = (int32)d.lines[l].Size() - 1;
								continue;
							}
							const char cc = (c < (int32)d.lines[l].Size()) ? d.lines[l][c] : 0;
							if (isClose(cc))
								++depth;
							else if (isOpen(cc)) {
								if (--depth == 0) {
									bl2 = l;
									bc2 = c;
									break;
								}
							}
							--c;
						}
					}
				}
			}

			// ── Occurrences de la SELECTION (facon VSCode) : selection MONO-LIGNE non vide ->
			//    chaque correspondance exacte recoit un fond discret (plus fin que la selection). ──
			char selTxt[65];
			int32 selTxtLen = 0, selOccLine = -1, selOccCol = -1;
			{
				int32 sA, sB, sC, sD;
				d.SelRange(sA, sB, sC, sD);
				if (d.HasSel() && sA == sC && sD > sB && sD - sB <= 64) {
					const NkCodeLine &SL = d.lines[sA];
					bool onlySpace = true;
					for (int32 c = sB; c < sD; ++c) {
						selTxt[selTxtLen++] = SL[c];
						if (SL[c] != ' ' && SL[c] != '	')
							onlySpace = false;
					}
					if (onlySpace)
						selTxtLen = 0;
					else {
						selOccLine = sA;
						selOccCol = sB;
					}
				}
				selTxt[selTxtLen] = 0;
			}
			dl.PushClipRect(textArea, true);
			for (int32 i = startDoc; i < d.LineCount(); ++i) {
				const int32 vrow = d.RowOf(i);
				if (vrow < 0) { // ligne repliée (masquée) : garde l'état de bloc-commentaire, ne dessine pas
					if (lang != NkLang::None)
						inBlock = TokenizeLine(lang, d.lines[i].Data(), static_cast<int32>(d.lines[i].Size()), inBlock,
											   syn, [](int32, int32, const NkColor &) {});
					continue;
				}
				if (vrow > lastVis)
					break;
				const float32 y = textTop + vrow * lineH - d.scrollY;
				const float32 baseline = y + asc;
				const NkCodeLine &ln = d.lines[i];
				const int32 n = static_cast<int32>(ln.Size());

				// Guides d'indentation : une ligne verticale par niveau d'indentation.
				{
					int32 ind = 0;
					const char *dd = ln.Data();
					for (int32 k = 0; k < n; ++k) {
						if (dd[k] == ' ')
							++ind;
						else if (dd[k] == '\t')
							ind += tabSize;
						else
							break;
					}
					for (int32 col = 0; col < ind; col += tabSize) {
						const float32 gx = textLeft + col * chW - d.scrollX;
						if (gx >= textArea.x - 1.f && gx < textArea.x + textArea.w)
							dl.AddLine({gx, y}, {gx, y + lineH}, kGuide, 1.f);
					}
				}
				// Surlignage bracket matching (les deux extrémités).
				if (i == bl1 && bc1 >= 0)
					dl.AddRectFilled({textLeft + PrefixW(ctx, d, i, bc1) - d.scrollX, y, chW, lineH}, kBracket, 2.f);
				if (i == bl2 && bc2 >= 0)
					dl.AddRectFilled({textLeft + PrefixW(ctx, d, i, bc2) - d.scrollX, y, chW, lineH}, kBracket, 2.f);

				// Occurrences de la selection : fond FIN sous le texte (la vraie selection reste pleine).
				if (selTxtLen > 0 && n >= selTxtLen) {
					const char *dd2 = ln.Data();
					for (int32 c = 0; c + selTxtLen <= n; ++c) {
						bool m2 = true;
						for (int32 t = 0; t < selTxtLen; ++t)
							if (dd2[c + t] != selTxt[t]) {
								m2 = false;
								break;
							}
						if (!m2)
							continue;
						if (i == selOccLine && c == selOccCol) { // la selection elle-meme
							c += selTxtLen - 1;
							continue;
						}
						const float32 ox0 = textLeft + PrefixW(ctx, d, i, c) - d.scrollX;
						const float32 ox1 = textLeft + PrefixW(ctx, d, i, c + selTxtLen) - d.scrollX;
						dl.AddRectFilled({ox0, y, ox1 - ox0, lineH},
										 NkColor{ctx.theme.accent.r, ctx.theme.accent.g, ctx.theme.accent.b, 34}, 2.f);
						c += selTxtLen - 1;
					}
				}
				// Selection sur cette ligne.
				if (d.HasSel() && i >= aL && i <= bL) {
					const int32 c0 = (i == aL) ? aC : 0;
					const int32 c1 = (i == bL) ? bC : n;
					float32 x0 = textLeft + PrefixW(ctx, d, i, c0) - d.scrollX;
					float32 x1 = textLeft + PrefixW(ctx, d, i, c1) - d.scrollX;
					if (i < bL)
						x1 += 4.f; // marque le saut de ligne
					dl.AddRectFilled({x0, y, x1 - x0, lineH}, kSel);
				}
				// Texte COLORE : tokenise la ligne et dessine chaque plage (curseur x
				// incremental). Appel meme si n==0 pour propager l'etat de bloc.
				const char *data = ln.Data();
				float32 sx = textLeft - d.scrollX;
				const bool dim = d.InactiveAt(i); // branche préproc morte -> texte atténué vers le fond
				inBlock = TokenizeLine(
					lang, data, n, inBlock, syn,
					[&](int32 a, int32 b, const NkColor &col) {
						NkColor c = col;
						if (dim) {
							c.r = static_cast<uint8>((col.r * 42 + bgR * 58) / 100);
							c.g = static_cast<uint8>((col.g * 42 + bgG * 58) / 100);
							c.b = static_cast<uint8>((col.b * 42 + bgB * 58) / 100);
						}
						sx = NkDrawTextU(ctx, sx, baseline, y, lineH, data + a, data + b,
										 c); // box-drawing en primitives
					},
					&d.symTypes, &d.symFuncs, projTypes, projFuncs); // coloration sémantique (fichier + projet)
				// Repli : badge « … » en fin de ligne d'en-tête repliée (signale du code masqué).
				if (d.FoldedAt(i)) {
					const float32 bx = sx + chW * 0.6f;
					const NkRect badge = {bx, y + 2.f, chW * 2.6f, lineH - 4.f};
					dl.AddRectFilled(
						badge,
						NkColor{ctx.theme.textDisabled.r, ctx.theme.textDisabled.g, ctx.theme.textDisabled.b, 46}, 3.f);
					dl.AddText(face, tex, {bx + chW * 0.5f, baseline}, "...", ctx.theme.textDisabled);
				}
			}
			// ── Diagnostics : soulignement ondulé (rouge=erreur / jaune=warning) + message
			//    inline en fin de ligne (style « Error Lens » de VS Code). ──
			{
				auto isWd = [](char c) {
					return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
				};
				for (usize di = 0; di < d.diags.Size(); ++di) {
					const NkCodeDoc::Diag &dg = d.diags[di];
					if (dg.line < 0 || dg.line >= d.LineCount() || d.LineHidden(dg.line))
						continue;
					const int32 drow = d.RowOf(dg.line);
					if (drow < firstVis || drow > lastVis)
						continue;
					const NkColor col = dg.sev ? NkColor{240, 80, 80, 255} : NkColor{224, 190, 70, 255};
					const float32 yy = textTop + drow * lineH - d.scrollY;
					const NkCodeLine &ln = d.lines[dg.line];
					int32 c0 = dg.col;
					if (c0 < 0)
						c0 = 0;
					if (c0 > (int32)ln.Size())
						c0 = (int32)ln.Size();
					int32 c1 = c0;
					while (c1 < (int32)ln.Size() && isWd(ln[c1]))
						++c1;
					if (c1 <= c0)
						c1 = (c0 < (int32)ln.Size()) ? c0 + 1 : c0;
					float32 x0 = textLeft + PrefixW(ctx, d, dg.line, c0) - d.scrollX;
					float32 x1 = textLeft + PrefixW(ctx, d, dg.line, c1) - d.scrollX;
					if (x1 < x0 + chW)
						x1 = x0 + chW;
					const float32 wy = yy + lineH - 2.5f;
					for (float32 x = x0; x < x1; x += 4.f) {
						dl.AddLine({x, wy}, {x + 2.f, wy - 2.f}, col, 1.1f);
						dl.AddLine({x + 2.f, wy - 2.f}, {x + 4.f, wy}, col, 1.1f);
					}
					// Message inline « Error Lens » : UNE seule annotation par ligne (la plus sévère,
					// sinon la première) + tronquée — plusieurs diagnostics ne s'écrasent plus.
					bool best = true;
					for (usize dj = 0; dj < d.diags.Size() && best; ++dj) {
						if (dj == di)
							continue;
						const NkCodeDoc::Diag &og = d.diags[dj];
						if (og.line != dg.line)
							continue;
						if (og.sev > dg.sev || (og.sev == dg.sev && dj < di))
							best = false;
					}
					if (best && ctx.font && ctx.font->Valid()) {
						char mb[144];
						int32 mk = 0;
						const char *ms = dg.msg.CStr();
						while (ms[mk] && mk < 137) {
							mb[mk] = ms[mk];
							++mk;
						}
						if (ms[mk]) {
							mb[mk++] = '.';
							mb[mk++] = '.';
							mb[mk++] = '.';
						}
						mb[mk] = 0;
						const float32 endX =
							textLeft + PrefixW(ctx, d, dg.line, (int32)ln.Size()) - d.scrollX + chW * 2.f;
						dl.AddText(ctx.font->Face(), ctx.font->TexId(), {endX, yy + asc}, mb,
								   NkColor{col.r, col.g, col.b, 165});
					}
				}
			}
			// Ctrl+survol : souligne le lien navigable (style VSCode) sous la souris.
			if (ctrlLink && !d.LineHidden(linkL) && d.RowOf(linkL) >= firstVis && d.RowOf(linkL) <= lastVis) {
				const float32 ux = textLeft + PrefixW(ctx, d, linkL, linkC0) - d.scrollX;
				const float32 ux2 = textLeft + PrefixW(ctx, d, linkL, linkC1) - d.scrollX;
				const float32 uy = textTop + d.RowOf(linkL) * lineH - d.scrollY + lineH - 2.f;
				dl.AddLine({ux, uy}, {ux2, uy}, ctx.theme.accent, 1.2f);
			}
			// Caret : clignote (~0,5 s) ; frappe ou deplacement le RALLUME (facon VSCode).
			if (d.curLine != d.blinkL || d.curCol != d.blinkC || changed) {
				d.blinkL = d.curLine;
				d.blinkC = d.curCol;
				d.blinkTick = d.tick;
			}
			const bool caretOn = (((d.tick - d.blinkTick) / 32) % 2) == 0;
			if (focused && caretOn) {
				const float32 cx = textLeft + PrefixW(ctx, d, d.curLine, d.curCol) - d.scrollX;
				const float32 cy = textTop + d.RowOf(d.curLine) * lineH - d.scrollY;
				dl.AddLine({cx, cy + 1.f}, {cx, cy + lineH - 1.f}, kCaret, 1.5f);
			}
			// Curseurs SECONDAIRES (multi-curseur Ctrl+D / Alt+clic) : sélection + caret pour chacun.
			if (focused)
				for (usize e = 0; e < d.extraCarets.Size(); ++e) {
					NkCodeDoc::Caret c = d.extraCarets[e];
					if (c.cl < 0 || c.cl >= d.LineCount() || d.LineHidden(c.cl)) // repli : ignore les masqués
						continue;
					const int32 crow = d.RowOf(c.cl);
					const int32 llen = d.LineLen(c.cl);
					if (c.cc > llen)
						c.cc = llen;
					if (c.sc > llen)
						c.sc = llen;
					if (c.sl == c.cl && c.sc != c.cc) { // sélection (mono-ligne)
						const int32 lo = c.sc < c.cc ? c.sc : c.cc, hi = c.sc < c.cc ? c.cc : c.sc;
						const float32 sx0 = textLeft + PrefixW(ctx, d, c.cl, lo) - d.scrollX;
						const float32 sx1 = textLeft + PrefixW(ctx, d, c.cl, hi) - d.scrollX;
						dl.AddRectFilled({sx0, textTop + crow * lineH - d.scrollY, sx1 - sx0, lineH}, kSel);
					}
					const float32 ecx = textLeft + PrefixW(ctx, d, c.cl, c.cc) - d.scrollX;
					const float32 ecy = textTop + crow * lineH - d.scrollY;
					if (caretOn)
						dl.AddLine({ecx, ecy + 1.f}, {ecx, ecy + lineH - 1.f}, kCaret, 1.5f);
				}
			dl.PopClipRect();

			// ── Popup d'autocomplétion (façon VSCode) : liste sous le mot. Clavier (↑↓ · Tab/
			//    Entrée accepter · Échap fermer) ET souris (clic = insérer, molette = défiler,
			//    barres V/H). Son rect est mémorisé (d.acRect) pour router les clics de la frame
			//    suivante : un clic DANS le popup ne le ferme pas et n'atteint pas l'éditeur. ──
			if (focused && d.acOpen && !d.acItems.Empty() && ctx.font && ctx.font->Valid()) {
				const int32 n = static_cast<int32>(d.acItems.Size());
				const int32 shown = n < 9 ? n : 9;
				if (d.acSel < 0)
					d.acSel = 0;
				if (d.acSel >= n)
					d.acSel = n - 1;
				if (d.acSel < d.acTop)
					d.acTop = d.acSel;
				if (d.acSel >= d.acTop + shown)
					d.acTop = d.acSel - shown + 1;
				if (d.acTop > n - shown)
					d.acTop = n - shown;
				if (d.acTop < 0)
					d.acTop = 0;
				float32 maxW = 0.f;
				for (int32 i = 0; i < n; ++i) {
					const float32 w = ctx.font->MeasureWidth(d.acItems[static_cast<usize>(i)].CStr());
					if (w > maxW)
						maxW = w;
				}
				const float32 padX = ctx.S(10.f);
				const float32 sbT = ctx.S(7.f); // épaisseur des barres de défilement du popup
				float32 pw = maxW + padX * 2.f + sbT + ctx.S(10.f);
				if (pw < ctx.S(200.f))
					pw = ctx.S(200.f);
				if (pw > ctx.S(480.f))
					pw = ctx.S(480.f);
				const float32 rowH = lineH + ctx.S(2.f);
				const float32 innerW = pw - padX * 2.f - sbT;
				const bool hBar = maxW > innerW;
				const float32 ph = shown * rowH + ctx.S(8.f) + (hBar ? sbT + ctx.S(2.f) : 0.f);
				const float32 px = textLeft + PrefixW(ctx, d, d.curLine, d.acWordCol) - d.scrollX;
				float32 py = textTop + (d.RowOf(d.curLine) + 1) * lineH - d.scrollY;
				if (py + ph > area.y + area.h)
					py = textTop + d.RowOf(d.curLine) * lineH - d.scrollY - ph; // au-dessus si ça déborde
				const NkRect box = {px, py, pw, ph};
				// Ombre portée (2 couches) + panneau arrondi + liseré accent discret.
				dl.AddRectFilled({box.x + 4.f, box.y + 5.f, box.w, box.h}, NkColor{0, 0, 0, 46}, 8.f);
				dl.AddRectFilled({box.x + 2.f, box.y + 2.f, box.w, box.h}, NkColor{0, 0, 0, 60}, 7.f);
				dl.AddRectFilled(box, NkColor{ctx.theme.accent.r, ctx.theme.accent.g, ctx.theme.accent.b, 90}, 6.f);
				dl.AddRectFilled({box.x + 1.f, box.y + 1.f, box.w - 2.f, box.h - 2.f}, ctx.theme.header, 6.f);
				const float32 rowsY = box.y + ctx.S(4.f);
				const float32 rowsW = pw - ctx.S(4.f) - sbT;
				const NkColor selBg = {ctx.theme.accent.r, ctx.theme.accent.g, ctx.theme.accent.b, 80};
				const NkColor hovBg = {ctx.theme.text.r, ctx.theme.text.g, ctx.theme.text.b, 14};
				// Molette au-dessus du popup : défile la liste (V) / le texte (H). Consommée.
				const bool overPopup = InRect(box, mouse);
				if (overPopup && ctx.input.wheel != 0.f) {
					d.acTop -= static_cast<int32>(ctx.input.wheel);
					if (d.acTop > n - shown)
						d.acTop = n - shown;
					if (d.acTop < 0)
						d.acTop = 0;
					ctx.input.wheel = 0.f;
				}
				if (overPopup && ctx.input.wheelH != 0.f) {
					d.acXOff -= ctx.input.wheelH * 24.f;
					ctx.input.wheelH = 0.f;
				}
				const float32 xMaxOff = maxW > innerW ? maxW - innerW : 0.f;
				if (d.acXOff < 0.f)
					d.acXOff = 0.f;
				if (d.acXOff > xMaxOff)
					d.acXOff = xMaxOff;
				dl.PushClipRect({box.x + 2.f, rowsY, rowsW, shown * rowH}, true);
				for (int32 vi = 0; vi < shown; ++vi) {
					const int32 i = d.acTop + vi;
					if (i >= n)
						break;
					const NkRect row = {box.x + 2.f, rowsY + vi * rowH, rowsW, rowH};
					const bool hov = InRect(row, mouse);
					if (i == d.acSel) {
						dl.AddRectFilled(row, selBg, 3.f);
						dl.AddRectFilled({row.x, row.y, 3.f, row.h}, ctx.theme.accent); // barre accent
					} else if (hov)
						dl.AddRectFilled(row, hovBg, 3.f);
					dl.AddText(ctx.font->Face(), ctx.font->TexId(), {row.x + padX - d.acXOff, row.y + asc + 1.f},
							   d.acItems[static_cast<usize>(i)].CStr(), ctx.theme.text);
				}
				dl.PopClipRect();
				// Barre VERTICALE (liste plus longue que la fenêtre du popup).
				if (n > shown) {
					const NkRect vtr = {box.x + pw - sbT - 2.f, rowsY, sbT, shown * rowH};
					dl.AddRectFilled(vtr, NkColor{255, 255, 255, 14}, 3.f);
					float32 thH = vtr.h * (static_cast<float32>(shown) / static_cast<float32>(n));
					if (thH < ctx.S(18.f))
						thH = ctx.S(18.f);
					const float32 thY =
						vtr.y + (vtr.h - thH) * (n - shown > 0 ? static_cast<float32>(d.acTop) / (n - shown) : 0.f);
					dl.AddRectFilled({vtr.x + 1.f, thY, sbT - 2.f, thH}, NkColor{150, 158, 170, 200}, 3.f);
					if (InRect(vtr, mouse) && ctx.input.mouseDown[0]) { // clic/glisser -> position
						float32 fr = (mouse.y - vtr.y - thH * 0.5f) / (vtr.h - thH > 1.f ? vtr.h - thH : 1.f);
						if (fr < 0.f)
							fr = 0.f;
						if (fr > 1.f)
							fr = 1.f;
						d.acTop = static_cast<int32>(fr * (n - shown) + 0.5f);
					}
				}
				// Barre HORIZONTALE (élément plus large que la fenêtre du popup).
				if (hBar) {
					const NkRect htr = {box.x + 2.f, box.y + ph - sbT - 2.f, rowsW, sbT};
					dl.AddRectFilled(htr, NkColor{255, 255, 255, 14}, 3.f);
					float32 thW = htr.w * (innerW / maxW);
					if (thW < ctx.S(22.f))
						thW = ctx.S(22.f);
					const float32 thX = htr.x + (htr.w - thW) * (xMaxOff > 0.f ? d.acXOff / xMaxOff : 0.f);
					dl.AddRectFilled({thX, htr.y + 1.f, thW, sbT - 2.f}, NkColor{150, 158, 170, 200}, 3.f);
					if (InRect(htr, mouse) && ctx.input.mouseDown[0]) {
						float32 fr = (mouse.x - htr.x - thW * 0.5f) / (htr.w - thW > 1.f ? htr.w - thW : 1.f);
						if (fr < 0.f)
							fr = 0.f;
						if (fr > 1.f)
							fr = 1.f;
						d.acXOff = fr * xMaxOff;
					}
				}
				d.acRect = box; // pour router les clics de la frame suivante (insertion au clic)
			} else
				d.acRect = {0.f, 0.f, 0.f, 0.f};

			// ── Barre « Aller a la ligne » (Ctrl+G) : boite compacte en haut a droite. ──
			if (d.gotoOpen && ctx.font && ctx.font->Valid()) {
				const float32 gh = ctx.font->LineHeight() + ctx.S(12.f);
				const NkRect gb = {area.x + area.w - ctx.S(240.f) - ctx.S(18.f), area.y + ctx.S(8.f), ctx.S(240.f),
								   gh + ctx.S(6.f)};
				dl.AddRectFilled({gb.x + 3.f, gb.y + 4.f, gb.w, gb.h}, NkColor{0, 0, 0, 50}, 7.f);
				dl.AddRectFilled(gb, NkColor{ctx.theme.accent.r, ctx.theme.accent.g, ctx.theme.accent.b, 90}, 6.f);
				dl.AddRectFilled({gb.x + 1.f, gb.y + 1.f, gb.w - 2.f, gb.h - 2.f}, ctx.theme.header, 6.f);
				const float32 by2 = gb.y + (gb.h - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent();
				dl.AddText(face, tex, {gb.x + ctx.S(10.f), by2}, "Ligne :", ctx.theme.textDisabled);
				const float32 lx = gb.x + ctx.S(10.f) + ctx.font->MeasureWidth("Ligne :") + ctx.S(8.f);
				dl.AddText(face, tex, {lx, by2}, d.gotoBuf, ctx.theme.text);
				// caret clignotant au bout du nombre
				if ((d.tick / 30) % 2 == 0) {
					const float32 cx2 = lx + ctx.font->MeasureWidth(d.gotoBuf) + 1.f;
					dl.AddLine({cx2, by2 - ctx.font->Ascent() + 2.f}, {cx2, by2 + ctx.S(3.f)}, ctx.theme.text, 1.2f);
				}
			}
			// ── Carte HOVER (signature + doc) : au-dessus de la ligne si la place le permet. ──
			if (d.hovShow && ctx.font && ctx.font->Valid()) {
				const float32 padX = ctx.S(10.f), padY = ctx.S(6.f);
				float32 w = ctx.font->MeasureWidth(d.hovTitle.CStr());
				for (usize i = 0; i < d.hovBody.Size(); ++i) {
					const float32 lw = ctx.font->MeasureWidth(d.hovBody[i].CStr());
					if (lw > w)
						w = lw;
				}
				w += padX * 2.f;
				if (w > ctx.S(560.f))
					w = ctx.S(560.f);
				if (w < ctx.S(430.f)) // assez large pour les 3 boutons d action
					w = ctx.S(430.f);
				const int32 nb = static_cast<int32>(d.hovBody.Size());
				const int32 shownB = nb > 6 ? 6 : nb; // doc : fenetre de 6 lignes max (scroll V)
				const float32 hh = padY * 2.f + lineH * static_cast<float32>(2 + shownB) + ctx.S(20.f) +
								   (nb > 0 ? ctx.S(8.f) : 0.f);		// + rangée d action
				float32 cx = d.hovX, cy = d.hovY - hh - ctx.S(4.f); // au-dessus du mot
				if (cy < area.y + 2.f)
					cy = d.hovY + lineH + ctx.S(4.f); // sinon en dessous
				if (cx + w > area.x + area.w - sbW)
					cx = area.x + area.w - sbW - w - 2.f;
				if (cx < area.x + 2.f)
					cx = area.x + 2.f;
				const NkRect hb = {cx, cy, w, hh};
				dl.AddRectFilled({hb.x + 3.f, hb.y + 4.f, hb.w, hb.h}, NkColor{0, 0, 0, 50}, 7.f); // ombre
				dl.AddRectFilled(hb, NkColor{ctx.theme.accent.r, ctx.theme.accent.g, ctx.theme.accent.b, 80}, 6.f);
				dl.AddRectFilled({hb.x + 1.f, hb.y + 1.f, hb.w - 2.f, hb.h - 2.f}, ctx.theme.header, 6.f);
				dl.PushClipRect({hb.x + 2.f, hb.y + 1.f, hb.w - 4.f, hb.h - 2.f}, true);
				float32 ty = hb.y + padY + asc;
				// Pastille de TYPE (bleu fonction/membre, vert type, jaune variable, violet macro).
				const NkColor kdot = d.hovKind == 2	  ? NkColor{63, 185, 80, 255}
									 : d.hovKind == 3 ? NkColor{224, 190, 70, 255}
									 : d.hovKind == 4 ? NkColor{178, 132, 235, 255}
									 : d.hovKind == 5 ? NkColor{240, 80, 80, 255}
									 : d.hovKind == 6 ? NkColor{224, 190, 70, 255}
													  : NkColor{88, 166, 255, 255};
				dl.AddCircleFilled({hb.x + padX + 4.f, ty - asc * 0.35f}, 4.f, kdot);
				// PROTOTYPE défilable horizontalement (molette sur la carte) + mini-barre.
				const float32 titX = hb.x + padX + ctx.S(14.f);
				const float32 titW = hb.w - padX * 2.f - ctx.S(14.f);
				const float32 fullW = ctx.font->MeasureWidth(d.hovTitle.CStr());
				const float32 xoMax = fullW > titW ? fullW - titW : 0.f;
				if (d.hovXOff > xoMax)
					d.hovXOff = xoMax;
				dl.PushClipRect({titX, ty - asc, titW, lineH}, true);
				dl.AddText(face, tex, {titX - d.hovXOff, ty}, d.hovTitle.CStr(),
						   NkColor{120, 200, 255, 255}); // signature (prototype complet)
				dl.PopClipRect();
				d.hovTitleRect = {titX, ty - asc, titW, lineH + ctx.S(8.f)}; // zone de PRISE (glisser g/d)
				if (xoMax > 0.f) { // barre sous le prototype (plus epaisse ; le glisser se fait sur la prise)
					const float32 trkY = ty + ctx.S(3.f);
					dl.AddRectFilled({titX, trkY, titW, 4.f}, NkColor{255, 255, 255, 20}, 2.f);
					float32 thw = titW * (titW / fullW);
					if (thw < 24.f)
						thw = 24.f;
					dl.AddRectFilled({titX + (titW - thw) * (d.hovXOff / xoMax), trkY, thw, 4.f},
									 NkColor{150, 158, 170, 220}, 2.f);
				}
				if (nb > 0) {
					dl.AddLine({hb.x + padX, ty + ctx.S(5.f)}, {hb.x + hb.w - padX, ty + ctx.S(5.f)},
							   NkColor{255, 255, 255, 24}, 1.f);
					ty += ctx.S(8.f);
					// DOC scrollable : fenetre de `shownB` lignes — molette = V, molette H/Ctrl = H,
					// glisser dans la zone = pan libre (X+Y). Barres temoins 4 px.
					const float32 bodyTop = ty + lineH - asc;
					const float32 bodyW = hb.w - padX * 2.f - ctx.S(6.f);
					const float32 bodyH = static_cast<float32>(shownB) * lineH;
					float32 maxBW = 0.f;
					for (int32 i = 0; i < nb; ++i) {
						const float32 lw2 = ctx.font->MeasureWidth(d.hovBody[static_cast<usize>(i)].CStr());
						if (lw2 > maxBW)
							maxBW = lw2;
					}
					const float32 byMax = static_cast<float32>(nb - shownB) * lineH;
					const float32 bxMax = maxBW > bodyW ? maxBW - bodyW : 0.f;
					if (d.hovBodyOff > byMax)
						d.hovBodyOff = byMax;
					if (d.hovBodyXOff > bxMax)
						d.hovBodyXOff = bxMax;
					d.hovBodyRect = {hb.x + padX, bodyTop, bodyW, bodyH};
					dl.PushClipRect(d.hovBodyRect, true);
					for (int32 i = 0; i < nb; ++i) {
						const float32 ly = bodyTop + static_cast<float32>(i) * lineH - d.hovBodyOff + asc;
						if (ly < bodyTop - lineH || ly > bodyTop + bodyH + lineH)
							continue;
						dl.AddText(face, tex, {hb.x + padX - d.hovBodyXOff, ly},
								   d.hovBody[static_cast<usize>(i)].CStr(), ctx.theme.textDisabled);
					}
					dl.PopClipRect();
					if (byMax > 0.f) { // barre VERTICALE de la doc
						const float32 vx = hb.x + hb.w - padX + ctx.S(1.f);
						dl.AddRectFilled({vx, bodyTop, 4.f, bodyH}, NkColor{255, 255, 255, 20}, 2.f);
						float32 th2 = bodyH * (bodyH / (bodyH + byMax));
						if (th2 < 18.f)
							th2 = 18.f;
						dl.AddRectFilled({vx, bodyTop + (bodyH - th2) * (d.hovBodyOff / byMax), 4.f, th2},
										 NkColor{150, 158, 170, 220}, 2.f);
					}
					if (bxMax > 0.f) { // barre HORIZONTALE de la doc
						const float32 hy = bodyTop + bodyH + ctx.S(2.f);
						dl.AddRectFilled({hb.x + padX, hy, bodyW, 4.f}, NkColor{255, 255, 255, 20}, 2.f);
						float32 tw2 = bodyW * (bodyW / maxBW);
						if (tw2 < 24.f)
							tw2 = 24.f;
						dl.AddRectFilled({hb.x + padX + (bodyW - tw2) * (d.hovBodyXOff / bxMax), hy, tw2, 4.f},
										 NkColor{150, 158, 170, 220}, 2.f);
					}
					ty = bodyTop + bodyH - lineH + asc; // baseline equivalente pour la suite (boutons)
				} else
					d.hovBodyRect = {0.f, 0.f, 0.f, 0.f};
				// Rangée d'ACTIONS (maquette) : 3 boutons-puces — [Aller à la définition] actif,
				// [Références] et [IA : expliquer] grisés (branchés aux prochaines vagues).
				ty += lineH * 0.4f;
				dl.AddLine({hb.x + padX, ty + ctx.S(5.f)}, {hb.x + hb.w - padX, ty + ctx.S(5.f)},
						   NkColor{255, 255, 255, 24}, 1.f);
				const float32 chipH = lineH + ctx.S(2.f);
				const float32 chipY = ty + ctx.S(9.f);
				auto chip = [&](float32 x, const char *txt, bool enabled) -> NkRect {
					const float32 tw = ctx.font->MeasureWidth(txt);
					const NkRect r = {x, chipY, tw + ctx.S(16.f), chipH};
					const bool hv = enabled && InRect(r, mouse);
					const NkColor bg =
						enabled ? (hv ? NkColor{ctx.theme.accent.r, ctx.theme.accent.g, ctx.theme.accent.b, 110}
									  : NkColor{ctx.theme.accent.r, ctx.theme.accent.g, ctx.theme.accent.b, 45})
								: NkColor{255, 255, 255, 10};
					dl.AddRectFilled(r, bg, 4.f);
					dl.AddText(face, tex, {r.x + ctx.S(8.f), r.y + (chipH - lineH) * 0.5f + asc}, txt,
							   enabled ? ctx.theme.text : ctx.theme.textDisabled);
					return r;
				};
				float32 axx = hb.x + padX;
				const NkRect defR = chip(axx, "Aller a la definition", true);
				axx += defR.w + ctx.S(6.f);
				const NkRect refR = chip(axx, "References", !d.hovSym.Empty());
				d.hovRefsRect = refR;
				axx += refR.w + ctx.S(6.f);
				const NkRect iaR = chip(axx, "IA : expliquer", false);
				axx += iaR.w + ctx.S(6.f);
				d.hovCopyRect = chip(axx, "Copier", true);
				d.hovActRect = defR;
				dl.PopClipRect();
				d.hovRect = hb; // la carte reste tant que la souris est dessus
			}

			// ── Gouttiere : numeros + chevrons repli + colonne breakpoints (clic = toggle, survol = creux) ──
			const float32 bpX = area.x + numAreaW + foldW;
			const float32 bpCx = bpX + bpW * 0.5f;
			const NkColor kBpOn = {229, 74, 68, 255}; // point d'arret actif (#E54A44)
			const bool overBp = ctx.popupDepth == 0 && mouse.x >= bpX && mouse.x < bpX + bpW && mouse.y >= textArea.y &&
								mouse.y < textArea.y + viewH;
			const int32 bpHoverLine =
				overBp ? d.LineAtRow(static_cast<int32>((mouse.y - textTop + d.scrollY) / lineH)) : -1;
			if (overBp && ctx.input.mouseClicked[0] && bpHoverLine >= 0 && bpHoverLine < d.LineCount()) {
				d.ToggleBreakpoint(bpHoverLine);
				NkCodeFocusId() = id;
			}
			dl.PushClipRect({area.x, area.y, gutterW, area.h}, true);
			for (int32 i = startDoc; i < d.LineCount(); ++i) {
				const int32 vrow = d.RowOf(i);
				if (vrow < 0)
					continue; // ligne repliée (masquée)
				if (vrow > lastVis)
					break;
				const float32 baseline = textTop + vrow * lineH - d.scrollY + asc;
				const float32 cy = textTop + vrow * lineH - d.scrollY + lineH * 0.5f;
				const float32 yTop = textTop + vrow * lineH - d.scrollY;
				// Chevron de repli (colonne foldW) sur les lignes d'en-tête pliables : ▾ déplié / ▸ replié.
				if (d.IsFoldHeader(i)) {
					const float32 fcx = area.x + numAreaW + foldW * 0.5f;
					const NkColor fcol = (i == d.curLine) ? kLineNoCur : kLineNo;
					const float32 a = 3.0f;
					if (d.FoldedAt(i))
						dl.AddTriangleFilled({fcx - a * 0.5f, cy - a}, {fcx - a * 0.5f, cy + a}, {fcx + a * 0.9f, cy},
											 fcol);
					else
						dl.AddTriangleFilled({fcx - a, cy - a * 0.5f}, {fcx + a, cy - a * 0.5f}, {fcx, cy + a * 0.9f},
											 fcol);
				}
				// Bande Git au bord droit de la gouttière : vert=ajout, bleu=modif, triangle rouge=suppression.
				const uint8 gs = d.GitAt(i);
				if (gs)
					dl.AddRectFilled({area.x + gutterW - gitW, yTop, gitW, lineH},
									 gs == 1 ? NkColor{63, 185, 80, 255} : NkColor{84, 174, 255, 255});
				if (d.GitDelAt(i))
					dl.AddTriangleFilled({area.x + gutterW - gitW - 4.f, yTop - 3.f}, {area.x + gutterW, yTop - 3.f},
										 {area.x + gutterW - gitW - 4.f, yTop + 3.f}, NkColor{248, 81, 73, 255});
				// Point d'arret : plein si pose, creux (alpha) au survol de sa ligne.
				if (d.HasBreakpoint(i))
					dl.AddCircleFilled({bpCx, cy}, 4.5f, kBpOn);
				else if (i == bpHoverLine)
					dl.AddCircleFilled({bpCx, cy}, 4.5f, NkColor{kBpOn.r, kBpOn.g, kBpOn.b, 90});
				char nb[16];
				std::snprintf(nb, sizeof(nb), "%d", i + 1);
				const float32 nw = ctx.font->MeasureWidth(nb);
				// Marqueur d'erreur/avertissement (façon VSCode) : numéro coloré + pastille à gauche.
				const int32 sev = d.DiagSevOn(i);
				const NkColor nColor = (sev == 1)		  ? NkColor{240, 80, 80, 255}
									   : (sev == 0)		  ? NkColor{224, 190, 70, 255}
									   : (i == d.curLine) ? kLineNoCur
														  : kLineNo;
				if (sev >= 0)
					dl.AddCircleFilled({area.x + 4.f, cy}, 3.f, nColor);
				dl.AddText(ctx.font->Face(), ctx.font->TexId(), {area.x + numAreaW - pad - nw, baseline}, nb, nColor);
			}
			dl.PopClipRect();

			// ── Barres de defilement : gouttieres TOUJOURS visibles (theme-aware) ──
			const bool sbLight = ((int32)ctx.theme.bgPrimary.r + ctx.theme.bgPrimary.g + ctx.theme.bgPrimary.b) > 384;
			const NkColor kTrack = sbLight ? NkColor{0, 0, 0, 20} : NkColor{255, 255, 255, 16};
			const NkColor kThumb = sbLight ? NkColor{168, 176, 185, 255} : NkColor{80, 88, 98, 255};
			const NkColor kThumbH = sbLight ? NkColor{130, 138, 148, 255} : NkColor{120, 130, 142, 255};
			const NkRect vTrack = {area.x + area.w - sbW, area.y, sbW, viewH};
			const NkRect hTrack = {area.x, area.y + area.h - sbW, area.w - sbW, sbW}; // pleine largeur (- coin V)
			dl.AddRectFilled(vTrack, kTrack);
			dl.AddRectFilled(hTrack, kTrack);
			dl.AddRectFilled({vTrack.x, hTrack.y, sbW, sbW}, kTrack); // coin bas-droite

			// Bouton fleche (dir : 0=haut 1=bas 2=gauche 3=droite). Retourne MAINTENU.
			auto arrowBtn = [&](const NkRect &r, int32 dir) -> bool {
				const bool h = InRect(r, mouse);
				if (h)
					dl.AddRectFilled(r, NkColor{33, 39, 48, 255});
				const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f, a = 3.2f;
				const NkColor ac = h ? kThumbH : kThumb;
				if (dir == 0)
					dl.AddTriangleFilled({cx, cy - a}, {cx - a, cy + a}, {cx + a, cy + a}, ac);
				else if (dir == 1)
					dl.AddTriangleFilled({cx - a, cy - a}, {cx + a, cy - a}, {cx, cy + a}, ac);
				else if (dir == 2)
					dl.AddTriangleFilled({cx - a, cy}, {cx + a, cy - a}, {cx + a, cy + a}, ac);
				else
					dl.AddTriangleFilled({cx - a, cy - a}, {cx + a, cy}, {cx - a, cy + a}, ac);
				return h && ctx.input.mouseDown[0];
			};

			// ── Barre VERTICALE : fleche haut + piste (pouce) + fleche bas ──
			{
				const NkRect upB = {vTrack.x, vTrack.y, sbW, sbW};
				const NkRect dnB = {vTrack.x, vTrack.y + viewH - sbW, sbW, sbW};
				const NkRect inner = {vTrack.x, vTrack.y + sbW, sbW, viewH - 2.f * sbW};
				if (arrowBtn(upB, 0))
					d.scrollY -= lineH * 0.8f;
				if (arrowBtn(dnB, 1))
					d.scrollY += lineH * 0.8f;
				if (maxScrollY > 0.f && inner.h > 8.f) {
					float32 th = inner.h * (viewH / contentH);
					if (th < 24.f)
						th = 24.f;
					if (th > inner.h)
						th = inner.h;
					const float32 ty = inner.y + (d.scrollY / maxScrollY) * (inner.h - th);
					const NkRect thumb = {inner.x + 3.f, ty, sbW - 6.f, th};
					if (ctx.input.mouseClicked[0] && InRect(inner, mouse))
						ctx.activeId = vbarId;
					const bool act = (ctx.activeId == vbarId);
					if (act && ctx.input.mouseDown[0]) {
						const float32 t = (mouse.y - inner.y - th * 0.5f) / (inner.h - th);
						d.scrollY = (t < 0.f ? 0.f : t > 1.f ? 1.f : t) * maxScrollY;
					}
					dl.AddRectFilled(thumb, (act || InRect(inner, mouse)) ? kThumbH : kThumb, 3.f);
				}
			}
			{ // marques (dessinees APRES le pouce, sinon il les recouvre) : erreurs/avertissements (droite),
			  // occurrences recherche (gauche), breakpoints
				const float32 tot = static_cast<float32>(d.VisRowCount() > 0 ? d.VisRowCount() : 1);
				auto markY = [&](int32 line) {
					return vTrack.y + (static_cast<float32>(d.RowOf(line)) / tot) * vTrack.h;
				};
				for (usize i = 0; i < d.diags.Size(); ++i) {
					if (d.diags[i].line < 0 || d.diags[i].line >= d.LineCount() || d.LineHidden(d.diags[i].line))
						continue;
					const NkColor mc = d.diags[i].sev ? NkColor{240, 80, 80, 230} : NkColor{224, 190, 70, 230};
					dl.AddRectFilled({vTrack.x + sbW * 0.5f, markY(d.diags[i].line), sbW * 0.5f - 1.f, 3.f}, mc);
				}
				if (d.findOpen)
					for (usize i = 0; i < d.findLine.Size(); ++i)
						dl.AddRectFilled({vTrack.x + 1.f, markY(d.findLine[i]), sbW * 0.5f - 1.f, 3.f},
										 NkColor{255, 160, 60, 220}); // occurrences (orange)
				for (usize i = 0; i < d.breakpoints.Size(); ++i)
					if (d.breakpoints[i] >= 0 && d.breakpoints[i] < d.LineCount())
						dl.AddRectFilled({vTrack.x + 1.f, markY(d.breakpoints[i]), sbW - 2.f, 2.f},
										 NkColor{229, 74, 68, 255}); // breakpoints (rouge vif)
			}
			// ── Barre HORIZONTALE : fleche gauche + piste (pouce) + fleche droite ──
			{
				const NkRect lfB = {hTrack.x, hTrack.y, sbW, sbW};
				const NkRect rtB = {hTrack.x + hTrack.w - sbW, hTrack.y, sbW, sbW};
				const NkRect inner = {hTrack.x + sbW, hTrack.y, hTrack.w - 2.f * sbW, sbW};
				if (arrowBtn(lfB, 2))
					d.scrollX -= 18.f;
				if (arrowBtn(rtB, 3))
					d.scrollX += 18.f;
				if (maxScrollX > 0.f && inner.w > 8.f) {
					float32 tw = inner.w * (viewW / maxLineW);
					if (tw < 24.f)
						tw = 24.f;
					if (tw > inner.w)
						tw = inner.w;
					const float32 tx = inner.x + (d.scrollX / maxScrollX) * (inner.w - tw);
					const NkRect thumb = {tx, hTrack.y + 3.f, tw, sbW - 6.f};
					if (ctx.input.mouseClicked[0] && InRect(inner, mouse))
						ctx.activeId = hbarId;
					const bool act = (ctx.activeId == hbarId);
					if (act && ctx.input.mouseDown[0]) {
						const float32 t = (mouse.x - inner.x - tw * 0.5f) / (inner.w - tw);
						d.scrollX = (t < 0.f ? 0.f : t > 1.f ? 1.f : t) * maxScrollX;
					}
					dl.AddRectFilled(thumb, (act || InRect(inner, mouse)) ? kThumbH : kThumb, 3.f);
				}
			}
			// Re-borne apres defilement par les fleches (l'auto-scroll plus haut est passe).
			if (d.scrollY < 0.f)
				d.scrollY = 0.f;
			if (d.scrollY > maxScrollY)
				d.scrollY = maxScrollY;
			if (d.scrollX < 0.f)
				d.scrollX = 0.f;
			if (d.scrollX > maxScrollX)
				d.scrollX = maxScrollX;

			// ── Minimap : aperçu miniature du fichier (colonne droite) + viewport cliquable. ──
			if (showMinimap && d.VisRowCount() > 0) {
				const int32 total = d.VisRowCount();
				const NkRect mmArea = {area.x + area.w - sbW - mmW, area.y, mmW, area.h - sbW};
				// Façon VSCode : échelle FIXE -> le fichier entier ne tient pas forcément dans la
				// miniature ; elle DÉFILE proportionnellement au document. Fond = celui de
				// l'éditeur (déjà peint en dessous), donc transparent : rien à dessiner ici.
				const float32 rowPx = ctx.S(3.f);				   // hauteur fixe d'une ligne minimap
				const float32 barH = rowPx - ctx.S(0.7f);		   // hauteur des glyphes
				const float32 mmChar = (mmW - ctx.S(4.f)) / 100.f; // ~100 colonnes projetées sur la largeur
				const float32 mmContentH = total * rowPx;		   // hauteur totale du contenu minimap
				const float32 mmScrollMax = mmContentH > mmArea.h ? mmContentH - mmArea.h : 0.f;
				const float32 mmFrac = maxScrollY > 0.f ? d.scrollY / maxScrollY : 0.f;
				const float32 mmScroll = mmFrac * mmScrollMax; // défilement de la minimap (suit le doc)
				const int32 rFirst = static_cast<int32>(mmScroll / rowPx);
				int32 rLast = rFirst + static_cast<int32>(mmArea.h / rowPx) + 1;
				if (rLast >= total)
					rLast = total - 1;
				const float32 xMax = mmArea.x + mmW - ctx.S(1.f);
				const float32 mmLeft = mmArea.x + ctx.S(3.f); // marge gauche (bande Git)
				const int32 tabW = 4;
				int32 mmBudget = 24000; // garde-fou : nb max de rectangles-glyphes par frame
				dl.PushClipRect(mmArea, true);
				int32 mmBlk = 0; // état bloc-commentaire (approx : repart de 0 en haut de fenêtre)
				for (int32 r = rFirst; r <= rLast; ++r) {
					const int32 L = d.visRows[r];
					if (L < 0 || L >= d.LineCount())
						continue;
					const NkCodeLine &ln = d.lines[L];
					const int32 nn = static_cast<int32>(ln.Size());
					const float32 y = mmArea.y + r * rowPx - mmScroll;
					const char *dta = ln.Data();
					// Bande Git à gauche (vert=ajout, bleu=modif) — aperçu façon VSCode.
					const uint8 gs = d.GitAt(L);
					if (gs)
						dl.AddRectFilled({mmArea.x, y, ctx.S(2.f), barH < 1.f ? 1.f : barH},
										 gs == 1 ? NkColor{63, 185, 80, 220} : NkColor{84, 174, 255, 220});
					// « Texte flou » façon VSCode : UN micro-rectangle PAR CARACTÈRE, couleur syntaxe,
					// avec la SILHOUETTE du caractère (pleine hauteur pour A-Z/0-9/ascendantes,
					// hauteur d'x pour aceo…, jambage pour gjpqy, point bas pour la ponctuation).
					// L'interstice entre caractères + le sous-pixel donnent l'impression de texte.
					int32 pc = 0, pcol = 0; // curseur d'expansion des tabs (O(n) par ligne)
					mmBlk = TokenizeLine(
						lang, dta, nn, mmBlk, syn,
						[&](int32 a, int32 b, const NkColor &tc) {
							while (pc < a) { // rattrape les zones non couvertes (blancs entre tokens)
								pcol += (dta[pc] == '\t') ? tabW : 1;
								++pc;
							}
							const NkColor cc = {tc.r, tc.g, tc.b, 200};
							for (int32 k = a; k < b; ++k) {
								const char ch = dta[k];
								if (ch == ' ' || ch == '\t') {
									pcol += (ch == '\t') ? tabW : 1;
									++pc;
									continue;
								}
								const float32 xs = mmLeft + pcol * mmChar;
								++pcol;
								++pc;
								if (xs >= xMax)
									continue;					  // au-delà de la largeur de la minimap
								float32 xe = xs + mmChar * 0.78f; // interstice -> texture de texte
								if (xe > xMax)
									xe = xMax;
								// Silhouette verticale du caractère.
								float32 y0 = y, h = barH;
								if (ch >= 'a' && ch <= 'z') {
									if (ch == 'g' || ch == 'j' || ch == 'p' || ch == 'q' || ch == 'y') {
										y0 = y + barH * 0.25f; // jambage descendant
										h = barH * 0.75f;
									} else if (!(ch == 'b' || ch == 'd' || ch == 'f' || ch == 'h' || ch == 'k' ||
												 ch == 'l' || ch == 't')) {
										y0 = y + barH * 0.30f; // hauteur d'x
										h = barH * 0.70f;
									}
								} else if (ch == '.' || ch == ',' || ch == '\'' || ch == '`' || ch == '-' ||
										   ch == '_' || ch == '=' || ch == '~' || ch == ':' || ch == ';') {
									y0 = y + barH * 0.5f; // ponctuation basse
									h = barH * 0.5f;
								}
								if (--mmBudget < 0)
									return; // garde-fou anti-surcharge (fichiers/lignes énormes)
								dl.AddRectFilled({xs, y0, xe - xs, h}, cc);
							}
						},
						&d.symTypes, &d.symFuncs, projTypes, projFuncs);
				}
				// Marqueurs de diagnostics (erreurs/avertissements) façon VSCode : tick droit + bande légère.
				for (usize di = 0; di < d.diags.Size(); ++di) {
					const NkCodeDoc::Diag &dg = d.diags[di];
					if (dg.line < 0 || dg.line >= d.LineCount() || d.LineHidden(dg.line))
						continue;
					const float32 y = mmArea.y + d.RowOf(dg.line) * rowPx - mmScroll;
					const float32 h = rowPx < 2.f ? 2.f : rowPx;
					if (y + h < mmArea.y || y > mmArea.y + mmArea.h)
						continue; // hors de la fenêtre minimap
					const NkColor c = dg.sev ? NkColor{240, 80, 80, 240} : NkColor{224, 190, 70, 240};
					dl.AddRectFilled({mmArea.x, y, mmW, h},
									 NkColor{c.r, c.g, c.b, 40}); // bande pleine largeur atténuée
					dl.AddRectFilled({mmArea.x + mmW - ctx.S(6.f), y, ctx.S(5.f), h}, c); // tick vif à droite
				}
				// Curseur de viewport (plage visible), translucide façon VSCode, suit le défilement minimap.
				int32 lv = lastVis > total ? total : lastVis;
				const float32 vy0 = mmArea.y + firstVis * rowPx - mmScroll;
				float32 vh = (lv - firstVis) * rowPx;
				if (vh < 2.f)
					vh = 2.f;
				const bool mmLight =
					((int32)ctx.theme.bgPrimary.r + ctx.theme.bgPrimary.g + ctx.theme.bgPrimary.b) > 384;
				const NkColor vpFill = mmLight ? NkColor{0, 0, 0, 26} : NkColor{255, 255, 255, 22};
				dl.AddRectFilled({mmArea.x, vy0, mmW, vh}, vpFill);
				dl.PopClipRect();
				// Interaction : clic / glisser -> centre le défilement sur la position pointée.
				const bool overMm = ctx.popupDepth == 0 && InRect(mmArea, mouse);
				if (overMm && ctx.input.mouseClicked[0])
					ctx.activeId = mmId;
				if (ctx.activeId == mmId && ctx.input.mouseDown[0]) {
					const float32 rowAt = (mouse.y - mmArea.y + mmScroll) / rowPx; // row visuel visé
					d.scrollY = topPad + rowAt * lineH - viewH * 0.5f;			   // centre la ligne visée
					if (d.scrollY < 0.f)
						d.scrollY = 0.f;
					if (d.scrollY > maxScrollY)
						d.scrollY = maxScrollY;
				}
			}

			// Cadre de l'editeur (bordure permanente) + accent si focus.
			dl.AddRect(area, focused ? kBorder : NkColor{33, 39, 48, 255}, 1.f);

			// ── Menu contextuel (clic droit) Copier / Couper / Coller ── (editeur focus)
			if (focused && NkCodeCtxMenu().open) {
				const char *items[] = {"Copier", "Couper", "Coller"};
				const bool en[] = {d.HasSel(), d.HasSel(), true};
				const int32 act = NkCtxMenuDraw(ctx, NkCodeCtxMenu(), items, en, 3);
				if (act == 0 && d.HasSel())
					ctx.SetClipboard(d.GetSelectedText().CStr());
				else if (act == 1 && d.HasSel()) {
					ctx.SetClipboard(d.GetSelectedText().CStr());
					d.EraseSelection();
					changed = true;
				} else if (act == 2) {
					const NkString clip = ctx.GetClipboard();
					if (!clip.Empty()) {
						d.InsertText(clip.CStr());
						changed = true;
					}
				}
			}

			// ── Menu QUICK FIX (Ctrl+.) : applique l'action choisie sur le document ──
			if (focused && NkCodeQfMenu().open && !d.qfLabels.Empty()) {
				const char *items[6];
				bool en[6];
				const int32 n = d.qfLabels.Size() < 6 ? static_cast<int32>(d.qfLabels.Size()) : 6;
				for (int32 i = 0; i < n; ++i) {
					items[i] = d.qfLabels[static_cast<usize>(i)].CStr();
					en[i] = true;
				}
				const int32 act = NkCtxMenuDraw(ctx, NkCodeQfMenu(), items, en, n);
				if (act >= 0 && act < n && d.qfL[static_cast<usize>(act)] >= 0 &&
					d.qfL[static_cast<usize>(act)] < d.LineCount()) {
					const int32 ln = d.qfL[static_cast<usize>(act)], co = d.qfC[static_cast<usize>(act)];
					d.Checkpoint(3);
					d.ResetEditRun();
					if (d.qfKind[static_cast<usize>(act)] == 0) { // inserer ';' a la position exacte
						d.curLine = ln;
						d.curCol = co < d.LineLen(ln) ? co : d.LineLen(ln);
						d.selLine = d.curLine;
						d.selCol = d.curCol;
						d.InsertChar(';');
					} else { // remplacer le MOT sous (ln, co) par la proposition du compilateur
						const NkCodeLine &L = d.lines[ln];
						int32 ws = co, we = co;
						while (ws > 0 && NkCodeDoc::IsWChar(L[ws - 1]))
							--ws;
						while (we < static_cast<int32>(L.Size()) && NkCodeDoc::IsWChar(L[we]))
							++we;
						if (we > ws) {
							d.selLine = ln;
							d.selCol = ws;
							d.curLine = ln;
							d.curCol = we;
							d.InsertText(d.qfPay[static_cast<usize>(act)].CStr());
						}
					}
					changed = true;
				}
			}

			// ── Barre de RECHERCHE / REMPLACEMENT (Ctrl+F / Ctrl+H) : flottante en haut-droite ──
			//    Champs = NkOverlayTextField (curseur clignotant, sélection, copier/coller partagés). ──
			if (d.findOpen && ctx.font && ctx.font->Face()) {
				const float32 fh = ctx.font->LineHeight(), fasc = ctx.font->Ascent();
				const float32 rowH = fh + ctx.S(12.f), gap = ctx.S(5.f), pad = ctx.S(8.f);
				const float32 barW = ctx.S(380.f);
				const float32 barH = rowH * (d.findReplace ? 2.f : 1.f) + ctx.S(10.f);
				const float32 bx = area.x + area.w - barW - ctx.S(18.f), by = area.y + ctx.S(8.f);
				dl.AddRectFilled({bx + ctx.S(3.f), by + ctx.S(4.f), barW, barH}, NkColor{0, 0, 0, 70},
								 ctx.S(7.f)); // ombre portée
				dl.AddRectFilled({bx, by, barW, barH}, NkColor{37, 43, 51, 255}, ctx.S(7.f));
				dl.AddRect({bx, by, barW, barH}, ctx.theme.accent, 1.f);
				const NkVec2 mp = ctx.input.mousePos;
				const bool clk = ctx.input.mouseClicked[0];
				auto hit = [&](const NkRect &r) {
					return mp.x >= r.x && mp.x < r.x + r.w && mp.y >= r.y && mp.y < r.y + r.h;
				};
				auto btn = [&](float32 x, float32 y, float32 w, const char *lbl, bool on, const char *tip) -> bool {
					(void)tip;
					const NkRect r = {x, y, w, rowH - gap};
					const bool hv = hit(r);
					if (on)
						dl.AddRectFilled(r, ctx.theme.accent, ctx.S(4.f));
					else if (hv)
						dl.AddRectFilled(r, ctx.theme.buttonHover, ctx.S(4.f));
					const float32 tw = ctx.font->MeasureWidth(lbl);
					dl.AddText(ctx.font->Face(), ctx.font->TexId(),
							   {r.x + (w - tw) * 0.5f, r.y + (r.h - fh) * 0.5f + fasc}, lbl,
							   on ? NkColor{255, 255, 255, 255} : ctx.theme.text);
					return hv && clk;
				};
				// Champ texte partagé (NkOverlayTextField) + placeholder + focus au clic.
				auto tfield = [&](float32 x, float32 y, float32 w, char *buf, const char *ph, int32 idx) {
					const NkRect r = {x, y, w, rowH - gap};
					NkOverlayTextField(ctx, dl, ctx.font, r, buf, 256, d.findFocus == idx);
					if (buf[0] == '\0')
						dl.AddText(ctx.font->Face(), ctx.font->TexId(),
								   {r.x + ctx.S(8.f), r.y + (r.h - fh) * 0.5f + fasc}, ph, ctx.theme.textDisabled);
					if (hit(r) && clk)
						d.findFocus = idx;
				};
				const float32 btnW = ctx.S(30.f), countW = ctx.S(54.f);
				float32 rx = bx + pad, ry = by + ctx.S(5.f);
				const float32 queryW = barW - pad * 2.f - countW - btnW * 4.f - gap * 4.f;
				tfield(rx, ry, queryW, d.findQuery, "Rechercher", 0);
				// TEMPS RÉEL : le champ vient d'être édité (NkOverlayTextField). Si la requête a
				// changé, on recalcule MAINTENANT (occurrences en ordre ligne par ligne) et on saute
				// à la 1ʳᵉ occurrence >= curseur -> compteur + surlignage à jour dès la frappe.
				if (d.findSig != d.FindSigOf()) {
					d.FindRecompute();
					d.findCur = -1;
					d.FindNext(true);
					changed = true;
				}
				float32 cx2 = rx + queryW + gap;
				char cnt[32];
				if (d.FindCount() == 0) {
					cnt[0] = (d.findQuery[0] == '\0') ? '\0' : '0';
					cnt[1] = '\0';
				} else
					std::snprintf(cnt, sizeof(cnt), "%d/%d", d.findCur < 0 ? 0 : d.findCur + 1, d.FindCount());
				const float32 cntW2 = ctx.font->MeasureWidth(cnt);
				dl.AddText(ctx.font->Face(), ctx.font->TexId(),
						   {cx2 + (countW - cntW2) * 0.5f, ry + (rowH - gap - fh) * 0.5f + fasc}, cnt,
						   ctx.theme.textDisabled);
				cx2 += countW;
				if (btn(cx2, ry, btnW, "Aa", d.findCase, "Casse")) {
					d.findCase = !d.findCase;
					d.FindRecompute();
				}
				cx2 += btnW + gap;
				if (btn(cx2, ry, btnW, "<", false, "Prec")) {
					d.FindEnsure();
					d.FindNext(false);
					changed = true;
				}
				cx2 += btnW + gap; // précédent
				if (btn(cx2, ry, btnW, ">", false, "Suiv")) {
					d.FindEnsure();
					d.FindNext(true);
					changed = true;
				}
				cx2 += btnW + gap; // suivant
				if (btn(cx2, ry, btnW, "x", false, "Fermer")) {
					d.FindClose();
				}
				if (d.findReplace) {
					ry += rowH;
					const float32 rBtnW = ctx.S(56.f);
					const float32 replW = barW - pad * 2.f - rBtnW * 2.f - gap * 2.f;
					tfield(rx, ry, replW, d.findRepl, "Remplacer", 1);
					float32 rcx = rx + replW + gap;
					if (btn(rcx, ry, rBtnW, "Rempl.", false, "")) {
						d.ReplaceCurrent();
						changed = true;
					}
					rcx += rBtnW + gap;
					if (btn(rcx, ry, rBtnW, "Tout", false, "")) {
						d.ReplaceAll();
						changed = true;
					}
				}
			}

			// « Modifie » = le texte DIFFERE de l'etat sauvegarde (hash) — forcer true ici
			// ecrasait le calcul fait par Undo()/Redo() et le point ne s'eteignait jamais.
			if (changed)
				d.dirty = (d.SymSig() != d.savedSig);
			return changed;
		}

	} // namespace nkcode
} // namespace nkentseu
