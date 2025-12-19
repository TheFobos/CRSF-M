#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <vector>

namespace py = pybind11;

// 1) Функция сложения двух чисел
double add_numbers(double a, double b) {
    return a + b;
}

// 2) Функция для увеличения каждого элемента массива на 1
std::vector<int> increment_array(std::vector<int> arr) {
    for (int i = 0; i < arr.size(); i++) {
        arr[i] += 1;
    }
    return arr;
}

// 3) Структура Point2D с координатами x, y
struct Point2D {
    double x;
    double y;
    
    Point2D(double x = 0.0, double y = 0.0) : x(x), y(y) {}
    
    // Метод для смещения точки
    void shift(double dx, double dy) {
        x += dx;
        y += dy;
    }
    
    // Для удобного вывода
    std::string to_string() const {
        return "Point2D(x=" + std::to_string(x) + ", y=" + std::to_string(y) + ")";
    }
};

// Функция для смещения точки на (1, 1)
Point2D shift_point(Point2D point) {
    point.shift(1.0, 1.0);
    return point;
}

// Модуль pybind11
PYBIND11_MODULE(example_lib, m) {
    m.doc() = "Example C++ library for Python using pybind11";
    
    // Экспорт функции сложения
    m.def("add_numbers", &add_numbers, 
          "Add two numbers together",
          py::arg("a"), py::arg("b"));
    
    // Экспорт функции для массива
    m.def("increment_array", &increment_array,
          "Increment each element of array by 1",
          py::arg("arr"));
    
    // Экспорт структуры Point2D
    py::class_<Point2D>(m, "Point2D")
        .def(py::init<double, double>(), 
             py::arg("x") = 0.0, py::arg("y") = 0.0)
        .def_readwrite("x", &Point2D::x)
        .def_readwrite("y", &Point2D::y)
        .def("shift", &Point2D::shift,
             "Shift point by dx and dy",
             py::arg("dx"), py::arg("dy"))
        .def("to_string", &Point2D::to_string)
        .def("__repr__", &Point2D::to_string);
    
    // Экспорт функции смещения точки
    m.def("shift_point", &shift_point,
          "Shift point by (1, 1)",
          py::arg("point"));
}

