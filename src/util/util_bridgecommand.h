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
#ifndef UTIL_BRIDGECOMMAND_H_
#define UTIL_BRIDGECOMMAND_H_

#include "util_common.h"
#include "util_commands.h"
#include "util_circularbuffer.h"
#include "util_atomiccircularqueue.h"
#include "util_blockingcircularqueue.h"
#include "util_bridge_state.h"
#include "util_ipcchannel.h"

#include "../tracy/tracy.hpp"

extern bool gbBridgeRunning;

#define WAIT_FOR_SERVER_RESPONSE(func, value) \
  { \
    const uint32_t timeoutMs = GlobalOptions::getAckTimeout(); \
    if (Result::Success != ServerMessage::waitForCommandAndDiscard(Commands::Bridge_Response, timeoutMs)) { \
      Logger::err(func " failed with: no response from server."); \
      return value; \
    } \
  }

#define WAIT_FOR_OPTIONAL_SERVER_RESPONSE(func, value) \
  { \
    if (GlobalOptions::getSendAllServerResponses()) { \
      WAIT_FOR_SERVER_RESPONSE(func, value) \
      return (HRESULT) ServerMessage::get_data(); \
    } else { \
      return D3D_OK; \
    } \
  } 


template<typename T>
class BridgeCommand
{
public:
  BridgeCommand(const IpcChannel& ipcChannel, const Commands::D3D9Command command):
    BridgeCommand(command, NULL) {
  }

  BridgeCommand(const IpcChannel& ipcChannel, const Commands::D3D9Command command, uintptr_t pHandle):
    BridgeCommand(command, pHandle, 0) {
  }

  BridgeCommand(const IpcChannel& ipcChannel, const Commands::D3D9Command command, uintptr_t pHandle, const Commands::Flags commandFlags):
    m_ipcChannel(ipcChannel),
    m_command(command),
    m_commandFlags(commandFlags),
    m_handle((uint32_t) (size_t) pHandle) {
    // If the assert or exception gets triggered it means that there is more than one BridgeCommand
    // instance in a function or command block with overlapping object lifecycles. Only one instance
    // can be alive at a time to ensure data integrity on the command and data buffers. To resolve
    // this issue I recommend enclosing the BridgeCommand object in its own scope block, and make
    // sure there is no command nesting happening either.
    assert(!s_inProgress);
    if (s_inProgress) {
      Logger::err("Multiple active BridgeCommand instances detected!");
      throw std::exception("Multiple active BridgeCommand instances detected!");
    }
    // Only start a data batch if the bridge is actually enabled, otherwise this becomes a no-op
    if (gbBridgeRunning) {
      ipcChannel.data->begin_batch();
    }
    s_inProgress = true;
    m_batchStartPos = (int32_t) ipcChannel.data->get_pos();
    s_counter++;
  }

  ~BridgeCommand() {
    // Only actually send the command if the bridge is enabled, otherwise this becomes a no-op
    if (gbBridgeRunning) {
      m_ipcChannel.data->end_batch();
      m_batchStartPos = -1;
      uint32_t numRetries = 0;
      Result result;
      // We check if the bridge is enabled for each loop iteration in case it
      // was disabled externally by the server process exit callback.
      do {
        result = m_ipcChannel.commands->push({ m_command, m_commandFlags, (uint32_t) m_ipcChannel.data->get_pos(), m_handle });
      } while (
           RESULT_FAILURE(result)
        && numRetries++ < GlobalOptions::getCommandRetries()
        && gbBridgeRunning
#ifdef REMIX_BRIDGE_CLIENT
        && BridgeState::getServerState_NoLock() == BridgeState::ProcessState::Running
#endif
      );
#ifdef REMIX_BRIDGE_CLIENT
      if (BridgeState::getServerState_NoLock() >= BridgeState::ProcessState::DoneProcessing) {
        Logger::warn(format_string("The command %s will not be sent; Server is in the process of or has already shut down. Turning bridge off.", Commands::toString(m_command).c_str()));
        gbBridgeRunning = false;;
      } else
#endif
        if (RESULT_FAILURE(result) && gbBridgeRunning) {
          Logger::err(format_string("The command %s could not be successfully sent, turning bridge off and falling back to client rendering!", Commands::toString(m_command).c_str()));
          gbBridgeRunning = false;
        } else if (RESULT_SUCCESS(result) && numRetries > 1) {
          std::string command = Commands::toString(m_command);
          Logger::debug(format_string("The command %s took %d retries (%d ms)!", command.c_str(), numRetries, numRetries * GlobalOptions::getCommandTimeout()));
        }
    }

    s_inProgress = false;
  }

