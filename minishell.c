#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h> /* exit */
#include <sys/wait.h> /* wait */
#include <signal.h> 
#include "readcmd.h"
#include <malloc.h>
#include <errno.h>
#include <fcntl.h>

int id_global=0;

typedef struct job{
    int id;
    int pid;
    int etat; /* 0=suspendu 1=actif */
    char* commande;
    bool backgrounded;  /* 0=foregrounded 1=backgrounded */
} job;

typedef struct cellule {
    job processus;
    struct cellule* suivant;
} liste_jobs;

liste_jobs* liste_globale;

void initialiser_liste(liste_jobs* liste){
    liste->processus.pid = -2;
    liste->suivant = NULL;
} 


void add_job(liste_jobs* liste, job process){
    liste_jobs* curseur = liste;
    liste_jobs* nouvelle_cellule = malloc(sizeof(liste_jobs));
    nouvelle_cellule->processus=process;
    nouvelle_cellule->suivant = NULL;

    if (curseur->processus.pid == -2){
        curseur->processus = process;
        curseur->suivant=NULL;
        return;
    } else{
        while (curseur->suivant != NULL){
            curseur = curseur->suivant;
        } 
        curseur->suivant = nouvelle_cellule;
    }
}  

void del_job(liste_jobs* liste, int id){
    liste_jobs *curseur = liste;
    
    if (curseur == NULL){
        return;
    } else if (curseur->processus.id == id && curseur->suivant == NULL) {
        liste_jobs* nouvelle_cellule = malloc(sizeof(liste_jobs));
        nouvelle_cellule->processus.pid = -2;
        nouvelle_cellule->suivant = NULL;
        liste_globale=nouvelle_cellule;
        return;
    } else if (curseur->processus.id == id && curseur->suivant != NULL) {
        liste_globale = curseur->suivant;
    } else if (curseur->suivant == NULL) {
        return;
    } else { 
        while(curseur->suivant->processus.id != id && curseur->suivant->suivant !=NULL) {
            curseur = curseur->suivant;  
        }
        if (curseur->suivant->processus.id == id && curseur->suivant->suivant == NULL) {
            curseur->suivant = NULL;
            return;
        } else if (curseur->suivant->processus.id == id) {
            curseur->suivant = curseur->suivant->suivant;
            return;
        }
    }
} 
    
void modifier_etat_bg (liste_jobs* liste, job* process){
    liste_jobs *curseur = liste;
    if (curseur == NULL){
        printf("Ce job n'existe pas");
    } else { 
        while(curseur->processus.id != process->id && curseur->suivant !=NULL) {
            curseur = curseur->suivant;  
        }
        if (curseur->processus.id == process->id) {
            curseur->processus.etat = process->etat;
            curseur->processus.backgrounded = process->backgrounded;
            return;
        } else {
            printf("Ce job n'existe pas");
        } 
    }
} 

job* getJob(liste_jobs* liste, int identifiant){
    liste_jobs* curseur = liste;
    if (curseur == NULL) {
        printf("Identifiant invalide");
        return NULL;
    } 
    while (curseur->suivant != NULL && curseur->processus.id != identifiant){
        curseur = curseur->suivant;
    } 
    if (curseur->processus.id == identifiant){
        return &curseur->processus;
    } else{
        printf("Identifiant invalide");
        return NULL;
    } 
} 

job* getJobPid(liste_jobs* liste, int pid){
    liste_jobs* curseur = liste;
    if (curseur == NULL) {
        printf("Pid invalide");
        return NULL;
    } 
    while (curseur->suivant != NULL && curseur->processus.pid != pid){
        curseur = curseur->suivant;
    } 
    if (curseur->processus.pid == pid){
        return &curseur->processus;
    } else{
        return NULL;
    } 
} 

void afficher_liste(liste_jobs* liste){
    liste_jobs* curseur = liste;
    while(curseur!=NULL){
        if (curseur->processus.pid != -2) {
            char* etat = curseur->processus.etat == 0 ? "suspendu" : "actif";   
        printf("Processus d'identifiant %d | PID : %d | Etat : %s | commande: %s \n", 
            curseur->processus.id, curseur->processus.pid, etat, curseur->processus.commande);
        }
        curseur = curseur->suivant;
    } 
} 

