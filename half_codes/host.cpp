//#include #include <fp16.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
#include <xrt/xrt_bo.h>
#include <xrt/xrt_device.h>
#include <xrt/xrt_kernel.h>
#include <stdexcept>
#include <algorithm>
#include <regex>
//#include <boost/multiprecision/cpp_bin_float.hpp>
#include "half.hpp" // Adjust path as necessary


// Use __fp16 for half-precision floating-point
using half = half_float::half; // Adjust namespace if needed

// Function to parse matrix strings from CSV into separate real and imaginary parts
void parse_matrix(const std::string& matrix_str, std::vector<half>& real_parts, std::vector<half>& imag_parts) {
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

            // Convert to half-precision and store
            real_parts.push_back(static_cast<half>(real));
            imag_parts.push_back(static_cast<half>(imag));
            
        }
    } catch (const std::invalid_argument& e) {
        std::cerr << "Error parsing matrix string: '" << matrix_str << "'\n";
        std::cerr << "Invalid conversion to half-precision. Please check the matrix format in the CSV file." << std::endl;
        throw e;
    }
}

// Function to read gate data from CSV file and number of qubits
void read_gates(const std::string& filename, std::vector<std::vector<half>>& real_parts_list,
                std::vector<std::vector<half>>& imag_parts_list, std::vector<int>& control_qubits,
                std::vector<int>& target_qubits, int& num_qubits) {
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
        std::vector<half> real_parts, imag_parts;
        std::string matrix_str;

        // Skip unnecessary columns and extract relevant data
        std::getline(ss, value, ','); // Gate Number (skip)
        std::getline(ss, value, ','); // Gate Name (skip)

        std::getline(ss, value, ',');
        int control = (value.empty() || value == "NaN") ? -1 : std::stoi(value);  // Handle NaN for single-qubit gate

        std::getline(ss, value, ',');
        int target = std::stoi(value);  // Target qubit index

        // Read matrix string and parse it
        std::getline(ss, matrix_str);
        parse_matrix(matrix_str, real_parts, imag_parts);

        real_parts_list.push_back(real_parts);
        imag_parts_list.push_back(imag_parts);
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
    std::vector<std::vector<half>> real_parts_list;
    std::vector<std::vector<half>> imag_parts_list;
    std::vector<int> control_qubits;
    std::vector<int> target_qubits;
    int num_qubits = 0;

    try {
        read_gates("../quantum_circuit_gates.csv", real_parts_list, imag_parts_list, control_qubits, target_qubits, num_qubits);
    } catch (const std::exception& e) {
        std::cerr << "Error reading gates from CSV: " << e.what() << std::endl;
        return 1;
    }

    // Initialize state vector based on the number of qubits
    int state_vector_size = 1 << num_qubits;
    std::vector<half> state_real(state_vector_size, static_cast<half>(0.0));
    std::vector<half> state_imag(state_vector_size, static_cast<half>(0.0));
    state_real[0] = static_cast<half>(1.0);  // Initialize to |0000>
    

    // Allocate buffers on the device
    xrt::bo state_real_bo = xrt::bo(device, state_real.size() * sizeof(half), kernel.group_id(0));
    xrt::bo state_imag_bo = xrt::bo(device, state_imag.size() * sizeof(half), kernel.group_id(0));
    xrt::bo gate_real_bo = xrt::bo(device, 16 * sizeof(half), kernel.group_id(0));  // Buffer for gate real part
    xrt::bo gate_imag_bo = xrt::bo(device, 16 * sizeof(half), kernel.group_id(0));  // Buffer for gate imaginary part
    xrt::bo output_real_bo = xrt::bo(device, state_real.size() * sizeof(half), kernel.group_id(0));
    xrt::bo output_imag_bo = xrt::bo(device, state_imag.size() * sizeof(half), kernel.group_id(0));

    // Map buffers
    auto state_real_map = state_real_bo.map<half*>();
    auto state_imag_map = state_imag_bo.map<half*>();
    auto output_real_map = output_real_bo.map<half*>();
    auto output_imag_map = output_imag_bo.map<half*>();

    // Copy initial state to buffers
    std::copy(state_real.begin(), state_real.end(), state_real_map);
    std::copy(state_imag.begin(), state_imag.end(), state_imag_map);
    std::fill(output_real_map, output_real_map + state_real.size(), static_cast<half>(0.0));
    std::fill(output_imag_map, output_imag_map + state_imag.size(), static_cast<half>(0.0));
    
    /*
    
    // Print mapped buffer contents to verify
std::cout << "Mapped State Real (Before Sync):" << std::endl;
for (size_t i = 0; i < state_real.size(); ++i) {
    std::cout << static_cast<float>(state_real_map[i]) << " ";
}
std::cout << std::endl;

std::cout << "Mapped State Imaginary (Before Sync):" << std::endl;
for (size_t i = 0; i < state_imag.size(); ++i) {
    std::cout << static_cast<float>(state_imag_map[i]) << " ";
}
std::cout << std::endl;*/

    // Synchronize buffers to device
    state_real_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    state_imag_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    
    std::cout << real_parts_list.size() << "\n";

  // Apply gates sequentially
    for (size_t i = 0; i < real_parts_list.size(); ++i) {
        //Print gate information
        /*std::cout << "Applying gate " << i + 1 << " with control: " << control_qubits[i]<< ", target: " << target_qubits[i] << "\nGate matrix: ";
        std::cout << "\n";*/
    
    
        auto gate_real_map = gate_real_bo.map<half*>();
        auto gate_imag_map = gate_imag_bo.map<half*>();
        std::fill(gate_real_map, gate_real_map + 16, static_cast<half>(0.0));
        std::fill(gate_imag_map, gate_imag_map + 16, static_cast<half>(0.0));
        std::copy(real_parts_list[i].begin(), real_parts_list[i].end(), gate_real_map);
        std::copy(imag_parts_list[i].begin(), imag_parts_list[i].end(), gate_imag_map);
        

        gate_real_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        gate_imag_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        
        // Determine matrix size: 2x2 for single-qubit gates, 4x4 for two-qubit gates
    /*  size_t matrix_size = (control_qubits[i] == -1) ? 2 : 4; // Single-qubit if control is -1

  // Print the real and imaginary parts of the gate matrix
    std::cout << "Gate Matrix Real Part (Iteration " << i << "):" << std::endl;
    for (size_t row = 0; row < matrix_size; ++row) {
        for (size_t col = 0; col < matrix_size; ++col) {
            std::cout << static_cast<float>(gate_real_map[row * matrix_size + col]) << " ";
        }
        std::cout << std::endl;
    }

    std::cout << "Gate Matrix Imaginary Part (Iteration " << i << "):" << std::endl;
    for (size_t row = 0; row < matrix_size; ++row) {
        for (size_t col = 0; col < matrix_size; ++col) {
            std::cout << static_cast<float>(gate_imag_map[row * matrix_size + col]) << " ";
        }
        std::cout << std::endl;
    }*/


        // Run kernel
        auto run = kernel(state_real_bo, state_imag_bo, gate_real_bo, gate_imag_bo, output_real_bo, output_imag_bo, control_qubits[i], target_qubits[i], num_qubits);
        run.wait();

        // Synchronize back the output state vector
        output_real_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        output_imag_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        
        // Print the updated state vector after applying the gate
    /*std::cout << "State Vector (Iteration " << i << "):" << std::endl;
    for (int j = 0; j < static_cast<int>(state_real.size()); ++j) {
        std::cout << static_cast<float>(output_real_map[j]) << "+" << static_cast<float>(output_imag_map[j]) << "j ";
        if ((j + 1) % 4 == 0) std::cout << std::endl;  // Print 4 elements per row for readability
    }
    std::cout << std::endl;*/

        // Replace input state vector with output state
        std::copy(output_real_map, output_real_map + state_real.size(), state_real_map);
        std::copy(output_imag_map, output_imag_map + state_imag.size(), state_imag_map);
        state_real_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        state_imag_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    }

    // Write the final state vector
    std::ofstream outFile("final_state_vector.csv");
    if (outFile.is_open()) {
        for (int i = 0; i < static_cast<int>(state_real.size()); ++i) {
            outFile << static_cast<float>(state_real_map[i]) << "+" << static_cast<float>(state_imag_map[i]) << "i\n";
        }
        outFile.close();
        std::cout << "Final state vector written to final_state_vector.csv\n";
    } else {
        std::cerr << "Unable to open file for writing.\n";
    }

    return 0;
}
