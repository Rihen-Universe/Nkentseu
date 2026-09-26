#pragma once
// -----------------------------------------------------------------------------
// @File    Engine/NKEditorKit/src/NKEditorKit/NkEditorScriptEvenements.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   UN SCRIPT D'EVENEMENTS, rejoue DANS LE PROCESSUS par le systeme
//          d'evenements de l'application (NkEvents().DispatchEvent) : la souris,
//          le clavier, le texte et le depot de fichiers arrivent par les MEMES
//          rappels que ceux de Windows -- pas par l'etat de NKGui ecrit a la main.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POURQUOI (Q9, 21/09) : le banc du kit disait VERT pour Ctrl+A / C / X pendant
// que Rodolf, dans le modeleur, ne pouvait ni selectionner ni copier. Le banc
// ecrivait `wantCopy` et `mousePos` lui-meme : il passait A COTE des rappels de
// l'application (memoire « Sondes qui ne temoignent pas »). Ce script passe PAR
// eux. Aucune entree n'est injectee dans le systeme d'exploitation : les
// evenements naissent et meurent dans le processus.
//
//   NK_EVENEMENTS="<image>:<cmd>;<image>:<cmd>;..."
//     m:x:y        la souris se deplace        d:x:y   appui gauche (deplace d'abord)
//     u:x:y        relache gauche              r:x:y   clic droit (appui + relache)
//     2:x:y        double-clic gauche          k:<touche>   appui (relache a l'image suivante)
//                  touches : a..z, left, right, home, end, back, del, enter, esc,
//                  avec ctrl+ / shift+ / alt+ devant (« ctrl+c », « shift+left »)
//     h:<mod>      MAINTIENT un modificateur (shift, alt, ctrl) -- il reste
//                  enfonce jusqu'a `r:<mod>`. C'est ce qui rend Maj+clic et
//                  Alt+clic JOUABLES par un banc ; `k:` relache a l'image
//                  suivante et ne le permet pas.
//     l:<mod>      le lache (`l` et non `r` : `r:` est deja le clic droit)
//     t:<texte>    du texte tape, caractere par caractere
//     f:x:y:<chemin>  un fichier lache a (x, y)
// -----------------------------------------------------------------------------

