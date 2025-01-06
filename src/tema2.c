#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tema2.h"

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

#define TAG_REQUEST 1
#define TAG_CLIENT_DONE 2
#define TAG_ARE_SEGMENT 3
#define TAG_FINISHED 4
#define TAG_UPLOAD 5
#define TAG_PEERS 6
#define TAG_BUSY 7
#define TAG_RASP_SEGMENT 8
#define TAG_DOWNLOAD_DONE 9
#define TAG_DOWNLOAD_REPLY 10

struct Tracker files;
int numtasks;

void create_mpi_piece_type(MPI_Datatype *mpi_type) {
    int block_lengths[3] = {1, HASH_SIZE, 1};
    MPI_Aint displacements[3];
    MPI_Datatype types[3] = {MPI_INT, MPI_CHAR, MPI_INT};

    displacements[0] = offsetof(struct Piece, index);
    displacements[1] = offsetof(struct Piece, hash);
    displacements[2] = offsetof(struct Piece, e_descarcat);

    MPI_Type_create_struct(3, block_lengths, displacements, types, mpi_type);
    MPI_Type_commit(mpi_type);
}

// Funcție pentru crearea tipului MPI pentru FileInfo
void create_mpi_fileinfo_type(MPI_Datatype *mpi_type) {
    MPI_Datatype mpi_piece_type;
    create_mpi_piece_type(&mpi_piece_type);  // Creăm mai întâi tipul MPI pentru Piece

    int block_lengths[3] = {MAX_FILENAME, 1, MAX_CHUNKS};
    MPI_Aint displacements[3];
    MPI_Datatype types[3] = {MPI_CHAR, MPI_INT, mpi_piece_type};

    displacements[0] = offsetof(struct FileInfo, filename);
    displacements[1] = offsetof(struct FileInfo, nr_segmente);
    displacements[2] = offsetof(struct FileInfo, pieces);

    MPI_Type_create_struct(3, block_lengths, displacements, types, mpi_type);
    MPI_Type_commit(mpi_type);
}

// Funcție pentru crearea tipului MPI pentru TrackerFiles
void create_mpi_trackerfiles_type(MPI_Datatype *mpi_type) {
    MPI_Datatype mpi_fileinfo_type;
    create_mpi_fileinfo_type(&mpi_fileinfo_type);  // Creăm mai întâi tipul MPI pentru FileInfo

    // printf("numstask e %d\n", numtasks + 1);
    int block_lengths[3] = {1, numtasks + 1, 1};
    MPI_Aint displacements[3];
    MPI_Datatype types[3] = {MPI_INT, MPI_INT, mpi_fileinfo_type};

    displacements[0] = offsetof(struct TrackerFiles, nr_seeds_peers);
    displacements[1] = offsetof(struct TrackerFiles, seeds_peers);
    displacements[2] = offsetof(struct TrackerFiles, info);

    MPI_Type_create_struct(3, block_lengths, displacements, types, mpi_type);
    MPI_Type_commit(mpi_type);
}

