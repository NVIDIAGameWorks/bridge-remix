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

#include <Windows.h>
#include <stdio.h>
#include <tlhelp32.h>
#include <Psapi.h>
#include <Shlwapi.h>

#include <sstream>

namespace bridge_util {

  /**
   * \brief Wrapper around Windows built-in GetModuleFileName
   *
   * \param [in] hModuleHandle the HMODULE in question. If NULL, defaults to parent executable.
   */
  static std::string getModuleFileName(const HMODULE hModuleHandle = NULL) {
#ifdef UNICODE
    WCHAR moduleFileName[MAX_PATH];
#else
    char moduleFileName[MAX_PATH];
#endif // !UNICODE
    GetModuleFileName(hModuleHandle, moduleFileName, MAX_PATH);
    std::stringstream ss;
    ss << moduleFileName << std::endl; // generic parsing whether wchar or char
    return ss.str();
  }

  static DWORD getParentPID() {
    const DWORD pid = GetCurrentProcessId();
    HANDLE h = NULL;
    PROCESSENTRY32 pe = { 0 };
    DWORD ppid = 0;
    pe.dwSize = sizeof(PROCESSENTRY32);
    h = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (::Process32First(h, &pe)) {
      do {
        if (pe.th32ProcessID == pid) {
          ppid = pe.th32ParentProcessID;
          break;
        }
      } while (::Process32Next(h, &pe));
    }
    ::CloseHandle(h);
    return (ppid);
  }

  static std::string getProcessName(DWORD pid) {
    HANDLE h = ::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (h) {
      char exePath[MAX_PATH + 1];

      DWORD len = ::GetModuleFileNameExA(h, NULL, &exePath[0], MAX_PATH);

      ::CloseHandle(h);

      return std::string(exePath);
    }

    return "";
  }

  static void killProcess() {
    const DWORD pid = GetCurrentProcessId();
    HANDLE hnd;
    hnd = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, TRUE, pid);
    TerminateProcess(hnd, 0);
  }
}