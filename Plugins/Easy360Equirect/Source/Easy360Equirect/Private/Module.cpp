#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

class FEasy360EquirectModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Easy360Equirect"));
        if (Plugin.IsValid())
        {
            const FString ShaderDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Source/Easy360Equirect/Private/Shaders"));
            AddShaderSourceDirectoryMapping(TEXT("/Easy360Equirect"), ShaderDir);
        }
    }
    virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FEasy360EquirectModule, Easy360Equirect)
