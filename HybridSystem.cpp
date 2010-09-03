#include "HybridSystem.h"

using namespace HybridSim;
using namespace std;


HybridSystem::HybridSystem(uint id)
{
	systemID = id;
	cout << "Creating DRAM\n";
	dram = new DRAMSim::MemorySystem(0, dram_ini, sys_ini, ".", "resultsfilename"); 

	cout << "Creating Flash\n";
#if FDSIM
	flash = new FDSim::FlashDIMM(1,"ini/samsung_K9XXG08UXM.ini","ini/def_system.ini",".","");
#else
	flash = new DRAMSim::MemorySystem(1, flash_ini, sys_ini, ".", "resultsfilename"); 
#endif
	cout << "Done with creating memories\n";

	// Set up the callbacks for DRAM.
	typedef DRAMSim::Callback <HybridSystem, void, uint, uint64_t, uint64_t> dramsim_callback_t;
	DRAMSim::TransactionCompleteCB *read_cb = new dramsim_callback_t(this, &HybridSystem::DRAMReadCallback);
	DRAMSim::TransactionCompleteCB *write_cb = new dramsim_callback_t(this, &HybridSystem::DRAMWriteCallback);
	dram->RegisterCallbacks(read_cb, write_cb, NULL);

	// Set up the callbacks for DRAM.
#if FDSIM
	typedef FDSim::Callback <HybridSystem, void, uint, uint64_t, uint64_t> fdsim_callback_t;
	FDSim::Callback_t *f_read_cb = new fdsim_callback_t(this, &HybridSystem::FlashReadCallback);
	FDSim::Callback_t *f_write_cb = new fdsim_callback_t(this, &HybridSystem::FlashWriteCallback);
	flash->RegisterCallbacks(f_read_cb, f_write_cb);
#else
	read_cb = new dramsim_callback_t(this, &HybridSystem::FlashReadCallback);
	write_cb = new dramsim_callback_t(this, &HybridSystem::FlashWriteCallback);
	flash->RegisterCallbacks(read_cb, write_cb, NULL);
#endif
}

// static allocator for the library interface
HybridSystem *getMemorySystemInstance(uint id)
{
        return new HybridSystem(id);
}


void HybridSystem::update()
{
	// Process the transaction queue.
	// This will fill the dram_queue and flash_queue.
	//if (trans_queue.size() > 0)
	//if (currentClockCycle % 1000 == 0)
	if (false)
	{
		cout << currentClockCycle << ": ";
		cout << trans_queue.size() << "/" << pending_pages.size() << " is length of trans_queue/pending_pages (";
//		set<uint64_t>::iterator it;
//		for (it=pending_pages.begin(); it != pending_pages.end(); ++it)
//		{
//			cout << " " << *it;
//		}
		cout << ")\n(";
		list<Transaction>::iterator it2;
		for (it2=trans_queue.begin(); it2 != trans_queue.end(); ++it2)
		{
			cout << " " << (*it2).address;
		}
		cout << ")\n";
		cout << "dram_queue=" << dram_queue.size() << " flash_queue=" << flash_queue.size() << "\n";
		cout << "dram_pending=" << dram_pending.size() << " flash_pending=" << flash_pending.size() << "\n\n";
	}
	list<Transaction>::iterator it = trans_queue.begin();
	//while(!trans_queue.empty())
	//for (list<Transaction>::iterator it = trans_queue.begin(); it != trans_queue.end(); ++it)
	while(it != trans_queue.end())
	{
		//ProcessTransaction(trans_queue.front());
		//trans_queue.pop_front();

		// Compute the page address.
		uint64_t page_addr = PAGE_ADDRESS((*it).address);

		if (pending_pages.count(page_addr) == 0)
		//if (true)
		{
	//		cout << "PAGE NOT IN PENDING" << page_addr << "\n";
			// Add to the pending 
			pending_pages.insert(page_addr);

			// Process the transaction.
			ProcessTransaction(*it);

			// Delete this item and skip to the next.
			it = trans_queue.erase(it);
		}
		else
		{
			// Skip to the next and do nothing else.
	//		cout << "PAGE IN PENDING" << page_addr << "\n";
			++it;
			//it = trans_queue.end();
		}
	}

	// Process DRAM transaction queue until it is empty or addTransaction returns false.
	bool not_full = true;
	while(not_full && !dram_queue.empty())
	{
		not_full = dram->addTransaction(dram_queue.front());
		if (not_full)
			dram_queue.pop_front();
	}

	// Process Flash transaction queue until it is empty or addTransaction returns false.
	not_full = true;
	while(not_full && !flash_queue.empty())
	{
#if FDSIM
		// put some code to deal with FDSim interactions here
		DRAMSim::Transaction t = flash_queue.front();
		FDSim::FlashTransaction ft = FDSim::FlashTransaction(static_cast<FDSim::TransactionType>(t.transactionType), t.address, t.data);
		not_full = flash->add(ft);
#else
		not_full = flash->addTransaction(flash_queue.front());
#endif
		if (not_full)
			flash_queue.pop_front();
	}

	// Update the memories.
	dram->update();
	flash->update();

	// Increment the cycle count.
	step();
}

