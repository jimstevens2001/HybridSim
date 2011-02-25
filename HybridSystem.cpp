#include "HybridSystem.h"

using namespace std;

namespace HybridSim {

HybridSystem::HybridSystem(uint id)
{
	systemID = id;
	cout << "Creating DRAM" << endl;
	//dram = new DRAMSim::MemorySystem(0, dram_ini, sys_ini, ".", "resultsfilename"); 
	dram = DRAMSim::getMemorySystemInstance(0, dram_ini, sys_ini, "../NVHybridSim", "resultsfilename", (CACHE_PAGES * PAGE_SIZE) >> 20);
	cout << "Creating Flash" << endl;
#if FDSIM
	flash = new FDSim::FlashDIMM(1,"ini/samsung_K9XXG08UXM.ini","ini/def_system.ini","../NVHybridSim","");
#elif NVDSIM
	flash = new NVDSim::NVDIMM(1,"ini/samsung_K9XXG08UXM(pcm).ini","ini/def_system.ini","../NVHybridSim","");
	cout << "Did NVDSIM" << endl;
#else
	flash = DRAMSim::getMemorySystemInstance(1, flash_ini, sys_ini, "../HybridSim", "resultsfilename2", (TOTAL_PAGES * PAGE_SIZE) >> 20); 
#endif
	cout << "Done with creating memories" << endl;

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
#elif NVDSIM
	typedef NVDSim::Callback <HybridSystem, void, uint, uint64_t, uint64_t> nvdsim_callback_t;
	typedef NVDSim::Callback <HybridSystem, void, uint, vector<vector<double>>, uint64_t> nvdsim_callback_v;
	NVDSim::Callback_t *nv_read_cb = new nvdsim_callback_t(this, &HybridSystem::FlashReadCallback);
	NVDSim::Callback_t *nv_write_cb = new nvdsim_callback_t(this, &HybridSystem::FlashWriteCallback);
	NVDSim::Callback_v *nv_power_cb = new nvdsim_callback_v(this, &HybridSystem::FlashPowerCallback);
	flash->RegisterCallbacks(nv_read_cb, nv_write_cb, nv_power_cb);
#else
	read_cb = new dramsim_callback_t(this, &HybridSystem::FlashReadCallback);
	write_cb = new dramsim_callback_t(this, &HybridSystem::FlashWriteCallback);
	flash->RegisterCallbacks(read_cb, write_cb, NULL);
#endif

	// Power stuff
	idle_energy = vector<double>(NUM_PACKAGES, 0.0); 
	access_energy = vector<double>(NUM_PACKAGES, 0.0); 
	erase_energy = vector<double>(NUM_PACKAGES, 0.0); 
	vpp_idle_energy = vector<double>(NUM_PACKAGES, 0.0); 
	vpp_access_energy = vector<double>(NUM_PACKAGES, 0.0); 
	vpp_erase_energy = vector<double>(NUM_PACKAGES, 0.0); 


	// debug stuff to remove later
	pending_count = 0;
	max_dram_pending = 0;
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

	if (dram_pending.size() > max_dram_pending)
		max_dram_pending = dram_pending.size();
	if (pending_sets.size() > pending_sets_max)
		pending_sets_max = pending_sets.size();
	if (pending_pages.size() > pending_pages_max)
		pending_pages_max = pending_pages.size();
	if (trans_queue.size() > trans_queue_max)
		trans_queue_max = trans_queue.size();
	


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
		list<DRAMSim::Transaction>::iterator it2;
		for (it2=trans_queue.begin(); it2 != trans_queue.end(); ++it2)
		{
			cout << " " << (*it2).address;
		}
		cout << ")\n";
		cout << "dram_queue=" << dram_queue.size() << " flash_queue=" << flash_queue.size() << "\n";
		cout << "dram_pending=" << dram_pending.size() << " flash_pending=" << flash_pending.size() << "\n\n";
	}
	list<DRAMSim::Transaction>::iterator it = trans_queue.begin();
	//while(!trans_queue.empty())
	//for (list<Transaction>::iterator it = trans_queue.begin(); it != trans_queue.end(); ++it)
	//while(it != trans_queue.end())
	while((it != trans_queue.end()) && (pending_sets.size() < (CACHE_PAGES / SET_SIZE)))
	{
		//ProcessTransaction(trans_queue.front());
		//trans_queue.pop_front();

		// Compute the page address.
		uint64_t page_addr = PAGE_ADDRESS(ALIGN((*it).address));


		//if (pending_pages.count(page_addr) == 0)
		if (pending_sets.count(SET_INDEX(page_addr)) == 0)
		//if (true)
		{
			//cout << "PAGE NOT IN PENDING" << page_addr << "\n";
			// Add to the pending 
			//cout << "Inserted " << page_addr << " into pending pages set\n";
			pending_pages.insert(page_addr);
			pending_sets.insert(SET_INDEX(page_addr));

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
		DRAMSim::Transaction tmp = dram_queue.front();
		bool isWrite;
		if (tmp.transactionType == DATA_WRITE)
			isWrite = true;
		else
			isWrite = false;
		not_full = dram->addTransaction(isWrite, tmp.address);
		if (not_full)
		{
			dram_queue.pop_front();
			dram_pending_set.insert(tmp.address);
		}
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
#elif NVDSIM
		// put some code to deal with NVDSim interactions here
		//cout << "adding a flash transaction" << endl;
		DRAMSim::Transaction t = flash_queue.front();
		NVDSim::FlashTransaction ft = NVDSim::FlashTransaction(static_cast<NVDSim::TransactionType>(t.transactionType), t.address, t.data);
		//cout << "the address sent to flash was " << t.address << endl;
		not_full = flash->add(ft);
#else
		//not_full = flash->addTransaction(flash_queue.front());
		DRAMSim::Transaction tmp = flash_queue.front();
		bool isWrite;
		if (tmp.transactionType == DATA_WRITE)
			isWrite = true;
		else
			isWrite = false;
		not_full = flash->addTransaction(isWrite, tmp.address);
#endif
		if (not_full){
			flash_queue.pop_front();
			//cout << "popping front of flash queue" << endl;
		}
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

bool HybridSystem::addTransaction(DRAMSim::Transaction &trans)
{

	pending_count += 1;

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

void HybridSystem::ProcessTransaction(DRAMSim::Transaction &trans)
{
	uint64_t addr = ALIGN(trans.address);

//	if (addr != trans.address)
//	{
//		cout << "Assertion fail: aligned address and oriignal address are different.\n";
//		cout << "aligned=0x" << hex << addr << " orig=0x" << trans.address << "\n";
//		abort();
//	}

#if DEBUG_CACHE
	cout << currentClockCycle << ": " << "Starting transaction for address " << addr << endl;
#endif

	if (addr >= (TOTAL_PAGES * PAGE_SIZE))
	{
		cout << "ERROR: Address out of bounds - orig:" << trans.address << " aligned:" << addr << "\n";
		abort();
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
			CacheRead(trans.address, addr, cache_address);
		else if(trans.transactionType == DATA_WRITE)
			CacheWrite(trans.address, addr, cache_address);
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
		cout << currentClockCycle << ": " << "The victim is dirty? " << cur_line.dirty << endl;
#endif

		Pending p;
		p.orig_addr = trans.address;
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
	p.init_wait();
	for(uint64_t i=0; i<PAGE_SIZE/BURST_SIZE; i++)
	{
		uint64_t addr = p.cache_addr + i*BURST_SIZE;
		p.insert_wait(addr);
		DRAMSim::Transaction t = DRAMSim::Transaction(DATA_READ, addr, NULL);
		dram_queue.push_back(t);
	}
#endif

	// Add a record in the DRAM's pending table.
	p.op = VICTIM_READ;
	assert(dram_pending.count(p.cache_addr) == 0);
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
	// Schedule writes for the entire page.
	for(uint64_t i=0; i<PAGE_SIZE/FLASH_BURST_SIZE; i++)
	{
		DRAMSim::Transaction t = DRAMSim::Transaction(DATA_WRITE, victim_flash_addr + i*FLASH_BURST_SIZE, NULL);
		flash_queue.push_back(t);
	}
#endif
	
	// No pending event schedule necessary (might add later for debugging though).
}

void HybridSystem::LineRead(Pending p)
{
#if DEBUG_CACHE
	cout << currentClockCycle << ": " << "Performing LINE_READ for (" << p.flash_addr << ", " << p.cache_addr << ")\n";
	cout << "the page address was " << PAGE_ADDRESS(p.flash_addr) << endl;
#endif

	uint64_t page_addr = PAGE_ADDRESS(p.flash_addr);

#if SINGLE_WORD
	// Schedule a read from Flash to get the new line 
	DRAMSim::Transaction t = DRAMSim::Transaction(DATA_READ, page_addr, NULL);
	flash_queue.push_back(t);
#else
	// Schedule reads for the entire page.
	p.init_wait();
	for(uint64_t i=0; i<PAGE_SIZE/FLASH_BURST_SIZE; i++)
	{
		uint64_t addr = page_addr + i*FLASH_BURST_SIZE;
		p.insert_wait(addr);
		DRAMSim::Transaction t = DRAMSim::Transaction(DATA_READ, addr, NULL);
		flash_queue.push_back(t);
	}
#endif

	// Add a record in the Flash's pending table.
	p.op = LINE_READ;
	flash_pending[page_addr] = p;
}

void HybridSystem::CacheRead(uint64_t orig_addr, uint64_t flash_addr, uint64_t cache_addr)
{
#if DEBUG_CACHE
	cout << currentClockCycle << ": " << "Performing CACHE_READ for (" << flash_addr << ", " << cache_addr << ")\n";
#endif

	// Compute the actual DRAM address of the data word we care about.
	uint64_t data_addr = cache_addr + PAGE_OFFSET(flash_addr);

	assert(cache_addr == PAGE_ADDRESS(data_addr));

	DRAMSim::Transaction t = DRAMSim::Transaction(DATA_READ, data_addr, NULL);
	dram_queue.push_back(t);

	// Update the cache state
	cache_line cur_line = cache[cache_addr];
	cur_line.ts = currentClockCycle;
	cache[cache_addr] = cur_line;

	// Add a record in the DRAM's pending table.
	Pending p;
	p.op = CACHE_READ;
	p.orig_addr = orig_addr;
	p.flash_addr = flash_addr;
	p.cache_addr = cache_addr;
	p.type = DATA_READ;
	assert(dram_pending.count(cache_addr) == 0);
	dram_pending[cache_addr] = p;

	assert(dram_pending.count(PAGE_ADDRESS(data_addr)) != 0);
	assert(dram_pending.count(cache_addr) != 0);
}


void HybridSystem::CacheWrite(uint64_t orig_addr, uint64_t flash_addr, uint64_t cache_addr)
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
		(*WriteDone)(systemID, orig_addr, currentClockCycle);
	}

	// Erase the page from the pending set.
	int num = pending_pages.erase(PAGE_ADDRESS(flash_addr));
	int num2 = pending_sets.erase(SET_INDEX(PAGE_ADDRESS(flash_addr)));
	pending_count -= 1;
	if ((num != 1) || (num2 != 1))
	{
		cout << "pending_pages.erase() was called after CACHE_WRITE and num was 0.\n";
		cout << "orig:" << orig_addr << " aligned:" << flash_addr << "\n\n";
		abort();
	}
}

void HybridSystem::RegisterCallbacks(
	    TransactionCompleteCB *readDone,
	    TransactionCompleteCB *writeDone
	    /*void (*reportPower)(double bgpower, double burstpower, double refreshpower, double actprepower)*/)
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
		assert(dram_pending.count(PAGE_ADDRESS(addr)) == 0);

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
				dram_pending_set.erase(addr);
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
				(*ReadDone)(systemID, p.orig_addr, cycle);
			}

			// Erase the page from the pending set.
			int num = pending_pages.erase(PAGE_ADDRESS(p.flash_addr));
			int num2 = pending_sets.erase(SET_INDEX(PAGE_ADDRESS(p.flash_addr)));
			pending_count -= 1;
			if ((num != 1) || (num2 != 1))
			{
				cout << "pending_pages.erase() was called after CACHE_READ and num was 0.\n";
				cout << "orig:" << p.orig_addr << " aligned:" << p.flash_addr << "\n\n";
				abort();
			}
		}
		else
		{
			ERROR("DRAMReadCallback received an invalid op.");
			abort();
		}
	}
	else
	{
		ERROR("DRAMReadCallback received an address not in the pending set.");

		// DEBUG CODE (remove this later)
		cout << "dram_pending.size() = " << dram_pending.size() << "\n";
		for (unordered_map<uint64_t, Pending>::iterator it = dram_pending.begin(); it != dram_pending.end(); it++)
		{
			cout << (*it).first << " ";
		}
	

		cout << "\n\ndram_queue.size() = " << dram_queue.size() << "\n";
		
		cout << "\n\ndram_pending_set.size() = " << dram_pending_set.size() << "\n";
		for (set<uint64_t>::iterator it = dram_pending_set.begin(); it != dram_pending_set.end(); it++)
		{
			cout << (*it) << " ";
		}
		cout << "\n\n" << "addr= " << addr << endl;
		cout << "PAGE_ADDRESS(addr)= " << PAGE_ADDRESS(addr) << "\n";
		cout << "max_dram_pending= " << max_dram_pending << "\n";

		dram_bad_address.push_back(addr);
			
		// end debug code

		

		abort();
	}

	// Erase from the pending set AFTER everything else (so I can see if failed values are in the pending set).
	dram_pending_set.erase(addr);
}

