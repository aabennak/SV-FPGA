#include <complex>
#include <iostream>

extern "C" {
    void vadd(
        std::complex<float> *input_state_1,       // First half of the input state vector
        std::complex<float> *input_state_2,       // Second half of the input state vector
        std::complex<float> *gate_matrix,         // Input gate matrix (2x2 for single-qubit, 4x4 for two-qubit)
        std::complex<float> *output_state_1,      // First half of the output state vector
        std::complex<float> *output_state_2,      // Second half of the output state vector
        int control,                              // Control qubit index (-1 for no control)
        int target,                               // Target qubit index
        int num_qubits                            // Number of qubits
    ) {
#pragma HLS INTERFACE m_axi port=input_state_1 depth=1024 bundle=gmem0
#pragma HLS INTERFACE m_axi port=input_state_2 depth=1024 bundle=gmem0
#pragma HLS INTERFACE m_axi port=gate_matrix depth=32 bundle=gmem0
#pragma HLS INTERFACE m_axi port=output_state_1 depth=1024 bundle=gmem0
#pragma HLS INTERFACE m_axi port=output_state_2 depth=1024 bundle=gmem0
#pragma HLS INTERFACE s_axilite port=control
#pragma HLS INTERFACE s_axilite port=target
#pragma HLS INTERFACE s_axilite port=num_qubits
#pragma HLS INTERFACE s_axilite port=return
        
        int num_states = 1 << num_qubits; // Total states (2^num_qubits)
        int gate_size = (control == -1) ? 2 : 4; // Determine gate type based on control   
        
        int half_states = num_states / 2; // States in each half
/*
std::cout << "Gate Matrix:" << std::endl;
for (int i = 0; i < gate_size * gate_size; ++i) {
    std::cout << "Index " << i << ": " 
              << gate_matrix[i].real() << "+" << gate_matrix[i].imag() << "i" << std::endl;
}     
*/
// Single-qubit gate operation
if (gate_size == 2) {
    single_qubit_loop: for (int i = 0; i < num_states; ++i) {
        #pragma HLS PIPELINE II=1
        #pragma HLS UNROLL factor=3
        int target_bit = (i >> target) & 1; // Extract the target bit (0 or 1)
        int flipped_i = i ^ (1 << target);  // Calculate flipped index by toggling the target bit

        // Apply the gate matrix
        if (i < flipped_i) {
            // Handle both states (i and flipped_i) only once
            if (i < half_states && flipped_i < half_states) {
                // Both indices in the first half
                output_state_1[i] = gate_matrix[target_bit * 2] * input_state_1[i] +
                                    gate_matrix[1 - target_bit] * input_state_1[flipped_i];
                output_state_1[flipped_i] = gate_matrix[2 + target_bit * 2] * input_state_1[i] +
                                            gate_matrix[3 - target_bit] * input_state_1[flipped_i];
            } else if (i >= half_states && flipped_i >= half_states) {
                // Both indices in the second half
                int local_i = i - half_states;
                int local_flipped_i = flipped_i - half_states;

                output_state_2[local_i] = gate_matrix[target_bit * 2] * input_state_2[local_i] +
                                          gate_matrix[1 - target_bit] * input_state_2[local_flipped_i];
                output_state_2[local_flipped_i] = gate_matrix[2 + target_bit * 2] * input_state_2[local_i] +
                                                  gate_matrix[3 - target_bit] * input_state_2[local_flipped_i];
            } else if (i < half_states && flipped_i >= half_states) {
                // i in the first half, flipped_i in the second half
                int local_flipped_i = flipped_i - half_states;

                output_state_1[i] = gate_matrix[target_bit * 2] * input_state_1[i] +
                                    gate_matrix[1 - target_bit] * input_state_2[local_flipped_i];
                output_state_2[local_flipped_i] = gate_matrix[2 + target_bit * 2] * input_state_1[i] +
                                                  gate_matrix[3 - target_bit] * input_state_2[local_flipped_i];
            } else if (i >= half_states && flipped_i < half_states) {
                // i in the second half, flipped_i in the first half
                int local_i = i - half_states;

                output_state_2[local_i] = gate_matrix[target_bit * 2] * input_state_2[local_i] +
                                          gate_matrix[1 - target_bit] * input_state_1[flipped_i];
                output_state_1[flipped_i] = gate_matrix[2 + target_bit * 2] * input_state_2[local_i] +
                                            gate_matrix[3 - target_bit] * input_state_1[flipped_i];
            }
        }
    }
}







        // Two-qubit gate operation
                // Two-qubit gate operation
        else {
            copy_loop: for (int i = 0; i < num_states; ++i) {
        #pragma HLS PIPELINE II=1
        #pragma HLS UNROLL factor=3
            output_state_1[i] = input_state_1[i];
            output_state_2[i] = input_state_2[i];
        } 
two_qubit_loop: for (int i = 0; i < num_states; ++i) {
    #pragma HLS PIPELINE II=1
    #pragma HLS UNROLL factor=3

    int control_bit = (i >> control) & 1;  // Extract control bit
    int target_bit = (i >> target) & 1;   // Extract target bit

    if (control_bit == 1) {
        int flipped_i = i ^ (1 << target);  // Flip the target bit

        std::complex<float> temp;  // Temporary variable for swaps

        if (i < half_states && flipped_i < half_states) {
            // Both indices in the first half
            temp = input_state_1[flipped_i];
            output_state_1[flipped_i] = input_state_1[i];
            output_state_1[i] = temp;
        } else if (i >= half_states && flipped_i >= half_states) {
            // Both indices in the second half
            int local_i = i - half_states;
            int local_flipped_i = flipped_i - half_states;

            temp = input_state_2[local_flipped_i];
            output_state_2[local_flipped_i] = input_state_2[local_i];
            output_state_2[local_i] = temp;
        } else if (i < half_states && flipped_i >= half_states) {
            // i in the first half, flipped_i in the second half
            int local_flipped_i = flipped_i - half_states;

            temp = input_state_2[local_flipped_i];
            output_state_2[local_flipped_i] = input_state_1[i];
            output_state_1[i] = temp;
        } else if (i >= half_states && flipped_i < half_states) {
            // i in the second half, flipped_i in the first half
            int local_i = i - half_states;

            temp = input_state_1[flipped_i];
            output_state_1[flipped_i] = input_state_2[local_i];
            output_state_2[local_i] = temp;
        }
    } else {
        // If control bit is 0, copy input state to output state unchanged
        if (i < half_states) {
            output_state_1[i] = input_state_1[i];
        } else {
            output_state_2[i - half_states] = input_state_2[i - half_states];
        }
    }
}



            
            //end of Two-qubit loop

        }
        
       // Display contents of output_states
/*
std::cout << "Contents of output_state_1:" << std::endl;
for (int i = 0; i < num_states / 2; ++i) {
    std::cout << "Index " << i << ": "
              << output_state_1[i].real() << "+" << output_state_1[i].imag() << "i" << std::endl;
}

for (int i = 0; i < num_states / 2; ++i) {
    std::cout << "Index " << i + (num_states / 2) << ": "
              << output_state_2[i].real() << "+" << output_state_2[i].imag() << "i" << std::endl;
}
*/

    }
}
