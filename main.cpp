#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <iostream>

// Ошибки для обратной связи в консоли
const constexpr char* argError = "Invalid argument, use `-h` or `--help`";
const constexpr char* fileOpenError = "Failed to open file";
const constexpr char* readFileError = "Failed to read file";
const constexpr char* runtimeError = "Runtime error";

// Цвета для форматирования шрифта вывода
struct Color {
    static constexpr const char* RESET = "\033[0m";
    static constexpr const char* RED = "\033[31m";
    static constexpr const char* GREEN = "\033[32m";
    static constexpr const char* YELLOW = "\033[33m";
    static constexpr const char* BLUE = "\033[34m";
    static constexpr const char* MAGENTA = "\033[35m";
    static constexpr const char* CYAN = "\033[36m";
};

// Метод помошник - для ознакомления с синтаксисом утилиты
void Help() {
    std::cout << Color::CYAN << "Use syntax:\n\n" << Color::MAGENTA;

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

    std::cout << Color::RESET;
}

struct File {
    FILE* file;
    char name[256]{};
    bool place = false;

    File(const char* name) {
        file = fopen(name, "r");
        strcpy(this->name, name);
    }

    File() {
        file = nullptr;
        place = true;
    }

    bool isOpen() const { return file != nullptr; }

    void close() const {
        if (file != nullptr) {
            fclose(file);
        }
    }

    void clear() const {
        if (file != nullptr) {
            rewind(file);
        }
    }
};

void FreeFiles(File* files) {
    int f_i = 0;
    if (files == nullptr) {
        return;
    }
    while (true) {
        File file = files[f_i];
        ++f_i;
        if (file.place) {
            break;
        } else {
            file.close();
        }
    }

    free(files);
}

File* ParseArgs(int argc, char** argv, const char** err, bool* lines,
                bool* bytes, bool* words, bool* alnum) {
    size_t capacity = 4;
    File* files = (File*)malloc(capacity * sizeof(File));
    int f_i = 0;
    files[f_i] = File{};
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];

        if (strlen(arg) < 2) {
            *err = argError;
            free(files);
            return nullptr;
        }

        if (arg[0] != '-') {
            File file{arg};
            if (!file.isOpen()) {
                *err = fileOpenError;
                FreeFiles(files);
                return nullptr;
            } else {
                files[f_i] = file;
                ++f_i;
                files[f_i] = File{};
                if (f_i >= capacity) {
                    capacity += 4;
                    File* new_files =
                        (File*)realloc(files, capacity * sizeof(File));
                    if (new_files != nullptr) {
                        files = new_files;
                    } else {
                        *err = runtimeError;
                        FreeFiles(files);
                        // free(new_files);
                        return nullptr;
                    }
                }
            }
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
                } else {
                    *err = argError;
                    FreeFiles(files);
                    return nullptr;
                }
            } else {
                for (int j = 1; j < strlen(arg); ++j) {
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
                        default: {
                            *err = argError;
                            FreeFiles(files);
                            return nullptr;
                        }
                    }
                }
            }
        }
    }

    files[f_i] = File{};
    return files;
}

size_t CountBytes(const File* file, const char** err) {
    if (file->place || !file->isOpen()) {
        *err = readFileError;
        return 0;
    } file->clear();

    size_t size = 0;
    char buffer[4096 * 2];

    while (size_t read =
               fread(buffer, sizeof(char), sizeof(buffer), file->file)) {
        size += read;
    }

    if (ferror(file->file)) {
        *err = readFileError;
        return 0;
    }

    return size;
}

size_t CountWords(const File* file, const char** err) {
    if (file->place || !file->isOpen()) {
        *err = readFileError;
        return 0;
    }
    file->clear();

    size_t size = 0;

    while (fscanf(file->file, "%*s") != EOF) {
        ++size;
    }

    if (ferror(file->file)) {
        *err = readFileError;
        return 0;
    }

    return size;
}

size_t CountLines(const File* file, const char** err) {
    if (file->place || !file->isOpen()) {
        *err = readFileError;
        return 0;
    }
    file->clear();

    size_t size = 0;

    int ch;
    while ((ch = fgetc(file->file)) != EOF) {
        while (ch != '\n' && ch != EOF) {
            ch = fgetc(file->file);
        }
        ++size;
    }

    if (ferror(file->file)) {
        *err = readFileError;
        return 0;
    }

    return size;
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

size_t CountLatinsAndNumbers(const File* file, const char** err) {
    if (file->place || !file->isOpen()) {
        *err = readFileError;
        return 0;
    }
    file->clear();
    
    size_t size = 0;

    int ch;
    while ((ch = fgetc(file->file)) != EOF) {
        if (CheckLatinOrNumber(ch)) {
            ++size;
        }
        while (ch != '\n' && ch != EOF) {
            ch = fgetc(file->file);
            if (CheckLatinOrNumber(ch)) {
                ++size;
            }
        }
    }

    if (ferror(file->file)) {
        *err = readFileError;
        return 0;
    }

    return size;
}

int main(int argc, char** argv) {
    // std::cout << "Labwork1!\n";

    if (argc < 2) {
        std::cerr << argError << '\n';
        return 0;
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        Help();
        return 0;
    }

    bool lines = false, bytes = false, words = false, alnum = false;
    const char* err = "";

    File* files = ParseArgs(argc, argv, &err, &lines, &bytes, &words, &alnum);

    if (strcmp(err, "") != 0) {
        std::cerr << err << '\n';
        FreeFiles(files);
        return 0;
    }

    if (lines == false && bytes == false && words == false && alnum == false) {
        lines = true, bytes = true, words = true;
    }

    int f_i = 0;
    while (true) {
        File file = files[f_i];
        ++f_i;
        if (file.place) {
            break;
        }

        /*
         * Порядок вывода:
         * <lines> <words> <alnum> <bytes> <filename>
         */

        if (lines) {
            // std::cout << "lines " << CountLines(&file, &err);
            std::cout << CountLines(&file, &err) << " ";
        }
        if (words) {
            // std::cout << "words " << CountWords(&file, &err);
            std::cout << CountWords(&file, &err) << " ";
        }
        if (alnum) {
            // std::cout << "alnum " << CountLatinsAndNumbers(&file, &err);
            std::cout << CountLatinsAndNumbers(&file, &err) << " ";
        }
        if (bytes) {
            // std::cout << "bytes " << CountBytes(&file, &err);
            std::cout << CountBytes(&file, &err) << " ";
        }

        std::cout << file.name << '\n';
    }

    FreeFiles(files);
    return 0;
}
