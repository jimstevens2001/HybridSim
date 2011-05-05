#ifndef HYBRIDSYSTEM_INIREADER_H
#define HYBRIDSYSTEM_INIREADER_H

#include <string>
#include <iostream>
#include <fstream>
#include <list>
#include <sstream>

#include "util.h"

using namespace std;

namespace HybridSim
{
	class IniReader
	{
		public:
		void read(string inifile);
	};
}

#endif
