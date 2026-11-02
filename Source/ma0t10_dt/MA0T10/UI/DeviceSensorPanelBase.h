#pragma once
#include "CoreMinimal.h"
#include "UI/DxWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "DeviceSensorPanelTypes.h"
#include "DevicePanelHostBridge.h"
#include "DeviceSensorPanelBase.generated.h"

/** Common execution state only. Public legacy properties/pins and SaveGame policy live on the facade. */
UCLASS(Abstract)
class MA0T10_DT_API UDeviceSensorPanelBase : public UDxWidget
{
    GENERATED_BODY()
public:
    virtual bool SupportsOwnedDocking() const {return false;}
    virtual FLinearColor GetToolTextColor() const {return FLinearColor::White;}
    virtual FLinearColor GetToolMutedColor() const {return FLinearColor(.6f,.6f,.6f,1.f);}
    virtual const FButtonStyle& GetToolButtonStyle(bool bDanger=false) const;
    virtual const FEditableTextBoxStyle& GetToolInputStyle() const;
    void BindDevicePanelHost(UObject* Owner,TSharedPtr<IDevicePanelHostBridge> Bridge);
    const IDevicePanelHostBridge* GetDevicePanelHost() const { return DeviceHostOwner.IsValid()?DeviceHostBridge.Get():nullptr; }
    bool IsDeviceHostOwned() const { const auto* Host=GetDevicePanelHost();return Host&&Host->IsOwned(); }
    virtual void RegisterSensorNativeFont(TSharedRef<STextBlock>,const STextBlock::FArguments&);
    virtual void RegisterSensorNativeFont(TSharedRef<SButton>,const SButton::FArguments&);
    virtual void RegisterSensorNativeFont(TSharedRef<SEditableTextBox>,const SEditableTextBox::FArguments&);
    virtual float GetSensorToolVisualScale() const {return SensorToolFontScale;}
    void DeviceApplyFontScale(float Scale);
    void DeviceRegisterTextControl(class UTextBlock* Text);
    void DeviceRegisterInputControl(class UEditableTextBox* Input);
    static int32 DeviceScaledFontSize(int32 BaseSize,float Scale);
    static FVector2D DeviceClampPosition(FVector2D Position,FVector2D PanelSize,FVector2D ViewportSize,float Margin);
    static FVector2D DeviceDraggedPosition(FVector2D CurrentPosition,FVector2D CursorDelta,float DpiScale);
    static FVector2D DeviceResizedSize(FVector2D CurrentSize,FVector2D CursorDelta,float DpiScale,FVector2D MinimumSize,FVector2D MaximumSize);
protected:
    virtual bool HasDeviceAppearanceOwner() const {return IsDeviceHostOwned();}
    virtual void DeviceFontLayoutChanged() {}
    virtual void DeviceFontScaleChanged(float) {}
    void DevicePruneFontRegistrations();
    void DeviceAddFontRegistration(TFunction<void(float)> Setter,TFunction<bool()> IsAlive);
    virtual void NativeDestruct() override;
    bool bWorkspaceDocked=false;
    bool bPendingOwnedFontRefresh=false;
    bool bFloatingDraggable=true,bFloatingResizable=false,bFloatingClamp=true;
	bool bSensorPanelConstructed = false;
	TArray<TFunction<void(float)>> SensorFontSetters;
	TArray<TFunction<bool()>> SensorFontLifetimes;
	float SensorToolFontScale = 1.0f;
	TSet<TWeakObjectPtr<UWidget>> RegisteredSensorUmg;
    uint8 ResizeEdges=0;
    FVector2D ResizeStartScreen=FVector2D::ZeroVector,ResizeStartPosition=FVector2D::ZeroVector,ResizeStartSize=FVector2D::ZeroVector;
    float ResizeStartScale=1.f;
    bool bOwnedPointerFocus=false,bPreviousFocusable=false;
    int32 PanelPointerUser=0;
    EVirtualSensorPanelPlacement DefaultPlacement = EVirtualSensorPanelPlacement::RightCenter;
    FVector2D RequestedPanelSize = FVector2D(820.0f, 430.0f);
    FVector2D DefaultResolvedPanelSize = FVector2D(820.0f, 430.0f);
    FVector2D DesiredPanelSize = FVector2D(820.0f, 430.0f);
    FVector2D MinimumPanelSize = FVector2D(160.0f, 80.0f);
    FVector2D MaximumPanelSize = FVector2D::ZeroVector;
    FVector2D CurrentViewportPosition = FVector2D::ZeroVector;
    float ViewportMargin = 18.0f;
    float CollapsedPanelHeight = 48.0f;
    bool bDraggingPanel = false;
    bool bResizingPanel = false;
    bool bPanelCollapsed = false;
    bool bInitialLayoutPending = false;
    bool bPanelLayoutConfigured = false;
private:
    TWeakObjectPtr<UObject> DeviceHostOwner;
    TSharedPtr<IDevicePanelHostBridge> DeviceHostBridge;
};
