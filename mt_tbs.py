import sys
import pprint
import yaml

pp = pprint.PrettyPrinter()

import hybridsim

#########################################################################
# MT_TBS config

# Enable scheuler prefetching algorithm.
ENABLE_PREFETCHING = True

# How many outstanding accesses are allowed from a single thread.
THREAD_PENDING_MAX = 8

# Set this to force a thread to stop after N trace cycles
# 0 means to go until the thread finishes all accesses.
MAX_TRACE_CYCLES_PER_THREAD = 0

# Use a basic virtual to physical address translation
# This should be used for most real traces, since they might stomp on each other.
VIRTUAL_ADDRESS_TRACES = True
PREALLOCATE = True

# How many pages should we give to the process when the process runs out of memory
NUM_PAGES_PER_ALLOC = 512
DEBUG_SCHEDULER_PREFETCHER=False
FAKE_PREFETCHES=True

#########################################################################

#########################################################################
# Config info we will get from HybridSim

# The numbers here are just placeholder values
TOTAL_PAGES = None
PAGE_SIZE = None
ADDRESS_SPACE_SIZE = None
CACHE_PAGES = None
PREFILL_CACHE = None
CACHE_SIZE = None

#TOTAL_PAGES = 8388608
#PAGE_SIZE = 4096
#ADDRESS_SPACE_SIZE = TOTAL_PAGES * PAGE_SIZE

#########################################################################

#########################################################################
# Other constants

# Definitions for page states.
PAGE_ALLOCATED = 0
PAGE_PREFETCHED = 1 # It is in the cache and got there by being prefetched.
PAGE_ACCESSED = 2 # It is in the cached and has been accessed with a read or write.
PAGE_DIRTY = 4 # It has been written since being brought into the cache.
PAGE_PREFETCH_ATTEMPTED = 8 # Always set when a page could have been prefetched, even if it wasn't needed.

#########################################################################

# Contains a dictionary for each unique trace file with a memory mapping.
# Each thread will have to request its own set of physical pages for all virtual pages
# in this master mapping.
preallocated_traces = {}

def get_page_address(address):
	page_offset = address % PAGE_SIZE
	return address - page_offset
	

