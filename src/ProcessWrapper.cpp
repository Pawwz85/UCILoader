#include <UCILoader/ProcessWrapper.h>

#include <algorithm>
#include <mutex>
#include <csignal>

std::mutex processWrapperInstancesLock;
std::vector<UCILoader::ProcessWrapper *> proccessWrapperInstances;

void UCILoader::ProcessWrapper::killAll() {
    std::lock_guard<std::mutex> guard(processWrapperInstancesLock);
        for (UCILoader::ProcessWrapper * process : proccessWrapperInstances)
            process->kill();
 }

UCILoader::ProcessWrapper::ProcessWrapper()  {
	std::lock_guard<std::mutex> guard(processWrapperInstancesLock);
    proccessWrapperInstances.push_back(this);
}

UCILoader::ProcessWrapper::~ProcessWrapper() {
			std::lock_guard<std::mutex> guard(processWrapperInstancesLock);
			auto remIt = std::remove(proccessWrapperInstances.begin(), proccessWrapperInstances.end(), this);
			proccessWrapperInstances.erase(remIt, proccessWrapperInstances.end());
}