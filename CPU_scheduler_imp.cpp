#include <algorithm>
#include <cctype>
#include <climits>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

const int MAX_LIMIT = 50;

struct Task {
    int pid = 0;
    int at = 0;
    int bt = 0;
    int pr = 1;
    int rem = 0;
    int ct = 0;
    int tat = 0;
    int wt = 0;
    int rt = -1;
    int firstResponse = -1;
};

struct TimelineBlock {
    int id;  // -1 represents CPU idle
    int start;
    int end;
};

struct ExecutionReport {
    string algoTitle;
    vector<Task> tasks;
    vector<TimelineBlock> chart;
    double meanWT = 0.0;
    double meanTAT = 0.0;
    double meanRT = 0.0;
    double efficiency = 0.0;
    int span = 0;
};

// Merge consecutive blocks for same process
void recordBlock(vector<TimelineBlock>& list, int pid, int t1, int t2) {
    if (t2 <= t1) return;
    if (!list.empty() && list.back().id == pid && list.back().end == t1) {
        list.back().end = t2;
    } else {
        list.push_back({pid, t1, t2});
    }
}

// Compute metrics: TAT, WT, RT, and CPU Utilization
void computeMetrics(ExecutionReport& rep) {
    double totalWait = 0, totalTurn = 0, totalResp = 0;
    for (auto& item : rep.tasks) {
        item.tat = item.ct - item.at;
        item.wt = item.tat - item.bt;
        item.rt = item.firstResponse - item.at;
        totalWait += item.wt;
        totalTurn += item.tat;
        totalResp += item.rt;
    }

    int count = (int)rep.tasks.size();
    rep.meanWT = (count > 0) ? (totalWait / count) : 0.0;
    rep.meanTAT = (count > 0) ? (totalTurn / count) : 0.0;
    rep.meanRT = (count > 0) ? (totalResp / count) : 0.0;
    rep.span = rep.chart.empty() ? 0 : rep.chart.back().end;

    int activeTicks = 0;
    for (const auto& blk : rep.chart) {
        if (blk.id != -1) activeTicks += (blk.end - blk.start);
    }
    rep.efficiency = (rep.span > 0) ? (100.0 * activeTicks / rep.span) : 0.0;
}

// 1. FCFS Scheduling
ExecutionReport executeFCFS(const vector<Task>& inputs) {
    ExecutionReport rep;
    rep.algoTitle = "FCFS";
    rep.tasks = inputs;
    int n = (int)rep.tasks.size();

    vector<int> sequence(n);
    iota(sequence.begin(), sequence.end(), 0);
    stable_sort(sequence.begin(), sequence.end(), [&](int x, int y) {
        return rep.tasks[x].at < rep.tasks[y].at;
    });

    queue<int> readyQ;
    for (int idx : sequence) readyQ.push(idx);

    int clockTime = 0;
    while (!readyQ.empty()) {
        int cur = readyQ.front();
        readyQ.pop();
        Task& t = rep.tasks[cur];

        if (clockTime < t.at) {
            recordBlock(rep.chart, -1, clockTime, t.at);
            clockTime = t.at;
        }

        t.firstResponse = clockTime;
        recordBlock(rep.chart, t.pid, clockTime, clockTime + t.bt);
        clockTime += t.bt;
        t.ct = clockTime;
    }

    computeMetrics(rep);
    return rep;
}

// 2. Non-Preemptive SJF / Priority using priority_queue
ExecutionReport executeNonPreemptive(const vector<Task>& inputs, bool usePriority) {
    ExecutionReport rep;
    rep.algoTitle = usePriority ? "Priority (Non-Preemptive)" : "SJF (Non-Preemptive)";
    rep.tasks = inputs;
    int n = (int)rep.tasks.size();

    vector<int> order(n);
    iota(order.begin(), order.end(), 0);
    stable_sort(order.begin(), order.end(), [&](int a, int b) {
        return rep.tasks[a].at < rep.tasks[b].at;
    });

    // Custom comparator for ready min-heap
    auto comp = [&](int a, int b) {
        int vA = usePriority ? rep.tasks[a].pr : rep.tasks[a].bt;
        int vB = usePriority ? rep.tasks[b].pr : rep.tasks[b].bt;
        if (vA != vB) return vA > vB;
        return rep.tasks[a].at > rep.tasks[b].at;
    };
    priority_queue<int, vector<int>, decltype(comp)> pq(comp);

    int nextIdx = 0, currentTime = 0, resolved = 0;

    while (resolved < n) {
        while (nextIdx < n && rep.tasks[order[nextIdx]].at <= currentTime) {
            pq.push(order[nextIdx++]);
        }

        if (pq.empty()) {
            if (nextIdx >= n) break;
            int nextArrival = rep.tasks[order[nextIdx]].at;
            recordBlock(rep.chart, -1, currentTime, nextArrival);
            currentTime = nextArrival;
            continue;
        }

        int cur = pq.top();
        pq.pop();
        Task& t = rep.tasks[cur];

        t.firstResponse = currentTime;
        recordBlock(rep.chart, t.pid, currentTime, currentTime + t.bt);
        currentTime += t.bt;
        t.ct = currentTime;
        resolved++;
    }

    computeMetrics(rep);
    return rep;
}

