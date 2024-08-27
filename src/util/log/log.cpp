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
#include "log.h"
#include "log_strings.h"

#include "util_filesys.h"

#include <array>
#include <iostream>
#include <iomanip>
#include <sstream>

#ifndef _WIN32
#include <sys/time.h>
#endif

namespace bridge_util {
  template<int N>
  static inline void getLocalTimeString(char (&timeString)[N]) {
    // [HH:MM:SS.MS]
    static const char* format = "[%02d:%02d:%02d.%03d]";

#ifdef _WIN32
    SYSTEMTIME lt;
    GetLocalTime(&lt);

    sprintf_s(timeString, format,
              lt.wHour, lt.wMinute, lt.wSecond, lt.wMilliseconds);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);

    struct tm* lt = localtime(&tv.tv_sec);

    sprintf_s(timeString, format,
              lt->tm_hour, lt->tm_min, lt->tm_sec, (tv.tv_usec / 1000) % 1000);
#endif
  }

  inline static Logger* logger = nullptr;

  void Logger::init(const LogLevel logLevel, void* hModuleLogOwner) {
    logger = nullptr;
    get(logLevel, hModuleLogOwner);
  }

  Logger& Logger::get(const LogLevel logLevel, void* hModuleLogOwner) {
    if (logger == nullptr) {
      logger = new Logger(logLevel, hModuleLogOwner);
    }
    return *logger;
  }

  Logger::Logger(const LogLevel log_level, void* hModuleLogOwner)
    : m_level(log_level) {
    if (m_level != LogLevel::None) {
      const HMODULE _hModuleLogOwner = reinterpret_cast<HMODULE>(hModuleLogOwner);
      auto moduleFilePath = getModuleFileName(_hModuleLogOwner);
      const size_t dotExePos = moduleFilePath.find_last_of('.');
      std::string logNameStr = (dotExePos != std::string::npos) ? moduleFilePath.substr(0, dotExePos) : "out";
#ifdef _WIN32
      char logPath[MAX_PATH];
      sprintf_s(logPath, "%s.log", logNameStr.c_str());

      uint32_t attempt = 0;
      while (attempt < 4) {
        m_hFile = CreateFileA(logPath, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL |
                            FILE_FLAG_WRITE_THROUGH, NULL);
        if (m_hFile != INVALID_HANDLE_VALUE) {
          break;
        }

        // Dump Win32 errors to debug output in DEBUG mode
        emitMsg(LogLevel::Error, format_string("Log CreateFile() failed with %d", GetLastError()));

        attempt++;
        sprintf_s(logPath, "%s_%02d.log", logNameStr.c_str(), attempt);
      }
#else
      m_fileStream = std::ofstream(logPathStr.c_str());
#endif
    }
  }

  Logger::~Logger() {
#ifdef _WIN32
    CloseHandle(m_hFile);
#else
    if (m_fileStream.is_open()) {
      m_fileStream.close();
    }
#endif
  }

  void Logger::trace(const std::string& message) {
    get().emitMsg(LogLevel::Trace, message);
  }

  void Logger::debug(const std::string& message) {
    get().emitMsg(LogLevel::Debug, message);
  }

  void Logger::info(const std::string& message) {
    get().emitMsg(LogLevel::Info, message);
  }

  void Logger::warn(const std::string& message) {
    get().emitMsg(LogLevel::Warn, message);
  }

  void Logger::err(const std::string& message) {
    get().emitMsg(LogLevel::Error, message);
  }

  void Logger::errLogMessageBoxAndExit(const std::string& message) {
    Logger::err(message);
    MessageBox(nullptr, message.c_str(), logger_strings::RtxRemixRuntimeError, MB_OK | MB_TOPMOST | MB_TASKMODAL);
    std::exit(-1 );
  }

  void Logger::log(const LogLevel level, const std::string& message) {
    get().emitMsg(level, message);
  }

  void Logger::logLine(const LogLevel level, const char* line) {
    get().emitLine(level, line);
  }

  void Logger::emitMsg(const LogLevel level, const std::string& message) {
    if (level >= m_level) {
      std::lock_guard<std::mutex> lock(m_mutex);

      std::stringstream stream(message);
      std::string       line;

      while (std::getline(stream, line, '\n')) {
        emitLine(level, line.c_str());
      }
    }
  }

  void Logger::emitLine(const LogLevel level, const char* line) {
    static std::array<const char*, 5> s_prefixes
      = { { "trace: ", "debug: ", "info:  ", "warn:  ", "err:   " } };

    const char* prefix = s_prefixes.at(static_cast<uint32_t>(level));

    char timeString[64];
    getLocalTimeString(timeString);

    static char tmpSpace[4096];
    int len = sprintf_s(tmpSpace, "%s %s%s\n", timeString, prefix, line);

    if (len <= 0) {
      return;
    }

#ifdef _WIN32

#ifdef _DEBUG
    OutputDebugStringA(tmpSpace);
#endif

    if (m_hFile != INVALID_HANDLE_VALUE) {
      WriteFile(m_hFile, tmpSpace, len, NULL, NULL);
    }

#else

    if (m_fileStream.is_open()) {
      m_fileStream << prefix << line;
    }

#endif
  }

  void Logger::set_loglevel(const LogLevel level) {
    get().m_level = level;
  }

}