  inline void syncDataQueue(size_t expectedMemUsage, bool posResetOnLastIndex = false) const {
    int serverCount = *m_ipcChannel.serverDataPos;
    size_t currClientDataPos = get_data_pos(*m_ipcChannel.data);
    size_t expectedClientDataPos = currClientDataPos + ((expectedMemUsage != 0) ? expectedMemUsage : 1) - 1;
    size_t totalSize = m_ipcChannel.data->get_total_size();

    if (expectedClientDataPos >= totalSize) {
      if (posResetOnLastIndex) {
        // Reset index pos to 0 if the size is larger than the remaining buffer
        expectedClientDataPos = expectedMemUsage - 1;
      } else {
        // Evaluate the respective pos when the end of the queue is reached
        expectedClientDataPos = expectedClientDataPos - totalSize;
      }
      // Below variable is set when the server needs to complete a loop to get to
      // the client's expected position. When this is set on pull we check if
      // m_pos is reset and toggle this variable if it is
      *m_ipcChannel.serverResetPosRequired = true;
    }

    /*
     * Override conditions
     * 1. client < server, expectedClient >= server
     * 2. client > server, expectedClient >= server, expectedClient < client
     */
    bool overrideConditionMet = false;
    if (currClientDataPos < serverCount && expectedClientDataPos >= serverCount) {
      // Below variable is set to let the server know that a particular position
      // in the queue not yet accessed by it is going to be used
      *m_ipcChannel.clientDataExpectedPos = currClientDataPos - 1;
      overrideConditionMet = true;
    } else if (currClientDataPos > serverCount && expectedClientDataPos >= serverCount && expectedClientDataPos < currClientDataPos) {
      // Below variable is set to let server know that a particular position
      // in the queue not yet accessed by it is going to be used
      *m_ipcChannel.clientDataExpectedPos = expectedClientDataPos;
      overrideConditionMet = true;
    }

    if (overrideConditionMet) {
      Logger::warn("Data Queue override condition triggered");
      if (s_inProgress && m_batchStartPos <= *m_ipcChannel.clientDataExpectedPos) {
        Logger::err("Command's data batch size is too large and override could not be prevented!");
        *m_ipcChannel.clientDataExpectedPos = -1;
        *m_ipcChannel.serverResetPosRequired = false;
        return;
      }
      // Wait for the server to access the data at the above postion
      const auto maxRetries = GlobalOptions::getCommandRetries();
      size_t numRetries = 0;
      while (RESULT_FAILURE(m_ipcChannel.dataSemaphore->wait()) && numRetries++ < maxRetries) {
        Logger::warn("Waiting on server to process enough data from data queue to prevent override...");
      }
      if (numRetries >= maxRetries) {
        Logger::err("Max retries reached waiting on the server to process enough data to prevent a override!");
      }
      *m_ipcChannel.clientDataExpectedPos = -1;
      *m_ipcChannel.serverResetPosRequired = false;
      Logger::info("DataQueue override condition resolved");
    }
  }

  inline void send_data(const T obj) {
    ZoneScoped;
    if (gbBridgeRunning) {
      syncDataQueue(1, false);
      const auto result = m_ipcChannel.data->push(obj);
      if (RESULT_FAILURE(result)) {
        // For now just log when things go wrong, but could use some robustness improvements
        Logger::err("DataQueue send_data: Failed to send data!");
      }
    }
  }

  inline void send_data(const T size, const void* obj) {
    ZoneScoped;
    if (gbBridgeRunning) {
      size_t memUsed = align<size_t>(size, sizeof(T)) / sizeof(T);
      syncDataQueue(memUsed, true);
      const auto result = m_ipcChannel.data->push(size, obj);
      if (RESULT_FAILURE(result)) {
        // For now just log when things go wrong, but could use some robustness improvements
        Logger::err("DataQueue send_data: Failed to send data object!");
      }
    }
  }

  template<typename... Ts>
  inline void send_many(const Ts... objs) {
    ZoneScoped;
    if (gbBridgeRunning) {
      size_t count = sizeof...(Ts);
      syncDataQueue(count, false);
      const auto result = m_ipcChannel.data->push_many(objs...);
      if (RESULT_FAILURE(result)) {
        // For now just log when things go wrong, but could use some robustness improvements
        Logger::err("DataQueue send_many: Failed to send multiple data items!");
      }
    }
  }

  template<typename DataType>
  DataType* begin_data_blob(const T size) {
    ZoneScoped;
    DataType* blobPacketPtr = nullptr;
    if (gbBridgeRunning) {
      size_t memUsed = align<size_t>(size, sizeof(T)) / sizeof(T);
      syncDataQueue(memUsed, true);
      const auto result = m_ipcChannel.data->begin_blob_push(size, blobPacketPtr);
      if (RESULT_FAILURE(result)) {
        // For now just log when things go wrong, but could use some robustness improvements
        Logger::err("DataQueue begin_data_blob: Failed to begin sending a data blob!");
      }
    }
    return blobPacketPtr;
  }

