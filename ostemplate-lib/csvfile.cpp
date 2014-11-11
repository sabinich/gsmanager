#include "csvfile.h"
#include <fstream>
#include <string>
#include <vector>
#include <map>

#include <mgr/mgrerr.h>
#include <mgr/mgrstr.h>

CSVFile::CSVFile (const string& filename)
{
		this->m_file.open(filename.c_str(),std::fstream::in | std::fstream::out);
		if (!this->m_file.is_open()) throw mgr_err::Error("file_open",filename);
		string head = GetLine(m_file);
		int pos = 0;
		while(!head.empty()) {
				Headmap[str::GetWord(head,",")] = pos++; //Сохраняю индекси полей
		}

		while (!m_file.eof()) {
				string row = GetLine(m_file);
				if (!row.empty()) {
						string field;
						this->Data.push_back(std::vector<string>());
						while(!row.empty()) {
								field = str::GetWord(row,",");
								this->Data.back().push_back(field);
						}
				}
		}
		this->CurrentRow = 0;
}

string CSVFile::GetLine(std::fstream & file, int bufsize) {
		char buf[bufsize];
		file.getline(buf,bufsize);
		return string(buf);
}


bool CSVFile::Eof()
{
		if (this->CurrentRow < this->Data.size())
				return false;
		return true;
}

bool CSVFile::Next()
{
		this->CurrentRow++;
		if (this->CurrentRow < this->Data.size())
				return true;
		return false;

}

string CSVFile::operator[] (const string& name)
{
		return this->Data[CurrentRow][this->Headmap[name]];
}

CSVFile::~CSVFile ()
{
		this->m_file.close();
}
