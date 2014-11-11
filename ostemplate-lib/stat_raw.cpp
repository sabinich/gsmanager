#include <api/module.h>
#include <mgr/mgrlog.h>
#include "stat_raw.h"

#define COUNTER_FILE_NAME "var/stat.counters"

MODULE ("stat");

namespace Stat
{

string Field::Name() const
{
	return m_Name;
}

Field::Field(Recordset *rs, const string &name)
	: m_Name (name)
{
	rs->Register (this);
}

Field::~Field(){}

void Recordset::Register (Field * field)
{
	m_Fields.push_back(field);
}
FieldCollection Recordset::Fields() {return m_Fields;}
FieldCollection Recordset::Fields(FieldType type)
{
	FieldCollection result;
	ForEachI (m_Fields, field) {
		if ((*field)->Type() == type) {
			result.push_back(*field);
		}
	}
	return result;
}

void Recordset::SetKeys (const StringVector & keys)
{
	StringVector::const_iterator currentKey = keys.begin();
	FieldCollection::iterator field = m_Fields.begin();
	while ((field != m_Fields.end()) && (currentKey != keys.end())) {
		if ((*field)->Type() == ftKey) {
			(*field)->Set (*currentKey);
		}
		++field; ++ currentKey;
	}
}

void Recordset::Fold(const Recordset &other)
{
	m_Count += other.m_Count;
	if (Name() != other.Name()) return;
	ForEachI (m_Fields, field) {
		Field * current = *field;
		string fieldName = current->Name();
		Field * otherField = other.FieldByName(fieldName);
		if (current->Type() != ftKey) {
			current->Fold(otherField);
		}
	}
}

StringVector Recordset::GetKeys()
{
	StringVector result;
	ForEachI (m_Fields, field) {
		if ((*field)->Type() == ftKey) {
			result.push_back((*field)->Name());
		}
	}
	return result;
}
StringVector Recordset::GetKeysValues()
{
	StringVector result;
	ForEachI (m_Fields, field) {
		if ((*field)->Type() == ftKey && (!(*field)->Value().empty())) {
			result.push_back((*field)->Value());
		}
	}
	return result;
}

StringVector Recordset::GetKeyNamesAndValues()
{
	StringVector result;
	ForEachI (m_Fields,field) {
		if ((*field)->Type() == ftKey && (!(*field)->Value().empty())) {
			result.push_back((*field)->Name() + "='" + (*field)->Value() + "'");
		}
	}
	return result;
}

StringVector Recordset::GetDataFields()
{
	StringVector result;
	ForEachI (m_Fields, field) {
		if ((*field)->Type() == ftData) {
			result.push_back((*field)->Name());
		}
	}
	return result;
}

void Recordset::FieldDump()
{
	ForEachI (m_Fields, field) {
		Debug ("%s:'%s'", (*field)->Name().c_str(), (*field)->Value().c_str());
	}
}

int Recordset::GetCount() const
{
	return m_Count;
}

void Recordset::Normalize(Field &field)
{
	if (field.Type() != ftData) return;
	try {
		DataField & dataField = dynamic_cast<DataField&>(field);
		if (!dataField.NeedNormalize())
			return;
		string name = str::Join(GetKeysValues(), ":");
		SINGLETON (counter::Cache).Normalize (name, dataField);
	} catch (...) {
		return;
	}
}

void Recordset::Normalize()
{
	ForEachI (m_Fields, field) {
		Normalize(**field);
	}
}

Recordset::Recordset()
	: m_Count (1)
{

}

mgr_db::Field *KeyField::CreateDbField(mgr_db::CustomTable *table)
{
	return new mgr_db::StringField (table, Name(), 64);
}

void Recordset::FromQuery(mgr_db::QueryPtr queryPtr)
{
	ForEachI (m_Fields, field) {
			(*field)->Set (queryPtr->AsString((*field)->Name()));
	}
}

Field *Recordset::FieldByName(const string & name) const
{
	ForEachI (m_Fields, field) {
		if ((*field)->Name() == name)
			return *field;
	}
	throw mgr_err::Missed ("recordset_field", name);
}


KeyField::KeyField(Recordset *rs, const string &name)
	: Field (rs, name)
{}

FieldType KeyField::Type() const { return ftKey; }

void KeyField::Set(const string &value)
{
	m_Val = value;
}

string KeyField::Value() const
{
	return m_Val;
}

KeyField &KeyField::operator =(const string &data) {
	m_Val = data;
	return *this;
}


DataField::DataField(Recordset *rs, const string &name, bool needNormalize, DataFoldType foldType)
	: Field (rs, name)
	, m_Value (0)
	, m_NeedNormalize(needNormalize)
	, m_FoldType (foldType)
{}

FieldType DataField::Type() const { return ftData; }

mgr_db::Field *DataField::CreateDbField(mgr_db::CustomTable *table)
{
	return new mgr_db::LongField (table, Name());
}



void DataField::Set(const string &value)
{
	m_Value = str::Int64(value);
}

string DataField::GetFun()
{
	switch (m_FoldType) {
	case dftMax:
		return "max(" + Name() + ") " + Name();
	case dftMin:
		return "min(" + Name() + ") " + Name();
	case dftSum:
		return "sum(" + Name() + ") " + Name();
	default:
		return Name();
	}
}

void DataField::Fold(Field *other)
{
	switch (m_FoldType) {
	case dftMax:
		m_Value = max(str::Int64(other->Value()), m_Value);
		break;
	case dftMin:
		m_Value = min(str::Int64(other->Value()), m_Value);
		break;
	case dftSum:
		m_Value += str::Int64(other->Value());
		break;
	default:
		m_Value = str::Int64(other->Value());
		break;
	}
}

string Field::GetFun()
{
	return Name();
}

string DataField::Value() const
{
	return str::Str(m_Value);
}

int64_t DataField::Int() const {return m_Value;}

DataField &DataField::operator =(int64_t data) {
	m_Value = data;
	return *this;
}

bool DataField::NeedNormalize() const { return m_NeedNormalize; }


CountersTable::CountersTable()
	: mgr_db::IdTable 		("counters")
		, Name			(this, "name", 64)
		, CounterName   (this, "cname", 64)
		, CounterValue	(this, "value")
{
}

string to_fun(Field *field)
{
	if (field->Type() == ftData) {
		DataField * data = dynamic_cast <DataField *>(field);
		if (data) {
			return data->GetFun();
		}
	}
	return field->Name();
}

}

