// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class m7at10_dtTarget : TargetRules
{
	public m7at10_dtTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;
		ExtraModuleNames.Add("m7at10_dt");

		// // [추가] 프로젝트 고유의 빌드 환경을 사용하도록 설정 (로그 설정을 덮어쓰기 위해 필요)
		// BuildEnvironment = TargetBuildEnvironment.Unique;
		//
		// // Shipping 빌드에서도 로그 활성화
		// if (Target.Configuration == UnrealTargetConfiguration.Shipping)
		// {
		// 	bUseLoggingInShipping = true;
		// }
	}
}
