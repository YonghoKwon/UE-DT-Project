#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "DTCore.h"

// Deployment-side transport settings (PKG-01). Shipping keeps SaveGames under the user
// profile while Development keeps them inside the package, so server settings saved from
// the UI did not carry over to Shipping. Both now read the same file next to the package.
//
// Config/SensorTransport.ini is our own file: DTCore rewrites Config/Game.ini through
// Unreal's ini writer (which cuts values at "//"), and a project Config/Game.ini staged
// into the pak shadows the loose file. Both ini names are excluded from staging in
// DefaultGame.ini [Staging]. Game.ini is still read for compatibility.
const TCHAR* UVirtualSensorTransportComponent::RuntimeConfigSection=TEXT("MA0T10.SensorTransport");

TArray<FString> UVirtualSensorTransportComponent::GetRuntimeConfigCandidateFiles()
{
	// Same directory order as DTCore's runtime Game.ini (DTCoreRuntimeConfig.cpp).
	TArray<FString> Dirs,Paths;
	auto AddDir=[&Dirs](const FString& Dir){FString Full=FPaths::ConvertRelativePathToFull(Dir);FPaths::NormalizeDirectoryName(Full);Dirs.AddUnique(Full);};
	AddDir(FPaths::ProjectDir()/TEXT("Config"));
	AddDir(FPaths::LaunchDir()/TEXT("Config"));
	AddDir(FPaths::LaunchDir()/TEXT("../../Config"));
	AddDir(FPaths::LaunchDir()/FApp::GetProjectName()/TEXT("Config"));
	for(const TCHAR* Name:{TEXT("SensorTransport.ini"),TEXT("Game.ini")})for(const FString& Dir:Dirs)Paths.AddUnique(Dir/Name);
	return Paths;
}

void UVirtualSensorTransportComponent::BeginPlay()
{
	Super::BeginPlay();
#if !WITH_EDITOR
	// Packaged builds: leave an empty, self-describing template next to the package the
	// first time, so operators know which file to edit. Empty values change nothing.
	const FString Template=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("Config")/TEXT("SensorTransport.ini"));
	if(!IFileManager::Get().FileExists(*Template))
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Template),true);
		FFileHelper::SaveStringToFile(FString::Printf(TEXT(
			"; PCD/sensor transport settings for this package. Applied at start; empty values keep the map/UI values.\r\n"
			"; TransportMode: StompWebSocket | HttpPost | LogOnly | SaveToFile | None. Quotes are optional.\r\n"
			"[%s]\r\nTransportMode=\r\nBrokerUrl=\r\nExportTopic=\r\nLidarTopic=\r\nCameraTopic=\r\nAckTopic=\r\nUserName=\r\nPasscode=\r\nHttpEndpoint=\r\n"),RuntimeConfigSection),
			*Template,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
#endif
	ApplyRuntimeConfigFile();
}

bool UVirtualSensorTransportComponent::ApplyRuntimeConfigFile()
{
	return ApplyRuntimeConfigFrom(GetRuntimeConfigCandidateFiles(),true);
}

