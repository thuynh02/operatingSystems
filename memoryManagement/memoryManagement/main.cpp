#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <deque>
#include <cmath>
using namespace std;


struct Reference{
	int addr;
	char type;

	Reference(){}

	Reference( int addr, char type )
		: addr(addr), type(type) {}

	Reference( Reference& other )
		: addr(other.addr), type(other.type){}

	void operator=( Reference& other ){
		addr = other.addr;
		type = other.type;
	}
};


struct PageTableEntry{
	bool validBit, dirtyBit, refBit;
	int pid, addr, offset, page, frame;
	PageTableEntry( bool validBit, bool dirtyBit, bool refBit, int pid, int addr, int offset, int page ) 
		: validBit(validBit), dirtyBit(dirtyBit), refBit(refBit), pid(pid), addr(addr), offset(offset), page(page) {}
};


struct PageTable{
	int maxPages;
	vector<PageTableEntry*> pages;

	PageTable( int maxPages ) : maxPages(maxPages) { 
		//Fill the page table with null values
		for( int i = 0; i < maxPages; ++i ){ pages.push_back(NULL); }
	}

};


class Process{
private:
	int pid, arrivalTime, waitTime, pageSize, vaSize, currentRef;
	deque<Reference*> references;
	PageTable* pageTable;

public:
	Process( int pid, int arrivalTime, int pageSize, int vaSize, deque<Reference*>& references )
		: pid(pid), arrivalTime(arrivalTime), waitTime(0), pageSize(pageSize), vaSize(vaSize), currentRef(0), references(references) {

		//Initialize process' page table
		pageTable = new PageTable( pow(2, vaSize)/pageSize );

		//Map all references in the process' page table
		for( int i = 0; i < references.size(); ++i ){
			int pageNum = references[i]->addr/pageSize;
			int offset = references[i]->addr%pageSize;
			bool dirtyBit = (references[i]->type == 'W') ? true : false;
			pageTable->pages[i] = new PageTableEntry( 0, dirtyBit, 0, pid, references[i]->addr, offset, pageNum );
		}
	}


	//Return process id
	int getPID(){ return pid; }		

	//Return process arrival time
	int getArrivalTime(){ return arrivalTime; }

	//Return wait time
	int getWaitTime(){ return waitTime; }

	//Return page size
	int getPageSize(){ return pageSize; }

	//Set wait time by the given amount
	void setWaitTime( int value ){ waitTime = value; }

	//Decrement wait time by 1
	void decrementWait(){ if( waitTime > 0 ){ waitTime--; } }

	//Increment the current pointer
	void incrementNext(){ currentRef++; }

	//Return the number of the current reference the process wants to run
	PageTableEntry* nextTableEntry(){ 
		if( currentRef < pageTable->pages.size() ){ return pageTable->pages[currentRef]; }
		else{ return NULL; }
	}

	//Return the table
	PageTable* getPageTable(){ return pageTable; }

	//Return the deque of references (Used for display tests)
	deque<Reference*> getReferences(){ return references; }

	//Testing purposes
	void displayPageTable(){
		cout << pid << endl;
		for( size_t i = 0; i < pageTable->pages.size(); ++i ){
			if( pageTable->pages[i] != NULL ){
				PageTableEntry* current = pageTable->pages[i];
				cout << " R/W: " << ((current->dirtyBit) ? 'W' : 'R' ) << " VA: " << current->addr
					<< " Page: " << current->page << " Offset: " << current->offset 
					<< " Ref: " << current->refBit << endl;
			}
		}
		cout << endl;
	}

};


class Clock{
private:
	PageTable* frames;
	int next;
	bool debug;

public:
	Clock( PageTable* frames, bool debug ) : frames(frames), next(int(0)), debug(debug) {}


	//Searches through the vector of pages to see if the process reference exists. True is returned if a fault occurs
	bool checkPageFault( PageTableEntry* request ){
		int checkIndex = request->frame;
		if( frames->pages[checkIndex] != NULL &&
			frames->pages[checkIndex]->pid == request->pid ){
				return false;
		}
		return true;
	}


