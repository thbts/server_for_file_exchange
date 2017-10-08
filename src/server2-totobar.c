#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>

#define RCVSIZE 1494
#define MAX_CLIENTS 100


typedef struct
   {
	struct sockaddr_in addr;
	struct sockaddr_in client;
	int descSock;
	int port;
   }descConnexion;

typedef struct {
	unsigned int seqNumber;
	char buffer [RCVSIZE];
} segment;

int handshakeServer(int descSL, int newPort, int pos);
void closingRoutine();
void *connectionFileSend(void *arg);
void serialize_charArray (char* res, char* src);
void serialize_int(char* res, unsigned int a);
void serialize(char* buf, segment toSend);
void *readSocket(void *arg);

int descSL;
socklen_t alen;
descConnexion descData[MAX_CLIENTS];
int window,timeout;
int seqMax[MAX_CLIENTS];
int lecture[MAX_CLIENTS];
struct timeval debut,fin;
double SRTT;
int arrete[MAX_CLIENTS], maxAckReceived[MAX_CLIENTS];

int main(int argc, char **argv)
{
	srand(time(NULL));
	signal(SIGINT, closingRoutine);
	struct sockaddr_in adresse;
    //struct sockaddr_in client;
    int port;
    int maxSock=0;
	int i;
	for(i=0;i<MAX_CLIENTS;i++){
		descData[i].descSock=0;
		memset((char*)&descData[i].client,0,sizeof(adresse));
	}
	if(argc > 1){
		port = atoi(argv[1]);
	}
	else exit(0);
	int valid= 1;
	alen= sizeof(adresse);
	//char buffer[RCVSIZE];
	//thread array
	pthread_t threadArray[MAX_CLIENTS];
	//create socket
	descSL= socket(AF_INET, SOCK_DGRAM, 0);
    maxSock=descSL;
	// handle error
	if (descSL < 0) {
		perror("cannot create socket\n");
		return -1;
	}
	setsockopt(descSL, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof(int));
	fd_set readfds;
	adresse.sin_family= AF_INET;
	adresse.sin_port= htons(port);
	adresse.sin_addr.s_addr= htonl(INADDR_ANY);

	if (bind(descSL, (struct sockaddr*) &adresse, sizeof(adresse))== -1){
		perror("Bind UDP fail\n");
		close(descSL);
		return -1;
	}

	window=50;
	printf("Window=%d\n",window);


	int k;
	for(k=0;k<MAX_CLIENTS;k++){
		arrete[k]=0;
		maxAckReceived[k]=0;
		lecture[k]=0;
    seqMax[k]=0;
	}

	while(1){
		FD_ZERO(&readfds);
		FD_SET(descSL,&readfds);
        select(descSL + 1, &readfds, NULL, NULL, NULL);
        // Listen socket
        if(FD_ISSET(descSL,&readfds)){
           //handshake
           //choose port
           //Recherche de la premiere case vide du tableau
            int pos=0;
            for(i = 0; i < MAX_CLIENTS; i++){
                if(descData[i].descSock == 0){
                    pos=i;
                    break;
                }
            }
            int newIntPort;
            newIntPort = port + maxSock;
            maxSock++;
            printf("New Port : %d\n",newIntPort);
            int valide = handshakeServer(descSL, newIntPort, pos);

            if(valide == -1){
                printf("handshake failed\n");
                close(descSL);
                exit(0);
            }
            maxSock++;
            pthread_create(&threadArray[pos], NULL, connectionFileSend, &pos);
            pthread_create(&threadArray[pos+1], NULL, readSocket, &pos);

        }
	}
	close(descSL);
	return 0;
}


int handshakeServer(int descSL, int newPort, int pos){
	char buffer[RCVSIZE];
    memset(buffer,'\0',RCVSIZE);
    if(recvfrom(descSL, buffer, RCVSIZE, 0, (struct sockaddr*)&descData[pos].client, &alen) > 0){
        if(strcmp(buffer,"SYN") == 0){
	        // Received SYN
	        //printf("Received SYN, Sending SYN-ACK\n");



			int newDesc = socket(AF_INET, SOCK_DGRAM, 0);
			if (descSL < 0) {
				perror("cannot create socket\n");
				return -1;
			}
			int valid=1;
			//sendto(descSL,portString,strlen(portString), 0, (struct sockaddr*)&descData[pos].client, sizeof(descData[pos].client));
			setsockopt(newDesc, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof(int));

			struct sockaddr_in addr;
			memset((char*)&addr,0,sizeof(addr));
			addr.sin_port = htons(newPort);
			addr.sin_family = AF_INET;
			addr.sin_addr.s_addr = htonl(INADDR_ANY);
			//addr.sin_addr.s_addr = inet_addr("127.0.0.2");

			descData[pos].port=newPort;

			if (bind(newDesc, (struct sockaddr*) &addr, alen) == -1){
	            perror("Bind UDP fail\n");
	            close(newDesc);
	            return -1;
            }
			descData[pos].descSock=newDesc;

	        char synAck[11];
	        sprintf(synAck,"SYN-ACK%d",newPort);
	        sendto(descSL,synAck,11*sizeof(char), 0, (struct sockaddr*) &descData[pos].client, sizeof(descData[pos].client));

	        //Receiving ACK
	        memset(buffer,'\0',RCVSIZE);
	        recvfrom(descSL, buffer,RCVSIZE, 0, (struct sockaddr*) &descData[pos].client, &alen);
	        if(strcmp(buffer,"ACK") == 0){
		        //printf("Received ACK, handshake completed\n");
                return 0;
	        }
            else {
		        fprintf(stderr," Received %s instead of \"ACK\" handshake failed\n", buffer);
		        return -1;
	        }
        }
        else return -1;
    }
    else return -1;
}


