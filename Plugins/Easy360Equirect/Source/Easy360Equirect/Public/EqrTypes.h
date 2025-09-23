#pragma once
#include "CoreMinimal.h"
#include "EqrTypes.generated.h"

UENUM(BlueprintType)
enum class EEqrStereoMode : uint8
{
    Mono UMETA(DisplayName="Mono"),
    StereoSBS UMETA(DisplayName="Stereo Side-by-Side"),
    StereoTB UMETA(DisplayName="Stereo Top-Bottom")
};

UENUM(BlueprintType)
enum class EEqrStereoEyeSource : uint8
{
    Both UMETA(DisplayName="Both Eyes"),
    LeftOnly UMETA(DisplayName="Left Eye Only"),
    RightOnly UMETA(DisplayName="Right Eye Only")
};
