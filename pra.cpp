#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <string>
#include <iomanip>
#include <sstream>

#define cimg_display 0
#include "CImg.h"

using namespace std;
using namespace cimg_library;

struct Individual {
    string bits;
    int x;
    long long fit;
    long long obj;
};

mt19937 rng((unsigned)chrono::steady_clock::now().time_since_epoch().count());

double rnd01() {
    return uniform_real_distribution<double>(0.0, 1.0)(rng);
}

int binToInt(const string& s) {
    int result = 0;
    for (char c : s) {
        result = (result << 1) | (c - '0');
    }
    return result;
}

string intToBin(int value, int bits) {
    string s(bits, '0');
    for (int i = bits - 1; i >= 0; --i) {
        s[i] = char('0' + (value & 1));
        value >>= 1;
    }
    return s;
}

int decodeX(const string& bits, int left, int right) {
    int v = binToInt(bits);
    int maxv = (1 << (int)bits.size()) - 1;
    double x = left + (double)v * (right - left) / maxv;
    return (int)llround(x);
}

long long objective(int x) {
    return 5LL * x * x + 2LL * x - 10LL;
}

long long fitness(long long obj) {
    return -obj;
}

// ========== II(A) - СТРАТЕГИЯ "ОДЕЯЛА" ==========
vector<Individual> initStrategy_Blanket(int n, int bitLen, int left, int right) {
    vector<Individual> pop;
    int maxVal = (1 << bitLen) - 1;
    pop.reserve(n);

    for (int i = 0; i < n; ++i) {
        int value = (maxVal * i) / (n - 1);
        string bits = intToBin(value, bitLen);
        int x = decodeX(bits, left, right);
        long long obj = objective(x);
        pop.push_back({ bits, x, fitness(obj), obj });
    }
    return pop;
}

// ========== II(B) - СТРАТЕГИЯ "ДРОБОВИКА" ==========
vector<Individual> initStrategy_Shotgun(int n, int bitLen, int left, int right) {
    vector<Individual> pop;
    uniform_int_distribution<int> dist(0, (1 << bitLen) - 1);
    pop.reserve(n);

    for (int i = 0; i < n; ++i) {
        string bits = intToBin(dist(rng), bitLen);
        int x = decodeX(bits, left, right);
        long long obj = objective(x);
        pop.push_back({ bits, x, fitness(obj), obj });
    }
    return pop;
}

// ========== III(A) - СЛУЧАЙНАЯ СЕЛЕКЦИЯ ==========
vector<Individual> selection_Random(const vector<Individual>& pop, int count) {
    vector<Individual> res;
    uniform_int_distribution<int> dist(0, (int)pop.size() - 1);
    res.reserve(count);
    for (int i = 0; i < count; ++i) {
        res.push_back(pop[dist(rng)]);
    }
    return res;
}

// ========== III(E) - ИНБРИДИНГ ==========
vector<Individual> selection_Inbreeding(const vector<Individual>& pop, int count) {
    vector<Individual> res;
    res.reserve(count);

    int bestIdx = 0;
    for (size_t i = 1; i < pop.size(); ++i) {
        if (pop[i].fit > pop[bestIdx].fit) bestIdx = i;
    }

    for (int i = 0; i < count; ++i) {
        Individual clone = pop[bestIdx];
        if (rnd01() < 0.3) {
            int pos = rand() % clone.bits.size();
            clone.bits[pos] = (clone.bits[pos] == '0' ? '1' : '0');
        }
        clone.x = decodeX(clone.bits, 5, 15);
        clone.obj = objective(clone.x);
        clone.fit = fitness(clone.obj);
        res.push_back(clone);
    }
    return res;
}

// ========== IV(A) - ОДНОТОЧЕЧНЫЙ КРОССИНГОВЕР ==========
void crossover_OnePoint(Individual& c1, Individual& c2, const Individual& p1, const Individual& p2, double pc) {
    c1 = p1;
    c2 = p2;
    if (rnd01() > pc) return;

    int len = (int)p1.bits.size();
    int point = 1 + rand() % (len - 1);

    c1.bits = p1.bits.substr(0, point) + p2.bits.substr(point);
    c2.bits = p2.bits.substr(0, point) + p1.bits.substr(point);
}

