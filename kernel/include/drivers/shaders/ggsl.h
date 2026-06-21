#ifndef _GGSL_DRIVER
#define _GGSL_DRIVER
#include "subsystem/subsystem.h"
#include "subsystem/gpu.h"
#define GGSL_DRIVER_SIGNATURE ((uint32_t)'LSGG')
#define GGSL_DRIVER_IDENT {0xC4, 0x94, 0x36, 0xA2,  0x3F, 0x9E, 0x98, 0x96,  0x9E, 0x1D, 0x4B, 0x06,  0xFD, 0x85, 0x2E, 0x4E}
#define GGSL_DRIVER_MAX_IDENT_COUNT (65536)
#define GGSL_DRIVER_IDENT_LIST_COUNT (66536)
#define GGSL_DRIVER_IDENT_TYPE_INVALID (0x00)
#define GGSL_DRIVER_IDENT_TYPE_OPCODE (0x01)
#define GGSL_DRIVER_IDENT_TYPE_REFERENCE (0x02)
#define GGSL_DRIVER_IDENT_TYPE_SEPARATOR (0x03)
#define GGSL_DRIVER_IDENT_TYPE_TAG (0x04)

struct ggsl_driver_ident_info{
	uint32_t offset;
	uint16_t length;
	uint16_t identType;
	uint16_t hash;
	uint8_t reserved0[6];
}__attribute__((packed));
struct ggsl_driver_ident_desc{
	uint64_t identId;
	struct ggsl_driver_ident_info identInfo;
	struct ggsl_driver_ident_desc* pFlink;
	struct ggsl_driver_ident_desc* pBlink;
}__attribute__((packed));
struct ggsl_driver_ident_list_desc{
	struct ggsl_driver_ident_desc* pFirstIdentDesc;
	struct ggsl_driver_ident_desc* pLastIdentDesc;
}__attribute__((packed));
struct ggsl_driver_shader_desc{
	uint64_t shaderObjectId;
	unsigned char* pShaderCode;
	uint64_t shaderCodeSize;
	uint64_t shaderType;
	uint64_t shaderCodeOffset;
	struct subsystem_desc* pIdentSubsystemDesc;
	uint64_t maxIdentCount;
	struct ggsl_driver_ident_list_desc* pIdentListDescList;
	uint64_t identListDescListSize;
};
struct ggsl_driver_info{
	uint64_t driverId;
	struct gpu_shader_driver_vtable vtable;
};
int ggsl_driver_init(void);
int ggsl_driver_deinit(void);
int ggsl_driver_hash16(uint8_t* pData, uint64_t dataSize, uint16_t* pHash);
int ggsl_driver_ident_register(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_ident_info* pIdentInfo, struct ggsl_driver_ident_desc** ppIdentDesc);
int ggsl_driver_ident_unregister(struct ggsl_driver_shader_desc* pShaderDesc, uint64_t identId);
int ggsl_driver_ident_get_desc(struct ggsl_driver_shader_desc* pShaderDesc, unsigned char* pIdent, uint64_t identLength, struct ggsl_driver_ident_desc** ppIdentDesc);
int ggsl_driver_ident_get_next_desc(struct ggsl_driver_shader_desc* pShaderDesco, struct ggsl_driver_ident_desc** ppIdentDesc);
int ggsl_driver_instruction_list_reset(struct ggsl_driver_shader_desc* pShaderDesc);
int ggsl_driver_instruction_get_info(struct ggsl_driver_shader_desc* pShaderDesc, struct gpu_instruction_info* pInstructionInfo);
int ggsl_driver_subsystem_driver_init(uint64_t driverId);
int ggsl_driver_subsystem_driver_deinit(uint64_t driverId);
int ggsl_driver_subsystem_shader_init(uint64_t gpuId, uint64_t contextId, uint64_t objectId, struct gpu_create_shader_object_info* pCreateObjectInfo);
int ggsl_driver_subsystem_shader_deinit(uint64_t gpuId, uint64_t contextId, uint64_t objectId);
int ggsl_driver_subsystem_instruction_list_reset(uint64_t gpuId, uint64_t contextId, uint64_t objectId);
int ggsl_driver_subsystem_instruction_get_info(uint64_t gpuId, uint64_t contextId, uint64_t objectId, struct gpu_instruction_info* pInstructionInfo);
#endif
