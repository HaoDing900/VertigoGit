#include "VTGQuestPresentationCommandlet.h"
#include "UI/VTGQuestSequenceBox.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "UObject/UnrealType.h"
#include "UObject/SavePackage.h"

UVTGQuestPresentationCommandlet::UVTGQuestPresentationCommandlet(){IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}
int32 UVTGQuestPresentationCommandlet::Main(const FString& Params)
{
    auto* BP=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/Narrative/NarrativeUI/Widgets/BP_Narrative3Overlay"));check(BP && BP->WidgetTree);
    if(FParse::Param(*Params,TEXT("Apply")) && !Cast<UVTGQuestSequenceBox>(BP->WidgetTree->FindWidget(TEXT("TaskBox"))))
    {
        FString Backup;check(FParse::Value(*Params,TEXT("Backup="),Backup));
        const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);check(!IFileManager::Get().FileExists(*Backup));
        check(IFileManager::Get().Copy(*Backup,*File,false)==COPY_OK);
        auto* Old=CastChecked<UVerticalBox>(BP->WidgetTree->FindWidget(TEXT("TaskBox")));
        auto* Parent=Old->GetParent();check(Parent);const int32 Index=Parent->GetChildIndex(Old);check(Index!=INDEX_NONE);
        auto* CanvasSlot=CastChecked<UCanvasPanelSlot>(Old->Slot);const auto Layout=CanvasSlot->GetLayout();
        const int32 Count=Old->GetChildrenCount();check(Count==3);
        auto* Dialogue=BP->WidgetTree->FindWidget(TEXT("Line_Main"));auto* Portrait=BP->WidgetTree->FindWidget(TEXT("AvtProfile_Main"));
        check(Dialogue && Portrait && !Dialogue->IsChildOf(Old) && !Portrait->IsChildOf(Old));
        BP->Modify();BP->WidgetTree->Modify();Old->Modify();Parent->Modify();
        Old->Rename(TEXT("TaskBox_BeforeSequenceGate"),BP->WidgetTree,REN_DontCreateRedirectors|REN_NonTransactional);
        auto* New=BP->WidgetTree->ConstructWidget<UVTGQuestSequenceBox>(UVTGQuestSequenceBox::StaticClass(),TEXT("TaskBox"));
        // Preserve authored UWidget properties; panel slots are moved explicitly below.
        for(TFieldIterator<FProperty> It(UVerticalBox::StaticClass());It;++It)
            if(It->GetFName()!=TEXT("Slots") && It->GetFName()!=TEXT("Slot") && !It->HasAnyPropertyFlags(CPF_Transient|CPF_DuplicateTransient|CPF_NonPIEDuplicateTransient))
                It->CopyCompleteValue_InContainer(New,Old);
        check(Parent->ReplaceChildAt(Index,New));Old->Slot=nullptr;
        while(Old->GetChildrenCount())
        {
            auto* Child=Old->GetChildAt(0);auto* Previous=CastChecked<UVerticalBoxSlot>(Child->Slot);
            const auto Padding=Previous->GetPadding();const auto Size=Previous->GetSize();const auto H=Previous->GetHorizontalAlignment();const auto V=Previous->GetVerticalAlignment();
            auto* Slot=New->AddChildToVerticalBox(Child);Slot->SetPadding(Padding);Slot->SetSize(Size);Slot->SetHorizontalAlignment(H);Slot->SetVerticalAlignment(V);
        }
        check(New->Slot==CanvasSlot && CanvasSlot->GetLayout()==Layout && New->GetChildrenCount()==Count);
        check(BP->WidgetTree->FindWidget(TEXT("Line_Main"))==Dialogue && BP->WidgetTree->FindWidget(TEXT("AvtProfile_Main"))==Portrait);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FBlueprintEditorUtils::RefreshAllNodes(BP);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipGarbageCollection,&Log);check(Log.NumErrors==0);
        FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;check(UPackage::SavePackage(BP->GetOutermost(),BP,*File,Args));
        UE_LOG(LogTemp,Display,TEXT("QUEST_GATE_APPLY_PASS preserved TaskBox name, canvas slot, child slots, dialogue/portrait widgets and Blueprint compile"));
    }
    auto* Gate=Cast<UVTGQuestSequenceBox>(BP->WidgetTree->FindWidget(TEXT("TaskBox")));check(Gate && Gate->GetChildrenCount()==3);
    check(BP->WidgetTree->FindWidget(TEXT("Line_Main")) && !BP->WidgetTree->FindWidget(TEXT("Line_Main"))->IsChildOf(Gate));
    check(BP->WidgetTree->FindWidget(TEXT("AvtProfile_Main")) && !BP->WidgetTree->FindWidget(TEXT("AvtProfile_Main"))->IsChildOf(Gate));
    UE_LOG(LogTemp,Display,TEXT("QUEST_GATE_VERIFY_PASS real overlay uses a Quest-only presentation gate"));return 0;
}
