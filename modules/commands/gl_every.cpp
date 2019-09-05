/* Global core functions
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
#include <sstream>

class SendTimer;

class SendTimerManager
{
    public:
        SendTimerManager(Anope::string _nick, Anope::string _msg, time_t _interval);
        ~SendTimerManager();
        void setTimer(SendTimer* _timer) { timer = _timer; }

        const Anope::string GetNick() const { return nick; }
        const Anope::string GetMsg() const { return msg; }
        int GetInterval() const { return interval; }

    private:
        SendTimer* timer;
        const Anope::string nick;
        const Anope::string msg;
        const time_t interval;
};

class SendTimer : public Timer
{
    public:
        SendTimer(SendTimerManager* mng, time_t interval): Timer(interval), manager(mng) {}
        SendTimer(const SendTimer &other): Timer(other.manager->GetInterval()), manager(other.manager) {}

        void Tick(time_t t) anope_override
        {
            ServiceReference<GlobalService> GService("GlobalService", "Global");

            if (GService)
            {
                GService->SendGlobal(NULL, manager->GetNick(), manager->GetMsg());
                manager->setTimer(new SendTimer(*this));
            }
        }

    private:
        SendTimerManager* manager;
};

SendTimerManager::SendTimerManager(Anope::string _nick, Anope::string _msg, time_t _interval):
    timer(new SendTimer(this, _interval -((Anope::CurTime +_interval) % _interval))),
    nick(_nick), msg(_msg), interval(_interval)
{}

SendTimerManager::~SendTimerManager() {
    TimerManager::DelTimer(timer);
    delete timer;
}

class CommandGLEvery : public Command
{
public:
	CommandGLEvery(Module *creator) : Command(creator, "global/every", 1, 3)
	{
		this->SetDesc(_("Send periodically a message to all users"));
		this->SetSyntax(_("\037Option\037 \037parameters\037"));
	}

	void Execute(CommandSource &source, const std::vector<Anope::string> &params) anope_override
	{
        if (params[0].equals_ci("ADD")) {
            time_t t = Anope::DoTime(params[1]);
            const Anope::string &msg = params[2];
            timers.push_back(new SendTimerManager(source.GetNick(), msg, t));
            Log(LOG_ADMIN, source, this);
        } else if (params[0].equals_ci("DEL")) {
            size_t id = 0;
            std::stringstream hexParser(params[1].c_str());
            hexParser >> std::hex >> id;
            if (hexParser.rdstate() & std::stringstream::failbit)
            {
                OnHelp(source, "");
                return;
            }
            for (std::vector<SendTimerManager*>::iterator i = timers.begin(); i != timers.end(); ++i) {
                if ((size_t) *i == id) {
                    delete *i;
                    timers.erase(i);
                    source.Reply("Global message removed");
                    return;
                }
            }
            source.Reply("Timer not found");
        } else if (params[0].equals_ci("LIST")) {
            source.Reply(" ");
            if (timers.empty())
                source.Reply("No Timer");
            else {
                for (std::vector<SendTimerManager*>::iterator i = timers.begin(); i != timers.end(); ++i) {
                    std::stringstream ss;
                    ss << *i << " [" << (*i)->GetNick() << "] " << (*i)->GetMsg();
                    source.Reply(ss.str());
                }
            }
        } else {
            OnHelp(source, "");
        }
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand) anope_override
	{
		this->SendSyntax(source);
		source.Reply(" ");
		source.Reply(_("ADD <time> <msg>\n"
                    "LIST\n"
                    "DEL <id>\n"
                    "message will be sent from the nick \002%s\002."), source.service->nick.c_str());
		return true;
	}

private:
    std::vector<SendTimerManager *> timers;
};

class GLEvery : public Module
{
	CommandGLEvery commandglevery;

 public:
	GLEvery(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, VENDOR),
		commandglevery(this)
	{

	}
};

MODULE_INIT(GLEvery)
