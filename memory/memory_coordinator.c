#include <stdlib.h>
#include <stdio.h>
#include <libvirt/libvirt.h>
#include <unistd.h>

#define DEBUG 0

void error(char* message);
void coordinateMemory(void);
void test(virDomainPtr domain, char* msg);

int main (int argc, char* argv[]){
	//make sure there is one argument
	if(argc != 2){
		error("USAGE: ./memory_coordinator interval\n");
	}
	int interval = atoi(argv[1]);
	if(DEBUG){
		printf("Interval is: %d\n",interval);
	}
	
	while(1){
		coordinateMemory();	
		sleep(interval);
	}
}

void coordinateMemory(){
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
		
	//initialize an array of tag values
	char * statTagsMapping[] = 
	{
		"VIR_DOMAIN_MEMORY_STAT_SWAP_IN", //total amount of memory written out to swap space
		"VIR_DOMAIN_MEMORY_STAT_SWAP_OUT", //
		"VIR_DOMAIN_MEMORY_STAT_MAJOR_FAULT",//when a page fault has to go to disk
		"VIR_DOMAIN_MEMORY_STAT_MINOR_FAULT",
		"VIR_DOMAIN_MEMORY_STAT_UNUSED", //amount of memory left unused by the system
		"VIR_DOMAIN_MEMORY_STAT_AVAILABLE", //Total amount of usable memory as seen by the domain, in kB
		"VIR_DOMAIN_MEMORY_STAT_ACTUAL_BALLOON", //current balloon value, in kB
		"VIR_DOMAIN_MEMORY_STAT_RSS", //Resient set size of the process running the domain, in kB
		"VIR_DOMAIN_MEMORY_STAT_USABLE", //how much the balloon can be inflated w/o pushing guest system to swap
		"VIR_DOMAIN_MEMORY_STAT_LAST_UPDATE" //Timestamp of last update, in seconds
	};
	
	//virDomainPtr dom = calloc(1,sizeof(virDomainPtr));
	//numDomains = 1;
	for(int i = 0; i < numDomains; i++){
		virDomainPtr curDomain = domains[i];
		if(!curDomain){
			printf("Domain at %d is null\n",i);
			exit(-1);
		}
		printf("CurDomain:%p\n",curDomain);
		int domainID = virDomainGetID(curDomain);
		printf("Domain ID is %d\n",domainID);
		//since there is no performance check, set the stat collection period to 1, then sleep
		printf("Setting memory stats period\n");
		virDomainSetMemoryStatsPeriod(curDomain,1,VIR_DOMAIN_AFFECT_LIVE);
		printf("Sleeping for 1 second\n");
		sleep(1);
		//get all memory stats for this domain
		
		//max memory available
		unsigned long maxMemAvailable = virDomainGetMaxMemory(curDomain);
		printf("Max Memory available for Domain %d is: %lu\n",domainID,maxMemAvailable);
		//continue;
		
		virDomainMemoryStatPtr stats = NULL;
		stats = calloc(9, sizeof(*stats));
		
		//memory stats
		printf("Getting memory stats\n");
		int numStats = virDomainMemoryStats(curDomain, stats, 9, 0);
		printf("Returned %d stats\n", numStats);		
		
		//need Available, unused, balloon size
		unsigned long long curDomainAvailableMemory = 0;
		unsigned long long curDomainUnusedMemory = 0;
		unsigned long long curDomainBalloonSize = 0;
		
		//loop through the stats, since you can't index by tag......
		for(int j = 0; j < numStats; j++){
			int tag = stats[j].tag;
			if(DEBUG) printf("TAG:%d, %s\n",tag, statTagsMapping[tag]);
			if(DEBUG) printf("VAL:%lld\n",stats[j].val);
			if(tag == VIR_DOMAIN_MEMORY_STAT_AVAILABLE){
				curDomainAvailableMemory = stats[j].val;
			}
			if(tag == VIR_DOMAIN_MEMORY_STAT_UNUSED){
				curDomainUnusedMemory = stats[j].val;
			}
			if(tag == VIR_DOMAIN_MEMORY_STAT_ACTUAL_BALLOON){
				curDomainBalloonSize = stats[j].val;
			}
		}
		
		//calculate ratio of unused memory to available
		//ratio means that X% of memory is unused. i.e: .7 means 70% of memory is unused
		double ratio = (double)(curDomainUnusedMemory)/(double)(curDomainAvailableMemory);
		
		printf("Max Domain Available Memory: %lu\n",maxMemAvailable);
		printf("Current Domain Balloon Size: %llu\n", curDomainBalloonSize);
		printf("Current Domain Available Memory: %llu\n",curDomainAvailableMemory);
		printf("Current Domain Unused Memory: %llu\n", curDomainUnusedMemory);
		printf("Ratio of Unused to Available: %f\n", ratio);
		
		/*****************************************************
		* Here's the memory policy:
		* 1-if a guest OS is using 80% or more of it's available memory, set the memory pool to 150% of it's current value, or max available memory, whichever is less
		* 2-if a guest OS is using less than 60% of it's available memory, decrease the memory pool to 70% of it's current size		
		******************************************************/
		
		unsigned long newMemSize = curDomainBalloonSize;
		
		//check for more than 60% free memory
		if(ratio > .6){
			newMemSize = newMemSize * .7;
		}
		//check for less than 20% free memory
		else if(ratio < .2){
			newMemSize = newMemSize * 1.5;
			if(newMemSize > maxMemAvailable){
				newMemSize = maxMemAvailable;
				printf("Warning: Memory increase needed, but cannot increase above maximum memory allowed. Memory set to %lu kB\n",newMemSize);
			}
		}
		
		//time to set the memory size
		int res = virDomainSetMemory(curDomain,newMemSize);
		if(res == 0){
			printf("Successfully set memory size to %lu kB\n", newMemSize);
		}
		else{
			printf("Could not set memory size to %lu kB\n", newMemSize);
		}
		
		//free up the memory used by the domain, to avoid memory leaks
		virDomainFree(curDomain);
	}	
	//last thing, free the hypervisor connection
	printf("Closing hypervisor connection\n");
	virConnectClose(hypervisorConnection);
}	

void test(virDomainPtr domain, char * msg){
	printf("%s",msg);
	int domainID = virDomainGetID(domain);
	printf("Domain id is:%d\n",domainID);
	unsigned long newMemSizeTest = (500l*1024l);
	printf("________TESTING SETTING MEMORY_______________\n");
	int res = virDomainSetMemory(domain,newMemSizeTest);
	if(res == 0){
		printf("Successfully set memory size to %lu\n",newMemSizeTest);
	}
	else{
		printf("Could not set memory size\n");
	}
}


void error(char* message){
	printf("%s",message);
	exit(-1);
}
















































