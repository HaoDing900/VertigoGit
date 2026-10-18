// Included by the editor-only commandlet. Existing identifiers are never renamed.
#include "UI/VTGHintLayer.h"
#include "UI/VTGDisplayLocalizationLibrary.h"
#include "Engine/UserDefinedStruct.h"
#include "Kismet2/StructureEditorUtils.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "EdGraphSchema_K2.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "UObject/SavePackage.h"
#include "HAL/FileManager.h"

namespace DisplayMigration
{
TArray<UObject*> Changed;
FString BackupRoot;
bool Backup(UObject* Object)
{
    if(Changed.Contains(Object))return true;
    const FString Package=Object->GetOutermost()->GetName();
    const FString Source=FPackageName::LongPackageNameToFilename(Package,FPackageName::GetAssetPackageExtension());
    const FString Target=BackupRoot/Package.RightChop(1)+TEXT(".uasset");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Target),true);
    if(IFileManager::Get().Copy(*Target,*Source)!=COPY_OK)return false;
    Changed.Add(Object);return true;
}
FProperty* Field(UStruct* Struct,const FString& Prefix)
{
    for(TFieldIterator<FProperty> It(Struct);It;++It)if(It->GetName().StartsWith(Prefix))return *It;
    return nullptr;
}
bool Link(UEdGraphPin* From,UEdGraphPin* To)
{
    return From && To && GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(From,To);
}
UK2Node_CallFunction* Call(UEdGraph* Graph,UFunction* Function,UEdGraphNode* At)
{
    auto* Node=NewObject<UK2Node_CallFunction>(Graph);Graph->AddNode(Node,false,false);
    Node->CreateNewGuid();Node->SetFromFunction(Function);Node->AllocateDefaultPins();
    Node->NodePosX=At->NodePosX;Node->NodePosY=At->NodePosY+150;return Node;
}
bool ReplaceConversion(UK2Node_CallFunction* Old,UK2Node_CallFunction* New,const FName Input)
{
    auto* OldInput=Old->FindPin(TEXT("InName"));auto* Output=Old->FindPin(TEXT("ReturnValue"));
    if(!OldInput || !Output)return false;
    const auto Sources=OldInput->LinkedTo,Targets=Output->LinkedTo;
    if(Sources.IsEmpty())New->FindPinChecked(Input)->DefaultValue=OldInput->DefaultValue;
    else for(auto* Source:Sources)if(!Link(Source,New->FindPin(Input)))return false;
    Output->BreakAllPinLinks();
    for(auto* Target:Targets)if(!Link(New->FindPin(TEXT("ReturnValue")),Target))return false;
    Old->GetGraph()->RemoveNode(Old);return true;
}
bool Compile(UBlueprint* BP)
{
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Log);
    return Log.NumErrors==0 && BP->Status!=BS_Error;
}
bool Apply()
{
    Changed.Reset();BackupRoot=FPaths::ProjectSavedDir()/TEXT("Backups/DisplayLocalization")/FGuid::NewGuid().ToString();
    auto* Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/Narrative/Mail/DT_Mail_ID_FillHere.DT_Mail_ID_FillHere"));
    auto* Hint=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Blueprints/Widget/WBP_HintLayer.WBP_HintLayer"));
    auto* Mail=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Inventory/InventoryXBased/UMG/TabSwitcher/InTabWidget/WB_TabWidget_Mail.WB_TabWidget_Mail"));
    if(!Table || !Hint || !Mail)return false;
    auto* Struct=Cast<UUserDefinedStruct>(const_cast<UScriptStruct*>(Table->GetRowStruct()));
    if(!Struct || !Backup(Table) || !Backup(Struct) || !Backup(Hint) || !Backup(Mail))return false;
    auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();Registry.SearchAllAssets(true);
    TArray<UBlueprint*> StructUsers;
    TArray<FName> StructReferences;Registry.GetReferencers(Struct->GetOutermost()->GetFName(),StructReferences);
    for(const auto& Package:StructReferences)
    {
        TArray<FAssetData> Assets;Registry.GetAssetsByPackageName(Package,Assets);
        for(const auto& Asset:Assets)if(auto* BP=Cast<UBlueprint>(Asset.GetAsset()))
        {if(!Backup(BP))return false;StructUsers.AddUnique(BP);}
    }
    // Capture all legacy row fields, including dialogue references and IDs, before changing the schema.
    TMap<FName,TMap<FName,FString>> Before;
    for(const auto& Row:Table->GetRowMap())for(TFieldIterator<FProperty> It(Struct);It;++It)
    {
        if(It->GetName().StartsWith(TEXT("SenderDisplayName_")))continue;
        FString Value;It->ExportText_InContainer(0,Value,Row.Value,Row.Value,nullptr,PPF_None);Before.FindOrAdd(Row.Key).Add(It->GetFName(),Value);
    }
    if(!Field(Struct,TEXT("SenderDisplayName_")))
    {
        FEdGraphPinType Type;Type.PinCategory=UEdGraphSchema_K2::PC_Text;
        if(!FStructureEditorUtils::AddVariable(Struct,Type))return false;
        const auto& Variables=FStructureEditorUtils::GetVarDesc(Struct);
        if(!FStructureEditorUtils::RenameVariable(Struct,Variables.Last().VarGuid,TEXT("SenderDisplayName")))return false;
    }
    auto* Legacy=CastField<FNameProperty>(Field(Struct,TEXT("SenderName_")));
    auto* Display=CastField<FTextProperty>(Field(Struct,TEXT("SenderDisplayName_")));
    if(!Legacy || !Display || Table->GetRowMap().Num()!=Before.Num())return false;
    for(const auto& Row:Table->GetRowMap())
    {
        const auto* Expected=Before.Find(Row.Key);if(!Expected)return false;
        for(const auto& Pair:*Expected)
        {
            auto* P=Struct->FindPropertyByName(Pair.Key);if(!P)return false;
            FString Actual;P->ExportText_InContainer(0,Actual,Row.Value,Row.Value,nullptr,PPF_None);
            if(Actual!=Pair.Value){UE_LOG(LogTemp,Error,TEXT("Legacy Mail field changed: %s.%s"),*Row.Key.ToString(),*Pair.Key.ToString());return false;}
        }
        if(Display->GetPropertyValue_InContainer(Row.Value).IsEmpty())
            Display->SetPropertyValue_InContainer(Row.Value,FText::ChangeKey(TEXT("VTG.Mail.Sender"),Legacy->GetPropertyValue_InContainer(Row.Value).ToString(),FText::FromName(Legacy->GetPropertyValue_InContainer(Row.Value))));
    }
    // Replace only the visible sender conversion. Stored Mail_Sender_Name remains FName.
    TArray<UEdGraph*> Graphs;Mail->GetAllGraphs(Graphs);
    int32 MailChanges=0;
    for(auto* Graph:Graphs)
    {
        if(Graph->GetName().StartsWith(TEXT("ExecuteUbergraph")) || Graph->GetName().EndsWith(TEXT("_MERGED")))continue;
        const auto Nodes=Graph->Nodes;
        for(auto Node:Nodes)if(auto* Old=Cast<UK2Node_CallFunction>(Node))if(Old->FunctionReference.GetMemberName()==TEXT("Conv_NameToText"))
        {
            auto* Input=Old->FindPin(TEXT("InName"));if(!Input || Input->LinkedTo.Num()!=1)continue;
            auto* Getter=Cast<UK2Node_VariableGet>(Input->LinkedTo[0]->GetOwningNode());
            if(!Getter || Getter->VariableReference.GetMemberName()!=TEXT("Mail_Sender_Name"))continue;
            auto* New=Call(Graph,UVTGDisplayLocalizationLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UVTGDisplayLocalizationLibrary,GetMailSenderText)),Old);
            New->FindPinChecked(TEXT("Table"))->DefaultObject=Table;
            if(!ReplaceConversion(Old,New,TEXT("SenderID")))return false;++MailChanges;
        }
    }
    // Preserve Show Hint's legacy Name argument; map it to an independent FText surface.
    Graphs.Reset();Hint->GetAllGraphs(Graphs);int32 HintChanges=0;
    for(auto* Graph:Graphs)if(Graph->GetName()==TEXT("Show Hint"))
    {
        const auto Nodes=Graph->Nodes;
        for(auto Node:Nodes)if(auto* Old=Cast<UK2Node_CallFunction>(Node))
        {
            if(Old->FunctionReference.GetMemberName()==TEXT("Conv_NameToText"))
            {
                auto* Input=Old->FindPin(TEXT("InName"));if(!Input || Input->LinkedTo.Num()!=1 || Input->LinkedTo[0]->PinName!=TEXT("HintDescription"))continue;
                auto* New=Call(Graph,UVTGHintLayer::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UVTGHintLayer,ResolveHintDescription)),Old);
                if(!ReplaceConversion(Old,New,TEXT("ID")))return false;++HintChanges;
            }
            else if(Old->FunctionReference.GetMemberName()==TEXT("SetText"))
            {
                auto* Input=Old->FindPin(TEXT("InText"));if(!Input || !Input->LinkedTo.IsEmpty() || Input->DefaultTextValue.ToString()!=TEXT("New Message Received. "))continue;
                auto* Getter=NewObject<UK2Node_VariableGet>(Graph);Graph->AddNode(Getter,false,false);Getter->CreateNewGuid();
                Getter->VariableReference.SetSelfMember(GET_MEMBER_NAME_CHECKED(UVTGHintLayer,NewMailHintText));Getter->AllocateDefaultPins();
                Getter->NodePosX=Old->NodePosX-250;Getter->NodePosY=Old->NodePosY+150;
                if(!Link(Getter->FindPin(GET_MEMBER_NAME_CHECKED(UVTGHintLayer,NewMailHintText)),Input))return false;
            }
        }
    }
    if(!Compile(Hint) || !Compile(Mail))return false;
    for(auto* BP:StructUsers)if(BP!=Hint && BP!=Mail && !Compile(BP))return false;
    // Gather literal legacy hint descriptions without changing caller IDs or action names.
    auto* Defaults=Cast<UVTGHintLayer>(Hint->GeneratedClass->GetDefaultObject());if(!Defaults)return false;
    TArray<FName> References;Registry.GetReferencers(Hint->GetOutermost()->GetFName(),References);
    for(const auto& Package:References)
    {
        TArray<FAssetData> Assets;Registry.GetAssetsByPackageName(Package,Assets);
        for(const auto& Asset:Assets)if(auto* BP=Cast<UBlueprint>(Asset.GetAsset()))
        {
            TArray<UEdGraph*> CallerGraphs;BP->GetAllGraphs(CallerGraphs);
            for(auto* Graph:CallerGraphs)for(auto Node:Graph->Nodes)if(auto* CallNode=Cast<UK2Node_CallFunction>(Node))
                if(CallNode->FunctionReference.GetMemberName()==TEXT("Show Hint"))if(auto* Pin=CallNode->FindPin(TEXT("HintDescription")))
                    if(Pin->LinkedTo.IsEmpty() && !Pin->DefaultValue.TrimStartAndEnd().IsEmpty() && !FName(*Pin->DefaultValue).IsNone())
                    {
                        const FName ID(*Pin->DefaultValue);
                        if(!Defaults->HintDescriptions.ContainsByPredicate([ID](const FVTGHintDisplayText& Entry){return Entry.ID==ID;}))
                        {
                            auto& Entry=Defaults->HintDescriptions.AddDefaulted_GetRef();Entry.ID=ID;
                            Entry.Text=FText::ChangeKey(TEXT("VTG.Hint.Description"),FGuid::NewGuid().ToString(),FText::FromString(Pin->DefaultValue));
                        }
                    }
        }
    }
    // Save only after all changed blueprints compile and legacy fields have been verified.
    for(auto* Object:Changed)
    {
        Object->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
        const FString File=FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
        if(!UPackage::SavePackage(Object->GetOutermost(),Object,*File,Args))return false;
    }
    UE_LOG(LogTemp,Display,TEXT("DISPLAY LOCALIZATION APPLY PASS: Mail conversions=%d, Hint conversions=%d, rows=%d. Backup: %s"),MailChanges,HintChanges,Table->GetRowMap().Num(),*BackupRoot);
    return true;
}
}