bool HybridSystem::addTransaction(bool isWrite, uint64_t addr)
{
	DRAMSim::TransactionType type;
	if (isWrite)
	{
		type = DATA_WRITE;
	}
	else
	{
		type = DATA_READ;
	}
	DRAMSim::Transaction t = DRAMSim::Transaction(type, addr, NULL);
	return addTransaction(t);
}

bool HybridSystem::addTransaction(Transaction &trans)
{
	//cout << "enter HybridSystem::addTransaction\n";
	trans_queue.push_back(trans);
	//cout << "pushed\n";

	return true; // TODO: Figure out when this could be false.
}

bool HybridSystem::WillAcceptTransaction()
{
	// Always true for now since MARSS expects this.
	// Might change later.
	return true;
}

void HybridSystem::ProcessTransaction(Transaction &trans)
{
	uint64_t addr = ALIGN(trans.address);
#if DEBUG_CACHE
	cout << currentClockCycle << ": " << "Starting transaction for address " << addr << endl;
#endif

	if (addr >= (TOTAL_PAGES * PAGE_SIZE))
	{
		cout << "ERROR: Address out of bounds" << endl;
		exit(1);
	}

	// Compute the set number and tag
	uint64_t set_index = SET_INDEX(addr);
	uint64_t tag = TAG(addr);

	list<uint64_t> set_address_list;
	for (uint64_t i=0; i<SET_SIZE; i++)
	{
		uint64_t next_address = (i * NUM_SETS + set_index) * PAGE_SIZE;
		set_address_list.push_back(next_address);
	}

	bool hit = false;
	uint64_t cache_address;
	uint64_t cur_address;
	cache_line cur_line;
	for (list<uint64_t>::iterator it = set_address_list.begin(); it != set_address_list.end(); ++it)
	{
		cur_address = *it;
		if (cache.count(cur_address) == 0)
		{
			// If i is not allocated yet, allocate it.
			cache[cur_address] = *(new cache_line());
		}

		cur_line = cache[cur_address];

		if (cur_line.valid && (cur_line.tag == tag))
		{
			hit = true;
			cache_address = cur_address;
#if DEBUG_CACHE
			cout << currentClockCycle << ": " << "HIT: " << cur_address << " " << " " << cur_line.str() << endl;
#endif
			break;
		}

	}

	if (hit)
	{
		if (trans.transactionType == DATA_READ)
			CacheRead(addr, cache_address);
		else if(trans.transactionType == DATA_WRITE)
			CacheWrite(addr, cache_address);
	}

	if (!hit)
	{
		// Select a victim offset within the set (LRU)
		uint64_t victim = *(set_address_list.begin());
		uint64_t min_ts = 0;
		bool min_init = false;

		for (list<uint64_t>::iterator it=set_address_list.begin(); it != set_address_list.end(); it++)
		{
			cur_address = *it;
			cur_line = cache[cur_address];
			if ((cur_line.ts < min_ts) || (!min_init))
			{
				victim = cur_address;	
				min_ts = cur_line.ts;
				min_init = true;
			}
		}

		cache_address = victim;
		cur_line = cache[cache_address];

#if DEBUG_CACHE
		cout << currentClockCycle << ": " << "MISS: victim is cache_address " << cache_address << endl;
		cout << cur_line.str() << endl;
#endif

		Pending p;
		p.flash_addr = addr;
		p.cache_addr = cache_address;
		p.victim_tag = cur_line.tag;
		p.type = trans.transactionType;

		// If the cur_line is dirty, then do a victim writeback process (starting with VictimRead).
		// Otherwise, read the line.
		if (cur_line.dirty)
		{
			VictimRead(p);
		}
		else
		{
			LineRead(p);
		}
	}
}

