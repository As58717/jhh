#include "EqrCaptureActor.h"
#include "Components/SceneCaptureComponentCube.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureRenderTargetCube.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformProcess.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Sound/SoundSubmixBase.h"
#include "Sound/SoundWave.h"
#include "EqrRenderer.h"
#include "Engine/Engine.h"
#include "RenderingThread.h"

AEqrCaptureActor::AEqrCaptureActor(){
  PrimaryActorTick.bCanEverTick=true;
  RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

  LeftCapture=CreateDefaultSubobject<USceneCaptureComponentCube>(TEXT("LeftCapture"));
  LeftCapture->SetupAttachment(RootComponent);
  LeftCapture->bCaptureEveryFrame=false; LeftCapture->bCaptureOnMovement=false; LeftCapture->CaptureSource=ESceneCaptureSource::SCS_FinalColorHDR;

  RightCapture=CreateDefaultSubobject<USceneCaptureComponentCube>(TEXT("RightCapture"));
  RightCapture->SetupAttachment(RootComponent);
  RightCapture->bCaptureEveryFrame=false; RightCapture->bCaptureOnMovement=false; RightCapture->CaptureSource=ESceneCaptureSource::SCS_FinalColorHDR;

  LeftCubeRT = NewObject<UTextureRenderTargetCube>(this);
  LeftCubeRT->RenderTargetFormat=RTF_RGBA16f; LeftCubeRT->InitAutoFormat(1024); LeftCubeRT->bAutoGenerateMips=false; LeftCubeRT->UpdateResourceImmediate(true);
  LeftCapture->TextureTarget=LeftCubeRT;

  RightCubeRT = NewObject<UTextureRenderTargetCube>(this);
  RightCubeRT->RenderTargetFormat=RTF_RGBA16f; RightCubeRT->InitAutoFormat(1024); RightCubeRT->bAutoGenerateMips=false; RightCubeRT->UpdateResourceImmediate(true);
  RightCapture->TextureTarget=RightCubeRT;

  EquirectRT = NewObject<UTextureRenderTarget2D>(this);
  EquirectRT->RenderTargetFormat=ETextureRenderTargetFormat::RTF_RGBA16f;
  EquirectRT->InitAutoFormat(Width, Height);
  EquirectRT->bAutoGenerateMips=false; EquirectRT->UpdateResourceImmediate(true);
}

void AEqrCaptureActor::BeginPlay(){ Super::BeginPlay(); FrameInterval=1.0/FMath::Max(1.0f,TargetFPS); }

void AEqrCaptureActor::EnsureTargets(){
  const int32 Face=FMath::Max(512, Width/2);
  LeftCubeRT->InitAutoFormat(Face); LeftCubeRT->UpdateResourceImmediate(true);
  RightCubeRT->InitAutoFormat(Face); RightCubeRT->UpdateResourceImmediate(true);

#if EASY360_ENABLE_NVENC
  EquirectRT->RenderTargetFormat = (bUseNvenc ? ETextureRenderTargetFormat::RTF_RGBA8 : ETextureRenderTargetFormat::RTF_RGBA16f);
#else
  EquirectRT->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA16f;
#endif
  EquirectRT->InitAutoFormat(Width,Height); EquirectRT->UpdateResourceImmediate(true);
}

void AEqrCaptureActor::BuildSessionPaths(){
  FString Base=OutputDir; if(Base.IsEmpty()) Base=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Movies"),TEXT("Easy360"));
  const FString Stamp=FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
  CurrentSessionDir=FPaths::Combine(Base,Stamp);
  IPlatformFile& PF=FPlatformFileManager::Get().GetPlatformFile();
  if(!PF.CreateDirectoryTree(*CurrentSessionDir))
  {
    UE_LOG(LogTemp, Warning, TEXT("[Easy360] Failed to create capture directory: %s"), *CurrentSessionDir);
  }
  PngPattern=FPaths::Combine(CurrentSessionDir,TEXT("frame_%06d.png"));
  MoviePath =FPaths::Combine(CurrentSessionDir,TEXT("output.mp4"));
  AudioWavPath=FPaths::Combine(CurrentSessionDir,TEXT("audio.wav"));
  const FString NvExt = bNvencUseHEVC ? TEXT("nvenc.hevc") : TEXT("nvenc.h264");
  NvencBitstreamPath = FPaths::Combine(CurrentSessionDir, NvExt);
}

