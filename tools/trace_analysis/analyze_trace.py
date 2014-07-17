import sys

import matplotlib
matplotlib.use('Agg')
import numpy as np
import matplotlib.pyplot as plt

QUANTUM_LENGTH = 2666667
PAGE_SIZE = 4096
TOTAL_PAGES = 8388608
PREFETCH = 5
QUANTUMS = 25

OUTPUT_ACCESSES = 1000000

class TraceAnalysis(object):
	def __init__(self, filename):
		self.filename = filename

		self.cycle = 0
		self.write_flag = 0
		self.address = 0

		self.last_cycle = 0

		self.max_cycle = 0
		self.total_accesses = 0
		self.total_writes = 0
		self.total_reads = 0

		self.quantum_count = -1
		self.quantum_cycles = QUANTUM_LENGTH

		self.cur_quantum_accesses = 0
		self.quantum_accesses_history = []
		self.cur_quantum_writes = 0
		self.quantum_writes_history = []
		self.cur_quantum_reads = 0
		self.quantum_reads_history = []

		self.all_pages = {}
		self.cur_quantum_pages = {}
		self.quantum_page_history = []

		self.cur_quantum_unique_pages = {}
		self.unique_quantum_page_history = []

		self.last_quantum_pages = {}
		self.cur_quantum_last_unique_pages = {}
		self.last_unique_quantum_page_history = []

		self.all_pages_prefetch = {}
		self.cur_quantum_prefetch = {}
		self.last_quantum_prefetch = {}
		self.unique_last_prefetch = {}
		self.unique_all_prefetch = {}
		self.unique_last_prefetch_history = []
		self.unique_all_prefetch_history = []


	def read_trace(self):
		self.inFile = open(self.filename, 'r')
		for line in self.inFile:
			components = line.strip().split()
			if components[0].startswith('#'):
				continue

			self.last_cycle = self.cycle
			self.cycle = int(components[0])
			self.write_flag = int(components[1]) == 1
			self.address = int(components[2])


			if self.cycle >= self.max_cycle:
				self.max_cycle = self.cycle
			else:
				print 'Warning: cycles appear out of order! Cannot analyze this trace.'
				sys.exit(1)

			# Compute the time since the last cycle.
			cycles_passed = self.cycle - self.last_cycle
			self.quantum_cycles += cycles_passed

			while self.quantum_cycles >= QUANTUM_LENGTH:
				self.new_quantum()


			self.total_accesses += 1
			if (self.total_accesses % OUTPUT_ACCESSES) == 0:
				print >> sys.stderr, 'Finished loading %d accesses (cycle: %d, quantum_count: %d)'%(self.total_accesses, self.cycle, self.quantum_count)
			self.cur_quantum_accesses += 1
			if self.write_flag:
				self.total_writes += 1 
				self.cur_quantum_writes += 1
			else:
				self.total_reads += 1 
				self.cur_quantum_reads += 1

			# Compute page address by aligning (using integer division)
			# Mod with memory size to wrap around.
			self.page_address = ((self.address / PAGE_SIZE) * PAGE_SIZE) % (TOTAL_PAGES * PAGE_SIZE)

			if self.page_address not in self.all_pages_prefetch:
				self.unique_all_prefetch[self.page_address] = 1
			if self.page_address not in self.last_quantum_prefetch:
				self.unique_last_prefetch[self.page_address] = 1

			if self.page_address not in self.all_pages:
				self.all_pages[self.page_address] = 0
				if self.page_address not in self.cur_quantum_unique_pages:
					self.cur_quantum_unique_pages[self.page_address] = 1
				for i in range(PREFETCH):
					prefetch_page = self.page_address + i * PAGE_SIZE
					if prefetch_page < TOTAL_PAGES * PAGE_SIZE:
						if prefetch_page not in self.all_pages_prefetch:
							self.all_pages_prefetch[prefetch_page] = 1
			self.all_pages[self.page_address] += 1

			if self.page_address not in self.cur_quantum_pages:
				self.cur_quantum_pages[self.page_address] = 0
				for i in range(PREFETCH):
					prefetch_page = self.page_address + i * PAGE_SIZE
					if prefetch_page < TOTAL_PAGES * PAGE_SIZE:
						if prefetch_page not in self.cur_quantum_prefetch:
							self.cur_quantum_prefetch[prefetch_page] = 1
			self.cur_quantum_pages[self.page_address] += 1

			if self.page_address not in self.last_quantum_pages:
				if self.page_address not in self.cur_quantum_last_unique_pages:
					self.cur_quantum_last_unique_pages[self.page_address] = 0
				self.cur_quantum_last_unique_pages[self.page_address] += 1



		# One last quantum to finalize everything.
		self.new_quantum()

	def new_quantum(self):
			self.quantum_cycles -= QUANTUM_LENGTH
			self.quantum_count += 1

			if self.quantum_count == 0:
				return

			self.quantum_accesses_history.append(self.cur_quantum_accesses)
			self.cur_quantum_accesses = 0
			self.quantum_writes_history.append(self.cur_quantum_writes)
			self.cur_quantum_writes = 0
			self.quantum_reads_history.append(self.cur_quantum_reads)
			self.cur_quantum_reads = 0

			self.quantum_page_history.append(self.cur_quantum_pages)
			self.last_quantum_pages = self.cur_quantum_pages
			self.cur_quantum_pages = {}
			self.unique_quantum_page_history.append(self.cur_quantum_unique_pages)
			self.cur_quantum_unique_pages = {}
			self.last_unique_quantum_page_history.append(self.cur_quantum_last_unique_pages)
			self.cur_quantum_last_unique_pages = {}

			self.last_quantum_prefetch = self.cur_quantum_prefetch
			self.cur_quantum_prefetch = {}
			self.unique_last_prefetch_history.append(self.unique_last_prefetch)
			self.unique_last_prefetch = {}
			self.unique_all_prefetch_history.append(self.unique_all_prefetch)
			self.unique_all_prefetch = {}

	def generate_plot(self, output_plot):
		relevant_fields = ['P','LUP', 'UP', 'C']
		colors = ['b','g','r', 'y']
		labels = ['Pages', 'Unique last', 'Unique all', 'Cumulative']

		x = range(QUANTUMS)

		data = {}
		data['P'] = [len(i) for i in self.quantum_page_history[:QUANTUMS]]
		data['UP'] = [len(i) for i in self.unique_quantum_page_history[:QUANTUMS]]
		data['LUP']= [len(i) for i in self.last_unique_quantum_page_history[:QUANTUMS]]

		data['C'] = []
		count = 0
		for i in data['UP']:
			count += int(i)
			data['C'].append(str(count))



		for i in range(len(relevant_fields)):
			plt.subplot(len(relevant_fields), 1, i+1)
			cur_field = relevant_fields[i]
			cur_color = colors[i]
			cur_label = labels[i]
			plt.plot(x, data[cur_field][:QUANTUMS], cur_color)
			plt.xlabel('Quantum')
			plt.ylabel(cur_label)
			plt.locator_params(nbins=4)

		plt.tight_layout()

		plt.savefig(output_plot)

	def generate_prefetch_plot(self, output_prefetch_plot):
		relevant_fields = ['P','PL', 'PA']
		colors = ['b','g','r']
		labels = ['Pages', 'Prefetch Last', 'Prefetch All']

		x = range(QUANTUMS)

		data = {}
		data['P'] = [len(i) for i in self.quantum_page_history[:QUANTUMS]]
		data['PL'] = [len(i) for i in self.unique_last_prefetch_history[:QUANTUMS]]
		data['PA']= [len(i) for i in self.unique_all_prefetch_history[:QUANTUMS]]


		for i in range(len(relevant_fields)):
			plt.subplot(len(relevant_fields), 1, i+1)
			cur_field = relevant_fields[i]
			cur_color = colors[i]
			cur_label = labels[i]
			plt.plot(x, data[cur_field][:QUANTUMS], cur_color)
			plt.xlabel('Quantum')
			plt.ylabel(cur_label)
			plt.locator_params(nbins=4)

		plt.tight_layout()

		plt.savefig(output_prefetch_plot)
		

	def output_results(self):
		print 'max_cycle: %d'%(self.max_cycle)
		print 'quantum_count: %d'%(self.quantum_count)
		print 'total_accesses: %d'%(self.total_accesses)
		print 'total_writes: %d'%(self.total_writes)
		print 'total_reads: %d'%(self.total_reads)
		print 'total_pages: %d'%(len(self.all_pages))
		first_25_unique_pages = 0
		for i in range(25):
			first_25_unique_pages += len(self.unique_quantum_page_history[i])
		print 'total_pages (25 quanta): %d'%(first_25_unique_pages)
		print
		print 'Quantum Accesses History'
		print 'Q\tA\tW\tR\tP\tUP\tLUP'
		for i in range(len(self.quantum_accesses_history)):
			quantum = i
			accesses = self.quantum_accesses_history[i]
			writes = self.quantum_writes_history[i]
			reads = self.quantum_reads_history[i]
			pages_accessed = len(self.quantum_page_history[i])
			unique_pages = len(self.unique_quantum_page_history[i])
			last_unique_pages = len(self.last_unique_quantum_page_history[i])
			print '%d\t%d\t%d\t%d\t%d\t%d\t%d'%(quantum, accesses, writes, reads, pages_accessed, unique_pages, last_unique_pages)
		print
		print 'All pages accesses'
		print 'page\tcnt'
		for page_address in sorted(self.all_pages.keys()):
			print '%d\t%d'%(page_address, self.all_pages[page_address])
			

def main():
	filename = sys.argv[1]
	ta = TraceAnalysis(filename)
	ta.read_trace()

	if len(sys.argv) >= 4:
		# If a third argument is given, then output the prefetch plots.
		output_plot = sys.argv[3]
		ta.generate_prefetch_plot(output_plot)

	if len(sys.argv) >= 3:
		# If a plot name is given, output a picture.
		output_plot = sys.argv[2]
		ta.generate_plot(output_plot)

	if len(sys.argv) == 2:
		# If not outputing pictures, then just output the text analysis.
		ta.output_results()
	if len(sys.argv) < 2:
		print 'Give at least one parameter!'

if __name__=='__main__':
	main()


