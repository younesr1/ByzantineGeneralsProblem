#include <cmsis_os2.h>
#include "general.h"

// add any #includes here
#include "stdlib.h"
#include "string.h"

// add any #defines here
#define BUFFER_SIZE0 1 // TODO ADAPT THIS FOR CASE N
#define BUFFER_SIZE1 5 // TODO ADAPT THIS FOR CASE N
#define BUFFER_SIZE2 20 // TODO ADAPT THIS FOR CASE N
#define DEFAULT_PRIORITY 0
#define FOUR 4
#define SIX 6
#define EIGHT 8
// add global variables here
uint8_t m_reporter = 0;
uint8_t m_commander = 0;
uint8_t m_nGeneral = 0; // at most 7
uint8_t m_nTraitors = 0; // at most 2
bool *m_loyal = NULL; // heap allocation
// array of size nGeneneral
osMessageQueueId_t *m_buffers0 = NULL; // this buffer holds strings of size 4
osMessageQueueId_t *m_buffers1 = NULL; // this buffer holds strings of size 6
osMessageQueueId_t *m_buffers2 = NULL; // this buffer holds strings of size 8
osSemaphoreId_t m_enter, m_exit;
// add function declarations here
void om(uint8_t temp_commander, char *path, uint8_t path_length, uint8_t recursion_lvl);
bool general_in_path(uint8_t id, char *path, uint8_t length);

/** Record parameters and set up any OS and other resources
  * needed by your general() and broadcast() functions.
  * nGeneral: number of generals
  * loyal: array representing loyalty of corresponding generals
  * reporter: general that will generate output
  * return true if setup successful and n > 3*m, false otherwise
  */
bool setup(uint8_t nGeneral, bool loyal[], uint8_t reporter) {
	// Before we malloc or change any variables check the condition so we dont have to call cleanup
	for(uint8_t i = 0; i < nGeneral; i++) {
		if(!loyal[i]) m_nTraitors++;
	}
	if(!c_assert(nGeneral > 3*m_nTraitors)) {
		m_nTraitors = 0;
		return false;
	}
	// ANY OTHER REASON FOR FAILURE (e.g malloc) MIGHT MESS UP FUTURE TESTS
	m_reporter = reporter;
	m_nGeneral = nGeneral;
	// store loyalty array
	m_loyal = malloc(nGeneral*sizeof(bool));
	if(!c_assert(m_loyal)) return false;
	// copy loyalty array
	for(uint8_t i = 0; i < nGeneral; i++) {
		m_loyal[i] = loyal[i];
	}
	// declare buffers for ALL generals. Commander buffer won't be used
	m_buffers0 = malloc(nGeneral * sizeof(osMessageQueueId_t));
	m_buffers1 = malloc(nGeneral * sizeof(osMessageQueueId_t));
	m_buffers2 = malloc(nGeneral * sizeof(osMessageQueueId_t));
	if(!c_assert(m_buffers1 && m_buffers1 && m_buffers2)) return false;
	for(uint8_t i = 0; i < nGeneral; i++) {
		m_buffers0[i] = osMessageQueueNew(BUFFER_SIZE0, FOUR, NULL);
		m_buffers1[i] = osMessageQueueNew(BUFFER_SIZE1, SIX, NULL);
		m_buffers2[i] = osMessageQueueNew(BUFFER_SIZE2, EIGHT, NULL);
		if(!c_assert(m_buffers0[i] && m_buffers1[i])) return false;
	}
	m_enter = osSemaphoreNew(nGeneral, 0, NULL);
	m_exit = osSemaphoreNew(nGeneral-1, 0, NULL);
	if(!c_assert(m_enter) || !c_assert(m_exit)) return false;
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
		c_assert(osMessageQueueDelete(m_buffers0[i]) == osOK);
		c_assert(osMessageQueueDelete(m_buffers1[i]) == osOK);
		c_assert(osMessageQueueDelete(m_buffers2[i]) == osOK);
	}
	c_assert(osSemaphoreDelete(m_enter) == osOK);
	c_assert(osSemaphoreDelete(m_exit) == osOK);
	free(m_buffers0);
	free(m_buffers1);
	free(m_buffers2);
	m_buffers0 = NULL;
	m_buffers1 = NULL;
	m_buffers2 = NULL;
	m_reporter = 0;
	m_commander = 0;
	m_nTraitors = 0;
	m_nGeneral = 0;
}

