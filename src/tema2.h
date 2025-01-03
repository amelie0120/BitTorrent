#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

struct Piece {
    int index;
    char hash[HASH_SIZE];
    int e_descarcat;
};

struct FileInfo {
    char filename[MAX_FILENAME];
    int nr_segmente;
    struct Piece pieces[MAX_CHUNKS];
};

struct Client {
    int rank;
    struct FileInfo fisier[MAX_FILES];
};

struct Tracker {
    int nr_fisiere_descarcate;
    int nr_fisiere_dorite;
    struct FileInfo fisiere[MAX_FILES];
    char fisiere_dorite[MAX_FILES][MAX_FILENAME];
};

struct TrackerFiles {
    //char filename[MAX_FILENAME];
    int *seeds_peers;
    struct FileInfo info;
};

