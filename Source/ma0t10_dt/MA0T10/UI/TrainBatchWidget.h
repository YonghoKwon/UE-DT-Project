#pragma once
#include "CoreMinimal.h"
#include "UI/DxWidget.h"
#include "Styling/SlateBrush.h"
#include "ma0t10_dt/MA0T10/Scenario/Training/TrainBatchDelivery.h"
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
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    friend class FTrainBatchPartialUiDeliveryTest;
#endif
    void Refresh();
    void ApplyTargets();
    /** Archive format, SFTP server and the explicit send button. */
    TSharedRef<SWidget> BuildDeliverySection();
    FTrainDeliverySettings ReadDeliveryFields() const;
    void SaveDelivery();
    void StartDelivery(bool bUpload);
    void FetchFingerprint();
    FText DeliveryStatusText() const;
    TSharedRef<ITableRow> GenerateRow(TSharedPtr<FTrainBatchListRow> Row,const TSharedRef<STableViewBase>& Owner);
    TSharedPtr<SEditableTextBox> ManifestPath,Preset,SlabName,LidarId,CameraId,RestoreId;
    int32 WorkerCount=1;
    TSharedPtr<SListView<TSharedPtr<FTrainBatchListRow>>> List;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> JobPicker;
    TArray<TSharedPtr<FTrainBatchListRow>> Rows;
    TArray<TSharedPtr<FString>> JobIds;
    FString SelectedJob;
    FText Summary,Feedback,ViewText;
    FSlateBrush PreviewBrush;
    double NextRefresh=0;
    int64 SeenRevision=-1;
    TSharedPtr<SEditableTextBox> Host,Port,User,RemoteDir,KeyPath,Level;
    ETrainArchiveFormat Format=ETrainArchiveFormat::Zip;
    bool bSendSuccessOnly=false;
    TSharedPtr<FTrainBatchDelivery,ESPMode::ThreadSafe> Delivery;
    /** Host key fetched for confirmation; trusted only after the user presses "키 등록". */
    FString PendingFingerprint,PendingKnownHosts;
    bool bFetchingFingerprint=false;
    FText DeliveryFeedback;
};
