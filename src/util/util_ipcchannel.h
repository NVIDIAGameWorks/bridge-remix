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
#ifndef UTIL_IPCCHANNEL_H_
#define UTIL_IPCCHANNEL_H_

#include "log/log.h"

#include "util_sharedmemory.h"

namespace bridge_util {
  // Due to semaphore latency, BlockingCircularQueue is slower than AtomicCircularQueue
#ifdef USE_BLOCKING_QUEUE
  typedef BlockingCircularQueue<Header> CommandQueue;
#else
  typedef AtomicCircularQueue<Header> CommandQueue;
#endif
}

// Helper struct that ties together everything needed to send commands and data and for synchronization
typedef struct _IpcChannel {
  bridge_util::CommandQueue* commands = nullptr;
  bridge_util::DataQueue* data = nullptr;
  int64_t* serverDataPos = nullptr;
  int64_t* clientDataExpectedPos = nullptr;
  bool* serverResetPosRequired = false;
  bridge_util::SharedMemory* sharedMem = nullptr;
  bridge_util::NamedSemaphore* dataSemaphore = nullptr;

  ~_IpcChannel() {
    if (commands) delete commands;
    if (data) delete data;
    if (sharedMem) delete sharedMem;
    if (dataSemaphore) delete dataSemaphore;
  }

  void initMem(const std::string& name, size_t memSize) {
    // Extra storage needed for data queue synchronization
    auto extra = sizeof(*serverDataPos) + sizeof(*clientDataExpectedPos) + sizeof(*serverResetPosRequired);

    // Allocate shared memory
    try {
      sharedMem = new bridge_util::SharedMemory(name, memSize + extra);
    }
    catch (...) {
      bridge_util::Logger::err("Failed to create the shared memory component of our channel. IPC impossible. Expect failures.");
      return;
    }
    m_sharedMemSize = memSize;
    auto memPtr = (uintptr_t) sharedMem->data();

    // Below pointers are used in preventing buffer override in data queue
    serverDataPos = (int64_t*) memPtr;
    clientDataExpectedPos = (int64_t*) (serverDataPos + sizeof(*serverDataPos));
    serverResetPosRequired = (bool*) (clientDataExpectedPos + sizeof(*clientDataExpectedPos));
  }

  void initQueues(const std::string& prefix, bridge_util::Accessor accessor, size_t cmdMemSize, size_t cmdQueueSize, size_t dataMemSize, size_t dataQueueSize) {
    assert(sharedMem != nullptr);
    if(!sharedMem) {
      return;
    }
    
    // Check that we're leaving enough space.
    assert(cmdMemSize + dataMemSize <= m_sharedMemSize);

    auto extra = sizeof(*serverDataPos) + sizeof(*clientDataExpectedPos) + sizeof(*serverResetPosRequired);
    auto memPtr = (uintptr_t) sharedMem->data();

    // Offsetting shared memory to account for 3 pointers used above
    commands = new bridge_util::CommandQueue(prefix + "Command", accessor, (void*) (memPtr + extra), cmdMemSize, cmdQueueSize);
    data = new bridge_util::DataQueue(prefix + "Data", accessor, (void*) (memPtr + extra + cmdMemSize), dataMemSize, dataQueueSize);
    dataSemaphore = new bridge_util::NamedSemaphore(prefix + "DataQueue", 0, 1);

    // Initialize buffer override protection
    if (accessor == bridge_util::Accessor::Writer) {
      *serverDataPos = -1;
      *clientDataExpectedPos = -1;
      *serverResetPosRequired = false;
    }
  }

private:
  size_t m_sharedMemSize = 0;

} IpcChannel;

#endif // UTIL_IPCCHANNEL_H_