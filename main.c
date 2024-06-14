#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

const int MEMORY_SIZE = 4096;

int checkInputNumberMatches(FILE *file, int numElements)
{
    int numsInFile = 0;
    int num;
    // Read through file and tally the amount of numbers in it
    while (fscanf(file, "%d", &num) != EOF)
    {
        numsInFile++;
        
        // If they differ, exit program
        if (numsInFile > numElements)
        {
            printf("Number of elements in file is greater than number of elements user specified\n");
            return 0;
        }
    }
    
    // If they differ, exit program
    if (numsInFile != numElements)
    {
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

    int segment_id;

    // create a memory segment to be shared
    segment_id = shmget( IPC_PRIVATE, MEMORY_SIZE, S_IRUSR | S_IWUSR );

    if ( segment_id < 0 ) errormsg( "ERROR in creating a shared memory segment\n" );

    fprintf( stdout, "Segment id = %d\n", segment_id );

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

    // Check if the input number matches the number of elements in the file
    int numElementsInFile = checkInputNumberMatches(file, numElements);
    //Exit program if we have invalid number of elements
    if (numElementsInFile == 0){return 1;}
    //If we have more cores then elements in the file, trim down the number of cores
    else if(numElementsInFile < numCores){numCores = numElementsInFile;}
    // // If we reach here, we are ready to begin
    pid_t pidOfRoot = getpid();
    spawnProcesses(numCores, pidOfRoot);

    //Once the children cores are created, they can start working on the array right away
}