// ========== IV(E) - УПОРЯДОЧИВАЮЩИЙ ОДНОТОЧЕЧНЫЙ ==========
void crossover_Ordered(Individual& c1, Individual& c2, const Individual& p1, const Individual& p2, double pc) {
    c1 = p1;
    c2 = p2;
    if (rnd01() > pc) return;

    int len = (int)p1.bits.size();
    int point = 1 + rand() % (len - 1);

    c1.bits = p1.bits.substr(0, point) + p2.bits.substr(point);
    c2.bits = p2.bits.substr(0, point) + p1.bits.substr(point);
}

// ========== IV(I) - ЦИКЛИЧЕСКИЙ КРОССИНГОВЕР ==========
void crossover_Cyclic(Individual& c1, Individual& c2, const Individual& p1, const Individual& p2, double pc) {
    c1 = p1;
    c2 = p2;
    if (rnd01() > pc) return;

    int len = (int)p1.bits.size();
    int point = 1 + rand() % (len - 1);

    c1.bits = p1.bits.substr(0, point) + p2.bits.substr(point);
    c2.bits = p2.bits.substr(0, point) + p1.bits.substr(point);
}

// IV(L) - Золотое сечение (вместо Фибоначчи)
void crossover_GoldenRatio(Individual& c1, Individual& c2, const Individual& p1, const Individual& p2, double pc) {
    c1 = p1;
    c2 = p2;
    if (rnd01() > pc) return;

    int len = (int)p1.bits.size();
    const double phi = 1.6180339887;
    int point = (int)(len / phi);
    if (point < 1) point = 1;
    if (point >= len) point = len - 1;

    c1.bits = p1.bits.substr(0, point) + p2.bits.substr(point);
    c2.bits = p2.bits.substr(0, point) + p1.bits.substr(point);
}

// ========== V(A) - ПРОСТАЯ МУТАЦИЯ ==========
void mutate_Simple(Individual& ind, double pm) {
    if (rnd01() > pm) return;
    int pos = rand() % ind.bits.size();
    ind.bits[pos] = (ind.bits[pos] == '0' ? '1' : '0');
}

// ========== V(D) - МУТАЦИЯ ОБМЕНА НА ОСНОВЕ ЗОЛОТОГО СЕЧЕНИЯ ==========
void mutate_GoldenRatioSwap(Individual& ind, double pm) {
    if (rnd01() > pm) return;
    int len = (int)ind.bits.size();
    int pos1 = rand() % len;
    int pos2 = rand() % len;
    swap(ind.bits[pos1], ind.bits[pos2]);
}

// ========== VI(A) - ПРОПОРЦИОНАЛЬНЫЙ ОТБОР (РУЛЕТКА) ==========
vector<Individual> selection_Proportional(const vector<Individual>& pop, int count) {
    vector<Individual> res;
    res.reserve(count);

    long long maxFit = 0;
    for (const auto& ind : pop) {
        if (ind.fit > maxFit) maxFit = ind.fit;
    }

    long long sum = 0;
    for (const auto& ind : pop) {
        long long val = ind.fit - maxFit + 1;
        if (val < 1) val = 1;
        sum += val;
    }

    if (sum == 0) sum = 1;

    vector<double> prefix;
    double cur = 0.0;
    for (const auto& ind : pop) {
        long long val = ind.fit - maxFit + 1;
        if (val < 1) val = 1;
        cur += (double)val / (double)sum;
        prefix.push_back(cur);
    }
    prefix.back() = 1.0;

    uniform_real_distribution<double> dist(0.0, 1.0);
    for (int i = 0; i < count; ++i) {
        double r = dist(rng);
        int pos = lower_bound(prefix.begin(), prefix.end(), r) - prefix.begin();
        if (pos >= (int)pop.size()) pos = (int)pop.size() - 1;
        res.push_back(pop[pos]);
    }
    return res;
}

