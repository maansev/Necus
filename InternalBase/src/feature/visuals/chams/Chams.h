#pragma once

using DrawArrayFn = void(__fastcall*)(void*, void*, void*, int, void*, void*, void*, void*);
extern DrawArrayFn oDrawArray;

namespace chams {
    bool Init();
    void Shutdown();
    void __fastcall DrawArrayHook(void* a1, void* a2, void* pMeshScene, int nMeshCount,
        void* pSceneView, void* pSceneLayer, void* pUnk, void* pUnk2);
}