#pragma once

#include "ProcessWrapper.h"
#include "Parser.h"
#include "EngineEvent.h"
#include "Logger.h"
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <condition_variable>

namespace UCILoader {
	/*!
	 * @brief Enumeration of possible search operation status codes.
	 * 
	 * These codes represent the various states a chess engine search can be in,
	 * from initialization through completion or termination.
	 */
	enum SearchStatusCode {
		//! Status indicating the search hasn't been initiated yet (go command not sent)
		InitialisingSearch,
		//! Status indicating an active, ongoing search operation
		OnGoing,
		//! Status indicating the engine was killed or crashed before sending bestmove
		Terminated,	
		//! Status indicating the search exceeded the specified time limit
		TimedOut,  
		//! Status indicating a stop signal was sent but bestmove response not yet received
		Stopped,	
		//! Status indicating the search completed and bestmove result is ready for retrieval
		ResultReady 
	};

	/*!
	 * @brief Container for the result of a chess engine search operation.
	 * 
	 * @tparam Move The move type used by the chess engine
	 * 
	 * @details
	 * Holds the best move and optional ponder move returned by the engine after a search completes.
	 * The bestMove field is guaranteed to be non-null only when the search status is ResultReady.
	 * The ponderMove field is optional and will be nullptr if the engine did not provide one.
	 * 
	 * @warning Memory Management: The parent SearchConnection object is responsible for freeing
	 * the memory of pointers in this structure. Users should NOT manually delete members of
	 * this structure unless they constructed the instance manually.
	 */
	template<class Move>
	struct SearchResult {
		//! The best move chosen by the engine for the given position
		Move* bestMove;
		//! [Optional] Move the engine wishes to ponder on, or nullptr if not provided by the engine
		Move* ponderMove;
	};

	/*!
	 * @brief Cleanup utility function for SearchResult structures.
	 * 
	 * Safely deallocates the Move objects stored in a SearchResult structure.
	 * This function handles nullptr values gracefully and is safe to call on partially initialized structures.
	 * 
	 * @tparam Move The move type
	 * @param _search_result The SearchResult structure to clean up
	 */
	template<class Move>
	void destroySearchResultStruct(SearchResult<Move>& _search_result) {
		if (_search_result.bestMove) {
			delete _search_result.bestMove;
			_search_result.bestMove = nullptr;
		}

		if (_search_result.ponderMove) {
			delete _search_result.ponderMove;
			_search_result.ponderMove = nullptr;
		}
	}

	/*!
	 * @brief Thread-safe wrapper for managing SearchStatusCode state.
	 * 
	 * @details
	 * Provides synchronized access to the search status with atomic-like operations.
	 * Supports blocking waits, condition variables, and compare-swap semantics for safe concurrent access.
	 * Used internally by SearchConnection to manage search state across multiple threads.
	 */
	class SearchStatusWrapper {
		std::mutex statusLock;
		SearchStatusCode status =InitialisingSearch;
		std::condition_variable var;
	public:
		/*!
		 * @brief Set the search status to the specified code.
		 * 
		 * @param code The new SearchStatusCode value
		 */
		void set(SearchStatusCode code) {
			std::unique_lock<std::mutex> guard(statusLock);
			this->status = code;
			var.notify_all();
		}

		/*!
		 * @brief Atomically swap status if current status is OnGoing.
		 * 
		 * This is a compare-and-swap operation that only changes the status if it's currently OnGoing.
		 * Useful for implementing asynchronous timeouts and preventing race conditions.
		 * 
		 * @param newStatus The status to set if current status is OnGoing
		 * @return The status value before this operation
		 */
		SearchStatusCode swapIfOngoing(SearchStatusCode newStatus) {
			std::unique_lock<std::mutex> guard(statusLock);

			SearchStatusCode oldStatus = status;

			if (status == OnGoing) {
				status = newStatus;
			}

			return oldStatus;
		}

		/*!
		 * @brief Get the current search status.
		 * 
		 * @return The current SearchStatusCode
		 */
		SearchStatusCode get() {
			std::unique_lock<std::mutex> guard(statusLock);
			return status;
		}

		/*!
		 * @brief Block until the search status changes or timeout expires.
		 * 
		 * If the status remains OnGoing after the timeout period, automatically sets status to TimedOut.
		 * 
		 * @param dur Maximum duration to wait in milliseconds
		 * @return The final SearchStatusCode (may be TimedOut if no change occurred)
		 */
		SearchStatusCode waitFor(const std::chrono::milliseconds& dur) {

			std::unique_lock<std::mutex> guard(statusLock);


			if (status != OnGoing) 
				return status;
			

			var.wait_for(guard, dur);
			
			if (status == OnGoing) 
				status = TimedOut;
			
			return status;
		}

		/*!
		 * @brief Assignment operator for convenient status setting.
		 * 
		 * @param value The new status value
		 * @return Reference to the assigned value
		 */
		const SearchStatusCode & operator= (const SearchStatusCode& value) {
			set(value);
			return value;
		}

		bool operator == (const SearchStatusCode& value) {
			return value == get();
		}

		bool operator != (const SearchStatusCode& value) {
			return value != get();
		}
	};

