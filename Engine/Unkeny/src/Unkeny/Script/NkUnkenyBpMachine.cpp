// -----------------------------------------------------------------------------
// FICHIER: Unkeny/Script/NkUnkenyBpMachine.cpp
// DESCRIPTION: La machine virtuelle des Blueprints et la table de ses natifs
//              (les fonctions de NkUnkHoteV1, decrites une fois).
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Unkeny/Script/NkUnkenyBpMachine.h"

#include "NKMath/NKMath.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace unkeny {

		const char *NkAppelNatifBp::Texte(uint32 i) const noexcept {
			const int32 k = args[i].i;
			if (k < 0) {
				// Un texte FABRIQUE pendant l'appel : -1 - indice.
				const int64 d = -static_cast<int64>(k) - 1;
				return textes != nullptr && d < static_cast<int64>(textes->Size()) ? (*textes)[static_cast<uint32>(d)].CStr() : "";
			}
			if (module == nullptr || static_cast<uint32>(k) >= module->constantes.Size()) {
				return "";
			}
			return module->constantes[static_cast<uint32>(k)].texte.CStr();
		}

		int32 NkAppelNatifBp::NouveauTexte(const char *texte) const {
			if (textes == nullptr) {
				return -0x7FFFFFFF; // hors de toute table : se lit « »
			}
			textes->PushBack(NkString(texte != nullptr ? texte : ""));
			return -static_cast<int32>(textes->Size());
		}

		// =====================================================================
		// Les natifs : chacun appelle LA table de l'hote, rien d'autre
		// =====================================================================
		namespace {
			constexpr NkTypeBp RIEN = NkTypeBp::NK_RIEN, B_ = NkTypeBp::NK_BOOLEEN, I_ = NkTypeBp::NK_ENTIER,
							   R_ = NkTypeBp::NK_REEL, V_ = NkTypeBp::NK_VEC2, E_ = NkTypeBp::NK_ENTITE,
							   T_ = NkTypeBp::NK_TEXTE, C_ = NkTypeBp::NK_COULEUR;

			bool Afficher(NkAppelNatifBp &a) {
				a.hote->Afficher(a.hote->ctx, a.soi, a.Texte(0));
				return true;
			}
			bool AfficherReel(NkAppelNatifBp &a) {
				char b[192];
				std::snprintf(b, sizeof(b), "%s%g", a.Texte(0), static_cast<double>(a.args[1].x));
				a.hote->Afficher(a.hote->ctx, a.soi, b);
				return true;
			}
			bool Temps(NkAppelNatifBp &a) {
				a.res[0].x = a.hote->Temps(a.hote->ctx);
				return true;
			}
			bool Vivante(NkAppelNatifBp &a) {
				a.res[0].i = a.hote->Vivante(a.hote->ctx, a.Entite(0)) != 0 ? 1 : 0;
				return true;
			}
			bool Detruire(NkAppelNatifBp &a) {
				return a.hote->Detruire(a.hote->ctx, a.Entite(0)) != 0;
			}
			bool ParNom(NkAppelNatifBp &a) {
				NkUnkEntite e{0u};
				const bool ok = a.hote->ParNom(a.hote->ctx, a.Texte(0), &e) != 0;
				a.res[0].e = e.pack;
				return ok;
			}
			bool NomEst(NkAppelNatifBp &a) {
				a.res[0].i = a.hote->NomEst(a.hote->ctx, a.Entite(0), a.Texte(1)) != 0 ? 1 : 0;
				return true;
			}
			bool Activer(NkAppelNatifBp &a) {
				return a.hote->Activer(a.hote->ctx, a.Entite(0), a.args[1].i) != 0;
			}
			bool Position(NkAppelNatifBp &a) {
				return a.hote->Position(a.hote->ctx, a.Entite(0), &a.res[0].x, &a.res[0].y) != 0;
			}
			bool Teleporter(NkAppelNatifBp &a) {
				return a.hote->Teleporter(a.hote->ctx, a.Entite(0), a.args[1].x, a.args[1].y) != 0;
			}
			bool Rotation(NkAppelNatifBp &a) {
				return a.hote->Rotation(a.hote->ctx, a.Entite(0), &a.res[0].x) != 0;
			}
			bool PoserRotation(NkAppelNatifBp &a) {
				return a.hote->PoserRotation(a.hote->ctx, a.Entite(0), a.args[1].x) != 0;
			}
			bool Vitesse(NkAppelNatifBp &a) {
				return a.hote->Vitesse(a.hote->ctx, a.Entite(0), &a.res[0].x, &a.res[0].y) != 0;
			}
			bool PoserVitesse(NkAppelNatifBp &a) {
				return a.hote->PoserVitesse(a.hote->ctx, a.Entite(0), a.args[1].x, a.args[1].y) != 0;
			}
			bool Impulsion(NkAppelNatifBp &a) {
				return a.hote->AppliquerImpulsion(a.hote->ctx, a.Entite(0), a.args[1].x, a.args[1].y) != 0;
			}
			bool Force(NkAppelNatifBp &a) {
				return a.hote->AppliquerForce(a.hote->ctx, a.Entite(0), a.args[1].x, a.args[1].y) != 0;
			}
			bool Couleur(NkAppelNatifBp &a) {
				return a.hote->PoserCouleur(a.hote->ctx, a.Entite(0), static_cast<uint32>(a.args[1].i)) != 0;
			}
			bool Visible(NkAppelNatifBp &a) {
				return a.hote->PoserVisible(a.hote->ctx, a.Entite(0), a.args[1].i) != 0;
			}
			bool JouerClip(NkAppelNatifBp &a) {
				return a.hote->JouerClip(a.hote->ctx, a.Entite(0), a.args[1].i) != 0;
			}
			bool AnimParametre(NkAppelNatifBp &a) {
				return a.hote->AnimParametre(a.hote->ctx, a.Entite(0), a.Texte(1), a.args[2].x) != 0;
			}
			bool AnimDeclencher(NkAppelNatifBp &a) {
				return a.hote->AnimDeclencher(a.hote->ctx, a.Entite(0), a.Texte(1)) != 0;
			}
			bool JouerEffet(NkAppelNatifBp &a) {
				return a.hote->JouerEffet(a.hote->ctx, a.Entite(0)) != 0;
			}
			bool ArreterEffet(NkAppelNatifBp &a) {
				return a.hote->ArreterEffet(a.hote->ctx, a.Entite(0)) != 0;
			}
			bool ValeurAction(NkAppelNatifBp &a) {
				int32 i = -1;
				a.res[0].x = a.hote->ActionParNom(a.hote->ctx, a.Texte(0), &i) != 0 ? a.hote->ValeurAction(a.hote->ctx, i) : 0.f;
				return i >= 0;
			}
			bool ActionEnfoncee(NkAppelNatifBp &a) {
				int32 i = -1;
				a.res[0].i = (a.hote->ActionParNom(a.hote->ctx, a.Texte(0), &i) != 0 && a.hote->ActionEnfoncee(a.hote->ctx, i) != 0) ? 1 : 0;
				return i >= 0;
			}
			bool JouerSon(NkAppelNatifBp &a) {
				return a.hote->JouerSon(a.hote->ctx, a.Texte(0), a.args[1].x) != 0;
			}
			// ── (2026-10-01) Les textes, les couleurs : des CALCULS, sans l'hote ──
			bool Concatener(NkAppelNatifBp &a) {
				const NkString t = NkString(a.Texte(0)) + a.Texte(1);
				a.res[0].i = a.NouveauTexte(t.CStr());
				return true;
			}
			bool TexteDeReel(NkAppelNatifBp &a) {
				char b[48];
				std::snprintf(b, sizeof(b), "%g", static_cast<double>(a.args[0].x));
				a.res[0].i = a.NouveauTexte(b);
				return true;
			}
			bool TexteDeEntier(NkAppelNatifBp &a) {
				char b[24];
				std::snprintf(b, sizeof(b), "%d", static_cast<int>(a.args[0].i));
				a.res[0].i = a.NouveauTexte(b);
				return true;
			}
			bool TextesEgaux(NkAppelNatifBp &a) {
				a.res[0].i = std::strcmp(a.Texte(0), a.Texte(1)) == 0 ? 1 : 0;
				return true;
			}
			uint32 Canal(float32 v) {
				const float32 c = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
				return static_cast<uint32>(c * 255.f + 0.5f);
			}
			bool CouleurRvba(NkAppelNatifBp &a) {
				const uint32 rvba = (Canal(a.args[0].x) << 24) | (Canal(a.args[1].x) << 16) | (Canal(a.args[2].x) << 8) | Canal(a.args[3].x);
				a.res[0].i = static_cast<int32>(rvba);
				return true;
			}
			bool PoserCouleurC(NkAppelNatifBp &a) {
				return a.hote->PoserCouleur(a.hote->ctx, a.Entite(0), static_cast<uint32>(a.args[1].i)) != 0;
			}
			bool NomEntite(NkAppelNatifBp &a) {
				char b[64] = {};
				const bool ok = a.hote->Nom(a.hote->ctx, a.Entite(0), b, sizeof(b)) != 0;
				a.res[0].i = a.NouveauTexte(b);
				return ok;
			}

			/// Le constructeur d'une entree de la table.
			struct B {
					NkNatifBp n;
					B(const char *nom, const char *libelle, const char *categorie, bool (*f)(NkAppelNatifBp &)) {
						n.signature.nom = nom;
						n.libelle = libelle;
						n.categorie = categorie;
						n.appel = f;
					}
					B &P(NkTypeBp t, const char *nom) {
						n.nomsParams[n.signature.nbParams] = nom;
						n.signature.params[n.signature.nbParams++] = t;
						return *this;
					}
					B &R(NkTypeBp t, const char *nom) {
						n.nomsResultats[n.signature.nbResultats] = nom;
						n.signature.resultats[n.signature.nbResultats++] = t;
						return *this;
					}
					B &Pur() {
						n.pur = true;
						return *this;
					}
					B &Ev(uint32 masque) {
						n.signature.evenements = masque;
						return *this;
					}
			};

			struct NkTableNatifs {
					NkNatifBp natifs[64];
					NkSignatureNatifBp signatures[64];
					uint32 nombre = 0u;
					void Ajouter(const B &b) {
						natifs[nombre] = b.n;
						signatures[nombre] = b.n.signature;
						++nombre;
					}
					NkTableNatifs() {
						// ⚠️ LES NOMS SONT LE CONTRAT des modules ecrits : ne jamais en
						//    renommer un. Un natif de plus s'ajoute ; un natif change de
						//    signature sous un NOUVEAU nom.
						Ajouter(B("unkeny.journal.afficher", "Afficher", "Débogage", &Afficher).P(T_, "texte"));
						Ajouter(B("unkeny.journal.afficher_reel", "Afficher une valeur", "Débogage", &AfficherReel)
									.P(T_, "texte")
									.P(R_, "valeur"));
						Ajouter(B("unkeny.temps", "Temps écoulé", "Débogage", &Temps).R(R_, "secondes").Pur());
						Ajouter(B("unkeny.entite.vivante", "Est valide", "Entité", &Vivante).P(E_, "entité").R(B_, "valide").Pur());
						Ajouter(B("unkeny.entite.detruire", "Détruire", "Entité", &Detruire).P(E_, "entité"));
						Ajouter(B("unkeny.entite.par_nom", "Entité par nom", "Entité", &ParNom).P(T_, "nom").R(E_, "entité").Pur());
						Ajouter(B("unkeny.entite.nom_est", "Nom est", "Entité", &NomEst)
									.P(E_, "entité")
									.P(T_, "nom")
									.R(B_, "égal")
									.Pur());
						Ajouter(B("unkeny.entite.activer", "Activer", "Entité", &Activer).P(E_, "entité").P(B_, "active"));
						Ajouter(B("unkeny.transform.position", "Position", "Transform", &Position)
									.P(E_, "entité")
									.R(V_, "position")
									.Pur());
						Ajouter(B("unkeny.transform.teleporter", "Téléporter", "Transform", &Teleporter)
									.P(E_, "entité")
									.P(V_, "position"));
						Ajouter(B("unkeny.transform.rotation", "Rotation", "Transform", &Rotation).P(E_, "entité").R(R_, "radians").Pur());
						Ajouter(B("unkeny.transform.poser_rotation", "Poser la rotation", "Transform", &PoserRotation)
									.P(E_, "entité")
									.P(R_, "radians"));
						Ajouter(B("unkeny.corps.vitesse", "Vitesse", "Corps rigide", &Vitesse).P(E_, "entité").R(V_, "vitesse").Pur());
						Ajouter(B("unkeny.corps.poser_vitesse", "Poser la vitesse", "Corps rigide", &PoserVitesse)
									.P(E_, "entité")
									.P(V_, "vitesse"));
						Ajouter(B("unkeny.corps.impulsion", "Appliquer une impulsion", "Corps rigide", &Impulsion)
									.P(E_, "entité")
									.P(V_, "impulsion"));
						Ajouter(B("unkeny.corps.force", "Appliquer une force", "Corps rigide", &Force)
									.P(E_, "entité")
									.P(V_, "force")
									.Ev(NK_BP_PAS_FIXE_SEUL));
						Ajouter(B("unkeny.sprite.couleur", "Poser la couleur", "Sprite", &Couleur).P(E_, "entité").P(I_, "rgba"));
						Ajouter(B("unkeny.sprite.visible", "Poser visible", "Sprite", &Visible).P(E_, "entité").P(B_, "visible"));
						Ajouter(B("unkeny.anim.jouer_clip", "Jouer un clip", "Animation", &JouerClip).P(E_, "entité").P(I_, "clip"));
						Ajouter(B("unkeny.anim.parametre", "Paramètre d'animateur", "Animation", &AnimParametre)
									.P(E_, "entité")
									.P(T_, "nom")
									.P(R_, "valeur"));
						Ajouter(B("unkeny.anim.declencher", "Déclencher (animateur)", "Animation", &AnimDeclencher)
									.P(E_, "entité")
									.P(T_, "nom"));
						Ajouter(B("unkeny.effet.jouer", "Jouer l'effet", "Effets", &JouerEffet).P(E_, "entité"));
						Ajouter(B("unkeny.effet.arreter", "Arrêter l'effet", "Effets", &ArreterEffet).P(E_, "entité"));
						Ajouter(B("unkeny.entree.valeur", "Valeur d'action", "Entrée", &ValeurAction).P(T_, "action").R(R_, "valeur").Pur());
						Ajouter(B("unkeny.entree.enfoncee", "Action enfoncée", "Entrée", &ActionEnfoncee)
									.P(T_, "action")
									.R(B_, "enfoncée")
									.Pur());
						Ajouter(B("unkeny.son.jouer", "Jouer un son", "Son", &JouerSon).P(T_, "son").P(R_, "volume"));
						// (2026-10-01) Les textes et les couleurs (editeur de Blueprint a la UE5).
						Ajouter(B("unkeny.texte.concatener", "Concaténer", "Texte", &Concatener)
									.P(T_, "a")
									.P(T_, "b")
									.R(T_, "texte")
									.Pur());
						Ajouter(B("unkeny.texte.de_reel", "Réel en texte", "Texte", &TexteDeReel).P(R_, "valeur").R(T_, "texte").Pur());
						Ajouter(B("unkeny.texte.de_entier", "Entier en texte", "Texte", &TexteDeEntier).P(I_, "valeur").R(T_, "texte").Pur());
						Ajouter(B("unkeny.texte.egaux", "Textes égaux", "Texte", &TextesEgaux).P(T_, "a").P(T_, "b").R(B_, "égaux").Pur());
						Ajouter(B("unkeny.couleur.rvba", "Couleur RVBA", "Couleur", &CouleurRvba)
									.P(R_, "r")
									.P(R_, "v")
									.P(R_, "b")
									.P(R_, "a")
									.R(C_, "couleur")
									.Pur());
						Ajouter(B("unkeny.sprite.poser_couleur", "Poser la couleur (couleur)", "Sprite", &PoserCouleurC)
									.P(E_, "entité")
									.P(C_, "couleur"));
						Ajouter(B("unkeny.entite.nom", "Nom de l'entité", "Entité", &NomEntite).P(E_, "entité").R(T_, "nom").Pur());
					}
			};

			const NkTableNatifs &Table() {
				static const NkTableNatifs t;
				return t;
			}
		} // namespace

		const NkNatifBp *NkNatifsBp(uint32 &nombre) noexcept {
			nombre = Table().nombre;
			return Table().natifs;
		}

		const NkSignatureNatifBp *NkSignaturesBp(uint32 &nombre) noexcept {
			nombre = Table().nombre;
			return Table().signatures;
		}

		int32 NkTrouverNatifBp(const char *nom) noexcept {
			const NkTableNatifs &t = Table();
			for (uint32 i = 0; nom != nullptr && i < t.nombre; ++i) {
				if (std::strcmp(t.natifs[i].signature.nom, nom) == 0) {
					return static_cast<int32>(i);
				}
			}
			return -1;
		}

		bool NkPreparerProgrammeBp(NkProgrammeBp &p, NkRefusBp &refus) {
			uint32 n = 0;
			const NkSignatureNatifBp *s = NkSignaturesBp(n);
			p.pret = NkVerifierModuleBp(p.module, s, n, p.natifs, refus);
			return p.pret;
		}

		// =====================================================================
		// L'interpreteur
		// =====================================================================
		namespace {
			/// Une execution : le programme, le cadre, la faute -- et la pile des
			/// appels de fonctions, empilee dans le MEME tampon de registres.
			struct NkExecution {
					const NkProgrammeBp &p;
					NkCadreBp &c;
					NkFauteBp &faute;
					const NkNatifBp *table = nullptr;
					bool Echec(const char *raison, uint32 f, uint32 pc) {
						faute.raison = raison;
						faute.fonction = f;
						faute.pc = pc;
						faute.noeud = p.module.NoeudDe(f, pc);
						return false;
					}

					/// La fonction `f`, son cadre a partir du registre `base` (ses
					/// parametres y sont deja). false = faute.
					bool Executer(uint32 f, uint32 base, uint32 profondeur) {
						const NkModuleBp &m = p.module;
						if (profondeur >= NK_BP_PILE_MAX) {
							return Echec("pile d'appels trop profonde (recursion sans fin ?)", f, 0u);
						}
						const NkFonctionBp &fn = m.fonctions[f];
						NkVector<NkValeurBp> &regs = *c.registres;
						const uint32 nreg = static_cast<uint32>(fn.registres.Size());
						if (regs.Size() < base + nreg) {
							regs.Resize(base + nreg);
						}
						// Les registres au-dela des parametres repartent de zero (les
						// resultats compris : un chemin sans « Retour » rend 0).
						for (uint32 i = static_cast<uint32>(fn.params.Size()); i < nreg; ++i) {
							regs[base + i] = NkValeurBp();
						}
						NkValeurBp *r = regs.Data() + base;
						const uint32 *code = fn.code.Data();
						const uint32 taille = static_cast<uint32>(fn.code.Size());
						NkValeurBp args[8];
						NkValeurBp res[2];
						uint32 noeudTrace = 0xFFFFFFFFu;
						uint32 pc = 0;
						while (pc < taille) {
							if (++c.instructions > c.budget) {
								return Echec("budget d'instructions epuise (boucle sans fin ?)", f, pc);
							}
							const uint32 at = pc;
							if (c.trace != nullptr) {
								const uint32 n = m.NoeudDe(f, pc);
								if (n != noeudTrace) {
									noeudTrace = n;
									if (n != 0u) {
										c.trace(c.traceDonnees, f, n);
									}
								}
							}
							const NkOpBp op = static_cast<NkOpBp>(code[pc++]);
							switch (op) {
								case NkOpBp::NK_FIN:
									return true;
								case NkOpBp::NK_CONST: {
									const uint32 d = code[pc++], k = code[pc++];
									r[d] = m.constantes[k].valeur;
									if (m.constantes[k].type == NkTypeBp::NK_TEXTE) {
										r[d].i = static_cast<int32>(k);
									}
									break;
								}
								case NkOpBp::NK_COPIER: {
									const uint32 d = code[pc++], s = code[pc++];
									r[d] = r[s];
									break;
								}
								case NkOpBp::NK_LIRE_VAR: {
									const uint32 d = code[pc++], v = code[pc++];
									r[d] = c.variables[v];
									break;
								}
								case NkOpBp::NK_ECRIRE_VAR: {
									const uint32 v = code[pc++], s = code[pc++];
									c.variables[v] = r[s];
									break;
								}
								case NkOpBp::NK_SOI:
									r[code[pc++]].e = c.soi.pack;
									break;
								case NkOpBp::NK_ARG: {
									const uint32 d = code[pc++], n = code[pc++];
									NkValeurBp v;
									if (c.ev != nullptr) {
										switch (static_cast<NkArgBp>(n)) {
											case NkArgBp::NK_DT:
												v.x = c.ev->dt;
												break;
											case NkArgBp::NK_AUTRE:
												v.e = c.ev->autre.pack;
												break;
											case NkArgBp::NK_SOI_EST_ZONE:
												v.i = c.ev->soiEstLaZone != 0 ? 1 : 0;
												break;
											case NkArgBp::NK_VALEUR:
												v.x = c.ev->valeur;
												break;
											default:
												break;
										}
									}
									r[d] = v;
									break;
								}
								case NkOpBp::NK_ADD_R:
								case NkOpBp::NK_SUB_R:
								case NkOpBp::NK_MUL_R:
								case NkOpBp::NK_DIV_R: {
									const uint32 d = code[pc++], a = code[pc++], b = code[pc++];
									const float32 x = r[a].x, y = r[b].x;
									r[d] = NkValeurBp();
									r[d].x = op == NkOpBp::NK_ADD_R	  ? x + y
											 : op == NkOpBp::NK_SUB_R ? x - y
											 : op == NkOpBp::NK_MUL_R ? x * y
																	  : (y != 0.f ? x / y : 0.f);
									break;
								}
								case NkOpBp::NK_ADD_I:
								case NkOpBp::NK_SUB_I:
								case NkOpBp::NK_MUL_I:
								case NkOpBp::NK_DIV_I: {
									const uint32 d = code[pc++], a = code[pc++], b = code[pc++];
									const int32 x = r[a].i, y = r[b].i;
									if (op == NkOpBp::NK_DIV_I && y == 0) {
										return Echec("division entiere par zero", f, at);
									}
									r[d] = NkValeurBp();
									r[d].i = op == NkOpBp::NK_ADD_I	  ? x + y
											 : op == NkOpBp::NK_SUB_I ? x - y
											 : op == NkOpBp::NK_MUL_I ? x * y
																	  : x / y;
									break;
								}
								case NkOpBp::NK_ADD_V:
								case NkOpBp::NK_SUB_V: {
									const uint32 d = code[pc++], a = code[pc++], b = code[pc++];
									const float32 s = op == NkOpBp::NK_ADD_V ? 1.f : -1.f;
									const float32 x = r[a].x + s * r[b].x, y = r[a].y + s * r[b].y;
									r[d] = NkValeurBp();
									r[d].x = x;
									r[d].y = y;
									break;
								}
								case NkOpBp::NK_MUL_VR: {
									const uint32 d = code[pc++], a = code[pc++], b = code[pc++];
									const float32 x = r[a].x * r[b].x, y = r[a].y * r[b].x;
									r[d] = NkValeurBp();
									r[d].x = x;
									r[d].y = y;
									break;
								}
								case NkOpBp::NK_LT_R:
								case NkOpBp::NK_LE_R:
								case NkOpBp::NK_EQ_R: {
									const uint32 d = code[pc++], a = code[pc++], b = code[pc++];
									const float32 x = r[a].x, y = r[b].x;
									const bool v = op == NkOpBp::NK_LT_R ? x < y : op == NkOpBp::NK_LE_R ? x <= y : x == y;
									r[d] = NkValeurBp();
									r[d].i = v ? 1 : 0;
									break;
								}
								case NkOpBp::NK_LT_I:
								case NkOpBp::NK_LE_I:
								case NkOpBp::NK_EQ_I: {
									const uint32 d = code[pc++], a = code[pc++], b = code[pc++];
									const int32 x = r[a].i, y = r[b].i;
									const bool v = op == NkOpBp::NK_LT_I ? x < y : op == NkOpBp::NK_LE_I ? x <= y : x == y;
									r[d] = NkValeurBp();
									r[d].i = v ? 1 : 0;
									break;
								}
								case NkOpBp::NK_EQ_E: {
									const uint32 d = code[pc++], a = code[pc++], b = code[pc++];
									const bool v = r[a].e == r[b].e;
									r[d] = NkValeurBp();
									r[d].i = v ? 1 : 0;
									break;
								}
								case NkOpBp::NK_ET:
								case NkOpBp::NK_OU: {
									const uint32 d = code[pc++], a = code[pc++], b = code[pc++];
									const bool v = op == NkOpBp::NK_ET ? (r[a].i != 0 && r[b].i != 0) : (r[a].i != 0 || r[b].i != 0);
									r[d] = NkValeurBp();
									r[d].i = v ? 1 : 0;
									break;
								}
								case NkOpBp::NK_NON: {
									const uint32 d = code[pc++], a = code[pc++];
									const bool v = r[a].i == 0;
									r[d] = NkValeurBp();
									r[d].i = v ? 1 : 0;
									break;
								}
								case NkOpBp::NK_I2R: {
									const uint32 d = code[pc++], a = code[pc++];
									const float32 x = static_cast<float32>(r[a].i);
									r[d] = NkValeurBp();
									r[d].x = x;
									break;
								}
								case NkOpBp::NK_VEC2: {
									const uint32 d = code[pc++], a = code[pc++], b = code[pc++];
									const float32 x = r[a].x, y = r[b].x;
									r[d] = NkValeurBp();
									r[d].x = x;
									r[d].y = y;
									break;
								}
								case NkOpBp::NK_VX:
								case NkOpBp::NK_VY:
								case NkOpBp::NK_LONGUEUR: {
									const uint32 d = code[pc++], a = code[pc++];
									const float32 x = r[a].x, y = r[a].y;
									r[d] = NkValeurBp();
									r[d].x = op == NkOpBp::NK_VX ? x : op == NkOpBp::NK_VY ? y : math::NkSqrt(x * x + y * y);
									break;
								}
								case NkOpBp::NK_NORMALISER: {
									const uint32 d = code[pc++], a = code[pc++];
									const float32 x = r[a].x, y = r[a].y;
									const float32 l = math::NkSqrt(x * x + y * y);
									r[d] = NkValeurBp();
									if (l > 1e-12f) {
										r[d].x = x / l;
										r[d].y = y / l;
									}
									break;
								}
								case NkOpBp::NK_SAUT:
									pc = code[pc];
									break;
								case NkOpBp::NK_SAUT_SI_FAUX: {
									const uint32 a = code[pc++], cible = code[pc++];
									if (r[a].i == 0) {
										pc = cible;
									}
									break;
								}
								case NkOpBp::NK_NATIF: {
									const uint32 k = code[pc++];
									const NkImportBp &x = m.imports[k];
									const NkNatifBp &n = table[p.natifs[k]];
									const uint32 np = static_cast<uint32>(x.params.Size()), nr = static_cast<uint32>(x.resultats.Size());
									for (uint32 i = 0; i < np; ++i) {
										args[i] = r[code[pc++]];
									}
									res[0] = NkValeurBp();
									res[1] = NkValeurBp();
									NkAppelNatifBp appel;
									appel.hote = c.hote;
									appel.soi = c.soi;
									appel.args = args;
									appel.res = res;
									appel.module = &m;
									appel.textes = c.textes;
									if (!n.appel(appel)) {
										++c.refus;
									}
									for (uint32 i = 0; i < nr; ++i) {
										r[code[pc++]] = res[i];
									}
									break;
								}
								case NkOpBp::NK_APPEL: {
									// Le cadre de l'appele s'empile JUSTE AU-DESSUS du notre.
									const uint32 g = code[pc++];
									const NkFonctionBp &cible = m.fonctions[g];
									const uint32 np = static_cast<uint32>(cible.params.Size());
									const uint32 nr = static_cast<uint32>(cible.resultats.Size());
									const uint32 base2 = base + nreg;
									const uint32 nreg2 = static_cast<uint32>(cible.registres.Size());
									if (regs.Size() < base2 + nreg2) {
										regs.Resize(base2 + nreg2);
										r = regs.Data() + base; // le tampon a pu bouger
									}
									for (uint32 i = 0; i < np; ++i) {
										regs[base2 + i] = r[code[pc++]];
									}
									if (!Executer(g, base2, profondeur + 1u)) {
										return false;
									}
									r = regs.Data() + base; // l'appele a pu agrandir le tampon
									for (uint32 i = 0; i < nr; ++i) {
										r[code[pc++]] = regs[base2 + np + i];
									}
									if (c.trace != nullptr) {
										noeudTrace = 0xFFFFFFFFu; // de retour : le noeud d'appel se revoit
									}
									break;
								}
								case NkOpBp::NK_DIFFUSER: {
									const uint32 rc = code[pc++], k = code[pc++], n = code[pc++];
									NkTypeBp types[8];
									for (uint32 i = 0; i < n; ++i) {
										const uint32 ra = code[pc++];
										args[i] = r[ra];
										types[i] = fn.registres[ra];
									}
									NkUnkEntite cible;
									cible.pack = r[rc].e;
									if (c.diffuser == nullptr ||
										!c.diffuser(c.diffuserDonnees, cible, m.constantes[k].texte.CStr(), args, n, types)) {
										++c.refus;
									}
									break;
								}
								default:
									return Echec("opcode inconnu", f, at);
							}
						}
						return true;
					}
			};
		} // namespace

		bool NkExecuterBp(const NkProgrammeBp &p, uint32 f, NkCadreBp &c, NkFauteBp &faute) {
			const NkModuleBp &m = p.module;
			if (!p.pret || f >= m.fonctions.Size() || c.registres == nullptr) {
				faute.raison = "programme non verifie";
				faute.fonction = f;
				faute.pc = 0u;
				faute.noeud = 0u;
				return false;
			}
			NkExecution x{p, c, faute};
			uint32 n = 0;
			x.table = NkNatifsBp(n);
			c.instructions = 0u;
			// Les parametres d'un evenement personnalise : dans les premiers
			// registres (la signature de la fonction les type ; un nombre ou un
			// type discordant a ete refuse par l'hote avant l'appel).
			const NkFonctionBp &fn = m.fonctions[f];
			NkVector<NkValeurBp> &regs = *c.registres;
			if (regs.Size() < fn.registres.Size()) {
				regs.Resize(fn.registres.Size());
			}
			for (uint32 i = 0; i < fn.params.Size(); ++i) {
				regs[i] = (c.arguments != nullptr && i < c.nbArguments) ? c.arguments[i] : NkValeurBp();
			}
			return x.Executer(f, 0u, 0u);
		}

		// =====================================================================
		// L'assembleur
		// =====================================================================
		int32 NkAssembleurBp::Import(const char *nom) {
			for (uint32 i = 0; i < module.imports.Size(); ++i) {
				if (std::strcmp(module.imports[i].nom.CStr(), nom) == 0) {
					return static_cast<int32>(i);
				}
			}
			const int32 k = NkTrouverNatifBp(nom);
			if (k < 0) {
				return -1;
			}
			uint32 n = 0;
			const NkSignatureNatifBp &s = NkSignaturesBp(n)[k];
			NkImportBp x;
			x.nom = nom;
			for (uint8 i = 0; i < s.nbParams; ++i) {
				x.params.PushBack(s.params[i]);
			}
			for (uint8 i = 0; i < s.nbResultats; ++i) {
				x.resultats.PushBack(s.resultats[i]);
			}
			module.imports.PushBack(x);
			return static_cast<int32>(module.imports.Size() - 1u);
		}

		uint32 NkAssembleurBp::Constante(const NkConstanteBp &c) {
			for (uint32 i = 0; i < module.constantes.Size(); ++i) {
				const NkConstanteBp &k = module.constantes[i];
				if (k.type == c.type && k.valeur.x == c.valeur.x && k.valeur.y == c.valeur.y && k.valeur.i == c.valeur.i &&
					k.valeur.e == c.valeur.e && k.texte == c.texte) {
					return i;
				}
			}
			module.constantes.PushBack(c);
			return static_cast<uint32>(module.constantes.Size() - 1u);
		}
		uint32 NkAssembleurBp::ConstReel(float32 v) {
			NkConstanteBp c;
			c.type = NkTypeBp::NK_REEL;
			c.valeur.x = v;
			return Constante(c);
		}
		uint32 NkAssembleurBp::ConstEntier(int32 v) {
			NkConstanteBp c;
			c.type = NkTypeBp::NK_ENTIER;
			c.valeur.i = v;
			return Constante(c);
		}
		uint32 NkAssembleurBp::ConstBooleen(bool v) {
			NkConstanteBp c;
			c.type = NkTypeBp::NK_BOOLEEN;
			c.valeur.i = v ? 1 : 0;
			return Constante(c);
		}
		uint32 NkAssembleurBp::ConstVec2(float32 x, float32 y) {
			NkConstanteBp c;
			c.type = NkTypeBp::NK_VEC2;
			c.valeur.x = x;
			c.valeur.y = y;
			return Constante(c);
		}
		uint32 NkAssembleurBp::ConstCouleur(uint32 rvba) {
			NkConstanteBp c;
			c.type = NkTypeBp::NK_COULEUR;
			c.valeur.i = static_cast<int32>(rvba);
			return Constante(c);
		}
		uint32 NkAssembleurBp::ConstTexte(const char *texte) {
			NkConstanteBp c;
			c.type = NkTypeBp::NK_TEXTE;
			c.texte = texte != nullptr ? texte : "";
			return Constante(c);
		}
		uint32 NkAssembleurBp::Variable(const char *nom, NkTypeBp type, const NkValeurBp &defaut, bool exposee) {
			for (uint32 i = 0; i < module.variables.Size(); ++i) {
				if (std::strcmp(module.variables[i].nom.CStr(), nom) == 0) {
					return i;
				}
			}
			NkVariableBp v;
			v.nom = nom;
			v.type = type;
			v.defaut = defaut;
			v.exposee = exposee;
			module.variables.PushBack(v);
			return static_cast<uint32>(module.variables.Size() - 1u);
		}
		uint32 NkAssembleurBp::Fonction(const char *nom) {
			NkFonctionBp f;
			f.nom = nom;
			module.fonctions.PushBack(f);
			mCourante = static_cast<uint32>(module.fonctions.Size() - 1u);
			return mCourante;
		}
		void NkAssembleurBp::Signature(const NkTypeBp *params, uint32 np, const NkTypeBp *resultats, uint32 nr) {
			NkFonctionBp &f = module.fonctions[mCourante];
			f.params.Clear();
			f.resultats.Clear();
			for (uint32 i = 0; i < np; ++i) {
				f.params.PushBack(params[i]);
				f.registres.PushBack(params[i]);
			}
			for (uint32 i = 0; i < nr; ++i) {
				f.resultats.PushBack(resultats[i]);
				f.registres.PushBack(resultats[i]);
			}
		}
		void NkAssembleurBp::Appel(uint32 f, const uint32 *registres, uint32 nombre) {
			Emettre(NkOpBp::NK_APPEL, f);
			for (uint32 i = 0; i < nombre; ++i) {
				module.fonctions[mCourante].code.PushBack(registres[i]);
			}
		}
		uint32 NkAssembleurBp::Registre(NkTypeBp type) {
			module.fonctions[mCourante].registres.PushBack(type);
			return static_cast<uint32>(module.fonctions[mCourante].registres.Size() - 1u);
		}
		uint32 NkAssembleurBp::Pc() const noexcept {
			return mCourante < module.fonctions.Size() ? static_cast<uint32>(module.fonctions[mCourante].code.Size()) : 0u;
		}
		void NkAssembleurBp::Emettre(NkOpBp op) {
			module.fonctions[mCourante].code.PushBack(static_cast<uint32>(op));
		}
		void NkAssembleurBp::Emettre(NkOpBp op, uint32 a) {
			Emettre(op);
			module.fonctions[mCourante].code.PushBack(a);
		}
		void NkAssembleurBp::Emettre(NkOpBp op, uint32 a, uint32 b) {
			Emettre(op, a);
			module.fonctions[mCourante].code.PushBack(b);
		}
		void NkAssembleurBp::Emettre(NkOpBp op, uint32 a, uint32 b, uint32 c) {
			Emettre(op, a, b);
			module.fonctions[mCourante].code.PushBack(c);
		}
		void NkAssembleurBp::Natif(uint32 import, const uint32 *registres, uint32 nombre) {
			Emettre(NkOpBp::NK_NATIF, import);
			for (uint32 i = 0; i < nombre; ++i) {
				module.fonctions[mCourante].code.PushBack(registres[i]);
			}
		}
		void NkAssembleurBp::Patcher(uint32 pc, uint32 valeur) {
			if (pc < module.fonctions[mCourante].code.Size()) {
				module.fonctions[mCourante].code[pc] = valeur;
			}
		}
		void NkAssembleurBp::Ligne(uint32 noeud) {
			NkLigneBp l;
			l.pc = Pc();
			l.noeud = noeud;
			module.fonctions[mCourante].lignes.PushBack(l);
		}
		void NkAssembleurBp::Entree(uint32 genre, uint32 fonction, const char *parametre) {
			NkEntreeBp e;
			e.genre = genre;
			e.fonction = fonction;
			e.parametre = parametre != nullptr ? parametre : "";
			module.entrees.PushBack(e);
		}

	} // namespace unkeny
} // namespace nkentseu
