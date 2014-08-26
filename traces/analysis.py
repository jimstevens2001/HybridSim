import sys

PAGE_SIZE = 4096
unique_pages = {}

inFile = open(sys.argv[1],'r')
for i in inFile.readlines():
	cycle, rw, addr = [int(j) for j in i.strip().split()]

	page_number = addr / PAGE_SIZE
	
	if page_number not in unique_pages:
		unique_pages[page_number] = 0
	unique_pages[page_number] += 1

for page_number in sorted(unique_pages):
	print page_number, unique_pages[page_number]
