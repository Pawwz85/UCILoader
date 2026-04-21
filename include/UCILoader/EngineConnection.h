#pragma once

#include "ProcessWrapper.h"
#include "Parser.h"
#include "EngineEvent.h"
#include "Logger.h"
#include <mutex>
#include <unordered_map>
#include <condition_variable>

namespace UCILoader {
	/*!
		Possible return codes of search procedure. 
	*/
	enum SearchStatusCode {
		/*!
			InitialisingSearch is a status that is used if the _go_ command wasn't send yet to an engine.
		*/
		InitialisingSearch,
		/*!
			OnGoing is a status for the currently outgoing search.
		*/
		OnGoing,
		/*!
			Engine was killed or crashed before result was outputted
		*/
		Terminated,	
		/*!
			Search timed out.
		*/
		TimedOut,  
		/*!
			Stop signal was send, but  _bestmove_ response was not received yet
		*/
		Stopped,	
		/*!
			Engine has already responded with _bestmove_ for this search request
		*/
		ResultReady 
	};

	/*!
		A container for the result of a search call. The bestMove field is guaranteed to be set not NULL value if
		the search status is ResultReady, while ponderMove could be set to nullptr if the engine did not send a pondermove 
		candidate with the _bestmove_ command. Parent SearchConnection object is responsible for freeing the memory of 
		pointers in this structure once it goes out of scope, so user shouldn't call delete on members of this structure unless
		user constructed instance of this structure manually.
	*/
	template<class Move>
	struct SearchResult {
		/*!
			bestmove according to the engine
		*/
		Move* bestMove;
		/*!
			[Optional] move on which engines wishes to ponder on. In case if the engine didn't produce pondermove this field is set to nullptr
		*/
		Move* ponderMove;
	};

	/*!
		Cleaning procedure for SearchResult struct.
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
		An internal wrapper that provides a thread safe access over the SearchStatusCode variable.
		It provides various methods for interacting with the internal status, including explicit getter and setter,
		wait and swap mechanisms.
	*/
	class SearchStatusWrapper {
		std::mutex statusLock;
		SearchStatusCode status =InitialisingSearch;
		std::condition_variable var;
	public:
		/*!
			Explicitly set internal status to the provided code.
		*/
		void set(SearchStatusCode code) {
			std::unique_lock<std::mutex> guard(statusLock);
			this->status = code;
			var.notify_all();
		}

		/*!
			If the internal status is *OnGoin* this method will set status to the provided value.
			Otherwise this function will be a no-op. This method aims to allow asynchronous 
			timeouts that mimic atomic operations.
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
			Simple getter for the internal status code
		*/
		SearchStatusCode get() {
			std::unique_lock<std::mutex> guard(statusLock);
			return status;
		}

