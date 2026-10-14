#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"

// Deployment Game.ini [MA0T10.SensorTransport] applies over map defaults, first file with
// the section wins, empty keys keep current values, unknown mode is ignored.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorTransportRuntimeConfigTest,"MA0T10.SensorTransport.RuntimeConfigFile",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorTransportRuntimeConfigTest::RunTest(const FString&)
{
	const FString Dir=FPaths::ProjectIntermediateDir()/TEXT("TransportRuntimeConfigTest");
	IFileManager::Get().DeleteDirectory(*Dir,false,true);IFileManager::Get().MakeDirectory(*Dir,true);
	const FString Empty=Dir/TEXT("Empty.ini"),Game=Dir/TEXT("Game.ini"),Later=Dir/TEXT("Later.ini");
	FFileHelper::SaveStringToFile(TEXT("[DTCoreRuntimeOverride]\nWebSocketUrl=\n"),*Empty);
	FFileHelper::SaveStringToFile(TEXT("[MA0T10.SensorTransport]\nTransportMode=StompWebSocket\nBrokerUrl=tcp://10.0.0.5:61616\nExportTopic=pcd.topic\nUserName=deploy\nPasscode=secret-for-test\nCameraTopic=\nMaxMessageBytes=4096\n"),*Game);
	FFileHelper::SaveStringToFile(TEXT("[MA0T10.SensorTransport]\nBrokerUrl=tcp://never:1\n"),*Later);

	auto* T=NewObject<UVirtualSensorTransportComponent>();
	const FString CameraBefore=T->GetTransportProfile().CameraTopic;
	TestFalse(TEXT("no section anywhere applies nothing"),T->ApplyRuntimeConfigFrom({Empty},false));
	TestFalse(TEXT("nothing applied yet"),T->HasRuntimeConfigOverrides());
	TestTrue(TEXT("section applies"),T->ApplyRuntimeConfigFrom({Empty,Game,Later},false));
	const auto& P=T->GetTransportProfile();
	TestEqual(TEXT("broker from first file with the section"),P.BrokerUrl,FString(TEXT("tcp://10.0.0.5:61616")));
	TestEqual(TEXT("export topic"),P.ExportTopic,FString(TEXT("pcd.topic")));
	TestEqual(TEXT("user"),P.UserName,FString(TEXT("deploy")));
	TestEqual(TEXT("empty key keeps current value"),P.CameraTopic,CameraBefore);
	TestEqual(TEXT("numeric key"),P.MaxMessageBytes,4096);
	TestEqual(TEXT("mode by enum name"),T->TransportMode,EVirtualSensorTransportMode::StompWebSocket);
	TestEqual(TEXT("passcode reaches the session credentials"),T->GetSessionPasscodeForHighThroughput(),FString(TEXT("secret-for-test")));
	TestEqual(TEXT("source recorded"),T->GetRuntimeConfigSource(),Game);

	FFileHelper::SaveStringToFile(TEXT("[MA0T10.SensorTransport]\nTransportMode=NotAMode\n"),*Game);
	T->TransportMode=EVirtualSensorTransportMode::HttpPost;T->ApplyRuntimeConfigFrom({Game},false);
	TestEqual(TEXT("unknown mode is ignored"),T->TransportMode,EVirtualSensorTransportMode::HttpPost);
	IFileManager::Get().DeleteDirectory(*Dir,false,true);
	return true;
}
#endif
