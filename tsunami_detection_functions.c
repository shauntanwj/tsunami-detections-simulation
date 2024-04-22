/* This file contains some struct and functions that is used in the tsunami_detection_simulation.c file. */

/* A struct to store the info for the simulated sea water column height readings that 
    is used in the Satellite Altimeter thread that was spawned by the base station */
struct satellite_alt
{
    int year;
    int month;
    int day;
    int hour;
    int min;
    int sec;
    double simulated_height;
};


/* A struct to store sensor nodes info like SMA, sensor node's communicator and sensor node's rank 
   This struct is use for the sensor node listener thread so that the thread can listen to request
   from neigbouring nodes and send the SMA to the node requested it using the sensor node's 
   communicator */
struct sensor_info
{
    float sma;
    MPI_Comm threadcomm;
    int cart_rank;
};


/* A struct to store the sensor node's information then it is later used to send it to the base station */
struct sensor_to_base
{     
    int rep_node_rank; // reporting node rank
    
    float rep_node_sma; // reporting node SMA

    int adj_rank[4]; // int array for adj node ranks

    float adj_sma[4]; // float array for adj node SMA

    float comm_time; // communication time
    
    int num_adj_matches; // number of adj matches to reporting node/alert in sensor node
    
    int alert_date_time[6]; // alert date time
    
    int adj_x[4];   // adj node x coordinates
    
    int adj_y[4];   // adj node y coordinates
    
    int total_report;   // total number of report send by the sensor node
    
    int rep_node_coord[2];  // reporting node coordinates
};

/* This function creates a custome datatype for the sensor node and base station so that
   this sensor node can send its information to the base station using this custom datatype
   through the world communicator */
MPI_Datatype create_custom_datatype(){

    struct sensor_to_base custom_value;
   
    /* Creating report_value structure to send report to base */
    MPI_Datatype SensorBaseValuetype; // enumerator, specify the type of the enumerator, the new data type. 
	MPI_Datatype type[11] = { MPI_INT, MPI_FLOAT, MPI_INT, MPI_FLOAT, MPI_FLOAT, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT};
    int blocklen[11] = {1, 1, 4, 4, 1, 1, 6, 4, 4, 1, 2};
    MPI_Aint disp[11];
    
    /* Get the address of each variable */
    MPI_Get_address(&custom_value.rep_node_rank, &disp[0]);   
	MPI_Get_address(&custom_value.rep_node_sma, &disp[1]);
	MPI_Get_address(&custom_value.adj_rank, &disp[2]);   
	MPI_Get_address(&custom_value.adj_sma, &disp[3]);
	MPI_Get_address(&custom_value.comm_time, &disp[4]);   
	MPI_Get_address(&custom_value.num_adj_matches, &disp[5]); 
	MPI_Get_address(&custom_value.alert_date_time, &disp[6]); 
	MPI_Get_address(&custom_value.adj_x, &disp[7]);     
	MPI_Get_address(&custom_value.adj_y, &disp[8]);         
	MPI_Get_address(&custom_value.total_report, &disp[9]);  
	MPI_Get_address(&custom_value.rep_node_coord, &disp[10]);  
	
	/* Make relative, calculate the offset */
	disp[10] = disp[10] - disp[0];
	disp[9] = disp[9] - disp[0];
	disp[8] = disp[8] - disp[0];
	disp[7] = disp[7] - disp[0];
	disp[6] = disp[6] - disp[0];
	disp[5] = disp[5] - disp[0];
	disp[4] = disp[4] - disp[0];
	disp[3] = disp[3] - disp[0];
	disp[2] = disp[2] - disp[0];
	disp[1]=disp[1] - disp[0];
	disp[0]=0;
	
	/* Create MPI struct, create custom data type */
	MPI_Type_create_struct(11, blocklen, disp, type, &SensorBaseValuetype); // create a custom datatype to ValueType
	MPI_Type_commit(&SensorBaseValuetype);
	
	return SensorBaseValuetype;

}

/* Reference: https://stackoverflow.com/questions/5289613/generate-random-float-between-two-floats/5289624 */
/* This function is used to create a random float value that is within the 'a' and 'b' */
float randomFloatExceed(float a, float b){
    srand((unsigned int)time(NULL));
	float random = ((float) rand()) / (float) RAND_MAX;
	float diff = b - a;
	float r = random * diff;
	return a + r;
}