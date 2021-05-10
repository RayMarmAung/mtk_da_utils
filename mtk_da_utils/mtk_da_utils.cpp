// mtk_da_utils.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>

using namespace std;

struct header 
{
	unsigned char title[32];
	unsigned char version[64];
	uint32_t unknown;
	uint32_t magic;
	uint32_t count;
};
struct geometry 
{
	uint32_t offset;
	uint32_t length;
	uint32_t load_addr;
	uint32_t unknown0;
	uint32_t unknown1;
};
struct entry 
{
	uint16_t magic;
	uint16_t chid_id;
	uint16_t chid_ver;
	uint16_t fw_ver;
	uint16_t extra_ver;
	uint16_t unknown0;
	uint16_t unknown1;
	uint16_t unknown2;
	uint16_t unknown3;
	uint16_t part;	
	geometry geometries[10];
};

bool findString(const string &a, const string &b)
{
	if (a.size() != b.size())
		return false;

	for (string::const_iterator it = a.begin(), jt = b.begin(); it != a.end(); ++it, ++jt)
	{
		if (tolower(static_cast<unsigned char>(*it)) != tolower(static_cast<unsigned char>(*jt)))
			return false;
	}
	return true;
}
vector<string> split(string str, char d) 
{
	vector<string> res;
	string w = "";
	for (char s : str) 
	{
		if (s == d)
		{
			res.push_back(w);
			w = "";
		}
		else
			w = w + s;
	}
	res.push_back(w);
	return res;
}

void help(const char* app) 
{
	vector<string> s = split(app, '\\');
	const char *path = s.at(s.size() - 1).c_str();

	fprintf(stdout, "- Usage: %s l -i <da_file>\n\tList available download agents.\n", path);
	fprintf(stdout, "- Usage: %s e -i <da_file> -o <outdir>\n\tSplit all chipsets in download agent.\n", path);
	fprintf(stdout, "- Usage: %s s -i <da_file> -o <out_dir> -n <name>\n\tSplit desire chipset in download agent. (name eg. MT6739_8A00_CA00)\n", path);
}

void list(const char *input, header *h = nullptr, vector<entry> *e = nullptr) 
{
	ifstream in(input, ios::binary);

	header hdr = {};
	in.read((char*)&hdr, sizeof(header));
	if (hdr.magic != 0x22668899)
	{
		fprintf(stdout, "\nInvalid Download agent.\n");
		return;
	}

	if (h)
		*h = hdr;

	if (e)
		e->clear();

	if (!h || !e)
		fprintf(stdout, "Available Chips\n\n");

	for (int i = 0; i < hdr.count; i++) 
	{
		entry ee = {};
		in.read((char*)&ee, sizeof(entry));
		if (ee.magic != 0xdada)
			continue;

		if (!h || !e)
			fprintf(stdout, "MT%04X_%04X_%04X\n", ee.chid_id, ee.chid_ver, ee.fw_ver);

		if (e)
			e->push_back(ee);
	}
}
void split(const char *input, const char *output, const char *name, header h, vector<entry> entries) 
{
	fprintf(stdout, "\nSplitting %s from main download agent...", name);

	entry targetEntry = { 0 };
	for (const entry e: entries) 
	{
		char check[32];
		sprintf_s(check, sizeof(check), "MT%04X_%04X_%04X", e.chid_id, e.chid_ver, e.fw_ver);

		if (findString(check, name)) 
		{
			memcpy(&targetEntry, &e, sizeof(entry));
			break;
		}
	}

	if (targetEntry.magic != 0xdada) 
	{
		fprintf(stdout, "FAIL (target chipset not found to split)");
		return;
	}

	char path[100] = { 0 };
	snprintf(path, sizeof(path), "%s\\%s.bin", output, name);

	ifstream in(input, ios::binary);
	ofstream out(path, ios::binary);

	///write header
	{
		h.count = 1;
		out.write((char*)&h, sizeof(header));
	}

	///write entry header
	{
		uint32_t offset = 0x376c;
		entry writeEntry = targetEntry;
		for (int i = 0; i < 10; i++)
		{
			if (targetEntry.geometries[i].offset > 0 && targetEntry.geometries[i].length > 0)
			{
				writeEntry.geometries[i].offset = offset;
				offset += targetEntry.geometries[i].length;
			}
		}

		char buff[14080] = { 0 };
		memcpy(buff, &writeEntry, sizeof(entry));
		out.write(buff, sizeof(buff));
	}

	///write data 
	{
		for (int i = 0; i < 10; i++)
		{
			if (targetEntry.geometries[i].offset == 0 || targetEntry.geometries[i].length == 0)
				break;

			char *buff = (char*)calloc(sizeof(char), targetEntry.geometries[i].length);
			in.seekg(targetEntry.geometries[i].offset, in.beg);
			in.read(buff, targetEntry.geometries[i].length);
			out.write(buff, targetEntry.geometries[i].length);
		}
	}

	out.flush();
	out.close();

	fprintf(stdout, "DONE");
}

int main(int argc, char *argv[])
{
	if (argc < 3) 
	{
		help(argv[0]);
		return 0;
	}

	enum Task { UNKNOWN, LIST, SPLIT, ALL } task = UNKNOWN;

	if (strcmp(argv[1], "l") == 0)
		task = LIST;
	else if (strcmp(argv[1], "s") == 0)
		task = SPLIT;
	else if (strcmp(argv[1], "e") == 0)
		task = ALL;

	switch(task) 
	{
	case LIST: 
	{
		const char *input = 0;
		if (strcmp(argv[2], "-i") == 0)
			input = argv[3];
		else
			input = argv[2];

		list(input);
	} break;
	case ALL: 
	{
		const char *input = 0, *output = 0;
		for (int i = 2; i < argc; i += 2)
		{
			if (strcmp(argv[i], "-i") == 0)
				input = argv[i + 1];
			else if (strcmp(argv[i], "-o") == 0)
				output = argv[i + 1];
		}

		if (input == 0 || output == 0)
		{
			help(argv[0]);
			return 0;
		}

		header h = {};
		vector<entry> entries;

		list(input, &h, &entries);
		for (const entry e : entries) 
		{
			char check[32];
			sprintf_s(check, sizeof(check), "MT%04X_%04X_%04X", e.chid_id, e.chid_ver, e.fw_ver);

			split(input, output, check, h, entries);
		}
		fprintf(stdout, "\n");
	} break;
	case SPLIT: 
	{
		const char *input = 0, *output = 0, *name = 0;
		for (int i = 2; i < argc; i+=2) 
		{
			if (strcmp(argv[i], "-i") == 0)
				input = argv[i + 1];
			else if (strcmp(argv[i], "-o") == 0)
				output = argv[i + 1];
			else if (strcmp(argv[i], "-n") == 0)
				name = argv[i + 1];
		}

		if (input == 0 || output == 0 || name == 0) 
		{
			help(argv[0]);
			return 0;
		}

		header h = {};
		vector<entry> entries;
		
		list(input, &h, &entries);
		split(input, output, name, h, entries);
		fprintf(stdout, "\n");
	} break;
	}

	return 0;
}

