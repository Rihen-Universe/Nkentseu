// =============================================================================
// NKUIDesign/NkDocPoolControls.h
// Les controles du pool de chaines du document (`--pool-controles`).
//
// Application console, comme `--roundtrip-controles` : ne touche ni au GPU ni a
// l'ecran, donc tourne sur une machine d'integration. Meme raison qu'ailleurs
// dans ce depot : un banc pose sous `tests/` ne s'execute jamais.
//
// Auteur : Rihen. License : Proprietary - All Rights Reserved.
// =============================================================================

#pragma once

#ifndef NKUIDESIGN_NKDOCPOOLCONTROLS_H
#define NKUIDESIGN_NKDOCPOOLCONTROLS_H

#include <cstdio>

#include "NkDocStringPool.h"

namespace nkuidesign {
		namespace poolctl {

			static int s_pass = 0;
			static int s_fail = 0;

			inline void Check(bool ok, const char *what) {
				if (ok) {
					++s_pass;
				} else {
					++s_fail;
					printf("  FAIL %s\n", what);
				}
			}

			inline bool SameText(const char *a, const char *b) {
				if (!a || !b) {
					return false;
				}
				while (*a && *b) {
					if (*a != *b) {
						return false;
					}
					++a;
					++b;
				}
				return *a == *b;
			}

			// -----------------------------------------------------------------
			// P1 -- STABILITE DES ADRESSES : le controle qui compte vraiment
			// -----------------------------------------------------------------
			// Un pool naif (NkVector<NkString>) RELOGE ses elements en
			// grandissant, et tous les pointeurs deja distribues deviennent
			// pendouillants. Le document ne se corromprait pas a la premiere
			// metrique mais a la Nieme -- le pire des calendriers.
			//
			// On interne donc BEAUCOUP, en gardant les tout premiers pointeurs,
			// et on verifie qu'ils disent encore la verite a la fin.
			inline void P1_AdressesStables() {
				printf("[P1] Les adresses distribuees survivent a la croissance du pool\n");

				NkDocStringPool pool;

				const char *premier = pool.Intern("largeur_palette");
				const char *second = pool.Intern("gouttiere_ligne");

				// Assez pour forcer plusieurs reallocations du vecteur interne.
				for (int i = 0; i < 512; ++i) {
					char buf[32];
					snprintf(buf, sizeof(buf), "metrique_%d", i);
					const char *p = pool.Intern(buf);
					Check(SameText(p, buf), "une entree fraiche doit se relire");
				}

				// LE point : les deux premiers pointeurs, distribues avant des
				// centaines de reallocations, doivent etre intacts.
				Check(SameText(premier, "largeur_palette"), "P1 premier pointeur survit");
				Check(SameText(second, "gouttiere_ligne"), "P1 second pointeur survit");
				Check(pool.Size() == 514, "P1 compte des entrees possedees");
			}

			// -----------------------------------------------------------------
			// P2 -- IDENTITE : PAS de deduplication, et c'est un CONTRAT
			// -----------------------------------------------------------------
			// Dedupliquer ferait marcher la comparaison d'adresse comme test
			// d'egalite de nom -- mais seulement A L'INTERIEUR d'un document.
			// Deux documents, une copie, un import, et elle redevient fausse. On
			// refuse donc l'egalite d'adresse PARTOUT, pour que le premier qui
			// l'essaie echoue tout de suite plutot que trois mois plus tard.
			inline void P2_PasDeDeduplication() {
				printf("[P2] Deux fois le meme nom -> deux adresses DIFFERENTES (contrat)\n");

				NkDocStringPool pool;
				const char *a = pool.Intern("largeur_palette");
				const char *b = pool.Intern("largeur_palette");

				Check(a != b, "P2 l'egalite d'adresse ne doit JAMAIS marcher");
				Check(SameText(a, b), "P2 le CONTENU, lui, est bien le meme");
				Check(pool.Size() == 2, "P2 deux entrees, pas une");
			}