void HybridSystem::VictimRead(Pending p)
{
#if DEBUG_CACHE
	cout << currentClockCycle << ": " << "Performing VICTIM_READ for (" << p.flash_addr << ", " << p.cache_addr << ")\n";
#endif

	// flash_addr is the original Flash address requested from the top level Transaction.
	// victim is the base address of the DRAM page to read.
	// victim_tag is the cache tag for the victim page (used to compute the victim's flash address).

#if SINGLE_WORD
	// Schedule a read from DRAM to get the line being evicted.
	DRAMSim::Transaction t = DRAMSim::Transaction(DATA_READ, p.cache_addr, NULL);
	dram_queue.push_back(t);
#else
	// Schedule reads for the entire page.
	p.init_wait(p.cache_addr);
	for(uint64_t i=0; i<PAGE_SIZE/BURST_SIZE; i++)
	{
		DRAMSim::Transaction t = DRAMSim::Transaction(DATA_READ, p.cache_addr + i*BURST_SIZE, NULL);
		dram_queue.push_back(t);
	}
#endif

	// Add a record in the DRAM's pending table.
	p.op = VICTIM_READ;
	dram_pending[p.cache_addr] = p;
}

void HybridSystem::VictimWrite(Pending p)
{
#if DEBUG_CACHE
	cout << currentClockCycle << ": " << "Performing VICTIM_WRITE for (" << p.flash_addr << ", " << p.cache_addr << ")\n";
#endif

	// Compute victim flash address.
	// This is where the victim line is stored in the Flash address space.
	uint64_t victim_flash_addr = (p.victim_tag * NUM_SETS + SET_INDEX(p.flash_addr)) * PAGE_SIZE; 

#if SINGLE_WORD
	// Schedule a write to Flash to save the evicted line.
	DRAMSim::Transaction t = DRAMSim::Transaction(DATA_WRITE, victim_flash_addr, NULL);
	flash_queue.push_back(t);
#else
	// Schedule reads for the entire page.
	for(uint64_t i=0; i<PAGE_SIZE/BURST_SIZE; i++)
	{
		DRAMSim::Transaction t = DRAMSim::Transaction(DATA_WRITE, victim_flash_addr + i*BURST_SIZE, NULL);
		flash_queue.push_back(t);
	}
#endif

	// No pending event schedule necessary (might add later for debugging though).
}

void HybridSystem::LineRead(Pending p)
{
#if DEBUG_CACHE
	cout << currentClockCycle << ": " << "Performing LINE_READ for (" << p.flash_addr << ", " << p.cache_addr << ")\n";
#endif

	uint64_t page_addr = PAGE_ADDRESS(p.flash_addr);

#if SINGLE_WORD
	// Schedule a read from Flash to get the new line 
	DRAMSim::Transaction t = DRAMSim::Transaction(DATA_READ, page_addr, NULL);
	flash_queue.push_back(t);
#else
	// Schedule reads for the entire page.
	p.init_wait(page_addr);
	for(uint64_t i=0; i<PAGE_SIZE/BURST_SIZE; i++)
	{
		DRAMSim::Transaction t = DRAMSim::Transaction(DATA_READ, page_addr + i*BURST_SIZE, NULL);
		flash_queue.push_back(t);
	}
#endif

	// Add a record in the Flash's pending table.
	p.op = LINE_READ;
	flash_pending[page_addr] = p;
}

void HybridSystem::CacheRead(uint64_t flash_addr, uint64_t cache_addr)
{
#if DEBUG_CACHE
	cout << currentClockCycle << ": " << "Performing CACHE_READ for (" << flash_addr << ", " << cache_addr << ")\n";
#endif

	// Compute the actual DRAM address of the data word we care about.
	uint64_t data_addr = cache_addr + PAGE_OFFSET(flash_addr);

	DRAMSim::Transaction t = DRAMSim::Transaction(DATA_READ, data_addr, NULL);
	dram_queue.push_back(t);

	// Add a record in the DRAM's pending table.
	Pending p;
	p.op = CACHE_READ;
	p.flash_addr = flash_addr;
	p.cache_addr = cache_addr;
	p.type = DATA_READ;
	dram_pending[cache_addr] = p;
}

