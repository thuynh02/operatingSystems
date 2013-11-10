#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cmath>
#include <queue>
using namespace std;

class Process{
private:
	unsigned int pid, arrivalTime, totalCPU, avgBurst; //General information about the process
	unsigned int elapsedTime, timeLeft, burstInterval; //Time tracking data members used in the scheduling algorithms
	unsigned int guaranteedTime, priorityLevel; //CTSS specific
public:

	//Accessors
	int getPID(){ return pid; }								//Returns the process id
	int getArrivalTime(){ return arrivalTime; }				//Returns the time that the process arrived
	int getTotalCPUTime(){ return totalCPU; }				//Returns the total CPU time that the process needs
	int getAverageBurst(){ return avgBurst; }				//Returns the average CPU burst time of the process
	int getWaitTime(){ return elapsedTime; }				//Returns the amount of time the process has waited
	int getTimeLeft(){ return timeLeft; }					//Returns the remaining time left the process needs in the CPU
	int getBurstInterval(){ return burstInterval; }			//Returns the current amount of time spent in the CPU (loop iterations)
	int getGuaranteedTime(){ return guaranteedTime; }		//Returns the guaranteed time left given to the process
	int getPriorityLevel(){ return priorityLevel; }			//Returns the current priority level of the process

	//Mutators
	void incrementWaitTime() { elapsedTime++; }				//Increment the time the process has been waiting (called when waiting for IO)
	void incrementburstInterval() { burstInterval++; }		//Increment the time spent in the CPU - Should be called after each loop iteration
	void incrementPriority(){ priorityLevel++; }			//Increment the priority level of the process
	void decrementPriority(){ priorityLevel--; }			//Decrement the priorit level of the process
	void decrementTimeLeft(){ timeLeft--; }					//Decrement total time needed in CPU  - Should be called after each loop iteration
	void decrementGuaranteedTime(){ guaranteedTime--; }		//Decrement the guaranteed time left
	void setGuaranteedTime( int t ){ guaranteedTime = t; }	//Sets the guaranteed time - Used to assign time quantums 

	//Resets
	void resetWaitTime() { elapsedTime = 0; }
	void resetBurstInterval(){ burstInterval = 0; }

	//Constructor
	Process( unsigned int pid, unsigned int arrivalTime, unsigned int totalCPU, unsigned int averageBurst) 
		: pid( pid ), arrivalTime( arrivalTime ), totalCPU( totalCPU ), avgBurst( averageBurst ), 
		  elapsedTime( 0 ), timeLeft( totalCPU ), burstInterval( 0 ), guaranteedTime(1), priorityLevel(0) {}
};
class Scheduler{
protected:
	bool debug;
	vector<int> randomInts;
	unsigned int ioDelay, contextSwitchDelay;
	unsigned int sElapsedTime;
	size_t randomIntPos;

public:
	Scheduler( int ioDelay, int contextSwitchDelay, bool debug, ifstream& randomFile ) 
		: ioDelay( ioDelay ), contextSwitchDelay( contextSwitchDelay ), sElapsedTime( 0 ), randomIntPos( 0 ), debug( debug ) {
		string randomLine;
		if (!randomFile ){ cerr << "Could not open the file." << endl; exit(1); }
		while( getline( randomFile, randomLine ) ){ randomInts.push_back( atoi( randomLine.c_str() ) ); }
	}

	//Simulates and outputs the probability that a process is complete using the vector of random integers. 
	//The return value is the next random number of the vector dividied by 2^31
	double getProbability(){ 
		double randomNum = randomInts[randomIntPos++] / pow( 2, 31 );
		cout << "[Random number (" << randomIntPos << "): " << randomInts[randomIntPos-1] << "]\nProbability == " << randomNum << endl;
		return randomNum; 
	}

