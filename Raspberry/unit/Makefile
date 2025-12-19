CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -I.. -I../libs -I../libs/crsf
LDFLAGS := -lgtest -lgtest_main -lgmock -lpthread

# Исходные файлы для тестов (старые)
TEST_SRC_OLD := \
	test_crc8.cpp \
	test_crsf_basic_operations.cpp \
	test_crsf_callbacks.cpp \
	test_crsf_channels.cpp \
	test_crsf_link.cpp \
	test_crsf_telemetry.cpp

# Исходные файлы для новых тестов (fobos_)
TEST_SRC_FOBOS := \
	test_fobos_crc8_extended.cpp \
	test_fobos_crsf_channel_encoding.cpp \
	test_fobos_crsf_packet_parsing.cpp \
	test_fobos_crsf_link_state.cpp \
	test_fobos_crsf_telemetry_parsing.cpp \
	test_fobos_crsf_packet_sending.cpp \
	test_fobos_crsf_buffer_management.cpp \
	test_fobos_crsf_error_handling.cpp

# Все исходные файлы тестов
TEST_SRC := $(TEST_SRC_OLD) $(TEST_SRC_FOBOS)

# Исходные файлы библиотеки (нужны для тестов)
LIB_SRC := \
	../libs/crsf/CrsfSerial.cpp \
	../libs/crsf/crc8.cpp \
	../libs/rpi_hal.cpp \
	../libs/SerialPort.cpp

# Объектные файлы
TEST_OBJ := $(TEST_SRC:.cpp=.o)
LIB_OBJ := $(patsubst ../libs/crsf/%.cpp,libs/crsf/%.o,$(filter ../libs/crsf/%.cpp,$(LIB_SRC))) \
           $(patsubst ../libs/%.cpp,libs/%.o,$(filter ../libs/%.cpp,$(filter-out ../libs/crsf/%.cpp,$(LIB_SRC))))

# Исполняемый файл тестов
TEST_BIN := test_runner

# Цель по умолчанию
all: $(TEST_BIN)

# Сборка тестов
$(TEST_BIN): $(TEST_OBJ) $(LIB_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Правило компиляции объектных файлов тестов
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Правило компиляции объектных файлов библиотеки
# ВАЖНО: Объектные файлы компилируются из исходников в ../libs/
# и сохраняются в unit/libs/ для изоляции тестов.
# Make автоматически пересоберет объектные файлы, если исходники новее.
# 
# Структура: ../libs/*.cpp -> unit/libs/*.o
# Это НЕ копии - это скомпилированные объектные файлы из исходников!
libs/crsf/%.o: ../libs/crsf/%.cpp
	@mkdir -p libs/crsf
	@echo "Compiling library: $< -> $@"
	$(CXX) $(CXXFLAGS) -c $< -o $@

libs/%.o: ../libs/%.cpp
	@mkdir -p libs
	@echo "Compiling library: $< -> $@"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Запуск тестов
test: $(TEST_BIN)
	./$(TEST_BIN)

# Принудительная пересборка библиотек (полезно после изменений в ../libs/)
rebuild-libs:
	@echo "Force rebuilding library objects..."
	rm -f $(LIB_OBJ)
	$(MAKE) $(LIB_OBJ)

# Очистка артефактов сборки
clean:
	rm -f $(TEST_OBJ) $(LIB_OBJ) $(TEST_BIN)
	rm -rf libs

.PHONY: all test clean rebuild-libs

