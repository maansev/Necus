#pragma once

namespace NoFlash {
    // Call once at startup to find m_flFlashMaxAlpha offset via pattern scan
    void Init();
    // Call every frame (from hkPresent) and on FSN stage 5
    void Update();
}
