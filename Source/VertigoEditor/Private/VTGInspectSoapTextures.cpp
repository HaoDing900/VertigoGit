#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "ImageCore.h"
#include "PixelFormat.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "EditorFramework/AssetImportData.h"
int32 VTGInspectSoapTextures()
{
 FString Report;
 const FString Directory=FPaths::ProjectSavedDir()/"SlideshowTextureReview";
 IFileManager::Get().MakeDirectory(*Directory,true);
 for(const TCHAR* Name:{TEXT("ep1_calvin"),TEXT("ep1_nika_copy"),TEXT("Poster_copy")})
 {
  FString Path=FString::Printf(TEXT("/Game/2DArt/SoapTV/ep1/%s.%s"),Name,Name);
  auto* T=LoadObject<UTexture2D>(nullptr,*Path);if(!T)return 1;
  T->ForceRebuildPlatformData();
  T->FinishCachePlatformData();
  auto* Platform=T->GetPlatformData();
  const FString Format=Platform?GPixelFormats[Platform->PixelFormat].Name:TEXT("None");
  const FString Compression=StaticEnum<TextureCompressionSettings>()->GetNameStringByValue(T->CompressionSettings);
  FString Entry=FString::Printf(TEXT("%s\nSource=%dx%d; Runtime=%dx%d; Compression=%s; PixelFormat=%s; Mips=%d; GPUDataBytes=%d; Group=%s; MipGen=%s; MaxTextureSize=%d; SRGB=%d; CompressionNoAlpha=%d; NeverStream=%d; SourceFile=%s\n"),Name,T->Source.GetSizeX(),T->Source.GetSizeY(),T->GetSizeX(),T->GetSizeY(),*Compression,*Format,T->GetNumMips(),T->CalcTextureMemorySizeEnum(TMC_AllMips),UTexture::GetTextureGroupString(T->LODGroup),UTexture::GetMipGenSettingsString(T->MipGenSettings),T->MaxTextureSize,T->SRGB,T->CompressionNoAlpha,T->NeverStream,T->AssetImportData?*T->AssetImportData->GetFirstFilename():TEXT(""));
  // Transient comparison only: never save or alter the original asset.
  auto* Comparison=DuplicateObject<UTexture2D>(T,GetTransientPackage());
  Comparison->CompressionSettings=TC_BC7;
  Comparison->ForceRebuildPlatformData();Comparison->FinishCachePlatformData();
  auto* ComparePlatform=Comparison->GetPlatformData();
  Entry+=FString::Printf(TEXT("BC7TestFormat=%s; BC7TestGPUBytes=%u\n"),ComparePlatform?GPixelFormats[ComparePlatform->PixelFormat].Name:TEXT("None"),Comparison->CalcTextureMemorySizeEnum(TMC_AllMips));
  Report+=Entry;
  UE_LOG(LogTemp,Display,TEXT("SOAP TEXTURE %s"),*Entry);
  FImage Image;
  if(!T->Source.GetMipImage(Image,0)||!FImageUtils::SaveImageByExtension(*(Directory/(FString(Name)+TEXT(".png"))),Image))return 2;
 }
 return FFileHelper::SaveStringToFile(Report,*(Directory/"TextureReport.txt"))?0:3;
}



