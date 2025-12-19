CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic -I.
LDFLAGS := -lpthread -Wl,--export-dynamic

# Исходные файлы основного приложения
SRC := \
	main.cpp \
	globals.cpp \
	crsf/crsf.cpp \
	libs/crsf/CrsfSerial.cpp \
	libs/SerialPort.cpp \
	libs/rpi_hal.cpp \
	libs/crsf/crc8.cpp \
	libs/joystick.cpp

# Объектные файлы
OBJ := $(SRC:.cpp=.o)

# Исполняемые файлы
BIN := crsf_io_rpi
API_SERVER_BIN := crsf_api_server
API_INTERPRETER_BIN := crsf_api_interpreter

# Цель по умолчанию
all: $(BIN) $(API_SERVER_BIN) $(API_INTERPRETER_BIN)

# Сборка основного приложения
$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Сборка API сервера
$(API_SERVER_BIN): api_server.o globals.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Сборка API интерпретатора
$(API_INTERPRETER_BIN): api_interpreter.o globals.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Правило компиляции объектных файлов
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Очистка артефактов сборки
clean:
	rm -f $(OBJ) $(BIN) api_server.o api_interpreter.o globals.o $(API_SERVER_BIN) $(API_INTERPRETER_BIN)

.PHONY: all clean


