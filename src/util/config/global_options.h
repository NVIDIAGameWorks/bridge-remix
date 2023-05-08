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

#include "config/config.h"
#include "log/log.h"
#include "util_bridgecommand.h"

#include <d3d9types.h>
#include <debugapi.h>
#include <vector>

class GlobalOptions {
  enum SharedHeapPolicy: uint32_t {
    Textures       = 1<<0,
    DynamicBuffers = 1<<1,
    StaticBuffers  = 1<<2,

    BuffersOnly    = DynamicBuffers | StaticBuffers,

    None           = 0,
    All            = Textures | DynamicBuffers | StaticBuffers,
  };

public:
  static void init() {
    get().initialize();
  }

  static uint32_t getClientChannelMemSize() {
    return get().clientChannelMemSize;
  }

  static uint32_t getClientCmdQueueSize() {
    return get().clientCmdQueueSize;
  }

  static uint32_t getClientCmdMemSize() {
    return sizeof(Header) * get().clientCmdQueueSize + bridge_util::CommandQueue::getExtraMemoryRequirements(); // Adding some padding to help with aligning atomics in cacheline.
  }

  static uint32_t getClientDataQueueSize() {
    return get().clientDataQueueSize;
  }

  static uint32_t getClientDataMemSize() {
    return get().clientChannelMemSize - getClientCmdMemSize();
  }

  static uint32_t getServerChannelMemSize() {
    return get().serverChannelMemSize;
  }

  static uint32_t getServerCmdQueueSize() {
    return get().serverCmdQueueSize;
  }

  static uint32_t getServerCmdMemSize() {
    return sizeof(Header) * get().serverCmdQueueSize + bridge_util::CommandQueue::getExtraMemoryRequirements(); // Adding some padding to help with aligning atomics in cacheline.
  }

  static uint32_t getServerDataQueueSize() {
    return get().serverDataQueueSize;
  }

  static uint32_t getServerDataMemSize() {
    return get().serverChannelMemSize - getServerCmdMemSize();
  }

  static bool getSendReadOnlyCalls() {
    return get().sendReadOnlyCalls;
  }

  static bool getSendAllServerResponses() {
    return get().sendAllServerResponses;
  }

  static bool getLogAllCalls() {
    return get().logAllCalls;
  }

  static uint32_t getCommandTimeout() {
#ifdef _DEBUG
    return (get().disableTimeouts || (IsDebuggerPresent() && get().disableTimeoutsWhenDebugging)) ? 0 : get().commandTimeout;
#else
    return get().disableTimeouts ? 0 : get().commandTimeout;
#endif
  }

  static uint32_t getStartupTimeout() {
#ifdef _DEBUG
    return (get().disableTimeouts || (IsDebuggerPresent() && get().disableTimeoutsWhenDebugging)) ? 0 : get().startupTimeout;
#else
    return get().disableTimeouts ? 0 : get().startupTimeout;
#endif
  }

  static uint32_t getAckTimeout() {
#ifdef _DEBUG
    return (get().disableTimeouts || (IsDebuggerPresent() && get().disableTimeoutsWhenDebugging)) ? 0 : get().ackTimeout;
#else
    return get().disableTimeouts ? 0 : get().ackTimeout;
#endif
  }

  static bool getDisableTimeouts() {
    return get().disableTimeouts;
  }
  static void setDisableTimeouts(bool disableTimeouts) {
    get().disableTimeouts = disableTimeouts;
  }

  static uint32_t getCommandRetries() {
    return get().infiniteRetries ? INFINITE : get().commandRetries;
  }

  static bool getInfiniteRetries() {
    return get().infiniteRetries;
  }
  static void setInfiniteRetries(bool infiniteRetries) {
    get().infiniteRetries = infiniteRetries;
  }

  static bridge_util::LogLevel getLogLevel() {
    return get().logLevel;
  }

  static uint16_t getKeyStateCircBufMaxSize() {
    return get().keyStateCircBufMaxSize;
  }

  static uint8_t getPresentSemaphoreMaxFrames() {
    return get().presentSemaphoreMaxFrames;
  }

  static bool getPresentSemaphoreEnabled() {
    return get().presentSemaphoreEnabled;
  }

  static bool getCommandBatchingEnabled() {
    return get().commandBatchingEnabled;
  }

  static bool getUseSharedHeap() {
    return get().useSharedHeap;
  }

  static bool getUseSharedHeapForTextures() {
    return (get().sharedHeapPolicy & SharedHeapPolicy::Textures) != 0;
  }

