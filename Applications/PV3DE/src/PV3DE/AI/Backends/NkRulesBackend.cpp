#include "NkRulesBackend.h"
#include <cstring>
namespace nkentseu { namespace humanoid {
    bool NkRulesBackend::Complete(const NkVector<NkConvMessage>& msgs,
                                   nk_float32, nk_uint32,
                                   NkConvResponse& out) noexcept {
        if (msgs.IsEmpty()) { out.text = "..."; out.success = true; return true; }
        const char* q = msgs[msgs.Size()-1].content.CStr();
        if (strstr(q,"douleur")||strstr(q,"mal"))
            out.text = "J'ai mal... dans la poitrine, surtout.";
        else if (strstr(q,"respir")||strstr(q,"souffle"))
            out.text = "Ça me gêne pour respirer, oui.";
        else if (strstr(q,"depuis")||strstr(q,"quand"))
            out.text = "Depuis ce matin, vers huit heures.";
        else if (strstr(q,"antécédent")||strstr(q,"maladie"))
            out.text = "J'ai de l'hypertension. Je prends des médicaments.";
        else if (strstr(q,"comment")||strstr(q,"allez"))
            out.text = "Pas très bien... j'ai très mal.";
        else out.text = "Je... je ne sais pas trop.";
        out.success = true;
        return true;
    }
}} // namespace