FString AEqrCaptureActor::ResolveFFmpeg() const{
  if(!FFmpegExePath.IsEmpty() && FPaths::FileExists(FFmpegExePath)) return FFmpegExePath;
  if(FPlatformProcess::ExecutableExists(TEXT("ffmpeg"))) return TEXT("ffmpeg");
  TArray<FString> C; C.Add(TEXT("C:/ffmpeg/bin/ffmpeg.exe")); C.Add(TEXT("C:/Program Files/ffmpeg/bin/ffmpeg.exe")); C.Add(TEXT("C:/Program Files (x86)/ffmpeg/bin/ffmpeg.exe"));
  for(const FString& P : C) if(FPaths::FileExists(P)) return P;
  return TEXT("");
}

void AEqrCaptureActor::StartAudioRecord()
{
  if(!bRecordAudio)
  {
    return;
  }

  UAudioMixerBlueprintLibrary::StartRecordingOutput(this, 0.f, SubmixToRecord.Get());
}

void AEqrCaptureActor::StopAudioRecord()
{
  if(!bRecordAudio)
  {
    return;
  }

  if(USoundWave* Captured = UAudioMixerBlueprintLibrary::StopRecordingOutput(
        this, EAudioRecordingExportType::WavFile, TEXT("audio"), CurrentSessionDir, SubmixToRecord.Get()))
  {
    Captured->MarkAsGarbage();
  }
}

bool AEqrCaptureActor::WaitForAudioReady() const{
  const double End = FPlatformTime::Seconds() + 2.0;
  while(FPlatformTime::Seconds()<End){
    const int64 s1 = IFileManager::Get().FileSize(*AudioWavPath);
    if(s1>0){
      FPlatformProcess::Sleep(0.1f);
      const int64 s2 = IFileManager::Get().FileSize(*AudioWavPath);
      if(s2==s1) return true;
    }else{
      FPlatformProcess::Sleep(0.1f);
    }
  }
  return false;
}

void AEqrCaptureActor::StartCapture(){
  Width=FMath::Max(64,Width); Height=FMath::Max(32,Height);
  if(Width%2) ++Width; if(Height%2) ++Height;
  FrameInterval=1.0/FMath::Max(1.0f,TargetFPS);
  EnsureTargets();
  FlushRenderingCommands();
  bCapturing=true; Accum=0.0; FrameIndex=0;
  if(bRecordSequence || bUseNvenc || bPipeEncode) BuildSessionPaths();
  StartAudioRecord();
  NvencStartIfNeeded();
}

void AEqrCaptureActor::StopCapture(){
  bCapturing=false;
  StopAudioRecord();
  if(bRecordAudio){ WaitForAudioReady(); }

  if(bUseNvenc){ NvencStop(); }
  if(bRecordSequence && FrameIndex>0){ RunFFmpegMux(); }
}

void AEqrCaptureActor::Tick(float dt){
  Super::Tick(dt); if(!bCapturing) return;
  Accum+=dt;
  while(Accum>=FrameInterval){
    Accum-=FrameInterval;
    CaptureOnceAndConvert();

    if(bRecordSequence){
      FlushRenderingCommands();
      const FString Path=FPaths::Combine(CurrentSessionDir,FString::Printf(TEXT("frame_%06lld.png"),FrameIndex));
      UKismetRenderingLibrary::ExportRenderTarget(this,EquirectRT,Path); ++FrameIndex;
    }
    if(bUseNvenc && bNvencActive){
      FlushRenderingCommands();
      NvencSubmitFrame();
    }
    // bPipeEncode: 此示例未实现回读喂管，可后续扩展
  }
}