// 3. Round Robin Scheduling
ExecutionReport executeRoundRobin(const vector<Task>& inputs, int q) {
    ExecutionReport rep;
    rep.algoTitle = "Round Robin (Q=" + to_string(q) + ")";
    rep.tasks = inputs;
    int n = (int)rep.tasks.size();

    for (auto& item : rep.tasks) item.rem = item.bt;

    vector<int> sortedIdx(n);
    iota(sortedIdx.begin(), sortedIdx.end(), 0);
    stable_sort(sortedIdx.begin(), sortedIdx.end(), [&](int a, int b) {
        return rep.tasks[a].at < rep.tasks[b].at;
    });

    queue<int> qList;
    int ptr = 0, timer = 0, completedCount = 0;

    while (completedCount < n) {
        while (ptr < n && rep.tasks[sortedIdx[ptr]].at <= timer) {
            qList.push(sortedIdx[ptr++]);
        }

        if (qList.empty()) {
            if (ptr >= n) break;
            int nextArrival = rep.tasks[sortedIdx[ptr]].at;
            recordBlock(rep.chart, -1, timer, nextArrival);
            timer = nextArrival;
            continue;
        }

        int currIdx = qList.front();
        qList.pop();
        Task& currTask = rep.tasks[currIdx];

        if (currTask.firstResponse == -1) currTask.firstResponse = timer;

        int slice = min(q, currTask.rem);
        recordBlock(rep.chart, currTask.pid, timer, timer + slice);
        timer += slice;
        currTask.rem -= slice;

        while (ptr < n && rep.tasks[sortedIdx[ptr]].at <= timer) {
            qList.push(sortedIdx[ptr++]);
        }

        if (currTask.rem > 0) {
            qList.push(currIdx);
        } else {
            currTask.ct = timer;
            completedCount++;
        }
    }

    computeMetrics(rep);
    return rep;
}

// 4. Preemptive Scheduling: SRTF (Preemptive SJF) and Preemptive Priority
ExecutionReport executePreemptive(const vector<Task>& inputs, bool usePriority) {
    ExecutionReport rep;
    rep.algoTitle = usePriority ? "Priority (Preemptive)" : "SRTF (Preemptive SJF)";
    rep.tasks = inputs;
    int n = (int)rep.tasks.size();

    for (auto& item : rep.tasks) item.rem = item.bt;

    int completed = 0, clock = 0;
    int lastPid = -1, blockStart = 0;

    while (completed < n) {
        int best = -1;
        for (int i = 0; i < n; i++) {
            if (rep.tasks[i].at <= clock && rep.tasks[i].rem > 0) {
                if (best == -1) {
                    best = i;
                } else {
                    int a = usePriority ? rep.tasks[i].pr : rep.tasks[i].rem;
                    int b = usePriority ? rep.tasks[best].pr : rep.tasks[best].rem;
                    if (a < b || (a == b && rep.tasks[i].at < rep.tasks[best].at)) {
                        best = i;
                    }
                }
            }
        }

        int currentPid = (best == -1) ? -1 : rep.tasks[best].pid;

        // Context switch or starting block
        if (currentPid != lastPid) {
            if (lastPid != -1 || (clock > blockStart)) {
                recordBlock(rep.chart, lastPid, blockStart, clock);
            }
            lastPid = currentPid;
            blockStart = clock;
        }

        if (best == -1) {
            clock++;
            continue;
        }

        Task& t = rep.tasks[best];
        if (t.firstResponse == -1) t.firstResponse = clock;

        t.rem--;
        clock++;

        if (t.rem == 0) {
            t.ct = clock;
            completed++;
        }
    }

    if (blockStart < clock) {
        recordBlock(rep.chart, lastPid, blockStart, clock);
    }

    computeMetrics(rep);
    return rep;
}

