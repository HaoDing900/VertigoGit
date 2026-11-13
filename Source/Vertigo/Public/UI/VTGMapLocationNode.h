#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "VTGMapLocationNode.generated.h"

class UTextBlock;
class UButton;
class UBorder;

UCLASS()
class VERTIGO_API UVTGMapLocationTooltip : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetLocation(const FText& Name, const FText& Description, const FSlateFontInfo& Font);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    FText Title, Body;
    FSlateFontInfo TextFont;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleLabel;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> BodyLabel;
};

/** Display text is separate from MyNodeData's location IDs and interaction data. */
UCLASS()
class VERTIGO_API UVTGMapLocationNode : public UUserWidget
{
    GENERATED_BODY()
    friend struct FVTGMapPresentationRegression;
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map Location", meta=(DisplayName="地点名称 / Location Name", ExposeOnSpawn="true"))
    FText LocationDisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map Location", meta=(DisplayName="地点介绍 / Location Description", MultiLine="true", ExposeOnSpawn="true"))
    FText LocationDescription;
    UFUNCTION(BlueprintCallable, Category="Map Location")
    void RefreshLocationText();
protected:
    virtual void NativePreConstruct() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
private:
    bool IsNodeSelected() const;
    void BuildLocationPresentation();
    void RefreshDescriptionPanel();
    UFUNCTION() void NodePressed();
    UFUNCTION() void NodeClicked();
    UFUNCTION() void NameClicked();
    UPROPERTY(Transient) TObjectPtr<UButton> LocationNameButton;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> LocationNameLabel;
    UPROPERTY(Transient) TObjectPtr<UBorder> DescriptionPanel;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DescriptionLabel;
    bool ShowDescription=false;
    bool CarryDescription=false;
};