  void end_data_blob() const {
    ZoneScoped;
    if (gbBridgeRunning) {
      m_ipcChannel.data->end_blob_push();
    }
  }

  /*
   * Static functions
   */

  static DWORD get_default_timeout() {
    const auto timeout = GlobalOptions::getCommandTimeout();
    const auto retries = GlobalOptions::getCommandRetries();
    const auto default = timeout * retries;
    // Catch overflow and return infinite in that case
    return ((timeout != 0) && (default / timeout != retries)) ? INFINITE : default;
  }

  static const T& get_data(const IpcChannel& ipcChannel) {
    ZoneScoped;
    size_t prevPos = get_data_pos(*ipcChannel.data);
    const T& retval = ipcChannel.data->pull();
    // Check if the server completed a loop
    if (*ipcChannel.serverResetPosRequired && get_data_pos(*ipcChannel.data) < prevPos) {
      *ipcChannel.serverResetPosRequired = false;
    }
    return retval;
  }

  static const T& get_data(const IpcChannel& ipcChannel, void** obj) {
    ZoneScoped;
    size_t prevPos = get_data_pos(*ipcChannel.data);
    const T& retval = ipcChannel.data->pull(obj);
    // Check if the server completed a loop
    if (*ipcChannel.serverResetPosRequired && get_data_pos(*ipcChannel.data) < prevPos) {
      *ipcChannel.serverResetPosRequired = false;
    }
    return retval;
  }

  template<typename V>
  static const T& copy_data(const IpcChannel& ipcChannel, V& obj, bool checkSize = true) {
    ZoneScoped;
    size_t prevPos = get_data_pos(*ipcChannel.data);
    const T& retval = ipcChannel.data->pull_and_copy(obj);

    if (checkSize) {
      assert((size_t) retval == sizeof(V) && "Size of source and target object does not match!");
      if ((size_t) retval != sizeof(V)) {
        Logger::err("DataQueue copy data: Size of source and target object does not match!");
      }
    }

    // Check if the server completed a loop
    if (*ipcChannel.serverResetPosRequired && get_data_pos(*ipcChannel.data) < prevPos) {
      *ipcChannel.serverResetPosRequired = false;
    }
    return retval;
  }

  static Header pop_front(bridge_util::CommandQueue& commandQueue) {
    ZoneScoped;
    Result result;
    // No retries, but wait the same amount of time
    const auto response = commandQueue.pull(result, get_default_timeout());
    if (RESULT_FAILURE(result)) {
      // For now just log when things go wrong, but could use some robustness improvements
      Logger::err("CommandQueue get_response: Failed to retrieve the command response!");
    }
    return response;
  }

  static bridge_util::Result ensureQueueEmpty(const bridge_util::CommandQueue& commandQueue) {
    if (commandQueue.isEmpty()) {
      return bridge_util::Result::Success;
    }

    const uint32_t maxAttempts = GlobalOptions::getCommandRetries();
    uint32_t attemptNum = 0;
    do {
      Result result;
      commandQueue.peek(result, 1);

      if (result != Result::Timeout) {
        // Give the server some time to process the commands
        Sleep(8);
      } else {
        // Timeout from peek() means the queue is empty
        return bridge_util::Result::Success;
      }
    } while (attemptNum++ <= maxAttempts && gbBridgeRunning);

    return bridge_util::Result::Timeout;
  }

