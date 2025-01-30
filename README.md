This code was operational using the following system specifications...

Software:
- OS: RHE 7.9
- Synthesis Software: Vivado/Vitis 2023.2

Hardware:
- Data center Accelerator card: Alveo U200
- Processor: Intel i9-10980XE

File Functionality:
- host.cpp: Contains the code responsible for handling the data on the workstation side that is inputted onto the FPGA or received from it. It feeds the data to the synthesized FPGA in a digestible format and stores the FPGA's output data.
- vadd.cpp: This code is translated from C++ to RTL using an interpreter(in our setup, it was Vivado/Vitis 2023.2). This RTL code is then used to alter the FPGA's configuration to perform the code's designated function.
- config file: This file designates the FPGA's buffer allocation. In our case, the Alveo U200 has four 16 GB DDR4 memory chips, which can be used as buffers to handle the data coming in from the workstation to the FPGA.

