/**
 * @file test_runner_console.cpp
 * @brief Консольное приложение для запуска unit тестов с псевдографическим интерфейсом
 * 
 * Приложение запускает все unit тесты и выводит результаты
 * в красивом текстовом консольном интерфейсе с использованием
 * псевдографических символов.
 * 
 * @version CRSF-IO-mkII
 */

#include <gtest/gtest.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <chrono>
#include <cstdlib>

// ANSI коды цветов для терминала
namespace Colors {
    const std::string RESET = "\033[0m";
    const std::string BOLD = "\033[1m";
    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string BLUE = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN = "\033[36m";
    const std::string WHITE = "\033[37m";
    const std::string BG_GREEN = "\033[42m";
    const std::string BG_RED = "\033[41m";
    const std::string BG_YELLOW = "\033[43m";
}

// Псевдографические символы
namespace Box {
    const std::string HORIZONTAL = "─";
    const std::string VERTICAL = "│";
    const std::string TOP_LEFT = "┌";
    const std::string TOP_RIGHT = "┐";
    const std::string BOTTOM_LEFT = "└";
    const std::string BOTTOM_RIGHT = "┘";
    const std::string T_LEFT = "├";
    const std::string T_RIGHT = "┤";
    const std::string T_TOP = "┬";
    const std::string T_BOTTOM = "┴";
    const std::string CROSS = "┼";
    const std::string CHECK = "✓";
    const std::string CROSS_MARK = "✗";
}

// Структура для хранения информации о тесте
struct TestResult {
    std::string suite;
    std::string name;
    bool passed;
    std::string message;
    double duration_ms;
};

// Кастомный TestEventListener для сбора результатов
class TestResultCollector : public ::testing::EmptyTestEventListener {
public:
    std::vector<TestResult> results;
    int totalTests = 0;
    int passedTests = 0;
    int failedTests = 0;
    std::chrono::steady_clock::time_point startTime;
    
    void OnTestProgramStart(const ::testing::UnitTest&) override {
        startTime = std::chrono::steady_clock::now();
        results.clear();
        totalTests = 0;
        passedTests = 0;
        failedTests = 0;
    }
    
    void OnTestStart(const ::testing::TestInfo& test_info) override {
        totalTests++;
    }
    
    void OnTestPartResult(const ::testing::TestPartResult& result) override {
        // Результаты собираются в OnTestEnd
    }
    
    void OnTestEnd(const ::testing::TestInfo& test_info) override {
        bool passed = test_info.result()->Passed();
        if (passed) {
            passedTests++;
        } else {
            failedTests++;
            
            // Добавляем результат только если тест провалился
            TestResult tr;
            tr.suite = test_info.test_suite_name();
            tr.name = test_info.name();
            tr.passed = false;
            
            // Собираем сообщения об ошибках
            std::string errorMsg;
            const ::testing::TestResult* result = test_info.result();
            for (int i = 0; i < result->total_part_count(); i++) {
                const ::testing::TestPartResult& part = result->GetTestPartResult(i);
                if (part.failed()) {
                    if (!errorMsg.empty()) errorMsg += "; ";
                    errorMsg += part.message();
                }
            }
            tr.message = errorMsg.empty() ? "Test failed" : errorMsg;
            results.push_back(tr);
        }
    }
    
    void OnTestProgramEnd(const ::testing::UnitTest& unit_test) override {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();
        
        // Выводим результаты
        printResults(unit_test, duration);
    }
    
private:
    void printResults(const ::testing::UnitTest& unit_test, long long duration_ms) {
        std::cout << "\n";
        printHeader();
        printSummary(unit_test, duration_ms);
        
        if (failedTests > 0) {
            printFailedTests();
        }
        
        printFooter();
    }
    
    void printHeader() {
        std::cout << Colors::CYAN << Colors::BOLD;
        std::cout << Box::TOP_LEFT;
        for (int i = 0; i < 76; i++) std::cout << Box::HORIZONTAL;
        std::cout << Box::TOP_RIGHT << "\n";
        
        std::cout << Box::VERTICAL << " " << Colors::WHITE << Colors::BOLD;
        std::cout << std::left << std::setw(74) << "CRSF-IO Unit Tests - Test Results";
        std::cout << Colors::RESET << Colors::CYAN << " " << Box::VERTICAL << "\n";
        
        std::cout << Box::T_LEFT;
        for (int i = 0; i < 76; i++) std::cout << Box::HORIZONTAL;
        std::cout << Box::T_RIGHT << "\n";
        std::cout << Colors::RESET;
    }
    
