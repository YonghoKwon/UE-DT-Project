#pragma once
#include "CoreMinimal.h"

namespace SensorLidarHelp
{
inline FText Summary(FName Key)
{
    if(Key==TEXT("PointTime"))return NSLOCTEXT("SensorLidarHelp","TimeShort","점을 찍은 순서대로 시각을 기록합니다.");
    if(Key==TEXT("MotionDistortion"))return NSLOCTEXT("SensorLidarHelp","MotionShort","움직이는 물체의 점군이 늘어나거나 비틀립니다.");
    if(Key==TEXT("MaterialResponse"))return NSLOCTEXT("SensorLidarHelp","MaterialShort","표면에 따라 반사 강도와 검출점이 달라집니다.");
    if(Key==TEXT("MultiHit"))return NSLOCTEXT("SensorLidarHelp","MultiShort","한 광선이 만난 여러 표면의 점을 기록합니다.");
    if(Key==TEXT("ReflectionIntensity"))return NSLOCTEXT("SensorLidarHelp","IntensityShort","강하게 반사한 점은 밝게, 약한 점은 어둡게 표시합니다.");
    if(Key==TEXT("MeasurementTime"))return NSLOCTEXT("SensorLidarHelp","ColorTimeShort","먼저 찍은 점부터 나중 점까지 색이 바뀝니다.");
    return FText::GetEmpty();
}
inline FText Detail(FName Key)
{
    if(Key==TEXT("PointTime"))return NSLOCTEXT("SensorLidarHelp","TimeDetail","점별 시간차\n켜면 화면에서: 색상을 ‘측정 시각’으로 두면 찍은 순서에 따라 색이 바뀝니다.\n회전형 장비가 약 0.1초에 한 바퀴 찍는 것처럼 한 측정 주기 안의 시각을 나눕니다. 이 프로젝트의 ML-X는 비회전 행 순차 가정이며 제조사 발사 순서를 재현하지 않습니다.\n언제: 시간 정보가 필요한 학습 데이터용. 평소 PCD 송신은 기본 꺼짐입니다.\n비용: 점마다 시각을 계산·기록합니다. 꺼져 있으면 추가 계산하지 않습니다.");
    if(Key==TEXT("MotionDistortion"))return NSLOCTEXT("SensorLidarHelp","MotionDetail","움직임 왜곡\n켜면 화면에서: 측정 중 Slab이 움직이면 점군이 진행 방향으로 늘어나거나 비틀립니다. 멈춘 Slab은 차이가 거의 없습니다. 시간차도 함께 켜집니다.\n언제: 움직임이 있는 학습 데이터용. 평소 PCD 송신은 기본 꺼짐입니다.\n비용: 점별 위치 보정 계산이 추가됩니다. 속도·표면 평면을 이용한 근사이며 가려짐·가속·실장비 왜곡을 모두 재현하지는 않습니다.");
    if(Key==TEXT("MaterialResponse"))return NSLOCTEXT("SensorLidarHelp","MaterialDetail","재질 반사\n켜면 화면에서: 색상을 ‘반사 강도’로 두면 산화 면은 어둡고 금속은 밝게 보이는 등 표면 차이가 나타납니다. 거리·입사각과 표면 설정에 따라 달라집니다.\n언제: 재질 차이를 포함한 학습 데이터용. 평소 PCD 송신은 기본 꺼짐입니다.\n비용: 강도·미검출·거리 오차 계산, GPU에서는 기존 분류 캡처 비용이 추가됩니다. 반사율은 가정값이며 실장비 교정값이 아닙니다.");
    if(Key==TEXT("MultiHit"))return NSLOCTEXT("SensorLidarHelp","MultiDetail","다중 검출\n켜면 화면에서: 광선이 여러 표면을 만나면 모서리 주변 등의 점이 늘어날 수 있습니다. 실제 장비의 가장자리·먼지 반사를 이해하기 위한 근사이며 먼지를 자동 생성하지 않습니다.\n언제: 여러 검출이 필요한 학습 데이터용. 평소 PCD 송신은 기본 꺼짐입니다.\n비용: 높음. CPU 광선 검사·점 수·메모리·전송량이 늘어납니다. GPU 깊이 방식은 첫 표면만 검출하므로 여러 검출을 보장하지 않습니다.");
    if(Key==TEXT("ReflectionIntensity"))return NSLOCTEXT("SensorLidarHelp","IntensityDetail","반사 강도 색상\n켜면 화면에서: 강한 반사는 밝게, 약한 반사는 어둡게 표시됩니다. 재질 반사를 켜면 산화 면·금속 등의 차이를 보기 쉽습니다. 거리와 각도의 영향도 받습니다.\n언제: 반사 강도 학습 데이터나 재질 설정을 확인할 때. 색상은 화면 표시만 바꾸며 PCD 내용은 바꾸지 않습니다. 재질 반사 계산은 기본 꺼짐입니다.\n비용: 기존 점군의 색만 바꿉니다. 재질 반사를 별도로 켜면 추가 측정 비용이 듭니다.");
    if(Key==TEXT("MeasurementTime"))return NSLOCTEXT("SensorLidarHelp","ColorTimeDetail","측정 시각 색상\n켜면 화면에서: 한 측정 주기에서 먼저 찍은 점부터 나중 점까지 색이 변합니다. 점별 시간차가 꺼져 있으면 색 차이가 거의 없습니다.\n언제: 점별 시간차나 움직임 왜곡 학습 데이터를 확인할 때. 색상은 화면 표시만 바꾸며 PCD 내용은 바꾸지 않습니다. 점별 시간차는 기본 꺼짐입니다.\n비용: 기존 점군의 색만 바꿉니다. 점별 시간차·움직임 왜곡을 별도로 켜면 추가 계산이 듭니다.");
    return FText::GetEmpty();
}
}
