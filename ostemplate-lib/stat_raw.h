#ifndef STAT_RAW_H
#define STAT_RAW_H

#include <algorithm>
#include <vector>
#include <string>
#include <fstream>
#include <string.h>
#include <classes.h>
#include <map>
#include <mgr/mgrdb_struct.h>
#include <mgr/mgrstr.h>
#include <mgr/mgrfile.h>
#include <sstream>
#include "stat_utils.h"
using namespace std;

#define MGR_NAME_SIZE 15

#define STAT_DIR_RAW "var/stat/raw"
#define STAT_DIR_MON "var/stat/mon"
#define STAT_KEYPREFIX "@"
#define TIMEBLOCK_DAY (24*60*60)
#define TIMEBLOCK_HOUR (60*60)
#define TIMEBLOCK_5MIN (60*5)

void HeaderDebug(const char * func, int line, const string &msg);
#define DEBUG(x) HeaderDebug(__FILE__, __LINE__, (x))

using StatUtils::DB;

namespace Stat
{

class Field;

enum FieldType { ftKey, ftData };
enum DataFoldType { dftSum, dftMax, dftMin, dftNop };
typedef vector<Field *> FieldCollection;

/**
 * @brief The CountersTable class Таблица счетчиков (нужно для нормализации счетчиков)
 */
class CountersTable : public mgr_db::IdTable
{
public:
		CountersTable ();
		mgr_db::StringField Name;
		mgr_db::StringField CounterName;
		mgr_db::LongField CounterValue;
};

/**
 * @brief The Recordset class Класс для хранения данных
 */
class Recordset
{
public:
	void Register (Field * field);
	/**
	 * @brief Fields Возвращает массив с полями
	 * @return
	 */
	FieldCollection Fields()  ;
	/**
	 * @brief Fields Возвращает массив с полями определенного типа
	 * @param type Тип возвращаемого поля (ftData или ftKey)
	 * @return
	 */
	FieldCollection Fields(FieldType type) ;
	/**
	 * @brief FieldByName Поиск поля по его имени
	 * @param name Имя поля
	 * @return
	 */
	Field * FieldByName(const string &name) const;
	/**
	 * @brief Name Возвращает имя поля
	 * @return
	 */
	virtual string Name() const = 0;
	/**
	 * @brief Fold Сворачивает статистику
	 * @param other другой рекордсет, с которым нужно свернуться
	 */
	void Fold (const Recordset & other);
	/**
	 * @brief SetKeys Выставляет значения ключам
	 * @param keys Сами ключи
	 */
	void SetKeys (const StringVector & keys);
	/**
	 * @brief FromQuery Берет данные из запроса.
	 * @param queryPtr
	 */
	void FromQuery (mgr_db::QueryPtr queryPtr);
	/**
	 * @brief GetKeys Возвращает имена ключей
	 * @return
	 */
	StringVector GetKeys();
	/**
	 * @brief GetKeysValues Возвращает значения ключей
	 * @return
	 */
	StringVector GetKeysValues();
	StringVector GetKeyNamesAndValues();

	/**
	 * @brief GetDataFields Возвращает имена полей с данными
	 * @return
	 */
	StringVector GetDataFields();
	/**
	 * @brief Normalize Нормализует все поля. Поле, которое содержит измерение какого-либо параметра,
	 * оформленное в виде счетчика, требует нормализацию (то есть вычисление дельты между прошлым
	 * измерением и текушим), очистку от остатков прошлых измерений. Для этого испольщуется таблица в базе данных.
	 */
	void Normalize();
	Recordset();
	virtual ~Recordset() {}
	void FieldDump();