void *download_thread_func(void *arg)
{
    int rank = *(int*) arg;

    MPI_Datatype mpi_type;
    create_mpi_trackerfiles_type(&mpi_type);

    struct TrackerFiles data;
    data.seeds_peers = calloc((numtasks + 1), sizeof(int));

    int ten_counter = 0;
    
    // printf("pentru client %d sunt %d fisiere dorite\n", rank, files.nr_fisiere_dorite);
    for (int filenr = 0; filenr < files.nr_fisiere_dorite; filenr++){
        char numefisier[256];
        sprintf(numefisier, "client%d_%s", rank, files.fisiere_dorite[filenr].filename);
        FILE *file; 
        file = fopen(numefisier, "w");
        if (file == NULL) {
            // Dacă fișierul nu poate fi deschis, afișează un mesaj de eroare
            printf("Eroare la deschiderea fișierului!\n");
            exit(1);
        }

        // printf("rank %d urmeaza sa descarce fisierul %s\n", rank, files.fisiere_dorite[filenr].filename);
        MPI_Request requestfile, requestsegm, requestsegm1, requestbusy, requestbusy1;

        // Trimitem numele fișierului către tracker
        MPI_Isend(files.fisiere_dorite[filenr].filename, MAX_FILENAME + 1, MPI_CHAR, 0, TAG_REQUEST, MPI_COMM_WORLD, &requestfile);
        MPI_Wait(&requestfile, MPI_STATUS_IGNORE);
        // printf("client %d a trimis request pentru %s\n", rank, files.fisiere_dorite[filenr].filename);

        MPI_Irecv(&data, 1, mpi_type, 0, TAG_REQUEST, MPI_COMM_WORLD, &requestfile);
        // printf("client %d trebuie sa isi primeasca seeds/peers pentru %s\n", rank, files.fisiere_dorite[filenr].filename);
        MPI_Wait(&requestfile, MPI_STATUS_IGNORE);
        
        // printf("client %d a primit info despre %s fara peersi\n", rank, data.info.filename);

        MPI_Request requestfile1;
        int *seeds_peers = calloc((numtasks + 1), sizeof(int));
        MPI_Irecv(seeds_peers, numtasks + 1, MPI_INT, 0, TAG_REQUEST, MPI_COMM_WORLD, &requestfile1);
        MPI_Wait(&requestfile1, MPI_STATUS_IGNORE);

        printf("fisierul %s are %d segmente:\n", files.fisiere_dorite[filenr].filename, data.info.nr_segmente);

        files.fisiere_dorite[filenr].nr_segmente = data.info.nr_segmente;
        
        for (int i = 0; i < files.fisiere_dorite[filenr].nr_segmente; i++){
            strcpy(files.fisiere_dorite[filenr].pieces[i].hash, data.info.pieces[i].hash);
            files.fisiere_dorite[filenr].pieces[i].e_descarcat = 0;
            files.fisiere_dorite[filenr].pieces[i].index = i;
        }
        

        // if (rank == 1)
        for (int segment = 0; segment < data.info.nr_segmente; segment++){
        // if (rank == 3){
        //     if (files.fisiere_dorite[filenr].pieces[segment].e_descarcat == 0){
                if (ten_counter % 10 == 9){
                    // printf("inainte sa actualizeze seeds_peers pentru %s erau:\n", files.fisiere_dorite[filenr].filename);
                    // for (int x = 1; x < numtasks; x++){
                    //     if (seeds_peers[x] == 1){
                    //         printf("%d ", x);
                    //     }
                    // }
                    // printf("\n");
                    MPI_Isend(files.fisiere_dorite[filenr].filename, MAX_FILENAME + 1, MPI_CHAR, 0, TAG_PEERS, MPI_COMM_WORLD, &requestfile);
                    MPI_Wait(&requestfile, MPI_STATUS_IGNORE);
                    MPI_Irecv(seeds_peers, numtasks + 1, MPI_INT, 0, TAG_PEERS, MPI_COMM_WORLD, &requestfile1);
                    // printf("client %d trebuie sa isi primeasca seeds/peers pentru %s\n", rank, files.fisiere_dorite[filenr].filename);
                    MPI_Wait(&requestfile1, MPI_STATUS_IGNORE);
                    if ((rank == 5 || rank == 3) && strncmp(files.fisiere_dorite[filenr].filename, "file4", 5) == 0){
                    printf("dupa sa actualizeze seeds_peers pentru %s erau:\n", files.fisiere_dorite[filenr].filename);
                    for (int x = 1; x < numtasks; x++){
                        if (seeds_peers[x] == 1){
                            printf("%d ", x);
                        }
                    }
                    printf("\n");
                    }
                    // printf("a facut asta cu ten\n");
                    // ten_counter = 0;
                }

                int less_busy = 1000000000, dest = 0;
                // if (rank == 5 || rank == 1)
                // printf("rank %d, alege pentru fisier %s dintre seeds/peers:\n", rank, files.fisiere_dorite[filenr].filename);
                for (int i = 1; i < numtasks; i++){
                    // printf("rank %d verifica daca poate trimite la client %d\n", rank, i);
                    if (i != rank && seeds_peers[i] == 1){
                        // printf("rank %d veri %d\n", rank, i);
                        MPI_Isend("ai hash?", 9, MPI_CHAR, i, TAG_UPLOAD, MPI_COMM_WORLD, &requestsegm);
                        // printf("a trimis ai hash?\n");
                        MPI_Wait(&requestsegm, MPI_STATUS_IGNORE);
                        // if (err != MPI_SUCCESS) {
                        //     printf("Eroare la MPI_Isend: %d\n", err);
                        // }
                        MPI_Isend(files.fisiere_dorite[filenr].pieces[segment].hash, HASH_SIZE + 1, MPI_CHAR, i, TAG_ARE_SEGMENT, MPI_COMM_WORLD, &requestsegm1);
                        MPI_Wait(&requestsegm1, MPI_STATUS_IGNORE);
                        
                        char mesaj[256];
                        MPI_Irecv(mesaj, 256, MPI_CHAR, i, TAG_RASP_SEGMENT, MPI_COMM_WORLD, &requestsegm);
                        MPI_Wait(&requestsegm, MPI_STATUS_IGNORE);
                        // printf("client %d - ", i);
                        // printf("a primit ca raspuns %s\n", mesaj);
                        if (strncmp(mesaj, "ACK", 3) == 0){
                            //ii cer busy-ul
                            int busy;
                            MPI_Isend("busy", 5, MPI_CHAR, i, TAG_UPLOAD, MPI_COMM_WORLD, &requestbusy);
                            MPI_Wait(&requestbusy, MPI_STATUS_IGNORE);
                            MPI_Irecv(&busy, 1, MPI_INT, i, TAG_BUSY, MPI_COMM_WORLD, &requestbusy1);
                            MPI_Wait(&requestbusy1, MPI_STATUS_IGNORE);
                            // if (strncmp(files.fisiere_dorite[filenr].filename, "file1", 5) == 0)
                            // printf("%d e busy %d\n", i, busy);
                            // if ((rank == 5 || rank == 3) && strncmp(files.fisiere_dorite[filenr].filename, "file4", 5) == 0)
                            // printf("client %d - busy %d pt fisier %s, \n",i, busy, files.fisiere_dorite[filenr].filename);
                            if (busy < less_busy){
                                less_busy = busy;
                                dest = i;
                            }
                        }
                        else{
                            // if ((rank == 5 || rank == 3) && strncmp(files.fisiere_dorite[filenr].filename, "file4", 5) == 0)
                            // printf("client %d nu are segm %d din fisierul %s\n",i, segment, files.fisiere_dorite[filenr].filename);
                        }

                        
                   }
                }
            // printf("\n");
            char rasp[256];
            MPI_Isend(files.fisiere_dorite[filenr].pieces[segment].hash, HASH_SIZE + 1, MPI_CHAR, dest, TAG_UPLOAD, MPI_COMM_WORLD, &requestbusy);
            MPI_Wait(&requestbusy, MPI_STATUS_IGNORE);
            ten_counter++;
            MPI_Request requestdownload;
            MPI_Irecv(rasp, 256, MPI_CHAR, dest, TAG_DOWNLOAD_REPLY, MPI_COMM_WORLD, &requestdownload);
            MPI_Wait(&requestdownload, MPI_STATUS_IGNORE);
            // if ((rank == 5 || rank == 3) && strncmp(files.fisiere_dorite[filenr].filename, "file4", 5) == 0)
            // printf("rank %d, descarca din %s segment %d de la client %d\n", rank, files.fisiere_dorite[filenr].filename, segment, dest);
            if (strncmp(rasp, "ACK", 3) == 0){
                files.fisiere_dorite[filenr].pieces[segment].e_descarcat = 1;
                fprintf(file, "%.*s\n", HASH_SIZE, files.fisiere_dorite[filenr].pieces[segment].hash);
                // "%.*s", num_chars, string
            }
            
        }

        fclose(file);
    }

    MPI_Request request_done;
    MPI_Isend("done", 4, MPI_CHAR, 0, TAG_CLIENT_DONE, MPI_COMM_WORLD, &request_done);
    // printf("client %d trimite download done lui insusi\n", rank);
    MPI_Wait(&request_done, MPI_STATUS_IGNORE);    

    return NULL;
}

