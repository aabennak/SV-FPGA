
Version Explanation:
- version_1.0: Contains the Basic implementation, optimizations include loop unrolling and pipelineing.
- version_1.1: Same as the version 1.0 with extra optimizations including: moving the copy loop inside the 2-qubit gate loop and the dataflow pragma
- version_1.1a: built on the version 1.1, except that it uses two buffers instead of one for each state vector(input and output) in order to run 29-qubit circuits.
