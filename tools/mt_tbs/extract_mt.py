import sys
import yaml

lines = [i.strip() for i in open(sys.argv[1]).readlines()]

output = {}
output['threads'] = {} # Key: thread id, Value: thread data
output['order'] = [] # Holds thread id that executed for each timestep

# Each thread is a dictionary
# working_set - size of working set
# quantums - data per quantum (list)
# totals - total data (dictionary)


# Extract the working set sizes
for i,v in enumerate(lines):
	map_str = ' has a memory map of size '
	if v.find(map_str) != -1:
		splitline = v.split(map_str)
		thread = int(splitline[0].split('Thread ')[1])
		working_set = int(splitline[1])

		output['threads'][thread] = {'working_set':working_set, 'quantums':[], 'totals':{}}

		print thread, working_set

# Extract data for each quantum
quantum_text = {}
quantum_num = -1
for i,v in enumerate(lines):
	quantum_str = 'Starting quantum '
	if v.find(quantum_str) != -1:
		quantum_num = int(v.split(quantum_str)[1].split()[0])
		print quantum_num, i
		quantum_text[quantum_num] = []

	if quantum_num >= 0:
		quantum_text[quantum_num].append(v)


for i in range(quantum_num+1):
	output['order'].append([])
	cur_text = quantum_text[i]
	active = False
	start_phrase = 'Data for threads that just ran'
	stop_phrase = 'done_threads'
	thread_phrase = 'thread_id '
	cur_thread = -1
	cur_thread_dict = None
	for j,v in enumerate(cur_text):
		if active:
			if v == '':
				continue
			elif v.startswith(stop_phrase):
				active = False
			elif v.startswith(thread_phrase):
				cur_thread = int(v.split()[1])
				output['threads'][cur_thread]['quantums'].append({})
				cur_thread_dict = output['threads'][cur_thread]['quantums'][-1]
				output['order'][-1].append(cur_thread)
			elif cur_thread != -1:
				field = v.split()[0]
				data = v.split()[-1]
				if data.find('.') != -1:
					data = float(data)
				else:
					data = int(data)
				cur_thread_dict[field] = data
		if v.find(start_phrase) != -1:
			active = True
		
		
start_phrase = 'Completed transactions'
stop_phrase = 'TLB Misses'
thread_phrase = 'thread '
active = False
for i,v in enumerate(lines):
	if active:
		if v == '':
			continue
		elif v.startswith(stop_phrase):
			active = False
		elif v.startswith(thread_phrase):
			cur_thread = int(v.split()[1])
			cur_thread_dict = output['threads'][cur_thread]['totals']
		elif cur_thread != -1:
			field = v.split()[0]
			data = v.split()[-1]
			if field == 'tracefile':
				pass
			elif data.find('.') != -1:
				data = float(data)
			else:
				data = int(data)
			cur_thread_dict[field] = data
	if v.find(start_phrase) != -1:
		active = True

outputfile = open('out.yaml','w')
yaml.dump(output, outputfile)
