#pragma once
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Kernel/System/NKConverse/src/NKConverse/NkConverseFauxServeur.h
// @Brief   UN FAUX SERVEUR DE MODELES, LOCAL, QUI PARLE LES VRAIS PROTOCOLES :
//          Ollama (/api/tags, /api/show, /api/chat en NDJSON), OpenAI
//          (/v1/models, /v1/chat/completions en SSE) et Anthropic (/v1/messages
//          en SSE). Pour les bancs.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// ⚠️ CE N'EST PAS UN BOUCHON DE CONFORT -- c'est l'instrument qui permet de
//    prouver toute la chaine (liste des modeles, flux morceau par morceau,
//    appels d'outils, refus nommes) SANS installer Ollama, SANS modele, SANS
//    reseau, et de facon REPRODUCTIBLE : un banc cale sur un vrai modele
//    dependrait de ce que le modele a envie de repondre ce jour-la. Il passe par
//    de VRAIS sockets sur 127.0.0.1 : le client HTTP, le decoupage `chunked`, la
//    lecture en flux et le decodeur sont ceux de la production.
//    Condition de retrait : aucune. Il reste a cote des vrais fournisseurs,
//    comme `NkConverseBackendConserve` a cote des vrais dorsaux.
//
// CE QU'IL FAIT
//   - Les MODELES declares (nom, outils oui/non) : /api/tags et /v1/models les
//     listent ; /api/show dit la capacite « tools ».
//   - Des TOURS scriptes, rendus dans l'ordre (une file) : des morceaux de
//     texte, envoyes un par un avec une pause (le flux se VOIT), puis des
//     appels d'outils -- chez OpenAI, les arguments coupes en DEUX fragments,
//     comme le font les vrais serveurs.
//   - Les REFUS des vrais services : modele inconnu (404), cle refusee (401),
//     outils refuses par un modele qui ne les sait pas (400, le texte exact
//     d'Ollama).
//   - Il RETIENT chaque corps de conversation recu : le banc lit ce qui est
//     REELLEMENT parti (le message systeme, les outils, les resultats).
// -----------------------------------------------------------------------------

#include "NKConverse/NkConverseFournisseurs.h"
#include "NKCore/NkAtomic.h"
#include "NKNetwork/Transport/NkSocket.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkScopedLock.h"
#include "NKThreading/NkThread.h"
#include "NKTime/NkChrono.h"

#include <cstdio>
#include <cstring>

namespace nkentseu::converse {

	class NkFauxServeurIA {
		public:
			struct Modele {
					NkString nom;
					bool outils = true;
			};
			struct Tour {
					nkentseu::NkVector<NkString> morceaux;
					nkentseu::NkVector<NkAppelOutil> appels;
			};

			nkentseu::NkVector<Modele> modeles;
			/// Non vide : OpenAI exige « Authorization: Bearer <cle> », Anthropic
			/// « x-api-key: <cle> ». Ollama n'en demande jamais (comme le vrai).
			NkString cleAttendue;
			nkentseu::uint32 pauseMs = 30u; ///< entre deux morceaux du flux

			NkFauxServeurIA() = default;
			NkFauxServeurIA(const NkFauxServeurIA &) = delete;
			NkFauxServeurIA &operator=(const NkFauxServeurIA &) = delete;
			~NkFauxServeurIA() {
				Arreter();
			}

			/// Ecoute sur 127.0.0.1, au premier port libre a partir de `premier`.
			bool Demarrer(nkentseu::uint16 premier = 39410u) {
				Arreter();
				(void)nkentseu::net::NkSocket::PlatformInit();
				for (nkentseu::uint16 p = premier; p < premier + 40u; ++p) {
					if (mEcoute.Create(nkentseu::net::NkAddress::Loopback(p), nkentseu::net::NkSocket::Type::NK_TCP) !=
						nkentseu::net::NkNetResult::NK_NET_OK)
						continue;
					if (mEcoute.Listen(8u) != nkentseu::net::NkNetResult::NK_NET_OK) {
						mEcoute.Close();
						continue;
					}
					(void)mEcoute.SetNonBlocking(true);
					mPort = p;
					break;
				}
				if (mPort == 0u)
					return false;
				mStop.Store(false);
				mFil = new nkentseu::threading::NkThread([this](void *) { Boucle(); });
				return true;
			}
			void Arreter() {
				if (mFil) {
					mStop.Store(true);
					mFil->Join();
					delete mFil;
					mFil = nullptr;
				}
				mEcoute.Close();
				mPort = 0u;
			}
			nkentseu::uint16 Port() const {
				return mPort;
			}
			NkString Adresse() const {
				char b[48];
				std::snprintf(b, sizeof(b), "http://127.0.0.1:%u", static_cast<unsigned>(mPort));
				return NkString(b);
			}