class SchedulerPrefetcher(object):
	def __init__(self, mt_tbs):
		self.mt_tbs = mt_tbs
		self.thread_pages = {}
		self.old_thread_pages = {}
		self.next_threads = None

		self.halfway_cycles = self.mt_tbs.quantum_cycles / 2

		if DEBUG_SCHEDULER_PREFETCHER:
			self.debug = open('sched_prefetch_debug.log', 'w')

	def set_initial_pages(self, thread_id, page_list):
		for addr in page_list:
			page_num = addr / PAGE_SIZE

			# Save page in thread pages set.
			if thread_id not in self.thread_pages:
				self.thread_pages[thread_id] = {}
			if page_num not in self.thread_pages[thread_id]:
				self.thread_pages[thread_id][page_num] = 0
			self.thread_pages[thread_id][page_num] += 1

		# Save the old thread pages.
		if thread_id not in self.old_thread_pages:
			self.old_thread_pages[thread_id] = []
		self.old_thread_pages[thread_id].append(self.thread_pages[thread_id])
		
		# Reset the thread pages.
		self.thread_pages[thread_id] = {}
		
		

	def done(self):
		outFile = open('scheduler_prefetcher.log', 'w')
		outFile.write(pp.pformat(self.old_thread_pages))
		outFile.close()

	def new_quantum(self, last_threads, next_threads):
		self.next_threads = next_threads

		if self.mt_tbs.quantum_num > 0:
			for thread_id in last_threads:
				# Save the old thread pages.
				if thread_id not in self.old_thread_pages:
					self.old_thread_pages[thread_id] = []
				self.old_thread_pages[thread_id].append(self.thread_pages[thread_id])
				
				# Reset the thread pages.
				self.thread_pages[thread_id] = {}

		if DEBUG_SCHEDULER_PREFETCHER:
			print >> self.debug, 'quantum:',self.mt_tbs.quantum_num
			print >> self.debug, 'last_threads:',last_threads
			print >> self.debug, 'next_threads:',next_threads
			print >> self.debug, self.old_thread_pages
			print >> self.debug
			self.debug.flush()



	def update(self):
		# Issue prefetches when halfway_cycles is reached.
		# TODO: Combine pages into ranges.
		# TODO: Use the access counts for each page to prioritize what pages are sent.
		if (self.mt_tbs.quantum_cycles_left == self.halfway_cycles) and (self.mt_tbs.quantum_num > 0):
			print 'Issuing prefetches for threads',self.next_threads
			prefetch_count = 0
			for thread_id in self.next_threads:
				if thread_id in self.old_thread_pages:
					last_thread_pages = self.old_thread_pages[thread_id][-1]
					page_list = [page_num * PAGE_SIZE for page_num in last_thread_pages.keys()]
					for page in page_list:
						if FAKE_PREFETCHES:
							self.mt_tbs.mem.mmio(4, page)
						else:
							self.mt_tbs.mem.mmio(3, page)
						prefetch_count += 1
						(thread_id, virtual_page_address, valid) = self.mt_tbs.physical_page_map[page]
						prefetched, accessed, dirty, prefetch_attempted = self.mt_tbs.threads[thread_id].get_page_state(virtual_page_address)
						self.mt_tbs.threads[thread_id].set_page_state(virtual_page_address, not accessed, accessed, dirty, True)
			print 'Issued %d prefetches.'%(prefetch_count)

					

	def addTransaction(self, thread_id, isWrite, addr):
		# Note: addr is a PHYSICAL address.
		# That means all sched prefetch state is tracked as physical addresses instead of virtual addresses.

		# Compute page number.
		page_num = addr / PAGE_SIZE

		# Save page in thread pages set.
		if thread_id not in self.thread_pages:
			self.thread_pages[thread_id] = {}
		if page_num not in self.thread_pages[thread_id]:
			self.thread_pages[thread_id][page_num] = 0
		self.thread_pages[thread_id][page_num] += 1