void AEqrCaptureActor::CaptureOnceAndConvert(){
  const float HalfIPD = (IPDcm*0.01f)*0.5f;
  LeftCapture->SetWorldLocation(GetActorLocation() + GetActorRightVector()*(-HalfIPD));
  RightCapture->SetWorldLocation(GetActorLocation()+ GetActorRightVector()*(+HalfIPD));
  LeftCapture->SetWorldRotation(GetActorRotation());
  RightCapture->SetWorldRotation(GetActorRotation());

  const bool bNeedLeftEyeCapture = StereoEyeSource != EEqrStereoEyeSource::RightOnly;

  if (bNeedLeftEyeCapture)
  {
    LeftCapture->CaptureScene();
  }

  const bool bNeedRightEyeCapture =
    (StereoEyeSource == EEqrStereoEyeSource::RightOnly) ||
    (StereoMode != EEqrStereoMode::Mono && StereoEyeSource != EEqrStereoEyeSource::LeftOnly);

  if (bNeedRightEyeCapture)
  {
    RightCapture->CaptureScene();
  }

  FTextureRHIRef LeftTex = LeftCubeRT->GameThread_GetRenderTargetResource()->GetRenderTargetTexture();
  FTextureRHIRef RightTex = RightCubeRT->GameThread_GetRenderTargetResource()->GetRenderTargetTexture();

  if (StereoMode == EEqrStereoMode::Mono)
  {
    FTextureRHIRef Source = LeftTex;
    if (StereoEyeSource == EEqrStereoEyeSource::RightOnly && bNeedRightEyeCapture)
    {
      Source = RightTex;
    }
    ConvertEyeToRegion(Source, FIntRect(0, 0, Width, Height));
    return;
  }

  if (StereoEyeSource == EEqrStereoEyeSource::LeftOnly)
  {
    RightTex = LeftTex;
  }
  else if (StereoEyeSource == EEqrStereoEyeSource::RightOnly && bNeedRightEyeCapture)
  {
    LeftTex = RightTex;
  }

  if (StereoMode == EEqrStereoMode::StereoTB)
  {
    const int32 HalfH = Height / 2;
    ConvertEyeToRegion(LeftTex, FIntRect(0, 0, Width, HalfH));
    ConvertEyeToRegion(RightTex, FIntRect(0, HalfH, Width, Height));
  }
  else
  {
    const int32 HalfW = Width / 2;
    ConvertEyeToRegion(LeftTex, FIntRect(0, 0, HalfW, Height));
    ConvertEyeToRegion(RightTex, FIntRect(HalfW, 0, Width, Height));
  }
}

void AEqrCaptureActor::ConvertEyeToRegion(FTextureRHIRef CubeRHI, const FIntRect& Region){
  const FMatrix44f Rot=FMatrix44f::Identity;
  FTextureRHIRef Out=EquirectRT->GameThread_GetRenderTargetResource()->GetRenderTargetTexture();
  ENQUEUE_RENDER_COMMAND(Easy360_ConvertEye)([CubeRHI,Out,Region,Rot](FRHICommandListImmediate& RHICmdList){
    EqrRenderer::ConvertCubeToEquirect_RenderThread(RHICmdList, CubeRHI, Out, Region, Rot);
  });
}

