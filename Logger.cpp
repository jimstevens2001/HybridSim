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
		sum_read_latency = 0;
		sum_write_latency = 0;
		sum_queue_latency = 0;
		sum_miss_latency = 0;
		sum_hit_latency = 0;

		sum_read_hit_latency = 0;
		sum_read_miss_latency = 0;

		sum_write_hit_latency = 0;
		sum_write_miss_latency = 0;

		if (DEBUG_LOGGER) 
			debug.open("debug.log", ios_base::out | ios_base::trunc);
	}

	Logger::~Logger()
	{
		if (DEBUG_LOGGER) 
			debug.close();
	}

	void Logger::update()
	{
		this->step();
	}

	void Logger::access_start(uint64_t addr)
	{
		access_queue.push_back(pair <uint64_t, uint64_t>(addr, currentClockCycle));

		if (DEBUG_LOGGER)
		{
			list<pair <uint64_t, uint64_t>>::iterator it = access_queue.begin();
			debug << "access_start( " << addr << " , " << currentClockCycle << " ) / aq: ( " << (*it).first << " , " << (*it).second << " )\n\n";
		}
	}

	void Logger::access_process(uint64_t addr, bool read_op)
	{
		if (DEBUG_LOGGER)
			debug << "access_process( " << addr << " , " << read_op << " )\n";

		// Get entry off of the access_queue.
		uint64_t start_cycle = 0;
		bool found = false;
		list<pair <uint64_t, uint64_t>>::iterator it;
		uint64_t counter = 0;
		for (it = access_queue.begin(); it != access_queue.end(); it++, counter++)
		{
			uint64_t cur_addr = (*it).first;
			uint64_t cur_cycle = (*it).second;

			if (DEBUG_LOGGER)
				debug << counter << " cur_addr = " << cur_addr << ", cur_cycle = " << cur_cycle << "\n";

			if (cur_addr == addr)
			{
				start_cycle = cur_cycle;
				found = true;
				access_queue.erase(it);

				if (DEBUG_LOGGER)
					debug << "found match!\n";

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

		uint64_t time_in_queue = a.process - a.start;
		this->queue_latency(time_in_queue);

		if (DEBUG_LOGGER)
			debug << "finished access_process. time_in_queue = " << time_in_queue << "\n\n";
	}

	void Logger::access_stop(uint64_t addr)
	{
		if (DEBUG_LOGGER)
			debug << "access_stop( " << addr << " )\n";

		if (access_map.count(addr) == 0)
		{
			cerr << "ERROR: Logger.access_stop() called with address not in access_map. address=" << hex << addr << "\n" << dec;
			abort();
		}

		AccessMapEntry a = access_map[addr];
		a.stop = this->currentClockCycle;
		access_map[addr] = a;

		uint64_t latency = a.stop - a.start;

		if (a.read_op && a.hit)
			this->read_hit_latency(latency);
		else if (a.read_op && !a.hit)
			this->read_miss_latency(latency);
		else if (!a.read_op && a.hit)
			this->write_hit_latency(latency);
		else if (!a.read_op && !a.hit)
			this->write_miss_latency(latency);

		
		access_map.erase(addr);

		if (DEBUG_LOGGER)
			debug << "finished access_stop. latency = " << latency << "\n\n";
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
		//average_latency = compute_running_average(average_latency, num_accesses, cycles);
		sum_latency += cycles;
	}

	void Logger::read_latency(uint64_t cycles)
	{
		this->latency(cycles);
		//average_read_latency = compute_running_average(average_read_latency, num_reads, cycles);
		sum_read_latency += cycles;
	}

	void Logger::write_latency(uint64_t cycles)
	{
		this->latency(cycles);
		sum_write_latency += cycles;
	}

	void Logger::queue_latency(uint64_t cycles)
	{
		sum_queue_latency += cycles;
	}

	void Logger::hit_latency(uint64_t cycles)
	{
		sum_hit_latency += cycles;
	}

	void Logger::miss_latency(uint64_t cycles)
	{
		sum_miss_latency += cycles;
	}

	void Logger::read_hit_latency(uint64_t cycles)
	{
		this->read_latency(cycles);
		this->hit_latency(cycles);
		sum_read_hit_latency += cycles;
	}

	void Logger::read_miss_latency(uint64_t cycles)
	{
		this->read_latency(cycles);
		this->miss_latency(cycles);
		sum_read_miss_latency += cycles;
	}

	void Logger::write_hit_latency(uint64_t cycles)
	{
		this->write_latency(cycles);
		this->hit_latency(cycles);
		sum_write_hit_latency += cycles;
	}

	void Logger::write_miss_latency(uint64_t cycles)
	{
		this->write_latency(cycles);
		this->miss_latency(cycles);
		sum_write_miss_latency += cycles;
	}

	double Logger::divide(uint64_t a, uint64_t b)
	{
		// This is a division routine to prevent floating point exceptions if the denominator is 0.
		if (b == 0)
			return 0.0;
		else
			return (double)a / (double)b;
	}

	double Logger::miss_rate()
	{
		return this->divide(num_misses, num_accesses);
	}

	double Logger::read_miss_rate()
	{
		return this->divide(num_read_misses, num_reads);
	}

	double Logger::write_miss_rate()
	{
		return this->divide(num_write_misses, num_writes);
	}

	double Logger::compute_throughput(uint64_t cycles, uint64_t accesses)
	{
		// Calculate the throughput in kilobytes per second.
		return ((this->divide(accesses, cycles) * CYCLES_PER_SECOND) * BURST_SIZE) / 1024.0;
	}


	void Logger::print()
	{
		ofstream savefile;
		savefile.open("hybridsim.log", ios_base::out | ios_base::trunc);

		savefile << "total accesses: " << num_accesses << "\n";
		savefile << "misses: " << num_misses << "\n";
		savefile << "hits: " << num_hits << "\n";
		savefile << "miss rate: " << miss_rate() << "\n";
		savefile << "average latency: " << this->divide(sum_latency, num_accesses) << " cycles\n";
		savefile << "average queue latency: " << this->divide(sum_queue_latency, num_accesses) << " cycles\n";
		savefile << "average miss latency: " << this->divide(sum_miss_latency, num_misses) << " cycles\n";
		savefile << "average hit latency: " << this->divide(sum_hit_latency, num_hits) << " cycles\n";
		savefile << "throughput: " << this->compute_throughput(this->currentClockCycle, num_accesses) << " KB/s\n";
		savefile << "\n";

		savefile << "reads: " << num_reads << "\n";
		savefile << "misses: " << num_read_misses << "\n";
		savefile << "hits: " << num_read_hits << "\n";
		savefile << "miss rate: " << read_miss_rate() << "\n";
		savefile << "average latency: " << this->divide(sum_read_latency, num_reads) << " cycles\n";
		savefile << "average miss latency: " << this->divide(sum_read_miss_latency, num_read_misses) << " cycles\n";
		savefile << "average hit latency: " << this->divide(sum_read_hit_latency, num_read_hits) << " cycles\n";
		savefile << "throughput: " << this->compute_throughput(this->currentClockCycle, num_reads) << " KB/s\n";
		savefile << "\n";

		savefile << "writes: " << num_writes << "\n";
		savefile << "misses: " << num_write_misses << "\n";
		savefile << "hits: " << num_write_hits << "\n";
		savefile << "miss rate: " << write_miss_rate() << "\n";
		savefile << "average latency: " << this->divide(sum_write_latency, num_writes) << " cycles\n";
		savefile << "average miss latency: " << this->divide(sum_write_miss_latency, num_write_misses) << " cycles\n";
		savefile << "average hit latency: " << this->divide(sum_write_hit_latency, num_write_hits) << " cycles\n";
		savefile << "throughput: " << this->compute_throughput(this->currentClockCycle, num_writes) << " KB/s\n";
		savefile << "\n";

		savefile.close();
	}

}
