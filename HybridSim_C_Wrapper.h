namespace HybridSim 
{

extern "C"
{

struct HybridSim_C_Wrapper;
typedef struct HybridSim_C_Wrapper HybridSim_C_Wrapper_t;

HybridSim_C_Wrapper *HybridSim_C_getMemorySystemInstance(uint id, char *ini);
void HybridSim_C_RegisterCallbacks(HybridSim_C_Wrapper_t *hsc, void (*readDone)(uint, uint64_t, uint64_t), void (*writeDone)(uint, uint64_t, uint64_t));
bool HybridSim_C_addTransaction(HybridSim_C_Wrapper_t *hsc, bool isWrite, uint64_t addr);
bool HybridSim_C_WillAcceptTransaction(HybridSim_C_Wrapper_t *hsc);
void HybridSim_C_update(HybridSim_C_Wrapper_t *hsc);
void HybridSim_C_mmio(HybridSim_C_Wrapper_t *hsc, uint64_t operation, uint64_t address);
void HybridSim_C_syncAll(HybridSim_C_Wrapper_t *hsc);
void HybridSim_C_reportPower(HybridSim_C_Wrapper_t *hsc);
void HybridSim_C_printLogfile(HybridSim_C_Wrapper_t *hsc);
void HybridSim_C_delete(HybridSim_C_Wrapper_t *hsc);


} // End extern "C"
} // End namespace HybridSim