void HybridSystem::DRAMWriteCallback(uint id, uint64_t addr, uint64_t cycle)
{
	// Nothing to do (it doesn't matter when the DRAM write finishes for the cache controller, as long as it happens).
	dram_pending_set.erase(addr);
}

void HybridSystem::DRAMPowerCallback(double a, double b, double c, double d)
{
  	printf("power callback: %0.3f, %0.3f, %0.3f, %0.3f\n",a,b,c,d);
}

void HybridSystem::FlashReadCallback(uint id, uint64_t addr, uint64_t cycle)
{
        cout << flash_pending.count(PAGE_ADDRESS(addr)) << endl;
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
				CacheRead(p.orig_addr, p.flash_addr, p.cache_addr);
			else if(p.type == DATA_WRITE)
				CacheWrite(p.orig_addr, p.flash_addr, p.cache_addr);
			
		}
		else
		{
			ERROR("FlashReadCallback received an invalid op.");
			abort();
		}
	}
	else
	{
		ERROR("FlashReadCallback received an address not in the pending set.");
		abort();
	}
}

void HybridSystem::FlashWriteCallback(uint id, uint64_t addr, uint64_t cycle)
{
	// Nothing to do (it doesn't matter when the flash write finishes for the cache controller, as long as it happens).

#if DEBUG_CACHE
	cout << "The write to Flash line " << PAGE_ADDRESS(addr) << " has completed.\n";
	//Remove this pending object from flash_pending
	//flash_pending.erase(PAGE_ADDRESS(addr));
#endif
}

