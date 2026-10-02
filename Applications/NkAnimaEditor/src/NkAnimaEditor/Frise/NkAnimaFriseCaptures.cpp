// =============================================================================
// NkAnimaFriseCaptures.cpp — `NkAnimaEditor --captures-frise=DOSSIER [modele]`
// (2026-10-01 au soir) : le panneau de la FRISE (TimelinePanel, Panels.h), HORS
// ECRAN -- sans fenetre, sans GPU, sans souris : le modele est charge et cuit
// (AnimInit), NKGui est rasterise par NkGuiDrawListRaster (le geste de la
// sonde de main.cpp), l'image part en PNG (NKImage).
//
// Le meme harnais a pris l'image « avant » (l'ancienne rangee de ronds) et
// celles d'« apres » : il appelle TimelinePanel::OnUI, quel qu'en soit le corps.
// =============================================================================
#include "Frise/NkAnimaFrise.h"
#include "Frise/NkAnimaGraphe.h"

#include "AnimBridge.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKGui/Core/NkGuiDrawListRaster.h"
#include "NKImage/Core/NkImage.h"
#include "Panels.h"

#include <cstdio>

namespace nkanima {

	using namespace nkentseu;

	namespace {
		bool Capturer(TimelinePanel &panneau, nkgui::NkGuiFont &police, bool policeOk, int32 w, int32 h, const NkString &chemin,
					  int32 trames) {
			bool ok = false;
			for (int32 k = 0; k < trames; ++k) {
				nkgui::NkGuiContext ctx;
				ctx.viewW = w;
				ctx.viewH = h;
				if (policeOk) {
					ctx.font = &police;
				}
				ctx.BeginFrame(0.016f);
				ctx.BeginLayout({0.f, 0.f, (float32)w, (float32)h});
				ctx.DL().Reset();
				editorkit::NkEditorFrameContext ec;
				ec.ui = &ctx;
				ec.dt = 0.f;
				panneau.OnUI(ec);
				if (k + 1 < trames) {
					continue; // les premieres trames installent le modele et la vue
				}
				nkgui::NkGuiDrawListRaster ras;
				if (!ras.Init(w, h)) {
					break;
				}
				ras.Effacer(0xFF101010u);
				if (policeOk) {
					ras.PoserTexture(police.TexId(), police.pixels, police.atlasW, police.atlasH, 1);
				}
				ras.Rasteriser(ctx.dl);
				ras.Rasteriser(ctx.dlOverlay);
				NkImage img = NkImage::Wrap(const_cast<uint8 *>(ras.Pixels()), w, h, NkImagePixelFormat::NK_RGBA32);
				ok = img.SavePNG(chemin.CStr());
			}
			std::printf("  capture %s : %s\n", chemin.CStr(), ok ? "ok" : "ECHEC");
			return ok;
		}
	} // namespace

