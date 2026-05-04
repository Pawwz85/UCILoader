#include <UCILoader/Parser.h>

void UCILoader::Option::dispose_content() {
	switch (type_)
	{
	case Check:
		dispose_content_internal<bool>();
		break;
	case Spin:
		dispose_content_internal<spin_option>();
		break;
	case Combo:
		dispose_content_internal<combo_option>();
		break;
	case String:
		dispose_content_internal<std::string>();
		break;
		case Button:
	default:
		break;
	}
}

void* UCILoader::Option::deep_copy_content() const {
	switch (type_)
	{
	case Check:
		return new bool(check_content());
	case Spin:
		return new spin_option(spin_content());
	case Combo:
		return new combo_option(combo_content());
	case String:
		return new std::string(str_content());
	default:
		return nullptr;
	}
}

std::string UCILoader::formatSetOptionCommand(const Option& option)
{
	std::string result = std::string("setoption name ") + option.id();

	if (option.type() == Button) goto FINALE;

	result += " value ";

	switch (option.type())
	{
	case Check:
		result += option.check_content() ? "true" : "false";
		break;
	case String:
		result += option.str_content().empty() ? "<empty>" : option.str_content();
		break;
	case Combo:
		result += option.combo_content().value;
		break;
	case Spin:
		result += std::to_string(option.spin_content().value);
		break;
	default:
		break;
	}

FINALE:
	result.push_back('\n');
	return result;
}