	template <class Move> class EngineInstance;

	/*!
	 * @brief Manages and controls an ongoing chess engine search operation.
	 * 
	 * @tparam Move The move type used by the chess engine
	 * 
	 * @details
	 * SearchConnection provides a handle to an active search, allowing the user to:
	 * - Monitor search progress and status
	 * - Stop or pause the search
	 * - Retrieve results when ready
	 * - Implement timeouts
	 * 
	 * Instances are typically obtained by calling the search() method on an EngineInstance.
	 * The SearchConnection takes ownership of the search and manages its lifecycle.
	 * 
	 * @see EngineInstance::search() for initiating a search
	 * @see SearchStatusCode for understanding search states
	 */
	template<class Move>
	class SearchConnection {
	public:
		/*!
		 * @brief Exception thrown when attempting to retrieve results before search completes.
		 */
		class ResultNotReadyException : public std::exception {};
	private:

		friend EngineInstance<Move>;

		std::mutex lock;
		SearchResult<Move> result = { nullptr, nullptr };
		SearchStatusWrapper status;
		

		std::shared_ptr<ProcessWrapper> engine;

		void receiveBestMoveSignal(const Move* bestMove, const Move* ponderMove);
		SearchStatusCode getStatusNoLock();
	public:
		~SearchConnection();
		
		/*!
		 * @brief Constructor for SearchConnection.
		 * 
		 * @param engine Shared pointer to the engine process
		 */
		SearchConnection(std::shared_ptr<ProcessWrapper> engine) : engine(engine) {};

		/*!
		 * @brief Get the current status of the search operation.
		 * 
		 * @return The current SearchStatusCode
		 */
		SearchStatusCode getStatus();
		
		/*!
		 * @brief Retrieve the search result.
		 * 
		 * @return Const reference to the SearchResult containing bestMove and ponderMove
		 * @throws ResultNotReadyException if the search status is not ResultReady
		 * 
		 * @details
		 * Always call getStatus() first to verify the status is ResultReady before calling this method.
		 * This avoids potential ResultNotReadyException thrown when results are not yet available.
		 */
		const SearchResult<Move> & getResult();
		/*!
		 * @brief Send a stop command to halt the engine's search.
		 * 
		 * Sets search status to Stopped unless already in a final state (ResultReady or Terminated).
		 * This is a no-op if the search has already completed or been terminated.
		 * However, for TimedOut status, stop will still be sent to terminate the engine's ongoing work.
		 */
		void stop();
		/*!
		 * @brief Send a ponderhit command to the engine.
		 * 
		 * Instructs the engine to switch from pondering mode to active searching.
		 * This is a no-op if the search status is not OnGoing.
		 * The search status remains unchanged regardless of whether the command is sent.
		 */
		void ponderhit();
		/*!
		 * @brief Block until the search completes or timeout expires.
		 * 
		 * The calling thread will be suspended and woken when:
		 * - Search completes with a bestmove result
		 * - Engine crashes or terminates
		 * - The specified timeout duration expires
		 * 
		 * If the engine doesn't deliver bestmove before timeout, status will be set to TimedOut.
		 * 
		 * @param time Maximum duration to wait in milliseconds
		 * 
		 * @see getStatus() to check the final status after waiting
		 */
		void waitFor(const std::chrono::milliseconds& time);
		/*!
		 * @brief Set status to TimedOut if search is still ongoing.
		 * 
		 * This is intended for asynchronous use cases where the caller manages timeouts manually
		 * instead of using the waitFor() method. If status is already in a final state, this is a no-op.
		 */
		void timeOutIfNotFinished(); 
	};



	template<class Move>
	inline void SearchConnection<Move>::receiveBestMoveSignal(const Move* bestMove, const Move* ponderMove){
		assert(bestMove != nullptr);
		std::lock_guard<std::mutex> guard(lock);
		status = ResultReady;

		result.bestMove = new Move(*bestMove);
		if (ponderMove) result.ponderMove = new Move(*ponderMove);
	}

	template<class Move>
	inline SearchStatusCode SearchConnection<Move>::getStatusNoLock()
	{
		if (status == ResultReady || status == Terminated || status == TimedOut) // final statuses
			return status.get();


		if (!engine->healthCheck()) {
			status = Terminated;
			return status.get();
		}

		return status.get();
	}

	template<class Move>
	inline SearchConnection<Move>::~SearchConnection()
	{
		destroySearchResultStruct(result);
	}
	template<class Move>
	inline SearchStatusCode SearchConnection<Move>::getStatus()
	{
		std::lock_guard<std::mutex> guard(lock);
		return getStatusNoLock();
	}
	template<class Move>
	inline const SearchResult<Move>& SearchConnection<Move>::getResult()
	{
		std::lock_guard<std::mutex> guard(lock);
		if (status != ResultReady)
			throw ResultNotReadyException();
		
		return result;
	}
	template<class Move>
	inline void SearchConnection<Move>::stop()
	{
		std::lock_guard<std::mutex> guard(lock);
		SearchStatusCode currentStatus = getStatusNoLock();
		if (currentStatus == ResultReady || currentStatus == Terminated)
			return;

		status = Stopped;
		engine->getWriter()->write("stop\n", 5);
	}

