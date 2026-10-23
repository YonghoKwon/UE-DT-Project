#include "VirtualPcdBandwidthPresentation.h"

FText FVirtualPcdBandwidthPresentation::Text() const
{
 if(ActiveCount==0)
  return FText::FromString(FString::Printf(TEXT("전송 정지 · 압축 전 최대 추정\n선택 %.2f MiB/s (%.1f Mbps)\n합계 %d대 %.2f MiB/s (%.1f Mbps)\n설정 예산 %.0f MiB/s · %.1f%%"),SelectedMaximum/1048576.,SelectedMaximum*8/1000000.,SensorCount,TotalMaximum/1048576.,TotalMaximum*8/1000000.,BudgetMiB,BudgetMiB>0?TotalMaximum/1048576./BudgetMiB*100:0.));
 if(UnmeasuredCount>0||!Total.bComplete)
  return FText::FromString(TEXT("전송 중 · 최근 1초 실측 집계 불완전\n실측 대역폭·예산 비율 확인 불가"));
 const FString Ratio=Total.OriginalPointBytes>0?FString::Printf(TEXT("점 데이터 압축 절감 %.1f%%"),Total.ReductionPercent()):TEXT("압축률 대기");
 return FText::FromString(FString::Printf(TEXT("전송 중 · 최근 1초 실측\n선택 %.2f MiB/s (%.1f Mbps) · %.1f Hz\n합계 %d대 %.2f MiB/s (%.1f Mbps)\n설정 예산 %.0f MiB/s · %.1f%%\n%s"),Selected.BodyBytes/1048576.,Selected.BodyBytes*8/1000000.,double(Selected.Frames),ActiveCount,Total.BodyBytes/1048576.,Total.BodyBytes*8/1000000.,BudgetMiB,BudgetMiB>0?Total.BodyBytes/1048576./BudgetMiB*100:0.,*Ratio));
}

FText FVirtualPcdBandwidthPresentation::Details() const
{
 const FString Size=FString::Printf(TEXT("%d B/점 (압축 전) · MiB/s가 주 단위, Mbps는 보조 단위\n"),PointBytes);
 if(ActiveCount==0)return FText::FromString(Size+TEXT("전송 안 함 · 규격 최대 점 수 × 비압축 크기 · 압축 효과 미반영. 추정 예산 초과는 실제 전송 장애를 뜻하지 않습니다."));
 if(UnmeasuredCount>0||!Total.bComplete)return FText::FromString(Size+TEXT("전송 형식 미지원 또는 최근 1초 집계 불완전으로 실측 대역폭을 확인할 수 없습니다 · 추정 예산 % 미표시. 0이나 성공으로 판단하지 마세요."));
 return FText::FromString(Size+TEXT("PCD 파일 바이트(헤더 포함) · 전송 완료 기준 · 압축 절감은 점 데이터 기준. 서버 수락이나 외부 프로그램 처리 완료를 뜻하지 않습니다."));
}
