/*
 * Anope IRC Services
 *
 * Copyright (C) 2011-2017 Anope Team <team@anope.org>
 *
 * This file is part of Anope. Anope is free software; you can
 * redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software
 * Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see see <http://www.gnu.org/licenses/>.
 */

#include "module.h"
#include "modules/nickserv/update.h"
#include "modules/help.h"
#include "modules/botserv/bot.h"
#include "modules/memoserv.h"
#include "memotype.h"
#include "memoinfotype.h"
#include "ignoretype.h"

class MemoServCore : public Module, public MemoServ::MemoServService
	, public EventHook<NickServ::Event::NickRegister>
	, public EventHook<Event::ChanRegistered>
	, public EventHook<Event::BotDelete>
	, public EventHook<Event::NickIdentify>
	, public EventHook<Event::JoinChannel>
	, public EventHook<Event::UserAway>
	, public EventHook<Event::NickUpdate>
	, public EventHook<Event::Help>
{
	Reference<ServiceBot> MemoServ;

	MemoInfoType memoinfo_type;
	MemoType memo_type;
	IgnoreType ignore_type;

	bool SendMemoMail(NickServ::Account *nc, MemoServ::MemoInfo *mi, MemoServ::Memo *m)
	{
		Anope::string subject = Language::Translate(nc, Config->GetBlock("mail")->Get<Anope::string>("memo_subject").c_str()),
			message = Language::Translate(Config->GetBlock("mail")->Get<Anope::string>("memo_message").c_str());

		subject = subject.replace_all_cs("%n", nc->GetDisplay());
		subject = subject.replace_all_cs("%s", m->GetSender());
		subject = subject.replace_all_cs("%d", stringify(mi->GetIndex(m) + 1));
		subject = subject.replace_all_cs("%t", m->GetText());
		subject = subject.replace_all_cs("%N", Config->GetBlock("networkinfo")->Get<Anope::string>("networkname"));

		message = message.replace_all_cs("%n", nc->GetDisplay());
		message = message.replace_all_cs("%s", m->GetSender());
		message = message.replace_all_cs("%d", stringify(mi->GetIndex(m) + 1));
		message = message.replace_all_cs("%t", m->GetText());
		message = message.replace_all_cs("%N", Config->GetBlock("networkinfo")->Get<Anope::string>("networkname"));

		return Mail::Send(nc, subject, message);
	}

 public:
	MemoServCore(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, PSEUDOCLIENT | VENDOR)
		, MemoServ::MemoServService(this)

		, EventHook<NickServ::Event::NickRegister>(this)
		, EventHook<Event::ChanRegistered>(this)
		, EventHook<Event::BotDelete>(this)
		, EventHook<Event::NickIdentify>(this)
		, EventHook<Event::JoinChannel>(this)
		, EventHook<Event::UserAway>(this)
		, EventHook<Event::NickUpdate>(this)
		, EventHook<Event::Help>(this)

		, memoinfo_type(this)
		, memo_type(this)
		, ignore_type(this)
	{
		MemoServ::service = this;
	}

	~MemoServCore()
	{
		MemoServ::service = nullptr;
	}

	MemoResult Send(const Anope::string &source, const Anope::string &target, const Anope::string &message, bool force) override
	{
		bool ischan, isregistered;
		MemoServ::MemoInfo *mi = GetMemoInfo(target, ischan, isregistered, true);

		if (mi == NULL)
			return MEMO_INVALID_TARGET;

		Anope::string sender_display = source;

		User *sender = User::Find(source, true);
		if (sender != NULL)
		{
			if (!sender->HasPriv("memoserv/no-limit") && !force)
			{
				time_t send_delay = Config->GetModule("memoserv/main")->Get<time_t>("senddelay");
				if (send_delay > 0 && sender->lastmemosend + send_delay > Anope::CurTime)
					return MEMO_TOO_FAST;
				if (!mi->GetMemoMax())
					return MEMO_TARGET_FULL;
				if (mi->GetMemoMax() > 0 && mi->GetMemos().size() >= static_cast<unsigned>(mi->GetMemoMax()))
					return MEMO_TARGET_FULL;
				if (mi->HasIgnore(sender))
					return MEMO_SUCCESS;
			}

			NickServ::Account *acc = sender->Account();
			if (acc != NULL)
			{
				sender_display = acc->GetDisplay();
			}
		}

		if (sender != NULL)
			sender->lastmemosend = Anope::CurTime;

		MemoServ::Memo *m = Serialize::New<MemoServ::Memo *>();
		m->SetMemoInfo(mi);
		m->SetSender(sender_display);
		m->SetTime(Anope::CurTime);
		m->SetText(message);
		m->SetUnread(true);

		EventManager::Get()->Dispatch(&MemoServ::Event::MemoSend::OnMemoSend, source, target, mi, m);

		if (ischan)
		{
			ChanServ::Channel *ci = ChanServ::Find(target);
			Channel *c = ci->GetChannel();

			if (c)
			{
				for (Channel::ChanUserList::iterator it = c->users.begin(), it_end = c->users.end(); it != it_end; ++it)
				{
					ChanUserContainer *cu = it->second;

					if (ci->AccessFor(cu->user).HasPriv("MEMO"))
					{
						if (cu->user->Account() && cu->user->Account()->IsMemoReceive())
							cu->user->SendMessage(*MemoServ, _("There is a new memo on channel \002{0}\002. Type \002{1}{2} READ {3} {4}\002 to read it."), ci->GetName(), Config->StrictPrivmsg, MemoServ->nick, ci->GetName(), mi->GetMemos().size()); // XXX
					}
				}
			}
		}
		else
		{
			NickServ::Account *nc = NickServ::FindNick(target)->GetAccount();

			if (nc->IsMemoReceive())
				for (User *u : nc->users)
					u->SendMessage(*MemoServ, _("You have a new memo from \002{0}\002. Type \002{1}{2} READ {3}\002 to read it."), source, Config->StrictPrivmsg, MemoServ->nick, mi->GetMemos().size());//XXX

			/* let's get out the mail if set in the nickcore - certus */
			if (nc->IsMemoMail())
				SendMemoMail(nc, mi, m);
		}

		return MEMO_SUCCESS;
	}

	void Check(User *u) override
	{
		NickServ::Account *nc = u->Account();
		if (!nc || !nc->GetMemos())
			return;

		auto memos = nc->GetMemos()->GetMemos();
		unsigned i = 0, end = memos.size(), newcnt = 0;
		for (; i < end; ++i)
			if (memos[i]->GetUnread())
				++newcnt;
		if (newcnt > 0)
			u->SendMessage(*MemoServ, newcnt == 1 ? _("You have 1 new memo.") : _("You have %d new memos."), newcnt);
		if (nc->GetMemos()->GetMemoMax() > 0 && memos.size() >= static_cast<unsigned>(nc->GetMemos()->GetMemoMax()))
		{
			if (memos.size() > static_cast<unsigned>(nc->GetMemos()->GetMemoMax()))
				u->SendMessage(*MemoServ, _("You are over your maximum number of memos (%d). You will be unable to receive any new memos until you delete some of your current ones."), nc->GetMemos()->GetMemoMax());
			else
				u->SendMessage(*MemoServ, _("You have reached your maximum number of memos (%d). You will be unable to receive any new memos until you delete some of your current ones."), nc->GetMemos()->GetMemoMax());
		}
	}

	MemoServ::MemoInfo *GetMemoInfo(const Anope::string &target, bool &is_registered, bool &ischan, bool create) override
	{
		if (!target.empty() && target[0] == '#')
		{
			ischan = true;
			ChanServ::Channel *ci = ChanServ::Find(target);
			if (ci != NULL)
			{
				is_registered = true;
				if (create && !ci->GetMemos())
				{
					MemoServ::MemoInfo *mi = Serialize::New<MemoServ::MemoInfo *>();
					mi->SetChannel(ci);
				}
				return ci->GetMemos();
			}
			else
			{
				is_registered = false;
			}
		}
		else
		{
			ischan = false;
			NickServ::Nick *na = NickServ::FindNick(target);
			if (na != NULL)
			{
				is_registered = true;
				if (create && !na->GetAccount()->GetMemos())
				{
					MemoServ::MemoInfo *mi = Serialize::New<MemoServ::MemoInfo *>();
					mi->SetAccount(na->GetAccount());
				}
				return na->GetAccount()->GetMemos();
			}
			else
			{
				is_registered = false;
			}
		}

		return NULL;
	}

	void OnReload(Configuration::Conf *conf) override
	{
		const Anope::string &msnick = conf->GetModule(this)->Get<Anope::string>("client");

		if (msnick.empty())
			throw ConfigException(Module::name + ": <client> must be defined");

		ServiceBot *bi = ServiceBot::Find(msnick, true);
		if (!bi)
			throw ConfigException(Module::name + ": no bot named " + msnick);

		MemoServ = bi;
	}

	void OnNickRegister(User *, NickServ::Nick *na, const Anope::string &) override
	{
		MemoServ::MemoInfo *mi = Serialize::New<MemoServ::MemoInfo *>();
		mi->SetAccount(na->GetAccount());
		mi->SetMemoMax(Config->GetModule(this)->Get<int>("maxmemos"));
	}

	void OnChanRegistered(ChanServ::Channel *ci) override
	{
		MemoServ::MemoInfo *mi = Serialize::New<MemoServ::MemoInfo *>();
		mi->SetChannel(ci);
		mi->SetMemoMax(Config->GetModule(this)->Get<int>("maxmemos"));
	}

	void OnBotDelete(ServiceBot *bi) override
	{
		if (bi == MemoServ)
			MemoServ = NULL;
	}

	void OnNickIdentify(User *u) override
	{
		this->Check(u);
	}

	void OnJoinChannel(User *u, Channel *c) override
	{
		ChanServ::Channel *ci = c->GetChannel();
		if (u->server && u->server->IsSynced() && ci && ci->GetMemos() && !ci->GetMemos()->GetMemos().empty() && ci->AccessFor(u).HasPriv("MEMO"))
		{
			if (ci->GetMemos()->GetMemos().size() == 1)
				u->SendMessage(*MemoServ, _("There is \002{0}\002 memo on channel \002{1}\002."), ci->GetMemos()->GetMemos().size(), ci->GetName());
			else
				u->SendMessage(*MemoServ, _("There are \002{0}\002 memos on channel \002{1}\002."), ci->GetMemos()->GetMemos().size(), ci->GetName());
		}
	}

	void OnUserAway(User *u, const Anope::string &message) override
	{
		if (message.empty())
			this->Check(u);
	}

	void OnNickUpdate(User *u) override
	{
		this->Check(u);
	}

	EventReturn OnPreHelp(CommandSource &source, const std::vector<Anope::string> &params) override
	{
		if (!params.empty() || source.c || source.service != *MemoServ)
			return EVENT_CONTINUE;
		source.Reply(_("\002%s\002 is a utility allowing IRC users to send short\n"
			"messages to other IRC users, whether they are online at\n"
			"the time or not, or to channels(*). Both the sender's\n"
			"nickname and the target nickname or channel must be\n"
			"registered in order to send a memo.\n"
			"%s's commands include:"), MemoServ->nick.c_str(), MemoServ->nick.c_str());
		return EVENT_CONTINUE;
	}

	void OnPostHelp(CommandSource &source, const std::vector<Anope::string> &params) override
	{
		if (!params.empty() || source.c || source.service != *MemoServ)
			return;
		source.Reply(_(" \n"
			"Type \002%s%s HELP \037command\037\002 for help on any of the\n"
			"above commands."), Config->StrictPrivmsg.c_str(), MemoServ->nick.c_str());
	}
};

MODULE_INIT(MemoServCore)
