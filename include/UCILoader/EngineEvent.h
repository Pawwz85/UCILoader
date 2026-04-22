#pragma once

#include <cstdint>
#include <mutex>
#include <list>
#include <memory>
#include <functional>
#include "UCI.h"


namespace UCILoader {
	
	/*!
	 * @brief Base class for events emitted by an EngineInstance.
	 * 
	 * @details
	 * EngineEvent is the foundation of the asynchronous event system. Each event has a unique type code
	 * with exactly one bit set (power of 2), enabling efficient filtering and matching using bitwise operations.
	 * 
	 * **Event Type Codes:**
	 * All event type codes are defined in the NamedEngineEvents namespace as powers of 2, allowing safe
	 * bitwise OR operations to create event filters.
	 * 
	 * **Event Payloads:**
	 * Some events carry associated data (payload) accessible via getPayload(). Others have no payload and return nullptr.
	 * The specific event type determines both whether a payload exists and its data type.
	 * 
	 * **Typical Usage:**
	 * @code
	 *     uint32_t myFilter = (NamedEngineEvents::SearchCompleted | 
	 *                          NamedEngineEvents::EngineCrashed);
	 *     
	 *     if (event->getType() & myFilter) {
	 *         // Handle search completion or engine crash
	 *     }
	 * @endcode
	 * 
	 * @see NamedEngineEvents for predefined event types
	 * @see EventReceiver for handling events
	 * @see EventEmitter for emitting events
	 */
	class EngineEvent {

	public:
		/*!
		 * @brief Virtual destructor for proper cleanup of derived classes.
		 */
		virtual ~EngineEvent() {};
		
		/*!
		 * @brief Get the type code of this event.
		 * 
		 * @return A uint32_t type code with exactly one bit set (power of 2)
		 * 
		 * @details
		 * Each event type is represented by a unique power-of-2 value, enabling efficient bitwise filtering.
		 * Type codes are defined in the NamedEngineEvents namespace.
		 * 
		 * **Bitwise Filtering Example:**
		 * @code
		 *     // Check if event is either SearchCompleted or EngineCrashed
		 *     uint32_t filter = (NamedEngineEvents::SearchCompleted | 
		 *                        NamedEngineEvents::EngineCrashed);
		 *     
		 *     if (event->getType() & filter) {
		 *         // Handle the matched event
		 *     }
		 * @endcode
		 * 
		 * @see NamedEngineEvents for available event types
		 */
		virtual uint32_t getType() const = 0;

		/*!
		 * @brief Get the payload data associated with this event.
		 * 
		 * @return Const void pointer to the event payload, or nullptr if no payload exists
		 * 
		 * @details
		 * Different event types carry different payload types. When a non-null pointer is returned,
		 * it points to an internally managed payload object. The caller must:
		 * 1. Know the expected type based on getType()
		 * 2. Cast the pointer to the appropriate type using C-style cast
		 * 3. NOT attempt to delete or modify the payload
		 * 
		 * **Payload Type Reference:**
		 * See NamedEngineEvents namespace documentation for which events have payloads and their types.
		 * For example:
		 * - InfoReceived: payload is Info<Move>*
		 * - InfoClampReceived: payload is std::vector<Info<Move>>*
		 * - EngineSynchronized, SearchStarted, etc.: payload is nullptr
		 * 
		 * **Example Usage:**
		 * @code
		 *     if (event->getType() == NamedEngineEvents::InfoReceived) {
		 *         auto info = (Info<StandardChessMove>*) event->getPayload();
		 *         std::cout << "Engine depth: " << info->getDepth() << std::endl;
		 *     }
		 * @endcode
		 */
		virtual const void* getPayload() const = 0;
	};


	/*!
	 * @brief Template for creating typed event objects with a payload.
	 * 
	 * @tparam Payload The data type of the event payload
	 * @tparam EventCode The unique event type code (must be power of 2)
	 * 
	 * @details
	 * ConcreteEvent is a template base class for implementing concrete event types that carry data.
	 * Instances are typically created by factory functions in NamedEngineEvents namespace
	 * rather than instantiated directly.
	 * 
	 * **Type Safety:**
	 * Payload is type-checked at compile time. Getters use virtual methods to return the payload pointer,
	 * which the caller must cast to the appropriate type based on getType().
	 * 
	 * @see NamedEngineEvents for concrete event type definitions
	 */
	template<class Payload, uint32_t EventCode>
	class ConcreteEvent: public EngineEvent {
		Payload value;
	public:

