
#include "Core/Non.h"
#include "Modules/ModuleManager.h"


class FNonModule : public FDefaultGameModuleImpl
{
public:
    virtual void StartupModule() override
    {
    }  
    virtual void ShutdownModule() override {}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FNonModule, Non, "Non");
