#pragma once
// =============================================================================
// NetworkLobbyScene.h
// -----------------------------------------------------------------------------
// Lobby reseau LAN (Phase 1) — permet a 2 joueurs de se connecter avant
// de lancer un match. 2 actions principales :
//   - HEBERGER : ouvre un serveur sur le port 7777, attend qu'un peer rejoigne.
//   - REJOINDRE : tente de se connecter a une IP/port saisi.
//
// Etats afficher a l'user :
//   - IDLE       : choix initial entre HEBERGER et REJOINDRE
//   - HOSTING    : "EN ATTENTE D'UN JOUEUR..." + port d'ecoute
//   - JOINING    : "CONNEXION EN COURS..." vers IP cible
//   - CONNECTED  : "PEER CONNECTE" + bouton LANCER LE MATCH
//   - ERROR      : message d'erreur
//
// Quand l'user clique LANCER LE MATCH (host) ou recoit "MATCH START" (client),
// le SceneManager push GameplayScene avec le flag reseau actif.
//
// Phase 1 = uniquement la connexion. Phase 2 ajoutera le sync gameplay.
// =============================================================================

#include "Pong/UI/Scene.h"

namespace nkentseu
{
    namespace pong
    {

        class NetworkLobbyScene : public Scene
        {
        public:
            static constexpr int kMaxIpLen = 32;
            static constexpr uint16 kDefaultPort = 7777;

            NetworkLobbyScene()  = default;
            ~NetworkLobbyScene() override = default;

            const char* Name() const noexcept override { return "NetworkLobby"; }

            void OnEnter (AppContext& ctx) override;
            void OnUpdate(AppContext& ctx, float dt) override;
            void OnRender(AppContext& ctx) override;
            void OnEvent (AppContext& ctx, NkEvent& ev) override;
            void OnExit  (AppContext& ctx) override;

        private:
            float mTime      = 0.0f;
            float mEnterAnim = 0.0f;

            // Grace period d'entree de scene : on ignore les events click/touch
            // pendant ce delai pour eviter qu'un release "fantome" du clic qui
            // a pousse cette scene (depuis SelectModeScene, qui declenche sur
            // Press) ne declenche automatiquement un bouton ici si la souris se
            // trouve dessus. 200ms = largement suffisant pour absorber le release
            // suivant.
            float mInputArmDelay = 0.0f;

            // Mode UI : 0 = ecran de choix, 1 = formulaire REJOINDRE (saisie IP)
            int  mView = 0;

            // IP cible (editable au clavier). Default localhost pour debug.
            char mIp[kMaxIpLen] = { '1','2','7','.','0','.','0','.','1','\0' };
            int  mIpLen = 9;

            // ── Boutons (coords ECRAN, sync chaque frame) ──────────────────
            float mBackX = 0.0f, mBackY = 0.0f, mBackW = 0.0f, mBackH = 0.0f;
            float mHostX = 0.0f, mHostY = 0.0f, mHostW = 0.0f, mHostH = 0.0f;
            float mJoinX = 0.0f, mJoinY = 0.0f, mJoinW = 0.0f, mJoinH = 0.0f;
            float mConnectX = 0.0f, mConnectY = 0.0f;
            float mConnectW = 0.0f, mConnectH = 0.0f;
            float mLaunchX = 0.0f, mLaunchY = 0.0f;
            float mLaunchW = 0.0f, mLaunchH = 0.0f;
            float mCancelX = 0.0f, mCancelY = 0.0f;
            float mCancelW = 0.0f, mCancelH = 0.0f;

            long long mActiveTouchId = -1;

            // Helpers
            bool HitTest(float sx, float sy, float bx, float by, float bw, float bh) const;
            void DoHost  (AppContext& ctx);
            void DoJoin  (AppContext& ctx);
            void DoLaunch(AppContext& ctx);
            void DoCancel(AppContext& ctx);
            /// Pousse GameplayScene avec le mode reseau actif.
            void PushGameplay(AppContext& ctx);
            /// Insere/supprime des caracteres dans mIp (clavier).
            void IpAppendDigit(char c);
            void IpBackspace();
        };

    } // namespace pong
} // namespace nkentseu
