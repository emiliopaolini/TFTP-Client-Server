
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#define REQUEST_OPCODE 1
#define DATA_OPCODE 3
#define ACK_OPCODE 4
#define ERROR_OPCODE 5

#define IP_SERVER_LENGTH 16
#define MAX_BUFFER_LENGTH 516
#define MAX_MODE_LENGTH 9
#define MAX_FILENAME_LENGTH 504
#define BUFFERACK_LENGTH 4
#define MAX_CMD_LENGTH 510
#define MAX_MSG_LENGTH 512

#define BIN_MODE "octet\0"
#define TXT_MODE "netascii\0"




void usage(){
    printf("USAGE: ./tftp_client <IP server> <porta server>\n");
}

void showCommands(){
    printf("Comandi disponibili: \n");
    printf("!help --> mostra l'elenco dei comandi disponibili\n");
	printf("!mode {txt|bin} --> imposta il modo di trasferimento dei files(testo o binario)\n");
	printf("!get filename nome_locale --> richiede al server il nome del file <filename> e lo salva localmente con il nome <nome_locale>\n");
	printf("!quit --> termina il client\n");
}

int sendRequest(char mode[MAX_MODE_LENGTH],char filename[MAX_FILENAME_LENGTH],int sd,struct sockaddr_in sv_addr){
    
    char buffer[MAX_BUFFER_LENGTH];
    int pos=0;
    uint16_t op_code = htons(REQUEST_OPCODE);

    memcpy(buffer+pos,&op_code,sizeof(op_code));
    pos += sizeof(op_code);
   
    strcpy(buffer+pos,filename);
    
    pos += strlen(filename)+1;
    
    strcpy(buffer+pos,mode);
    pos += strlen(mode)+1;
    
    sendto(sd, buffer, pos, 0, (struct sockaddr*)&sv_addr, sizeof(sv_addr));
}

void sendAck(uint16_t ackNumber,int sd,struct sockaddr_in sv_addr){

    char msgAck[BUFFERACK_LENGTH];
    int pos = 0;
    uint16_t op_code = htons(ACK_OPCODE);

    memcpy(msgAck+pos,&op_code,sizeof(op_code));
    pos += sizeof(op_code);

    memcpy(msgAck+pos,&ackNumber,sizeof(ackNumber));
    pos += sizeof(ackNumber);

    sendto(sd, msgAck, pos, 0, (struct sockaddr*)&sv_addr, sizeof(sv_addr));
}

int validIP(char* ip){
	char delim[]= ".";
	char *ptr = strtok(ip, delim);
    int count = 0;
    int numIter = 0;
	while(ptr != NULL)
	{
        numIter ++;
        int valore = atoi(ptr);
        if(valore >=0 && valore <= 255){
            count++;
        }
		ptr = strtok(NULL, delim);
	}
    if (count == 4 && numIter == 4)
        return 1;
    return 0;
}


