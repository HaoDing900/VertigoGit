#include "VTGQuestDisplayRepairCommandlet.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/RichTextBlock.h"
#include "Engine/DataTable.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

int32 UVTGQuestDisplayRepairCommandlet::Main(const FString& Params)
{
    const TCHAR* WidgetPath = TEXT("/Game/Narrative/NarrativeUI/Widgets/BP_QuestTask");
    const TCHAR* StylePath = TEXT("/Game/Narrative/NarrativeUI/Assets/DT_RichText_NarrativeQuestUpdate");
    const TCHAR* TaskStylePath = TEXT("/Game/Narrative/NarrativeUI/Assets/DT_RichText_QuestTask");
    auto* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, WidgetPath);
    auto* Source = LoadObject<UDataTable>(nullptr, StylePath);
    auto* Text = Blueprint && Blueprint->WidgetTree ? Cast<URichTextBlock>(Blueprint->WidgetTree->FindWidget(TEXT("TaskDesc"))) : nullptr;
    if (!Text || !Source || Source->GetRowStruct() != FRichTextStyleRow::StaticStruct()) return 1;
    const bool bVerify = Params.Contains(TEXT("VerifyOnly"));
    auto* Target = LoadObject<UDataTable>(nullptr, TaskStylePath);
    if (!Target && !bVerify)
    {
        auto* Package = CreatePackage(TaskStylePath);
        Target = DuplicateObject<UDataTable>(Source, Package, TEXT("DT_RichText_QuestTask"));
        Target->SetFlags(RF_Public | RF_Standalone);
    }
    if (!Target) return 2;
    if (!bVerify)
    {
        Target->Modify();
        Target->EmptyTable();
        for (const auto& Pair : Source->GetRowMap())
        {
            auto Row = *reinterpret_cast<const FRichTextStyleRow*>(Pair.Value);
            Row.TextStyle.Font.Size += 4;
            Target->AddRow(Pair.Key, Row);
        }
        Blueprint->Modify();
        Text->Modify();
        Text->SetTextStyleSet(Target);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        if (Blueprint->Status == BS_Error) return 3;
        for (UObject* Asset : {static_cast<UObject*>(Target), static_cast<UObject*>(Blueprint)})
        {
            auto* Package = Asset->GetOutermost();
            Package->MarkPackageDirty();
            FSavePackageArgs SaveArgs;
            SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
            const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
            if (!UPackage::SavePackage(Package, Asset, *Filename, SaveArgs)) return 4;
        }
    }
    if (Text->GetTextStyleSet() != Target) return 5;
    for (const auto& Pair : Source->GetRowMap())
    {
        auto* Row = Target->FindRow<FRichTextStyleRow>(Pair.Key, TEXT("Font verification"));
        auto Expected = *reinterpret_cast<const FRichTextStyleRow*>(Pair.Value);
        const float OldSize = Expected.TextStyle.Font.Size;
        Expected.TextStyle.Font.Size += 4;
        if (!Row || !FRichTextStyleRow::StaticStruct()->CompareScriptStruct(Row, &Expected, 0)) return 6;
        UE_LOG(LogTemp, Display, TEXT("QuestTask font %s: %.0f -> %.0f"), *Pair.Key.ToString(), OldSize, Row->TextStyle.Font.Size);
    }
    UE_LOG(LogTemp, Display, TEXT("QuestTask styles verified (+4); shared overlay styles unchanged."));
    return 0;
}
