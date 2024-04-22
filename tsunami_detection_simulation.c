/*
Tsunami Detection in a Distributed Wireless Sensor Network (WSN)
*/

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include "functions.c"

#define SHIFT_ROW 0
#define SHIFT_COL 1
#define DISP 1
#define REQ 2
#define NO_REQ 3
#define SMA_SEND 4
#define SEND_BASE 5
#define TERMINATE 6

int base_station(MPI_Comm world_comm, MPI_Comm slave_comm, int iteration_run, int threshold);
int sensor_node(MPI_Comm world_comm, MPI_Comm slave_comm, int dims[], int threshold);
void* SensorListener(void *pArg);
void* SatelliteAltimeter(void *pArg);
struct satellite_alt satellite_array[15];
pthread_mutex_t g_Mutex = PTHREAD_MUTEX_INITIALIZER;
int terminate_code = 0;

int main( int argc, char **argv )
{
    int rank, size, nrows, ncols;
    int dims[2];
    int secs_to_run, threshold;
    MPI_Comm new_comm;
    
	/* Start up initial MPI environment */
	int provided;
    MPI_Init_thread( &argc, &argv, MPI_THREAD_SERIALIZED,&provided);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );

    /* Check if the user define the grid dimension, total run time, predefined threshold or not */
    if(argc == 5){
        nrows = atoi (argv[1]);
		ncols = atoi (argv[2]);
		dims[0] = nrows; //number of rows 
		dims[1] = ncols; // number of columns 
		if( (nrows*ncols) != size-1) {
			if( rank ==0) printf("ERROR: nrows*ncols)=%d * %d = %d != %d\n", nrows, ncols, nrows*ncols,size-1);
			MPI_Finalize(); 
			return 0;
		}
		secs_to_run = atoi (argv[3]);   // time specified by users to execute the code
		threshold = atoi (argv[4]);     // predefined sea water column height threshold value
    }  

    /* If the users does not define the grid dimension, total run time, predefined threshold */  
	else {
		nrows=ncols=(int)sqrt(size-1);
		dims[0]=dims[1]=0;
		secs_to_run = 25;
		threshold = 6000;
	}

    /* Split the communicator */
    MPI_Comm_split( MPI_COMM_WORLD, rank == size-1, 0, &new_comm );
	
    /* Rank size-1 would be the base station, others would be the sensor nodes */
    if (rank == size - 1) 
	    base_station( MPI_COMM_WORLD, new_comm, secs_to_run, threshold);
    else
	    sensor_node( MPI_COMM_WORLD, new_comm, dims, threshold);
    
    MPI_Finalize( );
    return 0;
}

