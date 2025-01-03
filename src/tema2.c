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

struct Tracker files;

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

    int block_lengths[2] = {1, 1};
    MPI_Aint displacements[2];
    MPI_Datatype types[2] = {MPI_INT, mpi_fileinfo_type};

    displacements[0] = offsetof(struct TrackerFiles, seeds_peers);
    displacements[1] = offsetof(struct TrackerFiles, info);

    MPI_Type_create_struct(2, block_lengths, displacements, types, mpi_type);
    MPI_Type_commit(mpi_type);
}

void *download_thread_func(void *arg)
{
    int rank = *(int*) arg;

    int flag;
    MPI_Datatype mpi_type;
    create_mpi_trackerfiles_type(&mpi_type);

    struct TrackerFiles data;
    for (int filenr = 0; filenr < files.nr_fisiere_dorite; filenr++){
        MPI_Request requestfile;
        MPI_Status status;

        // Trimitem numele fișierului către tracker
        MPI_Send(files.fisiere_dorite[filenr], MAX_FILENAME + 1, MPI_CHAR, 0, TAG_REQUEST, MPI_COMM_WORLD);
        printf("client %d a trimis request\n", rank);
        MPI_Test(&requestfile, &flag, &status);
 
        if (flag) {
            //printf("[P0] The send operation is over\n");
        } else {
            //printf("[P0] The send operation is not over yet\n");
            MPI_Wait(&requestfile, &status);
        }


        MPI_Irecv(&data, 1, mpi_type, 0, 0, MPI_COMM_WORLD, &requestfile);
        printf("client %d trebuie sa isi primeasca seeds/peers", rank);
        MPI_Test(&requestfile, &flag, &status);
 
        if (flag) {
            //printf("[P1] The receive operation is over\n");
        } else {
            //printf("[P1] The receive operation is not over yet\n");
            MPI_Wait(&requestfile, &status);
        }

        // // Așteptăm finalizarea transferului non-blocant
        // MPI_Wait(&requestfile, MPI_STATUS_IGNORE);
        // //printf("Rank %d a primit structura de la Rank 0.\n", rank);

        printf("Rank %d a primit structura:\n", rank);
        printf("Filename: %s\n", data.info.filename);
        printf("Nr segmente: %d\n", data.info.nr_segmente);
        for (int i = 0; i < data.info.nr_segmente; i++) {
            printf("Piece %d - Index: %d, Hash: %s, Descarcat: %d\n", i,
            data.info.pieces[i].index, data.info.pieces[i].hash, data.info.pieces[i].e_descarcat);
        }
        
    }

    MPI_Request request_done;
    MPI_Status status_done;
    MPI_Isend("ACK", 4, MPI_CHAR, 0, TAG_CLIENT_DONE, MPI_COMM_WORLD, &request_done);
    MPI_Test(&request_done, &flag, &status_done);
 
    if (flag) {
        printf("[P1] The receive operation is over\n");
    } else {
        printf("[P1] The receive operation is not over yet\n");
        MPI_Wait(&request_done, &status_done);
    }
    // if (rank == 1) {
    //     // Rank 1 trimite o cerere (non-blocantă)
    //     MPI_Request request;
    //     MPI_Isend("Request", 7, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &request);
    //     printf("Rank %d a trimis cererea către Rank 0.\n", rank);
        
    //     MPI_Irecv(&data, 1, mpi_type, 0, 0, MPI_COMM_WORLD, &request);

    //     // Așteptăm finalizarea transferului non-blocant
    //     MPI_Wait(&request, MPI_STATUS_IGNORE);
    //     printf("Rank %d a primit structura de la Rank 0.\n", rank);

    //     // Afișăm datele primite
    //     printf("Rank %d a primit structura:\n", rank);
    //     //for (int k = 0; k < fisieretotal; k++){
    //         printf("Filename: %s\n", data.info.filename);
    //         printf("Nr segmente: %d\n", data.info.nr_segmente);
    //         for (int i = 0; i < data.info.nr_segmente; i++) {
    //             printf("Piece %d - Index: %d, Hash: %s, Descarcat: %d\n", i,
    //             data.info.pieces[i].index, data.info.pieces[i].hash, data.info.pieces[i].e_descarcat);
    //         }
    //     //}
    // }

    

    return NULL;
}

