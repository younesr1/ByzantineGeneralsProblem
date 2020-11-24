#include <cmsis_os2.h>
#include "general.h"

// add any #includes here
#include "stdlib.h"

// add any #defines here
#define BUFFER_SIZE 20 // TODO look at post 228 on piazza for how to figure out this number
#define DEFAULT_PRIORITY 0
// add global variables here
typedef struct msg_t {
	char cmd;
	int8_t sender1, sender2, sender3;
} msg_t;
uint8_t m_reporter = 0;
uint8_t m_commander = 0;
uint8_t m_nGeneral = 0; // at most 7
uint8_t m_nTraitors = 0; // at most 2
bool *m_loyal = NULL; // heap alloc
// array of size nGeneneral
osMessageQueueId_t *m_buffers = NULL; // heap allocation


/** Record parameters and set up any OS and other resources
  * needed by your general() and broadcast() functions.
  * nGeneral: number of generals
  * loyal: array representing loyalty of corresponding generals
  * reporter: general that will generate output
  * return true if setup successful and n > 3*m, false otherwise
  */
bool setup(uint8_t nGeneral, bool loyal[], uint8_t reporter) {
	m_reporter = reporter;
	// store loyalty array
	m_loyal = malloc(nGeneral*sizeof(bool));
	if(!c_assert(m_loyal)) return false;
	// count number of traitors
	for(uint8_t i = 0; i < nGeneral; i++) {
		if(!loyal[i]) m_nTraitors++;
		m_loyal[i] = loyal[i];
	}
	if(!c_assert(nGeneral > 3*m_nTraitors)) return false;
	// declare buffers for ALL generals. Commander buffer won't be used
	// This simplifies logic in broadcast()
	m_buffers = malloc(nGeneral * sizeof(osMessageQueueId_t));
	if(!c_assert(m_buffers)) return false;
	for(uint8_t i = 0; i < nGeneral; i++) {
		m_buffers[i] = osMessageQueueNew(BUFFER_SIZE, sizeof(msg_t), NULL);
		if(!c_assert(m_buffers[i])) return false;
	}		
	return true;
}


/** Delete any OS resources created by setup() and free any memory
  * dynamically allocated by setup().
  */
void cleanup(void) {
	// cleanup loyalty array
	free(m_loyal);
	m_loyal = NULL;
	// cleanup buffer array 
	for(uint8_t i = 0; i < m_nGeneral; i++) {
		c_assert(osMessageQueueDelete(m_buffers[i]) == osOK);
	}
	free(m_buffers);
	m_buffers = NULL;
	// TODO: RESET GLOBALS
}

/** This function performs the initial broadcast to n-1 generals.
  * It should wait for the generals to finish before returning.
  * Note that the general sending the command does not participate
  * in the OM algorithm.
  * command: either 'A' or 'R'
  * sender: general sending the command to other n-1 generals
  */
void broadcast(char command, uint8_t commander) {
	m_commander = commander;
	msg_t msg;
	msg.sender1 = commander;
	msg.sender2 = -1;
	msg.sender3 = -1;
	const bool sender_is_loyal = m_loyal[commander];
	const char loyal_cmd = command;
	char traitor_cmd;
	for(uint8_t i = 0; i < m_nGeneral; i++) {
		traitor_cmd = (i%2) == 0 ? ATTACK : RETREAT;
		msg.cmd = sender_is_loyal ? loyal_cmd : traitor_cmd;
		if(commander != i) {
			c_assert(osMessageQueuePut(m_buffers[i], &msg, DEFAULT_PRIORITY, osWaitForever) == osOK);
		}
	}
	// TODO: "It should wait for the generals to finish before returning."	
	while(true);
}


/** Intermediate function used in general()
* temp_commander: id of the commander for the purpose of the function. Not necessarily m_commander
* lieutenants: array of lieutenant id's not including the general's
* recursion_level: number of traitors
	*/
void om(uint8_t temp_commander, uint8_t *lieutenants, uint8_t nLieutenants, uint8_t recursion_level) {
	if(0 == recursion_level) {
		
	}
	else if (recursion_level > 0) {
		// step 1
		for(uint8_t i = 0; i < nLieutenants; i++) {
			c_assert(osMessageQueuePut(m_buffers[lieutenants[i]], SOME_MSG, 0, osWaitForever) == osOK);
		}
		
		
		
		
		
		
		
		
		
		for(uint8_t i = 0; i < m_nGeneral; i++) {
			if(i != m_commander) {
				uint8_t ignore = temp_commander;
				om(m-1, i, cmd, &ignore);
			}
		}
	}
	else {
		c_assert(false);// SHOULDNT GET HERE
	}
}

/** Generals are created before each test and deleted after each
  * test.  The function should wait for a value from broadcast()
  * and then use the OM algorithm to solve the Byzantine General's
  * Problem.  The general designated as reporter in setup()
  * should output the messages received in OM(0).
  * idPtr: pointer to general's id number which is in [0,n-1]
  */
void general(void *idPtr) {
	const uint8_t id = *(uint8_t *)idPtr;
	if (id == m_commander) {return;} // TODO: COMMANDER ISNT INVOLVED IN THIS RIGHT?
	// read from your buffer:
	msg_t input;
	c_assert(osMessageQueueGet(m_buffers[id], &input, DEFAULT_PRIORITY,osWaitForever) == osOK);
	// recursive OM
	om(m_nTraitors, m_commander, something, NULL);
#if false
	########################################### OM(2) ###############################################
	// write to everyone's buffer except yours and the commander's
	msg_t output;
	output.sender1 = input.sender1;
	output.sender2 = id;
	output.sender3 = -1;
	output.cmd = m_loyal[id] ? input.cmd : (id%2 ? ATTACK : RETREAT);
	for(uint8_t i = 0; i < m_nGeneral; i++) {
		if (i!= id && i != m_commander) {
			c_assert(osMessageQueuePut(m_buffers[i], &output, DEFAULT_PRIORITY, osWaitForever) == osOK);
		}
	}
##################################################################################################
	// read what everone else told you:
	msg_t input2;
	for(uint8_t i = 0; i < (m_nGeneral-2); i++) {
		c_assert(osMessageQueueGet(m_buffers[id], &input2, DEFAULT_PRIORITY, osWaitForever) == osOK);
	}
#endif
	// TODO: call recursive function called om()
}
