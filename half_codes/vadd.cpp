#include <hls_half.h> // Include the header for hls::half
#include <iostream>

extern "C" {
    void vadd(
        half* state_real,    // Input real part of state vector
        half* state_imag,    // Input imaginary part of state vector
        half* gate_real,     // Input real part of gate matrix
        half* gate_imag,     // Input imaginary part of gate matrix
        half* output_real,   // Output real part of state vector
        half* output_imag,   // Output imaginary part of state vector
        int control,         // Control qubit index (-1 for no control)
        int target,          // Target qubit index
        int num_qubits       // Number of qubits
    ) {
#pragma HLS INTERFACE m_axi port=state_real depth=1024 bundle=gmem0
#pragma HLS INTERFACE m_axi port=state_imag depth=1024 bundle=gmem0
#pragma HLS INTERFACE m_axi port=gate_real depth=16 bundle=gmem0
#pragma HLS INTERFACE m_axi port=gate_imag depth=16 bundle=gmem0
#pragma HLS INTERFACE m_axi port=output_real depth=1024 bundle=gmem0
#pragma HLS INTERFACE m_axi port=output_imag depth=1024 bundle=gmem0
#pragma HLS INTERFACE s_axilite port=control
#pragma HLS INTERFACE s_axilite port=target
#pragma HLS INTERFACE s_axilite port=num_qubits
#pragma HLS INTERFACE s_axilite port=return

        int num_states = 1 << num_qubits;
        int gate_size = (control == -1) ? 2 : 4;
        
        
       /*  // Debugging: Print all inputs
        std::cout << "Number of Qubits: " << num_qubits << std::endl;
        std::cout << "Control Qubit: " << control << ", Target Qubit: " << target << std::endl;


        // Print the state vector (real and imaginary parts)
        std::cout << "State Vector (Real):" << std::endl;
        for (int i = 0; i < num_states; ++i) {
            std::cout << static_cast<float>(state_real[i]) << " ";
            if ((i + 1) % 8 == 0) std::cout << std::endl; // Print 8 elements per row
        }
        std::cout << std::endl;

        std::cout << "State Vector (Imaginary):" << std::endl;
        for (int i = 0; i < num_states; ++i) {
            std::cout << static_cast<float>(state_imag[i]) << " ";
            if ((i + 1) % 8 == 0) std::cout << std::endl;
        }
        std::cout << std::endl;

        // Print the gate matrix (real and imaginary parts)
        std::cout << "Gate Matrix (Real):" << std::endl;
        for (int i = 0; i < gate_size; ++i) {
            for (int j = 0; j < gate_size; ++j) {
                std::cout << static_cast<float>(gate_real[i * gate_size + j]) << " ";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;

        std::cout << "Gate Matrix (Imaginary):" << std::endl;
        for (int i = 0; i < gate_size; ++i) {
            for (int j = 0; j < gate_size; ++j) {
                std::cout << static_cast<float>(gate_imag[i * gate_size + j]) << " ";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;*/

        copy_loop: for (int i = 0; i < num_states; ++i) {
            #pragma HLS PIPELINE II=1
            output_real[i] = state_real[i];
            output_imag[i] = state_imag[i];
        }

        // Single-qubit gate operation
        if (gate_size == 2) {
            single_qubit_loop: for (int i = 0; i < num_states; ++i) {
                #pragma HLS PIPELINE II=1
                #pragma HLS UNROLL factor=5
                int target_bit = (i >> target) & 1;
                int flipped_i = i ^ (1 << target);

                if (i < flipped_i) {

                    // Apply gate to the state vector: Real and Imaginary parts
                    half real_i = state_real[i];
                    half imag_i = state_imag[i];
                    half real_flipped = state_real[flipped_i];
                    half imag_flipped = state_imag[flipped_i];

                    // Calculate real and imaginary for output_state_vector[i]
                    output_real[i] = gate_real[0 + target_bit] * real_i - gate_imag[0 + target_bit] * imag_i + gate_real[1 - target_bit] * real_flipped - gate_imag[1 - target_bit] * imag_flipped;

                    output_imag[i] = gate_real[0 + target_bit] * imag_i + gate_imag[0 + target_bit] * real_i + gate_real[1 - target_bit] * imag_flipped + gate_imag[1 - target_bit] * real_flipped;

                    // Calculate real and imaginary for output_state_vector[flipped_i]
                    output_real[flipped_i] = gate_real[2 + target_bit] * real_i - gate_imag[2 + target_bit] * imag_i + gate_real[3 - target_bit] * real_flipped - gate_imag[3 - target_bit] * imag_flipped;

                    output_imag[flipped_i] = gate_real[2 + target_bit] * imag_i + gate_imag[2 + target_bit] * real_i + gate_real[3 - target_bit] * imag_flipped + gate_imag[3 - target_bit] * real_flipped;
                                           
                }
            }
        }

        // Two-qubit gate operation
        else {
            two_qubit_loop: for (int i = 0; i < num_states; ++i) {
                #pragma HLS PIPELINE II=1
                #pragma HLS UNROLL factor=5
                int control_bit = (i >> control) & 1;
                int target_bit = (i >> target) & 1;

                if (control_bit == 1) {
                    int flipped_i = i ^ (1 << target);

                    if (target_bit == 0) {
                        // Swap i and flipped_i: Real and Imaginary parts
                        half temp_real = output_real[i];
                        half temp_imag = output_imag[i];

                        output_real[i] = state_real[flipped_i];
                        output_imag[i] = state_imag[flipped_i];

                        output_real[flipped_i] = temp_real;
                        output_imag[flipped_i] = temp_imag;
                    }
                }
            }
        }
    }
}
