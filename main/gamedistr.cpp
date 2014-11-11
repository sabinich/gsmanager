#include <api/action.h>
#include <api/module.h>
#include <api/stdconfig.h>
#include <mgr/mgrlog.h>
#include <mgr/mgrfile.h>
#include <api/longtask.h>
#include <mgr/mgrxml.h>
#include "defines.h"
#include <algorithm>
#define GAME_INSTALL_QUEUE_NAME "GAME_DIST_INSTALL"
MODULE("gsmgr");

using namespace isp_api;

using namespace mgr_log;

using namespace mgr_file;

// --- User management
class aGameDistr : public ListAction {
public:
	aGameDistr( const string& distr_xml_path )
		: ListAction( "gamedistr", MinLevel( lvAdmin) )
		, InstallDistr( this )
		, DistrXmlPath( distr_xml_path )
	{
	}

	virtual void List( Session &ses ) const {
		const string name_end = ".xml";
		Dir distXmlDir( DistrXmlPath );
		while( distXmlDir.Read() ) {
			string current_file_name = distXmlDir.name();
			if( current_file_name.size() - current_file_name.find( name_end ) != name_end.size() )
				continue;
			mgr_xml::Xml distr_xml;
			distr_xml.LoadFromFile( distXmlDir.FullName() );
			lt::InfoList tasks = lt::GetQueueTasks( GAME_INSTALL_QUEUE_NAME );
			if( auto game_node = distr_xml.GetNode( "/game" ) ) {
				ses.NewElem();
				ses.AddChildNode( "name", game_node.GetProp( "name" ) );
				ses.AddChildNode( "distrxml", distXmlDir.FullName() );

				string game_dir_name = str::GetWord( current_file_name, name_end );
				string game_path = ConcatPath( mgr_cf::GetPath( ConfGameInstallDir ), game_dir_name );
				Debug( "game_path %s ", game_path.c_str() );
				if( Exists( game_path ) ) {
					auto last_task = tasks.begin();
					for( auto iter = tasks.begin(); iter != tasks.end(); ++iter ) 
						if( iter->elid() == distXmlDir.name() && last_task->id() < iter->id() )
							last_task = iter;
					if( tasks.empty()
						|| ( lt::Info::sRun != last_task->status()
							&& lt::Info::sPre != last_task->status()
							&& lt::Info::sQueue != last_task->status() ) )
						ses.AddChildNode( "installed" );
					else
						ses.AddChildNode( "installing" );
				}
			} else Warning( "Incorrect game distributive xml: %s", distXmlDir.FullName().c_str() );
		}
	}

	class aInstallDistr: public GroupAction {
		public:
			aInstallDistr(aGameDistr *parent):GroupAction("install", MinLevel(lvAdmin), parent) {}
			void ProcessOne(Session & ses, const string &elid) const
			{
				auto xml_name = mgr_file::Name( elid );
				lt::LongTask GameDistInstallLT("sbin/gsinstalldist", xml_name, GAME_INSTALL_QUEUE_NAME);
				StringVector params;
				params.push_back("-x");
				params.push_back(xml_name);

				GameDistInstallLT.SetParams(params);
				new mgr_job::RunLongTask(GameDistInstallLT);
			}
		} InstallDistr;

		
	const string DistrXmlPath;
};

MODULE_INIT(gamedistr, "gsmgr")
{
	STrace();
	new aGameDistr( "var/gsdist" );
}
