#ifndef CONFIG_H
#define CONFIG_H

#include <iostream>
#include <string>
#include <sstream>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <cstdlib>
#include <ctime>
#include <stdint.h>

#include <DRAMSim/MemorySystem.h>

// Default values for alternate code.
#define DEBUG_CACHE 0
#define SINGLE_WORD 0

// GLOBAL CONSTANTS (move to ini file eventually)


const uint64_t WORD_SIZE = 8; // This should never change, but is just paranoia just in case we need 32-bit words.
const uint64_t PAGE_SIZE = 1024; // in bytes, so 128 64-bit words
const uint64_t SET_SIZE = 64; // associativity of cache

const uint64_t BURST_SIZE = 64; // number of bytes in a single transaction, this means with PAGE_SIZE=1024, 16 transactions are needed

// Number of pages total and number of pages in the cache
const uint64_t TOTAL_PAGES = 4194304; // 4 GB
const uint64_t CACHE_PAGES = 1048576; // 1 GB

const uint64_t NUM_SETS = CACHE_PAGES / SET_SIZE;

#define PAGE_NUMBER(addr) (addr / PAGE_SIZE)
#define PAGE_ADDRESS(addr) ((addr / PAGE_SIZE) * PAGE_SIZE)
#define PAGE_OFFSET(addr) (addr % PAGE_SIZE)
#define SET_INDEX(addr) (PAGE_NUMBER(addr) % NUM_SETS)
#define TAG(addr) (PAGE_NUMBER(addr) / NUM_SETS)
#define ALIGN(addr) (addr / BURST_SIZE) * BURST_SIZE;

// INI files
const string dram_ini = "ini/DDR3_micron_8M_8B_x8_sg15.ini";
const string flash_ini = "ini/DDR3_micron_32M_8B_x8_sg15.ini";
const string sys_ini = "ini/system.ini";

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
	uint64_t flash_addr;
	uint64_t cache_addr;
	uint64_t victim_tag;
	TransactionType type; // DATA_READ or DATA_WRITE
	unordered_set<uint64_t> *wait;

	Pending() : op(VICTIM_READ), flash_addr(0), cache_addr(0), victim_tag(0), type(DATA_READ), wait(0) {};
        string str() { stringstream out; out << "O=" << op << " F=" << flash_addr << " C=" << cache_addr << " V=" << victim_tag 
		<< " T=" << type; return out.str(); }
	void init_wait(uint64_t base)
	{
		wait = new unordered_set<uint64_t>;

#if DEBUG_CACHE
		cout << "initializing wait set\n";
#endif
		for (uint64_t i=0; i < PAGE_SIZE/BURST_SIZE; i++)
		{
#if DEBUG_CACHE
			cout << base+i*BURST_SIZE << " ";
#endif
			wait->insert(base+i*BURST_SIZE);
		}
#if DEBUG_CACHE
		cout << "\n\n";
#endif
	}
	void delete_wait()
	{
		delete wait;
		wait = NULL;
	}
};



#endif
