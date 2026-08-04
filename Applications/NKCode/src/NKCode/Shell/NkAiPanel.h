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
#include "NKEditorKit/NkEditorScrollbar.h" // NkVScrollbar (scrollbar STANDARD reutilisable, cf. regle CLAUDE.md UI/UX)
#include "NKCode/Project/NkCodeState.h"
#include "NKCode/Project/NkProcess.h"
#include "NKCode/Project/NkLsp.h" // NkPipeProc (process a pipes CreateProcessW, reutilise pour le CLI `claude`)
#include "NKCode/Editor/NkTextDraw.h" // NkEncodeU8 (décodage \uXXXX -> UTF-8)
#include "NKCode/Shell/NkI18n.h"
#include "NKCode/Shell/NkUi.h" // NkIcons (icones de la vue IDE)
#include "NKContainers/String/NkFormat.h" // NkPrintf (formatage maison)
#include "NKFileSystem/NkFile.h"		  // NkFile::WriteAllText (fichiers maison)
#include "NKFileSystem/NkDirectory.h"	  // NkDirectory::Exists (drag-drop fichiers -> contexte)
#include "NKEditorKit/NkEditorTooltip.h"  // NkTooltip : infobulle avec delai (boutons icone-seule)
#include "NKPlatform/NkEnv.h"			  // env::GetEnvVar (variables d'environnement maison)
#include <cmath> // std::cos/std::sin (spinner du statut "en cours")
#include "NKWindow/Core/NkLauncher.h" // OpenURL (bouton "View help docs" de la palette d'actions)
#include <cstdio> // _popen/fgets UNIQUEMENT (pipe process, cf. wrapper désigné NkProcess.h)
#include <ctime>  // std::time (temps relatif de reinitialisation, popover Utilisation & limites)

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu::editorkit;
		using namespace nkentseu::nkgui;

		// ── Echappe un argument pour une ligne de commande Win32 (CreateProcessW),
		// SANS shell (NkPipeProc::Start ne passe PAS par cmd.exe — lpApplicationName
		// est nul, wcmd est decoupe par l'ARGV du process ENFANT, pas interprete comme
		// du shell). Algorithme standard Microsoft ("Parsing C++ Command-Line
		// Arguments") : necessaire des qu'un argument contient espace/tabulation/
		// guillemet — typiquement le PROMPT de l'utilisateur, texte libre non fiable.
		// Ne PAS remplacer par une concatenation/echappement "shell" (cmd.exe) : il
		// n'y en a pas ici, et ce serait de toute facon un terrain mine (injection). ──
		inline NkString NkWin32QuoteArg(const char *s) {
			bool needsQuotes = (*s == '\0');
			for (const char *p = s; *p && !needsQuotes; ++p)
				if (*p == ' ' || *p == '\t' || *p == '"')
					needsQuotes = true;
			if (!needsQuotes)
				return NkString(s);
			NkString out = "\"";
			usize len = 0;
			while (s[len])
				++len;
			usize i = 0;
			while (i < len) {
				usize nBs = 0;
				while (i < len && s[i] == '\\') {
					++nBs;
					++i;
				}
				if (i == len) { // backslashes en fin de chaine : doubles (avant le guillemet fermant)
					for (usize k = 0; k < nBs * 2; ++k)
						out += '\\';
					break;
				} else if (s[i] == '"') { // 2N+1 backslashes puis le guillemet echappe
					for (usize k = 0; k < nBs * 2 + 1; ++k)
						out += '\\';
					out += '"';
					++i;
				} else { // backslashes litteraux (pas suivis d'un guillemet) : inchanges
					for (usize k = 0; k < nBs; ++k)
						out += '\\';
					out += s[i];
					++i;
				}
			}
			out += '"';
			return out;
		}

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
					// ── PERSISTANCE PAR WORKSPACE : les chats appartiennent au workspace
					// qui les a generes. Au changement de racine (ou 1er passage), on
					// recharge les conversations sauvegardees de CE workspace. Jamais
					// pendant un tour en cours (le flux ecrirait dans le mauvais chat) —
					// le rechargement est simplement differe a la fin du tour. ──
					{
						const NkString root = (mS && mS->HasWorkspace()) ? mS->root.ToString() : NkString();
						if (root != mLoadedRoot && !mBusy) {
							mLoadedRoot = root;
							LoadChats();
						}
					}
					if (mChats.Empty())
						NewChat(); // 1re conversation (locale correcte : posée après construction)
					// Compte & Usage : lance les requetes DES l'entree dans le panneau (pas
					// besoin d'ouvrir le popover ni d'avoir ecrit un prompt) — le calcul est
					// deja pret des que l'utilisateur regarde "Compte et utilisation".
					if (!mAutoUsageFetched) {
						mAutoUsageFetched = true;
						EnsureUsageDataFetching();
					}
					// Sauvegarde debouncee (~1,5 s) sur changement d'empreinte, hors flux.
					if (++mSaveTick >= 90) {
						mSaveTick = 0;
						if (!mBusy)
							SaveChatsIfChanged();
					}
					// ── Pont barre de menus -> chat (menu IA : Expliquer/Corriger/... la
					// selection) : prompt depose dans NkCodeState, copie dans la saisie ici ;
					// aiSend = envoi immediat (si un tour est deja en cours, le prompt reste
					// dans la saisie — l'utilisateur enverra, jamais de perte silencieuse).
					if (mS && !mS->aiPrompt.Empty()) {
						const usize cap = sizeof(mInput) - 1;
						const usize n = mS->aiPrompt.Size() < cap ? mS->aiPrompt.Size() : cap;
						::memcpy(mInput, mS->aiPrompt.CStr(), n);
						mInput[n] = 0;
						const bool wantSend = mS->aiSend;
						mS->aiPrompt.Clear();
						mS->aiSend = false;
						if (wantSend && !mBusy)
							Send();
					}
					Poll(); // draine la réponse en cours

					// ── CIBLE de DRAG & DROP (explorateur interne + OS) : ajoute le(s) chemin(s)
					// au brouillon de saisie (façon @mention), l'utilisateur complète son message
					// autour. Meme mecanisme que EditorPanel/TerminalPanel (mS->dragActive/
					// osDropPaths), absent ici jusqu'a present -> aucun effet au depot de fichier. ──
					if (mS) {
						auto appendPath = [&](const NkString &p) {
							int32 len = 0;
							while (mInput[len])
								++len;
							NkString tok = NkString("@\"") + p + "\" ";
							for (usize k = 0; k < tok.Size() && len < 8191; ++k)
								mInput[len++] = tok[k];
							mInput[len] = 0;
						};
						if (mS->dragActive && ctx.input.mouseReleased[0] && NkGuiRectContains(r, ctx.input.mousePos)) {
							for (usize di = 0; di < mS->dragPaths.Size(); ++di)
								if (!NkDirectory::Exists(mS->dragPaths[di].CStr()))
									appendPath(mS->dragPaths[di]);
						}
						if (!mS->osDropPaths.Empty() &&
							NkGuiRectContains(
								r, {static_cast<float32>(mS->osDropX), static_cast<float32>(mS->osDropY)})) {
							for (usize di = 0; di < mS->osDropPaths.Size(); ++di)
								if (!NkDirectory::Exists(mS->osDropPaths[di].CStr()))
									appendPath(mS->osDropPaths[di]);
							mS->osDropPaths.Clear();
							mS->osDropTtl = 0;
						}
					}

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
					// Banniere d'utilisation (Claude Code, façon VSCode : "Utilisation à 56% de
					// Hebdomadaire (7 jours) · reinitialisation dans 5j [Voir l'utilisation] [x]")
					// — juste au-dessus de la saisie, visible SEULEMENT au-dela d'un seuil et tant
					// que l'utilisateur ne l'a pas ecartee pour ce palier de 10% (reapparait si
					// l'usage grimpe encore). Donnee REELLE (mRlBuckets), aucune simulation.
					const RlBucket *topBucket = nullptr;
					if (mKind == 1 && chatView)
						for (usize i = 0; i < mRlBuckets.Size(); ++i)
							if (!topBucket || mRlBuckets[i].utilization > topBucket->utilization)
								topBucket = &mRlBuckets[i];
					NkString bannerDismissKey;
					bool showBanner = false;
					if (topBucket && topBucket->utilization >= 0.5f) {
						bannerDismissKey = NkPrintf("%s:%d", topBucket->type.CStr(), (int32)(topBucket->utilization * 10.f));
						showBanner = (mUsageBannerDismissKey != bannerDismissKey);
					}
					const float32 bannerH = showBanner ? (ctx.ItemHeight() + ctx.S(10.f)) : 0.f;
					const NkRect hdr = {r.x, r.y, r.w, hdrH};
					const float32 bodyY = r.y + hdrH + tabsH;
					const float32 bodyH = r.h - hdrH - tabsH - inH - toolH - bannerH;
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
					{ // pilule modèle (combo, aligné à droite) : « claude-3.5-sonnet ▾ » (ou le
					  // titre humain "Sonnet"/"Opus"/... pour Claude Code, cf. ClaudeModelTitles)
						int32 nModels = 0;
						const char *const *models = mKind == 1 ? ClaudeModelTitles(nModels) : kModels(nModels);
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

					// Un popover/menu ouvert (dessiné APRÈS, donc PAR-DESSUS) doit rendre le contenu
					// EN DESSOUS non-interactif — sinon la molette/les clics « traversent » vers le
					// chat caché dessous (bug remonté par Rihen : la molette au-dessus de la
					// palette d'actions scrollait le chat au lieu de la liste filtrée).
					const bool overlayOpen =
						mComboOpen != 0 || mPropsOpen || mChatListOpen || mPlusOpen || mActionsOpen || mUsageOpen;

					// ══ CORPS : conversation, OU sous-vue Génération/Revue (Assistant général). ══
					if (chatView) {
						DrawMessages(ctx, body, kViolet, overlayOpen);
						// Banniere d'utilisation (voir calcul de bannerH plus haut).
						if (showBanner && topBucket) {
							const NkRect ban = {r.x, r.y + r.h - toolH - inH - bannerH, r.w, bannerH};
							dl.AddRectFilled(ban, NkColor{58, 46, 20, 255}, ctx.S(4.f));
							dl.AddRect(ban, NkColor{158, 122, 46, 255}, 1.f);
							const NkString msg =
								NkPrintf(NkT("ai.usage.banner"), (int32)(topBucket->utilization * 100.f + 0.5f),
										 UsageBucketLabel(topBucket->type).CStr(), RelTimeFromNow(topBucket->resetsAt).CStr());
							const float32 by = ban.y + (ban.h - lineH) * 0.5f + (font ? font->Ascent() : 12.f);
							if (font && font->Valid())
								dl.AddText(font->Face(), font->TexId(), {ban.x + ctx.S(10.f), by}, msg.CStr(),
										   NkColor{230, 200, 150, 255}, ban.w - ctx.S(160.f));
							// "Voir l'utilisation" (ouvre le popover) + fermer (x), a droite.
							const NkString viewLbl = NkT("ai.usage.viewusage");
							const float32 vw = measure(viewLbl.CStr());
							const NkRect viewR = {ban.x + ban.w - ctx.S(30.f) - vw - ctx.S(14.f), ban.y, vw + ctx.S(14.f),
												  ban.h};
							const bool viewHov = NkGuiRectContains(viewR, mp);
							if (font && font->Valid())
								dl.AddText(font->Face(), font->TexId(), {viewR.x + ctx.S(7.f), by}, viewLbl.CStr(),
										   viewHov ? NkColor{255, 220, 170, 255} : NkColor{210, 175, 120, 255});
							if (viewHov && ctx.input.mouseClicked[0])
								OpenUsagePopover();
							const NkRect xR = {ban.x + ban.w - ctx.S(26.f), ban.y + (ban.h - ctx.S(16.f)) * 0.5f,
											   ctx.S(16.f), ctx.S(16.f)};
							const bool xHov = NkGuiRectContains(xR, mp);
							if (xHov)
								dl.AddRectFilled(xR, ctx.theme.buttonHover, ctx.S(3.f));
							const float32 xa = ctx.S(4.f), xcx = xR.x + xR.w * 0.5f, xcy = xR.y + xR.h * 0.5f;
							dl.AddLine({xcx - xa, xcy - xa}, {xcx + xa, xcy + xa}, NkColor{210, 175, 120, 255}, 1.4f);
							dl.AddLine({xcx - xa, xcy + xa}, {xcx + xa, xcy - xa}, NkColor{210, 175, 120, 255}, 1.4f);
							if (xHov && ctx.input.mouseClicked[0])
								mUsageBannerDismissKey = bannerDismissKey;
						}
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
						if (mKind == 1) { // Claude Code : menu titre+description façon VSCode
							DrawModelMenu(ctx, r, kViolet);
						} else {
							int32 n = 0;
							const char *const *m = kModels(n);
							NkComboMenu(ctx, dl, font, mModelAnchor, r, m, n, mModelIdx, mComboOpen, kViolet,
										ctx.theme.panel, ctx.theme.border, ctx.theme.text, NkColor{255, 255, 255, 255},
										ctx.theme.buttonHover);
						}
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
					if (mUsageOpen)
						DrawUsagePopover(ctx, r, kViolet);
				}

			private:
				struct Msg {
						int32 role;
						NkString text;
						// icône OUTIL (role==2 uniquement) : 0 aucune, 1 fichier (Read/Write/Edit),
						// 2 terminal (Bash), 3 loupe (Grep/Glob), 4 générique (autre outil). PAS
						// d'emoji — la police NotoSans du projet n'a pas de couverture emoji
						// (rendu en tofu/« ? ») — icônes VECTORIELLES dessinées à la main, comme
						// partout ailleurs dans ce fichier (DrawFileIcon, DrawGear...).
						int32 tool = 0;
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
						// Brouillon de saisie, PROPRE a ce chat. Taille FIXE imposee par le
						// widget NKGui (convention ImGui : il ecrit dans un tampon fourni).
						// 8 Ko etait trop juste des qu'on collait un fichier ; 64 Ko couvre
						// tres largement la saisie manuelle. Les prompts COMPOSES (Revue,
						// Generation, Rejouer) ne passent plus par ici du tout, cf. mOut.
						char input[65536] = {0};
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
						// ── Backend CLI reel (mKind==1, Claude Code) ──
						NkString claudeSessionId; // session_id retourne par system/init -> --resume au tour suivant
				};

				// ── SORTIE DECOUPLEE du brouillon de saisie ────────────────────────
				// Le brouillon `input` est un tableau C de taille FIXE : le widget
				// NKGui de saisie multiligne suit la convention ImGui
				// (`InputTextMultiline(ctx, id, char *buf, int32 bufSize, ...)`) et
				// ecrit dans un tampon fourni par l'appelant. Or trois actions
				// composent un prompt qui embarque le contenu ENTIER d'un fichier —
				// Revue de code, Generation, Rejouer — puis appellent Send() dans la
				// foulee. Elles copiaient ce prompt dans le tampon : au-dela de la
				// capacite, le texte etait coupe EN PLEIN CODE, silencieusement, et
				// l'IA repondait sur un fichier ampute sans que personne le sache.
				// Ces prompts ne transitent donc plus par le brouillon : ils passent
				// par `mOut`, une NkString sans borne. Le tampon ne limite plus que ce
				// que l'utilisateur tape a la main.
				NkString mOut;		  // prompt COMPOSE en attente d'envoi
				bool mOutOn = false;  // true = c'est `mOut` qui part, pas le brouillon

				// Texte effectivement envoye.
				NkString OutText() const { return mOutOn ? mOut : NkString(mInput); }

				// Arme un envoi de prompt COMPOSE (jamais tronque).
				void SendComposed(const NkString &prompt) {
					mOut = prompt;
					mOutOn = true;
					Send();
				}

				// Vide la sortie ET le brouillon apres un envoi.
				void ClearOut() {
					mOut.Clear();
					mOutOn = false;
					mInput[0] = 0;
				}

				NkCodeState *mS;
				NkEditorShell *mShell = nullptr; // pour focus terminal (onglet Agent)
				NkVector<ChatSession> mChats;
				int32 mActiveChat = 0;
				int32 mBusyChat = -1; // chat CIBLE de la reponse en cours (peut differer de l'actif si switch)
				int32 mChatSeq = 0;	   // numerotation "Chat N"
				// ── Persistance par workspace (voir SaveChatsIfChanged/LoadChats) ──
				NkString mLoadedRoot; // racine du workspace dont les chats sont charges
				uint32 mSavedFp = 0;  // empreinte du dernier etat sauve (anti-ecritures inutiles)
				uint32 mSaveTick = 0; // debounce de la sauvegarde periodique
				bool mChatListOpen = false;
				NkRect mChatListAnchor = {0, 0, 0, 0};
				// ── Selection de texte LIBRE dans le fil de messages : PARTIELLE (glisser) ou
				// INTEGRALE (Ctrl+A souris sur le fil). Points A/B en coordonnees CONTENU
				// (x ecran fixe, y + scroll) -> la selection reste accrochee au texte pendant
				// le defilement. Ctrl+C copie la partie selectionnee (messages hors ecran
				// compris). Un simple clic efface (A == B). ──
				bool mSelDrag = false;
				NkVec2 mSelA{0.f, 0.f}, mSelB{0.f, 0.f};
				int32 mSelChat = -1; // chat proprietaire de la selection (reset au changement)
				float32 mToolScroll = 0.f;	   // défilement horizontal de la barre d'outils du bas
				float32 mToolContentW = 0.f;  // largeur intrinsèque de son contenu (frame précédente)
				int32 mProvider = 0; // 0 Claude, 1 Ollama, 2 Maison
				NkProcess mProc;
				NkVector<NkString> mRespLines; // accumule la sortie curl entre frames
				bool mBusy = false;

				// ── Backend CLI reel `claude` (mKind==1 UNIQUEMENT) : NkPipeProc (pipes
				// CreateProcessW, deja utilise pour clangd/LSP) au lieu de curl+NkProcess —
				// flux NDJSON (une ligne = un evenement), pas un JSON unique en fin de reponse. ──
				NkPipeProc mClaudeProc;
				NkString mClaudeBuf;		 // accumulation stdout entre frames (coupe sur '\n' -> 1 evenement)
				bool mClaudeStarted = false; // un message assistant "en cours" a deja ete pousse dans msgs
				int32 mClaudeMsgIdx = -1;	 // index STABLE du message assistant en cours (pas juste "le dernier" -
											 // des messages tool_use/thinking peuvent s'intercaler apres lui)
				bool mClaudeThinkStarted = false; // un bloc "reflexion" est deja en cours (mThinking uniquement)
				int32 mClaudeThinkIdx = -1;		  // index STABLE du message de reflexion en cours
				NkString mClaudeExePath;			  // cache : chemin NATIF resolu de claude.exe (voir ClaudeExe())
				bool mClaudeExeResolved = false;
				// ── PERMISSION EN ATTENTE (protocole can_use_tool, cf. SendClaudeCli) : le
				// CLI est BLOQUE en attendant notre control_response -> carte Autoriser/
				// Refuser dans le fil (DrawMessages) ; les boutons appellent
				// AnswerClaudePermission(). Une seule demande a la fois (le CLI serialise). ──
				// ── UTILISATION / LIMITES DE COMPTE (evenements reels `rate_limit_event` +
				// `result.usage`/`total_cost_usd` du CLI, PAS de simulation) : alimente le
				// popover "Compte et utilisation" (voir DrawUsagePopover). Compte partage
				// entre TOUS les chats Claude Code (l'API n'a qu'un seul compte).
				//
				// PLUSIEURS fenetres de quota coexistent (verifie via capture reelle du 20
				// juil + capture d'ecran VSCode fournie par Rihen : Session 5h, Hebdomadaire
				// 7 jours, Hebdomadaire PAR MODELE...) -> une entree PAR `rateLimitType` recu
				// (mise a jour independante, jamais une seule remplace les autres). ──
				struct RlBucket {
						NkString type;	// rateLimitType brut (cle) — ex "seven_day", "session"
						NkString status; // "allowed", "allowed_warning", "rejected"...
						float32 utilization = 0.f; // 0..1 (peut depasser 1 en depassement)
						int64 resetsAt = 0;		   // epoch unix (source rate_limit_event)
						NkString resetsText;	   // texte brut deja forme (source /usage, pas d'epoch fourni)
						bool overage = false;	   // isUsingOverage
						bool surpassed = false;   // surpassedThreshold (avertissement seuil)
				};
				NkVector<RlBucket> mRlBuckets;

				// ── USAGE PAR MODELE + GLOBAL (persiste sur disque, survit aux redemarrages
				// de NKCode) : une entree par nom de modele REELLEMENT utilise (capture depuis
				// "message.model" du CLI Claude Code, ou "model" de la reponse API brute) —
				// jamais agrege sous un libelle generique inventé. ──
				struct ModelUsage {
						NkString model;
						double costUsd = 0.0; // Claude Code : total_cost_usd (exact) ; API brute : ESTIME (pas de
											   // champ cout dans la reponse Messages -> tarif public applique aux tokens)
						int64 tokIn = 0, tokOut = 0;
						int32 turns = 0;
				};
				NkVector<ModelUsage> mModelUsage; // persiste (voir SaveUsagePersist/EnsureUsageLoaded)
				bool mUsageLoaded = false;
				double mLastCostUsd = 0.0; // cout de la DERNIERE requete (ephemere, pas persiste)
				int32 mLastTokIn = 0, mLastTokOut = 0;
				NkString mClaudeCurModel; // "message.model" du tour Claude Code EN COURS

				bool mUsageOpen = false; // popover "Compte et utilisation" ouvert
				// Le clic qui OUVRE le popover (lien "Voir l'utilisation" de la banniere, ou item
				// du panneau Actions) reste vu comme "mouseClicked[0]" par le test de fermeture
				// (clic en dehors) de DrawUsagePopover EXECUTE la MEME frame, juste apres : sans
				// ce garde-fou, le popover se referme instantanement des qu'il vient d'apparaitre
				// (sauf coincidence de position). Latch d'UNE frame, remis a false a chaque appel.
				bool mUsageJustOpened = false;
				NkString mUsageBannerDismissKey; // banniere ecartee POUR CETTE cle (type+%) — reapparait
												  // des que le bucket le plus urgent change de cle

				// ── Section ACCOUNT du popover (Auth method/Email/Organisation/Plan, comme
				// la capture VSCode) : `claude auth status --json` — sous-commande DEDIEE et
				// SURE du CLI (lit ses identifiants elle-meme), aucun fichier de secrets
				// touche par NKCode. Requete UNIQUE par session (pas d'appel API, gratuit,
				// quasi instantane), lancee au premier affichage du popover. ──
				NkProcess mAcctProc;
				NkVector<NkString> mAcctLines;
				bool mAcctRequested = false, mAcctLoaded = false, mAcctLoggedIn = false;
				NkString mAcctMethod, mAcctEmail, mAcctOrg, mAcctPlan;

				void PollAccountStatus() {
					if (!mAcctRequested)
						return;
					mAcctProc.Drain(mAcctLines);
					if (!mAcctProc.Done())
						return;
					mAcctRequested = false;
					NkString raw;
					for (usize i = 0; i < mAcctLines.Size(); ++i)
						raw += mAcctLines[i].CStr();
					mAcctLines.Clear();
					const char *j = raw.CStr();
					// Repli auto-reparable : si la sortie ne ressemble meme pas a du JSON
					// "auth status" (CLI absent, erreur transitoire, sortie vide...), ne
					// verrouille PAS mAcctLoaded -> un reessai reste possible au prochain
					// affichage du popover, au lieu de rester bloque sur "Chargement..."
					// pour le reste de la session (mAcctRequested est deja retombe a false,
					// donc OpenUsagePopover() relancera normalement la requete).
					if (NkFindSub(j, "loggedIn") == nullptr)
						return;
					mAcctLoaded = true;
					mAcctLoggedIn = NkFindSub(j, "\"loggedIn\":true") != nullptr ||
									NkFindSub(j, "\"loggedIn\": true") != nullptr;
					mAcctMethod = JsonStr(j, "authMethod");
					mAcctEmail = JsonStr(j, "email");
					mAcctOrg = JsonStr(j, "orgName");
					mAcctPlan = JsonStr(j, "subscriptionType");
				}

				// ── Rapport "/usage" : COMPLEMENT de rate_limit_event, qui ne rapporte QU'UNE
				// seule fenetre a la fois (celle qui vient de franchir un seuil — verifie
				// empiriquement le 20 juil, CLI v2.1.212 : un tour normal n'emet qu'un seul
				// `rate_limit_event`). `/usage` envoye comme prompt declenche une reponse
				// SYNTHETIQUE (model:"<synthetic>", num_turns:0, cout nul) qui contient TOUJOURS
				// la session (5h) ET la semaine (tous modeles), plus le detail "contributing to
				// your limits usage" — capture INTEGRALE du texte, aucun champ trie/invente. ──
				NkPipeProc mQuickUsageProc;
				NkString mQuickUsageBuf;
				bool mQuickUsageRequested = false;
				NkString mQuickUsageText; // texte brut renvoye par /usage (vide = jamais recu)
				bool mAutoUsageFetched = false; // declenche EnsureUsageDataFetching() une fois a l'entree du panneau
				int32 mUsagePeriod = 0;	  // bascule "contributing to your limits usage" : 0 Jour, 1 Semaine

				// Decoupe mQuickUsageText en 3 : preambule (avant "Last 24h"), bloc Jour
				// ("Last 24h" -> "Last 7d"), bloc Semaine ("Last 7d" -> fin). Repli honnete :
				// si les marqueurs ne sont pas trouves (format different), preamble recoit le
				// texte ENTIER tel quel (aucune donnee perdue, juste pas de bascule).
				void SplitQuickUsageDetail(NkString &preamble, NkString &dayBlock, NkString &weekBlock) const {
					preamble.Clear();
					dayBlock.Clear();
					weekBlock.Clear();
					const char *base = mQuickUsageText.CStr();
					const char *d = NkFindSub(base, "Last 24h");
					if (!d) {
						preamble = mQuickUsageText;
						preamble.Trim();
						return;
					}
					for (const char *q = base; q < d; ++q)
						preamble += *q;
					preamble.Trim();
					const char *w = NkFindSub(d, "Last 7d");
					if (w) {
						for (const char *q = d; q < w; ++q)
							dayBlock += *q;
						dayBlock.Trim();
						weekBlock = NkString(w);
						weekBlock.Trim();
					} else {
						dayBlock = NkString(d);
						dayBlock.Trim();
					}
				}

				// Lance les requetes REELLES (compte + /usage) si pas deja en cours —
				// factorise pour etre appelable SOIT au clic explicite "Voir l'utilisation"/
				// "Compte et utilisation", SOIT en arriere-plan des l'entree dans le panneau
				// (voir l'appel dans OnUI) : le temps que l'utilisateur ouvre le popover, les
				// donnees sont deja pretes (plus de "Chargement..." ni d'attente au clic).
				void EnsureUsageDataFetching() {
					if (mKind != 1)
						return;
					if (!mAcctLoaded && !mAcctRequested && !ClaudeExe().Empty()) {
						mAcctRequested = true;
						mAcctLines.Clear();
						mAcctProc.Start(NkWin32QuoteArg(ClaudeExe().CStr()) + " auth status --json");
					}
					TriggerQuickUsage();
				}

				// Ouvre le popover "Compte et utilisation" — les requetes qui l'alimentent sont
				// normalement DEJA en cours/faites (declenchees a l'entree du panneau, cf.
				// EnsureUsageDataFetching) ; ce rappel couvre le cas ou l'utilisateur rouvre le
				// popover APRES un premier essai infructueux (pas de latch definitif en echec).
				void OpenUsagePopover() {
					mUsageOpen = true;
					mUsageJustOpened = true;
					EnsureUsageDataFetching();
				}

				void TriggerQuickUsage() {
					if (mQuickUsageRequested || ClaudeExe().Empty())
						return;
					mQuickUsageRequested = true;
					mQuickUsageBuf.Clear();
					// --verbose est OBLIGATOIRE des que --output-format=stream-json est utilise
					// avec -p (sinon le CLI refuse purement et simplement : "Error: When using
					// --print, --output-format=stream-json requires --verbose") -> sans lui,
					// TOUTE la requete /usage echouait silencieusement (mQuickUsageText jamais
					// rempli, donc "Session" n'apparaissait JAMAIS, meme apres le parsing ajoute
					// dans ParseQuickUsageBuckets).
					const NkString cmd = NkWin32QuoteArg(ClaudeExe().CStr()) +
										  " -p --verbose --input-format stream-json --output-format stream-json "
										  "--permission-mode default";
					const NkString cwd = (mS && mS->HasWorkspace()) ? mS->root.ToString() : NkString(".");
					NkVector<NkString> noEnv;
					if (!mQuickUsageProc.StartWithEnv(cmd, cwd, noEnv, /*mergeStderr=*/true)) {
						mQuickUsageRequested = false; // pas de latch : reessai possible au prochain ouverture
						return;
					}
					const NkString um = "{\"type\":\"user\",\"message\":{\"role\":\"user\",\"content\":[{\"type\":"
										 "\"text\",\"text\":\"/usage\"}]}}\n";
					mQuickUsageProc.WriteData(um.CStr(), static_cast<int32>(um.Size()));
				}

				void PollQuickUsage() {
					if (!mQuickUsageRequested)
						return;
					char buf[4096];
					int32 n;
					while ((n = mQuickUsageProc.ReadAvail(buf, sizeof(buf))) > 0)
						mQuickUsageBuf.Append(buf, static_cast<usize>(n));
					if (mQuickUsageProc.Running())
						return;
					mQuickUsageRequested = false;
					NkString rest = mQuickUsageBuf;
					mQuickUsageBuf.Clear();
					for (;;) {
						const usize nl = rest.Find('\n');
						const NkString line = nl == NkString::npos ? rest : rest.SubStr(0, nl);
						if (NkFindSub(line.CStr(), "\"type\":\"result\"") != nullptr) {
							mQuickUsageText = JsonStr(line.CStr(), "result");
							mQuickUsageText.Trim();
							break;
						}
						if (nl == NkString::npos)
							break;
						rest.Erase(0, nl + 1);
					}
					// Rien recu (erreur CLI, flag manquant, etc.) : mQuickUsageRequested est deja
					// retombe a false plus haut -> reessai automatique a la prochaine ouverture.
					if (mQuickUsageText.Empty())
						return;
					ParseQuickUsageBuckets();
				}

				// Extrait "Current session: X% used · resets ..." et "Current week (all
				// models): X% used · resets ..." du texte /usage -> upsert dans mRlBuckets
				// (types synthetiques "session"/"seven_day", reconnus par UsageBucketLabel)
				// pour qu'ils apparaissent comme barres colorees dans la section USAGE
				// principale, PAS seulement dans le detail texte brut plus bas — sinon la
				// session (rarement rapportee par rate_limit_event, qui ne rapporte qu'UNE
				// seule fenetre a la fois) n'apparaissait jamais tant qu'aucun evenement
				// dedie ne l'avait fait remonter.
				void ParseQuickUsageBuckets() {
					EnsureUsageLoaded(); // fusionne les buckets deja connus (autres sessions) avant d'upserter
					auto upsert = [&](const char *marker, const char *type) {
						const char *m = NkFindSub(mQuickUsageText.CStr(), marker);
						if (!m)
							return;
						int32 markerLen = 0;
						while (marker[markerLen])
							++markerLen;
						const char *p = m + markerLen;
						int32 pct = 0;
						bool any = false;
						while (*p >= '0' && *p <= '9') {
							pct = pct * 10 + (*p - '0');
							++p;
							any = true;
						}
						if (!any || *p != '%')
							return;
						// avance jusqu'a "resets " (repli : garde le reste de la ligne tel quel).
						const char *rp = NkFindSub(p, "resets ");
						const char *le = p;
						while (*le && *le != '\n')
							++le;
						NkString resetsTxt;
						if (rp && rp < le)
							for (const char *q = rp + 7; q < le; ++q)
								resetsTxt += *q;
						// Met a jour SEULEMENT utilization/resetsText si un bucket du meme type
						// existe deja (ex. rate_limit_event, plus precis sur resetsAt/status/
						// surpassed) — ne degrade jamais une donnee plus riche deja connue.
						bool found = false;
						for (usize i = 0; i < mRlBuckets.Size() && !found; ++i)
							if (mRlBuckets[i].type == type) {
								mRlBuckets[i].utilization = static_cast<float32>(pct) / 100.f;
								mRlBuckets[i].resetsText = resetsTxt;
								found = true;
							}
						if (!found) {
							RlBucket b;
							b.type = type;
							b.utilization = static_cast<float32>(pct) / 100.f;
							b.resetsText = resetsTxt;
							mRlBuckets.PushBack(b);
						}
					};
					upsert("Current session: ", "session");
					upsert("Current week (all models): ", "seven_day");
					SaveUsagePersist();
				}

				// Decoupe un texte libre en lignes ne depassant pas maxW (mots entiers jamais
				// coupes ; les "\n" du texte source deviennent des lignes vides preservees).
				static void WrapTextLines(const NkGuiFont *font, const char *text, float32 maxW,
										   NkVector<NkString> &out) {
					if (!text)
						return;
					if (!font || !font->Valid()) {
						out.PushBack(NkString(text));
						return;
					}
					const char *p = text;
					while (*p) {
						const char *lineEnd = p;
						while (*lineEnd && *lineEnd != '\n')
							++lineEnd;
						if (lineEnd == p) {
							out.PushBack(NkString());
						} else {
							NkString cur;
							const char *w = p;
							while (w < lineEnd) {
								const char *ws = w;
								while (w < lineEnd && *w != ' ')
									++w;
								NkString word;
								for (const char *q = ws; q < w; ++q)
									word += *q;
								NkString trial = cur;
								if (!trial.Empty())
									trial += " ";
								trial += word;
								if (!cur.Empty() && font->MeasureWidth(trial.CStr()) > maxW) {
									out.PushBack(cur);
									cur = word;
								} else {
									cur = trial;
								}
								while (w < lineEnd && *w == ' ')
									++w;
							}
							if (!cur.Empty())
								out.PushBack(cur);
						}
						p = lineEnd;
						if (*p == '\n')
							++p;
					}
				}

				bool mPermPending = false;
				NkString mPermReqId;	// request_id a renvoyer dans la reponse
				NkString mPermTool;		// nom de l'outil demande (Write/Bash/...)
				NkString mPermDetail;	// detail lisible (file_path/command/pattern)
				NkString mPermInputRaw; // sous-objet JSON brut "input" (renvoye en updatedInput)
				int32 mPermIcon = 0;	// icone vectorielle (ToolIconFor)

				NkVector<Msg> &Msgs() { return mChats[static_cast<usize>(mActiveChat)].msgs; }
				const NkVector<Msg> &Msgs() const { return mChats[static_cast<usize>(mActiveChat)].msgs; }
				NkVector<Msg> &MsgsOf(int32 idx) { return mChats[static_cast<usize>(idx)].msgs; }

				void NewChat() {
					ChatSession c;
					c.title = NkPrintf("%s %d", NkT("ai.chat"), ++mChatSeq);
					c.msgs.PushBack({2, NkString(NkT("ai.hello"))});
					// Claude Code (mKind==1) : "Manuel" (idx 0, --permission-mode default) est
					// REELLEMENT interactif depuis le 20 juil (protocole --permission-prompt-tool
					// stdio + control_request/control_response, verifie empiriquement) -> plus
					// besoin de retomber sur "Plan" par prudence, "Manuel" est desormais le
					// defaut le plus utile (façon VSCode : demande a chaque outil).
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

				// ── PERSISTANCE des conversations PAR WORKSPACE ─────────────────────
				// Fichier : <ws>/.nkcode/ai_chats_<kind>.cfg (1 fichier par agent).
				// Format v1 ligne-a-ligne, '\n' et '\\' echappes dans les textes.
				NkString ChatsPath() const {
					return mLoadedRoot + "/.nkcode/ai_chats_" + NkPrintf("%d", mKind).CStr() + ".cfg";
				}
				static void EscTo(NkString &o, const char *s) {
					for (; s && *s; ++s) {
						if (*s == '\n')
							o += "\\n";
						else if (*s == '\\')
							o += "\\\\";
						else if (*s != '\r')
							o += *s;
					}
				}
				static NkString Unesc(const char *s, usize n) {
					NkString o;
					for (usize i = 0; i < n; ++i) {
						if (s[i] == '\\' && i + 1 < n) {
							++i;
							o += (s[i] == 'n') ? '\n' : s[i];
						} else
							o += s[i];
					}
					return o;
				}
				// Empreinte bon marche : detecte tout changement notable sans re-serialiser.
				uint32 ChatsFingerprint() const {
					uint32 h = 2166136261u;
					auto mix = [&h](uint32 v) { h = (h ^ v) * 16777619u; };
					mix((uint32)mChats.Size());
					mix((uint32)mActiveChat);
					mix((uint32)mChatSeq);
					for (usize i = 0; i < mChats.Size(); ++i) {
						const ChatSession &c = mChats[i];
						mix((uint32)c.msgs.Size());
						mix((uint32)c.title.Size());
						mix((uint32)::strlen(c.input));
						mix((uint32)::strlen(c.system));
						mix((uint32)c.claudeSessionId.Size());
						mix((uint32)(c.modelIdx | (c.mode << 4) | (c.scope << 8) | (c.editAuth << 12) |
									 (c.effort << 16) | (c.thinking ? 1 << 20 : 0) | (c.ctxFileOn ? 1 << 21 : 0)));
						mix((uint32)(c.temp * 1000.f));
						mix((uint32)c.maxTokens);
						if (!c.msgs.Empty())
							mix((uint32)c.msgs[c.msgs.Size() - 1].text.Size());
					}
					return h;
				}
				void SaveChatsIfChanged() {
					if (mLoadedRoot.Empty())
						return;
					const uint32 fp = ChatsFingerprint();
					if (fp == mSavedFp)
						return;
					NkString o("v1\n");
					o += NkPrintf("active=%d\nseq=%d\n", mActiveChat, mChatSeq).CStr();
					for (usize i = 0; i < mChats.Size(); ++i) {
						const ChatSession &c = mChats[i];
						o += "[chat]\n";
						o += "title=";
						EscTo(o, c.title.CStr());
						o += "\nsid=";
						EscTo(o, c.claudeSessionId.CStr());
						o += NkPrintf("\nprops=%d %d %d %d %d %d %d %d %d %d\n", c.modelIdx, c.mode, c.scope,
									  c.editAuth, c.ctxFileOn ? 1 : 0, (int32)(c.temp * 1000.f), c.maxTokens,
									  c.effort, c.thinking ? 1 : 0, c.autoSwitchFlagged ? 1 : 0)
								 .CStr();
						o += "system=";
						EscTo(o, c.system);
						o += "\ninput=";
						EscTo(o, c.input);
						o += "\n";
						for (usize m = 0; m < c.msgs.Size(); ++m) {
							o += NkPrintf("m=%d %d ", c.msgs[m].role, c.msgs[m].tool).CStr();
							EscTo(o, c.msgs[m].text.CStr());
							o += "\n";
						}
					}
					NkDirectory::CreateRecursive(NkPath(mLoadedRoot + "/.nkcode"));
					if (NkFile::WriteAllText(NkPath(ChatsPath().CStr()), o))
						mSavedFp = fp;
				}
				void LoadChats() {
					mChats.Clear();
					mActiveChat = 0;
					mChatSeq = 0;
					mSelChat = -1;
					if (mLoadedRoot.Empty()) {
						mSavedFp = ChatsFingerprint();
						return;
					}
					const NkString txt = NkFile::ReadAllText(NkPath(ChatsPath().CStr()));
					const char *p = txt.CStr();
					if (txt.Size() < 3 || ::strncmp(p, "v1\n", 3) != 0) {
						mSavedFp = ChatsFingerprint(); // pas de fichier / version inconnue
						return;
					}
					ChatSession *cur = nullptr;
					int32 active = 0;
					for (const char *ls = p; *ls;) {
						const char *le = ls;
						while (*le && *le != '\n')
							++le;
						const usize ln = (usize)(le - ls);
						auto is = [&](const char *k) {
							const usize kl = ::strlen(k);
							return ln >= kl && ::strncmp(ls, k, kl) == 0;
						};
						if (is("active="))
							active = ::atoi(ls + 7);
						else if (is("seq="))
							mChatSeq = ::atoi(ls + 4);
						else if (is("[chat]")) {
							mChats.PushBack(ChatSession());
							cur = &mChats[mChats.Size() - 1];
						} else if (cur && is("title="))
							cur->title = Unesc(ls + 6, ln - 6);
						else if (cur && is("sid="))
							cur->claudeSessionId = Unesc(ls + 4, ln - 4);
						else if (cur && is("props=")) {
							int32 v[10] = {0};
							::sscanf(ls + 6, "%d %d %d %d %d %d %d %d %d %d", &v[0], &v[1], &v[2], &v[3], &v[4],
									 &v[5], &v[6], &v[7], &v[8], &v[9]);
							cur->modelIdx = v[0];
							cur->mode = v[1];
							cur->scope = v[2];
							cur->editAuth = v[3];
							cur->ctxFileOn = v[4] != 0;
							cur->temp = (float32)v[5] / 1000.f;
							cur->maxTokens = v[6];
							cur->effort = v[7];
							cur->thinking = v[8] != 0;
							cur->autoSwitchFlagged = v[9] != 0;
						} else if (cur && is("system=")) {
							const NkString s2 = Unesc(ls + 7, ln - 7);
							const usize cap = sizeof(cur->system) - 1;
							const usize n = s2.Size() < cap ? s2.Size() : cap;
							::memcpy(cur->system, s2.CStr(), n);
							cur->system[n] = 0;
						} else if (cur && is("input=")) {
							const NkString s2 = Unesc(ls + 6, ln - 6);
							const usize cap = sizeof(cur->input) - 1;
							const usize n = s2.Size() < cap ? s2.Size() : cap;
							::memcpy(cur->input, s2.CStr(), n);
							cur->input[n] = 0;
						} else if (cur && is("m=")) {
							Msg msg;
							int32 role = 0, tool = 0, off = 0;
							if (::sscanf(ls + 2, "%d %d %n", &role, &tool, &off) >= 2 && off > 0) {
								msg.role = role;
								msg.tool = tool;
								msg.text = Unesc(ls + 2 + off, ln - 2 - (usize)off);
								cur->msgs.PushBack(msg);
							}
						}
						ls = *le ? le + 1 : le;
					}
					if (active >= 0 && active < (int32)mChats.Size())
						mActiveChat = active;
					mSavedFp = ChatsFingerprint(); // etat charge = etat sauve
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

				// Modèles par agent (pilule + menu). Le provider effectif est dérivé du choix.
				const char *const *ModelsFor(int32 kind, int32 &n) const {
					static const char *general[] = {"claude-3.5-sonnet", "claude-sonnet-5", "Ollama (llama3.2)",
													"NkAI (maison)"};
					// Alias --model REELS acceptes par le CLI `claude` (verifie via --help :
					// "Provide an alias for the latest model (e.g. 'fable', 'opus', or
					// 'sonnet')") — "auto" = ne passe PAS --model, laisse le defaut du CLI.
					// Remplace l'ancienne liste figee (claude-3.5-sonnet/claude-opus-4, perimee).
					static const char *claudeCode[] = {"auto", "sonnet", "opus", "fable", "haiku"};
					static const char *codex[] = {"gpt-5-codex", "o4-mini"};
					static const char *nkai[] = {"NkAI-base (Rihen)"};
					switch (kind) {
						case 1:
							n = 5;
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

				// ── Segment d'une réponse assistant : prose (enveloppée par mot, comme
				// WrapLines) OU bloc de code ``` (lignes BRUTES, pas de word-wrap — un
				// découpage arbitraire casserait la lisibilité du code). ──
				struct MsgBlock {
						bool code = false;
						NkVector<NkString> lines;
				};

				// ── Rend une ligne de PROSE lisible : le modele ecrit du Markdown, on
				// n'affichait que sa syntaxe brute (« **texte** », « ### Titre », « - item »,
				// « `code` »). On NORMALISE le texte AVANT l'habillage (WrapLines) — et non
				// au dessin — pour que l'habillage, la SELECTION, les liens fichier et le
				// copier travaillent tous sur EXACTEMENT le texte affiche. Styler par
				// caractere au dessin desynchroniserait ces quatre mecanismes.
				// Traite : **gras** / __gras__ / *ital* / _ital_ (marqueurs retires),
				// `code` (guillemets retires), titres # (# retires), puces -/*/+ -> « • ».
				// Les blocs ``` ne passent PAS ici (traites a part, texte brut preserve).
				static NkString NormalizeProse(const char *s) {
					NkString out;
					bool atLineStart = true;
					for (const char *p = s; *p;) {
						if (atLineStart) {
							const char *q = p;
							int32 indent = 0;
							while (*q == ' ' || *q == '\t') { // conserve l'indentation (listes imbriquees)
								out += *q;
								++q;
								++indent;
							}
							if (*q == '#') { // titre : « ### Titre » -> « Titre »
								while (*q == '#')
									++q;
								while (*q == ' ')
									++q;
								p = q;
								atLineStart = false;
								continue;
							}
							// puce : « - item », « * item », « + item » -> « • item »
							if ((*q == '-' || *q == '*' || *q == '+') && q[1] == ' ') {
								out += "\xE2\x80\xA2"; // •
								out += ' ';
								p = q + 2;
								atLineStart = false;
								continue;
							}
							// « --- » / « *** » : separateur horizontal -> ligne de tirets
							if ((*q == '-' || *q == '*' || *q == '_') && q[1] == *q && q[2] == *q) {
								const char rc = *q; // teste APRES les puces (« - item » a q[1]==' ')
								while (*q == rc)
									++q;
								out += "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"; // ────
								p = q;
								atLineStart = false;
								continue;
							}
							p = q;
							atLineStart = false;
							continue;
						}
						if (*p == '\n') {
							out += *p++;
							atLineStart = true;
							continue;
						}
						// **gras** / __gras__ : marqueurs APPARIES uniquement (« a ** b » reste tel quel)
						if ((p[0] == '*' && p[1] == '*') || (p[0] == '_' && p[1] == '_')) {
							const char c = p[0];
							const char *e = p + 2;
							while (*e && *e != '\n' && !(e[0] == c && e[1] == c))
								++e;
							if (*e == c && e[1] == c) { // ferme sur la meme ligne -> on retire les 4 marqueurs
								for (const char *k = p + 2; k < e; ++k)
									out += *k;
								p = e + 2;
								continue;
							}
						}
						// `code` en ligne : retire les backticks, garde le contenu
						if (*p == '`') {
							const char *e = p + 1;
							while (*e && *e != '\n' && *e != '`')
								++e;
							if (*e == '`') {
								for (const char *k = p + 1; k < e; ++k)
									out += *k;
								p = e + 1;
								continue;
							}
						}
						// *italique* / _italique_ : un seul marqueur, apparie sur la ligne.
						// Prudence : on n'y touche que si le contenu ne contient pas d'espace
						// avant la fermeture immediate (evite de manger « 3 * 4 = 12 »).
						if ((*p == '*' || *p == '_') && p[1] && p[1] != ' ' && p[1] != *p) {
							const char c = *p;
							const char *e = p + 1;
							while (*e && *e != '\n' && *e != c)
								++e;
							if (*e == c && e > p + 1 && e[-1] != ' ') {
								for (const char *k = p + 1; k < e; ++k)
									out += *k;
								p = e + 1;
								continue;
							}
						}
						out += *p++;
					}
					return out;
				}

				// ── Détecte les fences ``` (même heuristique que NkMarkdown.h : une ligne qui,
				// une fois les espaces de tête retirés, commence par ```) et découpe `text` en
				// blocs prose/code alternés. La prose entre deux blocs de code est ré-enveloppée
				// d'un coup (WrapLines) pour ne pas casser le word-wrap sur les sauts de ligne
				// internes au bloc. ──
				static void SplitBlocks(NkGuiContext &ctx, const char *text, float32 maxW, NkVector<MsgBlock> &out) {
					out.Clear();
					NkVector<NkString> rawLines;
					{
						NkString ln;
						for (const char *p = text;; ++p) {
							if (*p == '\n' || *p == '\0') {
								rawLines.PushBack(ln);
								ln.Clear();
								if (!*p)
									break;
							} else if (*p != '\r')
								ln += *p;
						}
					}
					NkString proseAcc;
					auto flushProse = [&]() {
						if (proseAcc.Empty())
							return;
						MsgBlock b;
						b.code = false;
						WrapLines(ctx, NormalizeProse(proseAcc.CStr()).CStr(), maxW, b.lines);
						out.PushBack(b);
						proseAcc.Clear();
					};
					usize i = 0;
					while (i < rawLines.Size()) {
						const char *t = rawLines[i].CStr();
						int32 sp = 0;
						while (t[sp] == ' ')
							++sp;
						if (t[sp] == '`' && t[sp + 1] == '`' && t[sp + 2] == '`') {
							flushProse();
							MsgBlock b;
							b.code = true;
							usize j = i + 1;
							while (j < rawLines.Size()) {
								const char *e = rawLines[j].CStr();
								int32 es = 0;
								while (e[es] == ' ')
									++es;
								if (e[es] == '`' && e[es + 1] == '`' && e[es + 2] == '`')
									break;
								b.lines.PushBack(rawLines[j]);
								++j;
							}
							if (b.lines.Empty()) // fence vide (```\n```) : évite un bloc à hauteur 0 invisible
								b.lines.PushBack(NkString(""));
							out.PushBack(b);
							i = (j < rawLines.Size()) ? j + 1 : j; // saute la fence fermante si trouvée
							continue;
						}
						if (!proseAcc.Empty())
							proseAcc += "\n";
						proseAcc += rawLines[i];
						++i;
					}
					flushProse();
					if (out.Empty()) { // texte vide ou sans fences reconnues : un seul bloc prose (comportement d'avant)
						MsgBlock b;
						b.code = false;
						WrapLines(ctx, NormalizeProse(text).CStr(), maxW, b.lines);
						out.PushBack(b);
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
				void DrawMessages(NkGuiContext &ctx, const NkRect &body, const NkColor &violet, bool overlayOpen) {
					auto &dl = ctx.DL();
					// Un popover/menu ouvert PAR-DESSUS (palette d'actions, combo, props...) rend ce
					// contenu non-interactif : un point hors-écran ne matche jamais aucun
					// NkGuiRectContains ici -> plus aucun hover/clic/molette ne « traverse » vers le
					// chat caché dessous (bug remonté par Rihen).
					const NkVec2 mp = overlayOpen ? NkVec2{-99999.f, -99999.f} : ctx.input.mousePos;
					NkVector<Msg> &msgs = Msgs();
					float32 &scroll = mChats[static_cast<usize>(mActiveChat)].scroll;
					bool &stick = mChats[static_cast<usize>(mActiveChat)].stick;
					const float32 lineH = ctx.font && ctx.font->Valid() ? ctx.font->LineHeight() : 16.f;
					const float32 asc = ctx.font && ctx.font->Valid() ? ctx.font->Ascent() : 12.f;
					const float32 pad = ctx.S(12.f), bubPad = ctx.S(11.f), gap = ctx.S(14.f), rnd = ctx.S(10.f);
					const NkColor kUserBg = {42, 46, 58, 255}; // bulle utilisateur PALE (pas violette)
					// Largeur de contenu REDUITE de la largeur de la scrollbar STANDARD
					// (NkEditorScrollbar.h) — jamais de scrollbar maison redessinee a la main
					// (et jamais de zone de scroll SANS scrollbar visible/glissable non plus).
					const float32 sbW = NkScrollbarWidth();
					const float32 contentW = body.w - sbW;
					const float32 userMaxW = contentW - pad * 2.f;
					const float32 userTextMaxW = userMaxW - bubPad * 2.f;
					// Fil vertical (thread) : puce a threadX, texte a partir de threadX+dotGap.
					const float32 threadX = body.x + pad + ctx.S(3.f);
					const float32 dotGap = ctx.S(16.f);
					const float32 threadTextMaxW = contentW - pad - dotGap - ctx.S(4.f) - pad;
					const bool typing = mBusy && mBusyChat == mActiveChat;
					// Carte de PERMISSION en attente (protocole can_use_tool) : le CLI est
					// BLOQUE, remplace l'indicateur "Thinking..." tant qu'elle est affichee.
					const bool permHere = mPermPending && mBusyChat == mActiveChat;
					const float32 permCardH = ctx.S(70.f);

					// 1) mesure de la hauteur totale (bulle pour user, texte simple pour le reste).
					// Les réponses assistant (role 1) réservent une ligne d'actions (Copier /
					// Insérer dans l'éditeur / Retry) sous le texte, façon Copilot Chat/Cursor,
					// et sont découpées en blocs prose/code (```) — chaque bloc de code reçoit un
					// fond distinct, pas de word-wrap dessus (voir SplitBlocks/MsgBlock).
					const float32 actRowH = ctx.S(24.f);
					const float32 blockGap = ctx.S(6.f);  // interligne entre 2 blocs (prose/code) d'un MEME message
					const float32 codePad = ctx.S(16.f);  // marge haut+bas a l'interieur d'un bloc de code
					auto blockHeight = [&](const NkVector<MsgBlock> &blocks) {
						float32 h = 0.f;
						for (usize bi = 0; bi < blocks.Size(); ++bi) {
							if (bi > 0)
								h += blockGap;
							h += blocks[bi].lines.Size() * lineH + (blocks[bi].code ? codePad : 0.f);
						}
						return h;
					};
					NkVector<NkVector<NkString>> wrapped;		 // role 0 (user) et role 2 (systeme) UNIQUEMENT
					NkVector<NkVector<MsgBlock>> blocksFor; // role 1 (assistant) UNIQUEMENT — vide sinon (indices alignes)
					float32 total = pad;
					for (usize i = 0; i < msgs.Size(); ++i) {
						const bool user = (msgs[i].role == 0);
						if (msgs[i].role == 1) {
							NkVector<MsgBlock> blocks;
							SplitBlocks(ctx, msgs[i].text.CStr(), threadTextMaxW, blocks);
							total += blockHeight(blocks) + actRowH + gap;
							wrapped.PushBack(NkVector<NkString>());
							blocksFor.PushBack(blocks);
							continue;
						}
						NkVector<NkString> ls;
						WrapLines(ctx, msgs[i].text.CStr(), user ? userTextMaxW : threadTextMaxW, ls);
						total += ls.Size() * lineH + (user ? bubPad * 2.f : 0.f) + gap;
						wrapped.PushBack(ls);
						blocksFor.PushBack(NkVector<MsgBlock>());
					}
					if (permHere)
						total += permCardH + gap; // carte Autoriser/Refuser
					else if (typing)
						total += lineH * 2.f + gap; // statut « Thinking… » + « Mustering… »
					const float32 viewH = body.h;
					const float32 maxScroll = total > viewH ? (total - viewH) : 0.f;
					if (NkGuiRectContains(body, mp) && ctx.input.wheel != 0.f) {
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

					// ── SELECTION DE TEXTE (partielle par glisser, integrale par Ctrl+A) ──
					if (mSelChat != mActiveChat) { // la selection appartient a UN chat
						mSelA = mSelB = NkVec2{0.f, 0.f};
						mSelDrag = false;
						mSelChat = mActiveChat;
					}
					if (!overlayOpen && ctx.input.mouseClicked[0] && NkGuiRectContains(body, ctx.input.mousePos) &&
						ctx.input.mousePos.x < body.x + contentW && !ctx.input.ctrlDown) {
						mSelDrag = true; // nouveau point d'ancre (un simple clic EFFACE : A == B)
						mSelA = NkVec2{ctx.input.mousePos.x, ctx.input.mousePos.y - body.y + scroll};
						mSelB = mSelA;
					}
					if (mSelDrag) {
						if (ctx.input.mouseDown[0])
							mSelB = NkVec2{ctx.input.mousePos.x, ctx.input.mousePos.y - body.y + scroll};
						else
							mSelDrag = false;
					}
					if (!overlayOpen && ctx.input.wantSelectAll && NkGuiRectContains(body, ctx.input.mousePos)) {
						mSelA = NkVec2{-1.0e8f, -1.0e8f}; // englobe TOUT le contenu (integrale)
						mSelB = NkVec2{1.0e8f, 1.0e8f};
						ctx.input.wantSelectAll = false;
					}
					const bool selHas = (mSelA.x != mSelB.x || mSelA.y != mSelB.y);
					NkVec2 sTop = mSelA, sBot = mSelB; // points ordonnes haut -> bas
					if (sBot.y < sTop.y || (sBot.y == sTop.y && sBot.x < sTop.x)) {
						const NkVec2 t = sTop;
						sTop = sBot;
						sBot = t;
					}
					// Largeur d'un prefixe de `s` (k octets, borne) — mesure police partagee par
					// la selection et les liens fichiers.
					char selBuf[1024];
					auto prefW = [&](const char *s, int32 k) -> float32 {
						if (!ctx.font || !ctx.font->Valid())
							return 0.f;
						if (k > 1000)
							k = 1000;
						for (int32 q = 0; q < k; ++q)
							selBuf[q] = s[q];
						selBuf[k < 0 ? 0 : k] = 0;
						return ctx.font->MeasureWidth(selBuf);
					};
					// Index d'octet le plus proche de la position X `target` (relative au debut de
					// ligne), en avancant codepoint par codepoint (jamais coupe un UTF-8).
					auto colAtX = [&](const char *s, float32 target) -> int32 {
						if (target <= 0.f)
							return 0;
						int32 n = 0;
						while (s[n])
							++n;
						int32 k = 0, bestC = n;
						float32 best = 1.0e9f;
						while (k <= n) {
							const float32 w = prefW(s, k);
							const float32 d = w > target ? w - target : target - w;
							if (d < best) {
								best = d;
								bestC = k;
							}
							if (k >= n)
								break;
							const unsigned char c = (unsigned char)s[k];
							k += (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : (c >= 0xC0) ? 2 : 1;
						}
						return bestC;
					};
					// Bornes [c0, c1) de la partie selectionnee d'une ligne (coordonnees CONTENU).
					// false = ligne entierement hors selection.
					auto selRange = [&](float32 tx, float32 cTop, const char *s, int32 &c0, int32 &c1) -> bool {
						if (!selHas)
							return false;
						const float32 cBot = cTop + lineH;
						if (cBot <= sTop.y || cTop >= sBot.y)
							return false;
						int32 n = 0;
						while (s[n])
							++n;
						const bool startHere = sTop.y >= cTop && sTop.y < cBot;
						const bool endHere = sBot.y >= cTop && sBot.y < cBot;
						c0 = startHere ? colAtX(s, sTop.x - tx) : 0;
						c1 = endHere ? colAtX(s, sBot.x - tx) : n;
						if (c1 < c0) {
							const int32 t = c0;
							c0 = c1;
							c1 = t;
						}
						return true;
					};
					// Surlignage (rendu uniquement — la collecte pour Ctrl+C est faite dans la
					// passe dediee ci-dessous, messages hors ecran compris).
					const NkColor kSelBg = {64, 96, 158, 110};
					auto selDraw = [&](float32 tx, float32 tyTop, const char *s) {
						int32 c0, c1;
						if (!selRange(tx, tyTop - body.y + scroll, s, c0, c1))
							return;
						const float32 w0 = prefW(s, c0), w1 = prefW(s, c1);
						const float32 w = (w1 - w0) < 2.f ? 2.f : (w1 - w0); // ligne vide = petit marqueur
						dl.AddRectFilled({tx + w0, tyTop, w, lineH}, kSelBg, 0.f);
					};
					// ── LIEN FICHIER (Ctrl+survol) : le token sous la souris qui ressemble a un
					// chemin EXISTANT (absolu, ou relatif a la racine du workspace, suffixe :NNN
					// tolere) est souligne ; Ctrl+clic l'ouvre dans l'editeur. ──
					auto fileLink = [&](float32 tx, float32 tyTop, const char *s) {
						if (!ctx.input.ctrlDown || overlayOpen || !mS)
							return;
						if (mp.y < tyTop || mp.y >= tyTop + lineH || mp.x < tx)
							return;
						const int32 col = colAtX(s, mp.x - tx);
						int32 n = 0;
						while (s[n])
							++n;
						if (col >= n)
							return;
						int32 a = col, b = col;
						while (a > 0 && s[a - 1] != ' ' && s[a - 1] != '\t')
							--a;
						while (b < n && s[b] != ' ' && s[b] != '\t')
							++b;
						// retire la ponctuation d'emballage (guillemets, parentheses, backticks...)
						while (b > a && (s[b - 1] == ',' || s[b - 1] == ')' || s[b - 1] == ';' || s[b - 1] == '"' ||
										 s[b - 1] == '\'' || s[b - 1] == '`' || s[b - 1] == '.'))
							--b;
						while (a < b && (s[a] == '(' || s[a] == '"' || s[a] == '\'' || s[a] == '`' || s[a] == '@'))
							++a;
						if (b - a < 3 || b - a > 500)
							return;
						char tok[512];
						int32 tn = 0;
						bool pathish = false;
						for (int32 q = a; q < b; ++q) {
							tok[tn++] = s[q];
							if (s[q] == '/' || s[q] == '\\')
								pathish = true;
						}
						tok[tn] = 0;
						// suffixe :NNN (style compilateur "fichier.cpp:42") -> retire pour le test
						int32 cut = tn;
						while (cut > 0 && tok[cut - 1] >= '0' && tok[cut - 1] <= '9')
							--cut;
						if (cut > 0 && cut < tn && tok[cut - 1] == ':')
							tok[cut - 1] = 0;
						else if (!pathish) { // sans separateur ni :NNN : exige une extension (a.b)
							bool dot = false;
							for (int32 q = 1; tok[q]; ++q)
								if (tok[q] == '.')
									dot = true;
							if (!dot)
								return;
						}
						NkString full(tok);
						if (!NkFile::Exists(full.CStr())) { // relatif a la racine du workspace ?
							full = mS->root.ToString();
							full += "/";
							full += tok;
							if (!NkFile::Exists(full.CStr()))
								return;
						}
						const float32 wA = prefW(s, a), wB = prefW(s, b);
						dl.AddLine({tx + wA, tyTop + lineH - 1.f}, {tx + wB, tyTop + lineH - 1.f}, violet, 1.2f);
						if (ctx.input.mouseClicked[0]) {
							mS->OpenPath(NkPath(full));
							ctx.input.mouseClicked[0] = false;
							mSelDrag = false; // le clic-lien n'amorce pas une selection
							mSelA = mSelB;
						}
					};
					// ── COLLECTE pour Ctrl+C : rejoue la MEME disposition que le rendu (memes
					// formules y/tx) sur TOUS les messages, y compris hors ecran. ──
					if (selHas && ctx.input.wantCopy && !overlayOpen) {
						NkString out;
						auto grab = [&](float32 tx, float32 cTop, const char *s) {
							int32 c0, c1;
							if (!selRange(tx, cTop, s, c0, c1))
								return;
							for (int32 q = c0; q < c1; ++q)
								out += s[q];
							out += '\n';
						};
						float32 cy = pad; // y en coordonnees CONTENU (= ecran + scroll - body.y)
						for (usize i = 0; i < msgs.Size(); ++i) {
							if (msgs[i].role == 0) {
								const NkVector<NkString> &ls = wrapped[i];
								for (usize k = 0; k < ls.Size(); ++k)
									grab(body.x + pad + bubPad, cy + bubPad + k * lineH, ls[k].CStr());
								cy += ls.Size() * lineH + bubPad * 2.f + gap;
							} else if (msgs[i].role == 1) {
								const NkVector<MsgBlock> &blocks = blocksFor[i];
								float32 by = cy;
								for (usize bi = 0; bi < blocks.Size(); ++bi) {
									if (bi > 0)
										by += blockGap;
									const MsgBlock &blk = blocks[bi];
									if (blk.code) {
										for (usize k = 0; k < blk.lines.Size(); ++k)
											grab(threadX + dotGap + ctx.S(10.f), by + codePad * 0.5f + k * lineH,
												 blk.lines[k].CStr());
										by += blk.lines.Size() * lineH + codePad;
									} else {
										for (usize k = 0; k < blk.lines.Size(); ++k)
											grab(threadX + dotGap, by + k * lineH, blk.lines[k].CStr());
										by += blk.lines.Size() * lineH;
									}
								}
								cy += blockHeight(blocks) + actRowH + gap;
							} else {
								const NkVector<NkString> &ls = wrapped[i];
								const NkComboIconFn ti = ToolIconFor(msgs[i].tool);
								const float32 iw = ti ? lineH * 0.85f : 0.f;
								for (usize k = 0; k < ls.Size(); ++k)
									grab(threadX + dotGap + (k == 0 ? iw + ctx.S(4.f) : 0.f), cy + k * lineH,
										 ls[k].CStr());
								cy += ls.Size() * lineH + gap;
							}
						}
						if (!out.Empty()) {
							while (!out.Empty() && out.Back() == '\n')
								out.PopBack(); // pas de saut de ligne final parasite
							ctx.SetClipboard(out.CStr());
							ctx.input.wantCopy = false;
						}
					}

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
						const bool user = (m.role == 0);
						if (user) {
							const NkVector<NkString> &ls = wrapped[i];
							prevDotY = -1.f; // casse la chaine : le fil ne traverse pas cette bulle
							const float32 bh = ls.Size() * lineH + bubPad * 2.f;
							if (y + bh >= body.y && y <= body.y + body.h) {
								dl.AddRectFilled({body.x + pad, y, userMaxW, bh}, kUserBg, rnd);
								if (ctx.font && ctx.font->Valid())
									for (usize k = 0; k < ls.Size(); ++k) {
										selDraw(body.x + pad + bubPad, y + bubPad + k * lineH, ls[k].CStr());
										dl.AddText(ctx.font->Face(), ctx.font->TexId(),
												   {body.x + pad + bubPad, y + bubPad + k * lineH + asc}, ls[k].CStr(),
												   ctx.theme.text);
										fileLink(body.x + pad + bubPad, y + bubPad + k * lineH, ls[k].CStr());
									}
							}
							y += bh + gap;
						} else if (m.role == 1) { // assistant : puce + blocs prose/code + actions
							const NkVector<MsgBlock> &blocks = blocksFor[i];
							const float32 bh = blockHeight(blocks);
							const float32 blockH = bh + actRowH;
							const float32 dotY = y + lineH * 0.5f;
							if (prevDotY >= 0.f)
								dl.AddLine({threadX, prevDotY}, {threadX, dotY}, ctx.theme.border, 1.2f);
							prevDotY = dotY;
							if (y + blockH >= body.y && y <= body.y + body.h) {
								dl.AddCircleFilled({threadX, dotY}, ctx.S(3.f), NkColor{88, 209, 143, 255});
								float32 by = y;
								for (usize bi = 0; bi < blocks.Size(); ++bi) {
									if (bi > 0)
										by += blockGap;
									const MsgBlock &blk = blocks[bi];
									if (blk.code) {
										const float32 boxH = blk.lines.Size() * lineH + codePad;
										const NkRect box = {threadX + dotGap, by, threadTextMaxW, boxH};
										dl.AddRectFilled(box, NkColor{32, 36, 44, 255}, ctx.S(5.f));
										dl.AddRect(box, ctx.theme.border, 1.f);
										if (ctx.font && ctx.font->Valid())
											for (usize k = 0; k < blk.lines.Size(); ++k) {
												selDraw(box.x + ctx.S(10.f), by + codePad * 0.5f + k * lineH,
														blk.lines[k].CStr());
												dl.AddText(ctx.font->Face(), ctx.font->TexId(),
														   {box.x + ctx.S(10.f), by + codePad * 0.5f + k * lineH + asc},
														   blk.lines[k].CStr(), NkColor{224, 196, 148, 255});
												fileLink(box.x + ctx.S(10.f), by + codePad * 0.5f + k * lineH,
														 blk.lines[k].CStr());
											}
										by += boxH;
									} else {
										if (ctx.font && ctx.font->Valid())
											for (usize k = 0; k < blk.lines.Size(); ++k) {
												selDraw(threadX + dotGap, by + k * lineH, blk.lines[k].CStr());
												dl.AddText(ctx.font->Face(), ctx.font->TexId(),
														   {threadX + dotGap, by + k * lineH + asc}, blk.lines[k].CStr(),
														   ctx.theme.text);
												fileLink(threadX + dotGap, by + k * lineH, blk.lines[k].CStr());
											}
										by += blk.lines.Size() * lineH;
									}
								}
								// Ligne d'actions : Copier · Insérer dans l'éditeur · Retry (dernier
								// message seulement — regénère en renvoyant le même tour utilisateur).
								const float32 acy = y + bh + actRowH * 0.5f;
								const float32 asz = ctx.S(20.f), agap = ctx.S(4.f);
								float32 ax = threadX + dotGap;
								if (ActionIconBtn(ctx, dl, ax, acy, asz, &DrawCopyIcon, mp, NkT("ai.tip.copy"))) {
									ctx.SetClipboard(m.text.CStr());
									ctx.input.mouseClicked[0] = false;
								}
								ax += asz + agap;
								if (ActionIconBtn(ctx, dl, ax, acy, asz, &DrawInsertIcon, mp, NkT("ai.tip.insert"))) {
									if (mS && mS->HasActive())
										mS->files[mS->active].doc.InsertText(m.text.CStr());
									ctx.input.mouseClicked[0] = false;
								}
								ax += asz + agap;
								if (!mBusy && i == msgs.Size() - 1) { // Retry : uniquement la derniere reponse
									if (ActionIconBtn(ctx, dl, ax, acy, asz, &DrawRetryIcon, mp, NkT("ai.tip.retry"))) {
										doRetry = true; // appliqué APRÈS la boucle (ne pas muter `msgs` ici)
										ctx.input.mouseClicked[0] = false;
									}
								}
							}
							y += blockH + gap;
						} else { // systeme/erreur (role 2) : puce grisee + texte simple (pas de bulle),
								 // + icone OUTIL vectorielle optionnelle (Msg::tool, voir ToolIconFor)
							const NkVector<NkString> &ls = wrapped[i];
							const float32 bh = ls.Size() * lineH;
							const float32 dotY = y + lineH * 0.5f;
							if (prevDotY >= 0.f)
								dl.AddLine({threadX, prevDotY}, {threadX, dotY}, ctx.theme.border, 1.2f);
							prevDotY = dotY;
							if (y + bh >= body.y && y <= body.y + body.h) {
								dl.AddCircleFilled({threadX, dotY}, ctx.S(3.f), ctx.theme.textDisabled);
								const NkComboIconFn toolIcon = ToolIconFor(m.tool);
								const float32 iconW = toolIcon ? lineH * 0.85f : 0.f;
								if (toolIcon)
									toolIcon(dl, {threadX + dotGap + iconW * 0.5f, y + lineH * 0.5f}, iconW * 0.4f,
											ctx.theme.textDisabled);
								if (ctx.font && ctx.font->Valid())
									for (usize k = 0; k < ls.Size(); ++k) {
										const float32 sx = threadX + dotGap + (k == 0 ? iconW + ctx.S(4.f) : 0.f);
										selDraw(sx, y + k * lineH, ls[k].CStr());
										dl.AddText(ctx.font->Face(), ctx.font->TexId(), {sx, y + k * lineH + asc},
												   ls[k].CStr(), ctx.theme.textDisabled);
										fileLink(sx, y + k * lineH, ls[k].CStr());
									}
							}
							y += bh + gap;
						}
					}
					// ── Carte de PERMISSION (protocole can_use_tool) : le CLI est bloque en
					// attente de notre reponse -> icone outil + nom + detail + 2 boutons
					// Autoriser/Refuser (AnswerClaudePermission). Remplace l'indicateur de
					// generation tant qu'elle est affichee (le CLI n'avance plus). ──
					if (permHere) {
						const float32 cardY = y;
						const NkColor kAmber = {230, 160, 60, 255};
						const float32 dotY = cardY + lineH * 0.5f;
						if (prevDotY >= 0.f)
							dl.AddLine({threadX, prevDotY}, {threadX, dotY}, ctx.theme.border, 1.2f);
						dl.AddCircleFilled({threadX, dotY}, ctx.S(3.f), kAmber);
						const NkRect card = {threadX + dotGap, cardY, threadTextMaxW, permCardH};
						dl.AddRectFilled(card, NkColor{46, 38, 26, 255}, ctx.S(6.f));
						dl.AddRect(card, kAmber, 1.2f);
						const NkComboIconFn ti = ToolIconFor(mPermIcon);
						const float32 iw = ctx.S(16.f), ipad = ctx.S(10.f);
						if (ti)
							ti(dl, {card.x + ipad + iw * 0.5f, card.y + ipad + iw * 0.5f}, iw * 0.4f, kAmber);
						if (ctx.font && ctx.font->Valid()) {
							const NkString title = NkPrintf("%s : %s", NkT("ai.perm.title"), mPermTool.CStr());
							dl.AddText(ctx.font->Face(), ctx.font->TexId(),
									   {card.x + ipad + iw + ctx.S(6.f), card.y + ipad + asc - lineH * 0.15f},
									   title.CStr(), ctx.theme.text);
							if (!mPermDetail.Empty())
								dl.AddText(ctx.font->Face(), ctx.font->TexId(),
										   {card.x + ipad + iw + ctx.S(6.f), card.y + ipad + lineH + asc - lineH * 0.15f},
										   mPermDetail.CStr(), ctx.theme.textDisabled,
										   card.w - ipad - iw - ctx.S(6.f) - ipad);
						}
						auto permBtn = [&](float32 bx, float32 bw, const char *label, bool accent,
											const NkColor &fg) -> bool {
							const NkRect r = {bx, card.y + card.h - ctx.S(30.f), bw, ctx.S(22.f)};
							const bool hov = NkGuiRectContains(r, mp);
							dl.AddRectFilled(r, accent ? kAmber : (hov ? ctx.theme.buttonHover : ctx.theme.button),
											 ctx.S(4.f));
							if (ctx.font && ctx.font->Valid()) {
								const float32 tw = ctx.font->MeasureWidth(label);
								dl.AddText(ctx.font->Face(), ctx.font->TexId(),
										   {r.x + (r.w - tw) * 0.5f, r.y + (r.h - lineH) * 0.5f + asc}, label, fg);
							}
							return hov && ctx.input.mouseClicked[0];
						};
						const float32 bw = ctx.S(90.f), bgap = ctx.S(8.f);
						const float32 bx1 = card.x + card.w - ipad - bw;
						const float32 bx0 = bx1 - bgap - bw;
						if (permBtn(bx0, bw, NkT("ai.perm.deny"), false, ctx.theme.text)) {
							AnswerClaudePermission(false);
							ctx.input.mouseClicked[0] = false;
						}
						if (permBtn(bx1, bw, NkT("ai.perm.allow"), true, NkColor{20, 20, 20, 255})) {
							AnswerClaudePermission(true);
							ctx.input.mouseClicked[0] = false;
						}
						y += permCardH + gap;
					}
					// Statut animé pendant la génération : puce + "Thinking… · Nk tokens" (dim)
					// puis "Mustering…" (accent) — façon Claude Code, remplace l'ancien "...".
					else if (typing) {
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
					// Scrollbar STANDARD (NkEditorScrollbar.h) — visible + glissable, jamais de
					// zone de scroll sans elle (cf. regle CLAUDE.md UI/UX). `id` derive du chat
					// actif : chaque chat garde son propre etat de drag independant.
					// TOUJOURS appelee (jamais conditionnee par maxScroll>0) : NkVScrollbar gere
					// deja en interne le cas « rien a faire defiler » (pas de pouce dessine), et
					// SURTOUT relache ctx.activeId (verrou GLOBAL de widget en cours de glissement)
					// inconditionnellement en fin de fonction. Si on saute l'appel pile quand
					// maxScroll retombe a 0 (ex. pendant le streaming d'une reponse, le contenu
					// fluctue), le relachement ne s'execute JAMAIS -> ctx.activeId reste bloque
					// pour TOUTE l'application (clavier ET drag-drop casses partout, pas que le
					// chat) — regression reelle detectee le 18 juil, cause du blocage clavier/DnD
					// global remonte par Rihen.
					{
						const NkRect sbTrack = {body.x + contentW, body.y, sbW, body.h};
						if (NkVScrollbar(ctx, dl, sbTrack, scroll, total, body.h,
										ctx.GetId(NkPrintf("##aiChatSb%d", mActiveChat).CStr())))
							stick = false; // glissement manuel -> ne recolle plus en bas tout seul
					}
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
				// ── Terminal (Bash/PowerShell) : cadre + chevron ">" — icône OUTIL par-message
				// (fil de conversation), PAS d'emoji (pas de couverture NotoSans). ──
				static void DrawTerminalIcon(NkGuiDrawList &dl, const NkVec2 &c, float32 rad, const NkColor &col) {
					const float32 w = rad * 1.7f, h = rad * 1.3f;
					dl.AddRect({c.x - w * 0.5f, c.y - h * 0.5f, w, h}, col, 1.2f);
					const float32 ix = c.x - w * 0.28f, iy = c.y;
					dl.AddLine({ix, iy - rad * 0.32f}, {ix + rad * 0.32f, iy}, col, 1.3f);
					dl.AddLine({ix, iy + rad * 0.32f}, {ix + rad * 0.32f, iy}, col, 1.3f);
				}
				// ── Loupe (Grep/Glob) : cercle (segments, pas de primitive stroke-circle) +
				// manche. Icône OUTIL par-message, même contrainte anti-emoji. ──
				static void DrawSearchIcon(NkGuiDrawList &dl, const NkVec2 &c, float32 rad, const NkColor &col) {
					const float32 r = rad * 0.55f;
					const NkVec2 cc = {c.x - rad * 0.15f, c.y - rad * 0.15f};
					const int32 N = 12;
					NkVec2 prev = {};
					for (int32 i = 0; i <= N; ++i) {
						const float32 a = (float32)i / (float32)N * 6.2832f;
						const NkVec2 p = {cc.x + std::cos(a) * r, cc.y + std::sin(a) * r};
						if (i > 0)
							dl.AddLine(prev, p, col, 1.2f);
						prev = p;
					}
					dl.AddLine({cc.x + r * 0.7f, cc.y + r * 0.7f}, {c.x + rad * 0.75f, c.y + rad * 0.75f}, col, 1.4f);
				}
				// ── Route un ID d'icône OUTIL (Msg::tool) vers son glyphe vectoriel. nullptr si
				// aucune icône (tool==0, messages système ordinaires non liés à un outil). ──
				static NkComboIconFn ToolIconFor(int32 tool) {
					switch (tool) {
						case 1:
							return &DrawFileIcon;
						case 2:
							return &DrawTerminalIcon;
						case 3:
							return &DrawSearchIcon;
						case 4:
							return &DrawGear;
						default:
							return nullptr;
					}
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
					// Hauteur MINIMALE genereuse (3 lignes) même vide — la zone de saisie ne
					// doit pas paraître trop petite au démarrage, façon Claude Code (VSCode).
					if (n < 3)
						n = 3;
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
					// ── Compteur de caracteres ────────────────────────────────────────
					// Le tampon est borne (contrainte du widget) et la coupure etait
					// SILENCIEUSE : on ne s'en apercevait qu'a la reponse de l'IA. Le
					// compteur n'apparait qu'a partir de 60 % pour ne pas encombrer, et
					// vire au rouge une fois plein.
					if (font && font->Valid() && mInput[0] != 0) {
						const usize cap = sizeof(mInput) - 1;
						const usize len = ::strlen(mInput);
						if (len * 5 >= cap * 3) { // >= 60 %
							const NkString lbl = NkPrintf("%llu / %llu", (unsigned long long)len,
														  (unsigned long long)cap); // NkPrintf maison
							const float32 tw = font->MeasureWidth(lbl.CStr());
							dl.AddText(font->Face(), font->TexId(),
									   {field.x + field.w - tw - ctx.S(8.f), field.y + field.h - ctx.S(6.f)},
									   lbl.CStr(),
									   len >= cap ? NkColor{232, 106, 106, 255} : ctx.theme.textDisabled);
						}
					}
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
					mView = 0; // bascule sur Chat : la génération se déroule dans le fil normal
					SendComposed(prompt); // integral : ne transite plus par le brouillon borne
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
					mView = 0;
					// Le prompt embarque le contenu ENTIER du fichier relu : integral,
					// sans passer par le brouillon (qui le coupait en plein code).
					SendComposed(prompt);
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
				// `tip` (optionnel) : infobulle apres ~0,45 s de survol (NkEditorTooltip) —
				// indispensable sur ces boutons icone-seule, sans libelle visible.
				static bool ActionIconBtn(NkGuiContext &ctx, NkGuiDrawList &dl, float32 x, float32 cy, float32 sz,
										  NkComboIconFn iconFn, const NkVec2 &mp, const char *tip = nullptr) {
					const NkRect r = {x, cy - sz * 0.5f, sz, sz};
					const bool hov = NkGuiRectContains(r, mp);
					if (hov)
						dl.AddRectFilled(r, ctx.theme.buttonHover, ctx.S(4.f));
					iconFn(dl, {r.x + sz * 0.5f, cy}, sz * 0.3f, hov ? ctx.theme.text : ctx.theme.textDisabled);
					if (tip)
						editorkit::NkTooltip(ctx, hov, tip);
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
					ctx.PushOcclusion(menu, 50); // routeur d'occlusion : rien ne passe derriere
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
					ctx.PushOcclusion(menu, 50); // routeur d'occlusion : rien ne passe derriere
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
					// ── DOUBLE PROTECTION, comme pour les dialogues modaux ──
					// Les deux mecanismes sont conserves parce qu'ils ne couvrent PAS le meme
					// cas, et que la traversee de clics de ce panneau a deja resiste a chacun
					// pris isolement (bug remonte par Rihen, mecanisme exact jamais identifie).
					//
					// 1) ROUTEUR D'OCCLUSION : la palette est une surface de couche 50 (rect
					//    declare plus bas via PushOcclusion) -> les widgets de couche 0
					//    derriere elle deviennent aveugles sous son rect, quel que soit leur
					//    ordre de dessin. Ne protege que ce qui est SOUS le rect.
					// 2) MODALITE GLOBALE : force ctx.popupDepth > 0 tant que la palette est
					//    ouverte -> bloque aussi ce qui est HORS du rect et ce qui est traite
					//    AVANT ce panneau dans l'ordre de la frame (consommer le clic ici ne
					//    suffisait pas si un autre panneau avait deja reagi plus tot).
					//    Remis a 0 explicitement a la fermeture, plus bas.
					NkGuiContext::NkInputLayerScope _layer(ctx, 50);
					if (ctx.popupDepth == 0)
						ctx.popupDepth = 1;
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
					const char *const *models = mKind == 1 ? ClaudeModelTitles(nModels) : kModels(nModels);
					const char *curModel = models[mModelIdx < nModels ? mModelIdx : 0];
					const NkString effLblStr = NkPrintf("%d/%d", mEffort + 1, (int32)kEffortLevels);
					const char *effLbl = effLblStr.CStr();
					Group groups[6] = {
						{NkT("ai.grp.context"),
						 {{NkString(NkT("ai.act.attach")), 0, nullptr, 0}, {NkString(NkT("ai.act.mention")), 0, nullptr, 1},
						  {NkString(NkT("ai.act.clearconv")), 0, nullptr, 2}, {NkString(NkT("ai.act.rewind")), 0, nullptr, 3},
						  {NkString(NkT("ai.act.copyconv")), 0, nullptr, 5},
						  {NkString(NkT("ai.act.selectall")), 0, nullptr, 6}},
						 6},
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
					ctx.PushOcclusion(menu, 50); // routeur d'occlusion : rien ne passe derriere
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

					// Largeur de contenu REDUITE de la largeur de la scrollbar STANDARD
					// (NkEditorScrollbar.h) — jamais de scrollbar maison redessinee a la main.
					const float32 sbW = NkScrollbarWidth();
					const NkRect listR = {menu.x, menu.y + filterH, w - sbW, listH};
					dl.PushClipRect(listR, true);
					float32 y = listR.y - mActionsScroll;
					int32 clicked = -1;
					auto drawRow = [&](const NkString &label, int32 kind, const char *value, int32 action) {
						const NkRect row = {listR.x + ctx.S(6.f), y, listR.w - ctx.S(12.f), rowH};
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
					// Scrollbar STANDARD (NkEditorScrollbar.h) — jamais de scrollbar maison
					// redessinee a la main (cf. regle CLAUDE.md UI/UX). `listR` reserve deja
					// `sbW` a droite (voir plus haut) : la piste se place juste apres.
					// TOUJOURS appelee, meme raison que le chat principal (cf. commentaire dans
					// DrawMessages) : sinon ctx.activeId peut rester bloque globalement.
					{
						const NkRect track = {listR.x + listR.w, listR.y, sbW, listR.h};
						NkVScrollbar(ctx, dl, track, mActionsScroll, contentH, listH, ctx.GetId("##aiActSb"));
					}

					// Actions REELLES cablees ; les autres affichent un message honnete "a venir".
					if (clicked >= 0) {
						if (clicked == 2) { // Clear conversation
							Msgs().Clear();
							Msgs().PushBack({2, NkString(NkT("ai.hello"))});
						} else if (clicked == 5) { // Copier TOUTE la conversation
							const NkVector<Msg> &msgs = Msgs();
							NkString all;
							for (usize i = 0; i < msgs.Size(); ++i) {
								if (!all.Empty())
									all += "\n\n";
								all += (msgs[i].role == 0) ? "Moi :\n" : (msgs[i].role == 1) ? "Assistant :\n" : "";
								all += msgs[i].text;
							}
							ctx.SetClipboard(all.CStr());
						} else if (clicked == 6) { // Selectionner TOUT le fil (equivalent Ctrl+A sur le fil)
							mSelChat = mActiveChat;
							mSelA = NkVec2{-1.0e8f, -1.0e8f};
							mSelB = NkVec2{1.0e8f, 1.0e8f};
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
						} else if (clicked == 14) { // Compte et utilisation -> popover REEL (rate_limit_event)
							OpenUsagePopover();
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
					// MODAL-LITE (comme NkCtxMenuDraw) : un clic DANS le rect du panneau — qu'il
					// touche une ligne precise ou seulement le fond/padding/titre de groupe — ne
					// doit JAMAIS traverser jusqu'a ce qu'il y a derriere (bug remonte par Rihen).
					// Calcule AVANT de fermer sur clic exterieur (sinon un clic sur une ligne qui
					// ferme le panneau ce meme instant echapperait a la consommation).
					const bool clickedInMenu = ctx.input.mouseClicked[0] && NkGuiRectContains(menu, mp);
					if (ctx.input.mouseClicked[0] && !clickedInMenu && !NkGuiRectContains(mActionsAnchor, mp))
						mActionsOpen = false;
					if (clickedInMenu) {
						ctx.input.mouseClicked[0] = false;
						ctx.input.mouseClicked[1] = false;
					}
					if (!mActionsOpen)
						ctx.popupDepth = 0; // libere la modalite (voir le force en debut de fonction)
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
							// Ouvrir le panneau ne changeait pas le focus TEXTE global
							// (ctx.inputId) : la saisie du chat (toujours "focus") continuait de
							// recevoir chaque caractere tape alors que l'utilisateur croit ecrire
							// dans le filtre du panneau qui vient d'apparaitre par-dessus (bug
							// remonte par Rihen).
							if (mActionsOpen)
								ctx.inputId = NKGUI_ID_NONE;
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
					// Devient un bouton STOP tant qu'une requête est en cours SUR CE chat, pour
					// Claude Code (mKind==1, seul backend qui sait vraiment tuer son process —
					// NkProcess/curl des autres agents n'a pas de Stop(), limite pré-existante).
					// Sans ça, une requête bloquée (panneau caché entre-temps, process qui traîne)
					// laissait l'utilisateur coincé sur "A request is already running." sans recours.
					const bool busyHere = mBusy && mBusyChat == mActiveChat;
					const bool canCancel = busyHere && mKind == 1;
					const NkRect send = {row2.x + row2.w - pad - btn, cy - btn * 0.5f, btn, btn};
					const bool canSend = !mBusy && mInput[0] != 0;
					const bool hov = (canSend || canCancel) && NkGuiRectContains(send, mp);
					const NkColor kRed = {224, 90, 90, 255}, kRedHov = {240, 110, 110, 255};
					dl.AddRectFilled(send, canCancel ? (hov ? kRedHov : kRed) : (canSend ? (hov ? violetHov : violet) : ctx.theme.button),
									ctx.S(8.f));
					const NkColor ac = (canSend || canCancel) ? NkColor{255, 255, 255, 255} : ctx.theme.textDisabled;
					if (canCancel) { // carré plein (glyphe "stop" universel)
						const float32 s = btn * 0.32f, cx = send.x + btn * 0.5f, cyy = send.y + btn * 0.5f;
						dl.AddRectFilled({cx - s * 0.5f, cyy - s * 0.5f, s, s}, ac, ctx.S(2.f));
					} else if (ic && ic->up) {
						dl.AddImage(ic->up, {send.x + ctx.S(7.f), send.y + ctx.S(7.f), btn - ctx.S(14.f), btn - ctx.S(14.f)},
									{0.f, 0.f}, {1.f, 1.f}, ac);
					} else {
						const float32 cx = send.x + btn * 0.5f, cyy = send.y + btn * 0.5f, a = ctx.S(5.f);
						dl.AddLine({cx, cyy + a}, {cx, cyy - a}, ac, 2.f);
						dl.AddLine({cx - a * 0.6f, cyy - a * 0.3f}, {cx, cyy - a}, ac, 2.f);
						dl.AddLine({cx + a * 0.6f, cyy - a * 0.3f}, {cx, cyy - a}, ac, 2.f);
					}
					if (hov && ctx.input.mouseClicked[0]) {
						if (canCancel)
							CancelClaudeCli();
						else if (canSend)
							Send();
					}
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
					ctx.PushOcclusion(menu, 50); // routeur d'occlusion : rien ne passe derriere
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

				// ── Titres/descriptions HUMAINS pour le picker de modèle Claude Code
				// (mKind==1 uniquement), façon VSCode : « Sonnet 5 · Efficient for routine
				// tasks », etc. (capture d'écran de référence fournie par Rihen). Les VALEURS
				// réellement transmises à --model restent celles de kModels() (alias bruts
				// "sonnet"/"opus"/...) — ces deux tableaux ne servent qu'à l'AFFICHAGE. Buffer
				// `static` REMPLI À CHAQUE APPEL (pas figé) : évite le piège de verrouillage de
				// langue déjà rencontré sur ModeOptionsFor/ModeDescFor (NkT() doit être
				// ré-évalué à chaque frame pour suivre un changement de langue à chaud). ──
				const char *const *ClaudeModelTitles(int32 &n) const {
					const char *fresh[5] = {NkT("ai.model.default"), NkT("ai.model.sonnet"), NkT("ai.model.opus"),
											NkT("ai.model.fable"), NkT("ai.model.haiku")};
					static const char *out[5];
					for (int32 i = 0; i < 5; ++i)
						out[i] = fresh[i];
					n = 5;
					return out;
				}
				const char *const *ClaudeModelDescs(int32 &n) const {
					const char *fresh[5] = {NkT("ai.model.default.desc"), NkT("ai.model.sonnet.desc"),
											NkT("ai.model.opus.desc"), NkT("ai.model.fable.desc"),
											NkT("ai.model.haiku.desc")};
					static const char *out[5];
					for (int32 i = 0; i < 5; ++i)
						out[i] = fresh[i];
					n = 5;
					return out;
				}

				// ── Menu du picker de modèle Claude Code (titre + description par option),
				// même structure que DrawModeMenu mais sans le pied Effort (déjà dans le menu
				// Mode). Uniquement utilisé pour mKind==1 (voir dispatch dans OnUI). ──
				void DrawModelMenu(NkGuiContext &ctx, const NkRect &bounds, const NkColor &violet) {
					auto &dl = ctx.DL();
					const NkGuiFont *font = ctx.font;
					const NkVec2 mp = ctx.input.mousePos;
					int32 n = 0;
					const char *const *opts = ClaudeModelTitles(n);
					int32 nd = 0;
					const char *const *descs = ClaudeModelDescs(nd);
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
					const float32 menuH = rowsH + ctx.S(8.f);

					const float32 spaceBelow = (bounds.y + bounds.h) - (mModelAnchor.y + mModelAnchor.h);
					const float32 spaceAbove = mModelAnchor.y - bounds.y;
					const bool openUp = spaceBelow < menuH + ctx.S(4.f) && spaceAbove > spaceBelow;
					NkRect menu = {mModelAnchor.x,
								   openUp ? (mModelAnchor.y - menuH - ctx.S(4.f))
										 : (mModelAnchor.y + mModelAnchor.h + ctx.S(4.f)),
								   w, menuH};
					if (menu.x + menu.w > bounds.x + bounds.w - ctx.S(4.f))
						menu.x = bounds.x + bounds.w - ctx.S(4.f) - menu.w;
					if (menu.x < bounds.x + ctx.S(4.f))
						menu.x = bounds.x + ctx.S(4.f);
					ctx.PushOcclusion(menu, 50); // routeur d'occlusion : rien ne passe derriere
					dl.AddRectFilled(menu, ctx.theme.panel, ctx.S(8.f));
					dl.AddRect(menu, ctx.theme.border, 1.f);

					float32 y = menu.y + ctx.S(4.f);
					for (int32 i = 0; i < n; ++i) {
						const NkVector<NkString> &ls = wrapped[i];
						const float32 rh = titleH + ls.Size() * descLineH + ctx.S(10.f);
						const NkRect row = {menu.x + ctx.S(4.f), y, w - ctx.S(8.f), rh};
						const bool hov = NkGuiRectContains(row, mp);
						const bool sel = (i == mModelIdx);
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
							mModelIdx = i;
							mComboOpen = 0;
							ctx.input.mouseClicked[0] = false;
						}
						y += rh;
					}
					if (ctx.input.mouseClicked[0] && !NkGuiRectContains(menu, mp) && !NkGuiRectContains(mModelAnchor, mp))
						mComboOpen = 0;
				}

				// ── Température/Max tokens sont des paramètres de l'API Messages BRUTE
				// (Assistant général/NkAI, appel curl direct) — le CLI `claude` (Claude Code)
				// n'a PAS d'équivalent documenté pour ces deux réglages : les masquer plutôt
				// que de laisser des contrôles qui ne changeraient RIEN à la requête réelle. ──
				bool ShowTempMaxTokens() const { return mKind != 1; }

				void DrawPropsPopover(NkGuiContext &ctx, const NkColor &violet) {
					(void)violet;
					auto &dl = ctx.DL();
					const NkGuiFont *font = ctx.font;
					const NkVec2 mp = ctx.input.mousePos;
					const float32 w = ctx.S(268.f), it = ctx.ItemHeight();
					const float32 sysH = ctx.S(92.f);
					const bool showTM = ShowTempMaxTokens();
					const float32 mh = ctx.S(10.f) + it + ctx.S(8.f) + (showTM ? (it + ctx.S(10.f)) * 2.f : 0.f) + it +
									   ctx.S(10.f) + it + ctx.S(4.f) + sysH + ctx.S(12.f); // Effort + Systeme (+ Temp/MaxTok)
					NkRect menu = {mGearRect.x + mGearRect.w - w, mGearRect.y + mGearRect.h + ctx.S(4.f), w, mh};
					if (menu.x < ctx.S(4.f))
						menu.x = ctx.S(4.f);
					ctx.PushOcclusion(menu, 50); // routeur d'occlusion : rien ne passe derriere
					dl.AddRectFilled(menu, ctx.theme.panel, ctx.S(8.f));
					dl.AddRect(menu, ctx.theme.border, 1.f);
					auto txt = [&](float32 x, float32 yy, const char *s, const NkColor &c) {
						if (font && font->Valid())
							dl.AddText(font->Face(), font->TexId(), {x, yy + font->Ascent()}, s, c);
					};
					float32 y = menu.y + ctx.S(8.f);
					txt(menu.x + ctx.S(12.f), y, NkT("ai.props"), ctx.theme.text);
					y += it + ctx.S(8.f);
					// Température (slider) — UNIQUEMENT si l'API brute la consomme réellement.
					if (showTM) {
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
					// Effort (raisonnement) — icône + « Effort (Valeur) » + toggle. Pour Claude
					// Code (mKind==1), ceci est REELLEMENT transmis via --effort au CLI.
					{
						DrawEffortToggle(ctx, dl, {menu.x + ctx.S(12.f), y, w - ctx.S(24.f), it}, mEffort);
						y += it + ctx.S(10.f);
					}
					// Max tokens : − valeur + — UNIQUEMENT si l'API brute le consomme réellement.
					if (showTM) {
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
					// Instructions système (multi-ligne, scrollable) — pour Claude Code, transmises
					// via --append-system-prompt (le prompt système NATIF de Claude Code reste
					// actif, on AJOUTE juste ces instructions par-dessus, cf. SendClaudeCli()).
					txt(menu.x + ctx.S(12.f), y, NkT("ai.system"), ctx.theme.textDisabled);
					y += it + ctx.S(4.f);
					const NkRect sys = {menu.x + ctx.S(12.f), y, w - ctx.S(24.f), sysH};
					InputTextMultiline(ctx, "##aiSys", mSystem, sizeof(mSystem), sys, NkGuiInputFlags::None,
									   sizeof(mSystem) - 1, /*wrap=*/true);
					if (ctx.input.mouseClicked[0] && !NkGuiRectContains(menu, mp) && !NkGuiRectContains(mGearRect, mp))
						mPropsOpen = false;
				}

				// Temps relatif depuis MAINTENANT jusqu'a un epoch unix (donnee REELLE
				// resetsAt) — jamais d'horloge murale a formater, correct quel que soit le
				// fuseau du poste. "" si epoch inconnu/passe.
				static NkString RelTimeFromNow(int64 epoch) {
					if (epoch <= 0)
						return NkString();
					const int64 now = static_cast<int64>(std::time(nullptr));
					const int64 secs = epoch > now ? epoch - now : 0;
					if (secs >= 86400)
						return NkPrintf("%dj %dh", (int32)(secs / 86400), (int32)((secs % 86400) / 3600));
					if (secs >= 3600)
						return NkPrintf("%dh %dmin", (int32)(secs / 3600), (int32)((secs % 3600) / 60));
					return NkPrintf("%dmin", (int32)(secs / 60));
				}

				// Libelle humain d'un `rateLimitType` brut ("session", "seven_day",
				// "seven_day_fable"...) — reconnait les motifs vus empiriquement + un nom de
				// modele imbrique (fenetre hebdo PAR MODELE, cf. capture VSCode fournie par
				// Rihen) ; repli HONNETE sur le type brut si rien ne correspond (jamais invente).
				static NkString UsageBucketLabel(const NkString &type) {
					const char *t = type.CStr();
					auto has = [&](const char *k) { return NkFindSub(t, k) != nullptr; };
					if (has("session"))
						return NkString(NkT("ai.usage.bucket.session"));
					const char *mdl = nullptr;
					if (has("fable") || has("mythos"))
						mdl = "Fable";
					else if (has("opus"))
						mdl = "Opus";
					else if (has("sonnet"))
						mdl = "Sonnet";
					else if (has("haiku"))
						mdl = "Haiku";
					if (mdl)
						return NkPrintf("%s %s", NkT("ai.usage.bucket.weekly"), mdl);
					if (has("seven_day") || has("week"))
						return NkString(NkT("ai.usage.bucket.weekly7"));
					return type;
				}

				// ── Popover "Compte et utilisation" (Claude Code, mKind==1) : donnees
				// REELLES du CLI — PLUSIEURS fenetres de quota simultanees (evenements
				// rate_limit_event, upsert par type) + usage/cout PAR MODELE persiste sur
				// disque (survit aux redemarrages, AccumModelUsage). AUCUNE valeur inventee :
				// si rien n'a encore ete recu, le dit explicitement plutot qu'un faux 0%. ──
				void DrawUsagePopover(NkGuiContext &ctx, const NkRect &bounds, const NkColor &violet) {
					(void)violet;
					EnsureUsageLoaded();
					auto &dl = ctx.DL();
					const NkGuiFont *font = ctx.font;
					const NkVec2 mp = ctx.input.mousePos;
					const float32 w = ctx.S(300.f), it = ctx.ItemHeight(), pad = ctx.S(12.f);
					const float32 bucketH = it * 2.f + ctx.S(16.f); // label+% / barre / reinit
					const float32 sepH = ctx.S(18.f);
					const bool hasBuckets = !mRlBuckets.Empty();
					const bool hasModels = !mModelUsage.Empty();
					// Detail "/usage" (capture INTEGRALE, cf. TriggerQuickUsage/PollQuickUsage) :
					// seule source qui rapporte TOUJOURS session + semaine ensemble (bien plus
					// fiable que les buckets rate_limit_event, qui n'en rapportent qu'un a la fois).
					const bool hasQuickUsage = !mQuickUsageText.Empty();
					// Bascule Jour/Semaine (comme la reference VSCode fournie par Rihen) : le
					// texte /usage contient un bloc "Last 24h" et un bloc "Last 7d" distincts —
					// on n'affiche QUE celui selectionne, pas les deux empiles.
					NkString qPreamble, qDayBlock, qWeekBlock;
					NkVector<NkString> quickLines;		 // preambule (session/semaine deja couverts par
														 // les barres ; garde juste l'intro + disclaimer)
					NkVector<NkString> quickPeriodLines; // bloc Jour OU Semaine selon mUsagePeriod
					bool hasPeriodToggle = false;
					if (hasQuickUsage) {
						SplitQuickUsageDetail(qPreamble, qDayBlock, qWeekBlock);
						hasPeriodToggle = !qDayBlock.Empty() && !qWeekBlock.Empty();
						if (!qPreamble.Empty())
							WrapTextLines(font, qPreamble.CStr(), w - pad * 2.f, quickLines);
						const NkString &sel =
							hasPeriodToggle ? (mUsagePeriod == 0 ? qDayBlock : qWeekBlock)
											: (qDayBlock.Empty() ? qWeekBlock : qDayBlock);
						if (!sel.Empty())
							WrapTextLines(font, sel.CStr(), w - pad * 2.f, quickPeriodLines);
					}
					// Section ACCOUNT ("claude auth status --json", cf. PollAccountStatus) :
					// 4 lignes fixes (Auth method/Email/Organisation/Plan) si les identifiants
					// sont bien charges ET valides — sinon 1 ligne d'etat honnete (chargement/
					// non connecte), jamais de champ vide fait passer pour une vraie valeur.
					const bool hasAcct = mAcctLoaded && mAcctLoggedIn;
					// Hauteur : titre + [ACCOUNT] + [detail /usage] + (rien encore ? 1 ligne :
					// [USAGE + N buckets] + [sep + MODELES + M lignes + sep + TOTAL]) + lien
					// "Gerer sur claude.ai" (toujours).
					float32 h = pad * 2.f + it;
					if (mKind == 1)
						h += it + (hasAcct ? it * 4.f : it) + sepH;
					if (hasQuickUsage) {
						h += it + static_cast<float32>(quickLines.Size()) * it;
						if (hasPeriodToggle)
							h += it; // ligne des boutons Jour/Semaine
						h += static_cast<float32>(quickPeriodLines.Size()) * it + sepH;
					}
					if (!hasBuckets && !hasModels && !hasQuickUsage)
						h += it;
					if (hasBuckets)
						h += it + static_cast<float32>(mRlBuckets.Size()) * bucketH;
					if (hasModels) {
						if (hasBuckets)
							h += sepH;
						h += it + static_cast<float32>(mModelUsage.Size()) * it + sepH + it;
					}
					h += it; // lien "Gerer sur claude.ai"
					NkRect menu = {bounds.x + bounds.w - w - ctx.S(8.f), bounds.y + ctx.S(40.f), w, h};
					if (menu.x < ctx.S(4.f))
						menu.x = ctx.S(4.f);
					const float32 vh = static_cast<float32>(ctx.viewH);
					if (menu.y + menu.h > vh)
						menu.y = vh - menu.h - ctx.S(4.f);
					ctx.PushOcclusion(menu, 50); // routeur d'occlusion : rien ne passe derriere
					dl.AddRectFilled(menu, ctx.theme.panel, ctx.S(8.f));
					dl.AddRect(menu, ctx.theme.border, 1.f);
					auto txt = [&](float32 x, float32 yy, const char *s, const NkColor &c, float32 maxW = -1.f) {
						if (font && font->Valid())
							dl.AddText(font->Face(), font->TexId(), {x, yy + font->Ascent()}, s, c, maxW);
					};
					auto sectionHdr = [&](float32 yy, const char *s) {
						txt(menu.x + pad, yy, s, ctx.theme.textDisabled);
					};
					float32 y = menu.y + pad;
					txt(menu.x + pad, y, NkT("ai.usage.title"), ctx.theme.text);
					y += it;
					// ── ACCOUNT (comme la capture VSCode) : Auth method/Email/Organisation/
					// Plan, source = "claude auth status --json" (sous-commande dediee, cf.
					// PollAccountStatus) — jamais lu directement depuis un fichier de secrets. ──
					if (mKind == 1) {
						sectionHdr(y, NkT("ai.usage.section.account"));
						y += it;
						auto row = [&](const char *label, const char *value) {
							txt(menu.x + pad, y, label, ctx.theme.textDisabled);
							const float32 vw = font && font->Valid() ? font->MeasureWidth(value) : 0.f;
							txt(menu.x + w - pad - vw, y, value, ctx.theme.text, w * 0.6f);
							y += it;
						};
						if (!mAcctLoaded) {
							txt(menu.x + pad, y, NkT("ai.usage.acct.loading"), ctx.theme.textDisabled);
							y += it;
						} else if (!mAcctLoggedIn) {
							txt(menu.x + pad, y, NkT("ai.usage.acct.notloggedin"), ctx.theme.textDisabled);
							y += it;
						} else {
							row(NkT("ai.usage.acct.method"), mAcctMethod.Empty() ? "—" : mAcctMethod.CStr());
							row(NkT("ai.usage.acct.email"), mAcctEmail.Empty() ? "—" : mAcctEmail.CStr());
							row(NkT("ai.usage.acct.org"), mAcctOrg.Empty() ? "—" : mAcctOrg.CStr());
							// "pro" -> "Claude Pro" (capitalise) ; valeur brute si non reconnue —
							// jamais invente au-dela d'une simple mise en forme du texte REEL recu.
							NkString planLbl = mAcctPlan;
							if (!planLbl.Empty()) {
								char up0 = planLbl.CStr()[0];
								if (up0 >= 'a' && up0 <= 'z')
									up0 = (char)(up0 - 32);
								NkString cap;
								cap += up0;
								cap += (planLbl.CStr() + 1);
								planLbl = NkString("Claude ") + cap.CStr();
							}
							row(NkT("ai.usage.acct.plan"), planLbl.Empty() ? "—" : planLbl.CStr());
						}
						y += sepH;
					}
					// Detail "/usage" : capture INTEGRALE (session + semaine + repartition), texte
					// libre du CLI reproduit tel quel, jamais reformule/invente. Bascule Jour/
					// Semaine (comme la reference VSCode) : n'affiche que le bloc selectionne.
					if (hasQuickUsage) {
						sectionHdr(y, NkT("ai.usage.section.detail"));
						y += it;
						for (usize i = 0; i < quickLines.Size(); ++i) {
							if (!quickLines[i].Empty())
								txt(menu.x + pad, y, quickLines[i].CStr(), ctx.theme.text, w - pad * 2.f);
							y += it;
						}
						if (hasPeriodToggle) {
							const char *tabs[2] = {NkT("ai.usage.day"), NkT("ai.usage.week")};
							float32 tx = menu.x + pad;
							for (int32 ti = 0; ti < 2; ++ti) {
								const float32 tw =
									(font && font->Valid() ? font->MeasureWidth(tabs[ti]) : 40.f) + ctx.S(16.f);
								const NkRect tr = {tx, y, tw, it - ctx.S(4.f)};
								const bool sel = (mUsagePeriod == ti);
								const bool hov = NkGuiRectContains(tr, mp);
								dl.AddRectFilled(tr, sel ? ctx.theme.buttonActive : (hov ? ctx.theme.buttonHover : ctx.theme.button),
												 ctx.S(4.f));
								if (font && font->Valid())
									txt(tr.x + ctx.S(8.f), tr.y + ctx.S(2.f), tabs[ti], ctx.theme.text);
								if (hov && ctx.input.mouseClicked[0]) {
									mUsagePeriod = ti;
									ctx.input.mouseClicked[0] = false;
								}
								tx += tw + ctx.S(6.f);
							}
							y += it;
						}
						for (usize i = 0; i < quickPeriodLines.Size(); ++i) {
							if (!quickPeriodLines[i].Empty())
								txt(menu.x + pad, y, quickPeriodLines[i].CStr(), ctx.theme.text, w - pad * 2.f);
							y += it;
						}
						y += sepH;
					}
					if (!hasBuckets && !hasModels && !hasQuickUsage) {
						txt(menu.x + pad, y, NkT("ai.usage.none"), ctx.theme.textDisabled, w - pad * 2.f);
						y += it;
					}
					if (hasBuckets) {
						sectionHdr(y, NkT("ai.usage.section.usage"));
						y += it;
						for (usize i = 0; i < mRlBuckets.Size(); ++i) {
							const RlBucket &b = mRlBuckets[i];
							const float32 frac = b.utilization < 0.f ? 0.f : (b.utilization > 1.f ? 1.f : b.utilization);
							// Repli honnete : sans "surpassed" (rate_limit_event uniquement), un seuil
							// simple sur l'utilisation evite qu'un bucket a 90% affiche vert (source
							// /usage, qui ne fournit pas ce flag).
							const NkColor barC = b.utilization >= 1.f					? NkColor{225, 70, 70, 255}
												  : (b.surpassed || b.utilization >= 0.75f) ? NkColor{230, 160, 50, 255}
																						  : NkColor{88, 209, 143, 255};
							const NkString label = UsageBucketLabel(b.type);
							txt(menu.x + pad, y, label.CStr(), ctx.theme.text, w - pad * 2.f - ctx.S(50.f));
							const NkString pct = NkPrintf("%d%%", (int32)(b.utilization * 100.f + 0.5f));
							const float32 pctW = font && font->Valid() ? font->MeasureWidth(pct.CStr()) : 0.f;
							txt(menu.x + w - pad - pctW, y, pct.CStr(), ctx.theme.text);
							y += it;
							const NkRect trk = {menu.x + pad, y + ctx.S(3.f), w - pad * 2.f, ctx.S(6.f)};
							dl.AddRectFilled(trk, ctx.theme.button, ctx.S(3.f));
							dl.AddRectFilled({trk.x, trk.y, trk.w * frac, trk.h}, barC, ctx.S(3.f));
							y += ctx.S(12.f);
							NkString sub = !b.resetsText.Empty()
											   ? NkPrintf("%s %s", NkT("ai.usage.resetsin"), b.resetsText.CStr())
											   : b.resetsAt > 0
													 ? NkPrintf("%s %s", NkT("ai.usage.resetsin"),
																RelTimeFromNow(b.resetsAt).CStr())
													 : NkString();
							if (b.overage)
								sub += sub.Empty() ? NkString(NkT("ai.usage.overage"))
												   : (NkString("  \xC2\xB7  ") + NkT("ai.usage.overage"));
							txt(menu.x + pad, y, sub.CStr(), ctx.theme.textDisabled);
							y += it;
						}
					}
					if (hasModels) {
						if (hasBuckets) {
							dl.AddRectFilled({menu.x + pad, y + ctx.S(6.f), w - pad * 2.f, 1.f}, ctx.theme.border);
							y += sepH;
						}
						sectionHdr(y, NkT("ai.usage.section.models"));
						y += it;
						for (usize i = 0; i < mModelUsage.Size(); ++i) {
							const ModelUsage &mu = mModelUsage[i];
							txt(menu.x + pad, y, mu.model.CStr(), ctx.theme.text, w * 0.5f - pad);
							const NkString c = NkPrintf("$%.4f", mu.costUsd);
							const float32 cw = font && font->Valid() ? font->MeasureWidth(c.CStr()) : 0.f;
							txt(menu.x + w - pad - cw, y, c.CStr(), ctx.theme.text);
							y += it;
						}
						dl.AddRectFilled({menu.x + pad, y + ctx.S(6.f), w - pad * 2.f, 1.f}, ctx.theme.border);
						y += sepH;
						const NkString tot = NkPrintf("$%.4f", TotalUsageCostUsd());
						const float32 tw = font && font->Valid() ? font->MeasureWidth(tot.CStr()) : 0.f;
						txt(menu.x + pad, y, NkT("ai.usage.total"), ctx.theme.text);
						txt(menu.x + w - pad - tw, y, tot.CStr(), ctx.theme.text);
						y += it;
					}
					// Lien REEL (pas une donnee simulee) : ouvre la page officielle d'usage —
					// c'est la que vivent auth/email/organisation/plan, que le protocole CLI
					// stream-json n'expose pas (repli honnete plutot qu'invente).
					{
						const bool hov = mp.y >= y && mp.y < y + it && mp.x >= menu.x + pad && mp.x < menu.x + w - pad;
						txt(menu.x + pad, y, NkT("ai.usage.managelink"), hov ? ctx.theme.accent : ctx.theme.textDisabled);
						if (hov) {
							ctx.wantCursor = NkGuiCursor::Hand;
							if (ctx.input.mouseClicked[0])
								NkLauncher::OpenURL("https://claude.ai/settings/usage");
						}
					}
					// Le clic qui vient d'OUVRIR ce popover (meme frame) ne doit jamais le refermer,
					// quelle que soit sa position — cf. mUsageJustOpened.
					if (ctx.input.mouseClicked[0] && !NkGuiRectContains(menu, mp) && !mUsageJustOpened)
						mUsageOpen = false;
					mUsageJustOpened = false;
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

				// Extrait un champ NUMERIQUE `"key":123.45` (0.0 si absent) — meme recherche
				// maison que JsonStr, pour usage.input_tokens/output_tokens (API brute mKind==0).
				static double JsonNum(const char *json, const char *key) {
					NkString pat = NkString("\"") + key + "\"";
					const char *k = NkFindSub(json, pat.CStr());
					if (!k)
						return 0.0;
					const char *p = k + pat.Size();
					while (*p && *p != ':')
						++p;
					if (*p)
						++p;
					while (*p == ' ' || *p == '\t' || *p == '\n')
						++p;
					bool neg = false;
					if (*p == '-') {
						neg = true;
						++p;
					}
					double v = 0.0;
					while (*p >= '0' && *p <= '9') {
						v = v * 10.0 + (*p - '0');
						++p;
					}
					if (*p == '.') {
						++p;
						double frac = 0.1;
						while (*p >= '0' && *p <= '9') {
							v += (*p - '0') * frac;
							frac *= 0.1;
							++p;
						}
					}
					return neg ? -v : v;
				}

				// Conversion decimale minimale ("12.3456" -> double) — pas de dependance
				// <cstdlib>, coherent avec les autres parseurs "maison" de ce fichier.
				static double AtofSimple(const char *s) {
					if (!s)
						return 0.0;
					bool neg = false;
					if (*s == '-') {
						neg = true;
						++s;
					}
					double v = 0.0;
					while (*s >= '0' && *s <= '9')
						v = v * 10.0 + (*s++ - '0');
					if (*s == '.') {
						++s;
						double frac = 0.1;
						while (*s >= '0' && *s <= '9') {
							v += (*s++ - '0') * frac;
							frac *= 0.1;
						}
					}
					return neg ? -v : v;
				}

				// ── Persistance GLOBALE de l'usage (survit aux redemarrages de NKCode, PAS
				// lie a un workspace : le compte/la cle API sont partages entre projets) —
				// meme convention que TerminalPanel::PrefPath (%APPDATA%/NKCode/*.cfg,
				// format ligne "cle=valeur"). ──
				static NkString UsagePrefPath() {
					const char *base = env::GetEnvVar("APPDATA");
					if (!base)
						base = env::GetEnvVar("HOME");
					if (!base)
						return NkString();
					NkString dir = NkString(base) + "/NKCode";
					NkDirectory::Create(dir.CStr());
					return dir + "/ai_usage.cfg";
				}

				void EnsureUsageLoaded() {
					if (mUsageLoaded)
						return;
					mUsageLoaded = true;
					const NkString p = UsagePrefPath();
					if (p.Empty() || !NkFile::Exists(NkPath(p)))
						return;
					const NkString txt = NkFile::ReadAllText(NkPath(p));
					const char *s = txt.CStr();
					while (*s) {
						const char *e = s;
						while (*e && *e != '\n' && *e != '\r')
							++e;
						if (e - s > 6 && s[0] == 'm' && s[1] == 'o' && s[2] == 'd' && s[3] == 'e' && s[4] == 'l' &&
							s[5] == '=') {
							// "model=<nom>|cost=<f>|tokin=<i>|tokout=<i>|turns=<i>"
							NkString line;
							for (const char *q = s + 6; q < e; ++q)
								line += *q;
							ModelUsage mu;
							usize barPos = line.Find('|');
							mu.model = barPos == NkString::npos ? line : line.SubStr(0, barPos);
							auto field = [&](const char *key) -> NkString {
								const NkString pat = NkString(key) + "=";
								const char *k = NkFindSub(line.CStr(), pat.CStr());
								if (!k)
									return NkString();
								const char *v = k + pat.Size();
								NkString out;
								while (*v && *v != '|')
									out += *v++;
								return out;
							};
							mu.costUsd = AtofSimple(field("cost").CStr());
							mu.tokIn = NkAtoi(field("tokin").CStr());
							mu.tokOut = NkAtoi(field("tokout").CStr());
							mu.turns = NkAtoi(field("turns").CStr());
							if (!mu.model.Empty())
								mModelUsage.PushBack(mu);
						} else if (e - s > 7 && s[0] == 'b' && s[1] == 'u' && s[2] == 'c' && s[3] == 'k' &&
								   s[4] == 'e' && s[5] == 't' && s[6] == '=') {
							// "bucket=<type>|status=<s>|util=<f>|resets=<i64>|overage=<0/1>|surpassed=<0/1>"
							// Repli entre redemarrages : affiche la DERNIERE valeur connue par fenetre de
							// quota tant qu'un `rate_limit_event` plus frais ne l'a pas remplacee ce tour-ci.
							NkString line;
							for (const char *q = s + 7; q < e; ++q)
								line += *q;
							RlBucket b;
							usize barPos2 = line.Find('|');
							b.type = barPos2 == NkString::npos ? line : line.SubStr(0, barPos2);
							auto field2 = [&](const char *key) -> NkString {
								const NkString pat = NkString(key) + "=";
								const char *k = NkFindSub(line.CStr(), pat.CStr());
								if (!k)
									return NkString();
								const char *v = k + pat.Size();
								NkString out;
								while (*v && *v != '|')
									out += *v++;
								return out;
							};
							b.status = field2("status");
							b.utilization = static_cast<float32>(AtofSimple(field2("util").CStr()));
							b.resetsAt = static_cast<int64>(AtofSimple(field2("resets").CStr()));
							b.overage = field2("overage") == "1";
							b.surpassed = field2("surpassed") == "1";
							if (!b.type.Empty())
								mRlBuckets.PushBack(b);
						}
						s = e;
						while (*s == '\n' || *s == '\r')
							++s;
					}
				}

				void SaveUsagePersist() {
					const NkString p = UsagePrefPath();
					if (p.Empty())
						return;
					NkString out;
					for (usize i = 0; i < mModelUsage.Size(); ++i) {
						const ModelUsage &mu = mModelUsage[i];
						out += NkPrintf("model=%s|cost=%.6f|tokin=%lld|tokout=%lld|turns=%d\n", mu.model.CStr(),
										mu.costUsd, (long long)mu.tokIn, (long long)mu.tokOut, mu.turns);
					}
					for (usize i = 0; i < mRlBuckets.Size(); ++i) {
						const RlBucket &b = mRlBuckets[i];
						out += NkPrintf("bucket=%s|status=%s|util=%.6f|resets=%lld|overage=%d|surpassed=%d\n",
										b.type.CStr(), b.status.CStr(), (double)b.utilization, (long long)b.resetsAt,
										b.overage ? 1 : 0, b.surpassed ? 1 : 0);
					}
					NkFile::WriteAllText(NkPath(p), out.CStr());
				}

				// Cumule `cost`/`tokIn`/`tokOut` sous `model` (nouvelle entree si jamais vu),
				// persiste immediatement (perte de donnees nulle meme si NKCode plante).
				void AccumModelUsage(const NkString &model, double cost, int64 tokIn, int64 tokOut) {
					EnsureUsageLoaded();
					const NkString key = model.Empty() ? NkString("?") : model;
					for (usize i = 0; i < mModelUsage.Size(); ++i)
						if (mModelUsage[i].model == key) {
							mModelUsage[i].costUsd += cost;
							mModelUsage[i].tokIn += tokIn;
							mModelUsage[i].tokOut += tokOut;
							mModelUsage[i].turns += 1;
							SaveUsagePersist();
							return;
						}
					ModelUsage mu;
					mu.model = key;
					mu.costUsd = cost;
					mu.tokIn = tokIn;
					mu.tokOut = tokOut;
					mu.turns = 1;
					mModelUsage.PushBack(mu);
					SaveUsagePersist();
				}

				double TotalUsageCostUsd() const {
					double t = 0.0;
					for (usize i = 0; i < mModelUsage.Size(); ++i)
						t += mModelUsage[i].costUsd;
					return t;
				}

				// Tarif PUBLIC ($/1M tokens in,out) — utilise UNIQUEMENT pour l'Assistant
				// general (API brute, mKind==0) qui n'a pas de champ cout dans sa reponse
				// (contrairement au CLI Claude Code, qui renvoie total_cost_usd EXACT). Le
				// popover marque ces couts "estimes", jamais confondus avec les couts exacts.
				static void PublicRateFor(const NkString &model, double &inRate, double &outRate) {
					const char *m = model.CStr();
					if (NkFindSub(m, "fable") || NkFindSub(m, "mythos")) {
						inRate = 10.0;
						outRate = 50.0;
					} else if (NkFindSub(m, "opus")) {
						inRate = 5.0;
						outRate = 25.0;
					} else if (NkFindSub(m, "haiku")) {
						inRate = 1.0;
						outRate = 5.0;
					} else { // sonnet, ou modele inconnu -> repli le plus courant
						inRate = 3.0;
						outRate = 15.0;
					}
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

				// ── Resout le chemin NATIF de claude.exe. Piege Windows : les shims npm
				// "claude"/"claude.cmd" ne sont PAS lancables par CreateProcessW direct
				// (utilisé par NkPipeProc, sans cmd.exe) — CreateProcess n'essaie QUE
				// l'extension .exe implicite, JAMAIS .cmd/.bat/PATHEXT (contrairement à
				// cmd.exe/_popen, qui eux résolvent .cmd). Le paquet npm
				// @anthropic-ai/claude-code fournit un VRAI binaire natif à
				// <dossier-du-shim>\node_modules\@anthropic-ai\claude-code\bin\claude.exe —
				// on le localise UNE FOIS (`where claude.cmd`, qui passe par cmd.exe et
				// résout donc .cmd correctement) et on met en cache (ClaudeExe()). ──
				// Un fichier .exe qui EXISTE n'est pas forcement un executable. Le paquet
				// npm @anthropic-ai/claude-code livre a bin\claude.exe un SCRIPT-GARDE de
				// ~500 octets (« Error: claude native binary not installed. ») quand le
				// postinstall n'a pas tourne (--ignore-scripts, certains pnpm) ou que la
				// dependance optionnelle native n'a pas ete telechargee (--omit=optional).
				// Le donner a CreateProcessW declenche une boite MODALE Windows
				// « Application 16 bits non prise en charge » — bloquante, et au demarrage
				// (deux sondes automatiques : auth status + usage). On exige donc la
				// signature PE « MZ » avant d'accepter un chemin.
				static bool IsNativeExe(const NkString &path) {
					if (path.Empty() || !NkFile::Exists(path.CStr()))
						return false;
					FILE *f = std::fopen(path.CStr(), "rb");
					if (!f)
						return false;
					char sig[2] = {0, 0};
					const usize n = std::fread(sig, 1, 2, f);
					std::fclose(f);
					return n == 2 && sig[0] == 'M' && sig[1] == 'Z';
				}

				static NkString ResolveClaudeExe() {
					const char *appData = env::GetEnvVar("APPDATA");
					if (appData && *appData) {
						const NkString candidate =
							NkString(appData) + "\\npm\\node_modules\\@anthropic-ai\\claude-code\\bin\\claude.exe";
						if (IsNativeExe(candidate))
							return candidate;
					}
#ifdef _WIN32
					FILE *pipe = _popen("where claude.cmd 2>nul", "r");
#else
					FILE *pipe = popen("which claude 2>/dev/null", "r");
#endif
					if (pipe) {
						char line[512];
						NkString shimPath;
						if (std::fgets(line, sizeof(line), pipe)) {
							for (char *q = line; *q; ++q)
								if (*q == '\n' || *q == '\r') {
									*q = 0;
									break;
								}
							shimPath = NkString(line);
						}
#ifdef _WIN32
						_pclose(pipe);
#else
						pclose(pipe);
#endif
						if (!shimPath.Empty()) {
							const usize slash = shimPath.RFind('\\');
							if (slash != NkString::npos) {
								const NkString dir = shimPath.SubStr(0, slash);
								const NkString candidate =
									dir + "\\node_modules\\@anthropic-ai\\claude-code\\bin\\claude.exe";
								if (IsNativeExe(candidate))
									return candidate;
							}
						}
					}
#ifdef _WIN32
					// Aucun binaire natif VALIDE : renvoyer « claude » ferait retomber
					// CreateProcessW sur le script-garde. Chaine vide = « CLI absent »,
					// deja gere par les appelants (sondes auto ignorees, message clair
					// dans le panneau). L'utilisateur doit terminer l'installation :
					//   node node_modules/@anthropic-ai/claude-code/install.cjs
					return NkString();
#else
					return NkString("claude"); // Unix : pas de piege .exe, le shim est executable
#endif
				}
				const NkString &ClaudeExe() {
					if (!mClaudeExeResolved) {
						mClaudeExePath = ResolveClaudeExe();
						mClaudeExeResolved = true;
					}
					return mClaudeExePath;
				}

				// ── Backend REEL pour l'agent Claude Code (mKind==1) : spawn le CLI `claude`
				// en sous-processus (NkPipeProc, mêmes pipes que le client LSP) au lieu du
				// curl+API brut des autres agents. Récupère GRATUITEMENT : mémoire
				// (CLAUDE.md/.claude auto-chargés par le CLI), outils réels (Read/Write/
				// Edit/Bash selon --permission-mode), session persistée sur disque
				// (--resume <session_id>). Voir NkWin32QuoteArg : le prompt utilisateur
				// (texte libre, non fiable) ne doit JAMAIS être interpolé dans une chaîne de
				// commande sans échappement — CreateProcessW n'a pas de shell pour absorber
				// ça, contrairement à _popen/cmd.exe. ──
				void SendClaudeCli() {
					// Aucun binaire natif VALIDE (cf. ResolveClaudeExe) : on le dit, au lieu
					// de tenter un spawn avec un argv[0] vide.
					if (ClaudeExe().Empty()) {
						Msgs().PushBack({2, NkString(NkT("ai.claude.launchfail"))});
						return;
					}
					mClaudeCurModel = NkString(); // capture du modele REEL de ce tour (voir "assistant" plus bas)
					// Portée (Scope) + chip contexte fichier : préfixe le prompt d'un rappel du
					// contexte actif — l'agent a de VRAIS outils Read donc peut déjà tout
					// explorer seul, mais lui rappeler le fichier/la sélection ACTUELLEMENT
					// visible dans NKCode évite qu'il parte à l'aveugle chercher où regarder.
					NkString contextPrefix;
					if (mCtxFileOn && mS && mS->HasActive()) {
						OpenFile &f = mS->files[mS->active];
						contextPrefix +=
							NkPrintf("[Contexte NKCode : fichier actif %s, ligne %d]\n", f.Name().CStr(), f.doc.curLine + 1);
					}
					if (mScope == 1 && mS && mS->HasActive() && mS->files[mS->active].doc.HasSel()) {
						OpenFile &f = mS->files[mS->active];
						contextPrefix += NkString("[Sélection concernée]\n```\n") + f.doc.GetSelectedText().CStr() + "\n```\n";
					}
					// ── MEMOIRE GLOBALE DES INTERACTIONS IDE : le JOURNAL (commandes, resultats,
					// executions, sauvegardes, changements de projet...) part avec CHAQUE requete
					// (compact, ~25 lignes) -> l'agent sait « ce qui vient de se passer ». ──
					if (mS && !mS->ideJournal.Empty()) {
						contextPrefix += "[Journal IDE recent — du plus ancien au plus recent]\n";
						const usize jn = mS->ideJournal.Size();
						const usize js = jn > 25 ? jn - 25 : 0;
						for (usize i = js; i < jn; ++i) {
							contextPrefix += "- ";
							contextPrefix += mS->ideJournal[i];
							contextPrefix += "\n";
						}
					}
					// ── En complement : si le prompt parle de build/erreur/compilation ET
					// qu'une compilation a ECHOUE, on attache le TRANSCRIPT de ce dernier echec
					// (NkCodeState::lastBuildFail, aussi persiste dans .nkcode/
					// last_build_fail.log) -> « corrige la derniere erreur de build »
					// fonctionne sans que l'agent ait a chercher ou regarder. ──
					if (mS && !mS->lastBuildFail.Empty()) {
						auto containsI = [](const char *hay, const char *needle) {
							return NkCodeState::ContainsI(hay, needle);
						};
						const NkString qs = OutText(); // brouillon OU prompt compose
						const char *q = qs.CStr();
						const bool wants = containsI(q, "build") || containsI(q, "compil") || containsI(q, "erreur") ||
										   containsI(q, "error") || containsI(q, "link") || containsI(q, "warning");
						if (wants) {
							contextPrefix += NkPrintf("[Dernière compilation ÉCHOUÉE — commande : %s]\n```\n",
													  mS->lastBuildFailCmd.CStr());
							// Cap : dernieres ~150 lignes (les erreurs sont a la fin du transcript).
							const usize n = mS->lastBuildFail.Size();
							const usize start = n > 150 ? n - 150 : 0;
							for (usize i = start; i < n; ++i) {
								contextPrefix += mS->lastBuildFail[i];
								contextPrefix += "\n";
							}
							contextPrefix += "```\n";
						}
					}

					const NkString sent = OutText();		   // brouillon tape OU prompt compose (integral)
					Msgs().PushBack({0, sent});				   // bulle utilisateur : le texte TEL QUE TAPE (sans prefixe)
					const NkString prompt = contextPrefix + sent.CStr(); // le prefixe part AVEC la requete au CLI
					ClearOut();
					mChats[static_cast<usize>(mActiveChat)].stick = true;
					mBusyChat = mActiveChat;
					mClaudeStarted = false;
					mClaudeMsgIdx = -1;
					mClaudeThinkStarted = false;
					mClaudeThinkIdx = -1;
					mClaudeBuf.Clear();

					// permission-mode : mappe le combo Mode existant (Manuel/Auto-Edit/Plan/
					// Auto). « Manuel » = "default" (le CLI demande CHAQUE outil) — couple au
					// protocole de controle stdio ci-dessous, il devient REELLEMENT interactif :
					// chaque demande arrive en `control_request { can_use_tool }` et NKCode
					// affiche une carte Autoriser/Refuser (verifie empiriquement le 20 juil :
					// allow -> l'outil s'execute, fichier cree). Les autres modes profitent du
					// meme canal pour leurs demandes residuelles (ex. Bash en Auto-Edit).
					static const char *const kPermModes[4] = {"default", "acceptEdits", "plan", "auto"};
					const int32 permIdx = (mMode >= 0 && mMode < 4) ? mMode : 0;

					// Clé API PAR-PROJET (optionnelle) : si `.nkcode/ai_claude_key.txt` existe
					// dans le workspace, --bare + ANTHROPIC_API_KEY DÉDIÉ (n'affecte PAS la
					// session OAuth partagée ni les autres projets — chaque appel construit
					// son propre bloc d'environnement, cf. NkPipeProc::StartWithEnv). Sinon :
					// session OAuth globale (claude auth login), comportement par défaut.
					NkString projectKey;
					if (mS && mS->HasWorkspace()) {
						const NkString keyPath = (mS->root / ".nkcode" / "ai_claude_key.txt").ToString();
						if (NkFile::Exists(keyPath.CStr()))
							projectKey = NkFile::ReadAllText(keyPath.CStr()).Trim();
					}

					NkVector<NkString> argv;
					argv.PushBack(ClaudeExe()); // chemin NATIF resolu (voir ResolveClaudeExe) -- PAS le shim .cmd
					argv.PushBack(NkString("-p"));
					argv.PushBack(NkString("--input-format")); // prompt via STDIN (NDJSON) : le canal
					argv.PushBack(NkString("stream-json"));	   // reste OUVERT pour repondre aux
					argv.PushBack(NkString("--output-format")); // demandes de permission du CLI
					argv.PushBack(NkString("stream-json"));
					argv.PushBack(NkString("--verbose"));
					argv.PushBack(NkString("--include-partial-messages"));
					argv.PushBack(NkString("--permission-mode"));
					argv.PushBack(NkString(kPermModes[permIdx]));
					// Protocole de permission INTERACTIF : les demandes d'outils arrivent en
					// `control_request { subtype:"can_use_tool", tool_name, input }` sur stdout
					// et attendent notre `control_response { behavior:"allow"|"deny" }` sur
					// stdin (verifie empiriquement, CLI v2.1.212).
					argv.PushBack(NkString("--permission-prompt-tool"));
					argv.PushBack(NkString("stdio"));
					// --model : alias reel du combo Modele ("auto" = ne passe PAS le flag, laisse
					// le CLI decider — evite de figer un choix par defaut qu'on ne controle pas).
					{
						int32 nModels = 0;
						const char *const *models = kModels(nModels);
						const char *mdl = (mModelIdx >= 0 && mModelIdx < nModels) ? models[mModelIdx] : "auto";
						if (mdl && NkString(mdl) != "auto") {
							argv.PushBack(NkString("--model"));
							argv.PushBack(NkString(mdl));
						}
					}
					// --effort : mappe les 6 niveaux de l'UI (kEffortLevels) sur les 5 valeurs
					// REELLES du CLI ("low","medium","high","xhigh","max" — verifie via --help).
					// Bucketing forcement approximatif (6 -> 5) : les deux niveaux les plus bas
					// partagent "low" plutot que d'inventer un 6e palier cote CLI.
					{
						static const char *const kCliEffort[6] = {"low", "low", "medium", "high", "xhigh", "max"};
						const int32 effIdx = (mEffort >= 0 && mEffort < 6) ? mEffort : 2;
						argv.PushBack(NkString("--effort"));
						argv.PushBack(NkString(kCliEffort[effIdx]));
					}
					// Instructions systeme (gear popover) : AJOUTEES au prompt systeme natif de
					// Claude Code (pas --system-prompt, qui le REMPLACERAIT entierement et
					// perdrait le comportement agentique par defaut).
					if (mSystem[0]) {
						argv.PushBack(NkString("--append-system-prompt"));
						argv.PushBack(NkString(mSystem));
					}
					// --bare desactive CLAUDE.md/memoire auto (voulu pour l'ISOLATION de la cle
					// API) — mais on veut GARDER la memoire projet malgre tout (meme systeme
					// que VSCode : CLAUDE.md + memoire auto qui s'enrichit avec le temps) : on
					// la restaure explicitement via --add-dir, comme documente par `--bare`
					// lui-meme ("Explicitly provide context via: ... --add-dir (CLAUDE.md dirs)").
					if (!projectKey.Empty()) {
						argv.PushBack(NkString("--bare"));
						if (mS && mS->HasWorkspace()) {
							argv.PushBack(NkString("--add-dir"));
							argv.PushBack(mS->root.ToString());
						}
					}
					const NkString &sid = mChats[static_cast<usize>(mActiveChat)].claudeSessionId;
					if (!sid.Empty()) {
						argv.PushBack(NkString("--resume"));
						argv.PushBack(sid);
					}
					// (plus d'argument [prompt] : il part via STDIN en NDJSON, cf. ci-dessous)

					NkString cmd;
					for (usize i = 0; i < argv.Size(); ++i) {
						if (i > 0)
							cmd += " ";
						cmd += NkWin32QuoteArg(argv[i].CStr());
					}

					NkVector<NkString> envOverrides;
					if (!projectKey.Empty())
						envOverrides.PushBack(NkString("ANTHROPIC_API_KEY=") + projectKey.CStr());

					const NkString cwd = (mS && mS->HasWorkspace()) ? mS->root.ToString() : NkString(".");
					if (!mClaudeProc.StartWithEnv(cmd, cwd, envOverrides, /*mergeStderr=*/true)) {
						Msgs().PushBack({2, NkString(NkT("ai.claude.launchfail"))});
						return;
					}
					// Prompt -> stdin (une ligne NDJSON). Le canal stdin RESTE ouvert tout le
					// tour pour pouvoir repondre aux demandes de permission (control_response).
					{
						NkString um = "{\"type\":\"user\",\"message\":{\"role\":\"user\",\"content\":[{\"type\":"
									  "\"text\",\"text\":\"";
						um += NkJsonEscape(prompt.CStr());
						um += "\"}]}}\n";
						mClaudeProc.WriteData(um.CStr(), static_cast<int32>(um.Size()));
					}
					mBusy = true;
				}

				// Echappe `s` pour l'inserer dans une chaine JSON (guillemets, antislashs,
				// sauts de ligne, caracteres de controle -> \uXXXX). L'UTF-8 passe tel quel.
				static NkString NkJsonEscape(const char *s) {
					NkString out;
					for (const char *p = s; *p; ++p) {
						const unsigned char c = (unsigned char)*p;
						if (c == '"')
							out += "\\\"";
						else if (c == '\\')
							out += "\\\\";
						else if (c == '\n')
							out += "\\n";
						else if (c == '\r')
							out += "\\r";
						else if (c == '\t')
							out += "\\t";
						else if (c < 0x20)
							out += NkPrintf("\\u%04x", (int32)c);
						else
							out += (char)c;
					}
					return out;
				}

				// Extrait le SOUS-OBJET JSON brut `"key":{...}` d'une ligne NDJSON (scanner
				// d'accolades conscient des chaines/echappements). Retourne "{}" si absent —
				// sert a renvoyer `updatedInput` tel quel dans la reponse de permission sans
				// re-serialiser (NkJsonDoc est un lecteur, pas un ecrivain).
				static NkString NkJsonRawObject(const char *line, const char *key) {
					const char *k = line;
					const usize kl = [&] {
						usize n = 0;
						while (key[n])
							++n;
						return n;
					}();
					while ((k = NkFindSub(k, key)) != nullptr) {
						const char *p = k + kl;
						while (*p == ' ' || *p == ':')
							++p;
						if (*p == '{') {
							int32 depth = 0;
							bool inStr = false;
							const char *q = p;
							for (; *q; ++q) {
								if (inStr) {
									if (*q == '\\' && q[1])
										++q;
									else if (*q == '"')
										inStr = false;
								} else if (*q == '"')
									inStr = true;
								else if (*q == '{')
									++depth;
								else if (*q == '}') {
									if (--depth == 0) {
										NkString out;
										for (const char *r = p; r <= q; ++r)
											out += *r;
										return out;
									}
								}
							}
							break; // objet non ferme : ligne corrompue
						}
						k += kl;
					}
					return NkString("{}");
				}

				// ── Traite UNE ligne NDJSON du flux stream-json (un événement complet par
				// ligne). V1 volontairement MINIMAL et honnête : capture le session_id
				// (system/init) + accumule le texte (stream_event/text_delta) + finalise sur
				// `result` (texte AUTORITAIRE final, remplace l'accumulation partielle si
				// elle divergeait). Les événements tool_use/tool_result ne sont PAS encore
				// affichés (pas de fausse carte d'outil qui ne ferait rien) — prochain
				// incrément à faire, noté en mémoire projet. ──
				void HandleClaudeLine(const NkString &line, NkVector<Msg> &msgs, ChatSession &chat) {
					NkJsonDoc doc;
					if (!doc.Parse(line.CStr(), static_cast<int32>(line.Size())))
						return;
					const NkJsonVal *root = doc.Root();
					const NkJsonVal *typeV = doc.Member(root, "type");
					if (!typeV || typeV->kind != 3)
						return;
					if (typeV->str == "system") {
						const NkJsonVal *subV = doc.Member(root, "subtype");
						if (subV && subV->kind == 3 && subV->str == "init") {
							const NkJsonVal *sidV = doc.Member(root, "session_id");
							if (sidV && sidV->kind == 3)
								chat.claudeSessionId = sidV->str;
						}
					} else if (typeV->str == "rate_limit_event") {
						// ── UTILISATION REELLE (protocole du CLI, verifie empiriquement le 20
						// juil) : {"rate_limit_info":{"status","utilization","resetsAt",
						// "rateLimitType","isUsingOverage","surpassedThreshold"}}. PLUSIEURS
						// fenetres coexistent (session/hebdo/par-modele, cf. capture VSCode
						// fournie par Rihen) -> upsert par `rateLimitType`, jamais un remplace
						// tous les autres. Alimente le popover — AUCUNE valeur inventee. ──
						const NkJsonVal *infoV = doc.Member(root, "rate_limit_info");
						if (infoV) {
							EnsureUsageLoaded(); // fusionne les fenetres deja connues (autres sessions)
													  // avant d'upserter celle-ci
							const NkJsonVal *stV = doc.Member(infoV, "status");
							const NkJsonVal *utV = doc.Member(infoV, "utilization");
							const NkJsonVal *raV = doc.Member(infoV, "resetsAt");
							const NkJsonVal *rtV = doc.Member(infoV, "rateLimitType");
							const NkJsonVal *ovV = doc.Member(infoV, "isUsingOverage");
							const NkJsonVal *spV = doc.Member(infoV, "surpassedThreshold");
							RlBucket b;
							b.type = (rtV && rtV->kind == 3) ? rtV->str : NkString("?");
							b.status = (stV && stV->kind == 3) ? stV->str : NkString();
							b.utilization = (utV && utV->kind == 2) ? (float32)utV->num : 0.f;
							b.resetsAt = (raV && raV->kind == 2) ? (int64)raV->num : 0;
							b.overage = ovV && ovV->kind == 1 && ovV->b;
							b.surpassed = spV && spV->kind == 1 && spV->b;
							bool found = false;
							for (usize i = 0; i < mRlBuckets.Size() && !found; ++i)
								if (mRlBuckets[i].type == b.type) {
									mRlBuckets[i] = b;
									found = true;
								}
							if (!found)
								mRlBuckets.PushBack(b);
							SaveUsagePersist(); // survit aux redemarrages (toutes les fenetres vues a ce jour)
						}
					} else if (typeV->str == "stream_event") {
						const NkJsonVal *eventV = doc.Member(root, "event");
						const NkJsonVal *deltaV = eventV ? doc.Member(eventV, "delta") : nullptr;
						const NkJsonVal *dtypeV = deltaV ? doc.Member(deltaV, "type") : nullptr;
						if (dtypeV && dtypeV->kind == 3 && dtypeV->str == "text_delta") {
							const NkJsonVal *textV = doc.Member(deltaV, "text");
							if (textV && textV->kind == 3) {
								// index STABLE (pas "le dernier message") : un tool_use peut avoir
								// poussé un message-statut ENTRE deux deltas de texte.
								if (!mClaudeStarted) {
									msgs.PushBack({1, NkString()});
									mClaudeMsgIdx = static_cast<int32>(msgs.Size()) - 1;
									mClaudeStarted = true;
								}
								msgs[static_cast<usize>(mClaudeMsgIdx)].text += textV->str;
							}
						} else if (mThinking && dtypeV && dtypeV->kind == 3 && dtypeV->str == "thinking_delta") {
							// Réflexion (extended thinking) : SEULEMENT si l'utilisateur l'a demandé
							// (mThinking) — accumulée dans un message dédié, distinct de la réponse
							// finale, façon Claude Code (bloc "Réflexion" séparé).
							const NkJsonVal *thinkV = doc.Member(deltaV, "thinking");
							if (thinkV && thinkV->kind == 3) {
								if (!mClaudeThinkStarted) {
									msgs.PushBack({2, NkString(NkT("ai.thinking")) + " :\n"});
									mClaudeThinkIdx = static_cast<int32>(msgs.Size()) - 1;
									mClaudeThinkStarted = true;
								}
								msgs[static_cast<usize>(mClaudeThinkIdx)].text += thinkV->str;
							}
						}
					} else if (typeV->str == "assistant") {
						// Visibilité des ACTIONS de l'agent (Read/Write/Edit/Bash/Grep/Glob/...) :
						// chaque tool_use devient une ligne-statut discrète (role système, pas de
						// bouton Copier/Insérer dessus — ce n'est pas une réponse). PAS d'emoji
						// (la police NotoSans du projet n'a pas de couverture emoji -> tofu/« ? »)
						// — texte brut uniquement, façon "[Outil] détail". Cherche le champ le
						// plus parlant selon l'outil : file_path (Read/Write/Edit), command
						// (Bash), pattern (Grep/Glob).
						const NkJsonVal *messageV = doc.Member(root, "message");
						// Modele REEL de ce tour (utile quand --model est "auto" : c'est le CLI qui
						// choisit) — chaque chunk "assistant" le porte, capture des le premier vu.
						const NkJsonVal *modelV = messageV ? doc.Member(messageV, "model") : nullptr;
						if (modelV && modelV->kind == 3)
							mClaudeCurModel = modelV->str;
						const NkJsonVal *contentV = messageV ? doc.Member(messageV, "content") : nullptr;
						const int32 cn = contentV ? doc.Count(contentV) : 0;
						for (int32 ci = 0; ci < cn; ++ci) {
							const NkJsonVal *item = doc.At(contentV, ci);
							const NkJsonVal *itypeV = item ? doc.Member(item, "type") : nullptr;
							if (!itypeV || itypeV->kind != 3 || itypeV->str != "tool_use")
								continue;
							const NkJsonVal *nameV = doc.Member(item, "name");
							if (!nameV || nameV->kind != 3)
								continue;
							const NkString &toolName = nameV->str;
							int32 toolIcon = 4; // generique par defaut
							if (toolName == "Read" || toolName == "Write" || toolName == "Edit" ||
								toolName == "NotebookEdit")
								toolIcon = 1;
							else if (toolName == "Bash" || toolName == "PowerShell")
								toolIcon = 2;
							else if (toolName == "Grep" || toolName == "Glob")
								toolIcon = 3;
							NkString label = NkPrintf("[%s]", toolName.CStr());
							const NkJsonVal *inputV = doc.Member(item, "input");
							const NkJsonVal *pathV = inputV ? doc.Member(inputV, "file_path") : nullptr;
							const NkJsonVal *cmdV = inputV ? doc.Member(inputV, "command") : nullptr;
							const NkJsonVal *patV = inputV ? doc.Member(inputV, "pattern") : nullptr;
							if (pathV && pathV->kind == 3)
								label += NkString(" ") + pathV->str;
							else if (cmdV && cmdV->kind == 3)
								label += NkString(" ") + cmdV->str;
							else if (patV && patV->kind == 3)
								label += NkString(" ") + patV->str;
							msgs.PushBack({2, label, toolIcon});
						}
					} else if (typeV->str == "control_request") {
						// ── DEMANDE DE PERMISSION (can_use_tool) : le CLI est BLOQUE jusqu'a
						// notre control_response -> memorise la demande, la carte Autoriser/
						// Refuser est rendue par DrawMessages, les boutons repondent via
						// AnswerClaudePermission(). ──
						const NkJsonVal *reqV = doc.Member(root, "request");
						const NkJsonVal *subV = reqV ? doc.Member(reqV, "subtype") : nullptr;
						if (subV && subV->kind == 3 && subV->str == "can_use_tool") {
							const NkJsonVal *ridV = doc.Member(root, "request_id");
							const NkJsonVal *toolV = doc.Member(reqV, "tool_name");
							mPermReqId = (ridV && ridV->kind == 3) ? ridV->str : NkString();
							mPermTool = (toolV && toolV->kind == 3) ? toolV->str : NkString("?");
							mPermInputRaw = NkJsonRawObject(line.CStr(), "\"input\"");
							mPermIcon = 4;
							if (mPermTool == "Read" || mPermTool == "Write" || mPermTool == "Edit" ||
								mPermTool == "NotebookEdit")
								mPermIcon = 1;
							else if (mPermTool == "Bash" || mPermTool == "PowerShell")
								mPermIcon = 2;
							else if (mPermTool == "Grep" || mPermTool == "Glob")
								mPermIcon = 3;
							mPermDetail = NkString();
							const NkJsonVal *inputV = doc.Member(reqV, "input");
							const NkJsonVal *pathV = inputV ? doc.Member(inputV, "file_path") : nullptr;
							const NkJsonVal *cmdV = inputV ? doc.Member(inputV, "command") : nullptr;
							const NkJsonVal *patV = inputV ? doc.Member(inputV, "pattern") : nullptr;
							if (pathV && pathV->kind == 3)
								mPermDetail = pathV->str;
							else if (cmdV && cmdV->kind == 3)
								mPermDetail = cmdV->str;
							else if (patV && patV->kind == 3)
								mPermDetail = patV->str;
							mPermPending = !mPermReqId.Empty();
						}
					} else if (typeV->str == "result") {
						const NkJsonVal *resV = doc.Member(root, "result");
						if (resV && resV->kind == 3) {
							if (!mClaudeStarted) {
								msgs.PushBack({1, resV->str});
								mClaudeMsgIdx = static_cast<int32>(msgs.Size()) - 1;
								mClaudeStarted = true;
							} else {
								msgs[static_cast<usize>(mClaudeMsgIdx)].text = resV->str; // version FINALE autoritaire
							}
						}
						// Cout REEL de ce tour (total_cost_usd, EXACT — fourni par le CLI, pas
						// estime) + tokens (usage.*) : cumule PAR MODELE (mClaudeCurModel, capture
						// depuis "assistant" plus haut) + persiste sur disque (AccumModelUsage).
						{
							double cost = 0.0;
							int64 tokIn = 0, tokOut = 0;
							const NkJsonVal *costV = doc.Member(root, "total_cost_usd");
							if (costV && costV->kind == 2)
								cost = costV->num;
							const NkJsonVal *usageV = doc.Member(root, "usage");
							const NkJsonVal *inV = usageV ? doc.Member(usageV, "input_tokens") : nullptr;
							const NkJsonVal *outV = usageV ? doc.Member(usageV, "output_tokens") : nullptr;
							if (inV && inV->kind == 2)
								tokIn = (int64)inV->num;
							if (outV && outV->kind == 2)
								tokOut = (int64)outV->num;
							mLastCostUsd = cost;
							mLastTokIn = (int32)tokIn;
							mLastTokOut = (int32)tokOut;
							if (cost > 0.0 || tokIn > 0 || tokOut > 0)
								AccumModelUsage(mClaudeCurModel.Empty() ? NkString("claude-code") : mClaudeCurModel,
												cost, tokIn, tokOut);
						}
						// Tour termine : on ferme le process NOUS-MEMES (stdin reste sinon ouvert
						// et le CLI attendrait d'autres messages) — le tour suivant repart via
						// --resume, comportement inchange.
						mClaudeProc.Stop();
						mPermPending = false;
					}
					chat.stick = true;
				}

				// Repond a la demande de permission en attente (boutons de la carte du fil).
				// allow -> renvoie l'input INCHANGE (updatedInput) ; deny -> message court.
				void AnswerClaudePermission(bool allow) {
					if (!mPermPending || mPermReqId.Empty())
						return;
					NkString resp = "{\"type\":\"control_response\",\"response\":{\"subtype\":\"success\","
									"\"request_id\":\"";
					resp += NkJsonEscape(mPermReqId.CStr());
					resp += "\",\"response\":";
					if (allow) {
						resp += "{\"behavior\":\"allow\",\"updatedInput\":";
						resp += mPermInputRaw.Empty() ? NkString("{}") : mPermInputRaw;
						resp += "}";
					} else {
						resp += "{\"behavior\":\"deny\",\"message\":\"";
						resp += NkJsonEscape(NkT("ai.perm.denymsg"));
						resp += "\"}";
					}
					resp += "}}\n";
					mClaudeProc.WriteData(resp.CStr(), static_cast<int32>(resp.Size()));
					const int32 idx =
						(mBusyChat >= 0 && mBusyChat < static_cast<int32>(mChats.Size())) ? mBusyChat : mActiveChat;
					MsgsOf(idx).PushBack({2, NkPrintf("%s : [%s] %s",
													  allow ? NkT("ai.perm.allowed") : NkT("ai.perm.denied"),
													  mPermTool.CStr(), mPermDetail.CStr()),
										  mPermIcon});
					mPermPending = false;
				}

				// ── Draine le pipe du CLI `claude` (non-bloquant), découpe le NDJSON en
				// lignes COMPLETES (une ligne partielle reste dans mClaudeBuf pour la
				// prochaine frame — jamais de JSON tronqué traité comme complet). ──
				void PollClaudeCli() {
					char buf[4096];
					int32 n;
					while ((n = mClaudeProc.ReadAvail(buf, sizeof(buf))) > 0)
						mClaudeBuf.Append(buf, static_cast<usize>(n));

					const int32 idx =
						(mBusyChat >= 0 && mBusyChat < static_cast<int32>(mChats.Size())) ? mBusyChat : mActiveChat;
					NkVector<Msg> &msgs = MsgsOf(idx);
					ChatSession &chat = mChats[static_cast<usize>(idx)];

					for (;;) {
						const usize nl = mClaudeBuf.Find('\n');
						if (nl == NkString::npos)
							break;
						NkString line = mClaudeBuf.SubStr(0, nl);
						mClaudeBuf.Erase(0, nl + 1);
						line.Trim();
						if (!line.Empty())
							HandleClaudeLine(line, msgs, chat);
					}

					if (!mClaudeProc.Running()) {
						mBusy = false;
						mPermPending = false; // le process est mort : plus personne n'attend la reponse
						if (!mClaudeStarted) { // aucun texte recu : erreur (auth/CLI absent/crash) — le
												// reste du buffer contient souvent le message (stderr fusionne).
							NkString diag = mClaudeBuf;
							diag.Trim();
							msgs.PushBack({2, diag.Empty() ? NkString(NkT("ai.errnet")) : diag});
						}
						mClaudeBuf.Clear();
						chat.stick = true;
					}
				}

				// ── Annule la requête Claude Code en cours (bouton Stop, cf. DrawBottomToolbar).
				// Termine le process (NkPipeProc::Stop -> ferme stdin, TerminateProcess après un
				// court délai si besoin) et redonne la main immédiatement — corrige le blocage
				// "A request is already running." quand une requête traîne sans jamais se
				// résoudre (ex. le panneau a été masqué entre-temps : Poll()/OnUI() ne tournent
				// que pour le panneau VISIBLE, donc une requête terminée pendant ce temps ne se
				// signale jamais — ce bouton donne toujours un moyen de s'en sortir). ──
				void CancelClaudeCli() {
					mClaudeProc.Stop();
					mPermPending = false; // une demande en attente meurt avec le process
					const int32 idx =
						(mBusyChat >= 0 && mBusyChat < static_cast<int32>(mChats.Size())) ? mBusyChat : mActiveChat;
					if (!mClaudeStarted) // aucun texte recu : rien a garder, juste noter l'annulation
						MsgsOf(idx).PushBack({2, NkString(NkT("ai.cancelled"))});
					mChats[static_cast<usize>(idx)].stick = true;
					mClaudeBuf.Clear();
					mBusy = false;
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
					// Rejouer un message : il peut etre long (il contenait peut-etre
					// deja un fichier entier) -> envoi integral.
					SendComposed(userText);
				}

				void Send() {
					if (mBusy || OutText().Empty())
						return;
					if (mKind == 1) { // Claude Code : CLI reel (memoire/outils/permissions natifs), pas de curl
						SendClaudeCli();
						return;
					}
					mProvider = ProviderForModel(); // dérivé de l'agent + du modèle choisi
					if (mProvider == 5) {			// Codex / OpenAI : intégration à venir
						Msgs().PushBack({0, OutText()});
						ClearOut();
						Msgs().PushBack({2, NkString(NkT("ai.codexsoon"))});
						mChats[static_cast<usize>(mActiveChat)].stick = true;
						return;
					}
					if (mProvider == 2) { // IA maison (NkAI)
						Msgs().PushBack({0, OutText()});
						ClearOut();
						Msgs().PushBack({2, NkString(NkT("ai.homesoon"))});
						mChats[static_cast<usize>(mActiveChat)].stick = true;
						return;
					}
					Msgs().PushBack({0, OutText()});
					ClearOut();
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
					PollAccountStatus(); // independant de mBusy (requete ponctuelle "claude auth status")
					PollQuickUsage();	 // independant de mBusy (requete ponctuelle "/usage")
					if (!mBusy)
						return;
					if (mKind == 1) { // Claude Code : draine le pipe NkPipeProc (NDJSON), pas curl/NkProcess
						PollClaudeCli();
						return;
					}
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
						// Ollama : message ACTIONNABLE (le cas le plus frequent = service pas
						// lance) plutot que le "reseau/curl ?" generique — cf. issue beta #6.
						msgs.PushBack({2, NkString(NkT(mProvider == 1 ? "ai.errollama" : "ai.errnet"))});
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
					// Cout/usage — Assistant général en API DIRECTE (mProvider==0) uniquement :
					// Ollama (mProvider==1) tourne en LOCAL, aucun cout a suivre. La reponse
					// Messages API brute n'a PAS de champ cout (contrairement au CLI Claude
					// Code) -> ESTIME via le tarif PUBLIC applique aux tokens reels retournes,
					// marque comme tel dans le popover (PublicRateFor), jamais confondu avec
					// les couts EXACTS du CLI.
					if (mProvider == 0) {
						const NkString model = JsonStr(j, "model");
						const int64 tokIn = (int64)JsonNum(j, "input_tokens");
						const int64 tokOut = (int64)JsonNum(j, "output_tokens");
						if (!model.Empty() && (tokIn > 0 || tokOut > 0)) {
							double inRate = 3.0, outRate = 15.0;
							PublicRateFor(model, inRate, outRate);
							const double cost = (double)tokIn / 1.0e6 * inRate + (double)tokOut / 1.0e6 * outRate;
							AccumModelUsage(model, cost, tokIn, tokOut);
						}
					}
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
