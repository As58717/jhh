#include "EqrRenderer.h"
#include "EqrEquirectCS.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderGraphResources.h"
#include "RHIStaticStates.h"

namespace EqrRenderer{
void ConvertCubeToEquirect_RenderThread(FRHICommandListImmediate& RHICmdList,
    FTextureRHIRef InCubeRHI,FTextureRHIRef Out2DRHI,const FIntRect& ViewRect,const FMatrix44f& Rot)
{
  check(IsInRenderingThread()); check(IsValidRef(InCubeRHI)&&IsValidRef(Out2DRHI));
  FRDGBuilder Graph(RHICmdList);
  FRDGTextureRef InTex=Graph.RegisterExternalTexture(CreateRenderTarget(InCubeRHI,TEXT("EqrInCube")));
  FRDGTextureRef OutTex=Graph.RegisterExternalTexture(CreateRenderTarget(Out2DRHI,TEXT("EqrOutRT")));
  FEqrCubeToEqrCS::FParameters* P=Graph.AllocParameters<FEqrCubeToEqrCS::FParameters>();
  P->OutSize=FVector2f((float)Out2DRHI->GetSizeXYZ().X,(float)Out2DRHI->GetSizeXYZ().Y);
  P->ViewportMin=FVector2f(ViewRect.Min.X,ViewRect.Min.Y);
  P->ViewportMax=FVector2f(ViewRect.Max.X,ViewRect.Max.Y);
  P->RotMat=Rot;
  P->LinearClamp=TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
  P->InCube=Graph.CreateSRV(FRDGTextureSRVDesc::Create(InTex));
  P->OutEqr=Graph.CreateUAV(FRDGTextureUAVDesc(OutTex));
  TShaderMapRef<FEqrCubeToEqrCS> CS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
  const FIntVector Groups(FMath::DivideAndRoundUp(ViewRect.Width(),8),
                          FMath::DivideAndRoundUp(ViewRect.Height(),8),1);
  FComputeShaderUtils::AddPass(Graph,RDG_EVENT_NAME("Eqr CubeToEquirect"),*CS,P,Groups);
  Graph.Execute();
}}