	template<class Move>
	inline void SearchConnection<Move>::ponderhit()
	{
		std::lock_guard<std::mutex> guard(lock);
		SearchStatusCode currentStatus = getStatusNoLock();
		if (currentStatus != OnGoing)
			return;

		engine->getWriter()->write("ponderhit\n", 10);
	}

	template<class Move>
	inline void SearchConnection<Move>::waitFor(const std::chrono::milliseconds& time)
	{
		SearchStatusCode returnCode = status.waitFor(time);
	}

	template<class Move>
	inline void SearchConnection<Move>::timeOutIfNotFinished()
	{
		SearchStatusCode returnCode = status.swapIfOngoing(TimedOut); 
	}

	/*!
	 * @brief Proxy object for interacting with a UCI engine option.
	 * 
	 * @details
	 * EngineOptionProxy provides a convenient interface for reading and modifying engine options.
	 * Instances are typically obtained through bracket notation on EngineInstance::options:
	 * 
	 * @code
	 *     auto& hashOption = instance->options["Hash"];
	 * @endcode
	 * 
	 * Assigning values to this proxy automatically sends the appropriate setoption UCI command
	 * to the engine. The initial value is the engine's declared default.
	 * 
	 * @see EngineInstance::options
	 */
	class EngineOptionProxy {
		std::shared_ptr<AbstractPipeWriter> writer;
		Option value;

		void tryWrite(const std::string& value);
		void tryWrite(const char* text);
		void assertType(const OptionType& type) const;
		void validateSpinCandidate(const int32_t& value) const;
		void validateComboCandidate(const std::string& value) const;
		void parse(const std::string& s);

		int32_t parseInteger(const std::string& value); // throws ParsingError

	public:
		/*!
		 * @brief Exception thrown when an operation is incompatible with the option type.
		 * 
		 * For example, calling click() on a non-Button option, or assigning an integer to a string option.
		 */
		class WrongTypeError : public std::exception {};
		/*!
		 * @brief Exception thrown when setting an option to an unsupported value.
		 * 
		 * @details
		 * Different option types have different validation rules:
		 * - **Check options**: Value must be "on", "off", "true", or "false" (case insensitive)
		 * - **Spin options**: Value must be within [min, max] range (use getAsOption().spin_content() to check)
		 * - **Combo options**: Value must be in the list of supported values (case sensitive)
		 *   Use getAsOption().combo_content().supported_values to check allowed values
		 * - **Button options**: Cannot be assigned a value
		 */
		class NotSupportedValueException : public std::exception {};
		/*!
		 * @brief Exception thrown when parsing fails for a miscellaneus reason.
		 * 
		 * This often occurs when attempting to assign a non-integer string to a spin option.
		 */
		class ParsingError : public std::exception {};

		EngineOptionProxy() : value(), writer(nullptr) {};
		EngineOptionProxy(const Option& option, std::shared_ptr<AbstractPipeWriter> pipeWriter) : value(option), writer(pipeWriter) {};
		EngineOptionProxy(const EngineOptionProxy& other) : value(other.value), writer(other.writer) {};

		/*!
		 * @brief Get the option type.
		 * 
		 * @return The OptionType (Button, Check, Spin, Combo, or String)
		 */
		inline OptionType type() const { return value.type(); };
		
		/*!
		 * @brief Get the option name.
		 * 
		 * @return The option identifier (e.g., "Hash", "UCI_ShowWDL", "Clear Hash")
		 */
		inline const std::string& id() const { return value.id(); };
		/*!
		 * @brief Get the full Option object with all metadata.
		 * 
		 * @return Const reference to the underlying Option
		 * 
		 * Useful for exporting or inspecting the full option configuration including type-specific details.
		 */
		inline const Option& getAsOption() const { return value; };

		/*!
		 * @brief Activate a button option.
		 * 
		 * @throws WrongTypeError if the option type is not Button
		 */
		void click();
		
		/*!
		 * @brief Set option value from a string.
		 * 
		 * @param value The string value to assign
		 * @return Reference to the assigned value
		 * 
		 * @throws ParsingError if value cannot be parsed as integer for a spin option
		 * @throws NotSupportedValueException if value violates option constraints
		 * @throws WrongTypeError if trying to set a Button option
		 * 
		 * @see NotSupportedValueException for validation rules by option type
		 */
		const std::string& operator = (const std::string& value);

		/*!
		 * @brief Set option value from a C-string.
		 * 
		 * @param value The C-string value to assign
		 * @return Pointer to the assigned value
		 * 
		 * @throws ParsingError if value cannot be parsed as integer for a spin option
		 * @throws NotSupportedValueException if value violates option constraints
		 * @throws WrongTypeError if trying to set a Button option
		 * 
		 * @see NotSupportedValueException for validation rules by option type
		 */
		const char* operator = (const char* value);

		/*!
		 * @brief Set a spin option to an integer value.
		 * 
		 * @param number The integer value to assign
		 * @return Reference to the assigned value
		 * 
		 * @throws NotSupportedValueException if number is outside the valid range [min, max]
		 * @throws WrongTypeError if option type is not Spin
		 * 
		 * @see getAsOption().spin_content() to check min/max range
		 */
		const int32_t& operator= (const int32_t& number); 

