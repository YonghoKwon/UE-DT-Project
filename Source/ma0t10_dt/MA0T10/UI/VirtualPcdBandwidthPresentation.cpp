#include "VirtualPcdBandwidthPresentation.h"

FText FVirtualPcdBandwidthPresentation::Text() const
{
 if(ActiveCount==0)
  return FText::FromString(FString::Printf(TEXT("최대 추정 · %d B/점 (압축 전)\n선택 대당 %.2f MiB/s / %.1f Mbps\n합계 %d대 %.2f MiB/s / %.1f Mbps · %.0f MiB/s 예산 %.1f%%\n미송신 · 규격 최대 점 수 × 비압축 크기 · 압축 효과 미반영"),PointBytes,SelectedMaximum/1048576.,SelectedMaximum*8/1000000.,SensorCount,TotalMaximum/1048576.,TotalMaximum*8/1000000.,BudgetMiB,BudgetMiB>0?TotalMaximum/1048576./BudgetMiB*100:0.));
 if(UnmeasuredCount>0||!Total.bComplete)
  return FText::FromString(TEXT("송신 중 · 최근 1초 실측 집계 불완전\nRaw TCP PCD binary/binary_compressed 송신 완료 후 표시 · 최대 추정 및 예산 % 미표시"));
 const FString Ratio=Total.OriginalPointBytes>0?FString::Printf(TEXT("payload 압축 절감 %.1f%%"),Total.ReductionPercent()):TEXT("압축률 대기");
 return FText::FromString(FString::Printf(TEXT("최근 1초 실측 · %d B/점 (압축 전)\n선택 대당 %.1f Hz · %.2f MiB/s / %.1f Mbps\n송신 합계 %d대 %.2f MiB/s / %.1f Mbps · %.0f MiB/s 예산 %.1f%%\n%s · PCD 헤더 포함 body 바이트 · socket 쓰기 완료 기준"),PointBytes,double(Selected.Frames),Selected.BodyBytes/1048576.,Selected.BodyBytes*8/1000000.,ActiveCount,Total.BodyBytes/1048576.,Total.BodyBytes*8/1000000.,BudgetMiB,BudgetMiB>0?Total.BodyBytes/1048576./BudgetMiB*100:0.,*Ratio));
}
