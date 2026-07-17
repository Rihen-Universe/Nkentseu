#pragma once
// =============================================================================
// NkAiPanel.h — Panneau ASSISTANT IA (chat) fonctionnel.
//   UI : sélecteur de fournisseur + liste de messages (bulles) + zone de saisie.
//   Backends (via `curl`, appel ASYNCHRONE NkProcess -> ne gèle pas l'UI) :
//     0) Claude API (Anthropic) — clé dans NKCODE_ANTHROPIC_KEY / ANTHROPIC_API_KEY
//     1) Ollama local          — http://localhost:11434 (aucune clé)
//     2) Maison (à venir)      — réservé pour l'IA maison de Rihen (stub désactivé)
//   Non-streaming pour ce 1er jet (réponse complète). Parsing JSON minimal.
// =============================================================================
#include "NKEditorKit/NkEditorKit.h"
#include "NKEditorKit/NkEditorCombo.h" // NkComboButton/NkComboMenu (combo reutilisable, ouverture haut/bas auto)
#include "NKCode/Project/NkCodeState.h"
#include "NKCode/Project/NkProcess.h"
#include "NKCode/Editor/NkTextDraw.h" // NkEncodeU8 (décodage \uXXXX -> UTF-8)
#include "NKCode/Shell/NkI18n.h"
#include "NKCode/Shell/NkUi.h" // NkIcons (icones de la vue IDE)
#include "NKContainers/String/NkFormat.h" // NkPrintf (formatage maison)
#include "NKFileSystem/NkFile.h"		  // NkFile::WriteAllText (fichiers maison)
#include "NKPlatform/NkEnv.h"			  // env::GetEnvVar (variables d'environnement maison)
#include <cmath> // std::cos/std::sin (spinner du statut "en cours")
#include "NKWindow/Core/NkLauncher.h" // OpenURL (bouton "View help docs" de la palette d'actions)
#include <cstdio> // _popen/fgets UNIQUEMENT (pipe process, cf. wrapper désigné NkProcess.h)

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu::editorkit;
		using namespace nkentseu::nkgui;

		// ── Agent IA en ligne de commande (Claude Code, Codex...) : panneau lateral DROIT qui
		//    DETECTE le CLI et le LANCE dans un nouvel onglet du terminal integre (workspace). ──
		class AgentCliPanel : public NkEditorPanel {
			public:
				AgentCliPanel(const char *title, const char *exe, const char *installHint, NkCodeState *s,
							  NkEditorShell *shell)
					: NkEditorPanel(title, NkEditorDockSide::NK_RIGHT), mExe(exe), mHint(installHint), mS(s),
					  mShell(shell) {
					SetOpen(false); // sidebar exclusive : ouvert via l'activity bar
				}

				void OnUI(NkEditorFrameContext &ec) override {
					auto &ctx = ec.Ui();
					Detect();
					ec.Text(Title());
					ec.Separator();
					if (mFoundPath.Empty()) {
						ec.Text(NkT("agent.missing"));
						ec.Text(mHint.CStr());
						return;
					}
					ec.Text(NkT("agent.found"));
					ec.Text(mFoundPath.CStr());
					ec.Separator();
					if (Selectable(ctx, NkT("agent.launch"), false) && mS) {
						mS->termOpenCmd = mExe; // la commande EST l'agent (nouvel onglet du terminal)
						mS->termOpenKind = -1;
						mS->termOpenAt = mS->HasWorkspace() ? mS->root.ToString() : NkString(".");
						if (mShell)
							mShell->FocusPanel("TERMINAL");
					}
				}

			private:
				NkString mExe;	// executable du CLI (claude / codex)
				NkString mHint; // commande d'installation affichee si absent
				NkCodeState *mS;
				NkEditorShell *mShell;
				bool mDetected = false;
				NkString mFoundPath;

				void Detect() { // `where` UNE fois (léger, au premier affichage du panneau)
					if (mDetected)
						return;
					mDetected = true;
#ifdef _WIN32
					const NkString cmd = NkString("where ") + mExe.CStr() + " 2>nul";
					FILE *pipe = _popen(cmd.CStr(), "r");
#else
					const NkString cmd = NkString("which ") + mExe.CStr() + " 2>/dev/null";
					FILE *pipe = popen(cmd.CStr(), "r");
#endif
					if (!pipe)
						return;
					char line[512]; // fgets sur PIPE process : conservé (cf. wrapper désigné NkProcess.h)
					if (std::fgets(line, sizeof(line), pipe)) {
						for (char *q = line; *q; ++q)
							if (*q == '\n' || *q == '\r')
								*q = 0;
						mFoundPath = NkString(line);
					}
#ifdef _WIN32
					_pclose(pipe);
#else
					pclose(pipe);
#endif
				}
		};

		class AiPanel : public NkEditorPanel {
			public:
				// ── Alias PAR-CHAT : ces macros redirigent les identifiants historiques (mInput,
				// mModelIdx, mMode...) vers le champ correspondant du chat ACTIF (mChats[mActiveChat]).
				// Choisi plutot que de renommer ~65 sites d'usage : preserve sizeof(mInput)/
				// sizeof(mSystem) correct (vrais tableaux, pas des pointeurs). DOIT être placé tout
				// en haut de la classe (le preprocesseur substitue en un seul passage TEXTUEL du
				// haut vers le bas — peu importe l'ordre de déclaration des membres en C++, un
				// usage AVANT le #define ne serait PAS substitué). #undef en fin de classe pour ne
				// PAS fuiter dans le reste du fichier/des includeurs. ──
#define mInput (mChats[static_cast<usize>(mActiveChat)].input)
#define mModelIdx (mChats[static_cast<usize>(mActiveChat)].modelIdx)
#define mMode (mChats[static_cast<usize>(mActiveChat)].mode)
#define mScope (mChats[static_cast<usize>(mActiveChat)].scope)
#define mEditAuth (mChats[static_cast<usize>(mActiveChat)].editAuth)
#define mCtxFileOn (mChats[static_cast<usize>(mActiveChat)].ctxFileOn)
#define mTemp (mChats[static_cast<usize>(mActiveChat)].temp)
#define mMaxTokens (mChats[static_cast<usize>(mActiveChat)].maxTokens)
#define mSystem (mChats[static_cast<usize>(mActiveChat)].system)
#define mEffort (mChats[static_cast<usize>(mActiveChat)].effort)
#define mThinking (mChats[static_cast<usize>(mActiveChat)].thinking)
#define mAutoSwitchFlagged (mChats[static_cast<usize>(mActiveChat)].autoSwitchFlagged)

				// kind : 0 Assistant (général), 1 Claude Code, 2 Codex, 3 NkAI (maison). Chaque IA
				// partage CETTE interface mais garde son propre backend + ses propres propriétés.
				AiPanel(NkCodeState *s, NkEditorShell *shell, int32 kind, const char *title)
					: NkEditorPanel(title, NkEditorDockSide::NK_RIGHT), mS(s), mShell(shell), mKind(kind), mTitle(title) {
					// mInput/mSystem sont des alias PAR-CHAT (mChats[mActiveChat].input/.system) —
					// aucun chat n'existe encore ici (NewChat() est appelé paresseusement dans OnUI) ;
					// leur init à {0} est déjà garantie par ChatSession, rien à faire au ctor.
				}

				void OnUI(NkEditorFrameContext &ec) override {
					auto &ctx = ec.Ui();
					auto &dl = ctx.DL();
					const NkRect r = dl.CurrentClip();
					if (mChats.Empty())
						NewChat(); // 1re conversation (locale correcte : posée après construction)
					Poll(); // draine la réponse en cours

					const NkGuiFont *font = ctx.font;
					const NkVec2 mp = ctx.input.mousePos;
					// ── Palette : structure = thème (theme-aware) ; accent = VIOLET de marque IA. ──
					const NkColor kViolet = {124, 108, 246, 255};
					const NkColor kVioletHov = {143, 128, 250, 255};
					const NkColor kBody = ctx.theme.bgPrimary;
					const NkColor kHdr = ctx.theme.header;
					const NkColor kChip = ctx.theme.button;
					NkIcons *ic = mS ? mS->icons : nullptr;

					auto drawIcon = [&](uint32 tex, const NkRect &rc, const NkColor &tint) {
						if (tex)
							dl.AddImage(tex, rc, {0.f, 0.f}, {1.f, 1.f}, tint);
					};
					auto textAt = [&](float32 x, float32 cy, const char *s, const NkColor &col, float32 maxW = -1.f) {
						if (font && font->Valid())
							dl.AddText(font->Face(), font->TexId(), {x, cy - font->LineHeight() * 0.5f + font->Ascent()},
									   s, col, maxW);
					};
					auto measure = [&](const char *s) { return (font && font->Valid()) ? font->MeasureWidth(s) : 0.f; };

					const float32 pad = ctx.S(10.f);
					const float32 lineH = (font && font->Valid()) ? font->LineHeight() : 16.f;
					const float32 hdrH = ctx.ItemHeight() + ctx.S(12.f);
					// Onglets Chat/Génération/Revue : uniquement l'Assistant général (mKind==0) —
					// les agents CLI (Claude Code/Codex/NkAI) restent Chat seul, pas de sous-vues.
					const float32 tabsH = (mKind == 0) ? (ctx.ItemHeight() + ctx.S(8.f)) : 0.f;
					const bool chatView = (mView == 0) || (mKind != 0);
					// Barre d'outils du bas = DEUX lignes : (1) fichiers attachés au contexte,
					// (2) crayon + Mode/Portée/Édition + envoi. Absente hors de la vue Chat (les
					// onglets Génération/Revue ont leur propre bouton d'action, pas de saisie libre).
					const float32 toolRowH = ctx.ItemHeight() + ctx.S(10.f);
					const float32 toolH = chatView ? (toolRowH * 2.f) : 0.f;
					// Zone de saisie qui GRANDIT avec le texte (façon VSCode/Claude Code), bornée à 6 lignes.
					const float32 inInnerW = r.w - ctx.S(24.f);
					const int32 inRows = InputRows(ctx, inInnerW);
					const float32 inH = chatView ? (inRows * lineH + ctx.S(20.f)) : 0.f;
					const NkRect hdr = {r.x, r.y, r.w, hdrH};
					const float32 bodyY = r.y + hdrH + tabsH;
					const float32 bodyH = r.h - hdrH - tabsH - inH - toolH;
					const NkRect body = {r.x, bodyY, r.w, bodyH};

					dl.AddRectFilled(r, kBody);

					// ══ EN-TÊTE : logo étincelle · pilule modèle · engrenage · liste des chats · + nouveau chat ══
					dl.AddRectFilled(hdr, kHdr);
					dl.AddRectFilled({hdr.x, hdr.y + hdrH - 1.f, hdr.w, 1.f}, ctx.theme.border);
					const float32 hcy = hdr.y + hdrH * 0.5f;
					{
						const float32 isz = ctx.S(18.f);
						const NkRect logo = {hdr.x + pad, hcy - isz * 0.5f, isz, isz};
						if (ic && ic->sparkles)
							drawIcon(ic->sparkles, logo, kViolet);
						else // fallback : petit losange violet
							dl.AddCircleFilled({logo.x + isz * 0.5f, hcy}, isz * 0.4f, kViolet);
						// Le nom (barre d'activité/onglet) n'est PAS répété ici : juste le logo.
					}
					// À droite : + nouveau chat, liste des chats, engrenage, pilule modèle (de droite à gauche).
					float32 rx = hdr.x + hdr.w - pad;
					const float32 bsz = ctx.S(24.f);
					{ // + nouveau chat
						const NkRect rb = {rx - bsz, hcy - bsz * 0.5f, bsz, bsz};
						const bool hov = NkGuiRectContains(rb, mp);
						if (hov)
							dl.AddRectFilled(rb, ctx.theme.buttonHover, ctx.theme.rounding);
						if (ic && ic->plus)
							drawIcon(ic->plus, {rb.x + ctx.S(5.f), rb.y + ctx.S(5.f), bsz - ctx.S(10.f), bsz - ctx.S(10.f)},
									 ctx.theme.textDisabled);
						else {
							const float32 a = ctx.S(6.f), cx = rb.x + bsz * 0.5f;
							dl.AddLine({cx - a, hcy}, {cx + a, hcy}, ctx.theme.textDisabled, 1.6f);
							dl.AddLine({cx, hcy - a}, {cx, hcy + a}, ctx.theme.textDisabled, 1.6f);
						}
						if (hov && ctx.input.mouseClicked[0])
							NewChat();
						rx -= bsz + ctx.S(4.f);
					}
					{ // liste des chats (switch/suppression) — dessinée (pas d'asset)
						const NkRect lb = {rx - bsz, hcy - bsz * 0.5f, bsz, bsz};
						const bool hov = NkGuiRectContains(lb, mp);
						if (hov || mChatListOpen)
							dl.AddRectFilled(lb, mChatListOpen ? kViolet : ctx.theme.buttonHover, ctx.theme.rounding);
						const NkColor lc = mChatListOpen ? NkColor{255, 255, 255, 255} : ctx.theme.textDisabled;
						for (int32 k = 0; k < 3; ++k)
							dl.AddLine({lb.x + ctx.S(6.f), lb.y + ctx.S(6.f) + k * ctx.S(5.f)},
									   {lb.x + bsz - ctx.S(6.f), lb.y + ctx.S(6.f) + k * ctx.S(5.f)}, lc, 1.5f);
						mChatListAnchor = lb;
						if (hov && ctx.input.mouseClicked[0]) {
							mChatListOpen = !mChatListOpen;
							ctx.input.mouseClicked[0] = false;
						}
						rx -= bsz + ctx.S(6.f);
					}
					{ // engrenage (réglages) — dessiné (pas d'asset)
						const NkRect gb = {rx - bsz, hcy - bsz * 0.5f, bsz, bsz};
						const bool hov = NkGuiRectContains(gb, mp);
						if (hov || mPropsOpen)
							dl.AddRectFilled(gb, mPropsOpen ? kViolet : ctx.theme.buttonHover, ctx.theme.rounding);
						DrawGear(dl, {gb.x + bsz * 0.5f, hcy}, ctx.S(7.f),
								 mPropsOpen ? NkColor{255, 255, 255, 255} : ctx.theme.textDisabled);
						mGearRect = gb;
						if (hov && ctx.input.mouseClicked[0]) {
							mPropsOpen = !mPropsOpen;
							ctx.input.mouseClicked[0] = false;
						}
						rx -= bsz + ctx.S(6.f);
					}
					{ // pilule modèle (combo, aligné à droite) : « claude-3.5-sonnet ▾ »
						int32 nModels = 0;
						const char *const *models = kModels(nModels);
						const char *cur = models[mModelIdx < nModels ? mModelIdx : 0];
						const float32 chevW = ctx.S(14.f);
						const float32 pw = measure(cur) + ctx.S(20.f) + chevW;
						NkComboButton(ctx, dl, font, rx - pw, hcy, 0, nullptr, cur, 1, mComboOpen, mModelAnchor, kChip,
								 ctx.theme.buttonHover, ctx.theme.text); // largeur = pw (identique)
					}

					// ══ Onglets (Assistant général UNIQUEMENT) : Chat / Génération de Code / Revue de
					//    Code IA — façon Section 6 du spec Banani. ══
					if (mKind == 0)
						DrawViewTabs(ctx, {r.x, r.y + hdrH, r.w, tabsH}, kViolet);

					// ══ CORPS : conversation, OU sous-vue Génération/Revue (Assistant général). ══
					if (chatView) {
						DrawMessages(ctx, body, kViolet);
						// ══ BAS : champ de saisie (pleine largeur) PUIS barre d'outils (Mode/Portée/
						//    Édition + chips de contexte + envoi), façon Claude Code (VSCode). ══
						DrawInput(ctx, {r.x, r.y + r.h - toolH - inH, r.w, inH}, ic);
						DrawBottomToolbar(ctx, {r.x, r.y + r.h - toolH, r.w, toolH}, ic, kViolet, kVioletHov, kChip);
					} else if (mView == 1) {
						DrawCodeGenView(ctx, body, kViolet);
					} else if (mView == 2) {
						DrawCodeReviewView(ctx, body, kViolet);
					}

					// ══ Overlays (au-dessus de tout) : menu du combo ouvert (ouvre haut/bas selon la
					//    place dans `r`) · popover réglages. Ancre DEDIEE par combo (jamais partagee). ══
					if (mComboOpen == 1) {
						int32 n = 0;
						const char *const *m = kModels(n);
						NkComboMenu(ctx, dl, font, mModelAnchor, r, m, n, mModelIdx, mComboOpen, kViolet,
									ctx.theme.panel, ctx.theme.border, ctx.theme.text, NkColor{255, 255, 255, 255},
									ctx.theme.buttonHover);
					} else if (mComboOpen == 2) {
						DrawModeMenu(ctx, r, kViolet); // menu spécialisé : titre+description par option + Effort
					} else if (mComboOpen == 3) {
						const char *o[3] = {NkT("ai.scope.file"), NkT("ai.scope.sel"), NkT("ai.scope.ws")};
						NkComboMenu(ctx, dl, font, mScopeAnchor, r, o, 3, mScope, mComboOpen, kViolet, ctx.theme.panel,
									ctx.theme.border, ctx.theme.text, NkColor{255, 255, 255, 255}, ctx.theme.buttonHover);
					} else if (mComboOpen == 4 && ShowEditAuth()) {
						const char *o[3] = {NkT("ai.edit.read"), NkT("ai.edit.diff"), NkT("ai.edit.apply")};
						NkComboMenu(ctx, dl, font, mEditAuthAnchor, r, o, 3, mEditAuth, mComboOpen, kViolet,
									ctx.theme.panel, ctx.theme.border, ctx.theme.text, NkColor{255, 255, 255, 255},
									ctx.theme.buttonHover);
					} else if (mComboOpen == 5) { // Portée (onglet Revue de Code IA)
						const char *o[3] = {NkT("ai.scope.file"), NkT("ai.scope.sel"), NkT("ai.scope.ws")};
						NkComboMenu(ctx, dl, font, mRevScopeAnchor, r, o, 3, mRevScope, mComboOpen, kViolet,
									ctx.theme.panel, ctx.theme.border, ctx.theme.text, NkColor{255, 255, 255, 255},
									ctx.theme.buttonHover);
					} else if (mComboOpen == 6) { // Langage (onglet Génération de Code)
						int32 n = 0;
						const char *const *o = GenLangOpts(n);
						NkComboMenu(ctx, dl, font, mGenLangAnchor, r, o, n, mGenLangIdx, mComboOpen, kViolet,
									ctx.theme.panel, ctx.theme.border, ctx.theme.text, NkColor{255, 255, 255, 255},
									ctx.theme.buttonHover);
					}
					if (mPropsOpen)
						DrawPropsPopover(ctx, kViolet);
					if (mChatListOpen)
						DrawChatListMenu(ctx, kViolet);
					if (mPlusOpen)
						DrawPlusMenu(ctx, r);
					if (mActionsOpen)
						DrawActionsPalette(ctx, r, kViolet);
				}

			private:
				struct Msg {
						int32 role;
						NkString text;
				}; // role: 0 user, 1 assistant, 2 système/erreur

				// ── Conversation (façon Copilot/Claude Code : plusieurs chats par agent).
				// Chaque chat a SES PROPRES propriétés (saisie en cours, modèle, mode, portée,
				// autorisation d'édition, température, max tokens, système, effort...) — modifier
				// l'un n'impacte jamais les autres. ──
				struct ChatSession {
						NkString title;
						NkVector<Msg> msgs;
						float32 scroll = 0.f;
						bool stick = true; // colle en bas tant qu'on n'a pas scrollé
						char input[8192] = {0};	// brouillon de saisie, PROPRE a ce chat
						int32 modelIdx = 0;			// index dans ModelsFor(mKind)
						int32 mode = 0;				// 0 Agent, 1 Ask, 2 Edit (libellés varient par agent)
						int32 scope = 0;			// 0 Fichier courant, 1 Sélection, 2 Workspace
						int32 editAuth = 1;			// 0 Lecture seule, 1 Proposer diff, 2 Appliquer
						bool ctxFileOn = true;		// chip « fichier:lignes » actif
						float32 temp = 0.4f;		// température (slider)
						int32 maxTokens = 1024;
						char system[1024] = {0};	// instructions système personnalisées
						int32 effort = 2;			// 0 Low, 1 Medium, 2 High
						bool thinking = true;		// afficher le raisonnement (extended thinking)
						bool autoSwitchFlagged = false; // change de modele si un message est signale
				};

				NkCodeState *mS;
				NkEditorShell *mShell = nullptr; // pour focus terminal (onglet Agent)
				NkVector<ChatSession> mChats;
				int32 mActiveChat = 0;
				int32 mBusyChat = -1; // chat CIBLE de la reponse en cours (peut differer de l'actif si switch)
				int32 mChatSeq = 0;	   // numerotation "Chat N"
				bool mChatListOpen = false;
				NkRect mChatListAnchor = {0, 0, 0, 0};
				float32 mToolScroll = 0.f;	   // défilement horizontal de la barre d'outils du bas
				float32 mToolContentW = 0.f;  // largeur intrinsèque de son contenu (frame précédente)
				int32 mProvider = 0; // 0 Claude, 1 Ollama, 2 Maison
				NkProcess mProc;
				NkVector<NkString> mRespLines; // accumule la sortie curl entre frames
				bool mBusy = false;

				NkVector<Msg> &Msgs() { return mChats[static_cast<usize>(mActiveChat)].msgs; }
				const NkVector<Msg> &Msgs() const { return mChats[static_cast<usize>(mActiveChat)].msgs; }
				NkVector<Msg> &MsgsOf(int32 idx) { return mChats[static_cast<usize>(idx)].msgs; }

				void NewChat() {
					ChatSession c;
					c.title = NkPrintf("%s %d", NkT("ai.chat"), ++mChatSeq);
					c.msgs.PushBack({2, NkString(NkT("ai.hello"))});
					mChats.PushBack(c);
					mActiveChat = static_cast<int32>(mChats.Size()) - 1;
				}
				void DeleteChat(int32 idx) {
					if (idx < 0 || idx >= static_cast<int32>(mChats.Size()) || mChats.Size() <= 1)
						return; // toujours garder au moins 1 conversation
					mChats.RemoveAt(static_cast<usize>(idx));
					if (mActiveChat >= static_cast<int32>(mChats.Size()))
						mActiveChat = static_cast<int32>(mChats.Size()) - 1;
					else if (mActiveChat > idx)
						--mActiveChat;
				}
				// ── Identité de l'agent (profil) ──
				int32 mKind = 0;  // 0 Assistant général, 1 Claude Code, 2 Codex, 3 NkAI
				NkString mTitle;  // titre du panneau (= barre d'activité)
				// ── UI (design facon Copilot/Cursor/Claude Code) — panneau-wide, PAS par-chat ──
				int32 mComboOpen = 0;	// combo ouvert : 0 aucun, 1 modele, 2 mode, 3 portee, 4 edition
				// Ancre DEDIEE par combo (jamais partagee : sinon le dernier bouton dessine dans
				// la frame ecraserait l'ancre du combo reellement ouvert -> mauvais menu/position).
				NkRect mModelAnchor = {0, 0, 0, 0};
				NkRect mModeAnchor = {0, 0, 0, 0};
				NkRect mScopeAnchor = {0, 0, 0, 0};
				NkRect mEditAuthAnchor = {0, 0, 0, 0};
				NkRect mRevScopeAnchor = {0, 0, 0, 0}; // combo Portée de l'onglet Revue de Code (mComboOpen==5)
				NkRect mGenLangAnchor = {0, 0, 0, 0};  // combo Langage de l'onglet Génération de Code (mComboOpen==6)
				bool mDragTemp = false;	// glissement du slider de temperature
				bool mPropsOpen = false; // popover réglages (engrenage)
				NkRect mGearRect = {0, 0, 0, 0};
				// ── Onglet actif du panneau Assistant général (mKind==0 UNIQUEMENT — les
				// agents CLI Claude Code/Codex/NkAI restent Chat seul) : 0 Chat, 1 Génération
				// de Code, 2 Revue de Code IA (façon Section 6 du spec Banani). ──
				int32 mView = 0;
				// Génération de Code : réglage de génération, PAS par-chat (pas une conversation).
				char mGenDesc[2048] = {0};
				int32 mGenLangIdx = 0;
				bool mGenComments = true;
				bool mGenTests = false;
				bool mGenConventions = true;
				bool mGenVerbose = false;
				// Revue de Code IA : réglage de revue.
				int32 mRevScope = 0; // 0 fichier courant, 1 selection, 2 workspace (memes libelles que mScope)
				bool mRevBugs = true, mRevPerf = false, mRevSec = false, mRevRead = false, mRevArch = false;
				bool mRemoteControl = false;	 // "Enable Remote Control for all sessions" (cosmetique, global)
				// ── Menu « + » (Upload from computer / Add context / Browse the web) ──
				bool mPlusOpen = false;
				NkRect mPlusAnchor = {0, 0, 0, 0};
				// ── Palette d'actions filtrable (icone crayon) : Context/Modele/Personnaliser/
				// Commandes slash/Réglages/Support, façon Claude Code. ──
				bool mActionsOpen = false;
				NkRect mActionsAnchor = {0, 0, 0, 0};
				char mActionsFilter[128] = {0};
				float32 mActionsScroll = 0.f;
				bool mActionsScrollDrag = false; // glissement de la scrollbar de la palette

				// Modèles par agent (pilule + menu). Le provider effectif est dérivé du choix.
				const char *const *ModelsFor(int32 kind, int32 &n) const {
					static const char *general[] = {"claude-3.5-sonnet", "claude-sonnet-5", "Ollama (llama3.2)",
													"NkAI (maison)"};
					static const char *claudeCode[] = {"claude-3.5-sonnet", "claude-sonnet-5", "claude-opus-4"};
					static const char *codex[] = {"gpt-5-codex", "o4-mini"};
					static const char *nkai[] = {"NkAI-base (Rihen)"};
					switch (kind) {
						case 1:
							n = 3;
							return claudeCode;
						case 2:
							n = 2;
							return codex;
						case 3:
							n = 1;
							return nkai;
						default:
							n = 4;
							return general;
					}
				}
				const char *const *kModels(int32 &n) const { return ModelsFor(mKind, n); }

				// ── Modes d'interaction PAR AGENT : chaque IA a son propre vocabulaire de
				// permissions (le menu s'adapte, pas de "taille unique"). ──
				// (SANS `static` : NkT() doit être ré-évalué à chaque appel, sinon la traduction
				// resterait figée sur la langue active lors du 1er appel — bug de changement
				// de langue à chaud.)
				const char *const *ModeOptionsFor(int32 kind, int32 &n) const {
					const char *general[] = {NkT("ai.mode.agent"), NkT("ai.mode.ask"), NkT("ai.mode.edit")};
					const char *claudeCode[] = {NkT("ai.mode.manual"), NkT("ai.mode.autoedit"), NkT("ai.mode.plan"),
											   NkT("ai.mode.auto")};
					const char *codex[] = {NkT("ai.mode.suggest"), NkT("ai.mode.autoedit"), NkT("ai.mode.fullauto")};
					static const char *out[4];
					switch (kind) {
						case 1:
							n = 4;
							for (int32 i = 0; i < 4; ++i)
								out[i] = claudeCode[i];
							return out;
						case 2:
							n = 3;
							for (int32 i = 0; i < 3; ++i)
								out[i] = codex[i];
							return out;
						default: // 0 Assistant, 3 NkAI : notre propre chat -> notre propre vocabulaire
							n = 3;
							for (int32 i = 0; i < 3; ++i)
								out[i] = general[i];
							return out;
					}
				}
				// Descriptions courtes par mode (façon Claude Code), même index que ModeOptionsFor.
				const char *const *ModeDescFor(int32 kind, int32 &n) const {
					const char *general[] = {NkT("ai.mode.agent.desc"), NkT("ai.mode.ask.desc"), NkT("ai.mode.edit.desc")};
					const char *claudeCode[] = {NkT("ai.mode.manual.desc"), NkT("ai.mode.autoedit.desc"),
											   NkT("ai.mode.plan.desc"), NkT("ai.mode.auto.desc")};
					const char *codex[] = {NkT("ai.mode.suggest.desc"), NkT("ai.mode.autoedit.desc"),
										   NkT("ai.mode.fullauto.desc")};
					static const char *out[4];
					switch (kind) {
						case 1:
							n = 4;
							for (int32 i = 0; i < 4; ++i)
								out[i] = claudeCode[i];
							return out;
						case 2:
							n = 3;
							for (int32 i = 0; i < 3; ++i)
								out[i] = codex[i];
							return out;
						default:
							n = 3;
							for (int32 i = 0; i < 3; ++i)
								out[i] = general[i];
							return out;
					}
				}
				// L'autorisation d'édition dédiée n'a de sens QUE pour notre propre backend
				// (Assistant/NkAI) : les agents CLI (Claude Code/Codex) gèrent déjà cette
				// permission via LEUR mode -> pas de combo redondant pour eux.
				bool ShowEditAuth() const { return mKind == 0 || mKind == 3; }

				// ── Découpe `s` en lignes tenant dans maxW (mots ; repli caractère si besoin). ──
				static void WrapLines(NkGuiContext &ctx, const char *s, float32 maxW, NkVector<NkString> &out) {
					out.Clear();
					const bool valid = ctx.font && ctx.font->Valid();
					char line[1024];
					int32 n = 0;
					int32 lastSpace = -1;
					auto push = [&]() {
						line[n] = 0;
						out.PushBack(NkString(line));
						n = 0;
						lastSpace = -1;
					};
					for (const char *p = s;; ++p) {
						const char c = *p;
						if (c == '\0') {
							if (n > 0 || out.Empty())
								push();
							break;
						}
						if (c == '\r')
							continue;
						if (c == '\n') {
							push();
							continue;
						}
						if (n < 1022) {
							line[n] = c;
							if (c == ' ')
								lastSpace = n;
							++n;
							line[n] = 0;
						}
						if (valid && ctx.font->MeasureWidth(line) > maxW && n > 1) {
							if (lastSpace > 0 && lastSpace < n - 1) {
								char save[1024];
								int32 k = 0;
								for (int32 j = lastSpace + 1; j < n; ++j)
									save[k++] = line[j];
								save[k] = 0;
								line[lastSpace] = 0;
								out.PushBack(NkString(line));
								n = 0;
								lastSpace = -1;
								for (int32 j = 0; save[j]; ++j) {
									line[n++] = save[j];
								}
								line[n] = 0;
							} else {
								const char last = line[n - 1];
								line[n - 1] = 0;
								out.PushBack(NkString(line));
								n = 0;
								lastSpace = -1;
								line[n++] = last;
								line[n] = 0;
							}
						}
					}
				}

				// ── Onglets Chat / Génération de Code / Revue de Code IA (Assistant général
				// UNIQUEMENT — mKind==0). Simple sélecteur segmenté, pas de menu déroulant. ──
				void DrawViewTabs(NkGuiContext &ctx, const NkRect &r, const NkColor &violet) {
					auto &dl = ctx.DL();
					const NkGuiFont *font = ctx.font;
					const NkVec2 mp = ctx.input.mousePos;
					dl.AddRectFilled(r, ctx.theme.bgPrimary);
					dl.AddRectFilled({r.x, r.y + r.h - 1.f, r.w, 1.f}, ctx.theme.border);
					struct Tab {
							const char *label;
							int32 id;
					};
					const Tab tabs[3] = {{NkT("ai.tab.chat"), 0}, {NkT("ai.tab.gen"), 1}, {NkT("ai.tab.review"), 2}};
					const float32 pad = ctx.S(10.f);
					float32 x = r.x + pad;
					for (int32 i = 0; i < 3; ++i) {
						const float32 tw = (font && font->Valid()) ? font->MeasureWidth(tabs[i].label) : 0.f;
						const float32 w = tw + ctx.S(20.f);
						const NkRect tr = {x, r.y + ctx.S(4.f), w, r.h - ctx.S(8.f)};
						const bool sel = (mView == tabs[i].id);
						const bool hov = NkGuiRectContains(tr, mp);
						if (sel)
							dl.AddRectFilled(tr, NkColor{violet.r, violet.g, violet.b, 55}, ctx.S(6.f));
						else if (hov)
							dl.AddRectFilled(tr, ctx.theme.buttonHover, ctx.S(6.f));
						if (font && font->Valid())
							dl.AddText(font->Face(), font->TexId(),
									   {tr.x + ctx.S(10.f), tr.y + (tr.h - font->LineHeight()) * 0.5f + font->Ascent()},
									   tabs[i].label, sel ? violet : ctx.theme.textDisabled);
						if (sel)
							dl.AddRectFilled({tr.x, tr.y + tr.h - ctx.S(2.f), tr.w, ctx.S(2.f)}, violet, ctx.S(1.f));
						if (hov && ctx.input.mouseClicked[0]) {
							mView = tabs[i].id;
							mComboOpen = 0; // un combo ouvert d'une autre vue (ancre différente) ne doit pas survivre
							ctx.input.mouseClicked[0] = false;
						}
						x += w + ctx.S(4.f);
					}
				}

				// ── Conversation façon Claude Code (timeline) : le message UTILISATEUR est une
				// bulle pâle pleine largeur ; les réponses s'enchaînent sur un FIL VERTICAL à
				// puces (pas de bulle violette), avec un statut animé pendant la génération. ──
				void DrawMessages(NkGuiContext &ctx, const NkRect &body, const NkColor &violet) {
					auto &dl = ctx.DL();
					const NkVec2 mp = ctx.input.mousePos; // hover des boutons Copier/Insérer/Retry par-message
					NkVector<Msg> &msgs = Msgs();
					float32 &scroll = mChats[static_cast<usize>(mActiveChat)].scroll;
					bool &stick = mChats[static_cast<usize>(mActiveChat)].stick;
					const float32 lineH = ctx.font && ctx.font->Valid() ? ctx.font->LineHeight() : 16.f;
					const float32 asc = ctx.font && ctx.font->Valid() ? ctx.font->Ascent() : 12.f;
					const float32 pad = ctx.S(12.f), bubPad = ctx.S(11.f), gap = ctx.S(14.f), rnd = ctx.S(10.f);
					const NkColor kUserBg = {42, 46, 58, 255}; // bulle utilisateur PALE (pas violette)
					const float32 userMaxW = body.w - pad * 2.f;
					const float32 userTextMaxW = userMaxW - bubPad * 2.f;
					// Fil vertical (thread) : puce a threadX, texte a partir de threadX+dotGap.
					const float32 threadX = body.x + pad + ctx.S(3.f);
					const float32 dotGap = ctx.S(16.f);
					const float32 threadTextMaxW = body.w - pad - dotGap - ctx.S(4.f) - pad;
					const bool typing = mBusy && mBusyChat == mActiveChat;

					// 1) mesure de la hauteur totale (bulle pour user, texte simple pour le reste).
					// Les réponses assistant (role 1) réservent une ligne d'actions (Copier /
					// Insérer dans l'éditeur / Retry) sous le texte, façon Copilot Chat/Cursor.
					const float32 actRowH = ctx.S(24.f);
					NkVector<NkVector<NkString>> wrapped;
					float32 total = pad;
					for (usize i = 0; i < msgs.Size(); ++i) {
						const bool user = (msgs[i].role == 0);
						NkVector<NkString> ls;
						WrapLines(ctx, msgs[i].text.CStr(), user ? userTextMaxW : threadTextMaxW, ls);
						total += ls.Size() * lineH + (user ? bubPad * 2.f : 0.f) + (msgs[i].role == 1 ? actRowH : 0.f) + gap;
						wrapped.PushBack(ls);
					}
					if (typing)
						total += lineH * 2.f + gap; // statut « Thinking… » + « Mustering… »
					const float32 viewH = body.h;
					const float32 maxScroll = total > viewH ? (total - viewH) : 0.f;
					if (NkGuiRectContains(body, ctx.input.mousePos) && ctx.input.wheel != 0.f) {
						scroll -= ctx.input.wheel * lineH * 3.f;
						ctx.input.wheel = 0.f;
						stick = false;
					}
					if (stick)
						scroll = maxScroll;
					if (scroll < 0.f)
						scroll = 0.f;
					if (scroll > maxScroll)
						scroll = maxScroll;
					if (scroll >= maxScroll - 1.f)
						stick = true;

					// 2) rendu (clippé).
					dl.PushClipRect(body, true);
					float32 y = body.y + pad - scroll;
					// Trait du fil : trace UNIQUEMENT entre deux puces CONSECUTIVES (jamais a
					// travers une bulle utilisateur, qui casse la chaine) — sinon le trait
					// traverse visuellement le texte d'un message utilisateur intercale.
					float32 prevDotY = -1.f;
					// Retry différé : NE JAMAIS muter `msgs` pendant cette boucle (RetryLast()
					// fait un RemoveAt() qui invaliderait `wrapped`/les indices en cours d'itération).
					bool doRetry = false;
					for (usize i = 0; i < msgs.Size(); ++i) {
						const Msg &m = msgs[i];
						const NkVector<NkString> &ls = wrapped[i];
						const bool user = (m.role == 0);
						if (user) {
							prevDotY = -1.f; // casse la chaine : le fil ne traverse pas cette bulle
							const float32 bh = ls.Size() * lineH + bubPad * 2.f;
							if (y + bh >= body.y && y <= body.y + body.h) {
								dl.AddRectFilled({body.x + pad, y, userMaxW, bh}, kUserBg, rnd);
								if (ctx.font && ctx.font->Valid())
									for (usize k = 0; k < ls.Size(); ++k)
										dl.AddText(ctx.font->Face(), ctx.font->TexId(),
												   {body.x + pad + bubPad, y + bubPad + k * lineH + asc}, ls[k].CStr(),
												   ctx.theme.text);
							}
							y += bh + gap;
						} else { // assistant/systeme : puce sur le fil + texte simple (pas de bulle)
							const float32 bh = ls.Size() * lineH;
							const bool isAssistant = (m.role == 1); // pas les messages systeme/erreur (role 2)
							const float32 blockH = bh + (isAssistant ? actRowH : 0.f);
							const float32 dotY = y + lineH * 0.5f;
							if (prevDotY >= 0.f)
								dl.AddLine({threadX, prevDotY}, {threadX, dotY}, ctx.theme.border, 1.2f);
							prevDotY = dotY;
							if (y + blockH >= body.y && y <= body.y + body.h) {
								const NkColor dotCol = (m.role == 2) ? ctx.theme.textDisabled : NkColor{88, 209, 143, 255};
								dl.AddCircleFilled({threadX, dotY}, ctx.S(3.f), dotCol);
								const NkColor fg = (m.role == 2) ? ctx.theme.textDisabled : ctx.theme.text;
								if (ctx.font && ctx.font->Valid())
									for (usize k = 0; k < ls.Size(); ++k)
										dl.AddText(ctx.font->Face(), ctx.font->TexId(),
												   {threadX + dotGap, y + k * lineH + asc}, ls[k].CStr(), fg);
								if (isAssistant) {
									// Ligne d'actions : Copier · Insérer dans l'éditeur · Retry (dernier
									// message seulement — regénère en renvoyant le même tour utilisateur).
									const float32 acy = y + bh + actRowH * 0.5f;
									const float32 asz = ctx.S(20.f), agap = ctx.S(4.f);
									float32 ax = threadX + dotGap;
									if (ActionIconBtn(ctx, dl, ax, acy, asz, &DrawCopyIcon, mp)) {
										ctx.SetClipboard(m.text.CStr());
										ctx.input.mouseClicked[0] = false;
									}
									ax += asz + agap;
									if (ActionIconBtn(ctx, dl, ax, acy, asz, &DrawInsertIcon, mp)) {
										if (mS && mS->HasActive())
											mS->files[mS->active].doc.InsertText(m.text.CStr());
										ctx.input.mouseClicked[0] = false;
									}
									ax += asz + agap;
									if (!mBusy && i == msgs.Size() - 1) { // Retry : uniquement la derniere reponse
										if (ActionIconBtn(ctx, dl, ax, acy, asz, &DrawRetryIcon, mp)) {
											doRetry = true; // appliqué APRÈS la boucle (ne pas muter `msgs` ici)
											ctx.input.mouseClicked[0] = false;
										}
									}
								}
							}
							y += blockH + gap;
						}
					}
					// Statut animé pendant la génération : puce + "Thinking… · Nk tokens" (dim)
					// puis "Mustering…" (accent) — façon Claude Code, remplace l'ancien "...".
					if (typing) {
						const float32 dotY = y + lineH * 0.5f;
						if (prevDotY >= 0.f)
							dl.AddLine({threadX, prevDotY}, {threadX, dotY}, ctx.theme.border, 1.2f);
						int32 used = 0;
						for (usize i = 0; i < msgs.Size(); ++i)
							used += (int32)msgs[i].text.Size();
						used /= 4;
						const NkString status1 = NkPrintf("%s… · %s tokens", NkT("ai.thinking"), CompactTok(used).CStr());
						dl.AddCircleFilled({threadX, dotY}, ctx.S(3.f), ctx.theme.textDisabled);
						if (ctx.font && ctx.font->Valid())
							dl.AddText(ctx.font->Face(), ctx.font->TexId(), {threadX + dotGap, y + asc}, status1.CStr(),
									   ctx.theme.textDisabled);
						y += lineH;
						const NkColor kOrange = {230, 140, 60, 255};
						dl.AddLine({threadX, dotY}, {threadX, y + lineH * 0.5f}, ctx.theme.border, 1.2f);
						DrawSpinnerAsterisk(dl, {threadX, y + lineH * 0.5f}, ctx.S(5.f), (float32)ctx.time, kOrange);
						if (ctx.font && ctx.font->Valid())
							dl.AddText(ctx.font->Face(), ctx.font->TexId(), {threadX + dotGap, y + asc},
									   NkT("ai.mustering"), kOrange);
					}
					dl.PopClipRect();
					if (doRetry) // appliqué ICI (après la boucle) : RetryLast() mute `msgs` (RemoveAt + Send)
						RetryLast();
				}

				// ── Petit astérisque tournant (statut "en cours"), façon Claude Code. ──
				static void DrawSpinnerAsterisk(NkGuiDrawList &dl, const NkVec2 &c, float32 rad, float32 t,
												const NkColor &col) {
					for (int32 i = 0; i < 3; ++i) {
						const float32 a = t * 3.0f + i * 1.0472f; // 2pi/3 par branche
						const float32 dx = std::cos(a) * rad, dy = std::sin(a) * rad;
						dl.AddLine({c.x - dx, c.y - dy}, {c.x + dx, c.y + dy}, col, 1.6f);
					}
				}

				// ── Engrenage vectoriel (réglages) : 8 dents + disque + trou sombre. ──
				static void DrawGear(NkGuiDrawList &dl, const NkVec2 &c, float32 rad, const NkColor &col) {
					static const float32 dx[8] = {1.f, 0.7071f, 0.f, -0.7071f, -1.f, -0.7071f, 0.f, 0.7071f};
					static const float32 dy[8] = {0.f, 0.7071f, 1.f, 0.7071f, 0.f, -0.7071f, -1.f, -0.7071f};
					for (int32 i = 0; i < 8; ++i)
						dl.AddLine({c.x + dx[i] * rad * 0.7f, c.y + dy[i] * rad * 0.7f},
								   {c.x + dx[i] * rad * 1.3f, c.y + dy[i] * rad * 1.3f}, col, 2.f);
					dl.AddCircleFilled(c, rad * 0.8f, col);
					dl.AddCircleFilled(c, rad * 0.34f, NkColor{20, 22, 28, 255});
				}
				// ── Thermomètre (température) : tige + bulbe. ──
				static void DrawThermo(NkGuiDrawList &dl, const NkVec2 &c, float32 h, const NkColor &col) {
					dl.AddRect({c.x - h * 0.16f, c.y - h * 0.5f, h * 0.32f, h * 0.7f}, col, 1.2f);
					dl.AddCircleFilled({c.x, c.y + h * 0.36f}, h * 0.3f, col);
				}
				static NkString CompactTok(int32 t) {
					return t >= 1000 ? NkPrintf("%.1fk", (double)t / 1000.0) : NkPrintf("%d", t);
				}
				// Provider effectif selon l'agent (mKind) et le modèle choisi.
				//   0 Claude API · 1 Ollama · 2 IA maison (NkAI) · 5 Codex/OpenAI (stub)
				int32 ProviderForModel() const {
					if (mKind == 2)
						return 5; // Codex -> OpenAI (intégration à venir)
					if (mKind == 3)
						return 2; // NkAI -> IA maison
					if (mKind == 0) {
						if (mModelIdx == 2)
							return 1; // Ollama
						if (mModelIdx == 3)
							return 2; // NkAI maison
					}
					return 0; // Assistant (claude-*) ou Claude Code -> Claude API
				}

				// ── Chips de contexte : fichier:ligne (✕) + bouton (+). Dessine à partir
				//    de `x0` (comme ComboPill) et renvoie le nouveau `x` — s'enchaîne dans la barre
				//    d'outils du bas avec les combos Mode/Portée/Édition. ──
				float32 DrawChips(NkGuiContext &ctx, float32 x0, float32 cy, NkIcons *ic, const NkColor &chip) {
					auto &dl = ctx.DL();
					const NkGuiFont *font = ctx.font;
					const NkVec2 mp = ctx.input.mousePos;
					const float32 h = ctx.ItemHeight(), gap = ctx.S(6.f);
					auto textAt = [&](float32 x, const char *s, const NkColor &col) {
						if (font && font->Valid())
							dl.AddText(font->Face(), font->TexId(), {x, cy - font->LineHeight() * 0.5f + font->Ascent()},
									   s, col);
					};
					auto measure = [&](const char *s) { return (font && font->Valid()) ? font->MeasureWidth(s) : 0.f; };
					float32 x = x0;
					if (mCtxFileOn && mS && mS->HasActive()) {
						OpenFile &f = mS->files[mS->active];
						const NkString lbl = NkPrintf("%s:%d", f.Name().CStr(), f.doc.curLine + 1);
						const float32 iw = ctx.S(13.f), xw = ctx.S(13.f), tw = measure(lbl.CStr());
						const float32 cw = ctx.S(8.f) + iw + ctx.S(5.f) + tw + ctx.S(6.f) + xw + ctx.S(6.f);
						const NkRect ch = {x, cy - h * 0.5f, cw, h};
						dl.AddRectFilled(ch, chip, ctx.S(6.f));
						if (ic && ic->fileText)
							dl.AddImage(ic->fileText, {ch.x + ctx.S(7.f), cy - iw * 0.5f, iw, iw}, {0.f, 0.f}, {1.f, 1.f},
										ctx.theme.textDisabled);
						textAt(ch.x + ctx.S(8.f) + iw + ctx.S(5.f), lbl.CStr(), ctx.theme.text);
						const NkRect xb = {ch.x + cw - xw - ctx.S(5.f), cy - xw * 0.5f, xw, xw};
						const bool xhov = NkGuiRectContains(xb, mp);
						const NkColor xc = xhov ? ctx.theme.text : ctx.theme.textDisabled;
						const float32 a = ctx.S(3.f), xcx = xb.x + xw * 0.5f;
						dl.AddLine({xcx - a, cy - a}, {xcx + a, cy + a}, xc, 1.4f);
						dl.AddLine({xcx - a, cy + a}, {xcx + a, cy - a}, xc, 1.4f);
						if (xhov && ctx.input.mouseClicked[0])
							mCtxFileOn = false;
						x += cw + gap;
					}
					// Bouton (+) retiré d'ici : fixe en bas à gauche de la barre d'outils
					// (DrawBottomToolbar), à côté du crayon — plus dans cette rangée de chips.
					return x;
				}

				// Nombre de lignes affichées par le champ de saisie (auto-grandit, borné à 6).
				int32 InputRows(NkGuiContext &ctx, float32 innerW) const {
					if (!ctx.font || !ctx.font->Valid() || innerW < 10.f)
						return 1;
					NkVector<NkString> ls;
					WrapLines(ctx, mInput[0] ? mInput : " ", innerW, ls);
					int32 n = (int32)ls.Size();
					if (n < 1)
						n = 1;
					return n > 6 ? 6 : n;
				}

				// ── Saisie : champ PLEINE LARGEUR qui GRANDIT + placeholder (sans bouton — l'envoi
				//    est dans la barre d'outils du bas, façon Claude Code/VSCode).
				//    Entrée = ENVOYER · Maj+Entrée = nouvelle ligne (jamais d'envoi par caractère). ──
				void DrawInput(NkGuiContext &ctx, const NkRect &inR, NkIcons *ic) {
					(void)ic;
					auto &dl = ctx.DL();
					const NkGuiFont *font = ctx.font;
					dl.AddRectFilled(inR, ctx.theme.bgPrimary);
					const float32 pad = ctx.S(10.f);
					const NkRect field = {inR.x + pad, inR.y + ctx.S(6.f), inR.w - pad * 2.f, inR.h - ctx.S(12.f)};

					// Entrée (sans Maj) = ENVOYER : on CONSOMME la touche AVANT le widget pour
					// empêcher l'insertion d'un saut de ligne. Maj+Entrée = nouvelle ligne (widget).
					const NkGuiId fid = ctx.GetId("##aiInput");
					const bool focused = (ctx.inputId == fid);
					bool enterSend = false;
					if (focused && !ctx.input.shiftDown && ctx.input.KeyPressed(NkGuiKey::Enter)) {
						enterSend = true;
						ctx.input.keyInit[(int32)NkGuiKey::Enter] = false; // consommée
					}
					InputTextMultiline(ctx, "##aiInput", mInput, sizeof(mInput), field, NkGuiInputFlags::None,
									   sizeof(mInput) - 1, /*wrap=*/true);
					if (mInput[0] == 0 && font && font->Valid())
						dl.AddText(font->Face(), font->TexId(),
								   {field.x + ctx.S(8.f), field.y + ctx.S(6.f) + font->Ascent()}, NkT("ai.ask"),
								   ctx.theme.textDisabled);
					if (enterSend && !mBusy && mInput[0] != 0)
						Send();
				}

				// ── Ligne case-à-cocher (toggle) réutilisée par Génération de Code / Revue de
				// Code IA : libellé à gauche + interrupteur à droite (même style que la palette
				// d'actions). ──
				static void DrawToggleRow(NkGuiContext &ctx, NkGuiDrawList &dl, const NkRect &row, const char *label,
										  bool &val) {
					const NkGuiFont *font = ctx.font;
					const NkVec2 mp = ctx.input.mousePos;
					if (font && font->Valid())
						dl.AddText(font->Face(), font->TexId(),
								   {row.x, row.y + (row.h - font->LineHeight()) * 0.5f + font->Ascent()}, label,
								   ctx.theme.text);
					const float32 sw = ctx.S(30.f), sh = ctx.S(15.f);
					const NkRect sr = {row.x + row.w - sw, row.y + (row.h - sh) * 0.5f, sw, sh};
					dl.AddRectFilled(sr, val ? NkColor{72, 145, 232, 255} : ctx.theme.button, sh * 0.5f);
					dl.AddCircleFilled({val ? sr.x + sr.w - sh * 0.5f : sr.x + sh * 0.5f, sr.y + sh * 0.5f},
									   sh * 0.5f - ctx.S(1.5f), NkColor{255, 255, 255, 255});
					if (NkGuiRectContains(row, mp) && ctx.input.mouseClicked[0]) {
						val = !val;
						ctx.input.mouseClicked[0] = false;
					}
				}

				// ── Langages source réellement compilés par Jenga (Core/Api.py) — pas une liste
				// générique : la Génération de Code ne propose que ce que le projet sait construire. ──
				static const char *const *GenLangOpts(int32 &n) {
					static const char *langs[6] = {"C++", "C", "Objective-C", "Assembly", "Rust", "Zig"};
					n = 6;
					return langs;
				}

				// ── Onglet « 6.2 Génération de Code » (Assistant général UNIQUEMENT) : description +
				// langage + options, puis Générer/Régénérer compose un prompt et l'envoie via le
				// CHAT existant — pas de second backend à maintenir, Send() gère déjà requête +
				// streaming + affichage (avec les boutons Copier/Insérer/Retry par-message). ──
				void DrawCodeGenView(NkGuiContext &ctx, const NkRect &r, const NkColor &violet) {
					auto &dl = ctx.DL();
					const NkGuiFont *font = ctx.font;
					const NkVec2 mp = ctx.input.mousePos;
					dl.AddRectFilled(r, ctx.theme.bgPrimary);
					const float32 pad = ctx.S(14.f), it = ctx.ItemHeight();
					float32 y = r.y + pad;
					auto txt = [&](const char *s, const NkColor &c) {
						if (font && font->Valid())
							dl.AddText(font->Face(), font->TexId(), {r.x + pad, y + font->Ascent()}, s, c);
					};
					txt(NkT("ai.gen.title"), ctx.theme.textDisabled);
					y += it + ctx.S(10.f);

					const float32 descH = ctx.S(120.f);
					const NkRect descR = {r.x + pad, y, r.w - pad * 2.f, descH};
					InputTextMultiline(ctx, "##aiGenDesc", mGenDesc, sizeof(mGenDesc), descR, NkGuiInputFlags::None,
									   sizeof(mGenDesc) - 1, /*wrap=*/true);
					if (mGenDesc[0] == 0 && font && font->Valid())
						dl.AddText(font->Face(), font->TexId(),
								   {descR.x + ctx.S(8.f), descR.y + ctx.S(6.f) + font->Ascent()}, NkT("ai.gen.descph"),
								   ctx.theme.textDisabled);
					y += descH + ctx.S(16.f);

					// Langage (combo).
					{
						const char *lbl = NkT("ai.gen.lang");
						txt(lbl, ctx.theme.textDisabled);
						int32 n = 0;
						const char *const *langs = GenLangOpts(n);
						const float32 lw = (font && font->Valid()) ? font->MeasureWidth(lbl) : 0.f;
						NkComboButton(ctx, dl, font, r.x + pad + lw + ctx.S(12.f), y + it * 0.5f, 0, nullptr,
									 langs[mGenLangIdx < n ? mGenLangIdx : 0], 6, mComboOpen, mGenLangAnchor,
									 ctx.theme.button, ctx.theme.buttonHover, ctx.theme.text);
						y += it + ctx.S(14.f);
					}

					// Options.
					const float32 rowH = it + ctx.S(6.f);
					DrawToggleRow(ctx, dl, {r.x + pad, y, r.w - pad * 2.f, rowH}, NkT("ai.gen.opt.comments"),
								 mGenComments);
					y += rowH;
					DrawToggleRow(ctx, dl, {r.x + pad, y, r.w - pad * 2.f, rowH}, NkT("ai.gen.opt.tests"), mGenTests);
					y += rowH;
					DrawToggleRow(ctx, dl, {r.x + pad, y, r.w - pad * 2.f, rowH}, NkT("ai.gen.opt.conventions"),
								 mGenConventions);
					y += rowH;
					DrawToggleRow(ctx, dl, {r.x + pad, y, r.w - pad * 2.f, rowH}, NkT("ai.gen.opt.verbose"),
								 mGenVerbose);
					y += rowH + ctx.S(16.f);

					// Générer / Régénérer (violet, désactivé tant qu'aucune description n'est saisie).
					const bool canGen = mGenDesc[0] != 0 && !mBusy;
					const char *btnLbl = Msgs().Empty() ? NkT("ai.gen.generate") : NkT("ai.gen.regenerate");
					const float32 bw = (font && font->Valid() ? font->MeasureWidth(btnLbl) : 40.f) + ctx.S(28.f);
					const NkRect btn = {r.x + pad, y, bw, it + ctx.S(10.f)};
					const bool hov = canGen && NkGuiRectContains(btn, mp);
					dl.AddRectFilled(btn, canGen ? (hov ? NkColor{143, 128, 250, 255} : violet) : ctx.theme.button,
									ctx.S(6.f));
					if (font && font->Valid())
						dl.AddText(font->Face(), font->TexId(),
								   {btn.x + ctx.S(14.f), btn.y + (btn.h - font->LineHeight()) * 0.5f + font->Ascent()},
								   btnLbl, canGen ? NkColor{255, 255, 255, 255} : ctx.theme.textDisabled);
					if (canGen && hov && ctx.input.mouseClicked[0]) {
						ctx.input.mouseClicked[0] = false;
						SendCodeGenPrompt();
					}
				}

				// ── Compose le prompt de génération (langage + description + options cochées),
				// le pousse dans la saisie du chat ACTIF puis rejoue le chemin d'envoi normal
				// (Send()) — bascule sur l'onglet Chat pour regarder la génération en direct. ──
				void SendCodeGenPrompt() {
					int32 n = 0;
					const char *const *langs = GenLangOpts(n);
					const char *lang = langs[mGenLangIdx < n ? mGenLangIdx : 0];
					NkString prompt = NkPrintf("Génère du code %s pour : %s\n", lang, mGenDesc);
					if (mGenComments)
						prompt += NkString("- ") + NkT("ai.gen.opt.comments") + ".\n";
					if (mGenTests)
						prompt += NkString("- ") + NkT("ai.gen.opt.tests") + ".\n";
					if (mGenConventions)
						prompt += NkString("- ") + NkT("ai.gen.opt.conventions") + ".\n";
					if (mGenVerbose)
						prompt += NkString("- ") + NkT("ai.gen.opt.verbose") + ".\n";
					const char *s = prompt.CStr();
					usize k = 0;
					while (s[k] && k < sizeof(mInput) - 1) {
						mInput[k] = s[k];
						++k;
					}
					mInput[k] = 0;
					mView = 0; // bascule sur Chat : la génération se déroule dans le fil normal
					Send();
				}

				// ── La Revue de Code IA a besoin d'une cible reelle avant de pouvoir analyser :
				// fichier/selection -> un fichier doit etre actif (selection -> selection non vide) ;
				// workspace -> toujours pret (pas de contenu de fichier a joindre, juste une note). ──
				bool ReviewScopeReady() const {
					if (mRevScope == 2)
						return true;
					if (!mS || !mS->HasActive())
						return false;
					return mRevScope != 1 || mS->files[mS->active].doc.HasSel();
				}

				// ── Onglet « 6.3 Revue de Code IA » (Assistant général UNIQUEMENT) : portée + focus,
				// puis Analyser joint le VRAI code (fichier/sélection) au prompt et l'envoie via le
				// chat existant — aucun faux widget de résultats structurés (pas de parsing IA
				// fiable côté client) : la réponse s'affiche normalement, avec Copier/Insérer/Retry. ──
				void DrawCodeReviewView(NkGuiContext &ctx, const NkRect &r, const NkColor &violet) {
					auto &dl = ctx.DL();
					const NkGuiFont *font = ctx.font;
					const NkVec2 mp = ctx.input.mousePos;
					dl.AddRectFilled(r, ctx.theme.bgPrimary);
					const float32 pad = ctx.S(14.f), it = ctx.ItemHeight();
					float32 y = r.y + pad;
					auto txt = [&](const char *s, const NkColor &c) {
						if (font && font->Valid())
							dl.AddText(font->Face(), font->TexId(), {r.x + pad, y + font->Ascent()}, s, c);
					};
					txt(NkT("ai.review.title"), ctx.theme.textDisabled);
					y += it + ctx.S(10.f);

					// Portée (combo, memes libelles fichier/selection/workspace que le Chat).
					{
						const char *lbl = NkT("ai.scope.lbl");
						txt(lbl, ctx.theme.textDisabled);
						const char *o[3] = {NkT("ai.scope.file"), NkT("ai.scope.sel"), NkT("ai.scope.ws")};
						const float32 lw = (font && font->Valid()) ? font->MeasureWidth(lbl) : 0.f;
						NkComboButton(ctx, dl, font, r.x + pad + lw + ctx.S(12.f), y + it * 0.5f, 0, nullptr,
									 o[mRevScope < 3 ? mRevScope : 0], 5, mComboOpen, mRevScopeAnchor,
									 ctx.theme.button, ctx.theme.buttonHover, ctx.theme.text);
						y += it + ctx.S(14.f);
					}

					txt(NkT("ai.review.focus"), ctx.theme.textDisabled);
					y += it;
					const float32 rowH = it + ctx.S(6.f);
					DrawToggleRow(ctx, dl, {r.x + pad, y, r.w - pad * 2.f, rowH}, NkT("ai.review.focus.bugs"),
								 mRevBugs);
					y += rowH;
					DrawToggleRow(ctx, dl, {r.x + pad, y, r.w - pad * 2.f, rowH}, NkT("ai.review.focus.perf"),
								 mRevPerf);
					y += rowH;
					DrawToggleRow(ctx, dl, {r.x + pad, y, r.w - pad * 2.f, rowH}, NkT("ai.review.focus.sec"),
								 mRevSec);
					y += rowH;
					DrawToggleRow(ctx, dl, {r.x + pad, y, r.w - pad * 2.f, rowH}, NkT("ai.review.focus.read"),
								 mRevRead);
					y += rowH;
					DrawToggleRow(ctx, dl, {r.x + pad, y, r.w - pad * 2.f, rowH}, NkT("ai.review.focus.arch"),
								 mRevArch);
					y += rowH + ctx.S(12.f);

					const bool ready = ReviewScopeReady();
					if (!ready && font && font->Valid()) {
						NkVector<NkString> ls;
						WrapLines(ctx, NkT("ai.review.noscope"), r.w - pad * 2.f, ls);
						for (usize k = 0; k < ls.Size(); ++k)
							dl.AddText(font->Face(), font->TexId(),
									   {r.x + pad, y + k * font->LineHeight() + font->Ascent()}, ls[k].CStr(),
									   ctx.theme.textDisabled);
						y += ls.Size() * font->LineHeight() + ctx.S(12.f);
					}

					const bool canReview = ready && !mBusy;
					const char *btnLbl = NkT("ai.review.analyze");
					const float32 bw = (font && font->Valid() ? font->MeasureWidth(btnLbl) : 40.f) + ctx.S(28.f);
					const NkRect btn = {r.x + pad, y, bw, it + ctx.S(10.f)};
					const bool hov = canReview && NkGuiRectContains(btn, mp);
					dl.AddRectFilled(btn, canReview ? (hov ? NkColor{143, 128, 250, 255} : violet) : ctx.theme.button,
									ctx.S(6.f));
					if (font && font->Valid())
						dl.AddText(font->Face(), font->TexId(),
								   {btn.x + ctx.S(14.f), btn.y + (btn.h - font->LineHeight()) * 0.5f + font->Ascent()},
								   btnLbl, canReview ? NkColor{255, 255, 255, 255} : ctx.theme.textDisabled);
					if (canReview && hov && ctx.input.mouseClicked[0]) {
						ctx.input.mouseClicked[0] = false;
						SendReviewPrompt();
					}
				}

				// ── Compose le prompt de revue (portée + focus cochés) en joignant le VRAI code
				// (fichier entier ou sélection) puis rejoue le chemin d'envoi normal (Send()). ──
				void SendReviewPrompt() {
					const char *scopeLbl = (mRevScope == 0)   ? NkT("ai.scope.file")
										  : (mRevScope == 1) ? NkT("ai.scope.sel")
															  : NkT("ai.scope.ws");
					NkString prompt = NkPrintf("Fais une revue de code (%s).\nConcentre-toi sur :\n", scopeLbl);
					if (mRevBugs)
						prompt += NkString("- ") + NkT("ai.review.focus.bugs") + "\n";
					if (mRevPerf)
						prompt += NkString("- ") + NkT("ai.review.focus.perf") + "\n";
					if (mRevSec)
						prompt += NkString("- ") + NkT("ai.review.focus.sec") + "\n";
					if (mRevRead)
						prompt += NkString("- ") + NkT("ai.review.focus.read") + "\n";
					if (mRevArch)
						prompt += NkString("- ") + NkT("ai.review.focus.arch") + "\n";
					if (mRevScope != 2 && mS && mS->HasActive()) {
						OpenFile &f = mS->files[mS->active];
						const NkString code = (mRevScope == 1) ? f.doc.GetSelectedText() : f.doc.GetText();
						prompt += NkString("\nFichier : ") + f.Name().CStr() + "\n```\n" + code.CStr() + "\n```\n";
					} else if (mRevScope == 2) {
						prompt += "\n(Analyse au niveau du workspace : base-toi sur le contexte du projet.)\n";
					}
					const char *s = prompt.CStr();
					usize k = 0;
					while (s[k] && k < sizeof(mInput) - 1) {
						mInput[k] = s[k];
						++k;
					}
					mInput[k] = 0;
					mView = 0;
					Send();
				}

				// ── Glyphes vectoriels (pas d'asset) pour les combos icone-seule Portée/Édition. ──
				static void DrawFileIcon(NkGuiDrawList &dl, const NkVec2 &c, float32 rad, const NkColor &col) {
					const float32 w = rad * 1.1f, h = rad * 1.4f, fold = rad * 0.4f;
					const NkRect r = {c.x - w * 0.5f, c.y - h * 0.5f, w, h};
					dl.AddRect(r, col, 1.3f);
					dl.AddLine({r.x + w - fold, r.y}, {r.x + w - fold, r.y + fold}, col, 1.2f);
					dl.AddLine({r.x + w - fold, r.y + fold}, {r.x + w, r.y + fold}, col, 1.2f);
				}
				static void DrawSelIcon(NkGuiDrawList &dl, const NkVec2 &c, float32 rad, const NkColor &col) {
					// Texte "sélectionné" : lignes fines + une bande surlignée (façon glisser-surligner).
					const float32 w = rad * 1.6f;
					dl.AddLine({c.x - w * 0.5f, c.y - rad * 0.55f}, {c.x + w * 0.5f, c.y - rad * 0.55f}, col, 1.2f);
					dl.AddRectFilled({c.x - w * 0.5f, c.y - rad * 0.15f, w * 0.7f, rad * 0.5f},
									 NkColor{col.r, col.g, col.b, 90});
					dl.AddLine({c.x - w * 0.5f, c.y + rad * 0.55f}, {c.x + w * 0.2f, c.y + rad * 0.55f}, col, 1.2f);
				}
				static void DrawWorkspaceIcon(NkGuiDrawList &dl, const NkVec2 &c, float32 rad, const NkColor &col) {
					const float32 s = rad * 0.75f, g = rad * 0.25f;
					dl.AddRect({c.x - s - g * 0.5f, c.y - s - g * 0.5f, s, s}, col, 1.2f);
					dl.AddRect({c.x + g * 0.5f, c.y - s - g * 0.5f, s, s}, col, 1.2f);
					dl.AddRect({c.x - s * 0.5f, c.y + g * 0.5f, s, s}, col, 1.2f);
				}
				static void DrawLockIcon(NkGuiDrawList &dl, const NkVec2 &c, float32 rad, const NkColor &col) {
					const float32 bw = rad * 1.3f, bh = rad * 1.0f;
					const NkRect body = {c.x - bw * 0.5f, c.y - bh * 0.15f, bw, bh};
					dl.AddRect(body, col, 1.3f);
					dl.AddCircleFilled({c.x, body.y}, bw * 0.32f, {0, 0, 0, 0}); // (no-op, garde le style rond coherent)
					dl.AddLine({c.x - bw * 0.3f, body.y}, {c.x - bw * 0.3f, body.y - rad * 0.5f}, col, 1.4f);
					dl.AddLine({c.x + bw * 0.3f, body.y}, {c.x + bw * 0.3f, body.y - rad * 0.5f}, col, 1.4f);
					dl.AddLine({c.x - bw * 0.3f, body.y - rad * 0.5f}, {c.x + bw * 0.3f, body.y - rad * 0.5f}, col, 1.4f);
				}
				static void DrawDiffIcon(NkGuiDrawList &dl, const NkVec2 &c, float32 rad, const NkColor &col) {
					// Crayon simplifie (pas d'asset "editer" garanti a toutes les tailles).
					dl.AddLine({c.x - rad * 0.6f, c.y + rad * 0.6f}, {c.x + rad * 0.6f, c.y - rad * 0.6f}, col, 1.6f);
					dl.AddLine({c.x + rad * 0.2f, c.y - rad * 0.9f}, {c.x + rad * 0.9f, c.y - rad * 0.2f}, col, 1.6f);
				}
				static void DrawCheckIcon(NkGuiDrawList &dl, const NkVec2 &c, float32 rad, const NkColor &col) {
					dl.AddLine({c.x - rad * 0.7f, c.y}, {c.x - rad * 0.15f, c.y + rad * 0.6f}, col, 1.8f);
					dl.AddLine({c.x - rad * 0.15f, c.y + rad * 0.6f}, {c.x + rad * 0.75f, c.y - rad * 0.55f}, col, 1.8f);
				}
				// ── Icônes des actions PAR-MESSAGE (Copier / Insérer dans l'éditeur / Retry),
				// façon GitHub Copilot Chat/Cursor : deux feuilles superposées, flèche vers un
				// bloc (insertion), flèche circulaire (régénérer). ──
				static void DrawCopyIcon(NkGuiDrawList &dl, const NkVec2 &c, float32 rad, const NkColor &col) {
					const float32 w = rad * 1.15f, h = rad * 1.35f;
					dl.AddRect({c.x - w * 0.5f + rad * 0.3f, c.y - h * 0.5f - rad * 0.18f, w, h}, col, 1.1f);
					dl.AddRect({c.x - w * 0.5f - rad * 0.3f, c.y - h * 0.5f + rad * 0.18f, w, h}, col, 1.1f);
				}
				static void DrawInsertIcon(NkGuiDrawList &dl, const NkVec2 &c, float32 rad, const NkColor &col) {
					dl.AddRect({c.x - rad * 0.85f, c.y + rad * 0.15f, rad * 1.7f, rad * 0.6f}, col, 1.1f);
					dl.AddLine({c.x, c.y - rad * 0.9f}, {c.x, c.y + rad * 0.05f}, col, 1.4f);
					dl.AddLine({c.x - rad * 0.4f, c.y - rad * 0.3f}, {c.x, c.y + rad * 0.05f}, col, 1.4f);
					dl.AddLine({c.x + rad * 0.4f, c.y - rad * 0.3f}, {c.x, c.y + rad * 0.05f}, col, 1.4f);
				}
				static void DrawRetryIcon(NkGuiDrawList &dl, const NkVec2 &c, float32 rad, const NkColor &col) {
					const int32 N = 10;
					NkVec2 prev = {};
					for (int32 i = 0; i <= N; ++i) {
						const float32 a = -1.0f + (float32)i / (float32)N * 5.0f; // ~285° d'arc
						const NkVec2 p = {c.x + std::cos(a) * rad, c.y + std::sin(a) * rad};
						if (i > 0)
							dl.AddLine(prev, p, col, 1.5f);
						prev = p;
					}
					const float32 aEnd = -1.0f + 5.0f;
					const NkVec2 tip = {c.x + std::cos(aEnd) * rad, c.y + std::sin(aEnd) * rad};
					const NkVec2 back = {c.x + std::cos(aEnd - 0.55f) * rad * 1.35f, c.y + std::sin(aEnd - 0.55f) * rad * 1.35f};
					dl.AddLine(tip, back, col, 1.5f);
				}
				// ── Petit bouton icône-seule pour la ligne d'actions par-message. Retourne
				// true au clic (le caller consomme mouseClicked lui-même). ──
				static bool ActionIconBtn(NkGuiContext &ctx, NkGuiDrawList &dl, float32 x, float32 cy, float32 sz,
										  NkComboIconFn iconFn, const NkVec2 &mp) {
					const NkRect r = {x, cy - sz * 0.5f, sz, sz};
					const bool hov = NkGuiRectContains(r, mp);
					if (hov)
						dl.AddRectFilled(r, ctx.theme.buttonHover, ctx.S(4.f));
					iconFn(dl, {r.x + sz * 0.5f, cy}, sz * 0.3f, hov ? ctx.theme.text : ctx.theme.textDisabled);
					return hov && ctx.input.mouseClicked[0];
				}

				// ── Menu « liste des chats » (ancré sous le bouton liste du header) : chaque ligne =
				//    titre (clic = active) + croix de suppression (gardée si un seul chat restant). ──
				void DrawChatListMenu(NkGuiContext &ctx, const NkColor &violet) {
					auto &dl = ctx.DL();
					const NkGuiFont *font = ctx.font;
					const NkVec2 mp = ctx.input.mousePos;
					const float32 rowH = ctx.ItemHeight() + ctx.S(4.f);
					const int32 n = static_cast<int32>(mChats.Size());
					float32 w = ctx.S(170.f);
					for (int32 i = 0; i < n; ++i) {
						const float32 tw = (font && font->Valid()) ? font->MeasureWidth(mChats[static_cast<usize>(i)].title.CStr()) : 0.f;
						if (tw + ctx.S(46.f) > w)
							w = tw + ctx.S(46.f);
					}
					NkRect menu = {mChatListAnchor.x + mChatListAnchor.w - w, mChatListAnchor.y + mChatListAnchor.h + ctx.S(4.f),
								   w, n * rowH + ctx.S(8.f)};
					if (menu.x < ctx.S(4.f))
						menu.x = ctx.S(4.f);
					dl.AddRectFilled(menu, ctx.theme.panel, ctx.S(6.f));
					dl.AddRect(menu, ctx.theme.border, 1.f);
					float32 y = menu.y + ctx.S(4.f);
					int32 toDelete = -1;
					for (int32 i = 0; i < n; ++i) {
						const bool sel = (i == mActiveChat);
						const NkRect row = {menu.x + ctx.S(4.f), y, w - ctx.S(8.f), rowH};
						const bool hov = NkGuiRectContains(row, mp);
						if (hov || sel)
							dl.AddRectFilled(row, sel ? violet : ctx.theme.buttonHover, ctx.S(4.f));
						const float32 xw = ctx.S(14.f);
						const NkRect xb = {row.x + row.w - xw - ctx.S(4.f), row.y + (rowH - xw) * 0.5f, xw, xw};
						const bool xhov = mChats.Size() > 1 && NkGuiRectContains(xb, mp);
						if (font && font->Valid())
							dl.AddText(font->Face(), font->TexId(),
									   {row.x + ctx.S(8.f), row.y + (rowH - font->LineHeight()) * 0.5f + font->Ascent()},
									   mChats[static_cast<usize>(i)].title.CStr(),
									   (sel || hov) ? NkColor{255, 255, 255, 255} : ctx.theme.text, row.w - xw - ctx.S(14.f));
						if (mChats.Size() > 1) { // croix de suppression (masquée s'il ne reste qu'1 chat)
							const NkColor xc = xhov ? NkColor{255, 255, 255, 255} : ctx.theme.textDisabled;
							const float32 a = ctx.S(3.5f), xcx = xb.x + xw * 0.5f, xcy = xb.y + xw * 0.5f;
							dl.AddLine({xcx - a, xcy - a}, {xcx + a, xcy + a}, xc, 1.4f);
							dl.AddLine({xcx - a, xcy + a}, {xcx + a, xcy - a}, xc, 1.4f);
						}
						if (xhov && ctx.input.mouseClicked[0]) {
							toDelete = i;
							ctx.input.mouseClicked[0] = false;
						} else if (hov && ctx.input.mouseClicked[0] && !xhov) {
							mActiveChat = i;
							mChatListOpen = false;
							ctx.input.mouseClicked[0] = false;
						}
						y += rowH;
					}
					if (toDelete >= 0)
						DeleteChat(toDelete);
					if (ctx.input.mouseClicked[0] && !NkGuiRectContains(menu, mp) && !NkGuiRectContains(mChatListAnchor, mp))
						mChatListOpen = false;
				}

				// ── Menu du bouton « + » : Upload from computer / Add context / Browse the web.
				//    Ouvre en HAUT ou en BAS de l'ancre selon la place libre dans `bounds`. ──
				void DrawPlusMenu(NkGuiContext &ctx, const NkRect &bounds) {
					auto &dl = ctx.DL();
					const NkGuiFont *font = ctx.font;
					const NkVec2 mp = ctx.input.mousePos;
					struct Row {
							const char *label;
							int32 action; // 0 upload, 1 add-context, 2 browse-web
					};
					const Row rows[3] = {{NkT("ai.plus.upload"), 0}, {NkT("ai.plus.addctx"), 1}, {NkT("ai.plus.web"), 2}};
					const float32 rowH = ctx.ItemHeight() + ctx.S(6.f);
					float32 w = ctx.S(190.f);
					for (int32 i = 0; i < 3; ++i) {
						const float32 tw = (font && font->Valid()) ? font->MeasureWidth(rows[i].label) : 0.f;
						if (tw + ctx.S(40.f) > w)
							w = tw + ctx.S(40.f);
					}
					const float32 menuH = 3 * rowH + ctx.S(8.f);
					const float32 spaceBelow = (bounds.y + bounds.h) - (mPlusAnchor.y + mPlusAnchor.h);
					const float32 spaceAbove = mPlusAnchor.y - bounds.y;
					const bool openUp = spaceBelow < menuH + ctx.S(4.f) && spaceAbove > spaceBelow;
					NkRect menu = {mPlusAnchor.x,
								   openUp ? (mPlusAnchor.y - menuH - ctx.S(4.f)) : (mPlusAnchor.y + mPlusAnchor.h + ctx.S(4.f)),
								   w, menuH};
					if (menu.x < bounds.x + ctx.S(4.f))
						menu.x = bounds.x + ctx.S(4.f);
					dl.AddRectFilled(menu, ctx.theme.panel, ctx.S(6.f));
					dl.AddRect(menu, ctx.theme.border, 1.f);
					float32 y = menu.y + ctx.S(4.f);
					for (int32 i = 0; i < 3; ++i) {
						const NkRect row = {menu.x + ctx.S(4.f), y, w - ctx.S(8.f), rowH};
						const bool hov = NkGuiRectContains(row, mp);
						if (hov)
							dl.AddRectFilled(row, ctx.theme.buttonHover, ctx.S(4.f));
						const float32 icx = row.x + ctx.S(14.f), icy = row.y + rowH * 0.5f, irad = ctx.S(7.f);
						if (rows[i].action == 0) { // upload : petite fleche vers le haut
							dl.AddLine({icx, icy + irad}, {icx, icy - irad}, ctx.theme.textDisabled, 1.5f);
							dl.AddLine({icx - irad * 0.5f, icy - irad * 0.3f}, {icx, icy - irad}, ctx.theme.textDisabled, 1.5f);
							dl.AddLine({icx + irad * 0.5f, icy - irad * 0.3f}, {icx, icy - irad}, ctx.theme.textDisabled, 1.5f);
						} else if (rows[i].action == 1) { // add context : page/fichier
							DrawFileIcon(dl, {icx, icy}, irad, ctx.theme.textDisabled);
						} else { // browse web : globe simplifie
							dl.AddRect({icx - irad, icy - irad, irad * 2.f, irad * 2.f}, ctx.theme.textDisabled, 1.f);
							dl.AddLine({icx, icy - irad}, {icx, icy + irad}, ctx.theme.textDisabled, 1.f);
							dl.AddLine({icx - irad, icy}, {icx + irad, icy}, ctx.theme.textDisabled, 1.f);
						}
						if (font && font->Valid())
							dl.AddText(font->Face(), font->TexId(),
									   {row.x + ctx.S(28.f), row.y + (rowH - font->LineHeight()) * 0.5f + font->Ascent()},
									   rows[i].label, hov ? ctx.theme.text : ctx.theme.textDisabled);
						if (hov && ctx.input.mouseClicked[0]) {
							mPlusOpen = false;
							ctx.input.mouseClicked[0] = false;
							if (rows[i].action == 1)
								mCtxFileOn = true; // Add context : réel (ré-attache le fichier courant)
							else // Upload from computer / Browse the web : pas encore câblé, message honnête
								Msgs().PushBack({2, NkString(NkT("ai.plus.soon"))});
						}
						y += rowH;
					}
					if (ctx.input.mouseClicked[0] && !NkGuiRectContains(menu, mp) && !NkGuiRectContains(mPlusAnchor, mp))
						mPlusOpen = false;
				}

				// ── Palette d'actions filtrable (icone crayon), façon Claude Code : groupes
				// Contexte / Modèle / Personnaliser / Commandes slash / Réglages / Support. ──
				void DrawActionsPalette(NkGuiContext &ctx, const NkRect &bounds, const NkColor &violet) {
					(void)violet;
					auto &dl = ctx.DL();
					const NkGuiFont *font = ctx.font;
					const NkVec2 mp = ctx.input.mousePos;
					struct Row {
							NkString label;
							int32 kind; // 0 action, 1 toggle, 2 valeur (texte a droite)
							const char *value;
							int32 action; // identifiant traite au clic
					};
					struct Group {
							const char *title;
							Row rows[6];
							int32 n;
					};
					int32 nModels = 0;
					const char *const *models = kModels(nModels);
					const char *curModel = models[mModelIdx < nModels ? mModelIdx : 0];
					const NkString effLblStr = NkPrintf("%d/%d", mEffort + 1, (int32)kEffortLevels);
					const char *effLbl = effLblStr.CStr();
					Group groups[6] = {
						{NkT("ai.grp.context"),
						 {{NkString(NkT("ai.act.attach")), 0, nullptr, 0}, {NkString(NkT("ai.act.mention")), 0, nullptr, 1},
						  {NkString(NkT("ai.act.clearconv")), 0, nullptr, 2}, {NkString(NkT("ai.act.rewind")), 0, nullptr, 3}},
						 4},
						{NkT("ai.grp.model"),
						 {{NkString(NkT("ai.act.switchmodel")), 2, curModel, 10},
						  {NkString(NkT("ai.effort")), 2, effLbl, 11},
						  {NkString(NkT("ai.act.thinking")), 1, nullptr, 12},
						  {NkString(NkT("ai.act.autoswitch")), 1, nullptr, 13},
						  {NkString(NkT("ai.act.account")), 0, nullptr, 14}},
						 5},
						{NkT("ai.grp.customize"),
						 {{NkString(NkT("ai.act.mcp")), 0, nullptr, 20}, {NkString(NkT("ai.act.plugins")), 0, nullptr, 21},
						  {NkString(NkT("ai.act.openterm")), 0, nullptr, 22}},
						 3},
						{NkT("ai.grp.slash"), {}, 0}, // rempli dynamiquement (kSlash) plus bas
						{NkT("ai.grp.settings"),
						 {{NkString(NkT("ai.act.switchaccount")), 0, nullptr, 40},
						  {NkString(NkT("ai.act.genconfig")), 0, nullptr, 41},
						  {NkString(NkT("ai.act.remotectl")), 1, nullptr, 42}},
						 3},
						{NkT("ai.grp.support"),
						 {{NkString(NkT("ai.act.helpdocs")), 0, nullptr, 50}, {NkString(NkT("ai.act.report")), 0, nullptr, 51}},
						 2},
					};
					static const char *const kSlash[] = {"/explain", "/fix",	  "/optimize", "/test",	 "/doc",
														 "/refactor", "/review", "/commit",	 "/search", "/new"};

					const float32 w = ctx.S(320.f);
					const float32 filterH = ctx.ItemHeight() + ctx.S(14.f);
					const float32 rowH = ctx.ItemHeight() + ctx.S(2.f);
					const float32 groupTitleH = ctx.S(22.f);
					const bool hasFilter = mActionsFilter[0] != 0;
					auto matches = [&](const char *s) -> bool {
						if (!hasFilter)
							return true;
						NkString a = s, b = mActionsFilter; // recherche insensible a la casse, sous-chaine (maison)
						for (usize k = 0; k < a.Size(); ++k)
							if (a[k] >= 'A' && a[k] <= 'Z')
								a[k] = (char)(a[k] - 'A' + 'a');
						for (usize k = 0; k < b.Size(); ++k)
							if (b[k] >= 'A' && b[k] <= 'Z')
								b[k] = (char)(b[k] - 'A' + 'a');
						return NkFindSub(a.CStr(), b.CStr()) != nullptr;
					};

					// Hauteur totale (mesure avant clip, pour le scroll).
					float32 contentH = 0.f;
					for (int32 g = 0; g < 6; ++g) {
						int32 shown = 0;
						if (g == 3) {
							for (int32 i = 0; i < 10; ++i)
								if (matches(kSlash[i]))
									++shown;
						} else {
							for (int32 i = 0; i < groups[g].n; ++i)
								if (matches(groups[g].rows[i].label.CStr()))
									++shown;
						}
						if (shown > 0)
							contentH += groupTitleH + shown * rowH + ctx.S(6.f);
					}
					const float32 maxListH = ctx.S(340.f);
					const float32 listH = contentH < maxListH ? contentH : maxListH;
					const float32 menuH = filterH + listH + ctx.S(8.f);

					const float32 spaceBelow = (bounds.y + bounds.h) - (mActionsAnchor.y + mActionsAnchor.h);
					const float32 spaceAbove = mActionsAnchor.y - bounds.y;
					const bool openUp = spaceBelow < menuH + ctx.S(4.f) && spaceAbove > spaceBelow;
					NkRect menu = {mActionsAnchor.x,
								   openUp ? (mActionsAnchor.y - menuH - ctx.S(4.f))
										 : (mActionsAnchor.y + mActionsAnchor.h + ctx.S(4.f)),
								   w, menuH};
					if (menu.x + menu.w > bounds.x + bounds.w - ctx.S(4.f))
						menu.x = bounds.x + bounds.w - ctx.S(4.f) - menu.w;
					if (menu.x < bounds.x + ctx.S(4.f))
						menu.x = bounds.x + ctx.S(4.f);
					dl.AddRectFilled(menu, ctx.theme.panel, ctx.S(8.f));
					dl.AddRect(menu, ctx.theme.border, 1.f);

					// Champ de filtre.
					const NkRect filterR = {menu.x + ctx.S(6.f), menu.y + ctx.S(6.f), w - ctx.S(12.f), filterH - ctx.S(10.f)};
					InputTextMultiline(ctx, "##aiActFilter", mActionsFilter, sizeof(mActionsFilter), filterR,
									   NkGuiInputFlags::None, sizeof(mActionsFilter) - 1);
					if (mActionsFilter[0] == 0 && font && font->Valid())
						dl.AddText(font->Face(), font->TexId(),
								   {filterR.x + ctx.S(6.f), filterR.y + ctx.S(4.f) + font->Ascent()}, NkT("ai.act.filter"),
								   ctx.theme.textDisabled);

					const NkRect listR = {menu.x, menu.y + filterH, w, listH};
					dl.PushClipRect(listR, true);
					float32 y = listR.y - mActionsScroll;
					int32 clicked = -1;
					auto drawRow = [&](const NkString &label, int32 kind, const char *value, int32 action) {
						const NkRect row = {listR.x + ctx.S(6.f), y, w - ctx.S(12.f), rowH};
						const bool hov = NkGuiRectContains(row, mp) && NkGuiRectContains(listR, mp);
						if (hov)
							dl.AddRectFilled(row, ctx.theme.buttonHover, ctx.S(4.f));
						if (font && font->Valid())
							dl.AddText(font->Face(), font->TexId(),
									   {row.x + ctx.S(8.f), row.y + (rowH - font->LineHeight()) * 0.5f + font->Ascent()},
									   label.CStr(), ctx.theme.text, row.w - ctx.S(90.f));
						if (kind == 1) { // toggle (switch)
							bool *flag = (action == 12) ? &mThinking : (action == 42) ? &mRemoteControl : &mAutoSwitchFlagged;
							const float32 sw = ctx.S(30.f), sh = ctx.S(15.f);
							const NkRect sr = {row.x + row.w - sw - ctx.S(6.f), row.y + (rowH - sh) * 0.5f, sw, sh};
							dl.AddRectFilled(sr, *flag ? NkColor{72, 145, 232, 255} : ctx.theme.button, sh * 0.5f);
							dl.AddCircleFilled({*flag ? sr.x + sr.w - sh * 0.5f : sr.x + sh * 0.5f, sr.y + sh * 0.5f},
											   sh * 0.5f - ctx.S(1.5f), NkColor{255, 255, 255, 255});
							if (hov && ctx.input.mouseClicked[0]) {
								*flag = !*flag;
								ctx.input.mouseClicked[0] = false;
							}
						} else if (kind == 2 && value && font && font->Valid()) { // valeur a droite
							const float32 vw = font->MeasureWidth(value);
							dl.AddText(font->Face(), font->TexId(),
									   {row.x + row.w - vw - ctx.S(6.f),
										row.y + (rowH - font->LineHeight()) * 0.5f + font->Ascent()},
									   value, ctx.theme.textDisabled);
						}
						if (kind != 1 && hov && ctx.input.mouseClicked[0]) {
							clicked = action;
							ctx.input.mouseClicked[0] = false;
						}
						y += rowH;
					};
					for (int32 g = 0; g < 6; ++g) {
						int32 shown = 0;
						if (g == 3) {
							for (int32 i = 0; i < 10; ++i)
								if (matches(kSlash[i]))
									++shown;
						} else {
							for (int32 i = 0; i < groups[g].n; ++i)
								if (matches(groups[g].rows[i].label.CStr()))
									++shown;
						}
						if (shown == 0)
							continue;
						if (font && font->Valid())
							dl.AddText(font->Face(), font->TexId(), {listR.x + ctx.S(8.f), y + ctx.S(4.f) + font->Ascent()},
									   groups[g].title, ctx.theme.textDisabled);
						y += groupTitleH;
						if (g == 3) {
							for (int32 i = 0; i < 10; ++i)
								if (matches(kSlash[i]))
									drawRow(NkString(kSlash[i]), 0, nullptr, 100 + i);
						} else {
							for (int32 i = 0; i < groups[g].n; ++i)
								if (matches(groups[g].rows[i].label.CStr()))
									drawRow(groups[g].rows[i].label, groups[g].rows[i].kind, groups[g].rows[i].value,
											groups[g].rows[i].action);
						}
						y += ctx.S(6.f);
					}
					dl.PopClipRect();
					if (NkGuiRectContains(listR, mp) && ctx.input.wheel != 0.f) {
						mActionsScroll -= ctx.input.wheel * rowH * 2.f;
						ctx.input.wheel = 0.f;
					}
					const float32 maxSc = contentH > listH ? contentH - listH : 0.f;
					if (mActionsScroll < 0.f)
						mActionsScroll = 0.f;
					if (mActionsScroll > maxSc)
						mActionsScroll = maxSc;
					// Scrollbar verticale (visible + glissable) si le contenu deborde.
					if (maxSc > 0.f) {
						const float32 sbW = ctx.S(5.f);
						const NkRect track = {listR.x + listR.w - sbW - ctx.S(3.f), listR.y, sbW, listR.h};
						float32 thumbH = listH * (listH / contentH);
						if (thumbH < ctx.S(24.f))
							thumbH = ctx.S(24.f);
						if (thumbH > track.h)
							thumbH = track.h;
						const float32 thumbY = track.y + (mActionsScroll / maxSc) * (track.h - thumbH);
						const NkRect thumb = {track.x, thumbY, sbW, thumbH};
						const bool thumbHov = NkGuiRectContains(thumb, mp);
						dl.AddRectFilled(thumb, thumbHov ? ctx.theme.textDisabled : ctx.theme.border, sbW * 0.5f);
						if (thumbHov && ctx.input.mouseClicked[0])
							mActionsScrollDrag = true;
						if (mActionsScrollDrag) {
							if (ctx.input.mouseDown[0]) {
								const float32 t = (mp.y - track.y - thumbH * 0.5f) / (track.h - thumbH > 1.f ? track.h - thumbH : 1.f);
								mActionsScroll = (t < 0.f ? 0.f : (t > 1.f ? 1.f : t)) * maxSc;
							} else
								mActionsScrollDrag = false;
						}
					}

					// Actions REELLES cablees ; les autres affichent un message honnete "a venir".
					if (clicked >= 0) {
						if (clicked == 2) { // Clear conversation
							Msgs().Clear();
							Msgs().PushBack({2, NkString(NkT("ai.hello"))});
						} else if (clicked >= 100 && clicked < 110) { // commande slash -> insere dans la saisie
							const NkString cmd = NkString(kSlash[clicked - 100]) + " ";
							int32 L = 0;
							while (mInput[L])
								++L;
							for (int32 k = 0; cmd.CStr()[k] && L < (int32)sizeof(mInput) - 1; ++k)
								mInput[L++] = cmd.CStr()[k];
							mInput[L] = 0;
						} else if (clicked == 10) { // Switch model -> ouvre le combo modele existant
							mComboOpen = 1;
						} else if (clicked == 11) { // Effort -> cycle (raccourci rapide)
							mEffort = (mEffort + 1) % 3;
						} else if (clicked == 22 && mS) { // Open Claude in Terminal (reel, meme si claude absent)
							mS->termOpenCmd = "claude";
							mS->termOpenKind = -1;
							mS->termOpenAt = mS->HasWorkspace() ? mS->root.ToString() : NkString(".");
							if (mShell)
								mShell->FocusPanel("TERMINAL");
						} else if (clicked == 41 && mShell) { // General config -> Preferences (reel)
							mShell->OpenPreferences();
						} else if (clicked == 50) { // View help docs -> ouvre le wiki (reel)
							NkLauncher::OpenURL("https://github.com/Rihen-Universe/Nkentseu/wiki");
						} else {
							Msgs().PushBack({2, NkString(NkT("ai.plus.soon"))});
						}
						mActionsOpen = false;
					}
					if (ctx.input.mouseClicked[0] && !NkGuiRectContains(menu, mp) && !NkGuiRectContains(mActionsAnchor, mp))
						mActionsOpen = false;
				}

				// ── Barre d'outils du BAS (façon Claude Code / VSCode) : combos Mode/Portée/Édition
				//    + chips de contexte (défilement horizontal à la molette si ça déborde), puis
				//    bouton d'envoi violet à l'extrême droite (fixe, hors défilement). ──
				void DrawBottomToolbar(NkGuiContext &ctx, const NkRect &toolR, NkIcons *ic, const NkColor &violet,
									  const NkColor &violetHov, const NkColor &chip) {
					auto &dl = ctx.DL();
					const NkVec2 mp = ctx.input.mousePos;
					dl.AddRectFilled(toolR, ctx.theme.bgPrimary);
					dl.AddRectFilled({toolR.x, toolR.y, toolR.w, 1.f}, ctx.theme.border);
					const float32 pad = ctx.S(10.f), gap = ctx.S(6.f), btn = ctx.S(30.f);
					const float32 rowH = toolR.h * 0.5f;

					// ── Ligne 1 : fichiers ATTACHÉS au contexte (façon Claude Code : "N lignes
					// sélectionnées"). Rien d'autre ici — clarifié suite à la confusion sur l'ancien
					// chip de branche git, retiré. ──
					const NkRect row1 = {toolR.x, toolR.y, toolR.w, rowH};
					const float32 cy1 = row1.y + rowH * 0.5f;
					DrawChips(ctx, row1.x + pad, cy1, ic, chip);

					// ── Ligne 2 : crayon (palette) + Mode/Portée/Édition (scrollables) + envoi. ──
					const NkRect row2 = {toolR.x, toolR.y + rowH, toolR.w, rowH};
					const float32 cy = row2.y + rowH * 0.5f;
					dl.AddRectFilled({row2.x, row2.y, row2.w, 1.f}, ctx.theme.border);
					// Bouton (+) FIXE tout en bas à gauche (Upload from computer / Add context /
					// Browse the web), puis crayon (palette d'actions) juste après — ni l'un ni
					// l'autre ne défile avec les combos (façon Claude Code).
					const NkRect plusBtn = {row2.x + pad, cy - btn * 0.5f, btn, btn};
					{
						const bool hov = NkGuiRectContains(plusBtn, mp);
						dl.AddRectFilled(plusBtn, (hov || mPlusOpen) ? ctx.theme.buttonHover : chip, ctx.S(6.f));
						const float32 a = ctx.S(5.f), pcx = plusBtn.x + btn * 0.5f;
						dl.AddLine({pcx - a, cy}, {pcx + a, cy}, ctx.theme.textDisabled, 1.5f);
						dl.AddLine({pcx, cy - a}, {pcx, cy + a}, ctx.theme.textDisabled, 1.5f);
						mPlusAnchor = plusBtn;
						if (hov && ctx.input.mouseClicked[0]) {
							mPlusOpen = !mPlusOpen;
							ctx.input.mouseClicked[0] = false;
						}
					}
					const NkRect actionsBtn = {plusBtn.x + btn + gap, cy - btn * 0.5f, btn, btn};
					{
						const bool hov = NkGuiRectContains(actionsBtn, mp);
						dl.AddRectFilled(actionsBtn, (hov || mActionsOpen) ? ctx.theme.buttonHover : chip, ctx.S(6.f));
						const float32 icx = actionsBtn.x + btn * 0.5f, icy = cy, irad = ctx.S(7.f);
						dl.AddLine({icx - irad * 0.6f, icy + irad * 0.6f}, {icx + irad * 0.6f, icy - irad * 0.6f},
								   ctx.theme.textDisabled, 1.6f);
						dl.AddLine({icx + irad * 0.2f, icy - irad * 0.9f}, {icx + irad * 0.9f, icy - irad * 0.2f},
								   ctx.theme.textDisabled, 1.6f);
						mActionsAnchor = actionsBtn;
						if (hov && ctx.input.mouseClicked[0]) {
							mActionsOpen = !mActionsOpen;
							ctx.input.mouseClicked[0] = false;
						}
					}
					const float32 contentX = actionsBtn.x + btn + gap;
					const float32 contentW = row2.x + row2.w - pad - btn - contentX;
					const NkRect contentR = {contentX, row2.y, contentW, row2.h};

					// Molette sur la zone défilable = ajuste mToolScroll (façon barre d'onglets).
					if (NkGuiRectContains(contentR, mp) && ctx.input.wheel != 0.f) {
						mToolScroll -= ctx.input.wheel * ctx.S(40.f);
						ctx.input.wheel = 0.f;
					}
					const float32 maxScroll = mToolContentW > contentW ? (mToolContentW - contentW) : 0.f;
					if (mToolScroll < 0.f)
						mToolScroll = 0.f;
					if (mToolScroll > maxScroll)
						mToolScroll = maxScroll;

					dl.PushClipRect(contentR, true);
					int32 nModes = 0;
					const char *const *modes = ModeOptionsFor(mKind, nModes);
					float32 x = contentR.x - mToolScroll;
					const float32 x0 = x;
					// Mode : icone (eclair) + libelle court, propre a l'agent (mKind).
					x += NkComboButton(ctx, dl, ctx.font, x, cy, 0, [](NkGuiDrawList &d, const NkVec2 &c, float32 r,
																		const NkColor &col) {
							 // petit eclair (zap) vectoriel : coherent quel que soit le theme/l'atlas.
							 d.AddTriangleFilled({c.x + r * 0.15f, c.y - r * 0.9f}, {c.x - r * 0.5f, c.y + r * 0.1f},
												{c.x + r * 0.05f, c.y + r * 0.1f}, col);
							 d.AddTriangleFilled({c.x - r * 0.05f, c.y - r * 0.1f}, {c.x + r * 0.5f, c.y - r * 0.1f},
												{c.x - r * 0.15f, c.y + r * 0.9f}, col);
						 }, modes[mMode < nModes ? mMode : 0], 2, mComboOpen, mModeAnchor, chip, ctx.theme.buttonHover,
						 ctx.theme.text) +
						 gap;
					// Portée : ICONE SEULE (fichier / sélection / workspace), pas de texte.
					{
						NkComboIconFn fn = (mScope == 0) ? &DrawFileIcon : (mScope == 1) ? &DrawSelIcon : &DrawWorkspaceIcon;
						x += NkComboButton(ctx, dl, ctx.font, x, cy, 0, fn, nullptr, 3, mComboOpen, mScopeAnchor, chip,
										  ctx.theme.buttonHover, ctx.theme.text) +
							 gap;
					}
					// Autorisation d'édition : ICONE SEULE — MASQUÉE pour les agents CLI (Claude
					// Code/Codex) dont le Mode gère déjà cette permission (pas de doublon).
					if (ShowEditAuth()) {
						NkComboIconFn fn = (mEditAuth == 0) ? &DrawLockIcon : (mEditAuth == 1) ? &DrawDiffIcon : &DrawCheckIcon;
						x += NkComboButton(ctx, dl, ctx.font, x, cy, 0, fn, nullptr, 4, mComboOpen, mEditAuthAnchor,
										  chip, ctx.theme.buttonHover, ctx.theme.text) +
							 gap;
					}
					mToolContentW = (x - x0); // largeur intrinsèque (sans le décalage de scroll) -> prochaine frame
					dl.PopClipRect();

					// Bouton d'envoi (violet), fixe à l'extrême droite — hors zone de défilement.
					const NkRect send = {row2.x + row2.w - pad - btn, cy - btn * 0.5f, btn, btn};
					const bool canSend = !mBusy && mInput[0] != 0;
					const bool hov = canSend && NkGuiRectContains(send, mp);
					dl.AddRectFilled(send, canSend ? (hov ? violetHov : violet) : ctx.theme.button, ctx.S(8.f));
					const NkColor ac = canSend ? NkColor{255, 255, 255, 255} : ctx.theme.textDisabled;
					if (ic && ic->up)
						dl.AddImage(ic->up, {send.x + ctx.S(7.f), send.y + ctx.S(7.f), btn - ctx.S(14.f), btn - ctx.S(14.f)},
									{0.f, 0.f}, {1.f, 1.f}, ac);
					else {
						const float32 cx = send.x + btn * 0.5f, cyy = send.y + btn * 0.5f, a = ctx.S(5.f);
						dl.AddLine({cx, cyy + a}, {cx, cyy - a}, ac, 2.f);
						dl.AddLine({cx - a * 0.6f, cyy - a * 0.3f}, {cx, cyy - a}, ac, 2.f);
						dl.AddLine({cx + a * 0.6f, cyy - a * 0.3f}, {cx, cyy - a}, ac, 2.f);
					}
					if (canSend && hov && ctx.input.mouseClicked[0])
						Send();
				}

				// ── Popover réglages (engrenage) : max tokens + instructions système (multi-ligne). ──
				// ── Icône "effort" (façon égaliseur : 3 barres de hauteur croissante). ──
				static void DrawGaugeIcon(NkGuiDrawList &dl, const NkVec2 &c, float32 rad, const NkColor &col) {
					for (int32 i = 0; i < 3; ++i) {
						const float32 h = rad * (0.5f + i * 0.4f);
						const float32 x = c.x - rad * 0.7f + i * rad * 0.7f;
						dl.AddLine({x, c.y + rad * 0.9f}, {x, c.y + rad * 0.9f - h}, col, 1.8f);
					}
				}

				// ── Ligne « Effort (Valeur) » + toggle 3 arrêts (façon Claude Code), réutilisée
				// dans le popover réglages ET le menu Mode. `rect` = zone réservée. ──
				// 6 niveaux d'effort (0..5). Libellé numérique "Effort (n/6)" : évite d'inventer
				// 6 noms différents par langue pour une notion qui reste avant tout un curseur.
				// enum (pas `static const`) : garantit une CONSTANTE pure, jamais "ODR-used" ->
				// aucune definition hors-classe requise (le static const avait fait echouer
				// l'edition de liens en debug : reference indefinie a AiPanel::kEffortLevels).
				enum { kEffortLevels = 6 };
				void DrawEffortToggle(NkGuiContext &ctx, NkGuiDrawList &dl, const NkRect &rect, int32 &effort) {
					const NkGuiFont *font = ctx.font;
					const NkVec2 mp = ctx.input.mousePos;
					const float32 cy = rect.y + rect.h * 0.5f;
					if (effort < 0)
						effort = 0;
					if (effort >= kEffortLevels)
						effort = kEffortLevels - 1;
					const NkString label = NkPrintf("%s (%d/%d)", NkT("ai.effort"), effort + 1, (int32)kEffortLevels);
					const float32 irad = ctx.S(7.f);
					DrawGaugeIcon(dl, {rect.x + irad, cy}, irad, ctx.theme.textDisabled);
					if (font && font->Valid())
						dl.AddText(font->Face(), font->TexId(),
								   {rect.x + irad * 2.f + ctx.S(6.f), cy - font->LineHeight() * 0.5f + font->Ascent()},
								   label.CStr(), ctx.theme.text);
					const float32 trkW = ctx.S(78.f), trkH = ctx.S(15.f);
					const NkRect trk = {rect.x + rect.w - trkW, cy - trkH * 0.5f, trkW, trkH};
					const NkColor kBlue = {72, 145, 232, 255};
					dl.AddRectFilled(trk, ctx.theme.button, trkH * 0.5f);
					auto stopX = [&](int32 i) {
						const float32 frac = (float32)i / (float32)(kEffortLevels - 1);
						return trk.x + trkH * 0.5f + frac * (trkW - trkH);
					};
					const float32 knobX = stopX(effort);
					dl.AddRectFilled({trk.x, trk.y, (knobX - trk.x) + trkH * 0.5f, trkH}, kBlue, trkH * 0.5f);
					for (int32 i = 0; i < kEffortLevels; ++i) { // points des AUTRES arrêts
						if (i == effort)
							continue;
						dl.AddCircleFilled({stopX(i), cy}, ctx.S(2.f), ctx.theme.textDisabled);
					}
					dl.AddCircleFilled({knobX, cy}, trkH * 0.5f - ctx.S(1.5f), NkColor{255, 255, 255, 255});
					if (NkGuiRectContains(trk, mp) && ctx.input.mouseClicked[0]) {
						const float32 t = (mp.x - trk.x) / (trkW > 1.f ? trkW : 1.f);
						int32 lvl = (int32)(t * kEffortLevels);
						effort = lvl < 0 ? 0 : (lvl >= kEffortLevels ? kEffortLevels - 1 : lvl);
					}
				}

				// ── Menu MODE dédié (pas le combo générique) : chaque option affiche un TITRE +
				// une DESCRIPTION (façon Claude Code), coche l'option active, et se termine par
				// le contrôle Effort. Ouvre en haut/bas selon la place, comme les autres menus. ──
				void DrawModeMenu(NkGuiContext &ctx, const NkRect &bounds, const NkColor &violet) {
					auto &dl = ctx.DL();
					const NkGuiFont *font = ctx.font;
					const NkVec2 mp = ctx.input.mousePos;
					int32 n = 0;
					const char *const *opts = ModeOptionsFor(mKind, n);
					int32 nd = 0;
					const char *const *descs = ModeDescFor(mKind, nd);
					const float32 w = ctx.S(270.f);
					const float32 titleH = ctx.ItemHeight();
					const float32 descLineH = (font && font->Valid()) ? font->LineHeight() * 0.88f : 12.f;
					NkVector<NkVector<NkString>> wrapped;
					float32 rowsH = 0.f;
					for (int32 i = 0; i < n; ++i) {
						NkVector<NkString> ls;
						WrapLines(ctx, descs[i], w - ctx.S(44.f), ls);
						rowsH += titleH + ls.Size() * descLineH + ctx.S(10.f);
						wrapped.PushBack(ls);
					}
					const float32 effortH = ctx.ItemHeight() + ctx.S(10.f);
					const float32 menuH = rowsH + ctx.S(8.f) + 1.f + effortH + ctx.S(8.f);

					const float32 spaceBelow = (bounds.y + bounds.h) - (mModeAnchor.y + mModeAnchor.h);
					const float32 spaceAbove = mModeAnchor.y - bounds.y;
					const bool openUp = spaceBelow < menuH + ctx.S(4.f) && spaceAbove > spaceBelow;
					NkRect menu = {mModeAnchor.x,
								   openUp ? (mModeAnchor.y - menuH - ctx.S(4.f)) : (mModeAnchor.y + mModeAnchor.h + ctx.S(4.f)),
								   w, menuH};
					if (menu.x + menu.w > bounds.x + bounds.w - ctx.S(4.f))
						menu.x = bounds.x + bounds.w - ctx.S(4.f) - menu.w;
					if (menu.x < bounds.x + ctx.S(4.f))
						menu.x = bounds.x + ctx.S(4.f);
					dl.AddRectFilled(menu, ctx.theme.panel, ctx.S(8.f));
					dl.AddRect(menu, ctx.theme.border, 1.f);

					float32 y = menu.y + ctx.S(4.f);
					for (int32 i = 0; i < n; ++i) {
						const NkVector<NkString> &ls = wrapped[i];
						const float32 rh = titleH + ls.Size() * descLineH + ctx.S(10.f);
						const NkRect row = {menu.x + ctx.S(4.f), y, w - ctx.S(8.f), rh};
						const bool hov = NkGuiRectContains(row, mp);
						const bool sel = (i == mMode);
						if (hov || sel)
							dl.AddRectFilled(row, sel ? NkColor{violet.r, violet.g, violet.b, 55} : ctx.theme.buttonHover,
											 ctx.S(4.f));
						if (font && font->Valid()) {
							dl.AddText(font->Face(), font->TexId(), {row.x + ctx.S(8.f), y + ctx.S(4.f) + font->Ascent()},
									   opts[i], ctx.theme.text);
							if (sel)
								DrawCheckIcon(dl, {row.x + row.w - ctx.S(14.f), y + ctx.S(4.f) + font->LineHeight() * 0.5f},
											 ctx.S(6.f), violet);
							float32 dy = y + titleH;
							for (usize k = 0; k < ls.Size(); ++k)
								dl.AddText(font->Face(), font->TexId(),
										   {row.x + ctx.S(8.f), dy + k * descLineH + font->Ascent() * 0.88f}, ls[k].CStr(),
										   ctx.theme.textDisabled);
						}
						if (hov && ctx.input.mouseClicked[0]) {
							mMode = i;
							mComboOpen = 0;
							ctx.input.mouseClicked[0] = false;
						}
						y += rh;
					}
					dl.AddRectFilled({menu.x + ctx.S(8.f), y + ctx.S(2.f), w - ctx.S(16.f), 1.f}, ctx.theme.border);
					y += ctx.S(6.f);
					DrawEffortToggle(ctx, dl, {menu.x + ctx.S(12.f), y, w - ctx.S(24.f), ctx.ItemHeight()}, mEffort);

					if (ctx.input.mouseClicked[0] && !NkGuiRectContains(menu, mp) && !NkGuiRectContains(mModeAnchor, mp))
						mComboOpen = 0;
				}

				void DrawPropsPopover(NkGuiContext &ctx, const NkColor &violet) {
					(void)violet;
					auto &dl = ctx.DL();
					const NkGuiFont *font = ctx.font;
					const NkVec2 mp = ctx.input.mousePos;
					const float32 w = ctx.S(268.f), it = ctx.ItemHeight();
					const float32 sysH = ctx.S(92.f);
					const float32 mh = ctx.S(10.f) + it + ctx.S(8.f) + it + ctx.S(10.f) + it + ctx.S(10.f) + it +
									   ctx.S(10.f) + it + ctx.S(4.f) + sysH + ctx.S(12.f); // + ligne Effort
					NkRect menu = {mGearRect.x + mGearRect.w - w, mGearRect.y + mGearRect.h + ctx.S(4.f), w, mh};
					if (menu.x < ctx.S(4.f))
						menu.x = ctx.S(4.f);
					dl.AddRectFilled(menu, ctx.theme.panel, ctx.S(8.f));
					dl.AddRect(menu, ctx.theme.border, 1.f);
					auto txt = [&](float32 x, float32 yy, const char *s, const NkColor &c) {
						if (font && font->Valid())
							dl.AddText(font->Face(), font->TexId(), {x, yy + font->Ascent()}, s, c);
					};
					float32 y = menu.y + ctx.S(8.f);
					txt(menu.x + ctx.S(12.f), y, NkT("ai.props"), ctx.theme.text);
					y += it + ctx.S(8.f);
					// Température (slider) — toujours accessible ici même si masquée dans la barre.
					{
						txt(menu.x + ctx.S(12.f), y + (it - (font ? font->LineHeight() : 16.f)) * 0.5f, "Température",
							ctx.theme.textDisabled);
						const float32 trkW = ctx.S(96.f);
						const float32 trkX = menu.x + w - ctx.S(12.f) - trkW - ctx.S(30.f);
						const NkRect trk = {trkX, y + it * 0.5f - ctx.S(2.f), trkW, ctx.S(4.f)};
						dl.AddRectFilled(trk, ctx.theme.button, ctx.S(2.f));
						dl.AddRectFilled({trk.x, trk.y, trkW * mTemp, ctx.S(4.f)}, NkColor{236, 168, 56, 255}, ctx.S(2.f));
						dl.AddCircleFilled({trk.x + trkW * mTemp, y + it * 0.5f}, ctx.S(6.f), NkColor{236, 168, 56, 255});
						txt(trkX + trkW + ctx.S(8.f), y + (it - (font ? font->LineHeight() : 16.f)) * 0.5f,
							NkPrintf("%.1f", mTemp).CStr(), ctx.theme.text);
						const NkRect hit = {trkX - ctx.S(4.f), y, trkW + ctx.S(8.f), it};
						if (NkGuiRectContains(hit, mp) && ctx.input.mouseClicked[0])
							mDragTemp = true;
						if (mDragTemp && ctx.input.mouseDown[0]) {
							float32 t = (mp.x - trkX) / (trkW > 1.f ? trkW : 1.f);
							mTemp = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
						}
						y += it + ctx.S(10.f);
					}
					// Effort (raisonnement) — icône + « Effort (Valeur) » + toggle 3 arrêts.
					{
						DrawEffortToggle(ctx, dl, {menu.x + ctx.S(12.f), y, w - ctx.S(24.f), it}, mEffort);
						y += it + ctx.S(10.f);
					}
					// Max tokens : − valeur +
					{
						txt(menu.x + ctx.S(12.f), y + (it - (font ? font->LineHeight() : 16.f)) * 0.5f, NkT("ai.maxtokens"),
							ctx.theme.textDisabled);
						const float32 bw = ctx.S(24.f);
						const NkRect minus = {menu.x + w - ctx.S(12.f) - bw * 2.f - ctx.S(52.f), y, bw, it};
						const NkRect plus = {menu.x + w - ctx.S(12.f) - bw, y, bw, it};
						dl.AddRectFilled(minus, ctx.theme.button, ctx.S(4.f));
						dl.AddRectFilled(plus, ctx.theme.button, ctx.S(4.f));
						const float32 a = ctx.S(5.f);
						dl.AddLine({minus.x + bw * 0.5f - a, minus.y + it * 0.5f}, {minus.x + bw * 0.5f + a, minus.y + it * 0.5f},
								   ctx.theme.text, 1.6f);
						dl.AddLine({plus.x + bw * 0.5f - a, plus.y + it * 0.5f}, {plus.x + bw * 0.5f + a, plus.y + it * 0.5f},
								   ctx.theme.text, 1.6f);
						dl.AddLine({plus.x + bw * 0.5f, plus.y + it * 0.5f - a}, {plus.x + bw * 0.5f, plus.y + it * 0.5f + a},
								   ctx.theme.text, 1.6f);
						const NkString v = NkPrintf("%d", mMaxTokens);
						if (font && font->Valid())
							dl.AddText(font->Face(), font->TexId(),
									   {minus.x + bw + ctx.S(10.f), y + (it - font->LineHeight()) * 0.5f + font->Ascent()},
									   v.CStr(), ctx.theme.text);
						if (NkGuiRectContains(minus, mp) && ctx.input.mouseClicked[0])
							mMaxTokens = mMaxTokens > 256 ? mMaxTokens - 256 : 256;
						if (NkGuiRectContains(plus, mp) && ctx.input.mouseClicked[0])
							mMaxTokens = mMaxTokens < 8192 ? mMaxTokens + 256 : 8192;
						y += it + ctx.S(10.f);
					}
					// Instructions système (multi-ligne, scrollable).
					txt(menu.x + ctx.S(12.f), y, NkT("ai.system"), ctx.theme.textDisabled);
					y += it + ctx.S(4.f);
					const NkRect sys = {menu.x + ctx.S(12.f), y, w - ctx.S(24.f), sysH};
					InputTextMultiline(ctx, "##aiSys", mSystem, sizeof(mSystem), sys, NkGuiInputFlags::None,
									   sizeof(mSystem) - 1, /*wrap=*/true);
					if (ctx.input.mouseClicked[0] && !NkGuiRectContains(menu, mp) && !NkGuiRectContains(mGearRect, mp))
						mPropsOpen = false;
				}

				// ── JSON : échappe une chaîne pour un littéral JSON. ──
				static NkString JsonEsc(const char *s) {
					NkString o;
					for (const char *p = s; *p; ++p) {
						const unsigned char c = (unsigned char)*p;
						switch (c) {
							case '\"':
								o += "\\\"";
								break;
							case '\\':
								o += "\\\\";
								break;
							case '\n':
								o += "\\n";
								break;
							case '\r':
								o += "\\r";
								break;
							case '\t':
								o += "\\t";
								break;
							default:
								if (c < 0x20) {
									o += NkPrintf("\\u%04x", c); // NkPrintf maison
								} else {
									char b[2] = {(char)c, 0};
									o += b;
								}
								break;
						}
					}
					return o;
				}

				// ── JSON : extrait la valeur string de la 1re occurrence de "key":"..." après `from`. ──
				static NkString JsonStr(const char *json, const char *key) {
					NkString pat = NkString("\"") + key + "\"";
					const char *k = NkFindSub(json, pat.CStr()); // recherche maison (NkText.h)
					if (!k)
						return NkString();
					const char *p = k + pat.Size();
					while (*p && *p != ':')
						++p;
					if (*p)
						++p;
					while (*p == ' ' || *p == '\t' || *p == '\n')
						++p;
					if (*p != '\"')
						return NkString();
					++p;
					NkString o;
					while (*p && *p != '\"') {
						if (*p == '\\' && p[1]) {
							++p;
							char c = *p;
							if (c == 'n')
								o += "\n";
							else if (c == 't')
								o += "\t";
							else if (c == 'r') {
							} else if (c == 'u' && p[1] && p[2] && p[3] && p[4]) {
								// Conversion hex manuelle maison (ex-std::strtol base 16).
								long cp = 0;
								for (int32 hi = 1; hi <= 4; ++hi) {
									const char hc = p[hi];
									const int32 d = (hc >= '0' && hc <= '9')   ? hc - '0'
													: (hc >= 'a' && hc <= 'f') ? hc - 'a' + 10
													: (hc >= 'A' && hc <= 'F') ? hc - 'A' + 10
																			   : 0;
									cp = cp * 16 + d;
								}
								char u8[5];
								int32 n = NkEncodeU8((uint32)cp, u8);
								u8[n] = 0;
								o += u8;
								p += 4;
							} else {
								char b[2] = {c, 0};
								o += b;
							}
							++p;
						} else {
							char b[2] = {*p, 0};
							o += b;
							++p;
						}
					}
					return o;
				}

				NkString ReqPath() const {
					return (mS->root / ".nkcode" / "ai_req.json").ToString();
				}

				NkString BuildJson() const {
					const bool ollama = (mProvider == 1);
					NkString j = "{";
					if (ollama) {
						j += "\"model\":\"llama3.2\",\"stream\":false,";
					} else {
						int32 n = 0;
						const char *const *models = kModels(n);
						const char *mdl = (mModelIdx < n) ? models[mModelIdx] : "claude-sonnet-5";
						j += NkPrintf("\"model\":\"%s\",\"max_tokens\":%d,\"temperature\":%.2f,", mdl, mMaxTokens,
									  (double)mTemp)
								 .CStr();
						if (mSystem[0])
							j += NkString("\"system\":\"") + JsonEsc(mSystem).CStr() + "\",";
					}
					j += "\"messages\":[";
					bool first = true;
					const NkVector<Msg> &msgs = Msgs();
					for (usize i = 0; i < msgs.Size(); ++i) {
						if (msgs[i].role != 0 && msgs[i].role != 1)
							continue; // system/erreur exclus
						if (!first)
							j += ",";
						first = false;
						j += "{\"role\":\"";
						j += (msgs[i].role == 0 ? "user" : "assistant");
						j += "\",\"content\":\"";
						j += JsonEsc(msgs[i].text.CStr());
						j += "\"}";
					}
					j += "]}";
					return j;
				}

				// ── Retry (bouton par-message) : régénère la DERNIÈRE réponse assistant en
				// rejouant exactement le même tour — retire la réponse ET le message
				// utilisateur qui l'a produite, puis les repousse via Send() (pas de second
				// chemin d'envoi à maintenir : Send() gère déjà la conversion + la requête). ──
				void RetryLast() {
					if (mBusy)
						return;
					NkVector<Msg> &msgs = Msgs();
					if (msgs.Empty() || msgs[msgs.Size() - 1].role != 1)
						return;
					const usize last = msgs.Size() - 1;
					if (last == 0 || msgs[last - 1].role != 0)
						return; // pas de message utilisateur juste avant : rien a rejouer
					const NkString userText = msgs[last - 1].text;
					msgs.RemoveAt(last);
					msgs.RemoveAt(last - 1);
					const char *s = userText.CStr();
					usize n = 0;
					while (s[n] && n < sizeof(mInput) - 1) {
						mInput[n] = s[n];
						++n;
					}
					mInput[n] = 0;
					Send();
				}

				void Send() {
					if (mBusy || mInput[0] == 0)
						return;
					mProvider = ProviderForModel(); // dérivé de l'agent + du modèle choisi
					if (mProvider == 5) {			// Codex / OpenAI : intégration à venir
						Msgs().PushBack({0, NkString(mInput)});
						mInput[0] = 0;
						Msgs().PushBack({2, NkString(NkT("ai.codexsoon"))});
						mChats[static_cast<usize>(mActiveChat)].stick = true;
						return;
					}
					if (mProvider == 2) { // IA maison (NkAI)
						Msgs().PushBack({0, NkString(mInput)});
						mInput[0] = 0;
						Msgs().PushBack({2, NkString(NkT("ai.homesoon"))});
						mChats[static_cast<usize>(mActiveChat)].stick = true;
						return;
					}
					Msgs().PushBack({0, NkString(mInput)});
					mInput[0] = 0;
					mChats[static_cast<usize>(mActiveChat)].stick = true;
					mBusyChat = mActiveChat; // le switch de chat pendant l'attente n'egare pas la reponse

					// Corps de requête -> fichier temporaire (évite l'enfer des quotes en ligne de commande).
					const NkString path = ReqPath();
					const NkString body = BuildJson();
					if (!NkFile::WriteAllText(path.CStr(), body.CStr())) { // écriture maison (NkFile)
						Msgs().PushBack({2, NkString(NkT("ai.errtmp"))});
						return;
					}

					NkString cmd;
					if (mProvider == 1) {
						cmd = NkString("curl -s -X POST http://localhost:11434/api/chat -H \"content-type: "
									   "application/json\" -d @\"") +
							  path.CStr() + "\"";
					} else {
						const char *key = env::GetEnvVar("NKCODE_ANTHROPIC_KEY"); // API maison (NkEnv.h)
						if (!key || !*key)
							key = env::GetEnvVar("ANTHROPIC_API_KEY");
						if (!key || !*key) {
							Msgs().PushBack({2, NkString(NkT("ai.errkey"))});
							return;
						}
						cmd = NkString("curl -s -X POST https://api.anthropic.com/v1/messages -H \"x-api-key: ") + key +
							  "\" -H \"anthropic-version: 2023-06-01\" -H \"content-type: application/json\" -d @\"" +
							  path.CStr() + "\"";
					}
					mRespLines.Clear();
					if (!mProc.Start(cmd)) {
						Msgs().PushBack({2, NkString(NkT("ai.errbusy"))});
						return;
					}
					mBusy = true;
				}

				void Poll() {
					if (!mBusy)
						return;
					mProc.Drain(mRespLines); // accumule (Drain n'efface pas `out`)
					if (!mProc.Done())
						return;
					mBusy = false;
					// La réponse rejoint la conversation CIBLE (mBusyChat), même si l'utilisateur
					// a switché de chat entre-temps.
					const int32 idx = (mBusyChat >= 0 && mBusyChat < static_cast<int32>(mChats.Size())) ? mBusyChat
																										 : mActiveChat;
					NkVector<Msg> &msgs = MsgsOf(idx);
					auto stick = [&]() -> bool & { return mChats[static_cast<usize>(idx)].stick; };
					NkString raw;
					for (usize i = 0; i < mRespLines.Size(); ++i)
						raw += mRespLines[i].CStr();
					mRespLines.Clear();
					if (raw.Empty()) {
						msgs.PushBack({2, NkString(NkT("ai.errnet"))});
						stick() = true;
						return;
					}
					const char *j = raw.CStr();
					// Erreur API ?
					if (NkFindSub(j, "\"error\"")) { // recherche maison (NkText.h)
						NkString em = JsonStr(j, "message");
						msgs.PushBack({2, (NkString(NkT("ai.errapi")) + " " + (em.Empty() ? raw.CStr() : em.CStr()))});
						stick() = true;
						return;
					}
					// Réponse : Claude -> "text" ; Ollama -> "content".
					NkString ans = (mProvider == 1) ? JsonStr(j, "content") : JsonStr(j, "text");
					if (ans.Empty())
						ans = JsonStr(j, "content");
					if (ans.Empty())
						ans = raw; // dernier recours : brut
					msgs.PushBack({1, ans});
					stick() = true;
				}
		};

		// Alias PAR-CHAT : nettoyage — ne doivent PAS fuiter au-dela de cette classe/ce fichier.
#undef mInput
#undef mModelIdx
#undef mMode
#undef mScope
#undef mEditAuth
#undef mCtxFileOn
#undef mTemp
#undef mMaxTokens
#undef mSystem
#undef mEffort
#undef mThinking
#undef mAutoSwitchFlagged

	} // namespace nkcode
} // namespace nkentseu
