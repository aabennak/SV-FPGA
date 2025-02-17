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


        int num_states = 1 << num_qubits; // Total states (2^num_qubits)
        int gate_size = (control == -1) ? 2 : 4; // Determine gate type based on control

        
if (gate_size == 2) {
    single_qubit_loop: for (int i = 0; i < num_states; ++i) {
        #pragma HLS PIPELINE II=1
        //#pragma HLS UNROLL factor=7

        // Get the bit at the target position.
        int bit = (i >> target) & 1;
        // Compute the partner index by flipping the target bit.
        int partner = i ^ (1 << target);

        if (bit == 0) {
            // When the target bit is 0, i is the lower index.
            output_state_vector[i] = gate_matrix[0] * state_vector[i] +
                                       gate_matrix[1] * state_vector[partner];
        }
        else {
            // When the target bit is 1, i is the higher index.
            output_state_vector[i] = gate_matrix[2] * state_vector[partner] +
                                       gate_matrix[3] * state_vector[i];
        }
    }
}



        // Two-qubit gate operation
        else {
        // Initialize output_state_vector to be a copy of the original state_vector
        // Enable parallel copying
        copy_loop: for (int i = 0; i < num_states; ++i) {
        #pragma HLS PIPELINE II=1
        #pragma HLS UNROLL factor=6
            output_state_vector[i] = state_vector[i];
        }
// Loop over blocks of indices with the control bit set.
two_qubit_loop: 
for (int i = (1 << control); i < num_states; i += (1 << (control + 1))) {
    #pragma HLS PIPELINE II=1
    #pragma HLS UNROLL factor=6

    // Each block covers a contiguous range of size (1 << control).
    const int block_start = i;
    const int block_end   = i + (1 << control);

    if (target >= control) {
        // When the target bit is not among the bits that vary in the block,
        // its value is constant over the entire block.
        if ((block_start & (1 << target)) == 0) {
            // The target bit is 0 for every index in this block.
            for (int idx = block_start; idx < block_end; idx++) {
                int flipped_idx = idx ^ (1 << target);
                std::complex<float> temp = output_state_vector[idx];
                output_state_vector[idx] = state_vector[flipped_idx];
                output_state_vector[flipped_idx] = temp;
            }
        }
    }
    else {
        // When target < control, the block spans several periods of the target bit.
        // In each period of length 'period', the first half have target bit 0.
        const int period = 1 << (target + 1);
        const int half_period = 1 << target;
        int idx = block_start;
        while (idx < block_end) {
            int offset = idx & (period - 1);
            if (offset < half_period) {
                // idx has target bit 0; perform the swap.
                int flipped_idx = idx ^ (1 << target);
                std::complex<float> temp = output_state_vector[idx];
                output_state_vector[idx] = state_vector[flipped_idx];
                output_state_vector[flipped_idx] = temp;
                idx++; // move to next index
            }
            else {
                // Skip the remainder of this period (indices where target bit is 1)
                idx += (period - offset);
            }
        }
    }
}



        }
    }
}