void HeaderDebug(const char *func, int line, const string &msg)
{
	Debug ("%s:%d %s", func, line, msg.c_str());
}



void counter::Cache::Load()
{
	if (m_Init) return;
	mgr_thread::LockWrite lock (m_Lock);
	if (m_Init) return;
	m_Data.clear ();
	if (mgr_file::Exists (COUNTER_FILE_NAME)) {
		string counterCacheContent = mgr_file::Read (COUNTER_FILE_NAME);
		while (!counterCacheContent.empty ()) {
			string line = str::Trim (str::GetWord (counterCacheContent, "\n"));
			if (line.find ("#") == 0) continue; // Skip comment or header
			string objName = str::GetWord (line);
			string counterName = str::GetWord (line);
			string strValue = str::GetWord (line);
			m_Data[objName][counterName] = str::Int64 (strValue);
		}
	}
	m_Init = true;
}

bool counter::Cache::HasCounter(const string &objName, const string &counterName)
{
	return m_Data.find (objName) != m_Data.end ()
			&& m_Data[objName].find (counterName) != m_Data[objName].end ();
}

counter::Value counter::Cache::GetCounter(const string &objName, const string &counterName)
{
	mgr_thread::LockRead lock (m_Lock);
	return m_Data.at(objName).at(counterName);
}

void counter::Cache::UpdateCounter(const string &objName, const string &counterName, counter::Value val)
{
//	Debug ("Update counter %s -- %s -- %lld", objName.c_str(), counterName.c_str(), val.Val ());
	mgr_thread::LockWrite lock (m_Lock);
	m_Data[objName][counterName] = val;
}

void counter::Cache::Flush()
{
	if (!m_Init) return;
	mgr_thread::LockRead lock (m_Lock);
	stringstream out;
	out << "# counter cache data" << std::endl;
	out << "#name cname value" << std::endl;
	ForEachI (m_Data, dataItem) {
		string objName = dataItem->first;
		ForEachI (dataItem->second, counter) {
			string counterName = counter->first;
			out << objName << " " << counterName << " " << counter->second.Val() << std::endl;
		}
	}
	mgr_file::Write (COUNTER_FILE_NAME, out.str ());
}

void counter::Cache::Normalize(const string &key, Stat::DataField &dataField) {
	if (!m_Init) Load ();
	try {
		Value currentValue = dataField;
		if (HasCounter (key, dataField.Name ())) {
			counter::Value value = GetCounter (key, dataField.Name());
			value.Normalize (currentValue);
			dataField = value.Val();
		} else {
			dataField = 0;
		}
		UpdateCounter (key, dataField.Name(), currentValue);
	} catch (...) {
		return;
	}
}


counter::Value::Value() : m_Val (0) {}


counter::Value::Value(const Stat::DataField &field) : m_Val (field.Int()) {}


counter::Value::Value(int64_t val) : m_Val (val) {}

long long counter::Value::Val() const
{
	return m_Val;
}

void counter::Value::operator =(int64_t val)
{
	m_Val = val;
}

void counter::Value::Normalize(counter::Value newValue)
{
	m_Val = (newValue.Val() >= m_Val) ? (newValue.Val () - m_Val) : newValue.Val ();
}

void counter::Value::operator =(const Stat::DataField &field)
{
	m_Val = field.Int();
}


MODULE_INIT(stat,"")
{

}