    void printSummary(const ::testing::UnitTest& unit_test, long long duration_ms) {
        std::cout << Colors::CYAN << Box::VERTICAL << " " << Colors::RESET;
        std::cout << std::left << std::setw(74) << "Summary";
        std::cout << Colors::CYAN << " " << Box::VERTICAL << "\n";
        
        std::cout << Box::T_LEFT;
        for (int i = 0; i < 76; i++) std::cout << Box::HORIZONTAL;
        std::cout << Box::T_RIGHT << "\n";
        std::cout << Colors::RESET;
        
        // Общая статистика
        std::cout << Colors::CYAN << Box::VERTICAL << " " << Colors::RESET;
        std::cout << "Total Tests: " << Colors::BOLD << totalTests << Colors::RESET;
        std::cout << std::setw(60) << "";
        std::cout << Colors::CYAN << " " << Box::VERTICAL << "\n";
        
        std::cout << Colors::CYAN << Box::VERTICAL << " " << Colors::RESET;
        std::cout << Colors::GREEN << Box::CHECK << " Passed: " << Colors::BOLD << passedTests << Colors::RESET;
        std::cout << std::setw(60) << "";
        std::cout << Colors::CYAN << " " << Box::VERTICAL << "\n";
        
        std::cout << Colors::CYAN << Box::VERTICAL << " " << Colors::RESET;
        if (failedTests > 0) {
            std::cout << Colors::RED << Box::CROSS_MARK << " Failed: " << Colors::BOLD << failedTests << Colors::RESET;
        } else {
            std::cout << Colors::RED << Box::CROSS_MARK << " Failed: " << Colors::BOLD << "0" << Colors::RESET;
        }
        std::cout << std::setw(60) << "";
        std::cout << Colors::CYAN << " " << Box::VERTICAL << "\n";
        
        std::cout << Colors::CYAN << Box::VERTICAL << " " << Colors::RESET;
        std::cout << "Duration: " << Colors::BOLD << duration_ms << " ms" << Colors::RESET;
        std::cout << std::setw(60) << "";
        std::cout << Colors::CYAN << " " << Box::VERTICAL << "\n";
        
        std::cout << Box::T_LEFT;
        for (int i = 0; i < 76; i++) std::cout << Box::HORIZONTAL;
        std::cout << Box::T_RIGHT << "\n";
        std::cout << Colors::RESET;
        
        // Статистика по тестовым наборам
        std::cout << Colors::CYAN << Box::VERTICAL << " " << Colors::RESET;
        std::cout << std::left << std::setw(74) << "Test Suites";
        std::cout << Colors::CYAN << " " << Box::VERTICAL << "\n";
        
        std::cout << Box::T_LEFT;
        for (int i = 0; i < 76; i++) std::cout << Box::HORIZONTAL;
        std::cout << Box::T_RIGHT << "\n";
        std::cout << Colors::RESET;
        
        int totalSuites = unit_test.total_test_suite_count();
        for (int i = 0; i < totalSuites; i++) {
            const ::testing::TestSuite* suite = unit_test.GetTestSuite(i);
            if (suite) {
                std::cout << Colors::CYAN << Box::VERTICAL << " " << Colors::RESET;
                std::cout << "  " << Colors::YELLOW << suite->name() << Colors::RESET;
                std::cout << " (" << suite->test_count() << " tests)";
                std::cout << std::setw(74 - 20 - static_cast<int>(suite->name().length())) << "";
                std::cout << Colors::CYAN << " " << Box::VERTICAL << "\n";
            }
        }
    }
    
    void printFailedTests() {
        std::cout << Box::T_LEFT;
        for (int i = 0; i < 76; i++) std::cout << Box::HORIZONTAL;
        std::cout << Box::T_RIGHT << "\n";
        std::cout << Colors::RESET;
        
        std::cout << Colors::CYAN << Box::VERTICAL << " " << Colors::RESET;
        std::cout << Colors::RED << Colors::BOLD << std::left << std::setw(74) << "Failed Tests";
        std::cout << Colors::RESET << Colors::CYAN << " " << Box::VERTICAL << "\n";
        
        std::cout << Box::T_LEFT;
        for (int i = 0; i < 76; i++) std::cout << Box::HORIZONTAL;
        std::cout << Box::T_RIGHT << "\n";
        std::cout << Colors::RESET;
        
        for (const auto& result : results) {
            std::cout << Colors::CYAN << Box::VERTICAL << " " << Colors::RESET;
            std::cout << Colors::RED << Box::CROSS_MARK << " " << Colors::RESET;
            std::cout << Colors::YELLOW << result.suite << Colors::RESET << "::";
            std::cout << Colors::WHITE << result.name << Colors::RESET;
            std::cout << std::setw(74 - 5 - static_cast<int>(result.suite.length() + result.name.length())) << "";
            std::cout << Colors::CYAN << " " << Box::VERTICAL << "\n";
            
            if (!result.message.empty()) {
                std::cout << Colors::CYAN << Box::VERTICAL << " " << Colors::RESET;
                std::cout << "    " << Colors::RED << result.message.substr(0, 70) << Colors::RESET;
                std::cout << std::setw(74 - 75) << "";
                std::cout << Colors::CYAN << " " << Box::VERTICAL << "\n";
            }
        }
    }
    
