#pragma once
#include "RHICommandList.h"
#include "RHIGPUReadback.h"

namespace VirtualSensorTextureReadback
{
/** SceneCapture leaves its target in SRVMask. Readback requires CopySrc and must restore it. */
inline void Enqueue(FRHICommandListImmediate& Commands,FRHIGPUTextureReadback& Readback,FRHITexture* Texture)
{
    Commands.Transition(FRHITransitionInfo(Texture,ERHIAccess::SRVMask,ERHIAccess::CopySrc));
    Readback.EnqueueCopy(Commands,Texture);
    Commands.Transition(FRHITransitionInfo(Texture,ERHIAccess::CopySrc,ERHIAccess::SRVMask));
}
}
