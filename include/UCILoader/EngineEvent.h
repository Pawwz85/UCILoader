#pragma once

#include <cstdint>
#include <mutex>
#include <list>
#include <memory>
#include <functional>
#include "UCI.h"


namespace UCILoader {
	
	class EngineEvent {

	public:
		virtual ~EngineEvent() {};
		virtual uint32_t getType() const = 0;
		virtual const void* getPayload() const = 0;
	};

	template<class Payload, uint32_t EventCode>
	class ConcreteEvent: public EngineEvent {
		Payload value;
	public:

		ConcreteEvent(const Payload& p) : value(p) {};

		// Odziedziczono za poœrednictwem elementu EngineEvent
		uint32_t getType() const override{
			return EventCode;
		}

		virtual const void * getPayload() const override { return &value; };
	};

	class NoPayloadEvent : public EngineEvent{
		uint32_t code;
	public:
		NoPayloadEvent(uint32_t c) : code(c) {};
		// Odziedziczono za poœrednictwem elementu EngineEvent
		uint32_t getType() const override
		{
			return code;
		}

		virtual const void* getPayload() const override { return nullptr; };
	};



	namespace NamedEngineEvents {
		const uint32_t EmitterConnected = 1u;
		const uint32_t EmitterDestroyed = 2u;
		const uint32_t EngineSynchronized = 4u;
		const uint32_t SearchStarted = 8u;
		const uint32_t SearchCompleted = 16u;
		const uint32_t EngineCrashed = 32u;
		const uint32_t InfoClampReceived = 64u;
		const uint32_t InfoReceived = 128u;


		//using EngineConnectedEvent = NoPayloadEvent;
		using EmitterConnectedEvent = NoPayloadEvent;
		using EmitterDestroyedEvent = NoPayloadEvent;
		using EngineSynchronizedEvent = NoPayloadEvent;
		using SearchStartedEvent = NoPayloadEvent;
		using SearchCompletedEvent = NoPayloadEvent;
		using EngineCrashedEvent = NoPayloadEvent;
		static EngineSynchronizedEvent makeSynchronizedEvent() { return NoPayloadEvent(EngineSynchronized); };
		static SearchStartedEvent makeSearchStartedEvent() { return NoPayloadEvent(SearchStarted); };
		static SearchCompletedEvent makeSearchCompletedEvent() { return NoPayloadEvent(SearchCompleted); };
		static EngineCrashedEvent makeEngineCrashedEvent() { return NoPayloadEvent(EngineCrashed); };
		
		template<class Move>
		static ConcreteEvent<std::vector<Info<Move>>, InfoClampReceived> makeInfoClampEvent(const std::vector<Info<Move>> & clamp) { return ConcreteEvent<std::vector<Info<Move>>, InfoClampReceived>(clamp); };
		
		template<class Move>
		static ConcreteEvent<Info<Move>, InfoReceived> makeInfoEvent(const Info<Move> & i) { return ConcreteEvent<Info<Move>, InfoReceived>(i); };
	};
	
	class EventEmitter;

	class EventReceiver {
		friend EventEmitter;

		std::list<EventEmitter*> publishers; // vanilla pointers. Scary shit, handle with causion
		
	public:
		virtual ~EventReceiver() {
			this->unlinkAll();
		}

		void unlinkAll();

		virtual uint32_t eventFilter() = 0;
		virtual void receiveEvent(const EngineEvent* event) = 0;
	};


	class FunctionCallbackEventReceiver : public EventReceiver {
		std::function<void(const EngineEvent*)> clb;
		uint32_t allowedEvents;
	public:
		FunctionCallbackEventReceiver(std::function<void(const EngineEvent*)> callback, uint32_t allowedEvents) : clb(callback), allowedEvents(allowedEvents) {};
		FunctionCallbackEventReceiver(std::function<void()> callback, uint32_t allowedEvents) : clb([callback](const EngineEvent*) {callback(); }), allowedEvents(allowedEvents) {};

		virtual void receiveEvent(const EngineEvent* event) override;
		virtual uint32_t eventFilter() override { return allowedEvents; };
	};

	class EventEmitter {
		mutable std::mutex lock; 
		std::list<std::shared_ptr<EventReceiver>> receivers;
	protected:

		virtual ~EventEmitter();

		// Exposes receiver count for testing purposes. 
		int countActiveReceivers() const; 
		void emit(const EngineEvent* event);
	public:

		void unlink(const EventReceiver* receiver);
		void connect(std::shared_ptr<EventReceiver> receiver);
		void connect(std::function<void(const EngineEvent*)> callback, uint32_t eventFilter);
		void connect(std::function<void()> callback, uint32_t eventFilter);
	};
	


}

