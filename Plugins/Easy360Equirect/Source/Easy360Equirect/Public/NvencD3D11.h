#pragma once

#include "CoreMinimal.h"

#if EASY360_ENABLE_NVENC && PLATFORM_WINDOWS

#include "Templates/RefCounting.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include <d3d11.h>
#include <d3d12.h>
#include "nvEncodeAPI.h"
#include "Windows/HideWindowsPlatformTypes.h"

#ifndef NV_ENC_DEVICE_TYPE_DIRECTX11
#define NV_ENC_DEVICE_TYPE_DIRECTX11 0x3
#endif
#ifndef NV_ENC_DEVICE_TYPE_DIRECTX12
#define NV_ENC_DEVICE_TYPE_DIRECTX12 0x4
#endif
#ifndef NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX12
#define NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX12 0x4
#endif

enum class EEasyNvencDeviceType : uint8
{
    None = 0,
    D3D11,
    D3D12
};

struct FEasyNvencInit
{
    void* Device = nullptr;
    void* InputResource = nullptr;
    int32 Width = 0;
    int32 Height = 0;
    int32 BitrateMbps = 40;
    float FrameRate = 30.f;
    bool bH265 = false;
    EEasyNvencDeviceType DeviceType = EEasyNvencDeviceType::None;
    FString OutputPath;
};

class FEasyNvencD3D11
{
public:
    FEasyNvencD3D11();
    ~FEasyNvencD3D11();

    bool Initialize(const FEasyNvencInit& Init);
    bool EncodeFrame(void* NativeResource);
    bool Finalize();

    FString GetOutBitstreamPath() const { return OutPath; }

private:
    bool LoadApi();
    void ReleaseApi();
    bool CreateEncoder();
    void DestroyEncoder();
    bool EnsureRegistration(void* NativeResource);
    bool DrainBitstream(bool bBlocking);
    bool SubmitEOS();
    void CloseBitstreamFile();

    FEasyNvencInit CachedInit;
    FString OutPath;
    NV_ENCODE_API_FUNCTION_LIST ApiFunctions;
    void* Encoder = nullptr;
    void* ApiHandle = nullptr;
    NV_ENC_REGISTERED_PTR RegisteredResource = nullptr;
    void* CurrentResource = nullptr;
    NV_ENC_OUTPUT_PTR BitstreamBuffer = nullptr;
    NV_ENC_BUFFER_FORMAT BufferFormat = NV_ENC_BUFFER_FORMAT_ABGR;
    uint64 FrameCount = 0;

    TRefCountPtr<ID3D11Device> Device11;
    TRefCountPtr<ID3D12Device> Device12;

    TUniquePtr<IFileHandle> BitstreamFile;
};

#else

struct FEasyNvencInit
{
    void* Device = nullptr;
    void* InputResource = nullptr;
    int32 Width = 0;
    int32 Height = 0;
    int32 BitrateMbps = 40;
    float FrameRate = 30.f;
    bool bH265 = false;
    FString OutputPath;
};

class FEasyNvencD3D11
{
public:
    bool Initialize(const FEasyNvencInit&)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Easy360] NVENC disabled (EASY360_ENABLE_NVENC=0)."));
        return false;
    }

    bool EncodeFrame(void*) { return false; }
    bool Finalize() { return false; }
    FString GetOutBitstreamPath() const { return FString(); }
};

#endif

