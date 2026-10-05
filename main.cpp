#include <cstring>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <thread>
#include <atomic>

/// Ошибки для обратной связи в консоли
const constexpr char* kArgError = "Invalid argument, use `-h` or `--help`";
const constexpr char* kFileOpenError = "Failed to open file";
// const constexpr char* kReadFileError = "Failed to read file";
const constexpr char* kRuntimeError = "Runtime error";
const constexpr char* kUnresolveError = "Unresolve error";

enum class Errors : char {
    kFree = 0,
    kOpenFile = -1,
    kRuntime = -2,
};

/// Цвета для форматирования шрифта вывода
struct Color {
    static constexpr const char* kReset = "\033[0m";
    static constexpr const char* kRed = "\033[31m";
    static constexpr const char* kGreen = "\033[32m";
    static constexpr const char* kYellow = "\033[33m";
    static constexpr const char* kBlue = "\033[34m";
    static constexpr const char* kMagenta = "\033[35m";
    static constexpr const char* kCyan = "\033[36m";
};

/// Метод помошник - для ознакомления с синтаксисом утилиты
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

constexpr unsigned char kDefaultLastMask = 6;
unsigned char indexing_table[256]{0};

/// Для оптимизации кода: минимизации сравнений и проверок,
/// используется битовая арифметика. Биты выставлены в таблице,
/// где ключом является символы из ascii таблицы
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
    size_t* bytes_ = nullptr;
    size_t* lines_ = nullptr;
    size_t* words_ = nullptr;
    size_t* alnums_ = nullptr;
    Errors* errors_ = nullptr;
    size_t size_ = 0;
    size_t capacity_ = 0;