void HybridSystem::FlashPowerCallback(uint id, vector<vector<double>> power_data, uint64_t cycle)
{
  // Total power used
  vector<double> total_energy = vector<double>(NUM_PACKAGES, 0.0);
  vector<double> ave_idle_power = vector<double>(NUM_PACKAGES, 0.0);
  vector<double> ave_access_power = vector<double>(NUM_PACKAGES, 0.0);
  vector<double> ave_erase_power = vector<double>(NUM_PACKAGES, 0.0);
  vector<double> ave_vpp_idle_power = vector<double>(NUM_PACKAGES, 0.0);
  vector<double> ave_vpp_access_power = vector<double>(NUM_PACKAGES, 0.0);
  vector<double> ave_vpp_erase_power = vector<double>(NUM_PACKAGES, 0.0);
  vector<double> average_power = vector<double>(NUM_PACKAGES, 0.0);

  for(int i = 0; i < NUM_PACKAGES; i++)
  {
    idle_energy[i] = power_data[0][i];
    access_energy[i] = power_data[1][i];
    ave_idle_power[i] = idle_energy[i] / cycle;
    ave_access_power[i] = access_energy[i] / cycle;

    total_energy[i] = power_data[0][i] + power_data[1][i];  

    if(power_data.size() == 6)
    {
       erase_energy[i] = power_data[2][i];
       vpp_idle_energy[i] = power_data[3][i];
       vpp_access_energy[i] = power_data[4][i];
       vpp_erase_energy[i] = power_data[5][i];  
       total_energy[i] = total_energy[i] + power_data[2][i] + power_data[3][i]
	                 + power_data[4][i] + power_data[5][i];

       
       ave_erase_power[i] = erase_energy[i] / cycle;
       ave_vpp_idle_power[i] = vpp_idle_energy[i] / cycle;
       ave_vpp_access_power[i] = vpp_access_energy[i] / cycle;
       ave_vpp_erase_power[i] = vpp_erase_energy[i] / cycle;
    }
    else if(power_data.size() == 4)
    {
       vpp_idle_energy[i] = power_data[2][i];
       vpp_access_energy[i] = power_data[3][i];
       total_energy[i] = total_energy[i] + power_data[2][i] + power_data[3][i];

       ave_vpp_idle_power[i] = vpp_idle_energy[i] / cycle;
       ave_vpp_access_power[i] = vpp_access_energy[i] / cycle;
    }
    else if(power_data.size() == 3)
    {
       erase_energy[i] = power_data[2][i];
       total_energy[i] = total_energy[i] + power_data[2][i];

       ave_erase_power[i] = erase_energy[i] / cycle;
    }

    average_power[i] = total_energy[i] / cycle;
  }

#if PRINT_POWER_CB
  cout<<"\nCallback Power Data: \n";
  cout<<"========================\n";

  for(uint i = 0; i < NUM_PACKAGES; i++)
  {
        cout<<"Package: "<<i<<"\n";
	cout<<"Accumulated Idle Energy: "<<idle_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	cout<<"Accumulated Access Energy: "<<access_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";

        if(power_data.size() == 6)
	{
	  cout<<"Accumulated Erase Energy: "<<erase_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	  cout<<"Accumulated VPP Idle Energy: "<<vpp_idle_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	  cout<<"Accumulated VPP Access Energy: "<<vpp_access_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	  cout<<"Accumulated VPP Erase Energy: "<<vpp_erase_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	}
	else if(power_data.size() == 4)
	{
	  cout<<"Accumulated VPP Idle Energy: "<<vpp_idle_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	  cout<<"Accumulated VPP Access Energy: "<<vpp_access_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	}
	else if(power_data.size() == 3)
	{
	  cout<<"Accumulated Erase Energy: "<<erase_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	}
	cout<<"Total Energy: "<<total_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n\n";
        
	cout<<"Average Idle Power: "<<ave_idle_power[i]<<"mW\n";
	cout<<"Average Access Power: "<<ave_access_power[i]<<"mW\n";
	if(power_data.size() == 6)
	{
	  cout<<"Average Erase Power: "<<ave_erase_power[i]<<"mW\n";
	  cout<<"Average VPP Idle Power: "<<ave_vpp_idle_power[i]<<"mW\n";
	  cout<<"Average VPP Access Power: "<<ave_vpp_access_power[i]<<"mW\n";
	  cout<<"Average VPP Erase Power: "<<ave_vpp_erase_power[i]<<"mW\n";
	}
	else if(power_data.size() == 4)
	{
	  cout<<"Average VPP Idle Power: "<<ave_vpp_idle_power[i]<<"mW\n";
	  cout<<"Average VPP Access Power: "<<ave_vpp_access_power[i]<<"mW\n";
	}
	else if(power_data.size() == 3)
	{
	  cout<<"Average Erase Power: "<<ave_erase_power[i]<<"mW\n";
	}
	cout<<"Average Power: "<<average_power[i]<<"mW\n\n";
  }

  // Get statisics on the number of reads, writes and erases at this point in the simulation
  flash->printStats();
#endif

#if SAVE_POWER_CB

  std::ofstream savefile;
  savefile.open("PowerStats.log", ios_base::out | ios_base::trunc);

  if (!savefile) 
  {
      	ERROR("Cannot open PowerStats.log");
       	exit(-1); 
  }

  savefile<<"\nCallback Power Data: \n";
  savefile<<"========================\n";

  for(uint i = 0; i < NUM_PACKAGES; i++)
  {
        savefile<<"Package: "<<i<<"\n";
	savefile<<"Accumulated Idle Energy: "<<idle_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	savefile<<"Accumulated Access Energy: "<<access_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";

        if(power_data.size() == 6)
	{
	  savefile<<"Accumulated Erase Energy: "<<erase_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	  savefile<<"Accumulated VPP Idle Energy: "<<vpp_idle_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	  savefile<<"Accumulated VPP Access Energy: "<<vpp_access_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	  savefile<<"Accumulated VPP Erase Energy: "<<vpp_erase_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	}
	else if(power_data.size() == 4)
	{
	  savefile<<"Accumulated VPP Idle Energy: "<<vpp_idle_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	  savefile<<"Accumulated VPP Access Energy: "<<vpp_access_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	}
	else if(power_data.size() == 3)
	{
	  savefile<<"Accumulated Erase Energy: "<<erase_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	}
	savefile<<"Total Energy: "<<total_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n\n";
        
	savefile<<"Average Idle Power: "<<ave_idle_power[i]<<"mW\n";
	savefile<<"Average Access Power: "<<ave_access_power[i]<<"mW\n";
	if(power_data.size() == 6)
	{
	  savefile<<"Average Erase Power: "<<ave_erase_power[i]<<"mW\n";
	  savefile<<"Average VPP Idle Power: "<<ave_vpp_idle_power[i]<<"mW\n";
	  savefile<<"Average VPP Access Power: "<<ave_vpp_access_power[i]<<"mW\n";
	  savefile<<"Average VPP Erase Power: "<<ave_vpp_erase_power[i]<<"mW\n";
	}
	else if(power_data.size() == 4)
	{
	  savefile<<"Average VPP Idle Power: "<<ave_vpp_idle_power[i]<<"mW\n";
	  savefile<<"Average VPP Access Power: "<<ave_vpp_access_power[i]<<"mW\n";
	}
	else if(power_data.size() == 3)
	{
	  savefile<<"Average Erase Power: "<<ave_erase_power[i]<<"mW\n";
	}
	savefile<<"Average Power: "<<average_power[i]<<"mW\n\n";
  }

  savefile<<"Reads completed: "<<flash->numReads<<"\n";
  savefile<<"Writes completed: "<<flash->numWrites<<"\n";
  savefile<<"Erases completed: "<<flash->numErases<<"\n";

  savefile.close();
#endif
  
}

