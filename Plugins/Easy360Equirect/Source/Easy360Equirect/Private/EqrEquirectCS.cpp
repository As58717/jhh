#include "EqrEquirectCS.h"
#include "RenderGraphUtils.h"
IMPLEMENT_GLOBAL_SHADER(FEqrCubeToEqrCS, "/Easy360Equirect/EqrEquirectCS.usf", "Main", SF_Compute);