#include "NKEvent/NkEvent.h"
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkDropEvent.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKEditorKit/NkSondeInerte.h" // (25/09) marquer l'image ou CE script a injecte
#include "NKContainers/String/NkString.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace editorkit {

		class NkEditorScriptEvenements {
			public:
				/// Lit NK_EVENEMENTS une fois. Rend faux s'il n'y a rien a jouer.
				bool Charger() {
					if (mCharge)
						return mPas.Size() > 0;
					mCharge = true;
					const char *v = std::getenv("NK_EVENEMENTS");
					if (!v || !*v)
						return false;
					const char *c = v;
					while (*c) {
						const char *f = c;
						while (*f && *f != ';')
							++f;
						Pas p;
						p.image = (int32)std::atoi(c);
						const char *d = c;
						while (d < f && *d != ':')
							++d;
						if (d < f)
							p.cmd = NkString(d + 1, (NkString::SizeType)(f - d - 1));
						if (p.cmd.Length() > 0)
							mPas.PushBack(p);
						c = *f ? f + 1 : f;
					}
					return mPas.Size() > 0;
				}

				/// A appeler UNE fois par image, AVANT le dessin : les evenements dus a
				/// cette image sont rejoues par les rappels de l'application.
				void Tick() {
					if (!Charger())
						return;
					++mImage;
					auto &ev = NkEvents();
					// (25/09) CE QUI EST MARQUE, C'EST L'IMAGE OU L'ON INJECTE VRAIMENT.
					// ⚠️ PREMIER CABLAGE FAUX, ET IL AURAIT TOUT ANNULE : le marquage
					//    etait pose ICI, avant la boucle, donc a CHAQUE image -- la porte
					//    d'inertie aurait laisse passer toutes les entrees humaines en se
					//    croyant active. Un garde-fou qui ne garde rien est pire que pas
					//    de garde-fou : on cesse de se mefier. Le marquage se pose donc
					//    a l'endroit exact ou un evenement part, et nulle part ailleurs.
					// ⚠️ ET LE MAINTIEN COMPTE AUSSI. Entre le `d` et le `u` d'un glisser,
					//    les images intermediaires ne portent AUCUN pas -- mais le bouton
					//    est enfonce, et l'application le lit. Sans cette ligne, la porte
					//    d'inertie relachait le bouton a l'image suivant l'appui : tout
					//    glisser scripte cessait de fonctionner sous `NK_SONDE`, en
					//    silence, et c'est l'outil de mesure lui-meme qu'on aurait casse.
					if (mRelacher.Size() > 0 || mBoutonTenu)
						NkSondeInjecteCetteImage() = true;
					// la touche appuyee a l'image precedente se relache
					for (usize i = 0; i < mRelacher.Size(); ++i) {
						NkKeyReleaseEvent e(mRelacher[i]);
						ev.DispatchEvent(e);
					}
					mRelacher.Clear();
					for (usize i = 0; i < mPas.Size(); ++i) {
						if (mPas[i].image != mImage)
							continue;
						NkSondeInjecteCetteImage() = true;
						const char *c = mPas[i].cmd.CStr();
						const char t = c[0];
						const char *a = c + 2;
						float32 x = 0.f, y = 0.f;
						const char *reste = nullptr;
						if (t == 'm' || t == 'd' || t == 'u' || t == 'r' || t == '2' || t == 'f') {
							x = (float32)std::atof(a);
							const char *q = std::strchr(a, ':');
							y = q ? (float32)std::atof(q + 1) : 0.f;
							reste = q ? std::strchr(q + 1, ':') : nullptr;
						}
						const int32 ix = (int32)x, iy = (int32)y;
						switch (t) {
							case 'm': {
								NkMouseMoveEvent e(ix, iy, ix, iy, 0, 0);
								ev.DispatchEvent(e);
								break;
							}
							case 'd': {
								NkMouseMoveEvent e0(ix, iy, ix, iy, 0, 0);
								ev.DispatchEvent(e0);
								NkMouseButtonPressEvent e(NkMouseButton::NK_MB_LEFT, ix, iy);
								ev.DispatchEvent(e);
								mBoutonTenu = true; // le glisser court jusqu'au relache
								break;
							}
							case 'u': {
								NkMouseMoveEvent e0(ix, iy, ix, iy, 0, 0);
								ev.DispatchEvent(e0);
								NkMouseButtonReleaseEvent e(NkMouseButton::NK_MB_LEFT, ix, iy);
								ev.DispatchEvent(e);
								mBoutonTenu = false;
								break;
							}
							case 'r': {
								NkMouseMoveEvent e0(ix, iy, ix, iy, 0, 0);
								ev.DispatchEvent(e0);
								NkMouseButtonPressEvent e(NkMouseButton::NK_MB_RIGHT, ix, iy);
								ev.DispatchEvent(e);
								mRelacherDroitImage = mImage;
								mDroitX = ix;
								mDroitY = iy;
								break;
							}
							case '2': {
								NkMouseMoveEvent e0(ix, iy, ix, iy, 0, 0);
								ev.DispatchEvent(e0);
								NkMouseDoubleClickEvent e(NkMouseButton::NK_MB_LEFT, ix, iy);
								ev.DispatchEvent(e);
								break;
							}
							case 'k': {
								bool ctrl = false, shift = false, alt = false;
								const char *k = a;
								for (;;) {
									if (std::strncmp(k, "ctrl+", 5) == 0) {
										ctrl = true;
										k += 5;
									} else if (std::strncmp(k, "shift+", 6) == 0) {
										shift = true;
										k += 6;
									} else if (std::strncmp(k, "alt+", 4) == 0) {
										alt = true;
										k += 4;
									} else
										break;
								}
								const NkKey cle = Cle(k);
								NkKeyPressEvent e(cle, NkScancode::NK_SC_UNKNOWN, NkModifierState(ctrl, alt, shift));
								ev.DispatchEvent(e);
								mRelacher.PushBack(cle);
								break;
							}
							// ── (26/09) `h:<mod>` MAINTIENT, `r:<mod>` RELACHE ──────────
							// 🔴 POURQUOI ELLES EXISTENT. Rodolf : « Maj+clic pour
							//    accumuler ne fonctionne pas », « Alt+selection ne
							//    fonctionne pas ». AUCUN banc ne pouvait l'eprouver :
							//    `k:` relache la touche A L'IMAGE SUIVANTE, donc il
							//    etait impossible de TENIR un modificateur pendant un
							//    clic. L'instrument ne savait pas produire le geste
							//    qu'on accusait — et un geste qu'aucun banc ne peut
							//    jouer est un geste que personne ne protege.
							// ⚠️ Elles envoient un VRAI appui de LSHIFT / LALT /
							//    LCTRL, par les memes rappels que Windows : c'est ce
							//    qui alimente `NkEventSystem::UpdateInputState`, donc
							//    `NkInput.IsKeyDown`. Poser l'etat a la main aurait
							//    mesure un chemin que Rodolf n'emprunte jamais.
							// ⚠️ `l` ET NON `r` POUR LE RELACHEMENT : `r:` est DEJA le
							//    clic droit dans ce format, et la ligne de parsing des
							//    coordonnees le traite comme tel. Reutiliser la lettre
							//    aurait fait lire « shift » comme une abscisse -- un
							//    conflit muet que seul le parseur aurait tranche.
							case 'h':
							case 'l': {
								const NkKey m = Modificateur(a);
								if (m != NkKey::NK_UNKNOWN) {
									if (t == 'h') {
										NkKeyPressEvent e(m, NkScancode::NK_SC_UNKNOWN,
														  NkModifierState(false, false, false));
										ev.DispatchEvent(e);
									} else {
										NkKeyReleaseEvent e(m, NkScancode::NK_SC_UNKNOWN, NkModifierState(false, false, false));
										ev.DispatchEvent(e);
									}
								}
								break;
							}
							case 't':
								for (const char *q = a; *q; ++q) {
									NkTextInputEvent e((uint32)(unsigned char)*q);
									ev.DispatchEvent(e);
								}
								break;
							case 'f':
								if (reste) {
									NkDropFileData dd;
									dd.x = ix;
									dd.y = iy;
									dd.AddPath(NkString(reste + 1));
									NkDropFileEvent e(dd);
									ev.DispatchEvent(e);
								}
								break;
							default: break;
						}
						std::printf("[evenements] image=%d %s\n", (int)mImage, c);
						std::fflush(stdout);
					}
					// le relache droit, a l'image qui suit l'appui
					if (mRelacherDroitImage == mImage - 1 && mRelacherDroitImage > 0) {
						NkMouseButtonReleaseEvent e(NkMouseButton::NK_MB_RIGHT, mDroitX, mDroitY);
						ev.DispatchEvent(e);
						mRelacherDroitImage = 0;
					}
				}
				int32 Image() const {
					return mImage;
				}

			private:
				struct Pas {
						int32 image = 0;
						NkString cmd;
				};
				/// Les trois modificateurs, par leur nom. Rend `NK_UNKNOWN` si le
				/// nom n'en designe aucun -- un refus, pas un repli silencieux sur
				/// une touche au hasard.
				static NkKey Modificateur(const char *k) {
					if (std::strcmp(k, "shift") == 0)
						return NkKey::NK_LSHIFT;
					if (std::strcmp(k, "alt") == 0)
						return NkKey::NK_LALT;
					if (std::strcmp(k, "ctrl") == 0)
						return NkKey::NK_LCTRL;
					return NkKey::NK_UNKNOWN;
				}
				static NkKey Cle(const char *k) {
					// ⚠️ NkKey suit les RANGEES DU CLAVIER (A S D F…), pas l'alphabet :
					//    « NK_A + (c - 'a') » visait une autre touche. Table explicite.
					if (k[1] == 0) {
						switch (k[0]) {
							case 'a': return NkKey::NK_A;
							case 'c': return NkKey::NK_C;
							case 'v': return NkKey::NK_V;
							case 'x': return NkKey::NK_X;
							case 'z': return NkKey::NK_Z;
							case 's': return NkKey::NK_S;
							default: break;
						}
					}
					if (std::strcmp(k, "left") == 0)
						return NkKey::NK_LEFT;
					if (std::strcmp(k, "right") == 0)
						return NkKey::NK_RIGHT;
					if (std::strcmp(k, "home") == 0)
						return NkKey::NK_HOME;
					if (std::strcmp(k, "end") == 0)
						return NkKey::NK_END;
					if (std::strcmp(k, "back") == 0)
						return NkKey::NK_BACK;
					if (std::strcmp(k, "del") == 0)
						return NkKey::NK_DELETE;
					if (std::strcmp(k, "enter") == 0)
						return NkKey::NK_ENTER;
					if (std::strcmp(k, "esc") == 0)
						return NkKey::NK_ESCAPE;
					return NkKey::NK_UNKNOWN;
				}
				bool mCharge = false;
				bool mBoutonTenu = false; ///< un `d` sans `u` : le glisser est en cours
				int32 mImage = 0;
				int32 mRelacherDroitImage = 0, mDroitX = 0, mDroitY = 0;
				NkVector<Pas> mPas;
				NkVector<NkKey> mRelacher;
		};

	} // namespace editorkit
} // namespace nkentseu