	int GetCount() const;

private:
	void Normalize (Field & field);
	FieldCollection m_Fields;
	int m_Count;
};

/**
 * @brief The Field class Описание полей
 */
class Field
{
public:
	/**
	 * @brief Field Конструкор поля
	 * @param rs Родительский рекордсет
	 * @param name Имя поля
	 */
	Field (Recordset *rs, const string &name);
	string Name () const;
	virtual FieldType Type() const = 0;
	/**
	 * @brief CreateDbField Создает поле в базе данных
	 * @param table Таблица, в которой создается поле
	 * @return
	 */
	virtual mgr_db::Field * CreateDbField(mgr_db::CustomTable * table) = 0;
	virtual string Value() const = 0;
	/**
	 * @brief FoldType тип операции для свертки поля. По-умолчанию - dftNop
	 * @return
	 */
	virtual DataFoldType FoldType () { return dftNop; }
	/**
	 * @brief Fold Свертывает поле
	 */
	virtual void Fold (Field *) { }
	virtual void Set (const string &value) = 0;
	virtual string GetFun();
	virtual ~Field();
private:
	string m_Name;
};


class KeyField : public Field
{
public:
	KeyField (Recordset *rs, const string &name);
	FieldType Type() const;
	void Set(const string &value);
	string Value() const;
	KeyField & operator = (const string & data);
	mgr_db::Field * CreateDbField(mgr_db::CustomTable * table);
private:
	string m_Val;
};



class DataField : public Field
{
public:
	DataField (Recordset *rs, const string &name, bool needNormalize =  false, DataFoldType foldType = dftSum);
	FieldType Type() const;
	mgr_db::Field * CreateDbField(mgr_db::CustomTable * table);
	void Set(const string &value);
	string GetFun();
	void Fold(Field *other);
	string Value() const;
	int64_t Int() const;
	DataField & operator = (int64_t data);
	bool NeedNormalize() const;

private:
	int64_t m_Value;
	bool m_NeedNormalize;
	DataFoldType m_FoldType;

};




template <class RSType>
class DataTable : public mgr_db::CustomTable
{
	int m_Cells;
public:
	mgr_db::LongField Begin;
	mgr_db::BoolField Processed;
	mgr_db::LongField Count;

	typedef RSType RecordsetType;
	DataTable (const string &name, int cells)
		: mgr_db::CustomTable (name)
		, m_Cells (cells)
		, Begin (this, "begin")
		, Processed (this, "processed")
		, Count (this, "count")
	{
		Begin.info().primary = true;
		Begin.info().can_be_null = false;
		InitFields();
	}
	void DeleteOldCells()
	{
		string query = "select distinct(begin) begin from "+name()+" order by begin desc limit 1 offset "+str::Str(m_Cells);
		auto beginQuery = DB()->Query(query);
		if (!beginQuery->Eof()) {
			DB()->Query("delete from " + name() + " where begin < " + beginQuery->AsString("begin"));
		}
		ClearCache();
	}
	void RemoveStat (RSType & recordset)
	{
		string whereClause = str::Join(recordset.GetKeyNamesAndValues(), " and ");
		DB()->Query("delete from " + name() + " where " + whereClause);
		ClearCache();
	}

	void InitFields()
	{
		RSType	 recordset;
		FieldCollection fields = recordset.Fields();
		for (FieldCollection::iterator field = fields.begin(); field != fields.end(); ++ field) {
			shared_ptr<mgr_db::Field> dbField((*field)->CreateDbField(this));
			m_Fields[(*field)->Name()] = dbField;
			if ((*field)->Type() == ftKey) {
				dbField->info().primary = true;
				dbField->info().can_be_null = false;
			}
		}
	}

	void PushData (time_t begin, RSType & recordset)
	{
		if (!DbFind(str::Join(recordset.GetKeyNamesAndValues(), " and ") +  " and begin=" + str::Str(begin) )) {
			New();
		}
		Begin = begin;
		Processed = false;
		Count = recordset.GetCount();
		FieldCollection fields = recordset.Fields();
		for (FieldCollection::iterator field = fields.begin(); field != fields.end(); ++ field) {
			FieldByName((*field)->Name())->Set((*field)->Value());
		}
		Post();
	}

private:
	map<string, shared_ptr<mgr_db::Field>> m_Fields;
};



template <class RSType>
class RawData
{
public:
	void PushData (const mgr_date::DateTime & now, RSType & recordset)
	{
		string fileName = GetStatFilePath(now, recordset);
		if (!mgr_file::Exists(mgr_file::Path(fileName))) {
			mgr_file::MkDir(mgr_file::Path (fileName));
		}
		bool newFile = !mgr_file::Exists(fileName);
		string output;
		bool firstValue = true;
		FieldCollection fields = recordset.Fields();

		if (newFile) {
			output.append("#begin\t");
			for (FieldCollection::iterator field = fields.begin(); field != fields.end(); ++ field) {
				if ((*field)->Type() == ftKey) continue;
				if (!firstValue) output.append("\t");
				output.append((*field)->Name());
				firstValue = false;
			}
			output.append("\n");
		}
		firstValue = true;
		output.append(str::Str((time_t) now));
		for (FieldCollection::iterator field = fields.begin(); field != fields.end(); ++ field) {
			if ((*field)->Type() == ftKey) continue;
			output.append("\t" + (*field)->Value());
		}
		output.append("\n");
		mgr_file::Lock outpFileLock;

		outpFileLock.DoLock(fileName);
		mgr_file::Append(fileName, output);
		outpFileLock.Release();
	}

//	string GetLine(istream & input, string & rest)
//	{
//		streambuf * inpStreamBuf = input.rdbuf();
//		while (rest.find('\n') == string::npos) {
//			streamsize avail = inpStreamBuf->in_avail();
//			char buf[64];
//			bzero (buf, sizeof (buf));

//			streamsize readen = inpStreamBuf->sgetn(buf, sizeof (buf));
//			rest.append(buf, buf + readen);
//			if (!avail) break;
//		}
//		return str::GetWord(rest, "\n");
//	}

