#pragma once
// -----------------------------------------------------------------------------
// @File    NkNodeGraphIO.inl
// @Brief   Serialisation `.nkgraph` et historique annuler/refaire.
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// FORMAT TEXTE, une directive par ligne. Le choix du texte n'est pas de la
// paresse : un graphe se relit, se compare (`git diff`), et se repare a la main
// quand une version future casse quelque chose. Un format binaire ferait gagner
// des octets sur des fichiers qui pesent quelques kilo-octets.
//
//   nkgraph 2
//   compteurs <prochainNoeud> <prochainLien>
//   type <id> <nom...>
//   conv <de> <vers>
//   noeud <id> <x> <y> <cleType> <libelle...>
//   sock <idNoeud> <0=entree|1=sortie> <idType> <nom...>
//   def  <idNoeud> <0=entree|1=sortie> <nomPrise> <valeur...>
//   lien <id> <deNoeud> <nomPriseSortie> <versNoeud> <nomPriseEntree>
//
// ═══════════════════════════════════════════════════════════════════════════
// VERSION 2 (2026-08-23) — UNE PRISE SE DESIGNE PAR SON NOM, PLUS PAR SON RANG
// ═══════════════════════════════════════════════════════════════════════════
// ⚠️ CE QUI A ETE MESURE, ET QUI A DECIDE. En version 1, `lien` et `def` ne
// portaient que des NOMBRES : le rang de la prise dans l'ordre d'ecriture des
// lignes `sock`. Le cas `graphe/index-de-prise-contre-nom` a montre qu'une
// ligne `sock` glissee AVANT une autre repointe le lien vers une prise
// differente — et que `Deserialize` rendait `true`, `Validate` ZERO
// diagnostic. Un graphe silencieusement recable, avec un verdict vert.
//
// Ce n'etait pas une hypothese : le cas le FAIT et lit le nouveau nom.
//
// 🔴 ET CE QUI NOUS PROTEGEAIT N'ETAIT PAS LE FORMAT, C'ETAIT D'EN ETRE LE SEUL
// ECRIVAIN. Notre ecrivain emet toujours prises et liens dans un etat coherent,
// donc l'aller-retour etait sur — une propriete de l'unique producteur qui se
// lisait comme une propriete du format. Un outil tiers, une edition a la main
// ou une fusion de fichiers la faisait tomber, SANS MESSAGE.
//
// 📌 Et la convention etait deja ecrite dans `NkNodeGraph.h`, au-dessus de
// `SetSocketDefault` : « la prise se designe par son NOM et son SENS, jamais
// par son index ». L'API la respectait ; le format non. La regle et sa
// violation cohabitaient dans le meme module.
//
// ── LES TROIS CHOSES QUE LA VERSION 2 GARANTIT ────────────────────────────
// 1. L'ORDRE DES LIGNES `sock` N'A PLUS DE SENS. Un fichier dont les prises
//    sont reordonnees charge A L'IDENTIQUE. Temoin : `fichier/ordre-des-sock`.
// 2. UN NOM ABSENT EST REFUSE EN SE NOMMANT. `Deserialize` rend `false` et
//    remplit `outErreur` avec le nom demande et le noeud. Jamais de repli sur
//    une prise voisine : ce serait « rien » charge comme « la prise 0 ».
// 3. LA VERSION 1 SE LIT ENCORE, par les index, sans conversion. La migration
//    est EXERCEE — neuf fichiers ecrits a la main dans NkMatGraphCheck sont en
//    version 1 et doivent continuer de charger a l'identique.
//
// ⚠️ EN MEMOIRE, `NkLink` GARDE SON INDEX. Le nom est la cle SUR DISQUE ; la
// resolution se fait UNE FOIS au chargement. Un format se lit une fois, un
// graphe s'evalue a chaque image — payer une recherche par nom a chaque
// evaluation serait echanger un defaut rare contre un cout permanent.
//
// LA LIGNE `compteurs` EST LA PLUS IMPORTANTE DU FICHIER. Sans elle, un graphe
// recharge REATTRIBUERAIT les identifiants liberes par une suppression — et une
// courbe d'animation ou une reference sauvegardee ailleurs pointerait
// silencieusement sur un autre noeud. C'est un defaut qui ne se voit qu'au
// resultat, longtemps apres.
// -----------------------------------------------------------------------------

#include <stdio.h>	// snprintf : formatage des nombres, pas de flux
#include "NKCore/Text/NkSnprintf.h"
#include <stdlib.h> // strtod

namespace nkentseu {
	namespace graph {

		namespace detail {

			inline void PutU32(NkString &s, uint32 v) {
				char b[16];
				nkentseu::NkSnprintf(b, sizeof(b), "%u", v);
				s.Append(b);
			}

