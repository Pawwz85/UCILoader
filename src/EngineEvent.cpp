#include <UCILoader/EngineEvent.h>

void UCILoader::EventReceiver::unlinkAll(){
	for (auto& source : publishers) {
			source->unlink(this);
	}

	publishers.clear();
}

void UCILoader::FunctionCallbackEventReceiver::receiveEvent(const EngineEvent* e) {
	clb(e);
}

UCILoader::EventEmitter::~EventEmitter()
{
	NoPayloadEvent emitterDestroyed(NamedEngineEvents::EmitterDestroyed);
	emit(&emitterDestroyed);

	// remove a reference to this emitter from all receivers
	for (auto& receiver : receivers) 
		receiver->publishers.remove(this);
}

void UCILoader::EventEmitter::emit(const EngineEvent* event)
{
	if (event == nullptr) return;

	std::lock_guard<std::mutex> guard(lock);

	for (auto& receiver : receivers)
		if(receiver->eventFilter() & event->getType())
			receiver->receiveEvent(event);
}

void UCILoader::EventEmitter::unlink(const EventReceiver* receiver)
{

	auto it = receivers.begin();

	std::lock_guard<std::mutex> guard(lock);

	while (it != receivers.end()) {
		if (it->get() == receiver) {
			receivers.erase(it);
			break;
		}
		else
			++it;
	};

}

int UCILoader::EventEmitter::countActiveReceivers() const {
	std::lock_guard<std::mutex> guard(lock);
	return receivers.size();
}

void UCILoader::EventEmitter::connect(std::shared_ptr<EventReceiver> receiver)
{
	std::lock_guard<std::mutex> guard(lock);
	receivers.push_back(receiver);
	receiver->publishers.push_back(this);

	NoPayloadEvent connectionEvent(NamedEngineEvents::EmitterConnected);
	if (receiver->eventFilter() & connectionEvent.getType()) 
		receiver->receiveEvent(&connectionEvent);
	
}

void UCILoader::EventEmitter::connect(std::function<void(const EngineEvent*)> callback, uint32_t eventFilter)
{
	auto receiver = std::make_shared<FunctionCallbackEventReceiver>(callback, eventFilter);
	connect(std::static_pointer_cast<EventReceiver>(receiver));
}

void UCILoader::EventEmitter::connect(std::function<void()> callback, uint32_t eventFilter)
{
	auto receiver = std::make_shared<FunctionCallbackEventReceiver>(callback, eventFilter);
	connect(std::static_pointer_cast<EventReceiver>(receiver));
}