			// -----------------------------------------------------------------
			// P3 -- LE CAS LEGITIME : un document SANS metrique
			// -----------------------------------------------------------------
			// La contre-epreuve demandee. Le defaut du kit est la chaine vide ;
			// un document qui ne nomme aucune metrique est parfaitement valide et
			// ne doit RIEN couter. C'est le pendant exact du temoin de
			// non-regression pose sur la correction (c).
			inline void P3_SansMetriqueResteValide() {
				printf("[P3] Un document SANS metrique reste valide et ne coute rien\n");

				NkDocStringPool pool;
				const char *vide = pool.Intern("");
				const char *nul = pool.Intern(static_cast<const char *>(nullptr));

				Check(vide != nullptr, "P3 jamais nullptr");
				Check(nul != nullptr, "P3 jamais nullptr, meme depuis nullptr");
				Check(*vide == 0, "P3 la chaine vide reste vide");
				Check(pool.Size() == 0, "P3 aucune allocation pour du vide");
			}

			// -----------------------------------------------------------------
			// P4 -- LA PREUVE DE (b) : l'aller-retour du NOM
			// -----------------------------------------------------------------
			// C'est le geste exact de la deserialisation : le nom arrive dans une
			// NkString TEMPORAIRE lue de l'archive, on l'interne, la temporaire
			// meurt -- et le champ du document doit toujours dire le bon nom.
			// SANS pool, ce champ pointerait dans une chaine detruite.
			inline void P4_AllerRetourDuNom() {
				printf("[P4] Le nom survit a la mort de la source (le geste de la relecture)\n");

				NkDocStringPool pool;
				const char *champ = nullptr;

				{
					// Portee de la valeur lue depuis l'archive.
					NkString venuDuDisque("largeur_palette");
					champ = pool.Intern(venuDuDisque);
					Check(SameText(champ, "largeur_palette"), "P4 lu correctement");
				} // <- la source meurt ICI

				// Et on remue le tas pour qu'une memoire liberee ne puisse pas
				// passer pour intacte par simple chance.
				NkDocStringPool bruit;
				for (int i = 0; i < 64; ++i) {
					bruit.Intern("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
				}

				Check(SameText(champ, "largeur_palette"), "P4 LE NOM SURVIT A SA SOURCE");
			}

			// -----------------------------------------------------------------
			// P5 -- DUREE DE VIE : le pool possede, et il libere
			// -----------------------------------------------------------------
			inline void P5_DureeDeVie() {
				printf("[P5] Le pool possede ses chaines et les libere\n");

				NkDocStringPool pool;
				pool.Intern("a");
				pool.Intern("b");
				Check(pool.Size() == 2, "P5 deux entrees");

				pool.Clear();
				Check(pool.Size() == 0, "P5 Clear libere tout");

				// Reutilisable apres Clear : le pool n'est pas a usage unique.
				const char *apres = pool.Intern("c");
				Check(SameText(apres, "c"), "P5 utilisable apres Clear");
				Check(pool.Size() == 1, "P5 recompte depuis zero");
			}

			inline int RunPoolControls() {
				setvbuf(stdout, nullptr, _IONBF, 0);
				printf("=========================================================\n");
				printf(" NkUIDesign -- controles du pool de chaines du document\n");
				printf("=========================================================\n\n");

				s_pass = 0;
				s_fail = 0;

				P1_AdressesStables();
				P2_PasDeDeduplication();
				P3_SansMetriqueResteValide();
				P4_AllerRetourDuNom();
				P5_DureeDeVie();

				const int total = s_pass + s_fail;
				printf("\n---------------------------------------------------------\n");
				printf(" CONTROLES POOL : %d / %d\n", s_pass, total);
				printf("---------------------------------------------------------\n");
				return (s_fail == 0) ? 0 : 1;
			}

	} // namespace poolctl
} // namespace nkuidesign

#endif // NKUIDESIGN_NKDOCPOOLCONTROLS_H

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================
