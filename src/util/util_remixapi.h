/*
 * Copyright (c) 2024, NVIDIA CORPORATION. All rights reserved.
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
#include "util_serializable.h"

#include <remixapi/bridge_remix_api.h>

#include <typeinfo>

#define ASSERT_REMIXAPI_PFN_TYPE(REMIXAPI_FN_NAME) static_assert(std::is_same_v< decltype(&REMIXAPI_FN_NAME), PFN_##REMIXAPI_FN_NAME >)

namespace remixapi {

namespace util {

struct HandleUID {
  const uint32_t uid = 0;
  HandleUID(const HandleUID& handle) = default;
  HandleUID(HandleUID&& handle) = default;
#ifdef REMIX_BRIDGE_CLIENT
  static inline uint32_t nextUid = 1;
  HandleUID() : uid(nextUid++) {}
#endif
  template<typename T>
  HandleUID(const T* const p) : uid((uint32_t)(uintptr_t)(p)) {
    static_assert(std::is_pointer_v<T*>);
  }
  template<typename T>
  operator T*() {
    static_assert(std::is_pointer_v<T*>);
    return reinterpret_cast<T*>((uintptr_t)uid);
  }
#ifdef REMIX_BRIDGE_SERVER
  HandleUID(const uint32_t val) : uid(val) {}
  operator uint32_t () {
    return uid;
  }
#endif
  bool isValid() const {
#ifdef REMIX_BRIDGE_CLIENT
    return uid > 0 && uid < nextUid;
#endif
#ifdef REMIX_BRIDGE_SERVER
    return uid > 0;
#endif
  }
};
static_assert(sizeof(HandleUID::uid) == sizeof(HandleUID));

struct AnyInfoPrototype {
  remixapi_StructType sType;
  void* pNext;
};

static inline remixapi_StructType getSType(const void* const pInfo) {
  if (pInfo) {
    return reinterpret_cast<const AnyInfoPrototype* const>(pInfo)->sType;
  }
  return REMIXAPI_STRUCT_TYPE_NONE;
}

static inline void* getPNext(const void* const pInfo) {
  if (pInfo) {
    return reinterpret_cast<const AnyInfoPrototype* const>(pInfo)->pNext;
  }
  return nullptr;
}

template<typename RemixApiT>
static inline AnyInfoPrototype& getInfoProto(RemixApiT& remixApiT) {
  return reinterpret_cast<AnyInfoPrototype&>(remixApiT);
}

template< typename T > constexpr remixapi_StructType ToRemixApiStructEnum                 = REMIXAPI_STRUCT_TYPE_NONE;
template<> constexpr auto ToRemixApiStructEnum< remixapi_MaterialInfo                   > = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;
template<> constexpr auto ToRemixApiStructEnum< remixapi_MaterialInfoPortalEXT          > = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_PORTAL_EXT;
template<> constexpr auto ToRemixApiStructEnum< remixapi_MaterialInfoTranslucentEXT     > = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_TRANSLUCENT_EXT;
template<> constexpr auto ToRemixApiStructEnum< remixapi_MaterialInfoOpaqueEXT          > = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;
template<> constexpr auto ToRemixApiStructEnum< remixapi_MaterialInfoOpaqueSubsurfaceEXT> = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_SUBSURFACE_EXT;
template<> constexpr auto ToRemixApiStructEnum< remixapi_LightInfoSphereEXT             > = REMIXAPI_STRUCT_TYPE_LIGHT_INFO_SPHERE_EXT;
template<> constexpr auto ToRemixApiStructEnum< remixapi_LightInfoRectEXT               > = REMIXAPI_STRUCT_TYPE_LIGHT_INFO_RECT_EXT;
template<> constexpr auto ToRemixApiStructEnum< remixapi_LightInfoDiskEXT               > = REMIXAPI_STRUCT_TYPE_LIGHT_INFO_DISK_EXT;
template<> constexpr auto ToRemixApiStructEnum< remixapi_LightInfoCylinderEXT           > = REMIXAPI_STRUCT_TYPE_LIGHT_INFO_CYLINDER_EXT;
template<> constexpr auto ToRemixApiStructEnum< remixapi_LightInfoDistantEXT            > = REMIXAPI_STRUCT_TYPE_LIGHT_INFO_DISTANT_EXT;
template<> constexpr auto ToRemixApiStructEnum< remixapi_LightInfoDomeEXT               > = REMIXAPI_STRUCT_TYPE_LIGHT_INFO_DOME_EXT;
template<> constexpr auto ToRemixApiStructEnum< remixapi_LightInfoUSDEXT                > = REMIXAPI_STRUCT_TYPE_LIGHT_INFO_USD_EXT;
template<> constexpr auto ToRemixApiStructEnum< remixapi_LightInfo                      > = REMIXAPI_STRUCT_TYPE_LIGHT_INFO;
template<> constexpr auto ToRemixApiStructEnum< remixapi_MeshInfo                       > = REMIXAPI_STRUCT_TYPE_MESH_INFO;
template<> constexpr auto ToRemixApiStructEnum< remixapi_InstanceInfo                   > = REMIXAPI_STRUCT_TYPE_INSTANCE_INFO;
template<> constexpr auto ToRemixApiStructEnum< remixapi_InstanceInfoBoneTransformsEXT  > = REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BONE_TRANSFORMS_EXT;
template<> constexpr auto ToRemixApiStructEnum< remixapi_InstanceInfoBlendEXT           > = REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BLEND_EXT;
template<> constexpr auto ToRemixApiStructEnum< remixapi_InstanceInfoObjectPickingEXT   > = REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_OBJECT_PICKING_EXT;
template<> constexpr auto ToRemixApiStructEnum< remixapi_CameraInfo                     > = REMIXAPI_STRUCT_TYPE_CAMERA_INFO;
template<> constexpr auto ToRemixApiStructEnum< remixapi_CameraInfoParameterizedEXT     > = REMIXAPI_STRUCT_TYPE_CAMERA_INFO_PARAMETERIZED_EXT;


namespace serialize {
// Type declaration

// MaterialInfo
using MaterialInfo = bridge_util::Serializable<remixapi_MaterialInfo,false>;
using MaterialInfoOpaque = bridge_util::Serializable<remixapi_MaterialInfoOpaqueEXT,false>;
using MaterialInfoOpaqueSubsurface = bridge_util::Serializable<remixapi_MaterialInfoOpaqueSubsurfaceEXT,false>;
using MaterialInfoTranslucent = bridge_util::Serializable<remixapi_MaterialInfoTranslucentEXT,false>;
using MaterialInfoPortal = bridge_util::Serializable<remixapi_MaterialInfoPortalEXT,true>;

// MeshInfo
using MeshInfo = bridge_util::Serializable<remixapi_MeshInfo,false>;

// InstanceInfo
using InstanceInfo = bridge_util::Serializable<remixapi_InstanceInfo,true>;
using InstanceInfoObjectPicking = bridge_util::Serializable<remixapi_InstanceInfoObjectPickingEXT,true>;
using InstanceInfoBlend = bridge_util::Serializable<remixapi_InstanceInfoBlendEXT,true>;
using InstanceInfoTransforms = bridge_util::Serializable<remixapi_InstanceInfoBoneTransformsEXT,false>;

// Light Info
using LightInfo = bridge_util::Serializable<remixapi_LightInfo,true>;
using LightInfoSphere = bridge_util::Serializable<remixapi_LightInfoSphereEXT,true>;
using LightInfoRect = bridge_util::Serializable<remixapi_LightInfoRectEXT,true>;
using LightInfoDisk = bridge_util::Serializable<remixapi_LightInfoDiskEXT,true>;
using LightInfoCylinder = bridge_util::Serializable<remixapi_LightInfoCylinderEXT,true>;
using LightInfoDistant = bridge_util::Serializable<remixapi_LightInfoDistantEXT,true>;
using LightInfoDome = bridge_util::Serializable<remixapi_LightInfoDomeEXT,false>;
using LightInfoUSD = bridge_util::Serializable<remixapi_LightInfoUSDEXT,false>;

}

}

}
