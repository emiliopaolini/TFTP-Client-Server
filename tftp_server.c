#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include<sys/wait.h>


#define MAX_BUFFER_LENGTH 516
#define MAX_MODE_LENGTH 9
#define MAX_FILENAME_LENGTH 504
#define MAX_DIR_LENGTH 1024
#define BUFFERACK_LENGTH 4
#define ERROR_MSG_LENGTH 1024
#define BUFFER_APP_LENGTH 512

#define RRQ_OPCODE 1
#define DATA_OPCODE 3
#define ACK_OPCODE 4
#define ERROR_OPCODE 5
#define FILE_NOT_FOUND_CODE 1
#define ILLEGAL_FTP_OPERATION_CODE 4

#define BIN_MODE "octet\0"
#define TXT_MODE "netascii\0"






void usage(){
    printf("USAGE: ./tftp_server <porta> <directory files>\n");
}

void sendErrorPacket(struct sockaddr_in cl,uint16_t errNumber,int sd){
    //serializzazione msg di errore
    int pos = 0;
    char errorMsg[ERROR_MSG_LENGTH];
    uint16_t op_code = htons(ERROR_OPCODE);

    memcpy(errorMsg+pos,&op_code,sizeof(op_code));
    pos += sizeof(op_code);

    if(errNumber == FILE_NOT_FOUND_CODE){
        errNumber = htons(FILE_NOT_FOUND_CODE);
        memcpy(errorMsg+pos,&errNumber,sizeof(errNumber));
        pos += sizeof(errNumber);    
        strcpy(errorMsg+pos,"File Not Found.\0");
    }
    else{
        errNumber = htons(ILLEGAL_FTP_OPERATION_CODE);
        memcpy(errorMsg+pos,&errNumber,sizeof(errNumber));
        pos += sizeof(errNumber);
        strcpy(errorMsg+pos,"Illegal ftp operation.\0");
    }
        
    pos += strlen(errorMsg+pos)+1;
    
    sendto(sd, errorMsg,pos,0, (struct sockaddr*)&cl, sizeof(cl));
}

int sendMsg(uint16_t numeroBlocco,FILE* fptr,struct sockaddr_in cl,int sd){
    char bufferApp[BUFFER_APP_LENGTH];
    char bufferToSend[MAX_BUFFER_LENGTH];

    memset(bufferToSend,0,sizeof(bufferToSend));
    memset(bufferApp,0,sizeof(bufferApp));    

    int numByteLetti=fread(bufferApp,1,sizeof(bufferApp),fptr);
    int pos = 0;
    
    uint16_t op_code;
    
    

    printf("Buffer app: %s",bufferApp); 
    printf("Numero blocco: %d\n",numeroBlocco);
    printf("Byte Letti: %d\n",numByteLetti);

    
    numeroBlocco = htons(numeroBlocco);
    op_code = htons(DATA_OPCODE);
    
    memcpy(bufferToSend+pos,&op_code,sizeof(op_code));
    pos += sizeof(op_code);

    memcpy(bufferToSend+pos,&numeroBlocco,sizeof(numeroBlocco));
    pos += sizeof(numeroBlocco);
    
    strcpy(bufferToSend+pos,bufferApp);

    printf("messaggio mandato: %s",bufferToSend+pos);
    printf("Pacchetto di dimensioni: %d\n",numByteLetti+pos);
    
    return sendto(sd, bufferToSend,numByteLetti+pos, 0, (struct sockaddr*)&cl, sizeof(cl));
}