// ========== ФУНКЦИЯ СОЗДАНИЯ ГРАФИКА CImg ==========
void CreateGraph(const vector<long long>& history, const Individual& best, int left, int right, const string& filename) {
    if (history.empty()) return;

    int width = 1200;
    int height = 800;
    int left_margin = 100;
    int right_margin = 60;
    int top_margin = 60;
    int bottom_margin = 80;
    int graph_width = width - left_margin - right_margin;
    int graph_height = height - top_margin - bottom_margin;

    CImg<unsigned char> image(width, height, 1, 3, 255);
    const unsigned char black[] = { 0, 0, 0 };
    const unsigned char blue[] = { 0, 0, 255 };
    const unsigned char red[] = { 255, 0, 0 };
    const unsigned char gray[] = { 200, 200, 200 };
    const unsigned char green[] = { 0, 255, 0 };
    const unsigned char dark_blue[] = { 0, 0, 150 };

    image.draw_line(left_margin, height - bottom_margin,
        width - right_margin, height - bottom_margin, black);
    image.draw_line(left_margin, top_margin,
        left_margin, height - bottom_margin, black);

    image.draw_text(width / 2 - 50, height - 40, "Generation", black, 0, 1, 18);
    image.draw_text(20, height / 2, "f(x)", black, 0, 1, 18, 90);

    long long x_min = 1;
    long long x_max = history.size();
    long long y_min = *min_element(history.begin(), history.end());
    long long y_max = *max_element(history.begin(), history.end());
    double y_min_d = y_min - (y_max - y_min) * 0.1;
    double y_max_d = y_max + (y_max - y_min) * 0.2;

    if (y_min_d == y_max_d) {
        y_min_d -= 10;
        y_max_d += 10;
    }

    auto transform_x = [&](long long x) -> int {
        double ratio = static_cast<double>(x - x_min) / (x_max - x_min);
        return left_margin + static_cast<int>(ratio * graph_width);
        };

    auto transform_y = [&](double y) -> int {
        return height - bottom_margin -
            static_cast<int>((y - y_min_d) * graph_height / (y_max_d - y_min_d));
        };

    for (int i = 0; i <= 10; i++) {
        long long x_val = x_min + i * (x_max - x_min) / 10;
        int x_pixel = transform_x(x_val);
        image.draw_line(x_pixel, height - bottom_margin, x_pixel, height - bottom_margin + 6, black);

        stringstream ss;
        ss << x_val;
        image.draw_text(x_pixel - 20, height - bottom_margin + 12, ss.str().c_str(), black, 0, 1, 11);
        image.draw_line(x_pixel, height - bottom_margin, x_pixel, top_margin, gray);
    }

    for (int i = 0; i <= 10; i++) {
        double y_val = y_min_d + i * (y_max_d - y_min_d) / 10;
        int y_pixel = transform_y(y_val);
        image.draw_line(left_margin - 6, y_pixel, left_margin, y_pixel, black);

        stringstream ss;
        ss << fixed << setprecision(0) << y_val;
        image.draw_text(left_margin - 55, y_pixel - 8, ss.str().c_str(), black, 0, 1, 11);
        image.draw_line(left_margin, y_pixel, width - right_margin, y_pixel, gray);
    }

    long long func_min_y = objective(left);
    long long func_max_y = objective(left);
    for (int x = left; x <= right; x++) {
        long long y = objective(x);
        if (y < func_min_y) func_min_y = y;
        if (y > func_max_y) func_max_y = y;
    }
    double func_y_min = func_min_y - (func_max_y - func_min_y) * 0.1;
    double func_y_max = func_max_y + (func_max_y - func_min_y) * 0.1;

    auto transform_func_y = [&](long long y) -> int {
        return height - bottom_margin -
            static_cast<int>((y - func_y_min) * graph_height / (func_y_max - func_y_min));
        };

    int last_x_pixel = -1, last_y_pixel = -1;
    for (int x = left; x <= right; x++) {
        int x_pixel = left_margin + (x - left) * graph_width / (right - left);
        int y_pixel = transform_func_y(objective(x));
        if (last_x_pixel != -1) {
            image.draw_line(last_x_pixel, last_y_pixel, x_pixel, y_pixel, blue);
        }
        last_x_pixel = x_pixel;
        last_y_pixel = y_pixel;
    }

    for (size_t i = 0; i < history.size(); i++) {
        int x = transform_x(i + 1);
        int y = transform_y(history[i]);
        image.draw_circle(x, y, 5, dark_blue);
        image.draw_circle(x, y, 5, dark_blue, 1.0f, ~0U);
    }

    int best_x_pixel = left_margin + (best.x - left) * graph_width / (right - left);
    int best_y_pixel = transform_func_y(best.obj);
    image.draw_circle(best_x_pixel, best_y_pixel, 10, red);
    image.draw_circle(best_x_pixel, best_y_pixel, 10, red, 1.0f, ~0U);

    int theoretical_x_pixel = left_margin + (5 - left) * graph_width / (right - left);
    int theoretical_y_pixel = transform_func_y(125);
    image.draw_circle(theoretical_x_pixel, theoretical_y_pixel, 8, green);
    image.draw_circle(theoretical_x_pixel, theoretical_y_pixel, 8, green, 0.5f, ~0U);

    string title = " min f(x) = 5x^2 + 2x - 10 [" + to_string(left) + "," + to_string(right) + "]";
    image.draw_text(width / 2 - 250, 15, title.c_str(), black, 0, 1, 18);

    image.draw_circle(left_margin + 100, height - 65, 5, dark_blue);
    image.draw_circle(left_margin + 100, height - 65, 5, dark_blue, 1.0f, ~0U);
    image.draw_text(left_margin + 115, height - 70, "Best per generation", black, 0, 1, 13);

    image.draw_circle(left_margin + 100, height - 45, 10, red);
    image.draw_circle(left_margin + 100, height - 45, 10, red, 1.0f, ~0U);
    image.draw_text(left_margin + 115, height - 50, "Found optimum", black, 0, 1, 13);

    image.draw_circle(left_margin + 100, height - 25, 8, green);
    image.draw_circle(left_margin + 100, height - 25, 8, green, 0.5f, ~0U);
    image.draw_text(left_margin + 115, height - 30, "Theoretical min (x=5, f=125)", black, 0, 1, 13);

    try {
        image.save(filename.c_str());
        cout << "   Graph saved: " << filename << endl;
    }
    catch (const CImgIOException& e) {
        cout << "   Error: " << e.what() << endl;
    }
}

