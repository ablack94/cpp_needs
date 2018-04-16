

#include <iostream>
#include <memory>
#include <exception>
#include <stdexcept>
#include <type_traits>
#include <string>
#include <tuple>
#include <map>

#include "needs.hpp"

struct IPrint {
	virtual void print() = 0;
};

struct PrintHello : public IPrint {
	PrintHello() {
		this->suffix = "";
	}
	
	PrintHello(std::string suffix) {
		this->suffix = suffix;
	}
	
	void print() override {
		std::cout << "Hello " << suffix << std::endl;
	}
protected:
	std::string suffix;
};

struct IDuck {
	virtual void quack() = 0;
};

struct StdoutDuck : public IDuck {
	void quack() override {
		std::cout << "Quack!" << std::endl;
	}
};

struct A : public needs<IPrint,IDuck> {
	void print() {
		get<IPrint>()->print();
	}

	void quack() {
		get<IDuck>()->quack();
	}
};

int main(int argc, char** argv) {

	A a;
	
	a.set<IPrint>(PrintHello("someone"));
	a.print();

	a.set<IPrint>(PrintHello("X"));
	a.print();

	PrintHello ph("World!");
	a.set<IPrint, PrintHello>(&ph);
	a.print();

	a.quack();
	
	
	return 0;
}







