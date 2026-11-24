// CPU Scheduling Simulator - simple raylib version
// Algorithms: FCFS, SJF, Round Robin, Priority
// Note: for Priority, a LOWER number means HIGHER priority (1 is the best).

#include "raylib.h"
#include <vector>
#include <queue>
#include <string>
#include <cstdlib>
using namespace std;

// ------------------------------------------------------------
// Structs
// ------------------------------------------------------------
struct Process {
    int id;
    int arrival;
    int burst;
    int priority;
    int remaining;   // used only by Round Robin
    int completion;
    int turnaround;
    int waiting;
};

// one block of the Gantt chart (pid = -1 means the CPU was idle)
struct Block {
    int pid;
    int start;
    int end;
};

// ------------------------------------------------------------
// Global variables
// ------------------------------------------------------------
vector<Process> processes;   // processes entered by the user
vector<Process> results;     // copy of the processes used while running an algorithm
vector<Block> gantt;         // Gantt chart blocks

double avgWaiting = 0;
double avgTurnaround = 0;
bool hasResult = false;

int selectedAlgo = 0;        // 0 = FCFS, 1 = SJF, 2 = Round Robin, 3 = Priority
const char* algoNames[4] = {"FCFS", "SJF", "Round Robin", "Priority"};

string arrivalText = "";
string burstText = "";
string priorityText = "";
string quantumText = "2";
int activeBox = 0;           // 0 = arrival, 1 = burst, 2 = priority, 3 = quantum

int nextId = 1;
string message = "Add a process or press Sample.";
Color messageColor = LIGHTGRAY;

Color processColors[6] = {
    {231, 76, 60, 255}, {52, 152, 219, 255}, {39, 174, 96, 255},
    {191, 144, 0, 255}, {155, 89, 182, 255}, {230, 126, 34, 255}
};

// ---------- Theme colors (black and white) ----------
Color fieldGrey    = {40, 40, 40, 255};      // input box background
Color buttonGrey   = {55, 55, 55, 255};      // button background
Color selectedGrey = {85, 85, 85, 255};      // selected algorithm button
Color lineGrey     = {70, 70, 70, 255};      // divider lines
Color borderGrey   = {80, 80, 80, 255};      // border of an inactive input box
Color lightBlue    = {120, 180, 255, 255};   // border of the active input box
Color idleGrey     = {60, 60, 60, 255};      // idle block in the Gantt chart
Color errorColor   = {255, 120, 120, 255};   // error messages
Color okColor      = {130, 220, 150, 255};   // success messages

Color colorFor(int id) {
    return processColors[id % 6];
}

// ------------------------------------------------------------
// Scheduling part
// ------------------------------------------------------------

// add a block to the Gantt chart, join it with the last block if it is the same process
void addBlock(int pid, int start, int end) {
    if (end <= start) return;
    if (!gantt.empty() && gantt.back().pid == pid && gantt.back().end == start) {
        gantt.back().end = end;
        return;
    }
    Block b;
    b.pid = pid;
    b.start = start;
    b.end = end;
    gantt.push_back(b);
}

// bubble sort by arrival time
void sortByArrival(vector<Process>& v) {
    for (int i = 0; i < (int)v.size() - 1; i++) {
        for (int j = 0; j < (int)v.size() - 1 - i; j++) {
            if (v[j].arrival > v[j + 1].arrival) {
                Process temp = v[j];
                v[j] = v[j + 1];
                v[j + 1] = temp;
            }
        }
    }
}

// turnaround = completion - arrival,  waiting = turnaround - burst
void calculateTimes() {
    double totalWaiting = 0;
    double totalTurnaround = 0;
    for (int i = 0; i < (int)results.size(); i++) {
        results[i].turnaround = results[i].completion - results[i].arrival;
        results[i].waiting = results[i].turnaround - results[i].burst;
        totalWaiting += results[i].waiting;
        totalTurnaround += results[i].turnaround;
    }
    avgWaiting = totalWaiting / results.size();
    avgTurnaround = totalTurnaround / results.size();
}

void runFCFS() {
    results = processes;
    gantt.clear();
    sortByArrival(results);

    // ready queue: first in, first out
    queue<int> ready;
    for (int i = 0; i < (int)results.size(); i++) {
        ready.push(i);
    }

    int time = 0;
    while (!ready.empty()) {
        int i = ready.front();
        ready.pop();

        if (time < results[i].arrival) {          // CPU waits for the process
            addBlock(-1, time, results[i].arrival);
            time = results[i].arrival;
        }
        addBlock(results[i].id, time, time + results[i].burst);
        time = time + results[i].burst;
        results[i].completion = time;
    }
    calculateTimes();
}

