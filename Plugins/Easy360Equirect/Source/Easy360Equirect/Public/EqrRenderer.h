#pragma once
#include "CoreMinimal.h"
#include "RHI.h"
#include "RHIResources.h"
#include "EqrTypes.h"
namespace EqrRenderer{
  void ConvertCubeToEquirect_RenderThread(FRHICommandListImmediate& RHICmdList,
    FTextureRHIRef InCubeRHI,FTextureRHIRef Out2DRHI,const FIntRect& ViewRect,const FMatrix44f& RotMat,bool bApplySrgb);
}
