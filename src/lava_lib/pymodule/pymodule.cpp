#include <iostream>
#include <pybind11/pybind11.h>

namespace lava {

namespace py = pybind11;

class Example {
 public:
	Example( double a ) : _a( a) {}

	Example& operator+=( const Example& other ) {
		_a += other._a;
		return *this;
  	}

 private:
	double _a;
};

class Li {
 public:
	Li() { std::cout << "Li (Lava interfac ) class contructor" << std::endl; }	
};

PYBIND11_MODULE(lava, m) {
  m.doc() = "Python bindings for lava rendering library";

  py::class_<Li>(m, "Li");

  py::class_<Example>(m, "Example")
    .def( py::init( []( double a ) {
              return new Example(a);
            }
          )
    )
    .def( "__iadd__", &Example::operator+= );
}

}  // namespace lava