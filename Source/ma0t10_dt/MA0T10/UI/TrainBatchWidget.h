#pragma once
#include "CoreMinimal.h"
#include "UI/DxWidget.h"
#include "Styling/SlateBrush.h"
#include "TrainBatchWidget.generated.h"

class SEditableTextBox;
class STableViewBase;
class ITableRow;
template<typename T> class SListView;
template<typename T> class SComboBox;
struct FTrainBatchListRow
{
    int32 Index=0;
    FText Text;
};
/** Native skeleton; owned/closed/fronted by DTCore, separate from normal replay and TESTSET. */
UCLASS()
class MA0T10_DT_API UTrainBatchWidget : public UDxWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Training",meta=(WorldContext="WorldContextObject"))
    static UTrainBatchWidget* OpenTrainingPanel(UObject* WorldContextObject);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& Geometry,float DeltaTime) override;
    virtual void NativeDestruct() override;
private:
    void Refresh();
    void ApplyTargets();
    TSharedRef<ITableRow> GenerateRow(TSharedPtr<FTrainBatchListRow> Row,const TSharedRef<STableViewBase>& Owner);
    TSharedPtr<SEditableTextBox> ManifestPath,Preset,SlabName,LidarId,CameraId;
    TSharedPtr<SListView<TSharedPtr<FTrainBatchListRow>>> List;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> JobPicker;
    TArray<TSharedPtr<FTrainBatchListRow>> Rows;
    TArray<TSharedPtr<FString>> JobIds;
    FString SelectedJob;
    FText Summary,Feedback,ViewText;
    FSlateBrush PreviewBrush;
    double NextRefresh=0;
    int64 SeenRevision=-1;
};
