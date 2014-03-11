import sys

import hybridsim


class ThreadTrace(object):
	def __init__(self, trace_id, tracefile, parent):
		self.trace_id = trace_id
		self.tracefile = tracefile
		self.input_file = open(tracefile,'r')
		self.parent = parent

		self.complete = 0
		self.pending = 0
		self.trace_cycles = 0

		self.trace_done = False

		self.trans_cycle = 0
		self.trans_write = False
		self.trans_addr = 0

	def update(self):
		# Called each time a clock cycle runs with this trace active.
		# This is NOT called when the trace is being stalled.
		self.trace_cycles += 1

		if self.trace_cycles >= self.trans_cycle:
			self.parent.addTransaction(self.trace_id, self.trans_write, self.trans_addr)
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

			return

		# If we get to here, then there are no more transactions.
		self.trace_done = True

	
	def transaction_complete(self, isWrite, sysID, addr, cycle):
		self.pending -= 1
		self.complete += 1

	def close(self):
		inFile.close()


class MultiThreadedTBS(object):
	def __init__(self, config_file):
		self.MAX_PENDING=36
		self.MIN_PENDING=36
		self.complete = 0
		self.pending = 0

		self.trace_cycles = 0
		self.throttle_count = 0
		self.throttle_cycles = 0
		self.final_cycles = 0

		self.last_clock = 0
		self.CLOCK_DELAY = 1000000

		self.config_file = config_file

		self.cores = 1
		self.quantum_cycles = 2666667
		self.trace_files = ['traces/big_0.txt', 'traces/big_512.txt']
		self.schedule = {0: [0, 1]}

		self.cur_quantum = 0

		self.cur_cycles = 0

		self.cur_thread = {}
		for i in range(self.cores):
			self.cur_thread[i] = self.schedule[i][0]

		self.next_transaction = {}
		for i in range(self.cores):
			self.next_transaction[i] = None

		self.threads = {}
		for i in range(len(self.trace_files)):
			self.threads[i] = ThreadTrace(i, self.trace_files[i], self)

		self.pending_transactions = {}

		self.mem = hybridsim.HybridSim(1, '')

	def addTransaction(self, trace_id, isWrite, addr):
		self.mem.addTransaction(isWrite, addr)
		self.pending += 1

		# TODO: Figure out what happens if two traces request the same address at the same time
		# Idea: make the pending_transactions value a list of threads.
		if addr in self.pending_transactions:
			print 'Error for thread %d: address %d already in pending transactions for thread %d!'%(trace_id, addr, self.pending_transactions[addr])
			sys.exit(1)

		self.pending_transaction[addr] = trace_id

		

	def transaction_complete(self, isWrite, sysID, addr, cycle):
		sysID = sysID.value
		addr = addr.value
		cycle = cycle.value

		self.complete += 1
		self.pending -= 1

		if (self.complete % 10000 == 0) or (cycle - self.last_clock > self.CLOCK_DELAY):
			print 'Complete=',self.complete,'\t\tpending=',self.pending,'\t\tcycle_count=',cycle,'\t\tthrottle_count=',self.throttle_count
			self.last_clock = cycle

		# TODO: Call the appropriate ThreadTrace object to tell it the transactcion is done.
		if addr not in self.pending_transactions:
			print 'Error for thread %d: address %d not in pending transactions during transaction_complete() callback!'%(trace_id, addr)
			sys.exit(1)

		trace_id = self.pending_transactions[addr]
		del self.pending_transactions[addr]

		self.threads[trace_id].transaction_complete(isWrite, sysID, addr, cycle)

	def run(self):

		done = False
		while not done:

			# Update the threads that are active.

			mem.update()

			done = True
			
		


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
	tracefile = 'traces/test.txt'
	if len(sys.argv) > 1:
		tracefile = sys.argv[1]
		print 'Using trace file',tracefile
	else:
		print 'Using default trace file (traces/test.txt)'

	hs_tbs = HybridSimTBS()
	hs_tbs.run_trace(tracefile)


if __name__ == '__main__':
	main()