		/*!
		 * @brief Constructor that stores the payload.
		 * 
		 * @param p The payload value to store
		 */
		ConcreteEvent(const Payload& p) : value(p) {};
		/*!
		 * @brief Get the event type code.
		 * 
		 * @return The EventCode template parameter
		 */
		uint32_t getType() const override{
			return EventCode;
		}

		/*!
		 * @brief Get pointer to the stored payload.
		 * 
		 * @return Const void pointer to the Payload value
		 */
		virtual const void * getPayload() const override { return &value; };
	};

	/*!
	 * @brief Simple event type with no associated payload.
	 * 
	 * @details
	 * NoPayloadEvent is used for events that don't carry any data beyond their type code.
	 * Instances are typically created by factory functions in NamedEngineEvents namespace.
	 * 
	 * getPayload() always returns nullptr for this event type.
	 * 
	 * @see NamedEngineEvents for event type definitions using this class
	 */
	class NoPayloadEvent : public EngineEvent{
		uint32_t code;
	public:
		/*!
		 * @brief Constructor that stores the event type code.
		 * 
		 * @param c The event type code
		 */
		NoPayloadEvent(uint32_t c) : code(c) {};

		/*!
		 * @brief Get the event type code.
		 * 
		 * @return The stored event type code
		 */
		uint32_t getType() const override {
			return code;
		}

		/*!
		 * @brief Get the payload (always nullptr for NoPayloadEvent).
		 * 
		 * @return Always returns nullptr
		 */
		virtual const void* getPayload() const override { return nullptr; };
	};



	namespace NamedEngineEvents {

		//! @brief Event emitted when a receiver connects to an emitter
		const uint32_t EmitterConnected = 1u;
		//! @brief Event emitted when an emitter is destroyed
		const uint32_t EmitterDestroyed = 2u;
		//! @brief Event emitted after sync() completes successfully
		const uint32_t EngineSynchronized = 4u;
		//! @brief Event emitted when a new search starts
		const uint32_t SearchStarted = 8u;
		//! @brief Event emitted when a search completes
		const uint32_t SearchCompleted = 16u;
		//! @brief Event emitted when the engine crashes
		const uint32_t EngineCrashed = 32u;
		//! @brief Event emitted for batch info responses
		const uint32_t InfoClampReceived = 64u;
		//! @brief Event emitted for individual info responses
		const uint32_t InfoReceived = 128u;


		/*!
		 * @brief Event emitted when a receiver successfully connects to an emitter.
		 * 
		 * Guaranteed to be the first event sent to a newly connected receiver.
		 * No payload.
		 */
		using EmitterConnectedEvent = NoPayloadEvent;
		
		/*!
		 * @brief Event emitted before an emitter disconnects a receiver.
		 * 
		 * Notifies receivers that they are being disconnected from the emitter.
		 * No payload.
		 */
		using EmitterDestroyedEvent = NoPayloadEvent;
		
		/*!
		 * @brief Event emitted after sync() completes successfully.
		 * 
		 * Indicates the engine is synchronized and ready for operations.
		 * No payload.
		 */
		using EngineSynchronizedEvent = NoPayloadEvent;
		
		/*!
		 * @brief Event emitted when a new search request is accepted.
		 * 
		 * Emitted immediately after calling search().
		 * No payload.
		 */
		using SearchStartedEvent = NoPayloadEvent;
		
		/*!
		 * @brief Event emitted when a search completes with a result.
		 * 
		 * Engine has provided a bestmove response.
		 * To get the actual result, call SearchConnection::getResult().
		 * No payload.
		 */
		using SearchCompletedEvent = NoPayloadEvent;
		
		/*!
		 * @brief Event emitted when the engine process crashes or terminates unexpectedly.
		 * 
		 * No payload.
		 */
		using EngineCrashedEvent = NoPayloadEvent;
		
