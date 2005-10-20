/*
  Define the protocol structure to be used by NetPIPE for TCP.

  $Id: TCP.h,v 1.3 1998/09/24 15:17:44 ghelmer Exp $
  */

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

typedef struct protocolstruct ProtocolStruct;
struct protocolstruct
{
    struct sockaddr_in      sin1,   /* socket structure #1              */
                            sin2;   /* socket structure #2              */
    int                     nodelay;  /* Flag for TCP nodelay           */
    struct hostent          *addr;    /* Address of host                */
    int                     sndbufsz, /* Size of TCP send buffer        */
                            rcvbufsz; /* Size of TCP receive buffer     */
};

