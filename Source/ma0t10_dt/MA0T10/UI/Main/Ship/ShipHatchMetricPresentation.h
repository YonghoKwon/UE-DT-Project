#pragma once
#include "CoreMinimal.h"

namespace ShipHatchMetricPresentation
{
    inline const TCHAR* Caption(int32 Index)
    {
        const TCHAR* Labels[]={TEXT("남은 적재율\n모의값"),TEXT("남은 부피\n모의값"),TEXT("최고 높이\n측정값"),TEXT("평균 높이\n측정값"),TEXT("최저 높이\n측정값"),TEXT("미관측 격자 비율\n측정값")};
        return Index>=0&&Index<UE_ARRAY_COUNT(Labels)?Labels[Index]:TEXT("");
    }
    inline const TCHAR* Source(int32 Index)
    {
        if(Index==5)return TEXT("최근 검출점이 없는 격자의 비율입니다. 장비 고장 판정이 아닙니다. 차폐·거리·시야 범위는 회색 격자 상세(마우스 올리기)에서 확인하세요. 현재 격자 중심 시선 진단은 실제 측정 샘플과 별도입니다. 새 측정 대기·정지 중에는 마지막 값이 남아 있을 수 있습니다.");
        return Index<2?TEXT("모의 원료의 전체 홀드 기준 값입니다. 센서 측정으로 산출한 값이 아닙니다."):
            TEXT("수신한 센서 측정점의 분석 결과입니다. 상단 측정 상태와 마지막 갱신을 함께 확인하세요. 새 측정 대기·정지 중에는 마지막 값이 남아 있을 수 있습니다.");
    }
    inline const TCHAR* CoverageHint()
    { return TEXT("미관측: 최근 검출점 없는 격자 · 차폐·거리·시야 범위는 회색 격자에 마우스를 올려 확인"); }
}