// Trouver le job en premier plan 
job* job_fg(liste_jobs* liste) {
    liste_jobs* curseur = liste;
    if (curseur == NULL) {
        printf("Liste vide");
        return NULL;
    } else {
        while (curseur->suivant != NULL && curseur->processus.backgrounded) {
            curseur = curseur->suivant; 
        } 
        if (!curseur->processus.backgrounded && curseur->processus.etat == 1){
            return &curseur->processus;
        } else {
            printf("\n ");
            return NULL;
        } 
    }
}

void handler_z(int sig) {
    signal(sig,SIG_IGN);
    struct sigaction sa_z;
    sa_z.sa_handler = handler_z;
    sa_z.sa_flags = 0;
    sigemptyset(&sa_z.sa_mask);

    if ( sigaction(SIGTSTP, &sa_z, NULL) == -1 ) {
        perror("Couldn't set SIGTSTP handler\n");
        exit(EXIT_FAILURE);
    }
    job* job_stop = job_fg(liste_globale);
    if (job_stop !=NULL) {
        job_stop->etat = 0;
        job_stop->backgrounded=true;
        kill(job_stop->pid, SIGSTOP);
        modifier_etat_bg(liste_globale,job_stop);
    }
}

void handler_c(int sig) {
    signal(sig,SIG_IGN );
    struct sigaction sa_c;
    sa_c.sa_handler = handler_c;
    sa_c.sa_flags = 0;
    sigemptyset(&sa_c.sa_mask);

    if ( sigaction(SIGINT, &sa_c, NULL) == -1 ) {
        perror("Couldn't set SIGINT handler\n");
        exit(EXIT_FAILURE);
    }
    job* job_termine = job_fg(liste_globale);
    if (job_termine !=NULL) {
        job_termine->etat = 0;
        kill(job_termine->pid, SIGKILL);
        modifier_etat_bg(liste_globale,job_termine);
    }
}

void check_terminated_jobs(liste_jobs* liste) {
    liste_jobs* curseur = liste;
    int terminated;
    int job_courrant_id;
    while(curseur!=NULL){
        terminated = waitpid(curseur->processus.pid, NULL, WNOHANG);
        job_courrant_id = curseur->processus.id;
        curseur = curseur->suivant;
        if (terminated == -1) {
            del_job(liste, job_courrant_id);
        }
    }
}


