#include <stdio.h>
#include <math.h>
#include "hub_config.h"
main()
{
 int ufp_width, ufp_freq; //UFP width and freq (note 5 GT/s changed to 4 GT/s)
 int num_dfp; // number of downstream ports
 int dfp_usb_type[15];// Type of USB:1, 2, 3, 4
 int dfp_width[15], dfp_freq[15]; // Only meaningful for USB3 and above; else 0
 double dfp_bw_demand_ib[15], dfp_bw_demand_ob[15];
                // Projected B/W demand in each direction for each DFP
 double net_ufp_demand=0, net_dn_demand=0; // represents projected b/w demand
                   // per direction from all the DPs (util * raw b/w)
 double ufp_bw;  //total b/w available per direction in UP
 double dfp_raw_bw_aggr; // Raw B/W total of all DPs in either direction
 double raw_bw_allot[15]; // Raw B/W allottment if each DP requested full B/W
 double allotted_bw_ib[15],allotted_bw_ob[15];
 // Actual b/w allocated to each DP by the host/hub depending on their
 // demand and their fair share of b/w allocation
 //

 //
 // The following are used during the b/w allocation
 //
 double tot_bw_allot_ib=0,tot_bw_allot_ob=0; // Used during b/w allocation to track
                         //how much bw of the UFP has been allotted so far in each direction
 short int dfp_bw_demand_met_ib[15],dfp_bw_demand_met_ob[15];
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
 double frac_bw_remain_after_first_pass_ib=0;
 double frac_bw_remain_after_first_pass_ob=0;
 //
 // Actual UFP b/w remaining in each direction after the first pass
 double bw_remaining_after_first_pass_ib;
 double bw_remaining_after_first_pass_ob;
 //
 // Actual utilization after the 2-pass b/w allocation
 double dfp_util_ib[15],dfp_util_ob[15],dfp_util_link[15];
 double ufp_util_ib, ufp_util_ob, ufp_util_link;
 //
 double time_u0_or_entry_exit_u1,time_u0_or_entry_exit_u2,time_u0_or_entry_exit_u3;
 // The above represents the time the Port is in U0 or in entry/exit to a low-power state, as
 // the entry into or exit from a low-power state Ux is considered to consume the same power as
 // U0
 double time_tot; // total time per scheduling interval

 double total_power_savings_hub_u1, frac_u1; // Power savings and fractional time with U1
 double total_power_savings_hub_u2, frac_u2; // Power savings and fractional time with U2
 double total_power_savings_hub_u3, frac_u3; //// Power savings and fractional time with U3
 double power_savings_port_u1, power_savings_port_u2, power_savings_port_u3;
                  // partial per-port calculation towards the above

 int i,j;
 double raw_bw_allot_tot=0;
 FILE *fp1,*fp2;

 printf("-----------------------------------\n");
 printf("Enter the Hub configuration in 'hub_config.in' file\n");
 printf("Expected format: Upstream Port (UP) width, frequency for USB3+ (expected to have USB2) \n \t followed by no of downstream ports followed by for DP the USB Type, width, frequency, \n\t Upstream B/W demand, Downstream B/W demand (example below)\n hub_config.in Format as follows ...\n");
 printf("2, 20 // Line 1: Means x2 at 20G for UP\n");
 printf("4 // Line 2: No of downstream Ports in Hub is 4 - this means 4 more lines as follows\n");
 printf("3 1 5 2.0 0.4: Downstream Port 0 is USB 3; 1 wide; 5 GT/s, \n\tUpstream B/W demand 2.0 Gb/s, and Downstream B/W demand of 0.4 Gb/s\n");
 printf("2 0 0 0.5 0.1: Downstream Port 1 is USB 2; next two numbers are dont care, \n\t Upstream utilization of 0.5, and DOwnstream Utilization of 0.1\n");
 printf("3 2 10 10.0 18.0: Downstream Port 2 is USB 3.2; 2 wide; 10 GT/s, \n\t Upstream B/W demand is 10 Gb/s, and Downstream B/W demand is 18.0 Gb/s\n");
 printf("4 2 20 20.0 36.0: Downstream Port 3 is USB 4; 2 wide; 20 GT/s, \n\t Upstream B/W demand is 20.0 Gb/s, and Downstream B/W demand is 36.0 Gb/s\n");
 printf("-----------------------------------\n");
 //
 // Read from input file and print out
 //
 fp1=fopen("hub_config.in","r");
 fscanf(fp1, "%d %d", &ufp_width,&ufp_freq);
 if(ufp_freq == 5) ufp_freq =4;
 ufp_bw=(double)ufp_freq * (double)ufp_width;
 fscanf(fp1, "%d", &num_dfp);
 printf("USB Hub Specification considered:\n");
 printf("UP: x%d, %dGT/s, %d Downstream Ports\n",ufp_width,ufp_freq,num_dfp);
 dfp_raw_bw_aggr = 0;
 for(i=0;i<num_dfp;i++){
   fscanf(fp1,"%d %d %d %lf %lf",&dfp_usb_type[i],&dfp_width[i],&dfp_freq[i],&dfp_bw_demand_ib[i],&dfp_bw_demand_ob[i]);
   if(dfp_usb_type[i]<3){
     dfp_width[i]=0;
     dfp_freq[i]=0;
   }
   if(dfp_freq[i]==5) dfp_freq[i]=4;
   dfp_raw_bw_aggr += (double)(dfp_width[i]*dfp_freq[i]);
   printf("DP %d: USB %d, width: %d, %d GT/s, Util (%lf up, %lf dn)\n",i,dfp_usb_type[i],dfp_width[i],dfp_freq[i],dfp_bw_demand_ib[i],dfp_bw_demand_ob[i]);
   net_ufp_demand+= dfp_bw_demand_ib[i];
   net_dn_demand+= dfp_bw_demand_ob[i];
   //printf("i=%d, up = %lf, dn = %lf\n",i,net_ufp_demand,net_dn_demand);
 }
 fclose(fp1);
 printf("Net UP B/W (%lf up, %lf dn) vs net demand by DP ports (%lf up, %lf dn) vs raw b/w demand in DP %lf in each direction\n",ufp_bw,ufp_bw,net_ufp_demand,net_dn_demand,dfp_raw_bw_aggr);

 printf("----Raw B/W Allocation to DP based on width/ frequency -------\n");
 for(i=0;i<num_dfp;i++){
   raw_bw_allot[i]=(double)(dfp_width[i]*dfp_freq[i])/dfp_raw_bw_aggr;
   printf("DP Port %d: Raw Util: %lf\n",i,raw_bw_allot[i]);
   raw_bw_allot_tot+=raw_bw_allot[i];
 }
 printf("Total Raw B/W allot fraction: %lf\n",raw_bw_allot_tot);
 printf("---------------------\n");

 //
 // Now the actual b/w allocation to each DP in each direction
 // based on the actual b/w demand and the fair b/w allocation
 // (lower of the two). This happens in two phases. In the first
 // phase we do the lower of the two and then any remaining b/w
 // from the available UP b/w is redistributed proportionately among
 // those DP ports whose b/w demand is more than the fair allocation
 //
 //
 printf("-----First Pass B/W allocation to DP ports-----\n");
 for(i=0;i<num_dfp;i++){
   if((raw_bw_allot[i]*ufp_bw) < dfp_bw_demand_ib[i]){
     allotted_bw_ib[i]=0;
     dfp_bw_demand_met_ib[i]=0;
     frac_bw_remain_after_first_pass_ib += raw_bw_allot[i];
   }
   else{
     allotted_bw_ib[i]=dfp_bw_demand_ib[i];
     dfp_bw_demand_met_ib[i]=1;
   }
   //
   if((raw_bw_allot[i]*ufp_bw) < dfp_bw_demand_ob[i]){
     allotted_bw_ob[i]=0;
     dfp_bw_demand_met_ob[i]=0;
     frac_bw_remain_after_first_pass_ob += raw_bw_allot[i];
   }
   else{
     allotted_bw_ob[i]=dfp_bw_demand_ob[i];
     dfp_bw_demand_met_ob[i]=1;
   }
   tot_bw_allot_ib+=allotted_bw_ib[i];
   tot_bw_allot_ob+=allotted_bw_ob[i];
   //
   printf("DP %d: UP B/W: %lf (demand met=%d), DN B/W: %lf (demand met = %d)\n",i,allotted_bw_ib[i],dfp_bw_demand_met_ib[i],allotted_bw_ob[i],dfp_bw_demand_met_ob[i]);
 }
 bw_remaining_after_first_pass_ib = ufp_bw - tot_bw_allot_ib;
 bw_remaining_after_first_pass_ob = ufp_bw - tot_bw_allot_ob;
 printf("B/W allotted after first pass: %lf up, %lf dn\n",tot_bw_allot_ib,tot_bw_allot_ob);
 printf("B/W remaining to allot in second pass: %lf up, %lf dn\n",bw_remaining_after_first_pass_ib,bw_remaining_after_first_pass_ob);
 printf("Raw B/W fractions remaining (%lf up, %lf dn)\n",frac_bw_remain_after_first_pass_ib,frac_bw_remain_after_first_pass_ob);
 //
 //
 printf("-----Final Pass B/W allocation to DP ports-----\n");
 for(i=0;i<num_dfp;i++){
   if(dfp_bw_demand_met_ib[i]==0){
     allotted_bw_ib[i]=raw_bw_allot[i]*bw_remaining_after_first_pass_ib/frac_bw_remain_after_first_pass_ib;
     tot_bw_allot_ib+=allotted_bw_ib[i];
   }
   if(dfp_bw_demand_met_ob[i]==0){
     allotted_bw_ob[i]=raw_bw_allot[i]*bw_remaining_after_first_pass_ob/frac_bw_remain_after_first_pass_ob;
     tot_bw_allot_ob+=allotted_bw_ob[i];
   }

   if((dfp_width[i]*dfp_freq[i])>0){
     dfp_util_ib[i]=allotted_bw_ib[i]/(double)(dfp_width[i]*dfp_freq[i]);
     dfp_util_ob[i]=allotted_bw_ob[i]/(double)(dfp_width[i]*dfp_freq[i]);
     if(dfp_util_ib[i]>dfp_util_ob[i]) dfp_util_link[i]=dfp_util_ib[i];
     else dfp_util_link[i]=dfp_util_ob[i];
   }
   else{ // This is the USB1 and USB2 simplification
     dfp_util_ib[i]=dfp_util_ob[i]=dfp_util_link[i]=1;
   }

   printf("DP Port: %d, UP B/W: %lf (util: %lf), DN B/W: %lf (util: %lf), Link Util: %lf\n",i,allotted_bw_ib[i],dfp_util_ib[i],allotted_bw_ob[i], dfp_util_ob[i],dfp_util_link[i]);
 }
 ufp_util_ib = tot_bw_allot_ib/ufp_bw;
 ufp_util_ob = tot_bw_allot_ob/ufp_bw;
 if(ufp_util_ib > ufp_util_ob) ufp_util_link=ufp_util_ib;
 else ufp_util_link=ufp_util_ob;
 printf("Aggr B/w Allotted: (%lf up, %lf dn)\n",tot_bw_allot_ib,tot_bw_allot_ob);
 printf("Hub UP utilization: up: %lf, dn: %lf, link: %lf\n",ufp_util_ib,ufp_util_ob,ufp_util_link);
 //

 printf("----------------------------------------------\n");
 printf("Power Savings with U2 (Entry L1: %lf us, Exit L1: %lf us\n",USB3_ENTRY_U2,USB3_EXIT_U2);
 printf("----------------------------------------------\n");
 for(j=0;j<10;j++){
   time_tot = (double)(j+1)*125.0;
   //
   printf("------- Power Savings with %lf us scheduling interval ---\n",time_tot);
   total_power_savings_hub_u1 = 0;
   total_power_savings_hub_u2 = 0;
   total_power_savings_hub_u3 = 0;
   //
   time_u0_or_entry_exit_u1=(ufp_util_link*time_tot)+USB3_ENTRY_U1+USB3_EXIT_U1;
   time_u0_or_entry_exit_u2=(ufp_util_link*time_tot)+USB3_ENTRY_U2+USB3_EXIT_U2;
   time_u0_or_entry_exit_u3=(ufp_util_link*time_tot)+USB3_ENTRY_U3+USB3_EXIT_U3;
   //
   if(time_u0_or_entry_exit_u1 < time_tot){
     frac_u1 = 1.0-(time_u0_or_entry_exit_u1/time_tot);
     power_savings_port_u1 = frac_u1*(USB3_POWER_U0-USB3_POWER_U1);
     total_power_savings_hub_u1 += power_savings_port_u1;
   }
   else{
     frac_u1=0;
     power_savings_port_u1=0;
   }
   //
   if(time_u0_or_entry_exit_u2 < time_tot){
     frac_u2 = 1.0-(time_u0_or_entry_exit_u2/time_tot);
     power_savings_port_u2 = frac_u2*(USB3_POWER_U0-USB3_POWER_U2);
     total_power_savings_hub_u2 += power_savings_port_u2;
   }
   else{
     frac_u2=0;
     power_savings_port_u2=0;
   }
   //
   if(time_u0_or_entry_exit_u3 < time_tot){
     frac_u3 = 1.0-(time_u0_or_entry_exit_u3/time_tot);
     power_savings_port_u3 = frac_u3*(USB3_POWER_U0-USB3_POWER_U3);
     total_power_savings_hub_u3 += power_savings_port_u3;
   }
   else{
     frac_u3=0;
     power_savings_port_u3=0;
   }
   //
   printf("Hub UP: Util %lf: U1: (frac: %lf, Power Savings U1: %lf), U2: (frac: %lf, Power Savings U2: %lf), U3: (frac: %lf, Power Savings U3: %lf)\n",ufp_util_link,frac_u1,power_savings_port_u1,frac_u2,power_savings_port_u2,frac_u3,power_savings_port_u3);
   //
   for(i=0;i<num_dfp;i++){
     time_u0_or_entry_exit_u1=(dfp_util_link[i]*time_tot)+USB3_ENTRY_U1+USB3_EXIT_U1;
     time_u0_or_entry_exit_u2=(dfp_util_link[i]*time_tot)+USB3_ENTRY_U2+USB3_EXIT_U2;
     time_u0_or_entry_exit_u3=(dfp_util_link[i]*time_tot)+USB3_ENTRY_U3+USB3_EXIT_U3;
     frac_u1 = 1.0-(time_u0_or_entry_exit_u1/time_tot);
     if(frac_u1<0) frac_u1=0;
     frac_u2 = 1.0-(time_u0_or_entry_exit_u2/time_tot);
     if(frac_u2<0) frac_u2=0;
     frac_u3 = 1.0-(time_u0_or_entry_exit_u3/time_tot);
     if(frac_u3<0) frac_u3=0;
     //
     power_savings_port_u1 = frac_u1*(USB3_POWER_U0-USB3_POWER_U1);
     power_savings_port_u2 = frac_u2*(USB3_POWER_U0-USB3_POWER_U2);
     power_savings_port_u3 = frac_u3*(USB3_POWER_U0-USB3_POWER_U3);
     //
     total_power_savings_hub_u1 += power_savings_port_u1;
     total_power_savings_hub_u2 += power_savings_port_u2;
     total_power_savings_hub_u3 += power_savings_port_u3;
     //
     printf("Hub DP:%d:  Util %lf: U1: (frac: %lf, Power Savings U1: %lf), U2: (frac: %lf, Power Savings U2: %lf), U3: (frac: %lf, Power Savings U3: %lf),\n",i,dfp_util_link[i],frac_u1,power_savings_port_u1,frac_u2,power_savings_port_u2,frac_u3,power_savings_port_u3);
   }
   printf("Total HUB power savings: U1: %lf, U2: %lf, U3: %lf\n",total_power_savings_hub_u1,total_power_savings_hub_u2,total_power_savings_hub_u3);
 }

 printf("-------------\n");

}
