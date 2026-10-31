#include "AnimGraphNode_NarrativeJaw.h"
#include "AnimGraphNode_Root.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Animation/AnimationPoseData.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dialogue.h"
#include "NarrativeComponent.h"
#include "NarrativeJawComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace NPCJaw
{
struct FTarget
{
    const TCHAR* Tag;
    const TCHAR* Blueprint;
    const TCHAR* Animation;
    const TCHAR* Actor;
    const TCHAR* Cutscene;
    float NormalizedLoopSeconds = 0.f;
    const TCHAR* SourceAnimation = nullptr;
};
const FTarget Targets[] = {
    {TEXT("FemaleClient"), TEXT("/Game/Characters/NPC/FemaleClient/ABP_FemaleClientLS"), TEXT("/Game/Characters/NPC/FemaleClient/ANM/ANM_FemaleClient_Mouth"), TEXT("/Game/Characters/NPC/FemaleClient/BP_FemaleClient_StillNPC"), TEXT("Is Cutscene")},
    {TEXT("Bello"), TEXT("/Game/Characters/NPC/Bello/ABP_Bello_NPC"), TEXT("/Game/Characters/NPC/Bello/ANM_Bello_Mouth"), TEXT("/Game/Characters/NPC/Bello/BP_Bello_NPC"), TEXT("Is Cutscene")},
    {TEXT("Corvos"), TEXT("/Game/Characters/NPC/Corvos/ABP_NPC_Corvos"), TEXT("/Game/Characters/NPC/Corvos/ANM_Corvos_Mouth"), TEXT("/Game/Characters/NPC/Corvos/BP_Corvos_NPC"), TEXT("IsCutScene?")},
    {TEXT("Ulises"), TEXT("/Game/Characters/Ulises/ABP_Ulises"), TEXT("/Game/Characters/Ulises/ANM/Mouth_Ulises"), TEXT("/Game/Characters/Ulises/BP_Ulises_NPC"), TEXT("Is Cutscene")},
    {TEXT("May"), TEXT("/Game/Characters/May/ABP_NPC_May"), TEXT("/Game/Characters/May/Anim/May_Mouth_Dialogue"), TEXT("/Game/Characters/May/BP_May_NPC"), TEXT("Is Cutscene"), 0.3f, TEXT("/Game/Characters/May/Anim/May_Mouth")}
};
const FString Marker(TEXT("VTG Narrative Jaw Only"));
void EnsureActorTag(const FTarget& T, bool VerifyOnly)
{
    if (!T.SourceAnimation) return; // Existing NPCs already have their authored binding tags.
    auto* BP = LoadObject<UBlueprint>(nullptr, T.Actor); check(BP && BP->GeneratedClass);
    auto* Default = CastChecked<AActor>(BP->GeneratedClass->GetDefaultObject());
    if (Default->ActorHasTag(T.Tag)) return;
    check(!VerifyOnly);
    const FString File = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    const FString Backup = FPaths::ProjectDir() / TEXT("Scripts/NPCJawMovement/Backup") / (BP->GetName() + TEXT(".uasset"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
    if (!IFileManager::Get().FileExists(*Backup)) check(IFileManager::Get().Copy(*Backup, *File) == COPY_OK);
    Default->Tags.AddUnique(T.Tag);
    FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
    FCompilerResultsLog Log; FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipSave, &Log); check(Log.NumErrors == 0);
    auto* Package = BP->GetOutermost(); Package->FullyLoad(); ResetLoaders(Package); BP->MarkPackageDirty();
    FSavePackageArgs Save; Save.TopLevelFlags = RF_Public | RF_Standalone;
    check(UPackage::SavePackage(Package, BP, *File, Save));
    UE_LOG(LogTemp, Display, TEXT("NPC JAW TAG configured %s actor default tag %s"), *BP->GetName(), T.Tag);
}
UAnimSequence* LoadAnimation(const FTarget& T, bool VerifyOnly)
{
    if (auto* Existing = LoadObject<UAnimSequence>(nullptr, T.Animation)) return Existing;
    check(!VerifyOnly && T.SourceAnimation && T.NormalizedLoopSeconds > 0.f);
    auto* Source = LoadObject<UAnimSequence>(nullptr, T.SourceAnimation); check(Source);
    const int32 JawIndex = Source->GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(TEXT("c_jawbone_x")); check(JawIndex != INDEX_NONE);
    const int32 Frames = 9;
    TArray<FVector> Positions, Scales; TArray<FQuat> Rotations;
    for (int32 I = 0; I <= Frames; ++I)
    {
        FTransform Pose;
        // Raw evaluation incorporates May's sub-frame transform curve; copying only
        // the source bone-track endpoints would lose the authored opening entirely.
        Source->GetBoneTransform(Pose, FSkeletonPoseBoneIndex(JawIndex), double(Source->GetPlayLength()) * I / Frames, true);
        Positions.Add(Pose.GetTranslation()); Rotations.Add(Pose.GetRotation()); Scales.Add(Pose.GetScale3D());
    }
    auto* Package = CreatePackage(T.Animation);
    auto* Anim = NewObject<UAnimSequence>(Package, *FPackageName::GetLongPackageAssetName(T.Animation), RF_Public | RF_Standalone);
    Anim->SetSkeleton(Source->GetSkeleton()); Anim->RateScale = 1.f;
    auto& Controller = Anim->GetController();
    Controller.InitializeModel();
    Controller.OpenBracket(NSLOCTEXT("VTG", "MayJawBake", "Retiming May jaw for dialogue"), false);
    Controller.SetFrameRate(FFrameRate(30, 1), false);
    Controller.SetNumberOfFrames(FFrameNumber(Frames), false);
    check(Controller.AddBoneCurve(TEXT("c_jawbone_x"), false));
    check(Controller.SetBoneTrackKeys(TEXT("c_jawbone_x"), Positions, Rotations, Scales, false));
    Controller.NotifyPopulated(); Controller.CloseBracket(false);
    FAssetRegistryModule::AssetCreated(Anim);
    Anim->MarkPackageDirty();
    FSavePackageArgs Save; Save.TopLevelFlags = RF_Public | RF_Standalone;
    const FString File = FPackageName::LongPackageNameToFilename(T.Animation, FPackageName::GetAssetPackageExtension());
    check(UPackage::SavePackage(Package, Anim, *File, Save));
    UE_LOG(LogTemp, Display, TEXT("MAY JAW RETIME source=%.6fs target=%.3fs equivalent source speed=%.6f source RateScale=%.3f preserved; baked %d jaw keys including transform curve"), Source->GetPlayLength(), Anim->GetPlayLength(), Source->GetPlayLength() / Anim->GetPlayLength(), Source->RateScale, Frames + 1);
    return Anim;
}
void Audit(UAnimBlueprint* BP, UAnimSequence* Anim)
{
    check(BP->TargetSkeleton == Anim->GetSkeleton());
    check(Anim->AdditiveAnimType == AAT_None);
    check(Anim->GetPlayLength() > 0);
    check(BP->TargetSkeleton->GetReferenceSkeleton().FindBoneIndex(TEXT("c_jawbone_x")) != INDEX_NONE);
    const auto* Model = Anim->GetDataModel();
    TArray<FName> Tracks; Model->GetBoneTrackNames(Tracks);
    check(Tracks.Contains(TEXT("c_jawbone_x")));
    TArray<FTransform> Keys; Model->GetBoneTrackTransforms(TEXT("c_jawbone_x"), Keys);
    check(Keys.Num() > 1);
    bool Moves = false;
    for (const auto& Key : Keys) Moves |= !Key.Equals(Keys[0], 0.001f);
    check(Moves && Keys[0].Equals(Keys.Last(), 0.01f));
    UE_LOG(LogTemp, Display, TEXT("NPC JAW ASSET PASS %s duration=%.3f jaw keys=%d loop endpoints match; other %d tracks ignored"), *BP->GetName(), Anim->GetPlayLength(), Keys.Num(), Tracks.Num() - 1);
}
void Apply(UAnimBlueprint* BP, UAnimSequence* Anim, bool VerifyOnly, const FTarget& T)
{
    UEdGraph* Graph = nullptr;
    for (UEdGraph* G : BP->FunctionGraphs) if (G->GetFName() == TEXT("AnimGraph")) Graph = G;
    check(Graph);
    UAnimGraphNode_Root* Root = nullptr;
    UAnimGraphNode_NarrativeJaw* Jaw = nullptr;
    int32 Count = 0;
    for (UEdGraphNode* N : Graph->Nodes)
    {
        if (auto* R = Cast<UAnimGraphNode_Root>(N)) Root = R;
        if (auto* J = Cast<UAnimGraphNode_NarrativeJaw>(N)) { Jaw = J; ++Count; }
    }
    check(Root && Count <= 1);
    if (!Jaw && !VerifyOnly)
    {
        const FString File = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        const FString Backup = FPaths::ProjectDir() / TEXT("Scripts/NPCJawMovement/Backup") / (BP->GetName() + TEXT(".uasset"));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
        if (!IFileManager::Get().FileExists(*Backup)) check(IFileManager::Get().Copy(*Backup, *File) == COPY_OK);
        auto* Input = Root->FindPinChecked(TEXT("Result")); check(Input->LinkedTo.Num() == 1);
        auto* Previous = Input->LinkedTo[0];
        Jaw = NewObject<UAnimGraphNode_NarrativeJaw>(Graph);
        Graph->AddNode(Jaw, false, false); Jaw->CreateNewGuid(); Jaw->PostPlacedNewNode();
        Jaw->NodeComment = Marker; Jaw->bCommentBubbleVisible = true;
        Jaw->NodePosX = Root->NodePosX - 320; Jaw->NodePosY = Root->NodePosY;
        Jaw->Node.Sequence = Anim; Jaw->Node.JawBone.BoneName = TEXT("c_jawbone_x");
        Jaw->AllocateDefaultPins();
        Input->BreakAllPinLinks();
        check(Graph->GetSchema()->TryCreateConnection(Previous, Jaw->FindPinChecked(TEXT("SourcePose"))));
        check(Graph->GetSchema()->TryCreateConnection(Jaw->FindPinChecked(TEXT("Pose")), Input));
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FCompilerResultsLog Log;
        FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipSave, &Log);
        check(Log.NumErrors == 0);
        auto* Package = BP->GetOutermost(); Package->FullyLoad(); ResetLoaders(Package); BP->MarkPackageDirty();
        FSavePackageArgs Save; Save.TopLevelFlags = RF_Public | RF_Standalone;
        check(UPackage::SavePackage(Package, BP, *File, Save));
    }
    check(Jaw && Jaw->Node.Sequence == Anim && Jaw->Node.JawBone.BoneName == TEXT("c_jawbone_x"));
    if (T.NormalizedLoopSeconds > 0.f) check(FMath::IsNearlyEqual(Anim->GetPlayLength(), T.NormalizedLoopSeconds));
    check(Jaw->FindPinChecked(TEXT("SourcePose"))->LinkedTo.Num() == 1);
    check(Jaw->FindPinChecked(TEXT("Pose"))->LinkedTo.Num() == 1 && Jaw->FindPinChecked(TEXT("Pose"))->LinkedTo[0] == Root->FindPinChecked(TEXT("Result")));
    UE_LOG(LogTemp, Display, TEXT("NPC JAW GRAPH PASS %s one jaw-only node after existing montage/look/face graph"), *BP->GetName());
    UE_LOG(LogTemp, Display, TEXT("NPC JAW RATE %s effective loop=%.3fs sequence RateScale=%.3f"), T.Tag, Anim->GetPlayLength(), Anim->RateScale);
}
bool Test(bool MayOnly)
{
    TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
    auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("NPCJawRegression"), nullptr, true, ERHIFeatureLevel::Num, &Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* PlayerBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));
    auto* Player = World->SpawnActor<ACharacter>(PlayerBP->GeneratedClass, FTransform::Identity, Spawn);
    auto* PC = World->SpawnActor<APlayerController>(); PC->Possess(Player);
    auto* Comp = NewObject<UNarrativeComponent>(PC); Comp->RegisterComponent();
    int32 Fail = 0;
    auto Expect = [&](bool Pass, const FTarget& T, const TCHAR* What)
    { Fail += !Pass; UE_LOG(LogTemp, Display, TEXT("NPC JAW TEST %s %s: %s"), Pass ? TEXT("PASS") : TEXT("FAIL"), T.Tag, What); };
    for (const auto& T : Targets)
    {
        if (MayOnly && FCString::Strcmp(T.Tag, TEXT("May")) != 0) continue;
        auto* ABP = LoadObject<UAnimBlueprint>(nullptr, T.Blueprint);
        auto* BP = LoadObject<UBlueprint>(nullptr, T.Actor); check(BP && ABP);
        auto* Actor = World->SpawnActor<AActor>(BP->GeneratedClass, FTransform::Identity, Spawn); check(Actor);
        TArray<USkeletalMeshComponent*> Meshes; Actor->GetComponents(Meshes);
        USkeletalMeshComponent* Mesh = nullptr;
        for (auto* M : Meshes) if (M->GetAnimClass() == ABP->GeneratedClass) Mesh = M;
        check(Mesh); Mesh->InitAnim(true); auto* Instance = Mesh->GetAnimInstance(); check(Instance);
        auto* Cutscene = FindFProperty<FBoolProperty>(Instance->GetClass(), T.Cutscene); check(Cutscene);
        Cutscene->SetPropertyValue_InContainer(Instance, false);
        FString Tags; for (FName Tag : Actor->Tags) Tags += Tag.ToString() + TEXT(",");
        FString MeshTags; for (FName Tag : Mesh->ComponentTags) MeshTags += Tag.ToString() + TEXT(",");
        UE_LOG(LogTemp, Display, TEXT("NPC JAW ACTOR %s actor tags=%s mesh=%s mesh tags=%s"), T.Tag, *Tags, *Mesh->GetName(), *MeshTags);
        auto* Dialogue = NewObject<UDialogue>(World); Dialogue->OwningComp = Comp;
        Dialogue->JawMinPlayRate = Dialogue->JawMaxPlayRate = 1.f;
        Dialogue->JawMinAmplitude = Dialogue->JawMaxAmplitude = 1.f;
        auto* Node = NewObject<UDialogueNode_NPC>(Dialogue);
        FDialogueLine Line; Line.Text = FText::FromString(TEXT("这是一条测试NPC嘴部动画的长台词，用于检查Tag绑定和实际骨骼输出。"));
        FSpeakerInfo Speaker; Speaker.SpeakerID = T.Tag; Speaker.SpeakingActorTag = T.Tag;
        auto Start = [&]() { Dialogue->StartJawMovement(Node, Line, Speaker); };
        auto Frame = [&]() { Mesh->TickAnimation(0.f, false); Mesh->RefreshBoneTransforms(); };
        Frame(); Frame();
        const int32 Index = Mesh->GetBoneIndex(TEXT("c_jawbone_x")); check(Index != INDEX_NONE);
        Start();
        auto* State = Actor->FindComponentByClass<UNarrativeJawComponent>();
        float Age = 0, Weight = 0;
        Expect(State && State->Sample(Age, Weight), T, TEXT("existing scene actor/mesh tag resolves without synthetic tag"));
        if (!State) { Actor->Destroy(); continue; }
        World->Tick(LEVELTICK_TimeOnly, 0.12f); Frame();
        const auto Speaking = Mesh->GetBoneSpaceTransforms();
        if (T.NormalizedLoopSeconds > 0.f)
        {
            auto* Mouth = LoadObject<UAnimSequence>(nullptr, T.Animation);
            const float BaseRate = Mouth->GetPlayLength() / T.NormalizedLoopSeconds;
            FNarrativeJawPlayback Calibrated;
            Expect(State->SamplePlayback(T.NormalizedLoopSeconds, Calibrated) && Calibrated.LoopIndex == 0 && FMath::IsNearlyEqual(Calibrated.Position, .12f, .001f), T, TEXT("short animation is still inside its first calibrated 0.3s loop after 0.12s"));
            FTransform Expected;
            const int32 SkeletonIndex = Mouth->GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(TEXT("c_jawbone_x"));
            Mouth->GetBoneTransform(Expected, FSkeletonPoseBoneIndex(SkeletonIndex), double(Calibrated.Position * BaseRate), false);
            Expect(Speaking[Index].Equals(Expected, .001f), T, TEXT("actual evaluated jaw samples the retimed animation phase"));
        }
        Dialogue->StopJawMovement(); Frame(); const auto Baseline = Mesh->GetBoneSpaceTransforms();
        Expect(!Speaking[Index].Equals(Baseline[Index], 0.001f), T, TEXT("final evaluated jaw moves"));
        int32 OtherChanges = 0;
        for (int32 I = 0; I < Speaking.Num(); ++I) if (I != Index && !Speaking[I].Equals(Baseline[I], 0.001f)) ++OtherChanges;
        UE_LOG(LogTemp, Display, TEXT("NPC JAW OTHER BONES %s changed=%d"), T.Tag, OtherChanges);
        Expect(OtherChanges == 0, T, TEXT("all other local bones including eyes remain unchanged"));
        Expect(!State->Sample(Age, Weight), T, TEXT("skip/stop cancels immediately"));
        Node->bEnableJawMovement = false; Start(); Expect(!State->Sample(Age, Weight), T, TEXT("node switch disables jaw")); Node->bEnableJawMovement = true;
        Dialogue->bEnableJawMovement = false; Start(); Expect(!State->Sample(Age, Weight), T, TEXT("global switch disables jaw")); Dialogue->bEnableJawMovement = true;
        auto* Sequence = LoadObject<UAnimSequence>(nullptr, T.Animation);
        auto* Montage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(Sequence, TEXT("DefaultSlot"), 0.f, 0.f, 1.f, 100);
        Start(); World->Tick(LEVELTICK_TimeOnly, 0.12f); Frame(); const auto Jaw = Mesh->GetBoneSpaceTransforms()[Index];
        Expect(Instance->Montage_Play(Montage) > 0, T, TEXT("montage starts while speaking"));
        Mesh->TickAnimation(0.07f, false); Mesh->RefreshBoneTransforms();
        Expect(Instance->Montage_IsPlaying(Montage), T, TEXT("speaking never interrupts montage"));
        Expect(Jaw.Equals(Mesh->GetBoneSpaceTransforms()[Index], 0.001f), T, TEXT("jaw stays above montage mouth tracks"));
        Instance->Montage_Stop(0.f); Dialogue->StopJawMovement(); Frame();
        Start(); Cutscene->SetPropertyValue_InContainer(Instance, true); Frame();
        Expect(!State->Sample(Age, Weight), T, TEXT("authored cutscene flag cancels speech"));
        Cutscene->SetPropertyValue_InContainer(Instance, false); Frame();
        Expect(!State->Sample(Age, Weight), T, TEXT("leaving cutscene never resumes old speech"));
        Dialogue->JawMinPlayRate = 0.7f; Dialogue->JawMaxPlayRate = 1.2f;
        Dialogue->JawMinAmplitude = 0.5f; Dialogue->JawMaxAmplitude = 1.f;
        Start(); World->Tick(LEVELTICK_TimeOnly, 0.12f); Frame();
        FNarrativeJawPlayback Playback;
        const float EffectiveLength = T.NormalizedLoopSeconds > 0.f ? T.NormalizedLoopSeconds : Sequence->GetPlayLength();
        Expect(State->SamplePlayback(EffectiveLength, Playback) && Playback.PlayRate >= .7f && Playback.PlayRate <= 1.2f && Playback.Amplitude >= .5f && Playback.Amplitude <= 1.f, T, TEXT("shared random speed/amplitude settings work"));
        Dialogue->Deinitialize(); Expect(!State->Sample(Age, Weight), T, TEXT("dialogue shutdown clears speech"));
        Actor->Destroy();
    }
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    UE_LOG(LogTemp, Display, TEXT("NPC JAW TEST SUMMARY failures=%d"), Fail);
    return Fail == 0;
}
}
int32 VTGRunNPCJawMovement(const FString& Params)
{
    const bool MayOnly = FParse::Param(*Params, TEXT("MayOnly"));
    for (const auto& T : NPCJaw::Targets)
    {
        if (MayOnly && FCString::Strcmp(T.Tag, TEXT("May")) != 0) continue;
        auto* BP = LoadObject<UAnimBlueprint>(nullptr, T.Blueprint);
        auto* Anim = NPCJaw::LoadAnimation(T, Params.Contains(TEXT("VerifyOnly"))); check(BP && Anim);
        NPCJaw::Audit(BP, Anim);
        NPCJaw::Apply(BP, Anim, Params.Contains(TEXT("VerifyOnly")), T);
        NPCJaw::EnsureActorTag(T, Params.Contains(TEXT("VerifyOnly")));
    }
    return Params.Contains(TEXT("Test")) && !NPCJaw::Test(MayOnly) ? 1 : 0;
}
