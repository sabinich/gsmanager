#ifndef CSVFILE_H
#define CSVFILE_H

#include <fstream>
#include <string>
#include <vector>
#include <map>

using std::string;

class CSVFile {

private:
		std::fstream m_file;
		std::map<string,int> Headmap;
		string GetLine(std::fstream & file, int bufsize = 65536);
public:
		std::vector<std::vector<string> > Data;
		size_t CurrentRow;
		string operator[](const string& name);
		bool Eof();
		bool Next();
		CSVFile(const string& filename);
		~CSVFile();
};

#endif // CSVFILE_H
