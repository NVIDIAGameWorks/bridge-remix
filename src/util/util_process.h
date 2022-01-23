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
#ifndef UTIL_PROCESS_H_
#define UTIL_PROCESS_H_

#include <windows.h>
#include <tchar.h>
#include <stdio.h>

namespace bridge_util {

  class Process {
  public:
    typedef void (*ProcessExitCallback)(Process const*);

    Process() = delete;
    Process(Process& p) = delete;

    Process(LPCTSTR cmd, ProcessExitCallback callback):
      hProcess(NULL),
      hWait(NULL),
      exitCallback(NULL) {
      hProcess = createChildProcess(cmd);
      RegisterExitCallback(callback);
    }

    ~Process() {
      releaseChildProcess();
    }

    bool PostMessageToMainThread(UINT msg, WPARAM wParam, LPARAM lParam) const;

    bool RegisterExitCallback(ProcessExitCallback callback);
    void UnregisterExitCallback();
    HANDLE GetCurrentProcessHandle();

  private:
    static void CALLBACK OnExited(void* context, BOOLEAN isTimeout) {
      ((Process*) context)->OnExited();
    }

    void OnExited() {
      if (exitCallback) {
        exitCallback(this);
      }
    }

    DWORD  processMainThreadId = 0;
    HANDLE hProcess;
    HANDLE hWait;
    HANDLE hDuplicate;
    ProcessExitCallback exitCallback;

    HANDLE createChildProcess(LPCTSTR szCmdline);

    void releaseChildProcess();
  };

}

#endif // UTIL_PROCESS_H_
