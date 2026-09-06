#pragma once

#include "Components/SceneCaptureComponent2D.h"

namespace VirtualSensorCaptureRendering
{
/**
 * Keep a ViewState for scheduled (not bCaptureEveryFrame) sensor captures.
 * UE 5.3 SceneVisibility.cpp releases the relevance pipe before triggering the
 * occlusion event in the no-occlusion-context path. A capture without ViewState
 * enters that path; its packet can be destroyed before the event is complete.
 * Persistent state avoids that no-ViewState path under normal occlusion settings
 * and is reused per component. Do not change global renderer CVars here.
 */
inline void Prepare(USceneCaptureComponent2D& Capture, bool bDiscardHistory = false)
{
	Capture.bAlwaysPersistRenderingState = true;
	if (bDiscardHistory) Capture.bCameraCutThisFrame = true;
}
}
