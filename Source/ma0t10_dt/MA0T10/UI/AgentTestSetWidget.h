#pragma once
#include "CoreMinimal.h"
#include "VirtualSensorPanelWidgetBase.h"
#include "AgentTestSetWidget.generated.h"
class UAgentTestSetSubsystem;
UCLASS()
class MA0T10_DT_API UAgentTestSetWidget : public UVirtualSensorPanelWidgetBase
{
    GENERATED_BODY()
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& Geometry,float Delta) override;
    virtual void NativeDestruct() override;
private:
    UAgentTestSetSubsystem* Service() const;
    FSlateFontInfo Font(int32 Size) const;
    void Refresh();
    void SelectScenario(int32 Index);
    TSharedPtr<class SVerticalBox> List;
    TArray<TSharedPtr<class STextBlock>> Rows;
    TArray<FString> RowMessages;
    TArray<FString> RowIds;
    TArray<uint8> RowStates;
    FSlateBrush PreviewBrush;
    FText Summary,DetailText,Status,PreviewText,Timing;
    float Progress=0;
    int32 Selected=INDEX_NONE;
    int64 SeenRevision=-1;
    double NextRefresh=0,ConfirmUntil=0;
    bool Ready=false,Details=false,PreviewVisible=false;
};
