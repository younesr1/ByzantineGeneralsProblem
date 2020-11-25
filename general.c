#include <cmsis_os2.h>
#include "general.h"

// add any #includes here
#include "stdlib.h"
#include "string.h"

// add any #defines here
#define BUFFER_SIZE 20 // TODO look at post 228 on piazza for how to figure out this number
#define DEFAULT_PRIORITY 0
// add global variables here
typedef struct msg_t {
	// "{Sender1, Sender2, \0}" no need for sender 3rd bc its always m_commander
	char path[3];
	char cmd;
} msg_t;
uint8_t m_reporter = 0;
uint8_t m_commander = 0;
uint8_t m_nGeneral = 0; // at most 7
uint8_t m_nTraitors = 0; // at most 2
bool *m_loyal = NULL; // heap allocation
// array of size nGeneneral
osMessageQueueId_t *m_buffers = NULL; // heap allocation
// add function declarations here
void om(uint8_t temp_commander, char cmd, uint8_t recursion_lvl);


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
	// for(i : v) {om(who, input.cmd, m_nTraitors);} // TODO FIGURE OUT HOW TO CALL OM
}


void om(uint8_t temp_commander, char cmd, uint8_t recursion_lvl) {
	if(recursion_lvl == 0) {
		msg_t msg;
		msg.cmd = m_loyal[temp_commander] ? cmd : (temp_commander%2 ? ATTACK : RETREAT);
		char id = '0' + temp_commander;
		strcat(msg.path, &id);
		for(uint8_t i = 0; i < m_nGeneral; i++) {
			// send to everyone but the temp commander and real commander since latter isnt involved in OM()
			if(i != temp_commander && i != m_commander) {
				c_assert(osMessageQueuePut(m_buffers[i], &msg, DEFAULT_PRIORITY, osWaitForever) == osOK);
			}
		}
	}
	else {
		msg_t msg;
		msg.cmd = m_loyal[temp_commander] ? cmd : (temp_commander%2 ? ATTACK : RETREAT);
		char id = '0' + temp_commander;
		strcat(msg.path, &id);
		for(uint8_t i = 0; i < m_nGeneral; i++) {
			// send to everyone but the temp commander and real commander since latter isnt involved in OM()
			if(i != temp_commander && i != m_commander) {
				c_assert(osMessageQueuePut(m_buffers[i], &msg, DEFAULT_PRIORITY, osWaitForever) == osOK);
			}
		}
		for(uint8_t i = 0; i < m_nGeneral; i++) {
			if(i != m_commander && i != temp_commander) {
				char reported_cmd = m_loyal[i] ? cmd : (i%2 ? ATTACK : RETREAT);
				om(i, reported_cmd, recursion_lvl-1);
			}
		}
	}
	
}
