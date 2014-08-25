// This should only be included by C programs, not C++ programs.

#include <stdint.h>
typedef uint32_t uint;
typedef int bool;

//struct HybridSim_C_Wrapper;
//typedef struct HybridSim_C_Wrapper HybridSim_C_Wrapper_t;
typedef void* HybridSim_C_Wrapper_t;

HybridSim_C_Wrapper_t *HybridSim_C_getMemorySystemInstance(uint id, char *ini);
void HybridSim_C_RegisterCallbacks(HybridSim_C_Wrapper_t *hsc, void (*readDone)(uint, uint64_t, uint64_t), void (*writeDone)(uint, uint64_t, uint64_t));
bool HybridSim_C_addTransaction(HybridSim_C_Wrapper_t *hsc, bool isWrite, uint64_t addr);
bool HybridSim_C_WillAcceptTransaction(HybridSim_C_Wrapper_t *hsc);
void HybridSim_C_update(HybridSim_C_Wrapper_t *hsc);
void HybridSim_C_mmio(HybridSim_C_Wrapper_t *hsc, uint64_t operation, uint64_t address);
void HybridSim_C_syncAll(HybridSim_C_Wrapper_t *hsc);
void HybridSim_C_reportPower(HybridSim_C_Wrapper_t *hsc);
void HybridSim_C_printLogfile(HybridSim_C_Wrapper_t *hsc);
void HybridSim_C_RegisterNotifyCallback(HybridSim_C_Wrapper_t *hsc, void (*notify)(uint, uint64_t, uint64_t));
void HybridSim_C_ConfigureNotify(HybridSim_C_Wrapper_t *hsc, uint operation, bool enable);
void HybridSim_C_query(HybridSim_C_Wrapper_t *hsc, uint64_t operation, uint64_t input1, uint64_t input2, uint64_t *output1, uint64_t *output2);
void HybridSim_C_delete(HybridSim_C_Wrapper_t *hsc);


