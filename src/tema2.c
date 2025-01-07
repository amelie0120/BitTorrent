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
    MPI_Aint offsets[3];
    MPI_Datatype types[3] = {MPI_INT, MPI_CHAR, MPI_INT};

    offsets[0] = offsetof(struct Piece, index);
    offsets[1] = offsetof(struct Piece, hash);
    offsets[2] = offsetof(struct Piece, e_descarcat);

    MPI_Type_create_struct(3, block_lengths, offsets, types, mpi_type);
    MPI_Type_commit(mpi_type);
}

void create_mpi_fileinfo_type(MPI_Datatype *mpi_type) {
    MPI_Datatype mpi_piece_type;
    create_mpi_piece_type(&mpi_piece_type);

    int block_lengths[3] = {MAX_FILENAME, 1, MAX_CHUNKS};
    MPI_Aint offsets[3];
    MPI_Datatype types[3] = {MPI_CHAR, MPI_INT, mpi_piece_type};

    offsets[0] = offsetof(struct FileInfo, filename);
    offsets[1] = offsetof(struct FileInfo, nr_segmente);
    offsets[2] = offsetof(struct FileInfo, pieces);

    MPI_Type_create_struct(3, block_lengths, offsets, types, mpi_type);
    MPI_Type_commit(mpi_type);
}

void create_mpi_trackerfiles_type(MPI_Datatype *mpi_type) {
    MPI_Datatype mpi_fileinfo_type;
    create_mpi_fileinfo_type(&mpi_fileinfo_type);

    int block_lengths[3] = {1, numtasks + 1, 1};
    MPI_Aint offsets[3];
    MPI_Datatype types[3] = {MPI_INT, MPI_INT, mpi_fileinfo_type};

    offsets[0] = offsetof(struct TrackerFiles, nr_seeds_peers);
    offsets[1] = offsetof(struct TrackerFiles, seeds_peers);
    offsets[2] = offsetof(struct TrackerFiles, info);

    MPI_Type_create_struct(3, block_lengths, offsets, types, mpi_type);
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
    
    for (int filenr = 0; filenr < files.nr_fisiere_dorite; filenr++){
        char numefisier[256];
        sprintf(numefisier, "client%d_%s", rank, files.fisiere_dorite[filenr].filename);
        FILE *file; 
        file = fopen(numefisier, "w");
        if (file == NULL) {
            printf("Eroare la deschiderea fișierului!\n");
            exit(1);
        }

        MPI_Request requestfile, requestsegm, requestsegm1, requestbusy, requestbusy1;

        MPI_Isend(files.fisiere_dorite[filenr].filename, MAX_FILENAME + 1, MPI_CHAR, 0, TAG_REQUEST, MPI_COMM_WORLD, &requestfile);
        MPI_Wait(&requestfile, MPI_STATUS_IGNORE);

        MPI_Irecv(&data, 1, mpi_type, 0, TAG_REQUEST, MPI_COMM_WORLD, &requestfile);
        MPI_Wait(&requestfile, MPI_STATUS_IGNORE);
        
        MPI_Request requestfile1;
        int *seeds_peers = calloc((numtasks + 1), sizeof(int));
        MPI_Irecv(seeds_peers, numtasks + 1, MPI_INT, 0, TAG_REQUEST, MPI_COMM_WORLD, &requestfile1);
        MPI_Wait(&requestfile1, MPI_STATUS_IGNORE);

        files.fisiere_dorite[filenr].nr_segmente = data.info.nr_segmente;
        
        for (int i = 0; i < files.fisiere_dorite[filenr].nr_segmente; i++){
            strcpy(files.fisiere_dorite[filenr].pieces[i].hash, data.info.pieces[i].hash);
            files.fisiere_dorite[filenr].pieces[i].e_descarcat = 0;
            files.fisiere_dorite[filenr].pieces[i].index = i;
        }

        for (int segment = 0; segment < data.info.nr_segmente; segment++){
            if (ten_counter % 10 == 9){
                MPI_Isend(files.fisiere_dorite[filenr].filename, MAX_FILENAME + 1, MPI_CHAR, 0, TAG_PEERS, MPI_COMM_WORLD, &requestfile);
                MPI_Wait(&requestfile, MPI_STATUS_IGNORE);
                MPI_Irecv(seeds_peers, numtasks + 1, MPI_INT, 0, TAG_PEERS, MPI_COMM_WORLD, &requestfile1);
                MPI_Wait(&requestfile1, MPI_STATUS_IGNORE);
            }

            int less_busy = 1000000000, dest = 0;
            for (int i = 1; i < numtasks; i++){
                if (i != rank && seeds_peers[i] == 1){
                    MPI_Isend("ai hash?", 9, MPI_CHAR, i, TAG_UPLOAD, MPI_COMM_WORLD, &requestsegm);
                    MPI_Wait(&requestsegm, MPI_STATUS_IGNORE);
                    MPI_Isend(files.fisiere_dorite[filenr].pieces[segment].hash, HASH_SIZE + 1, MPI_CHAR, i, TAG_ARE_SEGMENT, MPI_COMM_WORLD, &requestsegm1);
                    MPI_Wait(&requestsegm1, MPI_STATUS_IGNORE);
                        
                    char mesaj[256];
                    MPI_Irecv(mesaj, 256, MPI_CHAR, i, TAG_RASP_SEGMENT, MPI_COMM_WORLD, &requestsegm);
                    MPI_Wait(&requestsegm, MPI_STATUS_IGNORE);
                    if (strncmp(mesaj, "ACK", 3) == 0){
                        int busy;
                        MPI_Isend("busy", 5, MPI_CHAR, i, TAG_UPLOAD, MPI_COMM_WORLD, &requestbusy);
                        MPI_Wait(&requestbusy, MPI_STATUS_IGNORE);
                        MPI_Irecv(&busy, 1, MPI_INT, i, TAG_BUSY, MPI_COMM_WORLD, &requestbusy1);
                        MPI_Wait(&requestbusy1, MPI_STATUS_IGNORE);
                        if (busy < less_busy){
                            less_busy = busy;
                            dest = i;
                        }
                    }
                }
            }
            char rasp[256];
            MPI_Isend(files.fisiere_dorite[filenr].pieces[segment].hash, HASH_SIZE + 1, MPI_CHAR, dest, TAG_UPLOAD, MPI_COMM_WORLD, &requestbusy);
            MPI_Wait(&requestbusy, MPI_STATUS_IGNORE);
            ten_counter++;
            MPI_Request requestdownload;
            MPI_Irecv(rasp, 256, MPI_CHAR, dest, TAG_DOWNLOAD_REPLY, MPI_COMM_WORLD, &requestdownload);
            MPI_Wait(&requestdownload, MPI_STATUS_IGNORE);
            if (strncmp(rasp, "ACK", 3) == 0){
                files.fisiere_dorite[filenr].pieces[segment].e_descarcat = 1;
                fprintf(file, "%.*s\n", HASH_SIZE, files.fisiere_dorite[filenr].pieces[segment].hash);
            }
        }
        free(seeds_peers);
        fclose(file);
    }

    MPI_Request request_done;
    MPI_Isend("done", 4, MPI_CHAR, 0, TAG_CLIENT_DONE, MPI_COMM_WORLD, &request_done);
    MPI_Wait(&request_done, MPI_STATUS_IGNORE); 


    return NULL;
}

