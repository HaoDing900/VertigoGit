#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/CurveIdentifier.h"
#include "Animation/Skeleton.h"
#include "Misc/FileHelper.h"

static int32 AdjustL2EyeTremor(const FString& Params)
{
 const bool Verify=Params.Contains(TEXT("VerifyOnly"));
 const bool Micro5=Params.Contains(TEXT("Micro5"));
 const bool Fast15=Params.Contains(TEXT("Fast15")) || Micro5;
 auto* Anim=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L2BarStreet/Anim/LSFace__l2BarFightSuprisedAnger"));check(Anim);
 const FString Dir=FPaths::ProjectDir()/TEXT("Scripts/L2EyeTremor");
 const FString ExpectedFile=Dir/(Micro5?TEXT("expected-micro5.txt"):Fast15?TEXT("expected-fast15.txt"):TEXT("expected.txt"));
 const auto* Model=Anim->GetDataModel();const auto& Ref=Anim->GetSkeleton()->GetReferenceSkeleton();
 const float Start=1.06f,Factor=Fast15?.5f:.3f;
 const float Speed=3.f,TremorEnd=Start+(Anim->GetPlayLength()-Start)/Speed,FreezeTime=1.56f;
 const float MicroStart=1.64f,MicroFull=1.68f,EditStart=Micro5?MicroStart:Start;
 TArray<float> Times;for(int32 I=0;I<=FMath::RoundToInt(Anim->GetPlayLength()*250.f);++I)Times.Add(FMath::Min(I/250.f,Anim->GetPlayLength()));Times.AddUnique(Start);Times.Sort();
 if(Fast15){Times.AddUnique(TremorEnd);Times.AddUnique(FreezeTime);Times.Sort();}
 if(Micro5){Times.AddUnique(MicroStart);Times.AddUnique(MicroFull);Times.Sort();}
 TArray<FName> Names;Model->GetBoneTrackNames(Names);
 auto Pose=[&](FName Name,float Time){FTransform P;Anim->GetBoneTransform(P,FSkeletonPoseBoneIndex(Ref.FindBoneIndex(Name)),Time,true);return P;};
 const FName Eyes[]={TEXT("c_eye_l"),TEXT("c_eye_r")};
 if(Verify)
 {
  TArray<FString> Lines;check(FFileHelper::LoadFileToStringArray(Lines,*ExpectedFile));float MaxError=0;
  for(const auto& Line:Lines){TArray<FString> Cells;Line.ParseIntoArray(Cells,TEXT("|"));check(Cells.Num()==5);FTransform Expected;check(Expected.InitFromString(Cells[2]+TEXT("|")+Cells[3]+TEXT("|")+Cells[4]));const auto Actual=Pose(FName(*Cells[0]),FCString::Atof(*Cells[1]));MaxError=FMath::Max(MaxError,float(Actual.GetRotation().AngularDistance(Expected.GetRotation())));check(Actual.GetTranslation().Equals(Expected.GetTranslation(),.0001));check(Actual.GetScale3D().Equals(Expected.GetScale3D(),.0001));}
  check(MaxError<.001f);UE_LOG(LogTemp,Display,TEXT("EYE VERIFY PASS: %d saved pose samples, max rotation error %.8f rad"),Lines.Num(),MaxError);return 0;
 }
 const FString Source=FPackageName::LongPackageNameToFilename(Anim->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
 const FString Backup=Dir/(Micro5?TEXT("Backup/LSFace__l2BarFightSuprisedAnger.before-micro5.uasset"):Fast15?TEXT("Backup/LSFace__l2BarFightSuprisedAnger.before-fast15.uasset"):TEXT("Backup/LSFace__l2BarFightSuprisedAnger.uasset"));
 check(!IFileManager::Get().FileExists(*Backup));IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);check(IFileManager::Get().Copy(*Backup,*Source)==COPY_OK);
 TMap<FName,TArray<FTransform>> Before;
 for(auto Name:Names){auto& Samples=Before.Add(Name);for(float Time:Times)Samples.Add(Pose(Name,Time));}
 TMap<FName,TArray<TPair<float,FQuat>>> Reference30;
 if(Micro5)
 {
  TArray<FString> Lines;check(FFileHelper::LoadFileToStringArray(Lines,*(Dir/TEXT("expected.txt"))));
  for(const auto& Line:Lines){TArray<FString> Cells;Line.ParseIntoArray(Cells,TEXT("|"));check(Cells.Num()==5);FTransform P;check(P.InitFromString(Cells[2]+TEXT("|")+Cells[3]+TEXT("|")+Cells[4]));Reference30.FindOrAdd(FName(*Cells[0])).Add(TPair<float,FQuat>(FCString::Atof(*Cells[1]),P.GetRotation()));}
 }
 auto Sample30=[&](FName Eye,float Time){const auto& Samples=Reference30.FindChecked(Eye);for(int32 I=1;I<Samples.Num();++I)if(Time<=Samples[I].Key)return FQuat::Slerp(Samples[I-1].Value,Samples[I].Value,FMath::Clamp((Time-Samples[I-1].Key)/(Samples[I].Key-Samples[I-1].Key),0.f,1.f)).GetNormalized();return Samples.Last().Value;};
 TMap<FName,TArray<FTransform>> Targets;
 for(auto Eye:Eyes)
 {
  const FTransform Base=Pose(Eye,Start);
  const FQuat Last=Pose(Eye,Anim->GetPlayLength()).GetRotation();
  for(int32 I=0;I<Times.Num();++I)
  {
   const float Time=Times[I];FTransform Desired=Before.FindChecked(Eye)[I];
   if(Micro5)
   {
    if(Time>=MicroStart)
    {
     const float Duration=Anim->GetPlayLength()-Start;const float Phase=FMath::Fmod((Time-MicroStart)*Speed,2.f*Duration);const float SourceTime=Start+(Phase<=Duration?Phase:2.f*Duration-Phase);
     const float A=FMath::Clamp((Time-MicroStart)/(MicroFull-MicroStart),0.f,1.f);const float Weight=(.05f/.30f)*A*A*(3.f-2.f*A);
     Desired.SetRotation(FQuat::Slerp(Base.GetRotation(),Sample30(Eye,SourceTime),Weight).GetNormalized());
    }
   }
   else if(Time>=Start)
   {
    FQuat Movement=Desired.GetRotation();float Weight=Factor;
    if(Fast15)
    {
     if(Time<TremorEnd)Movement=Pose(Eye,FMath::Min(Start+(Time-Start)*Speed,Anim->GetPlayLength())).GetRotation();
     else{Movement=Last;const float A=FMath::Clamp((Time-TremorEnd)/(FreezeTime-TremorEnd),0.f,1.f);Weight*=1.f-A*A*(3.f-2.f*A);}
    }
    Desired.SetRotation(FQuat::Slerp(Base.GetRotation(),Movement,Weight).GetNormalized());
   }
   Targets.FindOrAdd(Eye).Add(Desired);
  }
 }
 auto& Controller=Anim->GetController();Controller.OpenBracket(FText::FromString(Fast15?TEXT("15 percent eye tremor, 3x speed, quick settle and freeze"):TEXT("Reduce eye tremor after 1.06 seconds to 30 percent")),false);
 FString ExpectedText;
 for(auto Eye:Eyes)
 {
  check(Names.Contains(Eye));const FAnimationCurveIdentifier Id(Eye,ERawCurveTrackTypes::RCT_Transform);
  const auto* Existing=Model->FindTransformCurve(Id);check(Existing && !Existing->GetCurveTypeFlag(AACF_Disabled));const FTransformCurve Original=*Existing;
  TArray<FRichCurveKey> Keys[3];
  for(int32 Axis=0;Axis<3;++Axis)for(const auto& Key:Original.RotationCurve.FloatCurves[Axis].GetConstRefOfKeys())if(Key.Time<EditStart)Keys[Axis].Add(Key);
  FVector Previous=Original.RotationCurve.Evaluate(EditStart,1.f);
  for(int32 I=0;I<Times.Num();++I)
  {
   const float Time=Times[I];const auto Desired=Targets.FindChecked(Eye)[I];
   if(Time>=EditStart)
   {
    const auto Raw=Model->EvaluateBoneTrackTransform(Eye,Model->GetFrameRate().AsFrameTime(Time),Anim->Interpolation);
    const auto Correction=(Raw.GetRotation().Inverse()*Desired.GetRotation()).GetNormalized();FVector Euler=Correction.Euler();
    for(int32 Axis=0;Axis<3;++Axis){Euler[Axis]=Previous[Axis]+FMath::UnwindDegrees(Euler[Axis]-Previous[Axis]);FRichCurveKey Key(Time,Euler[Axis]);Key.InterpMode=RCIM_Linear;Keys[Axis].Add(Key);}Previous=Euler;
   }
   ExpectedText+=FString::Printf(TEXT("%s|%.9f|%s\n"),*Eye.ToString(),Time,*Desired.ToString());
  }
  for(int32 Axis=0;Axis<3;++Axis){auto Channel=Id;Channel.Channel=ETransformCurveChannel::Rotation;Channel.Axis=static_cast<EVectorCurveChannel>(Axis);check(Controller.SetCurveKeys(Channel,Keys[Axis],false));}
  UE_LOG(LogTemp,Display,TEXT("EYE edited %s rotation correction curves: baseline %.3fs, retained amplitude %.2f"),*Eye.ToString(),Start,Factor);
 }
 Controller.CloseBracket(false);
 int32 Unchanged=0;float MaxEyeError=0;
 for(auto Name:Names)for(int32 I=0;I<Times.Num();++I)
 {
  auto Expected=Before.FindChecked(Name)[I];const auto Actual=Pose(Name,Times[I]);
  if((Name==Eyes[0] || Name==Eyes[1]) && Times[I]>=EditStart)Expected=Targets.FindChecked(Name)[I];else ++Unchanged;
  const float Error=Actual.GetRotation().AngularDistance(Expected.GetRotation());MaxEyeError=FMath::Max(MaxEyeError,Error);check(Error<.001f);check(Actual.GetTranslation().Equals(Expected.GetTranslation(),.0001));check(Actual.GetScale3D().Equals(Expected.GetScale3D(),.0001));
 }
 if(Fast15)for(auto Eye:Eyes)
 {
  const auto Base=Pose(Eye,Start);
  for(float Time:Times)if(Time>=FreezeTime && (!Micro5 || Time<=MicroStart))check(Pose(Eye,Time).GetRotation().AngularDistance(Base.GetRotation())<.001f);
  UE_LOG(LogTemp,Display,TEXT("EYE FAST15: %s 3x tremor until %.6fs, smooth settle, baseline freeze from %.3fs"),*Eye.ToString(),TremorEnd,FreezeTime);
  if(Micro5){float MaxAngle=0;for(float Time:Times)if(Time>=MicroFull)MaxAngle=FMath::Max(MaxAngle,float(Pose(Eye,Time).GetRotation().AngularDistance(Base.GetRotation())));check(MaxAngle>.00001f);UE_LOG(LogTemp,Display,TEXT("EYE MICRO5: %s hold %.3f-%.3fs, fade in until %.3fs, 5 percent original tremor; max angle %.6f rad"),*Eye.ToString(),FreezeTime,MicroStart,MicroFull,MaxAngle);}
 }
 check(FFileHelper::SaveStringToFile(ExpectedText,*ExpectedFile));
 Anim->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;check(UPackage::SavePackage(Anim->GetOutermost(),Anim,*Source,Args));
 UE_LOG(LogTemp,Display,TEXT("EYE APPLY PASS: %d unchanged bone/time samples, max rotation error %.8f rad; saved %s"),Unchanged,MaxEyeError,*Source);return 0;
}

