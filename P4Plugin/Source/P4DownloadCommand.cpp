#include "VersionedAsset.h"
#include "Changes.h"
#include "P4Command.h"
#include "P4Task.h"
#include "P4Utility.h"
#include <map>

struct ConflictInfo
{
	std::string local;
	std::string base;
	std::string conflict;
};

const size_t kDelim1Len = 11; // length of " - merging "
const size_t kDelim2Len = 12; // length of " using base "
const size_t kDelim3Len = 6; // length of " - vs "

// Command to get the lowest base and highest merge target of all conflicts available 
// for the given files
static class P4ConflictInfoCommand : public P4Command
{
public:
	P4ConflictInfoCommand() : P4Command("_conflictInfo") { }
	
	bool Run(P4Task& task, const CommandArgs& args) 
	{ 
		ClearStatus();
		return true; 
	}
	
	// Default handle of perforce info callbacks. Called by the default P4Command::Message() handler.
	virtual void OutputInfo( char level, const char *data )
	{	
		// Level 48 is the correct level for view mapping lines. P4 API is really not good at providing these numbers
		std::string msg(data);
		bool propagate = true;
		size_t msgLen = msg.length();
		
		if (level != 48)
			P4Command::OutputInfo(level, data);

		Conn().VerboseLine(msg);

		if (msgLen > (kDelim1Len + kDelim2Len + 12) && msg.find(" - merging ") != std::string::npos) // 12 being the smallest possible path repr.
			HandleMergableAsset(msg);
		else if (msgLen > (kDelim3Len + 12) && msg.find(" - vs //") != std::string::npos)
			HandleNonMergableAsset(msg);
		else if (EndsWith(msg, " - no file(s) to resolve."))
			; // ignore
		else
			P4Command::OutputInfo(level, data);
	}

	std::string LookupByLocalPathWithNoCaseSentivitity(const std::string& path)
	{
		std::string localPath = path;
		ToLower(localPath);
		
		for (std::map<std::string, ConflictInfo>::iterator i = conflicts.begin(); i != conflicts.end(); ++i)
		{
			std::string p = i->first;
			ToLower(p);
			if (p == localPath)
				return i->first;
		}
		Conn().WarnLine("Cannot get conflict info for " + path);
		return path;
	}

	void HandleMergableAsset(const std::string& data)
	{
		// format of the string should be 
		// <localPath> - merging <conflictDepotPath> using base <baseDepotPath>
		// e.g.
		// /Users/foo/Project/foo.txt - merging //depot/Project/foo.txt#6,#7 using base //depot/foo.txt#5
		bool ok = false;
		{
		std::string::size_type i = data.find("//", 2);

		if (i == std::string::npos || i < (kDelim1Len + 2) ) // 2 being smallest possible path repr of "/x"
			goto out1;
		
		std::string localPath = Replace(data.substr(0, i - kDelim1Len), "\\\\", "/");
		localPath = Replace(localPath, "\\", "/");
		
		// Because case sensitivity between un*x and win is crazy in p4 we override the localPath 
		localPath = LookupByLocalPathWithNoCaseSentivitity(localPath);

		std::string::size_type j = data.find("//", i+2);
		if (j == std::string::npos || j < (kDelim1Len + kDelim2Len + 2 + 3)) // 2 + 5 + 2 being smallest possible path repr of "/x - merging //y#1 using base "
			goto out1;
		
		std::string conflictPath = data.substr(i,  j - i - kDelim2Len);
		
		if (j + 5 > data.length()) // the basePath must be a least 5 chars "//x#1"
			goto out1;
		
		std::string basePath = data.substr(j);

		std::string pref;
		Conn().VerboseLine(pref + "Local: " + localPath);
		Conn().VerboseLine(pref + "Base: " + basePath);
		Conn().VerboseLine(pref + "Conflict: " + conflictPath);

		ConflictInfo ci = { localPath, basePath, conflictPath };
		conflicts[localPath] = ci;
		ok = true;
		}
	out1:
		if (!ok)
			P4Command::OutputInfo(48, data.c_str());
	}

	void HandleNonMergableAsset(const std::string& data)
	{
		// format of the string should be 
		// <localPath> - vs <conflictDepotPath>
		// e.g.
		// /Users/foo/Project/foo.txt - vs //depot/Project/foo.txt#6,#7
		bool ok = false;
		{
		std::string::size_type i = data.find("//", 2);
		if (i == std::string::npos || i < (kDelim3Len + 2) ) // 2 being smallest possible path repr of "/x"
			goto out2;
		
		std::string localPath = Replace(data.substr(0, i - kDelim3Len), "\\\\", "/");
		localPath = Replace(localPath, "\\", "/");

		// Because case sensitivity between un*x and win is crazy in p4 we override the localPath 
		localPath = LookupByLocalPathWithNoCaseSentivitity(localPath);

		std::string conflictPath = data.substr(i);
		
		if (i + 5 > data.length()) // the basePath must be a least 5 chars "//x#1"
			goto out2;
	
		std::string pref;
		Conn().VerboseLine(pref + "Local: " + localPath);
		Conn().VerboseLine(pref + "Base: <empty>");
		Conn().VerboseLine(pref + "Conflict: " + conflictPath);

		ConflictInfo ci = { localPath, std::string(), conflictPath };
		conflicts[localPath] = ci;
		ok = true;
		}
	out2:
		if (!ok)
			P4Command::OutputInfo(48, data.c_str());
	}

