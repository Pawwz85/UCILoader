#pragma once

#include "EngineLoader.h"
#include "Parser.h"
#include "EngineEvent.h"
#include <mutex>
#include <unordered_map>
#include <condition_variable>

namespace UCILoader {
	enum SearchStatusCode {
		InitialisingSearch,	// no search command was send to engine yet
		OnGoing,	// engine is actively searching best move now
		Terminated,	// engine was killed or crashed before result was outputted
		TimedOut,  // search timed out
		Stopped,	// stop signal was send, but no best was not received yet
		ResultReady 
	};

	template<class Move>
	struct SearchResult {
		Move* bestMove;
		Move* ponderMove;
	};

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

	class SearchStatusWrapper {
		std::mutex statusLock;
		SearchStatusCode status =InitialisingSearch;
		std::condition_variable var;
	public:
		void set(SearchStatusCode code) {
			std::unique_lock<std::mutex> guard(statusLock);
			this->status = code;
			var.notify_all();
		}

		SearchStatusCode swapIfOngoing(SearchStatusCode newStatus) {
			std::unique_lock<std::mutex> guard(statusLock);

			SearchStatusCode oldStatus = status;

			if (status == OnGoing) {
				status = newStatus;
			}

			return oldStatus;
		}

		SearchStatusCode get() {
			std::unique_lock<std::mutex> guard(statusLock);
			return status;
		}

		SearchStatusCode waitFor(const std::chrono::milliseconds& dur) {

			std::unique_lock<std::mutex> guard(statusLock);


			if (status != OnGoing) 
				return status;
			

			var.wait_for(guard, dur);
			
			if (status == OnGoing) 
				status = TimedOut;
			
			return status;
		}

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

	template<class Move>
	class SearchConnection {
	public:
		class ResultNotReadyException : public std::exception {};
	private:

		friend EngineInstance<Move>;

		std::mutex lock;
		SearchResult<Move> result = { nullptr, nullptr };
		SearchStatusWrapper status;
		

		std::shared_ptr<EngineProcessWrapper> engine;

		void receiveBestMoveSignal(const Move* bestMove, const Move* ponderMove);
		SearchStatusCode getStatusNoLock();
	public:
		~SearchConnection();
		SearchConnection(std::shared_ptr<EngineProcessWrapper> engine) : engine(engine) {};

		SearchStatusCode getStatus();
		const SearchResult<Move> & getResult();
		void stop();
		void waitFor(const std::chrono::milliseconds& time);
		void timeOutIfNotFinished(); 
		void setInfoHandler(std::function<std::vector<Info<Move>>> handler);
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
	inline void SearchConnection<Move>::waitFor(const std::chrono::milliseconds& time)
	{
		SearchStatusCode returnCode = status.waitFor(time);
	}

	template<class Move>
	inline void SearchConnection<Move>::timeOutIfNotFinished()
	{
		SearchStatusCode returnCode = status.swapIfOngoing(TimedOut); 
	}


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

		class WrongTypeError : public std::exception {};
		class NotSupportedValueException : public std::exception {};
		class ParsingError : public std::exception {};

		EngineOptionProxy() : value(), writer(nullptr) {};
		EngineOptionProxy(const Option& option, std::shared_ptr<AbstractPipeWriter> pipeWriter) : value(option), writer(pipeWriter) {};
		EngineOptionProxy(const EngineOptionProxy& other) : value(other.value), writer(other.writer) {};

		inline OptionType type() const { return value.type(); };
		inline const std::string& id() const { return value.id(); };
		inline const Option& getAsOption() const { return value; };

		void click(); // button type only

		const std::string& operator = (const std::string& value);
		const char* operator = (const char* value);
		const int32_t& operator= (const int32_t& number); // spin type only
		const bool& operator= (const bool& value); // check type only

		operator bool();
		operator int();
		operator std::string();
	};

	/*
		The behavior of inserting a new options inside options map should be internal for EngineInstance. Therefore, EngineOptionsMap will
		inherit this interface privately to attempt hide that method from the user.
	*/
	class IOptionConsumer {
	public:
		virtual void consume(const Option& o) = 0;
	};

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

		EngineOptionProxy& operator[](const std::string id) {
			return get(id);
		}

		const EngineOptionProxy& operator[](const std::string id) const {
			return get(id);
		}

	};