bool UVirtualSensorTransportComponent::ApplyRuntimeConfigFrom(const TArray<FString>& CandidateFiles,bool bUseGameIniFallback)
{
	static const TCHAR* Keys[]={TEXT("TransportMode"),TEXT("BrokerUrl"),TEXT("CameraTopic"),TEXT("LidarTopic"),TEXT("ExportTopic"),TEXT("AckTopic"),
		TEXT("UserName"),TEXT("Passcode"),TEXT("HttpEndpoint"),TEXT("MaxMessageBytes"),TEXT("TimeoutSeconds")};
	TMap<FString,FString> Values;FString Source;
	// Parsed directly: Unreal's ini reader cuts values at "//", which broke unquoted
	// URLs such as tcp://host:port. Comments are whole lines starting with ; or #.
	// The first file whose section has values wins as a whole; keys are not mixed across files.
	auto Unquote=[](FString V){V.TrimStartAndEndInline();if(V.Len()>=2&&V.StartsWith(TEXT("\""))&&V.EndsWith(TEXT("\"")))V=V.Mid(1,V.Len()-2);return V.TrimStartAndEnd();};
	for(const FString& Path:CandidateFiles)
	{
		FString Text;if(!FPaths::FileExists(Path)||!FFileHelper::LoadFileToString(Text,*Path))continue;
		TArray<FString> Lines;Text.ParseIntoArrayLines(Lines,false);
		bool bInSection=false;
		for(FString Line:Lines)
		{
			Line.TrimStartAndEndInline();
			if(Line.IsEmpty()||Line.StartsWith(TEXT(";"))||Line.StartsWith(TEXT("#")))continue;
			if(Line.StartsWith(TEXT("["))){bInSection=Line.Equals(FString::Printf(TEXT("[%s]"),RuntimeConfigSection),ESearchCase::IgnoreCase);continue;}
			if(!bInSection)continue;
			FString Key,Value;if(!Line.Split(TEXT("="),&Key,&Value))continue;
			Key.TrimStartAndEndInline();Value=Unquote(Value);
			for(const TCHAR* Known:Keys)if(Key.Equals(Known,ESearchCase::IgnoreCase)&&!Value.IsEmpty())Values.Add(Known,Value);
		}
		if(!Values.IsEmpty()){Source=Path;break;}
	}
	if(Source.IsEmpty()&&bUseGameIniFallback&&GConfig)
	{
		// DefaultGame.ini fallback goes through Unreal's reader: quote URLs there.
		for(const TCHAR* Key:Keys){FString V;if(GConfig->GetString(RuntimeConfigSection,Key,V,GGameIni)&&!Unquote(V).IsEmpty())Values.Add(Key,Unquote(V));}
		if(!Values.IsEmpty())Source=TEXT("GGameIni");
	}
	if(Values.IsEmpty())
	{
		DX_LOG(GetWorld(),TEXT("[SensorTransport] no deployment values in [%s]; first candidate %s"),RuntimeConfigSection,CandidateFiles.Num()?*CandidateFiles[0]:TEXT("-"));
		return false;
	}

	FVirtualSensorTransportProfile Profile=TransportProfile;
	auto Str=[&Values](const TCHAR* Key,FString& Target){if(const FString* V=Values.Find(Key))Target=*V;};
	Str(TEXT("BrokerUrl"),Profile.BrokerUrl);Str(TEXT("CameraTopic"),Profile.CameraTopic);Str(TEXT("LidarTopic"),Profile.LidarTopic);
	Str(TEXT("ExportTopic"),Profile.ExportTopic);Str(TEXT("AckTopic"),Profile.AckTopic);Str(TEXT("UserName"),Profile.UserName);Str(TEXT("HttpEndpoint"),Profile.HttpEndpoint);
	if(const FString* V=Values.Find(TEXT("MaxMessageBytes"))){int32 N=0;if(LexTryParseString(N,**V))Profile.MaxMessageBytes=N;}
	if(const FString* V=Values.Find(TEXT("TimeoutSeconds"))){int32 N=0;if(LexTryParseString(N,**V))Profile.TimeoutSeconds=N;}
	if(const FString* V=Values.Find(TEXT("TransportMode")))
	{
		// Accept the enum name (StompWebSocket, HttpPost, LogOnly, SaveToFile, None).
		if(const UEnum* Enum=StaticEnum<EVirtualSensorTransportMode>())
		{
			const int64 Parsed=Enum->GetValueByNameString(*V,EGetByNameFlags::None);
			if(Parsed!=INDEX_NONE)TransportMode=static_cast<EVirtualSensorTransportMode>(Parsed);
			else DX_LOG(GetWorld(),TEXT("[SensorTransport] Unknown TransportMode '%s' in %s"),**V,*Source);
		}
	}
	ConfigureTransportProfile(Profile);
	if(const FString* V=Values.Find(TEXT("Passcode")))SetSessionCredentials(*V,SessionBearerToken);
	bRuntimeConfigApplied=true;RuntimeConfigSource=Source;
	// DX_LOG also writes <launch dir>/Logs/CustomLogs in Shipping. Never logs the passcode.
	DX_LOG(GetWorld(),TEXT("[SensorTransport] applied %d deployment keys from %s (broker=%s, mode=%d, passcode=%s)"),Values.Num(),*Source,*TransportProfile.BrokerUrl,int32(TransportMode),Values.Contains(TEXT("Passcode"))?TEXT("set"):TEXT("unchanged"));
	return true;
}