// SJF picks the smallest burst, Priority picks the smallest priority number
void runSJFOrPriority(bool usePriority) {
    results = processes;
    gantt.clear();

    int n = results.size();
    vector<bool> finished(n, false);
    int finishedCount = 0;
    int time = 0;

    while (finishedCount < n) {
        // find the best process among those that already arrived
        int best = -1;
        for (int i = 0; i < n; i++) {
            if (finished[i] || results[i].arrival > time) continue;
            if (best == -1) {
                best = i;
                continue;
            }
            int valueI, valueBest;
            if (usePriority) {
                valueI = results[i].priority;
                valueBest = results[best].priority;
            } else {
                valueI = results[i].burst;
                valueBest = results[best].burst;
            }
            if (valueI < valueBest) best = i;
            else if (valueI == valueBest && results[i].arrival < results[best].arrival) best = i;
        }

        // nobody has arrived yet, so the CPU is idle until the next arrival
        if (best == -1) {
            int nextArrival = 1000000;
            for (int i = 0; i < n; i++) {
                if (!finished[i] && results[i].arrival < nextArrival) nextArrival = results[i].arrival;
            }
            addBlock(-1, time, nextArrival);
            time = nextArrival;
            continue;
        }

        addBlock(results[best].id, time, time + results[best].burst);
        time = time + results[best].burst;
        results[best].completion = time;
        finished[best] = true;
        finishedCount++;
    }
    calculateTimes();
}

void runRoundRobin(int quantum) {
    results = processes;
    gantt.clear();
    sortByArrival(results);

    int n = results.size();
    for (int i = 0; i < n; i++) {
        results[i].remaining = results[i].burst;
    }

    queue<int> ready;
    int next = 0;       // next process (in arrival order) that has not entered the queue yet
    int time = 0;
    int finishedCount = 0;

    while (finishedCount < n) {
        // put the processes that have arrived into the queue
        while (next < n && results[next].arrival <= time) {
            ready.push(next);
            next++;
        }

        // queue is empty, so jump to the next arrival
        if (ready.empty()) {
            addBlock(-1, time, results[next].arrival);
            time = results[next].arrival;
            continue;
        }

        int i = ready.front();
        ready.pop();

        int runTime = quantum;
        if (results[i].remaining < quantum) runTime = results[i].remaining;

        addBlock(results[i].id, time, time + runTime);
        time = time + runTime;
        results[i].remaining = results[i].remaining - runTime;

        // new arrivals go to the queue before the process that just ran
        while (next < n && results[next].arrival <= time) {
            ready.push(next);
            next++;
        }

        if (results[i].remaining > 0) {
            ready.push(i);
        } else {
            results[i].completion = time;
            finishedCount++;
        }
    }
    calculateTimes();
}

// ------------------------------------------------------------
// Actions behind the buttons
// ------------------------------------------------------------
void setMessage(string text, Color color) {
    message = text;
    messageColor = color;
}

void addProcess() {
    if (processes.size() >= 10) {
        setMessage("Maximum 10 processes.", errorColor);
        return;
    }
    if (burstText == "" || atoi(burstText.c_str()) <= 0) {
        setMessage("Burst time must be more than 0.", errorColor);
        return;
    }

    Process p;
    p.id = nextId;
    nextId++;
    p.arrival = (arrivalText == "") ? 0 : atoi(arrivalText.c_str());
    p.burst = atoi(burstText.c_str());
    p.priority = (priorityText == "") ? 1 : atoi(priorityText.c_str());
    p.remaining = p.burst;
    p.completion = 0;
    p.turnaround = 0;
    p.waiting = 0;
    processes.push_back(p);

    arrivalText = "";
    burstText = "";
    priorityText = "";
    activeBox = 0;
    hasResult = false;
    setMessage("Process added.", okColor);
}

void loadSample() {
    processes.clear();
    nextId = 1;
    int sample[5][3] = { {0, 8, 3}, {1, 4, 1}, {2, 9, 4}, {3, 5, 2}, {6, 2, 5} };
    for (int i = 0; i < 5; i++) {
        Process p;
        p.id = nextId;
        nextId++;
        p.arrival = sample[i][0];
        p.burst = sample[i][1];
        p.priority = sample[i][2];
        p.remaining = p.burst;
        p.completion = 0;
        p.turnaround = 0;
        p.waiting = 0;
        processes.push_back(p);
    }
    hasResult = false;
    setMessage("Sample data loaded.", okColor);
}

