#include "VTGRepairFadeCommandlet.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Animation/WidgetAnimation.h"
#include "MovieScene.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "Sections/MovieSceneFloatSection.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "UObject/UnrealType.h"
namespace FadeRepair { int Repair(UWidgetBlueprint* B,bool Verify); }
UVTGRepairFadeCommandlet::UVTGRepairFadeCommandlet(){IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true;}
int32 UVTGRepairFadeCommandlet::Main(const FString& Params)
{
 auto* BP=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/2DArt/ScreenTransition/WBP_Fade.WBP_Fade"));
 if(!BP)return 1;
 if(!Params.Contains(TEXT("Inspect")))return FadeRepair::Repair(BP,Params.Contains(TEXT("Verify")));
 for(auto& V:BP->NewVariables)UE_LOG(LogTemp,Display,TEXT("FADE VAR %s default=%s"),*V.VarName.ToString(),*V.DefaultValue);
 BP->WidgetTree->ForEachWidget([](UWidget* W){UE_LOG(LogTemp,Display,TEXT("FADE WIDGET %s type=%s opacity=%f"),*W->GetName(),*W->GetClass()->GetName(),W->GetRenderOpacity());});
 for(auto A:BP->Animations){UE_LOG(LogTemp,Display,TEXT("FADE ANIM %s start=%f end=%f"),*A->GetName(),A->GetStartTime(),A->GetEndTime());
 for(auto& B:A->AnimationBindings)UE_LOG(LogTemp,Display,TEXT("FADE BIND %s"),*B.WidgetName.ToString());
 for(auto& B:A->GetMovieScene()->GetBindings())for(auto T:B.GetTracks()){
 auto P=Cast<UMovieScenePropertyTrack>(T);UE_LOG(LogTemp,Display,TEXT("FADE TRACK %s prop=%s"),*T->GetName(),P?*P->GetPropertyPath().ToString():TEXT("?"));
 for(auto S:T->GetAllSections())if(auto F=Cast<UMovieSceneFloatSection>(S)){auto D=F->GetChannel().GetData();for(int I=0;I<D.GetTimes().Num();++I)UE_LOG(LogTemp,Display,TEXT("FADE KEY %d = %f"),D.GetTimes()[I].Value,D.GetValues()[I].Value);}
 }}
 for(auto G:BP->UbergraphPages)for(auto N:G->Nodes){UE_LOG(LogTemp,Display,TEXT("FADE NODE %s %s"),*N->GetName(),*N->GetNodeTitle(ENodeTitleType::FullTitle).ToString());for(auto P:N->Pins){FString L;for(auto Q:P->LinkedTo)L+=Q->GetOwningNode()->GetName()+TEXT(".")+Q->PinName.ToString()+TEXT(" ");UE_LOG(LogTemp,Display,TEXT("FADE PIN %s = %s -> %s"),*P->PinName.ToString(),*P->DefaultValue,*L);}}
 return 0;
}

