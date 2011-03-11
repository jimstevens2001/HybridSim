#ifndef CONFIG_H
#define CONFIG_H

// Default values for alternate code.
#define DEBUG_CACHE 0
#define DEBUG_LOGGER 0
#define SINGLE_WORD 0
#define FDSIM 0
#define NVDSIM 1
#define PRINT_POWER_CB 1
#define SAVE_POWER_CB 1
#define EPOCH_LENGTH 200000


#include <iostream>
#include <string>
#include <sstream>
#include <list>
#include <set>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <cstdlib>
#include <ctime>
#include <stdint.h>
#include <vector>

//#include <MemorySystem.h>
#include <DRAMSim.h>

// Additional things I reuse from DRAMSim repo (for internal use only).
#include <Transaction.h>
#include <SimulatorObject.h>
using DRAMSim::SimulatorObject;
using DRAMSim::TransactionType;
using DRAMSim::DATA_READ;
using DRAMSim::DATA_WRITE;

#if FDSIM
//#include "FDSim.h"
#include <FlashDIMM.h>
#elif NVDSIM
#include <NVDIMM.h>
#endif

using namespace std;

// GLOBAL CONSTANTS (move to ini file eventually)

// these values are also specified in the ini file of the nvdimm but have a different name
const uint64_t WORD_SIZE = 8; // This should never change, but is just paranoia just in case we need 32-bit words.
const uint64_t PAGE_SIZE = 1024*4; // in bytes, so divide this by 64 to get the number of DDR3 transfers per page



const uint64_t SET_SIZE = 64; // associativity of cache

const uint64_t BURST_SIZE = 64; // number of bytes in a single transaction, this means with PAGE_SIZE=1024, 16 transactions are needed
const uint64_t FLASH_BURST_SIZE = 4096; // number of bytes in a single flash transaction

// Number of pages total and number of pages in the cache
const uint64_t TOTAL_PAGES = 2097152/4; // 2 GB
//const uint64_t TOTAL_PAGES = 4194304; // 4 GB
const uint64_t CACHE_PAGES = 1048576/4; // 1 GB


// Defined in marss memoryHierachy.cpp.
// Need to confirm this and make it more flexible later.
const uint64_t CYCLES_PER_SECOND = 667000000;

// INI files
const string dram_ini = "ini/DDR3_micron_8M_8B_x8_sg15.ini";
const string flash_ini = "ini/samsung_K9XXG08UXM(mod).ini";
const string sys_ini = "ini/system.ini";



// Derived constants/macros

const uint64_t NUM_SETS = CACHE_PAGES / SET_SIZE;

#define PAGE_NUMBER(addr) (addr / PAGE_SIZE)
#define PAGE_ADDRESS(addr) ((addr / PAGE_SIZE) * PAGE_SIZE)
#define PAGE_OFFSET(addr) (addr % PAGE_SIZE)
#define SET_INDEX(addr) (PAGE_NUMBER(addr) % NUM_SETS)
#define TAG(addr) (PAGE_NUMBER(addr) / NUM_SETS)
#define ALIGN(addr) (((addr / BURST_SIZE) * BURST_SIZE) % (TOTAL_PAGES * PAGE_SIZE))


class cache_line
{
        public:
        bool valid;
        bool dirty;
        uint64_t tag;
        uint64_t data;
        uint64_t ts;

        cache_line() : valid(false), dirty(false), tag(0), data(0), ts(0) {}
        string str() { stringstream out; out << "T=" << tag << " D=" << data << " V=" << valid << " D=" << dirty << " ts=" << ts; return out.str(); }

};

enum PendingOperation
{
	VICTIM_READ, // Read victim line from DRAM
	VICTIM_WRITE, // Write victim line to Flash
	LINE_READ, // Read new line from Flash
	CACHE_READ, // Perform a DRAM read and return the final result.
	CACHE_WRITE // Perform a DRAM read and return the final result.
};

class Pending
{
	public:
	PendingOperation op; // What operation is being performed.
	uint64_t orig_addr;
	uint64_t flash_addr;
	uint64_t cache_addr;
	uint64_t victim_tag;
	DRAMSim::TransactionType type; // DATA_READ or DATA_WRITE
	unordered_set<uint64_t> *wait;

	Pending() : op(VICTIM_READ), flash_addr(0), cache_addr(0), victim_tag(0), type(DATA_READ), wait(0) {};
        string str() { stringstream out; out << "O=" << op << " F=" << flash_addr << " C=" << cache_addr << " V=" << victim_tag 
		<< " T=" << type; return out.str(); }

	void init_wait()
	{
		wait = new unordered_set<uint64_t>;

	}

	void insert_wait(uint64_t n)
	{
		wait->insert(n);
	}

	void delete_wait()
	{
		delete wait;
		wait = NULL;
	}
};


#endif
