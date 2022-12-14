#include "Changes.h"
#include "P4FileSetBaseCommand.h"
#include "P4Task.h"
#include "P4Utility.h"

class P4ChangeMoveCommand : public P4FileSetBaseCommand
{
public:
	P4ChangeMoveCommand() : P4FileSetBaseCommand("changeMove") {}
	
	virtual void ReadConnection() 
	{
		Conn() >> m_ChangeRevision;
	}

	virtual std::string SetupCommand(const CommandArgs& args)
	{
		std::string cmd("reopen -c ");
		if (m_ChangeRevision == kDefaultListRevision)
			return cmd + "default";
		else
			return cmd + m_ChangeRevision;
	}

private:
	ChangelistRevision m_ChangeRevision;

} cChangeMove;
