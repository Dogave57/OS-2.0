#ifndef _VIRTIO_GPU_TGSI
#define _VIRTIO_GPU_TGSI
#include "subsystem/gpu.h"
#include "drivers/gpu/virtio/virtio.h"
typedef int(*virtioGpuShaderInstructionFunc)(struct gpu_instruction_info* pInstructionInfo, struct virtio_gpu_shader_code_info* pShaderCodeInfo);
int virtio_gpu_tgsi_shader_type_get_name(uint8_t shaderType, const unsigned char** ppShaderTypeName);
int virtio_gpu_tgsi_tag_type_get_name(uint8_t tagType, const unsigned char** ppTagTypeName);
int virtio_gpu_tgsi_declare_type_get_name(uint8_t declareType, const unsigned char** ppDeclareTypeName);
int virtio_gpu_tgsi_get_current_shader_code(struct virtio_gpu_shader_code_info* pShaderCodeInfo, unsigned char** ppShaderCode);
int virtio_gpu_tgsi_shader_code_push_string(struct virtio_gpu_shader_code_info* pShaderCodeInfo, unsigned char* pData, uint64_t dataLength);
int virtio_gpu_tgsi_shader_code_push_u64(struct virtio_gpu_shader_code_info* pShaderCodeInfo, uint64_t value);
int virtio_gpu_tgsi_shader_code_push_i64(struct virtio_gpu_shader_code_info* pShaderCodeInfo, int64_t value);
int virtio_gpu_tgsi_shader_code_push_tag_list(struct virtio_gpu_shader_code_info* pShaderCodeInfo, struct gpu_tag_list_info tagListInfo);
int virtio_gpu_tgsi_shader_code_push_declare(struct virtio_gpu_shader_code_info* pShaderCodeInfo, struct gpu_declare_location_info declareLocationInfo);
int virtio_gpu_tgsi_shader_code_push_swizzle(struct virtio_gpu_shader_code_info* pShaderCodeInfo, struct gpu_swizzle swizzle, uint8_t operandIndex);
int virtio_gpu_tgsi_shader_instruction_declare(struct gpu_instruction_info* pInstructionInfo, struct virtio_gpu_shader_code_info* pShaderCodeInfo);
int virtio_gpu_tgsi_shader_instruction_add(struct gpu_instruction_info_add* pInstructionInfo, struct virtio_gpu_shader_code_info* pShaderCodeInfo);
int virtio_gpu_tgsi_shader_instruction_sub(struct gpu_instruction_info_sub* pInstructionInfo, struct virtio_gpu_shader_code_info* pShaderCodeInfo);
int virtio_gpu_tgsi_shader_instruction_mul(struct gpu_instruction_info_mul* pInstructionInfo, struct virtio_gpu_shader_code_info* pShaderCodeInfo);
int virtio_gpu_tgsi_shader_instruction_div(struct gpu_instruction_info_div* pInstructionInfo, struct virtio_gpu_shader_code_info* pShaderCodeInfo);
int virtio_gpu_tgsi_shader_translate(uint64_t gpuId, uint64_t contextId, uint64_t objectId, struct gpu_create_shader_object_info* pCreateObjectInfo, struct virtio_gpu_shader_code_info* pShaderCodeInfo);
#endif
