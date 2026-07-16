#ifndef _GGSL_DRIVER
#define _GGSL_DRIVER
#include "subsystem/subsystem.h"
#include "subsystem/gpu.h"
struct ggsl_driver_shader_desc;
struct ggsl_driver_ident_location_info;
struct ggsl_driver_ident_desc;
struct gpu_get_instruction_info;
struct ggsl_driver_format_ident_info;
struct gpu_declare_location_info;
struct ggsl_driver_expression_info;
typedef int(*ggslDriverInstructionFunc)(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_ident_location_info identLocationInfo, struct ggsl_driver_ident_desc* pIdentDesc, struct gpu_get_instruction_info* pGetInstructionInfo);
typedef int(*ggslDriverMethodFunc)(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_format_ident_info* pFormatIdentInfo, struct gpu_get_instruction_info* pGetInstructionInfo, struct ggsl_driver_expression_info* pExpressionInfo);
#define GGSL_DRIVER_SIGNATURE ((uint32_t)'LSGG')
#define GGSL_DRIVER_IDENT {0xC4, 0x94, 0x36, 0xA2,  0x3F, 0x9E, 0x98, 0x96,  0x9E, 0x1D, 0x4B, 0x06,  0xFD, 0x85, 0x2E, 0x4E}
#define GGSL_DRIVER_MAX_IDENT_COUNT (65536)

#define GGSL_DRIVER_IDENT_LIST_COUNT (66536)
#define GGSL_DRIVER_IDENT_TYPE_INVALID (0x00)
#define GGSL_DRIVER_IDENT_TYPE_OPCODE (0x01)
#define GGSL_DRIVER_IDENT_TYPE_METHOD (0x02)
#define GGSL_DRIVER_IDENT_TYPE_REFERENCE (0x03)
#define GGSL_DRIVER_IDENT_TYPE_TAG (0x04)
#define GGSL_DRIVER_IDENT_TYPE_FORMAT (0x05)
#define GGSL_DRIVER_IDENT_TYPE_SCALAR (0x06)