#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
int32 VTGOptimizeSoapTextures()
{
 const FString SourceDir=FPaths::ProjectDir()/"SourceArt/SoapTV/ep1";
 const FString BackupDir=FPaths::ProjectSavedDir()/"SlideshowTextureBackup";
 const FString PreviewDir=FPaths::ProjectSavedDir()/"SlideshowTextureReview/Optimized";
 IFileManager::Get().MakeDirectory(*SourceDir,true);
 IFileManager::Get().MakeDirectory(*BackupDir,true);
 IFileManager::Get().MakeDirectory(*PreviewDir,true);
 TArray<UTexture2D*> Textures;
 FString Report;
 for(const TCHAR* Name:{TEXT("ep1_calvin"),TEXT("ep1_nika_copy"),TEXT("Poster_copy")})
 {
  const bool Poster=FString(Name)==TEXT("Poster_copy");
  const FString AssetPath=FString::Printf(TEXT("/Game/2DArt/SoapTV/ep1/%s.%s"),Name,Name);
  auto* T=LoadObject<UTexture2D>(nullptr,*AssetPath);if(!T)return 10;
  const FString AssetFile=FPackageName::LongPackageNameToFilename(T->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
  const FString BackupFile=BackupDir/(FString(Name)+TEXT(".uasset"));
  if(!IFileManager::Get().FileExists(*BackupFile)&&IFileManager::Get().Copy(*BackupFile,*AssetFile)!=COPY_OK)return 11;
  FImage Source,Resized;if(!T->Source.GetMipImage(Source,0))return 12;
  const int W=Poster?1920:2640,H=Poster?2844:1440;
  Source.ResizeTo(Resized,W,H,ERawImageFormat::BGRA8,EGammaSpace::sRGB);
  // Preserve compact JPEG source storage, as in the original three assets.
  // Optimized reimport files live outside Content, so they are not cooked.
  const FString SourceFile=SourceDir/(FString(Name)+TEXT(".jpg"));
  if(!FImageUtils::SaveImageByExtension(*SourceFile,Resized,92))return 13;
  TArray64<uint8> Jpeg;if(!FFileHelper::LoadFileToArray(Jpeg,*SourceFile))return 14;
  T->Modify();T->Source.InitWithCompressedSourceData(W,H,1,TSF_BGRA8,MakeArrayView(Jpeg),ETextureSourceCompressionFormat::TSCF_JPEG);
  T->CompressionSettings=Poster?TC_BC7:TC_Default;
  T->LODGroup=TEXTUREGROUP_UI;T->MipGenSettings=TMGS_NoMipmaps;T->NeverStream=true;T->SRGB=true;T->MaxTextureSize=0;
  if(T->AssetImportData)T->AssetImportData->Update(SourceFile);
  T->PostEditChange();T->FinishCachePlatformData();
  auto* Platform=T->GetPlatformData();
  if(!Platform||T->GetSizeX()!=W||T->GetSizeY()!=H||Platform->PixelFormat!=(Poster?PF_BC7:PF_DXT1))return 15;
  Report+=FString::Printf(TEXT("%s %dx%d %s GPUBytes=%u\n"),Name,W,H,GPixelFormats[Platform->PixelFormat].Name,T->CalcTextureMemorySizeEnum(TMC_AllMips));
  // Export GPU block data as DDS so the compressed result can be inspected.
  void* MipData=nullptr;T->GetMipData(0,&MipData);
  if(MipData)
  {
   const int BlockBytes=Poster?16:8;const uint32 PayloadBytes=((W+3)/4)*((H+3)/4)*BlockBytes;
   TArray<uint8> DDS;DDS.SetNumZeroed(Poster?148:128);
   auto Put=[&DDS](int Offset,uint32 Value){FMemory::Memcpy(DDS.GetData()+Offset,&Value,4);};
   Put(0,0x20534444);Put(4,124);Put(8,0x81007);Put(12,H);Put(16,W);Put(20,PayloadBytes);Put(28,1);
   Put(76,32);Put(80,4);Put(84,Poster?0x30315844:0x31545844);Put(108,0x1000);
   if(Poster){Put(128,99);Put(132,3);Put(140,1);}
   DDS.Append(static_cast<uint8*>(MipData),PayloadBytes);FMemory::Free(MipData);
   if(!FFileHelper::SaveArrayToFile(DDS,*(PreviewDir/(FString(Name)+TEXT(".dds")))))return 16;
  }
  Textures.Add(T);
 }
 // Save after all three candidate textures passed size and format validation.
 for(auto* T:Textures)
 {
  FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
  const FString File=FPackageName::LongPackageNameToFilename(T->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
  if(!UPackage::SavePackage(T->GetOutermost(),T,*File,Args))return 17;
 }
 FFileHelper::SaveStringToFile(Report,*(PreviewDir/"OptimizationReport.txt"));
 UE_LOG(LogTemp,Display,TEXT("SOAP OPTIMIZATION PASS\n%s"),*Report);return 0;
}
