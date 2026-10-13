
#include <iostream>
#include <thread>
#include <mutex>
#include <list>
#include <string>
#include <atomic>
#include <chrono>
#include <utility>
#include <windows.h>

using namespace std;

// ======================================================
// 1.2.1 — два потоки без join() і detach()
// УВАГА: цей пункт навмисно викликає std::terminate.
// ======================================================
void Thread1_1() {
    cout << "1" << endl;
}

void Thread2_1() {
    cout << "2" << endl;
}

void Task_1_2_1() {
    thread t1(Thread1_1);
    thread t2(Thread2_1);

    // Навмисно немає join() або detach().
    // При знищенні joinable thread програма викличе terminate.
}

// ======================================================
// 1.2.2 — два від'єднані потоки
// ======================================================
void Thread1_2() {
    cout << "1" << endl;
}

void Thread2_2() {
    cout << "2" << endl;
}

void Task_1_2_2() {
    thread t1(Thread1_2);
    thread t2(Thread2_2);

    t1.detach();
    t2.detach();

    // main не очікує завершення цих потоків.
    // Тому вивід може бути неповним.
}

// ======================================================
// 1.2.3 — спільний список без mutex
// Навчальний приклад небезпечного доступу до даних.
// ======================================================
list<int> sharedList3;

void AddToList3(int startValue) {
    for (int i = 0; i < 10; ++i) {
        int value = startValue + i;
        sharedList3.push_back(value);
        cout << "Додано: " << value << endl;
    }
}

void ListContains3(int target) {
    for (int i = 0; i < 10; ++i) {
        bool found = false;

        for (int value : sharedList3) {
            if (value == target) {
                found = true;
                break;
            }
        }

        cout << "Число " << target
            << (found ? " знайдено" : " не знайдено")
            << endl;

        this_thread::sleep_for(chrono::milliseconds(10));
    }
}

void Task_1_2_3() {
    sharedList3.clear();

    thread adder(AddToList3, 10);
    thread searcher(ListContains3, 15);

    adder.join();
    searcher.join();
}

// ======================================================
// 1.2.4 — список захищений mutex
// ======================================================
list<int> sharedList4;
mutex listMutex4;

void AddToList4(int startValue) {
    for (int i = 0; i < 10; ++i) {
        int value = startValue + i;

        {
            lock_guard<mutex> lock(listMutex4);
            sharedList4.push_back(value);
        }

        cout << "Додано: " << value << endl;
    }
}

void ListContains4(int target) {
    for (int i = 0; i < 10; ++i) {
        bool found = false;

        {
            lock_guard<mutex> lock(listMutex4);

            for (int value : sharedList4) {
                if (value == target) {
                    found = true;
                    break;
                }
            }
        }

        cout << "Число " << target
            << (found ? " знайдено" : " не знайдено")
            << endl;

        this_thread::sleep_for(chrono::milliseconds(10));
    }
}

void Task_1_2_4() {
    sharedList4.clear();

    thread adder(AddToList4, 10);
    thread searcher(ListContains4, 15);

    adder.join();
    searcher.join();
}

// ======================================================
// 1.2.5 — 10 потоків додавання та 10 потоків пошуку
// Потоки від'єднуються, але main чекає на їх завершення.
// ======================================================
list<int> sharedList5;
mutex listMutex5;
mutex outputMutex5;
atomic<int> completed5{ 0 };

void AddOne5(int value) {
    {
        lock_guard<mutex> lock(listMutex5);
        sharedList5.push_back(value);
    }

    {
        lock_guard<mutex> lock(outputMutex5);
        cout << "Додано: " << value << endl;
    }

    ++completed5;
}

void SearchOne5(int target) {
    bool found = false;

    {
        lock_guard<mutex> lock(listMutex5);

        for (int value : sharedList5) {
            if (value == target) {
                found = true;
                break;
            }
        }
    }

    {
        lock_guard<mutex> lock(outputMutex5);
        cout << "Пошук " << target << ": "
            << (found ? "знайдено" : "не знайдено")
            << endl;
    }

    ++completed5;
}

void Task_1_2_5() {
    sharedList5.clear();
    completed5 = 0;

    for (int i = 0; i < 10; ++i) {
        thread addThread(AddOne5, 100 + i);
        addThread.detach();

        thread searchThread(SearchOne5, 105);
        searchThread.detach();
    }

    // Чекаємо, поки всі 20 від'єднаних потоків завершать роботу.
    while (completed5.load() < 20) {
        this_thread::yield();
    }

    cout << "Усі 20 потоків завершили роботу." << endl;
}

// ======================================================
// 1.2.6 і 1.2.7 — клас exchangePerson
// ======================================================
struct someData {
    string name;
    string surname;
    string address;
    int age;
};

atomic<int> completed6{ 0 };

class exchangePerson {
private:
    someData data;
    mutable mutex mtx;

public:
    exchangePerson(string n, string s, string a, int years)
        : data{ n, s, a, years } {
    }

    // Забороняємо копіювання об'єкта з mutex.
    exchangePerson(const exchangePerson&) = delete;
    exchangePerson& operator=(const exchangePerson&) = delete;

