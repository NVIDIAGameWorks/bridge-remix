/*
 * Copyright (c) 2022-2024, NVIDIA CORPORATION. All rights reserved.
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
#include "pch.h"

#include "version.h"

#include "d3d9_lss.h"
#include "d3d9_surface.h"
#include "d3d9_texture.h"
#include "d3d9_cubetexture.h"
#include "d3d9_swapchain.h"
#include "swapchain_map.h"
#include "client_options.h"
#include "config/config.h"
#include "config/global_options.h"
#include "di_hook.h"
#include "log/log.h"
#include "remix_state.h"
#include "util_bridge_assert.h"
#include "util_bridge_state.h"
#include "util_common.h"
#include "util_devicecommand.h"
#include "util_modulecommand.h"
#include "util_filesys.h"
#include "util_hack_d3d_debug.h"
#include "util_messagechannel.h"
#include "util_seh.h"
#include "util_semaphore.h"

#include <assert.h>
#include <sstream>
#include <stdio.h>
#include <string>
#include <mutex>

using namespace bridge_util;

std::atomic<uintptr_t> D3dBaseIdFactory::id_counter = 1;
uintptr_t D3dBaseIdFactory::getNextId() {
  return id_counter++;
}

#if defined(_DEBUG) || defined(DEBUGOPT)

std::map<std::thread::id, std::atomic<size_t>> FunctionEntryExitLogger::s_counters;

FunctionEntryExitLogger::FunctionEntryExitLogger(const std::string functionName, void* thiz) {
  if (GlobalOptions::getLogApiCalls() || GlobalOptions::getLogAllCalls()) {
    
    std::string tabs = "";
    if (s_counters.count(std::this_thread::get_id()) == 0) {
      s_counters[std::this_thread::get_id()] = 0;
    }
    std::atomic<size_t>& counter = s_counters[std::this_thread::get_id()];
    for (int i = 0; i < counter; i++) {
      tabs.append("\t");
    }
    if (!GlobalOptions::getLogAllCalls()) {
      if (counter == 0) {
        _LogFunctionCall(functionName, thiz);
      }
    }
    else {
      _LogFunctionCall(tabs + functionName + " ENTRY", thiz);
    }
    counter++;
    m_functionName = functionName;
    m_thiz = thiz;
  }

}

FunctionEntryExitLogger::~FunctionEntryExitLogger() {
  if (GlobalOptions::getLogApiCalls() || GlobalOptions::getLogAllCalls()) {
    std::atomic<size_t>& counter = s_counters[std::this_thread::get_id()];
    counter--;
    if (!GlobalOptions::getLogAllCalls()) {
      return;
    }
    std::string tabs = "";
    for (int i = 0; i < counter; i++) {
      tabs.append("\t");
    }
    _LogFunctionCall(tabs + m_functionName + " EXIT", m_thiz);
  }
}

#endif

static bool gIsAttached = false;
Guid gUniqueIdentifier;
Process* gpServer = nullptr;
NamedSemaphore* gpPresent = nullptr;
ShadowMap gShadowMap;
std::mutex gShadowMapMutex;
SwapChainMap gSwapChainMap;
std::mutex gSwapChainMapMutex;
std::unordered_map<HWND, std::deque<WNDPROC>> ogWndProcList;
std::mutex gWndProcListMapMutex;
std::unique_ptr<MessageChannelClient> gpRemixMessageChannel;  // Message channel with the Remix renderer
std::unique_ptr<MessageChannelClient> gpServerMessageChannel; // Message channel with the Bridge server
std::mutex serverStartMutex;
SceneState gSceneState = WaitBeginScene;
std::chrono::steady_clock::time_point gTimeStart;
bool gbBridgeRunning = true;
std::string gRemixFolder = "";
thread_local int gRemixWndProcEntryExitCount = 0;

void PrintRecentCommandHistory() {
  // Log history of recent client side commands sent and received by the server
  Logger::info("Most recent Device Queue commands sent from Client");
  DeviceBridge::Command::print_writer_data_sent();
  Logger::info("Most recent Device Queue commands received by Server");
  DeviceBridge::Command::print_writer_data_received();
  Logger::info("Most recent Module Queue commands sent from Client");
  ModuleBridge::Command::print_writer_data_sent();
  Logger::info("Most recent Module Queue commands received by Server");
  ModuleBridge::Command::print_writer_data_received();
}

// Setup bridge exception handler if requested
void SetupExceptionHandler() {
  if (ClientOptions::getSetExceptionHandler()) {
    ExceptionHandler::get().init();
  }
}

void OnServerExited(Process const* process) {
  BridgeState::setServerState(BridgeState::ProcessState::Exited);

  // Disable the bridge to terminate any ongoing processing
  gbBridgeRunning = false;
  // Notify the user that we have to shut down the bridge entirely because we don't have a renderer anymore
  if (BridgeState::getClientState() != BridgeState::ProcessState::DoneProcessing) {
    PrintRecentCommandHistory();
    Logger::errLogMessageBoxAndExit(logger_strings::BridgeClientClosing);
    std::abort();
  }

  const auto timeServerEnd = std::chrono::high_resolution_clock::now();
  std::stringstream uptimeSS;
  uptimeSS << "[Uptime] Server (estimated): ";
  uptimeSS <<
    std::chrono::duration_cast<std::chrono::seconds>(timeServerEnd - gTimeStart).count();
  uptimeSS << "s";
  Logger::info(uptimeSS.str());
}

void InitServer() {
  std::lock_guard<std::mutex> guard(serverStartMutex);
  if (gpServer != nullptr) {
    return;
  }
  Logger::info("Launching server with GUID " + gUniqueIdentifier.toString());
  std::stringstream cmdSS;
  cmdSS << gRemixFolder;
  cmdSS << ".trex/NvRemixBridge.exe";
  cmdSS << " " << gUniqueIdentifier.toString();
  cmdSS << " " << BRIDGE_VERSION;
  cmdSS << " " << std::string(GetCommandLineA());
  const std::string command = cmdSS.str();
  gpServer = new Process(command.c_str(), OnServerExited);

  if (ClientOptions::getEnableDpiAwareness()) {
    Logger::info("Process set as DPI aware");
    static HINSTANCE shcore_dll = ::LoadLibraryA("shcore.dll");
    if (shcore_dll != NULL) {
      typedef HRESULT(WINAPI* PFN_SetProcessDpiAwareness)(int);
      if (PFN_SetProcessDpiAwareness SetProcessDpiAwarenessFn = (PFN_SetProcessDpiAwareness)::GetProcAddress(shcore_dll, "SetProcessDpiAwareness")) {
        constexpr int PROCESS_PER_MONITOR_DPI_AWARE = 2;
        SetProcessDpiAwarenessFn(PROCESS_PER_MONITOR_DPI_AWARE);
      } else {
        SetProcessDPIAware();
      }
    } else {
      DWORD errorCode = GetLastError();
      Logger::err("Fail shcore.dll, error code: " + std::to_string(errorCode));
    }
  }

  BridgeState::setServerState(BridgeState::ProcessState::Init);

  // Initialize our shared queue as a Reader.
  Logger::info("Sending SYN command, waiting for ACK from server...");
  ClientMessage { Commands::Bridge_Syn, (uintptr_t) gpServer->GetCurrentProcessHandle() };

  BridgeState::setClientState(BridgeState::ProcessState::Handshaking);
  const auto waitForAckResult = DeviceBridge::waitForCommand(Commands::Bridge_Ack, GlobalOptions::getStartupTimeout());
  switch (waitForAckResult) {
  case Result::Timeout:
  {
    Logger::err("Timeout. Connection not established to server.");
    Logger::err("Are you sure the server was invoked by this application and is running?");
    BridgeState::setServerState(BridgeState::ProcessState::DoneProcessing);
    gbBridgeRunning = false;
    return;
  }
  case Result::Failure:
  {
    Logger::err("Failed to connect to server.");
    BridgeState::setServerState(BridgeState::ProcessState::DoneProcessing);
    gbBridgeRunning = false;
    return;
  }
  }
  // Remove Ack from queue and get thread id for thread proc message handler from server
  const auto ackResponse = DeviceBridge::pop_front();
  gpServerMessageChannel = std::make_unique<MessageChannelClient>(static_cast<uint32_t>(ackResponse.pHandle));
  {
    // Special handling for certain window messages to disable semaphore timeouts
    // when the game window is not currently active or in the foreground. Note
    // that using keyboard focus is more reliable than WM_ACTIVATE and also does
    // not lead to duplicate messages.
    gpServerMessageChannel->registerHandler(WM_KILLFOCUS, [](uintptr_t wParam, intptr_t lParam) {
      Logger::info("Client window became inactive, disabling timeouts for bridge client...");
      GlobalOptions::setInfiniteRetries(true);
      gpServerMessageChannel->send(WM_KILLFOCUS, wParam, lParam);
      return true;
    });

    gpServerMessageChannel->registerHandler((UINT) WM_SETFOCUS, [](uintptr_t wParam, intptr_t lParam) {
      Logger::info("Client window became active, reenabling timeouts for bridge client...");
      GlobalOptions::setInfiniteRetries(false);
      gpServerMessageChannel->send(WM_SETFOCUS, wParam, lParam);
      return true;
    });
  }

  BridgeState::setServerState(BridgeState::ProcessState::Handshaking);
  Logger::info("Ack received! Handshake completed! Telling server to continue waiting for commands...");
  ClientMessage { Commands::Bridge_Continue };

#ifdef DEBUG
  {
    const char* text = "Hello";
    ClientMessage command(Commands::Bridge_DebugMessage);
    command.send_data(42);
    command.send_data(strlen(text), (void*) text);
  }
  {
    const char* text = "World!";
    ClientMessage command(Commands::Bridge_DebugMessage);
    command.send_data(1313);
    command.send_data(strlen(text), (void*) text);
  }
  {
    const char* text = "Test!";
    ClientMessage command(Commands::Bridge_DebugMessage);
    command.send_data(4090);
    command.send_data(strlen(text), (void*) text);
  }
#endif
  BridgeState::setClientState(BridgeState::ProcessState::Running);
  BridgeState::setServerState(BridgeState::ProcessState::Running);
  
  if (GlobalOptions::getUseSharedHeap()) {
    SharedHeap::init();
  }
}

bool InitRemixFolder(HMODULE hinst) {
  if (!gRemixFolder.empty()) {
    return true;
  }

  DWORD len;
  std::string tmp(MAX_PATH, '\0');
  do {
    len = GetModuleFileNameA(hinst, tmp.data(), tmp.length());

    if (len > 0 && len < tmp.length()) {
      break;
    }

    tmp.resize(tmp.length() * 2);
  } while (true);

  // Trim filename
  while (len) {
    if (tmp[len] == '\\' || tmp[len] == '/') {
      tmp.resize(len + 1);
      break;
    }
    --len;
  }

  gRemixFolder = tmp;

  return true;
}

/*
 * Public exports needed for D3D
 */