void HybridSystem::reportPower()
{
#if NVDSIM
  flash->powerCallback();
#endif  
}

void HybridSystem::printStats() 
{
	//dram->printStats();
  if(!idle_energy.empty())
  {    
        // Power Stats
        vector<double> total_energy = vector<double>(NUM_PACKAGES, 0.0);
        // Average power used
	vector<double> ave_idle_power = vector<double>(NUM_PACKAGES, 0.0);
	vector<double> ave_access_power = vector<double>(NUM_PACKAGES, 0.0);	
	vector<double> ave_erase_power = vector<double>(NUM_PACKAGES, 0.0);
	vector<double> ave_vpp_idle_power = vector<double>(NUM_PACKAGES, 0.0);
        vector<double> ave_vpp_access_power = vector<double>(NUM_PACKAGES, 0.0);
        vector<double> ave_vpp_erase_power = vector<double>(NUM_PACKAGES, 0.0);
	vector<double> average_power = vector<double>(NUM_PACKAGES, 0.0);

	for(uint i = 0; i < NUM_PACKAGES; i++)
	{
	  if(DEVICE_TYPE.compare("PCM") == 0 && GARBAGE_COLLECT == 1)
	  {
	     total_energy[i] = (idle_energy[i] + access_energy[i] + erase_energy[i]
				+ vpp_idle_energy[i] + vpp_access_energy[i] + vpp_erase_energy[i]);
	  }
	  else if(DEVICE_TYPE.compare("PCM") == 0)
	  {
	    total_energy[i] = (idle_energy[i] + access_energy[i] + vpp_idle_energy[i] + vpp_access_energy[i]);
	  }
	  else if(GARBAGE_COLLECT == 1)
	  {
	    total_energy[i] = (idle_energy[i] + access_energy[i] + erase_energy[i]);
	  }
	  else
	  {
	    total_energy[i] = (idle_energy[i] + access_energy[i]);
	  }
	  ave_idle_power[i] = idle_energy[i] / currentClockCycle;
	  ave_access_power[i] = access_energy[i] / currentClockCycle;
	  ave_erase_power[i] = erase_energy[i] / currentClockCycle;
	  ave_vpp_idle_power[i] = vpp_idle_energy[i] / currentClockCycle;
	  ave_vpp_access_power[i] = vpp_access_energy[i] / currentClockCycle;
	  ave_vpp_erase_power[i] = vpp_erase_energy[i] / currentClockCycle;
	  average_power[i] = total_energy[i] / currentClockCycle;
	}

	cout<<"\nStat Power Data: \n";
	cout<<"========================\n";

	for(uint i = 0; i < NUM_PACKAGES; i++)
	  {
	    cout<<"Package: "<<i<<"\n";
	    cout<<"Accumulated Idle Energy: "<<idle_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	    cout<<"Accumulated Access Energy: "<<access_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	    if(DEVICE_TYPE.compare("PCM") == 0 && GARBAGE_COLLECT == 1)
	    {
	      cout<<"Accumulated Erase Energy: "<<erase_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	      cout<<"Accumulated VPP Idle Energy: "<<vpp_idle_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	      cout<<"Accumulated VPP Access Energy: "<<vpp_access_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	      cout<<"Accumulated VPP Erase Energy: "<<vpp_erase_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	    }
	    else if(DEVICE_TYPE.compare("PCM") == 0)
	    {
	      cout<<"Accumulated VPP Idle Energy: "<<vpp_idle_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	      cout<<"Accumulated VPP Access Energy: "<<vpp_access_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	    }
	    else if(GARBAGE_COLLECT == 1)
	    {
		cout<<"Accumulated Erase Energy: "<<erase_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	    }
	    cout<<"Total Energy: "<<total_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n\n";
        
	    cout<<"Average Idle Power: "<<ave_idle_power[i]<<"mW\n";
	    cout<<"Average Access Power: "<<ave_access_power[i]<<"mW\n";
	    if(DEVICE_TYPE.compare("PCM") == 0 && GARBAGE_COLLECT == 1)
	    {
	      cout<<"Average Erase Power: "<<ave_erase_power[i]<<"mW\n";
	      cout<<"Average VPP Idle Power: "<<ave_vpp_idle_power[i]<<"mW\n";
	      cout<<"Average VPP Access Power: "<<ave_vpp_access_power[i]<<"mW\n";
	      cout<<"Average VPP Erase Power: "<<ave_vpp_erase_power[i]<<"mW\n";
	    }
	    else if(DEVICE_TYPE.compare("PCM") == 0)
	    {
	      cout<<"Average VPP Idle Power: "<<ave_vpp_idle_power[i]<<"mW\n";
	      cout<<"Average VPP Access Power: "<<ave_vpp_access_power[i]<<"mW\n";
	    }
	    else if(GARBAGE_COLLECT == 1)
	    {
		cout<<"Average Erase Power: "<<ave_erase_power[i]<<"mW\n";
	    }
	    cout<<"Average Power: "<<average_power[i]<<"mW\n\n";
	  }
  }
}

