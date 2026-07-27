#include "TextureBatchTools.h"

#include "AssetRegistry/AssetData.h"
#include "ContentBrowserDelegates.h"
#include "ContentBrowserModule.h"
#include "Engine/Texture.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/ScopedSlowTask.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "FTextureBatchToolsModule"

DEFINE_LOG_CATEGORY_STATIC(LogTextureBatch, Log, All);

namespace TextureBatchToolsImpl
{
	/** The sizes offered in the submenu. 0 == no limit / platform max. */
	static const int32 GOfferedSizes[] = { 0, 8192, 4096, 2048, 1024, 512, 256, 128, 64 };

	static FText SizeToText(int32 Size)
	{
		return (Size == 0)
			? LOCTEXT("NoLimit", "No Limit (platform max)")
			: FText::FromString(FString::FromInt(Size));
	}

	static bool AnyTextureSelected(const TArray<FAssetData>& Assets)
	{
		for (const FAssetData& Asset : Assets)
		{
			if (Asset.IsInstanceOf(UTexture::StaticClass(), EResolveClass::Yes))
			{
				return true;
			}
		}
		return false;
	}

	/** Set MaxTextureSize on every selected texture and rebuild it. */
	static void ApplyMaxTextureSize(TArray<FAssetData> Assets, int32 MaxSize)
	{
		int32 Changed = 0;

		FScopedSlowTask SlowTask(static_cast<float>(Assets.Num()),
			LOCTEXT("Working", "Setting Maximum Texture Size..."));
		SlowTask.MakeDialog();

		for (const FAssetData& AssetData : Assets)
		{
			SlowTask.EnterProgressFrame();

			UTexture* Texture = Cast<UTexture>(AssetData.GetAsset()); // loads if needed
			if (!Texture)
			{
				continue;
			}

			Texture->Modify();
			Texture->PreEditChange(nullptr);
			Texture->MaxTextureSize = MaxSize;
			Texture->PostEditChange();   // rebuilds platform data with the new cap
			Texture->MarkPackageDirty(); // mark as needing a save
			++Changed;
		}

		UE_LOG(LogTextureBatch, Log, TEXT("Set MaxTextureSize=%d on %d texture(s)."), MaxSize, Changed);

		FNotificationInfo Info(FText::Format(
			LOCTEXT("Done", "Maximum Texture Size = {0}\nApplied to {1} texture(s). Save to keep the changes."),
			SizeToText(MaxSize), FText::AsNumber(Changed)));
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	static void BuildSizeSubMenu(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
	{
		for (int32 Size : GOfferedSizes)
		{
			const FText Label = SizeToText(Size);
			MenuBuilder.AddMenuEntry(
				Label,
				FText::Format(LOCTEXT("SetToTip", "Set Maximum Texture Size to {0}"), Label),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([SelectedAssets, Size]()
				{
					ApplyMaxTextureSize(SelectedAssets, Size);
				}))
			);
		}
	}

	static TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
	{
		TSharedRef<FExtender> Extender = MakeShared<FExtender>();

		if (AnyTextureSelected(SelectedAssets))
		{
			Extender->AddMenuExtension(
				"CommonAssetActions",
				EExtensionHook::After,
				nullptr,
				FMenuExtensionDelegate::CreateLambda([SelectedAssets](FMenuBuilder& MenuBuilder)
				{
					MenuBuilder.BeginSection("TextureBatchTools", LOCTEXT("Section", "Texture Batch Tools"));
					MenuBuilder.AddSubMenu(
						LOCTEXT("SetMaxSize", "Set Maximum Texture Size"),
						LOCTEXT("SetMaxSizeTip", "Set Maximum Texture Size on every selected texture"),
						FNewMenuDelegate::CreateLambda([SelectedAssets](FMenuBuilder& SubMenuBuilder)
						{
							BuildSizeSubMenu(SubMenuBuilder, SelectedAssets);
						})
					);
					MenuBuilder.EndSection();
				})
			);
		}

		return Extender;
	}
}

void FTextureBatchToolsModule::StartupModule()
{
	FContentBrowserModule& ContentBrowserModule =
		FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	TArray<FContentBrowserMenuExtender_SelectedAssets>& Extenders =
		ContentBrowserModule.GetAllAssetViewContextMenuExtenders();

	Extenders.Add(FContentBrowserMenuExtender_SelectedAssets::CreateStatic(
		&TextureBatchToolsImpl::OnExtendContentBrowserAssetSelectionMenu));

	ContentBrowserExtenderHandle = Extenders.Last().GetHandle();
}

void FTextureBatchToolsModule::ShutdownModule()
{
	if (FContentBrowserModule* ContentBrowserModule =
			FModuleManager::GetModulePtr<FContentBrowserModule>(TEXT("ContentBrowser")))
	{
		ContentBrowserModule->GetAllAssetViewContextMenuExtenders().RemoveAll(
			[this](const FContentBrowserMenuExtender_SelectedAssets& Delegate)
			{
				return Delegate.GetHandle() == ContentBrowserExtenderHandle;
			});
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTextureBatchToolsModule, TextureBatchTools)