		/*!
		 * @brief Set a check option to a boolean value.
		 * 
		 * @param value The boolean state (true or false)
		 * @return Reference to the assigned value
		 * 
		 * @throws WrongTypeError if option type is not Check
		 */
		const bool& operator= (const bool& value);

		/*!
		 * @brief Get check option value as boolean.
		 * 
		 * @throws WrongTypeError if option type is not Check
		 * @return The boolean option value
		 */
		operator bool();
		
		/*!
		 * @brief Get spin option value as integer.
		 * 
		 * @throws WrongTypeError if option type is not Spin
		 * @return The integer option value
		 */
		operator int();
		
		/*!
		 * @brief Get option value as string.
		 * 
		 * @throws WrongTypeError if option type is Button
		 * @return The string representation of the option value
		 */
		operator std::string();
	};

	/*!
	 * @brief Internal interface for consuming option updates from the engine.
	 * 
	 * This interface is used internally by EngineOptionsMap to populate options
	 * as they are received from the engine during UCI initialization.
	 * 
	 * @internal
	 */
	class IOptionConsumer {
	public:
		/*!
		 * @brief Process a newly discovered engine option.
		 * 
		 * @param o The Option received from the engine
		 */
		virtual void consume(const Option& o) = 0;
	};

	/*!
	 * @brief Container and iterator for engine option proxies.
	 * 
	 * @details
	 * EngineOptionsMap acts as a map-like container populated from engine's option responses
	 * during UCI initialization. Access options by name using the [] operator or iterate over
	 * all options using begin() and end().
	 * 
	 * Options are populated automatically as the engine sends its option declarations. It's recommended to call sync() on the EngineInstance
	 * to ensure all options are loaded before accessing this map.
	 * It's recommended to call sync() before querying options to ensure completeness.
	 * 
	 * Usage:
	 * @code
	 *     auto instance = builder.build(engine, Loggers::toStd() | LoggerTraits::Pretty);
	 *     instance->sync();  // Ensure all options are loaded
	 *     
	 *     if (instance->options.contains("Hash")) {
	 *         instance->options["Hash"] = 256;
	 *     }
	 *     
	 *     for (auto& option : instance->options) {
	 *         std::cout << option.id() << std::endl;
	 *     }
	 * @endcode
	 * 
	 * @see EngineInstance::sync() for populating options
	 */
	class EngineOptionsMap : private IOptionConsumer {
	private:
		using Map = std::unordered_map<std::string, EngineOptionProxy>;
		std::shared_ptr<AbstractPipeWriter> writer;
		Map proxies;

		void consume(const Option& option) override {
			proxies.insert({ option.id(), EngineOptionProxy(option, writer) });
		}

	public:
		using map_iterator = Map::iterator;

		class iterator {
		private:
			map_iterator _it;
		public:
			iterator(const map_iterator& val) {
				_it = val;
			};
			bool operator != (const iterator& other) {
				return _it != other._it;
			}

			bool operator ==(const iterator& other) {
				return !(*this != other);
			}

			iterator & operator++() {
				_it++;
				return *this;
			}
			EngineOptionProxy& operator*() {
				return _it->second;
			}
			const EngineOptionProxy& operator*() const {
				return _it->second;
			}
		};

		explicit EngineOptionsMap(const std::shared_ptr<AbstractPipeWriter>& writer)
			: writer(writer) {
		}

		EngineOptionProxy& get(const std::string& id) {
			return proxies.at(id);
		}

		const EngineOptionProxy& get(const std::string& id) const {
			return proxies.at(id);
		}

		/*!
		 * @brief Check if the engine has an option with the specified name.
		 * 
		 * @param id The option identifier to check
		 * @return True if the option exists, false otherwise
		 */
		bool contains(const std::string& id) const {
			return proxies.count(id);
		}

		/*!
		 * @brief Get iterator to the first option.
		 * 
		 * @return Iterator pointing to the first EngineOptionProxy
		 */
		iterator begin() { return iterator(proxies.begin()); }
		
		/*!
		 * @brief Get iterator to one past the last option.
		 * 
		 * @return Iterator pointing past the last EngineOptionProxy
		 */
		iterator end() { return iterator(proxies.end()); }

		/*!
		 * @brief Access option by name using bracket notation.
		 * 
		 * @param id The option identifier
		 * @return Reference to the EngineOptionProxy for the specified option
		 * @throws std::out_of_range if the option doesn't exist
		 */
		EngineOptionProxy& operator[](const std::string & id) {
			return get(id);
		}

		/*!
		 * @brief Access option by name using bracket notation (const version).
		 * 
		 * @param id The option identifier
		 * @return Const reference to the EngineOptionProxy for the specified option
		 * @throws std::out_of_range if the option doesn't exist
		 */
		const EngineOptionProxy& operator[](const std::string & id) const {
			return get(id);
		}

	};