// Input validation helper
int getValidInteger(const string& msg, int minimum, int maximum) {
    while (true) {
        cout << msg;
        string raw;
        if (!getline(cin, raw)) {
            cout << "\nTerminating.\n";
            exit(0);
        }

        size_t first = raw.find_first_not_of(" \t\r\n");
        size_t last = raw.find_last_not_of(" \t\r\n");
        if (first == string::npos) {
            cout << "  Input cannot be empty.\n";
            continue;
        }

        raw = raw.substr(first, last - first + 1);
        bool onlyDigits = true;
        for (unsigned char ch : raw) {
            if (!isdigit(ch)) {
                onlyDigits = false;
                break;
            }
        }

        if (!onlyDigits || raw.length() > 9) {
            cout << "  Please enter a valid whole number.\n";
            continue;
        }

        long long val = stoll(raw);
        if (val < minimum || val > maximum) {
            cout << "  Value must be between " << minimum << " and " << maximum << ".\n";
            continue;
        }
        return static_cast<int>(val);
    }
}

void promptProcessDetails(vector<Task>& taskList) {
    taskList.clear();
    int count = getValidInteger("Number of processes (1-" + to_string(MAX_LIMIT) + "): ", 1, MAX_LIMIT);
    for (int i = 1; i <= count; i++) {
        cout << "\n--- Process P" << i << " ---\n";
        Task t;
        t.pid = i;
        t.at = getValidInteger("  Arrival time (0-999)  : ", 0, 999);
        t.bt = getValidInteger("  Burst time   (1-999)  : ", 1, 999);
        t.pr = getValidInteger("  Priority     (1-99, 1=highest): ", 1, 99);
        t.rem = t.bt;
        taskList.push_back(t);
    }
    cout << "\nSaved " << count << " processes.\n";
}

void loadDefaultBatch(vector<Task>& list) {
    list.clear();
    int matrix[5][3] = {{0, 8, 3}, {1, 4, 1}, {2, 9, 4}, {3, 5, 2}, {6, 2, 5}};
    for (int i = 0; i < 5; i++) {
        Task t;
        t.pid = i + 1;
        t.at = matrix[i][0];
        t.bt = matrix[i][1];
        t.pr = matrix[i][2];
        t.rem = t.bt;
        list.push_back(t);
    }
    cout << "Loaded 5 sample processes.\n";
}

// Read processes from file (format: Arrival Burst Priority per line)
void loadProcessesFromFile(vector<Task>& list) {
    cout << "Enter input filename (default: processes.txt): ";
    string fname;
    getline(cin, fname);
    size_t first = fname.find_first_not_of(" \t\r\n");
    size_t last = fname.find_last_not_of(" \t\r\n");
    if (first == string::npos) {
        fname = "processes.txt";
    } else {
        fname = fname.substr(first, last - first + 1);
    }

    ifstream fin(fname);
    if (!fin.is_open()) {
        cout << "Could not open file: " << fname << "\n";
        return;
    }

    vector<Task> parsed;
    string line;
    int lineNum = 0;
    int skipped = 0;
    bool limitHit = false;

    while (getline(fin, line)) {
        lineNum++;
        size_t b = line.find_first_not_of(" \t\r\n");
        if (b == string::npos || line[b] == '#') continue;

        if ((int)parsed.size() >= MAX_LIMIT) {
            limitHit = true;
            continue;
        }

        stringstream ss(line.substr(b));
        int at, bt, pr;
        string extra;
        if (!(ss >> at >> bt >> pr) || (ss >> extra)) {
            cout << "  Line " << lineNum << " skipped (expected: Arrival Burst Priority).\n";
            skipped++;
            continue;
        }
        if (at < 0 || at > 999 || bt < 1 || bt > 999 || pr < 1 || pr > 99) {
            cout << "  Line " << lineNum << " skipped (values out of range: AT 0-999, BT 1-999, PR 1-99).\n";
            skipped++;
            continue;
        }

        Task t;
        t.pid = (int)parsed.size() + 1;
        t.at = at;
        t.bt = bt;
        t.pr = pr;
        t.rem = t.bt;
        parsed.push_back(t);
    }
    fin.close();

    if (parsed.empty()) {
        cout << "No valid process records found in file.\n";
        return;
    }

    list = move(parsed);
    cout << "Successfully read " << list.size() << " processes from " << fname << "\n";
    if (limitHit) {
        cout << "Note: Capped at maximum of " << MAX_LIMIT << " processes.\n";
    }
    if (skipped > 0) {
        cout << "Skipped " << skipped << " invalid line(s).\n";
    }
}