    void printFooter() {
        std::cout << Box::BOTTOM_LEFT;
        for (int i = 0; i < 76; i++) std::cout << Box::HORIZONTAL;
        std::cout << Box::BOTTOM_RIGHT << "\n";
        std::cout << Colors::RESET;
        
        if (failedTests == 0) {
            std::cout << "\n" << Colors::GREEN << Colors::BOLD;
            std::cout << "  " << Box::CHECK << " All tests passed!" << Colors::RESET << "\n\n";
        } else {
            std::cout << "\n" << Colors::RED << Colors::BOLD;
            std::cout << "  " << Box::CROSS_MARK << " Some tests failed!" << Colors::RESET << "\n\n";
        }
    }
};

// Функция для выполнения команды shell
int executeCommand(const std::string& command) {
    std::cout << Colors::YELLOW << "Executing: " << Colors::RESET << command << "\n";
    int result = system(command.c_str());
    return result;
}

// Функция для пересборки тестов
void rebuildTests() {
    std::cout << "\n" << Colors::CYAN << Colors::BOLD;
    std::cout << Box::TOP_LEFT;
    for (int i = 0; i < 76; i++) std::cout << Box::HORIZONTAL;
    std::cout << Box::TOP_RIGHT << "\n";
    
    std::cout << Box::VERTICAL << " " << Colors::WHITE << Colors::BOLD;
    std::cout << std::left << std::setw(74) << "Rebuilding Tests...";
    std::cout << Colors::RESET << Colors::CYAN << " " << Box::VERTICAL << "\n";
    
    std::cout << Box::T_LEFT;
    for (int i = 0; i < 76; i++) std::cout << Box::HORIZONTAL;
    std::cout << Box::T_RIGHT << "\n";
    std::cout << Colors::RESET;
    
    // Выполняем clean
    std::cout << Colors::CYAN << Box::VERTICAL << " " << Colors::RESET;
    std::cout << "Step 1: Cleaning old build files..." << std::setw(50) << "";
    std::cout << Colors::CYAN << " " << Box::VERTICAL << "\n";
    std::cout << Colors::RESET;
    
    int cleanResult = executeCommand("make clean > /dev/null 2>&1");
    
    if (cleanResult == 0) {
        std::cout << Colors::CYAN << Box::VERTICAL << " " << Colors::RESET;
        std::cout << Colors::GREEN << Box::CHECK << " Clean completed" << Colors::RESET;
        std::cout << std::setw(55) << "";
        std::cout << Colors::CYAN << " " << Box::VERTICAL << "\n";
    } else {
        std::cout << Colors::CYAN << Box::VERTICAL << " " << Colors::RESET;
        std::cout << Colors::RED << Box::CROSS_MARK << " Clean failed" << Colors::RESET;
        std::cout << std::setw(57) << "";
        std::cout << Colors::CYAN << " " << Box::VERTICAL << "\n";
    }
    
    std::cout << Box::T_LEFT;
    for (int i = 0; i < 76; i++) std::cout << Box::HORIZONTAL;
    std::cout << Box::T_RIGHT << "\n";
    std::cout << Colors::RESET;
    
    // Выполняем build
    std::cout << Colors::CYAN << Box::VERTICAL << " " << Colors::RESET;
    std::cout << "Step 2: Building tests..." << std::setw(55) << "";
    std::cout << Colors::CYAN << " " << Box::VERTICAL << "\n";
    std::cout << Colors::RESET;
    
    int buildResult = executeCommand("make test_runner_console > /dev/null 2>&1");
    
    std::cout << Box::BOTTOM_LEFT;
    for (int i = 0; i < 76; i++) std::cout << Box::HORIZONTAL;
    std::cout << Box::BOTTOM_RIGHT << "\n";
    std::cout << Colors::RESET;
    
    if (buildResult == 0) {
        std::cout << "\n" << Colors::GREEN << Colors::BOLD;
        std::cout << "  " << Box::CHECK << " Rebuild completed successfully!" << Colors::RESET << "\n";
        std::cout << Colors::YELLOW << "  Note: Please restart the test runner to use the new build." << Colors::RESET << "\n\n";
    } else {
        std::cout << "\n" << Colors::RED << Colors::BOLD;
        std::cout << "  " << Box::CROSS_MARK << " Rebuild failed! Check errors above." << Colors::RESET << "\n\n";
    }
}