int main (int argc, char *argv[] ){
    setvbuf(stdout, NULL, _IONBF, 0);

    liste_globale = malloc(sizeof(liste_jobs));
    struct cmdline *command;
    int pidFils;
    int exitCode;
    int filsTermine;
    int status;
    int dup_in;
    int dup_out;

    struct sigaction sa_z;
    sa_z.sa_handler = handler_z;
    sa_z.sa_flags = 0;
    sigemptyset(&sa_z.sa_mask);

    if ( sigaction(SIGTSTP, &sa_z, NULL) == -1 ) {
        perror("Couldn't set SIGTSTP handler\n");
        exit(EXIT_FAILURE);
    }
    
    struct sigaction sa_c;
    sa_c.sa_handler = handler_c;
    sa_c.sa_flags = 0;
    sigemptyset(&sa_c.sa_mask);

    if ( sigaction(SIGINT, &sa_c, NULL) == -1 ) {
        perror("Couldn't set SIGINT handler\n");
        exit(EXIT_FAILURE);
    }

    initialiser_liste(liste_globale);

    while(1) {
        id_global++;
        check_terminated_jobs(liste_globale);
        
        printf("lweisbec@machine $ ");
        command =  readcmd(); /*Lecture de la ligne de commande */

        if (command == 0 || command->seq[0] == NULL ) {  /*S'il n'y a pas de commande, on répète la boucle */
            continue;
        } else if (!strcmp(command->seq[0][0], "cd")) {   /* Cas où la commande est cd */
            chdir(command->seq[0][1]);
        } else if (!strcmp(command->seq[0][0], "exit")) {   /* Cas où la commande est exit */
            exit(EXIT_SUCCESS);
        } else if (!strcmp(command->seq[0][0], "lj")) {   /* Cas où la commande est lj */
            int pid = fork();
            if (pidFils == -1) {        /* Erreur lors de la création du fils */
                printf("Erreur fork\n");
                exit(1);
            } else if (pid == 0) {
                if (command->in != NULL) {  // Redirection de l'entrée standard
                int fd_in = open(command->in, O_RDONLY) ;   
                if (fd_in < 0) {
                    printf("Erreur lors de l'ouverture du fichier in %s\n", command->in) ;
                    exit(1) ;
                }

                dup_in = dup2(fd_in, 0);
                if (dup_in == -1) {  
                    perror("Echec lors de la redirection de l'entrée");
                    exit(1) ;
                }
                close(fd_in);
                } 
            if (command->out != NULL) { // Redirection de la sortie standard 
                int fd_out = open(command->out, O_WRONLY | O_CREAT | O_TRUNC, 0640) ;  

                if (fd_out < 0) {
                    printf("Erreur lors de l'ouverture du fichier out %s\n", command->out) ;
                    exit(1) ;
                }

                dup_out = dup2(fd_out, 1) ;
                if (dup_out == -1) {   
                    perror("Echec lors de la redirection de l'entrée");
                    exit(1) ;
                }
                close(fd_out);
            }
            afficher_liste(liste_globale);
            exit(1);
            }      
            
        } else if (!strcmp(command->seq[0][0], "sj")) {   /* Cas où la commande est sj */
            job *job_stop = getJob(liste_globale, atoi(command->seq[0][1]));
            if (job_stop !=NULL) {
                job_stop->etat = 0;
                kill(job_stop->pid, SIGSTOP);
                modifier_etat_bg(liste_globale,job_stop);
            }
        } else if (!strcmp(command->seq[0][0], "bg")) {   /* Cas où la commande est bg */
            job *job_background = getJob(liste_globale, atoi(command->seq[0][1]));
            if (job_background !=NULL) {
                job_background->etat = 1;
                job_background->backgrounded = true;
                kill(job_background->pid, SIGCONT);
                modifier_etat_bg(liste_globale,job_background);
            }
        } else if (!strcmp(command->seq[0][0], "fg")) {   /* Cas où la commande est fg */
            job *job_foreground = getJob(liste_globale, atoi(command->seq[0][1]));
            if (job_foreground !=NULL) {
                job_foreground->etat = 1;
                job_foreground->backgrounded = false;
                kill(job_foreground->pid, SIGCONT);
                modifier_etat_bg(liste_globale,job_foreground);
                waitpid(job_foreground->pid, &status, 0);
            } /* else if (!strcmp(command->seq[0][0], "jobfg")) {
            job *jobFg = job_fg(liste_globale);
            if (jobFg == NULL) {
                printf("job null\n");
            } else {
                printf("job id: %d", jobFg->id);
            } */
        } else if ( !strcmp(command->seq[0][0], "susp") ){ /* cas où la commande est susp */
                kill(getpid(),SIGSTOP);    
        }  else {                                    /* Traitement des autres commandes */
            pidFils=fork();
            
            if (pidFils == -1) {        /* Erreur lors de la création du fils */
                printf("Erreur fork\n");
                exit(1);
            } else if (pidFils == 0) {		/* Processus fils */

                if (command->in != NULL) {  // Redirection de l'entrée standard
                int fd_in = open(command->in, O_RDONLY) ;   
                if (fd_in < 0) {
                    printf("Erreur lors de l'ouverture du fichier in %s\n", command->in) ;
                    exit(1) ;
                }

                dup_in = dup2(fd_in, 0);
                if (dup_in == -1) {  
                    perror("Echec lors de la redirection de l'entrée");
                    exit(1) ;
                }
                close(fd_in);
                } 
            if (command->out != NULL) { // Redirection de la sortie standard 
                int fd_out = open(command->out, O_WRONLY | O_CREAT | O_TRUNC, 0640) ;  

                if (fd_out < 0) {
                    printf("Erreur lors de l'ouverture du fichier out %s\n", command->out) ;
                    exit(1) ;
                }

                dup_out = dup2(fd_out, 1);
                if (dup_out == -1) {   
                    perror("Echec lors de la redirection de l'entrée");
                    exit(1) ;
                }
                close(fd_out);
            }
                int nb_tubes = 0;
                while (command->seq[nb_tubes] != NULL){
                    nb_tubes++;
                } 

                nb_tubes--;

                int tubes[nb_tubes][2];
                

                if (nb_tubes>0){
                    pipe(tubes[0]);
                    int pidFils1 = fork();
                    if (pidFils1 == -1) {        /* Erreur lors de la création du fils 1 */
                        exit(1);
                    } else if (pidFils1 == 0){
                        close(tubes[0][0]);
                        dup2(tubes[0][1],1);
                        close(tubes[0][1]);
                        exitCode = execvp(command->seq[0][0], command->seq[0]);
                        printf("%s : commande invalide\n", command->seq[0][0]);
                        exit(exitCode);
                    } else {
                        for (int i=1; i<nb_tubes;i++) {
                            close(tubes[i-1][1]);
                            pipe(tubes[i]);
                            int pidFils2=fork();
                            if (pidFils2 == -1) {        /* Erreur lors de la création du fils 2 */
                                printf("Erreur fork fils %d\n",i+1);
                                exit(1);
                            } else if (pidFils2 == 0) {		/* Processus fils 2 */
                                close(tubes[i-1][1]);
                                dup2(tubes[i-1][0],0);
                                close(tubes[i-1][0]);
                                close(tubes[i][0]);
                                dup2(tubes[i][1],1);
                                close(tubes[i][1]);
                                exitCode = execvp(command->seq[i][0], command->seq[i]);
                                printf("%s : commande invalide\n", command->seq[i][0]);
                                exit(exitCode);
                                
                            }  
                        }
                        if (nb_tubes > 1) {
                            close(tubes[nb_tubes-2][0]);
                            close(tubes[nb_tubes-1][1]);
                            dup2(tubes[nb_tubes-1][0] ,0);
                            close(tubes[nb_tubes-1][0]);
                            exitCode = execvp(command->seq[nb_tubes][0], command->seq[nb_tubes]);
                            printf("%s : commande invalide\n", command->seq[nb_tubes][0]);
                            exit(exitCode); 
                        } else {
                            close(tubes[0][1]);
                            dup2(tubes[0][0] ,0);
                            close(tubes[0][0]);
                            exitCode = execvp(command->seq[nb_tubes][0], command->seq[nb_tubes]);
                            printf("%s : commande invalide\n", command->seq[nb_tubes][0]);
                            exit(exitCode); 
                        } 
                        
                    } 
                } else {
                    execvp(command->seq[0][0], command->seq[0]);
                    printf("%s : commande invalide\n", command->seq[0][0]);
                    exit(1);
                } 
            } else {		/* Processus père */
                job process;
                process.pid=pidFils;
                process.id=id_global;
                int index=0;
/*                int longueur_ligne_commande=0;
                 while (command->seq[0][index] !=NULL){
                    longueur_ligne_commande=longueur_ligne_commande+strlen(command->seq[0][index])+1;
                    index++;
                }  */
                process.commande = (char*)malloc(sizeof(char) * 1024);
                while (command->seq[0][index] != NULL){
                    strcat(process.commande," ");
                    strcat(process.commande,command->seq[0][index]);
                    index++;
                } 
                process.backgrounded = command->backgrounded != NULL;
                process.etat = 1;

                add_job(liste_globale,process);
                if (command->backgrounded == NULL){     /*Test processus en background avec &*/
                    filsTermine = waitpid(pidFils, &status,0);
 
                    if (filsTermine == -1) {
     //                   printf("\n processus fils : %d termine\n",pidFils);
                    } else if WIFSIGNALED(status) {  /* Processus fils kill par un signal externe*/
                        printf("\n processus fils : %d kill par le signal %d\n", filsTermine, WTERMSIG(status)) ;
                    } else if WIFEXITED(status) {   /* Processus fils termine avec exit */
     //                   printf("\n processus fils : %d termine avec exit %d\n",filsTermine, WEXITSTATUS(status)) ;
                    } 
                }
            }
        } 
    }
}