	bool GetData(string & fileContent, time_t & date, RSType & recordset)
	{
		bool empty = fileContent.empty();
		if (empty)
			return false;
		string line = str::Trim(str::GetWord(fileContent, "\n"));
		while (line.find('#') == 0 && !fileContent.empty()) {
			line = str::Trim(str::GetWord(fileContent, "\n"));
		}
		date = str::Int64(str::GetWord(line));
		FieldCollection fields = recordset.Fields();
		ForEachI (fields, field) {
			//skip keys
			if ((*field)->Type() == ftKey) continue;
			string val = str::GetWord(line);
			(*field)->Set(val);
		}
		return !empty;
	}

	virtual string GetStatDir()
	{
		return mgr_file::ConcatPath(STAT_DIR_RAW, RSType().Name());
	}
	string AddKeys (string dir, RSType & recordset)
	{
		FieldCollection fields = recordset.Fields();
		ForEachI (fields, field) {
			if ((*field)->Type() == Stat::ftKey && (!(*field)->Value().empty())) {
				dir = mgr_file::ConcatPath(dir, STAT_KEYPREFIX + (*field)->Value());
			}
		}
		return dir;
	}
	string GetObjPath (RSType & recordset)
	{
		string path = GetStatDir();
		path = AddKeys(path, recordset);
		return path;
	}
	/**
	 * @brief RemoveStat Удаляет статистику по заданным ключам
	 * @param keys - один или более ключей
	 */
	void RemoveStat (RSType & recordset)
	{
		mgr_file::Remove(GetObjPath(recordset));
	}

	virtual string GetFileName (const mgr_date::DateTime & now, RSType & recordset)
	{
		return mgr_date::Format(now, "%Y%m%d%H.stat");
	}
	string GetStatFilePath (const mgr_date::DateTime & now, RSType & recordset)
	{
		string path = GetStatDir();
		path = AddKeys(path, recordset);
		return mgr_file::ConcatPath(path, GetFileName(now, recordset));

	}
};

template <class RSType>
class MonthHistory : public RawData<RSType>
{
public:

	virtual string GetStatDir()
	{
		return mgr_file::ConcatPath(STAT_DIR_MON, RSType().Name());
	}

