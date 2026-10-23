#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualPcdCodec.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualPointCloudStreamReceiverTC.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiPreferences.h"
#include "Serialization/ObjectWriter.h"
#include "ma0t10_dt/MA0T10/UI/VirtualPcdBandwidthPresentation.h"
#include "Serialization/ObjectReader.h"
#include "Misc/SecureHash.h"
#include "Kismet/GameplayStatics.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarExportComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarAnalysisComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorOutputPreflight.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/SlabRunResultsSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"

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
		TestEqual(TEXT("binary accounting excludes PCD header"),FVirtualPcdCodec::EncodedPointBytes(Binary,false),int64(3*A.PointBytes));
		TestEqual(TEXT("all selected fields have COUNT1"),A.Names.Num(),int32(FMath::CountBits(uint64(O.FieldMask()))));
		TestTrue(TEXT("XYZ cannot be unchecked"),(O.FieldMask()&7)==7);
		O.bCompressed=true;TestTrue(TEXT("encode compressed profile"),FVirtualPcdCodec::Encode(Points,TEXT(""),O,Compressed));
		TestTrue(TEXT("parse compressed profile"),FVirtualPcdCodec::Decode(Compressed,B,Error));
		TestTrue(TEXT("LZF/SoA roundtrip is bit exact for every selected field"),A.Records==B.Records);
		TestEqual(TEXT("LZF accounting excludes header and size prefix"),FVirtualPcdCodec::EncodedPointBytes(Compressed,true),int64(Compressed.Num()-B.PayloadOffset-8));
		TArray<FVirtualLidarPoint> Read;TestTrue(TEXT("decode defaults for omitted fields"),B.ReadPoints(Read));
		if(Read.Num()==3){TestEqual(TEXT("XYZ retained"),Read[0].SensorLocalPositionMeters,Source[0].SensorLocalPositionMeters);if(Profile==EVirtualPcdFieldProfile::Minimal){TestEqual(TEXT("missing intensity defaults0"),Read[0].RawIntensity,0);TestEqual(TEXT("missing confidence defaults1"),Read[0].Confidence,1.f);TestFalse(TEXT("missing ring/index do not claim grid metadata"),Read[0].bHasGridCoord);}}
		Compressed.Pop();TestFalse(TEXT("truncated compressed block rejected"),FVirtualPcdCodec::Decode(Compressed,B,Error));
	}
	FVirtualPcdSendOptions O;TestEqual(TEXT("Full default record33"),FVirtualPcdCodec::BytesPerPoint(O),33);TestFalse(TEXT("standalone/file codec keeps binary default"),O.bCompressed);
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
	auto* Defaults=NewObject<UVirtualSensorStreamPublisherComponent>();
	TestEqual(TEXT("new stream follows measured compression policy"),Defaults->GetPcdSendOptions(TEXT("new")).bCompressed,Defaults->bDefaultPcdCompression);
	TestEqual(TEXT("live default retains Full fields"),Defaults->GetPcdSendOptions(TEXT("new")).Profile,EVirtualPcdFieldProfile::Full);
	Defaults->bDefaultPcdCompression=true;
	FVirtualPcdSendOptions Explicit;FString DefaultError;
	TestTrue(TEXT("explicit user binary choice accepted"),Defaults->SetPcdSendOptions(TEXT("A"),Explicit,false,DefaultError));
	TestFalse(TEXT("existing explicit binary choice wins over new default"),Defaults->GetPcdSendOptions(TEXT("A")).bCompressed);
	TestTrue(TEXT("other sensor retains measured default"),Defaults->GetPcdSendOptions(TEXT("B")).bCompressed);
	for(int32 Divisor:{1,2,4})
	{
		auto* P=NewObject<UVirtualSensorStreamPublisherComponent>();P->bAutoAdjustPointCloudSendRate=false;
		FVirtualPcdSendOptions O;O.SendDivisor=Divisor;FString Error;TestTrue(TEXT("configure all before stream"),P->SetPcdSendOptions(TEXT(""),O,true,Error));
		for(int32 I=1;I<=20;++I)for(const FString Id:{TEXT("A"),TEXT("B"),TEXT("C")})
		{
			FVirtualSensorFrameEnvelope F;F.SensorKind=EVirtualSensorKind::Lidar;F.SensorId=Id;F.FrameId=I;F.SlabContext.RunId=TEXT("configured-divisor-run");
			F.PointSnapshot=MakeShared<const TArray<FVirtualLidarPoint>,ESPMode::ThreadSafe>();FPcdBandwidthTestAccess::Queue(P,F);
		}
		const auto& R=FPcdBandwidthTestAccess::Runtime(P,TEXT(""));TestEqual(TEXT("20Hz input unchanged for all3 wildcard producers"),R.Status.InputFrameCount,int64(60));
		TestEqual(TEXT("configured send Hz per sensor independent of wildcard interleave"),R.Status.ConfiguredDivisorSkippedCount,int64(60-60/Divisor));
		TestEqual(TEXT("intentional divisor skips never record Slab delivery failure"),P->GetFailedSlabRunCount(TEXT("configured-divisor-run")),int64(0));
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPcdBandwidthLiveDisplayTest,"MA0T10.PcdBandwidth.LiveDisplay",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPcdBandwidthLiveDisplayTest::RunTest(const FString&)
{
 FVirtualPcdBandwidthWindow Window;
 Window.Record(9.,99,200,50); // exact left boundary is excluded
 Window.Record(9.25,1048576,2000000,1000000);
 Window.Record(9.75,2097152,1000000,250000);
 Window.Record(10.25,99,200,50); // future events do not count
 auto Sample=Window.Read(10.);
 TestEqual(TEXT("recent second is not a lifetime or last-frame average"),Sample.Frames,2);
 TestEqual(TEXT("sum actual bytes including each header"),Sample.BodyBytes,int64(3*1048576));
 TestEqual(TEXT("weighted payload reduction across changing frame options"),Sample.ReductionPercent(),(1.-1250000./3000000.)*100.);
 TestEqual(TEXT("stalled sender expires to zero Hz"),Window.Read(12.).Frames,0);
 TestEqual(TEXT("stalled sender expires to zero bytes"),Window.Read(12.).BodyBytes,int64(0));
 FVirtualPcdBandwidthPresentation View;View.PointBytes=24;View.SensorCount=3;
 View.SelectedMaximum=30*1048576.;View.TotalMaximum=90*1048576.;
 auto Text=View.Text().ToString();TestTrue(TEXT("stopped maximum estimate is explicit"),Text.Contains(TEXT("최대 추정"))&&Text.Contains(TEXT("112.5%")));
 View.ActiveCount=3;View.Selected.Frames=20;View.Selected.BodyBytes=10*1048576;
 for(int I=0;I<3;++I){auto Device=View.Selected;Device.OriginalPointBytes=20*1048576;Device.EncodedPointBytes=10*1048576;View.Total.Add(Device);}
 Text=View.Text().ToString();
 TestTrue(TEXT("active UI prioritizes real recent bytes/Hz and weighted compression"),Text.Contains(TEXT("최근 1초 실측"))&&Text.Contains(TEXT("20.0 Hz"))&&Text.Contains(TEXT("30.00 MiB/s (251.7 Mbps)"))&&Text.Contains(TEXT("37.5%"))&&Text.Contains(TEXT("50.0%")));
 TestFalse(TEXT("active actual budget never uses uncompressed spec112.5%"),Text.Contains(TEXT("112.5%"))||Text.Contains(TEXT("최대 추정")));
 View.Selected={};View.Total={};Text=View.Text().ToString();
 TestTrue(TEXT("active idle interval shows zero actual without estimate fallback"),Text.Contains(TEXT("0.0 Hz"))&&Text.Contains(TEXT("MiB/s · 0.0%")));
 View.UnmeasuredCount=1;Text=View.Text().ToString();
 TestFalse(TEXT("unsupported backend does not assert an actual budget percent"),Text.Contains(TEXT("MiB/s · 0.0%")));
 FVirtualPcdBandwidthWindow Overflow;
 for(int I=0;I<513;++I)Overflow.Record(10.,1,2,1);
 TestFalse(TEXT("bounded history reports loss instead of silent undercount"),Overflow.Read(10.).bComplete);
 TestTrue(TEXT("overflow clears after missing events expire"),Overflow.Read(12.).bComplete);
 TestEqual(TEXT("unknown header cannot claim100% compression"),FVirtualPcdCodec::EncodedPointBytes({},true),int64(-1));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPcdLongMetadataTest,"MA0T10.PcdBandwidth.LongMetadata",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPcdLongMetadataTest::RunTest(const FString&)
{
 FVirtualLidarPoint Point;Point.bHit=true;Point.SensorLocalPositionMeters=FVector(1.25,-2.5,3.75);
 const TArray<const FVirtualLidarPoint*> Points={&Point};
 const FString Meta=TEXT("{\"context\":\"")+FString::ChrN(200000,TEXT('m'))+TEXT("한글\"}");
 for(auto Profile:{EVirtualPcdFieldProfile::Full,EVirtualPcdFieldProfile::Compact,EVirtualPcdFieldProfile::Minimal,EVirtualPcdFieldProfile::Custom})for(bool Compressed:{false,true})
 {
  FVirtualPcdSendOptions O;O.Profile=Profile;O.bCompressed=Compressed;O.CustomFieldMask=15;
  TArray<uint8> Bytes,Plain;FString Error;FVirtualPcdDocument Doc,Reference;
  TestTrue(TEXT("encode long UTF8 META for every profile/encoding"),FVirtualPcdCodec::Encode(Points,Meta,O,Bytes));
  TestTrue(TEXT("decode self-generated header beyond64KiB"),FVirtualPcdCodec::Decode(Bytes,Doc,Error));
  TestTrue(TEXT("complete UTF8 META retained"),Doc.PayloadOffset>65536&&Doc.Header.Contains(Meta));
  TestEqual(TEXT("long-header accounting excludes header/size prefix"),FVirtualPcdCodec::EncodedPointBytes(Bytes,Compressed),int64(Bytes.Num()-Doc.PayloadOffset-(Compressed?8:0)));
  O.bCompressed=false;FVirtualPcdCodec::Encode(Points,TEXT(""),O,Plain);FVirtualPcdCodec::Decode(Plain,Reference,Error);
  TestTrue(TEXT("long META does not alter selected point bits"),Doc.Records==Reference.Records);
 }
 for(bool Compressed:{false,true})
 {
  FVirtualPcdSendOptions O;O.bCompressed=Compressed;TArray<uint8> Bare,Bytes;FVirtualPcdDocument Doc;FString Error;
  FVirtualPcdCodec::Encode(Points,TEXT(""),O,Bare);FVirtualPcdCodec::Decode(Bare,Doc,Error);
  const FString EmptyJson=TEXT("{\"pad\":\"\"}");
  const int32 Fill=FVirtualPcdCodec::MaxHeaderBytes-Doc.PayloadOffset-FTCHARToUTF8(TEXT("# MA0T10_META \n")).Length()-FTCHARToUTF8(*EmptyJson).Length();
  const FString AtLimit=TEXT("{\"pad\":\"")+FString::ChrN(Fill,TEXT('p'))+TEXT("\"}");
  TestTrue(TEXT("inclusive1MiB UTF8 header boundary accepted by writer"),FVirtualPcdCodec::Encode(Points,AtLimit,O,Bytes));
  TestTrue(TEXT("inclusive1MiB boundary accepted by reader"),FVirtualPcdCodec::Decode(Bytes,Doc,Error));
  TestEqual(TEXT("header limit includes DATA newline"),Doc.PayloadOffset,FVirtualPcdCodec::MaxHeaderBytes);
  TestEqual(TEXT("accounting recognizes marker ending exactly at bound"),FVirtualPcdCodec::EncodedPointBytes(Bytes,Compressed),int64(Bytes.Num()-Doc.PayloadOffset-(Compressed?8:0)));
  TArray<uint8> Beyond={uint8('#'),uint8('x'),uint8('\n')};Beyond.Append(Bytes);
  TestFalse(TEXT("external header above bound remains rejected"),FVirtualPcdCodec::Decode(Beyond,Doc,Error));
  TestEqual(TEXT("oversized external header cannot claim valid accounting"),FVirtualPcdCodec::EncodedPointBytes(Beyond,Compressed),int64(-1));
  const FString TooLong=TEXT("{\"pad\":\"")+FString::ChrN(Fill+1,TEXT('p'))+TEXT("\"}");
  TestFalse(TEXT("writer rejects over-bound META instead of generating unreadable PCD"),FVirtualPcdCodec::Encode(Points,TooLong,O,Bytes));
  TestTrue(TEXT("rejected encoding returns no partial PCD"),Bytes.IsEmpty());
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPcdRequiredDivisorTest,"MA0T10.PcdBandwidth.RequiredDataDivisorPreflight",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPcdRequiredDivisorTest::RunTest(const FString&)
{
 auto* W=FAutomationEditorCommonUtils::CreateNewMap();auto* C=W->SpawnActor<AVirtualSensorCoordinator>();
 auto* A=W->SpawnActor<AVirtualLidarSensorActor>();auto* B=W->SpawnActor<AVirtualLidarSensorActor>();
 A->ScanComponent->SensorId=TEXT("divided");B->ScanComponent->SensorId=TEXT("full-rate");C->RegisterSensorActor(A);C->RegisterSensorActor(B);A->StopSensor();B->StopSensor();
 auto* P=C->StreamPublisherComponent.Get();auto* T=C->SharedTransportComponent.Get();T->TransportMode=EVirtualSensorTransportMode::StompWebSocket;
 auto Profile=T->GetTransportProfile();Profile.BrokerUrl=TEXT("tcp://127.0.0.1:1");Profile.UserName.Reset();Profile.ExportTopic=TEXT("topic.pcd-local");T->ConfigureTransportProfile(Profile);T->SetSessionCredentials(TEXT(""),TEXT(""));
 auto* Session=W->GetSubsystem<UVirtualSensorSlabContextSubsystem>();auto* Raw=W->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>();
 FVirtualSlabSensorOutputSelection Outputs=FVirtualSlabSensorOutputSelection::ObservationOnly();Outputs.bPointCloud=true;
 FString Error;FSlabExecutionOptions Execution;Execution.Policy=ESlabExecutionPolicy::RequireData;
 const FString Scenario=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
 for(int32 Divisor:{1,2,4})
 {
  FVirtualPcdSendOptions O;O.SendDivisor=Divisor;TestTrue(TEXT("configure wildcard divisor"),P->SetPcdSendOptions(TEXT(""),O,true,Error));
  FVirtualPcdSendOptions Full;TestTrue(TEXT("exact target retains1/1"),P->SetPcdSendOptions(B->GetSensorId(),Full,false,Error));
  const auto Required=UVirtualSensorOutputPreflight::InspectForPresentation(W,Outputs,{A->GetSensorId()},EVirtualSensorPreflightPurpose::ScenarioTransition,true,60,true);
  const auto PolicyError=[](const FString& Text){return Text.Contains(TEXT("PCD 전송 1/1"));};
  TestEqual(TEXT("required-data UI preflight blocks only configured division"),Required.BlockingReasons.ContainsByPredicate(PolicyError),Divisor!=1);
  const auto Observation=UVirtualSensorOutputPreflight::InspectForPurpose(W,Outputs,{A->GetSensorId()},EVirtualSensorPreflightPurpose::ScenarioTransition,60);
  TestFalse(TEXT("observation preflight does not block intended division"),Observation.BlockingReasons.ContainsByPredicate(PolicyError));
  TestEqual(TEXT("observation warns about stricter required-data contract"),Observation.Warnings.ContainsByPredicate(PolicyError),Divisor!=1);
  const auto Other=UVirtualSensorOutputPreflight::InspectForPresentation(W,Outputs,{B->GetSensorId()},EVirtualSensorPreflightPurpose::ScenarioTransition,true,60,true);
  TestFalse(TEXT("non-target divided sensor cannot block full-rate selected target"),Other.BlockingReasons.ContainsByPredicate(PolicyError));
  if(Divisor==1)continue;
  const auto Before=Session->GetSlabSensorSessionStatus();const auto Connection=Raw->GetConnectionRevision();
  const FString Run=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
  TestFalse(TEXT("authoritative required-data start rejects division before preparation"),Session->PrepareScenarioTransmission(Run,{A->GetSensorId()},Scenario,Outputs,Execution,Error));
  TestTrue(TEXT("blocking message includes actual divisor and corrective choice"),Error.Contains(FString::Printf(TEXT("1/%d"),Divisor))&&Error.Contains(TEXT("1/1"))&&Error.Contains(TEXT("관찰 허용")));
  TestEqual(TEXT("preflight rejection does not start a run"),Session->GetSlabSensorSessionStatus().RunId,Before.RunId);
  TestEqual(TEXT("preflight rejection leaves session state unchanged"),Session->GetSlabSensorSessionStatus().State,Before.State);
  TestFalse(TEXT("rejection claims no sensor ownership"),Session->ControlsSensor(A->GetSensorId()));
  TestFalse(TEXT("rejection starts no acquisition"),A->IsSensorRunning());
  TestEqual(TEXT("rejection starts no transport connection"),Raw->GetConnectionRevision(),Connection);
  TestEqual(TEXT("rejection registers no managed run"),Raw->GetRunRetentionSnapshot(Run).KnownRunIds,0);
  FSlabRunDeliverySummary Summary;TestFalse(TEXT("rejection creates no failed delivery ledger"),W->GetSubsystem<USlabRunResultsSubsystem>()->GetRunResult(Run,Summary));
 }
 // Full-rate selected target is valid even while another target retains the global1/4.
 const FString Run=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
 TestTrue(TEXT("full-rate target can prepare required data"),Session->PrepareScenarioTransmission(Run,{B->GetSensorId()},Scenario,Outputs,Execution,Error));
 Session->EndSlabSensorSession(Run,true);Raw->StopHighThroughputTransport();
 C->Destroy();A->Destroy();B->Destroy();return true;
}
#endif
