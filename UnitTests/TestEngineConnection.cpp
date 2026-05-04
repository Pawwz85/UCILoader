#include "pch.h"

#include <UCILoader/EngineConnection.h>

using namespace UCILoader;

TEST(SearchStatusWrapper, AssigmentOperator) {
	SearchStatusWrapper wrapper;
	wrapper = ResultReady;

	ASSERT_EQ(wrapper.get(), ResultReady);
	ASSERT_TRUE(wrapper == ResultReady);
}

TEST(SearchStatusWrapper, TimedOut) {
	SearchStatusWrapper wrapper;
	wrapper.set(OnGoing);
	wrapper.waitFor(std::chrono::milliseconds(10));
	ASSERT_EQ(wrapper.get(), TimedOut);
}


TEST(SearchStatusWrapper, StatusChangedByOtherThreadDuringWaiting) {
	SearchStatusWrapper wrapper;
	SearchStatusWrapper* ptr = &wrapper;
	ptr->set(OnGoing);
	std::thread worker([ptr]() {
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
		 ptr->set(Terminated);
		});

	auto start_ = std::chrono::steady_clock::now();
	auto statusCode = ptr->waitFor(std::chrono::milliseconds(50));
	auto end_ = std::chrono::steady_clock::now();

	ASSERT_EQ(statusCode, Terminated);
	ASSERT_LE(end_ - start_, std::chrono::milliseconds(50));
	worker.join();
}


TEST(SearchStatusWrapper, StatusChangedByOtherThreadBeforeWaiting) {
	SearchStatusWrapper wrapper;
	SearchStatusWrapper* ptr = &wrapper;
	ptr->set(OnGoing);

	std::thread worker([ptr]() {
		ptr->set(Terminated);
		});

	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	auto start_ = std::chrono::steady_clock::now();
	auto statusCode = ptr->waitFor(std::chrono::milliseconds(50));
	auto end_ = std::chrono::steady_clock::now();

	ASSERT_EQ(statusCode, Terminated);
	ASSERT_LE(end_ - start_, std::chrono::milliseconds(50));
	worker.join();
}

class MockedPipeWriter: public AbstractPipeWriter {
	std::string content;
	std::mutex lock;
public:

	std::string getContent() {
		std::lock_guard<std::mutex> g(lock);
		return content;
	}

	// Odziedziczono za po�rednictwem elementu AbstractPipeWriter
	void write(const char* buffer, size_t buffer_size) override
	{
		std::lock_guard<std::mutex> g(lock);
		content.insert(content.end(), buffer, buffer + buffer_size);
	}
	bool isOpen() const override
	{
		return true;
	}
};

std::shared_ptr<EngineOptionProxy> makeProxy(const Option & defaultValue, std::shared_ptr<AbstractPipeWriter> writer) {
	return std::make_shared<EngineOptionProxy>(defaultValue, writer);
}

TEST(OptionProxy, clickButton) {
	Option clickOption("clear hash");
	auto writer = std::make_shared<MockedPipeWriter>();
	auto proxy = makeProxy(clickOption, writer);

	proxy->click();
	ASSERT_EQ("setoption name clear hash\n", writer->getContent());
}

TEST(OptionProxy, stringOption1) {
	Option option("var", "?");
	auto writer = std::make_shared<MockedPipeWriter>();
	auto proxy = makeProxy(option, writer);

	*proxy = "test";
	
	ASSERT_EQ("setoption name var value test\n", writer->getContent());
}

TEST(OptionProxy, checkOption1) {
	Option option("var", true);
	auto writer = std::make_shared<MockedPipeWriter>();
	auto proxy = makeProxy(option, writer);

	*proxy = false;

	ASSERT_EQ("setoption name var value false\n", writer->getContent());
}

TEST(OptionProxy, checkOption2) {
	Option option("var", false);
	auto writer = std::make_shared<MockedPipeWriter>();
	auto proxy = makeProxy(option, writer);

	*proxy = true;

	ASSERT_EQ("setoption name var value true\n", writer->getContent());
}

TEST(OptionProxy, spinOption1) {
	Option option("var", spin_option({ 1, 1, 10 }));
	auto writer = std::make_shared<MockedPipeWriter>();
	auto proxy = makeProxy(option, writer);

	*proxy = 7;

	ASSERT_EQ("setoption name var value 7\n", writer->getContent());
}

TEST(OptionProxy, spinOption2) {
	Option option("var", spin_option({ 1, 1, 10 }));
	auto writer = std::make_shared<MockedPipeWriter>();
	auto proxy = makeProxy(option, writer);

	*proxy = "7";

	ASSERT_EQ("setoption name var value 7\n", writer->getContent());
}

TEST(OptionProxy, spinOption3) {
	Option option("var", spin_option({ 1, -10, 10 }));
	auto writer = std::make_shared<MockedPipeWriter>();
	auto proxy = makeProxy(option, writer);

	*proxy = -5;

	ASSERT_EQ("setoption name var value -5\n", writer->getContent());
}

TEST(OptionProxy, spinOption_parsingError) {
	Option option("var", spin_option({ 1, 1, 10 }));
	auto writer = std::make_shared<MockedPipeWriter>();
	auto proxy = makeProxy(option, writer);

	ASSERT_THROW(*proxy = "seven", EngineOptionProxy::ParsingError);

}

TEST(OptionProxy, spinOption_outOfRange) {
	Option option("var", spin_option({ 1, 1, 10 }));
	auto writer = std::make_shared<MockedPipeWriter>();
	auto proxy = makeProxy(option, writer);

	ASSERT_THROW(*proxy = 125, EngineOptionProxy::NotSupportedValueException);
}