void clearAll() {
    processes.clear();
    nextId = 1;
    hasResult = false;
    setMessage("Everything cleared.", LIGHTGRAY);
}

void deleteProcess(int index) {
    processes.erase(processes.begin() + index);
    hasResult = false;
    setMessage("Process removed.", okColor);
}

void runSelected() {
    if (processes.empty()) {
        setMessage("Add some processes first.", errorColor);
        return;
    }
    if (selectedAlgo == 0) {
        runFCFS();
    } else if (selectedAlgo == 1) {
        runSJFOrPriority(false);
    } else if (selectedAlgo == 2) {
        int q = atoi(quantumText.c_str());
        if (q <= 0) {
            setMessage("Time quantum must be at least 1.", errorColor);
            return;
        }
        runRoundRobin(q);
    } else {
        runSJFOrPriority(true);
    }
    hasResult = true;
    setMessage(string(algoNames[selectedAlgo]) + " finished.", okColor);
}

// ------------------------------------------------------------
// Small GUI helpers
// ------------------------------------------------------------

// make a color a little lighter (used for the hover effect)
Color lighter(Color c) {
    Color result = c;
    result.r = c.r + 30;
    result.g = c.g + 30;
    result.b = c.b + 30;
    return result;
}

bool drawButton(int x, int y, int w, int h, const char* text, Color color, bool selected = false) {
    Rectangle r = {(float)x, (float)y, (float)w, (float)h};
    bool hover = CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRec(r, hover ? lighter(color) : color);
    if (selected) DrawRectangleLinesEx(r, 2, lightBlue);
    DrawText(text, x + (w - MeasureText(text, 18)) / 2, y + (h - 18) / 2, 18, WHITE);
    return hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

void drawInputBox(int x, int y, int w, int h, const char* label, string& text, int index) {
    Rectangle r = {(float)x, (float)y, (float)w, (float)h};
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), r)) {
        activeBox = index;
    }
    DrawText(label, x, y - 22, 16, WHITE);
    DrawRectangleRec(r, fieldGrey);
    DrawRectangleLinesEx(r, 2, (activeBox == index) ? lightBlue : borderGrey);
    DrawText(text.c_str(), x + 8, y + 7, 20, WHITE);
}

// read the keyboard and write into the box that is selected
void handleTyping() {
    string* text;
    if (activeBox == 0) text = &arrivalText;
    else if (activeBox == 1) text = &burstText;
    else if (activeBox == 2) text = &priorityText;
    else text = &quantumText;

    int key = GetCharPressed();
    while (key > 0) {
        if (key >= '0' && key <= '9' && text->size() < 3) {
            text->push_back((char)key);
        }
        key = GetCharPressed();
    }
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !text->empty()) {
        text->pop_back();
    }
}

// ------------------------------------------------------------
// Drawing the screen
// ------------------------------------------------------------
void drawLeftSide() {
    DrawText("1. Add a process", 20, 60, 20, WHITE);

    drawInputBox(20, 110, 90, 32, "Arrival", arrivalText, 0);
    drawInputBox(120, 110, 90, 32, "Burst", burstText, 1);
    drawInputBox(220, 110, 90, 32, "Priority", priorityText, 2);

    if (drawButton(20, 155, 90, 32, "Add", buttonGrey)) addProcess();
    if (drawButton(120, 155, 90, 32, "Sample", buttonGrey)) loadSample();
    if (drawButton(220, 155, 90, 32, "Clear", buttonGrey)) clearAll();

    DrawText(message.c_str(), 20, 200, 16, messageColor);

    // list of processes
    DrawText("ID", 32, 232, 16, WHITE);
    DrawText("Arrival", 90, 232, 16, WHITE);
    DrawText("Burst", 160, 232, 16, WHITE);
    DrawText("Priority", 225, 232, 16, WHITE);
    DrawLine(20, 252, 320, 252, lineGrey);

    int deleteIndex = -1;
    for (int i = 0; i < (int)processes.size(); i++) {
        int y = 260 + i * 26;
        DrawRectangle(20, y + 2, 6, 18, colorFor(processes[i].id));
        DrawText(TextFormat("P%d", processes[i].id), 32, y, 18, WHITE);
        DrawText(TextFormat("%d", processes[i].arrival), 90, y, 18, WHITE);
        DrawText(TextFormat("%d", processes[i].burst), 160, y, 18, WHITE);
        DrawText(TextFormat("%d", processes[i].priority), 225, y, 18, WHITE);
        if (drawButton(288, y, 26, 22, "x", buttonGrey)) deleteIndex = i;
        DrawLine(20, y + 25, 320, y + 25, lineGrey);
    }
    // delete after the loop so the vector does not change while we are drawing it
    if (deleteIndex != -1) deleteProcess(deleteIndex);

    DrawText("Lower priority number = higher priority", 20, 560, 15, LIGHTGRAY);
    DrawText("Click a box, then type numbers.", 20, 582, 15, LIGHTGRAY);
    DrawText("Press x to remove a process.", 20, 604, 15, LIGHTGRAY);
}