void *connectionFileSend(void *arg){
	int i;
	int connectionNumber = *(int*)arg;
	int sockData=descData[connectionNumber].descSock;
	fd_set readData;
	//printf("Identifiant socket %d\n",sockData);


	char fichier[20];
	memset(fichier,'\0',20);
	//printf("Attends requête fichier\n");
	int msgSize = recvfrom(sockData, fichier,20, 0, (struct sockaddr*)&descData[connectionNumber].client, &alen);
	//printf("Le client cherche le fichier %s, taille chaîne %d\n",fichier,strlen(fichier));
	lecture[connectionNumber]=1;

	segment toSend;
	toSend.seqNumber=1; //premier num de seq aléatoire
	char buf [RCVSIZE+6];

	//printf("Nouveau thread file %d\n",connectionNumber);
	FILE *input;
	input = fopen(fichier,"rb");
	if(input == 0){
		perror("Couldn't open file");
		exit(0);
	}

	fseek(input, 0, SEEK_END);
	int fileLen=ftell(input);
	fseek(input, 0, SEEK_SET);

	int seqParBuff=67100;
	int tailleBuff=RCVSIZE*seqParBuff;
	int nbBuff=0;
	char* imgChar=(char *)malloc(tailleBuff+1);
	//printf("Taille %d\n",fileLen);

	i=1;
	seqMax[connectionNumber]=fileLen/RCVSIZE+1;
	int allAckReceived=1;
	int t2,t1;
	struct timeval debutThread;
	struct timeval finThread;
	struct timeval read_timeout;
	SRTT=timeout/1000000;
	read_timeout.tv_sec = SRTT/1000000;
	read_timeout.tv_usec = (int)SRTT%1000000;

	gettimeofday(&debutThread,NULL);
	gettimeofday(&debut,NULL);
	int debutMaxAckReceived=0,finMaxAckReceived=0;
	int nbSeq=0;
	long diff=0;

	while(finMaxAckReceived<seqMax[connectionNumber]){
		fread(imgChar, tailleBuff, 1, input);
		//printf("Passe a buff %d, seq %d\n",nbBuff,i);
		while(finMaxAckReceived!=(nbBuff+1)*seqParBuff){
			int finWhile=0;
			debutMaxAckReceived=finMaxAckReceived;
			int k;
			//printf("Envoi de window = %d paquets\n",window);
			//printf("SRTT de %d\n",SRTT);
			//printf("MaxAck : %d\n",maxAckReceived);
			for(k=1;k<=window;k++){
				//printf("segment %d de la window\n",k);
				if(maxAckReceived[connectionNumber]>i){
					i=maxAckReceived[connectionNumber]+1;
				}
				if(i>seqMax[connectionNumber] || i==(nbBuff+1)*seqParBuff+1){		//On arrive à la fin du fichier
					//printf("Fin du fichier %d\n",i);
					break;
				} else if (i==seqMax[connectionNumber]) {
					toSend.seqNumber=i%999999;
					//printf("Envoi de la derniere seq num : %u\n", toSend.seqNumber);
					memset(toSend.buffer,'\0',RCVSIZE);

					int sizeToSend=fileLen%RCVSIZE;

					//printf("Reste %d, seq a envoyer %d\n",sizeToSend,i);
					memset(toSend.buffer,'\0',RCVSIZE);
					int j;
					for(j=0;j<sizeToSend;j++){
						toSend.buffer[j]=imgChar[(i-1)*RCVSIZE+j-(seqParBuff)*RCVSIZE*nbBuff];
					}
					toSend.seqNumber=i;
					serialize(buf,toSend);
					sendto(sockData,buf,sizeToSend+6, 0, (struct sockaddr*)&descData[connectionNumber].client,sizeof(descData[connectionNumber].client));
				} else {
					toSend.seqNumber=i%999999;
					//printf("Envoi de seq num : %u\n", toSend.seqNumber);
					memset(toSend.buffer,'\0',RCVSIZE);
					//printf("Demarre tab a %d\n",(i-1)*RCVSIZE-nbBuff*(seqParBuff-1)*RCVSIZE);
					int j;
					for(j=0;j<RCVSIZE;j++){
						if((i-1)*RCVSIZE+j<fileLen)
							toSend.buffer[j]=imgChar[(i-1)*RCVSIZE+j-(seqParBuff)*RCVSIZE*nbBuff];
						else
							printf("Ne devrait pas être la \n");
					}
					//printf("taille buff %d et dernier indice %d\n",tailleBuff,(i-1)*RCVSIZE+RCVSIZE-1-(seqParBuff)*RCVSIZE*nbBuff);
					serialize(buf,toSend);
					if(k==1){
						gettimeofday(&debut,NULL);
					}
					sendto(sockData,buf,sizeof(buf), 0, (struct sockaddr*)&descData[connectionNumber].client,sizeof(descData[connectionNumber].client));
					usleep(50);
					i++;
				}
			}
      usleep(2000);
			finMaxAckReceived=maxAckReceived[connectionNumber];
			nbSeq++;
			diff+=(finMaxAckReceived-debutMaxAckReceived);
			//printf("Passe de %d a %d, soit diff de %d sum %d et nb %d\n",debutMaxAckReceived,finMaxAckReceived,(finMaxAckReceived-debutMaxAckReceived),diff,nbSeq);
			if(finMaxAckReceived==seqMax[connectionNumber] || finMaxAckReceived==(nbBuff+1)*seqParBuff)
				break;
			i=maxAckReceived[connectionNumber]+1;
		}
		if(maxAckReceived[connectionNumber]==seqMax[connectionNumber])
			break;
		nbBuff++;
	}
	usleep(40000);
	int k;
	for(k=1;k<=10;k++){
		sendto(sockData,"FIN",sizeof("FIN"), 0, (struct sockaddr*)&descData[connectionNumber].client,sizeof(descData[connectionNumber].client));
	}
	free(imgChar);
	fclose(input);
	//printf("'FIN' envoyé\n");
	gettimeofday(&finThread,NULL);
	t1=debutThread.tv_sec*1000000+debutThread.tv_usec;
	t2=finThread.tv_sec*1000000+finThread.tv_usec;
	//printf("Temps ecoule %d\n",(t2-t1));
	long moyenneDiff=diff/(long)nbSeq;
	printf("Debit: %f avec moyenne diff %d\n",((double)fileLen/(double)(t2-t1)),moyenneDiff);

	return NULL;
}


