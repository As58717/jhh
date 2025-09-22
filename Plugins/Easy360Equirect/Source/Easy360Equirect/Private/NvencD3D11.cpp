#include "NvencD3D11.h"

#if EASY360_ENABLE_NVENC && PLATFORM_WINDOWS

#include "HAL/FileManager.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"

namespace
{
static NVENCSTATUS (NVENCAPI* ResolveCreateInstance(void* Handle))(NV_ENCODE_API_FUNCTION_LIST*)
{
    return reinterpret_cast<NVENCSTATUS(NVENCAPI*)(NV_ENCODE_API_FUNCTION_LIST*)>(
        FPlatformProcess::GetDllExport(Handle, TEXT("NvEncodeAPICreateInstance")));
}
}

FEasyNvencD3D11::FEasyNvencD3D11()
{
    FMemory::Memzero(ApiFunctions);
}

FEasyNvencD3D11::~FEasyNvencD3D11()
{
    Finalize();
}

bool FEasyNvencD3D11::LoadApi()
{
    if (ApiHandle)
    {
        return true;
    }

    ApiHandle = FPlatformProcess::GetDllHandle(TEXT("nvEncodeAPI64.dll"));
    if (!ApiHandle)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Easy360] Failed to load nvEncodeAPI64.dll"));
        return false;
    }

    auto CreateInstance = ResolveCreateInstance(ApiHandle);
    if (!CreateInstance)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Easy360] Could not resolve NvEncodeAPICreateInstance"));
        ReleaseApi();
        return false;
    }

    FMemory::Memzero(ApiFunctions);
    ApiFunctions.version = NV_ENCODE_API_FUNCTION_LIST_VER;

    const NVENCSTATUS Status = CreateInstance(&ApiFunctions);
    if (Status != NV_ENC_SUCCESS)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Easy360] NvEncodeAPICreateInstance failed: 0x%08x"), Status);
        ReleaseApi();
        return false;
    }

    return true;
}

void FEasyNvencD3D11::ReleaseApi()
{
    if (ApiHandle)
    {
        FPlatformProcess::FreeDllHandle(ApiHandle);
        ApiHandle = nullptr;
    }

    FMemory::Memzero(ApiFunctions);
}

bool FEasyNvencD3D11::Initialize(const FEasyNvencInit& Init)
{
    Finalize();

    CachedInit = Init;
    OutPath = Init.OutputPath;

    if (OutPath.IsEmpty())
    {
        const FString Stamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
        const FString FileName = Init.bH265 ? TEXT("nvenc.hevc") : TEXT("nvenc.h264");
        OutPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Movies"), TEXT("Easy360"), Stamp, FileName);
    }

    IFileManager& FileManager = FPlatformFileManager::Get().GetPlatformFile();
    FileManager.MakeDirectory(*FPaths::GetPath(OutPath), true);
    BitstreamFile.Reset(FileManager.OpenWrite(*OutPath));
    if (!BitstreamFile.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Easy360] NVENC could not open output: %s"), *OutPath);
        return false;
    }

    if (!LoadApi())
    {
        return false;
    }

    if (!Init.Device || !Init.InputResource || Init.Width <= 0 || Init.Height <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Easy360] NVENC invalid initialization parameters"));
        return false;
    }

    Device11.SafeRelease();
    Device12.SafeRelease();

    if (Init.DeviceType == EEasyNvencDeviceType::D3D11)
    {
        Device11 = static_cast<ID3D11Device*>(Init.Device);
    }
    else if (Init.DeviceType == EEasyNvencDeviceType::D3D12)
    {
        Device12 = static_cast<ID3D12Device*>(Init.Device);
    }

    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS OpenParams = {};
    OpenParams.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
    OpenParams.apiVersion = NVENCAPI_VERSION;
    OpenParams.device = Init.Device;
#if defined(NV_ENC_DEVICE_TYPE_DIRECTX11) && defined(NV_ENC_DEVICE_TYPE_DIRECTX12)
    OpenParams.deviceType = (Init.DeviceType == EEasyNvencDeviceType::D3D12)
        ? NV_ENC_DEVICE_TYPE_DIRECTX12
        : NV_ENC_DEVICE_TYPE_DIRECTX11;
#else
    OpenParams.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
