#include <iostream>

extern "C" {
	double myfun(double);
	double foo(double a){
		return a+2;
	}
	double bar(double b){
		return b+5;
	}
}

int main() {
	std::cout << "myfun(4): " << myfun(4) << std::endl;
}
