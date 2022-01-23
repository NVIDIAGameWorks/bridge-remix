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
#include "d3d9_device_base.h"

#include "d3d9_lss.h"

#include <d3d9.h>
#include <util_servercommand.h>

BaseDirect3DDevice9Ex_LSS::BaseDirect3DDevice9Ex_LSS(const bool bExtended,
                                                     Direct3D9Ex_LSS* const pDirect3D,
                                                     const D3DDEVICE_CREATION_PARAMETERS& createParams,
                                                     const D3DPRESENT_PARAMETERS& presParams,
                                                     const D3DDISPLAYMODEEX* const pFullscreenDisplayMode,
                                                     HRESULT& hresultOut)
  : D3DBase<IDirect3DDevice9Ex>(nullptr, pDirect3D)
  , m_ex(bExtended)
  , m_pDirect3D(pDirect3D)
  , m_createParams(createParams) {
  Logger::debug("Creating Device...");

  // Games may override client's exception handler when it was setup early.
  // Attempt to restore the exeption handler.
  SetupExceptionHandler();

  // MSDN: For windowed mode, this parameter may be NULL only if the hDeviceWindow member
  // of pPresentationParameters is set to a valid, non-NULL value.

  const auto hWindow = createParams.hFocusWindow ? createParams.hFocusWindow : presParams.hDeviceWindow;
  setWinProc(hWindow);

  {
    ClientMessage c(Commands::IDirect3D9Ex_CreateDevice, getId());
    c.send_many(           createParams.AdapterOrdinal,
                           createParams.DeviceType,
                (uint32_t) createParams.hFocusWindow,
                           createParams.BehaviorFlags);
    if (m_ex) {
      assert(pFullscreenDisplayMode);
      c.send_data(sizeof(D3DDISPLAYMODEEX), pFullscreenDisplayMode);
    }
    c.send_data(sizeof(D3DPRESENT_PARAMETERS), &presParams);
  }
  Logger::debug("...server-side D3D9 device creation command sent...");

  Logger::debug("...waiting for create device ack response from server...");
  if (Result::Success != ServerMessage::waitForCommand(Commands::Bridge_Response)) {
    Logger::err("...server-side D3D9 device creation failed with: no response from server.");
    removeWinProc(hWindow);
    hresultOut = D3DERR_DEVICELOST;
    return;
  }
  Logger::debug("...create device response received from server...");
  const auto header = ServerMessage::pop_front();

  // Grab hresult from server
  hresultOut = (HRESULT) ServerMessage::get_data();
  assert(ServerMessage::get_data_pos() == header.dataOffset);

  if (FAILED(hresultOut)) {
    Logger::err(format_string("...server-side D3D9 device creation failed with %x.", hresultOut));
    // Release client device and report server error to the app
    removeWinProc(hWindow);
    return;
  }
  Logger::debug("...server-side D3D9 device successfully created...");
  Logger::debug("...Device successfully created!");
}