/*Base Station*/
int base_station(MPI_Comm world_comm, MPI_Comm slave_comm, int iteration_run, int threshold){
    printf("Total Seconds of Execution: %d\n", iteration_run);
    printf("Sea Water Column Height Threshold Value: %d\n", threshold);
    MPI_Status status;
    int no_rep_recv = 0, exit_data = 0, my_rank, size, match, counter = 0, false_total = 0, true_total = 0;
    MPI_Datatype REPORT_TYPE = create_custom_datatype(); // Create custom data type 
    struct sensor_to_base sensor_val;
    FILE *logFile; 
    
    MPI_Comm_size(world_comm, &size); // Size of the world communicator
    MPI_Comm_rank(world_comm, &my_rank); // Rank of base station

   /* Create a thread for the satellite */
    pthread_t tid;
    pthread_mutex_init(&g_Mutex, NULL); // Initialize the Mutex
    pthread_create(&tid, 0, SatelliteAltimeter, &threshold); 
    
    /*************************************************************/
	/* create log files */
	/*************************************************************/
    fclose(fopen("base_log.txt", "w"));
    time_t curr_time = time(NULL);
    struct tm curr_t = *localtime(&curr_time);
    time_t t = curr_time + iteration_run; // Current time + defined time
    struct tm tm = *localtime(&t);
    
    printf("Current Date and Time: %d-%02d-%02d %02d:%02d:%02d \n", curr_t.tm_year + 1900, curr_t.tm_mon + 1, curr_t.tm_mday, curr_t.tm_hour, curr_t.tm_min, curr_t.tm_sec);
    printf("Termination Date and Time: %d-%02d-%02d %02d:%02d:%02d \n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    printf("-----------------------------------------------------------------------------------\n");
    /* Loop until the current time reaches the termination time */
    while(time(NULL) < t){
        MPI_Iprobe(MPI_ANY_SOURCE, SEND_BASE, world_comm, &no_rep_recv, &status); // Listens for request from the sensor nodes to send the report info.
        if(no_rep_recv != 0){ 
        
             /* Receive the report info from the sensor nodes */
             MPI_Recv(&sensor_val, 1, REPORT_TYPE, MPI_ANY_SOURCE, SEND_BASE, world_comm, &status);
             time_t recv_t = time(NULL);
             struct tm recv_tm = *localtime(&recv_t);
             printf("Base Station received report from Rank %d at %d-%02d-%02d %02d:%02d:%02d \n", sensor_val.rep_node_rank, recv_tm.tm_year + 1900, recv_tm.tm_mon + 1, recv_tm.tm_mday, 
                    recv_tm.tm_hour, recv_tm.tm_min,  recv_tm.tm_sec );

            /* Output the info to log file */
            logFile = fopen("base_log.txt", "a"); 
            time_t t = time(NULL);
            struct tm base = *localtime(&t);
            
            /* Loop this until termination msg is sent */ 
            counter++;
            fprintf(logFile, "-----------------------------------------------------------------------------------\n");
            fprintf(logFile, "Iteration: %d\n", counter);
            
            /* Logged Time */
            fprintf(logFile, "Logged time: \t%d-%02d-%02d %02d:%02d:%02d\n", base.tm_year + 1900, base.tm_mon + 1, base.tm_mday, base.tm_hour, base.tm_min, base.tm_sec);
            
            /* Alert Reported Time */
            fprintf(logFile, "Alert Reported Time:\t%d-%02d-%02d %02d:%02d:%02d\n", sensor_val.alert_date_time[0], sensor_val.alert_date_time[1], sensor_val.alert_date_time[2], sensor_val.alert_date_time[3], sensor_val.alert_date_time[4], sensor_val.alert_date_time[5]);
            
            /* Reporting Node info */
            fprintf(logFile, "\nReporting Node\t\tCoord\t\t\tHeight(m)\n");
            fprintf(logFile, "%d\t\t\t\t\t(%d,%d)\t\t\t%.3f\n", sensor_val.rep_node_rank, sensor_val.rep_node_coord[0], sensor_val.rep_node_coord[1], sensor_val.rep_node_sma); // coord (0,0) for testing. Later change to real coord when sent alr.

            /* Adjacent Nodes info */
            fprintf(logFile, "\nAdjacent Nodes\t\tCoord\t\t\tHeight(m)\n");
            for (int i = 0; i < 4; i++){
                fprintf(logFile, "%d\t\t\t\t\t(%d,%d)\t\t\t%.3f\n", sensor_val.adj_rank[i], sensor_val.adj_x[i], sensor_val.adj_y[i], sensor_val.adj_sma[i]);
            }
                        
            /* This for loop is used to access the shared global array. If the time difference is lesser than 2s, it will check if the height difference 
               is within the predefined threshold or not. If it is, then it will log a match to the log file. Else, it will log a mismatch.  */
             for (int i = 0; i < 15; i++){
                pthread_mutex_lock(&g_Mutex); // Mutex Lock
                if (abs(satellite_array[i].sec - sensor_val.alert_date_time[5]) < 2){
                    fprintf(logFile, "\nSatellite altimeter reporting time: %d-%02d-%02d %02d:%02d:%02d\n", satellite_array[i].year, satellite_array[i].month, satellite_array[i].day, satellite_array[i].hour, satellite_array[i].min, satellite_array[i].sec);
                    fprintf(logFile, "Satellite altimeter reporting height (m): %.3f\n", satellite_array[i].simulated_height);
                    
                    if (abs(satellite_array[i].simulated_height - sensor_val.rep_node_sma) < 500){
                        match = 1;
                        fprintf(logFile, "Alert Type: %d\n", match);
                        true_total++;
                        }
                    else {
                        match = 0;
                        fprintf(logFile, "Alert Type: %d\n", match);
                        false_total++;
                    }
                }
                pthread_mutex_unlock(&g_Mutex); // Mutex Unlock
             }
            
            fprintf(logFile, "\nTotal Messages send between reporting node and base station: %d\n", sensor_val.total_report);
            fprintf(logFile, "Number of adjacent matches to reporting node: %d\n", sensor_val.num_adj_matches);
            fprintf(logFile, "Max. tolerance range between nodes rewadings (m): 500\n");
            fprintf(logFile, "Max. tolerance range between satellite altimeter and reporting node readings (m): 500\n");
            fprintf(logFile, "-----------------------------------------------------------------------------------\n");
            fclose(logFile);
        }
        no_rep_recv = 0;
    }  

    /* Send the termination message to the sensor nodes */
    for(int i = 0; i < size-1; i++){
        MPI_Send(&exit_data, 1, MPI_INT, i, TERMINATE, world_comm);
    }
    printf("Termination message sent to all sensor nodes\n");
    
    terminate_code = 1; // set the terminate code to 1
    pthread_join(tid, NULL); // thread joins
    pthread_mutex_destroy(&g_Mutex); // Destroy Mutex
    logFile = fopen("base_log.txt", "a"); // This will create and open the file

    /* Summary report for the log file */
    fprintf(logFile, "\n-----------------------------Summary Report-------------------------------------------\n");
    fprintf(logFile, "Total Number of Reports: %d\n", counter);
    fprintf(logFile, "Total Number of Alerts: %d\n", true_total);
    fprintf(logFile, "Total Number of False Alerts: %d\n", false_total);
    fprintf(logFile, "----------------------------- End of Summary Report-----------------------------------\n");
    fclose(logFile);

    return 0;
}

/* Sensor Node */
int sensor_node(MPI_Comm world_comm, MPI_Comm slave_comm, int dims[], int threshold){
    MPI_Datatype REPORT_DATA = create_custom_datatype();
    struct sensor_info sensor_value;    // struct variable to save the sensor value and used in the listener thread
    struct sensor_to_base report_value; // struct variable to save the sensor value and send to Base Station
    int ndims = 2, unused = 0, report_nbr=0, slave_size, slave_rank, reorder, my_cart_rank, ierr, world_size;
    
    /* Varibles to store rank values of neighbour */
	int nbr_i_lo, nbr_i_hi;
	int nbr_j_lo, nbr_j_hi; 
    MPI_Comm comm2D;
    int coord[ndims];
    int wrap_around[ndims];

    float moving_average;   // store the SMA of the sensor node
    float nbr_sma[4];       // an array to store the neighbour's SMA
    float arr_average[3];   // store the SMA of the sensor node and every 3 random 
    memset(arr_average, 0, 3*sizeof(float));
    memset(nbr_sma, 0, 4*sizeof(float));
	
    MPI_Comm_size(world_comm, &world_size); // Size of the world communicator
    MPI_Comm_size(slave_comm, &slave_size); // Size of the slave communicator
    MPI_Comm_rank(slave_comm, &slave_rank); // Rank of the slave communicator 

    /* Create the dimension for the sensor node */
    MPI_Dims_create(slave_size, ndims, dims);

    /* Creates a virtual topology */
    wrap_around[0] = 0;
	wrap_around[1] = 0; 
	reorder = 0;
	ierr =0;
	ierr = MPI_Cart_create(slave_comm, ndims, dims, wrap_around, reorder, &comm2D);
	if(ierr != 0) printf("ERROR[%d] creating CART\n",ierr);
	sensor_value.threadcomm = comm2D;
    
    /* Get the rank of the adjacent neighbour */ 
    MPI_Cart_shift( comm2D, SHIFT_ROW, DISP, &nbr_i_lo, &nbr_i_hi );
	MPI_Cart_shift( comm2D, SHIFT_COL, DISP, &nbr_j_lo, &nbr_j_hi );
    
    /* Getting the Coordinates */
    MPI_Cart_coords(comm2D, slave_rank, ndims, coord); // convert slave_rank to a coordinate
    MPI_Cart_rank(comm2D, coord, &my_cart_rank); // convert the coordinate to the cartesian rank
    sensor_value.cart_rank = my_cart_rank; 
    
    int nbr_rank[4];    // an array to store the neighbour's rank
    int nbr_coord[2];   // an array to store the neighbour's coordinate

    /* Assigning the neighbour's rank to the array */
    nbr_rank[0] = nbr_i_lo;
    nbr_rank[1] = nbr_i_hi;
    nbr_rank[2] = nbr_j_lo;
    nbr_rank[3] = nbr_j_hi;
    
    /* Getting the coordinates of the neighbour's */
    for(int i=0;i<4;i++){
        if(nbr_rank[i] < 0){    // if the rank is negative means it's a edge-most sensor node
            report_value.adj_x[i] = -2;     
            report_value.adj_y[i] = -2;
        }
        else{
            MPI_Cart_coords(comm2D, nbr_rank[i], ndims, nbr_coord);
            report_value.adj_x[i] = nbr_coord[0];
            report_value.adj_y[i] = nbr_coord[1];  
        }
    }
    
    MPI_Status status;
    MPI_Request receive_request[4]; // request array used for MPI_Irecv below 
    
    int exit=0, exit_var;
    MPI_Status exit_status;
    MPI_Request exit_request;
    
    while(true){
        pthread_t tid;

        /*********************************************************************/
        /* This part of the code used MPI_Iprobe to see whether there's termination signal send by the Base Station 
            If there is, then the exit flag would become 1 then the code will go into the if statement 
            and break out of the loop */ 
        /*********************************************************************/
        MPI_Iprobe(MPI_ANY_SOURCE, TERMINATE, world_comm, &exit, &exit_status); 
        if(exit != 0){
            // Receive the value send by base station 
            MPI_Irecv(&exit_var, 1, MPI_INT, MPI_ANY_SOURCE, TERMINATE, world_comm, &exit_request);
            pthread_join(tid, NULL);
            printf("Rank %d receive termination message\n", slave_rank);
            break;
         }
                
        /* Generate the Random Value */    
        sleep(my_cart_rank);
        float rand_val = randomFloatExceed(5000,8000);
        
        /* Spawn a thread to listen for neighbouring request, the information of the sensor nodes
           will be pass as an argument to the thread so that the thread will be able to use 
           the sensor'd node communicator to send request to neighbouring nodes */
        pthread_create(&tid, 0, SensorListener, &sensor_value);

        /* Listen if there's SMA sent by other */
        int sma_recv=0;

        /* Received the neighbouring SMA using the MPI_Iprobe function */
        MPI_Iprobe(MPI_ANY_SOURCE, SMA_SEND, comm2D, &sma_recv, &status);    
        if (sma_recv != 0 && terminate_code == 0){
            MPI_Irecv(&nbr_sma[0], 1, MPI_FLOAT, nbr_i_lo, SMA_SEND, comm2D, &receive_request[0]);
            MPI_Irecv(&nbr_sma[1], 1, MPI_FLOAT, nbr_i_hi, SMA_SEND, comm2D, &receive_request[1]);
            MPI_Irecv(&nbr_sma[2], 1, MPI_FLOAT, nbr_j_lo, SMA_SEND, comm2D, &receive_request[2]);
            MPI_Irecv(&nbr_sma[3], 1, MPI_FLOAT, nbr_j_hi, SMA_SEND, comm2D, &receive_request[3]);
            
            sma_recv = 0;
            int alert = 0;
            report_value.rep_node_rank = my_cart_rank;
            report_value.rep_node_sma = moving_average;
            report_value.rep_node_coord[0] = coord[0]; 
            report_value.rep_node_coord[1] = coord[1]; 
            
            /* Save the adjacent node's rank to the adjacent rank array */
            report_value.adj_rank[0] = nbr_i_lo;
            report_value.adj_rank[1] = nbr_i_hi;
            report_value.adj_rank[2] = nbr_j_lo;
            report_value.adj_rank[3] = nbr_j_hi;
                
            /* Calculate the difference between the neighbouring SMA and own SMA 
                to check if it's between the predefined threshold */   
            for(int i=0; i<4;i++){
                report_value.adj_sma[i] = nbr_sma[i];
                float diff = abs(nbr_sma[i] - moving_average);
                if(diff >= 0 && diff <= 500){
                    alert++;
                }
            }
            
            report_value.num_adj_matches = alert;
            
            /* If there are at least 2 readings from the neighbouring nodes which are 
               between the predefined threshold then send report to Base Station */
            if(alert >= 2){
                printf("Rank %d send signal to Base Station\n", my_cart_rank);
                
                time_t t = time(NULL);
                struct tm tm = *localtime(&t);         
                report_value.alert_date_time[0] = tm.tm_year + 1900;
                report_value.alert_date_time[1] = tm.tm_mon+1;
                report_value.alert_date_time[2] = tm.tm_mday;
                report_value.alert_date_time[3] = tm.tm_hour;
                report_value.alert_date_time[4] = tm.tm_min;
                report_value.alert_date_time[5] = tm.tm_sec;
                                
                report_nbr++;
                report_value.total_report = report_nbr;
                MPI_Send(&report_value, 1, REPORT_DATA, world_size - 1, SEND_BASE, world_comm);
                }
        }  
        /* FIFO to insert the SMA */
        for (int i= 0; i < 3; i++){
                arr_average[i] = arr_average[i+1];
          }
        
        /* Calculate the simple moving average */ 
        arr_average[2] = rand_val; // Assign new element at last position
        moving_average = ((arr_average[0] + arr_average[1] + arr_average[2]) / 3);
        sensor_value.sma = moving_average;      

        /* If the SMA is above the defined threshold than send request to neighbouring nodes to obtain their readings */  
        if(moving_average > threshold){
            MPI_Send(&unused, 1, MPI_INT, nbr_i_lo, REQ, comm2D);
            MPI_Send(&unused, 1, MPI_INT, nbr_i_hi, REQ, comm2D);
            MPI_Send(&unused, 1, MPI_INT, nbr_j_lo, REQ, comm2D);
            MPI_Send(&unused, 1, MPI_INT, nbr_j_hi, REQ, comm2D);
        }   
       pthread_join(tid, NULL);
       sleep(2);        // generate random value every 2 seconds
    }   
    MPI_Comm_free( &comm2D );
    return 0;
   
}

/* This thread is used to listen for neighbouring nodes request */
void* SensorListener(void *pArg) 
{
    struct sensor_info info = *((struct sensor_info *)pArg);    
    float sma = info.sma; // its own SMA
    MPI_Comm comm = info.threadcomm; // the sensor node's communicator
    MPI_Status status;
    int recv_req, flag=0;   
    
    /* This MPI_Iprobe is used to listen to neighbouring request */
    MPI_Iprobe(MPI_ANY_SOURCE, REQ, comm, &flag, &status); 
  
    /* If the flag is not equal to 0 then it means that there's request send
       by neighbour to obtain it's SMA */
    if(flag != 0){
        MPI_Recv(&recv_req, 1, MPI_INT, MPI_ANY_SOURCE, REQ, comm, &status); // Receive the sent request 
        
        MPI_Send(&sma, 1, MPI_FLOAT, status.MPI_SOURCE, SMA_SEND, comm); // Send the SMA to the node that requested it
        flag = 0;   //reset the flag
    }
    return 0;
}

/* This thread is used to generate random value which exceed the predefined threshold */
void* SatelliteAltimeter(void *pArg) 
{   
    int threshold;    
    int* p = (int*)pArg;
	threshold = *p;
    int counter = 0;
    int sizeOfArray = 15; // Initialize size of array to 15

    /* Repeats until receive a termination signal from base station */
    while(terminate_code == 0){
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        float rand_val = randomFloatExceed(threshold, 8000);        
        struct satellite_alt satellite_alt_info;
        
        /* Assign the values to the struct variables */
        satellite_alt_info.year = tm.tm_year + 1900;
        satellite_alt_info.month = tm.tm_mon + 1;
        satellite_alt_info.day = tm.tm_mday;
        satellite_alt_info.hour = tm.tm_hour;
        satellite_alt_info.min = tm.tm_min;
        satellite_alt_info.sec = tm.tm_sec;
        satellite_alt_info.simulated_height = rand_val;

        /* Saves the struct variable to the global array */
        if(counter < sizeOfArray){
            pthread_mutex_lock(&g_Mutex); // Lock Mutex
            satellite_array[counter] = satellite_alt_info;  
            pthread_mutex_unlock(&g_Mutex); // Unlock Mutex
            counter++;
        }
        else{ 
            /* Used FIFO method to add new elements into the array once the array is full */
            for (int i= 0; i < sizeOfArray; i++){
                pthread_mutex_lock(&g_Mutex); // Lock Mutex
                satellite_array[i] = satellite_array[i+1]; // Shift elements at position i+1 to i
                pthread_mutex_unlock(&g_Mutex); // Unlock Mutex
            }
            pthread_mutex_lock(&g_Mutex); // Lock Mutex
            satellite_array[sizeOfArray-1] = satellite_alt_info; // Assign new element at last position
            pthread_mutex_unlock(&g_Mutex); // Unlock Mutex
        }
        sleep(1); // Executes every 1s
    }
    printf("Satellite Altimter Stop Execution\n");
    return 0;
}



