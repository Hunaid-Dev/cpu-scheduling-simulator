// ============================================================================
//  CPU SCHEDULING SIMULATOR  (C++ console version, no external libraries)
//  Data Structures Project - BCS-3J
//
//  Algorithms  : FCFS, SJF (non-preemptive), Round Robin, Priority (non-preemptive)
//  Data Structs: struct (Process, GanttEntry, SimResult),
//                vector (process list, Gantt chart),
//                queue  (ready queue for FCFS and Round Robin)
//
//  Priority rule: LOWER number = HIGHER priority (1 is the highest).
//  Compile: g++ cpu_scheduler_console.cpp -o scheduler
// ============================================================================

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <queue>
#include <string>
#include <vector>

using namespace std;

const int MAX_PROC = 20;

// ----------------------------------------------------------------------------
//  Data structures
// ----------------------------------------------------------------------------
struct Process {
    int id = 0;
    int arrival = 0;
    int burst = 0;
    int priority = 1;
    int remaining = 0;
    int completion = 0;
    int turnaround = 0;
    int waiting = 0;
};

struct GanttEntry {
    int pid;  // process id, or -1 for CPU idle
    int start;
    int end;
};

struct SimResult {
    string name;
    vector<Process> procs;
    vector<GanttEntry> gantt;
    double avgWT = 0, avgTAT = 0, cpuUtil = 0;
    int totalTime = 0;
};

// ----------------------------------------------------------------------------
//  Scheduling logic
// ----------------------------------------------------------------------------

// Adds a block to the Gantt chart (merges with previous block if same process).
static void addGantt(vector<GanttEntry>& g, int pid, int s, int e) {
    if (e <= s) return;
    if (!g.empty() && g.back().pid == pid && g.back().end == s) {
        g.back().end = e;
        return;
    }
    g.push_back({pid, s, e});
}

// Calculates turnaround time, waiting time and averages.
static void finalize(SimResult& r) {
    double sumW = 0, sumT = 0;
    for (auto& p : r.procs) {
        p.turnaround = p.completion - p.arrival;
        p.waiting = p.turnaround - p.burst;
        sumW += p.waiting;
        sumT += p.turnaround;
    }
    int n = (int)r.procs.size();
    r.avgWT = n ? sumW / n : 0;
    r.avgTAT = n ? sumT / n : 0;
    r.totalTime = r.gantt.empty() ? 0 : r.gantt.back().end;
    int busy = 0;
    for (auto& e : r.gantt)
        if (e.pid != -1) busy += e.end - e.start;
    r.cpuUtil = r.totalTime ? 100.0 * busy / r.totalTime : 0;
}

// First Come First Served - a queue is used as the ready queue.
static SimResult runFCFS(const vector<Process>& ps) {
    SimResult r;
    r.name = "FCFS";
    r.procs = ps;
    int n = (int)r.procs.size();

    vector<int> order(n);
    iota(order.begin(), order.end(), 0);
    stable_sort(order.begin(), order.end(),
                [&](int a, int b) { return r.procs[a].arrival < r.procs[b].arrival; });

    queue<int> q;
    for (int i : order) q.push(i);

    int time = 0;
    while (!q.empty()) {
        int i = q.front();
        q.pop();
        Process& p = r.procs[i];
        if (time < p.arrival) {  // CPU idle until process arrives
            addGantt(r.gantt, -1, time, p.arrival);
            time = p.arrival;
        }
        addGantt(r.gantt, p.id, time, time + p.burst);
        time += p.burst;
        p.completion = time;
    }
    finalize(r);
    return r;
}

// SJF (shortest burst) or Priority (smallest number), both non-preemptive.
static SimResult runNonPreemptive(const vector<Process>& ps, bool byPriority) {
    SimResult r;
    r.name = byPriority ? "Priority" : "SJF";
    r.procs = ps;
    int n = (int)r.procs.size();

    vector<bool> done(n, false);
    int doneCount = 0, time = 0;

    while (doneCount < n) {
        int best = -1;
        for (int i = 0; i < n; i++) {
            if (done[i] || r.procs[i].arrival > time) continue;
            if (best == -1) {
                best = i;
                continue;
            }
            int a = byPriority ? r.procs[i].priority : r.procs[i].burst;
            int b = byPriority ? r.procs[best].priority : r.procs[best].burst;
            if (a < b || (a == b && r.procs[i].arrival < r.procs[best].arrival)) best = i;
        }

        if (best == -1) {  // nobody has arrived yet -> CPU idle
            int nextArrival = INT_MAX;
            for (int i = 0; i < n; i++)
                if (!done[i]) nextArrival = min(nextArrival, r.procs[i].arrival);
            addGantt(r.gantt, -1, time, nextArrival);
            time = nextArrival;
            continue;
        }

        Process& p = r.procs[best];
        addGantt(r.gantt, p.id, time, time + p.burst);
        time += p.burst;
        p.completion = time;
        done[best] = true;
        doneCount++;
    }
    finalize(r);
    return r;
}

