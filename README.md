**Q2SV git Repository**

This code repository was operational using the following system specifications...

Software:
- OS: RHEL 7.9
- Synthesis Software: Vivado/Vitis 2023.2

Hardware:
- Data center Accelerator card: Alveo U200
- Processor: Intel i9-10980XE

File Functionality:
- host.cpp: Contains the code responsible for handling the data on the workstation side that is inputted onto the FPGA or received from it. It feeds the data to the synthesized FPGA in a digestible format and stores the FPGA's output data.
- vadd.cpp: This code is translated from C++ to RTL using an interpreter(in our setup, it was Vivado/Vitis 2023.2). This RTL code is then used to alter the FPGA's configuration to perform the code's designated function.
- config file: This file designates the FPGA's buffer allocation. In our case, the Alveo U200 has four 16 GB DDR4 memory chips, which can be used as buffers to handle the data from the workstation to the FPGA.

File Contents:
- Qasm2CSV.ipynb: Contains a notebook script to produce a csv file from an OpenQasm file; by default, it is set to produce one from "qf21_n15_transpiled.qasm".
- qf21_n15_transpiled.qasm: An OpenQasm file taken from QASMBench as an example.
- Float_codes Folder: Contains all implementations that represent state vector values in float format.
- half_codes Folder: Contains all implementations that represent state vector values in half precision format.
- Script Folder: Contains the scripts for both a software emulation and real hardware synthesis.

Instructions:
- Download a qasm file of your own liking and run Qasm2CSV.ipynb either as a notebook file or a python script. Please make sure to adjust the filename parameter accordingly to match your qasm file before running it.
- Place the produced csv file in the same diretory as the example.zip(under u200).
- Run either the sw_emu or hw script according to you preference.
- The produced output state vector csv file should be under the sw_emu or hw diretory.

Version summary:
- version_1.0: Contains the Basic implementation, optimizations include loop unrolling and pipelineing.
- version_1.1: Same as the version 1.0 with extra optimizations including: moving the copy loop inside the 2-qubit gate loop and the dataflow pragma.
- version_1.2: Removed dataflow pragma since it introduced more delays, restructured 1-qubit op loop for contigous write locations, and restructured 2-qubit loop for targetted swaps rather than iterate over the entire state vector.
- version_1.1a: built on the version 1.1, except that it uses two buffers instead of one for each state vector(input and output) in order to run 29-qubit circuits.
- float impl: Is the one and only float implementation of the Q2SV system
![alt text](https://github.com/aabennak/SV-FPGA/blob/main/version_map.png?raw=true)
