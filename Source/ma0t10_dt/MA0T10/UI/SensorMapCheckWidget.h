#pragma once
#include "CoreMinimal.h"
#include "UI/DxWidget.h"
#include "SensorMapCheckWidget.generated.h"
class UDataTable;
struct FSensorMapCheckRow { FString Key, Status, Remedy; bool bOk=false; FText LabelText,RemedyText; };

/** Read-only map diagnostics, managed by the existing DTCore widget subsystem. No saved UI role. */
UCLASS()
class MA0T10_DT_API USensorMapCheckWidget : public UDxWidget
{
    GENERATED_BODY()
public:
    void RefreshChecks();
    const TArray<FSensorMapCheckRow>& GetRows() const { return Rows; }
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& Geometry,float Delta) override;
    virtual void NativeDestruct() override;
private:
    TArray<FSensorMapCheckRow> Rows;
    double NextRefresh=0;
    bool bTablesLoaded=false;
    FSlateFontInfo MapFont(int32 Size) const;
    UPROPERTY(Transient) TObjectPtr<UDataTable> TransactionTable;
    UPROPERTY(Transient) TObjectPtr<UDataTable> LevelTable;
};