int main(int argc, char* argv[]){

    int socketDescriptor;
    int ret,len;
    int sv_addr_length;
    int pos;
    int numByteScritti;

    uint16_t num;
    uint16_t ackNumber;
	uint16_t op_code;

    struct sockaddr_in server_addr;
	struct sockaddr_in temp;

    FILE *fptr;
	
    char ip_server[IP_SERVER_LENGTH];
	char mode[MAX_MODE_LENGTH];
    char filename[MAX_FILENAME_LENGTH];
    char cmd[MAX_CMD_LENGTH];
    char msg[MAX_MSG_LENGTH];
    char bufferToReceive[MAX_BUFFER_LENGTH];    
    char fileDaCreare[MAX_FILENAME_LENGTH];
    char nomeFileRichiesto[MAX_FILENAME_LENGTH];
    
    //Controllo num. argomenti passati
    if(argc != 3){
        usage();
        exit(1);
    }
    //Conversione porta e controllo eventuali errori
    int porta = atoi(argv[2]);
    if(porta<0 || porta>65535){
        printf("Valore porta non valido.\nTermino esecuzione\n");
        exit(1);
    }

    strcpy(ip_server,argv[1]);
    if(!validIP(argv[1])){
        printf("IP server non valido.\nTermino esecuzione\n");
        exit(1);
    }
    
    //modalita binaria di default
	strcpy(mode,BIN_MODE); 
    
    //inizializzazione struttura server_addr
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(porta);
    inet_pton(AF_INET,ip_server,&server_addr.sin_addr);
    sv_addr_length = sizeof(server_addr);


    printf("Client prova a connettersi a <%s,%d>\n",ip_server,porta); 
    showCommands();    

	 
	socketDescriptor = socket(AF_INET,SOCK_DGRAM,0);
    if(socketDescriptor < 0){
        perror("Errore creazione socket: ");
        exit(1);
    }
    
		
	
	
	

	while(1){
		        
		scanf("%s",cmd);
    
		printf("Comando inserito %s\n",cmd);
    
		if(strcmp(cmd,"!help\n")==0){
			showCommands();
		}
		else if(strcmp(cmd,"!mode")==0){
				
            scanf("%s",mode);
            fseek(stdin,0,SEEK_END);
            if(strcmp(mode,"txt")==0){
			    strcpy(mode,TXT_MODE);
			    printf("Modo di trasferimento testuale configurato\n");
            }
            else if(strcmp(mode,"bin")==0){
                strcpy(mode,BIN_MODE);
			    printf("Modo di trasferimento binario configurato\n");
            } 
            else{
                printf("Modo di trasferimento non riconosciuto\n");
            }
		}
		else if(strcmp(cmd,"!get")==0){
			server_addr.sin_port = htons(porta);

            scanf("%s",nomeFileRichiesto);
            scanf("%s",fileDaCreare);

            printf("Nome file: %s\n",nomeFileRichiesto);
            printf("File da creare: %s\n",fileDaCreare);

			if(strcmp(mode,TXT_MODE)==0)
    			fptr = fopen(fileDaCreare,"w");
			else
    			fptr = fopen(fileDaCreare,"wb");
			if(fptr == NULL){
				printf("Errore creazione file.\n");
				continue;
			}					
	
			
			
            sendRequest(mode,nomeFileRichiesto,socketDescriptor,server_addr);

			memset(fileDaCreare,0,sizeof(fileDaCreare));
			memset(nomeFileRichiesto,0,sizeof(nomeFileRichiesto));

			while(1){
				
				pos=0;
                
				memset(bufferToReceive,0,sizeof(bufferToReceive));

				len = recvfrom(socketDescriptor,bufferToReceive,MAX_BUFFER_LENGTH,0,(struct sockaddr*)&temp, &sv_addr_length);
    
                //aggiornamento porta con cui comunicare con il server
                server_addr.sin_port = temp.sin_port;

                //de-serializzazione msg ricevuto
				memcpy(&op_code,bufferToReceive+pos,sizeof(op_code));
		    	pos += sizeof(op_code);
                op_code = ntohs(op_code);

		    	memcpy(&num,bufferToReceive+pos,sizeof(num));						    	
				pos += sizeof(num);
                num = ntohs(num);
            
                strcpy(msg,bufferToReceive+pos);
                pos += strlen(msg)+1;

                if(op_code==ERROR_OPCODE){
                    printf("Ricevuto errore\n");
                    printf("Codice Errore %d\n",num);
                    printf("Messaggio di errore:%s\n",msg);
                    break;

                }
                else{
					
				    printf("Blocco ricevuto: %d\n",num);
				    printf("Messaggio ricevuto: %s",msg);
					
				    numByteScritti = fwrite(msg,1,strlen(msg),fptr);
				    printf("Num Byte Scritti: %d\n",numByteScritti);
				    printf("Dimensione totale pacchetto: %d\n",len);

			            
                    //serializzazione msg di ack
                    ackNumber = htons(num);					
                    sendAck(ackNumber,socketDescriptor,server_addr);
            		
				    if(len<MAX_BUFFER_LENGTH){ 
                        printf("Ricevuto msg minore di 512 -> file finito\n"); 
					    printf("Chiudo file\n");
					    fclose(fptr);
                        break;
				    }
		        	
                }
			}
				
			
		}
		else if(strcmp(cmd,"!quit\n")==0){
            printf("Chiudo programma.\n");
		    close(socketDescriptor);
            exit(0);
		}
		
		else{
			printf("Comando non disponibile.\n");	
		}
    }
    
 
}
    
    
    
    