class TraceThread(object):
	def __init__(self, thread_id, tracefile, base_address, parent):
		self.thread_id = thread_id
		self.tracefile = tracefile
		self.input_file = open(self.tracefile,'r')
		self.base_address = base_address
		if VIRTUAL_ADDRESS_TRACES and (self.base_address != 0):
			print >> sys.stderr, 'All base addresses must be 0 when using VIRTUAL_ADDRESS_TRACES mode.'
			sys.exit(1)
		self.parent = parent

		self.complete = 0
		self.pending = 0
		self.trace_cycles = 0 # Cycles in which we made progress in the trace file.
		self.throttle_count = 0 # Number of times we stalled during the trace execution.
		self.throttle_cycles = 0 # Number of cycles stalled during trace execution.
		self.final_cycles = 0 # Cycles passed after trace was done being generated, but while pending transactions were still outstanding.
		self.done_cycles = 0 # Cycles passed after all the trace was done and pending transactions completed.

		self.trace_done = False

		self.trans_cycle = 0
		self.trans_write = False
		self.trans_addr = 0

		self.memory_map = {}
		self.page_state = {}
		self.unallocated_page_addresses = self.parent.new_alloc(self.thread_id)

		# Stats
		self.cur_prefetch_hits = 0
		self.cur_prefetch_cached_hits = 0
		self.cur_non_prefetch_hits = 0
		self.cur_page_misses = 0
		self.cur_evictions = 0
		self.cur_unused_prefetch = 0 
		self.cur_dirty_evictions = 0
		self.cur_clean_evictions = 0

		self.total_prefetch_hits = 0
		self.total_prefetch_cached_hits = 0
		self.total_non_prefetch_hits = 0
		self.total_page_hits = 0
		self.total_page_misses = 0
		self.total_evictions = 0
		self.total_unused_prefetch = 0 
		self.total_dirty_evictions = 0
		self.total_clean_evictions = 0

		if VIRTUAL_ADDRESS_TRACES and PREALLOCATE:
			self.preallocate_memory()

		self.get_next_trans()

	def update_stats(self):
		print 'thread_id %d stats...'%(self.thread_id)
		print 'prefetch_hits',self.cur_prefetch_hits
		print 'prefetch_cached_hits',self.cur_prefetch_cached_hits
		print 'non_prefetch_hits',self.cur_non_prefetch_hits
		print 'page_misses', self.cur_page_misses
		print 'evictions', self.cur_evictions
		print 'unused_prefetch', self.cur_unused_prefetch
		print 'dirty_evictions', self.cur_dirty_evictions
		print 'clean_evictions', self.cur_clean_evictions
		print

		self.total_prefetch_hits += self.cur_prefetch_hits
		self.total_prefetch_cached_hits += self.cur_prefetch_cached_hits
		self.total_non_prefetch_hits += self.cur_non_prefetch_hits
		self.total_page_misses += self.cur_page_misses
		self.total_evictions += self.cur_evictions
		self.total_unused_prefetch += self.cur_unused_prefetch
		self.total_dirty_evictions += self.cur_dirty_evictions
		self.total_clean_evictions += self.cur_clean_evictions

		self.cur_prefetch_hits = 0
		self.cur_prefetch_cached_hits = 0
		self.cur_page_misses = 0
		self.cur_evictions = 0
		self.cur_unused_prefetch = 0 
		self.cur_dirty_evictions = 0
		self.cur_clean_evictions = 0

		

	def preallocate_memory(self):
		if self.tracefile in preallocated_traces:
			# Call translate_virtual_page for each virtual page in the master map
			# for this tracefile. That will allocate new physical pages for each
			# required virtual page.
			master_map = preallocated_traces[self.tracefile]
			for virtual_page in master_map:
				self.translate_virtual_page(virtual_page)
		else:
			while not self.trace_done:
				self.get_next_trans()
			# At this point, the memory map for this thread is completely specified.
			# Save it in the 
			preallocated_traces[self.tracefile] = self.memory_map

			# Simply reinitialize the state that was modified by the above and we are
			# ready to run.
			self.trace_done = False
			self.input_file = open(self.tracefile,'r')


	def update(self):
		if self.trace_done:
			if self.pending > 0:
				self.final_cycles += 1
			else:
				self.done_cycles += 1
			return

		if self.pending >= THREAD_PENDING_MAX:
			self.throttle_cycles += 1
			return

		# Called each time a clock cycle runs with this trace active.
		# This is NOT called when the trace is being stalled.
		self.trace_cycles += 1

		if (MAX_TRACE_CYCLES_PER_THREAD != 0) and (self.trace_cycles >= MAX_TRACE_CYCLES_PER_THREAD):
			self.done() 

		if self.trace_cycles >= self.trans_cycle:
			self.parent.addTransaction(self.thread_id, self.trans_write, self.trans_addr)
			self.pending += 1
			self.get_next_trans()


	def get_next_trans(self):
		# Set default values for the return tuple.

		if self.trace_done:
			return

		line = 'dummy value'
		while line:
			line = self.input_file.readline()
			tmp_line = line.strip().split('#')[0]
			if tmp_line == '':
				continue

			split_line = tmp_line.split()
			if len(split_line) != 3:
				print >> sys.stderr, 'ERROR: Parsing trace failed on line:'
				print >> sys.stderr, line
				print >> sys.stderr, 'There should be exactly three numbers per line'
				print >> sys.stderr, 'There are', len(split_line)
				sys.exit(1)

			line_vals = [int(i) for i in split_line]

			self.trans_cycle = int(split_line[0])
			self.trans_write = bool(int(split_line[1]) % 2)
			self.trans_addr = int(split_line[2])

			# Apply base address transformation.
			self.trans_addr = (self.trans_addr + self.base_address) % ADDRESS_SPACE_SIZE

			if VIRTUAL_ADDRESS_TRACES:
				# Perform virtual to physical translation.
				self.cur_virtual_address = self.trans_addr
				self.trans_addr = self.translate_virtual_page(self.trans_addr)

				# Get the page state.
				page_address = get_page_address(self.cur_virtual_address)
				prefetched, accessed, dirty, prefetch_attempted = self.get_page_state(page_address)

				# Count non-prefetch hits and non-prefetch misses.
				if prefetched:
					self.cur_prefetch_hits += 1 # Count all accessed that hit because of a prefetch
				elif not prefetched and accessed and prefetch_attempted:
					self.cur_prefetch_cached_hits += 1
				elif not prefetch_attempted and accessed:
					self.cur_non_prefetch_hits += 1 # Count all accesses that hit without a prefetch bringing in that page.
				elif not prefetch_attempted and not accessed:
					self.cur_page_misses += 1 # Count all misses. TODO: Deal with PREFILLED_CACHE
					
				# Update page state
				new_accessed = True
				if self.trans_write:
					new_dirty = True
				else:
					new_dirty = dirty

				self.set_page_state(page_address, prefetched, new_accessed, new_dirty, prefetch_attempted)

				
			return

		# If we get to here, then there are no more transactions.
		self.done()

	def get_page_state(self, page_address):
		page_state = self.page_state[page_address]

		prefetched = bool(page_state & PAGE_PREFETCHED)
		accessed = bool(page_state & PAGE_ACCESSED)
		dirty = bool(page_state & PAGE_DIRTY)
		prefetch_attempted = bool(page_state & PAGE_PREFETCH_ATTEMPTED)
		return prefetched, accessed, dirty, prefetch_attempted
		
	def set_page_state(self, page_address, prefetched, accessed, dirty, prefetch_attempted):
		new_page_state = PAGE_ALLOCATED 
		if prefetched:
			new_page_state |= PAGE_PREFETCHED
		if accessed:
			new_page_state |= PAGE_ACCESSED
		if dirty:
			new_page_state |= PAGE_DIRTY
		if prefetch_attempted:
			new_page_state |= PAGE_PREFETCH_ATTEMPTED
		self.page_state[page_address] = new_page_state

	def translate_virtual_page(self, virtual_address):
		page_offset = virtual_address % PAGE_SIZE
		virtual_page_address = virtual_address - page_offset

		if virtual_page_address not in self.memory_map:
			# Create a new memory mapping for this page.
			if len(self.unallocated_page_addresses) == 0:
				# We need to request more pages from the "operating system"
				self.unallocated_page_addresses = self.parent.new_alloc(self.thread_id)
				print 'Thread %d is requesting more memory. Received pages from %d to %d.'%(self.thread_id, 
						self.unallocated_page_addresses[0], self.unallocated_page_addresses[-1])

				# TODO: Put these pages into the pages to prefetch?

			# Add it to the memory map.
			next_page = self.unallocated_page_addresses.pop(0)
			self.memory_map[virtual_page_address] = next_page
			self.page_state[virtual_page_address] = PAGE_ALLOCATED
			self.parent.register_page(self.thread_id, virtual_page_address, next_page)

		physical_page_address = self.memory_map[virtual_page_address]
		physical_address = physical_page_address + page_offset
		return physical_address

	
	def transaction_complete(self, isWrite, sysID, addr, cycle):
		self.pending -= 1
		self.complete += 1

		if self.trace_done and self.pending == 0:
			print 'thread',self.thread_id,'received its last pending transaction.'

	def page_evicted(self, virtual_page_address):
		prefetched, accessed, dirty, prefetch_attempted = self.get_page_state(virtual_page_address)

		self.cur_evictions += 1
		if prefetched and not accessed:
			self.cur_unused_prefetch += 1
		if dirty:
			self.cur_dirty_evictions += 1
		if not dirty:
			self.cur_clean_evictions += 1

		# Reset page state.
		self.set_page_state(virtual_page_address, False, False, False, False)


	def done(self):
		print 'thread',self.thread_id,'is done issuing new transactions.'
		self.trace_done = True
		self.input_file.close()

	def print_summary(self):
		print 'thread',self.thread_id,'summary...'
		print 'tracefile = ',self.tracefile
		print 'completed transactions =',self.complete
		print 'trace_cycles =',self.trace_cycles
		print 'throttle_count =',self.throttle_count
		print 'throttle_cycles =',self.throttle_cycles
		print 'final_cycles =',self.final_cycles
		print 'done_cycles =',self.done_cycles
		print 'total_cycles = ', (self.trace_cycles + self.throttle_cycles + self.final_cycles + self.done_cycles)

		print 'prefetch_hits',self.total_prefetch_hits
		print 'page_hits', self.total_page_hits
		print 'page_misses', self.total_page_misses
		print 'evictions', self.total_evictions
		print 'unused_prefetch', self.total_unused_prefetch
		print 'dirty_evictions', self.total_dirty_evictions
		print 'clean_evictions', self.total_clean_evictions