void displayCurrentTable(const vector<Task>& list) {
    if (list.empty()) {
        cout << "No processes recorded.\n";
        return;
    }
    cout << "\n+-----+---------+-------+----------+\n";
    cout << "| PID | Arrival | Burst | Priority |\n";
    cout << "+-----+---------+-------+----------+\n";
    for (const auto& item : list) {
        cout << "| P" << left << setw(2) << item.pid << " | "
             << setw(7) << item.at << " | "
             << setw(5) << item.bt << " | "
             << setw(8) << item.pr << " |\n";
    }
    cout << "+-----+---------+-------+----------+\n";
}

string formatCenter(const string& text, int width) {
    int diff = width - (int)text.size();
    if (diff <= 0) return text;
    int padL = diff / 2;
    return string(padL, ' ') + text + string(diff - padL, ' ');
}

// Generate Gantt chart string
string buildGanttString(const vector<TimelineBlock>& blocks) {
    const int SLOT_W = 7;
    const int CHUNKS_PER_LINE = 10;
    stringstream ss;

    ss << "\nGantt Chart:\n";
    for (size_t rowStart = 0; rowStart < blocks.size(); rowStart += CHUNKS_PER_LINE) {
        size_t rowEnd = min(blocks.size(), rowStart + CHUNKS_PER_LINE);
        string border = "  ", labels = "  ", timestamps = "  ";

        for (size_t k = rowStart; k < rowEnd; k++) {
            string tag = (blocks[k].id == -1) ? "idle" : ("P" + to_string(blocks[k].id));
            border += "+" + string(SLOT_W - 1, '-');
            labels += "|" + formatCenter(tag, SLOT_W - 1);
            string startStr = to_string(blocks[k].start);
            timestamps += startStr + string(max(1, SLOT_W - (int)startStr.size()), ' ');
        }

        border += "+";
        labels += "|";
        timestamps += to_string(blocks[rowEnd - 1].end);

        ss << border << "\n" << labels << "\n" << border << "\n" << timestamps << "\n";
    }
    return ss.str();
}

// Render simulation results
void showSimulationOutput(const ExecutionReport& res, bool saveToFile = false) {
    stringstream out;
    out << "\n==================== " << res.algoTitle << " ====================\n";

    out << "Execution order: ";
    bool init = true;
    for (const auto& b : res.chart) {
        if (b.id == -1) continue;
        if (!init) out << " -> ";
        out << "P" << b.id;
        init = false;
    }
    out << "\n";

    out << buildGanttString(res.chart);

    out << "\n+-----+----+----+----+-----+-----+-----+-----+\n";
    out << "| PID | AT | BT | PR | CT  | TAT | WT  | RT  |\n";
    out << "+-----+----+----+----+-----+-----+-----+-----+\n";
    for (const auto& p : res.tasks) {
        out << "| P" << left << setw(2) << p.pid << " | "
            << setw(2) << p.at << " | "
            << setw(2) << p.bt << " | "
            << setw(2) << p.pr << " | "
            << setw(3) << p.ct << " | "
            << setw(3) << p.tat << " | "
            << setw(3) << p.wt << " | "
            << setw(3) << p.rt << " |\n";
    }
    out << "+-----+----+----+----+-----+-----+-----+-----+\n";
    out << "AT=Arrival BT=Burst PR=Priority CT=Completion TAT=Turnaround WT=Waiting RT=Response\n";

    out << fixed << setprecision(2);
    out << "\nAverage Waiting Time    : " << res.meanWT << "\n";
    out << "Average Turnaround Time : " << res.meanTAT << "\n";
    out << "Average Response Time   : " << res.meanRT << "\n";
    out << setprecision(1);
    out << "CPU Utilization         : " << res.efficiency << "%\n";
    out << setprecision(2);

    cout << out.str();

    if (saveToFile) {
        ofstream fout("report.txt", ios::app);
        if (fout.is_open()) {
            fout << out.str() << "\n\n";
            fout.close();
            cout << "Result appended to report.txt\n";
        }
    }
}