		/*!
		 * @brief Batch of info lines received from engine in the same update.
		 * 
		 * Payload: std::vector<Info<Move>>*
		 */
		// InfoClampReceived event type defined above
		
		/*!
		 * @brief Single info line received from the engine.
		 * 
		 * Payload: Info<Move>*
		 */
		// InfoReceived event type defined above


		/*!
		 * @brief Factory function to create an EngineSynchronized event.
		 * @return EngineSynchronizedEvent instance
		 */
		static EngineSynchronizedEvent makeSynchronizedEvent() { return NoPayloadEvent(EngineSynchronized); };
		/*!
		 * @brief Factory function to create a SearchStarted event.
		 * @return SearchStartedEvent instance
		 */
		static SearchStartedEvent makeSearchStartedEvent() { return NoPayloadEvent(SearchStarted); };
		/*!
		 * @brief Factory function to create a SearchCompleted event.
		 * @return SearchCompletedEvent instance
		 */
		static SearchCompletedEvent makeSearchCompletedEvent() { return NoPayloadEvent(SearchCompleted); };
		/*!
		 * @brief Factory function to create an EngineCrashed event.
		 * @return EngineCrashedEvent instance
		 */
		static EngineCrashedEvent makeEngineCrashedEvent() { return NoPayloadEvent(EngineCrashed); };
		
		/*!
		 * @brief Factory function to create an InfoClamp event with multiple Info objects.
		 * 
		 * @tparam Move The move type used by Info objects
		 * @param clamp Vector of Info objects from the engine
		 * @return ConcreteEvent with the vector as payload
		 */
		template<class Move>
		static ConcreteEvent<std::vector<Info<Move>>, InfoClampReceived> makeInfoClampEvent(const std::vector<Info<Move>> & clamp) { return ConcreteEvent<std::vector<Info<Move>>, InfoClampReceived>(clamp); };
		
		/*!
		 * @brief Factory function to create an Info event with a single Info object.
		 * 
		 * @tparam Move The move type used by the Info object
		 * @param i The Info object from the engine
		 * @return ConcreteEvent with the Info as payload
		 */
		template<class Move>
		static ConcreteEvent<Info<Move>, InfoReceived> makeInfoEvent(const Info<Move> & i) { return ConcreteEvent<Info<Move>, InfoReceived>(i); };
	};
	
	class EventEmitter;

	/*!
	 * @brief Base class for event observers that receive engine events.
	 * 
	 * @details
	 * EventReceiver defines the interface for custom event handlers. To receive events from an EngineInstance:
	 * 1. Derive from EventReceiver or use FunctionCallbackEventReceiver for simple cases
	 * 2. Implement eventFilter() to specify which events to receive (bitmask of event codes)
	 * 3. Implement receiveEvent() to handle each event
	 * 4. Connect to an EventEmitter via connect()
	 * 
	 * **Creating Custom Event Receivers:**
	 * @code
	 *     class MyEventReceiver : public EventReceiver {
	 *     public:
	 *         uint32_t eventFilter() override {
	 *             return (NamedEngineEvents::SearchCompleted | 
	 *                     NamedEngineEvents::EngineCrashed);
	 *         }
	 *         
	 *         void receiveEvent(const EngineEvent* event) override {
	 *             if (event->getType() == NamedEngineEvents::SearchCompleted) {
	 *                 std::cout << "Search done!" << std::endl;
	 *             }
	 *         }
	 *     };
	 * @endcode
	 * 
	 * @see FunctionCallbackEventReceiver for lambda/function callback approach
	 * @see EventEmitter for connecting receivers
	 */
	class EventReceiver {
		friend EventEmitter;

		std::list<EventEmitter*> publishers;
		
	public:
		/*!
		 * @brief Virtual destructor that disconnects from all connected emitters.
		 */
		virtual ~EventReceiver() {
			this->unlinkAll();
		}
		
		/*!
		 * @brief Disconnect this receiver from all connected emitters.
		 * 
		 * Safely removes all connections and prevents further event delivery.
		 */
		void unlinkAll();