	//Searches through the vector of pages to find a space for the reference.
	//	Returns the type of placement
	//		Free	(Returned if a NULL is replaced)
	//		Clean	(Returned if a clean entry was replaced)
	//		Dirty	(Returned if a dirty entry was replaced)
	string findOpenMemory( PageTableEntry& request ){
		string placementType;
		bool found = false;
		
		while( !found ){
			if( frames->pages[next] == NULL || frames->pages[next]->refBit == 0 ){

				if( frames->pages[next] == NULL )	{ placementType = "Free"; }
				else if( frames->pages[next]->dirtyBit )	{ placementType = "Dirty"; }
				else { placementType = "Clean"; }

				frames->pages[next] = &request;
				request.frame = next;
				found = true;
			}
			else if( frames->pages[next]->refBit == 1 ){
				frames->pages[next]->refBit = 0; 
			}
			
			//Circular increment
			next = (next+1) % frames->maxPages;
		}

		//Tell the request that they've got a spot in memory! Woohoo!
		request.validBit = true;
		return placementType;
	}


	//Returns the frame page table entry at the requested index
	PageTableEntry* getFrameEntryAt( int index ){
		return frames->pages[index];
	}

	//Cleans out pages with the given process id
	void clearPID( int pid ){
		if( debug ){ cout << "Freeing frames: "; }
		for( size_t i = 0; i < frames->pages.size(); ++i ){
			if( frames->pages[i] != NULL && frames->pages[i]->pid == pid ){
				frames->pages[i] = NULL;
				if( debug ){ cout << i << " "; }
			}
		}
		if( debug ){ cout << endl; }
	}


	//Debug
	void status(){
		cout << "Clock:" << endl;
		for( size_t i = 0; i < frames->pages.size(); ++i ){
			if( i == next ){ cout << "->" << i << ") "; }
			else{ cout << "  " << i << ") ";}

			if( frames->pages[i] == NULL ){
				cout << "EMPTY" << endl;
			}
			else{
				PageTableEntry* current = frames->pages[i];
				cout << "R/W: " << ((current->dirtyBit) ? 'W' : 'R' ) << "; VA: " << current->addr
					<< "; PID: " << current->pid << "; Ref: " << current->refBit << endl;
			}
		}

		
		cout << "  Free Frames: ";
		for( size_t i = 0; i < frames->pages.size(); ++i ){
			if( frames->pages[i] == NULL ){ cout << i << " "; }
		}
		cout << endl;
	}

};


class Scheduler{
private:
	Process* running;
	deque<Process*> arrivals;
	deque<Process*> ready;
	deque<Process*> blocked;
	int missPenalty, dirtyPagePenalty, elapsedTime;
	Clock* MMU;
	bool debug;

public:
	Scheduler( deque<Process*>& arrivals, int missPenalty, int dirtyPagePenalty, Clock* MMU, bool debug ) 
		: running(NULL), arrivals(arrivals), missPenalty(missPenalty), dirtyPagePenalty(dirtyPagePenalty), MMU(MMU), debug(debug) {}
	
	//Display entry info
	void displayEntry( PageTableEntry* currentEntry, string placementType ){
		cout << "R/W: "	<< (currentEntry->dirtyBit ? 'W' : 'R' )
						<< "; VA: "     <<  currentEntry->addr
						<< "; Page: "   <<  currentEntry->page
						<< "; Offset: " <<  currentEntry->offset
						<< "; "			<<  placementType
						<< "; Frame: "  <<  currentEntry->frame
						<< "; PA: "     <<  currentEntry->frame*running->getPageSize() + currentEntry->offset
						<< endl;

	}

