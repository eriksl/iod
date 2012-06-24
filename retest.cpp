#include <stdio.h>

#include <string>
using std::string;

#include <boost/regex.hpp>
using boost::regex;
using boost::smatch;

bool match(string pattern, string str)
{
	regex e(pattern);
	smatch s;

	printf("\npattern: \"%s\", string: \"%s\"\n", pattern.c_str(), str.c_str());

	if(regex_match(str, s, e))
	{
		printf("match found\n");
		printf("sub-expressions: %d\n", s.size());

		smatch::iterator i;

		for(i = s.begin(); i != s.end(); i++)
			printf("    \"%s\"\n", string(*i).c_str());
	}
	else
		printf("no match found\n");
}

int main(int, char *[])
{
	match("([0-9A-F]{2}) ([0-9A-F]{2}) ([0-9A-F]{2}) ([0-9A-F]{2})", "mismatch");
	match("([0-9A-F]{2}) ([0-9A-F]{2}) ([0-9A-F]{2}) ([0-9A-F]{2})", "FF");
	match("([0-9A-F]{2}) ([0-9A-F]{2}) ([0-9A-F]{2}) ([0-9A-F]{2})", "FF FF");
	match("([0-9A-F]{2}) ([0-9A-F]{2}) ([0-9A-F]{2}) ([0-9A-F]{2})", "FF FF FF");
	match("([0-9A-F]{2}) ([0-9A-F]{2}) ([0-9A-F]{2}) ([0-9A-F]{2})", "FF FF FF FF");
	match("([0-9A-F]{2}) ([0-9A-F]{2}) ([0-9A-F]{2}) ([0-9A-F]{2})", "FF FF FF FF ");
	match("([0-9A-F]{2}) ([0-9A-F]{2}) ([0-9A-F]{2}) ([0-9A-F]{2})", " 12 34 AB CD");
	match("([0-9A-F]{2}) ([0-9A-F]{2}) ([0-9A-F]{2}) ([0-9A-F]{2})", "12 34 AB CD");

	return(0);
}
