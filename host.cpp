#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
#include <xrt/xrt_bo.h>
#include <xrt/xrt_device.h>
#include <xrt/xrt_kernel.h>
#include <complex>
#include <stdexcept>
#include <algorithm>
#include <regex>

// Function to parse matrix strings from CSV
std::vector<std::complex<float>> parse_matrix(const std::string& matrix_str) {
    std::vector<std::complex<float>> matrix;
    std::string clean_str = matrix_str;

    // Remove unwanted characters: '[', ']', '(', ')', spaces, and double quotes
    clean_str.erase(remove(clean_str.begin(), clean_str.end(), '['), clean_str.end());
    clean_str.erase(remove(clean_str.begin(), clean_str.end(), ']'), clean_str.end());
    clean_str.erase(remove(clean_str.begin(), clean_str.end(), '('), clean_str.end());
    clean_str.erase(remove(clean_str.begin(), clean_str.end(), ')'), clean_str.end());
    clean_str.erase(remove(clean_str.begin(), clean_str.end(), ' '), clean_str.end());
    clean_str.erase(remove(clean_str.begin(), clean_str.end(), '"'), clean_str.end());

    std::stringstream ss(clean_str);
    std::string token;

    try {
        while (std::getline(ss, token, ',')) {
            float real = 0.0f, imag = 0.0f;
            size_t j_pos = token.find('j');

            if (j_pos != std::string::npos) {
                // Handle complex numbers
                size_t plus_pos = token.find('+');
                size_t minus_pos = token.find('-', 1); // Look for '-' after the first character

                if (plus_pos != std::string::npos) {
                    // Format: "a+bi"
                    real = std::stof(token.substr(0, plus_pos));
                    imag = std::stof(token.substr(plus_pos + 1, j_pos - plus_pos - 1));
                } else if (minus_pos != std::string::npos) {
                    // Format: "a-bi"
                    real = std::stof(token.substr(0, minus_pos));
                    imag = std::stof(token.substr(minus_pos, j_pos - minus_pos));
                } else {
                    // Purely imaginary (like "bi")
                    imag = std::stof(token.substr(0, j_pos));
                }
            } else {
                // Handle purely real number
                real = std::stof(token);
            }

            matrix.emplace_back(real, imag);
        }
    } catch (const std::invalid_argument& e) {
        std::cerr << "Error parsing matrix string: '" << matrix_str << "'\n";
        std::cerr << "Invalid conversion to complex<float>. Please check the matrix format in the CSV file." << std::endl;
        throw e;
    }

    return matrix;
}

// Function to read gate data from CSV file and number of qubits
void read_gates(const std::string& filename, std::vector<std::vector<std::complex<float>>>& gate_matrices, std::vector<int>& control_qubits, std::vector<int>& target_qubits, int& num_qubits) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file");
    }
    
    std::string line;
    std::getline(file, line);  // Read the header row containing the number of qubits

    // Extract the number of qubits from the sixth entry in the header row
    std::stringstream ss(line);
    std::string value;
    for (int i = 0; i < 5; ++i) {
        std::getline(ss, value, ','); // Skip the first five columns
    }
    std::getline(ss, value, ','); // The sixth entry should be the number of qubits
    num_qubits = std::stoi(value); // Convert to integer

    // Now process the gate data rows
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string value;
        std::vector<std::complex<float>> gate;
        std::string matrix_str;

        // Skip unnecessary columns and extract relevant data
        std::getline(ss, value, ','); // Gate Number (skip)
        std::getline(ss, value, ','); // Gate Name (skip)

        std::getline(ss, value, ',');
        int control = (value == "" || value == "NaN") ? -1 : std::stoi(value);  // Handle NaN for single-qubit gate

        std::getline(ss, value, ',');
        int target = std::stoi(value);  // Target qubit index

        // Read matrix string and parse it
        std::getline(ss, matrix_str);
        gate = parse_matrix(matrix_str);

        gate_matrices.push_back(gate);
        control_qubits.push_back(control);
        target_qubits.push_back(target);
    }

    file.close();
}


