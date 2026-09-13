#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""matgraph_mutation.py -- campagne de mutation du graphe de materiaux.

POURQUOI CE FICHIER EXISTE, ET POURQUOI IL EST COMMITE
======================================================
Deux nuits de suite, la campagne a EFFACE du travail non commite : sa
restauration est un `git checkout`, qui ne distingue pas la mutation du travail
voisin. La lecon « commiter avant la premiere mutation » etait juste et
INSUFFISANTE -- elle comptait sur la vigilance. Elle est ici INSTRUMENTEE :

  GARDE 1 -- l'arbre doit etre PROPRE avant de muter. Sinon on refuse, on ne
             mute pas, et on dit quoi commiter.
  GARDE 2 -- l'ancre doit apparaitre EXACTEMENT UNE FOIS. Zero ancre = la
             mutation muterait dans le vide et rendrait un vert pour rien ;
             plusieurs = on ne sait pas ce qu'on a mute.
  GARDE 3 -- la restauration porte sur LES FICHIERS MUTES, jamais sur un
             dossier entier. Et l'arbre est reverifie propre APRES.
  GARDE 4 -- le compte d'erreurs de CONSTRUCTION se lit AVANT le banc. Un banc
             qui rend « 0 echec » sur un binaire perime ne mesure rien ; c'est
             la seule sortie qui ment sans qu'on puisse le voir.
  GARDE 7 -- AUCUN CARACTERE HORS CP1252 DANS LES DESCRIPTIONS. Mesure le
             2026-08-24 : un emoji dans un `attendu` a fait TOMBER `--liste`
             sur `UnicodeEncodeError`, ce qui a rompu la chaine shell et
             empeche un commit -- puis la campagne suivante a REFUSE de muter
             sur un arbre sale. La garde 1 a bien joue son role, mais la cause
             etait un caractere dans un texte d'aide. Un outil doit rester
             lisible sur la console qui l'execute.
  GARDE 6 -- la construction se REJOUE avant d'accuser la mutation. Une
             construction qui echoue une fois sur deux accuse le LANCEUR, pas
             la mutation ; un verdict non reproductible est un verdict faux,
             meme quand il est prudent.
  GARDE 5 -- la restauration RECONSTRUIT. Restaurer la source ne suffit pas :
             le binaire reste celui de la derniere mutation, `git status` dit
             « propre », et le banc lance juste apres rend UN ECHEC pour un
             defaut qui n'existe plus nulle part. Ce piege-la est PIRE que le
             binaire perime ordinaire -- il rend un ROUGE sur du code sain, et
             le premier reflexe est de chercher le defaut dans la source.

La troisieme campagne l'a paye a son tour : le script precedent n'existait plus
sur le disque quand l'agent suivant a repris. Il est donc DANS LE DEPOT, avec
ses mutations, la ou quelqu'un le chercherait (regle 5).

EMPLOI
    python scripts/matgraph_mutation.py --liste
    python scripts/matgraph_mutation.py M11 M12 M13