void *upload_thread_func(void *arg)
{
    int busy = 0;

    while(1){
        char mesaj[256];
        int source;
        MPI_Status status;
        MPI_Request request;
        
        MPI_Irecv(mesaj, 256, MPI_CHAR, MPI_ANY_SOURCE, TAG_UPLOAD, MPI_COMM_WORLD, &request);
        MPI_Wait(&request, &status);
        
        source = status.MPI_SOURCE;
        
        if (strncmp(mesaj, "busy", 4) == 0){
            MPI_Isend(&busy, 1, MPI_INT, source, TAG_BUSY, MPI_COMM_WORLD, &request);
            MPI_Wait(&request, MPI_STATUS_IGNORE);
        }
        else if (strncmp(mesaj, "ai hash?", 8) == 0){
            MPI_Request request1;
            MPI_Status status1;
            char hash[HASH_SIZE + 1];
            MPI_Irecv(hash, HASH_SIZE + 1, MPI_CHAR, source, TAG_ARE_SEGMENT, MPI_COMM_WORLD, &request1);
            MPI_Wait(&request1, &status1);
            
            int filenr = 0, segm = 0, gasit = 0;
            
            for (filenr = 0; filenr < files.nr_fisiere_descarcate; filenr++){
                for (segm = 0; segm < files.fisiere[filenr].nr_segmente; segm++){
                    if (strncmp(hash, files.fisiere[filenr].pieces[segm].hash, HASH_SIZE) == 0 && files.fisiere[filenr].pieces[segm].e_descarcat == 1){
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
            break;
        }
        else{
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
                trackerfiles[t].info.pieces[k].index = k;
                trackerfiles[t].info.pieces[k].e_descarcat = 1;
            }
        }
    }

    MPI_Datatype mpi_type;
    create_mpi_trackerfiles_type(&mpi_type);

    for (int i = 1; i < numtasks; i++) {
        MPI_Send("ACK", 4, MPI_CHAR, i, 0, MPI_COMM_WORLD);
    }

    while (clienti_gata < numtasks - 1){
        char mesaj[256];
        int flag, source;
        MPI_Status status;
        MPI_Request request, request1;
        MPI_Irecv(mesaj, 256, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &request);
        MPI_Wait(&request, &status);
 
        source = status.MPI_SOURCE;
        
        if (status.MPI_TAG == TAG_REQUEST){
            int nr = 0;
            for (nr = 0; nr < MAX_FILES; nr++){
                if (strcmp(mesaj, trackerfiles[nr].info.filename) == 0){
                    break;
                }
            }
            
            MPI_Isend(&trackerfiles[nr], 1, mpi_type, source, TAG_REQUEST, MPI_COMM_WORLD, &request);
            MPI_Wait(&request, &status);
        
            MPI_Isend(trackerfiles[nr].seeds_peers, numtasks + 1, MPI_INT, source, TAG_REQUEST, MPI_COMM_WORLD, &request1);
            MPI_Test(&request1, &flag, &status);
            
            if (!flag) {
                MPI_Wait(&request1, &status);
            }
            if (trackerfiles[nr].seeds_peers[source] == 0){
                trackerfiles[nr].nr_seeds_peers++;
                trackerfiles[nr].seeds_peers[source] = 1;
            }            

        }
        else if (status.MPI_TAG == TAG_PEERS){
            int nr = 0;
            for (nr = 0; nr < MAX_FILES; nr++){
                if (strcmp(mesaj, trackerfiles[nr].info.filename) == 0){
                    break;
                }
            }
            MPI_Isend(trackerfiles[nr].seeds_peers, numtasks + 1, MPI_INT, source, TAG_PEERS, MPI_COMM_WORLD, &request1);
            MPI_Test(&request1, &flag, &status);
            
            if (!flag) {
                MPI_Wait(&request1, &status);
            }
        }
        else if (status.MPI_TAG == TAG_CLIENT_DONE){
            clienti_gata++;
        }
    }
    for (int i = 1; i < numtasks; i++) {
        MPI_Request request;
        MPI_Isend("ACK", 4, MPI_CHAR, i, TAG_UPLOAD, MPI_COMM_WORLD, &request);
        MPI_Wait(&request, MPI_STATUS_IGNORE);
    }

    for (int i = 0; i < MAX_FILES; i++)
        free(trackerfiles[i].seeds_peers);
    
}

void peer(int numtasks, int rank) {
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;

    char filename[50];
    FILE *file;

    snprintf(filename, sizeof(filename), "in%d.txt", rank);

    file = fopen(filename, "r");
    if (file == NULL) {
        printf("Process [%d]: Failed to open file %s\n", rank, filename);
        MPI_Finalize();
        exit(-1);
    }

    fscanf(file, "%d", &files.nr_fisiere_descarcate);

    MPI_Send(&files.nr_fisiere_descarcate, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);

    for (int i = 0; i < files.nr_fisiere_descarcate; i++){
        fscanf(file, "%s %d", files.fisiere[i].filename, &files.fisiere[i].nr_segmente);
        MPI_Send(files.fisiere[i].filename, strlen(files.fisiere[i].filename) + 1, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
        MPI_Send(&files.fisiere[i].nr_segmente, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        for (int index = 0; index < files.fisiere[i].nr_segmente; index ++){
            fscanf(file, "%s", files.fisiere[i].pieces[index].hash);
            files.fisiere[i].pieces[index].index = index;
            files.fisiere[i].pieces[index].e_descarcat = 1;
            MPI_Send(files.fisiere[i].pieces[index].hash, 34, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
            files.fisiere[i].pieces[index].e_descarcat = 1;
        }
    }
    
    fscanf(file, "%d", &files.nr_fisiere_dorite);
    for (int i = 0; i < files.nr_fisiere_dorite; i++){
        fscanf(file, "%s", files.fisiere_dorite[i].filename);
    }

    char ack[4];
    MPI_Recv(ack, 4, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

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
