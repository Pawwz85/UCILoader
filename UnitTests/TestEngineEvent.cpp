
#include "pch.h"
#include <UCILoader/EngineEvent.h>
#include <UCILoader/StandardChess.h>

using namespace UCILoader;
class ConcreteEventEmitter : public EventEmitter {
public:
	void broadcastEvent(const EngineEvent* e) {
		emit(e);
	}

	int countReceivers() {
		return countActiveReceivers();
	}
};


class  CustomEventReceiver : public EventReceiver {
public:

	uint32_t lastEventType = -1;
	uint32_t currentFilter = -1;

	uint32_t eventFilter() override
	{
		return currentFilter;
	}

	void receiveEvent(const EngineEvent* event) override
	{
		lastEventType = event->getType();
	}

	void setFilter(const uint32_t& filter) {
		currentFilter = filter;
	}

public:
};

TEST(Event, Emit) {
	ConcreteEventEmitter emitter;
	auto receiver = std::make_shared<CustomEventReceiver>();
	auto e = NamedEngineEvents::makeSynchronizedEvent();

	emitter.connect(receiver);
	emitter.broadcastEvent(&e);
	ASSERT_EQ(e.getType(), receiver->lastEventType);
	ASSERT_EQ(emitter.countReceivers(), 1);
}

TEST(Event, Unlink) {
	ConcreteEventEmitter emitter;
	auto receiver = std::make_shared<CustomEventReceiver>();
	auto e = NamedEngineEvents::makeSynchronizedEvent(); 
	emitter.connect(receiver);
	emitter.unlink(receiver.get());
	emitter.broadcastEvent(&e);
	ASSERT_NE(receiver->lastEventType, e.getType());
	ASSERT_EQ(emitter.countReceivers(), 0);
}

TEST(Event, EmptyFilter) {
	ConcreteEventEmitter emitter;
	auto receiver = std::make_shared<CustomEventReceiver>();
	auto e = NamedEngineEvents::makeSynchronizedEvent();

	receiver->setFilter(0);
	emitter.connect(receiver);
	emitter.broadcastEvent(&e);
	ASSERT_EQ(receiver->lastEventType, -1);
}

TEST(Event, MatchingFilter) {
	ConcreteEventEmitter emitter;
	auto receiver = std::make_shared<CustomEventReceiver>();
	auto e = NamedEngineEvents::makeSynchronizedEvent();
	receiver->setFilter(e.getType());
	emitter.connect(receiver);
	emitter.broadcastEvent(&e);
	ASSERT_EQ(receiver->lastEventType, e.getType());
}

TEST(Event, ReceiverUnlinkAll) {
	ConcreteEventEmitter emitter;
	auto receiver = std::make_shared<CustomEventReceiver>();
	auto e = NamedEngineEvents::makeSynchronizedEvent();

	emitter.connect(receiver);
	receiver->unlinkAll();
	ASSERT_EQ(emitter.countReceivers(), 0);
}

TEST(Event, EmitterConnectionEventIsSingleCasted) {
	ConcreteEventEmitter emitter;
	auto receiver = std::make_shared<CustomEventReceiver>();
	auto receiver2 = std::make_shared<CustomEventReceiver>();
	auto e = NamedEngineEvents::makeSynchronizedEvent();

	emitter.connect(receiver);
	emitter.broadcastEvent(&e);
	emitter.connect(receiver2);

	ASSERT_EQ(receiver->lastEventType, NamedEngineEvents::EngineSynchronized);
	ASSERT_EQ(receiver2->lastEventType, NamedEngineEvents::EmitterConnected);
}

TEST(Event, EmitterDestroyedEmitsEvent) {
	ConcreteEventEmitter* emitter = new ConcreteEventEmitter();
	auto receiver = std::make_shared<CustomEventReceiver>();

	emitter->connect(receiver);
	delete emitter;

	ASSERT_EQ(receiver->lastEventType, NamedEngineEvents::EmitterDestroyed);
}

TEST(Event, SingleReceiverMultipleEmmiters) {
	ConcreteEventEmitter emitter, emitter2;
	auto receiver = std::make_shared<CustomEventReceiver>();

	auto e = NamedEngineEvents::makeSynchronizedEvent();
	auto e2 = NamedEngineEvents::makeInfoEvent(InfoFactory<StandardChess::StandardChessMove>::makeCPUloadInfo(10));

	emitter.connect(receiver);
	emitter2.connect(receiver);
	emitter.broadcastEvent(&e);
	ASSERT_EQ(receiver->lastEventType, NamedEngineEvents::EngineSynchronized);
	emitter2.broadcastEvent(&e2);
	ASSERT_EQ(receiver->lastEventType, NamedEngineEvents::InfoReceived);
}

TEST(Event, ConnectToCallback1) {
	ConcreteEventEmitter emitter;

	auto e = NamedEngineEvents::makeSynchronizedEvent();
	uint32_t type = -1;

	emitter.connect([&type](const EngineEvent* e) {type = e->getType(); }, NamedEngineEvents::EngineSynchronized);
	ASSERT_EQ(type, -1);
	emitter.broadcastEvent(&e);
	ASSERT_EQ(type, NamedEngineEvents::EngineSynchronized);
}

TEST(Event, ConnectToCallback2) {
	ConcreteEventEmitter emitter;

	auto e = NamedEngineEvents::makeSynchronizedEvent();
	bool clb_activated = false;

	emitter.connect([&clb_activated]() {clb_activated = true; }, NamedEngineEvents::EngineSynchronized);
	ASSERT_FALSE(clb_activated);
	emitter.broadcastEvent(&e);
	ASSERT_TRUE(clb_activated);
}