	std::map<std::string,ConflictInfo> conflicts;

} cConflictInfo;


class P4DownloadCommand : public P4Command
{
public:
	P4DownloadCommand() : P4Command("download") { }

	bool Run(P4Task& task, const CommandArgs& args)
	{
		ClearStatus();
		Conn().Log().Info() << args[0] << "::Run()" << Endl;
		
		std::string baseCmd = "print -q -o ";
		std::string targetDir;
		Conn().ReadLine(targetDir);
		
		std::vector<std::string> versions;
		// The wanted versions to download. e.g. you can download both head, base of a file at the same time
		Conn() >> versions;
		
		VersionedAssetList assetList;
		Conn() >> assetList;
		std::vector<std::string> paths;
		ResolvePaths(paths, assetList, kPathWild | kPathSkipFolders);
		
		Conn().Log().Debug() << "Paths resolved" << Endl;

		Conn().BeginList();
		
		if (paths.empty())
		{
			Conn().EndList();
			Conn().WarnLine("No paths in fileset perforce command", MARemote);
			Conn().EndResponse();
			return true;
		}

		bool hasConflictInfo = false;
		
		// One call per file per version
		int idx = 0;
		for (std::vector<std::string>::const_iterator i = paths.begin(); i != paths.end(); ++i, ++idx)
		{
			std::string cmd = baseCmd;
			std::string tmpFile = targetDir + "/" + IntToString(idx) + "_";
			std::string path = *i;

			for (std::vector<std::string>::const_iterator j = versions.begin(); j != versions.end(); ++j) 
			{
				if (*j == kDefaultListRevision || *j == "head")
				{
					// default is head
					tmpFile += "head";
					std::string fileCmd = cmd + "\"" + tmpFile + "\" \"" + path + "#head\"";

					Conn().Log().Info() << fileCmd << Endl;
					if (!task.CommandRun(fileCmd, this))
						break;
					
					VersionedAsset asset;
					asset.SetPath(tmpFile);
					Conn() << asset;

				}
				else if (*j == "mineAndConflictingAndBase")
				{
					// make a dry run resolve with the -o flag to determine the first 
					// conflicting version and its base. The download.

					if (!hasConflictInfo)
					{
						cConflictInfo.ClearStatus();
						cConflictInfo.conflicts.clear();
						std::string localPaths = ResolvePaths(assetList, kPathWild | kPathSkipFolders);
						std::string rcmd = "resolve -o -n " + localPaths;
						Conn().Log().Info() << rcmd << Endl;

						// Tell conflict info job about the paths we want so that it can get the case sensitivity correct.
						cConflictInfo.conflicts.clear();
						std::vector<std::string> localPathsVector;
						ResolvePaths(localPathsVector, assetList, kPathWild | kPathSkipFolders);
						ConflictInfo emptyConflictInfo = { "", "", "" };
						for (std::vector<std::string>::const_iterator i = localPathsVector.begin(); i != localPathsVector.end(); ++i)
							cConflictInfo.conflicts[*i] = emptyConflictInfo;
						
						task.CommandRun(rcmd, &cConflictInfo);
						Conn() << cConflictInfo.GetStatus();
						
						if (cConflictInfo.HasErrors())
						{
							// Abort since there was an error fetching conflict info
							std::string msg = cConflictInfo.GetStatusMessage();
							if (!StartsWith(msg, "No file(s) to resolve"))
							{
								Conn() << cConflictInfo.GetStatus();
								goto error;
							}
						}
						hasConflictInfo = true;
					}
					
					Conn().VerboseLine(std::string("ConflictFor: ") + path);

					std::map<std::string,ConflictInfo>::const_iterator ci = cConflictInfo.conflicts.find(path);
					
					// Location of "mine" version of file. In Perforce this is always
					// the original location of the file.
					Conn() << assetList[idx];

					VersionedAsset asset;
					if (ci != cConflictInfo.conflicts.end())
					{
						std::string conflictFile = tmpFile + "conflicting";
						std::string conflictCmd = cmd + "\"" + conflictFile + "\" \"" + ci->second.conflict + "\"";
						Conn().Log().Info() << conflictCmd << Endl;
						if (!task.CommandRun(conflictCmd, this))
							break;
						
						asset.SetPath(conflictFile);
						Conn() << asset;
						
						std::string baseFile = "";

						// base can be empty when files are not mergeable
						if (!ci->second.base.empty())
						{
							baseFile = tmpFile + "base";
							std::string baseCmd = cmd + "\"" + baseFile + "\" \"" + ci->second.base + "\"";
							Conn().Log().Info() << baseCmd << Endl;
							if (!task.CommandRun(baseCmd, this))
								break;
						}
						else
						{
							asset.SetState(kMissing);
						}
						asset.SetPath(baseFile);
						Conn() << asset;						
					}
					else 
					{
						// no conflict info for this file
						asset.SetState(kMissing);
						Conn() << asset << asset;
					}	
				}
			}
		}
	error:
		// The OutputState and other callbacks will now output to stdout.
		// We just wrap up the communication here.
		Conn().EndList();
		Conn() << GetStatus();
		Conn().EndResponse();

		cConflictInfo.conflicts.clear();
		
		return true;
	}

} cDownload;
