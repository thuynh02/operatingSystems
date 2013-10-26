#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cmath>
using namespace std;

class Scheduler{
protected:
	bool debug;
	vector<int> randomInts;
	int ioDelay, contextSwitchDelay;
	size_t elapsedTime, randomIntPos;
public:
	Scheduler( int ioDelay, int contextSwitchDelay, bool debug ) :  ioDelay( ioDelay ), contextSwitchDelay( contextSwitchDelay ), elapsedTime( 0 ), randomIntPos( 0 ), debug( debug ) {
		ifstream randomFile( "random-numbers.txt" );
		if (!randomFile ){ cerr << "Could not open the file." << endl; exit(1); }
		
		string randomLine;
		while( getline( randomFile, randomLine ) ){ randomInts.push_back( atoi( randomLine.c_str() ) ); }
	}

	double getProbability(){
		return randomInts[randomIntPos++] / pow( 2, 31 );
	}

	void display(){
		for( size_t i = 0; i < 5; i++ ) {
			cout << getProbability() << endl;
		}
	}
};

int main(){
	bool debug;
	int time = 0, ioDelay, contextSwitchDelay, CTSSQueues;

	string processLine;
	string schedulingLine;
	ifstream randomFile( "random-numbers.txt" );
	ifstream schedulingFile( "scheduling.txt" );
	ifstream processFile;

	if( !schedulingFile ){ cerr << "Could not open the file." << endl; exit(1); }
	while( getline( schedulingFile, schedulingLine ) ){
		size_t foundEqual = schedulingLine.find("=");
		string variableName = schedulingLine.substr( 0, foundEqual );
		string variableValue = schedulingLine.substr( foundEqual + 1 );
		cout << "variableName: " << variableName << endl << "variableValue: " << variableValue << endl << endl;
		if( variableName == "ProcessFile" ){
			processFile.open( variableValue );
			if( !processFile ) { cerr << "Could not open the file." << endl; exit(1); }
		}
		else if( variableName == "IOdelay" ){ ioDelay = atoi(variableValue.c_str());}
		else if( variableName == "ContextSwitchDelay" ){ contextSwitchDelay = atoi(variableValue.c_str());}
		else if( variableName == "CTSSQueues" ){ CTSSQueues = atoi(variableValue.c_str()); }
		else if( variableName == "Debug" ){ 
			if( variableValue == "0" || variableValue[0] == 'f' || variableValue[0] == 'F' ) { debug = false; }
			else if ( variableValue == "1" || variableValue[0] == 't' || variableValue[0] == 'T' ){ debug = true; }
		}
	}

	Scheduler scheduler = Scheduler( ioDelay, contextSwitchDelay, debug );
	//scheduler.display();

}