class MultiThreadedTBS(object):
	def __init__(self, config_file):
		self.complete = 0
		self.pending = 0

		self.cycles = 0

		self.done = False
		self.quantum_cycles_left = 0
		self.quantum_num = -1
		self.cur_running = None
		self.schedule_index = -1

		self.last_clock = 0
		self.CLOCK_DELAY = 1000000

		self.config_file = config_file

		self.next_alloc_address = 0

		# Maps each physical page to a thread_id and virtual address.
		# This is needed to send notify callbacks back to the correct thread.
		self.physical_page_map = {}

		# Set up the memory.
		self.mem = hybridsim.HybridSim(5, '')
		def read_cb(sysID, addr, cycle):
			self.transaction_complete(False, sysID, addr, cycle)
		def write_cb(sysID, addr, cycle):
			self.transaction_complete(True, sysID, addr, cycle)
		self.read_cb = read_cb
		self.write_cb = write_cb
		self.mem.RegisterCallbacks(self.read_cb, self.write_cb);

		# Query config data we need from HybridSim.
		global TOTAL_PAGES
		global PAGE_SIZE
		global ADDRESS_SPACE_SIZE
		global CACHE_PAGES
		global PREFILL_CACHE
		global CACHE_SIZE
		TOTAL_PAGES, PAGE_SIZE = self.mem.query(1, 0, 0)
		ADDRESS_SPACE_SIZE = TOTAL_PAGES * PAGE_SIZE
		CACHE_PAGES, PREFILL_CACHE = self.mem.query(2, 0, 0)
		CACHE_SIZE = CACHE_PAGES * PAGE_SIZE
		print 'TOTAL_PAGES=%d'%(TOTAL_PAGES)
		print 'CACHE_PAGES=%d'%(CACHE_PAGES)
		print 'PAGE_SIZE=%d'%(PAGE_SIZE)
		print 'ADDRESS_SPACE_SIZE=%d'%(ADDRESS_SPACE_SIZE)
		print 'CACHE_SIZE=%d'%(CACHE_SIZE)
		print 'PREFILL_CACHE=%d'%(PREFILL_CACHE)

		# Set up the notify callback.
		self.evict_count = 0
		def notify_cb(operation, addr, cycle):
			self.handle_notify_cb(operation, addr, cycle)
		self.notify_cb = notify_cb
		self.mem.RegisterNotifyCallback(self.notify_cb)
		self.mem.ConfigureNotify(0, True)

		# Load the configuration data.
		try:
			configFile = open(self.config_file)
			config_data = yaml.load(configFile)
			configFile.close()

			self.cores = config_data['cores']
			self.quantum_cycles = config_data['quantum_cycles']
			self.trace_files = config_data['trace_files']
			self.base_addresses = config_data['base_addresses']
			self.schedule = config_data['schedule']
		except Exception as e:
			print 'Failed to parse the config file properly.'
			raise e

		# Verify the integrity of the schedule...
		for i in self.schedule:
			if len(i) != self.cores:
				print 'Schedule entry does not have length that matches core count %s'%(str(i))
				sys.exit(1)
			if len(i) != len(set(i)):
				print 'Schedule entry has a thread scheduled on more than one core: %s'%(str(i))
				sys.exit(1)

		if len(self.trace_files) != len(self.base_addresses):
			print 'Length of trace_files (%d) does not match length of base_addresses (%d)'%(len(self.trace_files), len(self.base_addresses))
			sys.exit(1)

		self.threads = {}
		for thread_id in range(len(self.trace_files)):
			self.threads[thread_id] = TraceThread(thread_id, self.trace_files[thread_id], self.base_addresses[thread_id], self)

		self.pending_transactions = {}

		# Set up the scheduler prefetcher.
		self.scheduler_prefetcher = SchedulerPrefetcher(self)


		# Initialize the prefetch state for all threads.
		for thread_id in self.threads:
			if VIRTUAL_ADDRESS_TRACES and PREALLOCATE:
				# TODO: Figure out the number of pages to use. Using NUM_PAGES_PER_ALLOC for now.
				first_prefetch_pages = [self.threads[thread_id].memory_map[i] for i in sorted(self.threads[thread_id].memory_map.keys())]
				print 'Thread %d has a memory map of size %d'%(thread_id, len(self.threads[thread_id].memory_map))
				#first_prefetch_pages = self.threads[thread_id].memory_map.keys()[0:NUM_PAGES_PER_ALLOC]
			else:
				first_prefetch_pages = self.threads[thread_id].unallocated_page_addresses
			self.scheduler_prefetcher.set_initial_pages(thread_id, first_prefetch_pages)

	def new_alloc(self, thread_id):
		# Used by threads to request more memory
		new_alloc_pages = []

		for i in range(NUM_PAGES_PER_ALLOC):
			new_alloc_pages.append(self.next_alloc_address)
			self.register_page(thread_id, 0, self.next_alloc_address, valid=False)
			self.next_alloc_address += PAGE_SIZE

		if self.next_alloc_address >= (PAGE_SIZE * TOTAL_PAGES):
			print 'ALLOCATOR RAN OUT OF MEMORY!'
			sys.exit(1)
			
		return new_alloc_pages

	def register_page(self, thread_id, virtual_page_address, physical_page_address, valid=True):
		self.physical_page_map[physical_page_address] = (thread_id, virtual_page_address, valid)

	def handle_notify_cb(self, operation, addr, cycle):
		#print 'Notify callback (operation: %d, addr: %d, cycle: %d)'%(operation, addr, cycle)
		if operation == 0:
			self.evict_count += 1

			# Look up the physical page that was evicted.
			if addr not in self.physical_page_map:
				print 'Error: Notify callback received for address not in physical page map.'
				print 'Notify callback (operation: %d, addr: %d, cycle: %d)'%(operation, addr, cycle)
				print self.evict_count
				sys.exit(1)
			(thread_id, virtual_page_address, valid) = self.physical_page_map[addr]

			if not valid:
				return

			# Tell the thread.
			self.threads[thread_id].page_evicted(virtual_page_address)

		else:
			print 'Error: Illegal operation returned by notify callback.'
			print 'Notify callback (operation: %d, addr: %d, cycle: %d)'%(operation, addr, cycle)
			sys.exit(1)

	def addTransaction(self, thread_id, isWrite, addr):
		trans_key = (addr, isWrite)
		if trans_key not in self.pending_transactions:
			self.pending_transactions[trans_key] = []
		self.pending_transactions[trans_key].append(thread_id)

		self.scheduler_prefetcher.addTransaction(thread_id, isWrite, addr)

		self.pending += 1

		# Note: this must go last due to potential issues with MMIO
		# That said, REMAP_MMIO should always be turned off with mt_tbs.
		self.mem.addTransaction(isWrite, addr) 


		#print 'Added (%d,%d,%d)'%(thread_id, isWrite, addr)
		

	def transaction_complete(self, isWrite, sysID, addr, cycle):
