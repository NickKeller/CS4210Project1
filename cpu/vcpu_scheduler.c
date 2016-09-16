#include <stdlib.h>
#include <stdio.h>
#include <libvirt/libvirt.h>
#include <string.h>

#define DEBUG 0
#define PCPU0 (1 << 0)
#define PCPU1 (1 << 1)
#define PCPU2 (1 << 2)
#define PCPU3 (1 << 3)
#define PCPU4 (1 << 4)
#define PCPU5 (1 << 5)
#define PCPU6 (1 << 6)
#define PCPU7 (1 << 7)

void error(char* message);
void printUCharAsBinary(unsigned char n);
void vcpuSchedule(void);
char* getFormat(int type);


typedef struct pCPUStats {
	unsigned long long kernel;
	unsigned long long user;
	unsigned long long idle;
	unsigned long long iowait;
	unsigned long long totalTime;
	int cpuNum;
} pCPUStats;


int main(int argc, char* argv[]){

	//make sure there is one argument
	if(argc != 2){
		error("USAGE: ./vcpu_scheduler interval\n");
	}
	int interval = atoi(argv[1]);
	if(DEBUG){
		printf("Interval is: %d\n",interval);
	}
	
	vcpuSchedule();
}

void vcpuSchedule(){
	//connect to the hypervisor
	virConnectPtr hypervisorConnection = calloc(1, sizeof(virConnectPtr));
	hypervisorConnection = virConnectOpen("qemu:///system");
	if(hypervisorConnection != NULL){
		if(DEBUG){
			printf("Successful connection to hypervisor!\n");
		}
	}
	else{
		error("ERROR, could not connect to the hypervisor!\n");
	}
	
	//before we work on running VMs, get the host system's cpu stats
	pCPUStats * individualPCPUStats;
	pCPUStats * totalCPUStats = calloc(1, sizeof(pCPUStats));
	
	//get number of cpus
	printf("Determining number of CPUs\n");
	unsigned int numCPUsOnline = -1;
	int numCPUS = virNodeGetCPUMap(hypervisorConnection,NULL,&numCPUsOnline,0);
	printf("NumCPUS: %d,Num online: %d\n",numCPUS,numCPUsOnline);
	
	//prep to store the information
	individualPCPUStats = calloc(numCPUS,sizeof(pCPUStats));
	
	//get cpu time for individual cpus
	for(int i = 0; i < numCPUS;i++){
		virNodeCPUStatsPtr params;
		int nParams = 0;
		int cpuNum = i;
		if(virNodeGetCPUStats(hypervisorConnection,cpuNum,NULL,&nParams,0) == 0 && nParams != 0){
			params = calloc(nParams,sizeof(virNodeCPUStats));
			if(virNodeGetCPUStats(hypervisorConnection, cpuNum, params, &nParams, 0)){
				printf("Error getting Host CPU stats\n");
			}
			unsigned long long runningTotal = 0;
			for(int j = 0; j < nParams; j++){
				virNodeCPUStats currentStats = params[j];
				char* field = currentStats.field;
				unsigned long long value = currentStats.value;
				runningTotal += value;
				//kernel,user,idle,iowait
				if(strcmp(field,"kernel") == 0){
					individualPCPUStats[i].kernel = value;
				}
				if(strcmp(field,"user") == 0){
					individualPCPUStats[i].user = value;
				}
				if(strcmp(field,"idle") == 0){
					individualPCPUStats[i].idle = value;
				}
				if(strcmp(field,"iowait") == 0){
					individualPCPUStats[i].iowait = value;
				}
			}
			individualPCPUStats[i].totalTime = runningTotal;
		}		
	}
	
	//cpu time for all cpus, could be useful
	int cpuNum = VIR_NODE_CPU_STATS_ALL_CPUS;
	int nParams = 0;
	virNodeCPUStatsPtr params;
	if(virNodeGetCPUStats(hypervisorConnection,cpuNum,NULL,&nParams,0) == 0 && nParams != 0){
		params = calloc(nParams,sizeof(virNodeCPUStats));
		if(virNodeGetCPUStats(hypervisorConnection, cpuNum, params, &nParams, 0)){
			printf("Error getting Host CPU stats\n");
		}
		unsigned long long runningTotal = 0;
		for(int j = 0; j < nParams; j++){
			virNodeCPUStats currentStats = params[j];
			char* field = currentStats.field;
			unsigned long long value = currentStats.value;
			runningTotal += value;
			//kernel,user,idle,iowait
			if(strcmp(field,"kernel") == 0){
				totalCPUStats->kernel = value;
			}
			if(strcmp(field,"user") == 0){
				totalCPUStats->user = value;
			}
			if(strcmp(field,"idle") == 0){
				totalCPUStats->idle = value;
			}
			if(strcmp(field,"iowait") == 0){
				totalCPUStats->iowait = value;
			}
		}
		totalCPUStats->totalTime = runningTotal;
	}
	
	//debug printout
	if(DEBUG){
		printf("___________________________________________________________________\n");
		printf("CPU STATS\n");
		printf("___________________________________________________________________\n");
		for(int i = 0; i < numCPUS; i++){
			pCPUStats current = individualPCPUStats[i];
			printf("_______________________________________________\n");
			printf("CPU%d\n",i);
			printf("Kernel:%llu\n",current.kernel);
			printf("User:%llu\n",current.user);
			printf("Idle:%llu\n",current.idle);
			printf("IOWait:%llu\n",current.iowait);
			printf("Total Time:%llu\n",current.totalTime);
		}
	
		printf("___________________________________________________________________\n");
		printf("TOTAL CPU STATS\n");
		printf("Kernel:%llu\n",totalCPUStats->kernel);
		printf("User:%llu\n",totalCPUStats->user);
		printf("Idle:%llu\n",totalCPUStats->idle);
		printf("IOWait:%llu\n",totalCPUStats->iowait);
		printf("Total Time:%llu\n",totalCPUStats->totalTime);		
		printf("___________________________________________________________________\n");
	}

	//list all running VMs on the system. Keyword: RUNNING
	//using listAllDomains, to allow for a variable amount of VMs running
	unsigned int flags = VIR_CONNECT_LIST_DOMAINS_RUNNING;
	virDomainPtr* domains = calloc(1, sizeof(virDomainPtr));
	//this returns a list of virDomainPtrs, can use virDomainGetID to get id of specific one
	int numDomains = virConnectListAllDomains(hypervisorConnection, &domains, flags);
	if(numDomains <= 0){
		//bad
		error("ERROR, returned 0 or less domains\n");
	}
	
	if(DEBUG)
		printf("There are %d domains running\n",numDomains);
	
	
	
	//for now, print out stats of the single vcpu for each domain
	for(int i = 0; i < numDomains; i++){
	
		virDomainPtr curDomain = domains[i];
		//int domainID = virDomainGetID(curDomain);
		virTypedParameterPtr params;
		int nparams = virDomainGetCPUStats(curDomain, NULL, 0, -1, 1, 0);
		params = calloc(nparams, sizeof(virTypedParameter));
		int numStats = virDomainGetCPUStats(curDomain, params, nparams, -1, 1, 0);
		if(numStats == -1){
			error("Error fetching vCPU stats");
		}
		
		//let's figure out which parameter means what
		for(int j = 0; j < numStats; j++){
			printf("FIELD: %s\n",params[j].field);
			printf("TYPE: %d\n",params[j].type);
			printf("DATA:\n");
			printf("%d",params[j].value.i);
			printf("%u",params[j].value.ui);
			printf("%lld",params[j].value.l);
			printf("%llu",params[j].value.ul);
			printf("%f",params[j].value.d);
			printf("%c",params[j].value.b);
			printf("%s",params[j].value.s);
			//char* format = getFormat(params[j].type);
			//printf(format,params[j].value);
			printf("\n");
		}
		
	}
	
	//last thing, finish up and close the connection
	virConnectClose(hypervisorConnection);
}

char* getFormat(int type){
	char* format = "";
	switch(type){
		case 1: //int
			format = "%d";
		case 2: //unsigned int
			format = "%u";
		case 3: //long long
			format = "%lld";
		case 4: //unsigned long long
			format = "%llu";
		case 5: //double
			format = "%f";
		case 6: //boolean (really char)
			format = "%c";
		case 7: //char *
			format = "%s";
	}
	return format;
}

void printUCharAsBinary(unsigned char n){
	//char n = c;
	while (n) {
    if (n & 1)
        printf("1");
    else
        printf("0");

    n >>= 1;
}
printf("\n");
}

void error(char* message){
	printf("%s",message);
	exit(-1);
}