			// L'empreinte s'ecrit en HEXADECIMAL SUR 16 CHIFFRES, largeur fixe.
			// En decimal elle depasserait ce qu'un `TokenU32` sait relire, et une
			// largeur variable rendrait deux fichiers identiques differents a
			// l'oeil selon la valeur.
			inline void PutU64Hex(NkString &s, uint64 v) {
				const char *chiffres = "0123456789abcdef";
				char b[17];
				for (int32 i = 15; i >= 0; --i) {
					b[i] = chiffres[v & 0xF];
					v >>= 4;
				}
				b[16] = 0;
				s.Append(b);
			}


			inline void PutF32(NkString &s, float32 v) {
				char b[32];
				nkentseu::NkSnprintf(b, sizeof(b), "%.6f", (double)v);
				s.Append(b);
			}

			// ⚠️ FORMATEUR SEPARE POUR LES VALEURS, et la raison est mesurable :
			// `%.6f` PERD de l'information. Un tiers s'ecrit « 0.333333 » et se
			// relit 0.333333f, qui n'est pas le flottant de depart — l'egalite
			// exacte de `NkGraphValue::Equals` echouerait alors sur un
			// aller-retour pourtant correct. `%.9g` est le nombre de chiffres
			// significatifs qui garantit qu'un `float` traverse texte -> binaire
			// sans changer d'un bit.
			//
			// Les positions `x, y` gardent `%.6f` : c'est une dette anterieure a
			// ce fichier-ci, elle n'a jamais gene (un pixel de canevas ne se
			// compare pas au bit pres) et la changer reecrirait tous les fichiers
			// existants sans rien corriger de reel.
			inline void PutValF32(NkString &s, float32 v) {
				char b[40];
				snprintf(b, sizeof(b), "%.9g", (double)v);
				s.Append(b);
			}

			// Ecrit la charge utile commune a `def` et `prop` :
			//     <idType> <nbReels> <r0> ... [texte]
			// Le texte vient EN DERNIER et occupe le reste de la ligne — meme
			// convention que les libelles, pour la meme raison : aucun
			// echappement a mal gerer. Il n'est ecrit que s'il n'est pas vide,
			// sinon la ligne trainerait une espace finale.
			inline void PutValue(NkString &s, const NkGraphValue &v) {
				PutU32(s, v.type);
				s.Append(' ');
				PutU32(s, (uint32)v.numbers.Size());
				for (uint32 i = 0; i < (uint32)v.numbers.Size(); ++i) {
					s.Append(' ');
					PutValF32(s, v.numbers[i]);
				}
				if (v.text.Size() > 0) {
					s.Append(' ');
					s.Append(v.text);
				}
			}

			// Avance jusqu'au prochain jeton, renvoie sa longueur. `p` pointe dessus.
			inline const char *NextToken(const char *p, uint32 &len) {
				while (*p == ' ' || *p == '\t')
					++p;
				const char *start = p;
				while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
					++p;
				len = (uint32)(p - start);
				return start;
			}

			inline uint32 TokenU32(const char *&p) {
				uint32 len = 0;
				const char *t = NextToken(p, len);
				uint32 v = 0;
				for (uint32 i = 0; i < len; ++i) {
					if (t[i] < '0' || t[i] > '9')
						break;
					v = v * 10u + (uint32)(t[i] - '0');
				}
				p = t + len;
				return v;
			}

			inline uint64 TokenU64Hex(const char *&p) {
				uint32 len = 0;
				const char *t = NextToken(p, len);
				uint64 v = 0;
				for (uint32 i = 0; i < len; ++i) {
					const char c = t[i];
					uint64 d = 16;
					if (c >= '0' && c <= '9')
						d = (uint64)(c - '0');
					else if (c >= 'a' && c <= 'f')
						d = (uint64)(c - 'a') + 10;
					else if (c >= 'A' && c <= 'F')
						d = (uint64)(c - 'A') + 10;
					if (d < 16)
						v = v * 16 + d;
				}
				p = t + len;
				return v;
			}

			inline float32 TokenF32(const char *&p) {
				uint32 len = 0;
				const char *t = NextToken(p, len);
				char b[40];
				const uint32 n = len < 39 ? len : 39;
				for (uint32 i = 0; i < n; ++i)
					b[i] = t[i];
				b[n] = 0;
				p = t + len;
				return (float32)strtod(b, nullptr);
			}

			// Jeton simple (une cle, jamais d'espace).
			inline void TokenStr(const char *&p, NkString &out) {
				uint32 len = 0;
				const char *t = NextToken(p, len);
				out = NkString("");
				for (uint32 i = 0; i < len; ++i)
					out.Append(t[i]);
				p = t + len;
			}

			// RESTE de la ligne : les libelles sont traduisibles, donc ils
			// contiennent des espaces. Ils sont toujours places en DERNIER pour que
			// ce cas reste trivial, sans guillemets ni echappement a mal gerer.
			inline void RestOfLine(const char *&p, NkString &out) {
				while (*p == ' ' || *p == '\t')
					++p;
				out = NkString("");
				while (*p && *p != '\n' && *p != '\r')
					out.Append(*p++);
			}