void drawAlgorithmChoice() {
    DrawText("2. Choose an algorithm", 350, 60, 20, WHITE);
    for (int i = 0; i < 4; i++) {
        Color c = (i == selectedAlgo) ? selectedGrey : buttonGrey;
        if (drawButton(350 + i * 155, 90, 145, 36, algoNames[i], c, i == selectedAlgo)) {
            selectedAlgo = i;
        }
    }

    drawInputBox(350, 170, 70, 32, "Time quantum (Round Robin only)", quantumText, 3);
    if (drawButton(440, 170, 120, 32, "RUN", buttonGrey)) runSelected();
}

void drawGantt() {
    DrawText("3. Gantt chart", 350, 225, 20, WHITE);

    if (!hasResult) {
        DrawText("Press RUN to see the chart.", 350, 262, 18, LIGHTGRAY);
        return;
    }

    int total = gantt.back().end;
    float scale = 620.0f / total;     // pixels for one unit of time

    for (int i = 0; i < (int)gantt.size(); i++) {
        float x = 350 + gantt[i].start * scale;
        float w = (gantt[i].end - gantt[i].start) * scale;

        Color c = idleGrey;
        if (gantt[i].pid != -1) c = colorFor(gantt[i].pid);
        DrawRectangle((int)x, 255, (int)w, 45, c);
        DrawRectangleLines((int)x, 255, (int)w, 45, LIGHTGRAY);

        // write the process name inside the block if there is enough space
        if (w >= 30) {
            if (gantt[i].pid == -1) DrawText("idle", (int)x + 5, 268, 18, WHITE);
            else DrawText(TextFormat("P%d", gantt[i].pid), (int)x + 8, 268, 20, WHITE);
        }
        // start time under the block
        DrawText(TextFormat("%d", gantt[i].start), (int)x, 305, 16, WHITE);
    }
    // end time of the last block
    DrawText(TextFormat("%d", total), (int)(350 + total * scale) - 5, 305, 16, WHITE);
}

void drawResults() {
    DrawText("4. Results", 350, 345, 20, WHITE);

    if (!hasResult) {
        DrawText("No results yet.", 350, 380, 18, LIGHTGRAY);
        return;
    }

    const char* heads[6] = {"Process", "Arrival", "Burst", "Completion", "Turnaround", "Waiting"};
    for (int i = 0; i < 6; i++) {
        DrawText(heads[i], 350 + i * 105, 375, 16, WHITE);
    }
    DrawLine(350, 395, 980, 395, lineGrey);

    for (int i = 0; i < (int)results.size(); i++) {
        int y = 402 + i * 24;
        DrawText(TextFormat("P%d", results[i].id), 350, y, 18, WHITE);
        DrawText(TextFormat("%d", results[i].arrival), 455, y, 18, WHITE);
        DrawText(TextFormat("%d", results[i].burst), 560, y, 18, WHITE);
        DrawText(TextFormat("%d", results[i].completion), 665, y, 18, WHITE);
        DrawText(TextFormat("%d", results[i].turnaround), 770, y, 18, WHITE);
        DrawText(TextFormat("%d", results[i].waiting), 875, y, 18, WHITE);
        DrawLine(350, y + 22, 980, y + 22, lineGrey);
    }

    DrawText(TextFormat("Average waiting time: %.2f", avgWaiting), 350, 655, 20, WHITE);
    DrawText(TextFormat("Average turnaround time: %.2f", avgTurnaround), 650, 655, 20, WHITE);
}

// ------------------------------------------------------------
// main
// ------------------------------------------------------------
int main() {
    InitWindow(1000, 700, "CPU Scheduling Simulator");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        handleTyping();
        if (IsKeyPressed(KEY_ENTER) && activeBox < 3) addProcess();

        BeginDrawing();
        ClearBackground(BLACK);

        DrawText("CPU Scheduling Simulator", 20, 15, 30, WHITE);
        DrawLine(20, 50, 980, 50, lineGrey);

        drawLeftSide();
        drawAlgorithmChoice();
        drawGantt();
        drawResults();

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
