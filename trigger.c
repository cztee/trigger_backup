/**
 * $Id: trigger.c peter.ferjancic 2014/11/17 $
 *
 * @brief Red Pitaya triggered acquisition of multiple traces
 *
 * (c) Red Pitaya  http://www.redpitaya.com
 *
 * This part of code is written in C programming language.
 * Please visit http://en.wikipedia.org/wiki/C_(programming_language)
 * for more details on the language used herein.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <stddef.h>
#include <sys/param.h>
#include "fpga_osc.h"
#include <time.h>

#define BILLION 1000000000L

//Buffer depth 
const int BUF = 16*1024;
const int N = 80; 			// desired length of trace (1,..., 16383)
const int decimation = 1; 	// decimation: [1;8;64;1024;8192;65536]

int main(int argc, char *argv[]) 
{

	if (argc !=4){
	    printf("Missing arguments! Please check that you have 3 arguments: Number of events, Trigger Voltage, and Acquisition duration.\n");
        }
	
	if (argv[1] < 0 ){
	    printf("Invalid number of events!\n");
            return 1;
	}
	if (argv[3]<0){
	    printf("Invalid duration!\n");
	    return 1;
	} 
	// initialization
	int start = osc_fpga_init(); 
	if(start) {
	    printf("osc_fpga_init didn't work, retval = %d",start);
	    return -1;
	}


	time_t timer;
	char buffer[26];
	struct tm* tm_info;
	timer = time(NULL);
	tm_info = localtime(&timer);
	strftime(buffer,26,"%Y%m%d%H%M%S", tm_info);
	FILE * fp;
	fp = fopen (strcat(buffer,".txt"), "w+"); // open the file output is stored
	int * cha_signal; 
	int * chb_signal;

	// set acquisition parameters
	osc_fpga_set_trigger_delay(N);
	g_osc_fpga_reg_mem->data_dec = decimation;

	// put sampling engine into reset
	osc_fpga_reset();

	// define initial parameters
	int trace_counts = atoi(argv[1]); 		// counts how many traces were sampled in for loop
	int trig_ptr; 			// trigger pointer shows memory adress where trigger was met
	int trig_test;			// trigger test checks if writing the trace has completed yet
	float trigger_voltage= atof(argv[2]); // enter trigger voltage in [V] as parameter [1V...~600 RP units]
	g_osc_fpga_reg_mem->cha_thr = osc_fpga_cnv_v_to_cnt(trigger_voltage); //sets trigger voltage
	//time check
	time_t endwait;    
	time_t start_time = time(NULL);
        time_t seconds = (long int)atoi(argv[3]);

	endwait = start_time + seconds;
	//printf("Endwait:%ld, Start Time: %ld\n",endwait, start_time);
	//printf("No of events:%d, Trig level:%f, Duration:%ld \n", trace_counts,trigger_voltage,seconds);
	/***************************/
	/** MAIN ACQUISITION LOOP **/
	/***************************/
	
	for (int trace_count=0; trace_count<trace_counts; trace_count++)
	{	

		// calculate how many microsecond elapsed per event 
		uint64_t diff;
		struct timespec start, end;

		/* measure monotonic time */
		clock_gettime(CLOCK_MONOTONIC, &start);	/* mark start time */
		/*Set trigger, begin acquisition when condition is met*/
		osc_fpga_arm_trigger(); //start acquiring, incrementing write pointer
		osc_fpga_set_trigger(0x3); // where do you want your triggering from?
	 	/*    0 - end of acquisition/no acquisition
	 	*     1 - trig immediately
     	*     2 - ChA positive edge 
     	*     3 - ChA negative edge
     	*     4 - ChB positive edge 
     	*     5 - ChB negative edge
     	*     6 - External trigger 0
     	*     7 - External trigger 1*/
	 	// Trigger always changes to 0 after acquisition is completed, write pointer stops incrementing
	 	//->fpga.osc.h l66

	//printf("Checkpoint A\n");
	    /*Wait for the acquisition to finish = trigger is set to 0*/
     	trig_test=(g_osc_fpga_reg_mem->trig_source); // it gets the above trigger value
	//printf("Checkpoint B\n");
	//printf("%d\n",trig_test);
     	// if acquistion is not yet completed it should return the number you set above and 0 otherwise
     	while (trig_test!=0){ // with this loop the program waits until the acquistion is completed before cont.
     		trig_test=(g_osc_fpga_reg_mem->trig_source);
     	}
     	//->fpga_osc.c l366

	//printf("Checkpoint C\n");
	    trig_ptr = g_osc_fpga_reg_mem->wr_ptr_trigger; // get pointer to mem. adress where trigger was met
	    //->fpga_osc.c l283
	    osc_fpga_get_sig_ptr(&cha_signal, &chb_signal); 
	    //printf("Checkpoint 1\n");
 	    //->fpga_osc.c l378
            //now read N samples from the trigger pointer location //
	    int i;
            int ptr;
	    for (i=-10; i < N; i++) {
	        ptr = (trig_ptr+i)%BUF;
        	if (cha_signal[ptr]>=8192) // properly display negative values fix
        	{
        		//printf("%d ",cha_signal[ptr]-16384);
        		fprintf(fp, "%d ", cha_signal[ptr]-16384);
        	}
        	else
        	{
        	//	printf("%d ",cha_signal[ptr]);
        		fprintf(fp, "%d ", cha_signal[ptr]);
        	}
            }
	    clock_gettime(CLOCK_MONOTONIC, &end);
	    fprintf(fp, " ");
	    //char str[ENOUGH];
	    //sprintf(str, "%ld", time(NULL)-start_time);
	    //printf("Almost!\n");
	    fprintf(fp,"%d ", -9999); // this separates the data from timestamp
	    diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
	    fprintf(fp,"%llu ",(long long unsigned int) diff);
	    fprintf(fp, "\n");
	   //printf("Checkpoint 2\n"); 
	   if(endwait <time(NULL)){break;}
	}

	// cleaning up all nice like mommy taught me
	fclose(fp);
    osc_fpga_exit();
	return 0;
}

