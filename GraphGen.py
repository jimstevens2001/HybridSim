import matplotlib

def parse_file(logfile):
	section = '================================================================================'

	inFile = open(logfile, 'r')
	lines = inFile.readlines()
	inFile.close()
	lines = [line.strip() for line in lines]

	section_list = []
	for i in range(len(lines)):
		if lines[i] == section:
			section_list.append(i)
		
	sections = {}
	sections['total'] = lines[0:section_list[0]-2]	
	sections['epoch'] = lines[section_list[0]+5:section_list[1]]	
	sections['miss'] = lines[section_list[1]+3:section_list[2]-2]	
	sections['access'] = lines[section_list[2]+3:]

	return sections


def parse_total(total_lines):
	sections = ['total', 'read', 'write']
	total_dict = {}
	for j in sections:
		total_dict[j] = {}
	cur_section = 0

	for i in total_lines:
		if i == '':
			cur_section += 1 
			continue
		tmp = i.split(':')
		key = tmp[0].strip()
		val = tmp[1].strip()
		total_dict[sections[cur_section]][key] = val
		if key.endswith('latency'):
			latency_vals = [v.strip(') ') for v in val.split('(')]
			total_dict[sections[cur_section]][key+' cycles'] = float(latency_vals[0].split(' ')[0])
			total_dict[sections[cur_section]][key+' us'] = float(latency_vals[1].split(' ')[0])
			
			

	return total_dict


def parse_epoch(epoch_lines):
	epoch_section = '---------------------------------------------------'
	
	epoch_lines_list = []
	cur_section = []
	for i in epoch_lines:
		if i == epoch_section:
			epoch_lines_list.append(cur_section)
			cur_section = []
			continue
		cur_section.append(i)

	epoch_list = []
	for i in epoch_lines_list:
		epoch_list.append(parse_total(i[:-2]))

	return epoch_list



def parse_misses(miss_lines):
	miss_list = []
	for i in miss_lines:
		cur_dict = {}
		tmp = i.split(':')
		cur_dict['address'] = int(tmp[0], 16)
		data = tmp[1].split(';')
		for j in data:
			if j == '':
				continue
			key, val = j.split('=')
			val = val.strip()
			if val.startswith('0x'):
				cur_dict[key] = int(val, 16)
			else:
				cur_dict[key] = int(val)
		miss_list.append(cur_dict)
	return miss_list


def parse_accesses(access_lines):
	access_dict = {} 
	for i in access_lines:
		tmp = i.split(':')
		key = int(tmp[0], 16)
		val = int(tmp[1])
		access_dict[key] = val
	return access_dict


sections = parse_file('hybridsim.log')
total_dict = parse_total(sections['total'])
epoch_list = parse_epoch(sections['epoch'])
miss_list = parse_misses(sections['miss'])
access_dict = parse_accesses(sections['access'])

#print epoch_list

print total_dict
