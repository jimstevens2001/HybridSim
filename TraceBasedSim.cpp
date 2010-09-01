/****************************************************************************
*	 DRAMSim2: A Cycle Accurate DRAM simulator 
*	 
*	 Copyright (C) 2010   	Elliott Cooper-Balis
*									Paul Rosenfeld 
*									Bruce Jacob
*									University of Maryland
*
*	 This program is free software: you can redistribute it and/or modify
*	 it under the terms of the GNU General Public License as published by
*	 the Free Software Foundation, either version 3 of the License, or
*	 (at your option) any later version.
*
*	 This program is distributed in the hope that it will be useful,
*	 but WITHOUT ANY WARRANTY; without even the implied warranty of
*	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	 GNU General Public License for more details.
*
*	 You should have received a copy of the GNU General Public License
*	 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*****************************************************************************/



#include "TraceBasedSim.h"

using namespace HybridSim;
using namespace std;

int complete = 0;

int main()
{
	printf("dramsim_test main()\n");
	some_object obj;
	obj.add_one_and_run();
}


void some_object::read_complete(uint id, uint64_t address, uint64_t clock_cycle)
{
	printf("[Callback] read complete: %d 0x%lx cycle=%lu\n", id, address, clock_cycle);
	complete++;
}

void some_object::write_complete(uint id, uint64_t address, uint64_t clock_cycle)
{
	printf("[Callback] write complete: %d 0x%lx cycle=%lu\n", id, address, clock_cycle);
	complete++;
}

/* FIXME: this may be broken, currently */
void power_callback(double a, double b, double c, double d)
{
	printf("power callback: %0.3f, %0.3f, %0.3f, %0.3f\n",a,b,c,d);
}

int some_object::add_one_and_run()
{
	/* pick a DRAM part to simulate */
	HybridSystem *mem = new HybridSystem(1);
	//MemorySystem *mem = new MemorySystem(0, "ini/DDR3_micron_32M_8B_x8_sg15.ini", "ini/system.ini", "", "");


	/* create and register our callback functions */
	Callback_t *read_cb = new Callback<some_object, void, uint, uint64_t, uint64_t>(this, &some_object::read_complete);
	Callback_t *write_cb = new Callback<some_object, void, uint, uint64_t, uint64_t>(this, &some_object::write_complete);
	mem->RegisterCallbacks(read_cb, write_cb, power_callback);

	srand (time(NULL));

	cout << "Preparing transactions to preload cache with data...\n";
	uint64_t num_init = 10000;
	for (uint64_t i=0; i<num_init; i++)
	{
		Transaction t = Transaction(DATA_READ, i*PAGE_SIZE, NULL);
		cout << i << "calling HybridSystem::addTransaction\n";
		mem->addTransaction(t);
		if (i%10000 == 0)
			cout << i << "/" << num_init << endl;
	}
	cout << "Running transactions to preload cache with data...\n";
	for (uint64_t i=0; i<num_init*1000; i++)
	{
		mem->update();
		if (i%1000000 == 0)
			cout << i << "/" << num_init*1000 << endl;
	}

	uint64_t cur_addr = 0;

	const uint64_t NUM_ACCESSES = 1000;
	const int MISS_RATE = 99;

	for (uint64_t i=0; i<NUM_ACCESSES; i++)
	{
		TransactionType type = DATA_READ;

		// Decide whether to do a hit or a miss with probability miss rate.
		bool want_hit = true;
		if (rand() % 100 < MISS_RATE)
			want_hit = false;

		cout << mem->currentClockCycle << ": want_hit=" << want_hit << " cur_addr=" << cur_addr << endl;

		// Pick the address that will give a hit or a miss.
		while(mem->is_hit(cur_addr) != want_hit)
		{
			cur_addr = (rand() % TOTAL_PAGES) * PAGE_SIZE;
		}
		//cur_addr = (cur_addr + PAGE_SIZE) % (TOTAL_PAGES * PAGE_SIZE);
		

		Transaction t = Transaction(type, cur_addr, NULL);
		mem->addTransaction(t);

#if DEBUG_CACHE
		cout << "\n\tAdded transaction " << i << " of type=" << type << " addr=" << cur_addr << " set=" << SET_INDEX(cur_addr) 
			<< " tag=" << TAG(cur_addr) << endl;
#endif

		for (int j=0; j<1; j++)
		{
			mem->update();
		}
	}


	for (int i=0; i<1000000; i++)
	{
		mem->update();
	}


	cout << "\n\n" << mem->currentClockCycle << ": completed " << complete << "\n\n";
	cout << "dram_pending=" << mem->dram_pending.size() << " flash_pending=" << mem->flash_pending.size() << "\n\n";

	return 0;
}