#		sysID = sysID.value
#		addr = addr.value
#		cycle = cycle.value

		#print 'Complete (%d,%d,%d, %d)'%(isWrite, sysID, addr, cycle)

		self.complete += 1
		self.pending -= 1

		if (self.complete % 10000 == 0) or (cycle - self.last_clock > self.CLOCK_DELAY):
			print 'Complete=',self.complete,'\t\tpending=',self.pending,'\t\tcycle_count=',cycle,'/',self.cycles,'\t\tQuantum=',self.quantum_num
			self.last_clock = cycle

		# Call the appropriate TraceThread object to tell it the transactcion is done.
		trans_key = (addr, isWrite)
		if trans_key not in self.pending_transactions:
			print 'Error: (address: %d, isWrite: %d) not in pending transactions during transaction_complete() callback!'%trans_key
			sys.exit(1)
		thread_id = self.pending_transactions[trans_key].pop(0)
		if len(self.pending_transactions[trans_key]) == 0:
			del self.pending_transactions[trans_key]

		self.threads[thread_id].transaction_complete(isWrite, sysID, addr, cycle)

		#print 'Complete thread = %d'%(thread_id)

	def clean_schedule(self):
		# This method removes done threads from the schedule if there is something else that can run.

		done_threads = []
		for thread_id in self.threads:
			if self.threads[thread_id].trace_done:
				done_threads.append(thread_id)
		done_threads.sort()

		print 'done_threads =',done_threads

		for quantum in range(len(self.schedule)):
			for i in range(len(self.schedule[quantum])):
				thread_id = self.schedule[quantum][i]
				if thread_id in done_threads:
					for new_thread_id in self.threads:
						if (new_thread_id not in self.schedule[quantum]) and (new_thread_id not in done_threads):
							self.schedule[quantum][i] = new_thread_id
							print 'Replacing thread %d with thread %d in quantum %d'%(thread_id, new_thread_id, quantum)
							break


	def new_quantum(self):
		# Determine if the simulation is done.
		tmp_done = True
		for thread_id in self.threads:
			if not self.threads[thread_id].trace_done:
				tmp_done = False # Run another quantum if any thread still has work to do.
		if tmp_done and self.pending != 0:
			print 'All threads are done, but there are still pending transactions. Running another quantum.'
			tmp_done = False # Run another quantum if there are any pending transactions.
		self.done = tmp_done
		if self.done:
			print 'Simulation is done! Here is a summary of what just happened...'
			print 'Last quantum =',self.quantum_num
			print 'Completed transactions =',self.complete
			print
			for thread_id in self.threads:
				self.threads[thread_id].print_summary()
			self.mem.printLogfile()

			self.scheduler_prefetcher.done()

			return 

		print 'Data for threads that just ran...'
		print
		if self.quantum_num != -1:
			for thread_id in self.cur_running:
				self.threads[thread_id].update_stats()

		self.clean_schedule()

		self.quantum_cycles_left = self.quantum_cycles
		self.quantum_num += 1
		self.schedule_index = self.quantum_num % len(self.schedule)
		last_threads = self.cur_running
		self.cur_running = list(self.schedule[self.schedule_index])

		print '------------------------------------------------------------------------'
		print 'Starting quantum %d at cycle count %d. completed=%d cur_running=%s'%(self.quantum_num, self.cycles, self.complete, str(self.cur_running))
		print 'Quantum evictions = ',self.evict_count
		print
		self.evict_count = 0


		# TODO: Pick another thread to run if any of the current threads are done.

		next_index = (self.schedule_index + 1) % len(self.schedule)
		next_threads = self.schedule[next_index]
		self.scheduler_prefetcher.new_quantum(last_threads, next_threads)


	def run(self):
		print 'Initialization done. Starting MT-TBS run...'
		while not self.done:
			# Handle quantum switches.
			if self.quantum_cycles_left == 0:
				self.new_quantum()
				if self.done:
					return

			# Update all running threads.
			for thread_id in self.cur_running:
				self.threads[thread_id].update()

			# Update the HybridSim instance.
			self.mem.update()

			# Update the scheduler prefetcher.
			if ENABLE_PREFETCHING:
				self.scheduler_prefetcher.update()

			# Update the cycle counters.
			self.cycles += 1
			self.quantum_cycles_left -= 1

			
		


