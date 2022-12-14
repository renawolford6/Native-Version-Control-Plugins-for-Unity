#include "P4Command.h"
#include "P4Task.h"
#include "POpen.h"
#include "P4MFA.h"
#include <sstream>
#include <stdio.h>

class P4SpecCommand : public P4Command
{
public:
	P4SpecCommand(const char* name) : P4Command(name) { }
	virtual bool Run(P4Task& task, const CommandArgs& args)
	{
		ClearStatus();
		m_Root.clear();

		Conn().Log().Info() << args[0] << "::Run()" << Endl;
		m_IsTestMode = args.size() > 2 && (args[2] == "-test");
		const std::string fallback_error = (args.size() > 1) ? args[1] : std::string();

		const std::string cmd = std::string("client -o ") + Quote(task.GetP4Client());
		if (!task.CommandRun(cmd, this))
		{
			//MFA: This error is handled here instead of in P4LoginCommand as it only shows up when running the first non-login command (currently always spec)
			if (StatusContains(GetStatus(), "login2"))
			{
				Conn().VerboseLine(GetStatusMessage());
				ClearStatus();

				if(!m_IsTestMode)
				{
					std::string error;
					if (!P4MFA::s_Singleton->WaitForHelixMFA(Conn(), task.GetP4Port(), task.GetP4User(), error))
					{
						GetStatus().insert(VCSStatusItem(VCSSEV_Error, error));
						Conn().Log().Notice() << GetStatusMessage() << Endl;
						return false;
					}
				}
				else
				{
					std::vector<std::string> args;
					P4Command* p4c = LookupCommand("login2");
					args.push_back("login2");
					bool res = p4c->Run(task, args);
					if (!res) return false;
				}

				//Try again the spec command so that we can get a value for m_Root
				if (!task.CommandRun(cmd, this))
				{
					if (StatusContains(GetStatus(), "login2"))
					{
						GetStatus().insert(VCSStatusItem(VCSSEV_Error, fallback_error));
						Conn().Log().Fatal() << GetStatusMessage() << Endl;
						return false;
					}
				}
			}
		}
		if (!m_Root.empty())
			task.SetP4Root(m_Root);
		Conn().Log().Info() << "Root set to " << m_Root << Endl;
		return true;
	}

    // Called with entire spec file as data
	void OutputInfo( char level, const char *data )
    {
		std::stringstream ss(data);
		Conn().VerboseLine(data);
		size_t minlen = 5; // "Root:"

		std::string line;
		while ( getline(ss, line) )
		{

			if (line.length() <= minlen || line.substr(0,minlen) != "Root:")
				continue;

			// Got the Root specification line
			std::string::size_type i = line.find_first_not_of(" \t:", minlen-1);
			if (i == std::string::npos)
			{

				GetStatus().insert(VCSStatusItem(VCSSEV_Error, std::string("invalid root specification - ") + line));
				return;
			}
			m_Root = line.substr(i);
			break;
		}
	}
private:
	std::string m_Root;
	bool m_IsTestMode;

} cSpec("spec");
