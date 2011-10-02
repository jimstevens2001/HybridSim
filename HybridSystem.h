#ifndef HYBRIDSIM_HYBRIDSYSTEM_H
#define HYBRIDSIM_HYBRIDSYSTEM_H

//#include <FlashDIMMSim/FlashDIMM.h>

#include <iostream>
#include <fstream>

#include "config.h"
#include "util.h"
#include "CallbackHybrid.h"
#include "Logger.h"
#include "IniReader.h"

namespace HybridSim
{
	class HybridSystem: public SimulatorObject
	{
		public:
		HybridSystem(uint id);
		~HybridSystem();
		void update();
		bool addTransaction(bool isWrite, uint64_t addr);
		bool addTransaction(Transaction &trans);
		void addPrefetch(uint64_t flush_addr, uint64_t prefetch_addr);
		bool WillAcceptTransaction();
		/*void RegisterCallbacks(
				TransactionCompleteCB *readDone,
				TransactionCompleteCB *writeDone,
				void (*reportPower)(double bgpower, double burstpower, double refreshpower, double actprepower));*/
		void RegisterCallbacks(
				TransactionCompleteCB *readDone,
				TransactionCompleteCB *writeDone);
		void DRAMReadCallback(uint id, uint64_t addr, uint64_t cycle);
		void DRAMWriteCallback(uint id, uint64_t addr, uint64_t cycle);
		void DRAMPowerCallback(double a, double b, double c, double d);
		void FlashReadCallback(uint id, uint64_t addr, uint64_t cycle, bool unmapped);
		void FlashCriticalLineCallback(uint id, uint64_t addr, uint64_t cycle, bool unmapped);
		void FlashWriteCallback(uint id, uint64_t addr, uint64_t cycle, bool unmapped);

		// Functions to run the callbacks to the module using HybridSim.
		void ReadDoneCallback(uint systemID, uint64_t orig_addr, uint64_t cycle);
		void WriteDoneCallback(uint sysID, uint64_t orig_addr, uint64_t cycle);

		void reportPower();
		string SetOutputFileName(string tracefilename);

		// Print out the logging data for HybridSim only.
		void printLogfile();

		// Save/Restore cache table functions
		void restoreCacheTable();
		void saveCacheTable();


		// Helper functions
		void ProcessTransaction(Transaction &trans);

		void VictimRead(Pending p);
		void VictimReadFinish(uint64_t addr, Pending p);

		void VictimWrite(Pending p);

		void LineRead(Pending p);
		void LineReadFinish(uint64_t addr, Pending p);

		void LineWrite(Pending p);

		void CacheRead(uint64_t orig_addr, uint64_t flash_addr, uint64_t cache_addr);
		void CacheReadFinish(uint64_t addr, Pending p);

		void CacheWrite(uint64_t orig_addr, uint64_t flash_addr, uint64_t cache_addr);
		void CacheWriteFinish(uint64_t orig_addr, uint64_t flash_addr, uint64_t cache_addr, bool callback_sent);

		void Flush(uint64_t cache_addr);

		// Testing function
		bool is_hit(uint64_t address);
		uint64_t get_hit();
		list<uint64_t> get_valid_pages();

		// State
		IniReader iniReader;

		TransactionCompleteCB *ReadDone;
		TransactionCompleteCB *WriteDone;
		uint systemID;

		DRAMSim::MemorySystem *dram;

		NVDSim::NVDIMM *flash;

		unordered_map<uint64_t, cache_line> cache;

		unordered_map<uint64_t, Pending> dram_pending;
		unordered_map<uint64_t, Pending> flash_pending;

		unordered_map<uint64_t, uint64_t> pending_pages; // If a page is in the pending set, then skip subsequent transactions to the page.
		unordered_map<uint64_t, uint64_t> pending_sets; // If a page is in the pending map, then skip subsequent transactions to the page.

		bool check_queue; // If there is nothing to do, don't check the queue until the next event occurs that will make new work.

		uint64_t delay_counter; // Used to stall the controller while it is "doing work".
		Transaction active_transaction; // Used to hold the transaction waiting for SRAM.
		bool active_transaction_flag; // Indicates that a transaction is waiting for SRAM.

		int64_t pending_count;
		set<uint64_t> dram_pending_set;
		list<uint64_t> dram_bad_address;
		uint64_t max_dram_pending;
		uint64_t pending_sets_max;
		uint64_t pending_pages_max;
		uint64_t trans_queue_max;

		list<Transaction> trans_queue; // Entry queue for the cache controller.
		list<Transaction> dram_queue; // Buffer to wait for DRAM
		list<Transaction> flash_queue; // Buffer to wait for Flash

		// Logger is used to store HybridSim-specific logging events.
		Logger log;

		// Prefetch data stores the prefetch sets from the prefetch file.
		// This is stored as a map of lists. It could be stored more compactly as an array of pointers to pointers,
		// but I chose not to since random access is not needed and this makes the code simpler.
		// If space becomes a problem, I'm just going to switch this to loading the data from a file per set at runtime.
		unordered_map<uint64_t, list<uint64_t>> prefetch_access_number;
		unordered_map<uint64_t, list<uint64_t>> prefetch_flush_addr;
		unordered_map<uint64_t, list<uint64_t>> prefetch_new_addr;
		unordered_map<uint64_t, uint64_t> prefetch_counter;

		ofstream debug_victim;
		ofstream debug_nvdimm_trace;
		ofstream debug_full_trace;

	};

	HybridSystem *getMemorySystemInstance(uint id);

}

#endif
