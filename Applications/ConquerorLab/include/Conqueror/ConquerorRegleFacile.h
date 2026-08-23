#pragma once
// =============================================================================
// ConquerorRegleFacile.h — ECRIRE DES REGLES EN TROIS FONCTIONS.
//
// LE PROBLEME
// -----------
// `NkcRulesVTable` compte vingt et une fonctions. Sur ces vingt et une, TROIS
// contiennent votre jeu :
//
//     construire le plateau        ce a quoi la partie ressemble au depart
//     enumerer les coups           ce qu'on a le droit de faire
//     appliquer un coup            ce qui se passe quand on le fait
//
// Les dix-huit autres sont les memes pour tout le monde : creer et detruire une
// instance, cloner un etat, le serialiser, le hacher, dire si la partie est
// finie, dire qui a gagne, verifier qu'un coup est legal... Les recopier a
// chaque module, c'est dix-huit occasions de se tromper sur du code qui n'a
// aucun rapport avec le jeu qu'on essaie de concevoir.
//
// CE QUE CE FICHIER FAIT
// ----------------------
// Il fournit un ETAT CONCRET (`Partie`) et genere les dix-huit fonctions a
// partir de lui. Vous ecrivez une structure avec trois methodes, une ligne de
// macro, et vous avez un module complet :
//
//     struct MesRegles {
//         void Construire(Grille &g);
//         void CoupsPossibles(const Partie &p, ListeCoups &out);
//         void Appliquer(Partie &p, const NkcMove &m, Evenements &ev);
//     };
//     NKC_REGLES(MesRegles, "MesRegles", "1.0.0", "Moi")
//
// POURQUOI LES DIX-HUIT AUTRES DEVIENNENT GRATUITES
// -------------------------------------------------
// Parce que l'etat cesse d'etre libre. Le contrat dit « le module choisit sa
// representation interne » — c'est sa force, et c'est ce qui coute cher : tant
// que la structure est inconnue, personne ne peut ecrire sa serialisation ni son
// empreinte a votre place.
//
// Ici, on RENONCE a cette liberte : `Partie` est une structure fixe, plate, sans
// pointeur. Du coup :
//     serialiser  = memcpy
//     cloner      = affectation
//     hacher      = FNV-1a sur les octets, donc identique sur toute plateforme
//     coup legal  = enumerer et comparer
// et ces quatre-la sont correctes par CONSTRUCTION, pas par relecture.
//
// CE QU'ON PERD, ET QUAND IL FAUDRA PARTIR
// ----------------------------------------
// Le jour ou votre moteur a besoin d'un etat que `Partie` ne sait pas porter —
// une pile de pouvoirs, un historique, un cache d'evaluation — ce fichier ne
// suffit plus. Vous reprendrez alors le contrat nu, et vous saurez exactement
// quelles dix-huit fonctions ecrire, parce que vous les aurez vues a l'oeuvre.
//
// C'est un ECHAFAUDAGE, pas une cage. Rien de ce qu'il fait n'est cache : tout
// est inline, dans ce fichier, lisible.
// =============================================================================

