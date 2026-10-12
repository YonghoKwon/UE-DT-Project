#include "SlabInformationCard.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "SensorToolUiText.h"

FString SlabInformationCard::Build(const ASlabActor* Slab)
{
    const FString Empty=SensorToolUiText::SlabInformationEmpty().ToString();
    if(!IsValid(Slab))return Empty;
    const auto Status=Slab->GetSimulationStatus();const auto Data=Slab->GetScenario();
    if(!Data||Data->ScenarioUUID!=Status.ScenarioUUID||!Data->Rows.IsValidIndex(Status.RowIndex)||Status.State==ESlabSimulationState::Idle||Status.State==ESlabSimulationState::Preparing)return Empty;
    const auto& R=Data->Rows[Status.RowIndex];
    const auto Unit=[](ESlabInputUnit U){return U==ESlabInputUnit::Millimeters?TEXT("mm"):U==ESlabInputUnit::Meters?TEXT("m"):TEXT("cm");};
    const auto Number=[](double V){return FMath::IsFinite(V)?FString::Printf(TEXT("%.3f"),V):FString(TEXT("—"));};
    const auto Metrics=Slab->CalculateMetricsForRow(R);
    FString Speed=TEXT("—");
    if(Data->Rows.IsValidIndex(Status.RowIndex+1))
    {
        const auto& Next=Data->Rows[Status.RowIndex+1];const double Dt=Next.ElapsedSec-R.ElapsedSec;
        if(Dt>0&&FMath::IsFinite(Dt))Speed=Number((Next.CenterX-R.CenterX)/Dt)+TEXT(" ")+Unit(Slab->GetPositionUnit())+TEXT("/s");
    }
    const FString SourceText=FString::Printf(SensorToolUiText::SlabInformationFormat,
        *R.MtlNo,*Number(R.Length),*Number(R.Width),*Number(R.Thickness),Unit(Slab->GetDimensionUnit()),*Number(R.Weight),Status.RowIndex+1,Status.RowCount,Status.ElapsedSec,Status.DurationSec,Status.Progress*100,
        *Number(R.CenterX),*(R.bHasCenterY?Number(R.CenterY):FString(TEXT("—"))),Unit(Slab->GetPositionUnit()),*Speed,*Number(R.LeftAngle),*Number(R.RightAngle),*Number(Metrics.CenterOffsetCm),
        *(Metrics.bMarginsValid?Number(Metrics.MarginLeftCm)+TEXT(" / ")+Number(Metrics.MarginRightCm)+TEXT(" cm"):FString(TEXT("—"))),
        R.bHasMeandering?(R.bMeandering?TEXT("예 (원본 값)"):TEXT("아니요 (원본 값)")):TEXT("—"));
    const FVector Current=Slab->GetTrackLocalPositionCm();
    const FString CurrentSpeed=Status.State==ESlabSimulationState::Playing?Number(Slab->GetCurrentMetrics().SpeedCmPerSec)+TEXT(" cm/s"):Status.State==ESlabSimulationState::Paused||Status.State==ESlabSimulationState::Completed?FString(TEXT("0.000 cm/s")):FString(TEXT("—"));
    return SourceText+FString::Printf(SensorToolUiText::SlabCurrentPositionFormat,*Number(Current.X),*Number(Current.Y),*Number(Current.Z),*CurrentSpeed);
}
