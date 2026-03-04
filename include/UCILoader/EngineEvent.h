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
		from the engine. See documentation of EventReceiver class or EventEmiter::connect method for more details about how 
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
			return nullptr. Other events return pointer to internally stored value and the C style cast could be performed over
			the returned pointer in ordered to access the associated event payload. 
			
			See documentation for constants in UCILoader::NamedEngineEvents to see which events carry a value
			and if they do, what is a type of a stored value. 
		*/
		virtual const void* getPayload() const = 0;
	};


	/*!
		Helper class used to define simple events with payload. It is recommended that the user should use one of the aliases
		of this class defined in NamedEngineEvents namespace instead of using this base class directly.
	*/
	template<class Payload, uint32_t EventCode>
	class ConcreteEvent: public EngineEvent {
		Payload value;
	public:

		ConcreteEvent(const Payload& p) : value(p) {};
		uint32_t getType() const override{
			return EventCode;
		}

		virtual const void * getPayload() const override { return &value; };
	};

	/*!
		Helper class used to define simple events with no payload. It is recommended that the user should use one of the aliases
		of this class defined in NamedEngineEvents namespace instead of using this base class directly.
	*/
	class NoPayloadEvent : public EngineEvent{
		uint32_t code;
	public:
		NoPayloadEvent(uint32_t c) : code(c) {};

		uint32_t getType() const override {
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


		/*!
			When a event receiver connects to an event emitter it is guaranteed that the will first event it will receive
			will be of EmitterConnectedEvent. This type of event has no payload. 
		*/
		using EmitterConnectedEvent = NoPayloadEvent;
		/*!
			Before emitter disconnects the event receiver, the removed receiver will be notified by this event. This type of
			event has no payload. 
		*/
		using EmitterDestroyedEvent = NoPayloadEvent;
		/*!
			Engine emits this event every time te sync method finishes successfully. This type of event has no payload. 
		*/
		using EngineSynchronizedEvent = NoPayloadEvent;
		/*!
			Engine emits this event every time it accepts new search request. This type of event has no payload. 
		*/
		using SearchStartedEvent = NoPayloadEvent;
		/*!
			Engine has finished calculating previous search request by successfully providing bestmove response.
			This type of event has no payload. To obtain result of the search call SearchConnection::getResult() method.
		*/
		using SearchCompletedEvent = NoPayloadEvent;
		/*!
			This event is emitted if engine's healthcheck() method has failed at any point. Note that healthcheck() method 
			is not being called automatically, so to fully benefit from capturing this event, user has to define its own
			mechanism that periodically checks engine health status.
		*/
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

	/*!
		The base class for all engine event observers. This class defines the interface that must be implemented by any custom
		event receiver that can be passed to EventEmitter::connect() method to handle incoming engine events. Refer to 
		NamedEngineEvents namespace to see the list of defined events. 
		
		To create a custom event receiver:
		1. Implement the `eventFilter()` method to return a bitmask that includes only the events you want to process (see
		documentation of eventFilter() method for examples).
		2. Implement the `receiveEvent()` method to handle each relevant event.
	*/
	class EventReceiver {
		friend EventEmitter;

		std::list<EventEmitter*> publishers;
		
	public:
		virtual ~EventReceiver() {
			this->unlinkAll();
		}
		
		/*!
			Disconnect the receiver from all connected emitters
		*/
		void unlinkAll();

		/*! 
			Returns the bitmask of events the receiver is listening for.

			For example, receiver listening only for search completions or engine crashes would implement this method
			like this:

			uint32_t eventFilter() {
				return NamedEngineEvents::SearchCompleted | NamedEngineEvents::EngineCrashed;
			}
		*/
		virtual uint32_t eventFilter() = 0;

		/*
			Handle the incoming event. It is guaranteed that event passed to this function will match the receiver's
			event filter.
		*/
		virtual void receiveEvent(const EngineEvent* event) = 0;
	};

	/*!
		Adaptor class that wraps a user defined callback into a EventReceiver object instance.
	*/
	class FunctionCallbackEventReceiver : public EventReceiver {
		std::function<void(const EngineEvent*)> clb;
		uint32_t allowedEvents;
	public:
		FunctionCallbackEventReceiver(std::function<void(const EngineEvent*)> callback, uint32_t allowedEvents) : clb(callback), allowedEvents(allowedEvents) {};
		FunctionCallbackEventReceiver(std::function<void()> callback, uint32_t allowedEvents) : clb([callback](const EngineEvent*) {callback(); }), allowedEvents(allowedEvents) {};

		virtual void receiveEvent(const EngineEvent* event) override;
		virtual uint32_t eventFilter() override { return allowedEvents; };
	};

	/*!
		A base class of all event engine emitters. It provides a mechanisms for notifying all connected receivers asynchronously.
	*/
	class EventEmitter {
		mutable std::mutex lock; 
		std::list<std::shared_ptr<EventReceiver>> receivers;
	protected:

		virtual ~EventEmitter();

		// Exposes receiver count for testing purposes. 
		int countActiveReceivers() const; 
		void emit(const EngineEvent* event);
	public:
		/*!
			Unregister given receiver from all future events.
		*/
		void unlink(const EventReceiver* receiver);

		/*!
			Register new event receiver to listen for engine events.

			See EventReceiver documentation for information about creating custom event receivers.
		*/
		void connect(std::shared_ptr<EventReceiver> receiver);

		/*!
			Register callback function to handle engine events generated by the engine.

			The first parameter is callback function, used to handle incoming events. 
			The second parameter is bitmask of event types this callback should handle. To obtain such bitmask, user
			should binary OR constant flag values defined in NamedEngineEvents namespace, such as EngineCrashed or 
			InfoReceived. The function will be called on the events, for which appropriate bit was set.
		*/
		void connect(std::function<void(const EngineEvent*)> callback, uint32_t eventFilter);

		/*!
			Register callback function to handle engine events generated by the engine.

			The first parameter is callback function, used to handle incoming events.
			The second parameter is bitmask of event types this callback should handle. To obtain such bitmask, user
			should binary OR constant flag values defined in NamedEngineEvents namespace, such as EngineCrashed or
			InfoReceived. The function will be called on the events, for which appropriate bit was set.
		*/
		void connect(std::function<void()> callback, uint32_t eventFilter);
	};
	


}

