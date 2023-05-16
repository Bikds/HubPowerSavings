#include <stdio.h>
#include <math.h>
#include "hub_config_bulkandisoc.h"
main()
{
    int ufp_width, ufp_freq;                                                           // UFP width and freq (note 5 GT/s changed to 4 GT/s)
    int num_dfp;                                                                       // number of downstream ports
    int dfp_usb_type[15];                                                              // Type of USB:1, 2, 3, 4
    int dfp_width[15], dfp_freq[15];                                                   // Only meaningful for USB3 and above; else 0
    double bw_demand_ib_temp, isoc_frac_ib_temp, bw_demand_ob_temp, isoc_frac_ob_temp; // temp variables to read from file
    double dfp_bulk_bw_demand_ib[15], dfp_bulk_bw_demand_ob[15], dfp_isoc_bw_demand_ib[15], dfp_isoc_bw_demand_ob[15];
    // Projected B/W demand in each direction for each DFP
    int dfp_isoc_latency_tol_ib[15], dfp_isoc_latency_tol_ob[15]; // in usec - 0 means no limit
    int dfp_bulk_latency_tol_ib[15], dfp_bulk_latency_tol_ob[15]; // in usec - 0 means no limit
    int dfp_latency_tol[15];
    int isoc_latency_tol = 0, bulk_latency_tol = 0, ufp_latency_tol = 0;
    short int dfp_allotted_isoc_bw[15];
    // Minimum tolerance of the two latencies - Lower of these two decides the interval when the ufp needs to service
    double net_ufp_demand = 0, net_dn_demand = 0, net_ib_bulk_demand = 0, net_ob_bulk_demand = 0;
    // represents projected b/w demand
    // per direction from all the DPs (util * raw b/w)
    double ufp_bw; // total b/w available per direction in UP
    double ufp_bw_allotted_ib_isoc = 0, ufp_bw_allotted_ob_isoc = 0, ufp_bw_avail_ib_bulk = 0, ufp_bw_avail_ob_bulk = 0;
    double dfp_raw_bw_aggr;  // Raw B/W total of all DPs in either direction
    double raw_bw_allot[15]; // Raw B/W allottment if each DP requested full B/W
    double allotted_bw_ib[15], allotted_bw_ob[15];
    // Actual b/w allocated to each DP by the host/hub depending on their
    // demand and their fair share of b/w allocation
    //

    //
    // The following are used during the b/w allocation
    //
    double tot_bw_allot_bulk_ib = 0, tot_bw_allot_bulk_ob = 0; // Used during b/w allocation to track
                                                               // how much bw of the UFP has been allotted so far in each direction
    short int dfp_bw_demand_met_ib[15], dfp_bw_demand_met_ob[15];
    // The above is used to denote whether b/w demand is fully met or not
    // The following two variables are the remainder
    // Following two variable: For each DP/direction, we count if the
    // b/w is allocated or not after first pass - if yes, that DP's ratio is
    // made 0 - these represent the remaining ratios added - so in the second
    // pass we do the proportionate b/w distribution to those ports whose
    // full demand can not be met
    //
    // The following two variables track the actual b/w remaining after first pass
    //
    // The fractional b/w in each direction unused after first pass due to demand being greater
    // than the allotted amount
    double frac_bw_remain_after_first_pass_ib = 0;
    double frac_bw_remain_after_first_pass_ob = 0;
    //
    // Actual UFP b/w remaining in each direction after the first pass
    double bw_remaining_after_first_pass_ib;
    double bw_remaining_after_first_pass_ob;
    //
    double bw_isoc_remain_ib;
    double bw_isoc_remain_ob;

    //
    // Actual utilization after the 2-pass b/w allocation
    double dfp_util_ib[15], dfp_util_ob[15], dfp_util_link[15];
    double ufp_util_ib, ufp_util_ob, ufp_util_link;
    //
    double time_u0_or_entry_exit_u1, time_u0_or_entry_exit_u2, time_u0_or_entry_exit_u3;
    // The above represents the time the Port is in U0 or in entry/exit to a low-power state, as
    // the entry into or exit from a low-power state Ux is considered to consume the same power as
    // U0
    double time_tot; // total time per scheduling interval

    double total_power_savings_hub_u1, frac_u1; // Power savings and fractional time with U1
    double total_power_savings_hub_u2, frac_u2; // Power savings and fractional time with U2
    double total_power_savings_hub_u3, frac_u3; //// Power savings and fractional time with U3
    double power_savings_port_u1, power_savings_port_u2, power_savings_port_u3, power_savings_port_hybrid;
    // partial per-port calculation towards the above
    double total_power_savings_hub_hybrid;

    int i, j, time_mult;
    double raw_bw_allot_tot = 0;
    FILE *fp1, *fp2;

    printf("-----------------------------------\n");
    printf("Enter the Hub configuration in 'hub_config_bulkandisoc.in' file\n");
    printf("Expected format: Upstream Port (UP) width, frequency for USB3+ (expected to have USB2) \n \t followed by no of downstream ports followed by for DP the USB Type, width, frequency, \n\t Upstream B/W demand, isoc fraction, isoc followed by non-isoc tolerable schedule interval in usec (0 means arbitrarily large), Downstream B/W demand, isoc fraction, isoc followed by non-isoc tolerable schedule interval in usec (example below)\n hub_config_bulkandisoc.in Format as follows ...\n\n");
    printf("2, 20 // Line 1: Means x2 at 20G for UP\n");
    printf("4 // Line 2: No of downstream Ports in Hub is 4 - this means 4 more lines as follows\n\n");
    printf("3 1 5 2.0 0 0 0 0.4 0 0 0 \n\t Downstream Port 0 is USB 3; 1 wide; 5 GT/s, \n\tUpstream B/W demand 2.0 Gb/s, no isoc - all bulk, any latency \n\t Downstream B/W demand of 0.4 Gb/s, no-isoc - all bulk and any latency\n\n");
    printf("2 1 5 0.5 0.8 125 0 0.1 1 125 0 \n\t Downstream Port 1 is USB 2; next 2 numbers for width/frequency are dont care, \n\t Upstream B/W itilization is 0.5, 0.8 isoc/ 0.2 bulk, isoc 125 usec latency tolerance, non-isoc no latency bound \n\t Downstream B/W utilization 0.1, all isoc with 125usec latency \n\n");
    printf("3 2 10 10.0 0.1 125 1000 18.0 0 0 0 \n\t Downstream Port 2 is USB 3.2; 2 wide; 10 GT/s, \n\t Upstream B/W demand is 10 Gb/s, 0.1 isoc / 0.9 bulk, isoc latency 125 us, bulk latency requirement 1000 us \n\t Downstream B/W demand is 18.0 Gb/s, all bulk and no latency requirement isoc/bulk\n\n");
    printf("4 2 20 20.0 0 0 0 36.0 0 0 0 \n\t Downstream Port 3 is USB 4; 2 wide; 20 GT/s, \n\t Upstream B/W demand is 20.0 Gb/s, no isoc, no latency restriction isoc/ bulk \n\t Downstream B/W demand is 36.0 Gb/s, no isoc, and no latency requirement isoc / bulk\n\n");
    printf("-----------------------------------\n");
    //

    // Read from input file and print out
    //

    fp1 = fopen("hub_config_bulkandisoc.in", "r");
    fp2 = fopen("results", "w");
    fscanf(fp1, "%d %d", &ufp_width, &ufp_freq);
    if (ufp_freq == 5)
        ufp_freq = 4;
    ufp_bw = (double)ufp_freq * (double)ufp_width;
    fscanf(fp1, "%d", &num_dfp);
    printf("USB Hub Specification considered:\n");
    printf("UP: x%d, %dGT/s, %d Downstream Ports\n", ufp_width, ufp_freq, num_dfp);
    dfp_raw_bw_aggr = 0;
    for (i = 0; i < num_dfp; i++)
    {
        fscanf(fp1, "%d %d %d %lf %lf %d %d %lf %lf %d %d", &dfp_usb_type[i], &dfp_width[i], &dfp_freq[i], &bw_demand_ib_temp, &isoc_frac_ib_temp, &dfp_isoc_latency_tol_ib[i], &dfp_bulk_latency_tol_ib[i], &bw_demand_ob_temp, &isoc_frac_ob_temp, &dfp_isoc_latency_tol_ob[i], &dfp_bulk_latency_tol_ob[i]);
        dfp_bulk_bw_demand_ib[i] = bw_demand_ib_temp * (1.0 - isoc_frac_ib_temp);
        dfp_isoc_bw_demand_ib[i] = bw_demand_ib_temp * isoc_frac_ib_temp;
        dfp_bulk_bw_demand_ob[i] = bw_demand_ob_temp * (1.0 - isoc_frac_ob_temp);
        dfp_isoc_bw_demand_ob[i] = bw_demand_ob_temp * isoc_frac_ob_temp;

        net_ib_bulk_demand += dfp_bulk_bw_demand_ib[i];
        net_ob_bulk_demand += dfp_bulk_bw_demand_ob[i];

        if (dfp_usb_type[i] < 3)
        {
            dfp_width[i] = 0;
            dfp_freq[i] = 0;
        }
        if (dfp_freq[i] == 5)
            dfp_freq[i] = 4;
        dfp_raw_bw_aggr += (double)(dfp_width[i] * dfp_freq[i]);

        printf("DP %d: USB %d, width: %d, %d GT/s, Isoc Bw (%lf up, %lf dn) Bulk BW (%lf up, %lf dn) latency sensitivity (us):isoc (%d up, %d dn), Bulk (%d up, %d dn))\n", i, dfp_usb_type[i], dfp_width[i], dfp_freq[i], dfp_isoc_bw_demand_ib[i], dfp_isoc_bw_demand_ob[i], dfp_bulk_bw_demand_ib[i], dfp_bulk_bw_demand_ob[i], dfp_isoc_latency_tol_ib[i], dfp_isoc_latency_tol_ob[i], dfp_bulk_latency_tol_ib[i], dfp_bulk_latency_tol_ob[i]);
        fprintf(fp2, "DP %d: USB %d, width: %d, %d GT/s, Isoc Bw (%lf up, %lf dn) Bulk BW (%lf up, %lf dn) latency sensitivity (us):isoc (%d up, %d dn), Bulk (%d up, %d dn))\n", i, dfp_usb_type[i], dfp_width[i], dfp_freq[i], dfp_isoc_bw_demand_ib[i], dfp_isoc_bw_demand_ob[i], dfp_bulk_bw_demand_ib[i], dfp_bulk_bw_demand_ob[i], dfp_isoc_latency_tol_ib[i], dfp_isoc_latency_tol_ob[i], dfp_bulk_latency_tol_ib[i], dfp_bulk_latency_tol_ob[i]);
        net_ufp_demand += bw_demand_ib_temp;
        net_dn_demand += bw_demand_ob_temp;
        // printf("i=%d, up = %lf, dn = %lf\n",i,net_ufp_demand,net_dn_demand);
    }
    fclose(fp1);
    printf("Net UP B/W (%lf up, %lf dn) vs net demand by DP ports (%lf up, %lf dn) vs net bulk demand by DP ports (%lf up, %lf dn) vs raw b/w demand in DP %lf in each direction\n", ufp_bw, ufp_bw, net_ufp_demand, net_dn_demand, net_ib_bulk_demand, net_ob_bulk_demand, dfp_raw_bw_aggr);
    fprintf(fp2, "Net UP B/W (%lf up, %lf dn) vs net demand by DP ports (%lf up, %lf dn) vs net bulk demand by DP ports (%lf up, %lf dn) vs raw b/w demand in DP %lf in each direction\n", ufp_bw, ufp_bw, net_ufp_demand, net_dn_demand, net_ib_bulk_demand, net_ob_bulk_demand, dfp_raw_bw_aggr);
    if (USB3_ENTRY_U2 == 1.0)
    {
        fprintf(fp2, "--------------------------------------------------\n");
        fprintf(fp2, "Net UP B/W (%lf up, %lf dn) vs net demand by DP ports (%lf up, %lf dn) vs net bulk demand by DP ports (%lf up, %lf dn) vs raw b/w demand in DP %lf in each direction\n", ufp_bw, ufp_bw, net_ufp_demand, net_dn_demand, net_ib_bulk_demand, net_ob_bulk_demand, dfp_raw_bw_aggr);
    }
    fprintf(fp2, "\n---Entry, Exit Latency for U1/U2/U3: U1 (%lf, %lf), U2 (%lf, %lf), U3 (%lf, %lf)\n---\n", USB3_ENTRY_U1, USB3_EXIT_U1, USB3_ENTRY_U2, USB3_EXIT_U2, USB3_ENTRY_U3, USB3_EXIT_U3);
    printf("----Raw B/W Allocation to DP based on width/ frequency -------\n");
    for (i = 0; i < num_dfp; i++)
    {
        raw_bw_allot[i] = (double)(dfp_width[i] * dfp_freq[i]) / dfp_raw_bw_aggr;
        printf("DP Port %d: Raw Util: %lf\n", i, raw_bw_allot[i]);
        raw_bw_allot_tot += raw_bw_allot[i];
    }
    printf("Total Raw B/W allot fraction: %lf\n", raw_bw_allot_tot);
    printf("---------------------\n");

    // Isoc Bandwidth Allocation: Pass 0 - we simply allocate the requested bandwidth
    // If any direction falls short, we just remove that isoc demand from the DFP(s) when we fall short
    // FUTURE IDEA: Sort these to minimize the number of ports not serviced
    // Right now it is FCFS - which may reflect reality as different DFPs may make request at different times
    // We allot a maximum MAX_ISOC_FRAC b/w

    bw_isoc_remain_ib = bw_isoc_remain_ob = MAX_ISOC_FRAC * ufp_bw;
    ufp_bw_avail_ib_bulk = ufp_bw_avail_ob_bulk = ufp_bw;
    for (i = 0; i < num_dfp; i++)
    {
        dfp_allotted_isoc_bw[i] = 0;
        allotted_bw_ib[i] = allotted_bw_ob[i] = 0;
        if ((dfp_isoc_bw_demand_ib[i] > bw_isoc_remain_ib) || (dfp_isoc_bw_demand_ob[i] > bw_isoc_remain_ob))
        {
            printf("DFP %d's isoc demand (%lf, %lf) more than what the UFP has available (%lf, %lf).. so no allocation\n", i, dfp_isoc_bw_demand_ib[i], dfp_isoc_bw_demand_ob[i], bw_isoc_remain_ib, bw_isoc_remain_ob);
        }
        else
        {
            allotted_bw_ib[i] = dfp_isoc_bw_demand_ib[i];
            allotted_bw_ob[i] = dfp_isoc_bw_demand_ob[i];
            bw_isoc_remain_ib -= dfp_isoc_bw_demand_ib[i];
            bw_isoc_remain_ob -= dfp_isoc_bw_demand_ob[i];
            ufp_bw_allotted_ib_isoc += dfp_isoc_bw_demand_ib[i];
            ufp_bw_allotted_ob_isoc += dfp_isoc_bw_demand_ob[i];
            ufp_bw_avail_ib_bulk -= dfp_isoc_bw_demand_ib[i];
            ufp_bw_avail_ob_bulk -= dfp_isoc_bw_demand_ob[i];
            if ((dfp_isoc_bw_demand_ib[i] > 0) || (dfp_isoc_bw_demand_ob[i] > 0))
            {
                dfp_allotted_isoc_bw[i] = 1;
            }
        }
        // Calculating the latency tolerance for each port, isoc vs bulk, and upstream
        // The advertised numbers for each DFP is considered for isoc/bulk in each direction
        // only if it is allotted b/w so far for isoc or will be allotted for bulk
        // Bulk is guaranteed some allocation since UFP reserves some nb/w for bulk and then
        // distributes in an equitable mannner to each port asking for it
        //
        if ((allotted_bw_ib[i] > 0) && (dfp_isoc_latency_tol_ib[i] > 0) &&
            ((dfp_isoc_latency_tol_ib[i] < isoc_latency_tol) || (isoc_latency_tol == 0)))
        {
            isoc_latency_tol = dfp_isoc_latency_tol_ib[i];
        }
        if ((allotted_bw_ob[i] > 0) && (dfp_isoc_latency_tol_ob[i] > 0) &&
            ((dfp_isoc_latency_tol_ob[i] < isoc_latency_tol) || (isoc_latency_tol == 0)))
        {
            isoc_latency_tol = dfp_isoc_latency_tol_ob[i];
        }
        if ((dfp_bulk_bw_demand_ib[i] > 0) && (dfp_bulk_latency_tol_ib[i] > 0) &&
            ((dfp_bulk_latency_tol_ib[i] < bulk_latency_tol) || (bulk_latency_tol == 0)))
        {
            bulk_latency_tol = dfp_bulk_latency_tol_ib[i];
        }
        if ((dfp_bulk_bw_demand_ob[i] > 0) && (dfp_bulk_latency_tol_ob[i] > 0) &&
            ((dfp_bulk_latency_tol_ob[i] < bulk_latency_tol) || (bulk_latency_tol == 0)))
        {
            bulk_latency_tol = dfp_bulk_latency_tol_ob[i];
        }
        if ((isoc_latency_tol != 0) && ((isoc_latency_tol < ufp_latency_tol) || (ufp_latency_tol == 0)))
            ufp_latency_tol = isoc_latency_tol;
        if ((bulk_latency_tol != 0) && ((bulk_latency_tol < ufp_latency_tol) || (ufp_latency_tol == 0)))
            ufp_latency_tol = bulk_latency_tol;
        // Now compute the DFP port
        dfp_latency_tol[i] = 0;
        if ((allotted_bw_ib[i] > 0) && (dfp_isoc_latency_tol_ib[i] > 0) &&
            ((dfp_isoc_latency_tol_ib[i] < dfp_latency_tol[i]) || (dfp_latency_tol[i] == 0)))
        {
            dfp_latency_tol[i] = dfp_isoc_latency_tol_ib[i];
        }
        if ((allotted_bw_ib[i] > 0) && (dfp_isoc_latency_tol_ob[i] > 0) &&
            ((dfp_isoc_latency_tol_ob[i] < dfp_latency_tol[i]) || (dfp_latency_tol[i] == 0)))
        {
            dfp_latency_tol[i] = dfp_isoc_latency_tol_ob[i];
        }
        if ((dfp_bulk_bw_demand_ib[i] > 0) && (dfp_bulk_latency_tol_ib[i] > 0) &&
            ((dfp_bulk_latency_tol_ib[i] < dfp_latency_tol[i]) || (dfp_latency_tol[i] == 0)))
        {
            dfp_latency_tol[i] = dfp_bulk_latency_tol_ib[i];
        }
        if ((dfp_bulk_bw_demand_ob[i] > 0) && (dfp_bulk_latency_tol_ob[i] > 0) &&
            ((dfp_bulk_latency_tol_ob[i] < dfp_latency_tol[i]) || (dfp_latency_tol[i] == 0)))
        {
            dfp_latency_tol[i] = dfp_bulk_latency_tol_ob[i];
        }

        printf("Isoc B/W allocation for DFP %d: (%lf, %lf) - UFP has (%lf,%lf) allotted to isoc and (%lf, %lf) remaining in bulk\n", i, allotted_bw_ib[i], allotted_bw_ob[i], ufp_bw_allotted_ib_isoc, ufp_bw_allotted_ob_isoc, ufp_bw_avail_ib_bulk, ufp_bw_avail_ob_bulk);
        printf("Latency Tolerance aggregate min: UFP: Isoc %d, Bulk %d, Combined %d, DFP %d is %d combined\n", isoc_latency_tol, bulk_latency_tol, ufp_latency_tol, i, dfp_latency_tol[i]);
    }

    double ufp_max_avail_bw_bulk_ib, ufp_max_avail_bw_bulk_ob, ufp_allotted_bw_isoc_ib, ufp_allotted_bw_isoc_ob;
    //
    ufp_max_avail_bw_bulk_ib = ufp_bw_avail_ib_bulk;
    ufp_max_avail_bw_bulk_ob = ufp_bw_avail_ob_bulk;
    ufp_allotted_bw_isoc_ib = ufp_bw - ufp_max_avail_bw_bulk_ib;
    ufp_allotted_bw_isoc_ob = ufp_bw - ufp_max_avail_bw_bulk_ob;
    printf("UFP: Max BW: %lf, Max Avail Bulk IB: %lf, Max Avail Bulk OB: %lf, Allotted Isoc IB: %lf, Allotted Isoc OB: %lf\n", ufp_bw, ufp_max_avail_bw_bulk_ib, ufp_max_avail_bw_bulk_ob, ufp_allotted_bw_isoc_ib, ufp_allotted_bw_isoc_ob);

    bw_remaining_after_first_pass_ib = ufp_max_avail_bw_bulk_ib;
    bw_remaining_after_first_pass_ob = ufp_max_avail_bw_bulk_ob;

    //
    // Now we do the bulk b/w allocation to each DP in each direction
    // based on the actual b/w demand and the fair b/w allocation
    // (lower of the two). This happens in two phases. In the first
    // phase we do the lower of the two and then any remaining b/w
    // from the available UP b/w is redistributed proportionately among
    // those DP ports whose b/w demand is more than the fair allocation
    //
    //
    printf("-----Bulk BW: First Pass B/W allocation to DP ports-----\n");
    for (i = 0; i < num_dfp; i++)
    {
        if (((raw_bw_allot[i] * ufp_max_avail_bw_bulk_ib) < dfp_bulk_bw_demand_ib[i]) &&
            (net_ib_bulk_demand > ufp_max_avail_bw_bulk_ib))
        {
            dfp_bw_demand_met_ib[i] = 0;
            frac_bw_remain_after_first_pass_ib += raw_bw_allot[i];
            bw_remaining_after_first_pass_ib += (dfp_isoc_bw_demand_ib[i] * dfp_allotted_isoc_bw[i]); // Add any isoc b/w allotted to the device waiting for seocnd
                                                                                                      // pass to be considered during equitable distro in 2nd pass - that way a device's isoc allocation is considered during proportionate distribution
        }
        else
        {
            allotted_bw_ib[i] += dfp_bulk_bw_demand_ib[i];
            tot_bw_allot_bulk_ib += dfp_bulk_bw_demand_ib[i];
            dfp_bw_demand_met_ib[i] = 1;
            bw_remaining_after_first_pass_ib -= dfp_bulk_bw_demand_ib[i];
        }
        //
        if (((raw_bw_allot[i] * ufp_max_avail_bw_bulk_ob) < dfp_bulk_bw_demand_ob[i]) &&
            (net_ob_bulk_demand > ufp_max_avail_bw_bulk_ob))
        {
            dfp_bw_demand_met_ob[i] = 0;
            frac_bw_remain_after_first_pass_ob += raw_bw_allot[i];
            bw_remaining_after_first_pass_ob += (dfp_isoc_bw_demand_ob[i] * dfp_allotted_isoc_bw[i]); // Add any isoc b/w allotted to the device waiting for seocnd
                                                                                                      // pass to be considered during equitable distro in 2nd pass - that way a device's isoc allocation is considered during proportionate distribution
        }
        else
        {
            allotted_bw_ob[i] += dfp_bulk_bw_demand_ob[i];
            tot_bw_allot_bulk_ob += dfp_bulk_bw_demand_ob[i];
            dfp_bw_demand_met_ob[i] = 1;
            bw_remaining_after_first_pass_ob -= dfp_bulk_bw_demand_ob[i];
        }
        //
        printf("DP %d: UP B/W: (Bulk +isoc)- %lf, Bulk demand met=%d, DN B/W: (Bulk + Isoc)- %lf, Bulk demand met = %d)\n", i, allotted_bw_ib[i], dfp_bw_demand_met_ib[i], allotted_bw_ob[i], dfp_bw_demand_met_ob[i]);
    }
    printf("B/W allotted for bulk after first pass: %lf up, %lf dn\n", tot_bw_allot_bulk_ib, tot_bw_allot_bulk_ob);
    printf("B/W remaining to allot in second pass: %lf up, %lf dn\n", bw_remaining_after_first_pass_ib, bw_remaining_after_first_pass_ob);
    printf("Raw B/W fractions remaining (%lf up, %lf dn)\n", frac_bw_remain_after_first_pass_ib, frac_bw_remain_after_first_pass_ob);
    //
    //
    printf("-----Bulk BW: Final Pass B/W allocation to DP ports-----\n");
    double bulk_bw_proportion_alloc;
    for (i = 0; i < num_dfp; i++)
    {
        if (dfp_bw_demand_met_ib[i] == 0)
        {
            bulk_bw_proportion_alloc = (raw_bw_allot[i] * bw_remaining_after_first_pass_ib / frac_bw_remain_after_first_pass_ib) - (dfp_allotted_isoc_bw[i] * dfp_isoc_bw_demand_ib[i]);
            // Subtract any isoc b/w allotted to the device from its proportionate allocation
            tot_bw_allot_bulk_ib += bulk_bw_proportion_alloc;
            allotted_bw_ib[i] += bulk_bw_proportion_alloc;
        }
        if (dfp_bw_demand_met_ob[i] == 0)
        {
            bulk_bw_proportion_alloc = (raw_bw_allot[i] * bw_remaining_after_first_pass_ob / frac_bw_remain_after_first_pass_ob) - (dfp_allotted_isoc_bw[i] * dfp_isoc_bw_demand_ib[i]);
            // Subtract any isoc b/w allotted to the device from its proportionate allocation
            tot_bw_allot_bulk_ob += bulk_bw_proportion_alloc;
            allotted_bw_ob[i] += bulk_bw_proportion_alloc;
        }

        if ((dfp_width[i] * dfp_freq[i]) > 0)
        {
            dfp_util_ib[i] = allotted_bw_ib[i] / (double)(dfp_width[i] * dfp_freq[i]);
            dfp_util_ob[i] = allotted_bw_ob[i] / (double)(dfp_width[i] * dfp_freq[i]);
            if (dfp_util_ib[i] > dfp_util_ob[i])
                dfp_util_link[i] = dfp_util_ib[i];
            else
                dfp_util_link[i] = dfp_util_ob[i];
        }
        else
        { // This is the USB1 and USB2 simplification
            dfp_util_ib[i] = dfp_util_ob[i] = dfp_util_link[i] = 1;
        }

        printf("DP Port: %d, UP B/W: %lf (util: %lf), DN B/W: %lf (util: %lf), Link Util: %lf\n", i, allotted_bw_ib[i], dfp_util_ib[i], allotted_bw_ob[i], dfp_util_ob[i], dfp_util_link[i]);
    }
    ufp_util_ib = (tot_bw_allot_bulk_ib + ufp_allotted_bw_isoc_ib) / ufp_bw;
    ufp_util_ob = (tot_bw_allot_bulk_ob + ufp_allotted_bw_isoc_ob) / ufp_bw;
    if (ufp_util_ib > ufp_util_ob)
        ufp_util_link = ufp_util_ib;
    else
        ufp_util_link = ufp_util_ob;
    printf("Aggr B/w Allotted: Bulk: (%lf ib, %lf ob), Isoc: (%lf ib, %lf ob)\n", tot_bw_allot_bulk_ib, tot_bw_allot_bulk_ob, ufp_allotted_bw_isoc_ib, ufp_allotted_bw_isoc_ob);
    printf("Hub UP utilization: up: %lf, dn: %lf, link: %lf\n", ufp_util_ib, ufp_util_ob, ufp_util_link);
    //

    double time_tot_ideal; // this represents the time we are considering but it gets derated to
    // time_tot represents the number with ufp_latency_tol, dfp_latency_tol[i]

    printf("----------------------------------------------\n");
    printf("Power:(W)  U0: %lf, U1: %lf, U2: %lf, U3: %lf\n", USB3_POWER_U0, USB3_POWER_U1, USB3_POWER_U2, USB3_POWER_U3);
    printf("Entry, Exit times (us): U1 (%lf, %lf), U2: (%lf, %lf), U3: (%lf, %lf)\n", USB3_ENTRY_U1, USB3_EXIT_U1, USB3_ENTRY_U2, USB3_EXIT_U2, USB3_ENTRY_U3, USB3_EXIT_U3);
    printf("----------------------------------------------\n");
    time_mult = 1;
    for (j = 0; j < 10; j++)
    {
        // Use exactly one of these three for the scheduling interval
        // --------------Mechanism #1 for scheduling interval below (next line)
        // time_tot_ideal = (double)(j+1)*125.0;  // use this for linear increase in scheduling interval every iteration
        //
        // --------------Mechanism #2: Next 2 line result in 2x increase in scheduling interval for every iteration
        time_tot_ideal = (double)(time_mult)*125.0;
        time_mult *= 2;
        //
        // Mechanism #3: Use these two for two scheduling intervals: as noted below
        // if(j==0) time_tot_ideal = 4000;
        // else time_tot_ideal = 64000;
        // -------------------------------------------------------
        //
        printf("------- Power Savings with %lf us scheduling interval ---\n", time_tot_ideal);
        total_power_savings_hub_u1 = 0;
        total_power_savings_hub_u2 = 0;
        total_power_savings_hub_u3 = 0;
        total_power_savings_hub_hybrid = 0;
        //
        if ((time_tot_ideal < ufp_latency_tol) || (ufp_latency_tol == 0))
            time_tot = time_tot_ideal;
        else
            time_tot = ufp_latency_tol;

        time_u0_or_entry_exit_u1 = (ufp_util_link * time_tot) + USB3_ENTRY_U1 + USB3_EXIT_U1;
        time_u0_or_entry_exit_u2 = (ufp_util_link * time_tot) + USB3_ENTRY_U2 + USB3_EXIT_U2;
        time_u0_or_entry_exit_u3 = (ufp_util_link * time_tot) + USB3_ENTRY_U3 + USB3_EXIT_U3;
        //
        if (time_u0_or_entry_exit_u1 < time_tot)
        {
            frac_u1 = 1.0 - (time_u0_or_entry_exit_u1 / time_tot);
            power_savings_port_u1 = frac_u1 * (USB3_POWER_U0 - USB3_POWER_U1);
            total_power_savings_hub_u1 += power_savings_port_u1;
        }
        else
        {
            frac_u1 = 0;
            power_savings_port_u1 = 0;
        }
        //
        if (time_u0_or_entry_exit_u2 < time_tot)
        {
            frac_u2 = 1.0 - (time_u0_or_entry_exit_u2 / time_tot);
            power_savings_port_u2 = frac_u2 * (USB3_POWER_U0 - USB3_POWER_U2);
            total_power_savings_hub_u2 += power_savings_port_u2;
        }
        else
        {
            frac_u2 = 0;
            power_savings_port_u2 = 0;
        }
        //
        if (time_u0_or_entry_exit_u3 < time_tot)
        {
            frac_u3 = 1.0 - (time_u0_or_entry_exit_u3 / time_tot);
            power_savings_port_u3 = frac_u3 * (USB3_POWER_U0 - USB3_POWER_U3);
            total_power_savings_hub_u3 += power_savings_port_u3;
        }
        else
        {
            frac_u3 = 0;
            power_savings_port_u3 = 0;
        }
        // Hybrid mode parks the port in the highest power savings based on the calculation
        power_savings_port_hybrid = power_savings_port_u3;
        if (power_savings_port_u2 > power_savings_port_hybrid)
            power_savings_port_hybrid = power_savings_port_u2;
        if (power_savings_port_u1 > power_savings_port_hybrid)
            power_savings_port_hybrid = power_savings_port_u1;
        total_power_savings_hub_hybrid += power_savings_port_hybrid;
        //
        //
        printf("Hub UP: Util %lf: U1: (frac: %lf, Power Savings U1: %lf), U2: (frac: %lf, Power Savings U2: %lf), U3: (frac: %lf, Power Savings U3: %lf), Power Savings Hybrid: %lf\n", ufp_util_link, frac_u1, power_savings_port_u1, frac_u2, power_savings_port_u2, frac_u3, power_savings_port_u3, power_savings_port_hybrid);
        fprintf(fp2, "UFP Power Savings: U1: %lf, U2: %lf, U3: %lf, Hybrid (U1, U2, U3): %lf\n", power_savings_port_u1, power_savings_port_u2, power_savings_port_u3, power_savings_port_hybrid);
        //
        for (i = 0; i < num_dfp; i++)
        {
            if ((time_tot_ideal < dfp_latency_tol[i]) || (dfp_latency_tol[i] == 0))
                time_tot = time_tot_ideal;
            else
                time_tot = dfp_latency_tol[i];
            time_u0_or_entry_exit_u1 = (dfp_util_link[i] * time_tot) + USB3_ENTRY_U1 + USB3_EXIT_U1;
            time_u0_or_entry_exit_u2 = (dfp_util_link[i] * time_tot) + USB3_ENTRY_U2 + USB3_EXIT_U2;
            time_u0_or_entry_exit_u3 = (dfp_util_link[i] * time_tot) + USB3_ENTRY_U3 + USB3_EXIT_U3;
            frac_u1 = 1.0 - (time_u0_or_entry_exit_u1 / time_tot);
            if (frac_u1 < 0)
                frac_u1 = 0;
            frac_u2 = 1.0 - (time_u0_or_entry_exit_u2 / time_tot);
            if (frac_u2 < 0)
                frac_u2 = 0;
            frac_u3 = 1.0 - (time_u0_or_entry_exit_u3 / time_tot);
            if (frac_u3 < 0)
                frac_u3 = 0;
            //
            power_savings_port_u1 = frac_u1 * (USB3_POWER_U0 - USB3_POWER_U1);
            power_savings_port_u2 = frac_u2 * (USB3_POWER_U0 - USB3_POWER_U2);
            power_savings_port_u3 = frac_u3 * (USB3_POWER_U0 - USB3_POWER_U3);
            //
            power_savings_port_hybrid = power_savings_port_u3;
            if (power_savings_port_u2 > power_savings_port_hybrid)
                power_savings_port_hybrid = power_savings_port_u2;
            if (power_savings_port_u1 > power_savings_port_hybrid)
                power_savings_port_hybrid = power_savings_port_u1;
            //
            total_power_savings_hub_u1 += power_savings_port_u1;
            total_power_savings_hub_u2 += power_savings_port_u2;
            total_power_savings_hub_u3 += power_savings_port_u3;
            total_power_savings_hub_hybrid += power_savings_port_hybrid;
            //
            printf("Hub DP:%d:  Util %lf: U1: (frac: %lf, Power Savings U1: %lf), U2: (frac: %lf, Power Savings U2: %lf), U3: (frac: %lf, Power Savings U3: %lf), Hybrid (U1, U2, U3): %lf\n", i, dfp_util_link[i], frac_u1, power_savings_port_u1, frac_u2, power_savings_port_u2, frac_u3, power_savings_port_u3, power_savings_port_hybrid);
            //
            fprintf(fp2, "DFP %d Power Savings: U1: %lf, U2: %lf, U3: %lf, Hybrid (U1, U2, U3): %lf\n", i, power_savings_port_u1, power_savings_port_u2, power_savings_port_u3, power_savings_port_hybrid);
        }
        printf("Total HUB power savings: U1: %lf, U2: %lf, U3: %lf, Hybrid (U1, U2, U3): %lf\n", total_power_savings_hub_u1, total_power_savings_hub_u2, total_power_savings_hub_u3, total_power_savings_hub_hybrid);
        fprintf(fp2, "Total HUB power savings: U1: %lf, U2: %lf, U3: %lf, Hybrid (U1, U2, U3): %lf\n", total_power_savings_hub_u1, total_power_savings_hub_u2, total_power_savings_hub_u3, total_power_savings_hub_hybrid);
    }
    fclose(fp2);
    printf("-------------\n");
}