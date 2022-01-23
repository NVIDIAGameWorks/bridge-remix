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
#pragma once

#include "util_common.h"
#include "base.h"

#include <d3d9.h>

class CommonShader {
  DWORD getShaderInstructionSize(const DWORD* pTokens) {
    const DWORD* pStart = pTokens;

    switch ((*pTokens) & D3DSI_OPCODE_MASK) {
    case D3DSIO_COMMENT:
      return ((*pTokens) & D3DSI_COMMENTSIZE_MASK) >> D3DSI_COMMENTSIZE_SHIFT;
    case D3DSIO_END:
      return 0;
    default:
      ++pTokens;
      while ((*pTokens) & 0x80000000) {
        ++pTokens;
      }
      return (pTokens - pStart);
    }
  }

  size_t getShaderByteSize(const DWORD* pTokens) {
    const DWORD* pStart = pTokens;
    while (((*pTokens) & D3DSI_OPCODE_MASK) != D3DSIO_END) {
      pTokens += getShaderInstructionSize(pTokens);
    }
    return ((pTokens - pStart) + 1) * sizeof(DWORD);
  }

  size_t analyze(const DWORD* pFunction) {
    m_majorVersion = D3DSHADER_VERSION_MAJOR(*pFunction);
    m_minorVersion = D3DSHADER_VERSION_MINOR(*pFunction);

    return getShaderByteSize(pFunction);
  }

  std::vector<uint8_t> m_code;

  uint32_t m_majorVersion = -1;
  uint32_t m_minorVersion = -1;

public:
  CommonShader(const DWORD* pFunction) {
    const size_t size = analyze(pFunction);

    m_code.resize(size);
    memcpy(m_code.data(), pFunction, m_code.size());
  }

  const DWORD* getCode() const {
    return (DWORD*) m_code.data();
  }

  const size_t getSize() const {
    return m_code.size();
  }

  const uint32_t getMajorVersion() const {
    return m_majorVersion;
  }

  const uint32_t getMinorVersion() const {
    return m_minorVersion;
  }
};