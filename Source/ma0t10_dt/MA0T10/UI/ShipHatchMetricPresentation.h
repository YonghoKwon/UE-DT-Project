#pragma once
#include "CoreMinimal.h"

namespace ShipHatchMetricPresentation
{
    inline const TCHAR* Caption(int32 Index)
    {
        const TCHAR* Labels[]={TEXT("남은 적재율\n모의값"),TEXT("남은 부피\n모의값"),TEXT("최고 높이\n측정값"),TEXT("평균 높이\n측정값"),TEXT("최저 높이\n측정값"),TEXT("측정 불가 비율\n측정값")};
        return Index>=0&&Index<UE_ARRAY_COUNT(Labels)?Labels[Index]:TEXT("");
    }
    inline const TCHAR* Source(int32 Index)
    {
        return Index<2?TEXT("모의 원료의 전체 홀드 기준 값입니다. 센서 측정으로 산출한 값이 아닙니다."):
            TEXT("수신한 센서 측정점의 분석 결과입니다. 상단 측정 상태와 마지막 갱신을 함께 확인하세요. 새 측정 대기·정지 중에는 마지막 값이 남아 있을 수 있습니다.");
    }
}