TEST(OptionProxy, integerGivenForNoSpinOption) {
	Option option("var", "?");
	auto writer = std::make_shared<MockedPipeWriter>();
	auto proxy = makeProxy(option, writer);

	ASSERT_THROW(*proxy = 5, EngineOptionProxy::WrongTypeError);
}

TEST(OptionProxy, comboOption) {
	Option option("var", combo_option({ "normal", {"aggressive", "normal", "dynamic"} }));
	auto writer = std::make_shared<MockedPipeWriter>();
	auto proxy = makeProxy(option, writer);

	*proxy = "dynamic";
	ASSERT_EQ("setoption name var value dynamic\n", writer->getContent());

}

TEST(OptionProxy, comboOption_outOfRange) {
	
	Option option("var", combo_option({ "normal", {"aggressive", "normal", "dynamic"} }));
	auto writer = std::make_shared<MockedPipeWriter>();
	auto proxy = makeProxy(option, writer);

	ASSERT_THROW(*proxy = "static", EngineOptionProxy::NotSupportedValueException);
}

TEST(OptionProxy, stringOption_empty_string) {
	Option option("var", "?");
	auto writer = std::make_shared<MockedPipeWriter>();
	auto proxy = makeProxy(option, writer);

	*proxy = "";
	ASSERT_EQ("setoption name var value <empty>\n", writer->getContent());
}

TEST(OptionProxy, getCheckValue) {
	Option option1("var", false), option2("var2", true);
	auto writer = std::make_shared<MockedPipeWriter>();
	auto proxy = makeProxy(option1, writer);
	auto proxy2 = makeProxy(option2, writer);

	bool var_1_bool = *proxy;
	bool var_2_bool = *proxy2;

	ASSERT_FALSE(var_1_bool);
	ASSERT_TRUE(var_2_bool);

	std::string var_1_str = *proxy;
	std::string var_2_str = *proxy2;

	ASSERT_EQ(var_1_str, "off");
	ASSERT_EQ(var_2_str, "on");

	ASSERT_THROW((int)*proxy, EngineOptionProxy::WrongTypeError);
}

TEST(OptionProxy, getSpinValue) {
	Option option("var", spin_option({ 7, 1, 10 }));
	auto writer = std::make_shared<MockedPipeWriter>();
	auto proxy = makeProxy(option, writer);

	int32_t var_int = *proxy;
	std::string var_str = *proxy;
	
	ASSERT_EQ(var_int, 7);
	ASSERT_EQ(var_str, "7");


	ASSERT_THROW((bool)*proxy, EngineOptionProxy::WrongTypeError);
}


TEST(OptionProxy, getStringValue) {
	Option option1("var", ""), option2("var2", "test");
	auto writer = std::make_shared<MockedPipeWriter>();
	auto proxy = makeProxy(option1, writer);
	auto proxy2 = makeProxy(option2, writer);


	std::string var_1_str = *proxy;
	std::string var_2_str = *proxy2;

	ASSERT_EQ(var_1_str, "");
	ASSERT_EQ(var_2_str, "test");

	ASSERT_THROW((int)*proxy, EngineOptionProxy::WrongTypeError);
	ASSERT_THROW((bool)*proxy, EngineOptionProxy::WrongTypeError);
}

TEST(OptionProxy, getComboValue) {
	Option option("var", combo_option({ "normal", {"aggressive", "normal", "dynamic"} }));
	auto writer = std::make_shared<MockedPipeWriter>();
	auto proxy = makeProxy(option, writer);

	std::string var_str = *proxy;

	ASSERT_EQ(var_str, "normal");

	ASSERT_THROW((bool)*proxy, EngineOptionProxy::WrongTypeError);
	ASSERT_THROW((int)*proxy, EngineOptionProxy::WrongTypeError);
}

TEST(OptionProxy, getButtonValue) {
	Option option("clear hash");
	auto writer = std::make_shared<MockedPipeWriter>();
	auto proxy = makeProxy(option, writer);

	ASSERT_THROW((bool)*proxy, EngineOptionProxy::WrongTypeError);
	ASSERT_THROW((int)*proxy, EngineOptionProxy::WrongTypeError);
	ASSERT_THROW((std::string)*proxy, EngineOptionProxy::WrongTypeError);
}

TEST(OptionProxy, setCheckValueUsingOnString) {
	Option option("var", false);
	auto writer = std::make_shared<MockedPipeWriter>();
	auto proxy = makeProxy(option, writer);

	*proxy = "on";
	ASSERT_TRUE((bool)*proxy);
	*proxy = "off";
	ASSERT_FALSE((bool)*proxy);
	*proxy = "true";
	ASSERT_TRUE((bool)*proxy);
	*proxy = "false";
	ASSERT_FALSE((bool)*proxy);
}

TEST(OptionProxy, setCheckValueUsingOnString_caseInSensitive) {
	Option option("var", false);
	auto writer = std::make_shared<MockedPipeWriter>();
	auto proxy = makeProxy(option, writer);

	*proxy = "oN";
	ASSERT_TRUE((bool)*proxy);
	*proxy = "oFf";
	ASSERT_FALSE((bool)*proxy);
	*proxy = "tRuE";
	ASSERT_TRUE((bool)*proxy);
	*proxy = "fAlsE";
	ASSERT_FALSE((bool)*proxy);
}

TEST(OptionProxy, setCheckValueUsingOnString_notSupportedValue) {
	Option option("var", false);
	auto writer = std::make_shared<MockedPipeWriter>();
	auto proxy = makeProxy(option, writer);

	ASSERT_THROW(*proxy = "yes", EngineOptionProxy::NotSupportedValueException);
}