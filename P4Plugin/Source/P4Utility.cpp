#include "P4Utility.h"
#include "Utility.h"
#include <algorithm>
#include <functional>

int ActionToState(const std::string& action, const std::string& headAction,
				  const std::string& haveRev, const std::string& headRev)
{
	int state = kNone; // kLocal
	
	if (action == "add") state |= kAddedLocal;
	else if (action == "move/add") state |= kAddedLocal | kMovedLocal;
	else if (action == "edit") state |= kCheckedOutLocal;
	else if (action == "delete") state |= kDeletedLocal;
	else if (action == "move/delete") state |= kDeletedLocal | kMovedLocal;
	// else if (action == "local") state |= kLocal;
	/*
	 else if (action == "")
	 {
	 // No action means that we're not working with it locally.
	 
	 
	 return headAction == "delete" ? kLocal : kSynced;
	 }
	 */
	bool serverHaveRevForFile = !headRev.empty();
	bool localHaveRevForFile = !haveRev.empty();

	if (serverHaveRevForFile)
	{
		bool remoteUpdates = haveRev != headRev;
	
		if (remoteUpdates)
		{
			state |= kOutOfSync;
			if (headAction == "add") state |= kAddedRemote;
			else if (headAction == "move/add") state |= kAddedRemote | kMovedRemote;
			// else if (headAction == "edit") state |= kOutOfSync;
			else if (headAction == "delete")
			{
				if (haveRev.empty())
				{
					// Not in registered as in workspace and deleted remote ie. remove outofsync flag
					// This may happen deleting a file in vcs and creating a new file with the
					// same name.
					state = state & ~kOutOfSync;
				}
				else
				{
					state |= kDeletedRemote;
				}
			}
			else if (headAction == "move/delete") 
			{
				if (haveRev.empty())
				{
					// Not in registered as in workspace and moved remote ie. remove outofsync flag
					// This may happen moving a file in vcs and creating a new file with the
					// same name.
					state = state & ~kOutOfSync;
				}
				else
				{
					state |= kDeletedRemote | kMovedRemote;			
				}
			}
		}
		else
		{
			state |= kSynced;
		}
	}
	else if (localHaveRevForFile)
	{
		state |= kSynced;
	}

	return state;
}


std::string WildcardsAdd(const std::string& pathIn)
{
	// Perforce wildcards use hex values.  
	// The following characters below must be swapped for these
	std::string path = Replace (pathIn, "%", "%25"); // Must be 1st :)
	path = Replace (path, "#", "%23");
	path = Replace (path, "@", "%40");
	return Replace (path, "*", "%2A");
}


std::string WildcardsRemove (const std::string& pathIn)
{
	std::string path = Replace (pathIn, "%23", "#");
	path = Replace (path, "%40", "@");
	path = Replace (path, "%2A", "*");
	return Replace (path, "%25", "%"); // Must do this last or we could convert an actual % to another wildcard
}	


std::string ResolvedPath(const VersionedAsset& asset, int flags)
{
	std::string path = asset.GetPath();
	
	if (flags & kPathWild)
		path = WildcardsAdd(path);
	
	if (asset.IsFolder())
		path += (flags & kPathRecursive) ? "..." : "*";

	return path;
}


std::string ResolvePaths(VersionedAssetList::const_iterator b,
					VersionedAssetList::const_iterator e,
					int flags, const std::string& delim, const std::string& postfix)
{
	std::string paths;
	
	for (VersionedAssetList::const_iterator i = b; i != e; i++) 
	{
		if (!paths.empty())
			paths += delim;
		if ((flags & kPathSkipFolders) && !(flags & kPathRecursive) && i->IsFolder())
			continue;
		paths += "\"";
		paths += ResolvedPath(*i, flags);
		paths += postfix;
		paths += "\" ";
	}
	return paths;
}

void ResolvePaths(std::vector<std::string>& result, 
				  VersionedAssetList::const_iterator b,
				  VersionedAssetList::const_iterator e,
				  int flags, const std::string& delim)
{
	for (VersionedAssetList::const_iterator i = b; i != e; i++) 
	{
		if ((flags & kPathSkipFolders) && !(flags & kPathRecursive) && i->IsFolder())
			continue;
		result.push_back(ResolvedPath(*i, flags));
	}
}

std::string ResolvePaths(const VersionedAssetList& list, int flags, const std::string& delim, const std::string& postfix)
{
	return ResolvePaths(list.begin(), list.end(), flags, delim, postfix);
}

void ResolvePaths(std::vector<std::string>& result, const VersionedAssetList& list, int flags, const std::string& delim)
{
	ResolvePaths(result, list.begin(), list.end(), flags, delim);
}

std::string WorkspacePathToDepotPath(const std::string& root, const std::string& wp)
{
	return std::string("/") + wp.substr(root.length());
}

void PathToMovedPath(VersionedAssetList& l)
{
	std::for_each(l.begin(), l.end(), std::mem_fun_ref(&VersionedAsset::SwapMovedPaths));
}

void Partition(const StateFilter& filter,
	VersionedAssetList& l1_InOut,
	VersionedAssetList& l2_Out)
{
	VersionedAssetList::iterator bound = 
		stable_partition(l1_InOut.begin(), l1_InOut.end(), filter);
	l2_Out.clear();
	l2_Out.reserve(distance(bound, l1_InOut.end()));
	l2_Out.insert(l2_Out.end(), bound, l1_InOut.end());
	l1_InOut.resize(distance(l1_InOut.begin(), bound));
}