	/*!
	 * @brief Top-level interface for managing and interacting with a UCI chess engine.
	 * 
	 * @tparam Move The move type used by the engine
	 * 
	 * @details
	 * EngineInstance is the primary interface for communicating with a chess engine over UCI protocol.
	 * It handles:
	 * - Engine initialization and synchronization
	 * - Option management and configuration
	 * - Search operations and result retrieval
	 * - Event emission for status changes
	 * - Graceful shutdown
	 * 
	 * **Typical Usage:**
	 * @code
	 *     // Create engine instance with logging
	 *     auto engine = builder.build(process, Loggers::toFile("debug.log") | LoggerTraits::Pretty);
	 *     
	 *     // Synchronize to load options and metadata
	 *     engine->sync();
	 *     
	 *     // Query engine info
	 *     std::cout << "Engine: " << engine->getName() << std::endl;
	 *     
	 *     // Configure options
	 *     if (engine->options.contains("Hash")) {
	 *         engine->options["Hash"] = 256;
	 *     }
	 *     
	 *     // Start search
	 *     auto search = engine->search(params, position, moves);
	 *     search->waitFor(std::chrono::seconds(5));
	 *     
	 *     if (search->getStatus() == ResultReady) {
	 *         auto result = search->getResult();
	 *         std::cout << "Best move: " << moveToString(result.bestMove) << std::endl;
	 *     }
	 * @endcode
	 * 
	 * @warning Always call sync() after construction to ensure engine options and metadata are loaded.
	 * @see EngineInstanceBuilder for creating instances
	 * @see Logger for configuring logging behavior
	 */
	template <class Move>
	class EngineInstance : public UCILoader::EventEmitter {
		std::shared_ptr<SearchConnection<Move>> currentConnection = nullptr;
		std::shared_ptr<ProcessWrapper> processWrapper;

		std::unique_ptr<Logger> logger;

		bool receivedReadyOk = false;
		std::atomic_bool quitCommandSend;

		std::string name = "<empty>";
		std::string author = "<empty>";

		std::condition_variable conditional_var;
		std::mutex lock;

		void sendToEngine(const std::string& msg);

		void tryReportEngineCrash();
	public:
		
		class _CommandHandler : public AbstractEngineHandler<Move> {
			EngineInstance<Move>* parent;
		public:
			_CommandHandler(EngineInstance<Move>* parent) : parent(parent) {};
			void onEngineName(const std::string& name) override;
			void onEngineAuthor(const std::string& author) override;
			void onUCIOK() override;
			void onReadyOK() override;
			void onBestMove(const Move& bestMove) override;
			void onBestMove(const Move& bestMove, const Move& ponderMove) override;
			void onInfo(const std::vector<Info<Move>>& infos) override;
			void onCopyProtection(ProcedureStatus status) override;
			void onRegistration(ProcedureStatus status) override;
			void onOption(const Option& option) override;
			void onError(const std::string& errorMsg) override;
		};

		EngineInstance(std::shared_ptr<ProcessWrapper> engineProcess, std::shared_ptr<Marschaler<Move>> moveMarshaler, std::shared_ptr<PatternMatcher> moveValidator, std::unique_ptr<Logger> && logger) :
			processWrapper(engineProcess), options(engineProcess->getWriter()), logger(std::move(logger)), quitCommandSend(false) {
			std::shared_ptr<AbstractEngineHandler<Move>> handler = std::static_pointer_cast<AbstractEngineHandler<Move>>(std::make_shared<EngineInstance<Move>::_CommandHandler>(this));
			auto parser = std::make_shared<UCIParser<Move>>(handler, moveMarshaler, moveValidator);
			engineProcess->listen([parser, this](std::string line) {
				parser->parseLine(line);
				line.push_back('\n'); // append newline character to line to style logger entry 
				this->logger->log(Logger::FromEngine, line);
			},
			[this](){this->tryReportEngineCrash();});
			sendToEngine("uci\n");
		};
		
		~EngineInstance() {
			quit();
		}

		/*!
		 * @brief Exception thrown when a sync() or ping() operation times out.
		 * 
		 * When this exception is thrown, the engine process will be terminated and the listener
		 * thread associated with the engine instance is shut down.
		 */
		class TimeoutException : public std::exception {};

		/*!
		 * @brief Exception thrown when attempting to start a search while one is already in progress.
		 * 
		 * If you need to start a new search anyway, obtain the current search with getCurrentSearch(),
		 * call stop() on it, then wait for it to complete with waitFor() before starting a new search.
		 */
		class EngineBusyException : public std::exception{};

		/*!
		 * @brief Map of available engine options.
		 * 
		 * Populated automatically from UCI option responses. Use sync() to ensure all options are loaded.
		 */
		EngineOptionsMap options;

		/*!
		 * @brief Synchronize engine state with local representation.
		 * 
		 * Sends an 'isready' command to the engine and waits for the 'readyok' response.
		 * This ensures the engine is responsive and have all options and metadata (name, author) loaded.
		 * 
		 * This method should be called after constructing an EngineInstance and before performing
		 * operations like querying options or starting searches.
		 * 
		 * @param timeout Maximum time to wait for engine response (default 10 seconds)
		 * @throws TimeoutException if engine fails to respond within timeout period
		 * 
		 * @details
		 * If a timeout occurs, the engine process will be terminated and this exception thrown.
		 * After a timeout, the EngineInstance should be considered unusable.
		 */
		void sync(std::chrono::milliseconds timeout = std::chrono::milliseconds(10000));
		
