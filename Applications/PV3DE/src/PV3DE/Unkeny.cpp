#include "Nkentseu/NkCore.h"
#include "Nkentseu/Nkentseu.h"



nkentseu::Application* nkentseu::CreateApplication(const NkApplicationConfig& config) {
    nkentseu::Log::Init(config.logLevel);
    nkentseu::Log::Info("Application Starting: {}", config.appName);

    nkentseu::UnkenyAppConfig unkenyConfig;
    unkenyConfig.appConfig = config;
    unkenyConfig.cmdArgs = args;

    unkenyConfig.Initialize();

    return new Unkeny::UnkenyApp(unkenyConfig);
}