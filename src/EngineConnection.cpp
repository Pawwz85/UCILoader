#include <UCILoader/EngineConnection.h>
#include <sstream> 
#include <algorithm> // for std::transform

void UCILoader::EngineOptionProxy::tryWrite(const std::string& value)
{
	if (writer && writer->isOpen()) 
		writer->write(value.c_str(), value.size());
}
void UCILoader::EngineOptionProxy::tryWrite(const char* text)
{
	if (writer && writer->isOpen())
		writer->write(text, strlen(text));
}

void UCILoader::EngineOptionProxy::validateSpinCandidate(const int32_t& value) const
{
	int32_t min_ = this->value.spin_content().min;
	int32_t max_ = this->value.spin_content().max;

	if (value < min_ || value > max_)
		throw NotSupportedValueException();
}

void UCILoader::EngineOptionProxy::validateComboCandidate(const std::string& value) const
{
	for (const std::string& val : this->value.combo_content().supported_values)
		if (val == value)
			return;

	throw NotSupportedValueException();
}

void UCILoader::EngineOptionProxy::parse(const std::string& s)
{
	int32_t tmp;
	std::string str;
	/*
		Currently we can not set check option by text.
	*/

	switch (type())
	{
	case String:
		value = Option(id(), s);
		break;
	case Spin:
		tmp = parseInteger(s);
		validateSpinCandidate(tmp);
		value.spin_content().value = tmp;
		break;
	case Combo:
		validateComboCandidate(s);
		value.combo_content().value = s;
		break;
	case Check:
		str = s;
		std::transform(s.begin(), s.end(), str.begin(), [](unsigned char c){return std::tolower(c);});
		if (str == "on" || str == "true")
			value.check_content() = true;
		else if (str == "off" || str == "false")
			value.check_content() = false;
		else
		throw NotSupportedValueException();
		break;
	default:
		throw WrongTypeError();
	}

	tryWrite(formatSetOptionCommand(value));
}

int32_t UCILoader::EngineOptionProxy::parseInteger(const std::string& value)
{
	int32_t result;

	if ((std::stringstream(value) >> result).fail())
		throw ParsingError();

	return result;
}

void UCILoader::EngineOptionProxy::assertType(const OptionType& type) const
{
	if (type != this->type()) throw WrongTypeError();
}

void UCILoader::EngineOptionProxy::click()
{
	assertType(Button);
	tryWrite(formatSetOptionCommand(value));
}

const std::string& UCILoader::EngineOptionProxy::operator=(const std::string & s) {
	parse(s);
	return s;
}

const char* UCILoader::EngineOptionProxy::operator=(const char* value)
{
	parse(value);
	return value;
}

const int32_t& UCILoader::EngineOptionProxy::operator=(const int32_t& number)
{
	assertType(Spin);
	validateSpinCandidate(number);
	value.spin_content().value = number;
	tryWrite(formatSetOptionCommand(value));
	return number;
}

const bool& UCILoader::EngineOptionProxy::operator=(const bool& value)
{
	assertType(Check);
	this->value.check_content() = value;
	tryWrite(formatSetOptionCommand(this->value));
	return value;
}

UCILoader::EngineOptionProxy::operator bool()
{
	if (type() != Check)
		throw WrongTypeError();

	return value.check_content();
}

UCILoader::EngineOptionProxy::operator int()
{
	if (type() != Spin)
		throw WrongTypeError();

	return value.spin_content().value;
}

UCILoader::EngineOptionProxy::operator std::string()
{
	switch (type()) {
	case String:
		return value.str_content();
	case Spin:
		return std::to_string(value.spin_content().value);
	case Combo:
		return value.combo_content().value;
	case Check:
		return value.check_content() ? "on" : "off";
	default:
		throw WrongTypeError();
	}

}