			// Miroir exact de PutValue. Le texte prend le reste de la ligne.
			inline void TakeValue(const char *&p, NkGraphValue &v) {
				v.Clear();
				v.type = (NkTypeId)TokenU32(p);
				const uint32 n = TokenU32(p);
				// Borne DURE. Le compte vient du FICHIER : un fichier corrompu
				// annoncant quatre milliards de reels ferait exploser la memoire avant
				// que la moindre validation n'ait la parole. On borne donc a la LECTURE,
				// la ou le nombre entre — valider apres serait deja trop tard.
				const uint32 kMax = 4096u;
				const uint32 count = n < kMax ? n : kMax;
				for (uint32 i = 0; i < count; ++i)
					v.numbers.PushBack(TokenF32(p));
				RestOfLine(p, v.text);
			}

			inline const char *NextLine(const char *p) {
				while (*p && *p != '\n')
					++p;
				return *p ? p + 1 : p;
			}

			inline bool LineIs(const char *p, const char *kw) {
				while (*p == ' ' || *p == '\t')
					++p;
				while (*kw) {
					if (*p != *kw)
						return false;
					++p;
					++kw;
				}
				return *p == ' ' || *p == '\t';
			}

		} // namespace detail

		// ── ECRITURE ────────────────────────────────────────────────────────────
		inline void NkNodeGraph::Serialize(NkString &out) const {
			// ⚠️ LA VERSION VIENT DE LA CONSTANTE, PAS D'UN LITTERAL. Deux endroits qui
			// decideraient du meme numero finiraient par ne plus s'accorder — et
			// c'est l'ecrivain qui aurait raison contre le lecteur, en silence.
			out = NkString("nkgraph ");
			detail::PutU32(out, NK_NKGRAPH_VERSION);
			out.Append('\n');

			out.Append("compteurs ");
			detail::PutU32(out, mNextNode);
			out.Append(' ');
			detail::PutU32(out, mNextLink);
			out.Append('\n');

			for (uint32 i = 1; i < (uint32)mTypeNames.Size(); ++i) {
				// ⚠️ LA LIGNE `type` NE CHANGE PAS POUR UNE FEUILLE. Son nom EST sa
				// definition : lui ajouter une empreinte vide obligerait a ecrire
				// « rien » sous la forme d'un zero, et un lecteur de version 1 ou 2
				// cesserait de la comprendre sans raison.
				out.Append("type ");
				detail::PutU32(out, i);
				out.Append(' ');
				out.Append(mTypeNames[i]);
				out.Append('\n');
				uint64 emp = 0;
				if (!TypeFingerprint((NkTypeId)i, &emp))
					continue; // feuille : rien de plus a ecrire
				// `typec` porte le GENRE, l'EMPREINTE et le NOMBRE de membres ; les
				// `typem` qui suivent portent les membres DANS L'ORDRE. L'ordre est
				// du sens, pas de la mise en forme : voir EmpreinteDeStructure.
				out.Append("typec ");
				detail::PutU32(out, i);
				out.Append(' ');
				detail::PutU32(out, (uint32)TypeKind((NkTypeId)i));
				out.Append(' ');
				detail::PutU64Hex(out, emp);
				out.Append(' ');
				detail::PutU32(out, TypeMemberCount((NkTypeId)i));
				out.Append('\n');
				for (uint32 k = 0; k < TypeMemberCount((NkTypeId)i); ++k) {
					const NkTypeMember *m = TypeMemberAt((NkTypeId)i, k);
					out.Append("typem ");
					detail::PutU32(out, i);
					out.Append(' ');
					out.Append(m->name);
					out.Append(' ');
					// ⚠️ UN TIRET, PAS UN VIDE. Un champ vide en fin de ligne se
					// relit comme un jeton absent, et « enumerateur (sans type) »
					// deviendrait indiscernable d'une ligne tronquee.
					out.Append(m->type.Size() ? m->type : NkString("-"));
					out.Append('\n');
				}
			}

			for (uint32 i = 0; i < (uint32)mConversions.Size(); ++i) {
				out.Append("conv ");
				detail::PutU32(out, (uint32)(mConversions[i] >> 32));
				out.Append(' ');
				detail::PutU32(out, (uint32)(mConversions[i] & 0xFFFFFFFFull));
				out.Append('\n');
			}

			// Les noeuds morts ne sont PAS ecrits — mais la ligne `compteurs`
			// ci-dessus garde leur identifiant hors d'atteinte.
			for (uint32 i = 0; i < (uint32)mNodes.Size(); ++i) {
				const NkNode &n = mNodes[i];
				if (!n.alive)
					continue;
				out.Append("noeud ");
				detail::PutU32(out, n.id);
				out.Append(' ');
				detail::PutF32(out, n.x);
				out.Append(' ');
				detail::PutF32(out, n.y);
				out.Append(' ');
				out.Append(n.type);
				out.Append(' ');
				out.Append(n.label);
				out.Append('\n');
				// Directive DEDIEE plutot qu'un champ de plus sur la ligne `noeud` :
				// les fichiers ecrits avant les sous-graphes restent lisibles tels
				// quels, et un noeud ordinaire n'ecrit rien du tout.
				if (n.subgraph.Size() > 0) {
					out.Append("sousgraphe ");
					detail::PutU32(out, n.id);
					out.Append(' ');
					out.Append(n.subgraph);
					out.Append('\n');
				}
				// Les proprietes suivent leur noeud, AVANT ses prises, et dans leur
				// ordre d'insertion : cet ordre est ce qui rend l'aller-retour
				// reproductible au caractere pres.
				for (uint32 k = 0; k < (uint32)n.props.Size(); ++k) {
					const NkGraphProp &pr = n.props[k];
					// Une valeur JAMAIS RENSEIGNEE n'ecrit AUCUNE ligne. C'est ce qui evite
					// le defaut paye par l'agent NkUIDesign dans la nuit du 21 au 22/08 :
					// un ecrivain qui emet quand meme sa ligne ecrirait un type 0 et zero
					// reel, et la relecture rendrait une valeur « renseignee a vide » —
					// indiscernable a l'oeil, differente au sens, et SANS erreur.
					if (!pr.value.IsSet())
						continue;
					out.Append("prop ");
					detail::PutU32(out, n.id);
					out.Append(' ');
					out.Append(pr.name);
					out.Append(' ');
					detail::PutValue(out, pr.value);
					out.Append('\n');
				}
				// Les sockets suivent leur noeud, dans l'ordre : cet ordre EST leur
				// index, et les liens s'y referent.
				//
				// 🔴 ET C'EST LE POINT FAIBLE DU FORMAT, MESURE LE 2026-08-23 par
				// `graphe/index-de-prise-contre-nom`. Une ligne `sock` glissee
				// AVANT une autre decale tout ce qui suit ; les lignes `lien` et
				// `def` ne portent que des nombres, donc le lien se met a designer
				// une AUTRE prise. `Deserialize` rend `true`, `Validate` rend ZERO
				// diagnostic : le fichier reste bien forme et le graphe s'evalue
				// faux. Il n'y a aucun nom du cote du lien a confronter.
				//
				// Ce qui nous protege n'est pas le format, c'est que nous sommes le
				// seul a l'ecrire. Un producteur tiers, une edition a la main, ou
				// une migration qui regenererait les prises depuis un catalogue ou
				// le type a gagne une prise suffisent a le casser. Le correctif,
				// le jour venu : un NOM dans la ligne `lien`, et un refus quand il
				// ne s'accorde pas avec l'index.
				for (uint32 k = 0; k < (uint32)n.sockets.Size(); ++k) {
					const NkSocket &s = n.sockets[k];
					out.Append("sock ");
					detail::PutU32(out, n.id);
					out.Append(s.dir == NkSocketDir::Output ? " 1 " : " 0 ");
					detail::PutU32(out, s.type);
					out.Append(' ');
					out.Append(s.name);
					out.Append('\n');
					// ⚠️ LA FAMILLE S'ECRIT SUR UNE LIGNE A PART, ET SEULEMENT SI ELLE
					// N'EST PAS `Data`. La ligne `sock` ne bouge donc pas d'un octet pour
					// un document sans execution -- meme choix que `typec` : ce qui
					// n'existe pas dans un document ne doit pas peser sur son fichier,
					// et un fichier ancien se relit inchange.
					//
					// La prise se designe par son NOM et son SENS, jamais par son rang :
					// c'est la regle du format depuis la version 2, et elle vaut aussi
					// pour ce qui QUALIFIE une prise, pas seulement pour ce qui la vise.
					if (s.family != NkSocketFamily::Data) {
						out.Append("sockf ");
						detail::PutU32(out, n.id);
						out.Append(s.dir == NkSocketDir::Output ? " 1 " : " 0 ");
						detail::PutU32(out, (uint32)s.family);
						out.Append(' ');
						out.Append(s.name);
						out.Append('\n');
					}
					// ⚠️ LE DEFAUT SUIT SA PRISE ET LA DESIGNE PAR SON NOM ET SON SENS
					// (version 2). Il portait son INDEX en version 1, et c'etait le
					// meme defaut que les liens : reordonner les `sock` posait la
					// valeur sur une prise QUI N'EST PAS LA SIENNE. Le commentaire
					// d'origine disait « il vaut l'ordre d'ecriture des prises, et
					// rien d'autre » — c'etait exact, et c'etait le probleme.
					if (s.defaultValue.IsSet()) {
						out.Append("def ");
						detail::PutU32(out, n.id);
						out.Append(s.dir == NkSocketDir::Output ? " 1 " : " 0 ");
						out.Append(s.name);
						out.Append(' ');
						detail::PutValue(out, s.defaultValue);
						out.Append('\n');
					}
				}
			}

			for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i) {
				const NkLink &l = mLinks[i];
				if (!l.alive)
					continue;
				// ⚠️ LES DEUX PRISES SE DESIGNENT PAR LEUR NOM (version 2). Le sens
				// n'a pas besoin d'etre ecrit : `de` est forcement une SORTIE et
				// `vers` une ENTREE — c'est la seule forme qu'un lien puisse
				// prendre, et `Connect` refuse tout le reste.
				//
				// Un lien dont une extremite ne se resout pas s'ecrit avec un nom
				// que rien ne porte : la lecture le REFUSERA en le nommant. Ecrire
				// un nom vide, ou retomber sur l'index, redonnerait au fichier le
				// defaut qu'on vient d'en retirer.
				const NkNode *nf = Find(l.fromNode);
				const NkNode *nt = Find(l.toNode);
				const bool okF = nf && l.fromSocket >= 0 && l.fromSocket < (int32)nf->sockets.Size();
				const bool okT = nt && l.toSocket >= 0 && l.toSocket < (int32)nt->sockets.Size();
				out.Append("lien ");
				detail::PutU32(out, l.id);
				out.Append(' ');
				detail::PutU32(out, l.fromNode);
				out.Append(' ');
				out.Append(okF ? nf->sockets[(uint32)l.fromSocket].name : NkString("?prise-introuvable"));
				out.Append(' ');
				detail::PutU32(out, l.toNode);
				out.Append(' ');
				out.Append(okT ? nt->sockets[(uint32)l.toSocket].name : NkString("?prise-introuvable"));
				out.Append('\n');
				// ⚠️ LA QUALIFICATION S'ECRIT SEULEMENT SI ELLE EXISTE, comme
				// `sockf` et `typec` : un lien nu produit exactement les memes
				// octets qu'avant la version 5.
				if (l.subgraph.Size() > 0) {
					out.Append("liensg ");
					detail::PutU32(out, l.id);
					out.Append(' ');
					out.Append(l.subgraph);
					out.Append('\n');
				}
				for (uint32 k = 0; k < (uint32)l.props.Size(); ++k) {
					// Meme regle que pour les proprietes de noeud : on n'ecrit pas
					// une valeur jamais renseignee -- la relire rendrait une valeur
					// « renseignee a vide », indiscernable a l'oeil et differente
					// au sens.
					if (!l.props[k].value.IsSet())
						continue;
					out.Append("lienp ");
					detail::PutU32(out, l.id);
					out.Append(' ');
					out.Append(l.props[k].name);
					out.Append(' ');
					detail::PutValue(out, l.props[k].value);
					out.Append('\n');
				}
			}
		}

