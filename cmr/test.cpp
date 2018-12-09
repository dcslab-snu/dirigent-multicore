//
// Created by 남윤성 on 16/10/2018.
//

#include <iostream>
#include <fstream>
//#include <string.h>
#include <cstring>

static int SAMPLE_LEN;
//static int MAX_SAMPLE_LEN = 200/0.05; // 4000 samples
static int MAX_SAMPLE_LEN = 6; // 4000 samples
static uint64_t *scycle;
static uint64_t *sinstr;
static uint EXCERPT_LEN;
static const int EXCERPT_NUM = 350;
static const int MAX_SAMPLE_NUM = 500;
static const int FREQ_ARR[] = {1200000, 1300000, 1400000, 1500000, 1600000, 1700000, 1800000, 1900000, 2000000};

//static int cyc_samples = 0;
//static int instr_samples = 0;

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); } while (0)


using namespace std;

string fg_name = "streamcluster";

int main(int argc, char *argv[]) {
    scycle = new uint64_t[MAX_SAMPLE_NUM];    // uint
    sinstr = new uint64_t[MAX_SAMPLE_NUM];    //

    string cycle_filename = fg_name + "_cycle.csv";
    string instr_filename = fg_name + "_instr.csv";
    const char *cycle_cstr_file = cycle_filename.c_str();
    const char *instr_cstr_file = instr_filename.c_str();
    int cyc_samples = 0;
    int instr_samples = 0;
    uint64_t scycle_item, sinstr_item;
    string scycle_line, sinstr_line;
    memset(scycle, 0, sizeof(scycle));
    memset(sinstr, 0, sizeof(sinstr));

    // reading samples of cycles from ${fg_name}_cycle.data

    ifstream ifile_cyc(cycle_cstr_file, ifstream::in); // ifstream::in -> ios::in
    if (!ifile_cyc.is_open()) {
        errExit("failed to open file: cycle.data");
    }


    cyc_samples = 0;
    while (ifile_cyc.peek() != '\n') {
        getline(ifile_cyc, scycle_line, ',');
        sscanf(scycle_line.c_str(), "%llu", &scycle[cyc_samples]);
        cyc_samples++;
    }
    cout << "count: " << cyc_samples << endl;
    ifile_cyc.close();

    // reading samples of instr. rates from ${fg_name}_instr.data
    ifstream ifile_ins(instr_cstr_file, ifstream::in);
    if (!ifile_ins.is_open()) {
        errExit("failed to open file: instr.data");
    }

    instr_samples = 0;
    while (ifile_ins.peek() != '\n') {
        getline(ifile_ins, sinstr_line, ',');
        sscanf(sinstr_line.c_str(), "%llu", &sinstr[instr_samples]);
        instr_samples++;
    }
    cout << "count: " << instr_samples << endl;
    ifile_ins.close();

    cout << "scycle" << endl;
    for (int i = 0; i < MAX_SAMPLE_NUM; i++) {
        cout << scycle[i] << ", ";
    }
    cout << " " << endl;

    cout << "sinstr" << endl;
    for (int i = 0; i < MAX_SAMPLE_NUM; i++) {
        cout << sinstr[i] << ", ";
    }
    cout << " " << endl;

    return 0;
}