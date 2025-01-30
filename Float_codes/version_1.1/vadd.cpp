#include <complex>

extern "C" {
    void vadd(
        std::complex<float> *state_vector,       // Input complex state vector
        std::complex<float> *gate_matrix,        // Input gate matrix (2x2 for single-qubit, 4x4 for two-qubit)
        std::complex<float> *output_state_vector,// Output complex state vector
        int control,                             // Control qubit index (-1 for no control)
        int target,                              // Target qubit index
        int num_qubits                           // Number of qubits
    ) {
#pragma HLS INTERFACE m_axi port=state_vector depth=1024 bundle=gmem0
#pragma HLS INTERFACE m_axi port=gate_matrix depth=32 bundle=gmem1
#pragma HLS INTERFACE m_axi port=output_state_vector depth=1024 bundle=gmem2
#pragma HLS INTERFACE s_axilite port=control
#pragma HLS INTERFACE s_axilite port=target
#pragma HLS INTERFACE s_axilite port=num_qubits
#pragma HLS INTERFACE s_axilite port=return

#pragma HLS DATAFLOW // Enables concurrent memory access, computation, and write-back

        int num_states = 1 << num_qubits; // Total states (2^num_qubits)
        int gate_size = (control == -1) ? 2 : 4; // Determine gate type based on control

        

        // Single-qubit gate operation
        if (gate_size == 2) {
            single_qubit_loop: for (int i = 0; i < num_states; ++i) {
                #pragma HLS PIPELINE II=1
                #pragma HLS UNROLL factor=3
                int target_bit = (i >> target) & 1;
                int flipped_i = i ^ (1 << target);

                if (i < flipped_i) {
                    output_state_vector[i] = gate_matrix[0 + target_bit] * state_vector[i] +
                                             gate_matrix[1 - target_bit] * state_vector[flipped_i];
                    output_state_vector[flipped_i] = gate_matrix[2 + target_bit] * state_vector[i] +
                                                     gate_matrix[3 - target_bit] * state_vector[flipped_i];
                }
            }
        } 
        // Two-qubit gate operation
        else {
        // Initialize output_state_vector to be a copy of the original state_vector
        // Enable parallel copying
        copy_loop: for (int i = 0; i < num_states; ++i) {
        #pragma HLS PIPELINE II=1
        #pragma HLS UNROLL factor=3
            output_state_vector[i] = state_vector[i];
        }
            two_qubit_loop: for (int i = 0; i < num_states; ++i) {
                #pragma HLS PIPELINE II=1
                #pragma HLS UNROLL factor=3
                int control_bit = (i >> control) & 1;
                int target_bit = (i >> target) & 1;

                if (control_bit == 1) {
                    int flipped_i = i ^ (1 << target);
                    if (target_bit == 0) {
                        // Swap i and flipped_i
                        std::complex<float> temp = output_state_vector[i];
                        output_state_vector[i] = state_vector[flipped_i];
                        output_state_vector[flipped_i] = temp;
                    }
                }
            }
        }
    }
}