void *upload_thread_func(void *arg)
{
    int rank = *(int*) arg;
    int busy = 0;

    // for (int contor = 1; contor <= 3; contor++){
    while(1){
        char mesaj[256];
        int source;
        MPI_Status status;
        MPI_Request request;
        
        MPI_Irecv(mesaj, 256, MPI_CHAR, MPI_ANY_SOURCE, TAG_UPLOAD, MPI_COMM_WORLD, &request); //TODO modif tag
        // printf("rank %d a primit un mesaj pe upload\n", rank);
        MPI_Wait(&request, &status);

        // printf("rank %d a primit un mesajul %s pe upload cu tag-ul %d\n", rank, mesaj, status.MPI_TAG);
        
        source = status.MPI_SOURCE;
        // printf("sursa e %d\n", source);
        // printf("mesajul e %s\n", mesaj);
        
        if (strncmp(mesaj, "busy", 4) == 0){
            MPI_Isend(&busy, 1, MPI_INT, source, TAG_BUSY, MPI_COMM_WORLD, &request);
            MPI_Wait(&request, MPI_STATUS_IGNORE);
        }
        else if (strncmp(mesaj, "ai hash?", 8) == 0){
            // printf("rank %d a primit mesajul %s pe TAG_UPLOAD de la client %d\n", rank, mesaj, source);
            //in mesaj e hash-ul
            MPI_Request request1;
            MPI_Status status1;
            char hash[HASH_SIZE + 1];
            MPI_Irecv(hash, HASH_SIZE + 1, MPI_CHAR, source, TAG_ARE_SEGMENT, MPI_COMM_WORLD, &request1); //TODO modif tag
            // printf("rank %d a primit un mesaj pe upload\n", rank);
            MPI_Wait(&request1, &status1);
            
            char filename[MAX_FILENAME];
            
            int filenr = 0, segm = 0, gasit = 0;
            
            for (filenr = 0; filenr < files.nr_fisiere_descarcate; filenr++){
                
                for (segm = 0; segm < files.fisiere[filenr].nr_segmente; segm++){
                    
                    if (strncmp(hash, files.fisiere[filenr].pieces[segm].hash, HASH_SIZE) == 0 && files.fisiere[filenr].pieces[segm].e_descarcat == 1){
                        //nrsegm = files.fisiere[filenr].nr_segmente;
                        strcpy(filename, files.fisiere[filenr].filename);
                        gasit = 1;
                        break;
                    }
                if (gasit)
                    break;
                }
            }
            if (!gasit){
                for (filenr = 0; filenr < files.nr_fisiere_dorite; filenr++){
                    for (segm = 0; segm < files.fisiere_dorite[filenr].nr_segmente; segm++){
                        if (strncmp(hash, files.fisiere_dorite[filenr].pieces[segm].hash, HASH_SIZE) == 0 && files.fisiere_dorite[filenr].pieces[segm].e_descarcat == 1){
                            //nrsegm = files.fisiere[filenr].nr_segmente;
                            strcpy(filename, files.fisiere[filenr].filename);
                            gasit = 1;
                            break;
                        }
                    if (gasit)
                        break;
                    }
                }
            }
            if (gasit){
                MPI_Isend("ACK", 4, MPI_CHAR, source, TAG_RASP_SEGMENT, MPI_COMM_WORLD, &request);
                MPI_Wait(&request, MPI_STATUS_IGNORE);
            }
            else{
                MPI_Isend("NACK", 4, MPI_CHAR, source, TAG_RASP_SEGMENT, MPI_COMM_WORLD, &request);
                MPI_Wait(&request, MPI_STATUS_IGNORE);
            }
            
        }
        else if (strcmp(mesaj, "ACK") == 0){
            // MPI_Barrier(MPI_COMM_WORLD);
            printf("rank %d termina upload\n", rank);
            break;
        }
        else{ //e de download
            busy++;
            char hash[HASH_SIZE + 1];
            MPI_Request requestsend;
            strncpy(hash, mesaj, HASH_SIZE);

            int filenr = 0, segm = 0, gasit = 0;
            
            for (filenr = 0; filenr < files.nr_fisiere_descarcate; filenr++){
                
                for (segm = 0; segm < files.fisiere[filenr].nr_segmente; segm++){
                    if (strncmp(hash, files.fisiere[filenr].pieces[segm].hash, HASH_SIZE) == 0 && files.fisiere[filenr].pieces[segm].e_descarcat == 1){
                        gasit = 1;
                        files.fisiere[filenr].pieces[segm].e_descarcat = 1;
                        break;
                    }
                if (gasit)
                    break;
                }
            }
            if (!gasit){
                for (filenr = 0; filenr < files.nr_fisiere_dorite; filenr++){
                    for (segm = 0; segm < files.fisiere_dorite[filenr].nr_segmente; segm++){
                        if (strcmp(hash, files.fisiere_dorite[filenr].pieces[segm].hash) == 0 && files.fisiere_dorite[filenr].pieces[segm].e_descarcat == 1){
                            gasit = 1;
                            files.fisiere_dorite[filenr].pieces[segm].e_descarcat = 1;
                            break;
                        }
                    if (gasit)
                        break;
                    }
                }
            }

            MPI_Isend("ACK", 4, MPI_CHAR, source, TAG_DOWNLOAD_REPLY, MPI_COMM_WORLD, &requestsend);
            MPI_Wait(&requestsend, MPI_STATUS_IGNORE);
        }
    }

    return NULL;
}