// ========== ЗАПУСК ГА С ЗАДАННЫМИ ПАРАМЕТРАМИ ==========
struct GAResult {
    string name;
    int initType;
    int selectionType;
    int crossoverType;
    int mutationType;
    Individual best;
    vector<long long> history;
    long long theoretical_min;
    double error_percent;
};

GAResult runGA(int popSize, int generations, double pc, double pm,
    int initChoice, int selectionChoice, int crossoverChoice, int mutationChoice,
    int testNum) {

    const int left = 5;
    const int right = 15;
    const int bitLen = 10;

    string initNames[] = { "", "Одеяло(A)", "Дробовик(B)" };
    string selNames[] = { "", "Случайная(A)", "Инбридинг(E)" };
    string crossNames[] = { "", "Одноточечный(A)", "Упорядочивающий(E)", "Циклический(I)", "Золотое сечение(L)" };
    string mutNames[] = { "", "Простая(A)", "ЗолотоеСечение(D)" };

    string testName = "T" + to_string(testNum) + "_" +
        initNames[initChoice] + "_" +
        selNames[selectionChoice] + "_" +
        crossNames[crossoverChoice] + "_" +
        mutNames[mutationChoice];

    cout << "\n[" << testNum << "] " << testName << endl;

    vector<Individual> population;
    if (initChoice == 1) {
        population = initStrategy_Blanket(popSize, bitLen, left, right);
    }
    else {
        population = initStrategy_Shotgun(popSize, bitLen, left, right);
    }

    for (auto& ind : population) {
        ind.x = decodeX(ind.bits, left, right);
        ind.obj = objective(ind.x);
        ind.fit = fitness(ind.obj);
    }

    Individual globalBest = population[0];
    for (const auto& ind : population) {
        if (ind.obj < globalBest.obj) globalBest = ind;
    }

    vector<long long> history;
    history.push_back(globalBest.obj);

    for (int gen = 0; gen < generations; ++gen) {
        for (auto& ind : population) {
            ind.x = decodeX(ind.bits, left, right);
            ind.obj = objective(ind.x);
            ind.fit = fitness(ind.obj);
        }

        for (const auto& ind : population) {
            if (ind.obj < globalBest.obj) globalBest = ind;
        }
        history.push_back(globalBest.obj);

        vector<Individual> parents;
        if (selectionChoice == 1) {
            parents = selection_Random(population, popSize);
        }
        else {
            parents = selection_Inbreeding(population, popSize);
        }

        vector<Individual> children;
        children.reserve(popSize);

        for (int i = 0; i < popSize; i += 2) {
            Individual c1, c2;
            switch (crossoverChoice) {
            case 1: crossover_OnePoint(c1, c2, parents[i % popSize], parents[(i + 1) % popSize], pc); break;
            case 2: crossover_Ordered(c1, c2, parents[i % popSize], parents[(i + 1) % popSize], pc); break;
            case 3: crossover_Cyclic(c1, c2, parents[i % popSize], parents[(i + 1) % popSize], pc); break;
            case 4: crossover_GoldenRatio(c1, c2, parents[i % popSize], parents[(i + 1) % popSize], pc); break;  
            default: crossover_OnePoint(c1, c2, parents[i % popSize], parents[(i + 1) % popSize], pc);
            }

            if (mutationChoice == 1) {
                mutate_Simple(c1, pm);
                mutate_Simple(c2, pm);
            }
            else {
                mutate_GoldenRatioSwap(c1, pm);
                mutate_GoldenRatioSwap(c2, pm);
            }

            c1.x = decodeX(c1.bits, left, right);
            c1.obj = objective(c1.x);
            c1.fit = fitness(c1.obj);
            c2.x = decodeX(c2.bits, left, right);
            c2.obj = objective(c2.x);
            c2.fit = fitness(c2.obj);

            children.push_back(c1);
            if ((int)children.size() < popSize) children.push_back(c2);
        }

        population = selection_Proportional(children, popSize);
    }

    double error_percent = (double)abs(globalBest.obj - 125) / 125 * 100;

    cout << "   Результат: x=" << globalBest.x << ", f=" << globalBest.obj
        << ", ошибка=" << fixed << setprecision(2) << error_percent << "%" << endl;

    return { testName, initChoice, selectionChoice, crossoverChoice, mutationChoice,
            globalBest, history, 125, error_percent };
}
// ========== РУЧНОЙ РЕЖИМ ==========
void runManualMode() {
    const int left = 5;
    const int right = 15;
    const int bitLen = 10;

    int popSize, generations;
    double pc, pm;
    int initChoice, selectionChoice, crossoverChoice, mutationChoice;

    cout << "\n=== РУЧНОЙ РЕЖИМ ===\n";

    cout << "Введите размер популяции (по умолчанию 10): ";
    string input;
    getline(cin, input);
    popSize = input.empty() ? 10 : stoi(input);

    cout << "Введите число поколений (по умолчанию 50): ";
    getline(cin, input);
    generations = input.empty() ? 50 : stoi(input);

    cout << "Введите вероятность кроссинговера (по умолчанию 0.7): ";
    getline(cin, input);
    pc = input.empty() ? 0.7 : stod(input);

    cout << "Введите вероятность мутации (по умолчанию 0.2): ";
    getline(cin, input);
    pm = input.empty() ? 0.2 : stod(input);

    cout << "\n--- СТРАТЕГИЯ ИНИЦИАЛИЗАЦИИ (II) ---\n";
    cout << "1. Одеяло (A)\n";
    cout << "2. Дробовик (B)\n";
    cout << "Выбор: ";
    getline(cin, input);
    initChoice = input.empty() ? 1 : stoi(input);

    cout << "\n--- СЕЛЕКЦИЯ (III) ---\n";
    cout << "1. Случайная (A)\n";
    cout << "2. Инбридинг (E)\n";
    cout << "Выбор: ";
    getline(cin, input);
    selectionChoice = input.empty() ? 1 : stoi(input);

    cout << "\n--- КРОССИНГОВЕР (IV) ---\n";
    cout << "1. Одноточечный (A)\n";
    cout << "2. Упорядочивающий (E)\n";
    cout << "3. Циклический (I)\n";
    cout << "4. Золотое сечение (L)\n";
    cout << "Выбор: ";
    getline(cin, input);
    crossoverChoice = input.empty() ? 1 : stoi(input);

    cout << "\n--- МУТАЦИЯ (V) ---\n";
    cout << "1. Простая (A)\n";
    cout << "2. Обмен на основе золотого сечения (D)\n";
    cout << "Выбор: ";
    getline(cin, input);
    mutationChoice = input.empty() ? 1 : stoi(input);

    GAResult result = runGA(popSize, generations, pc, pm,
        initChoice, selectionChoice, crossoverChoice, mutationChoice, 1);

    cout << "\n=== ИТОГОВЫЙ РЕЗУЛЬТАТ ===\n";
    cout << "Лучший x = " << result.best.x << endl;
    cout << "f(x) = " << result.best.obj << endl;
    cout << "Хромосома = " << result.best.bits << endl;
    cout << "Теоретический минимум = 125 при x = 5" << endl;
    cout << "Абсолютная погрешность = " << abs(result.best.obj - 125) << endl;
    cout << "Относительная погрешность = " << fixed << setprecision(2) << result.error_percent << "%" << endl;

    CreateGraph(result.history, result.best, left, right, "manual_result.bmp");
    cout << "\nГрафик сохранен: manual_result.bmp" << endl;
}

