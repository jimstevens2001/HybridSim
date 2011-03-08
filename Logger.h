#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <fstream>

#include "config.h"

namespace HybridSim
{

	class Logger
	{
		public:
		Logger();

		void read();
		void write();

		void hit();
		void miss();

		void read_latency(uint64_t cycles);
		void write_latency(uint64_t cycles);

		double miss_rate();
		void print();

		uint64_t num_accesses;
		uint64_t num_reads;
		uint64_t num_writes;

		uint64_t average_latency;
		uint64_t average_read_latency;
		uint64_t average_write_latency;

		uint64_t throughput;

		uint64_t num_misses;
		uint64_t num_hits;
	};

}

#endif

