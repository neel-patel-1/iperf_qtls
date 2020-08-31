/*---------------------------------------------------------------
 * Copyright (c) 1999,2000,2001,2002,2003
 * The Board of Trustees of the University of Illinois
 * All Rights Reserved.
 *---------------------------------------------------------------
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software (Iperf) and associated
 * documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 *
 * Redistributions of source code must retain the above
 * copyright notice, this list of conditions and
 * the following disclaimers.
 *
 *
 * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimers in the documentation and/or other materials
 * provided with the distribution.
 *
 *
 * Neither the names of the University of Illinois, NCSA,
 * nor the names of its contributors may be used to endorse
 * or promote products derived from this Software without
 * specific prior written permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE CONTIBUTORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * ________________________________________________________________
 * National Laboratory for Applied Network Research
 * National Center for Supercomputing Applications
 * University of Illinois at Urbana-Champaign
 * http://www.ncsa.uiuc.edu
 * ________________________________________________________________
 *
 * Launch.cpp
 * by Kevin Gibbs <kgibbs@nlanr.net>
 * -------------------------------------------------------------------
 * Functions to launch new server and client threads from C while
 * the server and client are in C++.
 * The launch function for reporters is in Reporter.c since it is
 * in C and does not need a special launching function.
 * ------------------------------------------------------------------- */

#include "headers.h"
#include "Thread.h"
#include "Settings.hpp"
#include "Client.hpp"
#include "Listener.hpp"
#include "Server.hpp"
#include "PerfSocket.hpp"
#include "Write_ack.hpp"

#define MINBARRIERTIMEOUT 3

static int bidir_startstop_barrier (struct BarrierMutex *barrier) {
    int rc = 0;
    assert(barrier != NULL);
    Condition_Lock(barrier->await);
    if (++barrier->count == 2) {
	rc = 1;
	barrier->count = 0;
	// last one wake's up everyone else'
	Condition_Broadcast(&barrier->await);
    } else {
	int timeout = barrier->timeout;
	while ((barrier->count != 2) && (timeout > 0)) {
#ifdef HAVE_THREAD_DEBUG
	    thread_debug("BiDir startstop barrier wait  %p %d/2 (%d)", (void *)&barrier->await, barrier->count, timeout);
#endif
	    Condition_TimedWait(&barrier->await, 1);
	    timeout--;
	    if ((timeout ==0) && (barrier->count != 2)) {
		fprintf(stdout, "Failed to start full duplex traffic\n");
		_exit(0);
	    }
	}
    }
    Condition_Unlock(barrier->await);
    return rc;
}
int bidir_start_barrier (struct BarrierMutex *barrier) {
    int rc=bidir_startstop_barrier(barrier);
#ifdef HAVE_THREAD_DEBUG
    thread_debug("BiDir start barrier done on condition %p ", (void *)&barrier->await, rc);
#endif
    return rc;
}
int bidir_stop_barrier (struct BarrierMutex *barrier) {
    int rc =bidir_startstop_barrier(barrier);
#ifdef HAVE_THREAD_DEBUG
    thread_debug("BiDir stop barrier done on condition %p ", (void *)&barrier->await, rc);
#endif
    return rc;
}

/*
 * listener_spawn is responsible for creating a Listener class
 * and launching the listener. It is provided as a means for
 * the C thread subsystem to launch the listener C++ object.
 */
void listener_spawn(struct thread_Settings *thread) {
    Listener *theListener = NULL;
    // the Listener need to trigger a settings report
    setReport(thread);
    // start up a listener
    theListener = new Listener(thread);

    // Start listening
    theListener->Run();
    DELETE_PTR(theListener);
}

/*
 * server_spawn is responsible for creating a Server class
 * and launching the server. It is provided as a means for
 * the C thread subsystem to launch the server C++ object.
 */
void server_spawn(struct thread_Settings *thread) {
    Server *theServer = NULL;
#ifdef HAVE_THREAD_DEBUG
    thread_debug("Server spawn settings=%p GroupSumReport=%p sock=%d", \
		 (void *) thread, (void *)thread->mSumReport, thread->mSock);
#endif
    // set traffic thread to realtime if needed
#if HAVE_SCHED_SETSCHEDULER
    thread_setscheduler(thread);
#endif
    // Start up the server
    theServer = new Server(thread);
    // Run the test
    if (isUDP(thread)) {
        theServer->RunUDP();
    } else {
        theServer->RunTCP();
    }
    DELETE_PTR(theServer);
}