#endif

    const NVENCSTATUS OpenStatus = ApiFunctions.nvEncOpenEncodeSessionEx(&OpenParams, &Encoder);
    if (OpenStatus != NV_ENC_SUCCESS)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Easy360] nvEncOpenEncodeSessionEx failed: 0x%08x"), OpenStatus);
        DestroyEncoder();
        return false;
    }

    GUID CodecGUID = Init.bH265 ? NV_ENC_CODEC_HEVC_GUID : NV_ENC_CODEC_H264_GUID;
    GUID PresetGUID = NV_ENC_PRESET_P3_GUID;
    GUID ProfileGUID = Init.bH265 ? NV_ENC_HEVC_PROFILE_MAIN_GUID : NV_ENC_H264_PROFILE_HIGH_GUID;

    NV_ENC_PRESET_CONFIG PresetConfig = {};
    PresetConfig.version = NV_ENC_PRESET_CONFIG_VER;
    PresetConfig.presetCfg.version = NV_ENC_CONFIG_VER;

    NVENCSTATUS PresetStatus = ApiFunctions.nvEncGetEncodePresetConfigEx(
        Encoder, CodecGUID, PresetGUID, NV_ENC_TUNING_INFO_HIGH_QUALITY, &PresetConfig);
    if (PresetStatus != NV_ENC_SUCCESS)
    {
        PresetStatus = ApiFunctions.nvEncGetEncodePresetConfig(Encoder, CodecGUID, PresetGUID, &PresetConfig);
        if (PresetStatus != NV_ENC_SUCCESS)
        {
            UE_LOG(LogTemp, Warning, TEXT("[Easy360] Failed to fetch NVENC preset configuration."));
            DestroyEncoder();
            return false;
        }
    }

    NV_ENC_CONFIG EncodeConfig = PresetConfig.presetCfg;
    EncodeConfig.profileGUID = ProfileGUID;
    const uint32 Bitrate = static_cast<uint32>(FMath::Max(1, Init.BitrateMbps) * 1000000);
    const uint32 FrameRateInt = FMath::Max(1, static_cast<int32>(FMath::RoundToInt(Init.FrameRate)));
    const uint32 VbvSize = FrameRateInt > 0 ? Bitrate / FrameRateInt : Bitrate;
    EncodeConfig.rcParams.averageBitRate = Bitrate;
    EncodeConfig.rcParams.maxBitRate = Bitrate;
    EncodeConfig.rcParams.vbvBufferSize = VbvSize;
    EncodeConfig.rcParams.vbvInitialDelay = VbvSize;
    EncodeConfig.gopLength = NVENC_INFINITE_GOPLENGTH;
    EncodeConfig.frameIntervalP = 1;

    NV_ENC_INITIALIZE_PARAMS InitParams = {};
    InitParams.version = NV_ENC_INITIALIZE_PARAMS_VER;
    InitParams.encodeGUID = CodecGUID;
    InitParams.presetGUID = PresetGUID;
    InitParams.encodeWidth = Init.Width;
    InitParams.encodeHeight = Init.Height;
    InitParams.darWidth = Init.Width;
    InitParams.darHeight = Init.Height;
    InitParams.frameRateNum = FMath::Max(1, (int32)FMath::RoundToInt(Init.FrameRate * 1000.f));
    InitParams.frameRateDen = 1000;
    InitParams.enablePTD = 1;
    InitParams.enableOutputInVidmem = 0;
    InitParams.enableInputInVidmem = 1;
    InitParams.maxEncodeWidth = Init.Width;
    InitParams.maxEncodeHeight = Init.Height;
    InitParams.encodeConfig = &EncodeConfig;

    const NVENCSTATUS InitStatus = ApiFunctions.nvEncInitializeEncoder(Encoder, &InitParams);
    if (InitStatus != NV_ENC_SUCCESS)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Easy360] nvEncInitializeEncoder failed: 0x%08x"), InitStatus);
        DestroyEncoder();
        return false;
    }

    NV_ENC_CREATE_BITSTREAM_BUFFER CreateBS = {};
    CreateBS.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
    CreateBS.memoryHeap = NV_ENC_MEMORY_HEAP_SYSMEM_CACHED;

    const NVENCSTATUS BitstreamStatus = ApiFunctions.nvEncCreateBitstreamBuffer(Encoder, &CreateBS);
    if (BitstreamStatus != NV_ENC_SUCCESS)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Easy360] nvEncCreateBitstreamBuffer failed: 0x%08x"), BitstreamStatus);
        DestroyEncoder();
        return false;
    }

    BitstreamBuffer = CreateBS.bitstreamBuffer;
    BufferFormat = NV_ENC_BUFFER_FORMAT_ARGB;
    FrameCount = 0;

    if (!EnsureRegistration(Init.InputResource))
    {
        DestroyEncoder();
        return false;
    }

    return true;
}

bool FEasyNvencD3D11::EnsureRegistration(void* NativeResource)
{
    if (!Encoder)
    {
        return false;
    }

    if (CurrentResource == NativeResource && RegisteredResource)
    {
        return true;
    }

    if (RegisteredResource)
    {
        ApiFunctions.nvEncUnregisterResource(Encoder, RegisteredResource);
        RegisteredResource = nullptr;
    }

    CurrentResource = NativeResource;

    if (!NativeResource)
    {
        return false;
    }

    NV_ENC_REGISTER_RESOURCE Register = {};
    Register.version = NV_ENC_REGISTER_RESOURCE_VER;
#if defined(NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX12)
    Register.resourceType = (CachedInit.DeviceType == EEasyNvencDeviceType::D3D12)
        ? NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX12
        : NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
#else
    Register.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
#endif
    Register.width = CachedInit.Width;
    Register.height = CachedInit.Height;
    Register.pitch = 0;
    Register.subResourceIndex = 0;
    Register.bufferFormat = BufferFormat;
    Register.bufferUsage = NV_ENC_INPUT_IMAGE;
    Register.resourceToRegister = NativeResource;

    const NVENCSTATUS RegisterStatus = ApiFunctions.nvEncRegisterResource(Encoder, &Register);
    if (RegisterStatus != NV_ENC_SUCCESS)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Easy360] nvEncRegisterResource failed: 0x%08x"), RegisterStatus);
        CurrentResource = nullptr;
        return false;
    }

    RegisteredResource = Register.registeredResource;
    return true;
}

