//
// NkEditeurPageAnimation.cpp
// =============================================================================
// Description :
//   LA PAGE ANIMATION : un clip de proprietes (.nkanim) sur une entite.
//     - en-tete : le document, l'entite animee, la duree, les images par
//       seconde, l'apercu, « Jouer en jeu », Enregistrer ;
//     - l'APERCU : le rendu du jeu, cadre sur l'entite, montree AU CURSEUR ;
//     - la FRISE du kit (NkTimelineModel.h) : pistes, cles, courbes, lecture ;
//     - le choix « + Propriete » : les proprietes animables de l'entite et de
//       ses descendants (Unkeny/Anim/NkUnkenyProprietes.h).
//
// ⚠️ L'APERCU NE LAISSE RIEN DANS LA SCENE. A chaque trame : les valeurs des
//    pistes sont RETENUES, le clip est applique au curseur, l'apercu est
//    dessine, puis les valeurs sont RENDUES. Une scene qu'on enregistre, qu'on
//    joue (la photo de Jouer) ou dont on suit l'empreinte ne voit jamais la
//    pose de l'apercu -- le defaut que le « mode apercu » d'Unity laisse a
//    l'utilisateur, ou une pose oubliee part dans la scene.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurActions.h"
#include "Unkeny/Squelette/NkUnkenySquelette.h"
#include "NKMemory/NKMemory.h"
#include "Editeur/NkEditeurInterface.h"
#include "Editeur/NkEditeurPagesAnim.h"
#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKEditorKit/Components/NkGuiComponentPaint.h"
#include "Unkeny/Partie/NkUnkenyPartie.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;
		using editorkit::NkRole;

		namespace {
			constexpr float32 HAUTEUR_EN_TETE = 34.f;

			struct NkCtxFrise {
					NkEditeurCadre *c = nullptr;
					NkDocAnim *d = nullptr;
			};

			/// L'interpolation de NKAnima (celle que le jeu jouera) : la piste du
			/// clip synchronise. Faux = le repli du kit.
			bool Evaluer(void *user, const editorkit::NkTimelineTrack &t, float32 temps, float32 out[4]) {
				NkCtxFrise *cf = static_cast<NkCtxFrise *>(user);
				// (R30) Une piste d'OS : la place de l'os telle que le jeu la jouera.
				if (t.property == NkString(NK_FRISE_PROPRIETE_OS)) {
					const int32 j = NkOsDuClipParNom(cf->d->clip, NkNomOsDePiste(t.object));
					if (j < 0 || (uint32)j >= (uint32)cf->d->clip.boneTracks.Size() ||
						cf->d->clip.boneTracks[(uint32)j].KeyCount() != (uint32)t.keys.Size()) {
						return false;
					}
					const anim::NkBone2D b = anim::NkSampleBone2D(cf->d->clip, (uint32)j, temps);
					out[0] = b.angle * 180.f / 3.14159265f;
					out[1] = b.x;
					out[2] = b.y;
					out[3] = 0.f;
					return true;
				}
				anim::NkAnimationClip::NkPropertyTrack *p = cf->d->clip.FindPropertyTrack(t.object, t.property);
				if (p == nullptr || p->curve.Empty() || p->curve.KeyCount() != (uint32)t.keys.Size()) {
					return false; // le clip a une trame de retard sur un geste : le repli
				}
				const math::NkVec4f v = p->Evaluate(temps); // (01/10 soir) la Courbe d'Hermite, comme le jeu
				out[0] = v.x;
				out[1] = v.y;
				out[2] = v.z;
				out[3] = v.w;
				return true;
			}

			/// La valeur VIVANTE de la propriete (une piste sans cle : ce que montre
			/// l'entite). Une piste qui a des cles rend faux : la courbe fait foi.
			bool LireVivant(void *user, const editorkit::NkTimelineTrack &t, float32 out[4]) {
				NkCtxFrise *cf = static_cast<NkCtxFrise *>(user);
				if (!t.keys.Empty()) {
					return false;
				}
				NkEditeurModele &m = cf->c->m;
				const ecs::NkEntityId racine = NkEditeurCibleDoc(m, *cf->d);
				// (R30) Une piste d'OS sans cle : la pose vivante de l'os.
				if (t.property == NkString(NK_FRISE_PROPRIETE_OS)) {
					const unkeny::NkSquelette2D *sq = m.scene.Monde().IsAlive(racine) ? m.scene.Monde().Get<unkeny::NkSquelette2D>(racine) : nullptr;
					const int32 j = sq != nullptr ? unkeny::NkSqueletteTrouverOs(*sq, NkNomOsDePiste(t.object).CStr()) : -1;
					if (j < 0) {
						return false;
					}
					out[0] = sq->os[j].pangle * 180.f / 3.14159265f;
					out[1] = sq->os[j].px;
					out[2] = sq->os[j].py;
					out[3] = 0.f;
					return true;
				}
				const ecs::NkEntityId cible = unkeny::NkResoudreCible(m.scene, racine, t.object);
				math::NkVec4f v;
				if (!unkeny::NkLireProprieteAnimee(m.scene, cible, t.property, v)) {
					return false;
				}
				out[0] = v.x;
				out[1] = v.y;
				out[2] = v.z;
				out[3] = v.w;
				return true;
			}

			editorkit::NkTimelineStyle StyleFrise() {
				editorkit::NkTimelineStyle s;
				s.panelBg = (uint16)NkRole::PanelBg;
				s.headerBg = (uint16)NkRole::PanelHeader;
				s.rowAltBg = (uint16)NkRole::InputBg;
				s.border = (uint16)NkRole::Border;
				s.grid = (uint16)NkRole::GridLine;
				s.text = (uint16)NkRole::Text;
				s.textMuted = (uint16)NkRole::TextMuted;
				s.accent = (uint16)NkRole::AccentUi;
				s.textOnAccent = (uint16)NkRole::TextOnAccent;
				s.key = (uint16)NkRole::Text;
				s.keySelected = (uint16)NkRole::AccentSel;
				s.playhead = (uint16)NkRole::StatusErr;
				s.channel[0] = (uint16)NkRole::AxisX;
				s.channel[1] = (uint16)NkRole::AxisY;
				s.channel[2] = (uint16)NkRole::AxisZ;
				s.channel[3] = (uint16)NkRole::TextMuted;
				s.buttonBg = (uint16)NkRole::ButtonBg;
				s.inputBg = (uint16)NkRole::InputBg;
				// (01/10 soir) la frise « UE5 » : plage, marqueurs, clips, tangentes.
				s.rangeIn = (uint16)NkRole::StatusOk;
				s.rangeOut = (uint16)NkRole::StatusErr;
				s.marker = (uint16)NkRole::AccentSel;
				s.clip = (uint16)NkRole::TypeAnim;
				s.clipAlt = (uint16)NkRole::NodeActionHeader;
				s.tangent = (uint16)NkRole::AccentSel;
				return s;
			}

			editorkit::NkTimelineValueKind GenreFrise(unkeny::NkGenrePropriete g) {
				using PK = unkeny::NkGenrePropriete;
				if (g == PK::NK_COLOR) {
					return editorkit::NkTimelineValueKind::Couleur;
				}
				if (g == PK::NK_STEP) {
					return editorkit::NkTimelineValueKind::Palier;
				}
				return editorkit::NkTimelineValueKind::Nombre;
			}

			NkString NomDe(NkEditeurModele &m, ecs::NkEntityId id) {
				const NkEtiquette *e = m.scene.Monde().IsAlive(id) ? m.scene.Monde().Get<NkEtiquette>(id) : nullptr;
				return e != nullptr && e->nom[0] != '\0' ? NkString(e->nom) : NkString("(sans nom)");
			}

			/// Les propositions de « + Propriete » : l'entite, puis ses descendants,
			/// sans les proprietes deja en piste.
			void Proposer(NkEditeurModele &m, NkDocAnim &d) {
				d.propositions.Clear();
				d.choixDefil = 0.f;
				d.choixClips = false;
				// (01/10 soir) En tete : une PISTE DE CLIPS (des animations posees bout a
				// bout, qui se chevauchent et se fondent -- le NLA de Blender).
				{
					NkProposition p;
					p.pisteClips = true;
					p.entete = "Séquence";
					p.propriete.nom = "Clips";
					p.propriete.libelle = "Piste de clips (animations posées, fondus)";
					d.propositions.PushBack(p);
				}
				const ecs::NkEntityId racine = NkEditeurCibleDoc(m, d);
				if (!m.scene.Monde().IsAlive(racine)) {
					return;
				}
				// (2026-10-02, R30) Les OS du squelette 2D : une piste par os (angle, x, y),
				// et « tous les os » (une cle de pose au curseur).
				if (const unkeny::NkSquelette2D *sq = m.scene.Monde().Get<unkeny::NkSquelette2D>(racine)) {
					if (sq->nbOs > 0u) {
						NkProposition tous;
						tous.entete = "Squelette 2D";
						tous.cible = NkString(NK_FRISE_OBJET_SQUELETTE);
						tous.propriete.nom = "Os*";
						tous.propriete.libelle = NkString::Format("Tous les os (clé de pose, %u os)", static_cast<uint32>(sq->nbOs));
						d.propositions.PushBack(tous);
					}
					for (uint32 j = 0; j < sq->nbOs; ++j) {
						NkString chemin(NK_FRISE_OBJET_SQUELETTE);
						chemin.Append("/");
						chemin.Append(unkeny::NkSqueletteCheminOs(*sq, j).CStr());
						bool deja = false;
						for (uint32 t = 0; t < (uint32)d.frise.tracks.Size(); ++t) {
							deja = deja || (d.frise.tracks[t].property == NkString(NK_FRISE_PROPRIETE_OS) &&
											NkNomOsDePiste(d.frise.tracks[t].object) == NkString(sq->os[j].nom));
						}
						if (deja) {
							continue;
						}
						NkProposition p;
						p.entete = "Squelette 2D";
						p.cible = chemin;
						p.propriete.nom = NK_FRISE_PROPRIETE_OS;
						p.propriete.libelle = NkString::Format("Os : %s (angle, x, y)", sq->os[j].nom);
						p.propriete.canaux = 3;
						d.propositions.PushBack(p);
					}
				}
				NkVector<ecs::NkEntityId> pile;
				pile.PushBack(racine);
				NkVector<ecs::NkEntityId> enfants;
				NkVector<unkeny::NkProprieteAnimable> props;
				for (uint32 k = 0; k < (uint32)pile.Size() && k < 256u; ++k) {
					const ecs::NkEntityId id = pile[k];
					NkString chemin;
					unkeny::NkCheminCible(m.scene, racine, id, chemin);
					unkeny::NkListerProprietesAnimables(m.scene, id, props);
					for (uint32 i = 0; i < (uint32)props.Size(); ++i) {
						bool deja = false;
						for (uint32 t = 0; t < (uint32)d.frise.tracks.Size(); ++t) {
							deja = deja || (d.frise.tracks[t].object == chemin && d.frise.tracks[t].property == props[i].nom);
						}
						if (deja) {
							continue;
						}
						NkProposition p;
						p.cible = chemin;
						p.objet = NomDe(m, id);
						p.propriete = props[i];
						d.propositions.PushBack(p);
					}
					m.scene.Enfants(id, enfants);
					for (uint32 e = 0; e < (uint32)enfants.Size(); ++e) {
						pile.PushBack(enfants[e]);
					}
				}
			}

			/// (01/10 soir) Les clips a poser sur une piste de clips : ceux qu'Unkeny
			/// connait (enregistres), et les autres animations ouvertes.
			void ProposerClips(NkEditeurInterface &ui, NkDocAnim &d, nk_uint64 piste, float32 t) {
				d.propositions.Clear();
				d.choixDefil = 0.f;
				d.choixClips = true;
				d.choixPiste = piste;
				d.choixTemps = t;
				auto ajouter = [&](const NkString &nom, float32 duree) {
					if (nom.Empty() || nom == d.nom) {
						return; // une sequence ne se pose pas dans elle-meme
					}
					for (uint32 i = 0; i < (uint32)d.propositions.Size(); ++i) {
						if (d.propositions[i].clip == nom) {
							return;
						}
					}
					NkProposition p;
					p.entete = "Animations";
					p.clip = nom;
					p.duree = duree > 1e-3f ? duree : 1.f;
					p.propriete.libelle = nom;
					p.propriete.nom = NkString::Format("%.2f s", (double)p.duree);
					d.propositions.PushBack(p);
				};
				for (uint32 i = 0; i < unkeny::NkNbClipsProprietes(); ++i) {
					const char *n = unkeny::NkNomClipProprietes(i);
					const anim::NkAnimationClip *c = unkeny::NkClipProprietesEnregistre(n);
					ajouter(NkString(n), c != nullptr ? c->duration : 1.f);
				}
				for (uint32 i = 0; i < (uint32)ui.pagesAnim.docs.Size(); ++i) {
					const NkDocAnim &o = ui.pagesAnim.docs[i];
					if (o.genre == NkGenreDocAnim::NK_ANIMATION) {
						ajouter(o.nom, o.frise.duration);
					}
				}
			}

			/// Pose le clip choisi sur la piste de clips (et l'enregistre s'il ne
			/// l'etait pas : l'apercu et le jeu le retrouvent par son nom).
			void PoserClip(NkEditeurInterface &ui, NkDocAnim &d, const NkProposition &p) {
				if (unkeny::NkClipProprietesEnregistre(p.clip.CStr()) == nullptr) {
					for (uint32 i = 0; i < (uint32)ui.pagesAnim.docs.Size(); ++i) {
						NkDocAnim &o = ui.pagesAnim.docs[i];
						if (o.nom == p.clip && o.genre == NkGenreDocAnim::NK_ANIMATION) {
							NkClipDepuisFrise(o.frise, o.clip);
							o.clip.name = o.nom;
							unkeny::NkEnregistrerClipProprietes(o.nom.CStr(), o.clip);
						}
					}
				}
				const nk_uint64 id = d.frise.AddClip(d.choixPiste, p.clip, d.choixTemps, p.duree, p.duree);
				d.frise.SelectClip(id, false);
				d.frise.activeTrack = d.choixPiste;
			}

			/// Ajoute la piste d'une proposition, et sa premiere cle AU CURSEUR, de
			/// la valeur que l'entite montre.
			void AjouterPiste(NkEditeurModele &m, NkDocAnim &d, const NkProposition &p) {
				nk_uint64 id = 1;
				for (uint32 t = 0; t < (uint32)d.frise.tracks.Size(); ++t) {
					id = d.frise.tracks[t].id >= id ? d.frise.tracks[t].id + 1 : id;
				}
				if (p.pisteClips) {
					editorkit::NkTimelineTrack &t = d.frise.AddTrack(id, NkString(), "Clips", editorkit::NkTimelineValueKind::Clips, 1);
					t.label = "Clips";
					d.frise.activeTrack = id;
					return;
				}
				// (2026-10-02, R30) Un OS (ou tous) : le clip apprend le squelette, puis une
				// piste par os et sa cle au curseur, de la pose vivante.
				if (p.propriete.nom == NkString(NK_FRISE_PROPRIETE_OS) || p.propriete.nom == NkString("Os*")) {
					const ecs::NkEntityId racine = NkEditeurCibleDoc(m, d);
					const unkeny::NkSquelette2D *sq = m.scene.Monde().IsAlive(racine) ? m.scene.Monde().Get<unkeny::NkSquelette2D>(racine) : nullptr;
					if (sq == nullptr) {
						return;
					}
					NkClipDepuisFrise(d.frise, d.clip); // le clip a jour avant de lui apprendre le squelette
					uint8 choisis[unkeny::NK_SQUELETTE2D_OS_MAX] = {};
					const bool tous = p.propriete.nom == NkString("Os*");
					for (uint32 j = 0; j < sq->nbOs; ++j) {
						choisis[j] = (tous || NkNomOsDePiste(p.cible) == NkString(sq->os[j].nom)) ? 1u : 0u;
					}
					const uint32 avant = (uint32)d.frise.tracks.Size();
					unkeny::NkSqueletteCle(*sq, d.clip, d.frise.cursor, choisis);
					// La frise relit le clip (les pistes d'os), en gardant son curseur.
					const float32 curseur = d.frise.cursor;
					NkFriseDepuisClip(d.clip, d.frise);
					d.frise.cursor = curseur;
					d.frise.activeTrack = (uint32)d.frise.tracks.Size() > avant ? d.frise.tracks[d.frise.tracks.Size() - 1].id : 0;
					return;
				}
				editorkit::NkTimelineTrack &t =
					d.frise.AddTrack(id, p.cible, p.propriete.nom, GenreFrise(p.propriete.genre), p.propriete.canaux);
				t.label = p.propriete.libelle;
				const ecs::NkEntityId racine = NkEditeurCibleDoc(m, d);
				math::NkVec4f v;
				unkeny::NkLireProprieteAnimee(m.scene, unkeny::NkResoudreCible(m.scene, racine, p.cible), p.propriete.nom, v);
				const float32 vals[4] = {v.x, v.y, v.z, v.w};
				d.frise.SetKey(id, d.frise.cursor, vals);
				d.frise.activeTrack = id;
			}

			/// Le choix « + Propriete », PAR-DESSUS tout (dlOverlay). Rend vrai si la
			/// souris est dessus (la frise ne doit pas la voir).
			/// `ouvertAvant` : le choix etait deja ouvert au debut de la trame. Le clic
			/// qui vient de l'OUVRIR (sur « + Propriete ») ne doit pas le refermer.
			bool DessinerChoix(NkEditeurCadre &c, NkDocAnim &d, const NkRect &ancre, bool ouvertAvant) {
				if (!d.choix) {
					return false;
				}
				auto &dl = c.ctx.dlOverlay;
				const nkgui::NkGuiInput &in = c.ctx.input;
				const float32 lh = 22.f;
				const float32 w = 340.f;
				// Les lignes : un en-tete par objet, puis ses proprietes.
				uint32 lignes = 0;
				auto groupeNeuf = [&](uint32 i) {
					return i == 0 || !(d.propositions[i - 1].cible == d.propositions[i].cible) ||
						   !(d.propositions[i - 1].entete == d.propositions[i].entete);
				};
				for (uint32 i = 0; i < (uint32)d.propositions.Size(); ++i) {
					lignes += groupeNeuf(i) ? 2u : 1u;
				}
				const float32 hMax = c.ui.ecran.h * 0.6f;
				const float32 h = (float32)(lignes > 0 ? lignes : 1u) * lh + 8.f;
				const NkRect r{ancre.x, ancre.y + ancre.h + 2.f, w, h < hMax ? h : hMax};
				d.choixRect = r;
				const bool dessus = NkEditeurDans(r, in.mousePos);
				dl.AddRectFilled(NkRect{r.x + 3.f, r.y + 3.f, r.w, r.h}, NkColor(0, 0, 0, 90), 4.f);
				dl.AddRectFilled(r, c.pal.panneau, 4.f);
				dl.AddRect(r, c.pal.bord, 1.f, 4.f);
				if (dessus && in.wheel != 0.f) {
					d.choixDefil -= in.wheel * lh * 3.f;
				}
				const float32 maxDefil = (float32)lignes * lh + 8.f - r.h;
				d.choixDefil = d.choixDefil > maxDefil ? maxDefil : d.choixDefil;
				d.choixDefil = d.choixDefil < 0.f ? 0.f : d.choixDefil;
				dl.PushClipRect(r, true);
				float32 y = r.y + 4.f - d.choixDefil;
				int32 choisie = -1;
				if (d.propositions.Empty()) {
					renderer::NkTexte(dl, c.police, r.x + 10.f, y + 3.f,
									  d.choixClips ? "Aucune animation à poser (enregistrez-en une)" : "Aucune propriété à ajouter (entité animée ?)",
									  c.pal.attenue);
				}
				for (uint32 i = 0; i < (uint32)d.propositions.Size(); ++i) {
					NkProposition &p = d.propositions[i];
					if (groupeNeuf(i)) {
						const NkString titre = !p.entete.Empty() ? p.entete
											   : p.cible.Empty() ? NkString::Format("%s (l'entité animée)", p.objet.CStr())
																 : NkString::Format("%s  — %s", p.objet.CStr(), p.cible.CStr());
						dl.AddRectFilled(NkRect{r.x + 1.f, y, r.w - 2.f, lh}, c.pal.entete);
						renderer::NkTexte(dl, c.police, r.x + 8.f, y + 3.f, titre.CStr(), c.pal.texte);
						y += lh;
					}
					p.rect = NkRect{r.x + 1.f, y, r.w - 2.f, lh};
					const bool survol = NkEditeurDans(p.rect, in.mousePos) && dessus;
					if (survol) {
						dl.AddRectFilled(p.rect, c.pal.accent);
						// Le clic qui vient d'OUVRIR le choix ne choisit rien (01/10 soir : le
						// choix des clips s'ouvre sous le « + » d'une piste, deja sous la souris).
						if (in.mouseClicked[0] && ouvertAvant) {
							choisie = (int32)i;
						}
					}
					renderer::NkTexte(dl, c.police, r.x + 22.f, y + 3.f, p.propriete.libelle.CStr(), survol ? c.pal.surAccent : c.pal.texte);
					renderer::NkTexteADroite(dl, c.petite, r.x + r.w - 10.f, y + 4.f, p.propriete.nom.CStr(),
											 survol ? c.pal.surAccent : c.pal.attenue);
					y += lh;
				}
				dl.PopClipRect();
				if (choisie >= 0) {
					const NkProposition p = d.propositions[(uint32)choisie];
					if (d.choixClips) {
						PoserClip(c.ui, d, p);
					} else {
						AjouterPiste(c.m, d, p);
					}
					d.choix = false;
					d.choixClips = false;
				} else if (ouvertAvant && in.mouseClicked[0] && !dessus) {
					d.choix = false; // un clic ailleurs ferme, comme un menu
				}
				return dessus;
			}

			/// Le libelle d'une duree : « 1,50 s (45 images) ».
			NkString TexteDuree(const editorkit::NkTimelineModel &f) {
				return NkString::Format("%.2f s (%d im.)", (double)f.duration, f.FrameOf(f.duration));
			}
		} // namespace

		// =====================================================================
		// L'APERCU
		// =====================================================================
		void NkEditeurDessinerApercuAnim(NkEditeurCadre &c, ecs::NkEntityId cible, const NkRect &r) {
			auto &dl = c.ctx.dl;
			dl.AddRectFilled(r, NkColor(20, 23, 31), 3.f);
			dl.AddRect(r, c.pal.bord, 1.f, 3.f);
			NkScene &scene = c.m.scene;
			if (!scene.Monde().IsAlive(cible)) {
				renderer::NkTexteDansBoite(dl, c.police, r, "Aucune entité : choisissez-en une dans la scène, puis « Utiliser la sélection »",
										   c.pal.attenue);
				return;
			}
			// La camera de la scene, PRETEE le temps du dessin : cadree sur l'entite,
			// rendue telle quelle ensuite (la vue de la scene ne bouge pas).
			NkVue2D &cam = scene.Camera();
			const nkgui::NkRect viseur0 = cam.Viseur();
			const NkVec2f centre0 = cam.Centre();
			const float32 zoom0 = cam.Zoom();
			const float32 rot0 = cam.Rotation();
			NkVec2f centre(0.f, 0.f), taille(2.f, 2.f);
			if (const NkTransform2D *t = scene.Monde().Get<NkTransform2D>(cible)) {
				centre = t->position;
				if (const NkSprite2D *s = scene.Monde().Get<NkSprite2D>(cible)) {
					const float32 ex = s->taille.x * (t->echelle.x < 0.f ? -t->echelle.x : t->echelle.x);
					const float32 ey = s->taille.y * (t->echelle.y < 0.f ? -t->echelle.y : t->echelle.y);
					taille = NkVec2f(ex > 0.5f ? ex : 0.5f, ey > 0.5f ? ey : 0.5f);
				}
			}
			cam.PoserViseur(r);
			cam.PoserRotation(0.f);
			cam.Cadrer(centre, NkVec2f(taille.x * 3.5f, taille.y * 3.5f));
			dl.PushClipRect(r, true);
			NkDessinerGrille(dl, cam, 1.f);
			NkDessinerPartie(dl, scene, c.m.rendu);
			dl.PopClipRect();
			cam.PoserViseur(viseur0);
			cam.PoserCentre(centre0);
			cam.PoserZoom(zoom0);
			cam.PoserRotation(rot0);
			const NkString nom = NkString::Format("Aperçu : %s", NomDe(c.m, cible).CStr());
			renderer::NkTexte(dl, c.petite, r.x + 8.f, r.y + 6.f, nom.CStr(), c.pal.attenue);
		}

		void NkEditeurRendreApercu(NkEditeurModele &m, NkDocAnim &d) {
			// L'apercu ne laisse rien entre deux trames (voir l'en-tete) : il n'y a
			// a rendre que ce qu'une trame interrompue aurait laisse.
			if (d.retenues.Empty()) {
				return;
			}
			const ecs::NkEntityId racine = NkEditeurCibleDoc(m, d);
			for (uint32 i = 0; i < (uint32)d.retenues.Size(); ++i) {
				const NkValeurRetenue &v = d.retenues[i];
				unkeny::NkEcrireProprieteAnimee(m.scene, unkeny::NkResoudreCible(m.scene, racine, v.cible), v.propriete, v.valeur);
			}
			d.retenues.Clear();
		}

		void NkEditeurEntretenirDocAnim(NkEditeurCadre &c, NkDocAnim &d, bool auPremierPlan) {
			if (d.genre == NkGenreDocAnim::NK_ANIMATEUR) {
				NkEditeurSuivreAnimateur(c, d);
				return;
			}
			if (d.frise.revision != d.revisionClip) {
				NkClipDepuisFrise(d.frise, d.clip);
				d.revisionClip = d.frise.revision;
			}
			d.modifie = d.frise.revision != d.revisionEnregistree || d.chemin.Empty();
			if (!auPremierPlan) {
				d.frise.playing = false;
				NkEditeurRendreApercu(c.m, d);
			}
		}

		// =====================================================================
		// LA PAGE
		// =====================================================================
		void NkEditeurDessinerPageAnimation(NkEditeurCadre &c, NkDocAnim &d, const NkRect &zone) {
			NkEditeurModele &m = c.m;
			NkPagesAnim &pa = c.ui.pagesAnim;
			auto &dl = c.ctx.dl;
			NkEditeurEntretenirDocAnim(c, d, true);
			// La lecture avance au temps de la TRAME de l'editeur.
			d.frise.Advance(c.ui.dt);
			const ecs::NkEntityId cible = NkEditeurCibleDoc(m, d);

			// ── L'en-tete ──────────────────────────────────────────────────────
			const NkRect tete{zone.x, zone.y, zone.w, HAUTEUR_EN_TETE};
			dl.AddRectFilled(tete, c.pal.entete);
			dl.AddRectFilled(NkRect{tete.x, tete.y + tete.h - 1.f, tete.w, 1.f}, c.pal.bord);
			const float32 ty = tete.y + (tete.h - renderer::NkTexteHauteurLigne(c.police, 16.f)) * 0.5f;
			float32 x = tete.x + 12.f;
			const NkColor nature(c.theme.Get(NkRole::TypeAnim));
			dl.AddRectFilled(NkRect{x, tete.y + tete.h * 0.5f - 5.f, 10.f, 10.f}, nature, 2.f);
			x += 18.f;
			const NkString titre = NkString::Format("Animation  %s", d.nom.CStr());
			renderer::NkTexte(dl, c.police, x, ty, titre.CStr(), c.pal.texte);
			x += renderer::NkTexteLargeur(c.police, titre.CStr());
			if (d.modifie) {
				// Le point « non enregistre », TRACE (la police embarquee n'a pas « ● »).
				dl.AddCircleFilled(NkVec2{x + 8.f, tete.y + tete.h * 0.5f}, 3.5f, c.pal.selection);
			}
			x += 24.f;
			const NkString objet = NkString::Format("Entité animée : %s", m.scene.Monde().IsAlive(cible) ? NomDe(m, cible).CStr() : "(aucune)");
			renderer::NkTexte(dl, c.police, x, ty, objet.CStr(), c.pal.attenue);
			x += renderer::NkTexteLargeur(c.police, objet.CStr()) + 8.f;
			const float32 bh = tete.h - 8.f;
			auto bouton = [&](NkBoutonPage b, const char *texte, bool enfonce, bool actif, float32 bx) {
				const float32 w = renderer::NkTexteLargeur(c.police, texte) + 18.f;
				const NkRect r{bx, tete.y + 4.f, w, bh};
				pa.boutons[(uint8)b] = r;
				return NkEditeurBouton(c, r, texte, enfonce, actif);
			};
			const bool sel = m.aSelection && m.scene.Monde().IsAlive(m.selection);
			if (bouton(NkBoutonPage::NK_SELECTION, "Utiliser la sélection", false, sel, x) && sel) {
				d.cibleUid = m.scene.AssurerUid(m.selection);
			}
			x += pa.boutons[(uint8)NkBoutonPage::NK_SELECTION].w + 24.f;
			// La duree et les images par seconde.
			renderer::NkTexte(dl, c.police, x, ty, "Durée", c.pal.attenue);
			x += renderer::NkTexteLargeur(c.police, "Durée") + 6.f;
			if (bouton(NkBoutonPage::NK_DUREE_MOINS, "-", false, d.frise.duration > 0.25f, x)) {
				d.frise.PushUndo();
				d.frise.duration = d.frise.duration - 0.25f > 0.25f ? d.frise.duration - 0.25f : 0.25f;
				d.frise.Touch();
			}
			x += pa.boutons[(uint8)NkBoutonPage::NK_DUREE_MOINS].w + 4.f;
			const NkString duree = TexteDuree(d.frise);
			renderer::NkTexte(dl, c.police, x, ty, duree.CStr(), c.pal.texte);
			x += renderer::NkTexteLargeur(c.police, duree.CStr()) + 4.f;
			if (bouton(NkBoutonPage::NK_DUREE_PLUS, "+", false, true, x)) {
				d.frise.PushUndo();
				d.frise.duration += 0.25f;
				d.frise.Touch();
			}
			x += pa.boutons[(uint8)NkBoutonPage::NK_DUREE_PLUS].w + 12.f;
			const NkString fps = NkString::Format("%.0f im./s", (double)d.frise.fps);
			if (bouton(NkBoutonPage::NK_FPS, fps.CStr(), false, true, x)) {
				static const float32 kFps[4] = {12.f, 24.f, 30.f, 60.f};
				int32 k = 0;
				while (k < 4 && kFps[k] <= d.frise.fps + 0.5f) {
					++k;
				}
				d.frise.PushUndo();
				d.frise.fps = kFps[k % 4];
				d.frise.Touch();
			}
			// A droite : l'apercu, « Jouer en jeu », Enregistrer.
			float32 xd = tete.x + tete.w - 10.f;
			auto boutonDroite = [&](NkBoutonPage b, const char *texte, bool enfonce, bool actif) {
				const float32 w = renderer::NkTexteLargeur(c.police, texte) + 18.f;
				xd -= w;
				const bool clic = bouton(b, texte, enfonce, actif, xd);
				xd -= 6.f;
				return clic;
			};
			if (boutonDroite(NkBoutonPage::NK_ENREGISTRER, "Enregistrer", false, true)) {
				NkEditeurEnregistrerDocAnim(m, d);
			}
			if (boutonDroite(NkBoutonPage::NK_ATTACHER, "Jouer en jeu", false, m.scene.Monde().IsAlive(cible))) {
				// Le clip enregistre sous son nom, et pose sur l'entite : en Jouer, le
				// systeme NkAvancerClipsProprietes le fait tourner.
				NkClipDepuisFrise(d.frise, d.clip);
				d.clip.name = d.nom;
				unkeny::NkEnregistrerClipProprietes(d.nom.CStr(), d.clip);
				unkeny::NkClipProprietes2D cp;
				cp.Jouer(d.nom.CStr());
				cp.boucle = d.frise.loop;
				m.scene.Monde().Add<unkeny::NkClipProprietes2D>(cible, cp); // Add : ajoute OU remplace (Set ignore un composant absent)
				NkEditeurAnnoncer(m, NkString::Format("« %s » jouera sur %s en Jouer", d.nom.CStr(), NomDe(m, cible).CStr()).CStr());
			}
			if (boutonDroite(NkBoutonPage::NK_APERCU, "Aperçu", d.apercu, true)) {
				d.apercu = !d.apercu;
			}

			// ── L'apercu, puis la frise ───────────────────────────────────────
			const float32 corpsY = tete.y + tete.h;
			const float32 corpsH = zone.y + zone.h - corpsY;
			const float32 apercuH = corpsH * 0.38f;
			const NkRect apercu{zone.x + 6.f, corpsY + 6.f, zone.w - 12.f, apercuH - 12.f};
			pa.apercu = apercu;
			const NkRect frise{zone.x, corpsY + apercuH, zone.w, corpsH - apercuH};
			if (d.apercu && m.etat == NkEtatJeu::NK_EDITION && m.scene.Monde().IsAlive(cible)) {
				// RETENIR, appliquer au curseur, dessiner, RENDRE (voir l'en-tete).
				// (01/10 soir) La POSE melangee de NKAnima : les pistes du clip ET ses
				// pistes de clips (NLA) ; une piste muette, ou hors du solo, n'y est pas.
				const anim::NkClipLookup lookup = unkeny::NkRechercheClipsProprietes();
				anim::NkAnimPose pose;
				anim::NkSampleClip(d.clip, d.frise.cursor, pose, &lookup);
				for (int32 i = (int32)pose.props.Size() - 1; i >= 0; --i) {
					for (uint32 t = 0; t < (uint32)d.frise.tracks.Size(); ++t) {
						const editorkit::NkTimelineTrack &tr = d.frise.tracks[t];
						if (tr.object == pose.props[(uint32)i].target && tr.property == pose.props[(uint32)i].property &&
							!d.frise.TrackActive(tr)) {
							pose.props.Erase(pose.props.Begin() + i);
							break;
						}
					}
				}
				d.retenues.Clear();
				for (uint32 i = 0; i < (uint32)pose.props.Size(); ++i) {
					const anim::NkPropValue &p = pose.props[i];
					NkValeurRetenue v;
					v.cible = p.target;
					v.propriete = p.property;
					if (unkeny::NkLireProprieteAnimee(m.scene, unkeny::NkResoudreCible(m.scene, cible, p.target), p.property, v.valeur)) {
						d.retenues.PushBack(v);
					}
				}
				// (R30) La pose des OS va au squelette : il est GARDE et RENDU apres le
				// dessin, comme les proprietes (l'apercu ne laisse aucune trace).
				unkeny::NkSquelette2D *sqVif = m.scene.Monde().Get<unkeny::NkSquelette2D>(cible);
				unkeny::NkSquelette2D *sqGarde = sqVif != nullptr ? memory::NkGetDefaultAllocator().New<unkeny::NkSquelette2D>(*sqVif) : nullptr;
				unkeny::NkAppliquerPoseProprietes(m.scene, cible, pose);
				NkEditeurDessinerApercuAnim(c, cible, apercu);
				NkEditeurRendreApercu(m, d);
				if (sqGarde != nullptr) {
					if (unkeny::NkSquelette2D *s2 = m.scene.Monde().Get<unkeny::NkSquelette2D>(cible)) {
						*s2 = *sqGarde;
					}
					memory::NkGetDefaultAllocator().Delete(sqGarde);
				}
			} else {
				NkEditeurDessinerApercuAnim(c, cible, apercu);
				if (m.etat != NkEtatJeu::NK_EDITION && m.scene.Monde().IsAlive(cible)) {
					renderer::NkTexte(dl, c.petite, apercu.x + 8.f, apercu.y + apercu.h - 18.f,
									  "En jeu : la scène joue, l'aperçu de la frise attend « Arrêter »", c.pal.attenue);
				}
			}

			// ── La frise du kit ───────────────────────────────────────────────
			// Le choix « + Propriete » est PAR-DESSUS : la souris qui est sur lui
			// ne doit pas atteindre la frise (elle est dessinee d'abord).
			editorkit::NkComponentInput ci = NkEditeurEntreeComposant(c.ctx);
			const bool surChoix = d.choix && NkEditeurDans(d.choixRect, c.ctx.input.mousePos);
			if (surChoix || c.ui.menu != NkMenuEditeur::NK_AUCUN) {
				ci.mouseX = ci.mouseY = -10000.f;
				ci.mousePressed = ci.mouseReleased = ci.doubleClick = false;
				ci.wheel = 0.f;
			}
			const bool choixOuvert = d.choix;
			NkCtxFrise cf;
			cf.c = &c;
			cf.d = &d;
			editorkit::NkTimelineHooks hooks;
			hooks.user = &cf;
			hooks.evaluate = &Evaluer;
			hooks.readLive = &LireVivant;
			editorkit::NkGuiComponentPaint peintre(c.ctx, c.theme);
			const editorkit::NkTimelineStyle style = StyleFrise();
			// La racine de l'arbre des pistes porte le nom de l'entite animee.
			d.frise.rootLabel = m.scene.Monde().IsAlive(cible) ? NomDe(m, cible) : NkString();
			pa.frise = editorkit::NkDrawTimeline(peintre, ci, editorkit::NkPaintRect{frise.x, frise.y, frise.w, frise.h}, d.frise,
												  style, hooks);
			if (pa.frise.changed) {
				NkClipDepuisFrise(d.frise, d.clip);
				d.revisionClip = d.frise.revision;
			}
			if (pa.frise.addTrackRequested) {
				if (d.choix) {
					d.choix = false;
				} else {
					Proposer(m, d);
					d.choix = true;
				}
			}
			// (01/10 soir) Une piste de clips demande un clip : le meme choix, en clips.
			if (pa.frise.addClipRequested) {
				ProposerClips(c.ui, d, pa.frise.requestTrack, pa.frise.requestTime);
				d.choix = true;
			}
			const editorkit::NkPaintRect &bt = pa.frise.buttons[(uint8)editorkit::NkTimelineButton::AddTrack];
			// Le choix des CLIPS s'ouvre sous le « + » de sa piste ; celui des pistes sous « + Piste ».
			NkRect ancre{bt.x, bt.y, bt.w, bt.h};
			for (uint32 k = 0; d.choixClips && k < (uint32)pa.frise.rows.Size(); ++k) {
				if (pa.frise.rows[k].track == d.choixPiste) {
					const editorkit::NkPaintRect &kb = pa.frise.rows[k].keyButton;
					ancre = NkRect{kb.x, kb.y, kb.w, kb.h};
				}
			}
			DessinerChoix(c, d, ancre, choixOuvert);
			if (d.frise.tracks.Empty() && !d.choix) {
				const NkRect aide{pa.frise.area.x, pa.frise.area.y + 30.f, pa.frise.area.w, 40.f};
				renderer::NkTexteDansBoite(dl, c.police, aide,
										   "« + Piste » : une propriété (position, rotation, couleur, image…). Double-clic : une clé.",
										   c.pal.attenue);
			}
		}

		// (2026-10-02, R30) Les propositions de « + Piste » et leur ajout, pour le banc
		// du squelette (les memes fonctions que le choix de la page).
		void NkEditeurProposerPistes(NkEditeurModele &m, NkDocAnim &d) {
			Proposer(m, d);
		}

		void NkEditeurAjouterPisteProposee(NkEditeurModele &m, NkDocAnim &d, const NkProposition &p) {
			AjouterPiste(m, d, p);
		}

	} // namespace editeur
} // namespace nkentseu