#include "Conqueror/ConquerorRulesABI.h"
#include "Conqueror/ConquerorGeometry.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace nkentseu {
	namespace conqueror {
		namespace facile {

			// -----------------------------------------------------------------
			// Allocateurs injectes par l'atelier. Repli malloc/free.
			// -----------------------------------------------------------------
			inline NkcAllocFn &RegleAlloc() noexcept { static NkcAllocFn f = nullptr; return f; }
			inline NkcFreeFn  &RegleFree() noexcept	 { static NkcFreeFn f = nullptr; return f; }

			inline void *RegleMalloc(usize n) noexcept {
				return RegleAlloc() ? RegleAlloc()(n) : std::malloc(n);
			}
			inline void RegleFreeMem(void *p) noexcept {
				if (!p) return;
				if (RegleFree()) RegleFree()(p); else std::free(p);
			}

			/// Bornes du cadre simplifie. Volontairement plus basses que
			/// `kMaxCells` : `Partie` est copiee a chaque essai d'une IA, et une
			/// structure de 512 cases pesait inutilement lourd dans une boucle
			/// chaude. Un plateau plus grand demande le contrat nu.
			inline constexpr int32 kFacileMaxCases = 128;
			inline constexpr int32 kFacileMaxCoups = 256;

			// =================================================================
			/// LA GRILLE — ce qu'on remplit au demarrage, une seule fois.
			// =================================================================
			struct Grille {
					NkcCoord	ou[kFacileMaxCases];
					int8		bloquee[kFacileMaxCases] = {};
					int32		nbCases					 = 0;
					NkcTopology topologie				 = NkcTopology::Square4;
					uint8		nbJoueurs				 = 2;

					struct Depart { uint8 joueur; NkcCoord ou; int8 niveau; };
					Depart departs[kMaxPlayers * 8];
					int32  nbDeparts = 0;

					/// Ajoute une case. Une coordonnee non ajoutee est du
					/// HORS-PLATEAU : c'est `ou[]` qui definit la forme reelle, pas
					/// une boite englobante.
					void Ajouter(NkcCoord c) noexcept {
						if (nbCases >= kFacileMaxCases) return;
						ou[nbCases]		 = c;
						bloquee[nbCases] = 0;
						++nbCases;
					}

					void AjouterRectangle(int32 largeur, int32 hauteur) noexcept {
						for (int32 y = 0; y < hauteur; ++y)
							for (int32 x = 0; x < largeur; ++x) {
								NkcCoord c;
								c.q = static_cast<int16>(x);
								c.r = static_cast<int16>(y);
								Ajouter(c);
							}
					}

					/// Rectangle d'HEXAGONES, avec la conversion odd-r -> axial.
					/// C'est LE piege des plateaux hexagonaux : l'oublier donne un
					/// losange penche, et l'erreur ne se voit qu'a l'ecran.
					void AjouterHexagones(int32 colonnes, int32 lignes) noexcept {
						for (int32 r = 0; r < lignes; ++r)
							for (int32 col = 0; col < colonnes; ++col) {
								NkcCoord c;
								c.q = static_cast<int16>(col - (r >> 1));
								c.r = static_cast<int16>(r);
								Ajouter(c);
							}
					}

					void Bloquer(NkcCoord c) noexcept {
						for (int32 i = 0; i < nbCases; ++i)
							if (ou[i].q == c.q && ou[i].r == c.r) { bloquee[i] = 1; return; }
					}

					void Poser(uint8 joueur, NkcCoord c, int8 niveau = 0) noexcept {
						if (nbDeparts >= static_cast<int32>(kMaxPlayers * 8)) return;
						departs[nbDeparts].joueur = joueur;
						departs[nbDeparts].ou	  = c;
						departs[nbDeparts].niveau = niveau;
						++nbDeparts;
					}

					/// Pose un totem par joueur, aux extremites de la liste. La
					/// SYMETRIE est exigee (REGLES §4.2) : un plateau asymetrique
					/// rend tout ecart de winrate ininterpretable.
					void PoserAuxCoins(uint8 joueurs) noexcept {
						const int32 idx[4] = {0, nbCases - 1, 1, nbCases - 2};
						for (uint8 p = 0; p < joueurs && p < 4; ++p)
							if (idx[p] >= 0 && idx[p] < nbCases) Poser(p, ou[idx[p]]);
					}
			};

			// =================================================================
			/// L'ETAT D'UNE PARTIE. Plat, sans pointeur, copiable par affectation.
			///
			/// C'est cette contrainte qui rend gratuites la serialisation, le
			/// clonage et l'empreinte — et donc le rejeu bit-a-bit.
			// =================================================================
			struct Partie {
					NkcCellView cases[kFacileMaxCases];
					int32		nbCases		= 0;
					uint8		nbJoueurs	= 2;
					uint8		joueur		= 0;	 ///< qui a le trait
					uint8		finie		= 0;
					int8		vainqueur	= -2;	 ///< -1 nul, -2 en cours
					uint32		tour		= 0;
					int32		energie[kMaxPlayers]  = {};
					int32		conquete[kMaxPlayers] = {};	 ///< en DIXIEMES entiers
					int32		totems[kMaxPlayers]	  = {};
					uint64		alea = 0x9E3779B97F4A7C15ull;

					// La geometrie ne change pas en cours de partie : elle est
					// recopiee ici pour que `Partie` se suffise a elle-meme.
					NkcCoord	ou[kFacileMaxCases];
					NkcTopology topologie = NkcTopology::Square4;

					// ---- lire ------------------------------------------------
					int32 Index(NkcCoord c) const noexcept {
						for (int32 i = 0; i < nbCases; ++i)
							if (ou[i].q == c.q && ou[i].r == c.r) return i;
						return -1;
					}
					bool Existe(NkcCoord c) const noexcept { return Index(c) >= 0; }

					int8 Proprietaire(NkcCoord c) const noexcept {
						const int32 i = Index(c);
						return i < 0 ? kCellBlocked : cases[i].owner;
					}
					bool Vide(NkcCoord c) const noexcept {
						return Proprietaire(c) == kCellEmpty;
					}
					bool AMoi(NkcCoord c, uint8 j) const noexcept {
						return Proprietaire(c) == static_cast<int8>(j);
					}
					/// Niveau du totem pose sur `c`, 0 s'il n'y en a pas. Indispensable
					/// des le palier 1 : une FUSION se decide sur les niveaux, et sans
					/// lecteur il fallait passer par `cases[Index(c)].level` -- exact,
					/// mais personne ne le devine en lisant les autres accesseurs.
					int8 Niveau(NkcCoord c) const noexcept {
						const int32 i = Index(c);
						return i < 0 ? 0 : cases[i].level;
					}

					bool Ennemie(NkcCoord c, uint8 j) const noexcept {
						const int8 o = Proprietaire(c);
						return o >= 0 && o != static_cast<int8>(j);
					}

					int32 Voisins(NkcCoord c, NkcCoord *sortie, int32 cap) const noexcept {
						const int32 n = NeighborCount(topologie);
						int32		w = 0;
						for (int32 i = 0; i < n && w < cap; ++i)
							sortie[w++] = Neighbor(topologie, c, i);
						return w;
					}

					// ---- ecrire ----------------------------------------------
					void Poser(NkcCoord c, uint8 j, int8 niveau = 0) noexcept {
						const int32 i = Index(c);
						if (i < 0) return;
						cases[i].owner = static_cast<int8>(j);
						cases[i].level = niveau;
					}
					void Vider(NkcCoord c) noexcept {
						const int32 i = Index(c);
						if (i >= 0) cases[i].owner = kCellEmpty;
					}

					/// Recompte les totems. Appelez-la apres avoir bouge des pieces —
					/// ou laissez le cadre le faire : il la rappelle apres chaque coup.
					void Recompter() noexcept {
						for (uint32 p = 0; p < kMaxPlayers; ++p) totems[p] = 0;
						for (int32 i = 0; i < nbCases; ++i) {
							const int8 o = cases[i].owner;
							if (o >= 0 && o < static_cast<int8>(kMaxPlayers)) ++totems[o];
						}
					}

					void PasserLaMain() noexcept {
						++tour;
						joueur = static_cast<uint8>((joueur + 1) % nbJoueurs);
					}

					void Terminer(int8 gagnant) noexcept {
						finie	  = 1;
						vainqueur = gagnant;
					}

					/// Termine en donnant la victoire au plus grand nombre de totems.
					void TerminerAuxPoints() noexcept {
						int32 meilleur = -1, combien = -1;
						bool  egalite  = false;
						for (uint8 p = 0; p < nbJoueurs; ++p) {
							if (totems[p] > combien)	  { combien = totems[p]; meilleur = p; egalite = false; }
							else if (totems[p] == combien) { egalite = true; }
						}
						Terminer(egalite ? static_cast<int8>(-1) : static_cast<int8>(meilleur));
					}

					/// PRNG porte par l'etat, jamais global (REGLES §17.2) : sans
					/// cela, deux clones d'une position piochent dans le meme flux
					/// et le rejeu meurt.
					uint32 Hasard() noexcept {
						alea ^= alea << 13; alea ^= alea >> 7; alea ^= alea << 17;
						return static_cast<uint32>(alea >> 32);
					}
			};

			// =================================================================
			/// LA LISTE DES COUPS qu'on remplit dans `CoupsPossibles`.
			///
			/// `Dupliquer` met le coup a ZERO avant de le remplir : le contrat
			/// compare les coups OCTET PAR OCTET, et un champ oublie rend un coup
			/// different de lui-meme. C'est l'erreur numero un des premiers
			/// modules, et elle disparait ici.
			// =================================================================
			struct ListeCoups {
					NkcMove coups[kFacileMaxCoups];
					int32	nb = 0;

					void Ajouter(const NkcMove &m) noexcept {
						if (nb < kFacileMaxCoups) coups[nb++] = m;
					}

					void Dupliquer(uint8 joueur, NkcCoord de, NkcCoord vers) noexcept {
						if (nb >= kFacileMaxCoups) return;
						NkcMove &m = coups[nb];
						std::memset(&m, 0, sizeof(m));
						m.kind		  = NkcMoveKind::Duplicate;
						m.player	  = joueur;
						m.from		  = de;
						m.to		  = vers;
						m.targetLevel = -1;
						m.powerId	  = -1;
						++nb;
					}

					/// FUSIONNER : plusieurs totems a vous disparaissent, un seul
					/// reparait en `vers`, un niveau plus haut.
					///
					/// Le raccourci existe pour LA MEME RAISON que `Dupliquer` : le
					/// contrat compare les coups OCTET PAR OCTET, un coup de fusion a
					/// deux champs de plus que les autres (`fuseCount`, `fuseCells`),
					/// et les cases NON UTILISEES du tableau doivent etre a zero --
					/// sinon le coup que votre IA propose n'est pas egal au coup que
					/// `CoupsPossibles` avait genere, et l'atelier le refuse sans que
					/// rien ne soit faux dans votre logique. Le memset est ici.
					///
					/// `vers` est libre : la plupart des regles la prennent parmi les
					/// cases consommees, mais rien ne l'impose.
					/// `niveauVise` = -1 laisse le moteur decider.
					void Fusionner(uint8 joueur, const NkcCoord *cases, int32 nbCases,
								   NkcCoord vers, int8 niveauVise = -1) noexcept {
						if (nbCases <= 0 || nbCases > static_cast<int32>(kMaxFuseCells)) return;
						if (nb >= kFacileMaxCoups) return;
						NkcMove &m = coups[nb];
						std::memset(&m, 0, sizeof(m));
						m.kind		  = NkcMoveKind::Fuse;
						m.player	  = joueur;
						m.to		  = vers;
						m.fuseCount	  = static_cast<uint8>(nbCases);
						m.targetLevel = niveauVise;
						m.powerId	  = -1;
						for (int32 k = 0; k < nbCases; ++k) m.fuseCells[k] = cases[k];
						++nb;
					}

					/// POUVOIR : `de` lance, `vers` subit, `idPouvoir` dit LEQUEL.
					///
					/// LES TROIS CHAMPS COMPTENT, ET LE TROISIEME EST CELUI QU'ON
					/// OUBLIE. Deux pouvoirs differents du meme totem sur la meme
					/// cible ne different QUE par `idPouvoir`. Si vous les generez
					/// tous les deux avec le meme identifiant, ce sont deux coups
					/// identiques : l'atelier n'en montrera qu'un, et vous chercherez
					/// longtemps pourquoi votre second pouvoir « ne marche pas ».
					/// Quand ils different, l'atelier affiche « P0 », « P1 » sur la
					/// case et ouvre le menu « Quel coup ? » si la cible est la meme.
					///
					/// `idPouvoir` vous appartient : le contrat ne lui donne aucun
					/// sens, il le transporte. Numerotez a partir de 0 et gardez la
					/// meme numerotation dans `AppliquerCoup`.
					void Pouvoir(uint8 joueur, NkcCoord de, NkcCoord vers, int16 idPouvoir) noexcept {
						if (nb >= kFacileMaxCoups) return;
						NkcMove &m = coups[nb];
						std::memset(&m, 0, sizeof(m));
						m.kind		  = NkcMoveKind::Power;
						m.player	  = joueur;
						m.from		  = de;
						m.to		  = vers;
						m.powerId	  = idPouvoir;
						m.targetLevel = -1;
						++nb;
					}

					void Passer(uint8 joueur) noexcept {
						if (nb >= kFacileMaxCoups) return;
						NkcMove &m = coups[nb];
						std::memset(&m, 0, sizeof(m));
						m.kind		  = NkcMoveKind::Pass;
						m.player	  = joueur;
						m.targetLevel = -1;
						m.powerId	  = -1;
						++nb;
					}

					bool Vide() const noexcept { return nb == 0; }
			};

			// =================================================================
			/// LES EVENEMENTS. Le moteur DECRIT ce qui s'est produit ; l'atelier
			/// decide comment le montrer. Emettre coute une ligne, et c'est ce qui
			/// fait apparaitre les halos rouges et le bandeau « CASCADE ×N ».
			// =================================================================
			struct Evenements {
					NkcEventSink puits = nullptr;
					void		*user  = nullptr;

					void Emettre(NkcEventKind genre, uint8 joueur, NkcCoord a, NkcCoord b,
								 int32 valeur) noexcept {
						if (!puits) return;	  // une IA ne paie pas le cout des evenements
						NkcEvent e;
						std::memset(&e, 0, sizeof(e));
						e.kind = genre; e.player = joueur; e.a = a; e.b = b; e.value = valeur;
						puits(user, &e);
					}

					void Duplique(uint8 j, NkcCoord de, NkcCoord vers) noexcept {
						Emettre(NkcEventKind::TotemDuplicated, j, de, vers, 0);
					}
					void Retourne(uint8 j, NkcCoord ou, int8 ancien) noexcept {
						Emettre(NkcEventKind::TotemTransformed, j, ou, ou, ancien);
					}
					void Cascade(uint8 j, NkcCoord ou, int32 combien) noexcept {
						Emettre(NkcEventKind::Cascade, j, ou, ou, combien);
					}
					/// FUSION accomplie. `ou` = la case du RESULTAT, `niveau` = celui
					/// du totem qui vient d'apparaitre. L'atelier s'en sert pour
					/// l'animation et pour la ligne « FUSIONNER » du journal : sans
					/// l'emettre, la fusion se joue mais ne se VOIT pas.
					void Fusionne(uint8 j, NkcCoord ou, int32 niveau) noexcept {
						Emettre(NkcEventKind::FusionPerformed, j, ou, ou, niveau);
					}
					void Bloque(uint8 j) noexcept {
						Emettre(NkcEventKind::PlayerBlocked, j, NkcCoord{}, NkcCoord{}, j);
					}
					void FinDePartie(int8 vainqueur) noexcept {
						Emettre(NkcEventKind::GameOver, 0, NkcCoord{}, NkcCoord{}, vainqueur);
					}
			};

			// =================================================================
			/// LES REGLAGES — « toute valeur numerique est un parametre nomme,
			/// modifiable SANS RECOMPILATION » (REGLES §1). C'est la regle de
			/// travail du projet, pas une commodite : une regle ne se defend pas
			/// par argument, elle se refute par simulation de masse — encore
			/// faut-il pouvoir faire varier ce qu'on mesure.
			///
			/// Le cadre en fournit UN d'office, `max_tours`, et il n'est pas
			/// optionnel : sans lui, un jeu de regles pathologique boucle
			/// indefiniment et la campagne ne rend jamais la main. Un taux de
			/// parties coupees non nul SIGNALE cette pathologie (REGLES §12.3) —
			/// encore faut-il que la coupe existe.
			// =================================================================
			inline constexpr int32 kFacileMaxReglages = 24;

			struct Reglage {
					char  cle[32]	 = {};
					char  libelle[64] = {};
					char  groupe[32]  = {};
					bool  booleen	  = false;
					int32 valeur = 0, defaut = 0, mini = 0, maxi = 0;
			};

			struct TableReglages {
					Reglage liste[kFacileMaxReglages];
					int32	nb = 0;

					void Entier(const char *cle, const char *libelle, const char *groupe,
								int32 defaut, int32 mini, int32 maxi) noexcept {
						if (nb >= kFacileMaxReglages) return;
						Reglage &r = liste[nb++];
						Copier(r.cle, sizeof(r.cle), cle);
						Copier(r.libelle, sizeof(r.libelle), libelle);
						Copier(r.groupe, sizeof(r.groupe), groupe);
						r.booleen = false;
						r.defaut = r.valeur = defaut;
						r.mini = mini;
						r.maxi = maxi;
					}

					void Booleen(const char *cle, const char *libelle, const char *groupe,
								 bool defaut) noexcept {
						if (nb >= kFacileMaxReglages) return;
						Reglage &r = liste[nb++];
						Copier(r.cle, sizeof(r.cle), cle);
						Copier(r.libelle, sizeof(r.libelle), libelle);
						Copier(r.groupe, sizeof(r.groupe), groupe);
						r.booleen = true;
						r.defaut = r.valeur = defaut ? 1 : 0;
						r.mini = 0;
						r.maxi = 1;
					}

					int32 Index(const char *cle) const noexcept {
						if (!cle) return -1;
						for (int32 i = 0; i < nb; ++i)
							if (std::strcmp(liste[i].cle, cle) == 0) return i;
						return -1;
					}

					/// La valeur courante, ou `siAbsent` si la cle n'existe pas.
					int32 operator()(const char *cle, int32 siAbsent = 0) const noexcept {
						const int32 i = Index(cle);
						return i < 0 ? siAbsent : liste[i].valeur;
					}

					/// BORNE, ne refuse pas. Le contrat l'exige et le banc d'essai
					/// le verifie : une valeur hors plage doit etre ramenee dans la
					/// plage, pas rejetee — sinon un curseur d'interface qui depasse
					/// laisserait le moteur dans un etat que personne n'a voulu.
					bool Poser(const char *cle, float64 v) noexcept {
						const int32 i = Index(cle);
						if (i < 0) return false;
						int32 x = static_cast<int32>(v < 0 ? v - 0.5 : v + 0.5);
						if (x < liste[i].mini) x = liste[i].mini;
						if (x > liste[i].maxi) x = liste[i].maxi;
						liste[i].valeur = x;
						return true;
					}

				private:
					static void Copier(char *dst, usize cap, const char *src) noexcept {
						usize i = 0;
						if (src) for (; i + 1 < cap && src[i]; ++i) dst[i] = src[i];
						dst[i] = '\0';
					}
			};

			// -----------------------------------------------------------------
			// Detection de la methode optionnelle `Reglages(TableReglages&)`.
			//
			// Sans cela, il faudrait l'imposer a tout le monde — y compris a qui
			// n'a aucun reglage a declarer — et la promesse « trois fonctions »
			// deviendrait « quatre, dont une vide ». Le detour SFINAE coute dix
			// lignes ici et zero au lecteur du fichier d'exemple.
			// -----------------------------------------------------------------
			template <typename U>
			auto AppelerReglages(U &u, TableReglages &t, int32)
				-> decltype(u.Reglages(t), void()) {
				u.Reglages(t);
			}
			template <typename U>
			void AppelerReglages(U &, TableReglages &, long) {}

			// =================================================================
			// LE GENERATEUR — les dix-huit fonctions, ecrites une fois.
			//
			// `T` est la structure du stagiaire. On n'exige rien d'elle par
			// heritage : si une methode manque, l'erreur de compilation nomme la
			// methode manquante, ce qui est plus clair qu'un message de traits.
			// =================================================================
			template <typename T>
			struct Moteur {
					T			  regles;
					Grille		  grille;
					TableReglages reglages;
					Partie	 modele;	 ///< l'etat de depart, construit une fois
					char	 schema[1024];
					char	 plateauJson[8192];

					void Construire() noexcept {
						// `max_tours` EN PREMIER, toujours : c'est le garde-fou du
						// cadre, pas un reglage parmi d'autres.
						reglages = TableReglages();
						reglages.Entier("max_tours", "Tours maximum", "Fin de partie",
										200, 10, 100000);
						AppelerReglages(regles, reglages, 0);

						grille = Grille();
						regles.Construire(grille);

						modele			= Partie();
						modele.nbCases	= grille.nbCases;
						modele.topologie = grille.topologie;
						modele.nbJoueurs = grille.nbJoueurs;
						for (int32 i = 0; i < grille.nbCases; ++i) {
							modele.ou[i]			= grille.ou[i];
							modele.cases[i]			= NkcCellView{};
							modele.cases[i].owner	= grille.bloquee[i] ? kCellBlocked : kCellEmpty;
							modele.cases[i].people	= -1;
							modele.cases[i].power	= -1;
							modele.cases[i].artefact = -1;
						}
					}
			};

		} // namespace facile
	} // namespace conqueror
} // namespace nkentseu