// Round Robin - a queue is used as the ready queue.
static SimResult runRR(const vector<Process>& ps, int quantum) {
    SimResult r;
    r.name = "Round Robin (Q=" + to_string(quantum) + ")";
    r.procs = ps;
    int n = (int)r.procs.size();
    for (auto& p : r.procs) p.remaining = p.burst;

    vector<int> order(n);
    iota(order.begin(), order.end(), 0);
    stable_sort(order.begin(), order.end(),
                [&](int a, int b) { return r.procs[a].arrival < r.procs[b].arrival; });

    queue<int> q;
    int next = 0, time = 0, done = 0;

    while (done < n) {
        while (next < n && r.procs[order[next]].arrival <= time) q.push(order[next++]);

        if (q.empty()) {  // CPU idle
            if (next >= n) break;
            int t = r.procs[order[next]].arrival;
            addGantt(r.gantt, -1, time, t);
            time = t;
            continue;
        }

        int i = q.front();
        q.pop();
        Process& p = r.procs[i];
        int run = min(quantum, p.remaining);
        addGantt(r.gantt, p.id, time, time + run);
        time += run;
        p.remaining -= run;

        // processes that arrived during this slice enter the queue first
        while (next < n && r.procs[order[next]].arrival <= time) q.push(order[next++]);

        if (p.remaining > 0)
            q.push(i);
        else {
            p.completion = time;
            done++;
        }
    }
    finalize(r);
    return r;
}

// ----------------------------------------------------------------------------
//  Input helpers (with validation)
// ----------------------------------------------------------------------------
static int readInt(const string& prompt, int lo, int hi) {
    while (true) {
        cout << prompt;
        string s;
        if (!getline(cin, s)) {
            cout << "\nInput closed. Exiting.\n";
            exit(0);
        }
        size_t b = s.find_first_not_of(" \t\r");
        size_t e = s.find_last_not_of(" \t\r");
        if (b == string::npos) {
            cout << "  Invalid: input cannot be empty.\n";
            continue;
        }
        s = s.substr(b, e - b + 1);
        bool digitsOnly = all_of(s.begin(), s.end(), [](unsigned char c) { return isdigit(c); });
        if (!digitsOnly || s.size() > 6) {
            cout << "  Invalid: enter a whole number (no letters, signs or decimals).\n";
            continue;
        }
        int v = stoi(s);
        if (v < lo || v > hi) {
            cout << "  Invalid: value must be between " << lo << " and " << hi << ".\n";
            continue;
        }
        return v;
    }
}

static void enterProcesses(vector<Process>& procs) {
    procs.clear();
    int n = readInt("Number of processes (1-" + to_string(MAX_PROC) + "): ", 1, MAX_PROC);
    for (int i = 1; i <= n; i++) {
        cout << "\n--- Process P" << i << " ---\n";
        Process p;
        p.id = i;
        p.arrival = readInt("  Arrival time (0-999)  : ", 0, 999);
        p.burst = readInt("  Burst time   (1-999)  : ", 1, 999);
        p.priority = readInt("  Priority     (1-99, 1 = highest): ", 1, 99);
        p.remaining = p.burst;
        procs.push_back(p);
    }
    cout << "\n" << n << " process(es) saved.\n";
}

static void loadSample(vector<Process>& procs) {
    procs.clear();
    int data[5][3] = {{0, 8, 3}, {1, 4, 1}, {2, 9, 4}, {3, 5, 2}, {6, 2, 5}};
    for (int i = 0; i < 5; i++) {
        Process p;
        p.id = i + 1;
        p.arrival = data[i][0];
        p.burst = data[i][1];
        p.priority = data[i][2];
        p.remaining = p.burst;
        procs.push_back(p);
    }
    cout << "Sample data with 5 processes loaded.\n";
}

// ----------------------------------------------------------------------------
//  Output
// ----------------------------------------------------------------------------
static void showProcesses(const vector<Process>& procs) {
    if (procs.empty()) {
        cout << "No processes entered yet.\n";
        return;
    }
    cout << "\n+-----+---------+-------+----------+\n";
    cout << "| PID | Arrival | Burst | Priority |\n";
    cout << "+-----+---------+-------+----------+\n";
    for (const auto& p : procs)
        cout << "| P" << left << setw(2) << p.id << " | " << setw(7) << p.arrival << " | " << setw(5)
             << p.burst << " | " << setw(8) << p.priority << " |\n";
    cout << "+-----+---------+-------+----------+\n";
}

static string centerText(const string& s, int w) {
    int total = w - (int)s.size();
    if (total <= 0) return s;
    int left = total / 2;
    return string(left, ' ') + s + string(total - left, ' ');
}

