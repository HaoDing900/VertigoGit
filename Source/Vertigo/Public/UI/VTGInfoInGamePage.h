#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/DataTable.h"
#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"
#include "VTGInfoInGamePage.generated.h"

class UHorizontalBox;
class UImage;
class UInputAction;
class UPanelWidget;
class UTextBlock;
class UVTGLocalizationSubsystem;

/**
 * One in-game info page. The row NAME is the page id a level asks for ("Combat"), so a new page is a
 * new row plus its lines - no new widget asset and no C++.
 */
USTRUCT(BlueprintType)
struct VERTIGO_API FVTGInfoPageRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 面包屑 / The small line above the title, e.g. INFO / COMBAT. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info Page", meta = (DisplayName = "面包屑 / Breadcrumb"))
	FText Breadcrumb;

	/** 标题 / Page title. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info Page", meta = (DisplayName = "标题 / Title"))
	FText Title;

	/** 正文 / Optional paragraph above the key list. Left empty, its line is hidden. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info Page", meta = (MultiLine = true, DisplayName = "正文 / Body"))
	FText Body;

	/** 关闭按钮文字 / Label on the button that closes the page. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info Page", meta = (DisplayName = "关闭按钮 / Dismiss Label"))
	FText DismissLabel;
};

/**
 * One "key - what it does" line on an info page.
 *
 * Action is preferred over KeyOverride: it reads the player's actual binding and the current input
 * device, so the prompt follows a rebind and turns into the gamepad glyph by itself.
 */
USTRUCT(BlueprintType)
struct VERTIGO_API FVTGInfoPageLineRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 属于哪一页 / Which page row this line belongs to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info Page Line", meta = (DisplayName = "所属页面 / Page"))
	FName Page;

	/** 排序 / Lines are shown in ascending order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info Page Line", meta = (DisplayName = "排序 / Order"))
	int32 Order = 0;

	/** 输入动作 / The action whose current binding this line shows. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info Page Line",
		meta = (AllowedClasses = "/Script/EnhancedInput.InputAction", DisplayName = "输入动作 / Action"))
	TSoftObjectPtr<UInputAction> Action;

	/** 固定按键 / Used when Action is empty, for things with no Input Action of their own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info Page Line", meta = (DisplayName = "固定按键 / Key Override"))
	FKey KeyOverride;

	/** 说明 / What the key does. Translated through the manual localization panel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info Page Line", meta = (DisplayName = "说明 / Label"))
	FText Label;
};

/** Where the two info-page tables live. */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "VTG Info Pages"))
class VERTIGO_API UVTGInfoPageSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Vertigo"); }

	/** 页面表。行名就是 page id，例如 Combat */
	UPROPERTY(config, EditAnywhere, Category = "Info Pages",
		meta = (AllowedClasses = "/Script/Engine.DataTable", DisplayName = "页面表 / Info Pages"))
	TSoftObjectPtr<UDataTable> Pages;

	/** 按键提示表。每行一条提示，用 Page 列挂到页面上 */
	UPROPERTY(config, EditAnywhere, Category = "Info Pages",
		meta = (AllowedClasses = "/Script/Engine.DataTable", DisplayName = "按键提示表 / Info Page Lines"))
	TSoftObjectPtr<UDataTable> Lines;
};

/**
 * The reusable in-game info page (WBP_InfoInGame).
 *
 * The layout lives in the widget asset, laid out on the pause menu's sub-page grid so every page in
 * the game reads the same. This class only fills it: the wording comes from the page tables, and the
 * key prompts are resolved the way WBP_Action's are - the platform's input brush when there is one,
 * the key's short name when there is not.
 *
 * Which page is shown comes from InfoPageId, so one asset serves every page: set it on a child
 * widget blueprint, or call Show() with the id.
 */
UCLASS()
class VERTIGO_API UVTGInfoInGamePage : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Row name in the page table. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Info Page")
	FName InfoPageId = TEXT("Combat");

	/** Swaps the page in place; safe before or after the widget is constructed. */
	UFUNCTION(BlueprintCallable, Category = "Info Page")
	void SetInfoPageId(FName PageId);

	/** Creates WBP_InfoInGame showing PageId and adds it to the viewport. */
	UFUNCTION(BlueprintCallable, Category = "Info Page", meta = (WorldContext = "WorldContextObject"))
	static UVTGInfoInGamePage* ShowInfoPage(const UObject* WorldContextObject, FName PageId, int32 ZOrder = 0);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void Refresh();
	void BuildLines();

	UFUNCTION()
	void HandleLanguageChanged(const FString& Language);

	/** The prompt for one line: the platform brush if it has one, otherwise the key's short name. */
	void ApplyKeyPrompt(UImage* Icon, UTextBlock* Text, const FVTGInfoPageLineRow& Line) const;
	FKey ResolveKey(const FVTGInfoPageLineRow& Line) const;

	UPROPERTY(Transient)
	TObjectPtr<UVTGLocalizationSubsystem> Languages;

	/** Lines are rebuilt from the table, so the ones from a previous refresh are dropped first. */
	UPROPERTY(Transient)
	TObjectPtr<UPanelWidget> LineBox;
};
