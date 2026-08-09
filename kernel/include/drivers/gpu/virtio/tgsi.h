#ifndef _VIRTIO_GPU_TGSI
#define _VIRTIO_GPU_TGSI
#include "subsystem/gpu.h"
#include "drivers/gpu/virtio/virtio.h"
#define VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_INVALID (0x00)
#define VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_DECLARE (0x01)
#define VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE (0x02)
typedef int(*virtioGpuShaderInstructionFunc)(struct gpu_instruction_info* pInstructionInfo, struct virtio_gpu_shader_data_info* pShaderDataInfo);
int virtio_gpu_tgsi_shader_type_get_name(uint8_t shaderType, const unsigned char** ppShaderTypeName);
int virtio_gpu_tgsi_tag_type_get_name(uint8_t tagType, const unsigned char** ppTagTypeName);
int virtio_gpu_tgsi_declare_type_get_name(uint8_t declareType, const unsigned char** ppDeclareTypeName);
int virtio_gpu_tgsi_method_type_get_name(uint8_t methodType, const unsigned char** ppMethodTypeName);
int virtio_gpu_tgsi_get_current_shader_data(struct virtio_gpu_shader_data_info* pShaderDataInfo, uint8_t shaderDataType, unsigned char** ppShaderData);
int virtio_gpu_tgsi_get_current_shader_data_size(struct virtio_gpu_shader_data_info* pShaderDataInfo, uint8_t shaderDataType, uint64_t* pShaderDataSize);
int virtio_gpu_tgsi_shader_data_push(struct virtio_gpu_shader_data_info* pShaderDataInfo, uint8_t shaderDataType, uint64_t shaderDataLength);
int virtio_gpu_tgsi_shader_data_push_string(struct virtio_gpu_shader_data_info* pShaderDataInfo, uint8_t shaderDataType, unsigned char* pData, uint64_t dataLength);
int virtio_gpu_tgsi_shader_data_push_u64(struct virtio_gpu_shader_data_info* pShaderDataInfo, uint8_t shaderDataType, uint64_t value);
int virtio_gpu_tgsi_shader_data_push_i64(struct virtio_gpu_shader_data_info* pShaderDataInfo, uint8_t shaderDataType, int64_t value);
int virtio_gpu_tgsi_shader_data_push_tag_list(struct virtio_gpu_shader_data_info* pShaderDataInfo, uint8_t shaderDataType, struct gpu_tag_list_info tagListInfo);
int virtio_gpu_tgsi_shader_data_push_declare(struct virtio_gpu_shader_data_info* pShaderDataInfo, uint8_t shaderDataType, struct gpu_declare_location_info declareLocationInfo, uint64_t vectorCount);
int virtio_gpu_tgsi_shader_data_push_swizzle(struct virtio_gpu_shader_data_info* pShaderDataInfo, uint8_t shaderDataType, struct gpu_swizzle swizzle, uint8_t operandIndex);
int virtio_gpu_tgsi_shader_instruction_declare(struct gpu_instruction_info* pInstructionInfo, struct virtio_gpu_shader_data_info* pShaderDataInfo);
int virtio_gpu_tgsi_shader_instruction_method_dp(struct gpu_instruction_info_method_dp* pInstructionInfo, struct virtio_gpu_shader_data_info* pShaderDataInfo);
int virtio_gpu_tgsi_shader_instruction_method_tex(struct gpu_instruction_info_method* pInstructionInfo, struct virtio_gpu_shader_data_info* pShaderDataInfo);
int virtio_gpu_tgsi_shader_instruction_method(struct gpu_instruction_info_method* pInstructionInfo, struct virtio_gpu_shader_data_info* pShaderDataInfo);
int virtio_gpu_tgsi_shader_instruction_kill(struct gpu_instruction_info_kill* pInstructionInfo, struct virtio_gpu_shader_data_info* pShaderDataInfo);
int virtio_gpu_tgsi_shader_instruction_if(struct gpu_instruction_info_if* pInstructionInfo, struct virtio_gpu_shader_data_info* pShaderDataInfo);
int virtio_gpu_tgsi_shader_instruction_else(struct gpu_instruction_info_else* pInstructionInfo, struct virtio_gpu_shader_data_info* pShaderDataInfo);
int virtio_gpu_tgsi_shader_instruction_endif(struct gpu_instruction_info_endif* pInstructionInfo, struct virtio_gpu_shader_data_info* pShaderDataInfo);
int virtio_gpu_tgsi_shader_translate(uint64_t gpuId, uint64_t contextId, uint64_t objectId, struct gpu_create_shader_object_info* pCreateObjectInfo, struct virtio_gpu_shader_data_info* pShaderDataInfo);
#endif
