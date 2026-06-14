#include "Noge/NkCore.h"
#include "Noge/Nkentseu.h"



nkentseu::Application* nkentseu::CreateApplication(const NkApplicationConfig& config) {
    nkentseu::Log::Init(config.logLevel);
    nkentseu::Log::Info("Application Starting: {}", config.appName);

    nkentseu::NogeAppConfig NogeConfig;
    NogeConfig.appConfig = config;
    NogeConfig.cmdArgs = args;

    NogeConfig.Initialize();

    return new Noge::NogeApp(NogeConfig);
}