extern "C" {

  HRESULT WINAPI Direct3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex** ppDeviceEx) {
    return LssDirect3DCreate9Ex(SDKVersion, ppDeviceEx);
  }

  IDirect3D9* WINAPI Direct3DCreate9(UINT SDKVersion) {
    return LssDirect3DCreate9(SDKVersion);
  }

  DLLEXPORT int __stdcall D3DPERF_BeginEvent(D3DCOLOR col, LPCWSTR wszName) {
    return 0;
  }

  DLLEXPORT int __stdcall D3DPERF_EndEvent(void) {
    return 0;
  }

  DLLEXPORT void __stdcall D3DPERF_SetMarker(D3DCOLOR col, LPCWSTR wszName) {
  }

  DLLEXPORT void __stdcall D3DPERF_SetRegion(D3DCOLOR col, LPCWSTR wszName) {
  }

  DLLEXPORT BOOL __stdcall D3DPERF_QueryRepeatFrame(void) {
    return FALSE;
  }

  DLLEXPORT void __stdcall D3DPERF_SetOptions(DWORD dwOptions) {
  }

  DLLEXPORT DWORD __stdcall D3DPERF_GetStatus(void) {
    return 0;
  }

  DLLEXPORT void __stdcall DebugSetMute(void) {
  }

  DLLEXPORT int __stdcall DebugSetLevel(void) {
    return 0;
  }

  DLLEXPORT int __stdcall Direct3D9EnableMaximizedWindowedModeShim(UINT a) {
    return 0;
  }
}