void HybridSystem::CacheWrite(uint64_t flash_addr, uint64_t cache_addr)
{
#if DEBUG_CACHE
	cout << currentClockCycle << ": " << "Performing CACHE_WRITE for (" << flash_addr << ", " << cache_addr << ")\n";
#endif

	// Compute the actual DRAM address of the data word we care about.
	uint64_t data_addr = cache_addr + PAGE_OFFSET(flash_addr);

	DRAMSim::Transaction t = DRAMSim::Transaction(DATA_WRITE, data_addr, NULL);
	dram_queue.push_back(t);

	// Update the cache state
	cache_line cur_line = cache[cache_addr];
	cur_line.dirty = true;
	cur_line.valid = true;
	cur_line.ts = currentClockCycle;
	cache[cache_addr] = cur_line;
#if DEBUG_CACHE
	cout << cur_line.str() << endl;
#endif

	// Call the top level callback.
	// This is done immediately rather than waiting for callback.
	if (WriteDone != NULL)
	{
		// Call the callback.
		(*WriteDone)(systemID, flash_addr, currentClockCycle);
	}

	// Erase the page from the pending set.
	int num = pending_pages.erase(PAGE_ADDRESS(flash_addr));
	if (num != 1)
	{
		cout << "pending_pages.erase() was called after CACHE_WRITE and num was 0.\n\n";
		exit(1);
	}
}

void HybridSystem::RegisterCallbacks(
	    DRAMSim::TransactionCompleteCB *readDone,
	    DRAMSim::TransactionCompleteCB *writeDone,
	    void (*reportPower)(double bgpower, double burstpower, double refreshpower, double actprepower))
{
	// Save the external callbacks.
	ReadDone = readDone;
	WriteDone = writeDone;

}


void HybridSystem::DRAMReadCallback(uint id, uint64_t addr, uint64_t cycle)
{
	if (dram_pending.count(PAGE_ADDRESS(addr)) != 0)
	{
		// Get the pending object for this transaction.
		Pending p = dram_pending[PAGE_ADDRESS(addr)];

		// Remove this pending object from dram_pending
		dram_pending.erase(PAGE_ADDRESS(addr));

		if (p.op == VICTIM_READ)
		{
#if DEBUG_CACHE
			cout << currentClockCycle << ": " << "VICTIM_READ callback for (" << p.flash_addr << ", " << p.cache_addr << ") offset="
				<< PAGE_OFFSET(addr) << " num_left=" << p.wait->size() << "\n";
#endif

#if SINGLE_WORD
#else
			// Remove the read that just finished from the wait set.
			p.wait->erase(addr);

			if (!p.wait->empty())
			{
				// If not done with this line, then re-enter pending map.
				dram_pending[PAGE_ADDRESS(addr)] = p;
				return;
			}

			// The line has completed. Delete the wait set object and move on.
			p.delete_wait();
#endif

#if DEBUG_CACHE
			cout << "The read to DRAM line " << PAGE_ADDRESS(addr) << " has completed.\n";
#endif

			// Schedule a write to the flash to simulate the transfer
			VictimWrite(p);

			// Schedule a read to the flash to get the new line (can be done in parallel)
			LineRead(p);
		}
		else if (p.op == CACHE_READ)
		{
#if DEBUG_CACHE
			cout << currentClockCycle << ": " << "CACHE_READ callback for (" << p.flash_addr << ", " << p.cache_addr << ")\n";
#endif

			// Read operation has completed, call the top level callback.
			if (ReadDone != NULL)
			{
				// Call the callback.
				(*ReadDone)(systemID, addr, cycle);
			}

			// Erase the page from the pending set.
			int num = pending_pages.erase(PAGE_ADDRESS(p.flash_addr));
			if (num != 1)
			{
				cout << "pending_pages.erase() was called after CACHE_READ and num was 0.\n\n";
				exit(1);
			}
		}
	}
}

void HybridSystem::DRAMWriteCallback(uint id, uint64_t addr, uint64_t cycle)
{
	// Nothing to do (it doesn't matter when the DRAM write finishes for the cache controller, as long as it happens).
}

void HybridSystem::FlashReadCallback(uint id, uint64_t addr, uint64_t cycle)
{
	if (flash_pending.count(PAGE_ADDRESS(addr)) != 0)
	{
		// Get the pending object.
		Pending p = flash_pending[PAGE_ADDRESS(addr)];

		// Remove this pending object from flash_pending
		flash_pending.erase(PAGE_ADDRESS(addr));

		if (p.op == LINE_READ)
		{
#if DEBUG_CACHE
			cout << currentClockCycle << ": " << "LINE_READ callback for (" << p.flash_addr << ", " << p.cache_addr << ") offset="
				<< PAGE_OFFSET(addr) << " num_left=" << p.wait->size() << "\n";
#endif

#if SINGLE_WORD
#else
			// Remove the read that just finished from the wait set.
			p.wait->erase(addr);

			if (!p.wait->empty())
			{
				// If not done with this line, then re-enter pending map.
				flash_pending[PAGE_ADDRESS(addr)] = p;
				return;
			}

			// The line has completed. Delete the wait set object and move on.
			p.delete_wait();
#endif

#if DEBUG_CACHE
			cout << "The read to Flash line " << PAGE_ADDRESS(addr) << " has completed.\n";
#endif

			// Update the cache state
			cache_line cur_line = cache[p.cache_addr];
                        cur_line.tag = TAG(p.flash_addr);
                        cur_line.dirty = false;
                        cur_line.valid = true;
                        cur_line.ts = currentClockCycle;
			cache[p.cache_addr] = cur_line;


			// Schedule the final operation (CACHE_READ or CACHE_WRITE)
			if (p.type == DATA_READ)
				CacheRead(p.flash_addr, p.cache_addr);
			else if(p.type == DATA_WRITE)
				CacheWrite(p.flash_addr, p.cache_addr);
			
		}
	}
}