/*
 * client_spawn is responsible for creating a Client class
 * and launching the client. It is provided as a means for
 * the C thread subsystem to launch the client C++ object.
 *
 * There are a few different client startup modes
 * o) Normal
 * o) Dual (-d or -r) (legacy)
 * o) Reverse (Client side) (client acts like server)
 * o) Bidir (Client side) client starts server
 * o) ServerReverse (Server side) (listener starts a client)
 * o) Bidir (Server side) (listener starts server & client)
 * o) WriteAck
 *
 * Note: This runs in client thread context
 */
void client_spawn(struct thread_Settings *thread) {
    Client *theClient = NULL;
    struct thread_Settings *reverse_client = NULL;

    // set traffic thread to realtime if needed
#if HAVE_SCHED_SETSCHEDULER
    thread_setscheduler(thread);
#endif
    // start up the client
    theClient = new Client(thread);
    // let the reporter thread go first in the case of -P greater than 1
    Condition_Lock(reporter_state.await);
    while (!reporter_state.ready) {
	Condition_TimedWait(&reporter_state.await, 1);
    }
    Condition_Unlock(reporter_state.await);

    if (isConnectOnly(thread)) {
	theClient->ConnectPeriodic();
    } else if (!isServerReverse(thread)) {
	theClient->my_connect();
    }
    if (!isNoConnectSync(thread))
	theClient->BarrierClient(thread->connects_done);

    if (theClient->isConnected()) {
	if (!isReverse(thread) && !isServerReverse(thread) && !isWriteAck(thread)) {
	    // Code for the normal case
	    // Perform any intial startup delays between the connect() and the data xfer phase
	    // this will also initiliaze the report header timestamps needed by the reporter thread
#ifdef HAVE_THREAD_DEBUG
	    thread_debug("Client spawn thread normal (sock=%d)", thread->mSock);
#endif
	    theClient->StartSynch();
	    theClient->Run();
	} else if (isServerReverse(thread)) {
#ifdef HAVE_THREAD_DEBUG
	    thread_debug("Client spawn thread server-reverse (sock=%d)", thread->mSock);
#endif
	    // This is the case of the listener launching a client, no test exchange nor connect
	    theClient->Run();
	} else if (isReverse(thread) || isWriteAck(thread)) {
	    // This is a client side initiated reverse test,
	    // Could be bidir or reverse only
	    // Create thread setting for the reverse_client (i.e. client as server)
	    // Note: Settings copy will malloc space for the
	    // reverse thread settings and the run_wrapper will free it
	    if (isBidir(thread)) {
		thread->bidir_startstop.timeout = (isModeTime(thread) ? ((int)(thread->mAmount / 100) + 1) : 2);
		if (thread->bidir_startstop.timeout < MINBARRIERTIMEOUT)
		    thread->bidir_startstop.timeout = MINBARRIERTIMEOUT;
		Condition_Initialize(&thread->bidir_startstop.await);
		thread->bidir_startstop.count = 0;
		thread->mBidirReport = InitSumReport(thread, thread->mSock);
		IncrSumReportRefCounter(thread->mBidirReport);
	    }
	    Settings_Copy(thread, &reverse_client);
	    FAIL((!reverse_client || !(thread->mSock > 0)), "Reverse test failed to start per thread settings or socket problem",  thread);
	    reverse_client->mSock = thread->mSock; // use the same socket for both directions
	    if (isWriteAck(thread))
		reverse_client->mThreadMode = kMode_WriteAckClient;
	    else
		reverse_client->mThreadMode = kMode_Server;
	    setServerReverse(reverse_client); // cause the connection report to show reverse
	    if (isModeTime(reverse_client)) {
		reverse_client->mAmount += (SLOPSECS * 100);  // add 2 sec for slop on reverse, units are 10 ms
		if (isTxHoldback(thread)) {
		    reverse_client->mAmount += (thread->txholdback_timer.tv_sec * 100);
		    reverse_client->mAmount += (thread->txholdback_timer.tv_usec / 1000000 * 100);
		}
	    }
#ifdef HAVE_THREAD_DEBUG
	    thread_debug("Client spawn thread reverse (sock=%d)", thread->mSock);
#endif
	    theClient->StartSynch();
	    // RJM ADD a thread event here so reverse_client is in a known ready state prior to test exchange
	    // Now exchange client's test information with remote server
	    setReverse(reverse_client);
	    thread_start(reverse_client);
	    // Now handle bidir vs reverse-only for client side invocation
	    if (!isBidir(thread) && !isWriteAck(thread)) {
		// Reverse only, client thread waits on reverse_server and never runs any traffic
		if (!thread_equalid(reverse_client->mTID, thread_zeroid())) {
#ifdef HAVE_THREAD_DEBUG
		    thread_debug("Reverse pthread join sock=%d", reverse_client->mSock);
#endif
		    if (pthread_join(reverse_client->mTID, NULL) != 0) {
			WARN( 1, "pthread_join reverse failed" );
		    } else {
#ifdef HAVE_THREAD_DEBUG
			thread_debug("Client reverse thread finished sock=%d", reverse_client->mSock);
#endif
		    }
		}
	    } else {
		// bidir case or Write ack case, start the client traffic
		theClient->Run();
	    }
	}
    }
    // Call the client's destructor
    DELETE_PTR(theClient);
}