void *upload_thread_func(void *arg)
{
    int rank = *(int*) arg;



    return NULL;
}

void tracker(int numtasks, int rank) {
    char hash[32];
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
                    gasit = 1;
                }
                if (gasit)
                    break;
            }

            if (!gasit){
                strcpy(trackerfiles[fisieretotal].info.filename, filename);
                trackerfiles[fisieretotal].seeds_peers[i] = 1;
                trackerfiles[fisieretotal].info.nr_segmente = nrsegm;
                t = fisieretotal;
                fisieretotal++;
            }


            for(int k = 0; k < nrsegm; k++){
                MPI_Recv(trackerfiles[t].info.pieces[k].hash, 34, MPI_CHAR, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                trackerfiles[t].info.pieces[k].index = k;
            }
        }
    }

    // for (int t = 0; t < fisieretotal; t++){
    //     printf("%s\n", trackerfiles[t].info.filename);
    //     printf("seeds/peers: ");
    //     for (int i = 1; i <= numtasks; i++){
    //         if (trackerfiles[t].seeds_peers[i])
    //             printf("%d ", i);
    //     }
    //     printf("\n");
    //     printf("%d segmente\n", trackerfiles[t].info.nr_segmente);
    //     for (int k = 0; k < trackerfiles[t].info.nr_segmente; k++){
    //         printf("%s\n", trackerfiles[t].info.pieces[k].hash);
    //     }
    //     printf("\n");
    // }

    MPI_Datatype mpi_type;
    create_mpi_trackerfiles_type(&mpi_type);

    for (int i = 1; i < numtasks; i++) {
        MPI_Send("ACK", 4, MPI_CHAR, i, 0, MPI_COMM_WORLD);
        //printf("Tracker: Sent ACK to client %d.\n", i);
    }

    while (clienti_gata < numtasks - 1){
        char mesaj[256];
        int r, flag;
        MPI_Status status;
        MPI_Request request;
        MPI_Recv(mesaj, 256, MPI_CHAR, r, TAG_REQUEST, MPI_COMM_WORLD, &status);
        printf("tracker-ul a primit request de la %d\n", r);
        if (status.MPI_TAG == TAG_REQUEST){
            int nr = 0;
            for (nr = 0; nr < MAX_FILES; nr++){
                if (strcmp(mesaj, trackerfiles[nr].info.filename) == 0){
                    break;
                }
            }
            MPI_Isend(&trackerfiles[nr], 1, mpi_type, r, TAG_REQUEST, MPI_COMM_WORLD, &request);
            MPI_Test(&request, &flag, &status);
 
            if (flag) {
                printf("[P0] The send operation is over\n");
            } else {
                printf("[P0] The send operation is not over yet\n");
                MPI_Wait(&request, &status);
            }

        }

    }
    
    //printf("Rank %d a trimis structura către Rank 1.\n", rank);
    
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
    printf("s a deschis fisierul %s\n", filename);

    fscanf(file, "%d", &files.nr_fisiere_descarcate);

    MPI_Send(&files.nr_fisiere_descarcate, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);

    for (int i = 0; i < files.nr_fisiere_descarcate; i++){
        fscanf(file, "%s %d", files.fisiere->filename, &files.fisiere->nr_segmente);
        MPI_Send(files.fisiere->filename, strlen(files.fisiere->filename) + 1, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
        MPI_Send(&files.fisiere->nr_segmente, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        for (int index = 0; index < files.fisiere->nr_segmente; index ++){
            fscanf(file, "%s", files.fisiere->pieces[index].hash);
            files.fisiere->pieces[index].index = index;
            files.fisiere->pieces[index].e_descarcat = 1;
            MPI_Send(files.fisiere->pieces[index].hash, 34, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
            
        }
    }
    
    fscanf(file, "%d", &files.nr_fisiere_dorite);
    for (int i = 0; i < files.nr_fisiere_dorite; i++){
        fscanf(file, "%s", files.fisiere_dorite[i]);
        //printf("clientul %d vrea %s\n", rank, files.fisiere_dorite[i]);
    }

    char ack[4];
    MPI_Recv(ack, 4, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    if (strcmp(ack, "ACK") == 0) {
        printf("Client %d: Received ACK from tracker. Starting download/upload phase.\n", rank);
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
}
 
int main (int argc, char *argv[]) {
    int numtasks, rank;
 
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
