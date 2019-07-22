
/* ChanServ core functions
 *
 * (C) 2003-2019 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

#include "module.h"

static Module *me;

class CommandCSBanList : public Command
{
 public:
	CommandCSBanList(Module *creator) : Command(creator, "chanserv/banlist", 1, 1)
	{
		this->SetDesc(_("List bans on a channel"));
		this->SetSyntax(_("\037channel\037"));
	}

	void sendBanList(Channel &c, CommandSource &source)
	{
		const Anope::string& modeName = "BAN";
		if (c.HasMode(modeName))
		{
			const std::vector<Anope::string> banned = c.GetModeList(modeName);

			source.Reply("BAN list for " +c.name +":");
			for (unsigned j =0; j < banned.size(); ++j)
				source.Reply(banned[j]);
			source.Reply("End of BAN list");
		}
		else
		{
			source.Reply(c.name +": No BAN");
		}
	}

	void Execute(CommandSource &source, const std::vector<Anope::string> &params) anope_override
	{
		const Anope::string &chan = params[0];
		Configuration::Block *block = Config->GetCommand(source);
		const Anope::string &mode = block->Get<Anope::string>("mode", "BAN");

		ChannelInfo *ci = ChannelInfo::Find(chan);
		if (ci == NULL)
		{
			source.Reply(CHAN_X_NOT_REGISTERED, chan.c_str());
			return;
		}

		Channel *c = ci->c;
		if (c == NULL)
		{
			source.Reply(CHAN_X_NOT_IN_USE, chan.c_str());
			return;
		}

		sendBanList(*(ci->c), source);
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand) anope_override
	{
		this->SendSyntax(source);
		source.Reply(" ");
		source.Reply(_("list the banned masks on a channel."));
		return true;
	}
};

class CSBanList : public Module
{
	CommandCSBanList commandcsbanlist;

 public:
	CSBanList(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, VENDOR), commandcsbanlist(this)
	{
		me = this;
	}
};

MODULE_INIT(CSBanList)
