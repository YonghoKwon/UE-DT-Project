#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualPcdCodec.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualPointCloudStreamReceiverTC.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiPreferences.h"
#include "Serialization/ObjectWriter.h"
#include "Serialization/ObjectReader.h"
#include "Misc/SecureHash.h"
#include "Kismet/GameplayStatics.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarExportComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarAnalysisComponent.h"

struct FPcdBandwidthTestAccess
{
	static auto& Runtime(UVirtualSensorStreamPublisherComponent* P,const FString& Id) {return P->FindOrAddRuntime(EVirtualSensorStreamKind::PointCloud,Id);}
	static void Queue(UVirtualSensorStreamPublisherComponent* P,const FVirtualSensorFrameEnvelope& F)
	{auto& R=Runtime(P,TEXT(""));R.Config.bEnabled=true;R.Config.MaxBufferedFrames=120;R.bSerializationInFlight=true;P->QueueFrameForRuntime(TEXT("test"),R,F);}
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPcdBandwidthCodecTest,"MA0T10.PcdBandwidth.Codec",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPcdBandwidthCodecTest::RunTest(const FString&)
{
	TArray<FVirtualLidarPoint> Source;Source.SetNum(3);
	for(int32 I=0;I<3;++I){auto& P=Source[I];P.bHit=true;P.SensorLocalPositionMeters=FVector(1.25+I,-2.5,3.75);P.RawIntensity=1234;P.Ring=63;P.HorizontalIndex=900;P.EchoIndex=1;P.EchoCount=2;P.PointTimeOffsetNanoseconds=1234567890123;P.Confidence=.75;}
	TArray<const FVirtualLidarPoint*> Points;for(const auto& P:Source)Points.Add(&P);
	for(auto Profile:{EVirtualPcdFieldProfile::Full,EVirtualPcdFieldProfile::Compact,EVirtualPcdFieldProfile::Minimal,EVirtualPcdFieldProfile::Custom})
	{
		FVirtualPcdSendOptions O;O.Profile=Profile;O.CustomFieldMask=(1<<3)|(1<<10);
		TArray<uint8> Binary,Compressed;FVirtualPcdDocument A,B;FString Error;
		TestTrue(TEXT("encode binary profile"),FVirtualPcdCodec::Encode(Points,TEXT(""),O,Binary));
		TestTrue(TEXT("parse binary profile"),FVirtualPcdCodec::Decode(Binary,A,Error));
		TestEqual(TEXT("header-derived record size"),A.PointBytes,FVirtualPcdCodec::BytesPerPoint(O));
		TestEqual(TEXT("exact binary bytes"),Binary.Num()-A.PayloadOffset,3*A.PointBytes);
		TestEqual(TEXT("all selected fields have COUNT1"),A.Names.Num(),int32(FMath::CountBits(uint64(O.FieldMask()))));
		TestTrue(TEXT("XYZ cannot be unchecked"),(O.FieldMask()&7)==7);
		O.bCompressed=true;TestTrue(TEXT("encode compressed profile"),FVirtualPcdCodec::Encode(Points,TEXT(""),O,Compressed));
		TestTrue(TEXT("parse compressed profile"),FVirtualPcdCodec::Decode(Compressed,B,Error));
		TestTrue(TEXT("LZF/SoA roundtrip is bit exact for every selected field"),A.Records==B.Records);
		TArray<FVirtualLidarPoint> Read;TestTrue(TEXT("decode defaults for omitted fields"),B.ReadPoints(Read));
		if(Read.Num()==3){TestEqual(TEXT("XYZ retained"),Read[0].SensorLocalPositionMeters,Source[0].SensorLocalPositionMeters);if(Profile==EVirtualPcdFieldProfile::Minimal){TestEqual(TEXT("missing intensity defaults0"),Read[0].RawIntensity,0);TestEqual(TEXT("missing confidence defaults1"),Read[0].Confidence,1.f);TestFalse(TEXT("missing ring/index do not claim grid metadata"),Read[0].bHasGridCoord);}}
		Compressed.Pop();TestFalse(TEXT("truncated compressed block rejected"),FVirtualPcdCodec::Decode(Compressed,B,Error));
	}
	FVirtualPcdSendOptions O;TestEqual(TEXT("Full default record33"),FVirtualPcdCodec::BytesPerPoint(O),33);TestFalse(TEXT("compression default remains gated off"),O.bCompressed);
	O.Profile=EVirtualPcdFieldProfile::Compact;TestEqual(TEXT("Compact record24"),FVirtualPcdCodec::BytesPerPoint(O),24);
	O.Profile=EVirtualPcdFieldProfile::Minimal;TestEqual(TEXT("Minimal record12"),FVirtualPcdCodec::BytesPerPoint(O),12);
	// Independent fixed PCL-layout vector: XYZ planes for two points followed by
	// a legal one-byte LZF literal command. 24 zero bytes are literal, no compressor involved.
	const ANSICHAR Header[]="# .PCD v0.7\nVERSION 0.7\nFIELDS x y z\nSIZE 4 4 4\nTYPE F F F\nCOUNT 1 1 1\nWIDTH 2\nHEIGHT 1\nPOINTS 2\nDATA binary_compressed\n";
	TArray<uint8> Golden;Golden.Append(reinterpret_cast<const uint8*>(Header),sizeof(Header)-1);
	const uint8 SizesAndControl[]={25,0,0,0,24,0,0,0,23};Golden.Append(SizesAndControl,9);
	// x=(1,2), y=(3,4), z=(5,6), explicit little endian floats in field-major order.
	const uint8 Soa[]={0,0,128,63,0,0,0,64,0,0,64,64,0,0,128,64,0,0,160,64,0,0,192,64};Golden.Append(Soa,24);
	FVirtualPcdDocument G;FString Error;TestTrue(TEXT("fixed PCL binary_compressed size prefix+LZF+SoA vector"),FVirtualPcdCodec::Decode(Golden,G,Error));
	TArray<FVirtualLidarPoint> Decoded;TestTrue(TEXT("fixed vector points"),G.ReadPoints(Decoded));if(Decoded.Num()==2){TestEqual(TEXT("SoA first XYZ"),Decoded[0].SensorLocalPositionMeters,FVector(1,3,5));TestEqual(TEXT("SoA second XYZ"),Decoded[1].SensorLocalPositionMeters,FVector(2,4,6));}
	// Encoder oracle: distinct finite float bits force a single 24-byte LZF literal.
	// Expected bytes are handwritten field-major planes, independent of Decode.
	auto FloatFromBits=[](uint32 Bits){float F;FMemory::Memcpy(&F,&Bits,4);return F;};
	FVirtualLidarPoint P0,P1;P0.SensorLocalPositionMeters=FVector(FloatFromBits(0x3f010203),FloatFromBits(0x40040506),FloatFromBits(0x41070809));P1.SensorLocalPositionMeters=FVector(FloatFromBits(0x420a0b0c),FloatFromBits(0x430d0e0f),FloatFromBits(0x44101112));
	O.Profile=EVirtualPcdFieldProfile::Minimal;O.bCompressed=true;TArray<uint8> Known;
	TestTrue(TEXT("fixed two-point encoder input"),FVirtualPcdCodec::Encode({&P0,&P1},TEXT(""),O,Known));
	FVirtualPcdDocument KnownHeader;TestTrue(TEXT("locate golden encoder body"),FVirtualPcdCodec::Decode(Known,KnownHeader,Error));
	const uint8 ExpectedBody[]={25,0,0,0,24,0,0,0,23,3,2,1,63,12,11,10,66,6,5,4,64,15,14,13,67,9,8,7,65,18,17,16,68};
	TestTrue(TEXT("PCD encoder emits exact fixed PCL size-prefix/LZF/SoA bytes"),Known.Num()-KnownHeader.PayloadOffset==sizeof(ExpectedBody)&&FMemory::Memcmp(Known.GetData()+KnownHeader.PayloadOffset,ExpectedBody,sizeof(ExpectedBody))==0);
	// Golden compressor stream without a match: literal command+three bytes.
	TArray<uint8> Raw={11,22,33},Packed;FVirtualPcdCodec::CompressLzf(Raw,Packed);TestTrue(TEXT("fixed LZF encoder literal bytes"),Packed==TArray<uint8>({2,11,22,33}));
	const uint8 Backref[]={0,65,224,0,0};TArray<uint8> Expanded;TestTrue(TEXT("fixed extended overlapping back-reference"),FVirtualPcdCodec::DecompressLzf(Backref,5,10,Expanded));TestTrue(TEXT("ten repeated As"),Expanded==TArray<uint8>({65,65,65,65,65,65,65,65,65,65}));
	TArray<const FVirtualLidarPoint*> Empty;TArray<uint8> Bytes;O.bCompressed=true;TestTrue(TEXT("zero point compression"),FVirtualPcdCodec::Encode(Empty,TEXT(""),O,Bytes));TestTrue(TEXT("zero point decode"),FVirtualPcdCodec::Decode(Bytes,G,Error));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPcdBandwidthReceiverTest,"MA0T10.PcdBandwidth.Receiver",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPcdBandwidthReceiverTest::RunTest(const FString&)
{
	FVirtualLidarPoint P;P.bHit=true;TArray<const FVirtualLidarPoint*> Points={&P};
	for(auto Profile:{EVirtualPcdFieldProfile::Full,EVirtualPcdFieldProfile::Compact,EVirtualPcdFieldProfile::Minimal,EVirtualPcdFieldProfile::Custom})for(bool Compressed:{false,true})
	{
		FVirtualPcdSendOptions O;O.Profile=Profile;O.bCompressed=Compressed;O.CustomFieldMask=0;
		TArray<uint8> Bytes;FVirtualPcdCodec::Encode(Points,TEXT(""),O,Bytes);uint8 Hash[FSHA1::DigestSize];FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Hash);
		TMap<FName,FString> H={{TEXT("schema"),TEXT("virtual-pointcloud.pcd.v1")},{TEXT("x-sensor-id"),TEXT("A")},{TEXT("x-frame-id"),TEXT("1")},{TEXT("x-point-count"),TEXT("1")},{TEXT("x-source-point-count"),TEXT("1")},{TEXT("x-filter-revision"),TEXT("0")},{TEXT("x-checksum-sha1"),BytesToHex(Hash,FSHA1::DigestSize).ToLower()},{TEXT("x-acquisition-profile"),TEXT("test")},{TEXT("x-utc"),TEXT("2026-10-23T00:00:00.000Z")},{TEXT("content-type"),TEXT("application/vnd.pcd")}};
		const auto D=StaticCastSharedPtr<FVirtualPointCloudStreamReceiverData>(UVirtualPointCloudStreamReceiverTC::ParseBinaryPcdToStruct(Bytes,H));TestTrue(TEXT("TC accepts each header-selected profile/encoding"),D.IsValid()&&D->bValid);
		H[TEXT("x-point-count")]=TEXT("2");const auto Bad=StaticCastSharedPtr<FVirtualPointCloudStreamReceiverData>(UVirtualPointCloudStreamReceiverTC::ParseBinaryPcdToStruct(Bytes,H));TestFalse(TEXT("TC does not weaken header count integrity"),Bad->bValid);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPcdBandwidthDivisorTest,"MA0T10.PcdBandwidth.DivisorAndIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPcdBandwidthDivisorTest::RunTest(const FString&)
{
	for(int32 Divisor:{1,2,4})
	{
		auto* P=NewObject<UVirtualSensorStreamPublisherComponent>();P->bAutoAdjustPointCloudSendRate=false;
		FVirtualPcdSendOptions O;O.SendDivisor=Divisor;FString Error;TestTrue(TEXT("configure all before stream"),P->SetPcdSendOptions(TEXT(""),O,true,Error));
		for(int32 I=1;I<=20;++I)for(const FString Id:{TEXT("A"),TEXT("B"),TEXT("C")})
		{
			FVirtualSensorFrameEnvelope F;F.SensorKind=EVirtualSensorKind::Lidar;F.SensorId=Id;F.FrameId=I;
			F.PointSnapshot=MakeShared<const TArray<FVirtualLidarPoint>,ESPMode::ThreadSafe>();FPcdBandwidthTestAccess::Queue(P,F);
		}
		const auto& R=FPcdBandwidthTestAccess::Runtime(P,TEXT(""));TestEqual(TEXT("20Hz input unchanged for all3 wildcard producers"),R.Status.InputFrameCount,int64(60));
		TestEqual(TEXT("configured send Hz per sensor independent of wildcard interleave"),R.Status.ConfiguredDivisorSkippedCount,int64(60-60/Divisor));
		TestFalse(TEXT("option mutation rejects retained FIFO work"),P->SetPcdSendOptions(TEXT("A"),O,false,Error));
	}
	auto* Saved=NewObject<UVirtualSensorUiPreferencesSaveGame>();FVirtualPcdSendOptions O;O.Profile=EVirtualPcdFieldProfile::Custom;O.CustomFieldMask=511;O.SendDivisor=4;O.bCompressed=true;Saved->PcdSendOptionsBySensor.Add(TEXT("A"),O);Saved->SensorStreamFrameStride=7;
	TArray<uint8> Bytes;TestTrue(TEXT("SaveGame memory writer succeeds"),UGameplayStatics::SaveGameToMemory(Saved,Bytes));auto* Restored=Cast<UVirtualSensorUiPreferencesSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if(!TestNotNull(TEXT("SaveGame memory reader succeeds"),Restored))return false;
	TestTrue(TEXT("new options survive SaveGame serialization"),Restored->PcdSendOptionsBySensor.FindChecked(TEXT("A"))==O);TestEqual(TEXT("existing saved preferences preserved"),Restored->SensorStreamFrameStride,7);
	FVirtualPcdSendOptions Full;const double B=FVirtualPcdCodec::EstimateBytesPerSecond(Full,65536,20);
	TestEqual(TEXT("Full per-device payload estimate MiB/s"),B/1048576.,41.25);TestEqual(TEXT("three devices 80MiB/s budget percent"),B*3/1048576./80*100,154.6875);TestEqual(TEXT("decimal Mbps"),B*8/1000000.,346.03008);
	O.Profile=EVirtualPcdFieldProfile::Minimal;TestEqual(TEXT("Minimal divisor4 estimate"),FVirtualPcdCodec::EstimateBytesPerSecond(O,65536,20)/1048576.,3.75);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPcdBandwidthIsolationTest,"MA0T10.PcdBandwidth.PreviewStorageAnalysisIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPcdBandwidthIsolationTest::RunTest(const FString&)
{
	auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* L=World->SpawnActor<AVirtualLidarSensorActor>();
	TArray<uint8> ScanBefore,ViewBefore,AnalysisBefore;
	FObjectWriter S(L->ScanComponent,ScanBefore);FObjectWriter V(L->VisualizationComponent,ViewBefore);FObjectWriter A(L->AnalysisComponent,AnalysisBefore);
	FVirtualLidarPoint Point;Point.bHit=true;Point.SensorLocalPositionMeters=FVector(1,2,3);
	FVirtualSensorFrameEnvelope Frame;Frame.SensorId=TEXT("A");Frame.FrameId=10;Frame.TimestampUtc=FDateTime(2026,10,23);Frame.PointSnapshot=MakeShared<const TArray<FVirtualLidarPoint>,ESPMode::ThreadSafe>(TArray<FVirtualLidarPoint>({Point}));
	FVirtualSensorStreamConfig C;C.PointCloudFormat=EVirtualPointCloudStreamFormat::PCD;FString Ext,Error;int32 Count=0;TArray<uint8> Before,After;
	TestTrue(TEXT("Full file encode before send options"),UVirtualLidarExportComponent::SerializeImmutableFrame(Frame,C,Ext,Before,Count,Error));
	auto* P=NewObject<UVirtualSensorStreamPublisherComponent>();FVirtualPcdSendOptions O;O.Profile=EVirtualPcdFieldProfile::Minimal;O.bCompressed=true;O.SendDivisor=4;
	TestTrue(TEXT("set isolated sender options"),P->SetPcdSendOptions(TEXT("A"),O,false,Error));C.PcdSendOptions=O;
	TestTrue(TEXT("file encoder ignores transmission options"),UVirtualLidarExportComponent::SerializeImmutableFrame(Frame,C,Ext,After,Count,Error));TestTrue(TEXT("FileSave/Recorder Full binary bytes unchanged"),Before==After);
	TArray<uint8> ScanAfter,ViewAfter,AnalysisAfter;FObjectWriter S2(L->ScanComponent,ScanAfter);FObjectWriter V2(L->VisualizationComponent,ViewAfter);FObjectWriter A2(L->AnalysisComponent,AnalysisAfter);
	TestTrue(TEXT("scan component state unchanged"),ScanBefore==ScanAfter);TestTrue(TEXT("preview component state unchanged"),ViewBefore==ViewAfter);TestTrue(TEXT("Slab analysis state unchanged"),AnalysisBefore==AnalysisAfter);
	L->Destroy();return true;
}
#endif
