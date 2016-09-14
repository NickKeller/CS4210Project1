#include <stdlib.h>
#include <stdio.h>
#include <libvirt/libvirt.h>

#define DEBUG 1

void error(char* message);
void printUCharAsBinary(unsigned char n);

int main(int argc, char* argv[]){

	//make sure there is one argument
	if(argc != 2){
		error("USAGE: ./vcpu_scheduler interval\n");
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
	
	
	//get the cpumaps of vcpu to pcpu for each domain
	unsigned char* cpumap = calloc(1, sizeof(unsigned char));
	//can use numDomains for ncpumaps because there's only 1 vcpu per domain
	int numVcpus = virDomainGetVcpuPinInfo(domains[0],numDomains,cpumap,sizeof(char),VIR_DOMAIN_AFFECT_LIVE);
	printf("Num VCPUs:%d\n",numVcpus);
	printUCharAsBinary(cpumap[0]);
	
	//last thing, finish up and close the connection
	virConnectClose(hypervisorConnection);
}

void printUCharAsBinary(unsigned char n){
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
