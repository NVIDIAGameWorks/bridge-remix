/*
 * Copyright (c) 2022-2023, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#pragma once

#include <unknwn.h>
#include <unordered_map>

using ShadowMap = std::unordered_map<uintptr_t, IUnknown*>;
extern ShadowMap gShadowMap;

class BaseDirect3DDevice9Ex_LSS;

template<class WrapperType>
static WrapperType* trackWrapper(WrapperType* const pLss) {
  gShadowMap[pLss->getId()] = pLss;
  return pLss;
}

template<class WrapperType, class ParentType>
static WrapperType* getOrTrackWrapper(void* instance,
                                      BaseDirect3DDevice9Ex_LSS* const pDevice,
                                      ParentType* const pParent) {
  auto* const pD3D = (IUnknown*) instance;
  WrapperType* pLss;
  if (gShadowMap.count(pD3D) == 0 || pD3D == nullptr) {
    if constexpr (std::is_same_v<ParentType, void>) {
      pLss = new WrapperType(pD3D, pDevice);
    } else {
      pLss = new WrapperType(pD3D, pDevice, pParent);
    }

    gShadowMap[pLss->getD3DObj()] = pLss;
    return pLss;
  }
  pLss = bridge_cast<WrapperType*>(gShadowMap[pD3D]);
  pLss->AddRef();
  return pLss;
}

template<class WrapperType>
static WrapperType* getOrTrackWrapper(void* instance,
                                      BaseDirect3DDevice9Ex_LSS* const pDevice) {
  return getOrTrackWrapper<WrapperType, void>(instance, pDevice, nullptr);
}

template<class WrapperType>
static WrapperType* getWrapperOnly(void* instance,
                                   BaseDirect3DDevice9Ex_LSS* const pDevice) {
  auto* const pD3D = (IUnknown*) instance;
  if (gShadowMap.count(pD3D) == 0)
    return nullptr;

  WrapperType* pLss = bridge_cast<WrapperType*>(gShadowMap[pD3D]);
  pLss->AddRef();
  return pLss;
}