void tracker(int numtasks, int rank) {
    int nrsegm, fisiere;
    int clienti_gata = 0;
    struct TrackerFiles trackerfiles[MAX_FILES];
    int fisieretotal = 0;
    for (int i = 0; i < MAX_FILES; i++)
        trackerfiles[i].seeds_peers = calloc((numtasks + 1), sizeof(int));
    for (int i = 1; i < numtasks; i++){
        MPI_Recv(&fisiere, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for (int j = 0; j < fisiere; j++){
            char filename[MAX_FILENAME];
            MPI_Recv(filename, MAX_FILENAME, MPI_CHAR, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(&nrsegm, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            
            int gasit = 0, t;
            for (t = 0; t < fisieretotal; t++){
                if (strcmp(filename, trackerfiles[t].info.filename) == 0){
                    trackerfiles[t].seeds_peers[i] = 1;
                    trackerfiles[t].nr_seeds_peers++;
                    gasit = 1;
                }
                if (gasit)
                    break;
            }
            

            if (!gasit){
                strcpy(trackerfiles[fisieretotal].info.filename, filename);
                trackerfiles[fisieretotal].seeds_peers[i] = 1;
                trackerfiles[fisieretotal].info.nr_segmente = nrsegm;
                trackerfiles[fisieretotal].nr_seeds_peers = 1;
                t = fisieretotal;
                fisieretotal++;
            }


            for(int k = 0; k < nrsegm; k++){
                MPI_Recv(trackerfiles[t].info.pieces[k].hash, 34, MPI_CHAR, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                // printf("hashu e %s\n", trackerfiles[t].info.pieces[k].hash);
                trackerfiles[t].info.pieces[k].index = k;
                trackerfiles[t].info.pieces[k].e_descarcat = 1;
            }

            
            // if (strncmp(trackerfiles[t].info.filename, "file7", 5) == 0){
            //     printf("de la clientu 7 care are fisier 7\n");
            //     printf("Nr segmente: %d\n", trackerfiles[t].info.nr_segmente);
            //     for (int c = 0; c < trackerfiles[t].info.nr_segmente; c++) {
            //         printf("Piece %d - Index: %d, Hash: %s, Descarcat: %d\n", i,
            //         trackerfiles[t].info.pieces[c].index, trackerfiles[t].info.pieces[c].hash, trackerfiles[t].info.pieces[c].e_descarcat);
            //     }
            // }


        }
        
    }

    MPI_Datatype mpi_type;
    create_mpi_trackerfiles_type(&mpi_type);

    for (int i = 1; i < numtasks; i++) {
        MPI_Send("ACK", 4, MPI_CHAR, i, 0, MPI_COMM_WORLD);
        // printf("Tracker: Sent ACK to client %d.\n", i);
    }

    

    while (clienti_gata < numtasks - 1){
        char mesaj[256];
        int flag, source;
        MPI_Status status;
        MPI_Request request, request1;
        // clienti_gata++;
        MPI_Irecv(mesaj, 256, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &request); //TODO modif tag
        // printf("asteapta sa primeasca\n");
        MPI_Wait(&request, &status);
        // MPI_Test(&request, &flag, &status);
        // printf("tracker a primit mesaj cu tag-ul %d de la rank %d\n", status.MPI_TAG, status.MPI_SOURCE);
 
        source = status.MPI_SOURCE;
        
        if (status.MPI_TAG == TAG_REQUEST){
            // printf("tracker-ul a primit request de la %d pentru swarm-ul fisierului %s\n", source, mesaj);
            int nr = 0;
            for (nr = 0; nr < MAX_FILES; nr++){
                if (strcmp(mesaj, trackerfiles[nr].info.filename) == 0){
                    // printf("a gasit fisierul\n");
                    break;
                }
            }
            
            // printf("tracker-ul trimite catre client %d swarm ul pentru %s\n", source, mesaj);
            MPI_Isend(&trackerfiles[nr], 1, mpi_type, source, TAG_REQUEST, MPI_COMM_WORLD, &request);
            MPI_Wait(&request, &status);
        
            MPI_Isend(trackerfiles[nr].seeds_peers, numtasks + 1, MPI_INT, source, TAG_REQUEST, MPI_COMM_WORLD, &request1);
            MPI_Test(&request1, &flag, &status);
            
            if (!flag) {
                // printf("[P0] The send operation is not over yet for seeds_peers in swarm\n");
                MPI_Wait(&request1, &status);
            }
            if (trackerfiles[nr].seeds_peers[source] == 0){
                trackerfiles[nr].nr_seeds_peers++;
                trackerfiles[nr].seeds_peers[source] = 1;
            }
            // printf("s a trim toata info\n");
            

        }
        else if (status.MPI_TAG == TAG_PEERS){
            // printf("a primit mesaj pe TAG_PEERS\n");
            int nr = 0;
            for (nr = 0; nr < MAX_FILES; nr++){
                if (strcmp(mesaj, trackerfiles[nr].info.filename) == 0){
                    // printf("a gasit fisierul\n");
                    break;
                }
            }
            MPI_Isend(trackerfiles[nr].seeds_peers, numtasks + 1, MPI_INT, source, TAG_PEERS, MPI_COMM_WORLD, &request1);
            MPI_Test(&request1, &flag, &status);
            if (flag) {
                // printf("[P0] The send operation is over for seeds_peers in swarm\n");
            } else {
                // printf("[P0] The send operation is not over yet for seeds_peers in swarm\n");
                MPI_Wait(&request1, &status);
            }
        }
        else if (status.MPI_TAG == TAG_CLIENT_DONE){
            // printf("tracker a aflat ca client %d e gata\n", source);
            clienti_gata++;
        }

    }
    // printf("%d clienti done\n", clienti_gata);
    for (int i = 1; i < numtasks; i++) {
        MPI_Request request;
        MPI_Isend("ACK", 4, MPI_CHAR, i, TAG_UPLOAD, MPI_COMM_WORLD, &request);
        //MPI_I
        MPI_Wait(&request, MPI_STATUS_IGNORE);
        // printf("Tracker: Sent ACK to finish upload %d.\n", i);
    }
    // printf("Rank %d a trimis structura către Rank 1.\n", rank);
    
}

void peer(int numtasks, int rank) {
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;

    char filename[50];
    FILE *file;

    snprintf(filename, sizeof(filename), "in%d.txt", rank);

    // Deschide fișierul pentru citire sau scriere
    file = fopen(filename, "r");
    if (file == NULL) {
        printf("Process [%d]: Failed to open file %s\n", rank, filename);
        MPI_Finalize();
        exit(-1);
    }
    // printf("s a deschis fisierul %s\n", filename);

    fscanf(file, "%d", &files.nr_fisiere_descarcate);

    MPI_Send(&files.nr_fisiere_descarcate, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);

    for (int i = 0; i < files.nr_fisiere_descarcate; i++){
        fscanf(file, "%s %d", files.fisiere[i].filename, &files.fisiere[i].nr_segmente);
        MPI_Send(files.fisiere[i].filename, strlen(files.fisiere[i].filename) + 1, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
        // printf("bumsacalaca %s\n", files.fisiere[i].filename);
        MPI_Send(&files.fisiere[i].nr_segmente, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        for (int index = 0; index < files.fisiere[i].nr_segmente; index ++){
            fscanf(file, "%s", files.fisiere[i].pieces[index].hash);
            files.fisiere[i].pieces[index].index = index;
            files.fisiere[i].pieces[index].e_descarcat = 1;
            // if (rank == 1 && strcmp(files.fisiere[i].filename, "file1") == 0)
            //     printf("s a descarcat rank 1 file1\n");
            MPI_Send(files.fisiere[i].pieces[index].hash, 34, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
            files.fisiere[i].pieces[index].e_descarcat = 1;
        }
    }
    
    fscanf(file, "%d", &files.nr_fisiere_dorite);
    //printf("rank %d vrea %d fisiere\n", rank, files.nr_fisiere_dorite);
    for (int i = 0; i < files.nr_fisiere_dorite; i++){
        fscanf(file, "%s", files.fisiere_dorite[i].filename);
        //files.fisiere_dorite[i].pieces->e_descarcat = 0
    }

    char ack[4];
    MPI_Recv(ack, 4, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    if (strcmp(ack, "ACK") == 0) {
        // printf("Client %d: Received ACK from tracker. Starting download/upload phase.\n", rank);
        // Continuă cu logica de download/upload
    }

    r = pthread_create(&download_thread, NULL, download_thread_func, (void *) &rank);
    if (r) {
        printf("Eroare la crearea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *) &rank);
    if (r) {
        printf("Eroare la crearea thread-ului de upload\n");
        exit(-1);
    }

    r = pthread_join(download_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_join(upload_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de upload\n");
        exit(-1);
    }
    fclose(file);
}
 
int main (int argc, char *argv[]) {
    int rank;
 
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);


    if (rank == TRACKER_RANK) {
        tracker(numtasks, rank);
    } else {
        peer(numtasks, rank);
    }

    MPI_Finalize();
}
