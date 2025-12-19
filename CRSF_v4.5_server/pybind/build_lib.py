from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
import sys
import os
import setuptools

__version__ = '4.3'

# Получаем путь к корню проекта (на уровень выше pybind)
project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

class get_pybind_include(object):
    """Helper class to determine the pybind11 include path
    The purpose of this class is to postpone importing pybind11
    until it is actually installed, so that the ``get_include()``
    method can be invoked."""

    def __str__(self):
        import pybind11
        return pybind11.get_include()


# Список исходных файлов для CRSF модуля
crsf_sources = [
    'src/crsf_bindings.cpp',
    os.path.join(project_root, 'libs/crsf/CrsfSerial.cpp'),
    os.path.join(project_root, 'libs/crsf/crc8.cpp'),
    os.path.join(project_root, 'libs/SerialPort.cpp'),
    os.path.join(project_root, 'libs/rpi_hal.cpp'),
]

# Директории с заголовками
include_dirs = [
    get_pybind_include(),
    project_root,  # для config.h
    os.path.join(project_root, 'libs'),
    os.path.join(project_root, 'libs/crsf'),
    os.path.join(project_root, 'crsf'),
]

# Флаги компиляции
compile_args = ['-std=c++17', '-O2']
if sys.platform != 'win32':
    compile_args.extend(['-fPIC'])

ext_modules = [
    Extension(
        'crsf_native',
        crsf_sources,
        include_dirs=include_dirs,
        language='c++',
        extra_compile_args=compile_args,
    ),
]


setup(
    name='crsf_native',
    version=__version__,
    author='CRSF IO',
    description='CRSF Native C++ bindings for Python using pybind11',
    long_description='',
    ext_modules=ext_modules,
    setup_requires=['pybind11>=2.6.0'],
    install_requires=['pybind11>=2.6.0'],
    cmdclass={'build_ext': build_ext},
    zip_safe=False,
    python_requires='>=3.6',
)
