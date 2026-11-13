#include "VTGHeadacheVolumeCommandlet.h"
#include "Components/PostProcessComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

int32 VTGIntegrateHeadacheNarrative(const FString& Params);

namespace HeadacheVolume
{
const TCHAR* PackageName = TEXT("/Game/VTG/Blueprints/BP_HeadachePostProcess");
const FName StrengthName(TEXT("HeadacheIntensity"));

UEdGraphPin* Pin(UEdGraphNode* N, const TCHAR* Name)
{
    auto* Result = N->FindPin(Name); checkf(Result, TEXT("Missing pin %s on %s"), Name, *N->GetClass()->GetName()); return Result;
}
void Link(UEdGraphNode* A, const TCHAR* AP, UEdGraphNode* B, const TCHAR* BP)
{
    check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Pin(A, AP), Pin(B, BP)));
}
template<class T> T* Add(UEdGraph* G, int X, int Y)
{
    auto* N = NewObject<T>(G); G->AddNode(N, false, false); N->CreateNewGuid(); N->NodePosX=X; N->NodePosY=Y; return N;
}
void Compile(UBlueprint* BP)
{
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipSave, &Results);
    checkf(Results.NumErrors == 0, TEXT("Headache BP has %d compiler errors"), Results.NumErrors);
}
bool Verify(UBlueprint* BP)
{
    check(BP && BP->GeneratedClass);
    check(BP->ParentClass == AActor::StaticClass());
    auto* Strength = FindFProperty<FDoubleProperty>(BP->GeneratedClass, StrengthName); check(Strength);
    check(Strength->HasAnyPropertyFlags(CPF_Edit));
    check(!Strength->HasAnyPropertyFlags(CPF_DisableEditOnInstance));
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* W = UWorld::CreateWorld(EWorldType::Editor, false, TEXT("HeadacheVolumeVerification"), nullptr, true, ERHIFeatureLevel::Num, &Init);
    auto* A = W->SpawnActor<AActor>(BP->GeneratedClass, FTransform::Identity); check(A);
    check(!A->PrimaryActorTick.bCanEverTick);
    for (double Value : {1.0, 0.0, 0.65, 2.0, -0.5, 1.0})
    {
        Strength->SetPropertyValue_InContainer(A, Value);
        A->RerunConstructionScripts();
        TArray<UPostProcessComponent*> Components; A->GetComponents(Components);
        check(Components.Num() == 1);
        auto* PP = Components[0];
        check(PP->bUnbound && PP->bEnabled);
        check(FMath::IsNearlyEqual(PP->BlendWeight, (float)FMath::Clamp(Value, 0.0, 1.0)));
        check(PP->Settings.WeightedBlendables.Array.Num() == 1);
        check(PP->Settings.WeightedBlendables.Array[0].Object != nullptr);
        check(!PP->Settings.bOverride_VignetteIntensity && !PP->Settings.bOverride_MotionBlurAmount
            && !PP->Settings.bOverride_SceneFringeIntensity && !PP->Settings.bOverride_AutoExposureBias);
        UE_LOG(LogTemp, Display, TEXT("HEADACHE_VOLUME_CHECK intensity=%.2f blend=%.2f blendables=%d"),
            Value, PP->BlendWeight, PP->Settings.WeightedBlendables.Array.Num());
    }
    W->DestroyWorld(false);
    UE_LOG(LogTemp, Display, TEXT("HEADACHE_VOLUME_VERIFY_PASS"));
    return true;
}
}

