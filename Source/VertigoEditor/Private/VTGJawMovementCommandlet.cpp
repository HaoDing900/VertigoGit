#include "VTGJawMovementCommandlet.h"
#include "AnimGraphNode_NarrativeJaw.h"
#include "AnimGraphNode_Root.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dialogue.h"
#include "NarrativeJawComponent.h"
#include "NarrativeComponent.h"
#include "DialogueBlueprintGeneratedClass.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Animation/AnimMontage.h"
#include "Animation/VTGCameraLookAnimInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxAnimSequenceImportData.h"
#include "Exporters/Exporter.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "UObject/SavePackage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DialogueBlueprint.h"

namespace JawEditor
{
const TCHAR* AssetPath = TEXT("/Game/Characters/Sa/Anm/Expression_JawMovement");
const FString Marker = TEXT("VTG Narrative Jaw Only");
void Save(UObject* Asset)
{
    auto* Package = Asset->GetOutermost();
    Package->FullyLoad();
    ResetLoaders(Package);
    Asset->MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    check(UPackage::SavePackage(Package, Asset, *FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension()), Args));
}
UAnimSequence* Import(USkeleton* Skeleton)
{
    if (auto* Existing = LoadObject<UAnimSequence>(nullptr, AssetPath)) return Existing;
    auto* Task = NewObject<UAssetImportTask>();
    Task->Filename = TEXT("D:/VTG Dev/Models/Character Models/Sa/In Dev/ANM/Expression/Expression_JawMovement.fbx");
    Task->DestinationPath = TEXT("/Game/Characters/Sa/Anm");
    Task->DestinationName = TEXT("Expression_JawMovement");
    Task->bAutomated = true;
    Task->bSave = false;
    auto* Options = NewObject<UFbxImportUI>();
    Options->bAutomatedImportShouldDetectType = false;
    Options->MeshTypeToImport = FBXIT_Animation;
    Options->bImportMesh = false;
    Options->bImportAnimations = true;
    Options->bImportMaterials = false;
    Options->bImportTextures = false;
    Options->Skeleton = Skeleton;
    Options->AnimSequenceImportData->AnimationLength = FBXALIT_ExportedTime;
    Options->AnimSequenceImportData->bImportCustomAttribute = false;
    Task->Options = Options;
    Task->Factory = NewObject<UFbxFactory>();
    FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().ImportAssetTasks({Task});
    auto* Anim = LoadObject<UAnimSequence>(nullptr, AssetPath);
    check(Anim);
    Save(Anim);
    return Anim;
}
void Inspect(UAnimBlueprint* BP, const FString& Folder)
{
    FStringOutputDevice Out;
    UExporter::ExportToOutputDevice(nullptr, BP, nullptr, Out, TEXT("copy"), 0, PPF_Copy, false);
    FFileHelper::SaveStringToFile(Out, *(Folder / (BP->GetName() + TEXT(".copy"))));
    const auto& Ref = BP->TargetSkeleton->GetReferenceSkeleton();
    for (int32 I = 0; I < Ref.GetNum(); ++I)
        if (Ref.GetBoneName(I).ToString().Contains(TEXT("jaw"))) UE_LOG(LogTemp, Display, TEXT("JAW BONE %s parent %s"), *Ref.GetBoneName(I).ToString(), *Ref.GetBoneName(Ref.GetParentIndex(I)).ToString());
}
void Audit(UAnimSequence* Anim)
{
    const auto* Model = Anim->GetDataModel();
    TArray<FName> Names;
    Model->GetBoneTrackNames(Names);
    int32 Moving = 0;
    for (FName Name : Names)
    {
        TArray<FTransform> Keys;
        Model->GetBoneTrackTransforms(Name, Keys);
        bool Changed = false;
        for (const FTransform& Key : Keys) if (!Key.Equals(Keys[0], 0.0001f)) { Changed = true; break; }
        if (Changed) { ++Moving; UE_LOG(LogTemp, Display, TEXT("JAW AUDIT moving track %s keys %d first %s last %s"), *Name.ToString(), Keys.Num(), *Keys[0].ToString(), *Keys.Last().ToString()); }
    }
    UE_LOG(LogTemp, Display, TEXT("JAW AUDIT duration %.3f tracks %d moving %d float curves %d transform curves %d"), Anim->GetPlayLength(), Names.Num(), Moving, Model->GetNumberOfFloatCurves(), Model->GetNumberOfTransformCurves());
}
void Clean(UAnimSequence* Anim, const FString& Folder)
{
    const FString File = FPackageName::LongPackageNameToFilename(Anim->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    const FString Backup = Folder / TEXT("Expression_JawMovement.raw-import.uasset");
    if (!IFileManager::Get().FileExists(*Backup)) check(IFileManager::Get().Copy(*Backup, *File) == COPY_OK);
    auto& Controller = Anim->GetController();
    Controller.OpenBracket(NSLOCTEXT("VTG", "CleanJaw", "Keep jaw animation only"), false);
    TArray<FName> Names;
    Anim->GetDataModel()->GetBoneTrackNames(Names);
    check(Names.Contains(TEXT("c_jawbone_x")));
    for (FName Name : Names) if (Name != TEXT("c_jawbone_x")) Controller.RemoveBoneTrack(Name, false);
    Controller.RemoveAllCurvesOfType(ERawCurveTrackTypes::RCT_Float, false);
    Controller.RemoveAllCurvesOfType(ERawCurveTrackTypes::RCT_Transform, false);
    Controller.CloseBracket(false);
    Save(Anim);
}
void Apply(UAnimBlueprint* BP, UAnimSequence* Anim, const FString& Folder, bool VerifyOnly)
{
    check(BP->TargetSkeleton == Anim->GetSkeleton());
    UEdGraph* Graph = nullptr;
    for (UEdGraph* G : BP->FunctionGraphs) if (G->GetFName() == TEXT("AnimGraph")) Graph = G;
    check(Graph);
    UAnimGraphNode_NarrativeJaw* Jaw = nullptr;
    int32 Count = 0;
    for (UEdGraphNode* N : Graph->Nodes) if (auto* J = Cast<UAnimGraphNode_NarrativeJaw>(N)) { Jaw = J; ++Count; }
    check(Count <= 1);
    if (!Jaw && !VerifyOnly)
    {
        const FString File = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        const FString Backup = Folder / (BP->GetName() + TEXT(".before.uasset"));
        if (!IFileManager::Get().FileExists(*Backup)) check(IFileManager::Get().Copy(*Backup, *File) == COPY_OK);
        // After the final montage and before component-space look/eye controls. Blink stays untouched upstream.
        const FName TargetName = BP->GetFName() == TEXT("ABP_Sa") ? TEXT("AnimGraphNode_LocalToComponentSpace_3") : TEXT("AnimGraphNode_LocalToComponentSpace_0");
        UEdGraphPin* Input = nullptr;
        for (UEdGraphNode* N : Graph->Nodes) if (N->GetFName() == TargetName) Input = N->FindPin(TEXT("LocalPose"));
        check(Input && Input->LinkedTo.Num() == 1);
        auto* Previous = Input->LinkedTo[0];
        Jaw = NewObject<UAnimGraphNode_NarrativeJaw>(Graph);
        Graph->AddNode(Jaw, false, false);
        Jaw->CreateNewGuid(); Jaw->PostPlacedNewNode();
        Jaw->NodeComment = Marker; Jaw->bCommentBubbleVisible = true;
        Jaw->NodePosX = Input->GetOwningNode()->NodePosX - 320;
        Jaw->NodePosY = Input->GetOwningNode()->NodePosY;
        Jaw->Node.Sequence = Anim;
        Jaw->Node.JawBone.BoneName = TEXT("c_jawbone_x");
        Jaw->AllocateDefaultPins();
        Input->BreakAllPinLinks();
        check(Graph->GetSchema()->TryCreateConnection(Previous, Jaw->FindPinChecked(TEXT("SourcePose"))));
        check(Graph->GetSchema()->TryCreateConnection(Jaw->FindPinChecked(TEXT("Pose")), Input));
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FCompilerResultsLog Log;
        FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipSave, &Log);
        check(Log.NumErrors == 0);
        Save(BP);
    }
    check(Jaw && Jaw->Node.Sequence == Anim && Jaw->Node.JawBone.BoneName == TEXT("c_jawbone_x"));
    check(Jaw->FindPinChecked(TEXT("SourcePose"))->LinkedTo.Num() == 1 && Jaw->FindPinChecked(TEXT("Pose"))->LinkedTo.Num() == 1);
    UE_LOG(LogTemp, Display, TEXT("JAW GRAPH PASS %s: one connected jaw-only layer"), *BP->GetName());
}
bool Test()
{
    TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
    auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("JawRegression"), nullptr, true, ERHIFeatureLevel::Num, &Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    int32 Fail = 0;
    auto Expect = [&](bool Pass, const FString& Label) { Fail += !Pass; UE_LOG(LogTemp, Display, TEXT("JAW TEST %s: %s"), Pass ? TEXT("PASS") : TEXT("FAIL"), *Label); };
    Expect(UNarrativeJawComponent::CountVisibleCharacters(TEXT(" <b>你好</> \n")) == 2, TEXT("visible character count excludes whitespace and rich text tags"));
    Expect(UNarrativeJawComponent::CountVisibleCharacters(TEXT(" \t\n")) == 0, TEXT("whitespace has no speaking duration"));
    Expect(UNarrativeJawComponent::CountVisibleCharacters(TEXT("...")) == 0, TEXT("ASCII ellipsis is silent"));
    Expect(UNarrativeJawComponent::CountVisibleCharacters(TEXT("……？！——，。；：、（）【】《》「」『』“”‘’")) == 0, TEXT("Chinese punctuation, quotes, brackets and dashes are silent"));
    Expect(UNarrativeJawComponent::CountVisibleCharacters(TEXT("!?.,:;-'\"()[]{}")) == 0, TEXT("English punctuation is silent"));
    Expect(UNarrativeJawComponent::CountVisibleCharacters(TEXT("你好……")) == 2, TEXT("trailing punctuation never adds speaking characters"));
    Expect(UNarrativeJawComponent::CountVisibleCharacters(TEXT("“真的吗？”")) == 3, TEXT("quoted dialogue counts only its text"));
    Expect(UNarrativeJawComponent::CountVisibleCharacters(TEXT("Hello, Sa! 123。你好")) == 12, TEXT("Chinese, English and digits remain counted"));
    Expect(UNarrativeJawComponent::CountVisibleCharacters(TEXT("<b>……</> \n <i>？！</>")) == 0, TEXT("rich-text punctuation-only lines are silent"));
    Expect(UNarrativeJawComponent::CountVisibleCharacters(TEXT("<b>你好</>，<i>Sa</>！")) == 4, TEXT("rich text tags and punctuation do not extend speech"));
    Expect(UNarrativeJawComponent::CountVisibleCharacters(TEXT("１２３，ＡＢＣ！")) == 6, TEXT("full-width digits and letters are preserved"));
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    {
        auto* Owner = World->SpawnActor<AActor>();
        auto* State = NewObject<UNarrativeJawComponent>(Owner); State->RegisterComponent();
        auto* Dialogue = NewObject<UDialogue>(World);
        Expect(FMath::IsNearlyEqual(Dialogue->JawDurationMultiplier, 0.25f), TEXT("new dialogue duration multiplier defaults to 0.25"));
        State->Start(Dialogue, 5.f);
        FNarrativeJawPlayback Previous; State->SamplePlayback(1.f / 3.f, Previous);
        bool InRange = true, StableWithinLoop = true, ContinuousAmplitude = true, Repeatable = true;
        int32 RateChanges = 0;
        for (int32 I = 0; I < 900; ++I)
        {
            World->Tick(LEVELTICK_TimeOnly, 0.005f);
            FNarrativeJawPlayback Current, Repeated;
            InRange &= State->SamplePlayback(1.f / 3.f, Current);
            State->SamplePlayback(1.f / 3.f, Repeated);
            Repeatable &= Current.Position == Repeated.Position && Current.PlayRate == Repeated.PlayRate && Current.Amplitude == Repeated.Amplitude;
            InRange &= Current.PlayRate >= 0.7f && Current.PlayRate <= 1.2f && Current.Amplitude >= 0.5f && Current.Amplitude <= 1.f;
            if (Current.LoopIndex == Previous.LoopIndex) StableWithinLoop &= Current.PlayRate == Previous.PlayRate && Current.Position >= Previous.Position;
            else if (!FMath::IsNearlyEqual(Current.PlayRate, Previous.PlayRate)) ++RateChanges;
            ContinuousAmplitude &= FMath::Abs(Current.Amplitude - Previous.Amplitude) < 0.025f;
            Previous = Current;
        }
        Expect(InRange && RateChanges >= 5, TEXT("each loop randomizes speed within 0.7-1.2 and amplitude within 0.5-1"));
        Expect(StableWithinLoop, TEXT("speed stays constant inside each loop and animation phase never jumps backward"));
        Expect(ContinuousAmplitude, TEXT("amplitude remains continuous across loop boundaries"));
        Expect(Repeatable, TEXT("multiple mesh evaluations at the same world time never advance or rerandomize playback"));
        World->Tick(LEVELTICK_TimeOnly, 0.1f); World->Tick(LEVELTICK_TimeOnly, 0.1f); World->Tick(LEVELTICK_TimeOnly, 0.1f);
        FNarrativeJawPlayback AfterHitch;
        Expect(State->SamplePlayback(1.f / 3.f, AfterHitch) && AfterHitch.LoopIndex >= Previous.LoopIndex, TEXT("larger time steps preserve loop progression"));
        for (int32 I = 0; I < 4; ++I) World->Tick(LEVELTICK_TimeOnly, 0.1f);
        Expect(!State->SamplePlayback(1.f / 3.f, AfterHitch), TEXT("random speed never extends the total speaking duration"));
        Owner->Destroy();
    }
    for (const TCHAR* Path : {TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"), TEXT("/Game/Characters/Sa/BP_Cine_Sa")})
    {
        auto* BP = LoadObject<UBlueprint>(nullptr, Path);
        check(BP && BP->GeneratedClass);
        auto* Actor = World->SpawnActor<AActor>(BP->GeneratedClass, FTransform::Identity, Spawn);
        auto* Character = Cast<ACharacter>(Actor);
        auto* Mesh = Character ? Character->GetMesh() : Actor->FindComponentByClass<USkeletalMeshComponent>();
        check(Mesh);
        Mesh->InitAnim(true);
        auto* Instance = Mesh->GetAnimInstance();
        check(Instance);
        if (auto* Camera = Cast<UVTGCameraLookAnimInstance>(Instance)) Camera->bCameraLookEnabled = false;
        auto SetBool = [](UObject* Object, const TCHAR* Name, bool Value) { if (auto* P = FindFProperty<FBoolProperty>(Object->GetClass(), Name)) P->SetPropertyValue_InContainer(Object, Value); };
        SetBool(Instance, TEXT("IsCutscene"), false);
        SetBool(Instance, TEXT("AutoBlink?"), true);
        for (const TCHAR* Name : {TEXT("UsingStunGun"), TEXT("UsingSMG1"), TEXT("IsDead?"), TEXT("HidePlayerNCam"), TEXT("IsInCineCutscene_LockInput(NotWorkForMont)"), TEXT("IsReadingMail")}) SetBool(Actor, Name, false);
        auto* Comp = NewObject<UNarrativeComponent>(Actor); Comp->RegisterComponent();
        auto* Dialogue = NewObject<UDialogue>(World); Dialogue->OwningComp = Comp;
        auto* Node = NewObject<UDialogueNode_NPC>(Dialogue);
        FDialogueLine Line; Line.Text = FText::FromString(TEXT("这是一句测试台词，嘴部动画必须只影响下颌骨骼。"));
        FSpeakerInfo Speaker; Speaker.SpeakerID = TEXT("TestSpeaker"); Speaker.SpeakingActorTag = TEXT("JawRegressionActor");
        Actor->Tags.Add(Speaker.SpeakingActorTag);
        auto Start = [&]() { Dialogue->StartJawMovement(Node, Line, Speaker); };
        float Time = 0, Weight = 0;
        Start(); auto* State = Actor->FindComponentByClass<UNarrativeJawComponent>(); check(State);
        Expect(State->Sample(Time, Weight), FString(Path) + TEXT(" resolves speaker Actor Tag"));
        Dialogue->StopJawMovement(); Actor->Tags.Remove(Speaker.SpeakingActorTag);
        Mesh->ComponentTags.Add(Speaker.SpeakingActorTag); Start();
        Expect(State->Sample(Time, Weight), FString(Path) + TEXT(" resolves skeletal mesh component tag without an actor tag"));
        Mesh->ComponentTags.Remove(Speaker.SpeakingActorTag); Actor->Tags.Add(Speaker.SpeakingActorTag);
        Dialogue->StopJawMovement(); Expect(!State->Sample(Time, Weight), TEXT("skip/stop immediately cancels playback"));
        Node->bEnableJawMovement = false; Start(); Expect(!State->Sample(Time, Weight), TEXT("node false disables jaw")); Node->bEnableJawMovement = true;
        Dialogue->bEnableJawMovement = false; Start(); Expect(!State->Sample(Time, Weight), TEXT("global false overrides enabled node")); Dialogue->bEnableJawMovement = true;
        const FText Text = Line.Text; Line.Text = FText::FromString(TEXT(" \t\n")); Start(); Expect(!State->Sample(Time, Weight), TEXT("blank line is silent")); Line.Text = Text;
        Start(); Line.Text = FText::FromString(TEXT("<b>……</>？！——")); Start();
        Expect(!State->Sample(Time, Weight), TEXT("punctuation-only next line cancels previous speech and never starts jaw playback"));
        Line.Text = Text;
        auto* Duplicate = World->SpawnActor<AActor>(); Duplicate->Tags.Add(Speaker.SpeakingActorTag); Start(); Expect(!State->Sample(Time, Weight), TEXT("duplicate tags never animate an arbitrary actor")); Duplicate->Destroy();
        auto Frame = [&]() { Mesh->TickAnimation(0.f, false); Mesh->RefreshBoneTransforms(); };
        Frame(); Frame();
        const int32 JawIndex = Mesh->GetBoneIndex(TEXT("c_jawbone_x")); check(JawIndex != INDEX_NONE);
        const auto Baseline = Mesh->GetBoneSpaceTransforms();
        Start(); World->Tick(LEVELTICK_TimeOnly, 0.12f); Frame();
        const auto Speaking = Mesh->GetBoneSpaceTransforms();
        Expect(!Baseline[JawIndex].Equals(Speaking[JawIndex], 0.001f), FString(Path) + TEXT(" evaluated jaw actually moves"));
        int32 OtherChanges = 0;
        for (int32 I = 0; I < Baseline.Num(); ++I) if (I != JawIndex && !Baseline[I].Equals(Speaking[I], 0.001f)) ++OtherChanges;
        Expect(OtherChanges == 0, FString(Path) + FString::Printf(TEXT(" other local bones unchanged including eyes (changed=%d)"), OtherChanges));
        auto* JawAnim = LoadObject<UAnimSequence>(nullptr, AssetPath);
        auto* Montage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(JawAnim, TEXT("FullBodySlot"), 0.f, 0.f, 1.f);
        Expect(Instance->Montage_Play(Montage) > 0, TEXT("montage can start while dialogue jaw is active"));
        Mesh->TickAnimation(0.08f, false); Mesh->RefreshBoneTransforms();
        Expect(Instance->Montage_IsPlaying(Montage), TEXT("jaw module does not interrupt the montage"));
        Expect(Mesh->GetBoneSpaceTransforms()[JawIndex].Equals(Speaking[JawIndex], 0.001f), TEXT("jaw output takes priority over montage jaw tracks"));
        Instance->Montage_Stop(0.f); Frame();
        Dialogue->StopJawMovement(); Frame();
        Expect(Mesh->GetBoneSpaceTransforms()[JawIndex].Equals(Baseline[JawIndex], 0.001f), TEXT("cancel restores upstream jaw pose"));
        Dialogue->JawMinPlayRate = Dialogue->JawMaxPlayRate = 1.f;
        Dialogue->JawMinAmplitude = Dialogue->JawMaxAmplitude = 1.f;
        Start(); Frame();
        const FQuat ClosedRotation = Mesh->GetBoneSpaceTransforms()[JawIndex].GetRotation();
        World->Tick(LEVELTICK_TimeOnly, 0.12f); Frame();
        const FQuat FullRotation = Mesh->GetBoneSpaceTransforms()[JawIndex].GetRotation();
        Dialogue->JawMinAmplitude = Dialogue->JawMaxAmplitude = 0.5f;
        Start(); World->Tick(LEVELTICK_TimeOnly, 0.12f); Frame();
        const FQuat HalfRotation = Mesh->GetBoneSpaceTransforms()[JawIndex].GetRotation();
        Expect(FMath::IsNearlyEqual(ClosedRotation.AngularDistance(HalfRotation), ClosedRotation.AngularDistance(FullRotation) * 0.5f, 0.002f), TEXT("0.5 strength halves actual jaw opening relative to the closed pose"));
        Montage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(JawAnim, TEXT("FullBodySlot"), 0.f, 0.f, 1.f);
        Instance->Montage_Play(Montage); Mesh->TickAnimation(0.08f, false); Mesh->RefreshBoneTransforms();
        Expect(Mesh->GetBoneSpaceTransforms()[JawIndex].GetRotation().Equals(HalfRotation, 0.001f), TEXT("reduced amplitude still overrides montage jaw instead of blending montage motion back in"));
        Instance->Montage_Stop(0.f); Dialogue->StopJawMovement(); Frame();
        Start(); SetBool(Instance, TEXT("IsCutscene"), true); Frame();
        Expect(!State->Sample(Time, Weight), TEXT("entering cutscene cancels active speech"));
        SetBool(Instance, TEXT("IsCutscene"), false); Frame(); Expect(!State->Sample(Time, Weight), TEXT("leaving cutscene never resumes stale speech"));
        Start(); for (int32 I = 0; I < 100; ++I) World->Tick(LEVELTICK_TimeOnly, 0.1f);
        Expect(!State->Sample(Time, Weight), TEXT("duration expires without stale timer callbacks"));
        Dialogue->JawDurationMultiplier = 0.f; Start(); Expect(!State->Sample(Time, Weight), TEXT("zero multiplier is silent"));
        Dialogue->JawDurationMultiplier = 0.25f;
        const FName Tag = Speaker.SpeakingActorTag; Speaker.SpeakingActorTag = NAME_None; Start(); Expect(!State->Sample(Time, Weight), TEXT("empty binding is opt out")); Speaker.SpeakingActorTag = Tag;
        Start(); Dialogue->Deinitialize(); Expect(!State->Sample(Time, Weight), TEXT("dialogue deinitialize clears speech"));
        Actor->Destroy();
    }
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    return Fail == 0;
}
}
UVTGJawMovementCommandlet::UVTGJawMovementCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}
extern int32 VTGRunNPCJawMovement(const FString& Params);
extern int32 VTGMoveSaHeadLookAfterSlot(const FString& Params);
int32 UVTGJawMovementCommandlet::Main(const FString& Params)
{
    if (FParse::Param(*Params, TEXT("SaHeadLookAfterSlot"))) return VTGMoveSaHeadLookAfterSlot(Params);
    if (FParse::Param(*Params, TEXT("NPC"))) return VTGRunNPCJawMovement(Params);
    const FString Folder = FPaths::ProjectDir() / TEXT("Scripts/JawMovement");
    IFileManager::Get().MakeDirectory(*Folder, true);
    if (Params.Contains(TEXT("UpdateDefaults")) || Params.Contains(TEXT("VerifyDefaults")))
    {
        const bool ReadOnly = Params.Contains(TEXT("VerifyDefaults"));
        auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        Registry.SearchAllAssets(true);
        FARFilter Filter; Filter.PackagePaths.Add(TEXT("/Game")); Filter.bRecursivePaths = true;
        Filter.ClassPaths.Add(UDialogueBlueprint::StaticClass()->GetClassPathName()); Filter.bRecursiveClasses = true;
        TArray<FAssetData> Assets; Registry.GetAssets(Filter, Assets);
        int32 Updated = 0, Defaults = 0, Custom = 0, Old = 0;
        for (const auto& Asset : Assets)
        {
            auto* BP = Cast<UDialogueBlueprint>(Asset.GetAsset());
            if (!BP || !BP->GeneratedClass || !BP->GeneratedClass->IsChildOf(UDialogue::StaticClass())) continue;
            auto* Default = CastChecked<UDialogue>(BP->GeneratedClass->GetDefaultObject());
            TArray<UDialogue*> Configs = {Default, BP->DialogueTemplate};
            if (auto* Class = Cast<UDialogueBlueprintGeneratedClass>(BP->GeneratedClass)) Configs.AddUnique(Class->GetDialogueTemplate());
            bool NeedsUpdate = false;
            for (auto* Config : Configs) if (Config && FMath::IsNearlyEqual(Config->JawDurationMultiplier, 0.5f)) NeedsUpdate = true;
            if (NeedsUpdate)
            {
                ++Old;
                if (!ReadOnly)
                {
                    const FString Package = BP->GetOutermost()->GetName();
                    const FString File = FPackageName::LongPackageNameToFilename(Package, FPackageName::GetAssetPackageExtension());
                    const FString Backup = Folder / TEXT("DefaultsBackup") / (Package.Mid(6) + TEXT(".uasset"));
                    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
                    if (!IFileManager::Get().FileExists(*Backup)) check(IFileManager::Get().Copy(*Backup, *File) == COPY_OK);
                    for (auto* Config : Configs) if (Config && FMath::IsNearlyEqual(Config->JawDurationMultiplier, 0.5f)) Config->JawDurationMultiplier = 0.25f;
                    FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
                    JawEditor::Save(BP); ++Updated;
                    UE_LOG(LogTemp, Display, TEXT("JAW DEFAULT updated %s from 0.5 to 0.25"), *Package);
                }
            }
            if (FMath::IsNearlyEqual(Default->JawDurationMultiplier, 0.25f)) ++Defaults;
            else { ++Custom; UE_LOG(LogTemp, Display, TEXT("JAW DEFAULT preserved custom %s = %.4f"), *BP->GetPathName(), Default->JawDurationMultiplier); }
        }
        UE_LOG(LogTemp, Display, TEXT("JAW DEFAULTS %s assets=%d updated=%d default025=%d custom=%d old05=%d"), ReadOnly && Old ? TEXT("FAIL") : TEXT("PASS"), Assets.Num(), Updated, Defaults, Custom, Old);
        if (ReadOnly && Old) return 1;
        if (!Params.Contains(TEXT("Test"))) return 0;
    }
    if (Params.Contains(TEXT("DiagnoseBase")))
    {
        auto Export = [&](UObject* Object, const FString& Name)
        {
            if (!Object) return;
            FStringOutputDevice Out;
            UExporter::ExportToOutputDevice(nullptr, Object, nullptr, Out, TEXT("copy"), 0, PPF_Copy, false);
            FFileHelper::SaveStringToFile(Out, *(Folder / (Name + TEXT(".copy"))));
        };
        auto* BP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Narrative/Dialogues/Levels/Base")); check(BP);
        Export(BP, TEXT("Base"));
        auto Dump = [](UDialogue* Dialogue, const TCHAR* Label)
        {
            if (!Dialogue) return;
            UE_LOG(LogTemp, Display, TEXT("BASE JAW %s enabled=%d multiplier=%f root=%s"), Label, Dialogue->bEnableJawMovement, Dialogue->JawDurationMultiplier, *GetNameSafe(Dialogue->RootDialogue));
            for (const auto& S : Dialogue->Speakers) UE_LOG(LogTemp, Display, TEXT("BASE JAW speaker %s tag=%s"), *S.SpeakerID.ToString(), *S.SpeakingActorTag.ToString());
            UE_LOG(LogTemp, Display, TEXT("BASE JAW player tag=%s"), *Dialogue->PlayerSpeakerInfo.SpeakingActorTag.ToString());
        };
        Dump(Cast<UDialogue>(BP->GeneratedClass->GetDefaultObject()), TEXT("CDO"));
        auto* BG = Cast<UDialogueBlueprintGeneratedClass>(BP->GeneratedClass); check(BG);
        Dump(BG->GetDialogueTemplate(), TEXT("Template"));
        auto* Instance = NewObject<UDialogue>(GetTransientPackage(), BP->GeneratedClass);
        BG->InitializeDialogue(Instance);
        Dump(Instance, TEXT("Instance"));
        for (auto* Node : Instance->NPCReplies) if (Node) UE_LOG(LogTemp, Display, TEXT("BASE JAW node=%s speaker=%s enabled=%d text=%s"), *Node->GetName(), *Node->GetSpeakerID().ToString(), Node->bEnableJawMovement, *Node->Line.Text.ToString());
        auto* Map = LoadObject<UWorld>(nullptr, TEXT("/Game/ThirdPerson/Maps/BaseLevel")); check(Map);
        Export(Map->PersistentLevel->GetLevelScriptBlueprint(true), TEXT("BaseLevel"));
        for (AActor* Actor : Map->PersistentLevel->Actors)
        {
            if (!Actor) continue;
            if (Actor->GetName().Contains(TEXT("Sa")))
            {
                FString Tags; for (FName Tag : Actor->Tags) Tags += Tag.ToString() + TEXT(",");
                UE_LOG(LogTemp, Display, TEXT("BASE JAW actor=%s class=%s tags=%s"), *Actor->GetName(), *Actor->GetClass()->GetPathName(), *Tags);
                TArray<USkeletalMeshComponent*> Meshes; Actor->GetComponents(Meshes);
                for (auto* Mesh : Meshes) UE_LOG(LogTemp, Display, TEXT("BASE JAW mesh=%s animclass=%s"), *Mesh->GetName(), *GetPathNameSafe(Mesh->GetAnimClass()));
            }
        }
        return 0;
    }
    if (Params.Contains(TEXT("PlayBase")))
    {
        TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
        auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
        auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("BaseJawPlayback"), nullptr, true, ERHIFeatureLevel::Num, &Init);
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* BP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));
        auto* Player = World->SpawnActor<ACharacter>(BP->GeneratedClass, FTransform::Identity, Spawn);
        auto* Controller = World->SpawnActor<APlayerController>(); Controller->Possess(Player);
        FString Tags; for (FName Tag : Player->Tags) Tags += Tag.ToString() + TEXT(",");
        UE_LOG(LogTemp, Display, TEXT("BASE PLAY player tags=%s"), *Tags);
        TArray<UActorComponent*> PlayerComponents; Player->GetComponents(PlayerComponents);
        for (auto* C : PlayerComponents)
        {
            FString ComponentTags; for (FName Tag : C->ComponentTags) ComponentTags += Tag.ToString() + TEXT(",");
            if (!ComponentTags.IsEmpty()) UE_LOG(LogTemp, Display, TEXT("BASE PLAY component=%s tags=%s"), *C->GetName(), *ComponentTags);
        }
        auto* Mesh = Player->GetMesh(); Mesh->InitAnim(true);
        auto* Anim = Mesh->GetAnimInstance();
        auto* Cutscene = FindFProperty<FBoolProperty>(Anim->GetClass(), TEXT("IsCutscene"));
        UE_LOG(LogTemp, Display, TEXT("BASE PLAY anim=%s cutscene=%d"), *Anim->GetClass()->GetPathName(), Cutscene && Cutscene->GetPropertyValue_InContainer(Anim));
        auto* Component = NewObject<UNarrativeComponent>(Controller); Component->RegisterComponent();
        auto* BaseBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Narrative/Dialogues/Levels/Base")); check(BaseBP);
        const bool Began = Component->BeginDialogue(TSubclassOf<UDialogue>(BaseBP->GeneratedClass.Get()));
        UE_LOG(LogTemp, Display, TEXT("BASE PLAY began=%d dialogue=%s"), Began, *GetNameSafe(Component->CurrentDialogue));
        const int32 JawIndex = Mesh->GetBoneIndex(TEXT("c_jawbone_x"));
        Mesh->TickAnimation(0.f, false); Mesh->RefreshBoneTransforms();
        const FTransform InitialJaw = Mesh->GetBoneSpaceTransforms()[JawIndex];
        bool SawActive = false, SawMovement = false;
        for (int I = 0; I < 12; ++I)
        {
            World->Tick(LEVELTICK_TimeOnly, 0.1f);
            Mesh->TickAnimation(0.f, false); Mesh->RefreshBoneTransforms();
            auto* State = Player->FindComponentByClass<UNarrativeJawComponent>(); float Time = 0, Weight = 0;
            const bool Active = State && State->Sample(Time, Weight);
            SawActive |= Active;
            SawMovement |= !InitialJaw.Equals(Mesh->GetBoneSpaceTransforms()[JawIndex], 0.001f);
            UE_LOG(LogTemp, Display, TEXT("BASE PLAY frame=%d state=%d active=%d age=%.3f weight=%.3f cutscene=%d jaw=%s"), I, State != nullptr, Active, Time, Weight, Cutscene && Cutscene->GetPropertyValue_InContainer(Anim), *Mesh->GetBoneSpaceTransforms()[JawIndex].ToString());
        }
        Component->ExitDialogue();
        World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
        UE_LOG(LogTemp, Display, TEXT("BASE PLAY %s: saved Base dialogue drives actual SaP mesh tag and moves the evaluated jaw"), Began && SawActive && SawMovement ? TEXT("PASS") : TEXT("FAIL"));
        return Began && SawActive && SawMovement ? 0 : 1;
    }
    for (const TCHAR* Path : {TEXT("/Game/Characters/Sa/ABP_Sa"), TEXT("/Game/Characters/Sa/ABP_SaCine")})
    {
        auto* BP = LoadObject<UAnimBlueprint>(nullptr, Path);
        check(BP && BP->TargetSkeleton);
        if (Params.Contains(TEXT("Inspect"))) JawEditor::Inspect(BP, Folder);
        const bool VerifyOnly = Params.Contains(TEXT("VerifyOnly"));
        UAnimSequence* Anim = VerifyOnly ? LoadObject<UAnimSequence>(nullptr, JawEditor::AssetPath) : JawEditor::Import(BP->TargetSkeleton);
        check(Anim);
        if (Params.Contains(TEXT("Inspect"))) JawEditor::Audit(Anim);
        else
        {
            if (!VerifyOnly) JawEditor::Clean(Anim, Folder);
            TArray<FName> Names;
            Anim->GetDataModel()->GetBoneTrackNames(Names);
            check(Names.Num() == 1 && Names[0] == TEXT("c_jawbone_x"));
            check(Anim->GetDataModel()->GetNumberOfFloatCurves() == 0 && Anim->GetDataModel()->GetNumberOfTransformCurves() == 0);
            TArray<FTransform> Keys;
            Anim->GetDataModel()->GetBoneTrackTransforms(Names[0], Keys);
            check(Keys.Num() > 1 && Keys[0].Equals(Keys.Last(), 0.001f));
            UE_LOG(LogTemp, Display, TEXT("JAW ASSET PASS: jaw-only, no curves, matching loop endpoints, duration %.3fs"), Anim->GetPlayLength());
            JawEditor::Apply(BP, Anim, Folder, VerifyOnly);
        }
    }
    return Params.Contains(TEXT("Test")) && !JawEditor::Test() ? 1 : 0;
}