		/*!
			Wait up to *dur* milliseconds for search to complete. If the status is still
			*OnGoing* after awaiting period, the status is set to *timedOut*. 
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
			Convenience wrapper over *set* method
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
		The *SearchConnection* class allows user to interact with the currently performed search. 
		The instance of this object is typically obtained by calling *search* method of EngineInstance class.
	*/
	template<class Move>
	class SearchConnection {
	public:
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
		SearchConnection(std::shared_ptr<ProcessWrapper> engine) : engine(engine) {};

		
		SearchStatusCode getStatus();
		/*!
			Return SearchResult instance if result is ready. The ResultNotReadyException will be thrown in case if there is no result to return.
			To avoid that exception, user might call _getStatus_ method in order to make sure the status of the connection is *ResultReady*.
		*/
		const SearchResult<Move> & getResult();
		/*!
			Send the *stop* command to the engine and sets search status to _Stopped_ provided that current status is neither _ResultReady_ nor _Terminated_. 
			If the search was terminated with either of those codes, the resulting operation is a NOOP. Note that _TimedOut_ status doesn't result in a noop, so
			user can send *stop* command to terminate search of the engine in case if the engine fails do respond with the user specified time.
		*/
		void stop();
		/*!
			Send the *ponderhit* command to the engine. If the engine status is not equal to _OnGoing_ the resulting operation will be a NOOP, otherwise engine will be instructed
			to switch from pondering mode to searching mode. In any case, the actual search status will be left unchanged.
		*/
		void ponderhit();
		/*!
			Blocks the calling thread and waits up to _time_ milliseconds for the search to complete. The thread will wake up early if the search status changes - 
			engine crashes or a search completes. In the case the engine fails to deliver *bestmove* command before the wait ends, the TimedDown status is set once
			thread wakes up. Once waits is complete, call the _getStatus_ method to check exact status code.
		*/
		void waitFor(const std::chrono::milliseconds& time);
		/*!
			Immediately sets status to _TimedOut_ if the current status is _OnGoing_ and do nothing in any other case. This function is intended in asynchronous use cases
			where caller opted out from using _waitFor_ method in order to embrace manual timeouts.			
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
		engine->getWriter()->write("stop\n", 6);
	}

	template<class Move>
	inline void SearchConnection<Move>::ponderhit()
	{
		std::lock_guard<std::mutex> guard(lock);
		SearchStatusCode currentStatus = getStatusNoLock();
		if (currentStatus != OnGoing)
			return;

		engine->getWriter()->write("ponderhit\n", 11);
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
		An utitlity class designed to provide interface for interacting with UCI engines option.
		Provided that `instance` is a pointer to EngineInstance object, the instance of EngineOptionProxy is typically
		obtained in the following manner: instance->options["Option Name"]

		Initially, the value associated with given option is set to default value of an option declared by an engine using *setoption* command.
		Assigning value to instances of this class will automatically send *setoption* command to the engine with the specified value. 
		User can also query the value of the underlying option and store underlying value into chosen primitives (int, bool, std::string).
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
			This exception will be thrown if operation called is not available for given option type.
			For example, the click() method will throw this error if underlying option is not a option of a type Button or 
			user tries assigning integer value to a string option
		*/
		class WrongTypeError : public std::exception {};
		/*!
			This exception will be thrown when setting value for check, spin or combo option to a value that is not supported by this option.
			The check option will throw this error if user tries to assign string value to it that is not one of those values (case insensitive): [on, off, true, false]
			The spin option will throw this error if given value is not in range <min; max>. To check the supported range use .getAsOption().spin_value().
			The combo option will throw this error if given value is not in list of allowed values (case sensitive). To check the list of supported values call 
			.getAsOption().combo_value().supported_values
		*/
		class NotSupportedValueException : public std::exception {};
		/*!
			This exception will be thrown if you try to set spin option value to a string that doesn't containt an integer value
		*/
		class ParsingError : public std::exception {};

		EngineOptionProxy() : value(), writer(nullptr) {};
		EngineOptionProxy(const Option& option, std::shared_ptr<AbstractPipeWriter> pipeWriter) : value(option), writer(pipeWriter) {};
		EngineOptionProxy(const EngineOptionProxy& other) : value(other.value), writer(other.writer) {};

		inline OptionType type() const { return value.type(); };
		/*!
			Return the name of the option like 'clear hash' or 'UCI_ShowWDL'
		*/
		inline const std::string& id() const { return value.id(); };
		/*!
			Get current value as Option object. This method could be used to export current option configuration of the chess engine.
		*/
		inline const Option& getAsOption() const { return value; };

		/*!
			Click the button options. If the option's type is not 'Button' this will throw WrongTypeError.
		*/
		void click(); // button type only
		
		/*!
		   Set the value of the option to a given string. 
		   This method will throw ParsingError if you pass not integer string for a spin option.
		   This method will throw NotSupportedValueException if you violate option's supported values list. See NotSupportedValueException for details.
		   This method will throw WrongTypeError if you try to set ButtonValue.
		*/
		const std::string& operator = (const std::string& value);

		/*!
		   Set the value of the option to a given string. 
		   This method will throw ParsingError if you pass not integer string for a spin option.
		   This method will throw NotSupportedValueException if you violate option's supported values list. See NotSupportedValueException for details.
		   This method will throw WrongTypeError if you try to set ButtonValue.
		*/
		const char* operator = (const char* value);

