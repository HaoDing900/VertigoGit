#include "VTGCustomizeFadeCommandlet.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Engine/World.h"
#include "Engine/LatentActionManager.h"
#include "Editor.h"
#include "Widgets/SWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Modules/ModuleManager.h"
#include "HAL/IConsoleManager.h"
#include "Animation/WidgetAnimation.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_ExecutionSequence.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace FadeConfig
{
UEdGraphPin* P(UEdGraphNode* N,const TCHAR* Name){auto* R=N->FindPin(Name);checkf(R,TEXT("Missing %s on %s"),Name,*N->GetName());return R;}
void Link(UEdGraphNode* A,const TCHAR* AP,UEdGraphNode* B,const TCHAR* BP){check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(P(A,AP),P(B,BP)));}
void Default(UEdGraphNode* N,const TCHAR* Pin,const FString& Value){GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*P(N,Pin),Value);}
template<class T>T* Add(UEdGraph* G,int X,int Y){auto* N=NewObject<T>(G);G->AddNode(N,false,false);N->CreateNewGuid();N->NodePosX=X;N->NodePosY=Y;return N;}
UK2Node_CallFunction* Call(UEdGraph* G,UClass* Class,const TCHAR* Fn,int X,int Y){auto* N=Add<UK2Node_CallFunction>(G,X,Y);auto* F=Class->FindFunctionByName(Fn);check(F);N->SetFromFunction(F);N->AllocateDefaultPins();return N;}
template<class T>T* Var(UEdGraph* G,UWidgetBlueprint* BP,const TCHAR* Name,int X,int Y){auto* N=Add<T>(G,X,Y);N->VariableReference.SetSelfMember(Name);N->AllocateDefaultPins();return N;}
UK2Node_Event* Event(UEdGraph* G,const TCHAR* Name,int X,int Y){auto* N=Add<UK2Node_Event>(G,X,Y);N->EventReference.SetExternalMember(Name,UUserWidget::StaticClass());N->bOverrideFunction=true;N->AllocateDefaultPins();return N;}
UK2Node_IfThenElse* Branch(UEdGraph* G,int X,int Y){auto* N=Add<UK2Node_IfThenElse>(G,X,Y);N->AllocateDefaultPins();return N;}
void Metadata(UWidgetBlueprint* BP,const TCHAR* Name,const TCHAR* Display,const TCHAR* Tip,bool Public)
{
    FBlueprintEditorUtils::SetBlueprintVariableCategory(BP,Name,nullptr,FText::FromString(Public?TEXT("Fade Settings"):TEXT("Fade Internal")));
    FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP,Name,nullptr,TEXT("DisplayName"),Display);
    FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP,Name,nullptr,TEXT("ToolTip"),Tip);
    if(Public){FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP,Name,false);FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP,Name,nullptr,TEXT("ExposeOnSpawn"),TEXT("true"));}
}
void AddVar(UWidgetBlueprint* BP,const TCHAR* Name,bool Bool,const TCHAR* Initial)
{
    FEdGraphPinType Type;Type.PinCategory=Bool?UEdGraphSchema_K2::PC_Boolean:UEdGraphSchema_K2::PC_Real;if(!Bool)Type.PinSubCategory=UEdGraphSchema_K2::PC_Double;
    check(FBlueprintEditorUtils::AddMemberVariable(BP,Name,Type,Initial));
}
}
UVTGCustomizeFadeCommandlet::UVTGCustomizeFadeCommandlet(){IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true;}
int32 UVTGCustomizeFadeCommandlet::Main(const FString& Params)
{
    using namespace FadeConfig;
    const bool Verify=Params.Contains(TEXT("VerifyOnly"));
    auto* BP=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/2DArt/ScreenTransition/WBP_Fade.WBP_Fade"));check(BP && BP->Animations.Num()==1);
    UWidgetAnimation* Animation=BP->Animations[0];const float Length=Animation->GetEndTime()-Animation->GetStartTime();check(Length>0);
    if(!Verify)
    {
        const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
        const FString Backup=FPaths::ProjectDir()/TEXT("Scripts/FadeCustomization/WBP_Fade.before_customization.uasset");
        check(!IFileManager::Get().FileExists(*Backup));check(IFileManager::Get().Copy(*Backup,*File,false)==COPY_OK);
        AddVar(BP,TEXT("FadeDuration"),false,TEXT("1.5"));
        AddVar(BP,TEXT("ReverseFadeOnAppear"),true,TEXT("False"));
        AddVar(BP,TEXT("FadeOutStarted"),true,TEXT("False"));
        Metadata(BP,TEXT("FadeDuration"),TEXT("Fade Duration (seconds)"),TEXT("纯黑到透明的持续秒数。0 表示立即透明；负数按 0 处理。"),true);
        Metadata(BP,TEXT("TimeBeforeKill"),TEXT("Total Widget Lifetime (seconds)"),TEXT("从 Widget 出现开始计时，到此时间从屏幕移除。包含渐黑、黑屏等待和淡出。0 或负数表示淡出结束后自动移除。若比整个效果短，会提前截断。RemoveFromParent 不会立即销毁仍被引用的 UObject。"),true);
        Metadata(BP,TEXT("HoldDurationUntilFade"),TEXT("Black Hold Before Fade (seconds)"),TEXT("达到纯黑后，等待多少秒才开始变透明。启用 Reverse Fade 时，从 1 秒渐黑完成后开始计算。负数按 0 处理。"),true);
        Metadata(BP,TEXT("ReverseFadeOnAppear"),TEXT("Reverse Fade On Appear (1 second)"),TEXT("勾选：出现时从透明渐变到纯黑，持续 1 秒。不勾选：立即纯黑。然后等待 Black Hold Before Fade。"),true);
        Metadata(BP,TEXT("DisableFade?"),TEXT("Keep Black (disable fade out)"),TEXT("旧变量 DisableFade?：勾选后不执行纯黑到透明。仍可渐黑出现。总时长大于 0 时按时移除；总时长为 0 时保持黑屏，直到外部移除。"),true);
        Metadata(BP,TEXT("FadeOutStarted"),TEXT("Fade Out Started (internal)"),TEXT("内部状态，避免开场反向动画结束时提前移除 Widget。无需手动修改。"),false);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave);
        UEdGraph* G=BP->UbergraphPages[0];auto Old=G->Nodes;for(UEdGraphNode* N:Old)FBlueprintEditorUtils::RemoveNode(BP,N,true);
        auto* Start=Event(G,TEXT("Construct"),0,0);
        auto* Reset=Var<UK2Node_VariableSet>(G,BP,TEXT("FadeOutStarted"),220,0);Default(Reset,TEXT("FadeOutStarted"),TEXT("false"));Link(Start,TEXT("then"),Reset,TEXT("execute"));
        auto* Stop=Call(G,UUserWidget::StaticClass(),TEXT("StopAllAnimations"),440,0);Link(Reset,TEXT("then"),Stop,TEXT("execute"));
        auto* Black=Var<UK2Node_VariableGet>(G,BP,TEXT("Black"),450,220);
        auto* Color=Call(G,UImage::StaticClass(),TEXT("SetColorAndOpacity"),650,0);Default(Color,TEXT("InColorAndOpacity"),TEXT("(R=0,G=0,B=0,A=1)"));Link(Black,TEXT("Black"),Color,TEXT("self"));Link(Stop,TEXT("then"),Color,TEXT("execute"));
        auto* Opacity=Call(G,UWidget::StaticClass(),TEXT("SetRenderOpacity"),900,0);Default(Opacity,TEXT("InOpacity"),TEXT("1"));Link(Black,TEXT("Black"),Opacity,TEXT("self"));Link(Color,TEXT("then"),Opacity,TEXT("execute"));
        auto* Sequence=Add<UK2Node_ExecutionSequence>(G,1150,0);Sequence->AllocateDefaultPins();Link(Opacity,TEXT("then"),Sequence,TEXT("execute"));
        // Independent total lifetime, measured from Construct.
        auto* Total=Var<UK2Node_VariableGet>(G,BP,TEXT("TimeBeforeKill"),1300,-380);
        auto* Positive=Call(G,UKismetMathLibrary::StaticClass(),TEXT("Greater_DoubleDouble"),1500,-320);Link(Total,TEXT("TimeBeforeKill"),Positive,TEXT("A"));Default(Positive,TEXT("B"),TEXT("0"));
        auto* LifetimeGate=Branch(G,1740,-500);Link(Sequence,TEXT("then_0"),LifetimeGate,TEXT("execute"));Link(Positive,TEXT("ReturnValue"),LifetimeGate,TEXT("Condition"));
        auto* LifeDelay=Call(G,UKismetSystemLibrary::StaticClass(),TEXT("Delay"),1980,-500);Link(LifetimeGate,TEXT("then"),LifeDelay,TEXT("execute"));Link(Total,TEXT("TimeBeforeKill"),LifeDelay,TEXT("Duration"));
        auto* RemoveTimed=Call(G,UWidget::StaticClass(),TEXT("RemoveFromParent"),2240,-500);Link(LifeDelay,TEXT("then"),RemoveTimed,TEXT("execute"));
        LifetimeGate->NodeComment=TEXT("Total lifetime > 0: remove at this time, even during hold or fade.");LifetimeGate->bCommentBubbleVisible=true;
        // Optional reverse animation, then hold at full black.
        auto* Reverse=Var<UK2Node_VariableGet>(G,BP,TEXT("ReverseFadeOnAppear"),1310,230);
        auto* ReverseGate=Branch(G,1530,70);Link(Sequence,TEXT("then_1"),ReverseGate,TEXT("execute"));Link(Reverse,TEXT("ReverseFadeOnAppear"),ReverseGate,TEXT("Condition"));
        auto* Anim=Var<UK2Node_VariableGet>(G,BP,TEXT("Anim_FadeOut"),1750,450);
        auto* PlayReverse=Call(G,UUserWidget::StaticClass(),TEXT("PlayAnimation"),1790,40);Link(ReverseGate,TEXT("then"),PlayReverse,TEXT("execute"));Link(Anim,TEXT("Anim_FadeOut"),PlayReverse,TEXT("InAnimation"));Default(PlayReverse,TEXT("PlayMode"),TEXT("Reverse"));Default(PlayReverse,TEXT("PlaybackSpeed"),FString::SanitizeFloat(Length));Default(PlayReverse,TEXT("bRestoreState"),TEXT("false"));
        auto* ReverseWait=Call(G,UKismetSystemLibrary::StaticClass(),TEXT("Delay"),2040,40);Default(ReverseWait,TEXT("Duration"),TEXT("1"));Link(PlayReverse,TEXT("then"),ReverseWait,TEXT("execute"));
        auto* Hold=Call(G,UKismetSystemLibrary::StaticClass(),TEXT("Delay"),2310,130);Link(ReverseWait,TEXT("then"),Hold,TEXT("execute"));Link(ReverseGate,TEXT("else"),Hold,TEXT("execute"));
        auto* HoldValue=Var<UK2Node_VariableGet>(G,BP,TEXT("HoldDurationUntilFade"),1990,330);
        auto* ClampHold=Call(G,UKismetMathLibrary::StaticClass(),TEXT("FMax"),2220,390);Link(HoldValue,TEXT("HoldDurationUntilFade"),ClampHold,TEXT("A"));Default(ClampHold,TEXT("B"),TEXT("0"));Link(ClampHold,TEXT("ReturnValue"),Hold,TEXT("Duration"));
        auto* Disabled=Var<UK2Node_VariableGet>(G,BP,TEXT("DisableFade?"),2500,360);
        auto* FadeGate=Branch(G,2600,130);Link(Hold,TEXT("then"),FadeGate,TEXT("execute"));Link(Disabled,TEXT("DisableFade?"),FadeGate,TEXT("Condition"));
        auto* Mark=Var<UK2Node_VariableSet>(G,BP,TEXT("FadeOutStarted"),2820,130);Default(Mark,TEXT("FadeOutStarted"),TEXT("true"));Link(FadeGate,TEXT("else"),Mark,TEXT("execute"));
        auto* Duration=Var<UK2Node_VariableGet>(G,BP,TEXT("FadeDuration"),2800,420);
        auto* HasDuration=Call(G,UKismetMathLibrary::StaticClass(),TEXT("Greater_DoubleDouble"),3020,420);Link(Duration,TEXT("FadeDuration"),HasDuration,TEXT("A"));Default(HasDuration,TEXT("B"),TEXT("0"));
        auto* DurationGate=Branch(G,3100,130);Link(Mark,TEXT("then"),DurationGate,TEXT("execute"));Link(HasDuration,TEXT("ReturnValue"),DurationGate,TEXT("Condition"));
        auto* SafeDuration=Call(G,UKismetMathLibrary::StaticClass(),TEXT("FMax"),3060,650);Link(Duration,TEXT("FadeDuration"),SafeDuration,TEXT("A"));Default(SafeDuration,TEXT("B"),TEXT("0.001"));
        auto* Speed=Call(G,UKismetMathLibrary::StaticClass(),TEXT("Divide_DoubleDouble"),3300,560);Default(Speed,TEXT("A"),FString::SanitizeFloat(Length));Link(SafeDuration,TEXT("ReturnValue"),Speed,TEXT("B"));
        auto* Forward=Call(G,UUserWidget::StaticClass(),TEXT("PlayAnimation"),3390,130);Link(DurationGate,TEXT("then"),Forward,TEXT("execute"));Link(Anim,TEXT("Anim_FadeOut"),Forward,TEXT("InAnimation"));Link(Speed,TEXT("ReturnValue"),Forward,TEXT("PlaybackSpeed"));Default(Forward,TEXT("bRestoreState"),TEXT("false"));
        auto* Instant=Call(G,UImage::StaticClass(),TEXT("SetColorAndOpacity"),3390,850);Link(DurationGate,TEXT("else"),Instant,TEXT("execute"));Link(Black,TEXT("Black"),Instant,TEXT("self"));Default(Instant,TEXT("InColorAndOpacity"),TEXT("(R=0,G=0,B=0,A=0)"));
        auto* AutoGate=Branch(G,3640,850);Link(Instant,TEXT("then"),AutoGate,TEXT("execute"));Link(Positive,TEXT("ReturnValue"),AutoGate,TEXT("Condition"));auto* InstantRemove=Call(G,UWidget::StaticClass(),TEXT("RemoveFromParent"),3880,850);Link(AutoGate,TEXT("else"),InstantRemove,TEXT("execute"));
        auto* Finished=Event(G,TEXT("OnAnimationFinished"),0,1400);
        auto* Equal=Call(G,UKismetMathLibrary::StaticClass(),TEXT("EqualEqual_ObjectObject"),220,1600);Link(Finished,TEXT("Animation"),Equal,TEXT("A"));Link(Anim,TEXT("Anim_FadeOut"),Equal,TEXT("B"));
        auto* FinishGate=Branch(G,470,1400);Link(Finished,TEXT("then"),FinishGate,TEXT("execute"));Link(Equal,TEXT("ReturnValue"),FinishGate,TEXT("Condition"));
        auto* Started=Var<UK2Node_VariableGet>(G,BP,TEXT("FadeOutStarted"),450,1800);auto* StartedGate=Branch(G,700,1400);Link(FinishGate,TEXT("then"),StartedGate,TEXT("execute"));Link(Started,TEXT("FadeOutStarted"),StartedGate,TEXT("Condition"));
        auto* FinishAuto=Branch(G,930,1400);Link(StartedGate,TEXT("then"),FinishAuto,TEXT("execute"));Link(Positive,TEXT("ReturnValue"),FinishAuto,TEXT("Condition"));auto* RemoveAuto=Call(G,UWidget::StaticClass(),TEXT("RemoveFromParent"),1160,1400);Link(FinishAuto,TEXT("else"),RemoveAuto,TEXT("execute"));
        StartedGate->NodeComment=TEXT("Ignore completion of the opening reverse fade. Auto remove only after forward fade, when lifetime <= 0.");StartedGate->bCommentBubbleVisible=true;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    }
    const bool Cleanup=Params.Contains(TEXT("Cleanup"));
    if(Cleanup)
    {
        auto* StopFn=UUserWidget::StaticClass()->FindFunctionByName(TEXT("StopAnimationsAndLatentActions"));check(StopFn);
        UEdGraph* G=BP->UbergraphPages[0];
        for(UEdGraphNode* N:G->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N);C && C->FunctionReference.GetMemberName()==TEXT("StopAllAnimations")){C->SetFromFunction(StopFn);C->ReconstructNode();}
        auto* End=Event(G,TEXT("Destruct"),0,2100);
        auto* Reset=Var<UK2Node_VariableSet>(G,BP,TEXT("FadeOutStarted"),220,2100);Default(Reset,TEXT("FadeOutStarted"),TEXT("false"));Link(End,TEXT("then"),Reset,TEXT("execute"));
        auto* Stop=Call(G,UUserWidget::StaticClass(),TEXT("StopAnimationsAndLatentActions"),450,2100);Link(Reset,TEXT("then"),Stop,TEXT("execute"));
        Stop->NodeComment=TEXT("Cancel pending hold/lifetime delays when removed; safely reset if reused.");Stop->bCommentBubbleVisible=true;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    }
    FCompilerResultsLog Results;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Results);
    UE_LOG(LogTemp,Display,TEXT("CUSTOM FADE compile errors=%d warnings=%d"),Results.NumErrors,Results.NumWarnings);check(Results.NumErrors==0);
    check(FindFProperty<FDoubleProperty>(BP->GeneratedClass,TEXT("FadeDuration")));
    check(FindFProperty<FBoolProperty>(BP->GeneratedClass,TEXT("ReverseFadeOnAppear")));
    int32 Plays=0,Delays=0,Removes=0;
    for(UEdGraphNode* N:BP->UbergraphPages[0]->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N))
    {const FName Fn=C->FunctionReference.GetMemberName();if(Fn==TEXT("PlayAnimation"))++Plays;if(Fn==TEXT("Delay"))++Delays;if(Fn==TEXT("RemoveFromParent"))++Removes;}
    check(Plays==2 && Delays==3 && Removes==3);
    for(const TCHAR* Name:{TEXT("TimeBeforeKill"),TEXT("HoldDurationUntilFade"),TEXT("FadeDuration"),TEXT("ReverseFadeOnAppear"),TEXT("DisableFade?")})
    {auto* Prop=FindFProperty<FProperty>(BP->GeneratedClass,Name);check(Prop && Prop->HasMetaData(TEXT("ExposeOnSpawn")));}
    if(Params.Contains(TEXT("Test")))
    {
        if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
        IConsoleManager::Get().FindConsoleVariable(TEXT("Widget.UseParallelAnimation"))->Set(false,ECVF_SetByCode);
        FEditorScriptExecutionGuard Guard;
        UWorld* World=GEditor->GetEditorWorldContext().World();check(World);
        auto Test=[&](bool Reverse,double Hold,double Duration,double Lifetime,bool Disabled,double CheckTime,double ExpectedAlpha,bool ExpectedParent)
        {
            auto* Widget=NewObject<UUserWidget>(World,BP->GeneratedClass);check(Widget->Initialize());
            auto SetDouble=[&](const TCHAR* Name,double Value){auto* Prop=FindFProperty<FDoubleProperty>(Widget->GetClass(),Name);check(Prop);Prop->SetPropertyValue_InContainer(Widget,Value);};
            SetDouble(TEXT("HoldDurationUntilFade"),Hold);SetDouble(TEXT("FadeDuration"),Duration);SetDouble(TEXT("TimeBeforeKill"),Lifetime);
            FindFProperty<FBoolProperty>(Widget->GetClass(),TEXT("ReverseFadeOnAppear"))->SetPropertyValue_InContainer(Widget,Reverse);
            FindFProperty<FBoolProperty>(Widget->GetClass(),TEXT("DisableFade?"))->SetPropertyValue_InContainer(Widget,Disabled);
            auto* Parent=NewObject<UCanvasPanel>(World);Parent->AddChild(Widget);
            auto Slate=Widget->TakeWidget();
            auto* Image=CastChecked<UImage>(Widget->WidgetTree->FindWidget(TEXT("Black")));
            for(int32 I=0;I<FMath::RoundToInt(CheckTime/.01);++I)
            {
                ++GFrameCounter;
                World->GetLatentActionManager().BeginFrame();
                World->GetLatentActionManager().ProcessLatentActions(Widget,.01f);
                Slate->Tick(FGeometry(),I*.01,.01f);Widget->FlushAnimations();
            }
            const float Alpha=Image->GetColorAndOpacity().A;
            UE_LOG(LogTemp,Display,TEXT("CUSTOM FADE TEST reverse=%d hold=%.2f fade=%.2f lifetime=%.2f disabled=%d t=%.2f alpha=%.3f parent=%d"),Reverse,Hold,Duration,Lifetime,Disabled,CheckTime,Alpha,Widget->GetParent()!=nullptr);
            check(FMath::IsNearlyEqual(Alpha,(float)ExpectedAlpha,.08f));check((Widget->GetParent()!=nullptr)==ExpectedParent);
            Widget->StopAnimationsAndLatentActions();Widget->RemoveFromParent();
        };
        Test(false,.2,.4,0,false,.4,.5,true);
        Test(false,.2,.4,0,false,.8,0,false);
        Test(true,.2,.4,0,false,.5,.5,true);
        Test(true,.2,.4,0,false,1.1,1,true);
        Test(true,.2,.4,0,false,1.8,0,false);
        Test(false,0,.4,2,false,1,0,true);
        Test(false,0,.4,2,false,2.2,0,false);
        Test(false,1,2,.2,false,.4,1,false);
        Test(false,0,0,0,false,.1,0,false);
        Test(false,0,.4,0,true,1,1,true);
        Test(false,0,.4,.2,true,.4,1,false);
    }
    if(!Verify || Cleanup){FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());check(UPackage::SavePackage(BP->GetOutermost(),BP,*File,Args));}
    if(Params.Contains(TEXT("MigrateCaller")))
    {
        auto* Player=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa"));check(Player);
        UEdGraphNode* Create=nullptr;
        for(auto Graph:Player->UbergraphPages)for(UEdGraphNode* N:Graph->Nodes)if(N->GetName()==TEXT("K2Node_CreateWidget_0") && N->FindPin(TEXT("HoldDurationUntilFade")))Create=N;
        check(Create && P(Create,TEXT("TimeBeforeKill"))->DefaultValue==TEXT("3.500000") && P(Create,TEXT("HoldDurationUntilFade"))->DefaultValue==TEXT("2.500000"));
        check(P(Create,TEXT("TimeBeforeKill"))->LinkedTo.IsEmpty());
        const FString File=FPackageName::LongPackageNameToFilename(Player->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
        const FString Backup=FPaths::ProjectDir()/TEXT("Scripts/FadeCustomization/BP_Player_Sa.before_fade_lifetime.uasset");check(!IFileManager::Get().FileExists(*Backup));check(IFileManager::Get().Copy(*Backup,*File,false)==COPY_OK);
        Create->ReconstructNode();Default(Create,TEXT("TimeBeforeKill"),TEXT("4.000000"));
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Player);
        FCompilerResultsLog PlayerResults;FKismetEditorUtilities::CompileBlueprint(Player,EBlueprintCompileOptions::SkipSave,&PlayerResults);check(PlayerResults.NumErrors==0);
        FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;check(UPackage::SavePackage(Player->GetOutermost(),Player,*File,Args));
        UE_LOG(LogTemp,Display,TEXT("CUSTOM FADE caller migrated: player hold 2.5 + fade 1.5 = total 4.0; compile errors=%d"),PlayerResults.NumErrors);
    }
    UE_LOG(LogTemp,Display,TEXT("CUSTOM FADE %s PASS"),Verify?TEXT("VERIFY"):TEXT("SAVE"));return 0;
}