	//Modified FIFO process scheduler algorithm
	void run(){
		if( arrivals.size() == 0 ){ return; }

		PageTable* currentTable;
		PageTableEntry* currentEntry;
		string placementType;
		bool faulted; 

		while( arrivals.size() > 0 || ready.size() > 0 || blocked.size() > 0 ){

			//If it's time for a process to be ready, put it in the ready queue
			if( arrivals.size() > 0 ){
				ready.push_back( arrivals.front() );
				arrivals.pop_front();
			}

			
			//After waiting in the blocked stage, return to waiting
			if( blocked.size() > 0 ){
				if( blocked.front()->getWaitTime() == 0 ){ 
					ready.push_back( blocked.front() );
					blocked.pop_front();
				}
				else{
					blocked.front()->decrementWait();
				}
			}


			//Put the next ready process in running and check for page fault. Move to 'blocked' if there is one.
			if( ready.size() > 0 && running == NULL ){
				running = ready.front();
				ready.pop_front();
				cout << "Running " << running->getPID() << endl;

				//Let's get the next table entry
				currentTable = running->getPageTable();
				currentEntry = running->nextTableEntry();

				//== Before anything, see if the current reference is in physical memory. ==

				//If it is, then handle it and move on to the next until there isn't
				while( currentEntry != NULL && currentEntry->validBit == 1 ){

					//For the most part, we know the entry has a place in mem, but check just to make sure
					faulted = MMU->checkPageFault( currentEntry );
					if( faulted ){
						//If there WAS a fault, fix that valid bit to 0 and break
						currentEntry->validBit = 0;
						break;
					}

					//If you didn't fault, that means the reference is good to go! You've got a hit
					placementType = "Hit";
					displayEntry( currentEntry, placementType );
					MMU->getFrameEntryAt( currentEntry->frame )->refBit = 1; //Update ref bit in clock


					//If the reference was a write, we need to make sure to flag the entry as "dirty"
					if( currentEntry->dirtyBit ){
						MMU->getFrameEntryAt( currentEntry->frame )->dirtyBit = currentEntry->dirtyBit;
					}

					running->incrementNext();
					currentEntry = running->nextTableEntry();

					//If the currentEntry becomes NULL, then the process is finished with all references!
					//Make sure to "clean" out the pages it used up in physical memory
					if( currentEntry == NULL ){
						MMU->clearPID( running->getPID() );
					}

				}

				//If it isn't, it needs to be blocked and find one
				if( currentEntry != NULL && currentEntry->validBit == 0 ){
					currentEntry->refBit = 1;
					placementType = MMU->findOpenMemory( *currentEntry );
					
					//Other references of the same page must be notified that they have a spot in physical mem now
					for( int i = 0; currentTable->pages[i] != NULL && i < currentTable->pages.size(); ++i ){
						if( currentTable->pages[i]->page == currentEntry->page ){
							currentTable->pages[i]->validBit = 1;
							currentTable->pages[i]->frame = currentEntry->frame;
						}
					}

					//Apply penalty time as see fit
					displayEntry( currentEntry, placementType );
					if( placementType == "Dirty" ) { running->setWaitTime( missPenalty + dirtyPagePenalty ); }
					else{ running->setWaitTime( missPenalty ); }
					blocked.push_back( running );

				}

				
				//debug
				if( debug ){
					MMU->status(); //For debugging
					if( running->getWaitTime() > 0 ){
						cout << "Process: " << running->getPID()
						<< "\tWaiting: " << running->getWaitTime() << endl << endl;
					}
				}

				//The process has done all it can in this run cycle, by this point. Open running up for another process
				running = NULL;
			}


		}
	}
};


//Retrieves all the variable values from the Memory Management file
void readMemManagementFile( ifstream& memManagementFile, string& referenceFileName, int& missPenalty, int& dirtyPagePenalty, int& pageSize, int& VAbits, int& PAbits, bool& debug ){
	string memManagementLine;
	while( getline(memManagementFile, memManagementLine) ){
		size_t foundEqual = memManagementLine.find("=");
		string variableName = memManagementLine.substr( 0, foundEqual );
		string variableValue = memManagementLine.substr( foundEqual + 1 );

		int i = 0;
		while( variableName[i] ){ variableName[i] = tolower( variableName[i] ); i++; }

		if( variableName == "referencefile" ){ referenceFileName = variableValue; }
		else if( variableName == "misspenalty" ){ missPenalty = atoi( variableValue.c_str() ); }
		else if( variableName == "dirtypagepenalty" ){ dirtyPagePenalty = atoi( variableValue.c_str() ); }
		else if( variableName == "pagesize" ){ pageSize = atoi( variableValue.c_str() ); }
		else if( variableName == "vabits" ){ VAbits = atoi( variableValue.c_str() ); }
		else if( variableName == "pabits" ){ PAbits = atoi( variableValue.c_str() ); }
		else if( variableName == "debug" ){ 
			if( variableValue == "0" || variableValue[0] == 'f' || variableValue[0] == 'F' ) { debug = false; }
			else if ( variableValue == "1" || variableValue[0] == 't' || variableValue[0] == 'T' ){ debug = true; }
		}
	}
}


