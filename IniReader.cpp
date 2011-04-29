#include "IniReader.h"

// Define the globals read from the ini file here.
// Also provide default values here.

// Other constants
uint64_t CONTROLLER_DELAY = 2;
uint64_t EPOCH_LENGTH = 200000;
uint64_t HISTOGRAM_BIN = 100;
uint64_t HISTOGRAM_MAX = 20000;

// these values are also specified in the ini file of the nvdimm but have a different name
uint64_t PAGE_SIZE = 4096; // in bytes, so divide this by 64 to get the number of DDR3 transfers per page



uint64_t SET_SIZE = 64; // associativity of cache

uint64_t BURST_SIZE = 64; // number of bytes in a single transaction, this means with PAGE_SIZE=1024, 16 transactions are needed
uint64_t FLASH_BURST_SIZE = 4096; // number of bytes in a single flash transaction

// Number of pages total and number of pages in the cache
uint64_t TOTAL_PAGES = 2097152/4; // 2 GB
uint64_t CACHE_PAGES = 1048576/4; // 1 GB


// Defined in marss memoryHierachy.cpp.
// Need to confirm this and make it more flexible later.
uint64_t CYCLES_PER_SECOND = 667000000;

// INI files
string dram_ini = "ini/DDR3_micron_8M_8B_x8_sg15.ini";
string flash_ini = "ini/samsung_K9XXG08UXM(mod).ini";
string sys_ini = "ini/system.ini";


namespace HybridSim 
{
	void IniReader::read(string inifile)
	{
		ifstream inFile;
		char tmp[256];
		string tmp2;
		list<string> lines;

		inFile.open(inifile);
		if (!inFile.is_open())
		{
			cout << "ERROR: Failed to load HybridSim's Ini file: " << inifile << "\n";
			abort();
		}

		while(!inFile.eof())
		{
			inFile.getline(tmp, 256);
			tmp2 = (string)tmp;

			// Filter comments out.
			size_t pos = tmp2.find("#");
			tmp2 = tmp2.substr(0, pos);

			// Strip whitespace from the ends.
			tmp2 = strip(tmp2);

			// Filter newlines out.
			if (tmp2.empty())
				continue;

			// Add it to the lines list.
			lines.push_back(tmp2);
		}
		inFile.close();

		list<string>::iterator it;
		for (it = lines.begin(); it != lines.end(); it++)
		{
			list<string> split_line = split((*it), "=", 2);

			if (split_line.size() != 2)
			{
				cout << "ERROR: Parsing ini failed on line: " << (*it) << "\n";
				cout << "There should be exactly one '=' per line\n";
				abort();
			}

			string key = split_line.front();
			string value = split_line.back();

			// Place the value into the appropriate global.
			if (key.compare("CONTROLLER_DELAY") == 0)
				convert_uint64_t(CONTROLLER_DELAY, value, key);
			else if (key.compare("EPOCH_LENGTH") == 0)
				convert_uint64_t(EPOCH_LENGTH, value, key);
			else if (key.compare("HISTOGRAM_BIN") == 0)
				convert_uint64_t(HISTOGRAM_BIN, value, key);
			else if (key.compare("HISTOGRAM_MAX") == 0)
				convert_uint64_t(HISTOGRAM_MAX, value, key);
			else if (key.compare("PAGE_SIZE") == 0)
				convert_uint64_t(PAGE_SIZE, value, key);
			else if (key.compare("SET_SIZE") == 0)
				convert_uint64_t(SET_SIZE, value, key);
			else if (key.compare("BURST_SIZE") == 0)
				convert_uint64_t(BURST_SIZE, value, key);
			else if (key.compare("FLASH_BURST_SIZE") == 0)
				convert_uint64_t(FLASH_BURST_SIZE, value, key);
			else if (key.compare("TOTAL_PAGES") == 0)
				convert_uint64_t(TOTAL_PAGES, value, key);
			else if (key.compare("CACHE_PAGES") == 0)
				convert_uint64_t(CACHE_PAGES, value, key);
			else if (key.compare("CYCLES_PER_SECOND") == 0)
				convert_uint64_t(CYCLES_PER_SECOND, value, key);
			else if (key.compare("dram_ini") == 0)
				dram_ini = value;
			else if (key.compare("flash_ini") == 0)
				flash_ini = value;
			else if (key.compare("sys_ini") == 0)
				sys_ini = value;
			else
			{
				cout << "ERROR: Illegal key/value pair in HybridSim ini file: " << key << "=" << value << "\n";
				cout << "This could either be due to an illegal key or the incorrect value type for a key\n";
				abort();
			}
		}
	}
}
