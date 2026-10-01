#include <cstring>
#include <cstdio>
#include <fstream>
#include <iostream>

// Ошибки для обратной связи в консоли
const constexpr char* kArgError = "Invalid argument, use `-h` or `--help`";
const constexpr char* kFileOpenError = "Failed to open file";
const constexpr char* kReadFileError = "Failed to read file";
// const constexpr char* kRuntimeError = "Runtime error";

// Цвета для форматирования шрифта вывода
struct Color {
    static constexpr const char* kReset = "\033[0m";
    static constexpr const char* kRed = "\033[31m";
    static constexpr const char* kGreen = "\033[32m";
    static constexpr const char* kYellow = "\033[33m";
    static constexpr const char* kBlue = "\033[34m";
    static constexpr const char* kMagenta = "\033[35m";
    static constexpr const char* kCyan = "\033[36m";
};

// Метод помошник - для ознакомления с синтаксисом утилиты
void Help() {
    std::cout << Color::kCyan << "Use syntax:\n\n" << Color::kMagenta;

    printf(
        "Короткий аргумент | Длинный аргумент |                Описание        "
        " \n");
    printf(
        "----------------------------------------------------------------------"
        "-------\n");
    printf(
        "       -l         |     --lines      | Вывод только количества "
        "строк\n");
    printf(
        "----------------------------------------------------------------------"
        "-------\n");
    printf(
        "       -c         |     --bytes      | Вывод размера файла в "
        "байтах\n");
    printf(
        "----------------------------------------------------------------------"
        "-------\n");
    printf("       -w         |     --words      | Вывод количества слов\n");
    printf(
        "----------------------------------------------------------------------"
        "-------\n");
    printf(
        "       -a         |     --alnum      | Вывод количества латинских "
        "букв и цифр\n");
    printf(
        "----------------------------------------------------------------------"
        "-------\n\n");

    std::cout << Color::kReset;
}

char indexing_table[256]{0};
void InitTable() {
    // Латиница и цифры
    for (int i = '0'; i <= '9'; ++i) {
        indexing_table[i] |= 1;
    }
    for (int i = 'a'; i <= 'z'; ++i) {
        indexing_table[i] |= 1;
    }
    for (int i = 'A'; i <= 'Z'; ++i) {
        indexing_table[i] |= 1;
    }

    // Строки
    indexing_table['\n'] |= 2;

    // Пустоты
    indexing_table[' '] |= 4;
    indexing_table['\n'] |= 4;
    indexing_table['\t'] |= 4;
    indexing_table['\r'] |= 4;
    indexing_table['\v'] |= 4;
    indexing_table['\f'] |= 4;
}

struct Slice {
private:
    const char** names_ = nullptr;
    size_t size_ = 0;
    size_t capacity_ = 0;

public:

    Slice(size_t cap) {
        capacity_ = cap;
        names_ = new const char*[capacity_];
    }

    void Append(const char* name) {
        if (size_+1 >= capacity_) {
            capacity_ *= 2;
            const char** new_names = new const char*[capacity_];

            for (size_t i = 0; i < size_; ++i) {
                // Предупреждение срабатывает потому что names указывает на память,
                // не принадлежащую ему, argv.
                // Но эта память остается валидной во время работы программы, поэтому опасности нет
                // ReSharper disable once CppDFADeletedPointer
                new_names[i] = names_[i];
            }
            // Мы очищаем массив сырых указателей, у них нет диструктора и argv не пострадает
            // ReSharper disable once CppDFADeletedPointer
            delete[] names_;
            names_ = new_names;
        }
        names_[size_] = name;
        ++size_;
    }

    const char* Get(size_t i) const {
        // ReSharper disable once CppDFADeletedPointer
        return names_[i];
    }

    size_t Len() const {
        return size_;
    }

    ~Slice() {
        delete[] names_;
    }
};

