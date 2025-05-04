#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "gbn.h"

/* ******************************************************************
   Go Back N protocol.  Adapted from J.F.Kurose
   ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.2

   Modifications:
   - Removed bidirectional GBN code and other unused code.
   - Fixed C style to adhere to current programming standards.
   - Added GoBackN implementation for A and B.
**********************************************************************/

#define RTT  16.0       /* round trip time.  MUST BE SET TO 16.0 when submitting assignment */
#define WINDOWSIZE 6    /* the maximum number of buffered unacked packets, must be 6 */
#define SEQSPACE 7      /* the minimum sequence space for GBN must be at least windowsize + 1 */
#define NOTINUSE (-1)   /* used to fill header fields that are not being used */

/* Compute checksum for packet validation */
int ComputeChecksum(struct pkt packet)
{
  int checksum = 0;
  int i;

  checksum = packet.seqnum;
  checksum += packet.acknum;
  for (i = 0; i < 20; i++)
    checksum += (int)(packet.payload[i]);

  return checksum;
}

/* Check if the packet is corrupted by comparing its checksum */
bool IsCorrupted(struct pkt packet)
{
  if (packet.checksum == ComputeChecksum(packet))
    return false;
  else
    return true;
}

/********* Sender (A) variables and functions ************/

static struct pkt buffer[WINDOWSIZE];  /* array for storing packets waiting for ACK */
static int windowfirst, windowlast;    /* array indexes of the first/last packet awaiting ACK */
static int windowcount;                /* the number of packets currently awaiting an ACK */
static int A_nextseqnum;               /* the next sequence number to be used by the sender */

/* Function to send a message from A to B */
void A_output(struct msg message)
{
  struct pkt sendpkt;
  int i;

  /* If not blocked, send the packet */
  if (windowcount < WINDOWSIZE) {
    if (TRACE > 1)
      printf("----A: New message arrives, send window is not full, sending new message to layer3!\n");

    /* Create and prepare packet */
    sendpkt.seqnum = A_nextseqnum;
    sendpkt.acknum = NOTINUSE;
    for (i = 0; i < 20; i++)
      sendpkt.payload[i] = message.data[i];
    sendpkt.checksum = ComputeChecksum(sendpkt);

    /* Place packet in window buffer */
    windowlast = (windowlast + 1) % WINDOWSIZE;
    buffer[windowlast] = sendpkt;
    windowcount++;

    /* Send out packet */
    if (TRACE > 0)
      printf("Sending packet %d to layer 3\n", sendpkt.seqnum);
    tolayer3(A, sendpkt);

    /* Start timer if it's the first packet in the window */
    if (windowcount == 1)
      starttimer(A, RTT);

    /* Update sequence number for the next packet */
    A_nextseqnum = (A_nextseqnum + 1) % SEQSPACE;
  } else {
    if (TRACE > 0)
      printf("----A: New message arrives, send window is full\n");
    window_full++;  // Optional handling for full window
  }
}

/* Function to handle incoming ACK packets at A */
void A_input(struct pkt packet)
{
  int ackcount = 0;
  int i;

  /* If received ACK is not corrupted */
  if (!IsCorrupted(packet)) {
    if (TRACE > 0)
      printf("----A: uncorrupted ACK %d received\n", packet.acknum);
    total_ACKs_received++;

    /* Check if ACK is new or duplicate */
    if (windowfirst <= windowlast && windowcount > 0) {
      /* Window is sliding, send the next packet */
      windowfirst = (windowfirst + 1) % WINDOWSIZE;
      windowcount--;
      if (windowcount > 0)
        starttimer(A, RTT);  // Restart the timer
    }
  } else {
    if (TRACE > 0)
      printf("----A: corrupted ACK %d received\n", packet.acknum);
  }
}

/* Function to handle timeouts at A */
void A_timerinterrupt()
{
  int i;

  if (TRACE > 0)
    printf("----A: Timeout occurred, resending packet...\n");

  /* Resend all the packets in the window */
  for (i = windowfirst; i != windowlast; i = (i + 1) % WINDOWSIZE) {
    tolayer3(A, buffer[i]);
  }

  /* Restart the timer */
  starttimer(A, RTT);
}

/* Function to handle incoming packets at A */
void A_init()
{
  windowfirst = 0;
  windowlast = -1;
  windowcount = 0;
  A_nextseqnum = 0;
}

/********* Receiver (B) variables and functions ************/

static int B_lastack;  /* the last ACK number received */

void B_output(struct msg message)
{
  /* Receiver does not send anything */
}

void B_input(struct pkt packet)
{
  /* If packet is uncorrupted and its sequence number is the next expected */
  if (!IsCorrupted(packet) && packet.seqnum == B_lastack + 1) {
    if (TRACE > 0)
      printf("----B: uncorrupted packet %d received\n", packet.seqnum);

    /* Send ACK for the received packet */
    B_lastack = packet.seqnum;
    struct pkt ackpkt;
    ackpkt.seqnum = NOTINUSE;
    ackpkt.acknum = B_lastack;
    ackpkt.checksum = ComputeChecksum(ackpkt);
    tolayer3(B, ackpkt);
  } else {
    if (TRACE > 0)
      printf("----B: corrupted or out-of-order packet %d received\n", packet.seqnum);
  }
}

void B_init()
{
  B_lastack = -1;
}

