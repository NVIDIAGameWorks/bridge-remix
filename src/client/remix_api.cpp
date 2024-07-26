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

#include "log/log.h"
#include "util_bridgecommand.h"
#include "util_devicecommand.h"
#include "remix_api.h"


#define SEND_FLOAT(MSG, V) \
  (MSG).send_data((uint32_t) *(uint32_t*)(&(V)))

#define SEND_FLOAT2D(MSG, V) \
  SEND_FLOAT(MSG, (V).x); \
  SEND_FLOAT(MSG, (V).y)

#define SEND_FLOAT3D(MSG, V) \
  SEND_FLOAT(MSG, (V).x); \
  SEND_FLOAT(MSG, (V).y); \
  SEND_FLOAT(MSG, (V).z)

#define SEND_FLOAT4D(MSG, V) \
  SEND_FLOAT(MSG, (V).x); \
  SEND_FLOAT(MSG, (V).y); \
  SEND_FLOAT(MSG, (V).z); \
  SEND_FLOAT(MSG, (V).w)

#define SEND_INT(MSG, T) \
  (MSG).send_data((int32_t) (T))

#define SEND_STYPE(MSG, T) \
  (MSG).send_data((uint32_t) (T))

#define SEND_U32(MSG, U32) \
  (MSG).send_data((uint32_t) (U32))

#define SEND_U64(MSG, U64) \
  (MSG).send_data(sizeof(uint64_t), &(U64))

#define SEND_PATH(MSG, PATH) \
  (MSG).send_data((uint32_t)wcslen((PATH) ? (PATH) : L"") * sizeof(wchar_t), (PATH) ? (PATH) : L"")

