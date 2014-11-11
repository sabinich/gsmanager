#include <mgr/mgrlog.h>
#include <mgr/mgrstr.h>
#include <mgr/mgrproc.h>
#include <mgr/mgrthread.h>
#include <api/module.h>
#include <api/action.h>

MODULE("stat");

namespace {

using namespace isp_api;

class CollectNodeStat : public Action {
public:
	CollectNodeStat() : Action("nodestat", MinLevel(lvAdmin)) {}

	void Execute(Session &ses) const {
		//load average
		LoadAverage(ses);

		//cpu statistics /proc/stat
		CpuUsage(ses);

		//iface statistics /proc/net/dev
		NetStat(ses);

		//meminfo
		MemInfo(ses);

		//iops
		Iops(ses);

		//ses params after execute:
		//loadavg1, loadavg5, loadavg15,
		//user, nice, system,  idle, iowait
		//rx, tx
		//mem, maxmem
		//iops
		ses.Ok();
	}
private:
	int ExecuteCmd(const string& cmd, string& out) const {
		mgr_thread::SafeLock m_lock;
		mgr_thread::SafeSection safe_section(m_lock);
		StringVector args;
		string tmp = cmd;
		str::Split(tmp, " ", args);
		mgr_proc::Execute exec(args);
		out = exec.Str();
		return exec.Result();
	}

	void LoadAverage(Session &ses) const {
		string resstr;
		string loadavg1, loadavg5, loadavg15;
		int res = ExecuteCmd("cat /proc/loadavg", resstr);
		if (!res && !resstr.empty()) {
			loadavg1 = str::GetWord(resstr);
			loadavg5 = str::GetWord(resstr);
			loadavg15 = str::GetWord(resstr);
		}
		ses.NewNode("loadavg1", loadavg1);
		ses.NewNode("loadavg5", loadavg5);
		ses.NewNode("loadavg15", loadavg15);
	}

	void CpuUsage(Session &ses) const {
		string resstr, line, key;
		string user, nice, system, idle, iowait;
		auto res = ExecuteCmd("cat /proc/stat", resstr);
		if (!res) {
			while (!resstr.empty()) {
				line = str::GetWord(resstr, "\n");
				key = str::GetWord(line);
				if (key != "cpu")
					continue;
				line = str::Trim(line);
				user = str::GetWord(line);
				nice = str::GetWord(line);
				system = str::GetWord(line);
				idle = str::GetWord(line);
				iowait = str::GetWord(line);
				break;
			}
		}
		ses.NewNode("user", user);
		ses.NewNode("nice", nice);
		ses.NewNode("system", system);
		ses.NewNode("idle", idle);
		ses.NewNode("iowait", iowait);
	}

	void NetStat(Session &ses) const {
		string iface, line, key;
		string rx, tx;

		//define default HN iface
		string resstr;
		int res = ExecuteCmd("ip route | grep '^default' | grep -o 'dev [^ ]*' | cut -f2 -d' '", resstr);
		if (!res)
			iface = resstr;
		iface = iface.empty() ? string("eth0") : str::Trim(iface);

		res = ExecuteCmd("cat /proc/net/dev", resstr);
		if (!res && !resstr.empty()) {
			//skip 2 lines - header
			str::GetWord(resstr, "\n");
			str::GetWord(resstr, "\n");
			while (!resstr.empty()) {
				line = str::Trim(str::GetWord(resstr, "\n"));
				key = str::GetWord(line, ":");
				if (key != iface)
					continue;
				line = str::Trim(line);
				rx = str::GetWord(line);
				//skip unused params:
				for(int i = 0; i < 7; ++i) {
					str::GetWord(line);
					line = str::Trim(line);
				}
				tx = str::GetWord(line);
				break;
			}
		}

		ses.NewNode("rx", rx);
		ses.NewNode("tx", tx);
	}

	void MemInfo(Session &ses) const {
		string line, key;
		string mem, max_mem, free_mem, buf_mem, cache_mem;
		string meminfo = mgr_file::ReadE("/proc/meminfo");
		while (!meminfo.empty()) {
			line = str::GetWord(meminfo, "\n");
			key = str::GetWord(line, ':');
			line = str::Trim(line);
			if (key == "MemTotal")
				max_mem = str::GetWord(line);
			else if (key == "MemFree")
				free_mem = str::GetWord(line);
			else if (key == "Buffers")
				buf_mem = str::GetWord(line);
			else if (key == "Cached")
				cache_mem = str::GetWord(line);
		}
		if (!max_mem.empty() && !free_mem.empty()) {
			try {
				mem = str::Str(str::Int(max_mem) - str::Int(free_mem) - str::Int(buf_mem) - str::Int(cache_mem));
			} catch (...) {}
		}
		ses.NewNode("mem", mem);
		ses.NewNode("maxmem", max_mem);
	}

	void Iops(Session& ses) const {
		string diskstats = mgr_file::ReadE("/proc/diskstats");
		int64_t NumReadsIssued, NumWritesIssued, NumSectorsRead, NumSectorsWritten;
		int64_t io = 0;
		int64_t rd_bytes = 0;
		int64_t wr_bytes = 0;
		int64_t rd_req = 0;
		int64_t wr_req = 0;
		int64_t sectorSize = 0;
		string line;
		while (!diskstats.empty()) {
			line = str::Trim(str::GetWord(diskstats, "\n"));
			string maj = str::GetWord(line);
			if (maj != "8") // skip other devices; 8 - for block devices
				continue;
			str::GetWord(line); //minor number of device
			string dname = str::GetWord(line);
			//Get model exclude VIRTUAL-DISK
			string model = str::Trim(mgr_file::ReadE(str::Format("/sys/block/%s/device/model", dname.c_str())));
			if (model == "VIRTUAL-DISK")
				continue;
			string block_file = str::Format("/sys/block/%s/queue/physical_block_size", dname.c_str());
			bool is_logical = !mgr_file::Exists(block_file);
			if (is_logical)
				continue;
			string physical_block_size = mgr_file::ReadE(block_file);
			sectorSize = str::Int64(physical_block_size);

			NumReadsIssued = str::Int64(str::GetWord(line));
			str::GetWord(line);//NumReadsMerged
			NumSectorsRead		= str::Int64(str::GetWord(line));
			str::GetWord(line);//ReadingMS
			NumWritesIssued = str::Int64(str::GetWord(line));
			str::GetWord(line);//NumWritesMerged
			NumSectorsWritten	= str::Int64(str::GetWord(line));

			rd_bytes += NumSectorsRead * sectorSize;
			wr_bytes += NumSectorsWritten * sectorSize;
			rd_req += NumReadsIssued;
			wr_req += NumWritesIssued;
			io += NumReadsIssued + NumWritesIssued;
		}
		ses.NewNode("iops", str::Str(io));
		ses.NewNode("rd_bytes", str::Str(rd_bytes));
		ses.NewNode("wr_bytes", str::Str(wr_bytes));
		ses.NewNode("rd_req", str::Str(rd_req));
		ses.NewNode("wr_req", str::Str(wr_req));
	}
};

MODULE_INIT(portmgr, "gsmini") {
	new CollectNodeStat;
}

} //end of private namespace
