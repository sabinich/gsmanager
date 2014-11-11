#ifndef UTILS_H
#define UTILS_H
#include <mgr/mgrdb.h>
#include <mgr/mgrdb_struct.h>
#include <api/action.h>

namespace  gsutils {

//remote query timeout, in seconds
#define MINI_QUERY_TIMEOUT		20

void DbGetValues(std::shared_ptr<mgr_db::CustomTable> table, StringMap& values);

class NodeThread {
public:
	NodeThread(int node_id) : m_node_id(node_id) {}
	virtual void operator ()() = 0;

	int NodeId() {return m_node_id;}
private:
	int m_node_id;
};

template<typename Thread> //Thread - это NodeThread
class NodeThreadPool {
public:
	typedef std::shared_ptr<mgr_thread::Handle> ThreadHandlePtr;

	~NodeThreadPool() {
		ForEachI(m_threads, th) {
			(*th)->join();
		}
	}

	void StartThread(int node_id) {
		m_threads.push_back(ThreadHandlePtr(new mgr_thread::Handle(Thread(node_id))));
	}

private:
	std::vector<ThreadHandlePtr> m_threads;
};

string ResolveHostname(const string & name);
void MakeQuery(string& query, StringMap& values);

class FieldsList {
private :
	std::list<StringPair> m_fields;
public :
	std::list<StringPair> & GetParams()
	{
		return m_fields;
	}
	void SetParam(const string & name, const string & val)
	{
		ForEachI(m_fields, param) {
			if (param->first == name) {
				param->second = val;
				return;
			}
		}
		AddParam(name,val);
	}
	void AddParam(const string & name, const string & val)
	{
		m_fields.push_back(StringPair(name,val));
	}
	
	bool HasParam(const string & name)
	{
		ForEachI(m_fields, param) {
			if (param->first == name) {
				return true;
			}
		}
		return false;
	}
	
	string GetParam(const string & name)
	{
		ForEachI(m_fields, param) {
			if (param->first == name) {
				return param->second;
			}
		}
		return "";
	}
};


class SmartDbListCallBack {
public:
	virtual void Before(isp_api::Session & ses, mgr_db::QueryPtr){};
	virtual void OnRow(isp_api::Session & ses, FieldsList & row){};
	virtual void After(isp_api::Session & ses, mgr_db::QueryPtr){};
	virtual ~SmartDbListCallBack(){};
};

template <class CallBack> void SmartDbList(isp_api::Session &ses, mgr_db::QueryPtr query)
{
	CallBack callback;
	callback.Before(ses, query);
	while (!query->Eof()) {
		FieldsList res;
		for (size_t i = 0; i < query->ColCount(); i++) {
			res.AddParam(query->ColName(i), query->AsString(i));
		}
		callback.OnRow(ses,res);
		ses.NewElem();
		auto results = res.GetParams();
		ForEachI(results, data) {
			ses.AddChildNode(data->first.c_str(),data->second);
		}
		query->Next();
	}
	callback.After(ses, query);
}


}
#endif // UTILS_H