void AEqrCaptureActor::NvencStartIfNeeded(){
#if EASY360_ENABLE_NVENC && PLATFORM_WINDOWS
  bNvencActive=false;

  if(!bUseNvenc){ NvencEncoder.Finalize(); return; }

  if (!GDynamicRHI)
  {
    UE_LOG(LogTemp, Warning, TEXT("[Easy360] NVENC requires a D3D11/D3D12 RHI."));
    return;
  }

  if (NvencBitstreamPath.IsEmpty())
  {
    const FString Stamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
    const FString NvExt = bNvencUseHEVC ? TEXT("nvenc.hevc") : TEXT("nvenc.h264");
    NvencBitstreamPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Movies"), TEXT("Easy360"), Stamp, NvExt);
  }

  FTextureRHIRef Out=EquirectRT->GameThread_GetRenderTargetResource()->GetRenderTargetTexture();
  void* NativeRes = Out->GetNativeResource();
  if(!NativeRes)
  {
    UE_LOG(LogTemp,Warning,TEXT("[Easy360] NVENC: No native texture resource."));
    return;
  }

  FEasyNvencInit Init;
  Init.Width=Width;
  Init.Height=Height;
  Init.BitrateMbps=NvencBitrateMbps;
  Init.FrameRate=TargetFPS;
  Init.bH265=bNvencUseHEVC;
  Init.OutputPath=NvencBitstreamPath;

  const FString RHIName = GDynamicRHI->GetName();
  if (RHIName.Equals(TEXT("D3D11"), ESearchCase::IgnoreCase))
  {
    ID3D11Texture2D* Tex = static_cast<ID3D11Texture2D*>(NativeRes);
    TRefCountPtr<ID3D11Device> Device;
    Tex->GetDevice(Device.GetInitReference());
    if (!Device.IsValid())
    {
      UE_LOG(LogTemp,Warning,TEXT("[Easy360] NVENC: Unable to acquire D3D11 device."));
      return;
    }

    Init.DeviceType = EEasyNvencDeviceType::D3D11;
    Init.Device = Device.GetReference();
    Init.InputResource = Tex;
    bNvencActive = NvencEncoder.Initialize(Init);
  }
  else if (RHIName.Equals(TEXT("D3D12"), ESearchCase::IgnoreCase))
  {
    ID3D12Resource* Resource = static_cast<ID3D12Resource*>(NativeRes);
    TRefCountPtr<ID3D12Device> Device;
    if (FAILED(Resource->GetDevice(__uuidof(ID3D12Device), reinterpret_cast<void**>(Device.GetInitReference()))) || !Device.IsValid())
    {
      UE_LOG(LogTemp,Warning,TEXT("[Easy360] NVENC: Unable to acquire D3D12 device."));
      return;
    }

    Init.DeviceType = EEasyNvencDeviceType::D3D12;
    Init.Device = Device.GetReference();
    Init.InputResource = Resource;
    bNvencActive = NvencEncoder.Initialize(Init);
  }
  else
  {
    UE_LOG(LogTemp, Warning, TEXT("[Easy360] NVENC requires D3D11/D3D12. Current RHI: %s"), *RHIName);
    return;
  }

  if (bNvencActive)
  {
    NvencBitstreamPath = NvencEncoder.GetOutBitstreamPath();
    UE_LOG(LogTemp, Log, TEXT("[Easy360] NVENC writing bitstream to %s"), *NvencBitstreamPath);
  }
  else
  {
    UE_LOG(LogTemp, Warning, TEXT("[Easy360] NVENC initialization failed."));
  }
#else
  bNvencActive=false;
#endif
}

void AEqrCaptureActor::NvencSubmitFrame(){
#if EASY360_ENABLE_NVENC && PLATFORM_WINDOWS
  if(!bNvencActive) return;
  FTextureRHIRef Out=EquirectRT->GameThread_GetRenderTargetResource()->GetRenderTargetTexture();
  void* NativeRes = Out->GetNativeResource();
  if(!NativeRes)
  {
    UE_LOG(LogTemp, Warning, TEXT("[Easy360] NVENC: Missing native texture for encode."));
    bNvencActive=false;
    return;
  }
  if(!NvencEncoder.EncodeFrame(NativeRes))
  {
    UE_LOG(LogTemp, Warning, TEXT("[Easy360] NVENC encode failed; stopping."));
    bNvencActive=false;
  }
#endif
}

void AEqrCaptureActor::NvencStop(){
#if EASY360_ENABLE_NVENC && PLATFORM_WINDOWS
  if(!bUseNvenc) return;
  NvencEncoder.Finalize();
  bNvencActive=false;
  NvencBitstreamPath = NvencEncoder.GetOutBitstreamPath();

  if(NvencBitstreamPath.IsEmpty() || !IFileManager::Get().FileExists(*NvencBitstreamPath))
  {
    UE_LOG(LogTemp, Warning, TEXT("[Easy360] NVENC bitstream missing; skip mux."));
    return;
  }

  const FString FfmpegExe = ResolveFFmpeg();
  if(FfmpegExe.IsEmpty())
  {
    UE_LOG(LogTemp,Warning,TEXT("[Easy360] ffmpeg not found for NVENC mux."));
    return;
  }
  const bool bHasAudio = IFileManager::Get().FileExists(*AudioWavPath);

  FString OutMP4 = MoviePath.IsEmpty()? FPaths::ChangeExtension(NvencBitstreamPath, TEXT("mp4")) : MoviePath;
  const FString In264 = FString::Printf(TEXT("\"%s\""), *NvencBitstreamPath);
  const FString OutQ  = FString::Printf(TEXT("\"%s\""), *OutMP4);
  const FString TagArg = bNvencUseHEVC ? TEXT(" -tag:v hvc1") : TEXT("");
  FString Args;
  if(bHasAudio){
    const FString WavQ = FString::Printf(TEXT("\"%s\""), *AudioWavPath);
    Args = FString::Printf(TEXT("-y -i %s -i %s -c:v copy -c:a aac -b:a 192k%s %s"), *In264, *WavQ, *TagArg, *OutQ);
  }else{
    Args = FString::Printf(TEXT("-y -i %s -c:v copy%s %s"), *In264, *TagArg, *OutQ);
  }
  int32 RC=-1;
  FProcHandle P=FPlatformProcess::CreateProc(*FfmpegExe,*Args,true,false,false,nullptr,0,nullptr,nullptr);
  if(!P.IsValid())
  {
    UE_LOG(LogTemp,Warning,TEXT("[Easy360] Failed to launch ffmpeg for NVENC mux: %s"), *FfmpegExe);
    return;
  }

  FPlatformProcess::WaitForProc(P);
  FPlatformProcess::GetProcReturnCode(P,RC);
  FPlatformProcess::CloseProc(P);
  if(RC!=0)
  {
    UE_LOG(LogTemp,Warning,TEXT("[Easy360] NVENC mux failed rc=%d"),RC);
  }
  else
  {
    UE_LOG(LogTemp,Log,TEXT("[Easy360] NVENC mux done: %s"),*OutMP4);
  }
#endif
}