// Process a window message for Remix purposes.
// Returns true when messsage was consumed by Remix and needs to be swallowed
// and so removed from the client application queue.
bool ProcessMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
#ifdef _DEBUG
  Logger::info(format_string("msg: %d, %d, %d, %d", msg, hWnd, wParam, lParam));
#endif

  {
    std::scoped_lock lock(gWndProcListMapMutex);
    if (ogWndProcList.find(hWnd) == ogWndProcList.end())
      return true;
  }

  const bool uiWasActive = RemixState::isUIActive();

  // Process remix renderer-related messages
  if (gpRemixMessageChannel->onMessage(msg, wParam, lParam)) {
    if (!uiWasActive && RemixState::isUIActive()) {
      // Remix UI has been activated - unstick modifier keys at application side
      WNDPROC prevWndProc;
      {
        std::scoped_lock lock(gWndProcListMapMutex);
        prevWndProc = ogWndProcList[hWnd][0];
      }
      auto unstick = [hWnd, prevWndProc](uint32_t vk) {
        CallWindowProc(prevWndProc, hWnd, WM_KEYUP, vk,
          ((KF_REPEAT + KF_UP + MapVirtualKeyA(vk, MAPVK_VK_TO_VSC)) << 16) + 1);
      };

      unstick(VK_CONTROL);
      unstick(VK_SHIFT);
      unstick(VK_INSERT);

      // To be able to ignore target app WinHooks,
      // bridge WinHooks must be on a top of the hook chain.
      // So reattach bridge WinHooks each time, as the app might
      // set and reset its own hooks at any moment
      InputWinHooksReattach();
    }

    // Message was handled - bail out
    return true;
  }

  // Process server-related messages
  gpServerMessageChannel->onMessage(msg, wParam, lParam);

  if (RemixState::isUIActive()) {
    // ImGUI attempts to track when mouse leaves the window area using Windows API.
    // Some games with DirectInput in windowed mode may receive a WM_MOUSELEAVE message
    // after every WM_MOUSEMOVE message and this will result in ImGUI mouse cursor
    // toggling between -FLT_MAX and current mouse position.
    // To WAR it just swallow the WM_MOUSELEAVE messages when Remix UI is active.
    if (msg == WM_MOUSELEAVE) {
      // Swallow message 
      return true;
    }

    // Game overlay style message swallowing section
    {
      if (msg == WM_SYSCOMMAND) {
        // Swallow window move and size messages when UI is active.
        if (wParam == SC_MOVE || wParam == SC_SIZE || wParam == 0xF012 ||
            wParam == SC_MINIMIZE || wParam == SC_MAXIMIZE) {
          return true;
        }
      }

      // Process WM_NCMOUSEMOVE to WM_NCXBUTTONDBLCLK
      if ((msg & 0xFFA0) == 0x00A0) {
        // Swallow all non-client window messages when UI is active.
        switch (wParam) {
        case HTCLOSE:
          // Only Close button is allowed
          break;
        default:
          return true;
        }
      }
    }
  }

  // WAR: on Win11 preview build 25236 the WM_INPUT message sent to a thread proc
  // causes a WIN32K_CRITICAL_FAILURE bug check. This could creep into winnext
  // release so let's just block it here since we do not need this message anyway
  // on Remix renderer side.
  const bool doForward = msg != WM_INPUT;

  // Forward to remix renderer
  if (doForward) {
    gpRemixMessageChannel->send(msg, wParam, lParam);
  }

  // Block the input message when Remix UI is active
  if (RemixState::isUIActive() && isInputMessage(msg)) {
    // Block all input except ALT key up event.
    // ALT is a very special key, we must pass the key up event for ALT or risking
    // to stop receiving mouse events.
    if (msg != WM_KEYUP || wParam != VK_MENU) {
      return true;
    }
  }
  return false;
}

