#ifndef _NVME
#define _NVME
#include <stdint.h>
#include "drivers/pcie.h"
struct nvme_driveinfo{
	struct pcie_location location;
	uint64_t mmio_base;
};
int nvme_init(void);
int nvme_drive_exists(struct pcie_location location);
int nvme_get_drive_info(struct pcie_location location, struct nvme_driveinfo* pInfo);
#endif
