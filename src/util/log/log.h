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

#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>

namespace bridge_util {

  enum class LogLevel: uint32_t {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    None = 5,
  };

  /**
   * \brief Logger
   *
   * Logger for one DLL. Creates a text file and
   * writes all log messages to that file.
   */
  class Logger {
  public:
    static void init(const LogLevel logLevel = LogLevel::None, void* hModuleLogOwner = NULL);

    static void trace(const std::string& message);
    static void debug(const std::string& message);
    static void info(const std::string& message);
    static void warn(const std::string& message);
    static void err(const std::string& message);
    static void log(const LogLevel level, const std::string& message);
    // The lowest level method. NOT thread-safe. Use at your own risk!
    static void logLine(const LogLevel level, const char* line);

    static void set_loglevel(const LogLevel level);

  private:
    inline static Logger* logger = nullptr;

    Logger(const LogLevel logLevel, void* hModuleLogOwner);
    ~Logger();

    static Logger& get(const LogLevel logLevel = LogLevel::None, void* hModuleLogOwner = NULL);
    LogLevel m_level;

    std::mutex m_mutex;

#ifdef _WIN32
    void* m_hFile;
#else
    std::ofstream m_fileStream;
#endif

    void emitMsg(const LogLevel level, const std::string& message);
    void emitLine(const LogLevel level, const char* line);
  };

  static LogLevel str_to_loglevel(const std::string& strLogLevel) {
    static std::unordered_map<std::string, LogLevel> const lut = {
      { "Trace", LogLevel::Trace },
      { "Debug", LogLevel::Debug },
      { "Info", LogLevel::Info },
      { "Warn", LogLevel::Warn },
      { "Error", LogLevel::Error },
      { "None", LogLevel::None }
    };
    auto it = lut.find(strLogLevel);
    if (it != lut.end()) {
      return it->second;
    } else {
      return LogLevel::Info;
    }
  }

  template<typename... Args>
  static std::string format_string(const std::string& format, Args... args) {
    int size_s = std::snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'
    if (size_s <= 0) {
      throw std::runtime_error("Error during formatting!");
    }
    auto size = static_cast<size_t>(size_s);
    auto buf = std::make_unique<char[]>(size);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
  }

}
