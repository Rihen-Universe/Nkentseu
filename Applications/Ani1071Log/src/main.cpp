// Chapitre 9 du livre ANI-1071 : le journal de Nkentseu a la place de printf.
#include <NKLogger/NkLog.h>

int main()
{
    nkentseu::NkLog::Initialize("Ani1071");

    logger.Info("Le programme demarre");
    logger.Debug("Ceci ne s'affiche pas : le niveau par defaut est INFO");
    logger.Warn("Une valeur suspecte : {0}", 42);
    logger.Error("Impossible d'ouvrir le fichier {0} (code {1})", "niveau1.map", 2);

    double dt = 1.0 / 120.0;
    logger.Info("pas de temps : {0} s, soit {1} pas par seconde", dt, 120);

    nkentseu::NkLog::Shutdown();
    return 0;
}