public:

    Slice(size_t cap) {
        capacity_ = cap;
        names_ = new const char*[capacity_];
        bytes_ = new size_t[capacity_];
        lines_ = new size_t[capacity_];
        words_ = new size_t[capacity_];
        alnums_ = new size_t[capacity_];
        errors_ = new Errors[capacity_];
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

            delete[] bytes_;
            delete[] lines_;
            delete[] words_;
            delete[] alnums_;
            delete[] errors_;
            bytes_ = new size_t[capacity_];
            lines_ = new size_t[capacity_];
            words_ = new size_t[capacity_];
            alnums_ = new size_t[capacity_];
            errors_ = new Errors[capacity_];
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

    size_t* GetBytes(size_t i) const {
        return &bytes_[i];
    }
    size_t* GetLines(size_t i) const {
        return &lines_[i];
    }
    size_t* GetWords(size_t i) const {
        return &words_[i];
    }
    size_t* GetAlnums(size_t i) const {
        return &alnums_[i];
    }

    void SetError(size_t i, Errors error) const {
        errors_[i] = error;
    }

    void PrintResult(size_t i,
        bool lines, bool bytes, bool words, bool alnums) const {
        if (!static_cast<char>(errors_[i])) {
            if (lines) {
                std::cout << lines_[i] << " ";
            }
            if (words) {
                std::cout << words_[i] << " ";
            }
            if (alnums) {
                std::cout << alnums_[i] << " ";
            }
            if (bytes) {
                std::cout << bytes_[i] << " ";
            }
        } else {
            switch (errors_[i]) {
                case Errors::kOpenFile: {
                    std::cerr << Color::kRed << kFileOpenError << ": " << Color::kReset;
                    break;
                }
                case Errors::kRuntime: {
                    std::cerr << Color::kRed << kRuntimeError << ": " << Color::kReset;
                    break;
                }
                default: {
                    std::cerr << Color::kRed << kUnresolveError << ": " << Color::kReset;
                }
            }
        }

        std::cout << names_[i] << '\n';
    }

    ~Slice() {
        // ReSharper disable once CppDFADeletedPointer
        delete[] names_;
        // ReSharper disable once CppDFADeletedPointer
        delete[] bytes_;
        // ReSharper disable once CppDFADeletedPointer
        delete[] lines_;
        // ReSharper disable once CppDFADeletedPointer
        delete[] words_;
        // ReSharper disable once CppDFADeletedPointer
        delete[] alnums_;
        delete[] errors_;
    }

    // Переопределяем копирование
    Slice(const Slice& other) {
        this->capacity_ = other.capacity_;
        this->size_ = other.size_;

        this->names_ = new const char*[capacity_];
        this->bytes_ = new size_t[capacity_];
        this->lines_ = new size_t[capacity_];
        this->words_ = new size_t[capacity_];
        this->alnums_ = new size_t[capacity_];
        this->errors_ = new Errors[capacity_];
        // ReSharper disable once CppDFAConstantConditions
        // ReSharper disable CppDFAUnreachableCode
        for (size_t i = 0; i < this->size_; ++i) {
            // ReSharper disable CppDFANullDereference
            this->names_[i] = other.names_[i];
            this->bytes_[i] = other.bytes_[i];
            this->lines_[i] = other.lines_[i];
            this->words_[i] = other.words_[i];
            this->alnums_[i] = other.alnums_[i];
            this->errors_[i] = other.errors_[i];
            // ReSharper restore CppDFANullDereference
        }
        // ReSharper restore CppDFAUnreachableCode
    }
    Slice& operator=(const Slice& other) {
        if (this == &other) {
            return *this;
        }

        this->capacity_ = other.capacity_;
        this->size_ = other.size_;

        delete[] this->names_;
        delete[] this->bytes_;
        delete[] this->lines_;
        delete[] this->words_;
        delete[] this->alnums_;
        delete[] this->errors_;

        this->names_ = new const char*[capacity_];
        this->bytes_ = new size_t[capacity_];
        this->lines_ = new size_t[capacity_];
        this->words_ = new size_t[capacity_];
        this->alnums_ = new size_t[capacity_];
        this->errors_ = new Errors[capacity_];
        // ReSharper disable once CppDFAConstantConditions
        // ReSharper disable CppDFAUnreachableCode
        for (size_t i = 0; i < this->size_; ++i) {
            this->names_[i] = other.names_[i];
            this->bytes_[i] = other.bytes_[i];
            this->lines_[i] = other.lines_[i];
            this->words_[i] = other.words_[i];
            this->alnums_[i] = other.alnums_[i];
            this->errors_[i] = other.errors_[i];
        }
        // ReSharper restore CppDFAUnreachableCode
        return *this;
    }

    // Переопределяем перемещение
    Slice(Slice&& other) noexcept {
        this->capacity_ = other.capacity_;
        this->size_ = other.size_;

        this->names_ = other.names_;
        this->bytes_ = other.bytes_;
        this->lines_ = other.lines_;
        this->words_ = other.words_;
        this->alnums_ = other.alnums_;
        this->errors_ = other.errors_;

        other.capacity_ = 0;
        other.size_ = 0;
        other.names_ = nullptr;
        other.bytes_ = nullptr;
        other.lines_ = nullptr;
        other.words_ = nullptr;
        other.alnums_ = nullptr;
        other.errors_ = nullptr;
    }
    Slice& operator=(Slice&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        this->capacity_ = other.capacity_;
        this->size_ = other.size_;

        delete[] this->names_;
        delete[] this->bytes_;
        delete[] this->lines_;
        delete[] this->words_;
        delete[] this->alnums_;
        delete[] this->errors_;

        this->names_ = other.names_;
        this->bytes_ = other.bytes_;
        this->lines_ = other.lines_;
        this->words_ = other.words_;
        this->alnums_ = other.alnums_;
        this->errors_ = other.errors_;

        other.capacity_ = 0;
        other.size_ = 0;
        other.names_ = nullptr;
        other.bytes_ = nullptr;
        other.lines_ = nullptr;
        other.words_ = nullptr;
        other.alnums_ = nullptr;
        other.errors_ = nullptr;

        return *this;
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

void CountElements(std::ifstream* file, Errors* err, size_t* byte, size_t* line, size_t* word, size_t* alnum) {

    unsigned char mask = 0;
    // Чтоб не было ложного срабатывания на слова и \n
    unsigned char last_mask = kDefaultLastMask;
    size_t lines_c = 0;
    size_t bytes_c = 0;
    size_t words_c = 0;
    size_t alnum_c = 0;

    unsigned char c = 0;
    char buffer[65536];
    while (file->read(buffer, sizeof(buffer)) || file->gcount() > 0) {
        size_t bytes_count = static_cast<size_t>(file->gcount());

        // bytes: увеличиваем счетчик байт по количеству считаного в буфер
        bytes_c += bytes_count;
        for (size_t i = 0; i < bytes_count; ++i) {
            // Получаем битовую маску по символу из файла
            c = static_cast<unsigned char>(buffer[i]);
            mask = indexing_table[c];

            // Если символ был \n, то выражение 1, иначе - 0
            lines_c += ((mask & 2) >> 1);
            // Если символ был латинским или цифрой, то  =1, иначе =0
            alnum_c += (mask & 1);
            /* Если текущий символ `пробельный`,
             * а предидущий не `пробельный`,
             * то =1, иначе =0 */
            words_c += ((mask & 4) >> 2) & !((last_mask & 4) >> 2);
            last_mask = mask;
        }
    }

    // Если последний символ был не переносом строки, то увеличиваем счетчик строк
    lines_c += !(last_mask & 2);
    // Если последний симаол был не `пробельным`, то это слово
    words_c += !(last_mask & 4);


    /// Проверяем состояяние файла по маске.
    /// Смотрим только на состояние bad, fail и eof.
    /// Ошибка есть если состояние bad или fail(но без eof)
    std::ios_base::iostate s = file->rdstate() & (std::ios_base::badbit|std::ios_base::failbit|std::ios_base::eofbit);
    if (
        (s & std::ios_base::badbit) |
        !((s & (std::ios_base::failbit|std::ios_base::eofbit)) ^ std::ios_base::failbit)
    ) {
        *err = Errors::kRuntime;
        return;
    }

    if (file->bad() || (file->fail() && !file->eof())) {
        *err = Errors::kRuntime;
        return;
    }

    *byte = bytes_c;
    *line = lines_c;
    *word = words_c;
    *alnum = alnum_c;
}

struct ThreadScope {
private:
    // Пул потоков для распределения задач
    std::jthread* jthreads_ = nullptr;
    // Количество доступных ядер
    size_t cores_ = 0;
    // Счетчик жизней, нужен для этапа завершения жизни пула
    size_t live_cores_ = 0;
    // Метка говорит что пул уничтожается
    std::atomic<bool> death_ = false;

    enum class TaskState : char {
        // Жизненый цикл выполнени задач потоками
        kFree = 0,
        kInitializing = 1,
        kWork = 2,
        kDeath = 3,
    };
    enum class InitState : char {
        // Состояние создания потока
        kNoninitialized = 0,
        kInitializing = 1,
        kInitialized = 2,
        kDeath = 3,
    };

    // Флаги состояния потоков
    std::atomic<TaskState>* task_flags_ = nullptr;
    std::atomic<InitState>* init_flags_ = nullptr;

    // Список для передачи задач
    void** tasks_ = nullptr;
    // Счетчик задач
    std::atomic<size_t> task_counter_{0};

public:
    ThreadScope(size_t n) {
        // Пытаемся узнать число доступных потоков
        unsigned int cores = std::thread::hardware_concurrency();
        if (cores == 0) {
            // Если не распозняли окружение, ставим дефолт для экономии
            cores_ = 2;
        } else if (cores >= n) {
            cores_ = n;
        } else {
            cores_ = cores;
        }
        live_cores_ = cores_;
        jthreads_= new std::jthread[cores_];
        task_flags_ = new std::atomic<TaskState>[cores_]{TaskState::kFree};
        init_flags_ = new std::atomic<InitState>[cores_]{InitState::kNoninitialized};
        tasks_ = new void*[cores_];
    }

    template <typename F>
    void Push(F* lambda) {
        if (death_.load()) {
            delete lambda;
            return;
        }
        ++task_counter_;
        // ReSharper disable once CppJoinDeclarationAndAssignment
        size_t task_count;
        while (true) {
            task_count = task_counter_.load();
            for (size_t i = 0; i < cores_; ++i) {
                InitState expected = InitState::kNoninitialized;
                if (init_flags_[i].compare_exchange_strong(expected, InitState::kInitializing)) {
                    // Если тред еще не был инициализирован, создаем
                    tasks_[i] = static_cast<void*>(lambda);
                    task_flags_[i].store(TaskState::kWork);
                    jthreads_[i] = std::jthread([this, i] {
                        // Создаем в потоке бесконечное исполнение со своим жизниным циклом
                        while (true) {
                            // Если поток пуст, он ждет назначения задачи
                            if (task_flags_[i].load() == TaskState::kFree) {
                                task_flags_[i].wait(TaskState::kFree);
                            }

                            // Когда задача появилась, он ждет инициализации задачи
                            if (task_flags_[i].load() == TaskState::kInitializing) {
                                task_flags_[i].wait(TaskState::kInitializing);
                            }

                            if (task_flags_[i].load() == TaskState::kDeath) {
                                init_flags_[i].store(InitState::kDeath);
                                return;
                            }

                            // Выполняем задачу и очизаем ресурсы
                            (*static_cast<F*>(tasks_[i]))();// Вызвали задачу
                            delete static_cast<F*>(tasks_[i]); // Удалили задачу
                            task_flags_[i].store(TaskState::kFree);
                            --task_counter_; // Уменьшаем общее число задач
                            task_counter_.notify_all(); // Будим очередь
                        }
                    });
                    init_flags_[i].store(InitState::kInitialized);
                    return;
                }
                TaskState exp = TaskState::kFree;
                if (task_flags_[i].compare_exchange_strong(exp, TaskState::kInitializing)) {
                    // Если тред существует, кладем ему таску и выставляем флаг
                    tasks_[i] = static_cast<void*>(lambda);
                    task_flags_[i].store(TaskState::kWork);
                    task_flags_[i].notify_one(); // Будим поток на работу
                    return;
                }
            }
            // Ждем освобождения потока
            task_counter_.wait(task_count);
        }
    }

    void Done() {
        death_.store(true);
        size_t task_count = task_counter_.load();
        while (task_count > 0) {
            task_counter_.wait(task_count);
            task_count = task_counter_.load();
        }
    }

    ~ThreadScope() {
        Done();
        for (size_t i = 0; i < cores_; ++i) {

            if (task_flags_[i].load() == TaskState::kFree) {
                task_flags_[i].store(TaskState::kDeath);
                task_flags_[i].notify_one();
                if (jthreads_[i].joinable()) {
                    jthreads_[i].join();
                }
            }
        }
        delete[] jthreads_;
        delete[] task_flags_;
        delete[] init_flags_;
        delete[] tasks_;
    }
};

inline void AnalyzeFile(const Slice& files, std::atomic<bool>& error, size_t i) {
    const char* name = files.Get(i);
    std::ifstream file;
    file.open(name, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        files.SetError(i, Errors::kOpenFile);
        error.store(true);
        return;
    }
    Errors runtime_error = Errors::kFree;
    CountElements(&file, &runtime_error,
        files.GetBytes(i), files.GetLines(i), files.GetWords(i), files.GetAlnums(i));
    files.SetError(i, runtime_error);
    if (runtime_error != Errors::kFree) {
        error.store(true);
    }
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
        return 0;
    }

    if (lines == false && bytes == false && words == false && alnum == false) {
        lines = true, bytes = true, words = true;
    }

    size_t n = files.Len();
    // ReSharper disable CppDFAConstantConditions
    // ReSharper disable CppDFAUnreachableCode
    if (n > 1) {
        std::atomic<bool> error{false};
        ThreadScope scope(n);
        for (size_t i = 0; i < n; ++i) {
            // ReSharper disable once CppDFAUnreachableFunctionCall
            auto lambda_local = [&files, &error, i] {
                AnalyzeFile(files, error, i);
            };

            auto* lambda = new auto(lambda_local);
            scope.Push(lambda);
        }

        scope.Done();

        for (size_t i = 0; i < files.Len(); ++i) {
            files.PrintResult(i, lines, bytes, words, alnum);
        }

        if (error.load()) {
            return 1;
        }

        return 0;
    }

    if (files.Len() == 1) {
        std::atomic<bool> error{false};
        AnalyzeFile(files, error, 0);
        files.PrintResult(0, lines, bytes, words, alnum);
        if (error.load()) {
            return 1;
        }
    }
    // ReSharper restore CppDFAConstantConditions
    // ReSharper restore CppDFAUnreachableCode

    return 0;
}