LRESULT WINAPI RemixWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  const bool isUnicode = IsWindowUnicode(hWnd);

  LRESULT lresult = 0;
  gRemixWndProcEntryExitCount += 1;
  int curEntryExitCount = gRemixWndProcEntryExitCount;
  // Detecting recursive calls
  if (curEntryExitCount > 1) {
    int wndProcListLen = 0;
    {
      std::scoped_lock lock(gWndProcListMapMutex);
      if(ogWndProcList.find(hWnd) != ogWndProcList.end())
        wndProcListLen = ogWndProcList[hWnd].size();
    }
    int curWndProcIndex = curEntryExitCount-1;
    if (curWndProcIndex >= wndProcListLen) {
      gRemixWndProcEntryExitCount = 0;
      lresult = !isUnicode ?
        DefWindowProcA(hWnd, msg, wParam, lParam) :
        DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    else {
      WNDPROC prevWndProc;
      {
        std::scoped_lock lock(gWndProcListMapMutex);
        prevWndProc = ogWndProcList[hWnd][curWndProcIndex];
      }
      lresult = !isUnicode ?
        CallWindowProcA(prevWndProc, hWnd, msg, wParam, lParam) :
        CallWindowProcW(prevWndProc, hWnd, msg, wParam, lParam);
    }
  }
  else {
    if (msg == WM_ACTIVATEAPP || msg == WM_SIZE || msg == WM_DESTROY) {
      gSwapChainMapMutex.lock();
      if (gSwapChainMap.find(hWnd) != gSwapChainMap.end()) {
        /*
        * The following lines are adapted from source in the DXVK repo
        * at https://github.com/doitsujin/dxvk/blob/master/src/d3d9/d3d9_window.cpp
        */
        if (msg == WM_DESTROY) {
          gSwapChainMap.erase(hWnd);
        } else {
          struct WindowDisplayData data = gSwapChainMap[hWnd];
          D3DPRESENT_PARAMETERS presParams = data.presParam;
          if (msg == WM_ACTIVATEAPP && !presParams.Windowed && !(msg == WM_NCCALCSIZE && wParam == TRUE)) {
            D3DDEVICE_CREATION_PARAMETERS create_parms = data.createParam;
            if (!(create_parms.BehaviorFlags & D3DCREATE_NOWINDOWCHANGES)) {
              if (wParam) {
                RECT rect;
                bridge_util::GetMonitorRect(bridge_util::GetDefaultMonitor(), &rect);
                SetWindowPos(hWnd, HWND_TOP, rect.left, rect.top, presParams.BackBufferWidth, presParams.BackBufferHeight,
                  SWP_NOACTIVATE | SWP_NOZORDER | SWP_ASYNCWINDOWPOS);
                Logger::info(format_string("Window's position is reset. Left: %d, Top: %d, Width: %d, Height: %d", rect.left, rect.top, presParams.BackBufferWidth, presParams.BackBufferHeight));
              } else {
                if (IsWindowVisible(hWnd))
                  ShowWindowAsync(hWnd, SW_MINIMIZE);
              }
            }
          } else if (msg == WM_SIZE) {
            D3DDEVICE_CREATION_PARAMETERS create_parms = data.createParam;

            if (!(create_parms.BehaviorFlags & D3DCREATE_NOWINDOWCHANGES) && !IsIconic(hWnd)) {
              PostMessageW(hWnd, WM_ACTIVATEAPP, 1, GetCurrentThreadId());
            }
          }
        }
      }
      gSwapChainMapMutex.unlock();
    }
    if (ProcessMessage(hWnd, msg, wParam, lParam)) {
      lresult = !isUnicode ?
        DefWindowProcA(hWnd, msg, wParam, lParam) :
        DefWindowProcW(hWnd, msg, wParam, lParam);
    } else {
      WNDPROC prevWndProc;
      {
        std::scoped_lock lock(gWndProcListMapMutex);
        prevWndProc = ogWndProcList[hWnd][0];
      }
      lresult = !isUnicode ?
        CallWindowProcA(prevWndProc, hWnd, msg, wParam, lParam) :
        CallWindowProcW(prevWndProc, hWnd, msg, wParam, lParam);
    }
  }
  gRemixWndProcEntryExitCount -= 1;
  return lresult;
}