  static bool getUseSharedHeapForDynamicBuffers() {
    return (get().sharedHeapPolicy & SharedHeapPolicy::DynamicBuffers) != 0;
  }

  static bool getUseSharedHeapForStaticBuffers() {
    return (get().sharedHeapPolicy & SharedHeapPolicy::StaticBuffers) != 0;
  }

  static const uint32_t getSharedHeapDefaultSegmentSize() {
    return get().sharedHeapDefaultSegmentSize;
  }

  static const uint32_t getSharedHeapChunkSize() {
    return get().sharedHeapChunkSize;
  }

  static const uint32_t getSharedHeapFreeChunkWaitTimeout() {
    return get().sharedHeapFreeChunkWaitTimeout;
  }

  static const uint32_t getSemaphoreTimeout() {
    return get().commandTimeout;
  }

  static uint32_t getThreadSafetyPolicy() {
    return get().threadSafetyPolicy;
  }

  static const uint32_t getServerSyncFlags() {
    uint32_t flags = 0;
    // The order of these flags needs to be read out in the same order
    // or else we will end up with wrong settings.
    flags |= getDisableTimeouts() ? 1 : 0;
    flags |= getInfiniteRetries() ? 2 : 0;
    return flags;
  }

  static void applyServerSyncFlags(uint32_t flags) {
    // Using same flag order from list above
    setDisableTimeouts((flags & 1) == 1);
    setInfiniteRetries((flags & 2) == 2);
    bridge_util::Logger::debug(bridge_util::format_string("Global settings are being applied from flags value %d", flags));
  }

private:
  GlobalOptions() = default;

