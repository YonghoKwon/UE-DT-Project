#pragma once
#include "CoreMinimal.h"
#include "UI/DxWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "AgentTestSetWidget.generated.h"
class UAgentTestSetSubsystem;
UCLASS()
class MA0T10_DT_API UAgentTestSetWidget : public UDxWidget
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
    TSharedPtr<class SVerticalBox> List;
    TSharedPtr<SComboBox<TSharedPtr<int32>>> Select;
    TArray<TSharedPtr<int32>> Options;
    FText Summary,ConfigText,Status,SelectedText;
    float Progress=0;
    int64 SeenRevision=-1;
    double NextRefresh=0,ConfirmUntil=0;
    bool Ready=false;
};