void *readSocket(void *arg){
	int connectionNumber = *(int*)arg;
	int sockData=descData[connectionNumber].descSock;
  int thisSeqMax;
	//printf("Identifiant socket %d\n",sockData);
	int lastMax=0;
	char* ack=(char *)malloc(9);
	//printf("Thread lecture, %d\n",connectionNumber);
	while(lecture[connectionNumber]==0){
		//Do nothing
	}
	int i=0;

	while(1==1){
		//printf("Attends message \n");
		int msgSize = recvfrom(sockData, ack,9, 0, (struct sockaddr*)&descData[connectionNumber].client, &alen);

		//window++;
		//printf("ACK reçu: %s\n",ack);
		if(i==0){
      thisSeqMax=seqMax[connectionNumber];
      gettimeofday(&fin,NULL);
			double t1=debut.tv_sec+debut.tv_usec/1000000;
			double t2=fin.tv_sec+fin.tv_usec/1000000;
			SRTT=(double)(fin.tv_sec-debut.tv_sec);
			SRTT+=(fin.tv_usec-debut.tv_usec);
			SRTT/=1000000;
			//printf("SRTT de %f car %d %d  et %d %d\n",SRTT,debut.tv_sec,debut.tv_usec,fin.tv_sec,fin.tv_usec);
			i++;
		}
		int ackNum=0;
		ack[9]='\0';
		if(ack+3!=NULL)
			ackNum=atoi((ack+3));
		//printf("Après Atoi\n");
		memset(ack,'0',9);
		if(ackNum>maxAckReceived[connectionNumber]){
			lastMax=maxAckReceived[connectionNumber];
			maxAckReceived[connectionNumber]=ackNum;
		}
		if(ackNum==thisSeqMax){
      maxAckReceived[connectionNumber]=ackNum;
      break;
		}
	}
	//printf("fin fonction car %d\n",fileLen);
	free(ack);
	return NULL;
}

void closingRoutine(){
    close(descSL);
    exit(0);
}

void serialize(char* buf, segment toSend){
	serialize_int(buf,toSend.seqNumber);
	serialize_charArray(buf,toSend.buffer);
	return;
}

void serialize_int(char* res, unsigned int a){
	char intermediaire[6];
	memset(intermediaire,'0',6);
	sprintf(intermediaire,"%d",a);
	int j;
	for(j=0;j<6;j++)
		res[j]=intermediaire[j];
	return;

}

void serialize_charArray (char* buf, char* segBuf){
	int i;
	for(i=6;i<RCVSIZE+6;i++){
		buf[i]=segBuf[i-6];
	}
	return;
}