#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Sections/MovieSceneColorSection.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_IfThenElse.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
namespace FadeRepair {
UEdGraphPin* P(UEdGraphNode* N,const TCHAR* S){auto R=N->FindPin(S);checkf(R,TEXT("Missing %s on %s"),S,*N->GetName());return R;}
void L(UEdGraphNode* A,const TCHAR* AP,UEdGraphNode* B,const TCHAR* BP){check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(P(A,AP),P(B,BP)));}
void D(UEdGraphNode* N,const TCHAR* S,const TCHAR* V){GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*P(N,S),V);}
template<class T>T* Add(UEdGraph* G,int X,int Y){auto N=NewObject<T>(G);G->AddNode(N,false,false);N->CreateNewGuid();N->NodePosX=X;N->NodePosY=Y;return N;}
UK2Node_CallFunction* C(UEdGraph* G,UClass* K,const TCHAR* F,int X,int Y){auto N=Add<UK2Node_CallFunction>(G,X,Y);auto Fn=K->FindFunctionByName(F);check(Fn);N->SetFromFunction(Fn);N->AllocateDefaultPins();return N;}
UK2Node_VariableGet* V(UEdGraph* G,UWidgetBlueprint* B,const TCHAR* S,int X,int Y){auto N=Add<UK2Node_VariableGet>(G,X,Y);auto Prop=FindFProperty<FProperty>(B->SkeletonGeneratedClass,S);check(Prop);N->SetFromProperty(Prop,true,B->SkeletonGeneratedClass);N->AllocateDefaultPins();return N;}
UK2Node_Event* E(UEdGraph* G,const TCHAR* S,int X,int Y){auto N=Add<UK2Node_Event>(G,X,Y);N->EventReference.SetExternalMember(S,UUserWidget::StaticClass());N->bOverrideFunction=true;N->AllocateDefaultPins();return N;}
UK2Node_IfThenElse* Branch(UEdGraph* G,int X,int Y){auto N=Add<UK2Node_IfThenElse>(G,X,Y);N->AllocateDefaultPins();return N;}
int Repair(UWidgetBlueprint* B,bool Verify){
 if(!Verify){
 auto G=B->UbergraphPages[0];
 // This widget's event graph contains only its fade lifecycle.
 check(B->UbergraphPages.Num()==1);
 auto Old=G->Nodes;for(auto N:Old)FBlueprintEditorUtils::RemoveNode(B,N,true);
 auto Start=E(G,TEXT("Construct"),0,0);
 auto Stop=C(G,UUserWidget::StaticClass(),TEXT("StopAllAnimations"),220,0);
 auto Image=V(G,B,TEXT("Black"),220,220);
 auto Color=C(G,UImage::StaticClass(),TEXT("SetColorAndOpacity"),450,0);
 D(Color,TEXT("InColorAndOpacity"),TEXT("(R=0,G=0,B=0,A=1)"));L(Image,TEXT("Black"),Color,TEXT("self"));
 auto Opacity=C(G,UWidget::StaticClass(),TEXT("SetRenderOpacity"),720,0);D(Opacity,TEXT("InOpacity"),TEXT("1"));L(Image,TEXT("Black"),Opacity,TEXT("self"));
 auto Disabled=V(G,B,TEXT("DisableFade?"),960,200);auto Gate=Branch(G,960,0);
 L(Start,TEXT("then"),Stop,TEXT("execute"));L(Stop,TEXT("then"),Color,TEXT("execute"));L(Color,TEXT("then"),Opacity,TEXT("execute"));L(Opacity,TEXT("then"),Gate,TEXT("execute"));L(Disabled,TEXT("DisableFade?"),Gate,TEXT("Condition"));
 Gate->NodeComment=TEXT("True: keep the screen black indefinitely. False: hold, then fade.");Gate->bCommentBubbleVisible=true;
 auto Hold=C(G,UKismetSystemLibrary::StaticClass(),TEXT("Delay"),1220,80);auto Duration=V(G,B,TEXT("HoldDurationUntilFade"),1220,260);
 L(Gate,TEXT("else"),Hold,TEXT("execute"));L(Duration,TEXT("HoldDurationUntilFade"),Hold,TEXT("Duration"));
 auto Recheck=Branch(G,1470,80);L(Hold,TEXT("then"),Recheck,TEXT("execute"));L(Disabled,TEXT("DisableFade?"),Recheck,TEXT("Condition"));
 auto Anim=V(G,B,TEXT("Anim_FadeOut"),1690,300);auto Play=C(G,UUserWidget::StaticClass(),TEXT("PlayAnimation"),1690,80);
 L(Recheck,TEXT("else"),Play,TEXT("execute"));L(Anim,TEXT("Anim_FadeOut"),Play,TEXT("InAnimation"));D(Play,TEXT("PlaybackSpeed"),TEXT("1"));D(Play,TEXT("NumLoopsToPlay"),TEXT("1"));D(Play,TEXT("bRestoreState"),TEXT("false"));
 auto Finished=E(G,TEXT("OnAnimationFinished"),0,520);auto Equal=C(G,UKismetMathLibrary::StaticClass(),TEXT("EqualEqual_ObjectObject"),250,690);auto Which=V(G,B,TEXT("Anim_FadeOut"),0,830);auto FinishGate=Branch(G,510,520);
 L(Finished,TEXT("Animation"),Equal,TEXT("A"));L(Which,TEXT("Anim_FadeOut"),Equal,TEXT("B"));L(Equal,TEXT("ReturnValue"),FinishGate,TEXT("Condition"));L(Finished,TEXT("then"),FinishGate,TEXT("execute"));
 auto Remove=C(G,UWidget::StaticClass(),TEXT("RemoveFromParent"),780,520);L(FinishGate,TEXT("then"),Remove,TEXT("execute"));
 auto Black=CastChecked<UImage>(B->WidgetTree->FindWidget(TEXT("Black")));Black->SetColorAndOpacity(FLinearColor::Black);Black->SetRenderOpacity(1);
 for(auto& Var:B->NewVariables)if(Var.VarName==TEXT("DisableFade?"))Var.DefaultValue=TEXT("False");
 auto Flag=FindFProperty<FBoolProperty>(B->GeneratedClass,TEXT("DisableFade?"));check(Flag);Flag->SetPropertyValue_InContainer(B->GeneratedClass->GetDefaultObject(),false);
 check(B->Animations.Num()==1);auto A=B->Animations[0];check(A->GetName()==TEXT("Anim_FadeOut"));int Count=0;
 for(auto& Binding:A->GetMovieScene()->GetBindings())for(auto T:Binding.GetTracks())for(auto S:T->GetAllSections())if(auto ColorSection=Cast<UMovieSceneColorSection>(S)){
 auto& Alpha=ColorSection->GetAlphaChannel();Alpha.Reset();Alpha.SetDefault(1);Alpha.AddLinearKey(A->GetMovieScene()->GetPlaybackRange().GetLowerBoundValue(),1);Alpha.AddLinearKey(A->GetMovieScene()->GetPlaybackRange().GetUpperBoundValue()-1,0);
 for(auto Channel:{&ColorSection->GetRedChannel(),&ColorSection->GetGreenChannel(),&ColorSection->GetBlueChannel()}){Channel->Reset();Channel->SetDefault(0);}
 S->SetCompletionMode(EMovieSceneCompletionMode::KeepState);++Count;
 }check(Count==1);
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B);
 }
 FCompilerResultsLog Results;FKismetEditorUtilities::CompileBlueprint(B,EBlueprintCompileOptions::None,&Results);
 UE_LOG(LogTemp,Display,TEXT("FADE COMPILE errors=%d warnings=%d"),Results.NumErrors,Results.NumWarnings);if(Results.NumErrors)return 3;
 check(!FindFProperty<FBoolProperty>(B->GeneratedClass,TEXT("DisableFade?"))->GetPropertyValue_InContainer(B->GeneratedClass->GetDefaultObject()));
 if(Verify)return 0;
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
 auto File=FPackageName::LongPackageNameToFilename(B->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
 if(!UPackage::SavePackage(B->GetOutermost(),B,*File,Args))return 4;
 UE_LOG(LogTemp,Display,TEXT("FADE SAVED %s"),*File);return 0;
}
}

