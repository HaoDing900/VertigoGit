namespace StreetFlowFix
{
using namespace MarketFollowup;
const FString Root=TEXT("Scripts/StreetFlowFix/");
void SaveAsset(UObject* O)
{
 const FString File=FPackageName::LongPackageNameToFilename(O->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
 const FString Backup=FPaths::ProjectDir()/Root/TEXT("Backup")/(O->GetName()+TEXT(".uasset"));
 IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
 if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
 FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;check(UPackage::SavePackage(O->GetOutermost(),O,*File,A));
}
UActorComponent* BehaviorTemplate(UBlueprint* BP,bool Write)
{
 for(UClass* C=BP->GeneratedClass;C;C=C->GetSuperClass())if(auto* B=Cast<UBlueprint>(C->ClassGeneratedBy);B && B->SimpleConstructionScript)
  for(auto* N:B->SimpleConstructionScript->GetAllNodes())if(N->GetVariableName()==TEXT("AIBehaviorsComponent"))
  {if(B==BP || !Write)return N->GetActualComponentTemplate(CastChecked<UBlueprintGeneratedClass>(BP->GeneratedClass));auto* H=BP->GetInheritableComponentHandler(true);FComponentKey K(N);auto* O=H->GetOverridenComponentTemplate(K);return O?O:H->CreateOverridenComponentTemplate(K);}
 checkNoEntry();return nullptr;
}
double Number(UObject* O,const TCHAR* Name){auto* P=FindFProperty<FNumericProperty>(O->GetClass(),Name);check(P);return P->GetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(O));}
void Number(UObject* O,const TCHAR* Name,double V){auto* P=FindFProperty<FNumericProperty>(O->GetClass(),Name);check(P);P->SetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(O),V);}
void DeathPolicy(UBlueprint* BP,bool Verify)
{
 const FName Name(TEXT("AnimationOnlyDeath"));auto* G=Graph(BP);const FString DeathMarker(TEXT("VTG optional animation-only death"));
 bool Found=false;for(UEdGraphNode* N:G->Nodes)if(N->NodeComment==DeathMarker)Found=true;
 if(!Found)
 {
  check(!Verify);FEdGraphPinType Type;Type.PinCategory=UEdGraphSchema_K2::PC_Object;Type.PinSubCategoryObject=UAnimSequence::StaticClass();
  check(FBlueprintEditorUtils::AddMemberVariable(BP,Name,Type));FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP,Name,false);FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);Compile(BP);
  auto* E=Node(G,TEXT("K2Node_CustomEvent_14"));auto Next=E->FindPinChecked(TEXT("then"))->LinkedTo;E->FindPinChecked(TEXT("then"))->BreakAllPinLinks();
  auto* Try=Call(G,UVTGCombatStatics::StaticClass()->FindFunctionByName(TEXT("TryAnimationOnlyDeath")),18000,18000);Try->NodeComment=DeathMarker;
  auto* Owner=Call(G,UActorComponent::StaticClass()->FindFunctionByName(TEXT("GetOwner")),17700,18200);
  auto* Anim=Get(G,Name,17700,18400);Link(Owner,TEXT("ReturnValue"),Try,TEXT("Actor"));Link(Anim,TEXT("AnimationOnlyDeath"),Try,TEXT("Animation"));Link(E,TEXT("then"),Try,TEXT("execute"));
  auto* Gate=Add<UK2Node_IfThenElse>(G,18300,18000);Gate->AllocateDefaultPins();Link(Try,TEXT("then"),Gate,TEXT("execute"));Link(Try,TEXT("ReturnValue"),Gate,TEXT("Condition"));for(auto* P:Next)Link(Gate->FindPinChecked(TEXT("else")),P);
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);Compile(BP);SaveAsset(BP);
 }
 check(!FindFProperty<FObjectPropertyBase>(BP->GeneratedClass,Name)->GetObjectPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject()));
}
void ConfigureEnemies(bool Verify)
{
 auto* Component=LoadObject<UBlueprint>(nullptr,TEXT("/Game/AI/AIBehaviorSystem/Blueprint/Component/BP_AIBehaviorsComponent"));check(Component);DeathPolicy(Component,Verify);
 TArray<FString> Baselines;const FString File=FPaths::ProjectDir()/Root/TEXT("damage-baseline.txt");
 const bool HaveBaseline=FFileHelper::LoadFileToStringArray(Baselines,*File);check(!Verify || HaveBaseline);if(HaveBaseline)check(Baselines.Num()==3);
 const TCHAR* Names[]={TEXT("BP_Enm_StreetGoon1Misto"),TEXT("BP_Enm_StreetGoon2Nara"),TEXT("BP_Enm_SteetGoon3_Brosky")};const double Targets[]={10.,10.,20.};
 auto* Death=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/AI/AIBehaviorSystem/Demo/Animation/Death_Bwd"));check(Death);
 for(int I=0;I<3;++I)
 {
  auto* BP=LoadObject<UBlueprint>(nullptr,*(FString(TEXT("/Game/Characters/NPC/StreetGoons/"))+Names[I]));check(BP);
  auto* T=BehaviorTemplate(BP,!Verify);const double Before=Number(T,TEXT("MeleeAttackDamage"));
  if(!HaveBaseline)Baselines.Add(FString::Printf(TEXT("%.9g"),Before));const double Target=Targets[I];
  if(!Verify){Number(T,TEXT("MeleeAttackDamage"),Target);if(I==2)Obj(T,TEXT("AnimationOnlyDeath"),Death);FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);Compile(BP);SaveAsset(BP);}
  check(FMath::IsNearlyEqual(Number(BehaviorTemplate(BP,false),TEXT("MeleeAttackDamage")),Target,.0001));
  auto* Configured=FindFProperty<FObjectPropertyBase>(T->GetClass(),TEXT("AnimationOnlyDeath"))->GetObjectPropertyValue_InContainer(T);check(Configured==(I==2?Death:nullptr));
  UE_LOG(LogTemp,Display,TEXT("STREET_DAMAGE %s original=%.3f current=%.3f animationOnly=%d"),Names[I],FCString::Atod(*Baselines[I]),Target,I==2);
 }
 if(!HaveBaseline)check(FFileHelper::SaveStringArrayToFile(Baselines,*File));
}
void RepairSwitch(bool Verify)
{
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt"));check(BP);auto* G=Graph(BP);
 auto* Open=Node(G,TEXT("K2Node_CallFunction_9"));auto* Begin=Node(G,TEXT("K2Node_CallFunction_60"));
 auto* Out=Open->FindPinChecked(TEXT("then"));auto* In=Begin->FindPinChecked(TEXT("execute"));
 if(!Out->LinkedTo.Contains(In)){check(!Verify && Out->LinkedTo.IsEmpty() && In->LinkedTo.IsEmpty());Link(Out,In);Begin->NodeComment=TEXT("RoundDoorOpen: finish the interaction through Narrative so the trigger camera lock is released");FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);Compile(BP);SaveAsset(BP);}
 check(Out->LinkedTo.Contains(In));
 UE_LOG(LogTemp,Display,TEXT("STREET_SWITCH linked L3_RoundDoorSwitch -> L3_OpenRoundDoor -> BeginDialogue NPC_22"));
}
UObject* Ref(UObject* O,const TCHAR* Name){auto* P=FindFProperty<FObjectPropertyBase>(O->GetClass(),Name);check(P);return P->GetObjectPropertyValue_InContainer(O);}
void RuntimeTests()
{
 FEditorScriptExecutionGuard Guard;
 if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
 auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("StreetFlowRegression"),nullptr,true,ERHIFeatureLevel::Num,&Init);check(W);
 auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);Context.SetCurrentWorld(W);auto* GI=NewObject<UVTGGameInstanceBase>();W->SetGameInstance(GI);Context.OwningGameInstance=GI;
 check(W->SetGameMode(FURL(nullptr,TEXT("/Engine/Maps/Entry?game=/Script/Engine.GameModeBase"),TRAVEL_Absolute)));
 FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Stage=W->SpawnActor<AVTGLevelManagerBase>();Stage->bOverrideStageForTesting=true;Stage->TestStage=TEXT("EditorOnly");GI->CurrentStage=TEXT("Default");
 W->WorldType=EWorldType::PIE;check(Stage->GetActiveStage()==TEXT("EditorOnly"));GI->OnWorldChanged(nullptr,W);check(Stage->GetActiveStage()==TEXT("EditorOnly"));
 W->bBegunPlay=true;GI->OnWorldChanged(W,nullptr);W->bBegunPlay=false;check(Stage->GetActiveStage()==TEXT("Default"));
 GI->SetCurrentStage(TEXT("TravelStage"));check(Stage->GetActiveStage()==TEXT("TravelStage"));W->WorldType=EWorldType::Game;check(Stage->GetActiveStage()==TEXT("TravelStage"));
 UE_LOG(LogTemp,Display,TEXT("STREET_STAGE_PASS: direct PIE override; explicit stage wins; runtime ignores test override"));W->DestroyActor(Stage);
 auto* PlayerClass=LoadClass<ACharacter>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa_C"));check(PlayerClass);
 auto* Sa=W->SpawnActor<ACharacter>(PlayerClass,FTransform::Identity,Spawn);auto* PC=W->SpawnActor<APlayerController>();W->AddController(PC);PC->Possess(Sa);
 auto* Local=NewObject<ULocalPlayer>(GEngine);GI->AddLocalPlayer(Local,0);PC->SetPlayer(Local);
 auto* NC=Sa->FindComponentByClass<UNarrativeComponent>();check(NC);
 auto* EntranceClass=LoadClass<AActor>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L4BlackMarket/L4BlackMarketEntrance/BPLM_BlackMarketEntrance.BPLM_BlackMarketEntrance_C"));check(EntranceClass);
 auto* Entrance=W->SpawnActor<AActor>(EntranceClass,FTransform::Identity,Spawn);
 auto* InteractionClass=LoadClass<AActor>(nullptr,TEXT("/Game/Blueprints/BP_0ISXInteractiveCollision.BP_0ISXInteractiveCollision_C"));check(InteractionClass);
 auto* Exit=W->SpawnActor<AActor>(InteractionClass,FTransform(FVector(5000,0,0)),Spawn);Obj(Entrance,TEXT("ISXColi_Uni_GoToMarketStreet"),Exit);Bool(Exit,TEXT("CanInteract?"),false);
 FDialoguePlayParams Params;Params.StartFromID=TEXT("L4_Entrance_BlackMarket_Freemove_DialogueNode_NPC_14");
 auto* DialogueClass=LoadClass<UDialogue>(nullptr,TEXT("/Game/Narrative/Dialogues/Levels/L4BlackMarket/Entrance/L4_Entrance_BlackMarket_Freemove.L4_Entrance_BlackMarket_Freemove_C"));check(DialogueClass && NC->BeginDialogue(DialogueClass,Params));
 auto* D=NC->GetCurrentDialogue();D->SetAutoAdvance(false);check(D->IsAutoAdvanceEnabled());
 auto Finish=[&](){auto* Current=NC->GetCurrentDialogue();check(Current);Current->ProcessEvent(Current->FindFunctionChecked(TEXT("FinishNPCDialogue")),nullptr);};
 Finish();check(CastChecked<UDialogueNode>(Ref(D,TEXT("CurrentNode")))->GetID()==TEXT("L4_Entrance_BlackMarket_Freemove_DialogueNode_NPC_16") && !D->IsWaitingForManualAdvance());
 Finish();check(FindFProperty<FBoolProperty>(Exit->GetClass(),TEXT("CanInteract?"))->GetPropertyValue_InContainer(Exit));Finish();check(!NC->GetCurrentDialogue());
 auto* Ordinary=NewObject<UDialogue>();Ordinary->bFreeMovement=false;Ordinary->SetAutoAdvance(false);check(!Ordinary->IsAutoAdvanceEnabled());
 UE_LOG(LogTemp,Display,TEXT("STREET_DIALOGUE_PASS: actual NPC_14 -> NPC_16 -> NPC_17 -> exit; real bound event enables leave collision with Auto OFF; ordinary story keeps manual mode"));
 auto* SewerClass=LoadClass<AActor>(nullptr,TEXT("/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt.BPLM_SewerUnderApt_C"));check(SewerClass);auto* Sewer=W->SpawnActor<AActor>(SewerClass,FTransform::Identity,Spawn);
 Obj(Sewer,TEXT("Player Pawn"),Sa);Obj(Sewer,TEXT("Player Controller"),PC);Obj(Sewer,TEXT("Narrative"),NC);
 auto* Switch=W->SpawnActor<ASkeletalMeshActor>();auto* Door=W->SpawnActor<ASkeletalMeshActor>();Obj(Sewer,TEXT("RoundDoorSwitch"),Switch);Obj(Sewer,TEXT("RoundDoor"),Door);
 Sewer->ProcessEvent(Sewer->FindFunctionChecked(TEXT("L3_RoundDoorSwitch")),nullptr);check(NC->GetCurrentDialogue());check(CastChecked<UDialogueNode>(Ref(NC->GetCurrentDialogue(),TEXT("CurrentNode")))->GetID()==TEXT("L3Sewer_Still_DialogueNode_NPC_22"));
 for(int I=0;I<20 && NC->GetCurrentDialogue();++I)NC->TrySkipCurrentDialogueLine();check(!NC->GetCurrentDialogue());
 W->GetLatentActionManager().BeginFrame();W->GetLatentActionManager().ProcessLatentActions(Sewer,1.1f);W->GetLatentActionManager().BeginFrame();W->GetLatentActionManager().ProcessLatentActions(Sewer,.01f);
 check(FindFProperty<FBoolProperty>(Sewer->GetClass(),TEXT("L3_CheckpointDoorOpened"))->GetPropertyValue_InContainer(Sewer));
 UE_LOG(LogTemp,Display,TEXT("STREET_SWITCH_PASS: actual switch event starts NPC_22; dialogue can finish and door-open checkpoint is set"));
 const TCHAR* Names[]={TEXT("BP_Enm_StreetGoon1Misto"),TEXT("BP_Enm_StreetGoon2Nara"),TEXT("BP_Enm_SteetGoon3_Brosky")};
 for(int I=0;I<3;++I)
 {
  auto* Class=LoadClass<ACharacter>(nullptr,*(FString(TEXT("/Game/Characters/NPC/StreetGoons/"))+Names[I]+TEXT(".")+Names[I]+TEXT("_C")));check(Class);
  auto* E=W->SpawnActor<ACharacter>(Class,FTransform(FVector(10000+I*1000,0,0)),Spawn);check(E);auto* B=Ref(E,TEXT("AIBehaviorsComponent"));auto* Collision=Ref(E,TEXT("MeleeCollisionComponent"));check(B && Collision);
  auto* Target=W->SpawnActor<AVTGGoonDamageTarget>();Target->SetActorLocation(E->GetActorLocation()+FVector(100,0,0));
  auto* F=Collision->FindFunctionChecked(TEXT("ApplyDamageToActor"));FStructOnScope Args(F);auto* Hit=FindFProperty<FStructProperty>(F,TEXT("Hit"));check(Hit);*Hit->ContainerPtrToValuePtr<FHitResult>(Args.GetStructMemory())=FHitResult(Target,Cast<UPrimitiveComponent>(Target->GetRootComponent()),Target->GetActorLocation(),FVector::UpVector);
  auto* Base=FindFProperty<FNumericProperty>(F,TEXT("BaseDamage"));check(Base);Base->SetFloatingPointPropertyValue(Base->ContainerPtrToValuePtr<void>(Args.GetStructMemory()),Number(B,TEXT("MeleeAttackDamage")));Collision->ProcessEvent(F,Args.GetStructMemory());
  check(FMath::IsNearlyEqual(double(100-Target->Health),Number(B,TEXT("MeleeAttackDamage")),.001));
  if(I==2)
  {
   E->GetMesh()->InitAnim(true);FHitResult Impact(E,E->GetMesh(),E->GetActorLocation(),FVector::UpVector);UGameplayStatics::ApplyPointDamage(E,1000,FVector::ForwardVector,Impact,PC,Sa,UDamageType::StaticClass());
   E->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;E->GetMesh()->bEnableUpdateRateOptimizations=false;
   for(int T=0;T<100;++T){++GFrameCounter;W->GetLatentActionManager().BeginFrame();W->GetLatentActionManager().ProcessLatentActions(E,.1f);for(auto* C:E->GetComponents())W->GetLatentActionManager().ProcessLatentActions(C,.1f);W->GetTimerManager().Tick(.1f);E->GetMesh()->TickAnimation(.1f,false);E->GetMesh()->RefreshBoneTransforms();}
   auto* Single=E->GetMesh()->GetSingleNodeInstance();check(Single && Single->GetCurrentAsset() && Single->GetCurrentAsset()->GetName()==TEXT("Death_Bwd") && !E->GetMesh()->IsSimulatingPhysics());UE_LOG(LogTemp,Display,TEXT("STREET_DEATH time=%.3f length=%.3f playing=%d pause=%d"),Single->GetCurrentTime(),Single->GetLength(),Single->IsPlaying(),E->GetMesh()->bPauseAnims);check(Single->GetCurrentTime()>=Single->GetLength()-.05f);
   UE_LOG(LogTemp,Display,TEXT("STREET_DEATH_PASS: lethal point damage plays Death_Bwd, holds last frame and never enables ragdoll after five seconds"));
  }
  W->DestroyActor(E);W->DestroyActor(Target);
 }
 auto* Map=LoadObject<UWorld>(nullptr,TEXT("/Game/ThirdPerson/Maps/Street/BlackMarketStreet"));check(Map);const double Targets[]={10.,10.,20.};int Found=0;
 for(AActor* A:Map->PersistentLevel->Actors)if(A)for(int I=0;I<3;++I)if(A->GetClass()->GetName()==FString(Names[I])+TEXT("_C")){auto* B=Ref(A,TEXT("AIBehaviorsComponent"));check(B && FMath::IsNearlyEqual(Number(B,TEXT("MeleeAttackDamage")),Targets[I],.001));++Found;}
 check(Found==3);UE_LOG(LogTemp,Display,TEXT("STREET_MAP_PASS: actual BlackMarketStreet placed enemy components use all three requested damage values"));
 GI->RemoveLocalPlayer(Local);W->DestroyWorld(false);GEngine->DestroyWorldContext(W);
}
int32 Run(bool Verify)
{
 IFileManager::Get().MakeDirectory(*(FPaths::ProjectDir()/Root),true);
 ConfigureEnemies(Verify);RepairSwitch(Verify);RuntimeTests();
 UE_LOG(LogTemp,Display,TEXT("STREET_FLOW_FIX_PASS"));return 0;
}
}
