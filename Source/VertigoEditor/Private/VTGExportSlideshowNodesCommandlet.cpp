#include "VTGExportSlideshowNodesCommandlet.h"
#include "UI/VTGSlideshow.h"
#include "NarrativeComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphUtilities.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_AddDelegate.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Kismet/KismetSystemLibrary.h"

UVTGExportSlideshowNodesCommandlet::UVTGExportSlideshowNodesCommandlet() { IsEditor=true; IsClient=false; IsServer=false; LogToConsole=true; }
namespace SlideNodes
{
 template<class T> T* Add(UEdGraph* G, int X, int Y)
 { auto* N=NewObject<T>(G); G->AddNode(N,false,false); N->CreateNewGuid(); N->NodePosX=X; N->NodePosY=Y; return N; }
 UK2Node_CallFunction* Call(UEdGraph* G,UClass* Class,const TCHAR* Name,int X,int Y)
 { auto* N=Add<UK2Node_CallFunction>(G,X,Y); N->SetFromFunction(Class->FindFunctionByName(Name)); N->AllocateDefaultPins(); return N; }
 void Link(UEdGraphNode* A,const TCHAR* AP,UEdGraphNode* B,const TCHAR* BP)
 { check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A->FindPinChecked(AP),B->FindPinChecked(BP))); }
 bool ExportAndCheck(UEdGraph* G, const FString& Filename)
 {
  TSet<UObject*> Nodes; for(UEdGraphNode* N:G->Nodes)Nodes.Add(N);
  FString Text; FEdGraphUtilities::ExportNodesToText(Nodes,Text);
  // Clipboard event signatures resolve against the destination Blueprint, not our transient source.
  Text = Text.Replace(TEXT("MemberParent=\"/Script/Engine.BlueprintGeneratedClass'/Engine/Transient.SlideshowClipboardSource_C'\","),TEXT("bSelfContext=True,"));
  const TArray<FString> Defaults = {
   TEXT("PinType.PinSubCategory=\"\","),TEXT("PinType.PinSubCategoryObject=None,"),TEXT("PinType.PinSubCategoryMemberReference=(),"),TEXT("PinType.PinValueType=(),"),TEXT("PinType.ContainerType=None,"),
   TEXT("PinType.bIsReference=False,"),TEXT("PinType.bIsConst=False,"),TEXT("PinType.bIsWeakPointer=False,"),TEXT("PinType.bIsUObjectWrapper=False,"),TEXT("PinType.bSerializeAsSinglePrecisionFloat=False,"),
   TEXT("PersistentGuid=00000000000000000000000000000000,"),TEXT("bHidden=False,"),TEXT("bNotConnectable=False,"),TEXT("bDefaultValueIsReadOnly=False,"),TEXT("bDefaultValueIsIgnored=False,"),TEXT("bAdvancedView=False,"),TEXT("bOrphanedPin=False,")};
  for(const FString& Default : Defaults) Text=Text.Replace(*Default,TEXT(""));
  auto* Blueprint=FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),GetTransientPackage(),MakeUniqueObjectName(GetTransientPackage(),UBlueprint::StaticClass(),TEXT("ClipboardImportCheck")),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
  auto* Imported=FBlueprintEditorUtils::CreateNewGraph(Blueprint, FName(*("Check"+Filename)), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
  FBlueprintEditorUtils::AddUbergraphPage(Blueprint,Imported);
  if(!FEdGraphUtilities::CanImportNodesFromText(Imported,Text)) { UE_LOG(LogTemp,Error,TEXT("Cannot import %s (%d nodes, %d chars)"),*Filename,G->Nodes.Num(),Text.Len()); return false; }
  TSet<UEdGraphNode*> Result; FEdGraphUtilities::ImportNodesFromText(Imported,Text,Result);
  if(Result.Num()!=G->Nodes.Num())return false;
  if (Filename.Contains(TEXT("StartEnding")))
  {
   FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
   FCompilerResultsLog ImportLog; FKismetEditorUtilities::CompileBlueprint(Blueprint,EBlueprintCompileOptions::None,&ImportLog);
   if(ImportLog.NumErrors)return false;
  }
  return FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectDir()/"Docs"/Filename),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
 }
}
int32 VTGSetupSoapTVExample(const FString& Params);
int32 VTGInspectSoapTextures();
int32 VTGOptimizeSoapTextures();
int32 UVTGExportSlideshowNodesCommandlet::Main(const FString& Params)
{
 if(Params.Contains(TEXT("OptimizeSoapTextures")))return VTGOptimizeSoapTextures();
 if(Params.Contains(TEXT("InspectSoapTextures")))return VTGInspectSoapTextures();
 if(Params.Contains(TEXT("SoapExample")))return VTGSetupSoapTVExample(Params);
 using namespace SlideNodes;
 auto* BP=FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),GetTransientPackage(),TEXT("SlideshowClipboardSource"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
 auto* G=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("EndingNodes"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
 FBlueprintEditorUtils::AddUbergraphPage(BP,G);
 auto* Start=Add<UK2Node_CustomEvent>(G,0,0); Start->CustomFunctionName=TEXT("StartEnding_A"); Start->AllocateDefaultPins();
 FEdGraphPinType Type; Type.PinCategory=UEdGraphSchema_K2::PC_Object;
 Type.PinSubCategoryObject=AVTGSlideshowDirector::StaticClass(); Start->CreateUserDefinedPin(TEXT("Slideshow_A"),Type,EGPD_Output);
 Type.PinSubCategoryObject=UNarrativeComponent::StaticClass(); Start->CreateUserDefinedPin(TEXT("NarrativeComp"),Type,EGPD_Output);
 auto* Bind=Add<UK2Node_AddDelegate>(G,400,0);
 Bind->SetFromProperty(FindFProperty<FMulticastDelegateProperty>(AVTGSlideshowDirector::StaticClass(),TEXT("OnSlideshowClosed")),false,AVTGSlideshowDirector::StaticClass()); Bind->AllocateDefaultPins();
 auto* Begin=Call(G,UNarrativeComponent::StaticClass(),TEXT("BeginDialogue"),780,0);
 auto* Closed=Add<UK2Node_CustomEvent>(G,400,320); Closed->CustomFunctionName=TEXT("HandleEndingSlideshowClosed"); Closed->AllocateDefaultPins();
 Link(Start,TEXT("then"),Bind,TEXT("execute")); Link(Start,TEXT("Slideshow_A"),Bind,TEXT("self"));
 check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Closed->FindPinChecked(TEXT("OutputDelegate")),Bind->GetDelegatePin()));
 Link(Bind,TEXT("then"),Begin,TEXT("execute")); Link(Start,TEXT("NarrativeComp"),Begin,TEXT("self"));
 auto* Print=Call(G,UKismetSystemLibrary::StaticClass(),TEXT("PrintString"),780,320);
 Print->FindPinChecked(TEXT("InString"))->DefaultValue=TEXT("Slideshow closed: replace this Print String with your next-level / credits logic");
 Link(Closed,TEXT("then"),Print,TEXT("execute"));
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 FCompilerResultsLog Log; FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);
 if(Log.NumErrors)return 1;
 if(!ExportAndCheck(G,TEXT("Slideshow_StartEnding_A.nodes.txt")))return 2;
 auto* Direct=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("DirectSlideCalls"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
 auto* Show=Call(Direct,AVTGSlideshowDirector::StaticClass(),TEXT("ShowSlide"),0,0); Show->FindPinChecked(TEXT("SlideNumber"))->DefaultValue=TEXT("1");
 auto* Close=Call(Direct,AVTGSlideshowDirector::StaticClass(),TEXT("CloseSlideshow"),420,0); Close->FindPinChecked(TEXT("FadeSeconds"))->DefaultValue=TEXT("0.5");
 if(!ExportAndCheck(Direct,TEXT("Slideshow_DirectCalls.nodes.txt")))return 3;
 UE_LOG(LogTemp,Display,TEXT("SLIDESHOW CLIPBOARD EXPORT PASS: source compiled, both exports re-imported successfully"));
 return 0;
}






