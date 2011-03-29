#ifndef HYBRIDSYSTEM_INIREADER_H
#define HYBRIDSYSTEM_INIREADER_H

#include <string>
#include <iostream>
#include <fstream>
#include <list>
#include <sstream>

using namespace std;

namespace HybridSim
{
	class IniReader
	{
		public:
		void read(string inifile);
		void convert_uint64_t(uint64_t &var, string key, string value);
		string strip(string input, string chars = " \t\f\v\n\r");
		list<string> split(string input, string chars = " \t\f\v\n\r", size_t maxsplit=string::npos);
	};
}

#endif