	//Handles the end of a process' CPU burst based on their average burst time and the random number probability
	//Returns true if they are considered "finished" with their burst
	bool endBurst( Process* current ){
		unsigned int currentElapsedTime = current->getBurstInterval();
		unsigned int avgBurst = current->getAverageBurst();
		if( currentElapsedTime == current->getTotalCPUTime() ) return true;
		else if( currentElapsedTime < (avgBurst - 1) ) return false;
		else if( currentElapsedTime == (avgBurst - 1)  ) {
			if ( getProbability() <= 1/3.0 ){ return true; }
			else { return false;}
		}
		else if( currentElapsedTime == avgBurst  ) {
			if ( getProbability() <= 0.5 ){ return true; }
			else { return false;}
		}
		else if( currentElapsedTime > avgBurst ) return true;
	}

	//Pure virtual to enforce that all scheduler algorithms have a run function
	virtual void run() = 0;
};
class FCFS : public Scheduler{
private:
	Process* running;
	deque<Process*> arrivalQueue;
	deque<Process*> readyQueue;
	deque<Process*> waitingQueue;

public: 
	FCFS( deque<Process*>& arrivalQueue, int ioDelay, int contextSwitchDelay, bool debug, ifstream& randomFile ) 
		: Scheduler(ioDelay, contextSwitchDelay, debug, randomFile ), arrivalQueue( arrivalQueue ), running( NULL ) {}

	//Goes through the provided queue of process pointers and displays each process ID with a space in between
	//Once it is finished displaying all the process IDs, it outputs an endline 
	void displayQueue( deque<Process*>& currentQueue ){
		for( size_t i = 0; i < currentQueue.size(); i++ ){ 
			cout << currentQueue[i]->getPID() << " ";
		}
		cout << endl;
	}

	//Displays the processes that occupy each stage (Running, Arrival, Ready, and Waiting)
	void displayCurrentPeriod(){
		cout << "==========" << endl;
		cout << "Time; " << sElapsedTime << endl;
		if( running == NULL ) { cout << "Running: none" << endl; }
		else{ cout << "Running: " << running->getPID() << endl; }

		if( arrivalQueue.size() == 0 ) { cout << "Arrival: none" << endl; }
		else { cout << "Arrival: "; displayQueue( arrivalQueue ); }

		if( readyQueue.size() == 0 ) { cout << "Ready: none" << endl; }
		else { cout << "Ready: "; displayQueue( readyQueue ); }

		if( waitingQueue.size() == 0 ) { cout << "Waiting: none" << endl; }
		else { cout << "Waiting: "; displayQueue( waitingQueue ); }
		
		cout << "==========" << endl;
	}