		/*!
		 * @brief Get the event filter bitmask for this receiver.
		 * 
		 * @return A bitmask of NamedEngineEvents constants indicating which events to receive
		 * 
		 * @details
		 * This method determines which events this receiver will be notified about.
		 * The returned value should be a bitwise OR of one or more event constants from NamedEngineEvents.
		 * 
		 * **Example:**
		 * @code
		 *     uint32_t eventFilter() override {
		 *         return (NamedEngineEvents::SearchCompleted | 
		 *                 NamedEngineEvents::EngineCrashed);
		 *     }
		 * @endcode
		 * 
		 * Guaranteed that receiveEvent() is only called with events matching this filter.
		 */
		virtual uint32_t eventFilter() = 0;

		/*!
		 * @brief Handle an incoming event.
		 * 
		 * @param event Pointer to the EngineEvent to handle
		 * 
		 * @details
		 * This method is invoked by the event emitter when an event matching the receiver's
		 * eventFilter() is emitted. It's guaranteed that:
		 * - The event type matches the eventFilter() bitmask
		 * - The event pointer is valid (not nullptr)
		 * - The method is called from the context where emit() was called
		 * 
		 * Implementations should:
		 * - Keep processing time minimal to avoid blocking other receivers
		 * - Use thread-safe mechanisms if accessing shared state
		 * - Not delete the event pointer (it's owned by the emitter)
		 * - Not modify the event's state
		 */
		virtual void receiveEvent(const EngineEvent* event) = 0;
	};

	/*!
	 * @brief Adapter class that wraps a callback function as an EventReceiver.
	 * 
	 * @details
	 * FunctionCallbackEventReceiver allows simple function or lambda callbacks to be used
	 * with the event system without creating a full EventReceiver subclass.
	 * 
	 * Typically used via EventEmitter::connect() overloads:
	 * @code
	 *     emitter->connect(
	 *         [](const EngineEvent* event) {
	 *             if (event->getType() == NamedEngineEvents::SearchCompleted) {
	 *                 std::cout << "Search finished!" << std::endl;
	 *             }
	 *         },
	 *         NamedEngineEvents::SearchCompleted | NamedEngineEvents::EngineCrashed
	 *     );
	 * @endcode
	 * 
	 * @see EventEmitter::connect() for usage
	 */
	class FunctionCallbackEventReceiver : public EventReceiver {
		std::function<void(const EngineEvent*)> clb;
		uint32_t allowedEvents;
	public:
		/*!
		 * @brief Constructor for callbacks that receive the full event.
		 * 
		 * @param callback Function to invoke for each matching event (receives EngineEvent*)
		 * @param allowedEvents Bitmask of event types to pass to the callback
		 */
		FunctionCallbackEventReceiver(std::function<void(const EngineEvent*)> callback, uint32_t allowedEvents) : clb(callback), allowedEvents(allowedEvents) {};
		/*!
		 * @brief Constructor for simple parameterless callbacks.
		 * 
		 * @param callback Function to invoke for each matching event (receives no parameters)
		 * @param allowedEvents Bitmask of event types to trigger the callback
		 */
		FunctionCallbackEventReceiver(std::function<void()> callback, uint32_t allowedEvents) : clb([callback](const EngineEvent*) {callback(); }), allowedEvents(allowedEvents) {};

		/*!
		 * @brief Handle the event by invoking the stored callback.
		 * 
		 * @param event The EngineEvent to process
		 */
		virtual void receiveEvent(const EngineEvent* event) override;
		/*!
		 * @brief Get the event filter bitmask.
		 * 
		 * @return The allowedEvents bitmask passed at construction
		 */
		virtual uint32_t eventFilter() override { return allowedEvents; };
	};

