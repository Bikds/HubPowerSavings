# HubPowerSavings

BULK and ISOC Traffic:

C Program to calculate USB Hub Power Savings.

"hub_power_scheduling_bulkisoc.c" is the C program which simulates and outputs the calculation of power savings using the proposed dynamic scheduling algorithm, taking into account whether traffic is isochronous or bulk.

"hub_config_bulkandisoc.h" contains values used by the program, including entry/exit latencies and power savings in each Ux state.

"hub_config_bulkandisoc.in" provides the set of input values specifying information about the various downstream ports, including upstream/downstream traffic for both bulk and isochronus traffic.


BULK ONLY:
C Program for USB Hub Power Savings

"hub_power_scheduling.c" is the C program which calculates the total power savings using our proposed dynamic scheduling algorithm.

The file "hub_config.h" defines some of the values used by the program such as the power consumption in various states and the entry and exit times for the various low-power states.

"hub_config.in" provides the set of input values representing the link width and frequency of each DFP and UFP, along with the bandwidth demand for each DFP in each direction. 

The C program computes the power savings for various scenarios, including scheduling interval for each of the low power states. The output is printed on the screen. The system designer can choose to include this scheduling algorithm and control the power savings dynamically in a real system. 

"hub.out" contains the results of our program "hub_power_scheduling.c" for "hub_config.h" and "hub_config.in" by redirecting the screen output to "hub.out". This is provided to give the reader an idea of the type of output to expect without having to run the program.
