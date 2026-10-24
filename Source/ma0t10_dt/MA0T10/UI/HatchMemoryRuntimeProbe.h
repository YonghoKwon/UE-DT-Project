#pragma once

// Explicit packaged-build diagnostic only. No object/tick is created without
// -HatchMemoryRuntimeReport=<fresh directory>.
void StartHatchMemoryRuntimeProbe();
void StopHatchMemoryRuntimeProbe();
