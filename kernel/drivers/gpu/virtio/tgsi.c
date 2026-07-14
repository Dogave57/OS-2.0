#include "mem/vmm.h"
#include "mem/heap.h"
#include "cpu/mutex.h"
#include "stdlib/stdlib.h"
#include "subsystem/gpu.h"
#include "drivers/timer.h"
#include "drivers/gpu/virtio/virtio.h"
#include "drivers/gpu/virtio/tgsi.h"
int virtio_gpu_tgsi_shader_type_get_name(uint8_t shaderType, const unsigned char** ppShaderTypeName){
	if (!ppShaderTypeName)
		return -1;
	static const unsigned char* shaderTypeNameList[256]={
		[GPU_SHADER_TYPE_VERTEX]="VERT",
		[GPU_SHADER_TYPE_FRAGMENT]="FRAG",
		[GPU_SHADER_TYPE_GEOMETRY]="GEOM",
	};	
	const unsigned char* pShaderTypeName = shaderTypeNameList[shaderType];
	pShaderTypeName = !pShaderTypeName ? (const unsigned char*)"Unknown" : pShaderTypeName;
	*ppShaderTypeName = pShaderTypeName;
	return 0;
}
int virtio_gpu_tgsi_tag_type_get_name(uint8_t tagType, const unsigned char** ppTagTypeName){
	if (!ppTagTypeName)
		return -1;
	static const unsigned char* tagTypeNameList[256]={
		[GPU_SHADER_TAG_TYPE_INVALID]="Invalid",
		[GPU_SHADER_TAG_TYPE_POSITION]="POSITION",
		[GPU_SHADER_TAG_TYPE_COLOR]="COLOR",
		[GPU_SHADER_TAG_TYPE_TEXCOORD]="TEXCOORD",
		[GPU_SHADER_TAG_TYPE_PERSPECTIVE]="PERSPECTIVE",
		[GPU_SHADER_TAG_TYPE_GENERIC]="GENERIC",
		[GPU_SHADER_TAG_TYPE_2D]="2D",
		[GPU_SHADER_TAG_TYPE_3D]="3D",
		[GPU_SHADER_TAG_TYPE_FLOAT]="FLOAT",
	};	
	const unsigned char* pTagTypeName = tagTypeNameList[tagType];
	pTagTypeName = !pTagTypeName ? (const unsigned char*)"Unknown" : pTagTypeName;
	*ppTagTypeName = pTagTypeName;
	return 0;
}
int virtio_gpu_tgsi_declare_type_get_name(uint8_t declareType, const unsigned char** ppDeclareTypeName){
	if (!ppDeclareTypeName)
		return -1;
	static const unsigned char* declareTypeNameList[256]={
		[GPU_SHADER_DECLARE_TYPE_INVALID]="Invalid",
		[GPU_SHADER_DECLARE_TYPE_INPUT]="IN",
		[GPU_SHADER_DECLARE_TYPE_OUTPUT]="OUT",
		[GPU_SHADER_DECLARE_TYPE_IMMEDIATE]="IMM",
		[GPU_SHADER_DECLARE_TYPE_CONSTANT]="CONST",
		[GPU_SHADER_DECLARE_TYPE_SAMP]="SAMP",
		[GPU_SHADER_DECLARE_TYPE_SVIEW]="SVIEW",
		[GPU_SHADER_DECLARE_TYPE_TEMP]="TEMP",
	};
	const unsigned char* pDeclareTypeName = declareTypeNameList[declareType];
	pDeclareTypeName = !pDeclareTypeName ? (const unsigned char*)"Unknown" : pDeclareTypeName;
	*ppDeclareTypeName = pDeclareTypeName;
	return 0;
}
int virtio_gpu_tgsi_get_current_shader_code(struct virtio_gpu_shader_code_info* pShaderCodeInfo, unsigned char** ppShaderCode){
	if (!pShaderCodeInfo||!ppShaderCode)
		return -1;
	unsigned char* pShaderCode = pShaderCodeInfo->pShaderCode+pShaderCodeInfo->shaderCodeLength;
	*ppShaderCode = pShaderCode;
	return 0;
}
int virtio_gpu_tgsi_shader_code_push_string(struct virtio_gpu_shader_code_info* pShaderCodeInfo, unsigned char* pData, uint64_t dataLength){
	if (!pShaderCodeInfo||!pData)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	unsigned char* pShaderData = (unsigned char*)0x00;
	if (virtio_gpu_tgsi_get_current_shader_code(pShaderCodeInfo, &pShaderData)!=0){
		printf("failed to get current virtual I/O GPU host controller string-form TGSI shader code\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	for (uint64_t i = 0;;i++){
		if (dataLength&&i>dataLength){
			pShaderData[i] = 0x00;
			pShaderCodeInfo->shaderCodeLength+=dataLength;
			break;
		}
		if (!dataLength&&pData[i]==0x00){
			pShaderData[i] = 0x00;
			pShaderCodeInfo->shaderCodeLength+=i;
			break;
		}
		pShaderData[i] = pData[i];
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_code_push_u64(struct virtio_gpu_shader_code_info* pShaderCodeInfo, uint64_t value){
	if (!pShaderCodeInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	unsigned char* pShaderCode = (unsigned char*)0x00;
	if (virtio_gpu_tgsi_get_current_shader_code(pShaderCodeInfo, &pShaderCode)!=0){
		printf("failed to get current virtual I/O GPU host controller string-form TGSI shader code\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t bufferSize = pShaderCodeInfo->shaderCodeSize-pShaderCodeInfo->shaderCodeLength;
	uint64_t bufferLength = 0x00;
	if (utoa64(value, pShaderCode, bufferSize, &bufferLength)!=0){
		printf("failed to convert standard QWORD to string physical page data\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	pShaderCodeInfo->shaderCodeLength+=bufferLength;
	pShaderCodeInfo->pShaderCode[pShaderCodeInfo->shaderCodeLength] = 0x00;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_code_push_i64(struct virtio_gpu_shader_code_info* pShaderCodeInfo, int64_t value){
	if (!pShaderCodeInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	unsigned char* pShaderCode = (unsigned char*)0x00;
	if (virtio_gpu_tgsi_get_current_shader_code(pShaderCodeInfo, &pShaderCode)!=0){
		printf("failed to get current virtual I/O GPU host controller string-form TGSI shader code\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t bufferSize = pShaderCodeInfo->shaderCodeSize-pShaderCodeInfo->shaderCodeLength;
	uint64_t bufferLength = 0x00;
	if (itoa64(value, pShaderCode, bufferSize, &bufferLength)!=0){
		printf("failed to convert signed QWORD to string physical page data\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	pShaderCodeInfo->shaderCodeLength+=bufferLength;
	pShaderCodeInfo->pShaderCode[pShaderCodeInfo->shaderCodeLength] = 0x00;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_code_push_tag_list(struct virtio_gpu_shader_code_info* pShaderCodeInfo, struct gpu_tag_list_info tagListInfo){
	if (!pShaderCodeInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	for (uint64_t i = 0;i<tagListInfo.tagCount;i++){
		struct gpu_tag_list_entry* pTagListEntry = ((struct gpu_tag_list_entry*)tagListInfo.tagList)+i;
		unsigned char* pTagTypeName = (unsigned char*)"Unknown";
		virtio_gpu_tgsi_tag_type_get_name(pTagListEntry->tagType, (const unsigned char**)&pTagTypeName);
		virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, pTagTypeName, 0x00);
		if (pTagListEntry->tagIndex){
			virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "[", 0x01);
			virtio_gpu_tgsi_shader_code_push_u64(pShaderCodeInfo, (uint64_t)(pTagListEntry->tagIndex-0x01));
			virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "]", 0x01);
		}
		if (i==tagListInfo.tagCount-0x01)
			continue;
		virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, ", ", 0x02);
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_code_push_declare(struct virtio_gpu_shader_code_info* pShaderCodeInfo, struct gpu_declare_location_info declareLocationInfo){
	if (!pShaderCodeInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	unsigned char* pDeclareTypeName = (unsigned char*)0x00;
	virtio_gpu_tgsi_declare_type_get_name(declareLocationInfo.declareType, (const unsigned char**)&pDeclareTypeName);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, pDeclareTypeName, 0x00);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "[", 0x01);
	virtio_gpu_tgsi_shader_code_push_u64(pShaderCodeInfo, (uint64_t)declareLocationInfo.declareId);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "]", 0x01);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_code_push_swizzle(struct virtio_gpu_shader_code_info* pShaderCodeInfo, struct gpu_swizzle swizzle, uint8_t operandIndex){
	if (!pShaderCodeInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	static const unsigned char swizzleNameList[256]={
		[GPU_SWIZZLE_RED]='x',
		[GPU_SWIZZLE_GREEN]='y',
		[GPU_SWIZZLE_BLUE]='z',
		[GPU_SWIZZLE_ALPHA]='w',
	};
	unsigned char* pCurrentShaderCode = (unsigned char*)0x00;
	if (virtio_gpu_tgsi_get_current_shader_code(pShaderCodeInfo, &pCurrentShaderCode)!=0){
		printf("failed to get current GPU host controller GGSL shader protocol driver shader code\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t swizzleCount = 0x00;
	uint8_t lastSwizzleEntry = 0x00;
	*pCurrentShaderCode = '.';
	pCurrentShaderCode++;
	for (swizzleCount = 0x00;swizzleCount<0x04;swizzleCount++){
		uint8_t swizzleEntry = swizzle.swizzleList[swizzleCount];
		if (!operandIndex&&!swizzleEntry)
			break;
		swizzleEntry = !swizzleEntry ? lastSwizzleEntry : swizzleEntry;
		*(pCurrentShaderCode+swizzleCount) = swizzleNameList[swizzleEntry];
		lastSwizzleEntry = swizzleEntry;
	}
	pShaderCodeInfo->shaderCodeLength+=swizzleCount+0x01;
	*(pShaderCodeInfo->pShaderCode+pShaderCodeInfo->shaderCodeLength) = 0x00;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_instruction_declare(struct gpu_instruction_info* pInstructionInfo, struct virtio_gpu_shader_code_info* pShaderCodeInfo){
	if (!pInstructionInfo||!pShaderCodeInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_instruction_info_dcl* pDeclareInstructionInfo = (struct gpu_instruction_info_dcl*)pInstructionInfo;
	struct gpu_declare_info* pDeclareInfo = &pDeclareInstructionInfo->declareInfo;
	switch (pDeclareInfo->declareLocation.declareType){
		case GPU_SHADER_DECLARE_TYPE_IMMEDIATE:{
			static const unsigned char* scalarTypeNameList[256] = {
				[GPU_SHADER_SCALAR_TYPE_FLOAT]="FLT",
				[GPU_SHADER_SCALAR_TYPE_UINT]="UINT",
				[GPU_SHADER_SCALAR_TYPE_SINT]="SINT",	
			};	
			static const unsigned char* scalarLengthNameList[256]= {
				[sizeof(uint8_t)]="8",
				[sizeof(uint16_t)]="16",
				[sizeof(uint32_t)]="32",
				[sizeof(uint64_t)]="64",
			};
			unsigned char* pScalarTypeName = (unsigned char*)scalarTypeNameList[pDeclareInfo->scalarType];
			if (!pScalarTypeName){
				printf("invalid GPU host controller declaration scalar type\r\n");
				mutex_unlock(&mutex);
				return -1;
			}
			unsigned char* pScalarLengthName = (unsigned char*)scalarLengthNameList[pDeclareInfo->scalarSize];
			if (!pScalarLengthName){
				printf("invalid GPU host controller declaration scalar length\r\n");
				mutex_unlock(&mutex);
				return -1;
			}
			virtio_gpu_tgsi_shader_code_push_declare(pShaderCodeInfo, pDeclareInfo->declareLocation);
			virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, " ", 0x01);
			virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, pScalarTypeName, 0x00);
			virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, pScalarLengthName, 0x00);
			virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, " { ", 0x03);
			for (uint64_t i = 0;i<0x04;i++){
				unsigned char* pScalarName = (i==0x03) ? "1.0" : "0.0";
				pScalarName = (i<pDeclareInfo->scalarCount) ? *(pDeclareInfo->scalarIdentList+i) : pScalarName;
				uint64_t scalarNameLength = 0x03;
				scalarNameLength = (i<pDeclareInfo->scalarCount) ? *(pDeclareInfo->scalarLengthList+i) : scalarNameLength;
				virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, pScalarName, scalarNameLength);
				virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, (i>=0x03) ? " }\n" : ", ", 0x00);
			}
			break;				       
		}
		default:{
			virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "DCL ", 0x04);
			virtio_gpu_tgsi_shader_code_push_declare(pShaderCodeInfo, pDeclareInfo->declareLocation);
			if (!pDeclareInfo->tagListInfo.tagCount){
				virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "\n", 0x01);
				break;
			}
			virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, ", ", 0x02);
			virtio_gpu_tgsi_shader_code_push_tag_list(pShaderCodeInfo, pDeclareInfo->tagListInfo);
			virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "\n", 0x01);
			break;
		}
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_instruction_mov(struct gpu_instruction_info_mov* pInstructionInfo, struct virtio_gpu_shader_code_info* pShaderCodeInfo){
	if (!pInstructionInfo||!pShaderCodeInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_operand_info* pOperandInfoList = (struct gpu_operand_info*)pInstructionInfo->operandInfoList;
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "MOV ", 0x04);
	virtio_gpu_tgsi_shader_code_push_declare(pShaderCodeInfo, (pOperandInfoList+0x00)->declareLocationInfo);
	virtio_gpu_tgsi_shader_code_push_swizzle(pShaderCodeInfo, (pOperandInfoList+0x00)->swizzle, 0x00);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, ", ", 0x02);
	virtio_gpu_tgsi_shader_code_push_declare(pShaderCodeInfo, (pOperandInfoList+0x01)->declareLocationInfo);
	virtio_gpu_tgsi_shader_code_push_swizzle(pShaderCodeInfo, (pOperandInfoList+0x01)->swizzle, 0x01);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "\n", 0x01);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_instruction_add(struct gpu_instruction_info_add* pInstructionInfo, struct virtio_gpu_shader_code_info* pShaderCodeInfo){
	if (!pInstructionInfo||!pShaderCodeInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_operand_info* pOperandInfoList = (struct gpu_operand_info*)pInstructionInfo->operandInfoList;
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "ADD ", 0x04);
	virtio_gpu_tgsi_shader_code_push_declare(pShaderCodeInfo, (pOperandInfoList+0x00)->declareLocationInfo);
	virtio_gpu_tgsi_shader_code_push_swizzle(pShaderCodeInfo, (pOperandInfoList+0x00)->swizzle, 0x00);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, ", ", 0x02);
	virtio_gpu_tgsi_shader_code_push_declare(pShaderCodeInfo, (pOperandInfoList+0x01)->declareLocationInfo);
	virtio_gpu_tgsi_shader_code_push_swizzle(pShaderCodeInfo, (pOperandInfoList+0x01)->swizzle, 0x01);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, ", ", 0x02);
	virtio_gpu_tgsi_shader_code_push_declare(pShaderCodeInfo, (pOperandInfoList+0x02)->declareLocationInfo);
	virtio_gpu_tgsi_shader_code_push_swizzle(pShaderCodeInfo, (pOperandInfoList+0x02)->swizzle, 0x02);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "\n", 0x01);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_instruction_sub(struct gpu_instruction_info_sub* pInstructionInfo, struct virtio_gpu_shader_code_info* pShaderCodeInfo){
	if (!pInstructionInfo||!pShaderCodeInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_operand_info* pOperandInfoList = (struct gpu_operand_info*)pInstructionInfo->operandInfoList;
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "SUB ", 0x04);
	virtio_gpu_tgsi_shader_code_push_declare(pShaderCodeInfo, (pOperandInfoList+0x00)->declareLocationInfo);
	virtio_gpu_tgsi_shader_code_push_swizzle(pShaderCodeInfo, (pOperandInfoList+0x00)->swizzle, 0x00);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, ", ", 0x02);
	virtio_gpu_tgsi_shader_code_push_declare(pShaderCodeInfo, (pOperandInfoList+0x01)->declareLocationInfo);
	virtio_gpu_tgsi_shader_code_push_swizzle(pShaderCodeInfo, (pOperandInfoList+0x01)->swizzle, 0x01);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, ", ", 0x02);
	virtio_gpu_tgsi_shader_code_push_declare(pShaderCodeInfo, (pOperandInfoList+0x02)->declareLocationInfo);
	virtio_gpu_tgsi_shader_code_push_swizzle(pShaderCodeInfo, (pOperandInfoList+0x02)->swizzle, 0x02);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "\n", 0x01);
	mutex_unlock(&mutex);
	return 0;
}		
int virtio_gpu_tgsi_shader_instruction_mul(struct gpu_instruction_info_mul* pInstructionInfo, struct virtio_gpu_shader_code_info* pShaderCodeInfo){
	if (!pInstructionInfo||!pShaderCodeInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_operand_info* pOperandInfoList = (struct gpu_operand_info*)pInstructionInfo->operandInfoList;
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "MUL ", 0x04);
	virtio_gpu_tgsi_shader_code_push_declare(pShaderCodeInfo, (pOperandInfoList+0x00)->declareLocationInfo);
	virtio_gpu_tgsi_shader_code_push_swizzle(pShaderCodeInfo, (pOperandInfoList+0x00)->swizzle, 0x00);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, ", ", 0x02);
	virtio_gpu_tgsi_shader_code_push_declare(pShaderCodeInfo, (pOperandInfoList+0x01)->declareLocationInfo);
	virtio_gpu_tgsi_shader_code_push_swizzle(pShaderCodeInfo, (pOperandInfoList+0x01)->swizzle, 0x01);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, ", ", 0x02);
	virtio_gpu_tgsi_shader_code_push_declare(pShaderCodeInfo, (pOperandInfoList+0x02)->declareLocationInfo);
	virtio_gpu_tgsi_shader_code_push_swizzle(pShaderCodeInfo, (pOperandInfoList+0x02)->swizzle, 0x02);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "\n", 0x01);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_instruction_div(struct gpu_instruction_info_div* pInstructionInfo, struct virtio_gpu_shader_code_info* pShaderCodeInfo){
	if (!pInstructionInfo||!pShaderCodeInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_operand_info* pOperandInfoList = (struct gpu_operand_info*)pInstructionInfo->operandInfoList;
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "DIV ", 0x04);
	virtio_gpu_tgsi_shader_code_push_declare(pShaderCodeInfo, (pOperandInfoList+0x00)->declareLocationInfo);
	virtio_gpu_tgsi_shader_code_push_swizzle(pShaderCodeInfo, (pOperandInfoList+0x00)->swizzle, 0x00);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, ", ", 0x02);
	virtio_gpu_tgsi_shader_code_push_declare(pShaderCodeInfo, (pOperandInfoList+0x01)->declareLocationInfo);
	virtio_gpu_tgsi_shader_code_push_swizzle(pShaderCodeInfo, (pOperandInfoList+0x01)->swizzle, 0x01);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, ", ", 0x02);
	virtio_gpu_tgsi_shader_code_push_declare(pShaderCodeInfo, (pOperandInfoList+0x02)->declareLocationInfo);
	virtio_gpu_tgsi_shader_code_push_swizzle(pShaderCodeInfo, (pOperandInfoList+0x02)->swizzle, 0x02);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "\n", 0x01);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_instruction_ddx(struct gpu_instruction_info_ddx* pInstructionInfo, struct virtio_gpu_shader_code_info* pShaderCodeInfo){
	if (!pInstructionInfo||!pShaderCodeInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_operand_info* pOperandInfoList = (struct gpu_operand_info*)pInstructionInfo->operandInfoList;
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "DDX ", 0x04);
	virtio_gpu_tgsi_shader_code_push_declare(pShaderCodeInfo, (pOperandInfoList+0x00)->declareLocationInfo);
	virtio_gpu_tgsi_shader_code_push_swizzle(pShaderCodeInfo, (pOperandInfoList+0x00)->swizzle, 0x00);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, ", ", 0x02);
	virtio_gpu_tgsi_shader_code_push_declare(pShaderCodeInfo, (pOperandInfoList+0x01)->declareLocationInfo);
	virtio_gpu_tgsi_shader_code_push_swizzle(pShaderCodeInfo, (pOperandInfoList+0x01)->swizzle, 0x01);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "\n", 0x01);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_instruction_ddy(struct gpu_instruction_info_ddy* pInstructionInfo, struct virtio_gpu_shader_code_info* pShaderCodeInfo){
	if (!pInstructionInfo||!pShaderCodeInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_operand_info* pOperandInfoList = (struct gpu_operand_info*)pInstructionInfo->operandInfoList;
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "DDY ", 0x04);
	virtio_gpu_tgsi_shader_code_push_declare(pShaderCodeInfo, (pOperandInfoList+0x00)->declareLocationInfo);
	virtio_gpu_tgsi_shader_code_push_swizzle(pShaderCodeInfo, (pOperandInfoList+0x00)->swizzle, 0x00);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, ", ", 0x02);
	virtio_gpu_tgsi_shader_code_push_declare(pShaderCodeInfo, (pOperandInfoList+0x01)->declareLocationInfo);
	virtio_gpu_tgsi_shader_code_push_swizzle(pShaderCodeInfo, (pOperandInfoList+0x01)->swizzle, 0x01);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "\n", 0x01);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_instruction_tex(struct gpu_instruction_info_tex* pInstructionInfo, struct virtio_gpu_shader_code_info* pShaderCodeInfo){
	if (!pInstructionInfo||!pShaderCodeInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_operand_info* pOperandInfoList = (struct gpu_operand_info*)pInstructionInfo->operandInfoList;
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "TEX ", 0x04);
	virtio_gpu_tgsi_shader_code_push_declare(pShaderCodeInfo, (pOperandInfoList+0x00)->declareLocationInfo);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, ", ", 0x02);
	virtio_gpu_tgsi_shader_code_push_declare(pShaderCodeInfo, (pOperandInfoList+0x01)->declareLocationInfo);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, ", ", 0x02);
	virtio_gpu_tgsi_shader_code_push_declare(pShaderCodeInfo, (pOperandInfoList+0x02)->declareLocationInfo);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, ", ", 0x02);
	virtio_gpu_tgsi_shader_code_push_tag_list(pShaderCodeInfo, pInstructionInfo->tagListInfo);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "\n", 0x01);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_translate(uint64_t gpuId, uint64_t contextId, uint64_t objectId, struct gpu_create_shader_object_info* pCreateObjectInfo, struct virtio_gpu_shader_code_info* pShaderCodeInfo){
	if (!pCreateObjectInfo||!pShaderCodeInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x00;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (gpu_instruction_list_reset(gpuId, contextId, objectId)!=0){
		printf("failed to reset GPU host controller shader code instruction list\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_instruction_info* pInstructionInfoList = (struct gpu_instruction_info*)0x00;
	uint64_t maxInstructionInfoCount = 0x100;
	uint64_t instructionInfoListSize = sizeof(struct gpu_instruction_info)*maxInstructionInfoCount;
	if (virtualAlloc((uint64_t*)&pInstructionInfoList, instructionInfoListSize, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate physical pages for virtual I/O GPU host controller shader code instruction info descriptor list\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	unsigned char* pShaderTypeName = (unsigned char*)"Unknown";
	virtio_gpu_tgsi_shader_type_get_name(pCreateObjectInfo->shaderType, (const unsigned char**)&pShaderTypeName);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, pShaderTypeName, 0x00);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "\n", 0x01);
	uint64_t instructionCount = 0x00;
	for (instructionCount = 0x00;;instructionCount++){
		struct gpu_get_instruction_info getInstructionInfo = {0};
		memset((void*)&getInstructionInfo, 0, sizeof(struct gpu_get_instruction_info));
		getInstructionInfo.pInstructionInfoList = pInstructionInfoList;
		getInstructionInfo.maxInstructionInfoCount = maxInstructionInfoCount;
		if (gpu_instruction_get_info(gpuId, contextId, objectId, &getInstructionInfo)!=0)
			break;
		for (uint64_t i = 0;i<getInstructionInfo.instructionInfoCount;i++){
			struct gpu_instruction_info* pInstructionInfo = pInstructionInfoList+i;
			static const virtioGpuShaderInstructionFunc instructionFuncList[256]={
				[GPU_SHADER_OPCODE_DECLARE]=(virtioGpuShaderInstructionFunc)virtio_gpu_tgsi_shader_instruction_declare,
				[GPU_SHADER_OPCODE_MOV]=(virtioGpuShaderInstructionFunc)virtio_gpu_tgsi_shader_instruction_mov,
				[GPU_SHADER_OPCODE_ADD]=(virtioGpuShaderInstructionFunc)virtio_gpu_tgsi_shader_instruction_add,
				[GPU_SHADER_OPCODE_SUB]=(virtioGpuShaderInstructionFunc)virtio_gpu_tgsi_shader_instruction_sub,
				[GPU_SHADER_OPCODE_MUL]=(virtioGpuShaderInstructionFunc)virtio_gpu_tgsi_shader_instruction_mul,
				[GPU_SHADER_OPCODE_DIV]=(virtioGpuShaderInstructionFunc)virtio_gpu_tgsi_shader_instruction_div,
				[GPU_SHADER_OPCODE_DDX]=(virtioGpuShaderInstructionFunc)virtio_gpu_tgsi_shader_instruction_ddx,
				[GPU_SHADER_OPCODE_DDY]=(virtioGpuShaderInstructionFunc)virtio_gpu_tgsi_shader_instruction_ddy,
				[GPU_SHADER_OPCODE_TEX]=(virtioGpuShaderInstructionFunc)virtio_gpu_tgsi_shader_instruction_tex,
			};	
			virtioGpuShaderInstructionFunc instructionFunc = instructionFuncList[pInstructionInfo->opcode];
			if (!instructionFunc){
				printf("unsupported GPU host controller shader instruction 0x%x\r\n", pInstructionInfo->opcode);
				mutex_unlock(&mutex);
				return -1;
			}
		//	printf("instruction opcode: 0x%x\r\n", pInstructionInfo->opcode);
			uint64_t startTime = get_time_us();
			if (instructionFunc(pInstructionInfo, pShaderCodeInfo)!=0){
				printf("failed to translate GPU host controller shader instruction of opcode 0x%x to virtual I/O GPU host controller string-form TGSI\r\n", pInstructionInfo->opcode);
				mutex_unlock(&mutex);	
				return -1;
			}
			uint64_t elapsedTime = get_time_us()-startTime;
		//	printf("GPU host controller instruction info descriptor translation time: %fms\r\n", ((double)elapsedTime)/1000.0);
		}
	}
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "END\n", 0x04);
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, (" "+0x01), 0x01);
	mutex_unlock(&mutex);
	return 0;
}
