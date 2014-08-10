#include "stdio.h"
#include "HybridSim_C_Wrapper.h"

void read_cb(uint sysID, uint64_t addr, uint64_t cycle)
{
	printf("read_cb() received address %lu at cycle %lu from sysID %d\n", addr, cycle, sysID);
}
void write_cb(uint sysID, uint64_t addr, uint64_t cycle)
{
	printf("write_cb() received address %lu at cycle %lu from sysID %d\n", addr, cycle, sysID);
}
void notify_cb(uint operation, uint64_t addr, uint64_t cycle)
{
	printf("notify_cb() received address %lu at cycle %lu from operation %d\n", addr, cycle, operation);
}

int main()
{
	int i;

	HybridSim_C_Wrapper_t *hsc = HybridSim_C_getMemorySystemInstance(0, "");
	HybridSim_C_RegisterCallbacks(hsc, read_cb, write_cb);
	HybridSim_C_RegisterNotifyCallback(hsc, notify_cb);
	HybridSim_C_addTransaction(hsc, 1, 0);
	HybridSim_C_addTransaction(hsc, 1, 8);
	HybridSim_C_addTransaction(hsc, 1, 16);
	HybridSim_C_addTransaction(hsc, 1, 24);

	for (i=0; i < 100000; i++)
	{
		HybridSim_C_update(hsc);
	}

	HybridSim_C_WillAcceptTransaction(hsc);
	HybridSim_C_reportPower(hsc);
	HybridSim_C_mmio(hsc, 0, 0);
	HybridSim_C_syncAll(hsc);
	HybridSim_C_printLogfile(hsc);

	return 0;
}
