#pragma once

#include <cstdint>
#include <mutex>
#include <list>
#include <memory>
#include <functional>
#include "UCI.h"


namespace UCILoader {
	
	/*!
		EngineEvent class describes events that could be emitted by a EngineInstance and are fundamental to the engine event system.
		Each event has dedicated type code which has exactly one bit set in its binary representation, allowing user to filter and 
		match events using bitwise operations. See documentation of getType() for an example.

		The event system of a library is pretty minimalistic and is focused on delivering asynchronous events that originated
		from the engine. See documentation of EventReceiver class or EventEmiiter::connect method for more details about how 
		user defined code interacts with engine events.
	*/
	class EngineEvent {

	public:
		virtual ~EngineEvent() {};
		/*!
			Returns an event type code associated with given type. The returned value will always be one of the constants
			defined in UCILoader::NamedEngineEvents namespace. Each of those predefined values is power of two, which allows
			user to check if event type is present in certain set of values by performing bitwise operations:

			if(event.getType() & (UCILoader::NamedEngineEvents::SearchStarted & UCILoader::NamedEngineEvents::EngineCrashed))'\n'
			// some logic here
			
			See documentation od UCILoader::NamedEngineEvents for detailed explanation of each event type emitted by a 
			library.
		*/
		virtual uint32_t getType() const = 0;

		/*!
			Returns a pointer to value associated with a given Event. Some events do not carry any value of them and will always 
			return nullptr. Other events return pointer to internaly stored value and the C style cast could be performed over
			the returned pointer in orderd to access the associated event payload. 
			
			See documentation for constants in UCILoader::NamedEngineEvents to see which events carry a value
			and if they do, what is a type of a stored value. 
		*/
		virtual const void* getPayload() const = 0;
	};

	template<class Payload, uint32_t EventCode>
	class ConcreteEvent: public EngineEvent {
		Payload value;
	public:

		ConcreteEvent(const Payload& p) : value(p) {};

		// Odziedziczono za po�rednictwem elementu EngineEvent
		uint32_t getType() const override{
			return EventCode;
		}

		virtual const void * getPayload() const override { return &value; };
	};

	class NoPayloadEvent : public EngineEvent{
		uint32_t code;
	public:
		NoPayloadEvent(uint32_t c) : code(c) {};
		// Odziedziczono za po�rednictwem elementu EngineEvent
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

