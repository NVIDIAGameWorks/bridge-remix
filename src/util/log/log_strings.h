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
#pragma once

#include <string>
#include <map>

namespace logger_strings {
  constexpr char* OutOfBufferMemory = "The host application has tried to write data larger than one of the RTX Remix Bridge's buffers. Increase one of the buffer sizes in \".trex\\bridge.conf\".\n\n";
  constexpr char* OutOfBufferMemory1 = " Buffer Option: ";
  constexpr char* MultipleActiveCommands = "Multiple active Command instances detected!";
  constexpr char* RtxRemixRuntimeError = "RTX Remix Runtime Error!";
  constexpr char* BridgeClientClosing = "The RTX Remix Runtime has encountered an unexpected issue. The application will close.\n\nPlease collect any *.log or *.dmp files next to the application or in the .trex folder, and report the error at https://github.com/NVIDIAGameWorks/rtx-remix/issues.";

  inline static const std::map<const std::string, const std::string> bufferNameToOptionMap =
  {
    {"ModuleClient2ServerData", "moduleClientChannelMemSize"},
    {"ModuleServer2ClientData", "moduleServerChannelMemSize"},
    {"DeviceClient2ServerData", "clientChannelMemSize"},
    {"DeviceServer2ClientData", "serverChannelMemSize"}
  };

  static inline std::string bufferNameToOption(std::string name) {
    std::string optionName = "";
    auto it = logger_strings::bufferNameToOptionMap.find(name);
    if (it != logger_strings::bufferNameToOptionMap.end()) {
      optionName = it->second;
    }
    return optionName;
  }

}