void setWinProc(const HWND hwnd, const bool bForce) {
  if (!bForce) {
    std::scoped_lock lock(gWndProcListMapMutex);
    if (ogWndProcList.find(hwnd) != ogWndProcList.end())
      return;
  }
  WNDPROC prevWndProc = reinterpret_cast<WNDPROC>(
    SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(RemixWndProc))
  );
  {
    std::scoped_lock lock(gWndProcListMapMutex);
    // Remove previously stored window procedure to avoid duplicates
    if (bForce) {
      auto it = std::find(ogWndProcList[hwnd].begin(), ogWndProcList[hwnd].end(), prevWndProc);
      if (it != ogWndProcList[hwnd].end()) {
        ogWndProcList[hwnd].erase(it);
      }
    }
    ogWndProcList[hwnd].push_front(prevWndProc);
  }
  DInputSetDefaultWindow(hwnd);
}

void removeWinProc(const HWND hwnd) {
  WNDPROC prevWndProc;
  {
    std::scoped_lock lock(gWndProcListMapMutex);
    if (ogWndProcList.find(hwnd) != ogWndProcList.end()) {
      int wndProcListLen = ogWndProcList[hwnd].size();
      prevWndProc = ogWndProcList[hwnd][wndProcListLen - 1];
      ogWndProcList[hwnd].clear();
      ogWndProcList.erase(hwnd);
    } 
    else
      return;
  }
  SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(prevWndProc));
}

