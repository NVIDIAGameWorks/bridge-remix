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
#ifndef UTIL_CLIENTCOMMAND_H_
#define UTIL_CLIENTCOMMAND_H_

#include "util_bridgecommand.h"
#include <cstdint>

extern IpcChannel gClientChannel;

template<typename T>
class ClientCommand: public BridgeCommand<T> {
public:
  ClientCommand(const Commands::D3D9Command command):
    ClientCommand(command, NULL) {
  }

  ClientCommand(const Commands::D3D9Command command, uintptr_t pHandle):
    ClientCommand(command, pHandle, 0) {
  }

  ClientCommand(const Commands::D3D9Command command, uintptr_t pHandle, const Commands::Flags commandFlags):
    BridgeCommand<T>(gClientChannel,
                     command,
                     pHandle,
                     commandFlags) {
  }

  static const T& get_data() {
    return BridgeCommand<T>::get_data(gClientChannel);
  }

  static const T& get_data(void** obj) {
    return BridgeCommand<T>::get_data(gClientChannel, obj);
  }

  template<typename V>
  static const T& copy_data(V& obj, bool checkSize = true) {
    return BridgeCommand<T>::copy_data(gClientChannel, obj, checkSize);
  }

  static Header pop_front() {
    return BridgeCommand<T>::pop_front(*gClientChannel.commands);
  }

  static bridge_util::Result ensureQueueEmpty() {
    return BridgeCommand<T>::ensureQueueEmpty(*gClientChannel.commands);
  }

  static bridge_util::Result waitForCommand(const Commands::D3D9Command& command = Commands::Bridge_Any, DWORD overrideTimeoutMS = 0) {
    return BridgeCommand<T>::waitForCommand(*gClientChannel.commands, command, overrideTimeoutMS);
  }

  static bridge_util::Result waitForCommandAndDiscard(const Commands::D3D9Command& command = Commands::Bridge_Any, DWORD overrideTimeoutMS = 0) {
    return BridgeCommand<T>::waitForCommandAndDiscard(*gClientChannel.commands, command, overrideTimeoutMS);
  }

  static size_t get_data_pos() {
    return BridgeCommand<T>::get_data_pos(*gClientChannel.data);
  }

  static bridge_util::Result begin_batch() {
    return BridgeCommand<T>::begin_batch(*gClientChannel.commands);
  }

  static size_t end_batch() {
    return BridgeCommand<T>::end_batch(*gClientChannel.commands);
  }

  static bridge_util::Result begin_read_data() {
    return BridgeCommand<T>::begin_read_data(*gClientChannel.data);
  }

  static size_t end_read_data() {
    return BridgeCommand<T>::end_read_data(*gClientChannel.data);
  }

private:
  ClientCommand(const ClientCommand&) = delete;
  ClientCommand(ClientCommand&&) = delete;
};


typedef ClientCommand<uint32_t> ClientMessage;

#endif // UTIL_CLIENTCOMMAND_H_
