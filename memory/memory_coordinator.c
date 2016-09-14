#include <stdlib.h>
#include <stdio.h>
#include <libvirt/libvirt.h>

#define DEBUG 1

void error(char* message);

int main (int argc, char* argv[]){
	//make sure there is one argument
	if(argc != 2){
		error("USAGE: ./memory_coordinator interval\n");
	}
	int interval = atoi(argv[1]);
	if(DEBUG){
		printf("Interval is: %d\n",interval);
	}
	
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
	
	for(int i = 0; i < numDomains; i++){
		virDomainPtr curDomain = domains[i];
		virDomainSetMemoryStatsPeriod(
		if(!curDomain){
			printf("Domain at %d is null\n",i);
			exit(-1);
		}
		int domainID = virDomainGetID(curDomain);
		printf("Domain ID is %d\n",domainID);
		//get all memory stats for this domain
		//max memory available
		printf("Getting max mem available\n");
		//unsigned long maxMemAvailable = virDomainGetMaxMemory(curDomain);
		
		virDomainMemoryStatPtr stats = calloc(1, sizeof(virDomainMemoryStatPtr));
		//memory stats
		printf("Getting memory stats\n");
		int numStats = virDomainMemoryStats(curDomain, stats, 9, 0);
		printf("Returned %d stats\n", numStats);
		for(int j = 0; j < numStats; j++){
			printf("TAG:%d\n",stats[j].tag);
		}
		
		//printf("Max Mem available: %lu\n", maxMemAvailable);
	}
	
	
	//last thing, free the hypervisor connection
	virConnectClose(hypervisorConnection);
}


void error(char* message){
	printf("%s",message);
	exit(-1);
}
















