void AEqrCaptureActor::RunFFmpegMux() const{
  const FString FfmpegExe = ResolveFFmpeg();
  if(FfmpegExe.IsEmpty())
  {
    UE_LOG(LogTemp,Warning,TEXT("[Easy360] ffmpeg not found."));
    return;
  }

  const bool bHasAudio=IFileManager::Get().FileExists(*AudioWavPath);
  const FString InPng = FString::Printf(TEXT("\"%s\""), *PngPattern);
  const FString OutQ  = FString::Printf(TEXT("\"%s\""), *MoviePath);

  FString Args = FString::Printf(TEXT("-y -r %.3f -i %s "), TargetFPS, *InPng);
  if(bHasAudio)
  {
    const FString WavQ = FString::Printf(TEXT("\"%s\""), *AudioWavPath);
    Args += FString::Printf(TEXT("-i %s "), *WavQ);
  }

  const FString VideoEncoder = FFmpegEncoder.IsEmpty() ? TEXT("libx264") : FFmpegEncoder;
  FString VideoArgs;
  if(VideoEncoder.Equals(TEXT("libx264"), ESearchCase::IgnoreCase))
  {
    VideoArgs = FString::Printf(TEXT("-c:v libx264 -crf %d -pix_fmt yuv420p "), FMath::Clamp(FFmpegCRF, 0, 35));
  }
  else if(VideoEncoder.Equals(TEXT("h264_nvenc"), ESearchCase::IgnoreCase))
  {
    VideoArgs = FString::Printf(TEXT("-c:v h264_nvenc -preset %s -b:v %dM -pix_fmt yuv420p "), *FFmpegNvPreset, FMath::Max(1, NvencBitrateMbps));
  }
  else if(VideoEncoder.Equals(TEXT("hevc_nvenc"), ESearchCase::IgnoreCase))
  {
    VideoArgs = FString::Printf(TEXT("-c:v hevc_nvenc -preset %s -b:v %dM -pix_fmt yuv420p -tag:v hvc1 "), *FFmpegNvPreset, FMath::Max(1, NvencBitrateMbps));
  }
  else
  {
    VideoArgs = FString::Printf(TEXT("-c:v %s "), *VideoEncoder);
  }
  Args += VideoArgs;

  if(bHasAudio)
  {
    Args += TEXT("-c:a aac -b:a 192k ");
  }
  Args += OutQ;

  int32 RC=-1;
  FProcHandle P=FPlatformProcess::CreateProc(*FfmpegExe,*Args,true,false,false,nullptr,0,nullptr,nullptr);
  if(!P.IsValid())
  {
    UE_LOG(LogTemp,Warning,TEXT("[Easy360] Failed to launch ffmpeg: %s"), *FfmpegExe);
    return;
  }

  FPlatformProcess::WaitForProc(P);
  FPlatformProcess::GetProcReturnCode(P,RC);
  FPlatformProcess::CloseProc(P);

  if(RC!=0)
  {
    UE_LOG(LogTemp,Warning,TEXT("[Easy360] ffmpeg rc=%d"),RC);
  }
  else
  {
    UE_LOG(LogTemp,Log,TEXT("[Easy360] mux done: %s"),*MoviePath);
  }
}