class HybridSimTBS(object):
	def __init__(self):
		self.MAX_PENDING=36
		self.MIN_PENDING=36
		self.complete = 0
		self.pending = 0
		self.throttle_count = 0
		self.throttle_cycles = 0
		self.final_cycles = 0

		self.trace_cycles = 0

		self.last_clock = 0
		self.CLOCK_DELAY = 1000000

	def transaction_complete(self, isWrite, sysID, addr, cycle):
		sysID = sysID.value
		addr = addr.value
		cycle = cycle.value

		self.complete += 1
		self.pending -= 1

		if (self.complete % 10000 == 0) or (cycle - self.last_clock > self.CLOCK_DELAY):
			print 'Complete=',self.complete,'\t\tpending=',self.pending,'\t\tcycle_count=',cycle,'\t\tthrottle_count=',self.throttle_count
			self.last_clock = cycle

	def run_trace(self, tracefile):
		mem = hybridsim.HybridSim(1, '')
		
		def read_cb(sysID, addr, cycle):
			self.transaction_complete(False, sysID, addr, cycle)
		def write_cb(sysID, addr, cycle):
			self.transaction_complete(True, sysID, addr, cycle)

		mem.RegisterCallbacks(read_cb, write_cb);

		inFile = open(tracefile, 'r')

		line = 'dummy value'

		while line:
			line = inFile.readline()
			tmp_line = line.strip().split('#')[0]
			if tmp_line == '':
				continue

			split_line = tmp_line.split()
			if len(split_line) != 3:
				print >> sys.stderr, 'ERROR: Parsing trace failed on line:'
				print >> sys.stderr, line
				print >> sys.stderr, 'There should be exactly three numbers per line'
				print >> sys.stderr, 'There are', len(split_line)
				sys.exit(1)

			line_vals = [int(i) for i in split_line]

			trans_cycle = int(split_line[0])
			write = bool(int(split_line[1]) % 2)
			addr = int(split_line[2])

			while (self.trace_cycles < trans_cycle):
				mem.update()
				self.trace_cycles += 1

			mem.addTransaction(write, addr)
			self.pending += 1

			if self.pending >= self.MAX_PENDING:
				self.throttle_count += 1
				while self.pending > self.MIN_PENDING:
					mem.update()
					self.throttle_cycles += 1


		inFile.close()

		while self.pending > 0:
			mem.update()
			self.final_cycles += 1

		# This is a hack for the moment to ensure that a final write completes.
		# In the future, we need two callbacks to fix this.
		# This is not counted towards the cycle counts for the run though.
		for i in range(1000000):
			mem.update()

		# TODO: finish this up

		print 'trace_cycles =',self.trace_cycles
		print 'throttle_count =',self.throttle_count
		print 'throttle_cycles =',self.throttle_cycles
		print 'final_cycles =',self.final_cycles
		print 'total_cycles = trace_cycles + throttle_cycles + final_cycles =', (self.trace_cycles + self.throttle_cycles + self.final_cycles)

		mem.printLogfile()

def main():
	if len(sys.argv) > 1:
		yaml_file = sys.argv[1]
	else:
		yaml_file = 'ini/scheduler_prefetcher.yaml'
	hs_tbs = MultiThreadedTBS(yaml_file)
	hs_tbs.run()


if __name__ == '__main__':
	main()



