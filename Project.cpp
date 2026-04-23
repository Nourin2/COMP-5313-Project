#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <condition_variable>
#include <fstream>
#include <cstdlib>
#include <ctime>

using namespace std;

// ==========================================
// RANDOM DELAY
// ==========================================
void random_delay() {
    for (volatile int i = 0; i < rand() % 1000; i++);
}

// ==========================================
// SEMAPHORE
// ==========================================
class Semaphore {
private:
    int value;
    mutex mtx;
    condition_variable cv;

public:
    Semaphore(int val) : value(val) {}

    void P() {
        unique_lock<mutex> lock(mtx);
        while (value <= 0) cv.wait(lock);
        value--;
    }

    void V() {
        unique_lock<mutex> lock(mtx);
        value++;
        cv.notify_one();
    }
};

// ==========================================
// GLOBALS
// ==========================================
int X = 0;
mutex critical_mtx;

vector<int> buffer;
int buffer_in = 0, buffer_out = 0;
mutex buffer_mtx;

int overflow_count = 0;
int underflow_count = 0;

// ==========================================
// PART 1 TASK
// ==========================================
void counter_task(int iterations, bool sync, bool inc) {
    for (int i = 0; i < iterations; i++) {
        if (sync) {
            critical_mtx.lock();
            inc ? X++ : X--;
            critical_mtx.unlock();
        } else {
            inc ? X++ : X--;
        }
    }
}

// ==========================================
// PRODUCER
// ==========================================
void producer(int n, int iterations,
              Semaphore* empty, Semaphore* full,
              bool sync, bool verbose) {

    for (int i = 0; i < iterations; i++) {

        if (sync && empty) empty->P();

        buffer_mtx.lock();

        buffer[buffer_in]++;

        if (verbose && i < 20) {
            cout << "[PRODUCER] Index " << buffer_in
                 << " = " << buffer[buffer_in];

            if (buffer[buffer_in] > 1 && !sync) {
                cout << "  <--- !!! OVERFLOW !!!";
                overflow_count++;
            }
            cout << endl;
        } else {
            if (buffer[buffer_in] > 1 && !sync)
                overflow_count++;
        }

        buffer_in = (buffer_in + 1) % n;

        buffer_mtx.unlock();

        if (sync && full) full->V();

        random_delay();
    }
}

// ==========================================
// CONSUMER
// ==========================================
void consumer(int n, int iterations,
              Semaphore* empty, Semaphore* full,
              bool sync, bool verbose) {

    for (int i = 0; i < iterations; i++) {

        if (sync && full) full->P();

        buffer_mtx.lock();

        buffer[buffer_out]--;

        if (verbose && i < 20) {
            cout << "[CONSUMER] Index " << buffer_out
                 << " = " << buffer[buffer_out];

            if (buffer[buffer_out] < 0 && !sync) {
                cout << "  <--- !!! UNDERFLOW !!!";
                underflow_count++;
            }
            cout << endl;
        } else {
            if (buffer[buffer_out] < 0 && !sync)
                underflow_count++;
        }

        buffer_out = (buffer_out + 1) % n;

        buffer_mtx.unlock();

        if (sync && empty) empty->V();

        random_delay();
    }
}

// ==========================================
// MAIN
// ==========================================
int main() {
    srand(time(0));

    int n = 5;
    int iterations_p1 = 1000000;
    int iterations_p2 = 100;
    int samples = 4;

    ofstream file("results.csv");

    // ======================================
    // PART 1
    // ======================================
    file << "Part 1 Results\n";
    file << "Sample,No Sync,With Sync\n";

    cout << "===== RUNNING EXPERIMENTS =====\n";
    cout << "\n--- PART 1: CRITICAL SECTION ---\n";

    for (int s = 1; s <= samples; s++) {

        X = 0;
        thread t1(counter_task, iterations_p1, false, true);
        thread t2(counter_task, iterations_p1, false, false);
        t1.join(); t2.join();
        int unsync = X;

        X = 0;
        thread t3(counter_task, iterations_p1, true, true);
        thread t4(counter_task, iterations_p1, true, false);
        t3.join(); t4.join();
        int sync = X;

        cout << "Sample " << s
             << " | No Sync: " << unsync
             << " | With Sync: " << sync << endl;

        file << s << "," << unsync << "," << sync << "\n";
    }

    // ======================================
    // PART 2
    // ======================================
    file << "\n\nPart 2 Results\n";
    file << "Sample,Overflow (No Sync),Underflow (No Sync),Overflow (With Sync),Underflow (With Sync)\n";

    cout << "\n--- PART 2: PRODUCER-CONSUMER ---\n";

    for (int s = 1; s <= samples; s++) {

        bool verbose = (s == 1);

        // ================= NO SYNC =================
        overflow_count = 0;
        underflow_count = 0;

        if (verbose)
            cout << "\n[PART 2 - A] OVERFLOW (No Sync, Fast Producer)\n";

        buffer.assign(n, 0);
        buffer_in = buffer_out = 0;

        thread p1(producer, n, iterations_p2, nullptr, nullptr, false, verbose);
        thread c1(consumer, n, iterations_p2, nullptr, nullptr, false, verbose);
        p1.join(); c1.join();

        if (verbose)
            cout << "\n[PART 2 - B] UNDERFLOW (No Sync, Fast Consumer)\n";

        buffer.assign(n, 0);
        buffer_in = buffer_out = 0;

        thread p2(producer, n, iterations_p2, nullptr, nullptr, false, verbose);
        thread c2(consumer, n, iterations_p2, nullptr, nullptr, false, verbose);
        p2.join(); c2.join();

        int overflow_no = overflow_count;
        int underflow_no = underflow_count;

        // ================= WITH SYNC =================
        overflow_count = 0;
        underflow_count = 0;

        if (verbose)
            cout << "\n[PART 2 - C] WITH FULL SYNCHRONIZATION\n";

        buffer.assign(n, 0);
        buffer_in = buffer_out = 0;

        Semaphore empty(n), full(0);

        thread p3(producer, n, iterations_p2, &empty, &full, true, verbose);
        thread c3(consumer, n, iterations_p2, &empty, &full, true, verbose);
        p3.join(); c3.join();

        int overflow_sync = overflow_count;
        int underflow_sync = underflow_count;

        cout << "Sample " << s
             << " | No Sync -> Overflow: " << overflow_no
             << ", Underflow: " << underflow_no
             << " | With Sync -> Overflow: " << overflow_sync
             << ", Underflow: " << underflow_sync
             << endl;

        file << s << ","
             << overflow_no << ","
             << underflow_no << ","
             << overflow_sync << ","
             << underflow_sync << "\n";
    }

    file.close();

    cout << "\nCSV file generated: results.csv\n";

    // KEEP TERMINAL OPEN
    cout << "Press Enter to exit...";
    cin.get();
    cin.get();

    return 0;
}