// =============================================================================
// NKC_REGLES — une ligne, et le module est complet.
//
// A ecrire en FIN de fichier, apres votre structure. Elle definit les deux
// symboles exportes et les vingt et une fonctions de la vtable.
// =============================================================================
#define NKC_REGLES(TYPE, NOM, VERSION, AUTEUR)                                        \
	namespace {                                                                       \
		using namespace nkentseu;                                                     \
		using namespace nkentseu::conqueror;                                          \
		using namespace nkentseu::conqueror::facile;                                  \
                                                                                      \
		using MoteurT = Moteur<TYPE>;                                                 \
                                                                                      \
		/* Declaree avant usage : NkcF_Apply l'appelle, et elle-meme appelle          \
		   NkcF_IsBlocked. Deux fonctions qui se citent l'une l'autre demandent       \
		   une declaration prealable, macro ou pas. */                                \
		void NkcF_CheckFin(NkcRules, Partie *, Evenements &);                         \
                                                                                      \
		NkcRules NkcF_Create() {                                                      \
			void *m = RegleMalloc(sizeof(MoteurT));                                   \
			if (!m) return nullptr;                                                   \
			MoteurT *e = new (m) MoteurT();                                           \
			e->Construire();                                                          \
			return static_cast<NkcRules>(e);                                          \
		}                                                                             \
		void NkcF_Destroy(NkcRules s) {                                               \
			if (!s) return;                                                           \
			MoteurT *e = static_cast<MoteurT *>(s);                                   \
			e->~MoteurT();                                                            \
			RegleFreeMem(e);                                                          \
		}                                                                             \
		NkcState NkcF_CreateState(NkcRules) {                                         \
			void *m = RegleMalloc(sizeof(Partie));                                    \
			return m ? static_cast<NkcState>(new (m) Partie()) : nullptr;              \
		}                                                                             \
		void NkcF_DestroyState(NkcRules, NkcState st) {                               \
			if (!st) return;                                                          \
			Partie *p = static_cast<Partie *>(st);                                    \
			p->~Partie();                                                             \
			RegleFreeMem(p);                                                          \
		}                                                                             \
		/* Cloner = affecter. C'est correct parce que Partie est plate. */            \
		void NkcF_CloneState(NkcRules, NkcState d, const NkcState s) {                \
			if (d && s) *static_cast<Partie *>(d) = *static_cast<const Partie *>(s);  \
		}                                                                             \
		int32 NkcF_Setup(NkcRules self, NkcState st, uint8 joueurs, uint64 graine) {  \
			MoteurT *e = static_cast<MoteurT *>(self);                                \
			Partie  *p = static_cast<Partie *>(st);                                   \
			if (!e || !p) return 0;                                                   \
			*p		= e->modele;                                                      \
			p->alea = graine ? graine : 0x9E3779B97F4A7C15ull;                        \
			if (joueurs >= 2 && joueurs <= e->grille.nbJoueurs) p->nbJoueurs = joueurs;\
			for (int32 i = 0; i < e->grille.nbDeparts; ++i) {                         \
				const Grille::Depart &d = e->grille.departs[i];                       \
				if (d.joueur < p->nbJoueurs) p->Poser(d.ou, d.joueur, d.niveau);      \
			}                                                                         \
			p->Recompter();                                                           \
			return 1;                                                                 \
		}                                                                             \
		void NkcF_GetView(NkcRules, const NkcState st, NkcStateView *out) {           \
			const Partie *p = static_cast<const Partie *>(st);                        \
			if (!out || !p) return;                                                   \
			out->cells = p->cases;   out->coords = p->ou;                             \
			out->cellCount = static_cast<uint32>(p->nbCases);                         \
			out->topology = p->topologie;      out->playerCount = p->nbJoueurs;       \
			out->current = p->joueur;          out->finished = p->finie;              \
			out->winner = p->vainqueur;        out->turn = p->tour;                   \
			out->energy = p->energie;          out->conquestTenths = p->conquete;      \
			out->totemCount = p->totems;                                              \
		}                                                                             \
		uint32 NkcF_Generate(NkcRules self, const NkcState st, NkcMove *out,          \
							 uint32 cap) {                                            \
			MoteurT		 *e = static_cast<MoteurT *>(self);                           \
			const Partie *p = static_cast<const Partie *>(st);                        \
			if (!e || !p || p->finie) return 0;                                       \
			ListeCoups l;                                                             \
			e->regles.CoupsPossibles(*p, l);                                          \
			/* PASSER n'est legal que si rien d'autre ne l'est (REGLES §13). */       \
			if (l.Vide()) l.Passer(p->joueur);                                        \
			const int32 n = l.nb;                                                     \
			for (int32 i = 0; i < n && static_cast<uint32>(i) < cap; ++i)             \
				out[i] = l.coups[i];                                                  \
			return static_cast<uint32>(n);                                            \
		}                                                                             \
		/* Legal = present dans la liste. Comparaison octet a octet, d'ou le          \
		   memset systematique dans ListeCoups. */                                    \
		int32 NkcF_IsLegal(NkcRules self, const NkcState st, const NkcMove *mv) {     \
			if (!mv) return 0;                                                        \
			NkcMove		 buf[kFacileMaxCoups];                                        \
			const uint32 n = NkcF_Generate(self, st, buf, kFacileMaxCoups);           \
			const uint32 k = n < kFacileMaxCoups ? n : kFacileMaxCoups;               \
			for (uint32 i = 0; i < k; ++i)                                            \
				if (std::memcmp(&buf[i], mv, sizeof(NkcMove)) == 0) return 1;         \
			return 0;                                                                 \
		}                                                                             \
		int32 NkcF_Apply(NkcRules self, NkcState st, const NkcMove *mv,               \
						 NkcEventSink sink, void *user) {                             \
			MoteurT *e = static_cast<MoteurT *>(self);                                \
			Partie  *p = static_cast<Partie *>(st);                                   \
			if (!e || !p || !mv || p->finie) return 0;                                \
			if (mv->player != p->joueur) return 0;                                    \
			if (!NkcF_IsLegal(self, st, mv)) return 0;                                \
			Evenements ev; ev.puits = sink; ev.user = user;                           \
			if (mv->kind == NkcMoveKind::Pass) { p->PasserLaMain(); }                 \
			else                               { e->regles.Appliquer(*p, *mv, ev); }  \
			p->Recompter();                                                           \
			if (!p->finie) NkcF_CheckFin(self, p, ev);                                \
			return 1;                                                                 \
		}                                                                             \
		int32 NkcF_IsFinished(NkcRules, const NkcState st) {                          \
			return static_cast<const Partie *>(st)->finie ? 1 : 0;                    \
		}                                                                             \
		int32 NkcF_GetWinner(NkcRules, const NkcState st) {                           \
			return static_cast<const Partie *>(st)->vainqueur;                        \
		}                                                                             \
		/* Bloque = aucun coup autre que PASSER. L'atelier verifie l'equivalence      \
		   entre ceci et « aucun coup legal » : ici elle est vraie par                \
		   construction, parce que les deux lisent la meme liste. */                  \
		int32 NkcF_IsBlocked(NkcRules self, const NkcState st, uint8 joueur) {        \
			MoteurT		 *e = static_cast<MoteurT *>(self);                           \
			const Partie *p = static_cast<const Partie *>(st);                        \
			if (!e || !p || joueur >= p->nbJoueurs) return 1;                         \
			Partie vue = *p;                                                          \
			vue.joueur = joueur;                                                      \
			ListeCoups l;                                                             \
			e->regles.CoupsPossibles(vue, l);                                         \
			return l.Vide() ? 1 : 0;                                                  \
		}                                                                             \
		void NkcF_CheckFin(NkcRules self, Partie *p, Evenements &ev) {                     \
			MoteurT *e = static_cast<MoteurT *>(self);                                        \
			for (uint8 j = 0; j < p->nbJoueurs; ++j) {                                        \
				if (NkcF_IsBlocked(self, p, j)) {                                                \
					ev.Bloque(j);                                                                   \
					p->TerminerAuxPoints();                                                         \
					ev.FinDePartie(p->vainqueur);                                                   \
					return;                                                                         \
				}                                                                                \
			}                                                                                 \
			/* GARDE-FOU. Sans lui, un jeu de regles pathologique boucle sans                 \
			   fin et la campagne ne rend jamais la main. On compte en TOURS de               \
			   table : max_tours x nombre de joueurs. */                                      \
			const int32 cap = e ? e->reglages("max_tours", 200) : 200;                        \
			if (static_cast<int32>(p->tour) >= cap * static_cast<int32>(p->nbJoueurs)) {      \
				p->TerminerAuxPoints();                                                          \
				ev.FinDePartie(p->vainqueur);                                                    \
			}                                                                                 \
		}                                                                                  \
		/* Serialiser = memcpy. Correct parce que Partie n'a aucun pointeur. */       \
		uint32 NkcF_Serialize(NkcRules, const NkcState st, void *buf, uint32 cap) {   \
			const uint32 n = static_cast<uint32>(sizeof(Partie));                     \
			if (!buf || cap < n) return n;                                            \
			std::memcpy(buf, st, n);                                                  \
			return n;                                                                 \
		}                                                                             \
		int32 NkcF_Deserialize(NkcRules, NkcState st, const void *buf, uint32 n) {    \
			if (!buf || n != static_cast<uint32>(sizeof(Partie))) return 0;           \
			std::memcpy(st, buf, n);                                                  \
			return 1;                                                                 \
		}                                                                             \
		/* FNV-1a sur les octets qui DECRIVENT la position. On ne hache ni la         \
		   geometrie (constante) ni le PRNG : deux etats identiques doivent           \
		   donner la meme empreinte sur toute plateforme (REGLES §17.3). */           \
		uint64 NkcF_Hash(NkcRules, const NkcState st) {                               \
			const Partie *p = static_cast<const Partie *>(st);                        \
			uint64		  h = 1469598103934665603ull;                                 \
			auto		  mix = [&h](uint64 v) {                                      \
				 for (int32 b = 0; b < 8; ++b) {                                      \
					 h ^= (v >> (b * 8)) & 0xFF;                                      \
					 h *= 1099511628211ull;                                           \
				 }                                                                    \
			};                                                                        \
			for (int32 i = 0; i < p->nbCases; ++i) {                                  \
				mix(static_cast<uint64>(static_cast<uint8>(p->cases[i].owner)));      \
				mix(static_cast<uint64>(static_cast<uint8>(p->cases[i].level)));      \
			}                                                                         \
			mix(p->joueur); mix(p->tour); mix(p->finie);                              \
			return h;                                                                 \
		}                                                                             \
		const char *NkcF_Schema(NkcRules self) {                                           \
			MoteurT *e = static_cast<MoteurT *>(self);                                        \
			char	*w = e->schema;                                                              \
			usize	 left = sizeof(e->schema);                                                  \
			int32	 k = std::snprintf(w, left, "[");                                           \
			w += k; left -= static_cast<usize>(k);                                            \
			for (int32 i = 0; i < e->reglages.nb; ++i) {                                      \
				const Reglage &r = e->reglages.liste[i];                                         \
				k = std::snprintf(w, left,                                                       \
					"%s{\"key\":\"%s\",\"label\":\"%s\",\"group\":\"%s\","                          \
					"\"type\":\"%s\",\"min\":%d,\"max\":%d,\"def\":%d,\"val\":%d}",                 \
					i ? "," : "", r.cle, r.libelle, r.groupe,                                       \
					r.booleen ? "bool" : "int", r.mini, r.maxi, r.defaut, r.valeur);                \
				if (k <= 0 || static_cast<usize>(k) >= left) break;                              \
				w += k; left -= static_cast<usize>(k);                                           \
			}                                                                                 \
			std::snprintf(w, left, "]");                                                      \
			return e->schema;                                                                 \
		}                                                                                  \
		/* BORNE, ne refuse pas : le contrat l'exige et le banc d'essai le                 \
		   verifie. Un curseur d'interface qui depasse ne doit pas laisser le              \
		   moteur dans un etat que personne n'a voulu. */                                  \
		int32 NkcF_SetParam(NkcRules self, const char *cle, float64 v) {                   \
			MoteurT *e = static_cast<MoteurT *>(self);                                        \
			return (e && e->reglages.Poser(cle, v)) ? 1 : 0;                                  \
		}                                                                                  \
		float64 NkcF_GetParam(NkcRules self, const char *cle) {                            \
			MoteurT *e = static_cast<MoteurT *>(self);                                        \
			if (!e) return 0.0;                                                               \
			const int32 i = e->reglages.Index(cle);                                           \
			return i < 0 ? 0.0 : static_cast<float64>(e->reglages.liste[i].valeur);           \
		}                                                                                  \
		/* Plateau construit en C++ : on refuse franchement un chargement plutot      \
		   que de faire semblant. L'atelier l'affiche proprement. */                  \
		int32 NkcF_LoadBoard(NkcRules, const char *) { return 0; }                    \
		const char *NkcF_GetBoard(NkcRules self) {                                    \
			MoteurT *e = static_cast<MoteurT *>(self);                                \
			char	*w = e->plateauJson;                                              \
			usize	 left = sizeof(e->plateauJson);                                   \
			static const char *kTopo[4] = {"HEX_POINTY", "HEX_FLAT", "SQUARE_4",      \
										   "SQUARE_8"};                               \
			int32 k = std::snprintf(w, left, "{\"topology\":\"%s\",\"cells\":[",      \
									kTopo[static_cast<int32>(e->grille.topologie)]);  \
			w += k; left -= static_cast<usize>(k);                                    \
			for (int32 i = 0; i < e->grille.nbCases; ++i) {                           \
				k = std::snprintf(w, left, "%s[%d,%d]", i ? "," : "",                 \
								  e->grille.ou[i].q, e->grille.ou[i].r);              \
				if (k <= 0 || static_cast<usize>(k) >= left) break;                   \
				w += k; left -= static_cast<usize>(k);                                \
			}                                                                         \
			std::snprintf(w, left, "],\"blocked\":[],\"starts\":[],"                  \
								   "\"min_players\":2,\"max_players\":%u}",           \
						  static_cast<unsigned>(e->grille.nbJoueurs));                \
			return e->plateauJson;                                                    \
		}                                                                             \
		void NkcF_FillFactory(NkcRulesFactory *out) {                                 \
			if (!out) return;                                                         \
			std::memset(out, 0, sizeof(*out));                                        \
			std::snprintf(out->info.name, sizeof(out->info.name), "%s", NOM);         \
			std::snprintf(out->info.version, sizeof(out->info.version), "%s", VERSION);\
			std::snprintf(out->info.author, sizeof(out->info.author), "%s", AUTEUR);  \
			out->info.maxPlayers = static_cast<uint8>(kMaxPlayers);                   \
			out->info.palier	 = 0;                                                 \
			out->vtable.Create = NkcF_Create;   out->vtable.Destroy = NkcF_Destroy;   \
			out->vtable.CreateState = NkcF_CreateState;                               \
			out->vtable.DestroyState = NkcF_DestroyState;                             \
			out->vtable.CloneState = NkcF_CloneState;                                 \
			out->vtable.Setup = NkcF_Setup;     out->vtable.GetView = NkcF_GetView;   \
			out->vtable.GenerateLegalMoves = NkcF_Generate;                           \
			out->vtable.IsLegalMove = NkcF_IsLegal;                                   \
			out->vtable.ApplyMove = NkcF_Apply;                                       \
			out->vtable.IsFinished = NkcF_IsFinished;                                 \
			out->vtable.GetWinner = NkcF_GetWinner;                                   \
			out->vtable.IsPlayerBlocked = NkcF_IsBlocked;                             \
			out->vtable.SerializeState = NkcF_Serialize;                              \
			out->vtable.DeserializeState = NkcF_Deserialize;                          \
			out->vtable.HashState = NkcF_Hash;                                        \
			out->vtable.GetParamsSchemaJson = NkcF_Schema;                            \
			out->vtable.SetParam = NkcF_SetParam;                                     \
			out->vtable.GetParam = NkcF_GetParam;                                     \
			out->vtable.LoadBoardJson = NkcF_LoadBoard;                               \
			out->vtable.GetBoardJson = NkcF_GetBoard;                                 \
			NkcRulesStamp(out);                                                       \
		}                                                                             \
	}                                                                                 \
	NKC_MODULE_EXPORT void nkc_rules_set_allocator(                                   \
		nkentseu::conqueror::NkcAllocFn a, nkentseu::conqueror::NkcFreeFn f) {        \
		nkentseu::conqueror::facile::RegleAlloc() = a;                                \
		nkentseu::conqueror::facile::RegleFree()  = f;                                \
	}                                                                                 \
	NKC_MODULE_EXPORT void nkc_rules_get_factory(                                     \
		nkentseu::conqueror::NkcRulesFactory *out) {                                  \
		NkcF_FillFactory(out);                                                        \
	}
