using UnrealBuildTool;
using System.IO;

public class Easy360Equirect : ModuleRules
{
    public Easy360Equirect(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "RenderCore",
            "RHI",
            "Projects",
            "AudioMixer"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { "Renderer" });

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicDefinitions.Add("EASY360_PLATFORM_WIN64=1");
            PublicDefinitions.Add("EASY360_ENABLE_NVENC=1");

            string NvInc = Path.Combine(ModuleDirectory, "../../ThirdParty/NvCodec/include");
            if (Directory.Exists(NvInc))
            {
                PublicIncludePaths.Add(NvInc);
            }

            PrivateDependencyModuleNames.AddRange(new string[] { "D3D11RHI", "D3D12RHI" });
        }
        else
        {
            PublicDefinitions.Add("EASY360_PLATFORM_WIN64=0");
            PublicDefinitions.Add("EASY360_ENABLE_NVENC=0");
        }
    }
}
