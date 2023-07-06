// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RopeGrapple : ModuleRules
{
	public RopeGrapple(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay", "EnhancedInput" });
	}
}
