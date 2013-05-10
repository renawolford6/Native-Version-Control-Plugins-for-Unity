#include "P4Command.h"
#include "P4Task.h"
#include "Utility.h"
#include <sstream>

using namespace std;

class P4LoginCommand : public P4Command
{
public:
	P4LoginCommand(const char* name) : P4Command(name) { m_AllowConnect = false; }
	virtual bool Run(P4Task& task, const CommandArgs& args)
	{
		if (!task.IsConnected()) // Cannot login without being connected
		{
			Pipe().Log().Info() << "Cannot login when not connected" << unityplugin::Endl;
			return false;
		}
		
		ClearStatus();
		
		m_LoggedIn = false;
		m_Password = task.GetP4Password();
		m_CheckingForLoggedIn = args.size() > 1;
		const string cmd = string("login") + (m_CheckingForLoggedIn ? string(" " ) + args[1] : string());

		if (!task.CommandRun(cmd, this) && !m_CheckingForLoggedIn)
		{
			string errorMessage = GetStatusMessage();			
			Pipe().Log().Notice() << "ERROR: " << errorMessage << unityplugin::Endl;
		}
		
		if (m_CheckingForLoggedIn)
			Pipe().Log().Debug() << "Is logged in: " << (m_LoggedIn ? "yes" : "no") << unityplugin::Endl;
		else
			Pipe().Log().Info() << "Login " << (m_LoggedIn ? "succeeded" : "failed") << unityplugin::Endl;
		m_CheckingForLoggedIn = false;
		return m_LoggedIn;
	}
	
	void OutputInfo( char level, const char *data )
    {
		string d(data);
		Pipe().Log().Debug() << "OutputInfo: " << d << unityplugin::Endl;
		if (m_CheckingForLoggedIn)
		{
			m_LoggedIn = StartsWith(d, "User ") && d.find(" ticket expires in ") != string::npos;
		}
		else
		{
			m_LoggedIn = StartsWith(d, "User ") && EndsWith(d, " logged in.");
			if (m_LoggedIn) 
				return;
			GetStatus().insert(VCSStatusItem(VCSSEV_Error, d));
		}
	}

	// Default handler of P4 error output. Called by the default P4Command::Message() handler.
	void HandleError( Error *err )
	{
		if ( err == 0 )
			return;
		
		// Suppress errors when just checking for logged in state
		if (m_CheckingForLoggedIn)
			return;

		// Base implementation. Will callback to P4Command::OutputError 
		P4Command::HandleError( err );
	}

	void OutputError( const char *errBuf )
	{
		if (StartsWith(string(errBuf), "Perforce password (P4PASSWD) invalid or unset."))
		{
			Pipe().WarnLine(errBuf);
		}
	}

	// Entering password
	void Prompt( const StrPtr &msg, StrBuf &buf, int noEcho ,Error *e )
	{
		Pipe().Log().Info() << "Prompted for password by server" << unityplugin::Endl;
		Pipe().Log().Debug() << "Prompt: " << msg.Text() << unityplugin::Endl;
		buf.Set(m_Password.c_str());
	}
	
private:
	bool m_LoggedIn;
	string m_Password;
	bool m_CheckingForLoggedIn;
	
} cLogin("login");