struct ggsl_driver_ident_location_info{
	unsigned char* pName;
	uint64_t nameLength;
	uint32_t hash;
};
struct ggsl_driver_basic_ident_info{
	unsigned char* pName;
	uint64_t nameLength;
	uint64_t identType;
	uint64_t extra0;
	uint64_t extra1;
};
struct ggsl_driver_opcode_ident_info{
	uint64_t instructionFunc;
};
struct ggsl_driver_method_ident_info{
	uint64_t methodFunc;
};
struct ggsl_driver_reference_ident_info{
	unsigned char* pReferenceName;
	uint64_t referenceNameLength;
	uint16_t referenceHash;
	uint64_t vectorCount;
	struct ggsl_driver_format_ident_info* pFormatIdentInfo;
	struct gpu_declare_location_info declareLocation;
};	
struct ggsl_driver_format_ident_info{
	uint32_t format;
	uint32_t scalarType;
	uint32_t scalarSize;
	uint32_t scalarCount;
	uint32_t reserved0;
};
struct ggsl_driver_tag_ident_info{
	uint16_t tagType;
};
struct ggsl_driver_ident_info{
	unsigned char* pName;
	uint16_t nameLength;
	uint32_t hash;
	uint16_t identType;
	uint64_t extra0;
	uint64_t extra1;
};
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
struct ggsl_driver_tag_list_info{
	uint64_t tagCount;
	uint16_t tagInfo;
};
struct ggsl_driver_expression_info{
	struct gpu_declare_location_info declareLocation;
	struct gpu_swizzle swizzleInfo;
};
struct ggsl_driver_get_next_ident_info{
	struct ggsl_driver_ident_location_info* pIdentLocationInfo;
	struct ggsl_driver_ident_desc** ppIdentDesc;
};
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
	uint64_t declareCountList[GPU_SHADER_DECLARE_TYPE_TEMP];
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
int ggsl_driver_ident_get_desc(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_ident_location_info* pIdentLocationInfo, struct ggsl_driver_ident_desc** ppIdentDesc);
int ggsl_driver_get_next_ident(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_get_next_ident_info getIdentInfo);
int ggsl_driver_ident_type_get_name(uint8_t identType, const unsigned char** ppIdentTypeName);
int ggsl_driver_tag_list_get_info(struct ggsl_driver_shader_desc* pShaderDesc, struct gpu_tag_list_info* pTagListInfo);
int ggsl_driver_vector_get_declare_info(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_format_ident_info* pFormatIdentInfo, struct gpu_declare_info* pDeclareInfo);
int ggsl_driver_swizzle_get_info(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_ident_location_info identLocationInfo, struct gpu_swizzle* pSwizzleInfo);
int ggsl_driver_reference_ident_register(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_reference_ident_info identInfo, struct ggsl_driver_ident_desc** ppIdentDesc);
int ggsl_driver_reference_ident_unregister(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_ident_desc* pIdentDesc);
int ggsl_driver_get_current_instruction(struct ggsl_driver_shader_desc* pShaderDesc, struct gpu_get_instruction_info* pGetInstructionInfo, struct gpu_instruction_info** ppInstructionInfo);
int ggsl_driver_expression_get_info(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_format_ident_info* pFormatIdentInfo, struct gpu_get_instruction_info* pGetInstructionInfo, struct ggsl_driver_expression_info* pExpressionInfo);
int ggsl_driver_instruction_push(struct ggsl_driver_shader_desc* pShaderDesc, struct gpu_get_instruction_info* pGetInstructionInfo, uint64_t instructionCount);
int ggsl_driver_instruction_declare_push(struct ggsl_driver_shader_desc* pShaderDesc, struct gpu_get_instruction_info* pGetInstructionInfo, uint64_t instructionCount);
int ggsl_driver_instruction_in(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_ident_location_info identLocationInfo, struct ggsl_driver_ident_desc* pIdentDesc, struct gpu_get_instruction_info* pGetInstructionInfo);
int ggsl_driver_instruction_imm(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_ident_location_info identLocationInfo, struct ggsl_driver_ident_desc* pIdentDesc, struct gpu_get_instruction_info* pGetInstructionInfo);
int ggsl_driver_instruction_const(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_ident_location_info identLocationInfo, struct ggsl_driver_ident_desc* pIdentDesc, struct gpu_get_instruction_info* pGetInstructionInfo);
int ggsl_driver_instruction_samp(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_ident_location_info identLocationInfo, struct ggsl_driver_ident_desc* pIdentDesc, struct gpu_get_instruction_info* pGetInstructionInfo);
int ggsl_driver_instruction_sview(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_ident_location_info identLocationInfo, struct ggsl_driver_ident_desc* pIdentDesc, struct gpu_get_instruction_info* pGetInstructionInfo);
int ggsl_driver_instruction_temp(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_ident_location_info identLocationInfo, struct ggsl_driver_ident_desc* pIdentDesc, struct gpu_get_instruction_info* pGetInstructionInfo);
int ggsl_driver_instruction_reference(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_ident_location_info identLocationInfo, struct ggsl_driver_ident_desc* pIdentDesc, struct gpu_get_instruction_info* pGetInstructionInfo);
int ggsl_driver_method_fadd(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_format_ident_info* pFormatIdentInfo, struct gpu_get_instruction_info* pGetInstructionInfo, struct ggsl_driver_expression_info* pExpressionInfo);
int ggsl_driver_method_fsub(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_format_ident_info* pFormatIdentInfo, struct gpu_get_instruction_info* pGetInstructionInfo, struct ggsl_driver_expression_info* pExpressionInfo);
int ggsl_driver_method_fmul(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_format_ident_info* pFormatIdentInfo,  struct gpu_get_instruction_info* pGetInstructionInfo, struct ggsl_driver_expression_info* pExpressionInfo);
int ggsl_driver_method_fdiv(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_format_ident_info* pFormatIdentInfo, struct gpu_get_instruction_info* pGetInstructionInfo, struct ggsl_driver_expression_info* pExpressionInfo);
int ggsl_driver_method_fsqrt(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_format_ident_info* pFormatIdentInfo, struct gpu_get_instruction_info* pGetInstructionInfo, struct ggsl_driver_expression_info* pExpressionInfo);
int ggsl_driver_method_frcp(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_format_ident_info* pFormatIdentInfo, struct gpu_get_instruction_info* pGetInstructionInfo, struct ggsl_driver_expression_info* pExpressionInfo);
int ggsl_driver_method_min(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_format_ident_info* pFormatIdentInfo, struct gpu_get_instruction_info* pGetInstructionInfo, struct ggsl_driver_expression_info* pExpressionInfo);
int ggsl_driver_method_max(struct ggsl_driver_shader_desc* pshaderdesc, struct ggsl_driver_format_ident_info* pformatidentinfo, struct gpu_get_instruction_info* pgetinstructioninfo, struct ggsl_driver_expression_info* pExpressionInfo);
int ggsl_driver_method_fddx(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_format_ident_info* pFormatIdentInfo, struct gpu_get_instruction_info* pGetInstructionInfo, struct ggsl_driver_expression_info* pExpressionInfo);
int ggsl_driver_method_fddy(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_format_ident_info* pFormatIdentInfo, struct gpu_get_instruction_info* pGetInstructionInfo, struct ggsl_driver_expression_info* pExpressionInfo);
int ggsl_driver_method_imm(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_format_ident_info* pFormatIdentInfo, struct gpu_get_instruction_info* pGetInstructionInfo, struct ggsl_driver_expression_info* pExpressionInfo);
int ggsl_driver_method_tex(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_format_ident_info* pFormatIdentInfo, struct gpu_get_instruction_info* pGetInstructionInfo, struct ggsl_driver_expression_info* pExpressionInfo);
int ggsl_driver_instruction_list_reset(struct ggsl_driver_shader_desc* pShaderDesc);
int ggsl_driver_instruction_get_info(struct ggsl_driver_shader_desc* pShaderDesc, struct gpu_get_instruction_info* pGetInstructionInfo);
int ggsl_driver_subsystem_driver_init(uint64_t driverId);
int ggsl_driver_subsystem_driver_deinit(uint64_t driverId);
int ggsl_driver_subsystem_shader_init(uint64_t gpuId, uint64_t contextId, uint64_t objectId, struct gpu_create_shader_object_info* pCreateObjectInfo);
int ggsl_driver_subsystem_shader_deinit(uint64_t gpuId, uint64_t contextId, uint64_t objectId);
int ggsl_driver_subsystem_instruction_list_reset(uint64_t gpuId, uint64_t contextId, uint64_t objectId);
int ggsl_driver_subsystem_instruction_get_info(uint64_t gpuId, uint64_t contextId, uint64_t objectId, struct gpu_get_instruction_info* pGetInstructionInfo);
#endif
