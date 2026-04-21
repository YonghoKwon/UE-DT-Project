// Fill out your copyright notice in the Description page of Project Settings.


#include "VirtualCameraAct.h"

#include "VirtualCameraComp.h"


// Sets default values
AVirtualCameraAct::AVirtualCameraAct()
{
	PrimaryActorTick.bCanEverTick = false;

	// 컴포넌트 생성
	VirtualCameraComp = CreateDefaultSubobject<UVirtualCameraComp>(TEXT("VirtualCameraComp"));

	// 이 컴포넌트를 액터의 최상위(Root) 컴포넌트로 설정하여 공간상에서 직접 트랜스폼(위치/회전)을 조작할 수 있게 함
	RootComponent = VirtualCameraComp;
}

// Called when the game starts or when spawned
void AVirtualCameraAct::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void AVirtualCameraAct::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