bool RemixAttach(HMODULE hModule) {
  if (!gIsAttached) {
    hModule = hModule != NULL ? hModule : GetModuleHandle(nullptr);

    // Initialize logger with level Info until we read the config file and
    // know whether the level needs to be set to something else. That way
    // we can still get log messages from the Config module itself.
    Logger::init(LogLevel::Info, hModule);

    // Setup Remix folder first hand
    if (!InitRemixFolder(hModule)) {
      Logger::err("Fatal: Unable to initialize Remix folder...");
      return false;
    }

    Config::init(Config::App::Client, hModule);
    GlobalOptions::init();
    Logger::set_loglevel(GlobalOptions::getLogLevel());

    SetupExceptionHandler();

    // Identify yourself
    Logger::info("==================\nNVIDIA RTX Remix Bridge Client\n==================");
    Logger::info(std::string("Version: ") + std::string(BRIDGE_VERSION));
    auto clientPath = getModuleFileName();
    Logger::info(format_string("Loaded d3d9.dll from %s", clientPath.c_str()));

    DInputHookAttach();

    gpRemixMessageChannel = std::make_unique<MessageChannelClient>("UWM_REMIX_BRIDGE_REGISTER_THREADPROC_MSG");
    RemixState::init(*gpRemixMessageChannel);

    initModuleBridge();
    initDeviceBridge();

    gpPresent = new NamedSemaphore("Present", 0, GlobalOptions::getPresentSemaphoreMaxFrames());

    BridgeState::setClientState(BridgeState::ProcessState::Init);

    // Deprecated config options, will be removed in future versions!!!
    if (bridge_util::Config::isOptionDefined("client.shaderVersion")) {
      Logger::warn("[deprecated-config] 'client.shaderVersion' has been deprecated, please use d3d9.shaderModel in the dxvk.conf instead");
    }

    if (bridge_util::Config::isOptionDefined("client.maxActiveLights")) {
      Logger::warn("[deprecated-config] 'client.maxActiveLights' has been deprecated, please use d3d9.maxActiveLights in the dxvk.conf instead");
    }

#ifdef WITH_INJECTION
    void RemixDetach();
    atexit(RemixDetach);
#endif

    gIsAttached = true;
  }

  return true;
}

void RemixDetach() {
  if (gIsAttached) {
    BridgeState::setClientState(BridgeState::ProcessState::DoneProcessing);
    Logger::info("About to unload bridge client.");

    if (gpServer) {
      // Instruct the server to wrap things up and bail
      // Note that while we can queue up the command the semaphore doesn't work anymore at this point
      Logger::info("Sending Terminate command to server...");
      // Unregister exit callback handler first so we don't trigger it when exiting the server normally
      gpServer->UnregisterExitCallback();
      // Send Terminate command immediately before we clean up resources
      {
        ClientMessage { Commands::Bridge_Terminate };
      }

      const auto result = DeviceBridge::waitForCommandAndDiscard(Commands::Bridge_Ack,
                                                                  GlobalOptions::getCommandTimeout());
      if (RESULT_SUCCESS(result)) {
        Logger::info("Server notified that it has cleanly terminated. Cleaning up.");
      } else {
        Logger::err("Timeout waiting for clean server termination. Moving ahead anyway.");
      }

      delete gpServer;
    }

    PrintRecentCommandHistory();

    // Clean up resources
    delete gpPresent;

    Logger::info("Shutdown cleanup successful, exiting now!");
    BridgeState::setClientState(BridgeState::ProcessState::Exited);

    DInputHookDetach();

    gIsAttached = false;
  }
}

/*
 * Direct3D9 Interface Implementation
 */

HRESULT LssDirect3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex** ppDeviceEx) {
  if (!RemixAttach(NULL)) {
    return D3DERR_NOTAVAILABLE;
  }

  // A game may override client's exception handler when it was setup early.
  // Attempt to restore the exeption handler.
  SetupExceptionHandler();

  (*ppDeviceEx) = new Direct3D9Ex_LSS((IDirect3D9Ex*) nullptr);
  InitServer();
  return S_OK;
}

IDirect3D9* LssDirect3DCreate9(UINT SDKVersion) {
  if (!RemixAttach(NULL)) {
    return nullptr;
  }

  // A game may override client's exception handler when it was setup early.
  // Attempt to restore the exeption handler.
  SetupExceptionHandler();

  auto retval = new Direct3D9Ex_LSS((IDirect3D9*) nullptr);
  InitServer();
  return retval;
}