			void Pousser(const Tour &t) {
				threading::NkLockGuard g(mVerrou);
				mTours.PushBack(t);
			}
			/// Raccourcis : un tour de texte coupe en morceaux, un tour d'outil.
			void PousserTexte(const char *const *morceaux, nkentseu::uint32 n) {
				Tour t;
				for (nkentseu::uint32 i = 0; i < n; ++i)
					t.morceaux.PushBack(NkString(morceaux[i]));
				Pousser(t);
			}
			void PousserOutil(const char *nom, const char *argumentsJson, const char *texteAvant = nullptr) {
				Tour t;
				if (texteAvant)
					t.morceaux.PushBack(NkString(texteAvant));
				NkAppelOutil a;
				a.nom = NkString(nom);
				a.arguments = NkString(argumentsJson ? argumentsJson : "{}");
				t.appels.PushBack(a);
				Pousser(t);
			}
			nkentseu::uint32 ToursRestants() {
				threading::NkLockGuard g(mVerrou);
				return static_cast<nkentseu::uint32>(mTours.Size());
			}
			/// Les corps des requetes de CONVERSATION recues, dans l'ordre.
			nkentseu::uint32 Requetes() {
				threading::NkLockGuard g(mVerrou);
				return static_cast<nkentseu::uint32>(mCorps.Size());
			}
			NkString Requete(nkentseu::uint32 i) {
				threading::NkLockGuard g(mVerrou);
				return i < mCorps.Size() ? mCorps[i] : NkString();
			}
			NkString DerniereRequete() {
				threading::NkLockGuard g(mVerrou);
				return mCorps.Empty() ? NkString() : mCorps[mCorps.Size() - 1];
			}
			/// Tous les chemins demandes (« GET /api/tags »...).
			nkentseu::uint32 Appels() {
				threading::NkLockGuard g(mVerrou);
				return static_cast<nkentseu::uint32>(mChemins.Size());
			}

		private:
			nkentseu::net::NkSocket mEcoute;
			nkentseu::uint16 mPort = 0u;
			nkentseu::threading::NkThread *mFil = nullptr;
			nkentseu::NkAtomicBool mStop{false};
			threading::NkMutex mVerrou;
			nkentseu::NkVector<Tour> mTours;
			nkentseu::NkVector<NkString> mCorps;
			nkentseu::NkVector<NkString> mChemins;
			nkentseu::uint32 mNumeroAppel = 0u;

			void Boucle() {
				while (!mStop.Load()) {
					nkentseu::net::NkSocket client;
					nkentseu::net::NkAddress de;
					if (mEcoute.Accept(client, de) == nkentseu::net::NkNetResult::NK_NET_OK && client.IsValid()) {
						(void)client.SetNonBlocking(false);
						Servir(client);
						client.Close();
						continue;
					}
					nkentseu::NkChrono::Sleep(static_cast<nkentseu::int64>(2));
				}
			}

			static bool Envoyer(nkentseu::net::NkSocket &s, const NkString &t) {
				return t.Empty() || s.Send(t.CStr(), static_cast<nkentseu::uint32>(t.Length())) ==
										nkentseu::net::NkNetResult::NK_NET_OK;
			}
			static void Repondre(nkentseu::net::NkSocket &s, nkentseu::uint32 statut, const char *type, const NkString &corps) {
				char b[256];
				std::snprintf(b, sizeof(b), "HTTP/1.1 %u %s\r\nContent-Type: %s\r\nContent-Length: %u\r\nConnection: close\r\n\r\n",
							  static_cast<unsigned>(statut), statut == 200u ? "OK" : "Erreur", type,
							  static_cast<unsigned>(corps.Length()));
				Envoyer(s, NkString(b));
				Envoyer(s, corps);
			}
			void DebutFlux(nkentseu::net::NkSocket &s, const char *type) {
				char b[200];
				std::snprintf(b, sizeof(b),
							  "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n", type);
				Envoyer(s, NkString(b));
			}
			void Morceau(nkentseu::net::NkSocket &s, const NkString &d) {
				char b[24];
				std::snprintf(b, sizeof(b), "%X\r\n", static_cast<unsigned>(d.Length()));
				NkString m(b);
				m.Append(d);
				m.Append("\r\n");
				Envoyer(s, m);
				if (pauseMs)
					nkentseu::NkChrono::Sleep(static_cast<nkentseu::int64>(pauseMs));
			}
			void FinFlux(nkentseu::net::NkSocket &s) {
				Envoyer(s, NkString("0\r\n\r\n"));
			}

