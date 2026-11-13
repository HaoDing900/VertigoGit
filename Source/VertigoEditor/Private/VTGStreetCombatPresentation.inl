namespace StreetCombatPresentation
{
using namespace MarketFollowup;
const FString TaskFolder=TEXT("Scripts/StreetCombatPresentation/");
const TCHAR* EnemyNames[]={TEXT("BP_Enm_StreetGoon1Misto"),TEXT("BP_Enm_StreetGoon2Nara"),TEXT("BP_Enm_SteetGoon3_Brosky")};
const TCHAR* FontPath=TEXT("/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-Light_Font");
void Persist(UObject* O)
{
 const FString File=FPackageName::LongPackageNameToFilename(O->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
 const FString Backup=FPaths::ProjectDir()/TaskFolder/TEXT("Backup")/(O->GetName()+TEXT(".uasset"));IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
 if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
 FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;check(UPackage::SavePackage(O->GetOutermost(),O,*File,A));
}
void PatchGate(UBlueprint* BP,const TCHAR* EventNode,const TCHAR* EventName,bool Verify)
{
 auto* G=Graph(BP);const FString Tag=FString(TEXT("VTG dialogue-exit combat gate "))+EventName;
 for(UEdGraphNode* N:G->Nodes)if(N->NodeComment==Tag)return;
 check(!Verify);auto* E=Node(G,EventNode);auto* Out=E->FindPinChecked(TEXT("then"));auto Next=Out->LinkedTo;check(Next.Num()==1);Out->BreakAllPinLinks();
 auto* Wait=Call(G,AVTGLevelManagerBase::StaticClass()->FindFunctionByName(TEXT("DeferEncounterUntilDialogueEnds")),16000,E->NodePosY);Wait->NodeComment=Tag;Wait->FindPinChecked(TEXT("ResumeEvent"))->DefaultValue=EventName;
 auto* NC=Get(G,TEXT("Narrative"),15700,E->NodePosY+180);Link(NC,TEXT("Narrative"),Wait,TEXT("Narrative"));Link(Out,Wait->FindPinChecked(TEXT("execute")));
 auto* B=Add<UK2Node_IfThenElse>(G,16300,E->NodePosY);B->AllocateDefaultPins();Link(Wait,TEXT("then"),B,TEXT("execute"));Link(Wait,TEXT("ReturnValue"),B,TEXT("Condition"));for(auto* P:Next)Link(B->FindPinChecked(TEXT("else")),P);
}
void PatchManager(UBlueprint* BP,bool Verify)
{
 PatchGate(BP,TEXT("K2Node_CustomEvent_48"),TEXT("L4_GoonInCombat"),Verify);
 PatchGate(BP,TEXT("K2Node_CustomEvent_30"),TEXT("StartAlleyConflict"),Verify);
 PatchGate(BP,TEXT("K2Node_CustomEvent_60"),TEXT("InitializeAlleyCombat"),Verify);
 auto* G=Graph(BP);const FString CleanupTag=TEXT("VTG remove all three defeated goons together");bool Found=false;
 for(UEdGraphNode* N:G->Nodes)if(N->NodeComment==CleanupTag)Found=true;
 if(!Found)
 {
  check(!Verify);auto* Cleanup=Call(G,UVTGCombatStatics::StaticClass()->FindFunctionByName(TEXT("RemoveActorsTogether")),17000,26000);Cleanup->NodeComment=CleanupTag;Cleanup->FindPinChecked(TEXT("DelaySeconds"))->DefaultValue=TEXT("5.0");
  auto* Self=Add<UK2Node_Self>(G,16700,26200);Self->AllocateDefaultPins();Link(Self,TEXT("self"),Cleanup,TEXT("WorldContextObject"));
  auto* Array=Add<UK2Node_MakeArray>(G,16700,26400);Array->NumInputs=3;Array->AllocateDefaultPins();Link(Array,TEXT("Array"),Cleanup,TEXT("Actors"));
  for(int I=0;I<3;++I){const FName Name(*FString::Printf(TEXT("Enm_Goon%d"),I+1));auto* Ref=Get(G,Name,16400,26400+I*150);Link(Ref,*Name.ToString(),Array,*FString::Printf(TEXT("[%d]"),I));}
  Before(Node(G,TEXT("K2Node_CallFunction_188"))->FindPinChecked(TEXT("execute")),Cleanup);
 }
 if(!Verify){FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);Compile(BP);Persist(BP);}
}
void Configure(bool Verify)
{
 for(const TCHAR* Name:EnemyNames)
 {
  auto* BP=LoadObject<UBlueprint>(nullptr,*(FString(TEXT("/Game/Characters/NPC/StreetGoons/"))+Name));check(BP);
  auto* T=StreetFlowFix::BehaviorTemplate(BP,!Verify);
  if(!Verify){StreetFlowFix::Number(T,TEXT("DeadLifespan"),0);FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);Compile(BP);FindFProperty<FTextProperty>(BP->GeneratedClass,TEXT("EnemyDisplayName"))->SetPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject(),NSLOCTEXT("VTG","StreetGoon","Street Goon"));Persist(BP);}
  check(StreetFlowFix::Number(StreetFlowFix::BehaviorTemplate(BP,false),TEXT("DeadLifespan"))==0);
  check(FindFProperty<FTextProperty>(BP->GeneratedClass,TEXT("EnemyDisplayName"))->GetPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject()).ToString()==TEXT("Street Goon"));
 }
 auto* Bar=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/Widget/EnemyHealthBar/WBP_EnemyHealthBar"));check(Bar);auto* Text=CastChecked<UTextBlock>(Bar->WidgetTree->FindWidget(TEXT("EnemyName")));auto* Light=LoadObject<UFont>(nullptr,FontPath);check(Light);
 if(!Verify){auto Font=Text->GetFont();Font.FontObject=Light;Font.TypefaceFontName=NAME_None;Text->SetFont(Font);Compile(Bar);Persist(Bar);}
 check(Text->GetFont().FontObject==Light);
}
void Tests(UBlueprint* BP)
{
 FEditorScriptExecutionGuard Guard;
 if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
 auto* Copy=DuplicateObject<UBlueprint>(BP,GetTransientPackage(),TEXT("StreetCombatGateRegression"));check(Copy);FEdGraphPinType Type;Type.PinCategory=UEdGraphSchema_K2::PC_Int;check(FBlueprintEditorUtils::AddMemberVariable(Copy,TEXT("RegressionStarts"),Type,TEXT("0")));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Copy);Compile(Copy);auto* G=Graph(Copy);
 auto* Original=Node(G,TEXT("K2Node_CallFunction_131"))->FindPinChecked(TEXT("execute"));auto Inputs=Original->LinkedTo;Original->BreakAllPinLinks();
 auto* Capture=Set(G,TEXT("RegressionStarts"),TEXT("0"),0,0);auto* Value=Get(G,TEXT("RegressionStarts"),0,200);auto* AddOne=Call(G,UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("Add_IntInt")),200,200);Link(Value,TEXT("RegressionStarts"),AddOne,TEXT("A"));AddOne->FindPinChecked(TEXT("B"))->DefaultValue=TEXT("1");Link(AddOne,TEXT("ReturnValue"),Capture,TEXT("RegressionStarts"));for(auto* P:Inputs)Link(P,Capture->FindPinChecked(TEXT("execute")));
 // Keep real defeat counting and group timer, excluding unrelated post-fight narrative/camera work.
 Node(G,TEXT("K2Node_CallFunction_188"))->FindPinChecked(TEXT("execute"))->BreakAllPinLinks();FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Copy);Compile(Copy);
 auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("StreetCombatPresentationTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);Context.SetCurrentWorld(W);auto* GI=NewObject<UGameInstance>();W->SetGameInstance(GI);Context.OwningGameInstance=GI;
 FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* M=W->SpawnActor<AVTGLevelManagerBase>(Copy->GeneratedClass,FTransform::Identity,Spawn);auto* Pawn=W->SpawnActor<ACharacter>();auto* NC=NewObject<UNarrativeComponent>(Pawn);Pawn->AddInstanceComponent(NC);NC->RegisterComponent();Obj(M,TEXT("Narrative"),NC);
 auto* First=NewObject<UDialogue>(NC);NC->CurrentDialogue=First;
 auto Run=[](UObject* O,const TCHAR* Event){O->ProcessEvent(O->FindFunctionChecked(Event),nullptr);};
 auto Tick=[&](float Dt){++GFrameCounter;W->GetTimerManager().Tick(Dt);};
 auto* Counter=FindFProperty<FIntProperty>(M->GetClass(),TEXT("RegressionStarts"));check(Counter);Run(M,TEXT("L4_GoonInCombat"));Run(M,TEXT("L4_GoonInCombat"));Tick(2);check(Counter->GetPropertyValue_InContainer(M)==0);
 NC->OnDialogueFinished.Broadcast(First,true);check(Counter->GetPropertyValue_InContainer(M)==0);auto* Replacement=NewObject<UDialogue>(NC);NC->CurrentDialogue=Replacement;Tick(.1f);check(Counter->GetPropertyValue_InContainer(M)==0);
 NC->OnDialogueFinished.Broadcast(Replacement,false);check(Counter->GetPropertyValue_InContainer(M)==0);NC->CurrentDialogue=nullptr;Tick(.1f);check(Counter->GetPropertyValue_InContainer(M)==1);Tick(.1f);check(Counter->GetPropertyValue_InContainer(M)==1);
 UE_LOG(LogTemp,Display,TEXT("STREET_PRESENTATION_GATE_PASS: entry blocked throughout dialogue and replacement; resumes once on next tick after full exit"));
 auto* PC=W->SpawnActor<APlayerController>();W->AddController(PC);PC->Possess(Pawn);auto* Local=NewObject<ULocalPlayer>(GEngine);GI->AddLocalPlayer(Local,0);PC->SetPlayer(Local);
 auto* Bar=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/Widget/EnemyHealthBar/WBP_EnemyHealthBar"));auto* Font=LoadObject<UFont>(nullptr,FontPath);TArray<AActor*> Enemies;
 for(int I=0;I<3;++I)
 {
  const FString Path=FString(TEXT("/Game/Characters/NPC/StreetGoons/"))+EnemyNames[I];auto* Class=LoadClass<AActor>(nullptr,*(Path+TEXT(".")+EnemyNames[I]+TEXT("_C")));check(Class);auto* E=W->SpawnActor<AActor>(Class,FTransform(FVector(5000+I*1000,0,0)),Spawn);Enemies.Add(E);Obj(M,*FString::Printf(TEXT("Enm_Goon%d"),I+1),E);
  auto* UI=CreateWidget<UUserWidget>(PC,Bar->GeneratedClass.Get());check(UI);auto* Label=CastChecked<UTextBlock>(UI->GetWidgetFromName(TEXT("EnemyName")));auto OriginalFont=Label->GetFont();OriginalFont.FontObject=nullptr;Label->SetFont(OriginalFont);
  auto* Widget=CastChecked<UWidgetComponent>(StreetFlowFix::Ref(E,TEXT("HealthBar")));Widget->SetWidget(UI);auto* Identity=E->FindComponentByClass<UVTGEnemyIdentityComponent>();check(Identity);Identity->RefreshName();check(Label->GetText().ToString()==TEXT("Street Goon") && Label->GetFont().FontObject==Font);
 }
 auto* Left=FindFProperty<FIntProperty>(M->GetClass(),TEXT("L4_LeftEnemies"));Left->SetPropertyValue_InContainer(M,3);
 for(int I=0;I<2;++I){Run(M,TEXT("OnDefeated"));Tick(6);for(auto* E:Enemies)check(!E->IsActorBeingDestroyed());}
 Run(M,TEXT("OnDefeated"));Tick(4.9f);for(auto* E:Enemies)check(!E->IsActorBeingDestroyed());Tick(.2f);for(auto* E:Enemies)check(E->IsActorBeingDestroyed());
 UE_LOG(LogTemp,Display,TEXT("STREET_PRESENTATION_CLEANUP_PASS: all remain before final defeat; all three removed together after five seconds; Street Goon names and Noto Sans Light verified on live widgets"));
 GI->RemoveLocalPlayer(Local);W->DestroyWorld(false);GEngine->DestroyWorldContext(W);
}
int32 Run(bool Verify)
{
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L4BlackMarket/BPLM_BlackMarket"));check(BP);PatchManager(BP,Verify);Configure(Verify);Tests(BP);
 UE_LOG(LogTemp,Display,TEXT("STREET_COMBAT_PRESENTATION_PASS"));return 0;
}
}
