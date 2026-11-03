#pragma once
#include "CoreMinimal.h"

/** Presentation only; inputs and output are in the monitor's viewport UI units. */
namespace CoilLoadingPresentation
{
    inline FString FontLabel(float Scale)
    { return FString::Printf(TEXT("코일 글자 %d%%"),FMath::RoundToInt(Scale*100)); }

    inline FString MetricSummary(double TotalMass,double NextMass,double KG,double GM,double Heel,const FString& Udl,double CurrentDraft,double NextDraft,double Tpc,bool Confirmed)
    {
        return FString::Printf(TEXT("다음 포함 %.2f t · 다음 코일 %.3f t · 무게중심 높이(KG) %.3f m / 복원성 높이(GM) %.3f m\n횡경사 %+.3f° · 균등분포하중(UDL) %s · 흘수 현재 %.3f m / 다음 포함 %.3f m\n흘수 1cm당 톤수(TPC) %.2f t/cm%s · 코일 구멍축 선수미 / CSS Annex6"),TotalMass,NextMass,KG,GM,Heel,*Udl,CurrentDraft,NextDraft,Tpc,Confirmed?TEXT(""):TEXT(" · 자료 필요"));
    }

    inline FString ComparisonSummary(const FString& PhysicsState,double Displacement,double Rotation,double Hz,int32 Fits,bool Compared,double Deviation,double Residual,const FString& DeviceLabel=FString(),double TargetHz=20.)
    {
        const FString Center=Compared?FString::Printf(TEXT("%.3f m"),Deviation):TEXT("비교 불가 (점/방향 부족)");
        return FString::Printf(TEXT("%s물리: %s · 이동 %.3f m · 회전 %.1f° · LiDAR %.1f Hz / %.0f Hz · 코일 맞춤 %d개\n중심 편차 p95 %s / 표면 잔차 %.3f m"),*(DeviceLabel.IsEmpty()?FString():DeviceLabel+TEXT(" · ")),*PhysicsState,Displacement,Rotation,Hz,TargetHz,Fits,*Center,Compared?Residual:0.);
    }

    inline FString IssueSummary(int32 Danger,int32 Caution,int32 Unknown)
    { return FString::Printf(TEXT("위험 %d · 주의 %d · 미확인 %d개 사유"),Danger,Caution,Unknown); }

    inline FString AdditionalChecks(int32 Unknown,bool Expanded)
    { return FString::Printf(TEXT("추가 확인 %d개 사유 · %s"),Unknown,Expanded?TEXT("접기"):TEXT("펼치기")); }

    inline const TCHAR* UnknownExplanation()
    { return TEXT("미확인은 같은 사유를 묶은 수입니다. 코일 수·문서 수가 아닙니다. 위험 0도 확인 완료를 뜻하지 않습니다. 아래 사유와 확인 위치에서 현재 입력을 확인하세요."); }

    inline const TCHAR* InputLocation(FName Code)
    {
        if(Code==TEXT("HydrostaticsUnknown"))return TEXT("설정 · 상세 → TPC·기준 흘수·실선 TPC 확인");
        if(Code==TEXT("DraftLimitUnknown"))return TEXT("설정 · 상세 → 허용 흘수");
        if(Code==TEXT("GearWLLUnknown"))return TEXT("설정 · 상세 → 장구 다리 WLL");
        if(Code==TEXT("LashingUnknown"))return TEXT("설정 · 상세 → 래싱 있음·선체 연결 래싱 내력");
        if(Code==TEXT("ChockUnknown"))return TEXT("설정 · 상세 → 쐐기 있음·쐐기 검증 내력");
        if(Code==TEXT("LocalUnknown"))return TEXT("설정 · 상세 → 국부 허용압");
        if(Code==TEXT("PatchUnknown"))return TEXT("설정 · 상세 → 승인 유효 면적");
        if(Code==TEXT("WetWoodFriction"))return TEXT("다음 배치 → 목재 접촉 μ / 설정 · 상세 → 냉연 포장 접촉 μ 인증값 확인");
        if(Code==TEXT("RollingCapacityUnknown"))return TEXT("설정 · 상세 → 쐐기·래싱 내력·구름 저항 모멘트 팔");
        if(Code==TEXT("OvalityUnknown"))return TEXT("설정 · 상세 → 보정 지름 강성·제조사 ovality 한계");
        if(Code==TEXT("TrimUnknown"))return TEXT("설정 · 상세 → MCT1cm");
        if(Code==TEXT("LCFUnknown"))return TEXT("설정 · 상세 → LCF·LCF 정수력 입력 확인");
        if(Code==TEXT("CSSUnknown"))return TEXT("설정 · 상세 → 선박 운동 시험·Loading Manual 입력 확인 / 승인 자료 대조");
        return TEXT("설정 · 상세 → 선박 입력·승인 자료 대조 (입력만으로 확인할 수 없는 항목 포함)");
    }

    inline FBox2D AvailableArea(FVector2D Viewport, FVector2D ToolsInset)
    {
        const double Left=FMath::Clamp(ToolsInset.X,0.,FMath::Max(0.,Viewport.X-1.));
        const double Bottom=FMath::Clamp(ToolsInset.Y,0.,FMath::Max(0.,Viewport.Y-1.));
        return FBox2D(FVector2D(Left,0),FVector2D(Viewport.X,Viewport.Y-Bottom));
    }
}
