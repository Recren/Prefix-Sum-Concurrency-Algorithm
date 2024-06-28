#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

int inputArraySize = 0;

//Function to write values of input file into an array as well as check that the number of elements match what user specified
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
        // Read the number into the array
        else
        {
            inputArraySize++;

            // Initial array allocation to store one elemenet
            if (numsInFile == 1)
            {
                *arr = (int *)malloc(1 * sizeof(int));
                // Check if memory allocation failed
                if (*arr == NULL)
                {
                    printf("Memory allocation failed.\n");
                    return 0;
                }
            }
            // As we read more numbers from the file, grow our array by one each time
            else
            {
                *arr = (int *)realloc(*arr, numsInFile * sizeof(int));
                // Check if memory allocation failed
                if (*arr == NULL)
                {
                    printf("Memory allocation failed.\n");
                    return 0;
                }
            }
            // Store the number into the array
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

//Allows children processes to wait until all other children processes have completed their portion of computation
void arriveAndWait(int *shared_array, int *updated_array, int *barrier_array, int numCores, int idCore)
{

    // set spot in barrier to 1 to signify core is completed
    barrier_array[idCore] = 1;

    // Let our first core take care of barrier checking
    if (idCore == 0)
    {
        // Perfom the barrier checking here
        // Continuously loop through the barrier and see if there are any zeroes
        int currentIteration = 0;

        while (1)
        {

            // When barrier should now be lifted
            // If we reach here, that means we detected no zeroes in our barrier which means child processes are ready to go on
            if (currentIteration == numCores)
            {

                // First copy the iterated array over to the shared_array so children processes have right values to work with
                memcpy(shared_array, updated_array, inputArraySize * sizeof(int));

                // Then, reset the barrier values to let children exit the loop and continue execution
                int i = 0;
                for (i = 0; i < numCores; i++)
                {
                    barrier_array[i] = 0;
                }
                currentIteration = 0;
                // Allow children to move onto next iteration
                break;
            }

            // This should be always running to check the barriers
            if (barrier_array[currentIteration] == 1)
            { // If current index value is 1, it means that child completed its work
                currentIteration++;
            }
            //If we detect any zero, start over
            else
            {
                currentIteration = 0;
            }
        }
    }
    // If we are not core zero, we will wait here until we are free to go
    else
    {
        while (1)
        {
            // We will continue to loop until we get our barrier index set back to zero (it means core zero was in the process of resetting the barrier array meaning we are good to go)
            if (barrier_array[idCore] == 0)
            {
                break;
            }
        }
    }
}

//Allows child processes to perform the Hillis Steele Algorithm
void compute(int idCore, int *shared_array, int numCores, int *updated_array, int *barrier_array)
{
    int i;
    // i*=2 to determine the space between neighbor
    for (i = 1; i < inputArraySize; i *= 2)
    {
        // inputArraySize - i will be the amount of computations needed to be done for the current iteration
  
        // If we have more cores then computations needed
        if (numCores >= (inputArraySize - i))
        {

            // Ensures that only the cores needed perform the computation (atmost amount of cores is inputArraySize - i)
            if ((idCore) < (inputArraySize - i))
            {
                // Take the current index needed to be computed and add the required previous index to it
                updated_array[idCore + i] = shared_array[idCore] + shared_array[idCore + i];
            }
        }
        // If we have less cores than computations needed
        else if (numCores < (inputArraySize - i))
        {

            // Will need to divide the work up between cores
            int range = (inputArraySize - i) / numCores; // How many computations each core needs to do

            int carryOver = (inputArraySize - i) % numCores; // If the core id is less than the carryover, the core will need to do one more computation
            int z;
            //If the core needs to do plus 1 computations
            if (idCore < carryOver)
            {
                for (z = 0; z < (range + 1); z++)
                {
                    // Gets us to the right index       //adding idCore to ensure proper offset due to the values in front of it creating an offset of plus 1
                    updated_array[(idCore * (range)) + i + z + idCore] = shared_array[(idCore * (range)) + z + idCore] + shared_array[(idCore * (range)) + i + z + idCore];
                }
            }
            //If the core just needs to do standard range of computation
            else
            {
                for (z = 0; z < (range); z++)
                {
                    // Gets us to the right index               //add carryover in case of offset
                    updated_array[(idCore * (range)) + i + z + carryOver] = shared_array[(idCore * (range)) + z + carryOver] + shared_array[(idCore * (range)) + i + z + carryOver];
                }
            }
        }
        // Send the core to the waiting room
        arriveAndWait(shared_array, updated_array, barrier_array, numCores, idCore);

        // Wait for all other cores to finish before going again
    }
    // If we are here, that means the child process has exited and completed its work

}

//Function that has the parent process create all the child processes the user specified
void spawnProcesses(int numCores, pid_t pidOfRoot, int *shared_array, int *updated_array, int *barrier_array, int childPIDS[])
{

    // Run the loop m times to create all the child cores
    int i;
    for (i = 0; i < numCores; i++)
    {
        // This ensures that only the root pid is creating the processes
        if (getpid() == pidOfRoot)
        {
            int forkID = fork();

            // In the parent process, this array index will have the PID of the child process
            childPIDS[i] = forkID;

            // If we have an issue forking, stop program
            if (forkID < 0)
            {
                // Scanario may occur where we have an issue mid loop so we must terminate all the children pid
                //  Kill all child processes made
                int j;
                for (j = 0; j < i; j++)
                {
                    if (kill(childPIDS[j], SIGKILL) == -1)
                    {
                        perror("kill");
                    }
                    else
                    {
                        printf("Terminated child process %d\n", j);
                    }
                }
                printf("Problem with forking. Terminating\n");
                exit(1); // Exit the parent process
            }
            // If we are the child process, start execution
            else if (forkID == 0)
            {
                // start the computation and assign it the index for its spot in the array
                compute(i, shared_array, numCores, updated_array, barrier_array);
                exit(0);    //When child finishes, exit
            }
        }
    }

}

int main(int argc, char *argv[])
{

    //Make sure we have correct number of arguments
    if(argc != 5){
        printf("Invalid number of arguments provided, arguments needed must be 4\n");
        exit(1);
    }

    // Must make sure value is greater than zero
    int numElements = atoi(argv[1]);
    if (numElements <= 0)
    {
        printf("Invalid number of elements, number must be greater than 0\n");
        exit(1);
    }

    // Make sure number of cores is greater than zero
    int numCores = atoi(argv[2]);
    if (numCores <= 0)
    {
        printf("Invalid number of cores, number must greater than 0\n");
        exit(1);
    }

    char fileName[50];
    FILE *inputFile;
    FILE *outputFile;

    // Make sure input file is valid
    strcpy(fileName, argv[3]);
    inputFile = fopen(fileName, "r");
    if (inputFile == NULL)
    {
        printf("Invalid input file name\n");
        exit(1);
    }

    // Make sure output file is valid
    strcpy(fileName, argv[4]);
    outputFile = fopen(fileName, "w");
    if (outputFile == NULL)
    {
        printf("Invalid output file name\n");
        exit(1);
    }


    // Check if the input number matches the number of elements in the file and write the elements into the array
    // Our array will have to be dynamic and grow based on size of input file
    int *inputArray;
    int numElementsInFile = checkInputNumberMatches(inputFile, numElements, &inputArray);

    // Exit program if we have invalid number of elements or something failed when allocating memory
    if (numElementsInFile == 0)
    {
        exit(1);
    }
    // If we have more cores then elements in the file, trim down the number of cores
    else if (numElementsInFile < numCores)
    {
        numCores = numElementsInFile;
    }

    // Create a barrier for m amount of cores
    int barrier[numCores];
    int i;
    for (i = 0; i < numCores; i++)
    {
        barrier[i] = 0;
    }

    // create a memory segment to be shared
    int segment_idOne = shmget(IPC_PRIVATE, (inputArraySize * sizeof(int)), S_IRUSR | S_IWUSR);
    int segment_idTwo = shmget(IPC_PRIVATE, (inputArraySize * sizeof(int)), S_IRUSR | S_IWUSR);
    int segment_idThree = shmget(IPC_PRIVATE, (numCores * sizeof(int)), S_IRUSR | S_IWUSR);
    if (segment_idOne < 0 || segment_idTwo < 0 || segment_idThree < 0){
        printf("ERROR in creating a shared memory segment\n");
        exit(1);
    }

    int *shared_array;
    int *updated_array;
    int *barrier_array;

    // copy our input array to the shared array
    shared_array = (int *)shmat(segment_idOne, NULL, 0);
    memcpy(shared_array, inputArray, inputArraySize * sizeof(int));

    // Use this array to store the values after each iteration
    updated_array = (int *)shmat(segment_idTwo, NULL, 0);
    memcpy(updated_array, inputArray, inputArraySize * sizeof(int));

    // Create our barrier array
    barrier_array = (int *)shmat(segment_idThree, NULL, 0);
    memcpy(barrier_array, barrier, numCores * sizeof(int));

    // If we reach here, we are ready to begin
    pid_t pidOfRoot = getpid();
    pid_t childPIDS[numCores];


    // Spawn the processes
    spawnProcesses(numCores, pidOfRoot, shared_array, updated_array, barrier_array, childPIDS);

    //If we are the parent process, wait for all the children to finish
     if(pidOfRoot == getpid()){
         for (i = 0; i < numCores; i++) {
             wait(NULL);
         }
         printf("All child processes have exited\n");
    }

    // Write to output file
    int j = 0;
    for (j; j < inputArraySize; j++)
    {
        fprintf(outputFile, "%d ", shared_array[j]);
    }

    // mark the shared memory segment for destruction
    shmdt(shared_array);
    shmdt(updated_array);
    shmdt(barrier_array);
    shmctl(segment_idOne, IPC_RMID, NULL);
    shmctl(segment_idTwo, IPC_RMID, NULL);
    shmctl(segment_idThree, IPC_RMID, NULL);

    return 0;
}