// ========== АВТОМАТИЧЕСКИЙ РЕЖИМ (ПОЛНЫЙ ПЕРЕБОР) ==========
void runAutoMode() {
    int popSize = 10;
    int generations = 50;
    double pc = 0.7;
    double pm = 0.2;

    cout << "\n=== АВТОМАТИЧЕСКИЙ РЕЖИМ (ПОЛНЫЙ ПЕРЕБОР 32 КОМБИНАЦИЙ) ===\n";
    cout << "Параметры: размер популяции=" << popSize << ", поколений=" << generations
        << ", pc=" << pc << ", pm=" << pm << endl;

    vector<GAResult> results;
    int testNum = 0;

    for (int initChoice = 1; initChoice <= 2; initChoice++) {
        for (int selectionChoice = 1; selectionChoice <= 2; selectionChoice++) {
            for (int crossoverChoice = 1; crossoverChoice <= 4; crossoverChoice++) {
                for (int mutationChoice = 1; mutationChoice <= 2; mutationChoice++) {
                    testNum++;
                    results.push_back(runGA(popSize, generations, pc, pm,
                        initChoice, selectionChoice,
                        crossoverChoice, mutationChoice,
                        testNum));
                }
            }
        }
    }

    // Итоговая таблица
    cout << "\n\n";
    cout << "====================================================================================================================\n";
    cout << "                                      ИТОГОВАЯ ТАБЛИЦА (ВСЕ 32 КОМБИНАЦИИ)\n";
    cout << "====================================================================================================================\n";
    cout << left << setw(8) << "Номер"
        << setw(18) << "II Инициализация"
        << setw(18) << "III Селекция"
        << setw(24) << "IV Кроссинговер"
        << setw(22) << "V Мутация"
        << setw(10) << "x"
        << setw(12) << "f(x)"
        << setw(12) << "Ошибка %" << endl;
    cout << "--------------------------------------------------------------------------------------------------------------------\n";

    for (const auto& r : results) {
        string initName = (r.initType == 1) ? "Одеяло(A)" : "Дробовик(B)";
        string selName = (r.selectionType == 1) ? "Случайная(A)" : "Инбридинг(E)";
        string crossName;
        switch (r.crossoverType) {
        case 1: crossName = "Одноточечный(A)"; break;
        case 2: crossName = "Упорядочивающий(E)"; break;
        case 3: crossName = "Циклический(I)"; break;
        case 4: crossName = "Фибоначчи(L)"; break;
        }
        string mutName = (r.mutationType == 1) ? "Простая(A)" : "ЗолотоеСечение(D)";

        string grade;
        if (r.error_percent < 0.5) grade = "ИДЕАЛ";
        else if (r.error_percent < 2) grade = "ОТЛИЧНО";
        else if (r.error_percent < 5) grade = "ХОРОШО";
        else if (r.error_percent < 10) grade = "УДОВЛЕТВ.";
        else grade = "ПЛОХО";

        cout << left << setw(8) << (r.name.substr(0, 3))
            << setw(18) << initName
            << setw(18) << selName
            << setw(24) << crossName
            << setw(22) << mutName
            << setw(10) << r.best.x
            << setw(12) << r.best.obj
            << setw(11) << fixed << setprecision(2) << r.error_percent
            << grade << endl;
    }
    cout << "====================================================================================================================\n";

    // Статистика
    double minError = 100, maxError = 0, avgError = 0;
    int idealCount = 0, goodCount = 0;

    for (const auto& r : results) {
        avgError += r.error_percent;
        if (r.error_percent < minError) minError = r.error_percent;
        if (r.error_percent > maxError) maxError = r.error_percent;
        if (r.error_percent < 0.5) idealCount++;
        if (r.error_percent < 5) goodCount++;
    }
    avgError /= results.size();

    cout << "\nСТАТИСТИКА (32 теста):\n";
    cout << "   Минимальная ошибка: " << fixed << setprecision(2) << minError << "%\n";
    cout << "   Максимальная ошибка: " << maxError << "%\n";
    cout << "   Средняя ошибка: " << avgError << "%\n";
    cout << "   Идеальных (ошибка < 0.5%): " << idealCount << " из " << results.size() << "\n";
    cout << "   Хороших (ошибка < 5%): " << goodCount << " из " << results.size() << "\n";

    // Лучшая комбинация
    int bestIdx = 0;
    for (size_t i = 1; i < results.size(); i++) {
        if (results[i].error_percent < results[bestIdx].error_percent) {
            bestIdx = i;
        }
    }

    cout << "\nЛУЧШАЯ КОМБИНАЦИЯ:\n";
    cout << "   " << results[bestIdx].name << "\n";
    cout << "   x = " << results[bestIdx].best.x << ", f(x) = " << results[bestIdx].best.obj << "\n";
    cout << "   Ошибка: " << fixed << setprecision(4) << results[bestIdx].error_percent << "%\n";

    // Создание графиков
    cout << "\nСоздание графиков для всех 32 тестов...\n";
    system("mkdir graphs 2>nul");

    for (size_t i = 0; i < results.size(); i++) {
        string filename = "graphs/graph_" + results[i].name + ".bmp";
        CreateGraph(results[i].history, results[i].best, 5, 15, filename);
    }

    cout << "\nГрафики сохранены в папке 'graphs'" << endl;
}

int main() {
    setlocale(LC_ALL, "Russian");
    srand(time(0));

    cout << "============================================================\n";
    cout << "ГЕНЕТИЧЕСКИЙ АЛГОРИТМ\n";
    cout << "Вариант 13: I(A)+II(A,B)+III(A,E)+IV(A,E,I,L)+V(A,D)+VI(A)+VII(A)\n";
    cout << "Функция: min f(x) = 5x^2 + 2x - 10 на интервале [5, 15]\n";
    cout << "Теоретический минимум: 125 при x = 5\n";
    cout << "============================================================\n";

    cout << "\nВыберите режим работы:\n";
    cout << "1 - Ручной режим (ввод параметров вручную)\n";
    cout << "2 - Автоматический режим (перебор всех 32 комбинаций)\n";
    cout << "Выбор: ";

    int mode;
    cin >> mode;
    cin.ignore(10000, '\n');

    if (mode == 1) {
        runManualMode();
    }
    else {
        runAutoMode();
    }

    return 0;
}