/*
 * client_init handles multiple threaded connects. It creates
 * a listener object if either the dual test or tradeoff were
 * specified. It also creates settings structures for all the
 * threads and arranges them so they can be managed and started
 * via the one settings structure that was passed in.
 *
 * Note: This runs in main thread context
 */
void client_init(struct thread_Settings *clients) {
    struct thread_Settings *itr = NULL;
    struct thread_Settings *next = NULL;

    itr = clients;
    setReport(clients);
    // See if we need to start a listener as well
    Settings_GenerateListenerSettings(clients, &next);

#ifdef HAVE_THREAD
    if (next != NULL) {
        // We have threads and we need to start a listener so
        // have it ran before the client is launched
        itr->runNow = next;
        itr = next;
    }
    // For each of the needed threads create a copy of the
    // provided settings, unsetting the report flag and add
    // to the list of threads to start
    for (int i = 1; i < clients->mThreads; i++) {
        Settings_Copy(clients, &next);
	if (next) {
	    if (isIncrDstIP(clients))
		next->incrdstip = i;
	}
        itr->runNow = next;
        itr = next;
    }
#else
    if (next != NULL) {
        // We don't have threads and we need to start a listener so
        // have it ran after the client is finished
        itr->runNext = next;
    }
#endif
}

/*
 * writeack_server_spawn
 */
void writeack_server_spawn(struct thread_Settings *thread) {
    WriteAck *theServerAck = NULL;
#ifdef HAVE_THREAD_DEBUG
    thread_debug("Write ack server spawn settings=%p sock=%d", (void *) thread, thread->mSock);
#endif
    // set traffic thread to realtime if needed
#if HAVE_SCHED_SETSCHEDULER
    thread_setscheduler(thread);
#endif
    // Start up the server
    theServerAck = new WriteAck(thread);
    // Run the thread
    theServerAck->RunServer();
    DELETE_PTR( theServerAck);
}

/*
 * writeack_client_spawn
 */
void writeack_client_spawn(struct thread_Settings *thread) {
    Server *theServer = NULL;
    WriteAck *theClientAck = NULL;
#ifdef HAVE_THREAD_DEBUG
    thread_debug("Write ack client spawn settings=%p sock=%d", (void *) thread, thread->mSock);
#endif
#if HAVE_SCHED_SETSCHEDULER
    // set traffic thread to realtime if needed
    thread_setscheduler(thread);
#endif
    // the client side server doesn't do write acks
    unsetWriteAck(thread);
    // Start up the server
    theServer = new Server( thread );
    // Run the test
    if ( isUDP( thread ) ) {
        theServer->RunUDP();
    } else {
        theServer->RunTCP();
    }
    DELETE_PTR( theServer);
}
