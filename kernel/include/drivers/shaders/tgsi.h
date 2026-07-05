#ifndef _TGSI_DRIVER
#define _TGSI_DRIVER
#include "subsystem/subsystem.h"
#include "subsystem/gpu.h"
#define TGSI_DRIVER_IDENT {0x6D, 0xF8, 0x56, 0x0B,  0xBC, 0xEE, 0x2F, 0xF6,  0x6F, 0x4C, 0xF4, 0x29,  0xA6, 0xEB, 0x5B, 0x00}
struct tgsi_driver_shader_desc{
	unsigned char* pShaderCode;
	uint64_t shaderCodeSize;
	uint64_t shaderType;
	uint64_t shaderCodeOffset;
};
struct tgsi_driver_info{
	uint64_t driverId;
	struct gpu_shader_driver_vtable vtable;
};
int tgsi_driver_init(void);
int tgsi_driver_deinit(void);
int tgsi_driver_instruction_list_reset(struct tgsi_driver_shader_desc* pShaderDesc);
int tgsi_driver_subsystem_driver_init(uint64_t driverId);
int tgsi_driver_subsystem_driver_deinit(uint64_t driverId);
int tgsi_driver_subsystem_shader_init(uint64_t gpuId, uint64_t contextId, uint64_t objectId, struct gpu_create_shader_object_info* pCreateObjectInfo);
int tgsi_driver_subsystem_shader_deinit(uint64_t gpuId, uint64_t contextId, uint64_t objectId);
int tgsi_driver_subsystem_instruction_list_reset(uint64_t gpuId, uint64_t contextId, uint64_t objectId);
int tgsi_driver_subsystem_instruction_get_info(uint64_t gpuId, uint64_t contextId, uint64_t objectId, struct gpu_get_instruction_info* pGetInstructionInfo);
#endif