  // Waits for a command to appear in the command queue. Upon success the command will NOT be removed from queue
  // and client MUST pull the command header manually by using pop_front() or pull() methods. The queue will enter
  // into an unrecoverable state otherwise.
  static bridge_util::Result waitForCommand(const bridge_util::CommandQueue& commandQueue, const Commands::D3D9Command& command = Commands::Bridge_Any, DWORD overrideTimeoutMS = 0) {
    ZoneScoped;
    DWORD peekTimeoutMS = overrideTimeoutMS > 0 ? overrideTimeoutMS : GlobalOptions::getCommandTimeout();
    uint32_t maxAttempts = GlobalOptions::getCommandRetries();
#ifdef ENABLE_WAIT_FOR_COMMAND_TRACE
    if (command != Commands::Any) {
      Logger::trace(format_string("Waiting for command %s for %d ms up to %d times...", Commands::toString(command).c_str(), peekTimeoutMS, maxAttempts));
    }
#endif
    bool inifiniteRetries = false;
    uint32_t attemptNum = 0;
    while (attemptNum++ <= maxAttempts && gbBridgeRunning) {
      Result result;
      Header header = commandQueue.peek(result, peekTimeoutMS);

      switch (result) {

      case Result::Success:
      {
        if (RESULT_SUCCESS(result) && (command == Commands::Bridge_Any || header.command == command)) {
#ifdef ENABLE_WAIT_FOR_COMMAND_TRACE
          if (command != Commands::Bridge_Any) {
            Logger::trace(format_string("...success, command %s received!", Commands::toString(command).c_str()));
          }
#endif
          return Result::Success;
        } else if (RESULT_FAILURE(result)) {
          Logger::err("Failed to peek command header!");
          return result;
        } else {
          Logger::debug(format_string("Wrong command detected: %s. Expected: %s.", Commands::toString(header.command).c_str(), Commands::toString(command).c_str()));
          // If we see the incorrect command, we want to give the other side of
          // the bridge ample time to make an attempt to process it first
          Sleep(peekTimeoutMS);
        }
        break;
      }

      case Result::Timeout:
      {
        if (GlobalOptions::getInfiniteRetries()) {
          // Infinite retries requested, the application might be alt-tabbed and sleeping, and so we need to wait too.

          // Set timeout for consecutive peeks to 1ms to relieve spin-waits.
          peekTimeoutMS = 1;
          // Decrease the attempt counter so it won't overrun maxAttempts in case it did not capture infinite retries.
          --attemptNum;
          // Set the flag so that the consecutive peek 1ms-timeout would not generate a failure in case if
          // infinite retries are revoked in the process.
          inifiniteRetries = true;

          // Sleep for default OS period
          Sleep(1);
        } else if (inifiniteRetries) {
          // A timeout in infinite retries loop but infinite retries have been revoked (app restored from alt-tab).

          // Restore peek timeout interval and continue.
          peekTimeoutMS = overrideTimeoutMS > 0 ? overrideTimeoutMS : GlobalOptions::getCommandTimeout();
          // Drop the flag - we're in the normal loop now.
          inifiniteRetries = false;
        }
        Logger::trace(format_string("Peek timeout while waiting for command: %s.", Commands::toString(command).c_str()));
        break;
      }

      case Result::Failure:
      {
        Logger::trace(format_string("Peek failed while waiting for command: %s.", Commands::toString(command).c_str()));
        return Result::Failure;
      }

      }
    }
    return Result::Timeout;
  }

  // Waits for a command to appear in the command queue. Upon success the command will be removed from the queue
  // and discarded.
  static bridge_util::Result waitForCommandAndDiscard(bridge_util::CommandQueue& commandQueue, const Commands::D3D9Command& command = Commands::Bridge_Any, DWORD overrideTimeoutMS = 0) {
    auto result = waitForCommand(commandQueue, command, overrideTimeoutMS);

    if (Result::Success == result) {
      pop_front(commandQueue);
    }

    return result;
  }

  static size_t get_data_pos(const bridge_util::DataQueue& dataQueue) {
    ZoneScoped;
    return dataQueue.get_pos();
  }

  static bridge_util::Result begin_batch(bridge_util::CommandQueue& commandQueue) {
    ZoneScoped;
#ifdef USE_BLOCKING_QUEUE
    if (gbBridgeRunning) {
      return commandQueue.begin_write_batch();
    }
#endif
    return bridge_util::Result::Failure;
  }

  static size_t end_batch(bridge_util::CommandQueue& commandQueue) {
    ZoneScoped;
#ifdef USE_BLOCKING_QUEUE
    if (gbBridgeRunning) {
      return commandQueue.end_write_batch();
    }
#endif
    return 0;
  }

  static bridge_util::Result begin_read_data(bridge_util::DataQueue& dataQueue) {
    ZoneScoped;
    if (gbBridgeRunning) {
      return dataQueue.begin_batch();
    }
    return bridge_util::Result::Failure;
  }

  static size_t end_read_data(bridge_util::DataQueue& dataQueue) {
    ZoneScoped;
    if (gbBridgeRunning) {
      return dataQueue.end_batch();
    }
    return 0;
  }

  static size_t get_counter() {
    return s_counter;
  }

  static void reset_counter() {
    s_counter = 0;
  }

private:
  const IpcChannel& m_ipcChannel;
  const Commands::D3D9Command m_command;
  const Commands::Flags m_commandFlags;
  const uint32_t m_handle;
  int32_t m_batchStartPos;
  inline static bool s_inProgress = false;

  inline static size_t s_counter = 0;

  BridgeCommand(const BridgeCommand&) = delete;
  BridgeCommand(BridgeCommand&&) = delete;
};

#endif // UTIL_BRIDGECOMMAND_H_