	/*!
	 * @brief Base class for objects that emit engine events.
	 * 
	 * @details
	 * EventEmitter manages a collection of EventReceiver instances and notifies them when events occur.
	 * Multiple receivers can be connected and will all be notified synchronously when an event is emitted.
	 * 
	 * **Thread Safety:**
	 * Emitter uses internal locking to protect receiver list during additions/removals.
	 * However, event delivery is synchronous - receivers are called from the thread that calls emit().
	 * 
	 * **Usage Pattern:**
	 * @code
	 *     EventEmitter emitter;
	 *     
	 *     // Connect with lambda
	 *     emitter.connect(
	 *         [](const EngineEvent* e) {
	 *             std::cout << "Event type: " << e->getType() << std::endl;
	 *         },
	 *         NamedEngineEvents::SearchCompleted
	 *     );
	 *     
	 *     // Connect custom receiver
	 *     auto receiver = std::make_shared<MyEventReceiver>();
	 *     emitter.connect(receiver);
	 * @endcode
	 * 
	 * @see EventReceiver for implementing custom receivers
	 * @see EngineInstance for a real-world user of EventEmitter
	 */
	class EventEmitter {
		mutable std::mutex lock; 
		std::list<std::shared_ptr<EventReceiver>> receivers;
	protected:

		/*!
		 * @brief Virtual destructor ensuring proper cleanup.
		 */
		virtual ~EventEmitter();

		/*!
		 * @brief Count active receivers (for testing purposes).
		 * 
		 * @return Number of connected receivers
		 * @private
		 */
		int countActiveReceivers() const; 
		/*!
		 * @brief Emit an event to all connected receivers.
		 * 
		 * @param event The EngineEvent to deliver
		 * 
		 * @details
		 * Synchronously delivers the event to all receivers whose eventFilter() includes the event type.
		 * The delivery happens in the calling thread. Receivers are called in the order they were connected.
		 * 
		 * @protected This method is typically called only by subclasses (e.g., EngineInstance).
		 */
		void emit(const EngineEvent* event);
	public:
		/*!
		 * @brief Disconnect a specific receiver from future events.
		 * 
		 * @param receiver Pointer to the EventReceiver to disconnect
		 * 
		 * @details
		 * Safely removes the receiver from the emitter's receiver list.
		 * The receiver is not deleted (caller retains ownership).
		 * Safe to call even if receiver is not connected.
		 */
		void unlink(const EventReceiver* receiver);

		/*!
		 * @brief Connect a custom event receiver.
		 * 
		 * @param receiver Shared pointer to EventReceiver to connect
		 * 
		 * @details
		 * Registers a custom EventReceiver to receive events. The emitter maintains a shared_ptr
		 * to the receiver, ensuring it remains alive as long as connected.
		 * 
		 * The first event sent will be EmitterConnected with no payload.
		 * 
		 * See EventReceiver documentation for implementing custom receivers.
		 */
		void connect(std::shared_ptr<EventReceiver> receiver);

		/*!
		 * @brief Connect a callback function that receives the full event.
		 * 
		 * @param callback Function invoked for each matching event (receives const EngineEvent*)
		 * @param eventFilter Bitmask of NamedEngineEvents constants indicating which events to receive
		 * 
		 * @details
		 * Wraps the callback in a FunctionCallbackEventReceiver and connects it.
		 * Useful for simple event handling without creating a custom EventReceiver class.
		 * 
		 * **Example:**
		 * @code
		 *     engine->connect(
		 *         [](const EngineEvent* e) {
		 *             std::cout << "Event type: " << e->getType() << std::endl;
		 *         },
		 *         NamedEngineEvents::SearchCompleted | NamedEngineEvents::EngineCrashed
		 *     );
		 * @endcode
		 */
		void connect(std::function<void(const EngineEvent*)> callback, uint32_t eventFilter);

		/*!
		 * @brief Connect a simple parameterless callback function.
		 * 
		 * @param callback Function invoked for each matching event (takes no parameters)
		 * @param eventFilter Bitmask of NamedEngineEvents constants indicating which events to trigger the callback
		 * 
		 * @details
		 * Similar to the full event callback version, but for callbacks that don't need event details.
		 * Useful for simple notifications where you just need to know an event occurred.
		 * 
		 * **Example:**
		 * @code
		 *     engine->connect(
		 *         []() { std::cout << "Search done!" << std::endl; },
		 *         NamedEngineEvents::SearchCompleted
		 *     );
		 * @endcode
		 */
		void connect(std::function<void()> callback, uint32_t eventFilter);
	};
	


}