	int32 NkAnimaCapturesFrise(const char *dossier, const char *modele) {
		std::printf("=== CAPTURES DE LA FRISE (NkAnimaEditor) ===\n");
		if (dossier == nullptr || !AnimInit(modele)) {
			std::printf("  /!\\ modele non charge : %s\n", modele != nullptr ? modele : "(aucun)");
			return 1;
		}
		NkDirectory::CreateRecursive(dossier);
		nkgui::NkGuiFont police;
		const bool policeOk = police.LoadEmbedded(NkEmbeddedFontId::DroidSans, 15.f, false);
		TimelinePanel panneau;
		const int32 W = 1600, H = 520;
		auto fichier = [&](const char *nom) {
			NkString c(dossier);
			c.Append("/");
			c.Append(nom);
			return c;
		};
		int32 erreurs = 0;
		AnimSetPlaying(false);
		AnimSeek(AnimDuration() * 0.4f);
		// 1. La FEUILLE : la piste des poses-cles, l'arbre des os ouvert sur deux
		//    niveaux, deux poses choisies (la boite de transformation).
		Capturer(panneau, police, policeOk, W, H, fichier("tmp.png"), 1);
		editorkit::NkTimelineModel &m = NkAnimaFriseModele();
		m.collapsed.Clear();
		for (uint32 i = 0; i < (uint32)m.tracks.Size(); ++i) {
			const NkString &o = m.tracks[i].object;
			if (editorkit::NkTimelineModel::ObjectDepth(o) >= 3 && !m.IsCollapsed(o)) {
				m.collapsed.PushBack(o);
			}
		}
		if (editorkit::NkTimelineTrack *poses = m.Track(1)) {
			for (uint32 k = 0; k < (uint32)poses->keys.Size(); ++k) {
				poses->keys[k].selected = k == 2 || k == 4;
			}
		}
		m.AddMarker(AnimDuration() * 0.25f, "Contact");
		m.AddMarker(AnimDuration() * 0.75f, "Appui");
		erreurs += Capturer(panneau, police, policeOk, W, H, fichier("11_nkanima_frise_feuille.png"), 2) ? 0 : 1;
		// 2. Les COURBES d'un os (rotation, en degres), comme le Curve Editor d'UE5.
		for (uint32 i = 0; i < (uint32)m.tracks.Size(); ++i) {
			// La rotation d'un os VISIBLE (pas dans un objet replie) qui bouge.
			if (m.tracks[i].id >= 1000 && (m.tracks[i].id % 2) == 1 && m.tracks[i].keys.Size() > 2 && !m.IsHidden(m.tracks[i].object)) {
				m.activeTrack = m.tracks[i].id;
				break;
			}
		}
		m.curveMode = true;
		m.curveAutoFit = true;
		erreurs += Capturer(panneau, police, policeOk, W, H, fichier("12_nkanima_frise_courbes.png"), 2) ? 0 : 1;
		// 3. Le THEME CLAIR.
		m.curveMode = false;
		NkAnimaThemeFrise(editorkit::NkTheme::Light());
		erreurs += Capturer(panneau, police, policeOk, W, H, fichier("13_nkanima_frise_clair.png"), 2) ? 0 : 1;
		nkentseu::NkFile::Delete(fichier("tmp.png").CStr());
		// (02/10) 4. LE GRAPHE D'ETATS partage, son APERCU dans la vue 3D : le
		//    controleur avance (une seconde), la pose part dans la vue, l'etat s'allume.
		{
			NkAnimaGrapheApercu() = true;
			const NkString chemin = fichier("16_nkanima_graphe_apercu.png");
			bool ok = false;
			for (int32 k = 0; k < 31; ++k) {
				nkgui::NkGuiContext ctx;
				ctx.viewW = W;
				ctx.viewH = H;
				if (policeOk) {
					ctx.font = &police;
				}
				ctx.BeginFrame(1.f / 30.f);
				ctx.BeginLayout({0.f, 0.f, (float32)W, (float32)H});
				ctx.DL().Reset();
				editorkit::NkEditorFrameContext ec;
				ec.ui = &ctx;
				ec.dt = 1.f / 30.f;
				NkAnimaDessinerGraphe(ec, 0.f, 0.f, (float32)W, (float32)H, editorkit::NkTheme::Dark(), "NkAnimaEditor_banc");
				if (k + 1 < 31) {
					continue;
				}
				nkgui::NkGuiDrawListRaster ras;
				if (ras.Init(W, H)) {
					ras.Effacer(0xFF101010u);
					if (policeOk) {
						ras.PoserTexture(police.TexId(), police.pixels, police.atlasW, police.atlasH, 1);
					}
					ras.Rasteriser(ctx.dl);
					ras.Rasteriser(ctx.dlOverlay);
					NkImage img = NkImage::Wrap(const_cast<uint8 *>(ras.Pixels()), W, H, NkImagePixelFormat::NK_RGBA32);
					ok = img.SavePNG(chemin.CStr());
				}
			}
			const editorkit::NkStateGraphModel &g = NkAnimaGrapheModele();
			const bool allume = g.live && g.liveState != 0;
			std::printf("  capture %s : %s\n", chemin.CStr(), ok ? "ok" : "ECHEC");
			std::printf("  [%s] apercu du controleur : pose dans la vue 3D (%s), etat allume (%s)\n",
						AnimApercuActif() && allume ? " OK " : "ECHEC", AnimApercuActif() ? "oui" : "non", allume ? "oui" : "non");
			erreurs += ok && AnimApercuActif() && allume ? 0 : 1;
			NkAnimaGrapheCache();
			erreurs += AnimApercuActif() ? 1 : 0; // l'onglet cache rend la pose du lecteur
		}
		return erreurs == 0 ? 0 : 1;
	}

} // namespace nkanima
