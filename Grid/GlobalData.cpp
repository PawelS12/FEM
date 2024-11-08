#include <math.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <stdexcept>
#include "GlobalData.h"
#include "Node.h" 
#include "Elem4.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::istringstream;
using std::ifstream;
using std::vector;
using std::runtime_error;

void GlobalData::read_file() {
    try {
        ifstream dataFile("data.txt");
        if (!dataFile.is_open()) {
            throw runtime_error("File: data.txt not found.");
        }

        if (!(dataFile >> simulation_time >> simulation_step_time >> conductivity >> alfa >> tot
                       >> initial_temp >> density >> specific_heat >> nN >> nE >> nH >> nW >> H >> W)) {
            throw runtime_error("Error reading data from file: data.txt");
        }

        if (simulation_time < 0) {
            throw runtime_error("Simulation time cannot be negative.");
        }
        if (simulation_step_time <= 0 || conductivity <= 0 || alfa <= 0 || density <= 0 || specific_heat <= 0 
                || nN <= 0 || nE <= 0 || nH <= 0 || nW <= 0 || H <= 0 || W <= 0) {
            throw runtime_error("Simulation data must be positive.");
        }

        dataFile.close();

        ifstream nodeFile("XY_coordinates.txt");
        if (!nodeFile.is_open()) {
            throw runtime_error("File: XY_coordinates.txt not found.");
        }

        string line;
        while (getline(nodeFile, line)) {
            double x, y;
            if (!(istringstream(line) >> x >> y)) {
                throw runtime_error("Failed to read line: " + line);
            }
            nodes_xy.emplace_back(x, y);
        }
        nodeFile.close();

        create_elements_from_nodes();
    } catch (const runtime_error& e) {
        cerr << "Error: " << e.what() << endl;
    }
}

double GlobalData::get_simulation_time() { return simulation_time; }
double GlobalData::get_simulation_step_time() { return simulation_step_time; }
double GlobalData::get_conductivity() { return conductivity; }
double GlobalData::get_alfa() { return alfa; }
double GlobalData::get_tot() { return tot; }
double GlobalData::get_initial_temp() { return initial_temp; }
double GlobalData::get_density() { return density; }
double GlobalData::get_specific_heat() { return specific_heat; }
double GlobalData::get_nN() { return nN; }
double GlobalData::get_nE() { return nE; }
double GlobalData::get_nH() { return nH; }
double GlobalData::get_nW() { return nW; }
double GlobalData::get_height() { return H; }
double GlobalData::get_width() { return W; }
const vector<Elem4>& GlobalData::get_elements() const { return elements; }

void GlobalData::display_simulation_data() {
    cout << "-----------------------------------" << endl;
    cout << "Simulation data: " << endl << endl;
    cout << "Simulation time: " << simulation_time << endl;
    cout << "Simulation step time: " << simulation_step_time << endl;
    cout << "Conductivity: " << conductivity << endl;
    cout << "Alfa: " << alfa << endl;
    cout << "Tot: " << tot << endl;
    cout << "Initial temperature: " << initial_temp << endl;
    cout << "Density: " << density << endl;
    cout << "Specific heat: " << specific_heat << endl;
    cout << "Number of nodes: " << nN << endl;
    cout << "Number of elements: " << nE << endl;
    cout << "Nodes height: " << nH << endl;
    cout << "Nodes width: " << nW << endl;
    cout << "Height: " << H << endl;
    cout << "Width: " << W << endl;
    cout << "-----------------------------------" << endl;
    cout << "XY nodes:" << endl;
    for (const auto& node : nodes_xy) {
        node.display_node();
    }
    cout << "-----------------------------------" << endl;
}

void GlobalData::create_elements_from_nodes() {
    for (size_t i = 0; i < nodes_xy.size() - 3; i += 4) {
        Elem4 newElement(nodes_xy[i], nodes_xy[i + 1], nodes_xy[i + 2], nodes_xy[i + 3]);
        elements.push_back(newElement);
    }
}
