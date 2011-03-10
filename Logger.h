#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <fstream>

#include "config.h"

//test

namespace HybridSim
{

	class Logger: public SimulatorObject
	{
		public:
		Logger();
		~Logger();

		// State
		uint64_t num_accesses;
		uint64_t num_reads;
		uint64_t num_writes;

		uint64_t num_misses;
		uint64_t num_hits;

		uint64_t num_read_misses;
		uint64_t num_read_hits;
		uint64_t num_write_misses;
		uint64_t num_write_hits;
		
		uint64_t sum_latency;
		uint64_t sum_read_latency;
		uint64_t sum_write_latency;
		uint64_t sum_queue_latency;

		uint64_t sum_hit_latency;
		uint64_t sum_miss_latency;

		uint64_t average_latency;
		uint64_t average_read_latency;
		uint64_t average_write_latency;
		uint64_t average_queue_latency;

		uint64_t average_hit_latency;
		uint64_t average_miss_latency;


		class AccessMapEntry
		{
			public:
			uint64_t start; // Starting cycle of access
			uint64_t process; // Cycle when processing starts
			uint64_t stop; // Stopping cycle of access
			bool read_op; // Is this a read_op?
			bool hit; // Is this a hit?
			AccessMapEntry()
			{
				start = 0;
				process = 0;
				stop = 0;
				read_op = false;
				hit = false;
			}
		};

		// Store access info while the access is being processed.
		unordered_map<uint64_t, AccessMapEntry> access_map;

		// Store the address and arrival time while access is waiting to be processed.
		// Must do this because duplicate addresses may arrive close together.
		list<pair <uint64_t, uint64_t>> access_queue;

		// Methods
		void update();

		// External logging methods.
		void access_start(uint64_t addr);
		void access_process(uint64_t addr, bool read_op);
		void access_stop(uint64_t addr);

		void access_cache(uint64_t addr, bool hit);

		// Internal helper methods.
		void read();
		void write();

		void hit();
		void miss();

		void read_hit();
		void read_miss();
		void write_hit();
		void write_miss();

		double compute_running_average(double old_average, double num_values, double new_value);
		void latency(uint64_t cycles);
		void read_latency(uint64_t cycles);
		void write_latency(uint64_t cycles);
		void queue_latency(uint64_t cycles);

		void hit_latency(uint64_t cycles);
		void miss_latency(uint64_t cycles);

		double miss_rate();
		double read_miss_rate();
		double write_miss_rate();
		void print();

		ofstream debug;
	};

}

#endif

