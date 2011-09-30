#include "util.h"

void convert_uint64_t(uint64_t &var, string value, string infostring)
{
	// Check that each character in value is a digit.
	for(size_t i = 0; i < value.size(); i++)
	{
		if(!isdigit(value[i]))
		{
			cout << "ERROR: Non-digit character found: " << infostring << " : " << value << "\n";
			abort();
		}
	}

	// Convert it
	stringstream ss;
	ss << value;
	ss >> var;
}

string strip(string input, string chars)
{
	size_t pos;

	// Strip front.
	pos = input.find_first_not_of(chars);
	input.erase(0, pos);

	// Strip back.
	pos = input.find_last_not_of(chars);
	input.erase(pos+1);

	return input;
}


list<string> split(string input, string chars, size_t maxsplit)
{
	list<string> ret;

	string cur = input;

	size_t pos = 0;

	if (maxsplit == 0)
		maxsplit = 1;

	while (!cur.empty())
	{
		if (ret.size() == (maxsplit-1))
		{
			ret.push_back(cur);
			return ret;
		}

		pos = cur.find_first_of(chars);
		string tmp = cur.substr(0, pos);
		ret.push_back(tmp);


		if (pos == string::npos)
			cur.erase();
		else
		{
			// Skip ahead to the next non-split char
			size_t new_pos = cur.find_first_not_of(chars, pos);
			//cur.erase(0, pos+1);
			cur.erase(0, new_pos);
		}
	}

	// If not at npos, then there is still an extra empty string at the end.
	if (pos != string::npos)
	{
		ret.push_back("");
	}

	return ret;
}

void confirm_directory_exists(string path)
{
	string command_str = "test -e "+path+" || mkdir "+path;
	const char * command = command_str.c_str();
	int sys_done = system(command);
	if (sys_done != 0)
	{
		cout << "system command to confirm directory "+path+" exists has failed.";
		abort();
	}
}
