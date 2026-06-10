#pragma once

namespace Viewmodel {
    // Call every frame (Present or FSN render-start). Writes the user's
    // X/Y/Z offset and FOV into the local pawn; restores game defaults
    // once when the feature is turned off.
    void Update();
}
