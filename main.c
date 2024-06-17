#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

const int MEMORY_SIZE = 4096;
int inputArraySize = 0;

void errormsg( char *msg ) {
   perror( msg );
   exit( 1 );
}

int checkInputNumberMatches(FILE *file, int numElements, int **arr)
{
    int numsInFile = 0;
    int num;
    *arr = NULL;
    // Read through file and tally the amount of numbers in it
    while (fscanf(file, "%d", &num) != EOF)
    {
        numsInFile++;
        
        // If they differ, exit program
        if (numsInFile > numElements)
        {
            free(*arr);
            printf("Number of elements in file is greater than number of elements user specified\n");
            return 0;
        }
        //Read the number into the array
        else{    
            inputArraySize++;

            //Initial array allocation to store one elemenet
            if(numsInFile == 1){
                *arr = (int *)malloc(1 * sizeof(int));
                //Check if memory allocation failed
                if(*arr == NULL) {
                    printf("Memory allocation failed.\n");
                    return 0;
                }
            }
            //As we read more numbers from the file, grow our array by one each time
            else{
                *arr = (int *)realloc(*arr, numsInFile * sizeof(int));
                //Check if memory allocation failed
                if(*arr == NULL) {
                    printf("Memory allocation failed.\n");
                    return 0;
                }
            }
            //Store the number into the array
            (*arr)[numsInFile - 1] = num;
        }
    }
    
    // If they differ, exit program
    if (numsInFile != numElements)
    {
        free(*arr);
        printf("Number of elements in file is less than number of elements user specified\n");
        return 0;
    }

    return numsInFile;
}

void spawnProcesses(int numCores, pid_t pidOfRoot)
{
    int i;
    // Run the loop m times to create all the child cores
    for (i = 0; i < numCores; i++)
    {
        // This ensures that only the root pid is creating the processes
        if (getpid() == pidOfRoot)
        {
            int forkID = fork();

            // If we have an issue forking, stop program
            if (forkID < 0)
            {
                printf("Problem with forking. Terminating\n");
                exit(EXIT_FAILURE);
            }
            // If we are the child process, exit the for loop
            else if (forkID == 0)
            {
                printf("Child created");
                return;
            }
        }
    }
    printf("Parent finished");
}

int main()
{

    // int segment_id;

    // // create a memory segment to be shared
    // segment_id = shmget( IPC_PRIVATE, MEMORY_SIZE, S_IRUSR | S_IWUSR );

    // if ( segment_id < 0 ) errormsg( "ERROR in creating a shared memory segment\n" );

    // fprintf( stdout, "Segment id = %d\n", segment_id );

    // Must make sure value is greater than zero
    int numElements;
    while (1)
    {
        printf("Enter number of elements in the input array: ");
        scanf("%d", &numElements);
        if(numElements > 0)
        {
            break;
        }
        else
        {
            printf("Invalid number of elements, number must be greater than 0\n");
        }
    }

    int numCores;
    while (1)
    {
        printf("Enter number of cores: ");
        scanf("%d", &numCores);

        if (numCores > 0)
        {
            break;
        }
        else
        {
            printf("Invalid number of cores, number must be even and greater than 0\n");
        }
    }

    char fileName[50];
    FILE *file;

    while (1)
    {
        printf("Enter name of the input file: ");
        scanf("%s", fileName);

        // Check to see if the file opens
        file = fopen(fileName, "r");
        if (file != NULL)
        {
            break;
        }
        else
        {
            printf("Invalid file name\n");
        }
    }

    // Check if the input number matches the number of elements in the file and write the elements into the array
    //Our array will have to be dynamic and grow based on size of input file
    int *inputArray;
    int numElementsInFile = checkInputNumberMatches(file, numElements, &inputArray);

    //Exit program if we have invalid number of elements
    if (numElementsInFile == 0){return 1;}
    //If we have more cores then elements in the file, trim down the number of cores
    else if(numElementsInFile < numCores){numCores = numElementsInFile;}

    // int j = 0;
    // for(j = 0; j < inputArraySize; j++){
    //     printf("%d", inputArray[j]);
    // }
    //Create a barrier for m amount of cores
    int barrier[numCores];
    int i;
    for(i = 0; i < numCores; i++){
        barrier[i] == 0;
    }
    // // If we reach here, we are ready to begin
    pid_t pidOfRoot = getpid();
    spawnProcesses(numCores, pidOfRoot);

    //Once the children cores are created, they can start working on the array right away
}
