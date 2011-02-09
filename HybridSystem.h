#ifndef HYBRIDSYSTEM_H
#define HYBRIDSYSTEM_H

//#include <FlashDIMMSim/FlashDIMM.h>

#include "config.h"
#include "CallbackHybrid.h"

namespace HybridSim
{
	class HybridSystem: public SimulatorObject
	{
		public:
		HybridSystem(uint id);
		void update();
		bool addTransaction(bool isWrite, uint64_t addr);
		bool addTransaction(DRAMSim::Transaction &trans);
		bool WillAcceptTransaction();
		void RegisterCallbacks(
			    TransactionCompleteCB *readDone,
			    TransactionCompleteCB *writeDone,
			    void (*reportPower)(double bgpower, double burstpower, double refreshpower, double actprepower));
		void DRAMReadCallback(uint id, uint64_t addr, uint64_t cycle);
		void DRAMWriteCallback(uint id, uint64_t addr, uint64_t cycle);
		void FlashReadCallback(uint id, uint64_t addr, uint64_t cycle);
		void FlashWriteCallback(uint id, uint64_t addr, uint64_t cycle);
		void FlashIdlePower(uint id, vector<double> idle_energy, uint64_t cycle);
		void FlashAccessPower(uint id, vector<double> access_energy, uint64_t cycle);
		void FlashErasePower(uint id, vector<double> erase_energy, uint64_t cycle);

		void printStats();
		string SetOutputFileName(string tracefilename);


		// Functions that schedule pending operations (second part to these operations is in the callbacks).
//		void VictimRead(uint64_t flash_addr, uint64_t victim_cache_addr, uint64_t victim_tag, TransactionType type);
//		void VictimWrite(uint64_t flash_addr, uint64_t victim_cache_addr, uint64_t victim_tag);
//		void LineRead(uint64_t flash_addr, TransactionType type); 

		// Helper functions
		void ProcessTransaction(DRAMSim::Transaction &trans);

		void VictimRead(Pending p);
		void VictimWrite(Pending p);
		void LineRead(Pending p);
		void CacheRead(uint64_t orig_addr, uint64_t flash_addr, uint64_t cache_addr);
		void CacheWrite(uint64_t orig_addr, uint64_t flash_addr, uint64_t cache_addr);

		// Testing function
		bool is_hit(uint64_t address);
		uint64_t get_hit();
		list<uint64_t> get_valid_pages();

		// State
		TransactionCompleteCB *ReadDone;
		TransactionCompleteCB *WriteDone;
		uint systemID;

		DRAMSim::MemorySystem *dram;

#if FDSIM
		FDSim::FlashDIMM *flash;
#elif NVDSIM
		NVDSim::NVDIMM *flash;
#else
		DRAMSim::MemorySystem *flash;
#endif

		unordered_map<uint64_t, cache_line> cache;

		unordered_map<uint64_t, Pending> dram_pending;
		unordered_map<uint64_t, Pending> flash_pending;

		set<uint64_t> pending_pages; // If a page is in the pending set, then skip subsequent transactions to the page.

		list<DRAMSim::Transaction> trans_queue; // Entry queue for the cache controller.
		list<DRAMSim::Transaction> dram_queue; // Buffer to wait for DRAM
		list<DRAMSim::Transaction> flash_queue; // Buffer to wait for Flash

	};

	HybridSystem *getMemorySystemInstance(uint id);

}

#endif
