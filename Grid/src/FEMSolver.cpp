#include "FEMSolver.h"
#include "Integration.h"
#include "Element.h"
#include "Grid.h"
#include <cmath>
#include <vector>
#include <iomanip> 
#include <fstream>

using std::abs;
using std::cout;
using std::cerr;
using std::endl;
using std::vector;
using std::fill;
using std::fixed;
using std::setprecision;
using std::ofstream;

FEMSolver::FEMSolver(Grid& grid, double alpha, double ambient_temperature) : grid(grid) {
    local_H_matrices.resize(grid.get_elements().size(), vector<double>(4, 0.0));
    calculate_local_Hbc_matrix(alpha);
}

void FEMSolver::display_matrix(vector<vector<double>>& matrix) {  
    for (const auto& row : matrix) {
        for (const auto& value : row) {
            cout << value << " ";
        }
        cout << endl;
    }
}

void FEMSolver::compute_jacobian(const Element& element, double xi, double eta, double J[2][2]) const {
    double dN_dxi[4] = {
        -0.25 * (1 - eta),  
         0.25 * (1 - eta),  
         0.25 * (1 + eta),  
        -0.25 * (1 + eta)  
    };

    double dN_deta[4] = {
        -0.25 * (1 - xi),  
        -0.25 * (1 + xi),  
         0.25 * (1 + xi),   
         0.25 * (1 - xi)   
    };

    const Node* nodes = element.get_nodes();
    fill(&J[0][0], &J[0][0] + 4, 0.0);

    for (int i = 0; i < 4; ++i) {
        J[0][0] += dN_dxi[i] * nodes[i].get_x();
        J[0][1] += dN_deta[i] * nodes[i].get_x();
        J[1][0] += dN_dxi[i] * nodes[i].get_y();
        J[1][1] += dN_deta[i] * nodes[i].get_y();
    }
}

double FEMSolver::compute_jacobian_determinant(double J[2][2]) const {
    return J[0][0] * J[1][1] - J[0][1] * J[1][0];
}

void FEMSolver::compute_inverse_jacobian(double J[2][2], double invJ[2][2]) const {
    double detJ = compute_jacobian_determinant(J);

    if (abs(detJ) < 1e-12) {  
        cerr << "Error: Jacobian determinant is close to 0! Skipping element." << endl;
        return; 
    }

    invJ[0][0] = J[1][1] / detJ;
    invJ[0][1] = -J[0][1] / detJ;
    invJ[1][0] = -J[1][0] / detJ;
    invJ[1][1] = J[0][0] / detJ;
}

double FEMSolver::calculate_H_integrand(const Element& element, double conductivity, int i, int j, double xi, double eta) const {
    double dN_dxi[4] = { 
        -0.25 * (1 - eta), 
         0.25 * (1 - eta), 
         0.25 * (1 + eta), 
        -0.25 * (1 + eta) 
    };

    double dN_deta[4] = { 
        -0.25 * (1 - xi), 
        -0.25 * (1 + xi), 
         0.25 * (1 + xi), 
         0.25 * (1 - xi) 
    };

    double J[2][2];
    compute_jacobian(element, xi, eta, J);
    double detJ = compute_jacobian_determinant(J);

    double invJ[2][2];
    compute_inverse_jacobian(J, invJ);

    const auto& nodes = element.get_nodes();

    double dN_dx[4], dN_dy[4];
    for (int k = 0; k < 4; ++k) {
        dN_dx[k] = invJ[0][0] * dN_dxi[k] + invJ[0][1] * dN_deta[k];
        dN_dy[k] = invJ[1][0] * dN_dxi[k] + invJ[1][1] * dN_deta[k];
    }
    
    return conductivity * (dN_dx[i] * dN_dx[j] + dN_dy[i] * dN_dy[j]) * detJ;
}

void FEMSolver::calculate_Hbc_matrix(double conductivity) {
    Integration integrator;
    local_H_matrices.clear(); 

    for (const auto& element : grid.get_elements()) {
        vector<double> H(16, 0.0);  

        const auto& nodes = element.get_nodes();
        const auto& ID = element.get_ID();

        cout << "Element ID: ";
        for (size_t i = 0; i < ID.size(); ++i) {
            cout << ID[i] << " "; 
        }
        cout << endl;

        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                H[i * 4 + j] = integrator.gauss_integration_2D(
                    [this, &element, conductivity, i, j](double xi, double eta) -> double {
                        return this->calculate_H_integrand(element, conductivity, i, j, xi, eta);
                    }, 4, -1, 1, -1, 1);
            }
        }

        const auto& Hbc_local = element.get_Hbc();
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                H[i * 4 + j] += Hbc_local[i][j];  
            }
        }

        local_H_matrices.push_back(H); 

        cout << "Local H matrix with Hbc:" << endl;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                cout << H[i * 4 + j] << " ";
            }
            cout << endl;
        }
        cout << endl;
    }
}

void FEMSolver::aggregate_Hbc_matrix(vector<vector<double>>& H_global, int nodes_num) const {
    H_global.assign(nodes_num, vector<double>(nodes_num, 0.0));  
    const auto& elements = grid.get_elements();

    for (size_t elem_idx = 0; elem_idx < elements.size(); ++elem_idx) {
        const auto& element = elements[elem_idx];
        const auto& H_local = local_H_matrices[elem_idx];  
        const auto& ID = element.get_ID();  

        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                if (ID[i] < nodes_num && ID[j] < nodes_num) {
                    H_global[ID[i]][ID[j]] += H_local[i * 4 + j];
                } else {
                    cerr << "Invalid global index: " << ID[i] << " or " << ID[j] << endl;
                }
            }
        }
    }

    ofstream output_file("../Grid/results/global_Hbc_matrix.txt");
    if (output_file.is_open()) {
        output_file << fixed << setprecision(5);
        output_file << "Global Hbc matrix:" << endl << endl;
        for (const auto& row : H_global) {
            for (const auto& value : row) {
                output_file << value << " ";
            }
            output_file << endl; 
        }
        output_file.close(); 
    } else {
        cerr << "Error" << endl;
    }

    cout << "-----------------------------------" << endl;
    cout << "Global Hbc Matrix:" << endl << endl;
    for (const auto& row : H_global) {
        for (const auto& value : row) {
            cout << value << " ";
        }
        cout << endl;
    }
    cout << endl;
}

