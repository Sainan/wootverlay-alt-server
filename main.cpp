#include <iostream>

#include <AnalogueKeyboard.hpp>
#include <DigitalKeyboard.hpp>
#include <Server.hpp>
#include <ServerWebService.hpp>
#include <Socket.hpp>
#include <Task.hpp>
#include <Thread.hpp>
#include <WebSocketConnection.hpp>

using namespace soup;

struct Subscriber {};

struct AnnounceStateTask : public Task
{
	std::string state;

	AnnounceStateTask(std::string&& state)
		: state(std::move(state))
	{
	}

	void onTick() final
	{
		for (const auto& w : Scheduler::get()->workers)
		{
			if (w->type == Worker::SOCKET
				&& static_cast<Socket*>(w.get())->custom_data.isStructInMap(Subscriber)
				)
			{
				ServerWebService::wsSendText(*static_cast<Socket*>(w.get()), state);
			}
		}
		setWorkDone();
	}
};

static Server serv;

int main()
{
	ServerWebService srv;
	srv.should_accept_websocket_connection = [](Socket& s, const HttpRequest&, ServerWebService&)
	{
		s.custom_data.addStructToMap(Subscriber, Subscriber{});
		return true;
	};

	if (!serv.bind(32312, &srv))
	{
		std::cout << "Failed to bind TCP/32312" << std::endl;
		return 1;
	}
	std::cout << "Listening on TCP/32312" << std::endl;

	Thread t([](Capture&&)
	{
		AnalogueKeyboard akbd; akbd.disconnected = true;
		DigitalKeyboard dkbd;
		while (true)
		{
			if (akbd.disconnected)
			{
				for (auto& a : AnalogueKeyboard::getAll())
				{
					akbd = std::move(a);
					break;
				}
				continue;
			}

			const auto analogue_state = akbd.getActiveKeys();
			dkbd.update();

			std::string state;
			for (const auto& ak : analogue_state)
			{
				state.push_back('(');
				state.append(std::to_string(static_cast<unsigned int>(ak.getHidScancode())));
				state.push_back(':');
				state.append(std::to_string(ak.getFValue()));
				state.push_back(':');
				state.push_back(dkbd.keys[ak.sk] ? '1' : '0');
				state.push_back(')');
			}
			serv.add<AnnounceStateTask>(std::move(state));
		}
	});

	serv.reduceAddWorkerDelay();
	serv.run();
	return 0;
}
