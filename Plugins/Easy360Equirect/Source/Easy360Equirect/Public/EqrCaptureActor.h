#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EqrTypes.h"
#include "NvencD3D11.h"

class USoundSubmixBase;
#include "EqrCaptureActor.generated.h"

class UTextureRenderTarget2D; class UTextureRenderTargetCube; class USceneCaptureComponentCube;

UCLASS(BlueprintType, Blueprintable)
class EASY360EQUIRECT_API AEqrCaptureActor : public AActor
{
    GENERATED_BODY()
public:
    AEqrCaptureActor();

    // Output
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Easy360|Output") int32 Width=4096;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Easy360|Output") int32 Height=2048;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Easy360|Output") float TargetFPS=30.0f;

    // Stereo
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Easy360|Stereo") EEqrStereoMode StereoMode=EEqrStereoMode::Mono;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Easy360|Stereo") EEqrStereoEyeSource StereoEyeSource=EEqrStereoEyeSource::Both;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Easy360|Stereo", meta=(ClampMin="0.0")) float IPDcm=6.4f;

    // Recording (PNG/ffmpeg)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Easy360|Record") bool bRecordSequence=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Easy360|Record") FString FFmpegExePath;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Easy360|Record") int32 FFmpegCRF=18;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Easy360|Record") FString OutputDir;

    // Audio
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Easy360|Audio") bool bRecordAudio=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Easy360|Audio") TObjectPtr<USoundSubmixBase> SubmixToRecord=nullptr;

    // NVENC (optional)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Easy360|NVENC") bool bUseNvenc=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Easy360|NVENC") bool bNvencUseHEVC=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Easy360|NVENC") int32 NvencBitrateMbps=40;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Easy360|Runtime") bool bNvencActive=false;

    // FFmpeg pipe（示例开关，仅 UI；未实现回读喂管）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Easy360|FFmpeg") bool bPipeEncode=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Easy360|FFmpeg") FString FFmpegEncoder=TEXT("libx264");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Easy360|FFmpeg") FString FFmpegNvPreset=TEXT("p5");

    // Runtime objects
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Easy360|Runtime") FString CurrentSessionDir;
    UPROPERTY(VisibleAnywhere, Category="Easy360|Runtime") TObjectPtr<USceneCaptureComponentCube> LeftCapture=nullptr;
    UPROPERTY(VisibleAnywhere, Category="Easy360|Runtime") TObjectPtr<USceneCaptureComponentCube> RightCapture=nullptr;
    UPROPERTY(VisibleAnywhere, Category="Easy360|Runtime") TObjectPtr<UTextureRenderTargetCube> LeftCubeRT=nullptr;
    UPROPERTY(VisibleAnywhere, Category="Easy360|Runtime") TObjectPtr<UTextureRenderTargetCube> RightCubeRT=nullptr;
    UPROPERTY(VisibleAnywhere, Category="Easy360|Runtime") TObjectPtr<UTextureRenderTarget2D> EquirectRT=nullptr;

    UFUNCTION(BlueprintCallable, Category="Easy360") void StartCapture();
    UFUNCTION(BlueprintCallable, Category="Easy360") void StopCapture();

protected:
    virtual void BeginPlay() override; virtual void Tick(float DeltaSeconds) override;

private:
    bool bCapturing=false; double Accum=0.0; double FrameInterval=1.0/30.0;
    int64 FrameIndex=0; FString PngPattern; FString MoviePath; FString AudioWavPath; FString NvencBitstreamPath;

    void CaptureOnceAndConvert();
    void ConvertEyeToRegion(FTextureRHIRef CubeRHI, const FIntRect& Region);
    void BuildSessionPaths(); FString ResolveFFmpeg() const; void RunFFmpegMux() const;
    void StartAudioRecord(); void StopAudioRecord(); bool WaitForAudioReady() const;
    void EnsureTargets();

    // NVENC
    void NvencStartIfNeeded();
    void NvencSubmitFrame();
    void NvencStop();

    FEasyNvencD3D11 NvencEncoder;
};
