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

		sum_latency = 0;

		average_latency = 0;
		average_read_latency = 0;
		average_write_latency = 0;
		average_queue_latency = 0;

		average_miss_latency = 0;
		average_hit_latency = 0;

		debug.open("debug.log", ios_base::out | ios_base::trunc);
	}

	Logger::~Logger()
	{
		debug.close();
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

		this->queue_latency(a.process - a.start);
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

		uint64_t latency = a.stop - a.start;

		if (a.read_op)
			this->read_latency(latency);
		else
			this->write_latency(latency);

		if (a.hit)
			this->hit_latency(latency);
		else
			this->miss_latency(latency);
			
		
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


	double Logger::compute_running_average(double old_average, double num_values, double new_value)
	{
		return ((old_average * (num_values - 1)) + (new_value)) / num_values;
	}

	void Logger::latency(uint64_t cycles)
	{
		average_latency = compute_running_average(average_latency, num_accesses, cycles);
		sum_latency += cycles;
	}

	void Logger::read_latency(uint64_t cycles)
	{
		this->latency(cycles);
		average_read_latency = compute_running_average(average_read_latency, num_reads, cycles);
	}

	void Logger::write_latency(uint64_t cycles)
	{
		this->latency(cycles);
		average_write_latency = compute_running_average(average_write_latency, num_writes, cycles);
	}

	void Logger::queue_latency(uint64_t cycles)
	{
		average_queue_latency = compute_running_average(average_queue_latency, num_accesses, cycles);
	}

	void Logger::hit_latency(uint64_t cycles)
	{
		average_hit_latency = compute_running_average(average_hit_latency, num_hits, cycles);
	}

	void Logger::miss_latency(uint64_t cycles)
	{
		average_miss_latency = compute_running_average(average_miss_latency, num_misses, cycles);
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
			savefile << "sum_latency " << sum_latency << "\n";
			savefile << "average_latency " << average_latency << "\n";
			savefile << "average_read_latency " << average_read_latency << "\n";
			savefile << "average_write_latency " << average_write_latency << "\n";
			savefile << "average_queue_latency " << average_queue_latency << "\n";
			savefile << "average_hit_latency " << average_hit_latency << "\n";
			savefile << "average_miss_latency " << average_miss_latency << "\n";

			savefile.close();
	}

}
