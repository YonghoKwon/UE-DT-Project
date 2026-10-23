#pragma once

#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualPcdCodec.h"

/** L3 display adapter only. Field bits, order, sizes and wire names belong to the codec. */
namespace VirtualPcdOptionsPresentation
{
    inline const TCHAR* FieldLabel(int32 Index)
    {
        static const TCHAR* Labels[]={TEXT("X 좌표"),TEXT("Y 좌표"),TEXT("Z 좌표"),TEXT("반사 강도"),TEXT("채널 번호"),TEXT("수평 샘플 번호"),TEXT("반사 순번"),TEXT("반사 수"),TEXT("프레임 내 시간차(ns)"),TEXT("유효 여부"),TEXT("신뢰도")};
        return Index>=0&&Index<UE_ARRAY_COUNT(Labels)?Labels[Index]:TEXT("");
    }

    inline FString FieldTooltip(int32 Index)
    {
        const FString Raw=FVirtualPcdCodec::FieldName(Index);
        return Index>=0&&Index<3?Raw+TEXT(" · 센서 로컬 m · X 전방 / Y 좌측 / Z 위쪽 · 필수 필드"):Raw;
    }

    inline FString ProfileLabel(EVirtualPcdFieldProfile Profile)
    {
        switch(Profile)
        {
        case EVirtualPcdFieldProfile::Compact:return TEXT("축소(Compact) · 24 B/점");
        case EVirtualPcdFieldProfile::Minimal:return TEXT("위치만(Minimal) · 12 B/점");
        case EVirtualPcdFieldProfile::Custom:return TEXT("사용자 지정(Custom)");
        default:return TEXT("전체(Full) · 33 B/점 (기본)");
        }
    }

    inline FString IncludedFields(const FVirtualPcdSendOptions& Options,bool Raw=false)
    {
        TArray<FString> Names;
        for(int32 I=0;I<11;++I)if(Options.FieldMask()&(1<<I))Names.Add(Raw?FVirtualPcdCodec::FieldName(I):FieldLabel(I));
        return FString::Join(Names,TEXT(" · "));
    }

    inline FString FieldSummary(const FVirtualPcdSendOptions& Options)
    {
        switch(Options.Profile)
        {
        case EVirtualPcdFieldProfile::Full:return TEXT("포함: 좌표·반사·채널·시간 등 전체 11개");
        case EVirtualPcdFieldProfile::Compact:return TEXT("포함: 좌표·반사 강도·채널·시간차");
        case EVirtualPcdFieldProfile::Minimal:return TEXT("포함: X / Y / Z 좌표만");
        default:return TEXT("포함: ")+IncludedFields(Options);
        }
    }

    inline FString DivisorLabel(int32 Divisor)
    {
        return Divisor==4?TEXT("4프레임마다(1/4)"):Divisor==2?TEXT("2프레임마다(1/2)"):TEXT("매 프레임(1/1)");
    }

    inline const TCHAR* DivisorTooltip()
    { return TEXT("측정 Hz와 서버 전송 간격은 별개입니다. 예: 20 Hz 측정 시 매 프레임 20 Hz / 2프레임마다 10 Hz / 4프레임마다 5 Hz. 실제 전송률을 뜻하지 않습니다."); }

    inline const TCHAR* CompressionCompatibility()
    { return TEXT("기본 On · PCD 표준 binary_compressed(LZF). PCL(loadPCDFile)·본 프로젝트 수신기·ROS2 브리지는 읽을 수 있음. 자체 파서를 쓰는 서버는 압축 해제 지원 확인 후 사용, 미지원이면 체크 해제."); }
}
