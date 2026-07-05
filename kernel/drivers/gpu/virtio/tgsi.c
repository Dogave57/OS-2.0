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
	};	
	const unsigned char* pTagTypeName = tagTypeNameList[tagType];
	pTagTypeName = !pTagTypeName ? (const unsigned char*)"Unknown" : pTagTypeName;
	*ppTagTypeName = pTagTypeName;
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
	unsigned char* pShaderCode = (unsigned char*)0x00;
	if (virtio_gpu_tgsi_get_current_shader_code(pShaderCodeInfo, &pShaderCode)!=0){
		printf("failed to get current virtual I/O GPU host controller string-form TGSI shader code\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	for (uint64_t i = 0;;i++){
		if (dataLength&&i>dataLength){
			pShaderCode[i] = 0x00;
			pShaderCodeInfo->shaderCodeLength+=dataLength;
			break;
		}
		if (!dataLength&&pData[i]==0x00){
			pShaderCode[i] = 0x00;
			pShaderCodeInfo->shaderCodeLength+=i;
			break;
		}
		pShaderCode[i] = pData[i];
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
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_code_push_tag_list(struct virtio_gpu_shader_code_info* pShaderCodeInfo, struct gpu_tag_list_info tagListInfo){
	if (!pShaderCodeInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	for (uint64_t i = 0;i<tagListInfo.tagCount;i++){
		uint8_t tagType = tagListInfo.tagList[i];
		unsigned char* pTagTypeName = (unsigned char*)"Unknown";
		virtio_gpu_tgsi_tag_type_get_name(tagType, (const unsigned char**)&pTagTypeName);
		virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, pTagTypeName, 0x00);
		if (i==tagListInfo.tagCount-0x01)
			continue;
		virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, ", ", 0x02);
	}
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
		case GPU_SHADER_DECLARE_TYPE_INPUT:{
			virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "DCL IN[", 0x00);
			virtio_gpu_tgsi_shader_code_push_u64(pShaderCodeInfo, pDeclareInfo->declareLocation.declareId);
			virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "]", 0x01);
			if (!pDeclareInfo->tagListInfo.tagCount){
				virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "\n", 0x01);
				break;
			}
			virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, ", ", 0x02);
			virtio_gpu_tgsi_shader_code_push_tag_list(pShaderCodeInfo, pDeclareInfo->tagListInfo);
			virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "\n", 0x01);
			break;				   
		}
		case GPU_SHADER_DECLARE_TYPE_OUTPUT:{
			virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "DCL OUT[", 0x00);
			virtio_gpu_tgsi_shader_code_push_u64(pShaderCodeInfo, pDeclareInfo->declareLocation.declareId);
			virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "]", 0x01);
			if (!pDeclareInfo->tagListInfo.tagCount){
				virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "\n", 0x01);
				break;
			}
			virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, ", ", 0x02);
			virtio_gpu_tgsi_shader_code_push_tag_list(pShaderCodeInfo, pDeclareInfo->tagListInfo);
			virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "\n", 0x01);
			break;				    
		}
		case GPU_SHADER_DECLARE_TYPE_IMMEDIATE:{
			static const unsigned char* scalarTypeNameList[256]={
				[GPU_SHADER_SCALAR_TYPE_INVALID]="Invalid",
				[GPU_SHADER_SCALAR_TYPE_FLOAT]="FLT",
				[GPU_SHADER_SCALAR_TYPE_UINT]="UINT",
				[GPU_SHADER_SCALAR_TYPE_SINT]="SINT",
			};
			static const unsigned char* scalarSizeNameList[256]={
				[0x00]="Invalid",
				[sizeof(uint8_t)]="8",
				[sizeof(uint16_t)]="16",
				[sizeof(uint32_t)]="32",
				[sizeof(uint64_t)]="64",
			};
			unsigned char* pScalarTypeName = (unsigned char*)scalarTypeNameList[pDeclareInfo->scalarType];
			unsigned char* pScalarSizeName = (unsigned char*)scalarSizeNameList[pDeclareInfo->scalarSize];
			pScalarTypeName = !pScalarTypeName ? (unsigned char*)"Invalid" : pScalarTypeName;
			pScalarSizeName = !pScalarSizeName ? (unsigned char*)"Invalid" : pScalarSizeName;
			virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "IMM[", 0x00);
			virtio_gpu_tgsi_shader_code_push_u64(pShaderCodeInfo, (uint64_t)pDeclareInfo->declareLocation.declareId);
			virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "] ", 0x00);
			virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, pScalarTypeName, 0x00);
			virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, pScalarSizeName, 0x00);
			virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, " { ", 0x00);
			for (uint64_t i = 0;i<pDeclareInfo->scalarCount;i++){
				unsigned char* pScalarName = pDeclareInfo->scalarIdentList[i];
				uint64_t scalarNameLength = pDeclareInfo->scalarLengthList[i];
				virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, pScalarName, scalarNameLength);
				virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, (i==pDeclareInfo->scalarCount-0x01) ? " }\n" : ", ", 0x00);
			}
			break;	       
		}
		case GPU_SHADER_DECLARE_TYPE_TEMP:{
			virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "DCL TEMP[", 0x00);
			virtio_gpu_tgsi_shader_code_push_u64(pShaderCodeInfo, (uint64_t)pDeclareInfo->declareLocation.declareId);
			virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "]\n", 0x02);
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

	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_instruction_add(struct gpu_instruction_info_add* pInstructionInfo, struct virtio_gpu_shader_code_info* pShaderCodeInfo){
	if (!pInstructionInfo||!pShaderCodeInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_instruction_sub(struct gpu_instruction_info_sub* pInstructionInfo, struct virtio_gpu_shader_code_info* pShaderCodeInfo){
	if (!pInstructionInfo||!pShaderCodeInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);

	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_instruction_mul(struct gpu_instruction_info_mul* pInstructionInfo, struct virtio_gpu_shader_code_info* pShaderCodeInfo){
	if (!pInstructionInfo||!pShaderCodeInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);

	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_instruction_div(struct gpu_instruction_info_div* pInstructionInfo, struct virtio_gpu_shader_code_info* pShaderCodeInfo){
	if (!pInstructionInfo||!pShaderCodeInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);

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
				[GPU_SHADER_OPCODE_ADD]=(virtioGpuShaderInstructionFunc)virtio_gpu_tgsi_shader_instruction_add,
				[GPU_SHADER_OPCODE_SUB]=(virtioGpuShaderInstructionFunc)virtio_gpu_tgsi_shader_instruction_sub,
				[GPU_SHADER_OPCODE_MUL]=(virtioGpuShaderInstructionFunc)virtio_gpu_tgsi_shader_instruction_mul,
				[GPU_SHADER_OPCODE_DIV]=(virtioGpuShaderInstructionFunc)virtio_gpu_tgsi_shader_instruction_div,
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
			printf("GPU host controller instruction info descriptor translation time: %fms\r\n", ((double)elapsedTime)/1000.0);
		}
	}
	virtio_gpu_tgsi_shader_code_push_string(pShaderCodeInfo, "END\n", 0x04);
	mutex_unlock(&mutex);
	return 0;
}
