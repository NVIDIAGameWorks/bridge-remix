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
#ifndef UTIL_SERVERCOMMAND_H_
#define UTIL_SERVERCOMMAND_H_

#include "util_bridgecommand.h"

extern IpcChannel gServerChannel;

template<typename T>
class ServerCommand: public BridgeCommand<T> {
public:
  ServerCommand(const Commands::D3D9Command command):
    ServerCommand(command, NULL) {
  }

  ServerCommand(const Commands::D3D9Command command, uintptr_t pHandle):
    ServerCommand(command, pHandle, 0) {
  }

  ServerCommand(const Commands::D3D9Command command, uintptr_t pHandle, const Commands::Flags commandFlags):
    BridgeCommand<T>(gServerChannel,
                     command,
                     pHandle,
                     commandFlags) {
  }

  static const T& get_data() {
    return BridgeCommand<T>::get_data(gServerChannel);
  }

  static const T& get_data(void** obj) {
    return BridgeCommand<T>::get_data(gServerChannel, obj);
  }

  template<typename V>
  static const T& copy_data(V& obj, bool checkSize = true) {
    return BridgeCommand<T>::copy_data(gServerChannel, obj, checkSize);
  }

  static Header pop_front() {
    return BridgeCommand<T>::pop_front(*gServerChannel.commands);
  }

  static bridge_util::Result ensureQueueEmpty() {
    return BridgeCommand<T>::ensureQueueEmpty(*gServerChannel.commands);
  }

  static bridge_util::Result waitForCommand(const Commands::D3D9Command& command = Commands::Bridge_Any, DWORD overrideTimeoutMS = 0) {
    return BridgeCommand<T>::waitForCommand(*gServerChannel.commands, command, overrideTimeoutMS);
  }

  static bridge_util::Result waitForCommandAndDiscard(const Commands::D3D9Command& command = Commands::Bridge_Any, DWORD overrideTimeoutMS = 0) {
    return BridgeCommand<T>::waitForCommandAndDiscard(*gServerChannel.commands, command, overrideTimeoutMS);
  }

  static size_t get_data_pos() {
    return BridgeCommand<T>::get_data_pos(*gServerChannel.data);
  }

  static bridge_util::Result begin_batch() {
    return BridgeCommand<T>::begin_batch(*gServerChannel.commands);
  }

  static size_t end_batch() {
    return BridgeCommand<T>::end_batch(*gServerChannel.commands);
  }

  static bridge_util::Result begin_read_data() {
    return BridgeCommand<T>::begin_read_data(*gServerChannel.data);
  }

  static size_t end_read_data() {
    return BridgeCommand<T>::end_read_data(*gServerChannel.data);
  }

private:
  ServerCommand(const ServerCommand&) = delete;
  ServerCommand(ServerCommand&&) = delete;
};


typedef ServerCommand<uint32_t> ServerMessage;

#endif // UTIL_SERVERCOMMAND_H_
