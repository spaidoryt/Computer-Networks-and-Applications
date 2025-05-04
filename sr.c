#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "sr.h"

/* Global counters */
int total_ACKs_received = 0;
int window_full = 0;
int packets_resent = 0;
int correct_packets_received = 0;
int messages_delivered = 0;

#define RTT 16.0
#define WINDOWSIZE 6
#define SEQSPACE 7
#define NOTINUSE (-1)

/* Compute checksum for packet validation */
int ComputeChecksum(struct pkt packet) {
    int checksum = packet.seqnum + packet.acknum;
    for (int i = 0; i < 20; i++) {
        checksum += (int)packet.payload[i];
    }
    return checksum;
}

/* Check if packet is corrupted */
bool IsCorrupted(struct pkt packet) {
    return packet.checksum != ComputeChecksum(packet);
}

/********* Sender (A) Implementation *********/
static struct pkt buffer[WINDOWSIZE];
static int windowfirst = 0;
static int windowlast = -1;
static int windowcount = 0;
static int A_nextseqnum = 0;

void A_output(struct msg message) {
    if (windowcount < WINDOWSIZE) {
        struct pkt sendpkt;
        
        /* Prepare the packet */
        sendpkt.seqnum = A_nextseqnum;
        sendpkt.acknum = NOTINUSE;
        for (int i = 0; i < 20; i++) {
            sendpkt.payload[i] = message.data[i];
        }
        sendpkt.checksum = ComputeChecksum(sendpkt);

        /* Add to buffer */
        windowlast = (windowlast + 1) % WINDOWSIZE;
        buffer[windowlast] = sendpkt;
        windowcount++;

        /* Send packet */
        if (TRACE > 0) {
            printf("Sending packet %d to layer 3\n", sendpkt.seqnum);
        }
        tolayer3(A, sendpkt);

        /* Start timer if first packet in window */
        if (windowcount == 1) {
            if (TRACE > 0) {
                printf("          START TIMER: starting timer at current time\n");
            }
            starttimer(A, RTT);
        }

        /* Update sequence number */
        A_nextseqnum = (A_nextseqnum + 1) % SEQSPACE;
    } else {
        if (TRACE > 0) {
            printf("----A: New message arrives, send window is full\n");
        }
        window_full++;
    }
}

void A_input(struct pkt packet) {
    if (!IsCorrupted(packet)) {
        if (TRACE > 0) {
            printf("----A: uncorrupted ACK %d received\n", packet.acknum);
        }
        total_ACKs_received++;
        
        /* Slide window forward to the ACK number */
        while (windowcount > 0 && 
               buffer[windowfirst].seqnum != packet.acknum) {
            windowfirst = (windowfirst + 1) % WINDOWSIZE;
            windowcount--;
        }
        
        /* If we found the packet, acknowledge it */
        if (windowcount > 0 && buffer[windowfirst].seqnum == packet.acknum) {
            windowfirst = (windowfirst + 1) % WINDOWSIZE;
            windowcount--;
        }
        
        /* Update timer */
        if (windowcount == 0) {
            if (TRACE > 0) {
                printf("          STOP TIMER: stopping timer at current time\n");
            }
            stoptimer(A);
        } else {
            starttimer(A, RTT);
        }
    } else {
        if (TRACE > 0) {
            printf("----A: corrupted ACK received\n");
        }
    }
}

void A_timerinterrupt() {
    if (TRACE > 0) {
        printf("----A: Timeout occurred, resending window...\n");
    }
    packets_resent += windowcount;
    for (int i = 0; i < windowcount; i++) {
        int index = (windowfirst + i) % WINDOWSIZE;
        tolayer3(A, buffer[index]);
    }
    starttimer(A, RTT);
}

void A_init() {
    windowfirst = 0;
    windowlast = -1;
    windowcount = 0;
    A_nextseqnum = 0;
}

/********* Receiver (B) Implementation *********/
static int B_expectedseqnum = 0;

void B_input(struct pkt packet) {
    struct pkt ackpkt;
    ackpkt.seqnum = NOTINUSE;

    if (!IsCorrupted(packet)) {
        if (packet.seqnum == B_expectedseqnum) {
            if (TRACE > 0) {
                printf("----B: uncorrupted packet %d received\n", packet.seqnum);
            }
            correct_packets_received++;
            tolayer5(B, packet.payload);
            messages_delivered++;
            ackpkt.acknum = B_expectedseqnum;
            B_expectedseqnum = (B_expectedseqnum + 1) % SEQSPACE;
        } else {
            if (TRACE > 0) {
                printf("----B: out-of-order packet %d received (expected %d)\n", 
                      packet.seqnum, B_expectedseqnum);
            }
            /* Send ACK for last correctly received packet */
            ackpkt.acknum = (B_expectedseqnum - 1 + SEQSPACE) % SEQSPACE;
        }
        ackpkt.checksum = ComputeChecksum(ackpkt);
        tolayer3(B, ackpkt);
    } else {
        if (TRACE > 0) {
            printf("----B: corrupted packet received\n");
        }
    }
}

void B_timerinterrupt() {
    /* Receiver doesn't need timer functionality */
}

void B_output(struct msg message) {
    /* Receiver doesn't generate messages */
}

void B_init() {
    B_expectedseqnum = 0;
}