	template <class Move>
	class EngineInstance : public UCILoader::EventEmitter {
		std::shared_ptr<SearchConnection<Move>> currentConnection = nullptr;
		std::shared_ptr<EngineProcessWrapper> processWrapper;

		bool receivedReadyOk = false;

		std::string name = "<empty>";
		std::string author = "<empty>";

		std::condition_variable conditional_var;
		std::mutex lock;


		void sendToEngine(const std::string& msg);
	public:
		
		class _CommandHandler : public AbstractEngineHandler<Move> {
			EngineInstance<Move>* parent;
		public:
			_CommandHandler(EngineInstance<Move>* parent) : parent(parent) {};
			// Odziedziczono za poœrednictwem elementu AbstractEngineHandler
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

		EngineInstance(std::shared_ptr<EngineProcessWrapper> engineProcess, std::shared_ptr<Marschaler<Move>> moveMarshaler, std::shared_ptr<PatternMatcher> moveValidator) :
			processWrapper(engineProcess), options(engineProcess->getWriter()) {
			std::shared_ptr<AbstractEngineHandler<Move>> handler = std::static_pointer_cast<AbstractEngineHandler<Move>>(std::make_shared<EngineInstance<Move>::_CommandHandler>(this));
			auto parser = std::make_shared<UCIParser<Move>>(handler, moveMarshaler, moveValidator);
			engineProcess->listen([parser](std::string line) {
				parser->parseLine(line);
			});
			sendToEngine("uci\n");
		};

		class TimeoutException : public std::exception {};
		class EngineBusyException : public std::exception{};

		EngineOptionsMap options;

		/*
			Sends 'readyok' token for the engine and waits for the 'isready' response. If the
			the engine fails to do so in 'timeout' milliseconds, it will get killed and TimeoutException will be thrown
		*/
		void sync(std::chrono::milliseconds timeout = std::chrono::milliseconds(10000));
		
		// wrapper around sync command that measures time to response
		std::chrono::milliseconds ping(std::chrono::milliseconds timeout = std::chrono::milliseconds(10000));
		std::shared_ptr<SearchConnection<Move>> getCurrentSearch();
		std::shared_ptr<SearchConnection<Move>> search(const GoParams<Move>& params, const PositionFormatter& pos,
			const std::vector<Move> moves = {});

		std::string getName();
		std::string getAuthor();

		void quit();
	};

	template<class Move>
	inline void EngineInstance<Move>::sendToEngine(const std::string& msg)
	{
		processWrapper->getWriter()->write(msg.c_str(), msg.size());
	}

	template<class Move>
	inline void EngineInstance<Move>::sync(std::chrono::milliseconds timeout)
	{
		std::unique_lock<std::mutex> guard(lock);
		receivedReadyOk = false;
		sendToEngine("isready\n");
		conditional_var.wait_for(guard, timeout);
		if (!receivedReadyOk) {
			processWrapper->kill();
			throw TimeoutException();
		}
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
		sendToEngine("quit\n");

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
		if (parent->currentConnection != nullptr) {
			parent->currentConnection->receiveBestMoveSignal(&bestMove, nullptr);
			parent->currentConnection = nullptr;
		}
	}

	template<class Move>
	inline void EngineInstance<Move>::_CommandHandler::onBestMove(const Move& bestMove, const Move& ponderMove)
	{
		std::unique_lock<std::mutex> guard(parent->lock);
		if (parent->currentConnection != nullptr) {
			parent->currentConnection->receiveBestMoveSignal(&bestMove, &ponderMove);
			parent->currentConnection = nullptr;
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
	}

	template <class Move>
	class EngineInstanceBuilder {
		std::shared_ptr<PatternMatcher> moveValidator;
		std::shared_ptr<Marschaler<Move>> moveMarshaler;
	public:
		EngineInstanceBuilder(std::shared_ptr<PatternMatcher> validator, std::shared_ptr<Marschaler<Move>> marschaler) :
			moveValidator(validator), moveMarshaler(marschaler) {};
		EngineInstance<Move>* build(EngineProcessWrapper* engineProcess);
	};

	template<class Move>
	inline EngineInstance<Move>* EngineInstanceBuilder<Move>::build(EngineProcessWrapper* engineProcess)
	{
		std::shared_ptr<EngineProcessWrapper> proces(engineProcess);
		return new EngineInstance<Move>(proces, moveMarshaler, moveValidator);
	}

}
