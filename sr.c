#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "sr.h"

#define RTT  16.0
#define WINDOWSIZE 6
#define SEQSPACE 7
#define NOTINUSE (-1)

/* Global counters */
int total_ACKs_received = 0;
int window_full = 0;
int packets_resent = 0;
int correct_packets_received = 0;

int ComputeChecksum(struct pkt packet) {
  int checksum = packet.seqnum + packet.acknum;
  for (int i = 0; i < 20; i++)
    checksum += (int)packet.payload[i];
  return checksum;
}

bool IsCorrupted(struct pkt packet) {
  return packet.checksum != ComputeChecksum(packet);
}

/********* Sender (A) ************/
static struct pkt buffer[WINDOWSIZE];
static int windowfirst = 0, windowlast = -1;
static int windowcount = 0;
static int A_nextseqnum = 0;

void A_output(struct msg message) {
  if (windowcount < WINDOWSIZE) {
    struct pkt sendpkt;
    sendpkt.seqnum = A_nextseqnum;
    sendpkt.acknum = NOTINUSE;
    for (int i = 0; i < 20; i++)
      sendpkt.payload[i] = message.data[i];
    sendpkt.checksum = ComputeChecksum(sendpkt);

    windowlast = (windowlast + 1) % WINDOWSIZE;
    buffer[windowlast] = sendpkt;
    windowcount++;

    tolayer3(A, sendpkt);
    if (windowcount == 1) starttimer(A, RTT);
    
    A_nextseqnum = (A_nextseqnum + 1) % SEQSPACE;
  } else {
    window_full++;
  }
}

void A_input(struct pkt packet) {
  if (!IsCorrupted(packet)) {
    total_ACKs_received++;
    
    if (windowcount > 0) {
      int acked = 0;
      while (windowcount > 0 && 
             buffer[windowfirst].seqnum != packet.acknum) {
        windowfirst = (windowfirst + 1) % WINDOWSIZE;
        windowcount--;
      }
      
      if (windowcount > 0 && buffer[windowfirst].seqnum == packet.acknum) {
        windowfirst = (windowfirst + 1) % WINDOWSIZE;
        windowcount--;
      }
      
      if (windowcount == 0) stoptimer(A);
      else starttimer(A, RTT);
    }
  }
}

void A_timerinterrupt() {
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

/********* Receiver (B) ************/
static int B_expectedseqnum = 0;

void B_input(struct pkt packet) {
  struct pkt ackpkt;
  ackpkt.seqnum = NOTINUSE;

  if (!IsCorrupted(packet)) {
    if (packet.seqnum == B_expectedseqnum) {
      correct_packets_received++;
      tolayer5(B, packet.payload);
      ackpkt.acknum = B_expectedseqnum;
      B_expectedseqnum = (B_expectedseqnum + 1) % SEQSPACE;
    } else {
      ackpkt.acknum = (B_expectedseqnum - 1 + SEQSPACE) % SEQSPACE;
    }
    ackpkt.checksum = ComputeChecksum(ackpkt);
    tolayer3(B, ackpkt);
  }
}

void B_timerinterrupt() {}
void B_output(struct msg message) {}
void B_init() { B_expectedseqnum = 0; }
