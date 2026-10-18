#include "VTGDisplayLocalizationCommandlet.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "UObject/UnrealType.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "VTGDisplayLocalizationMigration.inl"

namespace
{
void DumpStruct(const UStruct* Struct,void* Container,const FString& Prefix,int32 Depth=0)
{
    if(Depth>3)return;
    for(TFieldIterator<FProperty> It(Struct);It;++It)
    {
        auto* P=*It;
        if(Depth==0 && P->GetOwnerStruct()!=Struct && Cast<UClass>(P->GetOwnerStruct()) && !Cast<UClass>(P->GetOwnerStruct())->ClassGeneratedBy)continue;
        FString Value;P->ExportText_InContainer(0,Value,Container,Container,nullptr,PPF_None);
        UE_LOG(LogTemp,Display,TEXT("DISPLAY AUDIT %s.%s [%s] display=%s value=%s"),*Prefix,*P->GetName(),*P->GetCPPType(),*P->GetDisplayNameText().ToString(),*Value.Left(1800));
        if(auto* S=CastField<FStructProperty>(P))DumpStruct(S->Struct,S->ContainerPtrToValuePtr<void>(Container),Prefix+TEXT(".")+P->GetName(),Depth+1);
        if(auto* A=CastField<FArrayProperty>(P))if(auto* S=CastField<FStructProperty>(A->Inner))
        {
            FScriptArrayHelper H(A,A->ContainerPtrToValuePtr<void>(Container));
            if(H.Num())DumpStruct(S->Struct,H.GetRawPtr(0),Prefix+TEXT(".")+P->GetName()+TEXT("[0]"),Depth+1);
        }
    }
}
void DumpBlueprint(UBlueprint* BP)
{
    if(!BP){UE_LOG(LogTemp,Error,TEXT("DISPLAY AUDIT missing Blueprint"));return;}
    UE_LOG(LogTemp,Display,TEXT("DISPLAY AUDIT ASSET %s parent=%s"),*BP->GetPathName(),*GetPathNameSafe(BP->ParentClass));
    if(BP->GeneratedClass)DumpStruct(BP->GeneratedClass,BP->GeneratedClass->GetDefaultObject(),BP->GetName());
    TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
    for(auto* G:Graphs)for(auto N:G->Nodes)
    {
        FString Pins;
        for(auto* P:N->Pins)
        {
            if(P->PinType.PinCategory==TEXT("exec"))continue;
            Pins+=FString::Printf(TEXT(" %s:%s=%s%s ->"),*P->PinName.ToString(),*P->PinType.PinCategory.ToString(),*P->DefaultValue,*P->DefaultTextValue.ToString());
            for(auto* Other:P->LinkedTo)Pins+=Other->GetOwningNode()->GetName()+TEXT(".")+Other->PinName.ToString()+TEXT(",");
        }
        UE_LOG(LogTemp,Display,TEXT("DISPLAY GRAPH %s/%s %s %s"),*G->GetName(),*N->GetName(),*N->GetNodeTitle(ENodeTitleType::ListView).ToString().Replace(TEXT("\n"),TEXT(" ")),*Pins.Left(4000));
    }
}
}
UVTGDisplayLocalizationCommandlet::UVTGDisplayLocalizationCommandlet(){IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}
int32 UVTGDisplayLocalizationCommandlet::Main(const FString& Params)
{
    if(FParse::Param(*Params,TEXT("Apply")))return DisplayMigration::Apply()?0:1;
    if(!FParse::Param(*Params,TEXT("Audit"))){UE_LOG(LogTemp,Error,TEXT("Use -Audit for read-only field inspection."));return 1;}
    auto* Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/Narrative/Mail/DT_Mail_ID_FillHere.DT_Mail_ID_FillHere"));
    if(!Table)return 1;
    UE_LOG(LogTemp,Display,TEXT("DISPLAY AUDIT ROW STRUCT %s"),*Table->GetRowStruct()->GetPathName());
    for(auto& Row:Table->GetRowMap()){DumpStruct(Table->GetRowStruct(),Row.Value,TEXT("Mail.")+Row.Key.ToString());break;}
    auto* Note=LoadObject<UObject>(nullptr,TEXT("/Game/Inventory/InventoryXBased/DefaultNote.DefaultNote"));
    if(Note){DumpStruct(Note->GetClass(),Note,TEXT("DefaultNote"));DumpBlueprint(Cast<UBlueprint>(Note->GetClass()->ClassGeneratedBy));}
    DumpBlueprint(LoadObject<UBlueprint>(nullptr,TEXT("/Game/Blueprints/Widget/WBP_HintLayer.WBP_HintLayer")));
    DumpBlueprint(LoadObject<UBlueprint>(nullptr,TEXT("/InventorySystemX/InventorySystemX/Components/AC_ActorInteractionComponent.AC_ActorInteractionComponent")));
    return 0;
}
