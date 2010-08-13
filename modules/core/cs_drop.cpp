/* ChanServ core functions
 *
 * (C) 2003-2010 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

/*************************************************************************/

#include "module.h"

class CommandCSDrop : public Command
{
 public:
	CommandCSDrop() : Command("DROP", 1, 1)
	{
		this->SetFlag(CFLAG_ALLOW_FORBIDDEN);
		this->SetFlag(CFLAG_ALLOW_SUSPENDED);
	}

	CommandReturn Execute(User *u, const std::vector<Anope::string> &params)
	{
		Anope::string chan = params[0];
		ChannelInfo *ci;

		if (readonly)
		{
			notice_lang(Config.s_ChanServ, u, CHAN_DROP_DISABLED); // XXX: READ_ONLY_MODE?
			return MOD_CONT;
		}

		ci = cs_findchan(chan);

		if (ci->HasFlag(CI_FORBIDDEN) && !u->Account()->HasCommand("chanserv/drop"))
		{
			notice_lang(Config.s_ChanServ, u, CHAN_X_FORBIDDEN, chan.c_str());
			return MOD_CONT;
		}

		if (ci->HasFlag(CI_SUSPENDED) && !u->Account()->HasCommand("chanserv/drop"))
		{
			notice_lang(Config.s_ChanServ, u, CHAN_X_FORBIDDEN, chan.c_str());
			return MOD_CONT;
		}

		if ((ci->HasFlag(CI_SECUREFOUNDER) ? !IsFounder(u, ci) : !check_access(u, ci, CA_FOUNDER)) && !u->Account()->HasCommand("chanserv/drop"))
		{
			notice_lang(Config.s_ChanServ, u, ACCESS_DENIED);
			return MOD_CONT;
		}

		int level = get_access(u, ci);

		if (ci->c && ModeManager::FindChannelModeByName(CMODE_REGISTERED))
			ci->c->RemoveMode(NULL, CMODE_REGISTERED, "", false);

		if (ircd->chansqline && ci->HasFlag(CI_FORBIDDEN))
		{
			XLine x(ci->name);
			ircdproto->SendSQLineDel(&x);
		}

		Alog() << Config.s_ChanServ << ": Channel " << ci->name << " dropped by " << u->GetMask() << " (founder: " << (ci->founder ? ci->founder->display : "(none)") << ")";

		delete ci;

		/* We must make sure that the Services admin has not normally the right to
		 * drop the channel before issuing the wallops.
		 */
		if (Config.WallDrop && (level < ACCESS_FOUNDER || (!IsFounder(u, ci) && ci->HasFlag(CI_SECUREFOUNDER))))
			ircdproto->SendGlobops(ChanServ, "\2%s\2 used DROP on channel \2%s\2", u->nick.c_str(), chan.c_str());

		notice_lang(Config.s_ChanServ, u, CHAN_DROPPED, chan.c_str());

		FOREACH_MOD(I_OnChanDrop, OnChanDrop(chan));

		return MOD_CONT;
	}

	bool OnHelp(User *u, const Anope::string &subcommand)
	{
		if (u->Account() && u->Account()->IsServicesOper())
			notice_help(Config.s_ChanServ, u, CHAN_SERVADMIN_HELP_DROP);
		else
			notice_help(Config.s_ChanServ, u, CHAN_HELP_DROP);

		return true;
	}

	void OnSyntaxError(User *u, const Anope::string &subcommand)
	{
		syntax_error(Config.s_ChanServ, u, "DROP", CHAN_DROP_SYNTAX);
	}

	void OnServHelp(User *u)
	{
		notice_lang(Config.s_ChanServ, u, CHAN_HELP_CMD_DROP);
	}
};

class CSDrop : public Module
{
	CommandCSDrop commandcsdrop;

 public:
	CSDrop(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator)
	{
		this->SetAuthor("Anope");
		this->SetType(CORE);

		this->AddCommand(ChanServ, &commandcsdrop);
	}
};

MODULE_INIT(CSDrop)