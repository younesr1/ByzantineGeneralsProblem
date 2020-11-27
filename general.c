#include <cmsis_os2.h>
#include "general.h"

// add any #includes here
#include "stdlib.h"
#include "string.h"

//#define DEBUG
#ifdef DEBUG
osMutexId_t debug_mutex;
#endif

// add any #defines here
#define BUFFER_SIZE0 1 // TODO ADAPT THIS FOR CASE N
#define BUFFER_SIZE1 2 // TODO ADAPT THIS FOR CASE N
#define DEFAULT_PRIORITY 0
// add global variables here
typedef struct msg0_t {
	char msg[4]; 
} msg0_t;
typedef struct msg1_t {
	char msg[6];
} msg1_t;
uint8_t m_reporter = 0;
uint8_t m_commander = 0;
uint8_t m_nGeneral = 0; // at most 7
uint8_t m_nTraitors = 0; // at most 2
bool *m_loyal = NULL; // heap allocation
// array of size nGeneneral
osMessageQueueId_t *m_buffers0 = NULL; // heap allocation
osMessageQueueId_t *m_buffers1 = NULL; // heap allocation
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
	if(!c_assert(m_buffers1 && m_buffers1)) return false;
	for(uint8_t i = 0; i < nGeneral; i++) {
		m_buffers0[i] = osMessageQueueNew(BUFFER_SIZE0, sizeof(msg0_t), NULL);
		m_buffers1[i] = osMessageQueueNew(BUFFER_SIZE1, sizeof(msg1_t), NULL);
		if(!c_assert(m_buffers0[i] && m_buffers1[i])) return false;
	}
	m_enter = osSemaphoreNew(nGeneral, 0, NULL);
	m_exit = osSemaphoreNew(nGeneral-1, 0, NULL);
	if(!c_assert(m_enter) || !c_assert(m_exit)) return false;
#ifdef DEBUG
	debug_mutex = osMutexNew(NULL);
#endif
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
	}
	c_assert(osSemaphoreDelete(m_enter) == osOK);
	c_assert(osSemaphoreDelete(m_exit) == osOK);
	free(m_buffers0);
	free(m_buffers1);
	m_buffers0 = NULL;
	m_buffers1 = NULL;
	m_reporter = 0;
	m_commander = 0;
	m_nTraitors = 0;
	m_nGeneral = 0;
#ifdef DEBUG
	c_assert(osMutexDelete(debug_mutex) == osOK);
#endif
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
		msg0_t msg;
		msg.msg[0] = '0' + commander;
		msg.msg[1] = ':';
		msg.msg[2] = m_loyal[commander] ? command : (i%2 ? ATTACK : RETREAT);
		msg.msg[3] = '\0';
		if(commander != i) {
			c_assert(osMessageQueuePut(m_buffers0[i], &msg, DEFAULT_PRIORITY, osWaitForever) == osOK);
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
	msg0_t input;
	c_assert(osMessageQueueGet(m_buffers0[id], &input, DEFAULT_PRIORITY,osWaitForever) == osOK);
#ifdef DEBUG
	osMutexAcquire(debug_mutex, osWaitForever);
	printf("%s\n", input.msg);
	osMutexRelease(debug_mutex);
#endif
	// recursive OM
	om(id, input.msg, sizeof(input.msg)/sizeof(input.msg[0]), m_nTraitors);
	c_assert(osSemaphoreRelease(m_exit) == osOK);
}

void om(uint8_t temp_commander, char *path, uint8_t path_length, uint8_t recursion_lvl) {
	if(recursion_lvl == 0) {
		if(temp_commander == m_reporter) {
			printf("%s ", path);
		}
	} 
	else if(recursion_lvl == 1) {
		c_assert(path_length == 4); // TODO: DOES THIS SCALE BEYOND CASE 1??? if it doesnt, put a for loop after msg.msg[1] = ':';
		for(uint8_t i = 0; i < m_nGeneral; i++) {
			if(!general_in_path(i, path, path_length) && i!= temp_commander) {
				msg1_t msg;
				msg.msg[0] = '0' + temp_commander;
				msg.msg[1] = ':';
				msg.msg[2] = path[0];
				msg.msg[3] = path[1];
				msg.msg[4] = m_loyal[temp_commander] ? path[2] : (temp_commander%2 ? ATTACK : RETREAT);
				msg.msg[5] = path[3];
				c_assert(osMessageQueuePut(m_buffers1[i], &msg, DEFAULT_PRIORITY, osWaitForever) == osOK);
			}
		}
		for(uint8_t i = 0; i < (m_nGeneral-1-recursion_lvl); i++) {
			msg1_t input;
			c_assert(osMessageQueueGet(m_buffers1[temp_commander], &input, DEFAULT_PRIORITY, osWaitForever) == osOK);
			om(temp_commander, input.msg, sizeof(input.msg)/sizeof(input.msg[0]), recursion_lvl-1);
		}
	}
	else if(recursion_lvl == 2) {
		c_assert(false); // not done this yet
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

#if false
void om(uint8_t temp_commander, char *path, uint8_t recursion_lvl) {
	if(recursion_lvl == 0) {
		if(temp_commander == m_reporter) {
			printf("%s ", path);
		}
	} else if(recursion_lvl > 0) {
		msg1_t msg;
		msg.msg[0] = '0' + temp_commander;
		msg.msg[1] = ':';
		msg.msg[2] = '0' + m_commander;
		msg.msg[3] = ':';
		msg.msg[4] = m_loyal[temp_commander] ? cmd : (temp_commander%2 ? ATTACK : RETREAT);
		msg.msg[5] = '\0';
		for(uint8_t i = 0; i < m_nGeneral; i++) {
			if (i != m_commander && i != temp_commander) {
				c_assert(osMessageQueuePut(m_buffers1, &msg, DEFAULT_PRIORITY, osWaitForever) == osOK);
			}
		}
	}
}
#endif