UVTGHeadacheVolumeCommandlet::UVTGHeadacheVolumeCommandlet()
{
    IsClient=false; IsServer=false; IsEditor=true; LogToConsole=true;
}
int32 UVTGHeadacheVolumeCommandlet::Main(const FString& Params)
{
    if (Params.Contains(TEXT("IntegrateNarrative"))) return VTGIntegrateHeadacheNarrative(Params);
    using namespace HeadacheVolume;
    const FString AssetPath = FString(PackageName) + TEXT(".BP_HeadachePostProcess");
    if (Params.Contains(TEXT("VerifyOnly")))
    {
        auto* BP = LoadObject<UBlueprint>(nullptr, *AssetPath); check(BP); Compile(BP); return Verify(BP) ? 0 : 1;
    }
    // Never replace an existing authored Blueprint on a rerun.
    checkf(!FPackageName::DoesPackageExist(PackageName), TEXT("BP already exists; use -VerifyOnly."));
    auto* Full = LoadObject<UMaterialInstanceConstant>(nullptr,
        TEXT("/Game/VTG/Materials/PostProcess/Headache/MI_PP_Headache_FullStrength.MI_PP_Headache_FullStrength")); check(Full);
    UPackage* Package = CreatePackage(PackageName);
    auto* BP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("BP_HeadachePostProcess"),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass()); check(BP);
    USCS_Node* Component = BP->SimpleConstructionScript->CreateNode(UPostProcessComponent::StaticClass(), TEXT("HeadachePostProcess"));
    auto* PP = CastChecked<UPostProcessComponent>(Component->ComponentTemplate);
    PP->bUnbound=true; PP->bEnabled=true; PP->BlendWeight=1.f; PP->Priority=100.f;
    PP->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, Full));
    BP->SimpleConstructionScript->AddNode(Component);

    FEdGraphPinType Type; Type.PinCategory=UEdGraphSchema_K2::PC_Real; Type.PinSubCategory=UEdGraphSchema_K2::PC_Double;
    check(FBlueprintEditorUtils::AddMemberVariable(BP, StrengthName, Type, TEXT("1.0")));
    FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP, StrengthName, false);
    FBlueprintEditorUtils::SetBlueprintVariableCategory(BP, StrengthName, nullptr, FText::FromString(TEXT("Headache")));
    for (const TCHAR* Key : {TEXT("ClampMin"), TEXT("UIMin")}) FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP, StrengthName, nullptr, Key, TEXT("0.0"));
    for (const TCHAR* Key : {TEXT("ClampMax"), TEXT("UIMax")}) FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP, StrengthName, nullptr, Key, TEXT("1.0"));
    FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP, StrengthName, nullptr, TEXT("ToolTip"),
        TEXT("0 = original image / no headache pass; 1 = full grain, double image and peripheral pressure. Updates in editor via Construction Script. Global unbound volume."));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); Compile(BP);
    UEdGraph* G = FBlueprintEditorUtils::FindUserConstructionScript(BP); check(G);
    UK2Node_FunctionEntry* Entry=nullptr;
    for (UEdGraphNode* N : G->Nodes) if (auto* E = Cast<UK2Node_FunctionEntry>(N)) Entry=E;
    check(Entry);
    // Keep the existing parent construction call, then append our weight assignment.
    UEdGraphNode* Exec = Entry;
    if (Pin(Entry,TEXT("then"))->LinkedTo.Num()) Exec=Pin(Entry,TEXT("then"))->LinkedTo[0]->GetOwningNode();
    check(Pin(Exec,TEXT("then"))->LinkedTo.Num() == 0);
    auto* GetStrength=Add<UK2Node_VariableGet>(G,0,200); GetStrength->VariableReference.SetSelfMember(StrengthName); GetStrength->AllocateDefaultPins();
    auto* GetComponent=Add<UK2Node_VariableGet>(G,280,350); GetComponent->VariableReference.SetSelfMember(TEXT("HeadachePostProcess")); GetComponent->AllocateDefaultPins();
    auto* Clamp=Add<UK2Node_CallFunction>(G,260,170);
    Clamp->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("FClamp"))); Clamp->AllocateDefaultPins();
    GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*Pin(Clamp,TEXT("Min")),TEXT("0.0"));
    GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*Pin(Clamp,TEXT("Max")),TEXT("1.0"));
    Link(GetStrength,TEXT("HeadacheIntensity"),Clamp,TEXT("Value"));
    auto* SetWeight=Add<UK2Node_VariableSet>(G,620,0);
    SetWeight->VariableReference.SetExternalMember(TEXT("BlendWeight"),UPostProcessComponent::StaticClass()); SetWeight->AllocateDefaultPins();
    Link(Exec,TEXT("then"),SetWeight,TEXT("execute"));
    Link(GetComponent,TEXT("HeadachePostProcess"),SetWeight,TEXT("self"));
    Link(Clamp,TEXT("ReturnValue"),SetWeight,TEXT("BlendWeight"));
    SetWeight->NodeComment=TEXT("Volume weight interpolates FullStrength's single material parameter from the neutral parent (0) to 1. No Tick, MID recreation or camera edits.");
    SetWeight->bCommentBubbleVisible=true;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); Compile(BP);
    BP->GeneratedClass->GetDefaultObject<AActor>()->PrimaryActorTick.bCanEverTick=false;
    Verify(BP);
    const FString Filename=FPackageName::LongPackageNameToFilename(PackageName,FPackageName::GetAssetPackageExtension());
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename),true);
    FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; Args.SaveFlags=SAVE_NoError;
    check(UPackage::SavePackage(Package,BP,*Filename,Args));
    UE_LOG(LogTemp,Display,TEXT("HEADACHE_VOLUME_SAVED %s"),*Filename);
    return 0;
}