		/*!
		 * @brief Send the ucinewgame command to the engine.
		 * 
		 * Instructs the engine to reset its internal state (typically clearing the transposition table)
		 * before starting future searches. Useful when starting a new game or test suite.
		 */
		void ucinewgame();

		/*!
		 * @brief Measure engine latency by performing a ping operation.
		 * 
		 * This is a convenience wrapper around sync() that measures the round-trip time.
		 * 
		 * @param timeout Maximum time to wait for engine response (default 10 seconds)
		 * @return The latency in milliseconds
		 * @throws TimeoutException if engine fails to respond within timeout period
		 */
		std::chrono::milliseconds ping(std::chrono::milliseconds timeout = std::chrono::milliseconds(10000));
		/*!
		 * @brief Get the currently active search connection.
		 * 
		 * @return Shared pointer to the current SearchConnection, or nullptr if no search is in progress
		 */
		std::shared_ptr<SearchConnection<Move>> getCurrentSearch();
		
		/*!
		 * @brief Start a new search operation with specified parameters.
		 * 
		 * @tparam Move The move type
		 * @param params Search parameters (time, depth, nodes, etc.) - see GoParamsBuilder from UCI.h
		 * @param pos Root position to analyze - must implement PositionFormatter interface
		 * @param moves Sequence of moves to play before the search position (useful for openings)
		 * @return Shared pointer to the SearchConnection for monitoring and controlling the search
		 * @throws EngineBusyException if a search is already in progress
		 * 
		 * @details
		 * Example with StandardChess variant:
		 * @code
		 *     std::vector<StandardChessMove> moves = {e2e4, e7e5, g1f3};
		 *     auto search = engine->search(
		 *         GoParamsBuilder().depth(20).build(),
		 *         StartPos(),
		 *         moves
		 *     );
		 *     
		 *     search->waitFor(std::chrono::seconds(5));
		 *     if (search->getStatus() == ResultReady) {
		 *         auto result = search->getResult();
		 *         // Process bestMove and optional ponderMove
		 *     }
		 * @endcode
		 * 
		 * The position parameter determines the root position, while moves specify which
		 * moves the engine must play before reaching the actual search position.
		 */
		std::shared_ptr<SearchConnection<Move>> search(const GoParams<Move>& params, const PositionFormatter& pos,
			const std::vector<Move> moves = {});
		
		/*!
		 * @brief Get the engine's name as declared via the id name command.
		 * 
		 * @return The engine name, or "<empty>" if not provided by the engine
		 * 
		 * @details
		 * It's recommended to call sync() before querying this to ensure the engine name
		 * has been received and loaded.
		 */
		std::string getName();

		/*!
		 * @brief Get the engine author's name as declared via the id author command.
		 * 
		 * @return The author name, or "<empty>" if not provided by the engine
		 * 
		 * @details
		 * It's recommended to call sync() before querying this to ensure the author name
		 * has been received and loaded.
		 */
		std::string getAuthor();

		/*!
		 * @brief Check the health status of the underlying engine process.
		 * 
		 * @return True if the engine process is alive and responsive, false if it has been terminated
		 * 
		 * @details
		 * If this method returns false, the EngineInstance is in an unstable state and should not be used further.
		 */
		bool healthCheck();

		/*!
		 * @brief Manually terminate the engine process.
		 * 
		 * @details
		 * **Note:** Manually calling this method is unnecessary. The engine will be automatically terminated
		 * when the EngineInstance object goes out of scope via the destructor.
		 * 
		 * However, manual termination is allowed for exceptional cases, such as when the engine becomes
		 * stuck in an infinite loop or is otherwise unresponsive. In such scenarios, calling quit() will
		 * forcibly shut down the engine process.
		 * 
		 * @warning **Important:** After calling quit() manually, the EngineInstance object becomes unusable.
		 * Do not attempt to perform any operations other than health check (search, sync, option configuration, etc.) on the 
		 * EngineInstance after calling this method.  
		 */
		void quit();
	};

	template<class Move>
	inline void EngineInstance<Move>::sendToEngine(const std::string& msg)
	{
			processWrapper->getWriter()->write(msg.c_str(), msg.size());	
			logger->log(Logger::ToEngine, msg);
	}

	template<class Move>
	inline void EngineInstance<Move>::sync(std::chrono::milliseconds timeout)
	{
		std::unique_lock<std::mutex> guard(lock);
		receivedReadyOk = false;

		try {
			sendToEngine("isready\n");
		}
		catch (PipeClosedException e) {
			throw TimeoutException(); 
		}
		
		conditional_var.wait_for(guard, timeout);
		if (!receivedReadyOk) {
			processWrapper->kill();
			throw TimeoutException();
		}

		auto e = NamedEngineEvents::makeSynchronizedEvent();
		emit(&e);
	}
	template<class Move>
	inline std::chrono::milliseconds EngineInstance<Move>::ping(std::chrono::milliseconds timeout)
	{
		auto start = std::chrono::steady_clock::now();
		sync(timeout);
		return std::chrono::milliseconds((std::chrono::steady_clock::now() - start).count()/1000000);
	}