//Retrieves all the variable values from the references file
void readReferenceFile( ifstream& referenceFile, deque<Process*>& processes, int pageSize, int VAbits ){
	string referenceLine;
	getline( referenceFile, referenceLine );
	int numOfProcesses = atoi( referenceLine.c_str() );

	for( int i = 0; i < numOfProcesses; ++i ){
		int addr, pid, numOfReferences;
		char type;
		size_t foundSpace;
		deque<Reference*> references;

		getline( referenceFile, referenceLine );
		while( referenceLine == "" ){ getline( referenceFile, referenceLine ); }
		pid = atoi( referenceLine.c_str() );

		getline( referenceFile, referenceLine );
		numOfReferences = atoi( referenceLine.c_str() );

		for( int j = 0; j < numOfReferences; ++j ){
			getline( referenceFile, referenceLine );
			foundSpace = referenceLine.find(" ");
			addr = atoi( referenceLine.substr( 0, foundSpace ).c_str() );
			type = referenceLine.substr( foundSpace + 1 )[0];
			references.push_back( new Reference(addr, type) );
		}
		processes.push_back( new Process(pid, 0, pageSize, VAbits, references) );
	}
}


//Display all the values from the memory management file
void displayMemFileInfo( string& referenceFileName, int& missPenalty, int& dirtyPagePenalty, int& pageSize, int& VAbits, int& PAbits, bool& debug ){
	cout << "Reference file: " << referenceFileName << endl;
	cout << "Page size: " << pageSize << endl;
	cout << "VA size: " << VAbits << endl;
	cout << "PA size: " << PAbits << endl;
	cout << "Miss penalty: " << missPenalty << endl;
	cout << "Dirty page penalty: " << dirtyPagePenalty << endl;
	cout << "Debug: " << debug << endl;
}


//Display all the process information from the reference file
void displayRefFileInfo( deque<Process*> processes ){
	for( size_t i = 0; i < processes.size(); ++i ){
		cout << processes[i]->getPID() << endl;
		deque<Reference*> tempRefs = processes[i]->getReferences();
		for( size_t j = 0; j < tempRefs.size(); ++j ){
			cout << tempRefs[j]->addr << "\t" << tempRefs[j]->type << endl;
		}
		cout << endl;
	}
}


int main(){

	//Read information from memory management file
	ifstream memManagementFile("MemoryManagement.txt");
	string referenceFileName;
	int missPenalty, dirtyPagePenalty, pageSize, VAbits, PAbits;
	bool debug;

	if( !memManagementFile ){ cout << "Could not open memory management file" << endl; exit(1); }
	else{ readMemManagementFile( memManagementFile, referenceFileName, missPenalty, dirtyPagePenalty, pageSize, VAbits, PAbits, debug ); }


	//Read information from reference file
	ifstream referenceFile( referenceFileName.c_str() );
	deque<Process*> processes;

	if( !referenceFile ){ cout << "Could not open reference file" << endl; exit(1); }
	else{ readReferenceFile(referenceFile, processes, pageSize, VAbits); }

	PageTable frameTable = PageTable( pow(2, PAbits)/pageSize );
	Clock MMU = Clock( &frameTable, debug );
	Scheduler scheduler(processes, missPenalty, dirtyPagePenalty, &MMU, debug);
	scheduler.run();
}