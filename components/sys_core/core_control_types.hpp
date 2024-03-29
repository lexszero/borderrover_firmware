#pragma once

#include "AbstractControl.hpp"

namespace Core {

using nlohmann::json;

template <typename Type>
class GenericControl : public AbstractControl {
public:
	using Setter = std::function<void(const Type&)>;

	explicit GenericControl(const std::string& _name, const char *_desc, int _order, const Type& _defval, Setter _setter = nullptr) :
		AbstractControl(
				_name,
				_desc,
				!_setter,
				_order),
		value(_defval),
		setter(_setter)
	{}

	Type& operator=(const Type& newval) {
		set(newval, true, true);
		return value;
	}

	GenericControl<Type>& operator=(const GenericControl<Type>& src) {
		set(src.value, true, true);
		return *this;
	}

	operator Type&() {
		return value;
	}

	const Type& get() const {
		return value;
	}

	virtual std::string show() override {
		return to_string();
	}

	virtual void set(const Type& newval, bool publish = true, bool call = true) {
		if (call && setter)
			setter(newval);
		value = newval;
		if (publish) {
		//	mqttPublish();
		}
	}
protected:
	Type value;
	Setter setter;
};

template <typename Type>
class Control : public GenericControl<Type> {
public:
	using GenericControl<Type>::GenericControl;
	using GenericControl<Type>::operator=;
	using GenericControl<Type>::operator Type&;
	using GenericControl<Type>::set;

	virtual std::string to_string() override {
		using std::to_string;
		return to_string(this->value);
	}

	virtual std::string show() override {
		return to_string();
	}

	virtual void append_json(json& j) override {
		j[GenericControl<Type>::name] = this->value;
	}
};

class ControlSwitch : public Control<bool> {
	using Control<bool>::Control;
/*
	void mqttMeta() {
		AbstractControl::mqttMetaDefault("switch");
	}
*/

	void from_string(const std::string& newval) override {
		Control<bool>::set(newval == "1" ? true : false);
	}

	virtual std::string show() override {
		return "sw:" + std::string(this->value ? "\e[1;32mON\e[0m" : "\e[1;31mOFF\e[0m");
	}

};

class ControlPushButton : public Control<bool> {
public:
	explicit ControlPushButton(const std::string& _name, const char *_desc, int _order, Setter _setter = nullptr) :
		Control(_name, _desc, _order, false, _setter)
	{}

	/*
	void mqttMeta() {
		AbstractControl::mqttMetaDefault("pushbutton");
	}
	*/

	void from_string(const std::string& newval) override {
		Control<bool>::set(std::stoi(newval), false);
	}

	virtual std::string show() override {
		return "btn:" + std::string(this->value ? "\e[1;32mON\e[0m" : "\e[1;31mOFF\e[0m");
	}
};

template <typename Type, Type Max>
class ControlRange : public Control<Type> {
public:
	using Control<Type>::Control;
	using Control<Type>::operator=;

	/*
	void mqttMeta() {
		AbstractControl::mqttMetaDefault("range");
		AbstractControl::mqttMetaAttr("max", String(Max));
	}
	*/

	void from_string(const std::string& newval) override {
		Control<Type>::set(std::stoi(newval));
	}
	
	std::string show() override {
		return std::to_string(this->value) + " [max: " + std::to_string(Max) + "]";
	}
};

}