int main(int argc, char* argv[]){

    int socketDescriptor,sd;
    int ret,len,pos;
    int porta;
    int addrlen;    
    int numByteLetti;
    int byteMandati;

    pid_t pid;

    uint16_t op_code;
    uint16_t ackNumber;
	uint16_t numeroBlocco;
    
    struct sockaddr_in my_addr;
    struct sockaddr_in cl_addr;
    FILE *fptr;

    char directory[MAX_DIR_LENGTH];    
    char buffer[MAX_BUFFER_LENGTH];   
    char filename[MAX_FILENAME_LENGTH]; 
    char mode[MAX_MODE_LENGTH];    
    char temporary[MAX_DIR_LENGTH];
    char bufferAck[BUFFERACK_LENGTH];


    //Controllo num. argomenti passati
    if(argc != 3){
        usage();
        exit(1);
    }
    //Conversione porta e controllo eventuali errori
    porta = atoi(argv[1]);
    if(porta<0 || porta>65535){
        printf("Valore porta non valido.\nTermino esecuzione\n");
        exit(1);
    }
    
    strcpy(directory,argv[2]);
    
    addrlen = sizeof(cl_addr);
    
    //inizializzazione socket
    socketDescriptor = socket(AF_INET,SOCK_DGRAM,0);
    if(socketDescriptor < 0){
        perror("Errore creazione socket: ");
        exit(1);
    }    

    //inizializzazione struttura my_addr
    memset(&my_addr,0,sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(porta);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    
    //socket in ascolto
    ret = bind(socketDescriptor,(struct sockaddr*)&my_addr,sizeof(my_addr));
    if(ret < 0){
        perror("Bind non riuscita");
        exit(1);
    }

    printf("Server in ascolto sulla porta: %d\n",porta);
    printf("Server lavora sulla directory: %s\n",directory); 

    while(1){
        
        pos = 0;
        
        memset(&cl_addr,0,sizeof(cl_addr));
        
        //ricevo il pkt in buffer
        len = recvfrom(socketDescriptor,buffer,MAX_BUFFER_LENGTH,0,(struct sockaddr*)&cl_addr, &addrlen);
        
        //creazione processo figlio che gestirà la comunicazione con cl_addr
        pid = fork();
        
        if(pid == 0){
			//processo figlio
            printf("Creato nuovo figlio\n");
            close(socketDescriptor);

            //assegno una porta libera per la comunicazione con quel client
            my_addr.sin_port = 0;
            
            sd = socket(AF_INET,SOCK_DGRAM,0);            

            ret = bind(sd,(struct sockaddr*)&my_addr,sizeof(my_addr));
            if(ret < 0){
                perror("Errore bind figlio\n");
                exit(1);
            }

            numeroBlocco = 0;
            //de-serializzazione msg ricevuto
            pos = 0;
            memcpy(&op_code,buffer+pos,sizeof(op_code));
            pos += sizeof(op_code);        
            op_code = ntohs(op_code);
			

            if(op_code == RRQ_OPCODE){
                //creazione percorso file da mandare -> directory + filename ricevuto
                memset(temporary,0,sizeof(temporary));
                strcat(temporary,directory);
                strcat(temporary,"/");
                strcpy(filename,buffer+pos);
                strcat(temporary,filename);

                printf("nome file: %s\n",filename);
                printf("percorso file da mandare: %s\n",temporary);
                //printf("dim file: %d\n",strlen(filename));
                pos += strlen(filename)+1;

                strcpy(mode,buffer+pos);
                pos += strlen(mode)+1;            

                printf("OP_CODE Ricevuto: %d\nModalita' ricevuta: %s\n",op_code,mode);
                printf("Ricerca file %s\n",temporary);
            
                if(strcmp(mode,TXT_MODE)==0){
                    fptr = fopen(temporary,"r");
                }
                else {
                    fptr = fopen(temporary,"rb");             
                }
                if(fptr == NULL){
                    printf("File non esistente, invio pkt di errore\n");
                    sendErrorPacket(cl_addr,FILE_NOT_FOUND_CODE,sd);
					close(sd);
					exit(1);
                }     
                else{
                    
                    while(1){
		                pos = 0;
        
                        byteMandati = sendMsg(numeroBlocco,fptr,cl_addr,sd);

		                printf("Byte mandati: %d\n",byteMandati);
                        printf("Waiting for ACK..\n");

                        //attesa messaggio di ack
                        len = recvfrom(sd,bufferAck,BUFFERACK_LENGTH,0,(struct sockaddr*)&cl_addr, &addrlen);
                        
                        //de-serializzazione msg di ack
                        memcpy(&op_code,bufferAck+pos,sizeof(op_code));
                        op_code = ntohs(op_code);
                        pos += sizeof(op_code);

		                memcpy(&ackNumber,bufferAck+pos,sizeof(ackNumber));
                        ackNumber = ntohs(ackNumber);
                        pos += sizeof(ackNumber);   

                        if(op_code==ACK_OPCODE){
                            //ricevuto msg di ack
                            //controllo num. blocco con num.ack
                            printf("Numero di ack ricevuto: %d\n",ackNumber);
                            if(ackNumber == numeroBlocco)
                                numeroBlocco++;        
                        }          
            
		                if(byteMandati<MAX_BUFFER_LENGTH){
                            fclose(fptr);
							close(sd);
							exit(0);
                        }
                        
                    }
                
                }
            }
            else{ 
                sendErrorPacket(cl_addr,ILLEGAL_FTP_OPERATION_CODE,sd);
				close(sd); 
				exit(1);
            }
            
        }
    }
}
    
    
    
    