void FEMSolver::integrate_Hbc_on_edge(const Node& node1, const Node& node2, double alpha, vector<vector<double>>& Hbc) const {
    Integration integrator;
    vector<double> xi_points = integrator.get_points(2);  
    vector<double> weights = integrator.get_weights(2);  

    for (int k = 0; k < 2; ++k) {
        double xi = xi_points[k];
        double weight = weights[k];
        double N1 = 0.5 * (1 - xi);
        double N2 = 0.5 * (1 + xi);
        double x = N1 * node1.get_x() + N2 * node2.get_x();
        double y = N1 * node1.get_y() + N2 * node2.get_y();
        double dx_dxi = -0.5 * (node1.get_x() - node2.get_x());
        double dy_dxi = -0.5 * (node1.get_y() - node2.get_y());
        double detJ = sqrt(dx_dxi * dx_dxi + dy_dxi * dy_dxi);
        double Hbc_value = alpha * weight * detJ; 

        Hbc[0][0] += Hbc_value * N1 * N1;
        Hbc[0][1] += Hbc_value * N1 * N2;
        Hbc[1][0] += Hbc_value * N2 * N1;
        Hbc[1][1] += Hbc_value * N2 * N2;
    }
}

void FEMSolver::calculate_local_Hbc_matrix(double alpha) {
    for (auto& element : grid.get_elements()) {
        vector<vector<double>> Hbc_local(4, vector<double>(4, 0.0));

        const Node* nodes = element.get_nodes();
        for (int edge = 0; edge < 4; ++edge) {
            const Node& node1 = nodes[edge];
            const Node& node2 = nodes[(edge + 1) % 4];

            if (node1.get_BC() && node2.get_BC()) {
                vector<vector<double>> Hbc_edge(2, vector<double>(2, 0.0));
                integrate_Hbc_on_edge(node1, node2, alpha, Hbc_edge);
                int local_indices[2] = {edge, (edge + 1) % 4};

                for (int i = 0; i < 2; ++i) {
                    for (int j = 0; j < 2; ++j) {
                        Hbc_local[local_indices[i]][local_indices[j]] += Hbc_edge[i][j];
                    }
                }
            }
        }

        element.set_Hbc(Hbc_local);
        cout << "Local Hbc matrix for element:" << endl;

        for (const auto& row : Hbc_local) {
            for (const auto& value : row) {
                cout << value << " ";
            }
            cout << endl;
        }
        cout << endl;
    }
}

void FEMSolver::calculate_P_vector(double alpha, double ambient_temperature) {
    cout << "-----------------------------------" << endl;
    cout << "Local P vectors for elements:" << endl << endl;

    for (auto& element : grid.get_elements()) {
        vector<double> P_local(4, 0.0);
        const Node* nodes = element.get_nodes();

        for (int edge = 0; edge < 4; ++edge) {
            const Node& node1 = nodes[edge];
            const Node& node2 = nodes[(edge + 1) % 4];

            if (node1.get_BC() && node2.get_BC()) {
                Integration integrator;
                vector<double> xi_points = integrator.get_points(2);
                vector<double> weights = integrator.get_weights(2);
                double L = sqrt(pow(node2.get_x() - node1.get_x(), 2) + pow(node2.get_y() - node1.get_y(), 2));

                for (int k = 0; k < 2; ++k) {
                    double xi = xi_points[k];
                    double weight = weights[k];
                    double N1 = 0.5 * (1 - xi);
                    double N2 = 0.5 * (1 + xi);
                    double detJ = 0.5 * L; 
                    double q = alpha * ambient_temperature * weight * detJ;

                    P_local[edge] += q * N1;
                    P_local[(edge + 1) % 4] += q * N2;
                }
            }
        }

        element.set_P(P_local);

        for (const auto& value : P_local) {
            cout << value << " ";
        }
        cout << endl << endl;
    }
}

void FEMSolver::aggregate_P_vector(vector<double>& P_global, int nodes_num) const {
    P_global.assign(nodes_num, 0.0);  
    const auto& elements = grid.get_elements();

    for (const auto& element : elements) {
        const auto& P_local = element.get_P(); 
        const auto& ID = element.get_ID();   

        for (int i = 0; i < 4; ++i) {
            if (ID[i] < nodes_num) {
                P_global[ID[i]] += P_local[i]; 
            } else {
                cerr << "Invalid global index: " << ID[i] << endl;
            }
        }
    }

    cout << "-----------------------------------" << endl;
    cout << "Global P vector:" << endl << endl;
    for (const auto& value : P_global) {
        cout << value << " ";
    }
    cout << endl << endl;

    ofstream output_file("../Grid/results/global_P_vector.txt");
    if (output_file.is_open()) {
        output_file << "Global P vector:" << endl << endl;
        for (const auto& val : P_global) {
            output_file << val << " ";
        }
        
        output_file.close(); 
    } else {
        cerr << "Error" << endl;
    }
}