    void Print() const {
        lock_guard<mutex> lock(mtx);

        cout << data.name << " " << data.surname
            << ", адреса: " << data.address
            << ", вік: " << data.age << endl;
    }

    static void JohnDoe(exchangePerson& person) {
        {
            lock_guard<mutex> lock(person.mtx);
            person.data.name = "John";
            person.data.surname = "Doe";
            person.data.address = "Unknown";
            person.data.age = 120;
        }
        ++completed6;
    }

    static void JacobSmith(exchangePerson& person) {
        {
            lock_guard<mutex> lock(person.mtx);
            person.data.name = "Jacob";
            person.data.surname = "Smith";
            person.data.address = "Known";
            person.data.age = 1;
        }
        ++completed6;
    }

    // 1.2.6 — std::lock + lock_guard + adopt_lock
    static void Swap12_6(
        exchangePerson& first,
        exchangePerson& second) {

        if (&first == &second) {
            return;
        }

        std::lock(first.mtx, second.mtx);

        lock_guard<mutex> lockFirst(first.mtx, adopt_lock);
        lock_guard<mutex> lockSecond(second.mtx, adopt_lock);

        swap(first.data, second.data);
    }

    // 1.2.7 — unique_lock + defer_lock
    static void Swap12_7(
        exchangePerson& first,
        exchangePerson& second) {

        if (&first == &second) {
            return;
        }

        unique_lock<mutex> lockFirst(first.mtx, defer_lock);
        unique_lock<mutex> lockSecond(second.mtx, defer_lock);

        std::lock(lockFirst, lockSecond);

        swap(first.data, second.data);
    }
};



void Task_1_2_6() {
    completed6 = 0;

    exchangePerson person1("Initial", "One", "Address A", 20);
    exchangePerson person2("Initial", "Two", "Address B", 30);

    cout << "До змін:" << endl;
    person1.Print();
    person2.Print();

    thread t1(exchangePerson::JohnDoe, ref(person1));
    thread t2(exchangePerson::JacobSmith, ref(person2));

    t1.detach();
    t2.detach();

    // Чекаємо завершення обох від'єднаних потоків.
    while (completed6.load() < 2) {
        this_thread::yield();
    }

    thread swapThread(
        exchangePerson::Swap12_6,
        ref(person1),
        ref(person2)
    );

    swapThread.join();

    cout << "Після змін і Swap:" << endl;
    person1.Print();
    person2.Print();
}

void Task_1_2_7() {
    exchangePerson person1("John", "Doe", "Unknown", 120);
    exchangePerson person2("Jacob", "Smith", "Known", 1);

    cout << "До Swap:" << endl;
    person1.Print();
    person2.Print();

    thread swapThread(
        exchangePerson::Swap12_7,
        ref(person1),
        ref(person2)
    );

    swapThread.join();

    cout << "Після Swap:" << endl;
    person1.Print();
    person2.Print();
}

// ======================================================
// 1.2.8 — демонстрація уникнення deadlock
// ======================================================
void Task_1_2_8() {
    cout << "Для уникнення deadlock потрібно узгоджено "
        << "захоплювати декілька mutex." << endl;

    cout << "std::lock намагається захопити всі передані "
        << "м'ютекси без взаємного блокування." << endl;

    cout << "lock_guard автоматично звільняє mutex, "
        << "а unique_lock із defer_lock дозволяє "
        << "відкласти його захоплення." << endl;

    Task_1_2_7();
}

// ======================================================
// Головна функція — вибір завдання через меню
// ======================================================
int main() {
    

    SetConsoleCP(1251);
	SetConsoleOutputCP(1251);

    cout << "Лабораторна робота №1" << endl;
    cout << "Оберіть завдання:" << endl;
    cout << "1 - Завдання 1.2.1 (очікується terminate)" << endl;
    cout << "2 - Завдання 1.2.2 (detach)" << endl;
    cout << "3 - Завдання 1.2.3 (без mutex)" << endl;
    cout << "4 - Завдання 1.2.4 (з mutex)" << endl;
    cout << "5 - Завдання 1.2.5 (20 потоків)" << endl;
    cout << "6 - Завдання 1.2.6 (adopt_lock)" << endl;
    cout << "7 - Завдання 1.2.7 (defer_lock)" << endl;
    cout << "8 - Завдання 1.2.8 (deadlock)" << endl;
    cout << "Ваш вибір: ";

    int choice;
    if (!(cin >> choice)) {
        cout << "Помилка введення." << endl;
        return 1;
    }

    switch (choice) {
    case 1: Task_1_2_1(); break;
    case 2: Task_1_2_2(); break;
    case 3: Task_1_2_3(); break;
    case 4: Task_1_2_4(); break;
    case 5: Task_1_2_5(); break;
    case 6: Task_1_2_6(); break;
    case 7: Task_1_2_7(); break;
    case 8: Task_1_2_8(); break;
    default:
        cout << "Невідомий пункт меню." << endl;
        return 1;
    }

    if (choice != 1 && choice != 2) {
        cout << "Завдання завершено." << endl;
    }

    return 0;
}
