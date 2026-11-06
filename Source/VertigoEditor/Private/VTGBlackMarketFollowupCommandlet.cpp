#include "VTGBlackMarketFollowupCommandlet.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/VTGCameraLookAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "DialogueBlueprint.h"
#include "Dialogue.h"
#include "DialogueSM.h"
#include "EdGraph/EdGraph.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Editor.h"
#include "NarrativeComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "VTGLevelManagerBase.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/MorphTarget.h"
#include "Rendering/SkeletalMeshModel.h"
#include "AnimNodes/AnimNode_LayeredBoneBlend.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "Save/VTGL2FightStateComponent.h"
#include "Combat/VTGSewerEncounterComponent.h"

namespace MarketFollowup
{
const FString Folder=TEXT("Scripts/BlackMarketFollowup/");
void Dump(UBlueprint* BP)
{
 check(BP);FString Out;
 auto Properties=[&](UObject* O)
 {
  Out+=TEXT("OBJECT ")+O->GetPathName()+TEXT("\n");
  for(TFieldIterator<FProperty> P(O->GetClass());P;++P)
  {
   if(P->HasAnyPropertyFlags(CPF_Transient))continue;
   FString V;P->ExportText_InContainer(0,V,O,O,O,PPF_None);if(V.Len()<5000)Out+=P->GetName()+TEXT("=")+V+TEXT("\n");
  }
 };
 Properties(BP->GeneratedClass->GetDefaultObject());
 TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
 for(auto* G:Graphs)
 {
  Out+=TEXT("\nGRAPH ")+G->GetName()+TEXT("\n");
  for(UEdGraphNode* N:G->Nodes)
  {
   Out+=N->GetName()+TEXT(" | ")+N->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Replace(TEXT("\n"),TEXT(" "))+TEXT(" | ")+N->NodeComment+TEXT("\n");
   for(auto* P:N->Pins)
   {
    Out+=TEXT("  ")+P->PinName.ToString()+(P->Direction==EGPD_Input?TEXT(" IN "):TEXT(" OUT "))+P->DefaultValue+TEXT(" ")+GetPathNameSafe(P->DefaultObject);
    for(auto* L:P->LinkedTo)Out+=TEXT(" -> ")+L->GetOwningNode()->GetName()+TEXT(".")+L->PinName.ToString();Out+=TEXT("\n");
   }
  }
 }
 if(auto* DBP=Cast<UDialogueBlueprint>(BP))
  for(auto* N:DBP->DialogueTemplate->GetNodes())
   if(N->GetName()==TEXT("DialogueNode_NPC_331") || N->GetName()==TEXT("DialogueNode_NPC_333"))
   {Properties(N);for(auto* E:N->Events)Properties(E);}
 check(FFileHelper::SaveStringToFile(Out,*(FPaths::ProjectDir()/Folder/(BP->GetName()+TEXT(".graph.txt")))));
}
void Bool(UObject* O,const TCHAR* N,bool V){auto* P=FindFProperty<FBoolProperty>(O->GetClass(),N);check(P);P->SetPropertyValue_InContainer(O,V);}
void Obj(UObject* O,const TCHAR* N,UObject* V){auto* P=FindFProperty<FObjectPropertyBase>(O->GetClass(),N);check(P);P->SetObjectPropertyValue_InContainer(O,V);}
void Injured()
{
 FEditorScriptExecutionGuard Guard;
 auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("SaInjuredDiagnosis"),nullptr,true,ERHIFeatureLevel::Num,&Init);check(W);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* Player=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));check(Player);
 FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 FString Report;
 for(int Case=0;Case<6;++Case)
 {
  auto* Sa=W->SpawnActor<ACharacter>(Player->GeneratedClass,FTransform::Identity,Spawn);check(Sa);
  auto* M=Sa->GetMesh();M->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;M->bEnableUpdateRateOptimizations=false;M->InitAnim(true);
  auto* A=CastChecked<UVTGCameraLookAnimInstance>(M->GetAnimInstance());A->bCameraLookEnabled=false;
  for(const TCHAR* N:{TEXT("StoryMode?"),TEXT("HidePlayerNCam"),TEXT("IsInCineCutscene_LockInput(NotWorkForMont)"),TEXT("IsDead?"),TEXT("IsAttacking"),TEXT("UsingStunGun"),TEXT("UsingSMG1"),TEXT("IsReadingMail")})Bool(Sa,N,false);
  Bool(Sa,TEXT("ShowLowHealth?"),Case!=0);Bool(Sa,TEXT("InCombat"),Case==1);Bool(A,TEXT("IsCutscene"),false);Obj(A,TEXT("As Sa"),Sa);Obj(A,TEXT("MovementComponent"),Sa->GetCharacterMovement());
  FindFProperty<FIntProperty>(Sa->GetClass(),TEXT("Health"))->SetPropertyValue_InContainer(Sa,Case==0?100:10);
  M->SetDisablePostProcessBlueprint(Case>=3);
  if(Case>=4)
   for(TFieldIterator<FStructProperty> It(A->GetClass());It;++It)if(It->Struct==FAnimNode_LayeredBoneBlend::StaticStruct())
   {
    auto* B=It->ContainerPtrToValuePtr<FAnimNode_LayeredBoneBlend>(A);
    if(B->BlendWeights.Num()==1 && FMath::IsNearlyEqual(B->BlendWeights[0],.7f))
    {
     if(Case==4)B->bMeshSpaceRotationBlend=false;else B->BlendWeights[0]=1.f;
     Report+=FString::Printf(TEXT("EXPERIMENT case=%d node=%s localRotation=%d fullWeight=%d\n"),Case,*It->GetName(),Case==4,Case==5);
    }
   }
  const auto& Ref=M->GetSkeletalMeshAsset()->GetRefSkeleton();
  TArray<FTransform> RefCS=Ref.GetRefBonePose();for(int B=1;B<RefCS.Num();++B)RefCS[B]*=RefCS[Ref.GetParentIndex(B)];
  struct FPalmVertex{FVector Position;TArray<TPair<int,double>> Influences;};TArray<FPalmVertex> PalmVertices;
  const int PalmHand=Ref.FindBoneIndex(TEXT("hand_l"));auto* Imported=M->GetSkeletalMeshAsset()->GetImportedModel();check(Imported && Imported->LODModels.Num());
  for(const auto& Section:Imported->LODModels[0].Sections)for(const auto& V:Section.SoftVertices)
  {
   double HandWeight=0;for(int I=0;I<MAX_TOTAL_INFLUENCES;++I)if(V.InfluenceWeights[I] && Section.BoneMap[V.InfluenceBones[I]]==PalmHand)HandWeight+=double(V.InfluenceWeights[I])/65535.;
   if(HandWeight<.25)continue;auto& P=PalmVertices.AddDefaulted_GetRef();P.Position=FVector(V.Position);for(int I=0;I<MAX_TOTAL_INFLUENCES;++I)if(V.InfluenceWeights[I])P.Influences.Add({int(Section.BoneMap[V.InfluenceBones[I]]),double(V.InfluenceWeights[I])/65535.});
  }
  FVector PalmSpanMin(1.e20),PalmSpanMax(-1.e20);
  if(Case==0)
  {
   TMap<FName,double> Weights;int Count=0;const int Hand=Ref.FindBoneIndex(TEXT("hand_l"));
   auto* Model=M->GetSkeletalMeshAsset()->GetImportedModel();check(Model && Model->LODModels.Num());
   for(const auto& Section:Model->LODModels[0].Sections)for(const auto& V:Section.SoftVertices)
   {
    double HandWeight=0;
    for(int I=0;I<MAX_TOTAL_INFLUENCES;++I)if(V.InfluenceWeights[I] && Section.BoneMap[V.InfluenceBones[I]]==Hand)HandWeight+=double(V.InfluenceWeights[I])/65535.;
    if(HandWeight<.25)continue;++Count;
    for(int I=0;I<MAX_TOTAL_INFLUENCES;++I)if(V.InfluenceWeights[I])Weights.FindOrAdd(Ref.GetBoneName(Section.BoneMap[V.InfluenceBones[I]]))+=double(V.InfluenceWeights[I])/65535.;
   }
   Report+=FString::Printf(TEXT("PALM VERTEX COUNT %d (hand_l weight >=25%%)\n"),Count);
   for(const auto& Pair:Weights)Report+=FString::Printf(TEXT("PALM WEIGHT %s total=%.6f\n"),*Pair.Key.ToString(),Pair.Value);
  }
  TArray<FVector> Min,Max,First;Min.Init(FVector(1.e20),Ref.GetNum());Max.Init(FVector(-1.e20),Ref.GetNum());First.SetNum(Ref.GetNum());
  FString Csv=TEXT("frame,bone,sx,sy,sz,tx,ty,tz\n");
  TMap<FName,FVector2D> MorphRanges;
  for(UMorphTarget* Morph:M->GetSkeletalMeshAsset()->GetMorphTargets())MorphRanges.Add(Morph->GetFName(),FVector2D(1.e20,-1.e20));
  for(int Frame=0;Frame<240;++Frame)
  {
   M->TickAnimation(1.f/60,false);M->RefreshBoneTransforms();
   if(Frame>=60)for(auto& Pair:MorphRanges){float V=M->GetMorphTarget(Pair.Key);Pair.Value.X=FMath::Min(Pair.Value.X,double(V));Pair.Value.Y=FMath::Max(Pair.Value.Y,double(V));}
   const auto& Pose=M->GetBoneSpaceTransforms();
   if(Frame>=60)
   {
    const auto& CS=M->GetComponentSpaceTransforms();FVector PalmMin(1.e20),PalmMax(-1.e20);
    for(const auto& V:PalmVertices)
    {
     FVector P=FVector::ZeroVector;for(const auto& Influence:V.Influences)P+=CS[Influence.Key].TransformPosition(RefCS[Influence.Key].InverseTransformPosition(V.Position))*Influence.Value;
     P=CS[PalmHand].InverseTransformPosition(P);PalmMin=PalmMin.ComponentMin(P);PalmMax=PalmMax.ComponentMax(P);
    }
    PalmSpanMin=PalmSpanMin.ComponentMin(PalmMax-PalmMin);PalmSpanMax=PalmSpanMax.ComponentMax(PalmMax-PalmMin);
   }
   for(int B=0;B<Pose.Num();++B)
   {
    if(Frame<60)continue;
    const FVector S=Pose[B].GetScale3D();Min[B]=Min[B].ComponentMin(S);Max[B]=Max[B].ComponentMax(S);
    const FString Name=Ref.GetBoneName(B).ToString();
    if(Frame==60)First[B]=M->GetBoneTransform(B).GetLocation();
    if(Name.EndsWith(TEXT("_l")) && (Name.Contains(TEXT("hand")) || Name.Contains(TEXT("index")) || Name.Contains(TEXT("thumb"))))
     Csv+=FString::Printf(TEXT("%d,%s,%.9f,%.9f,%.9f,%.6f,%.6f,%.6f\n"),Frame,*Name,S.X,S.Y,S.Z,Pose[B].GetLocation().X,Pose[B].GetLocation().Y,Pose[B].GetLocation().Z);
   }
  }
  FString Head=FString::Printf(TEXT("CASE %d low=%d combat=%d disablePP=%d\n"),Case,FindFProperty<FBoolProperty>(A->GetClass(),TEXT("Low Health"))->GetPropertyValue_InContainer(A),Case==1,Case>=3);Report+=Head;UE_LOG(LogTemp,Display,TEXT("%s"),*Head);
  Report+=FString::Printf(TEXT("PALM SKIN SPAN hand-local min=%s max=%s variation=%s\n"),*PalmSpanMin.ToString(),*PalmSpanMax.ToString(),*(PalmSpanMax-PalmSpanMin).ToString());
  for(int B=0;B<Ref.GetNum();++B)
  {
   const FString Name=Ref.GetBoneName(B).ToString();
   if((Max[B]-Min[B]).GetMax()>.0001 || Name==TEXT("hand_l") || Name.Contains(TEXT("palm")))
    Report+=FString::Printf(TEXT("%s min=%s max=%s variation=%.8f movement=%.6f\n"),*Name,*Min[B].ToString(),*Max[B].ToString(),(Max[B]-Min[B]).GetMax(),FVector::Distance(First[B],M->GetBoneTransform(B).GetLocation()));
  }
  Report+=FString::Printf(TEXT("MORPH TARGET COUNT %d\n"),MorphRanges.Num());for(const auto& Pair:MorphRanges)if(FMath::Abs(Pair.Value.X)>.0001 || FMath::Abs(Pair.Value.Y)>.0001)Report+=FString::Printf(TEXT("MORPH %s min=%.6f max=%.6f\n"),*Pair.Key.ToString(),Pair.Value.X,Pair.Value.Y);
  for(auto* C:Sa->GetComponents())if(auto* S=Cast<UStaticMeshComponent>(C))Report+=FString::Printf(TEXT("ATTACHED %s mesh=%s socket=%s scale=%s visible=%d hidden=%d material=%s\n"),*S->GetName(),*GetPathNameSafe(S->GetStaticMesh()),*S->GetAttachSocketName().ToString(),*S->GetComponentScale().ToString(),S->IsVisible(),S->bHiddenInGame,*GetPathNameSafe(S->GetMaterial(0)));
  check(FFileHelper::SaveStringToFile(Csv,*(FPaths::ProjectDir()/Folder/FString::Printf(TEXT("injured-case%d.csv"),Case))));W->DestroyActor(Sa);
 }
 check(FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectDir()/Folder/TEXT("injured-report.txt"))));W->DestroyWorld(false);GEngine->DestroyWorldContext(W);
 UE_LOG(LogTemp,Display,TEXT("INJURED_RUNTIME_DIAGNOSIS_COMPLETE"));
}
void StyleProbe(bool Verify=false)
{
 FEditorScriptExecutionGuard Guard;
 if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
 auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("MarketStyleProbe"),nullptr,true,ERHIFeatureLevel::Num,&Init);auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);Context.SetCurrentWorld(W);auto* GI=NewObject<UGameInstance>();W->SetGameInstance(GI);Context.OwningGameInstance=GI;check(W->SetGameMode(FURL(nullptr,TEXT("/Engine/Maps/Entry?game=/Script/Engine.GameModeBase"),TRAVEL_Absolute)));
 auto* Player=LoadClass<ACharacter>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa_C"));
 FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* PC=W->SpawnActor<APlayerController>();W->AddController(PC);auto* Local=NewObject<ULocalPlayer>(GEngine);GI->AddLocalPlayer(Local,0);PC->SetPlayer(Local);auto* Sa=W->SpawnActor<ACharacter>(Player,FTransform::Identity,Spawn);PC->Possess(Sa);
 auto* NC=Sa->FindComponentByClass<UNarrativeComponent>();check(NC);
 auto* OC=LoadClass<UUserWidget>(nullptr,TEXT("/Game/Narrative/NarrativeUI/Widgets/BP_Narrative3Overlay.BP_Narrative3Overlay_C"));auto* UI=CreateWidget<UUserWidget>(PC,OC);check(UI);
 auto* Bind=UI->FindFunction(TEXT("Bind To Component"));check(Bind);FStructOnScope Args(Bind);FindFProperty<FObjectPropertyBase>(Bind,TEXT("Component"))->SetObjectPropertyValue_InContainer(Args.GetStructMemory(),NC);UI->ProcessEvent(Bind,Args.GetStructMemory());
 auto* D=LoadClass<UDialogue>(nullptr,TEXT("/Game/Narrative/Dialogues/Levels/L4BlackMarket/L4_BlackMarketInvestigation_Still_MainInvestigation.L4_BlackMarketInvestigation_Still_MainInvestigation_C"));check(D);
 auto* Name=FindFProperty<FNameProperty>(Sa->GetClass(),TEXT("DialogueStyle Story or Action or Cine"));auto* Enum=FindFProperty<FByteProperty>(Sa->GetClass(),TEXT("CurrentDialogueStyle"));check(Name && Enum);
 for(int ID:{331,333,335,328})
 {
  NC->ExitDialogue();Name->SetPropertyValue_InContainer(Sa,TEXT("Action"));Sa->ProcessEvent(Sa->FindFunctionChecked(TEXT("DialogueStyleChange")),nullptr);
  FDialoguePlayParams Params;Params.StartFromID=FName(*FString::Printf(TEXT("L4_BlackMarketInvestigation_Still_MainInvestigation_DialogueNode_NPC_%d"),ID));
  bool Started=NC->BeginDialogue(D,Params);
  UE_LOG(LogTemp,Display,TEXT("STYLE PROBE start=%d started=%d name=%s enum=%s dialogue=%s"),ID,Started,*Name->GetPropertyValue_InContainer(Sa).ToString(),*Enum->Enum->GetDisplayNameTextByValue(Enum->GetPropertyValue_InContainer(Sa)).ToString(),*GetNameSafe(NC->GetCurrentDialogue()));
  if(Verify && (ID==331 || ID==333)){check(Started && Name->GetPropertyValue_InContainer(Sa)==TEXT("Story"));check(Enum->Enum->GetDisplayNameTextByValue(Enum->GetPropertyValue_InContainer(Sa)).ToString()==TEXT("Story"));check(UI->GetWidgetFromName(TEXT("DialogueBox"))->GetVisibility()==ESlateVisibility::Visible && UI->GetWidgetFromName(TEXT("DialogueBox_Side"))->GetVisibility()==ESlateVisibility::Collapsed);}
  for(FName Widget:{TEXT("DialogueBox"),TEXT("DialogueBox_Side"),TEXT("AvtProfile_Main"),TEXT("AvtProfile_Side")})if(auto* V=UI->GetWidgetFromName(Widget))UE_LOG(LogTemp,Display,TEXT("STYLE PROBE widget %s=%d"),*Widget.ToString(),(int)V->GetVisibility());
 }
 if(Verify)
 {
  NC->ExitDialogue();Name->SetPropertyValue_InContainer(Sa,TEXT("Action"));Sa->ProcessEvent(Sa->FindFunctionChecked(TEXT("DialogueStyleChange")),nullptr);
  auto* MC=LoadClass<AActor>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L4BlackMarket/BPLM_BlackMarket.BPLM_BlackMarket_C"));auto* Manager=W->SpawnActor<AActor>(MC,FTransform::Identity,Spawn);Obj(Manager,TEXT("Player Pawn"),Sa);Obj(Manager,TEXT("Narrative"),NC);FindFProperty<FNameProperty>(MC,TEXT("CurrentStage"))->SetPropertyValue_InContainer(Manager,TEXT("L4MarketStage_AfterAdult"));
  Bool(Manager,TEXT("L4_MainDlg1Done"),false);Manager->ProcessEvent(Manager->FindFunctionChecked(TEXT("UndergroundLeave")),nullptr);
  for(int I=0;I<40;++I){W->GetLatentActionManager().BeginFrame();W->GetLatentActionManager().ProcessLatentActions(Manager,1.f/60.f);}
  check(NC->GetCurrentDialogue() && Name->GetPropertyValue_InContainer(Sa)==TEXT("Story"));
  check(UI->GetWidgetFromName(TEXT("DialogueBox"))->GetVisibility()==ESlateVisibility::Visible && UI->GetWidgetFromName(TEXT("DialogueBox_Side"))->GetVisibility()==ESlateVisibility::Collapsed);
  UE_LOG(LogTemp,Display,TEXT("STYLE TEST PASS: actual UndergroundLeave AfterAdult from prior Action shows Story main box; routing 331/333 preserve Story"));
 }
 NC->ExitDialogue();
 if(auto* Unbind=UI->FindFunction(TEXT("Unbind from Component"))){FStructOnScope P(Unbind);UI->ProcessEvent(Unbind,P.GetStructMemory());}
 UI->RemoveFromParent();W->GetLatentActionManager().RemoveActionsForObject(Sa);
 TArray<AActor*> Actors;for(AActor* A:W->PersistentLevel->Actors)if(A)Actors.Add(A);for(auto* A:Actors)W->DestroyActor(A);
 GI->RemoveLocalPlayer(Local);W->DestroyWorld(false);GEngine->DestroyWorldContext(W);
}
}
#include "VTGBlackMarketFollowupRepair.inl"
#include "VTGStreetSewerCheckpoints.inl"
#include "VTGStreetSewerCheckpointTests.inl"
UVTGBlackMarketFollowupCommandlet::UVTGBlackMarketFollowupCommandlet(){IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}
int32 VTGRepairCyanOrderFlow(bool bVerifyOnly);
int32 UVTGBlackMarketFollowupCommandlet::Main(const FString& Params)
{
 if(FParse::Param(*Params,TEXT("CyanOrderFlow")))return VTGRepairCyanOrderFlow(FParse::Param(*Params,TEXT("VerifyOnly")));
 using namespace MarketFollowup;
 if(FParse::Param(*Params,TEXT("EncounterApply")) || FParse::Param(*Params,TEXT("EncounterVerify")))return StreetSewerCheckpoints::Run(FParse::Param(*Params,TEXT("EncounterVerify")));
 if(FParse::Param(*Params,TEXT("EncounterAudit")))
 {
  for(const TCHAR* Path:{TEXT("/Game/LevelNSequence/In_Level/L2BarStreet/BPLM_L2StreetBarFight"),TEXT("/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt")})Dump(LoadObject<UBlueprint>(nullptr,Path));
  for(const TCHAR* Path:{TEXT("/Game/ThirdPerson/Maps/Street/L2_StreetOutsideBar"),TEXT("/Game/ThirdPerson/Maps/L_SewerUnderApartment")})
  {
   auto* W=LoadObject<UWorld>(nullptr,Path);check(W);FString Report;
   if(auto* LevelBP=W->PersistentLevel->GetLevelScriptBlueprint(true))Dump(LevelBP);
   for(AActor* A:W->PersistentLevel->Actors)if(A)
   {
    Report+=A->GetPathName()+TEXT(" class=")+A->GetClass()->GetPathName()+TEXT(" transform=")+A->GetActorTransform().ToString()+TEXT("\n");
    if(A->IsA<AVTGLevelManagerBase>() || A->GetClass()->GetName().Contains(TEXT("Checkpoint")))
     for(TFieldIterator<FProperty> P(A->GetClass());P;++P){FString V;P->ExportText_InContainer(0,V,A,A,A,PPF_None);if(V.Len()<5000)Report+=P->GetName()+TEXT("=")+V+TEXT("\n");}
    if(A->GetClass()->GetName().Contains(TEXT("Checkpoint")))for(auto* C:A->GetComponents())
     {Report+=TEXT("COMPONENT ")+C->GetName()+TEXT("\n");for(TFieldIterator<FProperty> P(C->GetClass());P;++P){FString V;P->ExportText_InContainer(0,V,C,C,C,PPF_None);if(V.Len()<5000)Report+=P->GetName()+TEXT("=")+V+TEXT("\n");}}
   }
   check(FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectDir()/Folder/(W->GetName()+TEXT(".actors.txt")))));
  }
  return 0;
 }
 if(FParse::Param(*Params,TEXT("Injured"))){Injured();return 0;}
 if(FParse::Param(*Params,TEXT("StyleProbe"))){StyleProbe();return 0;}
 if(FParse::Param(*Params,TEXT("Apply")) || FParse::Param(*Params,TEXT("VerifyOnly")))return MarketFollowup::Repair(FParse::Param(*Params,TEXT("VerifyOnly")));
 for(const TCHAR* Path:{TEXT("/Game/LevelNSequence/In_Level/L4BlackMarket/BPLM_BlackMarket"),TEXT("/Game/Blueprints/BPA_0SmartTrigger"),TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"),TEXT("/Game/Narrative/BP_Dialogue_VTG"),TEXT("/Game/Narrative/Dialogues/Levels/L4BlackMarket/L4_BlackMarketInvestigation_Still_MainInvestigation")})Dump(LoadObject<UBlueprint>(nullptr,Path));
 auto* Map=LoadObject<UWorld>(nullptr,TEXT("/Game/ThirdPerson/Maps/Street/BlackMarketStreet.BlackMarketStreet"));check(Map);
 for(AActor* Actor:Map->PersistentLevel->Actors)if(Actor && Actor->GetClass()->GetName()==TEXT("BPLM_BlackMarket_C"))
 {
  auto* P=FindFProperty<FObjectPropertyBase>(Actor->GetClass(),TEXT("L4_Trigger_Alley"));auto* T=P->GetObjectPropertyValue_InContainer(Actor);check(T);
  UE_LOG(LogTemp,Display,TEXT("FOLLOWUP trigger instance %s DoOnce=%d"),*T->GetPathName(),FindFProperty<FBoolProperty>(T->GetClass(),TEXT("DoOnce?"))->GetPropertyValue_InContainer(T));
 }
 return 0;
}


