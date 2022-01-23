/*
 * Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.
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
#include "d3d9_surface.h"
#include "util_bridge_assert.h" 
#include "util_clientcommand.h"
#include "util_servercommand.h"
#include "util_texture_and_volume.h"

#include <assert.h>

using namespace bridge_util;

static HRESULT copyServerSurfaceRawData(Direct3DSurface9_LSS* const pLssSurface) {
  // Obtaining raw surface data buffer from server
  const uint32_t timeoutMs = GlobalOptions::getAckTimeout();
  if (Result::Success != ServerMessage::waitForCommandAndDiscard(Commands::Bridge_Response, timeoutMs)) {
    Logger::err("getServerSurfaceBufferData() failed with: no response from server.");
    return D3DERR_INVALIDCALL;
  }

  HRESULT res = (HRESULT)ServerMessage::get_data();
  if (!SUCCEEDED(res)) {
    return res;
  }

  uint32_t width = (uint32_t) ServerMessage::get_data();
  uint32_t height = (uint32_t) ServerMessage::get_data();
  const D3DFORMAT format = (D3DFORMAT) ServerMessage::get_data();
  void* pData = NULL;
  size_t pulledSize = ServerMessage::get_data(&pData);

  // Copy data into a surface
  const size_t rowSize = bridge_util::calcRowSize(width, (D3DFORMAT) format);
  const size_t numRows = bridge_util::calcStride(height, (D3DFORMAT) format);
  assert(pulledSize == numRows * rowSize);

  // Copying server side render target buffer to client surface
  D3DLOCKED_RECT lockedRect;
  res = pLssSurface->LockRect(&lockedRect, NULL, D3DLOCK_DISCARD);
  if (S_OK == res) {
    FOR_EACH_RECT_ROW(lockedRect, height, format,
      memcpy(ptr, (PBYTE) pData + y * rowSize, rowSize);
    );
    res = pLssSurface->UnlockRect();
  }

  return res;
}