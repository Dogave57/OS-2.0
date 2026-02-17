#ifndef _NVME
#define _NVME
#define NVME_DRIVE_PCIE_CLASS (0x01)
#define NVME_DRIVE_PCIE_SUBCLASS (0x08)
#define NVME_DRIVE_PCIE_PROGIF (0x02)
#define NVME_DOORBELL_LIST_OFFSET (0x1000)
#define NVME_DEFAULT_SQE_COUNT (64)
#define NVME_DEFAULT_CQE_COUNT (64)
#define NVME_MAX_SQE_COUNT (64)
#define NVME_MAX_CQE_COUNT (64)
#define NVME_ADMIN_DELETE_IO_SUBMISSION_QUEUE_OPCODE (0x00)
#define NVME_ADMIN_CREATE_IO_SUBMISSION_QUEUE_OPCODE (0x01)
#define NVME_ADMIN_GET_LOG_PAGE_OPCODE (0x02)
#define NVME_ADMIN_DELETE_IO_COMPLETION_QUEUE_OPCODE (0x04)
#define NVME_ADMIN_CREATE_IO_COMPLETION_QUEUE_OPCODE (0x05)
#define NVME_ADMIN_IDENTIFY_OPCODE (0x06)
#define NVME_ADMIN_ABORT_OPCODE (0x08)
#define NVME_ADMIN_SET_FEATURES_OPCODE (0x09)
#define NVME_ADMIN_GET_FEATURES_OPCODE (0x0A)
#define NVME_ADMIN_ASYNC_EVENT_REQUEST_OPCODE (0x0C)
#define NVME_ADMIN_NAMESPACE_MANAGEMENT_OPCODE (0x0D)
#define NVME_ADMIN_FIRMWARE_COMMIT_OPCODE (0x10)
#define NVME_ADMIN_FIRMWARE_IMAGE_DOWNLOAD_OPCODE (0x11)
#define NVME_ADMIN_DEVICE_SELF_TEST_OPCODE (0x14)
#define NVME_ADMIN_NAMESPACE_ATTACHMENT_OPCODE (0x15)
#define NVME_ADMIN_KEEP_ALIVE_OPCODE (0x16)
#define NVME_ADMIN_DIRECTIVE_SEND_OPCODE (0x19)
#define NVME_ADMIN_DIRECTIVE_RECEIVE_OPCODE (0x1A)
#define NVME_ADMIN_VIRTUALIZATION_MANAGEMENT_OPCODE (0x1C)
#define NVME_ADMIN_MI_OOB_COMMAND_OPCODE (0x1D)
#define NVME_ADMIN_MI_OOB_RESPONSE_OPCODE (0x1E)
#define NVME_ADMIN_CAPACITY_MANAGEMENT_OPCODE (0x21)
#define NVME_ADMIN_DOORBELL_BUFFER_CONFIG_OPCODE (0x7C)
#define NVME_ADMIN_FORMAT_NVM_OPCODE (0x80)
#define NVME_ADMIN_SECURITY_SEND_OPCODE (0x81)
#define NVME_ADMIN_SECURITY_RECEIVE_OPCODE (0x82)
#define NVME_ADMIN_SANITIZE_OPCODE (0x84)
#define NVME_STATUS_CODE_SUCCESS (0x00)
#define NVME_STATUS_CODE_INVALID_OPCODE (0x01)
#define NVME_STATUS_CODE_INVALID_FIELD (0x02)
#define NVME_STATUS_CODE_CMD_ID_CONFLICT (0x03)
#define NVME_STATUS_CODE_DATA_TRANSFER_ERROR (0x04)
#define NVME_STATUS_CODE_ABORTED_POWER_LOSS (0x05)
#define NVME_STATUS_CODE_INTERNAL_ERROR (0x06)
#define NVME_STATUS_CODE_ABORT_REQUESTED (0x07)
#define NVME_STATUS_CODE_ABORT_SQ_DELETION (0x08)
#define NVME_STATUS_CODE_ABORT_FAILED_FUSED (0x09)
#define NVME_STATUS_CODE_ABORT_MISSING_FUSED (0x0A)
#define NVME_STATUS_CODE_INVALID_FORMAT (0x0B)
#define NVME_STATUS_CODE_COMMAND_SEQUENCE_ERROR (0x0C)
#define NVME_STATUS_CODE_INVALID_SGL_SEGMENT_DESC (0x0D)
#define NVME_STATUS_CODE_INVALID_SGL_DESC_COUNT (0x0E)
#define NVME_STATUS_CODE_INVALID_DATA_SGL_LENGTH (0x0F)
#define NVME_STATUS_CODE_INVALID_METADATA_SGL_LENGTH (0x10)
#define NVME_STATUS_CODE_INVALID_SGL_DESC_TYPE (0x11)
#define NVME_STATUS_CODE_INVALID_CMB_USE (0x12)
#define NVME_STATUS_CODE_INVALID_PRP_OFFSET (0x13)
#define NVME_STATUS_CODE_ATOMIC_WRITE_UNIT_EXCEEDED (0x14)
#define NVME_STATUS_CODE_OPERATION_DENIED (0x15)
#define NVME_STATUS_CODE_INVALID_SGL_OFFSET (0x16)
#define NVME_STATUS_CODE_HOST_IDENT_INCONSISTENT (0x18)
#define NVME_STATUS_CODE_KEEP_ALIVE_TIMEOUT_EXPIRED (0x19)
#define NVME_STATUS_CODE_INVALID_IO_CONTROLLER_STATE (0x1A)
#define NVME_STATUS_CODE_ABORT_TRIGGERED (0x1B)
#define NVME_STATUS_CODE_SANITIZE_OPERATION_FAILURE (0x1C)
#define NVME_STATUS_CODE_SANITIZE_IN_PROGRESS (0x1D)
#define NVME_STATUS_CODE_INVALID_SGL_GRANULARITY (0x1E)
#define NVME_STATUS_CODE_CMD_NOT_SUPPORTED (0x1F)
#define NVME_STATUS_CODE_NAMESPACE_WRITE_PROTECTED (0x20)
#define NVME_STATUS_CODE_CMD_INTERRUPTED (0x21)
#define NVME_STATUS_CODE_TRANSPORT_ERROR (0x22)
#define NVME_STATUS_CODE_LOCKDOWN_VIOLATION (0x23)
#define NVME_STATUS_CODE_ADMIN_CMD_MEDIA_NOT_READY (0x24)
#define NVME_STATUS_CODE_LBA_OUT_OF_RANGE (0x80)
#define NVME_STATUS_CODE_CAPACITY_EXCEEDED (0x81)
#define NVME_STATUS_CODE_TYPE_GENERIC_CMD (0x00)
#define NVME_STATUS_CODE_TYPE_CMD_SPECIFIC (0x01)
#define NVME_STATUS_CODE_TYPE_MEDIA_DATA_INTEGRITY (0x02)
#define NVME_STATUS_CODE_TYPE_PATH_RELATED (0x03)
#define NVME_DOORBELL_TYPE_SUBMISSION_QUEUE (0x00)
#define NVME_DOORBELL_TYPE_COMPLETION_QUEUE (0x01)
static const unsigned char* statusCodeNameMap[]={
	[NVME_STATUS_CODE_SUCCESS]="Success",
	[NVME_STATUS_CODE_INVALID_OPCODE]="Invalid opcode",
	[NVME_STATUS_CODE_INVALID_FIELD]="Invalid field",
	[NVME_STATUS_CODE_CMD_ID_CONFLICT]="Command ID conflict",
	[NVME_STATUS_CODE_DATA_TRANSFER_ERROR]="Data transfer error",
	[NVME_STATUS_CODE_ABORTED_POWER_LOSS]="Aborted due to power loss",
	[NVME_STATUS_CODE_INTERNAL_ERROR]="Internal error",
	[NVME_STATUS_CODE_ABORT_REQUESTED]="Abort requesteded",	
	[NVME_STATUS_CODE_ABORT_SQ_DELETION]="Abort due to I/O Submission queue deletion",
	[NVME_STATUS_CODE_ABORT_FAILED_FUSED]="Abort due to failed fused",
	[NVME_STATUS_CODE_ABORT_MISSING_FUSED]="Abort due to missing fused",
	[NVME_STATUS_CODE_INVALID_FORMAT]="Invalid format",
	[NVME_STATUS_CODE_COMMAND_SEQUENCE_ERROR]="Command sequence error",
	[NVME_STATUS_CODE_INVALID_SGL_SEGMENT_DESC]="Invalid SGL segment descriptor",
	[NVME_STATUS_CODE_INVALID_SGL_DESC_COUNT]="Invalid SGL descriptor count",
	[NVME_STATUS_CODE_INVALID_DATA_SGL_LENGTH]="Invalid data SGL length",
	[NVME_STATUS_CODE_INVALID_METADATA_SGL_LENGTH]="Invalid metadata SGL length",
	[NVME_STATUS_CODE_INVALID_SGL_DESC_TYPE]="Invalid SGL descriptor type",
	[NVME_STATUS_CODE_INVALID_CMB_USE]="Invalid controller memory buffer use",
	[NVME_STATUS_CODE_INVALID_PRP_OFFSET]="Invalid PRP offset",
	[NVME_STATUS_CODE_ATOMIC_WRITE_UNIT_EXCEEDED]="Atomic write unit exceeded",	
	[NVME_STATUS_CODE_OPERATION_DENIED]="Operation denied",
	[NVME_STATUS_CODE_INVALID_SGL_OFFSET]="Invalid SGL offset",
	[NVME_STATUS_CODE_HOST_IDENT_INCONSISTENT]="Host identification inconsistent",
	[NVME_STATUS_CODE_KEEP_ALIVE_TIMEOUT_EXPIRED]="Keep alive timeout expired",
	[NVME_STATUS_CODE_INVALID_IO_CONTROLLER_STATE]="Invalid I/O controller state",
	[NVME_STATUS_CODE_ABORT_TRIGGERED]="Abort triggered",	
	[NVME_STATUS_CODE_SANITIZE_OPERATION_FAILURE]="Sanitize operation failure",
	[NVME_STATUS_CODE_SANITIZE_IN_PROGRESS]="Sanitize in progress",
	[NVME_STATUS_CODE_INVALID_SGL_GRANULARITY]="Invalid SGL granularity",
	[NVME_STATUS_CODE_CMD_NOT_SUPPORTED]="Command not supported",
	[NVME_STATUS_CODE_NAMESPACE_WRITE_PROTECTED]="Write protected namespace",
	[NVME_STATUS_CODE_CMD_INTERRUPTED]="Command interrupted",
	[NVME_STATUS_CODE_TRANSPORT_ERROR]="Transport error",
	[NVME_STATUS_CODE_LOCKDOWN_VIOLATION]="Lockdown restriction violation",
	[NVME_STATUS_CODE_ADMIN_CMD_MEDIA_NOT_READY]="Admin command media not ready",
	[NVME_STATUS_CODE_LBA_OUT_OF_RANGE]="LBA out of namespace range",
	[NVME_STATUS_CODE_CAPACITY_EXCEEDED]="Capacity exceeded",
};
static const unsigned char* statusCodeTypeNameMap[]={
	[NVME_STATUS_CODE_TYPE_GENERIC_CMD]="Generic command",
	[NVME_STATUS_CODE_TYPE_CMD_SPECIFIC]="Command specific",
	[NVME_STATUS_CODE_TYPE_MEDIA_DATA_INTEGRITY]="Media and data integrity",
	[NVME_STATUS_CODE_TYPE_PATH_RELATED]="Path related",
};
struct nvme_capabilities_register{
	volatile uint64_t max_queue_entry_count:16;
	volatile uint64_t contiguous_queues_required:1;
	volatile uint64_t arbitration_mechanism_supported:2;
	volatile uint64_t reserved0:5;
	volatile uint64_t timeout:8;
	volatile uint64_t doorbell_stride:4;
	volatile uint64_t subsystem_reset_supported:1;
	volatile uint64_t command_sets_supporetd:8;
	volatile uint64_t boot_partition_support:1;
	volatile uint64_t controller_power_support:2;
	volatile uint64_t memory_page_size_minimum:4;
	volatile uint64_t memory_page_size_maximum:4;
	volatile uint64_t persistent_memory_region_supported:1;
	volatile uint64_t controller_memory_buffer_supported:1;
	volatile uint64_t reserved1:6;
}__attribute__((packed));
struct nvme_version_register{
	volatile uint32_t tertiary_version:8;
	volatile uint32_t minor_version:8;
	volatile uint32_t major_version:16;
}__attribute__((packed));
struct nvme_controller_config{
	volatile uint32_t enable:1;
	volatile uint32_t reserved0:3;
	volatile uint32_t io_cmd_set_selected:3;
	volatile uint32_t memory_page_size:4;
	volatile uint32_t arbitration_mechanism_selected:3;
	volatile uint32_t shutdown_notification:2;
	volatile uint32_t io_submission_queue_entry_size:4;
	volatile uint32_t io_completion_queue_entry_size:4;
	volatile uint32_t reserved1:8;
}__attribute__((packed));
struct nvme_controller_status{
	volatile uint32_t ready:1;
	volatile uint32_t controller_fatal_status:1;
	volatile uint32_t shutdown_status:2;
	volatile uint32_t subsystem_reset_occured:1;
	volatile uint32_t processing_paused:1;
	volatile uint32_t reserved0:26;
}__attribute__((packed));
struct nvme_admin_queue_attribs{
	volatile uint32_t admin_submission_queue_size:12;
	volatile uint32_t reserved0:4;
	volatile uint32_t admin_completion_queue_size:12;
	volatile uint32_t reserved1:4;
}__attribute__((packed));
struct nvme_controller_memory_buffer_status{
	volatile uint32_t controller_base_address_invalid:1;
	volatile uint32_t reserved0:31;
}__attribute__((packed));
struct nvme_base_mmio{
	volatile struct nvme_capabilities_register capabilities;
	volatile struct nvme_version_register version;
	volatile uint32_t interrupt_mask_set;
	volatile uint32_t interrupt_mask_clear;
	volatile struct nvme_controller_config controller_config;
	volatile uint32_t reserved0;
	volatile struct nvme_controller_status controller_status;
	volatile uint32_t reset_subsystem;
	volatile struct nvme_admin_queue_attribs admin_queue_attribs;	
	volatile uint64_t admin_submission_queue_base;
	volatile uint64_t admin_completion_queue_base;
	volatile uint32_t controller_buffer_base;
	volatile uint32_t controller_buffer_size;
	volatile uint32_t bootPartitionInfo;
	volatile uint32_t bootPartitionReadSelect;
	volatile uint64_t boot_partition_buffer_base;
	volatile uint64_t controller_buffer_space_ctrl;
	volatile uint32_t controller_buffer_status;
}__attribute__((packed));
struct nvme_completion_qe_status{
	uint16_t phase_bit:1;
	uint16_t status_code:8;
	uint16_t status_code_type:3;
	uint16_t cmd_retry_delay:2;
	uint16_t extra_info_available:1;
	uint16_t do_not_retry:1;
}__attribute__((packed));
struct nvme_cid{
	uint16_t submissionQueueIndex:4;
	uint16_t submissionQeIndex:12;	
}__attribute__((packed));
struct nvme_submission_qe{
	uint8_t opcode;
	uint8_t fused:2;
	uint8_t reserved0:4;
	uint8_t data_transfer_type:2;
	struct nvme_cid cmd_ident;	
	uint32_t nsid;
	uint64_t reserved1;
	uint64_t metadata_ptr;
	uint64_t prp1;
	uint64_t prp2;
	uint8_t cmd_specific[24];
}__attribute__((packed));
struct nvme_completion_qe{
	uint32_t cmd_specific;
	uint32_t reserved0;
	uint16_t submission_queue_head;
	uint16_t submission_queue_id;
	struct nvme_cid cmd_ident;	
	struct nvme_completion_qe_status status;
}__attribute__((packed));
struct nvme_admin_queue_info{
	volatile struct nvme_submission_qe* pSubmissionQueue;
	volatile struct nvme_completion_qe* pCompletionQueue;
	uint64_t submissionEntry;
	uint64_t maxSubmissionEntryCount;
};
struct nvme_completion_queue_desc{
	volatile struct nvme_completion_qe* pCompletionQueue;
	uint64_t pCompletionQueue_phys;	
	uint64_t completionEntry;	
	uint64_t maxCompletionEntryCount;
	uint8_t phase;	
	uint64_t queueId;
	struct nvme_drive_desc* pDriveDesc;
};
struct nvme_completion_qe_desc{
	struct nvme_completion_qe completionQe;
	uint64_t qeIndex;
};
struct nvme_submission_qe_desc{
	uint64_t qeIndex;
	uint8_t cmdComplete;
	struct nvme_completion_qe_desc completionQeDesc;
	struct nvme_submission_queue_desc* pSubmissionQueueDesc;
};
struct nvme_submission_queue_desc{
	volatile struct nvme_submission_qe* pSubmissionQueue;
	uint64_t pSubmissionQueue_phys;
	struct nvme_submission_qe_desc** ppSubmissionQeDescList;	
	uint64_t submissionEntry;
	uint64_t maxSubmissionEntryCount;
	uint8_t queueId;
	uint8_t queueIndex;
	struct nvme_completion_queue_desc* pCompletionQueueDesc;
	struct nvme_drive_desc* pDriveDesc;
};
struct nvme_drive_desc{
	struct pcie_location location;
	struct nvme_submission_queue_desc submissionQueueList[16];	
	uint8_t submissionQueueCount;	
	struct nvme_completion_queue_desc adminCompletionQueueDesc;
	volatile struct nvme_base_mmio* pBaseRegisters;
	uint64_t pBaseRegisters_phys;
	volatile uint32_t* pDoorbellBase;
};
struct nvme_driver_info{
	uint64_t driverId;
	struct nvme_drive_desc** pDriveDescMappingTable;
	uint64_t driveDescMappingTableSize;
};
int nvme_driver_init(void);
int nvme_driver_init_drive_desc_mapping_table(void);
int nvme_enable(struct nvme_drive_desc* pDriveDesc);
int nvme_disable(struct nvme_drive_desc* pDriveDesc);
int nvme_enabled(struct nvme_drive_desc* pDriveDesc);
int nvme_ring_doorbell(struct nvme_drive_desc* pDriveDesc, uint64_t queueId, uint8_t type, uint16_t tail);
int nvme_run_submission_queue(struct nvme_submission_queue_desc* pSubmissionQueueDesc);
int nvme_alloc_completion_queue(struct nvme_drive_desc* pDriveDesc, uint64_t maxEntryCount, uint64_t queueId, struct nvme_completion_queue_desc* pCompletionQueueDesc);
int nvme_free_completion_queue(struct nvme_completion_queue_desc* pCompletionQueueDesc);
int nvme_get_current_completion_qe(struct nvme_completion_queue_desc* pCompletionQueueDesc, volatile struct nvme_completion_qe** ppCompletionQe);
int nvme_acknowledge_completion_qe(struct nvme_completion_queue_desc* pCompletionQueueDesc);
int nvme_get_status_code_name(uint8_t statusCode, const unsigned char** ppName);
int nvme_get_status_code_type_name(uint8_t statusCodeType, const unsigned char** ppName);
int nvme_get_submission_queue_desc(struct nvme_submission_queue_desc* pSubmissionQueueDesc, uint64_t qeIndex, struct nvme_submission_qe_desc** ppSubmisionQeDesc);
int nvme_alloc_submission_qe(struct nvme_submission_queue_desc* pSubmissionQueueDesc, struct nvme_submission_qe entry, struct nvme_submission_qe_desc* pSubmissionQeDesc);
int nvme_alloc_submission_queue(struct nvme_drive_desc* pDriveDesc, uint64_t maxEntryCount, uint64_t queueId, struct nvme_submission_queue_desc** ppSubmissionQueueDesc, struct nvme_completion_queue_desc* pCompletionQueueDesc);
int nvme_free_submission_queue(struct nvme_submission_queue_desc* pSubmissionQueueDesc);
int nvme_init_admin_submission_queue(struct nvme_drive_desc* pDriveDesc);
int nvme_deinit_admin_submission_queue(struct nvme_drive_desc* pDriveDesc);
int nvme_admin_completion_queue_interrupt(uint8_t vector);
int nvme_io_completion_queue_interrupt(uint8_t vector);
int nvme_drive_init(struct pcie_location location);
int nvme_drive_deinit(struct pcie_location location);
int nvme_subsystem_function_register(struct pcie_location location);
int nvme_subsystem_function_unregister(struct pcie_location location);
int nvme_admin_completion_queue_isr(void);
#endif