			static NkString Entete(const NkString &entetes, const char *nom) {
				// recherche sans casse de « \r\nnom: »
				NkString bas = entetes;
				bas.ToLower();
				NkString cle("\r\n");
				cle.Append(nom);
				cle.Append(":");
				const NkString::SizeType i = bas.Find(cle.CStr());
				if (i == NkString::npos)
					return NkString();
				NkString::SizeType a = i + cle.Length();
				NkString::SizeType f = entetes.Find("\r\n", a);
				NkString v = entetes.SubStr(a, (f == NkString::npos ? entetes.Length() : f) - a);
				v.Trim();
				return v;
			}

			bool ModeleConnu(const NkString &nom, bool &outils) const {
				for (nkentseu::usize i = 0; i < modeles.Size(); ++i)
					if (modeles[i].nom == nom) {
						outils = modeles[i].outils;
						return true;
					}
				return false;
			}
			Tour Suivant() {
				threading::NkLockGuard g(mVerrou);
				if (mTours.Empty()) {
					Tour t;
					t.morceaux.PushBack(NkString("D'accord."));
					return t;
				}
				Tour t = mTours[0];
				mTours.Erase(mTours.Begin());
				return t;
			}
			NkString IdAppel() {
				char b[32];
				std::snprintf(b, sizeof(b), "call_%u", static_cast<unsigned>(++mNumeroAppel));
				return NkString(b);
			}

