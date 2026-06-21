#include "mem/pmm.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "cpu/mutex.h"
#include "stdlib/stdlib.h"
#include "subsystem/text.h"
#include "align.h"
#include "math/basic.h"
#include "math/trig.h"
#include "drivers/timer.h"
#include "drivers/fonts/unicode.h"
#include "drivers/fonts/macintosh.h"
#include "drivers/fonts/windows.h"
#include "drivers/fonts/ttf.h"
static struct font_driver_ttf_info fontDriverInfo = {0};
int font_driver_ttf_init(void){
	struct text_subsystem_font_driver_vtable vtable = {0};
	memset((void*)&vtable, 0, sizeof(struct text_subsystem_font_driver_vtable));
	vtable.fontDriverDeinit = (fontDriverDeinitFunc)font_driver_ttf_deinit;
	vtable.fontLoad = (fontDriverFontLoadFunc)ttf_font_load;
	vtable.fontUnload = (fontDriverFontUnloadFunc)ttf_font_unload;
	vtable.fontVerify = (fontDriverFontVerifyFunc)ttf_font_verify;
	vtable.fontGlyphGetId = (fontDriverFontGlyphGetIdFunc)ttf_font_glyph_get_id;
	vtable.fontGlyphTesselate = (fontDriverFontGlyphTesselateFunc)ttf_font_glyph_tesselate;
	struct text_subsystem_font_driver_info subsystemFontDriverInfo = {0};
	memset((void*)&subsystemFontDriverInfo, 0, sizeof(struct text_subsystem_font_driver_info));
	subsystemFontDriverInfo.vtable = vtable;
	if (text_subsystem_font_driver_register(&subsystemFontDriverInfo)!=0){
		printf("failed to register GPU host controller TTF font driver with text subsystem\r\n");
		return -1;
	}
	struct subsystem_desc* pBytecodeSubsystemDesc = (struct subsystem_desc*)0x00;
	if (subsystem_init(&pBytecodeSubsystemDesc, TTF_MAX_BYTECODE_CONTEXT_COUNT)!=0){
		printf("failed to initialize true type bytecode context subsystem descriptor\r\n");
		text_subsystem_font_driver_unregister(subsystemFontDriverInfo.fontDriverId);
		return -1;
	}
	fontDriverInfo.subsystemFontDriverInfo = subsystemFontDriverInfo;
	fontDriverInfo.bytecodeSubsystemInfo.pSubsystemDesc = pBytecodeSubsystemDesc;
	fontDriverInfo.bytecodeSubsystemInfo.maxBytecodeContextCount = TTF_MAX_BYTECODE_CONTEXT_COUNT;
	return 0;
}
int font_driver_ttf_deinit(void){
	if (fontDriverInfo.bytecodeSubsystemInfo.pSubsystemDesc)
		subsystem_deinit(fontDriverInfo.bytecodeSubsystemInfo.pSubsystemDesc);	
	return 0;
}
int ttf_get_checksum(struct ttf_font_desc* pFontDesc, uint32_t* pChecksum){
	if (!pFontDesc||!pChecksum)
		return -1;
	uint32_t checksum = 0x00;
	for (uint64_t i = 0;i<pFontDesc->fontBufferSize/sizeof(uint32_t);i++){
		uint32_t value = __builtin_bswap32(*(((uint32_t*)pFontDesc->pFontBuffer)+i));
		checksum+=value;
	}
	checksum = TTF_CHECKSUM_MAGIC-((uint64_t)checksum%((uint64_t)1<<32));
	*pChecksum = checksum;
	return 0;
}
int ttf_table_get_desc(struct ttf_font_desc* pFontDesc, uint64_t tableId, struct ttf_table_desc** ppTableDesc){
	if (!pFontDesc||!ppTableDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct ttf_table_desc* pTableDesc = pFontDesc->tableDescList[tableId];
	if (!pTableDesc){
		mutex_unlock(&mutex);
		return -1;
	}
	*ppTableDesc = pTableDesc;
	mutex_unlock(&mutex);
	return 0;
}
int ttf_control_get_value(struct ttf_font_desc* pFontDesc, uint32_t controlValueIndex, double* pControlValue){
	if (!pFontDesc||!pControlValue)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct ttf_table_desc* pCvtTableDesc = (struct ttf_table_desc*)0x00;
	if (ttf_table_get_desc(pFontDesc, TTF_TABLE_ID_CVT, &pCvtTableDesc)!=0){
		printf("failed to get true type control value table descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	uint32_t* pCvtTable = (uint32_t*)(pFontDesc->pFontBuffer+__builtin_bswap32(pCvtTableDesc->table_offset));
	uint32_t controlValue = __builtin_bswap32(pCvtTable[controlValueIndex]);
	int32_t controlValueInteger = (int32_t)(controlValue>>0x06);
	uint8_t controlValueFraction = (uint8_t)(controlValue&0x3F);
	double controlValueFloat = (double)controlValueInteger;
	controlValueFloat+=((((double)controlValueFraction)/64.0)*(absf(controlValueFloat)/controlValueFloat));
	*pControlValue = controlValueFloat;
	mutex_unlock(&mutex);
	return 0;
}
int ttf_opcode_get_name(uint8_t opcode, const unsigned char** ppOpcodeName){
	if (!ppOpcodeName)
		return -1;
	static const unsigned char* opcodeNameList[255]={
		[TTF_OPCODE_NPUSHB]="NPUSHB",            [TTF_OPCODE_NPUSHW]="NPUSHW",            [TTF_OPCODE_PUSHB]="PUSHB",
	     	[TTF_OPCODE_PUSHW]="PUSHW",

		[TTF_OPCODE_AA]="AA",                    [TTF_OPCODE_ABS]="ABS",	          [TTF_OPCODE_ADD]="ADD",
	 	[TTF_OPCODE_ALIGNPTS]="ALIGNPTS",         [TTF_OPCODE_ALIGNRP]="ALIGNRP",           [TTF_OPCODE_AND]="AND",
		
		[TTF_OPCODE_CALL]="CALL",                [TTF_OPCODE_CEILING]="CEILING",          [TTF_OPCODE_CINDEX]="CINDEX",
	       	[TTF_OPCODE_CLEAR]="CLEAR",

		[TTF_OPCODE_DEBUG]="DEBUG",              [TTF_OPCODE_DELTAC1]="DELTAC1",          [TTF_OPCODE_DELTAC2]="DELTAC2",
	       	[TTF_OPCODE_DELTAC3]="DELTAC3",          [TTF_OPCODE_DELTAP1]="DELTAP1",          [TTF_OPCODE_DELTAP2]="DELTAP2",
		[TTF_OPCODE_DELTAP3]="DELTAP3",          [TTF_OPCODE_DEPTH]="DEPTH",              [TTF_OPCODE_DIV]="DIV",
	       	[TTF_OPCODE_DUP]="DUP",

		[TTF_OPCODE_EIF]="EIF",                  [TTF_OPCODE_ELSE]="ELSE",                [TTF_OPCODE_ENDF]="ENDF", 
		[TTF_OPCODE_EQ]="EQ",                    [TTF_OPCODE_EVEN]="EVEN", 
		
		[TTF_OPCODE_FDEF]="FDEF",                [TTF_OPCODE_FLIP_OFF]="FLIPOFF",         [TTF_OPCODE_FLIP_ON]="FLIPON",
		[TTF_OPCODE_FLIP_PT]="FLIPPT",           [TTF_OPCODE_FLIP_PRGOFF]="FLIPPRGOFF",   [TTF_OPCODE_FLIP_PRGON]="FLIPPRGON", 
		[TTF_OPCODE_FLOOR]="FLOOR",

		[TTF_OPCODE_GC]="GC",                    [TTF_OPCODE_GETINFO]="GETINFO",          [TTF_OPCODE_GFV]="GFV", 
		[TTF_OPCODE_GPV]="GPV",                  [TTF_OPCODE_GT]="GT",                    [TTF_OPCODE_GTEQ]="GTEQ",
		[TTF_OPCODE_IDEF]="IDEF",                [TTF_OPCODE_IF]="IF",
		[TTF_OPCODE_INSTCTRL]="INSTRCTRL",       [TTF_OPCODE_IP]="IP",                    [TTF_OPCODE_ISECT]="ISECT", 
		[TTF_OPCODE_IUP]="IUP",

		[TTF_OPCODE_JMPR]="JMPR",                [TTF_OPCODE_JROF]="JROF",                [TTF_OPCODE_JROT]="JROT",
	       
		[TTF_OPCODE_LOOPCALL]="LOOPCALL",        [TTF_OPCODE_LT]="LT",                    [TTF_OPCODE_LTEQ]="LTEQ",

		[TTF_OPCODE_MAX]="MAX",                  [TTF_OPCODE_MD]="MD",                    [TTF_OPCODE_MDAP]="MDAP",
		[TTF_OPCODE_MDRP]="MDRP",                [TTF_OPCODE_MIAP]="MIAP",                [TTF_OPCODE_MIN]="MIN",
		[TTF_OPCODE_MINDEX]="MINDEX",        [TTF_OPCODE_MIRP]="MIRP",                [TTF_OPCODE_MPPEM]="MPPEM",
		[TTF_OPCODE_MPS]="MPS",                  [TTF_OPCODE_MSIRP]="MSIRP",              [TTF_OPCODE_MUL]="MUL",

		[TTF_OPCODE_NEG]="NEG",                  [TTF_OPCODE_NEQ]="NEQ",                  [TTF_OPCODE_NOT]="NOT",
		[TTF_OPCODE_NROUND]="NROUND",            
	
		[TTF_OPCODE_ODD]="ODD",                  [TTF_OPCODE_OR]="OR",
		
		[TTF_OPCODE_POP]="POP",                  
		
		[TTF_OPCODE_RCVT]="RCVT",                [TTF_OPCODE_RDTG]="RDTG",                [TTF_OPCODE_ROFF]="ROFF",
		[TTF_OPCODE_ROLL]="ROLL",                [TTF_OPCODE_ROUND]="ROUND",              [TTF_OPCODE_RS]="RS",
		[TTF_OPCODE_RTDG]="RTDG",                [TTF_OPCODE_RTG]="RTG",                  [TTF_OPCODE_RTHG]="RTHG",
		[TTF_OPCODE_RUTG]="RUTG",

		[TTF_OPCODE_S45ROUND]="S45ROUND",        [TTF_OPCODE_SANGW]="SANGW",              [TTF_OPCODE_SCANCTRL]="SCANCTRL",
		[TTF_OPCODE_SCANTYPE]="SCANTYPE",        [TTF_OPCODE_SCFS]="SCFS",                [TTF_OPCODE_SCVTCI]="SCVTCI",
		[TTF_OPCODE_SDB]="SDB",                  [TTF_OPCODE_SDPVTL]="SDPVTL",            [TTF_OPCODE_SDS]="SDS",
		[TTF_OPCODE_SFVFS]="SFVFS",              [TTF_OPCODE_SFVTCA]="SFVTCA",            [TTF_OPCODE_SFVTL]="SFVTL",
		[TTF_OPCODE_SFVTPV]="SFVTPV",            [TTF_OPCODE_SHC]="SHC",                  [TTF_OPCODE_SHP]="SHP",
		[TTF_OPCODE_SHPIX]="SHPIX",              [TTF_OPCODE_SHZ]="SHZ",                  [TTF_OPCODE_SLOOP]="SLOOP",
		[TTF_OPCODE_SMD]="SMD",                  [TTF_OPCODE_SPVFS]="SPVFS",              [TTF_OPCODE_SPVTCA]="SPVTCA",
		[TTF_OPCODE_SPVTL]="SPVTL",              [TTF_OPCODE_SROUND]="SROUND",            [TTF_OPCODE_SRP0]="SRP0",
		[TTF_OPCODE_SRP1]="SRP1",                [TTF_OPCODE_SRP2]="SRP2",                [TTF_OPCODE_SSW]="SSW",
		[TTF_OPCODE_SSWCI]="SSWCI",              [TTF_OPCODE_SUB]="SUB",                  [TTF_OPCODE_SVTCA]="SVTCA",
		[TTF_OPCODE_SWAP]="SWAP",                [TTF_OPCODE_SZP0]="SZP0",                [TTF_OPCODE_SZP1]="SZP1",
		[TTF_OPCODE_SZP2]="SZP2",                [TTF_OPCODE_SZPS]="SZPS",                [TTF_OPCODE_UTP]="UTP",
		[TTF_OPCODE_WCVTF]="WCVTF",              [TTF_OPCODE_WCVTP]="WCVTP",              [TTF_OPCODE_WS]="WS",
	};
	const unsigned char* pOpcodeName = opcodeNameList[opcode];
	pOpcodeName = (!pOpcodeName) ? (const unsigned char*)"Unknown true type opcode" : pOpcodeName;
	*ppOpcodeName = pOpcodeName;
	return 0;
}
int ttf_status_code_get_name(uint16_t statusCode, const unsigned char** ppStatusCodeName){
	if (!ppStatusCodeName)
		return -1;
	static const unsigned char* statusCodeNameList[]={
		[TTF_STATUS_CODE_UNKNOWN]="Unknown",
		[TTF_STATUS_CODE_SUCCESS]="Success",
		[TTF_STATUS_CODE_OPERATION_FAILURE]="Operation failure",
		[TTF_STATUS_CODE_OPERATION_UNSUPPORTED]="Operation unsupported",
	};
	const unsigned char* pStatusCodeName = (const unsigned char*)"Unknown status code name";
	if (statusCode<(sizeof(statusCodeNameList)/sizeof(const unsigned char*)))
		pStatusCodeName = statusCodeNameList[statusCode];
	*ppStatusCodeName = pStatusCodeName;
	return 0;
}
int ttf_bytecode_context_init(struct ttf_font_desc* pFontDesc, uint64_t stackSize, uint64_t* pBytecodeContextId){
	if (!pFontDesc||!pBytecodeContextId)
		return -1;
	if (!stackSize)
		stackSize = TTF_DEFAULT_BYTECODE_STACK_SIZE;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct ttf_bytecode_context_desc* pBytecodeContextDesc = (struct ttf_bytecode_context_desc*)kmalloc(sizeof(struct ttf_bytecode_context_desc));
	if (!pBytecodeContextDesc){
		printf("failed to allocate true type bytecode context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pBytecodeContextDesc, 0, sizeof(struct ttf_bytecode_context_desc));
	pBytecodeContextDesc->pFontDesc = pFontDesc;
	uint8_t* pStack = (uint8_t*)0x00;
	if (virtualAlloc((uint64_t*)&pStack, stackSize, PTE_RW|PTE_NX, MAP_FLAG_LAZY, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate true type bytecode context descriptor execution stack physical pages\r\n");
		kfree((void*)pBytecodeContextDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	uint8_t** pReturnStack = (uint8_t**)0x00;
	uint64_t returnStackSize = PAGE_SIZE;
	if (virtualAlloc((uint64_t*)&pReturnStack, returnStackSize, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate true type bytecode context descriptor return stack physical pages\r\n");
		virtualFree((uint64_t)pStack, stackSize);
		kfree((void*)pBytecodeContextDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	struct ttf_bytecode_function_desc* pFunctionDescList = (struct ttf_bytecode_function_desc*)0x00;
	uint64_t functionDescListSize = sizeof(struct ttf_bytecode_function_desc)*TTF_MAX_BYTECODE_FUNCTION_COUNT;
	if (virtualAlloc((uint64_t*)&pFunctionDescList, functionDescListSize, PTE_RW|PTE_NX, MAP_FLAG_LAZY, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate physical pages for true type bytecode context descriptor function descriptor list\r\n");
		virtualFree((uint64_t)pReturnStack, returnStackSize);
		virtualFree((uint64_t)pStack, stackSize);
		kfree((void*)pBytecodeContextDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t bytecodeContextId = 0x00;
	if (subsystem_alloc_entry(fontDriverInfo.bytecodeSubsystemInfo.pSubsystemDesc, (unsigned char*)pBytecodeContextDesc, &bytecodeContextId)!=0){
		printf("failed to allocate true type bytecode context subsystem descriptor entry\r\n");
		virtualFree((uint64_t)pStack, stackSize);
		kfree((void*)pBytecodeContextDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	pBytecodeContextDesc->pStack = pStack;
	pBytecodeContextDesc->pStackEntry = pStack;
	pBytecodeContextDesc->stackSize = stackSize;
	pBytecodeContextDesc->pReturnStack = pReturnStack;
	pBytecodeContextDesc->pReturnStackEntry = pReturnStack;
	pBytecodeContextDesc->returnStackSize = returnStackSize;
	pBytecodeContextDesc->pFunctionDescList = pFunctionDescList;
	pBytecodeContextDesc->functionDescListSize = functionDescListSize;
	pBytecodeContextDesc->bytecodeContextId = bytecodeContextId;
	*pBytecodeContextId = bytecodeContextId;
	mutex_unlock(&mutex);
	return 0;
}
int ttf_bytecode_context_deinit(uint64_t bytecodeContextId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct ttf_bytecode_context_desc* pBytecodeContextDesc = (struct ttf_bytecode_context_desc*)0x00;
	if (subsystem_get_entry(fontDriverInfo.bytecodeSubsystemInfo.pSubsystemDesc, bytecodeContextId, (uint64_t*)&pBytecodeContextDesc)!=0){
		printf("failed to get true type bytecode context subsystem descriptor entry\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pBytecodeContextDesc->pStack)
		virtualFree((uint64_t)pBytecodeContextDesc->pStack, pBytecodeContextDesc->stackSize);
	if (pBytecodeContextDesc->pReturnStack)
		virtualFree((uint64_t)pBytecodeContextDesc->pReturnStack, pBytecodeContextDesc->returnStackSize);
	if (pBytecodeContextDesc->pFunctionDescList)
		virtualFree((uint64_t)pBytecodeContextDesc->pFunctionDescList, pBytecodeContextDesc->functionDescListSize);
	kfree((void*)pBytecodeContextDesc);
	subsystem_free_entry(fontDriverInfo.bytecodeSubsystemInfo.pSubsystemDesc, bytecodeContextId);
	mutex_unlock(&mutex);
	return 0;
}
int ttf_bytecode_context_get_desc(uint64_t bytecodeContextId, struct ttf_bytecode_context_desc** ppBytecodeContextDesc){
	if (!ppBytecodeContextDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct ttf_bytecode_context_desc* pBytecodeContextDesc = (struct ttf_bytecode_context_desc*)0x00;
	if (subsystem_get_entry(fontDriverInfo.bytecodeSubsystemInfo.pSubsystemDesc, bytecodeContextId, (uint64_t*)&pBytecodeContextDesc)!=0){
		printf("failed to get true type bytecode context subsystem entry descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	*ppBytecodeContextDesc = pBytecodeContextDesc;
	mutex_unlock(&mutex);
	return 0;
}
int ttf_bytecode_context_set_bytecode(uint64_t bytecodeContextId, uint8_t* pBytecode, uint64_t bytecodeLength){
	if (!pBytecode||!bytecodeLength)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct ttf_bytecode_context_desc* pBytecodeContextDesc = (struct ttf_bytecode_context_desc*)0x00;
	if (ttf_bytecode_context_get_desc(bytecodeContextId, &pBytecodeContextDesc)!=0){
		printf("failed to get true type bytecode context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	pBytecodeContextDesc->pBytecode = pBytecode;
	pBytecodeContextDesc->bytecodeLength = bytecodeLength;
	mutex_unlock(&mutex);
	return 0;
}
int ttf_bytecode_context_stack_clear(uint64_t bytecodeContextId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct ttf_bytecode_context_desc* pBytecodeContextDesc = (struct ttf_bytecode_context_desc*)0x00;
	if (ttf_bytecode_context_get_desc(bytecodeContextId, &pBytecodeContextDesc)!=0){
		printf("failed to get true type bytecode context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	pBytecodeContextDesc->pStackEntry = pBytecodeContextDesc->pStack;
	mutex_unlock(&mutex);
	return 0;
}
int ttf_bytecode_context_push(uint64_t bytecodeContextId, uint8_t* pStackData, uint64_t stackDataSize){
	if (!pStackData||!stackDataSize)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct ttf_bytecode_context_desc* pBytecodeContextDesc = (struct ttf_bytecode_context_desc*)0x00;
	if (ttf_bytecode_context_get_desc(bytecodeContextId, &pBytecodeContextDesc)!=0){
		printf("failed to get true type bytecode context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memcpy((void*)pBytecodeContextDesc->pStackEntry, (void*)pStackData, stackDataSize);
	pBytecodeContextDesc->pStackEntry+=stackDataSize;
	mutex_unlock(&mutex);
	return 0;
}
int ttf_bytecode_context_pop(uint64_t bytecodeContextId, uint8_t** ppStackData, uint64_t stackDataSize){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct ttf_bytecode_context_desc* pBytecodeContextDesc = (struct ttf_bytecode_context_desc*)0x00;
	if (ttf_bytecode_context_get_desc(bytecodeContextId, &pBytecodeContextDesc)!=0){
		printf("failed to get true type bytecode context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	pBytecodeContextDesc->pStackEntry-=stackDataSize;
	if (ppStackData)
		*ppStackData = pBytecodeContextDesc->pStackEntry;
	mutex_unlock(&mutex);
	return 0;
}
int ttf_bytecode_context_roundf(uint64_t bytecodeContextId, double value, double* pResult){
	if (!pResult)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct ttf_bytecode_context_desc* pBytecodeContextDesc = (struct ttf_bytecode_context_desc*)0x00;
	if (ttf_bytecode_context_get_desc(bytecodeContextId, &pBytecodeContextDesc)!=0){
		printf("failed to get true type bytecode context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	double result = 0.0;
	switch (pBytecodeContextDesc->executionState.currentRoundingOpcode){
		case TTF_OPCODE_RTG:{
			result = roundf(value);
			break;		    
		}
		case TTF_OPCODE_RTHG:{
			result = floorf(value)+0.5;	     
			break;		     
		}
		case TTF_OPCODE_RTDG:{
			result = roundf(value*2.0)*0.5;
			break;	     
		}
		case TTF_OPCODE_RDTG:{
			result = floorf(value);
			break;		     
		}
		case TTF_OPCODE_RUTG:{
			result = ceilf(value);	     
			break;		     
		}
		case TTF_OPCODE_ROFF:{
			result = value;
			break;		     
		}
		default:{
			printf("unsupported rounding opcode: 0x%x\r\n", pBytecodeContextDesc->executionState.currentRoundingOpcode);
			mutex_unlock(&mutex);
			return -1;
		}
	}
	*pResult = result;
	mutex_unlock(&mutex);
	return 0;
}
int ttf_bytecode_context_op_svtca(struct ttf_bytecode_context_desc* pBytecodeContextDesc, uint8_t* pInstructionData, uint8_t argument, struct ttf_status_code* pStatusCode){
	if (!pBytecodeContextDesc||!pInstructionData||!pStatusCode)
		return -1;
	struct ttf_status_code statusCode = {0};
	memset((void*)&statusCode, 0, sizeof(struct ttf_status_code));
	statusCode.code = TTF_STATUS_CODE_SUCCESS;
	statusCode.subCode = TTF_STATUS_CODE_SUCCESS;
	if (pBytecodeContextDesc->executionState.pExecutionInfo->readOnly){
		*pStatusCode = statusCode;
		return 0;
	}
	if (argument==0x01){
		pBytecodeContextDesc->executionState.projectionVector.x = 0.0;
		pBytecodeContextDesc->executionState.projectionVector.y = 1.0;
		pBytecodeContextDesc->executionState.freedomVector.x = 0.0;
		pBytecodeContextDesc->executionState.freedomVector.y = 1.0;
		*pStatusCode = statusCode;
		return 0;
	}	
	pBytecodeContextDesc->executionState.projectionVector.x = 1.0;
	pBytecodeContextDesc->executionState.projectionVector.y = 0.0;
	pBytecodeContextDesc->executionState.freedomVector.x = 1.0;
	pBytecodeContextDesc->executionState.freedomVector.y = 0.0;
	*pStatusCode = statusCode;
	return 0;
}
int ttf_bytecode_context_op_clear(struct ttf_bytecode_context_desc* pBytecodeContextDesc, uint8_t* pInstructionData, uint8_t argument, struct ttf_status_code* pStatusCode){
	if (!pBytecodeContextDesc||!pInstructionData||!pStatusCode)
		return -1;
	struct ttf_status_code statusCode = {0};
	memset((void*)&statusCode, 0, sizeof(struct ttf_status_code));
	statusCode.code = TTF_STATUS_CODE_SUCCESS;
	statusCode.subCode = TTF_STATUS_CODE_SUCCESS;
	if (ttf_bytecode_context_stack_clear(pBytecodeContextDesc->bytecodeContextId)!=0){
		statusCode.code = TTF_STATUS_CODE_UNKNOWN;
		statusCode.code = TTF_STATUS_CODE_UNKNOWN;
		*pStatusCode = statusCode;
		return -1;
	}
	*pStatusCode = statusCode;
	return 0;
}
int ttf_bytecode_context_op_mdap(struct ttf_bytecode_context_desc* pBytecodeContextDesc, uint8_t* pInstructionData, uint8_t argument, struct ttf_status_code* pStatusCode){
	if (!pBytecodeContextDesc||!pInstructionData||!pStatusCode)
		return -1;
	struct ttf_status_code statusCode = {0};
	memset((void*)&statusCode, 0, sizeof(struct ttf_status_code));
	statusCode.code = TTF_STATUS_CODE_SUCCESS;
	statusCode.subCode = TTF_STATUS_CODE_SUCCESS;
	uint16_t* pStackData = (uint16_t*)0x00;
	if (ttf_bytecode_context_pop(pBytecodeContextDesc->bytecodeContextId, (uint8_t**)&pStackData, sizeof(uint16_t))!=0){
		statusCode.code = TTF_STATUS_CODE_UNKNOWN;
		statusCode.subCode = TTF_STATUS_CODE_UNKNOWN;
		*pStatusCode = statusCode;
		return -1;
	}
	uint16_t pointIndex = __builtin_bswap16(*pStackData);
	struct fvec2_64 projectionVector = pBytecodeContextDesc->executionState.projectionVector;
	struct fvec2_64 freedomVector = pBytecodeContextDesc->executionState.freedomVector;
	struct ttf_point_entry_desc* pPointDesc = pBytecodeContextDesc->executionState.pExecutionInfo->pPointDescList+pointIndex;
	printf("---MDAP---\r\n");
	printf("    MDAP point index: %d\r\n", pointIndex);
	printf("    MDAP round: %s\r\n", (argument) ? "Yes" : "No");
	printf("    MDAP projection vector: {%f, %f}\r\n", pBytecodeContextDesc->executionState.projectionVector.x, pBytecodeContextDesc->executionState.projectionVector.y);
	printf("    MDAP freedom vector: {%f, %f}\r\n", pBytecodeContextDesc->executionState.freedomVector.x, pBytecodeContextDesc->executionState.freedomVector.y);
	struct fvec2_64 resultCoord = {0};
	resultCoord.x = 0.0;
	resultCoord.y = 0.0;
	printf("original coord: {%f, %f}\r\n", pPointDesc->position.x, pPointDesc->position.y);
	printf("result coord: {%f, %f}\r\n", resultCoord.x, resultCoord.y);
	pBytecodeContextDesc->executionState.referencePointList[0] = pointIndex;
	pBytecodeContextDesc->executionState.referencePointList[1] = pointIndex;
	*pStatusCode = statusCode;
	return 0;
}
int ttf_bytecode_context_op_miap(struct ttf_bytecode_context_desc* pBytecodeContextDesc, uint8_t* pInstructionData, uint8_t argument, struct ttf_status_code* pStatusCode){
	if (!pBytecodeContextDesc||!pInstructionData||!pStatusCode)
		return -1;
	struct ttf_status_code statusCode = {0};
	memset((void*)&statusCode, 0, sizeof(struct ttf_status_code));
	statusCode.code = TTF_STATUS_CODE_SUCCESS;
	statusCode.subCode = TTF_STATUS_CODE_SUCCESS;
	uint16_t* pStackData = (uint16_t*)0x00;
	if (ttf_bytecode_context_pop(pBytecodeContextDesc->bytecodeContextId, (uint8_t**)&pStackData, sizeof(uint16_t)*0x02)!=0){
		statusCode.code = TTF_STATUS_CODE_UNKNOWN;
		statusCode.subCode = TTF_STATUS_CODE_UNKNOWN;
		*pStatusCode = statusCode;
		return -1;
	}
	if (pBytecodeContextDesc->executionState.pExecutionInfo->readOnly){
		*pStatusCode = statusCode;
		return 0;
	}
	uint16_t pointIndex = __builtin_bswap16(*(pStackData+0x00));
	uint16_t cvtIndex = __builtin_bswap16(*(pStackData+0x01));
	double controlValue = 0.0;
	ttf_control_get_value(pBytecodeContextDesc->pFontDesc, cvtIndex, &controlValue);
	printf("---MIAP---\r\n");
	printf("    point index: %d\r\n", pointIndex);
	printf("    CVT index: %d\r\n", cvtIndex);
	printf("    control value: %f\r\n", controlValue);
	
	*pStatusCode = statusCode;
	return 0;
}
int ttf_bytecode_context_op_fdef(struct ttf_bytecode_context_desc* pBytecodeContextDesc, uint8_t* pInstructionData, uint8_t argument, struct ttf_status_code* pStatusCode){
	if (!pBytecodeContextDesc||!pInstructionData||!pStatusCode)
		return -1;
	struct ttf_status_code statusCode = {0};
	memset((void*)&statusCode, 0, sizeof(struct ttf_status_code));
	statusCode.code = TTF_STATUS_CODE_SUCCESS;
	statusCode.subCode = TTF_STATUS_CODE_SUCCESS;
	uint16_t* pStackData = (uint16_t*)0x00;
	if (ttf_bytecode_context_pop(pBytecodeContextDesc->bytecodeContextId, (uint8_t**)&pStackData, sizeof(uint16_t))!=0){
		statusCode.code = TTF_STATUS_CODE_UNKNOWN;
		statusCode.subCode = TTF_STATUS_CODE_UNKNOWN;
		*pStatusCode = statusCode;
		return -1;
	}
	uint64_t functionId = (uint64_t)__builtin_bswap16(*(uint16_t*)pStackData);
	struct ttf_bytecode_function_desc* pFunctionDesc = pBytecodeContextDesc->pFunctionDescList+functionId;
	pFunctionDesc->pFunction = (((uint8_t*)pInstructionData)+0x01);
	printf("FDEF: %d\r\n", functionId);
	*pStatusCode = statusCode;
	return 0;
}
int ttf_bytecode_context_op_call(struct ttf_bytecode_context_desc* pBytecodeContextDesc, uint8_t* pInstructionData, uint8_t argument, struct ttf_status_code* pStatusCode){
	if (!pBytecodeContextDesc||!pInstructionData||!pStatusCode)
		return -1;
	struct ttf_status_code statusCode = {0};
	memset((void*)&statusCode, 0, sizeof(struct ttf_status_code));
	statusCode.code = TTF_STATUS_CODE_SUCCESS;
	statusCode.subCode = TTF_STATUS_CODE_SUCCESS;
	uint16_t* pStackData = (uint16_t*)0x00;
	if (ttf_bytecode_context_pop(pBytecodeContextDesc->bytecodeContextId, (uint8_t**)&pStackData, sizeof(uint16_t))!=0){
		statusCode.code = TTF_STATUS_CODE_UNKNOWN;
		statusCode.subCode = TTF_STATUS_CODE_UNKNOWN;
		*pStatusCode = statusCode;
		return -1;
	}
	if (pBytecodeContextDesc->executionState.pExecutionInfo->readOnly){
		*pStatusCode = statusCode;
		return 0;
	}
	uint64_t functionId = (uint64_t)__builtin_bswap16(*pStackData);
	struct ttf_bytecode_function_desc* pFunctionDesc = pBytecodeContextDesc->pFunctionDescList+functionId;
	*pBytecodeContextDesc->pReturnStackEntry = pInstructionData+0x01;
	pBytecodeContextDesc->pReturnStackEntry++;
	pBytecodeContextDesc->executionState.pCurrentInstruction = ((uint8_t*)pFunctionDesc->pFunction)-0x01;
	printf("CALL: %d\r\n", functionId);
	*pStatusCode = statusCode;
	return 0;
}
int ttf_bytecode_context_op_endf(struct ttf_bytecode_context_desc* pBytecodeContextDesc, uint8_t* pInstructionData, uint8_t argument, struct ttf_status_code* pStatusCode){
	if (!pBytecodeContextDesc||!pInstructionData||!pStatusCode)
		return -1;
	struct ttf_status_code statusCode = {0};
	memset((void*)&statusCode, 0, sizeof(struct ttf_status_code));
	statusCode.code = TTF_STATUS_CODE_SUCCESS;
	statusCode.subCode = TTF_STATUS_CODE_SUCCESS;
	if (pBytecodeContextDesc->executionState.pExecutionInfo->readOnly){
		*pStatusCode = statusCode;
		return 0;
	}
	pBytecodeContextDesc->pReturnStackEntry--;
	uint8_t* pFunction = (uint8_t*)*pBytecodeContextDesc->pReturnStack;
	pBytecodeContextDesc->executionState.pCurrentInstruction = pFunction-0x01;
	*pStatusCode = statusCode;
	return 0;
}
int ttf_bytecode_context_op_npushb(struct ttf_bytecode_context_desc* pBytecodeContextDesc, uint8_t* pInstructionData, uint8_t argument, struct ttf_status_code* pStatusCode){
	if (!pBytecodeContextDesc||!pInstructionData||!pStatusCode)
		return -1;
	struct ttf_status_code statusCode = {0};
	memset((void*)&statusCode, 0, sizeof(struct ttf_status_code));
	statusCode.code = TTF_STATUS_CODE_SUCCESS;
	statusCode.subCode = TTF_STATUS_CODE_SUCCESS;
	uint8_t* pPushData = pInstructionData+0x01;
	uint64_t pushDataSize = (uint64_t)*pInstructionData;
	if (ttf_bytecode_context_push(pBytecodeContextDesc->bytecodeContextId, (uint8_t*)pPushData, pushDataSize)!=0){
		statusCode.code = TTF_STATUS_CODE_UNKNOWN;
		statusCode.code = TTF_STATUS_CODE_UNKNOWN;
		*pStatusCode = statusCode;
		return -1;
	}
	printf("NPUSHB: %d\r\n", pushDataSize);
	pBytecodeContextDesc->executionState.pCurrentInstruction+=pushDataSize+0x01;
	*pStatusCode = statusCode;
	return 0;
}
int ttf_bytecode_context_op_npushw(struct ttf_bytecode_context_desc* pBytecodeContextDesc, uint8_t* pInstructionData, uint8_t argument, struct ttf_status_code* pStatusCode){
	if (!pBytecodeContextDesc||!pInstructionData||!pStatusCode)
		return -1;
	struct ttf_status_code statusCode = {0};
	memset((void*)&statusCode, 0, sizeof(struct ttf_status_code));
	statusCode.code = TTF_STATUS_CODE_SUCCESS;
	statusCode.subCode = TTF_STATUS_CODE_SUCCESS;
	uint16_t* pPushData = (uint16_t*)(pInstructionData+0x01);
	uint64_t pushDataSize = (*pInstructionData)*sizeof(uint16_t);
	if (ttf_bytecode_context_push(pBytecodeContextDesc->bytecodeContextId, (uint8_t*)pPushData, pushDataSize)!=0){
		statusCode.code = TTF_STATUS_CODE_UNKNOWN;
		statusCode.code = TTF_STATUS_CODE_UNKNOWN;
		*pStatusCode = statusCode;
		return -1;
	}
	printf("NPUSHW: %d\r\n", *pInstructionData);
	pBytecodeContextDesc->executionState.pCurrentInstruction+=pushDataSize+0x01;
	*pStatusCode = statusCode;
	return 0;
}
int ttf_bytecode_context_op_pushb(struct ttf_bytecode_context_desc* pBytecodeContextDesc, uint8_t* pInstructionData, uint8_t argument, struct ttf_status_code* pStatusCode){
	if (!pBytecodeContextDesc||!pInstructionData||!argument||!pStatusCode)
		return -1;
	struct ttf_status_code statusCode = {0};
	memset((void*)&statusCode, 0, sizeof(struct ttf_status_code));
	statusCode.code = TTF_STATUS_CODE_SUCCESS;
	statusCode.subCode = TTF_STATUS_CODE_SUCCESS;
	uint8_t* pPushData = (uint8_t*)(pInstructionData);
	uint64_t pushDataSize = (uint64_t)argument;
	if (ttf_bytecode_context_push(pBytecodeContextDesc->bytecodeContextId, (uint8_t*)pPushData, pushDataSize)!=0){
		statusCode.code = TTF_STATUS_CODE_UNKNOWN;
		statusCode.subCode = TTF_STATUS_CODE_UNKNOWN;
		*pStatusCode = statusCode;
		return -1;
	}
	printf("PUSHB: %d\r\n", argument);
	pBytecodeContextDesc->executionState.pCurrentInstruction+=pushDataSize;
	*pStatusCode = statusCode;
	return 0;
}
int ttf_bytecode_context_op_pushw(struct ttf_bytecode_context_desc* pBytecodeContextDesc, uint8_t* pInstructionData, uint8_t argument, struct ttf_status_code* pStatusCode){
	if (!pBytecodeContextDesc||!pInstructionData||!argument||!pStatusCode)
		return -1;
	struct ttf_status_code statusCode = {0};
	memset((void*)&statusCode, 0, sizeof(struct ttf_status_code));
	statusCode.code = TTF_STATUS_CODE_SUCCESS;
	statusCode.subCode = TTF_STATUS_CODE_SUCCESS;
	uint16_t* pPushData = (uint16_t*)(pInstructionData);
	uint64_t pushDataSize = ((uint64_t)argument)*sizeof(uint16_t);
	if (ttf_bytecode_context_push(pBytecodeContextDesc->bytecodeContextId, (uint8_t*)pPushData, pushDataSize)!=0){
		statusCode.code = TTF_STATUS_CODE_UNKNOWN;
		statusCode.subCode = TTF_STATUS_CODE_UNKNOWN;
		*pStatusCode = statusCode;
		return -1;
	}
	printf("PUSHW: %d\r\n", argument);
	pBytecodeContextDesc->executionState.pCurrentInstruction+=pushDataSize;
	*pStatusCode = statusCode;
	return 0;
}
int ttf_bytecode_context_execute(uint64_t bytecodeContextId, struct ttf_bytecode_execution_info* pExecutionInfo, struct ttf_status_code* pStatusCode){
	if (!pExecutionInfo||!pStatusCode)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct ttf_bytecode_context_desc* pBytecodeContextDesc = (struct ttf_bytecode_context_desc*)0x00;
	if (ttf_bytecode_context_get_desc(bytecodeContextId, &pBytecodeContextDesc)!=0){
		printf("failed to get true type bytecode context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct ttf_font_desc* pFontDesc = pBytecodeContextDesc->pFontDesc;
	if (!pFontDesc){
		printf("true type bytecode context descriptor not linked with true type font descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	uint8_t* pBytecode = pBytecodeContextDesc->pBytecode;
	uint64_t bytecodeLength = pBytecodeContextDesc->bytecodeLength;
	struct ttf_status_code statusCode = {0};
	memset((void*)&statusCode, 0, sizeof(struct ttf_status_code));
	statusCode.code = TTF_STATUS_CODE_SUCCESS;
	statusCode.subCode = TTF_STATUS_CODE_SUCCESS;
	struct ttf_bytecode_execution_state* pExecutionState = &pBytecodeContextDesc->executionState;
	memset((void*)pExecutionState, 0, sizeof(struct ttf_bytecode_execution_state));
	pExecutionState->projectionVector.x = 1.0f;
	pExecutionState->projectionVector.y = 0.0f;
	pExecutionState->freedomVector.x = 1.0f;
	pExecutionState->freedomVector.y = 0.0f;
	pExecutionState->currentRoundingOpcode = TTF_OPCODE_RTG;
	pExecutionState->pExecutionInfo = pExecutionInfo;
	pExecutionState->pCurrentInstruction = (uint8_t*)pBytecodeContextDesc->pBytecode;
	for (;;pExecutionState->pCurrentInstruction++){
		struct ttf_instruction* pInstruction = (struct ttf_instruction*)pExecutionState->pCurrentInstruction;
		uint8_t* pInstructionData = (uint8_t*)(pInstruction+0x01);
		static const uint8_t argumentRangeList[] = {
			[0xB0]=0xB7,
			[0xB8]=0xBF,
			[0x46]=0x47,
			[0x30]=0x31,
			[0x49]=0x4A,
			[0x2E]=0x2F,
			[0xC0]=0xDF,
			[0x3E]=0x3F,
			[0xE0]=0xFF,
			[0x3A]=0x3B,
			[0x6C]=0x6F,
			[0x68]=0x6B,
			[0x04]=0x05,
			[0x08]=0x09,
			[0x34]=0x35,
			[0x32]=0x33,
			[0x36]=0x37,
			[0x02]=0x03,
			[0x06]=0x07,
			[0x00]=0x01,
		};
		static uint8_t argumentListSet = 0x00;
		static uint8_t argumentList[255]={
			
		};
		static const TTFBytecodeInstructionFunc bytecodeInstructionFuncList[255]={
			[TTF_OPCODE_SVTCA]=ttf_bytecode_context_op_svtca,
			[TTF_OPCODE_CLEAR]=ttf_bytecode_context_op_clear,
			[TTF_OPCODE_MIAP]=ttf_bytecode_context_op_miap,
			[TTF_OPCODE_MDAP]=ttf_bytecode_context_op_mdap,
			[TTF_OPCODE_FDEF]=ttf_bytecode_context_op_fdef,
			[TTF_OPCODE_CALL]=ttf_bytecode_context_op_call,
			[TTF_OPCODE_ENDF]=ttf_bytecode_context_op_endf,
			[TTF_OPCODE_NPUSHB]=ttf_bytecode_context_op_npushb,
			[TTF_OPCODE_NPUSHW]=ttf_bytecode_context_op_npushw,
			[TTF_OPCODE_PUSHB]=ttf_bytecode_context_op_pushb,
			[TTF_OPCODE_PUSHW]=ttf_bytecode_context_op_pushw,
		};
		static const unsigned char bytecodeReadOnlyInstructionFuncList[255]={
			[TTF_OPCODE_FDEF]=0x01,
			[TTF_OPCODE_PUSHB]=0x01,
			[TTF_OPCODE_PUSHW]=0x01,
			[TTF_OPCODE_NPUSHB]=0x01,
			[TTF_OPCODE_NPUSHW]=0x01,
		};
		if (!argumentListSet){
			for (uint64_t i = 0;i<sizeof(argumentRangeList);i++){
				if (!argumentRangeList[i])
					continue;
				uint64_t argumentRange = argumentRangeList[i]-i+0x01;
				for (uint64_t argument = 0x00;argument<argumentRange;argument++){
					argumentList[i+argument] = (uint8_t)(argument+0x01);
				}
			}
			argumentListSet = 0x01;
		}
		uint8_t opcode = pInstruction->opcode;
		uint8_t argument = argumentList[pInstruction->opcode];
		const unsigned char* opcodeName = (const unsigned char*)"Unsupported opcode";
		if (argument){
			opcode-=argument-0x01;
		}
		ttf_opcode_get_name(opcode, &opcodeName);
		if (pExecutionInfo->readOnly&&!bytecodeReadOnlyInstructionFuncList[opcode])
			continue;
		TTFBytecodeInstructionFunc instructionFunc = bytecodeInstructionFuncList[opcode];
		if (!instructionFunc){
			printf("unsupported opcode: %s\r\n", opcodeName);
			statusCode.code = TTF_STATUS_CODE_OPERATION_UNSUPPORTED;
			break;
		}
		printf("opcode: %s\r\n", opcodeName);
		struct ttf_status_code instructionStatusCode = {0};
		memset((void*)&instructionStatusCode, 0, sizeof(struct ttf_status_code));
		instructionStatusCode.code = TTF_STATUS_CODE_UNKNOWN;
		instructionStatusCode.subCode = TTF_STATUS_CODE_UNKNOWN;
		instructionFunc(pBytecodeContextDesc, pInstructionData, argument, &instructionStatusCode);
		if (instructionStatusCode.code!=TTF_STATUS_CODE_SUCCESS){
			printf("true type execution failure at opcode: %s\r\n", opcodeName);
			statusCode.code = TTF_STATUS_CODE_OPERATION_FAILURE;
			statusCode.subCode = instructionStatusCode.code;
			break;
		}
	}
	*pStatusCode = statusCode;
	if (statusCode.code!=TTF_STATUS_CODE_SUCCESS){
		printf("failed to execute true type bytecode context\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int ttf_glyph_get_id_fmt_4(struct ttf_font_desc* pFontDesc, struct ttf_subtable_cmap_header* pCharacterMapSubtableHeader, uint32_t characterCode, uint32_t* pGlyphId){
	if (!pFontDesc||!pCharacterMapSubtableHeader||!pGlyphId)
		return -1;
	uint32_t* pGlyphIdMapping = pFontDesc->pGlyphIdMappingTable+characterCode;
	uint32_t* pGlyphIdMappingPage = (uint32_t*)align_down((uint64_t)pGlyphIdMapping, PAGE_SIZE);
	uint64_t mappingPhysicalAddress = 0x00;
	if (virtualToPhysical((uint64_t)pGlyphIdMappingPage, &mappingPhysicalAddress)!=0){
		printf("failed to get glyph ID mapping page physical address\r\n");
		return -1;
	}
	if (mappingPhysicalAddress){
		uint32_t glyphId = *pGlyphIdMapping;
		if (glyphId!=0xFFFFFFFF){
			*pGlyphId = glyphId;
			return 0;
		}
	}
	struct ttf_subtable_cmap4* pCharacterMapSubtable = (struct ttf_subtable_cmap4*)pCharacterMapSubtableHeader;
	uint16_t segmentCount = __builtin_bswap16(pCharacterMapSubtable->segment_count_x2)/2;
	uint16_t searchRange = __builtin_bswap16(pCharacterMapSubtable->search_range);
	uint16_t entrySelector = __builtin_bswap16(pCharacterMapSubtable->entry_selector);
	uint16_t rangeShift = __builtin_bswap16(pCharacterMapSubtable->range_shift);
	uint16_t* pEndCodeList = (uint16_t*)(pCharacterMapSubtable->lookup_data);
	uint16_t* pStartCodeList = (pEndCodeList+segmentCount+1);
	int16_t* pDeltaList = pStartCodeList+segmentCount;
	uint16_t* pRangeOffsetList = pDeltaList+segmentCount;
	uint16_t* pGlyphIdList = pRangeOffsetList+segmentCount;
	for (uint16_t i = 0;i<segmentCount;i++){
		uint16_t segmentIndex = i;
		uint16_t startCode = __builtin_bswap16(pStartCodeList[segmentIndex]);
		uint16_t endCode = __builtin_bswap16(pEndCodeList[segmentIndex]);
		int16_t delta = __builtin_bswap16(pDeltaList[segmentIndex]);
		uint16_t rangeOffset = __builtin_bswap16(pRangeOffsetList[segmentIndex]);
		if (characterCode<startCode||characterCode>endCode)
			continue;
		if (!rangeOffset){
			uint32_t glyphId = (characterCode+delta)%(1<<16);
			*pGlyphId = glyphId;
			return 0;
		}
		uint32_t glyphId = (uint32_t)__builtin_bswap16(*((uint16_t*)(((unsigned char*)(pRangeOffsetList+segmentIndex)+rangeOffset+((characterCode-startCode)*2)))));
		glyphId = (glyphId+delta)%(1<<16);
		uint32_t* pGlyphIdMapping = pFontDesc->pGlyphIdMappingTable+characterCode;
		uint32_t* pGlyphIdMappingPage = (uint32_t*)(align_down((uint64_t)pGlyphIdMapping, PAGE_SIZE));
		uint64_t mappingPhysicalAddress = 0x00;
		if (virtualToPhysical((uint64_t)pGlyphIdMappingPage, &mappingPhysicalAddress)!=0){
			*pGlyphId = glyphId;
			return 0;
		}
		if (!mappingPhysicalAddress){
			memset_64((void*)pGlyphIdMappingPage, 0xFFFFFFFF, PAGE_SIZE);
		}
		*pGlyphIdMapping = glyphId;
		*pGlyphId = glyphId;
		return 0;
	}
	return -1;
}
int ttf_glyph_get_id(struct ttf_font_desc* pFontDesc, uint16_t platformId, uint16_t encodingId, uint32_t characterCode, uint32_t* pGlyphId){
	if (!pFontDesc||!pGlyphId)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	static const TTFGetGlyphIdFunc getGlyphIdFuncList[32]={
		[0x04] = (TTFGetGlyphIdFunc)ttf_glyph_get_id_fmt_4,
	};
	struct ttf_table_desc* pCharacterMapTableDesc = (struct ttf_table_desc*)0x00;
	if (ttf_table_get_desc(pFontDesc, TTF_TABLE_ID_CMAP, &pCharacterMapTableDesc)!=0){
		printf("failed to get TTF CMAP table descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct ttf_table_cmap_header* pCharacterMapTableHeader = (struct ttf_table_cmap_header*)(pFontDesc->pFontBuffer+__builtin_bswap32(pCharacterMapTableDesc->table_offset));
	uint16_t characterMapTableVersion = __builtin_bswap16(pCharacterMapTableHeader->version);
	uint16_t characterMapTableRecordCount = __builtin_bswap16(pCharacterMapTableHeader->record_count);
	for (uint16_t i = 0;i<characterMapTableRecordCount;i++){
		struct ttf_table_cmap_record* pRecord = ((struct ttf_table_cmap_record*)pCharacterMapTableHeader->record_list)+i;
		uint16_t recordPlatformId = __builtin_bswap16(pRecord->platform_id);
		uint16_t recordEncodingId = __builtin_bswap16(pRecord->encoding_id);
		uint32_t subtableOffset = __builtin_bswap32(pRecord->subtable_offset);
		struct ttf_subtable_cmap_header* pSubtableHeader = (struct ttf_subtable_cmap_header*)(((uint64_t)pCharacterMapTableHeader)+(uint64_t)subtableOffset);
		uint16_t subtableFormat = __builtin_bswap16(pSubtableHeader->format);
		if ((recordPlatformId!=platformId&&platformId!=0xFFFF)||(recordEncodingId!=encodingId&&encodingId!=0xFFFF))
			continue;
		TTFGetGlyphIdFunc getGlyphId = getGlyphIdFuncList[subtableFormat];
		if (!getGlyphId){
			printf("unsupported CMAP subtable format 0x%x\r\n", subtableFormat);
			mutex_unlock(&mutex);
			return -1;
		}
		uint32_t glyphId = 0x00;
		if (getGlyphId(pFontDesc, pSubtableHeader, characterCode, &glyphId)!=0){
			mutex_unlock(&mutex);
			return -1;
		}
		*pGlyphId = glyphId;
		mutex_unlock(&mutex);
		return 0;
	}
	mutex_unlock(&mutex);
	return -1;
}
int ttf_glyf_get_desc(struct ttf_font_desc* pFontDesc, uint32_t glyphId, struct ttf_glyf_header** ppGlyfDesc, uint32_t* pGlyfDescLength){
	if (!pFontDesc||!ppGlyfDesc||!pGlyfDescLength)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct ttf_table_desc* pHeadTableDesc = (struct ttf_table_desc*)0x00;
	if (ttf_table_get_desc(pFontDesc, TTF_TABLE_ID_HEAD, &pHeadTableDesc)!=0){
		printf("failed to get TTF HEAD table descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct ttf_table_desc* pLocaTableDesc = (struct ttf_table_desc*)0x00;
	if (ttf_table_get_desc(pFontDesc, TTF_TABLE_ID_LOCA, &pLocaTableDesc)!=0){
		printf("failed to get TTF LOCA table descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct ttf_table_desc* pGlyfTableDesc = (struct ttf_table_desc*)0x00;
	if (ttf_table_get_desc(pFontDesc, TTF_TABLE_ID_GLYF, &pGlyfTableDesc)!=0){
		printf("failed to get TTF glyf table descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	uint32_t headTableOffset = __builtin_bswap32(pHeadTableDesc->table_offset);
	uint32_t locaTableOffset = __builtin_bswap32(pLocaTableDesc->table_offset);
	uint32_t glyfTableOffset = __builtin_bswap32(pGlyfTableDesc->table_offset);
	struct ttf_table_head* pHeadTable = (struct ttf_table_head*)(pFontDesc->pFontBuffer+headTableOffset);
	uint16_t* pLocaTable = (uint16_t*)(pFontDesc->pFontBuffer+locaTableOffset);
	uint16_t unitsPerEm = __builtin_bswap16(pHeadTable->units_per_em);
	uint64_t locaTableEntrySize = __builtin_bswap16(pHeadTable->index_to_loca_format) ? sizeof(uint32_t) : sizeof(uint16_t);
	uint32_t currentEntry = (locaTableEntrySize==sizeof(uint32_t)) ? __builtin_bswap32(*(((uint32_t*)pLocaTable)+glyphId)) : __builtin_bswap16(*(pLocaTable+glyphId))*2;
	uint32_t nextEntry = (locaTableEntrySize==sizeof(uint32_t)) ? __builtin_bswap32(*(uint32_t*)(pLocaTable+((glyphId+1)*2))) : __builtin_bswap16(*(pLocaTable+glyphId+1))*2;
	if (currentEntry==nextEntry){
		mutex_unlock(&mutex);
		return -1;
	}
	struct ttf_glyf_header* pGlyfDesc = (struct ttf_glyf_header*)(pFontDesc->pFontBuffer+glyfTableOffset+currentEntry);
	uint32_t glyfDescLength = nextEntry-currentEntry;
	*ppGlyfDesc = pGlyfDesc;
	*pGlyfDescLength = glyfDescLength;
	mutex_unlock(&mutex);
	return 0;
}
int ttf_glyf_tesselate(struct ttf_font_desc* pFontDesc, uint32_t glyphId, uint8_t* pTextureBuffer, struct uvec2_32 textureBufferRect, uint8_t glyphSpread){
	if (!pFontDesc||!pTextureBuffer||glyphSpread>127)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct ttf_table_desc* pHeadTableDesc = (struct ttf_table_desc*)0x00;
	if (ttf_table_get_desc(pFontDesc, TTF_TABLE_ID_HEAD, &pHeadTableDesc)!=0){
		printf("failed to get true type HEAD table descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct ttf_table_desc* pCvtTableDesc = (struct ttf_table_desc*)0x00;
	if (ttf_table_get_desc(pFontDesc, TTF_TABLE_ID_CVT, &pCvtTableDesc)!=0){
		printf("failed to get true type CVT table descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct ttf_table_desc* pControlProgramTableDesc = (struct ttf_table_desc*)0x00;
	if (ttf_table_get_desc(pFontDesc, TTF_TABLE_ID_PREP, &pControlProgramTableDesc)!=0){
		printf("failed to get true type PREP table descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	uint32_t* pCvtTable = (uint32_t*)(pFontDesc->pFontBuffer+__builtin_bswap32(pCvtTableDesc->table_offset));
	uint64_t cvtTableLength = (uint64_t)__builtin_bswap32(pCvtTableDesc->table_length);
	uint8_t* pControlProgramBytecode = (uint8_t*)(pFontDesc->pFontBuffer+__builtin_bswap32(pControlProgramTableDesc->table_offset));
	uint64_t controlProgramBytecodeSize = (uint64_t)__builtin_bswap32(pControlProgramTableDesc->table_length);
//	printf("control value table length: %d\r\n", cvtTableLength);
	struct ttf_glyf_header* pGlyfHeader = (struct ttf_glyf_header*)0x00;
	uint32_t glyfDescLength = 0x00;
	if (ttf_glyf_get_desc(pFontDesc, glyphId, &pGlyfHeader, &glyfDescLength)!=0){
		printf("failed to get true type GLYF descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	uint32_t headTableOffset = __builtin_bswap32(pHeadTableDesc->table_offset);
	struct ttf_table_head* pHeadTable = (struct ttf_table_head*)(pFontDesc->pFontBuffer+headTableOffset);
	uint16_t unitsPerEm = __builtin_bswap16(pHeadTable->units_per_em);
	int16_t contourCount = __builtin_bswap16(pGlyfHeader->contour_count);
	if (contourCount==-1){
		printf("composite GLYF descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	int16_t minX = __builtin_bswap16(pGlyfHeader->min_x);
	int16_t minY = __builtin_bswap16(pGlyfHeader->min_y);
	int16_t maxX = __builtin_bswap16(pGlyfHeader->max_x);
	int16_t maxY = __builtin_bswap16(pGlyfHeader->max_y);
	maxX+=absq(minX);
	maxY+=absq(minY);
	struct uvec2_32 glyphBounds = {0};
	double glyphMaxScale = (maxX>maxY) ? ((double)maxX)/(double)unitsPerEm : ((double)maxY)/(double)unitsPerEm;
	glyphBounds.x = (uint32_t)(((double)unitsPerEm)*glyphMaxScale);
	glyphBounds.y = (uint32_t)(((double)unitsPerEm)*glyphMaxScale);
	uint16_t* pContourEndpointTable = (uint16_t*)(pGlyfHeader+1);
	uint16_t pointCount = __builtin_bswap16(pContourEndpointTable[contourCount-1])+0x01;
	uint16_t instructionListLength = __builtin_bswap16(*(pContourEndpointTable+contourCount));
	uint8_t* pInstructionList = (uint8_t*)(pContourEndpointTable+contourCount+0x01);
	struct ttf_glyf_point_flags* pPointFlagsList = (struct ttf_glyf_point_flags*)(pInstructionList+instructionListLength);
	int8_t* pCoordListX = (int8_t*)0x00;
	int8_t* pCoordListY = (int8_t*)0x00;
	uint64_t textureBufferSize = sizeof(struct fvec4_32)*textureBufferRect.x*textureBufferRect.y;
	uint64_t pointFlagOffset = 0x00;
	if (!pointCount){
		mutex_unlock(&mutex);
		return 0;
	}
/*	printf("glyph contour count: %d\r\n", contourCount);
	printf("glyph point count: %d\r\n", pointCount);
*/	for (uint64_t i = 0;i<pointCount;i++,pointFlagOffset++){
		struct ttf_glyf_point_flags* pPointFlags = (struct ttf_glyf_point_flags*)(((unsigned char*)pPointFlagsList)+pointFlagOffset);
		if (!pPointFlags->repeat)
			continue;
		uint8_t repeatCount = *(uint8_t*)(pPointFlags+0x01);
		pointFlagOffset++;
		i+=repeatCount;
	}
	pCoordListX = ((uint8_t*)pPointFlagsList)+pointFlagOffset;
	struct ttf_contour_entry_desc* pContourDescList = (struct ttf_contour_entry_desc*)0x00;
	uint64_t contourDescListSize = sizeof(struct ttf_contour_entry_desc)*contourCount;
	if (virtualAlloc((uint64_t*)&pContourDescList, contourDescListSize, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate physical pages for true type contour descriptor list\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct ttf_point_entry_desc* pPointDescList = (struct ttf_point_entry_desc*)0x00;
	uint64_t pointDescListSize = sizeof(struct ttf_point_entry_desc)*pointCount;
	if (virtualAlloc((uint64_t*)&pPointDescList, pointDescListSize, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate physical pages for true type point descriptor list\r\n");
		virtualFree((uint64_t)pContourDescList, contourDescListSize);
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t coordEntryIndex = 0x00;
	int32_t lastCoordEntry = 0x00;
	uint64_t pointFlagsIndex = 0x00;
	uint64_t pointFlagRepeatFence = 0x00;
	uint64_t indexBufferEntryCount = pointCount;
	uint64_t indexBufferSize = indexBufferEntryCount*sizeof(uint16_t);
	for (uint64_t i = 0;i<pointCount;i++){
		struct ttf_glyf_point_flags* pPointFlags = pPointFlagsList+pointFlagsIndex;
		uint8_t* pCoordListEntry = (uint8_t*)(pCoordListX+coordEntryIndex);
		if (pPointFlags->x_short){
			if (pPointFlags->x_same){
				lastCoordEntry = (int32_t)(((uint32_t)*pCoordListEntry)+(uint32_t)lastCoordEntry);
				coordEntryIndex++;
			}
			if (!pPointFlags->x_same){
				lastCoordEntry = (int32_t)(((int32_t)lastCoordEntry)-(int32_t)*pCoordListEntry);
				coordEntryIndex++;
			}
		}
		if (!pPointFlags->x_short){
			if (pPointFlags->x_same){
				
			}
			if (!pPointFlags->x_same){
				int16_t coordEntry = (int16_t)__builtin_bswap16(*(uint16_t*)pCoordListEntry);
				lastCoordEntry = ((int32_t)coordEntry)+(int32_t)lastCoordEntry;
				coordEntryIndex+=2;
			}
		}
		memset((void*)(pPointDescList+i), 0, sizeof(struct ttf_point_entry_desc));
		pPointDescList[i].position.x = ((((double)(((int64_t)lastCoordEntry)+absq(minX)))/(double)glyphBounds.x)*(double)(textureBufferRect.x-0x01));
		pPointDescList[i].flags = *pPointFlags;
		if (i<pointFlagRepeatFence)
			continue;
		if (!pPointFlags->repeat||(i==pointFlagRepeatFence&&i)){
			pointFlagsIndex+=(i==pointFlagRepeatFence&&i) ? 0x02 : 0x01;
			continue;
		}
		uint8_t repeatCount = *(uint8_t*)(pPointFlagsList+pointFlagsIndex+0x01);
		pointFlagRepeatFence = i+repeatCount;
		continue;
	}
	pCoordListY = pCoordListX+coordEntryIndex;
	coordEntryIndex = 0x00;
	lastCoordEntry = 0x00;
	pointFlagsIndex = 0x00;
	pointFlagRepeatFence = 0x00;
	uint64_t currentContourIndex = 0x00;
	double nextMinY = (double)(textureBufferRect.y);
	double nextMaxY = 0.0;
	for (uint64_t i = 0;i<pointCount;i++){
		if (i>__builtin_bswap16(pContourEndpointTable[currentContourIndex])||!i){
			currentContourIndex+=(i ? 0x01 : 0x00);
			struct ttf_contour_entry_desc* pContourDesc = pContourDescList+currentContourIndex;
			memset((void*)pContourDesc, 0, sizeof(struct ttf_contour_entry_desc));
			nextMinY = (double)(textureBufferRect.y);
			nextMaxY = 0.0;
		}
		struct ttf_contour_entry_desc* pCurrentContourDesc = pContourDescList+currentContourIndex;
		struct ttf_glyf_point_flags* pPointFlags = pPointFlagsList+pointFlagsIndex;
		uint8_t* pCoordListEntry = (uint8_t*)(pCoordListY+coordEntryIndex);
		if (pPointFlags->y_short){
			if (pPointFlags->y_same){
				lastCoordEntry = (int32_t)(((uint32_t)*pCoordListEntry)+(uint32_t)lastCoordEntry);
				coordEntryIndex++;
			}
			if (!pPointFlags->y_same){
				lastCoordEntry = (int32_t)((uint32_t)lastCoordEntry-((uint32_t)*pCoordListEntry));
				coordEntryIndex++;
			}
		}
		if (!pPointFlags->y_short){
			if (pPointFlags->y_same){
			
			}
			if (!pPointFlags->y_same){
				int16_t coordEntry = (int16_t)__builtin_bswap16(*(uint16_t*)pCoordListEntry);
				lastCoordEntry = ((int32_t)coordEntry)+(int32_t)lastCoordEntry;
				coordEntryIndex+=0x02;
			}
		}
		pPointDescList[i].position.y = ((((double)(((int64_t)lastCoordEntry)+absq(minY)))/(double)glyphBounds.y)*(double)(textureBufferRect.y-0x01));
		pPointDescList[i].flags = *pPointFlags;
		if (i<pointFlagRepeatFence)
			continue;
		if (!pPointFlags->repeat||(i==pointFlagRepeatFence&&i)){
			pointFlagsIndex+=(i==pointFlagRepeatFence&&i) ? 0x02 : 0x01;
			continue;
		}
		uint8_t repeatCount = *(uint8_t*)(pPointFlagsList+pointFlagsIndex+0x01);
		pointFlagRepeatFence = i+repeatCount;
	}
	static struct uvec2_32 lastTextureRect = {0x00, 0x00};
	if (*((uint64_t*)&lastTextureRect)!=*(uint64_t*)&textureBufferRect&&0x00){
		ttf_bytecode_context_set_bytecode(pFontDesc->bytecodeContextId, (uint8_t*)pControlProgramBytecode, controlProgramBytecodeSize);
		struct ttf_bytecode_execution_info executionInfo = {0};
		memset((void*)&executionInfo, 0, sizeof(struct ttf_bytecode_execution_info));
		executionInfo.readOnly = 0x00;
		executionInfo.textureRect = textureBufferRect;
		struct ttf_status_code statusCode = {0};
		memset((void*)&statusCode, 0, sizeof(struct ttf_status_code));
		statusCode.code = TTF_STATUS_CODE_SUCCESS;
		statusCode.subCode = TTF_STATUS_CODE_SUCCESS;
//		ttf_bytecode_context_execute(pFontDesc->bytecodeContextId, &executionInfo, &statusCode);
		if (statusCode.code!=TTF_STATUS_CODE_SUCCESS){
			const unsigned char* pStatusCodeName = "Unknown status code";
			const unsigned char* pStatusSubCodeName = "Unknown status subcode";
			ttf_status_code_get_name(statusCode.code, &pStatusCodeName);
			ttf_status_code_get_name(statusCode.subCode, &pStatusSubCodeName);
			printf("failed to execute true type ISA control value list bytecode\r\n");
			virtualFree((uint64_t)pContourDescList, contourDescListSize);
			virtualFree((uint64_t)pPointDescList, pointDescListSize);
			mutex_unlock(&mutex);
			return -1;
		}
	}
	lastTextureRect = textureBufferRect;
	ttf_bytecode_context_set_bytecode(pFontDesc->bytecodeContextId, (uint8_t*)pInstructionList, instructionListLength);
	struct ttf_bytecode_execution_info executionInfo = {0};
	memset((void*)&executionInfo, 0, sizeof(struct ttf_bytecode_execution_info));
	executionInfo.readOnly = 0x00;
	executionInfo.pPointDescList = pPointDescList;
	executionInfo.textureRect = textureBufferRect;
	struct ttf_status_code statusCode = {0};
	memset((void*)&statusCode, 0, sizeof(struct ttf_status_code));
//	ttf_bytecode_context_execute(pFontDesc->bytecodeContextId, &executionInfo, &statusCode);
	if (statusCode.code!=TTF_STATUS_CODE_SUCCESS&&0x00){
		const unsigned char* statusCodeName = "Unknown status code";
		const unsigned char* statusSubCodeName = "Unknown status code";
		ttf_status_code_get_name(statusCode.code, &statusCodeName);
		ttf_status_code_get_name(statusCode.subCode, &statusSubCodeName);
		printf("failed to execute true type bytecode execution context (%s, %s)\r\n", statusCodeName, statusSubCodeName);
		virtualFree((uint64_t)pContourDescList, contourDescListSize);
		virtualFree((uint64_t)pPointDescList, pointDescListSize);
		mutex_unlock(&mutex);
		return -1;
	}
/*	printf("GLYF contour count: %d\r\n", contourCount);
	printf("GLYF point count: %d\r\n", pointCount);
	printf("GLYF min range: {%d, %d}\r\n", minX, minY);
	printf("GLYF max range: {%d, %d}\r\n", maxX, maxY);
	printf("units per em: %d\r\n", unitsPerEm);
*/	uint64_t textureBufferEntryCount = textureBufferRect.x*textureBufferRect.y;
	struct ttf_segment_entry_desc* pSegmentDescList = (struct ttf_segment_entry_desc*)0x00;
	struct uvec2_32 segmentRect = {0};
	segmentRect.x = (uint32_t)glyphSpread;
	segmentRect.y = (uint32_t)glyphSpread;
	struct uvec2_32 segmentCount = {0};
	segmentCount.x = (textureBufferRect.x/segmentRect.x);
	segmentCount.y = (textureBufferRect.y/segmentRect.y);
	uint64_t segmentEntryCount = (segmentCount.x*segmentCount.y);
	uint64_t segmentDescListSize = segmentEntryCount*sizeof(struct ttf_segment_entry_desc);
	if (virtualAlloc((uint64_t*)&pSegmentDescList, segmentDescListSize, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate physical pages for true type glyph segment descriptor list\r\n");
		virtualFree((uint64_t)pContourDescList, contourDescListSize);
		virtualFree((uint64_t)pPointDescList, pointDescListSize);
		virtualFree((uint64_t)pSegmentDescList, segmentDescListSize);
		mutex_unlock(&mutex);
		return -1;	
	}
	memset((void*)pSegmentDescList, 0, segmentDescListSize);
	struct ttf_pixel_entry_desc* pPixelDescList = (struct ttf_pixel_entry_desc*)0x00;
	uint64_t pixelDescListSize = (textureBufferRect.x*textureBufferRect.y)*sizeof(struct ttf_pixel_entry_desc);
	if (virtualAlloc((uint64_t*)&pPixelDescList, pixelDescListSize, PTE_RW|PTE_NX, MAP_FLAG_LAZY, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate physical pages for true type glyph pixel descriptor list\r\n");
		virtualFree((uint64_t)pContourDescList, contourDescListSize);
		virtualFree((uint64_t)pPointDescList, pointDescListSize);
		virtualFree((uint64_t)pSegmentDescList, segmentDescListSize);
		mutex_unlock(&mutex);
		return -1;
	}
	struct uvec2_32 pointCoord = {0};
	pointCoord.x = 0x00;
	pointCoord.y = 0x00;
	pointFlagsIndex = 0x00;
	float fontWidth = 0.01f;
	pointFlagsIndex = 0x00;
	pointFlagRepeatFence = 0x00;
	struct ttf_pixel_entry_desc* pFirstActivePixelDesc = (struct ttf_pixel_entry_desc*)0x00;
	struct ttf_pixel_entry_desc* pLastActivePixelDesc = (struct ttf_pixel_entry_desc*)0x00;
	uint64_t currentPointIndex = 0x00;
	uint64_t activePixelCount = 0x00;
	uint64_t pointDelta = 0x00;
	uint64_t startTime = get_time_us();
	currentContourIndex = 0x00;
	for (uint64_t i = 0;currentPointIndex<pointCount;i++,currentPointIndex+=pointDelta){
		if (currentPointIndex>__builtin_bswap16(pContourEndpointTable[currentContourIndex])||!currentPointIndex){
			currentContourIndex+=(currentPointIndex ? 0x01 : 0x00);
			struct ttf_contour_entry_desc* pContourDesc = pContourDescList+currentContourIndex;
		}
		struct ttf_contour_entry_desc* pContourDesc = pContourDescList+currentContourIndex;
		uint64_t nextPointIndex = (currentPointIndex+0x01)%(__builtin_bswap16(pContourEndpointTable[currentContourIndex])+0x01);
		nextPointIndex = (!nextPointIndex&&currentContourIndex) ? __builtin_bswap16(pContourEndpointTable[currentContourIndex-0x01])+0x01 : nextPointIndex;
		pointDelta = 0x00;
		if ((pPointDescList+currentPointIndex)->flags.on_curve&&(pPointDescList+nextPointIndex)->flags.on_curve&&!pointDelta){
			struct fvec2_64 startCoord = {0};
			startCoord = pPointDescList[currentPointIndex].position;
			struct fvec2_64 endCoord = {0};
			endCoord = pPointDescList[nextPointIndex].position;
			struct fvec2_64 deltaCoord = {0};
			deltaCoord.x = endCoord.x-startCoord.x;
			deltaCoord.y = endCoord.y-startCoord.y;
			uint64_t deltaPixelCount = (uint64_t)ceilf(sqrtf((double)((absf(deltaCoord.x)*absf(deltaCoord.x))+(absf(deltaCoord.y)*absf(deltaCoord.y)))));
			for (uint64_t pixelEntry = 0;pixelEntry<deltaPixelCount;pixelEntry++){
				struct fvec2_64 textureFloatingCoord = {0};
				struct uvec2_32 textureCoord = {0};
				double lerpRatio = ((double)(pixelEntry))/(double)(deltaPixelCount);
				textureFloatingCoord = lerpf64(startCoord, endCoord, lerpRatio);
				textureCoord.x = roundf(textureFloatingCoord.x);
				textureCoord.y = roundf(textureFloatingCoord.y);
				struct uvec2_32 segmentEntryCoord = {0};
				segmentEntryCoord.x = textureCoord.x/segmentRect.x;
				segmentEntryCoord.y = textureCoord.y/segmentRect.y;
				uint64_t pixelOffset = (textureCoord.y*textureBufferRect.x)+textureCoord.x;
				uint64_t segmentOffset = (segmentEntryCoord.y*segmentCount.x)+segmentEntryCoord.x;
				uint8_t pixelIntersection = (*((((int8_t*)pTextureBuffer)+pixelOffset))==0x00) ? 0x01 : 0x00;
				struct ttf_pixel_entry_desc* pPixelDesc = pPixelDescList+pixelOffset;
				struct ttf_segment_entry_desc* pSegmentDesc = pSegmentDescList+segmentOffset;
				if (pixelIntersection)
					continue;
				*(uint64_t*)pPixelDesc = 0x00;
				pPixelDesc->contourIndex = currentContourIndex;
				pPixelDesc->windingDelta = 0x00;
				pPixelDesc->nextActivePixelOffset = 0x00;
				pPixelDesc->windingDelta+=(deltaCoord.y>0x00) ? 1 : -1;
				*(((int8_t*)pTextureBuffer)+pixelOffset) = 0x00;
				activePixelCount++;
				struct uvec2_32 segmentCoord = {0};
				segmentCoord.x = segmentEntryCoord.x*segmentRect.x;
				segmentCoord.y = segmentEntryCoord.y*segmentRect.y;
				for (uint64_t i = 0;i<4;i++){
					if (!pSegmentDesc->cornerPixelList[i]){
						pSegmentDesc->cornerPixelList[i] = pixelOffset+0x01;
						continue;
					}
					struct uvec2_32 cornerCoord = {0};
					cornerCoord.x = (!i||i==0x03) ? segmentCoord.x : segmentCoord.x+segmentRect.x-0x01;
					cornerCoord.y = (!i||i==0x01) ? segmentCoord.y : segmentCoord.y-segmentRect.y+0x01;
					uint64_t oldPixelOffset = pSegmentDesc->cornerPixelList[i]-0x01;
					struct ttf_pixel_entry_desc* pOldPixelDesc = pPixelDescList+oldPixelOffset;
					struct uvec2_32 oldPixelPosition = {0};
					oldPixelPosition.x = oldPixelOffset%textureBufferRect.x;
					oldPixelPosition.y = oldPixelOffset/textureBufferRect.x;
					uint32_t oldMagnitude = distu32(oldPixelPosition, cornerCoord);
					uint32_t magnitude = distu32(textureCoord, cornerCoord);
					if (oldMagnitude<magnitude)
						continue;
					pSegmentDesc->cornerPixelList[i] = pixelOffset+0x01;
				}
				if (!pSegmentDesc->firstActivePixelOffset){
					pSegmentDesc->firstActivePixelOffset = pixelOffset+0x01;
				}
				if (pSegmentDesc->lastActivePixelOffset){
					struct ttf_pixel_entry_desc* pLastActivePixelDesc = pPixelDescList+pSegmentDesc->lastActivePixelOffset-0x01;
					pLastActivePixelDesc->nextActivePixelOffset = pixelOffset+0x01;
				}
				pSegmentDesc->lastActivePixelOffset = pixelOffset+0x01;
				pSegmentDesc->activePixelCount++;
			}
			pointDelta = 0x01;
		}
		if ((pPointDescList+currentPointIndex)->flags.on_curve&&!((pPointDescList+nextPointIndex)->flags.on_curve)&&!pointDelta){
			uint64_t curvePointCount = 0x00;
			uint64_t controlPointCount = 0x00;
			for (uint64_t controlPointIndex = currentPointIndex+0x01;;){
				if ((pPointDescList+controlPointIndex)->flags.on_curve)
					break;
				controlPointIndex = (controlPointIndex+0x01)%(__builtin_bswap16(pContourEndpointTable[currentContourIndex])+0x01);
				controlPointIndex = (!controlPointIndex&&currentContourIndex) ? __builtin_bswap16(pContourEndpointTable[currentContourIndex-0x01])+0x01 : controlPointIndex;
				controlPointCount++;
			}
			curvePointCount = 0x02+controlPointCount;
		//	printf("    control point count: %d\r\n", controlPointCount);
			uint64_t maxCurveCount = controlPointCount;
			for (uint64_t curveCount = 0x00;curveCount<maxCurveCount+0x02;curveCount++){
				struct fvec2_64 startCoord = {0};
				startCoord.x = 0.0;
				startCoord.y = 0.0;
				struct fvec2_64 controlCoord = {0};
				controlCoord.x = 0.0;
				controlCoord.y = 0.0;
				struct fvec2_64 endCoord = {0};
				endCoord.x = 0.0;
				endCoord.y = 0.0;
				if (controlPointCount==0x01){
					uint64_t startCoordIndex = currentPointIndex;
					uint64_t controlCoordIndex = nextPointIndex;
					uint64_t endCoordIndex = (controlCoordIndex+0x01)%(__builtin_bswap16(pContourEndpointTable[currentContourIndex])+0x01);
					endCoordIndex = (!endCoordIndex&&currentContourIndex) ? __builtin_bswap16(pContourEndpointTable[currentContourIndex-0x01])+0x01 : endCoordIndex;
					startCoord = pPointDescList[startCoordIndex].position;
					controlCoord = pPointDescList[controlCoordIndex].position;
					endCoord = pPointDescList[endCoordIndex].position;
				}
				if (controlPointCount!=0x01){
					static uint64_t controlCoordIndex = 0x00;
					static struct fvec2_64 nextStartCoord = {0};
					if (!curveCount){
						controlCoordIndex = nextPointIndex;
						startCoord = pPointDescList[currentPointIndex].position;
						nextStartCoord.x = 0.0;
						nextStartCoord.y = 0.0;
					}
					uint64_t nextControlCoordIndex = (controlCoordIndex+0x01)%(__builtin_bswap16(pContourEndpointTable[currentContourIndex])+0x01);
					nextControlCoordIndex = (!nextControlCoordIndex&&currentContourIndex) ? __builtin_bswap16(pContourEndpointTable[currentContourIndex-0x01])+0x01 : nextControlCoordIndex;
					uint8_t controlCoordSingle = (pPointDescList+nextControlCoordIndex)->flags.on_curve ? 0x01 : 0x00;
					if (curveCount){
						startCoord = nextStartCoord;
					}
					controlCoord = pPointDescList[controlCoordIndex].position;
					if (controlCoordSingle){
						endCoord = pPointDescList[nextControlCoordIndex].position;
					}
					if (!controlCoordSingle){
						uint64_t nextControlCoordIndex = (controlCoordIndex+0x01)%(__builtin_bswap16(pContourEndpointTable[currentContourIndex])+0x01);
						nextControlCoordIndex = (!nextControlCoordIndex&&currentContourIndex) ? __builtin_bswap16(pContourEndpointTable[currentContourIndex-0x01])+0x01 : nextControlCoordIndex;
						endCoord.x = controlCoord.x+pPointDescList[nextControlCoordIndex].position.x;
						endCoord.y = controlCoord.y+pPointDescList[nextControlCoordIndex].position.y;
						endCoord.x/=0x02;
						endCoord.y/=0x02;
					}
					controlCoordIndex = (controlCoordIndex+0x01)%(__builtin_bswap16(pContourEndpointTable[currentContourIndex])+0x01);
					controlCoordIndex = (!controlCoordIndex&&currentContourIndex) ? (__builtin_bswap16(pContourEndpointTable[currentContourIndex-0x01])+0x01) : controlCoordIndex;
					nextStartCoord = endCoord;
				}
				uint64_t curveSegmentCount = 0x08;
				for (uint64_t curveSegment = 0x00;curveSegment<curveSegmentCount;curveSegment++){
					double curveStep = ((double)curveSegment)/(double)(curveSegmentCount);
					double nextCurveStep = ((double)(curveSegment+0x01))/(double)(curveSegmentCount);
					struct fvec2_64 segmentStartCoord = {0};
					segmentStartCoord.x = ((((1.0-curveStep)*(1.0-curveStep))*startCoord.x)+(((2.0*(1.0-curveStep))*curveStep)*controlCoord.x)+((curveStep*curveStep)*endCoord.x));
					segmentStartCoord.y = ((((1.0-curveStep)*(1.0-curveStep))*startCoord.y)+(((2.0*(1.0-curveStep))*curveStep)*controlCoord.y)+((curveStep*curveStep)*endCoord.y));
					struct fvec2_64 segmentEndCoord = {0};
					segmentEndCoord.x = ((((1.0-nextCurveStep)*(1.0-nextCurveStep))*startCoord.x)+(((2.0*(1.0-nextCurveStep))*nextCurveStep)*controlCoord.x)+((nextCurveStep*nextCurveStep)*endCoord.x));
					segmentEndCoord.y = ((((1.0-nextCurveStep)*(1.0-nextCurveStep))*startCoord.y)+(((2.0*(1.0-nextCurveStep))*nextCurveStep)*controlCoord.y)+((nextCurveStep*nextCurveStep)*endCoord.y));
					struct fvec2_64 deltaCoord = {0};
					deltaCoord.x = segmentEndCoord.x-segmentStartCoord.x;
					deltaCoord.y = segmentEndCoord.y-segmentStartCoord.y;
					uint64_t deltaPixelCount = (uint64_t)ceilf(sqrtf((double)((absf(deltaCoord.x)*absf(deltaCoord.x))+(absf(deltaCoord.y)*absf(deltaCoord.y)))));
					for (uint64_t pixelEntry = 0x00;pixelEntry<deltaPixelCount;pixelEntry++){
						struct uvec2_32 textureCoord = {0};
						struct fvec2_64 textureFloatingCoord = {0};
						double step = ((double)pixelEntry)/(double)deltaPixelCount;
						textureFloatingCoord = lerpf64(segmentStartCoord, segmentEndCoord, step);
						textureCoord.x = (uint32_t)roundf(textureFloatingCoord.x);
						textureCoord.y = (uint32_t)roundf(textureFloatingCoord.y);
						struct uvec2_32 segmentEntryCoord = {0};
						segmentEntryCoord.x = textureCoord.x/segmentRect.x;
						segmentEntryCoord.y = textureCoord.y/segmentRect.y;
						uint64_t pixelOffset = (textureCoord.y*textureBufferRect.x)+textureCoord.x;
						uint64_t segmentOffset = (segmentEntryCoord.y*segmentCount.x)+segmentEntryCoord.x;
						uint8_t pixelIntersection = (*(((int8_t*)pTextureBuffer)+pixelOffset)==0x00) ? 0x01 : 0x00;
						struct ttf_pixel_entry_desc* pPixelDesc = pPixelDescList+pixelOffset;
						struct ttf_segment_entry_desc* pSegmentDesc = pSegmentDescList+segmentOffset;
						if (pixelIntersection)
							continue;
						*(uint64_t*)pPixelDesc = 0x00;			
						pPixelDesc->contourIndex = currentContourIndex;
						pPixelDesc->windingDelta = 0x00;
						pPixelDesc->nextActivePixelOffset = 0x00;
						pPixelDesc->windingDelta+=((deltaCoord.y>0x00) ? 1 : -1);
						pSegmentDesc->activePixelCount++;
						struct uvec2_32 segmentCoord = {0};
						segmentCoord.x = segmentRect.x*segmentEntryCoord.x;
						segmentCoord.y = segmentRect.y*segmentEntryCoord.y;
						*(((int8_t*)pTextureBuffer)+pixelOffset) = 0x00;
						for (uint64_t i = 0;i<4;i++){
							if (!pSegmentDesc->cornerPixelList[i]){
								pSegmentDesc->cornerPixelList[i] = pixelOffset+0x01;
								continue;
							}
							struct uvec2_32 cornerLocation = {0};
							cornerLocation.x = (!i||i==0x03) ? segmentCoord.x : (segmentCoord.x+segmentRect.x-0x01);
							cornerLocation.x = (!i||i==0x01) ? segmentCoord.y : (segmentCoord.y-segmentRect.y+0x01);
							uint64_t oldPixelOffset = pSegmentDesc->cornerPixelList[i]-0x01;
							struct uvec2_32 oldPixelCoord = {0};
							oldPixelCoord.x = oldPixelOffset%textureBufferRect.x;
							oldPixelCoord.y = oldPixelOffset/textureBufferRect.x;
							struct ttf_pixel_entry_desc* pOldPixelDesc = pPixelDescList+oldPixelOffset;
							uint32_t oldMagnitude = distu32(oldPixelCoord, cornerLocation);
							uint32_t magnitude = distu32(textureCoord, cornerLocation);
							if (oldMagnitude<magnitude)
								continue;
							pSegmentDesc->cornerPixelList[i] = pixelOffset+0x01;
						}
						if (!pSegmentDesc->firstActivePixelOffset){
							pSegmentDesc->firstActivePixelOffset = pixelOffset+0x01;
						}
						if (pSegmentDesc->lastActivePixelOffset){
							struct ttf_pixel_entry_desc* pLastActivePixelDesc = pPixelDescList+pSegmentDesc->lastActivePixelOffset-0x01;
							pLastActivePixelDesc->nextActivePixelOffset = pixelOffset+0x01;
						}
						pSegmentDesc->lastActivePixelOffset = pixelOffset+0x01;
						pSegmentDesc->activePixelCount++;
					}
				}
			}
			pointDelta = curvePointCount-0x01;
		}
		if (!pointDelta){
			printf("unexpected true type point descriptor set: %d\r\n", currentPointIndex);
			virtualFree((uint64_t)pPointDescList, pointDescListSize);
			virtualFree((uint64_t)pContourDescList, contourDescListSize);
			virtualFree((uint64_t)pPixelDescList, pixelDescListSize);
			return -1;
		}
	}
	uint64_t elapsedTime = get_time_us()-startTime;
	printf("prefragmentation time: %f\r\n", ((double)elapsedTime)/1000.0);
	struct vec2_64 vec1 = {0};
	vec1.x = 64;
	vec1.y = -40;
	struct vec2_64 vec2 = {0};
	vec2.x = -32;
	vec2.y = -64;
	startTime = get_time_us();
	for (uint64_t row = 0x00;row<textureBufferRect.y;row++){
		uint64_t activePixelCount = 0x00;
		uint64_t inactivePixelCount = 0x00;
		uint64_t lastInactivePixelCount = 0xFFFFFFFFFFFFFFFF;
		int16_t winding = 0x00;
		for (uint64_t column = 0x00;column<textureBufferRect.x;column++){
			struct uvec2_32 pixelCoord = {0};
			pixelCoord.x = column;
			pixelCoord.y = row;
			struct uvec2_32 segmentEntryCoord = {0};
			segmentEntryCoord.x = pixelCoord.x/segmentRect.x;
			segmentEntryCoord.y = pixelCoord.y/segmentRect.y;
			uint64_t pixelOffset = (pixelCoord.y*textureBufferRect.x)+pixelCoord.x;
			uint64_t segmentEntryOffset = (segmentEntryCoord.y*segmentCount.x)+segmentEntryCoord.x;
			uint64_t physicalAddress = 0x00;
			if (virtualToPhysical((uint64_t)(pPixelDescList+pixelOffset), (uint64_t*)&physicalAddress)!=0){
				printf("failed to get true type pixel descriptor list physical page address\r\n");
				virtualFree((uint64_t)pPointDescList, pointDescListSize);
				virtualFree((uint64_t)pContourDescList, contourDescListSize);
				virtualFree((uint64_t)pPixelDescList, pixelDescListSize);
				return -1;
			}
			if (!physicalAddress&&0x00){
				pixelOffset+=PAGE_SIZE;
				inactivePixelCount+=PAGE_SIZE;
				row = pixelOffset/textureBufferRect.x;
				column = pixelOffset%textureBufferRect.x;
				if (row>textureBufferRect.y-0x01)
					break;
				continue;
			}
			struct ttf_pixel_entry_desc* pPixelDesc = pPixelDescList+pixelOffset;
			struct ttf_segment_entry_desc* pSegmentDesc = pSegmentDescList+segmentEntryOffset;
			struct ttf_contour_entry_desc* pContourDesc = pContourDescList+pPixelDesc->contourIndex;
			struct uvec2_32 segmentCoord = {0};
			segmentCoord.x = segmentEntryCoord.x*segmentRect.x;
			segmentCoord.y = segmentEntryCoord.y*segmentRect.y;
			if (*(((int8_t*)pTextureBuffer)+pixelOffset)==0x00&&lastInactivePixelCount!=inactivePixelCount){
				winding+=pPixelDesc->windingDelta;
				lastInactivePixelCount = inactivePixelCount;
				continue;
			}
			if (*(((int8_t*)pTextureBuffer)+pixelOffset)<0x00)
				inactivePixelCount++;
			if (*(((int8_t*)pTextureBuffer)+pixelOffset)==0x00)
				continue;
			struct ttf_pixel_entry_desc* pNearestActivePixel = (struct ttf_pixel_entry_desc*)0x00;
			int8_t magnitude = glyphSpread;
			for (uint64_t i = 0;i<0x09;i++){
				struct vec2_32 currentCoord = {0};
				currentCoord.x = (int32_t)((((int64_t)segmentCoord.x)-segmentRect.x)+((i%0x03)*segmentRect.x));
				currentCoord.y = (int32_t)((((int64_t)segmentCoord.y)-segmentRect.y)+((i/0x03)*segmentRect.y));
				if (currentCoord.x<0x00||currentCoord.x>(segmentCount.x-0x01)*segmentRect.x)
					continue;
				if (currentCoord.y<0x00||currentCoord.y>(segmentCount.y-0x01)*segmentRect.y)
					continue;
				struct uvec2_32 currentEntryCoord = {0};
				currentEntryCoord.x = ((uint32_t)currentCoord.x)/segmentRect.x;
				currentEntryCoord.y = ((uint32_t)currentCoord.y)/segmentRect.y;
				uint64_t currentSegmentOffset = (currentEntryCoord.y*segmentCount.x)+currentEntryCoord.x;
				struct ttf_segment_entry_desc* pCurrentSegmentDesc = pSegmentDescList+currentSegmentOffset;
				if (!pCurrentSegmentDesc->activePixelCount)
					continue;
				struct ttf_pixel_entry_desc* pCurrentPixelDesc = pPixelDescList+pCurrentSegmentDesc->firstActivePixelOffset-0x01;
				for (uint64_t activePixelIndex = 0x00;activePixelIndex<pCurrentSegmentDesc->activePixelCount;activePixelIndex++){
					uint64_t currentPixelOffset = (uint64_t)(pCurrentPixelDesc-pPixelDescList);
					struct uvec2_32 currentPixelCoord = {0};
					currentPixelCoord.x = currentPixelOffset%textureBufferRect.x;
					currentPixelCoord.y = currentPixelOffset/textureBufferRect.x;
					if (currentPixelCoord.x==pixelCoord.x&&currentPixelCoord.y==pixelCoord.y)
						continue;
					int32_t currentMagnitude = (int32_t)distu32(currentPixelCoord, pixelCoord);
					if (currentMagnitude<(int32_t)magnitude)
						magnitude = (int8_t)currentMagnitude;
					if (!pCurrentPixelDesc->nextActivePixelOffset)
						break;
					pCurrentPixelDesc = pPixelDescList+pCurrentPixelDesc->nextActivePixelOffset-0x01;
				}
			}
			if (!winding){
				*(((int8_t*)pTextureBuffer)+pixelOffset) = ((int8_t)magnitude)*-1;
				continue;
			}
			*(((int8_t*)pTextureBuffer)+pixelOffset) = ((int8_t)magnitude);
		}
	}
	elapsedTime = get_time_us()-startTime;
	printf("SDF floating point DWORD generation time: %fms\r\n", ((double)elapsedTime)/1000.0);
	virtualFree((uint64_t)pPointDescList, pointDescListSize);
	virtualFree((uint64_t)pContourDescList, contourDescListSize);
	virtualFree((uint64_t)pSegmentDescList, segmentDescListSize);
	virtualFree((uint64_t)pPixelDescList, pixelDescListSize);
	mutex_unlock(&mutex);
	return 0;
}
int ttf_font_load(unsigned char* pFontBuffer, uint64_t fontBufferSize, struct text_subsystem_font_desc* pSubsystemFontDesc){
	if (!pFontBuffer||!fontBufferSize||!pSubsystemFontDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	printf("loading GPU host controller TTF font with ID: %d\r\n", pSubsystemFontDesc->fontId);
	struct ttf_header* pFontHeader = (struct ttf_header*)pFontBuffer;
	struct ttf_table_desc* pTableDescList = (struct ttf_table_desc*)(pFontHeader+1);
	uint32_t signature = __builtin_bswap32(pFontHeader->signature);
	uint32_t tableCount = __builtin_bswap16(pFontHeader->table_count);
	if (signature!=TTF_SIGNATURE){
		printf("invalid GPU host controller TTF font signature\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct ttf_font_desc* pFontDesc = (struct ttf_font_desc*)kmalloc(sizeof(struct ttf_font_desc));
	if (!pFontDesc){
		printf("failed to allocate GPU host controller TTF font descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pFontDesc, 0, sizeof(struct ttf_font_desc));
	pFontDesc->pSubsystemFontDesc = pSubsystemFontDesc;
	pFontDesc->pFontBuffer = pFontBuffer;
	pFontDesc->fontBufferSize = fontBufferSize;
	pFontDesc->tableCount = tableCount;
	pSubsystemFontDesc->extra = (uint64_t)pFontDesc;
	for (uint64_t i = 0;i<tableCount;i++){
		struct ttf_table_desc* pTableDesc = pTableDescList+i;
		uint32_t tableType = pTableDesc->table_type;
		uint32_t tableOffset = __builtin_bswap32(pTableDesc->table_offset);
		uint32_t tableLength = __builtin_bswap32(pTableDesc->table_length);
		unsigned char tableTypeName[8] = {0};
		*(uint32_t*)tableTypeName = tableType;
		tableTypeName[4] = 0x00;
		switch (tableType){
			case TTF_TABLE_TYPE_CMAP:
				pFontDesc->tableDescList[TTF_TABLE_ID_CMAP] = pTableDesc;
				break;
			case TTF_TABLE_TYPE_GLYF:
				pFontDesc->tableDescList[TTF_TABLE_ID_GLYF] = pTableDesc;
				break;
			case TTF_TABLE_TYPE_HEAD:
				pFontDesc->tableDescList[TTF_TABLE_ID_HEAD] = pTableDesc;
				break;
			case TTF_TABLE_TYPE_LOCA:
				pFontDesc->tableDescList[TTF_TABLE_ID_LOCA] = pTableDesc;
				break;
			case TTF_TABLE_TYPE_MAXP:
				pFontDesc->tableDescList[TTF_TABLE_ID_MAXP] = pTableDesc;
				break;
			case TTF_TABLE_TYPE_NAME:{
				struct ttf_table_name_header* pTableHeader = (struct ttf_table_name_header*)(pFontBuffer+tableOffset);	
				struct ttf_table_name_record* pTableRecordList = (struct ttf_table_name_record*)(pTableHeader+1);
				uint16_t stringRecordCount = __builtin_bswap16(pTableHeader->string_record_count);
				uint16_t stringDataOffset = __builtin_bswap16(pTableHeader->string_data_offset);
				printf("string record count: %d\r\n", stringRecordCount);
				printf("string physical page data offset: %d\r\n", stringDataOffset);
				for (uint16_t i = 0;i<stringRecordCount;i++){
					struct ttf_table_name_record* pTableRecord = pTableRecordList+i;
					uint16_t platformId = __builtin_bswap16(pTableRecord->platform_id);
					uint16_t encodingId = __builtin_bswap16(pTableRecord->encoding_id);
					uint16_t languageId = __builtin_bswap16(pTableRecord->language_id);
					uint16_t nameId = __builtin_bswap16(pTableRecord->name_id);
					uint16_t stringLength = __builtin_bswap16(pTableRecord->string_length);
					uint16_t stringOffset = __builtin_bswap16(pTableRecord->string_offset);
					uint16_t* pStringData = (uint16_t*)(((unsigned char*)pTableHeader)+stringDataOffset+stringOffset);
					if (platformId==0x03&&languageId!=0x0409&&languageId!=0x0809)
						continue;
					if (platformId==0x01&&languageId!=0x00)
						continue;
					struct text_subsystem_font_name_desc* pFontNameDesc = (struct text_subsystem_font_name_desc*)kmalloc(sizeof(struct text_subsystem_font_name_desc));
					if (!pFontNameDesc){
						printf("failed to allocate font name descriptor\r\n");
						mutex_unlock(&mutex);
						return -1;
					}
					uint64_t nameLength = 0x00;
					switch (platformId){
						case TTF_PLATFORM_ID_WINDOWS:{
							if (encodingId!=WINDOWS_ENCODE_ID_UNICODE_BMP)
								break;
							uint64_t characterCount = stringLength/sizeof(uint16_t);
							for (uint64_t i = 0;i<characterCount;i++){
								uint16_t* pNameEntry = ((uint16_t*)pStringData)+i;
								pFontNameDesc->name[i] = (uint8_t)__builtin_bswap16(*pNameEntry);
							}
							*(((uint16_t*)pStringData)+characterCount) = 0x00;
							nameLength = characterCount+1;
							break;	     
						}
						case TTF_PLATFORM_ID_UNICODE:{
							if (encodingId!=UNICODE_ENCODE_ID_UNICODE2_BMP)
								break;
							uint64_t characterCount = stringLength/sizeof(uint16_t);
							for (uint64_t i = 0;i<characterCount;i++){
								uint16_t* pNameEntry = ((uint16_t*)pStringData)+i;
								pFontNameDesc->name[i] = (uint8_t)__builtin_bswap16(*pNameEntry);
							}
							*(((uint16_t*)pStringData)+characterCount) = 0x00;
							nameLength = characterCount+1;
							break;
						}	
						case TTF_PLATFORM_ID_MACINTOSH:{
							if (encodingId!=MACINTOSH_ENCODE_ID_ROMAN||languageId!=0x01)
								break;
							uint64_t characterCount = stringLength;
							memcpy((void*)pFontNameDesc->name, (void*)pStringData, stringLength);
							nameLength = characterCount+1;
							break;
						}
						default:{
							uint64_t characterCount = 20;
							memcpy((void*)pFontNameDesc->name, (void*)"Unsupported encoding", 21);
							nameLength = characterCount+1;
							break;	
						}
					}
					pFontNameDesc->nameLength = nameLength;
					pFontNameDesc->platformId = platformId;
					pSubsystemFontDesc->ppFontNameDescList[nameId] = pFontNameDesc;
				}
				pFontDesc->tableDescList[TTF_TABLE_ID_NAME] = pTableDesc;
				break;	
			}
			case TTF_TABLE_TYPE_POST:
				pFontDesc->tableDescList[TTF_TABLE_ID_POST] = pTableDesc;
				break;
			case TTF_TABLE_TYPE_HHEA:
				pFontDesc->tableDescList[TTF_TABLE_ID_HHEA] = pTableDesc;
				break;
			case TTF_TABLE_TYPE_HMTX:
				pFontDesc->tableDescList[TTF_TABLE_ID_HMTX] = pTableDesc;
				break;
			case TTF_TABLE_TYPE_VHEA:
				pFontDesc->tableDescList[TTF_TABLE_ID_VHEA] = pTableDesc;
				break;
			case TTF_TABLE_TYPE_VMTX:
				pFontDesc->tableDescList[TTF_TABLE_ID_VMTX] = pTableDesc;
				break;
			case TTF_TABLE_TYPE_OS2:
				pFontDesc->tableDescList[TTF_TABLE_ID_OS2] = pTableDesc;
				break;
			case TTF_TABLE_TYPE_KERN:
				pFontDesc->tableDescList[TTF_TABLE_ID_KERN] = pTableDesc;
				break;
			case TTF_TABLE_TYPE_CVT:
				pFontDesc->tableDescList[TTF_TABLE_ID_CVT] = pTableDesc;
				break;
			case TTF_TABLE_TYPE_FPGM:
				pFontDesc->tableDescList[TTF_TABLE_ID_FPGM] = pTableDesc;
				break;
			case TTF_TABLE_TYPE_PREP:
				pFontDesc->tableDescList[TTF_TABLE_ID_PREP] = pTableDesc;
				break;
			case TTF_TABLE_TYPE_GASP:
				pFontDesc->tableDescList[TTF_TABLE_ID_GASP] = pTableDesc;
				break;
			case TTF_TABLE_TYPE_GDEF:
				pFontDesc->tableDescList[TTF_TABLE_ID_GDEF] = pTableDesc;
				break;
			case TTF_TABLE_TYPE_GSUB:
				pFontDesc->tableDescList[TTF_TABLE_ID_GSUB] = pTableDesc;
				break;			 
			case TTF_TABLE_TYPE_GPOS:
				pFontDesc->tableDescList[TTF_TABLE_ID_GPOS] = pTableDesc;
				break;
			case TTF_TABLE_TYPE_BASE:
				pFontDesc->tableDescList[TTF_TABLE_ID_BASE] = pTableDesc;
				break;
		}
	}
	uint32_t* pGlyphIdMappingTable = (uint32_t*)0x00;
	uint64_t glyphIdMappingTableSize = 0x10FFFF*sizeof(uint32_t);
	if (virtualAlloc((uint64_t*)&pGlyphIdMappingTable, glyphIdMappingTableSize, PTE_RW|PTE_NX, MAP_FLAG_LAZY, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate physical pages for glyph ID sparse direct mapping table\r\n");
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t bytecodeContextId = 0x00;
	if (ttf_bytecode_context_init(pFontDesc, PAGE_SIZE*0x04, &bytecodeContextId)!=0){
		printf("failed to initialize true type bytecode context descriptor\r\n");
		virtualFree((uint64_t)pGlyphIdMappingTable, glyphIdMappingTableSize);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	struct ttf_table_desc* pFontProgramTableDesc = (struct ttf_table_desc*)0x00;
	if (ttf_table_get_desc(pFontDesc, TTF_TABLE_ID_FPGM, &pFontProgramTableDesc)!=0){
		printf("failed to get FPGM table descriptor\r\n");
		ttf_bytecode_context_deinit(bytecodeContextId);
		virtualFree((uint64_t)pGlyphIdMappingTable, glyphIdMappingTableSize);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t fontProgramOffset = (uint64_t)__builtin_bswap32(pFontProgramTableDesc->table_offset);
	uint64_t fontProgramSize = (uint64_t)__builtin_bswap32(pFontProgramTableDesc->table_length);
	uint8_t* pFontProgramBytecode = (uint8_t*)(pFontDesc->pFontBuffer+fontProgramOffset);
	ttf_bytecode_context_set_bytecode(bytecodeContextId, pFontProgramBytecode, fontProgramSize);
	struct ttf_bytecode_execution_info executionInfo = {0};
	memset((void*)&executionInfo, 0, sizeof(struct ttf_bytecode_execution_info));
	executionInfo.readOnly = 0x01;
	struct ttf_status_code statusCode = {0};
	memset((void*)&statusCode, 0, sizeof(struct ttf_status_code));
	statusCode.code = TTF_STATUS_CODE_SUCCESS;
	statusCode.subCode = TTF_STATUS_CODE_SUCCESS;
//	ttf_bytecode_context_execute(bytecodeContextId, &executionInfo, &statusCode);
	if (statusCode.code!=TTF_STATUS_CODE_SUCCESS){
		const unsigned char* pStatusCode = "Unknown status";
		const unsigned char* pStatusSubCode = "Unknown status";
		ttf_status_code_get_name(statusCode.code, &pStatusCode);
		ttf_status_code_get_name(statusCode.subCode, &pStatusSubCode);
		printf("failed to execute font program true type ISA bytecode (%s, %s)\r\n", pStatusCode, pStatusSubCode);
		ttf_bytecode_context_deinit(bytecodeContextId);
		virtualFree((uint64_t)pGlyphIdMappingTable, glyphIdMappingTableSize);
		kfree((void*)pFontDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	pFontDesc->bytecodeContextId = bytecodeContextId;
	pFontDesc->pGlyphIdMappingTable = pGlyphIdMappingTable;
	pFontDesc->glyphIdMappingTableSize = glyphIdMappingTableSize;
	mutex_unlock(&mutex);
	return 0;
}
int ttf_font_unload(struct text_subsystem_font_desc* pSubsystemFontDesc){
	if (!pSubsystemFontDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	printf("unloading GPU host controller TTF font with ID: %d\r\n", pSubsystemFontDesc->fontId);
	struct ttf_font_desc* pFontDesc = (struct ttf_font_desc*)pSubsystemFontDesc->extra;
	if (!pFontDesc){
		printf("text subsystem font descriptor not linked with TTF driver font descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pFontDesc->pGlyphIdMappingTable)
		virtualFree((uint64_t)pFontDesc->pGlyphIdMappingTable, pFontDesc->glyphIdMappingTableSize);
	if (pFontDesc->bytecodeContextId)
		ttf_bytecode_context_deinit(pFontDesc->bytecodeContextId);
	kfree((void*)pFontDesc);
	mutex_unlock(&mutex);
	return 0;
}
int ttf_font_verify(unsigned char* pFontBuffer, uint64_t fontBufferSize){
	if (!pFontBuffer||!fontBufferSize)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	printf("verifying GPU host controller TTF font of %d physical pages\r\n", fontBufferSize/PAGE_SIZE);
	struct ttf_header* pFontHeader = (struct ttf_header*)pFontBuffer;
	uint32_t signature = __builtin_bswap32(pFontHeader->signature);
	if (signature!=TTF_SIGNATURE){
		printf("invalid TTF signature: 0x%x\r\n", signature);
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int ttf_font_glyph_get_id(struct text_subsystem_font_desc* pSubsystemFontDesc, uint64_t characterCode, uint64_t* pGlyphId){
	if (!pSubsystemFontDesc||!pGlyphId)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct ttf_font_desc* pFontDesc = (struct ttf_font_desc*)pSubsystemFontDesc->extra;
	if (!pFontDesc){
		printf("TTF driver font descriptor not linked with text subsystem font descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	uint32_t glyphId = 0x00;
	if (ttf_glyph_get_id(pFontDesc, 0xFFFF, 0xFFFF, characterCode, &glyphId)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	*pGlyphId = (uint64_t)glyphId;	
	mutex_unlock(&mutex);
	return 0;
}
int ttf_font_glyph_tesselate(struct text_subsystem_font_desc* pSubsystemFontDesc, uint32_t glyphId, int8_t* pTextureBuffer, struct uvec2_32 textureBufferRect, uint8_t glyphSpread){
	if (!pSubsystemFontDesc||!pTextureBuffer)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct ttf_font_desc* pFontDesc = (struct ttf_font_desc*)pSubsystemFontDesc->extra;
	if (!pFontDesc){
		printf("TTF driver font descriptor not linked with text subsystem font descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (ttf_glyf_tesselate(pFontDesc, glyphId, pTextureBuffer, textureBufferRect, glyphSpread)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