"""

import os
import re
import subprocess
import sys

RACINE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
COMPILE_H = "Kernel/Runtime/NKRenderer/src/NKRenderer/Materials/Graph/NkMatGraphCompile.h"
BANC_SRC = "Applications/NkMatGraphCheck/src/main.cpp"
GRAPH_IO = "Kernel/Runtime/NKGraph/src/NKGraph/NkNodeGraphIO.inl"
GRAPH_INL = "Kernel/Runtime/NKGraph/src/NKGraph/NkNodeGraph.inl"
NKSL_CC = "Kernel/Runtime/NKSL/src/NKSL/Compiler/NkSLCompiler.cpp"
BANC = "Build/Bin/Debug-Windows/NkMatGraphCheck/NkMatGraphCheck.exe"

# --------------------------------------------------------------------------
# LES MUTATIONS. Une entree = un defaut DELIBERE, et le cas qui DOIT rougir.
# Chaque `edits` est une liste de (ancre, remplacement) ; l'ancre doit etre
# unique dans le fichier.
# --------------------------------------------------------------------------

# La premiere passe de refus (~ligne 1322) : la boucle qui verifie les noms de
# canaux AVANT d'emettre. La neutraliser laisse passer les noms inconnus.
_ANCRE_PASSE1 = """				{
					for (uint32 i = 0; i < g.RawNodeCount(); ++i) {
						const graph::NkNode *n = g.RawNodeAt(i);
						if (!n || !n->alive)
							continue;
						const bool estUV = n->type == NkString(NK_MN_UV_MAP);"""
_MUTE_PASSE1 = """				{
					for (uint32 i = 0; i < 0u; ++i) {
						const graph::NkNode *n = g.RawNodeAt(i);
						if (!n || !n->alive)
							continue;
						const bool estUV = n->type == NkString(NK_MN_UV_MAP);"""

# Le second filet, au SITE D'EMISSION de UV Map.
_ANCRE_FILET_UV = """						if (!c) {
							r.error = NkString("canal UV non valide a l emission (le filet du rang 4 a repris "
											   "la main : la passe de refus ne l a pas attrape)");
							r.source = NkString("");
							return r;
						}
						s.Append(c->varying);
						s.Append(", 0.0);\\n");"""
_MUTE_FILET_UV = """						s.Append(c ? c->varying : "vUV");
						s.Append(", 0.0);\\n");"""

# Le second filet, au SITE D'EMISSION d'Attribute.
_ANCRE_FILET_ATTR = """						if (!c) {
							r.error = NkString("attribut non valide a l emission (le filet du rang 4 a repris "
											   "la main : la passe de refus ne l a pas attrape)");
							r.source = NkString("");
							return r;
						}
						s.Append(c->varying);
						s.Append(".rgb;\\n");"""
_MUTE_FILET_ATTR = """						s.Append(c ? c->varying : "vColor");
						s.Append(".rgb;\\n");"""

# La version D AVANT LE CORRECTIF du second filet : au lieu de refuser, il emet
# un nom qui n existe dans aucun backend. Sert a MESURER ce que le correctif du
# 23/08 a change -- et il ne se mesure qu en compagnie de M12, sinon le site est
# inatteignable.
_MUTE_FILET_UV_JETON = """						s.Append(c ? c->varying : "nkCANAL_UV_NON_VALIDE");
						s.Append(", 0.0);\\n");"""
_MUTE_FILET_ATTR_JETON = """						s.Append(c ? c->varying : "nkATTRIBUT_NON_VALIDE");
						s.Append(".rgb;\\n");"""

# L'insertion de la prise « intruse » dans le texte serialise, au banc. La
# neutraliser doit faire rougir le cas index/nom -- sinon ce cas serait vert
# pour une raison qu'il n'annonce pas.
_ANCRE_M15 = """				if (debute) {
					truque.Append(entete);
					pose = true;
				}"""
_MUTE_M15 = """				if (debute) {
					pose = true;
				}"""


# ── LE FORMAT v2 : LE NOM DE LA PRISE ────────────────────────────────────────
# M16 : l'ecrivain remet l'INDEX dans la ligne `lien`. C'est le retour exact au
# defaut de la version 1, sous une version qui annonce 2.
_ANCRE_M16 = """				out.Append(okF ? nf->sockets[(uint32)l.fromSocket].name : NkString("?prise-introuvable"));"""
_MUTE_M16 = """				detail::PutU32(out, (uint32)l.fromSocket); (void)okF; (void)nf;"""

# M17 : le refus devient un repli sur la prise 0 -- « rien » charge comme « la
# premiere ». C'est LA pente naturelle, et elle rend un graphe qui CHARGE.
_ANCRE_M17 = """						if (l.fromSocket < 0 || l.toSocket < 0) {
							NkString q("lien ");"""
_MUTE_M17 = """						if (l.fromSocket < 0)
							l.fromSocket = 0;
						if (l.toSocket < 0)
							l.toSocket = 0;
						if (false) {
							NkString q("lien ");"""

# M18 : la migration disparait -- la v1 se lit comme de la v2.
_ANCRE_M18 = """			const bool parNom = versionFichier >= 2;"""
_MUTE_M18 = """			const bool parNom = true;"""

# M19 : retour a UNE seule passe. C'est le defaut que le temoin a trouve tout
# seul ; on verifie qu'il le retrouverait.
# ATTENTION -- ANCRE REECRITE LE 2026-08-24. La forme d'origine visait la boucle
# a DEUX passes ; la version 5 du format l'a passee a TROIS, et l'ancre a POURRI.
# La garde 2 l'a dit (« ancrage x0 ») au lieu de muter dans le vide et de rendre
# un vert pour rien -- c'est la seconde fois de ce chantier qu'elle paie.
_ANCRE_M19 = """				if (!qualif && (matiere == estReference))
					continue;"""
_MUTE_M19 = """				if (!qualif && !matiere)
					continue;"""

# M20 : le graphe n'est PLUS vide apres un refus. Le `false` est toujours rendu,
# donc seul un cas qui REGARDE la matiere restante peut le voir.
_ANCRE_M20 = """				// ⚠️ ON VIDE. Un graphe a moitie charge est la pire des reponses :
				// il porte des noeuds justes et des liens faux, et l'appelant qui
				// ignore le `false` compile un materiau qui a l'air complet.
				Clear();
				return false;"""
_MUTE_M20 = """				return false;"""


# ── LE CAS « EMIS CREDIBLE CONTRE EMIS QUI ECHOUE » ──────────────────────────
# M21 : la substitution du jeton porte AUSSI sur la declaration -- le jeton
# devient declare, glslang l'accepte, et le cas ne mesure plus qu'un renommage.
# C'est LA faute que j'ai faite en l'ecrivant.
_ANCRE_M21 = """	const NkString casse =
		r.ok ? RemplaceTout(r.source, "vec3(vUV, 0.0)", "vec3(nkCANAL_UV_NON_VALIDE, 0.0)") : NkString("");"""
_MUTE_M21 = """	const NkString casse = r.ok ? RemplaceTout(r.source, "vUV", "nkCANAL_UV_NON_VALIDE") : NkString("");"""

# M22 : on lit le verdict de glslang dans `success` au lieu du MOT MAGIQUE --
# le piege que le banc documente depuis le 22/08, applique au nouveau cas.
_ANCRE_M22 = """	bool vrai = false;
	if (sp.bytecode.Size() >= 4) {
		const uint8 *o = sp.bytecode.Data();
		vrai = (o[0] == 0x03 && o[1] == 0x02 && o[2] == 0x23 && o[3] == 0x07);
	}
	if (outMsg) {"""
_MUTE_M22 = """	bool vrai = sp.success;
	if (outMsg) {"""


# ── LA REMONTEE DES ERREURS DE GLSLANG ───────────────────────────────────────
# M23 : les erreurs de glslang ne sont plus recopiees -- retour a l'echec MUET.
_ANCRE_M23 = """						res.errors.PushBack(etiquetee);
					}"""
_MUTE_M23 = """						(void)etiquetee;
					}"""

# M24 : les erreurs remontent mais SANS etiquette d'etape -- deux journaux
# concatenes a l'aveugle, ce que Rodolf a explicitement refuse.
_ANCRE_M24 = """						NkString m("[glslang/SPIR-V] ");
						m.Append(e.message);
						etiquetee.message = m;"""
_MUTE_M24 = """						etiquetee.message = e.message;"""

_ANCRE_M23BIS = """					res.success = false;
					res.bytecode.Clear(); // rien ne doit ressembler a du SPIR-V"""
_MUTE_M23BIS = """					res.bytecode.Clear(); // rien ne doit ressembler a du SPIR-V"""


# ── LES TYPES : NOM QUALIFIE ET EMPREINTE ────────────────────────────────────
# M26 : l'empreinte ignore l'ORDRE des membres. Permuter deux enumerateurs
# cesserait d'etre vu -- la corruption la plus silencieuse qui soit.
_ANCRE_M26 = """				for (uint32 i = 0; i < n; ++i) {
					EmpreinteAvale(h, m[i].name.CStr());
					EmpreinteAvale(h, ":");
					EmpreinteAvale(h, m[i].type.CStr());
					EmpreinteAvale(h, ";");
				}"""
_MUTE_M26 = """				uint64 somme = 0;
				for (uint32 i = 0; i < n; ++i) {
					uint64 t = 14695981039346656037ULL;
					EmpreinteAvale(t, m[i].name.CStr());
					EmpreinteAvale(t, m[i].type.CStr());
					somme += t;
				}
				h ^= somme;"""

# M27 : le conflit ECRASE au lieu de refuser -- le dernier enregistre gagne,
# en silence. C'est la pente naturelle d'un registre idempotent.
_ANCRE_M27 = """				if (d.aEmpreinte && d.empreinte == emp)
					return existant;"""
_MUTE_M27 = """				if (d.aEmpreinte)
					return existant;"""

# M28 : une FEUILLE se met a rendre une empreinte (nulle) au lieu de « rien ».
_ANCRE_M28 = """			if (t >= (NkTypeId)mTypeDefs.Size() || !mTypeDefs[t].aEmpreinte)
				return false; // FEUILLE : pas d'empreinte. Pas une empreinte nulle."""
_MUTE_M28 = """			if (t >= (NkTypeId)mTypeDefs.Size() || !mTypeDefs[t].aEmpreinte) {
				if (out)
					*out = 0;
				return true;
			}"""

# M29 : le refus ne nomme plus CE QUI differe -- il dit seulement que ca differe.
_ANCRE_M29 = """				q.Append(" » est deja declare avec une AUTRE definition -- ");
				q.Append(detail::DecrisDivergence(d.members, d.kind, members, count, kind));"""
_MUTE_M29 = """				q.Append(" » est deja declare avec une AUTRE definition");"""

# M30 : l'empreinte n'est plus ECRITE dans le fichier -- l'accord redevient un
# accord de memoire, qui ne survit pas a la sauvegarde.
_ANCRE_M30 = """				out.Append("typec ");"""
_MUTE_M30 = """				if (true)
					continue;
				out.Append("typec ");"""


# ── LA GARDE DU MODELE NON NODAL ─────────────────────────────────────────────
# M31 : la garde laisse passer les types composes. Le graphe manipulerait un
# type que la compilation n'a nulle part ou ecrire.
_ANCRE_M31 = """							const graph::NkTypeKind genre = g.TypeKind(n->sockets[k].type);
							if (genre == graph::NkTypeKind::Leaf)
								continue;"""
_MUTE_M31 = """							const graph::NkTypeKind genre = g.TypeKind(n->sockets[k].type);
							if (true)
								continue;"""

# M32 : le refus ne dit plus OU ca coince (« NkMaterial ») -- il devient un
# « type non supporte » qui envoie l'auteur chercher un reglage.
_ANCRE_M32 = """							r.error.Append(" : NkMaterial n'a aucune forme pour ce type. Le graphe COMPILE VERS "
										   "NkMaterial, il ne le remplace pas -- un type que la compilation n'a "
										   "nulle part ou ecrire ne peut pas exister dans le graphe. NkMaterial "
										   "porte float, vec2/3/4, couleur, entier, booleen, texture.");"""
_MUTE_M32 = """							r.error.Append(" : type non supporte.");"""


# ── LA FAMILLE DE PRISE : UNE MUTATION PAR COMPORTEMENT (§ 20.2) ─────────────
# M33 : la famille n'est plus comparee -- le croisement passe, ou se refuse par
# le type. C'est le retour exact a l'etat d'avant.
_ANCRE_M33 = """			if (fa != fb)
				return NkLinkError::FamilyMismatch;"""
_MUTE_M33 = """			if (false)
				return NkLinkError::FamilyMismatch;"""

# M34 : l'arite d'ENTREE redevient celle de la donnee -- la 2e source d'exec
# remplace la premiere, et neuf chemins sur dix disparaissent sans un mot.
_ANCRE_M34 = """			if (fb == NkSocketFamily::Data)
				for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i)
					if (mLinks[i].alive && mLinks[i].toNode == to && mLinks[i].toSocket == di)
						mLinks[i].alive = false;"""
_MUTE_M34 = """			for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i)
				if (mLinks[i].alive && mLinks[i].toNode == to && mLinks[i].toSocket == di)
					mLinks[i].alive = false;"""

# M35 : l'arite de SORTIE redevient celle de la donnee -- une instruction peut
# avoir deux suites, et l'ordre depend de l'ordre d'insertion des liens.
_ANCRE_M35 = """			if (fa == NkSocketFamily::Exec)
				for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i)
					if (mLinks[i].alive && mLinks[i].fromNode == from && mLinks[i].fromSocket == si)
						return NkLinkError::ExecOutputAlreadyBound;"""
_MUTE_M35 = """			if (false)
				return NkLinkError::ExecOutputAlreadyBound;"""

# ── M36 ET M38 ONT CHANGE DE SENS LE 24/08, ET IL FAUT LE LIRE ─────────────
# Elles RETIRAIENT l'exemption d'execution ; elles la REINTRODUISENT. Le code a
# bouge sous elles -- l'acyclicite est redevenue universelle -- et une mutation
# qui garde son ancienne forme aurait mute vers le code CORRECT en croyant
# introduire un defaut. La garde 2 l'aurait dit (ancrage x0), et c'est la
# troisieme fois de ce chantier qu'elle paie.
#
# M36 : `Connect` exempte a nouveau les liens d'execution du controle de cycle.
_ANCRE_M36 = """			if (WouldCreateCycle(from, to))
				return NkLinkError::WouldCycle;"""
_MUTE_M36 = """			if (fa == NkSocketFamily::Data && WouldCreateCycle(from, to))
				return NkLinkError::WouldCycle;"""

# M37 : la famille n'est plus ECRITE dans le fichier -- l'accord redevient un
# accord de memoire, et l'aller-retour perd l'axe entier.
_ANCRE_M37 = """					if (s.family != NkSocketFamily::Data) {
						out.Append("sockf ");"""
_MUTE_M37 = """					if (false) {
						out.Append("sockf ");"""


# M38 : le parcours de `WouldCreateCycle` cesse a nouveau de suivre les liens
# d'execution.
#
# 🔴 CE QU'ELLE MESURE MAINTENANT, ET C'EST LE RESULTAT LE PLUS INTERESSANT DU
# 24/08 : elle etait un COUPLE (M36 + le parcours) parce que l'exemption
# s'ecrivait a DEUX endroits, donc invisible a une mutation a un seul defaut.
# Les deux encodages sont partis ensemble avec l'exception. M36 et M38 doivent
# donc rougir SEULES desormais -- et si l'une survit, c'est qu'une redondance
# est revenue quelque part.
_ANCRE_M38 = """					if (mLinks[i].alive && mLinks[i].fromNode == cur)
						stack.PushBack(mLinks[i].toNode);"""
_MUTE_M38 = """					if (mLinks[i].alive && mLinks[i].fromNode == cur && LinkFamily(mLinks[i]) == NkSocketFamily::Data)
						stack.PushBack(mLinks[i].toNode);"""


# ── LE LIEN QUALIFIE ─────────────────────────────────────────────────────────
# M39 : la qualification n'est plus ECRITE dans le fichier.
_ANCRE_M39 = """				if (l.subgraph.Size() > 0) {
					out.Append("liensg ");"""
_MUTE_M39 = """				if (false) {
					out.Append("liensg ");"""

# M40 : `LinkSubgraph` rend une chaine vide pour un lien INEXISTANT -- « rien »
# rendu comme « pas de condition ».
_ANCRE_M40 = """			const NkLink *l = TrouveLien(id);
			// ⚠️ `nullptr` = LE LIEN N'EXISTE PAS ; chaine vide = il existe et n'a
			// pas de condition. Deux etats, deux reponses -- regle 3.
			return l ? &l->subgraph : nullptr;"""
_MUTE_M40 = """			static NkString vide;
			const NkLink *l = TrouveLien(id);
			return l ? &l->subgraph : &vide;"""

# M41 : poser deux fois la meme cle AJOUTE au lieu de remplacer -- la lecture
# redevient dependante de l'ordre d'insertion.
_ANCRE_M41 = """			for (uint32 i = 0; i < (uint32)l->props.Size(); ++i)
				if (detail::GraphStrEq(l->props[i].name, name)) {
					l->props[i].value = v; // REMPLACE : deux homonymes rendraient
					return true;		   // la lecture dependante de l'insertion
				}"""
_MUTE_M41 = """			if (false)
				return true;"""

# M42 : la lecture de la qualification retombe en passe 2 -- elle remarche TANT
# QUE l'ecrivain place chaque `lienp` apres son `lien`. Dependance a l'ORDRE.
_ANCRE_M42 = """			for (uint32 passe = 0; passe < 3; ++passe) {"""
_MUTE_M42 = """			for (uint32 passe = 0; passe < 2; ++passe) {"""


# M43 : la qualification se lit en passe 1 (avec les references) au lieu de 2.
# Elle remarche TANT QUE l'ecrivain place chaque `lienp` APRES son `lien` --
# c'est la dependance a l'ORDRE, et seul un fichier reordonne la voit.
_ANCRE_M43 = """			const bool qualif = (passe == 2);"""
_MUTE_M43 = """			const bool qualif = (passe == 1);"""

# ── L'ACYCLICITE UNIVERSELLE, SUITE ─────────────────────────────────────────
# M44 : `TopoSort` recompte la SEULE donnee. C'est la moitie du changement du
# 24/08, et la moitie qu'on oublierait : un tri qui ignore les fils d'execution
# rend un ordre parfaitement VALIDE, simplement moins contraint. Seul un
# controle qui regarde l'ORDRE RENDU peut le voir.
_ANCRE_M44A = """				if (!mLinks[i].alive)
					continue;
				const int32 t = indexOf(mLinks[i].toNode);"""
_MUTE_M44A = """				if (!mLinks[i].alive || LinkFamily(mLinks[i]) != NkSocketFamily::Data)
					continue;
				const int32 t = indexOf(mLinks[i].toNode);"""
_ANCRE_M44B = """						if (!mLinks[k].alive || mLinks[k].fromNode != ids[i])
							continue;"""
_MUTE_M44B = """						if (!mLinks[k].alive || mLinks[k].fromNode != ids[i] ||
							LinkFamily(mLinks[k]) != NkSocketFamily::Data)
							continue;"""

# ── LA MACHINE A ETATS EST UN NOEUD ─────────────────────────────────────────
# M45 : l'ecrivain croit qu'une valeur, c'est DES NOMBRES -- et il saute les
# valeurs qui n'en portent pas. C'est la pente naturelle de qui relit
# `NkGraphValue` en diagonale : `numbers` saute aux yeux, `text` non. La
# reference vers la machine a etats est un TEXTE ; elle disparait du fichier.
_ANCRE_M45 = """					if (!pr.value.IsSet())
						continue;
					out.Append("prop ");"""
_MUTE_M45 = """					if (!pr.value.IsSet() || pr.value.numbers.Size() == 0)
						continue;
					out.Append("prop ");"""

# M46 : dans le BANC, l'ALLER de la contre-epreuve n'est plus branche -- on
# pretend seulement qu'il l'est. Le retour ne referme alors AUCUNE boucle.
# C'est la mutation de la famille de M15 : elle ne touche pas a l'assertion,
# elle retire la PERTURBATION dont l'assertion depend. Sans elle, on ne saurait
# pas si « les memes etats cables dehors sont refuses » mesure un CYCLE ou
# seulement un refus quelconque.
_ANCRE_M46 = """	const NkLinkError aller = h.Connect(marche, "apres", saut, "avant");"""
_MUTE_M46 = """	const NkLinkError aller = NkLinkError::Ok;"""

MUTATIONS = {
	"M44": {
		"quoi": "TopoSort recompte la SEULE donnee -- l ordre cesse d honorer la succession d execution",
		"cas": "exec/acyclicite-universelle-la-boucle-est-un-noeud",
		"attendu": "ATTRAPEE PAR LE VOLET ORDRE SEULEMENT. Le tri REUSSIT toujours et rend 4 noeuds ; "
				   "seul le rang compare voit que la succession n est plus honoree.",
		"edits": [(GRAPH_INL, _ANCRE_M44A, _MUTE_M44A), (GRAPH_INL, _ANCRE_M44B, _MUTE_M44B)],
	},
	"M45": {
		"quoi": "l ecrivain saute les valeurs sans NOMBRES -- la reference texte vers la machine disparait",
		"cas": "etats/la-machine-a-etats-est-un-noeud",
		"attendu": "ATTRAPEE -- et pas seulement par ce cas : tout aller-retour portant une propriete "
				   "TEXTE doit tomber. Si un seul cas rougit, c est que le texte n est mesure qu ici.",
		"edits": [(GRAPH_IO, _ANCRE_M45, _MUTE_M45)],
	},
	"M46": {
		"quoi": "l ALLER de la contre-epreuve n est plus branche -- le retour ne referme plus de boucle",
		"cas": "etats/la-machine-a-etats-est-un-noeud",
		"attendu": "ATTRAPEE -- sinon la contre-epreuve mesurerait un refus quelconque et non un CYCLE, "
				   "et le cas se reduirait a « un graphe sans cycle n a pas de cycle » (regle 1).",
		"edits": [(BANC_SRC, _ANCRE_M46, _MUTE_M46)],
	},
	"M43": {
		"quoi": "la qualification se lit en passe 1 (avec les references) -- dependance a l ORDRE retablie",
		"cas": "exec/lien-qualifie",
		"attendu": "ATTRAPEE PAR LE VOLET ORDRE SEULEMENT. Avec l ordre naturel de l ecrivain elle marche ; "
				   "c est le fichier REORDONNE qui la fait tomber. C est la mutation que M42 pretendait etre.",
		"edits": [(GRAPH_IO, _ANCRE_M43, _MUTE_M43)],
	},
	"M39": {
		"quoi": "la qualification du lien n est plus ecrite dans le fichier",
		"cas": "exec/lien-qualifie",
		"attendu": "ATTRAPEE -- meme defaut que l empreinte non ecrite (M30) et la famille non ecrite (M37).",
		"edits": [(GRAPH_IO, _ANCRE_M39, _MUTE_M39)],
	},
	"M40": {
		"quoi": "LinkSubgraph rend une chaine vide pour un lien INEXISTANT",
		"cas": "exec/lien-qualifie",
		"attendu": "ATTRAPEE -- regle 3 : « ce lien n existe pas » et « ce lien n a pas de condition » sont "
				   "deux etats.",
		"edits": [(GRAPH_INL, _ANCRE_M40, _MUTE_M40)],
	},
	"M41": {
		"quoi": "poser deux fois la meme cle AJOUTE au lieu de remplacer",
		"cas": "exec/lien-qualifie",
		"attendu": "ATTRAPEE -- deux homonymes rendent la lecture dependante de l ordre d insertion.",
		"edits": [(GRAPH_INL, _ANCRE_M41, _MUTE_M41)],
	},
	"M42": {
		"quoi": "la qualification retombe en passe 2 -- la lecture redevient dependante de l ORDRE",
		"cas": "exec/lien-qualifie",
		"attendu": "ATTRAPEE -- mais ATTENTION, ELLE NE MESURE PAS CE QUE SON NOM ANNONCAIT. Supprimer la passe "
				   "empeche la qualification d etre lue DU TOUT ; elle mesure donc « la qualification est-elle "
				   "lue », pas « la lecture depend-elle de l ORDRE ». Les deux sont differentes, et la seconde "
				   "est celle qui compte : c est M43 qui la mesure, avec le volet ORDRE du cas.",
		"edits": [(GRAPH_IO, _ANCRE_M42, _MUTE_M42)],
	},
	"M38": {
		"quoi": "LE COUPLE : Connect ET le parcours exemptent a nouveau l execution du controle de cycle",
		"cas": "exec/acyclicite-universelle-la-boucle-est-un-noeud",
		"attendu": "ATTRAPEE -- et le couple n a plus de raison d etre : depuis le 24/08 chacune de ses deux "
				   "moities rougit SEULE. Il reste ecrit comme temoin de ce que la redondance masquait.",
		"edits": [(GRAPH_INL, _ANCRE_M36, _MUTE_M36), (GRAPH_INL, _ANCRE_M38, _MUTE_M38)],
	},
	"M38seul": {
		"quoi": "le PARCOURS seul exempte l execution (Connect garde sa garde universelle)",
		"cas": "exec/acyclicite-universelle-la-boucle-est-un-noeud",
		"attendu": "ATTRAPEE depuis le 24/08. Elle SURVIVAIT tant que Connect portait la meme exemption : "
				   "le parcours pouvait se tromper sans consequence visible. Mesure le 24/08 : elle rougit.",
		"edits": [(GRAPH_INL, _ANCRE_M38, _MUTE_M38)],
	},
	"M33": {
		"quoi": "la FAMILLE n est plus comparee dans Connect",
		"cas": "exec/refus-croise-nomme",
		"attendu": "ATTRAPEE -- c est l etat d avant : le croisement passe, ou se refuse par le TYPE.",
		"edits": [(GRAPH_INL, _ANCRE_M33, _MUTE_M33)],
	},
	"M34": {
		"quoi": "l arite d ENTREE redevient celle de la donnee -- la 2e source d exec REMPLACE",
		"cas": "exec/arite-croisee",
		"attendu": "ATTRAPEE -- neuf chemins sur dix disparaitraient sans un mot.",
		"edits": [(GRAPH_INL, _ANCRE_M34, _MUTE_M34)],
	},
	"M35": {
		"quoi": "l arite de SORTIE redevient celle de la donnee -- une instruction peut avoir DEUX suites",
		"cas": "exec/arite-croisee",
		"attendu": "ATTRAPEE -- l ordre d execution dependrait de l ordre d insertion des liens.",
		"edits": [(GRAPH_INL, _ANCRE_M35, _MUTE_M35)],
	},
	"M36": {
		"quoi": "Connect exempte a nouveau les liens d execution du controle de cycle",
		"cas": "exec/acyclicite-universelle-la-boucle-est-un-noeud",
		"attendu": "ATTRAPEE depuis le 24/08 -- et c est le resultat qui compte : elle SURVIVAIT tant que "
				   "l exemption s ecrivait AUSSI dans le parcours. Une defense redondante est invisible a "
				   "une mutation a un seul defaut ; retirer l exception a retire la redondance avec elle.",
		"edits": [(GRAPH_INL, _ANCRE_M36, _MUTE_M36)],
	},
	"M37": {
		"quoi": "la FAMILLE n est plus ecrite dans le fichier",
		"cas": "exec/aller-retour-octet-pour-octet",
		"attendu": "ATTRAPEE -- l axe entier serait perdu a la sauvegarde ; c est le meme defaut que "
				   "l empreinte non ecrite (M30).",
		"edits": [(GRAPH_IO, _ANCRE_M37, _MUTE_M37)],
	},
	"M31": {
		"quoi": "la garde du modele non nodal laisse passer les types composes",
		"cas": "nonnodal/type-compose-refuse-car-non-exprimable",
		"attendu": "ATTRAPEE -- sans elle le graphe manipule un type que la compilation n a nulle part ou ecrire.",
		"edits": [(COMPILE_H, _ANCRE_M31, _MUTE_M31)],
	},
	"M32": {
		"quoi": "le refus ne dit plus OU ca coince -- « type non supporte » au lieu de nommer NkMaterial",
		"cas": "nonnodal/type-compose-refuse-car-non-exprimable",
		"attendu": "ATTRAPEE -- un refus qui ne dit pas d ou vient la limite envoie l auteur chercher un reglage.",
		"edits": [(COMPILE_H, _ANCRE_M32, _MUTE_M32)],
	},
	"M26": {
		"quoi": "l empreinte ignore l ORDRE des membres (somme commutative)",
		"cas": "types/espace-de-noms-et-empreinte",
		"attendu": "ATTRAPEE -- les valeurs d une enumeration sont POSITIONNELLES ; permuter change le sens "
				   "des donnees deja sauvees.",
		"edits": [(GRAPH_INL, _ANCRE_M26, _MUTE_M26)],
	},
	"M27": {
		"quoi": "le conflit de definition ECRASE au lieu de refuser -- le dernier enregistre gagne",
		"cas": "types/espace-de-noms-et-empreinte",
		"attendu": "ATTRAPEE -- c est la pente naturelle d un registre idempotent, et elle rend le sens du "
				   "graphe dependant de l ORDRE d enregistrement.",
		"edits": [(GRAPH_INL, _ANCRE_M27, _MUTE_M27)],
	},
	"M28": {
		"quoi": "une FEUILLE rend une empreinte NULLE au lieu de « rien »",
		"cas": "types/espace-de-noms-et-empreinte",
		"attendu": "ATTRAPEE -- regle 3 a l etage des types ; le cas emploie une sentinelle qui doit survivre.",
		"edits": [(GRAPH_INL, _ANCRE_M28, _MUTE_M28)],
	},
	"M29": {
		"quoi": "le refus ne nomme plus CE QUI differe, seulement QUE ca differe",
		"cas": "types/espace-de-noms-et-empreinte",
		"attendu": "ATTRAPEE -- un refus muet force a comparer deux fichiers a la main.",
		"edits": [(GRAPH_INL, _ANCRE_M29, _MUTE_M29)],
	},
	"M30": {
		"quoi": "l empreinte n est plus ECRITE dans le fichier",
		"cas": "types/deux-fichiers-meme-nom-refuses-en-se-nommant",
		"attendu": "ATTRAPEE -- sans elle l accord redevient un accord de MEMOIRE, qui ne survit pas a la "
				   "sauvegarde ; c est tout l objet de la decision.",
		"edits": [(GRAPH_IO, _ANCRE_M30, _MUTE_M30)],
	},
	"M25": {
		"quoi": "M22 + M23bis : on lit `success` ET le repli remet success=true -- LE COUPLE de la regle 6",
		"cas": "rang4/emis-credible-contre-emis-qui-echoue",
		"attendu": "ATTRAPEE -- M22 seule survit depuis que NKSL rend `success` honnete. C est le couple qui "
				   "mesure ce que le controle du MOT MAGIQUE achete encore.",
		"edits": [(BANC_SRC, _ANCRE_M22, _MUTE_M22), (NKSL_CC, _ANCRE_M23BIS, _MUTE_M23BIS)],
	},
	"M23": {
		"quoi": "les erreurs de glslang ne remontent plus -- retour a l echec MUET",
		"cas": "rang4/emis-credible-contre-emis-qui-echoue",
		"attendu": "ATTRAPEE -- c est l etat exact d avant le correctif ; le cas doit le voir revenir.",
		"edits": [(NKSL_CC, _ANCRE_M23, _MUTE_M23)],
	},
	"M24": {
		"quoi": "les erreurs remontent mais SANS etiquette d etape -- deux journaux melanges a l aveugle",
		"cas": "rang4/emis-credible-contre-emis-qui-echoue",
		"attendu": "ATTRAPEE -- le mot seul ne suffit pas : l auteur doit savoir QUEL etage se plaint.",
		"edits": [(NKSL_CC, _ANCRE_M24, _MUTE_M24)],
	},
	"M23bis": {
		"quoi": "le repli remet success=true (le mensonge d origine, celui que 9033312d a retire)",
		"cas": "rang4/emis-credible-contre-emis-qui-echoue",
		"attendu": "A SURVECU attendu -- le cas lit le MOT MAGIQUE, pas `success` (regle du 22/08). "
				   "S il rougit, c est que quelque chose lit `success` sans le dire.",
		"edits": [(NKSL_CC, _ANCRE_M23BIS, _MUTE_M23BIS)],
	},
	"M21": {
		"quoi": "la substitution du jeton porte AUSSI sur la declaration -- le jeton devient DECLARE",
		"cas": "rang4/emis-credible-contre-emis-qui-echoue",
		"attendu": "ATTRAPEE -- c est la faute que j ai faite en ecrivant le cas : une mutation qui renomme "
				   "la declaration ne modele pas le defaut, elle renomme une variable.",
		"edits": [(BANC_SRC, _ANCRE_M21, _MUTE_M21)],
	},
	"M22": {
		"quoi": "le verdict de glslang se lit dans `success` au lieu du MOT MAGIQUE",
		"cas": "rang4/emis-credible-contre-emis-qui-echoue",
		"attendu": "A SURVECU depuis le correctif NKSL du 23/08 au soir, et c est ATTENDU. Tant que le repli "
				   "preservait `success`, lire `success` mentait et cette mutation etait attrapee. Maintenant "
				   "que NKSL le rend HONNETE, `success` et le mot magique s accordent : la defense est devenue "
				   "REDONDANTE, donc invisible a une mutation a un seul defaut (regle 6). C est M25 -- M22 + "
				   "M23bis -- qui mesure ce qu elle achete encore.",
		"edits": [(BANC_SRC, _ANCRE_M22, _MUTE_M22)],
	},
	"M16": {
		"quoi": "l ecrivain remet l INDEX dans la ligne `lien` -- retour au defaut v1 sous une version qui annonce 2",
		"cas": "fichier/ordre-des-sock",
		"attendu": "ATTRAPEE -- sinon le temoin ne mesure pas ce qu il annonce.",
		"edits": [(GRAPH_IO, _ANCRE_M16, _MUTE_M16)],
	},
	"M17": {
		"quoi": "le refus devient un repli sur la prise 0 -- « rien » charge comme « la premiere »",
		"cas": "fichier/prise-inconnue-refusee-en-se-nommant",
		"attendu": "ATTRAPEE -- c est LA pente naturelle, et elle rend un graphe qui CHARGE.",
		"edits": [(GRAPH_IO, _ANCRE_M17, _MUTE_M17)],
	},
	"M18": {
		"quoi": "la migration disparait : un fichier v1 est lu comme de la v2",
		"cas": "fichier/migration-version-1",
		"attendu": "ATTRAPEE -- une migration que rien ne mesure se fait retirer par le premier qui la croit morte.",
		"edits": [(GRAPH_IO, _ANCRE_M18, _MUTE_M18)],
	},
	"M19": {
		"quoi": "retour a UNE seule passe de lecture -- la resolution redevient dependante de l ORDRE",
		"cas": "fichier/ordre-des-sock",
		"attendu": "ATTRAPEE -- c est le defaut que le temoin a trouve tout seul ; on verifie qu il le retrouverait.",
		"edits": [(GRAPH_IO, _ANCRE_M19, _MUTE_M19)],
	},
	"M20": {
		"quoi": "le graphe n est plus vide apres un refus (le `false` est toujours rendu)",
		"cas": "fichier/prise-inconnue-refusee-en-se-nommant",
		"attendu": "ATTRAPEE -- seul un cas qui REGARDE la matiere restante peut voir ce defaut-la.",
		"edits": [(GRAPH_IO, _ANCRE_M20, _MUTE_M20)],
	},
	"M11": {
		"quoi": "le second filet (site d emission) REMET le repli plausible : vUV / vColor pour n importe quel nom",
		"cas": "rang4/canal-nomme-refuse-en-se-nommant",
		"attendu": "SURVIT -- le site est inatteignable tant que la passe 1 refuse. C est le fait mesure du 23/08.",
		"edits": [(COMPILE_H, _ANCRE_FILET_UV, _MUTE_FILET_UV), (COMPILE_H, _ANCRE_FILET_ATTR, _MUTE_FILET_ATTR)],
	},
	"M12": {
		"quoi": "retirer la PREMIERE passe de refus (les noms de canaux ne sont plus verifies avant emission)",
		"cas": "rang4/canal-nomme-refuse-en-se-nommant",
		"attendu": "ATTRAPEE -- le second filet refuse, mais son message perd le nom demande et la liste.",
		"edits": [(COMPILE_H, _ANCRE_PASSE1, _MUTE_PASSE1)],
	},
	"M13": {
		"quoi": "M11 + M12 : les DEUX lignes de defense tombent en meme temps",
		"cas": "rang4/canal-nomme-refuse-en-se-nommant",
		"attendu": "ATTRAPEE -- et c est le SEUL etat qui rend une image credible et fausse. "
				   "Il mesure ce que le second filet achete vraiment.",
		"edits": [
			(COMPILE_H, _ANCRE_PASSE1, _MUTE_PASSE1),
			(COMPILE_H, _ANCRE_FILET_UV, _MUTE_FILET_UV),
			(COMPILE_H, _ANCRE_FILET_ATTR, _MUTE_FILET_ATTR),
		],
	},
	"M15": {
		"quoi": "l insertion de la prise intruse devient un no-op -- le cas index/nom mesure-t-il l INSERTION ?",
		"cas": "graphe/index-de-prise-contre-nom",
		"attendu": "ATTRAPEE -- sinon le cas serait vert pour une autre raison que celle qu il annonce (regle 1).",
		"edits": [(BANC_SRC, _ANCRE_M15, _MUTE_M15)],
	},
	"M14": {
		"quoi": "M12 + le second filet DANS SA VERSION D AVANT LE CORRECTIF (jeton imprononcable au lieu de refus)",
		"cas": "rang4/canal-nomme-refuse-en-se-nommant",
		"attendu": "ATTRAPEE, mais A COMPARER A M12 : c est le seul couple qui mesure ce que le correctif a achete.",
		"edits": [
			(COMPILE_H, _ANCRE_PASSE1, _MUTE_PASSE1),
			(COMPILE_H, _ANCRE_FILET_UV, _MUTE_FILET_UV_JETON),
			(COMPILE_H, _ANCRE_FILET_ATTR, _MUTE_FILET_ATTR_JETON),
		],
	},
}


def _git(*args):
	return subprocess.run(["git"] + list(args), cwd=RACINE, capture_output=True, text=True)


def arbre_propre():
	r = _git("status", "--porcelain")
	return r.stdout.strip() == "", r.stdout.strip()


def construis():
	"""Rend (ok, nb_erreurs_lues, sortie)."""
	# ⚠️ jenga imprime des cadres UTF-8 ; la console Windows decode en cp1252 et
	# LEVE. Un decodage qui echoue ici tuerait la campagne APRES la mutation --
	# c est exactement comme ca que la session precedente s est arretee.
	r = subprocess.run(["jenga", "build", "--target", "NkMatGraphCheck"],
					   cwd=RACINE, capture_output=True, text=True,
					   encoding="utf-8", errors="replace", shell=(os.name == "nt"))
	txt = (r.stdout or "") + (r.stderr or "")
	nb = txt.lower().count("error:") + txt.lower().count(" error ")
	return (r.returncode == 0 and "BUILD COMPLETED" in txt and "SUCCESS" in txt), nb, txt


def joue_le_banc():
	r = subprocess.run([os.path.join(RACINE, BANC.replace("/", os.sep))],
					   cwd=RACINE, capture_output=True, text=True,
					   encoding="utf-8", errors="replace")
	return r.returncode, (r.stdout or "") + (r.stderr or "")


# ⚠️ « ECHEC » apparait AUSSI dans le detail des cas (« GLSLANG=ECHEC »), ou il
# est parfois ATTENDU. Le verdict est le CHAMP D ETAT, pas le mot : il se lit
# entre le nom du cas et la premiere barre.
_LIGNE_CAS = re.compile(r"^(\S+)\s+(OK|ECHEC)\s+\|")


def _etat(ligne):
	m = _LIGNE_CAS.match(ligne.strip())
	return (m.group(1), m.group(2)) if m else (None, None)


def verdict_du_cas(sortie, nom_cas):
	for ligne in sortie.splitlines():
		nom, etat = _etat(ligne)
		if nom == nom_cas:
			return ligne.strip(), etat
	return None, None


def compte_echecs(sortie):
	for ligne in sortie.splitlines():
		if ligne.startswith("-- ") and " cas, " in ligne:
			return ligne.strip()
	return "(compte introuvable -- LE BANC N A PAS FINI)"


def applique(edits):
	"""Rend (ok, message). Refuse si une ancre n'apparait pas exactement une fois.

	⚠️ LES SOURCES DU DEPOT SONT EN CRLF, LES ANCRES DE CE FICHIER EN LF. Une
	comparaison naive rend « ancrage x0 » sur une ancre parfaitement juste --
	et la garde 2 le dirait, mais en accusant l'ancre au lieu du saut de ligne.
	On lit donc le fichier en le NORMALISANT, et on REECRIT avec la fin de
	ligne d'origine : muter un fichier ne doit pas le reformater.
	"""
	contenus = {}
	crlf = {}
	# Verification de TOUTES les ancres AVANT d'ecrire quoi que ce soit.
	for (fic, ancre, _) in edits:
		chemin = os.path.join(RACINE, fic.replace("/", os.sep))
		if fic not in contenus:
			with open(chemin, "rb") as f:
				brut = f.read().decode("utf-8")
			crlf[fic] = "\r\n" in brut
			contenus[fic] = brut.replace("\r\n", "\n")
		n = contenus[fic].count(ancre)
		if n != 1:
			return False, "ancrage x%d (1 attendu) dans %s -- ON NE MUTE PAS" % (n, fic)
	for (fic, ancre, remp) in edits:
		contenus[fic] = contenus[fic].replace(ancre, remp, 1)
	for fic, txt in contenus.items():
		if crlf[fic]:
			txt = txt.replace("\n", "\r\n")
		with open(os.path.join(RACINE, fic.replace("/", os.sep)), "wb") as f:
			f.write(txt.encode("utf-8"))
	return True, "ancrages x1"


def restaure(edits):
	"""Restaure les SOURCES **et reconstruit**.

	🔴 GARDE 5 -- MESUREE LE 2026-08-23, PAR MON PROPRE SCRIPT CONTRE MOI.
	Restaurer la source ne suffit pas : le BINAIRE reste celui de la derniere
	mutation. `git status` dit « propre », la campagne dit « restauration :
	arbre propre », et le banc lance juste apres rend UN ECHEC pour un defaut
	qui n'existe plus nulle part. C'est exactement la regle 2 -- le binaire
	perime -- fabriquee par l'instrument cense la faire respecter.

	Le piege est pire que l'original : d'habitude un binaire perime rend un
	VERT sur du code casse. Ici il rend un ROUGE sur du code sain, et le
	premier reflexe est de chercher le defaut dans la source. On reconstruit
	donc, et on le DIT.
	"""
	fics = sorted({fic for (fic, _, _) in edits})
	_git("checkout", "--", *fics)
	propre, sale = arbre_propre()
	bien, _, _ = construis()
	return propre, sale, bien


def joue(cle):
	m = MUTATIONS[cle]
	print("\n" + "=" * 78)
	print("%s : %s" % (cle, m["quoi"]))
	print("   cas vise : %s" % m["cas"])
	print("   attendu  : %s" % m["attendu"])
	print("=" * 78)

	propre, sale = arbre_propre()
	if not propre:
		print("REFUS -- L ARBRE N EST PAS PROPRE. La restauration effacerait ce travail :")
		print(sale)
		print("Commite d abord. (C est la garde qui a coute deux nuits.)")
		return "REFUS"

	ok, msg = applique(m["edits"])
	print("   mutation : %s" % msg)
	if not ok:
		return "REFUS"

	try:
		bien, nberr, _ = construis()
		# GARDE 4 : le compte d'erreurs AVANT le banc.
		#
		# 🔴 GARDE 6 -- ON REJOUE LA CONSTRUCTION AVANT D'ACCUSER LA MUTATION.
		# Mesure du 2026-08-23 : M18 a rendu « NE COMPILE PAS » dans une campagne
		# de douze, et « ATTRAPEE » relancee seule, sans qu'une ligne ait bouge.
		# Une construction qui echoue une fois sur deux n'accuse pas la mutation,
		# elle accuse le LANCEUR -- verrou de fichier, artefact a moitie ecrit,
		# course entre deux reconstructions rapprochees.
		#
		# Un verdict non reproductible est un verdict faux, meme quand il est
		# prudent. On distingue donc les deux : ce qui ne compile pas DEUX FOIS
		# est une mutation qui ne compile pas ; ce qui ne compile qu'une fois est
		# un incident de construction, et il se DIT.
		if not bien:
			print("   construction : ECHEC (%d erreur(s) lue(s)) -- ON REJOUE avant d'accuser la mutation" % nberr)
			bien, nberr, _ = construis()
			if bien:
				print("   construction : OK a la seconde tentative -- INCIDENT DE LANCEUR, pas la mutation")
		print("   construction : %s (%d erreur(s) lue(s))" % ("OK" if bien else "ECHEC", nberr))
		if not bien:
			print("   => la mutation NE COMPILE PAS (deux fois de suite) : elle ne mesure rien.")
			return "NE COMPILE PAS"
		code, sortie = joue_le_banc()
		if code != 0 and "cas, " not in sortie:
			print("   => LE BANC EST MORT (code %s). Une mutation qui fait tomber le banc ne" % hex(code & 0xFFFFFFFF))
			print("      prouve pas que le cas mesure ; elle prouve qu on ne peut plus le lire.")
			return "BANC MORT"
		print("   %s" % compte_echecs(sortie))
		ligne, etat = verdict_du_cas(sortie, m["cas"])
		print("   cas vise : %s" % (ligne if ligne else "(ABSENT DE LA SORTIE)"))
		attrapee = etat == "ECHEC"
		# une mutation est aussi attrapee si un AUTRE cas rougit
		autres = []
		for l in sortie.splitlines():
			nom, e = _etat(l)
			if e == "ECHEC" and nom != m["cas"]:
				autres.append(l.strip())
		if autres:
			print("   autres cas rouges : %d" % len(autres))
			for l in autres[:6]:
				print("      %s" % l[:150])
		verdict = "ATTRAPEE" if (attrapee or autres) else "A SURVECU"
		print("   VERDICT : %s%s" % (verdict, "" if attrapee else (" (par un autre cas)" if autres else "")))
		return verdict
	finally:
		propre, sale, rebati = restaure(m["edits"])
		print("   restauration : %s, binaire rebati : %s"
			  % ("arbre propre" if propre else "⚠️ ARBRE SALE : " + sale,
				 "oui" if rebati else "⚠️ NON -- le banc rendrait un ROUGE sur du code sain"))


def main():
	if "--liste" in sys.argv[1:] or not sys.argv[1:]:
		for k in sorted(MUTATIONS):
			print("%s  %s" % (k, MUTATIONS[k]["quoi"]))
			print("      cas : %s" % MUTATIONS[k]["cas"])
			print("      attendu : %s" % MUTATIONS[k]["attendu"])
		return 0
	resultats = []
	for cle in sys.argv[1:]:
		if cle not in MUTATIONS:
			print("mutation inconnue : %s" % cle)
			return 2
		resultats.append((cle, joue(cle)))
	print("\n" + "=" * 78)
	print("RECAPITULATIF")
	for cle, v in resultats:
		print("   %-5s %-14s (attendu : %s)" % (cle, v, MUTATIONS[cle]["attendu"].split(" -- ")[0]))
	print("=" * 78)
	return 0


if __name__ == "__main__":
	sys.exit(main())
