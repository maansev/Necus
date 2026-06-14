#pragma once

namespace Viewmodel {
    // Call from FrameStageNotify: stage 0 (FRAME_START — engine samples the
    // pawn fields during prediction) and stage 5 (FRAME_RENDER_START).
    // Writes the user's X/Y/Z offset and FOV into the local pawn; restores
    // game defaults once when the feature is turned off.
    // Freeze-prevention: identical values are never re-written (eps check),
    // except for a short force window right after enabling so the engine
    // definitely picks the values up when (re)creating the viewmodel.
    void Update(int stage = 5);
}
