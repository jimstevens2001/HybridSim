#include "Logger.h"

using namespace std;

namespace HybridSim 
{
	Logger::Logger()
	{
		num_accesses = 0;
		num_reads = 0;
		num_writes = 0;

		average_latency = 0;
		average_read_latency = 0;
		average_write_latency = 0;

		throughput = 0;

		num_misses = 0;
		num_hits = 0;
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


	void Logger::read_latency(uint64_t cycles)
	{
		// Need to calculate a running average of latency.
	}

	void Logger::write_latency(uint64_t cycles)
	{
		// Need to calculate a running average of latency.
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

			savefile.close();
	}

}