	void run(){
		
		//Do nothing if no processes have arrived
		if( arrivalQueue.size()== 0 ) return;

		sElapsedTime = 0;				//Track the total clock ticks 
		bool endBurstTrigger = false;	//Used to determine if a burst/burstie was finished (acts as context switch flag)
		unsigned int idleTime = 0;		//During a context switch, there is idle time where nothing can enter running. This keeps track!

		//While there is something in any of the stages
		while( arrivalQueue.size() > 0 || waitingQueue.size() > 0 || readyQueue.size() > 0 || running != NULL ) {

			//Display the current status of those stages
			displayCurrentPeriod();

			//If something is scheduled to have arrived at the current clock tick, put it into the ready queue
			if( arrivalQueue.size() > 0 && arrivalQueue.front()->getArrivalTime() == sElapsedTime ) { 
				cout << "Time " << sElapsedTime << ": Moving process " << arrivalQueue.front()->getPID() << " from arrival to ready." << endl;
				readyQueue.push_back( arrivalQueue.front() );
				arrivalQueue.pop_front();
			}

			//If there are processes in the waiting stage, have the front process continue to wait for "IOdelay" amount of clock ticks
			//Once finished, send it to the ready stage and remove it from waiting
			if( waitingQueue.size() > 0 ) { 
				if( waitingQueue.front()->getWaitTime() == ioDelay - 1 ) {
					cout << "Time " << sElapsedTime << ": Moving process " << waitingQueue.front()->getPID() << " from waiting to ready." << endl;
					waitingQueue.front()->resetWaitTime();
					readyQueue.push_back( waitingQueue.front() );
					waitingQueue.pop_front();
				}
				else{ waitingQueue.front()->incrementWaitTime(); }
			}

			//If a process had finished a burst, the running stage cannot be occupied due to a context switch
			//This condition checks to see if the CPU has been idle for that amount of time before becoming open to ready processes
			//Note: Context switches that take 1 clock tick are not mentioned
			if( endBurstTrigger && running == NULL ){
				idleTime++;
				if( idleTime < contextSwitchDelay ){ 
					cout << "Time " << sElapsedTime << ": Undergoing context switch." << endl;
				}
				else{
					endBurstTrigger = false;
					idleTime = 0;
				}
			}

			
			//Otherwise, if the running stage is occupied, check the current process' progress.
			if( running != NULL ){
				running->decrementTimeLeft(); running->incrementburstInterval(); 
				if( running->getTimeLeft() == 0 ) { 
					cout << "Time " << sElapsedTime << ": Process " << running->getPID() << " finished." << endl;
					delete running;
					running = NULL; 
				}
				else if( endBurst( running ) ){
					cout << "Time " << sElapsedTime << ": Process " << running->getPID() << " ending burst (" << running->getBurstInterval() << ").  Remaining time: " << running->getTimeLeft() << endl;
					waitingQueue.push_back( running ); running = NULL; 
				}
				if( contextSwitchDelay > 0 ) { endBurstTrigger = true; }
			}
			//If there's a process ready to be put into the running stage and it is open, let it run!
			if( readyQueue.size() > 0 && running == NULL && !endBurstTrigger) {
				running = readyQueue.front();
				running->resetBurstInterval();
				readyQueue.pop_front();
				cout << "Time " << sElapsedTime << ": Moving process " << running->getPID() << " from ready to running. Remaining Time: " << running->getTimeLeft() << endl;
			}

			sElapsedTime++;
		}
	}
};
class CTSS : public Scheduler {
private:
	unsigned int CTSSQueues;
	Process* running;
	deque<Process*> arrivalQueue;
	vector<deque<Process*>> readyQueues;
	deque<Process*> waitingQueue;
public:
	CTSS( deque<Process*>& arrivalQueue, int ioDelay, int contextSwitchDelay, bool debug, ifstream& randomFile, unsigned int CTSSQueues ) 
		: Scheduler(ioDelay, contextSwitchDelay, debug, randomFile ), arrivalQueue( arrivalQueue ), running( NULL ), CTSSQueues( CTSSQueues ) 
	{
		for( int i = 0; i < CTSSQueues; ++i ){
			readyQueues.push_back( deque<Process*>() );
		}
	}

	//Goes through the provided queue of process pointers and displays each process ID with a space in between
	//Once it is finished displaying all the process IDs, it outputs an endline 
	void displayQueue( deque<Process*>& currentQueue ){
		for( size_t i = 0; i < currentQueue.size(); i++ ){ 
			cout << currentQueue[i]->getPID() << " ";
		}
		cout << endl;
	}

	//Displays the processes that occupy each stage (Running, Arrival, Ready, and Waiting)
	void displayCurrentPeriod(){
		cout << "==========" << endl;
		cout << "Time; " << sElapsedTime << endl;
		if( running == NULL ) { cout << "Running: none" << endl; }
		else{ cout << "Running: " << running->getPID() << " (" << running->getPriorityLevel() << ")" << endl; }

		if( arrivalQueue.size() == 0 ) { cout << "Arrival: none" << endl; }
		else { cout << "Arrival: "; displayQueue( arrivalQueue ); }

		for( size_t i = 0; i < readyQueues.size(); ++i ){
			cout << "Ready[" << i << "]: ";
			if( readyQueues[i].size() == 0 ) { cout << "none" << endl; }
			else { displayQueue( readyQueues[i] ); }
		}

		if( waitingQueue.size() == 0 ) { cout << "Waiting: none" << endl; }
		else { 
			cout << "Waiting: "; 
			for( size_t i = 0; i < waitingQueue.size(); ++i ){ 
				cout << waitingQueue[i]->getPID() << " (" << waitingQueue[i]->getPriorityLevel() << ") ";
			}
			cout << endl;
		}
		
		cout << "==========" << endl;
	}