// Функция для отображения меню
void printMenu() {
    std::cout << Colors::CYAN << Colors::BOLD;
    std::cout << Box::TOP_LEFT;
    for (int i = 0; i < 76; i++) std::cout << Box::HORIZONTAL;
    std::cout << Box::TOP_RIGHT << "\n";
    
    std::cout << Box::VERTICAL << " " << Colors::WHITE << Colors::BOLD;
    std::cout << std::left << std::setw(74) << "Actions";
    std::cout << Colors::RESET << Colors::CYAN << " " << Box::VERTICAL << "\n";
    
    std::cout << Box::T_LEFT;
    for (int i = 0; i < 76; i++) std::cout << Box::HORIZONTAL;
    std::cout << Box::T_RIGHT << "\n";
    std::cout << Colors::RESET;
    
    std::cout << Colors::CYAN << Box::VERTICAL << " " << Colors::RESET;
    std::cout << "  " << Colors::YELLOW << "[R]" << Colors::RESET << " - Rebuild tests (make clean && make)";
    std::cout << std::setw(40) << "";
    std::cout << Colors::CYAN << " " << Box::VERTICAL << "\n";
    
    std::cout << Colors::CYAN << Box::VERTICAL << " " << Colors::RESET;
    std::cout << "  " << Colors::YELLOW << "[Q]" << Colors::RESET << " - Quit";
    std::cout << std::setw(60) << "";
    std::cout << Colors::CYAN << " " << Box::VERTICAL << "\n";
    
    std::cout << Box::BOTTOM_LEFT;
    for (int i = 0; i < 76; i++) std::cout << Box::HORIZONTAL;
    std::cout << Box::BOTTOM_RIGHT << "\n";
    std::cout << Colors::RESET;
    
    std::cout << "\n" << Colors::CYAN << "Select action: " << Colors::RESET;
    std::cout.flush();
}

int main(int argc, char** argv) {
    // Проверяем, нужно ли запускать тесты сразу или показать меню
    bool runTests = true;
    bool showMenu = true;
    
    // Парсим аргументы командной строки
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--no-menu" || arg == "-n") {
            showMenu = false;
        }
        if (arg == "--rebuild" || arg == "-r") {
            rebuildTests();
            return 0;
        }
    }
    
    // Основной цикл
    while (true) {
        if (runTests) {
            // Инициализация Google Test
            ::testing::InitGoogleTest(&argc, argv);
            
            // Добавляем кастомный listener
            ::testing::TestEventListeners& listeners = 
                ::testing::UnitTest::GetInstance()->listeners();
            
            // Удаляем стандартный printer (если есть)
            delete listeners.Release(listeners.default_result_printer());
            
            // Добавляем наш кастомный collector
            TestResultCollector* collector = new TestResultCollector;
            listeners.Append(collector);
            
            // Запускаем тесты
            int result = RUN_ALL_TESTS();
            
            // Если не нужно показывать меню, выходим
            if (!showMenu) {
                return result;
            }
            
            runTests = false; // После первого запуска не запускаем автоматически
        }
        
        // Показываем меню
        printMenu();
        
        // Читаем выбор пользователя
        std::string choice;
        std::getline(std::cin, choice);
        
        if (choice.empty()) {
            continue;
        }
        
        // Обрабатываем выбор
        char ch = choice[0];
        if (ch == 'R' || ch == 'r') {
            rebuildTests();
            std::cout << Colors::YELLOW << "Press Enter to return to menu..." << Colors::RESET;
            std::cin.get();
        } else if (ch == 'Q' || ch == 'q') {
            std::cout << Colors::CYAN << "Goodbye!" << Colors::RESET << "\n";
            return 0;
        } else {
            std::cout << Colors::RED << "Invalid choice. Please try again." << Colors::RESET << "\n";
        }
    }
    
    return 0;
}

