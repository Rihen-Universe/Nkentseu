#pragma once
// =============================================================================
// NkProblemsPanel.h — panneau « Problemes » (spec §12, roadmap #8).
//
// Remplace la maquette. Les diagnostics affiches sont ceux du DERNIER BUILD :
// aucune donnee inventee, aucune ligne de demonstration. Quand rien n'a encore
// ete construit, le panneau le DIT au lieu de montrer des exemples.
//
// Source : `NkCodeState::buildDiags`, alimente par les evenements structures de
// Jenga (COMPILE_ERROR / COMPILE_WARNING), dont le message est la sortie BRUTE
// du compilateur.
//
// Surtout PAS le transcript affiche : Jenga y encadre les messages et TRONQUE
// les chemins pour tenir dans la largeur du cadre. Une ligne reelle y ressemble
// a « ║ P.cpp:483:6: warning: ... ║ » pour un fichier dont le nom est bien plus
// long — et certaines perdent leur nom de fichier entierement. Un panneau bati
// la-dessus afficherait des chemins faux et des sauts qui echouent. C'est ce
// qu'une premiere version faisait, avant verification sur un vrai journal.
// =============================================================================
#include "NKCode/Project/NkCodeState.h"
#include "NKCode/Project/NkDiagParse.h"
#include "NKEditorKit/NkEditorKit.h"

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu::editorkit;
		using namespace nkentseu::nkgui;

		class NkProblemsPanel : public NkEditorPanel {
			public:
				NkProblemsPanel(NkCodeState *st) noexcept
					: NkEditorPanel("Problemes", NkEditorDockSide::NK_BOTTOM), mS(st) {
					SetOpen(false);
				}

				void OnUI(NkEditorFrameContext &ec) override {
					auto &ctx = ec.Ui();
					Refresh();

					// ── En-tete : compteurs REELS + filtre ───────────────────────────
					int32 nErr = 0, nWarn = 0;
					for (usize i = 0; i < Items().Size(); ++i)
						(Items()[i].sev == NkDiagSev::Error ? nErr : nWarn)++;
					ec.Text(NkPrintf("%d erreur(s) · %d avertissement(s)", nErr, nWarn).CStr());
					InputText(ctx, "Filtre", mFilter, static_cast<int32>(sizeof(mFilter)));
					ec.Separator();

					if (Items().Empty()) {
						// Distinguer « rien a signaler » de « rien n'a encore ete construit » :
						// les deux donnent une liste vide, mais ne veulent pas dire la meme
						// chose pour l'utilisateur.
						ec.Text(mS && mS->projTotal == 0
									? "Aucune construction lancee dans cette session."
									: "Aucun probleme dans la derniere construction.");
						return;
					}

					// ── Lignes ───────────────────────────────────────────────────────
					for (usize i = 0; i < Items().Size(); ++i) {
						const NkDiagInfo &d = Items()[i];
						if (mFilter[0] && !Matches(d))
							continue;
						// « fichier:ligne » puis le message : l'emplacement d'abord, parce
						// que c'est ce qu'on cherche quand on parcourt la liste.
						const NkString lbl =
							NkPrintf("%s  %s:%d  %s", d.sev == NkDiagSev::Error ? "E" : "A",
									 BaseName(d.file).CStr(), d.line, d.msg.CStr());
						if (Selectable(ctx, lbl.CStr(), false))
							Jump(d);
					}
				}

			private:
				// Rien a analyser ici : l'etat tient deja la liste, deduplquee, remplie
				// au fil des evenements. Le panneau n'en est que la vue.
				void Refresh() {}

				// Recherche insensible a la casse. NkText.h n'offre que la variante
				// sensible (NkFindSub) : on ne va pas taper « Error » avec un E majuscule
				// pour filtrer.
				static bool ContainsI(const char *hay, const char *needle) {
					if (!needle || !*needle)
						return true;
					auto low = [](char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; };
					for (; *hay; ++hay) {
						const char *a = hay;
						const char *b = needle;
						while (*a && *b && low(*a) == low(*b)) {
							++a;
							++b;
						}
						if (!*b)
							return true;
					}
					return false;
				}
				bool Matches(const NkDiagInfo &d) const {
					return ContainsI(d.file.CStr(), mFilter) || ContainsI(d.msg.CStr(), mFilter);
				}

				static NkString BaseName(const NkString &p) {
					const char *s = p.CStr();
					const char *last = s;
					for (const char *q = s; *q; ++q)
						if (*q == '/' || *q == '\\')
							last = q + 1;
					return NkString(last);
				}

				// Ouvre le fichier et place le curseur sur le probleme. Le chemin ecrit
				// par le compilateur peut etre RELATIF au repertoire de la construction :
				// on le resout alors depuis la racine du workspace, sinon l'ouverture
				// echouerait sans rien dire.
				void Jump(const NkDiagInfo &d) {
					if (!mS)
						return;
					NkPath p(d.file.CStr());
					if (!NkFile::Exists(p.ToString().CStr()) && mS->HasWorkspace())
						p = mS->root / d.file.CStr();
					if (!NkFile::Exists(p.ToString().CStr())) {
						mS->status = NkPrintf("Fichier introuvable : %s", d.file.CStr());
						mS->statusError = true;
						return;
					}
					mS->OpenPath(p);
					if (!mS->HasActive())
						return;
					OpenFile &f = mS->files[mS->active];
					const int32 ln = d.line > 0 ? d.line - 1 : 0;
					const int32 co = d.col > 0 ? d.col - 1 : 0;
					f.doc.curLine = ln;
					f.doc.curCol = co;
					f.doc.selLine = ln;
					f.doc.selCol = co;
					f.doc.ClampCursor();
					f.doc.ResetEditRun();
					f.doc.wantReveal = true; // fait defiler l'editeur jusqu'a la ligne
				}

				static const NkVector<NkDiagInfo> &Empty() {
					static const NkVector<NkDiagInfo> vide;
					return vide;
				}
				const NkVector<NkDiagInfo> &Items() const {
					return mS ? mS->buildDiags : Empty();
				}

				NkCodeState *mS = nullptr;
				char mFilter[96] = {0};
		};

	} // namespace nkcode
} // namespace nkentseu