	//Returns true if the ready  are empty. Otherwise, false
	bool areReadyQueuesEmpty() {
		bool trigger = true;
		for( size_t i = 0; i < readyQueues.size(); i++ ){
			if( readyQueues[i].size() > 0 ) { trigger = false; }
		}
		return trigger;
	}
	
	//Returns the lowest index of an occupied queue in the readyQueues. This is the highest occupied priority
	size_t getHighestPrioritizedProcessQueue(){
		for( size_t i = 0; i < readyQueues.size(); i++ ){
			if( readyQueues[i].size() > 0 ) { return i; }
		}
	}

	void run(){
		//Do nothing if no processes have arrived
		if( arrivalQueue.size()== 0 ) return;

		sElapsedTime = 0;
		int quantum = 0;
		int highestOccupiedPriority = 0;
		bool endOfAvgBurst = false;
		bool endBurstTrigger = false;		//Used to determine if a burst/burstie was finished (acts as context switch flag)
		unsigned int idleTime = 0;			//This keeps track of the time where nothing can enter running. (for context switch)

		//While there is something in any of the stages
		while( arrivalQueue.size() > 0 || waitingQueue.size() > 0 || !areReadyQueuesEmpty() || running != NULL ) {

			//Display the current status of those stages
			displayCurrentPeriod();

			//If something is scheduled to have arrived at the current clock tick, put it into the first priority ready queue
			if( arrivalQueue.size() > 0 && arrivalQueue.front()->getArrivalTime() == sElapsedTime ) { 
				cout << "Time " << sElapsedTime << ": Moving process " << arrivalQueue.front()->getPID() << " from arrival to ready." << endl;
				readyQueues[0].push_back( arrivalQueue.front() );
				arrivalQueue.pop_front();
			}

			//If there are processes in the waiting stage, have the front process continue to wait for "IOdelay" amount of clock ticks
			//Once finished, send it to the correct priority ready stage and remove it from waiting
			if( waitingQueue.size() > 0 ) { 
				if( waitingQueue.front()->getWaitTime() == ioDelay - 1 ) {
					cout << "Time " << sElapsedTime 
						 << ": Moving process " << waitingQueue.front()->getPID() << " from waiting to ready." << endl;
					waitingQueue.front()->resetWaitTime();
					waitingQueue.front()->resetBurstInterval();
					waitingQueue.front()->setGuaranteedTime( pow( 2, waitingQueue.front()->getPriorityLevel() ) );
					readyQueues[waitingQueue.front()->getPriorityLevel()].push_back( waitingQueue.front() );
					waitingQueue.pop_front();
				}
				else{ waitingQueue.front()->incrementWaitTime(); }
			}

			//If a process had finished a burst, the running stage cannot be occupied due to a context switch
			//This condition checks to see if the CPU has been idle for that amount of time before becoming open to ready processes
			//Note: Context switches that take 1 clock tick are not mentioned
			if( endBurstTrigger && running == NULL ){
				idleTime++;
				if( idleTime < contextSwitchDelay ){ 
					cout << "Time " << sElapsedTime << ": Undergoing context switch." << endl;
				}
				else{
					endBurstTrigger = false;
					idleTime = 0;
				}
			}

			//If the running stage is occupied, check the current process' progress.
			if( running != NULL ){
				highestOccupiedPriority = getHighestPrioritizedProcessQueue();
				quantum = pow(2, running->getPriorityLevel());
				running->decrementGuaranteedTime(); 
				running->decrementTimeLeft(); 
				running->incrementburstInterval(); 

				//Check if it has used up all the CPU time it needs
				if( running->getTimeLeft() == 0 ) { 
					cout << "Time " << sElapsedTime << ": Process " << running->getPID() << " finished." << endl;
					delete running;
					running = NULL;
				}
				
				//Check if avg burst is finished & adjust priority accordingly
				else if( endBurst( running ) ){
					cout << "Time " << sElapsedTime 
						 << ": Process " << running->getPID() 
						 << " ending burst. Remaining time: " << running->getTimeLeft() << endl;

					//If the process used up less than or equal to half the quantum before heading to waiting, 
					//then give it a higher priority
					if( running->getPriorityLevel() != 0 && 
						running->getBurstInterval() - running->getGuaranteedTime() <= quantum/2 ) { running->decrementPriority(); }
					waitingQueue.push_back( running ); 
					running = NULL;
				}

				//If a higher priority process exists, preempt the currently running process
				else if( highestOccupiedPriority < running->getPriorityLevel() ){
					readyQueues[ running->getPriorityLevel() ].push_front( running );
					cout << "Time " << sElapsedTime << ": Process " << running->getPID() << " preempted." << endl;
					running = NULL;
				}

				//If the quantum has been used up, move the running process down a priority level
				else if( running->getGuaranteedTime() == 0){
					running->incrementPriority();
					running->setGuaranteedTime( pow(2, running->getPriorityLevel()) );
					readyQueues[ running->getPriorityLevel() ].push_back( running );
					cout << "Time " << sElapsedTime 
						 << ": Process " << running->getPID() 
						 << " ending quantum. Remaining time: " << running->getTimeLeft() << endl;
					running = NULL;
				}
				endBurstTrigger = true; 
			}

			//If there's a process ready to be put into the running stage and it is open, let it run!
			if( running == NULL && !endBurstTrigger && !areReadyQueuesEmpty()) {
				highestOccupiedPriority = getHighestPrioritizedProcessQueue();
				running = readyQueues[ highestOccupiedPriority ].front();
				readyQueues[ highestOccupiedPriority ].pop_front();
				cout << "Time " << sElapsedTime 
					 << ": Moving process " << running->getPID() 
					 << " from ready to running. Remaining Time: " << running->getTimeLeft() << endl;
			}
			sElapsedTime++;
		}
	}
};

