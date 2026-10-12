#include "SlabInformationCard.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "Internationalization/Internationalization.h"

namespace
{
    FString Number(double Value,int32 Decimals,bool Trim=false)
    {
        if(!FMath::IsFinite(Value))return TEXT("—");
        FNumberFormattingOptions Options;
        Options.SetUseGrouping(true).SetMinimumFractionalDigits(Trim?0:Decimals).SetMaximumFractionalDigits(Decimals);
        return FText::AsNumber(Value,&Options,FInternationalization::Get().GetCulture(TEXT("en-US"))).ToString();
    }
    const TCHAR* Unit(ESlabInputUnit U)
    {
        return U==ESlabInputUnit::Millimeters?TEXT("mm"):U==ESlabInputUnit::Meters?TEXT("m"):TEXT("cm");
    }
}
#define LOCTEXT_NAMESPACE "SlabInformationCard"
TArray<SlabInformationCard::FRow> SlabInformationCard::BuildRows(const ASlabActor* Slab,bool IncludeSource)
{
    const FText Material=LOCTEXT("Material","소재"),Progress=LOCTEXT("Progress","진행"),Pose=LOCTEXT("Pose","자세·위치"),Source=LOCTEXT("Source","원본 행 상세");
    TArray<FRow> Rows={
        {Material,LOCTEXT("MaterialId","소재번호"),TEXT("—")},
        {Material,LOCTEXT("Dimensions","치수 L×W×T"),TEXT("—")},
        {Material,LOCTEXT("Weight","중량"),TEXT("—")},
        {Progress,LOCTEXT("Row","원본 행"),TEXT("—")},
        {Progress,LOCTEXT("Time","경과 / 전체"),TEXT("—")},
        {Progress,LOCTEXT("Percent","진행률"),TEXT("—")},
        {Pose,LOCTEXT("CurrentPosition","현재 표시 위치"),TEXT("—")},
        {Pose,LOCTEXT("CurrentSpeed","현재 표시 속도"),TEXT("—")},
        {Pose,LOCTEXT("Angles","좌 / 우 기울기"),TEXT("—")},
        {Pose,LOCTEXT("Offset","중심 이탈 (행)"),TEXT("—")},
        {Pose,LOCTEXT("Margins","좌 / 우 여유 (행)"),TEXT("—")},
        {Pose,LOCTEXT("Meandering","사행 여부"),TEXT("—")}
    };
    if(IncludeSource)
    {
        Rows.Add({Source,LOCTEXT("SourcePosition","원본 행 위치"),TEXT("—"),true});
        Rows.Add({Source,LOCTEXT("SourceSpeed","원본 행 속도"),TEXT("—"),true});
    }
    if(!IsValid(Slab))return Rows;
    const auto Status=Slab->GetSimulationStatus();const auto Data=Slab->GetScenario();
    if(!Data||Data->ScenarioUUID!=Status.ScenarioUUID||!Data->Rows.IsValidIndex(Status.RowIndex)||Status.State==ESlabSimulationState::Idle||Status.State==ESlabSimulationState::Preparing)return Rows;
    const auto& R=Data->Rows[Status.RowIndex];
    const auto Metrics=Slab->CalculateMetricsForRow(R);
    Rows[0].Value=R.MtlNo;
    Rows[1].Value=Number(R.Length,3,true)+TEXT(" × ")+Number(R.Width,3,true)+TEXT(" × ")+Number(R.Thickness,3,true)+TEXT(" ")+Unit(Slab->GetDimensionUnit());
    Rows[2].Value=Number(R.Weight,3,true)+TEXT(" (원본 단위)");
    Rows[3].Value=FString::Printf(TEXT("행 %d/%d"),Status.RowIndex+1,Status.RowCount);
    Rows[4].Value=Number(Status.ElapsedSec,1)+TEXT(" / ")+Number(Status.DurationSec,1)+TEXT(" s");
    Rows[5].Value=Number(Status.Progress*100,1)+TEXT(" %");
    const FVector Current=Slab->GetTrackLocalPositionCm();
    Rows[6].Value=TEXT("X ")+Number(Current.X,1)+TEXT(" · Y ")+Number(Current.Y,1)+TEXT(" · Z ")+Number(Current.Z,1)+TEXT(" cm");
    if(Status.State==ESlabSimulationState::Playing)Rows[7].Value=Number(Slab->GetCurrentMetrics().SpeedCmPerSec,1)+TEXT(" cm/s");
    else if(Status.State==ESlabSimulationState::Paused)Rows[7].Value=TEXT("0.0 cm/s (일시정지)");
    else if(Status.State==ESlabSimulationState::Completed)Rows[7].Value=TEXT("0.0 cm/s");
    Rows[8].Value=Number(R.LeftAngle,2)+TEXT("° / ")+Number(R.RightAngle,2)+TEXT("°");
    Rows[9].Value=Number(Metrics.CenterOffsetCm,1)+TEXT(" cm");
    if(Metrics.bMarginsValid)Rows[10].Value=Number(Metrics.MarginLeftCm,1)+TEXT(" / ")+Number(Metrics.MarginRightCm,1)+TEXT(" cm");
    if(R.bHasMeandering)Rows[11].Value=R.bMeandering?TEXT("예 (원본 값)"):TEXT("아니요 (원본 값)");
    if(IncludeSource)
    {
        const int32 Decimals=Slab->GetPositionUnit()==ESlabInputUnit::Centimeters?1:3;
        Rows[12].Value=TEXT("X ")+Number(R.CenterX,Decimals)+TEXT(" · Y ")+(R.bHasCenterY?Number(R.CenterY,Decimals):FString(TEXT("—")))+TEXT(" ")+Unit(Slab->GetPositionUnit());
        if(Data->Rows.IsValidIndex(Status.RowIndex+1))
        {
            const auto& Next=Data->Rows[Status.RowIndex+1];const double Dt=Next.ElapsedSec-R.ElapsedSec;
            if(Dt>0&&FMath::IsFinite(Dt))Rows[13].Value=Number((Next.CenterX-R.CenterX)/Dt,Decimals)+TEXT(" ")+Unit(Slab->GetPositionUnit())+TEXT("/s");
        }
    }
    return Rows;
}
FString SlabInformationCard::Build(const ASlabActor* Slab)
{
    TArray<FString> Lines;
    for(const auto& Row:BuildRows(Slab,true))Lines.Add(Row.Label.ToString()+TEXT(": ")+Row.Value);
    return FString::Join(Lines,TEXT("\n"));
}
#undef LOCTEXT_NAMESPACE
