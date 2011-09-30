#ifndef HYBRIDSYSTEM_UTIL_H
#define HYBRIDSYSTEM_UTIL_H

#include <string>
#include <iostream>
#include <fstream>
#include <list>
#include <sstream>

using namespace std;

// Utility Library for HybridSim

void convert_uint64_t(uint64_t &var, string value, string infostring = "");
string strip(string input, string chars = " \t\f\v\n\r");
list<string> split(string input, string chars = " \t\f\v\n\r", size_t maxsplit=string::npos);

void confirm_directory_exists(string path);

#endif