Slice ParseArgs(int argc, char** argv, const char** err, bool* lines,
                bool* bytes, bool* words, bool* alnum, bool* help) {
    Slice files{4};
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];

        if (strlen(arg) < 2) {
            *err = kArgError;
            return files;
        }

        if (arg[0] != '-') {
            files.Append(arg);
        } else {
            if (arg[1] == '-') {
                if (strcmp(arg, "--lines") == 0) {
                    *lines = true;
                } else if (strcmp(arg, "--bytes") == 0) {
                    *bytes = true;
                } else if (strcmp(arg, "--words") == 0) {
                    *words = true;
                } else if (strcmp(arg, "--alnum") == 0) {
                    *alnum = true;
                } else if (strcmp(arg, "--help") == 0) {
                    *help = true;
                } else {
                    *err = kArgError;
                    return files;
                }
            } else {
                for (size_t j = 1; j < strlen(arg); ++j) {
                    switch (arg[j]) {
                        case 'l': {
                            *lines = true;
                            break;
                        }
                        case 'c': {
                            *bytes = true;
                            break;
                        }
                        case 'w': {
                            *words = true;
                            break;
                        }
                        case 'a': {
                            *alnum = true;
                            break;
                        }
                        case 'h': {
                            *help = true;
                            break;
                        }
                        default: {
                            *err = kArgError;
                            return files;
                        }
                    }
                }
            }
        }
    }

    return files;
}

inline bool CheckLatinOrNumber(const int ch) {
    if ('A' <= ch && ch <= 'Z') {
        return true;
    }
    if ('a' <= ch && ch <= 'z') {
        return true;
    }
    if ('0' <= ch && ch <= '9') {
        return true;
    }
    return false;
}

void CountElements(std::ifstream* file, const char* name, const char** err, bool lines,
                bool bytes, bool words, bool alnum) {

    unsigned char mask = 0;
    // Чтоб не было ложного срабатывания на слова и \n
    unsigned char last_mask = 6;
    size_t lines_c = 0;
    size_t bytes_c = 0;
    size_t words_c = 0;
    size_t alnum_c = 0;

    unsigned char c = 0;
    char buffer[65536];
    while (file->read(buffer, sizeof(buffer)) || file->gcount() > 0) {
        size_t bytes_count = static_cast<size_t>(file->gcount());

        // bytes
        bytes_c += bytes_count;
        for (size_t i = 0; i < bytes_count; ++i) {
            c = buffer[i];
            mask = indexing_table[c];

            lines_c += ((mask & 2) >> 1);
            alnum_c += (mask & 1);
            words_c += ((mask & 4) >> 2) & !((last_mask & 4) >> 2);
            last_mask = mask;
        }
    }

    lines_c += !(last_mask & 2);
    words_c += !(last_mask & 4);


    if (file->bad()) {
        *err = kReadFileError;
        return;
    }

    if (lines) {
        std::cout << lines_c << " ";
    }
    if (words) {
        std::cout << words_c << " ";
    }
    if (alnum) {
        std::cout << alnum_c << " ";
    }
    if (bytes) {
        std::cout << bytes_c << " ";
    }

    std::cout << name << '\n';
}

int main(int argc, char** argv) {

    if (argc < 2) {
        std::cerr << kArgError << '\n';
        return 1;
    }

    bool lines = false, bytes = false, words = false, alnum = false, help = false;
    const char* err = nullptr;

    Slice files = ParseArgs(argc, argv, &err, &lines, &bytes, &words, &alnum, &help);

    if (err != nullptr) {
        std::cerr << err << '\n';
        return 1;
    }

    InitTable();

    if (help) {
        Help();
    }

    if (lines == false && bytes == false && words == false && alnum == false) {
        lines = true, bytes = true, words = true;
    }

    for (size_t i = 0; i < files.Len(); ++i) {
        const char* name = files.Get(i);
        std::ifstream file;
        file.open(name);
        if (!file.is_open()) {
            std::cerr << kFileOpenError << '\n';
            return 1;
        }
        CountElements(&file, name, &err, lines, bytes, words, alnum);

        if (err != nullptr) {
            std::cerr << err << '\n';
            return 1;
        }
    }

    return 0;
}