		// ── LECTURE ─────────────────────────────────────────────────────────────
		inline bool NkNodeGraph::Deserialize(const char *text, NkString *outErreur) {
			if (outErreur)
				*outErreur = NkString("");
			auto refuse = [&](const NkString &quoi) {
				if (outErreur)
					*outErreur = quoi;
				// ⚠️ ON VIDE. Un graphe a moitie charge est la pire des reponses :
				// il porte des noeuds justes et des liens faux, et l'appelant qui
				// ignore le `false` compile un materiau qui a l'air complet.
				Clear();
				return false;
			};
			if (!text)
				return refuse(NkString("texte absent"));
			Clear();
			if (!detail::LineIs(text, "nkgraph"))
				return refuse(NkString("ce n'est pas un fichier .nkgraph (premiere ligne)"));

			// ── LA VERSION, LUE AVANT TOUT LE RESTE ──────────────────────────
			// Elle decide comment `lien` et `def` designent leur prise : par
			// INDEX en version 1, par NOM a partir de la 2. Une version qu'on ne
			// reconnait pas est REFUSEE en se nommant — lire un fichier futur
			// « au mieux » produirait un graphe plausible et faux.
			uint32 versionFichier = 0;
			{
				const char *pv = text;
				NkString kwv;
				detail::TokenStr(pv, kwv);
				versionFichier = detail::TokenU32(pv);
			}
			if (versionFichier < 1 || versionFichier > NK_NKGRAPH_VERSION) {
				NkString q("version de .nkgraph non prise en charge : ");
				detail::PutU32(q, versionFichier);
				q.Append(" (cette construction lit 1 a ");
				detail::PutU32(q, NK_NKGRAPH_VERSION);
				q.Append(")");
				return refuse(q);
			}
			const bool parNom = versionFichier >= 2;

			// ═══════════════════════════════════════════════════════════════
			// 🔴 DEUX PASSES, ET C'EST LA MESURE QUI L'A IMPOSE
			// ═══════════════════════════════════════════════════════════════
			// La version 2 devait rendre l'ORDRE DES LIGNES SANS IMPORTANCE. En
			// une seule passe elle ne le faisait qu'a moitie : l'ecrivain emet
			// chaque `def` JUSTE APRES sa prise, donc reordonner les `sock` place
			// des `def` AVANT la prise qu'ils nomment. Le cas
			// `fichier/ordre-des-sock` l'a fait tomber du premier coup — refus
			// nomme, ce qui est correct, mais ce n'est pas « charger a
			// l'identique », et c'etait bien la promesse.
			//
			// ⚠️ LA LECON EST PLUS GENERALE QUE LE CORRECTIF : passer de l'index
			// au nom ne suffit pas si la RESOLUTION reste dependante de l'ordre.
			// On avait retire la dependance a l'ordre du DESIGNANT, pas celle du
			// MOMENT. Ce sont deux choses, et la premiere cache la seconde.
			//
			// Passe 1 : la MATIERE (types, noeuds, prises, proprietes).
			// Passe 2 : les REFERENCES (valeurs par defaut, liens) — a ce
			//           moment, toutes les prises existent, quel que soit
			//           l'ordre dans lequel le fichier les a ecrites.
			// ⚠️ TROIS PASSES DEPUIS LA VERSION 5, et la troisieme existe pour la
			// MEME raison que la deuxieme : `lienp` et `liensg` designent leur lien
			// par son IDENTIFIANT, et cet identifiant doit exister quand on les
			// lit. Les mettre dans la passe des references marcherait TANT QUE
			// l'ecrivain place chaque `lienp` apres son `lien` -- c'est-a-dire
			// exactement la dependance a l'ORDRE qu'on a retiree du format en
			// version 2 puis des `def` en version 3. On ne la reintroduit pas par
			// la porte de derriere.
			for (uint32 passe = 0; passe < 3; ++passe) {
			const bool matiere = (passe == 0);
			const bool qualif = (passe == 2);
			for (const char *line = text; *line; line = detail::NextLine(line)) {
				const char *p = line;
				NkString kw;
				detail::TokenStr(p, kw);

				const bool estQualif = detail::GraphStrEq(kw, "lienp") || detail::GraphStrEq(kw, "liensg");
				const bool estReference = detail::GraphStrEq(kw, "def") || detail::GraphStrEq(kw, "lien");
				if (qualif != estQualif)
					continue;
				if (!qualif && (matiere == estReference))
					continue;

				if (detail::GraphStrEq(kw, "compteurs")) {
					mNextNode = detail::TokenU32(p);
					mNextLink = detail::TokenU32(p);
				} else if (detail::GraphStrEq(kw, "type")) {
					const uint32 id = detail::TokenU32(p);
					NkString name;
					detail::RestOfLine(p, name);
					// On RESPECTE l'identifiant ecrit au lieu d'en attribuer un
					// nouveau : les sockets s'y referent par numero.
					while ((uint32)mTypeNames.Size() <= id)
						mTypeNames.PushBack(NkString(""));
					while ((uint32)mTypeDefs.Size() <= id)
						mTypeDefs.PushBack(TypeDef());
					mTypeNames[id] = name;
				} else if (detail::GraphStrEq(kw, "typec")) {
					// La DEFINITION d'un type composite. Elle suit toujours la ligne
					// `type` qui a cree le nom, donc l'entree existe deja.
					const uint32 tid = detail::TokenU32(p);
					const uint32 genre = detail::TokenU32(p);
					const uint64 emp = detail::TokenU64Hex(p);
					const uint32 nbm = detail::TokenU32(p);
					while ((uint32)mTypeDefs.Size() <= tid)
						mTypeDefs.PushBack(TypeDef());
					TypeDef &d = mTypeDefs[tid];
					d.kind = (NkTypeKind)genre;
					d.empreinte = emp;
					d.aEmpreinte = true;
					d.members.Clear();
					(void)nbm; // les `typem` qui suivent font foi ; le compte est un controle
				} else if (detail::GraphStrEq(kw, "typem")) {
					const uint32 tid = detail::TokenU32(p);
					NkTypeMember m;
					detail::TokenStr(p, m.name);
					NkString ty;
					detail::TokenStr(p, ty);
					// « - » signifie « sans objet » (enumerateur). On le retraduit en
					// vide ICI, une seule fois, pour que le reste du code n'ait jamais
					// a connaitre la convention du fichier.
					if (!(ty == NkString("-")))
						m.type = ty;
					while ((uint32)mTypeDefs.Size() <= tid)
						mTypeDefs.PushBack(TypeDef());
					mTypeDefs[tid].members.PushBack(m);
				} else if (detail::GraphStrEq(kw, "conv")) {
					const uint32 a = detail::TokenU32(p);
					const uint32 b = detail::TokenU32(p);
					mConversions.PushBack(((uint64)a << 32) | (uint64)b);
				} else if (detail::GraphStrEq(kw, "noeud")) {
					NkNode n;
					n.id = detail::TokenU32(p);
					n.x = detail::TokenF32(p);
					n.y = detail::TokenF32(p);
					detail::TokenStr(p, n.type);
					detail::RestOfLine(p, n.label);
					n.alive = true;
					mNodes.PushBack(n);
				} else if (detail::GraphStrEq(kw, "sousgraphe")) {
					const uint32 nid = detail::TokenU32(p);
					NkNode *n = Find(nid);
					if (n)
						detail::RestOfLine(p, n->subgraph);
				} else if (detail::GraphStrEq(kw, "sock")) {
					const uint32 nid = detail::TokenU32(p);
					const uint32 dir = detail::TokenU32(p);
					NkSocket s;
					s.type = detail::TokenU32(p);
					s.dir = dir ? NkSocketDir::Output : NkSocketDir::Input;
					detail::RestOfLine(p, s.name);
					NkNode *n = Find(nid);
					if (n)
						n->sockets.PushBack(s);
				} else if (detail::GraphStrEq(kw, "liensg")) {
					const uint32 lid = detail::TokenU32(p);
					NkString nom;
					detail::RestOfLine(p, nom);
					NkLink *l = TrouveLien(lid);
					if (l)
						l->subgraph = nom;
				} else if (detail::GraphStrEq(kw, "lienp")) {
					const uint32 lid = detail::TokenU32(p);
					NkString nom;
					detail::TokenStr(p, nom);
					NkGraphValue v;
					detail::TakeValue(p, v);
					NkLink *l = TrouveLien(lid);
					if (l && nom.Size() > 0) {
						NkGraphProp pr;
						pr.name = nom;
						pr.value = v;
						l->props.PushBack(pr);
					}
				} else if (detail::GraphStrEq(kw, "sockf")) {
					// La FAMILLE d'une prise deja creee par sa ligne `sock`. Lue en
					// passe MATIERE : les liens, en passe 2, en dependent pour leur
					// arite.
					const uint32 nid = detail::TokenU32(p);
					const uint32 dir = detail::TokenU32(p);
					const uint32 fam = detail::TokenU32(p);
					NkString nom;
					detail::RestOfLine(p, nom);
					NkNode *n = Find(nid);
					if (n) {
						const int32 idx =
							n->FindSocket(nom.CStr(), dir ? NkSocketDir::Output : NkSocketDir::Input);
						if (idx >= 0)
							n->sockets[(uint32)idx].family = (NkSocketFamily)fam;
					}
				} else if (detail::GraphStrEq(kw, "prop")) {
					const uint32 nid = detail::TokenU32(p);
					NkString name;
					detail::TokenStr(p, name);
					NkGraphValue v;
					detail::TakeValue(p, v);
					NkNode *n = Find(nid);
					if (n && name.Size() > 0) {
						NkGraphProp pr;
						pr.name = name;
						pr.value = v;
						n->props.PushBack(pr);
					}
				} else if (detail::GraphStrEq(kw, "def")) {
					const uint32 nid = detail::TokenU32(p);
					int32 sidx = -1;
					NkString nomPrise;
					if (parNom) {
						const uint32 dir = detail::TokenU32(p);
						detail::TokenStr(p, nomPrise);
						NkNode *nn = Find(nid);
						if (nn)
							sidx = nn->FindSocket(nomPrise.CStr(),
												  dir ? NkSocketDir::Output : NkSocketDir::Input);
					} else {
						sidx = (int32)detail::TokenU32(p);
					}
					NkGraphValue v;
					detail::TakeValue(p, v);
					NkNode *n = Find(nid);
					// HORS BORNES : ON LAISSE TOMBER, ON NE RABAT PAS. Rabattre l'index sur
					// la derniere prise — ou sur zero — poserait la valeur sur une prise QUI
					// N'EST PAS LA SIENNE : le graphe paraitrait sain et calculerait autre
					// chose. Une valeur perdue finit par se voir ; une valeur DEPLACEE, non.
					//
					// ⚠️ EN VERSION 2 UN NOM INTROUVABLE EST REFUSE, PAS LAISSE TOMBER.
					// La difference tient a ce que chacun signifie : un index hors
					// bornes est un fichier d'une autre epoque, un NOM absent est un
					// fichier qui ment sur ce qu'il contient.
					if (parNom && sidx < 0) {
						NkString q("valeur par defaut sur une prise inconnue « ");
						q.Append(nomPrise);
						q.Append(" » du noeud ");
						detail::PutU32(q, nid);
						return refuse(q);
					}
					if (n && sidx >= 0 && sidx < (int32)n->sockets.Size())
						n->sockets[(uint32)sidx].defaultValue = v;
				} else if (detail::GraphStrEq(kw, "lien")) {
					NkLink l;
					l.id = detail::TokenU32(p);
					if (parNom) {
						// ⚠️ LA RESOLUTION SE FAIT ICI, UNE SEULE FOIS. Le nom est la
						// cle SUR DISQUE ; en memoire `NkLink` garde son index, parce
						// qu'un graphe s'evalue a chaque image et qu'un fichier ne se
						// lit qu'une fois. Payer une recherche par nom a chaque
						// evaluation echangerait un defaut rare contre un cout
						// permanent.
						NkString nomDe, nomVers;
						l.fromNode = detail::TokenU32(p);
						detail::TokenStr(p, nomDe);
						l.toNode = detail::TokenU32(p);
						detail::TokenStr(p, nomVers);
						const NkNode *nde = Find(l.fromNode);
						const NkNode *nvers = Find(l.toNode);
						// Le sens n'est pas ecrit parce qu'il n'a qu'une forme
						// possible : `de` est une SORTIE, `vers` une ENTREE.
						l.fromSocket = nde ? nde->FindSocket(nomDe.CStr(), NkSocketDir::Output) : -1;
						l.toSocket = nvers ? nvers->FindSocket(nomVers.CStr(), NkSocketDir::Input) : -1;
						// 🔴 REFUS NOMME. Rabattre sur la prise 0, ou laisser tomber le
						// lien, rendrait un graphe qui CHARGE et qui calcule autre
						// chose — precisement le defaut que la version 2 elimine.
						if (l.fromSocket < 0 || l.toSocket < 0) {
							NkString q("lien ");
							detail::PutU32(q, l.id);
							q.Append(" : prise introuvable — ");
							if (l.fromSocket < 0) {
								q.Append("sortie « ");
								q.Append(nomDe);
								q.Append(" » du noeud ");
								detail::PutU32(q, l.fromNode);
							}
							if (l.fromSocket < 0 && l.toSocket < 0)
								q.Append(" ; ");
							if (l.toSocket < 0) {
								q.Append("entree « ");
								q.Append(nomVers);
								q.Append(" » du noeud ");
								detail::PutU32(q, l.toNode);
							}
							return refuse(q);
						}
					} else {
						// ── VERSION 1 : LA MIGRATION ─────────────────────────
						// Les prises se designaient par leur rang dans l'ordre des
						// lignes `sock`. On le lit tel quel : les fichiers ecrits
						// avant le 2026-08-23 doivent charger a l'identique, et neuf
						// d'entre eux, dans NkMatGraphCheck, l'exercent a chaque
						// course du banc.
						l.fromNode = detail::TokenU32(p);
						l.fromSocket = (int32)detail::TokenU32(p);
						l.toNode = detail::TokenU32(p);
						l.toSocket = (int32)detail::TokenU32(p);
					}
					l.alive = true;
					mLinks.PushBack(l);
				}
			}
			}
			// Un `mNextNode` absent du fichier laisserait le graphe attribuer 1 au
			// prochain noeud, donc ecraser un identifiant existant.
			for (uint32 i = 0; i < (uint32)mNodes.Size(); ++i)
				if (mNodes[i].id >= mNextNode)
					mNextNode = mNodes[i].id + 1;
			for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i)
				if (mLinks[i].id >= mNextLink)
					mNextLink = mLinks[i].id + 1;
			return true;
		}

		// ── HISTORIQUE ──────────────────────────────────────────────────────────
		inline void NkGraphHistory::Reset(const NkNodeGraph &g) {
			mStack.Clear();
			NkString s;
			g.Serialize(s);
			mStack.PushBack(s);
			mCursor = 0;
		}

		inline void NkGraphHistory::Commit(const NkNodeGraph &g) {
			if (mStack.Empty()) {
				Reset(g);
				return;
			}
			// Modifier apres une annulation ABANDONNE la branche refaisable. C'est
			// le comportement de tous les editeurs : garder les deux branches
			// obligerait a montrer un arbre a l'utilisateur.
			while ((uint32)mStack.Size() > mCursor + 1)
				mStack.PopBack();
			NkString s;
			g.Serialize(s);
			mStack.PushBack(s);
			mCursor = (uint32)mStack.Size() - 1;
			while ((uint32)mStack.Size() > mLimit) {
				// On retire le PLUS ANCIEN. NkVector n'offre pas de retrait en tete :
				// on decale, le cout est negligeable devant la profondeur retenue.
				for (uint32 i = 1; i < (uint32)mStack.Size(); ++i)
					mStack[i - 1] = mStack[i];
				mStack.PopBack();
				if (mCursor > 0)
					mCursor--;
			}
		}

		inline bool NkGraphHistory::Undo(NkNodeGraph &g) {
			if (mCursor == 0 || mStack.Empty())
				return false;
			mCursor--;
			return g.Deserialize(mStack[mCursor].CStr());
		}

		inline bool NkGraphHistory::Redo(NkNodeGraph &g) {
			if (mStack.Empty() || mCursor + 1 >= (uint32)mStack.Size())
				return false;
			mCursor++;
			return g.Deserialize(mStack[mCursor].CStr());
		}

		inline uint32 NkGraphHistory::UndoDepth() const {
			return mCursor;
		}

		inline uint32 NkGraphHistory::RedoDepth() const {
			return mStack.Empty() ? 0u : (uint32)mStack.Size() - 1u - mCursor;
		}

	} // namespace graph
} // namespace nkentseu