// Compare all supported scheduling algorithms side by side
void runAlgorithmComparison(const vector<Task>& procs, int quantumVal, bool saveToFile = false) {
    ExecutionReport runs[6] = {
        executeFCFS(procs),
        executeNonPreemptive(procs, false),
        executePreemptive(procs, false),
        executeRoundRobin(procs, quantumVal),
        executeNonPreemptive(procs, true),
        executePreemptive(procs, true)
    };
    const char* labels[6] = {
        "FCFS", "SJF (Non-P)", "SRTF (Preempt)",
        "Round Robin", "Priority (Non-P)", "Priority (Preempt)"
    };

    stringstream out;
    out << "\n=========== Algorithm Comparison (RR Quantum = " << quantumVal << ") ===========\n";
    out << "+--------------------+-----------+-----------+-----------+------------+\n";
    out << "| Algorithm          | Avg WT    | Avg TAT   | Avg RT    | CPU Util % |\n";
    out << "+--------------------+-----------+-----------+-----------+------------+\n";
    out << fixed;

    int optimum = 0;
    for (int idx = 0; idx < 6; idx++) {
        out << setprecision(2);
        out << "| " << left << setw(18) << labels[idx] << " | "
            << setw(9) << runs[idx].meanWT << " | "
            << setw(9) << runs[idx].meanTAT << " | "
            << setw(9) << runs[idx].meanRT << " | ";
        out << setprecision(1) << setw(10) << runs[idx].efficiency << " |\n";
        if (runs[idx].meanWT < runs[optimum].meanWT) {
            optimum = idx;
        }
    }
    out << setprecision(2);
    out << "+--------------------+-----------+-----------+-----------+------------+\n";
    out << "Best algorithm (lowest waiting time): " << labels[optimum] << "\n";

    cout << out.str();

    if (saveToFile) {
        ofstream fout("report.txt", ios::app);
        if (fout.is_open()) {
            fout << out.str() << "\n\n";
            fout.close();
            cout << "Comparison appended to report.txt\n";
        }
    }
}

void printApplicationMenu() {
    cout << "\n=========================================\n";
    cout << "        CPU SCHEDULING SIMULATOR\n";
    cout << "=========================================\n";
    cout << " 1. Enter processes manually\n";
    cout << " 2. Load sample data\n";
    cout << " 3. Read processes from text file\n";
    cout << " 4. View current processes\n";
    cout << " 5. Run FCFS\n";
    cout << " 6. Run SJF (Non-Preemptive)\n";
    cout << " 7. Run SRTF (Preemptive SJF)\n";
    cout << " 8. Run Round Robin\n";
    cout << " 9. Run Priority (Non-Preemptive)\n";
    cout << "10. Run Priority (Preemptive)\n";
    cout << "11. Compare all algorithms\n";
    cout << " 0. Exit\n";
    cout << "-----------------------------------------\n";
}

int main() {
    vector<Task> jobList;

    while (true) {
        printApplicationMenu();
        int userChoice = getValidInteger("Enter your choice: ", 0, 11);

        if (userChoice == 0) {
            cout << "Goodbye!\n";
            break;
        }
        if (userChoice == 1) {
            promptProcessDetails(jobList);
            continue;
        }
        if (userChoice == 2) {
            loadDefaultBatch(jobList);
            continue;
        }
        if (userChoice == 3) {
            loadProcessesFromFile(jobList);
            continue;
        }
        if (userChoice == 4) {
            displayCurrentTable(jobList);
            continue;
        }

        if (jobList.empty()) {
            cout << "\nNo processes loaded. Please choose option 1, 2, or 3 first.\n";
            continue;
        }

        bool save = (getValidInteger("Save result to report.txt? (1=Yes, 0=No): ", 0, 1) == 1);

        switch (userChoice) {
            case 5:
                showSimulationOutput(executeFCFS(jobList), save);
                break;
            case 6:
                showSimulationOutput(executeNonPreemptive(jobList, false), save);
                break;
            case 7:
                showSimulationOutput(executePreemptive(jobList, false), save);
                break;
            case 8: {
                int q = getValidInteger("Enter time quantum (1-99): ", 1, 99);
                showSimulationOutput(executeRoundRobin(jobList, q), save);
                break;
            }
            case 9:
                showSimulationOutput(executeNonPreemptive(jobList, true), save);
                break;
            case 10:
                showSimulationOutput(executePreemptive(jobList, true), save);
                break;
            case 11: {
                int q = getValidInteger("Enter time quantum for Round Robin (1-99): ", 1, 99);
                runAlgorithmComparison(jobList, q, save);
                break;
            }
        }
    }

    return 0;
}