	string GetFileName (const mgr_date::DateTime & now, RSType & recordset)
	{
		return mgr_date::Format(now, "%Y%m.stat");
	}
};

template <class RSType>
void MinutelyProcess (time_t now, RSType & record) {
	Stat::RawData<RSType> raw;
	Stat::MonthHistory<RSType> monHist;
	time_t begin = StatUtils::block_left(now, TIMEBLOCK_5MIN);
	raw.PushData(begin, record);
	monHist.PushData(begin, record);
}

template <class StatDataTable>
void HourlyProcess (time_t now)
{
	// read all files and get data...
	time_t begin = StatUtils::block_left(now, TIMEBLOCK_HOUR);
	HeaderDebug(__FILE__, __LINE__, str::Format("cell: [%ld, %ld]", begin, StatUtils::block_right(now, TIMEBLOCK_HOUR)));
	StringVector fileList;
	if (!mgr_file::Exists(STAT_DIR_RAW)) {
		mgr_file::MkDir(STAT_DIR_RAW);
	}
	if (!fts::GetList(STAT_DIR_RAW, fileList)) return;
	// TODO: перенести следующую строку в RawData
	string currentFileName = mgr_date::Format(begin, "%Y%m%d%H.stat");
	auto dbTable = DB()->Get<StatDataTable>();
	ForEachI (fileList, file) {
		typename StatDataTable::RecordsetType current;
		typename StatDataTable::RecordsetType sum;
		Stat::RawData<typename StatDataTable::RecordsetType> rawData;
		string name = mgr_file::Name(*file);
		// Обработка только тех файлов, которые сформированы НЕ в этом часе, а в прошлых
		if (name == currentFileName)
			continue;
		string fullPath = *file;
		// var/stat/raw/RSNAME/@key1/@key2/2013072909.stat
		str::GetWord(fullPath, STAT_DIR_RAW);
		StringVector elems = StatUtils::SplitPath(fullPath);
		if (elems[0] != current.Name()) continue;
		StringVector keys = StatUtils::SplitKeys(fullPath);
		if (keys.empty())
			continue;
		DEBUG(str::Join(keys, ","));
		sum.SetKeys (keys);
		sum.FieldDump();
		string fileContent = mgr_file::Read(*file);
		HeaderDebug(__FILE__, __LINE__, "read file " + *file);
		time_t recordDate; //
		while (rawData.GetData(fileContent, recordDate, current)) {
			sum.Fold (current);
		}
		dbTable->PushData (StatUtils::block_left(recordDate, TIMEBLOCK_HOUR), sum);
		mgr_file::RemoveFile(*file);
	}
	dbTable->DeleteOldCells();

}

string to_fun (Field * field);

template <class DayTable, class HourTable>
void DailyProcess (time_t begin)
{
	DEBUG("Daily process");
	typename DayTable::RecordsetType recordset;
	StringVector keys = recordset.GetKeys();
	FieldCollection dataFields = recordset.Fields (ftData);

	StringVector funFields;
	// get previous day
	time_t left = StatUtils::block_left(begin - TIMEBLOCK_DAY, TIMEBLOCK_DAY);
	time_t right = StatUtils::block_right(begin - TIMEBLOCK_DAY, TIMEBLOCK_DAY);
	string timeInterval = "begin between " + str::Str(left) + " and " + str::Str(right);

	auto hrTable = DB()->Get<HourTable>();
	if (hrTable->DbFind ("processed='off' and " + timeInterval)) {
		string query = "select begin, " + str::Join(keys, ", ") + ", ";
		transform (dataFields.begin(), dataFields.end(), back_inserter (funFields), to_fun);
		query += str::Join (funFields, ", ");
		query += " from " + hrTable->name();
		query += " where processed='off' and " + timeInterval;
		query += " group by " + str::Join(keys, ", ");
		auto dayTable = DB()->Get <DayTable>();
		ForEachQuery (DB(), query, hrRow) {
			typename DayTable::RecordsetType dayResult;
			dayResult.FromQuery (hrRow);
			dayTable->PushData (left, dayResult);
		}
		DB()->Query("update " + hrTable->name() + " set processed='on' where processed='off' and " + timeInterval);
		hrTable->ClearCache();
	}
	DEBUG("end of daily process");
}

}

namespace counter
{
class Value
{
public:
	Value();
	Value(const Stat::DataField & field);
	Value(int64_t val);
	long long Val() const;
	void operator = (const Stat::DataField & field);
	void operator = (int64_t val);
	void Normalize (Value newValue);

private:
	long long m_Val;
};

typedef map <string, Value> Object;
typedef map <string, Object> ObjectMap;

class Cache
{
public:

	void Load ();
	bool HasCounter (const string & objName, const string & counterName);

	Value GetCounter (const string & objName, const string & counterName);
	void UpdateCounter (const string & objName, const string & counterName, Value val);
	void Flush();
	void Normalize (const string & key, Stat::DataField & dataField);

private:
	ObjectMap m_Data;
	bool m_Init;
	mgr_thread::RwLock m_Lock;

};

}
#endif // STAT_RAW_H
