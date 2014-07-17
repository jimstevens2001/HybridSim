import random

traces = {
	'canneal' : 37310,
	'facesim' : 27583,
	'fluidanimate' : 25815,
	'ferret' : 9631,
	'dedup' : 3045,
	'streamcluster' : 2896,
	'vips' : 2281,
	'freqmine' : 1506,
	'raytrace' : 1426,
	'bodytrack' : 1310,
	'blackscholes' : 970,
	'swaptions' : 607
}

CACHE_SIZE = 32768
CHOICES = 20
REPEATS = 2

goals = [2, 4, 8, 16]

tolerance = 0.1



lower_goals = [i - i*tolerance for i in goals]
upper_goals = [i + i*tolerance for i in goals]

workloads = []
working_sets = []

for i in range(len(goals)):
	for r in range(REPEATS):
		lower = lower_goals[i] * CACHE_SIZE
		upper = upper_goals[i] * CACHE_SIZE

		print goals[i], lower, upper

		try_count = 0
		while(1):
			#if try_count % 100 == 0:
			#	print 'Try %d'%(try_count)
			cur_choices = []
			for j in range(CHOICES):
				cur_choices.append(random.choice(traces.keys()))

			working_set = 0
			for t in cur_choices:
				working_set += traces[t]

			if working_set >= lower and working_set <= upper:
				print 'Found valid combo on try %d with working set %d'%(try_count, working_set)
				print cur_choices
				print
				workloads.append(cur_choices)
				working_sets.append(working_set)
				break
			try_count += 1
		
outFile = open('workloads.py', 'w')
outFile.write('workloads = ' + str(workloads) + '\n\n')
outFile.write('working_sets = ' + str(working_sets) + '\n\n')
