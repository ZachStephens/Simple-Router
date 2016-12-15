#include "ne.h"
#include  "router.h"






struct route_entry routingTable[MAX_ROUTERS];

int NumRoutes;

void InitRoutingTbl (struct pkt_INIT_RESPONSE *InitResponse, int myID){

  int i;
  int j;
  NumRoutes = 1 + InitResponse->no_nbr;

  //printf("no_nbr %d\n",NumRoutes);

  routingTable[0].dest_id = myID;
  routingTable[0].next_hop = myID;
  routingTable[0].cost = 0;


  j=1;
  for(i=0;i < (InitResponse->no_nbr);i++){
    // printf("dest id: %d \n",InitResponse->nbrcost[i-1].nbr);
    if(InitResponse->nbrcost[i].nbr == myID){
      continue;
    }
    routingTable[j].dest_id = InitResponse->nbrcost[i].nbr;
    routingTable[j].next_hop = InitResponse->nbrcost[i].nbr;
    routingTable[j].cost = InitResponse->nbrcost[i].cost;
    j++;
  }

}


int findEntry(int dest_id){
  int i;
  for(i=0;i<NumRoutes;i++){
    if(routingTable[i].dest_id == dest_id)
      return i;
  }
  return -1;
}

int getCost(int dest_id){
  int i;
  for(i=0;i<NumRoutes;i++){
    if(routingTable[i].dest_id == dest_id)
      return routingTable[i].cost;
  }
  return -1;
}


int UpdateRoutes(struct pkt_RT_UPDATE *RecvdUpdatePacket, int costToNbr, int myID){

  int i, index;
  int distance;

  int change = 0;

  for(i=0;i<RecvdUpdatePacket->no_routes;i++){
    index = findEntry(RecvdUpdatePacket->route[i].dest_id);
    distance = costToNbr +  RecvdUpdatePacket->route[i].cost;

    // printf("dest_id: %d; index: %d\n",RecvdUpdatePacket->route[i].dest_id,index );

    // printf("distance: %d; nexthop: %d\n",distance, RecvdUpdatePacket->sender_id);
    if(((routingTable[index].cost >= INFINITY) && (distance >= INFINITY)) || (RecvdUpdatePacket->route[i].dest_id==myID))
      continue;

    if((distance< 0) || (distance>INFINITY))
      distance = INFINITY;

    if(index == -1){
      //add
      routingTable[NumRoutes].cost = distance;
      routingTable[NumRoutes].next_hop = RecvdUpdatePacket->sender_id;
      routingTable[NumRoutes].dest_id =RecvdUpdatePacket->route[i].dest_id; 
      NumRoutes++;
      // printf("first & distance:%d\n costtonbr: %d\n rcv rout cost: %d\n",distance,costToNbr,RecvdUpdatePacket->route[i].cost);
      change  = 1;
    }
    else if(((routingTable[index].next_hop == RecvdUpdatePacket->sender_id)&&(routingTable[index].cost != distance))
	    || ((distance < routingTable[index].cost) 
		&& (RecvdUpdatePacket->route[i].next_hop != myID))){
      
      //   printf("elseif \n");
      routingTable[index].cost = distance;
      change = 1;
      routingTable[index].next_hop = RecvdUpdatePacket->sender_id;
	
    }
    
      
  }
  /*
  routingTable[0].dest_id = myID;
  routingTable[0].next_hop = myID;
  routingTable[0].cost = 0;*/
  return change;
}





void ConvertTabletoPkt(struct pkt_RT_UPDATE *UpdatePacketToSend, int myID){
  int i;
  UpdatePacketToSend->sender_id=myID;
  UpdatePacketToSend->no_routes=NumRoutes;
  for(i=0;i<NumRoutes;i++){
    UpdatePacketToSend->route[i].dest_id = routingTable[i].dest_id;
    UpdatePacketToSend->route[i].next_hop=routingTable[i].next_hop;
    UpdatePacketToSend->route[i].cost=routingTable[i].cost;
    //fprintf(Logfile,"R%d -> R%d: R%d, %d\n",myID,routingTable[i].dest_id,routingTable[i].next_hop,routingTable[i].cost);
  }

}


void PrintRoutes (FILE* Logfile, int myID){

  int i;
  //printf("Routing Table(%d):\n",myID);
  fprintf(Logfile,"Routing Table:\n");
  for(i=0;i<NumRoutes;i++){
    // printf("R%d -> R%d: R%d, %d\n",myID,routingTable[i].dest_id,routingTable[i].next_hop,routingTable[i].cost);
    fprintf(Logfile,"R%d -> R%d: R%d, %d\n",myID,routingTable[i].dest_id,routingTable[i].next_hop,routingTable[i].cost);
  }
  
  
}

void UninstallRoutesOnNbrDeath(int DeadNbr){
  int i;
  for(i=0;i<NumRoutes;i++){
    if(routingTable[i].next_hop == DeadNbr)
      routingTable[i].cost = INFINITY;
  }
}
