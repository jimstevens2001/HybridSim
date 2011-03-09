#include "Logger.h"

using namespace std;

namespace HybridSim 
{
	Logger::Logger()
	{
		num_accesses = 0;
		num_reads = 0;
		num_writes = 0;

		num_misses = 0;
		num_hits = 0;

		num_read_misses = 0;
		num_read_hits = 0;
		num_write_misses = 0;
		num_write_hits = 0;

		average_latency = 0;
		average_read_latency = 0;
		average_write_latency = 0;
		average_queue_latency = 0;
	}

	void Logger::update()
	{
		this->step();
	}

	void Logger::access_start(uint64_t addr)
	{
		access_queue.push_back(pair <uint64_t, uint64_t>(addr, currentClockCycle));
	}

	void Logger::access_process(uint64_t addr, bool read_op)
	{
		// Get entry off of the access_queue.
		uint64_t start_cycle = 0;
		bool found = false;
		list<pair <uint64_t, uint64_t>>::iterator it;
		for (it = access_queue.begin(); it != access_queue.end(); it++)
		{
			uint64_t cur_addr = (*it).first;
			uint64_t cur_cycle = (*it).second;

			if (cur_addr == addr)
			{
				start_cycle = cur_cycle;
				found = true;
				access_queue.erase(it);
				break;
			}
		}

		if (!found)
		{
			cerr << "ERROR: Logger.access_process() called with address not in the access_queue. address=0x" << hex << addr << "\n" << dec;
			abort();
		}

		if (access_map.count(addr) != 0)
		{
			cerr << "ERROR: Logger.access_process() called with address already in access_map. address=0x" << hex << addr << "\n" << dec;
			abort();
		}

		AccessMapEntry a;
		a.start = start_cycle;
		a.read_op = read_op;
		a.process = this->currentClockCycle;
		access_map[addr] = a;

		if (read_op)
			this->read();
		else
			this->write();
	}

	void Logger::access_stop(uint64_t addr)
	{
		if (access_map.count(addr) == 0)
		{
			cerr << "ERROR: Logger.access_stop() called with address not in access_map. address=" << hex << addr << "\n" << dec;
			abort();
		}

		AccessMapEntry a = access_map[addr];
		a.stop = this->currentClockCycle;
		access_map[addr] = a;

		if (a.read_op)
			this->read_latency(a.stop - a.start);
		else
			this->write_latency(a.stop - a.start);
		
		access_map.erase(addr);
	}

	void Logger::access_cache(uint64_t addr, bool hit)
	{
		if (access_map.count(addr) == 0)
		{
			cerr << "ERROR: Logger.access_cache() called with address not in access_map. address=" << hex << addr << "\n" << dec;
			abort();
		}

		AccessMapEntry a = access_map[addr];
		a.hit = hit;
		access_map[addr] = a;

		// Log cache event type.
		if (hit && a.read_op)
			this->read_hit();
		else if (hit && !a.read_op)
			this->write_hit();
		else if (!hit && a.read_op)
			this->read_miss();
		else if (!hit && !a.read_op)
			this->write_miss();
	}

	void Logger::read()
	{
		num_accesses += 1;
		num_reads += 1;
	}

	void Logger::write()
	{
		num_accesses += 1;
		num_writes += 1;
	}


	void Logger::hit()
	{
		num_hits += 1;
	}

	void Logger::miss()
	{
		num_misses += 1;
	}

	void Logger::read_hit()
	{
		hit();
		num_read_hits += 1;
	}

	void Logger::read_miss()
	{
		miss();
		num_read_misses += 1;
	}

	void Logger::write_hit()
	{
		hit();
		num_write_hits += 1;
	}

	void Logger::write_miss()
	{
		miss();
		num_write_misses += 1;
	}


	void Logger::read_latency(uint64_t cycles)
	{
		// Need to calculate a running average of latency.
	}

	void Logger::write_latency(uint64_t cycles)
	{
		// Need to calculate a running average of latency.
	}

	void Logger::queue_latency(uint64_t cycles)
	{
		// Need to calculate a running average of latency.
	}

	double Logger::miss_rate()
	{
		return (double)num_misses / num_accesses;
	}

	double Logger::read_miss_rate()
	{
		return (double)num_read_misses / num_reads;
	}

	double Logger::write_miss_rate()
	{
		return (double)num_write_misses / num_writes;
	}

	void Logger::print()
	{
			ofstream savefile;
			savefile.open("hybridsim.log", ios_base::out | ios_base::trunc);

			savefile << "num_accesses " << num_accesses << "\n";
			savefile << "num_reads " << num_reads << "\n";
			savefile << "num_writes " << num_writes << "\n";
			savefile << "num_misses " << num_misses << "\n";
			savefile << "num_hits " << num_hits << "\n";
			savefile << "num_read_misses " << num_read_misses << "\n";
			savefile << "num_read_hits " << num_read_hits << "\n";
			savefile << "num_write_misses " << num_write_misses << "\n";
			savefile << "num_write_hits " << num_write_hits << "\n";
			savefile << "miss_rate " << miss_rate() << "\n";
			savefile << "read_miss_rate " << read_miss_rate() << "\n";
			savefile << "write_miss_rate " << write_miss_rate() << "\n";

			savefile.close();
	}

}