/** This function performs the initial broadcast to n-1 generals.
  * It should wait for the generals to finish before returning.
  * Note that the general sending the command does not participate
  * in the OM algorithm.
  * command: either 'A' or 'R'
  * sender: general sending the command to other n-1 generals
  */
void broadcast(char command, uint8_t commander) { // broadcast is at the 0th level
	m_commander = commander;
	for(uint8_t i = 0; i < m_nGeneral; i++) {
		char msg[FOUR];
		msg[0] = '0' + commander;
		msg[1] = ':';
		msg[2] = m_loyal[commander] ? command : (i%2 ? ATTACK : RETREAT);
		msg[3] = '\0';
		if(commander != i) {
			c_assert(osMessageQueuePut(m_buffers0[i], msg, DEFAULT_PRIORITY, osWaitForever) == osOK);
		}
	}
	for(uint8_t i = 0; i < m_nGeneral; i++) {
		c_assert(osSemaphoreRelease(m_enter) == osOK);
	}
	for(uint8_t i = 0; i < (m_nGeneral-1); i++) {
		c_assert(osSemaphoreAcquire(m_exit, osWaitForever) ==osOK);
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
	c_assert(osSemaphoreAcquire(m_enter, osWaitForever) == osOK);
	const uint8_t id = *(uint8_t *)idPtr;
	if (id == m_commander) return;
	// read from your buffer:
	char input[FOUR];
	c_assert(osMessageQueueGet(m_buffers0[id], input, DEFAULT_PRIORITY,osWaitForever) == osOK);
	// recursive OM
	om(id, input, FOUR, m_nTraitors);
	c_assert(osSemaphoreRelease(m_exit) == osOK);
}

void om(uint8_t temp_commander, char *path, uint8_t path_length, uint8_t recursion_lvl) {
	if(recursion_lvl == 0) {
		if(temp_commander == m_reporter) {
			printf("%s ", path);
		}
	} 
	else if(recursion_lvl == 1 || recursion_lvl == 2) {
		c_assert(path_length == FOUR || path_length == SIX);
		for(uint8_t i = 0; i < m_nGeneral; i++) {
			if(!general_in_path(i, path, path_length) && i!= temp_commander) {
				char msg[path_length+2];
				msg[0] = '0' + temp_commander;
				msg[1] = ':';
				for(uint8_t i = 0; i < path_length; i++){
					if(i == (path_length-2)) { // if dealing with cmd entry
						msg[i+2] = m_loyal[temp_commander] ? path[i] : (temp_commander%2 ? ATTACK : RETREAT);
					} else {
						msg[i+2] = path[i];
					}
				}
				c_assert(osMessageQueuePut(path_length == FOUR ? m_buffers1[i] : m_buffers2[i], msg, DEFAULT_PRIORITY, osWaitForever) == osOK);
			}
		}
		for(uint8_t i = 0; i < m_nGeneral; i++) {
			if(!general_in_path(i, path, path_length) && i!= temp_commander) {
				char input[path_length+2];
				c_assert(osMessageQueueGet(path_length == FOUR ? m_buffers1[temp_commander] : m_buffers2[temp_commander], input, DEFAULT_PRIORITY, osWaitForever) == osOK); 
				om(temp_commander, input, path_length+2, recursion_lvl-1);
			}
		}
	}
	else {
		c_assert(false); // this should never get here
	}
}
	
bool general_in_path(uint8_t id, char *path, uint8_t length) {
	for(uint8_t i = 0; i < length; i++) {
		if(path[i] == ('0' + id)) return true;
	}
	return false;
}