// Prints the Gantt chart as text. Rows wrap after 10 blocks so it fits the console.
static void printGantt(const vector<GanttEntry>& g) {
    const int CELL = 7;        // width of one block (including the left '|')
    const int PER_ROW = 10;    // blocks per line
    cout << "\nGantt Chart:\n";
    for (size_t row = 0; row < g.size(); row += PER_ROW) {
        size_t last = min(g.size(), row + PER_ROW);
        string top = "  ", mid = "  ", times = "  ";
        for (size_t i = row; i < last; i++) {
            string label = (g[i].pid == -1) ? "idle" : "P" + to_string(g[i].pid);
            top += "+" + string(CELL - 1, '-');
            mid += "|" + centerText(label, CELL - 1);
            string t = to_string(g[i].start);
            times += t + string(max(1, CELL - (int)t.size()), ' ');
        }
        top += "+";
        mid += "|";
        times += to_string(g[last - 1].end);
        cout << top << "\n" << mid << "\n" << top << "\n" << times << "\n";
    }
}

static void printResult(const SimResult& r) {
    cout << "\n==================== " << r.name << " ====================\n";

    // Execution order (skip idle)
    cout << "Execution order: ";
    bool first = true;
    for (const auto& e : r.gantt) {
        if (e.pid == -1) continue;
        if (!first) cout << " -> ";
        cout << "P" << e.pid;
        first = false;
    }
    cout << "\n";

    printGantt(r.gantt);

    cout << "\n+-----+----+----+----+-----+-----+-----+\n";
    cout << "| PID | AT | BT | PR | CT  | TAT | WT  |\n";
    cout << "+-----+----+----+----+-----+-----+-----+\n";
    for (const auto& p : r.procs)
        cout << "| P" << left << setw(2) << p.id << " | " << setw(2) << p.arrival << " | " << setw(2)
             << p.burst << " | " << setw(2) << p.priority << " | " << setw(3) << p.completion << " | "
             << setw(3) << p.turnaround << " | " << setw(3) << p.waiting << " |\n";
    cout << "+-----+----+----+----+-----+-----+-----+\n";
    cout << "AT=Arrival  BT=Burst  PR=Priority  CT=Completion  TAT=Turnaround  WT=Waiting\n";

    cout << fixed << setprecision(2);
    cout << "\nAverage Waiting Time    : " << r.avgWT << "\n";
    cout << "Average Turnaround Time : " << r.avgTAT << "\n";
    cout << "CPU Utilization         : " << setprecision(1) << r.cpuUtil << "%\n";
}

static void compareAll(const vector<Process>& procs, int quantum) {
    SimResult res[4] = {runFCFS(procs), runNonPreemptive(procs, false), runRR(procs, quantum),
                        runNonPreemptive(procs, true)};
    const char* names[4] = {"FCFS", "SJF", "Round Robin", "Priority"};

    cout << "\n=========== Comparison (Round Robin quantum = " << quantum << ") ===========\n";
    cout << "+-------------+-----------+-----------+\n";
    cout << "| Algorithm   | Avg WT    | Avg TAT   |\n";
    cout << "+-------------+-----------+-----------+\n";
    cout << fixed << setprecision(2);
    int best = 0;
    for (int i = 0; i < 4; i++) {
        cout << "| " << left << setw(11) << names[i] << " | " << setw(9) << res[i].avgWT << " | "
             << setw(9) << res[i].avgTAT << " |\n";
        if (res[i].avgWT < res[best].avgWT) best = i;
    }
    cout << "+-------------+-----------+-----------+\n";
    cout << "Best algorithm (lowest average waiting time): " << names[best] << "\n";
}

// ----------------------------------------------------------------------------
//  Main menu
// ----------------------------------------------------------------------------
static void showMenu() {
    cout << "\n=========================================\n";
    cout << "        CPU SCHEDULING SIMULATOR\n";
    cout << "=========================================\n";
    cout << " 1. Enter processes\n";
    cout << " 2. Load sample data\n";
    cout << " 3. View processes\n";
    cout << " 4. Run FCFS\n";
    cout << " 5. Run SJF (non-preemptive)\n";
    cout << " 6. Run Round Robin\n";
    cout << " 7. Run Priority (non-preemptive)\n";
    cout << " 8. Compare all algorithms\n";
    cout << " 0. Exit\n";
    cout << "-----------------------------------------\n";
}

int main() {
    vector<Process> procs;

    while (true) {
        showMenu();
        int choice = readInt("Enter your choice: ", 0, 8);

        if (choice == 0) {
            cout << "Goodbye!\n";
            break;
        }
        if (choice == 1) {
            enterProcesses(procs);
            continue;
        }
        if (choice == 2) {
            loadSample(procs);
            continue;
        }
        if (choice == 3) {
            showProcesses(procs);
            continue;
        }

        // choices 4-8 need processes
        if (procs.empty()) {
            cout << "\nNo processes yet. Use option 1 or 2 first.\n";
            continue;
        }

        switch (choice) {
            case 4: printResult(runFCFS(procs)); break;
            case 5: printResult(runNonPreemptive(procs, false)); break;
            case 6: {
                int q = readInt("Enter time quantum (1-99): ", 1, 99);
                printResult(runRR(procs, q));
                break;
            }
            case 7: printResult(runNonPreemptive(procs, true)); break;
            case 8: {
                int q = readInt("Enter time quantum for Round Robin (1-99): ", 1, 99);
                compareAll(procs, q);
                break;
            }
        }
    }
    return 0;
}
