#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cmath>
#include <queue>
using namespace std;

class Process{
private:
	unsigned int pid, arrivalTime, totalCPU, avgBurst;
	unsigned int elapsedTime, timeLeft, burstInterval;
public:

	//Accessors
	int getPID(){ return pid; }								//Returns the process id
	int getArrivalTime(){ return arrivalTime; }				//Returns the time that the process arrived
	int getTotalCPUTime(){ return totalCPU; }				//Returns the total CPU time that the process needs
	int getAverageBurst(){ return avgBurst; }				//Returns the average CPU burst time of the process
	int getWaitTime(){ return elapsedTime; }				//Returns the amount of time the process has waited
	int getTimeLeft(){ return timeLeft; }					//Returns the remaining time left the process needs in the CPU
	int getburstInterval(){ return burstInterval; }			//Returns the amount of time the process has spent in the CPU (loop iterations)

	//Mutators
	void incrementWaitTime() { elapsedTime++; }				//Increment the time the process has been waiting (called when waiting for IO)
	void incrementburstInterval() { burstInterval++; }		//Increment the time spent in the CPU - Should be called after each loop iteration
	void decrementTimeLeft(){ timeLeft--; }					//Decrement total time needed in CPU  - Should be called after each loop iteration

	//Resets
	void resetWaitTime() { elapsedTime = 0; }
	void resetBurstInterval(){ burstInterval = 0; }

	//Constructor
	Process( unsigned int pid, unsigned int arrivalTime, unsigned int totalCPU, unsigned int averageBurst) 
		: pid( pid ), arrivalTime( arrivalTime ), totalCPU( totalCPU ), avgBurst( averageBurst ), 
		  elapsedTime( 0 ), timeLeft( totalCPU ), burstInterval( 0 ) {}
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
		cout << "[Random number (" << randomIntPos << "): " << randomInts[randomIntPos-1] << "\nProbability == " << randomNum << endl;
		return randomNum; 
	}

	//Handles the end of a process' CPU burst based on their average burst time and the random number probability
	//Returns true if they are considered "finished" with their burst
	bool endBurst( Process* current ){
		unsigned int currentElapsedTime = current->getburstInterval();
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

		//========================== REMOVE THIS IF CONTEXT SWITCH NOT NEEDED ============================//
		unsigned int idleTime = 0;		//During a context switch, there is idle time where nothing can enter running. This keeps track!
		//===============================================================================================//

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
			
			//========================== REMOVE THIS IF CONTEXT SWITCH NOT NEEDED ============================//
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
			//===============================================================================================//

			//If there's a process ready to be put into the running stage and it is open, let it run!
			if( readyQueue.size() > 0 && running == NULL && !endBurstTrigger) {
				running = readyQueue.front();
				running->resetBurstInterval();
				readyQueue.pop_front();
				cout << "Time " << sElapsedTime << ": Moving process " << running->getPID() << " from ready to running. Remaining Time: " << running->getTimeLeft() << endl;
			}

			//Otherwise, if the running stage is occupied, check the current process' progress.
			//If the running process is completely done, take it out of running and raise the flag for contextSwitchDelay
			//If it just finished it's current burst, put it into waiting and raise the flag for contextSwitchDelay
			else if( running != NULL ){
				running->decrementTimeLeft(); running->incrementburstInterval(); 
				if( running->getTimeLeft() == 0 ) { 
					cout << "Time " << sElapsedTime << ": Process " << running->getPID() << " finished." << endl;
					running = NULL; 
				}
				else if( endBurst( running ) ){
					cout << "Time " << sElapsedTime << ": Process " << running->getPID() << " ending burst (" << running->getburstInterval() << ").  Remaining time: " << running->getTimeLeft() << endl;
					waitingQueue.push_back( running ); running = NULL; 
				}

				//========================== REMOVE THIS IF CONTEXT SWITCH NOT NEEDED ============================//
				endBurstTrigger = true; 
				//===============================================================================================//
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
	vector<deque<Process*>> readyQueue;
	deque<Process*> waitingQueue;
public:
	CTSS( deque<Process*>& arrivalQueue, int ioDelay, int contextSwitchDelay, bool debug, ifstream& randomFile, unsigned int CTSSQueues ) 
		: Scheduler(ioDelay, contextSwitchDelay, debug, randomFile ), arrivalQueue( arrivalQueue ), running( NULL ), CTSSQueues( CTSSQueues ) {}

	void run(){}
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

	FCFS fcfs = FCFS( arrivalQueue, ioDelay, contextSwitchDelay, debug, randomFile );
	fcfs.run();

	//CTSS ctss = CTSS( arrivalQueue, ioDelay, contextSwitchDelay, debug, randomFile, CTSSQueues );

}