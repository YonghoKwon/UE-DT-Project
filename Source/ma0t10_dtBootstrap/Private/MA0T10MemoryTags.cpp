#include "MA0T10MemoryTags.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

bool MA0T10MemoryTags::IsEnabled()
{
    static const bool Enabled=!FParse::Param(FCommandLine::Get(),TEXT("NoProjectMemoryTags"));return Enabled;
}

LLM_DEFINE_TAG(MA0T10_LidarRtScene);
LLM_DEFINE_TAG(MA0T10_LidarReadback);
LLM_DEFINE_TAG(MA0T10_LidarPoints);
LLM_DEFINE_TAG(MA0T10_LidarPreview);
LLM_DEFINE_TAG(MA0T10_PcdEncode);
LLM_DEFINE_TAG(MA0T10_HatchAnalysis);
LLM_DEFINE_TAG(MA0T10_CameraGT);
