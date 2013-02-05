#pragma once

template<>
class SvnCommand<ConfigRequest>
{
public:
	bool Run(SvnTask& task, ConfigRequest& req, ConfigResponse& resp)
	{
		if (req.key == "pluginTraits")
		{
			resp.requiresNetwork = false;
			resp.enablesCheckout = false;
			resp.enablesLocking = true;
			resp.enablesRevertUnchanged = false;
			resp.addTrait("svnUsername", "Username", "Subversion username", "", ConfigResponse::TF_Required);
			resp.addTrait("svnPassword", "Password", "Subversion password", "", ConfigResponse::TF_Required | ConfigResponse::TF_Password);
			resp.addTrait("svnRepos", "Repository", "Subversion Repository", "", ConfigResponse::TF_Required);
			resp.addTrait("svnOptions", "Options", "Subversion config options", "", ConfigResponse::TF_None);
		}
		else if (req.key == "pluginVersions")
		{
			resp.supportedVersions.insert(1);
		}
		else if (req.key == "assetsPath")
		{
			req.conn.Log() << "Set assetsPath to " << req.values[0] << unityplugin::Endl;
			task.SetAssetsPath(Join(req.values, " "));
		}
		else if (req.key == "svnRepos")
		{
			req.conn.Log() << "Set repos to " << req.values[0] << unityplugin::Endl;
		}
		else if (req.key == "svnUsername")
		{
			req.conn.Log() << "Set username to " << req.values[0] << unityplugin::Endl;
		}		
		else if (req.key == "svnPassword")
		{
			req.conn.Log() << "Set password to *********" << unityplugin::Endl;
		}		
		else if (req.key == "svnOptions")
		{
			req.conn.Log() << "Set options to " << req.values[0] << unityplugin::Endl;
		}
		else
		{
			std::string msg = "Unknown config field set on subversion plugin: ";
			msg += req.key;
			req.conn.Pipe().WarnLine(msg, MAConfig);
			req.invalid = true;
		}

		resp.Write();
		return true;
	}
};
