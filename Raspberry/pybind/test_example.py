#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Пример использования C++ библиотеки из Python
"""

import example_lib

def main():
    print("=" * 60)
    print("Тестирование C++ библиотеки из Python")
    print("=" * 60)
    
    # 1) Тест сложения чисел
    print("\n1. Сложение чисел:")
    a, b = 10.5, 20.3
    result = example_lib.add_numbers(a, b)
    print(f"   add_numbers({a}, {b}) = {result}")
    
    # 2) Тест увеличения элементов массива
    print("\n2. Увеличение элементов массива на 1:")
    arr = [1, 2, 3, 4, 5]
    print(f"   Исходный массив: {arr}")
    result = example_lib.increment_array(arr)
    print(f"   Результат: {result}")
    
    # 3) Тест структуры Point2D
    print("\n3. Работа со структурой Point2D:")
    
    # Создание точки
    point = example_lib.Point2D(10.0, 20.0)
    print(f"   Создана точка: {point}")
    print(f"   x = {point.x}, y = {point.y}")
    
    # Смещение точки методом
    point.shift(5.0, 3.0)
    print(f"   После shift(5.0, 3.0): {point}")
    
    # Использование функции shift_point
    point2 = example_lib.Point2D(100.0, 200.0)
    print(f"\n   Создана точка2: {point2}")
    point2_shifted = example_lib.shift_point(point2)
    print(f"   После shift_point (смещение на (1,1)): {point2_shifted}")
    print(f"   Оригинальная точка2 осталась: {point2}")
    
    # Изменение координат напрямую
    point3 = example_lib.Point2D()
    print(f"\n   Создана точка3 с параметрами по умолчанию: {point3}")
    point3.x = 50.0
    point3.y = 75.0
    print(f"   После изменения координат: {point3}")
    
    print("\n" + "=" * 60)
    print("Все тесты выполнены успешно!")
    print("=" * 60)


if __name__ == "__main__":
    main()

