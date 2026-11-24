#include "DialogueBlueprintGeneratedClass.h"
#include "Dialogue.h"
#include "DialogueSM.h"

/**
 * Dropping the per-NPC-line records is only safe while nothing conditions on an NPC line. Every
 * Cond_PlayedNode in the project points at a player option node today; this asserts it stays that way,
 * or that the dialogue in question opted back in with bRecordEveryNodePlayed. Without this guard a
 * designer adding one condition would get a silently-never-true condition months later.
 */
struct FVTGDialogueRecordAudit
{
 static int32 Run()
 {
  TArray<FString> Files;
  IFileManager::Get().FindFilesRecursive(Files,*(FPaths::ProjectContentDir()/TEXT("Narrative/Dialogues")),TEXT("*.uasset"),true,false);
  int32 Dialogues=0,Conditions=0,OptedIn=0;
  for(const FString& File : Files)
  {
   FString Package;
   if(!FPackageName::TryConvertFilenameToLongPackageName(File,Package))continue;
   UObject* Object=LoadObject<UObject>(nullptr,*Package);
   if(!Object)continue;
   auto* BP=Cast<UBlueprint>(Object);
   if(!BP)
    if(auto* P=FindFProperty<FObjectPropertyBase>(Object->GetClass(),TEXT("DirectorBlueprint")))
     BP=Cast<UBlueprint>(P->GetObjectPropertyValue_InContainer(Object));
   auto* Class=BP?Cast<UDialogueBlueprintGeneratedClass>(BP->GeneratedClass):nullptr;
   auto* Template=Class?Class->GetDialogueTemplate():nullptr;
   if(!Template)continue;
   ++Dialogues;
   if(Template->bRecordEveryNodePlayed){++OptedIn;continue;} // records everything, so nothing to prove

   TMap<FString,UClass*> IdToClass;
   for(auto* N : Template->GetNodes())
    if(auto* Id=FindFProperty<FNameProperty>(N->GetClass(),TEXT("ID")))
     IdToClass.Add(Id->GetPropertyValue_InContainer(N).ToString(),N->GetClass());

   for(auto* N : Template->GetNodes())
   {
    auto* CondArray=FindFProperty<FArrayProperty>(N->GetClass(),TEXT("Conditions"));
    auto* Inner=CondArray?CastField<FObjectPropertyBase>(CondArray->Inner):nullptr;
    if(!Inner)continue;
    FScriptArrayHelper Conds(CondArray,CondArray->ContainerPtrToValuePtr<void>(N));
    for(int32 I=0;I<Conds.Num();++I)
    {
     UObject* Cond=Inner->GetObjectPropertyValue(Conds.GetRawPtr(I));
     if(!Cond || !Cond->GetClass()->GetName().Contains(TEXT("PlayedNode")))continue;
     auto* NodeId=FindFProperty<FStrProperty>(Cond->GetClass(),TEXT("NodeID"));
     if(!NodeId)continue;
     const FString Target=NodeId->GetPropertyValue_InContainer(Cond);
     UClass** TargetClass=IdToClass.Find(Target);
     if(!TargetClass)
     {
      // Points at a node that no longer exists: already broken, and not what this audit is about.
      UE_LOG(LogTemp,Warning,TEXT("%s: Cond_PlayedNode points at \"%s\", which is not a node in this dialogue"),*FPaths::GetBaseFilename(Package),*Target);
      continue;
     }
     ++Conditions;
     const bool bPlayerNode=(*TargetClass)->IsChildOf(UDialogueNode_Player::StaticClass());
     if(!bPlayerNode)
      UE_LOG(LogTemp,Error,TEXT("%s: Cond_PlayedNode points at NPC node \"%s\" (%s), but this dialogue does not set bRecordEveryNodePlayed - that condition can never become true."),
       *FPaths::GetBaseFilename(Package),*Target,*(*TargetClass)->GetName());
     check(bPlayerNode);
    }
   }
  }
  UE_LOG(LogTemp,Display,TEXT("DIALOGUE RECORD AUDIT PASS: %d dialogues, %d Cond_PlayedNode conditions, all pointing at player option nodes; %d dialogues opted back in to recording every node."),Dialogues,Conditions,OptedIn);
  return 0;
 }
};
