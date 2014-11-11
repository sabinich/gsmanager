#ifndef NFLOW_H
#define NFLOW_H

#include <mgr/mgrnet.h>
#include <classes.h>

void SetNetFlowParams();
void StopNfacctd();


namespace netflow
{

namespace daemon
{
void Start();
void Stop();
void WriteConfig();
void Check();
}


class ObjRecord
{
public:
	string Name;
	mgr_net::Ip Ip;
	string Parent;
	uint64_t Rx;
	uint64_t Tx;
	uint64_t Rp; //Received packets
	uint64_t Tp; //Transmitted packets
	ObjRecord();
	void Add (uint64_t rx, uint64_t tx, uint64_t rp, uint64_t tp);
	void Add (const ObjRecord & other);
	void DumpDbg() const;
};

class FilterRecord
{
public:
	string Object;
	int Direction;
	mgr_net::IpRange IpRange;
	FilterRecord (const string& object, int direction, const string & ipRange);
	void DumpDbg();
};

class DataRecord
{
public:
	mgr_net::Ip SrcIp;
	mgr_net::Ip  DstIp;
	uint64_t Bytes;
	uint64_t Packets;
	DataRecord()
		: Bytes (0)
		, Packets (0)
	{}
	void DumpDbg();
};


class Collector
{
public:

	void FillObjects();
	void FillFilters();
	bool NeedDrop (const ObjRecord & record, const FilterRecord & filter, int direction);
	void ProcessFlowData ();

private:
	/* Заполняем список объектов */
	virtual void OnFillObjects (std::vector<ObjRecord> & objects) = 0;
	/* Обрабатываем результат */
	virtual void OnProcessFile (time_t date, const std::vector<ObjRecord> & objects)=0;
private:
	std::vector<ObjRecord> m_Objects;
	std::vector<FilterRecord> m_Filters;

};
}


#endif // NFLOW_H