			void Servir(nkentseu::net::NkSocket &s) {
				// ── LIRE la requete : en-tetes, puis Content-Length octets ──
				NkString brut;
				char buf[4096];
				NkString::SizeType finEntetes = NkString::npos;
				nkentseu::uint32 longueur = 0u;
				nkentseu::NkChrono horloge;
				for (;;) {
					nkentseu::uint32 n = 0u;
					if (s.Recv(buf, sizeof(buf), n) != nkentseu::net::NkNetResult::NK_NET_OK || n == 0u)
						break;
					brut.Append(buf, static_cast<NkString::SizeType>(n));
					if (finEntetes == NkString::npos) {
						const NkString::SizeType p = brut.Find("\r\n\r\n");
						if (p != NkString::npos) {
							finEntetes = p + 4u;
							const NkString cl = Entete(brut.SubStr(0, p + 2), "content-length");
							longueur = static_cast<nkentseu::uint32>(std::atoi(cl.CStr()));
						}
					}
					if (finEntetes != NkString::npos && brut.Length() - finEntetes >= longueur)
						break;
					if (horloge.Elapsed().ToSeconds() > 5.0)
						break;
				}
				if (finEntetes == NkString::npos)
					return;
				const NkString entetes = NkString("\r\n") + brut.SubStr(0, finEntetes - 2);
				const NkString corps = brut.SubStr(finEntetes);
				const NkString::SizeType sp1 = brut.Find(' ');
				const NkString::SizeType sp2 = brut.Find(' ', sp1 + 1);
				const NkString methode = brut.SubStr(0, sp1);
				const NkString chemin = brut.SubStr(sp1 + 1, sp2 - sp1 - 1);
				{
					threading::NkLockGuard g(mVerrou);
					mChemins.PushBack(methode + " " + chemin);
				}

				// ── LES CLES ──
				const bool cheminOpenAI = chemin.StartsWith("/v1/chat") || chemin == "/v1/models";
				const bool cheminAnthropic = chemin.StartsWith("/v1/messages");
				if (!cleAttendue.Empty() && (cheminOpenAI || cheminAnthropic)) {
					const NkString bearer = Entete(entetes, "authorization");
					const NkString xkey = Entete(entetes, "x-api-key");
					const bool ok = bearer == NkString("Bearer ") + cleAttendue || xkey == cleAttendue;
					if (!ok) {
						if (!xkey.Empty() || cheminAnthropic)
							Repondre(s, 401u, "application/json",
									 NkString("{\"type\":\"error\",\"error\":{\"type\":\"authentication_error\",\"message\":\"invalid x-api-key\"}}"));
						else
							Repondre(s, 401u, "application/json",
									 NkString("{\"error\":{\"message\":\"Incorrect API key provided\",\"type\":\"invalid_request_error\"}}"));
						return;
					}
				}

				// ── LES LISTES ──
				if (methode == "GET" && chemin == "/api/tags") {
					NkString j("{\"models\":[");
					for (nkentseu::usize i = 0; i < modeles.Size(); ++i) {
						if (i)
							j.Append(',');
						j.Append("{\"name\":");
						NkJsonChaine(modeles[i].nom, j);
						j.Append(",\"model\":");
						NkJsonChaine(modeles[i].nom, j);
						j.Append(",\"size\":4683087332,\"details\":{\"family\":\"qwen2\",\"parameter_size\":\"7.6B\","
								 "\"quantization_level\":\"Q4_K_M\"}}");
					}
					j.Append("]}");
					Repondre(s, 200u, "application/json", j);
					return;
				}
				if (methode == "POST" && chemin == "/api/show") {
					NkJsonDoc d;
					bool outils = false;
					if (!d.Lire(corps) || !ModeleConnu(d.TexteDe(d.Racine(), "model"), outils)) {
						Repondre(s, 404u, "application/json", NkString("{\"error\":\"model not found\"}"));
						return;
					}
					Repondre(s, 200u, "application/json",
							 NkString(outils ? "{\"capabilities\":[\"completion\",\"tools\"]}" : "{\"capabilities\":[\"completion\"]}"));
					return;
				}
				if (methode == "GET" && chemin == "/v1/models") {
					NkString j("{\"object\":\"list\",\"data\":[");
					for (nkentseu::usize i = 0; i < modeles.Size(); ++i) {
						if (i)
							j.Append(',');
						j.Append("{\"id\":");
						NkJsonChaine(modeles[i].nom, j);
						j.Append(",\"object\":\"model\",\"display_name\":");
						NkJsonChaine(modeles[i].nom, j);
						j.Append('}');
					}
					j.Append("]}");
					Repondre(s, 200u, "application/json", j);
					return;
				}

				// ── LES CONVERSATIONS ──
				const bool ollama = methode == "POST" && chemin == "/api/chat";
				const bool openai = methode == "POST" && chemin == "/v1/chat/completions";
				const bool anthropic = methode == "POST" && cheminAnthropic;
				if (!ollama && !openai && !anthropic) {
					Repondre(s, 404u, "text/plain", NkString("404 page not found"));
					return;
				}
				{
					threading::NkLockGuard g(mVerrou);
					mCorps.PushBack(corps);
				}
				NkJsonDoc d;
				if (!d.Lire(corps)) {
					Repondre(s, 400u, "application/json", NkString("{\"error\":\"invalid JSON body\"}"));
					return;
				}
				const NkString modele = d.TexteDe(d.Racine(), "model");
				bool outils = false;
				if (!ModeleConnu(modele, outils)) {
					NkString e;
					if (ollama) {
						e = NkString("{\"error\":\"model \\\"");
						NkJsonEchapper(modele, e);
						e.Append("\\\" not found, try pulling it first\"}");
					} else {
						e = NkString("{\"error\":{\"message\":\"The model `");
						NkJsonEchapper(modele, e);
						e.Append("` does not exist\",\"type\":\"invalid_request_error\"}}");
					}
					Repondre(s, 404u, "application/json", e);
					return;
				}
				if (ollama && !outils && d.Membre(d.Racine(), "tools") >= 0) {
					NkString e("{\"error\":\"registry.ollama.ai/library/");
					NkJsonEchapper(modele, e);
					e.Append(" does not support tools\"}");
					Repondre(s, 400u, "application/json", e);
					return;
				}
				const Tour t = Suivant();
				if (ollama) {
					DebutFlux(s, "application/x-ndjson");
					for (nkentseu::usize i = 0; i < t.morceaux.Size(); ++i) {
						NkString l("{\"model\":");
						NkJsonChaine(modele, l);
						l.Append(",\"message\":{\"role\":\"assistant\",\"content\":");
						NkJsonChaine(t.morceaux[i], l);
						l.Append("},\"done\":false}\n");
						Morceau(s, l);
					}
					if (!t.appels.Empty()) {
						NkString l("{\"model\":");
						NkJsonChaine(modele, l);
						l.Append(",\"message\":{\"role\":\"assistant\",\"content\":\"\",\"tool_calls\":[");
						for (nkentseu::usize k = 0; k < t.appels.Size(); ++k) {
							if (k)
								l.Append(',');
							l.Append("{\"function\":{\"name\":");
							NkJsonChaine(t.appels[k].nom, l);
							l.Append(",\"arguments\":");
							l.Append(t.appels[k].arguments);
							l.Append("}}");
						}
						l.Append("]},\"done\":false}\n");
						Morceau(s, l);
					}
					NkString f("{\"model\":");
					NkJsonChaine(modele, f);
					f.Append(",\"message\":{\"role\":\"assistant\",\"content\":\"\"},\"done\":true,\"done_reason\":\"stop\","
							 "\"prompt_eval_count\":120,\"eval_count\":24}\n");
					Morceau(s, f);
					FinFlux(s);
					return;
				}
				if (openai) {
					DebutFlux(s, "text/event-stream");
					for (nkentseu::usize i = 0; i < t.morceaux.Size(); ++i) {
						NkString l("data: {\"choices\":[{\"index\":0,\"delta\":{\"content\":");
						NkJsonChaine(t.morceaux[i], l);
						l.Append("}}]}\n\n");
						Morceau(s, l);
					}
					for (nkentseu::usize k = 0; k < t.appels.Size(); ++k) {
						// LES ARGUMENTS EN DEUX FRAGMENTS, comme les vrais serveurs.
						const NkString &a = t.appels[k].arguments;
						const NkString::SizeType mi = a.Length() / 2u;
						char idx[16];
						std::snprintf(idx, sizeof(idx), "%u", static_cast<unsigned>(k));
						NkString l1("data: {\"choices\":[{\"index\":0,\"delta\":{\"tool_calls\":[{\"index\":");
						l1.Append(idx);
						l1.Append(",\"id\":");
						NkJsonChaine(IdAppel(), l1);
						l1.Append(",\"type\":\"function\",\"function\":{\"name\":");
						NkJsonChaine(t.appels[k].nom, l1);
						l1.Append(",\"arguments\":");
						NkJsonChaine(a.SubStr(0, mi), l1);
						l1.Append("}}]}}]}\n\n");
						Morceau(s, l1);
						NkString l2("data: {\"choices\":[{\"index\":0,\"delta\":{\"tool_calls\":[{\"index\":");
						l2.Append(idx);
						l2.Append(",\"function\":{\"arguments\":");
						NkJsonChaine(a.SubStr(mi), l2);
						l2.Append("}}]}}]}\n\n");
						Morceau(s, l2);
					}
					Morceau(s, NkString(t.appels.Empty() ? "data: {\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"stop\"}]}\n\n"
														   : "data: {\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"tool_calls\"}]}\n\n"));
					Morceau(s, NkString("data: [DONE]\n\n"));
					FinFlux(s);
					return;
				}
				// ANTHROPIC
				DebutFlux(s, "text/event-stream");
				Morceau(s, NkString("event: message_start\ndata: {\"type\":\"message_start\",\"message\":{\"usage\":{\"input_tokens\":120}}}\n\n"));
				nkentseu::uint32 bloc = 0u;
				if (!t.morceaux.Empty()) {
					Morceau(s, NkString("event: content_block_start\ndata: {\"type\":\"content_block_start\",\"index\":0,"
										"\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\n"));
					for (nkentseu::usize i = 0; i < t.morceaux.Size(); ++i) {
						NkString l("event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"index\":0,"
								   "\"delta\":{\"type\":\"text_delta\",\"text\":");
						NkJsonChaine(t.morceaux[i], l);
						l.Append("}}\n\n");
						Morceau(s, l);
					}
					Morceau(s, NkString("event: content_block_stop\ndata: {\"type\":\"content_block_stop\",\"index\":0}\n\n"));
					bloc = 1u;
				}
				for (nkentseu::usize k = 0; k < t.appels.Size(); ++k, ++bloc) {
					char idx[16];
					std::snprintf(idx, sizeof(idx), "%u", static_cast<unsigned>(bloc));
					NkString l("event: content_block_start\ndata: {\"type\":\"content_block_start\",\"index\":");
					l.Append(idx);
					l.Append(",\"content_block\":{\"type\":\"tool_use\",\"id\":");
					NkJsonChaine(IdAppel(), l);
					l.Append(",\"name\":");
					NkJsonChaine(t.appels[k].nom, l);
					l.Append(",\"input\":{}}}\n\n");
					Morceau(s, l);
					const NkString &a = t.appels[k].arguments;
					const NkString::SizeType mi = a.Length() / 2u;
					for (int part = 0; part < 2; ++part) {
						NkString p("event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"index\":");
						p.Append(idx);
						p.Append(",\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":");
						NkJsonChaine(part == 0 ? a.SubStr(0, mi) : a.SubStr(mi), p);
						p.Append("}}\n\n");
						Morceau(s, p);
					}
				}
				NkString md("event: message_delta\ndata: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"");
				md.Append(t.appels.Empty() ? "end_turn" : "tool_use");
				md.Append("\"},\"usage\":{\"output_tokens\":24}}\n\n");
				Morceau(s, md);
				Morceau(s, NkString("event: message_stop\ndata: {\"type\":\"message_stop\"}\n\n"));
				FinFlux(s);
			}
	};

} // namespace nkentseu::converse