	template<class Move>
	inline std::shared_ptr<SearchConnection<Move>> EngineInstance<Move>::getCurrentSearch()
	{
		std::unique_lock<std::mutex> guard(lock);
		return currentConnection;
	}
	template<class Move>
	inline std::shared_ptr<SearchConnection<Move>> EngineInstance<Move>::search(const GoParams<Move>& params, const PositionFormatter& pos,
		const std::vector<Move> moves)
	{
		std::unique_lock<std::mutex> guard(lock);

		if (currentConnection != nullptr)
			throw EngineBusyException();

		currentConnection = std::make_shared<SearchConnection<Move>>(processWrapper);

		sendToEngine(UciFormatter<Move>::position(pos, moves));
		sendToEngine(UciFormatter<Move>::go(params));
		currentConnection->status.set(OnGoing);

		auto event = NamedEngineEvents::makeSearchStartedEvent();
		emit(&event);
		return currentConnection;
	}
	template<class Move>
	inline std::string EngineInstance<Move>::getName()
	{
		std::unique_lock<std::mutex> guard(lock);
		return name;
	}

	template<class Move>
	inline std::string EngineInstance<Move>::getAuthor()
	{
		std::unique_lock<std::mutex> guard(lock);
		return author;
	}

	template<class Move>
	inline void EngineInstance<Move>::quit()
	{
		if (quitCommandSend)
			return;

		quitCommandSend = true;
		try {
			sendToEngine("quit\n");
		}
		catch (PipeClosedException ignored) {
			// If we got here, the engine process in in unstable state, but since we are already killing it, we can ignore it.
		};
		

		int counter = 0;
		
		while (processWrapper->healthCheck()) {
			
			if (counter == 10) {
				processWrapper->kill();
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			++counter;
		};
	}

	template<class Move>
	void EngineInstance<Move>::tryReportEngineCrash() {
		if (quitCommandSend == false) {
			auto event = NamedEngineEvents::makeEngineCrashedEvent();
			emit(&event);
		};
	};


	template<class Move>
	inline bool  EngineInstance<Move>::healthCheck() {
		return processWrapper->healthCheck();
	}

	template<class Move>
	inline void EngineInstance<Move>::ucinewgame(){
		sendToEngine("ucinewgame\n");
	}

	template<class Move>
	inline void EngineInstance<Move>::_CommandHandler::onEngineName(const std::string& name)
	{
		std::unique_lock<std::mutex> guard(parent->lock);
		parent->name = name;
	}

	template<class Move>
	inline void EngineInstance<Move>::_CommandHandler::onEngineAuthor(const std::string& author)
	{
		std::unique_lock<std::mutex> guard(parent->lock);
		parent->author = author;
	}

	template<class Move>
	inline void EngineInstance<Move>::_CommandHandler::onUCIOK()
	{
		// TODO: do something
	}

	template<class Move>
	inline void EngineInstance<Move>::_CommandHandler::onReadyOK()
	{
		std::unique_lock<std::mutex> guard(parent->lock);
		parent->receivedReadyOk = true;
		parent->conditional_var.notify_all();
	}

	template<class Move>
	inline void EngineInstance<Move>::_CommandHandler::onBestMove(const Move& bestMove)
	{
		std::unique_lock<std::mutex> guard(parent->lock);
		static auto searchCompletedEvent = NamedEngineEvents::makeSearchCompletedEvent();

		if (parent->currentConnection != nullptr) {
			parent->currentConnection->receiveBestMoveSignal(&bestMove, nullptr);
			parent->currentConnection = nullptr;
			parent->emit(&searchCompletedEvent);
		}
	}

	template<class Move>
	inline void EngineInstance<Move>::_CommandHandler::onBestMove(const Move& bestMove, const Move& ponderMove)
	{
		std::unique_lock<std::mutex> guard(parent->lock);
		static auto searchCompletedEvent = NamedEngineEvents::makeSearchCompletedEvent();
		if (parent->currentConnection != nullptr) {
			parent->currentConnection->receiveBestMoveSignal(&bestMove, &ponderMove);
			parent->currentConnection = nullptr;
			parent->emit(&searchCompletedEvent);
		}
	}

	template<class Move>
	inline void EngineInstance<Move>::_CommandHandler::onInfo(const std::vector<Info<Move>>& infos)
	{
		std::unique_lock<std::mutex>guard(parent->lock);

		auto clampEvent = UCILoader::NamedEngineEvents::makeInfoClampEvent<Move>(infos);
		parent->emit(&clampEvent);
		for (auto& info : infos) {
			auto infoEvent = UCILoader::NamedEngineEvents::makeInfoEvent<Move>(info);
			parent->emit(&infoEvent);
		}
	}

	template<class Move>
	inline void EngineInstance<Move>::_CommandHandler::onCopyProtection(ProcedureStatus status)
	{
	}

	template<class Move>
	inline void EngineInstance<Move>::_CommandHandler::onRegistration(ProcedureStatus status)
	{
	}

	template<class Move>
	inline void EngineInstance<Move>::_CommandHandler::onOption(const Option& option)
	{
		std::unique_lock<std::mutex> guard(parent->lock);
		IOptionConsumer* consumer = (IOptionConsumer*)(&parent->options);
		consumer->consume(option);
	}

	template<class Move>
	inline void EngineInstance<Move>::_CommandHandler::onError(const std::string& errorMsg)
	{
		parent->logger->log(Logger::FromParser, errorMsg);
	}

	/*!
	 * @brief Factory builder for creating EngineInstance objects.
	 * 
	 * @tparam Move The move type to use (standard chess, variant, etc.)
	 * 
	 * @details
	 * EngineInstanceBuilder is a factory class that creates and configures EngineInstance objects.
	 * It encapsulates move validation and parsing logic, allowing different chess variants to be supported.
	 * 
	 * **Standard Chess Usage:**
	 * If developing for standard chess, use the preconfigured StandardChess::ChessEngineInstanceBuilder:
	 * @code
	 *     auto process = openProcess({"stockfish.exe"}, "/");
	 *     auto engine = StandardChess::ChessEngineInstanceBuilder->build(
	 *         process, 
	 *         Loggers::toFile("engine.log") | LoggerTraits::Pretty
	 *     );
	 *     engine->sync();
	 * @endcode
	 * 
	 * **Custom Variant Usage:**
	 * For custom chess variants, provide your own PatternMatcher and Marschaler:
	 * @code
	 *     class CustomMove { ...  };
	 *     
	 *     auto validator = std::make_shared<MyMoveValidator>();
	 *     auto marshaler = std::make_shared<MyMoveMarshaler>();
	 *     
	 *     EngineInstanceBuilder<CustomMove> builder(validator, marshaler);
	 *     auto engine = builder.build(process, logger);
	 * @endcode
	 * 
	 * @see EngineInstance for the created object interface
	 * @see Logger for logging configuration
	 * @see ProcessWrapper for engine process management
	 */
	template <class Move>
	class EngineInstanceBuilder {
		std::shared_ptr<PatternMatcher> moveValidator;
		std::shared_ptr<Marschaler<Move>> moveMarshaler;
	public:
		/*!
		 * @brief Constructor for the builder.
		 * 
		 * @param validator Shared pointer to PatternMatcher for move validation
		 * @param marschaler Shared pointer to Marschaler for move parsing
		 */
		EngineInstanceBuilder(std::shared_ptr<PatternMatcher> validator, std::shared_ptr<Marschaler<Move>> marschaler) :
			moveValidator(validator), moveMarshaler(marschaler) {};
		
		/*!
		 * @brief Build an EngineInstance with default logging (no-op logger).
		 * 
		 * @param engineProcess Raw pointer to a ProcessWrapper for the engine executable
		 * @return Raw pointer to the newly created EngineInstance
		 * 
		 * @details
		 * The EngineInstance takes ownership of the engine process, so the caller does NOT need
		 * to manage its lifetime. However, the caller IS responsible for deleting the returned
		 * EngineInstance pointer when no longer needed.
		 * 
		 * For obtaining a ProcessWrapper, see the documentation of openEngineProcessFunction.
		 * 
		 * @see build(ProcessWrapper*, LoggerBuilder) for building with custom logging
		 */
		EngineInstance<Move>* build(ProcessWrapper* engineProcess);

		/*!
		 * @brief Build an EngineInstance with custom logging configuration.
		 * 
		 * @param engineProcess Raw pointer to a ProcessWrapper for the engine executable
		 * @param logger Configured LoggerBuilder for logging UCI messages
		 * @return Raw pointer to the newly created EngineInstance
		 * 
		 * @details
		 * The EngineInstance takes ownership of both the engine process and the logger instance,
		 * so the caller does NOT need to manage their lifetimes. However, the caller IS responsible
		 * for deleting the returned EngineInstance pointer when no longer needed.
		 * 
		 * Example:
		 * @code
		 *     auto logger = Loggers::toFile("engine.log") 
		 *                   | LoggerTraits::Pretty 
		 *                   | LoggerTraits::Timestamp;
		 *     auto engine = builder.build(process, logger);
		 * @endcode
		 * 
		 * For obtaining a ProcessWrapper, see the documentation of openEngineProcessFunction.
		 * For logger configuration options, see the Logger and Loggers documentation.
		 * 
		 * @see Logger for logging configuration options
		 * @see Loggers namespace for factory functions
		 * @see LoggerTraits namespace for customization traits
		 */
		EngineInstance<Move>* build(ProcessWrapper* engineProcess, LoggerBuilder logger);
	};

	template<class Move>
	inline EngineInstance<Move>* EngineInstanceBuilder<Move>::build(ProcessWrapper* engineProcess) {
		return build(engineProcess, Loggers::toNoting());
	};

	template<class Move>
	inline EngineInstance<Move>* EngineInstanceBuilder<Move>::build(ProcessWrapper* engineProcess, LoggerBuilder logger)
	{
		std::shared_ptr<ProcessWrapper> proces(engineProcess);
		return new EngineInstance<Move>(proces, moveMarshaler, moveValidator, std::move(logger.build()));
	}

}
