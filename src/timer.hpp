#pragma once

#define FMT_HEADER_ONLY
#include "fmt/format.h"
#include "fmt/color.h"

#include <iostream>
#include <chrono>
#include <vector>
#include <map>
#include <numeric>
#include <iomanip>
#include <functional>
#include <memory>
#include <algorithm>
//#include <omp.h>


#define _FUNC_ __FUNCTION__


class Timer
{
    std::chrono::high_resolution_clock hrc;
    struct Entry
    {
        std::string name, fullName;
        int count;
        float time;
        Entry* mommy = nullptr;
        std::vector<Entry*> children;
        std::chrono::time_point<decltype(hrc)> startTime;
    };
    std::map<std::string, Entry*> entries;
    Entry* current = nullptr;

public:
    Timer()
    {
        current = new Entry{ "" };
    }
    ~Timer()
    {
        std::function<void(Entry*)> cleanup = [&](Entry* e) {
            for (auto& a : e->children) { cleanup(a); a = nullptr; }
        };
        cleanup(current);
        delete current;
        current = nullptr;
    }
    void start(std::string cat)
    {
        auto fullName = current->fullName + "/" + cat;
        Entry* e;
        if (!(e = entries[fullName]))
        {
            e = new Entry{ cat,fullName,0,0,current };
            entries[fullName] = e;
            current->children.push_back(e);
        }
        e->startTime = hrc.now();
        current = e;
    }
    float end()
    {
        using namespace std::chrono;
        if (!current->mommy)
        {
            fmt::print(fg(fmt::color::orange), "WARNING: Timer stopped more often than started.\n");
            return -1;
        }
        auto end = hrc.now();
        auto passedSeconds = 1e-9f * duration_cast<nanoseconds>(end - current->startTime).count();
        current->count++;
        current->time += passedSeconds;
        current = current->mommy;
        return passedSeconds;
    }
    void print() const
    {
        using namespace std;
        fmt::print("\n{}\n", string(83, '='));

        fmt::color rowCols[] = { fmt::color::black, fmt::color::dark_slate_gray };
        int rowIdx = 0;
        fmt::print(bg(fmt::color::teal),
                   "{:<46} : {:>8} | {:>10} | {:>10}", "Function", "Count", "Time [s]", "Time/Call");
        fmt::print(bg(rowCols[0]), "\n");
        std::function<void(Entry*, int, bool)> printEntry = [&](Entry* e, int level, bool lastChild)
        {
            if (!e->fullName.empty()) {
                fmt::print(bg(rowCols[(rowIdx++) % 2]), "{:<46} : {:>8} | {:>10.6f} | {:>10.6f}",
                           std::string(2 * std::max(0, level - 1), ' ') +
                           (level ? lastChild ? "`-" : "|-" : "") + e->name,
                           e->count, e->time, (e->time / e->count));

                fmt::print(bg(rowCols[0]), "\n");
            }
            for (int i = 0;i<e->children.size();++i)
                printEntry(e->children[i], level + 1, i == e->children.size()-1);
        };
        Entry* root = current; while (root->mommy) { root = root->mommy; }
        printEntry(root, -1, false);

        fmt::print("{}\n", string(83, '='));
    }
};
class AutoTimer
{
    Timer* timer = nullptr;
public:

    AutoTimer(Timer& t, std::string cat)
    {
//		if (omp_in_parallel())
//			return; // Timer is not thread safe. Avoid UB.
        timer = std::addressof(t);
        timer->start(cat);
    }
    ~AutoTimer()
    {
        if (timer)
            timer->end();
        timer = nullptr;
    }
};


static Timer g_timer;

