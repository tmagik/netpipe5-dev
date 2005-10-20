/*
  Define the protocol structure to be used by NetPIPE for MPI.

  $Id: MPI.h,v 1.2 1998/09/24 15:11:29 ghelmer Exp $
  */

typedef struct protocolstruct ProtocolStruct;
struct protocolstruct
{
	int nbor, iproc;
};

