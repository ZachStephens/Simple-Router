#include "ne.h"
#include  "router.h"
#include <sys/timerfd.h>

int open_listenfd(int);
void sockaddrPort(struct sockaddr_in *,int,char*);
//int open_writefd(char*,int);

int main(int argc, char* argv[]){

  fd_set rfds;



  int retval, max_fd;

  int router_id, ne_port, router_port;

  int  router_fd;

  int no_nbr;

  int nbr_costs[MAX_ROUTERS];
  int nbr_ids[MAX_ROUTERS];

  int sizeResponse;

  socklen_t  addr_len;
  struct sockaddr_in ne_addr;
  struct pkt_INIT_REQUEST ne_init;
  struct pkt_INIT_RESPONSE* Response;
  char filename[50];
  FILE * fptr;
  char buffer[PACKETSIZE];

   if(argc != 5 )
    {
      printf("router <routerid> <nehostname> <neUDPport> <routerUDPport>\n");
    }
  
  router_id = atoi(argv[1]);
  
  ne_port = atoi(argv[3]);
  router_port = atoi(argv[4]);

  //build filename
  strcpy(filename,"router");
  strcat(filename,argv[1]);
  strcat(filename,".log");

  //bind descriptor to socket
  router_fd =  open_listenfd(router_port);
  
  //connect to emulator
  sockaddrPort(&ne_addr,ne_port,argv[2]);

  //initialize packet
  ne_init.router_id = htonl(router_id);//

  int died = 0;
  //send initialize packet
  memcpy((void *) buffer, &ne_init, sizeof(ne_init));// 
  addr_len = sizeof(ne_addr);

  //send init request & receive response

  sendto(router_fd,buffer,sizeof(struct pkt_INIT_REQUEST),0,(struct sockaddr *)&ne_addr, addr_len);


  sizeResponse = recvfrom(router_fd,buffer,PACKETSIZE,0,(struct sockaddr *)&ne_addr,&addr_len);

  Response = (struct pkt_INIT_RESPONSE*) &buffer;
  ntoh_pkt_INIT_RESPONSE(Response);
  InitRoutingTbl(Response,router_id);
 
  int i;
  for(i=0;i<Response->no_nbr;i++){
    nbr_costs[Response->nbrcost[i].nbr] = Response->nbrcost[i].cost;
    nbr_ids[i] = Response->nbrcost[i].nbr;
  }
  

  no_nbr = Response->no_nbr;
 
  //1 timer per neighbor for failure detection
  // 1 timer for convergence
  // 1 timer for update interval

  int update_fd,  converge_fd, converge_flag;

  struct itimerspec update,  converge;
  struct itimerspec * nbr_tim;
  int * nbr_fd = NULL;
  int * deadflags = NULL;


  struct pkt_RT_UPDATE pkt_update;
  struct pkt_RT_UPDATE send_update;

  converge_flag = 0;


  update.it_value.tv_sec = UPDATE_INTERVAL;
  update.it_value.tv_nsec = 0; 
  update.it_interval.tv_sec = 0;
  update.it_interval.tv_nsec = 0;  
  update_fd = timerfd_create(1,0);


  converge.it_value.tv_sec = CONVERGE_TIMEOUT;
  converge.it_value.tv_nsec = 0; 
  converge.it_interval.tv_sec = 0;
  converge.it_interval.tv_nsec = 0;  
  converge_fd = timerfd_create(1,0);
  
  nbr_tim = malloc((MAX_ROUTERS)*(sizeof(struct itimerspec)));
  nbr_fd = malloc((MAX_ROUTERS)*(sizeof(int)));
  deadflags = malloc((MAX_ROUTERS)*(sizeof(int)));

  for(i = 0; i< MAX_ROUTERS; i++){
    if(i != router_id){
      nbr_tim[i].it_value.tv_sec = FAILURE_DETECTION;
      nbr_tim[i].it_value.tv_nsec = 0; 
      nbr_tim[i].it_interval.tv_sec = 0;
      nbr_tim[i].it_interval.tv_nsec = 0;  
      nbr_fd[i] = timerfd_create(1,0);
      deadflags[i] = 0;
      timerfd_settime(nbr_fd[i],0,&nbr_tim[i],NULL);
    }
  }
  //print initial table to log
  fptr = fopen(filename,"w");
  PrintRoutes(fptr,router_id);

  fflush(fptr);

  int time = 0;


  timerfd_settime(update_fd,0,&update,NULL);
  timerfd_settime(converge_fd,0,&converge,NULL);
  max_fd = update_fd;
  max_fd = (max_fd > converge_fd)?max_fd:converge_fd;
  max_fd = (max_fd > router_fd)?max_fd:router_fd;

  while(1){
    FD_ZERO(&rfds);
    FD_SET(update_fd, &rfds);

    if(!converge_flag){
      FD_SET(converge_fd, &rfds);      
    }
    
    FD_SET(router_fd, &rfds);
    
    for(i=0;i<MAX_ROUTERS;i++){
      if(i != router_id){
	if(!deadflags[i]){
	  FD_SET(nbr_fd[i],&rfds);
	  max_fd = (max_fd > (nbr_fd[i]))?max_fd:nbr_fd[i];
	}
      }
    }
    max_fd++;
    retval = select(max_fd, &rfds, NULL, NULL, NULL); 

    if(FD_ISSET(router_fd,&rfds)){  
      //receive packet and update routing table & reset timeout of sender 

      recvfrom(router_fd,(struct pkt_RT_UPDATE *) &pkt_update,sizeof(pkt_update),0,NULL,NULL);
      ntoh_pkt_RT_UPDATE(&pkt_update);
      if(UpdateRoutes(&pkt_update,nbr_costs[pkt_update.sender_id],router_id)){
	timerfd_settime(converge_fd,0,&converge,NULL);
	fprintf(fptr,"\n");
	PrintRoutes(fptr,router_id);	
	fflush(fptr);
	converge_flag = 0;
      }
      timerfd_settime(nbr_fd[pkt_update.sender_id],0,&nbr_tim[pkt_update.sender_id],NULL);
      deadflags[pkt_update.sender_id] = 0;
    }		
	
  
    if(FD_ISSET(update_fd,&rfds)){
      time++;
      //sending routing table to each neigbor & reset update timer
      bzero((char*) &send_update,sizeof(send_update));
      ConvertTabletoPkt(&send_update,router_id);
      for(i=0;i<no_nbr;i++){
	send_update.dest_id=nbr_ids[i];
	hton_pkt_RT_UPDATE(&send_update);
	if(!sendto(router_fd,(struct pkt_RT_UPDATE *)&send_update,sizeof(send_update),0,(struct sockaddr *)&ne_addr, sizeof(ne_addr)))
	  printf("failed to send\n");
	ntoh_pkt_RT_UPDATE(&send_update);
      }	
      timerfd_settime(update_fd,0,&update,NULL);
    }


    if(FD_ISSET(converge_fd,&rfds)){
      //read & update neighbor routing table
      //reset neighbor timer
      //PrintRoutes(fptr,router_id);
      fprintf(fptr,"%d:Converged\n",time);

      fflush(fptr);
      converge_flag = 1;	
    }
     
    for(i=0;i<MAX_ROUTERS;i++){
      if(FD_ISSET(nbr_fd[i],&rfds)){
	deadflags[i] = 1;

	died = 1;
	UninstallRoutesOnNbrDeath(i);	  
      }
    }
    if(died)
      {
	fprintf(fptr,"\n");
	PrintRoutes(fptr,router_id);	
	fflush(fptr);
	died = 0;
      }
          
  }
  fclose(fptr);
  free(nbr_tim);
  free(nbr_fd);
  free(deadflags);
  return 0;
}







  void sockaddrPort(struct sockaddr_in * ne_addr,int ne_port,char * hostname){
    struct hostent *hostptr;
    if((hostptr = gethostbyname(hostname))== NULL)
      {
	printf("Error: Could not get hostbyname\n");
	return;
      }

    bzero((char *) ne_addr, sizeof(*ne_addr));
    ne_addr->sin_family = AF_INET;
    bcopy((char *)hostptr->h_addr,(char *)&ne_addr->sin_addr.s_addr, hostptr->h_length); 
    ne_addr->sin_port = htons((unsigned short)ne_port);
  }

  //obtains UDP descriptor
int open_listenfd(int port)
{
  int listenfd =-1; 
  int optval=1;
  struct sockaddr_in serveraddr;
  if ((listenfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    return -1;

  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
		 (const void *)&optval , sizeof(int)) < 0)
    return -1;
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)port);
  if (bind(listenfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
    return -1;
  return listenfd;
} 

