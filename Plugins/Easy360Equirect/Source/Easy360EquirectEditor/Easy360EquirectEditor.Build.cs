using UnrealBuildTool;
public class Easy360EquirectEditor : ModuleRules
{
    public Easy360EquirectEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Core","CoreUObject","Engine","Slate","SlateCore",
            "UnrealEd","LevelEditor","Projects","ToolMenus","MainFrame","Easy360Equirect"
        });
    }
}
