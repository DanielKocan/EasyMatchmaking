// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
// This is a module setup file for Unreal Engine. Think of it like a recipe that tells Unreal Engine how to build and connect your code module.
public class EasyMatchmaking : ModuleRules
{
	public EasyMatchmaking(ReadOnlyTargetRules Target) : base(Target)
	{
        // This is about speeding up compilation (making your code build faster). It's like choosing a faster way to process your code.
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs; 
        // These are folders where you put code files that OTHER modules can see and use.
        PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);

        //  These are folders for code files that only YOUR module can see. 
        PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);

        //These are other code modules that your module needs AND that other modules using yours will also need. 
        PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "CoreUObject",
				"Engine",
                "EOSShared",
                "DeveloperSettings",
                "UMG",           // for UUserWidget
				"Slate",         // for Slate UI
				"SlateCore",      // for core Slate functionality
                "EOSShared" 
				// ... add other public dependencies that you statically link with here ...
			}
			);

        //  These are modules only YOUR code needs, but others don't need to know about them
        PrivateDependencyModuleNames.AddRange(
			new string[]
			{
                "InputCore",     // For input handling

				"Slate",
				"SlateCore",
                "EOSSDK",       
				"Projects"          
				// ... add private dependencies that you statically link with here ...	
			}
			);

        // Only add editor modules when building for editor
        if (Target.Type == TargetType.Editor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "Settings",
                "ToolMenus",
                "UnrealEd",
                "EditorStyle",      // Only in editor builds
                "EditorWidgets"
            });
        }

        //  These are modules that get loaded only when needed, not right away.
        DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
