#pragma once
#include "SensorNativeSafetyStatus.h"

namespace LidarNativeSafetyText
{
inline const TCHAR* Message(SensorNativeSafety::EStatus Status)
{
    switch(Status)
    {
    case SensorNativeSafety::EStatus::Banned:
        return TEXT("native 비활성(비정상 종료 2회, 드라이버 변경 또는 안전 상태 초기화 필요)");
    case SensorNativeSafety::EStatus::Recovery:
        return TEXT("native 복구 확인 중(이전 비정상 종료, 이번 실행은 RHI 사용)");
    default:return TEXT("");
    }
}
}