		/*!
			Set the value of a spin option to a given number.
			This method will throw NotSupportedValueException if the given number is not in supported range. See NotSupportedValueException for details.
		    This method will throw WrongTypeError if you try to set value of non spin option
		*/
		const int32_t& operator= (const int32_t& number); 

		/*!
			Set the value of a check option to a given state. 
		    This method will throw WrongTypeError if you try to set value of non check option.
		*/
		const bool& operator= (const bool& value); // check type only

		
		//!	Get value of a check option. This will throw WrongTypeError if you try to get value of non check option
		operator bool();
		//! Get value of a spin option. This will throw WrongTypeError if you try to get value of non spin option
		operator int();
		//! Get the string representation of a given option. This will throw WrongTypeError if you try to get value of a Button.
		operator std::string();
	};

	/*
		The behavior of inserting a new options inside options map should be internal for EngineInstance. Therefore, EngineOptionsMap will
		inherit this interface privately to hide that method from the user.
	*/
	class IOptionConsumer {
	public:
		virtual void consume(const Option& o) = 0;
	};

	/*!
		A map container for EngineOptionProxy objects. The container options are added as 
		engine sends *option* command as a response for *uci* command. It is recommended to call sync method
		of a parent EngineInstance object to ensure the map contains all options supported by the engine.

		Use .contains() method to check if the engine has option identifiable by given id.
		Use [] operator to access particular option by its string id.
		Use begin() and end() methods to iterate over available proxies.
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

		bool contains(const std::string& id) const {
			return proxies.count(id);
		}

		iterator begin() { return iterator(proxies.begin()); }
		iterator end() { return iterator(proxies.end()); }

		EngineOptionProxy& operator[](const std::string & id) {
			return get(id);
		}

		const EngineOptionProxy& operator[](const std::string & id) const {
			return get(id);
		}

	};

	/*!
		This class is a top level interface for interacting with a chess engine.
		To construct instance of this class use build method of a EngineInstanceBuilder. If you
		are using features from StandardChess.h you can create instances of the engine using 
		StandardChess::ChessEngineInstanceBuilder->build(...)

		Once the instance is created, it is recommended to call 'sync' method of the freshly created engine in 
		order to make sure engine options, name and author are correctly detected before being used.

	*/
	template <class Move>
	class EngineInstance : public UCILoader::EventEmitter {
		std::shared_ptr<SearchConnection<Move>> currentConnection = nullptr;
		std::shared_ptr<ProcessWrapper> processWrapper;

		std::unique_ptr<Logger> logger;

		bool receivedReadyOk = false;

		std::string name = "<empty>";
		std::string author = "<empty>";

		std::condition_variable conditional_var;
		std::mutex lock;

		void sendToEngine(const std::string& msg);

		void quit();
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
			processWrapper(engineProcess), options(engineProcess->getWriter()), logger(std::move(logger)) {
			std::shared_ptr<AbstractEngineHandler<Move>> handler = std::static_pointer_cast<AbstractEngineHandler<Move>>(std::make_shared<EngineInstance<Move>::_CommandHandler>(this));
			auto parser = std::make_shared<UCIParser<Move>>(handler, moveMarshaler, moveValidator);
			engineProcess->listen([parser, this](std::string line) {
				parser->parseLine(line);
				line.push_back('\n'); // append newline character to line to style logger entry 
				this->logger->log(Logger::FromEngine, line);
			});
			sendToEngine("uci\n");
		};
		
		~EngineInstance() {
			quit();
		}

		/*!
			This exception will be thrown every time the _sync_ command or _ping_ command timeouts.
			If that happens, the engine process will be terminated along the listener thread associated with given engine instance.
		*/
		class TimeoutException : public std::exception {};

		/*!
			This method will be thrown if user tries to start a new search while there is already an ongoing search.
			If you *realy* want to start a new search you can getCurrentSearch() to obtain current search. Then call
			its stop() method and then follow it by waitFor() call. 
		*/
		class EngineBusyException : public std::exception{};

		EngineOptionsMap options;

