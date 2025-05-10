RAII（Resource Acquisition Is Initialization）是 C++ 編程的一個重要設計理念，它強調資源的管理應該與對象的生命週期捆綁在一起。簡而言之，資源的分配應該在對象初始化時完成，而資源的釋放則應在對象銷毀時自動完成。
核心概念：
- 資源綁定到對象的生命週期： 當資源（例如記憶體、文件句柄、網絡連接等）被分配時，它應與某個特定對象相關聯。當對象超出作用域或被顯式銷毀時，資源也會被自動釋放。
- 構造函數分配資源： 對象的構造函數負責資源的分配。例如打開文件、分配內存。
- 析構函數釋放資源： 對象的析構函數負責清理資源，確保無需手動釋放資源，避免內存洩漏或資源錯誤使用。

RAII 的優勢：
- 避免資源洩漏： 通過自動釋放資源，可以防止開發者忘記手動釋放資源。
- 提高程式的健壯性： 即使在出現異常的情況下，由於析構函數會自動執行，資源也能正確釋放。
- 簡化代碼： 減少重複的資源管理代碼，提升可讀性和維護性。

範例：
以下範例展示了如何使用 RAII 管理文件資源：
#include <iostream>
#include <fstream>

class FileHandler {
public:
    FileHandler(const std::string& filename) {
        file.open(filename, std::ios::in);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file!");
        }
    }
    
    ~FileHandler() {
        if (file.is_open()) {
            file.close(); // 自動關閉文件
        }
    }
    
    void read() {
        std::string line;
        while (std::getline(file, line)) {
            std::cout << line << std::endl;
        }
    }
    
private:
    std::ifstream file;
};

int main() {
    try {
        FileHandler fh("example.txt");
        fh.read();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 0; // FileHandler 的析構函數會自動執行
}


RAII 與智能指標：
C++11 推出了智能指標（例如 std::unique_ptr 和 std::shared_ptr），它們是 RAII 的典型實現，用於管理動態內存。例如：
#include <memory>

void func() {
    std::unique_ptr<int> ptr(new int(42)); // 分配資源
    // 無需顯式釋放資源，超出作用域後會自動銷毀
}

