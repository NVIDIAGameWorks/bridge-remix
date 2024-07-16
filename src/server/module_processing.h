#pragma once

#include <atomic>

#include "remix_api/remix_c.h"
#include <mutex>

void processModuleCommandQueue(std::atomic<bool>* const bSignalEnd);

namespace remix_api {
  extern remixapi_Interface g_remix;
  extern bool g_remix_initialized;
  extern HMODULE g_remix_dll;
  extern IDirect3DDevice9Ex* g_device;
  extern std::mutex g_device_mutex;
  extern IDirect3DDevice9Ex* getDevice();
}