		/*!
			Sends 'readyok' token for the engine and waits for the 'isready' response. If the
			the engine fails to do so in 'timeout' milliseconds, it will get killed and TimeoutException will be thrown
		*/
		void sync(std::chrono::milliseconds timeout = std::chrono::milliseconds(10000));
		
		/*!
			Sends "ucinewgame" command to the engine, presumably allowing the engine to clear its transposition table 
			before starting future searches.
		*/
		void ucinewgame();

		//! wrapper around sync command that measures time to response
		std::chrono::milliseconds ping(std::chrono::milliseconds timeout = std::chrono::milliseconds(10000));
		std::shared_ptr<SearchConnection<Move>> getCurrentSearch();
		
		/*!
			Starts a new search with the specified parameters. If the engine is already busy with another search request,
			the EngineBusyException will be thrown. 

			The _params_ parameter determines parameters of the *go* command send to a engine. Use GoParamsBuilder class 
			from UCI.h to build the instance of this class with the required parameters.

			The _pos_ parameter determines root position of the *position* command. It must implement PositionFormatter interface
			from the UCI.h header. If you are using StandardChess.h you can use pass StartPos() as a value if you want to pass a 
			starting position or FenPos if you want to pass custom position.

			The _moves_ parameter tells what moves engine must play BEFORE reaching the ACTUAL SEARCH POSITION. For example setting
			pos to StartPos() and moves to this sequence e2e4 e7e5 g1f3 b8c6 f1c4 f8c5 the actual position the engine will be analysing
			is r1bqk1nr/pppp1ppp/2n5/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4 
		*/
		std::shared_ptr<SearchConnection<Move>> search(const GoParams<Move>& params, const PositionFormatter& pos,
			const std::vector<Move> moves = {});
		
		/*!
			Return name of the engine declared by *id name* command. If the engine didn't send this command default value "*<empty>*""
			will be returned. It is recommended to query this method only after calling sync() method for the first time.  
		*/
		std::string getName();

		/*!
			Return name of the engine's author declared by *id author* command. If the engine didn't send this command default 
			value "*<empty>*"" will be returned. It is recommended to query this method only after calling sync() method for the first time.  
		*/
		std::string getAuthor();

		/*!
			Performs the health check of underlying engine process. If this function ever returns false, it means the underlying
			process was already killed and the current engine instance is in unstable state.
		*/
		bool healthCheck();
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
	inline bool  EngineInstance<Move>::healthCheck() {
		bool isHealthy = processWrapper->healthCheck();
		auto event = NamedEngineEvents::makeEngineCrashedEvent();
		if (!isHealthy)
			emit(&event);
		return isHealthy;
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
		A factory class for the EngineInstance objects. It inserts preconfigured move validator and 
		move parser into the instances of the created objects. 

		If you are developing script/program/GUI for custom chess variant that use move format different from standard chess,
		you must provide a PatternMarcher object responsible for validating moves emitted by an engine and a 
		Marschaler object responsible for parsing moves received into valid Move objects. 
	
		If you are developing with standard chess in aim, you can use
		StandardChess::ChessEngineInstanceBuilder with is a pointer to preconfigured instance builder for standard chess variant
	*/
	template <class Move>
	class EngineInstanceBuilder {
		std::shared_ptr<PatternMatcher> moveValidator;
		std::shared_ptr<Marschaler<Move>> moveMarshaler;
	public:
		EngineInstanceBuilder(std::shared_ptr<PatternMatcher> validator, std::shared_ptr<Marschaler<Move>> marschaler) :
			moveValidator(validator), moveMarshaler(marschaler) {};
		
		/*!
			This method builds EngineInstance based on a given engine process. 
			The newly constructed EngineInstance object takes ownership over given process, so you user doesn't need 
			to free it later. Note that the caller is nevertheless responsible for memory handling of the resulting pointer.
			
			To see how to get a pointer to ProcessWrapper refer to documentation of UCILoader::openEngineProcessFunction.
		*/
		EngineInstance<Move>* build(ProcessWrapper* engineProcess);
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