int main(){
	bool debug;
	unsigned int ioDelay, contextSwitchDelay, CTSSQueues;

	string processLine;
	string schedulingLine;
	ifstream randomFile( "random-numbers.txt" );
	ifstream schedulingFile( "scheduling.txt" );
	ifstream processFile;
	deque<Process*> arrivalQueue;

	if( !schedulingFile ){ cerr << "Could not open the file." << endl; exit(1); }

	//Gather the scheduler information from the scheduling file
	while( getline( schedulingFile, schedulingLine ) ){
		size_t foundEqual = schedulingLine.find("=");
		string variableName = schedulingLine.substr( 0, foundEqual );
		string variableValue = schedulingLine.substr( foundEqual + 1 );

		//From the scheduling file, get the name of the file that contains process information and fetch it
		if( variableName == "ProcessFile" ){
			processFile.open( variableValue );
			if( !processFile ) { cerr << "Could not open the file." << endl; exit(1); }

			while( getline( processFile, processLine ) ){
				int pid, arrivalTime, totalCPU, avgBurst;
				sscanf_s( processLine.c_str(), "%d %d %d %d", &pid, &arrivalTime, &totalCPU, &avgBurst );
				Process* newProcess = new Process( pid, arrivalTime, totalCPU, avgBurst );
				arrivalQueue.push_back( newProcess );
			}
		}

		//Convert the names and values gathered from the scheduling file and make them into usable variables
		else if( variableName == "IOdelay" ){ ioDelay = atoi(variableValue.c_str());}
		else if( variableName == "ContextSwitchDelay" ){ contextSwitchDelay = atoi(variableValue.c_str());}
		else if( variableName == "CTSSQueues" ){ CTSSQueues = atoi(variableValue.c_str()); }
		else if( variableName == "Debug" ){ 
			if( variableValue == "0" || variableValue[0] == 'f' || variableValue[0] == 'F' ) { debug = false; }
			else if ( variableValue == "1" || variableValue[0] == 't' || variableValue[0] == 'T' ){ debug = true; }
		}
	}

	//If times don't match sample output, then it might be the process & scheduler text files
	/*FCFS fcfs = FCFS( arrivalQueue, ioDelay, contextSwitchDelay, debug, randomFile );
	fcfs.run();*/

	CTSS ctss = CTSS( arrivalQueue, ioDelay, contextSwitchDelay, debug, randomFile, CTSSQueues );
	ctss.run();

}