#pragma once
#include "DevicePanelBodyCall.h"
#include "DevicePanelWidgetDecl.h"
#include "Blueprint/WidgetTree.h"
#include "DeviceSensorPanelTypes.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SensorOwnedPresentationStyle.h"
#include "SensorToolNumericInput.h"
#include "SensorToolUiText.h"
#include "SensorToolWidgetDecl.h"
#include "SensorToolWorkspaceStyle.h"
#include "Styling/CoreStyle.h"
#include "VirtualSensorUiStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Contracts/DeviceIdentityPolicy.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorDefaults.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorOutputPreflight.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPeriodicFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/DeviceSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorEditableTypes.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorExternalSourceHostActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#define LOCTEXT_NAMESPACE "VirtualSensorCaptureExportPanelWidget"
/** Generic View contract: host policy, public BP state and optional extensions stay in the facade.
 * No concrete project widget, Workspace, scenario or job type is included here.
 * Slate callbacks retain only a weak UObject view; bodies create no tick, subscription or readback. */
struct FDeviceSensorCaptureBody
{

#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "VirtualSensorCaptureExportPanelWidget"
    template<class TView> static TSharedRef<SWidget> BuildCaptureTab(TView& View)
    {
        TWeakObjectPtr<TView> DeviceOwner(&View);
        auto* DeviceSelf=&View;

	return SNew(SScrollBox)+SScrollBox::Slot()[ SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ SNewDeviceTool(DeviceSelf,STextBlock).ColorAndOpacity(DeviceSelf->GetToolAccentColor()).Text(LOCTEXT("CaptureTitle", "수동 및 시간 지정 캡처")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 8)[ SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot()[ SNewDeviceTool(DeviceOwner,SButton).ButtonStyle(&DeviceOwner->GetToolButtonStyle()).Text(LOCTEXT("CaptureOnceV2", "선택 센서 1회 캡처")).OnClicked_Lambda([DeviceOwner]() { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) { Self->CaptureOnce(); return FReply::Handled(); }); }) ]
			+ SWrapBox::Slot()[ SNewDeviceTool(DeviceOwner,SButton).ButtonStyle(&DeviceOwner->GetToolButtonStyle()).Text_Lambda([DeviceOwner]() { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) { return FText::FromString(Self->MonitorWidget && Self->MonitorWidget->IsLocalSensorCaptureActive() ? TEXT("시간 지정 캡처 중지") : TEXT("시간 지정 캡처 시작")); }); }).OnClicked_Lambda([DeviceOwner]() { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) { Self->ToggleTimedCapture(); return FReply::Handled(); }); }) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)[ SNewDeviceTool(DeviceOwner,SEditableTextBox).HintText(LOCTEXT("CaptureInterval", "캡처 간격(초), 0.05~3600")).Text_Lambda([DeviceOwner]() { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) { return FText::AsNumber(Self->CaptureSelection.IntervalSeconds); }); }).OnTextCommitted_Lambda([DeviceOwner](const FText& Text, ETextCommit::Type) { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) { Self->SetCaptureIntervalSeconds(FCString::Atof(*Text.ToString())); }); }) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)[ SNewDeviceTool(DeviceOwner,SButton).ButtonStyle(&DeviceOwner->GetToolButtonStyle()).Text(LOCTEXT("UseSensorInterval", "센서 주기 사용")).OnClicked_Lambda([DeviceOwner]() { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) { Self->UseSelectedSensorCaptureInterval(); return FReply::Handled(); }); }) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
		[
			SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot()[ SNew(SCheckBox).IsChecked_Lambda([DeviceOwner]() { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) { return Self->CaptureSelection.bCameraImage ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }); }).OnCheckStateChanged_Lambda([DeviceOwner](ECheckBoxState State) { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) { FVirtualSensorCaptureSelection Next = Self->CaptureSelection; Next.bCameraImage = State == ECheckBoxState::Checked; Self->SetCaptureSelection(Next); }); })[ SNewDeviceTool(DeviceSelf,STextBlock).ColorAndOpacity(DeviceSelf->GetToolTextColor()).Text(LOCTEXT("CaptureCameraJpeg", "Camera JPEG")) ] ]
			+ SWrapBox::Slot()[ SNew(SCheckBox).IsChecked_Lambda([DeviceOwner]() { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) { return Self->CaptureSelection.bCameraPayload ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }); }).OnCheckStateChanged_Lambda([DeviceOwner](ECheckBoxState State) { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) { FVirtualSensorCaptureSelection Next = Self->CaptureSelection; Next.bCameraPayload = State == ECheckBoxState::Checked; Self->SetCaptureSelection(Next); }); })[ SNewDeviceTool(DeviceSelf,STextBlock).ColorAndOpacity(DeviceSelf->GetToolTextColor()).Text(LOCTEXT("CaptureCameraPayload", "Camera Payload JSON")) ] ]
			+ SWrapBox::Slot()[ SNew(SCheckBox).IsChecked_Lambda([DeviceOwner]() { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) { return Self->CaptureSelection.bLidarPayload ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }); }).OnCheckStateChanged_Lambda([DeviceOwner](ECheckBoxState State) { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) { FVirtualSensorCaptureSelection Next = Self->CaptureSelection; Next.bLidarPayload = State == ECheckBoxState::Checked; Self->SetCaptureSelection(Next); }); })[ SNewDeviceTool(DeviceSelf,STextBlock).ColorAndOpacity(DeviceSelf->GetToolTextColor()).Text(LOCTEXT("CaptureLidarPayload", "LiDAR Payload JSON")) ] ]
			+ SWrapBox::Slot()[ SNew(SCheckBox).IsChecked_Lambda([DeviceOwner]() { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) { return Self->CaptureSelection.bPointCloud ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }); }).OnCheckStateChanged_Lambda([DeviceOwner](ECheckBoxState State) { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) { FVirtualSensorCaptureSelection Next = Self->CaptureSelection; Next.bPointCloud = State == ECheckBoxState::Checked; Self->SetCaptureSelection(Next); }); })[ SNewDeviceTool(DeviceSelf,STextBlock).ColorAndOpacity(DeviceSelf->GetToolTextColor()).Text(LOCTEXT("CapturePointCloud", "Point Cloud")) ] ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
		[
			SNew(SComboBox<TSharedPtr<EVirtualSensorExportKind>>).ComboBoxStyle(&DeviceSelf->GetToolComboBoxStyle()).ContentPadding(DeviceSelf->IsWorkspaceOwned()?FMargin(FSensorOwnedPresentationStyle::Gap,6):FMargin(4,2))
			.OptionsSource(&DeviceSelf->NativeExportKindOptions)
			.OnGenerateWidget_Lambda([DeviceOwner](TSharedPtr<EVirtualSensorExportKind> Item) { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) { return SNewDeviceTool(Self,STextBlock).ColorAndOpacity(Self->GetToolTextColor()).Text(FText::FromString(Item.IsValid() ? Self->ExportKindText(*Item) : TEXT("CSV"))); }); })
			.OnSelectionChanged_Lambda([DeviceOwner](TSharedPtr<EVirtualSensorExportKind> Item, ESelectInfo::Type) { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) { if (Item.IsValid()) { FVirtualSensorCaptureSelection Next = Self->CaptureSelection; Next.PointCloudFormat = *Item; Self->SetCaptureSelection(Next); } }); })
			[ SNewDeviceTool(DeviceOwner,STextBlock).ColorAndOpacity(DeviceOwner->GetToolTextColor()).Text_Lambda([DeviceOwner]() { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) { return FText::FromString(FString::Printf(TEXT("캡처 Point Cloud 형식: %s"), *Self->ExportKindText(Self->CaptureSelection.PointCloudFormat))); }); }) ]
		]
		+ SVerticalBox::Slot().AutoHeight()[ SNewDeviceTool(DeviceSelf,STextBlock).ColorAndOpacity(DeviceSelf->GetToolMutedColor()).AutoWrapText(true).Text(LOCTEXT("CaptureHelp", "1회 캡처는 새 coherent 프레임을 비동기로 요청한 뒤 저장합니다. 시간 지정 캡처는 완료된 최신 프레임을 지정 간격마다 저장하며 Topic 스트림 주기와 독립적입니다.")) ]];

    }