  void initialize() {
    // Default settings below
    // We only read the config values once from the config file and cache them in the
    // object so that it is transparent to the caller where the value is coming from.


    clientChannelMemSize = bridge_util::Config::getOption<uint32_t>("clientChannelMemSize", 1'024 * 1'024 * 96);
    clientCmdQueueSize = bridge_util::Config::getOption<uint32_t>("clientCmdQueueSize", 3'000);
    clientDataQueueSize = bridge_util::Config::getOption<uint32_t>("clientDataQueueSize", 3'000);

    serverChannelMemSize = bridge_util::Config::getOption<uint32_t>("serverChannelMemSize", 1'024 * 1'024 * 32);
    serverCmdQueueSize = bridge_util::Config::getOption<uint32_t>("serverCmdQueueSize", 10);
    serverDataQueueSize = bridge_util::Config::getOption<uint32_t>("serverDataQueueSize", 25);

    // Toggle this to also send read only calls to the server. This can be
    // useful for debugging to ensure the server side D3D is in the same state.
    sendReadOnlyCalls = bridge_util::Config::getOption<bool>("sendReadOnlyCalls", false);

    // In most cases it is only useful to log those D3D calls that have not been
    // implemented on the server side yet, but by toggling this you will get the
    // first usage of all D3D calls logged, including the implemented ones.
    logAllCalls = bridge_util::Config::getOption<bool>("logAllCalls", false);

    // These values strike a good balance between not waiting too long during the
    // handshake on startup, which we expect to be relatively quick, while still being
    // resilient enough against blips that can cause intermittent timeouts during
    // regular rendering due to texture loading or game blocking the render thread.
    commandTimeout = bridge_util::Config::getOption<uint32_t>("commandTimeout", 1'000);
    startupTimeout = bridge_util::Config::getOption<uint32_t>("startupTimeout", 100);
    commandRetries = bridge_util::Config::getOption<uint32_t>("commandRetries", 300);

    // The acknowledgement timeout is enforced at runtime on acknowledgement commands
    // like Ack and Continue to avoid hitting the long waits when an "unexpected"
    // command is picked up from the queue.
    ackTimeout = bridge_util::Config::getOption<uint32_t>("ackTimeout", 10);

    // If enabled sets the number of maximum retries for commands and semaphore wait
    // operations to INFINITE, therefore ensuring that even during long periods of
    // inactivity these calls won't time out.
    infiniteRetries = bridge_util::Config::getOption<bool>("infiniteRetries", false);

#ifdef _DEBUG
    const auto strLevel = bridge_util::Config::getOption<std::string>("logLevel", "Debug");
#else
    const auto strLevel = bridge_util::Config::getOption<std::string>("logLevel", "Info");
#endif
    logLevel = bridge_util::str_to_loglevel(strLevel);

    // We use a simple circular buffer to track user input state in order to send
    // it over the bridge for dxvk developer/user overlay manipulation. This sets
    // the max size of the circ buffer, which stores 2B elements. 100 is probably
    // overkill, but it's a fairly small cost.
    keyStateCircBufMaxSize = bridge_util::Config::getOption<uint16_t>("keyStateCircBufMaxSize", 100);

    // This is the maximum latency in number of frames the client can be ahead of the
    // server before it blocks and waits for the server to catch up. We want this value
    // to be rather small so the two processes don't get too far out of sync.
    presentSemaphoreMaxFrames = bridge_util::Config::getOption<uint8_t>("presentSemaphoreMaxFrames", 3);
    presentSemaphoreEnabled = bridge_util::Config::getOption<bool>("presentSemaphoreEnabled", true);

    // Toggles between waiting on and triggering the command queue semaphore for each
    // command separately when batching is off compared to waiting for it only once per
    // frame, used in conjunction with the Present semaphore above. Fewer semaphore
    // calls should give us better performance, so this is turned on by default.
    commandBatchingEnabled = bridge_util::Config::getOption<bool>("commandBatchingEnabled", false);

    // If this is enabled, timeouts will be set to their maximum value (INFINITE which is the max uint32_t) 
    // and retries will be set to 1 while the application is being launched with or attached to by a debugger
    disableTimeoutsWhenDebugging = bridge_util::Config::getOption<bool>("disableTimeoutsWhenDebugging", false);

    // Behaves the same as disableTimeoutsWhenDebugging, except that it does not require a debugger to be
    // attached. This is used to cover certain scenarios where an inactive game window may be running in
    // the background without actively rendering any frames for an undetermined amount of time.
    disableTimeouts = bridge_util::Config::getOption<bool>("disableTimeouts", false);

    // Rather than copying an entire index/vertex/etc. buffer on every buffer-type Unlock(), the bridge instead
    // directly stores all buffer data into a shared memory "heap" that both Client and Server are able to
    // access, providing a significant speed boost. Downside: Server/DXVK crashes are currently not recoverable.
    useSharedHeap = bridge_util::Config::getOption<bool>("useSharedHeap", true);

    initSharedHeapPolicy();

    // The SharedHeap is actually divvied up into multiple "segments":shared memory file mappings
    // This is that unit size
    static constexpr uint32_t kDefaultSharedHeapSegmentSize = 128 << 20; // 128MB
    sharedHeapDefaultSegmentSize = bridge_util::Config::getOption<uint32_t>("sharedHeapDefaultSegmentSize", kDefaultSharedHeapSegmentSize);

    // "shared heap chunk" size. Fundamental allocation unit size.
    static constexpr uint32_t kDefaultSharedHeapChunkSize = 4 << 10; // 4kB
    sharedHeapChunkSize = bridge_util::Config::getOption<uint32_t>("sharedHeapChunkSize", kDefaultSharedHeapChunkSize);

    // The number of seconds to wait for a avaliable chunk to free up in the shared heap
    sharedHeapFreeChunkWaitTimeout = bridge_util::Config::getOption<uint32_t>("sharedHeapFreeChunkWaitTimeout", 10);

    // Thread-safety policy: 0 - use client's choice, 1 - force thread-safe, 2 - force non-thread-safe
    threadSafetyPolicy = bridge_util::Config::getOption<uint32_t>("threadSafetyPolicy", 0);
  }

  void initSharedHeapPolicy();

  static GlobalOptions& get() {
    static GlobalOptions instance;
    return instance;
  }

  static GlobalOptions instance;

  uint32_t clientChannelMemSize;
  uint32_t clientCmdQueueSize;
  uint32_t clientDataQueueSize;
  uint32_t serverChannelMemSize;
  uint32_t serverCmdQueueSize;
  uint32_t serverDataQueueSize;
  bool sendReadOnlyCalls;
  bool sendAllServerResponses;
  bool logAllCalls;
  uint32_t commandTimeout;
  uint32_t startupTimeout;
  uint32_t ackTimeout;
  uint32_t commandRetries;
  bool infiniteRetries;
  bridge_util::LogLevel logLevel;
  uint16_t keyStateCircBufMaxSize;
  uint8_t presentSemaphoreMaxFrames;
  bool presentSemaphoreEnabled;
  bool commandBatchingEnabled;
  bool disableTimeoutsWhenDebugging;
  bool disableTimeouts;
  bool useSharedHeap;
  uint32_t sharedHeapPolicy;
  uint32_t sharedHeapSize;
  uint32_t sharedHeapDefaultSegmentSize;
  uint32_t sharedHeapChunkSize;
  uint32_t sharedHeapFreeChunkWaitTimeout;
  uint32_t threadSafetyPolicy;
};