void HybridSystem::FlashWriteCallback(uint id, uint64_t addr, uint64_t cycle)
{
	// Nothing to do (it doesn't matter when the flash write finishes for the cache controller, as long as it happens).
}

void HybridSystem::printStats() 
{
	dram->printStats();
}

string HybridSystem::SetOutputFileName(string tracefilename) { return ""; }

list<uint64_t> HybridSystem::get_valid_pages()
{
	//cout << "IN get_valid_pages()\n";
	list<uint64_t> valid_pages;

	unordered_map<uint64_t, cache_line>::iterator it;
	for (it = cache.begin(); it != cache.end(); ++it)
	{
		uint64_t addr = (*it).first;
		cache_line line = (*it).second;

		//cout << addr << " : " << line.str() << "\n";

		if (line.valid)
		{
			valid_pages.push_back(addr);
		}
	}

	return valid_pages;
}

uint64_t HybridSystem::get_hit()
{
	list<uint64_t> valid_pages = get_valid_pages();

	// Pick an element number to grab.
	int size = valid_pages.size();
	//cout << "size=" << size << "\n";
	srand (time(NULL));
	//cout << "pending=" << pending_pages.size() << " flash_pending size=" << flash_pending.size() << "\n";
	int x = rand() % size;
	//cout << "x=" << x << "\n";


	int i = 0;
	list<uint64_t>::iterator it2 = valid_pages.begin();
	while((i != x) && (it2 != valid_pages.end()))
	{
		i++;
		it2++;
	}

	uint64_t cache_addr = (*it2);

	// Compute flash address
	cache_line c = cache[cache_addr];
	uint64_t ret_addr = (c.tag * NUM_SETS + SET_INDEX(cache_addr)) * PAGE_SIZE;

	//cout << "cache_addr=" << cache_addr << " ";
	//cout << "ret_addr=" << ret_addr << " tag=" << TAG(ret_addr) << " cache set=" << SET_INDEX(cache_addr) << " ret set=" << SET_INDEX(ret_addr) << "\n";
	//cout << c.str() << "\n";

	// Check assertion.
	if (!is_hit(ret_addr))
	{
		cout << "get_hit generated a non-hit!!\n";
		exit(1);
	}

	return ret_addr;
}

bool HybridSystem::is_hit(uint64_t address)
{
	uint64_t addr = ALIGN(address);

	if (addr >= (TOTAL_PAGES * PAGE_SIZE))
	{
		cout << "ERROR: Address out of bounds" << endl;
		exit(1);
	}

	// Compute the set number and tag
	uint64_t set_index = SET_INDEX(addr);
	uint64_t tag = TAG(addr);

	//cout << "set address list: ";
	list<uint64_t> set_address_list;
	for (uint64_t i=0; i<SET_SIZE; i++)
	{
		uint64_t next_address = (i * NUM_SETS + set_index) * PAGE_SIZE;
		set_address_list.push_back(next_address);
		//cout << next_address << " ";
	}
	//cout << "\n";

	bool hit = false;
	uint64_t cache_address;
	uint64_t cur_address;
	cache_line cur_line;
	for (list<uint64_t>::iterator it = set_address_list.begin(); it != set_address_list.end(); ++it)
	{
		cur_address = *it;
		if (cache.count(cur_address) == 0)
		{
			// If i is not allocated yet, allocate it.
			cache[cur_address] = *(new cache_line());
		}

		cur_line = cache[cur_address];

		if (cur_line.valid && (cur_line.tag == tag))
		{
	//		cout << "HIT!!!\n";
			hit = true;
			cache_address = cur_address;
			break;
		}
	}

	return hit;

}