#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "SensorOwnedData"
    template<class TView> static TSharedRef<SWidget> BuildOwnedFileTab(TView& View)
    {
        TWeakObjectPtr<TView> DeviceOwner(&View);
        auto* DeviceSelf=&View;

	auto Modes=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8));
	const FText Labels[]={LOCTEXT("NewFrame","새 프레임 저장"),LOCTEXT("CurrentFrame","현재 프레임 저장"),LOCTEXT("Periodic","주기 저장")};
	for(int32 I=0;I<3;++I)Modes->AddSlot()[SNewDeviceTool(DeviceSelf,SButton).Text(Labels[I]).ForegroundColor_Lambda([DeviceOwner,I](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->OwnedFileAction==I?Self->GetToolAccentColor():DeviceOwner->GetToolTextColor();}); }).OnClicked_Lambda([DeviceOwner,I](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->SetActiveTab(I==1?EVirtualSensorCaptureExportTab::Export:EVirtualSensorCaptureExportTab::Capture);Self->OwnedFileAction=I;Self->LastNativeStatusRefreshSeconds=-1;return FReply::Handled();}); })];
	auto IsCamera=[DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {auto* A=Self->ResolveOwnedSelectedActor();return A&&A->GetSensorKind()==EVirtualSensorKind::Camera;}); };
	auto Options=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(16,8));
	auto Output=[DeviceOwner,Options,IsCamera](bool Camera,bool FVirtualSensorCaptureSelection::* Field,FText Name)
	{ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Options->AddSlot()[SNew(SCheckBox).Visibility_Lambda([DeviceOwner,IsCamera,Camera](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return IsCamera()==Camera?EVisibility::Visible:EVisibility::Collapsed;}); })
	 .IsChecked_Lambda([DeviceOwner,Field](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->CaptureSelection.*Field?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}); })
	 .OnCheckStateChanged_Lambda([DeviceOwner,Field](ECheckBoxState S){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {auto C=Self->CaptureSelection;C.*Field=S==ECheckBoxState::Checked;Self->SetCaptureSelection(C);}); })[SNewDeviceTool(Self,STextBlock).Text(Name)]];}); };
	Output(true,&FVirtualSensorCaptureSelection::bCameraImage,LOCTEXT("Jpeg","JPEG 이미지"));Output(true,&FVirtualSensorCaptureSelection::bCameraPayload,LOCTEXT("CameraJson","Camera JSON"));
	Output(false,&FVirtualSensorCaptureSelection::bPointCloud,LOCTEXT("PointCloud","포인트 클라우드"));Output(false,&FVirtualSensorCaptureSelection::bLidarPayload,LOCTEXT("LidarJson","LiDAR JSON"));
	return SNew(SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
	 +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[Modes]
	 +SVerticalBox::Slot().AutoHeight()[SNewDeviceTool(DeviceOwner,STextBlock).ColorAndOpacity(DeviceOwner->GetToolMutedColor()).AutoWrapText(true).Text_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->OwnedFileAction==0?LOCTEXT("NewHelp","선택 장비의 새 프레임을 비동기로 기다립니다. 요청 후 장비를 바꿔도 저장 대상은 유지됩니다."):DeviceOwner->OwnedFileAction==1?LOCTEXT("CurrentHelp","마지막으로 완료된 프레임을 저장합니다. 추가 측정을 하지 않습니다."):LOCTEXT("PeriodicHelp","시작할 때의 장비·출력·간격으로 최신 완료 프레임을 저장합니다. 서버 전송 간격과는 별개입니다.");}); })]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[Options]
	 +SVerticalBox::Slot().AutoHeight()[SNew(SComboBox<TSharedPtr<EVirtualSensorExportKind>>).ComboBoxStyle(&FSensorOwnedPresentationStyle::ComboBox()).Visibility_Lambda([DeviceOwner,IsCamera](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return !IsCamera()&&Self->CaptureSelection.bPointCloud?EVisibility::Visible:EVisibility::Collapsed;}); }).OptionsSource(&DeviceSelf->NativeExportKindOptions)
	  .OnGenerateWidget_Lambda([DeviceOwner](TSharedPtr<EVirtualSensorExportKind> K){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return SNewDeviceTool(Self,STextBlock).Text(FText::FromString(K?Self->ExportKindText(*K):FString()));}); })
	  .OnSelectionChanged_Lambda([DeviceOwner](TSharedPtr<EVirtualSensorExportKind> K,ESelectInfo::Type){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {if(!K)return;if(Self->OwnedFileAction==1)Self->SetSelectedPointCloudExportKind(*K);else{auto C=Self->CaptureSelection;C.PointCloudFormat=*K;Self->SetCaptureSelection(C);}}); })
	  [SNewDeviceTool(DeviceOwner,STextBlock).Text_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return FText::FromString(TEXT("파일 형식: ")+Self->ExportKindText(Self->ResolveOwnedFileSelection().PointCloudFormat));}); })]]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SVerticalBox).Visibility_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->OwnedFileAction==2?EVisibility::Visible:EVisibility::Collapsed;}); })
	  +SVerticalBox::Slot().AutoHeight()[SNewDeviceTool(DeviceSelf,STextBlock).Text(LOCTEXT("Interval","저장 간격 (초)"))]
	  +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[SNewDeviceTool(DeviceOwner,SEditableTextBox).Text_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return FText::AsNumber(Self->CaptureSelection.IntervalSeconds);}); }).OnTextCommitted_Lambda([DeviceOwner](const FText& T,ETextCommit::Type){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->SetCaptureIntervalSeconds(FCString::Atof(*T.ToString()));}); })]+SHorizontalBox::Slot().AutoWidth().Padding(8,0,0,0)[SNewDeviceTool(DeviceOwner,SButton).Text(LOCTEXT("SensorInterval","센서 주기 사용")).OnClicked_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->UseSelectedSensorCaptureInterval();return FReply::Handled();}); })]]]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNewDeviceTool(DeviceOwner,SButton).ButtonStyle(&FSensorOwnedPresentationStyle::PrimaryButton()).Text_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->OwnedFileAction==2?(Self->bOwnedPeriodicActive?LOCTEXT("StopPeriodic","주기 저장 중지"):LOCTEXT("StartPeriodic","주기 저장 시작")):DeviceOwner->bOwnedSaveBusy?LOCTEXT("Busy","파일 처리 중…"):LOCTEXT("SaveNow","파일 저장");}); })
	  .IsEnabled_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->OwnedFileAction==2?(Self->bOwnedPeriodicActive||Self->ResolveOwnedSelectedActor()!=nullptr):DeviceOwner->ResolveOwnedSelectedActor()&&!Self->bOwnedSaveBusy&&(Self->OwnedFileAction==0||Self->bOwnedCanSaveCurrent);}); })
	  .OnClicked_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {if(Self->OwnedFileAction==2)Self->ToggleOwnedPeriodicSave();else DeviceOwner->RequestFileSave(Self->OwnedFileAction==0?EVirtualSensorFileSaveMode::NewFrame:EVirtualSensorFileSaveMode::CurrentFrame);return FReply::Handled();}); })]
	 +SVerticalBox::Slot().AutoHeight()[SNewDeviceTool(DeviceOwner,STextBlock).AutoWrapText(true).ColorAndOpacity(DeviceOwner->GetToolMutedColor()).Text_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {if(Self->OwnedFileAction==2)return FText::FromString(DeviceOwner->OwnedPeriodicStatusText);if(Self->OwnedFileAction==1&&!Self->bOwnedCanSaveCurrent)return LOCTEXT("NoFrame","완료된 프레임이 없습니다. 측정을 시작하거나 새 프레임 저장을 선택하세요.");if(DeviceOwner->OwnedFileRequestId.IsEmpty())return SensorToolUiText::FileSaveWaiting();return FText::FromString(DeviceOwner->OwnedFileStatusText);}); })]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,16)[DeviceSelf->BuildDeviceFileExtension()]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SExpandableArea).InitiallyCollapsed(true).HeaderContent()[SNewDeviceTool(DeviceSelf,STextBlock).Text(LOCTEXT("FileTools","파일 전송 도구"))].BodyContent()[SNewDeviceTool(DeviceOwner,SButton).Text(LOCTEXT("SendRecent","최근 파일 수동 전송")).OnClicked_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->SendLastExportToServer();return FReply::Handled();}); })]]
	];

    }
};
#undef LOCTEXT_NAMESPACE