bool FEasyNvencD3D11::DrainBitstream(bool bBlocking)
{
    if (!BitstreamBuffer)
    {
        return false;
    }

    NV_ENC_LOCK_BITSTREAM Lock = {};
    Lock.version = NV_ENC_LOCK_BITSTREAM_VER;
    Lock.outputBitstream = BitstreamBuffer;
    Lock.doNotWait = bBlocking ? 0 : 1;

    NVENCSTATUS LockStatus = ApiFunctions.nvEncLockBitstream(Encoder, &Lock);
    if (LockStatus == NV_ENC_ERR_NEED_MORE_INPUT)
    {
        return true;
    }

    if (LockStatus != NV_ENC_SUCCESS)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Easy360] nvEncLockBitstream failed: 0x%08x"), LockStatus);
        return false;
    }

    if (Lock.bitstreamSizeInBytes > 0 && BitstreamFile.IsValid())
    {
        BitstreamFile->Serialize(Lock.bitstreamBufferPtr, Lock.bitstreamSizeInBytes);
    }

    ApiFunctions.nvEncUnlockBitstream(Encoder, BitstreamBuffer);
    return true;
}

bool FEasyNvencD3D11::EncodeFrame(void* NativeResource)
{
    if (!Encoder || !BitstreamFile.IsValid())
    {
        return false;
    }

    if (!EnsureRegistration(NativeResource))
    {
        return false;
    }

    NV_ENC_MAP_INPUT_RESOURCE Map = {};
    Map.version = NV_ENC_MAP_INPUT_RESOURCE_VER;
    Map.registeredResource = RegisteredResource;

    const NVENCSTATUS MapStatus = ApiFunctions.nvEncMapInputResource(Encoder, &Map);
    if (MapStatus != NV_ENC_SUCCESS)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Easy360] nvEncMapInputResource failed: 0x%08x"), MapStatus);
        return false;
    }

    NV_ENC_INPUT_PTR InputPtr = Map.mappedResource;

    NV_ENC_PIC_PARAMS Pic = {};
    Pic.version = NV_ENC_PIC_PARAMS_VER;
    Pic.inputBuffer = InputPtr;
    Pic.bufferFmt = BufferFormat;
    Pic.inputWidth = CachedInit.Width;
    Pic.inputHeight = CachedInit.Height;
    Pic.outputBitstream = BitstreamBuffer;
    Pic.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
    Pic.inputTimeStamp = FrameCount++;

    const NVENCSTATUS EncodeStatus = ApiFunctions.nvEncEncodePicture(Encoder, &Pic);

    ApiFunctions.nvEncUnmapInputResource(Encoder, InputPtr);

    if (EncodeStatus != NV_ENC_SUCCESS && EncodeStatus != NV_ENC_ERR_NEED_MORE_INPUT)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Easy360] nvEncEncodePicture failed: 0x%08x"), EncodeStatus);
        return false;
    }

    return DrainBitstream(true);
}

bool FEasyNvencD3D11::SubmitEOS()
{
    if (!Encoder)
    {
        return false;
    }

    NV_ENC_PIC_PARAMS Pic = {};
    Pic.version = NV_ENC_PIC_PARAMS_VER;
    Pic.encodePicFlags = NV_ENC_PIC_FLAG_EOS;
    Pic.outputBitstream = BitstreamBuffer;

    const NVENCSTATUS Status = ApiFunctions.nvEncEncodePicture(Encoder, &Pic);
    return Status == NV_ENC_SUCCESS || Status == NV_ENC_ERR_NEED_MORE_INPUT;
}

void FEasyNvencD3D11::CloseBitstreamFile()
{
    if (BitstreamFile.IsValid())
    {
        BitstreamFile->Flush();
    }
    BitstreamFile.Reset();
}

void FEasyNvencD3D11::DestroyEncoder()
{
    if (Encoder)
    {
        if (RegisteredResource)
        {
            ApiFunctions.nvEncUnregisterResource(Encoder, RegisteredResource);
            RegisteredResource = nullptr;
        }

        if (BitstreamBuffer)
        {
            ApiFunctions.nvEncDestroyBitstreamBuffer(Encoder, BitstreamBuffer);
            BitstreamBuffer = nullptr;
        }

        ApiFunctions.nvEncDestroyEncoder(Encoder);
        Encoder = nullptr;
    }

    Device11.SafeRelease();
    Device12.SafeRelease();
    CurrentResource = nullptr;
    FrameCount = 0;
}

bool FEasyNvencD3D11::Finalize()
{
    if (Encoder)
    {
        SubmitEOS();
        DrainBitstream(true);
    }

    DestroyEncoder();
    CloseBitstreamFile();
    ReleaseApi();

    return true;
}

#endif // EASY360_ENABLE_NVENC && PLATFORM_WINDOWS

