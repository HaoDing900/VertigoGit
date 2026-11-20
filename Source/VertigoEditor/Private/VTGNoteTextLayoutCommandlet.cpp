#include "VTGNoteTextLayoutCommandlet.h"
#include "UI/VTGNoteTextWidget.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "Engine/World.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/WidgetSwitcher.h"
#include "Components/ScrollBox.h"
#include "UObject/UnrealType.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"

namespace
{
bool TestLayout(UBlueprint* BP)
{
    if (!FSlateApplication::IsInitialized())
        FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    auto* First = CreateWidget<UVTGNoteTextWidget>(World, TSubclassOf<UUserWidget>(BP->GeneratedClass.Get()));
    auto* Second = CreateWidget<UVTGNoteTextWidget>(World, TSubclassOf<UUserWidget>(BP->GeneratedClass.Get()));
    bool OK = First && Second;
    if (OK)
    {
        First->TakeWidget(); Second->TakeWidget();
        auto* Switcher = NewObject<UWidgetSwitcher>(World);
        Switcher->AddChild(First); Switcher->AddChild(Second);
        auto* A = Cast<UTextBlock>(First->GetWidgetFromName(TEXT("Text")));
        auto* B = Cast<UTextBlock>(Second->GetWidgetFromName(TEXT("Text")));
        for (auto* Page : {First, Second})
        {
            FindFProperty<FIntProperty>(Page->GetClass(), TEXT("FontSize"))->SetPropertyValue_InContainer(Page, 20);
            auto* Box = CastChecked<USizeBox>(Page->GetWidgetFromName(TEXT("SizeBox_22")));
            Box->SetHeightOverride(250.f); Box->SetWidthOverride(650.f);
        }
        A->SetText(FText::FromString(TEXT("Short note.")));
        B->SetText(FText::FromString(TEXT("中文短文。")));
        First->FitDocument();
        OK &= A->GetFont().Size == 20 && B->GetFont().Size == 20;
        FString Long;
        for (int32 I = 0; I < 100; ++I) Long += TEXT("A much longer translated note paragraph.\n");
        B->SetText(FText::FromString(Long));
        First->FitDocument();
        OK &= A->GetFont().Size == 16 && B->GetFont().Size == 16;
        OK &= Cast<UScrollBox>(B->GetParent()) != nullptr && B->GetText().ToString() == Long;
        B->SetText(FText::FromString(TEXT("Short again.")));
        Second->FitDocument();
        OK &= A->GetFont().Size == 20 && B->GetFont().Size == 20;
        OK &= Cast<USizeBox>(B->GetParent()) != nullptr;
        // A moderate overflow must choose an intermediate size, rather than always subtract four.
        B->SetText(FText::FromString(TEXT("Line one\nLine two\nLine three\nLine four\nLine five\nLine six")));
        bool FoundIntermediate = false;
        auto* Box = CastChecked<USizeBox>(Second->GetWidgetFromName(TEXT("SizeBox_22")));
        for (int32 Height = 100; Height <= 350 && !FoundIntermediate; Height += 2)
        {
            Box->SetHeightOverride(float(Height));
            First->FitDocument();
            FoundIntermediate = B->GetFont().Size > 16 && B->GetFont().Size < 20 && !Cast<UScrollBox>(B->GetParent());
        }
        OK &= FoundIntermediate;
        OK &= A->GetFont().Size == B->GetFont().Size;
    }
    World->DestroyWorld(false);
    UE_LOG(LogTemp, Display, TEXT("NOTE TEXT LAYOUT: %s (short text, shared size, four-point limit, overflow access, reset)"), OK ? TEXT("PASS") : TEXT("FAIL"));
    return OK;
}
}

UVTGNoteTextLayoutCommandlet::UVTGNoteTextLayoutCommandlet()
{
    IsEditor = true;
    IsClient = IsServer = false;
    LogToConsole = true;
}
int32 UVTGNoteTextLayoutCommandlet::Main(const FString& Params)
{
    auto* BP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Inventory/InventoryXBased/UMG/WB_TextContainer.WB_TextContainer"));
    if (!BP) return 1;
    if (FParse::Param(*Params, TEXT("Test"))) return BP->ParentClass == UVTGNoteTextWidget::StaticClass() && TestLayout(BP) ? 0 : 7;
    if (FParse::Param(*Params, TEXT("VerifyOnly")))
        return BP->ParentClass == UVTGNoteTextWidget::StaticClass() ? 0 : 2;
    if (BP->ParentClass == UVTGNoteTextWidget::StaticClass()) return 0;
    if (BP->ParentClass != UUserWidget::StaticClass()) return 3;
    const FString File = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    const FString Backup = FPaths::ProjectSavedDir()/TEXT("Backups/NoteTextLayout/WB_TextContainer.uasset");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
    if (!IFileManager::Get().FileExists(*Backup) && IFileManager::Get().Copy(*Backup, *File) != COPY_OK) return 4;
    BP->ParentClass = UVTGNoteTextWidget::StaticClass();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Results);
    if (Results.NumErrors) return 5;
    FSavePackageArgs Save;
    Save.TopLevelFlags = RF_Public | RF_Standalone;
    Save.SaveFlags = SAVE_NoError;
    return UPackage::SavePackage(BP->GetOutermost(), BP, *File, Save) ? 0 : 6;
}