void HybridSystem::saveStats()
{
  if(!idle_energy.empty())
  {
        ofstream savefile;
        savefile.open("Results/PowerStats.txt");

        // Power Stats
        vector<double> total_energy = vector<double>(NUM_PACKAGES, 0.0);
        // Average power used
	vector<double> ave_idle_power = vector<double>(NUM_PACKAGES, 0.0);
	vector<double> ave_access_power = vector<double>(NUM_PACKAGES, 0.0);	
	vector<double> ave_erase_power = vector<double>(NUM_PACKAGES, 0.0);
	vector<double> ave_vpp_idle_power = vector<double>(NUM_PACKAGES, 0.0);
        vector<double> ave_vpp_access_power = vector<double>(NUM_PACKAGES, 0.0);
        vector<double> ave_vpp_erase_power = vector<double>(NUM_PACKAGES, 0.0);
	vector<double> average_power = vector<double>(NUM_PACKAGES, 0.0);

	for(uint i = 0; i < NUM_PACKAGES; i++)
	{
	  if(DEVICE_TYPE.compare("PCM") == 0 && GARBAGE_COLLECT == 1)
	  {
	     total_energy[i] = (idle_energy[i] + access_energy[i] + erase_energy[i]
				+ vpp_idle_energy[i] + vpp_access_energy[i] + vpp_erase_energy[i]);
	  }
	  else if(DEVICE_TYPE.compare("PCM") == 0)
	  {
	    total_energy[i] = (idle_energy[i] + access_energy[i] + vpp_idle_energy[i] + vpp_access_energy[i]);
	  }
	  else if(GARBAGE_COLLECT == 1)
	  {
	    total_energy[i] = (idle_energy[i] + access_energy[i] + erase_energy[i]);
	  }
	  else
	  {
	    total_energy[i] = (idle_energy[i] + access_energy[i]);
	  }
	  ave_idle_power[i] = idle_energy[i] / currentClockCycle;
	  ave_access_power[i] = access_energy[i] / currentClockCycle;
	  ave_erase_power[i] = erase_energy[i] / currentClockCycle;
	  ave_vpp_idle_power[i] = vpp_idle_energy[i] / currentClockCycle;
	  ave_vpp_access_power[i] = vpp_access_energy[i] / currentClockCycle;
	  ave_vpp_erase_power[i] = vpp_erase_energy[i] / currentClockCycle;
	  average_power[i] = total_energy[i] / currentClockCycle;
	}

	savefile<<"\nPower Data: \n";
	savefile<<"========================\n";

	for(uint i = 0; i < NUM_PACKAGES; i++)
	  {
	    savefile<<"Package: "<<i<<"\n";
	    savefile<<"Accumulated Idle Energy: "<<idle_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	    savefile<<"Accumulated Access Energy: "<<access_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	    if(DEVICE_TYPE.compare("PCM") == 0 && GARBAGE_COLLECT == 1)
	    {
	      savefile<<"Accumulated Erase Energy: "<<erase_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	      savefile<<"Accumulated VPP Idle Energy: "<<vpp_idle_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	      savefile<<"Accumulated VPP Access Energy: "<<vpp_access_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	      savefile<<"Accumulated VPP Erase Energy: "<<vpp_erase_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	    }
	    else if(DEVICE_TYPE.compare("PCM") == 0)
	    {
	      savefile<<"Accumulated VPP Idle Energy: "<<vpp_idle_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	      savefile<<"Accumulated VPP Access Energy: "<<vpp_access_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	    }
	    else if(GARBAGE_COLLECT == 1)
	    {
		savefile<<"Accumulated Erase Energy: "<<erase_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n";
	    }
	    savefile<<"Total Energy: "<<total_energy[i] * (CYCLE_TIME * 0.000000001)<<"mJ\n\n";
        
	    savefile<<"Average Idle Power: "<<ave_idle_power[i]<<"mW\n";
	    savefile<<"Average Access Power: "<<ave_access_power[i]<<"mW\n";
	    if(DEVICE_TYPE.compare("PCM") == 0 && GARBAGE_COLLECT == 1)
	    {
	      savefile<<"Average Erase Power: "<<ave_erase_power[i]<<"mW\n";
	      savefile<<"Average VPP Idle Power: "<<ave_vpp_idle_power[i]<<"mW\n";
	      savefile<<"Average VPP Access Power: "<<ave_vpp_access_power[i]<<"mW\n";
	      savefile<<"Average VPP Erase Power: "<<ave_vpp_erase_power[i]<<"mW\n";
	    }
	    else if(DEVICE_TYPE.compare("PCM") == 0)
	    {
	      savefile<<"Average VPP Idle Power: "<<ave_vpp_idle_power[i]<<"mW\n";
	      savefile<<"Average VPP Access Power: "<<ave_vpp_access_power[i]<<"mW\n";
	    }
	    else if(GARBAGE_COLLECT == 1)
	    {
		savefile<<"Average Erase Power: "<<ave_erase_power[i]<<"mW\n";
	    }
	    savefile<<"Average Power: "<<average_power[i]<<"mW\n\n";
	  }

	savefile<<"Reads completed: "<<flash->numReads<<"\n";
	savefile<<"Writes completed: "<<flash->numWrites<<"\n";
	savefile<<"Erases completed: "<<flash->numErases<<"\n";

	savefile.close();
  }
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

// TODO: probably need to change these computations to work on a finer granularity than pages
uint64_t HybridSystem::get_hit()
{
	list<uint64_t> valid_pages = get_valid_pages();

	if (valid_pages.size() == 0)
	{
		cout << "valid pages list is empty.\n";
		abort();
	}

	// Pick an element number to grab.
	int size = valid_pages.size();
	//cout << "size=" << size << "\n";
	//srand (time(NULL)); // this causes the address to repeat
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
		abort();
	}

	return ret_addr;
}

// TODO: probably need to change these computations to work on a finer granularity than pages
bool HybridSystem::is_hit(uint64_t address)
{
	uint64_t addr = ALIGN(address);

	if (addr >= (TOTAL_PAGES * PAGE_SIZE))
	{
		cout << "ERROR: Address out of bounds" << endl;
		abort();
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

} // Namespace HybridSim