#define PULL_DATA(SIZE, NAME) \
            uint32_t NAME##_len = DeviceBridge::get_data((void**)&(NAME)); \
            assert(NAME##_len == 0 || (SIZE) == NAME##_len)

namespace remix_api {
  bool interfaceInitialized = false;
  PFN_bridgeapi_RegisterEndSceneCallback interfaceGameCallback = nullptr;
}

namespace {
  void BRIDGEAPI_CALL bridgeapi_DebugPrint(const char* text) {
    if (text) {
      ClientMessage c(Commands::Api_DebugPrint);
      c.send_data((uint32_t) strlen(text), text);
    }
  }

  uint64_t BRIDGEAPI_CALL bridgeapi_CreateOpaqueMaterial(const x86::remixapi_MaterialInfo* info, const x86::remixapi_MaterialInfoOpaqueEXT* ext, const x86::remixapi_MaterialInfoOpaqueSubsurfaceEXT* ext_ss) {
    UID currentUID = 0;
    {
      ClientMessage c(Commands::Api_CreateOpaqueMaterial);
      currentUID = c.get_uid();

      // MaterialInfo
      SEND_STYPE(c, info->sType);
      SEND_U64(c, info->hash);
      SEND_PATH(c, info->albedoTexture);
      SEND_PATH(c, info->normalTexture);
      SEND_PATH(c, info->tangentTexture);
      SEND_PATH(c, info->emissiveTexture);
      SEND_FLOAT(c, info->emissiveIntensity);
      SEND_FLOAT3D(c, info->emissiveColorConstant);
      c.send_data((uint8_t) info->spriteSheetRow);
      c.send_data((uint8_t) info->spriteSheetCol);
      c.send_data((uint8_t) info->spriteSheetFps);
      c.send_data((uint8_t) info->filterMode);
      c.send_data((uint8_t) info->wrapModeU);
      c.send_data((uint8_t) info->wrapModeV);

      // MaterialInfoOpaqueEXT
      SEND_STYPE(c, ext->sType);
      SEND_PATH(c, ext->roughnessTexture);
      SEND_PATH(c, ext->metallicTexture);
      SEND_FLOAT(c, ext->anisotropy);
      SEND_FLOAT3D(c, ext->albedoConstant);
      SEND_FLOAT(c, ext->opacityConstant);
      SEND_FLOAT(c, ext->roughnessConstant);
      SEND_FLOAT(c, ext->metallicConstant);
      SEND_U32(c, ext->thinFilmThickness_hasvalue);
      SEND_FLOAT(c, ext->thinFilmThickness_value);
      SEND_U32(c, ext->alphaIsThinFilmThickness);
      SEND_PATH(c, ext->heightTexture);
      SEND_FLOAT(c, ext->heightTextureStrength);
      SEND_U32(c, ext->useDrawCallAlphaState); // If true, InstanceInfoBlendEXT is used as a source for alpha state
      SEND_U32(c, ext->blendType_hasvalue);
      SEND_INT(c, ext->blendType_value);
      SEND_U32(c, ext->invertedBlend);
      SEND_INT(c, ext->alphaTestType);
      c.send_data((uint8_t) ext->alphaReferenceValue);

      const x86::remixapi_Bool has_ss = ext_ss ? TRUE : FALSE;
      SEND_U32(c, has_ss);

      if (ext_ss) {
        // MaterialInfoOpaqueSubsurfaceEXT
        SEND_STYPE(c, ext_ss->sType);
        SEND_PATH(c, ext_ss->subsurfaceTransmittanceTexture);
        SEND_PATH(c, ext_ss->subsurfaceThicknessTexture);
        SEND_PATH(c, ext_ss->subsurfaceSingleScatteringAlbedoTexture);
        SEND_FLOAT3D(c, ext_ss->subsurfaceTransmittanceColor);
        SEND_FLOAT(c, ext_ss->subsurfaceMeasurementDistance);
        SEND_FLOAT3D(c, ext_ss->subsurfaceSingleScatteringAlbedo);
        SEND_FLOAT(c, ext_ss->subsurfaceVolumetricAnisotropy);
      }
    }

    WAIT_FOR_SERVER_RESPONSE("CreateMaterial()", 0, currentUID);
    uint64_t* result = nullptr;
    PULL_DATA(sizeof(uint64_t), result);
    DeviceBridge::pop_front();
    return *result;
  }

  uint64_t BRIDGEAPI_CALL bridgeapi_CreateTranslucentMaterial(const x86::remixapi_MaterialInfo* info, const x86::remixapi_MaterialInfoTranslucentEXT* ext) {
    UID currentUID = 0;
    {
      ClientMessage c(Commands::Api_CreateTranslucentMaterial);
      currentUID = c.get_uid();

      // MaterialInfo
      SEND_STYPE(c, info->sType);
      SEND_U64(c, info->hash);
      SEND_PATH(c, info->albedoTexture);
      SEND_PATH(c, info->normalTexture);
      SEND_PATH(c, info->tangentTexture);
      SEND_PATH(c, info->emissiveTexture);
      SEND_FLOAT(c, info->emissiveIntensity);
      SEND_FLOAT3D(c, info->emissiveColorConstant);
      c.send_data((uint8_t) info->spriteSheetRow);
      c.send_data((uint8_t) info->spriteSheetCol);
      c.send_data((uint8_t) info->spriteSheetFps);
      c.send_data((uint8_t) info->filterMode);
      c.send_data((uint8_t) info->wrapModeU);
      c.send_data((uint8_t) info->wrapModeV);

      // MaterialInfoTranslucentEXT
      SEND_STYPE(c, ext->sType);
      SEND_PATH(c, ext->transmittanceTexture);
      SEND_FLOAT(c, ext->refractiveIndex);
      SEND_FLOAT3D(c, ext->transmittanceColor);
      SEND_FLOAT(c, ext->transmittanceMeasurementDistance);
      SEND_U32(c, ext->thinWallThickness_hasvalue);
      SEND_FLOAT(c, ext->thinWallThickness_value);
      SEND_U32(c, ext->useDiffuseLayer);
    }

    WAIT_FOR_SERVER_RESPONSE("CreateMaterial()", 0, currentUID);
    uint64_t* result = nullptr;
    PULL_DATA(sizeof(uint64_t), result);
    DeviceBridge::pop_front();
    return *result;
  }

  uint64_t BRIDGEAPI_CALL bridgeapi_CreatePortalMaterial(const x86::remixapi_MaterialInfo* info, const x86::remixapi_MaterialInfoPortalEXT* ext) {
    UID currentUID = 0;
    {
      ClientMessage c(Commands::Api_CreatePortalMaterial);
      currentUID = c.get_uid();

      // MaterialInfo
      SEND_STYPE(c, info->sType);
      SEND_U64(c, info->hash);
      SEND_PATH(c, info->albedoTexture);
      SEND_PATH(c, info->normalTexture);
      SEND_PATH(c, info->tangentTexture);
      SEND_PATH(c, info->emissiveTexture);
      SEND_FLOAT(c, info->emissiveIntensity);
      SEND_FLOAT3D(c, info->emissiveColorConstant);
      c.send_data((uint8_t) info->spriteSheetRow);
      c.send_data((uint8_t) info->spriteSheetCol);
      c.send_data((uint8_t) info->spriteSheetFps);
      c.send_data((uint8_t) info->filterMode);
      c.send_data((uint8_t) info->wrapModeU);
      c.send_data((uint8_t) info->wrapModeV);

      // MaterialInfoPortalEXT
      SEND_STYPE(c, ext->sType);
      c.send_data((uint8_t) ext->rayPortalIndex);
      SEND_FLOAT(c, ext->rotationSpeed);
    }

    WAIT_FOR_SERVER_RESPONSE("CreateMaterial()", 0, currentUID);
    uint64_t* result = nullptr;
    PULL_DATA(sizeof(uint64_t), result);
    DeviceBridge::pop_front();
    return *result;
  }

  void BRIDGEAPI_CALL bridgeapi_DestroyMaterial(uint64_t handle) {
    ClientMessage c(Commands::Api_DestroyMaterial);
    SEND_U64(c, handle);
  }

  uint64_t BRIDGEAPI_CALL bridgeapi_CreateTriangleMesh(const x86::remixapi_MeshInfo* info) {
    UID currentUID = 0;
    {
      ClientMessage c(Commands::Api_CreateTriangleMesh);
      currentUID = c.get_uid();

      // MeshInfo
      SEND_STYPE(c, info->sType);
      SEND_U64(c, info->hash);

      // send each surface
      SEND_U32(c, info->surfaces_count); // send surface count before sending the surfaces
      for (uint32_t s = 0u; s < info->surfaces_count; s++) 
      {
        const auto& surf = info->surfaces_values[s];
       
        // send vertices of the current surface
        SEND_U64(c, surf.vertices_count); // send vertex count before vertices
        for (uint64_t v = 0u; v < surf.vertices_count; v++)
        {
          const auto& vert = surf.vertices_values[v];
          SEND_FLOAT(c, vert.position[0]);
          SEND_FLOAT(c, vert.position[1]);
          SEND_FLOAT(c, vert.position[2]);
          SEND_FLOAT(c, vert.normal[0]);
          SEND_FLOAT(c, vert.normal[1]);
          SEND_FLOAT(c, vert.normal[2]);
          SEND_FLOAT(c, vert.texcoord[0]);
          SEND_FLOAT(c, vert.texcoord[1]);
          SEND_U32(c, vert.color);
        }

        // send indices of the current surface
        SEND_U64(c, surf.indices_count); // send index count before indices
        for (uint64_t i = 0u; i < surf.indices_count; i++) {
          SEND_U32(c, surf.indices_values[i]);
        }

        SEND_U32(c, surf.skinning_hasvalue);
        // # TODO skinning

        // using remixapi_MaterialHandle is unpractical and kinda unsafe because its only 4 bytes <here> (ptr)
        // so user would have to send an actual pointer instead of the uint64_t hash val 
        SEND_U64(c, surf.material);
      }
    }

    WAIT_FOR_SERVER_RESPONSE("CreateMesh()", 0, currentUID);
    uint64_t* result = nullptr;
    PULL_DATA(sizeof(uint64_t), result);
    DeviceBridge::pop_front();
    return *result;
  }

  void BRIDGEAPI_CALL bridgeapi_DestroyMesh(uint64_t handle) {
    ClientMessage c(Commands::Api_DestroyMesh);
    SEND_U64(c, handle);
  }

  void BRIDGEAPI_CALL bridgeapi_DrawMeshInstance(uint64_t handle, const x86::remixapi_Transform* t, x86::remixapi_Bool double_sided) {
    ClientMessage c(Commands::Api_DrawMeshInstance);
    SEND_U64(c, handle);
    SEND_FLOAT(c, t->matrix[0][0]); SEND_FLOAT(c, t->matrix[0][1]); SEND_FLOAT(c, t->matrix[0][2]); SEND_FLOAT(c, t->matrix[0][3]);
    SEND_FLOAT(c, t->matrix[1][0]); SEND_FLOAT(c, t->matrix[1][1]); SEND_FLOAT(c, t->matrix[1][2]); SEND_FLOAT(c, t->matrix[1][3]);
    SEND_FLOAT(c, t->matrix[2][0]); SEND_FLOAT(c, t->matrix[2][1]); SEND_FLOAT(c, t->matrix[2][2]); SEND_FLOAT(c, t->matrix[2][3]);
    SEND_U32(c, double_sided);
  }

  uint64_t BRIDGEAPI_CALL bridgeapi_CreateSphereLight(const x86::remixapi_LightInfo* info, const x86::remixapi_LightInfoSphereEXT* ext) {
    UID currentUID = 0;
    {
      ClientMessage c(Commands::Api_CreateSphereLight);
      currentUID = c.get_uid();

      // LightInfo
      SEND_STYPE(c, info->sType);
      SEND_U64(c, info->hash);
      SEND_FLOAT3D(c, info->radiance);

      // LightInfoSphereEXT
      SEND_STYPE(c, ext->sType);
      SEND_FLOAT3D(c, ext->position);
      SEND_FLOAT(c, ext->radius);
      SEND_U32(c, ext->shaping_hasvalue);

      if (ext->shaping_hasvalue) {
        SEND_FLOAT3D(c, ext->shaping_value.direction);
        SEND_FLOAT(c, ext->shaping_value.coneAngleDegrees);
        SEND_FLOAT(c, ext->shaping_value.coneSoftness);
        SEND_FLOAT(c, ext->shaping_value.focusExponent);
      }
    }

    WAIT_FOR_SERVER_RESPONSE("CreateLight()", 0, currentUID);
    uint64_t* result = nullptr;
    PULL_DATA(sizeof(uint64_t), result);
    DeviceBridge::pop_front();
    return *result;
  }

  uint64_t BRIDGEAPI_CALL bridgeapi_CreateRectLight(const x86::remixapi_LightInfo* info, const x86::remixapi_LightInfoRectEXT* ext) {
    UID currentUID = 0;
    {
      ClientMessage c(Commands::Api_CreateRectLight);
      currentUID = c.get_uid();

      // LightInfo
      SEND_STYPE(c, info->sType);
      SEND_U64(c, info->hash);
      SEND_FLOAT3D(c, info->radiance);

      // LightInfoRectEXT
      SEND_STYPE(c, ext->sType);
      SEND_FLOAT3D(c, ext->position);
      SEND_FLOAT3D(c, ext->xAxis);
      SEND_FLOAT(c, ext->xSize);
      SEND_FLOAT3D(c, ext->yAxis);
      SEND_FLOAT(c, ext->ySize);
      SEND_FLOAT3D(c, ext->direction);
      SEND_U32(c, ext->shaping_hasvalue);

      if (ext->shaping_hasvalue) {
        SEND_FLOAT3D(c, ext->shaping_value.direction);
        SEND_FLOAT(c, ext->shaping_value.coneAngleDegrees);
        SEND_FLOAT(c, ext->shaping_value.coneSoftness);
        SEND_FLOAT(c, ext->shaping_value.focusExponent);
      }
    }

    WAIT_FOR_SERVER_RESPONSE("CreateLight()", 0, currentUID);
    uint64_t* result = nullptr;
    PULL_DATA(sizeof(uint64_t), result);
    DeviceBridge::pop_front();
    return *result;
  }

  uint64_t BRIDGEAPI_CALL bridgeapi_CreateDiscLight(const x86::remixapi_LightInfo* info, const x86::remixapi_LightInfoDiskEXT* ext) {
    UID currentUID = 0;
    {
      ClientMessage c(Commands::Api_CreateDiskLight);
      currentUID = c.get_uid();

      // LightInfo
      SEND_STYPE(c, info->sType);
      SEND_U64(c, info->hash);
      SEND_FLOAT3D(c, info->radiance);

      // LightInfoDiskEXT
      SEND_STYPE(c, ext->sType);
      SEND_FLOAT3D(c, ext->position);
      SEND_FLOAT3D(c, ext->xAxis);
      SEND_FLOAT(c, ext->xRadius);
      SEND_FLOAT3D(c, ext->yAxis);
      SEND_FLOAT(c, ext->yRadius);
      SEND_FLOAT3D(c, ext->direction);
      SEND_U32(c, ext->shaping_hasvalue);

      if (ext->shaping_hasvalue) {
        SEND_FLOAT3D(c, ext->shaping_value.direction);
        SEND_FLOAT(c, ext->shaping_value.coneAngleDegrees);
        SEND_FLOAT(c, ext->shaping_value.coneSoftness);
        SEND_FLOAT(c, ext->shaping_value.focusExponent);
      }
    }

    WAIT_FOR_SERVER_RESPONSE("CreateLight()", 0, currentUID);
    uint64_t* result = nullptr;
    PULL_DATA(sizeof(uint64_t), result);
    DeviceBridge::pop_front();
    return *result;
  }

  uint64_t BRIDGEAPI_CALL bridgeapi_CreateCylinderLight(const x86::remixapi_LightInfo* info, const x86::remixapi_LightInfoCylinderEXT* ext) {
    UID currentUID = 0;
    {
      ClientMessage c(Commands::Api_CreateCylinderLight);
      currentUID = c.get_uid();

      // LightInfo
      SEND_STYPE(c, info->sType);
      SEND_U64(c, info->hash);
      SEND_FLOAT3D(c, info->radiance);

      // LightInfoCylinderEXT
      SEND_STYPE(c, ext->sType);
      SEND_FLOAT3D(c, ext->position);
      SEND_FLOAT(c, ext->radius);
      SEND_FLOAT3D(c, ext->axis);
      SEND_FLOAT(c, ext->axisLength);
    }

    WAIT_FOR_SERVER_RESPONSE("CreateLight()", 0, currentUID);
    uint64_t* result = nullptr;
    PULL_DATA(sizeof(uint64_t), result);
    DeviceBridge::pop_front();
    return *result;
  }

  uint64_t BRIDGEAPI_CALL bridgeapi_CreateDistantLight(const x86::remixapi_LightInfo* info, const x86::remixapi_LightInfoDistantEXT* ext) {
    UID currentUID = 0;
    {
      ClientMessage c(Commands::Api_CreateDistantLight);
      currentUID = c.get_uid();

      // LightInfo
      SEND_STYPE(c, info->sType);
      SEND_U64(c, info->hash);
      SEND_FLOAT3D(c, info->radiance);

      // LightInfoDistantEXT
      SEND_STYPE(c, ext->sType);
      SEND_FLOAT3D(c, ext->direction);
      SEND_FLOAT(c, ext->angularDiameterDegrees);
    }

    WAIT_FOR_SERVER_RESPONSE("CreateLight()", 0, currentUID);
    uint64_t* result = nullptr;
    PULL_DATA(sizeof(uint64_t), result);
    DeviceBridge::pop_front();
    return *result;
  }

  void BRIDGEAPI_CALL bridgeapi_DestroyLight(uint64_t handle) {
    ClientMessage c(Commands::Api_DestroyLight);
    SEND_U64(c, handle);
  }

  void BRIDGEAPI_CALL bridgeapi_DrawLightInstance(uint64_t handle) {
    ClientMessage c(Commands::Api_DrawLightInstance);
    SEND_U64(c, handle);
  }

  void BRIDGEAPI_CALL bridgeapi_SetConfigVariable(const char* var, const char* value) {
    if (var && value)
    {
      ClientMessage c(Commands::Api_SetConfigVariable);
      c.send_data((uint32_t) strlen(var), var);
      c.send_data((uint32_t) strlen(value), value);
    }
  }

  void BRIDGEAPI_CALL bridgeapi_RegisterDevice() {
    ClientMessage c(Commands::Api_RegisterDevice);
  }

  BRIDGE_API void bridgeapi_RegisterEndSceneCallback(PFN_bridgeapi_RegisterEndSceneCallback callback) {
    remix_api::interfaceGameCallback = callback;
  }

  extern "C" {
    BRIDGE_API BRIDGEAPI_ErrorCode __cdecl bridgeapi_InitFuncs(bridgeapi_Interface* out_result) {
      if (!out_result) {
        return BRIDGEAPI_ERROR_CODE_INVALID_ARGUMENTS;
      }
      auto interf = bridgeapi_Interface {};
      {
        interf.DebugPrint = bridgeapi_DebugPrint;
        interf.CreateOpaqueMaterial = bridgeapi_CreateOpaqueMaterial;
        interf.CreateTranslucentMaterial = bridgeapi_CreateTranslucentMaterial;
        interf.CreatePortalMaterial = bridgeapi_CreatePortalMaterial;
        interf.DestroyMaterial = bridgeapi_DestroyMaterial;
        interf.CreateTriangleMesh = bridgeapi_CreateTriangleMesh;
        interf.DestroyMesh = bridgeapi_DestroyMesh;
        interf.DrawMeshInstance = bridgeapi_DrawMeshInstance;
        interf.CreateSphereLight = bridgeapi_CreateSphereLight;
        interf.CreateRectLight = bridgeapi_CreateRectLight;
        interf.CreateDiskLight = bridgeapi_CreateDiscLight;
        interf.CreateCylinderLight = bridgeapi_CreateCylinderLight;
        interf.CreateDistantLight = bridgeapi_CreateDistantLight;
        interf.DestroyLight = bridgeapi_DestroyLight;
        interf.DrawLightInstance = bridgeapi_DrawLightInstance;
        interf.SetConfigVariable = bridgeapi_SetConfigVariable;
        interf.RegisterDevice = bridgeapi_RegisterDevice;
        interf.RegisterEndSceneCallback = bridgeapi_RegisterEndSceneCallback;
      }

      *out_result = interf;
      remix_api::interfaceInitialized = true;

      return BRIDGEAPI_ERROR_CODE_SUCCESS;
    }
  }
}
