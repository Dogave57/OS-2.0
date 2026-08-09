#include "subsystem/gpu.h"
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
int virtio_gpu_tgsi_method_type_get_name(uint8_t methodType, const unsigned char** ppMethodTypeName){
	if (!ppMethodTypeName)
		return -1;
	static const unsigned char* methodTypeNameList[256]={
		[GPU_SHADER_METHOD_TYPE_INVALID]="Invalid",
		[GPU_SHADER_METHOD_TYPE_ADD]="ADD",
		[GPU_SHADER_METHOD_TYPE_SUB]="SUB",
		[GPU_SHADER_METHOD_TYPE_MUL]="MUL",
		[GPU_SHADER_METHOD_TYPE_DIV]="DIV",
		[GPU_SHADER_METHOD_TYPE_MOD]="UMOD",
		[GPU_SHADER_METHOD_TYPE_POW]="POW",
		[GPU_SHADER_METHOD_TYPE_SQRT]="SQRT",
		[GPU_SHADER_METHOD_TYPE_RCP]="RCP",
		[GPU_SHADER_METHOD_TYPE_DP]="DP",
		[GPU_SHADER_METHOD_TYPE_ABS]="ABS",
		[GPU_SHADER_METHOD_TYPE_FLOOR]="FLR",
		[GPU_SHADER_METHOD_TYPE_CEIL]="CEIL",
		[GPU_SHADER_METHOD_TYPE_ROUND]="ROUND",
		[GPU_SHADER_METHOD_TYPE_MIN]="MIN",
		[GPU_SHADER_METHOD_TYPE_MAX]="MAX",
		[GPU_SHADER_METHOD_TYPE_COS]="COS",
		[GPU_SHADER_METHOD_TYPE_SIN]="SIN",
		[GPU_SHADER_METHOD_TYPE_DDX]="DDX",
		[GPU_SHADER_METHOD_TYPE_DDY]="DDY",
		[GPU_SHADER_METHOD_TYPE_SEQ]="SEQ",
		[GPU_SHADER_METHOD_TYPE_SNE]="SNE",
		[GPU_SHADER_METHOD_TYPE_SLT]="SLT",
		[GPU_SHADER_METHOD_TYPE_SGT]="SGT",
		[GPU_SHADER_METHOD_TYPE_SLE]="SLE",
		[GPU_SHADER_METHOD_TYPE_SGE]="SGE",
		[GPU_SHADER_METHOD_TYPE_TEX]="TEX",
	};
	const unsigned char* pMethodTypeName = methodTypeNameList[methodType];
	pMethodTypeName = !pMethodTypeName ? (const unsigned char*)"Unknown" : pMethodTypeName;
	*ppMethodTypeName = pMethodTypeName;
	return 0;
}
int virtio_gpu_tgsi_get_current_shader_data(struct virtio_gpu_shader_data_info* pShaderDataInfo, uint8_t shaderDataType, unsigned char** ppShaderData){
	if (!pShaderDataInfo||!ppShaderData)
		return -1;
	unsigned char* pShaderData = (unsigned char*)0x00;
	switch (shaderDataType){
		case VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_DECLARE:{
			pShaderData = pShaderDataInfo->pDeclareInfo+pShaderDataInfo->declareInfoLength;
			break;
		}
		case VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE:{
			pShaderData = pShaderDataInfo->pShaderCode+pShaderDataInfo->shaderCodeLength;					   
			break;				   
		}
		default:{
			printf("unexpected virtual I/O GPU host controller shader physical page data type\r\n");
			return -1;	
		}
	}
	*ppShaderData = pShaderData;
	return 0;
}
int virtio_gpu_tgsi_get_current_shader_data_size(struct virtio_gpu_shader_data_info* pShaderDataInfo, uint8_t shaderDataType, uint64_t* pShaderDataSize){
	if (!pShaderDataInfo||!pShaderDataSize)
		return -1;
	uint64_t shaderDataSize = 0x00;
	switch (shaderDataType){
		case VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_DECLARE:{
			shaderDataSize = pShaderDataInfo->declareInfoSize-pShaderDataInfo->declareInfoLength;
			break;					 
		}
		case VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE:{
			shaderDataSize = pShaderDataInfo->shaderCodeSize-pShaderDataInfo->shaderCodeLength;
			break;				      
		}
	}
	*pShaderDataSize = shaderDataSize;
	return 0;
}
int virtio_gpu_tgsi_shader_data_push(struct virtio_gpu_shader_data_info* pShaderDataInfo, uint8_t shaderDataType, uint64_t shaderDataLength){
	if (!pShaderDataInfo||!shaderDataLength)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	switch (shaderDataType){
		case VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_DECLARE:{
			pShaderDataInfo->declareInfoLength+=shaderDataLength;			
		//	pShaderDataInfo->pDeclareInfo[pShaderDataInfo->declareInfoLength] = 0x00;
			break;					      
		}
		case VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE:{
			pShaderDataInfo->shaderCodeLength+=shaderDataLength;				   
		//	pShaderDataInfo->pShaderCode[pShaderDataInfo->shaderCodeLength] = 0x00;
			break;
		}
		default:{
			printf("unexpected virtual I/O GPU host controller shader physical page data type\r\n");
			mutex_unlock(&mutex);
			return -1;	
		}
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_data_push_string(struct virtio_gpu_shader_data_info* pShaderDataInfo, uint8_t shaderDataType, unsigned char* pData, uint64_t dataLength){
	if (!pShaderDataInfo||!pData)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	unsigned char* pShaderData = (unsigned char*)0x00;
	if (virtio_gpu_tgsi_get_current_shader_data(pShaderDataInfo, shaderDataType, &pShaderData)!=0){
		printf("failed to get current virtual I/O GPU host controller string-form TGSI shader physical page data\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t shaderDataLength = dataLength;
	for (uint64_t i = 0;;i++){
		if (dataLength&&i>=dataLength){
			pShaderData[i] = 0x00;
			break;
		}
		if (!dataLength&&pData[i]==0x00){
			pShaderData[i] = 0x00;
			shaderDataLength = i;
			break;
		}
		pShaderData[i] = pData[i];
	}
	if (virtio_gpu_tgsi_shader_data_push(pShaderDataInfo, shaderDataType, shaderDataLength)!=0){
		printf("failed to push virtual I/O GPU host controller shader physical page data\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_data_push_u64(struct virtio_gpu_shader_data_info* pShaderDataInfo, uint8_t shaderDataType, uint64_t value){
	if (!pShaderDataInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	unsigned char* pShaderData = (unsigned char*)0x00;
	uint64_t shaderDataSize = 0x00;
	if (virtio_gpu_tgsi_get_current_shader_data(pShaderDataInfo, shaderDataType, &pShaderData)!=0){
		printf("failed to get current virtual I/O GPU host controller string-form TGSI shader physical page data\r\n");
		mutex_unlock(&mutex);	
		return -1;
	}
	if (virtio_gpu_tgsi_get_current_shader_data_size(pShaderDataInfo, shaderDataType, &shaderDataSize)!=0){
		printf("failed to get current virtual I/O GPU host controller string-form TGSI shader physical page data length\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t shaderDataLength = 0x00;
	if (utoa64(value, pShaderData, shaderDataSize, &shaderDataLength)!=0){
		printf("failed to convert virtual I/O GPU host controller unsigned QWORD into shader physical page data\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtio_gpu_tgsi_shader_data_push(pShaderDataInfo, shaderDataType, shaderDataLength)!=0){
		printf("failed to push virtual I/O GPU host controller shader physical page data\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_data_push_i64(struct virtio_gpu_shader_data_info* pShaderDataInfo, uint8_t shaderDataType, int64_t value){
	if (!pShaderDataInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	unsigned char* pShaderData = (unsigned char*)0x00;
	uint64_t shaderDataSize = 0x00;
	if (virtio_gpu_tgsi_get_current_shader_data(pShaderDataInfo, shaderDataType, &pShaderData)!=0){
		printf("failed to get current virtual I/O GPU host controller shader physical page data\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtio_gpu_tgsi_get_current_shader_data_size(pShaderDataInfo, shaderDataType, &shaderDataSize)!=0){
		printf("failed to get current virtual I/O GPU host controller shader physical page data size\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t shaderDataLength = 0x00;
	if (itoa64(value, pShaderData, shaderDataSize, &shaderDataLength)!=0){
		printf("failed to convert virtual I/O GPU host controller signed QWORD into shader physical page data\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtio_gpu_tgsi_shader_data_push(pShaderDataInfo, shaderDataType, shaderDataLength)!=0){
		printf("failed to push virtual I/O GPU host controller shader physical page data\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_data_push_tag_list(struct virtio_gpu_shader_data_info* pShaderDataInfo, uint8_t shaderDataType, struct gpu_tag_list_info tagListInfo){
	if (!pShaderDataInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	for (uint64_t i = 0;i<tagListInfo.tagCount;i++){
		struct gpu_tag_list_entry* pTagListEntry = ((struct gpu_tag_list_entry*)tagListInfo.tagList)+i;
		unsigned char* pTagTypeName = (unsigned char*)"Unknown";
		virtio_gpu_tgsi_tag_type_get_name(pTagListEntry->tagType, (const unsigned char**)&pTagTypeName);
		virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, shaderDataType, pTagTypeName, 0x00);
		if (pTagListEntry->tagIndex){
			virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, shaderDataType, "[", 0x01);
			virtio_gpu_tgsi_shader_data_push_u64(pShaderDataInfo, shaderDataType, pTagListEntry->tagIndex-0x01);
			virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, shaderDataType, "]", 0x01);
		}
		if (i==tagListInfo.tagCount-0x01)
			continue;
		virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, shaderDataType, ", ", 0x02);
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_data_push_declare(struct virtio_gpu_shader_data_info* pShaderDataInfo, uint8_t shaderDataType, struct gpu_declare_location_info declareLocationInfo, uint64_t vectorCount){
	if (!pShaderDataInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	unsigned char* pDeclareTypeName = (unsigned char*)0x00;
	virtio_gpu_tgsi_declare_type_get_name(declareLocationInfo.declareType, (const unsigned char**)&pDeclareTypeName);
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, shaderDataType, pDeclareTypeName, 0x00);
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, shaderDataType, "[", 0x01);
	virtio_gpu_tgsi_shader_data_push_u64(pShaderDataInfo, shaderDataType, declareLocationInfo.declareId);
	if (vectorCount!=0x01){
		virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, shaderDataType, "..", 0x02);
		virtio_gpu_tgsi_shader_data_push_u64(pShaderDataInfo, shaderDataType, declareLocationInfo.declareId+vectorCount-0x01);
	}
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, shaderDataType, "]", 0x01);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_data_push_swizzle(struct virtio_gpu_shader_data_info* pShaderDataInfo, uint8_t shaderDataType, struct gpu_swizzle swizzle, uint8_t operandIndex){
	if (!pShaderDataInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	static const unsigned char swizzleNameList[256]={
		[GPU_SWIZZLE_RED]='x',
		[GPU_SWIZZLE_GREEN]='y',
		[GPU_SWIZZLE_BLUE]='z',
		[GPU_SWIZZLE_ALPHA]='w',
	};
	unsigned char* pShaderData = (unsigned char*)0x00;
	if (virtio_gpu_tgsi_get_current_shader_data(pShaderDataInfo, shaderDataType, &pShaderData)!=0){
		printf("failed to get current virtual I/O GPU host controller shader physical page data\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!*(uint32_t*)swizzle.swizzleList)
		*(uint32_t*)swizzle.swizzleList = 0x04030201;
	uint64_t swizzleCount = 0x00;
	uint8_t lastSwizzleEntry = 0x00;
	*pShaderData = '.';
	pShaderData++;
	for (swizzleCount = 0x00;swizzleCount<0x04;swizzleCount++){
		uint8_t swizzleEntry = swizzle.swizzleList[swizzleCount];
		if (!operandIndex&&!swizzleEntry)
			break;
		swizzleEntry = !swizzleEntry ? lastSwizzleEntry : swizzleEntry;
		*(pShaderData+swizzleCount) = swizzleNameList[swizzleEntry];
		lastSwizzleEntry = swizzleEntry;
	}
	uint64_t shaderDataLength = swizzleCount+0x01;
	if (virtio_gpu_tgsi_shader_data_push(pShaderDataInfo, shaderDataType, shaderDataLength)!=0){
		printf("failed to push virtual I/O GPU host controller shader physical page data\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_instruction_declare(struct gpu_instruction_info* pInstructionInfo, struct virtio_gpu_shader_data_info* pShaderDataInfo){
	if (!pInstructionInfo||!pShaderDataInfo)
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
			virtio_gpu_tgsi_shader_data_push_declare(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_DECLARE, pDeclareInfo->declareLocation, pDeclareInfo->vectorCount);
			virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_DECLARE, " ", 0x01);
			virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_DECLARE, pScalarTypeName, 0x00);
			virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_DECLARE, pScalarLengthName, 0x00);
			virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_DECLARE, " { ", 0x03);
			for (uint64_t i = 0;i<0x04;i++){
				unsigned char* pScalarName = " ";
				if (pDeclareInfo->scalarType==GPU_SHADER_SCALAR_TYPE_FLOAT){
					pScalarName = (i==0x03) ? "1.0" : "0.0";
				}
				if (pDeclareInfo->scalarType!=GPU_SHADER_SCALAR_TYPE_FLOAT){
					pScalarName = (i==0x03) ? "1" : "0";
				}
				pScalarName = (i<pDeclareInfo->scalarCount) ? *(pDeclareInfo->scalarIdentList+i) : pScalarName;
				uint64_t scalarNameLength = (pDeclareInfo->scalarType==GPU_SHADER_SCALAR_TYPE_FLOAT) ? 0x03 : 0x01;
				scalarNameLength = (i<pDeclareInfo->scalarCount) ? *(pDeclareInfo->scalarLengthList+i) : scalarNameLength;
				virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_DECLARE, pScalarName, scalarNameLength);
				virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_DECLARE, (i>=0x03) ? " }\n" : ", ", 0x00);
			}
			break;				       
		}
		case GPU_SHADER_DECLARE_TYPE_TEMP:{
		
			break;				  
		}
		default:{
			virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_DECLARE, "DCL ", 0x04);
			virtio_gpu_tgsi_shader_data_push_declare(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_DECLARE, pDeclareInfo->declareLocation, pDeclareInfo->vectorCount);
			if (!pDeclareInfo->tagListInfo.tagCount){
				virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_DECLARE, "\n", 0x01);
				break;	
			}
			virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_DECLARE, ", ", 0x02);
			virtio_gpu_tgsi_shader_data_push_tag_list(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_DECLARE, pDeclareInfo->tagListInfo);
			virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_DECLARE, "\n", 0x01);
			break;
		}
	}
	*(pShaderDataInfo->declareCountList+pDeclareInfo->declareLocation.declareType-0x01) = pDeclareInfo->declareLocation.declareId+0x01;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_instruction_mov(struct gpu_instruction_info_mov* pInstructionInfo, struct virtio_gpu_shader_data_info* pShaderDataInfo){
	if (!pInstructionInfo||!pShaderDataInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_operand_info* pOperandInfoList = (struct gpu_operand_info*)pInstructionInfo->operandInfoList;
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, "MOV ", 0x04);
	virtio_gpu_tgsi_shader_data_push_declare(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, (pOperandInfoList+0x00)->declareLocationInfo, 0x01);
	virtio_gpu_tgsi_shader_data_push_swizzle(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, (pOperandInfoList+0x00)->swizzle, 0x00);
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, ", ", 0x02);
	virtio_gpu_tgsi_shader_data_push_declare(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, (pOperandInfoList+0x01)->declareLocationInfo, 0x01);
	virtio_gpu_tgsi_shader_data_push_swizzle(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, (pOperandInfoList+0x01)->swizzle, 0x01);
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, "\n", 0x01);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_instruction_method_dp(struct gpu_instruction_info_method_dp* pInstructionInfo, struct virtio_gpu_shader_data_info* pShaderDataInfo){
	if (!pInstructionInfo||!pShaderDataInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_operand_info* pOperandInfoList = (struct gpu_operand_info*)pInstructionInfo->operandInfoList;
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, "DP", 0x02);
	virtio_gpu_tgsi_shader_data_push_u64(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, pInstructionInfo->scalarCount);
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, " ", 0x01);
	virtio_gpu_tgsi_shader_data_push_declare(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, (pOperandInfoList+0x00)->declareLocationInfo, 0x01);
	virtio_gpu_tgsi_shader_data_push_swizzle(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, (pOperandInfoList+0x00)->swizzle, 0x00);
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, ", ", 0x02);
	virtio_gpu_tgsi_shader_data_push_declare(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, (pOperandInfoList+0x01)->declareLocationInfo, 0x01);
	virtio_gpu_tgsi_shader_data_push_swizzle(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, (pOperandInfoList+0x01)->swizzle, 0x01);
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, ", ", 0x02);
	virtio_gpu_tgsi_shader_data_push_declare(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, (pOperandInfoList+0x02)->declareLocationInfo, 0x01);
	virtio_gpu_tgsi_shader_data_push_swizzle(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, (pOperandInfoList+0x02)->swizzle, 0x02);
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, "\n", 0x01);
	mutex_unlock(&mutex);	
	return 0;
}
int virtio_gpu_tgsi_shader_instruction_method_tex(struct gpu_instruction_info_method* pInstructionInfo, struct virtio_gpu_shader_data_info* pShaderDataInfo){
	if (!pInstructionInfo||!pShaderDataInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_tag_list_info tagListInfo = pInstructionInfo->tagListInfo;
	struct gpu_operand_info* pOperandInfoList = (struct gpu_operand_info*)pInstructionInfo->operandInfoList;
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, "TEX ", 0x04);
	virtio_gpu_tgsi_shader_data_push_declare(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, pOperandInfoList->declareLocationInfo, 0x01);
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, ", ", 0x02);
	virtio_gpu_tgsi_shader_data_push_declare(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, (pOperandInfoList+0x01)->declareLocationInfo, 0x01);
	virtio_gpu_tgsi_shader_data_push_swizzle(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, (pOperandInfoList+0x01)->swizzle, 0x00);
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, ", ", 0x02);
	virtio_gpu_tgsi_shader_data_push_declare(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, (pOperandInfoList+0x02)->declareLocationInfo, 0x01);
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, ", ", 0x02);
	virtio_gpu_tgsi_shader_data_push_tag_list(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, tagListInfo);
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, "\n", 0x01);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_instruction_method(struct gpu_instruction_info_method* pInstructionInfo, struct virtio_gpu_shader_data_info* pShaderDataInfo){
	if (!pInstructionInfo||!pShaderDataInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_tag_list_info tagListInfo = pInstructionInfo->tagListInfo;
	struct gpu_operand_info* pOperandInfoList = (struct gpu_operand_info*)pInstructionInfo->operandInfoList;
	static const virtioGpuShaderInstructionFunc methodInstructionFuncList[256]={
		[GPU_SHADER_METHOD_TYPE_DP]=(virtioGpuShaderInstructionFunc)virtio_gpu_tgsi_shader_instruction_method_dp,
		[GPU_SHADER_METHOD_TYPE_TEX]=(virtioGpuShaderInstructionFunc)virtio_gpu_tgsi_shader_instruction_method_tex,
	};
	virtioGpuShaderInstructionFunc instructionFunc = methodInstructionFuncList[pInstructionInfo->methodType];
	if (instructionFunc&&instructionFunc((struct gpu_instruction_info*)pInstructionInfo, pShaderDataInfo)!=0){
		mutex_unlock(&mutex);	
		return -1;
	}
	if (instructionFunc){
		mutex_unlock(&mutex);
		return 0;
	}
	unsigned char* pMethodTypeName = "Unknown method type\r\n";
	if (virtio_gpu_tgsi_method_type_get_name(pInstructionInfo->methodType, (const unsigned char**)&pMethodTypeName)!=0){
		printf("failed to get GPU host controller method type name\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, pMethodTypeName, 0x00);
	for (uint64_t i = 0x00;i<pInstructionInfo->operandCount;i++){
		if (!i)
			virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, " ", 0x01);
		struct gpu_operand_info* pOperandInfo = ((struct gpu_operand_info*)pInstructionInfo->operandInfoList)+i;
		virtio_gpu_tgsi_shader_data_push_declare(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, pOperandInfo->declareLocationInfo, 0x01);
		virtio_gpu_tgsi_shader_data_push_swizzle(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, pOperandInfo->swizzle, i);
		if (i!=pInstructionInfo->operandCount-0x01)
			virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, ", ", 0x02);
	}
	if (tagListInfo.tagCount){
		virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, ", ", 0x02);
		virtio_gpu_tgsi_shader_data_push_tag_list(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, tagListInfo);
	}
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, "\n", 0x01);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_instruction_kill(struct gpu_instruction_info_kill* pInstructionInfo, struct virtio_gpu_shader_data_info* pShaderDataInfo){
	if (!pInstructionInfo||!pShaderDataInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, "KILL\n", 0x05);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_instruction_if(struct gpu_instruction_info_if* pInstructionInfo, struct virtio_gpu_shader_data_info* pShaderDataInfo){
	if (!pInstructionInfo||!pShaderDataInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, "IF ", 0x03);
	virtio_gpu_tgsi_shader_data_push_declare(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, pInstructionInfo->operandInfo.declareLocationInfo, 0x01);
	virtio_gpu_tgsi_shader_data_push_swizzle(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, pInstructionInfo->operandInfo.swizzle, 0x00);
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, "\n", 0x01);	
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_instruction_else(struct gpu_instruction_info_else* pInstructionInfo, struct virtio_gpu_shader_data_info* pShaderDataInfo){
	if (!pInstructionInfo||!pShaderDataInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, "ELSE\n", 0x05);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_instruction_endif(struct gpu_instruction_info_endif* pInstructionInfo, struct virtio_gpu_shader_data_info* pShaderDataInfo){
	if (!pInstructionInfo||!pShaderDataInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, "ENDIF\n", 0x06);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_tgsi_shader_translate(uint64_t gpuId, uint64_t contextId, uint64_t objectId, struct gpu_create_shader_object_info* pCreateObjectInfo, struct virtio_gpu_shader_data_info* pShaderDataInfo){
	if (!pCreateObjectInfo||!pShaderDataInfo)
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
	unsigned char* pInstructionList = (unsigned char*)0x00;
	uint64_t instructionListSize = PAGE_SIZE*0x40;
	if (virtualAlloc((uint64_t*)&pInstructionList, instructionListSize, PTE_RW|PTE_NX, MAP_FLAG_LAZY, PAGE_TYPE_NORMAL)!=0){
		printf("failed to lazy allocate physical pages for GPU host controller instruction info descriptor list\r\n");
		mutex_unlock(&mutex);	
		return -1;
	}
	unsigned char* pShaderTypeName = (unsigned char*)"Unknown";
	virtio_gpu_tgsi_shader_type_get_name(pCreateObjectInfo->shaderType, (const unsigned char**)&pShaderTypeName);
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_DECLARE, pShaderTypeName, 0x00);
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_DECLARE, "\n", 0x01);
	uint64_t instructionCount = 0x00;
	struct gpu_get_instruction_info getInstructionInfo = {0};
	memset((void*)&getInstructionInfo, 0, sizeof(struct gpu_get_instruction_info));
	getInstructionInfo.pInstructionList = pInstructionList;
	getInstructionInfo.instructionListLength = 0x00;
	getInstructionInfo.instructionListSize = instructionListSize;
	for (instructionCount = 0x00;;instructionCount++){
		struct gpu_instruction_info* pCurrentInstruction = (struct gpu_instruction_info*)(pInstructionList+getInstructionInfo.instructionListLength);
		getInstructionInfo.instructionCount = 0x00;
		if (gpu_instruction_get_info(gpuId, contextId, objectId, &getInstructionInfo)!=0)
			break;
		for (uint64_t i = 0;i<getInstructionInfo.instructionCount;i++){
			static const virtioGpuShaderInstructionFunc instructionFuncList[256]={
				[GPU_SHADER_OPCODE_DECLARE]=(virtioGpuShaderInstructionFunc)virtio_gpu_tgsi_shader_instruction_declare,
				[GPU_SHADER_OPCODE_MOV]=(virtioGpuShaderInstructionFunc)virtio_gpu_tgsi_shader_instruction_mov,
				[GPU_SHADER_OPCODE_METHOD]=(virtioGpuShaderInstructionFunc)virtio_gpu_tgsi_shader_instruction_method,
				[GPU_SHADER_OPCODE_KILL]=(virtioGpuShaderInstructionFunc)virtio_gpu_tgsi_shader_instruction_kill,
				[GPU_SHADER_OPCODE_IF]=(virtioGpuShaderInstructionFunc)virtio_gpu_tgsi_shader_instruction_if,
				[GPU_SHADER_OPCODE_ELSE]=(virtioGpuShaderInstructionFunc)virtio_gpu_tgsi_shader_instruction_else,
				[GPU_SHADER_OPCODE_ENDIF]=(virtioGpuShaderInstructionFunc)virtio_gpu_tgsi_shader_instruction_endif,
			};	
			virtioGpuShaderInstructionFunc instructionFunc = (virtioGpuShaderInstructionFunc)instructionFuncList[pCurrentInstruction->opcode];
			if (!instructionFunc){
				printf("unexpected GPU host controller instruction info descriptor\r\n");
				mutex_unlock(&mutex);
				return -1;
			}
			uint64_t startTime = get_time_us();
			if (instructionFunc(pCurrentInstruction, pShaderDataInfo)!=0){
				printf("failed to translate GPU host controller instruction info descriptor of opcode 0x%x into string-form TGSI shader code\r\n", pCurrentInstruction->opcode);
				mutex_unlock(&mutex);	
				return -1;
			}
			uint64_t elapsedTime = get_time_us()-startTime;
		//	printf("GPU host controller instruction info descriptor translation time: %fms\r\n", ((double)elapsedTime)/1000.0);
			pCurrentInstruction = (struct gpu_instruction_info*)(((unsigned char*)pCurrentInstruction)+pCurrentInstruction->length);
		}
	}
	if (*(pShaderDataInfo->declareCountList+GPU_SHADER_DECLARE_TYPE_TEMP-0x01)){
		struct gpu_declare_location_info declareLocationInfo = {0};
		memset((void*)&declareLocationInfo, 0, sizeof(struct gpu_declare_location_info));
		declareLocationInfo.declareType = GPU_SHADER_DECLARE_TYPE_TEMP;
		declareLocationInfo.declareId = 0x00;
		uint64_t vectorCount = *(pShaderDataInfo->declareCountList+GPU_SHADER_DECLARE_TYPE_TEMP-0x01);
		virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_DECLARE, "DCL ", 0x04);
		virtio_gpu_tgsi_shader_data_push_declare(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_DECLARE, declareLocationInfo, vectorCount);
		virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_DECLARE, "\n", 0x01);
	}
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, "END\n", 0x04);
	virtio_gpu_tgsi_shader_data_push_string(pShaderDataInfo, VIRTIO_GPU_TGSI_SHADER_DATA_TYPE_CODE, " "+0x01, 0x01);
	mutex_unlock(&mutex);
	return 0;
}