int main(int argc, char** argv) {
    std::string binaryFile = "./vadd.xclbin";
    int device_index = 0;

    // Load device and xclbin
    std::cout << "Opening the device " << device_index << std::endl;
    auto device = xrt::device(device_index);
    std::cout << "Loading the xclbin " << binaryFile << std::endl;
    auto uuid = device.load_xclbin(binaryFile);

    // Set up kernel
    auto kernel = xrt::kernel(device, uuid, "vadd", xrt::kernel::cu_access_mode::exclusive);

    // Read gates and number of qubits from the CSV file
    std::vector<std::vector<std::complex<float>>> gate_matrices;
    std::vector<int> control_qubits;
    std::vector<int> target_qubits;
    int num_qubits = 0;

    try {
        read_gates("../quantum_circuit_gates.csv", gate_matrices, control_qubits, target_qubits, num_qubits);
    } catch (const std::exception& e) {
        std::cerr << "Error reading gates from CSV: " << e.what() << std::endl;
        return 1;
    }

    // Initialize state vector based on the number of qubits
    int state_vector_size = 1 << num_qubits;
    std::vector<std::complex<float>> state_vector(state_vector_size, {0.0f, 0.0f});
    state_vector[0] = {1.0f, 0.0f};  // Initialize to |0000>

    // Allocate buffers on the device
    xrt::bo state_bo_1 = xrt::bo(device, state_vector.size() / 2 * sizeof(std::complex<float>), kernel.group_id(0));
    xrt::bo state_bo_2 = xrt::bo(device, state_vector.size() / 2 * sizeof(std::complex<float>), kernel.group_id(0));
    
    xrt::bo gate_bo = xrt::bo(device, 16 * sizeof(std::complex<float>), kernel.group_id(0));  // Buffer for gates
    
    xrt::bo output_state_bo_1 = xrt::bo(device, state_vector.size() / 2 * sizeof(std::complex<float>), kernel.group_id(0));
    xrt::bo output_state_bo_2 = xrt::bo(device, state_vector.size() / 2 * sizeof(std::complex<float>), kernel.group_id(0));

    

    // Map buffers
    auto state_bo_map_1 = state_bo_1.map<std::complex<float>*>();
    auto state_bo_map_2 = state_bo_2.map<std::complex<float>*>();
    auto output_state_bo_map_1 = output_state_bo_1.map<std::complex<float>*>();
    auto output_state_bo_map_2 = output_state_bo_2.map<std::complex<float>*>();

    // Copy initial state to buffers
    std::copy(state_vector.begin(), state_vector.begin() + state_vector.size() / 2, state_bo_1.map<std::complex<float>*>());
    std::copy(state_vector.begin() + state_vector.size() / 2, state_vector.end(), state_bo_2.map<std::complex<float>*>());
    
     std::fill(output_state_bo_1.map<std::complex<float>*>(), output_state_bo_1.map<std::complex<float>*>() + state_vector.size() / 2, std::complex<float>(0.0f, 0.0f));
    std::fill(output_state_bo_2.map<std::complex<float>*>(), output_state_bo_2.map<std::complex<float>*>() + state_vector.size() / 2, std::complex<float>(0.0f, 0.0f));
    
 /*   
    std::cout << "Initial State Vector (Buffer 1):" << std::endl;
    for (size_t i = 0; i < state_vector.size() / 2; ++i) {
    std::cout << "Index " << i << ": " 
              << state_bo_map_1[i].real() << "+" << state_bo_map_1[i].imag() << "i" << std::endl;
    }

    std::cout << "Initial State Vector (Buffer 2):" << std::endl;
    for (size_t i = 0; i < state_vector.size() / 2; ++i) {
    std::cout << "Index " << i + state_vector.size() / 2 << ": " 
              << state_bo_map_2[i].real() << "+" << state_bo_map_2[i].imag() << "i" << std::endl;
    }
*/

    // Synchronize state buffer to device
    state_bo_1.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    state_bo_2.sync(XCL_BO_SYNC_BO_TO_DEVICE);
/*
std::cout << "Device Buffer Contents After Sync (Buffer 1):" << std::endl;
for (size_t i = 0; i < state_vector.size() / 2; ++i) {
    std::cout << "Index " << i << ": " 
              << state_bo_map_1[i].real() << "+" << state_bo_map_1[i].imag() << "i" << std::endl;
}

std::cout << "Device Buffer Contents After Sync (Buffer 2):" << std::endl;
for (size_t i = 0; i < state_vector.size() / 2; ++i) {
    std::cout << "Index " << i + state_vector.size() / 2 << ": " 
              << state_bo_map_2[i].real() << "+" << state_bo_map_2[i].imag() << "i" << std::endl;
}
*/
    
    output_state_bo_1.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    output_state_bo_2.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    
    std::cout << gate_matrices.size() << "\n";
    


    // Apply gates sequentially
    for (size_t i = 0; i < gate_matrices.size(); ++i) {

        auto gate_bo_map = gate_bo.map<std::complex<float>*>();
        std::fill(gate_bo_map, gate_bo_map + 16, std::complex<float>(0.0f, 0.0f));  // Ensure buffer is cleared
        std::copy(gate_matrices[i].begin(), gate_matrices[i].end(), gate_bo_map);
        gate_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);

        // Run kernel
        auto run = kernel(state_bo_1, state_bo_2, gate_bo, output_state_bo_1, output_state_bo_2, control_qubits[i], target_qubits[i], num_qubits);
        run.wait();

        // Synchronize back the output state vector
        output_state_bo_1.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        output_state_bo_2.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        

        // Replace the input state vector with the output state
        std::copy(output_state_bo_map_1, output_state_bo_map_1 + state_vector.size() / 2, state_vector.begin());
        std::copy(output_state_bo_map_2, output_state_bo_map_2 + state_vector.size() / 2, state_vector.begin() + state_vector.size() / 2);

        // Synchronize input buffers back to the device for the next iteration
        std::copy(state_vector.begin(), state_vector.begin() + state_vector.size() / 2, state_bo_map_1);
        std::copy(state_vector.begin() + state_vector.size() / 2, state_vector.end(), state_bo_map_2);
        
        state_bo_1.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        state_bo_2.sync(XCL_BO_SYNC_BO_TO_DEVICE);

        std::cout << "\n";
    }

    // Write the final complex state
    std::ofstream outFile("final_state_vector.csv");
    if (outFile.is_open()) {
    for (const auto& val : state_vector) {
        outFile << val.real() << "+" << val.imag() << "i" << "\n";
    }
    outFile.close();
    std::cout << "Final state vector written to final_state_vector.csv\n";
    } else {
    std::cerr << "Unable to open file for writing.\n";
    }


    return 0;
}
